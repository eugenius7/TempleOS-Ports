#include <assert.h>

#include "shader.h"
#include "debug.h"
#include "palette.h"
#include "shaders/ubershader.h"
#include "utils/minmax.h"
#include "utils/myds.h"
#include "utils/mypow.h"

#ifndef NDEBUG
#include "text.h"
#endif

void draw_frags(const engine_t* const e, frag_uniform_t* const uni, thread_vert_out_t* const frag_in_data) {
	for (int thread_id=0; thread_id<uni->thread_cnt; thread_id++) {
		switch (uni->shader_cfg_idx) {
			case 0:
				triangle_world(uni, &frag_in_data[thread_id].tris.wfrags);
				break;
			case 1:
				triangle_world_no_pl(uni, &frag_in_data[thread_id].tris.wfrags);
				break;
			case 2:
				triangle_world_no_sc(uni, &frag_in_data[thread_id].tris.wfrags);
				break;
			case 3:
				triangle_world_no_plsc(uni, &frag_in_data[thread_id].tris.wfrags);
				break;
		}
	}
	for (int thread_id=0; thread_id<uni->thread_cnt; thread_id++) {
		switch (uni->shader_cfg_idx) {
			case 0:
				triangle_sub(uni, &frag_in_data[thread_id].tris.mfrags);
				break;
			case 1:
				triangle_sub_no_pl(uni, &frag_in_data[thread_id].tris.mfrags);
				break;
			case 2:
				triangle_sub_no_sc(uni, &frag_in_data[thread_id].tris.mfrags);
				break;
			case 3:
				triangle_sub_no_plsc(uni, &frag_in_data[thread_id].tris.mfrags);
				break;
		}
	}

	/* convert internal fragment fb to 16-color */
	u8* const fb = uni->fb;
	const int y2 = uni->mask_y2;
	const int interlace = uni->interlace;

	if (e->vfx_flags.screen_pulse) {
		const float t = fmodf(e->screen_pulse_time*2, 1.0f);
		for (int y=uni->mask_y1; y<y2; y++) {
			if ((y+interlace) % 2) continue;
			for (int x=0; x<SCREEN_W; x++) {
				uint8_t val = fb[y*SCREEN_W+x];
				if (!(val&128)) {
					val += 64.0f*t;
					if (val > 127) val = 127;
					val = (float)val/(127.0f/10.0f);
				} else {
					const uint8_t alt_idx_lookup[4] = {0, 13, 14, 15};
					const size_t pal_idx = (float)val/(255.0f/3.0f);
					val = alt_idx_lookup[pal_idx];
				}
				assert(val < PALETTE_SIZE);
				fb[y*SCREEN_W+x] = val;
			}
		}
	} else {
		for (int y=uni->mask_y1; y<y2; y++) {
			if ((y+interlace) % 2) continue;
			for (int x=0; x<SCREEN_W; x++) {
				uint8_t val = fb[y*SCREEN_W+x];
				if (!(val&128)) {
					val = (float)val/(127.0f/10.0f);
				} else {
					const uint8_t alt_idx_lookup[4] = {0, 13, 14, 15};
					const size_t pal_idx = (float)val/(255.0f/3.0f);
					val = alt_idx_lookup[pal_idx];
				}
				assert(val < PALETTE_SIZE);
				fb[y*SCREEN_W+x] = val;
			}
		}
	}
}
