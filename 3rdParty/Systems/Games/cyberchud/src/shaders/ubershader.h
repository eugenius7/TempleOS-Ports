#ifndef SHADERS_UBERSHADER_H
#define SHADERS_UBERSHADER_H

#include "shader.h"
#include "engine.h"

void draw_frags(const engine_t* const e, frag_uniform_t* const uni, thread_vert_out_t* const frag_in_data);

void triangle_world(const frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);
void triangle_world_no_pl(const frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);
void triangle_world_no_sc(const frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);
void triangle_world_no_plsc(const frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);

void triangle_sub(frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);
void triangle_sub_no_pl(frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);
void triangle_sub_no_sc(frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);
void triangle_sub_no_plsc(frag_uniform_t* const uni, const shader_bundle_t* const frag_in_data);

#endif
