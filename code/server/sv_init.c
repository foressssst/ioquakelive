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
#include "../qcommon/q_zmq.h"

static void SV_SendConfigstring(client_t* client, int index) {
    int maxChunkSize = MAX_STRING_CHARS - 24;
    int len;

    len = strlen(sv.configstrings[index]);

    if (len >= maxChunkSize) {
        int sent = 0;
        int remaining = len;
        char* cmd;
        char buf[MAX_STRING_CHARS];

        while (remaining > 0) {
            if (sent == 0) {
                cmd = "bcs0";
            } else if (remaining < maxChunkSize) {
                cmd = "bcs2";
            } else {
                cmd = "bcs1";
            }
            Q_strncpyz(buf, &sv.configstrings[index][sent],
                       maxChunkSize);

            SV_SendServerCommand(client, "%s %i \"%s\"\n", cmd,
                                 index, buf);

            sent += (maxChunkSize - 1);
            remaining -= (maxChunkSize - 1);
        }
    } else {
        SV_SendServerCommand(client, "cs %i \"%s\"\n", index,
                             sv.configstrings[index]);
    }
}

void SV_UpdateConfigstrings(client_t* client) {
    int index;

    for (index = 0; index < MAX_CONFIGSTRINGS; index++) {
        if (!client->csUpdated[index]) {
            continue;
        }
        client->csUpdated[index] = qfalse;

        if (index == CS_SERVERINFO && client->gentity &&
            (client->gentity->r.svFlags & SVF_NOSERVERINFO)) {
            continue;
        }
        SV_SendConfigstring(client, index);
    }
}

void SV_SetConfigstring(int index, const char* val) {
    int i;
    client_t* client;

    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP, "SV_SetConfigstring: bad index %i", index);
    }

    if (!val) {
        val = "";
    }

    if (!sv.configstrings[index]) {
        sv.configstrings[index] = CopyString(val);
        return;
    }

    if (!strcmp(val, sv.configstrings[index])) {
        return;
    }

    Z_Free(sv.configstrings[index]);
    sv.configstrings[index] = CopyString(val);

    if (sv.state == SS_GAME || sv.restarting) {
        for (i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++) {
            if (client->state < CS_PRIMED) {
                continue;
            }
            if (index == CS_SERVERINFO && client->gentity && (client->gentity->r.svFlags & SVF_NOSERVERINFO)) {
                continue;
            }

            if (client->state == CS_PRIMED) {
                client->csUpdated[index] = qtrue;
                continue;
            }

            SV_SendConfigstring(client, index);
        }
    }
}

void SV_GetConfigstring(int index, char* buffer, int bufferSize) {
    if (bufferSize < 1) {
        Com_Error(ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize);
    }
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP, "SV_GetConfigstring: bad index %i", index);
    }
    if (!sv.configstrings[index]) {
        buffer[0] = 0;
        return;
    }

    Q_strncpyz(buffer, sv.configstrings[index], bufferSize);
}

void SV_SetUserinfo(int index, const char* val) {
    if (index < 0 || index >= sv_maxclients->integer) {
        Com_Error(ERR_DROP, "SV_SetUserinfo: bad index %i", index);
    }

    if (!val) {
        val = "";
    }

    Q_strncpyz(svs.clients[index].userinfo, val, sizeof(svs.clients[index].userinfo));
    Q_strncpyz(svs.clients[index].name, Info_ValueForKey(val, "name"), sizeof(svs.clients[index].name));
}

void SV_GetUserinfo(int index, char* buffer, int bufferSize) {
    if (bufferSize < 1) {
        Com_Error(ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize);
    }
    if (index < 0 || index >= sv_maxclients->integer) {
        Com_Error(ERR_DROP, "SV_GetUserinfo: bad index %i", index);
    }
    Q_strncpyz(buffer, svs.clients[index].userinfo, bufferSize);
}

static void SV_CreateBaseline(void) {
    sharedEntity_t* svent;
    int entnum;

    for (entnum = 1; entnum < sv.num_entities; entnum++) {
        svent = SV_GentityNum(entnum);
        if (!svent->r.linked) {
            continue;
        }
        svent->s.number = entnum;

        sv.svEntities[entnum].baseline = svent->s;
    }
}

