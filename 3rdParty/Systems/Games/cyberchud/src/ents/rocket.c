#include "ents/rocket.h"
#include "engine.h"
#include "text.h"

void touch_rocket(void *data, int32_t id, int32_t other) {
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
#ifdef VERBOSE
	myprintf("[touch_rocket] id:%d other:%d\n", id, other);
#endif

	if (ecs->parent[id] == other || ecs->parent[id] == ecs->parent[other])
		return;

	if (other == ecs->player_id || ecs->flags[other].mob) {
		ecs->ai[other].hit(e, other, ecs->custom0[id].projectile.dmg);
	}
	for (int i=0; i<32; i++) {
		vec3s pos, vel;
		rand_vec3(&e->seed, vel.raw);
		const float particle_spread = 0.2f;
		glm_vec3_scale(vel.raw, particle_spread, pos.raw);
		const float particle_speed = 2.0f;
		glm_vec3_scale(vel.raw, particle_speed, vel.raw);
		glm_vec3_add(pos.raw, ecs->pos[id].raw, pos.raw);
		new_particle(ecs, &pos, &vel, &e->assets.models_basic[MODELS_STATIC_CUBE], 0.025f);
	}
	free_rocket(ecs, id);
}
