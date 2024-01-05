#include <assert.h>

#include "engine.h"
#include "quake.h"
#include "mytime.h"
#include "text.h"
#include "alloc.h"
#include "bitarr.h"
#include "buddy_alloc.h"
#include "utils/minmax.h"
#include "utils/pad.h"
#include "utils/mymalloc.h"
#include "ents/chud.h"
#include "ents/pickup.h"
#include "ents/rocket.h"
#include "ents/trigger.h"

/* randomize malloc data for debugging */
#define STB_DS_IMPLEMENTATION
#include "utils/myds.h"

static const ai_t chud_ai_template = {
	.init=chud_init,
	.think=chud_think,
	.hit=chud_hit,
};

static const ai_t mutt_ai_template = {
	.init=mutt_init,
	.think=mutt_think,
	.hit=mutt_hit,
};

static const ai_t troon_ai_template = {
	.init=troon_init,
	.think=troon_think,
	.hit=troon_hit,
};

int32_t new_model_anim(engine_t* e, const model_anim_data_t* const model_data, const anim_data_t* const anim_data, const vec3s pos) {
#ifdef VERBOSE
	myprintf("[model_anim_init] pos: %fx%fx%f\n", pos.x, pos.y, pos.z);
#endif
	ecs_t* const ecs = &e->ecs;
	const i32 id = new_entity(ecs);
	ecs->flags[id].bones = 1;
	ecs->flags[id].model_mtx = 1;
	ecs->flags[id].tangent = 1;
	ecs->pos[id] = pos;
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->rot[id] = glms_quat_identity();
	ecs->scale[id] = (vec3s){{1,1,1}};
	ecs->model[id].model_anim = model_data;
	ecs->anim[id].frame = 0;
	ecs->anim[id].time = 0;
	ecs->anim[id].length = anim_data->length;
	ecs->anim[id].data = anim_data;
	ecs->anim[id].next = NULL;
	ecs->anim[id].bone_mtx_sz = model_data->basic.bone_cnt;
	ecs->anim[id].bone_mtx = balloc(&ecs->alloc, sizeof(mat4s)*model_data->basic.bone_cnt);
	return id;
}

int32_t new_mob(engine_t* e, const model_anim_data_t* const model_data, const anim_data_t* const anim_data, const vec3s pos) {
#ifdef VERBOSE
	myprintf("[mob] pos: %fx%fx%f\n", pos.x, pos.y, pos.z);
#endif
	ecs_t* const ecs = &e->ecs;
	const i32 id = new_model_anim(e, model_data, anim_data, pos);
	ecs->flags[id].gravity = 1;
	ecs->flags[id].mob = 1;
	ecs->flags[id].tangent = 1;
	ecs->bbox[id] = (bbox_t){(vec3s){.x=-16.0f/BSP_RESIZE_DIV, .y=0, .z=-16.0f/BSP_RESIZE_DIV}, (vec3s){.x=16.0f/BSP_RESIZE_DIV, .y=1.6, .z=16.0f/BSP_RESIZE_DIV}};
	ecs->rot[id] = glms_quat_identity();
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->hp[id].hp = 100;
	ecs->hp[id].max = 100;

	edict_t* const edict = new_edict(ecs, id);
	edict->solid = SOLID_SLIDEBOX;
	edict->movetype = MOVETYPE_STEP;
	return id;
}

int32_t new_chud(engine_t* e, const vec3s pos, const float yaw, const u8 task) {
#ifndef NDEBUG
	myprintf("[new_chud] pos: %fx%fx%f\n", pos.x, pos.y, pos.z);
#endif
	const i32 id = new_mob(e, &e->assets.models_anim[MODELS_ANIM_CHUD], &e->assets.models_anim[MODELS_ANIM_CHUD].anims[CHUD_ANIM_IDLE], pos);
	ecs_t* const ecs = &e->ecs;
	ecs->ai[id] = chud_ai_template;
	vec3s up = {{0,1,0}};
	vec3s dir;
	dir.y = 0;
	sincosf(yaw, &dir.z, &dir.x);
	glm_quat_for(dir.raw, up.raw, ecs->rot[id].raw);
	chud_task_init(e, id, task);
	return id;
}

