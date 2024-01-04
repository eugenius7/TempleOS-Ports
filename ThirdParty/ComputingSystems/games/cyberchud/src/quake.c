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

#include <assert.h>
#include <float.h>

#include "quake.h"
#include "text.h"
#include "visflags.h"
#include "ents/player.h"
#include "utils/myds.h"

#define CONTENTS_CURRENT_0    -9
#define CONTENTS_CURRENT_90   -10
#define CONTENTS_CURRENT_180  -11
#define CONTENTS_CURRENT_270  -12
#define CONTENTS_CURRENT_UP   -13
#define CONTENTS_CURRENT_DOWN -14

#define MOVE_NORMAL     0
#define MOVE_NOMONSTERS 1
#define MOVE_MISSILE    2

#define MOVE_HITALLCONTENTS (1 << 9)

#define DIST_EPSILON (0.03125/BSP_RESIZE_DIV) // 1/32 epsilon to keep floating point happy
#define MIN(a,b) (((a)<(b))?(a):(b))

#define DoublePrecisionDotProduct(a,b) ((double)(a).x*(b).x+(double)(a).y*(b).y+(double)(a).z*(b).z)

enum {
	rht_solid,
	rht_empty,
	rht_impact
};
struct rhtctx_s {
	uint32_t hitcontents;
	vec3s start;
	vec3s end;
	const bsp_clipnode_t* clipnodes;
	const bsp_plane_t* planes;
};

static inline vec3s InterpolateV3(const vec3s Left, const vec3s Right, const float t) {
	return glms_vec3_add(Left, glms_vec3_scale(glms_vec3_sub(Right, Left), t));
}

int SV_HullPointContents(const hull_t* const hull, int num, const vec3s p) {
	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			fputs("SV_HullPointContents: bad node number", stderr);

		const bsp_clipnode_t* const node = hull->clipnodes + num;
		const bsp_plane_t* const plane = hull->planes + node->planenum;
		const float d = glms_vec3_dot(plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}
	return num;
}

/*
==================
Q1BSP_RecursiveHullTrace

This does the core traceline/tracebox logic.
This version is from FTE and attempts to be more numerically stable than vanilla.
This is achieved by recursing at the actual decision points instead of vanilla's habit of vanilla's habit of using points that are outside of the child's
volume. It also uses itself to test solidity on the other side of the node, which ensures consistent precision. The actual collision point is (still) biased by
an epsilon, so the end point shouldn't be inside walls either way. FTE's version 'should' be more compatible with vanilla than DP's (which doesn't take care
with allsolid). ezQuake also has a version of this logic, but I trust mine more.
==================
*/
static int Q1BSP_RecursiveHullTrace(struct rhtctx_s *ctx, int num, const float p1f, const float p2f, const vec3s p1, const vec3s p2, trace_t* const trace) {
reenter:
	if (num < 0) {
		/*hit a leaf*/
		trace->contents = num;
		if (ctx->hitcontents & CONTENTMASK_FROMQ1 (num))
		{
			if (trace->allsolid)
				trace->startsolid = true;
			return rht_solid;
		}
		else
		{
			trace->allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else if (num != CONTENTS_SOLID)
				trace->inwater = true;
			return rht_empty;
		}
	}

	/*get the node info*/
	const bsp_clipnode_t* const node = ctx->clipnodes + num;
	const bsp_plane_t* const plane = ctx->planes + node->planenum;

	float t1 = glms_vec3_dot(plane->normal, p1) - plane->dist;
	float t2 = glms_vec3_dot(plane->normal, p2) - plane->dist;

	/*if its completely on one side, resume on that side*/
	if (t1 >= 0 && t2 >= 0) {
		// return Q1BSP_RecursiveHullTrace (hull, node->children[0], p1f, p2f, p1, p2, trace);
		num = node->children[0];
		goto reenter;
	}
	if (t1 < 0 && t2 < 0) {
		// return Q1BSP_RecursiveHullTrace (hull, node->children[1], p1f, p2f, p1, p2, trace);
		num = node->children[1];
		goto reenter;
	}

	t1 = glms_vec3_dot(plane->normal, ctx->start) - plane->dist;
	t2 = glms_vec3_dot(plane->normal, ctx->end) - plane->dist;

	const int side = t1 < 0;
	float midf = t1 / (t1 - t2);
	if (midf < p1f)
		midf = p1f;
	if (midf > p2f)
		midf = p2f;
	const vec3s mid = InterpolateV3(ctx->start, ctx->end, midf);

	int rht = Q1BSP_RecursiveHullTrace(ctx, node->children[side], p1f, midf, p1, mid, trace);
	if (rht != rht_empty && !trace->allsolid)
		return rht;
	rht = Q1BSP_RecursiveHullTrace(ctx, node->children[side ^ 1], midf, p2f, mid, p2, trace);
	if (rht != rht_solid)
		return rht;

	if (side) {
		/*we impacted the back of the node, so flip the plane*/
		trace->plane.dist = -plane->dist;
		trace->plane.normal = glms_vec3_negate(plane->normal);
		midf = (t1 + DIST_EPSILON) / (t1 - t2);
	} else {
		/*we impacted the front of the node*/
		trace->plane.dist = plane->dist;
		trace->plane.normal = plane->normal;
		midf = (t1 - DIST_EPSILON) / (t1 - t2);
	}

	t1 = glms_vec3_dot(trace->plane.normal, ctx->start) - trace->plane.dist;
	t2 = glms_vec3_dot(trace->plane.normal, ctx->end) - trace->plane.dist;
	midf = (t1 - DIST_EPSILON) / (t1 - t2);

	midf = clamp(midf, 0, 1);
	trace->fraction = midf;
	trace->endpos = InterpolateV3(ctx->start, ctx->end, midf);

	return rht_impact;
}

int SV_RecursiveHullCheck(const hull_t* const hull, const bbox_t* const move, trace_t* const trace, const unsigned int hitcontents) {
#if 1
	if (move->min.x == move->max.x && move->min.y == move->max.y && move->min.z == move->max.z) {
		/*points cannot cross planes, so do it faster*/
		int c = SV_HullPointContents(hull, hull->firstclipnode, move->min);
		trace->contents = c;
		if (hitcontents & CONTENTMASK_FROMQ1 (c))
			trace->startsolid = true;
		else {
			trace->allsolid = false;
			if (c == CONTENTS_EMPTY)
				trace->inopen = true;
			else if (c != CONTENTS_SOLID)
				trace->inwater = true;
		}
		return true;
	} else {
#endif
		struct rhtctx_s ctx;
		ctx.start = move->min;
		ctx.end = move->max;
		ctx.clipnodes = hull->clipnodes;
		ctx.planes = hull->planes;
		ctx.hitcontents = hitcontents;
		return Q1BSP_RecursiveHullTrace(&ctx, hull->firstclipnode, 0, 1, move->min, move->max, trace) != rht_impact;
	}
}

#if 0
// is angle (in degrees) within an arcsec of a mulitple of 90 degrees (ignoring gimbal lock)
static bool IsAxisAlignedDeg(const vec3s angle) {
#define ARCSECS_PER_RIGHT_ANGLE 324000
#define ARRSECS_PER_DEGREE      3600.f
	int remainder[3] = {
		((int)(angle.x * ARRSECS_PER_DEGREE) + 1) % ARCSECS_PER_RIGHT_ANGLE, ((int)(angle.y * ARRSECS_PER_DEGREE) + 1) % ARCSECS_PER_RIGHT_ANGLE,
		((int)(angle.z * ARRSECS_PER_DEGREE) + 1) % ARCSECS_PER_RIGHT_ANGLE};

	return (remainder[0] <= 2) && (remainder[1] <= 2) && (remainder[2] <= 2);
}
#endif

/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
static void SV_HullForBox(bsp_plane_t* box_planes, vec3s mins, vec3s maxs) {
	box_planes[0].dist = maxs.x;
	box_planes[1].dist = mins.x;
	box_planes[2].dist = maxs.y;
	box_planes[3].dist = mins.y;
	box_planes[4].dist = maxs.z;
	box_planes[5].dist = mins.z;
}

/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
static const hull_t* SV_HullForEntity(engine_t *e, edict_t *ent, const int32_t id, bbox_t *bbox, vec3s* offset) {
	vec3 size;
	const hull_t *hull;
	ecs_t *ecs = &e->ecs;

	if (ent->solid == SOLID_BSP) {
		glm_vec3_sub((float*)bbox->max.raw, (float*)bbox->min.raw, size);

		if (size[0] < 3.0f/BSP_RESIZE_DIV)
			hull = ent->hulls[0];
		else if (size[0] <= 32.0f/BSP_RESIZE_DIV)
			hull = ent->hulls[1];
		else
			hull = ent->hulls[2];

#ifdef VERBOSE
		myprintf("clip: %d %d %d\n", ent->hulls[0]->firstclipnode, ent->hulls[1]->firstclipnode, ent->hulls[2]->firstclipnode);
#endif

		glm_vec3_sub((float*)hull->clip_mins.raw, (float*)bbox->min.raw, offset->raw);
		glm_vec3_add(offset->raw, ecs->pos[id].raw, offset->raw);
	} else {
		bbox_t hullbox;
		bbox_t *ebbox = &ecs->bbox[id];
		glm_vec3_sub(ebbox->min.raw, (float*)bbox->max.raw, hullbox.min.raw);
		glm_vec3_sub(ebbox->max.raw, (float*)bbox->min.raw, hullbox.max.raw);
		SV_HullForBox(e->hb.box_planes, hullbox.min, hullbox.max);

		*offset = ecs->pos[id];
		hull = &e->hb.box_hull;
	}

	return hull;
}

