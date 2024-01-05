#ifndef ANIMATION_H
#define ANIMATION_H

#include <assimp/scene.h>

#include "types.h"

int extract_animations(model_t *model, const aiScene* scene);
int extract_anim_basic(model_t *model, const aiScene* scene);

#endif
