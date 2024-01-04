#include <assert.h>
#include <stdalign.h>

#include <SDL2/SDL.h>

#include "game.h"
#include "render.h"
#include "engine.h"
#include "playfield.h"
#include "sound.h"
#include "text.h"
#include "worker.h"
#include "scene/title.h"
#include "scene/playfield.h"
#include "utils/minmax.h"

engine_threads_t* ge;

#if 0
#include "palette.h"
static const palette_t palette = {
	(palette_color_t){.hex=0xff000000}, // TEAR COLOR
	(palette_color_t){.hex=0xff1a0000}, // STATUS BAR BACKGROUND

	(palette_color_t){.hex=0xff330000},
	(palette_color_t){.hex=0xff4d0000},
	(palette_color_t){.hex=0xff660000},
	(palette_color_t){.hex=0xff800000},
	(palette_color_t){.hex=0xff990000},
	(palette_color_t){.hex=0xffb30000},
	(palette_color_t){.hex=0xffcc0000},
	(palette_color_t){.hex=0xffe60000},
	(palette_color_t){.hex=0xffff0000},

	(palette_color_t){.hex=0xffff8400},
	(palette_color_t){.hex=0xffff9f00},
	(palette_color_t){.hex=0xffffba00},
	(palette_color_t){.hex=0xff55ff22}, // STATUS BAR FONT ALT-COLOR
	(palette_color_t){.hex=0xffffffff}, // STATUS BAR FONT COLOR
};
#endif

#ifdef __EMSCRIPTEN__
#include <unistd.h>
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE void window_focus_callback() {
#ifndef NDEBUG
	myprintf("[window_focus_callback]\n");
#endif
	/* web browsers wont let us open an audio device until the user interacts with the page */
	engine_t* const e = &ge->e;
	e->audio.spec.freq  = SND_FREQ;
	e->audio.spec.format = AUDIO_S8;
	e->audio.spec.channels = 1;
	e->audio.spec.samples = SND_SAMPLES;
	e->audio.spec.callback = snd_callback;
	e->audio.spec.userdata = e;
	const SDL_AudioDeviceID devid = SDL_OpenAudioDevice(NULL, 0, &e->audio.spec, NULL, 0);
	SDL_PauseAudioDevice(devid, 0);
}
#endif

#ifdef TOSLIKE
#include "tosfb.h"
	static void trigger_tosfb(tosfb_thread_t* const tosfb) {
		LockMutex(tosfb->mutex);
		tosfb->draw = 1;
		CondSignal(tosfb->mu_cond);
		UnlockMutex(tosfb->mutex);
	}
#endif

static inline SDL_Surface* get_winsurf(SDL_Window* win) {
	SDL_Surface* const surf = SDL_GetWindowSurface(win);
#ifndef NDEBUG
	if (!surf)
		fprintf(stderr, "no surface! %p %s\n", win, SDL_GetError());
#endif
	return surf;
}

