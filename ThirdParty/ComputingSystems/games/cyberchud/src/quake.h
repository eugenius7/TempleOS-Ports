/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
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
#ifndef QUAKE_H
#define QUAKE_H

#include <stdbool.h>
#include "engine.h"

#define FL_FLY 1
#define FL_SWIM 2
#define FL_CONVEYOR 4
#define FL_CLIENT 8
#define FL_INWATER 16
#define FL_MONSTER 32
#define FL_GODMODE 64
#define FL_NOTARGET 128
#define FL_ITEM 256
#define FL_ONGROUND 512
#define FL_PARTIALGROUND 1024 // not all corners are valid
#define FL_WATERJUMP 2048 // player jumping out of water
#define FL_JUMPRELEASED 4096 // for jump debouncing

#define SPAWNFLAGS_SPAWNER_AUTOSPAWNER 1

#define SPAWNFLAGS_LIGHT_STARTOFF       1
#define SPAWNFLAGS_LIGHT_NOSHADOWCASTER 2

typedef struct {
	bool allsolid;     // if true, plane is not valid
	bool startsolid;   // if true, the initial point was in a solid area
	bool inopen, inwater;
	float fraction;    // time completed, 1.0 = didn't hit anything
	vec3s endpos;      // final position
	bsp_plane_t plane; // surface normal at impact
	edict_t* ent;

	int contents; // spike -- the content type(s) that we found.
} trace_t;

typedef struct lightcache_s {
	int32_t surfidx; // < 0: black surface; == 0: no cache; > 0: 1+index of surface
	vec3s pos;
	int16_t ds;
	int16_t dt;
} lightcache_t;

typedef struct {
	u32 pointlight_cnt;
	u32 shadowcaster_cnt;
	i32* pointlight_ids;
	i32* shadowcaster_ids;
} light_query_t;

int SV_HullPointContents(const hull_t* const hull, int num, const vec3s p);
int SV_RecursiveHullCheck(const hull_t* const hull, const bbox_t* const move, trace_t* const trace, const unsigned int hitcontents);
trace_t SV_Move(engine_t *e, const int32_t id, bbox_t *move, int type);
void SV_WalkMove(engine_t *e, const int32_t id, const float delta);
int SV_FlyMove(engine_t *e, const int32_t id, const float time, trace_t *steptrace);

void SV_LinkEdict(engine_t* e, edict_t *ent, bool touch_triggers);
void SV_UnlinkEdict(edict_t *ent);

void SV_UnlinkLight(edict_light_t *ent);
void SV_LinkLight(engine_t* e, edict_light_t *ent);
void SV_LinkLightDynamic(engine_t* e, edict_light_t *ent);

void SV_UserFriction(engine_t* e, const i32 id, const f32 delta);
void SV_ClearWorld(engine_t *e);
trace_t SV_PushEntity(engine_t* const e, const int32_t id, const vec3s push);
bool SV_StepDirection(engine_t* const e, edict_t *ent, const vec3s dir, const float dist, const float delta);
void SV_Accelerate(ecs_t* const ecs, const i32 id, const float wishspeed, const vec3s wishdir, const float delta);
void SV_AirAccelerate(ecs_t* const ecs, const i32 id, float wishspeed, vec3s wishveloc, const float delta);
void SV_NewChaseDir(engine_t* const e, edict_t* const actor, edict_t* const enemy, float dist);
void SV_MoveToGoal(engine_t* const e, edict_t *ent, const vec3s dir, const float dist, const float delta);
u8* SV_FatPVS (vec3s org, const bsp_qmodel_t* const worldmodel);
bool SV_VisibleToClient(vec3s pos, edict_t *test, bsp_qmodel_t *worldmodel);

void R_MarkSurfaces(engine_t* const e, const vec3 pos, visflags_t* visflags);
void R_GetLights(engine_t* const e, const vec3 pos, light_query_t* const out);
float R_LightPoint(const bsp_t* const bsp, const vec3s p, const float ofs, lightcache_t *cache);

void TraceLine(engine_t *e, const hull_t* hull, const bbox_t* const move, trace_t* trace, const int32_t owner);

#endif
