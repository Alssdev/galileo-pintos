#include "vm/page_table.h"
#include "list.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <stdio.h>

/* inserts a new entry in suplementary page table, but not reserves memory
 * for the page.*/
struct page* ptable_get_page (void *upage, bool writable, flag_t flags) {
  ASSERT (upage != NULL && (unsigned)upage % PGSIZE == 0);

  struct page *page = malloc (sizeof *page);
  if (page != NULL) {
    page->owner = thread_current ();
    page->upage = upage;
    page->kpage = NULL;
    page->swap = NULL;
    page->writable = writable;
    page->used = true;

    page->flags = flags;
    if (flags & PTABLE_CODE) {
      /* extra fields needed to handle page faults in code pages. */
      page->code = malloc (sizeof (struct page_code));
      if (page->code == NULL) {
        PANIC ("kernel bug - malloc out of blocks.");
        NOT_REACHED ();
      }
    } else {
      page->code = NULL;
    }

    page_lock_acquire ();
    list_push_back (&page_list, &page->elem);
    page_lock_release ();

    return page;
  } else {
    PANIC ("kernel bug - malloc out of blocks.");
    NOT_REACHED ();
  }
}

/* finds an entry in the suplementary page table of the userprog. */
struct page* ptable_find_page (void *upage) {
  ASSERT ((unsigned)upage % PGSIZE == 0);

  struct thread *cur = thread_current ();
  struct list_elem *elem;
  struct page *page_found = NULL;

  if (is_user_vaddr (upage)) {
    page_lock_acquire ();

    for (elem = list_begin (&page_list); elem != list_end (&page_list);
    elem = list_next (elem)) {
      struct page *page_i = list_entry (elem, struct page, elem);

      if (page_i->owner == cur) {
        if (page_i->upage == upage) {
          page_found = page_i;
          page_i->used = page_i->kpage != NULL;

          break;
        }
      }
    }

    page_lock_release ();
  }

  return page_found;
}

void ptable_free_pages (struct thread *t) {
  ASSERT (t != NULL);

  page_lock_acquire();

  struct list_elem *elem;
  for (elem = list_begin (&page_list); elem != list_end (&page_list);
  elem = list_next (elem)) {
    struct page *page = list_entry (elem, struct page, elem);

    if (page->owner == t) {
      list_remove (elem);
      elem = elem->prev;

      if (page->code != NULL) free (page->code);
      if (page->swap != NULL) swap_free_page (page->swap);
      free (page);
    }
  }

  page_lock_release();
}

