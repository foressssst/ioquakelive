/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_loadso.h>
#define Sys_LoadLibrary(f) SDL_LoadObject(f)
#define Sys_UnloadLibrary(h) SDL_UnloadObject(h)
#define Sys_LoadFunction(h, fn) ((void*)SDL_LoadFunction(h, fn))
#define Sys_LibraryError() SDL_GetError()

void* QDECL Sys_LoadDll(const char* name, qboolean useSystemLib);
