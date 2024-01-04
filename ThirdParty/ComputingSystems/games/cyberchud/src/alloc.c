#include <assert.h>
#include <stdlib.h>

#define BUDDY_ALLOC_IMPLEMENTATION
#include "buddy_alloc.h"

#include "alloc.h"
#include "text.h"
#include "utils/myds.h"

static struct buddy* alloc_new_page(alloc_t* alloc) {
#ifndef NDEBUG
	myprintf("[alloc_new_page]\n");
#endif
	void* const mem = aligned_alloc(64, alloc->page_size);
	if (mem == NULL) {
		myprintf("[alloc_new_page] failed to aligned_alloc: %u\n", alloc->page_size);
		return NULL;
	}
	const size_t len = myarrlenu(alloc->pages);
	arrsetlen(alloc->pages, len+1);
	alloc->pages[len] = buddy_embed_alignment(mem, alloc->page_size, alloc->alignment);
	if (alloc->pages[len] == NULL) {
		myprintf("[alloc_new_page] failed to buddy_embed_alignment: mem:0x%lx size:%lu align:%lu\n", mem, alloc->page_size, alloc->alignment);
		return NULL;
	}
	return alloc->pages[len];
}

int alloc_init(alloc_t* alloc, size_t alignment, size_t page_size) {
#ifndef NDEBUG
	myprintf("[alloc_init] alignment:%lu page_size:%lu\n", alignment, page_size);
#endif
	alloc->alignment = alignment;
	alloc->page_size = page_size;
	arrsetcap(alloc->pages, 512); // 512=4096/8
	struct buddy* page = alloc_new_page(alloc);
	if (page == NULL) {
		myprintf("[alloc_init] failed to alloc: %u\n", alloc->page_size);
		return 1;
	}
	return 0;
}

void* balloc(alloc_t* alloc, size_t size) {
#ifdef VERBOSE
	myprintf("[balloc] size:%lu pages:%lu\n", size, myarrlenu(alloc->pages));
#endif
	assert(size+16 <= alloc->page_size);
	/* try to malloc with existing pages */
	for (size_t i=0; i<myarrlenu(alloc->pages); i++) {
		uint64_t* mem = buddy_malloc(alloc->pages[i], size+16);
		if (mem) {
			*mem = i;
			return mem+2;
		}
	}
	/* unable to malloc, create new page */
	alloc_new_page(alloc);
	return balloc(alloc, size);
}

void bfree(alloc_t* alloc, void* ptr) {
	size_t* nptr = (size_t*)((uint64_t*)ptr)-2;
	const size_t idx = *nptr;
	buddy_free(alloc->pages[idx], nptr);
}
