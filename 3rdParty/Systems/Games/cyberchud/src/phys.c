#include "phys.h"
#include "quake.h"
#include "text.h"
#include "utils/myds.h"

static int update_flymissile(engine_t* const e, const int32_t id, const vec3s vel) {
	const trace_t trace = SV_PushEntity(e, id, vel);
#ifdef VERBOSE
	myprintf("TRACE frac:%.2f con:%d dist:%.2f ent:%d %.2fx%.2fx%.2f\n", trace.fraction, trace.contents, trace.plane.dist, trace.ent, trace.plane.normal.x, trace.plane.normal.y, trace.plane.normal.z);
#endif
	if (trace.ent) {
#ifdef VERBOSE
		myprintf("HIT frac:%.2f con:%d dist:%.2f ent:%d %.2fx%.2fx%.2f\n", trace.fraction, trace.contents, trace.plane.dist, trace.ent, trace.plane.normal.x, trace.plane.normal.y, trace.plane.normal.z);
#endif

	}
	if (trace.contents != -1) {
		return 1;
	}
	return 0;
}

edict_t* SV_TestEntityPosition(engine_t* const e, edict_t* const ent) {
	trace_t trace = SV_Move(e, ent->basic.id, &ent->absbox, 0);
	if (trace.startsolid)
		return e->ecs.edict[e->ecs.qcvm.map_id].edict;

	return NULL;
}

void SV_PushMove(engine_t* const e, edict_t* const pusher, const float movetime) {
	const size_t push_id = pusher->basic.id;
	ecs_t* const ecs = &e->ecs;

	vec3s vel = ecs->vel[push_id];
	if (!vel.x && !vel.y && !vel.z) {
		ecs->custom0[push_id].door.ltime += movetime;
		return;
	}

	vec3s mins, maxs, move;
	for (int i=0 ; i<3 ; i++) {
		move.raw[i] = vel.raw[i] * movetime;
		mins.raw[i] = pusher->absbox.min.raw[i] + move.raw[i];
		maxs.raw[i] = pusher->absbox.max.raw[i] + move.raw[i];
	}

	const vec3s pushorig = ecs->pos[push_id];

	/* move the pusher to it's final position */
	glm_vec3_add(ecs->pos[push_id].raw, move.raw, ecs->pos[push_id].raw);
	ecs->custom0[push_id].door.ltime += movetime;
	SV_LinkEdict(e, pusher, false);
	qcvm_t* const qcvm = &e->ecs.qcvm;

	qcvm_resize(qcvm);
	edict_t** moved_edict = qcvm->moved_edict_buf;
	vec3s *moved_from = qcvm->moved_from_buf;

	/* see if any solid entities are inside the final position */
	int num_moved = 0;
	/* check = NEXT_EDICT(qcvm->edicts); */
	for (i32 i=0; i<myarrlen(ecs->flags); i++) {
		const flag_t flags = ecs->flags[i];
		if (!flags.mob)
			continue;
		edict_t* const check = ecs->edict[i].edict;

		if (check->movetype == MOVETYPE_PUSH
		|| check->movetype == MOVETYPE_NONE
		|| check->movetype == MOVETYPE_NOCLIP)
			continue;

		/* if the entity is standing on the pusher, it will definately be moved */
		if (!(check->flags&FL_ONGROUND)) { // && PROG_TO_EDICT (check->v.groundentity) == pusher)
			if ( check->absbox.min.x >= maxs.x
			|| check->absbox.min.y >= maxs.y
			|| check->absbox.min.z >= maxs.z
			|| check->absbox.max.x <= mins.x
			|| check->absbox.max.y <= mins.y
			|| check->absbox.max.z <= mins.z )
				continue;

			/* see if the ent's bbox is inside the pusher's final position */
			if (!SV_TestEntityPosition(e, check))
				continue;
		}

		// remove the onground flag for non-players
		if (check->movetype != MOVETYPE_WALK) {
			ecs->edict[i].edict->flags &= ~FL_ONGROUND;
		}

		vec3s entorig = ecs->pos[check->basic.id];
		moved_from[num_moved] = ecs->pos[check->basic.id];
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity
		pusher->solid = SOLID_NOT;
		SV_PushEntity(e, check->basic.id, move);
		pusher->solid = SOLID_BSP;

#if 0
		/* TODO fix up for doors */
		// if it is still inside the pusher, block
		edict_t* const block = SV_TestEntityPosition(e, check);
		if (block)
		{	// fail the move
			if (check->absbox.min.x == check->absbox.max.x)
				continue;
			if (check->solid == SOLID_NOT || check->solid == SOLID_TRIGGER)
			{	// corpse
				check->absbox.min.x = check->absbox.min.y = 0;
				check->absbox.max = check->absbox.min;
				continue;
			}

			ecs->pos[check->basic.id] = entorig;
			SV_LinkEdict(e, check, true);

			ecs->pos[push_id] = pushorig;
			SV_LinkEdict(e, pusher, false);
			ecs->custom0[push_id].door.ltime -= movetime;

#if 0
			// if the pusher has a "blocked" function, call it
			// otherwise, just stay in place until the obstacle is gone
			if (pusher->v.blocked)
			{
				pr_global_struct->self = EDICT_TO_PROG(pusher);
				pr_global_struct->other = EDICT_TO_PROG(check);
				PR_ExecuteProgram (pusher->v.blocked);
			}
#endif

			// move back any entities we already moved
			for (int j=0; j<num_moved; j++) {
				ecs->pos[moved_edict[j]->basic.id] = moved_from[j];
				SV_LinkEdict(e, moved_edict[j], false);
			}
			return;
		}
#endif
	}
}

