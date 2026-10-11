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
#ifndef TR_COMMON_H
#define TR_COMMON_H

#define USE_VULKAN

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

typedef union {
	byte rgba[4];
	uint32_t u32;
} color4ub_t;

#ifndef MAX_VIDEO_HANDLES
#define MAX_VIDEO_HANDLES 16
#endif

#ifndef MAX_UINT
#define MAX_UINT ((unsigned)(~0))
#endif

#ifndef VIS_HEADER
#define VIS_HEADER 8
#endif

#ifndef CONTENTS_NODE
#define CONTENTS_NODE 0xFFFFFFFF
#endif

#ifndef CVAR_ARCHIVE_ND
#define CVAR_ARCHIVE_ND CVAR_ARCHIVE
#endif
#ifndef Vector4Set
#define Vector4Set( v, x, y, z, w ) ( (v)[0]=(x), (v)[1]=(y), (v)[2]=(z), (v)[3]=(w) )
#endif

#ifndef S_COLOR_WARNING
#define S_COLOR_WARNING S_COLOR_YELLOW
#endif

#ifndef CVAR_DEVELOPER
#define CVAR_DEVELOPER 0
#endif

#ifndef myftol
#define myftol( x ) ( (int)( x ) )
#endif
#ifndef SGN
#define SGN( x ) ( ( ( x ) >= 0 ) ? !!( x ) : -1 )
#endif
#ifndef DotProduct4
#define DotProduct4( a, b ) ( (a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2] + (a)[3]*(b)[3] )
#endif
#ifndef VectorScale4
#define VectorScale4( a, b, c ) ( (c)[0]=(a)[0]*(b), (c)[1]=(a)[1]*(b), (c)[2]=(a)[2]*(b), (c)[3]=(a)[3]*(b) )
#endif
#ifndef VectorSet4
#define VectorSet4( v, x, y, z, w ) ( (v)[0]=(x), (v)[1]=(y), (v)[2]=(z), (v)[3]=(w) )
#endif

static ID_INLINE unsigned int log2pad( unsigned int v, int roundup ) {
	unsigned int x = 1;

	while ( x < v ) {
		x <<= 1;
	}
	if ( roundup == 0 && x > v ) {
		x >>= 1;
	}
	return x;
}

extern cvar_t *r_mode;
extern cvar_t *r_windowedMode;
extern cvar_t *r_fullscreen;
extern cvar_t *r_noborder;
extern cvar_t *r_colorbits;
extern cvar_t *r_depthbits;
extern cvar_t *r_stencilbits;
extern cvar_t *r_stereoEnabled;
extern cvar_t *r_ignorehwgamma;
extern cvar_t *r_customwidth;
extern cvar_t *r_customheight;
extern cvar_t *r_windowedWidth;
extern cvar_t *r_windowedHeight;
extern cvar_t *r_customPixelAspect;
extern cvar_t *r_displayRefresh;
extern cvar_t *r_swapInterval;
extern cvar_t *r_uiFullScreen;
extern float displayAspect;
extern int64_t r_imageDecodeUs, r_imageUploadUs, r_imageReadUs;
extern int r_imageCount;
extern int64_t r_shaderFindUs, r_shaderCachedUs;
extern int r_shaderFindCount, r_shaderCachedCount;
void R_ImageLoadStatsReset( void );
void R_ImageLoadStatsReport( void );

extern cvar_t *r_prefetchTextures;
void R_PrefetchStart( const char *listName );
void R_PrefetchFinish( void );

void R_DecodeJPG( byte *fileData, int fileLen, byte **pic, int *width, int *height,
	void *(*allocator)( size_t ), qboolean silent );
void R_DecodeTGA( byte *fileData, int fileLen, const char *name, byte **pic, int *width,
	int *height, void *(*allocator)( size_t ), void (*deallocator)( void * ), qboolean silent );
void R_DecodePNG( byte *fileData, int fileLen, const char *name, byte **pic, int *width, int *height );
extern char r_lastImageName[MAX_QPATH];

