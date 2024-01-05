#include <assert.h>

#include "text.h"
#include "engine.h"

static int get_current_idx(const pitch_data_t* const data, const double time) {
	for (uint32_t j=1; j<data->len; j++) {
		if (time < data->notes[j].time) {
			return j-1;
		}
	}
	return data->len-1;
}

void snd_play_sfx(audio_t* aud, const pitch_data_t* snd) {
#ifdef VERBOSE
	myprintf("[snd_play_sfx] aud:0x%lx snd:0x%lx\n", aud, snd);
#endif
	if (aud->mute)
		return;
	const i32 idx = aud->sfx_cnt;
	if (idx < SFX_BUF_LEN) {
		aud->sfx_buf[aud->sfx_cnt].data = snd;
		aud->sfx_buf[aud->sfx_cnt].time = 0;
		aud->sfx_cnt++;
	}
#ifndef NDEBUG
	else {
		myprintf("[play_sfx] too many skipping...\n");
	}
#endif
}

void snd_callback(void *userdata, unsigned char* stream, int len) {
#ifdef VERBOSE
	myprintf("[snd_callback] userdata:%lx stream:%lx len:%d\n", userdata, stream, len);
#endif
	engine_t* const e = userdata;
	audio_t* const aud = &e->audio;
	const double time_inc = 1.0 / e->audio.spec.freq;

#ifdef TOSLIKE
	int16_t* cstream = (int16_t*)stream;
#else
	static double freq_counter = 0;
#endif
	static double freq;
	static int idx;

	/* Fill Audio Buffer */
	for (int i=0; i<len; i++) {
		if (aud->sfx_cnt <= 0) {
#ifdef TOSLIKE
			cstream[i] = 0;
#else
			stream[i] = 0;
			freq_counter += time_inc;
#endif
			continue;
		}

		freq = 0;
		sfx_t* sfx = aud->sfx_buf;
		const int init_sfx_cnt = aud->sfx_cnt;
		for (int j=0; j<init_sfx_cnt; j++, sfx++) {
			const pitch_data_t* const pitch_data = sfx->data;
			const pitch_t* const pitch = pitch_data->notes;
			idx = get_current_idx(pitch_data, sfx->time);
			freq += pitch[idx].pitch;

			sfx->time += time_inc;
			if (sfx->time >= pitch_data->duration) {
				if (--aud->sfx_cnt > j) {
					*sfx = aud->sfx_buf[aud->sfx_cnt];
				}
			}
		}
		freq /= init_sfx_cnt;

#ifdef TOSLIKE
		cstream[i] = freq;
#else
		const int bit = (int)fmod(freq_counter*freq*2, 2) * 16;
		assert(bit <= 16);
		stream[i] = bit;
		freq_counter += time_inc;
#endif
	}
}
