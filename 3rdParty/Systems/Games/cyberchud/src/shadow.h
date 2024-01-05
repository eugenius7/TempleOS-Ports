#ifndef SHADOW_H
#define SHADOW_H

#include "vec.h"
#include "bitarr.h"

/* TODO terrible hack for visflags */
#define MAX_MESH_CNT 64
#define CUBE_FACE_CNT 6
#define SHADOW_WIDTH 480
#define SHADOW_HEIGHT 480
#define SHADOW_TOTAL (SHADOW_WIDTH*SHADOW_HEIGHT)

typedef struct {
	BITARR_TYPE* mesh[MAX_MESH_CNT];
} visflags_t;

typedef struct {
	mat4s proj;
	mat4s cube_mtx[CUBE_FACE_CNT];
	vec3s pos;
	float brightness;
	float near_plane;
	float far_plane;
	int clean;
	visflags_t* visflags;
	float depthbuffer[CUBE_FACE_CNT][SHADOW_WIDTH*SHADOW_HEIGHT];
	float cachebuffer[CUBE_FACE_CNT][SHADOW_WIDTH*SHADOW_HEIGHT];
} cubemap_t;

typedef struct {
	cubemap_t basic;
	BITARR_TYPE shadowfield[CUBE_FACE_CNT][((SHADOW_WIDTH*SHADOW_HEIGHT+BITARR_SIZE-1)>>BITARR_SHIFT)*sizeof(BITARR_TYPE)];
} cubemap_occlusion_t;

typedef struct {
	vec4_3 pts; // viewspace tri
	mat3s world_pos;
	vec2s pts2[3]; // post perspective divide
} frag_shadow_t;

typedef struct {
	rect_i16* masks;
	frag_shadow_t* frags;
	vec4_3* view_pos;
} shadow_shader_bundle_t;

typedef struct {
	shadow_shader_bundle_t tris;
	uint32_t tri_cnt[6];
} shadow_job_t;

void cubemap_update(cubemap_t* const shadow, const vec3s pos, const float brightness, visflags_t* const visflags);
void cubemap_init(cubemap_t* shadow);
void cubemap_clear(cubemap_t* cubemap);
void cubemap_occlusion_clear(cubemap_occlusion_t* cubemap);
void draw_shadowmap(const shadow_job_t* const job, cubemap_t* cubemaps, const i16 mask_y1, const i16 mask_y2);
void draw_shadowmap_occlusion(const shadow_job_t* const job, cubemap_occlusion_t* cubemaps, const i16 mask_y1, const i16 mask_y2);

#endif
