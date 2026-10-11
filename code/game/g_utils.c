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
#include "g_local.h"

int G_FindConfigstringIndex(char* name, int start, int max, qboolean create) {
    int i;
    char s[MAX_STRING_CHARS];

    if (!name || !name[0]) {
        return 0;
    }

    for (i = 1; i < max; i++) {
        trap_GetConfigstring(start + i, s, sizeof(s));
        if (!s[0]) {
            break;
        }
        if (!strcmp(s, name)) {
            return i;
        }
    }

    if (!create) {
        return 0;
    }

    if (i == max) {
        G_Error("G_FindConfigstringIndex: overflow");
    }

    trap_SetConfigstring(start + i, name);

    return i;
}

int G_ModelIndex(char* name) {
    return G_FindConfigstringIndex(name, CS_MODELS, MAX_MODELS, qtrue);
}

int G_SoundIndex(char* name) {
    return G_FindConfigstringIndex(name, CS_SOUNDS, MAX_SOUNDS, qtrue);
}

void G_TeamCommand(team_t team, char* cmd) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            if (level.clients[i].sess.sessionTeam == team) {
                trap_SendServerCommand(i, va("%s", cmd));
            }
        }
    }
}

gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match) {
    char* s;

    if (!from)
        from = g_entities;
    else
        from++;

    for (; from < &g_entities[level.num_entities]; from++) {
        if (!from->inuse)
            continue;
        s = *(char**)((byte*)from + fieldofs);
        if (!s)
            continue;
        if (!Q_stricmp(s, match))
            return from;
    }

    return NULL;
}

#define MAXCHOICES 32

gentity_t* G_PickTarget(char* targetname) {
    gentity_t* ent = NULL;
    int num_choices = 0;
    gentity_t* choice[MAXCHOICES];

    if (!targetname) {
        G_Printf("G_PickTarget called with NULL targetname\n");
        return NULL;
    }

    while (1) {
        ent = G_Find(ent, FOFS(targetname), targetname);
        if (!ent)
            break;
        choice[num_choices++] = ent;
        if (num_choices == MAXCHOICES)
            break;
    }

    if (!num_choices) {
        G_Printf("G_PickTarget: target %s not found\n", targetname);
        return NULL;
    }

    return choice[rand() % num_choices];
}

void G_UseTargets(gentity_t* ent, gentity_t* activator) {
    gentity_t* t;

    if (!ent) {
        return;
    }

    if (!ent->target) {
        return;
    }

    t = NULL;
    while ((t = G_Find(t, FOFS(targetname), ent->target)) != NULL) {
        if (t == ent) {
            G_Printf("WARNING: Entity used itself.\n");
        } else {
            if (t->use) {
                t->use(t, ent, activator);
            }
        }
        if (!ent->inuse) {
            G_Printf("entity was removed while using targets\n");
            return;
        }
    }
}

float* tv(float x, float y, float z) {
    static int index;
    static vec3_t vecs[8];
    float* v;

    v = vecs[index];
    index = (index + 1) & 7;

    v[0] = x;
    v[1] = y;
    v[2] = z;

    return v;
}

char* vtos(const vec3_t v) {
    static int index;
    static char str[8][32];
    char* s;

    s = str[index];
    index = (index + 1) & 7;

    Com_sprintf(s, 32, "(%.2f %.2f %.2f)", v[0], v[1], v[2]);

    return s;
}

void G_SetMovedir(vec3_t angles, vec3_t movedir) {
    static vec3_t VEC_UP = {0, -1, 0};
    static vec3_t MOVEDIR_UP = {0, 0, 1};
    static vec3_t VEC_DOWN = {0, -2, 0};
    static vec3_t MOVEDIR_DOWN = {0, 0, -1};

    if (VectorCompare(angles, VEC_UP)) {
        VectorCopy(MOVEDIR_UP, movedir);
    } else if (VectorCompare(angles, VEC_DOWN)) {
        VectorCopy(MOVEDIR_DOWN, movedir);
    } else {
        AngleVectors(angles, movedir, NULL, NULL);
    }
    VectorClear(angles);
}

