#include "vm/falloc.h"
#include "list.h"
#include "stddef.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <debug.h>
#include <stdio.h>

struct list frame_table;                    /* Reserved physical frames. */
struct lock frame_lock;                     /* General lock for frame table. */
struct list entry_free_list;                /* Available memory for frame_entries. */

struct frame_entry {
  void *kernel_addr;
  struct thread *owner;
  struct list_elem elem;
};

struct entry_free_elem {
  struct list_elem elem;
  struct frame_entry entry_addr;
};

struct frame_entry *falloc_get_entry (void);

void frame_table_init (void) {
  list_init (&frame_table);
  list_init (&entry_free_list);
  lock_init (&frame_lock);
}

void *falloc_get_page (enum palloc_flags flags) {
  return falloc_get_multiple (flags, 1);
}

void *falloc_get_multiple (enum palloc_flags flags, size_t page_cnt) {
  void *pages;
  struct thread *cur;
  struct frame_entry *entry;

  pages = palloc_get_multiple (flags | PAL_ZERO, page_cnt);

  if (pages != NULL) {
    cur = thread_current ();

    for (size_t i = 0; i < page_cnt; i++) {
      entry = falloc_get_entry ();
      ASSERT (entry != NULL);

      entry->kernel_addr = pages + i * PGSIZE;
      entry->owner = flags & PAL_USER ? cur : NULL;

      /* printf ("0x%x\n", (unsigned)entry->kernel_addr); */

      lock_acquire (&frame_lock);
      list_push_back (&frame_table, &entry->elem);
      lock_release (&frame_lock);
    }

    return pages;
  } else {
    // TODO: aqui NO se esta liberando la memoria correctamente. 
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
        list_push_back (&entry_free_list, elem);
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

struct frame_entry *falloc_get_entry (void) {
  void *page;
  struct frame_entry *entry = NULL;

  lock_acquire (&frame_lock);

  if (list_empty (&entry_free_list)) {
    /* a page has to be reserved to save new frame entries */
    page = palloc_get_page (PAL_ZERO);

    if (page != NULL) {
      /* insert free frame entries into entry_free_list. */
      for (entry = page; (void*)(entry + 1) <= page + PGSIZE; entry++)
        list_push_back (&entry_free_list, &entry->elem);

      /* use one free entry to add `page` into the frame table. */
      entry = list_entry (list_pop_front (&entry_free_list), struct frame_entry, elem);
      entry->owner = NULL;
      entry->kernel_addr = page;
      list_push_front (&frame_table, &entry->elem);
    } else {
      PANIC ("kernel bug - kernel out of frames.");
      NOT_REACHED ();
    }
  }

  /* return one free frame entry. */
  entry = list_entry (list_pop_front (&entry_free_list), struct frame_entry, elem);

  lock_release (&frame_lock);
  return entry; 
}

