#include <stdlib.h>

#include "assets.h"
#include "text.h"
#include "wad.h"
#include "bsp_parse.h"
#include "sound.h"
#include "utils/mymalloc.h"
#include "utils/pad.h"

static int lookup_rgb_px(const char* const material) {
	for (uint32_t i=0; i<PX_RGB_TOTAL; i++) {
		if (strcmp(material, PX_RGB_FILES_NAMES[i]) == 0) {
			return i;
		}
	}
	return -1;
}

static int lookup_gray_px(const char* const material) {
	for (uint32_t i=0; i<PX_GRAY_TOTAL; i++) {
		if (strcmp(material, PX_GRAY_FILES_NAMES[i]) == 0) {
			return i;
		}
	}
	return -1;
}

static inline const char* get_data(const char* const data, const WAD_IDX idx) {
	return data+g_wad[idx].pos;
}

static void load_notes(const char* const data, const pitch_data_t* snds[]) {
#ifndef NDEBUG
	myprintf("[load_notes]\n");
#endif
	for (uint32_t i=0; i<SND_TOTAL; i++) {
		snds[i] = (const pitch_data_t*)get_data(data, SND_FILES[i]);
#ifndef NDEBUG
		myprintf("[load_notes] %u len:%u dur:%.2f\n", i, snds[i]->len, snds[i]->duration);
#endif
	}
}

static const char* parse_mesh(const char* data, mesh_t* mesh, const parse_material_t* const model_mats, const assets_t* const assets, const int weights) {
	mesh->face_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	mesh->vert_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	const uint32_t material_idx = *(uint32_t*)data;
	data += sizeof(uint32_t);
	const parse_material_t* const mat_name = &model_mats[material_idx];

#ifndef NDEBUG
	myprintf("[parse_mesh] face_cnt:%u vert_cnt:%u diffuse:%s normal:%s\n", mesh->face_cnt, mesh->vert_cnt, mat_name->diffuse, mat_name->normal);
#endif

	mesh->mat_type = mat_name->type;

	const int diff_idx = lookup_gray_px(mat_name->diffuse);
	if (diff_idx < 0) {
#ifndef NDEBUG
		myprintf("[lookup_gray_px] can't find material: %s\n", mat_name->diffuse);
#endif
		mesh->tex_diffuse_cnt = 0;
		mesh->tex_diffuse[0] = NULL;
	} else {
#ifndef NDEBUG
		myprintf("[lookup_gray_px] found material: %s\n", mat_name->diffuse);
#endif
		mesh->tex_diffuse_cnt = 1;
		mesh->tex_diffuse[0] = assets->px_gray[diff_idx];
	}

	const int norm_idx = lookup_rgb_px(mat_name->normal);
	if (norm_idx < 0) {
#ifndef NDEBUG
		myprintf("[lookup_rgb_px] can't find material: %s\n", mat_name->normal);
#endif
		mesh->tex_normal = NULL;
	} else {
#ifndef NDEBUG
		myprintf("[lookup_rgb_px] found material: %s\n", mat_name->normal);
#endif
		mesh->tex_normal = assets->px_rgb[norm_idx];
	}

	mesh->faces = (const face_t*)data;
	data += mesh->face_cnt*sizeof(face_t);
	mesh->verts = (vert_tangent_t*)data;
	data += mesh->vert_cnt*sizeof(vert_tangent_t);
	mesh->uv = (vec2s*)data;
	data += mesh->vert_cnt*sizeof(vec2s);
	if (weights) {
		mesh->weights = (weights_t*)data;
		data += mesh->vert_cnt*sizeof(weights_t);
	}
#if 0
	/* Debug */
	for (size_t i=0; i<mesh.face_cnt; i++) {
		myprintf("[mesh_static_parse] face:%ux%ux%u\n", mesh.faces[i].p[0], mesh.faces[i].p[1], mesh.faces[i].p[2]);
	}
	for (size_t i=0; i<mesh.vert_cnt ; i++) {
		myprintf("[mesh_static_parse] [%u] pos:%.2fx%.2fx%.2f\n", i, mesh.verts[i].pos.X, mesh.verts[i].pos.Y, mesh.verts[i].pos.Z);
	}
#endif
	return data;
}

