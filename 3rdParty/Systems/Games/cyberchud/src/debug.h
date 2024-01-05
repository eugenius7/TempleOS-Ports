#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>
#include "cglm/struct/vec2.h"
#include "cglm/struct/vec3.h"

static inline void vec1_assert(const float vec) {
#ifndef NDEBUG
	assert(!__builtin_isnan(vec));
	assert(vec != INFINITY);
#endif
}

static inline void vec1_assert_zeroone(const float vec) {
#ifndef NDEBUG
	assert(!__builtin_isnan(vec));
	assert(!__builtin_isinf(vec));
	assert(vec >= 0.0f);
	assert(vec <= 1.0f);
#endif
}

static inline void vec1_assert_norm(const float vec) {
#ifndef NDEBUG
	assert(!__builtin_isnan(vec));
	assert(!__builtin_isinf(vec));
	assert(vec >= -1.0f);
	assert(vec <= 1.0f);
#endif
}

static inline void vec2_assert(const vec2 vec) {
#ifndef NDEBUG
	for (int i=0; i<2; i++) {
		assert(!__builtin_isnan(vec[i]));
		assert(vec[i] != INFINITY);
	}
#endif
}

static inline void vec2_assert_zeroone(const vec2 vec) {
#ifndef NDEBUG
	for (int i=0; i<2; i++) {
		assert(!__builtin_isnan(vec[i]));
		assert(vec[i] != INFINITY);
		assert(vec[i] >= 0.0f);
		assert(vec[i] <= 1.0f);
	}
#endif
}

static inline void vec2_assert_norm(const vec2 vec) {
#ifndef NDEBUG
	for (int i=0; i<2; i++) {
		assert(!__builtin_isnan(vec[i]));
		assert(vec[i] != INFINITY);
		assert(vec[i] >= -1.0f);
		assert(vec[i] <= 1.0f);
	}
#endif
}

static inline void vec3_assert(const vec3 vec) {
#ifndef NDEBUG
	for (int i=0; i<3; i++) {
		assert(!__builtin_isnan(vec[i]));
		assert(!__builtin_isinf(vec[i]));
	}
#endif
}

static inline void vec3_assert_norm(const vec3 vec) {
#ifndef NDEBUG
	for (int i=0; i<3; i++) {
		assert(!__builtin_isnan(vec[i]));
		assert(!__builtin_isinf(vec[i]));
		assert(vec[i] >= -1.0f);
		assert(vec[i] <= 1.0f);
	}
#endif
}

static inline void vec3_assert_zero_one(const vec3 vec) {
#ifndef NDEBUG
	for (int i=0; i<3; i++) {
		assert(!__builtin_isnan(vec[i]));
		assert(!__builtin_isinf(vec[i]));
		assert(vec[i] >= 0.0f);
		assert(vec[i] <= 1.0f);
	}
#endif
}

#endif
