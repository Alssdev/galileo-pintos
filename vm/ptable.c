#include "ptable.h"
#include "list.h"
#include "page.h"
#include "swap.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <stdio.h>

struct page *page_create (void *upage, bool writable, enum page_type type) {
  ASSERT (pg_ofs (upage) == 0);

  struct thread *cur = thread_current ();

  /* init page data. */
  struct page *page = malloc (sizeof *page);
  if (page != NULL) {
    page->owner = cur;
    page->upage = upage;
    page->kpage = NULL;
    page->type = type;              /* CODE || STACK */
    page->is_writable = writable;
    lock_init (&page->evict);

    /* type == CODE */
    page->ofs = 0;
    page->read_bytes = 0;

    page->swap = NULL;

    /* add to suplementary page table. */
    list_push_front (&cur->page_table, &page->elem);

    return page;
  } else {
    PANIC ("kernel bug - malloc out of blocks.");
    NOT_REACHED ();
  }
}

struct page *page_find (void *upage) {
  ASSERT (pg_ofs(upage) == 0);

  struct thread *cur = thread_current ();

  for (struct list_elem *elem = list_begin (&cur->page_table); elem != list_end (&cur->page_table);
       elem = list_next (elem)) {
    struct page *page = list_entry (elem, struct page, elem);

    if (page->upage == upage)
      return page;
  }

  return NULL;
}


void page_free_pages (void) {
  struct thread *cur = thread_current ();

  lock_acquire (&evict_lock);

  for (struct list_elem *elem = list_begin (&cur->page_table); elem != list_end (&cur->page_table);
       elem = list_next (elem)) {
    struct page *page = list_entry (elem, struct page, elem);

    /* remove from suplementary page table */
    list_remove (elem);
    elem = elem->prev;

    /* remove from page_list(frame table) and swap. */
    page_remove (page);
    free (page);
  }

  lock_release (&evict_lock);
}

