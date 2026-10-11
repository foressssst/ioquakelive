#include "g_local.h"

static void TeamCount_Health(int *healthTotals) {
    int i;
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

static const char *CA_TeamColor(int team) {
    if (team == TEAM_RED) return "^1";
    if (team == TEAM_BLUE) return "^4";
    return "^7";
}

int CA_GetRoundState(void) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return -1;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
    }
    return level.roundState.eCurrent;
}

void CA_RoundStateTransition(void) {
    int i;
    int winTeam = 0;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        trap_SetConfigstring(CS_ROUND_STATUS, "\\time\\-1\\round\\0");
        return;

    case RS_COUNTDOWN:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];
            if (cl->pers.connected == CON_CONNECTED &&
                cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_type = PM_NORMAL;
                ClientSpawn(ent);
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }
        Team_LivingTeamCounts(NULL, NULL);

        if (g_roundWarmupDelay.integer == 0) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_PLAYING;
            CA_RoundStateTransition();
        } else {
            level.roundState.tNext = level.time + g_roundWarmupDelay.integer;
            level.roundState.eNext = RS_PLAYING;
        }

        level.roundState.round =
            level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\time\\%d\\round\\%d",
                level.time + g_roundWarmupDelay.integer,
                level.roundState.round));
        return;

    case RS_PLAYING:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED) {
                cl->ps.pm_flags &= ~PMF_FROZEN;
                cl->round_shots = 0;
                cl->round_hits = 0;
                cl->round_damage = 0;
                cl->expandedStats.killStreak = 0;
                if (g_spawnArmor.integer != 0) {
                    cl->ps.powerups[PW_NONE] =
                        (level.time / 1000) * 1000 + g_spawnArmor.integer;
                }
            }
        }

        level.roundState.round =
            level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\round\\%d", level.roundState.round));
        return;

    case RS_ROUND_OVER:
    {
        int aliveCounts[4] = {0};
        int healthTotals[4] = {0};

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED &&
                cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }

        Team_LivingTeamCounts(&aliveCounts[TEAM_RED], &aliveCounts[TEAM_BLUE]);
        TeamCount_Health(healthTotals);

        if (aliveCounts[TEAM_RED] == 0 || aliveCounts[TEAM_BLUE] == 0) {
            if (aliveCounts[TEAM_RED] == 0) {
                if (aliveCounts[TEAM_BLUE] != 0) {
                    winTeam = TEAM_BLUE;
                    level.teamScores[TEAM_BLUE]++;
                }
            } else {
                winTeam = TEAM_RED;
                level.teamScores[TEAM_RED]++;
            }

            level.roundState.round++;
            CalculateRanks();

            if (aliveCounts[winTeam] < 2) {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i hp remaining)\n\"",
                        CA_TeamColor(winTeam), TeamName(winTeam), healthTotals[winTeam]));
            } else {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i players remaining)\n\"",
                        CA_TeamColor(winTeam), TeamName(winTeam), aliveCounts[winTeam]));
            }
        } else if (g_roundDrawLivingCount.integer != 0 &&
                   aliveCounts[TEAM_RED] != aliveCounts[TEAM_BLUE]) {
            if (aliveCounts[TEAM_BLUE] < aliveCounts[TEAM_RED]) {
                winTeam = TEAM_RED;
                level.teamScores[TEAM_RED]++;
            } else {
                winTeam = TEAM_BLUE;
                level.teamScores[TEAM_BLUE]++;
            }

            level.roundState.round++;
            CalculateRanks();

            trap_SendServerCommand(-1,
                va("print \"%s%s TEAM^3 WINS the round!^7 (%i players remaining)\n\"",
                    CA_TeamColor(winTeam), TeamName(winTeam), aliveCounts[winTeam]));
        } else if (g_roundDrawHealthCount.integer != 0 &&
                   healthTotals[TEAM_RED] != healthTotals[TEAM_BLUE]) {
            if (healthTotals[TEAM_BLUE] < healthTotals[TEAM_RED]) {
                winTeam = TEAM_RED;
                level.teamScores[TEAM_RED]++;
            } else {
                winTeam = TEAM_BLUE;
                level.teamScores[TEAM_BLUE]++;
            }

            level.roundState.round++;
            CalculateRanks();

            trap_SendServerCommand(-1,
                va("print \"%s%s TEAM^3 WINS the round!^7 (^1%i^7 hp vs ^4%i^7 hp)\n\"",
                    CA_TeamColor(winTeam), TeamName(winTeam),
                    healthTotals[TEAM_RED], healthTotals[TEAM_BLUE]));
        }

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];

            if (cl->pers.connected != CON_CONNECTED)
                continue;

            if (cl->round_shots != 0) {
                int acc = (cl->round_hits * 100) / cl->round_shots;
                if ((double)acc > 50.0) {
                    PlayerAwardEV(ent, AWARD_ACCURACY);
                    STAT_AddPlayerMedalStat(ent, "ACCURACY");
                }
            }

            if (cl->sess.sessionTeam == winTeam &&
                cl->round_damage == 0) {
                PlayerAwardEV(ent, AWARD_PERFECT);
                STAT_AddPlayerMedalStat(ent, "PERFECT");
            }

            if (cl->sess.sessionTeam == winTeam && !trap_HasAchievement(cl->ps.clientNum, 0x31) &&
                aliveCounts[cl->sess.sessionTeam] == 1 && cl->expandedStats.killStreak > 2) {
                trap_SetAchievement(cl->ps.clientNum, 0x31);
            }
        }

        STAT_AddRoundOverStat(level.roundState.round - 1, winTeam, winTeam == 0);

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
            if (g_gametype.integer == GT_RR) {
                if (winTeam == TEAM_RED || winTeam == TEAM_BLUE)
                    soundType = GTS_ROUND_OVER;
                else
                    soundType = GTS_DRAW_ROUND;
            } else {
                if (winTeam == TEAM_RED)
                    soundType = GTS_RED_WINS_ROUND;
                else if (winTeam == TEAM_BLUE)
                    soundType = GTS_BLUE_WINS_ROUND;
                else
                    soundType = GTS_DRAW_ROUND;
            }
            {
                gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
                te->r.svFlags |= SVF_BROADCAST;
                te->s.eventParm = soundType;
            }
        }

        level.roundState.tNext = level.time + 3500;
        level.roundState.eNext = RS_COUNTDOWN;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));
        return;
    }

    case RS_EXIT:
        CA_CheckExitRules(1);
        return;

    default:
        G_Error("CA_RoundStateTransition: invalid state");
        return;
    }
}

