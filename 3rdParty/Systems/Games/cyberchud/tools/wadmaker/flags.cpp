#include <string.h>

#include "flags.h"

#define REQUIRED_ARGS 4

int flags(cfg_t *cfg, const int argc, const char *argv[]) {
	const char* helpStr = "wadmaker [OPTIONS] [INPUT ...] [OUTPUT WAD] [OUTPUT HEADER]\n"
		"  -s  Skeleton Mode\n";

	/* Defaults */
	cfg->mode = MODE_BASIC;
	cfg->compression_ratio = 0.9f;
	cfg->pwd_dir = std::filesystem::current_path();

	/* Handle Flags */
	for (int i=1; i<argc; i++) {
		if (*argv[i] == '-') {
			cfg->flagCount++;
			if (strcmp(argv[i], "-g")==0) {
				cfg->mode = MODE_GLOB;
			} else if (strcmp(argv[i], "-r")==0) { // Compression Ratio
				cfg->flagCount++;
				cfg->compression_ratio = strtod(argv[++i], NULL);
			} else if (strcmp(argv[i], "-c")==0) { // Compression Level
				cfg->flagCount++;
				cfg->compression_level = atoi(argv[++i]);
			} else if (strcmp(argv[i], "-d")==0) { // PWD
				cfg->flagCount++;
				cfg->pwd_dir = argv[++i];
			} else if (strcmp(argv[i], "-b")==0) {
				cfg->flagCount++;
				cfg->blocksize = strtoul(argv[++i], NULL, 10);
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

	for (int i=cfg->flagCount+1; i<argc-2; i++) {
		std::filesystem::path path(argv[i]);
		if (path.empty()) {
			return 1;
		} else if (!std::filesystem::exists(path)) {
			return 1;
		} else if (std::filesystem::is_directory(path)) {
			for (auto const& dir_entry : std::filesystem::recursive_directory_iterator(path)) {
				if (!dir_entry.is_directory()) {
					cfg->filename_input.emplace_back(dir_entry);
				}
			}
		} else if (!path.has_filename()) {
			return 1;
		} else {
			cfg->filename_input.emplace_back(std::move(path));
		}
	}

	cfg->filename_output = argv[argc-2];
	cfg->header_filename = argv[argc-1];

	return 0;
}