static void SV_BoundMaxClients(int minimum) {
    Cvar_Get("sv_maxclients", "8", 0);

    sv_maxclients->modified = qfalse;

    if (sv_maxclients->integer < minimum) {
        Cvar_Set("sv_maxclients", va("%i", minimum));
    } else if (sv_maxclients->integer > MAX_CLIENTS) {
        Cvar_Set("sv_maxclients", va("%i", MAX_CLIENTS));
    }
}

static void SV_Startup(void) {
    if (svs.initialized) {
        Com_Error(ERR_FATAL, "SV_Startup: svs.initialized");
    }
    SV_BoundMaxClients(1);

    svs.clients = Z_Malloc(sizeof(client_t) * sv_maxclients->integer);
    if (com_dedicated->integer) {
        svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES;
    } else {
        svs.numSnapshotEntities = sv_maxclients->integer * 4 * MAX_SNAPSHOT_ENTITIES;
    }
    svs.initialized = qtrue;

    if (sv_killserver->integer) {
        Cvar_Set("sv_killserver", "0");
    }

    Cvar_Set("sv_running", "1");

    NET_JoinMulticast6();
}

void SV_ChangeMaxClients(void) {
    int oldMaxClients;
    int i;
    client_t* oldClients;
    int count;

    count = 0;
    for (i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            if (i > count)
                count = i;
        }
    }
    count++;

    oldMaxClients = sv_maxclients->integer;
    SV_BoundMaxClients(count);
    if (sv_maxclients->integer == oldMaxClients) {
        return;
    }

    oldClients = Hunk_AllocateTempMemory(count * sizeof(client_t));
    for (i = 0; i < count; i++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            oldClients[i] = svs.clients[i];
        } else {
            Com_Memset(&oldClients[i], 0, sizeof(client_t));
        }
    }

    Z_Free(svs.clients);

    svs.clients = Z_Malloc(sv_maxclients->integer * sizeof(client_t));
    Com_Memset(svs.clients, 0, sv_maxclients->integer * sizeof(client_t));

    for (i = 0; i < count; i++) {
        if (oldClients[i].state >= CS_CONNECTED) {
            svs.clients[i] = oldClients[i];
        }
    }

    Hunk_FreeTempMemory(oldClients);

    if (com_dedicated->integer) {
        svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES;
    } else {
        svs.numSnapshotEntities = sv_maxclients->integer * 4 * MAX_SNAPSHOT_ENTITIES;
    }

    Steam_GS_SetMaxPlayers(sv_maxclients->integer);
}

static void SV_ClearServer(void) {
    int i;

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (sv.configstrings[i]) {
            Z_Free(sv.configstrings[i]);
        }
    }
    Com_Memset(&sv, 0, sizeof(sv));
}

