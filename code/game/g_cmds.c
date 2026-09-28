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
//
#include "g_local.h"

#include "../../ui/menudef.h"  // for the voice chats

// FreeForAllScoreboardMessage moved to g_gametype_ffa.c

/*
==================
Cmd_Score_f

Request current scoreboard information.

[QL] Q3 sent the FFA scoreboard for every gametype. QL dispatches to a
per-gametype scoreboard emitter and, at intermission, also broadcasts the
extended per-client team-stats rows that feed cgame
CG_ParseTeamStats_TDM/CA/CTF: TeamDeathmatchStatisticsMessage -> "tdmstats" (TEAM + FREEZE),
ClanArenaStatisticsMessage -> "castats" (CA), CaptureTheFlagStatisticsMessage -> "ctfstats"
(CTF/1FCTF/HARVESTER/DOMINATION/AD). The Ghidra project mislabelled the last two
(CTF_impl was tagged the castats fn, CTF's ctfstats fn was tagged FT_impl); corrected
here and in the project to match what each emits.

// Address: 0x1003f690 (Cmd_Score_f) -> 0x1003cde0 (Cmd_Score_impl)
==================
*/
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
            // GT_OBELISK (7) has no dedicated scoreboard emitter in QL.
            break;
    }

    // QL only sends the extended per-client team stats at intermission.
    if (level.intermissionTime) {
        switch (g_gametype.integer) {
            case GT_TEAM:
            case GT_FREEZE:
                TeamDeathmatchStatisticsMessage(ent);   // "tdmstats"
                break;
            case GT_CA:
                ClanArenaStatisticsMessage(ent);    // "castats"
                break;
            case GT_CTF:
            case GT_1FCTF:
            case GT_HARVESTER:
            case GT_DOMINATION:
            case GT_AD:
                CaptureTheFlagStatisticsMessage(ent);   // "ctfstats"
                break;
            default:
                break;
        }
    }
}

/*
==================
CheatsOk
==================
*/
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

/*
==================
ConcatArgs
==================
*/
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

/*
==================
StringIsInteger
==================
*/
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

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString(gentity_t* to, char* s, qboolean checkNums, qboolean checkNames) {
    gclient_t* cl;
    int idnum;
    char cleanName[MAX_STRING_CHARS];

    // [QL] 0x1003f920: retail branches on the first character -- a digit means a slot
    // number, and the name search is skipped -- and every failure has its own message.
    // Falling through to the name search answered "User 5 is not on the server" when
    // the real problem was that slot 5 was empty.
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
        // check for a name match
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

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
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
        // [QL] 0x1003fcca: in the entity too
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

    // spawn a specific item right on the player
    if (!give_all) {
        it = BG_FindItem(name);
        if (!it) {
            return;
        }

        it_ent = G_Spawn();
        VectorCopy(ent->r.currentOrigin, it_ent->s.origin);
        it_ent->classname = it->classname;
        G_SpawnItem(it_ent, it, qtrue);  // [QL] forced (qagamex86.dll 0x1003fdbf)
        FinishSpawningItem(it_ent);
        memset(&trace, 0, sizeof(trace));
        Touch_Item(it_ent, ent, &trace);
        if (it_ent->inuse) {
            G_FreeEntity(it_ent);
        }
    }
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
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

/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
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

/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
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

/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f(gentity_t* ent) {
    // [QL] 0x10040010: four gates, in this order -- cheats, alive, gametype, local
    // client. The middle two were missing, and the last one said more than retail does:
    // the binary prints only "Must be executed by a local client".
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

/*
==================
Cmd_TeamTask_f
==================
*/
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

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f(gentity_t* ent) {
    if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }
    if (ent->health <= 0) {
        return;
    }
    // [QL] Cmd_Kill_f (binary 0x100403b0): /kill is gated on the g_allowKill cvar
    // (DAT_105a136c). The binary only tests it for zero (on/off) - despite the
    // "1000" default that looks like a millisecond value there is no cooldown,
    // rate limit or timestamp field read/written here. When disabled it prints the
    // message and returns without killing.
    if (!g_allowKill.integer) {
        trap_SendServerCommand(ent - g_entities,
                               "print \"Kill is not enabled on this server.\n\"");
        return;
    }

    // [QL] binary does NOT clear FL_GODMODE here (Q3 did). It forces -999 into
    // s.health (networked entityState), ent->health and ps.stats[STAT_HEALTH],
    // then calls player_die.
    ent->s.health = -999;
    ent->health = -999;
    ent->client->ps.stats[STAT_HEALTH] = -999;
    player_die(ent, ent, ent, 100000, MOD_SUICIDE);
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gclient_t* client, int oldTeam) {
    // [ioquakelive] The binary returns early in two cases (0x1004027d): during training,
    // where nothing is announced in the Crash Course, and in the first five seconds of a
    // level, when everyone connects at once.
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

/*
=================
SetTeam
=================
*/
/*
=================
SetTeam_SpectateOnly

[QL] The block SetTeam repeats in each of its three spectate branches (0x1004073d,
0x100407dd, 0x10040c10): in Duel, choosing to spectate marks the client spectate-only
so the play queue stops pulling them in, and tells them so with a centerprint.
=================
*/
static void SetTeam_SpectateOnly(gentity_t* ent) {
    if (g_gametype.integer != GT_DUEL) {
        return;
    }
    ent->client->sess.specOnly = 1;   // gclient_t+0x36c
    ent->client->sess.playQueue = 0;  // gclient_t+0x370
    trap_SendServerCommand(ent - g_entities, va("cp \"%s\n\"", "You are set to spectate only"));
}

/*
=================
G_TeamsBalanced

[QL] 0x10068100. With g_teamForceBalance the teams are off balance when one has two players
more than the other.
=================
*/
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

/*
=================
SetTeam

[QL] 0x100406d0. follow1, follow2, spectator (s) or a team. In the team modes the team asked for
is only honoured before the match; once it is on, PickTeam decides. A full team, and with
g_teamForceBalance off balance teams, turn the player away, and so does switching sides when
the teams are even. Duel takes the next in line only; a locked team nobody.
=================
*/
void SetTeam(gentity_t* ent, const char* s) {
    gclient_t* client = ent->client;
    int clientNum = client - level.clients;
    int oldTeam = client->sess.sessionTeam;
    spectatorState_t specState = SPECTATOR_NOT;
    int specClient = 0;
    int team;

    // [ioquakelive] NOT FROM RETAIL, added for this build. Training is not left by going
    // spectator: you finish the lap or skip it with readyup, which is what Crash says.
    // The binary has no such rule -- SetTeam (0x100406d0) never looks at g_training.
    if (g_training.integer && !(ent->r.svFlags & SVF_BOT) && s &&
        (!Q_stricmp(s, "spectator") || !Q_stricmp(s, "s") || !Q_stricmp(s, "follow1") ||
         !Q_stricmp(s, "follow2"))) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You cannot spectate during training. Finish the tour, or press ready to skip it.\n\"");
        return;
    }

    if (s && !Q_stricmp(s, "follow1")) {
        // [QL] 0x10040729: follow the leading player only if there is somebody playing,
        // free-fly otherwise. In Duel, asking to spectate also marks the client
        // spectate-only and takes them out of the play queue.
        team = TEAM_SPECTATOR;
        SetTeam_SpectateOnly(ent);
        if (level.numPlayingClients < 1) {
            specState = SPECTATOR_FREE;
        } else {
            specState = SPECTATOR_FOLLOW;
            specClient = -1;
        }
    } else if (s && !Q_stricmp(s, "follow2")) {
        // [QL] 0x100407c9: the second-place slot needs two players; with exactly one
        // it falls back to following the leader, with none it free-flies.
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
        // [QL] 0x10040883: plain spectate is always free-fly, and outside Duel it goes straight
        // to the change
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FREE;
        if (g_gametype.integer != GT_DUEL) {
            goto change;
        }
        SetTeam_SpectateOnly(ent);
    } else if (g_gametype.integer == GT_RR) {
        // [QL] 0x100408b5: Red Rover picks the side itself
        team = g_rrInfected.integer ? PickTeam_RoundAware(clientNum) : PickTeam(clientNum);
    } else if (g_gametype.integer >= GT_TEAM) {
        int counts[TEAM_NUM_TEAMS];

        counts[TEAM_FREE] = counts[TEAM_SPECTATOR] = 0;
        counts[TEAM_BLUE] = TeamCount(clientNum, TEAM_BLUE);
        counts[TEAM_RED] = TeamCount(clientNum, TEAM_RED);

        // [QL] 0x10040921: the side asked for counts before the match only
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
            // [QL] 0x10040a15: off balance, nobody joins the bigger side
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
            // [QL] 0x10040aca: even teams, nobody goes over to the other side
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
        // [QL] 0x10040b6a: a full arena turns a free for all player away
        if (g_gametype.integer == GT_FFA && g_teamsize.integer > 0 &&
            g_teamsize.integer <= level.numNonSpectatorClients) {
            trap_SendServerCommand(clientNum,
                va("print \"^1Arena is full. Team size is currently set to %d.^7\n\"", g_teamsize.integer));
            return;
        }
        team = TEAM_FREE;
        // [QL] 0x10040bd7: asking to play a duel puts the client in the play queue
        if (g_gametype.integer == GT_DUEL) {
            client->sess.specOnly = 0;
            trap_SendServerCommand(clientNum, va("cp \"%s\n\"", "You are in the queue to play"));
        }
    }

    // [QL] 0x10040c7c: a duel only takes the next in line (G_NextDuelPlayer), and only
    // while a spot is free; everyone else waits in the queue as a spectator, and a player
    // already in the match asking for the same team changes nothing
    if (g_gametype.integer == GT_DUEL) {
        if (level.numNonSpectatorClients > 1 || G_NextDuelPlayer() != client) {
            if (team == oldTeam && team != TEAM_SPECTATOR) {
                return;
            }
            team = TEAM_SPECTATOR;
        }
    }

change:
    // [QL] 0x10040cb5: only a change, or a spectator again; never into a locked team
    if ((team != oldTeam || team == TEAM_SPECTATOR) && !G_TeamIsLocked(ent, team)) {
        SetTeam_Execute(ent, team, specState, specClient);
        CalculateRanks();
    }
}

