/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <assert.h>

#include "bsp_parse.h"
#include "text.h"
#include "debug.h"
#include "bitarr.h"
#include "utils/myds.h"
#include "utils/pad.h"

#define BSP_SWAP_UP

static inline vec3s swapaxis(vec3s in) {
	vec3s out;
	out.x = in.x;
	out.y = in.z;
	out.z = -in.y;
	return out;
}

static bsp_edge_t parse_edge(const bsp_t* const bsp, int32_t idx) {
	int32_t ledge_idx = bsp->surfedges[idx];
	vec3s v0, v1;
	bsp_edge_t ret_edge;
	vec3s* pv0=&v0;
	vec3s* pv1=&v1;
	uint32_t *vidx0=&ret_edge.vertex0;
	uint32_t *vidx1=&ret_edge.vertex1;
	if (ledge_idx < 0) { // if idx is negative it's reverse order
		ledge_idx = -ledge_idx;
		pv0 = &v1;
		pv1 = &v0;
		vidx0 = &ret_edge.vertex1;
		vidx1 = &ret_edge.vertex0;
	}
	const bsp_edge_t* const edge = &bsp->edges[ledge_idx];
	/* myprintf("[BSP] idx:%d v0:%u v1:%u ledge_idx:%d\n", idx, edge->vertex0, edge->vertex1, ledge_idx); */
	*pv0 = bsp->verts[edge->vertex0];
	*pv1 = bsp->verts[edge->vertex1];
	*vidx0 = edge->vertex0;
	*vidx1 = edge->vertex1;
	return ret_edge;
}

/* Mod_MakeHull0: Duplicate the drawing hull structure as a clipping hull */
static void Mod_MakeHull0(const bsp_t* const bsp, bsp_qmodel_t* const qmod, bsp_clipnode_t* out) {
	const bsp_mnode_t *in, *child;
	int i, j, count;
	hull_t *hull;

	hull = &qmod->hulls[0];

	in = bsp->nodes;
	count = bsp->node_cnt;

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = bsp->planes;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->plane - bsp->planes;
		for (j = 0; j < 2; j++) {
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - bsp->nodes;
#ifdef VERBOSE2
			myprintf("[HULL] [%d/%d] plane:%d child:%d\n", i, j, out->planenum, out->children[j]);
#endif
		}
	}
}

