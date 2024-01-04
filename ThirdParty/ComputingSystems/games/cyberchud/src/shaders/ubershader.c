#include <assert.h>
#include <stdio.h>

#include "shaders/ubershader.h"
#include "debug.h"
#include "shader_utils.h"
#include "utils/minmax.h"
#include "utils/myds.h"
#include "utils/mypow.h"

/*
 * PL = Pointlights
 * SC = Shadowcasters
*/

static inline float get_lightmap_value_interp(const uint8_t* px, const int w, const int h, vec2s uv) {
	assert(px);
	assert(w > 0);
	assert(h > 0);
	assert(uv.x >= 0);
	assert(uv.y >= 0);
	/* this *should* never happen, but it does, I think because of *bar multiply */
	/* assert(uv.x <= 1.0); */
	/* assert(uv.y <= 1.0); */
	if (uv.x > 1.0) uv.x = 1;
	if (uv.y > 1.0) uv.y = 1;
	const double x = (double)(w-1) * (double)uv.u;
	const double y = (double)(h-1) * (double)uv.v;
	const int x0 = x;
	const int y0 = y;

	int x1 = x0+1;
	if (x1 >= w) x1 = w-1;
	int y1 = y0+1;
	if (y1 >= h) y1 = h-1;

	const int w0 = y0*w;
	const int w1 = y1*w;
	assert(x0 >= 0);
	assert(x1 < w);
	assert(y0 >= 0);
	assert(y1 < h);

	/* Calculate the fractional part of x and y */
	const double dx = x - x0;
	const double dy = y - y0;

	const double val =
		(double)px[w0+x0] * (1.0-dx) * (1.0-dy) +
		(double)px[w0+x1] * dx * (1.0-dy) +
		(double)px[w1+x0] * (1.0-dx) * dy +
		(double)px[w1+x1] * dx * dy;

	return val/255.0;
}

#ifdef SHADER_SC
static inline float sample_cubemap_occlusion(const cubemap_occlusion_t* const cubemap, const vec3s* const direction) {
	// Normalize the direction vector
	glm_vec3_normalize((float*)direction->raw);

	// Convert the direction vector to a texture coordinate
	vec3s absDir;
	glm_vec3_abs((float*)direction->raw, absDir.raw);
	const float ma = MAX(MAX(absDir.x, absDir.y), absDir.z);

	vec2s uv;
	int idx;
	if (ma == absDir.x) {
		if (direction->x > 0.0) {
			uv = (vec2s){{-direction->z, -direction->y}};
			idx = 0;
		} else {
			uv = (vec2s){{direction->z, -direction->y}};
			idx = 1;
		}
	} else if (ma == absDir.y) {
		if (direction->y > 0.0) {
			uv = (vec2s){{direction->x, direction->z}};
			idx = 2;
		} else {
			uv = (vec2s){{direction->x, -direction->z}};
			idx = 3;
		}
	} else {
		if (direction->z > 0.0) {
			uv = (vec2s){{direction->x, -direction->y}};
			idx = 4;
		} else {
			uv = (vec2s){{-direction->x, -direction->y}};
			idx = 5;
		}
	}
	glm_vec2_divs(uv.raw, ma, uv.raw);
	glm_vec2_adds(uv.raw, 1.0, uv.raw);
	glm_vec2_scale(uv.raw, 0.5, uv.raw);

	// Sample the cubemap texture using the calculated texture coordinate
	assert(uv.x >= 0);
	assert(uv.y >= 0);
	assert(uv.x <= 1);
	assert(uv.y <= 1);
	/* this *should* never happen, but it does, I think because of *bar multiply */
	/* glm_vec2_clamp(uv.raw, 0, 1); */
	const double x = (double)(SHADOW_WIDTH-1) * (double)uv.u;
	const double y = (double)(SHADOW_HEIGHT-1) * (1.0 - (double)uv.v);
	const int x0 = x;
	const int y0 = y;
	const size_t fb_idx = y0*SHADOW_WIDTH+x0;
	if (bitarr_get(cubemap->shadowfield[idx], fb_idx)) {
		return cubemap->basic.depthbuffer[idx][fb_idx];
	}
	return FLT_MAX;
}

static inline float sub_frag_shadowcaster(const cubemap_occlusion_t* const cubemap, const int cubemap_cnt, const vec3s* const fragPos) {
	float shadow = 0.0;

	for (int ci=0; ci<cubemap_cnt; ci++) {
		vec3s fragToLight;
		glm_vec3_sub((float*)fragPos->raw, (float*)cubemap[ci].basic.pos.raw, fragToLight.raw);
		vec3s tmp_dir = glms_vec3_negate(fragToLight);
		glm_vec3_normalize(tmp_dir.raw);
		glm_vec3_divs(tmp_dir.raw, cubemap_cnt, tmp_dir.raw);
		const float currentDepth = glm_vec3_norm(fragToLight.raw);
		/* const float attenuation = cubemap[ci].basic.brightness * 2 / (currentDepth * currentDepth); */
		const float attenuation = 1.0 / (currentDepth * currentDepth);
		const float bias = 0.15;
		float closestDepth = sample_cubemap_occlusion(&cubemap[ci], &fragToLight);
		closestDepth *= cubemap[ci].basic.far_plane; // undo mapping [0;1]
		if(currentDepth + bias >= closestDepth && currentDepth - bias <= closestDepth) {
			shadow += 100.0 * attenuation;
		}
	}

	return MAX(1.0-shadow, 0);
}
#endif

static inline vec3s u8_to_vec3s(rgb_u8_t rgb) {
	vec3s out;
	out.x = (float)rgb.r * 2.0f/255.0f - 1.0f;
	out.y = (float)rgb.g * 2.0f/255.0f - 1.0f;
	out.z = (float)rgb.b * 2.0f/255.0f - 1.0f;
	const float norm = glm_vec3_norm(out.raw);
	glm_vec3_scale(out.raw, 1.0f / norm, out.raw);
	vec3_assert_norm(out.raw);
	return out;
}

