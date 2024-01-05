#include "ui/widgets/button.h"
#include "px.h"
#include "stb_sprintf.h"

void draw_button_label(u8* const fb, const button_label_t* const ui, const font_t* const font) {
	const u8 BUTTON_STATE_COLORS[4] = {12, 13, 14, 15};
	const u8 color = BUTTON_STATE_COLORS[ui->state];
	for (int y=ui->rect.y1; y<ui->rect.y2; y++) {
		for (int x=ui->rect.x1; x<ui->rect.x2; x++) {
			fb[y*SCREEN_W+x] = color;
		}
	}
	/* TODO cache */
	vec2_i16 label_size = measure_text(font, ui->str);
	label_size.x /= 2;
	label_size.y /= 2;
	int cx = ui->rect.x1 + (ui->rect.x2 - ui->rect.x1)/2 - label_size.x;
	int cy = ui->rect.y1 + (ui->rect.y2 - ui->rect.y1)/2 - label_size.y;
	DrawText(fb, font, ui->str, cx, cy);
}

bool handle_button(button_label_t* const ui, const controls_t* const ctrl) {
	if (ui->state == BUTTON_DISABLED)
		return false;

	if (collide_aabb_point(ui->rect, ctrl->mouse_abs_ui)) {
		if (ui->state == BUTTON_NORMAL)
			ui->state = BUTTON_HOVER;
		if (ctrl->mouse_fullclick) {
			ui->state = BUTTON_CLICKED;
			return true;
		} else if (ctrl->mouse_lb_down) {
			if (ctrl->mouse_lb_pressed) {
				ui->state = BUTTON_DOWN;
			}
		} else if (ui->state == BUTTON_DOWN) {
			ui->state = BUTTON_CLICKED;
			return true;
		} else {
			ui->state = BUTTON_HOVER;
		}
	} else {
		ui->state = BUTTON_NORMAL;
	}
	return false;
}

void draw_checkbox(u8* const fb, const checkbox_t* const ui, const assets_t* const assets) {
	const px_t* px;
	if (ui->checked) {
		px = assets->px_pal[PX_PAL_CHECKBOX_TICKED];
	} else {
		px = assets->px_pal[PX_PAL_CHECKBOX_UNTICKED];
	}
	draw_px_pal_offset(fb, px, ui->pos.x, ui->pos.y, ui->state%4);
	DrawText(fb, &assets->font_matchup, ui->str, ui->pos.x+18, ui->pos.y+2);
}

bool handle_checkbox(checkbox_t* const ui, const controls_t* const ctrl) {
	if (ui->state == BUTTON_DISABLED)
		return false;

	rect_i16 aabb = {ui->pos.x, ui->pos.y, ui->pos.x+16, ui->pos.y+16};
	if (collide_aabb_point(aabb, ctrl->mouse_abs_ui)) {
		if (ui->state == BUTTON_NORMAL)
			ui->state = BUTTON_HOVER;
		if (ctrl->mouse_fullclick) {
			ui->state = BUTTON_CLICKED;
			ui->checked = !ui->checked;
			return true;
		} else if (ctrl->mouse_lb_down) {
			if (ctrl->mouse_lb_pressed) {
				ui->state = BUTTON_DOWN;
			}
		} else if (ui->state == BUTTON_DOWN) {
			ui->state = BUTTON_CLICKED;
			ui->checked = !ui->checked;
			return true;
		} else {
			ui->state = BUTTON_HOVER;
		}
	} else {
		ui->state = BUTTON_NORMAL;
	}
	return false;
}

#define SLIDER_W 128
#define SLIDER_H 16
#define SLIDER_BIT_W 16
#define SLIDER_BIT_H 16
#define SLIDER_PX_W 137
#define SLIDER_PX_X_OFFSET 4

void draw_slider(u8* const fb, slider_t* const slider, const assets_t* const assets) {
	draw_px_pal_offset(fb, assets->px_pal[PX_PAL_SLIDERBAR], slider->pos.x-SLIDER_PX_X_OFFSET, slider->pos.y, slider->state%3);
	draw_px(fb, slider->slider_px, slider->pos_bit.x, slider->pos_bit.y);
	DrawText(fb, &assets->font_matchup, slider->str, slider->pos.x+SLIDER_W+4, slider->pos.y-3);
}

void handle_slider(slider_t* const slider, const controls_t* const ctrl, const assets_t* const assets) {
	const rect_i16 aabb_bit = {slider->pos_bit.x, slider->pos_bit.y, slider->pos_bit.x+SLIDER_BIT_W, slider->pos_bit.y+SLIDER_BIT_H};
	if (slider->state == BUTTON_DOWN) {
		if (ctrl->mouse_lb_down) {
			slider->pos_bit.x = ctrl->mouse_abs_ui.x-SLIDER_BIT_W/2;
			if (slider->pos_bit.x < slider->pos.x)
				slider->pos_bit.x = slider->pos.x;
			else if (slider->pos_bit.x > slider->pos.x+SLIDER_W-SLIDER_BIT_W+1)
				slider->pos_bit.x = slider->pos.x+SLIDER_W-SLIDER_BIT_W+1;
			const float bitInc = (float)(SLIDER_W-SLIDER_BIT_W) / slider->max;
			slider->val = (float)(slider->pos_bit.x-slider->pos.x) / bitInc;
			stbsp_snprintf(slider->str, SLIDER_STR_LEN, "%d/%d", slider->val, slider->max);
		} else {
			slider->state = BUTTON_NORMAL;
		}
	} else if (collide_aabb_point(aabb_bit, ctrl->mouse_abs_ui)) {
		slider->slider_px = assets->px_pal[PX_PAL_CHECKBOX_TICKED];
		if (ctrl->mouse_lb_down) {
			slider->state = BUTTON_DOWN;
		}
	} else {
		const rect_i16 aabb = {slider->pos.x-SLIDER_PX_X_OFFSET, slider->pos.y, slider->pos.x+SLIDER_PX_W, slider->pos.y+SLIDER_H};
		if (collide_aabb_point(aabb, ctrl->mouse_abs_ui)) {
			slider->state = BUTTON_HOVER;
		} else {
			slider->state = BUTTON_NORMAL;
		}
		slider->slider_px = assets->px_pal[PX_PAL_CHECKBOX_UNTICKED];
	}
}

void slider_set(slider_t* const slider, const i16 val, const i16 max) {
	slider->val = val;
	slider->max = max;
	const float bitInc = (float)(SLIDER_W-SLIDER_BIT_W) / slider->max;
	slider->pos_bit.x = slider->pos.x + val*bitInc;
	slider->pos_bit.y = slider->pos.y;
	stbsp_snprintf(slider->str, SLIDER_STR_LEN, "%d/%d", slider->val, slider->max);
}

void slider_init(slider_t* const slider, const assets_t* assets, const i16 x, const i16 y, const i16 val, const i16 max) {
	slider->pos.x = x;
	slider->pos.y = y;
	slider->state = BUTTON_NORMAL;
	slider->slider_px = assets->px_pal[PX_PAL_CHECKBOX_UNTICKED];
	slider_set(slider, val, max);
}
