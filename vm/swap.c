#include "vm/swap.h"
#include "devices/block.h"
#include <bitmap.h>
#include <stdio.h>
#include "list.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#define SWAP_SIZE 1024          /* swap file size in PAGES. */

struct lock swap_lock;
struct list swap_table;

/* inits data needed for swap to work propertly. */
void swap_init (void) {
  ASSERT (PGSIZE % BLOCK_SECTOR_SIZE == 0);

  /* the swap table is a list... */
  list_init (&swap_table);
  lock_init (&swap_lock);

  lock_acquire (&swap_lock);

  /* registers how many blocks are available. */
  int sector_i = 0; 
  while (sector_i < SWAP_SIZE) {
    struct swap_page *page = malloc (sizeof *page);
    page->is_free = true;
    page->sector = sector_i;

    list_push_back (&swap_table, &page->elem);
    sector_i += PGSIZE / BLOCK_SECTOR_SIZE;
  }

  lock_release (&swap_lock);
}

/* Stores PGSIZE bytes from kpage into the swap file. If the data
 * is stored returns true, false otherwise. */
struct swap_page *swap_push_page (void *kpage) {
  struct list_elem *elem;
  struct swap_page *swap_page = NULL;
  struct block *swap_block = block_get_role (BLOCK_SWAP);

  ASSERT (kpage != NULL);
  ASSERT ((unsigned)kpage % PGSIZE == 0);

  lock_acquire (&swap_lock);

  /* finding an empty space in swap file.. */
  elem = list_begin (&swap_table);
  while (elem != list_end (&swap_table)) {
    swap_page = list_entry (elem, struct swap_page, elem);
    if (swap_page->is_free)
      break;              /* empty space found! */
  }

  /* empty space found. */
  if (swap_page != NULL) {
    /* copy memory data. */
    for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
      block_write (swap_block, swap_page->sector + i, kpage + i * BLOCK_SECTOR_SIZE);

    /* mark sectors as used. */
    swap_page->is_free = false;
  }

  lock_release (&swap_lock);
  return swap_page;
}

/* reads PGSIZE bytes FROM swap_file into MEM[kpage]. */
void swap_pop_page (struct swap_page *swap, void *kpage) {
  ASSERT (swap != NULL);
  ASSERT (kpage != NULL);
  ASSERT ((unsigned)kpage % PGSIZE == 0);

  struct block *swap_block = block_get_role (BLOCK_SWAP);

  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read (swap_block, swap->sector + i, kpage + i * BLOCK_SECTOR_SIZE);

}

