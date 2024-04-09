#ifndef SYSCALL_HANDLERS_H
#define SYSCALL_HANDLERS_H

#include "threads/interrupt.h"

void write_handler (struct intr_frame *f);
void exec_handler (struct intr_frame *);
void halt_handler (void);
void wait_handler (struct intr_frame *);
void remove_handler (struct intr_frame *f);
void create_handler (struct intr_frame *f);
void open_handler (struct intr_frame *f);
void filesize_handler (struct intr_frame *f);
void read_handler (struct intr_frame *f);
void close_handler (int fd);
void seek_handler (struct intr_frame *f);
void tell_handler (struct intr_frame *f);
void remove_handler (struct intr_frame *f);

#endif // !SYSCALL_HANDLERS_H
