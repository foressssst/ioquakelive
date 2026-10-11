#include "g_local.h"

#define MAX_RACE_POINTS      64

static const char *Race_TimeString(int msec) {
    float seconds;
    int minutes;

    seconds = (float)abs(msec) / (float)1000.0;
    minutes = (int)seconds / 60;
    seconds = seconds - (float)minutes * (float)60.0;
    return va("%s%d:%s%.03f", msec < 0 ? "-" : "", minutes, seconds < 10.0f ? "0" : "", seconds);
}

void RaceScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int best, ping;

    string[0] = 0;
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        best = cl->ps.persistant[PERS_SCORE];
        if (best == 0x7fffffff) {
            best = -1;
        }

        Com_sprintf(entry, sizeof(entry), " %i %i %i %i",
                    level.sortedClients[i], best, ping,
                    (level.time - cl->pers.enterTime) / 60000);
        j = strlen(entry);
        if (stringlength + j > SCORES_LIST_ROOM) {
            break;
        }
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
                           va("scores_race %i%s", level.numPlayingClients, string));
}

static void FinishSpawningRacePoint(gentity_t *ent) {
    trace_t tr;
    vec3_t start, dest;

    if (!(ent->spawnflags & 1)) {
        VectorSet(dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 256);
        trap_Trace(&tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ENTITYNUM_NONE, CONTENTS_SOLID);
        if (tr.startsolid) {
            G_Printf("FinishSpawningItem: race_point startsolid at %s\n", vtos(ent->s.origin));
            VectorSet(start, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] + 128);
            trap_Trace(&tr, start, ent->r.mins, ent->r.maxs, dest, ENTITYNUM_NONE, CONTENTS_SOLID);
        }
        if (!tr.startsolid && tr.fraction < 1.0f) {
            VectorCopy(tr.endpos, ent->s.origin);
            ent->s.groundEntityNum = tr.entityNum;
        }
    } else {
        G_SetOrigin(ent, ent->s.origin);
    }

    ent->s.origin[0] = (float)(int)ent->s.origin[0];
    ent->s.origin[1] = (float)(int)ent->s.origin[1];
    ent->s.origin[2] = (float)(int)(ent->s.origin[2] + 1.0);
    G_SetOrigin(ent, ent->s.origin);
}

void Touch_RaceCheckpoint(gentity_t *self, gentity_t *other, trace_t *trace) {
    gclient_t *cl = other->client;
    raceInfo_t *ri;
    gentity_t *te;
    int i;

    if (!cl || level.warmupTime > 0) {
        return;
    }
    ri = &cl->race;

    if (!self->targetname) {
        if (ri->racingActive == 1 && cl->pers.teamState.flagsince <= 999) {
            return;
        }
        ri->nextRacePoint = G_PickTarget(self->target);
        if (ri->nextRacePoint && ri->nextRacePoint->target) {
            ri->nextRacePoint2 = G_PickTarget(ri->nextRacePoint->target);
        }
        if (!ri->nextRacePoint) {
            return;
        }

        cl->pers.teamState.flagsince = 0;
        memset(ri->current_race, 0, sizeof(ri->current_race));

        te = G_TempEntity(cl->ps.origin, EV_RACE_START);
        te->s.clientNum = other - g_entities;
        te->s.otherEntityNum = ri->currentCheckPoint;
        te->s.otherEntityNum2 = ri->nextRacePoint->s.number;
        te->s.powerups = ri->nextRacePoint2 ? ri->nextRacePoint2->s.number : -1;
        te->s.time = level.time;
        te->r.svFlags = SVF_BROADCAST;

        ri->currentCheckPoint = 0;
        ri->startTime = level.time;
        ri->racingActive = 1;
        ri->weaponUsed = 0;
        return;
    }

    if (!ri->racingActive || ri->nextRacePoint != self) {
        return;
    }

    if (!self->target) {
        qboolean personalBest;

        for (i = 0; i < level.numPlayingClients; i++) {
            if (cl->pers.teamState.flagsince ==
                level.clients[level.sortedClients[i]].ps.persistant[PERS_SCORE]) {
                cl->pers.teamState.flagsince++;
            }
        }
        ri->lastTime = cl->pers.teamState.flagsince;

        STAT_PlayerRaceComplete(other, ri->lastTime, ri->weaponUsed);

        if (ri->lastTime < level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE]) {
            te = G_TempEntity(cl->ps.origin, EV_NEW_HIGH_SCORE);
            te->r.svFlags |= SVF_BROADCAST;
        }

        personalBest = ri->lastTime < cl->ps.persistant[PERS_SCORE];
        if (personalBest) {
            cl->ps.persistant[PERS_SCORE] = ri->lastTime;
        }

        te = G_TempEntity(cl->ps.origin, EV_RACE_FINISH);
        te->s.clientNum = other - g_entities;
        te->s.time = ri->lastTime;
        te->s.generic1 = 1;
        te->r.svFlags = SVF_BROADCAST;

        if (personalBest) {
            memcpy(ri->best_race, ri->current_race, sizeof(ri->best_race));
            PrintMsg(NULL, "^1Personal best! ^7%s^7 finished the race in in %s\n", cl->pers.netname,
                     Race_TimeString(cl->pers.teamState.flagsince));
        } else {
            PrintMsg(NULL, "%s^7 finished the race in %s\n", cl->pers.netname,
                     Race_TimeString(cl->pers.teamState.flagsince));
        }

        ri->nextRacePoint = NULL;
        ri->racingActive = 0;
        CalculateRanks();
    } else {
        int best = 0, split = 0;

        if (ri->currentCheckPoint < MAX_RACE_POINTS) {
            best = ri->best_race[ri->currentCheckPoint];
            if (best != 0) {
                split = cl->pers.teamState.flagsince - best;
            }
            ri->current_race[ri->currentCheckPoint] = cl->pers.teamState.flagsince;
        }
        ri->currentCheckPoint++;

        ri->nextRacePoint = G_PickTarget(self->target);
        if (ri->nextRacePoint && ri->nextRacePoint->target) {
            ri->nextRacePoint2 = G_PickTarget(ri->nextRacePoint->target);
        }

        te = G_TempEntity(cl->ps.origin, EV_RACE_CHECKPOINT);
        te->s.clientNum = other - g_entities;
        te->s.otherEntityNum = ri->currentCheckPoint;
        te->s.otherEntityNum2 = ri->nextRacePoint ? ri->nextRacePoint->s.number : -1;
        te->s.powerups = ri->nextRacePoint2 ? ri->nextRacePoint2->s.number : -1;
        te->s.time = split;
        te->r.svFlags = SVF_BROADCAST;
        te->s.generic1 = (best != 0);
    }
}

