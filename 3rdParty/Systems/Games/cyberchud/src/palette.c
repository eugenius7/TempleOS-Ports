#include "palette.h"
#include "vec.h"
#include "utils/minmax.h"
#ifndef NDEBUG
#include "text.h"
#endif

typedef struct {
	float h;
	float s;
	float v;
} ColorHSV;

ColorHSV rgbToHsv(vec3s rgb) {
	ColorHSV hsv;

	float r = rgb.r;
	float g = rgb.g;
	float b = rgb.b;

	float max = MAX(MAX(r, g), b);
	float min = MIN(MIN(r, g), b);

	hsv.v = max;

	float delta = max - min;

	if (delta < 0.00001f) {
		hsv.s = 0;
		hsv.h = 0;
		return hsv;
	}

	if (max > 0.0f) {
		hsv.s = (delta / max);
	} else {
		hsv.s = 0.0f;
		hsv.h = NAN;
		return hsv;
	}

	if (r >= max) {
		hsv.h = (g - b) / delta;
	} else if (g >= max) {
		hsv.h = 2.0f + (b - r) / delta;
	} else {
		hsv.h = 4.0f + (r - g) / delta;
	}

	hsv.h *= 60.0f;

	if (hsv.h < 0.0f) {
		hsv.h += 360.0f;
	}

	return hsv;
}

rgba_t hsvToRgb(ColorHSV hsv) {
	rgba_t rgb;
	rgb.a = 0xff;

	if (hsv.s <= 0.0f) {
		rgb.r = hsv.v * 255;
		rgb.g = hsv.v * 255;
		rgb.b = hsv.v * 255;
		return rgb;
	}

	float hh = hsv.h;
	if (hh >= 360.0f) {
		hh = 0.0f;
	}

	hh /= 60.0f;
	int i = (int)hh;
	float ff = hh - i;
	float p = hsv.v * (1.0f - hsv.s);
	float q = hsv.v * (1.0f - (hsv.s * ff));
	float t = hsv.v * (1.0f - (hsv.s * (1.0f - ff)));

	switch (i) {
		case 0:
			rgb.r = (u8)(hsv.v * 255);
			rgb.g = (u8)(t * 255);
			rgb.b = (u8)(p * 255);
			break;
		case 1:
			rgb.r = q * 255;
			rgb.g = hsv.v * 255;
			rgb.b = p * 255;
			break;
		case 2:
			rgb.r = p * 255;
			rgb.g = hsv.v * 255;
			rgb.b = (u8)(t * 255);
			break;
		case 3:
			rgb.r = p * 255;
			rgb.g = q * 255;
			rgb.b = hsv.v * 255;
			break;
		case 4:
			rgb.r = t * 255;
			rgb.g = p * 255;
			rgb.b = hsv.v * 255;
			break;
		case 5:
		default:
			rgb.r = hsv.v * 255;
			rgb.g = p * 255;
			rgb.b = q * 255;
			break;
	}

	return rgb;
}

static void palette_gen(palette_color_t* const pal, const int steps, const float r, const float g, const float b) {
	vec3s startColor = {{0, 0, 0}};
	vec3s endColor = {{r, g, b}};

	ColorHSV startHSV = rgbToHsv(startColor);
	ColorHSV endHSV = rgbToHsv(endColor);

	float stepSizeH = (endHSV.h - startHSV.h) / (steps - 1);
	float stepSizeS = (endHSV.s - startHSV.s) / (steps - 1);
	float stepSizeV = (endHSV.v - startHSV.v) / (steps - 1);

	for (int i = 0; i < steps; i++) {
		ColorHSV currentHSV;
		currentHSV.h = startHSV.h + (stepSizeH * i);
		currentHSV.s = startHSV.s + (stepSizeS * i);
		currentHSV.v = startHSV.v + (stepSizeV * i);

		pal[i].rgba = hsvToRgb(currentHSV);
	}

#ifndef NDEBUG
	for (int i = 0; i < steps; i++) {
		const rgba_t* const color = &pal[i].rgba;
		myprintf("[pal] [%d] %.1f %.1f %.1f %u %u %u\n", i, r, g, b, color->r, color->g, color->b);
	}
#endif
}

#if 0
static inline rgba_t linear_color_interp(vec3s color1, vec3s color2, float t) {
	rgba_t out;
	out.r = (unsigned char)((1.0f - t) * color1.r + t * color2.r);
	out.g = (unsigned char)((1.0f - t) * color1.g + t * color2.g);
	out.b = (unsigned char)((1.0f - t) * color1.b + t * color2.b);

	return out;
}

static void palette_linear(palette_color_t* const pal, const int steps, vec3s color1, vec3s color2) {
	for (int i=0; i<steps; i++) {
		float t = (float)i / steps+1;
		pal[i].rgba = linear_color_interp(color1, color2, t);
		/* printf("Step %d: R:%d G:%d B:%d\n", i, interpolatedColor.red, interpolatedColor.green, interpolatedColor.blue); */
	}
}
#endif

void palette_randomize(palette_t* palette, xorshift_t* seed) {
#ifndef NDEBUG
	myprintf("[palette_randomize] seed: %lu\n", seed->a);
#endif
	vec3s color;
	color.x = frand(seed);
	color.y = frand(seed);
	color.z = frand(seed);
#ifndef NDEBUG
	myprintf("[palette_randomize] %.2fx%.2fx%.2f\n", color.x, color.y, color.z);
#endif
	palette_gen(&palette->colors[0], 11, color.x, color.y, color.z);

	color.x = frand(seed);
	color.y = frand(seed);
	color.z = frand(seed);
	const int full_color = xorshift64(seed)%3;
	color.raw[full_color] = 1.0f;

	palette_gen(&palette->colors[11], 3, color.x, color.y, color.z);
	palette->colors[11].hex = 0xffff8400;
	palette->colors[12].hex = 0xffff9f00;
	palette->colors[13].hex = 0xffffba00;

	/* vec3s color1 = {{0}}; */
	/* palette_linear(&palette->colors[11], 3, color1, color); */

	palette->colors[14].hex = 0xff55ff22;
	palette->colors[15].hex = 0xffffffff;
}

void palette_scale(palette_t* const palette, palette_t* const palette_base, const float scale) {
	for (int i=0; i<PALETTE_SIZE; i++) {
		palette->colors[i].rgba.r = palette_base->colors[i].rgba.r * scale;
		palette->colors[i].rgba.g = palette_base->colors[i].rgba.g * scale;
		palette->colors[i].rgba.b = palette_base->colors[i].rgba.b * scale;
	}
}
