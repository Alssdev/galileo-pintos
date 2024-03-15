#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);
static uint32_t stack_arg (void *esp, uint8_t offset);

/* handlers */
static void write_handler (struct intr_frame *);
static void exit_handler (struct intr_frame *);
static void halt_handler (void);

/* locks */

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int sys_code = stack_arg(f->esp, 0);
  switch (sys_code) {
    case SYS_HALT:
      halt_handler ();
      break;

    case SYS_EXIT:
      exit_handler (f);
      break;
 
    case SYS_WRITE:
      write_handler (f);
      break;

    default:
      printf("system call %x !\n", sys_code);
      thread_exit ();
      break;
  }
}

static uint32_t
stack_arg (void *esp, uint8_t offset)
{
  return *((int*)esp + offset);
}

/* syscalls handlers */
static void
write_handler (struct intr_frame *f)
{
  if (stack_arg(f->esp, 1) == 1) {
    /* write to console. */
    
    /* extracting arguments from stack */
    char *buffer = (char*)stack_arg(f->esp, 2);
    unsigned size = (unsigned)stack_arg(f->esp, 3);

    /* putbuf prints to the console */
    putbuf(buffer, size);

    /* return size by eax = size */
    f->eax = size;
  } else {
    printf("write syscall!\n");
    thread_exit ();
  }
}

static void exit_handler (struct intr_frame *f) {
  // TODO: return exit code
  thread_current ()->my_exit_status = stack_arg(f->esp, 1);
  printf ("%s: exit(%d)\n", thread_current()->name, thread_current()->my_exit_status);
  thread_exit ();
  ASSERT(false); /* this code must not be executed */
}

static void halt_handler (void) {
  shutdown_power_off();
}
