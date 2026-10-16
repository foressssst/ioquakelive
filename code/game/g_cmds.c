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

#include "../../ui/menudef.h"

void Cmd_Score_f(gentity_t* ent) {
    if (!ent || !ent->client) {
        return;
    }

    switch (g_gametype.integer) {
        case GT_FFA:
            FreeForAllScoreboardMessage(ent);
            break;
        case GT_DUEL:
            DuelScoreboardMessage(ent);
            break;
        case GT_RACE:
            RaceScoreboardMessage(ent);
            break;
        case GT_TEAM:
            TeamDeathmatchScoreboardMessage(ent);
            break;
        case GT_CA:
            ClanArenaScoreboardMessage(ent);
            break;
        case GT_CTF:
        case GT_1FCTF:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            CaptureTheFlagScoreboardMessage(ent);
            break;
        case GT_FREEZE:
            FreezeTagScoreboardMessage(ent);
            break;
        case GT_RR:
            RedRoverScoreboardMessage(ent);
            break;
        default:
            break;
    }

    if (level.intermissionTime) {
        switch (g_gametype.integer) {
            case GT_TEAM:
            case GT_FREEZE:
                TeamDeathmatchStatisticsMessage(ent);
                break;
            case GT_CA:
                ClanArenaStatisticsMessage(ent);
                break;
            case GT_CTF:
            case GT_1FCTF:
            case GT_HARVESTER:
            case GT_DOMINATION:
            case GT_AD:
                CaptureTheFlagStatisticsMessage(ent);
                break;
            default:
                break;
        }
    }
}

qboolean CheatsOk(gentity_t* ent) {
    if (!g_cheats.integer) {
        trap_SendServerCommand(ent - g_entities, "print \"Cheats are not enabled on this server.\n\"");
        return qfalse;
    }
    if (ent->health <= 0) {
        trap_SendServerCommand(ent - g_entities, "print \"You must be alive to use this command.\n\"");
        return qfalse;
    }
    return qtrue;
}

char* ConcatArgs(int start) {
    int i, c, tlen;
    static char line[MAX_STRING_CHARS];
    int len;
    char arg[MAX_STRING_CHARS];

    len = 0;
    c = trap_Argc();
    for (i = start; i < c; i++) {
        trap_Argv(i, arg, sizeof(arg));
        tlen = strlen(arg);
        if (len + tlen >= MAX_STRING_CHARS - 1) {
            break;
        }
        memcpy(line + len, arg, tlen);
        len += tlen;
        if (i != c - 1) {
            line[len] = ' ';
            len++;
        }
    }

    line[len] = 0;

    return line;
}

qboolean StringIsInteger(const char* s) {
    int i;
    int len;
    qboolean foundDigit;

    len = strlen(s);
    foundDigit = qfalse;

    for (i = 0; i < len; i++) {
        if (!isdigit(s[i])) {
            return qfalse;
        }

        foundDigit = qtrue;
    }

    return foundDigit;
}

int ClientNumberFromString(gentity_t* to, char* s, qboolean checkNums, qboolean checkNames) {
    gclient_t* cl;
    int idnum;
    char cleanName[MAX_STRING_CHARS];

    if (checkNums && s[0] >= '0' && s[0] <= '9') {
        idnum = atoi(s);
        if (idnum < 0 || idnum >= level.maxclients) {
            trap_SendServerCommand(to - g_entities, va("print \"Bad client slot: %i\n\"", idnum));
            return -1;
        }
        if (level.clients[idnum].pers.connected != CON_CONNECTED) {
            trap_SendServerCommand(to - g_entities, va("print \"Client %i is not active\n\"", idnum));
            return -1;
        }
        return idnum;
    }

    if (checkNames) {
        for (idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++) {
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            Q_strncpyz(cleanName, cl->pers.netname, sizeof(cleanName));
            Q_CleanStr(cleanName);
            if (!Q_stricmp(cleanName, s)) {
                return idnum;
            }
        }
    }

    trap_SendServerCommand(to - g_entities, va("print \"User %s is not on the server\n\"", s));
    return -1;
}

void Cmd_Give_f(gentity_t* ent) {
    char* name;
    gitem_t* it;
    int i;
    qboolean give_all;
    gentity_t* it_ent;
    trace_t trace;

    if (!CheatsOk(ent)) {
        return;
    }

    name = ConcatArgs(1);

    if (Q_stricmp(name, "all") == 0)
        give_all = qtrue;
    else
        give_all = qfalse;

    if (give_all || Q_stricmp(name, "health") == 0) {
        ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "weapons") == 0) {
        ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_NUM_WEAPONS) - 1 -
                                              (1 << WP_GRAPPLING_HOOK) - (1 << WP_NONE);
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "ammo") == 0) {
        for (i = 0; i < MAX_WEAPONS; i++) {
            ent->client->ps.ammo[i] = 999;
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "armor") == 0) {
        ent->s.armor = ent->client->ps.stats[STAT_ARMOR] = 200;

        if (!give_all)
            return;
    }

    if (Q_stricmp(name, "excellent") == 0) {
        ent->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "impressive") == 0) {
        ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "gauntletaward") == 0) {
        ent->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "defend") == 0) {
        ent->client->ps.persistant[PERS_DEFEND_COUNT]++;
        return;
    }
    if (Q_stricmp(name, "assist") == 0) {
        ent->client->ps.persistant[PERS_ASSIST_COUNT]++;
        return;
    }

    if (!give_all) {
        it = BG_FindItem(name);
        if (!it) {
            return;
        }

        it_ent = G_Spawn();
        VectorCopy(ent->r.currentOrigin, it_ent->s.origin);
        it_ent->classname = it->classname;
        G_SpawnItem(it_ent, it, qtrue);
        FinishSpawningItem(it_ent);
        memset(&trace, 0, sizeof(trace));
        Touch_Item(it_ent, ent, &trace);
        if (it_ent->inuse) {
            G_FreeEntity(it_ent);
        }
    }
}

void Cmd_God_f(gentity_t* ent) {
    char* msg;

    if (!CheatsOk(ent)) {
        return;
    }

    ent->flags ^= FL_GODMODE;
    if (!(ent->flags & FL_GODMODE))
        msg = "godmode " S_COLOR_RED "OFF" S_COLOR_WHITE "\n";
    else
        msg = "godmode " S_COLOR_GREEN "ON" S_COLOR_WHITE "\n";

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

void Cmd_Notarget_f(gentity_t* ent) {
    char* msg;

    if (!CheatsOk(ent)) {
        return;
    }

    ent->flags ^= FL_NOTARGET;
    if (!(ent->flags & FL_NOTARGET))
        msg = "notarget " S_COLOR_RED "OFF" S_COLOR_WHITE "\n";
    else
        msg = "notarget " S_COLOR_GREEN "ON" S_COLOR_WHITE "\n";

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

void Cmd_Noclip_f(gentity_t* ent) {
    char* msg;

    if (!CheatsOk(ent)) {
        return;
    }

    if (ent->client->noclip) {
        msg = "noclip " S_COLOR_RED "OFF" S_COLOR_WHITE "\n";
    } else {
        msg = "noclip " S_COLOR_GREEN "ON" S_COLOR_WHITE "\n";
    }
    ent->client->noclip = !ent->client->noclip;

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

void Cmd_LevelShot_f(gentity_t* ent) {
    if (!CheatsOk(ent)) {
        return;
    }
    if (ent->health <= 0) {
        trap_SendServerCommand(ent - g_entities,
                               "print \"You must be alive to use this command.\n\"");
        return;
    }
    if (g_gametype.integer != GT_FFA) {
        trap_SendServerCommand(ent - g_entities,
                               "print \"Must be in g_gametype 0 for levelshot\n\"");
        return;
    }
    if (!ent->client->pers.localClient) {
        trap_SendServerCommand(ent - g_entities,
                               "print \"Must be executed by a local client\n\"");
        return;
    }

    BeginIntermission();
    trap_SendServerCommand(ent - g_entities, "clientLevelShot");
}

void Cmd_TeamTask_f(gentity_t* ent) {
    char userinfo[MAX_INFO_STRING];
    char arg[MAX_TOKEN_CHARS];
    int task;
    int client = ent->client - level.clients;

    if (trap_Argc() != 2) {
        return;
    }
    trap_Argv(1, arg, sizeof(arg));
    task = atoi(arg);

    trap_GetUserinfo(client, userinfo, sizeof(userinfo));
    Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
    trap_SetUserinfo(client, userinfo);
    ClientUserinfoChanged(client);
}

void Cmd_Kill_f(gentity_t* ent) {
    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }
    if (ent->health <= 0) {
        return;
    }
    if (!g_allowKill.integer) {
        trap_SendServerCommand(ent - g_entities,
                               "print \"Kill is not enabled on this server.\n\"");
        return;
    }

    ent->s.health = -999;
    ent->health = -999;
    ent->client->ps.stats[STAT_HEALTH] = -999;
    player_die(ent, ent, ent, 100000, MOD_SUICIDE);
}

void BroadcastTeamChange(gclient_t* client, int oldTeam) {
    if (g_training.integer || level.time < 5000) {
        return;
    }
    if (client->sess.sessionTeam == TEAM_RED) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the red team.\n\"",
                                      client->pers.netname));
    } else if (client->sess.sessionTeam == TEAM_BLUE) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the blue team.\n\"",
                                      client->pers.netname));
    } else if (client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the spectators.\n\"",
                                      client->pers.netname));
    } else if (client->sess.sessionTeam == TEAM_FREE) {
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the battle.\n\"",
                                      client->pers.netname));
    }
}

static void SetTeam_SpectateOnly(gentity_t* ent) {
    if (g_gametype.integer != GT_DUEL) {
        return;
    }
    ent->client->sess.specOnly = 1;
    ent->client->sess.playQueue = 0;
    trap_SendServerCommand(ent - g_entities, va("cp \"%s\n\"", "You are set to spectate only"));
}

static qboolean G_TeamsBalanced(void) {
    int blue, red;

    if (g_teamForceBalance.integer) {
        blue = TeamCount(-1, TEAM_BLUE);
        red = TeamCount(-1, TEAM_RED);
        if (blue + 1 < red || red + 1 < blue) {
            return qfalse;
        }
    }
    return qtrue;
}

