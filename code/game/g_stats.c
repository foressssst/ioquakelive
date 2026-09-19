/*
===========================================================================
ioquakelive - the match statistics events

Retail's game module builds these with jsoncpp and hands them to the engine
through its import +0x2e8 (0x004e2640 in the engine), which sends each one to
Steam's stats and to the ZeroMQ stats socket as {"DATA": ..., "TYPE": ...}.
The event names and the keys of each one were read in qagamex86.dll with
re/tools/stat_keys.py; the addresses are next to each function.

Every event carries the same three members first (the prologue each of retail's
builders repeats):

    TIME        seconds since the level started: (level.time - level.startTime) / 1000
    WARMUP      whether the match is still in warmup
    MATCH_GUID  the match's GUID, configstring CS_MATCH_GUID, set by G_InitGame

A player's STEAM_ID goes as a string ("%llu"); a bot's is "0".
===========================================================================
*/

#include "g_local.h"
#include "g_json.h"

#define STAT_DOC_SIZE 65536

static char statDoc[STAT_DOC_SIZE];

// the three members every event starts with
static jnode_t* STAT_Begin(void) {
    jnode_t* data;
    char guid[MAX_STRING_CHARS];

    J_Reset();
    data = J_Object();
    J_SetInt(data, "TIME", (level.time - level.startTime) / 1000);
    J_SetBool(data, "WARMUP", (qboolean)(level.warmupTime != 0));
    trap_GetConfigstring(CS_MATCH_GUID, guid, sizeof(guid));
    J_SetString(data, "MATCH_GUID", guid);
    return data;
}

// STEAM_ID: the account as a string, "0" for a bot (retail tests svFlags & SVF_BOT)
static void STAT_SteamId(jnode_t* obj, const gentity_t* ent) {
    if (ent->r.svFlags & SVF_BOT) {
        J_SetString(obj, "STEAM_ID", "0");
    } else {
        J_SetString(obj, "STEAM_ID", va("%llu", (unsigned long long)ent->client->pers.steamId));
    }
}

// retail's team names: 1 RED, 2 BLUE, 3 SPECTATOR, anything else FREE
static const char* STAT_TeamName(int team) {
    switch (team) {
        case TEAM_RED: return "RED";
        case TEAM_BLUE: return "BLUE";
        case TEAM_SPECTATOR: return "SPECTATOR";
        default: return "FREE";
    }
}

// wraps the data as the engine's publisher does (0x004f4b20) and sends it
static void STAT_Send(uint64_t steamId, const char* type, jnode_t* data) {
    jnode_t* root = J_Object();

    J_SetNode(root, "DATA", data);
    J_SetString(root, "TYPE", type);
    if (!J_Write(root, statDoc, sizeof(statDoc))) {
        G_Printf("^3stats: %s did not fit and was not sent\n", type);
        return;
    }
    trap_StatsEvent(steamId, type, statDoc);
}

static uint64_t STAT_Account(const gentity_t* ent) {
    return (ent->r.svFlags & SVF_BOT) ? 0 : ent->client->pers.steamId;
}

/*
===============
STAT_PlayerConnect

[QL] 0x10078450, called from ClientConnect (0x1003afb5).
===============
*/
void STAT_PlayerConnect(gentity_t* ent) {
    jnode_t* data;

    if (!ent || !ent->client) {
        return;
    }
    data = STAT_Begin();
    J_SetString(data, "NAME", ent->client->pers.netname);
    STAT_SteamId(data, ent);
    STAT_Send(STAT_Account(ent), "PLAYER_CONNECT", data);
}

/*
===============
STAT_PlayerDisconnect

[QL] 0x100786d0, called from ClientDisconnect (0x1003cac3). The same members as
the connect.
===============
*/
void STAT_PlayerDisconnect(gentity_t* ent) {
    jnode_t* data;

    if (!ent || !ent->client) {
        return;
    }
    data = STAT_Begin();
    J_SetString(data, "NAME", ent->client->pers.netname);
    STAT_SteamId(data, ent);
    STAT_Send(STAT_Account(ent), "PLAYER_DISCONNECT", data);
}

