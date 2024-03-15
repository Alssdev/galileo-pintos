#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
static uint32_t stack_arg (void *esp, uint8_t offset);

/* handlers */
static void write_handler (struct intr_frame *);
static void exit_handler (struct intr_frame *);
static void exec_handler (struct intr_frame *);
static void halt_handler (void);
static void wait_handler (struct intr_frame *);

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

    case SYS_EXEC:
      exec_handler (f);
      break;

    case SYS_WAIT:
      wait_handler (f);
      break;

    default:
      printf("system call %x !\n", sys_code);
      thread_exit ();                           /* thread_exit calls process_exit */
      break;
  }
}

/* this function is used to read an argument from stack. Offset param
 * is the current argument number. For Example:
 *    - 1st arg ---> offset = 1
 *    - 2nd arg ---> offset = 2
 *    - ...
 * */
static uint32_t
stack_arg (void *esp, uint8_t offset)
{
  return *((int*)esp + offset);
}

/* syscalls handlers */
static void
write_handler (struct intr_frame *f)
{
  uint32_t fd = stack_arg(f->esp, 1);
  char *buffer = (char*)stack_arg(f->esp, 2);       /* mem[buffer] contains the string. */
  unsigned size = (unsigned)stack_arg(f->esp, 3);   /* bytes to be printed. */
  
  if (fd == 1) {
    putbuf(buffer, size);               /* putbuf writes to console. */
    f->eax = size;                      /* this syscall returns the size writed. eax has the return value. */
  } else {
    /* TODO: complete write syscall */
    printf("write syscall!\n");
    thread_exit ();
  }
}

static void exit_handler (struct intr_frame *f) {
  uint32_t exit_status = stack_arg(f->esp, 1);            /* read exit code from stack. */
  thread_current ()->my_exit_status = exit_status;        /* set my own exit status. */
  thread_exit ();
}

static void halt_handler (void) {
  shutdown_power_off();               /* bye bye */
}

static void exec_handler (struct intr_frame *f) {
  char* cmd_line = (char*)stack_arg(f->esp, 1);
  tid_t tid = process_execute(cmd_line);
  f->eax = tid;
}

static void wait_handler (struct intr_frame *f) {
  tid_t tid = stack_arg(f->esp, 1);
  int exit_status = process_wait(tid);
  f->eax = exit_status;
}
