/*
===========================================================================
ioquakelive - the players' Steam stats and achievements

[QL] quakelive_steam.exe keeps an object for each player (0x00467850), in a map by
Steam ID (0x00e30390). It is made when Steam takes the player's ticket (0x00467cd0) and
asks Steam for the player's stats; it is stored and thrown away when the player goes
(0x00467b10). In between, the game module adds to the stats and grants achievements
through its imports, and every stats event the game sends (0x00468030) adds to them too.

Only the stats are read back from Steam. What the player already has of the achievements
is never asked: "has it" means granted in this session, as in retail.
===========================================================================
*/

#include "server.h"
#include "../qcommon/q_steam.h"

#define JSON_IMPLEMENTATION
#include "../qcommon/json.h"
#undef JSON_IMPLEMENTATION

#define STEAMSTATS_MAX_USERS (MAX_CLIENTS * 2)

// [QL] 0x00561060: every stat is an integer, and its Steam ID is its place in the table
static const char* const svStatNames[] = {
    "version",
    "kill_gauntlet", "kill_machinegun", "kill_shotgun", "kill_grenade", "kill_rocket",
    "kill_lightning", "kill_railgun", "kill_plasma", "kill_bfg", "kill_nailgun", "kill_proxmine",
    "kill_chaingun", "kill_hmg",
    "hits_machinegun", "hits_shotgun", "hits_grenade", "hits_rocket", "hits_lightning",
    "hits_railgun", "hits_plasma", "hits_bfg", "hits_nailgun", "hits_proxmine", "hits_chaingun",
    "hits_hmg",
    "shots_machinegun", "shots_shotgun", "shots_grenade", "shots_rocket", "shots_lightning",
    "shots_railgun", "shots_plasma", "shots_bfg", "shots_nailgun", "shots_proxmine",
    "shots_chaingun", "shots_hmg",
    "mod_shotgun", "mod_gauntlet", "mod_machinegun", "mod_grenade", "mod_rocket", "mod_plasma",
    "mod_railgun", "mod_lightning", "mod_bfg", "mod_water", "mod_slime", "mod_lava", "mod_crush",
    "mod_telefrag", "mod_laser", "BROKEN1", "mod_nailgun", "mod_chaingun", "mod_proxmine",
    "mod_kamikaze", "mod_juiced", "mod_suicide", "mod_falling", "mod_grapple", "mod_hmg",
    "mod_lightning_discharge", "mod_other",
    "medal_firstfrag", "medal_gauntlet", "medal_excellent", "medal_revenge", "medal_combokill",
    "medal_midair", "medal_perforated", "medal_rampage", "medal_impressive", "medal_capture",
    "medal_assist", "medal_defense", "medal_headshot", "medal_quadgod", "medal_perfect",
    "medal_accuracy",
    "wins", "losses", "played", "BROKEN2", "mod_hurt", "total_kills", "total_deaths",
};

#define STEAMSTATS_NUM_STATS ((int)ARRAY_LEN(svStatNames))

#define STAT_MEDAL_CAPTURE 74
#define STAT_MEDAL_ASSIST 75
#define STAT_MEDAL_DEFENSE 76
#define STAT_WINS 81
#define STAT_LOSSES 82
#define STAT_PLAYED 83
#define STAT_TOTAL_KILLS 86
#define STAT_TOTAL_DEATHS 87

