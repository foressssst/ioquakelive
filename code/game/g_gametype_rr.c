#include "g_local.h"

int rr_lastRedClient = -1;
int rr_lastBlueClient = -1;
static int rr_survivalNextTime = 0;
static int rr_infectionStartTime = -1;

void RR_SpreadInfection(void) {
    int i;
    int counts[TEAM_NUM_TEAMS];

    if (g_rrInfected.integer == 0)
        return;

    if (g_rrInfectedSpreadTime.integer == 0 || rr_infectionStartTime == -1)
        return;

    Team_ConnectedTeamCounts(counts);
    if (counts[TEAM_BLUE] <= 1)
        return;

    if (g_rrInfectedSpreadWarningTime.integer != 0) {
        int elapsed = level.time - rr_infectionStartTime;
        int warningTime = (g_rrInfectedSpreadTime.integer - g_rrInfectedSpreadWarningTime.integer) * 1000;

        if (elapsed > warningTime && elapsed - warningTime <= level.frametime) {
            trap_SendServerCommand(-1,
                va("cp \" ^3Infection spreads in %i second%s!\"",
                    g_rrInfectedSpreadWarningTime.integer,
                    (g_rrInfectedSpreadWarningTime.integer != 1) ? "s" : ""));
        }
    }

    if (level.time - rr_infectionStartTime > g_rrInfectedSpreadTime.integer * 1000) {
        for (i = level.numPlayingClients - 1; i >= 0; i--) {
            gclient_t *cl = &level.clients[level.sortedClients[i]];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_BLUE) {
                gentity_t *ent = &g_entities[level.sortedClients[i]];
                level.scoringDisabled = qtrue;
                SetTeam_Execute(ent, TEAM_RED, SPECTATOR_NOT, 0);
                level.scoringDisabled = qfalse;
                rr_infectionStartTime = level.time;
                return;
            }
        }
    }
}

void RR_SurvivorBonuses(int mode) {
    int i;

    if (g_rrInfected.integer == 0 || g_rrInfectedSurvivorScoreMethod.integer == 0)
        return;

    if (rr_survivalNextTime == 0) {
        rr_survivalNextTime = level.time + g_rrInfectedSurvivorScoreRate.integer * 1000;
    }

    if (g_rrInfectedSurvivorScoreMethod.integer == 1) {
        if (level.time <= rr_survivalNextTime) {
            CalculateRanks();
            return;
        }
    } else if (g_rrInfectedSurvivorScoreMethod.integer == 2) {
        if (mode != 1) {
            CalculateRanks();
            return;
        }
    } else {
        CalculateRanks();
        return;
    }

    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_BLUE) {
            cl->ps.persistant[PERS_SCORE] += g_rrInfectedSurvivorScoreBonus.integer;
            trap_SendServerCommand(cl - level.clients,
                va("print \"Survival Bonus! +%i\n\"", g_rrInfectedSurvivorScoreBonus.integer));
        }
    }

    {
        gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
        te->r.svFlags |= SVF_BROADCAST;
        te->s.eventParm = GTS_SURVIVOR;
        te->s.modelindex2 = TEAM_BLUE;
    }

    if (g_rrInfectedSurvivorScoreMethod.integer == 1) {
        rr_survivalNextTime = level.time + g_rrInfectedSurvivorScoreRate.integer * 1000;
    }

    CalculateRanks();
}

team_t PickTeam_RoundAware(int clientNum) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            goto pickBlue;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }

    if (level.roundState.eCurrent == RS_PLAYING)
        return TEAM_RED;

pickBlue:
    if (clientNum != rr_lastRedClient && clientNum != rr_lastBlueClient)
        return TEAM_BLUE;

    return TEAM_RED;
}

void RR_InitRoundState(void) {
    if (g_rrInfected.integer != 0) {
        rr_lastRedClient = -1;
        rr_lastBlueClient = -1;
        rr_infectionStartTime = -1;
        level.roundState.tNext = level.time + 500;
        level.roundState.eNext = (level.warmupTime != 0) ? RS_WARMUP : RS_SHUFFLE;
        level.roundState.round = 0;
        return;
    }

    level.roundState.tNext = 0;
    level.roundState.eCurrent = (level.warmupTime == 0) ? RS_COUNTDOWN : RS_WARMUP;
    RR_RoundStateTransition();
    level.roundState.round = 0;
}

