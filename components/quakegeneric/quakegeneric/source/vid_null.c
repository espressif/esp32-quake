/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"

#include "quakegeneric.h"

viddef_t	vid;				// global video state

#define	BASEWIDTH	QUAKEGENERIC_RES_X
#define	BASEHEIGHT	QUAKEGENERIC_RES_Y

static byte	*vid_buffer[2];
static short	*zbuffer;
static byte	*surfcache;
static size_t	surfcache_size;

void	VID_SetPalette (unsigned char *palette)
{
	// quake generic
	QG_SetPalette(palette);
}

void	VID_ShiftPalette (unsigned char *palette)
{
	// quake generic
	QG_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{
	zbuffer = calloc(BASEWIDTH*BASEHEIGHT, sizeof(short));
	vid_buffer[0] = calloc(BASEWIDTH*BASEHEIGHT, sizeof(byte));
	vid_buffer[1] = calloc(BASEWIDTH*BASEHEIGHT, sizeof(byte));
	vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer[0];
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;
	
	d_pzbuffer = zbuffer;

	surfcache_size = D_SurfaceCacheForRes(BASEWIDTH, BASEHEIGHT);
	surfcache = malloc(surfcache_size);
	D_InitCaches (surfcache, surfcache_size);

	// quake generic
	QG_Init();
}

void	VID_Shutdown (void)
{
		free(surfcache);
}

void	VID_Update (vrect_t *rects)
{
	// quake generic
	QG_DrawFrame(vid.buffer);
	//swap buffers
	if (vid.buffer == vid_buffer[0]) {
		vid.buffer = vid_buffer[1];
	} else {
		vid.buffer = vid_buffer[0];
	}
	vid.conbuffer=vid.buffer;
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