int32_t new_mutt(engine_t* e, const vec3s pos, const float yaw, const u8 task) {
#ifndef NDEBUG
	myprintf("[new_mutt] pos: %fx%fx%f\n", pos.x, pos.y, pos.z);
#endif
	const i32 id = new_mob(e, &e->assets.models_anim[MODELS_ANIM_MUTT], &e->assets.models_anim[MODELS_ANIM_MUTT].anims[0], pos);
	ecs_t* const ecs = &e->ecs;
	ecs->ai[id] = mutt_ai_template;
	vec3s up = {{0,1,0}};
	vec3s dir;
	dir.y = 0;
	sincosf(yaw, &dir.z, &dir.x);
	glm_quat_for(dir.raw, up.raw, ecs->rot[id].raw);
	return id;
}

int32_t new_troon(engine_t* e, const vec3s pos, const float yaw, const u8 task) {
	const i32 id = new_mob(e, &e->assets.models_anim[MODELS_ANIM_TROON], &e->assets.models_anim[MODELS_ANIM_TROON].anims[0], pos);
#ifndef NDEBUG
	myprintf("[new_troon] [%d] pos: %fx%fx%f\n", id, pos.x, pos.y, pos.z);
#endif
	ecs_t* const ecs = &e->ecs;
	ecs->ai[id] = troon_ai_template;
	vec3s up = {{0,1,0}};
	vec3s dir;
	dir.y = 0;
	sincosf(yaw, &dir.z, &dir.x);
	glm_quat_for(dir.raw, up.raw, ecs->rot[id].raw);
	return id;
}

int32_t new_rocket(engine_t* e, const model_basic_t* const model_data, const vec3s* pos, const vec3s* dir, const float speed, const int32_t parent, const float brightness, const int16_t dmg) {
#ifdef VERBOSE
	myprintf("[new_rocket] pos: %fx%fx%f\n", pos->x, pos->y, pos->z);
#endif
	ecs_t* const ecs = &e->ecs;
	const i32 id = new_model_static(ecs, model_data, pos);
	bitarr_set(ecs->bit_simplelight0, id);
	ecs->brightness[id] = brightness;

	glm_vec3_scale((float*)dir->raw, speed, ecs->vel[id].raw);
	glm_vec3_inv_to((float*)dir->raw, ecs->rot[id].raw);
	vec3 up = {0,1,0};
	glm_quat_for(ecs->rot[id].raw, up, ecs->rot[id].raw);
	ecs->flags[id].gravity = 1;
	ecs->flags[id].tangent = 1;
	ecs->bbox[id] = (bbox_t){(vec3s){.x=-1.0f/BSP_RESIZE_DIV, .y=0, .z=-1.0f/BSP_RESIZE_DIV}, (vec3s){.x=1.0f/BSP_RESIZE_DIV, .y=1.0f/BSP_RESIZE_DIV, .z=1.0f/BSP_RESIZE_DIV}};
	ecs->parent[id] = parent;
	ecs->custom0[id].projectile.dmg = dmg;

	edict_t* const edict = new_edict(ecs, id);
	edict->solid = SOLID_BBOX;
	edict->movetype = MOVETYPE_FLYMISSILE;
	edict->owner = parent;
	edict->touch = touch_rocket;
	return id;
}

