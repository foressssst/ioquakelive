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
#ifndef __QFILES_H__
#define __QFILES_H__

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#define SHADER_MAX_VERTEXES 4096
#define SHADER_MAX_INDEXES (6 * SHADER_MAX_VERTEXES)

#define MAX_QPATH 64

#define MAX_CAMERAPOINTS 512
#define MAX_SPLINEPOINTS (MAX_CAMERAPOINTS * 40)

#define MAX_MAP_ADVERTISEMENTS 30

#define MD3_IDENT (('3' << 24) + ('P' << 16) + ('D' << 8) + 'I')
#define MD3_VERSION 15

#define MD3_MAX_LODS 3
#define MD3_MAX_TRIANGLES 8192
#define MD3_MAX_VERTS 4096
#define MD3_MAX_SHADERS 256
#define MD3_MAX_FRAMES 1024
#define MD3_MAX_SURFACES 32
#define MD3_MAX_TAGS 16

#define MD3_XYZ_SCALE (1.0 / 64)

typedef struct md3Frame_s {
    vec3_t bounds[2];
    vec3_t localOrigin;
    float radius;
    char name[16];
} md3Frame_t;

typedef struct md3Tag_s {
    char name[MAX_QPATH];
    vec3_t origin;
    vec3_t axis[3];
} md3Tag_t;

typedef struct {
    int ident;

    char name[MAX_QPATH];

    int flags;
    int numFrames;

    int numShaders;
    int numVerts;

    int numTriangles;
    int ofsTriangles;

    int ofsShaders;
    int ofsSt;
    int ofsXyzNormals;

    int ofsEnd;
} md3Surface_t;

typedef struct {
    char name[MAX_QPATH];
    int shaderIndex;
} md3Shader_t;

typedef struct {
    int indexes[3];
} md3Triangle_t;

typedef struct {
    float st[2];
} md3St_t;

typedef struct {
    short xyz[3];
    short normal;
} md3XyzNormal_t;

typedef struct {
    int ident;
    int version;

    char name[MAX_QPATH];

    int flags;

    int numFrames;
    int numTags;
    int numSurfaces;

    int numSkins;

    int ofsFrames;
    int ofsTags;
    int ofsSurfaces;

    int ofsEnd;
} md3Header_t;

#define MDR_IDENT (('5' << 24) + ('M' << 16) + ('D' << 8) + 'R')
#define MDR_VERSION 2
#define MDR_MAX_BONES 128

typedef struct {
    int boneIndex;
    float boneWeight;
    vec3_t offset;
} mdrWeight_t;

typedef struct {
    vec3_t normal;
    vec2_t texCoords;
    int numWeights;
    mdrWeight_t weights[1];
} mdrVertex_t;

typedef struct {
    int indexes[3];
} mdrTriangle_t;

typedef struct {
    int ident;

    char name[MAX_QPATH];
    char shader[MAX_QPATH];
    int shaderIndex;

    int ofsHeader;

    int numVerts;
    int ofsVerts;

    int numTriangles;
    int ofsTriangles;

    int numBoneReferences;
    int ofsBoneReferences;

    int ofsEnd;
} mdrSurface_t;

typedef struct {
    float matrix[3][4];
} mdrBone_t;

typedef struct {
    vec3_t bounds[2];
    vec3_t localOrigin;
    float radius;
    char name[16];
    mdrBone_t bones[1];
} mdrFrame_t;

typedef struct {
    unsigned char Comp[24];
} mdrCompBone_t;

typedef struct {
    vec3_t bounds[2];
    vec3_t localOrigin;
    float radius;
    mdrCompBone_t bones[1];
} mdrCompFrame_t;

typedef struct {
    int numSurfaces;
    int ofsSurfaces;
    int ofsEnd;
} mdrLOD_t;

typedef struct {
    int boneIndex;
    char name[32];
} mdrTag_t;

typedef struct {
    int ident;
    int version;

    char name[MAX_QPATH];

    int numFrames;
    int numBones;
    int ofsFrames;

    int numLODs;
    int ofsLODs;

    int numTags;
    int ofsTags;

    int ofsEnd;
} mdrHeader_t;

#define BSP_IDENT (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I')

#define BSP_VERSION 47

#define MAX_MAP_MODELS 0x400
#define MAX_MAP_BRUSHES 0x8000
#define MAX_MAP_ENTITIES 0x800
#define MAX_MAP_ENTSTRING 0x40000
#define MAX_MAP_SHADERS 0x400

