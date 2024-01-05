#ifndef UI_TALKBOX_H
#define UI_TALKBOX_H

#include "assets.h"
#include "dialogue.h"

typedef struct {
	unsigned int enabled : 1;
	dialogue_t dialogue;
	text_bounce_t txt;
	size_t last_cnt;
	talkbox_callback_t callback;
	void* callback_payload;
	vec3s callback_pos;
} talkbox_t;

void talkbox_init(talkbox_t* const ui, const DIALOGUE_IDX diag_idx, void* const callback_payload, const vec3s pos);
void talkbox_handle(talkbox_t* const ui, controls_t* const controls, const float delta);
void talkbox_draw(talkbox_t* const ui, u8* const fb, const assets_t* const assets, audio_t* const aud, const float delta);

#endif