#if 0
static void calculate_tanget(vert_tangent_t* verts, vec2s* uv) {
	vec3s edge[2];
	glm_vec3_sub(verts[1].pos.raw, verts[0].pos.raw, edge[0].raw);
	glm_vec3_sub(verts[2].pos.raw, verts[0].pos.raw, edge[1].raw);
	float f = 1.0f / (uv[0].x*uv[1].y - uv[1].x*uv[0].y);
	verts[0].norm.x = edge[0].y*edge[1].z - edge[0].z*edge[1].y;
	verts[0].norm.y = edge[0].z*edge[1].x - edge[0].x*edge[1].z;
	verts[0].norm.z = edge[0].x*edge[1].y - edge[0].y*edge[1].x;
	verts[1].norm = verts[0].norm;
	verts[2].norm = verts[0].norm;

	vec2s deltaUV[2];
	glm_vec2_sub(uv[1].raw, uv[0].raw, deltaUV[0].raw);
	glm_vec2_sub(uv[2].raw, uv[0].raw, deltaUV[1].raw);

	verts[0].tangent.x = f * (deltaUV[1].y * edge[0].x - deltaUV[0].y * edge[1].x);
	verts[0].tangent.y = f * (deltaUV[1].y * edge[0].y - deltaUV[0].y * edge[1].y);
	verts[0].tangent.z = f * (deltaUV[1].y * edge[0].z - deltaUV[0].y * edge[1].z);
	verts[1].tangent = verts[0].tangent;
	verts[2].tangent = verts[0].tangent;
}

