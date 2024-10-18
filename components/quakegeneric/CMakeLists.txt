cmake_minimum_required(VERSION 3.5)

set(QUAKE_SOURCE_DIR quakegeneric)

set(QUAKEGENERIC_SOURCES
	${QUAKE_SOURCE_DIR}/source/chase.c
	${QUAKE_SOURCE_DIR}/source/cl_demo.c
	${QUAKE_SOURCE_DIR}/source/cl_input.c
	${QUAKE_SOURCE_DIR}/source/cl_main.c
	${QUAKE_SOURCE_DIR}/source/cl_parse.c
	${QUAKE_SOURCE_DIR}/source/cl_tent.c
	${QUAKE_SOURCE_DIR}/source/cmd.c
	${QUAKE_SOURCE_DIR}/source/common.c
	${QUAKE_SOURCE_DIR}/source/console.c
	${QUAKE_SOURCE_DIR}/source/crc.c
	${QUAKE_SOURCE_DIR}/source/cvar.c
	${QUAKE_SOURCE_DIR}/source/d_edge.c
	${QUAKE_SOURCE_DIR}/source/d_fill.c
	${QUAKE_SOURCE_DIR}/source/d_init.c
	${QUAKE_SOURCE_DIR}/source/d_modech.c
	${QUAKE_SOURCE_DIR}/source/d_part.c
	${QUAKE_SOURCE_DIR}/source/d_polyse.c
	${QUAKE_SOURCE_DIR}/source/d_scan.c
	${QUAKE_SOURCE_DIR}/source/d_sky.c
	${QUAKE_SOURCE_DIR}/source/d_sprite.c
	${QUAKE_SOURCE_DIR}/source/d_surf.c
	${QUAKE_SOURCE_DIR}/source/d_vars.c
	${QUAKE_SOURCE_DIR}/source/d_zpoint.c
	${QUAKE_SOURCE_DIR}/source/draw.c
	${QUAKE_SOURCE_DIR}/source/host_cmd.c
	${QUAKE_SOURCE_DIR}/source/host.c
	${QUAKE_SOURCE_DIR}/source/in_null.c
	${QUAKE_SOURCE_DIR}/source/keys.c
	${QUAKE_SOURCE_DIR}/source/mathlib.c
	${QUAKE_SOURCE_DIR}/source/menu.c
	${QUAKE_SOURCE_DIR}/source/model.c
	${QUAKE_SOURCE_DIR}/source/net_loop.c
	${QUAKE_SOURCE_DIR}/source/net_main.c
	${QUAKE_SOURCE_DIR}/source/net_none.c
	${QUAKE_SOURCE_DIR}/source/net_vcr.c
	${QUAKE_SOURCE_DIR}/source/nonintel.c
	${QUAKE_SOURCE_DIR}/source/pr_cmds.c
	${QUAKE_SOURCE_DIR}/source/pr_edict.c
	${QUAKE_SOURCE_DIR}/source/pr_exec.c
	${QUAKE_SOURCE_DIR}/source/r_aclip.c
	${QUAKE_SOURCE_DIR}/source/r_alias.c
	${QUAKE_SOURCE_DIR}/source/r_bsp.c
	${QUAKE_SOURCE_DIR}/source/r_draw.c
	${QUAKE_SOURCE_DIR}/source/r_edge.c
	${QUAKE_SOURCE_DIR}/source/r_efrag.c
	${QUAKE_SOURCE_DIR}/source/r_light.c
	${QUAKE_SOURCE_DIR}/source/r_main.c
	${QUAKE_SOURCE_DIR}/source/r_misc.c
	${QUAKE_SOURCE_DIR}/source/r_part.c
	${QUAKE_SOURCE_DIR}/source/r_sky.c
	${QUAKE_SOURCE_DIR}/source/r_sprite.c
	${QUAKE_SOURCE_DIR}/source/r_surf.c
	${QUAKE_SOURCE_DIR}/source/r_vars.c
	${QUAKE_SOURCE_DIR}/source/sbar.c
	${QUAKE_SOURCE_DIR}/source/screen.c
	${QUAKE_SOURCE_DIR}/source/snd_dma.c
	${QUAKE_SOURCE_DIR}/source/snd_mem.c
	${QUAKE_SOURCE_DIR}/source/snd_mix.c
	${QUAKE_SOURCE_DIR}/source/sv_main.c
	${QUAKE_SOURCE_DIR}/source/sv_move.c
	${QUAKE_SOURCE_DIR}/source/sv_phys.c
	${QUAKE_SOURCE_DIR}/source/sv_user.c
	${QUAKE_SOURCE_DIR}/source/sys_null.c
	${QUAKE_SOURCE_DIR}/source/vid_null.c
	${QUAKE_SOURCE_DIR}/source/view.c
	${QUAKE_SOURCE_DIR}/source/wad.c
	${QUAKE_SOURCE_DIR}/source/world.c
	${QUAKE_SOURCE_DIR}/source/zone.c
	${QUAKE_SOURCE_DIR}/source/quakegeneric.c
)


idf_component_register(
	SRCS ${QUAKEGENERIC_SOURCES}
	INCLUDE_DIRS ${QUAKE_SOURCE_DIR}/source/
)

idf_build_set_property(COMPILE_OPTIONS "-w" APPEND) # Suppress all warnings (which are often reported as errors)
