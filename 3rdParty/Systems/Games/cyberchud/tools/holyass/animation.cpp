#include "animation.h"

int extract_anim_basic(model_t *model, const aiScene* scene) {
	for (unsigned int i=0; i<scene->mNumAnimations; i++) {
		anim_t* internal_anim = &model->animations[i];
		const aiAnimation *anim = scene->mAnimations[i];
#ifndef NDEBUG
		printf("[ANIM] Animation:%u %s - mNumChannels:%u mNumMeshChannels:%u\n", i, anim->mName.data, anim->mNumChannels, anim->mNumMeshChannels);
#endif

		internal_anim->name = anim->mName.data;
		/* Find Length */
		float length = 0;
		for (unsigned int ci=0; ci<anim->mNumChannels; ci++) {
			const aiNodeAnim *ch = anim->mChannels[ci];
			printf("[CH] %s\n", ch->mNodeName.data);
		}
		internal_anim->length = length;
	}
	return EXIT_SUCCESS;
}

int extract_animations(model_t *model, const aiScene* scene) {
	for (unsigned int i=0; i<scene->mNumAnimations; i++) {
		anim_t* internal_anim = &model->animations[i];
		const aiAnimation *anim = scene->mAnimations[i];
#ifndef NDEBUG
		printf("[ANIM] Animation:%u %s - mNumChannels:%u mNumMeshChannels:%u\n", i, anim->mName.data, anim->mNumChannels, anim->mNumMeshChannels);
#endif

		internal_anim->name = anim->mName.data;
		/* Find Length */
		float length = 0;
		for (unsigned int bi=0; bi<model->bones.size(); bi++) {
			for (unsigned int ci=0; ci<anim->mNumChannels; ci++) {
				const aiNodeAnim *ch = anim->mChannels[ci];
				if (strcmp(ch->mNodeName.data, model->bones[bi].name) == 0) {
					if (ch->mNumPositionKeys > 0) {
#ifndef NDEBUG
						printf("pos:%f\n", ch->mPositionKeys[ch->mNumPositionKeys-1].mTime);
#endif
						if (ch->mPositionKeys[ch->mNumPositionKeys-1].mTime > length)
							length = ch->mPositionKeys[ch->mNumPositionKeys-1].mTime;
					}
					if (ch->mNumRotationKeys > 0) {
#ifndef NDEBUG
						printf("rot:%f\n", ch->mRotationKeys[ch->mNumRotationKeys-1].mTime);
#endif
						if (ch->mRotationKeys[ch->mNumRotationKeys-1].mTime > length)
							length = ch->mRotationKeys[ch->mNumRotationKeys-1].mTime;
					}
					if (ch->mNumScalingKeys > 0) {
#ifndef NDEBUG
						printf("scale:%f\n", ch->mScalingKeys[ch->mNumScalingKeys-1].mTime);
#endif
						if (ch->mScalingKeys[ch->mNumScalingKeys-1].mTime > length)
							length = ch->mScalingKeys[ch->mNumScalingKeys-1].mTime;
					}
				}
			}
		}
		internal_anim->length = length;

		for (unsigned int bi=0; bi<model->bones.size(); bi++) {
			for (unsigned int ci=0; ci<anim->mNumChannels; ci++) {
				const aiNodeAnim *ch = anim->mChannels[ci];
				if (strcmp(ch->mNodeName.data, model->bones[bi].name) == 0) {
#ifndef NDEBUG
					printf("[ANIM-CH] (%u) ChName:%s ChPosKeys:%u ChRotKeys:%u ChScaleKeys:%u\n", bi, ch->mNodeName.data, ch->mNumPositionKeys, ch->mNumRotationKeys, ch->mNumScalingKeys);
#endif
					anim_data_t* const int_ch = &internal_anim->channels[bi];
					int_ch->ch_pos_cnt = ch->mNumPositionKeys;
					int_ch->ch_rot_cnt = ch->mNumRotationKeys;
					int_ch->ch_scale_cnt = ch->mNumScalingKeys;
					int_ch->ch_pos = (anim_pos_t*)calloc(int_ch->ch_pos_cnt, sizeof(anim_pos_t));
					int_ch->ch_rot.time = (float*)calloc(int_ch->ch_rot_cnt, sizeof(float));
					int_ch->ch_rot.rot = (quat_t*)calloc(int_ch->ch_rot_cnt, sizeof(quat_t));
					int_ch->ch_scale = (anim_scale_t*)calloc(int_ch->ch_scale_cnt, sizeof(anim_rot_t));
					for (unsigned int pi=0; pi<int_ch->ch_pos_cnt; pi++) {
						const auto pos = ch->mPositionKeys[pi];
						int_ch->ch_pos[pi].time = pos.mTime;
						int_ch->ch_pos[pi].pos = pos.mValue;
#ifndef NDEBUG
						printf("posKey: %u t:%f pos:%fx%fx%f\n", pi, pos.mTime, pos.mValue.x, pos.mValue.y, pos.mValue.z);
#endif
					}
					for (unsigned int pi=0; pi<int_ch->ch_rot_cnt; pi++) {
						const auto rot = ch->mRotationKeys[pi];
						int_ch->ch_rot.time[pi] = rot.mTime;
						int_ch->ch_rot.rot[pi].x = rot.mValue.x;
						int_ch->ch_rot.rot[pi].y = rot.mValue.y;
						int_ch->ch_rot.rot[pi].z = rot.mValue.z;
						int_ch->ch_rot.rot[pi].w = rot.mValue.w;
#ifndef NDEBUG
						printf("rotKey: %u t:%f %fx%fx%fx%f\n", pi, rot.mTime, rot.mValue.x, rot.mValue.y, rot.mValue.z, rot.mValue.w);
#endif
					}
					for (unsigned int pi=0; pi<int_ch->ch_scale_cnt; pi++) {
						const auto scale = ch->mScalingKeys[pi];
						int_ch->ch_scale[pi].time = scale.mTime;
						int_ch->ch_scale[pi].scale = scale.mValue;
					}
				}
			}
		}
	}
	return EXIT_SUCCESS;
}
