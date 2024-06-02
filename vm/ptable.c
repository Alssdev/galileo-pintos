#include "ptable.h"
#include "list.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <stdio.h>

struct page *ptable_new_entry (void *upage, bool writable, enum page_type type) {
  ASSERT (pg_ofs (upage) == 0);

  struct thread *cur = thread_current ();

  /* init page data. */
  struct page *page = malloc (sizeof *page);
  if (page != NULL) {
    page->owner = cur;
    page->upage = upage;
    page->kpage = NULL;
    page->type = type;
    page->is_writable = writable;

    page->ofs = 0;
    page->read_bytes = 0;

    page->is_swap = false;
    page->swap_sector = 0;

    list_push_front (&cur->page_table, &page->elem);

    return page;
  } else {
    PANIC ("kernel bug - malloc out of blocks.");
    NOT_REACHED ();
  }
}

struct page *ptable_find_entry (void *upage) {
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
