#include "ui/statusline.h"
#include "ui/widgets/tos_text.h"
#include "stb_sprintf.h"

#ifdef TOSLIKE
#include "tosfb.h"
#endif

void draw_statusline(engine_threads_t* const et, const float delta) {
	engine_t* const e = &et->e;
	/* memset(e->fb_real, PAL_STATUS_BAR_BG, SCREEN_W*8); */
	char status_line[96];
	char* sptr = &status_line[stbsp_sprintf(status_line, "FPS:%03.0f ", 1.0f/delta)];
#ifdef TOSLIKE
	tos_get_statusline(sptr);
#endif
	draw_tos_text(e->fb_real, status_line, 0, 0, SCREEN_W, STATUSLINE_H);
}
