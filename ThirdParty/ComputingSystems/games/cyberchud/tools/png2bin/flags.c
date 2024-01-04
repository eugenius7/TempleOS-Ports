#include <stdio.h>
#include <string.h>

#include "flags.h"

#define REQUIRED_ARGS 3

int flags(cfg_t *cfg, const int argc, const char *argv[]) {
	const char* helpStr = "holypng [OPTIONS] [INPUT] [OUTPUT]\n"
		"  -g  Gray Mode\n"
		"  -c  Color Mode\n"
		"  -a  Enable Alt Color (Gray Mode Only)\n";

	/* Defaults */
	cfg->mode = MODE_BASIC;
	cfg->alt_color_enabled = 0;

	/* Handle Flags */
	for (int i=1; i<argc; i++) {
		if (*argv[i] == '-') {
			cfg->flagCount++;
			if (strcmp(argv[i], "-c")==0) {
				cfg->mode = MODE_RGB;
			} else if (strcmp(argv[i], "-g")==0) {
				cfg->mode = MODE_GRAY;
			} else if (strcmp(argv[i], "-a")==0) {
				cfg->alt_color_enabled = 1;
			} else if (strcmp(argv[i], "-h")==0 || strcmp(argv[i], "-help")==0 || strcmp(argv[i], "--help")==0) {
				fprintf(stderr, "%s", helpStr);
				return 1;
			}
		} else {
			break;
		}
	}

	if (argc < REQUIRED_ARGS+cfg->flagCount) {
		fprintf(stderr, "ERROR: missing arguments\n%s", helpStr);
		return 1;
	}

	cfg->filename_input = argv[cfg->flagCount+1];
	cfg->filename_output = argv[cfg->flagCount+2];

	return 0;
}
