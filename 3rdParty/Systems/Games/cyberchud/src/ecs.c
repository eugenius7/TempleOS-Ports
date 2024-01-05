#include <assert.h>

#include "ecs.h"
#include "quake.h"
#include "text.h"
#include "utils/minmax.h"
#include "utils/myds.h"
#include "utils/mymalloc.h"
#include "utils/pad.h"
#include "ents/door.h"
#include "ents/button.h"
#include "ents/dynlight.h"
#include "ents/spawner.h"

#define INITIAL_ECS_CAP 4096

static const ai_t door_ai_template = {
	.init=NULL,
	.think=door_think,
	.hit=door_hit,
	.activate=NULL,
};

static const ai_t spawner_ai_template = {
	.init=NULL,
	.think=NULL,
	.hit=NULL,
	.activate=spawner_activate,
};

static const ai_t dynlight_ai_template = {
	.init=NULL,
	.think=think_dynlight,
	.hit=NULL,
	.activate=NULL,
};

static inline edict_light_t* new_edict_light(ecs_t *ecs, const int32_t id) {
	edict_light_t *edict = balloc(&ecs->qcvm.edict_mem, sizeof(edict_light_t));
	edict->basic.area.prev = NULL;
	edict->basic.area.next = NULL;
	edict->basic.id = id;
	edict->pos = ecs->pos[id];
	ecs->target[id] = -1;
	ecs->edict[id].light = edict;
	ecs->qcvm.edict_light_cnt++;
	return edict;
}

static inline void free_edict(ecs_t* ecs, const int32_t id) {
	bitarr_clear(ecs->bit_edict, id);
	SV_UnlinkEdict(ecs->edict[id].edict);
	bfree(&ecs->qcvm.edict_mem, ecs->edict[id].edict);
	ecs->edict[id].edict = NULL;
	ecs->qcvm.edict_cnt--;
}

static void ecs_setlen(ecs_t* ecs, const size_t sz) {
#ifdef VERBOSE
	myprintf("[ecs_setlen] %zu to %zu\n", arrlenu(ecs->flags), sz);
#endif
	arrsetlen(ecs->type, sz);
	arrsetlen(ecs->flags, sz);
	arrsetlen(ecs->ai, sz);
	arrsetlen(ecs->parent, sz);
	arrsetlen(ecs->target, sz);
	arrsetlen(ecs->custom0, sz);
	arrsetlen(ecs->shader, sz);
	arrsetlen(ecs->brightness, sz);
	arrsetlen(ecs->cooldown0, sz);
	arrsetlen(ecs->hp, sz);
	arrsetlen(ecs->pos, sz);
	arrsetlen(ecs->pos_home, sz);
	arrsetlen(ecs->pos_target, sz);
	arrsetlen(ecs->vel, sz);
	arrsetlen(ecs->scale, sz);
	arrsetlen(ecs->rot, sz);
	arrsetlen(ecs->bbox, sz);
	arrsetlen(ecs->mtx, sz);
	arrsetlen(ecs->model, sz);
	arrsetlen(ecs->anim, sz);
	arrsetlen(ecs->edict, sz);
}

static void ecs_resize_bm(BITARR_TYPE** const bm, const size_t bm_cur_size, const size_t bm_new_size) {
	arrsetlen(*bm, bm_new_size);
	memset(&(*bm)[bm_cur_size], 0, (bm_new_size-bm_cur_size)*sizeof(BITARR_TYPE));
}

static void ecs_resize(ecs_t* ecs, const size_t sz) {
	const size_t curSize = arrlenu(ecs->flags);
#ifdef VERBOSE
	myprintf("[ecs_resize] %zu to %zu\n", curSize, sz);
#endif
	assert(sz > curSize);
	ecs_setlen(ecs, sz);
	for (size_t i=curSize; i<sz; i++) {
		ecs->type[i] = TYPE_NONE;
		ecs->flags[i] = (flag_t){0};
		ecs->parent[i] = -1;
		ecs->hp[i].hp = -1;
		ecs->hp[i].max = -1;
		ecs->anim[i].bone_mtx = NULL;
	}
	const size_t bm_cur_size = bitarr_get_pos(curSize);
	const size_t bm_new_size = bitarr_get_pos(sz);
	ecs_resize_bm(&ecs->used_bm, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_edict, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_light, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_light_dynamic, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_particle, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_particle_emitter, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_spawner, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_simplelight0, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_simplelight0_arr, bm_cur_size, bm_new_size);
	ecs_resize_bm(&ecs->bit_simplelight1, bm_cur_size, bm_new_size);
}

