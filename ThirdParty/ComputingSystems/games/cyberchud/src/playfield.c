#include <assert.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_video.h>

#include "playfield.h"
#include "mytime.h"
#include "quake.h"
#include "text.h"
#include "particle.h"
#include "phys.h"
#include "sound.h"
#include "ents/player.h"
#include "ents/spawner.h"
#include "ui/menu_controls.h"
#include "ui/menu_cubemap.h"
#include "ui/menu_debug.h"
#include "ui/menu_options.h"
#include "ui/menu_pause.h"
#include "utils/myds.h"
#include "vfx/vfx_camrock.h"

#define BSP_SWAP

void playfield_update(engine_t* const e, const float delta) {
	ecs_t* const ecs = &e->ecs;
	controls_t* const ctrl = &e->controls;
	if (e->switch_level) {
		e->switch_level = 0;
		if (assets_load_level(&e->assets, &ecs->alloc, ++e->current_level))
			myprintf("[ERR] [playfield_update] assets_load_level\n");
		ecs_reset(ecs);
		SV_ClearWorld(e);
	}

	if (ctrl->init_menu) {
		ctrl->init_menu = 0;
		init_menu_pause(e);
	}

	const int32_t player_id = ecs->player_id;

	/* Handle Controls */
#define CAM_SPEED (1500.0f)
#define PLAYER_SPEED (16.0f)
	vec2s wasd = {0};
	if (ctrl->forwardDown)
		wasd.y += 1;
	if (ctrl->backwardDown)
		wasd.y -= 1;
	if (ctrl->leftDown)
		wasd.x -= 1;
	if (ctrl->rightDown)
		wasd.x += 1;
	glm_vec2_normalize(wasd.raw);

	if (ctrl->noclip) {
		const float vel = delta * CAM_SPEED * ctrl->fly_speed;
		vec3s fdir;
		fdir.x = e->cam.front.x*wasd.y + e->cam.right.x*wasd.x;
		fdir.y = e->cam.front.y*wasd.y + e->cam.right.y*wasd.x;
		fdir.z = e->cam.front.z*wasd.y + e->cam.right.z*wasd.x;
		glm_vec3_normalize(fdir.raw);
		ecs->vel[player_id] = glms_vec3_add(ecs->vel[player_id], glms_vec3_scale(fdir, vel));
	} else {
		float speed = PLAYER_SPEED;
		if (e->controls.walk)
			speed /= 3;
		vec3s fdir;
		fdir.x = e->cam.front.x*wasd.y + e->cam.right.x*wasd.x;
		fdir.y = 0;
		fdir.z = e->cam.front.z*wasd.y + e->cam.right.z*wasd.x;
		glm_vec3_normalize(fdir.raw);
		if (ecs->edict[player_id].edict->flags&FL_ONGROUND) {
			SV_UserFriction(e, player_id, delta);
			SV_Accelerate(ecs, player_id, speed, fdir, delta);
		} else {
			SV_AirAccelerate(ecs, player_id, speed, fdir, delta);
		}
	}

	/* Handle Pause Menu */
	switch (e->ui_mode) {
		case UI_MODE_NONE:
			break;
		case UI_MODE_PAUSE:
			handle_menu_pause(e);
			break;
		case UI_MODE_CONTROLS:
			handle_menu_controls(e);
			break;
		case UI_MODE_OPTIONS:
			handle_menu_options(e);
			break;
		case UI_MODE_CUBEMAPS:
			handle_menu_cubemap(e);
			break;
		case UI_MODE_DEBUG:
			handle_menu_debug(e);
			break;
		default:
			myprintf("[ERR] [playfield_update] e->ui_mode = %d\n", e->ui_mode);
			break;
	}

	/* Handle Talkbox */
	talkbox_handle(&e->talkbox, &e->controls, delta);

	if (ctrl->weapon_switch >= 0)
		player_switch_weapon(e, ctrl->weapon_switch);

#if 0
	/* Run AI */
	{
		const TIME_TYPE start = get_time();
		for (i32 i=0; i<myarrlen(ecs->flags); i++) {
			if (!bitarr_get(ecs->bit_edict, i))
				continue;
			if (ecs->ai[i].think)
				ecs->ai[i].think(e, i, delta);
		}
		e->time_ai = time_diff(start, get_time())*1000;
	}
#endif

#if 0
	/* Generate AABBs */
	for (i32 i=0; i<(i32)myarrlenu(ecs->flags); i++) {
		const flag_t flags = ecs->flags[i];
		if (flags.mob) {
			const vec3s* const pos = ecs->pos + i;
			const bbox_t* const mesh_bbox = &ecs->model[i].model_anim->basic.bbox;
			bbox_t* const bbox = ecs->bbox + i;
			glm_vec3_add((float*)pos->raw, (float*)mesh_bbox->min.raw, bbox->min.raw);
			glm_vec3_add((float*)pos->raw, (float*)mesh_bbox->max.raw, bbox->max.raw);
		}
	}
#endif

	/* Fire Hitscan */
	ecs->player.fire_cooldown -= delta;
	if (ctrl->fire)
		player_fire(e);

	/* Touch Buttons */
	if (ctrl->enterPressed) {
		trace_t trace = {0};
		bbox_t move;
		move.min = e->cam.pos;
		move.max = move.min;
		vec3 dir;
		glm_vec3_scale(e->cam.front.raw, 2, dir);
		glm_vec3_add(dir, move.max.raw, move.max.raw);
		TraceLine(e, &e->assets.map.qmods[0].hulls[0], &move, &trace, player_id);
		/* myprintf("trace c:%d %.2f %.2fx%.2fx%.2f\n", trace.contents, trace.fraction, trace.endpos.x, trace.endpos.y, trace.endpos.z); */
		if (trace.ent) {
			/* myprintf("trace:%d\n", trace.ent->basic.id); */
			if (trace.ent->click) {
				trace.ent->click(e, trace.ent->basic.id, player_id);
			}
		}
	}

#if 0
	/* Gravity */
	for (i32 i=0; i<(i32)myarrlenu(ecs->flags); i++) {
		if (ecs->flags[i].gravity && !ctrl->noclip) {
			ecs->vel[i].y -= delta * 0.0001f;
		}
	}
#endif

	if (ctrl->jumpPressed) {
		if (ecs->edict[player_id].edict->flags&FL_ONGROUND) {
			snd_play_sfx(&e->audio, e->assets.snds[SND_JUMP]);
			ctrl->jumpPressed = 0;
			ecs->vel[player_id].y = 20.0f;
		}
	}

	if (ctrl->noclip) {
		ecs->flags[player_id].noclip = 1;
	} else {
		ecs->flags[player_id].noclip = 0;
	}

	/* update edicts */
	{
		const TIME_TYPE start = get_time();
		const i32 ent_cnt = myarrlen(ecs->flags);
		for (i32 i=0; i<ent_cnt; i++) {
			if (bitarr_get(ecs->bit_edict, i)) {
				/* TODO optimize? */
				glm_vec3_add(ecs->pos[i].raw, ecs->bbox[i].min.raw, ecs->edict[i].edict->absbox.min.raw);
				glm_vec3_add(ecs->pos[i].raw, ecs->bbox[i].max.raw, ecs->edict[i].edict->absbox.max.raw);

				if (ecs->edict[i].edict->solid == SOLID_SLIDEBOX) {
					/* myprintf("=trigger %d=\n", i); */
					/* print_vec3("XXZentmin", ecs->edict[i].absbox.min); */
					/* print_vec3("XXZentmax", ecs->edict[i].absbox.max); */
					/* print_vec3("XXZentmin", ecs->bbox[i].min); */
					/* print_vec3("XXZentmax", ecs->bbox[i].max); */
				}
			}
		}
		e->time_absbox = time_diff(start, get_time())*1000;
	}

	/* TODO: optimize how AI is called */
	/* Update Dynamic Lights */
	{
		const TIME_TYPE start = get_time();
		const i32 elen = myarrlen(ecs->flags);
		for (i32 i=0; i<elen; i++) {
			if (bitarr_get(ecs->bit_light_dynamic, i)) {
				ecs->ai[i].think(e, i, delta);
			}
		}
		e->time_ai_light = time_diff(start, get_time())*1000;
	}

	/* Update Spawner */
	spawners_update(e, delta);

	/* Update Particle Emitters */
	{
		const TIME_TYPE start = get_time();
		particle_emitter_update(e, delta);
		e->time_particle_emitter = time_diff(start, get_time())*1000;
	}

	/* Update Particles */
	{
		const TIME_TYPE start = get_time();
		particle_update(e, delta);
		e->time_particle = time_diff(start, get_time())*1000;
	}

	/* Update Edicts */
	{
		const TIME_TYPE start = get_time();
		update_edicts(e, delta);
		e->time_edict = time_diff(start, get_time())*1000;
	}

	/* Update Camera */
	vfx_camrock(e, delta);
	e->cam.pos = ecs->pos[player_id];
	/* adjust cam to place inside mob "head" */
	e->cam.pos.y += 0.5f;
	if (!ctrl->show_menu) {
		cam_process_movement(&e->cam, ctrl->mouse.x, ctrl->mouse.y);
	} else {
		cam_process_movement(&e->cam, 0, 0);
	}

	/* Update PoV Gun Model */
	/* TODO camera stuff shouldn't be in update? */
	glm_vec3_scale(e->cam.front.raw, 0.4, ecs->pos[ecs->pov_model_id].raw);
	const float r = 0.2f;

	/* TODO */
	static const vec3s WEAPON_POV_OFFSET[WEAPON_TOTAL] = { {{0}}, {{0,-0.2,0}}, {{0,-0.2,0}}, {{0,-0.2,0}} };

	ecs->pos[ecs->pov_model_id].x += e->cam.right.x*r;
	ecs->pos[ecs->pov_model_id].y += e->cam.right.y*r + WEAPON_POV_OFFSET[ecs->player.cur_weapon].y;
	ecs->pos[ecs->pov_model_id].z += e->cam.right.z*r;
	glm_vec3_add(ecs->pos[e->ecs.pov_model_id].raw, e->cam.pos.raw, ecs->pos[ecs->pov_model_id].raw);
	vec3s inv_front;
	inv_front.x = -e->cam.front.x;
	inv_front.y = -e->cam.front.y;
	inv_front.z = -e->cam.front.z;
	glm_quat_for(inv_front.raw, (vec3){0,1,0}, ecs->rot[ecs->pov_model_id].raw);
	glm_quat_for(e->cam.front.raw, (vec3){0,1,0}, ecs->rot[ecs->player_id].raw);

#if 0
	/* Collision */
	arrsetlen(e->idxs_culled, 0);
	arrsetlen(e->idxs_bones, 0);
	for (i32 i=0; i<(i32)myarrlenu(ecs->flags); i++) {
		if (ecs->flags[i].collision) {
			ecs->flags[i].highlight = 0;
			arrput(e->idxs_culled, i);
		}
		if (ecs->flags[i].collider) {
			arrput(e->idxs_bones, i);
		}
	}

	const i32 len = myarrlenu(e->idxs_culled);
	const i32 tlen = myarrlenu(e->idxs_bones);
	const u32* idxs = e->idxs_culled;
	const u32* tidxs = e->idxs_bones;
	for (i32 i=0; i<len; i++) {
		const size_t idx = idxs[i];
		collider_box_t box0;
		box0.pos = ecs->pos[idx];
		box0.size = ecs->size[idx];
		box0.rot = ecs->rot[idx];
		for (i32 ti=0; ti<tlen; ti++) {
			const size_t tidx = tidxs[ti];
			if (idx == tidx) continue;
			collider_box_t box1;
			box1.pos = ecs->pos[tidx];
			box1.size = ecs->size[tidx];
			box1.rot = ecs->rot[tidx];
			float depth;
			vec3s pos;
			vec3s dir;
			const int intersect = ccd_boxbox(&box0, &box1, &depth, &dir, &pos);
			if (intersect == 0) {
				ecs->flags[idx].highlight = 1;
				/* myprintf("intersect: %d pos:%fx%fx%f depth:%f dir:%fx%fx%f\nbox:%fx%fx%f\n", intersect, pos.x, pos.y, pos.z, depth, dir.x, dir.y, dir.z, box0.pos.x, box0.pos.y, box0.pos.z); */
				assert(!isnan(dir.x));
				ecs->pos[idx] = HMM_Add(ecs->pos[idx], glms_vec3_scale(dir, depth));
				ecs->vel[idx] = glms_vec3_scale(glms_vec3_scale(dir, depth), 100);
				break;
			}
		}
	}
#endif
}