// [QL] 0x00561c00, AW_MAX included: each player has room for 0x3b of them (0x004679b1)
static const char* const svAchievementNames[] = {
    "AW_MIDAIR", "AW_SPEED_KILLS", "AW_TRAINING_1_1", "AW_TRAINING_1_2", "AW_TRAINING_1_3",
    "AW_TRAINING_2_1", "AW_TRAINING_2_2", "AW_TRAINING_2_3", "AW_TRAINING_3_1",
    "AW_TRAINING_3_2", "AW_FIRST_FRAG", "AW_TESTING", "AW_BIG_TIME", "AW_PRIZE_FIGHTER",
    "AW_WICKED", "AW_BANDIT", "AW_CAMPER", "AW_PSYCHIC", "AW_WTF_WAS_THAT", "AW_OVERKILL",
    "AW_RAPTOR", "AW_PLUS_ONE", "AW_KILLJOY", "AW_HAT_TRICK", "AW_MIRACLE_MAKER", "AW_BRAWLER",
    "AW_AIR_HAMMER", "AW_AIM_BOT", "AW_SUCKER_PUNCH", "AW_RESOURCE_HOG", "AW_NINJA_CAP",
    "AW_MISSED_OPPORTUNITY", "AW_SKULL_TRUMPET", "AW_FIGHT_CLUB", "AW_GUARDIAN", "AW_SIDEKICK",
    "AW_COLOR_GUARD", "AW_2_IN_2", "AW_ASSASSIN", "AW_EVIL_EYE", "AW_VICTORY",
    "AW_POINT_DENIED", "AW_FIRST_TASTE", "AW_HOOKED", "AW_FEAR_ME", "AW_VADRIGAR", "AW_MVP",
    "AW_SMACK_DOWN", "AW_HERE_GOES_NOTHING", "AW_LAST_HOPE", "AW_PUNCH_OUT", "AW_NADE_SPAM",
    "AW_ROCKET_MAN", "AW_PULL", "AW_CLUTCH", "AW_JESSE_JAMES", "AW_FULL_HOUSE", "AW_TRIFECTA",
    "AW_MAX",
};

#define STEAMSTATS_NUM_ACHIEVEMENTS ((int)ARRAY_LEN(svAchievementNames))

#define AW_SPEED_KILLS 1
#define AW_TRAINING_3_2 9
#define AW_WICKED 14
#define AW_MVP 46

// a name the stats events use and the stat it adds to; 0 adds to none
typedef struct {
    const char* name;
    int stat;
} svStatKey_t;

// [QL] 0x00561a08: KILLER.WEAPON of PLAYER_KILL
static const svStatKey_t svKillWeapons[] = {
    {"GAUNTLET", 1}, {"MACHINEGUN", 2}, {"SHOTGUN", 3}, {"GRENADE", 4}, {"ROCKET", 5},
    {"LIGHTNING", 6}, {"RAILGUN", 7}, {"PLASMA", 8}, {"BFG", 9}, {"NAILGUN", 10},
    {"PROXMINE", 11}, {"CHAINGUN", 12}, {"HMG", 13},
};

// [QL] 0x00561a70: MOD of PLAYER_DEATH
static const svStatKey_t svDeathMods[] = {
    {"SHOTGUN", 38}, {"GAUNTLET", 39}, {"MACHINEGUN", 40}, {"GRENADE", 41}, {"GRENADE_SPLASH", 41},
    {"ROCKET", 42}, {"ROCKET_SPLASH", 42}, {"PLASMA", 43}, {"PLASMA_SPLASH", 43}, {"RAILGUN", 44},
    {"LIGHTNING", 45}, {"BFG", 46}, {"BFG_SPLASH", 46}, {"WATER", 47}, {"SLIME", 48},
    {"LAVA", 49}, {"CRUSH", 50}, {"TELEFRAG", 51}, {"LASER", 52}, {"HURT", 85}, {"NAILGUN", 54},
    {"CHAINGUN", 55}, {"PROXMINE", 56}, {"KAMIKAZE", 57}, {"JUICED", 58}, {"SUICIDE", 59},
    {"FALLING", 60}, {"GRAPPLE", 61}, {"SWITCHTEAM", 0}, {"THAW", 0}, {"HMG", 62},
    {"LIGHTNING_DISCHARGE", 63}, {"OTHER_DEATH", 64},
};

// [QL] 0x00561b80: MEDAL of PLAYER_MEDAL
static const svStatKey_t svMedals[] = {
    {"FIRSTFRAG", 65}, {"GAUNTLET", 66}, {"EXCELLENT", 67}, {"REVENGE", 68}, {"COMBOKILL", 69},
    {"MIDAIR", 70}, {"PERFORATED", 71}, {"RAMPAGE", 72}, {"IMPRESSIVE", 73}, {"CAPTURE", 74},
    {"ASSIST", 75}, {"DEFENSE", 76}, {"HEADSHOT", 77}, {"QUADGOD", 78}, {"PERFECT", 79},
    {"ACCURACY", 80},
};

typedef struct {
    qboolean inuse;
    uint64_t steamId;
    qboolean received;  // Steam has answered with the player's stats
    int stats[STEAMSTATS_NUM_STATS];
    qboolean achieved[STEAMSTATS_NUM_ACHIEVEMENTS];
} svSteamUser_t;