edict_t* new_edict(ecs_t *ecs, const int32_t id) {
	bitarr_set(ecs->bit_edict, id);
	edict_t *edict = balloc(&ecs->qcvm.edict_mem, sizeof(edict_t));
	*edict = (edict_t){0};
	edict->basic.id = id;
	edict->basic.num_leafs = 0;
	edict->owner = ecs->parent[id];
	ecs->target[id] = -1;
	ecs->edict[id].edict = edict;
	ecs->qcvm.edict_cnt++;
	ecs->ai[id] = (ai_t){0};
	return edict;
}

void free_entity(ecs_t* const ecs, const int32_t id) {
	ecs->flags[id] = (flag_t){0};
	bitarr_clear(ecs->used_bm, id);
	if (id < ecs->low_id)
		ecs->low_id = id;
	else
		ecs->low_id = -1;
}

void free_bsp_trigger(ecs_t* const ecs, const int32_t id) {
	free_edict(ecs, id);
	free_entity(ecs, id);
}

int32_t new_entity(ecs_t* const ecs) {
#ifdef VERBOSE
	fputs("[new_entity]\n", stdout);
#endif

	const int32_t curLen = myarrlenu(ecs->flags);

	/* find a slot */
	for (int32_t i=MAX(ecs->low_id, 0); i<curLen; i++) {
		if (!bitarr_get(ecs->used_bm, i)) {
			bitarr_set(ecs->used_bm, i);
			ecs->flags[i] = (flag_t){0};
			ecs->parent[i] = -1;
			ecs->low_id = -1;

			/* find next low ID since we already iterated this far */
			for (int32_t j=i+1; j<curLen; j++) {
				if (!bitarr_get(ecs->used_bm, j)) {
					ecs->low_id = j;
					break;
				}
			}

			/* return ID */
			return i;
		}
	}

	/* no free slot, resize */
#define DATA_INC_LEN 1024
	ecs_resize(ecs, curLen+DATA_INC_LEN);
	ecs->low_id = curLen+1;
	return curLen;
}

int32_t new_path_corner(ecs_t* ecs, bsp_entity_t* const ent) {
	const i32 id = new_entity(ecs);
	ent->ecs_id = id;
#ifndef NDEBUG
	myprintf("[new_path_corner] [%d] target:%s targetname:%s\n", id, ent->target, ent->targetname);
#endif
	ecs->pos[id] = ent->origin;
	return id;
}

edict_light_t* new_light(ecs_t* ecs, bsp_entity_t* const ent) {
	const i32 id = new_entity(ecs);
	ent->ecs_id = id;
#ifndef NDEBUG
	myprintf("[new_light] [%d] pos:%fx%fx%f flags:%u\n", id, ent->origin.x, ent->origin.y, ent->origin.z, ent->flags);
#endif
	bitarr_set(ecs->bit_light, id);
	ecs->pos[id] = ent->origin;
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->rot[id] = glms_quat_identity();
	ecs->brightness[id] = (float)ent->light/300.0f;
	dynamiclight_t* const light = &ecs->custom0[id].light;
	if (ent->flags&SPAWNFLAGS_LIGHT_NOSHADOWCASTER) {
		light->no_shadowcaster = 1;
	} else {
		light->no_shadowcaster = 0;
	}

	edict_light_t* const edict = new_edict_light(ecs, id);
	return edict;
}

edict_light_t* new_light_dynamic(ecs_t* ecs, const vec3s pos, const float brightness, const float speed, const float range, const DYN_LIGHT_STYLE style) {
	const i32 id = new_entity(ecs);
#ifndef NDEBUG
	myprintf("[new_light_dynamic] %d]\n", id);
#endif
	bitarr_set(ecs->bit_light_dynamic, id);

	ecs->pos[id] = pos;
	ecs->pos_home[id] = pos;
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->rot[id] = glms_quat_identity();
	ecs->brightness[id] = brightness;
	dynamiclight_t* const light = &ecs->custom0[id].light;
	light->style = style;
	light->direction = 0;
	light->ltime = 0;
	light->speed = speed;
	light->range = range;
	ecs->ai[id] = dynlight_ai_template;

	edict_light_t* const edict = new_edict_light(ecs, id);
	return edict;
}

