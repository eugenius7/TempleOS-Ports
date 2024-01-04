#include "ui/menu_options.h"
#include "ui/menu_common.h"

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

static inline void checkbox_inc(checkbox_t* const ui, rect_i16* const rect, const char* const str, const i8 checked) {
	ui->str = str;
	ui->pos.x = rect->x1;
	ui->pos.y = rect->y1;
	ui->state = BUTTON_NORMAL;
	ui->checked = checked;
	rect->y1 += 16 + MENU_BUTTON_MARGIN_H;
	rect->y2 += 16 + MENU_BUTTON_MARGIN_H;
}

static inline void slider_inc(slider_t* const ui, rect_i16* const rect, const assets_t* const assets, const i16 val, const i16 max) {
	slider_init(ui, assets, rect->x1, rect->y1, val, max);
	rect->y1 += 16 + MENU_BUTTON_MARGIN_H;
	rect->y2 += 16 + MENU_BUTTON_MARGIN_H;
}

void init_menu_options(engine_t* const e) {
	e->ui_mode_back = e->ui_mode;
	e->ui_mode = UI_MODE_OPTIONS;
	e->button_cnt = 1;
	e->checkbox_cnt = 4;
	e->slider_cnt = 2;

	rect_i16 button_rect;
	button_rect.x1 = SCREEN_W/2 - MENU_BUTTON_W/2;
	button_rect.x2 = SCREEN_W/2 + MENU_BUTTON_W/2;
	button_rect.y1 = MENU_BUTTON_START_Y;
	button_rect.y2 = MENU_BUTTON_START_Y + MENU_BUTTON_H;

	checkbox_inc(&e->checkboxes[0], &button_rect, "Mute", e->audio.mute);
	checkbox_inc(&e->checkboxes[1], &button_rect, "Enable FPS Graph", e->flags.fps_graph);
	checkbox_inc(&e->checkboxes[2], &button_rect, "Enable Pointlights Cubemaps [SLOW]", e->flags.pointlight_shadows_enabled);
	checkbox_inc(&e->checkboxes[3], &button_rect, "Enable Shadowcasters Cubemaps [SLOW]", e->flags.shadowcaster_shadows_enabled);

	slider_inc(&e->sliders[0], &button_rect, &e->assets, e->max_pointlights, MAX_DYNAMIC_LIGHTS);
	slider_inc(&e->sliders[1], &button_rect, &e->assets, e->max_shadowcasters, MAX_DYNAMIC_LIGHTS);

	button_inc(&e->buttons[0], &button_rect, "BACK");
}

void handle_menu_options(engine_t* const e) {
	if (handle_button(&e->buttons[0], &e->controls))
		menu_goback(e);

	if (handle_checkbox(&e->checkboxes[0], &e->controls)) {
		e->audio.mute = e->checkboxes[0].checked;
	}
	if (handle_checkbox(&e->checkboxes[1], &e->controls)) {
		e->flags.fps_graph = e->checkboxes[1].checked;
	}
	if (handle_checkbox(&e->checkboxes[2], &e->controls)) {
		e->flags.pointlight_shadows_enabled = e->checkboxes[2].checked;
		engine_cfg_update(e);
	}
	if (handle_checkbox(&e->checkboxes[3], &e->controls)) {
		e->flags.shadowcaster_shadows_enabled = e->checkboxes[3].checked;
		engine_cfg_update(e);
	}

	e->sliders[0].val = e->max_pointlights;
	handle_slider(&e->sliders[0], &e->controls, &e->assets);
	e->max_pointlights = e->sliders[0].val;

	e->sliders[1].val = e->max_shadowcasters;
	handle_slider(&e->sliders[1], &e->controls, &e->assets);
	e->max_shadowcasters = e->sliders[1].val;
}