void SetTeam(gentity_t* ent, const char* s) {
    gclient_t* client = ent->client;
    int clientNum = client - level.clients;
    int oldTeam = client->sess.sessionTeam;
    spectatorState_t specState = SPECTATOR_NOT;
    int specClient = 0;
    int team;

    if (g_training.integer && !(ent->r.svFlags & SVF_BOT) && s &&
        (!Q_stricmp(s, "spectator") || !Q_stricmp(s, "s") || !Q_stricmp(s, "follow1") ||
         !Q_stricmp(s, "follow2"))) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You cannot spectate during training. Finish the tour, or press ready to skip it.\n\"");
        return;
    }

    if (s && !Q_stricmp(s, "follow1")) {
        team = TEAM_SPECTATOR;
        SetTeam_SpectateOnly(ent);
        if (level.numPlayingClients < 1) {
            specState = SPECTATOR_FREE;
        } else {
            specState = SPECTATOR_FOLLOW;
            specClient = -1;
        }
    } else if (s && !Q_stricmp(s, "follow2")) {
        team = TEAM_SPECTATOR;
        SetTeam_SpectateOnly(ent);
        if (level.numPlayingClients > 1) {
            specState = SPECTATOR_FOLLOW;
            specClient = -2;
        } else if (level.numPlayingClients == 1) {
            specState = SPECTATOR_FOLLOW;
            specClient = -1;
        } else {
            specState = SPECTATOR_FREE;
        }
    } else if (s && (!Q_stricmp(s, "spectator") || !Q_stricmp(s, "s"))) {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FREE;
        if (g_gametype.integer != GT_DUEL) {
            goto change;
        }
        SetTeam_SpectateOnly(ent);
    } else if (g_gametype.integer == GT_RR) {
        team = g_rrInfected.integer ? PickTeam_RoundAware(clientNum) : PickTeam(clientNum);
    } else if (g_gametype.integer >= GT_TEAM) {
        int counts[TEAM_NUM_TEAMS];

        counts[TEAM_FREE] = counts[TEAM_SPECTATOR] = 0;
        counts[TEAM_BLUE] = TeamCount(clientNum, TEAM_BLUE);
        counts[TEAM_RED] = TeamCount(clientNum, TEAM_RED);

        if (level.warmupTime < 0 && s) {
            if (!Q_stricmp(s, "red") || !Q_stricmp(s, "r")) {
                team = TEAM_RED;
            } else if (!Q_stricmp(s, "blue") || !Q_stricmp(s, "b")) {
                team = TEAM_BLUE;
            } else {
                team = PickTeam(clientNum);
            }
        } else {
            team = PickTeam(clientNum);
        }

        if (g_teamsize.integer > 0 && team != TEAM_SPECTATOR && counts[team] >= g_teamsize.integer) {
            trap_SendServerCommand(clientNum,
                va("print \"^1Team is full. Team size is currently set to %d.^7\n\"", g_teamsize.integer));
            return;
        }

        if (!G_TeamsBalanced()) {
            counts[TEAM_BLUE] = TeamCount(clientNum, TEAM_BLUE);
            counts[TEAM_RED] = TeamCount(clientNum, TEAM_RED);
            if (team == TEAM_RED && counts[TEAM_BLUE] < counts[TEAM_RED]) {
                trap_SendServerCommand(clientNum, va("cp \"%s has too many players.\n\"", "Red Team"));
                return;
            }
            if (team == TEAM_BLUE && counts[TEAM_RED] < counts[TEAM_BLUE]) {
                trap_SendServerCommand(clientNum, va("cp \"%s has too many players.\n\"", "Blue Team"));
                return;
            }
        } else {
            counts[TEAM_BLUE] = TeamCount(clientNum, TEAM_BLUE);
            counts[TEAM_RED] = TeamCount(clientNum, TEAM_RED);
            counts[oldTeam]++;
            if (team != oldTeam && (oldTeam == TEAM_RED || oldTeam == TEAM_BLUE) &&
                (team == TEAM_RED || team == TEAM_BLUE) && counts[TEAM_RED] == counts[TEAM_BLUE]) {
                trap_SendServerCommand(clientNum,
                    va("cp \"%s team has too many players.\n\"", team == TEAM_RED ? "Red" : "Blue"));
                return;
            }
        }
    } else {
        if (g_gametype.integer == GT_FFA && g_teamsize.integer > 0 &&
            g_teamsize.integer <= level.numNonSpectatorClients) {
            trap_SendServerCommand(clientNum,
                va("print \"^1Arena is full. Team size is currently set to %d.^7\n\"", g_teamsize.integer));
            return;
        }
        team = TEAM_FREE;
        if (g_gametype.integer == GT_DUEL) {
            client->sess.specOnly = 0;
            trap_SendServerCommand(clientNum, va("cp \"%s\n\"", "You are in the queue to play"));
        }
    }

    if (g_gametype.integer == GT_DUEL) {
        if (level.numNonSpectatorClients > 1 || G_NextDuelPlayer() != client) {
            if (team == oldTeam && team != TEAM_SPECTATOR) {
                return;
            }
            team = TEAM_SPECTATOR;
        }
    }

change:
    if ((team != oldTeam || team == TEAM_SPECTATOR) && !G_TeamIsLocked(ent, team)) {
        SetTeam_Execute(ent, team, specState, specClient);
        CalculateRanks();
    }
}

void SetTeam_Execute(gentity_t* ent, int team, spectatorState_t specState, int specClient) {
    gclient_t* client = ent->client;
    int clientNum = client - level.clients;
    int oldTeam;
    int teamLeader;

    oldTeam = client->sess.sessionTeam;
    if (team == oldTeam && team != TEAM_SPECTATOR) {
        return;
    }

    if (client->ps.stats[STAT_HEALTH] <= 0) {
        CopyToBodyQue(ent);
    }

    client->pers.teamState.state = TEAM_BEGIN;
    if (oldTeam != TEAM_SPECTATOR) {
        ent->flags &= ~FL_GODMODE;
        ent->s.health = 0;
        ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
        player_die(ent, ent, ent, 100000, MOD_SUICIDE);
    }

    if (team == TEAM_SPECTATOR) {
        if (!client->sess.playQueue) {
            client->sess.spectatorTime = (int)time(NULL);
        }
        client->ps.eFlags |= EF_SPECTATOR;
        client->pers.ready = qfalse;
    } else {
        client->sess.playQueue = 0;
        client->sess.spectatorTime = 0;
    }

    if (g_gametype.integer != GT_RR || team == TEAM_SPECTATOR) {
        STAT_PlayerStats(client, qfalse, qtrue);
        STAT_InitClient(ent);
    }

    client->sess.sessionTeam = team;
    client->sess.spectatorState = specState;
    client->sess.spectatorClient = specClient;

    if (team == TEAM_SPECTATOR) {
        G_SendItemTimersToSpectator(clientNum);
    }

    client->sess.teamLeader = qfalse;
    if (team == TEAM_RED || team == TEAM_BLUE) {
        teamLeader = TeamLeader(team);
        if (teamLeader == -1 || (!(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT))) {
            SetLeader(team, clientNum);
        }
    }
    if (oldTeam == TEAM_RED || oldTeam == TEAM_BLUE) {
        CheckTeamLeader(oldTeam);
    }

    ClientUserinfoChanged(clientNum);

    if (client->pers.connected != CON_CONNECTED) {
        return;
    }

    BroadcastTeamChange(client, oldTeam);

    ClientBegin(clientNum);

    STAT_PlayerSwitchTeam(ent, oldTeam, client->sess.sessionTeam);
}

void StopFollowing(gentity_t* ent) {
    ent->client->ps.persistant[PERS_TEAM] = ent->client->sess.sessionTeam;
    ent->client->ps.pm_type = PM_SPECTATOR;
    ent->client->ps.pm_flags &= ~PMF_FOLLOW;
    ent->client->sess.spectatorState = SPECTATOR_FREE;
    ent->client->ps.clientNum = ent - g_entities;

    if (ent->r.linked) {
        trap_UnlinkEntity(ent);
    }
}

void Cmd_Team_f(gentity_t* ent) {
    int oldTeam;
    char s[MAX_TOKEN_CHARS];
    const char* teamName;

    if (trap_Argc() != 2) {
        oldTeam = ent->client->sess.sessionTeam;
        if (oldTeam == TEAM_RED) {
            teamName = "Red Team";
        } else if (oldTeam == TEAM_BLUE) {
            teamName = "Blue Team";
        } else if (oldTeam == TEAM_SPECTATOR) {
            teamName = "Spectator";
        } else {
            teamName = "Team Free";
        }
        trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", teamName));
        return;
    }

    if (ent->client->switchTeamTime > level.time) {
        trap_SendServerCommand(ent - g_entities,
            va("print \"" S_COLOR_RED "May not switch teams more than once per %i seconds." S_COLOR_WHITE "\n\"",
               g_switchTeamDelay.integer));
        return;
    }

    if ((level.warmupTime == 0) && (g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }

    trap_Argv(1, s, sizeof(s));

    oldTeam = ent->client->sess.sessionTeam;
    SetTeam(ent, s);

    if (ent->client->sess.sessionTeam != oldTeam && ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        ent->client->switchTeamTime = level.time + g_switchTeamDelay.integer * 1000;
    }
}

void Cmd_Follow_f(gentity_t* ent) {
    int i;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() != 2) {
        if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
            StopFollowing(ent);
        }
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    i = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (i == -1) {
        return;
    }

    if (&level.clients[i] == ent->client) {
        return;
    }

    if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    if ((g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }

    if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        SetTeam(ent, "spectator");
    }

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
}

void FollowCycle(gentity_t* ent, int dir) {
    int clientnum;
    int original;
    gclient_t* cl;

    if (dir != 1 && dir != -1) {
        G_Error("FollowCycle: bad dir %i", dir);
    }

    clientnum = ent->client->sess.spectatorClient;
    original = clientnum;
    if (clientnum < 0 || clientnum >= level.maxclients) {
        G_Printf("FollowCycle: bad input clientnum value: %d, maxclients: %d\n", clientnum, level.maxclients);
        original = 0;
    }

    do {
        clientnum += dir;
        if (clientnum >= level.maxclients) {
            clientnum = 0;
        } else if (clientnum < 0) {
            clientnum = level.maxclients - 1;
        }

        cl = &level.clients[clientnum];
        if (cl->pers.connected != CON_CONNECTED || cl->ps.pm_type == PM_SPECTATOR ||
            (cl->ps.pm_flags & PMF_FOLLOW)) {
            continue;
        }

        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && g_gametype.integer >= GT_TEAM &&
            cl->sess.sessionTeam != ent->client->sess.sessionTeam) {
            continue;
        }

        ent->client->sess.spectatorClient = clientnum;
        if (ent->client->sess.spectatorState != SPECTATOR_FOLLOW) {
            Cmd_Score_f(ent);
        }
        ent->client->sess.spectatorState = SPECTATOR_FOLLOW;

        if (g_gametype.integer == GT_RACE) {
            gclient_t* cl = &level.clients[clientnum];
            trap_SendServerCommand(ent - g_entities,
                va("race_info %i %i %i %i %i %i",
                   cl->race.racingActive == 1,
                   cl->race.startTime,
                   cl->race.lastTime,
                   cl->race.currentCheckPoint,
                   cl->race.nextRacePoint ? cl->race.nextRacePoint->s.number : -1,
                   cl->race.nextRacePoint2 ? cl->race.nextRacePoint2->s.number : -1));
        }
        return;
    } while (clientnum != original);

}

void Cmd_FollowCycle_f(gentity_t* ent, int dir) {
    if (level.warmupTime == 0 && g_gametype.integer == GT_DUEL && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }
    if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        SetTeam(ent, "spectator");
    }
    FollowCycle(ent, dir);
}

static void G_SayTo(gentity_t* ent, gentity_t* other, int mode, int color, const char* name, const char* message) {
    if (!other) {
        return;
    }
    if (!other->inuse) {
        return;
    }
    if (!other->client) {
        return;
    }
    if (other->client->pers.connected != CON_CONNECTED) {
        return;
    }
    if (mode == SAY_TEAM && !OnSameTeam(ent, other)) {
        return;
    }
    if ((g_gametype.integer == GT_DUEL) && other->client->sess.sessionTeam == TEAM_FREE && ent->client->sess.sessionTeam != TEAM_FREE) {
        return;
    }

    trap_SendServerCommand(other - g_entities, va("%s \"%02d %s%c%c%s\"",
                                                  mode == SAY_TEAM ? "tchat" : "chat",
                                                  ent->s.number,
                                                  name, Q_COLOR_ESCAPE, color, message));
}

#define EC "\x19"

static void G_CutUtf8Tail(char* text) {
    int len = (int)strlen(text);
    int i = len;
    int need;
    unsigned char lead;

    while (i > 0 && ((unsigned char)text[i - 1] & 0xC0) == 0x80) {
        i--;
    }
    if (i == 0 || !((unsigned char)text[i - 1] & 0x80)) {
        return;
    }
    lead = (unsigned char)text[i - 1];
    need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (len - (i - 1) < need) {
        text[i - 1] = '\0';
    }
}

