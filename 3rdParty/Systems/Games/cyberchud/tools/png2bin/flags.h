#ifndef FLAGS_H
#define FLAGS_H

#include <stddef.h>

typedef enum {
	MODE_BASIC,
	MODE_RGB,
	MODE_GRAY,
} MODE;

typedef struct {
	int flagCount;
	MODE mode;
	int alt_color_enabled;
	const char* filename_output;
	const char* filename_input;
} cfg_t;

int flags(cfg_t *cfg, const int argc, const char *argv[]);

#endif