static inline vec3s get_norm_value(const px_t* px, const vec2s uv) {
	if (px == NULL) {
		return (vec3s){.x=0,.y=0,.z=1};
	}
	const int w = px->w;
	int x = (float)(w) * uv.u;
	x = wrap(x, w);
	int y = (float)(px->h) * (1.0f - uv.v);
	y = wrap(y, px->h);
	assert(x >= 0);
	assert(x < w);
	assert(y >= 0);
	assert(y < px->h);
	const rgb_u8_t* const data = (const rgb_u8_t*)px->data;
	return u8_to_vec3s(data[y*w+x]);
}

static inline diffuse_t get_diffuse_value(const px_t* const px, const vec2s uv) {
	diffuse_t diffuse = {0, 1};
	if (px == NULL)
		return diffuse;
	const int w = px->w;
	int x = (float)(w) * uv.u;
	x = wrap(x, w);

	int y = (float)(px->h) * (1.0f - uv.v);
	y = wrap(y, px->h);

	assert(x >= 0);
	assert(x < w);
	assert(y >= 0);
	assert(y < px->h);
	diffuse.pal = px->data[y*w+x]&128;
	float div;
	if (!diffuse.pal) {
		div = 127.0f;
	} else {
		diffuse.pal = 1;
		div = 255.0f;
	}
	diffuse.albedo = (float)px->data[y*w+x]/div;
	return diffuse;
}

static inline float get_lux_value_interp_internal(const float a, const float b, const float c, const float d, const float dx, const float dy) {
	const float ndx = 1.0-dx;
	const float ndy = 1.0-dy;
	float val =
		a * ndx * ndy +
		b * dx * ndy +
		c * ndx * dy +
		d * dx * dy;
	return val/255.0*2.0-1.0;
}

static inline void get_lux_value_interp(const rgb_u8_t* const px, const int w, const int h, vec2s uv, vec3s* const out) {
	assert(px);
	assert(w > 0);
	assert(h > 0);
	assert(uv.x >= 0);
	assert(uv.y >= 0);
	/* this *should* never happen, but it does, I think because of *bar multiply */
	if (uv.x > 1.0) uv.x = 1;
	if (uv.y > 1.0) uv.y = 1;
	const float x = (float)(w-1) * uv.u;
	const float y = (float)(h-1) * uv.v;
	const int x0 = x;
	const int y0 = y;

	int x1 = x0+1;
	if (x1 >= w) x1 = w-1;
	int y1 = y0+1;
	if (y1 >= h) y1 = h-1;

	const int w0 = y0*w;
	const int w1 = y1*w;
	assert(x0 >= 0);
	assert(x1 < w);
	assert(y0 >= 0);
	assert(y1 < h);

	/* Calculate the fractional part of x and y */
	const float dx = x - x0;
	const float dy = y - y0;

	const rgb_u8_t px00 = px[w0+x0];
	const rgb_u8_t px01 = px[w0+x1];
	const rgb_u8_t px10 = px[w1+x0];
	const rgb_u8_t px11 = px[w1+x1];
	out->x = get_lux_value_interp_internal(px00.r, px01.r, px10.r, px11.r, dx, dy);
	out->y = get_lux_value_interp_internal(px00.g, px01.g, px10.g, px11.g, dx, dy);
	out->z = get_lux_value_interp_internal(px00.b, px01.b, px10.b, px11.b, dx, dy);
}

#ifdef SHADER_PL
static inline float bilinear_interp_inner(const double a, const double b, const double c, const double d, const double dx, const double dy) {
	double val =
		a * (1.0-dx) * (1.0-dy) +
		b * dx * (1.0-dy) +
		c * (1.0-dx) * dy +
		d * dx * dy;
	return val;
}

static inline float bilinear_interp(const float* const px, int w, int h, vec2s uv) {
	assert(px);
	assert(w > 0);
	assert(h > 0);
	assert(uv.x >= 0);
	assert(uv.y >= 0);
	assert(uv.x <= 1);
	assert(uv.y <= 1);
	/* this *should* never happen, but it does, I think because of *bar multiply */
	/* glm_vec2_clamp(uv.raw, 0, 1); */
	const double x = (double)(w-1) * (double)uv.u;
	const double y = (double)(h-1) * (1.0 - (double)uv.v);
	const int x0 = x;
	const int y0 = y;
	/* return px[y0*SHADOW_WIDTH+x0]; */

	int x1 = x0+1;
	if (x1 >= w) x1 = w-1;
	int y1 = y0+1;
	if (y1 >= h) y1 = h-1;

	const int w0 = y0*w;
	const int w1 = y1*w;
	assert(x0 >= 0);
	assert(x1 < w);
	assert(y0 >= 0);
	assert(y1 < h);

	/* Calculate the fractional part of x and y */
	const double dx = x - x0;
	const double dy = y - y0;

	return bilinear_interp_inner(px[w0+x0], px[w0+x1], px[w1+x0], px[w1+x1], dx, dy);
}