void G_Say(gentity_t* ent, gentity_t* target, int mode, const char* chatText) {
    int j;
    gentity_t* other;
    int color;
    char name[64];
    char text[MAX_SAY_TEXT];
    char location[64];

    if (g_training.integer == 2 && ent && ent->client && !(ent->r.svFlags & SVF_BOT)) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Pay attention to Crash. You can chat once the tour is over.\n\"");
        return;
    }

    if (g_gametype.integer < GT_TEAM && mode == SAY_TEAM) {
        mode = SAY_ALL;
    }

    switch (mode) {
        default:
        case SAY_ALL:
            G_LogPrintf("say: %s: %s\n", ent->client->pers.netname, chatText);
            Com_sprintf(name, sizeof(name), "%s%c%c" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            color = COLOR_GREEN;
            break;
        case SAY_TEAM:
            G_LogPrintf("sayteam: %s: %s\n", ent->client->pers.netname, chatText);
            if (Team_GetLocationMsg(ent, location, sizeof(location)))
                Com_sprintf(name, sizeof(name), EC "(%s%c%c" EC ") (%s)" EC ": ",
                            ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
            else
                Com_sprintf(name, sizeof(name), EC "(%s%c%c" EC ")" EC ": ",
                            ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            color = COLOR_CYAN;
            break;
        case SAY_TELL:
            if (target && target->inuse && target->client && g_gametype.integer >= GT_TEAM &&
                target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
                Team_GetLocationMsg(ent, location, sizeof(location)))
                Com_sprintf(name, sizeof(name), EC "[%s%c%c" EC "] (%s)" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
            else
                Com_sprintf(name, sizeof(name), EC "[%s%c%c" EC "]" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            color = COLOR_MAGENTA;
            break;
    }

    Q_strncpyz(text, chatText, sizeof(text));
    if (strlen(chatText) >= sizeof(text)) {
        G_CutUtf8Tail(text);
    }

    if (target) {
        G_SayTo(ent, target, mode, color, name, text);
        return;
    }

    if (g_dedicated.integer) {
        G_Printf("%s%s\n", name, text);
    }

    for (j = 0; j < level.maxclients; j++) {
        other = &g_entities[j];
        G_SayTo(ent, other, mode, color, name, text);
    }
}

static void SanitizeChatText(char* text) {
    int i;

    for (i = 0; text[i]; i++) {
        if (text[i] == '\n' || text[i] == '\r') {
            text[i] = ' ';
        }
    }
}

static void Cmd_Say_f(gentity_t* ent, int mode, qboolean arg0) {
    char* p;

    if (trap_Argc() < 2 && !arg0) {
        return;
    }

    if (arg0) {
        p = ConcatArgs(0);
    } else {
        p = ConcatArgs(1);
    }

    SanitizeChatText(p);

    G_Say(ent, NULL, mode, p);
}

static void Cmd_Tell_f(gentity_t* ent) {
    int targetNum;
    gentity_t* target;
    char* p;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() < 3) {
        trap_SendServerCommand(ent - g_entities, "print \"Usage: tell <player id> <message>\n\"");
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (targetNum == -1) {
        return;
    }

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }

    p = ConcatArgs(2);

    SanitizeChatText(p);

    G_LogPrintf("tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p);
    G_Say(ent, target, SAY_TELL, p);
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Say(ent, ent, SAY_TELL, p);
    }
}

static void Cmd_Voice_f(void) {
    if (trap_Argc() > 1) {
        ConcatArgs(1);
    }
}

static void Cmd_VoiceTell_f(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    gentity_t* target;
    int targetNum;

    if (trap_Argc() < 2) {
        return;
    }
    trap_Argv(1, arg, sizeof(arg));
    targetNum = atoi(arg);
    if (targetNum < 0 || targetNum >= level.maxclients) {
        return;
    }
    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }
    G_LogPrintf("vtell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, ConcatArgs(2));
}

static void Cmd_VoiceTaunt_f(gentity_t* ent) {
    if (!ent->client) {
        return;
    }

    if (ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client == ent->s.number) {
        ent->enemy = NULL;
        return;
    }
    if (ent->client->lastkilled_client >= 0 && ent->client->lastkilled_client != ent->s.number &&
        g_entities[ent->client->lastkilled_client].client) {
        ent->client->lastkilled_client = -1;
    }
}

static char* gc_orders[] = {
    "hold your position",
    "hold this position",
    "come here",
    "cover me",
    "guard location",
    "search and destroy",
    "report"};

static const int numgc_orders = ARRAY_LEN(gc_orders);

void Cmd_GameCommand_f(gentity_t* ent) {
    int targetNum;
    gentity_t* target;
    int order;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() != 3) {
        trap_SendServerCommand(ent - g_entities, va("print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1));
        return;
    }

    trap_Argv(2, arg, sizeof(arg));
    order = atoi(arg);

    if (order < 0 || order >= numgc_orders) {
        trap_SendServerCommand(ent - g_entities, va("print \"Bad order: %i\n\"", order));
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, qtrue, qtrue);
    if (targetNum == -1) {
        return;
    }

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }

    G_LogPrintf("tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order]);
    G_Say(ent, target, SAY_TELL, gc_orders[order]);
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Say(ent, ent, SAY_TELL, gc_orders[order]);
    }
}

void Cmd_Where_f(gentity_t* ent) {
    char buf[32];

    Com_sprintf(buf, sizeof(buf), "(%.2f %.2f %.2f)",
                ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
    trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", buf));
}

static qboolean IsValidCallvoteArg(const char *s) {
    const char *c;
    for (c = s; *c; c++) {
        if (*c == '\n' || *c == '\r' || *c == ';') {
            return qfalse;
        }
    }
    return qtrue;
}

static void StartVote(void) {
    int i;

    level.voteTime = level.time;
    level.voteYes = 1;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected != CON_CONNECTED) {
            continue;
        }
        if (g_entities[i].r.svFlags & SVF_BOT) {
            level.clients[i].pers.voteState = VOTE_NONE;
        } else if (!g_allowSpecVote.integer &&
                   level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) {
            level.clients[i].pers.voteState = VOTE_NONE;
        } else {
            level.clients[i].pers.voteState = VOTE_PENDING;
        }
    }
    level.clients[level.pendingVoteCaller].pers.voteState = VOTE_YES;

    trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
    trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
    trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
    trap_SetConfigstring(CS_VOTE_NO, "0");

    trap_SendServerCommand(-1, va("print \"%s called a vote.\n\"",
        g_entities[level.pendingVoteCaller].client->pers.netname));
}

void ClearVote(void) {
    int i;

    level.voteTime = 0;
    level.voteYes = 0;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++) {
        level.clients[i].pers.voteState = VOTE_NONE;
    }

    trap_SetConfigstring(CS_VOTE_TIME, "");
    trap_SetConfigstring(CS_VOTE_STRING, "");
    trap_SetConfigstring(CS_DISABLE_VOTE_UI, "");
}

void Cmd_CallVote_f(gentity_t *ent) {
    int clientNum = ent - g_entities;
    char arg1[MAX_STRING_TOKENS];
    char arg2[MAX_STRING_TOKENS];
    char arg3[MAX_STRING_TOKENS];
    int vf = g_voteFlags.integer;

    if (level.time < g_voteDelay.integer) {
        trap_SendServerCommand(clientNum,
            va("print \"Voting will be allowed in %.1f seconds.\n\"",
               (float)(g_voteDelay.integer - level.time) / 1000.0f));
        return;
    }

    if (g_training.integer) {
        trap_SendServerCommand(clientNum,
            "print \"Voting is not allowed in training.\n\"");
        return;
    }

    if (!g_allowVote.integer) {
        trap_SendServerCommand(clientNum,
            "print \"Public voting is not allowed here.\n\"");
        return;
    }

    if (level.voteTime) {
        trap_SendServerCommand(clientNum,
            "print \"A vote is already in progress.\n\"");
        return;
    }

    if (level.voteExecuteTime && level.time < level.voteExecuteTime) {
        trap_SendServerCommand(clientNum,
            "print \"A vote is being executed.\n\"");
        return;
    }

    if (g_voteLimit.integer &&
        ent->client->pers.voteCount >= g_voteLimit.integer) {
        trap_SendServerCommand(clientNum,
            "print \"You have called the maximum number of votes.\n\"");
        return;
    }

    if (!g_allowSpecVote.integer &&
        ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        trap_SendServerCommand(clientNum,
            "print \"Not allowed to call a vote as spectator.\n\"");
        return;
    }

    if (!g_allowVoteMidGame.integer && level.warmupTime == 0) {
        trap_SendServerCommand(clientNum,
            "print \"Voting is only allowed during the warm up period.\n\"");
        return;
    }

    trap_Argv(1, arg1, sizeof(arg1));
    trap_Argv(2, arg2, sizeof(arg2));
    trap_Argv(3, arg3, sizeof(arg3));

    if (!IsValidCallvoteArg(arg1) || !IsValidCallvoteArg(arg2) || !IsValidCallvoteArg(arg3)) {
        trap_SendServerCommand(clientNum, "print \"Invalid vote string.\n\"");
        return;
    }

    if (!Q_stricmp(arg1, "map_restart")) {
        if ((vf & VF_MAP_RESTART) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting on a map restart is disabled on this server.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "nextmap")) {
        char nextmap[MAX_STRING_CHARS];
        if ((vf & VF_NEXTMAP) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to move to the next map is disabled on this server.\n\"");
            return;
        }
        trap_Cvar_VariableStringBuffer("nextmap", nextmap, sizeof(nextmap));
        if (!nextmap[0] || !Q_stricmpn(nextmap, "map_restart 0", 13)) {
            trap_SendServerCommand(clientNum,
                "print \"No nextmap is currently set.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "map")) {
        if ((vf & VF_MAP) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the map being played is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum, "print \"Missing map name.\n\"");
            return;
        }
        if ((vf & VF_GAMETYPE) && arg3[0] && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the gametype being played is disabled on this server.\n\"");
            return;
        }
        {
            fileHandle_t f;
            char bspPath[MAX_QPATH];
            Com_sprintf(bspPath, sizeof(bspPath), "maps/%s.bsp", arg2);
            if (trap_FS_FOpenFile(bspPath, &f, FS_READ) < 0) {
                trap_SendServerCommand(clientNum,
                    "print \"Map does not exist.\n\"");
                trap_FS_FCloseFile(f);
                return;
            }
            trap_FS_FCloseFile(f);
        }
        if (arg3[0] && !trap_FactoryExists(arg3)) {
            trap_SendServerCommand(clientNum, "print \"Factory does not exist.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "kick")) {
        int i;
        if ((vf & VF_KICK) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to kick a player is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum, "print \"Missing player name.\n\"");
            return;
        }
        if (!Q_stricmp(arg2, "all")) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to kick all players is not allowed.\n\"");
            return;
        }
        i = ClientNumberFromString(ent, arg2, qfalse, qtrue);
        if (i == -1) {
            trap_SendServerCommand(clientNum,
                va("print \"Player %s is not on the server.\n\"", arg2));
            return;
        }
        if (level.clients[i].pers.localClient) {
            trap_SendServerCommand(clientNum,
                "print \"Cannot kick host player.\n\"");
            return;
        }
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "clientkick %d", i);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "kick %s", level.clients[i].pers.netname);
        goto start_vote;
    }
    else if (!Q_stricmp(arg1, "clientkick")) {
        int i;
        if ((vf & VF_KICK) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to kick a player is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum, "print \"Missing player id.\n\"");
            return;
        }
        if (!StringIsInteger(arg2)) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid player id, parameter must be an integer.\n\"");
            return;
        }
        i = ClientNumberFromString(ent, arg2, qtrue, qfalse);
        if (i == -1) {
            trap_SendServerCommand(clientNum, "print \"Invalid player id.\n\"");
            return;
        }
        if (level.clients[i].sess.privileges > 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to kick a server admin is not allowed.\n\"");
            return;
        }
        if (level.clients[i].pers.localClient) {
            trap_SendServerCommand(clientNum,
                "print \"Cannot kick host player.\n\"");
            return;
        }
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "clientkick %d", i);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "kick %s", level.clients[i].pers.netname);
        goto start_vote;
    }
    else if (!Q_stricmp(arg1, "timelimit")) {
        if ((vf & VF_TIMELIMIT) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the games time limit is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired timelimit.\n\"");
            return;
        }
        if (!StringIsInteger(arg2)) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid desired timelimit, parameter must be an integer in minutes.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "fraglimit")) {
        if ((vf & VF_FRAGLIMIT) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the games frag limit is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired fraglimit.\n\"");
            return;
        }
        if (!StringIsInteger(arg2)) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid desired fraglimit, parameter must be an integer.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "weaprespawn")) {
        if ((vf & VF_WEAPRESPAWN) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the weapon respawn time is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired weapon respawn time.\n\"");
            return;
        }
        if (!StringIsInteger(arg2)) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid desired weapon respawn time, parameter must be an integer.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "shuffle")) {
        if ((vf & VF_SHUFFLE) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to shuffle the teams is disabled on this server.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to shuffle the teams is only permitted during warmup.\n\"");
            return;
        }
        if (arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Too many parameters called for a shuffle.\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "teamsize")) {
        int ts;
        if (g_gametype.integer == GT_DUEL) {
            trap_SendServerCommand(clientNum,
                "print \"Teamsize is not available in Duel.\n\"");
            return;
        }
        if ((vf & VF_TEAMSIZE) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change team size is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Missing desired teamsize.\n\"");
            return;
        }
        if (!StringIsInteger(arg2)) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid desired teamsize, parameter must be an integer.\n\"");
            return;
        }
        ts = atoi(arg2);
        if ((unsigned)ts > 64) {
            trap_SendServerCommand(clientNum,
                va("print \"Invalid team size. (Valid Range: %d - %d)\n\"",
                   g_teamSizeMin.integer, g_maxclients.integer));
            return;
        }
        if (ts > 0) {
            int counts[TEAM_NUM_TEAMS];
            int red, blue;
            int maxSize = (g_gametype.integer >= GT_TEAM) ? g_maxclients.integer / 2
                                                         : g_maxclients.integer;

            Team_ConnectedTeamCounts(counts);
            red = counts[TEAM_RED];
            blue = counts[TEAM_BLUE];

            if (ts < counts[TEAM_FREE]) {
                trap_SendServerCommand(clientNum,
                    va("print \"^1The arena has more than %d players. Players must leave before this teamsize can be set.^7\"",
                       ts));
                return;
            }
            if (ts < red || ts < blue) {
                trap_SendServerCommand(clientNum,
                    va("print \"^1%s has more than %d players. Players must leave the team before this teamsize can be set.^7\"",
                       (ts < red) ? "Red Team" : "Blue Team", ts));
                return;
            }
            if (ts < g_teamSizeMin.integer || ts > maxSize) {
                trap_SendServerCommand(clientNum,
                    va("print \"Invalid team size. (Valid Range: %d - %d)\n\"",
                       g_teamSizeMin.integer, maxSize));
                return;
            }
        }
    }
    else if (!Q_stricmp(arg1, "cointoss")) {
        if ((vf & VF_COINTOSS) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to coin toss is disabled on this server.\n\"");
            return;
        }
        if (arg2[0] && Q_stricmp(arg2, "heads") && Q_stricmp(arg2, "tails")
            && Q_stricmp(arg2, "h") && Q_stricmp(arg2, "t")) {
            trap_SendServerCommand(clientNum,
                "print \"Valid cointoss parameters are:    ^5heads    ^5tails ^7\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "random")) {
        int val;
        if ((vf & VF_COINTOSS) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Random number generation is disabled on this server.\n\"");
            return;
        }
        if (!arg2[0]) {
            trap_SendServerCommand(clientNum,
                "print \"Usage: ^3\\callvote random <2 to 100>^7\n\"");
            trap_SendServerCommand(clientNum,
                "print \"       ^7Picks a number from 1 to <value>\n\"");
            trap_SendServerCommand(clientNum,
                "print \"       ^2callvote random 2 ^7mimics flipping a coin\n\"");
            return;
        }
        if (!StringIsInteger(arg2)) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid upper limit, parameter must be an integer.\n\"");
            return;
        }
        val = atoi(arg2);
        if (val < 2 || val > 100) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid upper limit. (Valid Range: 2 - 100)\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "loadouts")) {
        if ((vf & VF_LOADOUTS) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter loadouts is disabled on this server.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter loadouts is only allowed during the warm up period.\n\"");
            return;
        }
        if (Q_stricmp(arg2, "ON") && Q_stricmp(arg2, "OFF")) {
            trap_SendServerCommand(clientNum,
                "print \"^3Valid loadout options are:    ^5ON    ^5OFF^7\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "ammo")) {
        if ((vf & VF_AMMO) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the ammo system is disabled on this server.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the ammo system is only allowed during the warm up period.\n\"");
            return;
        }
        if (Q_stricmp(arg2, "GLOBAL") && Q_stricmp(arg2, "WEAP")) {
            trap_SendServerCommand(clientNum,
                "print \"^3Valid ammo options are:    ^5GLOBAL    ^5WEAP^7\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "timers")) {
        if ((vf & VF_TIMERS) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the item timers is disabled on this server.\n\"");
            return;
        }
        if (level.warmupTime == 0) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to alter the item timers is only allowed during the warm up period.\n\"");
            return;
        }
        if (Q_stricmp(arg2, "ON") && Q_stricmp(arg2, "OFF")) {
            trap_SendServerCommand(clientNum,
                "print \"^3Valid item timer options are:    ^5ON    ^5OFF^7\n\"");
            return;
        }
    }
    else if (!Q_stricmp(arg1, "g_gametype")) {
        int gt;
        if ((vf & VF_GAMETYPE) && ent->client->sess.privileges < 1) {
            trap_SendServerCommand(clientNum,
                "print \"Voting to change the gametype being played is disabled on this server.\n\"");
            return;
        }
        gt = atoi(arg2);
        if (gt < GT_FFA || gt >= GT_MAX_GAME_TYPE) {
            trap_SendServerCommand(clientNum,
                "print \"Invalid gametype.\n\"");
            return;
        }
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s %d", arg1, gt);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s %s", arg1, gametypeDisplayNames[gt]);
        goto start_vote;
    }
    else {
        trap_SendServerCommand(clientNum, "print \"^3Callvote commands:\n\"");
        trap_SendServerCommand(clientNum,
            va("print \"^%imap           ^%inextmap        ^%imap_restart   ^7\n\"",
               (vf & VF_MAP) ? 1 : 5,
               (vf & VF_NEXTMAP) ? 1 : 5,
               (vf & VF_MAP_RESTART) ? 1 : 5));
        trap_SendServerCommand(clientNum,
            va("print \"^%ikick          ^%iclientkick                      ^7\n\"",
               (vf & VF_KICK) ? 1 : 5,
               (vf & VF_KICK) ? 1 : 5));
        trap_SendServerCommand(clientNum,
            va("print \"^%ishuffle       ^%iteamsize       ^%icointoss      ^7\n\"",
               (vf & VF_SHUFFLE) ? 1 : 5,
               (vf & VF_TEAMSIZE) ? 1 : 5,
               (vf & VF_COINTOSS) ? 1 : 5));
        trap_SendServerCommand(clientNum,
            va("print \"^%itimelimit     ^%ifraglimit      ^%iweaprespawn   ^7\n\"",
               (vf & VF_TIMELIMIT) ? 1 : 5,
               (vf & VF_FRAGLIMIT) ? 1 : 5,
               (vf & VF_WEAPRESPAWN) ? 1 : 5));
        trap_SendServerCommand(clientNum,
            va("print \"^%iloadouts      ^%iammo           ^%itimers        ^7\n\"",
               (vf & VF_LOADOUTS) ? 1 : 5,
               (vf & VF_AMMO) ? 1 : 5,
               (vf & VF_TIMERS) ? 1 : 5));
        trap_SendServerCommand(clientNum,
            "print \"Usage: ^3\\callvote <command> <params>^7\n\"");
        return;
    }

    if (level.voteExecuteTime) {
        level.voteExecuteTime = 0;
        trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
    }

    if (!Q_stricmp(arg1, "map")) {
        if (arg3[0]) {
            Com_sprintf(level.voteString, sizeof(level.voteString),
                "%s %s %s", arg1, arg2, arg3);
        } else {
            Com_sprintf(level.voteString, sizeof(level.voteString),
                "%s %s", arg1, arg2);
        }
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s", level.voteString);
    } else if (!Q_stricmp(arg1, "nextmap")) {
        char nextmap[MAX_STRING_CHARS];
        trap_Cvar_VariableStringBuffer("nextmap", nextmap, sizeof(nextmap));
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s", nextmap);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s", level.voteString);
    } else if (!Q_stricmp(arg1, "cointoss") || !Q_stricmp(arg1, "random")) {
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s %s", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s %s", arg1, arg2);
    } else {
        Com_sprintf(level.voteString, sizeof(level.voteString),
            "%s \"%s\"", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString),
            "%s", level.voteString);
    }