/*
===============
SetTeam_Execute

[QL] 0x10040440. The team change itself, once the team has been decided. SetTeam
calls it after parsing the text, and the team shuffle calls it directly with the
team each player is due.
===============
*/
void SetTeam_Execute(gentity_t* ent, int team, spectatorState_t specState, int specClient) {
    gclient_t* client = ent->client;
    int clientNum = client - level.clients;
    int oldTeam;
    int teamLeader;

    // [QL] 0x10040cc8: a locked team takes nobody
    if (G_TeamIsLocked(ent, (team_t)team)) {
        return;
    }

    oldTeam = client->sess.sessionTeam;
    if (team == oldTeam && team != TEAM_SPECTATOR) {
        return;
    }

    //
    // execute the team change
    //

    // if the player was dead leave the body, but only if they're actually in game
    if (client->ps.stats[STAT_HEALTH] <= 0 && client->pers.connected == CON_CONNECTED) {
        CopyToBodyQue(ent);
    }

    // he starts at 'base'
    client->pers.teamState.state = TEAM_BEGIN;
    if (oldTeam != TEAM_SPECTATOR) {
        // Kill him (makes sure he loses flags, etc)
        ent->flags &= ~FL_GODMODE;
        ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
        player_die(ent, ent, ent, 100000, MOD_SUICIDE);
    }

    // they go to the end of the line for tournements
    if (team == TEAM_SPECTATOR && oldTeam != team)
        AddTournamentQueue(client);

    // [QL] 0x10040535: leaving a team closes the player's stats sheet; in Red Rover only
    // when going to spectator
    if (g_gametype.integer != GT_RR || team == TEAM_SPECTATOR) {
        STAT_PlayerStats(client, qfalse, qtrue);
    }

    client->sess.sessionTeam = team;
    client->sess.spectatorState = specState;
    client->sess.spectatorClient = specClient;

    // [QL] 0x10040567: a new spectator gets the item timers
    if (team == TEAM_SPECTATOR) {
        G_SendItemTimersToSpectator(clientNum);
    }

    client->sess.teamLeader = qfalse;
    if (team == TEAM_RED || team == TEAM_BLUE) {
        teamLeader = TeamLeader(team);
        // if there is no team leader or the team leader is a bot and this client is not a bot
        if (teamLeader == -1 || (!(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT))) {
            SetLeader(team, clientNum);
        }
    }
    // make sure there is a team leader on the team the player came from
    if (oldTeam == TEAM_RED || oldTeam == TEAM_BLUE) {
        CheckTeamLeader(oldTeam);
    }

    // get and distribute relevant parameters
    ClientUserinfoChanged(clientNum);

    // client hasn't spawned yet, they sent an early team command, teampref userinfo, or g_teamAutoJoin is enabled
    if (client->pers.connected != CON_CONNECTED) {
        return;
    }

    BroadcastTeamChange(client, oldTeam);

    ClientBegin(clientNum);

    // [QL] 0x100406b8: the last thing SetTeam does is the PLAYER_SWITCHTEAM event
    STAT_PlayerSwitchTeam(ent, oldTeam, client->sess.sessionTeam);
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing(gentity_t* ent) {
    // [QL] qagamex86.dll 0x10040d10, transcribed in full. Quake 3's version was kept
    // verbatim here and differs in a way that matters: it FORCED
    // sess.sessionTeam = TEAM_SPECTATOR. Retail never touches the session team - it
    // puts the player's own team back into persistant[PERS_TEAM], which the follow
    // had overwritten with the followed player's. With the Quake 3 version, a Clan
    // Arena player who died, followed a teammate and then stopped following was
    // silently taken off their team.
    //
    // Retail also does not clear SVF_BOT, re-apply the view angles or patch up a dead
    // player's health here; all three were Quake 3 and are gone.
    ent->client->ps.persistant[PERS_TEAM] = ent->client->sess.sessionTeam;  // 0x10040d1c
    ent->client->ps.pm_type = PM_SPECTATOR;                                 // 0x10040d28
    ent->client->ps.pm_flags &= ~PMF_FOLLOW;                                // 0x10040d35
    ent->client->sess.spectatorState = SPECTATOR_FREE;                      // 0x10040d43
    ent->client->ps.clientNum = ent - g_entities;                           // 0x10040d6e

    if (ent->r.linked) {                                                    // 0x10040d74
        trap_UnlinkEntity(ent);
    }
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f(gentity_t* ent) {
    int oldTeam;
    char s[MAX_TOKEN_CHARS];
    const char* teamName;

    if (trap_Argc() != 2) {
        // [QL] Binary (0x10040f70) prints a single "print \"%s\n\"" with the
        // team name: RED->"Red Team", BLUE->"Blue Team", SPECTATOR->"Spectator",
        // anything else (incl. TEAM_FREE)->"Team Free".
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

    // [QL] 0x10040e55: the delay comes from g_switchTeamDelay, and the message says how
    // long it is. It used to be hardcoded to 5 seconds, ignoring the cvar, which already
    // existed with the binary's own default of 3.
    if (ent->client->switchTeamTime > level.time) {
        trap_SendServerCommand(ent - g_entities,
            va("print \"" S_COLOR_RED "May not switch teams more than once per %i seconds." S_COLOR_WHITE "\n\"",
               g_switchTeamDelay.integer));
        return;
    }

    // if they are playing a tournement game, count as a loss
    // [QL] Binary also gates this on level.warmupTime == 0.
    if ((level.warmupTime == 0) && (g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }

    trap_Argv(1, s, sizeof(s));

    oldTeam = ent->client->sess.sessionTeam;
    SetTeam(ent, s);

    // [QL] 0x10040e88: the delay is only armed if the change happened and the resulting
    // team is NOT spectator. The binary tests SetTeam's return value together with the
    // resulting team; comparing the team before and after says the same thing. Going
    // spectator costs no delay, and neither does asking for the team you are already on.
    if (ent->client->sess.sessionTeam != oldTeam && ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        ent->client->switchTeamTime = level.time + g_switchTeamDelay.integer * 1000;
    }
}

/*
=================
Cmd_Follow_f
=================
*/
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

    // can't follow self
    if (&level.clients[i] == ent->client) {
        return;
    }

    // can't follow another spectator
    if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    // if they are playing a tournement game, count as a loss
    if ((g_gametype.integer == GT_DUEL) && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }

    // first set them to spectator
    if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        SetTeam(ent, "spectator");
    }

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
}

/*
=================
FollowCycle

[QL] 0x10041130: moves the view on to the next player there is to follow, without touching the
session team. A player on a team in a team mode only follows their own team; spectators and
players who are following someone are skipped. The round modes use it for their dead players,
who stay on their team. Quake 3's version, which put them on the spectator team first, now
lives only in the commands (Cmd_FollowCycle_f).
=================
*/
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

        // can only follow connected players who are neither spectating nor following
        cl = &level.clients[clientnum];
        if (cl->pers.connected != CON_CONNECTED || cl->ps.pm_type == PM_SPECTATOR ||
            (cl->ps.pm_flags & PMF_FOLLOW)) {
            continue;
        }

        // a player on a team keeps to their own team
        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && g_gametype.integer >= GT_TEAM &&
            cl->sess.sessionTeam != ent->client->sess.sessionTeam) {
            continue;
        }

        // this is good, we can use it
        ent->client->sess.spectatorClient = clientnum;
        if (ent->client->sess.spectatorState != SPECTATOR_FOLLOW) {
            Cmd_Score_f(ent);
        }
        ent->client->sess.spectatorState = SPECTATOR_FOLLOW;

        // [QL] Race: send the followed player's race timing to the spectator.
        // Binary: Cmd_FollowCycle_f 0x10041310 -> va("race_info %i %i %i %i %i %i", ...)
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

    // leave it where it was
}

