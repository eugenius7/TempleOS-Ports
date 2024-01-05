#include <stdio.h>
#include <string.h>

#include "flags.h"

#define REQUIRED_ARGS 3

int flags(cfg_t *cfg, const int argc, const char* const argv[]) {
	const char* helpStr = "[OPTIONS] [INPUT] [OUTPUT]\n"
		"  -ts  Trim Start\n"
		"  -te  Trim End\n";

	/* Defaults */
	cfg->flag_cnt = 0;
	cfg->trim_start = false;
	cfg->trim_end = false;
	cfg->print_export = false;

	/* Handle Flags */
	for (int i=1; i<argc; i++) {
		if (*argv[i] == '-') {
			cfg->flag_cnt++;
			if (strcmp(argv[i], "-v")==0) {
				cfg->print_export = true;
			} else if (strcmp(argv[i], "-ts")==0) {
				cfg->trim_start = true;
			} else if (strcmp(argv[i], "-te")==0) {
				cfg->trim_end = true;
			} else if (strcmp(argv[i], "-h")==0 || strcmp(argv[i], "-help")==0 || strcmp(argv[i], "--help")==0) {
				fprintf(stderr, "%s %s", argv[0], helpStr);
				return 1;
			}
		}
	}

	if (argc < REQUIRED_ARGS+cfg->flag_cnt) {
		fprintf(stderr, "ERROR: missing arguments\n%s", helpStr);
		return 1;
	}

	cfg->filename_input = argv[cfg->flag_cnt+1];
	cfg->filename_output = argv[cfg->flag_cnt+2];

	return 0;
}
