#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "devices/block.h"
#include "vm/swap.h"
#include <list.h>

/* entry->flags & PTABLE_CODE means code should be found in this
 * page. In a page fault, kaddr works the file address.*/
#define PTABLE_CODE 0x01
#define PTABLE_STACK 0x02
#define PTABLE_SWAP 0x04

/* list of memory pages. */
extern struct list page_list;

typedef uint8_t flag_t;
struct page {
  struct thread *owner;
  struct list_elem elem;
  void *upage;                    /* page (virtual) address. */
  void *kpage;                    /* frame (physical) address. */
  struct swap_page *swap;     /* frame (physical) address. */

  flag_t flags;
  bool writable;
  bool used;                                        /* for clock algorithm. */

  /* if this entry is used as DATA or CODE,
   * then it contains information to recover from a page fault.*/
  struct ptable_code *code;
};

void page_init (void);

void page_lock_acquire (void);
void page_lock_release (void);

struct page* next_page_evict (void);
void* page_evict (void);

#endif
