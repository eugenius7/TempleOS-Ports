#ifndef ENTS_SPAWNER_H
#define ENTS_SPAWNER_H

#include "engine.h"

void spawners_update(engine_t *e, const float delta);
void spawner_activate(void *e, int32_t id, int32_t other);

#endif
