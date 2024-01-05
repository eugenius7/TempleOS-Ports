#include "ents/pickup.h"
#include "engine.h"
#include "ents/player.h"
#include "ents/trigger.h"

void pickup_think(void* const engine, const int32_t id, const float delta) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	glm_quatv(ecs->rot[id].raw, e->scene_time*5, (vec3){0,1,0});
}

void pickup_touch(void* const engine, const int32_t id, const int32_t other) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	snd_play_sfx(&e->audio, e->assets.snds[SND_PICKUP]);

	e->ecs.ai[id].think = think_suicide;
}

void pickup_touch_shotgun(void* const engine, const int32_t id, const int32_t other) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	snd_play_sfx(&e->audio, e->assets.snds[SND_PICKUP]);
	const u8 mask = 1<<(WEAPON_SHOTGUN-1);
	if (!(ecs->player.avail_weapons & mask)) {
		ecs->player.avail_weapons |= mask;
		player_switch_weapon(e, WEAPON_SHOTGUN);
	}
	ecs->player.ammo[WEAPON_SHOTGUN] += 100;
	ecs->ai[id].think = think_suicide;
}

void pickup_touch_beam(void* const engine, const int32_t id, const int32_t other) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	snd_play_sfx(&e->audio, e->assets.snds[SND_PICKUP]);
	const u8 mask = 1<<(WEAPON_BEAM-1);
	if (!(ecs->player.avail_weapons & mask)) {
		ecs->player.avail_weapons |= mask;
		player_switch_weapon(e, WEAPON_BEAM);
	}
	ecs->player.ammo[WEAPON_BEAM] += 100;
	ecs->ai[id].think = think_suicide;
}

void pickup_touch_smg(void* const engine, const int32_t id, const int32_t other) {
	engine_t* const e = engine;
	ecs_t* const ecs = &e->ecs;
	if (ecs->player_id != other)
		return;
	snd_play_sfx(&e->audio, e->assets.snds[SND_PICKUP]);
	const u8 mask = 1<<(WEAPON_SMG-1);
	if (!(ecs->player.avail_weapons & mask)) {
		ecs->player.avail_weapons |= mask;
		player_switch_weapon(e, WEAPON_SMG);
	}
	ecs->player.ammo[WEAPON_SMG] += 100;
	ecs->ai[id].think = think_suicide;
}
