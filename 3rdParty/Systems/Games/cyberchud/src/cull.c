#include "cull.h"
#include "quake.h"
#include "utils/minmax.h"
#include "utils/myds.h"

typedef struct {
	plane_t topFace;
	plane_t bottomFace;

	plane_t rightFace;
	plane_t leftFace;

	plane_t farFace;
	plane_t nearFace;
} frustum_t;

static inline int sphere_is_on_or_forward_plane(vec4s sphere, plane_t plane) {
	const float signed_dist = glm_vec3_dot(plane.norm.raw, sphere.raw) + plane.dist;
	return signed_dist > -sphere.w;
}

static inline vec3s M4Scale(const mat4 model_mtx) {
	vec3s result;
	result.x = glm_vec3_norm((float*)model_mtx[0]);
	result.y = glm_vec3_norm((float*)model_mtx[1]);
	result.z = glm_vec3_norm((float*)model_mtx[2]);
	return result;
}

inline static int isOnFrutstrum(const vec3s pos, const float radius, const frustum_t* frustum, mat4 model_mtx) {
	const vec3s global_scale = M4Scale(model_mtx);
	vec4s global_center;
	vec4s pos4 = glms_vec4(pos, 1.0f);
	glm_mat4_mulv((vec4*)model_mtx, pos4.raw, global_center.raw);
	const float max_scale = MAX(MAX(global_scale.x, global_scale.y), global_scale.z);
	vec4s sphere;
	glm_vec4(global_center.raw, radius * (max_scale * 0.5f), sphere.raw);

	return (
			sphere_is_on_or_forward_plane(sphere, frustum->leftFace) &&
			sphere_is_on_or_forward_plane(sphere, frustum->rightFace) &&
			sphere_is_on_or_forward_plane(sphere, frustum->farFace) &&
			sphere_is_on_or_forward_plane(sphere, frustum->nearFace) &&
			sphere_is_on_or_forward_plane(sphere, frustum->topFace) &&
			sphere_is_on_or_forward_plane(sphere, frustum->bottomFace)
			);
}

void job_frustum_cull(engine_t* const e) {
	const TIME_TYPE start = get_time();
	ecs_t* const ecs = &e->ecs;

	/* Reset Array */
	arrsetlen(e->idxs_culled, 0);
	e->cull_tri_cnt = 0;

	/* Iterate Models for Culling */
	const frustum_t* const frustum = (const frustum_t* const)&e->cam.frustum;
	const bsp_qmodel_t* const worldmodel = &e->assets.map.qmods[0];
	const vec3s pos = ecs->pos[ecs->player_id];
	u8* pvs = SV_FatPVS(pos, worldmodel);
	for (i32 i=0; i<myarrlen(ecs->flags); i++) {
		const flag_t flags = ecs->flags[i];
		if (!flags.model_mtx || flags.skip_draw)
			continue;

		/* BSP Visflag Cull Models */
		const edict_basic_t* const ent = &ecs->edict[i].edict->basic;
		if (!flags.worldmap && ent) {
			// ignore if not touching a PV leaf
			i32 leaf_i;
			for (leaf_i=0; leaf_i < ent->num_leafs; leaf_i++) {
				const i32 ent_leaf = ent->leafnums[leaf_i];
				if (pvs[ent_leaf >> 3] & (1 << (ent_leaf&7) ))
					break;
			}

			// ericw -- added ent->num_leafs < MAX_ENT_LEAFS condition.
			//
			// if ent->num_leafs == MAX_ENT_LEAFS, the ent is visible from too many leafs
			// for us to say whether it's in the PVS, so don't try to vis cull it.
			// this commonly happens with rotators, because they often have huge bboxes
			// spanning the entire map, or really tall lifts, etc.
			if (leaf_i == ent->num_leafs && ent->num_leafs < MAX_ENT_LEAFS)
				continue; // not visible
		}

		/* Frustum Cull Models */
		const vec3s sphere_pos = {0};
		if ((flags.skip_cull || isOnFrutstrum(sphere_pos, 4, frustum, ecs->mtx[i].raw))) {
			/* Add Entity to Output */
			arrput(e->idxs_culled, i);

			/* Count Triangles */
			const model_basic_t* const model = ecs->model[i].model_static;
			const mesh_t* mesh = model->meshes;
			for (u32 mi=0; mi<model->mesh_cnt; mi++, mesh++) {
				e->cull_tri_cnt += mesh->face_cnt;
			}
		}
	}
	e->time_cull = time_diff(start, get_time())*1000;
}

