#include "g_local.h"

#define MAX_DOM_POINTS      5
#define DOM_CAPTURE_LIMIT   1000

static gentity_t *domPoints[MAX_DOM_POINTS];
static int numDomPoints;

static int domScoreTick;
static int domPendingScores[4];

static void DOM_Think(gentity_t *ent);
static void DOM_FrameThink(gentity_t *ent);

#define DOM_AWARD_MASK  (EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | \
                         EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | \
                         EF_AWARD_DEFEND | EF_AWARD_CAP)

static void DOM_CheckFlagScore(gentity_t *ent, gentity_t **players, int count) {
    int i;

    for (i = 0; i < count; i++) {
        gentity_t *player = players[i];
        gclient_t *cl = player->client;

        if (ent->domCapturer != NULL && player == ent->domCapturer) {
            cl->pers.teamState.captures++;
            cl->expandedStats.numCaptures++;
            cl->ps.persistant[PERS_CAPTURES]++;
            cl->ps.eFlags &= ~DOM_AWARD_MASK;
            cl->ps.eFlags |= EF_AWARD_CAP;
            cl->rewardTime = level.time + REWARD_SPRITE_TIME;
            AddScore(player, ent->r.currentOrigin, 25);
            STAT_AddPlayerMedalStat(player, "CAPTURE");
        } else {
            cl->pers.teamState.assists++;
            cl->expandedStats.numAssists++;
            AddScore(player, ent->r.currentOrigin, 15);
            cl->ps.persistant[PERS_ASSIST_COUNT]++;
            cl->ps.eFlags &= ~DOM_AWARD_MASK;
            cl->ps.eFlags |= EF_AWARD_ASSIST;
            cl->rewardTime = level.time + REWARD_SPRITE_TIME;
            STAT_AddPlayerMedalStat(player, "ASSIST");
        }
    }
}

static void DOM_AddTeamScores(int redCount, int blueCount, gentity_t *ent,
                              gentity_t **redPlayers, gentity_t **bluePlayers) {
    gentity_t *prev;
    int j;

    if (redCount == 0) {
        if (blueCount > 0) {
            if (blueCount == 1) {
                ent->domCapturer = bluePlayers[0];
                return;
            }
            prev = ent->domCapturer;
            if (prev != NULL && prev->client != NULL) {
                if (prev->client->sess.sessionTeam == TEAM_RED) {
                    ent->domCapturer = bluePlayers[0];
                    return;
                }
                if (prev->client->sess.sessionTeam == TEAM_BLUE) {
                    for (j = 0; j < blueCount; j++) {
                        if (prev == bluePlayers[j])
                            return;
                    }
                    ent->domCapturer = bluePlayers[0];
                    return;
                }
            }
        }
    } else if (redCount > 0) {
        if (blueCount == 0) {
            if (redCount == 1) {
                ent->domCapturer = redPlayers[0];
                return;
            }
            prev = ent->domCapturer;
            if (prev != NULL && prev->client != NULL) {
                if (prev->client->sess.sessionTeam == TEAM_BLUE) {
                    ent->domCapturer = redPlayers[0];
                    return;
                }
                if (prev->client->sess.sessionTeam == TEAM_RED) {
                    for (j = 0; j < redCount; j++) {
                        if (prev == redPlayers[j])
                            return;
                    }
                    ent->domCapturer = redPlayers[0];
                    return;
                }
            }
        } else if (blueCount > 0) {
            ent->domCapturer = NULL;
        }
    }
}

