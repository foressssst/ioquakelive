#include "g_local.h"

void DuelScoreboardMessage_impl(void) {
}

void DuelScoreboardMessage(gentity_t *ent) {
    char weaponString[1024];
    char playerBuf[1024];
    char selfView[1024];
    char oppView[1024];
    char selfView2[1024];
    char oppView2[1024];
    int weaponLen;
    int pass, w;
    int player1, player2;
    gclient_t *cl;
    int numPlayers;
    qboolean showDetail1 = qfalse, showDetail2 = qfalse;

    if (level.numPlayingClients == 0) {
        player1 = -1;
        player2 = -1;
    } else if (level.numPlayingClients == 1) {
        player1 = level.sortedClients[0];
        player2 = -1;
    } else {
        if (level.sortedClients[0] < level.sortedClients[1]) {
            player1 = level.sortedClients[0];
            player2 = level.sortedClients[1];
        } else {
            player1 = level.sortedClients[1];
            player2 = level.sortedClients[0];
        }
        level.clientNum1stPlayer = player1;
        level.clientNum2ndPlayer = player2;
    }

    cl = &level.clients[player1];
    {
        int ping, accuracy, perfect, bestWeapon;
        float redArmorAvg = 0, yellowArmorAvg = 0, greenArmorAvg = 0, megaHealthAvg = 0;
        int redArmorTotal = 0, yellowArmorTotal = 0, greenArmorTotal = 0, megaHealthTotal = 0;

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
        bestWeapon = STAT_GetBestWeapon(cl);

        weaponString[0] = 0;
        weaponLen = 0;
        for (w = 1; w < 15; w++) {
            char wentry[1024];
            int wj, weapAcc = 0;

            if (cl->expandedStats.shotsHit[w] && cl->expandedStats.shotsFired[w]) {
                weapAcc = cl->expandedStats.shotsHit[w] * 100 / cl->expandedStats.shotsFired[w];
            }
            Com_sprintf(wentry, sizeof(wentry), " %i %i %i %i %i",
                        cl->expandedStats.shotsHit[w],
                        cl->expandedStats.shotsFired[w],
                        weapAcc,
                        cl->expandedStats.damageDealt[w],
                        cl->expandedStats.numWeaponKills[w]);
            wj = strlen(wentry);
            if (weaponLen + wj >= (int)sizeof(weaponString))
                break;
            strcpy(weaponString + weaponLen, wentry);
            weaponLen += wj;
        }

        for (pass = 0; pass < 2; pass++) {
            if (pass == 1) {
                redArmorTotal = cl->expandedStats.numRedArmorPickups;
                if (redArmorTotal != cl->expandedStats.numFirstRedArmorPickups &&
                    redArmorTotal - cl->expandedStats.numFirstRedArmorPickups > 0) {
                    int diff = redArmorTotal - cl->expandedStats.numFirstRedArmorPickups;
                    redArmorAvg = ((float)cl->expandedStats.redArmorPickupTime / 1000.0f) / (float)diff;
                }
                yellowArmorTotal = cl->expandedStats.numYellowArmorPickups;
                if (yellowArmorTotal != cl->expandedStats.numFirstYellowArmorPickups &&
                    yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups > 0) {
                    int diff = yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups;
                    yellowArmorAvg = ((float)cl->expandedStats.yellowArmorPickupTime / 1000.0f) / (float)diff;
                }
                greenArmorTotal = cl->expandedStats.numGreenArmorPickups;
                if (greenArmorTotal != cl->expandedStats.numFirstGreenArmorPickups &&
                    greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups > 0) {
                    int diff = greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups;
                    greenArmorAvg = ((float)cl->expandedStats.greenArmorPickupTime / 1000.0f) / (float)diff;
                }
                megaHealthTotal = cl->expandedStats.numMegaHealthPickups;
                if (megaHealthTotal != cl->expandedStats.numFirstMegaHealthPickups &&
                    megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups > 0) {
                    int diff = megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups;
                    megaHealthAvg = ((float)cl->expandedStats.megaHealthPickupTime / 1000.0f) / (float)diff;
                }
            }

            Com_sprintf(playerBuf, sizeof(playerBuf),
                        "%i %i %i %i %i %i %i %i %i %i %i %i %i "
                        "%i %3.2f %i %3.2f %i %3.2f %i %3.2f%s",
                        player1,
                        cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
                        cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                        accuracy, bestWeapon, cl->expandedStats.totalDamageDealt,
                        cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                        cl->ps.persistant[PERS_EXCELLENT_COUNT],
                        cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                        perfect,
                        redArmorTotal, (double)redArmorAvg,
                        yellowArmorTotal, (double)yellowArmorAvg,
                        greenArmorTotal, (double)greenArmorAvg,
                        megaHealthTotal, (double)megaHealthAvg,
                        weaponString);

            if (pass == 0) {
                Q_strncpyz(oppView, playerBuf, sizeof(oppView));
            } else {
                Q_strncpyz(selfView, playerBuf, sizeof(selfView));
            }
        }
    }

    if (ent->client == cl || ent->client->sess.sessionTeam == TEAM_SPECTATOR || level.intermissionTime) {
        showDetail1 = qtrue;
    }

    if (player1 != -1) {
        if (player1 == level.clientNum1stPlayer) {
            Q_strncpyz(level.scoreboardArchive1, selfView, sizeof(level.scoreboardArchive1));
        } else {
            Q_strncpyz(level.scoreboardArchive2, selfView, sizeof(level.scoreboardArchive2));
        }
    }

    if (level.numPlayingClients > 1) {
        cl = &level.clients[player2];
        {
            int ping, accuracy, perfect, bestWeapon;
            float redArmorAvg = 0, yellowArmorAvg = 0, greenArmorAvg = 0, megaHealthAvg = 0;
            int redArmorTotal = 0, yellowArmorTotal = 0, greenArmorTotal = 0, megaHealthTotal = 0;

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
            bestWeapon = STAT_GetBestWeapon(cl);

            weaponString[0] = 0;
            weaponLen = 0;
            for (w = 1; w < 15; w++) {
                char wentry[1024];
                int wj, weapAcc = 0;

                if (cl->expandedStats.shotsHit[w] && cl->expandedStats.shotsFired[w]) {
                    weapAcc = cl->expandedStats.shotsHit[w] * 100 / cl->expandedStats.shotsFired[w];
                }
                Com_sprintf(wentry, sizeof(wentry), " %i %i %i %i %i",
                            cl->expandedStats.shotsHit[w],
                            cl->expandedStats.shotsFired[w],
                            weapAcc,
                            cl->expandedStats.damageDealt[w],
                            cl->expandedStats.numWeaponKills[w]);
                wj = strlen(wentry);
                if (weaponLen + wj >= (int)sizeof(weaponString))
                    break;
                strcpy(weaponString + weaponLen, wentry);
                weaponLen += wj;
            }

            for (pass = 0; pass < 2; pass++) {
                if (pass == 1) {
                    redArmorTotal = cl->expandedStats.numRedArmorPickups;
                    if (redArmorTotal != cl->expandedStats.numFirstRedArmorPickups &&
                        redArmorTotal - cl->expandedStats.numFirstRedArmorPickups > 0) {
                        int diff = redArmorTotal - cl->expandedStats.numFirstRedArmorPickups;
                        redArmorAvg = ((float)cl->expandedStats.redArmorPickupTime / 1000.0f) / (float)diff;
                    }
                    yellowArmorTotal = cl->expandedStats.numYellowArmorPickups;
                    if (yellowArmorTotal != cl->expandedStats.numFirstYellowArmorPickups &&
                        yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups > 0) {
                        int diff = yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups;
                        yellowArmorAvg = ((float)cl->expandedStats.yellowArmorPickupTime / 1000.0f) / (float)diff;
                    }
                    greenArmorTotal = cl->expandedStats.numGreenArmorPickups;
                    if (greenArmorTotal != cl->expandedStats.numFirstGreenArmorPickups &&
                        greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups > 0) {
                        int diff = greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups;
                        greenArmorAvg = ((float)cl->expandedStats.greenArmorPickupTime / 1000.0f) / (float)diff;
                    }
                    megaHealthTotal = cl->expandedStats.numMegaHealthPickups;
                    if (megaHealthTotal != cl->expandedStats.numFirstMegaHealthPickups &&
                        megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups > 0) {
                        int diff = megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups;
                        megaHealthAvg = ((float)cl->expandedStats.megaHealthPickupTime / 1000.0f) / (float)diff;
                    }
                }

                Com_sprintf(playerBuf, sizeof(playerBuf),
                            "%i %i %i %i %i %i %i %i %i %i %i %i %i "
                            "%i %3.2f %i %3.2f %i %3.2f %i %3.2f%s",
                            player2,
                            cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
                            cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                            accuracy, bestWeapon, cl->expandedStats.totalDamageDealt,
                            cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                            cl->ps.persistant[PERS_EXCELLENT_COUNT],
                            cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                            perfect,
                            redArmorTotal, (double)redArmorAvg,
                            yellowArmorTotal, (double)yellowArmorAvg,
                            greenArmorTotal, (double)greenArmorAvg,
                            megaHealthTotal, (double)megaHealthAvg,
                            weaponString);

                if (pass == 0) {
                    Q_strncpyz(oppView2, playerBuf, sizeof(oppView2));
                } else {
                    Q_strncpyz(selfView2, playerBuf, sizeof(selfView2));
                }
            }

            if (ent->client == cl || ent->client->sess.sessionTeam == TEAM_SPECTATOR || level.intermissionTime) {
                showDetail2 = qtrue;
            }

            if (player2 != -1) {
                Q_strncpyz(level.scoreboardArchive2, selfView2, sizeof(level.scoreboardArchive2));
            }
        }
    }

    numPlayers = level.numPlayingClients < 2 ? level.numPlayingClients : 2;

    if (level.intermissionQueued || level.intermissionTime) {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel 2 %s %s", level.scoreboardArchive1, level.scoreboardArchive2));
    } else if (level.numPlayingClients > 1) {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel %i %s %s",
               numPlayers,
               showDetail1 ? selfView : oppView,
               showDetail2 ? selfView2 : oppView2));
    } else if (numPlayers > 0) {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel %i %s", numPlayers, selfView));
    } else {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel %i %s", numPlayers, ""));
    }
}

