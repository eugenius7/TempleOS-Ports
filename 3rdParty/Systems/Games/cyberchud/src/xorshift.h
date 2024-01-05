#ifndef XORSHIFT_H
#define XORSHIFT_H

#include <stdint.h>
#include "cglm/vec3.h"

typedef struct {
	uint64_t a;
} xorshift_t;

static inline uint64_t xorshift64(xorshift_t* state) {
	uint64_t x = state->a;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	return state->a = x;
}

static inline uint64_t xrand(xorshift_t* state, const uint64_t mod) {
	return xorshift64(state)%mod;
}

static inline float frand(xorshift_t* state) {
	return (float)xorshift64(state)/(float)(UINT64_MAX/2)-1.0f;
}

static inline void rand_vec3(xorshift_t* const state, vec3 out) {
	out[0] = frand(state);
	out[1] = frand(state);
	out[2] = frand(state);
	glm_vec3_normalize(out);
}

#endif
