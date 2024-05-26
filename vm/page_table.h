#ifndef VM_PAGE_TABLE_H
#define VM_PAGE_TABLE_H

#include <list.h>
#include <filesys/off_t.h>
#include "vm/page.h"
#include "threads/thread.h"


struct page* ptable_get_page (void *upage, bool writable, flag_t flags);
struct page* ptable_find_page (void *upage);

void ptable_free_pages (struct thread *t);

#endif // !VM_PAGE_TABLE_H

