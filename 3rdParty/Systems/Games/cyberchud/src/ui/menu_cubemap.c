#include <SDL2/SDL_events.h>

#include "ui/menu_cubemap.h"
#include "engine.h"

#define CUBEMAP_MENU_BUTTON_START_Y 64
#define CUBEMAP_MENU_BUTTON_W 96
#define CUBEMAP_MENU_BUTTON_H 32
#define CUBEMAP_MENU_BUTTON_MARGIN_H 4

#define CUBEMAP_MENU_X1 (SCREEN_W/2 - CUBEMAP_MENU_W/2)
#define CUBEMAP_MENU_X2 (SCREEN_W/2 + CUBEMAP_MENU_W/2)
#define CUBEMAP_MENU_Y1 32
#define CUBEMAP_MENU_Y2 (CUBEMAP_MENU_Y1+CUBEMAP_MENU_H)

static inline void button_inc(engine_t* const e, rect_i16* const rect, const char* const str) {
	const size_t cnt = e->button_cnt++;
	e->buttons[cnt].str = str;
	e->buttons[cnt].rect = *rect;
	e->buttons[cnt].state = BUTTON_NORMAL;
	rect->y1 += CUBEMAP_MENU_BUTTON_H + CUBEMAP_MENU_BUTTON_MARGIN_H;
	rect->y2 += CUBEMAP_MENU_BUTTON_H + CUBEMAP_MENU_BUTTON_MARGIN_H;
}

static inline rect_i16 default_menu_rect() {
	rect_i16 rect;
	rect.x1 = SCREEN_W/2 - CUBEMAP_MENU_BUTTON_W/2;
	rect.x2 = SCREEN_W/2 + CUBEMAP_MENU_BUTTON_W/2;
	rect.y1 = CUBEMAP_MENU_Y1 + CUBEMAP_MENU_BUTTON_MARGIN_H;
	rect.y2 = rect.y1 + CUBEMAP_MENU_BUTTON_H;
	return rect;
}

static void draw_cubemap(engine_t* e, const int startx, const int starty, cubemap_t* cubemap, const int fidx) {
#define DEBUG_CUBEMAP_SCALE 2.3
	for (int y=0; y<SHADOW_HEIGHT/DEBUG_CUBEMAP_SCALE; y++) {
		for (int x=0; x<SHADOW_WIDTH/DEBUG_CUBEMAP_SCALE; x++) {
			const int cidx = (int)((float)y*DEBUG_CUBEMAP_SCALE)*SHADOW_WIDTH + (int)((float)x*DEBUG_CUBEMAP_SCALE);
			e->fb[(starty+y)*SCREEN_W+x+startx] = ((uint64_t)(cubemap->depthbuffer[fidx][cidx]*10)%11)+3;
		}
	}
}

static void draw_cubemaps(engine_t* e, const int startx, const int starty, cubemap_t* cubemaps) {
	int x = startx;
	int y = starty;
	for (int i=0; i<3; i++, x+=SHADOW_WIDTH/DEBUG_CUBEMAP_SCALE+2) {
		draw_cubemap(e, x, y, cubemaps, i);
	}
	x = startx;
	y += SHADOW_HEIGHT/DEBUG_CUBEMAP_SCALE+2;
	for (int i=3; i<6; i++, x+=SHADOW_WIDTH/DEBUG_CUBEMAP_SCALE+2) {
		draw_cubemap(e, x, y, cubemaps, i);
	}
}

static void draw_cubemap_occlusion(engine_t* e, const int startx, const int starty, cubemap_occlusion_t* cubemap, const int fidx) {
#define DEBUG_CUBEMAP_SCALE 2.3
	for (int y=0; y<SHADOW_HEIGHT/DEBUG_CUBEMAP_SCALE; y++) {
		for (int x=0; x<SHADOW_WIDTH/DEBUG_CUBEMAP_SCALE; x++) {
			const int cidx = (int)((float)y*DEBUG_CUBEMAP_SCALE)*SHADOW_WIDTH + (int)((float)x*DEBUG_CUBEMAP_SCALE);
			e->fb[(starty+y)*SCREEN_W+x+startx] = ((uint64_t)(bitarr_get(cubemap->shadowfield[fidx], cidx)*10)%11)+3;
		}
	}
}

