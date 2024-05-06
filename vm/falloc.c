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
  uint8_t flags;
};

uint8_t ACCESSED_BIT = 0x01;
uint8_t DIRTY_BIT = 0x02;

void frame_table_init (void) {
  list_init (&frame_table);
  lock_init (&frame_lock);
}

void *falloc_get_page (void) {
  void *pages;
  struct thread *cur;
  struct frame_entry *entry;

  /* current page allocation. this allocator is for USER MEMORY ONLY. */
  pages = palloc_get_multiple (PAL_USER | PAL_ZERO, 1);

  if (pages != NULL) {
    cur = thread_current ();

    /* register that this process requires all these pages. */
    entry = malloc (sizeof *entry);

    if (entry != NULL) {
      entry->kernel_addr = pages;
      entry->owner = cur;

      lock_acquire (&frame_lock);
      list_push_back (&frame_table, &entry->elem);
      lock_release (&frame_lock);
    } else {
      PANIC ("kernel bug - kernel out of memory.");
    }

    return pages;
  } else {
    PANIC ("kernel bug - kernel out of frames.");
    NOT_REACHED ();
  }
}

void falloc_free_page (void *kernel_addr) {
  struct list_elem *elem;
  struct frame_entry *entry;
  bool entry_found;

  lock_acquire (&frame_lock);

  entry_found = false;

  for (elem = list_begin (&frame_table); elem != list_end (&frame_table); elem = list_next(elem)) {
    entry = list_entry (elem, struct frame_entry, elem);

    if (entry->kernel_addr == kernel_addr) {
      palloc_free_page (entry->kernel_addr);
      list_remove (elem);
      free (entry);
      entry_found = true;
      break;
    }
  }

  if (!entry_found)
    PANIC("kernel bug - page not found on free.");

  lock_release (&frame_lock);
}

