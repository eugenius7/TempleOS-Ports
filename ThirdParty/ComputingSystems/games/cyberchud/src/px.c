#include "px.h"
#include "utils/minmax.h"

static void draw_px_intern(u8* fb, const px_t* px, int x, int y, const int w, const int h) {
	if (x >= w || y >= h)
		return;

	int start_x = 0;
	int start_y = 0;

	int fb_start_x = x;
	int fb_start_y = y;

	if (fb_start_x < 0) {
		start_x = -x;
		fb_start_x = 0;
	}
	if (fb_start_y < 0) {
		start_y = -y;
		fb_start_y = 0;
	}

	int end_x = px->w;
	const int test_x = x+end_x;
	if (test_x > w) {
		end_x -= test_x - h;
	}

	int end_y = px->h;
	const int test_y = y+end_y;
	if (test_y > h) {
		end_y -= test_y - h;
	}

	for (int yy=start_y, fby=fb_start_y; yy<end_y; yy++, fby++) {
		for (int xx=start_x, fbx=fb_start_x; xx<end_x; xx++, fbx++) {
			const u8 color = px->data[yy*px->w+xx];
			if (color != 0xff)
				fb[fby*w+fbx] = color;
		}
	}
}

void draw_px(u8* fb, const px_t* px, int x, int y) {
	draw_px_intern(fb, px, x, y, SCREEN_W, FB_H);
}

void draw_px_real(u8* fb, const px_t* px, int x, int y) {
	draw_px_intern(fb, px, x, y, SCREEN_W, SCREEN_H);
}

static void draw_px_pal_offset_intern(u8* fb, const px_t* px, int x, int y, const int fb_height, const u8 offset) {
	if (x >= SCREEN_W || y >= fb_height)
		return;

	int start_x = 0;
	int start_y = 0;

	int fb_start_x = x;
	int fb_start_y = y;

	if (fb_start_x < 0) {
		start_x = -x;
		fb_start_x = 0;
	}
	if (fb_start_y < 0) {
		start_y = -y;
		fb_start_y = 0;
	}

	int end_x = px->w;
	const int test_x = x+end_x;
	if (test_x > SCREEN_W) {
		end_x -= test_x - SCREEN_W;
	}

	int end_y = px->h;
	const int test_y = y+end_y;
	if (test_y > fb_height) {
		end_y -= test_y - fb_height;
	}

	for (int yy=start_y, fby=fb_start_y; yy<end_y; yy++, fby++) {
		for (int xx=start_x, fbx=fb_start_x; xx<end_x; xx++, fbx++) {
			u8 color = px->data[yy*px->w+xx];
			if (color != 0xff) {
				color += offset;
				color %= 16;
				fb[fby*SCREEN_W+fbx] = color;
			}
		}
	}
}

void draw_px_pal_offset(u8* fb, const px_t* px, int x, int y, const u8 offset) {
	draw_px_pal_offset_intern(fb, px, x, y, FB_H, offset);
}

void draw_px_real_pal_offset(u8* fb, const px_t* px, int x, int y, const u8 offset) {
	draw_px_pal_offset_intern(fb, px, x, y, SCREEN_H, offset);
}

vec2_i16 measure_text(const font_t* font, const char* str) {
	vec2_i16 pos = {0, font->font_height};
	for (size_t i=0; i<strlen(str); i++) {
		switch (str[i]) {
			case 10:
				pos.x = 0;
				pos.y += font->font_height;
				break;
			case 32:
				pos.x += font->space_width;
				break;
			default:
				{
				const size_t idx = str[i]-33;
				pos.x += font->glyph[idx]->w;
				}
		}
	}
	return pos;
}

void DrawText(u8* fb, const font_t* font, const char* str, int x, int y) {
	int cur = x;
	const size_t len = strlen(str);
	for (size_t i=0; i<len; i++) {
		const u8 ch = str[i];
		switch (ch) {
			case 10:
				cur = x;
				y += font->font_height;
				break;
			case 32:
				cur += font->space_width;
				break;
			default:
				{
				const u8 idx = ch-33;
				draw_px(fb, font->glyph[idx], cur, y+font->font_y[idx]);
				cur += font->glyph[idx]->w;
				}
		}
	}
}

void DrawTextCenteredFB(u8* fb, const font_t* font, const char* str, int x, int y, const int w, const int h) {
	vec2_i16 size = measure_text(font, str);
	x -= size.x/2;
	int cur = x;
	const size_t len = strlen(str);
	for (size_t i=0; i<len; i++) {
		const u8 ch = str[i];
		switch (ch) {
			case 10:
				cur = x;
				y += font->font_height;
				break;
			case 32:
				cur += font->space_width;
				break;
			default:
				{
				const u8 idx = ch-33;
				draw_px_intern(fb, font->glyph[idx], cur, y+font->font_y[idx], w, h);
				cur += font->glyph[idx]->w;
				}
		}
	}
}

void DrawTextBounceCentered(u8* fb, const font_t* font, text_bounce_t* txt, int x, int y) {
	vec2_i16 size = measure_text(font, txt->msg);
	x -= size.x/2;
	int cur = x;
	const float bounce = txt->time * txt->speed;
	const size_t ibounce = bounce;
	const size_t len = MIN(strlen(txt->msg), ibounce+1);
	for (size_t i=0; i<len; i++) {
		const u8 ch = txt->msg[i];
		switch (ch) {
			case 10:
				cur = x;
				y += font->font_height;
				break;
			case 32:
				cur += font->space_width;
				break;
			default:
				{
				float by = 0;
				if (i == ibounce)
					by += bounce-i;
				const u8 idx = ch-33;
				draw_px(fb, font->glyph[idx], cur, y+font->font_y[idx]+6*by);
				cur += font->glyph[idx]->w;
				}
		}
	}
}

size_t DrawTextBounce(u8* fb, const font_t* font, text_bounce_t* txt, int x, int y) {
	int cur = x;
	const float bounce = txt->time * txt->speed;
	const size_t ibounce = bounce;
	const size_t len = MIN(strlen(txt->msg), ibounce+1);
	/* const size_t len = strlen(txt->msg); */
	for (size_t i=0; i<len; i++) {
		const u8 ch = txt->msg[i];
		switch (ch) {
			case 10:
				cur = x;
				y += font->font_height;
				break;
			case 32:
				cur += font->space_width;
				break;
			default:
				{
				float by = 0;
				if (i == ibounce)
					by += bounce-i;
				const u8 idx = ch-33;
				draw_px(fb, font->glyph[idx], cur, y+font->font_y[idx]+6*by);
				/* draw_px(fb, font->glyph[idx], cur, y+font->font_y[idx]); */
				cur += font->glyph[idx]->w;
				}
		}
	}
	return len;
}
