
#include "server.h"
#include "../qcommon/q_steam.h"
#include "../qcommon/q_steamstats.h"
#include "zlib.h"

#define JSON_IMPLEMENTATION
#include "../qcommon/json.h"
#undef JSON_IMPLEMENTATION

#define STEAMSTATS_MAX_USERS (MAX_CLIENTS * 2)

#define STEAMSTATS_NUM_STATS STEAM_NUM_STATS

#define STAT_MEDAL_CAPTURE 74
#define STAT_MEDAL_ASSIST 75
#define STAT_MEDAL_DEFENSE 76
#define STAT_WINS 81
#define STAT_LOSSES 82
#define STAT_PLAYED 83
#define STAT_TOTAL_KILLS 86
#define STAT_TOTAL_DEATHS 87

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

typedef struct {
    const char* name;
    int stat;
} svStatKey_t;

static const svStatKey_t svKillWeapons[] = {
    {"GAUNTLET", 1}, {"MACHINEGUN", 2}, {"SHOTGUN", 3}, {"GRENADE", 4}, {"ROCKET", 5},
    {"LIGHTNING", 6}, {"RAILGUN", 7}, {"PLASMA", 8}, {"BFG", 9}, {"NAILGUN", 10},
    {"PROXMINE", 11}, {"CHAINGUN", 12}, {"HMG", 13},
};

static const svStatKey_t svDeathMods[] = {
    {"SHOTGUN", 38}, {"GAUNTLET", 39}, {"MACHINEGUN", 40}, {"GRENADE", 41}, {"GRENADE_SPLASH", 41},
    {"ROCKET", 42}, {"ROCKET_SPLASH", 42}, {"PLASMA", 43}, {"PLASMA_SPLASH", 43}, {"RAILGUN", 44},
    {"LIGHTNING", 45}, {"BFG", 46}, {"BFG_SPLASH", 46}, {"WATER", 47}, {"SLIME", 48},
    {"LAVA", 49}, {"CRUSH", 50}, {"TELEFRAG", 51}, {"LASER", 52}, {"HURT", 85}, {"NAILGUN", 54},
    {"CHAINGUN", 55}, {"PROXMINE", 56}, {"KAMIKAZE", 57}, {"JUICED", 58}, {"SUICIDE", 59},
    {"FALLING", 60}, {"GRAPPLE", 61}, {"SWITCHTEAM", 0}, {"THAW", 0}, {"HMG", 62},
    {"LIGHTNING_DISCHARGE", 63}, {"OTHER_DEATH", 64},
};

static const svStatKey_t svMedals[] = {
    {"FIRSTFRAG", 65}, {"GAUNTLET", 66}, {"EXCELLENT", 67}, {"REVENGE", 68}, {"COMBOKILL", 69},
    {"MIDAIR", 70}, {"PERFORATED", 71}, {"RAMPAGE", 72}, {"IMPRESSIVE", 73}, {"CAPTURE", 74},
    {"ASSIST", 75}, {"DEFENSE", 76}, {"HEADSHOT", 77}, {"QUADGOD", 78}, {"PERFECT", 79},
    {"ACCURACY", 80},
};

