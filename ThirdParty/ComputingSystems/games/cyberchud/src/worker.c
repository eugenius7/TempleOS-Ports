#include <stdio.h>
#include <assert.h>

#include "worker.h"
#include "mytime.h"
#include "text.h"
#include "shaders/ubershader.h"
#include "vshaders.h"
#include "shadow.h"
#include "utils/myds.h"

/* #define WORKER_VERBOSE */

static void calc_bone(anim_t* anim_data, const model_anim_data_t* const model_data, const uint32_t idx, const mat4s parent_mtx) {
	const mat4s global_mtx = glms_mat4_mul(parent_mtx, anim_data->bone_mtx[idx]);
	for (uint32_t i=0; i<MAX_BONE_CHILDREN; i++) {
		const int child_idx = model_data->bones[idx].children[i];
		if (child_idx < 0)
			break;
		calc_bone(anim_data, model_data, child_idx, global_mtx);
	}
	anim_data->bone_mtx[idx] = glms_mat4_mul(global_mtx, model_data->bones[idx].inv_matrix);
}

/* static inline void copy_mask_area(float* restrict const dest, const float* restrict const src, const rect_i32* const mask) { */
static inline void copy_mask_area(cubemap_t* cubemap, const int y1, const int y2) {
	const size_t idx = y1*SHADOW_WIDTH;
	const size_t len = (y2 - y1) * SHADOW_WIDTH * sizeof(float);
	for (int i=0; i<CUBE_FACE_CNT; i++) {
		memcpy(&cubemap->depthbuffer[i][idx], &cubemap->cachebuffer[i][idx], len);
	}
}

static inline void cubemap_update_cache(cubemap_t* cubemap, const int mask_y1, const int mask_y2) {
	const size_t idx = mask_y1*SHADOW_WIDTH;
	const size_t len = (mask_y2 - mask_y1) * SHADOW_WIDTH * sizeof(float);
	for (int i=0; i<CUBE_FACE_CNT; i++) {
		memcpy(&cubemap->cachebuffer[i][idx], &cubemap->depthbuffer[i][idx], len);
	}
}

static void job_shadow(thread_data_t* const data) {
	/* Setup */
	TIME_TYPE start = get_time();
	engine_t* const e = data->e;
	const int max_threads = data->e->cpu_cnt;
	thread_vert_out_t* const frag_in_data = data->frag_in_data;
	const int mask_y1 = data->shadow_mask_y1;
	const int mask_y2 = data->shadow_mask_y2;

	/* Point Light Shadow Mapping */
	TIME_TYPE sub_start = get_time();
	if (e->flags.pointlight_shadows_enabled) {
		pointlight_data_t* const pointlights = &e->pointlights;
		for (int j=0; j<pointlights->cnt; j++) {
			cubemap_t* const cubemap = &pointlights->cubemaps[j];
			if (cubemap->clean) {
				copy_mask_area(cubemap, mask_y1, mask_y2);
			} else {
				for (int i=0; i<max_threads; i++) {
					draw_shadowmap(&frag_in_data[i].pointlight_world[j], cubemap, mask_y1, mask_y2);
				}
				cubemap_update_cache(cubemap, mask_y1, mask_y2);
			}
		}
		for (int i=0; i<max_threads; i++) {
			for (int j=0; j<pointlights->cnt; j++) {
				cubemap_t* const cubemap = &pointlights->cubemaps[j];
				draw_shadowmap(&frag_in_data[i].pointlight_model[j], cubemap, mask_y1, mask_y2);
			}
		}
	}
	data->shadow_pointlight_time = time_diff(sub_start, get_time())*1000;

	/* Baked Shadow Mapping */
	sub_start = get_time();
	if (e->flags.shadowcaster_shadows_enabled) {
		shadowmap_data_t* const shadowcasters = &e->shadowcasters;
		for (int j=0; j<shadowcasters->cnt; j++) {
			cubemap_occlusion_t* const cubemap = &shadowcasters->cubemaps[j];
			if (cubemap->basic.clean) {
				copy_mask_area(&cubemap->basic, mask_y1, mask_y2);
			} else {
				for (int i=0; i<max_threads; i++) {
					draw_shadowmap(&frag_in_data[i].shadow_world[j], &cubemap->basic, mask_y1, mask_y2);
				}
				cubemap_update_cache(&cubemap->basic, mask_y1, mask_y2);
			}
		}
		for (int i=0; i<max_threads; i++) {
			for (int j=0; j<shadowcasters->cnt; j++) {
				cubemap_occlusion_t* const cubemap = &shadowcasters->cubemaps[j];
				draw_shadowmap_occlusion(&frag_in_data[i].shadow_model[j], cubemap, mask_y1, mask_y2);
			}
		}
	}
	data->shadow_shadowcaster_time = time_diff(sub_start, get_time())*1000;

	/* End Timer */
	data->shadow_time = time_diff(start, get_time())*1000;
}