static void StartNewRound(void) {
    int i;
    int counts[TEAM_NUM_TEAMS];

    level.scoringDisabled = qtrue;

    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_RED) {
            SetTeam_Execute(&g_entities[i], TEAM_BLUE, SPECTATOR_NOT, 0);
        }
    }

    if (rr_lastBlueClient != -1) {
        gclient_t *cl = &level.clients[rr_lastBlueClient];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->sess.sessionTeam == TEAM_BLUE) {
            SetTeam_Execute(&g_entities[rr_lastBlueClient], TEAM_RED, SPECTATOR_NOT, 0);
        }
    }

    Team_ConnectedTeamCounts(counts);
    if (counts[TEAM_RED] == 0 && level.numPlayingClients > 0) {
        int idx = level.sortedClients[rand() % level.numPlayingClients];
        gclient_t *cl = &level.clients[idx];
        if (cl->pers.connected == CON_CONNECTED) {
            SetTeam_Execute(&g_entities[idx], TEAM_RED, SPECTATOR_NOT, 0);
            rr_lastRedClient = cl->ps.clientNum;
        }
    }

    level.scoringDisabled = qfalse;
}

static int QDECL RR_SortRoundScores(const void *a, const void *b) {
    gclient_t *ca = &level.clients[*(const int *)a];
    gclient_t *cb = &level.clients[*(const int *)b];

    if (ca->sess.spectatorClient < 0) return 1;
    if (cb->sess.spectatorClient < 0) return -1;

    if (ca->pers.connected == CON_CONNECTING) return 1;
    if (cb->pers.connected == CON_CONNECTING) return -1;

    if (ca->sess.sessionTeam == TEAM_SPECTATOR) {
        if (cb->sess.sessionTeam != TEAM_SPECTATOR) return 1;
        if (ca->sess.spectatorTime < cb->sess.spectatorTime) return -1;
        return ca->sess.spectatorTime > cb->sess.spectatorTime;
    }
    if (cb->sess.sessionTeam == TEAM_SPECTATOR) return -1;

    if (ca->ps.localPersistant[0] > cb->ps.localPersistant[0]) return -1;
    return ca->ps.localPersistant[0] < cb->ps.localPersistant[0];
}

static int RR_RoundScoreList(int *list) {
    int i, count = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected != CON_DISCONNECTED) {
            list[count++] = i;
        }
    }
    qsort(list, count, sizeof(list[0]), RR_SortRoundScores);
    return count;
}

qboolean RR_CheckExitRules(int doExit) {
    if (ScoreIsTied())
        return qfalse;

    if (g_timelimit.integer != 0 &&
        g_timelimit.integer * 60000 <= level.time - level.startTime) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
        LogExit(0, 0, "Timelimit hit.");
        return qtrue;
    }

    if (roundlimit.integer != 0 &&
        level.teamScores[TEAM_RED] + level.teamScores[TEAM_BLUE] >= roundlimit.integer) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Game hit the roundlimit.\n\"");
        LogExit(0, 0, "Roundlimit hit.");
        return qtrue;
    }

    return qfalse;
}

