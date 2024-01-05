#ifndef CAMERA_H
#define CAMERA_H

#include "vec.h"

#define CAM_NEAR_Z 0.0001f

typedef struct {
	mat4s look_at;
	mat4s proj;
	mat4s viewproj;
	vec4s frustum[6];
	vec3s pos;
	vec3s front;
	vec3s up;
	vec3s right;

	float yaw;
	float pitch;

	float speed;
	float sensitivity;
	float zoom;
} cam_t;

static inline plane_t init_plane(const vec3s norm, const vec3s p1) {
	plane_t plane;
	plane.norm = glms_vec3_normalize(norm);
	plane.dist = glms_vec3_dot(plane.norm, p1);
	return plane;
}

static inline mat4s get_viewport(const int x, const int y, const int w, const int h) {
	return (mat4s){{
		{w/2., 0, 0, 0},
		{0, -h/2., 0, 0},
		{0,0,1,0},
		{x+w/2.0,-y+h/2.0,0,1}}};
}

void cam_init(cam_t *cam, vec3s pos, float yaw);
void cam_update(cam_t* cam);
void cam_lookat(cam_t* cam, vec3s origin, vec3s target);
void cam_process_movement(cam_t* cam, float xoffset, float yoffset);

#endif
