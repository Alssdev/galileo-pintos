#include <stdint.h>

struct swap_page {
  struct thread *owner;       /* page owner. (A userprog) */
  void *upage;                /* user page address. */
  uint32_t pos;               /* swap page pos. */
};

void swap_init (void);

void swap_pop_page (struct swap_page *entry, void *kpage);
void swap_push_page (void *kpage);