void SV_SpawnServer(char* server, qboolean killBots) {
    int i;
    int checksum;
    qboolean isBot;
    char systemInfo[16384];
    const char* p;

    SV_ShutdownGameProgs();

    Com_Printf("------ Server Initialization ------\n");
    Com_Printf("Server: %s\n", server);

    CL_MapLoading();

    CL_ShutdownAll(qfalse);

    Hunk_Clear();

    CM_ClearMap();

    if (!Cvar_VariableValue("sv_running")) {
        SV_Startup();
    } else {
        if (sv_maxclients->modified) {
            SV_ChangeMaxClients();
        }
    }

    FS_ClearPakReferences(0);

    svs.snapshotEntities = Hunk_Alloc(sizeof(entityState_t) * svs.numSnapshotEntities, h_high);
    svs.nextSnapshotEntities = 0;

    svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

    Cvar_Set("nextmap", "map_restart 0");

    for (i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            svs.clients[i].oldServerTime = sv.time;
        }
    }

    SV_ClearServer();
    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        sv.configstrings[i] = CopyString("");
    }

    Cvar_Set("cl_paused", "0");

    sv.checksumFeed = (((unsigned int)rand() << 16) ^ (unsigned int)rand()) ^ Com_Milliseconds();
    FS_Restart(sv.checksumFeed);

    CM_LoadMap(va("maps/%s.bsp", server), qfalse, &checksum);

    Cvar_Set("mapname", server);

    Cvar_Set("sv_mapChecksum", va("%i", checksum));

    sv.serverId = com_frameTime;
    sv.restartedServerId = sv.serverId;
    sv.checksumFeedServerId = sv.serverId;
    Cvar_Set("sv_serverid", va("%i", sv.serverId));

    SV_ClearWorld();

    sv.state = SS_LOADING;

    SV_InitGameProgs();

    sv_ammoPack->modified = qfalse;
    sv_gametype->modified = qfalse;

    for (i = 0; i < 3; i++) {
        SV_GameRunFrame(sv.time);
        SV_BotFrame(sv.time);
        sv.time += 100;
        svs.time += 100;
    }

    SV_CreateBaseline();

    for (i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            char* denied;

            if (svs.clients[i].netchan.remoteAddress.type == NA_BOT) {
                if (killBots) {
                    SV_DropClient(&svs.clients[i], "");
                    continue;
                }
                isBot = qtrue;
            } else {
                isBot = qfalse;
            }

            denied = SV_GameClientConnect(i, qfalse, isBot);
            if (denied) {
                SV_DropClient(&svs.clients[i], denied);
            } else {
                if (!isBot) {
                    svs.clients[i].state = CS_CONNECTED;
                } else {
                    client_t* client;
                    sharedEntity_t* ent;

                    client = &svs.clients[i];
                    client->state = CS_ACTIVE;
                    ent = SV_GentityNum(i);
                    ent->s.number = i;
                    client->gentity = ent;

                    client->deltaMessage = -1;
                    client->nextSnapshotTime = 0;
                    SV_GameClientBegin(i);
                }
            }
        }
    }

    SV_GameRunFrame(sv.time);
    SV_BotFrame(sv.time);
    sv.time += 100;
    svs.time += 100;

    if (sv_pure->integer) {
        p = FS_LoadedPakChecksums();
        Cvar_Set("sv_paks", p);
        if (strlen(p) == 0) {
            Com_Printf("WARNING: sv_pure set but no PK3 files loaded\n");
        }
        p = FS_LoadedPakNames();
        Cvar_Set("sv_pakNames", p);
    } else {
        Cvar_Set("sv_paks", "");
        Cvar_Set("sv_pakNames", "");
    }
    p = FS_ReferencedPakChecksums();
    Cvar_Set("sv_referencedPaks", p);
    p = FS_ReferencedPakNames();
    Cvar_Set("sv_referencedPakNames", p);

    Q_strncpyz(systemInfo, Cvar_InfoString_Big(CVAR_SYSTEMINFO), sizeof(systemInfo));
    cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
    SV_SetConfigstring(CS_SYSTEMINFO, systemInfo);

    SV_SetConfigstring(CS_SERVERINFO, Cvar_InfoString(CVAR_SERVERINFO));
    cvar_modifiedFlags &= ~CVAR_SERVERINFO;

    ZMQ_StatsInit();

    SV_SteamIdentity();

    Steam_GS_SetKeyValues(Cvar_InfoString(CVAR_SERVERINFO));
    SV_SteamFrame(qtrue);

    sv.state = SS_GAME;

    Hunk_SetMark();

#ifndef DEDICATED
    if (com_dedicated->integer) {
        CL_StartHunkUsers(qtrue);
    }
#endif

    SV_SetNextMap();
    SV_SetNextMaps();

    Com_Printf("-----------------------------------\n");
}

