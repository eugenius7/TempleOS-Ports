#include "ui/menu_common.h"
#include "ui/menu_controls.h"
#include "ui/menu_cubemap.h"
#include "ui/menu_debug.h"
#include "ui/menu_options.h"
#include "ui/menu_pause.h"
#include "ui/menu_title.h"

void menu_goback(engine_t* const e) {
	switch (e->ui_mode_back) {
		case UI_MODE_CONTROLS:
			init_menu_controls(e);
			break;
		case UI_MODE_CUBEMAPS:
			init_menu_cubemap(e);
			break;
		case UI_MODE_OPTIONS:
			init_menu_options(e);
			break;
		case UI_MODE_DEBUG:
			init_menu_debug(e);
			break;
		case UI_MODE_TITLE:
			init_menu_title(e);
			break;
		case UI_MODE_PAUSE:
			init_menu_pause(e);
			break;
		case UI_MODE_NONE:
			break;
	}
}

void draw_menu_common(engine_t* const e) {
	/* Draw Buttons */
	for (int i=0; i<e->button_cnt; i++)
		draw_button_label(e->fb, &e->buttons[i], &e->assets.font_matchup);

	/* Draw Checkboxes */
	for (int i=0; i<e->checkbox_cnt; i++)
		draw_checkbox(e->fb, &e->checkboxes[i], &e->assets);

	/* Draw Sliders */
	for (int i=0; i<e->slider_cnt; i++)
		draw_slider(e->fb, &e->sliders[i], &e->assets);
}
