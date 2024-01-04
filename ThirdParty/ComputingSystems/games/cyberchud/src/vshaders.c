#include <assert.h>

#include "vshaders.h"
#include "mytime.h"
#include "bitarr.h"
#include "text.h"
#include "quake.h"
#include "debug.h"
#include "stolenlib.h"
#include "utils/myds.h"
#include "utils/minmax.h"

static int check_backface(const mat3s* const tri, const vec3s* pos) {
	/* Backface Culling */
	const vec3s* const v0 = &tri->col[0];
	const vec3s* const v1 = &tri->col[1];
	const vec3s* const v2 = &tri->col[2];
	const vec3s v1v2 = {{ v1->x - v0->x, v1->y - v0->y, v1->z - v0->z }};
	const vec3s v1v3 = {{ v2->x - v0->x, v2->y - v0->y, v2->z - v0->z }};
	const vec3s normal = glms_vec3_cross(v1v2, v1v3);
	const vec3s view_to_tri = {{ v0->x - pos->x, v0->y - pos->y, v0->z - pos->z }};
	const float dot = glm_vec3_dot(view_to_tri.raw, normal.raw);
	if (dot > -FLT_EPSILON)
		return 1;
	return 0;
}

static void calculate_tbn_basic(const mat3 norm_mtx, const vec3 norm, const vec3 tangent, mat3 TBN) {
	glm_mat3_mulv((vec3*)norm_mtx, (float*)tangent, TBN[0]);
	glm_vec3_normalize(TBN[0]);
	glm_mat3_mulv((vec3*)norm_mtx, (float*)norm, TBN[2]);
	glm_vec3_normalize(TBN[2]);
	const float tdot = glm_vec3_dot(TBN[0], TBN[2]);
	TBN[0][0] -= TBN[2][0] * tdot;
	TBN[0][1] -= TBN[2][1] * tdot;
	TBN[0][2] -= TBN[2][2] * tdot;
	glm_vec3_normalize(TBN[0]);
	glm_vec3_cross(TBN[2], TBN[0], TBN[1]);
	glm_mat3_transpose(TBN);
}

typedef struct {
	mat3s norm_mtx;
	vec3s viewpos;
	vec3s pointlights[MAX_DYNAMIC_LIGHTS];
	vec3s shadowcasters[MAX_DYNAMIC_LIGHTS];
} uniform_tbn_t;

#if 0
static vec4s intersect_plane(const bbox_t* const planes, const vec4s* const lineStart, const vec4s* const lineEnd, float* t) {
	const float plane_d = -glm_vec3_dot(planes->max.raw, planes->min.raw);
	const float ad = glm_vec3_dot(lineStart->raw, planes->max.raw);
	const float bd = glm_vec3_dot(lineEnd->raw, planes->max.raw);
	*t = (-plane_d - ad) / (bd - ad);
	vec4s lineStartToEnd;
	glm_vec4_sub((float*)lineEnd->raw, (float*)lineStart->raw, lineStartToEnd.raw);
	glm_vec4_scale(lineStartToEnd.raw, *t, lineStartToEnd.raw);
	glm_vec4_add((float*)lineStart->raw, lineStartToEnd.raw, lineStartToEnd.raw);
	return lineStartToEnd;
}
#endif

#if 1
/* basic implementation */
static inline float intersect_plane_basic(const float near_plane, const vec4s* const lineStart, const vec4s* const lineEnd, vec4s* out) {
	glm_vec4_sub((float*)lineEnd->raw, (float*)lineStart->raw, out->raw);
	const float t = (near_plane - lineStart->z) / out->z;
	glm_vec4_scale(out->raw, t, out->raw);
	glm_vec4_add((float*)lineStart->raw, out->raw, out->raw);
	return t;
}
#else
/* WIP complex implementation */
inline static float intersect_plane_basic(const vec4s plane, const vec4s* const lineStart, const vec4s* const lineEnd, vec4s* out) {
	glm_vec4_sub((float*)lineEnd->raw, (float*)lineStart->raw, out->raw);
	const float dot = glm_vec3_dot(plane.raw, out->raw);
	if (fabs(dot) < 1e-6) {
		// No intersection or infinite intersection points (line lies on the plane)
		*out = *lineStart;
		return 0;
	}
	const float t = -(plane.w + (plane.x * lineStart->x + plane.y * lineStart->y + plane.z * lineStart->z)) / dot;
	/* const float t = (plane.w - (plane.x * lineStart->x + plane.y * lineStart->y + plane.z * lineStart->z)) / dot; */

	// Calculate the intersection point
	out->x = lineStart->x + t * out->x;
	out->y = lineStart->y + t * out->y;
	out->z = lineStart->z + t * out->z;
	/* const float t = (near_plane - lineStart->z) / out->z; */
	/* const float t = (plane.w - glm_vec3_dot(plane.raw, lineStart->raw)) / glm_vec3_dot(plane.raw, out->raw); */
	/* glm_vec4_scale(out->raw, t, out->raw); */
	/* glm_vec4_add((float*)lineStart->raw, out->raw, out->raw); */
	return t;
}
#endif

#if 0
static inline float dist(const vec4s p, const vec3s plane_p, const vec3s plane_n) {
	const float dot = glms_vec3_dot(plane_n, plane_p);
	return plane_n.x * p.x + plane_n.y * p.y + plane_n.z * p.z - dot;
}
#endif

static inline void interp_vec3(const vec3s* const inside, const vec3s* const outside, vec3s* const out, const float t) {
	glm_vec3_sub((float*)outside->raw, (float*)inside->raw, out->raw);
	glm_vec3_scale(out->raw, t, out->raw);
	glm_vec3_add(out->raw, (float*)inside->raw, out->raw);
}

static void interpolate_point(const clip_uniform_t* const uni, const u32 inside, const u32 outside, const vert_out_mesh_t* const in, const size_t idx, shader_bundle_t* out, const size_t oidx) {
	float t;
	t = intersect_plane_basic(uni->clip_plane.w, &in->view_pos[idx].vert[inside], &in->view_pos[idx].vert[outside], &out->view_pos[oidx].vert[outside]);
	assert(!isnan(t));
	shader_t* const outf = &out->frags[oidx];
	interp_vec3(&in->basic.world_pos[idx].col[inside], &in->basic.world_pos[idx].col[outside], &outf->world_pos.col[outside], t);

	outf->uv.uv[outside].u = t * (in->uv[idx].uv[outside].u - in->uv[idx].uv[inside].u) + in->uv[idx].uv[inside].u;
	outf->uv.uv[outside].v = t * (in->uv[idx].uv[outside].v - in->uv[idx].uv[inside].v) + in->uv[idx].uv[inside].v;
	outf->uv_light.uv[outside].u = t * (in->uv_light[idx].uv[outside].u - in->uv_light[idx].uv[inside].u) + in->uv_light[idx].uv[inside].u;
	outf->uv_light.uv[outside].v = t * (in->uv_light[idx].uv[outside].v - in->uv_light[idx].uv[inside].v) + in->uv_light[idx].uv[inside].v;

	interp_vec3(&in->norms[idx].col[inside], &in->norms[idx].col[outside], &outf->norm.col[outside], t);
	assert(uni->pointlight_cnt <= MAX_DYNAMIC_LIGHTS);
	for (int i=0; i<uni->pointlight_cnt; i++)
		interp_vec3(&in->tlightpos[idx].mtx[i].col[inside], &in->tlightpos[idx].mtx[i].col[outside], &outf->tlightpos.mtx[i].col[outside], t);
	for (int i=0; i<uni->shadowcaster_cnt; i++)
		interp_vec3(&in->tshadowpos[idx].mtx[i].col[inside], &in->tshadowpos[idx].mtx[i].col[outside], &outf->tshadowpos.mtx[i].col[outside], t);
	 interp_vec3(&in->tpos[idx].col[inside], &in->tpos[idx].col[outside], &outf->tpos.col[outside], t);
	 interp_vec3(&in->tviewpos[idx].col[inside], &in->tviewpos[idx].col[outside], &outf->tviewpos.col[outside], t);
}

