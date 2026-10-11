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

void ScorePlum(gentity_t* ent, vec3_t origin, int score) {
    gentity_t* plum;

    plum = G_TempEntity(origin, EV_SCOREPLUM);
    plum->r.svFlags |= SVF_SINGLECLIENT;
    plum->r.singleClient = ent->s.number;
    plum->s.otherEntityNum = ent->s.number;
    plum->s.time = score;
}

void DamagePlum(gentity_t *attacker, gentity_t *victim, int damage, int mod) {
    gentity_t *te;
    vec3_t org;

    VectorCopy(victim->r.currentOrigin, org);
    org[2] += 32.0f;

    te = G_TempEntity(org, EV_DAMAGEPLUM);
    te->r.svFlags |= (g_damagePlums.integer != 2) ? SVF_SINGLECLIENT : SVF_BROADCAST;
    te->r.singleClient = attacker->client->ps.clientNum;
    te->s.time = damage;
    te->s.clientNum = attacker->client->ps.clientNum;
    te->s.generic1 = BG_GetWeaponFromMeansOfDeath(mod);
}

void AddScore( gentity_t *ent, vec3_t origin, int score ) {
	if ( !ent->client ) {
		return;
	}
	if ( level.warmupTime ) {
		return;
	}
	if ( level.gameStatsReported ) {
		return;
	}
	if ( level.intermissionQueued || level.intermissionTime ) {
		return;
	}
	if ( bot_gauntletOnly.integer && !( ent->r.svFlags & SVF_BOT ) ) {
		return;
	}
	if ( g_gametype.integer == GT_RACE ) {
		return;
	}
	if ( level.scoringDisabled ) {
		return;
	}

	if ( !level.firstScorer[0] ) {
		Q_strncpyz( level.firstScorer, ent->client->pers.netname, sizeof( level.firstScorer ) );
	}
	Q_strncpyz( level.lastScorer, ent->client->pers.netname, sizeof( level.lastScorer ) );

	if ( g_gametype.integer == GT_RR && !g_rrAllowNegativeScores.integer ) {
		if ( ent->client->ps.persistant[PERS_SCORE] + score < 0 ) {
			score = -ent->client->ps.persistant[PERS_SCORE];
		}
	}

	ScorePlum( ent, origin, score );
	ent->client->ps.persistant[PERS_SCORE] += score;

	if ( g_gametype.integer == GT_TEAM ) {
		AddTeamScore( origin, ent->client->ps.persistant[PERS_TEAM], score );
	}

	CalculateRanks();
}

void TossClientItems(gentity_t* self) {
    gclient_t* client = self->client;
    gitem_t* item;
    gentity_t* drop;
    qboolean flag;
    float angle;
    int weapon;
    int i;

    angle = 45;
    for (i = 1; i < PW_NUM_POWERUPS; i++) {
        if (client->ps.powerups[i] <= level.time || i == PW_FLIGHT || i == PW_FREEZE) {
            continue;
        }
        item = BG_FindItemForPowerup((powerup_t)i);
        if (!item) {
            continue;
        }
        flag = item->giType == IT_TEAM && item->giTag >= PW_REDFLAG && item->giTag <= PW_NEUTRALFLAG;
        if (!g_dropPowerups.integer && !flag && item->giTag != PW_INVIS) {
            continue;
        }

        drop = Drop_Item(self, item, angle);
        if (flag) {
            drop->physicsObject = qtrue;
            drop->physicsBounce = g_flagBounce.value;
            drop->activator = self;
        }
        if (client->persistantPowerup) {
            client->ps.stats[STAT_RUNE] = 0;
            client->persistantPowerup = NULL;
        }
        drop->count = (client->ps.powerups[i] - level.time) / 1000;
        if (drop->count < 1) {
            drop->count = 1;
        }
        angle += 45;
    }

    if (g_instaGib.integer || g_gametype.integer == GT_RACE || g_gametype.integer == GT_CA ||
        g_gametype.integer == GT_AD || g_gametype.integer == GT_RR) {
        return;
    }

    weapon = self->s.weapon;

    if (weapon == WP_MACHINEGUN || weapon == WP_GRAPPLING_HOOK) {
        if (client->ps.weaponstate == WEAPON_DROPPING) {
            weapon = client->pers.cmd.weapon;
        }
        if (!(client->ps.stats[STAT_WEAPONS] & (1 << weapon))) {
            weapon = WP_NONE;
        }
    }

    if (weapon > WP_MACHINEGUN && weapon <= WP_HMG && weapon != WP_GRAPPLING_HOOK &&
        client->ps.ammo[weapon] > 0) {
        Drop_Item(self, BG_FindItemForWeapon(weapon), 0);
    }
}

extern gentity_t* neutralObelisk;

void TossClientCubes(gentity_t* self) {
    gitem_t* item;
    gentity_t* drop;
    vec3_t velocity;
    vec3_t angles;
    vec3_t origin;

    self->client->ps.generic1 = 0;

    if (!G_EntitiesFree()) {
        return;
    }

    if (self->client->sess.sessionTeam == TEAM_RED) {
        item = BG_FindItem("Red Skull");
    } else {
        item = BG_FindItem("Blue Skull");
    }

    angles[YAW] = (float)(level.time % 360);
    angles[PITCH] = 0;
    angles[ROLL] = 0;

    AngleVectors(angles, velocity, NULL, NULL);
    VectorScale(velocity, 150, velocity);
    velocity[2] += 200 + crandom() * 50;

    if (neutralObelisk) {
        VectorCopy(neutralObelisk->s.pos.trBase, origin);
        origin[2] += 44;
    } else {
        VectorClear(origin);
    }

    drop = LaunchItem(item, origin, velocity);

    drop->nextthink = level.time + g_cubeTimeout.integer * 1000;
    drop->think = G_FreeEntity;
    drop->spawnflags = self->client->sess.sessionTeam;
}

void LookAtKiller(gentity_t* self, gentity_t* inflictor, gentity_t* attacker) {
    vec3_t dir;

    if (attacker && attacker != self) {
        VectorSubtract(attacker->s.pos.trBase, self->s.pos.trBase, dir);
    } else if (inflictor && inflictor != self) {
        VectorSubtract(inflictor->s.pos.trBase, self->s.pos.trBase, dir);
    } else {
        self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
        return;
    }

    self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw(dir);
}

void GibEntity(gentity_t* self) {
    static vec3_t down = {0, 0, -1};
    gentity_t* ent;
    int i;

    if (self->s.eFlags & EF_KAMIKAZE) {
        for (i = 0; i < MAX_GENTITIES; i++) {
            ent = &g_entities[i];
            if (!ent->inuse)
                continue;
            if (ent->activator != self)
                continue;
            if (strcmp(ent->classname, "kamikaze timer"))
                continue;
            G_FreeEntity(ent);
            break;
        }
    }
    if (self->client) {
        G_AddEvent(self, EV_GIB_PLAYER, DirToByte(self->client->damage_from));
    } else {
        G_AddEvent(self, EV_GIB_PLAYER, DirToByte(down));
    }
    self->takedamage = qfalse;
    self->s.eType = ET_INVISIBLE;
    self->r.contents = 0;

    if (self->client && self->client->ps.powerups[PW_FREEZE]) {
        G_TempEntity(self->r.currentOrigin, EV_THAW_PLAYER);
        self->client->ps.pm_type = PM_NORMAL;
        ClientSpawn(self);
    }
}

void body_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int meansOfDeath) {
    if (self->health > GIB_HEALTH) {
        return;
    }
    GibEntity(self);
}

char* modNames[] = {
    "MOD_UNKNOWN",
    "MOD_SHOTGUN",
    "MOD_GAUNTLET",
    "MOD_MACHINEGUN",
    "MOD_GRENADE",
    "MOD_GRENADE_SPLASH",
    "MOD_ROCKET",
    "MOD_ROCKET_SPLASH",
    "MOD_PLASMA",
    "MOD_PLASMA_SPLASH",
    "MOD_RAILGUN",
    "MOD_LIGHTNING",
    "MOD_BFG",
    "MOD_BFG_SPLASH",
    "MOD_WATER",
    "MOD_SLIME",
    "MOD_LAVA",
    "MOD_CRUSH",
    "MOD_TELEFRAG",
    "MOD_FALLING",
    "MOD_SUICIDE",
    "MOD_TARGET_LASER",
    "MOD_TRIGGER_HURT",
    "MOD_NAIL",
    "MOD_CHAINGUN",
    "MOD_PROXIMITY_MINE",
    "MOD_KAMIKAZE",
    "MOD_JUICED",
    "MOD_GRAPPLE",
    "MOD_SWITCHTEAM",
    "MOD_THAW",
    "MOD_LIGHTNING_DISCHARGE",
    "MOD_HMG",
    "MOD_RAILGUN_HEADSHOT"};

