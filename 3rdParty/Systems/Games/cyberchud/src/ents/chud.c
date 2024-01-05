#include "ents/chud.h"
#include "quake.h"
#include "text.h"

#define TOO_CLOSE 4

static void chud_task_none(engine_t* const e, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[chud_task_none] [%d]\n", id);
#endif
	ecs_t* const ecs = &e->ecs;
	chud_t* const chud = &ecs->custom0[id].chud;
	/* is player nearby? */
	const i32 player_id = ecs->player_id;
	const vec3s player_pos = ecs->pos[player_id];
	const float dist = glm_vec3_distance(ecs->pos[id].raw, (float*)player_pos.raw);
	if (dist > TOO_CLOSE) {
		chud->nav_target = player_pos;
		chud->basic.state = MOB_STATE_NAV;
		anim_set(&ecs->anim[id], &e->assets.models_anim[MODELS_ANIM_CHUD].anims[CHUD_ANIM_WALK], NULL);
	}
}

static void chud_task_computer(engine_t* const e, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[chud_task_computer] [%d]\n", id);
#endif
}

static void chud_think_idle(engine_t* const e, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[chud_think_idle] [%d]\n", id);
#endif
	ecs_t* const ecs = &e->ecs;
	chud_t* const chud = &ecs->custom0[id].chud;

	switch (chud->basic.task.chud) {
		case CHUD_TASK_NONE:
			chud_task_none(e, id, delta);
			break;
		case CHUD_TASK_COMPUTER:
			chud_task_computer(e, id, delta);
			break;
		case CHUD_TASK_APARTMENT_GREETER:
			chud_task_computer(e, id, delta);
			break;
	}
}

static void chud_think_nav(engine_t* const e, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[chud_think_nav] [%d]\n", id);
#endif
	ecs_t* const ecs = &e->ecs;
	chud_t* const chud = &ecs->custom0[id].chud;
	edict_t* const ent = ecs->edict[id].edict;

	const i32 player_id = ecs->player_id;
	const vec3s player_pos = ecs->pos[player_id];
	chud->nav_target = player_pos;

	const float dist = glm_vec3_distance(chud->nav_target.raw, ecs->pos[id].raw);

	/* return if close enough */
	if (dist < TOO_CLOSE) {
		chud->basic.state = MOB_STATE_IDLE;
		anim_set(&ecs->anim[id], &e->assets.models_anim[MODELS_ANIM_CHUD].anims[CHUD_ANIM_IDLE], NULL);
		return;
	}

	vec3s dir;
	dir.x =  chud->nav_target.x - ecs->pos[id].x;
	dir.y = 0;
	dir.z = chud->nav_target.z - ecs->pos[id].z;
	/* glm_vec3_sub(ecs->pos[id].raw, chud->nav_target.raw, dir.raw); */
	glm_vec3_normalize(dir.raw);

	/* is player nearby? */
	SV_MoveToGoal(e, ent, dir, dist, delta);
}

void chud_think(void *engine, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[chud_think] [%d]\n", id);
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

	chud_t* const chud = &ecs->custom0[id].chud;
	if (chud->basic.flicker_cooldown > 0) {
		ecs->flags[id].highlight = 1;
	} else {
		ecs->flags[id].highlight = 0;
	}
	chud->basic.flicker_cooldown -= delta;

	/* Turn to player */
	switch (chud->basic.state) {
		case MOB_STATE_IDLE:
			chud_think_idle(e, id, delta);
			break;
		case MOB_STATE_NAV:
			chud_think_nav(e, id, delta);
			break;
		default:
			chud_think_idle(e, id, delta);
	}
}

void chud_hit(void *engine, int32_t id, const int16_t dmg) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
#ifndef NDEBUG
	myprintf("[chud_hit] [%d] hp:%d, dmg:%d\n", id, ecs->hp[id].hp, dmg);
#endif
	ecs->hp[id].hp -= dmg;
	chud_t* const chud = &ecs->custom0[id].chud;
	chud->basic.flicker_cooldown = 0.1f;
}

void chud_init(void *engine, int32_t id) {
#ifndef NDEBUG
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	myprintf("[chud_init] [%d] HP:%d\n", id, ecs->hp[id].hp);
#endif
}

void chud_task_init(engine_t* const e, const int32_t id, const chud_task_e task) {
	ecs_t* const ecs = &e->ecs;
	ecs->custom0[id].chud.basic.task.chud = task;
	switch (task) {
		case CHUD_TASK_APARTMENT_GREETER:
			anim_set(&ecs->anim[id], &e->assets.models_anim[MODELS_ANIM_CHUD].anims[CHUD_ANIM_IDLE], NULL);
			break;
		case CHUD_TASK_COMPUTER:
			anim_set(&ecs->anim[id], &e->assets.models_anim[MODELS_ANIM_CHUD].anims[CHUD_ANIM_COMPUTER], NULL);
			break;
		default:
			break;
	}
}