#if 0
static const char* parse_model_common(const char* data) {
	model_static_data_t model;
	model.mesh_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model->material_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model.bone_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);

#ifndef NDEBUG
	myprintf("[parse_model_common] mesh_cnt:%u bone_cnt:%u\n", model.mesh_cnt, model.bone_cnt);
#endif

	for (uint32_t i=0; i<model.mesh_cnt; i++) {
		data = parse_mesh(data, (mesh_t*)&model.meshes[i]);
	}
	return data;
}
#endif

static const char* parse_model_basic(const char* data, model_basic_t* model, const assets_t* const assets, alloc_t* const alloc, const int weights) {
	model->mesh_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model->material_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model->bone_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model->bbox = *(bbox_t*)data;
	data += sizeof(bbox_t);

#ifndef NDEBUG
	myprintf("[parse_model_basic] mesh_cnt:%u mat_cnt:%u bone_cnt:%u\n", model->mesh_cnt, model->material_cnt, model->bone_cnt);
#endif

	if (model->mesh_cnt > MAX_MESH_CNT) {
		myprintf("[ERR] [parse_model_basic] too many meshes: %u/%u\n", model->mesh_cnt, MAX_MESH_CNT);
		return NULL;
	}
	if (model->material_cnt > MAX_MATERIAL_CNT) {
		myprintf("[ERR] [parse_model_basic] too many materials: %u/%u\n", model->material_cnt, MAX_MATERIAL_CNT);
		return NULL;
	}
	if (model->bone_cnt > MAX_BONE) {
		myprintf("[ERR] [parse_model_basic] too many bones: %u/%u\n", model->bone_cnt, MAX_BONE);
		return NULL;
	}

	const parse_material_t* const mats = (const parse_material_t*)data;
	data += sizeof(parse_material_t)*model->material_cnt;

	/* so far only the world models have multiple diffuse textures, so just alloc 1 diffuse per mesh */
	const size_t alloc_sz = sizeof(mesh_t)*model->mesh_cnt + sizeof(px_t*)*model->mesh_cnt;
	model->meshes = balloc(alloc, alloc_sz);
	memset(model->meshes, 0, alloc_sz);
	const px_t** pxptr = (const px_t**)((char*)model->meshes + sizeof(mesh_t)*model->mesh_cnt);
	for (u32 i=0; i<model->mesh_cnt; i++, pxptr++) {
		model->meshes[i].tex_diffuse = pxptr;
	}
	for (uint32_t i=0; i<model->mesh_cnt; i++) {
		data = parse_mesh(data, (mesh_t*)&model->meshes[i], mats, assets, weights);
	}

	return data;
}

static const char* model_anim_parse(const char* data, model_anim_data_t* model, const assets_t* const assets, alloc_t* const alloc) {
	const char* const start_pos = data;
	data = parse_model_basic(data, &model->basic, assets, alloc, 1);
	if (data == NULL) {
		return NULL;
	}
#if 0
	model->mesh_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model->material_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);
	model->bone_cnt = *(uint32_t*)data;
	data += sizeof(uint32_t);

	for (uint32_t i=0; i<model->mesh_cnt; i++) {
		data = parse_mesh(data, (mesh_t*)&model->meshes[i]);
	}
#endif

	/* Parse Bones */
	data += pad_skip_padding(data-start_pos, 16);
	model->bones = (bone_data_t*)data;
	data += model->basic.bone_cnt*sizeof(bone_data_t);

	model->anim_cnt = *(u32*)data;
	data += sizeof(u32);

	/* TODO free this calloc somewhere */
	model->anims = mycalloc(model->anim_cnt, sizeof(anim_data_t));

#ifndef NDEBUG
	myprintf("[model_anim_parse] anim_cnt:%u\n", model->anim_cnt);
