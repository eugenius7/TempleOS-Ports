#ifndef ALLOC_H
#define ALLOC_H

#include "buddy_alloc.h"

typedef struct {
	size_t alignment;
	size_t page_size;
	struct buddy** pages;
} alloc_t;

int alloc_init(alloc_t* alloc, size_t alignment, size_t page_size);
void* balloc(alloc_t* alloc, size_t size);
void bfree(alloc_t* alloc, void* ptr);

#endif
