#include "ui/menu_controls.h"
#include "ui/menu_common.h"

#define MENU_W 570
#define MENU_H 216

#define MENU_BUTTON_START_Y 64
#define MENU_BUTTON_W 128
#define MENU_BUTTON_H 32
#define UI_MARGIN 4

#define TEXTBOX_X1 (SCREEN_W/2 - MENU_W/2)
#define TEXTBOX_X2 (TEXTBOX_X1 + MENU_W)

#define TEXTBOX_Y1 64
#define TEXTBOX_Y2 (TEXTBOX_Y1 + MENU_H)

#define BUTTON_X1 (TEXTBOX_X1 + MENU_W/2 - (MENU_BUTTON_W/2))
#define BUTTON_X2 (BUTTON_X1 + MENU_BUTTON_W)
#define BUTTON_Y2 (TEXTBOX_Y2 - UI_MARGIN)
#define BUTTON_Y1 (BUTTON_Y2 - MENU_BUTTON_H)

void init_menu_controls(engine_t* const e) {
	e->ui_mode_back = e->ui_mode;
	e->ui_mode = UI_MODE_CONTROLS;
	e->button_cnt = 1;
	e->checkbox_cnt = 0;
	e->slider_cnt = 0;

	button_label_t* const ui = &e->buttons[0];
	ui->str = "Back";
	ui->rect.x1 = BUTTON_X1;
	ui->rect.x2 = BUTTON_X2;
	ui->rect.y1 = BUTTON_Y1;
	ui->rect.y2 = BUTTON_Y2;
	ui->state = BUTTON_NORMAL;
}

void handle_menu_controls(engine_t* const e) {
	if (handle_button(&e->buttons[0], &e->controls))
		menu_goback(e);
}

void draw_menu_controls(engine_t* const e) {

	for (int y=TEXTBOX_Y1; y<TEXTBOX_Y2; y++) {
		for (int x=TEXTBOX_X1; x<TEXTBOX_X2; x++) {
			e->fb[y*SCREEN_W+x] = 0;
		}
	}

	/* Draw Buttons */
	draw_button_label(e->fb, &e->buttons[0], &e->assets.font_matchup);

	DrawText(e->fb, &e->assets.font_pinzelan, "Movement = WASD\nFire = Left Mouse\nSpace = Jump\nUse/Next = F/RETURN\n1 = Shotgun\n2 = Beam Gun\n3 = SMG", TEXTBOX_X1+8, TEXTBOX_Y1+8);
	DrawText(e->fb, &e->assets.font_pinzelan, "LShift = Walk\nY = No Clip\nH = Random Palette\nI = Toggle Wireframe\nM/N = NoClip Speed", TEXTBOX_X1+300, TEXTBOX_Y1+8);
}
