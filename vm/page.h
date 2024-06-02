#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "devices/block.h"
#include "filesys/off_t.h"
#include "stdbool.h"
#include <stdint.h>
#include <list.h>

enum page_type {
  CODE,
  STACK
};

struct page {
  struct thread *owner;
  void *upage;
  void *kpage;
  enum page_type type;
  bool is_writable;

  /* for page_list. Refer to page.h */
  struct list_elem allelem;

  /* for page table. */
  struct list_elem elem;

  /* code pages only. */
  off_t ofs;
  uint32_t read_bytes;

  /* swap. */
  bool is_swap;
  block_sector_t swap_sector;
};

void page_init (void);
void page_alloc (struct page *page);
void page_complete_alloc (struct page *page);

#endif // !VM_PAGE_H
