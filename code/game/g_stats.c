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

// [ioquakelive] how many of each medal a player has, for the event's TOTAL. Retail reads
// it from its own counters; which one feeds each call is not verified against the binary.
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

// how many of that medal the player has, this one included
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
the run's time in milliseconds (client->race.lastTime) and WEAPONS_USED says
whether the player shot anything during the run (client->race.weaponUsed).
===============
*/
void STAT_PlayerRaceComplete(gentity_t* ent, int raceTime, qboolean weaponsUsed) {
    jnode_t* data;

    if (!ent || !ent->client) {
        return;
    }
    data = STAT_Begin();
    J_SetString(data, "NAME", ent->client->pers.netname);
    STAT_SteamId(data, ent);
    J_SetBool(data, "WEAPONS_USED", weaponsUsed);  // [QL] 0x10071f84: a boolean, not a count
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

/*
===============================================================================

PLAYER_KILL and PLAYER_DEATH

[QL] 0x100726e0, called from player_die (0x1004751a). Retail builds a single document
and sends it twice: as PLAYER_KILL to the killer's account and as PLAYER_DEATH to the
victim's. The same function keeps the death and streak counts, which here
STAT_AddPlayerDeathStat already does, and unlocks Steam achievements, which are not
implemented.

===============================================================================
*/

// [QL] 0x100711b0: the MOD's name
static const char* STAT_ModName(int mod) {
    switch (mod) {
        case MOD_SHOTGUN: return "SHOTGUN";
        case MOD_GAUNTLET: return "GAUNTLET";
        case MOD_MACHINEGUN: return "MACHINEGUN";
        case MOD_GRENADE: return "GRENADE";
        case MOD_GRENADE_SPLASH: return "GRENADE_SPLASH";
        case MOD_ROCKET: return "ROCKET";
        case MOD_ROCKET_SPLASH: return "ROCKET_SPLASH";
        case MOD_PLASMA: return "PLASMA";
        case MOD_PLASMA_SPLASH: return "PLASMA_SPLASH";
        case MOD_RAILGUN: return "RAILGUN";
        case MOD_LIGHTNING: return "LIGHTNING";
        case MOD_BFG: return "BFG";
        case MOD_BFG_SPLASH: return "BFG_SPLASH";
        case MOD_WATER: return "WATER";
        case MOD_SLIME: return "SLIME";
        case MOD_LAVA: return "LAVA";
        case MOD_CRUSH: return "CRUSH";
        case MOD_TELEFRAG: return "TELEFRAG";
        case MOD_FALLING: return "FALLING";
        case MOD_SUICIDE: return "SUICIDE";
        case MOD_TARGET_LASER: return "LASER";
        case MOD_TRIGGER_HURT: return "HURT";
        case MOD_NAIL: return "NAILGUN";
        case MOD_CHAINGUN: return "CHAINGUN";
        case MOD_PROXIMITY_MINE: return "PROXMINE";
        case MOD_KAMIKAZE: return "KAMIKAZE";
        case MOD_JUICED: return "JUICED";
        case MOD_GRAPPLE: return "GRAPPLE";
        case MOD_SWITCH_TEAMS: return "SWITCHTEAM";
        case MOD_THAW: return "THAW";
        case MOD_LIGHTNING_DISCHARGE: return "LIGHTNING_DISCHARGE";
        case MOD_HMG: return "HMG";
        case MOD_RAILGUN_HEADSHOT: return "RAILGUN_HEADSHOT";
        default: return "OTHER_DEATH";
    }
}

// [QL] 0x100713e0: the weapon's name. The grapple has no name of its own
static const char* STAT_WeaponName(int weapon) {
    switch (weapon) {
        case WP_GAUNTLET: return "GAUNTLET";
        case WP_MACHINEGUN: return "MACHINEGUN";
        case WP_SHOTGUN: return "SHOTGUN";
        case WP_GRENADE_LAUNCHER: return "GRENADE";
        case WP_ROCKET_LAUNCHER: return "ROCKET";
        case WP_LIGHTNING: return "LIGHTNING";
        case WP_RAILGUN: return "RAILGUN";
        case WP_PLASMAGUN: return "PLASMA";
        case WP_BFG: return "BFG";
        case WP_NAILGUN: return "NAILGUN";
        case WP_PROX_LAUNCHER: return "PROXMINE";
        case WP_CHAINGUN: return "CHAINGUN";
        case WP_HMG: return "HMG";
        default: return "OTHER_WEAPON";
    }
}

// [QL] 0x10071480: the powerup's name
static const char* STAT_PowerupName(int pw) {
    switch (pw) {
        case PW_REDFLAG: return "RED_FLAG";
        case PW_BLUEFLAG: return "BLUE_FLAG";
        case PW_NEUTRALFLAG: return "NEUTRAL_FLAG";
        case PW_QUAD: return "QUAD";
        case PW_BATTLESUIT: return "BATTLESUIT";
        case PW_HASTE: return "HASTE";
        case PW_INVIS: return "INVIS";
        case PW_REGEN: return "REGEN";
        case PW_FLIGHT: return "FLIGHT";
        case PW_INVULNERABILITY: return "INVULNERABILITY";
        case PW_SCOUT: return "SCOUT";
        case PW_GUARD: return "GUARD";
        case PW_DOUBLER: return "DOUBLER";
        case PW_AMMOREGEN: return "ARMOR_REGEN";  // [QL] the fourth persistant is called this in the stats
        default: return "OTHER_POWERUP";
    }
}

// [QL] the holdable comes from the giTag of the carried item
static const char* STAT_HoldableName(int giTag) {
    switch (giTag) {
        case HI_TELEPORTER: return "TELEPORTER";
        case HI_MEDKIT: return "MEDKIT";
        case HI_KAMIKAZE: return "KAMIKAZE";
        case HI_PORTAL: return "PORTAL";
        case HI_INVULNERABILITY: return "INVULNERABILITY";
        default: return "OTHER_HOLDABLE";
    }
}

/*
===============
STAT_PlayerFrame

What the event says about a player at the moment of death: which weapon, what shape
they were in and where. The killer goes with the MOD's weapon and the health they have
now; the victim, with the weapon they were holding and the health and armor they had
before the last hit (expandedStats.previousHealth / previousArmor).
===============
*/
static void STAT_PlayerFrame(jnode_t* obj, gentity_t* ent, int weapon, int health, int armor) {
    gclient_t* cl = ent->client;
    jnode_t* pos;
    jnode_t* view;
    jnode_t* powerups;
    float speed;
    int i;

    J_SetString(obj, "NAME", cl->pers.netname);
    STAT_SteamId(obj, ent);
    J_SetString(obj, "WEAPON", STAT_WeaponName(weapon));
    J_SetBool(obj, "AIRBORNE",
              (qboolean)(cl->ps.groundEntityNum == ENTITYNUM_NONE || cl->ps.powerups[PW_FLIGHT] != 0));
    J_SetInt(obj, "HEALTH", health);
    J_SetInt(obj, "ARMOR", armor);
    J_SetInt(obj, "AMMO", cl->ps.ammo[weapon]);
    J_SetInt(obj, "TEAM", cl->sess.sessionTeam);

    // speed is horizontal only, and goes out as the float it is
    speed = (float)sqrt((double)(cl->ps.velocity[0] * cl->ps.velocity[0] +
                                 cl->ps.velocity[1] * cl->ps.velocity[1]));
    J_SetFloat(obj, "SPEED", speed);
    J_SetBool(obj, "SUBMERGED",
              (qboolean)(ent->waterlevel != 0 && (ent->watertype & CONTENTS_WATER) != 0));

    pos = J_Object();
    J_SetFloat(pos, "X", cl->ps.origin[0]);
    J_SetFloat(pos, "Y", cl->ps.origin[1]);
    J_SetFloat(pos, "Z", cl->ps.origin[2]);
    J_SetNode(obj, "POSITION", pos);

    view = J_Object();
    J_SetFloat(view, "X", cl->ps.viewangles[0]);
    J_SetFloat(view, "Y", cl->ps.viewangles[1]);
    J_SetFloat(view, "Z", cl->ps.viewangles[2]);
    J_SetNode(obj, "VIEW", view);

    // the flags do not count: the walk starts at the quad
    powerups = J_Array();
    for (i = PW_QUAD; i < PW_NUM_POWERUPS; i++) {
        if (cl->ps.powerups[i]) {
            J_PushString(powerups, STAT_PowerupName(i));
        }
    }
    J_SetNode(obj, "POWERUPS", powerups);

    // the holdable only shows when one is carried
    if (cl->ps.stats[STAT_HOLDABLE_ITEM]) {
        J_SetString(obj, "HOLDABLE",
                    STAT_HoldableName(bg_itemlist[cl->ps.stats[STAT_HOLDABLE_ITEM]].giTag));
    }

    if (ent->r.svFlags & SVF_BOT) {
        char userinfo[MAX_INFO_STRING];

        trap_GetUserinfo(cl->ps.clientNum, userinfo, sizeof(userinfo));
        J_SetBool(obj, "BOT", qtrue);
        J_SetFloat(obj, "BOT_SKILL", atof(Info_ValueForKey(userinfo, "skill")));
    } else {
        J_SetBool(obj, "BOT", qfalse);
        J_SetNull(obj, "BOT_SKILL");  // [QL] 0x10072f49: a human gets jsoncpp's null
    }
}

/*
===============
STAT_PlayerKill

victimStreak is the streak the victim was on, read before the death cuts it, which is
the order retail has inside its own function.
===============
*/
void STAT_PlayerKill(gentity_t* victim, gentity_t* attacker, int mod, int victimStreak) {
    jnode_t* data;
    jnode_t* who;
    qboolean teamkill;
    int alive[4];
    int dead[4];
    int i;

    // [QL] 0x10072716: while the game is moving people between teams (shuffle or balance)
    // nothing goes out. The global is 0x105df558 = level.teamShuffleActive, not
    // level.scoringDisabled.
    if (level.teamShuffleActive) {
        return;
    }

    data = STAT_Begin();
    J_SetString(data, "MOD", STAT_ModName(mod));

    teamkill = qfalse;
    if (attacker && attacker->client && victim && victim->client && attacker != victim &&
        (g_gametype.integer >= GT_TEAM || attacker->client->sess.sessionTeam == TEAM_SPECTATOR) &&
        attacker->client->sess.sessionTeam == victim->client->sess.sessionTeam) {
        teamkill = qtrue;
    }
    J_SetBool(data, "TEAMKILL", teamkill);
    J_SetBool(data, "SUICIDE", (qboolean)(attacker == victim));

    // retail leaves both null and only fills them when there is somebody
    J_SetNull(data, "KILLER");
    if (attacker && attacker->client) {
        who = J_Object();
        STAT_PlayerFrame(who, attacker, BG_GetWeaponFromMeansOfDeath(mod),
                         attacker->client->ps.stats[STAT_HEALTH],
                         attacker->client->ps.stats[STAT_ARMOR]);
        J_SetNode(data, "KILLER", who);
    }
    J_SetNull(data, "VICTIM");
    if (victim && victim->client) {
        who = J_Object();
        J_SetInt(who, "STREAK", victimStreak);
        STAT_PlayerFrame(who, victim, victim->client->ps.weapon,
                         victim->client->expandedStats.previousHealth,
                         victim->client->expandedStats.previousArmor);
        J_SetNode(data, "VICTIM", who);
    }

    J_SetNull(data, "ROUND");
    switch (g_gametype.integer) {
        case GT_CA:
        case GT_FREEZE:
        case GT_AD:
        case GT_RR:
            // in attack & defend a round counts both turns
            J_SetInt(data, "ROUND",
                     g_gametype.integer == GT_AD ? level.roundState.turn + level.roundState.round * 2
                                                 : level.roundState.round);
            break;
        default:
            break;
    }

    J_SetNull(data, "TEAM_ALIVE");
    J_SetNull(data, "TEAM_DEAD");
    J_SetNull(data, "OTHER_TEAM_ALIVE");
    J_SetNull(data, "OTHER_TEAM_DEAD");
    if (g_gametype.integer > GT_RACE && victim && victim->client) {
        int mine = victim->client->sess.sessionTeam == TEAM_RED ? TEAM_RED : TEAM_BLUE;
        int other = victim->client->sess.sessionTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED;

        // [QL] 0x10071520: alive is whoever is playing (normal pm_type), dead is everyone else
        Com_Memset(alive, 0, sizeof(alive));
        Com_Memset(dead, 0, sizeof(dead));
        for (i = 0; i < level.maxclients; i++) {
            gclient_t* cl = g_entities[i].client;

            if (!cl || cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            if (cl->sess.sessionTeam < 0 || cl->sess.sessionTeam >= 4) {
                continue;
            }
            if (cl->ps.pm_type == PM_NORMAL) {
                alive[cl->sess.sessionTeam]++;
            } else {
                dead[cl->sess.sessionTeam]++;
            }
        }
        J_SetInt(data, "TEAM_ALIVE", alive[mine]);
        J_SetInt(data, "TEAM_DEAD", dead[mine]);
        J_SetInt(data, "OTHER_TEAM_ALIVE", alive[other]);
        J_SetInt(data, "OTHER_TEAM_DEAD", dead[other]);
    }

    // the same document, to both accounts
    if (attacker && attacker->client) {
        STAT_Send(STAT_Account(attacker), "PLAYER_KILL", data);
    }
    if (victim && victim->client) {
        STAT_Send(STAT_Account(victim), "PLAYER_DEATH", data);
    }
}

/*
===============
STAT_MatchStarted

[QL] 0x10077a30, called when the warmup countdown runs out (0x10058860), before the
map_restart that starts the match. This event carries no TIME / WARMUP prologue: it
carries the rules being played and who was present. It goes out with no account
(steam id 0).
===============
*/

// [QL] 0x10071110: the gametype's short name
static const char* STAT_GameTypeName(int gametype) {
    switch (gametype) {
        case GT_FFA: return "FFA";
        case GT_DUEL: return "DUEL";
        case GT_RACE: return "RACE";
        case GT_TEAM: return "TDM";
        case GT_CA: return "CA";
        case GT_CTF: return "CTF";
        case GT_1FCTF: return "1FCTF";
        case GT_OBELISK: return "OBELISK";
        case GT_HARVESTER: return "HARVESTER";
        case GT_FREEZE: return "FT";
        case GT_DOMINATION: return "DOM";
        case GT_AD: return "AD";
        case GT_RR: return "RR";
        default: return "OTHER_GAME";
    }
}

void STAT_MatchStarted(void) {
    jnode_t* data;
    jnode_t* players;
    char serverinfo[MAX_INFO_STRING];
    char mapname[MAX_QPATH];
    char hostname[64];
    char guid[MAX_STRING_CHARS];
    int i;

    J_Reset();
    data = J_Object();

    J_SetString(data, "GAME_TYPE", STAT_GameTypeName(g_gametype.integer));
    J_SetString(data, "FACTORY", g_factory.string);
    J_SetString(data, "FACTORY_TITLE", g_factoryTitle.string);

    // the map comes from the serverinfo and goes out in lower case
    trap_GetServerinfo(serverinfo, sizeof(serverinfo));
    Q_strncpyz(mapname, Info_ValueForKey(serverinfo, "mapname"), sizeof(mapname));
    Q_strlwr(mapname);
    J_SetString(data, "MAP", mapname);

    J_SetInt(data, "FRAG_LIMIT", g_fraglimit.integer);
    J_SetInt(data, "CAPTURE_LIMIT", g_capturelimit.integer);
    J_SetInt(data, "SCORE_LIMIT", g_scorelimit.integer);
    J_SetInt(data, "MERCY_LIMIT", g_mercylimit.integer);
    J_SetInt(data, "TIME_LIMIT", g_timelimit.integer);
    J_SetInt(data, "ROUND_LIMIT", roundlimit.integer);
    J_SetInt(data, "TRAINING", g_training.integer);
    J_SetInt(data, "INSTAGIB", g_instaGib.integer);
    J_SetInt(data, "QUADHOG", g_quadHog.integer);
    J_SetInt(data, "INFECTED", g_rrInfected.integer);

    trap_GetConfigstring(CS_MATCH_GUID, guid, sizeof(guid));
    J_SetString(data, "MATCH_GUID", guid);

    // [QL] 0x1007801b: the server title comes from the cvar, not from the serverinfo
    trap_Cvar_VariableStringBuffer("sv_hostname", hostname, sizeof(hostname));
    J_SetString(data, "SERVER_TITLE", hostname);

    // everyone on the server, even those still connecting
    players = J_Array();
    for (i = 0; i < level.maxclients; i++) {
        gentity_t* ent = &g_entities[i];
        jnode_t* who;

        if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED) {
            continue;
        }
        who = J_Object();
        J_SetString(who, "NAME", ent->client->pers.netname);
        STAT_SteamId(who, ent);
        J_SetInt(who, "TEAM", ent->client->sess.sessionTeam);
        J_Push(players, who);
    }
    J_SetNode(data, "PLAYERS", players);

    STAT_Send(0, "MATCH_STARTED", data);
}

/*
===============================================================================

PLAYER_STATS

[QL] 0x10074360. A player's whole stats sheet. It goes out three times:

  - at the end of the match, from LogExit (0x100576eb), with quit = 0
  - when somebody leaves, from ClientDisconnect (0x1003c812), with quit = 1
  - when somebody leaves their team, from SetTeam (0x10040535), with quit = 1

Nothing goes out for a player who is still connecting, in intermission or a
spectator.

===============================================================================
*/

// [QL] the value retail uses for "this team or player has no score"
#define STAT_NO_SCORE (-999)

// [QL] 0x10074360: powerups go by the name of the one picked up, flags excluded
static void STAT_PickupCounts(jnode_t* pickups, const gclient_t* cl) {
    int i;

    for (i = PW_QUAD; i < PW_NUM_POWERUPS; i++) {
        J_SetInt(pickups, STAT_PowerupName(i), cl->expandedStats.powerups[i]);
    }
    for (i = HI_TELEPORTER; i < HI_NUM_HOLDABLE; i++) {
        J_SetInt(pickups, STAT_HoldableName(i), cl->expandedStats.holdablePickups[i]);
    }
    J_SetInt(pickups, "MEGA_HEALTH", cl->expandedStats.numMegaHealthPickups);
    J_SetInt(pickups, "AMMO", cl->expandedStats.numAmmoPickups);
    J_SetInt(pickups, "HEALTH", cl->expandedStats.numHealthPickups);
    J_SetInt(pickups, "ARMOR", cl->expandedStats.numArmorPickups);
    J_SetInt(pickups, "RED_ARMOR", cl->expandedStats.numRedArmorPickups);
    J_SetInt(pickups, "GREEN_ARMOR", cl->expandedStats.numGreenArmorPickups);
    J_SetInt(pickups, "YELLOW_ARMOR", cl->expandedStats.numYellowArmorPickups);
}

// the average time taken to return to each item; only items ever picked up are listed
static void STAT_ItemTiming(jnode_t* data, const gclient_t* cl) {
    jnode_t* timing = J_Object();
    const expandedStatObj_t* st = &cl->expandedStats;

    if (st->numMegaHealthPickups) {
        J_SetInt(timing, "MEGA_HEALTH", st->megaHealthPickupTime / st->numMegaHealthPickups);
    }
    if (st->numRedArmorPickups) {
        J_SetInt(timing, "RED_ARMOR", st->redArmorPickupTime / st->numRedArmorPickups);
    }
    if (st->numYellowArmorPickups) {
        J_SetInt(timing, "YELLOW_ARMOR", st->yellowArmorPickupTime / st->numYellowArmorPickups);
    }
    if (st->numGreenArmorPickups) {
        J_SetInt(timing, "GREEN_ARMOR", st->greenArmorPickupTime / st->numGreenArmorPickups);
    }
    J_SetNode(data, "ITEM_TIMING", timing);
}

// [QL] the medals: three come from persistant and ten from the awards array
static void STAT_Medals(jnode_t* data, const gclient_t* cl) {
    jnode_t* medals = J_Object();

    J_SetInt(medals, "IMPRESSIVE", cl->ps.persistant[PERS_IMPRESSIVE_COUNT]);
    J_SetInt(medals, "EXCELLENT", cl->ps.persistant[PERS_EXCELLENT_COUNT]);
    J_SetInt(medals, "HUMILIATION", cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]);
    J_SetInt(medals, "REVENGE", cl->ps.localPersistant[AWARD_REVENGE + 1]);
    J_SetInt(medals, "COMBOKILL", cl->ps.localPersistant[AWARD_COMBOKILL + 1]);
    J_SetInt(medals, "RAMPAGE", cl->ps.localPersistant[AWARD_RAMPAGE + 1]);
    J_SetInt(medals, "MIDAIR", cl->ps.localPersistant[AWARD_MIDAIR + 1]);
    J_SetInt(medals, "PERFORATED", cl->ps.localPersistant[AWARD_PERFORATED + 1]);
    J_SetInt(medals, "HEADSHOT", cl->ps.localPersistant[AWARD_HEADSHOT + 1]);
    J_SetInt(medals, "ACCURACY", cl->ps.localPersistant[AWARD_ACCURACY + 1]);
    J_SetInt(medals, "QUADGOD", cl->ps.localPersistant[AWARD_QUADGOD + 1]);
    J_SetInt(medals, "FIRSTFRAG", cl->ps.localPersistant[AWARD_FIRSTFRAG + 1]);
    J_SetInt(medals, "PERFECT", cl->ps.localPersistant[AWARD_PERFECT + 1]);
    J_SetInt(medals, "DEFENDS", cl->expandedStats.numDefends);
    J_SetInt(medals, "ASSISTS", cl->expandedStats.numAssists);
    J_SetInt(medals, "CAPTURES", cl->expandedStats.numCaptures);
    J_SetNode(data, "MEDALS", medals);
}

