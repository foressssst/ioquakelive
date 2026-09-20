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

#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
static client_t* SV_GetPlayerByHandle(void) {
    client_t* cl;
    int i;
    char* s;
    char cleanName[64];

    // make sure server is running
    if (!com_sv_running->integer) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No player specified.\n");
        return NULL;
    }

    s = Cmd_Argv(1);

    // Check whether this is a numeric player handle
    for (i = 0; s[i] >= '0' && s[i] <= '9'; i++)
        ;

    if (!s[i]) {
        int plid = atoi(s);

        // Check for numeric playerid match
        if (plid >= 0 && plid < sv_maxclients->integer) {
            cl = &svs.clients[plid];

            if (cl->state)
                return cl;
        }
    }

    // check for a name match
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (!cl->state) {
            continue;
        }
        if (!Q_stricmp(cl->name, s)) {
            return cl;
        }

        Q_strncpyz(cleanName, cl->name, sizeof(cleanName));
        Q_CleanStr(cleanName);
        if (!Q_stricmp(cleanName, s)) {
            return cl;
        }
    }

    Com_Printf("Player %s is not on the server\n", s);

    return NULL;
}

/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t* SV_GetPlayerByNum(void) {
    client_t* cl;
    int i;
    int idnum;
    char* s;

    // make sure server is running
    if (!com_sv_running->integer) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No player specified.\n");
        return NULL;
    }

    s = Cmd_Argv(1);

    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') {
            Com_Printf("Bad slot number: %s\n", s);
            return NULL;
        }
    }
    idnum = atoi(s);
    if (idnum < 0 || idnum >= sv_maxclients->integer) {
        Com_Printf("Bad client slot: %i\n", idnum);
        return NULL;
    }

    cl = &svs.clients[idnum];
    if (!cl->state) {
        Com_Printf("Client %i is not active\n", idnum);
        return NULL;
    }
    return cl;
}

//=========================================================

/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f(void) {
    char cmd[1024];
    char* map;
    const char* factoryName;
    factory_t* factory;
    const arenaInfo_t* arena;
    qboolean isMap, hidden;
    char expanded[MAX_QPATH];
    char mapname[MAX_QPATH];

    // [QL] "map <map> <factory>" (quakelive_steam.exe 0x4ddce0). The factory can
    // be left out once one is in play.
    map = Cmd_Argv(1);
    factoryName = Cmd_Argv(2);
    Q_strncpyz(cmd, Cmd_Argv(0), sizeof(cmd));
    Q_strncpyz(mapname, map, sizeof(mapname));
    if (!map) {
        return;
    }

    if (Cmd_Argc() < 3 - (svs.factory != NULL)) {
        Com_Printf("%s (map) (factory)\n", Cmd_Argv(0));
        SV_PrintValidFactories();
        return;
    }

    // make sure the level exists before trying to change, so that
    // a typo at the server console won't end the game
    Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);
    if (FS_ReadFile(expanded, NULL) == -1) {
        Com_Printf("Can't find map %s\n", expanded);
        return;
    }

    isMap = Q_stricmp(cmd, "devmap") != 0;

    if (!svs.factory || factoryName[0]) {
        factory = SV_FindFactory(factoryName);
    } else {
        factory = svs.factory;
    }

    // the hidden factories ("_training", "_rj", "_sj") are for offline servers
    if (svs.factory && svs.factory->id[0] == '_' && !factoryName[0]) {
        hidden = qtrue;
    } else {
        hidden = factoryName[0] == '_';
    }
    if (hidden && sv_serverType->integer > 0) {
        Com_Printf("%s (map) (factory)\n", Cmd_Argv(0));
        SV_PrintValidFactories();
        return;
    }

    if (!factory) {
        Com_Printf("Invalid factory specified.\n");
        return;
    }

    // devmap and the hidden factories skip the arena's gametypes
    arena = SV_FindArena(mapname);
    if (arena && isMap && !arena->gametypes[factory->basegt] && factoryName[0] != '_') {
        Com_Printf("Map not supported for this gametype.\n");
        return;
    }

    SV_ApplyFactory(factory);

    // force latched values to get set
    Cvar_Get("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH);
    Cvar_Get("g_ammoPack", "0", CVAR_LATCH);

    // start up the map
    SV_SpawnServer(mapname, !isMap);

    // set the cheat value
    // if the level was started with "map <levelname>", then
    // cheats will not be allowed.  If started with "devmap <levelname>"
    // then cheats will be allowed
    Cvar_Set("sv_cheats", isMap ? "0" : "1");

    if (!com_dedicated->integer) {
        Cvar_Set("ui_singlePlayerActive", "1");
        Cvar_Set("ui_priv", "3");
    }
}

