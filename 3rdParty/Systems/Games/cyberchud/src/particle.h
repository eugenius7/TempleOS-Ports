#ifndef PARTICLE_H
#define PARTICLE_H

#include "engine.h"

void particle_update(engine_t *e, const float delta);
void particle_emitter_update(engine_t *e, const float delta);

#endif
