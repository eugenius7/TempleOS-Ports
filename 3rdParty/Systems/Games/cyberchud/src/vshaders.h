#ifndef VSHADERS_H
#define VSHADERS_H

#include "model.h"
#include "engine.h"

typedef struct {
	mat4s mtx;
	vec4s warpcircle;
	anim_t anim;
} vert_uniform_t;

void job_vert_basic(thread_data_t* data);
void job_vert_basic_no_pl(thread_data_t* data);
void job_vert_basic_no_sc(thread_data_t* data);
void job_vert_basic_no_plsc(thread_data_t* data);

#endif