gclient_t *G_NextDuelPlayer(void) {
    gclient_t *cl;
    gclient_t *nextInLine = NULL;
    int i;

    for (i = 0; i < level.maxclients; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED && !cl->sess.spectatorTime) {
            continue;
        }
        if (cl->sess.sessionTeam != TEAM_SPECTATOR || cl->sess.spectatorClient < 0 || cl->sess.specOnly) {
            continue;
        }
        if (!nextInLine || cl->sess.spectatorTime < nextInLine->sess.spectatorTime) {
            nextInLine = cl;
        }
    }
    return nextInLine;
}

void G_CheckPlayQueueChanged(void) {
    gclient_t* cl;
    qboolean moved = qfalse;
    int i;

    if (g_gametype.integer != GT_DUEL) {
        return;
    }

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = level.clients + level.sortedClients[i];
        if (cl->sess.updatePlayQueue) {
            cl->sess.updatePlayQueue = qfalse;
            moved = qtrue;
        }
    }
    if (!moved) {
        return;
    }

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = level.clients + level.sortedClients[i];
        if (!cl->sess.specOnly && cl->sess.playQueue) {
            ClientUserinfoChanged(cl - level.clients);
        }
    }
}

void AddDuelPlayer(void) {
    int i;
    int numNonSpec;
    gclient_t *cl;
    gclient_t *next;

    if (g_gametype.integer != GT_DUEL) {
        return;
    }
    if (level.intermissionQueued || level.intermissionTime || level.timePauseBegin) {
        return;
    }

    numNonSpec = 0;
    for (i = 0; i < level.maxclients; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_DISCONNECTED &&
            cl->sess.sessionTeam != TEAM_SPECTATOR) {
            numNonSpec++;
        }
    }

    if (numNonSpec >= 2) {
        return;
    }

    next = G_NextDuelPlayer();
    if (!next) {
        return;
    }

    SetWarmupState(-1);
    if (level.allReadyTime != 0) {
        level.allReadyTime = 0;
        trap_SetConfigstring(CS_ALLREADY_TIME, va("%i", 0));
    }
    SetTeam(&g_entities[next - level.clients], "f");

    for (i = 0; i < level.maxclients; i++) {
        cl = level.clients + i;
        if (cl->pers.connected == CON_CONNECTED) {
            if (cl->pers.ready == 1) {
                cl->pers.ready = 0;
            }
            ClientUserinfoChanged(i);
        }
    }
}

