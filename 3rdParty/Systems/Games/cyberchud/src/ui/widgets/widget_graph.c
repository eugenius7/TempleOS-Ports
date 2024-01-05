#include <assert.h>

#include "ui/widgets/widget_graph.h"
#include "utils/mymalloc.h"
#include "px.h"
#include "text.h"

static void draw_graph_line(u8* const fb, int x1, int y1, int x2, int y2, const u8 color) {
	/* Clip to Edges */
	x1 = clamp_i32(x1, 0, SCREEN_W);
	x2 = clamp_i32(x2, 0, SCREEN_W);
	y1 = clamp_i32(y1, 0, FB_H);
	y2 = clamp_i32(y2, 0, FB_H);

	const int dx = abs(x2-x1);
	const int sx = x1<x2 ? 1 : -1;
	const int dy = abs(y2-y1);
	const int sy = y1<y2 ? 1 : -1;
	int err = (dx>dy ? dx : -dy)/2;

	for(;;){
		fb[y1*SCREEN_W+x1] = color;
		if (x1==x2 && y1==y2)
			break;
		const int e2 = err;
		if (e2 >-dx) { err -= dy; x1 += sx; }
		if (e2 < dy) { err += dx; y1 += sy; }
	}
}

void push_ui_graph(ui_graph_t* const ui, const float val) {
	ui->data[ui->buf_pos] = val;
	ui->buf_pos = (ui->buf_pos+1)%UI_GRAPH_LEN;
}

void draw_ui_graph(u8* const fb, ui_graph_t* const ui, const font_t* const font, const int x, const int y) {
#define UI_GRAPH_W 256
#define UI_GRAPH_H 128
	/* find max val */
	for (int i=0; i<UI_GRAPH_LEN; i++) {
		if (ui->max_val < ui->data[i])
			ui->max_val = ui->data[i];
	}
	/* const float width = ui->aabb.x2 - ui->aabb.x1; */
	const float bottom_y = y+UI_GRAPH_H;
	const float w_step = (float)UI_GRAPH_W/UI_GRAPH_LEN;
	const float max_val_div = ui->max_val / UI_GRAPH_H;

	assert(max_val_div > 0);

	/* Initial Point */
	float x1 = x;
	int pos = ui->buf_pos;
	float y1 = bottom_y - ui->data[pos]/max_val_div;
	/* Loop Starting from 1 */
	float val;
	for (int i=0; i<UI_GRAPH_LEN-1; i++) {
		const int data_idx = (++pos)%UI_GRAPH_LEN;
		const float x2 = x1+w_step;
		val = ui->data[data_idx];
		const float y2 = bottom_y - val/max_val_div;
		draw_graph_line(fb, x1, y1, x2, y2, 13);
		x1 = x2;
		y1 = y2;
	}

	const size_t tl_idx = y*SCREEN_W+x;
	for (u8* i=&fb[tl_idx]; i<&fb[tl_idx + UI_GRAPH_W]; i++) {
		*i = 12;
	}

	const size_t bl_idx = tl_idx + UI_GRAPH_H*SCREEN_W;
	for (u8* i=&fb[bl_idx]; i<&fb[bl_idx + UI_GRAPH_W]; i++) {
		*i = 12;
	}

	const char* const str = TextFormat("max:%.3f cur:%.3f\n", ui->max_val, val);
	DrawText(fb, font, str, x, y);
}
