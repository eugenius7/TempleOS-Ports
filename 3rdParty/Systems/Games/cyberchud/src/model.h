#ifndef MODEL_H
#define MODEL_H

#include "anim.h"
#include "shader.h"
#include "camera.h"
#include "controls.h"

#define MAX_MATERIAL_CNT 8

#define WEP_ANIM_FIRE 0
#define WEP_ANIM_IDLE 1

#define CHUD_ANIM_COMPUTER 0
#define CHUD_ANIM_IDLE     1
#define CHUD_ANIM_INTERACT 2
#define CHUD_ANIM_SHOOT    3
#define CHUD_ANIM_WALK     4

#define MUTT_ANIM_IDLE 0
#define MUTT_ANIM_WALK 1

typedef struct {
	int32_t bone_idx;
	float weight;
} weight_t;

typedef struct {
	weight_t weights[MAX_BONE_INFLUENCE];
} weights_t;

typedef struct {
	vec3s pos;
	vec3s norm;
} vert_t;

typedef struct {
	vec3s pos;
	vec3s norm;
	vec3s tangent;
} vert_tangent_t;

typedef struct {
	const char header[4];
	const int version;
	const rgb_u8_t norms[];
} bsp_lux_t;

typedef struct {
	uint32_t face_cnt;
	uint32_t vert_cnt;
	i8 mat_type;
	i8 tex_diffuse_cnt;
	const bsp_litinfo_t* litdata;
	const px_t** tex_diffuse;
	const px_t* tex_normal;
	const face_t* faces;
	const vert_tangent_t* verts;
	const vec2s* uv;
	const vec2s* uv_lightmap;
	const weights_t* weights;
} mesh_t;

#define TEX_NAME_LEN 32
typedef struct {
	int32_t type;
	char diffuse[TEX_NAME_LEN];
	char normal[TEX_NAME_LEN];
} parse_material_t;

typedef struct {
	uint32_t mesh_cnt;
	uint32_t material_cnt;
	uint32_t bone_cnt;
	bbox_t bbox;
	mesh_t* meshes;
} model_basic_t;

typedef struct {
	model_basic_t basic;
	const bone_data_t* bones;
	u32 anim_cnt;
	anim_data_t* anims;
} model_anim_data_t;

typedef struct {
	uint8_t* fb;
	float* db;
	const cam_t* cam;
	const light_t* lights;
} draw_uniform_t;

#endif
