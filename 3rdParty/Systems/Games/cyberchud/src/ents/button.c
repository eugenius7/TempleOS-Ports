#include "ents/button.h"
#include "text.h"
#include "engine.h"

void button_click(void *data, int32_t id, int32_t other) {
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	const i32 target = ecs->target[id];
#ifndef NDEBUG
	myprintf("[button_click] [%d] other:%d target:%d\n", id, other, target);
#endif
	if (target >= 0) {
		if (ecs->ai[target].activate) {
			snd_play_sfx(&e->audio, e->assets.snds[SND_BEEP]);
			ecs->ai[target].activate(e, target, other);
		}
	}
}

void button_touch(void *data, int32_t id, int32_t other) {
#ifndef NDEBUG
	myprintf("[button_click]\n");
#endif
}

void button_hit(void *data, int32_t id, int16_t dmg) {
#ifndef NDEBUG
	myprintf("[button_hit]\n");
#endif
}
