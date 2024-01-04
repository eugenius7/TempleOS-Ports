#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>

typedef struct {
	float time;
	float pitch;
} pitch_t;

typedef struct {
	uint32_t len;
	float duration;
	pitch_t *pitch;
} pitch_data_t;

static int get_current_idx(const pitch_data_t* const data, const double time) {
	for (uint32_t j=1; j<data->len; j++) {
		if (time < data->pitch[j].time) {
			return j-1;
		}
	}
	return data->len-1;
}

void audio_callback(void *userdata, unsigned char* stream, int len) {
	const pitch_data_t* const pitch_data = userdata;
	const pitch_t* const pitch = pitch_data->pitch;
	static double counter = 0;
	static double freq_counter = 0;
	static double freq;
	static int idx;
	idx = get_current_idx(pitch_data, counter);
	freq = pitch[idx].pitch;
	for (int i=0; i<len; i++) {
		const int bit = (int)fmod(freq_counter*freq*2, 2) * 16;
		assert(bit <= 16);
		stream[i] = bit;
		const double time_inc = 1.0 / 11025;
		freq_counter += time_inc;
		counter += time_inc;
		counter = fmod(counter, pitch_data->duration);
		const int new_idx = get_current_idx(pitch_data, counter);
		if (idx != new_idx) {
			freq = pitch[idx].pitch;
			/* printf("t:%.2f freq: %.1f\n", counter, freq); */
		}
	}
}

int main(const int argc, const char* argv[]) {
	FILE *fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "can't open file: %s\n", argv[1]);
		return 1;
	}
	fseek(fp, 0, SEEK_END); // seek to end of file
	const long int size = ftell(fp); // get current file pointer
	fseek(fp, 0, SEEK_SET); // seek back to beginning of file
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

	pitch_data.pitch = (pitch_t*)malloc(sizeof(pitch_t)*pitch_data.len);

	const char* token = strtok(buf, "\n");
	pitch_t* pitch = pitch_data.pitch;
	for (size_t i=0; i<pitch_data.len; i++, pitch++) {
		pitch->time = atof(token);
		token = strchr(token, ' ');
		pitch->pitch = atof(token);
		token = strtok(NULL, "\n");
		printf("time: %.2f freq: %.2f\n", pitch->time, pitch->pitch);
	}
	pitch_data.duration = pitch_data.pitch[pitch_data.len-1].time;
	printf("duration: %.2f\n", pitch_data.duration);

	if (argc >= 3) {
		FILE *fout = fopen(argv[2], "wb");
		fwrite(&pitch_data.len, sizeof(pitch_data.len), 1, fout);
		fwrite(&pitch_data.duration, sizeof(pitch_data.duration), 1, fout);
		fwrite(&pitch_data.pitch, sizeof(pitch_t), pitch_data.len, fout);
		fclose(fout);
	}

	/* Init SDL */
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_AUDIO) != 0) {
		/* fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError()); */
		return 1;
	}

	SDL_AudioSpec spec_req={0};
	spec_req.freq = 11025;
	spec_req.format = AUDIO_S8;
	spec_req.channels = 1;
	spec_req.samples = 8;
	spec_req.callback = audio_callback;
	spec_req.userdata = &pitch_data;
	const SDL_AudioDeviceID devid = SDL_OpenAudioDevice(NULL, 0, &spec_req, NULL, 0);
	SDL_PauseAudioDevice(devid, 0);

	fputs("[INIT] SDL Initialized\n", stdout);
	while (1) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					SDL_Quit();
					return 0;
			}
		}
	}
	return 0;
}
