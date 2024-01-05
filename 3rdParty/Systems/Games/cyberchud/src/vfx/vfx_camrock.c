#include "vfx/vfx_camrock.h"

void vfx_camrock(engine_t* const e, const float delta) {
	if (!e->vfx_flags.camrock)
		return;
	if (e->camrock_time >= 1.0f/3) {
		e->vfx_flags.camrock = 0;
		return;
	}
	const float s = sinf(e->camrock_time*3*M_PI*2)*0.005;
	e->cam.pitch += s;
	e->camrock_time += delta;
}
