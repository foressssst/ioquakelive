#include "g_local.h"

static void TeamCount_Health_FT(int *aliveCounts, int *healthTotals) {
    int i;
    (void)aliveCounts;
    healthTotals[TEAM_FREE] = 0;
    healthTotals[TEAM_RED] = 0;
    healthTotals[TEAM_BLUE] = 0;
    healthTotals[TEAM_SPECTATOR] = 0;

    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;
        if (cl->ps.pm_type != PM_NORMAL) continue;
        healthTotals[cl->sess.sessionTeam] += g_entities[i].health;
    }
}

static const char *FT_TeamColor(int team) {
    if (team == TEAM_RED) return "^1";
    if (team == TEAM_BLUE) return "^4";
    return "^7";
}

void Freeze_RoundStateTransition(void);

int Freeze_GetRoundState(void) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return -1;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        Freeze_RoundStateTransition();
    }
    return level.roundState.eCurrent;
}

qboolean Freeze_TeamFrozen(int team) {
    int i;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->sess.sessionTeam == team &&
            cl->ps.powerups[PW_FREEZE] == 0) {
            return qfalse;
        }
    }
    return qtrue;
}

qboolean Freeze_GameIsOver(int *aliveCounts, int *healthTotals) {
    if (roundtimelimit.integer != 0 &&
        roundtimelimit.integer * 1000 <= level.time - level.roundState.startTime) {
        if (g_roundDrawLivingCount.integer != 0) {
            if (aliveCounts[TEAM_BLUE] < aliveCounts[TEAM_RED]) {
                level.roundState.prevRoundWinningTeam = TEAM_RED;
                return qtrue;
            }
            if (aliveCounts[TEAM_RED] < aliveCounts[TEAM_BLUE]) {
                level.roundState.prevRoundWinningTeam = TEAM_BLUE;
                return qtrue;
            }
        }
        if (g_roundDrawHealthCount.integer != 0) {
            if (healthTotals[TEAM_BLUE] < healthTotals[TEAM_RED]) {
                level.roundState.prevRoundWinningTeam = TEAM_RED;
                return qtrue;
            }
            if (healthTotals[TEAM_RED] < healthTotals[TEAM_BLUE]) {
                level.roundState.prevRoundWinningTeam = TEAM_BLUE;
                return qtrue;
            }
        }
        level.roundState.prevRoundWinningTeam = TEAM_FREE;
        return qtrue;
    }

    if (aliveCounts[TEAM_RED] == 0 && aliveCounts[TEAM_BLUE] == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_FREE;
        return qtrue;
    }
    if (aliveCounts[TEAM_RED] == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_BLUE;
        return qtrue;
    }
    if (aliveCounts[TEAM_BLUE] == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_RED;
        return qtrue;
    }

    {
        qboolean redFrozen = Freeze_TeamFrozen(TEAM_RED);
        qboolean blueFrozen = Freeze_TeamFrozen(TEAM_BLUE);

        if (redFrozen && blueFrozen) {
            level.roundState.prevRoundWinningTeam = TEAM_FREE;
            return qtrue;
        }
        if (blueFrozen) {
            level.roundState.prevRoundWinningTeam = TEAM_RED;
            return qtrue;
        }
        if (redFrozen) {
            level.roundState.prevRoundWinningTeam = TEAM_BLUE;
            return qtrue;
        }
    }

    return qfalse;
}