start_vote:
    level.pendingVoteCaller = clientNum;
    ent->client->pers.voteCount++;
    StartVote();
}

void G_TallyMapVotes(void);

static void Cmd_IntermissionVote_f(gentity_t *ent) {
    int clientNum = ent - g_entities;
    char msg[64];
    int choice;
    char nextmaps[MAX_STRING_CHARS];

    if (g_voteFlags.integer & VF_ENDMAP_VOTING) {
        return;
    }

    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (!nextmaps[0]) {
        return;
    }

    trap_Argv(1, msg, sizeof(msg));
    choice = atoi(msg);

    if (level.time - level.voteTime >= 20000) {
        trap_SendServerCommand(clientNum, "print \"Voting time has expired.\n\"");
        return;
    }

    if (level.time - ent->client->pers.lastMapVoteTime < 2000) {
        trap_SendServerCommand(clientNum, "print \"You may only vote once every 2 seconds.\n\"");
        return;
    }

    if (choice == ent->client->pers.lastMapVoteIndex) {
        trap_SendServerCommand(clientNum, "print \"You already voted for this arena.\n\"");
        return;
    }

    if (choice < 1 || choice > 3 || !level.intermissionMapNames[choice - 1][0]) {
        return;
    }

    ent->client->pers.lastMapVoteTime = level.time;
    ent->client->pers.lastMapVoteIndex = choice;

    trap_SendServerCommand(clientNum, "disable_vote_ui");

    trap_SendServerCommand(-1,
        va("print \"%s" S_COLOR_WHITE " voted for %s.\n\"",
           ent->client->pers.netname,
           level.intermissionMapTitles[choice - 1][0]
               ? level.intermissionMapTitles[choice - 1]
               : level.intermissionMapNames[choice - 1]));

    G_TallyMapVotes();
}

void G_TallyMapVotes(void) {
    char votes[MAX_INFO_STRING];
    int i;

    level.intermissionMapVotes[0] = level.intermissionMapVotes[1] = level.intermissionMapVotes[2] = 0;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];

        if (cl->pers.connected == CON_CONNECTED && cl->pers.lastMapVoteIndex >= 1 &&
            cl->pers.lastMapVoteIndex <= 3) {
            level.intermissionMapVotes[cl->pers.lastMapVoteIndex - 1]++;
        }
    }
    votes[0] = '\0';
    for (i = 0; i < 3; i++) {
        Info_SetValueForKey(votes, va("%i", i), va("%i", level.intermissionMapVotes[i]));
    }
    trap_SetConfigstring(CS_ROTATIONVOTES, votes);
}

void Cmd_Vote_f(gentity_t *ent) {
    char msg[64];

    if (level.intermissionTime) {
        Cmd_IntermissionVote_f(ent);
        return;
    }

    if (!level.voteTime) {
        trap_SendServerCommand(ent - g_entities, "print \"No vote in progress.\n\"");
        return;
    }

    if (ent->client->sess.privileges < 1) {
        if (ent->client->pers.voteState == VOTE_NONE) {
            trap_SendServerCommand(ent - g_entities,
                "print \"You may not participate in this vote.\n\"");
            return;
        }
        if (ent->client->pers.voteState != VOTE_PENDING) {
            trap_SendServerCommand(ent - g_entities, "print \"Vote already cast.\n\"");
            return;
        }
    }

    trap_SendServerCommand(ent - g_entities, "print \"Vote cast.\n\"");

    trap_Argv(1, msg, sizeof(msg));

    if (msg[0] != 'y' && msg[0] != 'Y' && msg[0] != '1') {
        level.voteNo++;
        trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
        ent->client->pers.voteState =
            (ent->client->sess.privileges > 0) ? VOTE_VETOED : VOTE_NO;
    } else {
        level.voteYes++;
        trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
        ent->client->pers.voteState =
            (ent->client->sess.privileges > 0) ? VOTE_PASSED : VOTE_YES;
    }
}

