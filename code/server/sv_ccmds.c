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
#include "../qcommon/q_steam.h"

static client_t* SV_GetPlayerByHandle(void) {
    client_t* cl;
    int i;
    char* s;
    char cleanName[64];

    if (!com_sv_running->integer) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No player specified.\n");
        return NULL;
    }

    s = Cmd_Argv(1);

    for (i = 0; s[i] >= '0' && s[i] <= '9'; i++)
        ;

    if (!s[i]) {
        int plid = atoi(s);

        if (plid >= 0 && plid < sv_maxclients->integer) {
            cl = &svs.clients[plid];

            if (cl->state)
                return cl;
        }
    }

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

static client_t* SV_GetPlayerByNum(void) {
    client_t* cl;
    int i;
    int idnum;
    char* s;

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

static void SV_Map_f(void) {
    char cmd[1024];
    char* map;
    const char* factoryName;
    factory_t* factory;
    const arenaInfo_t* arena;
    qboolean isMap, hidden;
    char expanded[MAX_QPATH];
    char mapname[MAX_QPATH];

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

    arena = SV_FindArena(mapname);
    if (arena && isMap && !arena->gametypes[factory->basegt] && factoryName[0] != '_') {
        Com_Printf("Map not supported for this gametype.\n");
        return;
    }

    SV_ApplyFactory(factory);

    Cvar_Get("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH);
    Cvar_Get("g_ammoPack", "0", CVAR_LATCH);

    SV_SpawnServer(mapname, !isMap);

    Cvar_Set("sv_cheats", isMap ? "0" : "1");

    if (!com_dedicated->integer) {
        Cvar_Set("ui_singlePlayerActive", "1");
        Cvar_Set("ui_priv", "3");
    }
}

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

static void SV_MapRestart_f(void) {
    int i;
    client_t* client;
    char* denied;
    qboolean isBot;
    int delay;

    if (com_frameTime == sv.serverId) {
        return;
    }

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

    if (!Cvar_VariableValue("g_restarted")) {
        if (sv_ammoPack->modified || sv_gametype->modified) {
            char mapname[MAX_QPATH];

            Com_Printf("variable change -- restarting.\n");
            Q_strncpyz(mapname, Cvar_VariableString("mapname"), sizeof(mapname));

            SV_SpawnServer(mapname, qfalse);
            return;
        }
        if (sv_maxclients->modified) {
            SV_ChangeMaxClients();
        }
    }

    svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

    sv.serverId = com_frameTime;
    Cvar_Set("sv_serverid", va("%i", sv.serverId));

    Cvar_Set("g_levelStartTime", va("%i", (int)time(NULL)));

    for (i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state == CS_PRIMED) {
            svs.clients[i].oldServerTime = sv.restartTime;
        }
    }

    sv.state = SS_LOADING;
    sv.restarting = qtrue;

    SV_RestartGameProgs();

    for (i = 0; i < 3; i++) {
        SV_GameRunFrame(sv.time);
        sv.time += 100;
        svs.time += 100;
    }

    sv.state = SS_GAME;
    sv.restarting = qfalse;

    for (i = 0; i < sv_maxclients->integer; i++) {
        client = &svs.clients[i];

        if (client->state < CS_CONNECTED) {
            continue;
        }

        if (client->netchan.remoteAddress.type == NA_BOT) {
            isBot = qtrue;
        } else {
            isBot = qfalse;
        }

        SV_AddServerCommand(client, "map_restart\n");

        denied = SV_GameClientConnect(i, qfalse, isBot);
        if (denied) {
            SV_DropClient(client, denied);
            Com_Printf("SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i);
            continue;
        }

        if (client->state == CS_ACTIVE)
            SV_ClientEnterWorld(client, &client->lastUsercmd);
        else {
            SV_ClientEnterWorld(client, NULL);
        }
    }

    SV_GameRunFrame(sv.time);
    sv.time += 100;
    svs.time += 100;

    SV_SetNextMap();
    SV_SetNextMaps();
}

static void SV_Kick_f(void) {
    client_t* cl;
    const char* reason;
    int i;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() == 1) {
        Com_Printf("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
        return;
    }

    reason = "was kicked";
    if (Cmd_Argc() > 2 && strlen(Cmd_Argv(2)) < 128) {
        reason = va("was kicked: %s", Cmd_Argv(2));
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
                SV_DropClient(cl, reason);
                cl->lastPacketTime = svs.time;
            }
        } else if (!Q_stricmp(Cmd_Argv(1), "allbots")) {
            for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
                if (!cl->state) {
                    continue;
                }
                if (cl->netchan.remoteAddress.type != NA_BOT) {
                    continue;
                }
                SV_DropClient(cl, reason);
                cl->lastPacketTime = svs.time;
            }
        }
        return;
    }
    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player.\n");
        return;
    }

    SV_DropClient(cl, reason);
    cl->lastPacketTime = svs.time;
}

static void SV_KickNum_f(void) {
    client_t* cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: kicknum <client number>\n");
        return;
    }

    cl = SV_GetPlayerByNum();
    if (!cl) {
        return;
    }
    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
        return;
    }

    SV_DropClient(cl, "was kicked");
    cl->lastPacketTime = svs.time;
}

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

static void SV_Status_f(void) {
    int i, j, l;
    client_t* cl;
    playerState_t* ps;
    const char* s;
    int ping;

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

static void SV_ConSay_f(void) {
    char* p;
    char text[1024];

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

static void SV_Serverinfo_f(void) {
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    Com_Printf("Server info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SERVERINFO));
}

static void SV_DumpUser_f(void) {
    client_t* cl;

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

static void SV_KillServer_f(void) {
    SV_Shutdown("killserver");
}

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

static void SV_CompleteArenaName(char* args, int argNum) {
    if (argNum == 2) {
        Field_CompleteFilename("scripts", "sp_arena", qtrue, qfalse);
    }
}

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
    Cmd_AddCommand("steam_downloadugc", Steam_DownloadUGC_f);
#ifndef DEDICATED
    Cmd_AddCommand("steam_subscribeugc", Steam_SubscribeUGC_f);
    Cmd_AddCommand("steam_unsubscribeugc", Steam_UnsubscribeUGC_f);
#endif
    if (com_dedicated->integer) {
        Cmd_AddCommand("say", SV_ConSay_f);
    }

    Cmd_AddCommand("arena", SV_Arena_f);
    Cmd_SetCommandCompletionFunc("arena", SV_CompleteArenaName);
    Cmd_AddCommand("startRandomMap", SV_StartRandomMap_f);
    Cmd_AddCommand("reload_mappool", SV_ReloadMapPool_f);
    Cmd_AddCommand("reload_arenas", SV_ReloadArenas_f);
    Cmd_AddCommand("reload_factories", SV_ReloadFactories_f);
}

void SV_RemoveOperatorCommands(void) {
#if 0
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