/* COM_Parse: Parse a token out of a string */
static char com_token[1024]; // TODO something with this
static const char *COM_Parse (const char *data) {
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0)
			return NULL; // end of file
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// skip /*..*/ comments
	if (c == '/' && data[1] == '*') {
		data += 2;
		while (*data && !(*data == '*' && data[1] == '/'))
			data++;
		if (*data)
			data += 2;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			if ((c = *data) != 0)
				++data;
			if (c == '\"' || !c) {
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ':') {
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		/* commented out the check for ':' so that ip:port works */
		if (c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' /* || c == ':' */)
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

void bsp_parse_entities(bsp_t *bsp, const char* entities) {
	arrsetlen(bsp->entities, 0);
	bsp_entity_t ent = {0};
	ent.model = -1;
	char key[128], value[4096];
	const char *data;

	data = COM_Parse(entities);
	if (!data)
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1) {
		data = COM_Parse(data);
		if (!data)
			return;

		if (com_token[0] == '{') {
			ent.origin = (vec3s){0};
			ent.model = -1;
			ent.light = 300;
			ent.speed = 0;
			ent.range = 0;
			ent.angle = 0;
			ent.task = 0;
			ent.target[0] = 0;
			ent.targetname[0] = 0;
			ent.message[0] = 0;
			continue;
		} else if (com_token[0] == '}') {
#ifndef NDEBUG
			myprintf("[bsp_parse_entities] [END ENTRY] class:%d model:%d origin:%.0f %.0f %.0f\n", ent.classname, ent.model, ent.origin.x, ent.origin.y, ent.origin.z);
#endif
			if (ent.classname >= 0)
				arrput(bsp->entities, ent);
			continue;
		} else if (com_token[0] == '_') {
			strncpy(key, com_token + 1, sizeof(key)-1);
			key[sizeof(key)-1] = '\0';
		} else {
			strncpy(key, com_token, sizeof(key)-1);
			key[sizeof(key)-1] = '\0';
		}
		while (key[0] && key[strlen (key) - 1] == ' ') // remove trailing spaces
			key[strlen (key) - 1] = 0;
		data = COM_Parse (data);
		if (!data)
			return; // error
		strncpy(value, com_token, sizeof(value)-1);
		value[sizeof(value)-1] = '\0';
#ifndef NDEBUG
		myprintf("[bsp_parse_entities] key:%s value:%s\n", key, value);
#endif
		if (!strcmp(key, "classname")) {
			if (!strcmp(value, "info_player_start")) {
				ent.classname = BSP_ENTITY_START;
			} else if (!strcmp(value, "mob_chud")) {
				ent.classname = BSP_ENTITY_MOB_CHUD;
			} else if (!strcmp(value, "mob_mutt")) {
				ent.classname = BSP_ENTITY_MOB_MUTT;
			} else if (!strcmp(value, "mob_troon")) {
				ent.classname = BSP_ENTITY_MOB_TROON;
			} else if (!strcmp(value, "light")) {
				ent.classname = BSP_ENTITY_LIGHT;
			} else if (!strcmp(value, "light_dynamic")) {
				ent.classname = BSP_ENTITY_LIGHT_DYNAMIC;
			} else if (!strcmp(value, "particle_emitter")) {
				ent.classname = BSP_ENTITY_PARTICLE_EMITTER;
			} else if (!strcmp(value, "spawner")) {
				ent.classname = BSP_ENTITY_SPAWNER;
			} else if (!strcmp(value, "trigger_multiple")) {
				ent.classname = BSP_ENTITY_TRIGGER_MULTIPLE;
			} else if (!strcmp(value, "trigger_once")) {
				ent.classname = BSP_ENTITY_TRIGGER_ONCE;
			} else if (!strcmp(value, "trigger_talk")) {
				ent.classname = BSP_ENTITY_TRIGGER_TALK;
			} else if (!strcmp(value, "trigger_changelevel")) {
				ent.classname = BSP_ENTITY_TRIGGER_CHANGELEVEL;
			} else if (!strcmp(value, "func_button")) {
				ent.classname = BSP_ENTITY_BUTTON;
			} else if (!strcmp(value, "func_door")) {
				ent.classname = BSP_ENTITY_DOOR;
			} else if (!strcmp(value, "path_corner")) {
				ent.classname = BSP_ENTITY_PATH_CORNER;
			} else if (!strcmp(value, "weapon_lightning")) {
				ent.classname = BSP_ENTITY_WEAPON_BEAM;
			} else if (!strcmp(value, "weapon_supershotgun")) {
				ent.classname = BSP_ENTITY_WEAPON_SHOTGUN;
			} else if (!strcmp(value, "weapon_nailgun")) {
				ent.classname = BSP_ENTITY_WEAPON_SMG;
			} else {
				ent.classname = -1;
			}
		} else if (strcmp(key, "model")==0) {
#ifndef NDEBUG
			myprintf("[bsp_parse_entities] model: %s\n", value);
#endif
			ent.model = myatoi(value+1);
		} else if (strcmp(key, "origin")==0) {
			/* extract vec3, y and z are swapped */
			const char* p = value;
			ent.origin.x = (float)myatoi(p) / BSP_RESIZE_DIV;
			do { p++; } while (*p != ' '); p++;
			ent.origin.z = (float)-myatoi(p) / BSP_RESIZE_DIV;
			do { p++; } while (*p != ' '); p++;
			ent.origin.y = (float)myatoi(p) / BSP_RESIZE_DIV;
		} else if (strcmp(key, "light")==0) {
			ent.light = myatoi(value);
		} else if (strcmp(key, "speed")==0) {
			ent.speed = myatoi(value);
		} else if (strcmp(key, "range")==0) {
			ent.range = myatoi(value);
		} else if (strcmp(key, "angle")==0) {
			ent.angle = myatoi(value);
		} else if (strcmp(key, "spawnflags")==0) {
			ent.flags = myatoi(value);
		} else if (strcmp(key, "task")==0) {
			ent.task = myatoi(value);
		} else if (strcmp(key, "message")==0) {
			strncpy(ent.message, value, 31);
			ent.message[31] = 0;
		} else if (strcmp(key, "target")==0) {
			strncpy(ent.target, value, 31);
			ent.target[31] = 0;
		} else if (strcmp(key, "targetname")==0) {
			strncpy(ent.targetname, value, 31);
			ent.targetname[31] = 0;
		}
	}
}

static void CalcSurfaceExtents(bsp_t *bsp, bsp_msurface_t *s, const bsp_face_t* const face) {
	float mins[2], maxs[2], val;
	int e;
	const vec3s *v;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = FLT_MAX;
	maxs[0] = maxs[1] = -FLT_MAX;

	const bsp_dtexinfo_t* const tex = &bsp->texinfo[face->texinfo_id];

	/* TODO unsure if I actually need to do the axis flip for this */
	mat3s bsp_to_world={.col={(vec3s){.x=1,.y=0,.z=0},(vec3s){.x=0,.y=0,.z=-1},(vec3s){.x=0,.y=1,.z=0}}};
	const double tex_vecs[2][4] = {
		{tex->vectorS.x, tex->vectorS.y, tex->vectorS.z, tex->distS},
		{tex->vectorT.x, tex->vectorT.y, tex->vectorT.z, tex->distT},
	};

	for (int i=0; i<face->ledge_num; i++) {
		e = bsp->surfedges[face->ledge_id + i];
		if (e >= 0)
			v = &bsp->verts[bsp->edges[e].vertex0];
		else
			v = &bsp->verts[bsp->edges[-e].vertex1];

		for (int j=0; j<2; j++) {
			/* The following calculation is sensitive to floating-point
			 * precision.  It needs to produce the same result that the
			 * light compiler does, because R_BuildLightMap uses surf->
			 * extents to know the width/height of a surface's lightmap,
			 * and incorrect rounding here manifests itself as patches
			 * of "corrupted" looking lightmaps.
			 * Most light compilers are win32 executables, so they use
			 * x87 floating point.  This means the multiplies and adds
			 * are done at 80-bit precision, and the result is rounded
			 * down to 32-bits and stored in val.
			 * Adding the casts to double seems to be good enough to fix
			 * lighting glitches when Quakespasm is compiled as x86_64
			 * and using SSE2 floating-point.  A potential trouble spot
			 * is the hallway at the beginning of mfxsp17.  -- ericw
			 */
			vec3s vec = glms_mat3_mulv(bsp_to_world, *v);
			val = ((double)vec.x * tex_vecs[j][0]) + ((double)vec.y * tex_vecs[j][1]) + ((double)vec.z * tex_vecs[j][2]) + tex_vecs[j][3];

			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (int i=0; i<2; i++) {
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}

typedef enum {
	MATERIAL_ANIMUTV,
	MATERIAL_BOOKS0,
	MATERIAL_BRICKS,
	MATERIAL_BR_WALL0,
	MATERIAL_CHECKERFLOOR,
	MATERIAL_CLOCK0,
	MATERIAL_CONSOLE0,
	MATERIAL_DOOR0,
	MATERIAL_DOOR1,
	MATERIAL_DPLATE006C,
	MATERIAL_ERROR0,
	MATERIAL_FABRIC0020,
	MATERIAL_FABRIC025,
	MATERIAL_KEYBOARD,
	MATERIAL_LAIN0,
	MATERIAL_LITME0,
	MATERIAL_LIGHT0,
	MATERIAL_METAL0076,
	MATERIAL_METAL0086,
	MATERIAL_METAL022,
	MATERIAL_METALPLATE0,
	MATERIAL_METALPLATES001,
	MATERIAL_METALPLATES009,
	MATERIAL_MONITOR0,
	MATERIAL_NAMEPLATE0,
	MATERIAL_NOSLEEP,
	MATERIAL_PAINTMETAL005,
	MATERIAL_PANEL0,
	MATERIAL_PAVINGSTONE128,
	MATERIAL_PIXELMUTT,
	MATERIAL_PLASTIC0018,
	MATERIAL_RACK_FRONT,
	MATERIAL_SCIFI0,
	MATERIAL_SCRATCH0,
	MATERIAL_SCUM_WIGGLER,
	MATERIAL_SOY0,
	MATERIAL_TACTILE003,
	MATERIAL_TILES0094,
	MATERIAL_TRASHBIN0_FR,
	MATERIAL_TRASHBIN0_SIDE,
	MATERIAL_TRIPPY0,
	MATERIAL_WAVES0,
	MATERIAL_WIRES0,
	MATERIAL_WOOD066,
	MATERIAL_TOTAL,
} MATERIAL;

static const i16 ANIMU_TEX_DIFF[] = {PX_GRAY_TEX_ANIMUTV0,PX_GRAY_TEX_ANIMUTV1,PX_GRAY_TEX_ANIMUTV2,PX_GRAY_TEX_ANIMUTV3};
static const i16 BOOKS0_TEX_DIFF[] = {PX_GRAY_TEX_BOOKS0};
static const i16 BRICKS_TEX_DIFF[] = {PX_GRAY_TEX_BRICKS};
static const i16 BR_WALL0_TEX_DIFF[] = {PX_GRAY_TEX_BR_WALL0};
static const i16 CHECKERFLOOR_TEX_DIFF[] = {PX_GRAY_TEX_CHECKERFLOOR};
static const i16 CLOCK0_TEX_DIFF[] = {PX_GRAY_TEX_CLOCK0};
static const i16 CONSOLE0_TEX_DIFF[] = {PX_GRAY_TEX_CONSOLE0_0,PX_GRAY_TEX_CONSOLE0_1,PX_GRAY_TEX_CONSOLE0_2,PX_GRAY_TEX_CONSOLE0_3,PX_GRAY_TEX_CONSOLE0_4,PX_GRAY_TEX_CONSOLE0_5};
static const i16 DOOR0_TEX_DIFF[] = {PX_GRAY_TEX_DOOR0};
static const i16 DOOR1_TEX_DIFF[] = {PX_GRAY_TEX_DOOR1};
static const i16 DPLATE006C_TEX_DIFF[] = {PX_GRAY_TEX_DPLATE006C};
static const i16 ERROR0_TEX_DIFF[] = {PX_GRAY_TEX_ERROR0_0,PX_GRAY_TEX_ERROR0_1,PX_GRAY_TEX_ERROR0_2,PX_GRAY_TEX_ERROR0_3,PX_GRAY_TEX_ERROR0_4,PX_GRAY_TEX_ERROR0_5,PX_GRAY_TEX_ERROR0_6,PX_GRAY_TEX_ERROR0_7,PX_GRAY_TEX_ERROR0_8,PX_GRAY_TEX_ERROR0_9,PX_GRAY_TEX_ERROR0_10,PX_GRAY_TEX_ERROR0_11,PX_GRAY_TEX_ERROR0_12,PX_GRAY_TEX_ERROR0_13,PX_GRAY_TEX_ERROR0_14,PX_GRAY_TEX_ERROR0_15};
static const i16 FABRIC0020_TEX_DIFF[] = {PX_GRAY_TEX_FABRIC0020};
static const i16 FABRIC025_TEX_DIFF[] = {PX_GRAY_TEX_FABRIC025};
static const i16 KEYBOARD_TEX_DIFF[] = {PX_GRAY_TEX_KEYBOARD};
static const i16 LAIN0_TEX_DIFF[] = {PX_GRAY_TEX_LAIN0_0,PX_GRAY_TEX_LAIN0_1,PX_GRAY_TEX_LAIN0_2,PX_GRAY_TEX_LAIN0_3,PX_GRAY_TEX_LAIN0_4,PX_GRAY_TEX_LAIN0_5};
static const i16 LITME0_TEX_DIFF[] = {PX_GRAY_TEX_LITME0};
static const i16 LIGHT0_TEX_DIFF[] = {PX_GRAY_TEX_LIGHT0};
static const i16 METAL0076_TEX_DIFF[] = {PX_GRAY_TEX_METAL0076};
static const i16 METAL0068_TEX_DIFF[] = {PX_GRAY_TEX_METAL0068};
static const i16 METAL022_TEX_DIFF[] = {PX_GRAY_TEX_METAL022};
static const i16 METALPLATE0_TEX_DIFF[] = {PX_GRAY_TEX_METALPLATE0};
static const i16 METALPLATES001_TEX_DIFF[] = {PX_GRAY_TEX_METALPLATES001};
static const i16 METALPLATES009_TEX_DIFF[] = {PX_GRAY_TEX_METALPLATES009};
static const i16 MONITOR0_TEX_DIFF[] = {PX_GRAY_TEX_MONITOR0};
static const i16 NAMEPLATE0_TEX_DIFF[] = {PX_GRAY_TEX_NAMEPLATE0};
static const i16 NOSLEEP_TEX_DIFF[] = {PX_GRAY_TEX_NOSLEEP0,PX_GRAY_TEX_NOSLEEP1,PX_GRAY_TEX_NOSLEEP2,PX_GRAY_TEX_NOSLEEP3,PX_GRAY_TEX_NOSLEEP4,PX_GRAY_TEX_NOSLEEP5,PX_GRAY_TEX_NOSLEEP6,PX_GRAY_TEX_NOSLEEP7,PX_GRAY_TEX_NOSLEEP8,PX_GRAY_TEX_NOSLEEP9};
static const i16 PAINTMETAL005_TEX_DIFF[] = {PX_GRAY_TEX_PAINTMETAL005};
static const i16 PANEL0_TEX_DIFF[] = {PX_GRAY_TEX_PANEL0};
static const i16 PAVINGSTONE128_TEX_DIFF[] = {PX_GRAY_TEX_PAVINGSTONE128};
static const i16 PIXELMUTT_TEX_DIFF[] = {PX_GRAY_TEX_PIXELMUTT0,PX_GRAY_TEX_PIXELMUTT1,PX_GRAY_TEX_PIXELMUTT2,PX_GRAY_TEX_PIXELMUTT3,PX_GRAY_TEX_PIXELMUTT4,PX_GRAY_TEX_PIXELMUTT5,PX_GRAY_TEX_PIXELMUTT6,PX_GRAY_TEX_PIXELMUTT7,PX_GRAY_TEX_PIXELMUTT8,PX_GRAY_TEX_PIXELMUTT9,PX_GRAY_TEX_PIXELMUTT10,PX_GRAY_TEX_PIXELMUTT11,PX_GRAY_TEX_PIXELMUTT12,PX_GRAY_TEX_PIXELMUTT13,PX_GRAY_TEX_PIXELMUTT14,PX_GRAY_TEX_PIXELMUTT15,PX_GRAY_TEX_PIXELMUTT16,PX_GRAY_TEX_PIXELMUTT17,PX_GRAY_TEX_PIXELMUTT18,PX_GRAY_TEX_PIXELMUTT19,PX_GRAY_TEX_PIXELMUTT20};
static const i16 PLASTIC0018_TEX_DIFF[] = {PX_GRAY_TEX_PLASTIC0018};
static const i16 RACK_FRONT_TEX_DIFF[] = {PX_GRAY_TEX_RACK_FRONT};
static const i16 SCIFI0_TEX_DIFF[] = {PX_GRAY_TEX_SCIFI0};
static const i16 SCRATCH0_TEX_DIFF[] = {PX_GRAY_TEX_SCRATCH0};
static const i16 SCUM_WIGGLER_TEX_DIFF[] = {PX_GRAY_TEX_SCUM_WIGGLER0,PX_GRAY_TEX_SCUM_WIGGLER1,PX_GRAY_TEX_SCUM_WIGGLER2,PX_GRAY_TEX_SCUM_WIGGLER3,PX_GRAY_TEX_SCUM_WIGGLER4};
static const i16 SOY0_TEX_DIFF[] = {PX_GRAY_TEX_SOY0};
static const i16 TACTILE003_TEX_DIFF[] = {PX_GRAY_TEX_TACTILE003};
static const i16 TILES0094_TEX_DIFF[] = {PX_GRAY_TEX_TILES0094};
static const i16 TRASHBIN0_FR_TEX_DIFF[] = {PX_GRAY_TEX_TRASHBIN0_FR};
static const i16 TRASHBIN0_SIDE_TEX_DIFF[] = {PX_GRAY_TEX_TRASHBIN0_SIDE};
static const i16 TRIPPY0_TEX_DIFF[] = {PX_GRAY_TEX_TRIPPY0};
static const i16 WAVES0_TEX_DIFF[] = {PX_GRAY_TEX_WAVES0};
static const i16 WIRES0_TEX_DIFF[] = {PX_GRAY_TEX_WIRES0};
static const i16 WOOD066_TEX_DIFF[] = {PX_GRAY_TEX_WOOD066};

static const material_info_t* lookup_material(const char* const material) {
	static const material_info_t MATERIAL_DATA[MATERIAL_TOTAL] = {
		{"ANIMUTV", ANIMU_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 4},
		{"BOOKS0", BOOKS0_TEX_DIFF, PX_RGB_NMAP_BOOKS0, 0, 1},
		{"BRICKS", BRICKS_TEX_DIFF, PX_RGB_NMAP_BRICKS, 0, 1},
		{"BR_WALL0", BR_WALL0_TEX_DIFF, PX_RGB_NMAP_BR_WALL0, 0, 1},
		{"CHECKERFLOOR", CHECKERFLOOR_TEX_DIFF, PX_RGB_NMAP_CHECKERFLOOR, 0, 1},
		{"CLOCK0", CLOCK0_TEX_DIFF, PX_RGB_NMAP_CLOCK0, 0, 1},
		{"CONSOLE0", CONSOLE0_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 6},
		{"DOOR0", DOOR0_TEX_DIFF, PX_RGB_NMAP_DOOR0, 0, 1},
		{"DOOR1", DOOR1_TEX_DIFF, PX_RGB_NMAP_DOOR1, 0, 1},
		{"DPLATE006C", DPLATE006C_TEX_DIFF, PX_RGB_NMAP_DPLATE006C, 0, 1},
		{"ERROR0", ERROR0_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 16},
		{"FABRIC0020", FABRIC0020_TEX_DIFF, PX_RGB_NMAP_FABRIC0020, 0, 1},
		{"FABRIC025", FABRIC025_TEX_DIFF, PX_RGB_NMAP_FABRIC025, 0, 1},
		{"KEYBOARD", KEYBOARD_TEX_DIFF, PX_RGB_NMAP_KEYBOARD, 0, 1},
		{"LAIN0", LAIN0_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 6},
		{"LITME0", LITME0_TEX_DIFF, PX_RGB_NMAP_DEFAULT, 0, 1},
		{"LIGHT0", LIGHT0_TEX_DIFF, PX_RGB_NMAP_LIGHT0, 1, 1},
		{"METAL0076", METAL0076_TEX_DIFF, PX_RGB_NMAP_METAL0076, 0, 1},
		{"METAL0068", METAL0068_TEX_DIFF, PX_RGB_NMAP_METAL0068, 0, 1},
		{"METAL022", METAL022_TEX_DIFF, PX_RGB_NMAP_METAL022, 0, 1},
		{"METALPLATE0", METALPLATE0_TEX_DIFF, PX_RGB_NMAP_METALPLATE0, 0, 1},
		{"METALPLATES001", METALPLATES001_TEX_DIFF, PX_RGB_NMAP_METALPLATES001, 0, 1},
		{"METALPLATES009", METALPLATES009_TEX_DIFF, PX_RGB_NMAP_METALPLATES009, 0, 1},
		{"MONITOR0", MONITOR0_TEX_DIFF, PX_RGB_NMAP_MONITOR0, 0, 1},
		{"NAMEPLATE0", NAMEPLATE0_TEX_DIFF, PX_RGB_NMAP_NAMEPLATE0, 0, 1},
		{"NOSLEEP", NOSLEEP_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 10},
		{"PAINTMETAL005", PAINTMETAL005_TEX_DIFF, PX_RGB_NMAP_PAINTMETAL005, 0, 1},
		{"PANEL0", PANEL0_TEX_DIFF, PX_RGB_NMAP_PANEL0, 0, 1},
		{"PAVINGSTONE128", PAVINGSTONE128_TEX_DIFF, PX_RGB_NMAP_PAVINGSTONE128, 0, 1},
		{"PIXELMUTT", PIXELMUTT_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 21},
		{"PLASTIC0018", PLASTIC0018_TEX_DIFF, PX_RGB_NMAP_PLASTIC0018, 0, 1},
		{"RACK_FRONT", RACK_FRONT_TEX_DIFF, PX_RGB_NMAP_RACK_FRONT, 0, 1},
		{"SCIFI0", SCIFI0_TEX_DIFF, PX_RGB_NMAP_SCIFI0, 0, 1},
		{"SCRATCH0", SCRATCH0_TEX_DIFF, PX_RGB_NMAP_SCRATCH0, 0, 1},
		{"SCUM_WIGGLER", SCUM_WIGGLER_TEX_DIFF, PX_RGB_NMAP_CRT, 0, 5},
		{"SOY0", SOY0_TEX_DIFF, PX_RGB_NMAP_SOY0, 0, 1},
		{"TACTILE003", TACTILE003_TEX_DIFF, PX_RGB_NMAP_TACTILE003, 0, 1},
		{"TILES0094", TILES0094_TEX_DIFF, PX_RGB_NMAP_TILES0094, 0, 1},
		{"TRASHBIN0_FR", TRASHBIN0_FR_TEX_DIFF, PX_RGB_NMAP_TRASHBIN0_FR, 0, 1},
		{"TRASHBIN0_SIDE", TRASHBIN0_SIDE_TEX_DIFF, PX_RGB_NMAP_TRASHBIN0_SIDE, 0, 1},
		{"TRIPPY0", TRIPPY0_TEX_DIFF, PX_RGB_NMAP_TRIPPY0, 0, 1},
		{"WAVES0", WAVES0_TEX_DIFF, PX_RGB_NMAP_WAVES0, 0, 1},
		{"WIRES0", WIRES0_TEX_DIFF, PX_RGB_NMAP_WIRES0, 0, 1},
		{"WOOD066", WOOD066_TEX_DIFF, PX_RGB_NMAP_WOOD066, 0, 1},
	};

	for (uint32_t i=0; i<MATERIAL_TOTAL; i++) {
		if (strcmp(material, MATERIAL_DATA[i].name) == 0) {
			return &MATERIAL_DATA[i];
		}
	}
#ifndef NDEBUG
	myprintf("[WARN] [lookup_material] invalid material: %s\n", material);
#endif
	return NULL;
}

static void bsp_create_hulls(const bsp_t* const bsp, bsp_qmodel_t* const qmod, bsp_clipnode_t* hull0) {
	hull_t *hull;
	hull = &qmod->hulls[1];
	hull->clipnodes = bsp->clipnodes;
	hull->lastclipnode = bsp->clipnode_cnt - 1;
	hull->planes = bsp->planes;
	hull->clip_mins.x = -16.0f/BSP_RESIZE_DIV;
	hull->clip_mins.y = -24.0f/BSP_RESIZE_DIV;
	hull->clip_mins.z = -16.0f/BSP_RESIZE_DIV;
	hull->clip_maxs.x = 16.0f/BSP_RESIZE_DIV;
	hull->clip_maxs.y = 32.0f/BSP_RESIZE_DIV;
	hull->clip_maxs.z = 16.0f/BSP_RESIZE_DIV;

	hull = &qmod->hulls[2];
	hull->clipnodes = bsp->clipnodes;
	hull->lastclipnode = bsp->clipnode_cnt - 1;
	hull->planes = bsp->planes;
	hull->clip_mins.x = -16.0f/BSP_RESIZE_DIV;
	hull->clip_mins.y = -24.0f/BSP_RESIZE_DIV;
	hull->clip_mins.z = -16.0f/BSP_RESIZE_DIV;
	hull->clip_maxs.x = 16.0f/BSP_RESIZE_DIV;
	hull->clip_maxs.y = 32.0f/BSP_RESIZE_DIV;
	hull->clip_maxs.z = 16.0f/BSP_RESIZE_DIV;

	Mod_MakeHull0(bsp, qmod, hull0);
}

/* bsp_extract_textures requires msurfs */
static int bsp_extract_textures(bsp_t* const bsp, const assets_t* const assets, alloc_t* const alloc, const int qidx) {
	bsp_qmodel_t* const qmod = &bsp->qmods[qidx];
#ifndef NDEBUG
	myprintf("[bsp_extract_textures] qidx:%d firstsurf:%d numsurf:%d\n", qidx, qmod->firstmodelsurface, qmod->nummodelsurfaces);
#endif

	/* Clear Model Memory */
	/* memset(&qmod->model, 0, sizeof(model_basic_t)); */
	for (u32 i=0; i<bsp->miptex_cnt; i++)
		qmod->miptex_to_mesh_lookup[i] = -1;

	{
		/* Count Model Meshes */
		const bsp_msurface_t* const first_surf = &bsp->msurfs[qmod->firstmodelsurface];
		u32 mesh_cnt = 0;
		for (i32 i=0; i<qmod->nummodelsurfaces; i++) {
			const bsp_msurface_t* const msurf = &first_surf[i];
			const size_t bsp_tex_id = msurf->texinfo->texture_id;
			const bsp_miptex_t* const miptex = (const bsp_miptex_t*)((const char*)bsp->miptex_headers+bsp->miptex_headers->dataofs[bsp_tex_id]);
#ifdef VERBOSE
			myprintf("[bsp_extract_textures] [miptex] surf:%d name:%s w:%u h:%u offset1:%u bsp_tex_id:%zu msurf:0x%p\n", i, miptex->name, miptex->width, miptex->height, miptex->offset1, bsp_tex_id, msurf);
#endif

			/* skip if mesh already counted */
			const i32 mesh_idx = qmod->miptex_to_mesh_lookup[bsp_tex_id];
			if (mesh_idx >= 0)
				continue;

			/* lookup material from texture name */
			char uppername[16];
			for (int32_t si=0; si<15; si++) {
				uppername[si] = mytoupper(miptex->name[si]);
			}
			uppername[15] = '\0';
			const material_info_t* const mat_info = lookup_material(uppername);

			/* skip if material is invalid */
			if (!mat_info)
				continue;

			/* get mesh position */
			const size_t new_mesh_idx = mesh_cnt++;
#ifndef NDEBUG
			myprintf("[bsp_extract_textures] [new mat] surf:%d name:%s diffuse:%d norm:%d shader_idx:%d mesh_idx:%zu bsp_tex_id:%zu\n", i, mat_info->name, mat_info->diffuse, mat_info->normal,  mat_info->shader_idx, new_mesh_idx, bsp_tex_id);
#endif
			/* set miptex to mesh lookup */
			qmod->miptex_to_mesh_lookup[bsp_tex_id] = new_mesh_idx;
		}
#ifdef VERBOSE
		for (u32 i=0; i<bsp->miptex_cnt; i++) {
			myprintf("[bsp_extract_textures] [lookup] miptex_idx:%u mesh_idx:%d\n", i, qmod->miptex_to_mesh_lookup[i]);
		}
#endif

		/* Clear old diff allocs */
		for (u32 i=0; i<qmod->model.mesh_cnt; i++) {
			if (qmod->model.meshes[i].tex_diffuse)
				bfree(alloc, qmod->model.meshes[i].tex_diffuse);
		}

		/* Alloc Meshes */
		if (mesh_cnt > MAX_MESH_CNT) {
			/* TODO figure out dynamically allocating visflags to remove this */
			assert(mesh_cnt < MAX_MESH_CNT);
			return 1;
		}
		if (mesh_cnt > qmod->model.mesh_cnt) {
			if (qmod->model.meshes)
				bfree(alloc, qmod->model.meshes);
			qmod->model.meshes = balloc(alloc, sizeof(mesh_t)*mesh_cnt);
			memset(qmod->model.meshes, 0, sizeof(mesh_t)*mesh_cnt);
			if (qmod->model.meshes == NULL) {
				myprintf("[ERR] [bsp_extract_textures] can't alloc\n");
				return 1;
			}
		}
		qmod->model.mesh_cnt = mesh_cnt;
	}

	memset(qmod->model.meshes, 0, sizeof(mesh_t)*qmod->model.mesh_cnt);

	/* we reset all this so I don't have to alloc a reverse miptex lookup table, it should (lmao) be deterministic */
	qmod->model.mesh_cnt = 0;
	for (u32 i=0; i<bsp->miptex_cnt; i++)
		qmod->miptex_to_mesh_lookup[i] = -1;

	{
		/* Extract Model Textures */
		const bsp_msurface_t* const first_surf = &bsp->msurfs[qmod->firstmodelsurface];
		const bsp_face_t* const first_face = &bsp->faces[qmod->firstmodelsurface];
		for (i32 i=0; i<qmod->nummodelsurfaces; i++) {
			const bsp_msurface_t* const msurf = &first_surf[i];
			const bsp_face_t* const face = &first_face[i];
			const size_t bsp_tex_id = msurf->texinfo->texture_id;
			const bsp_miptex_t* const miptex = (const bsp_miptex_t*)((const char*)bsp->miptex_headers+bsp->miptex_headers->dataofs[bsp_tex_id]);
#ifdef VERBOSE
			myprintf("[bsp_extract_textures] [miptex] surf:%d name:%s w:%u h:%u offset1:%u bsp_tex_id:%zu msurf:0x%p\n", i, miptex->name, miptex->width, miptex->height, miptex->offset1, bsp_tex_id, msurf);
#endif

			/* skip if mesh already counted */
			const i32 mesh_idx = qmod->miptex_to_mesh_lookup[bsp_tex_id];
			if (mesh_idx >= 0) {
				/* count triangles for allocation */
				const int tris = face->ledge_num-2;
				qmod->model.meshes[mesh_idx].face_cnt += tris;
				continue;
			}

			/* lookup material from texture name */
			char uppername[16];
			for (int32_t si=0; si<15; si++) {
				uppername[si] = mytoupper(miptex->name[si]);
			}
			uppername[15] = '\0';
			const material_info_t* const mat_info = lookup_material(uppername);

			/* skip if material is invalid */
			if (!mat_info)
				continue;

			/* get mesh position */
			const size_t new_mesh_idx = qmod->model.mesh_cnt++;
#ifndef NDEBUG
			myprintf("[bsp_extract_textures] [new mat] surf:%d name:%s diffuse:%d norm:%d shader_idx:%d mesh_idx:%zu bsp_tex_id:%zu\n", i, mat_info->name, mat_info->diffuse, mat_info->normal,  mat_info->shader_idx, new_mesh_idx, bsp_tex_id);
#endif
			/* set miptex to mesh lookup */
			qmod->miptex_to_mesh_lookup[bsp_tex_id] = new_mesh_idx;

			/* set texture info */
			qmod->textures[new_mesh_idx].width = miptex->width;
			qmod->textures[new_mesh_idx].height = miptex->height;
			mesh_t* const mesh = &qmod->model.meshes[new_mesh_idx];
			mesh->tex_diffuse_cnt = mat_info->diff_cnt;
			/* TODO I could roll up these allocs */
			mesh->tex_diffuse = balloc(alloc, sizeof(px_t*)*mesh->tex_diffuse_cnt);
			for (i8 diff_idx=0; diff_idx<mat_info->diff_cnt; diff_idx++) {
				mesh->tex_diffuse[diff_idx] = assets->px_gray[mat_info->diffuse[diff_idx]];
			}
			if (mat_info->normal >= 0)
				mesh->tex_normal = assets->px_rgb[mat_info->normal];
			mesh->mat_type = mat_info->shader_idx;

			/* count triangles for allocation */
			const int tris = face->ledge_num-2;
			qmod->model.meshes[new_mesh_idx].face_cnt += tris;
		}
#ifdef VERBOSE
		for (u32 i=0; i<bsp->miptex_cnt; i++) {
			myprintf("[bsp_extract_textures] [lookup] miptex_idx:%u mesh_idx:%d\n", i, qmod->miptex_to_mesh_lookup[i]);
		}
#endif
	}

	return 0;
}

static void bsp_parse_qmods(bsp_t* const bsp) {
	const i32 clipnode_cnt = bsp->clipnode_cnt - 1;
	for (uint32_t i=0; i<bsp->model_cnt; i++) {
		bsp_dmodel_t* dmodel = &bsp->dmodels[i];
		bsp_qmodel_t* const qmod = &bsp->qmods[i];
		qmod->bbox = dmodel->bbox;
		if (i>0) {
			for (int j=0; j<MAX_MAP_HULLS; j++) {
				qmod->hulls[j] = bsp->qmods[i-1].hulls[j];
			}
		}
		qmod->hulls[0].firstclipnode = dmodel->headnode[0];
		for (int j=1; j<MAX_MAP_HULLS; j++) {
			qmod->hulls[j].firstclipnode = dmodel->headnode[j];
			qmod->hulls[j].lastclipnode = clipnode_cnt;
		}
		qmod->firstmodelsurface = dmodel->firstface;
		qmod->nummodelsurfaces = dmodel->numfaces;
		qmod->numleafs = dmodel->visleafs;
		qmod->nodes = bsp->nodes; // TODO remove?
		qmod->leafs = bsp->leafs; // TODO remove?
		qmod->lightdata = bsp->lightdata;
	}
}

static int bsp_alloc(bsp_t* const bsp) {
#ifndef NDEBUG
	fputs("[bsp_alloc]\n", stdout);
#endif
	/* count */
	size_t malloc_size = 0;
	malloc_size += pad_inc_count(bsp->model_cnt*sizeof(bsp_qmodel_t), 64); // qmods
	malloc_size += pad_inc_count(bsp->leaf_cnt*sizeof(bsp_mleaf_t), 64); // leafs
	malloc_size += pad_inc_count(bsp->face_cnt*sizeof(bsp_msurface_t), 64); // msurfs
	malloc_size += pad_inc_count(bsp->node_cnt*sizeof(bsp_mnode_t), 64); // nodes
	malloc_size += pad_inc_count(bsp->node_cnt*sizeof(bsp_clipnode_t), 64); // hull0_nodes

	for (u32 i=0; i<bsp->model_cnt; i++)
		malloc_size += pad_inc_count(bsp->miptex_cnt*sizeof(bsp_texture_t), 64); // model textures
	/* TODO: real face_cnt */
	for (u32 i=0; i<bsp->model_cnt; i++)
		malloc_size += pad_inc_count(bsp->face_cnt*sizeof(bsp_face_lookup_t), 64); // face_lookup
	for (u32 i=0; i<bsp->model_cnt; i++)
		malloc_size += pad_inc_count(bsp->miptex_cnt*sizeof(i16), 64); // miptex_to_mesh_lookup

#ifndef NDEBUG
	myprintf("[bsp_alloc] malloc_size:%zu\n", malloc_size);
#endif
	/* realloc, basically */
	char* ptr;
	if (bsp->qmods) {
		if (malloc_size > bsp->malloc_size) {
			bsp->malloc_size = malloc_size;
			ptr = myrealloc(bsp->qmods, malloc_size);
			if (ptr == NULL) {
				myprintf("[ERR] can't realloc BSP ptr\n");
				return 1;
			}
		} else {
			ptr = (void*)bsp->qmods;
			memset(ptr, 0, malloc_size);
		}
	} else {
		bsp->malloc_size = malloc_size;
		ptr = mycalloc(malloc_size, 1);
	}
	assert((uintptr_t)ptr%16 == 0);

	bsp->qmods = pad_inc_ptr(&ptr, bsp->model_cnt*sizeof(bsp_qmodel_t), 64);
	bsp->leafs = pad_inc_ptr(&ptr, bsp->leaf_cnt*sizeof(bsp_mleaf_t), 64);
	bsp->msurfs = pad_inc_ptr(&ptr, bsp->face_cnt*sizeof(bsp_msurface_t), 64);
	bsp->nodes = pad_inc_ptr(&ptr, bsp->node_cnt*sizeof(bsp_mnode_t), 64);
	bsp->hull0_nodes = pad_inc_ptr(&ptr, bsp->node_cnt*sizeof(bsp_clipnode_t), 64);

	for (u32 i=0; i<bsp->model_cnt; i++)
		bsp->qmods[i].textures = pad_inc_ptr(&ptr, bsp->miptex_cnt*sizeof(bsp_texture_t), 64);
	/* TODO: reflect true face_cnt */
	for (u32 i=0; i<bsp->model_cnt; i++)
		bsp->qmods[i].face_lookup = pad_inc_ptr(&ptr, bsp->face_cnt*sizeof(bsp_face_lookup_t), 64);
	for (u32 i=0; i<bsp->model_cnt; i++)
		bsp->qmods[i].miptex_to_mesh_lookup = pad_inc_ptr(&ptr, bsp->miptex_cnt*sizeof(i16), 64);

	return 0;
}

static void bsp_parse_header(char* const start, bsp_t* const bsp) {
	memset(bsp, 0, sizeof(bsp_t));
	const bsp_header_t* const data = (const bsp_header_t* const)start;

#ifndef NDEBUG
	myprintf("[BSP] ver:0x%lx\n", (uint8_t)data->version);

	/* Print BSP Entries */
	const bsp_dentry_t* entry = &data->entities;
	for (int i=0; i<15; i++, entry++) {
		myprintf("[BSP] [%d] offset:%d size:%d\n", i, entry->offset, entry->size);
	}
#endif

	/* Setup Entry Index */
	bsp->vert_cnt = data->vertices.size/sizeof(vec3s);
	bsp->plane_cnt = data->planes.size/sizeof(bsp_plane_t);
	bsp->node_cnt = data->nodes.size/sizeof(bsp_dnode_t);
	bsp->clipnode_cnt = data->clipnodes.size/sizeof(bsp_clipnode_t);
	bsp->leaf_cnt = data->leafs.size/sizeof(bsp_dleaf_t);
	bsp->face_cnt = data->faces.size/sizeof(bsp_face_t);
	bsp->edge_cnt = data->edges.size/sizeof(bsp_edge_t);
	bsp->lface_cnt = data->lfaces.size/sizeof(int32_t);
	bsp->surfedge_cnt = data->surfedges.size/sizeof(int32_t);
	bsp->texinfo_cnt = data->texinfo.size/sizeof(bsp_dtexinfo_t);
	bsp->model_cnt = data->models.size/sizeof(bsp_dmodel_t);
	bsp->visdata_cnt = data->visdata.size;
	bsp->verts = (const vec3s*)(start+data->vertices.offset);
	bsp->faces = (const bsp_face_t*)(start+data->faces.offset);
	bsp->planes = (const bsp_plane_t*)(start+data->planes.offset);
	bsp->dnodes = (const bsp_dnode_t*)(start+data->nodes.offset);
	bsp->edges = (const bsp_edge_t*)(start+data->edges.offset);
	bsp->dleafs = (const bsp_dleaf_t*)(start+data->leafs.offset);
	bsp->surfedges = (const int32_t*)(start+data->surfedges.offset);
	bsp->lfaces = (const int32_t*)(start+data->lfaces.offset);
	bsp->visdata = (const uint8_t*)(start+data->visdata.offset);
	bsp->texinfo = (const bsp_dtexinfo_t*)(start+data->texinfo.offset);
	bsp->miptex_headers = (const bsp_mipheader_t*)(start+data->miptex.offset);
	bsp->miptex_cnt = bsp->miptex_headers->numtex;
	assert(bsp->miptex_cnt > 1);
	bsp->clipnodes = (bsp_clipnode_t*)(start+data->clipnodes.offset);
	bsp->dmodels = (bsp_dmodel_t*)(start+data->models.offset);
	bsp->lightdata = (const u8*)(start+data->lightmaps.offset);

	/* Read Entities */
	bsp_parse_entities(bsp, (const char*)(start+data->entities.offset));
#ifndef NDEBUG
	myprintf("[BSP] [cnt] vert:%u plane:%u node:%u clipnode:%u leaf:%u face:%u edge:%u lface:%u surfedge:%u texinfo:%u model:%u visdata:%u\n", bsp->vert_cnt, bsp->plane_cnt, bsp->node_cnt, bsp->clipnode_cnt, bsp->leaf_cnt, bsp->face_cnt, bsp->edge_cnt, bsp->lface_cnt, bsp->surfedge_cnt, bsp->texinfo_cnt, bsp->model_cnt, bsp->visdata_cnt);
#endif
}

static void bsp_extract_leaf(bsp_t* const bsp, const mat3s* const bsp_to_world) {
#ifndef NDEBUG
	myprintf("[bsp_extract_leaf] cnt:%u\n", bsp->leaf_cnt);
#endif
	for (uint32_t i=0; i<bsp->leaf_cnt; i++) {
		bsp_mleaf_t* const leaf = &bsp->leafs[i];
		const bsp_dleaf_t* const dleaf = &bsp->dleafs[i];
#ifdef BSP_SWAP_UP
		// TODO this probably doesn't flip properly
		leaf->bbox.min = glms_mat3_mulv(*bsp_to_world, glms_vec3_divs(dleaf->bbox.min, BSP_RESIZE_DIV));
		leaf->bbox.max = glms_mat3_mulv(*bsp_to_world, glms_vec3_divs(dleaf->bbox.max, BSP_RESIZE_DIV));
#else
		leaf->bbox.min = glms_vec3_divs(dleaf->bbox.min, BSP_RESIZE_DIV);
		leaf->bbox.max = glms_vec3_divs(dleaf->bbox.max, BSP_RESIZE_DIV);
#endif
		leaf->contents = dleaf->contents;
		leaf->firstmarksurface = bsp->lfaces + dleaf->lface_id;
		leaf->nummarksurfaces = dleaf->lface_num;
		if (dleaf->visofs == -1) {
			leaf->compressed_vis = NULL;
		} else {
			leaf->compressed_vis = bsp->visdata + dleaf->visofs;
		}
		for (int j=0; j<NUM_AMBIENTS; j++)
			leaf->ambient_sound_level[j] = dleaf->ambient_level[j];

		leaf->lights = (link_t){0};
		leaf->lights_dynamic = (link_t){0};
	}
}

static void bsp_extract_nodes(bsp_t* const bsp, const mat3s* const bsp_to_world) {
	/* Extract Nodes */
#ifndef NDEBUG
	myprintf("[BSP] [node] cnt:%u\n", bsp->node_cnt);
#endif
	for (uint32_t i=0; i<bsp->node_cnt; i++) {
		bsp_mnode_t* const node = &bsp->nodes[i];
		const bsp_dnode_t* const dnode = &bsp->dnodes[i];
#ifdef VERBOSE2
		myprintf("[BSP] [node] [%u] c0:%d c1:%d plane_id:%d\n", i, dnode->children[0], dnode->children[1], dnode->plane_id);
#endif
#ifdef BSP_SWAP_UP
		node->min = glms_mat3_mulv(*bsp_to_world, glms_vec3_divs(dnode->box.min, BSP_RESIZE_DIV));
		node->max = glms_mat3_mulv(*bsp_to_world, glms_vec3_divs(dnode->box.max, BSP_RESIZE_DIV));
#else
		node->min = glms_vec3_divs(dnode->box.min, BSP_RESIZE_DIV);
		node->max = glms_vec3_divs(dnode->box.max, BSP_RESIZE_DIV);
#endif
		node->plane = bsp->planes + dnode->plane_id;
		node->firstsurface = dnode->firstface;
		node->numsurfaces = dnode->numfaces;
		for (int32_t j=0; j<2; j++) {
			// johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			int32_t p = dnode->children[j];
			if (p > 0 && p < (int32_t)bsp->node_cnt) {
				node->children[j] = bsp->nodes + p;
			} else {
				p = 0xffffffff - p; // note this uses 65535 intentionally, -1 is leaf 0
				if (p >= 0 && p < (int32_t)bsp->leaf_cnt) {
					node->children[j] = (const bsp_mnode_t*)(bsp->leafs + p);
				} else {
					myprintf("[BSP] invalid leaf index %d (file has only %d leafs)\n", p, bsp->leaf_cnt);
					node->children[j] = (bsp_mnode_t *)(bsp->leafs); // map it to the solid leaf
				}
			}
		}
#ifdef BSP_SWAP_X
		/* Swap */
		if (node->plane->type == 2 || node->plane->type == 5) {
			const struct mnode_s* tmp = node->children[0];
			node->children[0] = node->children[1];
			node->children[1] = tmp;
		}
#endif
	}

#ifdef BSP_SWAP_X
	/* Swap Clipnode Children for X-Axis */
	for (uint32_t i=0; i<bsp->clipnode_cnt; i++) {
		bsp_clipnode_t* clipnode = &clipnodes[i];
		if (bsp->planes[clipnode->planenum].type == 2 || bsp->planes[clipnode->planenum].type == 5) {
			const int32_t tmp = clipnode->children[0];
			clipnode->children[0] = clipnode->children[1];
			clipnode->children[1] = tmp;
		}
	}
#endif
}

static void bsp_calc_surfs(bsp_t* const bsp, const bsp_lux_t* const lux) {
	/* Calculate Surfaces */
	for (uint32_t i=0; i<bsp->face_cnt; i++) {
		const bsp_face_t* const face = &bsp->faces[i];
		bsp_msurface_t* const surf = &bsp->msurfs[i];
		surf->plane = bsp->planes + face->plane_id;
		surf->texinfo = bsp->texinfo + face->texinfo_id;
#ifdef VERBOSE
		myprintf("[bsp_calc_surfs] [%d] face->texinfo_id:%d surf->texinfo:0x%p surf->texinfo->texture_id:%d\n", i, face->texinfo_id, surf->texinfo, surf->texinfo->texture_id);
#endif
		if (face->lightmap >= 0) {
			surf->samples = &bsp->lightdata[face->lightmap];
			surf->luxmap = (rgb_u8_t*)&lux->norms + face->lightmap;
			assert(surf->luxmap);
		} else {
			surf->samples = NULL;
			surf->luxmap = NULL;
		}
		CalcSurfaceExtents(bsp, surf, face);
	}
}

static int bsp_alloc_models(bsp_t* const bsp) {
	/* Count Malloc Size */
	size_t model_size = 0;
	for (u32 i=0; i<bsp->model_cnt; i++) {
		const model_basic_t* const model = &bsp->qmods[i].model;
		for (u32 j=0; j<model->mesh_cnt; j++) {
			const size_t face_cnt = model->meshes[j].face_cnt;
			const size_t vert_cnt = face_cnt*3;
#ifdef VERBOSE
			myprintf("[bsp_alloc_models] model:%u mesh:%u face_cnt:%zu vert_cnt:%zu\n", i, j, face_cnt, vert_cnt);
#endif
			model_size += pad_inc_count(face_cnt*sizeof(face_t), 64); /* faces */
			model_size += pad_inc_count(face_cnt*sizeof(bsp_litinfo_t), 64); /* litdata */
			model_size += pad_inc_count(vert_cnt*sizeof(vert_tangent_t), 64); /* verts */
			model_size += pad_inc_count(vert_cnt*sizeof(vec2s), 64); /* uv */
			model_size += pad_inc_count(vert_cnt*sizeof(vec2s), 64); /* uv lightmap */
		}
	}

	/* Malloc */
	char* model_ptr = mycalloc(model_size, 1);
	if (model_ptr == NULL) {
		myprintf("[ERR] can't calloc model_ptr\n");
		return 1;
	}

	/* Setup Malloc */
	for (u32 i=0; i<bsp->model_cnt; i++) {
		model_basic_t* const model = &bsp->qmods[i].model;
		for (u32 j=0; j<model->mesh_cnt; j++) {
			mesh_t* const mesh = &model->meshes[j];
			const size_t face_cnt = mesh->face_cnt;
			const size_t vert_cnt = face_cnt*3;
			mesh->faces = pad_inc_ptr(&model_ptr, face_cnt*sizeof(face_t), 64);
			mesh->litdata = pad_inc_ptr(&model_ptr, face_cnt*sizeof(bsp_litinfo_t), 64);
			mesh->verts = pad_inc_ptr(&model_ptr, vert_cnt*sizeof(vert_tangent_t), 64);
			mesh->uv = pad_inc_ptr(&model_ptr, vert_cnt*sizeof(vec2s), 64);
			mesh->uv_lightmap = pad_inc_ptr(&model_ptr, vert_cnt*sizeof(vec2s), 64);
		}
	}

	return 0;
}

static void bsp_qmod_gen_model(bsp_t* const bsp, const u32 model_idx, const mat3s* const bsp_to_world) {
#ifndef NDEBUG
		myprintf("[bsp_qmod_gen_model] model_idx:%u\n", model_idx);
#endif
	bsp_qmodel_t* const qmod = &bsp->qmods[model_idx];
	model_basic_t* const model = &qmod->model;

#ifdef VERBOSE
	for (u32 i=0; i<bsp->miptex_cnt; i++) {
		myprintf("[bsp_qmod_gen_model] miptex_idx:%u mesh_idx:%d\n", i, qmod->miptex_to_mesh_lookup[i]);
	}
#endif

	/* Process Luxmap */
	/* this is a big fucky wucky mess because I honestly am not fucking sure */
	const int face_end = qmod->firstmodelsurface+qmod->nummodelsurfaces;
	for (int i=qmod->firstmodelsurface; i<face_end; i++) {
		const bsp_face_t* const face = &bsp->faces[i];
		if (face->lightmap < 0)
			continue;
		const bsp_dtexinfo_t* const texinfo = &bsp->texinfo[face->texinfo_id];
		const bsp_msurface_t* const surf = &bsp->msurfs[i];

#if 0
		if (texinfo->vectorS.x <= 0)
			continue;
		if (texinfo->vectorS.y != 0)
			continue;
		if (texinfo->vectorS.z != 0)
			continue;
		if (texinfo->distS != 0)
			continue;

		if (texinfo->vectorT.x != 0)
			continue;
		if (texinfo->vectorT.y >= 0)
			continue;
		if (texinfo->vectorT.z != 0)
			continue;
		if (texinfo->distT != 0)
			continue;

		if (surf->plane->normal.z >= 0)
			continue;
		if (surf->texturemins[1] >= 0)
			continue;

		print_vec3("vecS", texinfo->vectorS);
		print_vec3("vecT", texinfo->vectorT);
		myprintf("norm: %.2fx%.2fx%.2f\n", surf->plane->normal.x, surf->plane->normal.y, surf->plane->normal.z);
		myprintf("texmins %d %d pdist:%.2f\n", surf->texturemins[0], surf->texturemins[1], surf->plane->dist);
#endif

		const float dots = glm_vec3_dot(surf->plane->normal.raw, texinfo->vectorS.raw) + surf->plane->dist;
		const float dott = glm_vec3_dot(surf->plane->normal.raw, texinfo->vectorT.raw) + surf->plane->dist;
		/* const float dots = glm_vec3_dot(surf->plane->normal.raw, texinfo->vectorS.raw) + texinfo->distS + surf->plane->dist; */
		/* const float dott = glm_vec3_dot(surf->plane->normal.raw, texinfo->vectorT.raw) + texinfo->distT + surf->plane->dist; */
		if (dots >= 0 || dott >= 0)
			continue;

		const int w = (surf->extents[0]>>4)+1;
		const int h = (surf->extents[1]>>4)+1;
		for (int j=0; j<w*h; j++) {
			int val = surf->luxmap[j].r - 127;
			val = -val + 127;
			((rgb_u8_t*)surf->luxmap)[j].r = (u8)val;
		}
	}

	/* Reset tri_cnt for gen */
	for (u32 i=0; i<model->mesh_cnt; i++)
		model->meshes[i].face_cnt = 0;

	for (int i=qmod->firstmodelsurface; i<face_end; i++) {
		const bsp_face_t* const face = &bsp->faces[i];
		const bsp_dtexinfo_t* const texinfo = &bsp->texinfo[face->texinfo_id];
		assert(texinfo->texture_id > 0);
		const int32_t mesh_id = qmod->miptex_to_mesh_lookup[texinfo->texture_id];
		if (mesh_id < 0)
			continue;

		const bsp_texture_t* const texture = &qmod->textures[mesh_id];
		const int tris = face->ledge_num-2;

		mesh_t* const mesh = &model->meshes[mesh_id];

		/* cast away const, alias */
		face_t* faces = (face_t*)mesh->faces;
		vert_tangent_t* verts = (vert_tangent_t*)mesh->verts;
		bsp_litinfo_t* lightpx = (bsp_litinfo_t*)mesh->litdata;
		vec2s* uvs = (vec2s*)mesh->uv;
		vec2s* uvs_lightmap = (vec2s*)mesh->uv_lightmap;

		size_t face_idx = mesh->face_cnt;
		/* TODO is i aligned? */
		qmod->face_lookup[i].mesh_id = mesh_id;
		qmod->face_lookup[i].idx = face_idx;
		qmod->face_lookup[i].cnt = tris;
		mesh->face_cnt += tris;
		size_t vert_idx = mesh->vert_cnt;
		mesh->vert_cnt += tris*3;

		bsp_edge_t edges[3];
		const i32 first_edge_id = face->ledge_id;
		edges[0] = parse_edge(bsp, first_edge_id);

		for (int32_t ei=0; ei<tris; ei++, vert_idx+=3, face_idx++) {
			edges[1] = parse_edge(bsp, first_edge_id+ei+1);
			edges[2] = parse_edge(bsp, first_edge_id+ei+2);

			faces[face_idx].p[0] = vert_idx;
			faces[face_idx].p[1] = vert_idx+1;
			faces[face_idx].p[2] = vert_idx+2;

			/* Z-up to Y-up, Reverse Order for CW to CCW */
			glm_mat3_mulv((vec3*)bsp_to_world->raw, (float*)bsp->verts[edges[2].vertex0].raw, verts[vert_idx].pos.raw);
			glm_mat3_mulv((vec3*)bsp_to_world->raw, (float*)bsp->verts[edges[1].vertex0].raw, verts[vert_idx+1].pos.raw);
			glm_mat3_mulv((vec3*)bsp_to_world->raw, (float*)bsp->verts[edges[0].vertex0].raw, verts[vert_idx+2].pos.raw);

			/* Generate UVs */
			const bsp_msurface_t* const surf = &bsp->msurfs[i];
			for (int k=0; k<3; k++) {
				if (texture->width == 0 || texture->height == 0) {
					myprintf("[ERR] surface without UV\n");
					uvs[vert_idx+k].u = 0;
					uvs[vert_idx+k].v = 0;
					assert(0);
					continue;
				}
				vec3_assert(texinfo->vectorS.raw);
				vec3_assert(texinfo->vectorT.raw);
				assert(!isnan(texinfo->distS));
				assert(!isnan(texinfo->distT));

				/* this is a big fucky wucky mess I created through transmutation of pain to code */
				const vec3s vecs = texinfo->vectorS;
				const vec3s vect = texinfo->vectorT;
				const vec3s tvert = verts[vert_idx+k].pos;
				uvs[vert_idx+k].u = (glm_vec3_dot(tvert.raw, vecs.raw) + texinfo->distS) / texture->width;
				uvs[vert_idx+k].v = (-(glm_vec3_dot(tvert.raw, vect.raw) + texinfo->distT)) / texture->height;
#ifdef VERBOSE
				myprintf("[bsp_qmod_gen_model] [%d] tvert:%.1fx%.1fx%.1f vecS:%.1fx%.1fx%.1f ds:%.1f vecT:%.1fx%.1fx%.1f dt:%.1f uv:%.2fx%.2f\n", i, tvert.x, tvert.y, tvert.z, vecs.x, vecs.y, vecs.z, texinfo->distS, vect.x, vect.y, vect.z, texinfo->distT, uvs[vert_idx+k].u, uvs[vert_idx+k].v);
#endif
				vec2_assert(uvs[vert_idx+k].raw);
			}

			/* Generate UVs (Lightmap) */
			if (face->lightmap >= 0) {
				lightpx[face_idx].w = (surf->extents[0]>>4)+1;
				lightpx[face_idx].h = (surf->extents[1]>>4)+1;
				lightpx[face_idx].lightmap = surf->samples;
				lightpx[face_idx].luxmap = surf->luxmap;
				for (int k=0; k<3; k++) {
					vec2s* const uv = &uvs_lightmap[vert_idx+k];
					uv->u = glm_vec3_dot(verts[vert_idx+k].pos.raw, texinfo->vectorS.raw) + texinfo->distS;
					uv->u -= surf->texturemins[0];

					/* sometimes we're negative by a small fraction */
					if (uv->u < 0) {
						myprintf("[WARN] [bsp_qmod_gen_model] uv->u<0: %f\n", uv->u);
						uv->u = 0;
					}

					if (surf->extents[0] > 0) {
						uv->u /= surf->extents[0];
						/* sometimes we're negative by a small fraction */
						if (uv->u > 1.0f) {
							myprintf("[WARN] [bsp_qmod_gen_model] uv->u>1: %f\n", uv->u);
							uv->u = 1.0f;
						}
					} else {
						uv->u = 0;
					}
					uv->v = glm_vec3_dot(verts[vert_idx+k].pos.raw, texinfo->vectorT.raw) + texinfo->distT;
					uv->v -= surf->texturemins[1];

					/* sometimes we're negative by a small fraction */
					if (uv->v < 0) {
						myprintf("[WARN] [bsp_qmod_gen_model] uv->v<0: %f\n", uv->v);
						uv->v = 0;
					}

					if (surf->extents[1] > 0) {
						uv->v /= surf->extents[1];
						/* sometimes we're negative by a small fraction */
						if (uv->v > 1.0f) {
							myprintf("[WARN] [bsp_qmod_gen_model] uv->v>1: %f\n", uv->v);
							uv->v = 1.0f;
						}
					} else {
						uv->v = 0;
					}
#ifdef VERBOSE
					myprintf("[UV] uv:%f,%f min:%d,%d ex:%d,%d\n", uv->u, uv->v, surf->texturemins[0], surf->texturemins[1], surf->extents[0], surf->extents[1]);
#endif
					assert(uv->u >= 0);
					assert(uv->v >= 0);
					assert(uv->u <= 1.0);
					assert(uv->v <= 1.0);
					/* traditionally +8 is added to the UV, it helps with nearest neighbor rendering, but messes with my interpolation */
				}
			} else {
				lightpx[face_idx] = (bsp_litinfo_t){0};
			}

			/* Scale Quake BSP Size to Blender Scale (must happen after UV) */
			glm_vec3_divs(verts[vert_idx].pos.raw, BSP_RESIZE_DIV, verts[vert_idx].pos.raw);
			glm_vec3_divs(verts[vert_idx+1].pos.raw, BSP_RESIZE_DIV, verts[vert_idx+1].pos.raw);
			glm_vec3_divs(verts[vert_idx+2].pos.raw, BSP_RESIZE_DIV, verts[vert_idx+2].pos.raw);

			/* Calculate Tangent */
			/* maybe review R_BuildBumpVectors from fteqw */
			vec3_assert(verts[vert_idx+0].pos.raw);
			vec3_assert(verts[vert_idx+1].pos.raw);
			vec3_assert(verts[vert_idx+2].pos.raw);
			vec3s deltaPos[2];
			glm_vec3_sub(verts[vert_idx+1].pos.raw, verts[vert_idx].pos.raw, deltaPos[0].raw);
			glm_vec3_sub(verts[vert_idx+2].pos.raw, verts[vert_idx].pos.raw, deltaPos[1].raw);
			vec3_assert(deltaPos[0].raw);
			vec3_assert(deltaPos[1].raw);
			verts[vert_idx].norm.x = deltaPos[0].y*deltaPos[1].z - deltaPos[0].z*deltaPos[1].y;
			verts[vert_idx].norm.y = deltaPos[0].z*deltaPos[1].x - deltaPos[0].x*deltaPos[1].z;
			verts[vert_idx].norm.z = deltaPos[0].x*deltaPos[1].y - deltaPos[0].y*deltaPos[1].x;
			glm_normalize(verts[vert_idx].norm.raw);
			vec3_assert(verts[vert_idx].norm.raw);
			verts[vert_idx+1].norm = verts[vert_idx].norm;
			verts[vert_idx+2].norm = verts[vert_idx].norm;

			vec2s deltaUV[2];
#if 1
			glm_vec2_sub(uvs[vert_idx+1].raw, uvs[vert_idx+0].raw, deltaUV[0].raw);
			glm_vec2_sub(uvs[vert_idx+2].raw, uvs[vert_idx+0].raw, deltaUV[1].raw);
#else
			vec2s tmpUVs[3];
			for (int uvi=0; uvi<3; uvi++) {
				tmpUVs[uvi].u = wrap01(uvs[vert_idx+uvi].u);
				tmpUVs[uvi].v = wrap01(uvs[vert_idx+uvi].v);
			}
			glm_vec2_sub(tmpUVs[1].raw, tmpUVs[0].raw, deltaUV[0].raw);
			glm_vec2_sub(tmpUVs[2].raw, tmpUVs[0].raw, deltaUV[1].raw);
#endif

			vec3s svector, tvector;
			svector.x = deltaUV[0].y * deltaPos[1].x - deltaUV[1].y * deltaPos[0].x;
			svector.y = deltaUV[0].y * deltaPos[1].y - deltaUV[1].y * deltaPos[0].y;
			svector.z = deltaUV[0].y * deltaPos[1].z - deltaUV[1].y * deltaPos[0].z;

			tvector.x = deltaUV[0].x * deltaPos[1].x - deltaUV[1].x * deltaPos[0].x;
			tvector.y = deltaUV[0].x * deltaPos[1].y - deltaUV[1].x * deltaPos[0].y;
			tvector.z = deltaUV[0].x * deltaPos[1].z - deltaUV[1].x * deltaPos[0].z;

			/* float f = deltaUV[0].x * -deltaUV[1].y - deltaUV[1].x * -deltaUV[0].y; */
			float f = deltaUV[0].x * deltaUV[1].y - deltaUV[1].x * deltaUV[0].y;
			if (f != 0)
				f = 1.0 / f;
			/* vec1_assert_zeroone(f); */
			vec3s tangent, tmp1;
			glm_vec3_scale(deltaPos[0].raw, deltaUV[1].y, tangent.raw);
			glm_vec3_scale(deltaPos[1].raw, deltaUV[0].y, tmp1.raw);
			glm_vec3_sub(tangent.raw, tmp1.raw, tangent.raw);
			glm_vec3_scale(tangent.raw, f, tangent.raw);
			glm_vec3_normalize(tangent.raw);

			/* rotate if whatever */
			/* TODO this is probably still fucked */
			glm_vec3_cross(tvector.raw, svector.raw, tmp1.raw);
			if (glm_vec3_dot(tmp1.raw, verts[vert_idx].norm.raw) < 0) {
				/* glm_vec3_negate(tangent.raw); */
				tangent.x = -tangent.x;
				/* tangent.y = -tangent.y; */
				tangent.z = -tangent.z;
			}

			vec3_assert_norm(tangent.raw);
			glm_vec3_copy(tangent.raw, verts[vert_idx].tangent.raw);
			glm_vec3_copy(tangent.raw, verts[vert_idx+1].tangent.raw);
			glm_vec3_copy(tangent.raw, verts[vert_idx+2].tangent.raw);
		}
	}

#ifndef NDEBUG
	for (u32 i=0; i<model->mesh_cnt; i++) {
		myprintf("[bsp_qmod_gen_model] [POST] [%u] face_cnt:%u\n", i, model->meshes[i].face_cnt);
	}
#endif
}

int bsp_parse(char* const start, bsp_t* const bsp, const bsp_lux_t* const lux, const assets_t* const assets, alloc_t* const alloc) {
	bsp_parse_header(start, bsp);
	if (bsp_alloc(bsp))
		return 1;
	const mat3s bsp_to_world={.col={(vec3s){.x=1,.y=0,.z=0},(vec3s){.x=0,.y=0,.z=-1},(vec3s){.x=0,.y=1,.z=0}}};
	bsp_extract_leaf(bsp, &bsp_to_world);
	bsp_extract_nodes(bsp, &bsp_to_world);
	bsp_create_hulls(bsp, &bsp->qmods[0], bsp->hull0_nodes);
	bsp_calc_surfs(bsp, lux);
	bsp_parse_qmods(bsp);
	for (u32 i=0; i<bsp->model_cnt; i++)
		bsp_extract_textures(bsp, assets, alloc, i);
	bsp_alloc_models(bsp);
	for (u32 i=0; i<bsp->model_cnt; i++) {
		bsp_qmod_gen_model(bsp, i, &bsp_to_world);
	}
	return 0;
}

void bsp_transform(char* const start) {
	const bsp_header_t* const data = (const bsp_header_t* const)start;
#ifndef NDEBUG
	myprintf("[bsp_transform] ver:0x%lx\n", (uint8_t)data->version);
#endif

	/* Setup Entry Index */
	const size_t plane_cnt = data->planes.size/sizeof(bsp_plane_t);
	const size_t texinfo_cnt = data->texinfo.size/sizeof(bsp_dtexinfo_t);

	/* Extract and Transform Planes */
	mat3s bsp_to_world={.col={(vec3s){.x=1,.y=0,.z=0},(vec3s){.x=0,.y=0,.z=-1},(vec3s){.x=0,.y=1,.z=0}}};
	bsp_plane_t* planes = (bsp_plane_t*)(start+data->planes.offset);
	for (uint32_t i=0; i<plane_cnt; i++) {
#ifdef BSP_SWAP_UP
		glm_mat3_mulv(bsp_to_world.raw, planes[i].normal.raw, planes[i].normal.raw);
#endif
		int bits = 0;
		for (int j=0; j<3; j++) {
			if (planes[i].normal.raw[j] < 0)
				bits |= 1 << j;
		}

#ifdef VERBOSE2
		myprintf("[bsp_transform] [%u] type:%lx dist:%.2f %lx %lx %lx\n", i, planes[i].dist, planes[i].type, planes[i].signbits, planes[i].pad[0], planes[i].pad[1]);
#endif

		planes[i].dist /= BSP_RESIZE_DIV;

#ifdef BSP_SWAP_UP
		const uint8_t remap[6] = {0, 2, 1, 3, 5, 4};
		planes[i].type = remap[planes[i].type];
#endif

#ifdef BSP_SWAP_X
		if (planes[i].type == 2 || planes[i].type == 5) {
			planes[i].dist = -planes[i].dist;
		}
#endif

		planes[i].signbits = bits;
	}

	/* Transform Texinfo */
	bsp_dtexinfo_t *texinfo = (bsp_dtexinfo_t*)(start+data->texinfo.offset);
	for (uint32_t i=0; i<texinfo_cnt; i++, texinfo++) {
#ifdef VERBOSE
		myprintf("[bsp_transform] texinfo [%d] texinfo->texid:%d vecS:%.2fx%.2fx%.2f vecT:%.2fx%.2fx%.2f ds:%d dt:%d\n", i, texinfo->texture_id, texinfo->vectorS.x, texinfo->vectorS.y, texinfo->vectorS.z, texinfo->vectorT.x, texinfo->vectorT.y, texinfo->vectorT.z, texinfo->distS, texinfo->distT);
#endif
		float ftmp;
		ftmp = texinfo->vectorS.y;
		texinfo->vectorS.y = texinfo->vectorS.z;
		texinfo->vectorS.z = -ftmp;

		ftmp = texinfo->vectorT.y;
		texinfo->vectorT.y = texinfo->vectorT.z;
		texinfo->vectorT.z = -ftmp;
	}

	/* Transform Models */
	const size_t model_cnt = data->models.size/sizeof(bsp_dmodel_t);
	bsp_dmodel_t* const models = (bsp_dmodel_t*)(start+data->models.offset);
	for (uint32_t i=0; i<model_cnt; i++) {
		bsp_dmodel_t* model = &models[i];

		/* flip bbox axis */
		float tmp = -model->bbox.min.y;
		model->bbox.min.y = model->bbox.min.z;
		model->bbox.min.z = tmp;
		tmp = -model->bbox.max.y;
		model->bbox.max.y = model->bbox.max.z;
		model->bbox.max.z = tmp;

		if (model->bbox.min.z > model->bbox.max.z) {
			tmp = model->bbox.min.z;
			model->bbox.min.z = model->bbox.max.z;
			model->bbox.max.z = tmp;
		}

		/* Quake code does this to expand it by a "pixel", I dunno */
		model->bbox.min.x -= 1.0f;
		model->bbox.min.y -= 1.0f;
		model->bbox.min.z -= 1.0f;
		model->bbox.max.x += 1.0f;
		model->bbox.max.y += 1.0f;
		model->bbox.max.z += 1.0f;

		model->origin = swapaxis(model->origin);

		/* disable annoying warning, it doesn't matter */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
		glm_vec3_divs(model->bbox.min.raw, BSP_RESIZE_DIV, model->bbox.min.raw);
		glm_vec3_divs(model->bbox.max.raw, BSP_RESIZE_DIV, model->bbox.max.raw);
		glm_vec3_divs(model->origin.raw, BSP_RESIZE_DIV, model->origin.raw);
#pragma GCC diagnostic pop

#ifndef NDEBUG
		myprintf("[bsp_transform] model:%d firstface:%d numfaces:%d visleafs:%d headnode:%d %d %d %d\n", i, model->firstface, model->numfaces, model->visleafs, model->headnode[0], model->headnode[1], model->headnode[2], model->headnode[3]);
		print_vec3("[bsp_transform] model->origin:", model->origin);
		print_vec3("[bsp_transform] model->mins:", model->bbox.min);
		print_vec3("[bsp_transform] model->maxs:", model->bbox.max);
#endif
	}
}

/* SV_InitBoxHull: Set up the planes and clipnodes so that the six floats of a bounding box can just be stored out and get a proper hull_t structure */
void SV_InitBoxHull(hull_box_t* hb) {
	hb->box_hull.clipnodes = hb->box_clipnodes;
	hb->box_hull.planes = hb->box_planes;
	hb->box_hull.firstclipnode = 0;
	hb->box_hull.lastclipnode = 5;

	for (int i = 0; i < 6; i++) {
		hb->box_clipnodes[i].planenum = i;
		const int side = i & 1;
		hb->box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			hb->box_clipnodes[i].children[side ^ 1] = i + 1;
		else
			hb->box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		hb->box_planes[i].type = i >> 1;
		hb->box_planes[i].normal.raw[i >> 1] = 1;
	}
}

