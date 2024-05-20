#ifndef VM_PAGE_TABLE_H
#define VM_PAGE_TABLE_H

#include <list.h>
#include <stdint.h>
#include <filesys/off_t.h>
#include "threads/thread.h"

/* entry->flags & PTABLE_CODE means code should be found in this
 * page. In a page fault, kaddr works the file address.*/
#define PTABLE_CODE 0x01
#define PTABLE_STACK 0x02
#define PTABLE_SWAP 0x04

typedef uint8_t flag_t;

struct ptable_entry {
  struct thread *owner;
  struct list_elem elem;
  void *upage;              /* page (virtual) address. */
  void *kpage;              /* frame (physical) address. */
  flag_t flags;
  bool writable;

  /* if this entry is used as DATA or CODE,
   * then it contains information to recover from a page fault.*/
  struct ptable_code *code;
};

struct ptable_code {
  off_t ofs;
  uint32_t read_bytes;
};

/* creates a new entry in sumplemental page table. */
struct ptable_entry* ptable_create_entry (void *upage, void *kpage, flag_t flags);

/* returns an entry that corresponds to upage. */
struct ptable_entry* ptable_find_upage (void *upage);

/* deletes an existing entry in suplemental page table of
 * current process.*/
void ptable_delete_entry (struct ptable_entry *entry);

/* free all memory related with suplemental page table
 * of current process.*/
void ptable_free (void);

/* TODO: add a comment here */
struct ptable_entry* ptable_find_kpage (void *upage, struct thread *t);

#endif // !VM_PAGE_TABLE_H
