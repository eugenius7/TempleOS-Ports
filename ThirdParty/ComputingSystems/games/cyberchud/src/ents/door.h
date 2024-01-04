#ifndef ENTS_DOOR_H
#define ENTS_DOOR_H

#include <stdint.h>

void door_touch(void *data, int32_t id, int32_t other);
void door_think(void *data, int32_t id, const float delta);
void door_hit(void *data, int32_t id, int16_t dmg);

#endif