typedef struct {
    qboolean inuse;
    uint64_t steamId;
    qboolean received;
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

static void SV_SteamStoreStats(svSteamUser_t* u) {
    int i;

    if (!u->received) {
        return;
    }
    for (i = 0; i < STEAMSTATS_NUM_STATS; i++) {
        Steam_GS_SetUserStat(u->steamId, steamStatNames[i], u->stats[i]);
    }
    Steam_GS_StoreUserStats(u->steamId);
}

static void SV_SteamReadStats(svSteamUser_t* u, int result) {
    int i;

    if (result != 1) {
        Com_DPrintf("RequestStats - failed, %d\n", result);
        return;
    }
    Com_DPrintf("Received stats and achievements from Steam\n");
    for (i = 0; i < STEAMSTATS_NUM_STATS; i++) {
        Steam_GS_GetUserStat(u->steamId, steamStatNames[i], &u->stats[i]);
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

void SV_SteamStatsStored(int result) {
    int i;

    for (i = 0; i < STEAMSTATS_MAX_USERS; i++) {
        if (!svSteamUsers[i].inuse) {
            continue;
        }
        if (result == 1) {
            Com_DPrintf("StoreStats - success\n");
        } else if (result == 8) {
            Com_DPrintf("StoreStats - some failed to validate\n");
            SV_SteamReadStats(&svSteamUsers[i], 1);
        } else {
            Com_DPrintf("StoreStats - failed, %d\n", result);
        }
    }
}

void SV_SteamStatsConnected(void) {
    int i;

    for (i = 0; i < STEAMSTATS_MAX_USERS; i++) {
        if (svSteamUsers[i].inuse) {
            Steam_GS_RequestUserStats(svSteamUsers[i].steamId);
        }
    }
}

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

void SV_SteamStatsRemoveUser(uint64_t steamId) {
    svSteamUser_t* u = SV_SteamUser(steamId);

    if (!u) {
        return;
    }
    SV_SteamStoreStats(u);
    u->inuse = qfalse;
}

void SV_SteamStatsIncrement(uint64_t steamId, int stat, int amount) {
    svSteamUser_t* u = SV_SteamUser(steamId);

    if (!u || stat < 0 || stat >= STEAMSTATS_NUM_STATS) {
        return;
    }
    u->stats[stat] += amount;
    Com_DPrintf("%llu: %s: %i\n", (unsigned long long)steamId, steamStatNames[stat], u->stats[stat]);
}

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

    if (!u->received) {
        return;
    }
    granted = Steam_GS_SetUserAchievement(steamId, name);
    SV_SteamStoreStats(u);
    if (granted) {
        u->achieved[achievement] = qtrue;
    }
}

qboolean SV_SteamStatsHasAchievement(uint64_t steamId, int achievement) {
    const svSteamUser_t* u = SV_SteamUser(steamId);

    if (!u || !u->received || achievement < 0 || achievement >= STEAMSTATS_NUM_ACHIEVEMENTS) {
        return qfalse;
    }
    return u->achieved[achievement];
}

static const char* SV_JsonMember(const char* json, const char* end, const char* name) {
    if (!json || json >= end || *json != '{') {
        return NULL;
    }
    return JSON_ObjectGetNamedValue(json, end, name);
}

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

static int SV_StatForKey(const svStatKey_t* keys, int numKeys, const char* name) {
    int i;

    for (i = 0; i < numKeys; i++) {
        if (!strcmp(keys[i].name, name)) {
            return keys[i].stat ? keys[i].stat : -1;
        }
    }
    return -1;
}

static void SV_SteamCountStat(svSteamUser_t* u, int stat, qboolean withId) {
    u->stats[stat]++;
    SV_SteamStoreStats(u);
    if (withId) {
        Com_DPrintf("%llu: %s: %i\n", (unsigned long long)u->steamId, steamStatNames[stat], u->stats[stat]);
    } else {
        Com_DPrintf("%s: %i\n", steamStatNames[stat], u->stats[stat]);
    }
}

#define SV_REPORT_MAX_EVENTS 1000
#define SV_REPORT_MAX_TO (MAX_CLIENTS * 2)

typedef struct {
    char* text;
    int len;
    int size;
} svText_t;

static svText_t svReportStats;
static svText_t svReportEvents;
static int svReportNumEvents;
static uint64_t svReportTo[SV_REPORT_MAX_TO];
static int svReportNumTo;

static void SV_TextAdd(svText_t* t, const char* text, int len) {
    if (len < 0) {
        len = (int)strlen(text);
    }
    if (t->len + len + 1 > t->size) {
        int size = t->size ? t->size : 16384;
        char* grown;

        while (size < t->len + len + 1) {
            size *= 2;
        }
        grown = (char*)malloc(size);
        if (!grown) {
            return;
        }
        if (t->text) {
            Com_Memcpy(grown, t->text, t->len);
            free(t->text);
        }
        t->text = grown;
        t->size = size;
    }
    Com_Memcpy(t->text + t->len, text, len);
    t->len += len;
    t->text[t->len] = '\0';
}

static void SV_TextFree(svText_t* t) {
    free(t->text);
    Com_Memset(t, 0, sizeof(*t));
}

static void SV_ReportForget(void) {
    SV_TextFree(&svReportStats);
    SV_TextFree(&svReportEvents);
    svReportNumEvents = 0;
    svReportNumTo = 0;
}

static void SV_ReportCopy(svText_t* t, const char* obj, const char* end, const char* name, const char* none) {
    const char* v = SV_JsonMember(obj, end, name);

    if (v && v < end) {
        SV_TextAdd(t, v, (int)(JSON_ValueGetEnd(v, end) - v));
    } else {
        SV_TextAdd(t, none, -1);
    }
}

static void SV_ReportPerson(svText_t* t, const char* who, const char* end) {
    const char* powerups = SV_JsonMember(who, end, "POWERUPS");

    SV_TextAdd(t, "{\"NAME\":", -1);
    SV_ReportCopy(t, who, end, "NAME", "\"\"");
    if (powerups && JSON_ArrayGetFirstValue(powerups, end)) {
        SV_TextAdd(t, ",\"POWERUPS\":", -1);
        SV_TextAdd(t, powerups, (int)(JSON_ValueGetEnd(powerups, end) - powerups));
    }
    SV_TextAdd(t, ",\"STEAM_ID\":", -1);
    SV_ReportCopy(t, who, end, "STEAM_ID", "\"\"");
    SV_TextAdd(t, ",\"TEAM\":", -1);
    SV_ReportCopy(t, who, end, "TEAM", "null");
    SV_TextAdd(t, "}", 1);
}

static void SV_ReportDeath(const char* data, const char* end) {
    const char* killer;

    if (svReportNumEvents >= SV_REPORT_MAX_EVENTS || Cvar_VariableIntegerValue("g_gametype") == GT_RACE) {
        return;
    }
    if (svReportNumEvents++) {
        SV_TextAdd(&svReportEvents, ",", 1);
    }
    SV_TextAdd(&svReportEvents, "{", 1);
    killer = SV_JsonMember(data, end, "KILLER");
    if (killer && *killer == '{') {
        SV_TextAdd(&svReportEvents, "\"KILLER\":", -1);
        SV_ReportPerson(&svReportEvents, killer, end);
        SV_TextAdd(&svReportEvents, ",", 1);
    }
    SV_TextAdd(&svReportEvents, "\"MOD\":", -1);
    SV_ReportCopy(&svReportEvents, data, end, "MOD", "\"\"");
    SV_TextAdd(&svReportEvents, ",\"TIME\":", -1);
    SV_ReportCopy(&svReportEvents, data, end, "TIME", "0");
    SV_TextAdd(&svReportEvents, ",\"VICTIM\":", -1);
    SV_ReportPerson(&svReportEvents, SV_JsonMember(data, end, "VICTIM"), end);
    SV_TextAdd(&svReportEvents, "}", 1);
}

static void SV_ReportPlayer(const char* data, const char* end) {
    if (svReportStats.len) {
        SV_TextAdd(&svReportStats, ",", 1);
    }
    SV_TextAdd(&svReportStats, data, (int)(JSON_ValueGetEnd(data, end) - data));
}

static void SV_ReportTo(uint64_t steamId) {
    int i;

    for (i = 0; i < svReportNumTo; i++) {
        if (svReportTo[i] == steamId) {
            return;
        }
    }
    if (svReportNumTo < SV_REPORT_MAX_TO) {
        svReportTo[svReportNumTo++] = steamId;
    }
}

static void SV_ReportSend(const char* data, const char* end) {
    svText_t out;
    const char* close;
    Bytef* packed;
    uLongf packedLen;
    int i;

    if (SV_JsonInt(SV_JsonMember(data, end, "TRAINING"), end) || SV_JsonInt(SV_JsonMember(data, end, "ABORTED"), end)) {
        SV_ReportForget();
        return;
    }

    close = JSON_ValueGetEnd(data, end) - 1;
    if (close <= data || *close != '}') {
        SV_ReportForget();
        return;
    }
    Com_Memset(&out, 0, sizeof(out));
    SV_TextAdd(&out, data, (int)(close - data));
    if (JSON_ArrayGetFirstValue(data, end)) {
        SV_TextAdd(&out, ",", 1);
    }
    SV_TextAdd(&out, "\"PLYR_EVENTS\":[", -1);
    SV_TextAdd(&out, svReportEvents.text ? svReportEvents.text : "", svReportEvents.len);
    SV_TextAdd(&out, "],\"PLYR_STATS\":[", -1);
    SV_TextAdd(&out, svReportStats.text ? svReportStats.text : "", svReportStats.len);
    SV_TextAdd(&out, "]}", 2);

    packedLen = compressBound((uLong)out.len);
    packed = (Bytef*)malloc(packedLen);
    if (out.text && packed && compress2(packed, &packedLen, (const Bytef*)out.text, (uLong)out.len, 9) == Z_OK) {
        for (i = 0; i < svReportNumTo; i++) {
            Steam_GS_SendReport(svReportTo[i], packed, (int)packedLen);
        }
        Com_DPrintf("match report: %i bytes, %i packed, to %i players\n", out.len, (int)packedLen, svReportNumTo);
    }
    free(packed);
    SV_TextFree(&out);
    SV_ReportForget();
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
    if (!strcmp(type, "MATCH_REPORT")) {
        SV_ReportSend(data, end);
        return;
    }
    if (SV_JsonInt(SV_JsonMember(data, end, "WARMUP"), end) == 1 ||
        SV_JsonInt(SV_JsonMember(data, end, "ABORTED"), end) == 1) {
        return;
    }
    if (!strcmp(type, "PLAYER_STATS")) {
        SV_ReportPlayer(data, end);
    } else if (!strcmp(type, "PLAYER_DEATH")) {
        SV_ReportDeath(data, end);
    }
    u = SV_SteamUser(steamId);
    if (!u) {
        return;
    }

    if (!strcmp(type, "PLAYER_STATS")) {
        int win = SV_JsonInt(SV_JsonMember(data, end, "WIN"), end);

        SV_ReportTo(steamId);

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