qboolean CA_CheckExitRules(int doExit) {
    int mercyTime;

    if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE])
        return qfalse;

    if (g_timelimit.integer != 0 &&
        g_timelimit.integer * 60000 <= level.time - level.startTime) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
        LogExit(0, 0, "Timelimit hit.");
        return qtrue;
    }

    if (roundlimit.integer != 0) {
        if (roundlimit.integer <= level.teamScores[TEAM_RED]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Red hit the roundlimit.\n\"");
            LogExit(0, 0, "Roundlimit hit.");
            return qtrue;
        }
        if (roundlimit.integer <= level.teamScores[TEAM_BLUE]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Blue hit the roundlimit.\n\"");
            LogExit(0, 0, "Roundlimit hit.");
            return qtrue;
        }
    }

    if (g_mercylimit.integer == 0)
        return qfalse;

    mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
    if (level.time - level.startTime < mercyTime)
        return qfalse;

    if (level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] >= g_mercylimit.integer) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Red hit the mercylimit.\n\"");
        LogExit(0, 0, "Mercylimit hit.");
        return qtrue;
    }

    if (level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED] >= g_mercylimit.integer) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Blue hit the mercylimit.\n\"");
        LogExit(0, 0, "Mercylimit hit.");
        return qtrue;
    }

    return qfalse;
}

