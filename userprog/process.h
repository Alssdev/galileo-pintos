#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#define MAX_ARGS_LEN 100
#include "threads/thread.h"

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

#endif /* userprog/process.h */
