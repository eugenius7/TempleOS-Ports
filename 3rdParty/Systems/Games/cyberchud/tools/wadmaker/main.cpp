#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <filesystem>

#include "flags.h"
#include "common.h"
#include "zstd.h"

#define CATEGORY_CNT 7
typedef struct {
	const char* const path;
	int include_extension;
} category_settings_t;
static const category_settings_t CATEGORIES[CATEGORY_CNT] = {
	{"bsp/", 1},
	{"models/anim/", 0},
	{"models/static/", 0},
	{"px/gray/", 0},
	{"px/pal/", 0},
	{"px/rgb/", 0},
	{"snd/", 0},
};

cfg_t g_cfg;

typedef struct {
	uint64_t pos;
	uint64_t size;
	uint64_t compressed_size;
	uint64_t str_pos;
	uint32_t flags;

	const char* raw_path;
	std::filesystem::path relative_path;
} file_header_t;

typedef struct {
	uint32_t idx;
	char name[128];
} category_entry_t;

typedef struct {
	std::vector<category_entry_t> entries;
} category_t;

typedef struct {
	uint64_t pos;
	uint64_t size;
	uint64_t size_compressed;
	std::vector<file_header_t> file_headers;
	category_t categories[CATEGORY_CNT];
} header_t;

typedef struct {
	void* buffIn;
	void* buffOut;
	size_t buffInSize;
	size_t buffOutSize;
	ZSTD_CCtx* cctx;
} resources_t;

static inline void* malloc_orDie(size_t size) {
	void* const buff = malloc(size);
	if (buff) return buff;
	perror("malloc");
	exit(ERROR_malloc);
}

static size_t compress_guess(FILE* fout, file_header_t *header) {
	const size_t cBuffSize = ZSTD_compressBound(header->size);
	void* const cBuff = malloc_orDie(cBuffSize);

	FILE* const fin = fopen_orDie(header->raw_path, "rb");
	char* read_buffer = (char*)malloc(header->size);
	fread(read_buffer, 1, header->size, fin);
	size_t const cSize = ZSTD_compress(cBuff, cBuffSize, read_buffer, header->size, g_cfg.compression_level);
	CHECK_ZSTD(cSize);

#ifndef NDEBUG
	printf("[compress_guess] compressed:%zu uncompressed:%zu ratio:%.2f\n", cSize, header->size, static_cast<float>(cSize) / static_cast<float>(header->size));
#endif
	if (static_cast<float>(cSize) / static_cast<float>(header->size) <= g_cfg.compression_ratio) {
		header->compressed_size = cSize;
		fwrite(cBuff, 1, cSize, fout);
		return header->compressed_size;
	} else {
		header->compressed_size = 0;
		fwrite(read_buffer, 1, header->size, fout);
		return header->size;
	}
}

static int skip_padding(uintptr_t reader) {
	int align_mod = reader%16;
	if (align_mod != 0) {
		align_mod = 16-align_mod;
#ifndef NDEBUG
		printf("[skip_padding] skipping: %d %lu\n", align_mod, reader);
#endif
	}
	return align_mod;
}

static size_t glob_write(std::vector<char>& out, file_header_t *header) {
	FILE* const fin = fopen_orDie(header->raw_path, "rb");
	const size_t start = out.size();
	out.resize(start+header->size);
	fread(&out[start], 1, header->size, fin);

	header->compressed_size = 0;
	const size_t end_pos = out.size();
	const size_t align_mod = skip_padding(end_pos);
	const size_t aligned_pos = end_pos + align_mod;
	if (align_mod) {
		out.resize(aligned_pos);
		memset(&out[end_pos], 0, align_mod);
	}
	return aligned_pos;
}

void glob_mode(FILE* writer, header_t& header) {
	uint64_t total_glob_size = 0;
	for (auto& file_header : header.file_headers) {
		total_glob_size += file_header.size;
	}
	std::vector<char> buffer;
	buffer.reserve(total_glob_size);
	for (auto& file_header : header.file_headers) {
		file_header.pos = header.pos;
		header.pos = glob_write(buffer, &file_header);
	}

	const size_t cBuffSize = ZSTD_compressBound(buffer.size());
	void* const cBuff = malloc_orDie(cBuffSize);

	size_t const cSize = ZSTD_compress(cBuff, cBuffSize, buffer.data(), buffer.size(), g_cfg.compression_level);
	CHECK_ZSTD(cSize);
	fwrite(cBuff, 1, cSize, writer);

	header.size = buffer.size();
	header.size_compressed = cSize;
#ifndef NDEBUG
	printf("[glob_mode] compressed:%zu uncompressed:%zu ratio:.2%f\n", cSize, buffer.size(), static_cast<float>(cSize) / static_cast<float>(buffer.size()));
#endif
}

