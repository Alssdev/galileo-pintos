#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "stdbool.h"
#include <stdint.h>
#include <list.h>

struct swap_page {
  uint32_t sector;            /* swap page pos. */
  bool is_free;
  struct list_elem elem;
};

void swap_init (void);

void swap_pop_page (struct swap_page *entry, void *kpage);
bool swap_push_page (void *kpage);

#endif // !VM_SWAP_H