/*
==================
SV_Arena_f

[QL] "arena <name>": a single player match from scripts/<name>.sp_arena, with
its map, fraglimit, timelimit, bot_skill and bots, as Free For All with cheats
off. quakelive_steam.exe 0x4de050.
==================
*/
static void SV_Arena_f(void) {
    const char* arenaName;
    char filename[MAX_QPATH];
    char expanded[MAX_QPATH];
    char map[MAX_QPATH];
    char bots[1024];
    char buf[1024];
    char* text;
    char* token;
    fileHandle_t f;
    int len;
    int fraglimit = 20;
    int timelimit = 0;
    int skill;
    qboolean done = qfalse;

    arenaName = Cmd_Argv(1);
    skill = Cvar_VariableIntegerValue("g_spSkill");
    // the engine tested the pointer, so an empty name reaches the file lookup
    if (!arenaName) {
        Com_Printf("No arena name passed\n");
        return;
    }

    Com_sprintf(filename, sizeof(filename), "scripts/%s.sp_arena", arenaName);
    len = FS_FOpenFileRead(filename, &f, qtrue);
    if (!f) {
        Com_Printf("%s", va("^1file not found: %s\n", filename));
        return;
    }
    if (len > 0x3ff) {
        Com_Printf("%s", va("^1file too large: %s is %i, max allowed is %i", filename, len, 0x400));
        FS_FCloseFile(f);
        return;
    }
    FS_Read(buf, len, f);
    buf[len] = '\0';
    FS_FCloseFile(f);

    text = buf;
    map[0] = '\0';
    bots[0] = '\0';
    token = COM_Parse(&text);
    while (1) {
        if (done) {
            Cvar_SetValue("g_gametype", 0);
            Cvar_SetValue("g_spSkill", (float)skill);
            Cvar_SetValue("fraglimit", (float)fraglimit);
            Cvar_SetValue("timelimit", (float)timelimit);
            if (bots[0]) {
                Cvar_Set("g_botSpawnList", bots);
            }
            SV_SpawnServer(map, qtrue);
            Cvar_SetValue("sv_cheats", 0);
            return;
        }
        if (!token[0]) {
            Com_Printf("%s is empty!\n", filename);
            return;
        }
        if (strcmp(token, "{")) {
            Com_Printf("Missing initial \"{\" in %s!\n", filename);
            return;
        }

        token = COM_ParseExt(&text, qtrue);
        while (token[0]) {
            if (!strcmp(token, "}")) {
                done = qtrue;
                break;
            }
            if (!strcmp(token, "fraglimit")) {
                token = COM_ParseExt(&text, qfalse);
                if (!token[0]) {
                    Com_Printf("No matching value given for fraglimit!\n");
                    return;
                }
                fraglimit = atoi(token);
                if (fraglimit < 1) {
                    Com_Printf("Fraglimit was an invalid value! Valid values are 1 or greater. Setting to 20.\n");
                    fraglimit = 20;
                }
            } else if (!strcmp(token, "timelimit")) {
                token = COM_ParseExt(&text, qfalse);
                if (!token[0]) {
                    Com_Printf("No matching value given for timelimit!\n");
                    return;
                }
                timelimit = atoi(token);
                if (timelimit < 0) {
                    Com_Printf("timelimit was an invalid value! Valid values are 0 or greater. Setting to 0.\n");
                    timelimit = 0;
                }
            } else if (!strcmp(token, "bot_skill")) {
                token = COM_ParseExt(&text, qfalse);
                if (!token[0]) {
                    Com_Printf("No matching value given for botSkill!\n");
                    return;
                }
                skill = atoi(token);
                if (skill < 1 || skill > 5) {
                    Com_Printf("bot_skill was an invalid value! Valid values are 1 - 5. Setting to 4.\n");
                    skill = 4;
                }
            } else if (!strcmp(token, "map")) {
                token = COM_ParseExt(&text, qfalse);
                if (!token[0]) {
                    Com_Printf("No map name given!\n");
                    return;
                }
                Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", token);
                if (FS_ReadFile(expanded, NULL) == -1) {
                    Com_Printf("Can't find map %s!\n", expanded);
                    return;
                }
                Q_strncpyz(map, token, sizeof(map));
            } else if (!strcmp(token, "bots")) {
                token = COM_ParseExt(&text, qfalse);
                if (!token[0]) {
                    Com_Printf("No bot names given for bots!\n");
                    return;
                }
                Q_strncpyz(bots, token, sizeof(bots));
            } else {
                Com_Printf("Unknown value in %s\n", filename);
                return;
            }
            token = COM_ParseExt(&text, qtrue);
        }
        if (!done) {
            Com_Printf("Unexpected end of info file!\n");
            return;
        }
        token = COM_Parse(&text);
    }
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f(void) {
    int i;
    client_t* client;
    char* denied;
    qboolean isBot;
    int delay;

    // make sure we aren't restarting twice in the same frame
    if (com_frameTime == sv.serverId) {
        return;
    }

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (sv.restartTime) {
        return;
    }

    if (Cmd_Argc() > 1) {
        delay = atoi(Cmd_Argv(1));
    } else {
        delay = 5;
    }
    if (delay && !Cvar_VariableValue("g_doWarmup")) {
        char warmupInfo[MAX_INFO_STRING] = {0};
        int gametype;

        sv.restartTime = sv.time + delay * 1000;

        // [QL] Build info string with time and gametype keys; the gametype is the
        // latched one when there is one (quakelive_steam.exe 0x4de730)
        Info_SetValueForKey(warmupInfo, "time", va("%i", sv.restartTime));
        if (sv_gametype->latchedString) {
            gametype = atoi(sv_gametype->latchedString);
        } else {
            gametype = sv_gametype->integer;
        }
        Info_SetValueForKey(warmupInfo, "g_gametype", va("%d", gametype));
        SV_SetConfigstring(CS_WARMUP, warmupInfo);
        return;
    }

    // [QL] Skip variable-change checks when game triggered the restart (g_restarted)
    if (!Cvar_VariableValue("g_restarted")) {
        // check for changes in variables that can't just be restarted
        // [QL] g_ammoPack and g_gametype respawn the map; sv_maxclients only
        // resizes the client table (quakelive_steam.exe 0x4de800)
        if (sv_ammoPack->modified || sv_gametype->modified) {
            char mapname[MAX_QPATH];

            Com_Printf("variable change -- restarting.\n");
            // restart the map the slow way
            Q_strncpyz(mapname, Cvar_VariableString("mapname"), sizeof(mapname));

            SV_SpawnServer(mapname, qfalse);
            return;
        }
        if (sv_maxclients->modified) {
            SV_ChangeMaxClients();
        }
    }

    // toggle the server bit so clients can detect that a
    // map_restart has happened
    svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

    // generate a new serverid
    sv.serverId = com_frameTime;
    Cvar_Set("sv_serverid", va("%i", sv.serverId));

    // [QL] Record level start wall-clock time
    Cvar_Set("g_levelStartTime", va("%i", (int)time(NULL)));

    // if a map_restart occurs while a client is changing maps, we need
    // to give them the correct time so that when they finish loading
    // they don't violate the backwards time check in cl_cgame.c
    for (i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state == CS_PRIMED) {
            svs.clients[i].oldServerTime = sv.restartTime;
        }
    }

    // reset all the vm data in place without changing memory allocation
    // note that we do NOT set sv.state = SS_LOADING, so configstrings that
    // had been changed from their default values will generate broadcast updates
    sv.state = SS_LOADING;
    sv.restarting = qtrue;

    SV_RestartGameProgs();

    // run a few frames to allow everything to settle
    for (i = 0; i < 3; i++) {
        SV_GameRunFrame(sv.time);
        sv.time += 100;
        svs.time += 100;
    }

    sv.state = SS_GAME;
    sv.restarting = qfalse;

    // connect and begin all the clients
    for (i = 0; i < sv_maxclients->integer; i++) {
        client = &svs.clients[i];

        // send the new gamestate to all connected clients
        if (client->state < CS_CONNECTED) {
            continue;
        }

        if (client->netchan.remoteAddress.type == NA_BOT) {
            isBot = qtrue;
        } else {
            isBot = qfalse;
        }

        // add the map_restart command
        SV_AddServerCommand(client, "map_restart\n");

        // connect the client again, without the firstTime flag
        denied = SV_GameClientConnect(i, qfalse, isBot);
        if (denied) {
            // this generally shouldn't happen, because the client
            // was connected before the level change
            SV_DropClient(client, denied);
            Com_Printf("SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i);
            continue;
        }

        if (client->state == CS_ACTIVE)
            SV_ClientEnterWorld(client, &client->lastUsercmd);
        else {
            // If we don't reset client->lastUsercmd and are restarting during map load,
            // the client will hang because we'll use the last Usercmd from the previous map,
            // which is wrong obviously.
            SV_ClientEnterWorld(client, NULL);
        }
    }

    // run another frame to allow things to look at all the players
    SV_GameRunFrame(sv.time);
    sv.time += 100;
    svs.time += 100;

    // [QL] quakelive_steam.exe 0x4dea1a
    SV_SetNextMap();
    SV_SetNextMaps();
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
static void SV_Kick_f(void) {
    client_t* cl;
    int i;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        if (!Q_stricmp(Cmd_Argv(1), "all")) {
            for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
                if (!cl->state) {
                    continue;
                }
                if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
                    continue;
                }
                SV_DropClient(cl, "was kicked");
                cl->lastPacketTime = svs.time;  // in case there is a funny zombie
            }
        } else if (!Q_stricmp(Cmd_Argv(1), "allbots")) {
            for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
                if (!cl->state) {
                    continue;
                }
                if (cl->netchan.remoteAddress.type != NA_BOT) {
                    continue;
                }
                SV_DropClient(cl, "was kicked");
                cl->lastPacketTime = svs.time;  // in case there is a funny zombie
            }
        }
        return;
    }
    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        Com_Printf("Cannot kick host player\n");
        return;
    }

    SV_DropClient(cl, "was kicked");
    cl->lastPacketTime = svs.time;  // in case there is a funny zombie
}

