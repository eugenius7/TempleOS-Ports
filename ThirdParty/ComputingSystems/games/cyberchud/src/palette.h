#ifndef PALETTE_H
#define PALETTE_H

#include "xorshift.h"

#define PALETTE_SIZE 16

typedef struct {
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
} rgba_t;

typedef union {
	uint32_t hex;
	rgba_t rgba;
} palette_color_t;

typedef struct {
	palette_color_t colors[PALETTE_SIZE];
} palette_t;

void palette_randomize(palette_t* palette, xorshift_t* seed);
void palette_scale(palette_t* const palette, palette_t* const palette_base, const float scale);

#endif
