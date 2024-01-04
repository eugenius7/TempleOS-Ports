#ifndef UI_STATUSLINE_H
#define UI_STATUSLINE_H

#include "engine.h"

#define PAL_STATUS_BAR_BG       1
#define PAL_STATUS_BAR_FONT_ALT 14
#define PAL_STATUS_BAR_FONT     15

void draw_statusline(engine_threads_t* const et, const float delta);

static inline void statusline_reset(engine_t* const e) {
	memset(e->fb_real, PAL_STATUS_BAR_BG, SCREEN_W*8);
}

#endif