static void upper_str(char* dest, const char* src) {
	const size_t len = strlen(src);
	for (size_t i=0; i<len; i++) {
		if (isalpha(src[i])) {
			dest[i] = toupper(src[i]);
		} else if (isdigit(src[i])) {
			dest[i] = src[i];
		} else {
			dest[i] = '_';
		}
	}
	dest[len] = '\0';
}

int write_header_file(header_t *header, const char* path) {
	FILE* reader = fopen(path, "rb");
	if (reader == NULL) {
		return EXIT_FAILURE;
	}
	if (fseek(reader, 0L, SEEK_END) == -1) {
		return EXIT_FAILURE;
	}
	file_header_t file_header;
	file_header.size = ftell(reader);
	if (fclose(reader) != 0) {
		return EXIT_FAILURE;
	}
	if (file_header.size == 0) {
		return EXIT_FAILURE;
	}
	file_header.raw_path = path;
	file_header.relative_path = std::filesystem::proximate(path, g_cfg.pwd_dir);
	/* Add File to Category */
	for (int i=0; i<CATEGORY_CNT; i++) {
#ifndef NDEBUG
		printf("[CATEGORY] category:%s\n", file_header.relative_path.string().c_str());
#endif
		if (file_header.relative_path.string().rfind(CATEGORIES[i].path, 0) == 0) {
			category_entry_t entry;
			entry.idx = header->file_headers.size();
			std::string stem;
			if (CATEGORIES[i].include_extension) {
				stem = file_header.relative_path.string();
			} else {
				stem = file_header.relative_path.stem().string();
			}
			if (stem.size() >= 128) {
				fprintf(stderr, "stem too long: %s\n", stem.c_str());
				return 1;
			}
			upper_str(entry.name, stem.c_str());
			header->categories[i].entries.push_back(entry);
#ifndef NDEBUG
			printf("[CATEGORY] category:%d entry.idx:%u entry.name:%s\n", i, entry.idx, entry.name);
#endif
		}
	}

	header->file_headers.emplace_back(std::move(file_header));
	return EXIT_SUCCESS;
}

