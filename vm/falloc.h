#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"

void frame_table_init (void);

void *falloc_get_page (void);
void falloc_free_page (void* kernel_addr);

#endif // !VM_FRAME_H
