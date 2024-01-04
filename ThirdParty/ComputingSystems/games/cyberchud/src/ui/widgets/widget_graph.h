#ifndef UI_WIDGET_GRAPH_H
#define UI_WIDGET_GRAPH_H

#include "px.h"

#define UI_GRAPH_LEN 64
typedef struct {
	/* rect_i16 aabb; */
	int buf_pos;
	float max_val;
	float data[UI_GRAPH_LEN];
} ui_graph_t;

void draw_ui_graph(u8* const fb, ui_graph_t* const ui, const font_t* const font, const int x, const int y);
void push_ui_graph(ui_graph_t* const ui, const float val);

#endif
