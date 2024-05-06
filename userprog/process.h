#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define MAX_ARGS_LEN 100
#include "threads/thread.h"
#include <filesys/off_t.h>

/* argument passing struct */
struct filename_args {
  char *file_name;              /* filename. */
  char *cmdline_copy;           /* a copy of cmdlines */
  char *args;                   /* reference of the start of args in cmdline_copy */
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* TODO: add a comment here. */
enum loading {
  LAZY,
  REAL
};
bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable, enum loading loading);

#endif /* userprog/process.h */