void Cmd_SetViewpos_f(gentity_t* ent) {
    vec3_t origin, angles;
    char buffer[MAX_TOKEN_CHARS];
    int i;

    if (!g_cheats.integer) {
        trap_SendServerCommand(ent - g_entities, "print \"Cheats are not enabled on this server.\n\"");
        return;
    }
    if (trap_Argc() != 5) {
        trap_SendServerCommand(ent - g_entities, "print \"usage: setviewpos x y z yaw\n\"");
        return;
    }

    VectorClear(angles);
    for (i = 0; i < 3; i++) {
        trap_Argv(i + 1, buffer, sizeof(buffer));
        origin[i] = atof(buffer);
    }

    trap_Argv(4, buffer, sizeof(buffer));
    angles[YAW] = atof(buffer);

    TeleportPlayer(ent, origin, angles);
}

void Cmd_Stats_f(gentity_t* ent) {
}

static void Cmd_Acc_f(gentity_t* ent) {
    gclient_t* cl;
    char result[1024];
    char buf[MAX_TOKEN_CHARS];
    int i;

    cl = ent->client;
    if (!cl) {
        return;
    }
    if (cl->ps.pm_type == PM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW) {
        cl = &level.clients[cl->sess.spectatorClient];
    }

    result[0] = '\0';
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        if (cl->expandedStats.shotsHit[i] == 0 || cl->expandedStats.shotsFired[i] == 0) {
            Com_sprintf(buf, sizeof(buf), " 0");
        } else {
            Com_sprintf(buf, sizeof(buf), " %i",
                        cl->expandedStats.shotsHit[i] * 100 / cl->expandedStats.shotsFired[i]);
        }
        if (strlen(result) + strlen(buf) < sizeof(result)) {
            Q_strcat(result, sizeof(result), buf);
        }
    }
    trap_SendServerCommand(ent - g_entities, va("acc %s", result));
}

static void Cmd_PStats_f(gentity_t* ent) {
    gclient_t* cl;
    char result[1024];
    char buf[MAX_TOKEN_CHARS];
    int i;

    cl = ent->client;
    if (!cl) {
        return;
    }
    if (cl->ps.pm_type == PM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW) {
        cl = &level.clients[cl->sess.spectatorClient];
    }

    result[0] = '\0';
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        if (cl->expandedStats.shotsHit[i] == 0 || cl->expandedStats.shotsFired[i] == 0) {
            Com_sprintf(buf, sizeof(buf), " 0");
        } else {
            Com_sprintf(buf, sizeof(buf), " %i",
                        cl->expandedStats.shotsHit[i] * 100 / cl->expandedStats.shotsFired[i]);
        }
        if (strlen(result) + strlen(buf) < sizeof(result)) {
            Q_strcat(result, sizeof(result), buf);
        }
    }
    trap_SendServerCommand(ent - g_entities, va("pstats %s", result));
}

static void Cmd_ReadyUp_f(gentity_t* ent) {
    gclient_t* cl;

    if (level.warmupTime >= 0 && level.intermissionTime == 0)
        return;

    if (g_training.integer && g_skipTrainingEnable.integer) {
        Cmd_SkipTraining(ent->s.clientNum);
    } else if (g_training.integer == 2) {
        return;
    }
    cl = ent->client;
    if (!cl) return;
    if (cl->ps.pm_type == PM_SPECTATOR) return;
    if (cl->sess.sessionTeam == TEAM_SPECTATOR) return;
    if (cl->ps.pm_flags & PMF_FOLLOW) return;

    if (level.warmupTime < 0) {
        if (!TeamsPresent()) {
            if (cl->pers.ready) {
                cl->pers.ready = qfalse;
                ClientUserinfoChanged(ent - g_entities);
                return;
            }
            if (BG_IsScoreBasedGameType()) {
                trap_SendServerCommand(ent - g_entities,
                    "print \"Players cannot ready up until both teams are present.\n\"");
            } else {
                trap_SendServerCommand(ent - g_entities,
                    "print \"Cannot ready up until more players are present.\n\"");
            }
            return;
        }
        if (!G_CheckTeamBalance()) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Players cannot ready up until both teams are fully present.\n\"");
            return;
        }
    }

    if (level.intermissionTime != 0 && cl->pers.ready)
        return;

    cl->pers.ready = !cl->pers.ready;
    ClientUserinfoChanged(ent - g_entities);

    if (level.warmupTime < 0) {
        trap_SendServerCommand(-1, va("cp \"%s ^7is ^7%s\n\"",
            cl->pers.netname,
            cl->pers.ready ? "Ready" : "Not Ready"));
    }
}

static void Cmd_BotSay_f(gentity_t* ent) {
    int targetNum;
    int duration;
    char *p;
    char arg[MAX_TOKEN_CHARS];
    char name[64];

    if (trap_Argc() < 3) return;
    if (!(ent->r.svFlags & SVF_BOT)) return;

    trap_Argv(1, arg, sizeof(arg));
    targetNum = atoi(arg);
    if (targetNum < 0 || targetNum >= level.maxclients) return;
    if (!g_entities[targetNum].inuse || !g_entities[targetNum].client) return;
    if (g_entities[targetNum].client->pers.connected != CON_CONNECTED) return;

    trap_Argv(2, arg, sizeof(arg));
    duration = atoi(arg);
    if (duration < 0) return;

    p = ConcatArgs(3);

    Com_sprintf(name, sizeof(name), "%s\x19: ",
        ent->client->pers.netname);

    trap_SendServerCommand(targetNum,
        va("bchat \"%s%c%c%s\" %i", name, Q_COLOR_ESCAPE, COLOR_GREEN, p, duration));
}

static void Cmd_Spec_f(gentity_t* ent) {
    int clientnum, original;
    char filter[MAX_TOKEN_CHARS];
    gclient_t* cl;

    if (trap_Argc() != 2) {
        Cmd_Follow_f(ent);
        return;
    }

    trap_Argv(1, filter, sizeof(filter));

    clientnum = ent->client->sess.spectatorClient;
    if (clientnum < 0 || clientnum >= level.maxclients) {
        clientnum = 0;
    }
    original = clientnum;

    do {
        clientnum++;
        if (clientnum >= level.maxclients) clientnum = 0;

        cl = &g_clients[clientnum];
        if (cl->pers.connected != CON_CONNECTED) continue;
        if (cl->sess.sessionTeam == TEAM_SPECTATOR) continue;
        if (cl->ps.pm_flags & PMF_FOLLOW) continue;

        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
            g_gametype.integer >= GT_TEAM &&
            cl->sess.sessionTeam != ent->client->sess.sessionTeam) {
            continue;
        }

        if (!Q_stricmp(filter, "stats")) {
            if (cl->expandedStats.numKills == 0 &&
                cl->expandedStats.numDeaths == 0 &&
                cl->expandedStats.numAssists == 0) {
                continue;
            }
        } else if (!Q_stricmp(filter, "red") || !Q_stricmp(filter, "r")) {
            if (cl->sess.sessionTeam != TEAM_RED) continue;
        } else if (!Q_stricmp(filter, "blue") || !Q_stricmp(filter, "b")) {
            if (cl->sess.sessionTeam != TEAM_BLUE) continue;
        } else if (!Q_stricmp(filter, "powerup")) {
            int hasPowerup = 0;
            int pw;
            for (pw = PW_QUAD; pw <= PW_AMMOREGEN; pw++) {
                if (cl->ps.powerups[pw] > 0) { hasPowerup = 1; break; }
            }
            if (!hasPowerup) continue;
        }

        ent->client->sess.spectatorClient = clientnum;
        ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
        return;
    } while (clientnum != original);
}

static void Cmd_SpecResp_f(gentity_t *ent) {
    G_SendItemTimersToSpectator(ent - g_entities);
}

static void Cmd_Forfeit_f(gentity_t* ent) {
    if (!ent->client) return;
    if (ent->client->ps.pm_type == PM_SPECTATOR) return;

    if (level.timePauseBegin != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Forfeit is not available during a pause or timeout.\n\"");
        return;
    }

    switch (g_gametype.integer) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        if (level.warmupTime > 0 || level.roundState.eCurrent == RS_COUNTDOWN) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is not available during a round countdown.\n\"");
            return;
        }
        break;
    }

    if (CheckForfeitConditions(ent, 1)) {
        ForfeitMatch();
    }
}

static void Cmd_PermHelp_f(gentity_t* ent);

static void G_AdminReply(gentity_t* ent, const char* text) {
    if (!ent) {
        G_Printf("%s", text);
        return;
    }
    trap_SendServerCommand(ent - g_entities, text);
}

static qboolean G_AdminIsServer(gentity_t* ent) {
    return (qboolean)(!ent || ent->client->pers.localClient);
}

static void Cmd_Players_f(gentity_t* ent) {
    static const char privChars[] = " MA*";
    int i;
    int priv;
    gclient_t* cl;

    for (i = 0; i < level.maxclients; i++) {
        cl = &g_clients[i];
        if (!g_entities[i].inuse || cl->pers.connected != CON_CONNECTED) continue;

        priv = cl->sess.privileges;
        if (priv < 0 || priv > 3) priv = 0;
        G_AdminReply(ent, va("print \"%2d %llu %c %s\n\"", i,
                             (unsigned long long)cl->pers.steamId, privChars[priv],
                             cl->pers.netname));
    }
}

