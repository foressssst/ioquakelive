
#include "g_local.h"
#include "g_json.h"

#define STAT_DOC_SIZE 65536

static char statDoc[STAT_DOC_SIZE];

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

static void STAT_SteamId(jnode_t* obj, const gentity_t* ent) {
    if (ent->r.svFlags & SVF_BOT) {
        J_SetString(obj, "STEAM_ID", "0");
    } else {
        J_SetString(obj, "STEAM_ID", va("%llu", (unsigned long long)ent->client->pers.steamId));
    }
}

static const char* STAT_TeamName(int team) {
    switch (team) {
        case TEAM_RED: return "RED";
        case TEAM_BLUE: return "BLUE";
        case TEAM_SPECTATOR: return "SPECTATOR";
        default: return "FREE";
    }
}

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

int STAT_MedalTotal(gentity_t* ent, const char* medal) {
    if (!ent || !ent->client) {
        return 1;
    }
    return STAT_CountMedal((int)(ent - g_entities), medal);
}

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

void STAT_RoundOver(int round, int winningTeam, qboolean draw) {
    jnode_t* data = STAT_Begin();

    J_SetInt(data, "ROUND", round);
    J_SetString(data, "TEAM_WON", draw ? "DRAW" : STAT_TeamName(winningTeam));
    STAT_Send(0, "ROUND_OVER", data);
}

void STAT_PlayerRaceComplete(gentity_t* ent, int raceTime, qboolean weaponsUsed) {
    jnode_t* data;

    if (!ent || !ent->client) {
        return;
    }
    data = STAT_Begin();
    J_SetString(data, "NAME", ent->client->pers.netname);
    STAT_SteamId(data, ent);
    J_SetBool(data, "WEAPONS_USED", weaponsUsed);
    J_SetInt(data, "RACE_TIME", raceTime);
    STAT_Send(STAT_Account(ent), "PLAYER_RACECOMPLETE", data);
}

void G_SetMatchGUID(void) {
    char guid[64];

    trap_GenerateMatchGUID(guid, sizeof(guid));
    trap_SetConfigstring(CS_MATCH_GUID, guid);
}

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
        case PW_AMMOREGEN: return "ARMOR_REGEN";
        default: return "OTHER_POWERUP";
    }
}

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

    powerups = J_Array();
    for (i = PW_QUAD; i < PW_NUM_POWERUPS; i++) {
        if (cl->ps.powerups[i]) {
            J_PushString(powerups, STAT_PowerupName(i));
        }
    }
    J_SetNode(obj, "POWERUPS", powerups);

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
        J_SetNull(obj, "BOT_SKILL");
    }
}

static qboolean STAT_Airborne(const gclient_t* cl) {
    return (qboolean)(cl->ps.groundEntityNum == ENTITYNUM_NONE || cl->ps.powerups[PW_FLIGHT] != 0);
}

static void STAT_KillAchievements(gentity_t* attacker, gentity_t* victim, int weapon) {
    gclient_t* cl = attacker->client;
    gclient_t* vc = victim ? victim->client : NULL;
    int clientNum = cl->ps.clientNum;
    qboolean enemy = (qboolean)(victim && !OnSameTeam(attacker, victim) && attacker != victim);
    int i;

    if (!trap_HasAchievement(clientNum, 0x10) && weapon == WP_RAILGUN &&
        cl->expandedStats.numWeaponKills[WP_RAILGUN] > 14) {
        trap_SetAchievement(clientNum, 0x10);
    }
    if (!trap_HasAchievement(clientNum, 0x14) && STAT_Airborne(cl) && enemy) {
        trap_SetAchievement(clientNum, 0x14);
    }
    if (!trap_HasAchievement(clientNum, 0x15) && cl->ps.stats[STAT_HEALTH] == 1 && enemy) {
        trap_SetAchievement(clientNum, 0x15);
    }
    if (!trap_HasAchievement(clientNum, 0x16) && vc && vc->ps.powerups[PW_QUAD] && enemy) {
        for (i = PW_QUAD; i < MAX_POWERUPS; i++) {
            if (cl->ps.powerups[i]) {
                break;
            }
        }
        if (i == MAX_POWERUPS) {
            trap_SetAchievement(clientNum, 0x16);
        }
    }
    if (!trap_HasAchievement(clientNum, 0x19) && weapon == WP_GAUNTLET &&
        cl->expandedStats.numWeaponKills[WP_GAUNTLET] > 4) {
        trap_SetAchievement(clientNum, 0x19);
    }
    if (!trap_HasAchievement(clientNum, 0x1a) && vc &&
        ((weapon == WP_GAUNTLET && STAT_Airborne(cl) && vc->ps.groundEntityNum == ENTITYNUM_NONE) ||
         vc->ps.powerups[PW_FLIGHT])) {
        trap_SetAchievement(clientNum, 0x1a);
    }
    if (!trap_HasAchievement(clientNum, 0x1c) && weapon == WP_GAUNTLET && vc && vc->ps.powerups[PW_QUAD]) {
        trap_SetAchievement(clientNum, 0x1c);
    }
}

