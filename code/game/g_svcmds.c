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

typedef struct ipFilter_s {
    unsigned mask;
    unsigned compare;
} ipFilter_t;

#define MAX_IPFILTERS 1024

static ipFilter_t ipFilters[MAX_IPFILTERS];
static int numIPFilters;

static qboolean StringToFilter(char* s, ipFilter_t* f) {
    char num[128];
    int i, j;
    byte b[4];
    byte m[4];

    for (i = 0; i < 4; i++) {
        b[i] = 0;
        m[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') {
            if (*s == '*')
            {
                s++;
                if (!*s)
                    break;
                s++;
                continue;
            }
            G_Printf("Bad filter address: %s\n", s);
            return qfalse;
        }

        j = 0;
        while (*s >= '0' && *s <= '9') {
            num[j++] = *s++;
        }
        num[j] = 0;
        b[i] = atoi(num);
        m[i] = 255;

        if (!*s)
            break;
        s++;
    }

    f->mask = *(unsigned*)m;
    f->compare = *(unsigned*)b;

    return qtrue;
}

static void UpdateIPBans(void) {
    byte b[4] = {0};
    byte m[4] = {0};
    int i, j;
    char iplist_final[MAX_CVAR_VALUE_STRING] = {0};
    char ip[64] = {0};

    *iplist_final = 0;
    for (i = 0; i < numIPFilters; i++) {
        if (ipFilters[i].compare == 0xffffffff)
            continue;

        *(unsigned*)b = ipFilters[i].compare;
        *(unsigned*)m = ipFilters[i].mask;
        *ip = 0;
        for (j = 0; j < 4; j++) {
            if (m[j] != 255)
                Q_strcat(ip, sizeof(ip), "*");
            else
                Q_strcat(ip, sizeof(ip), va("%i", b[j]));
            Q_strcat(ip, sizeof(ip), (j < 3) ? "." : " ");
        }
        if (strlen(iplist_final) + strlen(ip) < MAX_CVAR_VALUE_STRING) {
            Q_strcat(iplist_final, sizeof(iplist_final), ip);
        } else {
            Com_Printf("g_banIPs overflowed at MAX_CVAR_VALUE_STRING\n");
            break;
        }
    }

    trap_Cvar_Set("g_banIPs", iplist_final);
}

qboolean G_FilterPacket(char* from) {
    int i;
    unsigned in;
    byte m[4] = {0};
    char* p;

    i = 0;
    p = from;
    while (*p && i < 4) {
        m[i] = 0;
        while (*p >= '0' && *p <= '9') {
            m[i] = m[i] * 10 + (*p - '0');
            p++;
        }
        if (!*p || *p == ':')
            break;
        i++, p++;
    }

    in = *(unsigned*)m;

    for (i = 0; i < numIPFilters; i++)
        if ((in & ipFilters[i].mask) == ipFilters[i].compare)
            return g_filterBan.integer != 0;

    return g_filterBan.integer == 0;
}

static void AddIP(char* str) {
    int i;

    for (i = 0; i < numIPFilters; i++)
        if (ipFilters[i].compare == 0xffffffff)
            break;
    if (i == numIPFilters) {
        if (numIPFilters == MAX_IPFILTERS) {
            G_Printf("IP filter list is full\n");
            return;
        }
        numIPFilters++;
    }

    if (!StringToFilter(str, &ipFilters[i]))
        ipFilters[i].compare = 0xffffffffu;

    UpdateIPBans();
}

void G_ProcessIPBans(void) {
    char *s, *t;
    char str[MAX_CVAR_VALUE_STRING];

    Q_strncpyz(str, g_banIPs.string, sizeof(str));

    for (t = s = g_banIPs.string; *t; ) {
        s = strchr(s, ' ');
        if (!s)
            break;
        while (*s == ' ')
            *s++ = 0;
        if (*t)
            AddIP(t);
        t = s;
    }
}

void Svcmd_AddIP_f(void) {
    char str[MAX_TOKEN_CHARS];

    if (trap_Argc() < 2) {
        G_Printf("Usage: addip <ip-mask>\n");
        return;
    }

    trap_Argv(1, str, sizeof(str));

    AddIP(str);
}