void DLL_PUBLIC loop(void) {
#ifdef VERBOSE
	fputs("[LOOP]\n", stdout);
#endif
	engine_threads_t* const et = ge;
	engine_t* const e = &et->e;
	e->ticks_last = e->ticks_cur;
	e->ticks_cur = get_time();
	float delta = time_diff(e->ticks_last, e->ticks_cur);
	if (delta > 1.0f) delta = 1.0f;

#define TICK_TIME 0.01666666666f
	e->ticks_accum += delta;
	const int ticks = e->ticks_accum/TICK_TIME;
	e->ticks_accum -= ticks*TICK_TIME;

	for (int i=0; i<ticks; i++) {
		if (e->switch_scene) {
			e->switch_scene = 0;
			engine_cfg_update(e);
			switch (e->scene) {
				case SCENE_TITLE:
					title_init(e);
					break;
				case SCENE_PLAYFIELD:
					playfield_init(e);
					break;
			}
		}

		input(&e->controls);

		switch (e->scene) {
			case SCENE_TITLE:
				title_update(e, TICK_TIME);
				break;
			case SCENE_PLAYFIELD:
				playfield_update(e, TICK_TIME);
				break;
		}
		e->scene_time += TICK_TIME;
	}

	switch (e->scene) {
		case SCENE_TITLE:
			title_draw(et, delta);
			break;
		case SCENE_PLAYFIELD:
			playfield_draw(et, delta);
			break;
	}

#if defined(HW_ACCEL) && !defined(TOSLIKE)
	/* Hardware Path */
	SDL_Rect rect = {.x=0, .y=0, .w=SCREEN_W, .h=SCREEN_H};
	u8* const internal_fb = e->fb_real;
	const int interlace = e->interlace;
	for (int y=0; y<SCREEN_H; y++) {
		if ((y+interlace) % 2) continue;
		for (int x=0; x<SCREEN_W; x++) {
			const u8 color = internal_fb[y*SCREEN_W+x];
			e->fb_swiz[y*SCREEN_W+x] = e->palette.colors[color].rgba;
		}
	}
	SDL_UpdateTexture(e->sdl_tex, &rect, e->fb_swiz, SCREEN_W*4);
	SDL_RenderCopy(e->sdl_render, e->sdl_tex, NULL, NULL);
	SDL_RenderPresent(e->sdl_render);
	/* 60 FPS */
	/* i32 delay = 16.66666666666f-delta; */
	/* if (delay > 0) { */
	/* 	SDL_Delay(delay); */
	/* }; */
#else
	/* Software Path */

#ifdef TEMPLEOS
	memcpy(et->tosfb_thr.fb, e->fb_real, SCREEN_W*SCREEN_H);
	trigger_tosfb(&et->tosfb_thr);
#endif

#ifdef ZEALOS
	SDL_Surface *winSurface = get_winsurf(e->win);
	uint8_t* fb = winSurface->pixels;
	const int win_w = winSurface->w;
	for (int y=0; y<SCREEN_H; y++) {
		for (int x=0; x<SCREEN_W; x++) {
			fb[y*win_w+x] = e->fb_real[y*SCREEN_W+x];
		}
	}
	SDL_UpdateWindowSurface(e->win);
#endif

#ifdef ZEALOS
	SDL_Surface *winSurface = get_winsurf(e->win);
	uint8_t* fb = winSurface->pixels;
	const int win_w = winSurface->w;
	for (int y=0; y<SCREEN_H; y++) {
		for (int x=0; x<SCREEN_W; x++) {
			fb[y*win_w+x] = e->fb_real[y*SCREEN_W+x];
		}
	}
	SDL_UpdateWindowSurface(e->win);
	/* 60 FPS */
	i32 delay = 16.66666666666f-delta;
	if (delay > 0) {
		SDL_Delay(delay);
	};
#endif

#if defined(PLATFORM_DESKTOP) || defined(__EMSCRIPTEN__)
	SDL_Surface *winSurface = get_winsurf(e->win);
	rgba_t* fb = (rgba_t*)winSurface->pixels;
	const int win_w = winSurface->w;
	for (int y=0; y<SCREEN_H; y++) {
		for (int x=0; x<SCREEN_W; x++) {
			const u8 color = e->fb_real[y*SCREEN_W+x];
			assert(color < PALETTE_SIZE);
#ifdef __EMSCRIPTEN__
			const u8* const swiz = (const u8*)&e->palette.colors[color];
			u8* const out = (u8*)&fb[y*win_w+x];
			out[0] = swiz[2];
			out[1] = swiz[1];
			out[2] = swiz[0];
			out[3] = swiz[3];
#else
			fb[y*win_w+x] = e->palette.colors[color].rgba;
#endif /* __EMSCRIPTEN__ */
		}
	}
	SDL_UpdateWindowSurface(e->win);

	/* Software Path Delay */
	/* i32 delay = 16.66666666666f-delta; */
	/* if (delay > 0) { */
	/* 	SDL_Delay(delay); */
	/* }; */
#endif /* defined(PLATFORM_DESKTOP) || defined(__EMSCRIPTEN__) */
#endif /* End Software Path */

	e->interlace = !e->interlace;
}

