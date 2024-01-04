#ifndef ENTS_PICKUP_H
#define ENTS_PICKUP_H

#include <stdint.h>

void pickup_think(void* const engine, const int32_t id, const float delta);
void pickup_touch(void* const engine, const int32_t id, const int32_t other);
void pickup_touch_beam(void* const engine, const int32_t id, const int32_t other);
void pickup_touch_shotgun(void* const engine, const int32_t id, const int32_t other);
void pickup_touch_smg(void* const engine, const int32_t id, const int32_t other);

#endif
