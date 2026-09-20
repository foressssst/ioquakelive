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

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "qgl.h"

typedef enum {
    IMGTYPE_COLORALPHA,  // for color, lightmap, diffuse, and specular
    IMGTYPE_NORMAL,
    IMGTYPE_NORMALHEIGHT,
    IMGTYPE_DELUXE,  // normals are swizzled, deluxe are not
} imgType_t;

typedef enum {
    IMGFLAG_NONE = 0x0000,
    IMGFLAG_MIPMAP = 0x0001,
    IMGFLAG_PICMIP = 0x0002,
    IMGFLAG_CUBEMAP = 0x0004,
    IMGFLAG_NO_COMPRESSION = 0x0010,
    IMGFLAG_NOLIGHTSCALE = 0x0020,
    IMGFLAG_CLAMPTOEDGE = 0x0040,
    IMGFLAG_GENNORMALMAP = 0x0080,
} imgFlags_t;

typedef struct image_s {
    char imgName[MAX_QPATH];        // game path, including extension
    int width, height;              // source image
    int uploadWidth, uploadHeight;  // after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
    GLuint texnum;                  // gl texture binding

    int frameUsed;  // for texture usage in frame statistics

    int internalFormat;
    int TMU;  // only needed for voodoo2

    imgType_t type;
    imgFlags_t flags;

    struct image_s* next;
} image_t;

// any change in the LIGHTMAP_* defines here MUST be reflected in
// R_FindShader() in tr_bsp.c
#define LIGHTMAP_2D -4         // shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX -3  // pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_NONE -1

extern refimport_t ri;
extern glconfig_t glConfig;  // outside of TR since it shouldn't be cleared during ref re-init

// These variables should live inside glConfig but can't because of
// compatibility issues to the original ID vms.  If you release a stand-alone
// game and your mod uses tr_types.h from this build you can safely move them
// to the glconfig_t struct.
extern qboolean textureFilterAnisotropic;
extern int maxAnisotropy;
extern float displayAspect;
extern qboolean haveClampToEdge;

//
// cvars
//
extern cvar_t* r_stencilbits;  // number of desired stencil bits
extern cvar_t* r_depthbits;    // number of desired depth bits
extern cvar_t* r_colorbits;    // number of desired color bits, only relevant for fullscreen
extern cvar_t* r_texturebits;  // number of desired texture bits
extern cvar_t* r_ext_multisample;
// 0 = use framebuffer depth
// 16 = use 16-bit textures
// 32 = use 32-bit textures
// all else = error

extern cvar_t* r_mode;  // video mode
extern cvar_t* r_noborder;
extern cvar_t* r_fullscreen;
extern cvar_t* r_windowedMode;  // [QL] video mode in a window
extern cvar_t* r_windowedWidth;
extern cvar_t* r_windowedHeight;
extern cvar_t* r_ignorehwgamma;  // overrides hardware gamma capabilities
extern cvar_t* r_drawBuffer;
extern cvar_t* r_swapInterval;

extern cvar_t* r_allowExtensions;          // global enable/disable of OpenGL extensions
extern cvar_t* r_ext_compressed_textures;
// [ioquakelive] the map's textures, decoded ahead of time across the workers
extern cvar_t* r_prefetchTextures;
void R_PrefetchStart(const char* listName);
void R_PrefetchFinish(void);
extern cvar_t* r_ext_multitexture;
extern cvar_t* r_ext_compiled_vertex_array;
extern cvar_t* r_ext_texture_env_add;

extern cvar_t* r_ext_texture_filter_anisotropic;
extern cvar_t* r_ext_max_anisotropy;

extern cvar_t* r_stereoEnabled;

extern cvar_t* r_saveFontData;

qboolean R_GetModeInfo(int* width, int* height, float* windowAspect, int* aspectRatio, int mode, qboolean fullscreen);
int R_AspectRatioOf(int width, int height);

float R_NoiseGet4f(float x, float y, float z, double t);
void R_NoiseInit(void);

image_t* R_FindImageFile(const char* name, imgType_t type, imgFlags_t flags);
// [ioquakelive] where image loading time goes: decoding against uploading. r_imageReadUs is
// the part of the decoding that is reading the file out of the pk3, which cannot leave the
// main thread without touching the filesystem
extern int64_t r_imageDecodeUs, r_imageUploadUs, r_imageReadUs;
extern int r_imageCount;
// [ioquakelive] what registering shaders costs, images aside
extern int64_t r_shaderFindUs, r_shaderCachedUs;
extern int r_shaderFindCount, r_shaderCachedCount;
void R_ImageLoadStatsReset(void);
void R_ImageLoadStatsReport(void);
image_t* R_CreateImage(const char* name, byte* pic, int width, int height, imgType_t type, imgFlags_t flags, int internalFormat);

void R_IssuePendingRenderCommands(void);
qhandle_t RE_RegisterShaderLightMap(const char* name, int lightmapIndex);
qhandle_t RE_RegisterShader(const char* name);
qhandle_t RE_RegisterShaderNoMip(const char* name);
qhandle_t RE_RegisterShaderFromImage(const char* name, int lightmapIndex, image_t* image, qboolean mipRawImage);

// font stuff ([QL] fontstash/stb_truetype - see tr_font_gl.c)
void R_InitFonts(void);
void R_FontShutdown(void);
void RE_Font_DrawString(int x, int y, const char* text, int fontIndex, float scale, int limit, float* maxX, int flags);
void RE_TextBounds(const char* text, int start, int fontIndex, float scale, int limit, float* extents);
void RE_GetGlyphInfo(int fontIndex, int charValue, glyphInfo_t* glyph);
void RE_SetCompositionFont(int fontIndex, float scale);
int RE_Font_Register(const char* name);
void R_FontList_f(void);
// dead stub kept for the legacy re.RegisterFont slot
void RE_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font);

/*
=============================================================

IMAGE LOADERS

=============================================================
*/

void R_LoadBMP(const char* name, byte** pic, int* width, int* height);
void R_LoadJPG(const char* name, byte** pic, int* width, int* height);
// [ioquakelive] decodes a JPEG that is already in memory. It exists so that the work can be
// spread across threads: the output buffer is asked for through allocator (the system's malloc
// on a worker, the engine's on the main thread) and silent turns off the messages, which are
// not safe off the main thread.
void R_DecodeJPG(byte* fileData, int fileLen, byte** pic, int* width, int* height,
                 void* (*allocator)(size_t), qboolean silent);
// [ioquakelive] the same for TGA, which is the format of most Quake Live textures. It also
// takes the name, used in its messages, and how to free the memory.
void R_DecodeTGA(byte* fileData, int fileLen, const char* name, byte** pic, int* width,
                 int* height, void* (*allocator)(size_t), void (*deallocator)(void*),
                 qboolean silent);
extern char r_lastImageName[MAX_QPATH];
// [ioquakelive] decodes a PNG that is already in memory; safe to call from a worker
void R_DecodePNG(byte* fileData, int fileLen, const char* name, byte** pic, int* width, int* height);
void R_LoadPCX(const char* name, byte** pic, int* width, int* height);
void R_LoadPNG(const char* name, byte** pic, int* width, int* height);
void R_LoadTGA(const char* name, byte** pic, int* width, int* height);

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void GLimp_Init(qboolean fixedFunction);
void GLimp_Shutdown(void);
void GLimp_EndFrame(void);

void GLimp_LogComment(char* comment);
void GLimp_Minimize(void);

void GLimp_SetGamma(unsigned char red[256],
                    unsigned char green[256],
                    unsigned char blue[256]);

#endif