// [QL] 0x10076de1: who won. A team with fewer points still wins if the other one ran
// out of players; outside team modes the top of the ranking wins, or anybody at all on
// a forfeit
static void STAT_WinLose(jnode_t* data, const gclient_t* cl) {
    qboolean win = qfalse;

    if (g_gametype.integer < 0 || (g_gametype.integer > GT_RACE && g_gametype.integer != GT_RR)) {
        int mine = cl->sess.sessionTeam == TEAM_RED ? TEAM_RED : TEAM_BLUE;
        int other = cl->sess.sessionTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED;

        if ((cl->sess.sessionTeam == TEAM_RED || cl->sess.sessionTeam == TEAM_BLUE) &&
            level.teamScores[mine] != STAT_NO_SCORE) {
            if (level.teamScores[mine] > level.teamScores[other]) {
                win = qtrue;
            } else {
                win = (qboolean)(TeamCount(-1, (team_t)other) < 1);
            }
        }
    } else if (cl->ps.persistant[PERS_SCORE] != STAT_NO_SCORE &&
               (cl->expandedStats.serverRank == 0 || level.matchForfeited)) {
        win = qtrue;
    }
    J_SetInt(data, "WIN", win ? 1 : 0);
    J_SetInt(data, "LOSE", win ? 0 : 1);
}