static void job_frag(thread_data_t* data) {
	/* Setup */
	TIME_TYPE start = get_time();

	/* Setup Frag Uniform */
	const engine_t* const e = data->e;
	frag_uniform_t frag_uni;
	frag_uni.seed = data->seed;
	frag_uni.fb = data->uniform.fb;
	frag_uni.db = data->uniform.db;
	frag_uni.interlace = e->interlace;
	frag_uni.shader_cfg_idx = e->shader_cfg_idx;
	frag_uni.thread_cnt = e->cpu_cnt;
	frag_uni.pointlight_cnt = e->pointlights.cnt;
	frag_uni.shadowcaster_cnt = e->shadowcasters.cnt;
	frag_uni.simplelight_cnt = e->simplelight_cnt;
	frag_uni.mask_y1 = data->mask_y1;
	frag_uni.mask_y2 = data->mask_y2;
	frag_uni.viewpos = e->cam.pos;
	frag_uni.scene_time = e->scene_time;
	frag_uni.pointlights = e->pointlights.cubemaps;
	frag_uni.shadowcasters = e->shadowcasters.cubemaps;
	frag_uni.simplelights = e->simplelights;
	frag_uni.noise = e->assets.px_gray[PX_GRAY_TEX_NOISE0];
	frag_uni.noise_nmap = e->assets.px_rgb[PX_RGB_NMAP_NOISE0];

	/* Iterate Triangles */
	draw_frags(e, &frag_uni, data->frag_in_data);

	data->seed = frag_uni.seed;

	/* End Timer */
	data->frag_time = time_diff(start, get_time())*1000;
}

static int job_anim(thread_data_t* data) {
	/* myprintf("calc_bone_thread, data:%p cnt:%lu\n", data, data->cnt); */
	ecs_t* const ecs = &data->e->ecs;
	const TIME_TYPE start = get_time();
	for (uint32_t i=0; i<data->cnt; i++) {
		const uint32_t idx = data->entities[i];
		anim_t* const anim = &ecs->anim[idx];
		const model_anim_data_t* const model = ecs->model[idx].model_anim;
		bone_update(anim, model->basic.bone_cnt, data->delta*1000);
		calc_bone(anim, model, 0, glms_mat4_identity());
	}
	data->anim_time = time_diff(start, get_time())*1000;
	return 0;
}

static void init_vert_out_basic(vert_out_mesh_basic_t* const buf) {
	arrsetcap(buf->idxs, 1024);
	arrsetcap(buf->world_pos, 10240);
}

