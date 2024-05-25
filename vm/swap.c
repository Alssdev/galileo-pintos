#include "vm/swap.h"
#include <stdio.h>
#include "list.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#define SWAP_SIZE 1024          /* swap file size in PAGES. */
#define SECTORS_PER_PAGE 8          /* swap file size in PAGES. */

struct lock swap_lock;
struct list swap_free;

/* inits data needed for swap to work propertly. */
void swap_init (void) {
  /* the swap table is a list... */
  list_init (&swap_free);
  lock_init (&swap_lock);

  lock_acquire (&swap_lock);

  /* registers how many blocks are available. */
  for (block_sector_t i = 0; i < SWAP_SIZE; i ++) {
    struct swap_page *page = malloc (sizeof *page);

    /* initial data. */
    page->sector = i * SECTORS_PER_PAGE;
    page->owner = NULL;

    list_push_back (&swap_free, &page->elem);
  }

  lock_release (&swap_lock);
}

/* Stores PGSIZE bytes from kpage into the swap file. If the data
 * is stored returns true, false otherwise. */
struct swap_page *swap_push_page (void *kpage, struct thread *owner) {
  ASSERT (kpage != NULL);
  ASSERT (pg_ofs (kpage) == 0);

  struct block *swap_block = block_get_role (BLOCK_SWAP);

  /* finding an empty space in swap file.. */
  lock_acquire (&swap_lock);
  struct list_elem *elem;
  if (!list_empty (&swap_free))
    elem = list_pop_front (&swap_free);
  else
    return  NULL;
  lock_release (&swap_lock);

  struct swap_page *swap = list_entry (elem, struct swap_page, elem);
  ASSERT (swap->owner == NULL);
  swap->owner = owner;

  /* copy memory data. No synchronization needed. */
  for (int i = 0; i < SECTORS_PER_PAGE; i++)
    block_write (swap_block, swap->sector + i, kpage + i * BLOCK_SECTOR_SIZE);

  return swap;
}

/* reads PGSIZE bytes FROM swap_file into MEM[kpage]. */
void swap_pop_page (struct swap_page *swap, void *kpage) {
  ASSERT (swap != NULL);
  ASSERT (kpage != NULL);
  ASSERT (pg_ofs (kpage) == 0);

  struct block *swap_block = block_get_role (BLOCK_SWAP);

  for (int i = 0; i < SECTORS_PER_PAGE; i++)
    block_read (swap_block, swap->sector + i, kpage + i * BLOCK_SECTOR_SIZE);

  /* mark sectors as free. */
  lock_acquire (&swap_lock);
  swap->owner = NULL;
  list_push_back (&swap_free, &swap->elem);
  lock_release (&swap_lock);
}

