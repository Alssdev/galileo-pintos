#ifndef VM_PAGE_TABLE_H
#define VM_PAGE_TABLE_H

#include <list.h>
#include <stdint.h>
#include <filesys/off_t.h>

/* entry->flags & PTABLE_CODE means code should be found in this
 * page. In a page fault, kaddr works the file address.*/
#define PTABLE_CODE 0x01
#define PTABLE_STACK 0x02

typedef uint8_t flag_t;

struct ptable_entry {
  struct list_elem elem;
  void *upage;              /* page (virtual) address. */
  void *kpage;              /* frame (physical) address. */
  flag_t flags;

  /* if this entry is used as DATA or CODE,
   * then it contains information to recover from a page fault.*/
  struct ptable_code *code;
};

struct ptable_code {
  off_t ofs;
  uint32_t read_bytes;
  bool writable;
};

/* creates a new entry in sumplemental page table. */
struct ptable_entry* ptable_create_entry (void *upage, void *kpage, flag_t flags);

/* returns an entry that corresponds to upage. */
struct ptable_entry* ptable_find_entry (void *upage);

/* deletes an existing entry in suplemental page table of
 * current process.*/
void ptable_delete_entry (struct ptable_entry *entry);

/* free all memory related with suplemental page table
 * of current process.*/
void ptable_free (void);

/* struct ptable_entry* ptable_new_entry (void *upage, uint8_t flags); */
/* void ptable_new_code (void *upage, off_t ofs, uint32_t read_bytes, bool writable); */
/* struct ptable_entry *ptable_alloc_page (struct ptable_entry *entry); */

/* struct ptable_entry *ptable_get_entry (void *upage); */
/* void ptable_free_entry (struct ptable_entry *entry); */
/* void ptable_free_table (void); */

#endif // !VM_PAGE_TABLE_H
