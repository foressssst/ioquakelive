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
#ifndef __TR_TYPES_H
#define __TR_TYPES_H

#define MAX_DLIGHTS 32

#define REFENTITYNUM_BITS 10
#define REFENTITYNUM_MASK ((1 << REFENTITYNUM_BITS) - 1)
#define MAX_REFENTITIES ((1 << REFENTITYNUM_BITS) - 1)
#define REFENTITYNUM_WORLD ((1 << REFENTITYNUM_BITS) - 1)

#define RF_MINLIGHT 0x0001
#define RF_THIRD_PERSON 0x0002
#define RF_FIRST_PERSON 0x0004
#define RF_DEPTHHACK 0x0008

#define RF_CROSSHAIR 0x0010

#define RF_NOSHADOW 0x0040

#define RF_LIGHTING_ORIGIN 0x0080

#define RF_SHADOW_PLANE 0x0100
#define RF_WRAP_FRAMES 0x0200

#define RDF_NOWORLDMODEL 0x0001
#define RDF_HYPERSPACE 0x0004

typedef struct {
    vec3_t xyz;
    float st[2];
    byte modulate[4];
} polyVert_t;

typedef struct poly_s {
    qhandle_t hShader;
    int numVerts;
    polyVert_t* verts;
} poly_t;

typedef enum {
    RT_MODEL,
    RT_POLY,
    RT_SPRITE,
    RT_BEAM,
    RT_RAIL_CORE,
    RT_RAIL_RINGS,
    RT_LIGHTNING,
    RT_PORTALSURFACE,

    RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct {
    refEntityType_t reType;
    int renderfx;

    qhandle_t hModel;

    vec3_t lightingOrigin;
    float shadowPlane;

    vec3_t axis[3];
    qboolean nonNormalizedAxes;
    float origin[3];
    int frame;

    float oldorigin[3];
    int oldframe;
    float backlerp;

    int skinNum;
    qhandle_t customSkin;
    qhandle_t customShader;

    byte shaderRGBA[4];
    float shaderTexCoord[2];
    float shaderTime;

    float radius;
    float rotation;
} refEntity_t;

#define MAX_RENDER_STRINGS 8
#define MAX_RENDER_STRING_LENGTH 32

typedef struct {
    int x, y, width, height;
    float fov_x, fov_y;
    vec3_t vieworg;
    vec3_t viewaxis[3];

    int time;

    int rdflags;

    byte areamask[MAX_MAP_AREA_BYTES];

    char text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
} refdef_t;

typedef enum {
    STEREO_CENTER,
    STEREO_LEFT,
    STEREO_RIGHT
} stereoFrame_t;

typedef enum {
    TC_NONE,
    TC_S3TC,
    TC_S3TC_ARB
} textureCompression_t;

typedef enum {
    GLDRV_ICD,
    GLDRV_STANDALONE,
    GLDRV_VOODOO
} glDriverType_t;

typedef enum {
    GLHW_GENERIC,
    GLHW_3DFX_2D3D,
    GLHW_RIVA128,
    GLHW_RAGEPRO,
    GLHW_PERMEDIA2
} glHardwareType_t;

typedef struct {
    char renderer_string[MAX_STRING_CHARS];
    char vendor_string[MAX_STRING_CHARS];
    char version_string[MAX_STRING_CHARS];
    char extensions_string[BIG_INFO_STRING];

    int maxTextureSize;
    int numTextureUnits;

    int colorBits, depthBits, stencilBits;

    glDriverType_t driverType;
    glHardwareType_t hardwareType;

    qboolean deviceSupportsGamma;
    textureCompression_t textureCompression;
    qboolean textureEnvAddAvailable;

    int vidWidth, vidHeight;
    float windowAspect;

    int displayFrequency;

    qboolean isFullscreen;
    qboolean stereoEnabled;
    qboolean smpActive;
} glconfig_t;

#endif
