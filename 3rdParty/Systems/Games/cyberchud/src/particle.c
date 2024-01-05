#include "particle.h"
#include "quake.h"
#include "utils/minmax.h"
#include "utils/myds.h"

#define PARTICLE_GRAVITY 10.0f
#define PARTICLE_DRAG 1.0f

void particle_update(engine_t *e, const float delta) {
	ecs_t* const ecs = &e->ecs;

	/* Update Particles */
	for (i32 i=0; i<myarrlen(ecs->flags); i++) {
		const flag_t flags = ecs->flags[i];
		if (bitarr_get(ecs->bit_particle, i)) {
			/* Cooldown and Cull */
			ecs->cooldown0[i] -= delta;
			if (ecs->cooldown0[i] <= 0) {
				free_particle(ecs, i);
				continue;
			}

			bbox_t move;
			vec3s pos = ecs->pos[i];
			move.min = pos;
			vec3s vel = ecs->vel[i];

			vel.y -= PARTICLE_GRAVITY * delta;
			glm_vec3_scale(vel.raw, delta, move.max.raw);
			glm_vec3_add(move.min.raw, move.max.raw, move.max.raw);
			trace_t trace = {0};
			trace.endpos = move.max;
			SV_RecursiveHullCheck(&e->assets.map.qmods[0].hulls[0], &move, &trace, CONTENTMASK_ANYSOLID);
			if (!trace.inopen) {
				/* myprintf("traceopen c:%d %.2f %.2fx%.2fx%.2f\n", trace.contents, trace.fraction, trace.endpos.x, trace.endpos.y, trace.endpos.z); */
				free_particle(&e->ecs, i);
				continue;
			} else if (trace.contents != -1) {
				/* myprintf("trace c:%d %.2f %.2fx%.2fx%.2f\n", trace.contents, trace.fraction, trace.endpos.x, trace.endpos.y, trace.endpos.z); */
				ecs->pos[i] = trace.endpos;
				ecs->vel[i] = (vec3s){{0,0,0}};
			} else {
				/* myprintf("trace d:%.2f c:%d %.2f %.2fx%.2fx%.2f\n", delta, trace.contents, trace.fraction, trace.endpos.x, trace.endpos.y, trace.endpos.z); */
				ecs->pos[i] = trace.endpos;
				vec3s vel2;
				glm_vec3_scale(vel.raw, MIN(PARTICLE_DRAG*delta, 1.0f), vel2.raw);
				glm_vec3_sub(vel.raw, vel2.raw, vel.raw);
				ecs->vel[i] = vel;
			}
		} else if (flags.decay) {
			ecs->cooldown0[i] -= delta;
			if (ecs->cooldown0[i] <= 0) {
				free_beam(ecs, i);
			}
		}
	}
}

void particle_emitter_update(engine_t *e, const float delta) {
	ecs_t* const ecs = &e->ecs;
	for (i32 i=0; i<myarrlen(ecs->flags); i++) {
		if (!bitarr_get(ecs->bit_particle_emitter, i))
			continue;
		particle_emitter_t* const emitter = &ecs->custom0[i].particle_emitter;
		if (emitter->cooldown <= 0) {
			emitter->cooldown = emitter->spawn_speed;
			vec3s pos, vel;
			rand_vec3(&e->seed, vel.raw);
			const float particle_spread = 0.2f;
			glm_vec3_scale(vel.raw, particle_spread, pos.raw);
			const float particle_speed = 2.0f;
			glm_vec3_scale(vel.raw, particle_speed, vel.raw);
			glm_vec3_add(pos.raw, ecs->pos[i].raw, pos.raw);
			new_particle(ecs, &pos, &vel, emitter->model, 0);
		}
		emitter->cooldown -= delta;
	}
}
