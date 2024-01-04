#include <assert.h>
#include <string.h>

#include "types/name_heap.h"
#include "utils/mymalloc.h"
#include "utils/pad.h"

static void name_heap_resize(name_heap_t* const heap, const size_t new_cap) {
	assert(new_cap > heap->cap);
	/* Malloc */
	size_t alloc_sz = 0;
	alloc_sz = pad_inc_count(sizeof(int32_t)*new_cap, 64);
	alloc_sz = pad_inc_count(sizeof(ent_name_t)*new_cap, 64);
	char* ptr = mymalloc(alloc_sz);

	/* Copy Old Mem */
	int32_t* new_idx_ptr = pad_inc_ptr(&ptr, sizeof(int32_t)*new_cap, 64);
	ent_name_t* new_names_ptr = (ent_name_t*)ptr;
	memcpy(new_idx_ptr, heap->ids, heap->len*sizeof(int32_t));
	memcpy(new_names_ptr, heap->names, heap->len*sizeof(name_heap_t));

	/* Free Old Mem */
	myfree(heap->ids);

	heap->cap = new_cap;
	heap->ids = new_idx_ptr;
	heap->names = new_names_ptr;
}

void init_name_heap(name_heap_t* const heap) {
	memset(heap, 0, sizeof(name_heap_t));
	heap->cap = 128;
	size_t alloc_sz = 0;
	alloc_sz = pad_inc_count(sizeof(int32_t)*heap->cap, 64);
	alloc_sz = pad_inc_count(sizeof(ent_name_t)*heap->cap, 64);
	char* ptr = mymalloc(alloc_sz);
	heap->ids = pad_inc_ptr(&ptr, sizeof(int32_t)*heap->cap, 64);
	heap->names = (ent_name_t*)ptr;
}

void put_name_heap(name_heap_t* const heap, const int32_t id, const char* name) {
	if (heap->len >= heap->cap)
		name_heap_resize(heap, heap->cap*2);
	const size_t idx = heap->len++;
	heap->ids[idx] = id;
	strncpy(heap->names[idx].str, name, NAME_HEAP_STR_LEN-1);
	heap->names[idx].str[NAME_HEAP_STR_LEN-1] = 0;
}