qboolean CA_AdjustDamage(gentity_t *target, gentity_t *attacker,
                             int *take, int *asave) {
    int flags = g_dmflags.integer;

    if (target == attacker) {
        if (flags & 4) *take = 0;
        flags &= 8;
    } else {
        gclient_t *tCl = target->client;
        gclient_t *aCl = attacker->client;

        if (tCl == NULL)
            goto checkRound;

        if (aCl != NULL &&
            (g_gametype.integer >= GT_TEAM || tCl->sess.sessionTeam == TEAM_SPECTATOR) &&
            tCl->sess.sessionTeam == aCl->sess.sessionTeam &&
            (flags & 1)) {
            *take = 0;
        }

        if (tCl == NULL || aCl == NULL ||
            (g_gametype.integer < GT_TEAM && tCl->sess.sessionTeam != TEAM_SPECTATOR) ||
            tCl->sess.sessionTeam != aCl->sess.sessionTeam)
            goto checkRound;

        flags &= 2;
    }

    if (flags != 0)
        *asave = 0;

checkRound:
    if (level.warmupTime != 0)
        return qtrue;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return qfalse;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return qfalse;

    if (attacker->client != NULL && !OnSameTeam(target, attacker) &&
        target->client != NULL && target->health > 0) {
        int takeClamp = target->client->ps.stats[STAT_HEALTH];
        if (takeClamp > *take) takeClamp = *take;
        int armorClamp = target->client->ps.stats[STAT_ARMOR];
        if (armorClamp > *asave) armorClamp = *asave;

        *take = takeClamp;
        *asave = armorClamp;

        attacker->client->pers.teamState.dmgAccumulator += armorClamp + takeClamp;
        if (attacker->client->pers.teamState.dmgAccumulator >= 100) {
            attacker->client->pers.teamState.dmgAccumulator -= 100;
            attacker->client->ps.persistant[PERS_SCORE]++;
            CalculateRanks();
        }
    }

    return qtrue;
}

void CA_PlayerKilled(int team) {
    int redAlive, blueAlive;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
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

void ClanArenaStatisticsMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, w;
    gclient_t *cl;

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = &level.clients[level.sortedClients[i]];

        string[0] = 0;
        stringlength = 0;

        Com_sprintf(entry, sizeof(entry), " %i %i",
                    cl->expandedStats.totalDamageDealt,
                    cl->expandedStats.totalDamageTaken);
        stringlength = strlen(entry);
        if (stringlength < (int)sizeof(string)) {
            strcpy(string, entry);
        }

        for (w = 0; w < 15; w++) {
            int j, weapAcc = 0;

            if (cl->expandedStats.shotsHit[w] && cl->expandedStats.shotsFired[w]) {
                weapAcc = cl->expandedStats.shotsHit[w] * 100 / cl->expandedStats.shotsFired[w];
            }

            Com_sprintf(entry, sizeof(entry), " %i %i",
                        weapAcc, cl->expandedStats.numWeaponKills[w]);
            j = strlen(entry);
            if (stringlength + j >= (int)sizeof(string))
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

        trap_SendServerCommand(ent - g_entities,
            va("castats %i%s", i, string));
    }
}

void ClanArenaScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;

    string[0] = 0;
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, perfect, bestWeapon, bestWeaponAccuracy;
        int alive;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        perfect = (cl->ps.persistant[PERS_RANK] == 0 &&
                   cl->ps.persistant[PERS_KILLED] == 0) ? 1 : 0;
        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        bestWeapon = STAT_GetBestWeapon(cl);
        if (cl->expandedStats.shotsFired[bestWeapon] > 0) {
            bestWeaponAccuracy = cl->expandedStats.shotsHit[bestWeapon] * 100
                                / cl->expandedStats.shotsFired[bestWeapon];
        } else {
            bestWeaponAccuracy = 0;
        }

        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->sess.sessionTeam,
                    cl->ps.persistant[PERS_SCORE], ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon, bestWeaponAccuracy,
                    cl->expandedStats.totalDamageDealt,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    perfect, alive);
        j = strlen(entry);
        if (stringlength + j > SCORES_LIST_ROOM)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_ca %i %i %i%s", i,
           level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
           string));
}
