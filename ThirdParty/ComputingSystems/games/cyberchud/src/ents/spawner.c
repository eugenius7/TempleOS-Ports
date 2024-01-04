#include "ents/spawner.h"
#include "utils/myds.h"
#include "text.h"

void spawners_update(engine_t *e, const float delta) {
	ecs_t* const ecs = &e->ecs;
	const i32 elen = myarrlen(ecs->flags);
	for (i32 i=0; i<elen; i++) {
		if (!bitarr_get(ecs->bit_spawner, i))
			continue;

		spawner_t* const spawner = &ecs->custom0[i].spawner;
		if (spawner->autospawn && spawner->cooldown <= 0) {
			spawner->cooldown = spawner->spawn_speed;
			vec3s pos, vel;
			rand_vec3(&e->seed, vel.raw);
			const float particle_spread = 0.2f;
			glm_vec3_scale(vel.raw, particle_spread, pos.raw);
			const float particle_speed = 2.0f;
			glm_vec3_scale(vel.raw, particle_speed, vel.raw);
			glm_vec3_add(pos.raw, ecs->pos[i].raw, pos.raw);
			switch (spawner->spawn_type) {
				case MOB_CHUD:
					new_chud(e, pos, 0, 0);
					break;
				case MOB_MUTT:
					new_mutt(e, pos, 0, 0);
					break;
				case MOB_TROON:
					new_troon(e, pos, 0, 0);
					break;
			}
		}
		spawner->cooldown -= delta;
	}
}

void spawner_activate(void *engine, int32_t id, int32_t other) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	spawner_t* const spawner = &ecs->custom0[id].spawner;
	vec3s pos = ecs->pos[id];
#ifndef NDEBUG
	myprintf("[spawner_activate] [%d] other:%d type:%d pos:%fx%fx%f\n", id, other, spawner->spawn_type, pos.x, pos.y, pos.z);
#endif
	switch (spawner->spawn_type) {
		case MOB_CHUD:
			new_chud(e, pos, 0, 0);
			break;
		case MOB_MUTT:
			new_mutt(e, pos, 0, 0);
			break;
		case MOB_TROON:
			new_troon(e, pos, 0, 0);
			break;
	}
}