void ecs_free(ecs_t* const ecs) {
	arrfree(ecs->flags);
	arrfree(ecs->type);
	arrfree(ecs->ai);
	arrfree(ecs->parent);
	arrfree(ecs->target);
	arrfree(ecs->custom0);
	arrfree(ecs->brightness);
	arrfree(ecs->cooldown0);
	arrfree(ecs->hp);
	arrfree(ecs->pos);
	arrfree(ecs->pos_home);
	arrfree(ecs->pos_target);
	arrfree(ecs->vel);
	arrfree(ecs->rot);
	arrfree(ecs->bbox);
	arrfree(ecs->mtx);
	arrfree(ecs->model);

	/* TODO we could just free the buddy_alloc instead */
	for (size_t i=0; i<myarrlenu(ecs->anim); i++) {
		if (ecs->anim[i].bone_mtx)
			bfree(&ecs->alloc, ecs->anim[i].bone_mtx);
	}
	arrfree(ecs->anim);
	arrfree(ecs->edict);
}

void free_model_anim(ecs_t* const ecs, const i32 id) {
	free_entity(ecs, id);
	bfree(&ecs->alloc, ecs->anim[id].bone_mtx);
	ecs->anim[id].bone_mtx = NULL;
}

#if 0
void free_beam(ecs_t* const ecs, const i32 id) {
	bitarr_clear(ecs->bit_simplelight0, id);
	bitarr_clear(ecs->bit_simplelight0_arr, id);
	bfree(&ecs->alloc, (void*)ecs->model[id].model_static->meshes);
	bfree(&ecs->alloc, (void*)ecs->model[id].model_static);
	ecs->model[id].model_static = NULL;
	free_entity(ecs, id);
}
#else
void free_beam(ecs_t* const ecs, const i32 id) {
	bitarr_clear(ecs->bit_simplelight0, id);
	bitarr_clear(ecs->bit_simplelight0_arr, id);
	ecs->model[id].model_static = NULL;
	free_entity(ecs, id);
}
#endif

void ecs_reset(ecs_t* const ecs) {
	for (int32_t i=0; i<myarrlen(ecs->flags); i++) {
		const flag_t flags = ecs->flags[i];
		if (bitarr_get(ecs->bit_edict, i)) {
			free_edict(ecs, i);
		}
		if (flags.bones) {
			bfree(&ecs->alloc, ecs->anim[i].bone_mtx);
			ecs->anim[i].bone_mtx = NULL;
		}
	}
	ecs_setlen(ecs, 0);
	ecs->low_id = 0;
	ecs_resize(ecs, INITIAL_ECS_CAP);
	assert(ecs->qcvm.edict_cnt == 0);
	arrsetlen(ecs->used_bm, 0);
}

/* QCVM doesn't copy/realloc, it just recreates it */
void qcvm_resize(qcvm_t* const qcvm) {
	if (qcvm->edict_cnt <= qcvm->buf_cap)
		return;

	i32 new_size = qcvm->buf_cap*2;
	while (new_size < qcvm->edict_cnt) {
		new_size *= 2;
	}
	qcvm->buf_cap = new_size;

	myfree(qcvm->moved_edict_buf);

	size_t byte_size = 0;
	byte_size += pad_inc_count(new_size*sizeof(edict_t*), 64);
	byte_size += pad_inc_count(new_size*sizeof(vec3s), 64);
	char* ptr = mymalloc(byte_size);
	qcvm->moved_edict_buf = pad_inc_ptr(&ptr, new_size*sizeof(edict_t*), 64);
	qcvm->moved_from_buf = (vec3s*)ptr;
}

static void qcvm_init(qcvm_t* const qcvm) {
#define QCVM_INIT_CAP 1024
	qcvm->buf_cap = QCVM_INIT_CAP;
	size_t byte_size = 0;
	byte_size += pad_inc_count(qcvm->buf_cap*sizeof(edict_t*), 64);
	byte_size += pad_inc_count(qcvm->buf_cap*sizeof(vec3s), 64);
	char* ptr = mymalloc(byte_size);
	qcvm->moved_edict_buf = pad_inc_ptr(&ptr, qcvm->buf_cap*sizeof(edict_t*), 64);
	qcvm->moved_from_buf = (vec3s*)ptr;
}

void ecs_init(ecs_t* const ecs) {
	/* Init ECS Capacity */
	ecs_resize(ecs, INITIAL_ECS_CAP);
	qcvm_init(&ecs->qcvm);
}

void free_particle(ecs_t* const ecs, const int32_t id) {
	bitarr_clear(ecs->bit_simplelight0, id);
	free_entity(ecs, id);
	bitarr_clear(ecs->bit_particle, id);
}