static void STAT_DeathAchievements(gentity_t* attacker, gentity_t* victim, int weapon) {
    gclient_t* cl = attacker ? attacker->client : NULL;
    gclient_t* vc = victim->client;

    if (!cl || attacker == victim) {
        return;
    }
    if (vc && !(g_gametype.integer < GT_TEAM && cl->sess.sessionTeam != TEAM_SPECTATOR) &&
        cl->sess.sessionTeam == vc->sess.sessionTeam) {
        return;
    }
    if (!trap_HasAchievement(cl->ps.clientNum, 0x11) && weapon == WP_ROCKET_LAUNCHER && vc && STAT_Airborne(vc)) {
        trap_SetAchievement(cl->ps.clientNum, 0x11);
    }
    if (!trap_HasAchievement(cl->ps.clientNum, 0x13) && weapon == WP_RAILGUN && vc &&
        vc->expandedStats.previousHealth == 1) {
        trap_SetAchievement(cl->ps.clientNum, 0x13);
    }
}

void STAT_PlayerKill(gentity_t* victim, gentity_t* attacker, int mod, int victimStreak) {
    jnode_t* data;
    jnode_t* who;
    qboolean teamkill;
    int alive[4];
    int dead[4];
    int i;

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

    J_SetNull(data, "KILLER");
    if (attacker && attacker->client) {
        who = J_Object();
        STAT_PlayerFrame(who, attacker, BG_GetWeaponFromMeansOfDeath(mod),
                         attacker->client->ps.stats[STAT_HEALTH],
                         attacker->client->ps.stats[STAT_ARMOR]);
        J_SetNode(data, "KILLER", who);
        STAT_KillAchievements(attacker, victim, BG_GetWeaponFromMeansOfDeath(mod));
    }
    J_SetNull(data, "VICTIM");
    if (victim && victim->client) {
        who = J_Object();
        J_SetInt(who, "STREAK", victimStreak);
        STAT_PlayerFrame(who, victim, victim->client->ps.weapon,
                         victim->client->expandedStats.previousHealth,
                         victim->client->expandedStats.previousArmor);
        J_SetNode(data, "VICTIM", who);
        STAT_DeathAchievements(attacker, victim, BG_GetWeaponFromMeansOfDeath(mod));
    }

    J_SetNull(data, "ROUND");
    switch (g_gametype.integer) {
        case GT_CA:
        case GT_FREEZE:
        case GT_AD:
        case GT_RR:
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

    if (attacker && attacker->client) {
        STAT_Send(STAT_Account(attacker), "PLAYER_KILL", data);
    }
    if (victim && victim->client) {
        STAT_Send(STAT_Account(victim), "PLAYER_DEATH", data);
    }
}

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

    trap_Cvar_VariableStringBuffer("sv_hostname", hostname, sizeof(hostname));
    J_SetString(data, "SERVER_TITLE", hostname);

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

#define STAT_NO_SCORE (-999)

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
    J_SetString(data, "STEAM_ID", va("%llu", (unsigned long long)cl->pers.steamId));
    trap_GetConfigstring(CS_MATCH_GUID, guid, sizeof(guid));
    J_SetString(data, "MATCH_GUID", guid);
    J_SetBool(data, "WARMUP", (qboolean)(level.warmupTime != 0));
    J_SetBool(data, "ABORTED", aborted);

    trap_GetUserinfo(cl->ps.clientNum, userinfo, sizeof(userinfo));
    J_SetString(data, "MODEL", Info_ValueForKey(userinfo, "model"));
    J_SetInt(data, "PLAY_TIME", (level.time - cl->pers.enterTime) / 1000);

    if (g_gametype.integer == GT_RACE && cl->ps.persistant[PERS_SCORE] == 0x7fffffff) {
        J_SetInt(data, "SCORE", -1);
    } else {
        J_SetInt(data, "SCORE", cl->ps.persistant[PERS_SCORE]);
    }

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

    {
        int hits = 0, shots = 0;

        for (i = 1; i < WP_NUM_WEAPONS; i++) {
            hits += cl->expandedStats.shotsHit[i];
            shots += cl->expandedStats.shotsFired[i];
        }
        if (!trap_HasAchievement(cl->ps.clientNum, 0x1b) && shots && (double)hits / (double)shots >= 0.6) {
            trap_SetAchievement(cl->ps.clientNum, 0x1b);
        }
    }

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
