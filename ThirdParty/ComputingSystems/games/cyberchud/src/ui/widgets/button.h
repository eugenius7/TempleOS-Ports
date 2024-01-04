#ifndef WIDGETS_BUTTON_H
#define WIDGETS_BUTTON_H

#include "assets.h"

#define BUTTON_NORMAL   0
#define BUTTON_HOVER    1
#define BUTTON_DOWN     2
#define BUTTON_CLICKED  3
#define BUTTON_DISABLED 4

typedef struct {
	const char* str;
	rect_i16 rect;
	i8 state;
} button_label_t;

typedef struct {
	const char* str;
	vec2_i16 pos;
	i8 state;
	i8 checked;
} checkbox_t;

#define SLIDER_STR_LEN 8
typedef struct {
	vec2_i16 pos;
	vec2_i16 pos_bit;
	i16 val;
	i16 max;
	i8 state;
	char str[SLIDER_STR_LEN];
	const px_t* slider_px;
} slider_t;

void draw_button_label(u8* const fb, const button_label_t* const ui, const font_t* const font);
bool handle_button(button_label_t* const ui, const controls_t* const ctrl);

void draw_checkbox(u8* const fb, const checkbox_t* const ui, const assets_t* const assets);
bool handle_checkbox(checkbox_t* const ui, const controls_t* const ctrl);

void draw_slider(u8* const fb, slider_t* const slider, const assets_t* const assets);
void handle_slider(slider_t* const slider, const controls_t* const ctrl, const assets_t* const assets);

void slider_init(slider_t* const slider, const assets_t* assets, const i16 x, const i16 y, const i16 val, const i16 max);
void slider_set(slider_t* const slider, const i16 val, const i16 max);

#endif