float vectoyaw(const vec3_t vec) {
    float yaw;

    if (vec[YAW] == 0 && vec[PITCH] == 0) {
        yaw = 0;
    } else {
        if (vec[PITCH]) {
            yaw = (atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
        } else if (vec[YAW] > 0) {
            yaw = 90;
        } else {
            yaw = 270;
        }
        if (yaw < 0) {
            yaw += 360;
        }
    }

    return yaw;
}

void G_InitGentity(gentity_t* e) {
    e->inuse = qtrue;
    e->classname = "noclass";
    e->s.number = e - g_entities;
    e->r.ownerNum = ENTITYNUM_NONE;
}

gentity_t* G_Spawn(void) {
    int i, force;
    gentity_t* e;

    e = NULL;
    for (force = 0; force < 2; force++) {
        e = &g_entities[MAX_CLIENTS];
        for (i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
            if (e->inuse) {
                continue;
            }

            if (!force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000) {
                continue;
            }

            G_InitGentity(e);
            return e;
        }
        if (level.num_entities < ENTITYNUM_MAX_NORMAL) {
            break;
        }
    }
    if (level.num_entities == ENTITYNUM_MAX_NORMAL) {
        for (i = 0; i < MAX_GENTITIES; i++) {
            G_Printf("%4i: %s\n", i, g_entities[i].classname);
        }
        G_Error("G_Spawn: no free entities");
    }

    level.num_entities++;

    trap_LocateGameData(level.gentities, level.num_entities, sizeof(gentity_t),
                        &level.clients[0].ps, sizeof(level.clients[0]));

    G_InitGentity(e);
    return e;
}

qboolean G_EntitiesFree(void) {
    int i;
    gentity_t* e;

    if (level.num_entities < ENTITYNUM_MAX_NORMAL) {
        return qtrue;
    }

    e = &g_entities[MAX_CLIENTS];
    for (i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
        if (e->inuse) {
            continue;
        }
        return qtrue;
    }
    return qfalse;
}

void G_FreeEntity(gentity_t* ed) {
    trap_UnlinkEntity(ed);

    if (ed->neverFree) {
        return;
    }

    memset(ed, 0, sizeof(*ed));
    ed->classname = "freed";
    ed->freetime = level.time;
    ed->inuse = qfalse;
}

gentity_t* G_TempEntity(vec3_t origin, int event) {
    gentity_t* e;
    vec3_t snapped;

    e = G_Spawn();
    e->s.eType = ET_EVENTS + event;

    e->classname = "tempEntity";
    e->eventTime = level.time;
    e->freeAfterEvent = qtrue;

    VectorCopy(origin, snapped);
    SnapVector(snapped);
    G_SetOrigin(e, snapped);

    trap_LinkEntity(e);

    return e;
}

void G_KillBox(gentity_t* ent) {
    int i, num;
    int touch[MAX_GENTITIES];
    gentity_t* hit;
    vec3_t mins, maxs;

    VectorAdd(ent->client->ps.origin, ent->r.mins, mins);
    VectorAdd(ent->client->ps.origin, ent->r.maxs, maxs);
    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (!hit->client) {
            continue;
        }

        G_Damage(hit, ent, ent, NULL, NULL,
                 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
    }
}

void G_AddPredictableEvent(gentity_t* ent, int event, int eventParm) {
    if (!ent->client) {
        return;
    }
    BG_AddPredictableEventToPlayerstate(event, eventParm, &ent->client->ps);
}

void G_AddEvent(gentity_t* ent, int event, int eventParm) {
    int bits;

    if (ent->client) {
        bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
        bits = (bits + EV_EVENT_BIT1) & EV_EVENT_BITS;
        ent->client->ps.externalEvent = event | bits;
        ent->client->ps.externalEventParm = eventParm;
    } else {
        bits = ent->s.event & EV_EVENT_BITS;
        bits = (bits + EV_EVENT_BIT1) & EV_EVENT_BITS;
        ent->s.event = event | bits;
        ent->s.eventParm = eventParm;
    }
    ent->eventTime = level.time;
}

void G_Sound(gentity_t* ent, int channel, int soundIndex) {
    gentity_t* te;

    te = G_TempEntity(ent->r.currentOrigin, EV_GENERAL_SOUND);
    te->s.eventParm = soundIndex;
}

void G_GlobalSound(int soundIndex) {
    gentity_t* te;

    te = G_TempEntity(vec3_origin, EV_GLOBAL_SOUND);
    te->s.eventParm = soundIndex;
}

void G_SetOrigin(gentity_t* ent, vec3_t origin) {
    VectorCopy(origin, ent->s.pos.trBase);
    ent->s.pos.trType = TR_STATIONARY;
    ent->s.pos.trTime = 0;
    ent->s.pos.trDuration = 0;
    VectorClear(ent->s.pos.trDelta);

    VectorCopy(origin, ent->r.currentOrigin);
}

int DebugLine(vec3_t start, vec3_t end, int color) {
    vec3_t points[4], dir, cross, up = {0, 0, 1};
    float dot;

    VectorCopy(start, points[0]);
    VectorCopy(start, points[1]);
    VectorCopy(end, points[2]);
    VectorCopy(end, points[3]);

    VectorSubtract(end, start, dir);
    VectorNormalize(dir);
    dot = DotProduct(dir, up);
    if (dot > 0.99 || dot < -0.99)
        VectorSet(cross, 1, 0, 0);
    else
        CrossProduct(dir, up, cross);

    VectorNormalize(cross);

    VectorMA(points[0], 2, cross, points[0]);
    VectorMA(points[1], -2, cross, points[1]);
    VectorMA(points[2], -2, cross, points[2]);
    VectorMA(points[3], 2, cross, points[3]);

    return trap_DebugPolygonCreate(color, 4, points);
}
