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
#include "tr_local.h"

cvar_t *r_mode;
cvar_t *r_windowedMode;
cvar_t *r_fullscreen;
cvar_t *r_noborder;
cvar_t *r_colorbits;
cvar_t *r_depthbits;
cvar_t *r_stencilbits;
cvar_t *r_stereoEnabled;
cvar_t *r_ignorehwgamma;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_windowedWidth;
cvar_t *r_windowedHeight;
cvar_t *r_customPixelAspect;
cvar_t *r_displayRefresh;
cvar_t *r_swapInterval;
cvar_t *r_uiFullScreen;

float displayAspect;

int64_t r_imageDecodeUs, r_imageUploadUs, r_imageReadUs;
int r_imageCount;

typedef struct vidmode_s {
	const char *description;
	int width, height;
	float pixelAspect;
	int aspectRatio;
} vidmode_t;

static const vidmode_t r_vidModes[] = {
	{ "Mode  0: 320x240", 320, 240, 1, 0 },
	{ "Mode  1: 400x300", 400, 300, 1, 0 },
	{ "Mode  2: 512x384", 512, 384, 1, 0 },
	{ "Mode  3: 640x360", 640, 360, 1, 1 },
	{ "Mode  4: 640x400", 640, 400, 1, 2 },
	{ "Mode  5: 640x480", 640, 480, 1, 0 },
	{ "Mode  6: 800x450", 800, 450, 1, 1 },
	{ "Mode  7: 852x480", 852, 480, 1, 1 },
	{ "Mode  8: 800x500", 800, 500, 1, 2 },
	{ "Mode  9: 800x600", 800, 600, 1, 0 },
	{ "Mode 10: 1024x640", 1024, 640, 1, 2 },
	{ "Mode 11: 1024x576", 1024, 576, 1, 1 },
	{ "Mode 12: 1024x768", 1024, 768, 1, 0 },
	{ "Mode 13: 1152x864", 1152, 864, 1, 0 },
	{ "Mode 14: 1280x720", 1280, 720, 1, 1 },
	{ "Mode 15: 1280x768", 1280, 768, 1, 2 },
	{ "Mode 16: 1280x800", 1280, 800, 1, 2 },
	{ "Mode 17: 1280x1024", 1280, 1024, 1, 3 },
	{ "Mode 18: 1440x900", 1440, 900, 1, 2 },
	{ "Mode 19: 1600x900", 1600, 900, 1, 1 },
	{ "Mode 20: 1600x1000", 1600, 1000, 1, 2 },
	{ "Mode 21: 1680x1050", 1680, 1050, 1, 2 },
	{ "Mode 22: 1600x1200", 1600, 1200, 1, 0 },
	{ "Mode 23: 1920x1080", 1920, 1080, 1, 1 },
	{ "Mode 24: 1920x1200", 1920, 1200, 1, 2 },
	{ "Mode 25: 1920x1440", 1920, 1440, 1, 0 },
	{ "Mode 26: 2048x1536", 2048, 1536, 1, 0 },
	{ "Mode 27: 2560x1600", 2560, 1600, 1, 2 }
};
static const int s_numVidModes = ARRAY_LEN( r_vidModes );

int R_AspectRatioOf( int width, int height ) {
	float aspect = (float)width / height;

	if ( aspect >= 16.0f / 9.0f ) {
		return 1;
	}
	if ( aspect >= 16.0f / 10.0f ) {
		return 2;
	}
	if ( aspect >= 4.0f / 3.0f ) {
		return 0;
	}
	return 3;
}

qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int *aspectRatio, int mode, qboolean fullscreen ) {
	const vidmode_t *vm;
	float pixelAspect;

	if ( mode < -1 || mode >= s_numVidModes ) {
		return qfalse;
	}

	if ( mode == -1 ) {
		if ( fullscreen ) {
			*width = r_customwidth->integer;
			*height = r_customheight->integer;
		} else {
			*width = r_windowedWidth->integer;
			*height = r_windowedHeight->integer;
		}
		pixelAspect = r_customPixelAspect->value;
		*aspectRatio = R_AspectRatioOf( *width, *height );
	} else {
		vm = &r_vidModes[mode];

		*width = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
		*aspectRatio = vm->aspectRatio;
	}

	*windowAspect = (float)*width / ( *height * pixelAspect );

	return qtrue;
}

