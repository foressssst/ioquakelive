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

extern qboolean itemRegistered[];

static vec3_t playerMins = {-15, -15, -24};
static vec3_t playerMaxs = {15, 15, 32};

void SP_info_player_deathmatch(gentity_t* ent) {
    int i;

    G_SpawnInt("nobots", "0", &i);
    if (i) {
        ent->flags |= FL_NO_BOTS;
    }
    G_SpawnInt("nohumans", "0", &i);
    if (i) {
        ent->flags |= FL_NO_HUMANS;
    }
}

void SP_info_player_start(gentity_t* ent) {
    ent->classname = "info_player_deathmatch";
    SP_info_player_deathmatch(ent);
}

void SP_info_player_intermission(gentity_t* ent) {
}

qboolean SpotWouldTelefrag(gentity_t* spot) {
    int i, num;
    int touch[MAX_GENTITIES];
    gentity_t* hit;
    vec3_t mins, maxs;

    VectorAdd(spot->s.origin, playerMins, mins);
    VectorAdd(spot->s.origin, playerMaxs, maxs);
    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (hit->client) {
            return qtrue;
        }
    }

    return qfalse;
}

#define MAX_SPAWN_POINTS 128
gentity_t* SelectNearestDeathmatchSpawnPoint(vec3_t from) {
    gentity_t* spot;
    vec3_t delta;
    float dist, nearestDist;
    gentity_t* nearestSpot;

    nearestDist = 999999;
    nearestSpot = NULL;
    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        VectorSubtract(spot->s.origin, from, delta);
        dist = VectorLength(delta);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestSpot = spot;
        }
    }

    return nearestSpot;
}

#define MAX_SPAWN_POINTS 128
gentity_t* SelectRandomDeathmatchSpawnPoint(qboolean isbot) {
    gentity_t* spot;
    int count;
    int selection;
    gentity_t* spots[MAX_SPAWN_POINTS];

    count = 0;
    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL && count < MAX_SPAWN_POINTS) {
        if (SpotWouldTelefrag(spot))
            continue;

        if (((spot->flags & FL_NO_BOTS) && isbot) ||
            ((spot->flags & FL_NO_HUMANS) && !isbot)) {
            continue;
        }

        spots[count] = spot;
        count++;
    }

    if (!count) {
        return G_Find(NULL, FOFS(classname), "info_player_deathmatch");
    }

    selection = rand() % count;
    return spots[selection];
}

gentity_t* SelectRandomFurthestSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot) {
    gentity_t* spot;
    vec3_t delta;
    float dist;
    float list_dist[MAX_SPAWN_POINTS];
    gentity_t* list_spot[MAX_SPAWN_POINTS];
    int numSpots, rnd, i, j;

    numSpots = 0;
    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        if (SpotWouldTelefrag(spot))
            continue;

        if (((spot->flags & FL_NO_BOTS) && isbot) ||
            ((spot->flags & FL_NO_HUMANS) && !isbot)) {
            continue;
        }

        VectorSubtract(spot->s.origin, avoidPoint, delta);
        dist = VectorLength(delta);

        for (i = 0; i < numSpots; i++) {
            if (dist > list_dist[i]) {
                if (numSpots >= MAX_SPAWN_POINTS)
                    numSpots = MAX_SPAWN_POINTS - 1;

                for (j = numSpots; j > i; j--) {
                    list_dist[j] = list_dist[j - 1];
                    list_spot[j] = list_spot[j - 1];
                }

                list_dist[i] = dist;
                list_spot[i] = spot;

                numSpots++;
                break;
            }
        }

        if (i >= numSpots && numSpots < MAX_SPAWN_POINTS) {
            list_dist[numSpots] = dist;
            list_spot[numSpots] = spot;
            numSpots++;
        }
    }

    if (!numSpots) {
        spot = G_Find(NULL, FOFS(classname), "info_player_deathmatch");

        if (!spot)
            G_Error("Couldn't find a spawn point");

        VectorCopy(spot->s.origin, origin);
        origin[2] += 9;
        VectorCopy(spot->s.angles, angles);
        return spot;
    }

    rnd = random() * (numSpots / 2);

    VectorCopy(list_spot[rnd]->s.origin, origin);
    origin[2] += 9;
    VectorCopy(list_spot[rnd]->s.angles, angles);

    return list_spot[rnd];
}

gentity_t* SelectSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot) {
    return SelectRandomFurthestSpawnPoint(avoidPoint, origin, angles, isbot);

}

#define MAX_SPAWN_CANDIDATES 26

typedef struct {
    gentity_t* spot;
    float dist;
} spawnCandidate_t;

