#include "vm/page.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "vm/falloc.h"
#include "threads/thread.h"
#include <stdio.h>

struct list page_list;                    /* Reserved physical frames. */
struct lock page_lock;                     /* General lock for frame table. */
static int page_lock_deep;

struct list_elem *clock_pos;                         /* clock algorithm. */

/* returns a candidate page to be evicted. */
struct page* next_page_evict (void) {
  struct page *page;

  page_lock_acquire ();

  while (true) {
    if (clock_pos == list_end (&page_list))
      clock_pos = list_begin (&page_list);

    page = list_entry (clock_pos, struct page, elem);
    if (page->kpage == NULL || page->used)
      page->used = false;
    else
     break;

    clock_pos = list_next (clock_pos);
  }
  page_lock_release ();


  return page;
}

void* page_evict (void) {
  page_lock_acquire ();

  struct page *page = next_page_evict ();
  unsigned replaced_page = (unsigned)page->kpage;

  if (page->writable) {
    struct swap_page *swap_page = swap_push_page (page->kpage);
    if (swap_page != NULL) {
      /* modify suplemental page table. */
      page->flags |= PTABLE_SWAP;

      /* modify real page table. */
      pagedir_clear_page (page->owner->pagedir, page->upage);
      page->kpage = NULL;
      page->swap = swap_page;

      page_lock_release ();

    } else {
      PANIC ("kernel bug - swap out of blocks.");
      NOT_REACHED ();
    }
  }

  return (void *)replaced_page;
}

/* TODO: add a comment here. */
void page_lock_acquire (void) {
  enum intr_level old_level = intr_disable ();

  if (!lock_held_by_current_thread (&page_lock))
    lock_acquire (&page_lock);
  else
    page_lock_deep++;

  intr_set_level (old_level);
}

/* TODO: add a comment here. */
void page_lock_release (void) {
  enum intr_level old_level = intr_disable ();

  if (page_lock_deep == 0)
    lock_release (&page_lock);
  else
    page_lock_deep--;

  intr_set_level (old_level);
}

/* TODO: add a comment here. */
void page_init (void) {
  list_init (&page_list);
  lock_init (&page_lock);

  page_lock_deep = 0;
  clock_pos = list_end (&page_list);
}
