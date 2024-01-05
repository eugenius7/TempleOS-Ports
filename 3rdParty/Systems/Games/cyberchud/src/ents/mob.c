#include "ents/mob.h"
#include "engine.h"
#include "text.h"

void touch_chud(void *data, int32_t id, int32_t other) {
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	myprintf("[touch_chud] %d %lx\n", id, e);
	if (ecs->flags[other].mob) {
		ecs->hp[other].hp -= 1;
	}
}

void touch_troon(void *data, int32_t id, int32_t other) {
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	myprintf("[touch_troon] %d %lx\n", id, e);
	if (ecs->flags[other].mob) {
		ecs->hp[other].hp -= 1;
	}
}

void hit_mob(void *data, int32_t id, int32_t other, int16_t dmg) {
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	myprintf("[hit_mob] %d %lx\n", id, e);
	if (ecs->flags[other].mob) {
		ecs->hp[other].hp -= 1;
	}
}
