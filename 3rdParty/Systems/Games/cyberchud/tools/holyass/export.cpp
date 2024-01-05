#include "export.h"

#include <stdio.h>

static void pad_write(FILE* fp) {
	const long pos = ftell(fp);
	const long align_mod = pos%16;
	if (align_mod != 0) {
		const int pad_size = 16-align_mod;
		const int32_t buf = 0;
		fwrite(&buf, 1, pad_size, fp);
#ifndef NDEBUG
		printf("[PADDING] fp:%ld adding padding:%d\n", pos, pad_size);
#endif
	}
}

int model_export(const model_t* model, const char* filename, const MODE mode) {
#ifndef NDEBUG
	printf("[EXPORT] (model) filename:%s prim_cnt:%u mat_cnt:%u\n",filename, model->prim_cnt, model->material_cnt);
#endif

	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		return EXIT_FAILURE;
	}

	/* Write Header */
	fwrite(&model->prim_cnt, sizeof(uint32_t), 1, fp);
	fwrite(&model->material_cnt, sizeof(uint32_t), 1, fp);
	uint32_t buf;
	buf = model->bones.size();
	fwrite(&buf, sizeof(uint32_t), 1, fp);
	fwrite(&model->bbox, sizeof(aabb_t), 1, fp);

	/* Write Materials */
	fwrite(model->materials, sizeof(material_t), model->material_cnt, fp);
#ifndef NDEBUG
	/* Debug Print Materials */
	for (size_t i=0; i<model->material_cnt; i++) {
		printf("[EXPORT] (materials) [%lu] diffuse:%s norm:%s\n", i, model->materials[i].diffuse, model->materials[i].normal);
	}
#endif

	/* Write Primitives */
	for (size_t i=0; i<model->prim_cnt; i++) {
		const prim_t *prim = &model->prims[i];
#ifndef NDEBUG
		printf("[EXPORT] (prims) [%lu] face_cnt:%u vert_cnt:%u mat_idx:%d diffuse:%s\n", i, prim->face_cnt, prim->vert_cnt, prim->material_idx, model->materials[prim->material_idx].diffuse);
#endif
		fwrite(&prim->face_cnt, sizeof(uint32_t), 1, fp);
		fwrite(&prim->vert_cnt, sizeof(uint32_t), 1, fp);
		fwrite(&prim->material_idx, sizeof(uint32_t), 1, fp);

		fwrite(prim->faces, sizeof(face_t), prim->face_cnt, fp);
#if 0
			for (unsigned int ii=0; ii<prim->vert_cnt; ii++) {
				printf("[model_export] pos:%.1fx%.1fx%.1f\n", prim->verts_extra[ii].pos.x, prim->verts_extra[ii].pos.y, prim->verts_extra[ii].pos.z);
			}
#endif
		/* fwrite(prim->verts, sizeof(vert_t), prim->vert_cnt, fp); */
		fwrite(prim->verts_extra, sizeof(vert_tangents_t), prim->vert_cnt, fp);
#if 0
			for (unsigned int ii=0; ii<prim->face_cnt; ii++) {
				printf("[model_export] face:%dx%dx%d\n", prim->faces[ii].p[0], prim->faces[ii].p[1], prim->faces[ii].p[2]);
			}
#endif
		fwrite(prim->uv, sizeof(vec2), prim->vert_cnt, fp);
		if (mode == MODE_SKELETON) {
			fwrite(prim->weights, sizeof(weights_t), prim->vert_cnt, fp);
		}
#if 0
			for (unsigned int ii=0; ii<prim->face_cnt*3; ii++) {
				for (unsigned int iii=0; iii<MAX_BONE_INFLUENCE; iii++) {
					printf("[model_export] weight:%d %f\n", prim->weights[ii].weights[iii].bone_idx, prim->weights[ii].weights[iii].weight);
				}
			}
#endif
	}

	if (mode == MODE_SKELETON) {
		const uint32_t bone_cnt = model->bones.size();
		if (bone_cnt > MAX_BONE) {
			fprintf(stderr, "[ERR] too many bones %u/%u\n", bone_cnt, MAX_BONE);
			return EXIT_FAILURE;
		}

		/* Export Bones */
		pad_write(fp);
		for (uint32_t i=0; i<bone_cnt; i++) {
			fwrite(&model->bones[i].data, sizeof(bone_data_t), 1, fp);
		}

		fwrite(&model->anim_cnt, sizeof(uint32_t), 1, fp);

		/* Export Animations */
		for (uint32_t i=0; i<model->anim_cnt; i++) {
#ifndef NDEBUG
			printf("[EXPORT] [ANIM] [%u] len:%.2f\n", i, model->animations[i].length);
#endif
			fwrite(&model->animations[i].length, sizeof(float), 1, fp);
			for (uint32_t bi=0; bi<bone_cnt; bi++) {
				const anim_data_t* const ch = &model->animations[i].channels[bi];
#ifndef NDEBUG
				printf("[EXPORT] [ANIM-CH] [%u] bone_idx:%u pos_cnt:%u rot_cnt:%u scale_cnt:%u\n", i, bi, ch->ch_pos_cnt, ch->ch_rot_cnt, ch->ch_scale_cnt);
#endif
				fwrite(&ch->ch_pos_cnt, sizeof(uint32_t), 3, fp);
				fwrite(ch->ch_pos, sizeof(anim_pos_t), ch->ch_pos_cnt, fp);
				fwrite(ch->ch_rot.time, sizeof(float), ch->ch_rot_cnt, fp);
				pad_write(fp);
				fwrite(ch->ch_rot.rot, sizeof(quat_t), ch->ch_rot_cnt, fp);
				fwrite(ch->ch_scale, sizeof(anim_scale_t), ch->ch_scale_cnt, fp);
			}
		}
	}

	fclose(fp);
	return EXIT_SUCCESS;
}
