#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <SDL2/SDL_audio.h>

typedef struct {
	float time;
	float pitch;
} pitch_t;

typedef struct {
	uint32_t len;
	float duration;
	pitch_t notes[];
} pitch_data_t;

typedef struct {
	const pitch_data_t* data;
	double time;
} sfx_t;

#define SFX_BUF_LEN 8
typedef struct {
	SDL_AudioSpec spec;
	int mute;
	int32_t sfx_cnt;
	sfx_t sfx_buf[SFX_BUF_LEN];
} audio_t;

void snd_callback(void *userdata, unsigned char* stream, int len);
void snd_play_sfx(audio_t* aud, const pitch_data_t* snd);

#endif
