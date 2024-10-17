CC=wcc386

CFLAGS=-omaxnet -zp4 -5r -fp3

OBJECTS=&
	cd_null.o&
	chase.o&
	cl_demo.o&
	cl_input.o&
	cl_main.o&
	cl_parse.o&
	cl_tent.o&
	cmd.o&
	common.o&
	console.o&
	crc.o&
	cvar.o&
	d_edge.o&
	d_fill.o&
	d_init.o&
	d_modech.o&
	d_part.o&
	d_polyse.o&
	d_scan.o&
	d_sky.o&
	d_sprite.o&
	d_surf.o&
	d_vars.o&
	d_zpoint.o&
	draw.o&
	host_cmd.o&
	host.o&
	in_null.o&
	keys.o&
	mathlib.o&
	menu.o&
	model.o&
	net_loop.o&
	net_main.o&
	net_none.o&
	net_vcr.o&
	nonintel.o&
	pr_cmds.o&
	pr_edict.o&
	pr_exec.o&
	r_aclip.o&
	r_alias.o&
	r_bsp.o&
	r_draw.o&
	r_edge.o&
	r_efrag.o&
	r_light.o&
	r_main.o&
	r_misc.o&
	r_part.o&
	r_sky.o&
	r_sprite.o&
	r_surf.o&
	r_vars.o&
	sbar.o&
	screen.o&
	snd_null.o&
	sv_main.o&
	sv_move.o&
	sv_phys.o&
	sv_user.o&
	sys_null.o&
	vid_null.o&
	view.o&
	wad.o&
	world.o&
	zone.o&
	quakegeneric.o&
	quakegeneric_dos.o

.c.o:
	$(CC) $(CFLAGS) $*.c

quakegeneric.exe: $(OBJECTS) wlink.rsp
	wlink @wlink.rsp

wlink.rsp: makefile.wat
	echo name quakegeneric.exe >wlink.rsp
	echo system dos4g >>wlink.rsp
	echo option dosseg,caseexact,quiet,stack=64k >>wlink.rsp
	for %i in ($(OBJECTS)) do echo file %i >>wlink.rsp
