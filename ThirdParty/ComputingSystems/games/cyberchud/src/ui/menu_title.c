#include "ui/menu_title.h"
#include "ui/menu_controls.h"
#include "ui/menu_options.h"
#include "text.h"

#define MENU_W 256
#define MENU_H 512
#define MENU_BUTTON_START_Y 64
#define MENU_BUTTON_W 140
#define MENU_BUTTON_H 32
#define MENU_BUTTON_MARGIN_H 4

static inline void button_inc(button_label_t* const ui, rect_i16* const rect, const char* const str) {
	ui->str = str;
	ui->rect = *rect;
	ui->state = BUTTON_NORMAL;
	rect->y1 += MENU_BUTTON_H + MENU_BUTTON_MARGIN_H;
	rect->y2 += MENU_BUTTON_H + MENU_BUTTON_MARGIN_H;
}

void init_menu_title(engine_t* const e) {
#ifndef NDEBUG
	myprintf("[init_menu_title]\n");
#endif
	e->ui_mode = UI_MODE_TITLE;
	e->button_cnt = 5;
	e->checkbox_cnt = 0;
	e->slider_cnt = 0;

	rect_i16 button_rect;
	button_rect.x1 = SCREEN_W/2 - MENU_BUTTON_W/2;
	button_rect.x2 = SCREEN_W/2 + MENU_BUTTON_W/2;
	button_rect.y1 = MENU_BUTTON_START_Y;
	button_rect.y2 = MENU_BUTTON_START_Y + MENU_BUTTON_H;

	button_inc(&e->buttons[0], &button_rect, "Play");
	button_inc(&e->buttons[1], &button_rect, "Randomize Palette");
	button_inc(&e->buttons[2], &button_rect, "Controls");
	button_inc(&e->buttons[3], &button_rect, "Options");
	button_inc(&e->buttons[4], &button_rect, "Quit");
}

void handle_menu_title(engine_t* const e) {
	/* Play */
	if (handle_button(&e->buttons[0], &e->controls)) {
		e->scene_transition = 1.0f;
		e->switch_scene = 1;
		e->scene = SCENE_PLAYFIELD;
	}

	/* Randomize Palette */
	if (handle_button(&e->buttons[1], &e->controls)) {
		e->controls.randomize_palette = 1;
	}

	/* Controls */
	if (handle_button(&e->buttons[2], &e->controls)) {
		init_menu_controls(e);
	}

	/* Options */
	if (handle_button(&e->buttons[3], &e->controls)) {
		init_menu_options(e);
	}

	/* Quit */
	if (handle_button(&e->buttons[4], &e->controls)) {
		e->controls.close = 1;
	}
}

