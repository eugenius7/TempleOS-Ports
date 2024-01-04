#include "draw.h"

void draw_line_mask(uint8_t* fb, rect_i32 line, const rect_i32 mask, uint8_t color) {
	/* Clip Offscreen */
	if (
			(line.x1<mask.x1 && line.x2<mask.x1) ||
			(line.x1>=mask.x2 && line.x2>=mask.x2) ||
			(line.y1<mask.y1 && line.y2<mask.y1) ||
			(line.y1>=mask.y2 && line.y2>=mask.y2))
		return;

	/* Clip to Edges */
	line.x1 = clamp_i32(line.x1, 0, SCREEN_W);
	line.x2 = clamp_i32(line.x2, 0, SCREEN_W);
	line.y1 = clamp_i32(line.y1, 0, FB_H);
	line.y2 = clamp_i32(line.y2, 0, FB_H);

	const int32_t dx = abs(line.x2-line.x1);
	const int32_t sx = line.x1<line.x2 ? 1 : -1;
	const int32_t dy = abs(line.y2-line.y1);
	const int32_t sy = line.y1<line.y2 ? 1 : -1;
	int32_t err = (dx>dy ? dx : -dy)/2;

	for(;;){
		if (line.x1 >= mask.x1 && line.x1 < mask.x2 && line.y1 >= mask.y1 && line.y1 < mask.y2)
			fb[line.y1*SCREEN_W+line.x1] = color;
		if (line.x1==line.x2 && line.y1==line.y2)
			break;
		const int32_t e2 = err;
		if (e2 >-dx) { err -= dy; line.x1 += sx; }
		if (e2 < dy) { err += dx; line.y1 += sy; }
	}
}
