#include "g_local.h"

void ClientBegin_RoundBased(gentity_t* ent) {
    gclient_t* client = ent->client;
    int redAlive, blueAlive;

    if (level.roundState.eCurrent == RS_WARMUP) {
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == RS_COUNTDOWN) {
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        client->ps.pm_flags |= PMF_FROZEN;
    } else {
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        Team_LivingTeamCounts(&redAlive, &blueAlive);
        if (!g_teamSpecFreeCam.integer
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && redAlive > 0 && blueAlive > 0) {
            FollowCycle(ent, 1);
        }
    }
}

void ClientBegin_RoundBased_impl(gentity_t* ent) {
    gclient_t* client = ent->client;
    int redAlive, blueAlive;

    CopyToBodyQue(ent);
    client->ps.pm_type = PM_SPECTATOR;
    ClientSpawn(ent);
    Team_LivingTeamCounts(&redAlive, &blueAlive);
    if (!g_teamSpecFreeCam.integer
        && client->sess.sessionTeam != TEAM_SPECTATOR
        && redAlive > 0 && blueAlive > 0) {
        FollowCycle(ent, 1);
    }
}

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

int CheckRoundTimeout(int redCount, int blueCount) {
    int i;

    if (redCount != 0 && blueCount != 0) {
        if (roundtimelimit.integer == 0 ||
            level.time - level.roundState.startTime < roundtimelimit.integer * 1000) {
            return 0;
        }
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

void STAT_AddPlayerMedalStat(gentity_t *ent, const char *medal) {
    if (ent && ent->client && medal) {
        gclient_t *cl = ent->client;

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
    STAT_RoundOver(round, winTeam, (qboolean)(isDraw != 0));
}

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

void STAT_AddPlayerDeathStat(gentity_t *self, gentity_t *attacker, int mod) {
    int weapon;
    int victimStreak;

    if (level.teamShuffleActive) {
        return;
    }

    victimStreak = self->client->expandedStats.killStreak;

    weapon = BG_GetWeaponFromMeansOfDeath(mod);
    if (attacker->client && !OnSameTeam(self, attacker) && attacker != self) {
        attacker->client->expandedStats.numKills++;
        attacker->client->expandedStats.numWeaponKills[weapon]++;
        attacker->client->expandedStats.killStreak++;
    }

    if (mod != MOD_SWITCH_TEAMS) {
        self->client->expandedStats.numDeaths++;
        weapon = BG_GetWeaponFromMeansOfDeath(mod);
        if (weapon != 0) {
            self->client->expandedStats.numWeaponDeaths[weapon]++;
        }
    }

    if (self->client->expandedStats.maxKillStreak < self->client->expandedStats.killStreak) {
        self->client->expandedStats.maxKillStreak = self->client->expandedStats.killStreak;
    }
    self->client->expandedStats.killStreak = 0;

    STAT_PlayerKill(self, attacker, mod, victimStreak);
}

void STAT_AddDamageStat(gentity_t *targ, gentity_t *attacker, int mod, int damage) {
    int weapon;

    if (attacker == targ) {
        return;
    }

    weapon = BG_GetWeaponFromMeansOfDeath(mod);

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

    if (weapon != 0 && attacker && attacker->client && targ->client &&
        (targ->health > 0 || g_instaGib.integer) && !OnSameTeam(targ, attacker)) {
        attacker->client->expandedStats.totalDamageDealt += damage;
        attacker->client->expandedStats.damageDealt[weapon] += damage;
    }
}

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
            te->s.eventParm = GTS_LAST_STANDING;
            te->s.modelindex2 = cl->sess.sessionTeam;
            trap_SendServerCommand(i,
                va("cp \"%s\n\"", g_lastManStandingMessage.string));
        }
        return;
    }
}

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

qboolean G_ClientIsAdmin(int clientNum) {
    gclient_t* client = g_entities[clientNum].client;

    if (!client) {
        return qfalse;
    }
    return client->sess.privileges == 2;
}

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

int STAT_GetBestWeapon(gclient_t *cl) {
    int bestWeapon = 0;
    int bestDamage = 0;
    int weapon;
    int fallbackWeapon = 1;

    for (weapon = 2; weapon < WP_NUM_WEAPONS; weapon++) {
        if (g_startingWeapons.integer & (1 << (weapon - 1))) {
            fallbackWeapon = weapon;
        }
    }

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
            trap_SendServerCommand(i, NULL);
        }
    }
}
