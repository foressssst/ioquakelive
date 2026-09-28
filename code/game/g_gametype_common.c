/*
 * g_gametype_common.c -- Shared gametype functions
 *
 * Functions used by multiple gametypes (CA, AD, etc.)
 */
#include "g_local.h"

/*
============
ClientBegin_RoundBased

[QL] CA/AD begin - spawn behaviour depends on round state:
  RS_WARMUP (0): normal spawn + weapon select
  RS_COUNTDOWN (1): spawn but frozen until the round starts
  round active: put the player in spectator until the next round
Address: 0x10035a10. The .so lists AD_ClientBegin and CA_ClientBegin separately; the
x86 build folded them (identical bodies) into this one function, so it keeps a shared
name rather than a single .so symbol.
============
*/
void ClientBegin_RoundBased(gentity_t* ent) {
    gclient_t* client = ent->client;
    int redAlive, blueAlive;

    if (level.roundState.eCurrent == RS_WARMUP) {
        // pre-round: normal spawn
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == RS_COUNTDOWN) {
        // countdown: spawn but freeze until the round begins
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        client->ps.pm_flags |= PMF_FROZEN;
    } else {
        // round active: force to spectator, wait for next round
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        Team_LivingTeamCounts(&redAlive, &blueAlive);
        // auto-follow when freecam is off, not on the spectator team, and both teams are alive
        // [QL] 0x1003c903: the cycle alone, so the player stays on their team
        if (!g_teamSpecFreeCam.integer
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && redAlive > 0 && blueAlive > 0) {
            FollowCycle(ent, 1);
        }
    }
}

/*
============
ClientBegin_RoundBased_impl

[QL] Round-active respawn path for CA/AD (called from ClientRespawn). Same as the
active-round branch of ClientBegin_RoundBased but queues the old body first.
Address: 0x10038260
============
*/
void ClientBegin_RoundBased_impl(gentity_t* ent) {
    gclient_t* client = ent->client;
    int redAlive, blueAlive;

    CopyToBodyQue(ent);
    client->ps.pm_type = PM_SPECTATOR;
    ClientSpawn(ent);
    Team_LivingTeamCounts(&redAlive, &blueAlive);
    // [QL] 0x100381cb (CA) and 0x100359c0 (A&D): the cycle alone, so the dead player stays on
    // their team. Through Cmd_FollowCycle_f they were put on the spectator team, killed and the
    // team forfeited.
    if (!g_teamSpecFreeCam.integer
        && client->sess.sessionTeam != TEAM_SPECTATOR
        && redAlive > 0 && blueAlive > 0) {
        FollowCycle(ent, 1);
    }
}

/*
============
RR_GetRoundState

[QL] Returns the current round state, advancing a scheduled transition if its time has
come. Returns -1 while a transition is pending but not yet due. Shared by RR and the
per-client timer in g_active.c.
Address: 0x100642e0
============
*/
int RR_GetRoundState(void) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return -1;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }
    return level.roundState.eCurrent;
}

/*
============
CheckRoundTimeout

[QL] Returns 1 if the round should end: either team left empty, or roundtimelimit exceeded.
The counts are the connected players of each team (the dead change sides as they fall).
When exactly one blue is left at timeout, records that client as rr_lastBlueClient for the
next round. Called from RR_Think.
Address: 0x10064670
============
*/
int CheckRoundTimeout(int redCount, int blueCount) {
    int i;

    if (redCount != 0 && blueCount != 0) {
        if (roundtimelimit.integer == 0 ||
            level.time - level.roundState.startTime < roundtimelimit.integer * 1000) {
            return 0;
        }
        // timed out with both teams standing: if one blue left, remember them
        if (blueCount == 1) {
            for (i = 0; i < level.numPlayingClients; i++) {
                gclient_t* cl = &level.clients[level.sortedClients[i]];
                if (cl->pers.connected == CON_CONNECTED &&
                    cl->sess.sessionTeam == TEAM_BLUE) {
                    rr_lastBlueClient = cl->ps.clientNum;
                    break;
                }
            }
        }
    }
    return 1;
}

