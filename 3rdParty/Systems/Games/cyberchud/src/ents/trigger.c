#include "trigger.h"
#include "engine.h"
#include "text.h"

void think_suicide(void* const data, int32_t id, const float delta) {
	engine_t *e = data;
#ifdef VERBOSE
	myprintf("[trigger_suicide] [%d]\n", id);
#endif
	free_bsp_trigger(&e->ecs, id);
}

void touch_trigger_multiple(void *data, int32_t id, int32_t other) {
#ifndef NDEBUG
	myprintf("[touch_trigger_multiple] %d %d\n", id, other);
#endif
}

void touch_trigger_once(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifndef NDEBUG
	myprintf("[touch_trigger_once] %d %d\n", id, other);
#endif
	e->ecs.ai[id].think = think_suicide;
}

void touch_trigger_neverhappen(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifndef NDEBUG
	myprintf("[touch_trigger_neverhappen] %d %d\n", id, other);
#endif
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	e->vfx_flags.neverhappen = 1;
	e->neverhappen_time = 0;
	e->ecs.ai[id].think = think_suicide;
}

void touch_trigger_glitchout(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifndef NDEBUG
	myprintf("[touch_trigger_glitchout] %d %d\n", id, other);
#endif
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	e->vfx_flags.glitchout = 1;
	e->glitchout_time = 0;
	e->ecs.ai[id].think = think_suicide;
}

void touch_trigger_noise(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifndef NDEBUG
	myprintf("[touch_trigger_noise] %d %d\n", id, other);
#endif
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	e->vfx_flags.noise = 1;
	e->noise_time = 0;
	e->ecs.ai[id].think = think_suicide;
}

void touch_trigger_wireframe(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifndef NDEBUG
	myprintf("[touch_trigger_wireframe] %d %d\n", id, other);
#endif
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	e->vfx_flags.wireframe = 1;
	e->wireframe_time = 0;
	e->ecs.ai[id].think = think_suicide;
}

void touch_talk(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifdef VERBOSE
	myprintf("[touch_talk] %d %d\n", id, other);
#endif
	ecs_t* const ecs = &e->ecs;
	if (e->talkbox.enabled || ecs->player_id != other)
		return;
	edict_t* const edict = ecs->edict[id].edict;
	vec3s out;
	if (ecs->target[id] >= 0) {
		out = ecs->pos[ecs->target[id]];
	} else {
		glm_vec3_sub(edict->absbox.max.raw, edict->absbox.min.raw, out.raw);
		glm_vec3_scale(out.raw, 0.5, out.raw);
		glm_vec3_add(out.raw, edict->absbox.min.raw, out.raw);
	}
	talkbox_init(&e->talkbox, ecs->custom0[id].trigger_talk.diag_idx, e, out);
	/* talkbox_init(&e->talkbox, ecs->custom0[id].trigger_talk.diag_idx, e, ecs->pos[id]); */
	ecs->ai[id].think = think_suicide;
}

void touch_trigger_levelchange(void *data, int32_t id, int32_t other) {
	engine_t *e = data;
#ifndef NDEBUG
	myprintf("[touch_trigger_levelchange] id:%d other:%d player_id:%d\n", id, other, e->ecs.player_id);
#endif
	if (other == e->ecs.player_id)
		e->switch_level = 1;
}