static void Cmd_Complaint_f(gentity_t* ent) {
    gclient_t* cl = ent->client;
    gentity_t* accused;
    char arg[MAX_TOKEN_CHARS];

    if (level.time >= cl->pers.complaintEndTime || level.warmupTime != 0 ||
        !g_complaintLimit.integer) {
        cl->pers.complaintEndTime = -1;
        cl->pers.complaintClient = -1;
        return;
    }
    if (cl->pers.complaintClient < 0 || cl->pers.complaintClient >= level.maxclients) {
        return;
    }

    accused = &g_entities[cl->pers.complaintClient];
    if (!accused->client || accused->client->pers.connected != CON_CONNECTED) {
        return;
    }
    if (accused->client->sess.privileges == 2) {
        trap_SendServerCommand(ent - g_entities, "complaint -3");
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    if (arg[0] == 'y' || arg[0] == 'Y' || arg[0] == '1') {
        accused->client->pers.complaints++;
        if (!accused->client->pers.localClient &&
            accused->client->pers.complaints >= g_complaintLimit.integer) {
            G_SetAccess(accused->client->pers.steamId, -1, qtrue);
            trap_DropClient(accused - g_entities, "kicked after too many complaints.");
            trap_SendServerCommand(ent - g_entities, "complaint -1");
            return;
        }
        trap_SendServerCommand(cl->pers.complaintClient,
            va("print \"^1Warning^7: Complaint filed against you by %s.\n\"", cl->pers.netname));
        trap_SendServerCommand(ent - g_entities, "complaint -1");
    } else {
        trap_SendServerCommand(cl->pers.complaintClient,
            "print\"No complaint filed against you.\n\"");
        trap_SendServerCommand(ent - g_entities, "complaint -2");
    }

    cl->pers.complaintEndTime = -1;
    cl->pers.complaintClient = -1;
}

#define CMD_PHASE_WARMUP 1
#define CMD_PHASE_MATCH 2
#define CMD_PHASE_INTERMISSION 4

static qboolean G_CommandPhaseAllowed(gentity_t* ent, int allowed) {
    const char* reason;

    if (level.intermissionTime != 0) {
        if (allowed & CMD_PHASE_INTERMISSION) {
            return qtrue;
        }
        reason = "print \"This command can not be used during intermission\n\"";
    } else if (level.warmupTime != 0) {
        if (allowed & CMD_PHASE_WARMUP) {
            return qtrue;
        }
        reason = "print \"This command can not be used during warmup\n\"";
    } else if ((g_gametype.integer == GT_CA || g_gametype.integer == GT_FREEZE ||
                g_gametype.integer == GT_AD || g_gametype.integer == GT_RR) &&
               level.roundState.eCurrent == RS_COUNTDOWN) {
        if (allowed & CMD_PHASE_MATCH) {
            return qtrue;
        }
        reason = "print \"This command can not be used during a round countdown\n\"";
    } else {
        if (allowed & CMD_PHASE_MATCH) {
            return qtrue;
        }
        reason = "print \"This command can not be used while a match is in progress\n\"";
    }

    if (ent) {
        trap_SendServerCommand(ent - g_entities, reason);
    } else {
        G_Printf("%s", reason);
    }
    return qfalse;
}

void MP_Pause(gentity_t* ent, int duration) {
    const char* name;

    if (!G_CommandPhaseAllowed(ent, CMD_PHASE_MATCH)) {
        return;
    }
    if (level.timePauseBegin != 0) {
        return;
    }

    level.timePauseBegin = level.time;
    level.mp_unpauseTime = duration ? level.time + duration * 1000 : 0;
    level.mp_pauseClient = ent ? (int)(ent - g_entities) : -1;

    trap_SetConfigstring(CS_PAUSE_START_TIME, va("%i", level.timePauseBegin));
    trap_SetConfigstring(CS_PAUSE_END_TIME, va("%i", level.mp_unpauseTime));

    name = ent ? ent->client->pers.netname : "The server";
    if (duration) {
        trap_SendServerCommand(-1, va("pcp \"%s has called timeout (%ds)\n\"", name, duration));
    } else {
        trap_SendServerCommand(-1, va("pcp \"%s has paused the match\n\"", name));
    }
}

void MP_PauseThink(void) {
    int pauseDuration, i, j;

    if (level.mp_unpauseTime == 0 || level.mp_unpauseTime > level.time) {
        return;
    }

    pauseDuration = level.time - level.timePauseBegin;

    for (i = 0; i < level.maxclients; i++) {
        gentity_t* ent = &g_entities[i];
        gclient_t* cl;
        if (!ent->inuse || !ent->client) {
            continue;
        }
        cl = ent->client;
        if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        cl->pers.enterTime += pauseDuration;
        cl->respawnTime += pauseDuration;
        for (j = 0; j < MAX_POWERUPS; j++) {
            if (cl->ps.powerups[j] != 0 && cl->ps.powerups[j] != INT_MAX) {
                cl->ps.powerups[j] += pauseDuration;
            }
        }
        if (cl->ps.weaponTime != 0) {
            cl->ps.weaponTime += pauseDuration;
        }
    }

    level.timePauseBegin = 0;
    level.mp_unpauseTime = 0;
    trap_SetConfigstring(CS_PAUSE_START_TIME, "0");
    trap_SetConfigstring(CS_PAUSE_END_TIME, "0");
    trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
}

static void Cmd_Timeout_f(gentity_t* ent) {
    team_t team;

    if (!ent) {
        MP_Pause(NULL, g_timeoutLen.integer);
        return;
    }
    if (!ent->client) {
        return;
    }
    team = ent->client->sess.sessionTeam;
    if (team == TEAM_SPECTATOR || level.timePauseBegin != 0) {
        return;
    }
    if (g_timeoutCount.integer == 0) {
        G_AdminReply(ent, "print \"Timeouts are not enabled on this server.\n\"");
        return;
    }

    if (g_gametype.integer < GT_TEAM) {
        if (ent->client->pers.timeouts >= g_timeoutCount.integer) {
            G_AdminReply(ent, "print \"You have no timeouts left to call\n\"");
            return;
        }
        ent->client->pers.timeouts++;
    } else {
        if (level.teamTimeoutsUsed[team] >= g_timeoutCount.integer) {
            G_AdminReply(ent, "print \"Your team has no timeouts left to call\n\"");
            return;
        }
        level.teamTimeoutsUsed[team]++;
        trap_SetConfigstring(CS_TIMEOUTS_RED,
            va("%d", g_timeoutCount.integer - level.teamTimeoutsUsed[TEAM_RED]));
        trap_SetConfigstring(CS_TIMEOUTS_BLUE,
            va("%d", g_timeoutCount.integer - level.teamTimeoutsUsed[TEAM_BLUE]));
    }

    MP_Pause(ent, g_timeoutLen.integer);
}

static void Cmd_Timein_f(gentity_t* ent) {
    if (level.timePauseBegin == 0) {
        G_AdminReply(ent, "print \"No timeout is in progress.\n\"");
        return;
    }
    if (level.mp_unpauseTime != 0 && level.mp_unpauseTime <= level.time + 4999) {
        return;
    }
    if (level.mp_pauseClient >= 0 && ent && ent->client &&
        (int)(ent - g_entities) != level.mp_pauseClient &&
        ent->client->sess.privileges == 0) {
        G_AdminReply(ent, "print \"You did not call the current timeout\n\"");
        return;
    }

    level.mp_unpauseTime = level.time + 5000;
    trap_SetConfigstring(CS_PAUSE_END_TIME, va("%i", level.mp_unpauseTime));
    if (ent && ent->client) {
        trap_SendServerCommand(-1, va("pcp \"%s has called timein\n\"",
                                      ent->client->pers.netname));
    } else {
        trap_SendServerCommand(-1, "pcp \"The server has called timein\n\"");
    }
}

static void Cmd_AllReady_f(gentity_t* ent) {
    int i;

    if (!G_CommandPhaseAllowed(ent, CMD_PHASE_WARMUP)) {
        return;
    }

    if (g_gametype.integer == GT_DUEL && level.numPlayingClients != 2) {
        G_AdminReply(ent, "print \"Allready may not be used until two players are in the match.\n\"");
        return;
    }

    for (i = 0; i < level.maxclients; i++) {
        gentity_t* e = &g_entities[i];
        gclient_t* cl = &level.clients[i];

        if (e->client && cl->pers.connected == CON_CONNECTED) {
            cl->pers.ready = qtrue;
        }
    }
}

static void Cmd_Pause_f(gentity_t* ent) {
    if (level.timePauseBegin != 0) {
        int clientNum;

        if (G_AdminIsServer(ent)) {
            return;
        }
        clientNum = ent - g_entities;
        if (level.mp_pauseClient != clientNum) {
            level.mp_pauseClient = clientNum;
            trap_SendServerCommand(clientNum,
                "print \"You have taken ownership of the current timeout\n\"");
        }
        return;
    }
    MP_Pause(ent, 0);
}

static gentity_t* G_FindPlayerArg(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    int id;
    int i;

    if (trap_Argc() <= 1) {
        G_AdminReply(ent,
            "print \"Missing PlayerID (use \\players for a list)\n\"");
        return NULL;
    }

    trap_Argv(1, arg, sizeof(arg));
    id = atoi(arg);

    for (i = 0; arg[i]; i++) {
        if (arg[i] < '0' || arg[i] > '9') {
            id = -1;
            break;
        }
    }

    if (id < 0 || id >= level.maxclients || !level.clients[id].pers.connected) {
        G_AdminReply(ent,
            "print \"Invalid PlayerID (use \\players for a list)\n\"");
        return NULL;
    }
    return &g_entities[id];
}

static team_t G_ParseTeamArg(gentity_t* ent, int argIndex) {
    char arg[MAX_TOKEN_CHARS];
    int c;
    qboolean teamGame = (qboolean)(g_gametype.integer >= GT_TEAM);

    if (trap_Argc() <= argIndex) {
        G_AdminReply(ent, "print \"Missing TeamName\n\"");
        return TEAM_NUM_TEAMS;
    }

    trap_Argv(argIndex, arg, sizeof(arg));
    c = tolower(arg[0]);

    if (c == 's') {
        return TEAM_SPECTATOR;
    }
    if (c == 'r' && teamGame) {
        return TEAM_RED;
    }
    if (c == 'b' && teamGame) {
        return TEAM_BLUE;
    }
    if (c == 'f' && !teamGame) {
        return TEAM_FREE;
    }

    if (c == 'r' || c == 'b' || c == 'f') {
        G_AdminReply(ent,
            teamGame ? "print \"Invalid TeamName (use R/B/S)\n\""
                     : "print \"Invalid TeamName (use F/S)\n\"");
    } else {
        G_AdminReply(ent,
            va("print \"Team %c is not valid for this command\n\"", c ? c : ' '));
    }
    return TEAM_NUM_TEAMS;
}

static qboolean teamLocked[TEAM_NUM_TEAMS];

qboolean G_TeamIsLocked(gentity_t* ent, team_t team) {
    if (team == TEAM_SPECTATOR || team == TEAM_FREE) {
        return qfalse;
    }
    if (team < 0 || team >= TEAM_NUM_TEAMS || !teamLocked[team]) {
        return qfalse;
    }
    trap_SendServerCommand(ent - g_entities,
        va("print \"The %s team is locked!\n\"", TeamName(team)));
    return qtrue;
}

void G_ClearTeamLocks(void) {
    Com_Memset(teamLocked, 0, sizeof(teamLocked));
}

static void G_AnnounceLock(team_t team, qboolean locked) {
    trap_SendServerCommand(-1, va("print \"The %s team is now %slocked\n\"",
                                  TeamName(team), locked ? "" : "un"));
}

static void Cmd_Lock_f(gentity_t* ent) {
    team_t team;

    if (trap_Argc() < 2) {
        if (!teamLocked[TEAM_RED]) {
            teamLocked[TEAM_RED] = qtrue;
            G_AnnounceLock(TEAM_RED, qtrue);
        }
        if (!teamLocked[TEAM_BLUE]) {
            teamLocked[TEAM_BLUE] = qtrue;
            G_AnnounceLock(TEAM_BLUE, qtrue);
        }
        return;
    }

    team = G_ParseTeamArg(ent, 1);
    if (team == TEAM_NUM_TEAMS || teamLocked[team]) {
        return;
    }
    teamLocked[team] = qtrue;
    G_AnnounceLock(team, qtrue);
}

static void Cmd_Unlock_f(gentity_t* ent) {
    team_t team;

    if (trap_Argc() < 2) {
        if (teamLocked[TEAM_RED]) {
            teamLocked[TEAM_RED] = qfalse;
            G_AnnounceLock(TEAM_RED, qfalse);
        }
        if (teamLocked[TEAM_BLUE]) {
            teamLocked[TEAM_BLUE] = qfalse;
            G_AnnounceLock(TEAM_BLUE, qfalse);
        }
        return;
    }

    team = G_ParseTeamArg(ent, 1);
    if (team == TEAM_NUM_TEAMS || !teamLocked[team]) {
        return;
    }
    teamLocked[team] = qfalse;
    G_AnnounceLock(team, qfalse);
}

static void Cmd_Put_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    team_t team;

    if (!target) {
        return;
    }

    team = G_ParseTeamArg(ent, 2);
    if (team == TEAM_NUM_TEAMS) {
        return;
    }

    if (g_gametype.integer == GT_DUEL && team == TEAM_FREE && level.numPlayingClients >= 2) {
        if (level.warmupTime >= 0) {
            G_AdminReply(ent, "print \"A maximum of two players are allowed in the match.\n\"");
            return;
        }
        G_AdminReply(ent, "print \"The match cannot begin until only two players are in the match.\n\"");
    }

    if (target->client->sess.sessionTeam == team) {
        G_AdminReply(ent, va("print \"%s is already on the %s team\n\"",
                             target->client->pers.netname, TeamName(team)));
        return;
    }

    if (G_AdminIsServer(ent)) {
        trap_SendServerCommand(target - g_entities,
            va("print \"The server has moved you to the %s team\n\"", TeamName(team)));
    } else {
        trap_SendServerCommand(target - g_entities,
            va("print \"%s has moved you to the %s team\n\"",
               ent->client->pers.netname, TeamName(team)));
    }

    SetTeam_Execute(target, team, SPECTATOR_FREE, 0);
    CalculateRanks();
}

static void Cmd_Mute_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);

    if (!target) {
        return;
    }

    if (!G_AdminIsServer(ent) &&
        ent->client->sess.privileges <= target->client->sess.privileges) {
        return;
    }

    target->client->sess.muted = 1;
    trap_SendServerCommand(-1, va("print \"%s has been muted\n\"",
        target->client->pers.netname));
}

static void Cmd_Unmute_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);

    if (!target) {
        return;
    }

    target->client->sess.muted = 0;
    trap_SendServerCommand(-1, va("print \"%s has been unmuted\n\"",
        target->client->pers.netname));
}