/*
==================
SV_KickNum_f

Kick a user off of the server
==================
*/
static void SV_KickNum_f(void) {
    client_t* cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <client number>\n", Cmd_Argv(0));
        return;
    }

    cl = SV_GetPlayerByNum();
    if (!cl) {
        return;
    }
    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        Com_Printf("Cannot kick host player\n");
        return;
    }

    SV_DropClient(cl, "was kicked");
    cl->lastPacketTime = svs.time;  // in case there is a funny zombie
}

/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f(void) {
    client_t* cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: banUser <player name>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();

    if (!cl) {
        return;
    }

    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        Com_Printf("Cannot kick host player\n");
        return;
    }

    Com_Printf("Not yet implemented.\n");
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f(void) {
    client_t* cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: banClient <client number>\n");
        return;
    }

    cl = SV_GetPlayerByNum();
    if (!cl) {
        return;
    }
    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        Com_Printf("Cannot kick host player\n");
        return;
    }

    Com_Printf("Not yet implemented.\n");
}

/*
** SV_Strlen -- skips color escape codes
*/
static int SV_Strlen(const char* str) {
    const char* s = str;
    int count = 0;

    while (*s) {
        if (Q_IsColorString(s)) {
            s += 2;
        } else {
            count++;
            s++;
        }
    }

    return count;
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f(void) {
    int i, j, l;
    client_t* cl;
    playerState_t* ps;
    const char* s;
    int ping;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    Com_Printf("map: %s\n", sv_mapname->string);

    Com_Printf("cl score ping name            address                                 rate \n");
    Com_Printf("-- ----- ---- --------------- --------------------------------------- -----\n");
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (!cl->state)
            continue;
        Com_Printf("%2i ", i);
        ps = SV_GameClientNum(i);
        Com_Printf("%5i ", ps->persistant[PERS_SCORE]);

        if (cl->state == CS_CONNECTED)
            Com_Printf("CON ");
        else if (cl->state == CS_ZOMBIE)
            Com_Printf("ZMB ");
        else {
            ping = cl->ping < 9999 ? cl->ping : 9999;
            Com_Printf("%4i ", ping);
        }

        Com_Printf("%s", cl->name);

        l = 16 - SV_Strlen(cl->name);
        j = 0;

        do {
            Com_Printf(" ");
            j++;
        } while (j < l);

        // TTimo adding a ^7 to reset the color
        s = NET_AdrToString(cl->netchan.remoteAddress);
        Com_Printf("^7%s", s);
        l = 39 - strlen(s);
        j = 0;

        do {
            Com_Printf(" ");
            j++;
        } while (j < l);

        Com_Printf(" %5i", cl->rate);

        Com_Printf("\n");
    }
    Com_Printf("\n");
}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void) {
    char* p;
    char text[1024];

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        return;
    }

    strcpy(text, "console: ");
    p = Cmd_Args();

    if (*p == '"') {
        p++;
        p[strlen(p) - 1] = 0;
    }

    strcat(text, p);

    Com_Printf("%s\n", text);
    SV_SendServerCommand(NULL, "chat \"%s\"", text);
}