/*
==================
STAT_AddPlayerMedalStat / STAT_AddRoundOverStat

[QL] Stats publishing stubs. The original binary uses C++ jsoncpp
for stats; these are no-ops in the C reimplementation.
Shared by CA, RR, DOM gametypes.
==================
*/
void STAT_AddPlayerMedalStat(gentity_t *ent, const char *medal) {
    // [ioquakelive] 0x100716a0: goes out as a PLAYER_MEDAL event on the stats channel
    if (ent && ent->client && medal) {
        gclient_t *cl = ent->client;

        // [QL] 0x100716f2: in CTF a medal can complete AW_FULL_HOUSE (a capture, an assist
        // and a defense) and AW_TRIFECTA (three of each)
        if (g_gametype.integer == GT_CTF) {
            if (!trap_HasAchievement(cl->ps.clientNum, 0x38) && cl->expandedStats.numCaptures > 0 &&
                cl->expandedStats.numAssists > 0 && cl->expandedStats.numDefends > 0) {
                trap_SetAchievement(cl->ps.clientNum, 0x38);
            }
            if (!trap_HasAchievement(cl->ps.clientNum, 0x39) && cl->expandedStats.numCaptures > 2 &&
                cl->expandedStats.numAssists > 2 && cl->expandedStats.numDefends > 2) {
                trap_SetAchievement(cl->ps.clientNum, 0x39);
            }
        }
        STAT_PlayerMedal(ent, medal, STAT_MedalTotal(ent, medal));
    }
}

void STAT_AddRoundOverStat(int round, int winTeam, int isDraw) {
    // [ioquakelive] 0x100720e0: evento ROUND_OVER
    STAT_RoundOver(round, winTeam, (qboolean)(isDraw != 0));
}

/*
==================
STAT_AddShots / STAT_AddHits

[QL] 0x10072320. What a player fires and hits goes to their Steam stats too (import
+0x324), outside warmup and while the teams are not being shuffled: shots_<weapon> and
hits_<weapon>, the weapon table's +0x08 and +0x0c (0x1008ff00). The gauntlet and the hook
have none there, so theirs land on stat 0, as in retail.
==================
*/
static const int statWeaponShots[WP_NUM_WEAPONS] = {0, 0, 26, 27, 28, 29, 30, 31, 32, 33, 0, 34, 35, 36, 37};
static const int statWeaponHits[WP_NUM_WEAPONS] = {0, 0, 14, 15, 16, 17, 18, 19, 20, 21, 0, 22, 23, 24, 25};

static void STAT_AddSteamStat(gentity_t *ent, int stat, int amount) {
    if (level.warmupTime != 0 || level.teamShuffleActive || !ent->client) {
        return;
    }
    trap_IncrementSteamStat(ent->client->ps.clientNum, stat, amount);
}

void STAT_AddShots(gentity_t *ent, int weapon, int amount) {
    if (weapon > WP_NONE && weapon < WP_NUM_WEAPONS) {
        STAT_AddSteamStat(ent, statWeaponShots[weapon], amount);
    }
}

void STAT_AddHits(gentity_t *ent, int weapon, int amount) {
    if (weapon > WP_NONE && weapon < WP_NUM_WEAPONS) {
        STAT_AddSteamStat(ent, statWeaponHits[weapon], amount);
    }
}

