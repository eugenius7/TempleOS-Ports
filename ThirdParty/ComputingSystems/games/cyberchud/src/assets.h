#ifndef ASSETS_H
#define ASSETS_H

#include "vec.h"
#include "model.h"
#include "wad.h"
#include "bsp.h"
#include "sound.h"
#include "px.h"

typedef struct {
	const char* name;
	const i16* diffuse;
	i16 normal;
	i8 shader_idx;
	i8 diff_cnt;
} material_info_t;

typedef struct {
	const char* data;
	model_basic_t models_basic[MODELS_STATIC_TOTAL];
	model_anim_data_t models_anim[MODELS_ANIM_TOTAL];
	const px_t* px_rgb[PX_RGB_TOTAL];
	const px_t* px_gray[PX_GRAY_TOTAL];
	const px_t* px_pal[PX_PAL_TOTAL];
	bsp_t map;
	font_t font_matchup;
	font_t font_pinzelan;
	font_t font_pinzelan2x;
	const pitch_data_t* snds[SND_TOTAL];
} assets_t;

int assets_init(assets_t* const assets, alloc_t* const alloc);
int assets_free(assets_t *assets);
int assets_load_level(assets_t* const assets, alloc_t* const alloc, int level_num);

#endif