static svSteamUser_t svSteamUsers[STEAMSTATS_MAX_USERS];

static svSteamUser_t* SV_SteamUser(uint64_t steamId) {
    int i;

    if (!steamId) {
        return NULL;
    }
    for (i = 0; i < STEAMSTATS_MAX_USERS; i++) {
        if (svSteamUsers[i].inuse && svSteamUsers[i].steamId == steamId) {
            return &svSteamUsers[i];
        }
    }
    return NULL;
}

/*
=================
SV_SteamStoreStats

[QL] 0x004670c0. Nothing goes up until Steam has answered with what the player had.
=================
*/
static void SV_SteamStoreStats(svSteamUser_t* u) {
    int i;

    if (!u->received) {
        return;
    }
    for (i = 0; i < STEAMSTATS_NUM_STATS; i++) {
        Steam_GS_SetUserStat(u->steamId, svStatNames[i], u->stats[i]);
    }
    Steam_GS_StoreUserStats(u->steamId);
}

/*
=================
SV_SteamReadStats

[QL] 0x004671d0. Each player's object answers every GSStatsReceived_t, whoever it is for,
and reads its own stats again.
=================
*/
static void SV_SteamReadStats(svSteamUser_t* u, int result) {
    int i;

    if (result != 1) {
        Com_DPrintf("RequestStats - failed, %d\n", result);
        return;
    }
    Com_DPrintf("Received stats and achievements from Steam\n");
    for (i = 0; i < STEAMSTATS_NUM_STATS; i++) {
        Steam_GS_GetUserStat(u->steamId, svStatNames[i], &u->stats[i]);
    }
    u->received = qtrue;
}

void SV_SteamStatsReceived(int result) {
    int i;

    for (i = 0; i < STEAMSTATS_MAX_USERS; i++) {
        if (svSteamUsers[i].inuse) {
            SV_SteamReadStats(&svSteamUsers[i], result);
        }
    }
}

// [QL] 0x00467360: stats Steam turned down are read back from it
void SV_SteamStatsStored(int result) {
    int i;

    for (i = 0; i < STEAMSTATS_MAX_USERS; i++) {
        if (!svSteamUsers[i].inuse) {
            continue;
        }
        if (result == 1) {
            Com_DPrintf("StoreStats - success\n");
        } else if (result == 8) {  // k_EResultInvalidParam
            Com_DPrintf("StoreStats - some failed to validate\n");
            SV_SteamReadStats(&svSteamUsers[i], 1);
        } else {
            Com_DPrintf("StoreStats - failed, %d\n", result);
        }
    }
}

// [QL] 0x00467190: once the server logs on, every player's stats are asked for
void SV_SteamStatsConnected(void) {
    int i;

    for (i = 0; i < STEAMSTATS_MAX_USERS; i++) {
        if (svSteamUsers[i].inuse) {
            Steam_GS_RequestUserStats(svSteamUsers[i].steamId);
        }
    }
}

/*
=================
SV_SteamStatsAddUser

[QL] 0x00467cd0, when Steam has taken the player's ticket. A player who already has an
object gets a new one, as the map's operator[] does in retail.
=================
*/
void SV_SteamStatsAddUser(uint64_t steamId) {
    svSteamUser_t* u;
    int i;

    if (!Steam_GS_Running() || !steamId) {
        return;
    }
    u = SV_SteamUser(steamId);
    for (i = 0; !u && i < STEAMSTATS_MAX_USERS; i++) {
        if (!svSteamUsers[i].inuse) {
            u = &svSteamUsers[i];
        }
    }
    if (!u) {
        return;
    }
    Com_Memset(u, 0, sizeof(*u));
    u->inuse = qtrue;
    u->steamId = steamId;
    Steam_GS_RequestUserStats(steamId);
    Steam_P2PHello(steamId, qtrue);
}

// [QL] 0x00467b10, from SV_DropClient (0x004df7f4) and for the host from CL_Disconnect
void SV_SteamStatsRemoveUser(uint64_t steamId) {
    svSteamUser_t* u = SV_SteamUser(steamId);

    if (!u) {
        return;
    }
    SV_SteamStoreStats(u);
    u->inuse = qfalse;
}