/*
===============
STAT_PlayerSwitchTeam

[QL] 0x10072350, called from SetTeam (0x100406b8). The player goes in a member of
its own, under the key "KILLER" -- a quirk of retail's that the stats readers
depend on.
===============
*/
void STAT_PlayerSwitchTeam(gentity_t* ent, int oldTeam, int newTeam) {
    jnode_t* data;
    jnode_t* who;

    if (!ent || !ent->client) {
        return;
    }
    data = STAT_Begin();
    who = J_Object();
    J_SetString(who, "NAME", ent->client->pers.netname);
    STAT_SteamId(who, ent);
    J_SetString(who, "OLD_TEAM", STAT_TeamName(oldTeam));
    J_SetString(who, "TEAM", STAT_TeamName(newTeam));
    J_SetNode(data, "KILLER", who);
    STAT_Send(STAT_Account(ent), "PLAYER_SWITCHTEAM", data);
}

// [ioquakelive] cuantas lleva cada jugador de cada medalla, para el TOTAL del evento. Retail lo
// saca de sus contadores; de cual sale cada llamada no esta verificado contra el binario.
#define STAT_MAX_MEDALS 24

static struct {
    char name[32];
    int count[MAX_CLIENTS];
} statMedals[STAT_MAX_MEDALS];

static int STAT_CountMedal(int clientNum, const char* medal) {
    int i;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return 1;
    }
    for (i = 0; i < STAT_MAX_MEDALS; i++) {
        if (!statMedals[i].name[0]) {
            Q_strncpyz(statMedals[i].name, medal, sizeof(statMedals[i].name));
        }
        if (!strcmp(statMedals[i].name, medal)) {
            return ++statMedals[i].count[clientNum];
        }
    }
    return 1;
}

void STAT_ResetMedals(void) {
    Com_Memset(statMedals, 0, sizeof(statMedals));
}

// cuantas lleva ese jugador de esa medalla, contando esta
int STAT_MedalTotal(gentity_t* ent, const char* medal) {
    if (!ent || !ent->client) {
        return 1;
    }
    return STAT_CountMedal((int)(ent - g_entities), medal);
}

/*
===============
STAT_PlayerMedal

[QL] 0x100716a0. MEDAL is the medal's name and TOTAL how many the player has of it.
===============
*/
void STAT_PlayerMedal(gentity_t* ent, const char* medal, int total) {
    jnode_t* data;

    if (!ent || !ent->client || !medal) {
        return;
    }
    data = STAT_Begin();
    J_SetString(data, "NAME", ent->client->pers.netname);
    STAT_SteamId(data, ent);
    J_SetString(data, "MEDAL", medal);
    J_SetInt(data, "TOTAL", total);
    STAT_Send(STAT_Account(ent), "PLAYER_MEDAL", data);
}

/*
===============
STAT_RoundOver

[QL] 0x100720e0. TEAM_WON is the winner by retail's names, or DRAW; the event
goes out with no account (steam id 0).
===============
*/
void STAT_RoundOver(int round, int winningTeam, qboolean draw) {
    jnode_t* data = STAT_Begin();

    J_SetInt(data, "ROUND", round);
    J_SetString(data, "TEAM_WON", draw ? "DRAW" : STAT_TeamName(winningTeam));
    STAT_Send(0, "ROUND_OVER", data);
}

/*
===============
STAT_PlayerRaceComplete

[QL] 0x10071dd0, called when a player finishes a race (0x100637be). RACE_TIME is
the time in milliseconds.
===============
*/
void STAT_PlayerRaceComplete(gentity_t* ent, int raceTime, int weaponsUsed) {
    jnode_t* data;

    if (!ent || !ent->client) {
        return;
    }
    data = STAT_Begin();
    J_SetString(data, "NAME", ent->client->pers.netname);
    STAT_SteamId(data, ent);
    J_SetInt(data, "WEAPONS_USED", weaponsUsed);
    J_SetInt(data, "RACE_TIME", raceTime);
    STAT_Send(STAT_Account(ent), "PLAYER_RACECOMPLETE", data);
}

/*
===============
G_SetMatchGUID

[QL] G_InitGame (0x10055386): the engine makes a GUID (import +0x328, 0x004e27d0,
a CoCreateGuid written in lower case) and it becomes CS_MATCH_GUID, which every
stats event carries.
===============
*/
void G_SetMatchGUID(void) {
    char guid[64];

    trap_GenerateMatchGUID(guid, sizeof(guid));
    trap_SetConfigstring(CS_MATCH_GUID, guid);
}
