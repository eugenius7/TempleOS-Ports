#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flags.h"

typedef struct {
	float time;
	float pitch;
} pitch_t;

typedef struct {
	uint32_t len;
	float duration;
	pitch_t *pitch;
} pitch_data_t;

static long int get_filesize(FILE* const fp) {
	fseek(fp, 0, SEEK_END);
	const long int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return size;
}

int main(const int argc, const char* const argv[]) {
	cfg_t cfg;
	if (flags(&cfg, argc, argv))
		return 1;

	FILE *fp = fopen(cfg.filename_input, "r");
	if (!fp) {
		fprintf(stderr, "[holytone] can't open file: %s\n", cfg.filename_input);
		return 1;
	}

	const long int size = get_filesize(fp);
	char* const buf = (char*)malloc(size);
	fread(buf, 1, size, fp);
	fclose(fp);

	pitch_data_t pitch_data;
	pitch_data.len = 0;
	for (long i=0; i<size-1; i++) {
		if (buf[i] == '\n') {
			pitch_data.len++;
		}
	}
	if (pitch_data.len == 0) {
		fprintf(stderr, "pitch_data.len == 0\n");
		return 1;
	}

	pitch_data.pitch = (pitch_t*)malloc(sizeof(pitch_t)*pitch_data.len);

	const char* token = strtok(buf, "\n");
	pitch_t* pitch = pitch_data.pitch;
	for (size_t i=0; i<pitch_data.len; i++, pitch++) {
		pitch->time = atof(token);
		token = strchr(token, ' ');
		pitch->pitch = atof(token);
		token = strtok(NULL, "\n");
	}
	pitch_data.duration = pitch_data.pitch[pitch_data.len-1].time;

	if (cfg.trim_start) {
		pitch_t* pitch = pitch_data.pitch;
		float trim_t = 0;
		uint32_t trim_cnt;
		for (trim_cnt=0; trim_cnt<pitch_data.len; trim_cnt++, pitch++) {
			if (pitch->pitch != 0) {
				trim_t = pitch->time;
				break;
			}
		}
		if (trim_cnt > 0) {
			pitch_data.len -= trim_cnt;
			pitch_data.duration -= trim_t;
			memcpy(pitch_data.pitch, pitch_data.pitch+trim_cnt, sizeof(pitch_t)*pitch_data.len);
			pitch_t* pitch = pitch_data.pitch;
			for (uint32_t i=0; i<pitch_data.len; i++, pitch++) {
				pitch->time -= trim_t;
			}
		}
	}

	if (cfg.trim_end) {
		pitch_t* pitch = pitch_data.pitch+pitch_data.len-1;
		for (uint32_t trim_cnt=0; trim_cnt<pitch_data.len; trim_cnt++, pitch--) {
			if (pitch->pitch != 0) {
				if (trim_cnt != 0) {
					pitch_data.len -= trim_cnt;
					pitch_data.duration = (pitch+1)->time;
				}
				break;
			}
		}
	}

	FILE *fout = fopen(cfg.filename_output, "wb");
	fwrite(&pitch_data.len, sizeof(pitch_data.len), 1, fout);
	fwrite(&pitch_data.duration, sizeof(pitch_data.duration), 1, fout);
	fwrite(pitch_data.pitch, sizeof(pitch_t), pitch_data.len, fout);
	fclose(fout);

	if (cfg.print_export) {
		pitch_t* pitch = pitch_data.pitch;
		for (uint32_t i=0; i<pitch_data.len; i++, pitch++) {
			printf("[holytone] [%u] t:%.2f f:%.2f\n", i, pitch->time, pitch->pitch);
		}
	}

	return 0;
}
