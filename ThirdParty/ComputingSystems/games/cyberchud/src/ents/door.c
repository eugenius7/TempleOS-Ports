#include "ents/door.h"
#include "engine.h"
#include "text.h"

void door_touch(void *data, int32_t id, int32_t other) {
#ifdef VERBOSE
	myprintf("[touch_door] %d %d\n", id, other);
#endif
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	door_t* const door = &ecs->custom0[id].door;

	if (door->moving == 0 && other == ecs->player_id) {
		snd_play_sfx(&e->audio, e->assets.snds[SND_DOOR_OPEN]);
		door->moving = 1;
		vec3s origin, target;
		if (door->at_target) {
			origin = ecs->pos_target[id];
			target = ecs->pos_home[id];
		} else {
			origin = ecs->pos_home[id];
			target = ecs->pos_target[id];
		}
		vec3s dir = glms_vec3_sub(target, origin);
		const float len = glm_vec3_norm(dir.raw);
		door->ltime = 0;
		door->time_end = len / door->speed;
		const float t = 1.0f/door->time_end;
		dir.x *= t;
		dir.y *= t;
		dir.z *= t;
		ecs->vel[id] = dir;
	}
}

void door_think(void *data, int32_t id, const float delta) {
#ifdef VERBOSE
	myprintf("[door_think] %d\n", id);
#endif
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	door_t* const door = &ecs->custom0[id].door;
	if (door->moving) {
		if (door->ltime >= door->time_end) {
			door->moving = 0;
			door->at_target = !door->at_target;
			door->cooldown = 3.0f;
			ecs->vel[id] = (vec3s){{0,0,0}};
		}
	} else if (door->at_target) {
		if (door->cooldown <= 0.0f) {
			door->moving = 1;
			vec3s origin, target;
			origin = ecs->pos_target[id];
			target = ecs->pos_home[id];
			vec3s dir = glms_vec3_sub(target, origin);
			const float len = glm_vec3_norm(dir.raw);
			door->ltime = 0;
			door->time_end = len / door->speed;
			const float t = 1.0f/door->time_end;
			dir.x *= t;
			dir.y *= t;
			dir.z *= t;
			ecs->vel[id] = dir;
		}
		door->cooldown -= delta;
	}
}

void door_hit(void *data, int32_t id, int16_t dmg) {
#ifdef VERBOSE
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	myprintf("[door_hit] [%d] hp:%d, dmg:%d\n", id, ecs->hp[id].hp, dmg);
#endif
}