void Svcmd_RemoveIP_f(void) {
    ipFilter_t f;
    int i;
    char str[MAX_TOKEN_CHARS];

    if (trap_Argc() < 2) {
        G_Printf("Usage: removeip <ip-mask>\n");
        return;
    }

    trap_Argv(1, str, sizeof(str));

    if (!StringToFilter(str, &f))
        return;

    for (i = 0; i < numIPFilters; i++) {
        if (ipFilters[i].mask == f.mask &&
            ipFilters[i].compare == f.compare) {
            ipFilters[i].compare = 0xffffffffu;
            G_Printf("Removed.\n");

            UpdateIPBans();
            return;
        }
    }

    G_Printf("Didn't find %s.\n", str);
}

void Svcmd_EntityList_f(void) {
    static const char* const etypeNames[15] = {
        "ET_GENERAL          ",
        "ET_PLAYER           ",
        "ET_ITEM             ",
        "ET_MISSILE          ",
        "ET_MOVER            ",
        "ET_BEAM             ",
        "ET_PORTAL           ",
        "ET_SPEAKER          ",
        "ET_PUSH_TRIGGER     ",
        "ET_TELEPORT_TRIGGER ",
        "ET_INVISIBLE        ",
        "ET_GRAPPLE          ",
        "ET_TEAM             ",
        "ET_EVENT            ",
        "ET_DEBUG_BOX        ",
    };
    int e;
    gentity_t* check;

    check = g_entities;
    for (e = 0; e < level.num_entities; e++, check++) {
        if (!check->inuse) {
            continue;
        }
        if (check->s.eType < 0 || check->s.eType > 14) {
            G_Printf("^1%3i:ET_UNKNOWN %i", e, check->s.eType);
        } else {
            G_Printf("%3i:%s", e, etypeNames[check->s.eType]);
        }

        if (check->classname) {
            G_Printf("%s", check->classname);
        }
        G_Printf("\n");
    }
}

gclient_t* ClientForString(const char* s) {
    gclient_t* cl;
    int i;
    int idnum;

    if (s[0] >= '0' && s[0] <= '9') {
        idnum = atoi(s);
        if (idnum < 0 || idnum >= level.maxclients) {
            Com_Printf("Bad client slot: %i\n", idnum);
            return NULL;
        }

        cl = &level.clients[idnum];
        if (cl->pers.connected == CON_DISCONNECTED) {
            G_Printf("Client %i is not connected\n", idnum);
            return NULL;
        }
        return cl;
    }

    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];
        if (cl->pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (!Q_stricmp(cl->pers.netname, s)) {
            return cl;
        }
    }

    G_Printf("User %s is not on the server\n", s);

    return NULL;
}

void Svcmd_ForceTeam_f(void) {
    gclient_t* cl;
    char str[MAX_TOKEN_CHARS];

    trap_Argv(1, str, sizeof(str));
    cl = ClientForString(str);
    if (!cl) {
        return;
    }

    trap_Argv(2, str, sizeof(str));
    SetTeam(&g_entities[cl - level.clients], str);
}

static int QDECL G_ShuffleCompare(const void* a, const void* b) {
    (void)a;
    (void)b;
    return (rand() % 3) - 1;
}

