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
    J_SetBool(data, "WEAPONS_USED", weaponsUsed);  // [QL] 0x10071f84: es un booleano, no una cuenta
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

PLAYER_KILL y PLAYER_DEATH

[QL] 0x100726e0, llamado desde player_die (0x1004751a). Retail arma un solo
documento y lo manda dos veces: como PLAYER_KILL a la cuenta del que mato y
como PLAYER_DEATH a la del que murio. La misma funcion lleva adentro la cuenta
de muertes y de la racha -- en el fork eso ya lo hace STAT_AddPlayerDeathStat --
y desbloquea logros de Steam, que todavia no tenemos.

===============================================================================
*/

// [QL] 0x100711b0: el nombre del MOD
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

// [QL] 0x100713e0: el nombre del arma. El gancho no tiene nombre propio
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

// [QL] 0x10071480: el nombre del powerup
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
        case PW_AMMOREGEN: return "ARMOR_REGEN";  // [QL] el cuarto persistente se llama asi en las estadisticas
        default: return "OTHER_POWERUP";
    }
}

// [QL] el holdable sale del giTag del item que lleva
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

Lo que el evento cuenta de un jugador en el momento de la muerte: con que arma,
como estaba y donde. El que mato va con el arma del MOD y con la vida que tiene
ahora; el que murio, con el arma que llevaba y con la vida y la armadura que
tenia antes del ultimo golpe (expandedStats.previousHealth / previousArmor).
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

    // la velocidad es solo la horizontal, y va como el float que es
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

    // las banderas no cuentan: el recorrido empieza en el quad
    powerups = J_Array();
    for (i = PW_QUAD; i < PW_NUM_POWERUPS; i++) {
        if (cl->ps.powerups[i]) {
            J_PushString(powerups, STAT_PowerupName(i));
        }
    }
    J_SetNode(obj, "POWERUPS", powerups);

    // el holdable solo aparece si lleva uno
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
        J_SetNull(obj, "BOT_SKILL");  // [QL] 0x10072f49: al humano le va el nulo de jsoncpp
    }
}

/*
===============
STAT_PlayerKill

victimStreak es la racha que traia el que murio, tomada antes de que la muerte
la corte, que es el orden que tiene retail adentro de su funcion.
===============
*/
void STAT_PlayerKill(gentity_t* victim, gentity_t* attacker, int mod, int victimStreak) {
    jnode_t* data;
    jnode_t* who;
    qboolean teamkill;
    int alive[4];
    int dead[4];
    int i;

    // [QL] 0x10072716: mientras el juego mueve gente de equipo (shuffle o balance) no sale nada.
    // La global es 0x105df558 = level.teamShuffleActive, no level.scoringDisabled.
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

    // retail deja las dos en nulo y solo las llena si hay quien
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
            // en attack & defend la ronda cuenta los dos turnos
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

        // [QL] 0x10071520: vivo es el que esta jugando (pm_type normal), muerto todo el resto
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

    // el mismo documento, a las dos cuentas
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

[QL] 0x10077a30, llamado cuando se acaba la cuenta regresiva del calentamiento
(0x10058860), antes del map_restart que arranca la partida. Este evento no lleva
el prologo TIME / WARMUP: lleva las reglas con las que se juega y quienes
estaban. Sale sin cuenta (steam id 0).
===============
*/

// [QL] 0x10071110: el nombre corto del tipo de juego
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

    // el mapa sale del serverinfo y va en minusculas
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

    // [QL] 0x1007801b: el titulo del servidor sale de la cvar, no del serverinfo
    trap_Cvar_VariableStringBuffer("sv_hostname", hostname, sizeof(hostname));
    J_SetString(data, "SERVER_TITLE", hostname);

    // todo el que esta en el servidor, aunque todavia se este conectando
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

[QL] 0x10074360. La planilla entera de un jugador. Sale tres veces:

  - al final de la partida, desde LogExit (0x100576eb), con quit = 0
  - cuando alguien se va, desde ClientDisconnect (0x1003c812), con quit = 1
  - cuando alguien deja su equipo, desde SetTeam (0x10040535), con quit = 1

El primer parametro de retail (ecx) es "la partida se abortó"; el tercero, "este
jugador se fue". No sale nada de un jugador que todavia se esta conectando, que
esta en el intermission o que es espectador.

===============================================================================
*/

// [QL] el valor que usa retail para "este equipo o jugador no tiene puntaje"
#define STAT_NO_SCORE (-999)

// [QL] 0x10074360: los powerups van con el nombre del que se levanto, sin las banderas
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

// el promedio de cuanto tardo en volver a cada item; solo va el que levanto alguna vez
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

// [QL] las medallas: tres salen de persistant y diez del arreglo de premios
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

// [QL] 0x10076de1: quien gano. Un equipo con menos puntos todavia gana si el otro
// se quedo sin nadie; fuera de los equipos, gana el primero del ranking o cualquiera
// si hubo forfeit
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
    // [QL] 0x100743fc: aca el bot no lleva "0" -- va su cuenta, que es cero igual
    J_SetString(data, "STEAM_ID", va("%llu", (unsigned long long)cl->pers.steamId));
    trap_GetConfigstring(CS_MATCH_GUID, guid, sizeof(guid));
    J_SetString(data, "MATCH_GUID", guid);
    J_SetBool(data, "WARMUP", (qboolean)(level.warmupTime != 0));
    J_SetBool(data, "ABORTED", aborted);

    trap_GetUserinfo(cl->ps.clientNum, userinfo, sizeof(userinfo));
    J_SetString(data, "MODEL", Info_ValueForKey(userinfo, "model"));
    J_SetInt(data, "PLAY_TIME", (level.time - cl->pers.enterTime) / 1000);

    // en race el que no marco tiempo tiene el puntaje al tope; sale como -1
    if (g_gametype.integer == GT_RACE && cl->ps.persistant[PERS_SCORE] == 0x7fffffff) {
        J_SetInt(data, "SCORE", -1);
    } else {
        J_SetInt(data, "SCORE", cl->ps.persistant[PERS_SCORE]);
    }

    // arma por arma: muertes, muertos, tiros, aciertos, daño, tiempo y cuantas levanto
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

    // lo de equipos solo aparece en los modos de equipo
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
        // el que se fue no tiene puesto ni gano nada
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

[QL] 0x10076fe0, lo ultimo de LogExit (0x10057785). Cierra la partida: con que
reglas se jugo, como quedo y quien anoto. Retail se lo pasa al motor por otra
importacion (+0x2e4, 0x004f4c30), que lo publica como MATCH_REPORT; aca se
publica igual que los demas eventos, con el mismo documento resultante.

Sale una sola vez por partida y nunca en un map_restart.
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

    // [QL] 0x100776fe: si nadie anoto, va "none"
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

[QL] 0x10078960, llamado desde ClientThink_real (0x100353cb) cuando el jugador
esta vivo. Suma el tiempo desde el ultimo paso al total jugado y, si esta en
juego, al del arma que lleva. De ahi salen el PLAY_TIME y la "T" de cada arma
del PLAYER_STATS.
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