static gentity_t* SelectSpawnSpot(qboolean isBot, int otherTeam, vec3_t origin, vec3_t angles,
                                  qboolean initial, int team, int mode) {
    spawnCandidate_t list[MAX_SPAWN_CANDIDATES];
    char serverinfo[MAX_INFO_STRING];
    const char* classname;
    gentity_t* spot;
    gclient_t* cl;
    float ratio, weight, dist, nearest, threshold;
    int numSpots, numCandidates, count, i, k, pick;

    for (;;) {
        ratio = g_spawnRandomRatio.value;
        if (ratio < 0.1) {
            ratio = 0.1f;
        } else if (ratio > 1.0f) {
            ratio = 1.0f;
        }

        if (team == TEAM_RED) {
            classname = initial ? "team_CTF_redplayer" : "team_CTF_redspawn";
        } else if (team == TEAM_BLUE) {
            classname = initial ? "team_CTF_blueplayer" : "team_CTF_bluespawn";
        } else {
            classname = "info_player_deathmatch";
        }

        numSpots = 0;
        numCandidates = 3;
        count = 0;
        for (spot = G_Find(NULL, FOFS(classname), classname); spot && count < MAX_SPAWN_CANDIDATES;
             spot = G_Find(spot, FOFS(classname), classname)) {
            if (level.mapIsTrainingMap) {
                if (level.warmupTime && initial) {
                    if (spot->spawnflags & (isBot ? 2 : 1)) {
                        VectorCopy(spot->s.origin, origin);
                        origin[2] += 9;
                        VectorCopy(spot->s.angles, angles);
                        return spot;
                    }
                    continue;
                }
                if (spot->spawnflags & 3) {
                    continue;
                }
            }
            if ((spot->spawnflags & 2) || (!team && initial && !(spot->spawnflags & 1))) {
                continue;
            }

            numSpots++;
            numCandidates = (int)ceil(numSpots * ratio);
            if (numCandidates > MAX_SPAWN_CANDIDATES) {
                numCandidates = MAX_SPAWN_CANDIDATES;
            } else if (numCandidates < 3) {
                numCandidates = 3;
            }
            threshold = (count >= numCandidates && count >= 5) ? list[count - 1].dist : 0.0f;

            nearest = -1.0f;
            for (i = 0; i < level.maxclients; i++) {
                cl = g_entities[i].client;
                if (!cl || cl->pers.connected == CON_DISCONNECTED ||
                    cl->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cl->ps.stats[STAT_HEALTH] <= 0) {
                    continue;
                }
                weight = 1.0f;
                if (otherTeam && cl->ps.persistant[PERS_TEAM] == otherTeam) {
                    if (g_enemyTeamRespawnRatio.value == 0) {
                        continue;
                    }
                    weight = g_enemyTeamRespawnRatio.value;
                }
                dist = fabs(spot->s.origin[0] - cl->ps.origin[0]);
                if (fabs(spot->s.origin[1] - cl->ps.origin[1]) > dist) {
                    dist = fabs(spot->s.origin[1] - cl->ps.origin[1]);
                }
                if (fabs(spot->s.origin[2] - cl->ps.origin[2]) > dist) {
                    dist = fabs(spot->s.origin[2] - cl->ps.origin[2]);
                }
                dist *= weight;
                if (nearest == -1.0f || dist < nearest) {
                    nearest = dist;
                    if (threshold != 0 && dist <= threshold) {
                        break;
                    }
                }
            }
            if (level.mapIsTrainingMap && nearest == -1.0f) {
                nearest = 1.0f;
            }

            if (nearest > threshold) {
                for (k = count; k > 0 && list[k - 1].dist < nearest; k--) {
                    list[k] = list[k - 1];
                }
                list[k].spot = spot;
                list[k].dist = nearest;
                count++;
            }
        }

        if (numSpots) {
            break;
        }
        if (!initial) {
            if (mode != 2) {
                trap_GetServerinfo(serverinfo, sizeof(serverinfo));
                G_Error("No spawns for this gametype: classname %s, mapname %s, gametype %d\n", classname,
                        Info_ValueForKey(serverinfo, "mapname"), g_gametype.integer);
            }
            return NULL;
        }
        initial = qfalse;
    }

    if (!count) {
        if (level.mapIsTrainingMap && !trap_Cvar_VariableIntegerValue("sv_serverType")) {
            Com_Error(ERR_DROP, "training level: bad spawn");
        }
        pick = (int)((rand() & 0x7fff) / 32767.0 * numSpots);
        i = 0;
        for (spot = G_Find(NULL, FOFS(classname), classname); spot; spot = G_Find(spot, FOFS(classname), classname)) {
            if (!team && initial && !(spot->spawnflags & 1)) {
                continue;
            }
            if (i == pick) {
                VectorCopy(spot->s.origin, origin);
                origin[2] += 9;
                VectorCopy(spot->s.angles, angles);
                return spot;
            }
            i++;
        }
    }

    if (numCandidates > count) {
        numCandidates = count;
    }
    if (mode) {
        while (numCandidates > 0 && list[numCandidates - 1].dist < g_spawnMinDistance.integer) {
            numCandidates--;
        }
        if (!numCandidates) {
            if (mode == 2) {
                return NULL;
            }
            numCandidates = 1;
        }
    }
    if (!count) {
        return NULL;
    }
    pick = (int)((rand() & 0x7fff) / 32767.0 * numCandidates);
    if (pick >= count) {
        pick = count - 1;
    }

    spot = list[pick].spot;
    VectorCopy(spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy(spot->s.angles, angles);
    return spot;
}

static gentity_t* SelectDominationSpawnPoint(gentity_t* ent, vec3_t origin, vec3_t angles) {
    gentity_t* linked[25];
    gentity_t* kept[25];
    gentity_t* point;
    gentity_t* spot;
    gclient_t* cl;
    vec3_t delta;
    float nearest, dist;
    int numLinked, numKept, i, j, pick;

    numLinked = 0;
    for (point = G_Find(NULL, FOFS(classname), "team_dom_point"); point;
         point = G_Find(point, FOFS(classname), "team_dom_point")) {
        if (point->s.modelindex2 != ent->client->sess.sessionTeam || !point->target) {
            continue;
        }
        for (spot = G_Find(NULL, FOFS(classname), "info_player_deathmatch"); spot;
             spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) {
            if (spot->targetname && !Q_stricmp(point->target, spot->targetname) && numLinked < 25) {
                linked[numLinked++] = spot;
            }
        }
    }

    numKept = 0;
    for (i = 0; i < numLinked; i++) {
        nearest = 1e8f;
        for (j = 0; j < level.maxclients; j++) {
            cl = g_entities[j].client;
            if (!cl || cl->pers.connected == CON_DISCONNECTED ||
                cl->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cl->ps.stats[STAT_HEALTH] <= 0) {
                continue;
            }
            VectorSubtract(linked[i]->r.currentOrigin, g_entities[j].r.currentOrigin, delta);
            dist = DotProduct(delta, delta);
            if (dist < nearest) {
                nearest = dist;
            }
        }
        if ((float)(g_spawnMinDistance.integer * g_spawnMinDistance.integer) < nearest) {
            kept[numKept++] = linked[i];
        }
    }

    if (numKept > 1) {
        pick = (int)((rand() & 0x7fff) / 32767.0 * numKept);
        if (pick >= 0 && pick < numKept) {
            VectorCopy(kept[pick]->s.origin, origin);
            origin[2] += 9;
            VectorCopy(kept[pick]->s.angles, angles);
            return linked[pick];
        }
    }
    return NULL;
}

static gentity_t* G_TrainingRoomSpot(qboolean isBot, vec3_t origin, vec3_t angles) {
    char buf[MAX_CVAR_VALUE_STRING];
    char* p = buf;
    float v[4] = {0, 0, 0, 0};
    gentity_t* spot = NULL;
    gentity_t* nearest = NULL;
    float d, best = 0;
    int i;

    Q_strncpyz(buf, isBot ? g_trainingTarget.string : g_trainingSpawn.string, sizeof(buf));
    for (i = 0; i < 4; i++) {
        const char* token = COM_Parse(&p);

        if (!token[0]) {
            break;
        }
        v[i] = atof(token);
    }
    if (i < 3) {
        return NULL;
    }
    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        d = DistanceSquared(spot->s.origin, v);
        if (!nearest || d < best) {
            nearest = spot;
            best = d;
        }
    }
    if (nearest) {
        VectorCopy(v, origin);
        origin[2] += 9;
        VectorSet(angles, 0, v[3], 0);
    }
    return nearest;
}

static gentity_t* SelectClientSpawnPoint(gentity_t* ent, vec3_t origin, vec3_t angles) {
    gclient_t* client = ent->client;
    qboolean isBot = (ent->r.svFlags & SVF_BOT) != 0;
    int gt = g_gametype.integer;
    qboolean teamSpots = gt >= GT_CA && gt <= GT_AD;
    int randomTeam = rand() % 2 + 1;
    qboolean initial = qfalse;
    int team = 0;
    int otherTeam = 0;
    gentity_t* spot;

    if (g_advancedTraining.integer) {
        spot = G_TrainingRoomSpot(isBot, origin, angles);
        if (spot) {
            return spot;
        }
    }

    if (gt == GT_DUEL && !client->pers.initialSpawn) {
        client->pers.initialSpawn = qtrue;
        initial = qtrue;
    }
    if (level.mapIsTrainingMap) {
        if (level.warmupTime && !client->pers.initialSpawn) {
            client->pers.initialSpawn = qtrue;
            initial = qtrue;
        } else if (!level.warmupTime) {
            initial = qfalse;
        }
    }

    if (teamSpots) {
        if (client->pers.teamState.state == TEAM_BEGIN || (gt == GT_AD && !level.warmupTime)) {
            initial = qtrue;
        }
        team = client->sess.sessionTeam == TEAM_RED ? TEAM_RED : TEAM_BLUE;
        if (gt == GT_FREEZE || gt == GT_DOMINATION) {
            otherTeam = team;
        } else {
            otherTeam = team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
        }
    }

    if (gt == GT_DOMINATION && !initial) {
        spot = SelectDominationSpawnPoint(ent, origin, angles);
    } else {
        spot = SelectSpawnSpot(isBot, otherTeam, origin, angles, initial, team, level.mapIsTrainingMap ? 1 : 2);
    }

    if (!spot && gt != GT_CTF && gt != GT_1FCTF && gt != GT_OBELISK && gt != GT_HARVESTER && gt != GT_AD) {
        if (initial) {
            spot = SelectSpawnSpot(isBot, 0, origin, angles, qfalse, team, 2);
        }
        if (!spot) {
            spot = SelectSpawnSpot(isBot, 0, origin, angles, qtrue, teamSpots ? 0 : randomTeam, 2);
        }
        if (!spot) {
            spot = SelectSpawnSpot(isBot, 0, origin, angles, qfalse, teamSpots ? 0 : randomTeam, 2);
        }
    }

    if (!spot && client->failedSpawns >= g_maxDeferredSpawns.integer) {
        spot = SelectSpawnSpot(isBot, otherTeam, origin, angles, qfalse, 0, 1);
    }
    return spot;
}

