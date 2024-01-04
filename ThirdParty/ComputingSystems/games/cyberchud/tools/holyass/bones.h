#ifndef BONES_H
#define BONES_H

#include <assimp/scene.h>

#include "types.h"

int find_bone_by_name(const model_t *model, const char* name);
int generate_bone_init(model_t* model, const aiScene* scene);
void calcInverseBind(model_t* model);

#endif
