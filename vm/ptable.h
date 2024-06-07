#ifndef VM_PTABLE_H
#define VM_PTABLE_H

#include "page.h"

struct page *page_create (void *upage, bool writable, enum page_type type);
struct page *page_find (void *upage);       /* suplementary page table. */
void page_free_pages (void);

#endif