gentity_t* SelectSpectatorSpawnPoint(vec3_t origin, vec3_t angles) {
    FindIntermissionPoint();

    VectorCopy(level.intermission_origin, origin);
    VectorCopy(level.intermission_angle, angles);

    return NULL;
}

void InitBodyQue(void) {
    int i;
    gentity_t* ent;

    level.bodyQueIndex = 0;
    for (i = 0; i < BODY_QUEUE_SIZE; i++) {
        ent = G_Spawn();
        ent->classname = "bodyque";
        ent->neverFree = qtrue;
        level.bodyQue[i] = ent;
    }
}

void BodySink(gentity_t* ent) {
    if (level.time - ent->timestamp > 6500) {
        trap_UnlinkEntity(ent);
        ent->physicsObject = qfalse;
        return;
    }
    ent->nextthink = level.time + 100;
    ent->s.pos.trBase[2] -= 1;
}

void CopyToBodyQue(gentity_t* ent) {
    gentity_t* e;
    int i;
    gentity_t* body;
    int contents;

    trap_UnlinkEntity(ent);

    contents = trap_PointContents(ent->s.origin, -1);
    if (contents & CONTENTS_NODROP) {
        return;
    }

    body = level.bodyQue[level.bodyQueIndex];
    level.bodyQueIndex = (level.bodyQueIndex + 1) % BODY_QUEUE_SIZE;

    body->s = ent->s;
    body->s.eFlags = EF_DEAD;

    if (ent->s.eFlags & EF_KAMIKAZE) {
        body->s.eFlags |= EF_KAMIKAZE;

        for (i = 0; i < level.num_entities; i++) {
            e = &g_entities[i];
            if (!e->inuse)
                continue;
            if (e->activator != ent)
                continue;
            if (strcmp(e->classname, "kamikaze timer"))
                continue;
            e->activator = body;
            break;
        }
    }

    body->s.powerups = 0;
    body->s.loopSound = 0;
    body->s.number = body - g_entities;
    body->timestamp = level.time;
    body->physicsObject = qtrue;
    body->physicsBounce = 0;
    if (body->s.groundEntityNum == ENTITYNUM_NONE) {
        body->s.pos.trType = TR_GRAVITY;
        body->s.pos.trTime = level.time;
        VectorCopy(ent->client->ps.velocity, body->s.pos.trDelta);
    } else {
        body->s.pos.trType = TR_STATIONARY;
    }
    body->s.event = 0;

    switch (body->s.legsAnim & ~ANIM_TOGGLEBIT) {
        case BOTH_DEATH1:
        case BOTH_DEAD1:
            body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD1;
            break;
        case BOTH_DEATH2:
        case BOTH_DEAD2:
            body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD2;
            break;
        case BOTH_DEATH3:
        case BOTH_DEAD3:
        default:
            body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD3;
            break;
    }

    body->r.svFlags = ent->r.svFlags;
    VectorCopy(ent->r.mins, body->r.mins);
    VectorCopy(ent->r.maxs, body->r.maxs);
    VectorCopy(ent->r.absmin, body->r.absmin);
    VectorCopy(ent->r.absmax, body->r.absmax);

    body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
    body->r.contents = CONTENTS_CORPSE;
    body->r.ownerNum = ent->s.number;

    body->nextthink = level.time + 5000;
    body->think = BodySink;

    body->die = body_die;

    if (ent->health <= GIB_HEALTH) {
        body->takedamage = qfalse;
    } else {
        body->takedamage = qtrue;
    }

    VectorCopy(body->s.pos.trBase, body->r.currentOrigin);
    trap_LinkEntity(body);
}

void SetClientViewAngle(gentity_t* ent, vec3_t angle) {
    int i;

    for (i = 0; i < 3; i++) {
        int cmdAngle;

        cmdAngle = ANGLE2SHORT(angle[i]);
        ent->client->ps.delta_angles[i] = cmdAngle - ent->client->pers.cmd.angles[i];
    }
    VectorCopy(angle, ent->s.angles);
    VectorCopy(ent->s.angles, ent->client->ps.viewangles);
}

void ClientRespawn(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (!level.intermissionTime && !level.intermissionQueued && g_gametype.integer == GT_FREEZE &&
        client->ps.pm_type == PM_FREEZE) {
        return;
    }
    if (client->respawnTime > level.time) {
        return;
    }

    if (!level.warmupTime && level.roundState.eCurrent != RS_COUNTDOWN &&
        (g_gametype.integer == GT_CA || g_gametype.integer == GT_AD)) {
        ClientBegin_RoundBased_impl(ent);
        return;
    }

    if (!(client->buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE)) &&
        level.time - client->respawnTime <= g_respawn_delay_max.integer) {
        switch (g_gametype.integer) {
            case GT_CA:
            case GT_FREEZE:
            case GT_AD:
            case GT_RR:
                if (level.warmupTime < 1 && level.roundState.eCurrent != RS_COUNTDOWN) {
                    return;
                }
                break;
            default:
                return;
        }
    }

    if (g_gametype.integer != GT_FREEZE || level.intermissionTime || level.intermissionQueued) {
        CopyToBodyQue(ent);
    }
    client->ps.pm_type = PM_NORMAL;
    ClientSpawn(ent);
}

int TeamCount(int ignoreClientNum, team_t team) {
    int i;
    int count = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (i == ignoreClientNum) {
            continue;
        }
        if (level.clients[i].pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (level.clients[i].sess.sessionTeam == team) {
            count++;
        }
    }

    return count;
}

int TeamLeader(int team) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (level.clients[i].sess.sessionTeam == team) {
            if (level.clients[i].sess.teamLeader)
                return i;
        }
    }

    return -1;
}

team_t PickTeam(int ignoreClientNum) {
    int counts[TEAM_NUM_TEAMS];

    counts[TEAM_BLUE] = TeamCount(ignoreClientNum, TEAM_BLUE);
    counts[TEAM_RED] = TeamCount(ignoreClientNum, TEAM_RED);

    if (counts[TEAM_BLUE] > counts[TEAM_RED]) {
        return TEAM_RED;
    }
    if (counts[TEAM_RED] > counts[TEAM_BLUE]) {
        return TEAM_BLUE;
    }
    if (level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED]) {
        return TEAM_RED;
    }
    return TEAM_BLUE;
}

