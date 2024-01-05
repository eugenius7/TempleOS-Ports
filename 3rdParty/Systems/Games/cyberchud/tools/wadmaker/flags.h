#ifndef FLAGS_H
#define FLAGS_H

#include <filesystem>
#include <stddef.h>
#include <vector>

typedef enum {
	MODE_BASIC,
	MODE_GLOB,
} MODE;

typedef struct {
	int flagCount;
	int compression_level;
	float compression_ratio;
	size_t blocksize;
	MODE mode;
	std::vector<std::filesystem::path> filename_input;
	std::filesystem::path filename_output;
	std::filesystem::path pwd_dir;
	const char* header_filename;
} cfg_t;

int flags(cfg_t *cfg, const int argc, const char *argv[]);

#endif