void Kamikaze_DeathActivate(gentity_t* ent) {
    G_StartKamikaze(ent);
    G_FreeEntity(ent);
}

void Kamikaze_DeathTimer(gentity_t* self) {
    gentity_t* ent;

    ent = G_Spawn();
    ent->classname = "kamikaze timer";
    VectorCopy(self->s.pos.trBase, ent->s.pos.trBase);
    ent->r.svFlags |= SVF_NOCLIENT;
    ent->think = Kamikaze_DeathActivate;
    ent->nextthink = level.time + 5 * 1000;

    ent->activator = self;
}

void CheckAlmostCapture(gentity_t* self, gentity_t* attacker) {
    gentity_t* ent;
    vec3_t dir;
    char* classname;

    if (self->client->ps.powerups[PW_REDFLAG] ||
        self->client->ps.powerups[PW_BLUEFLAG] ||
        self->client->ps.powerups[PW_NEUTRALFLAG]) {
        if (g_gametype.integer == GT_CTF || g_gametype.integer == GT_AD) {
            if (self->client->sess.sessionTeam == TEAM_BLUE) {
                classname = "team_CTF_blueflag";
            } else {
                classname = "team_CTF_redflag";
            }
        } else {
            if (self->client->sess.sessionTeam == TEAM_BLUE) {
                classname = "team_CTF_redflag";
            } else {
                classname = "team_CTF_blueflag";
            }
        }
        ent = NULL;
        do {
            ent = G_Find(ent, FOFS(classname), classname);
        } while (ent && (ent->flags & FL_DROPPED_ITEM));
        if (ent && !(ent->s.eFlags & EF_NODRAW)) {
            VectorSubtract(self->client->ps.origin, ent->s.origin, dir);
            if (VectorLength(dir) < 200) {
                self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
                if (attacker->client) {
                    attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
                    if (self->client->sess.sessionTeam != attacker->client->sess.sessionTeam) {
                        attacker->client->expandedStats.numHolyShits++;
                        if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x18) &&
                            attacker->client->expandedStats.numHolyShits > 1) {
                            trap_SetAchievement(attacker->client->ps.clientNum, 0x18);
                        }
                    }
                }
            }
        }
    }
}

void CheckAlmostScored(gentity_t* self, gentity_t* attacker) {
    gentity_t* ent;
    vec3_t dir;
    char* classname;

    if (self->client->ps.generic1) {
        if (self->client->sess.sessionTeam == TEAM_BLUE) {
            classname = "team_redobelisk";
        } else {
            classname = "team_blueobelisk";
        }
        ent = G_Find(NULL, FOFS(classname), classname);
        if (ent) {
            VectorSubtract(self->client->ps.origin, ent->s.origin, dir);
            if (VectorLength(dir) < 200) {
                self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
                if (attacker->client) {
                    attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
                    if (self->client->sess.sessionTeam != attacker->client->sess.sessionTeam) {
                        attacker->client->expandedStats.numHolyShits++;
                    }
                }
            }
        }
    }
}

void PlayerAwardEV(gentity_t* self, int award) {
    gentity_t* te;
    int idx = award + 1;

    self->client->ps.localPersistant[idx]++;
    te = G_TempEntity(self->client->ps.origin, EV_AWARD);
    te->r.svFlags |= SVF_BROADCAST;
    te->s.clientNum = self->client->ps.clientNum;
    te->s.generic1 = award;
    te->s.modelindex2 = self->client->ps.localPersistant[idx];
    self->client->rewardTime = level.time + REWARD_SPRITE_TIME;
}

void PlayerAwardEF(gentity_t* self, int persIdx, int efBit) {
    self->client->ps.persistant[persIdx]++;
    self->client->ps.eFlags &= ~(EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_CAP |
                                 EF_AWARD_IMPRESSIVE | EF_AWARD_DEFEND | EF_AWARD_ASSIST);
    self->client->ps.eFlags |= efBit;
    self->client->rewardTime = level.time + REWARD_SPRITE_TIME;
}

static void G_AnnouncePowerupCarrierKill(gentity_t* attacker, gentity_t* victim) {
    gclient_t* client = victim->client;
    const char* color = "^7";
    const char* powerup;

    if (!attacker->client || !client || attacker == victim || OnSameTeam(attacker, victim)) {
        return;
    }
    if (client->ps.powerups[PW_REDFLAG] || client->ps.powerups[PW_BLUEFLAG] || client->ps.powerups[PW_NEUTRALFLAG]) {
        return;
    }
    if (g_gametype.integer == GT_HARVESTER && (client->ps.generic1 & 0x3f)) {
        return;
    }
    switch (g_gametype.integer) {
        case GT_CTF:
        case GT_1FCTF:
        case GT_OBELISK:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            break;
        default:
            color = "^3";
            break;
    }
    if (client->ps.powerups[PW_QUAD]) {
        powerup = "Quad Damage";
    } else if (client->ps.powerups[PW_BATTLESUIT]) {
        powerup = "Battle Suit";
    } else if (client->ps.powerups[PW_REGEN]) {
        powerup = "Regeneration";
    } else if (client->ps.powerups[PW_HASTE]) {
        powerup = "Haste";
    } else if (client->ps.powerups[PW_INVIS]) {
        powerup = "Invisibility";
    } else {
        return;
    }
    trap_SendServerCommand(-1, va("print \"%s%s killed the %s carrier!^7\n\"", attacker->client->pers.netname, color,
                                  powerup));
}

