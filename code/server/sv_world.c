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
#include "server.h"

clipHandle_t SV_ClipHandleForEntity(const sharedEntity_t* ent) {
    if (ent->r.bmodel) {
        return CM_InlineModel(ent->s.modelindex);
    }
    if (ent->r.svFlags & SVF_CAPSULE) {
        return CM_TempBoxModel(ent->r.mins, ent->r.maxs, qtrue);
    }

    return CM_TempBoxModel(ent->r.mins, ent->r.maxs, qfalse);
}

typedef struct worldSector_s {
    int axis;
    float dist;
    struct worldSector_s* children[2];
    svEntity_t* entities;
} worldSector_t;

#define AREA_DEPTH 4
#define AREA_NODES 64

worldSector_t sv_worldSectors[AREA_NODES];
int sv_numworldSectors;

void SV_SectorList_f(void) {
    int i, c;
    worldSector_t* sec;
    svEntity_t* ent;

    for (i = 0; i < AREA_NODES; i++) {
        sec = &sv_worldSectors[i];

        c = 0;
        for (ent = sec->entities; ent; ent = ent->nextEntityInWorldSector) {
            c++;
        }
        Com_Printf("sector %i: %i entities\n", i, c);
    }
}

static worldSector_t* SV_CreateworldSector(int depth, vec3_t mins, vec3_t maxs) {
    worldSector_t* anode;
    vec3_t size;
    vec3_t mins1, maxs1, mins2, maxs2;

    anode = &sv_worldSectors[sv_numworldSectors];
    sv_numworldSectors++;

    if (depth == AREA_DEPTH) {
        anode->axis = -1;
        anode->children[0] = anode->children[1] = NULL;
        return anode;
    }

    VectorSubtract(maxs, mins, size);
    if (size[0] > size[1]) {
        anode->axis = 0;
    } else {
        anode->axis = 1;
    }

    anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
    VectorCopy(mins, mins1);
    VectorCopy(mins, mins2);
    VectorCopy(maxs, maxs1);
    VectorCopy(maxs, maxs2);

    maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

    anode->children[0] = SV_CreateworldSector(depth + 1, mins2, maxs2);
    anode->children[1] = SV_CreateworldSector(depth + 1, mins1, maxs1);

    return anode;
}

void SV_ClearWorld(void) {
    clipHandle_t h;
    vec3_t mins, maxs;

    Com_Memset(sv_worldSectors, 0, sizeof(sv_worldSectors));
    sv_numworldSectors = 0;

    h = CM_InlineModel(0);
    CM_ModelBounds(h, mins, maxs);
    SV_CreateworldSector(0, mins, maxs);
}

void SV_UnlinkEntity(sharedEntity_t* gEnt) {
    svEntity_t* ent;
    svEntity_t* scan;
    worldSector_t* ws;

    ent = SV_SvEntityForGentity(gEnt);

    gEnt->r.linked = qfalse;

    ws = ent->worldSector;
    if (!ws) {
        return;
    }
    ent->worldSector = NULL;

    if (ws->entities == ent) {
        ws->entities = ent->nextEntityInWorldSector;
        return;
    }

    for (scan = ws->entities; scan; scan = scan->nextEntityInWorldSector) {
        if (scan->nextEntityInWorldSector == ent) {
            scan->nextEntityInWorldSector = ent->nextEntityInWorldSector;
            return;
        }
    }

    Com_Printf("WARNING: SV_UnlinkEntity: not found in worldSector\n");
}

