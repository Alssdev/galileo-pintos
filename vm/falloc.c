#include "vm/falloc.h"
#include "stddef.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include <debug.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "vm/page.h"

/* TODO: add a comment here. */
void falloc_get_page (struct page* page) {
  ASSERT (page != NULL);
  ASSERT (page->upage != NULL);

  /* a valid pointer if there is an available free page. */
  page->kpage = palloc_get_page (PAL_USER | PAL_ZERO);

  if (page->kpage == NULL) {
    // printf ("[R2D2] swap??? \n");
    /* a valid pointer if there is an available free swap page. */
    page->kpage = page_evict ();

    if (page->kpage == NULL) {
      /* not enought memory. */
      PANIC ("kernel bug - swap out of sectors.");
      NOT_REACHED ();
    }
  }
}

/* free a memory page, but not the struct page. */
void falloc_free_page (struct page *page) {
  page_lock_acquire ();

  ASSERT (page->kpage != NULL);
  palloc_free_page (page->kpage);
  page->kpage = NULL;
  page->used = false;

  page_lock_release ();
}