static void Cmd_Ban_f(gentity_t* ent) {
    char cmd[MAX_TOKEN_CHARS];
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;

    if (g_clients[pid].sess.privileges > 0) {
        G_AdminReply(ent, "print \"Can not kick admins.\n\"");
        return;
    }

    trap_Argv(0, cmd, sizeof(cmd));
    G_SetAccess(g_clients[pid].pers.steamId, -1, (qboolean)(Q_stricmp(cmd, "ban") != 0));

    trap_DropClient(pid, "was kicked");
}

static void Cmd_Unban_f(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    uint64_t steamId;

    if (trap_Argc() < 2) {
        return;
    }
    trap_Argv(1, arg, sizeof(arg));
    steamId = (uint64_t)strtoll(arg, NULL, 10);
    if (!steamId || G_GetAccessBySteamId(steamId) != -1) {
        return;
    }
    G_SetAccess(steamId, 0, qfalse);
    G_AdminReply(ent, va("print \"%llu has been unbanned\n\"", (unsigned long long)steamId));
}

static void Cmd_ListAccess_f(gentity_t* ent) {
    static const char* levels[4] = {"      ", "MOD   ", "ADMIN ", "ROOT  "};
    char arg[MAX_TOKEN_CHARS];
    int total = G_AccessCount();
    int pages = total / 20;
    int page = 0;
    int i;

    if (trap_Argc() > 1) {
        trap_Argv(1, arg, sizeof(arg));
        page = atoi(arg);
    }
    if (page < 0) {
        page = 0;
    } else if (page > pages) {
        page = pages;
    }

    G_AdminReply(ent,
        va("print \"Access List: Page %i of %i\n\"", page + 1, pages + 1));
    G_AdminReply(ent,
        "print \"=============================\n\"");

    for (i = page * 20; i < total && i < (page + 1) * 20; i++) {
        uint64_t steamId = 0;
        int level = 0;
        qboolean temporary = qfalse;

        if (!G_AccessAt(i, &steamId, &level, &temporary)) {
            break;
        }
        G_AdminReply(ent,
            va("print \"%llu %s %s\n\"", (unsigned long long)steamId,
               (level >= 0 && level < 4) ? levels[level] : "BAN   ",
               temporary ? "TEMP" : "PERM"));
    }
}

static void Cmd_OpSay_f(gentity_t* ent) {
    char* msg;

    if (trap_Argc() < 2) {
        return;
    }
    msg = ConcatArgs(1);
    trap_SendServerCommand(-1, va("print \"<@%s^7> %s\n\"",
        G_AdminIsServer(ent) ? "server" : ent->client->pers.netname, msg));
}

static void Cmd_AddAdmin_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;
    if (g_clients[pid].sess.privileges >= 2) {
        return;
    }

    g_clients[pid].sess.privileges = 2;
    G_SetAccess(g_clients[pid].pers.steamId, 2, qfalse);
    trap_SendServerCommand(-1, va("print \"%s has become an administrator\n\"",
        g_clients[pid].pers.netname));
    trap_SendServerCommand(pid, va("priv %i", g_clients[pid].sess.privileges));
}

static void Cmd_AddMod_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;
    if (g_clients[pid].sess.privileges >= 1) {
        return;
    }

    g_clients[pid].sess.privileges = 1;
    G_SetAccess(g_clients[pid].pers.steamId, 1, qfalse);
    trap_SendServerCommand(-1, va("print \"%s has become a moderator\n\"",
        g_clients[pid].pers.netname));
    trap_SendServerCommand(pid, va("priv %i", g_clients[pid].sess.privileges));
}

static void Cmd_Demote_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;
    if (g_clients[pid].sess.privileges == 0) {
        return;
    }

    if (!G_AdminIsServer(ent) && ent->client->sess.privileges <= g_clients[pid].sess.privileges) {
        G_AdminReply(ent, "print \"Can not demote someone at or above your level.\n\"");
        return;
    }
    if (g_clients[pid].sess.privileges < 0) {
        return;
    }

    g_clients[pid].sess.privileges = 0;
    G_SetAccess(g_clients[pid].pers.steamId, 0, qfalse);
    trap_SendServerCommand(-1, va("print \"%s has had their privileges removed\n\"",
        g_clients[pid].pers.netname));
    trap_SendServerCommand(pid, va("priv %i", g_clients[pid].sess.privileges));
}

static void Cmd_Abort_f(gentity_t* ent) {
    if (level.warmupTime != 0) {
        return;
    }

    if (level.timePauseBegin) {
        G_AdminReply(ent, "print \"Match is currently paused. You must unpause before aborting the match.\n\"");
        return;
    }

    if (G_AdminIsServer(ent)) {
    trap_SendServerCommand(-1, "pcp \"The server has aborted the match\n\"");
    } else {
    trap_SendServerCommand(-1, va("pcp \"%s has aborted the match\n\"",
        ent->client->pers.netname));
    }
    trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
}

static void Cmd_AddScore_f(gentity_t* ent) {
    char scoreStr[MAX_TOKEN_CHARS];
    gentity_t* target = G_FindPlayerArg(ent);
    int score;

    if (!target) {
        return;
    }
    trap_Argv(2, scoreStr, sizeof(scoreStr));
    score = atoi(scoreStr);

    target->client->ps.persistant[PERS_SCORE] += score;
    CalculateRanks();
    trap_SendServerCommand(-1, "pcp \"Player score adjusted.\n\"");
    trap_SendServerCommand(-1, va("print \"%s has had their score %screased by %i.\n\"",
        target->client->pers.netname, score < 0 ? "de" : "in", abs(score)));
}

static void Cmd_AddTeamScore_f(gentity_t* ent) {
    char scoreStr[MAX_TOKEN_CHARS];
    team_t team = G_ParseTeamArg(ent, 1);

    if (team != TEAM_RED && team != TEAM_BLUE) {
        return;
    }
    trap_Argv(2, scoreStr, sizeof(scoreStr));
    level.teamScores[team] += atoi(scoreStr);
    CalculateRanks();
    trap_SendServerCommand(-1, "pcp \"Team score adjusted.\n\"");
}

static void Cmd_SetMatchTime_f(gentity_t* ent) {
    char timeStr[MAX_TOKEN_CHARS];
    int seconds;

    if (trap_Argc() < 2) {
        return;
    }

    trap_Argv(1, timeStr, sizeof(timeStr));
    seconds = atoi(timeStr);

    level.startTime = level.time - (seconds * 1000);
    trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
    trap_SendServerCommand(-1, va("pcp \"Match time has been set to %s.\n\"", timeStr));
}

typedef struct {
    const char* cmd;
    int requiredLevel;
    void (*handler)(gentity_t* ent);
    const char* description;
} permissionCmd_t;

static permissionCmd_t permissionCmds[] = {
    { "?",            0, Cmd_PermHelp_f,    "" },
    { "players",      0, Cmd_Players_f,     "^3 show currently connected players and their status" },
    { "timeout",      0, Cmd_Timeout_f,     "^3 call a time out, temporarily pausing the game" },
    { "timein",       0, Cmd_Timein_f,      "^3 call a time in, unpausing the game" },
    { "allready",     1, Cmd_AllReady_f,    "^3 force all players to 'ready' and start the match" },
    { "pause",        1, Cmd_Pause_f,       "^3 pause the current match indefinitely" },
    { "unpause",      1, Cmd_Timein_f,      "^3 unpause the current match" },
    { "lock",         1, Cmd_Lock_f,        " [team]^3 stops players from joining the team" },
    { "unlock",       1, Cmd_Unlock_f,      " [team]^3 allows players to join the team" },
    { "put",          1, Cmd_Put_f,         " <playerid> <team>^3 move a player to a team, use (R/B/S) or (F/S)" },
    { "mute",         1, Cmd_Mute_f,        " <playerid>^3 temporarily mutes a player on the server" },
    { "unmute",       1, Cmd_Unmute_f,      " <playerid>^3 restores a muted player's ability to chat" },
    { "tempban",      1, Cmd_Ban_f,         " <playerid>^3 temporarily kicks a player from the server for the match" },
    { "ban",          1, Cmd_Ban_f,         " <playerid>^3 permenantly bans a player from the server" },
    { "listaccess",   1, Cmd_ListAccess_f,  " <page>^3 lists the accounts with access " },
    { "unban",        1, Cmd_Unban_f,       " <account>^3 removes a player from the banlist" },
    { "opsay",        1, Cmd_OpSay_f,       " <message> ^3send a message to everyone on the server" },
    { "addadmin",     2, Cmd_AddAdmin_f,    " <playerid>^3 give a player admin privileges" },
    { "addmod",       2, Cmd_AddMod_f,      " <playerid>^3 give a player moderator privileges" },
    { "demote",       2, Cmd_Demote_f,      " <playerid>^3 remove a player's privileges" },
    { "abort",        1, Cmd_Abort_f,       "^3 abandon the current game and go back to warmup" },
    { "addscore",     1, Cmd_AddScore_f,    " <playerid> <score>^3 add points to a player's score" },
    { "addteamscore", 1, Cmd_AddTeamScore_f, " <team> <score>^3 add points to a team's score, use (R/B)" },
    { "setmatchtime", 1, Cmd_SetMatchTime_f, " <time> ^3set match time in seconds" },
    { "rcon",         2, NULL,              " <command> ^3run command on server" },
    { NULL,           0, NULL,              NULL }
};

static void Cmd_PermHelp_f(gentity_t* ent) {
    int i;
    int privLevel;

    privLevel = G_AdminIsServer(ent) ? 3 : ent->client->sess.privileges;

    for (i = 0; permissionCmds[i].cmd; i++) {
        if (permissionCmds[i].requiredLevel > privLevel) continue;
        G_AdminReply(ent, va("print \"%s%s\n\"",
                             permissionCmds[i].cmd,
                             permissionCmds[i].description ? permissionCmds[i].description : ""));
    }
}

static qboolean ClientCommandPermissionCheck(gentity_t* ent, const char* cmd) {
    int i;
    int privLevel;

    for (i = 0; permissionCmds[i].cmd; i++) {
        if (Q_stricmp(cmd, permissionCmds[i].cmd) == 0) {
            break;
        }
    }

    if (!permissionCmds[i].cmd) {
        return qfalse;
    }

    if (!permissionCmds[i].handler) {
        return qfalse;
    }

    if (!ent) {
        privLevel = 3;
    } else {
        privLevel = ent->client->sess.privileges;
        if (ent->client->pers.localClient) privLevel = 2;
    }

    if (privLevel < permissionCmds[i].requiredLevel) {
        G_AdminReply(ent, "print \"You do not have the privileges required to use this command\n\"");
        return qtrue;
    }

    permissionCmds[i].handler(ent);
    return qtrue;
}

qboolean G_ConsolePermissionCommand(const char* cmd) {
    return ClientCommandPermissionCheck(NULL, cmd);
}

const char* G_GametypeDisplayName(void) {
    if (g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE) {
        return "Unknown Gametype";
    }
    return gametypeDisplayNames[g_gametype.integer];
}

static void Cmd_DropFlag_f(gentity_t* ent) {
    vec3_t forward, velocity;

    if (!(g_dropCmds.integer & 1)) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropFlag is not enabled on this server.\n\"");
        return;
    }

    if (g_gametype.integer != GT_CTF && g_gametype.integer != GT_1FCTF &&
        g_gametype.integer != GT_AD) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropFlag is not available in non-flag gametypes.\n\"");
        return;
    }

    if (level.timePauseBegin) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropFlag is not available during a pause or timeout.\n\"");
        return;
    }

    if (g_throwFlagVelocity.integer) {
        AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
        VectorScale(forward, g_throwFlagForwardMult.value, forward);
        VectorMA(vec3_origin, g_throwFlagVelocity.value, forward, velocity);
        G_ThrowFlag(ent, velocity, ent->client->ps.origin);
    } else {
        G_ThrowFlag(ent, NULL, NULL);
    }
}