static void ClientCleanName(const char* in, char* out, int outSize) {
    int outpos = 0, colorlessLen = 0, spaces = 0;

    for (; *in == ' '; in++)
        ;

    for (; *in && outpos < outSize - 1; in++) {
        out[outpos] = *in;

        if (*in == ' ') {
            if (spaces > 2)
                continue;

            spaces++;
        } else if (outpos > 0 && out[outpos - 1] == Q_COLOR_ESCAPE) {
            if (Q_IsColorString(&out[outpos - 1])) {
                colorlessLen--;

                if (Q_ColorCodeIndex(*in, qtrue) == 0) {
                    outpos--;
                    continue;
                }
            } else {
                spaces = 0;
                colorlessLen++;
            }
        } else {
            spaces = 0;
            colorlessLen++;
        }

        outpos++;
    }

    out[outpos] = '\0';

    if (*out == '\0' || colorlessLen == 0)
        Q_strncpyz(out, "UnnamedPlayer", outSize);
}

static int QDECL G_CompareQueueTime(const void* a, const void* b) {
    gclient_t* ca = level.clients + *(const int*)a;
    gclient_t* cb = level.clients + *(const int*)b;

    if (ca->sess.spectatorTime > cb->sess.spectatorTime) {
        return 1;
    }
    return (ca->sess.spectatorTime == cb->sess.spectatorTime) ? 0 : -1;
}

void G_UpdatePlayQueue(void) {
    int list[MAX_CLIENTS];
    int count = 0;
    gclient_t* cl;
    int i;

    if (g_gametype.integer != GT_DUEL) {
        return;
    }

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = level.clients + level.sortedClients[i];
        if (cl->sess.sessionTeam == TEAM_SPECTATOR && !cl->sess.specOnly) {
            list[count++] = level.sortedClients[i];
        }
    }
    qsort(list, count, sizeof(list[0]), G_CompareQueueTime);

    for (i = 0; i < count; i++) {
        cl = level.clients + list[i];
        if (cl->sess.playQueue != i + 1) {
            cl->sess.playQueue = i + 1;
            cl->sess.updatePlayQueue = qtrue;
        }
    }
}

void ClientUserinfoChanged(int clientNum) {
    gentity_t* ent;
    int teamTask, teamLeader, health;
    char* s;
    char model[MAX_QPATH];
    char headModel[MAX_QPATH];
    char oldname[MAX_STRING_CHARS];
    gclient_t* client;
    char c1[MAX_INFO_STRING];
    char c2[MAX_INFO_STRING];
    char userinfo[MAX_INFO_STRING];

    ent = g_entities + clientNum;
    client = ent->client;

    if (client) {
        client->pers.userInfoFloodInfractions++;
    }

    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

    if (!Info_Validate(userinfo)) {
        strcpy(userinfo, "\\name\\badinfo");
        trap_DropClient(clientNum, "Invalid userinfo");
    }

    client->pers.ioqlClient = (qboolean)(Info_ValueForKey(userinfo, "ioql")[0] != '\0');

    client->pers.autoaction = atoi(Info_ValueForKey(userinfo, "cg_autoAction")) | g_autoAction.integer;

    s = Info_ValueForKey(userinfo, "cg_predictItems");
    if (!atoi(s)) {
        client->pers.predictItemPickup = qfalse;
    } else {
        client->pers.predictItemPickup = qtrue;
    }

    Q_strncpyz(oldname, client->pers.netname, sizeof(oldname));
    s = Info_ValueForKey(userinfo, "name");
    ClientCleanName(s, client->pers.netname, sizeof(client->pers.netname));

    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        if (client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
            Q_strncpyz(client->pers.netname, "scoreboard", sizeof(client->pers.netname));
        }
    }

    if (client->pers.connected == CON_CONNECTED) {
        if (strcmp(oldname, client->pers.netname)) {
            trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " renamed to %s\n\"", oldname,
                                          client->pers.netname));
        }
    }

    health = atoi(Info_ValueForKey(userinfo, "handicap"));
    if (health < 1 || health > 100) {
        health = 100;
    }
    client->pers.maxHealth = (int)(health / 100.0 * g_startingHealth.integer);
    if (client->pers.maxHealth < 1 || client->pers.maxHealth > g_startingHealth.integer) {
        client->pers.maxHealth = g_startingHealth.integer;
    }

    client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

    if (g_playermodelOverride.string[0] != '\0') {
        Q_strncpyz(model, g_playermodelOverride.string, sizeof(model));
    } else {
        Q_strncpyz(model, Info_ValueForKey(userinfo, "model"), sizeof(model));
    }
    if (g_playerheadmodelOverride.string[0] != '\0') {
        Q_strncpyz(headModel, g_playerheadmodelOverride.string, sizeof(headModel));
    } else {
        Q_strncpyz(headModel, Info_ValueForKey(userinfo, "headmodel"), sizeof(headModel));
    }

    if (g_gametype.integer >= GT_TEAM && !(ent->r.svFlags & SVF_BOT)) {
        client->pers.teamInfo = qtrue;
    } else {
        s = Info_ValueForKey(userinfo, "teamoverlay");
        if (!*s || atoi(s) != 0) {
            client->pers.teamInfo = qtrue;
        } else {
            client->pers.teamInfo = qfalse;
        }
    }

    teamTask = atoi(Info_ValueForKey(userinfo, "teamtask"));
    teamLeader = client->sess.teamLeader;

    Q_strncpyz(c1, Info_ValueForKey(userinfo, "color1"), sizeof(c1));
    Q_strncpyz(c2, Info_ValueForKey(userinfo, "color2"), sizeof(c2));

    Q_strncpyz(client->pers.country, Info_ValueForKey(userinfo, "country"), sizeof(client->pers.country));

    G_UpdatePlayQueue();

    if (ent->r.svFlags & SVF_BOT) {
        s = va("n\\%s\\t\\%i\\model\\%s\\hmodel\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\skill\\%s\\tt\\%d\\tl\\%d\\rp\\%d\\p\\%d\\so\\%i\\pq\\%i",
               client->pers.netname, client->sess.sessionTeam, model, headModel, c1, c2,
               client->pers.maxHealth, client->sess.wins, client->sess.losses,
               Info_ValueForKey(userinfo, "skill"), teamTask, teamLeader, client->pers.ready ? 1 : 0,
               client->sess.privileges, client->sess.specOnly, client->sess.playQueue);
    } else {
        s = va("n\\%s\\t\\%i\\model\\%s\\hmodel\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d\\rp\\%d\\p\\%d\\so\\%i\\pq\\%i\\st\\%llu\\c\\%s",
               client->pers.netname, client->sess.sessionTeam, model, headModel, c1, c2,
               client->pers.maxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader,
               client->pers.ready ? 1 : 0, client->sess.privileges, client->sess.specOnly,
               client->sess.playQueue, (unsigned long long)client->pers.steamId, client->pers.country);
    }

    trap_SetConfigstring(CS_PLAYERS + clientNum, s);

    G_LogPrintf("ClientUserinfoChanged: %i %s\n", clientNum, s);
}

