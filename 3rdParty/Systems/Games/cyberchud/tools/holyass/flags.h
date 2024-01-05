#ifndef FLAGS_H
#define FLAGS_H

typedef enum {
	MODE_BASIC,
	MODE_BASIC_ANIM,
	MODE_SKELETON,
	MODE_NORMAL,
} MODE;

typedef struct {
	int flagCount;
	MODE mode;
	const char* filename_input;
	const char* filename_output;
} cfg_t;

int flags(cfg_t *cfg, const int argc, const char *argv[]);

#endif