static void Cmd_DropPowerup_f(gentity_t* ent) {
    static vec3_t mins = { -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS };
    static vec3_t maxs = { ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS };
    gclient_t* client = ent->client;
    vec3_t angles, forward, end;
    gentity_t* drop;
    gitem_t* item;
    trace_t tr;
    int i;

    if (!(g_dropCmds.integer & 2)) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropPowerup is not enabled on this server.\n\"");
        return;
    }

    if (g_gametype.integer < GT_TEAM) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropPowerup is not available in non-team gametypes.\n\"");
        return;
    }

    if (g_gametype.integer == GT_CA || g_gametype.integer == GT_DOMINATION ||
        g_gametype.integer == GT_AD || g_gametype.integer == GT_RR) {
        trap_SendServerCommand(ent - g_entities,
            va("print \"DropPowerup is not available in %s.\n\"", G_GametypeDisplayName()));
        return;
    }

    if (level.timePauseBegin) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropPowerup is not available during a pause or timeout.\n\"");
        return;
    }

    if (g_instaGib.integer) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropPowerup is not available in InstaGib.\n\"");
        return;
    }

    if (!client || client->ps.pm_type == PM_SPECTATOR) {
        return;
    }

    VectorSet(angles, 0, client->ps.viewangles[YAW], 0);
    AngleVectors(angles, forward, NULL, NULL);
    VectorMA(client->ps.origin, 64, forward, end);
    trap_Trace(&tr, client->ps.origin, mins, maxs, end, ENTITYNUM_NONE, CONTENTS_SOLID);
    if (tr.fraction != 1.0f) {
        return;
    }

    for (i = 1; i < PW_NUM_POWERUPS; i++) {
        if (client->ps.powerups[i] <= level.time || i == PW_FLIGHT || i == PW_FREEZE) {
            continue;
        }
        item = BG_FindItemForPowerup((powerup_t)i);
        if (!item || item->giType == IT_PERSISTANT_POWERUP ||
            (item->giType == IT_TEAM && item->giTag >= PW_REDFLAG && item->giTag <= PW_NEUTRALFLAG)) {
            continue;
        }
        drop = LaunchItem(item, tr.endpos, vec3_origin);
        drop->count = (client->ps.powerups[i] - level.time) / 1000;
        drop->s.clientNum = ent->s.clientNum;
        drop->s.time = level.time;
        if (g_droppedPowerupsDecay.integer) {
            drop->flags |= FL_DROPPED_ITEM_TIMED;
            drop->nextthink = MIN(client->ps.powerups[i], level.time + 10000);
        } else {
            drop->nextthink = level.time + 10000;
        }
        client->ps.powerups[i] = 0;
        if (drop->count < 1) {
            drop->count = 1;
        }
    }
}

static void Cmd_DropRune_f(gentity_t* ent) {
    static vec3_t mins = { -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS };
    static vec3_t maxs = { ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS };
    gclient_t* client = ent->client;
    vec3_t angles, forward, end;
    gentity_t* drop;
    gitem_t* item;
    trace_t tr;
    int i;

    if (!g_runes.integer) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Runes are not enabled on this server.\n\"");
        return;
    }

    if (g_gametype.integer == GT_RACE) {
        trap_SendServerCommand(ent - g_entities,
            va("print \"DropRune is not available in %s.\n\"", G_GametypeDisplayName()));
        return;
    }

    if (level.timePauseBegin) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropRune is not available during a pause or timeout.\n\"");
        return;
    }

    if (!client || client->ps.pm_type == PM_SPECTATOR) {
        return;
    }

    VectorSet(angles, 0, client->ps.viewangles[YAW], 0);
    AngleVectors(angles, forward, NULL, NULL);
    VectorMA(client->ps.origin, 64, forward, end);
    trap_Trace(&tr, client->ps.origin, mins, maxs, end, ENTITYNUM_NONE, CONTENTS_SOLID);
    if (tr.fraction != 1.0f) {
        return;
    }

    for (i = 1; i < PW_NUM_POWERUPS; i++) {
        if (client->ps.powerups[i] <= level.time || i == PW_FLIGHT || i == PW_FREEZE) {
            continue;
        }
        item = BG_FindItemForPowerup((powerup_t)i);
        if (!item || item->giType != IT_PERSISTANT_POWERUP) {
            continue;
        }
        drop = LaunchItem(item, tr.endpos, vec3_origin);
        drop->count = (client->ps.powerups[i] - level.time) / 1000;
        drop->s.clientNum = ent->s.clientNum;
        drop->s.time = level.time;
        drop->nextthink = level.time + 10000;
        client->ps.powerups[i] = 0;
        client->persistantPowerup = NULL;
        client->ps.stats[STAT_RUNE] = 0;
        if (drop->count < 1) {
            drop->count = 1;
        }
    }
}

static void Cmd_DropWeapon_f(gentity_t* ent) {
    static vec3_t mins = { -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS };
    static vec3_t maxs = { ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS };
    gclient_t* client = ent->client;
    vec3_t angles, forward, end;
    gentity_t* drop;
    gitem_t* item;
    trace_t tr;
    int weapon, amount, next;

    if (!(g_dropCmds.integer & 4)) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropWeapon is not enabled on this server.\n\"");
        return;
    }

    if (g_gametype.integer < GT_TEAM) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropWeapon is not available in non-team gametypes.\n\"");
        return;
    }

    if (g_gametype.integer == GT_CA || g_gametype.integer == GT_DOMINATION ||
        g_gametype.integer == GT_AD || g_gametype.integer == GT_RR) {
        trap_SendServerCommand(ent - g_entities,
            va("print \"DropWeapon is not available in %s.\n\"", G_GametypeDisplayName()));
        return;
    }

    if (level.timePauseBegin) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropWeapon is not available during a pause or timeout.\n\"");
        return;
    }

    if (g_instaGib.integer) {
        trap_SendServerCommand(ent - g_entities,
            "print \"DropWeapon is not available in InstaGib.\n\"");
        return;
    }

    if (!client || client->ps.pm_type == PM_SPECTATOR) {
        return;
    }
    weapon = ent->s.weapon;
    if (weapon <= WP_MACHINEGUN || weapon > WP_HMG || client->ps.ammo[weapon] <= 0 ||
        !(client->ps.stats[STAT_WEAPONS] & (1 << weapon))) {
        return;
    }

    VectorSet(angles, 0, client->ps.viewangles[YAW], 0);
    AngleVectors(angles, forward, NULL, NULL);
    VectorMA(client->ps.origin, 64, forward, end);
    trap_Trace(&tr, client->ps.origin, mins, maxs, end, ENTITYNUM_NONE, CONTENTS_SOLID);
    if (tr.fraction != 1.0f) {
        return;
    }

    item = BG_FindItemForWeapon((weapon_t)weapon);
    amount = MIN(client->ps.ammo[weapon], item->quantity);
    for (next = item->giTag - 1; next > 0; next--) {
        if (client->ps.stats[STAT_WEAPONS] & (1 << next)) {
            break;
        }
    }

    client->ps.ammo[weapon] -= amount;
    client->ps.stats[STAT_WEAPONS] &= ~(1 << item->giTag);
    client->ps.weapon = next;

    drop = LaunchItem(item, tr.endpos, vec3_origin);
    drop->count = amount;
    drop->nextthink = level.time + 10000;
    drop->s.clientNum = ent->s.clientNum;
    drop->s.time = level.time;

    BG_AddPredictableEventToPlayerstate(EV_DROP_WEAPON, weapon, &client->ps);
}

void ClientCommand(int clientNum) {
    gentity_t* ent;
    char cmd[MAX_TOKEN_CHARS];

    ent = g_entities + clientNum;
    if (!ent->client || ent->client->pers.connected != CON_CONNECTED) {
        if (ent->client && ent->client->pers.localClient) {
            trap_Argv(0, cmd, sizeof(cmd));
            if (Q_stricmp(cmd, "team") == 0) {
                Cmd_Team_f(ent);
            }
        }
        return;
    }

    trap_Argv(0, cmd, sizeof(cmd));

    if (ClientCommandPermissionCheck(ent, cmd)) {
        return;
    }

    if (Q_stricmp(cmd, "say") == 0) {
        Cmd_Say_f(ent, SAY_ALL, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "say_team") == 0) {
        Cmd_Say_f(ent, SAY_TEAM, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "tell") == 0) {
        Cmd_Tell_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "botsay") == 0) {
        Cmd_BotSay_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "vsay") == 0) {
        Cmd_Voice_f();
        return;
    }
    if (Q_stricmp(cmd, "vsay_team") == 0) {
        Cmd_Voice_f();
        return;
    }
    if (Q_stricmp(cmd, "vtell") == 0) {
        Cmd_VoiceTell_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "vosay") == 0) {
        Cmd_Voice_f();
        return;
    }
    if (Q_stricmp(cmd, "vosay_team") == 0) {
        Cmd_Voice_f();
        return;
    }
    if (Q_stricmp(cmd, "votell") == 0) {
        Cmd_VoiceTell_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "vtaunt") == 0) {
        Cmd_VoiceTaunt_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "score") == 0) {
        Cmd_Score_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "acc") == 0) {
        Cmd_Score_f(ent);
        Cmd_Acc_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "pstats") == 0) {
        Cmd_PStats_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "readyup") == 0) {
        Cmd_ReadyUp_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "ragequit") == 0) {
        trap_DropClient(clientNum, "ragequit");
        return;
    }
    if (Q_stricmp(cmd, "vote") == 0) {
        Cmd_Vote_f(ent);
        return;
    }

    if (level.intermissionQueued || level.intermissionTime) {
        return;
    }

    if (Q_stricmp(cmd, "give") == 0)
        Cmd_Give_f(ent);
    else if (Q_stricmp(cmd, "god") == 0)
        Cmd_God_f(ent);
    else if (Q_stricmp(cmd, "notarget") == 0)
        Cmd_Notarget_f(ent);
    else if (Q_stricmp(cmd, "noclip") == 0)
        Cmd_Noclip_f(ent);
    else if (Q_stricmp(cmd, "kill") == 0)
        Cmd_Kill_f(ent);
    else if (Q_stricmp(cmd, "teamtask") == 0)
        Cmd_TeamTask_f(ent);
    else if (Q_stricmp(cmd, "levelshot") == 0)
        Cmd_LevelShot_f(ent);
    else if (Q_stricmp(cmd, "follow") == 0)
        Cmd_Follow_f(ent);
    else if (Q_stricmp(cmd, "follownext") == 0)
        Cmd_FollowCycle_f(ent, 1);
    else if (Q_stricmp(cmd, "followprev") == 0)
        Cmd_FollowCycle_f(ent, -1);
    else if (Q_stricmp(cmd, "team") == 0)
        Cmd_Team_f(ent);
    else if (Q_stricmp(cmd, "where") == 0)
        Cmd_Where_f(ent);
    else if (Q_stricmp(cmd, "complaint") == 0)
        Cmd_Complaint_f(ent);
    else if (Q_stricmp(cmd, "callvote") == 0 || Q_stricmp(cmd, "cv") == 0)
        Cmd_CallVote_f(ent);
    else if (Q_stricmp(cmd, "gc") == 0)
        Cmd_GameCommand_f(ent);
    else if (Q_stricmp(cmd, "setviewpos") == 0)
        Cmd_SetViewpos_f(ent);
    else if (Q_stricmp(cmd, "dropflag") == 0)
        Cmd_DropFlag_f(ent);
    else if (Q_stricmp(cmd, "droppowerup") == 0)
        Cmd_DropPowerup_f(ent);
    else if (Q_stricmp(cmd, "droprune") == 0)
        Cmd_DropRune_f(ent);
    else if (Q_stricmp(cmd, "dropweapon") == 0)
        Cmd_DropWeapon_f(ent);
    else if (Q_stricmp(cmd, "spec") == 0)
        Cmd_Spec_f(ent);
    else if (Q_stricmp(cmd, "forfeit") == 0)
        Cmd_Forfeit_f(ent);
    else if (Q_stricmp(cmd, "specresp") == 0)
        Cmd_SpecResp_f(ent);
    else if (Q_stricmp(cmd, "racepoint") == 0)
        Cmd_RacePoint_f(ent);
    else if (Q_stricmp(cmd, "raceinit") == 0)
        Cmd_RaceInit_f(ent);
    else
        trap_SendServerCommand(clientNum, va("print \"unknown cmd %s\n\"", cmd));
}
