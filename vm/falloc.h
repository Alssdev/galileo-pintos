#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"

void frame_table_init (void);

void *falloc_get_page (enum palloc_flags flags);
void *falloc_get_multiple (enum palloc_flags flags, size_t page_cnt);

void falloc_free_page (void* kernel_addr);
void falloc_free_multiple (void *kernel_addr, size_t page_cnt);

#endif // !VM_FRAME_H
