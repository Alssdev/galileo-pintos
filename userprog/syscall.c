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
struct lock lock_console;

void
syscall_init (void)
{
  /* locks init */
  lock_init(&lock_console);

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
    // TODO: putbuf has its own lock in its definition, is lock_console still needed?
    lock_acquire(&lock_console);
    putbuf(buffer, size);
    lock_release(&lock_console);

    /* return size by eax = size */
    f->eax = size;
  } else {
    printf("write syscall!\n");
    thread_exit ();
  }
}

static void exit_handler (struct intr_frame *f) {
  // TODO: return exit code
  // wake up processes waiting for thread_current
  thread_exit ();
  ASSERT(false); /* this code must not be executed */
}

static void halt_handler (void) {
  // TODO: asegurarnos de esto 
  shutdown_power_off();
}
