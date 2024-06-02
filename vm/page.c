#include "page.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <debug.h>
#include "threads/thread.h"
#include "userprog/pagedir.h"

/* list of virtual pages that are mapped to a memory frame. */
struct list page_list;
struct lock page_lock;

static bool install_page (void *upage, void *kpage, bool writable);
static void* page_evict (void);

void page_init (void) {
  list_init (&page_list);
  lock_init (&page_lock);
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
  PANIC ("kernel bug - page evict.");
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