void player_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int meansOfDeath) {
    gentity_t* ent;
    gentity_t* te;
    int anim;
    int contents;
    int killer;
    int i;
    char *killerName, *obit;
    qboolean isLiveFreeze;

    if (self->client->ps.pm_type == PM_DEAD || self->client->ps.pm_type == PM_FREEZE) {
        return;
    }
    if (level.intermissionQueued || level.intermissionTime) {
        return;
    }

    if ((self->r.svFlags & SVF_BOT) && (bot_training.integer == 1 || bot_training.integer == 2)) {
        trap_EA_Command(self->client->ps.clientNum,
            (char*)G_TrainingSay("say Oops! Um... I meant to do that! Just don't you do it, ok?"));
    }

    CheckAlmostCapture(self, attacker);
    CheckAlmostScored(self, attacker);

    if (self->client->hook) {
        Weapon_HookFree(self->client->hook);
    }

    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        if (cl->hook && cl->hook->enemy == self) {
            Weapon_HookFree(cl->hook);
        }
    }

    if ((self->client->ps.eFlags & EF_TICKING) && self->activator) {
        self->client->ps.eFlags &= ~EF_TICKING;
        self->activator->think = G_FreeEntity;
        self->activator->nextthink = level.time;
    }

    if (!level.intermissionTime && !level.intermissionQueued &&
        g_gametype.integer == GT_FREEZE && meansOfDeath != MOD_SWITCH_TEAMS) {
        self->client->ps.pm_type = PM_FREEZE;
        self->client->ps.thawtime = g_freezeThawTime.integer;
    } else {
        self->client->ps.pm_type = PM_DEAD;
    }

    if (attacker) {
        killer = attacker->s.number;
        if (attacker->client) {
            killerName = attacker->client->pers.netname;
        } else {
            killerName = "<non-client>";
        }
    } else {
        killer = ENTITYNUM_WORLD;
        killerName = "<world>";
    }
    if (killer < 0 || killer >= MAX_CLIENTS) {
        killer = ENTITYNUM_WORLD;
        killerName = "<world>";
    }
    if (meansOfDeath < 0 || meansOfDeath >= ARRAY_LEN(modNames)) {
        obit = "<bad obituary>";
    } else {
        obit = modNames[meansOfDeath];
    }

    if (attacker && attacker->client && meansOfDeath == MOD_SHOTGUN) {
        te = G_TempEntity(self->r.currentOrigin, EV_SHOTGUN_KILL);
        te->s.generic1 = attacker->client->damagePlum[self->s.number];
        te->s.otherEntityNum = attacker->s.number;
        te->s.otherEntityNum2 = self->s.number;
    }

    G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n",
                killer, self->s.number, meansOfDeath, killerName,
                self->client->pers.netname, obit);
    if (attacker) {
        G_AnnouncePowerupCarrierKill(attacker, self);
    }

    if (!level.scoringDisabled) {
        ent = G_TempEntity(self->r.currentOrigin, EV_OBITUARY);
        ent->s.eventParm = meansOfDeath;
        ent->s.otherEntityNum = self->s.number;
        ent->s.otherEntityNum2 = killer;
        ent->r.svFlags = SVF_BROADCAST;
    }

    self->enemy = attacker;
    self->client->ps.persistant[PERS_KILLED]++;
    STAT_AddPlayerDeathStat(self, attacker, meansOfDeath);

    if (attacker && attacker->client) {
        attacker->client->lastkilled_client = self->s.number;
        self->client->lastClientKilled = attacker->s.number;

        if (attacker == self) {
            AddScore(attacker, self->r.currentOrigin, -1);
            if (meansOfDeath == MOD_SUICIDE) {
                self->client->expandedStats.numSuicides++;
            }
        } else if (OnSameTeam(self, attacker)) {
            if (g_complaintLimit.integer && BG_IsScoreBasedGameType() &&
                level.warmupTime == 0 && meansOfDeath != MOD_TELEFRAG &&
                !(attacker->r.svFlags & SVF_BOT)) {
                if (attacker->client->sess.privileges == 2) {
                    trap_SendServerCommand(self->s.number, "complaint -4");
                } else {
                    trap_SendServerCommand(self->s.number, va("complaint %i", attacker->s.number));
                    self->client->pers.complaintClient = attacker->s.clientNum;
                    self->client->pers.complaintEndTime = level.time + 15500;
                    attacker->client->pers.damageToTeammates = 0;
                    self->client->pers.damageFromTeammates = 0;
                }
            }
            AddScore(attacker, self->r.currentOrigin, -1);
            attacker->client->expandedStats.numTeamKills++;
            self->client->expandedStats.numTeamKilled++;
        } else {

            if (g_quadHog.integer == 0) {
                int score = 1;
                if (g_gametype.integer == GT_RR && g_rrInfected.integer &&
                    self->client->sess.sessionTeam == TEAM_RED) {
                    score = g_rrInfectedZombieFragBonus.integer;
                }
                AddScore(attacker, self->r.currentOrigin, score);
                if (g_gametype.integer == GT_RR && g_rrDeathScorePenalty.integer) {
                    AddScore(self, self->r.currentOrigin, g_rrDeathScorePenalty.integer);
                }
            } else if (g_quadHog.integer == 1 && g_gametype.integer == GT_FFA &&
                       (attacker->client->ps.powerups[PW_QUAD] ||
                        self->client->ps.powerups[PW_QUAD])) {
                AddScore(attacker, self->r.currentOrigin, 1);
            }

            if (attacker->client->ps.powerups[PW_BATTLESUIT]) {
                attacker->client->ps.stats[5]++;
            }
            if (attacker->client->ps.powerups[PW_QUAD]) {
                attacker->client->ps.stats[13]++;
                attacker->client->expandedStats.numQuadDamageKills++;
                if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x2F) &&
                    attacker->client->expandedStats.numQuadDamageKills > 9) {
                    trap_SetAchievement(attacker->client->ps.clientNum, 0x2F);
                }
            }

            if (level.firstFrag[0] == '\0' && level.warmupTime == 0) {
                Q_strncpyz(level.firstFrag, attacker->client->pers.netname, sizeof(level.firstFrag));
                attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_FIRSTFRAG;
                PlayerAwardEV(attacker, AWARD_FIRSTFRAG);
                STAT_AddPlayerMedalStat(attacker, "FIRSTFRAG");
            }

            if (meansOfDeath == MOD_GAUNTLET &&
                !(g_gametype.integer == GT_RR && g_rrInfected.integer &&
                  attacker->client->sess.sessionTeam == TEAM_RED)) {
                PlayerAwardEF(attacker, PERS_GAUNTLET_FRAG_COUNT, EF_AWARD_GAUNTLET);
                self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
                STAT_AddPlayerMedalStat(attacker, "GAUNTLET");
                if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x30) &&
                    g_gametype.integer == GT_DUEL) {
                    trap_SetAchievement(attacker->client->ps.clientNum, 0x30);
                }
            }

            if (level.time - attacker->client->lastKillTime < CARNAGE_REWARD_TIME) {
                PlayerAwardEF(attacker, PERS_EXCELLENT_COUNT, EF_AWARD_EXCELLENT);
                STAT_AddPlayerMedalStat(attacker, "EXCELLENT");
                if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x17) &&
                    attacker->client->ps.persistant[PERS_EXCELLENT_COUNT] > 2) {
                    trap_SetAchievement(attacker->client->ps.clientNum, 0x17);
                }
            }

            self->client->revengeCounter[attacker->s.number]++;
            if (attacker->client->revengeCounter[self->s.number] > 2) {
                PlayerAwardEV(attacker, AWARD_REVENGE);
                STAT_AddPlayerMedalStat(attacker, "REVENGE");
            }
            attacker->client->revengeCounter[self->s.number] = 0;

            {
                int wpn = BG_GetWeaponFromMeansOfDeath(self->client->lasthurt_mod[1]);
                if ((meansOfDeath == MOD_RAILGUN || meansOfDeath == MOD_RAILGUN_HEADSHOT) &&
                    wpn != WP_NONE && wpn != WP_RAILGUN &&
                    self->client->lasthurt_client[1] == attacker->s.number &&
                    self->client->lasthurt_time[1] + 1500 > self->client->lasthurt_time[0]) {
                    PlayerAwardEV(attacker, AWARD_COMBOKILL);
                    STAT_AddPlayerMedalStat(attacker, "COMBOKILL");
                }
            }

            if (meansOfDeath == MOD_SHOTGUN) {
                if (self->client->ps.groundEntityNum == ENTITYNUM_NONE && self->waterlevel == 0) {
                    attacker->client->expandedStats.numMidairShotgunKills++;
                    if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x35) &&
                        attacker->client->expandedStats.numMidairShotgunKills > 4) {
                        trap_SetAchievement(attacker->client->ps.clientNum, 0x35);
                    }
                }
            } else {
                if ((meansOfDeath == MOD_GRENADE || meansOfDeath == MOD_ROCKET ||
                     meansOfDeath == MOD_PLASMA || meansOfDeath == MOD_BFG ||
                     meansOfDeath == MOD_NAIL) &&
                    self->client->ps.groundEntityNum == ENTITYNUM_NONE &&
                    self->waterlevel == 0) {
                    vec3_t end;
                    trace_t tr;
                    VectorCopy(self->client->ps.origin, end);
                    end[2] -= g_midAirMinHeight.value;
                    trap_Trace(&tr, self->client->ps.origin, NULL, NULL, end, ENTITYNUM_NONE, MASK_SOLID);
                    if (tr.fraction == 1.0f) {
                        PlayerAwardEV(attacker, AWARD_MIDAIR);
                        STAT_AddPlayerMedalStat(attacker, "MIDAIR");
                    }
                }
                if (meansOfDeath == MOD_RAILGUN_HEADSHOT) {
                    PlayerAwardEV(attacker, AWARD_HEADSHOT);
                    STAT_AddPlayerMedalStat(attacker, "HEADSHOT");
                } else if (meansOfDeath == MOD_TELEFRAG) {
                    PlayerAwardEV(attacker, AWARD_PERFORATED);
                    STAT_AddPlayerMedalStat(attacker, "PERFORATED");
                }
            }

            if (attacker->client->ps.powerups[PW_QUAD]) {
                int qk = attacker->client->ps.stats[13];
                if (qk > 9 && (qk % 10) == 0) {
                    PlayerAwardEV(attacker, AWARD_QUADGOD);
                    STAT_AddPlayerMedalStat(attacker, "QUADGOD");
                }
            }

            if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x12) &&
                meansOfDeath == MOD_KAMIKAZE) {
                trap_SetAchievement(attacker->client->ps.clientNum, 0x12);
            }

            attacker->client->lastKillTime = level.time;
        }
    } else {
        AddScore(self, self->r.currentOrigin, -1);
        if (meansOfDeath == MOD_WATER || meansOfDeath == MOD_SLIME || meansOfDeath == MOD_LAVA ||
            meansOfDeath == MOD_CRUSH || meansOfDeath == MOD_TELEFRAG || meansOfDeath == MOD_FALLING ||
            meansOfDeath == MOD_TARGET_LASER || meansOfDeath == MOD_TRIGGER_HURT) {
            self->client->expandedStats.numSuicides++;
        }
    }

    Team_FragBonuses(self, inflictor, attacker);

    if ((meansOfDeath == MOD_SUICIDE && g_returnFlagOnSuicide.integer) ||
        meansOfDeath == MOD_SWITCH_TEAMS) {
        if (self->client->ps.powerups[PW_NEUTRALFLAG]) {
            Team_ReturnFlag(TEAM_FREE);
            self->client->ps.powerups[PW_NEUTRALFLAG] = 0;
        } else if (self->client->ps.powerups[PW_REDFLAG]) {
            Team_ReturnFlag(TEAM_RED);
            self->client->ps.powerups[PW_REDFLAG] = 0;
        } else if (self->client->ps.powerups[PW_BLUEFLAG]) {
            Team_ReturnFlag(TEAM_BLUE);
            self->client->ps.powerups[PW_BLUEFLAG] = 0;
        }
    }

    if (self->client->ps.stats[STAT_KEY]) {
        if (self->client->ps.stats[STAT_KEY] & KEY_SILVER) G_RespawnKey(KEY_SILVER);
        if (self->client->ps.stats[STAT_KEY] & KEY_GOLD)   G_RespawnKey(KEY_GOLD);
        if (self->client->ps.stats[STAT_KEY] & KEY_MASTER) G_RespawnKey(KEY_MASTER);
        self->client->ps.stats[STAT_KEY] = 0;
    }

    contents = trap_PointContents(self->r.currentOrigin, -1);
    if (contents & CONTENTS_NODROP) {
        if (self->client->ps.powerups[PW_NEUTRALFLAG]) {
            Team_ReturnFlag(TEAM_FREE);
        } else if (self->client->ps.powerups[PW_REDFLAG]) {
            Team_ReturnFlag(TEAM_RED);
        } else if (self->client->ps.powerups[PW_BLUEFLAG]) {
            Team_ReturnFlag(TEAM_BLUE);
        }
    } else {
        TossClientItems(self);
    }

    if (g_gametype.integer == GT_HARVESTER && meansOfDeath != MOD_SWITCH_TEAMS) {
        TossClientCubes(self);
    }

    memset(self->client->ps.powerups, 0, sizeof(self->client->ps.powerups));
    Cmd_Score_f(self);
    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (cl->sess.sessionTeam != TEAM_SPECTATOR) {
            continue;
        }
        if (cl->sess.spectatorClient == self->s.number && g_entities[i].client) {
            Cmd_Score_f(&g_entities[i]);
        }
    }

    self->takedamage = qtrue;
    self->s.weapon = WP_NONE;
    self->s.powerups = 0;
    self->s.loopSound = 0;

    isLiveFreeze = (!level.intermissionTime && !level.intermissionQueued &&
                    g_gametype.integer == GT_FREEZE && meansOfDeath != MOD_SWITCH_TEAMS);

    if (!isLiveFreeze) {
        self->r.contents = CONTENTS_CORPSE;
    }

    self->s.angles[0] = 0;
    self->s.angles[2] = 0;
    if (attacker && attacker != self) {
        vec3_t dir;
        dir[0] = attacker->s.pos.trBase[0] - self->s.pos.trBase[0];
        dir[1] = attacker->s.pos.trBase[1] - self->s.pos.trBase[1];
        dir[2] = attacker->s.pos.trBase[2] - self->s.pos.trBase[2];
        self->s.angles[1] = vectoyaw(dir);
    }
    self->client->ps.viewangles[0] = 0;
    self->client->ps.viewangles[1] = self->s.angles[1];
    self->client->ps.viewangles[2] = self->s.angles[2];

    if (!isLiveFreeze) {
        self->r.maxs[2] = -8;
    }

    {
        qboolean roundGT = (g_gametype.integer == GT_CA || g_gametype.integer == GT_FREEZE ||
                            g_gametype.integer == GT_AD || g_gametype.integer == GT_RR);
        qboolean handled = qfalse;
        if (roundGT && (level.warmupTime > 0 || level.roundState.eCurrent == RS_COUNTDOWN)) {
            self->client->respawnTime = level.time + 500;
            handled = qtrue;
        }
        if (!handled) {
            if (level.intermissionTime || level.intermissionQueued ||
                g_gametype.integer != GT_FREEZE || meansOfDeath == MOD_SWITCH_TEAMS) {
                int delay = g_suddenDeathRespawn.integer ? level.forceRespawnDelay
                                                         : g_respawn_delay_min.integer;
                self->client->respawnTime = level.time + delay;
            }
        }
    }

    {
        qboolean doDeathBranch = qtrue;
        if (meansOfDeath == MOD_SUICIDE) {
            if (!(!level.intermissionTime && !level.intermissionQueued &&
                  g_gametype.integer == GT_FREEZE)) {
                GibEntity(self);
                doDeathBranch = qfalse;
            }
        }

        if (doDeathBranch) {
            if (self->health <= GIB_HEALTH &&
                (level.intermissionTime || level.intermissionQueued ||
                 g_gametype.integer != GT_FREEZE)) {
                GibEntity(self);
                if (attacker && attacker->client) {
                    if (attacker != self && !OnSameTeam(attacker, self)) {
                        int r = attacker->client->rampageCounter;
                        if (r == 0 ||
                            (r == 1 && level.time <= attacker->client->lastGibTime + 3000)) {
                            attacker->client->rampageCounter++;
                        } else if (r > 1 && level.time < attacker->client->lastGibTime + 3000) {
                            PlayerAwardEV(attacker, AWARD_RAMPAGE);
                            STAT_AddPlayerMedalStat(attacker, "RAMPAGE");
                            attacker->client->rampageCounter = 0;
                        }
                    }
                    attacker->client->lastGibTime = level.time;
                }
            } else {
                static int deathAnim;

                switch (deathAnim) {
                    case 0:  anim = BOTH_DEATH1; break;
                    case 1:  anim = BOTH_DEATH2; break;
                    case 2:
                    default: anim = BOTH_DEATH3; break;
                }
                G_AddEvent(self, meansOfDeath == MOD_WATER ? EV_DROWN : EV_DEATH1 + deathAnim, 0);
                deathAnim = (deathAnim + 1) % 3;

                if (self->health <= GIB_HEALTH) {
                    self->health = GIB_HEALTH + 1;
                }

                if (!level.intermissionTime && !level.intermissionQueued &&
                    g_gametype.integer == GT_FREEZE && meansOfDeath != MOD_SWITCH_TEAMS) {
                    self->client->ps.powerups[PW_FREEZE] = 0x7fffffff;
                    self->s.powerups = 0x8000;
                    self->client->ps.freezetime = level.time;
                    self->die = player_die;
                    if (meansOfDeath > MOD_LAVA &&
                        (meansOfDeath < MOD_FALLING || meansOfDeath == MOD_LIGHTNING_DISCHARGE)) {
                        Freeze_InstaKill(self, 1);
                    }
                } else {
                    self->client->ps.legsAnim =
                        ((self->client->ps.legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
                    self->client->ps.torsoAnim =
                        ((self->client->ps.torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
                    self->die = body_die;
                }

                if (self->s.eFlags & EF_KAMIKAZE) {
                    gentity_t* e = G_Spawn();
                    e->classname = "kamikaze timer";
                    VectorCopy(self->s.pos.trBase, e->s.pos.trBase);
                    e->r.svFlags |= SVF_NOCLIENT;
                    e->think = Kamikaze_DeathActivate;
                    e->nextthink = level.time + 3000;
                    e->activator = self;
                }
            }
        }
    }

    if (g_gametype.integer == GT_CA && attacker && attacker->client) {
        int aliveRed, aliveBlue;
        Team_LivingTeamCounts(&aliveRed, &aliveBlue);
        if (aliveRed == 0 || aliveBlue == 0) {
            if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x32) &&
                meansOfDeath == MOD_GAUNTLET) {
                trap_SetAchievement(attacker->client->ps.clientNum, 0x32);
            }
            if (!trap_HasAchievement(attacker->client->ps.clientNum, 0x33) &&
                (meansOfDeath == MOD_GRENADE || meansOfDeath == MOD_GRENADE_SPLASH)) {
                trap_SetAchievement(attacker->client->ps.clientNum, 0x33);
            }
        }
    }

    if (g_gametype.integer == GT_CA) {
        CA_PlayerKilled(self->client->sess.sessionTeam);
    } else if (g_gametype.integer == GT_AD) {
        AD_PlayerKilled(self->client->sess.sessionTeam);
    } else if (g_gametype.integer == GT_RR) {
        RR_OnPlayerDeath(self);
    }

    trap_LinkEntity(self);
}

int CheckArmor(gentity_t* ent, int damage, int dflags) {
    gclient_t* client;
    int save;
    int count;
    double armorProtection;

    if (!damage)
        return 0;

    client = ent->client;

    if (!client)
        return 0;

    if (dflags & DAMAGE_NO_ARMOR)
        return 0;

    if (client->ps.stats[STAT_PERSISTANT_POWERUP] == 4) {
        armorProtection = 0.75;
    } else if (!armor_tiered.integer || g_gametype.integer == GT_CA) {
        armorProtection = 0.66;
    } else {
        switch (client->ps.stats[STAT_ARMORTYPE]) {
            case 0:  armorProtection = 0.50; break;
            case 1:  armorProtection = 0.66; break;
            case 2:  armorProtection = 0.75; break;
            default: armorProtection = 0.66; break;
        }
    }

    save = (int)ceil((double)damage * armorProtection);
    count = client->ps.stats[STAT_ARMOR];
    if (save >= count)
        save = count;

    return save;
}

int RaySphereIntersections(vec3_t origin, float radius, vec3_t point, vec3_t dir, vec3_t intersections[2]) {
    float b, c, d, t;

    VectorNormalize(dir);
    b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
    c = (point[0] - origin[0]) * (point[0] - origin[0]) +
        (point[1] - origin[1]) * (point[1] - origin[1]) +
        (point[2] - origin[2]) * (point[2] - origin[2]) -
        radius * radius;

    d = b * b - 4 * c;
    if (d > 0) {
        t = (-b + sqrt(d)) / 2;
        VectorMA(point, t, dir, intersections[0]);
        t = (-b - sqrt(d)) / 2;
        VectorMA(point, t, dir, intersections[1]);
        return 2;
    } else if (d == 0) {
        t = (-b) / 2;
        VectorMA(point, t, dir, intersections[0]);
        return 1;
    }
    return 0;
}

int G_InvulnerabilityEffect(gentity_t* targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir) {
    gentity_t* impact;
    vec3_t intersections[2], vec;
    int n;

    if (!targ->client) {
        return qfalse;
    }
    VectorCopy(dir, vec);
    VectorInverse(vec);
    n = RaySphereIntersections(targ->client->ps.origin, 42, point, vec, intersections);
    if (n > 0) {
        impact = G_TempEntity(targ->client->ps.origin, EV_INVUL_IMPACT);
        VectorSubtract(intersections[0], targ->client->ps.origin, vec);
        vectoangles(vec, impact->s.angles);
        impact->s.angles[0] += 90;
        if (impact->s.angles[0] > 360)
            impact->s.angles[0] -= 360;
        if (impactpoint) {
            VectorCopy(intersections[0], impactpoint);
        }
        if (bouncedir) {
            VectorCopy(vec, bouncedir);
            VectorNormalize(bouncedir);
        }
        return qtrue;
    } else {
        return qfalse;
    }
}

static int DamageTier(int damage) {
    if (damage > 74) return 3;
    if (damage > 49) return 2;
    return (damage > 24);
}

static float G_DistanceToBBox(gentity_t* ent, const vec3_t point) {
    vec3_t v;
    int i;

    for (i = 0; i < 3; i++) {
        if (point[i] < ent->r.absmin[i]) {
            v[i] = ent->r.absmin[i] - point[i];
        } else if (point[i] > ent->r.absmax[i]) {
            v[i] = point[i] - ent->r.absmax[i];
        } else {
            v[i] = 0;
        }
    }
    return VectorLength(v);
}

static float G_KnockbackScale(int mod, qboolean isSelf) {
    if (isSelf) {
        switch (mod) {
            case MOD_ROCKET:
            case MOD_ROCKET_SPLASH:
                return g_knockback_rl_self.value;
            case MOD_PLASMA:
            case MOD_PLASMA_SPLASH:
                return g_knockback_pg_self.value;
            default:
                break;
        }
    }
    switch (mod) {
        case MOD_GAUNTLET:       return g_knockback_g.value;
        case MOD_MACHINEGUN:     return g_knockback_mg.value;
        case MOD_SHOTGUN:        return g_knockback_sg.value;
        case MOD_GRENADE:
        case MOD_GRENADE_SPLASH: return g_knockback_gl.value;
        case MOD_ROCKET:
        case MOD_ROCKET_SPLASH:  return g_knockback_rl.value;
        case MOD_LIGHTNING:
        case MOD_LIGHTNING_DISCHARGE: return g_knockback_lg.value;
        case MOD_RAILGUN:
        case MOD_RAILGUN_HEADSHOT:    return g_knockback_rg.value;
        case MOD_PLASMA:
        case MOD_PLASMA_SPLASH:  return g_knockback_pg.value;
        case MOD_BFG:
        case MOD_BFG_SPLASH:     return g_knockback_bfg.value;
        case MOD_GRAPPLE:        return g_knockback_gh.value;
        case MOD_NAIL:           return g_knockback_ng.value;
        case MOD_PROXIMITY_MINE: return g_knockback_pl.value;
        case MOD_CHAINGUN:       return g_knockback_cg.value;
        case MOD_HMG:            return g_knockback_hmg.value;
        default:                 return 1.0f;
    }
}

static void G_FlagPhysicsThink(gentity_t* ent) {
    int touch[MAX_GENTITIES];
    vec3_t mins, maxs, forward;
    gentity_t* hit;
    gentity_t* dest;
    gentity_t* te;
    int num, i;

    VectorSet(mins, ent->r.currentOrigin[0] - 15, ent->r.currentOrigin[1] - 15, ent->r.currentOrigin[2] - 15);
    VectorSet(maxs, ent->r.currentOrigin[0] + 15, ent->r.currentOrigin[1] + 15, ent->r.currentOrigin[2] + 15);
    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (!hit->classname) {
            continue;
        }
        if (!strcmp(hit->classname, "trigger_push")) {
            if (ent->fly_sound_debounce_time < level.time) {
                ent->fly_sound_debounce_time = level.time + 1500;
                te = G_TempEntity(ent->r.currentOrigin, EV_GENERAL_SOUND);
                te->s.eventParm = G_SoundIndex("sound/world/jumppad.wav");
            }
            VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
            VectorCopy(hit->s.origin2, ent->s.pos.trDelta);
            ent->s.pos.trTime = level.time - level.frametime;
        } else if (!strcmp(hit->classname, "trigger_teleport")) {
            dest = G_PickTarget(hit->target);
            if (!dest) {
                G_Printf("Couldn't find teleporter destination\n");
                return;
            }
            te = G_TempEntity(ent->r.currentOrigin, EV_GENERAL_SOUND);
            te->s.eventParm = G_SoundIndex("sound/world/teleout.ogg");

            VectorCopy(dest->s.origin, ent->s.pos.trBase);
            VectorCopy(dest->s.origin, ent->r.currentOrigin);
            AngleVectors(dest->s.angles, forward, NULL, NULL);
            VectorScale(forward, 400, ent->s.pos.trDelta);
            ent->s.pos.trDelta[2] += 96;
            ent->s.pos.trTime = level.time - level.frametime;
            SnapVector(ent->s.pos.trDelta);

            te = G_TempEntity(ent->r.currentOrigin, EV_GENERAL_SOUND);
            te->s.eventParm = G_SoundIndex("sound/world/telein.ogg");
        } else if (!strcmp(hit->classname, "trigger_capturezone")) {
            CaptureZone_Touch(hit, ent);
        }
    }
}