#endif

	/* Parse Animations */
	for (u32 ai=0; ai<model->anim_cnt; ai++) {
		model->anims[ai].length = *(float*)data;
		/* myprintf("bone_cnt:%u face_cnt:%u vert_cnt:%u length:%f\n", model->bone_cnt, model->face_cnt, model->vert_cnt, model->anim_data.length); */
		data += sizeof(float);

		for (uint32_t bi=0; bi<model->basic.bone_cnt; bi++) {
			anim_ch_data_t* const anim = &model->anims[ai].channels[bi];
			anim->ch_pos_cnt = *(uint32_t*)data;
			data += sizeof(uint32_t);
			anim->ch_rot_cnt = *(uint32_t*)data;
			data += sizeof(uint32_t);
			anim->ch_scale_cnt = *(uint32_t*)data;
			data += sizeof(uint32_t);
#ifdef VERBOSE
			myprintf("[model_anim_parse] [%u] ch_pos_cnt:%u ch_rot_cnt:%u ch_scale_cnt:%u\n", bi, anim->ch_pos_cnt, anim->ch_rot_cnt, anim->ch_scale_cnt);
#endif

			anim->ch_pos = (anim_pos_t*)data;
			data += anim->ch_pos_cnt*sizeof(anim_pos_t);

			anim->ch_rot.time = (float*)data;
			data += anim->ch_rot_cnt*sizeof(float);

			data += pad_skip_padding(data-start_pos, 16);
			anim->ch_rot.rot = (versors*)data;
			data += anim->ch_rot_cnt*sizeof(versors);

			anim->ch_scale = (anim_scale_t*)data;
			data += anim->ch_scale_cnt*sizeof(anim_scale_t);
		}
	}

	return data;
}

wad_t wad = {0};

#if 0
static int load_font(wad_t* wad, font_t* font, int font_height, int font_space, const int8_t* const font_y, const WAD_IDX idx) {
	myprintf("[load_font] idx:%d\n", idx);
	font->font_height = font_height;
	font->space_width = font_space;
	memcpy(font->font_y, font_y, GLYPH_TOTAL);

	size_t malloc_size = 0;
	for (size_t i=0; i<GLYPH_TOTAL; i++) {
		malloc_size += g_wad[idx+i].size;
	}
	char* data = (char*)malloc(malloc_size);
	char* reader = data;

	for (size_t i=0; i<GLYPH_TOTAL; i++) {
		const int ret = wad_get(wad, idx+i, reader);
		if (ret)
			return ret;
		font->glyph[i] = (px_t*)reader;
		reader += g_wad[idx+i].size;
	}

	return 0;
}
#endif

static void load_font(const char* const data, font_t* font, int font_height, int font_space, const int8_t* const font_y, const WAD_IDX idx) {
#ifndef NDEBUG
	myprintf("[load_font] idx:%d\n", idx);
#endif
	font->font_height = font_height;
	font->space_width = font_space;
	memcpy(font->font_y, font_y, GLYPH_TOTAL);
	for (size_t i=0; i<GLYPH_TOTAL; i++) {
		font->glyph[i] = (const px_t*)get_data(data, idx+i);
#ifdef VERBOSE
		myprintf("[FONT] [%d] %dx%d\n", idx+i, font->glyph[i]->h, font->glyph[i]->w);
#endif
	}
}

int assets_load_level(assets_t* const assets, alloc_t* const alloc, int level_num) {
	level_num *= 2; // assets are sorted BSP/LUX...
	if (bsp_parse((char*)get_data(assets->data, BSP_FILES[level_num]), &assets->map, (const bsp_lux_t*)get_data(assets->data, BSP_FILES[level_num+1]), assets, alloc))
		return 1;
	return 0;
}

static inline const px_t* parse_px(assets_t *assets, const WAD_IDX idx) {
#ifdef VERBOSE
	myprintf("[parse_px] assets:0x%p\n", assets);
#endif
	const px_t* const px = (const px_t*)get_data(assets->data, idx);
#ifdef VERBOSE
	myprintf("[parse_px] assets:0x%p px:0x%p\n", assets, px);
#endif
	return px;
}

