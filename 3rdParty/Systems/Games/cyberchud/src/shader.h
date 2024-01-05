#ifndef SHADER_H
#define SHADER_H

#include "camera.h"
#include "shadow.h"
#include "px.h"
#include "xorshift.h"

#define MAX_DYNAMIC_LIGHTS 2

#define SHADER_CFG_DEFAULT 0
#define SHADER_CFG_NO_PL   1
#define SHADER_CFG_NO_SC   2
#define SHADER_CFG_NO_PLSC 3

#define SHADER_WORLD_DEFAULT 0
#define SHADER_WORLD_EMITTER 1
#define SHADER_WORLD_NOISE   2

typedef struct {
	cubemap_t cubemap;
	float brightness;
} light_t;

typedef struct {
	vec4 pos[6][3];
} geo_shadow_out_t;

typedef struct {
	vec2s uv[3];
} uv_t;

typedef struct {
	uint32_t p[3];
} face_t;

typedef struct {
	mat3s mtx[MAX_DYNAMIC_LIGHTS];
} lightmtx_t;

typedef struct {
	u8* fb;
	float* db;
	xorshift_t seed;
	i8 interlace;
	i8 shader_cfg_idx;
	i8 thread_cnt;
	i8 pointlight_cnt;
	i8 shadowcaster_cnt;
	i8 simplelight_cnt;
	i16 mask_y1;
	i16 mask_y2;
	vec3s viewpos;
	float scene_time;
	const cubemap_t* pointlights;
	const cubemap_occlusion_t* shadowcasters;
	const vec4s* simplelights;
	const px_t* noise;
	const px_t* noise_nmap;
} frag_uniform_t;

typedef struct {
	/* Varying */
	vec4_3 vert_out; // view-projection verts
	mat3s world_pos; // view-space verts
	uv_t uv;
	uv_t uv_light;
	mat3s norm;
	mat3s tpos;
	mat3s tviewpos;
	lightmtx_t tlightpos;
	lightmtx_t tshadowpos;

	vec4s pts[3]; // viewspace tri
	vec2s pts2[3]; // post perspective divide
	const px_t* tex_diffuse;
	const px_t* tex_normal;
	bsp_litinfo_t litdata;
	float ambient;
	float color_mod;
	i8 shader_idx;
} shader_t;

typedef struct {
	rect_i16* masks;
	vec4_3* view_pos; // view-space verts
	shader_t* frags;
} shader_bundle_t;

typedef struct {
	shader_bundle_t wfrags;
	shader_bundle_t mfrags;
} thread_vert_shader_t;

typedef struct {
	thread_vert_shader_t tris;
	i8 pointlight_cnt;
	i8 shadow_cnt;
	shadow_job_t pointlight_world[MAX_DYNAMIC_LIGHTS];
	shadow_job_t pointlight_model[MAX_DYNAMIC_LIGHTS];
	shadow_job_t shadow_world[MAX_DYNAMIC_LIGHTS];
	shadow_job_t shadow_model[MAX_DYNAMIC_LIGHTS];
} thread_vert_out_t;

typedef struct {
	vec4s clip_plane;
	const cam_t* cam;
	const px_t* tex_diffuse;
	const px_t* tex_normal;
	bsp_litinfo_t litdata;
	i8 shader_idx;
	i8 pointlight_cnt;
	i8 shadowcaster_cnt;
	float ambient;
	float color_mod;
} clip_uniform_t;

typedef struct {
	const mat4s* proj;
	float near_plane;
} shadow_clip_uniform_t;

typedef struct {
	vec4s pos;
	vec3s norm;
	vec2s uv;
} vert_pre_data_t;
typedef struct {
	vert_pre_data_t p[3];
} vert_pre_t;

typedef struct {
	int pal;
	float albedo;
} diffuse_t;

#endif