static inline float sample_cubemap(const cubemap_t* const cubemap, const vec3s* const direction) {
	// Normalize the direction vector
	glm_vec3_normalize((float*)direction->raw);

	// Convert the direction vector to a texture coordinate
	vec3s absDir;
	glm_vec3_abs((float*)direction->raw, absDir.raw);
	const float ma = MAX(MAX(absDir.x, absDir.y), absDir.z);

	vec2s uv;
	int idx;
	if (ma == absDir.x) {
		if (direction->x > 0.0) {
			uv = (vec2s){{-direction->z, -direction->y}};
			idx = 0;
		} else {
			uv = (vec2s){{direction->z, -direction->y}};
			idx = 1;
		}
	} else if (ma == absDir.y) {
		if (direction->y > 0.0) {
			uv = (vec2s){{direction->x, direction->z}};
			idx = 2;
		} else {
			uv = (vec2s){{direction->x, -direction->z}};
			idx = 3;
		}
	} else {
		if (direction->z > 0.0) {
			uv = (vec2s){{direction->x, -direction->y}};
			idx = 4;
		} else {
			uv = (vec2s){{-direction->x, -direction->y}};
			idx = 5;
		}
	}
	glm_vec2_divs(uv.raw, ma, uv.raw);
	glm_vec2_adds(uv.raw, 1.0, uv.raw);
	glm_vec2_scale(uv.raw, 0.5, uv.raw);

	// Sample the cubemap texture using the calculated texture coordinate
	return bilinear_interp(cubemap->depthbuffer[idx], SHADOW_WIDTH, SHADOW_HEIGHT, uv);
}

/* TODO holy shit reduce parameters lmao */
static inline float sub_frag_pointlight(const cubemap_t* const cubemap, const int cubemap_cnt, const shader_t* const shader, const vec3s* const bar, const vec3s* const fragPos, const vec3s* const tfragpos, const vec3s* norm_val, const vec3s* const viewDir) {
	float shadow = 0.0;
	for (int ci=0; ci<cubemap_cnt; ci++) {
		vec3s fragToLight;
		glm_vec3_sub((float*)fragPos->raw, (float*)cubemap[ci].pos.raw, fragToLight.raw);
		const float currentDepth = glm_vec3_norm(fragToLight.raw);
		const float attenuation = cubemap[ci].brightness / (currentDepth * currentDepth);
		const float bias = 0.15;
		float closestDepth = sample_cubemap(&cubemap[ci], &fragToLight);
		closestDepth *= cubemap[ci].far_plane; // undo mapping [0;1]
		if (currentDepth - bias <= closestDepth) {
			vec3s light_dir;
			glm_mat3_mulv((vec3*)shader->tlightpos.mtx[ci].raw, (float*)bar->raw, light_dir.raw);
			glm_vec3_sub(light_dir.raw, (float*)tfragpos->raw, light_dir.raw);
			glm_vec3_normalize(light_dir.raw);

			glm_vec3_add(light_dir.raw, (float*)viewDir->raw, light_dir.raw); // halfway_dir
			glm_vec3_normalize(light_dir.raw);

			const float spec_dot = glm_vec3_dot(norm_val->raw, light_dir.raw);
			float spec = 0;
			if (spec_dot > 0.0f && spec_dot <= 1.0f) {
				vec1_assert_norm(spec_dot);
				spec = mypowf(spec_dot, 32.0f) * 0.2f;
				vec1_assert(spec);
			}

			shadow += (5.0f) * attenuation + spec;
		}
	}

	return shadow;
}
#else
static inline float sub_frag_pointlight_nopl(const cubemap_t* const cubemap, const int cubemap_cnt, const shader_t* const shader, const vec3s* const bar, const vec3s* const fragPos, const vec3s* const tfragpos, const vec3s* norm_val, const vec3s* const viewDir) {
	float shadow = 0.0;
	for (int ci=0; ci<cubemap_cnt; ci++) {
		vec3s fragToLight;
		glm_vec3_sub((float*)fragPos->raw, (float*)cubemap[ci].pos.raw, fragToLight.raw);
		const float dist = MAX(glm_vec3_norm(fragToLight.raw), 1.0f);
		const float attenuation = cubemap[ci].brightness / (dist * dist);

		vec3s light_dir;
		glm_mat3_mulv((vec3*)shader->tlightpos.mtx[ci].raw, (float*)bar->raw, light_dir.raw);
		glm_vec3_sub(light_dir.raw, (float*)tfragpos->raw, light_dir.raw);
		glm_vec3_normalize(light_dir.raw);

		glm_vec3_add(light_dir.raw, (float*)viewDir->raw, light_dir.raw); // halfway_dir
		glm_vec3_normalize(light_dir.raw);

		const float spec_dot = glm_vec3_dot(norm_val->raw, light_dir.raw);
		float spec = 0;
		if (spec_dot > 0.0f && spec_dot <= 1.0f) {
			vec1_assert_norm(spec_dot);
			spec = mypowf(spec_dot, 32.0f) * attenuation;
			vec1_assert(spec);
		}

		shadow += (5.0f) * attenuation + spec;
	}

	return shadow;
}
#endif

static inline vec3s blend_norm(vec3s c1, vec3s c2, float t) {
	vec3s out;
	out.r = c1.r * (1.0f - t) + c2.r * t;
	out.g = c1.g * (1.0f - t) + c2.g * t;
	out.b = c1.b * (1.0f - t) + c2.b * t;
	glm_vec3_normalize(out.raw);
	return out;
}