/*
=================
Cmd_FollowCycle_f

[QL] 0x100412e0, the follownext and followprev commands: a duel player gives up the game and a
player on a team becomes a spectator before FollowCycle.
=================
*/
void Cmd_FollowCycle_f(gentity_t* ent, int dir) {
    // if they are playing a tournement game, count as a loss
    if (level.warmupTime == 0 && g_gametype.integer == GT_DUEL && ent->client->sess.sessionTeam == TEAM_FREE) {
        ent->client->sess.losses++;
    }
    // first set them to spectator
    if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
        SetTeam(ent, "spectator");
    }
    FollowCycle(ent, dir);
}

/*
==================
G_Say
==================
*/

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
    // no chatting to players in tournements
    if ((g_gametype.integer == GT_DUEL) && other->client->sess.sessionTeam == TEAM_FREE && ent->client->sess.sessionTeam != TEAM_FREE) {
        return;
    }

    // [QL] The chat/tchat relay carries the sender's clientNum as a leading
    // "%02d " field inside the quoted payload; the cgame chat/tchat handler
    // reads it via atoi(CG_Argv(1)) to associate the message (and its voice
    // sound) with the speaking client. Q3 omitted this field.
    // Address: 0x10041510 (per-recipient chat/voice emitter)
    trap_SendServerCommand(other - g_entities, va("%s \"%02d %s%c%c%s\"",
                                                  mode == SAY_TEAM ? "tchat" : "chat",
                                                  ent->s.number,
                                                  name, Q_COLOR_ESCAPE, color, message));
}

#define EC "\x19"

void G_Say(gentity_t* ent, gentity_t* target, int mode, const char* chatText) {
    int j;
    gentity_t* other;
    int color;
    char name[64];
    // don't let text be too long for malicious reasons
    char text[MAX_SAY_TEXT];
    char location[64];

    // [ioquakelive] NOT FROM RETAIL, added for this build. While Crash walks the tour and
    // explains (g_training 2) the player's chat does not go out; what they type comes back
    // to them with a notice. Bots keep talking, since that is the explanation itself, and
    // nothing else is touched -- no command, no setting, only say. When the lap ends
    // g_training drops to 1 and chat comes back on its own.
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

    if (target) {
        G_SayTo(ent, target, mode, color, name, text);
        return;
    }

    // echo the text to the console
    if (g_dedicated.integer) {
        G_Printf("%s%s\n", name, text);
    }

    // send it to all the appropriate clients
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

/*
==================
Cmd_Say_f
==================
*/
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

/*
==================
Cmd_Tell_f
==================
*/
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
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Say(ent, ent, SAY_TELL, p);
    }
}

/*
==================
Cmd_Voice_f
qagamex86.dll 0x10041e40

[QL] vsay, vsay_team, vosay and vosay_team are still commands but say nothing: retail joins the
arguments and drops them. No voice chat reaches a client, and its cgame has no vchat handler.
==================
*/
static void Cmd_Voice_f(void) {
    if (trap_Argc() > 1) {
        ConcatArgs(1);
    }
}

/*
==================
Cmd_VoiceTell_f
qagamex86.dll 0x10041e60

[QL] vtell and votell only log the tell, when the target is a client in use.
==================
*/
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

/*
==================
Cmd_VoiceTaunt_f
qagamex86.dll 0x10041f50

[QL] vtaunt still picks whom Q3 would taunt but sends nothing: it only forgets the killer or the
last kill. The teammate-praise scan that follows in retail changes nothing and is left out.
==================
*/
static void Cmd_VoiceTaunt_f(gentity_t* ent) {
    if (!ent->client) {
        return;
    }

    // the one who just killed you
    if (ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client == ent->s.number) {
        ent->enemy = NULL;
        return;
    }
    // the one you just killed
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
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !(ent->r.svFlags & SVF_BOT)) {
        G_Say(ent, ent, SAY_TELL, gc_orders[order]);
    }
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f(gentity_t* ent) {
    // [QL] Binary (0x10042360) formats the origin as "(%.2f %.2f %.2f)" (floats,
    // parenthesised) via a rotating static buffer, NOT Q3's vtos() ("%i %i %i").
    char buf[32];

    Com_sprintf(buf, sizeof(buf), "(%.2f %.2f %.2f)",
                ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
    trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", buf));
}

// [QL] use shared gametypeDisplayNames[] from bg_misc.c

/*
==================
IsValidCallvoteArg
[QL] Check for command separators/injection in vote arguments
==================
*/
static qboolean IsValidCallvoteArg(const char *s) {
    const char *c;
    for (c = s; *c; c++) {
        if (*c == '\n' || *c == '\r' || *c == ';') {
            return qfalse;
        }
    }
    return qtrue;
}

/*
==================
StartVote
[QL] Begin a vote - set configstrings, announce to all clients
==================
*/
static void StartVote(void) {
    int i;

    level.voteTime = level.time;
    level.voteYes = 1;  // the caller's implicit yes
    level.voteNo = 0;

    // [QL] seed per-client vote state. Bots, and (unless g_allowSpecVote) spectators, are
    // not eligible voters and are excluded from the CheckVote tally, so a lone human's own
    // yes can carry a majority. Previously bots counted as pending voters, so a solo player
    // plus any bots could never reach voteYes > eligible/2.
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
    // caller has voted yes
    level.clients[level.pendingVoteCaller].pers.voteState = VOTE_YES;

    trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
    trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
    trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
    trap_SetConfigstring(CS_VOTE_NO, "0");

    trap_SendServerCommand(-1, va("print \"%s called a vote.\n\"",
        g_entities[level.pendingVoteCaller].client->pers.netname));
}

/*
==================
ClearVote
[QL] End a vote - clear configstrings and client vote states
==================
*/
void ClearVote(void) {
    int i;

    level.voteTime = 0;
    level.voteYes = 0;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++) {
        level.clients[i].pers.voteState = VOTE_NONE;
    }

    trap_SetConfigstring(CS_VOTE_TIME, "");
}