char* ClientConnect(int clientNum, qboolean firstTime, qboolean isBot) {
    char* value;
    gclient_t* client;
    char userinfo[MAX_INFO_STRING];
    gentity_t* ent;

    ent = &g_entities[clientNum];

    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

    value = Info_ValueForKey(userinfo, "ip");
    if (!isBot && G_GetAccess(clientNum) == -1) {
        return "You are banned from this server.";
    }

    if (ent->inuse) {
        G_LogPrintf("Forcing disconnect on active client: %i\n", clientNum);
        ClientDisconnect(clientNum);
    }

    ent->client = level.clients + clientNum;
    client = ent->client;

    memset(client, 0, sizeof(*client));

    client->pers.connected = CON_CONNECTING;
    client->pers.steamId = trap_GetSteamID(clientNum);

    value = Info_ValueForKey(userinfo, "ip");
    if (!strcmp(value, "localhost")) {
        client->pers.localClient = qtrue;
    }

    if (firstTime || level.newSession) {
        G_InitSessionData(client, userinfo);
        {
            int r;
            memset(&client->expandedStats, 0, sizeof(client->expandedStats));
            client->expandedStats.lastThinkTime = level.time;
            client->expandedStats.teamJoinTime = level.time - level.startTime;
            r = rand();
            client->expandedStats.statId = level.time + r + 636 + (int)(intptr_t)client;
        }
    }
    G_ReadSessionData(client);

    if (isBot) {
        ent->r.svFlags |= SVF_BOT;
        ent->inuse = qtrue;
        if (!G_BotConnect(clientNum, !firstTime)) {
            return "BotConnectfailed";
        }
    }

    if (firstTime && !isBot && !trap_VerifySteamAuth(clientNum)) {
        return "Failed to verify Steam auth token";
    }

    G_LogPrintf("ClientConnect: %i\n", clientNum);
    ClientUserinfoChanged(clientNum);

    if (firstTime && !g_training.integer) {
        trap_SendServerCommand(clientNum, va("priv %i", client->sess.privileges));

        if (level.time > 5000) {
            uint64_t steamId = trap_GetSteamID(clientNum);

            trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " connected\n\"", client->pers.netname));
            if (steamId) {
                G_Printf("%s connected with Steam ID %llu\n", client->pers.netname, (unsigned long long)steamId);
            }
        }

        STAT_SubscribeClient(ent);

        if (g_gametype.integer > GT_RACE &&
            client->sess.sessionTeam != TEAM_SPECTATOR) {
            BroadcastTeamChange(client, -1);
        }

        if (!isBot) {
            G_AutoActionClientConnect(ent);
        }
    }

    CalculateRanks();

    if (!firstTime && client->sess.sessionTeam == TEAM_SPECTATOR) {
        ent->client->ps.eFlags |= EF_SPECTATOR;
    }

    return NULL;
}

void ClientBegin(int clientNum) {
    gentity_t* ent;
    gclient_t* client;
    int flags;

    ent = g_entities + clientNum;

    client = level.clients + clientNum;

    if (ent->r.linked) {
        trap_UnlinkEntity(ent);
    }
    G_InitGentity(ent);
    ent->touch = 0;
    ent->pain = 0;
    ent->client = client;

    client->pers.connected = CON_CONNECTED;
    client->pers.enterTime = level.time;
    client->pers.teamState.state = TEAM_BEGIN;

    flags = client->ps.eFlags;
    memset(&client->ps, 0, sizeof(client->ps));
    client->ps.eFlags = flags;

    client->lastkilled_client = -1;
    client->lastClientKilled = -1;
    client->lasthurt_client[0] = -1;
    client->lasthurt_client[1] = -1;

    if (client->sess.sessionTeam != TEAM_SPECTATOR) {
        client->ps.pm_type = PM_NORMAL;
    } else {
        client->ps.pm_type = PM_SPECTATOR;
    }

    switch (g_gametype.integer) {
    case GT_RACE:
        RACE_ClientBegin(ent);
        break;
    case GT_CA:
    case GT_AD:
        ClientBegin_RoundBased(ent);
        break;
    case GT_FREEZE:
        Freeze_ClientBegin(ent);
        break;
    case GT_RR:
        RR_ClientBegin(ent);
        break;
    default:
        ClientSpawn(ent);
        break;
    }

    if (g_gametype.integer == GT_DUEL && (ent->r.svFlags & SVF_BOT)) {
        client->sess.specOnly = 0;
        if (client->sess.spectatorTime == 0) {
            client->sess.spectatorTime = (int)time(NULL);
        }
        ClientUserinfoChanged(clientNum);
    }

    level.nextTeamInfoTime = level.time + 1000;

    G_LogPrintf("ClientBegin: %i\n", clientNum);

    CalculateRanks();
}

static int G_StartingAmmo(int weapon) {
    return trap_Cvar_VariableIntegerValue(va("g_startingAmmo_%s", weaponShortName[weapon]));
}

void G_GiveStartingWeapons(gentity_t* ent) {
    gclient_t* client = ent->client;
    gitem_t* item;
    int w;

    if (client->ps.pm_flags & PMF_LOADOUT_FORCED) {
        return;
    }

    for (w = WP_GAUNTLET; w < WP_NUM_WEAPONS; w++) {
        int ammo = G_StartingAmmo(w);

        if (g_loadout.integer && BG_WeaponInLoadout(w) && client->sess.weaponPrimary == w &&
            !(g_disableLoadout.integer & (1 << w))) {
            client->ps.stats[STAT_WEAPONS] |= 1 << w;
            client->ps.ammo[w] = ammo;
            RegisterItem(BG_FindItemForWeapon((weapon_t)w));
        }
    }

    if (g_startingWeapons.integer & 0x4000) {
        for (w = WP_HMG; w >= WP_GAUNTLET; w--) {
            int ammo;

            item = BG_FindItemForWeapon((weapon_t)w);
            ammo = G_StartingAmmo(w);
            if ((w == WP_GAUNTLET && !(g_startingWeapons.integer & 1)) ||
                (w == WP_MACHINEGUN && !(g_startingWeapons.integer & 2)) ||
                (w == WP_GRAPPLING_HOOK && !(g_startingWeapons.integer & 0x200))) {
                continue;
            }
            if (item && itemRegistered[item - bg_itemlist]) {
                client->ps.stats[STAT_WEAPONS] |= 1 << w;
                client->ps.ammo[w] = ammo;
            }
        }
        return;
    }

    for (w = WP_GAUNTLET; w < WP_NUM_WEAPONS; w++) {
        if (g_startingWeapons.integer & (1 << (w - 1))) {
            client->ps.stats[STAT_WEAPONS] |= 1 << w;
            client->ps.ammo[w] = G_StartingAmmo(w);
            client->ps.weapon = w;
            RegisterItem(BG_FindItemForWeapon((weapon_t)w));
        }
    }
}

void G_SetInfiniteAmmo(gentity_t* ent) {
    int w;

    if (g_infiniteAmmo.integer || g_instaGib.integer) {
        for (w = WP_GAUNTLET; w < WP_NUM_WEAPONS; w++) {
            ent->client->ps.ammo[w] = -1;
        }
    }
}