#if 0
static void AngleVectors(const vec3s angles, vec3s* forward, vec3s* right, vec3s* up) {
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = angles.x;
	sy = sin (angle);
	cy = cos (angle);
	angle = angles.y;
	sp = sin (angle);
	cp = cos (angle);
	angle = angles.z;
	sr = sin (angle);
	cr = cos (angle);

	forward->x = cp * cy;
	forward->y = cp * sy;
	forward->z = -sp;
	right->x = (-1 * sr * sp * cy + -1 * cr * -sy);
	right->y = (-1 * sr * sp * sy + -1 * cr * cy);
	right->z = -1 * sr * cp;
	up->x = (cr * sp * cy + -sr * -sy);
	up->y = (cr * sp * sy + -sr * cy);
	up->z = cr * cp;
}
#endif

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
static trace_t SV_ClipMoveToEntity(engine_t* const e, edict_t* const ent, const int32_t id, bbox_t *move, bbox_t *aabb, const unsigned int hitcontents) {
	trace_t trace = {0};
	trace.allsolid = true;
	trace.fraction = 1;
	trace.endpos = move->max;

	// get the clipping hull
	vec3s offset;
	const hull_t *hull = SV_HullForEntity(e, ent, id, aabb, &offset);

	bbox_t move_l;
	glm_vec3_sub(move->min.raw, offset.raw, move_l.min.raw);
	glm_vec3_sub(move->max.raw, offset.raw, move_l.max.raw);

#if 0
	if (ent->solid == SOLID_BSP) {
#define DotProductTranspose(v, m, a) ((v).raw[0] * (m).raw[0][a] + (v).raw[1] * (m).raw[1][a] + (v).raw[2] * (m).raw[2][a])
		vec3s start_r, end_r, tmp;
		mat3s axis;
		AngleVectors((vec3s){.x=0,.y=0,.z=0}, &axis.col[0], &axis.col[1], &axis.col[2]);
		/* versors q = ecs->rot[id]; */
		/* vec3s forward, right, up; */
		/* vec3s dir = {{0,0,1}}; */
		/* glm_quat_rotatev(q.raw, dir.raw, forward.raw); */
		/* dir = (vec3s){{1,0,0}}; */
		/* glm_quat_rotatev(q.raw, dir.raw, right.raw); */
		/* dir = (vec3s){{0,1,0}}; */
		/* glm_quat_rotatev(q.raw, dir.raw, up.raw); */
		axis.col[1] = glms_vec3_negate(axis.col[1]);
		/* print_mat3("AngleVectors", &axis); */
		start_r.x = glms_vec3_dot(start_l, axis.col[0]);
		start_r.y = glms_vec3_dot(start_l, axis.col[1]);
		start_r.z = glms_vec3_dot(start_l, axis.col[2]);
		end_r.x = glms_vec3_dot(end_l, axis.col[0]);
		end_r.y = glms_vec3_dot(end_l, axis.col[1]);
		end_r.z = glms_vec3_dot(end_l, axis.col[2]);
		SV_RecursiveHullCheck(hull, start_r, end_r, &trace, hitcontents);
		tmp = trace.endpos;
		const mat3s transposed_axis = glms_mat3_transpose(axis);
		trace.endpos.x = glms_vec3_dot(tmp, transposed_axis.col[0]);
		trace.endpos.y = glms_vec3_dot(tmp, transposed_axis.col[1]);
		trace.endpos.z = glms_vec3_dot(tmp, transposed_axis.col[2]);
		/* trace.endpos.x = DotProductTranspose (tmp, *axis, 0); */
		/* trace.endpos.y = DotProductTranspose (tmp, *axis, 1); */
		/* trace.endpos.z = DotProductTranspose (tmp, *axis, 2); */
		tmp = trace.plane.normal;
		/* trace.plane.normal.x = DotProductTranspose (tmp, axis, 0); */
		/* trace.plane.normal.y = DotProductTranspose (tmp, axis, 1); */
		/* trace.plane.normal.z = DotProductTranspose (tmp, axis, 2); */
		trace.plane.normal.x = glms_vec3_dot(tmp, transposed_axis.col[0]);
		trace.plane.normal.y = glms_vec3_dot(tmp, transposed_axis.col[1]);
		trace.plane.normal.z = glms_vec3_dot(tmp, transposed_axis.col[2]);
	} else {
#endif
		SV_RecursiveHullCheck(hull, &move_l, &trace, hitcontents);
	/* } */

	// fix trace up by the offset
	if (trace.fraction != 1)
		trace.endpos = glms_vec3_add(trace.endpos, offset);

	// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid)
		trace.ent = ent;

	return trace;
}

typedef struct {
	bbox_t box;   // enclose the test object along entire move
	bbox_t size;  // size of the moving object
	bbox_t size2; // size when clipping against monsters
	bbox_t move;  // start and end
	trace_t trace;
	int32_t id;
	int32_t owner;
	int32_t type;
	unsigned int hitcontents; // content types to impact upon (1<<-CONTENTS_FOO) bitmask
} moveclip_t;

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
#define STRUCT_FROM_LINK(l, t, m) ((t*)((uint8_t*)l - offsetof (t, m)))
#define EDICT_FROM_AREA(l) STRUCT_FROM_LINK (l, edict_t, basic.area)
#define EDICT_TO_PROG(e) ((uint8_t*)e - (uint8_t*)e->qcvm.edicts)
#define PROG_TO_EDICT(e) ((edict_t*)((uint8_t*)e->qcvm.edicts + e))
static void SV_ClipToLinks(engine_t *e, areanode_t *node, moveclip_t *clip) {
	link_t *l, *next;
	edict_t *touch;
	trace_t trace;

	// touch linked edicts
	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->solid == SOLID_NOT)
			continue;
		if (touch->basic.id >= 0 && touch->basic.id == clip->id)
			continue;
		if (touch->solid == SOLID_TRIGGER)
			myprintf("Trigger in clipping list\n");

		/* if (clip->type == MOVE_NOMONSTERS && touch->solid != SOLID_BSP) */
		/* 	continue; */

#if 0
		if (touch->solid == SOLID_SLIDEBOX) {
			print_vec3("XXentmin", clip->box.min);
			print_vec3("XXentmax", clip->box.max);
			print_vec3("XXtouchmin", touch->absbox.min);
			print_vec3("XXtouchmax", touch->absbox.max);
			myprintf("XX:%d %d\n", touch->id, touch->solid);
		}
#endif

		if (clip->box.min.x > touch->absbox.max.x || clip->box.min.y > touch->absbox.max.y || clip->box.min.z > touch->absbox.max.z ||
			clip->box.max.x < touch->absbox.min.x || clip->box.max.y < touch->absbox.min.y || clip->box.max.z < touch->absbox.min.z)
			continue;

		/* if (clip->id && clip->passedict->v.size[0] && !touch->v.size[0]) */
		/* 	continue; // points never interact */

		// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return;
		if (clip->id >= 0) {
			if (touch->owner == clip->id)
				continue; // don't clip against own missiles
			if (clip->owner == touch->basic.id)
				continue; // don't clip against owner
		}

#if 0
		if (touch->v.skin < 0) {
			if (!(clip->hitcontents & (1 << -(int)touch->v.skin)))
				continue; // not solid, don't bother trying to clip.
			if ((int)touch->v.flags & FL_MONSTER)
				trace = SV_ClipMoveToEntity(touch, clip->start, clip->mins2, clip->maxs2, clip->end, ~(1u << -CONTENTS_EMPTY));
			else
				trace = SV_ClipMoveToEntity(touch, clip->start, clip->mins, clip->maxs, clip->end, ~(1u << -CONTENTS_EMPTY));
			if (trace.contents != CONTENTS_EMPTY)
				trace.contents = touch->v.skin;
		} else {
			if ((int)touch->v.flags & FL_MONSTER)
				trace = SV_ClipMoveToEntity(touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->hitcontents);
			else
#endif
				trace = SV_ClipMoveToEntity(e, touch, touch->basic.id, &clip->move, &clip->size, clip->hitcontents);
		/* } */

		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction) {
			trace.ent = touch;
			if (clip->trace.startsolid) {
				clip->trace = trace;
				clip->trace.startsolid = true;
			} else {
				clip->trace = trace;
			}
		} else if (trace.startsolid) {
			clip->trace.startsolid = true;
		}
	}

	// recurse down both sides
	if (node->axis == -1)
		return;

	if (clip->box.max.raw[node->axis] > node->dist)
		SV_ClipToLinks(e, node->children[0], clip);
	if (clip->box.min.raw[node->axis] < node->dist)
		SV_ClipToLinks(e, node->children[1], clip);
}

static void SV_MoveBounds(const bbox_t* const move, const bbox_t* const size, bbox_t *out) {
#if 0
// debug to test against everything
box.min[0] = box.min[1] = box.min[2] = -9999;
box.max[0] = box.max[1] = box.max[2] = 9999;
#else
	for (int i = 0; i < 3; i++) {
		if (move->max.raw[i] > move->min.raw[i]) {
			out->min.raw[i] = move->min.raw[i] + size->min.raw[i] - 1.0/BSP_RESIZE_DIV;
			out->max.raw[i] = move->max.raw[i] + size->max.raw[i] + 1.0/BSP_RESIZE_DIV;
		} else {
			out->min.raw[i] = move->max.raw[i] + size->min.raw[i] - 1.0/BSP_RESIZE_DIV;
			out->max.raw[i] = move->min.raw[i] + size->max.raw[i] + 1.0/BSP_RESIZE_DIV;
		}
	}
#endif
}

trace_t SV_Move(engine_t *e, const int32_t id, bbox_t *move, int type) {
	moveclip_t clip = {0};
	clip.id = id;

	if (type & MOVE_HITALLCONTENTS)
		clip.hitcontents = ~0u;
	else
		clip.hitcontents = CONTENTMASK_ANYSOLID;

	/* clip to world */
	const int32_t map_id = e->ecs.qcvm.map_id;
	clip.trace = SV_ClipMoveToEntity(e, e->ecs.edict[map_id].edict, map_id, move, &e->ecs.bbox[id], clip.hitcontents);

	clip.move = *move;
	clip.size = e->ecs.bbox[id];
	clip.type = type & 3;

	if (type == MOVE_MISSILE) {
		for (int i=0; i<3; i++) {
			clip.size2.min.raw[i] = -15 / BSP_RESIZE_DIV;
			clip.size2.max.raw[i] = 15 / BSP_RESIZE_DIV;
		}
	} else {
		clip.size2 = clip.size;
	}

	/* create the bounding box of the entire move */
	SV_MoveBounds(move, &clip.size2, &clip.box);

	/* clip to entities */
	SV_ClipToLinks(e, e->ecs.qcvm.areanodes, &clip);

	return clip.trace;
}

