#include "vm/page_table.h"
#include "list.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/falloc.h"
#include <stdint.h>
#include <stdio.h>

/* void free_entry (struct ptable_entry *entry); */
struct ptable_entry* ptable_create_entry (void *upage, void *kpage, flag_t flags) {
  struct thread *cur = thread_current ();
  struct ptable_entry *entry;
  struct ptable_code *ecode;

  ASSERT ((unsigned)upage % PGSIZE == 0);
  ASSERT (kpage == NULL || (unsigned)kpage % PGSIZE == 0);
  ASSERT (is_user_vaddr (upage));

  entry = malloc (sizeof *entry);
  if (entry != NULL) {
    entry->upage = upage;
    entry->kpage = kpage;
    entry->flags = flags;
    entry->writable = true;

    if (flags & PTABLE_CODE) {
      /* this page refers to a code segment page. */
      ecode = malloc (sizeof *ecode);

      if (ecode != NULL) {
        entry->code = ecode;
        ecode->ofs = 0;
        ecode->read_bytes = 0;
      } else {
        PANIC ("ptable bug - malloc out of blocks.");
        NOT_REACHED ();
      }
    } else {
      entry->code = NULL;
    }

    list_push_back (&cur->page_table, &entry->elem);

    return entry;
  } else {
    PANIC ("ptable bug - malloc out of blocks.");
    NOT_REACHED ();
  }
}

struct ptable_entry* ptable_find_entry (void *upage) {
  struct thread *cur = thread_current ();
  struct list_elem *elem;

  ASSERT ((unsigned)upage % PGSIZE == 0);

  if (is_user_vaddr (upage)) {
    for (elem = list_begin (&cur->page_table); elem != list_end (&cur->page_table);
    elem = list_next (elem)) {
      struct ptable_entry *entry = list_entry (elem, struct ptable_entry, elem);

      if (entry->upage == upage) {
        return entry;
      }
    }
  }

  return NULL;
}

void ptable_delete_entry (struct ptable_entry *entry) {
  ASSERT(entry != NULL);

  if (entry->code != NULL)
    free (entry->code);

  list_remove (&entry->elem);
  free (entry);
}

void ptable_free (void) {
  struct thread *cur = thread_current ();
  struct ptable_entry *entry;

  while (!list_empty (&cur->page_table)) {
    entry = list_entry (list_begin (&cur->page_table), struct ptable_entry, elem);
    ptable_delete_entry (entry);
  }
}

