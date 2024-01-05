#ifndef BITARR_H
#define BITARR_H

#include <stddef.h>
#include <stdint.h>

#define BITARR_SIZE 32
#define BITARR_SHIFT 5

#define BITARR_TYPE uint32_t

static inline int bitarr_get(const BITARR_TYPE* const data, size_t idx) {
	BITARR_TYPE mask = 1u<<(idx%BITARR_SIZE);
	idx >>= BITARR_SHIFT;
	return data[idx]&mask;
}

static inline void bitarr_set(BITARR_TYPE* const data, size_t idx) {
	BITARR_TYPE mask = 1u<<(idx%BITARR_SIZE);
	idx >>= BITARR_SHIFT;
	data[idx] |= mask;
}

static inline void bitarr_clear(BITARR_TYPE* const data, size_t idx) {
	BITARR_TYPE mask = 1u<<(idx%BITARR_SIZE);
	idx >>= BITARR_SHIFT;
	data[idx] &= ~mask;
}

static inline size_t bitarr_get_pos(size_t idx) {
	return (idx+BITARR_SIZE-1)>>BITARR_SHIFT;
}

static inline size_t bitarr_get_size(size_t cnt) {
	return ((cnt+BITARR_SIZE-1)>>BITARR_SHIFT)*sizeof(BITARR_TYPE);
}

static inline size_t bitarr_get_cnt(size_t cnt) {
	return ((cnt+BITARR_SIZE-1)>>BITARR_SHIFT);
}

#endif
