#include <SDL2/SDL.h>
#include "scene/playfield.h"
#include "quake.h"

void playfield_init(engine_t* e) {
	e->controls.fly_speed = 1.0f;
	e->controls.show_menu = 0;
	e->controls.show_wireframe = 0;
	e->warpcircle.w = -1;

#ifndef __EMSCRIPTEN__
	if (SDL_SetRelativeMouseMode(SDL_TRUE) == -1) {
		fputs("[INIT] SDL_SetRelativeMouseMode unsupported\n", stderr);
	}
	e->controls.mouse_grab = 1;
#endif

	e->ui_mode = UI_MODE_NONE;

	const int result = assets_load_level(&e->assets, &e->ecs.alloc, 0);
	ecs_reset(&e->ecs);
	SV_ClearWorld(e);

#if 0
	for (int i=0; i<1024; i++) {
		vec3s pos;
		pos.x = (float)(xorshift64(&e->seed)%0x100000)/0x0FFFFF;
		pos.x *= 3;
		pos.y = (float)(xorshift64(&e->seed)%0x100000)/0x0FFFFF;
		pos.y += 3;
		pos.z = (float)(xorshift64(&e->seed)%0x100000)/0x0FFFFF;
		pos.z *= 3;
		/* new_mob(e, &e->assets.models_anim[MODELS_ANIM_SOLDIER], &e->assets.models_anim[MODELS_ANIM_SOLDIER].anim_data, pos); */
		const int32_t id = new_model_static(&e->ecs, &e->assets.models_basic[MODELS_STATIC_CUBE], pos);
		e->ecs.flags[id].test0 = 1;
	}
#endif
}
