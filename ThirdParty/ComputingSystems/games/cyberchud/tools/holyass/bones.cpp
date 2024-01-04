#include <assimp/scene.h>

#include "bones.h"

int find_bone_by_name(const model_t *model, const char* name) {
	for (size_t i=0; i<model->bones.size(); i++) {
		if (strcmp(model->bones[i].name, name) == 0) {
			return i;
		}
	}
	return -1;
}

static const aiBone* find_bone_by_name_ai(const aiScene* scene, const char* name) {
	for (unsigned int mi=0; mi<scene->mNumMeshes; mi++) {
		const aiMesh* mesh = scene->mMeshes[mi];
#ifndef NDEBUG
		printf("[find_bone_by_name_ai] mesh: %s\n", mesh->mName.data);
#endif
		for (unsigned int bi=0; bi<mesh->mNumBones; bi++) {
			const aiBone* sk_bone = mesh->mBones[bi];
#ifndef NDEBUG
			printf("[find_bone_by_name_ai] %s\n", sk_bone->mNode->mName.data);
#endif
			if (strcmp(sk_bone->mNode->mName.data, name) == 0) {
				return sk_bone;
			}
		}
	}
	return nullptr;
}

#if 0
static void calcInverseBindTransform(model_t* model, const size_t idx, const aiMatrix4x4& parentBindTransform) {
	bone_t* bone = &model->bones[idx];
	aiMatrix4x4 bindTransform = parentBindTransform * (mat0 * bone->transform_mtx);
	aiMatrix4x4 inverseBindTransform =  bindTransform.Inverse();

	for (unsigned int i=0; i<MAX_BONE_CHILDREN; i++) {
		if (bone->children[i] < 0)
			break;
		calcInverseBindTransform(model, bone->children[i], bindTransform);
	}
}

void calcInverseBind(model_t* model) {
	aiMatrix4x4 parent;
	/* parent.FromEulerAnglesXYZ(M_PI/2, 0, 0); */
	calcInverseBindTransform(model, 0, parent);
}
#endif

static std::vector<const char*> generate_bone_names(const aiScene* scene) {
	std::vector<const char*> bones;
	/* Extract Bones */
	for (unsigned int mi=0; mi<scene->mNumMeshes; mi++) {
		const aiMesh* mesh = scene->mMeshes[mi];
		for (unsigned int bi=0; bi<mesh->mNumBones; bi++) {
			const aiBone* ai_bone = mesh->mBones[bi];
			for (const auto bone : bones) {
				if (strcmp(ai_bone->mName.data, bone) == 0) {
					goto done0;
				}
			}
			bones.emplace_back(ai_bone->mName.data);
done0:;
		}
	}
	return bones;
}

static int add_bone_node(model_t* model, const aiScene* scene, const aiNode *node) {
	bone_t new_bone;
	new_bone.name = node->mName.data;
	const aiBone* ai_bone = find_bone_by_name_ai(scene, new_bone.name);
	if (!ai_bone) {
		fprintf(stderr, "[add_bone_node] can't find %s\n", node->mName.data);
		return -1;
	}
	for (int i=0; i<MAX_BONE_CHILDREN; i++) {
		new_bone.data.children[i] = -1;
	}
	new_bone.data.transform_mtx = ai_bone->mNode->mTransformation;
	new_bone.data.transform_mtx = new_bone.data.transform_mtx.Transpose();
	new_bone.data.inverse_mtx = ai_bone->mOffsetMatrix;
	new_bone.data.inverse_mtx = new_bone.data.inverse_mtx.Transpose();

#ifndef NDEBUG
	printf("[add_bone_node] %s\n", node->mName.data);
	printf("[add_bone_node] inverse_mtx post\n");
	for (unsigned int y=0; y<4; y++) {
		for (unsigned int x=0; x<4; x++) {
			printf(" %f", new_bone.data.inverse_mtx[y][x]);
		}
		printf("\n");
	}
	printf("\n[add_bone_node] ai_bone->mOffsetMatrix\n");
	for (unsigned int y=0; y<4; y++) {
		for (unsigned int x=0; x<4; x++) {
			printf(" %f", ai_bone->mOffsetMatrix[y][x]);
		}
		printf("\n");
	}
	printf("\n[add_bone_node] node->mTransformation\n");
	for (unsigned int y=0; y<4; y++) {
		for (unsigned int x=0; x<4; x++) {
			printf(" %f", node->mTransformation[y][x]);
		}
		printf("\n");
	}
	printf("\n[add_bone_node] ai_bone->mNode->mTransformation\n");
	for (unsigned int y=0; y<4; y++) {
		for (unsigned int x=0; x<4; x++) {
			printf(" %f", ai_bone->mNode->mTransformation[y][x]);
		}
		printf("\n");
	}

	const aiMatrix4x4 inv = ai_bone->mNode->mTransformation.Inverse();
	printf("\n[add_bone_node]inv\n");
	for (unsigned int y=0; y<4; y++) {
		for (unsigned int x=0; x<4; x++) {
			printf(" %f", inv[y][x]);
		}
		printf("\n");
	}
	printf("\n");
#endif

	model->bones.emplace_back(std::move(new_bone));

	return model->bones.size()-1;
}

