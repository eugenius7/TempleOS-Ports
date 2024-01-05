#ifndef COLLISION_H
#define COLLISION_H

#include "vec.h"

typedef struct {
	vec3s pos;
	vec3s size;
	versors rot;
} collider_box_t;

/* int intersect_moving_sphere_plane(const vec4 s, const vec3s v, const plane_t p, float* const t, vec3s* const q); */
int ccd_boxbox(const collider_box_t* const obj0, const collider_box_t* const obj1, float* depth, vec3s* dir, vec3s* pos);
bool collide_ray_aabb(const ray_t* ray, const bbox_t* aabb);

#endif
