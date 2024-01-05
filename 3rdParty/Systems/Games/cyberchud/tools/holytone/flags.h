#ifndef FLAGS_H
#define FLAGS_H

#include <stdbool.h>

typedef struct {
	int flag_cnt;
	bool trim_start;
	bool trim_end;
	bool print_export;
	const char* filename_input;
	const char* filename_output;
} cfg_t;

int flags(cfg_t *cfg, const int argc, const char* const argv[]);

#endif
