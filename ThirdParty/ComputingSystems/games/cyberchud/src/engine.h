#ifndef ENGINE_H
#define ENGINE_H

#include <SDL2/SDL_video.h>

#ifdef HW_ACCEL
#include <SDL2/SDL_render.h>
#endif

#include "assets.h"
#include "controls.h"
#include "ecs.h"
#include "mytime.h"
#include "palette.h"
#include "sound.h"
#include "thread.h"
#include "visflags.h"
#include "xorshift.h"
#include "ui/talkbox.h"
#include "ui/widgets/button.h"
#include "ui/widgets/widget_graph.h"
#include "vfx/vfx_common.h"

#define MAX_SIMPLELIGHTS 16

typedef enum {
	PICKUP_BEAM,
	PICKUP_SHOTGUN,
	PICKUP_SMG,
} PICKUP_TYPE;

typedef struct {
	uint32_t cnt;
	float time;
	ecs_t* ecs;
	uint32_t* entities;
	cam_t* cam;
	shader_t* out;
	mutex_t* mutex;
	cond_t* mu_cond;
} thread_model_anim_t;

typedef struct {
	float delta;
	uint32_t cnt;
	float time;
	ecs_t* ecs;
	uint32_t* entities;
	mutex_t* mutex;
	cond_t* mu_cond;
} calc_bone_thread_t;

typedef enum {
	JOB_STATE_SLEEP,
	JOB_STATE_BONES,
	JOB_STATE_VERTS,
	JOB_STATE_SHADOW,
	JOB_STATE_FRAG,
	JOB_STATE_QUIT,
} JOB_STATE;

typedef struct {
	int cnt;
	cubemap_occlusion_t cubemaps[MAX_DYNAMIC_LIGHTS];
} shadowmap_data_t;

typedef struct {
	int cnt;
	cubemap_t cubemaps[MAX_DYNAMIC_LIGHTS];
} pointlight_data_t;

typedef enum {
	SCENE_TITLE,
	SCENE_PLAYFIELD,
} SCENE;

typedef enum {
	UI_MODE_NONE,
	UI_MODE_CONTROLS,
	UI_MODE_CUBEMAPS,
	UI_MODE_DEBUG,
	UI_MODE_OPTIONS,
	UI_MODE_PAUSE,
	UI_MODE_TITLE,
} UI_MODE;

typedef enum {
	CUBEMAP_UI_STATE_SHADOWCASTER,
	CUBEMAP_UI_STATE_POINTLIGHTS,
} CUBEMAP_UI_STATE;

typedef union {
	CUBEMAP_UI_STATE cubemap;
} scene_state_u;

typedef struct {
	int32_t light_id;
	int32_t logo_id;
} scene_title_t;

typedef union {
	scene_title_t title;
} scene_u;

typedef struct {
	unsigned int glitchout : 1;
	unsigned int neverhappen : 1;
	unsigned int noise : 1;
	unsigned int wireframe : 1;
	unsigned int camrock : 1;
	unsigned int skip_frag: 1;
	unsigned int screen_pulse: 1;
} vfx_flags_t;

typedef struct {
	unsigned int mute : 1;
	unsigned int fps_graph : 1;
	unsigned int pointlight_shadows_enabled : 1;
	unsigned int shadowcaster_shadows_enabled : 1;
} engine_flags_t;

#if __EMSCRIPTEN__
#define SND_SAMPLES 2560
#else
#define SND_SAMPLES 512
#endif

#define SND_FREQ 11025

#ifdef TOSLIKE
typedef struct {
	volatile u32 lock;
} mu_t;
#endif

typedef struct {
	float scene_time;
	float scene_transition;
	float neverhappen_time;
	float glitchout_time;
	float noise_time;
	float wireframe_time;
	float camrock_time;
	float screen_pulse_time;
	i8 cpu_cnt;
	i8 interlace;
	i8 switch_scene;
	i8 switch_level;
	i8 current_level;
	i8 waiting_thread_cnt;
	i8 jobs_queued;
	SCENE scene;
	SDL_Window* win;
	pointlight_data_t pointlights;
	shadowmap_data_t shadowcasters;
	cam_t cam;
	light_t lights[MAX_DYNAMIC_LIGHTS];
	audio_t audio;
	xorshift_t seed;
	TIME_TYPE ticks_cur;
	TIME_TYPE ticks_last;
	float ticks_accum;
	controls_t controls;
	assets_t assets;
	ecs_t ecs;
	hull_box_t hb;
	palette_t palette;
	palette_t palette_base;
	plane_t* world_colliders;

	/* Perf Timers */
	float time_cull;
	float time_edict;
	float time_particle_emitter;
	float time_particle;
	float time_ai;
	float time_ai_light;
	float time_absbox;

	/* Shared Scene UI Buffers */
	UI_MODE ui_mode;
	UI_MODE ui_mode_back;
	scene_state_u ui_state; // used differently per-scene
	scene_u scene_data;
	int ui_int0;
	engine_flags_t flags;
	vfx_flags_t vfx_flags;
	i8 shader_cfg_idx;
	i8 max_pointlights;
	i8 max_shadowcasters;
	i8 button_cnt;
	i8 checkbox_cnt;
	i8 slider_cnt;
	button_label_t buttons[8];
	checkbox_t checkboxes[4];
	slider_t sliders[4];
	ui_graph_t* graphs;
	talkbox_t talkbox;

	/* Draw List */
	line_t* lines;

	/* Buffers */
	visflags_t visflags[MAX_VISFLAGS];
	size_t cull_tri_cnt;
	uint32_t* idxs_culled; // scratch for update
	uint32_t* idxs_bones;

	/* Simplelight Buffer */
	i8 simplelight_cnt;
	vec4s simplelights[MAX_SIMPLELIGHTS];
	float simplelight_dist[MAX_SIMPLELIGHTS]; // scratch for selection
	vec4s warpcircle;

	char scratch[64];
	px_t* vfx_fb;
	u8* fb;
	u8 fb_real[SCREEN_W*SCREEN_H];
	float db[SCREEN_W*FB_H];

#ifdef HW_ACCEL
	SDL_Renderer* sdl_render;
	SDL_Texture* sdl_tex;
	rgba_t fb_swiz[SCREEN_W*SCREEN_H];
#endif

#ifdef TOSLIKE
	TIME_TYPE tos_sample_start_t;
	mu_t tos_snd_mu;
	i32 tos_snd_spkr;
	i32 tos_snd_pos;
	i16 tos_snd_buf[SND_SAMPLES];
#endif
} engine_t;

