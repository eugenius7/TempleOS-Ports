#include "vfx/vfx_neverhappen.h"
#include "vfx/vfx_common.h"
#include "ui/widgets/tos_text.h"
#include "utils/minmax.h"

int vfx_neverhappen(u8* const fb, const float t, const font_t* const font) {
	if (t > 3.6)
		return 0;

	/* this should never happen */
	if (t < 2) {
		const int lines = MIN(t*128, 2000);
		for (int i=0; i<lines; i++) {
			draw_tos_text(fb, "/* THIS SHOULD NEVER HAPPEN */", (2+(i>>3)*31)%SCREEN_W, (2+16*i)%FB_H, SCREEN_W, FB_H);
		}
	}

	if (t < 2) {
	} else if (t < 2.4) {
		memset(fb, 0, SCREEN_W*FB_H);
		DrawTextCentered(fb, font, "THIS", SCREEN_W/2, FB_H/2-64);
	} else if (t < 2.8) {
		memset(fb, 0, SCREEN_W*FB_H);
		DrawTextCentered(fb, font, "SHOULD", SCREEN_W/2, FB_H/2-64);
	} else if (t < 3.2) {
		memset(fb, 0, SCREEN_W*FB_H);
		DrawTextCentered(fb, font, "NEVER", SCREEN_W/2, FB_H/2-64);
	} else if (t < 3.6) {
		memset(fb, 0, SCREEN_W*FB_H);
		DrawTextCentered(fb, font, "HAPPEN", SCREEN_W/2, FB_H/2-64);
	}

	return 1;
}

int vfx_glitchout(u8* const fb, const float t, const font_t* const font, const int w, const int h) {
	/* this should never happen */
	if (t < 2) {
		const int lines = MIN(t*128, 2000);
		for (int i=0; i<lines; i++) {
			draw_tos_text(fb, "/* THIS SHOULD NEVER HAPPEN */", 0, 8*i, w, h);
			draw_tos_text(fb, "/* THIS SHOULD NEVER HAPPEN */", 30*8, 8*i, w, h);
			draw_tos_text(fb, "/* THIS SHOULD NEVER HAPPEN */", 60*8, 8*i, w, h);
		}
	}

#if 0
	if (t < 2) {
	} else if (t < 2.4) {
		memset(fb, 0, w*h);
		DrawTextCenteredFB(fb, font, "THIS", w/2, h/2-64, w, h);
	} else if (t < 2.8) {
		memset(fb, 0, w*h);
		DrawTextCenteredFB(fb, font, "SHOULD", w/2, h/2-64, w, h);
	} else if (t < 3.2) {
		memset(fb, 0, w*h);
		DrawTextCenteredFB(fb, font, "NEVER", w/2, h/2-64, w, h);
	} else if (t < 3.6) {
		memset(fb, 0, w*h);
		DrawTextCenteredFB(fb, font, "HAPPEN", w/2, h/2-64, w, h);
	}
#endif

#if 1
	/* vec3s pos = {{0.5f, 0.5f, t*2.0f}}; */
	vec3s pos = {{0.5f, 0.5f, sinf(t*2.0f)*4}};
	for (int y=0; y<VFX_FB_H; y++) {
		for (int x=0; x<VFX_FB_W; x++) {
			/* vec2s uv; */
			/* uv.x = (float)x/VFX_FB_W; */
			/* uv.y = (float)y/VFX_FB_H; */
			vec3s fpos;
			fpos.x = (float)x/VFX_FB_W;
			fpos.y = (float)y/VFX_FB_H;
			fpos.z = 0;

			const float dist = glm_vec3_distance(pos.raw, fpos.raw);
			float color = 0;
			if (dist > 0) {
				const float val = 1.0f / dist;

				/* const float dist = glm_vec3_distance(pos.raw, fpos.raw); */
				color = (u8)(64*val);
			}
			fb[y*VFX_FB_W+x] = color;

			/* for(float i = 1.0; i < 10.0; i++){ */
			/* 	uv.x += 0.6 / i * cosf(i * 2.5* uv.y + t); */
			/* 	uv.y += 0.6 / i * cosf(i * 1.5 * uv.x + t); */
			/* } */

			/* float val = 0.2f/fabsf(sinf(t-uv.y-uv.x)); */
			/* if (val > 1) val = 1; */
			/* if (val < 0) val = 0; */
			/* e->vfx_fb->data[y*VFX_FB_W+x] = val*255; */
		}
	}
#endif

	return 1;
}
