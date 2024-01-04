#ifndef AI_H
#define AI_H

#include <stdint.h>

typedef enum {
	MOB_CHUD,
	MOB_MUTT,
	MOB_TROON,
} MOB_CLASS;

typedef enum {
	MOB_STATE_IDLE,
	MOB_STATE_NAV,
	MOB_STATE_ATTACK,
} MOB_STATE;

#define MOB_TOTAL 3

typedef void (*ai_callback_t)(void *e, int32_t id);
typedef void (*ai_callback_think_t)(void *e, int32_t id, const float delta);
typedef void (*ai_callback_hit_t)(void *e, int32_t id, int16_t dmg);
typedef void (*ai_callback_activate_t)(void *e, int32_t id, int32_t other);
typedef struct {
	ai_callback_t init;
	ai_callback_think_t think;
	ai_callback_hit_t hit;
	ai_callback_activate_t activate;
} ai_t;

void player_hit(void *engine, int32_t id, const int16_t dmg);

void chud_init(void *engine, int32_t id);
void chud_think(void *engine, int32_t id, const float delta);
void chud_hit(void *engine, int32_t id, int16_t hp);

void mutt_init(void *engine, int32_t id);
void mutt_think(void *engine, int32_t id, const float delta);
void mutt_hit(void *engine, int32_t id, int16_t hp);

void troon_init(void *engine, int32_t id);
void troon_think(void *engine, int32_t id, const float delta);
void troon_hit(void *engine, int32_t id, int16_t hp);

#endif
