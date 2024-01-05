#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <map>
#include <vector>
#include <stdio.h>

#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <assimp/vector3.h>

#include "flags.h"

#define MAX_BONE_CHILDREN 16
#define MAX_BONE 64
#define MAX_BONE_INFLUENCE 4
#define MAX_MESH_CNT 32
#define MAX_MATERIAL_CNT 8

typedef struct {
	int8_t children[MAX_BONE_CHILDREN];
	aiMatrix4x4 transform_mtx;
	aiMatrix4x4 inverse_mtx;
} bone_data_t;

typedef struct {
	const char* name;
	bone_data_t data;
} bone_t;

typedef struct {
	float x;
	float y;
} vec2;

typedef struct {
	float x;
	float y;
	float z;
} vec3;

typedef struct {
	float x;
	float y;
	float z;
	float w;
} quat_t;

typedef struct {
	vec3 p[3];
} tri_t;

typedef struct {
	vec2 p[3];
} uv_t;

typedef struct {
	int32_t bone_idx;
	float weight;
} weight_t;

typedef struct {
	weight_t weights[MAX_BONE_INFLUENCE];
} weights_t;

typedef struct {
	uint32_t p[3];
} face_t;

typedef struct {
	vec3 pos;
	vec3 norm;
} vert_t;

typedef struct {
	vec3 pos;
	vec3 norm;
	vec3 tangent;
} vert_tangents_t;

typedef struct {
	vec3 min;
	vec3 max;
} aabb_t;

typedef struct {
	uint32_t face_cnt;
	uint32_t vert_cnt;
	uint32_t material_idx;
	face_t* faces;
	vert_t* verts;
	vert_tangents_t* verts_extra;
	vec2* uv;
	weights_t* weights;
} prim_t;

typedef struct {
	float time;
	aiVector3D pos;
} anim_pos_t;

typedef struct {
	float time;
	aiVector3D scale;
} anim_scale_t;

typedef struct {
	float *time;
	quat_t *rot;
} anim_rot_t;

typedef struct {
	uint32_t ch_pos_cnt;
	uint32_t ch_rot_cnt;
	uint32_t ch_scale_cnt;
	anim_pos_t* ch_pos;
	anim_rot_t ch_rot;
	anim_scale_t* ch_scale;
} anim_data_t;

typedef struct {
	const char* name;
	float length;
	anim_data_t channels[MAX_BONE];
} anim_t;

#define TEX_NAME_LEN 32
typedef struct {
	int32_t type;
	char diffuse[TEX_NAME_LEN];
	char normal[TEX_NAME_LEN];
} material_t;

typedef struct {
	uint32_t prim_cnt;
	uint32_t material_cnt;
	uint32_t anim_cnt;
	aabb_t bbox;
	prim_t *prims;
	std::vector<bone_t> bones;
	anim_t *animations;
	material_t *materials;
} model_t;

typedef struct {
	uint32_t model_cnt;
	uint32_t material_cnt;
	model_t* models;
} scene_t;

typedef struct {
	double x;
	double y;
} tmp_uv_t;

static inline void print_matrix(const aiMatrix4x4 mat) {
	for (int y=0; y<4; y++) {
		printf("  ");
		for (int x=0; x<4; x++) {
			printf("%f ", mat[x][y]);
		}
		printf("\n");
	}
}

#endif
