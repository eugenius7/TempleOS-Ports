#ifndef UTILS_PAD_H
#define UTILS_PAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef VERBOSE
#include "text.h"
#endif

static inline int pad_skip_padding(const uintptr_t reader, const int align) {
	int align_mod = reader%align;
	if (align_mod != 0) {
		align_mod = align-align_mod;
#ifdef VERBOSE
		myprintf("[skip_padding] skipping: %lu %p\n", align_mod, reader);
#endif
	}
	return align_mod;
}

static inline size_t pad_inc_count(size_t size, const int align) {
	return size + pad_skip_padding(size, align);
}

static inline void* pad_inc_ptr(char** ptr, size_t size, const int align) {
	char* const ret = *ptr;
	uintptr_t addr = (uintptr_t)ret;
	addr += size;
	addr += pad_skip_padding(addr, align);
	*ptr = (char*)addr;
	return ret;
}

#endif