void Freeze_RoundStateTransition(void) {
    int i;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        Freeze_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        trap_SetConfigstring(CS_ROUND_STATUS, "\\time\\-1\\round\\0");
        return;

    case RS_COUNTDOWN:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];

            if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR)
                continue;

            if (cl->ps.pm_type == PM_FREEZE &&
                (g_freezeThawWinningTeam.integer || cl->sess.sessionTeam != level.roundState.prevRoundWinningTeam)) {
                G_TempEntity(ent->r.currentOrigin, EV_THAW_PLAYER);
                cl->ps.pm_type = PM_NORMAL;
                ClientSpawn(ent);
            } else if (cl->ps.pm_type == PM_SPECTATOR || cl->ps.pm_type == PM_DEAD) {
                cl->ps.pm_type = PM_NORMAL;
                ClientSpawn(ent);
            }

            if (g_freezeRemovePowerupsOnRound.integer) {
                int j;
                for (j = PW_QUAD; j <= PW_INVIS; j++) {
                    if (cl->ps.powerups[j] > level.time + 3000) {
                        cl->ps.powerups[j] = level.time + 3000;
                    }
                }
            }

            if (g_freezeResetHealthOnRound.integer && cl->ps.pm_type != PM_FREEZE) {
                G_SetStartingHealth(ent);
            }

            if (g_freezeResetArmorOnRound.integer && cl->ps.pm_type != PM_FREEZE) {
                ent->s.armor = cl->ps.stats[STAT_ARMOR] = g_startingArmor.integer;
                if (armor_tiered.integer && g_startingArmor.integer) {
                    if (g_startingArmor.integer > 150) {
                        cl->ps.stats[STAT_ARMORTYPE] = 2;
                    } else if (g_startingArmor.integer > 100) {
                        cl->ps.stats[STAT_ARMORTYPE] = 1;
                    } else {
                        cl->ps.stats[STAT_ARMORTYPE] = 0;
                    }
                }
            }

            if (g_freezeResetWeaponsOnRound.integer && cl->ps.pm_type != PM_FREEZE) {
                int w;
                for (w = 0; w < MAX_WEAPONS; w++) {
                    cl->ps.stats[STAT_WEAPONS] &= ~(1 << w);
                    cl->ps.ammo[w] = 0;
                }
                G_GiveStartingWeapons(ent);
                SelectSpawnWeapon(ent);
                G_SetInfiniteAmmo(ent);
            }

            if (g_freezeRoundDelay.integer) {
                cl->ps.pm_flags |= PMF_FROZEN;
                if (g_freezeResetWeaponsOnRound.integer && level.ftDropCmdsSuspended) {
                    level.ftDropCmdsSuspended = qfalse;
                    trap_Cvar_Set("g_dropCmds", va("%i", g_dropCmds.integer | 4));
                }
            }
        }

        if (g_freezeRemovePowerupsOnRound.integer) {
            gentity_t *mapEnt;
            for (mapEnt = &g_entities[0]; mapEnt < &g_entities[MAX_GENTITIES]; mapEnt++) {
                if (mapEnt->inuse && mapEnt->item &&
                    mapEnt->item->giType == IT_POWERUP &&
                    (mapEnt->flags & FL_DROPPED_ITEM)) {
                    G_FreeEntity(mapEnt);
                }
            }
        }

        Team_LivingTeamCounts(NULL, NULL);
        level.roundState.round = level.teamScores[TEAM_BLUE] + 1 + level.teamScores[TEAM_RED];

        if (!g_freezeRoundDelay.integer) {
            level.roundState.tNext = level.time + 4000;
            level.roundState.eNext = RS_PLAYING;
            return;
        }
        {
            int countdown = g_freezeRoundDelay.integer;

            if (level.roundState.round == 1) {
                countdown = g_roundWarmupDelay.integer;
            }
            if (countdown) {
                level.roundState.tNext = level.time + countdown;
                level.roundState.eNext = RS_PLAYING;
            } else {
                level.roundState.tNext = 0;
                level.roundState.eCurrent = RS_PLAYING;
                Freeze_RoundStateTransition();
            }
            trap_SetConfigstring(CS_ROUND_STATUS,
                va("\\time\\%d\\round\\%d", level.time + countdown, level.roundState.round));
        }
        return;

    case RS_PLAYING:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED) {
                if (g_freezeRoundDelay.integer) {
                    cl->ps.pm_flags &= ~PMF_FROZEN;
                }
                cl->round_damage = 0;
                cl->round_hits = 0;
                cl->round_shots = 0;
                cl->expandedStats.killStreak = 0;
                if (g_spawnArmor.integer != 0) {
                    cl->ps.powerups[PW_NONE] =
                        (level.time / 1000) * 1000 + g_spawnArmor.integer;
                }
            }
        }
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\round\\%d", level.roundState.round));
        return;

    case RS_ROUND_OVER:
    {
        int winTeam = level.roundState.prevRoundWinningTeam;
        int aliveCounts[4] = {0}, healthTotals[4] = {0};
        int delay;

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED &&
                cl->sess.sessionTeam != TEAM_SPECTATOR && g_freezeRoundDelay.integer) {
                cl->ps.pm_flags |= PMF_FROZEN;
                if (g_freezeResetWeaponsOnRound.integer && (g_dropCmds.integer & 4)) {
                    level.ftDropCmdsSuspended = qtrue;
                    trap_Cvar_Set("g_dropCmds", va("%i", g_dropCmds.integer & ~4));
                }
            }
        }

        if (winTeam == TEAM_RED) {
            level.teamScores[TEAM_RED]++;
        } else if (winTeam == TEAM_BLUE) {
            level.teamScores[TEAM_BLUE]++;
        }

        Team_LivingTeamCounts(&aliveCounts[TEAM_RED], &aliveCounts[TEAM_BLUE]);
        TeamCount_Health_FT(aliveCounts, healthTotals);

        if (g_freezeRoundDelay.integer) {
            const char *teamColor = FT_TeamColor(winTeam);
            const char *teamName = TeamName(winTeam);

            if (aliveCounts[TEAM_RED] == 0 || aliveCounts[TEAM_BLUE] == 0) {
                if (aliveCounts[winTeam] < 2) {
                    trap_SendServerCommand(-1,
                        va("print \"%s%s TEAM^3 WINS the round!^7 (%i hp remaining)\n\"",
                            teamColor, teamName, healthTotals[winTeam]));
                } else {
                    trap_SendServerCommand(-1,
                        va("print \"%s%s TEAM^3 WINS the round!^7 (%i players remaining)\n\"",
                            teamColor, teamName, aliveCounts[winTeam]));
                }
            } else if (!g_roundDrawLivingCount.integer || aliveCounts[TEAM_RED] == aliveCounts[TEAM_BLUE]) {
                if (g_roundDrawHealthCount.integer) {
                    trap_SendServerCommand(-1,
                        va("print \"%s%s TEAM^3 WINS the round!^7 (%s%i^7 hp vs %s%i^7 hp)\n\"",
                            teamColor, teamName, S_COLOR_RED, healthTotals[TEAM_RED],
                            S_COLOR_BLUE, healthTotals[TEAM_BLUE]));
                }
            } else {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i players remaining)\n\"",
                        teamColor, teamName, aliveCounts[winTeam]));
            }
        }

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent2 = &g_entities[i];
            if (cl->pers.connected != CON_CONNECTED) continue;

            if (cl->round_shots != 0) {
                int acc = (cl->round_hits * 100) / cl->round_shots;
                if ((double)acc > 50.0) {
                    PlayerAwardEV(ent2, AWARD_ACCURACY);
                    STAT_AddPlayerMedalStat(ent2, "ACCURACY");
                }
            }

            if (cl->sess.sessionTeam == winTeam && cl->round_damage == 0) {
                PlayerAwardEV(ent2, AWARD_PERFECT);
                STAT_AddPlayerMedalStat(ent2, "PERFECT");
            }
        }

        level.roundState.round++;
        CalculateRanks();

        delay = g_freezeRoundDelay.integer;
        if (delay > 3500) {
            delay = 3500;
        }

        STAT_AddRoundOverStat(level.roundState.round - 1, winTeam, winTeam == TEAM_FREE);

        if (level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE]) {
            if ((g_timelimit.integer != 0 &&
                 g_timelimit.integer * 60000 <= level.time - level.startTime) ||
                (roundlimit.integer != 0 &&
                 (roundlimit.integer <= level.teamScores[TEAM_RED] ||
                  roundlimit.integer <= level.teamScores[TEAM_BLUE]))) {
                level.roundState.tNext = level.time + 1500;
                level.roundState.eNext = RS_EXIT;
                return;
            }
            if (g_mercylimit.integer != 0) {
                int mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
                if (mercyTime <= level.time - level.startTime) {
                    if (g_mercylimit.integer <= level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] ||
                        g_mercylimit.integer <= level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED]) {
                        level.roundState.tNext = level.time + 1500;
                        level.roundState.eNext = RS_EXIT;
                        return;
                    }
                }
            }
        }

        {
            int soundType;
            gentity_t *te;
            if (winTeam == TEAM_RED) soundType = GTS_REDTEAM_SCORED;
            else if (winTeam == TEAM_BLUE) soundType = GTS_BLUETEAM_SCORED;
            else soundType = GTS_DRAW_ROUND;
            te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = soundType;
        }

        if (delay == 0) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_COUNTDOWN;
            Freeze_RoundStateTransition();
        } else {
            level.roundState.tNext = level.time + delay;
            level.roundState.eNext = RS_COUNTDOWN;
        }
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));
        return;
    }

    case RS_EXIT:
        CA_CheckExitRules(1);
        return;

    default:
        G_Error("Freeze_RoundStateTransition: invalid state");
        return;
    }
}

