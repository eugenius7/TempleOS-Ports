#include <stdio.h>
#include <string.h>

#include "flags.h"

#define REQUIRED_ARGS 3

int flags(cfg_t *cfg, const int argc, const char *argv[]) {
	const char* helpStr = "holyass [OPTIONS] [INPUT] [OUTPUT]\n"
		"  -s  Skeleton Mode\n";

	/* Defaults */
	cfg->mode = MODE_BASIC;

	/* Handle Flags */
	for (int i=1; i<argc; i++) {
		if (*argv[i] == '-') {
			cfg->flagCount++;
			if (strcmp(argv[i], "-s")==0) {
				cfg->mode = MODE_SKELETON;
			} else if (strcmp(argv[i], "-a")==0) {
				cfg->mode = MODE_BASIC_ANIM;
			} else if (strcmp(argv[i], "-h")==0 || strcmp(argv[i], "-help")==0 || strcmp(argv[i], "--help")==0) {
				fprintf(stderr, "%s", helpStr);
				return 1;
			}
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