void G_ShuffleTeams(void) {
    int players[MAX_CLIENTS];
    int count = 0, moved = 0;
    int team = (rand() & 1) + 1;
    int i;

    level.teamShuffleActive = qtrue;
    level.autoShuffleArmed = qfalse;
    level.autoShuffleStart = 0;

    if (level.numPlayingClients < g_shuffle_minplayers.integer && g_gametype.integer != GT_RR) {
        trap_SendServerCommand(-1, "print \"Unable to shuffle: Not enough players.\n\"");
        level.teamShuffleActive = qfalse;
        return;
    }

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected != CON_CONNECTED) {
            continue;
        }
        if (level.clients[i].sess.sessionTeam != TEAM_RED &&
            level.clients[i].sess.sessionTeam != TEAM_BLUE) {
            continue;
        }
        players[count++] = i;
    }

    if (level.warmupTime != 0 || g_gametype.integer != GT_RR) {
        trap_SendServerCommand(-1, "print \"Shuffling Teams.\n\"");
    }

    qsort(players, count, sizeof(players[0]), G_ShuffleCompare);

    for (i = 0; i < count; i++) {
        gentity_t* target = &g_entities[players[i]];

        if (i == count - 1 && (count & 1)) {
            team = (team == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
        }

        if (target->client->sess.sessionTeam == team) {
            if (g_gametype.integer == GT_RR) {
                ClientSpawn(target);
                if (level.roundState.eCurrent == RS_COUNTDOWN) {
                    target->client->ps.pm_flags |= PMF_FROZEN;
                }
            }
        } else {
            SetTeam_Execute(target, team, SPECTATOR_NOT, 0);
            moved++;
        }

        team = (team == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
        target->client->switchTeamTime = level.time + 5000;
    }

    if (moved) {
        CalculateRanks();
    } else {
        trap_SendServerCommand(-1, "print \"No players shuffled.\n\"");
    }

    level.teamShuffleActive = qfalse;
}

void Svcmd_ForceShuffle_f(void) {
    G_ShuffleTeams();
}

char* ConcatArgs(int start);

void G_InitAccessList(void);
int  G_GetAccess(int clientNum);
void G_ReloadAccessList(void);

typedef struct {
    uint64_t steamId;
    int      level;
    int      flag;
} accessEntry_t;

#define MAX_ACCESS_ENTRIES 8192

static accessEntry_t g_accessList[MAX_ACCESS_ENTRIES];
static int           g_numAccessEntries;

int G_AccessCount(void) {
    return g_numAccessEntries;
}

qboolean G_AccessAt(int index, uint64_t* steamId, int* level, qboolean* temporary) {
    if (index < 0 || index >= g_numAccessEntries) {
        return qfalse;
    }
    *steamId = g_accessList[index].steamId;
    *level = g_accessList[index].level;
    *temporary = (qboolean)(g_accessList[index].flag != 0);
    return qtrue;
}

static accessEntry_t* AccessList_Get(uint64_t steamId) {
    int i;
    for (i = 0; i < g_numAccessEntries; i++) {
        if (g_accessList[i].steamId == steamId) {
            return &g_accessList[i];
        }
    }
    if (g_numAccessEntries >= MAX_ACCESS_ENTRIES) {
        return &g_accessList[MAX_ACCESS_ENTRIES - 1];
    }
    g_accessList[g_numAccessEntries].steamId = steamId;
    g_accessList[g_numAccessEntries].level = 0;
    g_accessList[g_numAccessEntries].flag = 0;
    return &g_accessList[g_numAccessEntries++];
}

void G_SaveAccessList(void) {
    static const char* const header =
        "# Be sure to run /reload_access if editing this file while server is running.\r\n"
        "# The server will overwrite your changes on map exit and shutdown\r\n"
        "# 1 entry per line, format: steamid|(mod|admin|ban)\r\n"
        "# ex: 76561198072786081|ban\r\n";
    fileHandle_t f;
    int i;

    trap_FS_FOpenFile(g_accessFile.string, &f, FS_WRITE);
    if (!f) {
        G_Printf("^1file not found: %s\n", g_accessFile.string);
        return;
    }
    trap_FS_Write(header, strlen(header), f);

    for (i = 0; i < g_numAccessEntries; i++) {
        const accessEntry_t* e = &g_accessList[i];
        const char* word;
        char line[64];

        if (e->flag != 0) {
            continue;
        }
        if (e->level == -1) {
            word = "ban";
        } else if (e->level == 1) {
            word = "mod";
        } else if (e->level == 2) {
            word = "admin";
        } else {
            continue;
        }
        Com_sprintf(line, sizeof(line), "%llu|%s\r\n", (unsigned long long)e->steamId, word);
        trap_FS_Write(line, strlen(line), f);
    }
    trap_FS_FCloseFile(f);
}

void G_SetAccess(uint64_t steamId, int level, qboolean temporary) {
    accessEntry_t* e = AccessList_Get(steamId);

    if (!e) {
        return;
    }
    e->level = level;
    e->flag = temporary ? 1 : 0;
}

int G_GetAccessBySteamId(uint64_t steamId) {
    int i;

    for (i = 0; i < g_numAccessEntries; i++) {
        if (g_accessList[i].steamId == steamId) {
            return g_accessList[i].level;
        }
    }
    return 0;
}

void G_InitAccessList(void) {
    fileHandle_t f;
    int len;
    char buf[16384];
    char* line;

    len = trap_FS_FOpenFile(g_accessFile.string, &f, FS_READ);
    G_Printf("initializing access list...\n");
    if (!f) {
        G_Printf("^1file not found: %s\n", g_accessFile.string);
        return;
    }

    if (len >= (int)sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);

    g_numAccessEntries = 0;

    line = strtok(buf, "\n");
    while (line) {
        if (*line != '#') {
            char* cr;
            uint64_t steamId;
            char word[64];

            cr = strchr(line, '\r');
            if (cr) {
                *cr = '\0';
            }
            if (sscanf(line, "%llu|%63s", &steamId, word) != 2) {
                G_Printf("^1invalid admin access format, skipping: %s\n", line);
            } else if (!strcmp(word, "ban")) {
                AccessList_Get(steamId)->level = -1;
            } else if (!strcmp(word, "mod")) {
                AccessList_Get(steamId)->level = 1;
            } else if (!strcmp(word, "admin")) {
                AccessList_Get(steamId)->level = 2;
            }
        }
        line = strtok(NULL, "\n");
    }

    G_Printf("loaded %i steam ids into the access list\n", g_numAccessEntries);
}

int G_GetAccess(int clientNum) {
    uint64_t steamId;
    char userinfo[MAX_INFO_STRING];
    char* ip;
    int i;

    steamId = trap_GetSteamID(clientNum);
    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));
    ip = Info_ValueForKey(userinfo, "ip");
    if (!strcmp(ip, "localhost")) {
        return 3;
    }
    for (i = 0; i < g_numAccessEntries; i++) {
        if (g_accessList[i].steamId == steamId) {
            return g_accessList[i].level;
        }
    }
    return 0;
}

