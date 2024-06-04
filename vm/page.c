#include "page.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <debug.h>
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "vm/swap.h"
#include <stdio.h>

/* list of virtual pages that are mapped to a memory frame. */
struct list page_list;
struct lock page_lock;
struct lock evict_lock;

static bool install_page (void *upage, void *kpage, bool writable);
static void* page_evict (void);

void page_init (void) {
  list_init (&page_list);
  lock_init (&page_lock);
  lock_init (&evict_lock);
}

void
page_alloc (struct page *page)
{
  ASSERT (page->kpage == NULL);
  ASSERT (pg_ofs (page->upage) == 0);

  /* try to alloc in easy way. */
  void *kpage = palloc_get_page (PAL_USER | PAL_ZERO);

  /* if not possible, then alloc in try hard mode. */
  if (kpage == NULL)
    kpage = page_evict ();

  /* assign frame to virtual page. */
  page->kpage = kpage;
  if (!install_page (page->upage, kpage, page->is_writable))
  {
    PANIC ("kernel bug - install a page in page table failed.");
    NOT_REACHED ();
  }
}

void
page_complete_alloc (struct page *page)
{
  lock_acquire (&page_lock);
  list_push_back (&page_list, &page->allelem);
  lock_release (&page_lock);
}

static void*
page_evict (void)
{
  lock_acquire (&evict_lock);
  void *kpage;
  struct thread *t;
  enum intr_level old_level;

  printf ("--ev\n"); 

  /* choose a page to evict. (FIFO) */
  lock_acquire (&page_lock);
  struct list_elem *elem = list_pop_front (&page_list);
  ASSERT (elem != NULL);
  struct page *page = list_entry (elem, struct page, allelem);
  kpage = page->kpage;
  t = page->owner;

  /* don't allow that thread to run. */
  old_level = intr_disable ();
  switch (t->status) {
    case THREAD_RUNNING:
      t = NULL;                 /* t is the current thread. */
      break;

    case THREAD_READY:
      t->swap_deep = 0;
      t->status = THREAD_EVICTION;
      list_remove (&t->elem);
      t->block_completed = true;
      break;

    case THREAD_BLOCKED:
      t->status = THREAD_EVICTION;
      t->block_completed = false;
      break;

    case THREAD_EVICTION:
      ASSERT ("what?? 1");
      t->swap_deep++;
      break;

    case THREAD_DYING:
      PANIC ("kernel bug - thread in eviction is dying.");
      break;
  }
  intr_set_level (old_level); 
  lock_release (&page_lock);

  printf ("  stop\n");

  /* move to swap. */
  if (page->is_writable)
    page->swap = swap_push_page (kpage, page->owner);

  printf ("  sw\n");

  /* remove from page table. */
  pagedir_clear_page (page->owner->pagedir, page->upage);
  page->kpage = NULL;

  if (t != NULL) {
    old_level = intr_disable ();

    if (t->swap_deep == 0) {
      t->status = THREAD_BLOCKED;
      if (t->block_completed)
        thread_unblock (t); 

    } else {
      ASSERT ("what?? 2");
      t->swap_deep--;
    }

    intr_set_level (old_level);
  }

  printf ("-en\n"); 
  lock_release (&evict_lock);
  return kpage;
}

void page_remove (struct page *page) {
  lock_acquire (&page_lock);

  if (page->kpage != NULL)
    list_remove (&page->allelem);

  if (page->swap != NULL)
    swap_free_page (page->swap);
    
  lock_release (&page_lock);
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