/*
==================
STAT_AddPlayerDeathStat

[QL] 0x10072610 (.so STAT_AddPlayerDeathStat; the "STAT_PublishDeath" Ghidra label
is wrong). self = victim, attacker = killer. The ZMQ PLAYER_KILL/PLAYER_DEATH JSON
emit is stubbed (external); the LOCAL expandedStats accumulation below IS in scope.
Called from player_die (replaces its old inline numKills/numDeaths double-counts).
==================
*/
void STAT_AddPlayerDeathStat(gentity_t *self, gentity_t *attacker, int mod) {
    int weapon;
    int victimStreak;

    // [QL] 0x10072716: retail gates on level.teamShuffleActive (0x105df558), the flag the
    // shuffle and the balance raise while moving people between teams. This looked at
    // level.scoringDisabled, which is a different thing (the Red Rover rounds).
    if (level.teamShuffleActive) {
        return;
    }

    // [QL] 0x100726e0 publishes it with the streak the victim was on, before cutting it
    victimStreak = self->client->expandedStats.killStreak;

    // killer side
    weapon = BG_GetWeaponFromMeansOfDeath(mod);
    if (attacker->client && !OnSameTeam(self, attacker) && attacker != self) {
        attacker->client->expandedStats.numKills++;
        attacker->client->expandedStats.numWeaponKills[weapon]++;
        attacker->client->expandedStats.killStreak++;
    }

    // victim side
    if (mod != MOD_SWITCH_TEAMS) {
        self->client->expandedStats.numDeaths++;
        weapon = BG_GetWeaponFromMeansOfDeath(mod);
        if (weapon != 0) {
            self->client->expandedStats.numWeaponDeaths[weapon]++;
        }
    }

    // the victim's own kill streak ends on death
    if (self->client->expandedStats.maxKillStreak < self->client->expandedStats.killStreak) {
        self->client->expandedStats.maxKillStreak = self->client->expandedStats.killStreak;
    }
    self->client->expandedStats.killStreak = 0;

    // [QL] 0x100726e0: the PLAYER_KILL / PLAYER_DEATH event on the stats channel
    STAT_PlayerKill(self, attacker, mod, victimStreak);
}

/*
==================
STAT_AddDamageStat

[QL] 0x100714b0 (.so STAT_AddDamageStat; the "STAT_PublishDamage" Ghidra label is
wrong). targ = victim, attacker = dealer. The ZMQ emit is stubbed; the LOCAL
per-weapon damage-dealt/taken accumulation below IS in scope. Called from G_Damage.
Attacker (dealt) uses RAW damage; victim (taken) clamps to remaining health+armour.
==================
*/
void STAT_AddDamageStat(gentity_t *targ, gentity_t *attacker, int mod, int damage) {
    int weapon;

    if (attacker == targ) {
        return;
    }

    weapon = BG_GetWeaponFromMeansOfDeath(mod);

    // victim (taken) side - clamped to remaining hp+armour
    if (targ->client && (attacker->client == NULL || !OnSameTeam(targ, attacker))) {
        int clamped = damage;
        int cap = targ->client->ps.stats[STAT_HEALTH] + targ->client->ps.stats[STAT_ARMOR];
        if (cap < clamped) {
            clamped = cap;
        }
        targ->client->expandedStats.totalDamageTaken += clamped;
        if (weapon != 0) {
            targ->client->expandedStats.damageTaken[weapon] += clamped;
        }
        targ->client->expandedStats.previousHealth = targ->client->ps.stats[STAT_HEALTH];
        targ->client->expandedStats.previousArmor = targ->client->ps.stats[STAT_ARMOR];
    }

    // attacker (dealt) side - raw damage
    if (weapon != 0 && attacker && attacker->client && targ->client &&
        (targ->health > 0 || g_instaGib.integer) && !OnSameTeam(targ, attacker)) {
        attacker->client->expandedStats.totalDamageDealt += damage;
        attacker->client->expandedStats.damageDealt[weapon] += damage;
    }
}

/*
==================
LastManStanding

[QL] Binary: 0x1006b210
Plays GTS_LAST_STANDING sound event and sends g_lastManStandingMessage
as a centerprint to the sole survivor on the given team.
Used by CA, AD, Freeze Tag and RR.
==================
*/
void LastManStanding(int team) {
    int i;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;
        if (cl->ps.pm_type != PM_NORMAL) continue;
        if (cl->sess.sessionTeam != team) continue;

        {
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = GTS_LAST_STANDING;  // 0x13 = 19
            // [QL] GTS team travels in modelindex2 (es 0xac), not otherEntityNum.
            te->s.modelindex2 = cl->sess.sessionTeam;
            trap_SendServerCommand(i,
                va("cp \"%s\n\"", g_lastManStandingMessage.string));
        }
        return;
    }
}

