#include <stdlib.h>
#include <stdio.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "types.h"
#include "export.h"
#include "bones.h"
#include "animation.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* names of special materials */
#define MAT_TYPE_CNT 2
static const char* const mat_type_names[MAT_TYPE_CNT] = {
	"scum_mat",
	"scum_bg_mat",
};

cfg_t g_cfg = {0, MODE_BASIC, NULL, NULL};

inline static void print_error(const char* msg) {
	printf("ERROR: %s\n", msg);
}

static void read_hierarchy(aiNode *node) {
	printf("[read_hierarchy] name:%s\n", node->mName.data);
	for (uint32_t i=0; i<node->mNumChildren; i++) {
		read_hierarchy(node->mChildren[i]);
	}
}

static const char* get_filename_from_path(const char* str, int *len) {
	const size_t slen = strlen(str);
	/* 6 = "/a.png" */
	if (slen < 6) {
		fprintf(stderr, "[ERR] get_file_from_path (slen < 6): slen:%lu str:%s\n", slen, str);
		return NULL;
	}
	const char* rs = str+slen-5;
	for (size_t i=0; i<slen; i++, rs--) {
		if (*rs == '/') {
			*len = i;
			return ++rs;
		}
	}
	return NULL;
}

static int store_texture_filename(const aiMaterial* const mat, const aiTextureType type, char* const out) {
	const unsigned int cnt = mat->GetTextureCount(type);
	if (cnt != 1) {
		printf("[MAT] mat->GetTextureCount(%d) != 1: %u\n", type, cnt);
		return 1;
	}
	aiString mpath;
	mat->GetTexture(type, 0, &mpath);
	int slen=-1;
	const char* tex_name = get_filename_from_path(mpath.data, &slen);
	printf("[MAT] tex_name:%s type:%d cnt:%u slen:%d path:%s\n", tex_name, type, cnt, slen, mpath.data);
	if (slen >= TEX_NAME_LEN-1) {
		fprintf(stderr, "[ERR] get_filename_from_path (slen >= TEX_NAME_LEN-1): slen:%d\n", slen);
		return 1;
	}
	for (int i=0; i<slen; i++) {
		out[i] = toupper(tex_name[i]);
	}
	return 0;
}

static int process_materials(const aiScene* const scene, model_t *model) {
	/* Extract Materials */
	if (model->material_cnt > MAX_MATERIAL_CNT) {
		fprintf(stderr, "[ERR] too many materials: %u/%u\n", model->material_cnt, MAX_MATERIAL_CNT);
		return 1;
	}

	/* allocate the maximum possible material count, but actual material count may differ */
	model->materials = static_cast<material_t*>(calloc(scene->mNumMaterials, sizeof(material_t)));

	for (uint32_t i=0; i<scene->mNumMaterials; i++) {
		const aiMaterial* const mat = scene->mMaterials[i];
		material_t* const export_mat = &model->materials[i];
		const aiString material_name = mat->GetName();
#ifndef NDEBUG
		printf("[MAT] [%u] material_name:%s\n", i, material_name.data);
#endif

		/* for whatever reason, assimp always seems to think there's this extra unnamed material in the GLTF */
		if (material_name.length == 0) {
			printf("[MAT] [%u] skipping unnamed material\n", i);
			continue;
		}

		/* Set Material Type */
		export_mat->type = 0;
		if (strcmp(material_name.data, "beam") == 0) {
			export_mat->type = 2;
		} else {
			for (int j=0; j<MAT_TYPE_CNT; j++) {
				if (strcmp(material_name.data, mat_type_names[j]) == 0) {
					export_mat->type = 1;
				}
			}
		}
#ifndef NDEBUG
		printf("[MAT] [%u] export_mat->type:%d\n", i, export_mat->type);
#endif

		store_texture_filename(mat, aiTextureType_DIFFUSE, export_mat->diffuse);
		store_texture_filename(mat, aiTextureType_NORMALS, export_mat->normal);

#ifndef NDEBUG
		for (uint32_t j=0; j<mat->mNumProperties; j++) {
			const aiMaterialProperty* const prop = mat->mProperties[j];
			printf("[MAT] key:%s idx:%d type:%d datalen:%d\n", prop->mKey.data, prop->mIndex, prop->mType, prop->mDataLength);
		}
#endif
		model->material_cnt++;
	}
	return 0;
}