/*
==================
Cmd_CallVote_f
[QL] Rewritten to match binary (0x10042580) - 15 vote types,
g_voteFlags, g_voteDelay, g_voteLimit, g_allowSpecVote, g_allowVoteMidGame.
Color-coded help text shows enabled/disabled vote types.
==================
*/
void Cmd_CallVote_f(gentity_t *ent) {
    int clientNum = ent - g_entities;
    char arg1[MAX_STRING_TOKENS];
    char arg2[MAX_STRING_TOKENS];
    char arg3[MAX_STRING_TOKENS];  // [QL] the factory of callvote map
    int vf = g_voteFlags.integer;

    // --- precondition checks ---

    if (level.time < g_voteDelay.integer) {
        trap_SendServerCommand(clientNum,
            va("print \"Voting will be allowed in %.1f seconds.\n\"",
               (float)(g_voteDelay.integer - level.time) / 1000.0f));
        return;
    }

    // [ioquakelive] There is no voting in the Crash Course. The binary cuts it off right here,
    // between the vote delay and the "Public voting is not allowed here" (0x100423xx, the string
    // vive en 0x100836a4).
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

    // --- parse arguments ---

    trap_Argv(1, arg1, sizeof(arg1));
    trap_Argv(2, arg2, sizeof(arg2));
    trap_Argv(3, arg3, sizeof(arg3));

    if (!IsValidCallvoteArg(arg1) || !IsValidCallvoteArg(arg2) || !IsValidCallvoteArg(arg3)) {
        trap_SendServerCommand(clientNum, "print \"Invalid vote string.\n\"");
        return;
    }

    // --- validate vote command + g_voteFlags checks ---

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
        // [QL] callvote map <map> [factory] (qagamex86.dll 0x10042520..0x100425b0):
        // naming a factory is a gametype vote, which VF_GAMETYPE disables
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
    // [QL] 0x10042580: by name and by number are two different paths, each with its own
    // messages. By number, an admin cannot be voted off either.
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
        if (ts > 64) {
            trap_SendServerCommand(clientNum,
                va("print \"Invalid team size. (Valid Range: %d - %d)\n\"",
                   g_teamSizeMin.integer, g_maxclients.integer));
            return;
        }
        if (ts > 0) {
            int total = TeamCount(-1, TEAM_RED) + TeamCount(-1, TEAM_BLUE) +
                        TeamCount(-1, TEAM_FREE);
            int red = TeamCount(-1, TEAM_RED);
            int blue = TeamCount(-1, TEAM_BLUE);
            int maxSize = (g_gametype.integer >= GT_TEAM) ? g_maxclients.integer / 2
                                                         : g_maxclients.integer;

            // [QL] 0x100430e0: the whole arena first, then each team
            if (ts < total) {
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
        // [QL] 0x10042580: the header, the five lines with their colour -- 5 when the vote
        // is enabled, 1 when it is not -- and the usage at the end. The "Invalid vote
        // string." above is for text with odd characters, not for an unknown vote.
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

    // --- execute any pending vote first ---

    if (level.voteExecuteTime) {
        level.voteExecuteTime = 0;
        trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
    }

    // --- build vote string (for commands not handled above) ---

    if (!Q_stricmp(arg1, "map")) {
        // [QL] with the factory when one was named (qagamex86.dll 0x10042c8d)
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

/*
==================
Cmd_IntermissionVote_f
[QL] End-game map voting - players vote 1/2/3 during intermission.
Binary: FUN_100441d0
==================
*/
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

    // 20-second voting window
    if (level.time - level.voteTime >= 20000) {
        trap_SendServerCommand(clientNum, "print \"Voting time has expired.\n\"");
        return;
    }

    // 2-second cooldown between votes
    if (level.time - ent->client->pers.lastMapVoteTime < 2000) {
        trap_SendServerCommand(clientNum, "print \"You may only vote once every 2 seconds.\n\"");
        return;
    }

    // Already voted for same map
    if (choice == ent->client->pers.lastMapVoteIndex) {
        trap_SendServerCommand(clientNum, "print \"You already voted for this arena.\n\"");
        return;
    }

    // Validate choice
    if (choice < 1 || choice > 3 || !level.intermissionMapNames[choice - 1][0]) {
        return;
    }

    // Record new vote
    ent->client->pers.lastMapVoteTime = level.time;
    ent->client->pers.lastMapVoteIndex = choice;

    // [QL] Hide this client's map-vote UI now that they have voted.
    // Binary: Cmd_IntermissionVote_f 0x100441d0 -> trap_SendServerCommand(client, "disable_vote_ui").
    // G_RunFrame re-sends "enable_vote_ui" ~2.25s after lastMapVoteTime so they can change it.
    trap_SendServerCommand(clientNum, "disable_vote_ui");

    // Broadcast vote
    trap_SendServerCommand(-1,
        va("print \"%s" S_COLOR_WHITE " voted for %s.\n\"",
           ent->client->pers.netname,
           level.intermissionMapTitles[choice - 1][0]
               ? level.intermissionMapTitles[choice - 1]
               : level.intermissionMapNames[choice - 1]));

    G_TallyMapVotes();
}

/*
==================
G_TallyMapVotes

[QL] 0x10043d80: the votes are counted again from what each connected player last chose,
so the vote of somebody who left no longer counts, and go out as an infostring by slot
==================
*/
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

/*
==================
Cmd_Vote_f
[QL] Updated to use per-client voteState. Redirects to
Cmd_IntermissionVote_f during intermission (binary: 0x10044450).
==================
*/
void Cmd_Vote_f(gentity_t *ent) {
    char msg[64];

    // [QL] During intermission, redirect to map voting
    if (level.intermissionTime) {
        Cmd_IntermissionVote_f(ent);
        return;
    }

    if (!level.voteTime) {
        trap_SendServerCommand(ent - g_entities, "print \"No vote in progress.\n\"");
        return;
    }

    // [QL] 0x10044270: an admin's state is not checked -- they can always vote, and can
    // even vote again. Everyone else is told why they cannot.
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

    // an admin's vote is not just another one: a yes forces it through and a no vetoes it
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

/*
=================
Cmd_SetViewpos_f
=================
*/
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

/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f(gentity_t* ent) {
    /*
        int max, n, i;

        max = trap_AAS_PointReachabilityAreaIndex( NULL );

        n = 0;
        for ( i = 0; i < max; i++ ) {
            if ( ent->client->areabits[i >> 3] & (1 << (i & 7)) )
                n++;
        }

        //trap_SendServerCommand( ent-g_entities, va("print \"visited %d of %d areas\n\"", n, max));
        trap_SendServerCommand( ent-g_entities, va("print \"%d%% level coverage\n\"", n * 100 / max));
    */
}

/*
=================
[QL] Accuracy command - sends per-weapon accuracy stats to client
Binary: qagamex86.dll 0x1003f6b0
=================
*/
static void Cmd_Acc_f(gentity_t* ent) {
    gclient_t* cl;
    char result[1024];
    char buf[MAX_TOKEN_CHARS];
    int i;

    // [QL] 0x1003f4d0: somebody following a player gets that player's
    cl = ent->client;
    if (!cl) {
        return;
    }
    if (cl->ps.pm_type == PM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW) {
        cl = &level.clients[cl->sess.spectatorClient];
    }

    // each weapon's own accuracy, " 0" for one not fired or never on target
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

/*
=================
[QL] PStats command - sends per-weapon stats to client
Binary: qagamex86.dll 0x1003f660 (identical structure to Cmd_Acc_f)
=================
*/
static void Cmd_PStats_f(gentity_t* ent) {
    gclient_t* cl;
    char result[1024];
    char buf[MAX_TOKEN_CHARS];
    int i;

    // [QL] 0x1003f4d0: somebody following a player gets that player's
    cl = ent->client;
    if (!cl) {
        return;
    }
    if (cl->ps.pm_type == PM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW) {
        cl = &level.clients[cl->sess.spectatorClient];
    }

    // each weapon's own accuracy, " 0" for one not fired or never on target
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

/*
=================
[QL] ReadyUp command - toggles player ready state during warmup or intermission
Binary: qagamex86.dll 0x10045db0
=================
*/
static void Cmd_ReadyUp_f(gentity_t* ent) {
    gclient_t* cl;

    // Valid during warmup (warmupTime < 0) or intermission
    if (level.warmupTime >= 0 && level.intermissionTime == 0)
        return;

    // [QL] qagamex86.dll 0x10045bd0: in a training match with the skip offered
    // (g_skipTrainingEnable, which the tour turns on), readyup first ends the tour
    // (Cmd_SkipTraining 0x10045a80) and then readies up as usual. Without the skip,
    // nobody can ready up during the tour's intro (g_training 2).
    if (g_training.integer && g_skipTrainingEnable.integer) {
        Cmd_SkipTraining(ent->s.clientNum);
    } else if (g_training.integer == 2) {
        return;
    }
    cl = ent->client;
    if (!cl) return;
    if (cl->ps.pm_type == PM_SPECTATOR) return;
    if (cl->sess.sessionTeam == TEAM_SPECTATOR) return;
    // Binary tests pm_flags & 0x1000 (PMF_FOLLOW), not PMF_FROZEN.
    if (cl->ps.pm_flags & PMF_FOLLOW) return;

    if (level.warmupTime < 0) {
        // Validate minimum players
        if (!TeamsPresent()) {
            // If already ready, un-ready them
            if (cl->pers.ready) {
                cl->pers.ready = qfalse;
                ClientUserinfoChanged(ent - g_entities);
                return;
            }
            // Print appropriate error
            if (BG_IsScoreBasedGameType()) {
                trap_SendServerCommand(ent - g_entities,
                    "print \"Players cannot ready up until both teams are present.\n\"");
            } else {
                trap_SendServerCommand(ent - g_entities,
                    "print \"Cannot ready up until more players are present.\n\"");
            }
            return;
        }
        // Validate team balance
        if (!G_CheckTeamBalance()) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Players cannot ready up until both teams are fully present.\n\"");
            return;
        }
    }

    // During intermission, only allow toggling TO ready (not un-readying)
    if (level.intermissionTime != 0 && cl->pers.ready)
        return;

    // Toggle ready state
    cl->pers.ready = !cl->pers.ready;
    ClientUserinfoChanged(ent - g_entities);

    // Announce during warmup
    if (level.warmupTime < 0) {
        trap_SendServerCommand(-1, va("cp \"%s ^7is ^7%s\n\"",
            cl->pers.netname,
            cl->pers.ready ? "Ready" : "Not Ready"));
    }
}

/*
=================
[QL] BotSay command - send a chat message as a bot to a specific client
Binary: qagamex86.dll 0x100413a0
Usage: botsay <targetNum> <duration> <message>
=================
*/
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

/*
=================
[QL] Spec command - filtered spectate/follow
Binary: qagamex86.dll 0x100459a0
Cycles through players matching a filter: stats, red/r, blue/b, powerup
=================
*/
static void Cmd_Spec_f(gentity_t* ent) {
    int clientnum, original;
    char filter[MAX_TOKEN_CHARS];
    gclient_t* cl;

    if (trap_Argc() != 2) {
        // No filter arg: just follow like normal
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

        // Team filter in team gametypes
        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
            g_gametype.integer >= GT_TEAM &&
            cl->sess.sessionTeam != ent->client->sess.sessionTeam) {
            continue;
        }

        // Apply filter
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

        // Found a valid target
        ent->client->sess.spectatorClient = clientnum;
        ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
        return;
    } while (clientnum != original);
}

/*
=================
[QL] Spectator item timer refresh: "specresp", which cgame sends on respawn while EF_SPECTATOR is set,
gets this spectator every item timer again (qagamex86.dll 0x100462e5)
=================
*/
static void Cmd_SpecResp_f(gentity_t *ent) {
    G_SendItemTimersToSpectator(ent - g_entities);
}

/*
=================
[QL] Forfeit command - forfeit the current match
Binary: qagamex86.dll 0x10045890
=================
*/
static void Cmd_Forfeit_f(gentity_t* ent) {
    if (!ent->client) return;
    // Binary guard is ps.pm_type == PM_SPECTATOR (client+4 == 2), NOT sessionTeam.
    if (ent->client->ps.pm_type == PM_SPECTATOR) return;

    // level.timePauseBegin (global 0x105dea08, Ghidra "level_restarted") = pause/
    // timeout start time; no forfeits while frozen.
    if (level.timePauseBegin != 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Forfeit is not available during a pause or timeout.\n\"");
        return;
    }

    // Round-based gametypes: not during the round countdown (eCurrent, not eNext).
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

/*
=================
[QL] Permission-based command handlers
These commands are checked via ClientCommandPermissionCheck
before being dispatched. Privilege levels: 0=anyone, 1=referee, 2=admin
=================
*/

// "?" - List available permission commands with descriptions
static void Cmd_PermHelp_f(gentity_t* ent);  // forward declare

// "players" - List connected players
static void Cmd_Players_f(gentity_t* ent) {
    int i;
    gclient_t* cl;
    char msg[1024];
    char line[128];
    char* teamStr;

    msg[0] = '\0';
    Q_strcat(msg, sizeof(msg), "print \"\n--- Connected Players ---\n\"");
    trap_SendServerCommand(ent - g_entities, msg);

    for (i = 0; i < level.maxclients; i++) {
        cl = &g_clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;

        switch (cl->sess.sessionTeam) {
        case TEAM_RED: teamStr = "RED"; break;
        case TEAM_BLUE: teamStr = "BLUE"; break;
        case TEAM_SPECTATOR: teamStr = "SPEC"; break;
        default: teamStr = "FREE"; break;
        }

        Com_sprintf(line, sizeof(line), "print \"%2d %-4s %s\n\"",
                   i, teamStr, cl->pers.netname);
        trap_SendServerCommand(ent - g_entities, line);
    }
}

/*
=================
Cmd_Complaint_f

[QL] 0x10043ab0. The victim's answer to the complaint prompt g_combat opened when a
teammate killed them: "complaint y" files it, anything else drops it. The window lasts
as long as pers.complaintEndTime says, and only exists with a match running and
g_complaintLimit non-zero. The numbers sent back are the ones the client expects: -1
filed, -2 dropped, -3 against somebody who cannot be reported.

Whoever collects g_complaintLimit complaints is kicked, unless they are a local client.
Retail also stores their Steam ID and writes two fields of a struct that has not been
identified; that part is not ported.
=================
*/
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
        return;  // [QL] 0x10043d6b: with the accused gone, it does not even clear it
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

/*
=================
G_CommandPhaseAllowed

[QL] 0x10061090. Which phase of a match a command may be used in. Retail has a single
one of these and every command tells it which phases it accepts; the three callers are
pause, timein and allready.

Four answers, one per phase, with the binary's own strings (0x10088978, 0x100889b4,
0x100889e8, 0x10088a28). From the server console the reason is printed there, "print"
and quotes included, which is what retail hands to trap_Printf.
=================
*/
#define CMD_PHASE_WARMUP 1        // durante el calentamiento
#define CMD_PHASE_MATCH 2         // with a match running, and during a round countdown
#define CMD_PHASE_INTERMISSION 4  // during the break at the end

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

/*
=================
[QL] MP_Pause -- pause the match (qagame 0x18ba00 / .so MP_Pause).

duration is in seconds (0 = indefinite pause). Records level.timePauseBegin (the frozen
match-clock base), schedules the auto-unpause, and publishes CS_PAUSE_START_TIME /
CS_PAUSE_END_TIME so every client freezes its match timer, stops autofire, and shows the
pause. G_RunFrame freezes the clock per-frame; players are frozen via PMF_PAUSED.
=================
*/
void MP_Pause(gentity_t* ent, int duration) {
    const char* name;

    // [QL] 0x10061e10: pause only runs with a match in progress, and the shared gate
    // decides that, not a copy of its rules. An already paused match is handled by the
    // caller (Cmd_Pause_f).
    if (!G_CommandPhaseAllowed(ent, CMD_PHASE_MATCH)) {
        return;
    }
    if (level.timePauseBegin != 0) {
        return;
    }

    level.timePauseBegin = level.time;
    level.mp_unpauseTime = duration ? level.time + duration * 1000 : 0;
    // [QL] binary stores mp_pauseClient=0 for a server pause plus a separate
    // server-pause flag (uRam1049abd8); the reimpl folds both into the -1 sentinel.
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

/*
=================
[QL] MP_PauseThink -- called each frame from G_RunFrame while paused.

Once the scheduled unpause time is reached, shifts every active client's respawn/powerup/
weapon timers forward by the paused duration (so nothing that was pending fires early on
resume), then clears the pause and republishes the match clock (CS_LEVEL_START_TIME, which
G_RunFrame advanced by the paused duration).
=================
*/
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

    // resume play
    level.timePauseBegin = 0;
    level.mp_unpauseTime = 0;
    trap_SetConfigstring(CS_PAUSE_START_TIME, "0");
    trap_SetConfigstring(CS_PAUSE_END_TIME, "0");
    trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
}

// "timeout" - Call a timeout (uses the caller's per-player or per-team timeout allowance)
static void Cmd_Timeout_f(gentity_t* ent) {
    team_t team;

    if (!ent->client) {
        return;
    }
    team = ent->client->sess.sessionTeam;
    if (team == TEAM_SPECTATOR || level.timePauseBegin != 0) {
        return;
    }
    if (g_timeoutCount.integer == 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Timeouts are not enabled on this server.\n\"");
        return;
    }

    if (g_gametype.integer < GT_TEAM) {
        // non-team: per-player timeout tracking
        if (ent->client->pers.timeouts >= g_timeoutCount.integer) {
            trap_SendServerCommand(ent - g_entities,
                "print \"You have no timeouts left to call\n\"");
            return;
        }
        ent->client->pers.timeouts++;
    } else {
        // team modes: per-team timeout tracking, published to the HUD
        if (level.teamTimeoutsUsed[team] >= g_timeoutCount.integer) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Your team has no timeouts left to call\n\"");
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

// "timein" / "unpause" - Resume play. Starts a 5-second countdown before unpausing.
static void Cmd_Timein_f(gentity_t* ent) {
    if (level.timePauseBegin == 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"No timeout is in progress.\n\"");
        return;
    }
    // already counting down within the last 5 seconds?
    if (level.mp_unpauseTime != 0 && level.mp_unpauseTime <= level.time + 4999) {
        return;
    }
    // only the player who called the timeout (or a referee/admin) may call timein
    if (level.mp_pauseClient >= 0 && ent && ent->client &&
        (int)(ent - g_entities) != level.mp_pauseClient &&
        ent->client->sess.privileges == 0) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You did not call the current timeout\n\"");
        return;
    }

    level.mp_unpauseTime = level.time + 5000;
    trap_SetConfigstring(CS_PAUSE_END_TIME, va("%i", level.mp_unpauseTime));
    // [QL] 0x1006240a: two separate messages in the binary, not one %s with "The server"
    // in it. Splitting them also avoids dereferencing ent when the command came from the
    // server console, which is exactly when ent does not exist.
    if (ent && ent->client) {
        trap_SendServerCommand(-1, va("pcp \"%s has called timein\n\"",
                                      ent->client->pers.netname));
    } else {
        trap_SendServerCommand(-1, "pcp \"The server has called timein\n\"");
    }
}

