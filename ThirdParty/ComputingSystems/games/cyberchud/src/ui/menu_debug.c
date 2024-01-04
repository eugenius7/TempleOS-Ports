#include "ui/menu_debug.h"
#include "ui/menu_cubemap.h"
#include "ui/menu_common.h"
#include "text.h"

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

static inline void checkbox_inc(engine_t* const e, rect_i16* const rect, const char* const str, const i8 checked) {
	const size_t cnt = e->checkbox_cnt++;
	e->checkboxes[cnt].str = str;
	e->checkboxes[cnt].pos.x = rect->x1;
	e->checkboxes[cnt].pos.y = rect->y1;
	e->checkboxes[cnt].checked = checked;
	rect->y1 += 16 + PAUSE_MENU_BUTTON_MARGIN_H;
	rect->y2 += 16 + PAUSE_MENU_BUTTON_MARGIN_H;
}

static inline rect_i16 default_menu_rect() {
	rect_i16 rect;
	rect.x1 = SCREEN_W/2 - PAUSE_MENU_BUTTON_W/2;
	rect.x2 = SCREEN_W/2 + PAUSE_MENU_BUTTON_W/2;
	rect.y1 = PAUSE_MENU_Y1 + PAUSE_MENU_BUTTON_MARGIN_H;
	rect.y2 = rect.y1 + PAUSE_MENU_BUTTON_H;
	return rect;
}

void init_menu_debug(engine_t* const e) {
	e->ui_mode_back = e->ui_mode;
	e->ui_mode = UI_MODE_DEBUG;
	rect_i16 rect = default_menu_rect();
	e->button_cnt = 0;
	e->checkbox_cnt = 0;
	e->slider_cnt = 0;
	button_inc(e, &rect, "Next Level");
	button_inc(e, &rect, "Cubemaps");
	checkbox_inc(e, &rect, "Wireframe", e->controls.show_wireframe);
	checkbox_inc(e, &rect, "Perf Metrics", e->controls.show_debug);
	checkbox_inc(e, &rect, "Show PVS+AABB", e->controls.show_pvs);
	checkbox_inc(e, &rect, "Skip Frag", e->vfx_flags.skip_frag);
	button_inc(e, &rect, "Back");
}

void handle_menu_debug(engine_t* const e) {
	controls_t* const ctrl = &e->controls;
	int button_idx = 0;
	int checkbox_idx = 0;

	/* Button Next Level */
	if (handle_button(&e->buttons[button_idx++], ctrl))
		e->switch_level = 1;

	/* Button Cubemaps */
	if (handle_button(&e->buttons[button_idx++], ctrl))
		init_menu_cubemap(e);

	/* Button BACK */
	if (handle_button(&e->buttons[button_idx++], ctrl))
		menu_goback(e);

	/* Button Wireframe */
	if (handle_checkbox(&e->checkboxes[checkbox_idx++], ctrl))
		e->controls.show_wireframe = !e->controls.show_wireframe;

	/* Button Performance */
	if (handle_checkbox(&e->checkboxes[checkbox_idx++], ctrl))
		e->controls.show_debug = !e->controls.show_debug;

	/* Button Show PVS+AABB */
	if (handle_checkbox(&e->checkboxes[checkbox_idx++], ctrl))
		e->controls.show_pvs = !e->controls.show_pvs;

	/* Button Skip Fragment Shader */
	if (handle_checkbox(&e->checkboxes[checkbox_idx++], ctrl))
		e->vfx_flags.skip_frag = 1;
}

void draw_menu_debug(engine_t* const e) {
	const char* str = TextFormat("player_id:%d", e->ecs.player_id);
#define LINE_MARGIN 2
	const int text_x = 4;
	DrawText(e->fb, &e->assets.font_matchup, str, text_x, LINE_MARGIN);
}
