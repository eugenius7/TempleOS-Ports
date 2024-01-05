#ifndef BSP_PARSE_H
#define BSP_PARSE_H

#include "assets.h"

int bsp_parse(char* const start, bsp_t* const bsp, const bsp_lux_t* const lux, const assets_t* const assets, alloc_t* const alloc);

/* called once per BSP to transform from Quake-space */
void bsp_transform(char* const start);

#endif