static inline void
#ifdef SHADER_NAME_DEFAULT
frag_lightmap_noise
#endif
#ifdef SHADER_NAME_NO_PL
frag_lightmap_noise_no_pl
#endif
#ifdef SHADER_NAME_NO_SC
frag_lightmap_noise_no_sc
#endif
#ifdef SHADER_NAME_NO_PLSC
frag_lightmap_noise_no_plsc
#endif
(const frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, uint8_t* frag_out) {
	/* Interpolate Verts */
	vec3s worldpos, tfragpos, tviewpos;
	glm_mat3_mulv((vec3*)shader->world_pos.raw, (float*)bar->raw, worldpos.raw);
	glm_mat3_mulv((vec3*)shader->tpos.raw, (float*)bar->raw, tfragpos.raw);
	glm_mat3_mulv((vec3*)shader->tviewpos.raw, (float*)bar->raw, tviewpos.raw);
	vec3s viewDir;
	glm_vec3_sub(tviewpos.raw, tfragpos.raw, viewDir.raw);
	if (!viewDir.x && !viewDir.y && !viewDir.z) {
		/* fputs("[frag_lightmap] viewDir this should never happen\n", stderr); */
		/* assert(0); */
		/* assert(viewDir.x || viewDir.y || viewDir.z); */
		return;
	}
	glm_vec3_normalize(viewDir.raw);

	vec2s uv;
	const uv_t* const varying_uv = &shader->uv;
	for (int i=0; i<2; i++) {
		uv.raw[i] = varying_uv->uv[0].raw[i] * bar->raw[0];
		uv.raw[i] += varying_uv->uv[1].raw[i] * bar->raw[1];
		uv.raw[i] += varying_uv->uv[2].raw[i] * bar->raw[2];
	}

	vec3s norm_map_val = get_norm_value(shader->tex_normal, uv);
	const float offs = uni->scene_time;
	const size_t noise_nmap_idx = ((size_t)((worldpos.x+worldpos.y+offs)*64)%uni->noise_nmap->w) + ((size_t)((worldpos.y+worldpos.z+offs)*64)%uni->noise_nmap->h) * uni->noise_nmap->w;
	const vec3s c1 = u8_to_vec3s(((rgb_u8_t*)uni->noise_nmap->data)[noise_nmap_idx]);
	/* norm_map_val = blend_norm(norm_map_val, c1, (sinf(uni->scene_time*M_PI)+1.0f)*0.5f*0.25f); */
	norm_map_val = blend_norm(norm_map_val, c1, 0.10f);
	vec3_assert(norm_map_val.raw);

	/* Get Albedo Value */
	diffuse_t albedo = get_diffuse_value(shader->tex_diffuse, uv);
	const size_t noise_diff_idx = ((size_t)((worldpos.x+worldpos.y+offs)*64)%uni->noise->w) + ((size_t)((worldpos.y+worldpos.z+offs)*64)%uni->noise->h) * uni->noise->w;
	albedo.albedo -= uni->noise->data[noise_diff_idx]/255.0f;
	albedo.albedo = MIN(albedo.albedo, 1.0f);

	float lval = 0;
	float color = 0;
	float spec = 0;
	vec3s light_dir = {{0,0,0}};

	if (shader->litdata.lightmap) {
		vec2s uv_light;
		const uv_t* const varying_uv_light = &shader->uv_light;
#ifndef NDEBUG
		for (int j=0; j<3; j++) {
			assert(varying_uv_light->uv[j].u >= 0);
			assert(varying_uv_light->uv[j].u <= 1.0);
			assert(varying_uv_light->uv[j].v >= 0);
			assert(varying_uv_light->uv[j].v <= 1.0);
		}
#endif

		for (int i=0; i<2; i++) {
			uv_light.raw[i] = varying_uv_light->uv[0].raw[i] * bar->raw[0];
			uv_light.raw[i] += varying_uv_light->uv[1].raw[i] * bar->raw[1];
			uv_light.raw[i] += varying_uv_light->uv[2].raw[i] * bar->raw[2];
		}
		lval = get_lightmap_value_interp(shader->litdata.lightmap, shader->litdata.w, shader->litdata.h, uv_light);
		assert(!isinf(lval));
		assert(!isnanf(lval));

		assert(shader->litdata.luxmap);
		get_lux_value_interp(shader->litdata.luxmap, shader->litdata.w, shader->litdata.h, uv_light, &light_dir);

		glm_vec3_add(light_dir.raw, viewDir.raw, light_dir.raw); // halfway_dir
		glm_vec3_normalize(light_dir.raw);
		vec3_assert(light_dir.raw);
		/* Calculate Specular */
		const float spec_dot = glm_vec3_dot(norm_map_val.raw, light_dir.raw);
		if (spec_dot > 0.0f && spec_dot <= 1.0f) {
			vec1_assert_norm(spec_dot);
			spec = mypowf(spec_dot, 128.0f)*lval*0.1f;
			vec1_assert(spec);
		}
	}

#ifdef SHADER_SC
	const float shadow = sub_frag_shadowcaster(uni->shadowcasters, uni->shadowcaster_cnt, &worldpos);
#endif

#ifdef SHADER_PL
	lval += sub_frag_pointlight(uni->pointlights, uni->pointlight_cnt, shader, bar, &worldpos, &tfragpos, &viewDir, &norm_map_val);
#else
	lval += sub_frag_pointlight_nopl(uni->pointlights, uni->pointlight_cnt, shader, bar, &worldpos, &tfragpos, &viewDir, &norm_map_val);
#endif

	/* Simple Pontlights */
	for (int i=0; i<uni->simplelight_cnt; i++) {
		const float dist = MAX(glm_vec3_distance(worldpos.raw, (float*)uni->simplelights[i].raw), 1.0f);
		const float sattenuation = uni->simplelights[i].w / dist;
		lval += sattenuation;
	}

	if (lval > 1.0) lval = 1.0;

#ifdef SHADER_SC
	color = (lval + spec) * albedo.albedo * shadow;
#else
	color = (lval + spec) * albedo.albedo;
#endif
	if (color < 0) {
		color = 0;
	} else {
		color = mypowf(color, 1.0f/2.2f);
	}

	vec1_assert(color); // TODO investigate why norm_map sometimes results in NaN
	if (color > 1.0) color = 1.0f;

	*frag_out = frag_calc_color(color, albedo.pal);
}