int32_t new_beam(engine_t* const e, const vec3s* const pos, const vec3s* dir, const float dist) {
#ifdef VERBOSE
	myprintf("[new_beam]\n");
#endif
	ecs_t* const ecs = &e->ecs;

	/* Allocate Model */
	const u32 face_cnt = 4;
	const u32 vert_cnt = face_cnt*3;
	size_t sz = 0;
	sz += pad_inc_count(sizeof(model_basic_t), 8);
	sz += pad_inc_count(sizeof(mesh_t), 8);
	sz += pad_inc_count(sizeof(px_t*), 8);
	sz += pad_inc_count(face_cnt*sizeof(face_t), 8);
	sz += pad_inc_count(vert_cnt*sizeof(vert_tangent_t), 8);
	sz += pad_inc_count(vert_cnt*sizeof(vec2s), 8);

	/* Allocate Light Positions */
	beam_t beam;
	beam.light_cnt = dist/1;
	if (beam.light_cnt < 2) {
		beam.light_cnt = 2;
	} else if (beam.light_cnt > 4) {
		beam.light_cnt = 4;
	}
	sz += pad_inc_count(beam.light_cnt*sizeof(vec3s), 8);

	char* ptr = balloc(&e->ecs.alloc, sz);
	if (ptr == NULL) {
		myprintf("[ERR] [new_beam] balloc failed size:%zu\n", sz);
		return -1;
	}

	model_basic_t* const model = pad_inc_ptr(&ptr, sizeof(model_basic_t), 8);
	memset(model, 0, sizeof(model_basic_t));
	model->meshes = pad_inc_ptr(&ptr, sizeof(mesh_t), 8);
	model->meshes[0] = (mesh_t){0};
	model->meshes[0].tex_diffuse = pad_inc_ptr(&ptr, sizeof(px_t*), 8);
	model->meshes[0].tex_diffuse[0] = NULL;
	face_t* const faces = pad_inc_ptr(&ptr, face_cnt*sizeof(face_t), 8);
	vert_tangent_t* const verts = pad_inc_ptr(&ptr, vert_cnt*sizeof(vert_tangent_t), 8);
	vec2s* const uv = pad_inc_ptr(&ptr, vert_cnt*sizeof(vec2s), 8);
	beam.lights = (void*)ptr;

	model->mesh_cnt = 1;
	mesh_t* const mesh = &model->meshes[0];
	mesh->face_cnt = face_cnt;
	mesh->vert_cnt = vert_cnt;

#define SPREAD 0.20f

	/* Left Side */
	verts[0].pos = (vec3s){{0,0,0}};
	uv[0] = (vec2s){{1,0}};

	verts[1].pos = verts[0].pos;
	verts[1].pos.x -= SPREAD;
	verts[1].pos.y += SPREAD;
	verts[1].pos.z -= dist;
	uv[1] = (vec2s){{1,1}};

	verts[2].pos = verts[0].pos;
	verts[2].pos.x -= SPREAD;
	verts[2].pos.y -= SPREAD;
	verts[2].pos.z -= dist;
	uv[2] = (vec2s){{0,1}};

	calculate_tanget(verts, uv);
	faces[0].p[0] = 0;
	faces[0].p[1] = 1;
	faces[0].p[2] = 2;

	/* Right Side */
	verts[3].pos = verts[0].pos;
	uv[3] = (vec2s){{0,0}};

	verts[4].pos = verts[0].pos;
	verts[4].pos.x += SPREAD;
	verts[4].pos.y -= SPREAD;
	verts[4].pos.z -= dist;
	uv[4] = (vec2s){{0,1}};

	verts[5].pos = verts[0].pos;
	verts[5].pos.x += SPREAD;
	verts[5].pos.y += SPREAD;
	verts[5].pos.z -= dist;
	uv[5] = (vec2s){{1,1}};

	calculate_tanget(&verts[3], uv);
	faces[1].p[0] = 3;
	faces[1].p[1] = 4;
	faces[1].p[2] = 5;

	/* Top Side */
	verts[6].pos = verts[0].pos;
	uv[6] = (vec2s){{0.5,0}};

	verts[7].pos = verts[0].pos;
	verts[7].pos.x += SPREAD;
	verts[7].pos.y += SPREAD;
	verts[7].pos.z -= dist;
	uv[7] = (vec2s){{1,1}};

	verts[8].pos = verts[0].pos;
	verts[8].pos.x -= SPREAD;
	verts[8].pos.y += SPREAD;
	verts[8].pos.z -= dist;
	uv[8] = (vec2s){{0,1}};

	calculate_tanget(&verts[6], uv);
	faces[2].p[0] = 6;
	faces[2].p[1] = 7;
	faces[2].p[2] = 8;

	/* Bottom Side */
	verts[9].pos = verts[0].pos;
	uv[9] = (vec2s){{0.5,0}};

	verts[10].pos = verts[0].pos;
	verts[10].pos.x -= SPREAD;
	verts[10].pos.y -= SPREAD;
	verts[10].pos.z -= dist;
	uv[10] = (vec2s){{0,1}};

	verts[11].pos = verts[0].pos;
	verts[11].pos.x += SPREAD;
	verts[11].pos.y -= SPREAD;
	verts[11].pos.z -= dist;
	uv[11] = (vec2s){{1,1}};

	calculate_tanget(&verts[9], uv);
	faces[3].p[0] = 9;
	faces[3].p[1] = 10;
	faces[3].p[2] = 11;

	mesh->faces = faces;
	mesh->verts = verts;
	mesh->uv = uv;
	mesh->mat_type = 3;

	const i32 id = new_model_static(ecs, model, pos);
#ifdef VERBOSE
	myprintf("[new_beam] %d\n", id);
#endif
	bitarr_set(ecs->bit_simplelight0, id);
	bitarr_set(ecs->bit_simplelight0_arr, id);
	ecs->brightness[id] = 0.1f;

	/* ecs->bbox[id] = (bbox_t){(vec3s){{-0.25f,0,-8}}, (vec3s){{0.25f,0.25f,0}}}; */
	ecs->flags[id].skip_cull = 1;
	ecs->cooldown0[id] = .2;
	ecs->flags[id].no_shadow = 1;
	ecs->flags[id].decay = 1;

	vec3 up = {0,1,0};
	glm_quat_for((float*)dir->raw, up, ecs->rot[id].raw);

	/* Add Lights */
	vec3s end_pos;
	end_pos.x = pos->x + dir->x * dist;
	end_pos.y = pos->y + dir->y * dist;
	end_pos.z = pos->z + dir->z * dist;
	beam.lights[0] = *pos;
	for (int i=1; i<beam.light_cnt; i++)
		glm_vec3_lerp((float*)pos->raw, end_pos.raw, ((float)i)/(beam.light_cnt-1), beam.lights[i].raw);

	ecs->custom0[id].beam = beam;

	return id;
}
#else
int32_t new_beam(engine_t* const e, const vec3s* const pos, const vec3s* dir, const float dist) {
#ifdef VERBOSE
	myprintf("[new_beam2]\n");
#endif
	ecs_t* const ecs = &e->ecs;

	/* Allocate Model */
	const model_basic_t* const model_template = &e->assets.models_basic[MODELS_STATIC_BEAM];
	size_t sz = 0;

	/* Allocate Light Positions */
	beam_t beam;
	beam.light_cnt = dist/1;
	if (beam.light_cnt < 2) {
		beam.light_cnt = 2;
	} else if (beam.light_cnt > 4) {
		beam.light_cnt = 4;
	}
	sz += pad_inc_count(beam.light_cnt*sizeof(vec3s), 8);

	beam.lights = balloc(&e->ecs.alloc, sz);
	if (beam.lights == NULL) {
		myprintf("[ERR] [new_beam] balloc failed size:%zu\n", sz);
		return -1;
	}

	const i32 id = new_model_static(ecs, model_template, pos);
#ifdef VERBOSE
	myprintf("[new_beam] %d\n", id);
#endif
	bitarr_set(ecs->bit_simplelight0, id);
	bitarr_set(ecs->bit_simplelight0_arr, id);
	ecs->brightness[id] = 0.1f;

	ecs->flags[id].skip_cull = 1;
	ecs->cooldown0[id] = .2;
	ecs->flags[id].no_shadow = 1;
	ecs->flags[id].decay = 1;
	ecs->flags[id].static_model_osc = 1;
	ecs->shader[id].warp.length = dist;

	vec3 up = {0,1,0};
	vec3 inv_dir = {-dir->x, -dir->y, -dir->z};
	glm_quat_for(inv_dir, up, ecs->rot[id].raw);

	/* Add Lights */
	vec3s end_pos;
	end_pos.x = pos->x + dir->x * dist;
	end_pos.y = pos->y + dir->y * dist;
	end_pos.z = pos->z + dir->z * dist;
	beam.lights[0] = *pos;
	for (int i=1; i<beam.light_cnt; i++)
		glm_vec3_lerp((float*)pos->raw, end_pos.raw, ((float)i)/(beam.light_cnt-1), beam.lights[i].raw);

	ecs->custom0[id].beam = beam;

	return id;
}
#endif

