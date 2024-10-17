# quakegeneric

![a low-resolution screenshot of quake](./.github/quakegeneric.png)

it's like [doomgeneric](https://github.com/ozkl/doomgeneric), but for quake. it's based on the GPL WinQuake source code.

currently it can only compile for 32-bit architechtures.

## implementations

- [`quakegeneric_null.c`](./source/quakegeneric_null.c) - null
- [`quakegeneric_dos.c`](./source/quakegeneric_dos.c) - MS-DOS
- [`quakegeneric_sdl2.c`](./source/quakegeneric_sdl2.c) - SDL2
- [`quakegeneric_w32.c`](./source/quakegeneric_w32.c) - Win32

## building

on unix-like platforms:

```
cd source/
make
```

for Open Watcom:

```
cd source/
wmake -f makefile.wat
```

for CMake:

```
mkdir cmake-build
cd cmake-build/
cmake ..
make
```

for Meson:

```
mkdir meson-build
meson setup meson-build
cd meson-build/
meson compile
```

for Windows:

```
cd source/
nmake makefile.win
```

## platforms

the following compilers have been tested to work with this source:

- GCC
- Clang
- MinGW
- TinyCC
- Open Watcom
- MSVC

## License

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