void G_ThrowFlag(gentity_t* self, vec3_t velocity, vec3_t point) {
    static vec3_t mins = { -15, -15, -15 };
    static vec3_t maxs = { 15, 15, 35 };
    gclient_t* client = self->client;
    vec3_t angles, forward, end;
    gentity_t* drop;
    trace_t tr;
    int pw;

    if (!client || client->ps.pm_type == PM_SPECTATOR || (client->ps.pm_flags & PMF_PAUSED)) {
        return;
    }

    if (!velocity) {
        VectorSet(angles, 0, client->ps.viewangles[YAW], 0);
        velocity = vec3_origin;
    } else {
        VectorCopy(client->ps.viewangles, angles);
    }
    AngleVectors(angles, forward, NULL, NULL);

    if (point) {
        VectorCopy(point, end);
    } else {
        VectorMA(client->ps.origin, 64, forward, end);
    }

    trap_Trace(&tr, client->ps.origin, mins, maxs, end, ENTITYNUM_NONE, CONTENTS_SOLID);
    if (tr.fraction != 1.0f) {
        return;
    }

    if (client->ps.powerups[PW_NEUTRALFLAG]) {
        pw = PW_NEUTRALFLAG;
    } else if (client->ps.powerups[PW_REDFLAG]) {
        pw = PW_REDFLAG;
    } else if (client->ps.powerups[PW_BLUEFLAG]) {
        pw = PW_BLUEFLAG;
    } else {
        return;
    }

    drop = LaunchItem(BG_FindItemForPowerup((powerup_t)pw), end, velocity);
    drop->physicsObject = qtrue;
    drop->physicsBounce = g_flagBounce.value;
    drop->parent = self;
    if (g_flagPhysics.integer) {
        drop->framethink = G_FlagPhysicsThink;
    }
    drop->nextthink = level.time + 10000;
    drop->s.clientNum = self->s.clientNum;
    drop->s.time = level.time;
    client->ps.powerups[pw] = 0;
}

