#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "list.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/off_t.h"
#include <syscall-nr.h>
#include <stdio.h>
#include <stdint.h>

static void syscall_handler (struct intr_frame *);
static uint32_t stack_int (void *esp, uint8_t offset);
struct file *fd_get_file (int fd);
static void *stack_ptr (void *esp, uint8_t offset);

/* handlers. */
static void write_handler (struct intr_frame *);
static void exit_handler (uint32_t exit_status);
static void exec_handler (struct intr_frame *);
static void halt_handler (void);
static void wait_handler (struct intr_frame *);
static void remove_handler (struct intr_frame *f);
static void create_handler (struct intr_frame *f);
static void open_handler (struct intr_frame *f);
static void filesize_handler (struct intr_frame *f);

/* file system */
struct lock filesys_lock;               /* synchronization for the file system. */
struct list fds;                        /* file descriptor list. */
struct fd_elem {
  struct list_elem elem;
  struct file *file;
  int fd;                               /* file descriptor. */
};
int next_fd;                            /* file descriptor counter. */ 

void
syscall_init (void)
{
  /* structs MUST be initilizated before enabling syscalls. */
  lock_init (&filesys_lock);
  list_init (&fds);
  next_fd = 2;                          /* 0 and 1 are reserved form stdo and stdi. */

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int sys_code = stack_int (f->esp, 0);
  uint32_t exit_status;                         /* required because exit_handler definition. */

  switch (sys_code) {
    case SYS_HALT:
      halt_handler ();
      break;

    case SYS_EXIT:
      exit_status = stack_int (f->esp, 1);
      exit_handler (exit_status);
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
 
    case SYS_REMOVE:
      remove_handler (f);
      break;

    case SYS_CREATE:
      create_handler (f);
      break;

    case SYS_OPEN:
      open_handler (f);
      break;

    case SYS_FILESIZE:
      filesize_handler (f);
      break;

    default:
      printf ("system call %x !\n", sys_code);
      thread_exit ();                           /* thread_exit calls process_exit */
      break;
  }
}

/* this function is used to read an argument from stack. Offset param
 * is the current argument number. For Example:
 *    - 1st arg ---> offset = 1
 *    - 2nd arg ---> offset = 2
 *    - ...
 */
static uint32_t
stack_int (void *esp, uint8_t offset)
{
  return *((int*)esp + offset);
}

/* Reads argument of type `pointer` from stack and validates it.
 * Offset param is the current argument number. For Example:
 *    - 1st ptr ---> offset = 1
 *    - 2nd ptr ---> offset = 2
 *    - ...
 */
static void *
stack_ptr (void *esp, uint8_t offset)
{
  void *addr = (void*)stack_int (esp, offset);          /* reads ptr from stack as normal. */

  if (is_user_vaddr (addr)) {                           /* addr is not PintOS code. */
    if (pagedir_get_page (thread_current ()->pagedir, addr) != NULL) {
      /* mem[addr] is owned by the process. */
      return addr;                                      /* addr is ok. */
    }
  }

  exit_handler (-1);                                    /* addr is bad. */
  NOT_REACHED();
}

/* TODO: add a comment here. */
struct file *fd_get_file (int fd) {
  struct fd_elem *fd_elem;
  struct list_elem *e;

  for (e = list_begin (&fds); e != list_end (&fds);
  e = list_next (e)) {
    fd_elem = list_entry(e, struct fd_elem, elem);
    if (fd_elem->fd == fd)
      return fd_elem->file;
  }

  return NULL;
}


/* syscalls handlers */
static void
write_handler (struct intr_frame *f)
{
  uint32_t fd = stack_int (f->esp, 1);
  char *buffer = (char*)stack_ptr (f->esp, 2);       /* mem[buffer] contains the string. */
  unsigned size = (unsigned)stack_int (f->esp, 3);   /* bytes to be printed. */
  
  if (fd == 1) {
    putbuf (buffer, size);               /* putbuf writes to console. */
    f->eax = size;                      /* this syscall returns the size writed. eax has the return value. */
  } else {
    lock_acquire (&filesys_lock);
    
    /* TODO: complete write syscall */
    printf ("write syscall!\n");

    lock_release (&filesys_lock);
    thread_exit ();
  }
}

static void exit_handler (uint32_t exit_status) {
  thread_current ()->exit_status = exit_status;        /* set my own exit status. */
  thread_exit ();
}

static void halt_handler (void) {
  shutdown_power_off();               /* bye bye */
}

static void exec_handler (struct intr_frame *f) {
  char* cmd_line = (char*)stack_ptr (f->esp, 1);
  tid_t tid = process_execute (cmd_line);
  f->eax = tid;
}

static void wait_handler (struct intr_frame *f) {
  tid_t tid = stack_int (f->esp, 1);
  int exit_status = process_wait (tid);
  f->eax = exit_status;
}

static void remove_handler(struct intr_frame *f) {
  char *name = (char*)stack_ptr (f->esp, 1);
 
  lock_acquire (&filesys_lock);

  f->eax = filesys_remove (name);
 
  lock_release (&filesys_lock);
}

static void create_handler(struct intr_frame *f) {
  char* name = (char*)stack_ptr (f->esp, 1);
  off_t init_size = (off_t)stack_int (f->esp, 2);

  lock_acquire (&filesys_lock);
  
  f->eax = filesys_create (name, init_size);

  lock_release (&filesys_lock);
}

static void open_handler (struct intr_frame *f) {
  char *filename = (char*)stack_ptr (f->esp, 1);           /* filename. */

  lock_acquire (&filesys_lock);
  
  struct file *file = filesys_open (filename);            /* open the file. */
  if (file != NULL) {
    struct fd_elem *elem = malloc (sizeof (*elem));       /* reserve mem for file descriptor. */
    if (elem != NULL) {
      elem->file = file;
      elem->fd = next_fd++;
      list_push_front (&fds, &elem->elem);                 /* register the file descriptor. */
      f->eax = elem->fd;                                  /* return the file descriptor. */
      goto end;
    } else {
      file_close (file);
    }
  }
  
  f->eax = -1;                                            /* something went wrong. */

end:
  lock_release(&filesys_lock);
}

static void filesize_handler (struct intr_frame *f) {
  int fd = (int)stack_int (f->esp, 1);                     /* file descriptor. */
  
  struct file *file = fd_get_file (fd);                    /* file in mem. */
  if (file != NULL) {
    uint32_t size = file_length (file);
    f->eax = size;
  }

  f->eax = -1;
}

