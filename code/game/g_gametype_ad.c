#include "g_local.h"

#define AD_SCORE_HISTORY_SIZE 20

static int ad_scoreRaw[AD_SCORE_HISTORY_SIZE];
static int ad_scoreSorted[AD_SCORE_HISTORY_SIZE];
static int ad_scoreHistoryIndex;

static int ad_prevScore[2];
static int ad_turnDelta[2];

static void AD_UpdateScoreHistory(void) {
    int turn = level.roundState.turn;
    int idx, i;

    ad_turnDelta[turn] = level.teamScores[turn + 1] - ad_prevScore[turn];
    ad_scoreRaw[ad_scoreHistoryIndex % AD_SCORE_HISTORY_SIZE] = ad_turnDelta[turn];
    ad_prevScore[turn] = level.teamScores[turn + 1];

    ad_scoreHistoryIndex++;
    idx = ad_scoreHistoryIndex % AD_SCORE_HISTORY_SIZE;

    if (idx == ad_scoreHistoryIndex) {
        for (i = 0; i < AD_SCORE_HISTORY_SIZE; i++) {
            if (i == idx)
                ad_scoreSorted[i] = -2;
            else if (i > idx)
                ad_scoreSorted[i] = -1;
            else
                ad_scoreSorted[i] = ad_scoreRaw[i];
        }
    } else {
        int offset = idx + (turn != 0 ? 1 : 0) + 1;
        int count = 0;

        if (offset < AD_SCORE_HISTORY_SIZE) {
            count = AD_SCORE_HISTORY_SIZE - offset;
            memcpy(ad_scoreSorted, &ad_scoreRaw[offset], count * sizeof(int));
        }
        if (offset > 0) {
            memcpy(&ad_scoreSorted[count], ad_scoreRaw, (idx + (turn != 0 ? 1 : 0) + 1) * sizeof(int));
        }
        if (turn == 0) {
            ad_scoreSorted[AD_SCORE_HISTORY_SIZE - 1] = -1;
        }
    }

    trap_SendServerCommand(-1,
        va("scores_ad %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
            ad_scoreSorted[0], ad_scoreSorted[1], ad_scoreSorted[2], ad_scoreSorted[3],
            ad_scoreSorted[4], ad_scoreSorted[5], ad_scoreSorted[6], ad_scoreSorted[7],
            ad_scoreSorted[8], ad_scoreSorted[9], ad_scoreSorted[10], ad_scoreSorted[11],
            ad_scoreSorted[12], ad_scoreSorted[13], ad_scoreSorted[14], ad_scoreSorted[15],
            ad_scoreSorted[16], ad_scoreSorted[17], ad_scoreSorted[18], ad_scoreSorted[19],
            level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]));
}

qboolean AD_CheckExitRules(int doExit) {
    int mercyTime;

    if (level.roundState.turn != 1)
        return qfalse;
    if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE])
        return qfalse;

    if (g_timelimit.integer != 0 &&
        g_timelimit.integer * 60000 <= level.time - level.startTime) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
        LogExit(0, 0, "Timelimit hit.");
        return qtrue;
    }

    if (g_scorelimit.integer != 0) {
        if (g_scorelimit.integer <= level.teamScores[TEAM_RED] &&
            level.teamScores[TEAM_BLUE] < level.teamScores[TEAM_RED]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Red hit the scorelimit.\n\"");
            LogExit(0, 0, "Scorelimit hit.");
            return qtrue;
        }
        if (g_scorelimit.integer <= level.teamScores[TEAM_BLUE] &&
            level.teamScores[TEAM_RED] < level.teamScores[TEAM_BLUE]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Blue hit the scorelimit.\n\"");
            LogExit(0, 0, "Scorelimit hit.");
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

int AD_GetRoundState(void) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return -1;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
    }
    return level.roundState.eCurrent;
}

int AD_AttackingTeam(void) {
    if (AD_GetRoundState() != RS_PLAYING)
        return TEAM_FREE;
    return level.roundState.turn ? TEAM_BLUE : TEAM_RED;
}

int AD_DefendingTeam(void) {
    if (AD_GetRoundState() != RS_PLAYING)
        return TEAM_FREE;
    return level.roundState.turn ? TEAM_RED : TEAM_BLUE;
}

qboolean AD_AdjustDamage(gentity_t *target, gentity_t *attacker,
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
        AD_RoundStateTransition();
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
        }
    }

    return qtrue;
}

qboolean AD_CheckRoundEnd(int redAlive, int blueAlive) {
    if (redAlive != 0 && blueAlive != 0 && level.roundState.capture != 1 &&
        (roundtimelimit.integer == 0 ||
         level.time - level.roundState.startTime < roundtimelimit.integer * 1000)) {
        return qfalse;
    }
    return qtrue;
}

