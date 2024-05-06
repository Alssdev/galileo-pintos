#ifndef VM_PAGE_TABLE_H
#define VM_PAGE_TABLE_H

#include <list.h>
#include <stdint.h>
#include <filesys/off_t.h>

typedef uint8_t flag_t;

/* entry->flags & PTABLE_CODE means code should be found in this
 * page. In a page fault, kaddr works the file address.*/
static flag_t PTABLE_CODE = 0x01;

struct ptable_entry {
  struct list_elem elem;
  void *vaddr;              /* page (virtual) address. */
  void *kaddr;              /* frame (physical) address. */

  /* if this entry is used as DATA or CODE,
   * then it contains information to recover from a page fault.*/
  struct ptable_code *code;

  flag_t flags;
};

struct ptable_code {
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};

struct ptable_entry *ptable_get_page (void *vaddr);
bool ptable_set_page (void *vaddr, void *kaddr);
bool ptable_set_code (void *vaddr, off_t ofs, uint32_t read_bytes, bool writable);

#endif // !VM_PAGE_TABLE_H