void R_RegisterWindowCvars( void );
void R_ModeList_f( void );
int R_AspectRatioOf( int width, int height );
qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int *aspectRatio, int mode, qboolean fullscreen );
float Q_atof( const char *str );
char *Q_stradd( char *dst, const char *src );
unsigned int crc32_buffer( const byte *buf, unsigned int len );
unsigned long Com_GenerateHashValue( const char *fname, const unsigned int size );
int Com_Split( char *in, char **out, int outsz, int delim );

extern refimport_t ri;

typedef enum {
	CV_NONE = 0,
	CV_FLOAT,
	CV_INTEGER,
	CV_FSPATH,
	CV_MAX,
} cvarValidator_t;

typedef enum {
	REF_KEEP_CONTEXT,
	REF_KEEP_WINDOW,
	REF_DESTROY_WINDOW,
	REF_UNLOAD_DLL
} refShutdownCode_t;

typedef enum {
	CVG_NONE = 0,
	CVG_RENDERER,
	CVG_SERVER,
	CVG_MAX,
} cvarGroup_t;

void RI_Cvar_SetGroup( cvar_t *cv, cvarGroup_t group );
int RI_Cvar_CheckGroup( cvarGroup_t group );
void RI_Cvar_ResetGroup( cvarGroup_t group, qboolean resetModifiedFlags );
void RI_Cvar_ClearGroups( void );
void RI_Cvar_CheckRange( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type );
int RI_Com_RealTime( qtime_t *qtime );
qboolean RI_CL_IsMinimized( void );
void RI_CL_SetScaling( float factor, int captureWidth, int captureHeight );

void GLimp_Init( qboolean fixedFunction );
qboolean GLimp_ApplyVideoMode( int *width, int *height );
void GLimp_Shutdown( void );
void GLimp_EndFrame( void );
void GLimp_Minimize( void );
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
void *GLimp_GetProcAddress( const char *name );

void RE_SaveJPG( char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
size_t RE_SaveJPGToBuffer( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );

#define MAX_TEXTURE_UNITS 8

typedef enum
{
	IMGFLAG_NONE           = 0x0000,
	IMGFLAG_MIPMAP         = 0x0001,
	IMGFLAG_PICMIP         = 0x0002,
	IMGFLAG_CLAMPTOEDGE    = 0x0004,
	IMGFLAG_CLAMPTOBORDER  = 0x0008,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE   = 0x0020,
	IMGFLAG_LIGHTMAP       = 0x0040,
	IMGFLAG_NOSCALE        = 0x0080,
	IMGFLAG_RGB            = 0x0100,
	IMGFLAG_COLORSHIFT     = 0x0200,
} imgFlags_t;

typedef enum {
	CT_FRONT_SIDED = 0,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

typedef struct image_s image_t;

#define LIGHTMAP_2D         -4
#define LIGHTMAP_BY_VERTEX  -3
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_NONE       -1

extern glconfig_t	glConfig;

extern qboolean  textureFilterAnisotropic;
extern int       maxAnisotropy;

extern cvar_t *r_texturebits;

extern cvar_t *r_drawBuffer;

extern cvar_t *r_allowExtensions;
extern cvar_t *r_ext_compressed_textures;
extern cvar_t *r_ext_multitexture;
extern cvar_t *r_ext_compiled_vertex_array;
extern cvar_t *r_ext_texture_env_add;

extern cvar_t *r_ext_texture_filter_anisotropic;
extern cvar_t *r_ext_max_anisotropy;

float R_NoiseGet4f( float x, float y, float z, double t );
void  R_NoiseInit( void );

image_t *R_FindImageFile( const char *name, imgFlags_t flags );
image_t *R_CreateImage( const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags );
void R_UploadSubImage( byte *data, int x, int y, int width, int height, image_t *image );

qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex );
qhandle_t RE_RegisterShader( const char *name );
qhandle_t RE_RegisterShaderNoMip( const char *name );
qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage);

void R_InitFreeType( void );
void R_DoneFreeType( void );
void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);

void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadPCX( const char *name, byte **pic, int *width, int *height );
void R_LoadPNG( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );

#endif
