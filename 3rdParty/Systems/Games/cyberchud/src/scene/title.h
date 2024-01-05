#ifndef SCENE_TITLE_H
#define SCENE_TITLE_H

#include "engine.h"

void title_update(engine_t* e, const float delta);
void title_draw(engine_threads_t* et, const float delta);
void title_init(engine_t* e);

#endif