static void DOM_FrameThink(gentity_t *ent) {
    int i;
    int redCount = 0, blueCount = 0;
    gentity_t *redPlayers[MAX_CLIENTS];
    gentity_t *bluePlayers[MAX_CLIENTS];
    float radiusSq;
    int deltaBase;
    int capDelta;
    int oldAccum, accum, progress;
    float scale;
    double base;
    qboolean neutralized = qfalse;

    ent->s.weapon = 0;

    radiusSq = (float)(ent->count * ent->count);
    for (i = 0; i < MAX_CLIENTS; i++) {
        gentity_t *player = &g_entities[i];
        gclient_t *cl = player->client;

        if (!player->inuse || !cl)
            continue;
        if (cl->pers.connected != CON_CONNECTED)
            continue;
        if (cl->sess.sessionTeam == TEAM_SPECTATOR)
            continue;
        if (player->health <= 0)
            continue;
        if (DistanceSquared(ent->r.currentOrigin, player->r.currentOrigin) > radiusSq)
            continue;

        if (cl->sess.sessionTeam == TEAM_RED)
            redPlayers[redCount++] = player;
        else
            bluePlayers[blueCount++] = player;
    }

    DOM_AddTeamScores(redCount, blueCount, ent, redPlayers, bluePlayers);

    ent->s.otherEntityNum2 = 0;
    if (redCount == 0 || blueCount == 0)
        ent->s.otherEntityNum2 = redCount + blueCount;

    deltaBase = 0;
    if (g_domCapTime.integer > 0)
        deltaBase = level.frametime / g_domCapTime.integer;

    scale = g_domTeammateCapScale.value;
    base = (float)deltaBase;
    capDelta = 0;

    if (g_domEnableContention.integer != 0 && g_domNeutralFlag.integer != 0 &&
        redCount != 0 && blueCount != 0) {
        oldAccum = ent->health;
        if (oldAccum != 0) {
            capDelta = (int)((base + base * scale) * (oldAccum > 0 ? -1.0 : 1.0));
            if (capDelta != 0 && ent->health + capDelta == 0)
                neutralized = qtrue;
        }
    } else {
        if (redCount > 0)
            capDelta = (int)(0.0 - (base + base * scale * (redCount - 1)));
        if (blueCount > 0)
            capDelta = (int)(base + base * scale * (blueCount - 1) + capDelta);
    }

    oldAccum = ent->health;
    accum = oldAccum + capDelta;
    if (accum > DOM_CAPTURE_LIMIT)
        accum = DOM_CAPTURE_LIMIT;
    else if (accum < -DOM_CAPTURE_LIMIT)
        accum = -DOM_CAPTURE_LIMIT;
    ent->health = accum;

    progress = (accum < 0 ? -accum : accum) / 10;
    ent->s.generic1 = progress;
    ent->s.weapon = 0;
    ent->s.eventParm = (accum > 0) + 1;

    if (redCount != 0 && blueCount != 0 && redCount == blueCount) {
        ent->s.weapon = 2;
    } else if (redCount != 0 && blueCount != 0) {
        ent->s.weapon = 1;
    } else if ((ent->s.modelindex2 == 0 && progress < 100 && progress != 0) ||
               ((float)progress < g_domDistressThreshold.value && progress != 0)) {
        ent->s.weapon = 1;
    } else {
        ent->s.weapon = 0;
    }

    if (capDelta == 0)
        return;

    {
        int absOld = oldAccum < 0 ? -oldAccum : oldAccum;
        int absDelta = deltaBase < 0 ? -deltaBase : deltaBase;
        if (neutralized || (absOld < absDelta && ent->s.modelindex2 != 0)) {
            if (g_domNeutralFlag.integer == 0) {
                ent->health = (ent->s.modelindex2 == TEAM_RED) ? DOM_CAPTURE_LIMIT
                                                               : -DOM_CAPTURE_LIMIT;
                ent->s.modelindex2 = 0;
            } else {
                gentity_t *te;
                ent->s.modelindex2 = 0;
                te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
                te->r.svFlags |= SVF_BROADCAST;
                te->s.eventParm = GTS_DOMINATION_POINT_CAPTURE;
                te->s.modelindex2 = ent->s.modelindex2;
            }
        }
    }

    accum = ent->health;
    if ((accum == DOM_CAPTURE_LIMIT || accum == -DOM_CAPTURE_LIMIT) &&
        ent->s.modelindex2 == 0) {
        int team = (accum == -DOM_CAPTURE_LIMIT) ? TEAM_RED : TEAM_BLUE;
        gentity_t *te;

        ent->s.modelindex2 = team;

        te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
        te->r.svFlags |= SVF_BROADCAST;
        te->s.eventParm = GTS_DOMINATION_POINT_CAPTURE;
        te->s.modelindex2 = team;
        te->s.powerups = level.warmupTime ? 0 : ent->s.powerups;
        if (level.warmupTime && ent->domCapturer && ent->domCapturer->client) {
            te->r.svFlags |= SVF_NOTSINGLECLIENT;
            te->r.singleClient = ent->domCapturer->client->ps.clientNum;
        }

        if (team == TEAM_RED)
            DOM_CheckFlagScore(ent, redPlayers, redCount);
        else
            DOM_CheckFlagScore(ent, bluePlayers, blueCount);
    }
}