static void draw_cubemaps_occlusion(engine_t* e, const int startx, const int starty, cubemap_occlusion_t* cubemaps) {
	int x = startx;
	int y = starty;
	for (int i=0; i<3; i++, x+=SHADOW_WIDTH/DEBUG_CUBEMAP_SCALE+2) {
		draw_cubemap_occlusion(e, x, y, cubemaps, i);
	}
	x = startx;
	y += SHADOW_HEIGHT/DEBUG_CUBEMAP_SCALE+2;
	for (int i=3; i<6; i++, x+=SHADOW_WIDTH/DEBUG_CUBEMAP_SCALE+2) {
		draw_cubemap_occlusion(e, x, y, cubemaps, i);
	}
}

void handle_menu_cubemap(engine_t* const e) {
	controls_t* const ctrl = &e->controls;
	/* Button PREV */
	if (handle_button(&e->buttons[0], &e->controls)) {
		if (--e->ui_int0 < 0) {
			e->ui_int0 = MAX_DYNAMIC_LIGHTS-1;
		}
	}

	/* Button NEXT */
	if (handle_button(&e->buttons[1], &e->controls)) {
		e->ui_int0 = (e->ui_int0+1)%MAX_DYNAMIC_LIGHTS;
	}

	/* Button Pointlights/Shadowcasters */
	if (handle_button(&e->buttons[2], &e->controls)) {
		switch (e->ui_state.cubemap) {
			case CUBEMAP_UI_STATE_POINTLIGHTS:
				e->ui_state.cubemap = CUBEMAP_UI_STATE_SHADOWCASTER;
				break;
			case CUBEMAP_UI_STATE_SHADOWCASTER:
				e->ui_state.cubemap = CUBEMAP_UI_STATE_POINTLIGHTS;
				break;
		}
	}

	/* Button TOGGLE */
	if (handle_button(&e->buttons[3], &e->controls)) {
		e->ui_state.cubemap = !e->ui_state.cubemap;
	}

	/* Button RETURN */
	if (handle_button(&e->buttons[4], &e->controls)) {
		e->ui_mode = UI_MODE_NONE;
		ctrl->show_menu = 0;
		ctrl->mouse_grab = 1;
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
}

void draw_menu_cubemap(engine_t* const e) {
	/* Draw Framebuffers */
	switch (e->ui_state.cubemap) {
		case CUBEMAP_UI_STATE_POINTLIGHTS:
			draw_cubemaps(e, 0, 0, &e->pointlights.cubemaps[e->ui_int0]);
			break;
		case CUBEMAP_UI_STATE_SHADOWCASTER:
			draw_cubemaps_occlusion(e, 0, 0, &e->shadowcasters.cubemaps[e->ui_int0]);
			break;
	}

	/* Draw Buttons */
	for (int i=0; i<e->button_cnt; i++) {
		draw_button_label(e->fb, &e->buttons[i], &e->assets.font_matchup);
	}
}

void init_menu_cubemap(engine_t* const e) {
	e->ui_mode_back = e->ui_mode;
	e->ui_mode = UI_MODE_CUBEMAPS;
	e->ui_state.cubemap = CUBEMAP_UI_STATE_SHADOWCASTER;
	e->ui_int0 = 0;
	rect_i16 rect = default_menu_rect();
	e->button_cnt = 0;
	e->checkbox_cnt = 0;
	e->slider_cnt = 0;
	button_inc(e, &rect, "PREV");
	button_inc(e, &rect, "NEXT");
	button_inc(e, &rect, "Lights/Shadows");
	button_inc(e, &rect, "TOGGLE");
	button_inc(e, &rect, "RETURN");
}