// "allready" - Force all players to ready state (referee+)
// Binary: qagamex86.dll 0x10061860
static void Cmd_AllReady_f(gentity_t* ent) {
    int i;

    // [QL] 0x10061805: allready belongs to warmup and to no other phase -- and outside it
    // it says so, which it used not to do
    if (!G_CommandPhaseAllowed(ent, CMD_PHASE_WARMUP)) {
        return;
    }

    if (g_gametype.integer == GT_DUEL && level.numPlayingClients != 2) {
    trap_SendServerCommand(ent - g_entities, "print \"Allready may not be used until two players are in the match.\n\"");
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

// "pause" - Pause the match indefinitely (referee+)
static void Cmd_Pause_f(gentity_t* ent) {
    // [QL] 0x10062140: on an already paused match, whoever asks again takes ownership of
    // the timeout -- and is then the one who can lift it
    if (level.timePauseBegin != 0) {
        int clientNum = ent - g_entities;

        if (level.mp_pauseClient != clientNum) {
            level.mp_pauseClient = clientNum;
            trap_SendServerCommand(clientNum,
                "print \"You have taken ownership of the current timeout\n\"");
        }
        return;
    }
    MP_Pause(ent, 0);  // 0 = indefinite; MP_Pause rejects warmup/intermission
}

// "unpause" - Unpause the match (referee+), same handler as timein
// (uses Cmd_Timein_f)

/*
=================
G_FindPlayerArg

[QL] 0x100611d0. The player named by the first argument of the admin commands. It
takes a client number only -- no name lookup -- and has two messages, which are the
only ones these commands give when the argument is wrong.
=================
*/
static gentity_t* G_FindPlayerArg(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    int id;
    int i;

    if (trap_Argc() <= 1) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Missing PlayerID (use \\players for a list)\n\"");
        return NULL;
    }

    trap_Argv(1, arg, sizeof(arg));
    id = atoi(arg);

    for (i = 0; arg[i]; i++) {
        if (arg[i] < '0' || arg[i] > '9') {
            id = -1;  // not a number: there is nobody to look for
            break;
        }
    }

    if (id < 0 || id >= level.maxclients || !level.clients[id].pers.connected) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Invalid PlayerID (use \\players for a list)\n\"");
        return NULL;
    }
    return &g_entities[id];
}

