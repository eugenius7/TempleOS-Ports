#include <SDL2/SDL_events.h>

#include "controls.h"
#include "text.h"
#include "stdio.h"
#include "shader.h"
#include "ecs.h"

static void soft_close(controls_t* c) {
#ifndef NDEBUG
	fputs("[soft_close]\n", stdout);
#endif
	c->fire = 0;
	if (c->mouse_grab) {
		c->mouse_grab = 0;
		c->show_menu = 1;
		c->init_menu = 1;
		SDL_SetRelativeMouseMode(SDL_FALSE);
	} else {
		/* c->close = 1; */
	}
}

void input(controls_t* const c) {
#ifdef VERBOSE
	fputs("[INPUT]\n", stdout);
#endif

	/* Reset */
	c->mouse.x = 0;
	c->mouse.y = 0;
	c->weapon_switch = -1;
	c->mouse_lb_pressed = 0;
	c->mouse_fullclick = 0;
	c->jumpPressed = 0;
	c->enterPressed = 0;

	/* Pump Events */
	SDL_Event event;
#ifdef TOSLIKE
	SDL_PumpEvents();
#endif
#if !defined(__EMSCRIPTEN__) && !defined(TOSLIKE)
	#define SCALE_MOUSE_X(...) (__VA_ARGS__)*((float)SCREEN_W/c->win_size.x)
	#define SCALE_MOUSE_Y(...) (__VA_ARGS__)*((float)SCREEN_H/c->win_size.y)
#else
	#define SCALE_MOUSE_X(...) (__VA_ARGS__)
	#define SCALE_MOUSE_Y(...) (__VA_ARGS__)
#endif

	while (SDL_PollEvent(&event)) {
		/* printf("event++ type:%u char:%u %c\n", event.type, event.key.keysym.sym, event.key.keysym.sym); */
		switch (event.type) {
		case SDL_QUIT:
#ifndef NDEBUG
			fputs("SDL_QUIT\n", stdout);
#endif
			c->close = 1;
			break;
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					c->win_size.x = event.window.data1;
					c->win_size.y = event.window.data2;
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			/* myprintf("%dx%d %dx%d\n", event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel); */
			// @eb-lan(github): we don't scale on {x,y}rel
			// since we want don't want the mouse motion to slow down
			c->mouse.x += event.motion.xrel;
			c->mouse.y += event.motion.yrel;
			c->mouse_abs = (vec2_i16) {
				.x = SCALE_MOUSE_X(event.motion.x),
				.y = SCALE_MOUSE_Y(event.motion.y),
			};
			c->mouse_abs_ui = (vec2_i16) {
				.x = SCALE_MOUSE_X(event.motion.x),
				.y = SCALE_MOUSE_Y(event.motion.y-STATUSLINE_H),
			};
			//
			break;
		case SDL_MOUSEBUTTONDOWN:
			c->mouse_lb_pressed = !c->mouse_lb_down;
			c->mouse_lb_down = 1;
			if (!c->show_menu) {
				if (!c->mouse_grab) {
#ifndef TOSLIKE
					c->mouse_grab = 1;
					SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
				} else {
					c->fire = 1;
				}
			}
			break;
		case SDL_MOUSEBUTTONUP:
			c->mouse_lb_down = 0;
			c->fire = 0;
			if (c->mouse_lb_pressed)
				c->mouse_fullclick = 1;
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_W:
			case SDL_SCANCODE_UP:
				c->forwardDown = 1;
				break;
			case SDL_SCANCODE_A:
			case SDL_SCANCODE_LEFT:
				c->leftDown = 1;
				break;
			case SDL_SCANCODE_S:
			case SDL_SCANCODE_DOWN:
				c->backwardDown = 1;
				break;
			case SDL_SCANCODE_D:
			case SDL_SCANCODE_RIGHT:
				c->rightDown = 1;
				break;
			case SDL_SCANCODE_Q:
				c->downDown = 1;
				break;
			case SDL_SCANCODE_E:
				c->upDown = 1;
				break;
			case SDL_SCANCODE_LSHIFT:
				c->walk = 1;
				break;
			case SDL_SCANCODE_SPACE:
				c->jumpPressed = 1;
				c->jumpDown = 1;
				break;
			case SDL_SCANCODE_F:
			case SDL_SCANCODE_RETURN:
				c->enterPressed = 1;
				c->enterDown = 1;
				break;
			case SDL_SCANCODE_Y:
				c->noclip = !c->noclip;
				break;
			case SDL_SCANCODE_U:
				c->show_debug = !c->show_debug;
				break;
			case SDL_SCANCODE_I:
				c->show_wireframe = !c->show_wireframe;
				break;
			case SDL_SCANCODE_H:
				c->randomize_palette = 1;
				break;
			case SDL_SCANCODE_N:
				c->fly_speed -= 0.1;
				if (c->fly_speed < 0.1) c->fly_speed = 0.1;
				break;
			case SDL_SCANCODE_M:
				c->test_toggle = 1;
				c->fly_speed += 0.1;
				break;
			case SDL_SCANCODE_1:
				c->weapon_switch = WEAPON_SHOTGUN;
				break;
			case SDL_SCANCODE_2:
				c->weapon_switch = WEAPON_BEAM;
				break;
			case SDL_SCANCODE_3:
				c->weapon_switch = WEAPON_SMG;
				break;
			case SDL_SCANCODE_0:
				c->weapon_switch = WEAPON_NONE;
				break;
			case SDL_SCANCODE_ESCAPE:
				soft_close(c);
				break;
			default:
				break;
			}
			break;

		case SDL_KEYUP:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_W:
			case SDL_SCANCODE_UP:
				c->forwardDown = 0;
				break;
			case SDL_SCANCODE_A:
			case SDL_SCANCODE_LEFT:
				c->leftDown = 0;
				break;
			case SDL_SCANCODE_S:
			case SDL_SCANCODE_DOWN:
				c->backwardDown = 0;
				break;
			case SDL_SCANCODE_D:
			case SDL_SCANCODE_RIGHT:
				c->rightDown = 0;
				break;
			case SDL_SCANCODE_Q:
				c->downDown = 0;
				break;
			case SDL_SCANCODE_E:
				c->upDown = 0;
				break;
			case SDL_SCANCODE_LSHIFT:
				c->walk = 0;
				break;
			case SDL_SCANCODE_SPACE:
				c->jumpDown = 0;
				break;
			case SDL_SCANCODE_RETURN:
				c->enterDown = 0;
				break;
			default:
				break;
			}
		default:
			break;
		}
	}
}