int32_t new_particle(ecs_t *ecs, const vec3s* pos, const vec3s* vel, const model_basic_t* const model, const float brightness) {
	const i32 id = new_model_static(ecs, model, pos);
	if (brightness > 0) {
		bitarr_set(ecs->bit_simplelight0, id);
		ecs->brightness[id] = brightness;
	}
	ecs->vel[id] = *vel;
	bitarr_set(ecs->bit_particle, id);
	ecs->cooldown0[id] = 1;
	return id;
}

int32_t new_particle_emitter(ecs_t *ecs, const vec3s* pos, const model_basic_t* const model, const float vel, const float spawn_speed) {
#ifndef NDEBUG
	myprintf("[new_particle_emitter]\n");
#endif
	const i32 id = new_entity(ecs);
	ecs->pos[id] = *pos;
	bitarr_set(ecs->bit_particle_emitter, id);
	ecs->custom0[id].particle_emitter.model = model;
	ecs->custom0[id].particle_emitter.vel = vel;
	ecs->custom0[id].particle_emitter.spawn_speed = spawn_speed;
	ecs->custom0[id].particle_emitter.cooldown = spawn_speed;
	return id;
}

int32_t new_spawner(ecs_t *ecs, const vec3s* pos, const float spawn_speed, const int spawn_type, const bool autospawner) {
	const i32 id = new_entity(ecs);
#ifndef NDEBUG
	myprintf("[new_spawner] [%d] type:%d speed:%f autspawn:%d\n", id, spawn_type, spawn_speed, autospawner);
#endif
	bitarr_set(ecs->bit_spawner, id);
	ecs->pos[id] = *pos;
	ecs->ai[id] = spawner_ai_template;
	spawner_t* const spawner = &ecs->custom0[id].spawner;
	spawner->autospawn = autospawner;
	spawner->spawn_type = spawn_type;
	spawner->spawn_speed = spawn_speed;
	spawner->cooldown = spawn_speed;
	return id;
}

void free_mob(ecs_t* const ecs, const i32 id) {
#ifndef NDEBUG
	myprintf("[free_mob] %d\n", id);
#endif
	free_model_anim(ecs, id);
	free_edict(ecs, id);
}

void free_rocket(ecs_t* const ecs, const int32_t id) {
	bitarr_clear(ecs->bit_simplelight0, id);
	free_edict(ecs, id);
	free_entity(ecs, id);
}

int32_t new_player(ecs_t* ecs, const vec3s pos, float angle) {
	const i32 id = new_entity(ecs);
#ifndef NDEBUG
	myprintf("[new_player] %d pos:%fx%fx%f angle:%f\n", id, pos.x, pos.y, pos.z, angle);
#endif
	ecs->player.cur_weapon = 0;
	ecs->player.avail_weapons = 0;

	ecs->flags[id].gravity = 1;
	ecs->pos[id] = pos;
	glm_quat_identity(ecs->rot[id].raw);
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->bbox[id] = (bbox_t){(vec3s){.x=-16.0f/BSP_RESIZE_DIV, .y=-1.2, .z=-16.0f/BSP_RESIZE_DIV}, (vec3s){.x=16.0f/BSP_RESIZE_DIV, .y=0.5, .z=16.0f/BSP_RESIZE_DIV}};

	edict_t* const edict = new_edict(ecs, id);
	edict->solid = SOLID_SLIDEBOX;
	edict->movetype = MOVETYPE_WALK;
	ecs->ai[id].hit = player_hit;
	ecs->hp[id].hp = 100;
	ecs->hp[id].max = 100;
	return id;
}

static int32_t new_bsp(ecs_t* ecs, bsp_t *bsp, const u32 qmod_idx) {
	const i32 id = new_entity(ecs);
#ifndef NDEBUG
	myprintf("[new_bsp] %d\n", id);
#endif
	ecs->flags[id].model_mtx = 1;
	ecs->flags[id].tangent = 1;
	ecs->pos[id] = bsp->dmodels[qmod_idx].origin;
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->rot[id] = glms_quat_identity();
	const bsp_qmodel_t* const qmod = &bsp->qmods[qmod_idx];
	ecs->bbox[id] = qmod->bbox;
	edict_t* const edict = new_edict(ecs, id);
	edict->absbox = qmod->bbox;
	edict->solid = SOLID_BSP;
	for (int i=0; i<MAX_MAP_HULLS; i++) {
		ecs->edict[id].edict->hulls[i] = &qmod->hulls[i];
	}

	ecs->model[id].model_static = &qmod->model;
	ecs->scale[id] = (vec3s){{1,1,1}};
	return id;
}