static void Drop_DamagedHealth_Ring(gentity_t* targ, gitem_t* item, int count) {
    float angle;
    float step;
    int i;

    if (!item || count <= 0) {
        return;
    }
    angle = (float)(rand() % 360);
    step = (float)(360 / count);
    for (i = 0; i < count; i++) {
        vec3_t velocity;
        vec3_t origin;
        float a = DEG2RAD(targ->s.pos.trBase[1] + angle);
        velocity[0] = cos(a) * 200.0f;
        velocity[1] = sin(a) * 200.0f;
        velocity[2] = 225.0f;
        origin[0] = targ->s.pos.trBase[0] + velocity[0] * 0.2f;
        origin[1] = targ->s.pos.trBase[1] + velocity[1] * 0.2f;
        origin[2] = targ->s.pos.trBase[2] + velocity[2] * 0.2f;
        LaunchItem(item, origin, velocity);
        angle += step;
    }
}

void Drop_DamagedHealth(gentity_t* attacker, gentity_t* targ, int amount) {
    if (!g_dropDamagedHealth.integer) {
        return;
    }
    if (targ == attacker && (g_dmflags.integer & DF_NO_SELF_DAMAGE)) {
        return;
    }
    Drop_DamagedHealth_Ring(targ, BG_FindItem("25 Health"), amount / 25);
    Drop_DamagedHealth_Ring(targ, BG_FindItem("5 Health"), (amount % 25) / 5);
}