int loadasset(const char* path) {
	const C_STRUCT aiScene* scene = aiImportFile(path,
			aiProcess_ImproveCacheLocality|
			aiProcess_FindDegenerates|
			aiProcess_LimitBoneWeights|
			/* aiProcess_GenUVCoords| */
			aiProcess_CalcTangentSpace|
			aiProcess_FindInvalidData|
			aiProcess_GenBoundingBoxes|
			aiProcess_ValidateDataStructure|
			aiProcess_JoinIdenticalVertices|
			aiProcess_PopulateArmatureData);
	if (scene == NULL) {
		fprintf(stderr, "[loadasset] ERROR can't load scene, file:%s err:%s\n", path, aiGetErrorString());
		return 1;
	}

#ifndef NDEBUG
	printf("[SCENE] mNumAnimations:%u\n", scene->mNumAnimations);
	printf("[SCENE] mNumMaterials:%u\n", scene->mNumMaterials);
	printf("[SCENE] mNumMeshes:%u\n", scene->mNumMeshes);
#endif

#ifndef NDEBUG
	read_hierarchy(scene->mRootNode);
#endif

	if (scene->mNumMeshes > MAX_MESH_CNT) {
		fprintf(stderr, "[ERR] too many meshes: %u/%u\n", scene->mNumMeshes, MAX_MESH_CNT);
		return 1;
	}

	/* Allocate Model Memory */
	scene_t p_scene = {0, 0, nullptr};
	p_scene.model_cnt = 1; // TODO support multiple mesh
	p_scene.models = static_cast<model_t*>(calloc(p_scene.model_cnt, sizeof(model_t)));
	model_t *model = &p_scene.models[0]; // TMP
	model->prim_cnt = scene->mNumMeshes;
	model->anim_cnt = scene->mNumAnimations;
	model->prims = static_cast<prim_t*>(calloc(model->prim_cnt, sizeof(prim_t)));
	model->animations = static_cast<anim_t*>(calloc(model->anim_cnt, sizeof(anim_t)));

	if (process_materials(scene, model)) {
		fprintf(stderr, "[ERR] process_materials failed\n");
		return 1;
	}

	/* Generate Bone Hierarchy */
	if (g_cfg.mode == MODE_SKELETON) {
		if (generate_bone_init(model, scene)) {
			fprintf(stderr, "[ERR] Generate Bone Hierarchy failed\n");
			return EXIT_FAILURE;
		}
	}

	/* Extract AABB */
	model->bbox.min.x = INFINITY;
	model->bbox.min.y = INFINITY;
	model->bbox.min.z = INFINITY;
	model->bbox.max.x = -INFINITY;
	model->bbox.max.y = -INFINITY;
	model->bbox.max.z = -INFINITY;
	for (unsigned int mi=0; mi<scene->mNumMeshes; mi++) {
		const aiMesh* mesh = scene->mMeshes[mi];
		model->bbox.min.x = MIN(model->bbox.min.x, mesh->mAABB.mMin.x);
		model->bbox.min.y = MIN(model->bbox.min.y, mesh->mAABB.mMin.y);
		model->bbox.min.z = MIN(model->bbox.min.z, mesh->mAABB.mMin.z);
		model->bbox.max.x = MAX(model->bbox.max.x, mesh->mAABB.mMax.x);
		model->bbox.max.y = MAX(model->bbox.max.y, mesh->mAABB.mMax.y);
		model->bbox.max.z = MAX(model->bbox.max.z, mesh->mAABB.mMax.z);
	}

	/* Handle Meshes */
	prim_t* prim = model->prims;
	for (unsigned int mi=0; mi<scene->mNumMeshes; mi++, prim++) {
		const aiMesh* mesh = scene->mMeshes[mi];
#ifndef NDEBUG
		printf("[MESH] (%u) name:%s vert_cnt:%u face_cnt:%u bone_cnt:%u material:%s\n", mi, mesh->mName.data, mesh->mNumVertices, mesh->mNumFaces, mesh->mNumBones, scene->mMaterials[mesh->mMaterialIndex]->GetName().data);
#endif

		/* Extract Material */
		prim->material_idx = mesh->mMaterialIndex;

		if (!mesh->HasTangentsAndBitangents()) {
			fprintf(stderr, "[MESH] doesn't have tangents\n");
			return 1;
		}
#ifndef NDEBUG
		if (mesh->HasTangentsAndBitangents()) {
			printf("[MESH] has tangents\n");
		}
#endif

		/* Init Prim */
		prim->face_cnt = mesh->mNumFaces;
		prim->vert_cnt = mesh->mNumVertices;
		prim->faces = static_cast<face_t*>(calloc(prim->face_cnt, sizeof(face_t)));
		prim->verts = static_cast<vert_t*>(calloc(prim->vert_cnt, sizeof(vert_t)));
		prim->verts_extra = static_cast<vert_tangents_t*>(calloc(prim->vert_cnt, sizeof(vert_tangents_t)));
		prim->uv = static_cast<vec2*>(calloc(prim->vert_cnt, sizeof(vec2)));
		prim->weights = static_cast<weights_t*>(calloc(prim->vert_cnt, sizeof(weights_t)));
		for (size_t i=0; i<prim->vert_cnt; i++) {
			for (size_t ii=0; ii<MAX_BONE_INFLUENCE; ii++)
				prim->weights[i].weights[ii].bone_idx = -1;
		}

		/* Extract Faces */
		for (unsigned int fi=0; fi<mesh->mNumFaces; fi++) {
			const aiFace face = mesh->mFaces[fi];
			prim->faces[fi].p[0] = face.mIndices[0];
			prim->faces[fi].p[1] = face.mIndices[1];
			prim->faces[fi].p[2] = face.mIndices[2];
			if (face.mNumIndices != 3) {
				fprintf(stderr, "not 3 indices!: %u\n", face.mNumIndices);
				return 1;
			}
		}

		/* Extract Verts */
		for (uint32_t vi=0; vi<prim->vert_cnt; vi++) {
			prim->verts[vi].pos.x = mesh->mVertices[vi].x;
			prim->verts[vi].pos.y = mesh->mVertices[vi].y;
			prim->verts[vi].pos.z = mesh->mVertices[vi].z;
			prim->verts_extra[vi].pos.x = mesh->mVertices[vi].x;
			prim->verts_extra[vi].pos.y = mesh->mVertices[vi].y;
			prim->verts_extra[vi].pos.z = mesh->mVertices[vi].z;
			/* printf("Vert: %.2f %.2f %.2f\n", mesh->mVertices[vi].x, mesh->mVertices[vi].y, mesh->mVertices[vi].z); */
			/* printf("Verte: %.2f %.2f %.2f\n", prim->verts_extra[vi].pos.x, prim->verts_extra[vi].pos.y, prim->verts_extra[vi].pos.z); */
		}

		/* Extract Normals */
		for (uint32_t vi=0; vi<prim->vert_cnt; vi++) {
			prim->verts[vi].norm.x = mesh->mNormals[vi].x;
			prim->verts[vi].norm.y = mesh->mNormals[vi].y;
			prim->verts[vi].norm.z = mesh->mNormals[vi].z;
			prim->verts_extra[vi].norm.x = mesh->mNormals[vi].x;
			prim->verts_extra[vi].norm.y = mesh->mNormals[vi].y;
			prim->verts_extra[vi].norm.z = mesh->mNormals[vi].z;
		}

		/* Extract Tangents */
		if (mesh->HasTangentsAndBitangents()) {
			for (uint32_t vi=0; vi<prim->vert_cnt; vi++) {
				prim->verts_extra[vi].tangent.x = mesh->mTangents[vi].x;
				prim->verts_extra[vi].tangent.y = mesh->mTangents[vi].y;
				prim->verts_extra[vi].tangent.z = mesh->mTangents[vi].z;
				/* printf("Tangent: [%u] %.2f %.2f %.2f\n", vi, prim->verts_extra[vi].norm.x, prim->verts_extra[vi].norm.y, prim->verts_extra[vi].norm.z); */
			}
		}

		/* Extract UV */
		for (uint32_t vi=0; vi<prim->vert_cnt; vi++) {
			if (mesh->mNumUVComponents[0] == 2) {
				/* printf("[UV] [%u] %.2f %.2f\n", vi, mesh->mTextureCoords[0][vi].x, mesh->mTextureCoords[0][vi].y); */
				prim->uv[vi].x = mesh->mTextureCoords[0][vi].x;
				prim->uv[vi].y = mesh->mTextureCoords[0][vi].y;
			} else {
				/* printf("[mNumUVComponents] %u\n", mesh->mNumUVComponents[0]); */
				prim->uv[vi].x = 0;
				prim->uv[vi].y = 0;
			}
		}

#if 0
			/* Extract Verts */
			for (int i=0; i<3; i++) {
				printf("mIndices: %u\n", face.mIndices[i]);
				prim->verts[fi].p[i].x = mesh->mVertices[face.mIndices[i]].x;
				prim->verts[fi].p[i].y = mesh->mVertices[face.mIndices[i]].y;
				prim->verts[fi].p[i].z = mesh->mVertices[face.mIndices[i]].z;
			}

			/* Extract Normals */
			for (int i=0; i<3; i++) {
				prim->norms[fi].p[i].x = mesh->mNormals[face.mIndices[i]].x;
				prim->norms[fi].p[i].y = mesh->mNormals[face.mIndices[i]].y;
				prim->norms[fi].p[i].z = mesh->mNormals[face.mIndices[i]].z;
			}

			/* Extract UV */
			for (int i=0; i<3; i++) {
				if (mesh->mNumUVComponents[0] == 2) {
					prim->uv[fi].p[i].x = mesh->mTextureCoords[0][face.mIndices[i]].x;
					prim->uv[fi].p[i].y = mesh->mTextureCoords[0][face.mIndices[i]].y;
				} else {
					printf("[mNumUVComponents] %u\n", mesh->mNumUVComponents[0]);
					prim->uv[fi].p[i].x = 0;
					prim->uv[fi].p[i].y = 0;
				}
			}
#endif

		/* Skeleton Mode Only */
		if (g_cfg.mode == MODE_SKELETON) {
			/* Extract Bone Weights */
			for (unsigned int bi=0; bi<mesh->mNumBones; bi++) {
				const aiBone* bone = mesh->mBones[bi];
				/* bone_t* internal_bone = &model->bones[add_bone(model, bone->mName.data)]; */
				const int bone_idx = find_bone_by_name(model, bone->mName.data);
				if (bone_idx < 0) {
					fprintf(stderr, "[ERROR] can't find_bone_by_name: %s\n", bone->mName.data);
					return 1;
				}
#ifndef NDEBUG
				printf("[BONE] (%u) %s: num_weights:%u\n", bi, bone->mName.data, bone->mNumWeights);
#endif
				for (unsigned int wi=0; wi<bone->mNumWeights; wi++) {
					const size_t v_idx = bone->mWeights[wi].mVertexId;
					for (int vi=0; vi<MAX_BONE_INFLUENCE; vi++) {
						if (prim->weights[v_idx].weights[vi].bone_idx < 0) {
							prim->weights[v_idx].weights[vi].bone_idx = bone_idx;
							prim->weights[v_idx].weights[vi].weight = bone->mWeights[wi].mWeight;
							break;
						}
					}
#ifndef NDEBUG
					/* printf("[%u] weight_idx:%u weight:%f\n", wi, bone->mWeights[wi].mVertexId, bone->mWeights[wi].mWeight); */
#endif
				}
			}
		}
	}

	if (g_cfg.mode == MODE_SKELETON) {
		if (extract_animations(model, scene)) {
			fprintf(stderr, "[ERR] extract_animations\n");
			return EXIT_FAILURE;
		}
		/* calcInverseBind(model); */
	} else if (g_cfg.mode == MODE_BASIC_ANIM) {
		if (extract_anim_basic(model, scene)) {
			fprintf(stderr, "[ERR] extract_anim_basic\n");
			return EXIT_FAILURE;
		}
	}

	if (model_export(model, g_cfg.filename_output, g_cfg.mode)) {
		fprintf(stderr, "[ERR] can't write filename: %s\n", g_cfg.filename_output);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main(const int argc, const char **argv) {
	if (flags(&g_cfg, argc, argv)) {
		return EXIT_FAILURE;
	}

	// Check and validate the specified model file extension.
	const char* extension = strrchr(g_cfg.filename_input, '.');
	if (!extension) {
		print_error("Please provide a file with a valid extension.");
		return EXIT_FAILURE;
	}

	if (AI_FALSE == aiIsExtensionSupported(extension)) {
		print_error("The specified model file extension is currently "
			"unsupported in Assimp.");
		return EXIT_FAILURE;
	}

	// Load the model file.
	if(0 != loadasset(g_cfg.filename_input)) {
		print_error("[FAILED] ending program");
		aiDetachAllLogStreams();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