/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f(void) {
    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    Com_Printf("Server info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SERVERINFO));
}

/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f(void) {
    client_t* cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: dumpuser <userid>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    Com_Printf("userinfo\n");
    Com_Printf("--------\n");
    Info_Print(cl->userinfo);
}

/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f(void) {
    SV_Shutdown("killserver");
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
/*
==================
SV_CompleteMapName

"map <map> <factory>". The map comes from the parsed arenas and not from maps/*.bsp: a
.bsp without its .arena cannot be loaded, and there are arenas whose name is not the
.bsp's. The factory comes from scripts/factories.txt, minus the ones starting with "_",
which are internal templates -- the same filter the list "map" prints with no arguments
uses.
==================
*/
static void SV_CompleteMapName(char* args, int argNum) {
    static const char* names[MAX_COMPLETION_NAMES];
    int count;

    if (argNum == 2) {
        count = SV_ArenaNameList(names, ARRAY_LEN(names));
    } else if (argNum == 3) {
        count = SV_FactoryNameList(names, ARRAY_LEN(names));
    } else {
        return;
    }

    Field_CompleteList(names, count);
}

/*
==================
SV_CompletePlayerName

For "kick", "clientkick" and "dumpuser", which take a player's name.
==================
*/
static void SV_CompletePlayerName(char* args, int argNum) {
    if (argNum == 2) {
        char names[MAX_CLIENTS][MAX_NAME_LENGTH];
        const char* namesPtr[MAX_CLIENTS];
        client_t* cl;
        int i;
        int nameCount;
        int clientCount;

        nameCount = 0;
        clientCount = sv_maxclients->integer;

        for (i = 0, cl = svs.clients; i < clientCount; i++, cl++) {
            if (!cl->state) {
                continue;
            }
            if (i >= MAX_CLIENTS) {
                break;
            }
            Q_strncpyz(names[nameCount], cl->name, sizeof(names[nameCount]));
            Q_CleanStr(names[nameCount]);

            namesPtr[nameCount] = names[nameCount];

            nameCount++;
        }
        qsort((void*)namesPtr, nameCount, sizeof(namesPtr[0]), Com_strCompare);

        Field_CompletePlayerName(namesPtr, nameCount);
    }
}

/*
==================
SV_CompleteArenaName

"arena <name>" loads scripts/<name>.sp_arena.
==================
*/
static void SV_CompleteArenaName(char* args, int argNum) {
    if (argNum == 2) {
        Field_CompleteFilename("scripts", "sp_arena", qtrue, qfalse);
    }
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands(void) {
    static qboolean initialized;

    if (initialized) {
        return;
    }
    initialized = qtrue;

    Cmd_AddCommand("kick", SV_Kick_f);
    Cmd_SetCommandCompletionFunc("kick", SV_CompletePlayerName);
    Cmd_AddCommand("clientkick", SV_KickNum_f);
    Cmd_SetCommandCompletionFunc("clientkick", SV_CompletePlayerName);
    Cmd_AddCommand("status", SV_Status_f);
    Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
    Cmd_AddCommand("dumpuser", SV_DumpUser_f);
    Cmd_SetCommandCompletionFunc("dumpuser", SV_CompletePlayerName);
    Cmd_AddCommand("map_restart", SV_MapRestart_f);
    Cmd_AddCommand("sectorlist", SV_SectorList_f);
    Cmd_AddCommand("map", SV_Map_f);
    Cmd_SetCommandCompletionFunc("map", SV_CompleteMapName);
    Cmd_AddCommand("devmap", SV_Map_f);
    Cmd_SetCommandCompletionFunc("devmap", SV_CompleteMapName);
    Cmd_AddCommand("killserver", SV_KillServer_f);
    if (com_dedicated->integer) {
        Cmd_AddCommand("say", SV_ConSay_f);
    }

    // [QL] arenas, factories and the map pool (sv_factories.c)
    Cmd_AddCommand("arena", SV_Arena_f);
    Cmd_SetCommandCompletionFunc("arena", SV_CompleteArenaName);
    Cmd_AddCommand("startRandomMap", SV_StartRandomMap_f);
    Cmd_AddCommand("reload_mappool", SV_ReloadMapPool_f);
    Cmd_AddCommand("reload_arenas", SV_ReloadArenas_f);
    Cmd_AddCommand("reload_factories", SV_ReloadFactories_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands(void) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("kick");
	Cmd_RemoveCommand ("clientkick");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
	Cmd_RemoveCommand ("map");
	Cmd_RemoveCommand ("devmap");
	Cmd_RemoveCommand ("killserver");
	Cmd_RemoveCommand ("say");
	Cmd_RemoveCommand ("arena");
	Cmd_RemoveCommand ("startRandomMap");
	Cmd_RemoveCommand ("reload_mappool");
	Cmd_RemoveCommand ("reload_arenas");
	Cmd_RemoveCommand ("reload_factories");
#endif
}