#ifdef DEDICATED
void SV_WorkshopInit(void) {
    unsigned long long item;
    uint64_t done, total;
    fileHandle_t f;
    qboolean any = qfalse;
    char* buf;
    char* line;
    int next, now, len;

    if (!Steam_GS_Running()) {
        return;
    }
    next = Sys_Milliseconds() + 1000;
    while (!Steam_GS_LoggedOn()) {
        Steam_GS_RunCallbacks();
        now = Sys_Milliseconds();
        if (now > next) {
            next = now + 1000;
            Com_Printf("Waiting for steam servers\n");
        }
        Sys_Sleep(10);
    }

    len = FS_FOpenFileRead(sv_workshopFile->string, &f, qfalse);
    if (!f) {
        Com_Printf("No workshop items file: %s\n", sv_workshopFile->string);
        return;
    }
    buf = Z_Malloc(len + 1);
    FS_Read(buf, len, f);
    buf[len] = '\0';
    FS_FCloseFile(f);

    for (line = strtok(buf, "\n"); line; line = strtok(NULL, "\n")) {
        if (line[0] == '#') {
            continue;
        }
        if (sscanf(line, "%llu", &item) < 1) {
            Com_Printf("Parse fail, skip line: \"%s\"\n", line);
            continue;
        }
        any = qtrue;
        Steam_WorkshopQueue((uint64_t)item, qtrue);
    }
    Z_Free(buf);
    if (!any) {
        return;
    }

    Steam_WorkshopDone();
    next = Sys_Milliseconds() + 1000;
    while (!Steam_WorkshopDone()) {
        now = Sys_Milliseconds();
        if (now > next) {
            Steam_WorkshopProgress(Steam_WorkshopCurrent(), &done, &total);
            Com_Printf("Waiting on %i Workshop downloads: %0.2f%% (%llu bytes remain)\n", Steam_WorkshopPending(),
                       total ? (double)done / (double)total * 100.0 : 0.0, (unsigned long long)(total - done));
            next = now + 1000;
        }
        Steam_GS_RunCallbacks();
        Sys_Sleep(10);
    }
    FS_Restart(0);
}
#endif

