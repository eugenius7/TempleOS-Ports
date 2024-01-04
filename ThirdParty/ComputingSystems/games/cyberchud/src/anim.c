#include <assert.h>

#include "anim.h"
#include "text.h"
#include "alloc.h"

static inline vec3s vec3_interpolate(const vec3s start, vec3s end, const float t) {
	glm_vec3_sub(end.raw, (float*)start.raw, end.raw);
	glm_vec3_scale(end.raw, t, end.raw);
	glm_vec3_add(end.raw, (float*)start.raw, end.raw);
	return end;
}

static inline versors quat_interpolate(const versors start, versors end, const float t) {
	const float dot = glm_quat_dot((float*)start.raw, end.raw);
	versors cache;
	glm_vec4_scale((float*)start.raw, 1.0f-t, cache.raw);
	if (dot < 0) {
		end.x = -end.x;
		end.y = -end.y;
		end.z = -end.z;
		end.w = -end.w;
	}
	versors cache2;
	glm_vec4_scale(end.raw, t, cache2.raw);
	return glms_quat_normalize(glms_quat_add(cache, cache2));
}

void anim_set(anim_t* const anim, const anim_data_t* const anim_data, const anim_data_t* const next) {
	anim->frame = 0;
	anim->time = 0;
	anim->length = anim_data->length;
	anim->data = anim_data;
	anim->next = next;
}

void bone_update(anim_t *anim, const uint32_t bone_cnt, const float delta) {
	/* Increment Animation */
	anim->time += delta;
	if (anim->time > anim->length) {
		if (anim->next) {
			anim_set(anim, anim->next, NULL);
		} else {
			anim->time = fmodf(anim->time, anim->length);
		}
	}

#ifdef VERBOSE2
	myprintf("bone_update: %f/%f\n", anim->time, anim->length);
#endif
	for (uint32_t bi=0; bi<bone_cnt; bi++) {
		const anim_ch_data_t* const data = &anim->data->channels[bi];

		/* Interpolate Position */
		mat4s pos_mat;
		switch (data->ch_pos_cnt) {
			case 0:
				pos_mat = glms_mat4_identity();
				break;
			case 1:
				pos_mat = glms_mat4_identity();
				glm_vec3_copy(data->ch_pos[0].pos.raw, pos_mat.col[3].raw);
				break;
			default:
			{
				uint32_t idx=0;
				for (; idx<data->ch_pos_cnt; idx++) {
					if (anim->time < data->ch_pos[idx].time) {
						break;
					}
				}
				assert(idx>0);
				const float mid_time = anim->time - data->ch_pos[idx-1].time;
				const float frame_diff = data->ch_pos[idx].time - data->ch_pos[idx-1].time;
				float progress = 0.0f;
				if (frame_diff > 0) {
					progress = mid_time / frame_diff;
				}
				const vec3s pos = vec3_interpolate(data->ch_pos[idx-1].pos, data->ch_pos[idx].pos, progress);
#ifdef VERBOSE2
				myprintf("pos [%u] time:%f progress:%f mid_time:%f frame_diff:%f\n", bi, anim->time, progress, mid_time, frame_diff);
				myprintf("[idx].time:%f [idx-1].time:%f\n", data->ch_pos[idx].time, data->ch_pos[idx-1].time);
				myprintf("posidx-1: %f %f %f\n", data->ch_pos[idx-1].pos.X, data->ch_pos[idx-1].pos.Y, data->ch_pos[idx-1].pos.Z);
				myprintf("posidx: %f %f %f\n", data->ch_pos[idx].pos.X, data->ch_pos[idx].pos.Y, data->ch_pos[idx].pos.Z);
				myprintf("pos: %f %f %f\n", pos.X, pos.Y, pos.Z);
#endif
				pos_mat = glms_mat4_identity();
				glm_vec3_copy((float*)pos.raw, pos_mat.col[3].raw);
			}
		}

		/* Interpolate Rotation */
		mat4s rot_mat;
		if (data->ch_rot_cnt == 0) {
#ifdef VERBOSE2
			myprintf("rot_cnt==0 %u\n", bi);
#endif
			rot_mat = glms_mat4_identity();
		} else if (data->ch_rot_cnt == 1) {
#ifdef VERBOSE2
			myprintf("rot_cnt==1 [%u] %f %f %f %f\n", bi, data->ch_rot[0].rot.X, data->ch_rot[0].rot.Y, data->ch_rot[0].rot.Z, data->ch_rot[0].rot.W);
#endif
			rot_mat = glms_quat_mat4(data->ch_rot.rot[0]);
		} else {
			uint32_t idx=0;
			for (; idx<data->ch_rot_cnt; idx++) {
				if (anim->time < data->ch_rot.time[idx]) {
					break;
				}
			}
			assert(idx>0);
			const float mid_time = anim->time - data->ch_rot.time[idx-1];
			const float frame_diff = data->ch_rot.time[idx] - data->ch_rot.time[idx-1];
			float progress = 0;
			if (frame_diff > 0.0f) {
				progress = mid_time / frame_diff;
			}
#ifdef VERBOSE2
			myprintf("rot [%u] idx:%u time:%f progress:%f mid_time:%f frame_diff:%f\n", bi, idx, anim->time, progress, mid_time, frame_diff);
			myprintf("rotidx-1: %f %f %f %f\n", data->ch_rot[idx-1].rot.X, data->ch_rot[idx-1].rot.Y, data->ch_rot[idx-1].rot.Z, data->ch_rot[idx].rot.W);
			myprintf("rotidx: %f %f %f %f\n", data->ch_rot[idx].rot.X, data->ch_rot[idx].rot.Y, data->ch_rot[idx].rot.Z, data->ch_rot[idx].rot.W);
#endif

			/* TODO investigate if there's a better function for this */
			rot_mat = glms_quat_mat4(quat_interpolate(data->ch_rot.rot[idx-1], data->ch_rot.rot[idx], progress));
			/* rot_mat = glms_quat_mat4(glms_quat_lerp(data->ch_rot.rot[idx-1], data->ch_rot.rot[idx], progress)); */

		}

		anim->bone_mtx[bi] = glms_mat4_mul(pos_mat, rot_mat);
	}
}
