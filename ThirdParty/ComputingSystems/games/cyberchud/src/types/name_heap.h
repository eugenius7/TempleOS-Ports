#ifndef TYPES_NAME_HEAP_H
#define TYPES_NAME_HEAP_H

#include <stddef.h>
#include <stdint.h>

#define NAME_HEAP_STR_LEN 32

typedef struct {
	char str[NAME_HEAP_STR_LEN];
} ent_name_t;

typedef struct {
	size_t len;
	size_t cap;
	int32_t *ids;
	ent_name_t *names;
} name_heap_t;

void init_name_heap(name_heap_t* const heap);
void put_name_heap(name_heap_t* const heap, const int32_t id, const char* name);

#endif