void AD_RoundStateTransition(void) {
    int i;
    int winTeam = 0;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            G_Error("AD_RoundStateTransition: invalid state");
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        ADScoreboardMessage(NULL);
        trap_SetConfigstring(CS_ROUND_STATUS,
            "\\time\\-1\\round\\0\\turn\\0\\state\\0");
        return;

    case RS_COUNTDOWN:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_type = PM_FREEZE;
                ClientSpawn(ent);
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }
        Team_LivingTeamCounts(NULL, NULL);

        if (g_roundWarmupDelay.integer == 0) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_PLAYING;
            AD_RoundStateTransition();
        } else {
            level.roundState.tNext = level.time + g_roundWarmupDelay.integer;
            level.roundState.eNext = RS_PLAYING;
        }

        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\time\\%d\\round\\%d\\turn\\%d\\state\\1",
                level.time + g_roundWarmupDelay.integer, level.roundState.round, level.roundState.turn));
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
                if (cl->sess.sessionTeam != TEAM_SPECTATOR) {
                    cl->ps.pm_type = PM_NORMAL;
                }
                if (g_spawnArmor.integer != 0) {
                    cl->ps.powerups[0] =
                        (level.time / 1000) * 1000 + g_spawnArmor.integer;
                }
            }
        }

        level.attackingTeam = (AD_GetRoundState() == RS_PLAYING) ? (level.roundState.turn ? TEAM_BLUE : TEAM_RED)
                                                                : TEAM_FREE;

        level.roundState.capture = 0;
        level.roundState.touch = 0;

        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\turn\\%d\\state\\2", level.roundState.turn));
        return;

    case RS_ROUND_OVER:
    {
        int redAlive, blueAlive;

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
                cl->ps.powerups[PW_REDFLAG] = 0;
                cl->ps.powerups[PW_BLUEFLAG] = 0;
            }
        }

        Team_LivingTeamCounts(&redAlive, &blueAlive);

        if (level.roundState.capture == 1) {
            winTeam = (level.roundState.turn == 0) ? TEAM_RED : TEAM_BLUE;
        } else if (redAlive == 0 && blueAlive != 0) {
            if (level.roundState.turn == 1) {
                level.teamScores[TEAM_BLUE] += g_adElimScoreBonus.integer;
            }
            winTeam = TEAM_BLUE;
        } else if (blueAlive == 0 && redAlive != 0) {
            if (level.roundState.turn == 0) {
                level.teamScores[TEAM_RED] += g_adElimScoreBonus.integer;
            }
            winTeam = TEAM_RED;
        }

        if (level.roundState.capture != 1 && !level.roundState.touch && winTeam == level.roundState.turn + 1) {
            for (i = 0; i < level.maxclients; i++) {
                gclient_t *cl = &level.clients[i];

                if (cl->pers.connected == CON_CONNECTED && !trap_HasAchievement(i, 0x1f) &&
                    cl->sess.sessionTeam == winTeam) {
                    trap_SetAchievement(i, 0x1f);
                }
            }
        }

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];
            if (cl->pers.connected != CON_CONNECTED) continue;

            if (cl->round_shots != 0) {
                int acc = (cl->round_hits * 100) / cl->round_shots;
                if ((double)acc > 50.0) {
                    PlayerAwardEV(ent, AWARD_ACCURACY);
                    STAT_AddPlayerMedalStat(ent, "ACCURACY");
                }
            }

            if (cl->round_damage == 0) {
                PlayerAwardEV(ent, AWARD_PERFECT);
                STAT_AddPlayerMedalStat(ent, "PERFECT");
            }
        }

        if (level.roundState.capture == 1 || redAlive == 0 || blueAlive == 0) {
            CalculateRanks();
        }
        AD_UpdateScoreHistory();

        Team_ReturnFlag(TEAM_RED);
        Team_ReturnFlag(TEAM_BLUE);

        STAT_AddRoundOverStat(level.roundState.round - 1, winTeam, winTeam == 0);

        if (level.roundState.turn == 1 && level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE]) {
            if ((g_timelimit.integer != 0 &&
                 g_timelimit.integer * 60000 <= level.time - level.startTime) ||
                (g_scorelimit.integer != 0 &&
                 ((g_scorelimit.integer <= level.teamScores[TEAM_RED] &&
                   level.teamScores[TEAM_BLUE] < level.teamScores[TEAM_RED]) ||
                  (g_scorelimit.integer <= level.teamScores[TEAM_BLUE] &&
                   level.teamScores[TEAM_RED] < level.teamScores[TEAM_BLUE])))) {
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
            if (winTeam == TEAM_RED) soundType = GTS_RED_WINS_ROUND;
            else if (winTeam == TEAM_BLUE) soundType = GTS_BLUE_WINS_ROUND;
            else soundType = GTS_DRAW_ROUND;
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = soundType;
        }

        level.roundState.tNext = level.time + 3500;
        level.roundState.eNext = RS_COUNTDOWN;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));

        level.roundState.turn ^= 1;
        if (level.roundState.turn == 0) {
            level.roundState.round++;
        }
        return;
    }

    case RS_EXIT:
        AD_CheckExitRules(1);
        return;

    default:
        G_Error("AD_RoundStateTransition: invalid state");
        return;
    }
}

void AD_PlayerKilled(int team) {
    int redAlive, blueAlive;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
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

int AD_IsInPlayState(void) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return 0;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
    }
    if (level.roundState.eCurrent != RS_PLAYING)
        return 0;
    return (level.roundState.turn != 0) ? 2 : 1;
}

int AD_CanScore(void) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return 0;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
    }
    if (level.roundState.eCurrent != RS_PLAYING)
        return 0;
    return 2 - (level.roundState.turn != 0 ? 1 : 0);
}

void ADScoreboardMessage(gentity_t *ent) {
    int i;

    (void)ent;

    for (i = 0; i < AD_SCORE_HISTORY_SIZE; i++) {
        ad_scoreRaw[i] = -1;
        ad_scoreSorted[i] = -1;
    }
    ad_scoreHistoryIndex = 0;

    trap_SendServerCommand(-1,
        va("scores_ad %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
           level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]));
}
