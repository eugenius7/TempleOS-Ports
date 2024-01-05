#ifndef ENTS_CHUD_H
#define ENTS_CHUD_H

#include "engine.h"

void chud_init(void *engine, int32_t id);
void chud_think(void *engine, int32_t id, const float delta);
void chud_hit(void *engine, int32_t id, const int16_t dmg);
void chud_task_init(engine_t* const e, const int32_t id, const chud_task_e task);

#endif
