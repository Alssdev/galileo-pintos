#ifndef VM_PTABLE_H
#define VM_PTABLE_H

#include "page.h"

struct page *ptable_new_entry (void *upage, bool writable, enum page_type type);
struct page *ptable_find_entry (void *upage);
void ptable_free_table (void);

#endif