/*
=================
SV_SteamStatsIncrement

[QL] 0x00467d40, the game's import +0x324. Kept here until the next time the player's
stats are stored.
=================
*/
void SV_SteamStatsIncrement(uint64_t steamId, int stat, int amount) {
    svSteamUser_t* u = SV_SteamUser(steamId);

    if (!u || stat < 0 || stat >= STEAMSTATS_NUM_STATS) {
        return;
    }
    u->stats[stat] += amount;
    Com_DPrintf("%llu: %s: %i\n", (unsigned long long)steamId, svStatNames[stat], u->stats[stat]);
}

/*
=================
SV_SteamStatsSetAchievement

[QL] 0x00467e00, the game's import +0x32c. Only while the match is on, or in training or
practice. A game of one's own (sv_serverType 0) grants it on the player's own Steam.
=================
*/
void SV_SteamStatsSetAchievement(uint64_t steamId, int achievement) {
    svSteamUser_t* u = SV_SteamUser(steamId);
    const char* name;
    qboolean granted;

    if (!u || achievement < 0 || achievement >= STEAMSTATS_NUM_ACHIEVEMENTS) {
        return;
    }
    if (!(Cvar_Flags("g_gameState") & CVAR_NONEXISTENT) &&
        strcmp(Cvar_VariableString("g_gameState"), "IN_PROGRESS") && !Cvar_VariableIntegerValue("g_training") &&
        !Cvar_VariableIntegerValue("practiceflags")) {
        return;
    }
    name = svAchievementNames[achievement];

    if (!sv_serverType->integer) {
        if (Steam_SetAchievement(name)) {
            u->achieved[achievement] = qtrue;
            Steam_StoreStats();
        }
        return;
    }

    // [QL] 0x004672d0
    if (!u->received) {
        return;
    }
    granted = Steam_GS_SetUserAchievement(steamId, name);
    SV_SteamStoreStats(u);
    if (granted) {
        u->achieved[achievement] = qtrue;
    }
}

// [QL] 0x00467f70, the game's import +0x330
qboolean SV_SteamStatsHasAchievement(uint64_t steamId, int achievement) {
    const svSteamUser_t* u = SV_SteamUser(steamId);

    if (!u || !u->received || achievement < 0 || achievement >= STEAMSTATS_NUM_ACHIEVEMENTS) {
        return qfalse;
    }
    return u->achieved[achievement];
}

/*
===============================================================================

THE STATS EVENTS

[QL] 0x00468030, from the game's stats event import (0x004e2640) before the event goes to
the ZeroMQ socket. Nothing counts in warmup or in an aborted match.

===============================================================================
*/

// a member of an object, or NULL
static const char* SV_JsonMember(const char* json, const char* end, const char* name) {
    if (!json || json >= end || *json != '{') {
        return NULL;
    }
    return JSON_ObjectGetNamedValue(json, end, name);
}

// asInt: numbers are cut to integers, true is 1, anything else 0
static int SV_JsonInt(const char* value, const char* end) {
    if (!value) {
        return 0;
    }
    return (int)JSON_ValueGetDouble(value, end);
}

static void SV_JsonString(const char* value, const char* end, char* out, int size) {
    out[0] = '\0';
    if (value && JSON_ValueGetType(value, end) == JSONTYPE_STRING) {
        JSON_ValueGetString(value, end, out, (unsigned int)size);
    }
}

// the stat a name adds to, or -1
static int SV_StatForKey(const svStatKey_t* keys, int numKeys, const char* name) {
    int i;

    for (i = 0; i < numKeys; i++) {
        if (!strcmp(keys[i].name, name)) {
            return keys[i].stat ? keys[i].stat : -1;
        }
    }
    return -1;
}

// one more of a stat, stored at once
static void SV_SteamCountStat(svSteamUser_t* u, int stat, qboolean withId) {
    u->stats[stat]++;
    SV_SteamStoreStats(u);
    if (withId) {
        Com_DPrintf("%llu: %s: %i\n", (unsigned long long)u->steamId, svStatNames[stat], u->stats[stat]);
    } else {
        Com_DPrintf("%s: %i\n", svStatNames[stat], u->stats[stat]);
    }
}

