#include "vfx/vfx_wireframe.h"
#include "draw.h"
#include "utils/myds.h"

void vfx_wireframe(engine_threads_t* const et, const float delta) {
	engine_t* const e = &et->e;
	const i32 max_threads = e->cpu_cnt;
	const rect_i32 lmask = {0, 0, SCREEN_W, FB_H};
	const uint8_t color = 15;
	for (i32 i=0; i<max_threads; i++) {
		/* Draw World Wireframes */
		for (size_t j=0; j<myarrlenu(et->jobs[i].vert_out_data.tris.wfrags.frags); j++) {
			const vec2s* const tri = et->jobs[i].vert_out_data.tris.wfrags.frags[j].pts2;
			rect_i32 line = {tri[0].x, tri[0].y, tri[1].x, tri[1].y};
			draw_line_mask(e->fb, line, lmask, color);
			line = (rect_i32){tri[0].x, tri[0].y, tri[2].x, tri[2].y};
			draw_line_mask(e->fb, line, lmask, color);
			line = (rect_i32){tri[1].x, tri[1].y, tri[2].x, tri[2].y};
			draw_line_mask(e->fb, line, lmask, color);
		}
		/* Draw Model Wireframes */
		for (size_t j=0; j<myarrlenu(et->jobs[i].vert_out_data.tris.mfrags.frags); j++) {
			const vec2s* const tri = et->jobs[i].vert_out_data.tris.mfrags.frags[j].pts2;
			rect_i32 line = {tri[0].x, tri[0].y, tri[1].x, tri[1].y};
			draw_line_mask(e->fb, line, lmask, color);
			line = (rect_i32){tri[0].x, tri[0].y, tri[2].x, tri[2].y};
			draw_line_mask(e->fb, line, lmask, color);
			line = (rect_i32){tri[1].x, tri[1].y, tri[2].x, tri[2].y};
			draw_line_mask(e->fb, line, lmask, color);
		}
	}
}