void RR_RoundStateTransition(void) {
    int i;
    int winTeam = 0;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        trap_SetConfigstring(CS_ROUND_STATUS, "\\time\\-1\\round\\0");
        return;

    case RS_COUNTDOWN:
        Team_LivingTeamCounts(NULL, NULL);
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_type = PM_NORMAL;
                ClientSpawn(ent);
                cl->ps.localPersistant[0] = 0;
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }
        Team_LivingTeamCounts(NULL, NULL);

        if (g_roundWarmupDelay.integer == 0) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_PLAYING;
            RR_RoundStateTransition();
        } else {
            level.roundState.tNext = level.time + g_roundWarmupDelay.integer;
            level.roundState.eNext = RS_PLAYING;
        }

        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\time\\%d\\round\\%d",
                level.time + g_roundWarmupDelay.integer, level.roundState.round));
        return;

    case RS_SHUFFLE:
        if (g_rrInfected.integer == 0) {
            Svcmd_ForceShuffle_f();
            level.roundState.tNext = level.time + 1;
            level.roundState.eNext = RS_COUNTDOWN;
            return;
        }
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
        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        rr_survivalNextTime = 0;
        rr_lastBlueClient = -1;
        rr_infectionStartTime = level.time;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\round\\%d", level.roundState.round));
        return;

    case RS_ROUND_OVER:
    {
        int redAlive, blueAlive;
        int j;
        int survivor[MAX_CLIENTS];
        int sorted[MAX_CLIENTS];
        int numSorted;
        char survivorList[1024];

        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
                cl->ps.persistant[PERS_SCORE] += g_rrRoundScoreBonus.integer;
                cl->ps.localPersistant[0] += g_rrRoundScoreBonus.integer;
            }
        }

        Team_LivingTeamCounts(&redAlive, &blueAlive);

        if (redAlive == 0) {
            if (blueAlive != 0) {
                winTeam = TEAM_BLUE;
                level.teamScores[TEAM_BLUE]++;
            }
        } else if (blueAlive == 0) {
            winTeam = TEAM_RED;
            level.teamScores[TEAM_RED]++;
        } else {
            if (level.teamScores[TEAM_BLUE] < level.teamScores[TEAM_RED] ||
                g_rrInfected.integer != 0) {
                level.teamScores[TEAM_BLUE]++;
            } else {
                level.teamScores[TEAM_RED]++;
            }
        }

        CalculateRanks();

        numSorted = RR_RoundScoreList(sorted);
        memset(survivor, 0, sizeof(survivor));
        survivorList[0] = '\0';
        if (g_rrInfected.integer == 0) {
            if (winTeam != 0) {
                for (j = 0; j < numSorted; j++) {
                    survivor[sorted[j]] = 1;
                    Q_strcat(survivorList, sizeof(survivorList), va("%d ", sorted[j]));
                    if (j + 1 < numSorted &&
                        level.clients[sorted[j]].ps.localPersistant[0] !=
                        level.clients[sorted[j + 1]].ps.localPersistant[0]) {
                        break;
                    }
                }
            }
        } else if (rr_lastBlueClient == -1) {
            for (j = 0; j < numSorted; j++) {
                if (level.clients[sorted[j]].sess.sessionTeam == TEAM_BLUE) {
                    survivor[sorted[j]] = 1;
                    Q_strcat(survivorList, sizeof(survivorList), va("%d ", sorted[j]));
                }
            }
        } else {
            survivor[rr_lastBlueClient] = 1;
            Q_strcat(survivorList, sizeof(survivorList), va("%d ", rr_lastBlueClient));
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

            if (survivor[i] == 1 && cl->round_damage == 0) {
                PlayerAwardEV(ent, AWARD_PERFECT);
                STAT_AddPlayerMedalStat(ent, "PERFECT");
            }
        }

        trap_SetConfigstring(CS_ROUND_WINNER, survivorList);

        STAT_AddRoundOverStat(level.roundState.round - 1, winTeam, winTeam == 0);

        {
            int soundType = (winTeam != 0) ? GTS_ROUND_OVER
                                           : GTS_DRAW_ROUND;
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = soundType;
        }

        if (!(level.numPlayingClients > 1 && ScoreIsTied())) {
            if ((g_timelimit.integer != 0 &&
                 g_timelimit.integer * 60000 <= level.time - level.startTime) ||
                (roundlimit.integer != 0 &&
                 roundlimit.integer <= level.teamScores[TEAM_RED] + level.teamScores[TEAM_BLUE])) {
                level.roundState.tNext = level.time + 1500;
                level.roundState.eNext = RS_EXIT;
                return;
            }
        }

        level.roundState.tNext = level.time + 3500;
        level.roundState.eNext = RS_SHUFFLE;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));
        return;
    }

    case RS_EXIT:
        RR_CheckExitRules(1);
        Svcmd_ForceShuffle_f();
        return;

    default:
        G_Error("RR_RoundStateTransition: invalid state");
        return;
    }
}

qboolean RR_AdjustDamage(gentity_t *target, gentity_t *attacker,
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
        RR_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return qfalse;

    if (attacker->client != NULL && !OnSameTeam(target, attacker) &&
        target->client != NULL && target->health > 0 &&
        g_rrDamageScoreBonus.integer != 0) {
        int takeClamp = target->client->ps.stats[STAT_HEALTH];
        if (takeClamp > *take) takeClamp = *take;
        int armorClamp = target->client->ps.stats[STAT_ARMOR];
        if (armorClamp > *asave) armorClamp = *asave;

        *take = takeClamp;
        *asave = armorClamp;

        attacker->client->pers.teamState.dmgAccumulator += armorClamp + takeClamp;
        if (attacker->client->pers.teamState.dmgAccumulator >= 100) {
            attacker->client->pers.teamState.dmgAccumulator -= 100;
            attacker->client->ps.persistant[PERS_SCORE] += g_rrDamageScoreBonus.integer;
            attacker->client->ps.localPersistant[0] += g_rrDamageScoreBonus.integer;
            CalculateRanks();
        }
    }

    return qtrue;
}

