#ifndef GAME_H
#define GAME_H

#include "macro.h"

typedef struct {
	unsigned int no_pointlights: 1;
	unsigned int no_shadowcasters: 1;
} init_cfg_t;

int DLL_PUBLIC init(init_cfg_t cfg);
int DLL_PUBLIC check_if_quitting(void);
void DLL_PUBLIC loop(void);
int quit(void);

#endif
