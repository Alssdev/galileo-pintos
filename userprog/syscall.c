#include "devices/shutdown.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "list.h"
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
#include "devices/input.h"
#include "userprog/syscall-handlers.h"

/* file system */
int next_fd;                            /* file descriptor counter. */
struct lock filesys_lock;               /* synchronization for the file system. */
struct fd_elem {
  struct list_elem elem;
  struct file *file;
  struct thread *t;
  int fd;                               /* file descriptor. */
};

static void syscall_handler (struct intr_frame *f);
static uint32_t stack_int (int *esp, uint8_t offset);
struct list_elem *fd_get_file (int fd);
static void *stack_ptr (void *esp, uint8_t offset);

void syscall_init (void) {
  /* structs MUST be initilizated before enabling syscalls. */
  lock_init (&filesys_lock);
  next_fd = 2;                          /* 0 and 1 are reserved form stdo and stdi. */

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int sys_code = stack_int (f->esp, 0);
  uint32_t exit_status;                         /* required because exit_handler definition. */
  int fd;

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

    case SYS_READ:
      read_handler (f);
      break;

    case SYS_CLOSE:
      fd = stack_int (f->esp, 1);
      close_handler (fd);
      break;

    case SYS_SEEK:
      seek_handler (f);
      break;

    case SYS_TELL:
      tell_handler (f);
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
static uint32_t stack_int (int *esp, uint8_t offset) {
  if (is_user_vaddr (esp + offset)) {
    if (pagedir_get_page (thread_current ()->pagedir , esp + offset)) {
      return *(esp + offset);
    }
  }

  exit_handler (-1);                                    /* addr is bad. */
  NOT_REACHED ();
}

/* Reads argument of type `pointer` from stack and validates it.
 * Offset param is the current argument number. For Example:
 *    - 1st ptr ---> offset = 1
 *    - 2nd ptr ---> offset = 2
 *    - ...
 */
static void *stack_ptr (void *esp, uint8_t offset) {
  void *addr = (void*)stack_int (esp, offset);          /* reads ptr from stack as normal. */

  if (is_user_vaddr (addr)) {                           /* addr is not PintOS code. */
    if (pagedir_get_page (thread_current ()->pagedir, addr) != NULL) {
      /* mem[addr] is owned by the process. */
      return addr;                                      /* addr is ok. */
    }
  }

  exit_handler (-1);
  NOT_REACHED();
}

/* TODO: add a comment here. */
struct list_elem *fd_get_file (int fd) {
  struct fd_elem *fd_elem;
  struct list_elem *e;
  struct thread *cur = thread_current ();

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
  e = list_next (e)) {
    fd_elem = list_entry (e, struct fd_elem, elem);
    if (fd_elem->fd == fd)
      return &fd_elem->elem;
  }

  return NULL;
}

/* syscalls handlers */
void write_handler (struct intr_frame *f)
{
  uint32_t fd = stack_int (f->esp, 1);
  char *buffer = (char*)stack_ptr (f->esp, 2);       /* mem[buffer] contains the string. */
  unsigned size = (unsigned)stack_int (f->esp, 3);   /* bytes to be printed. */
  
  if (fd == 1) {
    putbuf (buffer, size);                /* putbuf writes to console. */
    f->eax = size;
  } else {
    struct list_elem *elem = fd_get_file (fd);
    if (elem != NULL) {
      struct file *file = list_entry (elem, struct fd_elem, elem)->file;

      lock_acquire (&filesys_lock);
      f->eax = file_write (file, buffer, size);           /* write. */
      lock_release (&filesys_lock);
    } else {
      f->eax = -1;
    }
  }
}

void exit_handler (uint32_t exit_status) {
  thread_current ()->exit_status = exit_status;        /* set my own exit status. */
  thread_exit ();
}

void halt_handler (void) {
  shutdown_power_off ();               /* bye bye */
}

void exec_handler (struct intr_frame *f) {
  char* cmd_line = stack_ptr (f->esp, 1);
  tid_t tid = process_execute (cmd_line);
  f->eax = tid;
}

void wait_handler (struct intr_frame *f) {
  tid_t tid = stack_int (f->esp, 1);
  int exit_status = process_wait (tid);
  f->eax = exit_status;
}

void remove_handler (struct intr_frame *f) {
  char *name = stack_ptr (f->esp, 1);
 
  lock_acquire (&filesys_lock);

  f->eax = filesys_remove (name);
 
  lock_release (&filesys_lock);
}

void create_handler(struct intr_frame *f) {
  char* name = stack_ptr (f->esp, 1);
  off_t init_size = stack_int (f->esp, 2);

  lock_acquire (&filesys_lock);
  
  f->eax = filesys_create (name, init_size);

  lock_release (&filesys_lock);
}

void open_handler (struct intr_frame *f) {
  char *filename = stack_ptr (f->esp, 1);                 /* filename. */
  struct thread *cur = thread_current ();

  lock_acquire (&filesys_lock);
  
  struct file *file = filesys_open (filename);            /* open the file. */
  if (file != NULL) {
    struct fd_elem *elem = malloc (sizeof (*elem));       /* reserve mem for file descriptor. */
    if (elem != NULL) {
      elem->file = file;
      elem->fd = next_fd++;
      list_push_front (&cur->fds, &elem->elem);                /* register the file descriptor. */
      f->eax = elem->fd;                                  /* return the file descriptor. */
      goto end;
    } else {
      file_close (file);
    }
  }
  
  f->eax = -1;                                            /* something went wrong. */

end:
  lock_release (&filesys_lock);
}

void filesize_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);                         /* file descriptor. */
  uint32_t size = -1;                                     /* size = error_value by default. */

  lock_acquire (&filesys_lock);
  struct file *file = list_entry (fd_get_file (fd), struct fd_elem, elem)->file;
  if (file != NULL) {
    size = file_length (file);                            /* calc file's size. */
  }

  f->eax = size;
  lock_release (&filesys_lock);
}

void read_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);                     /* file descriptor. */
  char *buffer = stack_ptr(f->esp, 2);
  uint32_t size = stack_int (f->esp, 3);

  if (fd == 0) {
    /* read from keyboard. */
    for (uint32_t i = 0; i < size; i++) {
      buffer[i] = input_getc (); 
    }
    f->eax = size;
  } else {
    /* read from file. */
    lock_acquire (&filesys_lock);

    struct list_elem *elem = fd_get_file(fd);
    if (elem != NULL) {
      struct file *file = list_entry (elem, struct fd_elem, elem)->file;
      f->eax = file_read (file, buffer, size);
    } else {
      f->eax = -1;
    }

    lock_release (&filesys_lock);
  } 
}

void close_handler (int fd) {
  lock_acquire (&filesys_lock);

  struct fd_elem *fd_elem = list_entry (fd_get_file (fd), struct fd_elem, elem);
  if (fd_elem != NULL) {
    file_close (fd_elem->file);
    list_remove (&fd_elem->elem);
  }

  lock_release (&filesys_lock);
}

void seek_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);
  off_t size = stack_int (f->esp, 2);

  lock_acquire (&filesys_lock);

  struct list_elem *elem = fd_get_file(fd);
  if (elem != NULL) {
    struct file *file = list_entry (elem, struct fd_elem, elem)->file;
    file_seek (file, size);
  }

  lock_release (&filesys_lock);
}

void tell_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);

  lock_acquire (&filesys_lock);

  struct list_elem *elem = fd_get_file(fd);
  if (elem != NULL) {
    struct file *file = list_entry (elem, struct fd_elem, elem)->file;
    f->eax = file_tell (file);
  }

  lock_release (&filesys_lock);
}

