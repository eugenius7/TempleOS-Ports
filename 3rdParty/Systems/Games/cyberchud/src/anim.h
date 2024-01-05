#ifndef ANIM_H
#define ANIM_H

#include "vec.h"

#define MAX_BONE 64
#define MAX_BONE_CHILDREN 16
#define MAX_BONE_INFLUENCE 4

typedef struct {
	float time;
	vec3s pos;
} anim_pos_t;

typedef struct {
	float time;
	vec3s scale;
} anim_scale_t;

typedef struct {
	float *time;
	versors *rot;
} anim_rot_t;

typedef struct {
	uint32_t ch_pos_cnt;
	uint32_t ch_rot_cnt;
	uint32_t ch_scale_cnt;
	anim_pos_t* ch_pos;
	anim_rot_t ch_rot;
	anim_scale_t* ch_scale;
} anim_ch_data_t;

typedef struct {
	float length;
	anim_ch_data_t channels[MAX_BONE];
} anim_data_t;

typedef struct {
	int8_t children[MAX_BONE_CHILDREN];
	mat4s transform_mtx;
	mat4s inv_matrix;
} bone_data_t;

typedef struct {
	uint32_t frame;
	float time;
	float length;
	uint32_t bone_mtx_sz;
	const anim_data_t* data;
	const anim_data_t* next;
	mat4s* bone_mtx;
} anim_t;

void bone_update(anim_t *anim, const uint32_t bone_cnt, const float delta);
void anim_set(anim_t* const anim, const anim_data_t* const anim_data, const anim_data_t* const next);

#endif
