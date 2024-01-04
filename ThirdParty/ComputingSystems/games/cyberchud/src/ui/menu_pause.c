#include <SDL2/SDL_events.h>

#include "ui/menu_pause.h"
#include "ui/menu_controls.h"
#include "ui/menu_cubemap.h"
#include "ui/menu_debug.h"
#include "ui/menu_options.h"
#include "ui/widgets/button.h"

#define PAUSE_MENU_BUTTON_W 96
#define PAUSE_MENU_BUTTON_H 32
#define PAUSE_MENU_BUTTON_MARGIN_H 8

#define PAUSE_MENU_W 256
#define PAUSE_MENU_H 512

#define PAUSE_MENU_X1 (SCREEN_W/2 - PAUSE_MENU_W/2)
#define PAUSE_MENU_X2 (SCREEN_W/2 + PAUSE_MENU_W/2)
#define PAUSE_MENU_Y1 32
#define PAUSE_MENU_Y2 (PAUSE_MENU_Y1+PAUSE_MENU_H)

static inline void button_inc(engine_t* const e, rect_i16* const rect, const char* const str) {
	const size_t cnt = e->button_cnt++;
	e->buttons[cnt].str = str;
	e->buttons[cnt].rect = *rect;
	e->buttons[cnt].state = BUTTON_NORMAL;
	rect->y1 += PAUSE_MENU_BUTTON_H + PAUSE_MENU_BUTTON_MARGIN_H;
	rect->y2 += PAUSE_MENU_BUTTON_H + PAUSE_MENU_BUTTON_MARGIN_H;
}

static inline rect_i16 default_menu_rect() {
	rect_i16 rect;
	rect.x1 = SCREEN_W/2 - PAUSE_MENU_BUTTON_W/2;
	rect.x2 = SCREEN_W/2 + PAUSE_MENU_BUTTON_W/2;
	rect.y1 = PAUSE_MENU_Y1 + PAUSE_MENU_BUTTON_MARGIN_H;
	rect.y2 = rect.y1 + PAUSE_MENU_BUTTON_H;
	return rect;
}

void handle_menu_pause(engine_t* const e) {
	controls_t* const ctrl = &e->controls;
	/* Button RESUME */
	if (handle_button(&e->buttons[0], ctrl)) {
		e->ui_mode = UI_MODE_NONE;
		ctrl->show_menu = 0;
		ctrl->mouse_grab = 1;
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}

	/* Button Controls */
	if (handle_button(&e->buttons[1], ctrl)) {
		init_menu_controls(e);
	}

	/* Button OPTIONS */
	if (handle_button(&e->buttons[2], ctrl)) {
		init_menu_options(e);
	}

	/* Button DEBUG */
	if (handle_button(&e->buttons[3], ctrl)) {
		init_menu_debug(e);
	}

	/* Button QUIT TO TITLE */
	if (handle_button(&e->buttons[4], ctrl)) {
		e->switch_scene = 1;
		e->scene = SCENE_TITLE;
	}

	/* Button TERMINATE */
	if (handle_button(&e->buttons[5], ctrl)) {
		ctrl->close = 1;
	}
}

void draw_menu_pause(engine_t* const e) {
#if 0
#define MENU_W 256
#define MENU_H 512
	rect_i32 menu_rect;
	menu_rect.x1 = SCREEN_W/2 - MENU_W/2;
	menu_rect.x2 = SCREEN_W/2 + MENU_W/2;
	menu_rect.y1 = 32;
	menu_rect.y2 = 32+MENU_H;
#endif

	for (int i=0; i<e->button_cnt; i++) {
		draw_button_label(e->fb, &e->buttons[i], &e->assets.font_matchup);
	}
}

void init_menu_pause(engine_t* const e) {
	e->ui_mode = UI_MODE_PAUSE;
	rect_i16 rect = default_menu_rect();
	e->button_cnt = 0;
	e->checkbox_cnt = 0;
	e->slider_cnt = 0;
	button_inc(e, &rect, "Resume");
	button_inc(e, &rect, "Controls");
	button_inc(e, &rect, "Options");
	button_inc(e, &rect, "Debug");
	button_inc(e, &rect, "Quit To Title");
	button_inc(e, &rect, "TERMINATE");
}