static void init_vert_out(vert_out_mesh_t* const buf) {
	init_vert_out_basic(&buf->basic);
	const size_t default_len = 1024*10;
	arrsetcap(buf->face_id, default_len);
	arrsetcap(buf->face_idxs, default_len);
	arrsetcap(buf->view_pos, default_len);
	arrsetcap(buf->norms, default_len);
	arrsetcap(buf->uv, default_len);
	arrsetcap(buf->uv_light, default_len);
	arrsetcap(buf->tangents, default_len);
	arrsetcap(buf->tpos, default_len);
	arrsetcap(buf->tviewpos, default_len);
	arrsetcap(buf->tlightpos, default_len);
	arrsetcap(buf->tshadowpos, default_len);
}

static void thread_fun_init(thread_data_t* data) {
#ifndef NDEBUG
	myprintf("[thread_fun_init] id:%d\n", data->thread_id);
#endif
	data->seed.a = 42+data->thread_id;
	init_vert_out(&data->world_tris);
	init_vert_out(&data->worldmap_tris);
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++) {
		init_vert_out_basic(&data->pointlight_tris[i].world);
		init_vert_out_basic(&data->pointlight_tris[i].models);
		init_vert_out_basic(&data->shadowcaster_tris[i].world);
		init_vert_out_basic(&data->shadowcaster_tris[i].models);
	}

	arrsetcap(data->vert_out_data.tris.wfrags.masks, 10240*5);
	arrsetcap(data->vert_out_data.tris.wfrags.frags, 10240*5);
	arrsetcap(data->vert_out_data.tris.wfrags.view_pos, 10240*5);
	arrsetcap(data->vert_out_data.tris.mfrags.masks, 10240*5);
	arrsetcap(data->vert_out_data.tris.mfrags.frags, 10240*5);
	arrsetcap(data->vert_out_data.tris.mfrags.view_pos, 10240*5);
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++) {
		arrsetcap(data->vert_out_data.pointlight_world[i].tris.masks, 10240);
		arrsetcap(data->vert_out_data.pointlight_world[i].tris.frags, 10240);
		arrsetcap(data->vert_out_data.pointlight_world[i].tris.view_pos, 10240);
	}
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++) {
		arrsetcap(data->vert_out_data.pointlight_model[i].tris.masks, 10240);
		arrsetcap(data->vert_out_data.pointlight_model[i].tris.frags, 10240);
		arrsetcap(data->vert_out_data.pointlight_model[i].tris.view_pos, 10240);
	}
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++) {
		arrsetcap(data->vert_out_data.shadow_world[i].tris.masks, 10240);
		arrsetcap(data->vert_out_data.shadow_world[i].tris.frags, 10240);
		arrsetcap(data->vert_out_data.shadow_world[i].tris.view_pos, 10240);
	}
	for (int i=0; i<MAX_DYNAMIC_LIGHTS; i++) {
		arrsetcap(data->vert_out_data.shadow_model[i].tris.masks, 10240);
		arrsetcap(data->vert_out_data.shadow_model[i].tris.frags, 10240);
		arrsetcap(data->vert_out_data.shadow_model[i].tris.view_pos, 10240);
	}
}