void SP_race_point(gentity_t *ent) {
    if (g_gametype.integer != GT_RACE) {
        G_FreeEntity(ent);
        return;
    }

    if (ent->target || ent->targetname) {
        ent->r.svFlags |= SVF_BROADCAST;
        VectorSet(ent->r.mins, -40, -40, -15);
        VectorSet(ent->r.maxs, 40, 40, 128);
        ent->r.contents = CONTENTS_TRIGGER;
        ent->s.modelindex2 = 0;
        ent->touch = Touch_RaceCheckpoint;
        ent->think = FinishSpawningRacePoint;
        ent->nextthink = level.time + 1;
        ent->s.generic1 = ent->spawnflags;
        if (ent->targetname && !ent->target) {
            ent->s.modelindex2 = 1;
        }
        ent->s.modelindex = G_ModelIndex("models/flag3/d_flag3.md3");

        if (level.numRacePoints < MAX_RACE_POINTS) {
            level.numRacePoints++;
            trap_LinkEntity(ent);
            return;
        }
    }
    G_FreeEntity(ent);
}

void RACE_ClientBegin(gentity_t *ent) {
    gclient_t *client = ent->client;

    memset(client->race.best_race, 0, sizeof(client->race.best_race));
    client->ps.persistant[PERS_SCORE] = 0x7fffffff;
    trap_SetConfigstring(CS_RACE_POINTS, va("%i", level.numRacePoints));
    trap_SendServerCommand(ent - g_entities, "race_init");
    ClientSpawn(ent);
}

void Cmd_RaceInit_f(gentity_t *ent) {
    ent->client->race.nextRacePoint = NULL;
    ent->client->race.nextRacePoint2 = NULL;
    ent->client->race.racingActive = 0;
}

static int raceAdminPoints;
static gentity_t *raceAdminLast;

void Cmd_RacePoint_f(gentity_t *ent) {
    char cmd[MAX_TOKEN_CHARS];
    gentity_t *point;
    vec3_t origin;

    if (g_gametype.integer != GT_RACE) {
        trap_SendServerCommand(ent - g_entities,
            "print \"RacePoint is only permitted in the Race gametype.\n\"");
        return;
    }
    if (!g_cheats.integer) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Cheats are not enabled on this server.\n\"");
        return;
    }
    if (ent->health <= 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You must be alive to use this command.\n\"");
        return;
    }

    trap_Argv(1, cmd, sizeof(cmd));

    if (!Q_stricmp(cmd, "clear")) {
        gentity_t *e = NULL;

        trap_SendServerCommand(-1, "print \"clearing race points\n\"");
        while ((e = G_Find(e, FOFS(classname), "race_point")) != NULL) {
            G_FreeEntity(e);
        }
        raceAdminLast = NULL;
        raceAdminPoints = 0;
        return;
    }

    if (!Q_stricmp(cmd, "dump")) {
        gentity_t *e = NULL;

        while ((e = G_Find(e, FOFS(classname), "race_point")) != NULL) {
            trap_SendServerCommand(-1, va("print \"%f %f %f\n\"",
                e->r.currentOrigin[0], e->r.currentOrigin[1], e->r.currentOrigin[2]));
        }
        return;
    }

    point = G_Spawn();
    if (raceAdminPoints > MAX_RACE_POINTS) {
        trap_SendServerCommand(-1, "print \"too many race points\n\"");
        return;
    }

    point->classname = "race_point";
    point->s.eType = ET_GENERAL;
    VectorCopy(ent->client->ps.origin, origin);
    origin[2] -= 8;
    G_SetOrigin(point, origin);
    point->targetname = G_NewString(va("arp%i", raceAdminPoints));
    if (raceAdminLast) {
        raceAdminLast->target = point->targetname;
    }
    SP_race_point(point);

    raceAdminPoints++;
    raceAdminLast = point;
    trap_SendServerCommand(-1, "print \"spawning a race point\n\"");
}