int32_t new_bsp_worldmodel(ecs_t* const ecs, bsp_t* const bsp, const u32 qmod_idx) {
	const i32 id = new_bsp(ecs, bsp, qmod_idx);
#ifndef NDEBUG
	myprintf("[new_bsp_worldmodel] %d\n", id);
#endif
	ecs->flags[id].worldmap = 1;
	ecs->flags[id].skip_cull = 1; // TODO for large stuff like levels
	return id;
}

static edict_t* new_bsp_door_template(ecs_t* const ecs, bsp_t* const bsp, bsp_entity_t* const ent) {
	const i32 id = new_bsp(ecs, bsp, ent->model);
#ifndef NDEBUG
	myprintf("[new_bsp_door_template] %d\n", id);
#endif

	ecs->flags[id].bsp_model = 1;
	ecs->flags[id].skip_cull = 1; // TODO for large stuff like levels
	ecs->pos_home[id] = ent->origin;
	edict_t* const edict = ecs->edict[id].edict;
	/* edict->solid = SOLID_BSP; */
	edict->movetype = MOVETYPE_PUSH;
	ecs->ai[id] = door_ai_template;
	door_t* const door = &ecs->custom0[id].door;
	door->speed = 2.0f;
	if (ent->angle == -2) {
		ecs->pos_target[id] = (vec3s){{0,-1,0}};
	} else if (ent->angle == -1) {
		ecs->pos_target[id] = (vec3s){{0,3,0}};
	} else {
		const float yawRadians = (float)ent->angle * M_PI / 180.0f;
		const float cosYaw = cosf(yawRadians);
		const float sinYaw = sinf(yawRadians);
		ecs->pos_target[id].x = cosYaw;
		ecs->pos_target[id].y = 0;
		ecs->pos_target[id].z = sinYaw;
	}

	return edict;
}

edict_t* new_bsp_door(ecs_t* const ecs, bsp_t* const bsp, bsp_entity_t* const ent) {
	edict_t* const edict = new_bsp_door_template(ecs, bsp, ent);
	ent->ecs_id = edict->basic.id;
#ifndef NDEBUG
	myprintf("[new_bsp_door] %d\n", edict->basic.id);
#endif
	edict->touch = door_touch;
	ecs->ai[edict->basic.id].hit = button_hit;
	return edict;
}

edict_t* new_bsp_button(ecs_t* const ecs, bsp_t* const bsp, bsp_entity_t* const ent) {
	const i32 id = new_bsp(ecs, bsp, ent->model);
	ecs->flags[id].bsp_model = 1;
	ecs->flags[id].skip_cull = 1; // TODO for large stuff like levels
	ent->ecs_id = id;
#ifndef NDEBUG
	myprintf("[new_bsp_button] %d\n", id);
#endif
	edict_t* const edict = ecs->edict[id].edict;
	edict->click = button_click;
	return edict;
}

int32_t new_model_static(ecs_t* ecs, const model_basic_t* const model_data, const vec3s* pos) {
#ifdef VERBOSE
	myprintf("[new_model_static] pos: %fx%fx%f\n", pos->x, pos->y, pos->z);
#endif
	const i32 id = new_entity(ecs);
	ecs->flags[id].model_mtx = 1;
	ecs->flags[id].tangent = 1;
	ecs->pos[id] = *pos;
	ecs->vel[id] = (vec3s){.x=0,.y=0,.z=0};
	ecs->rot[id] = glms_quat_identity();
	ecs->scale[id] = (vec3s){{1,1,1}};
	ecs->model[id].model_static = model_data;
	return id;
}

void model_anim_change_model(ecs_t* const ecs, const i32 id, model_anim_data_t* const model_data, const anim_data_t* const anim_data) {
	ecs->anim[id].frame = 0;
	ecs->anim[id].time = 0;
	ecs->anim[id].length = anim_data->length;
	ecs->anim[id].data = anim_data;
	ecs->anim[id].next = NULL;
	if (model_data->basic.bone_cnt > ecs->anim[id].bone_mtx_sz) {
		ecs->anim[id].bone_mtx_sz = model_data->basic.bone_cnt;
		bfree(&ecs->alloc, ecs->anim[id].bone_mtx);
		ecs->anim[id].bone_mtx = balloc(&ecs->alloc, sizeof(mat4s)*model_data->basic.bone_cnt);
	}
	ecs->model[id].model_anim = model_data;
}
