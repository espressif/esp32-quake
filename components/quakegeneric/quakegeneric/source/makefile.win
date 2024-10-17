!include <ntwin32.mak>

all: winquake.exe

winquake.exe: cd_null.obj \
	chase.obj \
	cl_demo.obj \
	cl_input.obj \
	cl_main.obj \
	cl_parse.obj \
	cl_tent.obj \
	cmd.obj \
	common.obj \
	console.obj \
	crc.obj \
	cvar.obj \
	d_edge.obj \
	d_fill.obj \
	d_init.obj \
	d_modech.obj \
	d_part.obj \
	d_polyse.obj \
	d_scan.obj \
	d_sky.obj \
	d_sprite.obj \
	d_surf.obj \
	d_vars.obj \
	d_zpoint.obj \
	draw.obj \
	host_cmd.obj \
	host.obj \
	in_null.obj \
	keys.obj \
	mathlib.obj \
	menu.obj \
	model.obj \
	net_loop.obj \
	net_main.obj \
	net_none.obj \
	net_vcr.obj \
	nonintel.obj \
	pr_cmds.obj \
	pr_edict.obj \
	pr_exec.obj \
	r_aclip.obj \
	r_alias.obj \
	r_bsp.obj \
	r_draw.obj \
	r_edge.obj \
	r_efrag.obj \
	r_light.obj \
	r_main.obj \
	r_misc.obj \
	r_part.obj \
	r_sky.obj \
	r_sprite.obj \
	r_surf.obj \
	r_vars.obj \
	sbar.obj \
	screen.obj \
	snd_null.obj \
	sv_main.obj \
	sv_move.obj \
	sv_phys.obj \
	sv_user.obj \
	sys_null.obj \
	vid_null.obj \
	view.obj \
	wad.obj \
	world.obj \
	zone.obj \
	quakegeneric.obj \
	quakegeneric_w32.obj
  $(link) $(ldebug) $(conlflags) -out:$*.exe $** $(conlibs) \
    winmm.lib user32.lib gdi32.lib kernel32.lib

{src}.c{src}.obj:
  $(CC) /O2 /Zi /FA /nologo $(CFLAGS) /c /Fosrc\ $<
