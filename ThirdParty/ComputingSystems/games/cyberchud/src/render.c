#include <assert.h>

#include "render.h"

#include "cull.h"
#include "worker.h"
#include "draw.h"
#include "mytime.h"
#include "palette.h"
#include "quake.h"
#include "text.h"
#include "vfx/vfx_neverhappen.h"
#include "vfx/vfx_wireframe.h"
#include "ui/menu_cubemap.h"
#include "ui/menu_controls.h"
#include "ui/menu_options.h"
#include "ui/menu_pause.h"
#include "ui/menu_common.h"
#include "ui/menu_debug.h"
#include "ui/statusline.h"
#include "utils/myds.h"
#include "stb_sprintf.h"

static inline void aabb_to_lines(engine_t* e, bbox_t aabb, uint32_t color) {
	// Define the eight vertices of the AABB
	const vec3s vertices[8] = {
		{{aabb.min.x, aabb.min.y, aabb.min.z}},
		{{aabb.max.x, aabb.min.y, aabb.min.z}},
		{{aabb.min.x, aabb.max.y, aabb.min.z}},
		{{aabb.max.x, aabb.max.y, aabb.min.z}},
		{{aabb.min.x, aabb.min.y, aabb.max.z}},
		{{aabb.max.x, aabb.min.y, aabb.max.z}},
		{{aabb.min.x, aabb.max.y, aabb.max.z}},
		{{aabb.max.x, aabb.max.y, aabb.max.z}}
	};

	// Define the indices of the AABB edges
	const int indices[12][2] = {
		{0, 1}, {1, 3}, {3, 2}, {2, 0}, // Bottom face
		{4, 5}, {5, 7}, {7, 6}, {6, 4}, // Top face
		{0, 4}, {1, 5}, {3, 7}, {2, 6}  // Side faces
	};

	const size_t line_cnt = myarrlenu(e->lines);
	arrsetlen(e->lines, line_cnt+12);
	line_t* lines = e->lines+line_cnt;
	for (int i = 0; i < 12; i++, lines++) {
		lines->p[0].x = vertices[indices[i][0]].x;
		lines->p[0].y = vertices[indices[i][0]].y;
		lines->p[0].z = vertices[indices[i][0]].z;
		lines->p[1].x = vertices[indices[i][1]].x;
		lines->p[1].y = vertices[indices[i][1]].y;
		lines->p[1].z = vertices[indices[i][1]].z;
		lines->color = color;
	}
}

static int clip_to_plane(line_t* line, const plane_t* plane) {
	const float dot_start = glms_vec3_dot(line->p[0], plane->norm) + plane->dist;
	const float dot_end = glms_vec3_dot(line->p[1], plane->norm) + plane->dist;

	if (dot_start >= 0 && dot_end >= 0) {
		return 0; // line is entirely in front of the plane
	} else if (dot_start < 0 && dot_end < 0) {
		return 1; // line is entirely behind the plane
	}

	const vec3s dir = glms_vec3_sub(line->p[1], line->p[0]);
	const float t = dot_start / (dot_start - dot_end);
	const vec3s dirt = glms_vec3_scale(dir, t);
	const vec3s clipped_point = glms_vec3_add(dirt, line->p[0]);

	if (dot_start < 0) {
		line->p[0] = clipped_point;
	} else {
		line->p[1] = clipped_point;
	}
	return 0;
}

