#include "collision.h"
#include "ccd/ccd.h"
#include "ccd/quat.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* https://tavianator.com/2022/ray_box_boundary.html */
bool collide_ray_aabb(const ray_t* ray, const bbox_t* aabb) {
	vec3 tNear, tFar;
	vec3s invDir;
	invDir.x = 1.0 / ray->dir.x;
	invDir.y = 1.0 / ray->dir.y;
	invDir.z = 1.0 / ray->dir.z;

	glm_vec3_sub((float*)aabb->min.raw, (float*)ray->pos.raw, tNear);
	glm_vec3_mul(tNear, invDir.raw, tNear);
	glm_vec3_sub((float*)aabb->max.raw, (float*)ray->pos.raw, tFar);
	glm_vec3_mul(tFar, invDir.raw, tFar);

	float tmin = 0;
	float tmax = FLT_MAX;
	for (int i=0; i<3; i++) {
		tmin = MIN(MAX(tNear[i], tmin), MAX(tFar[i], tmin));
		tmax = MAX(MIN(tNear[i], tmax), MIN(tFar[i], tmax));
	}

	return tmin <= tmax;
}

void support_box(const void *_obj, const ccd_vec3_t *_dir, ccd_vec3_t *v) {
	// assume that obj_t is user-defined structure that holds info about
	// object (in this case box: x, y, z, pos, quat - dimensions of box,
	// position and rotation)
	const collider_box_t* const obj = (const collider_box_t*)_obj;
	ccd_vec3_t dir;
	ccd_quat_t qinv;

	ccdVec3Copy(&dir, _dir);
	const ccd_quat_t* const rot = (const ccd_quat_t*)&obj->rot;
	ccdQuatInvert2(&qinv, rot);
	ccdQuatRotVec(&dir, &qinv);

	// compute support point in specified direction
	ccd_vec3_t result;
	result.v[0] = ccdSign(ccdVec3X(&dir)) * obj->size.x * CCD_REAL(0.5);
	result.v[1] = ccdSign(ccdVec3Y(&dir)) * obj->size.y * CCD_REAL(0.5);
	result.v[2] = ccdSign(ccdVec3Z(&dir)) * obj->size.z * CCD_REAL(0.5);

	// transform support point according to position and rotation of object
	ccdQuatRotVec(&result, rot);
	ccdVec3Add(&result, (const ccd_vec3_t*)&obj->pos);
	*v = result;
}

void center_box(const void *_obj, ccd_vec3_t *center) {
	const collider_box_t* const obj = (const collider_box_t*)_obj;
	ccdVec3Copy(center, (const ccd_vec3_t*)&obj->pos);
}

void support_sphere(const void *_obj, const vec3s* _dir, ccd_vec3_t *v) {
	const vec4s* const obj = (const vec4s*)_obj;
	const vec3s result = glms_vec3_scale(*_dir, obj->w);
	v->v[0] = result.x;
	v->v[1] = result.y;
	v->v[2] = result.z;
}

#if 0
int ccd_boxbox(const collider_box_t* const mover, const collider_box_t* const collider, float* depth, vec3s* dir, vec3s* pos) {
	ccd_t ccd;
	CCD_INIT(&ccd);

	ccd.support1       = support_box;
	ccd.support2       = support_box;
	ccd.max_iterations = 100;
	ccd.epa_tolerance  = 0.0001;

	return ccdGJKPenetration(collider, mover, &ccd, depth, (ccd_vec3_t*)dir, (ccd_vec3_t*)pos);
}
#else
int ccd_boxbox(const collider_box_t* const mover, const collider_box_t* const collider, float* depth, vec3s* dir, vec3s* pos) {
	ccd_t ccd;
	CCD_INIT(&ccd);

	ccd.support1       = support_box;
	ccd.support2       = support_box;
	ccd.center1 = center_box;
	ccd.center2 = center_box;
	ccd.mpr_tolerance = 0.0001;

	return ccdMPRPenetration(collider, mover, &ccd, depth, (ccd_vec3_t*)dir, (ccd_vec3_t*)pos);
}
#endif
