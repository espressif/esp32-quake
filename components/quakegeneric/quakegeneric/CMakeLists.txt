
# project
cmake_minimum_required(VERSION 3.3)
project(quakegeneric DESCRIPTION "portable quake" HOMEPAGE_URL https://github.com/erysdren/quakegeneric LANGUAGES C VERSION 1.0.9)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# compile options
add_compile_options(-m32 -std=gnu99)
add_link_options(-m32)

# tinycc
if(CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
	add_compile_definitions(-DSDL_DISABLE_IMMINTRIN_H=1)
endif()

# find math library
find_library(MATH m)
if(MATH)
	link_libraries(${MATH})
endif()

# sources
set(QUAKEGENERIC_SOURCES
	${PROJECT_SOURCE_DIR}/source/cd_null.c
	${PROJECT_SOURCE_DIR}/source/chase.c
	${PROJECT_SOURCE_DIR}/source/cl_demo.c
	${PROJECT_SOURCE_DIR}/source/cl_input.c
	${PROJECT_SOURCE_DIR}/source/cl_main.c
	${PROJECT_SOURCE_DIR}/source/cl_parse.c
	${PROJECT_SOURCE_DIR}/source/cl_tent.c
	${PROJECT_SOURCE_DIR}/source/cmd.c
	${PROJECT_SOURCE_DIR}/source/common.c
	${PROJECT_SOURCE_DIR}/source/console.c
	${PROJECT_SOURCE_DIR}/source/crc.c
	${PROJECT_SOURCE_DIR}/source/cvar.c
	${PROJECT_SOURCE_DIR}/source/d_edge.c
	${PROJECT_SOURCE_DIR}/source/d_fill.c
	${PROJECT_SOURCE_DIR}/source/d_init.c
	${PROJECT_SOURCE_DIR}/source/d_modech.c
	${PROJECT_SOURCE_DIR}/source/d_part.c
	${PROJECT_SOURCE_DIR}/source/d_polyse.c
	${PROJECT_SOURCE_DIR}/source/d_scan.c
	${PROJECT_SOURCE_DIR}/source/d_sky.c
	${PROJECT_SOURCE_DIR}/source/d_sprite.c
	${PROJECT_SOURCE_DIR}/source/d_surf.c
	${PROJECT_SOURCE_DIR}/source/d_vars.c
	${PROJECT_SOURCE_DIR}/source/d_zpoint.c
	${PROJECT_SOURCE_DIR}/source/draw.c
	${PROJECT_SOURCE_DIR}/source/host_cmd.c
	${PROJECT_SOURCE_DIR}/source/host.c
	${PROJECT_SOURCE_DIR}/source/in_null.c
	${PROJECT_SOURCE_DIR}/source/keys.c
	${PROJECT_SOURCE_DIR}/source/mathlib.c
	${PROJECT_SOURCE_DIR}/source/menu.c
	${PROJECT_SOURCE_DIR}/source/model.c
	${PROJECT_SOURCE_DIR}/source/net_loop.c
	${PROJECT_SOURCE_DIR}/source/net_main.c
	${PROJECT_SOURCE_DIR}/source/net_none.c
	${PROJECT_SOURCE_DIR}/source/net_vcr.c
	${PROJECT_SOURCE_DIR}/source/nonintel.c
	${PROJECT_SOURCE_DIR}/source/pr_cmds.c
	${PROJECT_SOURCE_DIR}/source/pr_edict.c
	${PROJECT_SOURCE_DIR}/source/pr_exec.c
	${PROJECT_SOURCE_DIR}/source/r_aclip.c
	${PROJECT_SOURCE_DIR}/source/r_alias.c
	${PROJECT_SOURCE_DIR}/source/r_bsp.c
	${PROJECT_SOURCE_DIR}/source/r_draw.c
	${PROJECT_SOURCE_DIR}/source/r_edge.c
	${PROJECT_SOURCE_DIR}/source/r_efrag.c
	${PROJECT_SOURCE_DIR}/source/r_light.c
	${PROJECT_SOURCE_DIR}/source/r_main.c
	${PROJECT_SOURCE_DIR}/source/r_misc.c
	${PROJECT_SOURCE_DIR}/source/r_part.c
	${PROJECT_SOURCE_DIR}/source/r_sky.c
	${PROJECT_SOURCE_DIR}/source/r_sprite.c
	${PROJECT_SOURCE_DIR}/source/r_surf.c
	${PROJECT_SOURCE_DIR}/source/r_vars.c
	${PROJECT_SOURCE_DIR}/source/sbar.c
	${PROJECT_SOURCE_DIR}/source/screen.c
	${PROJECT_SOURCE_DIR}/source/snd_null.c
	${PROJECT_SOURCE_DIR}/source/sv_main.c
	${PROJECT_SOURCE_DIR}/source/sv_move.c
	${PROJECT_SOURCE_DIR}/source/sv_phys.c
	${PROJECT_SOURCE_DIR}/source/sv_user.c
	${PROJECT_SOURCE_DIR}/source/sys_null.c
	${PROJECT_SOURCE_DIR}/source/vid_null.c
	${PROJECT_SOURCE_DIR}/source/view.c
	${PROJECT_SOURCE_DIR}/source/wad.c
	${PROJECT_SOURCE_DIR}/source/world.c
	${PROJECT_SOURCE_DIR}/source/zone.c
	${PROJECT_SOURCE_DIR}/source/quakegeneric.c
)

# library
add_library(quakegeneric STATIC ${QUAKEGENERIC_SOURCES})
