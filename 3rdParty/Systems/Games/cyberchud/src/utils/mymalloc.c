#include <assert.h>
#include <string.h>

#include "utils/mymalloc.h"
#include "text.h"
#include "utils/pad.h"

#ifndef NDEBUG
#include "xorshift.h"
static inline void randomize_mem(uint8_t* mem, size_t size) {
	xorshift_t testshift = {.a=size};
	for (size_t i=0; i<size/8; i++) {
		mem[i] = xorshift64(&testshift);
	}
}
#endif

void myfree(void* ptr) {
#ifndef NDEBUG
	myprintf("[myfree] ptr:0x%p\n", ptr);
#endif
	free((char*)ptr-offsetof(mymem_t, data));
}

void* mycalloc(size_t nmemb, size_t size) {
#ifndef NDEBUG
	myprintf("[mycalloc] req:%zu\n", size);
#endif
	const size_t req_size = nmemb*size;
	size_t actual_size = req_size+offsetof(mymem_t, data);
	actual_size += pad_skip_padding(actual_size, 64);
	mymem_t* ptr = aligned_alloc(64, actual_size);
	if (!ptr)
		myprintf("[ERR] [mycalloc] cannot aligned_alloc: %zu\n", actual_size);
	assert((uint64_t)ptr%64==0);
	ptr->size = req_size;
	ptr->actual_size = actual_size;
	memset(ptr->data, 0, req_size);
	return ptr->data;
}

void* mymalloc(size_t size) {
#ifndef NDEBUG
	myprintf("[mymalloc] req:%zu\n", size);
#endif
	size_t actual_size = size+offsetof(mymem_t, data);
	actual_size += pad_skip_padding(actual_size, 64);
	mymem_t* ptr = aligned_alloc(64, actual_size);
	if (!ptr)
		myprintf("[ERR] [mymalloc] cannot aligned_alloc: %zu\n", actual_size);
	ptr->size = size;
	ptr->actual_size = actual_size;
#ifndef NDEBUG
	randomize_mem(ptr->data, size);
#endif
	return ptr->data;
}

void* myrealloc(void* data, size_t size) {
#ifndef NDEBUG
	myprintf("[realloc] ptr:0x%p req:%zu\n", data, size);
#endif
	assert(size);
	size_t actual_size = size+offsetof(mymem_t, data);
	actual_size += pad_skip_padding(actual_size, 64);

	mymem_t* ptr;
	if (data) {
		mymem_t* const old_ptr = (mymem_t*)((char*)data-offsetof(mymem_t, data));
		if (old_ptr->actual_size < actual_size) {
			ptr = aligned_alloc(64, actual_size);
			if (!ptr) {
				myprintf("[ERR] [myrealloc] cannot aligned_alloc: %zu\n", actual_size);
				return NULL;
			}
			ptr->actual_size = actual_size;
#ifndef NDEBUG
			randomize_mem(ptr->data, size);
#endif
			memcpy(ptr->data, old_ptr->data, old_ptr->size);
			free(old_ptr);
		} else {
			ptr = old_ptr;
		}
	} else {
		ptr = aligned_alloc(64, actual_size);
		if (!ptr) {
			myprintf("[ERR] [myrealloc] cannot aligned_alloc: %zu\n", actual_size);
			return NULL;
		}
		ptr->actual_size = actual_size;
	}
	ptr->size = size;

	return ptr->data;
}