#if 0
static void interpolate_point_shader(const clip_uniform_t* const uni, const u32 inside, const u32 outside, const size_t idx, shader_bundle_t* out, const size_t oidx) {
	float t;
	t = intersect_plane_basic(uni->clip_plane.w, &out->view_pos[idx].vert[inside], &out->view_pos[idx].vert[outside], &out->view_pos[oidx].vert[outside]);
	assert(!isnan(t));
	shader_t* const outf = &out->frags[oidx];
	const shader_t* const inf = &out->frags[idx];
	interp_vec3(&inf->world_pos.col[inside], &inf->world_pos.col[outside], &outf->world_pos.col[outside], t);

	outf->uv.uv[outside].u = t * (inf->uv.uv[outside].u - inf->uv.uv[inside].u) + inf->uv.uv[inside].u;
	outf->uv.uv[outside].v = t * (inf->uv.uv[outside].v - inf->uv.uv[inside].v) + inf->uv.uv[inside].v;
	outf->uv_light.uv[outside].u = t * (inf->uv_light.uv[outside].u - inf->uv_light.uv[inside].u) + inf->uv_light.uv[inside].u;
	outf->uv_light.uv[outside].v = t * (inf->uv_light.uv[outside].v - inf->uv_light.uv[inside].v) + inf->uv_light.uv[inside].v;

	interp_vec3(&inf->norm.col[inside], &inf->norm.col[outside], &outf->norm.col[outside], t);
	assert(uni->pointlight_cnt <= MAX_DYNAMIC_LIGHTS);
	for (int i=0; i<uni->pointlight_cnt; i++)
		interp_vec3(&inf->tlightpos.mtx[i].col[inside], &inf->tlightpos.mtx[i].col[outside], &outf->tlightpos.mtx[i].col[outside], t);
	for (int i=0; i<uni->shadowcaster_cnt; i++)
		interp_vec3(&inf->tshadowpos.mtx[i].col[inside], &inf->tshadowpos.mtx[i].col[outside], &outf->tshadowpos.mtx[i].col[outside], t);
	 interp_vec3(&inf->tpos.col[inside], &inf->tpos.col[outside], &outf->tpos.col[outside], t);
	 interp_vec3(&inf->tviewpos.col[inside], &inf->tviewpos.col[outside], &outf->tviewpos.col[outside], t);
}
#endif

typedef struct {
	mat3s world_pos;
	vec4_3 view_pos;
} trip_clip_basic_t;

static inline void copy_point_shadow(const int destp, const int srcp, const trip_clip_basic_t* const in, shadow_shader_bundle_t* const out, const size_t idx) {
	out->view_pos[idx].vert[destp] = in->view_pos.vert[srcp];
	out->frags[idx].world_pos.col[destp] = in->world_pos.col[srcp];
}

static inline void interpolate_point_shadow(const shadow_clip_uniform_t* const uni, const int inside, const int outside, const trip_clip_basic_t* const in, shadow_shader_bundle_t* const out, const size_t idx) {
	float t;
	t = intersect_plane_basic(uni->near_plane, &in->view_pos.vert[inside], &in->view_pos.vert[outside], &out->view_pos[idx].vert[outside]);
	glm_vec3_lerp((float*)in->world_pos.col[inside].raw, (float*)in->world_pos.col[outside].raw, t, out->frags[idx].world_pos.col[outside].raw);
}

static void copy_point(const clip_uniform_t* const uni, const int desti, const int srci, const vert_out_mesh_t* const in, const size_t idx, shader_bundle_t* out, const size_t oidx) {
	out->view_pos[oidx].vert[desti] = in->view_pos[idx].vert[srci];
	shader_t* const outf = &out->frags[oidx];
	outf->world_pos.col[desti] = in->basic.world_pos[idx].col[srci];
	outf->uv.uv[desti] = in->uv[idx].uv[srci];
	outf->uv_light.uv[desti] = in->uv_light[idx].uv[srci];
	outf->norm.col[desti] = in->norms[idx].col[srci];
	outf->tpos.col[desti] = in->tpos[idx].col[srci];
	outf->tviewpos.col[desti] = in->tviewpos[idx].col[srci];
	for (int i=0; i<uni->pointlight_cnt; i++)
		outf->tlightpos.mtx[i].col[desti] = in->tlightpos[idx].mtx[i].col[srci];
	for (int i=0; i<uni->shadowcaster_cnt; i++)
		outf->tshadowpos.mtx[i].col[desti] = in->tshadowpos[idx].mtx[i].col[srci];
}

static void copy_point_shader(const clip_uniform_t* const uni, const int desti, const int srci, shader_bundle_t* out, const size_t iidx, const size_t oidx) {
	out->view_pos[oidx].vert[desti] = out->view_pos[iidx].vert[srci];
	shader_t* const outf = &out->frags[oidx];
	shader_t* const inf = &out->frags[iidx];
	outf->world_pos.col[desti] = inf->world_pos.col[srci];
	outf->uv.uv[desti] = inf->uv.uv[srci];
	outf->uv_light.uv[desti] = inf->uv_light.uv[srci];
	outf->norm.col[desti] = inf->norm.col[srci];
	outf->tpos.col[desti] = inf->tpos.col[srci];
	outf->tviewpos.col[desti] = inf->tviewpos.col[srci];
	for (int i=0; i<uni->pointlight_cnt; i++)
		outf->tlightpos.mtx[i].col[desti] = inf->tlightpos.mtx[i].col[srci];
	for (int i=0; i<uni->shadowcaster_cnt; i++)
		outf->tshadowpos.mtx[i].col[desti] = inf->tshadowpos.mtx[i].col[srci];
}

static inline void copy_point_shader_shadow(const size_t desti, const size_t srci, shadow_shader_bundle_t* const out, const size_t iidx, const size_t oidx) {
	out->view_pos[oidx].vert[desti] = out->view_pos[iidx].vert[srci];
	out->frags[oidx].world_pos.col[desti] = out->frags[iidx].world_pos.col[srci];
}

static void tri_clip_apply_uniform(const clip_uniform_t* const uni, shader_bundle_t* out, const size_t idx) {
	shader_t* const outf = &out->frags[idx];
	for (int i=0; i<3; i++) {
		glm_mat4_mulv((vec4*)uni->cam->proj.raw, out->view_pos[idx].vert[i].raw, outf->vert_out.vert[i].raw);
	}
	outf->tex_diffuse = uni->tex_diffuse;
	outf->tex_normal = uni->tex_normal;
	outf->litdata = uni->litdata;
	outf->ambient = uni->ambient;
	outf->color_mod = uni->color_mod;
	outf->shader_idx = uni->shader_idx;
	for (int i=0; i<3; i++) {
		vec3_assert(outf->tpos.col[i].raw);
		vec3_assert(outf->tviewpos.col[i].raw);
		for (int j=0; j<uni->pointlight_cnt; j++)
			vec3_assert(outf->tlightpos.mtx[j].col[i].raw);
		for (int j=0; j<uni->shadowcaster_cnt; j++)
			vec3_assert(outf->tshadowpos.mtx[j].col[i].raw);
	}
}

static int tri_clip_tangent(const clip_uniform_t* const uni, const vert_out_mesh_t* const in, const size_t idx, shader_bundle_t* const out, const size_t oidx) {
	if (check_backface(&in->basic.world_pos[idx], &uni->cam->pos))
		return 0;

	u32 inside_points[3];  int nInsidePointCount = 0;
	u32 outside_points[3]; int nOutsidePointCount = 0;

	// Get signed distance of each point in triangle to plane
	for (u32 i=0; i<3; i++) {
		const float d = uni->clip_plane.w - in->view_pos[idx].vert[i].z;
		/* const float d = uni->clip_plane.w - glm_vec3_dot(uni->clip_plane.raw, in->view_pos[idx].vert[i].raw); */
		if (d >= 0) {
			inside_points[nInsidePointCount++] = i;
		} else {
			outside_points[nOutsidePointCount++] = i;
		}
	}

	shader_t* const outf0 = &out->frags[oidx];

	if (nInsidePointCount == 0) {
		return 0;
	} else if (nInsidePointCount == 3) {
		out->view_pos[oidx] = in->view_pos[idx];
		outf0->world_pos = in->basic.world_pos[idx];
		outf0->uv = in->uv[idx];
		outf0->uv_light = in->uv_light[idx];
		outf0->norm = in->norms[idx];
		outf0->tpos = in->tpos[idx];
		outf0->tviewpos = in->tviewpos[idx];
		outf0->tlightpos = in->tlightpos[idx];
		outf0->tshadowpos = in->tshadowpos[idx];
		tri_clip_apply_uniform(uni, out, oidx);
		return 1;
	} else if (nInsidePointCount == 1 && nOutsidePointCount == 2) {
		copy_point(uni, inside_points[0], inside_points[0], in, idx, out, oidx);
		interpolate_point(uni, inside_points[0], outside_points[0], in, idx, out, oidx);
		interpolate_point(uni, inside_points[0], outside_points[1], in, idx, out, oidx);
		tri_clip_apply_uniform(uni, out, oidx);
		return 1;
	}
	/* } else if (nInsidePointCount == 2 && nOutsidePointCount == 1) { */
	/* Triangle 1 */
	copy_point(uni, inside_points[0], inside_points[0], in, idx, out, oidx);
	copy_point(uni, inside_points[1], inside_points[1], in, idx, out, oidx);
	interpolate_point(uni, inside_points[0], outside_points[0], in, idx, out, oidx);

	/* Triangle 2 */
	size_t tidx0 = inside_points[0];
	size_t tidx1 = inside_points[1];
	if (tidx1 > tidx0) {
		const size_t tmp = tidx1;
		tidx1 = tidx0;
		tidx0 = tmp;
	}
	copy_point(uni, tidx0, inside_points[1], in, idx, out, oidx+1);
	copy_point_shader(uni, tidx1, outside_points[0], out, oidx, oidx+1);
	interpolate_point(uni, inside_points[1], outside_points[0], in, idx, out, oidx+1);
	tri_clip_apply_uniform(uni, out, oidx);
	tri_clip_apply_uniform(uni, out, oidx+1);
	return 2;
}