#define MAX_TOTAL_ENT_LEAFS 128
void SV_LinkEntity(sharedEntity_t* gEnt) {
    worldSector_t* node;
    int leafs[MAX_TOTAL_ENT_LEAFS];
    int cluster;
    int num_leafs;
    int i, j, k;
    int area;
    int lastLeaf;
    float *origin, *angles;
    svEntity_t* ent;

    ent = SV_SvEntityForGentity(gEnt);

    if (ent->worldSector) {
        SV_UnlinkEntity(gEnt);
    }

    if (gEnt->r.bmodel) {
        gEnt->s.solid = SOLID_BMODEL;
    } else if (gEnt->r.contents & (CONTENTS_SOLID | CONTENTS_BODY)) {
        i = gEnt->r.maxs[0];
        if (i < 1)
            i = 1;
        if (i > 255)
            i = 255;

        j = (-gEnt->r.mins[2]);
        if (j < 1)
            j = 1;
        if (j > 255)
            j = 255;

        k = (gEnt->r.maxs[2] + 32);
        if (k < 1)
            k = 1;
        if (k > 255)
            k = 255;

        gEnt->s.solid = (k << 16) | (j << 8) | i;
    } else {
        gEnt->s.solid = 0;
    }

    origin = gEnt->r.currentOrigin;
    angles = gEnt->r.currentAngles;

    if (gEnt->r.bmodel && (angles[0] || angles[1] || angles[2])) {
        float max;

        max = RadiusFromBounds(gEnt->r.mins, gEnt->r.maxs);
        for (i = 0; i < 3; i++) {
            gEnt->r.absmin[i] = origin[i] - max;
            gEnt->r.absmax[i] = origin[i] + max;
        }
    } else {
        VectorAdd(origin, gEnt->r.mins, gEnt->r.absmin);
        VectorAdd(origin, gEnt->r.maxs, gEnt->r.absmax);
    }

    gEnt->r.absmin[0] -= 1;
    gEnt->r.absmin[1] -= 1;
    gEnt->r.absmin[2] -= 1;
    gEnt->r.absmax[0] += 1;
    gEnt->r.absmax[1] += 1;
    gEnt->r.absmax[2] += 1;

    ent->numClusters = 0;
    ent->lastCluster = 0;
    ent->areanum = -1;
    ent->areanum2 = -1;

    num_leafs = CM_BoxLeafnums(gEnt->r.absmin, gEnt->r.absmax,
                               leafs, MAX_TOTAL_ENT_LEAFS, &lastLeaf);

    if (!num_leafs) {
        return;
    }

    for (i = 0; i < num_leafs; i++) {
        area = CM_LeafArea(leafs[i]);
        if (area != -1) {
            if (ent->areanum != -1 && ent->areanum != area) {
                if (ent->areanum2 != -1 && ent->areanum2 != area && sv.state == SS_LOADING) {
                    Com_DPrintf("Object %i touching 3 areas at %f %f %f\n",
                                gEnt->s.number,
                                gEnt->r.absmin[0], gEnt->r.absmin[1], gEnt->r.absmin[2]);
                }
                ent->areanum2 = area;
            } else {
                ent->areanum = area;
            }
        }
    }

    ent->numClusters = 0;
    for (i = 0; i < num_leafs; i++) {
        cluster = CM_LeafCluster(leafs[i]);
        if (cluster != -1) {
            ent->clusternums[ent->numClusters++] = cluster;
            if (ent->numClusters == MAX_ENT_CLUSTERS) {
                break;
            }
        }
    }

    if (i != num_leafs) {
        ent->lastCluster = CM_LeafCluster(lastLeaf);
    }

    gEnt->r.linkcount++;

    node = sv_worldSectors;
    while (1) {
        if (node->axis == -1)
            break;
        if (gEnt->r.absmin[node->axis] > node->dist)
            node = node->children[0];
        else if (gEnt->r.absmax[node->axis] < node->dist)
            node = node->children[1];
        else
            break;
    }

    ent->worldSector = node;
    ent->nextEntityInWorldSector = node->entities;
    node->entities = ent;

    gEnt->r.linked = qtrue;
}

typedef struct {
    const float* mins;
    const float* maxs;
    int* list;
    int count, maxcount;
} areaParms_t;

static void SV_AreaEntities_r(worldSector_t* node, areaParms_t* ap) {
    svEntity_t *check, *next;
    sharedEntity_t* gcheck;

    for (check = node->entities; check; check = next) {
        next = check->nextEntityInWorldSector;

        gcheck = SV_GEntityForSvEntity(check);

        if (gcheck->r.absmin[0] > ap->maxs[0] || gcheck->r.absmin[1] > ap->maxs[1] || gcheck->r.absmin[2] > ap->maxs[2] || gcheck->r.absmax[0] < ap->mins[0] || gcheck->r.absmax[1] < ap->mins[1] || gcheck->r.absmax[2] < ap->mins[2]) {
            continue;
        }

        if (ap->count == ap->maxcount) {
            Com_Printf("SV_AreaEntities: MAXCOUNT\n");
            return;
        }

        ap->list[ap->count] = check - sv.svEntities;
        ap->count++;
    }

    if (node->axis == -1) {
        return;
    }

    if (ap->maxs[node->axis] > node->dist) {
        SV_AreaEntities_r(node->children[0], ap);
    }
    if (ap->mins[node->axis] < node->dist) {
        SV_AreaEntities_r(node->children[1], ap);
    }
}

int SV_AreaEntities(const vec3_t mins, const vec3_t maxs, int* entityList, int maxcount) {
    areaParms_t ap;

    ap.mins = mins;
    ap.maxs = maxs;
    ap.list = entityList;
    ap.count = 0;
    ap.maxcount = maxcount;

    SV_AreaEntities_r(sv_worldSectors, &ap);

    return ap.count;
}

typedef struct {
    vec3_t boxmins, boxmaxs;
    const float* mins;
    const float* maxs;
    const float* start;
    vec3_t end;
    trace_t trace;
    int passEntityNum;
    int contentmask;
    int capsule;
} moveclip_t;

