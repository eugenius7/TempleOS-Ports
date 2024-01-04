#ifndef SHADER_UTILS_H
#define SHADER_UTILS_H

#include <assert.h>
#include "vec.h"

static inline void glm_vec3_cross2(const vec2 a, const vec2 b, vec3 dest) {
	dest[0] = a[1] - b[1];
	dest[1] = b[0] - a[0];
	dest[2] = a[0] * b[1] - a[1] * b[0];
}
static inline float glm_vec3_dot2(const vec3 a, const vec2 b) {
	return a[0] * b[0] + a[1] * b[1] + a[2];
}

static int barycentric(const vec2s* tri, float x, float y, vec3s* const out) {
	mat3 cross;
	glm_vec3_cross2(tri[1].raw, tri[2].raw, cross[0]);
	glm_vec3_cross2(tri[2].raw, tri[0].raw, cross[1]);
	glm_vec3_cross2(tri[0].raw, tri[1].raw, cross[2]);
	const float determinant = glm_vec3_dot2(cross[2], tri[2].raw);
	if (determinant>-1e-3)
		return 1;

	const float invDeterminant = 1.0f / determinant;
	glm_mat3_scale(cross, invDeterminant);
	glm_mat3_transpose(cross);

	out->x = cross[0][0] * x + cross[1][0] * y + cross[2][0];
	out->y = cross[0][1] * x + cross[1][1] * y + cross[2][1];
	out->z = cross[0][2] * x + cross[1][2] * y + cross[2][2];
	return 0;
}

static inline int wrap(int i, int i_max) {
	return ((i % i_max) + i_max) % i_max;
}

static inline uint8_t frag_calc_color(float val, int pal) {
	return val*127.0f + 128*pal;
}

#endif