int assets_init(assets_t* const assets, alloc_t* const alloc) {
#ifndef NDEBUG
	myprintf("[assets_init] assets_ptr:0x%lx\n", assets);
#endif
	int ret = wad_init(&wad, "data.wad");
	if (ret) return ret;

	char* data = mymalloc(WAD_GLOB_SIZE);
	if (data == NULL) {
		myprintf("[ERR] [assets_init] FAILED TO MALLOC bytes:%lu\n", WAD_GLOB_SIZE);
		return 1;
	}
	wad_get_glob(&wad, data);

	assets->data = data;

	/* Load Px */
	for (uint32_t i=0; i<PX_RGB_TOTAL; i++) {
		assets->px_rgb[i] = parse_px(assets, PX_RGB_FILES[i]);
	}
	for (uint32_t i=0; i<PX_GRAY_TOTAL; i++) {
		assets->px_gray[i] = parse_px(assets, PX_GRAY_FILES[i]);
	}
	for (uint32_t i=0; i<PX_PAL_TOTAL; i++) {
		assets->px_pal[i] = parse_px(assets, PX_PAL_FILES[i]);
	}

	/* Load Static Models */
	for (uint32_t i=0; i<MODELS_STATIC_TOTAL; i++) {
		if (!parse_model_basic(get_data(assets->data, MODELS_STATIC_FILES[i]), &assets->models_basic[i], assets, alloc, 0)) {
			return 1;
		}
	}

	/* Load Animated Models */
	for (uint32_t i=0; i<MODELS_ANIM_TOTAL; i++) {
		if (!model_anim_parse(get_data(assets->data, MODELS_ANIM_FILES[i]), &assets->models_anim[i], assets, alloc))
			return 1;
	}

	/* Load Sounds */
	load_notes(assets->data, assets->snds);

	/* Transform BSP */
	for (int i=0; i<BSP_TOTAL/2; i++) {
		bsp_transform((char*)get_data(assets->data, BSP_FILES[i*2]));
	}

	/* Matchup */
	const int8_t matchup_y[GLYPH_TOTAL] = {1,1,3,1,3,3,1,1,1,4,4,8,5,8,3,2,2,2,2,2,2,2,2,2,2,5,6,4,4,4,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,3,1,0,9,1,4,2,4,2,4,2,4,2,2,2,2,2,4,4,4,4,4,4,4,2,4,4,4,4,4,4,1,2,1,3};
	load_font(assets->data, &assets->font_matchup, 12, 6, matchup_y, WAD_FONTS_MATCHUP_033_BIN);

	const int8_t pinzelan_y[GLYPH_TOTAL] = {2,2,4,3,6,4,2,1,1,2,6,17,10,16,3,3,4,4,3,2,3,3,4,3,2,8,8,9,8,8,1,4,3,3,4,4,4,3,4,4,4,3,3,3,4,3,4,2,5,3,3,4,4,4,5,5,4,5,4,3,4,1,19,1,3,3,4,4,4,3,4,4,4,3,3,3,4,3,4,2,5,3,3,4,4,4,5,5,4,5,2,0,2,8};
	load_font(assets->data, &assets->font_pinzelan, 24, 8, pinzelan_y, WAD_FONTS_PINZELAN_033_BIN);

	/* static const int fontHeight = 53; */
	const int8_t pinzelan2x_y[GLYPH_TOTAL] = {4,5,9,7,13,10,5,3,3,4,14,38,23,36,7,7,10,10,7,5,7,6,8,8,6,18,18,19,18,18,3,10,7,7,9,8,10,8,9,10,9,6,6,6,10,6,8,6,10,7,7,9,10,10,11,11,9,10,9,8,9,2,42,2,7,7,9,8,10,8,9,10,9,6,6,6,10,6,8,6,10,7,7,9,10,10,11,11,9,10,4,0,4,17};
	load_font(assets->data, &assets->font_pinzelan2x, 53, 12, pinzelan2x_y, WAD_FONTS_PINZELAN2X_033_BIN);

	wad_free(&wad);
	return 0;
}

int assets_free(assets_t *assets) {
#ifndef NDEBUG
	myprintf("[assets_free] %p\n", assets);
#endif
	myfree((void*)assets->data);
#ifndef NDEBUG
	fputs("[assets_free] done\n", stdout);
#endif
	return 0;
}
