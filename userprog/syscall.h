#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>

void syscall_init (void);
void exit_handler (uint32_t exit_status);

#endif /* userprog/syscall.h */
