#include "vm/falloc.h"
#include "list.h"
#include "stddef.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <debug.h>
#include <stdio.h>
#include "threads/malloc.h"

struct list frame_table;                    /* Reserved physical frames. */
struct lock frame_lock;                     /* General lock for frame table. */

struct frame_entry {
  void *kernel_addr;
  struct thread *owner;
  struct list_elem elem;
};

void frame_table_init (void) {
  list_init (&frame_table);
  lock_init (&frame_lock);
}

void *falloc_get_page (void) {
  return falloc_get_multiple (1);
}

void *falloc_get_multiple (size_t page_cnt) {
  void *pages;
  struct thread *cur;
  struct frame_entry *entry;

  /* current page allocation. this allocator is for USER MEMORY ONLY. */
  pages = palloc_get_multiple (PAL_USER | PAL_ZERO, page_cnt);

  if (pages != NULL) {
    cur = thread_current ();

    for (size_t i = 0; i < page_cnt; i++) {
      /* register that this process requires all these pages. */
      entry = malloc (sizeof *entry);
      ASSERT (entry != NULL);

      entry->kernel_addr = pages + i * PGSIZE;
      entry->owner = cur;

      lock_acquire (&frame_lock);
      list_push_back (&frame_table, &entry->elem);
      lock_release (&frame_lock);
    }

    return pages;
  } else {
    PANIC ("kernel bug - kernel out of frames.");
    NOT_REACHED ();
  }
}

void falloc_free_page (void *kernel_addr) {
  falloc_free_multiple (kernel_addr, 1);
}

void falloc_free_multiple (void *kernel_addr, size_t page_cnt) {
  struct list_elem *elem;
  struct frame_entry *entry;
  bool entry_found;

  lock_acquire (&frame_lock);

  for (size_t i = 0; i < page_cnt; i ++) {
    entry_found = false;

    for (elem = list_begin (&frame_table); elem != list_end (&frame_table); elem = list_next(elem)) {
      entry = list_entry (elem, struct frame_entry, elem);

      if (entry->kernel_addr == kernel_addr + (i * PGSIZE)) {
        // palloc_free_page (entry->kernel_addr);
        list_remove (elem);
        free (entry);
        entry_found = true;
        break;
      }
    }

    if (!entry_found)
      PANIC("kernel bug - page not found on free.");
  }

  palloc_free_multiple (kernel_addr, page_cnt);

  lock_release (&frame_lock);
}

