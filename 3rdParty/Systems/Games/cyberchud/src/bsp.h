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

#ifndef BSP_H
#define BSP_H

#include "alloc.h"
#include "model.h"
#include "px.h"

#define BSP_RESIZE_DIV 32 // Ranger is 56 units tall, avg male height is ~1.75m. 56/1.75=32

#define MAX_MAP_HULLS 4

#define AMBIENT_WATER 0
#define AMBIENT_SKY 1
#define AMBIENT_SLIME 2
#define AMBIENT_LAVA 3

#define NUM_AMBIENTS 4 // automatic ambient sounds

#define CONTENTS_EMPTY  -1
#define CONTENTS_SOLID  -2
#define CONTENTS_WATER  -3
#define CONTENTS_SLIME  -4
#define CONTENTS_LAVA   -5
#define CONTENTS_SKY    -6
#define CONTENTS_ORIGIN -7 // removed at csg time
#define CONTENTS_CLIP   -8 // changed to contents_solid

#define CONTENTMASK_FROMQ1(c) (1u << (-(c)))
#define CONTENTMASK_ANYSOLID  (CONTENTMASK_FROMQ1 (CONTENTS_SOLID) | CONTENTMASK_FROMQ1 (CONTENTS_CLIP))

typedef struct {
	uint32_t mesh_id;
	uint32_t idx;
	uint32_t cnt;
} bsp_face_lookup_t;

typedef struct {
	int32_t  plane_id;    // The plane that splits the node, must be in [0,numplanes[
	int32_t  children[2]; // If bit15==0, index of Front child node, If bit15==1, ~front = index of child leaf
	bbox_t   box;         // Bounding box of node and all childs
	uint32_t firstface;   // Index of first Polygons in the node
	uint32_t numfaces;    // Number of faces in the node
} bsp_dnode_t;

typedef struct {
	int32_t numtex;     // Number of textures in Mip Texture list
	int32_t dataofs[4]; // Offset to each of the individual texture, from the beginning of mipheader_t
} bsp_mipheader_t;

typedef struct {
	char     name[16]; // Name of the texture.
	uint32_t width;    // width of picture, must be a multiple of 8
	uint32_t height;   // height of picture, must be a multiple of 8
	uint32_t offset1;  // offset to uint8_t Pix[width   * height]
	uint32_t offset2;  // offset to uint8_t Pix[width/2 * height/2]
	uint32_t offset4;  // offset to uint8_t Pix[width/4 * height/4]
	uint32_t offset8;  // offset to uint8_t Pix[width/8 * height/8]
} bsp_miptex_t;

typedef struct {
	uint16_t width;
	uint16_t height;
} bsp_texture_t;

typedef struct {
	int32_t  contents;                    // Special type of leaf
	int32_t  visofs;                      // Beginning of visibility lists, must be -1 or in [0,numvislist[
	bbox_t   bbox;                        // Bounding box of the leaf
	uint32_t lface_id;                    // First item of the list of faces, must be in [0,numlfaces[
	uint32_t lface_num;                   // Number of faces in the leaf
	uint8_t  ambient_level[NUM_AMBIENTS]; //  0=mute, 0xFF=max
} bsp_dleaf_t;

typedef struct link_s {
	struct link_s *prev, *next;
} link_t;

typedef struct {
	/* common with node */
	int contents; // wil be a negative contents number
	bbox_t bbox;

	/* leaf specific */
	int nummarksurfaces;
	int combined_deps; // contains index into brush_deps_data[] with used warp and lightmap textures
	uint8_t ambient_sound_level[NUM_AMBIENTS];
	const uint8_t *compressed_vis;
	const int32_t *firstmarksurface;
	/* efrag_t *efrags; */

	link_t lights;
	link_t lights_dynamic;
} bsp_mleaf_t;

typedef struct {
	vec3s normal;     // Vector orthogonal to plane (Nx,Ny,Nz) with Nx2+Ny2+Nz2 = 1
	float dist;       // Offset to plane, along the normal vector, Distance from (0,0,0) to the plane
	uint8_t type;     // for texture axis selection and fast side tests
	uint8_t signbits; // signx + signy<<1 + signz<<1
	uint8_t pad[2];
} bsp_plane_t;

typedef struct {
	int32_t planenum;    // The plane which splits the node
	int32_t children[2]; // 0=front, 1=back. positive val=id of child node, -2=part is inside the model, -1=part is outside the model
} bsp_clipnode_t;

typedef struct {
	int32_t plane_id;   // The plane in which the face lies, must be in [0,numplanes[
	int32_t side;       // 0 if in front of the plane, 1 if behind the plane
	int32_t ledge_id;   // first edge in the List of edges, must be in [0,numledges[
	int32_t ledge_num;  // number of edges in the List of edges
	int32_t texinfo_id; // index of the Texture info the face is part of must be in [0,numtexinfos[
	uint8_t typelight;  // type of lighting, for the face
	uint8_t baselight;  // from 0xFF (dark) to 0 (bright)
	uint8_t light[2];   // two additional light models
	int32_t lightmap;   // Pointer inside the general light map, or -1 this define the start of the face light map
} bsp_face_t;

