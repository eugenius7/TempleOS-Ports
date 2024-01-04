#ifndef ECS_H
#define ECS_H

#include "ai.h"
#include "bsp.h"
#include "dialogue.h"

typedef enum {
	TYPE_NONE,
	TYPE_MOB,
	TYPE_MISC,
} __attribute__ ((__packed__)) ECS_TYPE;

typedef struct {
	unsigned int mob : 1;
	unsigned int bones : 1;
	unsigned int model_mtx : 1;
	unsigned int skip_cull : 1;
	unsigned int skip_draw : 1;
	unsigned int highlight : 1;
	unsigned int gravity : 1;
	unsigned int tangent : 1;
	unsigned int worldmap : 1;
	unsigned int bsp_model : 1;
	unsigned int no_shadow: 1;
	unsigned int noclip : 1;
	unsigned int decay : 1;
	unsigned int static_model_osc : 1;
	unsigned int test0 : 1;
} __attribute__((packed)) flag_t;

typedef union {
	const model_basic_t* model_static;
	const model_anim_data_t* model_anim;
} ecs_model_t;

typedef struct {
	float length;
} shader_warp_t;

typedef union {
	shader_warp_t warp;
} ecs_shader_u;

typedef struct {
	int16_t hp;
	int16_t max;
} __attribute__((packed)) hp_t;

typedef struct {
	unsigned int moving : 1;
	unsigned int at_target : 1;
	float speed;
	float cooldown;
	float ltime;
	float time_end;
} door_t;

typedef enum {
	DYN_LIGHT_STYLE_NONE,
	DYN_LIGHT_STYLE_LINE,
	DYN_LIGHT_STYLE_ORBIT,
} __attribute__ ((__packed__)) DYN_LIGHT_STYLE;

typedef struct {
	DYN_LIGHT_STYLE style;
	unsigned int direction : 1;
	unsigned int no_shadowcaster : 1;
	float ltime;
	float speed;
	float range;
} dynamiclight_t;

typedef enum {
	WEAPON_NONE,
	WEAPON_SHOTGUN,
	WEAPON_BEAM,
	WEAPON_SMG,
} WEAPON;
#define WEAPON_TOTAL 4

typedef struct {
	float fire_cooldown;
	i8 cur_weapon;
	u8 avail_weapons;
	i16 ammo[WEAPON_TOTAL];
} player_t;

typedef struct {
	int light_cnt;
	vec3s* lights;
} beam_t;

typedef struct {
	const model_basic_t* model;
	vec3s dir;
	float vel;
	float spawn_speed;
	float cooldown;
} particle_emitter_t;

typedef struct {
	unsigned int autospawn: 1;
	MOB_CLASS spawn_type;
	float spawn_speed;
	float cooldown;
} spawner_t;

typedef struct {
	unsigned int damaged: 1;
} mob_flags_t;

typedef enum {
	CHUD_TASK_NONE,
	CHUD_TASK_COMPUTER,
	CHUD_TASK_APARTMENT_GREETER,
} __attribute__ ((__packed__)) chud_task_e;

typedef struct {
	mob_flags_t flags;
	MOB_STATE state;
	union {
		chud_task_e chud;
	} task;
	float fire_cooldown;
	float flicker_cooldown;
} mob_t;

typedef struct {
	mob_t basic;
	vec3s nav_target;
} chud_t;

typedef struct {
	mob_t basic;
} mutt_t;

typedef struct {
	mob_t basic;
} troon_t;

typedef struct {
	DIALOGUE_IDX diag_idx;
} trigger_talk_t;

typedef struct {
	int16_t dmg;
} projectile_t;

/* Unions */

/* Ideally this union should be exactly 64 bytes and store unique type information */
typedef union {
	beam_t beam;
	chud_t chud;
	door_t door;
	dynamiclight_t light;
	mutt_t mutt;
	particle_emitter_t particle_emitter;
	spawner_t spawner;
	trigger_talk_t trigger_talk;
	troon_t troon;
	projectile_t projectile;
} custom0_u;

typedef union {
	edict_t* edict;
	edict_light_t* light;
} edict_u;

/* ECS */

typedef struct {
	/* Global */
	int32_t low_id;
	int32_t player_id;
	int32_t pov_model_id;
	int32_t light_ids[4];
	player_t player;

	/* Struct of Arrays */
	flag_t* flags;
	ECS_TYPE* type;
	ai_t* ai;
	int32_t* parent;
	int32_t* target;
	custom0_u* custom0;
	ecs_shader_u* shader;
	float* brightness;
	float* cooldown0;
	hp_t* hp;
	vec3s* pos;
	vec3s* pos_home;
	vec3s* pos_target;
	vec3s* vel;
	vec3s* scale;
	versors* rot;
	bbox_t *bbox;
	mat4s* mtx;
	ecs_model_t* model;
	anim_t* anim;
	edict_u* edict;

	/* Heap */
	alloc_t alloc;

	/* Free List */
	BITARR_TYPE* bit_edict;
	BITARR_TYPE* bit_light;
	BITARR_TYPE* bit_light_dynamic;
	BITARR_TYPE* bit_particle;
	BITARR_TYPE* bit_particle_emitter;
	BITARR_TYPE* bit_spawner;
	BITARR_TYPE* bit_simplelight0; // top priority
	BITARR_TYPE* bit_simplelight0_arr; // top priority
	BITARR_TYPE* bit_simplelight1; // second priority
	BITARR_TYPE* used_bm;

	/* Quake Edict Stuff */
	qcvm_t qcvm;
} ecs_t;

edict_t* new_edict(ecs_t *ecs, const int32_t id);
int32_t new_entity(ecs_t* const ecs);
edict_light_t* new_light(ecs_t* ecs, bsp_entity_t* const ent);
edict_light_t* new_light_dynamic(ecs_t* ecs, const vec3s pos, const float brightness, const float speed, const float range, const DYN_LIGHT_STYLE style);
int32_t new_model_static(ecs_t* ecs, const model_basic_t* const model_data, const vec3s* pos);
int32_t new_particle_emitter(ecs_t *ecs, const vec3s* pos, const model_basic_t* const model, const float vel, const float spawn_speed);
int32_t new_particle(ecs_t *ecs, const vec3s *pos, const vec3s* vel, const model_basic_t* const model, const float brightness);
int32_t new_path_corner(ecs_t* ecs, bsp_entity_t* const ent);
int32_t new_player(ecs_t* ecs, const vec3s pos, float angle);
int32_t new_spawner(ecs_t *ecs, const vec3s* pos, const float spawn_speed, const int spawn_type, const bool autospawner);

edict_t* new_bsp_door(ecs_t* const ecs, bsp_t* const bsp, bsp_entity_t* const ent);
edict_t* new_bsp_button(ecs_t* const ecs, bsp_t* const bsp, bsp_entity_t* const ent);
int32_t new_bsp_worldmodel(ecs_t* const ecs, bsp_t* const bsp, const u32 qmod_idx);
void ecs_free(ecs_t* const ecs);
void ecs_init(ecs_t* const ecs);
void ecs_reset(ecs_t* const ecs);
void free_mob(ecs_t* const ecs, const i32 id);
void free_particle(ecs_t* const ecs, const int32_t id);
void free_rocket(ecs_t* const ecs, const int32_t id);
void free_beam(ecs_t* const ecs, const i32 id);
void free_bsp_trigger(ecs_t* const ecs, const int32_t id);

void qcvm_resize(qcvm_t* const qcvm);

void model_anim_change_model(ecs_t* const ecs, const i32 id, model_anim_data_t* const model_data, const anim_data_t* const anim_data);

#endif
