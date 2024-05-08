#include "vm/page_table.h"
#include "list.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/falloc.h"
#include <stdint.h>
#include <stdio.h>

void free_entry (struct ptable_entry *entry);

struct ptable_entry *ptable_new_page (void *upage, uint8_t flags) {
  ASSERT ((unsigned)upage % PGSIZE == 0);
  ASSERT (is_user_vaddr (upage));

  struct thread *cur = thread_current ();

  /* alloc a new frame. */
  void *kpage = falloc_get_page ();
  ASSERT (kpage != NULL);

  /* store entry in a non-pageable memory. */
  struct ptable_entry *entry = malloc (sizeof *entry);
  if (entry != NULL) {
    entry->upage = upage;
    entry->kpage = kpage;
    entry->flags = flags;
    entry->code = NULL;

    /* add to page table. */
    list_push_back (&cur->page_table, &entry->elem);
    return entry;
  } else {
    PANIC("kernel bug - malloc out of block.");
    NOT_REACHED ();
  }
}

struct ptable_entry *ptable_get_entry (void *upage) {
  ASSERT ((unsigned)upage % PGSIZE == 0);

  struct thread *cur = thread_current ();
  struct list_elem *elem;

  for (elem = list_begin (&cur->page_table); elem != list_end (&cur->page_table);
      elem = list_next(elem)) {
    struct ptable_entry *entry = list_entry (elem, struct ptable_entry, elem);

    if (entry->upage == upage) {
      return entry;
    }
  }

  return NULL;
}

/* TODO: add better comments. */
void ptable_new_code (void *upage, off_t ofs, uint32_t read_bytes, bool writable) {
  ASSERT ((unsigned)upage % PGSIZE == 0);
  ASSERT (is_user_vaddr (upage));

  struct thread *cur = thread_current (); 

  /* store entry in a non-pageable memory. */
  struct ptable_entry *entry = malloc (sizeof *entry);

  if (entry != NULL) {
    struct ptable_code *code = malloc (sizeof *code);

    if (code != NULL) {
      entry->upage = upage;
      entry->kpage = NULL;
      entry->flags = PTABLE_CODE;
      entry->code = code;

      code->ofs = ofs;
      code->read_bytes = read_bytes;
      code->writable = writable;

      /* add to page table. */
      list_push_back (&cur->page_table, &entry->elem);
      return;
    }
  }

  PANIC("kernel bug - malloc out of blocks.");
  NOT_REACHED ();
}

void ptable_free_entry (struct ptable_entry *entry) {
  if (entry->code != NULL)
    free (entry->code);

  if (entry->kpage != NULL)
    falloc_free_page (entry->kpage);

  list_remove (&entry->elem);
  free (entry);
}

void ptable_free_table (void) {
  struct thread *cur = thread_current ();
  struct ptable_entry *entry;

  while (!list_empty (&cur->page_table)) {
    entry = list_entry (list_begin (&cur->page_table), struct ptable_entry, elem);

    if (entry->code != NULL)
      free (entry->code);

    list_remove (&entry->elem);
    free (entry);
  }
}

