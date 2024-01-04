#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "flags.h"
#include "lodepng.h"

cfg_t g_cfg;

int palette_mode(const unsigned char* const png, const size_t pngsize) {
	unsigned width, height;
	LodePNGState state;
	lodepng_state_init(&state);
	state.decoder.color_convert = 0;
	/* state.info_raw.colortype = LCT_PALETTE; */
	/* state.info_png.color.colortype = LCT_PALETTE; */
	unsigned char* image = NULL;
	unsigned error = lodepng_decode(&image, &width, &height, &state, png, pngsize);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return EXIT_FAILURE;
	}
	/* free(png); */
	/* lodepng_state_cleanup(&state); */

	/* Convert to TempleOS Palette Style */
	for (size_t i=0; i<width*height; i++) {
		image[i] -= 1;
	}

	/* Write Binary */
	FILE* fp = fopen(g_cfg.filename_output, "wb");
	if (!fp) {
		fprintf(stderr, "[ERROR] can't open %s for writing\n", g_cfg.filename_output);
		return EXIT_FAILURE;
	}

	/* Write Header */
	const uint16_t buffer[2] = {width, height};
	fwrite(buffer, sizeof(uint16_t), 2, fp);

	/* Write Image */
	const size_t bytes_to_write = width*height;
	const size_t bytes_written = fwrite(image, 1, bytes_to_write, fp);
	if (bytes_written != bytes_to_write) {
		fprintf(stderr, "[ERROR] wasn't able to write bytes: %lu of %lu\n", bytes_written, bytes_to_write);
		return EXIT_FAILURE;
	}
	/* free(image); */

	return 0;
}

int rgb_mode(const unsigned char* const png, const size_t pngsize) {
	unsigned width, height;
	LodePNGState state;
	lodepng_state_init(&state);
	state.decoder.color_convert = 0;
	unsigned char* image = NULL;
	unsigned error = lodepng_decode(&image, &width, &height, &state, png, pngsize);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return EXIT_FAILURE;
	}
#ifndef NDEBUG
	printf("[rgb_mode] colortype:%u bitdepth:%u\n", state.info_png.color.colortype, state.info_png.color.bitdepth);
#endif
	if (state.info_png.color.bitdepth != 8) {
		fprintf(stderr, "[ERROR] bitdepth is not 8: %s %u\n", g_cfg.filename_input, state.info_png.color.bitdepth);
		return 1;
	}

	if (state.info_png.color.colortype != LCT_RGB) {
		fprintf(stderr, "[ERROR] colortype is not RGB: %s %u\n", g_cfg.filename_input, state.info_png.color.colortype);
		return 1;
	}

	/* Write Binary */
	FILE* fp = fopen(g_cfg.filename_output, "wb");
	if (!fp) {
		fprintf(stderr, "[ERROR] can't open %s for writing\n", g_cfg.filename_output);
		return EXIT_FAILURE;
	}

	/* Write Header */
	const uint16_t buffer[2] = {width, height};
	fwrite(buffer, sizeof(uint16_t), 2, fp);

	/* Write Image */
	const size_t bytes_to_write = width*height*3;
	const size_t bytes_written = fwrite(image, 1, bytes_to_write, fp);
	if (bytes_written != bytes_to_write) {
		fprintf(stderr, "[ERROR] wasn't able to write bytes: %lu of %lu\n", bytes_written, bytes_to_write);
		return EXIT_FAILURE;
	}

	return 0;
}

int gray_mode(const unsigned char* const png, const size_t pngsize) {
	unsigned width, height;
	LodePNGState state;
	lodepng_state_init(&state);
	state.decoder.color_convert = 0;
	unsigned char* image = NULL;
	unsigned error = lodepng_decode(&image, &width, &height, &state, png, pngsize);
	if (error) {
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return EXIT_FAILURE;
	}
#ifndef NDEBUG
	printf("[rgb_mode] colortype:%u bitdepth:%u\n", state.info_png.color.colortype, state.info_png.color.bitdepth);
#endif
	if (state.info_png.color.bitdepth != 8) {
		fprintf(stderr, "[ERROR] bitdepth is not 8: %s %u\n", g_cfg.filename_input, state.info_png.color.bitdepth);
		return 1;
	}

	if (state.info_png.color.colortype != LCT_GREY) {
		fprintf(stderr, "[ERROR] colortype is not GREY: %s %u\n", g_cfg.filename_input, state.info_png.color.colortype);
		return 1;
	}

	/* Write Binary */
	FILE* fp = fopen(g_cfg.filename_output, "wb");
	if (!fp) {
		fprintf(stderr, "[ERROR] can't open %s for writing\n", g_cfg.filename_output);
		return EXIT_FAILURE;
	}

	/* Write Header */
	const uint16_t buffer[2] = {width, height};
	fwrite(buffer, sizeof(uint16_t), 2, fp);

	/* Write Image */
	const size_t bytes_to_write = width*height;

	/* remap 0-255 to 0-127 unless we support alt-color */
	if (!g_cfg.alt_color_enabled) {
		for (size_t i=0; i<bytes_to_write; i++)
			image[i] /= 2;
	}

	const size_t bytes_written = fwrite(image, 1, bytes_to_write, fp);
	if (bytes_written != bytes_to_write) {
		fprintf(stderr, "[ERROR] wasn't able to write bytes: %lu of %lu\n", bytes_written, bytes_to_write);
		return EXIT_FAILURE;
	}

	return 0;
}

int main (const int argc, const char* argv[]) {
	if (flags(&g_cfg, argc, argv)) {
		return EXIT_FAILURE;
	}

	/* Read PNG */
	unsigned char* png = NULL;
	size_t pngsize;
	unsigned error = lodepng_load_file(&png, &pngsize, g_cfg.filename_input);
	if (error) {
		fprintf(stderr, "[ERROR] %u: %s\n", error, lodepng_error_text(error));
		return EXIT_FAILURE;
	}

	switch (g_cfg.mode) {
		case MODE_BASIC:
			if (palette_mode(png, pngsize))
				return EXIT_FAILURE;
			break;
		case MODE_RGB:
			if (rgb_mode(png, pngsize))
				return EXIT_FAILURE;
			break;
		case MODE_GRAY:
			if (gray_mode(png, pngsize))
				return EXIT_FAILURE;
			break;
		default:
			fprintf(stderr, "[ERROR] BAD MODE: %u\n", g_cfg.mode);
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
