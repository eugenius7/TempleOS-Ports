#include "camera.h"
#include "cglm/struct/cam.h"
#include "cglm/struct/frustum.h"

#define CAM_FAR_Z 256.0f
#define FOV 65.0

void cam_update(cam_t* cam) {
	// calculate the new Front vector
	const float cosf_pitch = cosf(cam->pitch);
	cam->front.x = cosf(cam->yaw) * cosf_pitch;
	cam->front.y = sinf(cam->pitch);
	cam->front.z = sinf(cam->yaw) * cosf_pitch;
	cam->front = glms_vec3_normalize(cam->front);
	// also re-calculate the Right and Up vector
	cam->right = glms_vec3_normalize(glms_vec3_cross(cam->front, (vec3s){.x=0,.y=1,.z=0}));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
	cam->up = glms_vec3_normalize(glms_vec3_cross(cam->right, cam->front));

	const vec3s center = glms_vec3_add(cam->pos, cam->front);
	cam->look_at = glms_lookat(cam->pos, center, cam->up);
	cam->viewproj = glms_mat4_mul(cam->proj, cam->look_at);

	glm_frustum_planes(cam->viewproj.raw, &cam->frustum[0].raw);
}

void cam_lookat(cam_t* cam, vec3s pos, vec3s target) {
	cam->pos = pos;
	cam->look_at = glms_lookat(cam->pos, target, (vec3s){{0,1,0}});
	cam->viewproj = glms_mat4_mul(cam->proj, cam->look_at);
	glm_frustum_planes(cam->viewproj.raw, &cam->frustum[0].raw);
}

void cam_process_movement(cam_t* cam, float xoffset, float yoffset) {
	cam->yaw += xoffset * cam->sensitivity;
	cam->pitch -= yoffset * cam->sensitivity;

	// make sure that when pitch is out of bounds, screen doesn't get flipped
	if (cam->pitch > 1.57)
		cam->pitch = 1.57;
	else if (cam->pitch < -1.57)
		cam->pitch = -1.57;

	// update Front, Right and Up Vectors using the updated Euler angles
	cam_update(cam);
}

void cam_init(cam_t *cam, vec3s pos, float yaw) {
	cam->pos = pos;
	cam->yaw = yaw;
	cam->pitch = 0;
	cam->speed = 1.0f;
	cam->sensitivity = 0.001f;
	cam->zoom = 1.0f;
	cam->proj = glms_perspective(FOV*M_PI/180, (float)SCREEN_W / (float)FB_H, CAM_NEAR_Z, CAM_FAR_Z);
	cam_update(cam);
}