void SV_SteamStatsEvent(uint64_t steamId, const char* type, const char* document) {
    const char* end;
    const char* data;
    svSteamUser_t* u;
    char text[64];
    int stat;

    if (!Steam_GS_Running() || !document) {
        return;
    }
    end = document + strlen(document);
    data = SV_JsonMember(document, end, "DATA");
    if (!data) {
        return;
    }
    if (SV_JsonInt(SV_JsonMember(data, end, "WARMUP"), end) == 1 ||
        SV_JsonInt(SV_JsonMember(data, end, "ABORTED"), end) == 1) {
        return;
    }
    u = SV_SteamUser(steamId);
    if (!u) {
        return;
    }

    if (!strcmp(type, "PLAYER_STATS")) {
        int win = SV_JsonInt(SV_JsonMember(data, end, "WIN"), end);

        SV_SteamStatsIncrement(steamId, STAT_WINS, win);
        SV_SteamStatsIncrement(steamId, STAT_LOSSES, SV_JsonInt(SV_JsonMember(data, end, "LOSE"), end));
        SV_SteamStatsIncrement(steamId, STAT_PLAYED, 1);
        SV_SteamStoreStats(u);

        if (!SV_SteamStatsHasAchievement(steamId, AW_TRAINING_3_2) &&
            !strcmp(Cvar_VariableString("mapname"), "qztraining") && Cvar_VariableIntegerValue("g_training") > 0 &&
            win > 0) {
            SV_SteamStatsSetAchievement(steamId, AW_TRAINING_3_2);
        }
        if (!SV_SteamStatsHasAchievement(steamId, AW_WICKED) && Cvar_VariableIntegerValue("g_gametype") == GT_CTF &&
            SV_JsonInt(SV_JsonMember(data, end, "SCORE"), end) == 666) {
            SV_SteamStatsSetAchievement(steamId, AW_WICKED);
        }
        return;
    }

    if (!strcmp(type, "PLAYER_KILL")) {
        const char* killer = SV_JsonMember(data, end, "KILLER");

        if (Cvar_VariableIntegerValue("g_gametype") == GT_RACE ||
            SV_JsonInt(SV_JsonMember(data, end, "SUICIDE"), end) == 1 ||
            SV_JsonInt(SV_JsonMember(data, end, "TEAMKILL"), end) == 1) {
            return;
        }
        u->stats[STAT_TOTAL_KILLS]++;
        SV_JsonString(SV_JsonMember(killer, end, "WEAPON"), end, text, sizeof(text));
        stat = SV_StatForKey(svKillWeapons, (int)ARRAY_LEN(svKillWeapons), text);
        if (stat >= 0) {
            SV_SteamCountStat(u, stat, qtrue);
        }
        if (!SV_SteamStatsHasAchievement(steamId, AW_SPEED_KILLS) &&
            SV_JsonInt(SV_JsonMember(killer, end, "SPEED"), end) > 500) {
            SV_SteamStatsSetAchievement(steamId, AW_SPEED_KILLS);
        }
        return;
    }

    if (!strcmp(type, "PLAYER_DEATH")) {
        if (Cvar_VariableIntegerValue("g_gametype") == GT_RACE ||
            SV_JsonInt(SV_JsonMember(data, end, "TEAMKILL"), end) == 1) {
            return;
        }
        u->stats[STAT_TOTAL_DEATHS]++;
        SV_JsonString(SV_JsonMember(data, end, "MOD"), end, text, sizeof(text));
        stat = SV_StatForKey(svDeathMods, (int)ARRAY_LEN(svDeathMods), text);
        if (stat >= 0) {
            SV_SteamCountStat(u, stat, qtrue);
        }
        return;
    }

    if (!strcmp(type, "PLAYER_MEDAL")) {
        SV_JsonString(SV_JsonMember(data, end, "MEDAL"), end, text, sizeof(text));
        stat = SV_StatForKey(svMedals, (int)ARRAY_LEN(svMedals), text);
        if (stat < 0) {
            return;
        }
        SV_SteamCountStat(u, stat, qfalse);
        if (!SV_SteamStatsHasAchievement(steamId, AW_MVP) &&
            u->stats[STAT_MEDAL_DEFENSE] + u->stats[STAT_MEDAL_ASSIST] + u->stats[STAT_MEDAL_CAPTURE] >= 1000) {
            SV_SteamStatsSetAchievement(steamId, AW_MVP);
        }
    }
}
