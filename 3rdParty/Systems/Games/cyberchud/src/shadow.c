#include <assert.h>
#include "shadow.h"
#include "cglm/cam.h"
#include "shader.h"
#include "shader_utils.h"
#include "utils/minmax.h"

#define SHADOW_FAR_PLANE 25

void cubemap_update(cubemap_t* const shadow, const vec3s pos, const float brightness, visflags_t* const visflags) {
	shadow->brightness = brightness;

	/* Clear Depthbuffer */
	for (int i=0; i<6; i++) {
		for (size_t j=0; j<SHADOW_TOTAL; j++)
			shadow->depthbuffer[i][j] = FLT_MAX;
	}
	/* Set Visflags */
	shadow->visflags = visflags;

	/* Return if position hasn't changed */
	if (
			shadow->clean &&
			pos.x == shadow->pos.x &&
			pos.y == shadow->pos.y &&
			pos.z == shadow->pos.z
		) {
		return;
	}
	shadow->pos = pos;
	shadow->clean = 0;
	vec3s target, up;

	/* X Axis */
	up = (vec3s){{0,-1,0}};
	target = (vec3s){{pos.x+1, pos.y, pos.z}};
	glm_lookat((float*)pos.raw, target.raw, up.raw, shadow->cube_mtx[0].raw);
	target = (vec3s){{pos.x-1, pos.y, pos.z}};
	glm_lookat((float*)pos.raw, target.raw, up.raw, shadow->cube_mtx[1].raw);

	/* Y Axis */
	target = (vec3s){{pos.x, pos.y+1, pos.z}};
	up = (vec3s){{0,0,1}};
	glm_lookat((float*)pos.raw, target.raw, up.raw, shadow->cube_mtx[2].raw);
	target = (vec3s){{pos.x, pos.y-1, pos.z}};
	up = (vec3s){{0,0,-1}};
	glm_lookat((float*)pos.raw, target.raw, up.raw, shadow->cube_mtx[3].raw);

	/* Z Axis */
	up = (vec3s){{0,-1,0}};
	target = (vec3s){{pos.x, pos.y, pos.z+1}};
	glm_lookat((float*)pos.raw, target.raw, up.raw, shadow->cube_mtx[4].raw);
	target = (vec3s){{pos.x, pos.y, pos.z-1}};
	glm_lookat((float*)pos.raw, target.raw, up.raw, shadow->cube_mtx[5].raw);

	/* Clear Cache */
	for (int i=0; i<CUBE_FACE_CNT; i++) {
		for (size_t j=0; j<SHADOW_TOTAL; j++)
			shadow->cachebuffer[i][j] = FLT_MAX;
	}
}

void cubemap_init(cubemap_t* shadow) {
	shadow->near_plane = 1.0f;
	shadow->far_plane = SHADOW_FAR_PLANE;
	glm_perspective(90.0f*M_PI/180, (float)SHADOW_WIDTH/(float)SHADOW_HEIGHT, shadow->near_plane, shadow->far_plane, shadow->proj.raw);
}

void cubemap_clear(cubemap_t* cubemap) {
	for (int i=0; i<6; i++) {
		for (size_t j=0; j<SHADOW_TOTAL; j++)
			cubemap->depthbuffer[i][j] = FLT_MAX;
	}
}

void cubemap_occlusion_clear(cubemap_occlusion_t* cubemap) {
	const size_t sz = bitarr_get_size(SHADOW_WIDTH*SHADOW_HEIGHT);
	for (int i=0; i<6; i++)
		memset(cubemap->shadowfield[i], 0, sz);
}

