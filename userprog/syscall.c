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
#include "vm/page_table.h"

/* file system */
int next_fd;                            /* file descriptor counter. */
int filesys_lock_deep;
struct lock filesys_lock;               /* synchronization for the file system. */
struct fd_elem {
  struct list_elem elem;
  struct file *file;
  int fd;                               /* file descriptor. */
};

static void syscall_handler (struct intr_frame *f);

struct list_elem *fd_get_file (int fd);

static bool is_valid_addr (void* addr);
static void* stack_ptr (void *esp, uint8_t offset);
static char* stack_str (void *esp, uint8_t offset);
static uint32_t stack_int (int *esp, uint8_t offset);


void filesys_acquire (void) {
  if (!lock_held_by_current_thread (&filesys_lock))
    lock_acquire (&filesys_lock);
  else
    filesys_lock_deep++;
}
// TODO: make this part of current lock implementation
void filesys_release (void) {
  if (filesys_lock_deep == 0)
    lock_release (&filesys_lock);
  else
    filesys_lock_deep--;
}

/* Inits all structs required for syscalls. Also enables
 * 0x30 interrupt as syscalls.*/
void syscall_init (void) {
  /* structs MUST be initilizated before enabling syscalls. */
  filesys_lock_deep = 0;
  lock_init (&filesys_lock);
  next_fd = 2;                          /* 0 and 1 are reserved form stdo and stdi. */

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* A general handler for all syscalls. */
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
      exit_handler (-1);                           /* thread_exit calls process_exit */
      break;
  }
}

/* Validates if addr points to a valid memory byte for the
 * current user program.*/
bool is_valid_addr (void* addr) {
  return ptable_find_page (pg_round_down (addr)) != NULL;
}

/* this function is used to read an argument from stack. Offset param
 * is the current argument number. For Example:
 *    - 1st arg ---> offset = 1
 *    - 2nd arg ---> offset = 2
 *    - ...
 */
static uint32_t stack_int (int *esp, uint8_t offset) {
  void *addr = esp + offset;

  if (is_valid_addr (addr) && is_valid_addr (addr + sizeof (int)))
    return *(int *)addr;        /* addr is ok. */

  exit_handler (-1);
  NOT_REACHED ();
}

/* Reads argument of type `pointer` from stack and validates it.
 * Offset param is the current argument number. For Example:
 *    - 1st ptr ---> offset = 1
 *    - 2nd ptr ---> offset = 2
 *    - ...
 */
static void *stack_ptr (void *esp, uint8_t offset) {
  void *addr = (void*)stack_int (esp, offset);

  if (is_valid_addr (addr))
    return addr;                                        /* addr is ok. */

  exit_handler (-1);
  NOT_REACHED ();
}

static char *stack_str (void *esp, uint8_t offset) {
  char *addr = stack_ptr (esp, offset);
  char *tmp = addr;

  while (*tmp != '\0') {
    if (is_valid_addr (tmp + 1)) {
      tmp++;
    } else {
      exit_handler (-1);
      NOT_REACHED ();
    }
  }

  return addr;
}

/* Uses the file descriptor param `fd` to search a 
 * file opened by the current user program.*/
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
  char *buffer = stack_str (f->esp, 2);               /* mem[buffer] contains the string. */
  unsigned size = (unsigned)stack_int (f->esp, 3);    /* bytes to be writed. */

  if (fd == 1) {
    putbuf (buffer, size);                /* putbuf writes to console. */
    f->eax = size;
  } else {
    struct list_elem *elem = fd_get_file (fd);
    if (elem != NULL) {
      struct file *file = list_entry (elem, struct fd_elem, elem)->file;
 
      filesys_acquire ();
      f->eax = file_write (file, buffer, size);           /* write. */
      filesys_release ();
    } else {
      f->eax = -1;
    }
  }
}

void exit_handler (uint32_t exit_status) {
  struct thread *cur = thread_current();

  cur->exit_status = exit_status;        /* set my own exit status. */
  struct list *fds_list = &cur->fds;

  /* closes all opened files. */
  struct list_elem *e;
  for (e = list_begin (fds_list); e != list_end (fds_list); e = list_next (e)) {
    struct fd_elem *f = list_entry (e, struct fd_elem, elem);
    e = f->elem.prev;
    close_handler (f->fd);
  }

  /* free the sumplementary page table. */
  ptable_free_pages (cur); 

  thread_exit ();
  NOT_REACHED ();
}