static edict_t* new_bsp_trigger_template(engine_t *e, bsp_entity_t *ent) {
	ecs_t* const ecs = &e->ecs;
	const i32 id = new_entity(ecs);
	ent->ecs_id = id;
#ifndef NDEBUG
	myprintf("[new_bsp_trigger_template] %d\n", id);
#endif
	ecs->pos[id] = ent->origin;

	const bsp_dmodel_t* const dmod = &e->assets.map.dmodels[ent->model];
	ecs->pos[id] = dmod->origin;
	ecs->rot[id] = glms_quat_identity();
	ecs->bbox[id] = dmod->bbox;

	edict_t* const edict = new_edict(&e->ecs, id);
	const bsp_qmodel_t* const qmod = &e->assets.map.qmods[ent->model];
	for (int i=0; i<MAX_MAP_HULLS; i++) {
		edict->hulls[i] = &qmod[ent->model].hulls[i];
	}

	edict->absbox = dmod->bbox;
	edict->solid = SOLID_TRIGGER;

	SV_LinkEdict(e, edict, false);
	return edict;
}

int32_t new_bsp_trigger_multiple(engine_t* const e, bsp_entity_t* const ent) {
	edict_t* const edict = new_bsp_trigger_template(e, ent);
#ifndef NDEBUG
	myprintf("[new_bsp_trigger_multiple] %d\n", edict->basic.id);
#endif

	edict->touch = touch_trigger_once;
	return edict->basic.id;
}