void G_SetStartingHealth(gentity_t* ent) {
    int health = g_startingHealth.integer + g_startingHealthBonus.integer;

    if (health < 1) {
        health = ent->client->pers.maxHealth;
    } else if (health > 999) {
        health = 999;
    }
    ent->client->ps.stats[STAT_HEALTH] = health;
    ent->s.health = health;
    ent->health = health;
}

static void G_GrantItem(gentity_t* ent, const char* name) {
    gentity_t* it;
    gitem_t* item;

    for (item = bg_itemlist + 1; item->classname; item++) {
        if (item->pickup_name && name && !Q_stricmp(name, item->pickup_name)) {
            break;
        }
    }
    if (!item->classname) {
        return;
    }

    it = G_Spawn();
    VectorCopy(ent->r.currentOrigin, it->s.origin);
    it->classname = item->classname;
    G_SpawnItem(it, item, qtrue);
    FinishSpawningItem(it);
    Touch_Item(it, ent, NULL);
    if (it->inuse) {
        G_FreeEntity(it);
    }
}

static void RR_SpawnLoadout(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (client->ps.pm_flags & PMF_LOADOUT_FORCED) {
        client->ps.stats[STAT_WEAPONS] = 1 << WP_GAUNTLET;
        client->pers.maxHealth = g_rrInfectedZombieHealthBonus.integer + g_startingHealth.integer;
        ent->health = client->pers.maxHealth;
        client->ps.weapon = WP_GAUNTLET;
        return;
    }
    SelectSpawnWeapon(ent);
}

static void G_SpawnLoadout(gentity_t* ent, gentity_t* spawnPoint) {
    gclient_t* client = ent->client;
    gitem_t* item;
    int w;

    client->ps.ammo[WP_GAUNTLET] = -1;
    client->ps.ammo[WP_GRAPPLING_HOOK] = -1;
    RegisterItem(BG_FindItemForWeapon(WP_GAUNTLET));
    RegisterItem(BG_FindItemForWeapon(WP_MACHINEGUN));
    RegisterItem(BG_FindItemForWeapon(WP_GRAPPLING_HOOK));
    client->ps.weaponstate = WEAPON_READY;
    client->ps.weaponTime = 0;

    if (g_playerCylinders.integer) {
        ent->r.svFlags |= SVF_CAPSULE;
    } else {
        ent->r.svFlags &= ~SVF_CAPSULE;
    }

    if (g_loadout.integer && !(client->ps.pm_flags & PMF_LOADOUT_FORCED) &&
        (!BG_WeaponInLoadout(client->sess.weaponPrimary) ||
         (g_disableLoadout.integer & (1 << client->sess.weaponPrimary)))) {
        if (!(ent->r.svFlags & SVF_BOT)) {
            client->sess.weaponPrimary = WP_HMG;
        } else {
            int tries = 0;
            int picked = 0;

            while (!picked) {
                int r;

                if (tries >= 16) {
                    picked = WP_HMG;
                    break;
                }
                r = (int)((rand() & 0x7fff) / 32767.0 * 16.0);
                if (BG_WeaponInLoadout(r)) {
                    picked = r;
                }
                tries++;
            }
            client->sess.weaponPrimary = picked;
        }
    }

    G_GiveStartingWeapons(ent);
    G_SetInfiniteAmmo(ent);

    if (level.intermissionTime) {
        MoveClientToIntermission(ent);
        return;
    }
    if (client->ps.pm_type == PM_SPECTATOR) {
        return;
    }

    G_SetStartingHealth(ent);
    ent->s.armor = client->ps.stats[STAT_ARMOR] = g_startingArmor.integer;
    if (armor_tiered.integer && g_startingArmor.integer) {
        if (g_startingArmor.integer >= 150) {
            client->ps.stats[STAT_ARMORTYPE] = 2;
        } else if (g_startingArmor.integer >= 100) {
            client->ps.stats[STAT_ARMORTYPE] = 1;
        } else {
            client->ps.stats[STAT_ARMORTYPE] = 0;
        }
    }

    if (g_instaGib.integer) {
        if (g_gametype.integer != GT_RR || !g_rrInfected.integer) {
            return;
        }
        RR_SpawnLoadout(ent);
    } else if (g_gametype.integer == GT_RR) {
        RR_SpawnLoadout(ent);
    }

    if (level.warmupTime && !g_training.integer && !(practiceflags.integer & 1) && !g_loadout.integer) {
        for (w = WP_MACHINEGUN; w < WP_NUM_WEAPONS; w++) {
            item = BG_FindItemForWeapon((weapon_t)w);
            if (!item || !itemRegistered[item - bg_itemlist]) {
                continue;
            }
            if ((w == WP_MACHINEGUN && !(g_startingWeapons.integer & 2)) ||
                (w == WP_GRAPPLING_HOOK && !(g_startingWeapons.integer & 0x200))) {
                continue;
            }
            if (g_gametype.integer != GT_RR || !g_rrInfected.integer || client->sess.sessionTeam != TEAM_RED) {
                client->ps.stats[STAT_WEAPONS] |= 1 << w;
            }
            if (g_infiniteAmmo.integer || g_instaGib.integer) {
                client->ps.ammo[w] = -1;
            } else {
                client->ps.ammo[w] = maxAmmoStandard[w];
            }
        }
        if (g_gametype.integer != GT_RACE && g_gametype.integer != GT_CA &&
            g_gametype.integer != GT_DOMINATION && g_gametype.integer != GT_RR) {
            return;
        }
    }

    G_UseTargets(spawnPoint, ent);
    if (practiceflags.integer & 1) {
        return;
    }
    SelectSpawnWeapon(ent);
}

