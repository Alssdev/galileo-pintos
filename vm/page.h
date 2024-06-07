#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "devices/block.h"
#include "filesys/off_t.h"
#include "stdbool.h"
#include "swap.h"
#include <stdint.h>
#include <list.h>

extern struct lock evict_lock;

enum page_type {
  CODE,
  STACK
};

struct page {
  struct thread *owner;
  void *upage;              /* user page. */
  void *kpage;              /* kernel page. */
  enum page_type type;
  bool is_writable;
  struct lock evict;

  /* for page_list. Refer to page.h */
  struct list_elem allelem;             /* frame table. */

  /* for page table. */
  struct list_elem elem;                /* suplementary page table. != page table micro */

  /* code pages only. */
  off_t ofs;
  uint32_t read_bytes;

  /* swap. */
  struct swap_page *swap;
};

void page_init (void);
void page_alloc (struct page *page);      /* void * palloc_get_page (); */
void page_unblock (struct page *page);              
void page_remove (struct page *page);
void page_block (struct page *page);

#endif // !VM_PAGE_H