void STAT_PlayerStats(gclient_t* cl, qboolean aborted, qboolean quit) {
    jnode_t* data;
    jnode_t* weapons;
    jnode_t* pickups;
    jnode_t* damage;
    char guid[MAX_STRING_CHARS];
    char userinfo[MAX_INFO_STRING];
    int i;

    if (!cl || cl->pers.connected == CON_CONNECTING || cl->ps.pm_type == PM_INTERMISSION ||
        cl->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    J_Reset();
    data = J_Object();

    J_SetString(data, "NAME", cl->pers.netname);
    // [QL] 0x100743fc: a bot does not carry "0" here -- it carries its account, which is
    // zero anyway
    J_SetString(data, "STEAM_ID", va("%llu", (unsigned long long)cl->pers.steamId));
    trap_GetConfigstring(CS_MATCH_GUID, guid, sizeof(guid));
    J_SetString(data, "MATCH_GUID", guid);
    J_SetBool(data, "WARMUP", (qboolean)(level.warmupTime != 0));
    J_SetBool(data, "ABORTED", aborted);

    trap_GetUserinfo(cl->ps.clientNum, userinfo, sizeof(userinfo));
    J_SetString(data, "MODEL", Info_ValueForKey(userinfo, "model"));
    J_SetInt(data, "PLAY_TIME", (level.time - cl->pers.enterTime) / 1000);

    // in race, whoever set no time has the score at its ceiling; it goes out as -1
    if (g_gametype.integer == GT_RACE && cl->ps.persistant[PERS_SCORE] == 0x7fffffff) {
        J_SetInt(data, "SCORE", -1);
    } else {
        J_SetInt(data, "SCORE", cl->ps.persistant[PERS_SCORE]);
    }

    // weapon by weapon: kills, deaths, shots, hits, damage, time and how many were picked up
    weapons = J_Object();
    for (i = 1; i < WP_NUM_WEAPONS; i++) {
        jnode_t* w = J_Object();

        J_SetInt(w, "K", cl->expandedStats.numWeaponKills[i]);
        J_SetInt(w, "D", cl->expandedStats.numWeaponDeaths[i]);
        J_SetInt(w, "S", cl->expandedStats.shotsFired[i]);
        J_SetInt(w, "H", cl->expandedStats.shotsHit[i]);
        J_SetInt(w, "DG", cl->expandedStats.damageDealt[i]);
        J_SetInt(w, "DR", cl->expandedStats.damageTaken[i]);
        J_SetInt(w, "T", cl->expandedStats.weaponUsageTime[i] / 1000);
        J_SetInt(w, "P", cl->expandedStats.weaponPickups[i]);
        J_SetNode(weapons, STAT_WeaponName(i), w);
    }
    J_SetNode(data, "WEAPONS", weapons);

    J_SetInt(data, "MAX_STREAK", cl->expandedStats.maxKillStreak);

    pickups = J_Object();
    STAT_PickupCounts(pickups, cl);
    J_SetNode(data, "PICKUPS", pickups);

    STAT_ItemTiming(data, cl);

    damage = J_Object();
    J_SetInt(damage, "DEALT", cl->expandedStats.totalDamageDealt);
    J_SetInt(damage, "TAKEN", cl->expandedStats.totalDamageTaken);
    J_SetNode(data, "DAMAGE", damage);

    J_SetInt(data, "KILLS", cl->expandedStats.numKills);
    J_SetInt(data, "DEATHS", cl->expandedStats.numDeaths);

    STAT_Medals(data, cl);

    // the team fields only appear in team modes
    if (g_gametype.integer > GT_RACE) {
        J_SetInt(data, "TEAM", cl->sess.sessionTeam);
        J_SetInt(data, "HOLY_SHITS", cl->expandedStats.numHolyShits);
        J_SetInt(data, "TEAM_JOIN_TIME", cl->expandedStats.teamJoinTime / 1000);
        J_SetInt(data, "RED_FLAG_PICKUPS", cl->expandedStats.numRedFlagPickups);
        J_SetInt(data, "BLUE_FLAG_PICKUPS", cl->expandedStats.numBlueFlagPickups);
        J_SetInt(data, "NEUTRAL_FLAG_PICKUPS", cl->expandedStats.numNeutralFlagPickups);
    }

    if (!quit) {
        J_SetInt(data, "RANK", cl->expandedStats.serverRank + 1);
        J_SetInt(data, "TIED_RANK", cl->expandedStats.serverRankIsTied == 1 ? 1 : 0);
        if (g_gametype.integer < 0 || (g_gametype.integer > GT_RACE && g_gametype.integer != GT_RR)) {
            J_SetInt(data, "TEAM_RANK", cl->expandedStats.teamRank + 1);
            J_SetInt(data, "TIED_TEAM_RANK", cl->expandedStats.teamRankIsTied == 1 ? 1 : 0);
        }
        J_SetInt(data, "QUIT", 0);
        STAT_WinLose(data, cl);
    } else {
        // whoever left has no rank and won nothing
        J_SetInt(data, "RANK", -1);
        J_SetInt(data, "TIED_RANK", 1);
        if (g_gametype.integer >= GT_TEAM) {
            J_SetInt(data, "TEAM_RANK", -1);
            J_SetInt(data, "TIED_TEAM_RANK", 1);
        }
        J_SetInt(data, "QUIT", 1);
        J_SetInt(data, "WIN", 0);
        J_SetInt(data, "LOSE", 0);
    }

    STAT_Send(cl->pers.steamId, "PLAYER_STATS", data);
}

/*
===============
STAT_MatchReport

[QL] 0x10076fe0, the last thing LogExit does (0x10057785). It closes the match: which
rules were played, how it ended and who scored. Retail publishes it through a
different import (+0x2e4, 0x004f4c30) as MATCH_REPORT; here it goes out like every
other event, with the same document. Once per match, never on a map_restart.
===============
*/
void STAT_MatchReport(qboolean aborted, int restarted, const char* exitMsg) {
    jnode_t* data;
    char serverinfo[MAX_INFO_STRING];
    char mapname[MAX_QPATH];
    char hostname[64];
    char guid[MAX_STRING_CHARS];

    J_Reset();
    data = J_Object();

    J_SetString(data, "GAME_TYPE", STAT_GameTypeName(g_gametype.integer));
    J_SetString(data, "FACTORY", g_factory.string);
    J_SetString(data, "FACTORY_TITLE", g_factoryTitle.string);

    J_SetInt(data, "TSCORE0", level.teamScores[TEAM_RED]);
    J_SetInt(data, "TSCORE1", level.teamScores[TEAM_BLUE]);
    J_SetInt(data, "GAME_LENGTH", (level.time - level.startTime) / 1000);

    trap_GetServerinfo(serverinfo, sizeof(serverinfo));
    Q_strncpyz(mapname, Info_ValueForKey(serverinfo, "mapname"), sizeof(mapname));
    Q_strlwr(mapname);
    J_SetString(data, "MAP", mapname);

    J_SetInt(data, "RESTARTED", restarted);
    J_SetBool(data, "ABORTED", aborted);

    J_SetInt(data, "FRAG_LIMIT", g_fraglimit.integer);
    J_SetInt(data, "CAPTURE_LIMIT", g_capturelimit.integer);
    J_SetInt(data, "SCORE_LIMIT", g_scorelimit.integer);
    J_SetInt(data, "MERCY_LIMIT", g_mercylimit.integer);
    J_SetInt(data, "TIME_LIMIT", g_timelimit.integer);
    J_SetInt(data, "ROUND_LIMIT", roundlimit.integer);
    J_SetString(data, "EXIT_MSG", exitMsg ? exitMsg : "");
    J_SetInt(data, "TRAINING", g_training.integer);
    J_SetInt(data, "INSTAGIB", g_instaGib.integer);
    J_SetInt(data, "QUADHOG", g_quadHog.integer);
    J_SetInt(data, "INFECTED", g_rrInfected.integer);

    // [QL] 0x100776fe: with nobody scoring, it is "none"
    J_SetString(data, "FIRST_SCORER", level.firstScorer[0] ? level.firstScorer : "none");
    J_SetString(data, "LAST_SCORER", level.lastScorer[0] ? level.lastScorer : "none");
    J_SetString(data, "LAST_TEAMSCORER", level.lastTeamScorer[0] ? level.lastTeamScorer : "none");

    trap_GetConfigstring(CS_MATCH_GUID, guid, sizeof(guid));
    J_SetString(data, "MATCH_GUID", guid);
    trap_Cvar_VariableStringBuffer("sv_hostname", hostname, sizeof(hostname));
    J_SetString(data, "SERVER_TITLE", hostname);

    J_SetInt(data, "LAST_LEAD_CHANGE_TIME", level.lastLeadChangeTime);

    STAT_Send(0, "MATCH_REPORT", data);
}

/*
===============
STAT_UpdatePlayTime

[QL] 0x10078960, called from ClientThink_real (0x100353cb) while the player is alive.
It adds the time since the last frame to the total played and, if they are in play, to
that of the weapon they hold. PLAY_TIME and the "T" of each weapon in PLAYER_STATS come
from here.
===============
*/
void STAT_UpdatePlayTime(gentity_t* ent) {
    gclient_t* cl;
    int delta;

    if (!ent || !ent->client) {
        return;
    }
    cl = ent->client;
    delta = level.time - cl->expandedStats.lastThinkTime;

    if (cl->ps.pm_type == PM_INTERMISSION) {
        return;
    }
    cl->expandedStats.totalPlayTime += delta;

    if (cl->sess.sessionTeam != TEAM_SPECTATOR && cl->ps.pm_type != PM_DEAD) {
        cl->expandedStats.weaponUsageTime[cl->ps.weapon] += delta;
    }
    cl->expandedStats.lastThinkTime = level.time;
}