int32_t new_bsp_trigger_once(engine_t* const e, bsp_entity_t* const ent) {
	edict_t* const edict = new_bsp_trigger_template(e, ent);
#ifndef NDEBUG
	myprintf("[new_bsp_trigger_once] [%d] msg:%s\n", edict->basic.id, ent->message);
#endif
	if (!strcmp(ent->message, "neverhappen")) {
		edict->touch = touch_trigger_neverhappen;
	} else if (!strcmp(ent->message, "glitchout")) {
		edict->touch = touch_trigger_glitchout;
	} else if (!strcmp(ent->message, "noise")) {
		edict->touch = touch_trigger_noise;
	} else if (!strcmp(ent->message, "wireframe")) {
		edict->touch = touch_trigger_wireframe;
	} else {
		edict->touch = touch_trigger_once;
	}
	return edict->basic.id;
}

int32_t new_bsp_trigger_talk(engine_t* const e, bsp_entity_t* const ent) {
	edict_t* const edict = new_bsp_trigger_template(e, ent);
	const i32 id = edict->basic.id;
#ifndef NDEBUG
	myprintf("[new_bsp_trigger_talk] [%d] msg:%s\n", id, ent->message);
#endif

	ecs_t* const ecs = &e->ecs;
	trigger_talk_t* const trigger_talk = &ecs->custom0[id].trigger_talk;
	trigger_talk->diag_idx = dialogue_by_name(ent->message);
	edict->touch = touch_talk;

	return id;
}

int32_t new_pickup(engine_t* const e, vec3s pos, const u8 type) {
	ecs_t* const ecs = &e->ecs;
	const model_anim_data_t* model;
	const anim_data_t* anim_data;
	switch (type) {
		case PICKUP_BEAM:
			model = &e->assets.models_anim[MODELS_ANIM_CAPSULEGUN];
			anim_data = &model->anims[WEP_ANIM_IDLE];
			break;
		case PICKUP_SHOTGUN:
			model = &e->assets.models_anim[MODELS_ANIM_SHOTGUN];
			anim_data = &model->anims[WEP_ANIM_IDLE];
			break;
		case PICKUP_SMG:
			model = &e->assets.models_anim[MODELS_ANIM_SMG];
			anim_data = &model->anims[WEP_ANIM_IDLE];
			break;
	}

	const i32 id = new_model_anim(e, model, anim_data, pos);
#ifndef NDEBUG
	myprintf("[new_pickup] [%d] %.1fx%.1fx%.1f\n", id, pos.x, pos.y, pos.z);
#endif
	ecs->pos[id] = pos;

	ecs->pos[id] = pos;
	ecs->rot[id] = glms_quat_identity();
	ecs->scale[id] = (vec3s){{3.0,3.0,3.0}};
	ecs->bbox[id].min.x = -0.5f;
	ecs->bbox[id].min.y = -0.5f;
	ecs->bbox[id].min.z = -0.5f;
	ecs->bbox[id].max.x = 0.5f;
	ecs->bbox[id].max.y = 0.5f;
	ecs->bbox[id].max.z = 0.5f;

	edict_t* const edict = new_edict(&e->ecs, id);
	glm_vec3_add(ecs->pos[id].raw, ecs->bbox[id].min.raw, edict->absbox.min.raw);
	glm_vec3_add(ecs->pos[id].raw, ecs->bbox[id].max.raw, edict->absbox.max.raw);
	edict->solid = SOLID_TRIGGER;
	ecs->ai[id].think = pickup_think;

	switch (type) {
		case PICKUP_BEAM:
			edict->touch = pickup_touch_beam;
			break;
		case PICKUP_SHOTGUN:
			edict->touch = pickup_touch_shotgun;
			break;
		case PICKUP_SMG:
			edict->touch = pickup_touch_smg;
			break;
	}

	SV_LinkEdict(e, edict, false);
	return edict->basic.id;
}