static void DOM_Think(gentity_t *ent) {
    int red, blue, newRed, newBlue;
    int announce;

    ent->nextthink = g_domScoreRate.integer * 1000 + level.time;

    if (level.warmupTime || level.intermissionQueued || level.intermissionTime)
        return;

    if (ent->s.modelindex2 != 0)
        domPendingScores[ent->s.modelindex2]++;

    domScoreTick++;
    if (domScoreTick < numDomPoints)
        return;

    {
        int owner = domPoints[0]->s.modelindex2;
        int held = 0;
        int i;

        for (i = 0; i < numDomPoints; i++) {
            if (!domPoints[i]->inuse ||
                (domPoints[i]->s.modelindex2 != 0 && domPoints[i]->s.modelindex2 == owner)) {
                held++;
            }
        }
        if (held >= 3) {
            for (i = 0; i < MAX_CLIENTS; i++) {
                gclient_t *cl = g_entities[i].client;

                if (!cl || cl->sess.sessionTeam != owner) {
                    continue;
                }
                if (!trap_HasAchievement(cl->ps.clientNum, 0x29) && cl->expandedStats.domThreeFlagsTime < 30 &&
                    cl->expandedStats.domThreeFlagsTime + g_domScoreRate.integer >= 30) {
                    trap_SetAchievement(cl->ps.clientNum, 0x29);
                }
                cl->expandedStats.domThreeFlagsTime += g_domScoreRate.integer;
            }
        }
    }

    red = level.teamScores[TEAM_RED];
    blue = level.teamScores[TEAM_BLUE];
    newRed = red + domPendingScores[TEAM_RED];
    newBlue = blue + domPendingScores[TEAM_BLUE];

    domScoreTick = 0;

    if (level.intermissionQueued || level.intermissionTime)
        return;

    if (!level.warmupTime) {
        announce = 0;
        if (blue < red) {
            if (newBlue > newRed)
                announce = GTS_BLUETEAM_TOOK_LEAD;
        } else if (red < blue) {
            if (newRed > newBlue)
                announce = GTS_REDTEAM_TOOK_LEAD;
        } else {
            if (newRed > newBlue)
                announce = GTS_REDTEAM_TOOK_LEAD;
            else if (newBlue > newRed)
                announce = GTS_BLUETEAM_TOOK_LEAD;
        }
        if (announce) {
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = announce;
        }
    }

    level.teamScores[TEAM_RED] += domPendingScores[TEAM_RED];
    level.teamScores[TEAM_BLUE] += domPendingScores[TEAM_BLUE];
    domPendingScores[TEAM_FREE] = 0;
    domPendingScores[TEAM_RED] = 0;
    domPendingScores[TEAM_BLUE] = 0;
    domPendingScores[TEAM_SPECTATOR] = 0;
    CalculateRanks();
}

static void DOM_FlagFirstThink(gentity_t *ent) {
    trace_t tr;
    vec3_t start, end;
    vec3_t mins = { -15, -15, -15 };
    vec3_t maxs = { 15, 15, 35 };

    VectorCopy(ent->r.currentOrigin, start);
    VectorCopy(ent->r.currentOrigin, end);
    end[2] -= 256.0f;

    trap_Trace(&tr, start, mins, maxs, end, ENTITYNUM_NONE, CONTENTS_SOLID);
    if (tr.startsolid) {
        start[2] += 128.0f;
        trap_Trace(&tr, start, mins, maxs, end, ENTITYNUM_NONE, CONTENTS_SOLID);
        if (tr.startsolid)
            goto done;
    }

    if (tr.fraction < 1.0f) {
        VectorCopy(tr.endpos, ent->s.pos.trBase);
        ent->s.pos.trType = TR_STATIONARY;
        ent->s.pos.trTime = 0;
        ent->s.pos.trDuration = 0;
        VectorClear(ent->s.pos.trDelta);
        VectorCopy(tr.endpos, ent->r.currentOrigin);
    }

done:
    ent->nextthink = g_domScoreRate.integer * 1000 + level.time;
    ent->think = DOM_Think;
    ent->framethink = DOM_FrameThink;
}

void DOM_CheckPlayers(void) {
    int i;
    int red = 0, blue = 0;

    for (i = 0; i < numDomPoints; i++) {
        if (domPoints[i]->s.modelindex2 == TEAM_RED)
            red++;
        else if (domPoints[i]->s.modelindex2 == TEAM_BLUE)
            blue++;
    }

    trap_SetConfigstring(700, va("%d", red));
    trap_SetConfigstring(701, va("%d", blue));
}

void DOM_FragBonuses(gentity_t *attacker, gentity_t *victim) {
    int i;

    for (i = 0; i < numDomPoints; i++) {
        gentity_t *point = domPoints[i];

        if (point->s.modelindex2 != attacker->client->sess.sessionTeam)
            continue;
        if (Distance(point->r.currentOrigin, victim->r.currentOrigin) >= 500.0f)
            continue;
        if (!trap_InPVS(point->r.currentOrigin, victim->r.currentOrigin))
            continue;
        if (attacker->client->sess.sessionTeam == victim->client->sess.sessionTeam)
            continue;

        attacker->client->pers.teamState.basedefense++;
        attacker->client->expandedStats.numDefends++;
        AddScore(attacker, victim->r.currentOrigin, 10);
        attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
        attacker->client->ps.eFlags &= ~DOM_AWARD_MASK;
        attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
        attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
        STAT_AddPlayerMedalStat(attacker, "DEFENSE");
        return;
    }
}

void SP_team_dom_point(gentity_t *ent) {
    char *s;

    if (g_gametype.integer != GT_DOMINATION) {
        G_FreeEntity(ent);
        return;
    }

    ent->r.svFlags |= SVF_BROADCAST;
    ent->health = 0;
    ent->s.groundEntityNum = ent->count;
    ent->s.loopSound = G_SoundIndex("sound/ambient/loop_elec_hum_01_short.wav");
    ent->s.modelindex = G_ModelIndex("models/flag3/d_flag3.md3");
    G_SpawnString("identifier", "1", &s);
    ent->s.powerups = atoi(s);
    ent->s.modelindex2 = 0;

    ent->think = DOM_FlagFirstThink;
    ent->nextthink = level.time + 1;

    if (numDomPoints < MAX_DOM_POINTS)
        domPoints[numDomPoints++] = ent;

    trap_LinkEntity(ent);
}