/*
=================
G_ParseTeamArg

[QL] 0x10061350. The team argument shared by put, lock and unlock: it looks at the
first letter, case insensitive. "s" is spectators, "r" and "b" are the teams (which
only exist in team modes) and "f" is free play (which only exists outside them). On
anything else it complains and returns TEAM_NUM_TEAMS.
=================
*/
static team_t G_ParseTeamArg(gentity_t* ent, int argIndex) {
    char arg[MAX_TOKEN_CHARS];
    int c;
    qboolean teamGame = (qboolean)(g_gametype.integer >= GT_TEAM);

    if (trap_Argc() <= argIndex) {
        trap_SendServerCommand(ent - g_entities, "print \"Missing TeamName\n\"");
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
        // the letter exists but not in this gametype
        trap_SendServerCommand(ent - g_entities,
            teamGame ? "print \"Invalid TeamName (use R/B/S)\n\""
                     : "print \"Invalid TeamName (use F/S)\n\"");
    } else {
        trap_SendServerCommand(ent - g_entities,
            va("print \"Team %c is not valid for this command\n\"", c ? c : ' '));
    }
    return TEAM_NUM_TEAMS;
}

// [QL] 0x1049abec: which teams are locked. The command can lock any of them, but only
// red and blue are checked on join (0x100618b0).
static qboolean teamLocked[TEAM_NUM_TEAMS];

/*
=================
G_TeamIsLocked

[QL] 0x100618b0, from SetTeam (0x10040cc8). Free play and spectators are never
locked.
=================
*/
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

// the message retail gives on locking or unlocking a team, the same for both
static void G_AnnounceLock(team_t team, qboolean locked) {
    trap_SendServerCommand(-1, va("print \"The %s team is now %slocked\n\"",
                                  TeamName(team), locked ? "" : "un"));
}