#if 0
/* WIP complex implementation */
static int tri_clip_tangent2(const clip_uniform_t* const uni, const shader_bundle_t* const in, const size_t idx, shader_bundle_t* const out, const size_t oidx) {
	u32 inside_points[3];  int nInsidePointCount = 0;
	u32 outside_points[3]; int nOutsidePointCount = 0;

	// Get signed distance of each point in triangle to plane
	for (u32 i=0; i<3; i++) {
		const float d = uni->clip_plane.w - glm_vec3_dot(uni->clip_plane.raw, in->view_pos[idx].vert[i].raw);
		if (d >= 0) {
			inside_points[nInsidePointCount++] = i;
		} else {
			outside_points[nOutsidePointCount++] = i;
		}
	}

	const shader_t* const inf = &out->frags[idx];
	shader_t* const outf0 = &out->frags[oidx];

	if (nInsidePointCount == 0) {
		return 0;
	} else if (nInsidePointCount == 3) {
		/* out->view_pos[oidx] = in->view_pos[idx]; */
		/* outf0->world_pos = inf->world_pos; */
		/* outf0->uv = inf->uv; */
		/* outf0->uv_light = inf->uv_light; */
		/* outf0->norm = inf->norm; */
		/* outf0->tpos = inf->tpos; */
		/* outf0->tviewpos = inf->tviewpos; */
		/* outf0->tlightpos = inf->tlightpos; */
		/* outf0->tshadowpos = inf->tshadowpos; */
		/* tri_clip_apply_uniform(uni, out, oidx); */
		return 0;
	} else if (nInsidePointCount == 1 && nOutsidePointCount == 2) {
		/* copy_point_shader(uni, inside_points[0], inside_points[0], out, idx, oidx); */
		interpolate_point_shader(uni, inside_points[0], outside_points[0], idx, out, idx);
		interpolate_point_shader(uni, inside_points[0], outside_points[1], idx, out, idx);
		/* tri_clip_apply_uniform(uni, out, oidx); */
		return 0;
	}
	/* } else if (nInsidePointCount == 2 && nOutsidePointCount == 1) { */
	/* Triangle 1 */
	/* copy_point_shader(uni, inside_points[0], inside_points[0], out, idx, oidx); */
	/* copy_point_shader(uni, inside_points[1], inside_points[1], out, idx, oidx); */
	interpolate_point_shader(uni, inside_points[0], outside_points[0], idx, out, idx);

	/* Triangle 2 */
	size_t tidx0 = inside_points[0];
	size_t tidx1 = inside_points[1];
	if (tidx1 > tidx0) {
		const size_t tmp = tidx1;
		tidx1 = tidx0;
		tidx0 = tmp;
	}
	copy_point_shader(uni, tidx0, inside_points[1], out, idx, oidx);
	copy_point_shader(uni, tidx1, outside_points[0], out, idx, oidx);
	interpolate_point_shader(uni, inside_points[1], outside_points[0], idx, out, oidx);
	/* tri_clip_apply_uniform(uni, out, oidx); */
	/* tri_clip_apply_uniform(uni, out, oidx+1); */
	return 1;
}

static inline void tri_clip_tangent_frustum(clip_uniform_t* const uni, const vert_out_mesh_t* const in, const size_t tri_idx, shader_bundle_t* const frags) {
	/* vec4s plane = {{-1,0,0}}; */
	/* vec4s plane = (vec4s){{1,0,0,1}}; */
	int tri_clip_cnt;

	uni->clip_plane = (vec4s){{0,0,1,CAM_NEAR_Z}};
	tri_clip_cnt = tri_clip_tangent(uni, in, tri_idx, frags, myarrlenu(frags->frags));
	stbds_header(frags->frags)->length += tri_clip_cnt;

	uni->clip_plane = (vec4s){{1,0,0,-1.0}};
	tri_clip_cnt = tri_clip_tangent(uni, in, tri_idx, frags, myarrlenu(frags->frags));
	/* stbds_header(frags->frags)->length += tri_clip_cnt; */
}
#endif

static inline void tri_clip_basic_apply(const shadow_clip_uniform_t* const uni, shadow_shader_bundle_t* out, const size_t idx) {
	for (int i=0; i<3; i++)
		glm_mat4_mulv((vec4*)uni->proj->raw, out->view_pos[idx].vert[i].raw, out->view_pos[idx].vert[i].raw);
	/* out->skip = uni->skip_shadowfield; */
}

static int tri_clip_shadow(const shadow_clip_uniform_t* const uni, const trip_clip_basic_t* const in, shadow_shader_bundle_t* out_data, const size_t idx) {
	u32 inside_points[3];  int nInsidePointCount = 0;
	u32 outside_points[3]; int nOutsidePointCount = 0;

	// Get signed distance of each point in triangle to plane
	for (u32 i=0; i<3; i++) {
		const float d = uni->near_plane - in->view_pos.vert[i].z;
		if (d >= 0) {
			inside_points[nInsidePointCount++] = i;
		} else {
			outside_points[nOutsidePointCount++] = i;
		}
	}

	if (nInsidePointCount == 0) {
		return 0;
	} else if (nInsidePointCount == 3) {
		out_data->frags[idx].world_pos = in->world_pos;
		out_data->view_pos[idx] = in->view_pos;
		tri_clip_basic_apply(uni, out_data, idx);
		return 1;
	} else if (nInsidePointCount == 1 && nOutsidePointCount == 2) {
		copy_point_shadow(inside_points[0], inside_points[0], in, out_data, idx);
		interpolate_point_shadow(uni, inside_points[0], outside_points[0], in, out_data, idx);
		interpolate_point_shadow(uni, inside_points[0], outside_points[1], in, out_data, idx);
		tri_clip_basic_apply(uni, out_data, idx);
		return 1;
	}
	/* } else if (nInsidePointCount == 2 && nOutsidePointCount == 1) { */
	/* Triangle 1 */
	copy_point_shadow(inside_points[0], inside_points[0], in, out_data, idx);
	copy_point_shadow(inside_points[1], inside_points[1], in, out_data, idx);
	interpolate_point_shadow(uni, inside_points[0], outside_points[0], in, out_data, idx);

	/* Triangle 2 */
	size_t tidx0 = inside_points[0];
	size_t tidx1 = inside_points[1];
	if (tidx1 > tidx0) {
		const size_t tmp = tidx1;
		tidx1 = tidx0;
		tidx0 = tmp;
	}
	copy_point_shadow(tidx0, inside_points[1], in, out_data, idx+1);
	copy_point_shader_shadow(tidx1, outside_points[0], out_data, idx, idx+1);
	interpolate_point_shadow(uni, inside_points[1], outside_points[0], in, out_data, idx+1);
	tri_clip_basic_apply(uni, out_data, idx);
	tri_clip_basic_apply(uni, out_data, idx+1);
	return 2;
}

static size_t vert_postworld_transform(thread_data_t* data, uniform_tbn_t* const uni, size_t tri_idx_out, vert_out_mesh_t** tris_out) {
#ifdef VERBOSE
	myprintf("[vert_postworld_transform] [%d]\n", data->thread_id);
#endif
	engine_t* const e = data->e;
	ecs_t* const ecs = &e->ecs;
	/* Process Mesh */
	vert_tri_idx_t* idxs = data->world_tris.basic.idxs;
	const size_t idx_cnt = myarrlenu(idxs);
	size_t tri_idx = tri_idx_out;
	vert_out_mesh_t* tris = *tris_out;
	for (size_t i=0; i<idx_cnt; i++, idxs++) {
		/* Setup Mesh Uniform */
		glm_mat4_pick3(ecs->mtx[idxs->id].raw, uni->norm_mtx.raw);
		glm_mat3_inv(uni->norm_mtx.raw, uni->norm_mtx.raw);
		glm_mat3_transpose(uni->norm_mtx.raw);

		const mesh_t* const mesh = idxs->mesh;

		/* Process UV Lightmap */
		if (mesh->uv_lightmap) { // TODO optimize out this if?
			for (size_t j=0; j<idxs->cnt; j++) {
				const size_t fidx = j+tri_idx;
				const face_t face = tris->face_idxs[fidx];
				for (int pi=0; pi<3; pi++) {
					tris->uv_light[fidx].uv[pi] = mesh->uv_lightmap[face.p[pi]];
				}
			}
		}

		/* Process Triangles per Mesh */
		for (size_t j=0; j<idxs->cnt; j++, tri_idx++) {
			const face_t face = tris->face_idxs[tri_idx];

			/* Setup UV */
			for (int pi=0; pi<3; pi++) {
				tris->uv[tri_idx].uv[pi] = mesh->uv[face.p[pi]];
			}

			/* Caculate TBN and Create Tangent Shader Values */
			/* TODO make optional */
			for (int pi=0; pi<3; pi++) {
				mat3s TBN;
				calculate_tbn_basic(uni->norm_mtx.raw, tris->norms[tri_idx].col[pi].raw, tris->tangents[tri_idx].col[pi].raw, TBN.raw);
				glm_mat3_mulv(TBN.raw, tris->basic.world_pos[tri_idx].col[pi].raw, tris->tpos[tri_idx].col[pi].raw);
				vec3_assert(tris->basic.world_pos[tri_idx].col[pi].raw);
				vec3_assert(tris->norms[tri_idx].col[pi].raw);
				vec3_assert(tris->tangents[tri_idx].col[pi].raw);
				vec3_assert(TBN.col[0].raw);
				vec3_assert(TBN.col[1].raw);
				vec3_assert(TBN.col[2].raw);
				glm_mat3_mulv(TBN.raw, uni->viewpos.raw, tris->tviewpos[tri_idx].col[pi].raw);
				for (int li=0; li<e->pointlights.cnt; li++)
					glm_mat3_mulv(TBN.raw, uni->pointlights[li].raw, tris->tlightpos[tri_idx].mtx[li].col[pi].raw);
				for (int li=0; li<e->shadowcasters.cnt; li++)
					glm_mat3_mulv(TBN.raw, uni->shadowcasters[li].raw, tris->tshadowpos[tri_idx].mtx[li].col[pi].raw);

				/* Transform World space to View Space warning: confusing placement, not actually part of TBN */
				vec4s tmpv = glms_vec4(tris->basic.world_pos[tri_idx].col[pi], 1);
				glm_mat4_mulv(e->cam.look_at.raw, tmpv.raw, tris->view_pos[tri_idx].vert[pi].raw);
			}
		}
	}
	*tris_out = tris;
	return tri_idx;
}