static inline void
#ifdef SHADER_NAME_DEFAULT
frag_lightmap
#endif
#ifdef SHADER_NAME_NO_PL
frag_lightmap_no_pl
#endif
#ifdef SHADER_NAME_NO_SC
frag_lightmap_no_sc
#endif
#ifdef SHADER_NAME_NO_PLSC
frag_lightmap_no_plsc
#endif
(const frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, uint8_t* frag_out) {
	/* Interpolate Verts */
	vec3s worldpos, tfragpos, tviewpos;
	glm_mat3_mulv((vec3*)shader->world_pos.raw, (float*)bar->raw, worldpos.raw);
	glm_mat3_mulv((vec3*)shader->tpos.raw, (float*)bar->raw, tfragpos.raw);
	glm_mat3_mulv((vec3*)shader->tviewpos.raw, (float*)bar->raw, tviewpos.raw);
	vec3s viewDir;
	glm_vec3_sub(tviewpos.raw, tfragpos.raw, viewDir.raw);
	if (!viewDir.x && !viewDir.y && !viewDir.z) {
		/* fputs("[frag_lightmap] viewDir this should never happen\n", stderr); */
		/* assert(0); */
		/* assert(viewDir.x || viewDir.y || viewDir.z); */
		return;
	}
	glm_vec3_normalize(viewDir.raw);

	vec2s uv;
	const uv_t* const varying_uv = &shader->uv;
	for (int i=0; i<2; i++) {
		uv.raw[i] = varying_uv->uv[0].raw[i] * bar->raw[0];
		uv.raw[i] += varying_uv->uv[1].raw[i] * bar->raw[1];
		uv.raw[i] += varying_uv->uv[2].raw[i] * bar->raw[2];
	}

	vec3s norm_map_val = get_norm_value(shader->tex_normal, uv);
	vec3_assert(norm_map_val.raw);

	/* Get Albedo Value */
	const diffuse_t albedo = get_diffuse_value(shader->tex_diffuse, uv);

	float lval = 0;
	float color = 0;
	float spec = 0;
	vec3s light_dir = {{0,0,0}};

	if (shader->litdata.lightmap) {
		vec2s uv_light;
		const uv_t* const varying_uv_light = &shader->uv_light;
#ifndef NDEBUG
		for (int j=0; j<3; j++) {
			assert(varying_uv_light->uv[j].u >= 0);
			assert(varying_uv_light->uv[j].u <= 1.0);
			assert(varying_uv_light->uv[j].v >= 0);
			assert(varying_uv_light->uv[j].v <= 1.0);
		}
#endif

		for (int i=0; i<2; i++) {
			uv_light.raw[i] = varying_uv_light->uv[0].raw[i] * bar->raw[0];
			uv_light.raw[i] += varying_uv_light->uv[1].raw[i] * bar->raw[1];
			uv_light.raw[i] += varying_uv_light->uv[2].raw[i] * bar->raw[2];
		}
		lval = get_lightmap_value_interp(shader->litdata.lightmap, shader->litdata.w, shader->litdata.h, uv_light);
		assert(!isinf(lval));
		assert(!isnanf(lval));

		assert(shader->litdata.luxmap);
		get_lux_value_interp(shader->litdata.luxmap, shader->litdata.w, shader->litdata.h, uv_light, &light_dir);

		glm_vec3_add(light_dir.raw, viewDir.raw, light_dir.raw); // halfway_dir
		glm_vec3_normalize(light_dir.raw);
		vec3_assert(light_dir.raw);
		/* Calculate Specular */
		const float spec_dot = glm_vec3_dot(norm_map_val.raw, light_dir.raw);
		if (spec_dot > 0.0f && spec_dot <= 1.0f) {
			vec1_assert_norm(spec_dot);
			spec = mypowf(spec_dot, 128.0f)*lval*0.1f;
			vec1_assert(spec);
		}
	}

#ifdef SHADER_SC
	const float shadow = sub_frag_shadowcaster(uni->shadowcasters, uni->shadowcaster_cnt, &worldpos);
#endif

#ifdef SHADER_PL
	lval += sub_frag_pointlight(uni->pointlights, uni->pointlight_cnt, shader, bar, &worldpos, &tfragpos, &viewDir, &norm_map_val);
#else
	lval += sub_frag_pointlight_nopl(uni->pointlights, uni->pointlight_cnt, shader, bar, &worldpos, &tfragpos, &viewDir, &norm_map_val);
#endif

	/* Simple Pontlights */
	for (int i=0; i<uni->simplelight_cnt; i++) {
		const float dist = MAX(glm_vec3_distance(worldpos.raw, (float*)uni->simplelights[i].raw), 1.0f);
		const float sattenuation = uni->simplelights[i].w / dist;
		lval += sattenuation;
	}

	if (lval > 1.0) lval = 1.0;

#ifdef SHADER_SC
	color = (lval + spec) * albedo.albedo * shadow;
#else
	color = (lval + spec) * albedo.albedo;
#endif
	if (color < 0) {
		color = 0;
	} else {
		color = mypowf(color, 1.0f/2.2f);
	}

	vec1_assert(color); // TODO investigate why norm_map sometimes results in NaN
	if (color > 1.0) color = 1.0f;

	*frag_out = frag_calc_color(color, albedo.pal);
}

