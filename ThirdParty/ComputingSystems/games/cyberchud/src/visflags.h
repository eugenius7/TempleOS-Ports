#ifndef VISFLAGS_H
#define VISFLAGS_H

#define VISFLAG_IDX_GLOBAL 0
#define VISFLAG_IDX_PLAYER 1
#define VISFLAG_IDX_POINTLIGHTS (VISFLAG_IDX_PLAYER+1)
#define VISFLAG_IDX_SHADOWCASTERS (VISFLAG_IDX_POINTLIGHTS+MAX_DYNAMIC_LIGHTS)
#define MAX_VISFLAGS (MAX_DYNAMIC_LIGHTS*2+2)

#include "model.h"

void new_visflags(model_basic_t* worldmap, visflags_t out_arr[MAX_VISFLAGS], const size_t cnt);

#endif