static void render_lines(engine_t* e, line_t *line) {
	/* Render Debug Lines */
	const size_t line_cnt = myarrlenu(line);
	const mat4s mvp = e->cam.viewproj;
	const mat4s viewport = get_viewport(0, 0, SCREEN_W, FB_H);
	const plane_t* const frustum = (const plane_t*)&e->cam.frustum[0];
	for (size_t i=0; i<line_cnt; i++, line++) {
		line_t tline = *line;
		bool skip = false;
		for (int j=0; j<5; j++) {
			if (clip_to_plane(&tline, &frustum[j])) {
				skip = true;
				break;
			}
		}
		if (skip)
			continue;
		vec2s line2[2];
		for (int j=0; j<2; j++) {
			vec4s pos = glms_vec4(tline.p[j], 1);
			pos = glms_mat4_mulv(mvp, pos);
			pos = glms_mat4_mulv(viewport, pos);
			line2[j].x = pos.x / pos.w;
			line2[j].y = pos.y / pos.w;
		}
		/* print_vec3(line->p[0], "draw line->p[0] ", "\n"); */
		/* print_vec3(line->p[1], "draw line->p[1] ", "\n"); */
		/* print_vec2(line2[0], "draw line2 0: "); */
		/* print_vec2(line2[1], "draw line2 1: "); */
		draw_line(e->fb, line2[0].x, line2[0].y, line2[1].x, line2[1].y, tline.color);
	}
}

static inline void mat4_init_pos(vec3 pos, mat4s* dest) {
	*dest = (mat4s){{
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{pos[0], pos[1], pos[2], 1.0f}
	}};
}

