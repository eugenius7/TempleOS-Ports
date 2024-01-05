#ifndef ENTS_BUTTON_H
#define ENTS_BUTTON_H

#include <stdint.h>

void button_click(void *data, int32_t id, int32_t other);
void button_hit(void *data, int32_t id, int16_t dmg);

#endif