int generate_bone_hierarchy(model_t* model, const aiScene* scene, const aiNode *node, const size_t bone_idx) {
#ifndef NDEBUG
	printf("[generate_bone_hierarchy] %s\n", node->mName.data);
#endif
	if (node->mNumChildren > MAX_BONE_CHILDREN) {
		fprintf(stderr, "[ERR] too many bone children! %s TOTAL:%d MAX:%d\n", node->mName.data, node->mNumChildren, MAX_BONE_CHILDREN);
		for (unsigned int i=0; i<node->mNumChildren; i++) {
			printf("[%u] %s\n", i, node->mChildren[i]->mName.data);
		}
		return EXIT_FAILURE;
	}
	for (unsigned int i=0, j=0; i<node->mNumChildren; i++) {
		const int child_idx = add_bone_node(model, scene, node->mChildren[i]);

		/* handle skipping stuff like IK bones */
		if (child_idx < 0)
			continue;

		model->bones[bone_idx].data.children[j] = child_idx;
		if (generate_bone_hierarchy(model, scene, node->mChildren[i], child_idx)) {
			return EXIT_FAILURE;
		}
#ifndef NDEBUG
		printf("[bone chil_idx] %d %d\n", child_idx, model->bones[bone_idx].data.children[i]);
#endif
		j++;
	}
	return EXIT_SUCCESS;
}

const aiNode* find_root_bone_node(model_t* model, const aiNode *node, const std::vector<const char*>& bone_names) {
#ifndef NDEBUG
	printf("[generate_bone_root] %s\n", node->mName.data);
#endif
	for (const auto name : bone_names) {
		if (strcmp(name, node->mName.data) == 0) {
			return node;
		}
	}

	for (unsigned int i=0; i<node->mNumChildren; i++) {
		const aiNode* result = find_root_bone_node(model, node->mChildren[i], bone_names);
		if (result)
			return result;
	}

	return nullptr;
}

int generate_bone_init(model_t* model, const aiScene* scene) {
	const std::vector<const char*> bone_names = generate_bone_names(scene);
	const aiNode *root_bone_node = find_root_bone_node(model, scene->mRootNode, bone_names);
	if (root_bone_node == nullptr) {
		fprintf(stderr, "[generate_bone_init] CAN'T FIND ROOT BONE!\n");
		return EXIT_FAILURE;
	}

	const int root_idx = add_bone_node(model, scene, root_bone_node);
	if (root_idx < 0)
		return EXIT_FAILURE;

	if (generate_bone_hierarchy(model, scene, root_bone_node, root_idx))
		return EXIT_FAILURE;

#ifndef NDEBUG
	int i = 0;
	for (const auto& bone: model->bones) {
		printf("[generate_bone_init] (%d) %s children:", i++, bone.name);
		for (int ci=0; ci<MAX_BONE_CHILDREN; ci++) {
			printf("%d ", bone.data.children[ci]);
		}
		printf("\n");
	}
#endif

	return EXIT_SUCCESS;
}