static inline void frag_emitter(const frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, uint8_t* frag_out) {
	/* Interpolate Verts */
	vec3s worldpos, tfragpos, tviewpos;
	glm_mat3_mulv((vec3*)shader->world_pos.raw, (float*)bar->raw, worldpos.raw);
	glm_mat3_mulv((vec3*)shader->tpos.raw, (float*)bar->raw, tfragpos.raw);
	glm_mat3_mulv((vec3*)shader->tviewpos.raw, (float*)bar->raw, tviewpos.raw);
	vec3s viewDir;
	glm_vec3_sub(tviewpos.raw, tfragpos.raw, viewDir.raw);
	if (!viewDir.x && !viewDir.y && !viewDir.z) {
		/* fputs("[frag_lightmap] viewDir this should never happen\n", stderr); */
		/* assert(0); */
		/* assert(viewDir.x || viewDir.y || viewDir.z); */
		return;
	}
	glm_vec3_normalize(viewDir.raw);

	vec2s uv;
	const uv_t* const varying_uv = &shader->uv;
	for (int i=0; i<2; i++) {
		uv.raw[i] = varying_uv->uv[0].raw[i] * bar->raw[0];
		uv.raw[i] += varying_uv->uv[1].raw[i] * bar->raw[1];
		uv.raw[i] += varying_uv->uv[2].raw[i] * bar->raw[2];
	}

	vec3s norm_map_val = get_norm_value(shader->tex_normal, uv);
	vec3_assert(norm_map_val.raw);

	/* Get Albedo Value */
	const diffuse_t albedo = get_diffuse_value(shader->tex_diffuse, uv);

	float spec = 0;
	float color = 0;

	/* Calculate Specular */
	const float spec_dot = glm_vec3_dot(norm_map_val.raw, viewDir.raw);
	if (spec_dot > 0.0f && spec_dot <= 1.0f) {
		vec1_assert_norm(spec_dot);
		spec = mypowf(spec_dot, 64.0f);
		vec1_assert(spec);
	}

	color = (1.0f + spec) * albedo.albedo;
	if (color < 0) {
		color = 0;
	} else {
		color = mypowf(color, 1.0f/2.2f);
	}

	vec1_assert(color); // TODO investigate why norm_map sometimes results in NaN
	if (color > 1.0) color = 1.0f;

	*frag_out = frag_calc_color(color, albedo.pal);
}

void
#ifdef SHADER_NAME_DEFAULT
triangle_world
#endif
#ifdef SHADER_NAME_NO_PL
triangle_world_no_pl
#endif
#ifdef SHADER_NAME_NO_SC
triangle_world_no_sc
#endif
#ifdef SHADER_NAME_NO_PLSC
triangle_world_no_plsc
#endif
(const frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data) {
	u8* const fb = uni->fb;
	float* const db = uni->db;
	const i16 uni_y1 = uni->mask_y1;
	const i16 uni_y2 = uni->mask_y2;
	const shader_t* const shaders = frag_in_data->frags;
	const rect_i16* const masks = frag_in_data->masks;
	const size_t tri_cnt = stbds_header(shaders)->length;
	const shader_t* shader = shaders;
	const rect_i16* mask = masks;
	for (size_t ii=0; ii<tri_cnt; ii++, mask++, shader++) {
		i16 y1 = mask->y1;
		i16 y2 = mask->y2;
		assert(y1 >= 0);
		assert(y2 <= SCREEN_H);
		if (uni_y1 > y2 || uni_y2 <= y1)
			continue;

		/* we dont't check x1/x2 because we always do full width */
		int start_y = MAX(y1, uni_y1);
		start_y += (start_y+uni->interlace)%2;
		const int end_y = MIN(y2, uni_y2);

		const int start_x = mask->x1;
		const int max_x = mask->x2;
		assert(start_x >= 0);
		assert(max_x <= SCREEN_W);

		void(*frag_shader)(const frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, u8* frag_out);
		switch (shader->shader_idx) {
			case SHADER_WORLD_EMITTER:
				frag_shader = frag_emitter;
				break;
			case SHADER_WORLD_NOISE:
#ifdef SHADER_NAME_DEFAULT
				frag_shader = frag_lightmap_noise;
#endif
#ifdef SHADER_NAME_NO_PL
				frag_shader = frag_lightmap_noise_no_pl;
#endif
#ifdef SHADER_NAME_NO_SC
				frag_shader = frag_lightmap_noise_no_sc;
#endif
#ifdef SHADER_NAME_NO_PLSC
				frag_shader = frag_lightmap_noise_no_plsc;
#endif
				break;
			case SHADER_WORLD_DEFAULT:
			default:
#ifdef SHADER_NAME_DEFAULT
				frag_shader = frag_lightmap;
#endif
#ifdef SHADER_NAME_NO_PL
				frag_shader = frag_lightmap_no_pl;
#endif
#ifdef SHADER_NAME_NO_SC
				frag_shader = frag_lightmap_no_sc;
#endif
#ifdef SHADER_NAME_NO_PLSC
				frag_shader = frag_lightmap_no_plsc;
#endif
		}

		const vec4s* const vert = shader->vert_out.vert;
		for (int y=start_y; y<end_y; y+=2) {
			for (int x=start_x; x<max_x; x++) {
				vec3s bc_screen;
				if (barycentric(shader->pts2, x, y, &bc_screen))
					continue;
				if (bc_screen.x<0.0 || bc_screen.y<0.0 || bc_screen.z<0.0) {
					continue; // 0.0 is causing NaNs
				}
				vec3s bc_clip = (vec3s){.x=bc_screen.x/shader->pts[0].w, .y=bc_screen.y/shader->pts[1].w, .z=bc_screen.z/shader->pts[2].w};
				const float bc_clip_sum = bc_clip.x+bc_clip.y+bc_clip.z;
				/* https://github.com/ssloy/tinyrenderer/wiki/Technical-difficulties-linear-interpolation-with-perspective-deformations */
				bc_clip.x /= bc_clip_sum;
				bc_clip.y /= bc_clip_sum;
				bc_clip.z /= bc_clip_sum;

				float frag_depth = 0;
				for (int i=0; i<3; i++)
					frag_depth += vert[i].z * bc_clip.raw[i];

				if (frag_depth > db[x+y*SCREEN_W]) continue;
				db[y*SCREEN_W+x] = frag_depth;

				uint8_t color = 0;
				frag_shader(uni, shader, &bc_clip, &color);
				fb[y*SCREEN_W+x] = color;
			}
		}
	}
}