static void SV_Impact(engine_t* e, const edict_t* e1, const edict_t* e2) {
	if (e1->touch && e1->solid != SOLID_NOT) {
#ifdef VERBOSE
	myprintf("[SV_Impact] (e1) e1:%d e2:%d\n", e1, e2);
#endif
		e1->touch(e, e1->basic.id, e2->basic.id);
	}

	if (e2->touch && e2->solid != SOLID_NOT) {
#ifdef VERBOSE
	myprintf("[SV_Impact] (e2) e1:%d e2:%d\n", e1, e2);
#endif
		e2->touch(e, e2->basic.id, e1->basic.id);
	}
}

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
static int ClipVelocity(const vec3s in, const vec3s normal, vec3s* out, const float overbounce) {
	float backoff;
	int blocked;

	blocked = 0;
	if (normal.y > 0)
		blocked |= 1; // floor
	if (!normal.y)
		blocked |= 2; // step

	backoff = glms_vec3_dot(in, normal) * overbounce;

	const vec3s change = glms_vec3_scale(normal, backoff);
	vec3s tout = glms_vec3_sub(in, change);
	for (int i=0; i<3; i++) {
		if (tout.raw[i] > -FLT_EPSILON && tout.raw[i] < FLT_EPSILON)
			tout.raw[i] = 0;
	}
	*out = tout;

	return blocked;
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
#define MAX_CLIP_PLANES 5
int SV_FlyMove(engine_t *e, const int32_t id, const float time, trace_t *steptrace) {
	ecs_t* const ecs = &e->ecs;
	vec3s dir;
	float d;
	vec3s planes[MAX_CLIP_PLANES];
	int i;
	trace_t trace;

	int numplanes = 0;
	int numbumps = 4;
	int blocked = 0;
	vec3s* const pvel = &ecs->vel[id];
	vec3s original_velocity = *pvel;
	vec3s primal_velocity = original_velocity;
	vec3s new_velocity = original_velocity;

	float time_left = time;

	for (int bumpcount=0; bumpcount<numbumps; bumpcount++) {
		if (!pvel->x && !pvel->y && !pvel->z)
			break;

		bbox_t move;
		move.min = ecs->pos[id];
		glm_vec3_scale(pvel->raw, time_left, move.max.raw);
		glm_vec3_add(move.min.raw, move.max.raw, move.max.raw);
		move.max = glms_vec3_add(move.min, glms_vec3_scale(*pvel, time_left));

		trace = SV_Move(e, id, &move, false);

		if (trace.allsolid) {
			// entity is trapped in another solid
#ifdef VERBOSE
			myprintf("[SV_FlyMove] trace.allsolid reseting to origin\n");
#endif
			*pvel = GLMS_VEC3_ZERO;
			return 3;
		}

		if (trace.fraction > 0) {
			// actually covered some distance
			ecs->pos[id] = trace.endpos;
			original_velocity = *pvel;
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break; // moved the entire distance

		/* TODO check */
		/* if (!trace.ent) */
		/* 	Sys_Error ("SV_FlyMove: !trace.ent"); */

		edict_t* const ent = ecs->edict[id].edict;
		if (trace.plane.normal.y > 0.7) {
			blocked |= 1; // floor
			/* TODO always BSP for now */
			/* if (trace.ent->v.solid == SOLID_BSP) */
			/* { */
				ent->flags |= FL_ONGROUND;
				/* ent->v.groundentity = EDICT_TO_PROG (trace.ent); */
			/* } */
		}
		if (!trace.plane.normal.y) {
			blocked |= 2; // step
			if (steptrace)
				*steptrace = trace; // save for player extrafriction
		}

		// run the impact function
		SV_Impact(e, ent, trace.ent);
		/* if (ent->free) */
		/* 	break; // removed by the impact function */

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {
			fputs("this shouldn't really happen\n", stdout);
			*pvel = GLMS_VEC3_ZERO;
			return 3;
		}

		planes[numplanes] = trace.plane.normal;
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++) {
			ClipVelocity(original_velocity, planes[i], &new_velocity, 1);
			int j;
			for (j = 0; j < numplanes; j++)
				if (j != i) {
					if (glms_vec3_dot(new_velocity, planes[j]) < 0)
						break; // not ok
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{ // go along this plane
			*pvel = new_velocity;
		} else {
			/* go along the crease */
			if (numplanes != 2) {
				*pvel = GLMS_VEC3_ZERO;
				return 7;
			}
			dir = glms_vec3_cross(planes[0], planes[1]);
			d = glms_vec3_dot(dir, *pvel);
			*pvel = glms_vec3_scale(dir, d);
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (glms_vec3_dot(*pvel, primal_velocity) <= 0) {
			*pvel = GLMS_VEC3_ZERO;
			return blocked;
		}
	}

	return blocked;
}

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity(engine_t* const e, const int32_t id, const vec3s push) {
	trace_t trace;
	bbox_t move;
	move.min = e->ecs.pos[id];
	glm_vec3_add(move.min.raw, (float*)push.raw, move.max.raw);

	/* if (ent->v.movetype == MOVETYPE_FLYMISSILE) */
	/* 	trace = SV_Move (start, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent); */
	/* else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT) */
	/* 	// only clip against bmodels */
	/* 	trace = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent); */
	/* else */
	edict_t* edict = e->ecs.edict[id].edict;
	trace = SV_Move(e, id, &move, edict->movetype);

	e->ecs.pos[id] = trace.endpos;
	SV_LinkEdict(e, edict, true);

	if (trace.ent)
		SV_Impact(e, edict, trace.ent);

	return trace;
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
static int SV_TryUnstick(engine_t *e, ecs_t *ecs, const int32_t id, const vec3s oldvel) {
#ifdef VERBOSE2
	myprintf("[SV_TryUnstick]\n");
#endif
	int i;
	int clip;
	trace_t steptrace;

	vec3s* const ppos = &ecs->pos[id];
	vec3s* const pvel = &ecs->vel[id];
	const vec3s oldorg = *ppos;
	vec3s dir = GLMS_VEC3_ZERO;

	for (i = 0; i < 8; i++) {
		// try pushing a little in an axial direction
		switch (i)
		{
		case 0:
			dir.x = 2.0f/BSP_RESIZE_DIV;
			dir.z = 0;
			break;
		case 1:
			dir.x = 0;
			dir.z = 2.0f/BSP_RESIZE_DIV;
			break;
		case 2:
			dir.x = -2.0f/BSP_RESIZE_DIV;
			dir.z = 0;
			break;
		case 3:
			dir.x = 0;
			dir.z = -2.0f/BSP_RESIZE_DIV;
			break;
		case 4:
			dir.x = 2.0f/BSP_RESIZE_DIV;
			dir.z = 2.0f/BSP_RESIZE_DIV;
			break;
		case 5:
			dir.x = -2.0f/BSP_RESIZE_DIV;
			dir.z = 2.0f/BSP_RESIZE_DIV;
			break;
		case 6:
			dir.x = 2.0f/BSP_RESIZE_DIV;
			dir.z = -2.0f/BSP_RESIZE_DIV;
			break;
		case 7:
			dir.x = -2.0f/BSP_RESIZE_DIV;
			dir.z = -2.0f/BSP_RESIZE_DIV;
			break;
		}

		SV_PushEntity(e, id, dir);

		// retry the original move
		pvel->x = oldvel.x;
		pvel->y = 0;
		pvel->z = oldvel.z;
		clip = SV_FlyMove(e, id, 0.1, &steptrace);

		if (fabsf(oldorg.z - ppos->z) > 4.0f/BSP_RESIZE_DIV || fabsf(oldorg.x - ppos->x) > 4.0f/BSP_RESIZE_DIV) {
			fputs("clip unstuck!\n", stdout);
			return clip;
		}

		// go back to the original pos and try again
		*ppos = oldorg;
	}

	*pvel = GLMS_VEC3_ZERO;
	return 7; // still not moving
}

static inline vec3s quake_get_forward(const versors q) {
	vec3s dir;
	dir.x = 2.0f * (q.x*q.z + q.w*q.y);
	dir.y = 2.0f * (q.y*q.z - q.w*q.x);
	dir.z = 1.0f - 2.0f * (q.x*q.x + q.y*q.y);
	return dir;
}

static void SV_WallFriction(ecs_t* ecs, const int32_t id, trace_t *trace) {
#ifdef VERBOSE2
	myprintf("[SV_WallFriction] clip\n");
#endif
	const vec3s forward = quake_get_forward(ecs->rot[id]);
	float d = glm_vec3_dot(trace->plane.normal.raw, forward.raw);

	d += 0.5;
	if (d >= 0)
		return;

	/* cut the tangential velocity */
	vec3s* const pvel = &ecs->vel[id];
	const float i = glm_vec3_dot(trace->plane.normal.raw, pvel->raw);
	vec3s into = glms_vec3_scale(trace->plane.normal, i);
	vec3s side;
	glm_vec3_sub(pvel->raw, into.raw, side.raw);

	pvel->x = side.x * (1 + d);
	pvel->z = side.z * (1 + d);
}

void SV_UserFriction(engine_t* e, const i32 id, const f32 delta) {
	float newspeed, control;
	float friction;
	ecs_t *ecs = &e->ecs;

	vec3s vel = ecs->vel[id];
	float speed = sqrtf(vel.x * vel.x + vel.z * vel.z);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	const vec3s origin = ecs->pos[id];
	bbox_t move;
	move.min.x = move.max.x = origin.x + vel.x / speed * 16;
	move.min.z = move.max.z = origin.z + vel.z / speed * 16;
	move.min.y = origin.y + ecs->bbox[id].min.y;
	move.max.y = move.min.y - (34.0f/BSP_RESIZE_DIV);

	const trace_t trace = SV_Move(e, id, &move, true);

	/* TODO investigate */
#define sv_friction 4.0 // 4 is Q1 default
#define sv_edgefriction 2.0f
#define sv_stopspeed 3.125f
	if (trace.fraction == 1.0)
		friction = sv_friction * sv_edgefriction;
	else
		friction = sv_friction;

	// apply friction
	control = speed < sv_stopspeed ? sv_stopspeed : speed;
	newspeed = speed - delta * control * friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	glm_vec3_scale(vel.raw, newspeed, vel.raw);
	ecs->vel[id] = vel;
}

#if 1
static void ClearLink(link_t *l) {
	l->prev = l->next = l;
}

static void RemoveLink(link_t *l) {
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

static void InsertLinkBefore(link_t *l, link_t *before) {
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void SV_UnlinkEdict(edict_t *ent) {
	if (!ent->basic.area.prev)
		return; // not linked in anywhere
	RemoveLink(&ent->basic.area);
	ent->basic.area.prev = ent->basic.area.next = NULL;
}

#if 0
static void InsertLinkAfter(link_t *l, link_t *after) {
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

static bool IsOriginWithinMinMax (vec3 origin, vec3 mins, vec3 maxs) {
	return origin[0] > mins[0] && origin[1] > mins[1] && origin[2] > mins[2] && origin[0] < maxs[0] && origin[1] < maxs[1] && origin[2] < maxs[2];
}
#endif

static int BoxOnPlaneSide(const vec3 emins, const vec3 emaxs, const bsp_plane_t* const p) {
	float dist1, dist2;
	int sides;

	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal.x * emaxs[0] + p->normal.y * emaxs[1] + p->normal.z * emaxs[2];
		dist2 = p->normal.x * emins[0] + p->normal.y * emins[1] + p->normal.z * emins[2];
		break;
	case 1:
		dist1 = p->normal.x * emins[0] + p->normal.y * emaxs[1] + p->normal.z * emaxs[2];
		dist2 = p->normal.x * emaxs[0] + p->normal.y * emins[1] + p->normal.z * emins[2];
		break;
	case 2:
		dist1 = p->normal.x * emaxs[0] + p->normal.y * emins[1] + p->normal.z * emaxs[2];
		dist2 = p->normal.x * emins[0] + p->normal.y * emaxs[1] + p->normal.z * emins[2];
		break;
	case 3:
		dist1 = p->normal.x * emins[0] + p->normal.y * emins[1] + p->normal.z * emaxs[2];
		dist2 = p->normal.x * emaxs[0] + p->normal.y * emaxs[1] + p->normal.z * emins[2];
		break;
	case 4:
		dist1 = p->normal.x * emaxs[0] + p->normal.y * emaxs[1] + p->normal.z * emins[2];
		dist2 = p->normal.x * emins[0] + p->normal.y * emins[1] + p->normal.z * emaxs[2];
		break;
	case 5:
		dist1 = p->normal.x * emins[0] + p->normal.y * emaxs[1] + p->normal.z * emins[2];
		dist2 = p->normal.x * emaxs[0] + p->normal.y * emins[1] + p->normal.z * emaxs[2];
		break;
	case 6:
		dist1 = p->normal.x * emaxs[0] + p->normal.y * emins[1] + p->normal.z * emins[2];
		dist2 = p->normal.x * emins[0] + p->normal.y * emaxs[1] + p->normal.z * emaxs[2];
		break;
	case 7:
		dist1 = p->normal.x * emins[0] + p->normal.y * emins[1] + p->normal.z * emins[2];
		dist2 = p->normal.x * emaxs[0] + p->normal.y * emaxs[1] + p->normal.z * emaxs[2];
		break;
	default:
		dist1 = dist2 = 0; // shut up compiler
		myprintf("BoxOnPlaneSide: bad signbits\n");
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

#ifdef PARANOID
	if (sides == 0)
		myprintf("BoxOnPlaneSide: sides==0\n");
#endif

	return sides;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

/* TODO put this in the engine probably */
static i32 fatbytes;
static u8* fatpvs;
static i32 fatpvs_capacity;

void SV_AddToFatPVS(vec3s org, const bsp_mnode_t* node, const bsp_qmodel_t* const worldmodel) { //johnfitz -- added worldmodel as a parameter
	u8* pvs;
	const bsp_plane_t* plane;

	while (1) {
	// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0) {
			if (node->contents != CONTENTS_SOLID) {
				pvs = Mod_LeafPVS( (bsp_mleaf_t *)node, worldmodel); //johnfitz -- worldmodel as a parameter
				for (int i=0; i<fatbytes; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		const float d = glm_vec3_dot(org.raw, (float*)plane->normal.raw) - plane->dist;
		if (d > 8.0f/BSP_RESIZE_DIV) {
			node = node->children[0];
		} else if (d < -8.0f/BSP_RESIZE_DIV) {
			node = node->children[1];
		} else { // go down both
			SV_AddToFatPVS(org, node->children[0], worldmodel); //johnfitz -- worldmodel as a parameter
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
u8* SV_FatPVS(vec3s org, const bsp_qmodel_t* const worldmodel) { //johnfitz -- added worldmodel as a parameter
	fatbytes = (worldmodel->numleafs+7)>>3; // ericw -- was +31, assumed to be a bug/typo
	if (fatpvs == NULL || fatbytes > fatpvs_capacity) {
		fatpvs_capacity = fatbytes;
		fatpvs = (u8*)realloc(fatpvs, fatpvs_capacity); // TODO alloc this somewhere else
		if (!fatpvs)
			myprintf("SV_FatPVS: realloc() failed on %d bytes", fatpvs_capacity);
	}

	memset(fatpvs, 0, fatbytes);
	SV_AddToFatPVS(org, worldmodel->nodes, worldmodel); //johnfitz -- worldmodel as a parameter
	return fatpvs;
}

bool SV_EdictInPVS(edict_t *test, u8 *pvs) {
	for (int i = 0 ; i < test->basic.num_leafs ; i++)
		if (pvs[test->basic.leafnums[i] >> 3] & (1 << (test->basic.leafnums[i] & 7)))
			return true;
	return false;
}

bool SV_VisibleToClient(vec3s pos, edict_t *test, bsp_qmodel_t *worldmodel) {
	u8 *pvs;
	/* VectorAdd (client->v.origin, client->v.view_ofs, org); */
	pvs = SV_FatPVS(pos, worldmodel);
	return SV_EdictInPVS(test, pvs);
}

static void SV_FindTouchedLeafs(engine_t *e, edict_t* const ent, const bsp_mnode_t* const node) {
	const bsp_mleaf_t *leaf;
	int leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;

	// add an efrag if the node is a leaf
	if (node->contents < 0) {
		if (ent->basic.num_leafs == MAX_ENT_LEAFS)
			return;

		leaf = (bsp_mleaf_t*)node;
		const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
		leafnum = leaf - worldmodel->leafs - 1;

		ent->basic.leafnums[ent->basic.num_leafs] = leafnum;
		ent->basic.num_leafs++;
		return;
	}

	// NODE_MIXED
	const bsp_plane_t* const splitplane = node->plane;
	const int sides = BoxOnPlaneSide(ent->absbox.min.raw, ent->absbox.max.raw, splitplane);

	// recurse down the contacted sides
	if (sides & 1)
		SV_FindTouchedLeafs(e, ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs(e, ent, node->children[1]);
}

static void SV_AreaTriggerEdicts(edict_t *ent, areanode_t *node, edict_t **list, int *listcount) {
	link_t *l, *next;
	edict_t *touch;

	// touch linked edicts
	for (l=node->trigger_edicts.next; l!=&node->trigger_edicts; l=next) {
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch == ent) {
			continue;
		} else if (!touch->touch || touch->solid != SOLID_TRIGGER) {
			continue;
		}
		if (touch->solid == SOLID_SLIDEBOX) {
			print_vec3("XXentmin", ent->absbox.min);
			print_vec3("XXentmax", ent->absbox.max);
			print_vec3("XXtouchmin", touch->absbox.min);
			print_vec3("XXtouchmax", touch->absbox.max);
			myprintf("XX:%d %d\n", touch->basic.id, touch->solid);
		}

		if (ent->absbox.min.x > touch->absbox.max.x || ent->absbox.min.y > touch->absbox.max.y ||
			 ent->absbox.min.z > touch->absbox.max.z || ent->absbox.max.x < touch->absbox.min.x ||
			 ent->absbox.max.y < touch->absbox.min.y || ent->absbox.max.z < touch->absbox.min.z)
			continue;

		list[*listcount] = touch;
		(*listcount)++;
	}

	// recurse down both sides
	if (node->axis == -1)
		return;

	if (ent->absbox.max.raw[node->axis] > node->dist)
		SV_AreaTriggerEdicts(ent, node->children[0], list, listcount);
	if (ent->absbox.min.raw[node->axis] < node->dist)
		SV_AreaTriggerEdicts(ent, node->children[1], list, listcount);
}

/*
====================
SV_TouchLinks

ericw -- copy the touching edicts to an array (on the hunk) so we can avoid
iteating the trigger_edicts linked list while calling PR_ExecuteProgram
which could potentially corrupt the list while it's being iterated.
Based on code from Spike.
====================
*/
static void SV_TouchLinks(engine_t* const e, edict_t* const ent) {
	static edict_t** list = NULL; // TODO do better?
	arrsetlen(list, e->ecs.qcvm.edict_cnt);

	int listcount = 0;
	SV_AreaTriggerEdicts(ent, e->ecs.qcvm.areanodes, list, &listcount);

	for (int i=0; i<listcount; i++) {
		edict_t *touch = list[i];
		// re-validate in case of PR_ExecuteProgram having side effects that make edicts later in the list no longer touch
		if (touch == ent)
			continue;
		if (!touch->touch || touch->solid != SOLID_TRIGGER)
			continue;
		if (ent->absbox.min.x > touch->absbox.max.x || ent->absbox.min.y > touch->absbox.max.y || ent->absbox.min.z > touch->absbox.max.z ||
			ent->absbox.max.x < touch->absbox.min.x || ent->absbox.max.y < touch->absbox.min.y || ent->absbox.max.z < touch->absbox.min.z)
			continue;

		touch->touch(e, touch->basic.id, ent->basic.id);
	}
}

void SV_LinkEdict(engine_t* e, edict_t *ent, bool touch_triggers) {
	areanode_t *node;

	if (ent->basic.area.prev)
		SV_UnlinkEdict(ent); // unlink from old position

	/* needed? */
	/* if (ent == qcvm->edicts) */
	/* 	return; // don't add the world */

#if 0
	// set the abs box
	if (ent->solid == SOLID_BSP && IsOriginWithinMinMax (ent->v.origin, ent->v.mins, ent->v.maxs) &&
		!IsAxisAlignedDeg (ent->v.angles))
	{ // expand for rotation the lame way. hopefully there's an origin brush in there.
		int i;
		float v1, v2;
		vec3s max;
		// q2 method
		for (i = 0; i < 3; i++) {
			v1 = fabs (ent->v.mins[i]);
			v2 = fabs (ent->v.maxs[i]);
			max[i] = q_max (v1, v2);
		}
		v1 = sqrtf(DotProduct (max, max));
		for (i = 0; i < 3; i++) {
			ent->v.absmin[i] = ent->v.origin[i] - v1;
			ent->v.absmax[i] = ent->v.origin[i] + v1;
		}
	}
	else
	{
		VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
		VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);
	}

	//
	// to make items easier to pick up and allow them to be grabbed off
	// of shelves, the abs sizes are expanded
	//
	if ((int)ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
	}
	else
	{ // because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v.absmin[0] -= 1;
		ent->v.absmin[1] -= 1;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 1;
		ent->v.absmax[1] += 1;
		ent->v.absmax[2] += 1;
	}
#endif

	/* link to PVS leafs */
	ent->basic.num_leafs = 0;
	/* TODO do the model check? */
	/* if (ent->v.modelindex) */
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
	SV_FindTouchedLeafs(e, ent, worldmodel->nodes);

	if (ent->solid == SOLID_NOT)
		return;

	/* find the first node that the ent's box crosses */
	node = e->ecs.qcvm.areanodes;
	while (1) {
		if (node->axis == -1)
			break;
		if (ent->absbox.min.raw[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absbox.max.raw[node->axis] < node->dist)
			node = node->children[1];
		else
			break; // crosses the node
	}

	/* link it in */
	if (ent->solid == SOLID_TRIGGER)
		InsertLinkBefore(&ent->basic.area, &node->trigger_edicts);
	else
		InsertLinkBefore(&ent->basic.area, &node->solid_edicts);

	/* if touch_triggers, touch all entities at this node and decend for more */
	if (touch_triggers)
		SV_TouchLinks(e, ent);
}

static bsp_mleaf_t* Mod_PointInLeaf(const vec3 p, const bsp_qmodel_t* const model) {
	if (!model || !model->nodes)
		myprintf("Mod_PointInLeaf: bad model\n");

	const bsp_mnode_t *node = model->nodes;
	while (1) {
		if (node->contents < 0)
			return (bsp_mleaf_t*)node;
		const bsp_plane_t* const plane = node->plane;
		const float d = glm_vec3_dot(p, plane->normal.raw) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL; // never reached
}

void SV_UnlinkLight(edict_light_t *ent) {
	if (!ent->basic.area.prev)
		return; // not linked in anywhere
	RemoveLink(&ent->basic.area);
	ent->basic.area.prev = ent->basic.area.next = NULL;
}

void SV_LinkLight(engine_t* e, edict_light_t *ent) {
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
	bsp_mleaf_t* const leaf = Mod_PointInLeaf(ent->pos.raw, worldmodel);
	assert(leaf);

	if (ent->basic.area.prev)
		SV_UnlinkLight(ent); // unlink from old position
	InsertLinkBefore(&ent->basic.area, &leaf->lights);
}

void SV_LinkLightDynamic(engine_t* e, edict_light_t *ent) {
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
	bsp_mleaf_t* const leaf = Mod_PointInLeaf(ent->pos.raw, worldmodel);
	assert(leaf);

	if (ent->basic.area.prev)
		SV_UnlinkLight(ent); // unlink from old position
	InsertLinkBefore(&ent->basic.area, &leaf->lights_dynamic);
}
#endif

/* SV_WalkMove: Only used by players */
void SV_WalkMove(engine_t *e, const int32_t id, const float delta) {
	vec3s upmove, downmove;
	vec3s oldvel;
	vec3s nosteporg, nostepvel;
	int32_t clip;
	/* int32_t oldonground; */
	trace_t steptrace, downtrace;

	ecs_t* ecs = &e->ecs;
	vec3s* ppos = &ecs->pos[id];
	vec3s* pvel = &ecs->vel[id];

	// do a regular slide move unless it looks like you ran into a step
	/* oldonground = pflags->onground; */
	edict_t* const edict = ecs->edict[id].edict;
	edict->flags &= ~FL_ONGROUND;

	const vec3s oldorg = *ppos;
	oldvel = *pvel;

	clip = SV_FlyMove(e, id, delta, &steptrace);

	if (!(clip & 2))
		return; // move didn't block on a step

	/* if (!oldonground && ent->v.waterlevel == 0) */
	/* 	return; // don't stair up while jumping */

	/* if (ent->v.movetype != MOVETYPE_WALK) */
	/* 	return; // gibbed by a trigger */

	/* if ((int)sv_player->v.flags & FL_WATERJUMP) */
	/* 	return; */

	nosteporg = *ppos;
	nostepvel = *pvel;

	// try moving up and forward to go up a step
	*ppos = oldorg; // back to start pos

	upmove = GLMS_VEC3_ZERO;
	downmove = GLMS_VEC3_ZERO;
#define STEPSIZE (18.0f/BSP_RESIZE_DIV)
	upmove.y = STEPSIZE;
	downmove.y = -STEPSIZE + oldvel.y * delta;

	/* move up */
	SV_PushEntity(e, id, upmove); // FIXME: don't link?

	/* move forward */
	pvel->x = oldvel.x;
	pvel->y = 0;
	pvel->z = oldvel.z;
	clip = SV_FlyMove(e, id, delta, &steptrace);

	/* check for stuckness, possibly due to the limited precision of floats in the clipping hulls */
	if (clip) {
		if (fabsf(oldorg.z - ppos->z) < DIST_EPSILON && fabsf(oldorg.x - ppos->x) < DIST_EPSILON) {
			/* stepping up didn't make any progress */
			clip = SV_TryUnstick(e, ecs, id, oldvel);
		}
	}

	/* extra friction based on view angle */
	if (clip & 2)
		SV_WallFriction(ecs, id, &steptrace);

	/* move down */
	downtrace = SV_PushEntity(e, id, downmove); // FIXME: don't link?

	if (downtrace.plane.normal.y > 0.7) {
		/* if (ent->v.solid == SOLID_BSP) { */
			edict->flags |= FL_ONGROUND;
			/* ent->v.groundentity = EDICT_TO_PROG (downtrace.ent); */
		/* } */
	} else {
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		*ppos = nosteporg;
		*pvel = nostepvel;
	}
}

areanode_t *SV_CreateAreaNode(qcvm_t* qcvm, int depth, const vec3 mins, const vec3 maxs) {
	areanode_t *anode;
	vec3 size;
	vec3 mins1, maxs1, mins2, maxs2;

	anode = &qcvm->areanodes[qcvm->numareanodes];
	qcvm->numareanodes++;

	ClearLink(&anode->trigger_edicts);
	ClearLink(&anode->solid_edicts);

	glm_vec3_sub((float*)maxs, (float*)mins, size);
	if (size[0] > size[2])
		anode->axis = 0;
	else
		anode->axis = 1;

	if (depth == MAX_AREA_DEPTH || size[anode->axis] < 500/BSP_RESIZE_DIV) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	glm_vec3_copy((float*)mins, mins1);
	glm_vec3_copy((float*)mins, mins2);
	glm_vec3_copy((float*)maxs, maxs1);
	glm_vec3_copy((float*)maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode(qcvm, depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode(qcvm, depth + 1, mins1, maxs1);

	return anode;
}

void SV_ClearWorld(engine_t *e) {
#ifndef NDEBUG
	fputs("[SV_ClearWorld]\n", stdout);
#endif

	e->vfx_flags = (vfx_flags_t){0};

	ecs_t* const ecs = &e->ecs;
	const i32 map_idx = new_bsp_worldmodel(ecs, &e->assets.map, 0);
	ecs->qcvm.map_id = map_idx;
	new_visflags(&e->assets.map.qmods[0].model, e->visflags, MAX_VISFLAGS);

#ifndef NDEBUG
	const bsp_dmodel_t* model = e->assets.map.dmodels;
	for (uint32_t i=0; i<e->assets.map.model_cnt; i++, model++) {
		myprintf("[BSP] model %d firstface:%d numfaces:%d visleafs:%d\n", i, model->firstface, model->numfaces, model->visleafs);
		print_vec3("[BSP] model->origin:", model->origin);
		print_vec3("[BSP] model->mins:", model->bbox.min);
		print_vec3("[BSP] model->maxs:", model->bbox.max);
	}
#endif

	SV_InitBoxHull(&e->hb);

	ecs->qcvm.numareanodes = 0;
	memset(ecs->qcvm.areanodes, 0, sizeof(areanode_t)*AREA_NODES);
	SV_CreateAreaNode(&ecs->qcvm, 0, e->assets.map.qmods[0].bbox.min.raw, e->assets.map.qmods[0].bbox.max.raw);

	for (u32 i=0; i<e->assets.map.leaf_cnt; i++) {
		bsp_mleaf_t* const leaf = &e->assets.map.leafs[i];
		ClearLink(&leaf->lights);
		ClearLink(&leaf->lights_dynamic);
	}

	bool found_player = false;
	vec3s player_start = {0};
	float player_angle = 0;
	bsp_t *bsp = &e->assets.map;

	/* Create Entities */
	for (size_t i=0; i<arrlenu(bsp->entities); i++) {
		bsp_entity_t* const ent = &bsp->entities[i];
		switch (ent->classname) {
			case BSP_ENTITY_MOB_CHUD:
				new_chud(e, ent->origin, (float)-ent->angle*M_PI/180.0+M_PI, ent->task);
				break;
			case BSP_ENTITY_MOB_MUTT:
				new_mutt(e, ent->origin, (float)-ent->angle*M_PI/180.0+M_PI, ent->task);
				break;
			case BSP_ENTITY_MOB_TROON:
				new_troon(e, ent->origin, (float)-ent->angle*M_PI/180.0+M_PI, ent->task);
				break;
			case BSP_ENTITY_LIGHT:
				{
					edict_light_t* const edict = new_light(ecs, ent);
					SV_LinkLight(e, edict);
				}
				break;
			case BSP_ENTITY_LIGHT_DYNAMIC:
				{
					const edict_light_t* const edict = new_light_dynamic(ecs, ent->origin, (float)ent->light/300.0f, (float)ent->speed*0.1, (float)ent->range*0.1, ent->task);
					ent->ecs_id = edict->basic.id;
				}
				break;
			case BSP_ENTITY_TRIGGER_MULTIPLE:
				new_bsp_trigger_multiple(e, ent);
				break;
			case BSP_ENTITY_TRIGGER_ONCE:
				new_bsp_trigger_once(e, ent);
				break;
			case BSP_ENTITY_TRIGGER_TALK:
				new_bsp_trigger_talk(e, ent);
				break;
			case BSP_ENTITY_TRIGGER_CHANGELEVEL:
				new_bsp_trigger_levelchange(e, ent);
				break;
			case BSP_ENTITY_BUTTON:
				SV_LinkEdict(e, new_bsp_button(ecs, &e->assets.map, ent), false);
				break;
			case BSP_ENTITY_DOOR:
				SV_LinkEdict(e, new_bsp_door(ecs, &e->assets.map, ent), false);
				break;
			case BSP_ENTITY_SPAWNER:
				ent->ecs_id = new_spawner(ecs, &ent->origin, 1.0f/ent->speed, ent->task, ent->flags&SPAWNFLAGS_SPAWNER_AUTOSPAWNER);
				break;
			case BSP_ENTITY_START:
				found_player = true;
				player_start = ent->origin;
				player_angle = (float)-ent->angle*M_PI/180.0;
				break;
			case BSP_ENTITY_PARTICLE_EMITTER:
				new_particle_emitter(ecs, &ent->origin, &e->assets.models_basic[MODELS_STATIC_FIREBALL], 1, 1.0f/ent->speed);
				break;
			case BSP_ENTITY_PATH_CORNER:
				new_path_corner(ecs, ent);
				break;
			case BSP_ENTITY_WEAPON_BEAM:
				new_pickup(e, ent->origin, PICKUP_BEAM);
				break;
			case BSP_ENTITY_WEAPON_SHOTGUN:
				new_pickup(e, ent->origin, PICKUP_SHOTGUN);
				break;
			case BSP_ENTITY_WEAPON_SMG:
				new_pickup(e, ent->origin, PICKUP_SMG);
				break;
			default:
				break;
		}
	}

	/* Link Targets */
	for (size_t i=0; i<arrlenu(bsp->entities); i++) {
		bsp_entity_t* const src = &bsp->entities[i];
		if (src->target[0] == 0)
			continue;
		for (size_t j=0; j<myarrlenu(bsp->entities); j++) {
			bsp_entity_t* const target = &bsp->entities[j];
			if (target->targetname[0] == 0)
				continue;
			else if (strncmp(src->target, target->targetname, BSP_NAME_LEN) == 0) {
#ifndef NDEBUG
				myprintf("[SV_ClearWorld] Linking %s [%d] to %s [%d]\n", src->target, src->ecs_id, target->targetname, target->ecs_id);
#endif
				ecs->target[src->ecs_id] = target->ecs_id;

				/* Generate Direction for Particle Emitter */
				if (bitarr_get(ecs->bit_particle_emitter, src->ecs_id)) {
					ecs->custom0[src->ecs_id].particle_emitter.dir = glms_vec3_sub(ecs->pos[target->ecs_id], ecs->pos[src->ecs_id]);
					glm_vec3_normalize(ecs->custom0[src->ecs_id].particle_emitter.dir.raw);
				}
			}
		}
	}

	/* Init Models */
	if (found_player) {
		ecs->player_id = new_player(ecs, player_start, player_angle);
		ecs->pov_model_id = new_model_anim(e, &e->assets.models_anim[MODELS_ANIM_SHOTGUN], &e->assets.models_anim[MODELS_ANIM_SHOTGUN].anims[1], (vec3s){0});
		ecs->player.cur_weapon = -1; // invalidate cur_weapon to force player_switch_weapon
		player_switch_weapon(e, 0);
		cam_init(&e->cam, player_start, player_angle);
	}
}

void R_GetLights(engine_t* const e, const vec3 pos, light_query_t* const out) {
	/* Setup */
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
	const bsp_mleaf_t* const r_viewleaf = Mod_PointInLeaf(pos, worldmodel);

	/* Prepare */
	const uint32_t* const vis = (const uint32_t*)Mod_LeafPVS(r_viewleaf, worldmodel);
	const bsp_mleaf_t *leaf = &worldmodel->leafs[1];

	static int32_t* buffer = NULL;
	/* TODO alloc better */
	arrsetlen(buffer, e->ecs.qcvm.edict_light_cnt*2);
	out->pointlight_cnt = 0;
	out->shadowcaster_cnt = 0;
	out->pointlight_ids = buffer;
	out->shadowcaster_ids = buffer+e->ecs.qcvm.edict_light_cnt;

	/* Find Lights */
	for (int32_t i=0; i<worldmodel->numleafs; i++, leaf++) {
		if (vis[i / 32] & (1u << (i % 32))) {
			link_t *l, *next;
			for (l = leaf->lights_dynamic.next; l != &leaf->lights_dynamic; l = next) {
				next = l->next;
				edict_light_t* edict = (void*)l;
				assert((i32)out->pointlight_cnt < e->ecs.qcvm.edict_light_cnt);
				out->pointlight_ids[out->pointlight_cnt++] = edict->basic.id;
#ifdef VERBOSE
				myprintf("edict_pointlight: %d\n", edict->basic.id);
#endif
			}

			for (l = leaf->lights.next; l != &leaf->lights; l = next) {
				next = l->next;
				edict_light_t* edict = (void*)l;
				assert((i32)out->shadowcaster_cnt < e->ecs.qcvm.edict_light_cnt);
				out->shadowcaster_ids[out->shadowcaster_cnt++] = edict->basic.id;
#ifdef VERBOSE
				myprintf("edict_shadowcaster: %d\n", edict->basic.id);
#endif
			}

#ifdef VERBOSE2
			myprintf("[MarkLeaf] idx:%d nummarksurfaces:%d\n", i, leaf->nummarksurfaces);
#endif
		}
	}
}

void R_MarkSurfaces(engine_t* const e, const vec3 pos, visflags_t* visflags) {
	assert(visflags);

	/* Setup */
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
	const bsp_mleaf_t* const r_viewleaf = Mod_PointInLeaf(pos, worldmodel);
	assert(r_viewleaf);

	/* Prepare */
	const uint32_t* const vis = (const uint32_t*)Mod_LeafPVS(r_viewleaf, worldmodel);
	const bsp_mleaf_t *leaf = &worldmodel->leafs[1];
	const bsp_t* const bsp = &e->assets.map;
	model_basic_t* const model = &e->assets.map.qmods[0].model;
	const bool show_wireframe = e->controls.show_pvs;

	/* Reset Bitmap */
	for (u32 i=0; i<model->mesh_cnt; i++) {
		mesh_t* const mesh = &model->meshes[i];
		assert(visflags->mesh[i]);
		memset(visflags->mesh[i], 0, bitarr_get_size(mesh->face_cnt));
	}

	/* Mark Visible */
	const bsp_qmodel_t* const qmod = &bsp->qmods[0];
	for (int32_t i=0; i<worldmodel->numleafs; i++, leaf++) {
		if (vis[i / 32] & (1u << (i % 32))) {
#ifdef VERBOSE2
			myprintf("[MarkLeaf] idx:%d nummarksurfaces:%d\n", i, leaf->nummarksurfaces);
#endif
			for (int32_t j=0; j<leaf->nummarksurfaces; j++) {
				const int32_t sidx = leaf->firstmarksurface[j];
				const bsp_face_lookup_t lface = qmod->face_lookup[sidx];
				mesh_t* const mesh = &model->meshes[lface.mesh_id];
				BITARR_TYPE* const m_visflags = visflags->mesh[lface.mesh_id];
				const face_t* face = &mesh->faces[lface.idx];
				for (uint32_t k=0; k<lface.cnt; k++, face++) {
					bitarr_set(m_visflags, lface.idx+k);
					/* TODO optimize by removing this if */
					if (show_wireframe) {
						line_t line;
						line.p[0] = mesh->verts[face->p[0]].pos;
						line.p[1] = mesh->verts[face->p[1]].pos;
						line.color = 15;
						arrput(e->lines, line);
						line.p[1] = mesh->verts[face->p[2]].pos;
						arrput(e->lines, line);
						line.p[0] = mesh->verts[face->p[1]].pos;
						arrput(e->lines, line);
					}
				}
			}
		}
	}
}

#if 0
void R_GetVisLeafsForMob(engine_t* const e, const vec3 pos) {
	/* Setup */
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmodels[0];
	const bsp_mleaf_t* const r_viewleaf = Mod_PointInLeaf(pos, worldmodel);

	/* Prepare */
	const uint32_t* const vis = (const uint32_t*)Mod_LeafPVS(r_viewleaf, worldmodel);

	/* Mark */
	const bsp_mleaf_t *leaf = &worldmodel->leafs[1];
	const bsp_t* const bsp = &e->assets.map;
	const bool show_wireframe = e->controls.show_pvs;
	mesh_t* const mesh = &e->assets.map.model.meshes[0];
	assert(mesh->visflags);
	memset(mesh->visflags, 0, bitarr_get_size(mesh->face_cnt));
	for (int32_t i=0; i<worldmodel->numleafs; i++, leaf++) {
		if (vis[i / 32] & (1u << (i % 32))) {
			/* myprintf("MarkLeaf idx:%d nummarksurfaces:%d\n", i, leaf->nummarksurfaces); */
			for (int32_t j=0; j<leaf->nummarksurfaces; j++) {
				const int32_t sidx = leaf->firstmarksurface[j];
				const bsp_face_lookup_t lface = bsp->face_lookup[sidx];
				const face_t* face = &mesh->faces[lface.idx];
				for (uint32_t k=0; k<lface.cnt; k++, face++) {
					bitarr_set(mesh->visflags, lface.idx+k);
					if (show_wireframe) {
						line_t line;
						line.p[0] = mesh->verts[face->p[0]].pos;
						line.p[1] = mesh->verts[face->p[1]].pos;
						line.color = 15;
						arrput(e->lines, line);
						line.p[1] = mesh->verts[face->p[2]].pos;
						arrput(e->lines, line);
						line.p[0] = mesh->verts[face->p[1]].pos;
						arrput(e->lines, line);
					}
				}
			}
		}
	}
}
#endif

void TraceLine(engine_t *e, const hull_t* hull, const bbox_t* const move, trace_t* trace, const int32_t owner) {
	moveclip_t clip = {0};
	clip.move = *move;
	clip.trace.allsolid = true;
	clip.trace.fraction = 1;
	clip.trace.endpos = clip.move.max;
	clip.id = owner;
	clip.owner = owner;
	clip.hitcontents = CONTENTMASK_ANYSOLID;
	SV_RecursiveHullCheck(hull, &clip.move, &clip.trace, CONTENTMASK_ANYSOLID);

	/* create the bounding box of the entire move */
	/* clip.type = type & 3; */
	SV_MoveBounds(&clip.move, &clip.size, &clip.box);

	/* clip to entities */
	SV_ClipToLinks(e, e->ecs.qcvm.areanodes, &clip);
	*trace = clip.trace;
	/* myprintf("trace: ent:%d\n", trace->ent); */
}

static int RecursiveLightPoint(const bsp_t* const bsp, lightcache_t *cache, const bsp_mnode_t *node, const vec3s* const rayorg, const vec3s* const start, const vec3s* const end, float *maxdist) {
	float front, back, frac;
	vec3s mid;

loc0:
	if (node->contents < 0)
		return false; // didn't hit anything

	// calculate mid point
	/* if (node->plane->type < 3) { */
	/* 	front = start->raw[node->plane->type] - node->plane->dist; */
	/* 	back = end->raw[node->plane->type] - node->plane->dist; */
	/* } else { */
	front = glm_vec3_dot(start->raw, node->plane->normal.raw) - node->plane->dist;
	back = glm_vec3_dot(end->raw, node->plane->normal.raw) - node->plane->dist;
	/* } */

	// LordHavoc: optimized recursion
	if ((back < 0) == (front < 0))
//		return RecursiveLightPoint (cache, node->children[front < 0], rayorg, start, end, maxdist);
	{
		node = node->children[front < 0];
		goto loc0;
	}

	frac = front / (front-back);
	glm_vec3_sub((float*)end->raw, (float*)start->raw, mid.raw);
	glm_vec3_scale(mid.raw, frac, mid.raw);
	glm_vec3_add(mid.raw, (float*)start->raw, mid.raw);

	// go down front side
	if (RecursiveLightPoint(bsp, cache, node->children[front < 0], rayorg, start, &mid, maxdist)) {
		return true;
	} else {
		int ds, dt;
		// check for impact on this node

		const bsp_msurface_t* surf = bsp->msurfs + node->firstsurface;
		for (u32 i=0; i<node->numsurfaces; i++, surf++) {
			float sfront, sback, dist;
			vec3s raydelta;

			/* if (surf->flags & SURF_DRAWTILED) */
			/* 	continue; // no lightmaps */

			// ericw -- added double casts to force 64-bit precision.
			// Without them the zombie at the start of jam3_ericw.bsp was
			// incorrectly being lit up in SSE builds.
			ds = (int)((double)DoublePrecisionDotProduct(mid, surf->texinfo->vectorS)*BSP_RESIZE_DIV + surf->texinfo->distS);
			dt = (int)((double)DoublePrecisionDotProduct(mid, surf->texinfo->vectorT)*BSP_RESIZE_DIV + surf->texinfo->distT);

			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;

			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];

			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;

			/* TODO optimization */
			/* if (surf->plane->type < 3) */
			/* { */
			/* 	sfront = rayorg[surf->plane->type] - surf->plane->dist; */
			/* 	sback = end[surf->plane->type] - surf->plane->dist; */
			/* } */
			/* else */
			/* { */
			sfront = glm_vec3_dot(rayorg->raw, (float*)surf->plane->normal.raw) - surf->plane->dist;
			sback = glm_vec3_dot(end->raw, (float*)surf->plane->normal.raw) - surf->plane->dist;
			/* } */
			glm_vec3_sub((float*)end->raw, (float*)rayorg->raw, raydelta.raw);
			dist = sfront / (sfront - sback) * glm_vec3_norm(raydelta.raw);

			if (!surf->samples) {
				// We hit a surface that is flagged as lightmapped, but doesn't have actual lightmap info.
				// Instead of just returning black, we'll keep looking for nearby surfaces that do have valid samples.
				// This fixes occasional pitch-black models in otherwise well-lit areas in DOTM (e.g. mge1m1, mge4m1)
				// caused by overlapping surfaces with mixed lighting data.
				const float nearby = 8.f/BSP_RESIZE_DIV;
				dist += nearby;
				*maxdist = MIN(*maxdist, dist);
				continue;
			}

			if (dist < *maxdist) {
				cache->surfidx = surf - bsp->msurfs + 1;
				cache->ds = ds;
				cache->dt = dt;
			} else {
				cache->surfidx = -1;
			}

			return true;
		}

		// go down back side
		return RecursiveLightPoint(bsp, cache, node->children[front >= 0], rayorg, &mid, end, maxdist);
	}
}

static float InterpolateLightmap(const bsp_msurface_t *surf, int ds, int dt) {
	const u8* lightmap;
	int dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, r01 = 0, r10 = 0, r11 = 0;
	const int stride = ((surf->extents[0]>>4)+1);

	lightmap = surf->samples + ((dt>>4) * stride + (ds>>4));

	/* for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++) */
	/* { */
	/* 	scale = d_lightstylevalue[surf->styles[maps]]; */
		r00 = lightmap[      0];
		r01 = lightmap[      1];
		r10 = lightmap[stride+0];
		r11 = lightmap[stride+1];
		/* lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting */
	/* } */
	float color = ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)) * (1.f/256.f);
	return color;
}

float R_LightPoint(const bsp_t* const bsp, const vec3s p, const float ofs, lightcache_t *cache) {
	float maxdist = 8192.f/BSP_RESIZE_DIV; //johnfitz -- was 2048

	vec3s start, end;
	start = p;
	start.y += ofs;
	end = start;
	end.y -= maxdist;

	/* lightcolor[0] = lightcolor[1] = lightcolor[2] = 0; */

	if (cache->surfidx <= 0 // no cache or pitch black
		|| cache->surfidx > (int32_t)bsp->face_cnt
		|| fabsf (cache->pos.x - p.x) >= 1.0f/BSP_RESIZE_DIV
		|| fabsf (cache->pos.y - p.y) >= 1.0f/BSP_RESIZE_DIV
		|| fabsf (cache->pos.z - p.z) >= 1.0f/BSP_RESIZE_DIV)
	{
		cache->surfidx = 0;
		cache->pos = p;
		RecursiveLightPoint(bsp, cache, bsp->nodes, &start, &start, &end, &maxdist);
	}

	/* myprintf("surfidx:%d ds:%d dt:%d\n", cache->surfidx, cache->ds, cache->dt); */

	float lightcolor = 0;
	if (cache->surfidx > 0)
		lightcolor = InterpolateLightmap(bsp->msurfs + cache->surfidx - 1, cache->ds, cache->dt);

	return lightcolor;
}

static void PF_changeyaw(ecs_t* const ecs, edict_t* const ent, const float delta) {
	/* current = anglemod (ent->v.angles[1]); */
	versors quat = ecs->rot[ent->basic.id];
	versors qout, iquat;
	glm_quat_inv(quat.raw, iquat.raw);
	glm_quat_mul(iquat.raw, ent->rot_ideal.raw, qout.raw);
	const float speed = 2.0f*delta;
	const float dist = glm_quat_norm(qout.raw);
	const float t = MIN(speed / dist, 1.0f);
	glm_quat_lerp(quat.raw, ent->rot_ideal.raw, t, ecs->rot[ent->basic.id].raw);
	glm_quat_normalize(ecs->rot[ent->basic.id].raw);
}

static inline int SV_PointContents(engine_t* const e, vec3s p) {
	int cont;

	cont = SV_HullPointContents(&e->assets.map.qmods[0].hulls[0], 0, p);
	if (cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
		cont = CONTENTS_WATER;
	return cont;
}

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.
=============
*/
bool SV_CheckBottom(engine_t* const e, edict_t* const ent) {
	bbox_t move;
	ecs_t* const ecs = &e->ecs;
	vec3s mins, maxs;
	trace_t trace;
	int x, z;
	float mid, bottom;
	const i32 id = ent->basic.id;

	glm_vec3_add(ecs->pos[id].raw, ecs->bbox[id].min.raw, mins.raw);
	glm_vec3_add(ecs->pos[id].raw, ecs->bbox[id].max.raw, maxs.raw);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	move.min.y = mins.y - 1;
	for (x=0; x<=1; x++) {
		for (z=0; z<=1; z++) {
			move.min.x = x ? maxs.x : mins.x;
			move.min.z = z ? maxs.z : mins.z;
			if (SV_PointContents(e, move.min) != CONTENTS_SOLID)
				goto realcheck;
		}
	}

	return true; // we got out easy

realcheck:
	// check it for real...
	move.min.y = mins.y;

	// the midpoint must be within 16 of the bottom
	move.min.x = move.max.x = (mins.x + maxs.x)*0.5;
	move.min.z = move.max.z = (mins.z + maxs.z)*0.5;
	move.max.y = move.min.y - 2*STEPSIZE;
	trace = SV_Move(e, id, &move, true);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos.y;

	// the corners must be within 16 of the midpoint
	for (x=0; x<=1; x++) {
		for (z=0; z<=1; z++) {
			move.min.x = move.max.x = x ? maxs.x : mins.x;
			move.min.z = move.max.z = z ? maxs.z : mins.z;

			trace = SV_Move(e, id, &move, true);

			if (trace.fraction != 1.0 && trace.endpos.y > bottom)
				bottom = trace.endpos.y;
			if (trace.fraction == 1.0 || mid - trace.endpos.y > STEPSIZE)
				return false;
		}
	}

	return true;
}

/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
bool SV_movestep(engine_t* const e, edict_t* const ent, const float delta) {
	ecs_t* const ecs = &e->ecs;
	vec3s oldorg;
	trace_t trace;

	/* try the move */
	oldorg = ecs->pos[ent->basic.id];
	const vec3s vel = ecs->vel[ent->basic.id];
	bbox_t move;
	move.min.x = oldorg.x + vel.x * delta;
	move.min.y = oldorg.y + vel.y * delta;
	move.min.z = oldorg.z + vel.z * delta;

#if 0
	// flying monsters don't step up
	if ((int)ent->v.flags & (FL_SWIM | FL_FLY)) {
		// try one move with vertical motion, then one without
		for (i = 0; i < 2; i++)
		{
			VectorAdd (ent->v.origin, move, neworg);
			enemy = PROG_TO_EDICT (ent->v.enemy);
			if (i == 0 && enemy != qcvm->edicts) {
				dz = ent->v.origin[2] - PROG_TO_EDICT (ent->v.enemy)->v.origin[2];
				if (dz > 40)
					neworg[2] -= 8;
				if (dz < 30)
					neworg[2] += 8;
			}
			trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, neworg, false, ent);

			if (trace.fraction == 1) {
				if (((int)ent->v.flags & FL_SWIM) && SV_PointContents (trace.endpos) == CONTENTS_EMPTY)
					return false; // swim monster left water

				VectorCopy (trace.endpos, ent->v.origin);
				if (relink)
					SV_LinkEdict (ent, true);
				return true;
			}

			if (enemy == qcvm->edicts)
				break;
		}

		return false;
	}
#endif

	// push down from a step height above the wished position
	move.min.y += STEPSIZE;
	move.max = move.min;
	move.max.y -= STEPSIZE * 2;

	trace = SV_Move(e, ent->basic.id, &move, false);

	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		move.min.y -= STEPSIZE;
		trace = SV_Move(e, ent->basic.id, &move, false);
		if (trace.allsolid || trace.startsolid)
			return false;
	}
	if (trace.fraction == 1)
	{
		// if monster had the ground pulled out, go ahead and fall
		if ((int)ent->flags & FL_PARTIALGROUND)
		{
			ecs->pos[ent->basic.id].x += vel.x * delta;
			ecs->pos[ent->basic.id].y += vel.y * delta;
			ecs->pos[ent->basic.id].z += vel.z * delta;
			/* if (relink) */
				SV_LinkEdict(e, ent, true);
			ent->flags = ent->flags & ~FL_ONGROUND;
			//	Con_Printf ("fall down\n");
			return true;
		}

		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	ecs->pos[ent->basic.id] = trace.endpos;

	if (!SV_CheckBottom(e, ent)) {
		if (ent->flags & FL_PARTIALGROUND)
		{ // entity had floor mostly pulled out from underneath it
			// and is trying to correct
			/* if (relink) */
				SV_LinkEdict(e, ent, true);
			return true;
		}
		ecs->pos[ent->basic.id] = oldorg;
		return false;
	}

	if (ent->flags & FL_PARTIALGROUND) {
		//		Con_Printf ("back on ground\n");
		ent->flags = (int)ent->flags & ~FL_PARTIALGROUND;
	}
	/* ent->v.groundentity = EDICT_TO_PROG (trace.ent); */

	// the move is ok
	/* if (relink) */
		SV_LinkEdict(e, ent, true);
	return true;
}

bool SV_StepDirection(engine_t* const e, edict_t *ent, const vec3s dir, const float dist, const float delta) {
	vec3s up = {{0,1,0}};
	vec3s quat_dir = {{-dir.x, -dir.y, -dir.z}};
	glm_quat_for(quat_dir.raw, up.raw, ent->rot_ideal.raw);
	ecs_t* const ecs = &e->ecs;
	const float speed = 200.0f;
	glm_vec3_scale((float*)dir.raw, speed*delta, ecs->vel[ent->basic.id].raw);
	PF_changeyaw(ecs, ent, delta);

	const float dot = glm_quat_dot(ecs->rot[ent->basic.id].raw, ent->rot_ideal.raw);
	if (dot >= 0.9f) {
		SV_movestep(e, ent, delta);
		SV_LinkEdict(e, ent, true);
		return true;
	}
	SV_LinkEdict(e, ent, true);
	return false;
}

#define sv_accelerate 10.0f
void SV_Accelerate(ecs_t* const ecs, const i32 id, const float wishspeed, const vec3s wishdir, const float delta) {
	int	  i;
	float addspeed, accelspeed, currentspeed;

	vec3s vel = ecs->vel[id];
	currentspeed = glm_vec3_dot(vel.raw, wishdir.raw);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate * delta * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		vel.raw[i] += accelspeed * wishdir.raw[i];
	ecs->vel[id] = vel;
}

void SV_AirAccelerate(ecs_t* const ecs, const i32 id, float wishspeed, vec3s wishveloc, const float delta) {
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = glm_vec3_norm(wishveloc.raw);
	if (wishspd > 30)
		wishspd = 30;

	vec3s vel = ecs->vel[id];
	currentspeed = glm_vec3_dot(vel.raw, wishveloc.raw);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	//	accelspeed = sv_accelerate.value * host_frametime;
	accelspeed = sv_accelerate * wishspeed * delta;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		vel.raw[i] += accelspeed * wishveloc.raw[i];
	ecs->vel[id] = vel;
}

void SV_FixCheckBottom(edict_t *ent) {
	ent->flags |= FL_PARTIALGROUND;
}

void SV_NewChaseDir(engine_t* const e, edict_t* const actor, edict_t* const enemy, float dist) {
#if 0
#define DI_NODIR -1
	float deltax, deltay;
	float d[3];
	float tdir, olddir, turnaround;
	olddir = anglemod ((int)(actor->v.ideal_yaw / 45) * 45);
	turnaround = anglemod (olddir - 180);

	deltax = enemy->v.origin[0] - actor->v.origin[0];
	deltay = enemy->v.origin[1] - actor->v.origin[1];
	if (deltax > 10)
		d[1] = 0;
	else if (deltax < -10)
		d[1] = 180;
	else
		d[1] = DI_NODIR;
	if (deltay < -10)
		d[2] = 270;
	else if (deltay > 10)
		d[2] = 90;
	else
		d[2] = DI_NODIR;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if (tdir != turnaround && SV_StepDirection (actor, tdir, dist))
			return;
	}

	// try other directions
	if (((rand () & 3) & 1) || abs ((int)deltay) > abs ((int)deltax)) // ericw -- explicit int cast to suppress clang suggestion to use fabsf
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] != DI_NODIR && d[1] != turnaround && SV_StepDirection (actor, d[1], dist))
		return;

	if (d[2] != DI_NODIR && d[2] != turnaround && SV_StepDirection (actor, d[2], dist))
		return;

	/* there is no direct path to the player, so pick another direction */

	if (olddir != DI_NODIR && SV_StepDirection (actor, olddir, dist))
		return;

	if (rand () & 1) /*randomly determine direction of search*/
	{
		for (tdir = 0; tdir <= 315; tdir += 45)
			if (tdir != turnaround && SV_StepDirection (actor, tdir, dist))
				return;
	}
	else
	{
		for (tdir = 315; tdir >= 0; tdir -= 45)
			if (tdir != turnaround && SV_StepDirection (actor, tdir, dist))
				return;
	}

	if (turnaround != DI_NODIR && SV_StepDirection (actor, turnaround, dist))
		return;

	actor->v.ideal_yaw = olddir; // can't move

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all
#endif

	if (!SV_CheckBottom(e, actor))
		SV_FixCheckBottom(actor);
}

#if 1
void SV_MoveToGoal(engine_t* const e, edict_t *ent, const vec3s dir, const float dist, const float delta) {
#if 0
	edict_t *ent, *goal;
	float	 dist;

	ent = PROG_TO_EDICT (pr_global_struct->self);
	goal = PROG_TO_EDICT (ent->v.goalentity);
	dist = G_FLOAT (OFS_PARM0);

	if (!((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
	{
		G_FLOAT (OFS_RETURN) = 0;
		return;
	}

	// if the next step hits the enemy, return immediately
	if (PROG_TO_EDICT (ent->v.enemy) != qcvm->edicts && SV_CloseEnough (ent, goal, dist))
		return;

	// bump around...
#endif
	if (!SV_StepDirection(e, ent, dir, dist, delta)) {
		SV_NewChaseDir(e, ent, e->ecs.edict[e->ecs.player_id].edict, dist);
	}
}
#endif
