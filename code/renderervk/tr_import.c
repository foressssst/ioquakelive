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
//
// tr_import.c -- [ioquakelive] what quake3e's engine gives its renderer and this one doesn't
//

#include "tr_local.h"

#ifdef USE_LOCAL_HEADERS
#include "SDL.h"
#else
#include <SDL.h>
#endif
#include <time.h>

extern SDL_Window* SDL_window;

/*
===============================================================================

CVAR GROUPS

A group is modified when one of its cvars changed since the group was last reset. The
engine's cvars keep a modification count, so the group only has to remember the counts it
saw.

===============================================================================
*/

#define MAX_GROUPED_CVARS 512

typedef struct {
	cvar_t *cv;
	cvarGroup_t group;
	int seen;
} groupedCvar_t;

static groupedCvar_t groupedCvars[MAX_GROUPED_CVARS];
static int numGroupedCvars;

void RI_Cvar_SetGroup( cvar_t *cv, cvarGroup_t group ) {
	int i;

	if ( !cv ) {
		return;
	}
	for ( i = 0; i < numGroupedCvars; i++ ) {
		if ( groupedCvars[i].cv == cv ) {
			groupedCvars[i].group = group;
			return;
		}
	}
	if ( numGroupedCvars == MAX_GROUPED_CVARS ) {
		ri.Printf( PRINT_WARNING, "WARNING: too many grouped cvars, %s left out\n", cv->name );
		return;
	}
	groupedCvars[numGroupedCvars].cv = cv;
	groupedCvars[numGroupedCvars].group = group;
	groupedCvars[numGroupedCvars].seen = cv->modificationCount;
	numGroupedCvars++;
}

int RI_Cvar_CheckGroup( cvarGroup_t group ) {
	int i;

	for ( i = 0; i < numGroupedCvars; i++ ) {
		if ( groupedCvars[i].group == group && groupedCvars[i].cv->modificationCount != groupedCvars[i].seen ) {
			return 1;
		}
	}
	return 0;
}

void RI_Cvar_ResetGroup( cvarGroup_t group, qboolean resetModifiedFlags ) {
	int i;

	for ( i = 0; i < numGroupedCvars; i++ ) {
		if ( groupedCvars[i].group == group ) {
			groupedCvars[i].seen = groupedCvars[i].cv->modificationCount;
			if ( resetModifiedFlags ) {
				groupedCvars[i].cv->modified = qfalse;
			}
		}
	}
}

// the renderer is unloaded and loaded again on vid_restart, and its cvars with it
void RI_Cvar_ClearGroups( void ) {
	numGroupedCvars = 0;
}

/*
=================
RI_Cvar_CheckRange

Quake3e gives the range as strings, either end possibly missing, and says what kind of
value it is; the engine takes numbers and whether they have to be whole.
=================
*/
void RI_Cvar_CheckRange( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type ) {
	float lo, hi;

	if ( !cv || ( type != CV_INTEGER && type != CV_FLOAT ) ) {
		return;
	}
	lo = minVal ? (float)atof( minVal ) : -1e30f;
	hi = maxVal ? (float)atof( maxVal ) : 1e30f;
	ri.Cvar_CheckRange( cv, lo, hi, type == CV_INTEGER ? qtrue : qfalse );
}

/*
===============================================================================

THE REST

===============================================================================
*/

int RI_Com_RealTime( qtime_t *qtime ) {
	time_t t;
	struct tm *tms;

	t = time( NULL );
	if ( !qtime ) {
		return (int)t;
	}
	tms = localtime( &t );
	if ( tms ) {
		qtime->tm_sec = tms->tm_sec;
		qtime->tm_min = tms->tm_min;
		qtime->tm_hour = tms->tm_hour;
		qtime->tm_mday = tms->tm_mday;
		qtime->tm_mon = tms->tm_mon;
		qtime->tm_year = tms->tm_year;
		qtime->tm_wday = tms->tm_wday;
		qtime->tm_yday = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}
	return (int)t;
}

qboolean RI_CL_IsMinimized( void ) {
	return ( SDL_window && ( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_MINIMIZED ) ) ? qtrue : qfalse;
}

// Quake3e's client scales the pointer and the captures when the scene is drawn at another
// size than the window; nothing here draws at another size yet
void RI_CL_SetScaling( float factor, int captureWidth, int captureHeight ) {
	(void)factor;
	(void)captureWidth;
	(void)captureHeight;
}
