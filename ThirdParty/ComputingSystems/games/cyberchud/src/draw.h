#ifndef DRAW_H
#define DRAW_H

#include "vec.h"

void draw_line_mask(uint8_t* fb, rect_i32 line, const rect_i32 mask, uint8_t color);

static inline void draw_line(uint8_t* fb, int x0, int y0, int x1, int y1, uint8_t color) {
	const rect_i32 mask = {0, 0, SCREEN_W, FB_H};
	const rect_i32 line = {x0, y0, x1, y1};
	draw_line_mask(fb, line, mask, color);
}

#endif
