set(C_SRC
	ai.c
	alloc.c
	anim.c
	assets.c
	bsp.c
	bsp_parse.c
	camera.c
	controls.c
	cull.c
	dialogue.c
	draw.c
	ecs.c
	engine.c
	game.c
	palette.c
	particle.c
	phys.c
	playfield.c
	px.c
	quake.c
	render.c
	shader.c
	shadow.c
	sound.c
	stolenlib.c
	text.c
	vec.c
	visflags.c
	wad.c
	worker.c
	zstddeclib.c
	ents/button.c
	ents/chud.c
	ents/door.c
	ents/dynlight.c
	ents/mob.c
	ents/pickup.c
	ents/player.c
	ents/rocket.c
	ents/spawner.c
	ents/trigger.c
	scene/playfield.c
	scene/title.c
	types/name_heap.c
	ui/menu_common.c
	ui/menu_controls.c
	ui/menu_cubemap.c
	ui/menu_debug.c
	ui/menu_options.c
	ui/menu_pause.c
	ui/menu_title.c
	ui/statusline.c
	ui/talkbox.c
	ui/widgets/button.c
	ui/widgets/tos_text.c
	ui/widgets/widget_graph.c
	vfx/vfx_camrock.c
	vfx/vfx_neverhappen.c
	vfx/vfx_wireframe.c
	utils/mymalloc.c
	# libccd/ccd.c
	# libccd/mpr.c
	# libccd/polytope.c
	# libccd/support.c
	# libccd/vec3.c
	)

set(MUSL_SRC
	musl/__cos.c
	musl/__cosdf.c
	musl/__math_oflowf.c
	musl/__math_uflowf.c
	musl/__math_xflowf.c
	musl/__rem_pio2.c
	musl/__rem_pio2_large.c
	musl/__rem_pio2f.c
	musl/__signbit.c
	musl/__signbitl.c
	musl/__sin.c
	musl/__sindf.c
	musl/__tandf.c
	musl/acosf.c
	musl/asinf.c
	musl/atan.c
	musl/atan2.c
	musl/atan2f.c
	musl/atanf.c
	musl/copysign.c
	musl/cos.c
	musl/cosf.c
	musl/exp.c
	musl/exp2f_data.c
	musl/exp_data.c
	musl/fminf.c
	musl/fmod.c
	musl/fmodf.c
	musl/log.c
	musl/log_data.c
	musl/math.c
	musl/pow.c
	musl/pow_data.c
	musl/powf.c
	musl/powf_data.c
	musl/qsort.c
	musl/qsort_nr.c
	musl/roundf.c
	musl/scalbn.c
	musl/sin.c
	musl/sinf.c
	musl/sincosf.c
	musl/stpncpy.c
	musl/strncat.c
	musl/strncpy.c
	musl/tanf.c
	)

set(ASM_SRC
	musl/x86_64/memcpy.s
	musl/x86_64/memmove.s
	musl/x86_64/memset.s
	)

set(TOS_SRC
	tosfb.c
	x86_64/isr_pit.s
	)

list(TRANSFORM C_SRC PREPEND src/)
list(TRANSFORM MUSL_SRC PREPEND src/)
list(TRANSFORM TOS_SRC PREPEND src/)
list(TRANSFORM ASM_SRC PREPEND src/)
