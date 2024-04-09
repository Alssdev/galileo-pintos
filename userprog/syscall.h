#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>

void syscall_init (void);
void exit_handler (uint32_t exit_status);

void filesys_acquire (void);
void filesys_release (void);
#endif /* userprog/syscall.h */