void G_ReloadAccessList(void) {
    int i;

    G_InitAccessList();
    for (i = 0; i < level.numConnectedClients; i++) {
        g_clients[i].sess.privileges = G_GetAccess(i);
    }
}

void Svcmd_DumpVars_f(void) {
    char str[MAX_TOKEN_CHARS];
    gclient_t* cl;

    trap_Argv(1, str, sizeof(str));
    cl = ClientForString(str);
    if (!cl) {
        return;
    }

    G_Printf("Data Dump: (%s)\n", cl->pers.netname);
    G_Printf("clientNum: %d\n", cl->ps.clientNum);
    G_Printf("pm_type: 0x%08x\n", cl->ps.pm_type);
    G_Printf("pm_flags: 0x%08x\n", cl->ps.pm_flags);
    G_Printf("pm_time: %d\n", cl->ps.pm_time);
    G_Printf("eFlags: %d\n", cl->ps.eFlags);
    G_Printf("origin: (%0.3f, %0.3f, %0.3f)\n",
            cl->ps.origin[0],
            cl->ps.origin[1],
            cl->ps.origin[2]);
}

qboolean ConsoleCommand(void) {
    char cmd[MAX_TOKEN_CHARS];

    trap_Argv(0, cmd, sizeof(cmd));

    if (Q_stricmp(cmd, "entitylist") == 0) {
        Svcmd_EntityList_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "forceteam") == 0) {
        Svcmd_ForceTeam_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "game_memory") == 0) {
        Svcmd_GameMem_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "addbot") == 0) {
        Svcmd_AddBot_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "botlist") == 0) {
        Svcmd_BotList_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "game_crash") == 0) {
        if (trap_Cvar_VariableValue("developer") == 1) {
            *(volatile int*)0 = 0x12345678;
        }
        return qtrue;
    }
    if (Q_stricmp(cmd, "forceshuffle") == 0) {
        Svcmd_ForceShuffle_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "dumpvars") == 0) {
        Svcmd_DumpVars_f();
        return qtrue;
    }
    if (Q_stricmp(cmd, "markstate") == 0) {
        return qtrue;
    }
    if (Q_stricmp(cmd, "reload_access") == 0) {
        G_ReloadAccessList();
        return qtrue;
    }
    if (Q_stricmp(cmd, "diffstate") == 0) {
        return qtrue;
    }
    if (Q_stricmp(cmd, "dumpentities") == 0) {
        return qtrue;
    }
    if (Q_stricmp(cmd, "printentitystates") == 0) {
        return qtrue;
    }

    if (g_dedicated.integer) {
        if (Q_stricmp(cmd, "say") == 0) {
            trap_SendServerCommand(-1, va("print \"server: %s\"", ConcatArgs(1)));
        }
        if (Q_stricmp(cmd, "tell") == 0) {
            char slotStr[MAX_TOKEN_CHARS];
            char* msg;
            const char* p;
            int slot;

            trap_Argv(1, slotStr, sizeof(slotStr));
            slot = atoi(slotStr);
            msg = ConcatArgs(2);
            for (p = slotStr; *p; p++) {
                if (*p < '0' || *p > '9') {
                    G_Printf("Bad slot number: %s\n", slotStr);
                    return qtrue;
                }
            }
            if (msg[0] == '\0') {
                return qtrue;
            }
            trap_SendServerCommand(slot, va("print \"server whisper: %s\"", msg));
        }

        G_ConsolePermissionCommand(cmd);
        return qtrue;
    }

    return qfalse;
}