static size_t vert_postworld_sub(shader_bundle_t* const frags) {
	/* Pre-Triangle Calcs */
	const mat4s viewport = get_viewport(0, 0, SCREEN_W, FB_H);
	for (size_t i=0; i<myarrlenu(frags->frags); i++) {
		shader_t* shader = &frags->frags[i];
		rect_i16* mask = &frags->masks[i];
		for (int pi=0; pi<3; pi++) {
			glm_mat4_mulv((vec4*)viewport.raw, shader->vert_out.vert[pi].raw, shader->pts[pi].raw);
		}

		for (int pi=0; pi<3; pi++) {
			shader->pts2[pi].x = shader->pts[pi].x / shader->pts[pi].w;
			shader->pts2[pi].y = shader->pts[pi].y / shader->pts[pi].w;
		}

		int bboxmin[2] = {SCREEN_W, FB_H};
		int bboxmax[2] = {0, 0};
		for (int ii=0; ii<3; ii++) {
			vec2_assert(shader->pts2[ii].raw);
			for (int j=0; j<2; j++) {
				bboxmin[j] = MIN(bboxmin[j], (int)shader->pts2[ii].raw[j]);
				/* +1.0f is a fast ceilf hack, in very rare circumstances when float is integer value it could spill over */
				bboxmax[j] = MAX(bboxmax[j], (int)(shader->pts2[ii].raw[j]+1.0f));
			}
		}
		mask->x1 = MAX(bboxmin[0], 0);
		mask->x2 = MIN(bboxmax[0], SCREEN_W);
		mask->y1 = MAX(bboxmin[1], 0);
		mask->y2 = MIN(bboxmax[1], FB_H);
	}
	return myarrlenu(frags->frags);
}

static void vert_postworld(thread_data_t* const data) {
#ifdef VERBOSE
	myprintf("[vert_postworld] [%d]\n", data->thread_id);
#endif
	engine_t* const e = data->e;
	ecs_t* const ecs = &e->ecs;

	/* Setup Uniform */
	uniform_tbn_t uni;
	uni.viewpos = e->cam.pos;
	for (int i=0; i<e->pointlights.cnt; i++)
		uni.pointlights[i] = e->pointlights.cubemaps[i].pos;
	for (int i=0; i<e->shadowcasters.cnt; i++)
		uni.shadowcasters[i] = e->shadowcasters.cubemaps[i].basic.pos;

	vert_out_mesh_t* tris = &data->world_tris;
	size_t tri_idx = 0;

	/* Update Buffer Lengths */
	const size_t buf_len = myarrlenu(tris->basic.world_pos);
	arrsetlen(tris->view_pos, buf_len);
	arrsetlen(tris->uv, buf_len);
	arrsetlen(tris->uv_light, buf_len);
	arrsetlen(tris->tpos, buf_len);
	arrsetlen(tris->tviewpos, buf_len);
	arrsetlen(tris->tlightpos, buf_len);
	arrsetlen(tris->tshadowpos, buf_len);

	/* Process Mesh */
	tri_idx = vert_postworld_transform(data, &uni, tri_idx, &tris);

	/* Setup Clipping Uniform */
	clip_uniform_t clip_uni;
	clip_uni.cam = &data->e->cam;
	clip_uni.clip_plane = (vec4s){{0,0,1,-CAM_NEAR_Z}}; // TODO TMP
	clip_uni.litdata.lightmap = NULL;
	clip_uni.pointlight_cnt = e->pointlights.cnt;
	clip_uni.shadowcaster_cnt = e->shadowcasters.cnt;

	/* Reset Output Buffer */
	stbds_header(data->vert_out_data.tris.wfrags.frags)->length = 0;
	stbds_header(data->vert_out_data.tris.wfrags.masks)->length = 0;
	stbds_header(data->vert_out_data.tris.wfrags.view_pos)->length = 0;
	stbds_header(data->vert_out_data.tris.mfrags.frags)->length = 0;
	stbds_header(data->vert_out_data.tris.mfrags.masks)->length = 0;
	stbds_header(data->vert_out_data.tris.mfrags.view_pos)->length = 0;

	/* Clip Triangle */
	const vert_tri_idx_t* idxs = data->world_tris.basic.idxs;
	const size_t idx_cnt = myarrlenu(idxs);
#ifdef VERBOSE
	myprintf("[vert_postworld clip] [%d] idx_cnt:%lu idxs->cnt:%u\n", data->thread_id, idx_cnt, idxs->cnt);
#endif
	const int scene_time = e->scene_time*15.0f;
	for (size_t i=0, tri_idx=0; i<idx_cnt; i++, idxs++) {
		/* myprintf("id:%d i:%lu idx_cnt:%lu idxs->cnt:%u\n", data->thread_id, i, idx_cnt, idxs->cnt); */
		const mesh_t* const mesh = idxs->mesh;

		clip_uni.shader_idx = mesh->mat_type;
		const flag_t flags = ecs->flags[idxs->id];
		if (e->vfx_flags.glitchout) {
			clip_uni.tex_diffuse = e->vfx_fb;
			clip_uni.tex_normal = e->assets.px_rgb[PX_RGB_NMAP_DEFAULT];
		} else {
			int diff_idx = 0;
			if (mesh->tex_diffuse_cnt > 0)
				diff_idx = scene_time%mesh->tex_diffuse_cnt;
			clip_uni.tex_diffuse = mesh->tex_diffuse[diff_idx];
			clip_uni.tex_normal = mesh->tex_normal;
		}
		lightcache_t lightcache = {0}; // TODO actually cache this lmao
		clip_uni.ambient = R_LightPoint(&e->assets.map, ecs->pos[idxs->id], ecs->bbox[idxs->id].max.y*0.5f, &lightcache) * 0.5;
		if (flags.highlight) {
			clip_uni.color_mod = 1;
		} else {
			clip_uni.color_mod = 0;
		}

		const size_t new_wsize = myarrlenu(data->vert_out_data.tris.wfrags.frags) + idxs->cnt*2;
		arrsetcap(data->vert_out_data.tris.wfrags.frags, new_wsize);
		arrsetcap(data->vert_out_data.tris.wfrags.masks, new_wsize);
		arrsetcap(data->vert_out_data.tris.wfrags.view_pos, new_wsize);

		const size_t new_msize = myarrlenu(data->vert_out_data.tris.mfrags.frags) + idxs->cnt*2;
		arrsetcap(data->vert_out_data.tris.mfrags.frags, new_msize);
		arrsetcap(data->vert_out_data.tris.mfrags.masks, new_msize);
		arrsetcap(data->vert_out_data.tris.mfrags.view_pos, new_msize);

		shader_bundle_t* const mfrags = &data->vert_out_data.tris.mfrags;
		shader_bundle_t* const wfrags = &data->vert_out_data.tris.wfrags;

		if (flags.bones) {
			for (size_t j=0; j<idxs->cnt; j++, tri_idx++) {
				const int tri_clip_cnt = tri_clip_tangent(&clip_uni, tris, tri_idx, mfrags, myarrlenu(mfrags->frags));
				stbds_header(mfrags->frags)->length += tri_clip_cnt;
			}
		} else if (flags.worldmap || flags.bsp_model) {
			/* Process Triangles per Mesh */
			if (e->vfx_flags.noise && clip_uni.shader_idx == SHADER_WORLD_DEFAULT)
				clip_uni.shader_idx = SHADER_WORLD_NOISE;
			for (size_t j=0; j<idxs->cnt; j++, tri_idx++) {
				if (mesh->litdata) clip_uni.litdata = mesh->litdata[tris->face_id[tri_idx]];
				else clip_uni.litdata.lightmap = NULL;
				const int tri_clip_cnt = tri_clip_tangent(&clip_uni, tris, tri_idx, wfrags, myarrlenu(wfrags->frags));
				stbds_header(wfrags->frags)->length += tri_clip_cnt;
#if 0
				/* WIP complex implementation */
				tri_clip_tangent_frustum(&clip_uni, tris, tri_idx, wfrags);
#endif
			}
		} else if (flags.tangent) {
			for (size_t j=0; j<idxs->cnt; j++, tri_idx++) {
				const int tri_clip_cnt = tri_clip_tangent(&clip_uni, tris, tri_idx, mfrags, myarrlenu(mfrags->frags));
				stbds_header(mfrags->frags)->length += tri_clip_cnt;
			}
		} else {
			for (size_t j=0; j<idxs->cnt; j++, tri_idx++) {
				const int tri_clip_cnt = tri_clip_tangent(&clip_uni, tris, tri_idx, mfrags, myarrlenu(mfrags->frags));
				stbds_header(mfrags->frags)->length += tri_clip_cnt;
			}
		}
	}

	/* masks should match frag length */
	shader_bundle_t* const wfrags = &data->vert_out_data.tris.wfrags;
	shader_bundle_t* const mfrags = &data->vert_out_data.tris.mfrags;
	stbds_header(wfrags->masks)->length = stbds_header(wfrags->frags)->length;
	stbds_header(wfrags->view_pos)->length = stbds_header(wfrags->frags)->length;
	stbds_header(mfrags->masks)->length = stbds_header(mfrags->frags)->length;
	stbds_header(mfrags->view_pos)->length = stbds_header(mfrags->frags)->length;

	/* Pre-Triangle Calcs */
	data->wtri_cnt = vert_postworld_sub(wfrags);
	data->mtri_cnt = vert_postworld_sub(mfrags);

#ifdef VERBOSE
	myprintf("[vert_postworld done] [%d]\n", data->thread_id);
#endif
}