void R_ModeList_f( void ) {
	int i;

	ri.Printf( PRINT_ALL, "\n" );
	for ( i = 0; i < s_numVidModes; i++ ) {
		ri.Printf( PRINT_ALL, "%s\n", r_vidModes[i].description );
	}
	ri.Printf( PRINT_ALL, "\n" );
}

void R_RegisterWindowCvars( void ) {
	r_mode = ri.Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_windowedMode = ri.Cvar_Get( "r_windowedMode", "12", CVAR_ARCHIVE | CVAR_LATCH | CVAR_REPLICATE | CVAR_USERSAVE );
	r_fullscreen = ri.Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_noborder = ri.Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorbits = ri.Cvar_Get( "r_colorbits", "32", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_depthbits = ri.Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_stencilbits = ri.Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_stereoEnabled = ri.Cvar_Get( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ignorehwgamma = ri.Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_customwidth = ri.Cvar_Get( "r_customWidth", "1600", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_customheight = ri.Cvar_Get( "r_customHeight", "1024", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_windowedWidth = ri.Cvar_Get( "r_windowedWidth", "1600", CVAR_ARCHIVE | CVAR_LATCH );
	r_windowedHeight = ri.Cvar_Get( "r_windowedHeight", "1024", CVAR_ARCHIVE | CVAR_LATCH );
	r_customPixelAspect = ri.Cvar_Get( "r_customPixelAspect", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_displayRefresh = ri.Cvar_Get( "r_displayRefresh", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_swapInterval = ri.Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_USERSAVE );

	r_uiFullScreen = ri.Cvar_Get( "r_uifullscreen", "0", 0 );

}

float Q_atof( const char *str ) {
	return (float)atof( str );
}

char *Q_stradd( char *dst, const char *src ) {
	char c;

	while ( ( c = *src++ ) != '\0' ) {
		*dst++ = c;
	}
	*dst = '\0';
	return dst;
}

unsigned int crc32_buffer( const byte *buf, unsigned int len ) {
	static unsigned int table[256];
	static qboolean inited;
	unsigned int crc = 0xFFFFFFFFu;

	if ( !inited ) {
		unsigned int c;
		int i, j;

		for ( i = 0; i < 256; i++ ) {
			c = i;
			for ( j = 0; j < 8; j++ ) {
				c = ( c & 1 ) ? ( c >> 1 ) ^ 0xEDB88320u : c >> 1;
			}
			table[i] = c;
		}
		inited = qtrue;
	}

	while ( len-- ) {
		crc = table[( crc ^ *buf++ ) & 0xFF] ^ ( crc >> 8 );
	}

	return crc ^ 0xFFFFFFFFu;
}

unsigned long Com_GenerateHashValue( const char *fname, const unsigned int size ) {
	unsigned long hash = 0;
	const byte *s = (const byte *)fname;
	int c;

	while ( ( c = *s++ ) != '\0' ) {
		if ( c >= 'A' && c <= 'Z' ) {
			c += 'a' - 'A';
		} else if ( c == '\\' ) {
			c = '/';
		}
		hash = hash * 101 + c;
	}

	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	hash &= ( size - 1 );

	return hash;
}

int Com_Split( char *in, char **out, int outsz, int delim ) {
	char **o = out, **end = out + outsz;
	int c;

	if ( delim >= ' ' ) {
		while ( ( c = *in ) != '\0' && c <= ' ' ) {
			in++;
		}
	}
	*out = in;
	out++;
	while ( out < end ) {
		while ( ( c = *in ) != '\0' && c != delim ) {
			in++;
		}
		*in = '\0';
		if ( !c ) {
			if ( out[-1][0] == '\0' ) {
				out--;
			}
			break;
		}
		in++;
		if ( delim >= ' ' ) {
			while ( ( c = *in ) != '\0' && c <= ' ' ) {
				in++;
			}
		}
		*out = in;
		out++;
	}
	while ( ( c = *in ) != '\0' && c != delim ) {
		in++;
	}
	*in = '\0';
	c = out - o;
	while ( out < end ) {
		*out = in;
		out++;
	}
	return c;
}
