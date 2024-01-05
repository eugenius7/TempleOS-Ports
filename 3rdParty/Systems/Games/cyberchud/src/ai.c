#include "ai.h"
#include "engine.h"
#include "text.h"
#include "quake.h"

#define TOO_CLOSE 4

void mutt_think(void *engine, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[mutt_think] [%d]\n", id);
#endif
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;

	if (ecs->hp[id].hp <= 0) {
		for (int i=0; i<32; i++) {
			vec3s pos, vel;
			rand_vec3(&e->seed, vel.raw);
			const float particle_spread = 0.2f;
			glm_vec3_scale(vel.raw, particle_spread, pos.raw);
			const float particle_speed = 5.0f;
			glm_vec3_scale(vel.raw, particle_speed, vel.raw);
			glm_vec3_add(pos.raw, ecs->pos[id].raw, pos.raw);
			new_particle(ecs, &pos, &vel, &e->assets.models_basic[MODELS_STATIC_CUBE], 0.1f);
		}
		free_mob(ecs, id);
		return;
	}
}

void mutt_hit(void *engine, int32_t id, int16_t dmg) {
#ifndef NDEBUG
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	myprintf("[mutt_hit] [%d] HP:%d\n", id, ecs->hp[id].hp);
#endif
}

void mutt_init(void *engine, int32_t id) {
#ifndef NDEBUG
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	myprintf("[mutt_init] [%d] HP:%d\n", id, ecs->hp[id].hp);
#endif
}

void troon_think(void *engine, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[troon_think] [%d]\n", id);
#endif
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;

	if (ecs->hp[id].hp <= 0) {
		for (int i=0; i<32; i++) {
			vec3s pos, vel;
			rand_vec3(&e->seed, vel.raw);
			const float particle_spread = 0.2f;
			glm_vec3_scale(vel.raw, particle_spread, pos.raw);
			const float particle_speed = 5.0f;
			glm_vec3_scale(vel.raw, particle_speed, vel.raw);
			glm_vec3_add(pos.raw, ecs->pos[id].raw, pos.raw);
			new_particle(ecs, &pos, &vel, &e->assets.models_basic[MODELS_STATIC_CUBE], 0.1f);
		}
		free_mob(ecs, id);
		return;
	}

	/* Turn to player */
	const int32_t player_id = ecs->player_id;
	vec3s diff2d, diff3d;
	vec3s pos = ecs->pos[id];
	pos.y += 1.0f;
	vec3s player_pos = ecs->pos[player_id];

	diff3d.x = player_pos.x - pos.x;
	diff3d.y = player_pos.y - pos.y;
	diff3d.z = player_pos.z - pos.z;
	glm_vec3_normalize(diff3d.raw);
	diff2d.x = pos.x - player_pos.x;
	diff2d.y = 0;
	diff2d.z = pos.z - player_pos.z;
	glm_vec3_normalize(diff2d.raw);
	vec3 up = {0,1,0};
	glm_quat_for(diff2d.raw, up, ecs->rot[id].raw);

	troon_t* const troon = &ecs->custom0[id].troon;
	if (troon->basic.fire_cooldown <= 0) {
		const float dist_to_player = glm_vec3_distance(pos.raw, player_pos.raw);
		if (dist_to_player < 256) {
			vec3s front = {{0,0,1}};
			glm_quat_rotatev(ecs->rot[id].raw, front.raw, front.raw);
			const float dot = glm_vec3_dot(front.raw, diff2d.raw);
			if (dot <= -0.95f) {
				trace_t trace = {0};
				bbox_t move;
				move.min = pos;
				move.max = player_pos;
				TraceLine(e, &e->assets.map.qmods[0].hulls[0], &move, &trace, id);
				if (trace.ent && trace.ent->basic.id == player_id) {
					snd_play_sfx(&e->audio, e->assets.snds[SND_TROON_FIRE]);
					troon->basic.fire_cooldown = 1.0f;
					new_rocket(e, &e->assets.models_basic[MODELS_STATIC_FIREBALL], &pos, &diff3d, 32, id, 0.1f, 10);
				}
			}
		}
	}
	troon->basic.fire_cooldown -= delta;
}

void troon_hit(void *engine, int32_t id, const int16_t dmg) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
#ifndef NDEBUG
	myprintf("[troon_hit] [%d] hp:%d, dmg:%d\n", id, ecs->hp[id].hp, dmg);
#endif
	ecs->hp[id].hp -= dmg;
	ecs->flags[id].highlight = 1;
}

void troon_init(void *engine, int32_t id) {
#ifndef NDEBUG
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	myprintf("[troon_init] [%d] HP:%d\n", id, ecs->hp[id].hp);
#endif
}