/*
=================
Cmd_Lock_f

[QL] 0x10061940. With no argument it locks both teams; with one, the team asked for.
A team that was already locked is not announced again.
=================
*/
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

/*
=================
Cmd_Unlock_f

[QL] 0x10061a40, the mirror of the one above.
=================
*/
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

// "put" - Move a player to a team (referee+)
static void Cmd_Put_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    const char* dest;
    team_t team;

    if (!target) {
        return;
    }

    // [QL] 0x10061b61: the same team parser lock and unlock use
    team = G_ParseTeamArg(ent, 2);
    switch (team) {
        case TEAM_RED: dest = "red"; break;
        case TEAM_BLUE: dest = "blue"; break;
        case TEAM_SPECTATOR: dest = "spectator"; break;
        case TEAM_FREE: dest = "free"; break;
        default: return;
    }

    // [QL] 0x10061b74: a third player does not enter a duel. With the match running or
    // counting down the move is refused; while waiting for players it only warns and
    // moves them anyway, which is the difference between the binary's two branches.
    if (g_gametype.integer == GT_DUEL && team == TEAM_FREE && level.numPlayingClients >= 2) {
        if (level.warmupTime >= 0) {
            trap_SendServerCommand(ent - g_entities,
                "print \"A maximum of two players are allowed in the match.\n\"");
            return;
        }
        trap_SendServerCommand(ent - g_entities,
            "print \"The match cannot begin until only two players are in the match.\n\"");
    }

    // [QL] 0x10061c78: if they were already there, tell the caller and do not move them
    if (target->client->sess.sessionTeam == team) {
        trap_SendServerCommand(ent - g_entities,
            va("print \"%s is already on the %s team\n\"",
               target->client->pers.netname, TeamName(team)));
        return;
    }

    SetTeam(target, dest);

    // [QL] 0x10061d00 and 0x10061d5c: whoever is moved is told who moved them
    if (ent->client->pers.localClient) {
        trap_SendServerCommand(target - g_entities,
            va("print \"The server has moved you to the %s team\n\"", TeamName(team)));
    } else {
        trap_SendServerCommand(target - g_entities,
            va("print \"%s has moved you to the %s team\n\"",
               ent->client->pers.netname, TeamName(team)));
    }
}

// "mute" - Mute a player's chat (referee+)
static void Cmd_Mute_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);

    if (!target) {
        return;
    }

    // [QL] 0x100627d4: nobody mutes someone of their own rank or above. From the server's
    // own console the check is skipped: retail passes the command a flag saying it came
    // from there.
    if (!ent->client->pers.localClient &&
        ent->client->sess.privileges <= target->client->sess.privileges) {
        return;
    }

    target->client->sess.muted = 1;
    trap_SendServerCommand(-1, va("print \"%s has been muted\n\"",
        target->client->pers.netname));
}

// "unmute" - Unmute a player's chat (referee+)
static void Cmd_Unmute_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);

    if (!target) {
        return;
    }

    // [QL] 0x100627d4: nobody mutes someone of their own rank or above. From the server's
    // own console the check is skipped: retail passes the command a flag saying it came
    // from there.
    if (!ent->client->pers.localClient &&
        ent->client->sess.privileges <= target->client->sess.privileges) {
        return;
    }

    target->client->sess.muted = 0;
    trap_SendServerCommand(-1, va("print \"%s has been unmuted\n\"",
        target->client->pers.netname));
}

// "tempban" / "ban" - Kick and ban a player (referee+)
static void Cmd_Ban_f(gentity_t* ent) {
    char cmd[MAX_TOKEN_CHARS];
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;

    // [QL] "Can not kick admins."
    if (g_clients[pid].sess.privileges >= ent->client->sess.privileges) {
        trap_SendServerCommand(ent - g_entities, "print \"Can not kick admins.\n\"");
        return;
    }

    // [QL] the two commands differ in the access list, not in the kick: ban leaves the
    // account marked and that is saved on map change (G_SaveAccessList, 0x10032a10);
    // tempban marks it as temporary, so it is forgotten with the map
    trap_Argv(0, cmd, sizeof(cmd));
    G_SetAccess(g_clients[pid].pers.steamId, -1, (qboolean)(Q_stricmp(cmd, "tempban") == 0));

    trap_DropClient(pid, "You are banned from this server.");
}

/*
===============
Cmd_Unban_f

[QL] " <account>^3 removes a player from the banlist": it takes an account number, not a
player id, because a banned player is precisely not connected. The message is the
binary's own, "%llu has been unbanned".
===============
*/
static void Cmd_Unban_f(gentity_t* ent) {
    char arg[MAX_TOKEN_CHARS];
    uint64_t steamId;

    // [QL] 0x10062ad0: the only message it gives is the one at the end
    if (trap_Argc() < 2) {
        return;
    }
    trap_Argv(1, arg, sizeof(arg));
    steamId = (uint64_t)strtoll(arg, NULL, 10);
    if (!steamId || G_GetAccessBySteamId(steamId) != -1) {
        return;
    }
    G_SetAccess(steamId, 0, qfalse);
    trap_SendServerCommand(-1, va("print \"%llu has been unbanned\n\"",
        (unsigned long long)steamId));
}

// "listaccess" - List access control entries (referee+)
static void Cmd_ListAccess_f(gentity_t* ent) {
    // [QL] 0x100327d0: the whole list, twenty to a page, with the level and whether it is
    // temporal o permanente
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

    trap_SendServerCommand(ent - g_entities,
        va("print \"Access List: Page %i of %i\n\"", page + 1, pages + 1));
    trap_SendServerCommand(ent - g_entities,
        "print \"=============================\n\"");

    for (i = page * 20; i < total && i < (page + 1) * 20; i++) {
        uint64_t steamId = 0;
        int level = 0;
        qboolean temporary = qfalse;

        if (!G_AccessAt(i, &steamId, &level, &temporary)) {
            break;
        }
        trap_SendServerCommand(ent - g_entities,
            va("print \"%llu %s %s\n\"", (unsigned long long)steamId,
               (level >= 0 && level < 4) ? levels[level] : "BAN   ",
               temporary ? "TEMP" : "PERM"));
    }
}

// "opsay" - Admin chat message (referee+)
static void Cmd_OpSay_f(gentity_t* ent) {
    char* msg;

    // [QL] 0x10062bf0: with nothing to say it says nothing
    if (trap_Argc() < 2) {
        return;
    }
    msg = ConcatArgs(1);
    trap_SendServerCommand(-1, va("print \"<@%s^7> %s\n\"",
        ent->client->pers.localClient ? "server" : ent->client->pers.netname, msg));
}

// "addadmin" - Grant admin privileges (admin only)
static void Cmd_AddAdmin_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;

    g_clients[pid].sess.privileges = 2;
    // [QL] the account stays in the access list, which is saved on a map change
    G_SetAccess(g_clients[pid].pers.steamId, 2, qfalse);
    trap_SendServerCommand(-1, va("print \"%s has become an administrator\n\"",
        g_clients[pid].pers.netname));
}

// "addmod" - Grant moderator/referee privileges (admin only)
static void Cmd_AddMod_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;

    g_clients[pid].sess.privileges = 1;
    G_SetAccess(g_clients[pid].pers.steamId, 1, qfalse);
    trap_SendServerCommand(-1, va("print \"%s has become a moderator\n\"",
        g_clients[pid].pers.netname));
}

// "demote" - Remove privileges (admin only)
static void Cmd_Demote_f(gentity_t* ent) {
    gentity_t* target = G_FindPlayerArg(ent);
    int pid;

    if (!target) {
        return;
    }
    pid = target - g_entities;

    // [QL] "Can not demote someone at or above your level."
    if (g_clients[pid].sess.privileges >= ent->client->sess.privileges) {
        trap_SendServerCommand(ent - g_entities,
            "print \"Can not demote someone at or above your level.\n\"");
        return;
    }

    g_clients[pid].sess.privileges = 0;
    G_SetAccess(g_clients[pid].pers.steamId, 0, qfalse);
    trap_SendServerCommand(-1, va("print \"%s has had their privileges removed\n\"",
        g_clients[pid].pers.netname));
}