void Freeze_Think(void) {
    int state;
    int aliveCounts[4] = {0}, healthTotals[4] = {0};

    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    if (level.timePauseBegin != 0) {
        level.roundState.startTime += level.frametime;
    }

    state = Freeze_GetRoundState();
    if (state == RS_PLAYING) {
        Team_LivingTeamCounts(&aliveCounts[TEAM_RED], &aliveCounts[TEAM_BLUE]);
        TeamCount_Health_FT(aliveCounts, healthTotals);
        if (Freeze_GameIsOver(aliveCounts, healthTotals)) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_ROUND_OVER;
            Freeze_RoundStateTransition();
        }
    }
}

void Freeze_InstaKill(gentity_t *self, int mode) {
    gclient_t *client = self->client;
    gentity_t *tent;
    int team;

    if (mode == 1) {
        int redCount = 0, blueCount = 0;

        client->ps.powerups[PW_FREEZE] = 0;
        self->s.powerups &= 0xffff7fff;
        client->ps.pm_type = PM_DEAD;
        client->respawnTime = level.time + g_respawn_delay_min.integer;
        self->health = -40;

        Team_LivingTeamCounts(&redCount, &blueCount);
        if ((redCount == 0 && client->sess.sessionTeam == TEAM_RED) ||
            (blueCount == 0 && client->sess.sessionTeam == TEAM_BLUE))
            return;
    }

    client->ps.thawClientNum_valid = 0;

    tent = G_TempEntity(self->r.currentOrigin, EV_OBITUARY);
    tent->s.eventParm = MOD_THAW;
    tent->s.otherEntityNum = client->ps.clientNum;
    tent->s.otherEntityNum2 = ENTITYNUM_WORLD;
    tent->r.svFlags = SVF_BROADCAST;

    team = client->sess.sessionTeam;
    tent = G_TempEntity(self->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
    tent->r.svFlags |= SVF_BROADCAST;
    tent->s.eventParm = (team != TEAM_BLUE) + 2;

    GibEntity(self);
    client->ps.freezetime = 0;
}

void Freeze_AutoThaw(int team) {
    int redAlive, blueAlive;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        Freeze_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return;
    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    Team_LivingTeamCounts(&redAlive, &blueAlive);
    if (redAlive == 0 || blueAlive == 0)
        return;
    if (!g_lastManStandingWarning.integer)
        return;

    if (redAlive == 1 && team == TEAM_RED) {
        LastManStanding(TEAM_RED);
        return;
    }
    if (blueAlive == 1 && team == TEAM_BLUE) {
        LastManStanding(TEAM_BLUE);
    }
}

void Freeze_ClientThawCheck(gentity_t *ent, int msec) {
    gclient_t *cl = ent->client;
    gentity_t *thawer = NULL;
    int oldSeconds = 0;
    int entityList[MAX_GENTITIES];
    int numNearby = 0;
    int i;
    int maxThaw = g_freezeThawTime.integer;

    {
        int t = cl->ps.thawtime;
        int one3 = maxThaw / 3;
        if (t > one3) {
            if (t > one3 * 2) cl->ps.generic1 &= ~3;
            else              cl->ps.generic1 |=  1;
        } else {
            cl->ps.generic1 |= 2;
        }
    }

    if (level.intermissionQueued != 0 || level.intermissionTime != 0)
        return;

    if (level.warmupTime != 0 && g_gametype.integer == GT_FREEZE) {
        cl->ps.thawtime -= msec;
        if (cl->ps.thawtime >= 1) return;
        goto do_thaw;
    }

    if (g_freezeRoundDelay.integer && Freeze_GetRoundState() != RS_PLAYING)
        return;

    if (g_freezeAutoThawTime.integer != 0 &&
        (level.time - cl->ps.freezetime) >= g_freezeAutoThawTime.integer) {
        Freeze_InstaKill(ent, 0);
        return;
    }

    {
        float r = (float)g_freezeThawRadius.integer;
        vec3_t mins, maxs;
        for (i = 0; i < 3; i++) {
            mins[i] = ent->r.currentOrigin[i] - r;
            maxs[i] = ent->r.currentOrigin[i] + r;
        }
        numNearby = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
        for (i = 0; i < numNearby; i++) {
            gentity_t *other = &g_entities[entityList[i]];
            if (entityList[i] != 0 && other->client == NULL) continue;
            if (other == ent)                              continue;
            if (ent->client == NULL)                       continue;
            if (other->client == NULL)                     continue;
            if (g_gametype.integer <= 2 &&
                other->client->sess.sessionTeam != TEAM_SPECTATOR) continue;
            if (other->client->sess.sessionTeam != ent->client->sess.sessionTeam) continue;
            if (other->client->ps.powerups[PW_FREEZE] != 0) continue;
            if (other->health <= 0)                        continue;
            if (g_freezeThawThroughSurface.integer == 0) {
                trace_t tr;
                trap_Trace(&tr, ent->client->ps.origin, NULL, NULL,
                           other->client->ps.origin,
                           ENTITYNUM_NONE, CONTENTS_SOLID);
                if (tr.fraction != 1.0f) continue;
            }
            thawer = other;
            break;
        }
    }

    if (thawer != NULL) {
        if (cl->ps.thawClientNum_valid == 0) {
            cl->ps.thawClientNum       = thawer->client->ps.clientNum;
            cl->ps.thawClientNum_valid = 1;
        }
        if (cl->ps.thawClientNum_valid != 0) {
            int k;
            for (k = 0; k < numNearby; k++) {
                gentity_t *cand = &g_entities[entityList[k]];
                if (cand->client &&
                    cand->client->ps.clientNum == cl->ps.thawClientNum) {
                    thawer = cand;
                    break;
                }
            }
        }
        oldSeconds = (cl->ps.thawtime == 0) ? 0 : cl->ps.thawtime / 1000;
        cl->ps.thawtime -= msec;
        if (g_debugThawTime.integer != 0) {
            trap_SendServerCommand(thawer->client->ps.clientNum,
                va("print \"Thaw time %i.\n\"", cl->ps.thawtime));
        }
        if (oldSeconds > 0 && oldSeconds != cl->ps.thawtime / 1000 &&
            g_freezeThawTick.integer != 0) {
            gentity_t *te = G_TempEntity(ent->r.currentOrigin, EV_THAW_TICK);
            te->r.ownerNum = ent->client->ps.clientNum;
        }
    } else {
        cl->ps.thawtime += msec;
        cl->ps.thawClientNum_valid = 0;
        if (cl->ps.thawtime > maxThaw) cl->ps.thawtime = maxThaw;
    }

    if (cl->ps.thawtime >= 1) return;

    if (thawer != NULL) {
        cl->ps.thawClientNum_valid = 0;
        thawer->client->pers.teamState.assists++;
        thawer->client->expandedStats.numAssists++;
        AddScore(thawer, ent->r.currentOrigin, 2);
        thawer->client->ps.persistant[PERS_ASSIST_COUNT]++;
        thawer->client->ps.eFlags =
            (thawer->client->ps.eFlags & 0xfffc77b7) | EF_AWARD_ASSIST;
        thawer->client->rewardTime = level.time + REWARD_SPRITE_TIME;
        STAT_AddPlayerMedalStat(thawer, "ASSIST");

        {
            gentity_t *tent = G_TempEntity(ent->r.currentOrigin, EV_OBITUARY);
            tent->s.eventParm       = MOD_THAW;
            tent->s.otherEntityNum  = ent->client->ps.clientNum;
            tent->s.otherEntityNum2 = thawer->client->ps.clientNum;
            tent->r.svFlags         = SVF_BROADCAST;
        }
    }

    {
        int team = ent->client->sess.sessionTeam;
        gentity_t *tent = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
        tent->r.svFlags  |= SVF_BROADCAST;
        tent->s.eventParm = (team != TEAM_BLUE) + 2;
    }

do_thaw:
    GibEntity(ent);
}

void Freeze_ClientBegin(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_FREE;
        if (client->ps.pm_type == PM_FREEZE || client->ps.pm_type == PM_DEAD) {
            client->ps.pm_type = PM_NORMAL;
        }
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == 1) {
        if (client->ps.pm_type == PM_FREEZE || client->ps.pm_type == PM_DEAD) {
            client->ps.pm_type = PM_NORMAL;
        }
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        if (g_freezeRoundDelay.integer) {
            client->ps.pm_flags |= PMF_FROZEN;
        }
    } else {
        int alive[4] = {0};
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        Team_LivingTeamCounts(&alive[TEAM_RED], &alive[TEAM_BLUE]);
        if (!g_teamSpecFreeCam.integer
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && alive[TEAM_RED] > 0 && alive[TEAM_BLUE] > 0) {
            FollowCycle(ent, 1);
        }
    }
}

void FreezeTagScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int listEnd[MAX_CLIENTS];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int viewerTeam;
    int redStats[14], blueStats[14];

    string[0] = 0;
    stringlength = 0;

    redStats[0]  = level.numRedArmorPickups[TEAM_RED];
    redStats[1]  = level.numYellowArmorPickups[TEAM_RED];
    redStats[2]  = level.numGreenArmorPickups[TEAM_RED];
    redStats[3]  = level.numMegaHealthPickups[TEAM_RED];
    redStats[4]  = level.numQuadDamagePickups[TEAM_RED];
    redStats[5]  = level.numBattleSuitPickups[TEAM_RED];
    redStats[6]  = level.numRegenerationPickups[TEAM_RED];
    redStats[7]  = level.numHastePickups[TEAM_RED];
    redStats[8]  = level.numInvisibilityPickups[TEAM_RED];
    redStats[9]  = level.quadDamagePossessionTime[TEAM_RED];
    redStats[10] = level.battleSuitPossessionTime[TEAM_RED];
    redStats[11] = level.regenerationPossessionTime[TEAM_RED];
    redStats[12] = level.hastePossessionTime[TEAM_RED];
    redStats[13] = level.invisibilityPossessionTime[TEAM_RED];

    blueStats[0]  = level.numRedArmorPickups[TEAM_BLUE];
    blueStats[1]  = level.numYellowArmorPickups[TEAM_BLUE];
    blueStats[2]  = level.numGreenArmorPickups[TEAM_BLUE];
    blueStats[3]  = level.numMegaHealthPickups[TEAM_BLUE];
    blueStats[4]  = level.numQuadDamagePickups[TEAM_BLUE];
    blueStats[5]  = level.numBattleSuitPickups[TEAM_BLUE];
    blueStats[6]  = level.numRegenerationPickups[TEAM_BLUE];
    blueStats[7]  = level.numHastePickups[TEAM_BLUE];
    blueStats[8]  = level.numInvisibilityPickups[TEAM_BLUE];
    blueStats[9]  = level.quadDamagePossessionTime[TEAM_BLUE];
    blueStats[10] = level.battleSuitPossessionTime[TEAM_BLUE];
    blueStats[11] = level.regenerationPossessionTime[TEAM_BLUE];
    blueStats[12] = level.hastePossessionTime[TEAM_BLUE];
    blueStats[13] = level.invisibilityPossessionTime[TEAM_BLUE];

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, bestWeapon;
        int alive;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        bestWeapon = STAT_GetBestWeapon(cl);

        viewerTeam = ent->client->sess.sessionTeam;
        if (viewerTeam == TEAM_RED && level.intermissionTime == 0) {
            memset(blueStats, 0, sizeof(blueStats));
        } else if (viewerTeam == TEAM_BLUE && level.intermissionTime == 0) {
            memset(redStats, 0, sizeof(redStats));
        }

        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->sess.sessionTeam,
                    cl->ps.persistant[PERS_SCORE], ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    cl->expandedStats.numTeamKills,
                    cl->expandedStats.numTeamKilled,
                    cl->expandedStats.totalDamageDealt,
                    alive);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
        listEnd[i] = stringlength;
    }

    for (;;) {
        const char *cmd = va("scores_ft %i %i %i %i %i %i %i %i %i %i %i %i %i %i "
               "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i%s",
               redStats[0], redStats[1], redStats[2], redStats[3], redStats[4],
               redStats[5], redStats[6], redStats[7], redStats[8],
               redStats[9], redStats[10], redStats[11], redStats[12], redStats[13],
               blueStats[0], blueStats[1], blueStats[2], blueStats[3], blueStats[4],
               blueStats[5], blueStats[6], blueStats[7], blueStats[8],
               blueStats[9], blueStats[10], blueStats[11], blueStats[12], blueStats[13],
               i, level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
               string);

        if (strlen(cmd) <= 1022 || i == 0) {
            trap_SendServerCommand(ent - g_entities, cmd);
            break;
        }
        i--;
        string[i ? listEnd[i - 1] : 0] = '\0';
    }
}