void render(engine_threads_t *et, const float delta) {
#ifdef VERBOSE
	myprintf("[render]\n");
#endif

	engine_t* const e = &et->e;
	ecs_t* const ecs = &e->ecs;

	/* don't clear screen, hall of mirrors effect is FUCKIN SICK!!! */
	/* memset(e->fb, 1, SCREEN_W*FB_H); */

	/* Update Palette */
	if (e->controls.randomize_palette) {
		e->controls.randomize_palette = 0;
		palette_randomize(&e->palette_base, &e->seed);
		e->palette = e->palette_base;
		e->controls.update_palette = 1;
	}
	if (e->scene_transition > 0) {
		e->scene_transition -= delta;
		if (e->scene_transition < 0) e->scene_transition = 0;
		palette_scale(&e->palette, &e->palette_base, 1.0f-e->scene_transition);
		e->controls.update_palette = 1;
	}
	if (e->controls.update_palette) {
		e->controls.update_palette = 0;
#ifdef TEMPLEOS
		et->tosfb_thr.palette = e->palette; // TODO redundant
		et->tosfb_thr.update_palette = 1;
#endif
	}

	/* Clear Depth Buffer */
	for (size_t i=0; i<SCREEN_W*FB_H; i++) {
		e->db[i] = FLT_MAX;
	}

	TIME_TYPE time_draw_start = get_time();

	/* Pre-Model VFX */
	if (e->vfx_flags.glitchout) {
		e->vfx_flags.glitchout = vfx_glitchout(e->vfx_fb->data, e->glitchout_time, &e->assets.font_pinzelan2x, VFX_FB_W, VFX_FB_H);
		e->glitchout_time += delta;
	}
	if (e->vfx_flags.noise) {
		if (e->noise_time >= 0.2f)
			e->vfx_flags.noise = 0;
		e->noise_time += delta;
	}

	/* Update Strobe (before frags) */
	e->screen_pulse_time += delta;
	if (e->screen_pulse_time > 1.0f)
		e->vfx_flags.screen_pulse = 0;

	/* Generate Model Matrix */
	const i32 elen = myarrlen(ecs->flags);
	for (i32 i=0; i<elen; i++) {
		if (ecs->flags[i].model_mtx) {
			ecs->mtx[i] = glms_mat4_identity();
			glm_scale(ecs->mtx[i].raw, ecs->scale[i].raw);
			glm_quat_rotate(ecs->mtx[i].raw, ecs->rot[i].raw, ecs->mtx[i].raw);
			mat4s tmat;
			mat4_init_pos(ecs->pos[i].raw, &tmat);
			glm_mat4_mul(tmat.raw, ecs->mtx[i].raw, ecs->mtx[i].raw);
		}
	}

	/* Link Lights */
	/* TODO optimize */
	for (i32 i=0; i<elen; i++) {
		if (bitarr_get(ecs->bit_light_dynamic, i)) {
			edict_light_t* const edict = ecs->edict[i].light;
			SV_LinkLightDynamic(e, edict);
		}
	}

	/* Update Warpcircle */
	if (e->warpcircle.w >= 0) {
		e->warpcircle.w += delta*50;
		if (e->warpcircle.w >= 32)
			e->warpcircle.w = -1;
	}

	/* Generate Visleafs */
	arrsetlen(e->lines, 0);
	R_MarkSurfaces(e, e->cam.pos.raw, &e->visflags[VISFLAG_IDX_PLAYER]);

	/* Find Simple Lights */
	{
		/* Reset Simple Lights */
		e->simplelight_cnt = 0;
		i32 simplelight_highest_idx = -1;
		for (int i=0; i<MAX_SIMPLELIGHTS; i++)
			e->simplelight_dist[i] = FLT_MAX;

		/* Generate Simple Lights */
		const i32 elen = myarrlen(ecs->flags);
		vec3s player_pos = ecs->pos[ecs->player_id];
		const float max_merge_dist = 0.05f;
		for (i32 i=0; i<elen; i++) {
			if (bitarr_get(ecs->bit_simplelight0, i)) {
				if (bitarr_get(ecs->bit_simplelight0_arr, i)) {
					/* Check for Entities with multiple lights (beams) */
					beam_t* const beam = &ecs->custom0[i].beam;
					for (i32 arr_idx=0; arr_idx<beam->light_cnt; arr_idx++) {
						vec3s pos = beam->lights[arr_idx];

						/* find furthest idx */
						int furthest_idx = 0;
						float furthest_dist = 0;
						for (int j=0; j<MAX_SIMPLELIGHTS; j++) {
							const float test_dist = e->simplelight_dist[j];
							if (test_dist != FLT_MAX) {
								const float merge_dist = glm_vec3_distance(pos.raw, e->simplelights[j].raw);
								if (merge_dist < max_merge_dist) {
									if (e->simplelights[j].w < ecs->brightness[i])
										e->simplelights[j].w = ecs->brightness[i];
									goto next_light_arr;
								}
							}
							if (test_dist == FLT_MAX || furthest_dist < test_dist) {
								furthest_idx = j;
								furthest_dist = test_dist;
							}
						}

						/* check distance and set */
						const float dist = glm_vec3_distance(player_pos.raw, pos.raw);
						if (furthest_dist == 0 || dist < furthest_dist) {
							e->simplelight_dist[furthest_idx] = dist;
							e->simplelights[furthest_idx].x = pos.x;
							e->simplelights[furthest_idx].y = pos.y;
							e->simplelights[furthest_idx].z = pos.z;
							e->simplelights[furthest_idx].w = ecs->brightness[i];
							if (furthest_idx > simplelight_highest_idx) {
								simplelight_highest_idx = furthest_idx;
								e->simplelight_cnt = simplelight_highest_idx+1;
							}
						}
						next_light_arr:;
					}
				} else {
					/* find furthest idx */
					vec3s pos = ecs->pos[i];
					int furthest_idx = 0;
					float furthest_dist = 0;
					for (int j=0; j<MAX_SIMPLELIGHTS; j++) {
						const float test_dist = e->simplelight_dist[j];
							if (test_dist != FLT_MAX) {
								const float merge_dist = glm_vec3_distance(pos.raw, e->simplelights[j].raw);
								if (merge_dist < max_merge_dist) {
									if (e->simplelights[j].w < ecs->brightness[i])
										e->simplelights[j].w = ecs->brightness[i];
									goto next_light;
								}
							}
						if (test_dist == FLT_MAX || furthest_dist < test_dist) {
							furthest_idx = j;
							furthest_dist = test_dist;
						}
					}

					/* check distance and set */
					const float dist = glm_vec3_distance(player_pos.raw, pos.raw);
					if (furthest_dist == 0 || dist < furthest_dist) {
						e->simplelight_dist[furthest_idx] = dist;
						e->simplelights[furthest_idx].x = pos.x;
						e->simplelights[furthest_idx].y = pos.y;
						e->simplelights[furthest_idx].z = pos.z;
						e->simplelights[furthest_idx].w = ecs->brightness[i];
						if (furthest_idx > simplelight_highest_idx) {
							simplelight_highest_idx = furthest_idx;
							e->simplelight_cnt = simplelight_highest_idx+1;
						}
					}
				}
			}
			next_light:;
		}
	}

	/* Find Best Lights */
	i32 light_ids[MAX_DYNAMIC_LIGHTS] = {-1, -1};
	i32 shadow_ids[MAX_DYNAMIC_LIGHTS] = {-1, -1};
	float light_dist[MAX_DYNAMIC_LIGHTS] = {FLT_MAX, FLT_MAX};
	float shadow_dist[MAX_DYNAMIC_LIGHTS] = {FLT_MAX, FLT_MAX};
	vec3s cam_pos = e->cam.pos;
#if 0
	light_query_t light_query;
	R_GetLights(e, e->cam.pos.raw, &light_query);

	for (u32 i=0; i<light_query.pointlight_cnt; i++) {
		const size_t idx = light_query.pointlight_ids[i];
		int highest=0;
		float dist_test = -FLT_MAX;
		for (int j=0; j<MAX_DYNAMIC_LIGHTS; j++) {
			if (dist_test < light_dist[j]) {
				dist_test = light_dist[j];
				highest = j;
			}
		}

		const float dist = glm_vec3_distance(cam_pos.raw, ecs->pos[idx].raw);
		if (dist < light_dist[highest]) {
			light_dist[highest] = dist;
			light_ids[highest] = idx;
		}
	}

	for (u32 i=0; i<light_query.shadowcaster_cnt; i++) {
		const size_t idx = light_query.shadowcaster_ids[i];
		int highest=0;
		float dist_test = -FLT_MAX;
		for (int j=0; j<MAX_DYNAMIC_LIGHTS; j++) {
			if (dist_test < shadow_dist[j]) {
				dist_test = shadow_dist[j];
				highest = j;
			}
		}

		const float dist = glm_vec3_distance(cam_pos.raw, ecs->pos[idx].raw);
		if (dist < shadow_dist[highest]) {
			shadow_dist[highest] = dist;
			shadow_ids[highest] = idx;
		}
	}
#else
	{
		const i32 len = myarrlen(ecs->flags);
		for (i32 i=0; i<len; i++) {
			if (bitarr_get(ecs->bit_light_dynamic, i)) {
				int highest=0;
				float dist_test = -FLT_MAX;
				for (int j=0; j<MAX_DYNAMIC_LIGHTS; j++) {
					if (dist_test < light_dist[j]) {
						dist_test = light_dist[j];
						highest = j;
					}
				}

				const float dist = glm_vec3_distance(cam_pos.raw, ecs->pos[i].raw);
				if (dist < light_dist[highest]) {
					light_dist[highest] = dist;
					light_ids[highest] = i;
				}
			}
		}
		for (i32 i=0; i<len; i++) {
			if (bitarr_get(ecs->bit_light, i) && !ecs->custom0[i].light.no_shadowcaster) {
				int highest=0;
				float dist_test = -FLT_MAX;
				for (int j=0; j<MAX_DYNAMIC_LIGHTS; j++) {
					if (dist_test < shadow_dist[j]) {
						dist_test = shadow_dist[j];
						highest = j;
					}
				}

				const float dist = glm_vec3_distance(cam_pos.raw, ecs->pos[i].raw);
				if (dist < shadow_dist[highest]) {
					shadow_dist[highest] = dist;
					shadow_ids[highest] = i;
				}
			}
		}
	}
#endif

	/* Pointlights */
	e->pointlights.cnt = 0;
	if (e->flags.pointlight_shadows_enabled) {
		for (int i=0; i<e->max_pointlights; i++) {
			if (light_ids[i] < 0)
				break;
			const u32 ent_idx = light_ids[i];
			vec3s pos = ecs->pos[ent_idx];
#ifdef VERBOSE
			myprintf("pointlight: [%d] id:%d\n", i, ent_idx);
			print_vec3("pointlight", pos);
#endif
			cubemap_t* const cubemap = &e->pointlights.cubemaps[e->pointlights.cnt];
			cubemap_update(cubemap, pos, ecs->brightness[ent_idx], &e->visflags[VISFLAG_IDX_POINTLIGHTS+e->pointlights.cnt]);
			if (!cubemap->clean)
				R_MarkSurfaces(e, pos.raw, cubemap->visflags);
			e->pointlights.cnt++;
		}
	} else {
		for (int i=0; i<e->max_pointlights; i++) {
			if (light_ids[i] < 0)
				break;
			const u32 ent_idx = light_ids[i];
			vec3s pos = ecs->pos[ent_idx];
#ifdef VERBOSE
			myprintf("pointlight: [%d] id:%d\n", i, ent_idx);
			print_vec3("pointlight", pos);
#endif
			cubemap_t* const cubemap = &e->pointlights.cubemaps[e->pointlights.cnt];
			cubemap->pos = pos;
			cubemap->brightness = ecs->brightness[ent_idx];
			e->pointlights.cnt++;
		}
	}

	/* Shadowcasters */
	e->shadowcasters.cnt = 0;
	if (e->flags.shadowcaster_shadows_enabled) {
		for (int i=0; i<e->max_shadowcasters; i++) {
			if (shadow_ids[i] < 0)
				break;
			const u32 ent_idx = shadow_ids[i];
			vec3s pos = ecs->pos[ent_idx];
#ifdef VERBOSE
			myprintf("shadowcaster: [%d] id:%d\n", i, ent_idx);
			print_vec3("shadowcaster", pos);
#endif
			cubemap_occlusion_t* const cubemap = &e->shadowcasters.cubemaps[e->shadowcasters.cnt];
			cubemap_update(&cubemap->basic, pos, ecs->brightness[ent_idx], &e->visflags[VISFLAG_IDX_SHADOWCASTERS+e->shadowcasters.cnt]);
			cubemap_occlusion_clear(cubemap);
			if (!cubemap->basic.clean)
				R_MarkSurfaces(e, pos.raw, cubemap->basic.visflags);
			e->shadowcasters.cnt++;
		}
	} else {
		for (int i=0; i<e->max_shadowcasters; i++) {
			if (shadow_ids[i] < 0)
				break;
			const u32 ent_idx = shadow_ids[i];
			vec3s pos = ecs->pos[ent_idx];
#ifdef VERBOSE
			myprintf("shadowcaster: [%d] id:%d\n", i, ent_idx);
			print_vec3("shadowcaster", pos);
#endif
			cubemap_occlusion_t* const cubemap = &e->shadowcasters.cubemaps[e->shadowcasters.cnt];
			cubemap->basic.pos = pos;
			cubemap->basic.brightness = ecs->brightness[ent_idx];
			e->shadowcasters.cnt++;
		}
	}

	/* Write Global Visleafs */
	visflags_t* const vis_global = &e->visflags[VISFLAG_IDX_GLOBAL];
	const model_basic_t* const worldmodel = &e->assets.map.qmods[0].model;
	for (u32 i=0; i<worldmodel->mesh_cnt; i++) {
		BITARR_TYPE* const global_vis_mesh = vis_global->mesh[i];
		const mesh_t* mesh = &worldmodel->meshes[i];
		const size_t byte_cnt = bitarr_get_size(mesh->face_cnt);
		const size_t type_cnt = bitarr_get_cnt(mesh->face_cnt);
		memset(global_vis_mesh, 0, byte_cnt);

		{
			visflags_t* const vis_target = &e->visflags[VISFLAG_IDX_PLAYER];
			BITARR_TYPE* const vis_target_mesh = vis_target->mesh[i];
			for (size_t ti=0; ti<type_cnt; ti++) {
				global_vis_mesh[ti] |= vis_target_mesh[ti];
			}
		}
		/* Pointlights */
		if (e->flags.pointlight_shadows_enabled) {
			for (int vi=0; vi<e->pointlights.cnt; vi++) {
				if (e->pointlights.cubemaps[vi].clean)
					continue;
				visflags_t* const vis_target = &e->visflags[VISFLAG_IDX_POINTLIGHTS];
				BITARR_TYPE* const vis_target_mesh = vis_target->mesh[i];
				for (size_t ti=0; ti<type_cnt; ti++) {
					global_vis_mesh[ti] |= vis_target_mesh[ti];
				}
			}
		}
		/* Shadowcasters */
		if (e->flags.shadowcaster_shadows_enabled) {
			for (int vi=0; vi<e->shadowcasters.cnt; vi++) {
				if (e->shadowcasters.cubemaps[vi].basic.clean)
					continue;
				visflags_t* const vis_target = &e->visflags[VISFLAG_IDX_SHADOWCASTERS+vi];
				BITARR_TYPE* const vis_target_mesh = vis_target->mesh[i];
				for (size_t ti=0; ti<type_cnt; ti++) {
					global_vis_mesh[ti] |= vis_target_mesh[ti];
				}
			}
		}
	}

	/* Frustum Cull Models */
	job_frustum_cull(e);

	/* Filter Culled for Bones */
	const int32_t max_threads = e->cpu_cnt;
	arrsetlen(e->idxs_bones, 0);
	for (size_t i=0; i<myarrlenu(e->idxs_culled); i++) {
		const size_t idx = e->idxs_culled[i];
		if (ecs->flags[idx].bones)
			arrput(e->idxs_bones, idx);
	}

	/* Animation */
	{
		const u32 div_cnt = myarrlenu(e->idxs_bones)/max_threads;
		const u32 leftover = myarrlenu(e->idxs_bones)-div_cnt*max_threads;
		for (i32 i=0; i<max_threads; i++) {
			et->jobs[i].delta = delta;
			et->jobs[i].cnt = div_cnt;
			et->jobs[i].e = e;
			et->jobs[i].entities = e->idxs_bones+i*div_cnt;
		}
		et->jobs[max_threads-1].cnt += leftover;
		thread_set_and_go(et, JOB_STATE_BONES);
	}

	/* Verts */
	/* Lets try putting faces into buckets */
	{
		/* Reset Buffers */
		for (int i=0; i<max_threads; i++) {
			arrsetlen(et->jobs[i].vert_job_data, 0);
		}

		/* Distribute Faces Into Buckets */
		const size_t face_cnt = e->cull_tri_cnt;
		const u32 face_cnt_div = face_cnt / max_threads;
		const u32 face_cnt_leftover = face_cnt-face_cnt_div*max_threads;

		/* add the leftovers to the first thread */
		int ti=0; /* thread idx */
		size_t thread_faces_needed = face_cnt_div + face_cnt_leftover;
		for (size_t i=0; i<myarrlenu(e->idxs_culled); i++) {
			const size_t idx = e->idxs_culled[i];
			const model_basic_t* const model = &ecs->model[idx].model_anim->basic;
			const uint32_t mesh_cnt = model->mesh_cnt;
			job_vert_t vert_job;
			vert_job.id = idx;
			for (u32 j=0; j<mesh_cnt; j++) {
				vert_job.face_idx = 0;
				vert_job.mesh_idx = j;
				vert_job.mesh = &model->meshes[j];
				while (1) {
					const size_t mesh_faces_left = vert_job.mesh->face_cnt-vert_job.face_idx;
					if (thread_faces_needed < mesh_faces_left) {
						/* filled thread */
						vert_job.face_cnt = thread_faces_needed;
						arrput(et->jobs[ti].vert_job_data, vert_job);
						vert_job.face_idx += thread_faces_needed;
						thread_faces_needed = face_cnt_div;
						ti++;
					} else if (thread_faces_needed == mesh_faces_left) {
						/* consumed entire mesh */
						vert_job.face_cnt = thread_faces_needed;
						arrput(et->jobs[ti].vert_job_data, vert_job);
						thread_faces_needed = face_cnt_div;
						ti++;
						break;
					} else {
						/* consumed entire mesh */
						vert_job.face_cnt = mesh_faces_left;
						thread_faces_needed -= mesh_faces_left;
						arrput(et->jobs[ti].vert_job_data, vert_job);
						break;
					}
				}
			}
		}
	}
	thread_set_and_go(et, JOB_STATE_VERTS);

	if (!e->vfx_flags.wireframe && !e->vfx_flags.skip_frag) {
		/* Shadow Job */
		for (i32 i=0; i<max_threads; i++) {
			for (i32 ii=0; ii<max_threads; ii++)
				et->jobs[i].frag_in_data[ii] = et->jobs[ii].vert_out_data;
		}
		thread_set_and_go(et, JOB_STATE_SHADOW);

		/* Mark Cubemaps as Clean */
		for (int i=0; i<e->pointlights.cnt; i++)
			e->pointlights.cubemaps[i].clean = 1;
		for (int i=0; i<e->shadowcasters.cnt; i++)
			e->shadowcasters.cubemaps[i].basic.clean = 1;

		/* Frag Shader Job */
		thread_set_and_go(et, JOB_STATE_FRAG);
	} else {
		memset(e->fb, 0, SCREEN_W*FB_H);
	}

	const float time_thread_total = time_diff(time_draw_start, get_time())*1000;

	/* Draw Wireframes */
	if (e->controls.show_wireframe || e->vfx_flags.wireframe) {
		const float wireframe_tlen = 0.1f;
		e->wireframe_time += delta;
		if (e->wireframe_time > wireframe_tlen) {
			e->vfx_flags.wireframe = 0;
		}
		vfx_wireframe(et, delta);
	}

#if 0
	/* Debugging: View Depthbuffer */
	for (int i=0; i<SCREEN_W*FB_H; i++) {
		e->fb[i] = ((uint64_t)(e->db[i]*10)%11)+3;
	}
#endif

	/* Draw Talkbox */
	if (e->talkbox.enabled)
		talkbox_draw(&e->talkbox, e->fb, &e->assets, &e->audio, delta);

	if (e->controls.show_pvs) {
		const i32 ent_cnt = myarrlen(ecs->flags);
		for (i32 i=0; i<ent_cnt; i++) {
			if (bitarr_get(ecs->bit_edict, i)) {
				uint32_t color = 13;
				aabb_to_lines(e, ecs->edict[i].edict->absbox, color);
			}
		}
		render_lines(e, e->lines);
	}

	if (e->vfx_flags.neverhappen) {
		e->vfx_flags.neverhappen = vfx_neverhappen(e->fb, e->neverhappen_time, &e->assets.font_pinzelan2x);
		e->neverhappen_time += delta;
	}

	if (e->controls.show_debug) {
		float worst_frag_delta = 0;
		float avg_frag_delta = 0;
		float best_frag_delta = FLT_MAX;
		for (int i=0; i<max_threads; i++) {
			avg_frag_delta += et->jobs[i].frag_time;
			if (worst_frag_delta < et->jobs[i].frag_time) {
				worst_frag_delta = et->jobs[i].frag_time;
			}
			if (best_frag_delta > et->jobs[i].frag_time) {
				best_frag_delta = et->jobs[i].frag_time;
			}
		}
		avg_frag_delta /= max_threads;
		myprintf("avg:%.2f best:%.2f worst:%.2f best_diff:%.2f avg_diff:%.2f\n", avg_frag_delta, best_frag_delta, worst_frag_delta, worst_frag_delta-best_frag_delta, worst_frag_delta-avg_frag_delta);

		/* Draw UI */
#define LINE_MARGIN 2
#define LINE_H 12
#define BLOCK_H (LINE_H*max_threads+4)
#define COL_3_X 448
		const char* str = TextFormat("delta:%.3f", delta);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN);
		str = TextFormat("cam_pos:%.2f %.2f %.2f", e->cam.pos.x, e->cam.pos.y, e->cam.pos.z);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H);
		str = TextFormat("cam_rot:%.2f %.2f %.2f", e->cam.front.x, e->cam.front.y, e->cam.front.z);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*2);
		str = TextFormat("thr_t:%03.0f", time_thread_total);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*3);
		str = TextFormat("cull:%03.0f", e->time_cull);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*4);
		str = TextFormat("edict:%03.0f", e->time_edict);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*5);
		str = TextFormat("time_particle_emitter:%.3f", e->time_particle_emitter);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*6);
		str = TextFormat("time_particle:%.3f", e->time_particle);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*7);
		str = TextFormat("time_ai:%.3f", e->time_ai);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*8);
		str = TextFormat("time_ai_light:%.3f", e->time_ai_light);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*9);
		str = TextFormat("time_absbox:%.3f", e->time_absbox);
		DrawText(e->fb, &e->assets.font_matchup, str, COL_3_X, LINE_MARGIN+LINE_H*10);

		for (int i=0; i<max_threads; i++) {
			str = TextFormat("bone [%d]:%03.0f", i, et->jobs[i].anim_time);
			DrawText(e->fb, &e->assets.font_matchup, str, 2, LINE_MARGIN+BLOCK_H+LINE_H*i);
		}
		for (int i=0; i<max_threads; i++) {
			str = TextFormat("vert [%d] w:%03.0f s:%03.0f v:%03.0f", i, et->jobs[i].vert_world_time, et->jobs[i].vert_shadow_time, et->jobs[i].vert_view_time);
			DrawText(e->fb, &e->assets.font_matchup, str, 2, LINE_MARGIN+BLOCK_H*2+LINE_H*i);
		}
		for (int i=0; i<max_threads; i++) {
			str = TextFormat("frag [%d] s:%03.0f sp:%03.0f ss:%03.0f f:%03.0f", i, et->jobs[i].shadow_time, et->jobs[i].shadow_pointlight_time, et->jobs[i].shadow_shadowcaster_time, et->jobs[i].frag_time);
			DrawText(e->fb, &e->assets.font_matchup, str, 200, LINE_MARGIN+LINE_H*i);
		}
		for (int i=0; i<max_threads; i++) {
			str = TextFormat("tri [%d] w:%lu m:%lu", i, et->jobs[i].wtri_cnt, et->jobs[i].mtri_cnt);
			DrawText(e->fb, &e->assets.font_matchup, str, 200, LINE_MARGIN+BLOCK_H*1+LINE_H*i);
		}
	}

	/* Draw Graphs */
	push_ui_graph(&e->graphs[0], delta);
	if (e->flags.fps_graph)
		draw_ui_graph(e->fb, &e->graphs[0], &e->assets.font_matchup, 16, 16);

	/* Draw Pause Menu */
	switch (e->ui_mode) {
		case UI_MODE_NONE:
			break;
		case UI_MODE_PAUSE:
			draw_menu_pause(e);
			break;
		case UI_MODE_DEBUG:
			draw_menu_common(e);
			draw_menu_debug(e);
			break;
		case UI_MODE_OPTIONS:
		case UI_MODE_TITLE:
			draw_menu_common(e);
			break;
		case UI_MODE_CONTROLS:
			draw_menu_controls(e);
			break;
		case UI_MODE_CUBEMAPS:
			draw_menu_cubemap(e);
			break;
	}

	/* Draw Status Line */
	statusline_reset(e);
	draw_statusline(et, delta);

	/* Draw Cursor */
	if (e->ui_mode != UI_MODE_NONE)
		draw_px(e->fb_real, e->assets.px_pal[PX_PAL_CURSOR], e->controls.mouse_abs.x, e->controls.mouse_abs.y);
}

void playfield_draw(engine_threads_t *et, const float delta) {
	render(et, delta);

	/* draw HUD */
	engine_t* const e = &et->e;
	ecs_t* const ecs = &e->ecs;
	const i16 hp = ecs->hp[ecs->player_id].hp;
	stbsp_snprintf(e->scratch, 32, "HP: %d\n", hp);
	DrawText(e->fb, &e->assets.font_pinzelan2x, e->scratch, 2, FB_H - e->assets.font_pinzelan2x.font_height);

	if (hp <= 0) {
		DrawTextCentered(e->fb, &e->assets.font_pinzelan2x, "YOU'RE FUCKING DEAD", SCREEN_W/2, FB_H/2 - e->assets.font_pinzelan2x.font_height/2);
	}
}