void RR_Think(void) {
    int counts[TEAM_NUM_TEAMS];
    int state;

    if (level.intermissionQueued || level.intermissionTime || level.warmupTime > 0)
        return;

    if (level.timePauseBegin != 0) {
        level.roundState.startTime += level.frametime;
    }

    state = RR_GetRoundState();
    if (state == -1) {
        return;
    }

    if (state == RS_SHUFFLE) {
        if (g_rrInfected.integer != 0) {
            StartNewRound();
            level.roundState.tNext = level.time + 1;
            level.roundState.eNext = RS_COUNTDOWN;
        }
        return;
    }

    if (state != RS_PLAYING)
        return;

    Team_ConnectedTeamCounts(counts);

    RR_SurvivorBonuses(0);
    RR_SpreadInfection();

    if (!CheckRoundTimeout(counts[TEAM_RED], counts[TEAM_BLUE])) {
        return;
    }

    level.roundState.tNext = 0;
    level.roundState.eCurrent = RS_ROUND_OVER;
    RR_RoundStateTransition();
}

void RR_OnPlayerDeath(gentity_t *victim) {
    int redAlive, blueAlive;
    int killedTeam;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return;
    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    if (!victim->client)
        return;

    killedTeam = victim->client->sess.sessionTeam;

    if (g_rrInfected.integer == 0 || killedTeam == TEAM_BLUE) {
        victim->client->sess.sessionTeam = OtherTeam(killedTeam);
        ClientUserinfoChanged(victim->client->ps.clientNum);

        if (g_rrInfected.integer != 0) {
            gentity_t *te = G_TempEntity(victim->client->ps.origin, EV_INFECTED);
            te->r.svFlags |= SVF_SINGLECLIENT;
            te->r.singleClient = victim->client->ps.clientNum;
            te->r.ownerNum = victim->client->ps.clientNum;
        }
    }

    Team_LivingTeamCounts(&redAlive, &blueAlive);

    if (g_rrInfected.integer != 0 && killedTeam == TEAM_BLUE) {
        rr_infectionStartTime = level.time;
        RR_SurvivorBonuses(1);
        if (blueAlive == 0) {
            rr_lastBlueClient = victim->client->ps.clientNum;
        }
    }

    if (redAlive != 0 && blueAlive != 0 && g_lastManStandingWarning.integer != 0) {
        if (redAlive == 1 && killedTeam == TEAM_RED && g_rrInfected.integer == 0) {
            LastManStanding(TEAM_RED);
        } else if (blueAlive == 1 && killedTeam == TEAM_BLUE) {
            LastManStanding(TEAM_BLUE);
        }
    }
}

void ClientSpawn_RedRover(gentity_t *ent) {
    if (g_rrInfected.integer != 0) {
        gclient_t *cl = ent->client;
        if (cl->sess.sessionTeam == TEAM_RED) {
            cl->ps.pm_flags |= PMF_LOADOUT_FORCED;
            cl->ps.pm_flags |= PMF_DOUBLE_JUMPED;
            cl->ps.pm_flags |= PMF_PROMODE;
        } else {
            cl->ps.pm_flags &= ~PMF_LOADOUT_FORCED;
            if (!pmove_DoubleJump.integer) {
                cl->ps.pm_flags &= ~PMF_DOUBLE_JUMPED;
            }
            if (!pmove_AirControl.integer) {
                cl->ps.pm_flags &= ~PMF_PROMODE;
            }
        }
    }

    if (level.warmupTime > 0 || level.roundState.eCurrent == RS_COUNTDOWN) {
        ent->client->ps.pm_flags |= PMF_FROZEN;
    }
}

void RR_ClientBegin(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent != RS_COUNTDOWN) {
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        return;
    }
    ClientSpawn(ent);
    SelectSpawnWeapon(ent);
    client->ps.pm_flags |= PMF_FROZEN;
}

void RedRoverScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;

    memset(string, 0, sizeof(string));
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, perfect, bestWeapon, bestWeaponAccuracy;
        int alive;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        perfect = (cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0) ? 1 : 0;
        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;

        bestWeapon = STAT_GetBestWeapon(cl);
        if (cl->expandedStats.shotsFired[bestWeapon] > 0) {
            bestWeaponAccuracy = cl->expandedStats.shotsHit[bestWeapon] * 100
                                / cl->expandedStats.shotsFired[bestWeapon];
        } else {
            bestWeaponAccuracy = 0;
        }

        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->ps.persistant[PERS_SCORE],
                    cl->ps.localPersistant[0],
                    ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon, bestWeaponAccuracy,
                    cl->expandedStats.totalDamageDealt,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->ps.persistant[PERS_DEFEND_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    perfect,
                    cl->ps.persistant[PERS_CAPTURES],
                    alive);
        j = strlen(entry);
        if (stringlength + j > SCORES_LIST_ROOM)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_rr %i %i %i%s", i,
           level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
           string));
}