static void shader_vert_anim_basic(const mesh_t* const mesh, const size_t fidx, const vert_uniform_t* const uni, vert_out_mesh_t* out, const size_t idx) {
#ifdef VERBOSE2
	fputs("[vertex_anim]\n", stdout);
#endif
	/* Bone Influence */
	out->face_idxs[idx] = mesh->faces[fidx];
	out->face_id[idx] = fidx;
	mat3s* const pos = &out->basic.world_pos[idx];
	for (int pi=0; pi<3; pi++) {
		const size_t vidx = out->face_idxs[idx].p[pi];
		const vec4s world_pos = glms_vec4(mesh->verts[vidx].pos, 1);
		const vec4s norm = glms_vec4(mesh->verts[vidx].norm, 0);
		const vec4s tangent = glms_vec4(mesh->verts[vidx].tangent, 0);
		vec4s total_pos = {0};
		vec4s total_norm = {0};
		vec4s total_tangent = {0};
		const weight_t* const weights = mesh->weights[vidx].weights;
		for (size_t i=0; i<MAX_BONE_INFLUENCE; i++) {
			const int bone_idx = weights[i].bone_idx;
			if (bone_idx < 0)
				break;

			/* Transform Position */
			vec4s local_pos;
			glm_mat4_mulv(uni->anim.bone_mtx[bone_idx].raw, (float*)world_pos.raw, local_pos.raw);
			const float weight = weights[i].weight;
			glm_vec4_scale(local_pos.raw, weight, local_pos.raw);
			glm_vec4_add(total_pos.raw, local_pos.raw, total_pos.raw);

			vec4s local_norm;
			glm_mat4_mulv(uni->anim.bone_mtx[bone_idx].raw, (float*)norm.raw, local_norm.raw);
			glm_vec4_scale(local_norm.raw, weight, local_norm.raw);
			glm_vec4_add(total_norm.raw, local_norm.raw, total_norm.raw);

			vec4s local_tangent;
			glm_mat4_mulv(uni->anim.bone_mtx[bone_idx].raw, (float*)tangent.raw, local_tangent.raw);
			glm_vec4_scale(local_tangent.raw, weight, local_tangent.raw);
			glm_vec4_add(total_tangent.raw, local_tangent.raw, total_tangent.raw);
		}

		/* Copy Position (world space) */
		glm_mat4_mulv((vec4*)uni->mtx.raw, total_pos.raw, total_pos.raw);
		pos->col[pi].x = total_pos.x;
		pos->col[pi].y = total_pos.y;
		pos->col[pi].z = total_pos.z;

		/* Copy Normal (model space) */
		out->norms[idx].col[pi].x = total_norm.x;
		out->norms[idx].col[pi].y = total_norm.y;
		out->norms[idx].col[pi].z = total_norm.z;

		/* Copy Tangent (model space) */
		vec3_assert(total_tangent.raw);
		out->tangents[idx].col[pi].x = total_tangent.x;
		out->tangents[idx].col[pi].y = total_tangent.y;
		out->tangents[idx].col[pi].z = total_tangent.z;
	}
}

static void shader_vert_tangent_basic(const mesh_t* const mesh, const size_t face_idx, const vert_uniform_t* const uni, vert_out_mesh_t* out, const size_t idx, const mat3s* const verts) {
	out->face_idxs[idx] = mesh->faces[face_idx];
	out->face_id[idx] = face_idx;

	/* Copy Position (world space) */
	out->basic.world_pos[idx] = *verts;

	for (int i=0; i<3; i++) {
		const size_t vidx = out->face_idxs[idx].p[i];

		/* Copy Normal (model space) */
		out->norms[idx].col[i] = mesh->verts[vidx].norm;

		/* Copy Tangent (model space) */
		out->tangents[idx].col[i] = mesh->verts[vidx].tangent;
		vec3_assert(mesh->verts[vidx].tangent.raw);
	}
}

static inline void shader_vert_tangent_basic_extra(const mesh_t* const mesh, const size_t face_idx, const vert_uniform_t* const uni, mat3s* out) {
	face_t face = mesh->faces[face_idx];
	for (int i=0; i<3; i++) {
		const size_t vidx = face.p[i];
		vec4s vert_pos = glms_vec4(mesh->verts[vidx].pos, 1);
		glm_mat4_mulv((vec4*)uni->mtx.raw, vert_pos.raw, vert_pos.raw);
		glm_vec3_copy(vert_pos.raw, out->col[i].raw); // copy vec3 from vec4

		/* World Warp */
		if (uni->warpcircle.w >= 0) {
			const float dist = glm_vec3_distance((float*)uni->warpcircle.raw, out->col[i].raw);
			float sdf = fabsf(dist - uni->warpcircle.w);
			if (sdf > 0.0f) {
				sdf = MAX(sdf, 1);
				sdf = 1.0 / (sdf*sdf);
			}
			out->col[i].y += sdf;
		}
	}
}

static inline void shader_vert_basic(const mesh_t* const mesh, const size_t face_idx, const vert_uniform_t* const uni, vert_out_mesh_t* out, const size_t out_idx) {
	out->face_idxs[out_idx] = mesh->faces[face_idx];
	out->face_id[out_idx] = face_idx;
	for (int i=0; i<3; i++) {
		const size_t vidx = out->face_idxs[out_idx].p[i];
		/* Copy Position (world space) */
		vec4s vert_pos = glms_vec4(mesh->verts[vidx].pos, 1);
		glm_mat4_mulv((vec4*)uni->mtx.raw, vert_pos.raw, vert_pos.raw);
		glm_vec3_copy(vert_pos.raw, out->basic.world_pos[out_idx].col[i].raw); // copy vec3 from vec4

		/* Copy Normal (model space) */
		out->norms[out_idx].col[i] = mesh->verts[vidx].norm;

		/* TODO review */
		/* Null Tangents (model space) */
		memset(&out->tangents[out_idx], 0, sizeof(out->tangents[out_idx]));
	}
}

static inline void shader_vert_basic_warp(const mesh_t* const mesh, const size_t face_idx, const vert_uniform_t* const uni, vert_out_mesh_t* out, const size_t out_idx, const float scene_time, const float length) {
	out->face_idxs[out_idx] = mesh->faces[face_idx];
	out->face_id[out_idx] = face_idx;
	for (int i=0; i<3; i++) {
		const size_t vidx = out->face_idxs[out_idx].p[i];
		/* Copy Position (world space) */
		vec4s vert_pos = glms_vec4(mesh->verts[vidx].pos, 1);

		const float t = 1.0f - fabsf(vert_pos.z - 0.5f)*2;
		vert_pos.x *= 1.0f + vert_pos.z * t * length * 10;
		vert_pos.x *= 0.01;
		vert_pos.y *= 1.0f + vert_pos.z * t * length * 10;
		vert_pos.y *= 0.01;

		vert_pos.z *= length;
		float sinval, cosval;
		sincosf(vert_pos.z+scene_time*10, &sinval, &cosval);
		vert_pos.x += t*cosval*0.1*length;
		vert_pos.y += t*sinval*0.1*length;

		glm_mat4_mulv((vec4*)uni->mtx.raw, vert_pos.raw, vert_pos.raw);
		glm_vec3_copy(vert_pos.raw, out->basic.world_pos[out_idx].col[i].raw); // copy vec3 from vec4

		/* Copy Normal (model space) */
		out->norms[out_idx].col[i] = mesh->verts[vidx].norm;

		/* TODO review */
		/* Null Tangents (model space) */
		memset(&out->tangents[out_idx], 0, sizeof(out->tangents[out_idx]));
	}
}


