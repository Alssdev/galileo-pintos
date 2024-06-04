#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/synch.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/ptable.h"

static thread_func start_process NO_RETURN;
static bool load (struct filename_args *fn_args, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmdline) 
{
  int str_len;
  char *token, *save_ptr;
  
  /* allocate filename_args */
  tid_t tid;
  struct filename_args *fn_args = malloc(sizeof(struct filename_args));

  /* creating a modificable copy of cmdline. */
  str_len = strlen(cmdline);                                  /* len of cmdline string */
  fn_args->cmdline_copy = malloc (str_len + 1);               /* reserve mem for file_name_copy, + 1 because of \0 */
  if (fn_args->cmdline_copy == NULL) {
    free (fn_args);
    return TID_ERROR;
  }
  strlcpy (fn_args->cmdline_copy, cmdline, str_len + 1);      /* create the copy of file_name */

  /* extracting filename from cmdline param. */
  token = strtok_r(fn_args->cmdline_copy, " ", &save_ptr);    /* separating filename from cmdline param. */
  str_len = strlen(token);                                    /* len of filename string. */
  fn_args->file_name = malloc(str_len + 1);                   /* reserve mem for fn_args->filename, + 1 because of \0 */
  if (fn_args->file_name == NULL) {
    free (fn_args->cmdline_copy);
    free (fn_args);
    return TID_ERROR;
  }
  strlcpy(fn_args->file_name, token, str_len + 1);            /* create the copy of filename */

  /* creating a copy of cmdline args. */
  fn_args->args = save_ptr;                                   /* pass as reference only, because cmdline_copy is a copy already
                                                               and isn't needed any more.*/ 

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (fn_args->file_name, PRI_DEFAULT, start_process, fn_args);

  if (tid != TID_ERROR) {
    /* wait for child to be successfully created. */
    enum intr_level old_level = intr_disable ();                /* synchronization for all_list. */

    struct thread *child = thread_find (tid);
    if (child != NULL)
      sema_down (&child->wait_sema);

    intr_set_level (old_level);
  }

  if (tid == TID_ERROR || thread_current ()->exec_status == ERROR) {
    /* free mem, otherwise, start_process will free mem. */
    free(fn_args->cmdline_copy);
    free(fn_args->file_name);
    free(fn_args);

    return TID_ERROR;
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *fn_args_)
{
  struct thread *cur = thread_current ();
  struct filename_args *fn_args = fn_args_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (fn_args, &if_.eip, &if_.esp);

  /* Wake up parent process. Parent process always wait for child process
   * to be created successfully. */
  cur->parent->exec_status = success ? SUCCESS : ERROR;
  sema_up (&cur->wait_sema);

  /* If load failed, quit. */
  if (!success)
    exit_handler (-1);

  /* free mem */
  free (fn_args->cmdline_copy);
  free (fn_args->file_name);
  free (fn_args);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread *alive_child;
  struct dead_thread *dead_child;
  int exit_status;

  enum intr_level old_level = intr_disable ();            /* synchronization for all_list. */

  /* finding an alive child process. */
  alive_child = thread_find (child_tid);                  /* find an alive child process. */
  if (alive_child == NULL || !alive_child->allow_wait) {                             
    dead_child = thread_dead_pop (child_tid);             /* find a dead child process. */
    if (dead_child == NULL) {
      intr_set_level (old_level);
      return -1;                                          /* child not found. */
    }

    /* get exit status. */
    exit_status = dead_child->exit_status;

    /* free mem. */
    free (dead_child);

    intr_set_level (old_level);
    return exit_status;
  }

  /* if child is alive, then go to wait */
  sema_down (&alive_child->wait_sema);                     /* wait simulation */

  /* after waiting, get exit status */
  dead_child = thread_dead_pop (child_tid);               /* when a child process exit it saves its exit
                                                             status in threads/dead_list. */
  /* get exit status. */
  exit_status = dead_child->exit_status;

  /* free mem. */
  free (dead_child);

  intr_set_level (old_level);
  return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Process Termination Message */
  printf ("%s: exit(%d)\n", cur->name, cur->exit_status);

  /* close source file. */
  if (cur->f != NULL) {
    filesys_acquire ();
    file_close (cur->f);
    filesys_release ();
  }

  /* save exit status. */
  enum intr_level old_level = intr_disable ();
  thread_dead_push (cur);
  cur->allow_wait = false;
  intr_set_level (old_level);

  /* wakes up parent thread, if it's waiting. */
  sema_up (&cur->wait_sema);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, struct filename_args *fn_args);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (struct filename_args *fn_args, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  filesys_acquire ();

  /* Open executable file. */
  file = filesys_open (fn_args->file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", fn_args->file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
    || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
    || ehdr.e_type != 2
    || ehdr.e_machine != 3
    || ehdr.e_version != 1
    || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
    || ehdr.e_phnum > 1024) 
  {
    printf ("load: %s: error loading executable\n", fn_args->file_name);
    goto done; 
  }
  file_deny_write (file);

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, fn_args))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  t->f = file;
  filesys_release ();
  return success;
}

/* load() helpers. */


/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file UNUSED, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  size_t page_read_bytes;
  size_t page_zero_bytes;
  struct page *page;

  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  while (read_bytes > 0 || zero_bytes > 0) {
    /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */

    page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    page_zero_bytes = PGSIZE - page_read_bytes;

    page = ptable_new_entry (upage, writable, CODE);
    page->ofs = ofs;
    page->read_bytes = page_read_bytes;

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
    ofs += page_read_bytes;
  }

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, struct filename_args *fn_args)
{
  /* reserve memory. */
  ptable_new_entry (((uint8_t *) PHYS_BASE) - PGSIZE, true, STACK);

  *esp = PHYS_BASE;

  uintptr_t arg_addrs[MAX_ARGS_LEN];
  uint8_t arg_i = 0;                            /* utinX_t must be enought for MAX_ARG_LEN */
  uint32_t args_len = 0;

  char *token, *save_ptr;

  /* adding filename to the top of the stack. */
  *esp -= strlen(fn_args->file_name) + 1;
  memcpy(*esp, fn_args->file_name, strlen(fn_args->file_name) + 1);
  arg_addrs[arg_i++] = (uintptr_t)*esp;

  /* adding args to the top of the stack. */
  for (token = strtok_r (fn_args->args, " ", &save_ptr); token != NULL;
  token = strtok_r (NULL, " ", &save_ptr)) {

    /* same as adding filename to the top of the stack. */
    *esp -= strlen(token) + 1;
    memcpy(*esp, token, strlen(token) + 1);
    arg_addrs[arg_i++] = (uintptr_t)*esp;
  }

  args_len = arg_i;

  /* word align */
  *esp -= (uint32_t)*esp % 4;
  memset(*esp, 0x0, arg_addrs[arg_i - 1] - (uintptr_t)*esp);

  /* sentinel */
  *esp -= 4;
  memset(*esp, 0x0, 4);

  for ( /* void */ ; arg_i > 0; arg_i--) {
    *esp -= sizeof(char*);
    memcpy(*esp, &arg_addrs[arg_i - 1], sizeof(char*));
  }

  /* add argv to the top of the stack */
  uintptr_t argv = (uintptr_t)*esp;
  *esp -= sizeof(char**);
  memcpy(*esp, &argv, sizeof(char**));

  /* ardd argc */
  *esp -= sizeof(int);
  memcpy(*esp, &args_len, sizeof(int));

  /* add a NULL pointer as return adderss */
  *esp -= sizeof(char**);
  memset(*esp, 0, sizeof(char**));

  return true;
}

