#ifndef PX_H
#define PX_H

#include "vec.h"

#define GLYPH_TOTAL 94

typedef struct {
	uint16_t w;
	uint16_t h;
	u8 data[];
} px_t;

typedef struct {
	int font_height;
	int space_width;
	const px_t* glyph[GLYPH_TOTAL];
	int8_t font_y[GLYPH_TOTAL];
} font_t;

typedef struct {
	float time;
	float speed;
	const char* msg;
} text_bounce_t;

vec2_i16 measure_text(const font_t* font, const char* str);
void DrawText(u8* fb, const font_t* font, const char* str, int x, int y);
void DrawTextCenteredFB(u8* fb, const font_t* font, const char* str, int x, int y, const int w, const int h);
size_t DrawTextBounce(u8* fb, const font_t* font, text_bounce_t* txt, int x, int y);
void DrawTextBounceCentered(u8* fb, const font_t* font, text_bounce_t* txt, int x, int y);

void draw_px(u8* fb, const px_t* px, int x, int y);
void draw_px_real(u8* fb, const px_t* px, int x, int y);

void draw_px_pal_offset(u8* fb, const px_t* px, int x, int y, const u8 offset);
void draw_px_real_pal_offset(u8* fb, const px_t* px, int x, int y, const u8 offset);

static inline void DrawTextCentered(u8* fb, const font_t* font, const char* str, int x, int y) {
	DrawTextCenteredFB(fb, font, str, x, y, SCREEN_W, SCREEN_H);
}

#endif
