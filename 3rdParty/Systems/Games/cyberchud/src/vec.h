#ifndef VEC_H
#define VEC_H

#include <math.h>
#include "macro.h"
#include "cglm/struct/vec2.h"
#include "cglm/struct/vec3.h"
#include "cglm/struct/vec4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "stolenlib.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define STATUSLINE_H 8
#define FB_H (SCREEN_H-STATUSLINE_H)

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

typedef struct {
	int32_t x1;
	int32_t y1;
	int32_t x2;
	int32_t y2;
} rect_i32;

typedef struct {
	int16_t x1;
	int16_t y1;
	int16_t x2;
	int16_t y2;
} rect_i16;

typedef struct {
	int16_t x;
	int16_t y;
} vec2_i16;

typedef struct {
	vec3s p[3];
} tri_t;

typedef struct {
	vec3s p[2];
	uint32_t color;
} line_t;

typedef struct {
	vec3s min;
	vec3s max;
} bbox_t;

typedef struct {
	vec3s pos;
	vec3s dir;
} ray_t;

typedef struct {
	u8 r;
	u8 g;
	u8 b;
} rgb_u8_t;

typedef struct {
	int16_t w;
	int16_t h;
	const uint8_t *lightmap;
	const rgb_u8_t *luxmap;
} bsp_litinfo_t;

typedef struct {
	vec4s vert[3];
} vec4_3;

typedef struct {
	vec3s norm;
	float dist;
} plane_t;

static inline float clamp(float val, const float min, const float max) {
	if (val > max) val = max;
	else if (val < min) val = min;
	return val;
}

static inline int32_t clamp_i32(int32_t val, const int32_t min, const int32_t max) {
	if (val > max) val = max;
	else if (val < min) val = min;
	return val;
}

static inline bool collide_aabb_point(const rect_i16 r1, const vec2_i16 p) {
	if (p.x < r1.x1 || p.x >= r1.x2 || p.y < r1.y1 || p.y >= r1.y2)
		return false;
	return true;
}

#if 0
static inline double vec3d_dot(const vec3s v0, const vec3s v1) {
	return (double)v0.x * (double)v1.x + (double)v0.y * (double)v1.y + (double)v0.z * (double)v1.z;
}
#endif

void print_vec2(const char* const str, const vec2s vec);
void print_vec3(const char* const str, const vec3s vec);
void print_mat3(const char* const str, const mat3s* const mat);
void get_vectors_from_angles(const vec3s angles, mat3s* vectors);

#endif
