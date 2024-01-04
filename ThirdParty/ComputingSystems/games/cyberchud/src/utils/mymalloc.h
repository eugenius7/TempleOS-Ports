#ifndef MYMALLOC_H
#define MYMALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
	uint64_t size;
	uint64_t actual_size;
	uint64_t pad[6];
	uint8_t data[];
} mymem_t;

void* mycalloc(size_t nmemb, size_t size);
void* mymalloc(size_t size);
void* myrealloc(void* data, size_t size);
void myfree(void* ptr);

#endif
