#include "vm/falloc.h"
#include <bitmap.h>
#include "devices/block.h"
#include "list.h"
#include "stddef.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <debug.h>
#include <stdint.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "vm/page_table.h"
#include "vm/swap.h"
#include <random.h>
#include "userprog/pagedir.h"
#include "threads/interrupt.h"

struct list frame_table;                    /* Reserved physical frames. */
struct lock frame_lock;                     /* General lock for frame table. */
static int frame_lock_deep;

struct list_elem *clock_pos;                         /* clock algorithm. */

struct frame_entry {
  void *kpage;
  struct thread *owner;
  struct list_elem elem;
  bool used;                                        /* for clock algorithm. */
};

uint8_t ACCESSED_BIT = 0x01;
uint8_t DIRTY_BIT = 0x02;

struct ptable_entry* next_clock_algorithm (void);
void frame_acquire (void);
void frame_release (void);

void falloc_mark_page (void *kpage) {
  struct list_elem *elem;

  for (elem = list_begin (&frame_table); elem != list_end (&frame_table); elem = list_next (elem)) {
    struct frame_entry *entry = list_entry (elem, struct frame_entry, elem);

    if (entry->kpage == kpage) {
      entry->used = true;
      break;
    }
  }
}

// TODO: add a comment here
struct ptable_entry* next_clock_algorithm (void) {
  struct frame_entry *frame;

  /* first call */
  if (clock_pos == NULL)
    clock_pos = list_begin (&frame_table);

  while (true) {
    if (clock_pos == list_end (&frame_table))
      clock_pos = list_begin (&frame_table);

    frame = list_entry (clock_pos, struct frame_entry, elem);
    if (frame->used)
      frame->used = false;
    else
     break;

    clock_pos = list_next (clock_pos);
  }

  return ptable_find_kpage (frame->kpage, frame->owner);
}

// TODO: add a comment here
void frame_acquire (void) {
  enum intr_level old_level = intr_disable ();

  if (!lock_held_by_current_thread (&frame_lock))
    lock_acquire (&frame_lock);
  else
    frame_lock_deep++;

  intr_set_level (old_level);
}

// TODO: make this part of current lock implementation
void frame_release (void) {
  enum intr_level old_level = intr_disable ();

  if (frame_lock_deep == 0)
    lock_release (&frame_lock);
  else
    frame_lock_deep--;

  intr_set_level (old_level);
}

/* TODO: add a comment here. */
void frame_table_init (void) {
  list_init (&frame_table);
  lock_init (&frame_lock);

  clock_pos = NULL;
  swap_init ();
}

/* TODO: add a comment here. */
void *falloc_get_page (void) {
  void *kpage = palloc_get_page (PAL_USER | PAL_ZERO);

  if (kpage != NULL) {
    struct thread *cur = thread_current ();

    struct frame_entry *entry = malloc (sizeof *entry);
    if (entry != NULL) {
      entry->kpage = kpage;
      entry->owner = cur;
      entry->used = true;

      frame_acquire ();
      list_push_back (&frame_table, &entry->elem);
      frame_release ();
    } else {
      PANIC ("kernel bug - kernel out of memory.");
      NOT_REACHED ();
    }

    return kpage;
  } else {
    frame_acquire ();
    struct ptable_entry *page = next_clock_algorithm ();

    /* store memory data into swap file. */
    if (swap_push_page (page->kpage)) {
      /* modify suplemental page table. */
      if (page->writable)
        page->flags |= PTABLE_SWAP;

      /* modify real page table. */
      pagedir_clear_page (page->owner->pagedir, page->upage);

      /* free */
      falloc_free_page (page->kpage);
      page->kpage = NULL;

      frame_release ();

      PANIC ("kernel bug - code uncompleted.");
      NOT_REACHED ();
    } else {
      PANIC ("kernel bug - swap out of blocks.");
      NOT_REACHED ();
    }

    frame_release ();
  }
}

void falloc_free_page (void *kpage) {
  struct list_elem *elem;
  struct frame_entry *entry;
  bool entry_found;

  frame_acquire ();

  entry_found = false;

  for (elem = list_begin (&frame_table); elem != list_end (&frame_table); elem = list_next (elem)) {
    entry = list_entry (elem, struct frame_entry, elem);

    if (entry->kpage == kpage) {
      palloc_free_page (entry->kpage);
      list_remove (elem);
      free (entry);
      entry_found = true;
      break;
    }
  }

  if (!entry_found)
    PANIC("kernel bug - page not found on free.");

  frame_release ();
}