static void shadow_viewport(shadow_shader_bundle_t* const in) {
	const size_t len = myarrlenu(in->frags);
#ifdef VERBOSE
	myprintf("[shadow_viewport] len:%zu\n", len);
#endif
	const mat4s viewport = get_viewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	frag_shadow_t* frag = in->frags;
	vec4_3* view_pos = in->view_pos;
	rect_i16* mask = in->masks;
	for (size_t i=0; i<len; i++, frag++, mask++, view_pos++) {
		for (int pi=0; pi<3; pi++)
			glm_mat4_mulv((vec4*)viewport.raw, view_pos->vert[pi].raw, frag->pts.vert[pi].raw);

		for (int pi=0; pi<3; pi++) {
			frag->pts2[pi].x = frag->pts.vert[pi].x / frag->pts.vert[pi].w;
			frag->pts2[pi].y = frag->pts.vert[pi].y / frag->pts.vert[pi].w;
		}

		int bboxmin[2] = {SHADOW_WIDTH, SHADOW_HEIGHT};
		int bboxmax[2] = {0, 0};
		for (int ii=0; ii<3; ii++) {
			vec2_assert(frag->pts2[ii].raw);
			for (int j=0; j<2; j++) {
				bboxmin[j] = MIN(bboxmin[j], (int)frag->pts2[ii].raw[j]);
				/* +1.0f is a fast ceilf hack, in very rare circumstances when float is integer value it could spill over */
				bboxmax[j] = MAX(bboxmax[j], (int)(frag->pts2[ii].raw[j]+1.0f));
			}
		}
		mask->x1 = MAX(bboxmin[0], 0);
		mask->x2 = MIN(bboxmax[0], SHADOW_WIDTH);
		mask->y1 = MAX(bboxmin[1], 0);
		mask->y2 = MIN(bboxmax[1], SHADOW_HEIGHT);
	}
}

static inline void reset_vert_output(vert_out_mesh_t* const buf) {
	arrsetlen(buf->basic.idxs, 0);
	arrsetlen(buf->basic.world_pos, 0);
	arrsetlen(buf->face_id, 0);
	arrsetlen(buf->face_idxs, 0);
	arrsetlen(buf->view_pos, 0);
	arrsetlen(buf->norms, 0);
	arrsetlen(buf->tangents, 0);
	arrsetlen(buf->uv, 0);
	arrsetlen(buf->uv_light, 0);
}

static inline void reset_vert_output_basic(vert_out_mesh_basic_t* const buf) {
	arrsetlen(buf->idxs, 0);
	arrsetlen(buf->world_pos, 0);
}

static void calc_shadowmap(cubemap_t* const shadowmap, const vert_out_mesh_basic_t* const in, shadow_job_t* const shadow_out, const u32 cube_face_idx) {
#ifdef VERBOSE2
	myprintf("[calc_shadowmap]\n");
#endif
	/* cubemap_t* const shadowmap = &data->e->shadowmaps.cubemaps[cube_idx]; */

	shadow_clip_uniform_t clip_uni;
	clip_uni.near_plane = -CAM_NEAR_Z;
	clip_uni.proj = &shadowmap->proj;

	/* we've already reset the len to 0 */
	const size_t len = myarrlenu(shadow_out->tris.frags) + in->tri_total*2;
	arrsetcap(shadow_out->tris.masks, len);
	arrsetcap(shadow_out->tris.frags, len);
	arrsetcap(shadow_out->tris.view_pos, len);

	/* Transform and Clip */
	const size_t idx_cnt = myarrlenu(in->idxs);
	const size_t start_cnt = myarrlenu(shadow_out->tris.frags);
	mat4s* const mtx = &shadowmap->cube_mtx[cube_face_idx];

	vert_tri_idx_t* idxs = in->idxs;
	const mat3s* in_iter = in->world_pos;
	for (size_t j=0; j<idx_cnt; j++, idxs++) {
		for (size_t k=0; k<idxs->cnt; k++, in_iter++) {
			if (check_backface(in_iter, &shadowmap->pos))
				continue;
			trip_clip_basic_t vtmp;
			for (int pi=0; pi<3; pi++) {
				vtmp.world_pos.col[pi] = in_iter->col[pi];
				glm_vec4(vtmp.world_pos.col[pi].raw, 1, vtmp.view_pos.vert[pi].raw);
				glm_mat4_mulv(mtx->raw, vtmp.view_pos.vert[pi].raw, vtmp.view_pos.vert[pi].raw);
			}
			const int tri_clip_cnt = tri_clip_shadow(&clip_uni, &vtmp, &shadow_out->tris, stbds_header(shadow_out->tris.frags)->length);
			stbds_header(shadow_out->tris.frags)->length += tri_clip_cnt;
		}
	}
	const size_t end_cnt = myarrlenu(shadow_out->tris.frags);
	stbds_header(shadow_out->tris.masks)->length = end_cnt;
	stbds_header(shadow_out->tris.view_pos)->length = end_cnt;
	shadow_out->tri_cnt[cube_face_idx] += end_cnt-start_cnt; // += because we call the function multiple times
}

static inline void inc_vert_out_basic(vert_out_mesh_basic_t* const buf, const u32 face_cnt, vert_tri_idx_t* idx, const mesh_t* mesh) {
	idx->cnt = 0;
	idx->mesh = mesh;
	const size_t buf_cnt_extra = myarrlenu(buf->world_pos);
	const size_t buf_cnt_post_extra = buf_cnt_extra + face_cnt;
	arrsetcap(buf->world_pos, buf_cnt_post_extra);
}

static inline void inc_vert_out(vert_out_mesh_t* const buf, const u32 face_cnt, vert_tri_idx_t* idx, const mesh_t* mesh) {
	idx->cnt = 0;
	idx->mesh = mesh;
	const size_t buf_cnt = myarrlenu(buf->basic.world_pos);
	const size_t buf_cnt_post = buf_cnt + face_cnt;
	arrsetcap(buf->basic.world_pos, buf_cnt_post);
	arrsetcap(buf->face_id, buf_cnt_post);
	arrsetcap(buf->face_idxs, buf_cnt_post);
	arrsetcap(buf->norms, buf_cnt_post);
	arrsetcap(buf->tangents, buf_cnt_post);
}

static inline void push_cubemap(const mat3s* const tri, int cnt, vert_tri_idx_t* const idxs, vert_out_cubemap_t* const out) {
	for (int li=0; li<cnt; li++) {
		vert_out_mesh_basic_t* const cube_out = &out[li].models;
		const size_t idx = myarrlenu(cube_out->world_pos);
		cube_out->world_pos[idx] = *tri;
		stbds_header(cube_out->world_pos)->length++;
		idxs[li].cnt++;
	}
}