static inline int
#ifdef SHADER_NAME_DEFAULT
frag_norm
#endif
#ifdef SHADER_NAME_NO_PL
frag_norm_no_pl
#endif
#ifdef SHADER_NAME_NO_SC
frag_norm_no_sc
#endif
#ifdef SHADER_NAME_NO_PLSC
frag_norm_no_plsc
#endif
(frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, uint8_t* frag_out) {
	/* Interpolate Verts */
	vec3s worldpos, tfragpos, tviewpos;
	glm_mat3_mulv((vec3*)shader->world_pos.raw, (float*)bar->raw, worldpos.raw);
	glm_mat3_mulv((vec3*)shader->tpos.raw, (float*)bar->raw, tfragpos.raw);
	glm_mat3_mulv((vec3*)shader->tviewpos.raw, (float*)bar->raw, tviewpos.raw);
	vec3s viewDir = glms_vec3_sub(tviewpos, tfragpos);
	if (!viewDir.x && !viewDir.y && !viewDir.z) {
#ifndef NDEBUG
		fputs("[frag_norm] viewDir this should never happen\n", stderr);
#endif
		/* assert(0); */
		/* assert(viewDir.x || viewDir.y || viewDir.z); */
		return 0;
	}
	glm_vec3_normalize(viewDir.raw);

	/* Get UV Coords */
	vec2s uv;
	const uv_t* const varying_uv = &shader->uv;
	for (int i=0; i<2; i++) {
		uv.raw[i] = varying_uv->uv[0].raw[i] * bar->raw[0];
		uv.raw[i] += varying_uv->uv[1].raw[i] * bar->raw[1];
		uv.raw[i] += varying_uv->uv[2].raw[i] * bar->raw[2];
	}

	/* Get Normal Value */
	vec3s norm_map_val = get_norm_value(shader->tex_normal, uv);
	vec3_assert_norm(norm_map_val.raw);

	/* Get Albedo Value */
	const diffuse_t albedo = get_diffuse_value(shader->tex_diffuse, uv);

	/* Calculate Ambient */
	float total_diff = shader->ambient;
	for (int i=0; i<uni->shadowcaster_cnt; i++) {
		vec3s light_dir;
		glm_mat3_mulv((vec3*)shader->tshadowpos.mtx[i].raw, (float*)bar->raw, light_dir.raw);
		glm_vec3_sub(light_dir.raw, tfragpos.raw, light_dir.raw);
		glm_vec3_normalize(light_dir.raw);
		vec3_assert_norm(light_dir.raw);

		vec3s worldToLight;
		glm_vec3_sub(worldpos.raw, (float*)uni->shadowcasters[i].basic.pos.raw, worldToLight.raw);
		const float light_dist = MAX(glm_vec3_norm(worldToLight.raw), 1.0f);
		const float attenuation = uni->shadowcasters[i].basic.brightness / (light_dist * light_dist);

		assert(attenuation >= 0);

		/* const float shadow = ShadowCalculation(&uni->cubemaps[0], &light_dir, &viewDir); */
		float norm_light_dot = glm_vec3_dot(light_dir.raw, norm_map_val.raw);
		if (norm_light_dot < 0)
			norm_light_dot = 1.0f;
		else if (norm_light_dot > 1.0f)
			norm_light_dot = 1.0f;
		vec1_assert_norm(norm_light_dot);
		const float diff = norm_light_dot*attenuation;

		/* Calculate Halfway Dir */
		glm_vec3_add(light_dir.raw, viewDir.raw, light_dir.raw);
		glm_vec3_normalize(light_dir.raw);
		vec3_assert(light_dir.raw);

		/* Calculate Specular */
		const float spec_dot = glm_vec3_dot(norm_map_val.raw, light_dir.raw);
		float spec = 0;
		if (spec_dot > 0.0f && spec_dot <= 1.0f) {
			vec1_assert_norm(spec_dot);
			spec = mypowf(spec_dot, 64.0f) * attenuation;
			vec1_assert(spec);
		}

		/* Gamma */
		float val = (diff + spec)*albedo.albedo;
		val = mypowf(val, 1.0f/2.2f);
		total_diff += val;
	}

	for (int i=0; i<uni->pointlight_cnt; i++) {
		vec3s light_dir;
		glm_mat3_mulv((vec3*)shader->tlightpos.mtx[i].raw, (float*)bar->raw, light_dir.raw);
		glm_vec3_sub(light_dir.raw, tfragpos.raw, light_dir.raw);
		glm_vec3_normalize(light_dir.raw);
		vec3_assert_norm(light_dir.raw);

		vec3s worldToLight;
		glm_vec3_sub(worldpos.raw, (float*)uni->pointlights[i].pos.raw, worldToLight.raw);
		const float light_dist = glm_vec3_norm(worldToLight.raw);

		assert(light_dist >= 0);
		vec1_assert(light_dist);
		const float attenuation = uni->pointlights[i].brightness / (light_dist * light_dist);
		assert(attenuation >= 0);

		float norm_light_dot = glm_vec3_dot(light_dir.raw, norm_map_val.raw);
		if (norm_light_dot < 0)
			norm_light_dot = 0.0f;
		else if (norm_light_dot > 1.0f)
			norm_light_dot = 1.0f;
		vec1_assert_norm(norm_light_dot);
		const float diff = norm_light_dot*attenuation;

		/* Calculate Halfway Dir */
		glm_vec3_add(light_dir.raw, viewDir.raw, light_dir.raw);
		glm_vec3_normalize(light_dir.raw);
		vec3_assert(light_dir.raw);

		/* Calculate Specular */
		const float spec_dot = glm_vec3_dot(norm_map_val.raw, light_dir.raw);
		float spec = 0;
		if (spec_dot > 0.0f && spec_dot <= 1.0f) {
			vec1_assert_norm(spec_dot);
			spec = mypowf(spec_dot, 64.0f) * attenuation;
			vec1_assert(spec);
		}

		/* Gamma */
		float val = (diff + spec + attenuation)*albedo.albedo;
		val = mypowf(val, 1.0f/2.2f);
		total_diff += val;
	}

	/* Simple Pontlights */
	float simple_val = 0;
	for (int i=0; i<uni->simplelight_cnt; i++) {
		const float dist = MAX(glm_vec3_distance(worldpos.raw, (float*)uni->simplelights[i].raw), 1.0f);
		const float sattenuation = uni->simplelights[i].w / (dist);
		simple_val += sattenuation;
	}
	simple_val = MIN(simple_val, 1.0);
	total_diff += simple_val * albedo.albedo;

	if (total_diff > 1.0) total_diff = 1;
	*frag_out = frag_calc_color(total_diff, albedo.pal+shader->color_mod);
	return 1;
}