void ClientSpawn(gentity_t* ent) {
    int index;
    vec3_t spawn_origin, spawn_angles;
    gclient_t* client;
    int i;
    clientPersistant_t saved;
    clientSession_t savedSess;
    int persistant[MAX_PERSISTANT];
    gentity_t* spawnPoint;
    int flags;
    int savedPing;
    int accuracy_hits, accuracy_shots;
    int eventSequence;
    int savedPmType;
    expandedStatObj_t savedStats;
    int savedRevenge[64];
    int localPersistant[MAX_PERSISTANT];
    int savedLastClientKilled, savedLastKilled;
    int savedRoundShots, savedRoundHits, savedRoundDamage;
    int savedBestRace[64];
    char userinfo[MAX_INFO_STRING];

    index = ent - g_entities;
    client = ent->client;

    VectorClear(spawn_origin);

    if (client->ps.pm_type == PM_SPECTATOR) {
        spawnPoint = SelectSpectatorSpawnPoint(
            spawn_origin, spawn_angles);
    } else {
        spawnPoint = SelectClientSpawnPoint(ent, spawn_origin, spawn_angles);
        if (!spawnPoint) {
            client->spawnRetryTime = level.time + 600;
            client->ps.pm_type = PM_SPECTATOR;
            client->failedSpawns++;
            return;
        }
        client->failedSpawns = 0;
    }

    if (client->ps.pm_type != PM_SPECTATOR) {
        G_TempEntity(spawn_origin, EV_PLAYER_TELEPORT_IN);
    }

    client->pers.teamState.state = TEAM_ACTIVE;

    ent->s.eFlags &= ~EF_KAMIKAZE;
    ent->client->ps.eFlags &= ~EF_KAMIKAZE;
    ent->s.eFlags &= ~EF_TICKING;
    ent->client->ps.eFlags &= ~EF_TICKING;

    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        ent->client->ps.eFlags |= EF_SPECTATOR;
    } else {
        ent->client->ps.eFlags &= ~EF_SPECTATOR;
    }

    flags = ent->client->ps.eFlags & (EF_TELEPORT_BIT | EF_SPECTATOR);
    flags ^= EF_TELEPORT_BIT;

    saved = client->pers;
    savedSess = client->sess;
    savedPing = client->ps.ping;
    accuracy_hits = client->accuracy_hits;
    accuracy_shots = client->accuracy_shots;

    savedStats = client->expandedStats;
    memcpy(savedRevenge, client->revengeCounter, sizeof(savedRevenge));
    savedLastClientKilled = client->lastClientKilled;
    savedLastKilled = client->lastkilled_client;
    savedRoundShots = client->round_shots;
    savedRoundHits = client->round_hits;
    savedRoundDamage = client->round_damage;
    memcpy(savedBestRace, client->race.best_race, sizeof(savedBestRace));

    for (i = 0; i < MAX_PERSISTANT; i++) {
        persistant[i] = client->ps.persistant[i];
        localPersistant[i] = client->ps.localPersistant[i];
    }
    eventSequence = client->ps.eventSequence;
    savedPmType = client->ps.pm_type;

    Com_Memset(client, 0, sizeof(*client));

    client->pers = saved;
    client->sess = savedSess;
    client->ps.ping = savedPing;
    client->accuracy_hits = accuracy_hits;
    client->accuracy_shots = accuracy_shots;
    client->expandedStats = savedStats;
    memcpy(client->revengeCounter, savedRevenge, sizeof(savedRevenge));
    client->lastClientKilled = savedLastClientKilled;
    client->lastkilled_client = savedLastKilled;
    client->round_shots = savedRoundShots;
    client->round_hits = savedRoundHits;
    client->round_damage = savedRoundDamage;
    client->lasthurt_client[0] = -1;
    client->lasthurt_client[1] = -1;

    for (i = 0; i < MAX_PERSISTANT; i++) {
        client->ps.persistant[i] = persistant[i];
        client->ps.localPersistant[i] = localPersistant[i];
    }
    client->ps.eventSequence = eventSequence;

    if (savedPmType == PM_SPECTATOR) {
        client->ps.pm_type = PM_SPECTATOR;
    }

    if (client->sess.sessionTeam != TEAM_SPECTATOR) {
        client->ps.persistant[PERS_SPAWN_COUNT]++;
    }
    client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

    client->airOutTime = level.time + 12000;

    trap_GetUserinfo(index, userinfo, sizeof(userinfo));
    if (pmove_AutoHop.integer) {
        if (!atoi(Info_ValueForKey(userinfo, "cg_autoHop"))) {
            client->ps.pm_flags |= PMF_NO_AUTOHOP;
        } else {
            client->ps.pm_flags &= ~PMF_NO_AUTOHOP;
        }
    }
    if (pmove_CrouchSlide.integer) {
        client->ps.pm_flags |= PMF_CROUCH_SLIDE;
    }
    if (pmove_DoubleJump.integer) {
        client->ps.pm_flags |= PMF_DOUBLE_JUMPED;
    }
    if (pmove_AirControl.integer) {
        client->ps.pm_flags |= PMF_PROMODE;
    }

    client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
    client->ps.eFlags = flags;

    ent->s.groundEntityNum = ENTITYNUM_NONE;
    ent->client = &level.clients[index];
    ent->takedamage = qtrue;
    ent->inuse = qtrue;
    ent->classname = "player";
    if (client->ps.pm_type == PM_SPECTATOR) {
        ent->r.contents = 0;
        ent->clipmask = 0;
    } else {
        ent->r.contents = CONTENTS_BODY;
        ent->clipmask = MASK_PLAYERSOLID;
    }
    ent->die = player_die;
    ent->waterlevel = 0;
    ent->watertype = 0;
    ent->flags = 0;

    VectorCopy(playerMins, ent->r.mins);
    VectorCopy(playerMaxs, ent->r.maxs);

    client->ps.clientNum = index;

    if (g_spawnArmor.integer && g_gametype.integer != GT_CA) {
        client->ps.powerups[PW_NONE] = (level.time / 1000) * 1000 + g_spawnArmor.integer;
    }
    if (g_regenHealth.integer || g_regenArmor.integer) {
        client->healthRegenActive = (qboolean)(g_regenHealth.integer != 0);
        client->armorRegenActive = (qboolean)(g_regenArmor.integer != 0);
        client->lasthurt_time[0] = level.time;
    }

    G_SetOrigin(ent, spawn_origin);
    VectorCopy(spawn_origin, client->ps.origin);

    client->ps.pm_flags |= PMF_RESPAWNED;

    switch (g_gametype.integer) {
    case GT_RACE:
        memcpy(client->race.best_race, savedBestRace, sizeof(savedBestRace));
        break;
    case GT_CA:
    case GT_AD:
        if (level.warmupTime > 0 || level.roundState.eCurrent == RS_COUNTDOWN) {
            client->ps.pm_flags |= PMF_FROZEN;
        }
        break;
    case GT_FREEZE:
        if ((level.warmupTime > 0 || level.roundState.eCurrent == RS_COUNTDOWN) && g_freezeRoundDelay.integer) {
            client->ps.pm_flags |= PMF_FROZEN;
        }
        break;
    case GT_RR:
        ClientSpawn_RedRover(ent);
        break;
    default:
        break;
    }

    G_SpawnLoadout(ent, spawnPoint);

    trap_GetUsercmd(client - level.clients, &ent->client->pers.cmd);
    SetClientViewAngle(ent, spawn_angles);

    if (client->ps.pm_type != PM_SPECTATOR) {
        G_KillBox(ent);
        trap_LinkEntity(ent);
    }

    client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    client->ps.pm_time = 100;

    client->respawnTime = level.time;

    client->ps.torsoAnim = TORSO_STAND;
    client->ps.legsAnim = LEGS_IDLE;

    client->ps.commandTime = level.time - 100;
    ent->client->pers.cmd.serverTime = level.time;
    ClientThink(ent - g_entities);

    if (client->ps.pm_type != PM_SPECTATOR) {
        if (g_grantItemOnSpawn.string[0]) {
            char grant[1024];
            char* tok;

            Q_strncpyz(grant, g_grantItemOnSpawn.string, sizeof(grant));
            for (tok = strtok(grant, ";"); tok; tok = strtok(NULL, ";")) {
                G_GrantItem(ent, tok);
            }
        }
        BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
        VectorCopy(client->ps.origin, ent->r.currentOrigin);
        trap_LinkEntity(ent);
    }

    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Clear(ent);
    }

    if (client->ps.pm_type == PM_SPECTATOR && client->sess.spectatorState == SPECTATOR_FOLLOW &&
        client->sess.spectatorClient >= 0) {
        client->pendingFollowClient = client->sess.spectatorClient;
        client->pendingFollow = qtrue;
    }

    ClientEndFrame(ent);

    if (g_gametype.integer >= GT_TEAM) {
        level.nextTeamInfoTime = level.time + 1000;
    }

    if (client->ps.pm_type == PM_SPECTATOR) {
        StopFollowing(ent);
    }

    BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
}