static int thread_func(thread_data_t* data) {
#ifndef NDEBUG
	myprintf("[job init] data:%lx thr:%lx wait:%d jobs:%d\n", data, data->thread, *data->waiting_thread_cnt, *data->jobs_queued);
#endif
	/* Init Thread */
	thread_fun_init(data);

	/* const TIME_TYPE start = get_time(); */
	/* Enter Loop */
	engine_t* const e = data->e;
	do {
#ifdef WORKER_VERBOSE
		myprintf("[job] data:%lx id:%d thr:0x%lx wait:%d jobs:%d\n", data, data->thread_id, data->thread, *data->waiting_thread_cnt, *data->jobs_queued);
#endif
		LockMutex(data->mutex);
		*data->jobs_queued -= 1;
		data->state = JOB_STATE_SLEEP;
		*data->waiting_thread_cnt += 1;
		assert(*data->waiting_thread_cnt <= e->cpu_cnt);
		CondSignal(data->done_mu_cond);
		while (data->state == JOB_STATE_SLEEP) {
#ifdef WORKER_VERBOSE
			myprintf("[job sl] id:%d thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
			CondWait(data->mu_cond, data->mutex);
		}
		*data->waiting_thread_cnt -= 1;
		UnlockMutex(data->mutex);
#ifdef WORKER_VERBOSE
		myprintf("[job go] id:%d thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
		assert(*data->waiting_thread_cnt >= 0);

		switch (data->state) {
			case JOB_STATE_BONES:
#ifdef WORKER_VERBOSE
				myprintf("[thread_func bones] id:%lx thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
				job_anim(data);
				break;
			case JOB_STATE_VERTS:
#ifdef WORKER_VERBOSE
				myprintf("[thread_func verts] id:%lx thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
				switch (e->shader_cfg_idx) {
					case 0:
						job_vert_basic(data);
						break;
					case 1:
						job_vert_basic_no_pl(data);
						break;
					case 2:
						job_vert_basic_no_sc(data);
						break;
					case 3:
						job_vert_basic_no_plsc(data);
						break;
				}
				break;
			case JOB_STATE_SHADOW:
#ifdef WORKER_VERBOSE
				myprintf("[thread_func shadow] id:%d thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
				job_shadow(data);
				break;
			case JOB_STATE_FRAG:
#ifdef WORKER_VERBOSE
				myprintf("[thread_func frag] id:%d thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
				job_frag(data);
				break;
			case JOB_STATE_QUIT:
#ifndef NDEBUG
				myprintf("[job quit] id:%d thr:0x%lx wait:%d jobs:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt, *data->jobs_queued);
#endif
				return 0;
				break;
			default:
#ifdef WORKER_VERBOSE
				myprintf("[thread_func verts] id:%d thr:0x%lx waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
				assert(1);
				break;
		}
#ifdef WORKER_VERBOSE
		myprintf("[thread_func done] id:%d thr:0x%lx data:%d waiting:%d\n", data->thread_id, data->thread, *data->waiting_thread_cnt);
#endif
	} while(1);
	return 0;
}

/* Initializes but doesn't malloc (thread init mallocs) */
int thread_init(engine_threads_t* e) {
#ifndef NDEBUG
	fputs("[thread_init]\n", stdout);
#endif
	const int32_t max_threads = e->e.cpu_cnt;
	e->e.jobs_queued = max_threads; // needed or bug
	mutex_t* mutex = CreateMutex();
	cond_t* cond = CreateCond();
	cond_t* done_cond = CreateCond();
	const int row_height = FB_H / max_threads;
	const int shadow_row_height = SHADOW_HEIGHT / max_threads;
	for (int32_t i=0; i<max_threads; i++) {
		e->jobs[i].state = JOB_STATE_SLEEP;
		e->jobs[i].e = &e->e;
		e->jobs[i].uniform.fb = e->e.fb;
		e->jobs[i].uniform.db = e->e.db;
		e->jobs[i].uniform.cam = &e->e.cam;
		e->jobs[i].uniform.lights = e->e.lights;
		e->jobs[i].entities = NULL;
		e->jobs[i].vert_out_data.tris.wfrags.masks = NULL;
		e->jobs[i].vert_out_data.tris.wfrags.frags = NULL;
		e->jobs[i].vert_out_data.tris.wfrags.view_pos = NULL;
		e->jobs[i].vert_out_data.tris.mfrags.masks = NULL;
		e->jobs[i].vert_out_data.tris.mfrags.frags = NULL;
		e->jobs[i].vert_out_data.tris.mfrags.view_pos = NULL;
		for (int j=0; j<MAX_DYNAMIC_LIGHTS; j++) {
			e->jobs[i].vert_out_data.shadow_world[j].tris.masks = NULL;
			e->jobs[i].vert_out_data.shadow_world[j].tris.frags = NULL;
		}
		for (int j=0; j<MAX_DYNAMIC_LIGHTS; j++) {
			e->jobs[i].vert_out_data.shadow_model[j].tris.masks = NULL;
			e->jobs[i].vert_out_data.shadow_model[j].tris.frags = NULL;
		}
		e->jobs[i].waiting_thread_cnt = &e->e.waiting_thread_cnt;
		e->jobs[i].jobs_queued = &e->e.jobs_queued;
		e->jobs[i].mutex = mutex;
		e->jobs[i].mu_cond = cond;
		e->jobs[i].done_mu_cond = done_cond;

		/* Set Framebuffer Mask */
		e->jobs[i].mask_y1 = i*row_height;
		e->jobs[i].mask_y2 = (i+1)*row_height;
		e->jobs[i].shadow_mask_y1 = i*shadow_row_height;
		e->jobs[i].shadow_mask_y2 = (i+1)*shadow_row_height;

		const char* thread_name = TextFormat("job%d\n", i);
#ifdef PTHREADS
		e->jobs[i].thread = CreateThread((void*)&thread_func, thread_name, &e->jobs[i]);
#else
		e->jobs[i].thread = CreateThread((SDL_ThreadFunction)&thread_func, thread_name, &e->jobs[i]);
#endif
		if (e->jobs[i].thread == NULL) {
			myprintf("[ERR] can't create thread!\n");
			return 1;
		}
	}

	/* Create Draw Mask */
	const int last_idx = max_threads-1;

	const int last_mask_y = row_height*last_idx;
	const int last_row_height = row_height + (FB_H - row_height*max_threads);
	e->jobs[last_idx].mask_y1 = last_mask_y;
	e->jobs[last_idx].mask_y2 = last_mask_y+last_row_height;

	const int last_shadow_mask_y = shadow_row_height*last_idx;
	const int last_shadow_row_height = shadow_row_height + (SHADOW_HEIGHT - shadow_row_height*max_threads);
	e->jobs[last_idx].shadow_mask_y1 = last_shadow_mask_y;
	e->jobs[last_idx].shadow_mask_y2 = last_shadow_mask_y+last_shadow_row_height;

	return 0;
}

int thread_set_and_go(engine_threads_t* e, const JOB_STATE state) {
#ifdef WORKER_VERBOSE
	myprintf("[job] start s:%d j:%d w:%d %d\n", state, e->e.jobs_queued, e->e.waiting_thread_cnt, *(int64_t*)e->jobs[0].mutex);
#endif
	/* Wait all threads */
	const int32_t max_threads = e->e.cpu_cnt;
	LockMutex(e->jobs[0].mutex);
	while (e->e.waiting_thread_cnt != max_threads) {
		CondWait(e->jobs[0].done_mu_cond, e->jobs[0].mutex);
	}

	/* Set State */
	e->e.jobs_queued = e->e.cpu_cnt;
	for (i32 i=0; i<e->e.cpu_cnt; i++) {
		e->jobs[i].state = state;
	}

	/* Broadcast */
#ifdef WORKER_VERBOSE
	myprintf("[job] broadcast s:%d j:%d w:%d\n", state, e->e.jobs_queued, e->e.waiting_thread_cnt);
#endif
	CondBroadcast(e->jobs[0].mu_cond);

	/* Wait for all jobs */
#ifdef WORKER_VERBOSE
	myprintf("[job] wait s:%d j:%d w:%d\n", state, e->e.jobs_queued, e->e.waiting_thread_cnt);
#endif
	while (e->e.jobs_queued != 0) {
		CondWait(e->jobs[0].done_mu_cond, e->jobs[0].mutex);
	}
#ifdef WORKER_VERBOSE
	myprintf("[job] wd s:%d j:%d w:%d\n", state, e->e.jobs_queued, e->e.waiting_thread_cnt);
#endif
	UnlockMutex(e->jobs[0].mutex);

	return 0;
}