void SV_ClipToEntity(trace_t* trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask, int capsule) {
    sharedEntity_t* touch;
    clipHandle_t clipHandle;
    float *origin, *angles;

    touch = SV_GentityNum(entityNum);

    Com_Memset(trace, 0, sizeof(trace_t));

    if (!(contentmask & touch->r.contents)) {
        trace->fraction = 1.0;
        return;
    }

    clipHandle = SV_ClipHandleForEntity(touch);

    origin = touch->r.currentOrigin;
    angles = touch->r.currentAngles;

    if (!touch->r.bmodel) {
        angles = vec3_origin;
    }

    CM_TransformedBoxTrace(trace, (float*)start, (float*)end,
                           (float*)mins, (float*)maxs, clipHandle, contentmask,
                           origin, angles, capsule);

    if (trace->fraction < 1) {
        trace->entityNum = touch->s.number;
    }
}

static void SV_ClipMoveToEntities(moveclip_t* clip) {
    int i, num;
    int touchlist[MAX_GENTITIES];
    sharedEntity_t* touch;
    int passOwnerNum;
    trace_t trace;
    clipHandle_t clipHandle;
    float *origin, *angles;

    num = SV_AreaEntities(clip->boxmins, clip->boxmaxs, touchlist, MAX_GENTITIES);

    if (clip->passEntityNum != ENTITYNUM_NONE) {
        passOwnerNum = (SV_GentityNum(clip->passEntityNum))->r.ownerNum;
        if (passOwnerNum == ENTITYNUM_NONE) {
            passOwnerNum = -1;
        }
    } else {
        passOwnerNum = -1;
    }

    for (i = 0; i < num; i++) {
        if (clip->trace.allsolid) {
            return;
        }
        touch = SV_GentityNum(touchlist[i]);

        if (clip->passEntityNum != ENTITYNUM_NONE) {
            if (touchlist[i] == clip->passEntityNum) {
                continue;
            }
            if (touch->r.ownerNum == clip->passEntityNum) {
                continue;
            }
            if (touch->r.ownerNum == passOwnerNum) {
                continue;
            }
        }

        if (!(clip->contentmask & touch->r.contents)) {
            continue;
        }

        clipHandle = SV_ClipHandleForEntity(touch);

        origin = touch->r.currentOrigin;
        angles = touch->r.currentAngles;

        if (!touch->r.bmodel) {
            angles = vec3_origin;
        }

        CM_TransformedBoxTrace(&trace, (float*)clip->start, (float*)clip->end,
                               (float*)clip->mins, (float*)clip->maxs, clipHandle, clip->contentmask,
                               origin, angles, clip->capsule);

        if (trace.allsolid) {
            clip->trace.allsolid = qtrue;
            trace.entityNum = touch->s.number;
        } else if (trace.startsolid) {
            clip->trace.startsolid = qtrue;
            trace.entityNum = touch->s.number;
        }

        if (trace.fraction < clip->trace.fraction) {
            qboolean oldStart;

            oldStart = clip->trace.startsolid;

            trace.entityNum = touch->s.number;
            clip->trace = trace;
            clip->trace.startsolid |= oldStart;
        }
    }
}

void SV_Trace(trace_t* results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, int capsule) {
    moveclip_t clip;
    int i;

    if (!mins) {
        mins = vec3_origin;
    }
    if (!maxs) {
        maxs = vec3_origin;
    }

    Com_Memset(&clip, 0, sizeof(moveclip_t));

    CM_BoxTrace(&clip.trace, start, end, mins, maxs, 0, contentmask, capsule);
    clip.trace.entityNum = clip.trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
    if (clip.trace.fraction == 0) {
        *results = clip.trace;
        return;
    }

    clip.contentmask = contentmask;
    clip.start = start;
    VectorCopy(end, clip.end);
    clip.mins = mins;
    clip.maxs = maxs;
    clip.passEntityNum = passEntityNum;
    clip.capsule = capsule;

    for (i = 0; i < 3; i++) {
        if (end[i] > start[i]) {
            clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
            clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
        } else {
            clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
            clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
        }
    }

    SV_ClipMoveToEntities(&clip);

    *results = clip.trace;
}

int SV_PointContents(const vec3_t p, int passEntityNum) {
    int touch[MAX_GENTITIES];
    sharedEntity_t* hit;
    int i, num;
    int contents, c2;
    clipHandle_t clipHandle;
    float* angles;

    contents = CM_PointContents(p, 0);

    num = SV_AreaEntities(p, p, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {
        if (touch[i] == passEntityNum) {
            continue;
        }
        hit = SV_GentityNum(touch[i]);
        clipHandle = SV_ClipHandleForEntity(hit);
        angles = hit->r.currentAngles;
        if (!hit->r.bmodel) {
            angles = vec3_origin;
        }

        c2 = CM_TransformedPointContents(p, clipHandle, hit->r.currentOrigin, angles);

        contents |= c2;
    }

    return contents;
}
