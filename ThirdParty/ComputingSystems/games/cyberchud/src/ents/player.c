#include <assert.h>

#include "ents/player.h"
#include "quake.h"
#include "text.h"
#include "utils/myds.h"

static const MODELS_ANIM_IDX weapon_model_lookup[WEAPON_TOTAL] = {
	MODELS_ANIM_SHOTGUN,
	MODELS_ANIM_SHOTGUN,
	MODELS_ANIM_CAPSULEGUN,
	MODELS_ANIM_SMG,
};

static const float weapon_cooldown_lookup[WEAPON_TOTAL] = {
	1.0f,
	1.0f,
	1.0f/10,
	1.0f/60,
};

void player_switch_weapon(engine_t* const e, const int weapon) {
#ifndef NDEBUG
	myprintf("[switch_weapon] weapon:%u\n", weapon);
#endif
	ecs_t* const ecs = &e->ecs;
	assert(weapon >= 0);
	assert(weapon < WEAPON_TOTAL);
	if (weapon != ecs->player.cur_weapon) {
		flag_t* const flags = &ecs->flags[ecs->pov_model_id];
		if (weapon > 0) {
			const u8 mask = 1<<(weapon-1);
			if (ecs->player.avail_weapons&mask) {
				snd_play_sfx(&e->audio, e->assets.snds[SND_INV_SWITCH]);
				ecs->player.cur_weapon = weapon;
				flags->skip_draw = 0;
				const MODELS_ANIM_IDX midx = weapon_model_lookup[weapon];
				model_anim_data_t* const model = &e->assets.models_anim[midx];
				model_anim_change_model(ecs, ecs->pov_model_id, model, &model->anims[1]);
			} else {
				snd_play_sfx(&e->audio, e->assets.snds[SND_INV_UNAVAIL]);
			}
		} else {
			ecs->player.cur_weapon = 0;
			flags->skip_draw = 1;
		}
	}
}

static void fire_smg(engine_t* const e, const i32 player_id) {
	/* TODO sound */
	snd_play_sfx(&e->audio, e->assets.snds[SND_SMG_FIRE]);

	/* TODO shooting origin */
	vec3s pos = e->ecs.pos[player_id];
	versors quat = e->ecs.rot[player_id];
	vec3s dir = {{0,0,-1}};
	glm_quat_rotatev(quat.raw, dir.raw, dir.raw);
	vec3s offset = {{0.35,0.3,0}};
	glm_quat_rotatev(quat.raw, offset.raw, offset.raw);
	pos.x += dir.x * 1 + offset.x;
	pos.y += dir.y * 1 + offset.y;
	pos.z += dir.z * 1 + offset.z;
	new_rocket(e, &e->assets.models_basic[MODELS_STATIC_TRACER], &pos, &dir, 32, player_id, 0.1f, 1);

	/* Recoil */
	e->cam.pitch += 0.0025f;

	bbox_t move;
	move.min = e->cam.pos;
	move.max = glms_vec3_scale(e->cam.front, 10.0);
	move.max = glms_vec3_add(move.max, move.min);
	trace_t trace = {0};
	TraceLine(e, &e->assets.map.qmods[0].hulls[0], &move, &trace, player_id);
#ifdef VERBOSE
	print_vec3("[player_fire] trace endpos", trace.endpos);
	print_vec3("[player_fire] trace plane_norm", trace.plane.normal);
	myprintf("[player_fire] trace: frac:%.2f plane_dist:%.2f allsolid:%d contents:%d startsolid:%d open:%d\n", trace.fraction, trace.plane.dist, trace.allsolid, trace.contents, trace.startsolid, trace.inopen);
#endif
}