static inline int frag_beam(frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, uint8_t* frag_out) {
	if (!(xorshift64(&uni->seed)&3))
		return 0;

	/* Get UV Coords */
	vec2s uv;
	const uv_t* const varying_uv = &shader->uv;

	for (int i=0; i<2; i++) {
		uv.raw[i] = varying_uv->uv[0].raw[i] * bar->raw[0];
		uv.raw[i] += varying_uv->uv[1].raw[i] * bar->raw[1];
		uv.raw[i] += varying_uv->uv[2].raw[i] * bar->raw[2];
	}

#if 1
	*frag_out = (fabsf((fmodf(uv.y - uni->scene_time*4, 1.0f)))*512);
#else
	const float offs = uni->scene_time*3;
	const size_t noise_nmap_idx = ((size_t)((uv.x-offs)*64)%uni->noise_nmap->w) + ((size_t)((uv.y-offs)*64)%uni->noise_nmap->h) * uni->noise_nmap->w;
	const vec3s c1 = u8_to_vec3s(((rgb_u8_t*)uni->noise_nmap->data)[noise_nmap_idx]);
	*frag_out = c1.x*15;
#endif
	return 1;
}

void
#ifdef SHADER_NAME_DEFAULT
triangle_sub
#endif
#ifdef SHADER_NAME_NO_PL
triangle_sub_no_pl
#endif
#ifdef SHADER_NAME_NO_SC
triangle_sub_no_sc
#endif
#ifdef SHADER_NAME_NO_PLSC
triangle_sub_no_plsc
#endif
(frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data) {
	u8* const fb = uni->fb;
	float* const db = uni->db;
	const i16 uni_y1 = uni->mask_y1;
	const i16 uni_y2 = uni->mask_y2;
	const shader_t* const shaders = frag_in_data->frags;
	const rect_i16* const masks = frag_in_data->masks;
	const size_t tri_cnt = stbds_header(shaders)->length;
	const shader_t* shader = shaders;
	const rect_i16* mask = masks;
	for (size_t ii=0; ii<tri_cnt; ii++, mask++, shader++) {
		i16 y1 = mask->y1;
		i16 y2 = mask->y2;
		assert(y1 >= 0);
		assert(y2 <= SCREEN_H);
		if (uni_y1 > y2 || uni_y2 <= y1)
			continue;

		/* we dont't check x1/x2 because we always do full width */
		int start_y = MAX(y1, uni_y1);
		start_y += (start_y+uni->interlace)%2;
		const int end_y = MIN(y2, uni_y2);

		int(*frag_shader)(frag_uniform_t* const uni, const shader_t* shader, const vec3s* const bar, uint8_t* frag_out);
		switch (shader->shader_idx) {
			case 1:
			case 2:
			case 3:
				frag_shader = frag_beam;
				break;
			case 0:
			default:
#ifdef SHADER_NAME_DEFAULT
				frag_shader = frag_norm;
#endif
#ifdef SHADER_NAME_NO_PL
				frag_shader = frag_norm_no_pl;
#endif
#ifdef SHADER_NAME_NO_SC
				frag_shader = frag_norm_no_sc;
#endif
#ifdef SHADER_NAME_NO_PLSC
				frag_shader = frag_norm_no_plsc;
#endif
		}

		const int start_x = mask->x1;
		const int max_x = mask->x2;
		assert(start_x >= 0);
		assert(max_x <= SCREEN_W);
		const vec4s* const vert = shader->vert_out.vert;
		for (int y=start_y; y<end_y; y+=2) {
			for (int x=start_x; x<max_x; x++) {
				vec3s bc_screen;
				if (barycentric(shader->pts2, x, y, &bc_screen))
					continue;
				if (bc_screen.x<0.0 || bc_screen.y<0.0 || bc_screen.z<0.0) {
					continue; // 0.0 is causing NaNs
				}
				vec3s bc_clip = (vec3s){.x=bc_screen.x/shader->pts[0].w, .y=bc_screen.y/shader->pts[1].w, .z=bc_screen.z/shader->pts[2].w};
				const float bc_clip_sum = bc_clip.x+bc_clip.y+bc_clip.z;
				/* https://github.com/ssloy/tinyrenderer/wiki/Technical-difficulties-linear-interpolation-with-perspective-deformations */
				bc_clip.x /= bc_clip_sum;
				bc_clip.y /= bc_clip_sum;
				bc_clip.z /= bc_clip_sum;

				float frag_depth = 0;
				for (int i=0; i<3; i++)
					frag_depth += vert[i].z * bc_clip.raw[i];

				if (frag_depth > db[x+y*SCREEN_W]) continue;
				uint8_t color = 0;
				if (frag_shader(uni, shader, &bc_clip, &color)) {
					fb[y*SCREEN_W+x] = color;
					db[y*SCREEN_W+x] = frag_depth;
				}
			}
		}
	}
}