int write_c_header(const header_t* header) {
	char filename_upper[512] = {0}; // TODO leaky
	FILE* fp_header = fopen_orDie(g_cfg.header_filename, "w");
	size_t size_max = 0;
	for (size_t i=0; i<header->file_headers.size(); i++) {
		const file_header_t *file_header = &header->file_headers[i];
		if (file_header->size > size_max)
			size_max = file_header->size;
		upper_str(filename_upper, file_header->raw_path);
#ifndef NDEBUG
		printf("[write_c_header] [%lu] %s %s pos:%lu size:%lu csize:%lu\n", i, file_header->raw_path, filename_upper, file_header->pos, file_header->size, file_header->compressed_size);
#endif
	}

	fprintf(fp_header,
"#ifndef WAD_H\n"
"#define WAD_H\n\n"
"#include <stdint.h>\n"
"#include <stdio.h>\n\n"
"#define ZSTD_STATIC_LINKING_ONLY\n"
"#include \"zstd.h\"\n\n"
"typedef struct {\n"
"	uint64_t pos;\n"
"	uint64_t size;\n"
"	uint64_t size_compressed;\n"
"	const char* filename;\n" // TMP
"} file_header_t;\n\n"
"typedef struct {\n"
"	FILE* fp;\n"
"	size_t buff_in_size;\n"
"	void* buff_in;\n"
"	ZSTD_DCtx* dctx;\n"
"} wad_t;\n\n"
"typedef enum {\n");

	for (const auto& file : header->file_headers) {
		upper_str(filename_upper, file.relative_path.c_str());
		fprintf(fp_header, "	WAD_%s,\n", filename_upper);
	}
	fprintf(fp_header,
"} WAD_IDX;\n\n");

	/* Write Category Data */
	for (int ci=0; ci<CATEGORY_CNT; ci++) {
		upper_str(filename_upper, CATEGORIES[ci].path);
		const category_t* const category = &header->categories[ci];
		fprintf(fp_header, "#define %sTOTAL %lu\ntypedef enum {\n", filename_upper, category->entries.size());
		for (const auto& entry : category->entries) {
			fprintf(fp_header, "	%s%s,\n", filename_upper, entry.name);
		}
		fprintf(fp_header, "} %sIDX;\n\n", filename_upper);
	}

	fprintf(fp_header,
"int wad_init(wad_t* const arc, const char* filename);\n"
"int wad_get(const wad_t* const arc, const WAD_IDX idx, char* data);\n"
"int wad_get_malloc(const wad_t* const arc, const WAD_IDX idx, char** data);\n"
"int wad_get_glob(const wad_t* const arc, char* data);\n"
"void wad_free(wad_t* const arc);\n\n"
"#define WAD_TOTAL %lu\n"
"#define WAD_SIZE_MAX %lu\n"
"#define WAD_GLOB_SIZE %lu\n"
"#define WAD_GLOB_SIZE_ZSTD %lu\n"
"extern const file_header_t g_wad[WAD_TOTAL];\n", header->file_headers.size(), size_max, header->size, header->size_compressed);
	for (int ci=0; ci<CATEGORY_CNT; ci++) {
		upper_str(filename_upper, CATEGORIES[ci].path);
		fprintf(fp_header, "extern const uint32_t %sFILES[%sTOTAL];\n", filename_upper, filename_upper);
		fprintf(fp_header, "extern const char* const %sFILES_NAMES[%sTOTAL];\n", filename_upper, filename_upper);
	}

	fprintf(fp_header, "\n#ifdef WAD_IMPLEMENTATION\n");
	/* Write Index Arrays */
	for (int ci=0; ci<CATEGORY_CNT; ci++) {
		upper_str(filename_upper, CATEGORIES[ci].path);
		const category_t* const category = &header->categories[ci];
		fprintf(fp_header, "const uint32_t %sFILES[%sTOTAL]={", filename_upper, filename_upper);
		for (const auto& entry : category->entries) {
			fprintf(fp_header, "%u,", entry.idx);
		}
		fseek(fp_header, -1L, SEEK_CUR); // backspace the last comma
		fprintf(fp_header, "};\n\n");
	}

	/* Write Category Entry Strings */
	for (int ci=0; ci<CATEGORY_CNT; ci++) {
		upper_str(filename_upper, CATEGORIES[ci].path);
		const category_t* const category = &header->categories[ci];
		fprintf(fp_header, "const char* const %sFILES_NAMES[%sTOTAL]={", filename_upper, filename_upper);
		for (const auto& entry : category->entries) {
			fprintf(fp_header, "\"%s\",", entry.name);
		}
		fseek(fp_header, -1L, SEEK_CUR); // backspace the last comma
		fprintf(fp_header, "};\n\n");
	}

	fprintf(fp_header, "const file_header_t g_wad[WAD_TOTAL] = {\n");
	for (const auto& file : header->file_headers) {
		fprintf(fp_header, "	{%lu,%lu,%lu,\"%s\"},\n", file.pos, file.size, file.compressed_size, file.relative_path.c_str());
	}
	fprintf(fp_header, "};\n\n#endif\n#endif\n");
	fclose(fp_header);
	return 0;
}

int main(const int argc, const char *argv[]) {
	header_t header;
	header.pos = 0;

	if (flags(&g_cfg, argc, argv)) {
		fprintf(stderr, "[ERR] flags error\n");
		return 1;
	}

#ifndef NDEBUG
	printf("[wadmaker] OUTPUT: %s\n", g_cfg.filename_output.c_str());
	printf("[wadmaker] HEADER OUTPUT: %s\n", g_cfg.header_filename);
#endif

	for (size_t i=0; i<g_cfg.filename_input.size(); i++) {
#ifndef NDEBUG
		printf("[wadmaker] INPUT [%lu] %s \n", i, g_cfg.filename_input[i].c_str());
#endif
		if (write_header_file(&header, g_cfg.filename_input[i].c_str())) {
			fprintf(stderr, "[ERR] write_header_file\n");
			return EXIT_FAILURE;
		}
	}

	FILE* writer = fopen(g_cfg.filename_output.c_str(), "wb");
	switch (g_cfg.mode) {
		case MODE_BASIC:
			for (auto& file_header : header.file_headers) {
				file_header.pos = header.pos;
				header.pos += compress_guess(writer, &file_header);
			}
			break;
		case MODE_GLOB:
			glob_mode(writer, header);
			break;
	}

	fclose(writer);

	if (write_c_header(&header)) {
		return 1;
	}

	return 0;
}