// "abort" - Abort match, return to warmup (referee+)
static void Cmd_Abort_f(gentity_t* ent) {
    // [QL] 0x10061550: nothing to abort during warmup, and it says nothing
    if (level.warmupTime != 0) {
        return;
    }

    // a paused match is not aborted
    if (level.timePauseBegin) {
    trap_SendServerCommand(ent - g_entities, "print \"Match is currently paused. You must unpause before aborting the match.\n\"");
        return;
    }

    if (ent->client->pers.localClient) {
    trap_SendServerCommand(-1, "pcp \"The server has aborted the match\n\"");
    } else {
    trap_SendServerCommand(-1, va("pcp \"%s has aborted the match\n\"",
        ent->client->pers.netname));
    }
    trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
}

// "addscore" - Add to a player's score (referee+)
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

// "addteamscore" - Add to a team's score (referee+)
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

// "setmatchtime" - Set match time in seconds (referee+)
static void Cmd_SetMatchTime_f(gentity_t* ent) {
    char timeStr[MAX_TOKEN_CHARS];
    int seconds;

    // [QL] 0x10062ce0: with no argument it does nothing and says nothing
    if (trap_Argc() < 2) {
        return;
    }

    trap_Argv(1, timeStr, sizeof(timeStr));
    seconds = atoi(timeStr);

    level.startTime = level.time - (seconds * 1000);
    trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
    trap_SendServerCommand(-1, va("pcp \"Match time has been set to %s.\n\"", timeStr));
}

/*
=================
[QL] Permission command table - checked by ClientCommandPermissionCheck
before the main ClientCommand dispatch.
Level: 0=anyone, 1=referee, 2=admin, 3=console-only
=================
*/
typedef struct {
    const char* cmd;
    int requiredLevel;
    void (*handler)(gentity_t* ent);
    const char* description;
} permissionCmd_t;

// Levels and description strings verified against the binary's table at
// 0x10080748. Cmd_Help_f prints "name" immediately followed by "description",
// so the descriptions carry their own leading params/colour codes. The
// "permenantly" typo in the ban description is present in the binary.
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
    { NULL,           0, NULL,              NULL }  // terminator
};

// "?" handler (binary Cmd_Help_f 0x10062dc0). Lists every command whose
// required level is within the caller's privileges, printing the name
// immediately followed by its description.
static void Cmd_PermHelp_f(gentity_t* ent) {
    int i;
    int privLevel;

    privLevel = ent->client->sess.privileges;
    if (ent->client->pers.localClient) privLevel = 3;

    for (i = 0; permissionCmds[i].cmd; i++) {
        if (permissionCmds[i].requiredLevel > privLevel) continue;
        trap_SendServerCommand(ent - g_entities,
            va("print \"%s%s\n\"",
               permissionCmds[i].cmd,
               permissionCmds[i].description ? permissionCmds[i].description : ""));
    }
}

/*
=================
ClientCommandPermissionCheck

Check if the command matches a permission-table entry.
Returns qtrue if the command was handled (either executed or denied).
Returns qfalse if the command is not in the permission table.
=================
*/
static qboolean ClientCommandPermissionCheck(gentity_t* ent, const char* cmd) {
    int i;
    int privLevel;

    for (i = 0; permissionCmds[i].cmd; i++) {
        if (Q_stricmp(cmd, permissionCmds[i].cmd) == 0) {
            break;
        }
    }

    if (!permissionCmds[i].cmd) {
        return qfalse;  // not a permission command
    }

    // Entries with no handler (rcon) are not handled here - fall through so the
    // engine/rcon path can deal with them. Matches the binary.
    if (!permissionCmds[i].handler) {
        return qfalse;
    }

    // Check privileges
    privLevel = ent->client->sess.privileges;
    if (ent->client->pers.localClient) privLevel = 2;  // localhost = admin

    if (privLevel < permissionCmds[i].requiredLevel) {
        trap_SendServerCommand(ent - g_entities,
            "print \"You do not have the privileges required to use this command\n\"");
        return qtrue;
    }

    permissionCmds[i].handler(ent);
    return qtrue;
}

/*
==================
G_GametypeDisplayName

The long name of the running gametype, or "Unknown Gametype" when the number falls
outside the table, which is what retail puts in the messages of the drop commands
(0x10044eac).
==================
*/
const char* G_GametypeDisplayName(void) {
    if (g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE) {
        return "Unknown Gametype";
    }
    return gametypeDisplayNames[g_gametype.integer];
}

/*
=================
Cmd_DropFlag_f

[QL] 0x10044910. The flag is set down ahead (G_ThrowFlag); with g_throwFlagVelocity it is thrown
from the carrier, where the view points, g_throwFlagForwardMult times that speed.
=================
*/
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

/*
=================
Cmd_DropPowerup_f

[QL] 0x10044ae0. Every powerup the player runs with is set down 64 units ahead, if there is no
wall in the way; flags and runes stay. Somebody else has ten seconds to take them, and with
g_droppedPowerupsDecay what is left keeps running out on the ground.
=================
*/
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
            // gone when it would have run out, or in ten seconds
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

/*
=================
Cmd_DropRune_f

[QL] 0x10044f10. The rune is set down 64 units ahead, standing, if there is no wall in the way.
Somebody else has ten seconds to take it; after that it goes back to a spawn point.
=================
*/
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

/*
=================
Cmd_DropWeapon_f

[QL] 0x10045250. The weapon in hand, if it has ammo, is set down 64 units ahead with as much of
it as the weapon comes with, if there is no wall in the way. The player goes to the next weapon
below it they have.
=================
*/
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

/*
=================
ClientCommand
=================
*/
void ClientCommand(int clientNum) {
    gentity_t* ent;
    char cmd[MAX_TOKEN_CHARS];

    ent = g_entities + clientNum;
    if (!ent->client || ent->client->pers.connected != CON_CONNECTED) {
        if (ent->client && ent->client->pers.localClient) {
            // Handle early team command sent by UI when starting a local
            // team play game.
            trap_Argv(0, cmd, sizeof(cmd));
            if (Q_stricmp(cmd, "team") == 0) {
                Cmd_Team_f(ent);
            }
        }
        return;  // not fully in game yet
    }

    trap_Argv(0, cmd, sizeof(cmd));

    // [QL] Check permission-table commands first
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
    // [QL] botsay is dispatched pre-intermission, right after tell
    if (Q_stricmp(cmd, "botsay") == 0) {
        Cmd_BotSay_f(ent);
        return;
    }
    // [QL] the voice commands, dispatched as in qagamex86.dll 0x10045ed2-0x10045f54
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
    // [QL] acc = score + accuracy (works during intermission)
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
    // [QL] ragequit and vote work at intermission too, so they sit before the gate
    if (Q_stricmp(cmd, "ragequit") == 0) {
        trap_DropClient(clientNum, "ragequit");
        return;
    }
    if (Q_stricmp(cmd, "vote") == 0) {
        Cmd_Vote_f(ent);
        return;
    }

    // everything below runs only during play, not at (or heading to)
    // intermission. Unlike Q3, QL does not fall back to a say command here.
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
    // [QL] drop commands
    else if (Q_stricmp(cmd, "dropflag") == 0)
        Cmd_DropFlag_f(ent);
    else if (Q_stricmp(cmd, "droppowerup") == 0)
        Cmd_DropPowerup_f(ent);
    else if (Q_stricmp(cmd, "droprune") == 0)
        Cmd_DropRune_f(ent);
    else if (Q_stricmp(cmd, "dropweapon") == 0)
        Cmd_DropWeapon_f(ent);
    // [QL] additional commands
    else if (Q_stricmp(cmd, "spec") == 0)
        Cmd_Spec_f(ent);  // [QL] filtered follow (stats/red/blue/powerup)
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
