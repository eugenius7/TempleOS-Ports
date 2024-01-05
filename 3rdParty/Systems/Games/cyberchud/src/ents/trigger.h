#ifndef ENTS_TRIGGER_H
#define ENTS_TRIGGER_H

#include <stdint.h>

void think_suicide(void* const engine, const int32_t id, const float delta);

void touch_trigger_multiple(void *data, int32_t id, int32_t other);
void touch_trigger_once(void *data, int32_t id, int32_t other);
void touch_talk(void *data, int32_t id, int32_t other);
void touch_trigger_levelchange(void *data, int32_t id, int32_t other);
void touch_trigger_neverhappen(void *data, int32_t id, int32_t other);
void touch_trigger_glitchout(void *data, int32_t id, int32_t other);
void touch_trigger_noise(void *data, int32_t id, int32_t other);
void touch_trigger_wireframe(void *data, int32_t id, int32_t other);

#endif