void CheckTournament(void) {
    int i;
    gentity_t *ent;
    gclient_t *cl;

    if (g_gametype.integer != GT_DUEL) {
        return;
    }
    if (g_warmupReadyDelay.integer == 0) {
        return;
    }

    if (level.numPlayingClients == 2 && level.numReadyHumans == 1) {
        if (level.allReadyTime != 0) {
            if (level.time <= level.allReadyTime) {
                return;
            }
            for (i = 0; i < level.maxclients; i++) {
                ent = &g_entities[i];
                if (!ent->client) {
                    continue;
                }
                cl = ent->client;
                if (cl->pers.connected != CON_CONNECTED) {
                    continue;
                }
                if (cl->sess.sessionTeam != TEAM_FREE) {
                    continue;
                }
                if (g_warmupReadyDelayAction.integer == 1) {
                    if (cl->pers.ready == 1) {
                        cl->pers.ready = 0;
                        ClientUserinfoChanged(cl->ps.clientNum);
                    } else {
                        SetTeam(ent, "s");
                    }
                } else if (g_warmupReadyDelayAction.integer == 2) {
                    cl->pers.ready = 1;
                }
            }
            level.allReadyTime = 0;
            trap_SetConfigstring(CS_ALLREADY_TIME, va("%i", 0));
            return;
        }
        level.allReadyTime = g_warmupReadyDelay.integer * 1000 + level.time;
    } else {
        if (level.allReadyTime == 0) {
            return;
        }
        level.allReadyTime = 0;
    }
    trap_SetConfigstring(CS_ALLREADY_TIME, va("%i", level.allReadyTime));
}
