#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "vm/page.h"

void falloc_get_page (struct page* page);
void falloc_free_page (struct page *page);

#endif // !VM_FRAME_H