static void fire_shotgun(engine_t* const e, const i32 player_id) {
	/* TODO sound */
	snd_play_sfx(&e->audio, e->assets.snds[SND_SHOTGUN_FIRE]);

	/* TODO shooting origin */
	vec3s pos = e->ecs.pos[player_id];
	versors quat = e->ecs.rot[player_id];
	vec3s dir = {{0,0,-1}};
	glm_quat_rotatev(quat.raw, dir.raw, dir.raw);
	vec3s offset = {{0.4,0,0}};
	glm_quat_rotatev(quat.raw, offset.raw, offset.raw);
	pos.x += dir.x * 1 + offset.x;
	pos.y += dir.y * 1 + offset.y;
	pos.z += dir.z * 1 + offset.z;

	/* generate pellets */
	for (int i=0; i<32; i++) {
		vec3s new_dir;
		rand_vec3(&e->seed, new_dir.raw);
		glm_vec3_scale(new_dir.raw, 0.2f, new_dir.raw);
		glm_vec3_add(dir.raw, new_dir.raw, new_dir.raw);
		glm_vec3_normalize(new_dir.raw);
		new_rocket(e, &e->assets.models_basic[MODELS_STATIC_SHOTGUN_PELLET], &pos, &new_dir, 64, player_id, 0.2f, 15);
	}

	/* Recoil */
	e->cam.pitch += 0.25f;

	/* Warp Circle */
	e->warpcircle.x = e->cam.pos.x;
	e->warpcircle.y = e->cam.pos.y;
	e->warpcircle.z = e->cam.pos.z;
	e->warpcircle.w = 0;

#if 1
	bbox_t move;
	move.min = e->cam.pos;
	move.max = glms_vec3_scale(e->cam.front, 10.0);
	move.max = glms_vec3_add(move.max, move.min);
	trace_t trace = {0};
	TraceLine(e, &e->assets.map.qmods[0].hulls[0], &move, &trace, player_id);
#ifdef VERBOSE
	print_vec3("[player_fire] trace endpos", trace.endpos);
	print_vec3("[player_fire] trace plane_norm", trace.plane.normal);
	myprintf("[player_fire] trace: frac:%.2f plane_dist:%.2f allsolid:%d contents:%d startsolid:%d open:%d\n", trace.fraction, trace.plane.dist, trace.allsolid, trace.contents, trace.startsolid, trace.inopen);
#endif
#endif
}

static void fire_beam(engine_t* const e, const i32 player_id) {
	ecs_t* const ecs = &e->ecs;
	snd_play_sfx(&e->audio, e->assets.snds[SND_LASER_FIRE]);
#define CAPSULE_RANGE 25600.0f
	bbox_t move;
	move.min = ecs->pos[player_id];
	vec3s weapon_offset = {{0.10,0.4,-0.20}};
	glm_quat_rotatev(ecs->rot[player_id].raw, weapon_offset.raw, weapon_offset.raw);
	glm_vec3_add(move.min.raw, weapon_offset.raw, move.min.raw);
	move.max = glms_vec3_scale(e->cam.front, CAPSULE_RANGE);
	move.max = glms_vec3_add(move.max, move.min);
	trace_t trace = {0};
	TraceLine(e, &e->assets.map.qmods[0].hulls[0], &move, &trace, player_id);
	float frac = 1.0f;
	if (trace.contents != CONTENTS_EMPTY) {
		frac = trace.fraction;
		if (trace.ent) {
			const i32 id = trace.ent->basic.id;
			if (ecs->ai[id].hit)
				ecs->ai[id].hit(e, id, 10);
		}
	}
	const float dist = CAPSULE_RANGE*frac;

	new_beam(e, &move.min, &e->cam.front, dist);

	/* Recoil */
	e->cam.pitch += 0.01f;

	e->vfx_flags.noise = 1;
	e->noise_time = 0;
}

void player_fire(engine_t* const e) {
	ecs_t* const ecs = &e->ecs;
	const u32 player_id = ecs->player_id;
#ifdef VERBOSE
	myprintf("[player_fire] player_id:%d\n", player_id);
#endif

	if (ecs->player.fire_cooldown > 0)
		return;

	/* Set Fire Animation */
	const i8 cur_weapon = ecs->player.cur_weapon;
	ecs->player.fire_cooldown = weapon_cooldown_lookup[cur_weapon];
	if (cur_weapon >= 0) {
		const MODELS_ANIM_IDX midx = weapon_model_lookup[cur_weapon];
		anim_set(&ecs->anim[e->ecs.pov_model_id], &e->assets.models_anim[midx].anims[0], &e->assets.models_anim[midx].anims[1]);
	}

	switch (ecs->player.cur_weapon) {
		case WEAPON_NONE:
			break;
		case WEAPON_SHOTGUN:
			fire_shotgun(e, player_id);
			break;
		case WEAPON_BEAM:
			fire_beam(e, player_id);
			break;
		case WEAPON_SMG:
			fire_smg(e, player_id);
			break;
	}
}

void player_hit(void *engine, int32_t id, const int16_t dmg) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
#ifndef NDEBUG
	myprintf("[player_hit] [%d] hp:%d, dmg:%d\n", id, ecs->hp[id].hp, dmg);
#endif
	ecs->hp[id].hp -= dmg;
	e->vfx_flags.wireframe = 1;
	e->vfx_flags.camrock = 1;
	e->wireframe_time = 0;
	e->camrock_time = 0;
}