void ClientDisconnect(int clientNum) {
    gentity_t* ent;
    gentity_t* tent;
    int i;
    gclient_t* cl;

    ent = g_entities + clientNum;

    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Clear(ent);
    }

    if (ent->client && ent->client->ps.pm_type != PM_INTERMISSION) {
        STAT_PublishClientDisconnect(ent->client, 1);
    }

    G_RemoveQueuedBotBegin(clientNum);

    if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED) {
        return;
    }

    if (g_gametype.integer == GT_DUEL) {
        DuelScoreboardMessage(ent);
    }

    Team_LivingTeamCounts(NULL, NULL);

    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];

        if (cl->sess.sessionTeam == TEAM_SPECTATOR
            && cl->sess.spectatorState == SPECTATOR_FOLLOW
            && cl->sess.spectatorClient == clientNum) {
            if (ent->client->sess.sessionTeam == TEAM_SPECTATOR
                || level.warmupTime
                || level.numPlayingClients < 1) {
                StopFollowing(&g_entities[i]);
            } else {
                Cmd_FollowCycle_f(&g_entities[i], 1);
            }
        }

        if (level.clients[i].hook
            && level.clients[i].hook->enemy == ent) {
            G_ReleaseGrapple(&g_entities[i]);
        }

        if (cl->pers.complaintClient == clientNum) {
            cl->pers.complaintClient = -1;
            cl->pers.complaintEndTime = 0;
            trap_SendServerCommand(level.sortedClients[i], "complaint -2");
        }

        cl->pers.damageFromTeammates = 0;
    }

    if (ent->client->pers.connected == CON_CONNECTED
        && ent->client->sess.sessionTeam != TEAM_SPECTATOR
        && !level.intermissionQueued && !level.intermissionTime) {
        tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT);
        tent->s.clientNum = ent->s.clientNum;

        TossClientItems(ent);

        if (ent->client->hook) {
            G_ReleaseGrapple(ent);
        }

        if (ent->client->ps.stats[STAT_KEY]) {
            if (ent->client->ps.stats[STAT_KEY] & KEY_SILVER) G_RespawnKey(KEY_SILVER);
            if (ent->client->ps.stats[STAT_KEY] & KEY_GOLD)   G_RespawnKey(KEY_GOLD);
            if (ent->client->ps.stats[STAT_KEY] & KEY_MASTER) G_RespawnKey(KEY_MASTER);
            ent->client->ps.stats[STAT_KEY] = 0;
        }
    }

    G_LogPrintf("ClientDisconnect: %i\n", clientNum);

    if ((g_gametype.integer == GT_DUEL) && !level.intermissionTime && !level.warmupTime && level.sortedClients[1] == clientNum) {
        level.clients[level.sortedClients[0]].sess.wins++;
        ClientUserinfoChanged(level.sortedClients[0]);
    }

    STAT_LogDisconnect(clientNum);

    trap_UnlinkEntity(ent);
    ent->s.modelindex = 0;
    ent->inuse = qfalse;
    ent->classname = "disconnected";
    ent->client->pers.connected = CON_DISCONNECTED;
    ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
    ent->client->sess.sessionTeam = TEAM_FREE;
    ent->client->sess.privileges = 0;

    trap_SetConfigstring(CS_PLAYERS + clientNum, "");

    CalculateRanks();

    if (ent->r.svFlags & SVF_BOT) {
        BotAIShutdownClient(clientNum, qfalse);
    }

    switch (g_gametype.integer) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        Team_LivingTeamCounts(NULL, NULL);
        break;
    }
}

void SelectSpawnWeapon(gentity_t* ent) {
    gclient_t* client = ent->client;
    char* p;
    char* token;
    int n, w, i;

    if (client->ps.pm_flags & PMF_LOADOUT_FORCED) {
        client->ps.weapon = WP_GAUNTLET;
        return;
    }

    if (g_loadout.integer && client->sess.weaponPrimary > 0 &&
        !(g_disableLoadout.integer & (1 << client->sess.weaponPrimary))) {
        client->ps.weapon = client->sess.weaponPrimary;
        return;
    }

    p = g_bestStartingWeapons.string;
    w = 0;
    for (n = 1; ; n++) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0]) {
            return;
        }
        for (i = WP_GAUNTLET; i < WP_NUM_WEAPONS; i++) {
            if (weaponShortName[i][0] && !Q_stricmp(token, weaponShortName[i])) {
                w = i;
            }
        }
        if (!w) {
            w = atoi(token);
        }
        if (w >= WP_GAUNTLET && w <= WP_HMG && (client->ps.stats[STAT_WEAPONS] & (1 << w))) {
            break;
        }
        if (n >= 14) {
            return;
        }
    }
    client->ps.weapon = w;
}

void Team_LivingTeamCounts(int *outRed, int *outBlue) {
    int counts[TEAM_NUM_TEAMS] = { 0 };
    int i;

    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];

        if (cl->pers.connected == CON_CONNECTED && cl->ps.pm_type == PM_NORMAL) {
            counts[cl->sess.sessionTeam]++;
        }
    }
    if (outRed) *outRed = counts[TEAM_RED];
    if (outBlue) *outBlue = counts[TEAM_BLUE];
}

void Team_ConnectedTeamCounts(int counts[TEAM_NUM_TEAMS]) {
    int i;

    memset(counts, 0, TEAM_NUM_TEAMS * sizeof(counts[0]));
    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];

        if (cl->pers.connected == CON_CONNECTED) {
            counts[cl->sess.sessionTeam]++;
        }
    }
}

void G_ReleaseGrapple(gentity_t* ent) {
    if (!ent || !ent->client) return;
    if (ent->client->hook) {
        Weapon_HookFree(ent->client->hook);
    }
}

void STAT_InitClient(gentity_t* ent) {
    gclient_t* client = ent->client;
    int r;

    memset(&client->expandedStats, 0, sizeof(client->expandedStats));
    client->expandedStats.lastThinkTime = level.time;
    client->expandedStats.teamJoinTime = level.time - level.startTime;
    r = rand();
    client->expandedStats.statId = level.time + r + 636 + (int)(intptr_t)client;
}
void STAT_PublishClientDisconnect(gclient_t* client, int reason) {
    STAT_PlayerStats(client, qfalse, (qboolean)(reason != 0));
}
void STAT_SubscribeClient(gentity_t* ent) {
    STAT_PlayerConnect(ent);
}
void STAT_LogDisconnect(int clientNum) {
    STAT_PlayerDisconnect(&g_entities[clientNum]);
}
void STAT_MatchEnd(int isShutdown, int restart, const char* exitMsg) {
    int i;

    if (level.gameStatsReported || trap_Cvar_VariableIntegerValue("g_restarted")) {
        return;
    }
    for (i = 0; i < level.numConnectedClients; i++) {
        gclient_t* cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == CON_CONNECTING || cl->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        if (g_gametype.integer == GT_DUEL && !trap_HasAchievement(level.sortedClients[i], 0x37) &&
            cl->ps.persistant[PERS_KILLED] == 0 && cl->expandedStats.serverRank == 0) {
            trap_SetAchievement(level.sortedClients[i], 0x37);
        }
        STAT_PlayerStats(cl, (qboolean)(isShutdown == 1), qfalse);
    }
    STAT_MatchReport((qboolean)(isShutdown == 1), restart, exitMsg);
}