void SV_Init(void) {
    SV_AddOperatorCommands();

    Cvar_Get("dmflags", "0", CVAR_SERVERINFO);

    SV_LoadArenas();
    SV_LoadFactories();
    sv_mapPoolFile = Cvar_Get("sv_mapPoolFile", "mappool.txt", CVAR_ARCHIVE);
    sv_includeCurrentMapInVote = Cvar_Get("sv_includeCurrentMapInVote", "0", CVAR_TEMP);
    SV_LoadMapPool(sv_mapPoolFile->string);

    Cvar_Get("fraglimit", "20", CVAR_SERVERINFO);
    Cvar_Get("timelimit", "0", CVAR_SERVERINFO);
    sv_gametype = Cvar_Get("g_gametype", "0", CVAR_SERVERINFO | CVAR_ROM);
    sv_ammoPack = Cvar_Get("g_ammoPack", "1", CVAR_LATCH);
    Cvar_Get("sv_keywords", "", CVAR_SERVERINFO);
    sv_mapname = Cvar_Get("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
    sv_privateClients = Cvar_Get("sv_privateClients", "0", CVAR_SERVERINFO);
    sv_hostname = Cvar_Get("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE);
    sv_maxclients = Cvar_Get("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);

    sv_minRate = Cvar_Get("sv_minRate", "0", CVAR_ARCHIVE);
    sv_maxRate = Cvar_Get("sv_maxRate", "0", CVAR_ARCHIVE);
    sv_dlRate = Cvar_Get("sv_dlRate", "100", CVAR_ARCHIVE);
    sv_minPing = Cvar_Get("sv_minPing", "0", CVAR_ARCHIVE);
    sv_maxPing = Cvar_Get("sv_maxPing", "0", CVAR_ARCHIVE);
    sv_serverType = Cvar_Get("sv_serverType", "0", CVAR_ARCHIVE);
    sv_floodProtect = Cvar_Get("sv_floodProtect", "10", CVAR_ARCHIVE);

    sv_master = Cvar_Get("sv_master", "1", CVAR_ARCHIVE);
    sv_idleExit = Cvar_Get("sv_idleExit", "120", CVAR_ARCHIVE);
    sv_errorExit = Cvar_Get("sv_errorExit", "1", 0);
#ifdef DEDICATED
    sv_workshopFile = Cvar_Get("sv_workshopFile", "workshop.txt", CVAR_ARCHIVE);
#endif
    sv_quitOnEmpty = Cvar_Get("sv_quitOnEmpty", "0", CVAR_ARCHIVE);
    sv_fullRedirect = Cvar_Get("sv_fullRedirect", "", CVAR_ARCHIVE);

    Cvar_Get("sv_referencedSteamworks", "", CVAR_ROM);
    Cvar_Get("sv_gtid", "", CVAR_SERVERINFO | CVAR_ROM);

    Cvar_Get("sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM);
    sv_serverid = Cvar_Get("sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM);
    sv_pure = Cvar_Get("sv_pure", "1", CVAR_SYSTEMINFO);

    Cvar_Get("sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM);
    Cvar_Get("sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM);
    Cvar_Get("sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM);
    Cvar_Get("sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM);

    sv_rconPassword = Cvar_Get("rconPassword", "", CVAR_TEMP);
    sv_privatePassword = Cvar_Get("sv_privatePassword", "", CVAR_TEMP);
    sv_fps = Cvar_Get("sv_fps", "40", CVAR_ROM);
    sv_timeout = Cvar_Get("sv_timeout", "40", CVAR_TEMP);
    sv_zombietime = Cvar_Get("sv_zombietime", "2", CVAR_TEMP);
    Cvar_Get("nextmap", "", CVAR_TEMP);

    sv_allowDownload = Cvar_Get("sv_allowDownload", "0", 0);
    Cvar_Get("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE);

    sv_reconnectlimit = Cvar_Get("sv_reconnectlimit", "3", 0);
    sv_showloss = Cvar_Get("sv_showloss", "0", 0);
    sv_padPackets = Cvar_Get("sv_padPackets", "0", 0);
    sv_killserver = Cvar_Get("sv_killserver", "0", 0);
    sv_mapChecksum = Cvar_Get("sv_mapChecksum", "", CVAR_ROM);
    sv_lanForceRate = Cvar_Get("sv_lanForceRate", "1", CVAR_ARCHIVE);
    sv_banFile = Cvar_Get("sv_banFile", "serverbans.dat", CVAR_ARCHIVE);
    sv_altEntDir = Cvar_Get("sv_altEntDir", "", 0);
    sv_dumpEntities = Cvar_Get("sv_dumpEntities", "0", 0);

    SV_BotInitCvars();

    SV_BotInitBotLib();

}

void SV_FinalMessage(char* message) {
    int i, j;
    client_t* cl;

    for (j = 0; j < 2; j++) {
        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
            if (cl->state >= CS_CONNECTED) {
                if (cl->netchan.remoteAddress.type != NA_LOOPBACK) {
                    SV_SendServerCommand(cl, "print \"%s\n\"\n", message);
                    SV_SendServerCommand(cl, "disconnect \"%s\"", message);
                }
                cl->nextSnapshotTime = 0;
                SV_SendClientSnapshot(cl);
            }
        }
    }
}

void SV_Shutdown(char* finalmsg) {
    if (!com_sv_running || !com_sv_running->integer) {
        return;
    }

    Com_Printf("----- Server Shutdown (%s) -----\n", finalmsg);

    NET_LeaveMulticast6();

    if (svs.clients && !com_errorEntered) {
        SV_FinalMessage(finalmsg);
    }

    SV_RemoveOperatorCommands();
    SV_ShutdownGameProgs();

    SV_ClearServer();

    if (svs.clients) {
        int index;

        for (index = 0; index < sv_maxclients->integer; index++)
            SV_FreeClient(&svs.clients[index]);

        Z_Free(svs.clients);
    }

    if (svs.factory) {
        SV_RestoreFactoryCvars(svs.factory);
    }
    Com_Memset(&svs, 0, sizeof(svs));

    Cvar_Set("sv_running", "0");
    Cvar_Set("ui_singlePlayerActive", "0");

    Com_Printf("---------------------------\n");

    if (sv_killserver->integer != 2)
        CL_Disconnect(qfalse);
}
