#include "ents/dynlight.h"
#include "engine.h"
#include "text.h"

void think_dynlight(void *data, int32_t id, const float delta) {
	engine_t* const e = data;
	ecs_t* const ecs = &e->ecs;
	const i32 target_id = ecs->target[id];
	dynamiclight_t* const light = &ecs->custom0[id].light;
#ifdef VERBOSE
	myprintf("[think_dynlight] [%d] style:%d target:%d\n", id, light->style, target_id);
#endif
	switch (light->style) {
		case DYN_LIGHT_STYLE_NONE:
			break;
		case DYN_LIGHT_STYLE_LINE:
			if (target_id >= 0) {
				vec3s target_pos = ecs->pos[target_id];
				vec3s* from;
				vec3s* to;
				dynamiclight_t* const light = &ecs->custom0[id].light;
				const float delta2 = delta * light->speed;
				float t = light->ltime;
				if (light->direction) {
					t -= delta2;
					if (t <= 0.0) {
						t = 0.0f;
						light->direction = 0;
					}
					/* to = &ecs->pos_home[id]; */
					/* from = &target_pos; */
				} else {
					t += delta2;
					if (t >= 1.0) {
						t = 1.0f;
						light->direction = 1;
					}
					/* from = &ecs->pos_home[id]; */
					/* to = &target_pos; */
				}
				from = &ecs->pos_home[id];
				to = &target_pos;
				light->ltime = t;
				glm_vec3_lerp(from->raw, to->raw, t, ecs->pos[id].raw);
			}
			break;
		case DYN_LIGHT_STYLE_ORBIT:
			{
				vec3s pos = ecs->pos_home[id];
				float s, c;
				sincosf(e->scene_time*light->speed, &s, &c);
				pos.x += c*light->range;
				pos.z += s*light->range;
				ecs->pos[id] = pos;
			}
			break;
	}
}