#define MAX_MAP_AREAS 0x100
#define MAX_MAP_FOGS 0x100
#define MAX_MAP_PLANES 0x20000
#define MAX_MAP_NODES 0x20000
#define MAX_MAP_BRUSHSIDES 0x20000
#define MAX_MAP_LEAFS 0x20000
#define MAX_MAP_LEAFFACES 0x20000
#define MAX_MAP_LEAFBRUSHES 0x40000
#define MAX_MAP_PORTALS 0x20000
#define MAX_MAP_LIGHTING 0x800000
#define MAX_MAP_LIGHTGRID 0x800000
#define MAX_MAP_VISIBILITY 0x200000

#define MAX_MAP_DRAW_SURFS 0x20000
#define MAX_MAP_DRAW_VERTS 0x80000
#define MAX_MAP_DRAW_INDEXES 0x80000

#define MAX_KEY 32
#define MAX_VALUE 1024

#define ANGLE_UP -1
#define ANGLE_DOWN -2

#define LIGHTMAP_WIDTH 128
#define LIGHTMAP_HEIGHT 128

#define MAX_WORLD_COORD (128 * 1024)
#define MIN_WORLD_COORD (-128 * 1024)
#define WORLD_SIZE (MAX_WORLD_COORD - MIN_WORLD_COORD)

typedef struct {
    int fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES 0
#define LUMP_SHADERS 1
#define LUMP_PLANES 2
#define LUMP_NODES 3
#define LUMP_LEAFS 4
#define LUMP_LEAFSURFACES 5
#define LUMP_LEAFBRUSHES 6
#define LUMP_MODELS 7
#define LUMP_BRUSHES 8
#define LUMP_BRUSHSIDES 9
#define LUMP_DRAWVERTS 10
#define LUMP_DRAWINDEXES 11
#define LUMP_FOGS 12
#define LUMP_SURFACES 13
#define LUMP_LIGHTMAPS 14
#define LUMP_LIGHTGRID 15
#define LUMP_VISIBILITY 16
#define LUMP_ADVERTISEMENTS 17
#define HEADER_LUMPS 18

typedef struct {
    int ident;
    int version;

    lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
    float mins[3], maxs[3];
    int firstSurface, numSurfaces;
    int firstBrush, numBrushes;
} dmodel_t;

typedef struct {
    char shader[MAX_QPATH];
    int surfaceFlags;
    int contentFlags;
} dshader_t;

typedef struct {
    float normal[3];
    float dist;
} dplane_t;

typedef struct {
    int planeNum;
    int children[2];
    int mins[3];
    int maxs[3];
} dnode_t;

typedef struct {
    int cluster;
    int area;

    int mins[3];
    int maxs[3];

    int firstLeafSurface;
    int numLeafSurfaces;

    int firstLeafBrush;
    int numLeafBrushes;
} dleaf_t;

typedef struct {
    int planeNum;
    int shaderNum;
} dbrushside_t;

typedef struct {
    int firstSide;
    int numSides;
    int shaderNum;
} dbrush_t;

typedef struct {
    char shader[MAX_QPATH];
    int brushNum;
    int visibleSide;
} dfog_t;

typedef struct {
    vec3_t xyz;
    float st[2];
    float lightmap[2];
    vec3_t normal;
    byte color[4];
} drawVert_t;

#define drawVert_t_cleared(x) drawVert_t(x) = {{0, 0, 0}, {0, 0}, {0, 0}, {0, 0, 0}, {0, 0, 0, 0}}

typedef enum {
    MST_BAD,
    MST_PLANAR,
    MST_PATCH,
    MST_TRIANGLE_SOUP,
    MST_FLARE
} mapSurfaceType_t;

typedef struct {
    int shaderNum;
    int fogNum;
    int surfaceType;

    int firstVert;
    int numVerts;

    int firstIndex;
    int numIndexes;

    int lightmapNum;
    int lightmapX, lightmapY;
    int lightmapWidth, lightmapHeight;

    vec3_t lightmapOrigin;
    vec3_t lightmapVecs[3];

    int patchWidth;
    int patchHeight;
} dsurface_t;

typedef struct {
    int cellId;
    vec3_t normal;
    vec3_t rect[4];
    char model[MAX_QPATH];
} dadvertisement_t;

#endif