int32_t new_bsp_trigger_levelchange(engine_t* const e, bsp_entity_t* const ent) {
	edict_t* const edict = new_bsp_trigger_template(e, ent);
#ifndef NDEBUG
	myprintf("[new_bsp_trigger_levelchange] %d\n", edict->basic.id);
#endif

	edict->touch = touch_trigger_levelchange;
	return edict->basic.id;
}

engine_threads_t* engine_alloc(const i8 cpu_cnt) {
	size_t alloc_sz = 0;
	alloc_sz += pad_inc_count(sizeof(engine_threads_t), 64);
	alloc_sz += pad_inc_count(cpu_cnt*sizeof(thread_data_t), 64);
	alloc_sz += pad_inc_count(cpu_cnt*sizeof(ui_graph_t), 64);
	alloc_sz += pad_inc_count(VFX_FB_W*VFX_FB_H+4, 64);
	for (i8 i=0; i<cpu_cnt; i++)
		alloc_sz += pad_inc_count(cpu_cnt*sizeof(thread_vert_out_t), 64);

	char* ptr = mycalloc(alloc_sz, 1);
	if (ptr == NULL)
		return NULL;

	engine_threads_t* const et = pad_inc_ptr(&ptr, sizeof(engine_threads_t), 64);
	engine_t* const e = &et->e;
	e->cpu_cnt = cpu_cnt;

	et->jobs = pad_inc_ptr(&ptr, cpu_cnt*sizeof(thread_data_t), 64);
	e->graphs = pad_inc_ptr(&ptr, cpu_cnt*sizeof(ui_graph_t), 64);
	e->vfx_fb = pad_inc_ptr(&ptr, VFX_FB_W*VFX_FB_H+4, 64);
	e->vfx_fb->w = VFX_FB_W;
	e->vfx_fb->h = VFX_FB_H;

	for (i8 i=0; i<cpu_cnt; i++) {
		et->jobs[i].thread_id = i;
		et->jobs[i].frag_in_data = pad_inc_ptr(&ptr, cpu_cnt*sizeof(thread_vert_out_t), 64);
		et->jobs[i].e = e;
		arrsetcap(et->jobs[i].vert_job_data, 1024);
	}

	/* Setup Allocators */
	alloc_init(&e->ecs.alloc, 16, 1024*1024*32);
	alloc_init(&e->ecs.qcvm.edict_mem, 8, sizeof(edict_t)*1024*10);

	/* Linear Buffers */
	arrsetcap(e->lines, 1024);
	arrsetcap(e->idxs_culled, 1024);
	arrsetcap(e->idxs_bones, 1024);

	return et;
}

void engine_free(engine_threads_t* const et) {
	/* TODO clean up */
	engine_t* const e = &et->e;

	DestroyMutex(et->jobs[0].mutex);
	DestroyCond(et->jobs[0].mu_cond);
	DestroyCond(et->jobs[0].done_mu_cond);

	assets_free(&e->assets);

	ecs_free(&e->ecs);
}

void engine_cfg_update(engine_t* const e) {
	const engine_flags_t flags = e->flags;
	if (!flags.pointlight_shadows_enabled && !flags.shadowcaster_shadows_enabled) {
		e->shader_cfg_idx = SHADER_CFG_NO_PLSC;
	} else if (!flags.pointlight_shadows_enabled) {
		e->shader_cfg_idx = SHADER_CFG_NO_PL;
	} else if (!flags.shadowcaster_shadows_enabled) {
		e->shader_cfg_idx = SHADER_CFG_NO_SC;
	} else {
		e->shader_cfg_idx = SHADER_CFG_DEFAULT;
	}
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++)
		e->pointlights.cubemaps[i].clean = 0;
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++)
		e->shadowcasters.cubemaps[i].basic.clean = 0;
#ifndef NDEBUG
	myprintf("[engine_cfg_update] shader_cfg_idx:%d\n", e->shader_cfg_idx);
#endif
}
