#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/ptable.h"
#include "vm/swap.h"

#define STACK_MAX_PAGES 2048
#define STACK_INIT PHYS_BASE - PGSIZE

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static void page_fault_code (struct page *page);
void page_fault_grow_stack (void *upage);
void page_fault_stack (struct page *page);
void page_fault_swap (struct page *page);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      exit_handler (-1);

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool write;        /* True: access was write, false: access was read. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  write = (f->error_code & PF_W) != 0;

  void *fault_upage = pg_round_down (fault_addr);

  /* search page in page_table. */
  struct page *page = page_find (fault_upage);
  if (page != NULL && (page->is_writable || !write)) {
    /* synchronization for eviction. */
    lock_acquire (&page->evict);
    lock_release (&page->evict);

    if (page->swap == NULL) {
      switch (page->type) {
        case CODE:
          return page_fault_code (page);
          break;

        case STACK:
          return page_fault_stack (page);
          break;
      }
    } else {
      return page_fault_swap (page);
    }
  } else if (fault_addr < f->esp) {
    /* pusha or push cause the page fault. */
    uint32_t bytes = f->esp - fault_addr;
    if (bytes == 4 || bytes == 32)
      return page_fault_grow_stack (fault_upage);

  } else if (fault_upage <= STACK_INIT){
    /* possible stack grow. */
    uint32_t required_pages = (int)(STACK_INIT - fault_upage)/(int)PGSIZE;
    if (required_pages <= STACK_MAX_PAGES)
      return page_fault_grow_stack (fault_upage);
  }

  // printf ("Page fault at %p\n", fault_addr);
  exit_handler (-1);
}

static void page_fault_code (struct page *page) {
  ASSERT (page != NULL);
  ASSERT (page->kpage == NULL);

  /* reserve memory. */
  // page_block (page);
  page_alloc (page);

  /* === filesystem critical section. === */
  filesys_acquire ();

  file_seek (page->owner->f, page->ofs);
  if (file_read (page->owner->f, page->kpage, page->read_bytes) != (int) page->read_bytes)
    PANIC ("page fault bug - code loading fail."); 
  memset (page->kpage + page->read_bytes, 0, PGSIZE - page->read_bytes);

  filesys_release ();
  /* ==================================== */

  /* mark as pageable memory. */
  page_unblock (page);
}

void page_fault_grow_stack (void *upage) {
  void *page_i = STACK_INIT; 

  while (page_i >= upage) { 
    if (!page_find (page_i))
      page_create (page_i, true, STACK);

    page_i -= PGSIZE;
  }
}

void page_fault_stack (struct page *page) {
  ASSERT (page != NULL);
  ASSERT (page->upage != NULL);
  ASSERT (page->kpage == NULL);

  /* reserves memory. */
  page_alloc (page);
  page_unblock (page);
}

void page_fault_swap (struct page *page) {
  ASSERT (pg_ofs(page->upage) == 0);
  ASSERT (page->kpage == NULL);
  ASSERT (page->swap != NULL);

  /* reserve memory. */
  // page_block (page);
  page_alloc (page);
 
  // lock_acquire (&evict_lock);
  // printf ("r:");

  /* read from swap. */
  swap_load (page->swap, page->kpage);
  page->swap = NULL;

  page_unblock (page);

  // printf ("ok\n");
  // lock_release (&evict_lock);
}
