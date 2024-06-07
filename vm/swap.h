#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include <list.h>
#include "threads/thread.h"

struct swap_page {
  block_sector_t sector;            /* swap page pos. */
  struct thread *owner;
  struct list_elem elem;
};

void swap_init (void);

void swap_load (struct swap_page *entry, void *kpage);
struct swap_page *swap_store (void *kpage, struct thread *owner);

void swap_free_page (struct swap_page *page);

#endif // !VM_SWAP_H
