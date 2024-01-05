#include <SDL2/SDL.h>
#include "scene/title.h"
#include "mytime.h"
#include "quake.h"
#include "render.h"
#include "text.h"
#include "ui/menu_controls.h"
#include "ui/menu_options.h"
#include "ui/menu_title.h"
#include "ui/statusline.h"
#include "utils/minmax.h"
#include "utils/myds.h"

static inline void draw_gradient(u8* const fb, float t) {
	for (int y=0; y<STATUSLINE_H; y++) {
		for (int x=0; x<SCREEN_W; x++) {
			vec2s uv;
			uv.x = (float)x/SCREEN_W;
			uv.y = (float)y/SCREEN_H;
			for(float i = 1.0; i < 10.0; i++){
				uv.x += 0.6 / i * cosf(i * 2.5* uv.y + t);
				uv.y += 0.6 / i * cosf(i * 1.5 * uv.x + t);
			}

			float val = 0.2f/fabsf(sinf(t-uv.y-uv.x));
			if (val > 1) val = 1;
			if (val < 0) val = 0;
			const u8 pal = (u8)((float)val*15.0f);
			fb[y*SCREEN_W+x] = pal;
		}
	}
}

void title_update(engine_t *e, const float delta) {
	switch (e->ui_mode) {
		case UI_MODE_TITLE:
			handle_menu_title(e);
			break;
		case UI_MODE_CONTROLS:
			handle_menu_controls(e);
			break;
		case UI_MODE_OPTIONS:
			handle_menu_options(e);
			break;
		default:
			myprintf("[ERR] [title_update] e->ui_mode = %d\n", e->ui_mode);
			break;
	}
}

void title_draw(engine_threads_t *et, const float delta) {
#ifdef VERBOSE
	myprintf("[title_draw]\n");
#endif

	engine_t* const e = &et->e;
	ecs_t* const ecs = &e->ecs;

	/* Clear FB */
	/* memset(e->fb_real, 0, SCREEN_W*SCREEN_H); */

	{
		/* const i32 lid = e->scene_data.title.light_id; */
		/* vec3s pos; */
		/* pos.x = cosf(e->scene_time); */
		/* pos.z = sinf(e->scene_time); */
		/* ecs->pos[lid] = pos; */
	}

	{
		vec3s cam_pos = {{cosf(e->scene_time/2)*4,0,sinf(e->scene_time/2)*4}};
		cam_lookat(&e->cam, cam_pos, (vec3s){{0,0,0}});
	}


	const float mstime = e->scene_time*1000;
	for (i32 i=0, cnt=0; i<myarrlen(ecs->flags); i++) {
		const flag_t flags = ecs->flags[i];
		if (!flags.test0) continue;
		vec3s light_pos;
		light_pos.x = cosf(mstime/(500+cnt*4)+cnt) * 2;
		light_pos.y = sinf(mstime/(2000+cnt*4)+cnt) * 2;
		light_pos.z = sinf(mstime/(1000+cnt*4)+cnt) * 2;
		ecs->pos[i] = light_pos;
		cnt++;
	}

	ecs->brightness[e->scene_data.title.light_id] = ((cosf(e->scene_time*1.2)+1.0f)*0.5)*5.0f;
	/* vec3s neg_pos = glms_vec3_negate(e->cam.pos); */
	/* e->ecs.rot[e->scene_data.title.logo_id] = glms_quat_forp((vec3s){{0,0,0}}, neg_pos, (vec3s){{0,1,0}}); */

	/* 3D Render Pipeline */
	render(et, delta);

	draw_px(e->fb, e->assets.px_pal[PX_PAL_CYBERCHUD_TITLE], SCREEN_W/2 - e->assets.px_pal[PX_PAL_CYBERCHUD_TITLE]->w/2, 320);

	DrawText(e->fb, &e->assets.font_matchup, "HTTPS://SCUMGAMES.NEOCITIES.ORG/", 2, FB_H-e->assets.font_matchup.font_height);
	DrawText(e->fb, &e->assets.font_matchup, "CrunkLord420 / SCUMGAMES", 2, FB_H-e->assets.font_matchup.font_height*2);
	DrawText(e->fb, &e->assets.font_matchup, "VER. 0.0000420", SCREEN_W-92-2, FB_H-e->assets.font_matchup.font_height);

	/* Draw Cursor */
	draw_px_real(e->fb_real, e->assets.px_pal[PX_PAL_CURSOR], e->controls.mouse_abs.x, e->controls.mouse_abs.y);

	draw_gradient(e->fb_real, e->scene_time);
	draw_statusline(et, delta);
}

void title_init(engine_t* e) {
#ifndef NDEBUG
	fputs("[title_init]\n", stdout);
#endif
	e->scene_time = 0;
	e->scene_transition = 1;
	e->controls.show_menu = 1;
	e->warpcircle.w = -1;
	e->controls.show_wireframe = 1;
	SDL_SetRelativeMouseMode(SDL_FALSE);

	assets_load_level(&e->assets, &e->ecs.alloc, WAD_BSP_LEVELTITLE_BSP/2);
	ecs_reset(&e->ecs);
	SV_ClearWorld(e);

	const edict_light_t* const edict = new_light_dynamic(&e->ecs, (vec3s){{0,0,0}}, 4.0f, 0, 0, DYN_LIGHT_STYLE_NONE);
	e->scene_data.title.light_id = edict->basic.id;

	vec3s cam_pos = {{0,-2,8}};
	cam_init(&e->cam, cam_pos, -M_PI/2);
	cam_lookat(&e->cam, cam_pos, (vec3s){{0,0,0}});
	/* e->scene_data.title.logo_id = new_model_anim(e, &e->assets.models_anim[MODELS_ANIM_SCUMLOGO], &e->assets.models_anim[MODELS_ANIM_SCUMLOGO].anims[0], (vec3s){0}); */
	/* e->ecs.rot[e->scene_data.title.logo_id] = glms_quat_forp((vec3s){{0,0,0}}, (vec3s){{0,0,-1}}, (vec3s){{0,1,0}}); */

	for (int i=0; i<1024; i++) {
		vec3s pos;
		pos.x = frand(&e->seed);
		pos.x *= 3;
		pos.y = frand(&e->seed);
		pos.y += 3;
		pos.z = frand(&e->seed);
		pos.z *= 3;
		const int32_t id = new_model_static(&e->ecs, &e->assets.models_basic[MODELS_STATIC_CUBE], &pos);
		e->ecs.flags[id].test0 = 1;
	}

	init_menu_title(e);
}