/*
==================
G_ObfuscateEnemyInfoInSnapshotCheck

[QL] qagamex86.dll 0x10052e40, the export (+0x44) the engine calls as it copies each client
entity into a client's snapshot (quakelive_steam.exe 0x4e5680). qtrue clears that copy's
health, armor and location. They stay for the viewer's own entity and its teammates (0x10067f00:
outside the team modes only spectators share a team), and whenever either one is a spectator
(0x10067f70); a player in the game loses them for everybody else. The team overlay reads its
teammates' values there.
==================
*/
qboolean G_ObfuscateEnemyInfoInSnapshotCheck(int clientNum1, int clientNum2) {
    if ((unsigned)clientNum1 >= MAX_CLIENTS || (unsigned)clientNum2 >= MAX_CLIENTS) {
        return qfalse;
    }
    if (clientNum1 == clientNum2) {
        return qfalse;
    }
    if (OnSameTeam(&g_entities[clientNum1], &g_entities[clientNum2])) {
        return qfalse;
    }
    if (level.clients[clientNum1].sess.sessionTeam == TEAM_SPECTATOR) {
        return qfalse;
    }
    return level.clients[clientNum2].sess.sessionTeam != TEAM_SPECTATOR;
}

/*
==================
G_ClientAlwaysInSnapshot

[QL] qagamex86.dll 0x10052dc0, the export (+0x30) the engine asks about every client entity
before the PVS test (quakelive_steam.exe 0x4e54c3): qtrue sends it to that viewer from anywhere
on the map. That is the viewer's teammates, the flag carrier in 1-Flag CTF, everybody for a
spectator and, in Red Rover with infection, the other team too.
==================
*/
qboolean G_ClientAlwaysInSnapshot(int viewerNum, int clientNum) {
    if ((unsigned)viewerNum >= MAX_CLIENTS || (unsigned)clientNum >= MAX_CLIENTS) {
        return qfalse;
    }
    if (g_gametype.integer == GT_1FCTF && (g_entities[clientNum].s.powerups & (1 << PW_NEUTRALFLAG))) {
        return qtrue;
    }
    if (g_gametype.integer >= GT_TEAM && OnSameTeam(&g_entities[viewerNum], &g_entities[clientNum])) {
        return qtrue;
    }
    if (level.clients[viewerNum].sess.sessionTeam == TEAM_SPECTATOR) {
        return qtrue;
    }
    if (g_gametype.integer == GT_RR && g_rrInfected.integer &&
        !OnSameTeam(&g_entities[viewerNum], &g_entities[clientNum])) {
        return qtrue;
    }
    return qfalse;
}

/*
==================
G_EntityAlwaysInSnapshot

[QL] qagamex86.dll 0x10052f40, the export (16) the engine asks about every entity past the
clients before the PVS test (quakelive_steam.exe 0x4e54e3): qtrue sends it from anywhere on the
map. The flags in CTF, 1-Flag and A&D, the obelisks in Harvester, the quad in FFA with g_quadHog,
and with g_itemTimers every item that has a timer.
==================
*/
qboolean G_EntityAlwaysInSnapshot(int entityNum) {
    gentity_t* ent;

    if ((unsigned)entityNum > MAX_GENTITIES - 1) {
        return qfalse;
    }
    ent = &g_entities[entityNum];

    switch (g_gametype.integer) {
        case GT_CTF:
        case GT_1FCTF:
        case GT_AD:
            if (ent->item && ent->item->giType == IT_TEAM && ent->item->giTag >= 1 && ent->item->giTag <= 3) {
                return qtrue;
            }
            break;
        case GT_FFA:
            if (g_quadHog.integer) {
                return (qboolean)(ent->item && ent->item->giType == IT_POWERUP && ent->item->giTag == PW_QUAD);
            }
            break;
        case GT_HARVESTER:
            if (ent->classname &&
                (!Q_stricmp(ent->classname, "team_redobelisk") || !Q_stricmp(ent->classname, "team_blueobelisk"))) {
                return qtrue;
            }
            break;
        default:
            break;
    }

    if (g_itemTimers.integer && ent->item && ent->item->itemTimer) {
        return qtrue;
    }
    return qfalse;
}