typedef struct {
	int32_t id;
	uint32_t cnt;
	const mesh_t* mesh;
} vert_tri_idx_t;

typedef struct {
	size_t tri_total;
	vert_tri_idx_t* idxs;
	mat3s* world_pos;
} vert_out_mesh_basic_t;

typedef struct {
	vert_out_mesh_basic_t basic;
	uint32_t* face_id;
	face_t* face_idxs;
	vec4_3* view_pos;
	mat3s* norms;
	uv_t* uv;
	uv_t* uv_light;
	mat3s* tangents;
	mat3s* tpos;
	mat3s* tviewpos;
	lightmtx_t* tlightpos;
	lightmtx_t* tshadowpos;
} vert_out_mesh_t;

typedef struct {
	u32 id;
	u32 face_idx;
	u32 face_cnt;
	u32 mesh_idx;
	const mesh_t* mesh; // TODO redundat with mesh_idx
} job_vert_t;

typedef struct {
	vert_out_mesh_basic_t world;
	vert_out_mesh_basic_t models;
} vert_out_cubemap_t;

typedef struct {
	xorshift_t seed;
	i8 thread_id;
	JOB_STATE state;
	i16 mask_y1;
	i16 mask_y2;
	i16 shadow_mask_y1;
	i16 shadow_mask_y2;
	uint32_t cnt;
	uint32_t wtri_cnt;
	uint32_t mtri_cnt;
	float delta;
	float anim_time;
	float vert_world_time;
	float vert_shadow_time;
	float vert_view_time;
	float shadow_time;
	float shadow_pointlight_time;
	float shadow_shadowcaster_time;
	float frag_time;
	engine_t* e;
	uint32_t* entities;
	draw_uniform_t uniform;

	vert_out_mesh_t worldmap_tris; // animated and world positioned
	vert_out_mesh_t world_tris; // animated and world positioned
	vert_out_cubemap_t pointlight_tris[MAX_DYNAMIC_LIGHTS];
	vert_out_cubemap_t shadowcaster_tris[MAX_DYNAMIC_LIGHTS];

	thread_vert_out_t vert_out_data;
	thread_vert_out_t* frag_in_data;
	job_vert_t* vert_job_data;
	i8* waiting_thread_cnt;
	i8* jobs_queued;
	thread_t* thread;
	mutex_t* mutex;
	cond_t* mu_cond;
	cond_t* done_mu_cond;
} thread_data_t;

typedef struct {
	i8 draw;
	i8 update_palette;
	i8 quit;
	engine_t* e;
	mutex_t* mutex;
	cond_t* mu_cond;
	thread_t* thread;
	palette_t palette;
	u8 fb[SCREEN_W*SCREEN_H];
} tosfb_thread_t;

typedef struct {
	engine_t e;
	thread_data_t* jobs;
#ifdef TEMPLEOS
	tosfb_thread_t tosfb_thr;
#endif
} engine_threads_t;

engine_threads_t* engine_alloc(const i8 cpu_cnt);
void engine_free(engine_threads_t* const et);
void engine_cfg_update(engine_t* const e);

int32_t new_bsp_trigger_levelchange(engine_t* const e, bsp_entity_t* const ent);
int32_t new_bsp_trigger_multiple(engine_t* const e, bsp_entity_t* const ent);
int32_t new_bsp_trigger_once(engine_t* const e, bsp_entity_t* const ent);
int32_t new_bsp_trigger_talk(engine_t* const e, bsp_entity_t* const ent);
int32_t new_mob(engine_t* e, const model_anim_data_t* const model_data, const anim_data_t* const anim_data, const vec3s pos);
int32_t new_chud(engine_t* e, const vec3s pos, const float yaw, const u8 task);
int32_t new_mutt(engine_t* e, const vec3s pos, const float yaw, const u8 task);
int32_t new_troon(engine_t* e, const vec3s pos, const float yaw, const u8 task);
int32_t new_model_anim(engine_t* e, const model_anim_data_t* const model_data, const anim_data_t* const anim_data, const vec3s pos);
int32_t new_rocket(engine_t* e, const model_basic_t* const model_data, const vec3s* pos, const vec3s* dir, const float speed, const int32_t parent, const float brightness, const int16_t dmg);
int32_t new_beam(engine_t* const e, const vec3s* const pos, const vec3s* dir, const float dist);
int32_t new_pickup(engine_t* const e, vec3s pos, const u8 type);

#endif