typedef struct {
	uint32_t vertex0; // index of the start vertex must be in [0,numvertices[
	uint32_t vertex1; // index of the end vertex must be in [0,numvertices[
} bsp_edge_t;

typedef struct {
	vec3s vectorS;    // S vector, horizontal in texture space)
	float distS;      // horizontal offset in texture space
	vec3s vectorT;    // T vector, vertical in texture space
	float distT;      // vertical offset in texture space
	uint32_t texture_id; // Index of Mip Texture must be in [0,numtex[
	uint32_t animated;   // 0 for ordinary textures, 1 for water
} bsp_dtexinfo_t;

typedef struct {
	int32_t offset; // Offset to entry, in bytes, from start of file
	int32_t size;   // Size of entry in file, in bytes
} bsp_dentry_t;

typedef struct mnode_s {
	// common with leaf
	int contents; // 0, to differentiate from leafs
	vec3s min;
	vec3s max;

	// node specific
	uint32_t           firstsurface;
	uint32_t           numsurfaces;
	const bsp_plane_t   *plane;
	const struct mnode_s *children[2];
} bsp_mnode_t;

typedef struct {
	const bsp_clipnode_t *clipnodes;
	const bsp_plane_t    *planes;
	int32_t              firstclipnode;
	int32_t              lastclipnode;
	vec3s                clip_mins;
	vec3s                clip_maxs;
} hull_t;

typedef struct {
	bbox_t bbox;
	vec3s origin;
	int32_t headnode[MAX_MAP_HULLS];
	int32_t visleafs; // not including the solid leaf 0
	int32_t firstface, numfaces;
} __attribute__((__packed__)) bsp_dmodel_t;

typedef struct {
	bbox_t bbox;
	hull_t hulls[MAX_MAP_HULLS];
	int32_t firstmodelsurface;
	int32_t nummodelsurfaces;
	int32_t numleafs;
	const bsp_mnode_t *nodes;
	const bsp_mleaf_t *leafs;
	const uint8_t *lightdata;

	/* Non-Quake */
	bsp_face_lookup_t* face_lookup;
	i16* miptex_to_mesh_lookup;
	bsp_texture_t* textures;
	model_basic_t model;
} bsp_qmodel_t;

typedef struct {
	int32_t version;    // Model version, must be 0x17 (23).
	bsp_dentry_t entities;  // List of Entities.
	bsp_dentry_t planes;    // Map Planes.numplanes = size/sizeof(plane_t)
	bsp_dentry_t miptex;    // Wall Textures.
	bsp_dentry_t vertices;  // Map Vertices.numvertices = size/sizeof(vertex_t)
	bsp_dentry_t visdata;  // Leaves Visibility lists.
	bsp_dentry_t nodes;     // BSP Nodes, numnodes = size/sizeof(node_t)
	bsp_dentry_t texinfo;   // Texture Info for faces, numtexinfo = size/sizeof(texinfo_t)
	bsp_dentry_t faces;     // Faces of each surface, numfaces = size/sizeof(face_t)
	bsp_dentry_t lightmaps; // Wall Light Maps.
	bsp_dentry_t clipnodes; // clip nodes, for Models, numclips = size/sizeof(clipnode_t)
	bsp_dentry_t leafs;    // BSP Leaves, numlaves = size/sizeof(leaf_t)
	bsp_dentry_t lfaces;    // List of Faces
	bsp_dentry_t edges;     // Edges of faces, numedges = Size/sizeof(edge_t)
	bsp_dentry_t surfedges; // List of Edges
	bsp_dentry_t models;    // List of Models, nummodels = Size/sizeof(model_t)
} bsp_header_t;

typedef struct areanode_s {
	int32_t axis; // -1 = leaf node
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} areanode_t;

typedef enum {
	BSP_ENTITY_START,
	BSP_ENTITY_MOB_CHUD,
	BSP_ENTITY_MOB_MUTT,
	BSP_ENTITY_MOB_TROON,
	BSP_ENTITY_BUTTON,
	BSP_ENTITY_DOOR,
	BSP_ENTITY_LIGHT,
	BSP_ENTITY_LIGHT_DYNAMIC,
	BSP_ENTITY_PARTICLE_EMITTER,
	BSP_ENTITY_PATH_CORNER,
	BSP_ENTITY_SPAWNER,
	BSP_ENTITY_TRIGGER_MULTIPLE,
	BSP_ENTITY_TRIGGER_ONCE,
	BSP_ENTITY_TRIGGER_TALK,
	BSP_ENTITY_TRIGGER_CHANGELEVEL,
	BSP_ENTITY_WEAPON_BEAM,
	BSP_ENTITY_WEAPON_SHOTGUN,
	BSP_ENTITY_WEAPON_SMG,
} BSP_ENTITY_TYPE;