/*
==================
G_ClientIsAdmin

[QL] qagamex86.dll 0x100530c0, the export (18) the engine asks before an in-game rcon
(quakelive_steam.exe 0x4dffeb): only an admin, privileges 2, may run one.
==================
*/
qboolean G_ClientIsAdmin(int clientNum) {
    gclient_t* client = g_entities[clientNum].client;

    if (!client) {
        return qfalse;
    }
    return client->sess.privileges == 2;
}

/*
==================
G_SnapshotSkipEntity

[QL] qagamex86.dll 0x10053020, the export (15) the engine asks about every entity before it
goes into a viewer's snapshot (quakelive_steam.exe 0x4e548c): qtrue keeps it out. In Freeze Tag
with g_freezeThawTick 1, a thaw tick only goes to the team of the player thawing.
==================
*/
qboolean G_SnapshotSkipEntity(int viewerNum, int entityNum) {
    gentity_t* viewer;
    gentity_t* ent;
    int owner;

    if ((unsigned)viewerNum > MAX_CLIENTS || (unsigned)entityNum > MAX_GENTITIES) {
        return qfalse;
    }
    if (g_gametype.integer != GT_FREEZE || g_freezeThawTick.integer != 1) {
        return qfalse;
    }
    viewer = &g_entities[viewerNum];
    if (!viewer->r.linked) {
        return qfalse;
    }
    ent = &g_entities[entityNum];
    if (ent->s.eType != ET_EVENTS + EV_THAW_TICK) {
        return qfalse;
    }
    owner = ent->r.ownerNum;
    if (owner < 0 || owner >= MAX_GENTITIES) {
        return qfalse;
    }
    return !OnSameTeam(viewer, &g_entities[owner]);
}

/*
==================
SendScoreboardMessageToTeam

Sends the current scoreboard to all connected clients on a given team.
The actual scoreboard content should have been prepared via va() before calling.
Address: 0x10067f80
==================
*/
/*
==================
STAT_GetBestWeapon

[QL] qagamex86.dll 0x100789d0. The weapon the player dealt the most damage with; with no
damage at all, the highest weapon of g_startingWeapons (bit w - 1 for weapon w, the gauntlet
when there is none). The fork took that fallback from the player's own STAT_WEAPONS, whose bits
are the weapon numbers themselves, so the one-bit shift turned a plasma gun into the "BFG" the
FFA board showed for anyone who had not fired yet.
==================
*/
int STAT_GetBestWeapon(gclient_t *cl) {
    int bestWeapon = 0;
    int bestDamage = 0;
    int weapon;
    int fallbackWeapon = 1;

    // highest starting weapon
    for (weapon = 2; weapon < WP_NUM_WEAPONS; weapon++) {
        if (g_startingWeapons.integer & (1 << (weapon - 1))) {
            fallbackWeapon = weapon;
        }
    }

    // Find weapon with most damage dealt
    for (weapon = 1; weapon < WP_NUM_WEAPONS; weapon++) {
        if (cl->expandedStats.damageDealt[weapon] > bestDamage) {
            bestDamage = cl->expandedStats.damageDealt[weapon];
            bestWeapon = weapon;
        }
    }

    if (bestWeapon == 0) {
        bestWeapon = fallbackWeapon;
    }
    return bestWeapon;
}

void SendScoreboardMessageToTeam(int team) {
    int i;
    gclient_t *cl;

    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == team) {
            trap_SendServerCommand(i, NULL);  // sends last va() result
        }
    }
}
