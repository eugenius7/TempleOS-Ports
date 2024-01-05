#include <string.h>
#include "dialogue.h"
#include "engine.h"
#include "text.h"

static void talk_apartment0_callback(void* engine) {
	engine_t* const e = engine;
	new_pickup(e, e->talkbox.callback_pos, WEAPON_SHOTGUN);
}

static const dialogue_data_t talk_mutt0 = {
	3, "talk_mutt0", NULL,
	{
		"I can't sneed, and we need a bad\n"
		"enough dude to help me sneed\n"
		"again",
		"Are you a bad enough dude to\nhelp me sneed again?",
		"[this is rhetorical]\n[[I didn't implement dialogue choices]]"
	}
};

static const dialogue_data_t talk_mutt1 = {
	2, "talk_mutt1", NULL,
	{
		"As you can see the chuds are uprising\nand we need a bad enough dude",
		"Are you a bad enough dude?"
	}
};

static const dialogue_data_t talk_crunk0 = {
	3, "talk_crunk0", NULL,
	{
		"Hello! Welcome to my bespoke\nesoteric code hellscape.",
		"This is where the gameplay should\nbe, but instead I filled it with\nmemes",
		"I've been crunching for 24 hours\nstraight trying to wrap it up.\nTruly, the work is endless.",
	}
};

static const dialogue_data_t talk_lain0 = {
	1, "talk_lain0", NULL,
	{
		"Let's all love lain.\nLet's all love lain.\nLet's all love lain.\nLet's all love lain.",
	}
};

static const dialogue_data_t talk_lvl3 = {
	1, "talk_lvl3", NULL,
	{
		"This area was made by WaveCruising\nI didn't have time to texture it\nSorry :(",
	}
};

static const dialogue_data_t talk_lightroom0 = {
	1, "talk_lightroom0", NULL,
	{
		"This room was built to test\nmy pointlight shadows\nWow, look at it go!",
	}
};

static const dialogue_data_t talk_ceo0 = {
	3, "talk_ceo0", NULL,
	{
		"I get it, where's the game?\nLook kid, there is no game.",
		"We tried really hard, seriously.\nIt's just...\nmy back is killing me\nand my doctor gave me these pills",
		"how I about I just get you a game\nnext year, it'll be great. Trust me.",
	}
};

static const dialogue_data_t talk_reception0 = {
	1, "talk_reception0", NULL,
	{
		"We're very busy saving western\ncivilization,\njust right after the pool party",
	}
};

static const dialogue_data_t talk_apartment0 = {
	5, "talk_apartment0", talk_apartment0_callback,
	{
		"Look at you...\nyou don't even have a gun.",
		"What do you think this is,\na walking simulator?",
		"Worse, it's a engine tech demo.",
		"Anyways here's your obligatory gun.\nNow make it everyone else's problem",
		"[INSERT ANIMATION HERE]",
	}
};

static const dialogue_data_t talk_spawner0 = {
	1, "talk_spawner0", NULL,
	{
		"This button is dedicated to the\nconcept of gameplay. Press it with F\nto spawn a troon who can live\nrent free in your virtual world",
	}
};

static const dialogue_data_t* const dialogue_lookup[DIALOGUE_TOTAL] = {
	&talk_apartment0,
	&talk_ceo0,
	&talk_crunk0,
	&talk_lain0,
	&talk_lightroom0,
	&talk_lvl3,
	&talk_mutt0,
	&talk_mutt1,
	&talk_reception0,
	&talk_spawner0,
};

void dialogue_init(dialogue_t* const dialogue, const DIALOGUE_IDX idx) {
	dialogue->cur_msg = 0;
	dialogue->data = dialogue_lookup[idx];
}

DIALOGUE_IDX dialogue_by_name(const char* const name) {
	for (int i=0; i<DIALOGUE_TOTAL; i++) {
		if (!strcmp(dialogue_lookup[i]->name, name)) {
			return i;
		}
	}
	myprintf("[ERR] [dialogue_by_name] can't find dialogue: %s\n", name);
	return 0;
}