int check_if_quitting(void) {
	return ge->e.controls.close;
}

int DLL_PUBLIC init(init_cfg_t cfg) {
#ifndef NDEBUG
	fputs("[INIT]\n", stdout);

#ifdef HW_ACCEL
	fputs("[INIT] HW_ACCEL enabled\n", stdout);
#else
	fputs("[INIT] HW_ACCEL disabled\n", stdout);
#endif
#endif

	/* Init SDL */
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_AUDIO) != 0) {
		/* fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError()); */
		return 1;
	}
#ifndef NDEBUG
	fputs("[INIT] SDL Initialized\n", stdout);
#endif

	/* Count CPUs */
#ifndef __EMSCRIPTEN__
	/* I've got reports that over 16 cores is a problem??? */
	const int cpu_cnt = MIN(SDL_GetCPUCount(), 16);
#else
	const int cpu_cnt = MIN(sysconf(_SC_NPROCESSORS_ONLN), 8);
	/* low settings by default for browsers */
	cfg.no_pointlights = 1;
#endif
	if (cpu_cnt <= 0) {
		myprintf("[ERR] not enough cores, need 4, have: %d\n", cpu_cnt);
		return 1;
	}

	/* Allocate Engine */
	ge = engine_alloc(cpu_cnt);
	if (ge == NULL)
		return 1;

	/* Init Engine */
	engine_t* const e = &ge->e;
	e->controls.win_size.x = SCREEN_W;
	e->controls.win_size.x = SCREEN_H;
	e->fb = &e->fb_real[SCREEN_W*STATUSLINE_H];
	e->switch_scene = 1;
	e->scene = SCENE_TITLE;
	e->seed.a = 42;
	e->max_pointlights = 2;
	e->max_shadowcasters = 2;
	e->flags.pointlight_shadows_enabled = !cfg.no_pointlights;
	e->flags.shadowcaster_shadows_enabled = !cfg.no_pointlights;

	/* Init Cubemaps */
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++)
		cubemap_init(&e->pointlights.cubemaps[i]);
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++)
		cubemap_init(&e->shadowcasters.cubemaps[i].basic);

	/* Init Palette */
	/* I like this palette */
	e->seed.a = 4055765075478523762lu;
	palette_randomize(&e->palette_base, &e->seed);
	e->palette = e->palette_base;

	/* Init ECS */
	ecs_init(&e->ecs);

	/* Create Window */
	e->win = SDL_CreateWindow("CyberChud", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_W, SCREEN_H,
#if !defined(__EMSCRIPTEN__) && !defined(TOSLIKE)
		SDL_WINDOW_RESIZABLE
#else
		SDL_WINDOW_SHOWN
#endif
	);
	if (e->win == NULL) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		return 1;
	}
#ifndef NDEBUG
	myprintf("[INIT] SDL Window Created: %lx\n", e->win);