void halt_handler (void) {
  shutdown_power_off ();               /* bye bye */
}

void exec_handler (struct intr_frame *f) {
  char* cmd_line = stack_str (f->esp, 1);
  tid_t tid = process_execute (cmd_line);
  f->eax = tid;
}

void wait_handler (struct intr_frame *f) {
  tid_t tid = stack_int (f->esp, 1);
  int exit_status = process_wait (tid);
  f->eax = exit_status;
}

void remove_handler (struct intr_frame *f) {
  char *name = stack_str (f->esp, 1);

  filesys_acquire ();
  f->eax = filesys_remove (name);
  filesys_release ();
}

void create_handler (struct intr_frame *f) {
  char* name = stack_str (f->esp, 1);
  off_t init_size = stack_int (f->esp, 2);

  filesys_acquire ();
  f->eax = filesys_create (name, init_size);
  filesys_release ();

}

void open_handler (struct intr_frame *f) {
  struct file *file;
  struct thread *cur;
  struct fd_elem *elem;
  char *filename;

  filename = stack_str (f->esp, 1);                 /* filename. */
  cur = thread_current ();

  filesys_acquire ();
  file = filesys_open (filename);            /* open the file. */

  if (file != NULL) {
    elem = malloc (sizeof (*elem));       /* reserve mem for file descriptor. */

    if (elem != NULL) {
      elem->file = file;
      elem->fd = next_fd++; 

      list_push_front (&cur->fds, &elem->elem);           /* register the file descriptor. */
      f->eax = elem->fd;                                  /* return the file descriptor. */

      filesys_release (); 
      return;
    } else {
      file_close (file);
    }
  }

  filesys_release ();
  f->eax = -1;                                            /* something went wrong. */
}

void filesize_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);                         /* file descriptor. */
  uint32_t size = -1;                                     /* size = error_value by default. */

  struct file *file = list_entry (fd_get_file (fd), struct fd_elem, elem)->file;
  if (file != NULL) {
    filesys_acquire ();
    size = file_length (file);                            /* calc file's size. */
    filesys_release ();
  }

  f->eax = size;
}

void read_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);                     /* file descriptor. */
  char *buffer = stack_str (f->esp, 2);
  uint32_t size = stack_int (f->esp, 3);

  /* verify buffer memory. */
  struct page *page = ptable_find_page (pg_round_down (buffer));

  if (page->writable) {
    if (fd == 0) {
      /* read from keyboard. */
      for (uint32_t i = 0; i < size; i++) {
        buffer[i] = input_getc (); 
      }
      f->eax = size;
    } else {
      /* read from file. */
      struct list_elem *elem = fd_get_file (fd);
      if (elem != NULL) {
        struct file *file = list_entry (elem, struct fd_elem, elem)->file;

        filesys_acquire ();
        f->eax = file_read (file, buffer, size);
        filesys_release ();
      } else {
        f->eax = -1;
      }
    } 
  } else {
    exit_handler (-1);
  }
}

void close_handler (int fd) {
  struct fd_elem *fd_elem = list_entry (fd_get_file (fd), struct fd_elem, elem);
  if (fd_elem != NULL) {
    filesys_acquire ();
    file_close (fd_elem->file);
    filesys_release ();

    list_remove (&fd_elem->elem);
    free (fd_elem);
  }
}

void seek_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);
  off_t new_pos = stack_int (f->esp, 2);

  struct list_elem *elem = fd_get_file (fd);
  if (elem != NULL) {
    struct file *file = list_entry (elem, struct fd_elem, elem)->file;

    filesys_acquire ();
    file_seek (file, new_pos);
    filesys_release ();
  }
}

void tell_handler (struct intr_frame *f) {
  int fd = stack_int (f->esp, 1);

  struct list_elem *elem = fd_get_file (fd);
  if (elem != NULL) {
    struct file *file = list_entry (elem, struct fd_elem, elem)->file;

    filesys_acquire ();
    f->eax = file_tell (file);
    filesys_release ();
  }
}

