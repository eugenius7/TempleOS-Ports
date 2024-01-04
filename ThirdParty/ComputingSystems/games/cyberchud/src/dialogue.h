#ifndef DIALOGUE_H
#define DIALOGUE_H

typedef void (*talkbox_callback_t)(void* const engine);

typedef enum {
	DIALOGUE_APARTMENT0,
	DIALOGUE_CEO0,
	DIALOGUE_CRUNK0,
	DIALOGUE_LAIN0,
	DIALOGUE_LIGHTROOM0,
	DIALOGUE_LVL3,
	DIALOGUE_MUTT_0,
	DIALOGUE_MUTT_1,
	DIALOGUE_RECEPTION0,
	DIALOGUE_SPAWNER0,
	DIALOGUE_TOTAL,
} DIALOGUE_IDX;

typedef struct {
	int msg_cnt;
	const char* name;
	talkbox_callback_t callback;
	const char* msgs[];
} dialogue_data_t;

typedef struct {
	int cur_msg;
	const dialogue_data_t* data;
} dialogue_t;

void dialogue_init(dialogue_t* const dialogue, const DIALOGUE_IDX idx);
DIALOGUE_IDX dialogue_by_name(const char* const name);

#endif