void update_edicts(engine_t* const e, const float delta) {
	ecs_t* const ecs = &e->ecs;
	#define DRAG_COEFFICIENT 0.1
	const i32 ent_cnt = myarrlen(ecs->flags);
	for (i32 i=0; i<ent_cnt; i++) {
		if (bitarr_get(ecs->bit_edict, i)) {
			vec3s vel = ecs->vel[i];
			const vec3s pos = ecs->pos[i];
#if 0
			myprintf("[DRAG] [%d]\n", i);
			print_vec3(vel, "[DRAG] vel: ", "\n");
			print_vec3(pos, "[DRAG] pos: ", "\n");
			print_vec3(ideal_pos, "[DRAG] ideal_pos: ", "\n");
#endif
#define GRAVITY 100
			const flag_t flags = ecs->flags[i];
			if (flags.noclip) {
				glm_vec3_scale(vel.raw, delta, vel.raw);
				const vec3s ideal_pos = glms_vec3_add(pos, vel);
				ecs->pos[i] = ideal_pos;
				ecs->vel[i] = (vec3s){.x=0,.y=0,.z=0};
			} else {
				edict_t* const edict = ecs->edict[i].edict;
				switch (edict->movetype) {
					case MOVETYPE_FLYMISSILE:
						glm_vec3_scale(vel.raw, delta, vel.raw);
						if (update_flymissile(e, i, vel))
							continue;
						break;
					case MOVETYPE_WALK:
						/* Apply Gravity */
						ecs->vel[i].y -= delta * GRAVITY;
						SV_WalkMove(e, i, delta);
						SV_LinkEdict(e, ecs->edict[i].edict, true);
						break;
					case MOVETYPE_STEP:
						SV_LinkEdict(e, edict, true);
						if (!(edict->flags&(FL_ONGROUND | FL_FLY | FL_SWIM))) {
							/* Apply Gravity */
							ecs->vel[i].y -= delta * GRAVITY;
							SV_FlyMove(e, i, delta, NULL);
							SV_LinkEdict(e, edict, true);
						}
						SV_LinkEdict(e, edict, true);
						ecs->ai[i].think(e, i, delta);
						break;
					case MOVETYPE_PUSH:
						SV_PushMove(e, ecs->edict[i].edict, delta);
						ecs->ai[i].think(e, i, delta);
						break;
					default:
						if (ecs->ai[i].think)
							ecs->ai[i].think(e, i, delta);
						break;
				}
			}
#if 0
			line_t line;
			line.p[0] = glms_mat3_mulv(world_to_bsp, pos);
			line.p[1] = glms_mat3_mulv(world_to_bsp, ideal_pos);
			trace_t trace = {0};
			bbox_t bbox = {(vec3s){.x=-.25f,.y=-.25f,.z=-.1f}, (vec3s){.x=.25f,.y=.25f,.z=.1f}};
			trace = SV_ClipMoveToEntity(&e->assets.map.hulls[1], line.p[0], &bbox, line.p[1], CONTENTMASK_ANYSOLID);
			print_vec3(line.p[0], "trace start ", "\n");
			print_vec3(line.p[1], "trace end ", "\n");
			print_vec3(trace.endpos, "trace endpos ", "\n");
			print_vec3(trace.plane.normal, "trace plane_norm ", "\n");
			myprintf("trace: plane_dist:%.2f allsolid:%d contents:%d startsolid:%d open:%d\n", trace.plane.dist, trace.allsolid, trace.contents, trace.startsolid, trace.inopen);
#endif

#if 0
			const float speed = HMM_LenV3(vel);
			const float drag = DRAG_COEFFICIENT * speed * speed * delta;
			const vec3s subvel = glms_vec3_scale(vel, drag);
#endif
		}
	}
}