#endif

	/* Setup Framebuffer*/
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_DESKTOP)
#ifdef HW_ACCEL
	e->sdl_render = SDL_CreateRenderer(e->win, -1, SDL_RENDERER_ACCELERATED);
	e->sdl_tex = SDL_CreateTexture(e->sdl_render, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
#else
	SDL_Surface* surface = SDL_GetWindowSurface(e->win);
	uint8_t* fb = surface->pixels;
/* #ifdef ZEALOS */
/* 	SDL_UpdateWindowSurface(e->win); */
/* #endif */
	memset(fb, 0, surface->w*surface->h*sizeof(rgba_t));
	SDL_UpdateWindowSurface(e->win);
#endif
#endif

#ifdef TOSLIKE
	/* Init Special TOSLIKE Sound System (BEFORE THREADS!) */
	tos_sound_init(e);
#endif

	/* Start Threads */
	/* we do this before tosfb because I don't want tosfb in core 0 */
	if (thread_init(ge)) {
		fputs("[err] thread_init failed\n", stderr);
		return 1;
	}

#ifdef TEMPLEOS
	tosfb_init(&ge->tosfb_thr, e, &e->palette);
#endif

	/* Load Assets */
	if (assets_init(&e->assets, &e->ecs.alloc)) {
		fputs("[INIT] FATAL\n", stdout);
		return 1;
	}
#ifndef NDEBUG
	fputs("[INIT] load done\n", stdout);
#endif

	/* Init Audio */
	/* web browsers wont let us open an audio device until the user interacts with the page */
	e->audio.spec.freq  = SND_FREQ;
	e->audio.spec.format = AUDIO_S8;
	e->audio.spec.channels = 1;
	e->audio.spec.samples = SND_SAMPLES;
#if !defined(__EMSCRIPTEN__) && !defined(TOSLIKE)
	e->audio.spec.callback = snd_callback;
	e->audio.spec.userdata = e;
	const SDL_AudioDeviceID devid = SDL_OpenAudioDevice(NULL, 0, &e->audio.spec, NULL, 0);
	SDL_PauseAudioDevice(devid, 0);
#ifndef NDEBUG
	myprintf("[AUDIO] freq:%d format:%u ch:%u silence:%u samples:%u padding:%u size:%u\n", e->audio.spec.freq, e->audio.spec.format, e->audio.spec.channels, e->audio.spec.silence, e->audio.spec.samples, e->audio.spec.padding, e->audio.spec.size);
#endif
#endif

#ifndef __EMSCRIPTEN__
	if (SDL_SetRelativeMouseMode(SDL_TRUE) == -1) {
		fputs("[INIT] SDL_SetRelativeMouseMode unsupported\n", stderr);
	}
	e->controls.mouse_grab = 1;
#endif
#ifndef NDEBUG
	fputs("[INIT] done\n", stdout);
#endif

	/* Disable OS Cursor */
#ifndef TOSLIKE
	SDL_ShowCursor(SDL_DISABLE);
#endif

	e->ticks_cur = get_time();
	e->ticks_accum = TICK_TIME;
	return 0;
}

int DLL_PUBLIC quit(void) {
	engine_threads_t* const et = ge;
	engine_t* const e = &et->e;
	/* Signal Threads */
#ifndef NDEBUG
	fputs("[quit] locking thread mutex\n", stdout);
#endif
	LockMutex(et->jobs[0].mutex);
#ifndef NDEBUG
	fputs("[quit] waiting for threads finish jobs\n", stdout);
#endif
	const i8 cpu_cnt = e->cpu_cnt;
	while (e->waiting_thread_cnt != cpu_cnt) {
		CondWait(et->jobs[0].done_mu_cond, et->jobs[0].mutex);
	}
	e->jobs_queued = cpu_cnt;
	for (i32 i=0; i<cpu_cnt; i++)
		et->jobs[i].state = JOB_STATE_QUIT;
#ifndef NDEBUG
	fputs("[quit] broadcast quit signal to threads\n", stdout);
#endif
	CondBroadcast(et->jobs[0].mu_cond);
#ifndef NDEBUG
	fputs("[quit] unlocking thread mutex\n", stdout);
#endif
	UnlockMutex(et->jobs[0].mutex);
#ifndef NDEBUG
	fputs("[quit] unlocked thread mutex\n", stdout);
#endif

	/* Wait for Threads */
	for (int i=0; i<cpu_cnt; i++) {
		int status;
#ifndef NDEBUG
		myprintf("[quit] WaitThread [%d]\n", i);
#endif
		WaitThread(et->jobs[i].thread, &status);
#ifndef NDEBUG
		myprintf("[quit] WaitThread [%d] done:%d\n", i, status);
#endif
	}

#ifdef TEMPLEOS
	/* Stop Render Thread */
#ifndef NDEBUG
	fputs("[quit] stopping render thread\n", stdout);
#endif
	et->tosfb_thr.quit = 1;
	trigger_tosfb(&et->tosfb_thr);
#endif

	engine_free(et);

	SDL_Quit();
#ifndef NDEBUG
	fputs("[quit] done\n", stdout);
#endif
	return 0;
}