void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod) {
    gclient_t* client;
    int take;
    int asave;
    int knockback;
    vec3_t bouncedir, impactpoint;

    client = targ->client;

    if (!targ->takedamage) {
        return;
    }
    if (client && client->noclip) {
        return;
    }
    if (level.intermissionQueued) {
        return;
    }
    if (level.intermissionTime) {
        return;
    }

    if (client) {
        if (mod != MOD_JUICED && client->invulnerabilityTime > level.time) {
            if (dir && point) {
                G_InvulnerabilityEffect(targ, dir, point, impactpoint, bouncedir);
            }
            return;
        }
        if (client->ps.pm_type == PM_SPECTATOR) {
            return;
        }
    }

    if (!inflictor) {
        inflictor = &g_entities[ENTITYNUM_WORLD];
    }
    if (!attacker) {
        attacker = &g_entities[ENTITYNUM_WORLD];
    }

    if (pmove_NoPlayerClip.integer && attacker->client && client && targ != attacker) {
        return;
    }

    if (targ->s.eType == ET_MOVER) {
        if (targ->use && targ->moverState == MOVER_POS1) {
            targ->use(targ, inflictor, attacker);
        }
        return;
    }

    if (g_gametype.integer == GT_OBELISK && CheckObeliskAttack(targ, attacker)) {
        return;
    }

    if (!dir) {
        dflags |= DAMAGE_NO_KNOCKBACK;
    } else {
        VectorNormalize(dir);
    }

    knockback = (int)((float)damage * G_KnockbackScale(mod, (targ == attacker)));
    if (client && targ == attacker && g_gametype.integer == GT_RACE && knockback > 0) {
        client->race.weaponUsed = 1;
    }
    if (mod == MOD_GRAPPLE && targ != attacker && inflictor->wait != 0.0f) {
        knockback = (int)(knockback * 0.2);
    }

    if (knockback > g_max_knockback.integer) {
        knockback = g_max_knockback.integer;
    }
    if (targ->flags & FL_NO_KNOCKBACK) {
        knockback = 0;
    }
    if (dflags & DAMAGE_NO_KNOCKBACK) {
        knockback = 0;
    }

    if (mod == MOD_ROCKET_SPLASH) {
        damage = (int)((1.0 - (double)G_DistanceToBBox(targ, inflictor->r.currentOrigin) / inflictor->splashRadius) *
                       g_splashdamage_rl.integer) *
                 inflictor->damageFactor;
    }

    if (attacker->client && attacker != targ) {
        char userinfo[MAX_INFO_STRING];
        int h;
        trap_GetUserinfo(attacker->client->ps.clientNum, userinfo, sizeof(userinfo));
        h = atoi(Info_ValueForKey(userinfo, "handicap"));
        if (h < 1 || h > 100) {
            h = 100;
        }
        damage = (damage * h) / 100;
    }

    if (knockback && client) {
        vec3_t kvel;
        vec3_t kdir;
        int k = knockback;

        if (k < 0) {
            k = -k;
            VectorNegate(dir, kdir);
        } else {
            VectorCopy(dir, kdir);
        }
        VectorScale(kdir, g_knockback.value * (float)k / 200.0f, kvel);
        VectorAdd(client->ps.velocity, kvel, client->ps.velocity);

        if (client->ps.pm_time == 0) {
            float cr = g_knockback_cripple.value;
            float t = (float)(k * 2);
            float r = cr;
            if (cr <= t) {
                r = t;
                if (t > 200.0f) {
                    r = 200.0f;
                }
            }
            client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
            client->ps.pm_time = (int)r;
        }
    }

    if (g_gametype.integer == GT_RR && g_rrInfected.integer && attacker->client &&
        attacker->client->sess.sessionTeam == TEAM_RED && mod == MOD_GAUNTLET) {
        damage = 666;
    }

    if (!(dflags & DAMAGE_NO_PROTECTION)) {
        if (mod != MOD_JUICED && targ != attacker && !(dflags & DAMAGE_NO_TEAM_PROTECTION)) {
            if (OnSameTeam(targ, attacker) && !g_friendlyFire.integer) {
                return;
            }
        }
        if (targ->client && attacker->client && g_quadHog.integer &&
            g_gametype.integer == GT_FFA && attacker != targ) {
            if (!attacker->client->ps.powerups[PW_QUAD] && !targ->client->ps.powerups[PW_QUAD]) {
                return;
            }
        }
        if (mod == MOD_PROXIMITY_MINE) {
            if (inflictor && inflictor->parent && OnSameTeam(targ, inflictor->parent)) {
                return;
            }
            if (targ == attacker) {
                return;
            }
        }
        if (bot_gauntletOnly.integer && (attacker->r.svFlags & SVF_BOT) && (targ->r.svFlags & SVF_BOT)) {
            return;
        }
        if (targ->flags & FL_GODMODE) {
            static int lastProtestTime;

            if ((targ->r.svFlags & SVF_BOT) && (targ->flags & FL_TRAINER) && client && attacker->client &&
                client->trainerSayTime < level.time && lastProtestTime + 7000 < level.time &&
                random() * 100 < 25) {
                trap_EA_Command(client->ps.clientNum,
                    (char*)G_TrainingSay("say Stop wasting your ammo! I'm here to help you out!"));
                trap_SendServerCommand(attacker->client->ps.clientNum, "playSound sound/vo/crash/40b.wav");
                lastProtestTime = level.time;
            }
            return;
        }
    }

    if (client) {
        if (client->ps.powerups[PW_BATTLESUIT]) {
            G_AddEvent(targ, EV_POWERUP_BATTLESUIT, 0);
            if (mod == MOD_FALLING) {
                return;
            }
            damage = (int)((float)damage * g_battleSuitDampen.value);
        }
        if (client->ps.powerups[0] != 0) {
            damage = (int)((float)damage * g_spawnArmorDmgScale.value);
        }
    }

    if (attacker->client && targ != attacker && targ->health > 0 &&
        targ->s.eType != ET_MISSILE && targ->s.eType != ET_GENERAL) {
        if (OnSameTeam(targ, attacker)) {
            attacker->client->ps.persistant[PERS_HITS]--;
            damage = (int)((float)damage * g_friendlyFireDampen.value);
        } else {
            attacker->client->ps.persistant[PERS_HITS]++;
            attacker->client->ps.generic1 =
                (attacker->client->ps.generic1 & 0x3F) | (DamageTier(damage) << 6);
        }
    }

    if (targ == attacker) {
        if (g_instaGib.integer) {
            return;
        }
        damage = (int)((float)damage * 0.5f);
    }
    if (damage < 1) {
        damage = 1;
    }

    if (client) {
        if (client->ps.stats[STAT_PERSISTANT_POWERUP] == 2) {
            damage = (int)((float)damage * 0.5f);
        } else if (client->ps.stats[STAT_PERSISTANT_POWERUP] == 1 &&
                   targ == attacker && mod != MOD_KAMIKAZE) {
            damage = 0;
        }
    }

    asave = CheckArmor(targ, damage, dflags);
    take = damage - asave;

    if (!(dflags & DAMAGE_NO_PROTECTION)) {
        if (g_gametype.integer == GT_CA) {
            if (!CA_AdjustDamage(targ, attacker, &take, &asave)) {
                return;
            }
        } else if (g_gametype.integer == GT_AD) {
            if (!AD_AdjustDamage(targ, attacker, &take, &asave)) {
                return;
            }
        } else if (g_gametype.integer == GT_RR) {
            if (!RR_AdjustDamage(targ, attacker, &take, &asave)) {
                return;
            }
        }
    }

    if (g_gametype.integer != GT_CA && g_gametype.integer != GT_AD &&
        g_dmflags.integer != 0 && targ == attacker) {
        if ((g_dmflags.integer & DF_NO_SELF_DAMAGE) && mod != MOD_KAMIKAZE) {
            take = 0;
        }
        if (g_dmflags.integer & DF_NO_SELF_ARMOR_DAMAGE) {
            asave = 0;
        }
    }

    if (client && client->ps.powerups[PW_FREEZE]) {
        take = 0;
    }
    if ((client && client->ps.powerups[PW_FREEZE]) || (targ->s.powerups & 0x8000)) {
        if (client) {
            if (mod == MOD_CRUSH || mod == MOD_TELEFRAG || mod == MOD_LIGHTNING_DISCHARGE) {
                Freeze_InstaKill(targ, 1);
            } else {
                switch (mod) {
                    case MOD_SLIME:
                    case MOD_LAVA:
                    case MOD_SUICIDE:
                    case MOD_TRIGGER_HURT:
                        if (level.time > client->ps.freezetime + g_freezeEnvironmentalRespawnDelay.integer) {
                            Freeze_InstaKill(targ, 1);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    if (g_debugDamage.integer) {
        G_Printf("%i: client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number,
                 targ->health, take, asave);
    }

    if (client) {
        client->damage_armor += asave;
        client->damage_blood += take;
        client->ps.persistant[PERS_ATTACKER] = attacker->s.number;
        if (dir) {
            VectorCopy(dir, client->damage_from);
            client->damage_fromWorld = qfalse;
        } else {
            VectorClear(client->damage_from);
            client->damage_fromWorld = qtrue;
        }
        client->lasthurt_client[1] = client->lasthurt_client[0];
        client->lasthurt_client[0] = attacker->s.number;
        client->lasthurt_mod[1] = client->lasthurt_mod[0];
        client->lasthurt_mod[0] = mod;
        client->lasthurt_time[1] = client->lasthurt_time[0];
        client->lasthurt_time[0] = level.time;
        if (g_regenHealth.integer) {
            client->healthRegenActive = qtrue;
        }
        if (g_regenArmor.integer) {
            client->armorRegenActive = qtrue;
        }
    }

    if (g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF ||
        g_gametype.integer == GT_AD) {
        Team_CheckHurtCarrier(targ, attacker);
    }

    if (g_instaGib.integer && client) {
        if (client->ps.powerups[0] != 0) {
            if (attacker->client && targ != attacker) {
                attacker->client->ps.persistant[PERS_HITS]--;
            }
            return;
        }
        switch (mod) {
            case MOD_SHOTGUN:
            case MOD_GAUNTLET:
            case MOD_MACHINEGUN:
            case MOD_GRENADE:
            case MOD_ROCKET:
            case MOD_PLASMA:
            case MOD_RAILGUN:
            case MOD_LIGHTNING:
            case MOD_BFG:
            case MOD_NAIL:
            case MOD_CHAINGUN:
            case MOD_PROXIMITY_MINE:
            case MOD_GRAPPLE:
            case MOD_HMG:
            case MOD_RAILGUN_HEADSHOT:
                targ->health = -39;
                if (attacker->client && targ != attacker) {
                    attacker->client->ps.persistant[PERS_ATTACKEE_ARMOR]++;
                }
                break;
            case MOD_GRENADE_SPLASH:
            case MOD_ROCKET_SPLASH:
            case MOD_PLASMA_SPLASH:
            case MOD_BFG_SPLASH:
                if (attacker->client && targ != attacker) {
                    attacker->client->ps.persistant[PERS_HITS]--;
                }
                return;
            default:
                break;
        }
    }

    if (take == 0 && asave == 0) {
        return;
    }

    if (g_tackleFlag.integer && mod == MOD_GAUNTLET && targ->client) {
        vec3_t flagPoint;
        vec3_t flagVelocity;
        flagPoint[0] = targ->client->ps.origin[0];
        flagPoint[1] = targ->client->ps.origin[1];
        flagPoint[2] = targ->client->ps.origin[2] + 64.0f;
        VectorSet(flagVelocity, 0, 0, (float)g_tackleFlag.integer);
        G_ThrowFlag(targ, flagVelocity, flagPoint);
    }

    if (g_damagePlums.integer && targ->client && attacker->client &&
        attacker != targ && targ->health > 0) {
        if (mod == MOD_SHOTGUN) {
            attacker->client->damagePlum[targ->client->ps.clientNum] += damage;
        } else {
            DamagePlum(attacker, targ, damage, mod);
        }
    }

    STAT_AddDamageStat(targ, attacker, mod, damage);
    if (targ->client && BG_IsRoundBasedGameType(g_gametype.integer)) {
        targ->client->round_damage += damage;
    }

    if (targ->health > 0 && (targ->health - take) <= 0 &&
        targ->client && attacker->client && targ != attacker) {
        attacker->client->ps.persistant[PERS_ATTACKEE_ARMOR]++;
    }
    targ->health -= take;
    if (targ->client) {
        targ->s.health = targ->health;
        targ->client->ps.stats[STAT_HEALTH] = targ->health;
        targ->client->ps.stats[STAT_ARMOR] -= asave;
        targ->s.armor = targ->client->ps.stats[STAT_ARMOR];
    }

    if (g_dropDamagedHealth.integer && targ->client && take != 0) {
        int overkill = 0;
        if (targ->health < 0) {
            overkill = -targ->health;
        }
        Drop_DamagedHealth(attacker, targ, take - overkill);
    }

    if (g_vampiricDamage.value != 0.0f && attacker->client && targ->client && attacker != targ &&
        !OnSameTeam(attacker, targ) && attacker->health > 0 &&
        targ->client->ps.pm_type != PM_DEAD && take != 0 &&
        attacker->health < attacker->client->ps.stats[STAT_MAX_HEALTH] * 2) {
        int overkill = 0;
        int heal;
        if (targ->health < 0) {
            overkill = -targ->health;
        }
        heal = (int)((float)(take - overkill) * g_vampiricDamage.value);
        attacker->health += heal;
        attacker->s.health = attacker->health;
        attacker->client->ps.stats[STAT_HEALTH] = attacker->health;
        if (attacker->health > attacker->client->ps.stats[STAT_MAX_HEALTH] * 2) {
            attacker->health = attacker->client->ps.stats[STAT_MAX_HEALTH] * 2;
            attacker->s.health = attacker->health;
            attacker->client->ps.stats[STAT_HEALTH] = attacker->health;
        }
        attacker->s.health = attacker->client->ps.stats[STAT_HEALTH];
        if (g_debugVampiricDamage.integer) {
            G_Printf("%i: client:%i health:%i damage:%i leeched:%i\n",
                     level.time, targ->s.number, targ->health, take, heal);
        }
    }

    if (targ->client && armor_tiered.integer && targ->client->ps.stats[STAT_ARMOR] <= 0) {
        targ->client->ps.stats[STAT_ARMORTYPE] = 0;
    }

    if (targ->health <= 0) {
        if (targ->client && !FreezeTagInGame() && !FreezeTagInWarmup()) {
            targ->flags |= FL_NO_KNOCKBACK;
        }
        if (targ->health < -999) {
            targ->health = -999;
        }
        targ->enemy = attacker;
        targ->die(targ, inflictor, attacker, take, mod);
        if (g_gametype.integer == GT_FREEZE && targ->client) {
            if (g_freezeProtectedSpawnTime.integer &&
                level.time < targ->client->respawnTime + g_freezeProtectedSpawnTime.integer) {
                Freeze_InstaKill(targ, 1);
                return;
            }
            Freeze_AutoThaw(targ->client->sess.sessionTeam);
            return;
        }
        return;
    }
    if (targ->pain) {
        targ->pain(targ, attacker, take);
    }
}

static void G_DebugTraceShot(int entityNum) {
    gclient_t* client = g_entities[entityNum].client;

    if (client && client->ps.pm_type == PM_SPECTATOR) {
        trap_SendServerCommand(-1, "print \"A spectator has been shot!\n\"");
    }
}

qboolean CanDamage(gentity_t* targ, vec3_t origin) {
    vec3_t dest;
    trace_t tr;
    vec3_t midpoint;

    VectorAdd(targ->r.absmin, targ->r.absmax, midpoint);
    VectorScale(midpoint, 0.5, midpoint);

    VectorCopy(midpoint, dest);
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

    if (tr.fraction == 1.0 || tr.entityNum == targ->s.number)
        return qtrue;
    if (g_enableDebugTrace.integer)
        G_DebugTraceShot(tr.entityNum);

    VectorCopy(midpoint, dest);
    dest[0] += 15;
    dest[1] += 15;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

    if (tr.fraction == 1.0)
        return qtrue;
    if (g_enableDebugTrace.integer)
        G_DebugTraceShot(tr.entityNum);

    VectorCopy(midpoint, dest);
    dest[0] += 15;
    dest[1] -= 15;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

    if (tr.fraction == 1.0)
        return qtrue;
    if (g_enableDebugTrace.integer)
        G_DebugTraceShot(tr.entityNum);

    VectorCopy(midpoint, dest);
    dest[0] -= 15;
    dest[1] += 15;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

    if (tr.fraction == 1.0)
        return qtrue;
    if (g_enableDebugTrace.integer)
        G_DebugTraceShot(tr.entityNum);

    VectorCopy(midpoint, dest);
    dest[0] -= 15;
    dest[1] -= 15;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

    if (tr.fraction == 1.0)
        return qtrue;
    if (g_enableDebugTrace.integer)
        G_DebugTraceShot(tr.entityNum);

    return qfalse;
}

qboolean G_RadiusDamage(vec3_t origin, const vec3_t dir, const trace_t* trace, gentity_t* missile, gentity_t* ignore) {
    float dist, radius;
    gentity_t* ent;
    gentity_t* attacker;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t push;
    vec3_t splashOrigin, rocketOrigin;
    int i, e;
    qboolean hitClient = qfalse;

    radius = missile->splashRadius;
    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    VectorCopy(origin, splashOrigin);
    VectorCopy(origin, rocketOrigin);
    if (trace) {
        VectorMA(origin, g_splashdamageOffset.value, trace->plane.normal, splashOrigin);
        if (missile->s.weapon == WP_ROCKET_LAUNCHER) {
            VectorMA(splashOrigin, g_rocketsplashOffset.value, dir, rocketOrigin);
        }
    }

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        if (ent == ignore || !ent->takedamage) {
            continue;
        }

        dist = G_DistanceToBBox(ent, origin);
        if (dist > radius) {
            continue;
        }
        if (!CanDamage(ent, splashOrigin)) {
            continue;
        }
        if (missile->s.weapon == WP_ROCKET_LAUNCHER && !CanDamage(ent, rocketOrigin)) {
            continue;
        }

        attacker = missile->parent;
        if (ent != attacker && ent->client && attacker->client && !OnSameTeam(ent, attacker) &&
            (!FreezeTagInGame() || !ent->client->ps.powerups[PW_FREEZE])) {
            hitClient = qtrue;
        }

        VectorSubtract(ent->r.currentOrigin, origin, push);
        push[2] += (ent == attacker) ? g_knockback_z_self.integer : g_knockback_z.integer;

        if (g_playerCylinders.integer && trace && trace->fraction == 0.0f && ent != attacker) {
            G_Damage(ent, missile, attacker, push, origin, missile->damage, 0, missile->methodOfDeath);
        } else {
            G_Damage(ent, missile, attacker, push, origin,
                     (int)((1.0 - (double)dist / missile->splashRadius) * missile->splashDamage), DAMAGE_RADIUS,
                     missile->splashMethodOfDeath);
        }
    }

    return hitClient;
}

qboolean G_RadiusDamageThrough(vec3_t origin, const vec3_t dir, const trace_t* trace, vec3_t throughOrigin,
                               gentity_t* missile, gentity_t* ignore) {
    float dist, radius, reach;
    gentity_t* ent;
    gentity_t* attacker;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t push;
    vec3_t splashOrigin, rocketOrigin;
    int i, e;
    qboolean hitClient = qfalse;

    radius = missile->splashRadius;
    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    VectorCopy(origin, splashOrigin);
    VectorCopy(origin, rocketOrigin);
    if (trace) {
        VectorMA(origin, g_splashdamageOffset.value, trace->plane.normal, splashOrigin);
        if (missile->s.weapon == WP_ROCKET_LAUNCHER) {
            VectorMA(splashOrigin, g_rocketsplashOffset.value, dir, rocketOrigin);
        }
    }

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        if (ent == ignore || !ent->takedamage) {
            continue;
        }

        reach = missile->splashRadius;
        dist = G_DistanceToBBox(ent, origin);
        if (dist > reach) {
            continue;
        }

        if (CanDamage(ent, splashOrigin) ||
            (missile->s.weapon == WP_ROCKET_LAUNCHER && CanDamage(ent, rocketOrigin))) {
            VectorSubtract(ent->r.currentOrigin, origin, push);
        } else {
            if (missile->s.weapon != WP_ROCKET_LAUNCHER) {
                continue;
            }
            reach = missile->splashRadius * g_dmgThroughSurfaceDampening.value;
            dist = G_DistanceToBBox(ent, origin);
            if (dist > reach) {
                continue;
            }
            if (!CanDamage(ent, throughOrigin)) {
                continue;
            }
            VectorSubtract(ent->r.currentOrigin, throughOrigin, push);
        }

        attacker = missile->parent;
        if (ent != attacker && ent->client && attacker->client && !OnSameTeam(ent, attacker) &&
            (!FreezeTagInGame() || !ent->client->ps.powerups[PW_FREEZE])) {
            hitClient = qtrue;
        }

        push[2] += (ent == attacker) ? g_knockback_z_self.integer : g_knockback_z.integer;

        if (g_playerCylinders.integer && trace && trace->fraction == 0.0f && ent != attacker) {
            G_Damage(ent, missile, attacker, push, origin, missile->damage, 0, missile->methodOfDeath);
        } else {
            G_Damage(ent, missile, attacker, push, origin,
                     (int)((1.0 - (double)dist / reach) * missile->splashDamage), DAMAGE_RADIUS,
                     missile->splashMethodOfDeath);
        }
    }

    return hitClient;
}

qboolean G_WaterRadiusDamage(vec3_t origin, gentity_t* attacker, float damage, float radius) {
    float points, dist;
    gentity_t* ent;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t dir;
    int i, e;
    qboolean hitClient = qfalse;

    if (!g_lightningDischarge.integer || !(trap_PointContents(origin, -1) & MASK_WATER)) {
        return qfalse;
    }

    if (radius < 1) {
        radius = 1;
    }

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];
        if (!ent->takedamage) {
            continue;
        }

        dist = G_DistanceToBBox(ent, origin);
        if (dist >= radius) {
            continue;
        }

        points = damage * (1.0f - dist / radius);

        if (CanDamage(ent, origin) && ent->waterlevel) {
            if (ent != attacker && ent->client && attacker->client && !OnSameTeam(ent, attacker) &&
                (!FreezeTagInGame() || !ent->client->ps.powerups[PW_FREEZE])) {
                hitClient = qtrue;
            }

            VectorSubtract(ent->r.currentOrigin, origin, dir);
            dir[2] += (ent == attacker) ? g_knockback_z_self.integer : g_knockback_z.integer;

            G_Damage(ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, MOD_LIGHTNING_DISCHARGE);
        }
    }

    return hitClient;
}
