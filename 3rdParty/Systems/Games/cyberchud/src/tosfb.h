#ifndef TOSFB_H
#define TOSFB_H

#include "engine.h"

void tosfb_init(tosfb_thread_t* thr, engine_t* const e, const palette_t* const palette);
void tos_get_statusline(char* str);

void tos_sound_init(engine_t* const e);
void tos_sound_update(engine_t* const e);

/* extern i32 CondWaitCallback(SDL_cond* cond, SDL_mutex* mu, void* callback); */

#endif
