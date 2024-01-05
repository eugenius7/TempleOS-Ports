#ifndef CONTROLS_H
#define CONTROLS_H

#include "vec.h"

typedef struct {
	float fly_speed;
	vec2_i16 win_size;
	vec2_i16 mouse;
	vec2_i16 mouse_abs;
	vec2_i16 mouse_abs_ui;
	i8 weapon_switch;
	unsigned int mouse_lb_down : 1;
	unsigned int mouse_lb_pressed : 1;
	unsigned int mouse_fullclick: 1; // when we get a down and up in a single frame
	unsigned int mouse_grab : 1;
	unsigned int close : 1;
	unsigned int forwardDown : 1;
	unsigned int backwardDown : 1;
	unsigned int leftDown : 1;
	unsigned int rightDown : 1;
	unsigned int upDown : 1;
	unsigned int downDown : 1;
	unsigned int jumpDown : 1;
	unsigned int jumpPressed : 1;
	unsigned int enterDown: 1;
	unsigned int enterPressed: 1;
	unsigned int walk : 1;
	unsigned int noclip : 1;
	unsigned int fire : 1;
	unsigned int show_menu: 1;
	unsigned int show_debug : 1;
	unsigned int show_wireframe : 1;
	unsigned int show_pvs : 1;
	unsigned int init_menu : 1;
	unsigned int update_palette: 1;
	unsigned int randomize_palette : 1;
	unsigned int test_toggle : 1;
} controls_t;

void input(controls_t* const c);

#endif
