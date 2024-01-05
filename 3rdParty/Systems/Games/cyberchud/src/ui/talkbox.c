#include "ui/talkbox.h"
#include "utils/minmax.h"

void talkbox_draw(talkbox_t* const ui, u8* const fb, const assets_t* const assets, audio_t* const aud, const float delta) {
#define TALKBOX_X1 32
#define TALKBOX_X2 (SCREEN_W-64)
#define TALKBOX_Y1 240
#define TALKBOX_Y2 (TALKBOX_Y1+128)
	vec2s center;
	center.x = (float)TALKBOX_X1+(float)(TALKBOX_X2-TALKBOX_X1)/2 + sinf(ui->txt.time)*128;
	center.y = (float)TALKBOX_Y1+(float)(TALKBOX_Y2-TALKBOX_Y1)/2;
	for (int y=TALKBOX_Y1; y<TALKBOX_Y2; y++) {
		for (int x=TALKBOX_X1; x<TALKBOX_X2; x++) {
			vec2s pos = {{x, y}};
			const float dist = MAX(glm_vec2_distance(center.raw, pos.raw)/64, 1.0f);
			const float lval = 1.0f/dist*4-1;
			fb[y*SCREEN_W+x] = (u8)lval;
		}
	}
	const px_t* enterpx;
	if ((int)(ui->txt.time*2)%2) {
		enterpx = assets->px_pal[PX_PAL_ENTER_KEY0];
	} else {
		enterpx = assets->px_pal[PX_PAL_ENTER_KEY1];
	}
	draw_px(fb, enterpx, TALKBOX_X2-32, TALKBOX_Y2-32);
	const size_t new_cnt = DrawTextBounce(fb, &assets->font_pinzelan, &ui->txt, TALKBOX_X1+4, TALKBOX_Y1+4);
	if (ui->last_cnt != new_cnt) {
		ui->last_cnt = new_cnt;
		snd_play_sfx(aud, assets->snds[SND_TEXT_BEEP]);
	}
	ui->txt.time += delta;
}

void talkbox_handle(talkbox_t* const ui, controls_t* const controls, const float delta) {
	if (!ui->enabled)
		return;
	if (controls->enterPressed && ui->txt.time >= 0.25f) {
		ui->dialogue.cur_msg++;
		if (ui->dialogue.cur_msg >= ui->dialogue.data->msg_cnt) {
			ui->enabled = 0;
			if (ui->callback)
				ui->callback(ui->callback_payload);
			return;
		}
		ui->txt.time = 0;
		ui->txt.msg = ui->dialogue.data->msgs[ui->dialogue.cur_msg];
	}
}

void talkbox_init(talkbox_t* const ui, const DIALOGUE_IDX diag_idx, void* const callback_payload, const vec3s pos) {
	ui->enabled = 1;
	dialogue_init(&ui->dialogue, diag_idx);
	ui->txt.time = 0;
	ui->txt.speed = 64.0f;
	ui->txt.msg = ui->dialogue.data->msgs[ui->dialogue.cur_msg];
	ui->last_cnt = 0;
	ui->callback = ui->dialogue.data->callback;
	ui->callback_payload = callback_payload;
	ui->callback_pos = pos;
}
