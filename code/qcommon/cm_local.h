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
#include "q_shared.h"
#include "qcommon.h"
#include "cm_polylib.h"

#define MAX_SUBMODELS 256
#define BOX_MODEL_HANDLE 255
#define CAPSULE_MODEL_HANDLE 254

typedef struct {
    cplane_t* plane;
    int children[2];
} cNode_t;

typedef struct {
    int cluster;
    int area;

    int firstLeafBrush;
    int numLeafBrushes;

    int firstLeafSurface;
    int numLeafSurfaces;
} cLeaf_t;

typedef struct cmodel_s {
    vec3_t mins, maxs;
    cLeaf_t leaf;
} cmodel_t;

typedef struct {
    cplane_t* plane;
    int surfaceFlags;
    int shaderNum;
} cbrushside_t;

typedef struct {
    int shaderNum;
    int contents;
    vec3_t bounds[2];
    int numsides;
    cbrushside_t* sides;
    int checkcount;
} cbrush_t;

typedef struct {
    int checkcount;
    int surfaceFlags;
    int contents;
    struct patchCollide_s* pc;
} cPatch_t;

typedef struct {
    int floodnum;
    int floodvalid;
} cArea_t;

typedef struct {
    char name[MAX_QPATH];

    int numShaders;
    dshader_t* shaders;

    int numBrushSides;
    cbrushside_t* brushsides;

    int numPlanes;
    cplane_t* planes;

    int numNodes;
    cNode_t* nodes;

    int numLeafs;
    cLeaf_t* leafs;

    int numLeafBrushes;
    int* leafbrushes;

    int numLeafSurfaces;
    int* leafsurfaces;

    int numSubModels;
    cmodel_t* cmodels;

    int numBrushes;
    cbrush_t* brushes;

    int numClusters;
    int clusterBytes;
    byte* visibility;
    qboolean vised;

    int numEntityChars;
    char* entityString;

    int numAreas;
    cArea_t* areas;
    int* areaPortals;

    int numSurfaces;
    cPatch_t** surfaces;

    int floodvalid;
    int checkcount;
} clipMap_t;

#define SURFACE_CLIP_EPSILON (0.125)

extern clipMap_t cm;
extern int c_pointcontents;
extern int c_traces, c_brush_traces, c_patch_traces;
extern cvar_t* cm_noAreas;
extern cvar_t* cm_noCurves;
extern cvar_t* cm_playerCurveClip;

typedef struct
{
    qboolean use;
    float radius;
    float halfheight;
    vec3_t offset;
} sphere_t;

typedef struct {
    vec3_t start;
    vec3_t end;
    vec3_t size[2];
    vec3_t offsets[8];
    float maxOffset;
    vec3_t extents;
    vec3_t bounds[2];
    vec3_t modelOrigin;
    int contents;
    qboolean isPoint;
    trace_t trace;
    sphere_t sphere;
} traceWork_t;

typedef struct leafList_s {
    int count;
    int maxcount;
    qboolean overflowed;
    int* list;
    vec3_t bounds[2];
    int lastLeaf;
    void (*storeLeafs)(struct leafList_s* ll, int nodenum);
} leafList_t;

int CM_BoxBrushes(const vec3_t mins, const vec3_t maxs, cbrush_t** list, int listsize);

void CM_StoreLeafs(leafList_t* ll, int nodenum);
void CM_StoreBrushes(leafList_t* ll, int nodenum);

void CM_BoxLeafnums_r(leafList_t* ll, int nodenum);

cmodel_t* CM_ClipHandleToModel(clipHandle_t handle);
qboolean CM_BoundsIntersect(const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2);
qboolean CM_BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs, const vec3_t point);

struct patchCollide_s* CM_GeneratePatchCollide(int width, int height, vec3_t* points);
void CM_TraceThroughPatchCollide(traceWork_t* tw, const struct patchCollide_s* pc);
qboolean CM_PositionTestInPatchCollide(traceWork_t* tw, const struct patchCollide_s* pc);
void CM_ClearLevelPatches(void);