void draw_shadowmap(const shadow_job_t* const job, cubemap_t* cubemaps, const i16 mask_y1, const i16 mask_y2) {
	frag_shadow_t* frag = job->tris.frags;
	rect_i16* mask = job->tris.masks;
	const vec3s light_pos = cubemaps->pos;
	for (int fi=0; fi<6; fi++) {
		float* const db = cubemaps->depthbuffer[fi];
		const uint32_t cnt = job->tri_cnt[fi];
		for (uint32_t j=0; j<cnt; j++, frag++, mask++) {
			/* TODO disabled mask for testing */
			assert(mask->x1 >= 0);
			assert(mask->x1 <= SHADOW_WIDTH);
			assert(mask->x2 >= 0);
			assert(mask->x2 <= SHADOW_WIDTH);
			assert(mask->y1 >= 0);
			assert(mask->y1 <= SHADOW_HEIGHT);
			assert(mask->y2 >= 0);
			assert(mask->y2 <= SHADOW_HEIGHT);

			if (mask_y1 > mask->y2 || mask_y2 <= mask->y1)
				continue;

			const int start_x = mask->x1;
			const int max_x = mask->x2;
			const int start_y = MAX(mask_y1, mask->y1);
			const int max_y = MIN(mask_y2, mask->y2);
			for (int y=start_y; y<max_y; y++) {
				for (int x=start_x; x<max_x; x++) {
					vec3s bc_screen;
					if (barycentric(frag->pts2, x, y, &bc_screen))
						continue;
					if (bc_screen.x<0.0 || bc_screen.y<0.0 || bc_screen.z<0.0)
						continue; // 0.0 is causing NaNs
					vec3s bc_clip = (vec3s){.x=bc_screen.x/frag->pts.vert[0].w, .y=bc_screen.y/frag->pts.vert[1].w, .z=bc_screen.z/frag->pts.vert[2].w};
					const float bc_clip_sum = bc_clip.x+bc_clip.y+bc_clip.z;
					/* https://github.com/ssloy/tinyrenderer/wiki/Technical-difficulties-linear-interpolation-with-perspective-deformations */
					bc_clip.x /= bc_clip_sum;
					bc_clip.y /= bc_clip_sum;
					bc_clip.z /= bc_clip_sum;

					vec3s worldpos;
					glm_mat3_mulv((vec3*)frag->world_pos.raw, bc_clip.raw, worldpos.raw);
					glm_vec3_sub(worldpos.raw, (float*)light_pos.raw, worldpos.raw);
					float lightDistance = glm_vec3_norm(worldpos.raw) / SHADOW_FAR_PLANE; // TODO

					const size_t pidx = y*SHADOW_WIDTH+x;
					if (lightDistance > db[pidx]) continue;
					db[pidx] = lightDistance;
				}
			}
		}
	}
}

void draw_shadowmap_occlusion(const shadow_job_t* const job, cubemap_occlusion_t* cubemaps, const i16 mask_y1, const i16 mask_y2) {
	rect_i16* mask = job->tris.masks;
	frag_shadow_t* frag = job->tris.frags;
	const vec3s light_pos = cubemaps->basic.pos;
	for (int fi=0; fi<6; fi++) {
		float* const db = cubemaps->basic.depthbuffer[fi];
		BITARR_TYPE* const shadowfield = cubemaps->shadowfield[fi];
		const uint32_t cnt = job->tri_cnt[fi];
		for (uint32_t j=0; j<cnt; j++, frag++, mask++) {
			/* TODO disabled mask for testing */
			assert(mask->x1 >= 0);
			assert(mask->x1 <= SHADOW_WIDTH);
			assert(mask->x2 >= 0);
			assert(mask->x2 <= SHADOW_WIDTH);
			assert(mask->y1 >= 0);
			assert(mask->y1 <= SHADOW_HEIGHT);
			assert(mask->y2 >= 0);
			assert(mask->y2 <= SHADOW_HEIGHT);

			if (mask_y1 > mask->y2 || mask_y2 <= mask->y1)
				continue;

			const int start_x = mask->x1;
			const int max_x = mask->x2;
			const int start_y = MAX(mask_y1, mask->y1);
			const int max_y = MIN(mask_y2, mask->y2);
			for (int y=start_y; y<max_y; y++) {
				for (int x=start_x; x<max_x; x++) {
					const size_t pidx = y*SHADOW_WIDTH+x;
					if (bitarr_get(shadowfield, pidx)) continue;
					vec3s bc_screen;
					if (barycentric(frag->pts2, x, y, &bc_screen))
						continue;
					if (bc_screen.x<0.0 || bc_screen.y<0.0 || bc_screen.z<0.0) continue; // 0.0 is causing NaNs
					vec3s bc_clip = (vec3s){.x=bc_screen.x/frag->pts.vert[0].w, .y=bc_screen.y/frag->pts.vert[1].w, .z=bc_screen.z/frag->pts.vert[2].w};
					const float bc_clip_sum = bc_clip.x+bc_clip.y+bc_clip.z;
					/* https://github.com/ssloy/tinyrenderer/wiki/Technical-difficulties-linear-interpolation-with-perspective-deformations */
					bc_clip.x /= bc_clip_sum;
					bc_clip.y /= bc_clip_sum;
					bc_clip.z /= bc_clip_sum;

					vec3s worldpos;
					glm_mat3_mulv((vec3*)frag->world_pos.raw, bc_clip.raw, worldpos.raw);
					worldpos.x -= light_pos.x;
					worldpos.y -= light_pos.y;
					worldpos.z -= light_pos.z;
					float lightDistance = glm_vec3_norm(worldpos.raw) / SHADOW_FAR_PLANE; // TODO

					if (lightDistance > db[pidx]) continue;
					/* TODO this a race condition */
					bitarr_set(shadowfield, pidx);
				}
			}
		}
	}
}
