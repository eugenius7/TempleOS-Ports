#include <stdlib.h>

#define WAD_IMPLEMENTATION
#include "wad.h"
#include "text.h"
#include "utils/mymalloc.h"

static int uncompressed_read(FILE* fin, const file_header_t header, char* const dest) {
	if (fseek(fin, header.pos, SEEK_SET) == -1)
		return 1;
	if (fread(dest, 1, header.size, fin) != header.size)
		return 1;
	return 0;
}

static int compressed_read(const wad_t* const arc, const file_header_t header, char* const dest) {
	if (fseek(arc->fp, header.pos, SEEK_SET) == -1)
		return 1;

	size_t remaining = header.size_compressed;
	size_t toRead = header.size_compressed < arc->buff_in_size ? header.size_compressed: arc->buff_in_size;
	size_t lastRet = 0;
	size_t read;
	ZSTD_outBuffer output = { dest, header.size, 0 };
	while ((read = fread(arc->buff_in, 1, toRead, arc->fp))) {
		if (read != toRead) {
			return 1;
		}
		remaining -= read;
		toRead = remaining < arc->buff_in_size ? remaining : arc->buff_in_size;
		ZSTD_inBuffer input = { arc->buff_in, read, 0 };
		/* Given a valid frame, zstd won't consume the last byte of the frame
		 * until it has flushed all of the decompressed data of the frame.
		 * Therefore, instead of checking if the return code is 0, we can
		 * decompress just check if input.pos < input.size.
		 */
		while (input.pos < input.size) {
			/* The return code is zero if the frame is complete, but there may
			 * be multiple frames concatenated together. Zstd will automatically
			 * reset the context when a frame is complete. Still, calling
			 * ZSTD_DCtx_reset() can be useful to reset the context to a clean
			 * state, for instance if the last decompression call returned an
			 * error.
			 */
			size_t const ret = ZSTD_decompressStream(arc->dctx, &output , &input);
			if (ZSTD_isError(ret))
				return ret;
			lastRet = ret;
		}
	}

	if (lastRet != 0) {
		/* The last return value from ZSTD_decompressStream did not end on a
		 * frame, but we reached the end of the file! We assume this is an
		 * error, and the input was truncated.
		 */
		myprintf("EOF before end of stream: %zu\n", lastRet);
		return 1;
	}

	return 0;
}

static void* zstd_malloc(void* __attribute__((unused)) opaque, size_t size) {
#ifndef NDEBUG
	myprintf("[zstd_malloc] %lu\n", size);
#endif
	void* ptr = mymalloc(size);
#ifndef NDEBUG
	myprintf("[zstd_malloc] size:%lu ptr:0x%p\n", size, ptr);
#endif
	return ptr;
}

static void zstd_free(void* __attribute__((unused)) opaque, void* ptr) {
#ifndef NDEBUG
	myprintf("[zstd_free] ptr:0x%p\n", ptr);
#endif
	myfree(ptr);
}

int wad_init(wad_t* const arc, const char* filename) {
	arc->fp = fopen(filename, "rb");
	if (arc->fp == NULL) {
		myprintf("[wad_init] fopen failed: %s\n", filename);
		return 1;
	}

#ifndef NDEBUG
	if(fseek(arc->fp, 0, SEEK_END) != 0) {
		myprintf("[wad_init] ERR! %s fseek SEEK_END\n", filename);
		fclose(arc->fp);
		return 1;
	}
	const long size = ftell(arc->fp);
	myprintf("[wad_init] %s size:%ld\n", filename, size);
	fseek(arc->fp, 0, SEEK_SET);
#endif

	arc->buff_in_size = ZSTD_DStreamInSize();
	arc->buff_in = mymalloc(arc->buff_in_size);

	if (arc->buff_in == NULL) {
		myprintf("[wad_init] arc->buff_in == NULL, bytes:%lu\n", arc->buff_in_size);
		return 1;
	}

	ZSTD_customMem customMem = { zstd_malloc, zstd_free, NULL };
	arc->dctx = ZSTD_createDCtx_advanced(customMem);
	if (arc->dctx == NULL) {
		fputs("[wad_init] arc->dctx == NULL\n", stderr);
		return 1;
	}

	return 0;
}

void wad_free(wad_t* const arc) {
	myfree(arc->buff_in);
	ZSTD_freeDCtx(arc->dctx);
}

int wad_get(const wad_t* const arc, const WAD_IDX idx, char* data) {
	const file_header_t* header = &g_wad[idx];
	if (header->size_compressed > 0) {
		if (compressed_read(arc, *header, data))
			return 1;
	} else {
		uncompressed_read(arc->fp, *header, data);
	}
	return 0;
}

int wad_get_glob(const wad_t* const arc, char* data) {
#ifndef NDEBUG
	myprintf("[wad_get_glob] wad:%lx data:%lx\n", arc, data);
#endif
	file_header_t header;
	header.pos = 0;
	header.size = WAD_GLOB_SIZE;
	header.size_compressed = WAD_GLOB_SIZE_ZSTD;
	compressed_read(arc, header, data);
#ifndef NDEBUG
	myprintf("[wad_get_glob] done wad:%lx data:%lx\n", arc, data);
#endif
	return 0;
}

int wad_get_malloc(const wad_t* const arc, const WAD_IDX idx, char** data) {
	*data = (char*)malloc(g_wad[idx].size);
	if (*data == NULL)
		return 1;

	wad_get(arc, idx, *data);
	return 0;
}