void
#ifdef SHADER_NAME_DEFAULT
job_vert_basic
#endif
#ifdef SHADER_NAME_NO_PL
job_vert_basic_no_pl
#endif
#ifdef SHADER_NAME_NO_SC
job_vert_basic_no_sc
#endif
#ifdef SHADER_NAME_NO_PLSC
job_vert_basic_no_plsc
#endif
(thread_data_t* data) {
#ifdef VERBOSE
	myprintf("job_vert: data:0x%lx id:%d\n", data, data->thread_id);
#endif
	TIME_TYPE start = get_time();
	engine_t* const e = data->e;
	ecs_t* const ecs = &e->ecs;

#if 0
	/* Count Triangles */
	size_t total_meshes = 0;
	size_t total_tris = 0;
	for (uint32_t i=0; i<data->model_in_cnt; i++) {
		const size_t idx = data->entities[i];
		const model_basic_t* const model = &ecs->model[idx].model_anim->basic;
		const uint32_t mesh_cnt = model->mesh_cnt;
		total_meshes += mesh_cnt;
		for (uint32_t mi=0; mi<mesh_cnt; mi++) {
			total_tris += model->meshes[mi].face_cnt;
		}
	}
#endif

	/* Reset Vertex Output Buffers */
	reset_vert_output(&data->world_tris);
	reset_vert_output(&data->worldmap_tris);
	for (int i=0; i<e->pointlights.cnt; i++) {
		reset_vert_output_basic(&data->pointlight_tris[i].world);
		reset_vert_output_basic(&data->pointlight_tris[i].models);
	}

	for (int i=0; i<e->shadowcasters.cnt; i++) {
		reset_vert_output_basic(&data->shadowcaster_tris[i].world);
		reset_vert_output_basic(&data->shadowcaster_tris[i].models);
	}

	const visflags_t* const visflags_arr = &e->visflags[VISFLAG_IDX_PLAYER];
	const visflags_t* const visflags_arr_global = &e->visflags[VISFLAG_IDX_GLOBAL];
#ifdef SHADER_PL
	const visflags_t* const visflags_arr_pointlights = &e->visflags[VISFLAG_IDX_POINTLIGHTS];
#endif
#ifdef SHADER_SC
	const visflags_t* const visflags_arr_shadowcasters = &e->visflags[VISFLAG_IDX_SHADOWCASTERS];
#endif

	/* Transform Vertex */
	for (u32 i=0; i<myarrlenu(data->vert_job_data); i++) {
		const job_vert_t* const job = &data->vert_job_data[i];
		vert_tri_idx_t idx_data;
		vert_tri_idx_t idx_data_worldmap;
		idx_data.id = job->id;
		idx_data_worldmap.id = job->id;
#ifdef SHADER_PL
		vert_tri_idx_t idxs_pointlights_world[MAX_DYNAMIC_LIGHTS];
		vert_tri_idx_t idxs_pointlights_models[MAX_DYNAMIC_LIGHTS];
		for (int li=0; li<e->pointlights.cnt; li++) {
			idxs_pointlights_world[li].id = job->id;
			idxs_pointlights_models[li].id = job->id;
		}
#endif
#ifdef SHADER_SC
		vert_tri_idx_t idxs_shadowcasters_world[MAX_DYNAMIC_LIGHTS];
		vert_tri_idx_t idxs_shadowcasters_models[MAX_DYNAMIC_LIGHTS];
		for (int li=0; li<e->shadowcasters.cnt; li++) {
			idxs_shadowcasters_world[li].id = job->id;
			idxs_shadowcasters_models[li].id = job->id;
		}
#endif

		/* Set Per Entity Uniform Values */
		vert_uniform_t vuniform;
		vuniform.mtx = ecs->mtx[idx_data.id];
		vuniform.warpcircle = e->warpcircle;

		/* Iterate Meshes */
		const mesh_t* const mesh = job->mesh;
		const BITARR_TYPE* const visflags = visflags_arr->mesh[job->mesh_idx];
		const BITARR_TYPE* const visflags_global = visflags_arr_global->mesh[job->mesh_idx];
		inc_vert_out(&data->world_tris, job->face_cnt, &idx_data, mesh);
		inc_vert_out(&data->worldmap_tris, job->face_cnt, &idx_data_worldmap, mesh);
		/* Pointlights */
#ifdef SHADER_PL
		for (int li=0; li<e->pointlights.cnt; li++) {
			inc_vert_out_basic(&data->pointlight_tris[li].world, job->face_cnt, &idxs_pointlights_world[li], mesh);
			inc_vert_out_basic(&data->pointlight_tris[li].models, job->face_cnt, &idxs_pointlights_models[li], mesh);
		}
#endif
		/* Shadowcasters */
#ifdef SHADER_SC
		for (int li=0; li<e->shadowcasters.cnt; li++) {
			inc_vert_out_basic(&data->shadowcaster_tris[li].world, job->face_cnt, &idxs_shadowcasters_world[li], mesh);
			inc_vert_out_basic(&data->shadowcaster_tris[li].models, job->face_cnt, &idxs_shadowcasters_models[li], mesh);
		}
#endif

		const flag_t flags = ecs->flags[job->id];
		if (flags.bones) {
			vuniform.anim = ecs->anim[job->id];
			for (uint32_t fi=0; fi<job->face_cnt; fi++) {
				const size_t fidx = job->face_idx+fi;
				const size_t out_idx = myarrlenu(data->world_tris.basic.world_pos);
				shader_vert_anim_basic(mesh, fidx, &vuniform, &data->world_tris, out_idx);
				stbds_header(data->world_tris.basic.world_pos)->length++;
				idx_data.cnt++;

				const mat3s* world_tri = &data->world_tris.basic.world_pos[out_idx];
#ifdef SHADER_PL
				push_cubemap(world_tri, e->pointlights.cnt, idxs_pointlights_models, data->pointlight_tris);
#endif
#ifdef SHADER_SC
				push_cubemap(world_tri, e->shadowcasters.cnt, idxs_shadowcasters_models, data->shadowcaster_tris);
#endif
			}
		} else if (flags.worldmap) {
#ifdef SHADER_PL
			pointlight_data_t* const pointlights = &e->pointlights;
			vert_out_cubemap_t* const pointlight_tris = data->pointlight_tris;
#endif
#ifdef SHADER_SC
			shadowmap_data_t* const shadowcasters = &e->shadowcasters;
			vert_out_cubemap_t* const shadowcaster_tris = data->shadowcaster_tris;
#endif
			for (uint32_t fi=0; fi<job->face_cnt; fi++) {
				/* skip if the triangle isn't visible */
				if (bitarr_get(visflags_global, job->face_idx+fi)) {
					/* basic transform to copy */
					mat3s world_tri;
					shader_vert_tangent_basic_extra(mesh, job->face_idx+fi, &vuniform, &world_tri);

					if (bitarr_get(visflags, job->face_idx+fi)) {
						shader_vert_tangent_basic(mesh, job->face_idx+fi, &vuniform, &data->world_tris, myarrlenu(data->world_tris.basic.world_pos), &world_tri);
						stbds_header(data->world_tris.basic.world_pos)->length++;
						idx_data.cnt++;
					}

#ifdef SHADER_PL
					if (mesh->mat_type != 1) {
						for (int ci=0; ci<pointlights->cnt; ci++) {
							if (pointlights->cubemaps[ci].clean)
								continue;
							if (bitarr_get(visflags_arr_pointlights[ci].mesh[job->mesh_idx], job->face_idx+fi)) {
								vert_out_mesh_basic_t* const cube_out = &pointlight_tris[ci].world;
								const size_t idx = myarrlenu(cube_out->world_pos);
								cube_out->world_pos[idx] = world_tri;
								stbds_header(cube_out->world_pos)->length++;
								idxs_pointlights_world[ci].cnt++;
							}
						}
					}
#endif
#ifdef SHADER_SC
					if (mesh->mat_type != 1) {
						for (int ci=0; ci<shadowcasters->cnt; ci++) {
							if (shadowcasters->cubemaps[ci].basic.clean)
								continue;
							if (bitarr_get(visflags_arr_shadowcasters[ci].mesh[job->mesh_idx], job->face_idx+fi)) {
								vert_out_mesh_basic_t* const cube_out = &shadowcaster_tris[ci].world;
								const size_t idx = myarrlenu(cube_out->world_pos);
								cube_out->world_pos[idx] = world_tri;
								stbds_header(cube_out->world_pos)->length++;
								idxs_shadowcasters_world[ci].cnt++;
							}
						}
					}
#endif
				}
			}
		} else if (flags.bsp_model) {
#ifdef SHADER_PL
			pointlight_data_t* const pointlights = &e->pointlights;
			vert_out_cubemap_t* const pointlight_tris = data->pointlight_tris;
#endif
#ifdef SHADER_SC
			shadowmap_data_t* const shadowcasters = &e->shadowcasters;
			vert_out_cubemap_t* const shadowcaster_tris = data->shadowcaster_tris;
#endif
			for (uint32_t fi=0; fi<job->face_cnt; fi++) {
				/* basic transform to copy */
				mat3s world_tri;
				shader_vert_tangent_basic_extra(mesh, job->face_idx+fi, &vuniform, &world_tri);

				shader_vert_tangent_basic(mesh, job->face_idx+fi, &vuniform, &data->world_tris, myarrlenu(data->world_tris.basic.world_pos), &world_tri);
				stbds_header(data->world_tris.basic.world_pos)->length++;
				idx_data.cnt++;

#ifdef SHADER_PL
				if (mesh->mat_type != 1) {
					for (int ci=0; ci<pointlights->cnt; ci++) {
						vert_out_mesh_basic_t* const cube_out = &pointlight_tris[ci].world;
						const size_t idx = myarrlenu(cube_out->world_pos);
						cube_out->world_pos[idx] = world_tri;
						stbds_header(cube_out->world_pos)->length++;
						idxs_pointlights_world[ci].cnt++;
					}
				}
#endif
#ifdef SHADER_SC
				if (mesh->mat_type != 1) {
					for (int ci=0; ci<shadowcasters->cnt; ci++) {
						vert_out_mesh_basic_t* const cube_out = &shadowcaster_tris[ci].world;
						const size_t idx = myarrlenu(cube_out->world_pos);
						cube_out->world_pos[idx] = world_tri;
						stbds_header(cube_out->world_pos)->length++;
						idxs_shadowcasters_world[ci].cnt++;
					}
				}
#endif
			}
		} else if (flags.static_model_osc) {
			for (uint32_t fi=0; fi<job->face_cnt; fi++) {
				const size_t fidx = job->face_idx+fi;
				const size_t out_idx = myarrlenu(data->world_tris.basic.world_pos);
				shader_vert_basic_warp(mesh, fidx, &vuniform, &data->world_tris, out_idx, e->scene_time, ecs->shader[job->id].warp.length);
				stbds_header(data->world_tris.basic.world_pos)->length++;
				idx_data.cnt++;

				if (!flags.no_shadow) {
					const mat3s* world_tri = &data->world_tris.basic.world_pos[out_idx];
#ifdef SHADER_PL
					push_cubemap(world_tri, e->pointlights.cnt, idxs_pointlights_models, data->pointlight_tris);
#endif
#ifdef SHADER_SC
					push_cubemap(world_tri, e->shadowcasters.cnt, idxs_shadowcasters_models, data->shadowcaster_tris);
#endif
				}
			}
		} else {
			for (uint32_t fi=0; fi<job->face_cnt; fi++) {
				const size_t fidx = job->face_idx+fi;
				const size_t out_idx = myarrlenu(data->world_tris.basic.world_pos);
				shader_vert_basic(mesh, fidx, &vuniform, &data->world_tris, out_idx);
				stbds_header(data->world_tris.basic.world_pos)->length++;
				idx_data.cnt++;

				if (!flags.no_shadow) {
					const mat3s* world_tri = &data->world_tris.basic.world_pos[out_idx];
#ifdef SHADER_PL
					push_cubemap(world_tri, e->pointlights.cnt, idxs_pointlights_models, data->pointlight_tris);
#endif
#ifdef SHADER_SC
					push_cubemap(world_tri, e->shadowcasters.cnt, idxs_shadowcasters_models, data->shadowcaster_tris);
#endif
				}
			}
		}

		/* push any created mesh idxs onto an array */
		if (idx_data.cnt > 0)
			arrput(data->world_tris.basic.idxs, idx_data);

		/* Push Pointlights Idxs */
#ifdef SHADER_PL
		for (int li=0; li<e->pointlights.cnt; li++) {
			if (idxs_pointlights_world[li].cnt > 0)
				arrput(data->pointlight_tris[li].world.idxs, idxs_pointlights_world[li]);
			if (idxs_pointlights_models[li].cnt > 0)
				arrput(data->pointlight_tris[li].models.idxs, idxs_pointlights_models[li]);
		}
#endif
		/* Push Shadowcasters Idxs */
#ifdef SHADER_SC
		for (int li=0; li<e->shadowcasters.cnt; li++) {
			if (idxs_shadowcasters_world[li].cnt > 0)
				arrput(data->shadowcaster_tris[li].world.idxs, idxs_shadowcasters_world[li]);
			if (idxs_shadowcasters_models[li].cnt > 0)
				arrput(data->shadowcaster_tris[li].models.idxs, idxs_shadowcasters_models[li]);
		}
#endif
	}
	const size_t world_pos_cnt = myarrlenu(data->world_tris.basic.world_pos);
	stbds_header(data->world_tris.face_id)->length = world_pos_cnt;
	stbds_header(data->world_tris.face_idxs)->length = world_pos_cnt;
	stbds_header(data->world_tris.norms)->length = world_pos_cnt;
	stbds_header(data->world_tris.tangents)->length = world_pos_cnt;
	data->vert_world_time = time_diff(start, get_time())*1000;

	/* Count Triangles */
	for (int li=0; li<e->pointlights.cnt; li++) {
		data->pointlight_tris[li].world.tri_total = 0;
		for (size_t j=0; j<myarrlenu(data->pointlight_tris[li].world.idxs); j++)
			data->pointlight_tris[li].world.tri_total += data->pointlight_tris[li].world.idxs[j].cnt;
		data->pointlight_tris[li].models.tri_total = 0;
		for (size_t j=0; j<myarrlenu(data->pointlight_tris[li].models.idxs); j++)
			data->pointlight_tris[li].models.tri_total += data->pointlight_tris[li].models.idxs[j].cnt;
	}
	for (int li=0; li<e->shadowcasters.cnt; li++) {
		data->shadowcaster_tris[li].world.tri_total = 0;
		for (size_t j=0; j<myarrlenu(data->shadowcaster_tris[li].world.idxs); j++)
			data->shadowcaster_tris[li].world.tri_total += data->shadowcaster_tris[li].world.idxs[j].cnt;
		data->shadowcaster_tris[li].models.tri_total = 0;
		for (size_t j=0; j<myarrlenu(data->shadowcaster_tris[li].models.idxs); j++)
			data->shadowcaster_tris[li].models.tri_total += data->shadowcaster_tris[li].models.idxs[j].cnt;
	}

	/* Shadow Mapping */
	start = get_time();
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++) {
		/* Point Lights */
		arrsetlen(data->vert_out_data.pointlight_world[i].tris.masks, 0);
		arrsetlen(data->vert_out_data.pointlight_world[i].tris.frags, 0);
		for (int j=0; j<CUBE_FACE_CNT; j++)
			data->vert_out_data.pointlight_world[i].tri_cnt[j] = 0;
		arrsetlen(data->vert_out_data.pointlight_model[i].tris.masks, 0);
		arrsetlen(data->vert_out_data.pointlight_model[i].tris.frags, 0);
		for (int j=0; j<CUBE_FACE_CNT; j++)
			data->vert_out_data.pointlight_model[i].tri_cnt[j] = 0;

		/* Shadowcasters */
		arrsetlen(data->vert_out_data.shadow_world[i].tris.masks, 0);
		arrsetlen(data->vert_out_data.shadow_world[i].tris.frags, 0);
		for (int j=0; j<CUBE_FACE_CNT; j++)
			data->vert_out_data.shadow_world[i].tri_cnt[j] = 0;
		arrsetlen(data->vert_out_data.shadow_model[i].tris.masks, 0);
		arrsetlen(data->vert_out_data.shadow_model[i].tris.frags, 0);
		for (int j=0; j<CUBE_FACE_CNT; j++)
			data->vert_out_data.shadow_model[i].tri_cnt[j] = 0;
	}

	data->vert_out_data.pointlight_cnt = e->pointlights.cnt;
	data->vert_out_data.shadow_cnt = e->shadowcasters.cnt;