#define BSP_NAME_LEN 32
typedef struct {
	BSP_ENTITY_TYPE classname;
	int32_t ecs_id;
	uint32_t flags;
	vec3s origin;
	int16_t model;
	int16_t light;
	int16_t speed;
	int16_t range;
	int16_t angle;
	uint8_t task;
	char target[BSP_NAME_LEN];
	char targetname[BSP_NAME_LEN];
	char message[BSP_NAME_LEN];
} bsp_entity_t;

typedef struct {
	const bsp_plane_t* plane;
	/* int flags; */
	const uint8_t* samples;
	const rgb_u8_t* luxmap;
	int32_t texturemins[2];
	const bsp_dtexinfo_t *texinfo;
	int16_t extents[2];
} bsp_msurface_t;

typedef struct {
	uint32_t entities_cnt;
	uint32_t plane_cnt;
	uint32_t miptex_cnt;
	uint32_t vert_cnt;
	uint32_t visdata_cnt;
	uint32_t node_cnt;
	uint32_t texinfo_cnt;
	uint32_t face_cnt;
	uint32_t clipnode_cnt;
	uint32_t leaf_cnt;
	uint32_t lface_cnt;
	uint32_t edge_cnt;
	uint32_t surfedge_cnt;
	uint32_t model_cnt;
	const vec3s* verts;
	const bsp_face_t* faces;
	const bsp_plane_t* planes;
	const bsp_edge_t* edges;
	const bsp_dnode_t* dnodes;
	const bsp_dleaf_t* dleafs;
	const bsp_dtexinfo_t* texinfo;
	const bsp_mipheader_t* miptex_headers;
	const bsp_clipnode_t* clipnodes;
	const int32_t* surfedges;
	const int32_t* lfaces;
	const uint8_t* visdata;
	bsp_dmodel_t* dmodels;

	/* Non-File Data */
	size_t malloc_size;
	bsp_qmodel_t* qmods;
	bsp_mleaf_t* leafs;
	bsp_mnode_t* nodes;
	bsp_msurface_t *msurfs;

	/* Entity Data */
	bsp_entity_t* entities;

	/* internal to paser */
	const u8* lightdata;
	bsp_clipnode_t *hull0_nodes;
} bsp_t;

typedef struct {
	hull_t box_hull;
	bsp_clipnode_t box_clipnodes[6];
	bsp_plane_t box_planes[6];
} hull_box_t;

typedef void (*edict_touch_t)(void *e, int32_t id, int32_t other);

#define MOVETYPE_NONE       0  // never moves
#define MOVETYPE_WALK       1  // gravity
#define MOVETYPE_STEP       2  // gravity, special edge handling
#define MOVETYPE_FLY        3
#define MOVETYPE_TOSS       4  // gravity
#define MOVETYPE_PUSH       5  // no clip to world, push and crush
#define MOVETYPE_NOCLIP     6
#define MOVETYPE_FLYMISSILE 7  // extra size to monsters
#define MOVETYPE_BOUNCE     8
#define MOVETYPE_GIB        9 // 2021 rerelease gibs

#define SOLID_NOT      0 // no interaction with other objects
#define SOLID_TRIGGER  1 // touch on edge, but not blocking
#define SOLID_BBOX     2 // touch on edge, block
#define SOLID_SLIDEBOX 3 // touch on edge, but not an onground
#define SOLID_BSP      4 // bsp clip, touch on edge, block

#define MAX_ENT_LEAFS 32
typedef struct {
	/* Link Data */
	link_t area;

	/* Entity Values */
	int32_t id;
	int32_t num_leafs;
	int32_t leafnums[MAX_ENT_LEAFS];
} edict_basic_t;

typedef struct  {
	edict_basic_t basic;
	edict_touch_t touch;
	edict_touch_t click;
	versors rot_ideal;
	bbox_t absbox;
	int32_t flags;
	int32_t owner;
	int16_t movetype;
	int16_t solid;

	const hull_t *hulls[MAX_MAP_HULLS];
} edict_t;

typedef struct {
	edict_basic_t basic;
	vec3s pos;
} edict_light_t;

#define MAX_AREA_DEPTH 9
#define AREA_NODES (2 << MAX_AREA_DEPTH)
typedef struct {
	int32_t numareanodes;
	int32_t map_id;
	int32_t edict_cnt;
	int32_t edict_light_cnt;
	areanode_t areanodes[AREA_NODES];
	alloc_t edict_mem;
	i32 buf_cap;
	edict_t** moved_edict_buf;
	vec3s* moved_from_buf;
} qcvm_t;

void SV_InitBoxHull(hull_box_t* hb);
uint8_t* Mod_LeafPVS(const bsp_mleaf_t* const leaf, const bsp_qmodel_t* const model);

#endif
