#include "vm/page_table.h"
#include "list.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <stdio.h>

struct ptable_entry *ptable_get_page (void *vaddr) {
  struct thread *cur = thread_current ();
  struct list_elem *elem;

  for (elem = list_begin (&cur->page_table); elem != list_end (&cur->page_table);
      elem = list_next(elem)) {
    struct ptable_entry *entry = list_entry (elem, struct ptable_entry, elem);

    // printf ("Amy entry %p\n", entry->vaddr);
    //
    if (entry->vaddr == vaddr) {
      return entry;
    }
  }

  return NULL;
}

/* TODO: add better comments. */
bool ptable_set_page (void *vaddr, void *kaddr) {
  struct thread *cur = thread_current (); 

  // TODO: verify this validations
  ASSERT (is_user_vaddr (vaddr));

  /* store entry in a non-pageable memory. */
  struct ptable_entry *entry = malloc (sizeof *entry);
  if (entry != NULL) {
    entry->vaddr = vaddr;
    entry->kaddr = kaddr;

    /* add to page table. */
    list_push_back (&cur->page_table, &entry->elem);
    return true;
  } else {
    return false;
  }
} 

/* TODO: add better comments. */
bool ptable_set_code (void *vaddr, off_t ofs, uint32_t read_bytes, bool writable) {

  struct thread *cur = thread_current (); 

  // TODO: verify this validations
  ASSERT (is_user_vaddr (vaddr));

  /* store entry in a non-pageable memory. */
  struct ptable_entry *entry = malloc (sizeof *entry);

  if (entry != NULL) {
    struct ptable_code *code = malloc (sizeof *code);

    if (code != NULL) {
      entry->vaddr = vaddr;
      entry->kaddr = NULL;
      entry->flags = PTABLE_CODE;
      entry->code = code;

      code->ofs = ofs;
      code->read_bytes = read_bytes;
      code->writable = writable;

      /* add to page table. */
      list_push_back (&cur->page_table, &entry->elem);

      return true;
    } else {
      free (entry);
    }
  }

  return false;
}