#if 0
	for (int cube_idx=0; cube_idx<e->shadowmaps.cnt; cube_idx++) {
		for (u32 face_idx=0; face_idx<CUBE_FACE_CNT; face_idx++) {
			calc_shadowmap(data, &data->world_tris.basic, cube_idx, face_idx);
			calc_shadowmap(data, &data->world_tris_extra, cube_idx, face_idx);
		}
	}
	for (int i=0; i<e->shadowmaps.cnt; i++) {
		shadow_viewport(data->vert_out_data.shadow[i].tris, myarrlenu(data->vert_out_data.shadow[i].tris));
	}
	data->vert_shadow_time = time_diff(start, get_time());
#else

	/* Transform Pointlights */
	for (int cube_idx=0; cube_idx<e->pointlights.cnt; cube_idx++) {
		for (u32 face_idx=0; face_idx<CUBE_FACE_CNT; face_idx++) {
			cubemap_t* const cubemap = &data->e->pointlights.cubemaps[cube_idx];
			calc_shadowmap(cubemap, &data->pointlight_tris[cube_idx].world, &data->vert_out_data.pointlight_world[cube_idx], face_idx);
			calc_shadowmap(cubemap, &data->pointlight_tris[cube_idx].models, &data->vert_out_data.pointlight_model[cube_idx], face_idx);
		}
	}
	/* Transform Shadowcasters */
	for (int cube_idx=0; cube_idx<e->shadowcasters.cnt; cube_idx++) {
		for (u32 face_idx=0; face_idx<CUBE_FACE_CNT; face_idx++) {
			cubemap_occlusion_t* const cubemap = &data->e->shadowcasters.cubemaps[cube_idx];
			calc_shadowmap(&cubemap->basic, &data->shadowcaster_tris[cube_idx].world, &data->vert_out_data.shadow_world[cube_idx], face_idx);
			calc_shadowmap(&cubemap->basic, &data->shadowcaster_tris[cube_idx].models, &data->vert_out_data.shadow_model[cube_idx], face_idx);
		}
	}

	/* Viewport Pointlights */
	for (int i=0; i<e->pointlights.cnt; i++)
		shadow_viewport(&data->vert_out_data.pointlight_world[i].tris);
	for (int i=0; i<e->pointlights.cnt; i++)
		shadow_viewport(&data->vert_out_data.pointlight_model[i].tris);

	/* Viewport Shadowcasters */
	for (int i=0; i<e->shadowcasters.cnt; i++)
		shadow_viewport(&data->vert_out_data.shadow_world[i].tris);
	for (int i=0; i<e->shadowcasters.cnt; i++)
		shadow_viewport(&data->vert_out_data.shadow_model[i].tris);

	data->vert_shadow_time = time_diff(start, get_time())*1000;
#endif

	/* Normal Mapping */
	/* TODO split? */
	start = get_time();
	vert_postworld(data);
	data->vert_view_time = time_diff(start, get_time())*1000;
#ifdef VERBOSE
	myprintf("job_vert done: data:0x%lx id:%d\n", data, data->thread_id);
#endif
}
