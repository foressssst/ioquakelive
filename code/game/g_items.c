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
//
#include "g_local.h"

/*

  Items are any object that a player can touch to gain some effect.

  Pickup will return the number of seconds until they should respawn.

  all items should pop when dropped in lava or slime

  Respawnable items don't actually go away when picked up, they are
  just made invisible and untouchable.  This allows them to ride
  movers and respawn appropriately.
*/

#define RESPAWN_ARMOR 25
#define RESPAWN_HEALTH 35
#define RESPAWN_AMMO 40
#define RESPAWN_HOLDABLE 60
#define RESPAWN_MEGAHEALTH 35  // 120
#define RESPAWN_POWERUP 120

//======================================================================

/*
===============
G_ItemPickupAnnounce

[QL] 0x1004dc20. A powerup taken is told to everybody ("X got the Quad Damage!"); the red armour
and the mega health to the taker's teammates and the spectators ("X got the Red Armor."), in
the flag modes coloured by the base the item is nearer to. What was dropped is not told.
===============
*/
static void G_ItemPickupAnnounce(gentity_t* ent, gentity_t* other, qboolean powerup) {
    const char* color = S_COLOR_WHITE;
    const char* name;
    vec3_t red, blue;
    int side, enemy, i;

    if (!other->client || (ent->flags & FL_DROPPED_ITEM)) {
        return;
    }

    VectorSubtract(ent->s.origin, level.itemTimerRedBase, red);
    VectorSubtract(level.itemTimerBlueBase, ent->s.origin, blue);
    side = (VectorLengthSquared(blue) > VectorLengthSquared(red)) ? TEAM_RED : TEAM_BLUE;

    switch (g_gametype.integer) {
        case GT_CTF:
        case GT_1FCTF:
        case GT_OBELISK:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            break;
        default:
            color = S_COLOR_YELLOW;
            break;
    }

    name = ent->item->pickup_name;
    if (powerup) {
        trap_SendServerCommand(-1, va("print \"%s%s got the %s!^7\n\"", other->client->pers.netname, color, name));
        return;
    }

    enemy = other->client->sess.sessionTeam;
    if (enemy == TEAM_RED) {
        enemy = TEAM_BLUE;
    } else if (enemy == TEAM_BLUE) {
        enemy = TEAM_RED;
    }
    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];

        if (cl->pers.connected != CON_CONNECTED || &g_entities[i] == other || cl->sess.sessionTeam == enemy) {
            continue;
        }
        if ((g_gametype.integer >= GT_CTF && g_gametype.integer <= GT_HARVESTER) || g_gametype.integer == GT_AD) {
            color = (side == TEAM_RED) ? S_COLOR_RED : S_COLOR_BLUE;
        } else {
            color = S_COLOR_WHITE;
        }
        trap_SendServerCommand(i, va("print \"%s ^7got the %s%s.\n\"", other->client->pers.netname, color, name));
    }
}

/*
===============
Pickup_Powerup

[QL] 0x1004dfe0.
===============
*/
int Pickup_Powerup(gentity_t* ent, gentity_t* other) {
    int quantity;
    int i;
    int giTag;
    int team;
    gclient_t* client;

    giTag = ent->item->giTag;
    team = other->client->sess.sessionTeam;

    if (ent->count) {
        quantity = ent->count;
    } else {
        quantity = ent->item->quantity;
    }

    // [QL] 0x1004dffb: a powerup dropped with g_droppedPowerupsDecay lost a second for each
    // second it lay on the ground
    if (ent->flags & FL_DROPPED_ITEM_TIMED) {
        quantity -= (level.time - ent->s.time) / 1000;
        if (quantity < 0) {
            quantity = 0;
        }
    }

    // [QL] 0x1004e02a: a new battle suit or quad starts its kill count again (ps.stats[5] and
    // ps.stats[13], the award counters of player_die)
    if (giTag == PW_BATTLESUIT && !other->client->ps.powerups[PW_BATTLESUIT]) {
        other->client->ps.stats[5] = 0;
    }
    if (giTag == PW_QUAD && !other->client->ps.powerups[PW_QUAD]) {
        other->client->ps.stats[13] = 0;
    }

    // [QL] 0x1004e089: the quad of the Quad Hog lasts g_quadHogTime
    if (g_quadHog.integer && g_gametype.integer == GT_FFA) {
        quantity = g_quadHogTime.integer;
    }

    if (!other->client->ps.powerups[giTag]) {
        // round timing to seconds to make multiple powerup timers
        // count in sync
        other->client->ps.powerups[giTag] =
            level.time - (level.time % 1000);
    }

    other->client->ps.powerups[giTag] += quantity * 1000;

    // [QL] pickup statistics
    other->client->expandedStats.powerups[giTag]++;

    // only fixed (non-dropped) powerup spawns count towards the per-powerup tallies
    if (ent->s.modelindex2 == 0) {
        switch (giTag) {
            case PW_QUAD:
                other->client->expandedStats.numQuadDamagePickups++;
                if (g_gametype.integer > GT_RACE)
                    level.numQuadDamagePickups[team]++;
                break;
            case PW_BATTLESUIT:
                other->client->expandedStats.numBattleSuitPickups++;
                if (g_gametype.integer > GT_RACE)
                    level.numBattleSuitPickups[team]++;
                break;
            // [QL] PW_REGEN/PW_INVIS are swapped vs the old stat field order, so
            // PW_INVIS lands in the regen counters and PW_REGEN in the invis ones
            case PW_INVIS:
                other->client->expandedStats.numRegenerationPickups++;
                if (g_gametype.integer > GT_RACE)
                    level.numRegenerationPickups[team]++;
                break;
            case PW_HASTE:
                other->client->expandedStats.numHastePickups++;
                if (g_gametype.integer > GT_RACE)
                    level.numHastePickups[team]++;
                break;
            case PW_REGEN:
                other->client->expandedStats.numInvisibilityPickups++;
                if (g_gametype.integer > GT_RACE)
                    level.numInvisibilityPickups[team]++;
                break;
            default:
                break;
        }
    }

    G_ItemPickupAnnounce(ent, other, qtrue);

    // [QL] 0x1004e271: AW_RESOURCE_HOG (0x1d), a quad and a battle suit picked up
    if (!trap_HasAchievement(other->client->ps.clientNum, 0x1d) &&
        other->client->expandedStats.numQuadDamagePickups > 0 &&
        other->client->expandedStats.numBattleSuitPickups > 0) {
        trap_SetAchievement(other->client->ps.clientNum, 0x1d);
    }

    // give any nearby players a "denied" anti-reward
    for (i = 0; i < level.maxclients; i++) {
        vec3_t delta;
        float len;
        vec3_t forward;
        trace_t tr;

        client = &level.clients[i];
        if (client == other->client) {
            continue;
        }
        if (client->pers.connected == CON_DISCONNECTED) {
            continue;
        }
        if (client->ps.stats[STAT_HEALTH] <= 0) {
            continue;
        }

        // if same team in team game, no sound
        // cannot use OnSameTeam as it expects to g_entities, not clients
        if (g_gametype.integer >= GT_TEAM && other->client->sess.sessionTeam == client->sess.sessionTeam) {
            continue;
        }

        // if too far away, no sound
        VectorSubtract(ent->s.pos.trBase, client->ps.origin, delta);
        len = VectorNormalize(delta);
        if (len > 192) {
            continue;
        }

        // if not facing, no sound
        AngleVectors(client->ps.viewangles, forward, NULL, NULL);
        if (DotProduct(delta, forward) < 0.4) {
            continue;
        }

        // if not line of sight, no sound
        trap_Trace(&tr, client->ps.origin, NULL, NULL, ent->s.pos.trBase, ENTITYNUM_NONE, CONTENTS_SOLID);
        if (tr.fraction != 1.0) {
            continue;
        }

        // [QL] 0x1004e3f6: AW_BANDIT (0xf), the quad taken in front of somebody
        if (!trap_HasAchievement(other->client->ps.clientNum, 0xf) && ent->item->giTag == PW_QUAD) {
            trap_SetAchievement(other->client->ps.clientNum, 0xf);
        }

        // anti-reward
        client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_DENIEDREWARD;
        other->client->expandedStats.numDenials++;
    }
    return RESPAWN_POWERUP;
}

//======================================================================

/*
===============
Pickup_PersistantPowerup

[QL] 0x1004e4a0. The rune's number goes in STAT_RUNE and its powerup never runs out; the
spawn point it stood on is free again. It does not respawn: -1.
===============
*/
int Pickup_PersistantPowerup(gentity_t* ent, gentity_t* other) {
    int rune = ent->item->giTag;

    other->client->persistantPowerup = ent;
    other->client->ps.stats[STAT_RUNE] = rune;
    other->client->ps.powerups[PW_INVULNERABILITY + rune] = INT_MAX;
    level.runeSpawnSpot[rune] = 0;

    return -1;
}

//======================================================================

int Pickup_Holdable(gentity_t* ent, gentity_t* other) {
    other->client->ps.stats[STAT_HOLDABLE_ITEM] = ent->item - bg_itemlist;

    if (ent->item->giTag == HI_KAMIKAZE) {
        other->client->ps.eFlags |= EF_KAMIKAZE;
    }

    // [QL] 0x1004df18: flight comes with its thrust, a full tank and the refuel rate. The
    // tank is kept under 32001, since pmove only burns it when it is 32000 or less
    if (ent->item->giTag == HI_FLIGHT) {
        other->client->ps.stats[STAT_FLIGHT_THRUST] = g_flightThrust.integer;
        other->client->ps.stats[STAT_CUR_FLIGHT_FUEL] = MIN(g_maxFlightFuel.integer, 32001);
        other->client->ps.stats[STAT_MAX_FLIGHT_FUEL] = MIN(g_maxFlightFuel.integer, 32001);
        other->client->ps.stats[STAT_FLIGHT_REFUEL] = g_flightRefuelRate.integer;
    }

    // [QL] pickup statistics
    if (ent->item->giTag == HI_MEDKIT && g_gametype.integer > GT_RACE) {
        other->client->expandedStats.numMedkitPickups++;
        level.numMedkitPickups[other->client->sess.sessionTeam]++;
    }
    other->client->expandedStats.holdablePickups[ent->item->giTag]++;

    return RESPAWN_HOLDABLE;
}

//======================================================================

// [QL] Per-weapon ammo data from binary weapon table (qagamex86.dll 0x1008ff00)
// Short names used for g_startingAmmo_<name> cvars (DOM gametype max ammo)
const char *weaponShortName[WP_NUM_WEAPONS] = {
    "",     // WP_NONE
    "g",    // WP_GAUNTLET
    "mg",   // WP_MACHINEGUN
    "sg",   // WP_SHOTGUN
    "gl",   // WP_GRENADE_LAUNCHER
    "rl",   // WP_ROCKET_LAUNCHER
    "lg",   // WP_LIGHTNING
    "rg",   // WP_RAILGUN
    "pg",   // WP_PLASMAGUN
    "bfg",  // WP_BFG
    "gh",   // WP_GRAPPLING_HOOK
    "ng",   // WP_NAILGUN
    "pl",   // WP_PROX_LAUNCHER
    "cg",   // WP_CHAINGUN
    "hmg",  // WP_HMG
};

// Ammo quantity given by ammo_pack per weapon (binary offset +0x14 in weapon table)
static const int ammoPackQuantity[WP_NUM_WEAPONS] = {
    0,    // WP_NONE
    0,    // WP_GAUNTLET
    50,   // WP_MACHINEGUN
    5,    // WP_SHOTGUN
    5,    // WP_GRENADE_LAUNCHER
    5,    // WP_ROCKET_LAUNCHER
    50,   // WP_LIGHTNING
    5,    // WP_RAILGUN
    50,   // WP_PLASMAGUN
    2,    // WP_BFG
    0,    // WP_GRAPPLING_HOOK
    5,    // WP_NAILGUN
    5,    // WP_PROX_LAUNCHER
    100,  // WP_CHAINGUN
    50,   // WP_HMG
};

// Maximum ammo per weapon for standard gametypes: canonical table now lives in
// bg_misc.c (shared, declared in bg_public.h) so cgame links it too. Add_Ammo
// below still uses maxAmmoStandard by name.

/*
===============
BG_ParseGametypeStringtoFlag

[QL] Parses space-separated weapon short names (e.g., "sg rl lg") into a weapon
bitmask. Used by SP_worldspawn to parse the "disable_loadout" entity key.
Binary: 0x1002da40
===============
*/
int BG_ParseGametypeStringtoFlag(const char *str) {
    char buf[MAX_STRING_CHARS];
    char *tok;
    int mask = 0, i;

    if (!str || !str[0])
        return 0;

    Q_strncpyz(buf, str, sizeof(buf));
    tok = strtok(buf, " ");
    while (tok) {
        for (i = WP_GAUNTLET; i < WP_NUM_WEAPONS; i++) {
            if (weaponShortName[i][0] && !Q_stricmp(tok, weaponShortName[i])) {
                mask |= (1 << i);
                break;
            }
        }
        tok = strtok(NULL, " ");
    }
    return mask;
}

/*
===============
Add_Ammo

[QL] 0x1004dab0. With g_infiniteAmmo a pickup adds nothing (the weapons already hold
-1), except that under g_lightningDischarge the lightning gun's ammo is set back to
-1. The ammo pack (weapon == WP_NUM_WEAPONS) fills every weapon owned, then the ammo
is clamped: to g_startingAmmo_<weapon> in Domination, to the fixed table elsewhere.

The cvars read are g_lightningDischarge (0x105acbec) and g_infiniteAmmo
(0x105a31cc). Retail's g_loadout is 0x104b334c and nothing here reads it.
===============
*/
void Add_Ammo(gentity_t* ent, int weapon, int count) {
    int i;

    if (g_infiniteAmmo.integer) {
        if (g_lightningDischarge.integer && weapon == WP_LIGHTNING) {
            ent->client->ps.ammo[WP_LIGHTNING] = -1;
        }
        return;
    }

    // don't add to weapons with unlimited ammo
    if (ent->client->ps.ammo[weapon] == -1) {
        return;
    }

    if (weapon == WP_NUM_WEAPONS) {
        // ammo_pack: add pack quantity to each owned weapon
        for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
            if (ent->client->ps.stats[STAT_WEAPONS] & (1 << i)) {
                ent->client->ps.ammo[i] += ammoPackQuantity[i];
            }
        }
    } else {
        // single weapon ammo pickup
        ent->client->ps.ammo[weapon] += count;
    }

    // clamp ammo to max per weapon
    if (g_gametype.integer == GT_DOMINATION) {
        // DOM: max ammo = g_startingAmmo_<weapon> cvar value
        for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
            int max = trap_Cvar_VariableIntegerValue(
                va("g_startingAmmo_%s", weaponShortName[i]));
            if (ent->client->ps.ammo[i] > max) {
                ent->client->ps.ammo[i] = max;
            }
        }
    } else {
        // all other gametypes: use fixed max ammo table
        for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
            if (maxAmmoStandard[i] > 0 &&
                ent->client->ps.ammo[i] > maxAmmoStandard[i]) {
                ent->client->ps.ammo[i] = maxAmmoStandard[i];
            }
        }
    }
}

/*
===============
Pickup_Ammo

[QL] Binary-matched from qagamex86.dll FUN_1004e580.
===============
*/
int Pickup_Ammo(gentity_t* ent, gentity_t* other) {
    int quantity;

    if (ent->count) {
        quantity = ent->count;
    } else {
        quantity = ent->item->quantity;
    }

    Add_Ammo(other, ent->item->giTag, quantity);

    // [QL] pickup statistics
    other->client->expandedStats.numAmmoPickups++;

    return g_ammoRespawn.integer;
}

//======================================================================

int Pickup_Weapon(gentity_t* ent, gentity_t* other) {
    int quantity;
    int giTag;
    gclient_t* client;

    client = other->client;
    giTag = ent->item->giTag;

    // [QL] when weapons don't respawn (g_weaponRespawn == 0), walking over a fixed
    // (non-dropped) weapon spawn does nothing if you already own the weapon and it
    // still has ammo. Binary gates this on g_weaponRespawn (0x1004e9d5), not g_loadout.
    if (g_weaponRespawn.integer == 0) {
        if ((client->ps.stats[STAT_WEAPONS] & (1 << giTag)) &&
            client->ps.ammo[giTag] != 0 &&
            ent->s.modelindex2 == 0) {
            return 0;
        }
    }

    // [QL] negative count is treated as a fatal map error
    if (ent->count < 0) {
        G_Error("Pickup_Weapon: item has infinite ammo");
    }

    // add the weapon
    client->ps.stats[STAT_WEAPONS] |= (1 << giTag);

    if (ent->count) {
        quantity = ent->count;
    } else {
        quantity = ent->item->quantity;
    }

    Add_Ammo(other, giTag, quantity);

    // [QL] when weapons don't respawn, a fixed weapon spawn sets ammo to the weapon's
    // base amount; dropped weapons keep the ammo Add_Ammo just gave them.
    // Binary gates this on g_weaponRespawn (0x1004ea3f), not g_loadout.
    if (g_weaponRespawn.integer == 0 && ent->s.modelindex2 == 0) {
        client->ps.ammo[giTag] = ent->item->quantity;
    }

    if (giTag == WP_GRAPPLING_HOOK)
        client->ps.ammo[giTag] = -1;  // unlimited ammo

    // [QL] pickup statistics
    client->expandedStats.weaponPickups[giTag]++;

    // [QL] When weapons don't respawn, Pickup_Weapon returns 0, so Touch_Item bails
    // out on respawn==0 before emitting the pickup event. Emit it here instead
    // (binary 0x1004eaa0). Predicting clients get a predictable event.
    if (g_weaponRespawn.integer == 0) {
        if (client->pers.predictItemPickup) {
            G_AddPredictableEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
        } else {
            G_AddEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
        }
    }

    // [QL] Binary uses g_weaponRespawn for all gametypes; Q3's g_weaponTeamRespawn
    // override is gone.
    return g_weaponRespawn.integer;
}

//======================================================================

int Pickup_Health(gentity_t* ent, gentity_t* other) {
    int max;
    gclient_t* client;

    client = other->client;

    // the small (4) and the mega (1) overheal to 2x max; others cap at max. [QL] 0x1004e810: in the
    // training below skill 3 the bots never go over their maximum
    if (g_training.integer && trap_Cvar_VariableValue("g_spSkill") < 3.0f && (other->r.svFlags & SVF_BOT)) {
        max = client->ps.stats[STAT_MAX_HEALTH];
    } else if (ent->item->giTag == 4 || ent->item->giTag == 1) {
        max = client->ps.stats[STAT_MAX_HEALTH] * 2;
    } else {
        max = client->ps.stats[STAT_MAX_HEALTH];
    }

    other->health += ent->item->quantity;

    if (other->health > max) {
        other->health = max;
    }
    client->ps.stats[STAT_HEALTH] = other->health;

    // [QL] pickup statistics
    if (ent->item->quantity == 100) {
        // mega health
        if (ent->pickupCount == 0) {
            client->expandedStats.numFirstMegaHealthPickups++;
        }
        client->expandedStats.numMegaHealthPickups++;
        if (ent->pickupCount > 0) {
            client->expandedStats.megaHealthPickupTime += level.time - ent->spawnTime;
        }
        if (g_gametype.integer > GT_RACE) {
            level.numMegaHealthPickups[client->sess.sessionTeam]++;
        }
    } else {
        client->expandedStats.numHealthPickups++;
    }

    if (ent->item->giTag == 1) {  // the mega, 0x1004e939
        G_ItemPickupAnnounce(ent, other, qfalse);
    }

    return RESPAWN_HEALTH;
}

//======================================================================

int Pickup_Armor(gentity_t* ent, gentity_t* other) {
    int max;
    int giTag;
    int team;
    gclient_t* client;

    client = other->client;
    max = client->ps.stats[STAT_MAX_HEALTH] * 2;
    giTag = ent->item->giTag;
    team = client->sess.sessionTeam;

    // [QL] pickup statistics, keyed on the armour tier giTag (1=red, 2=yellow,
    // 3=green, anything else counts as a shard)
    switch (giTag) {
        case 1:  // red
            if (ent->pickupCount == 0)
                client->expandedStats.numFirstRedArmorPickups++;
            client->expandedStats.numRedArmorPickups++;
            if (ent->pickupCount > 0)
                client->expandedStats.redArmorPickupTime += level.time - ent->spawnTime;
            if (g_gametype.integer > GT_RACE)
                level.numRedArmorPickups[team]++;
            break;
        case 2:  // yellow
            if (ent->pickupCount == 0)
                client->expandedStats.numFirstYellowArmorPickups++;
            client->expandedStats.numYellowArmorPickups++;
            if (ent->pickupCount > 0)
                client->expandedStats.yellowArmorPickupTime += level.time - ent->spawnTime;
            if (g_gametype.integer > GT_RACE)
                level.numYellowArmorPickups[team]++;
            break;
        case 3:  // green
            if (ent->pickupCount == 0)
                client->expandedStats.numFirstGreenArmorPickups++;
            client->expandedStats.numGreenArmorPickups++;
            if (ent->pickupCount > 0)
                client->expandedStats.greenArmorPickupTime += level.time - ent->spawnTime;
            if (g_gametype.integer > GT_RACE)
                level.numGreenArmorPickups[team]++;
            break;
        default:  // shard
            client->expandedStats.numArmorPickups++;
            break;
    }

    if (armor_tiered.integer == 0) {
        // Classic armor: just add quantity
        other->client->ps.stats[STAT_ARMOR] += ent->item->quantity;
        if (other->client->ps.stats[STAT_ARMOR] > max) {
            other->client->ps.stats[STAT_ARMOR] = max;
        }
        if (giTag == 1) {
            G_ItemPickupAnnounce(ent, other, qfalse);
        }
        // [QL] 0x1004e6f4 and 0x1004e803: the armor also goes out in the entity, for teammates
        other->s.armor = other->client->ps.stats[STAT_ARMOR];
        return RESPAWN_ARMOR;
    }

    // [QL] Tiered armor system
    if (giTag == 1) {  // Red armor
        other->client->ps.stats[STAT_ARMOR] += 150;
        if (other->client->ps.stats[STAT_ARMOR] > 200)
            other->client->ps.stats[STAT_ARMOR] = 200;
        other->client->ps.stats[STAT_ARMORTYPE] = 2;
    } else if (giTag == 2) {  // Yellow armor
        other->client->ps.stats[STAT_ARMOR] += 100;
        if (other->client->ps.stats[STAT_ARMOR] > 150)
            other->client->ps.stats[STAT_ARMOR] = 150;
        other->client->ps.stats[STAT_ARMORTYPE] = 1;
    } else if (giTag == 3) {  // Green armor
        other->client->ps.stats[STAT_ARMOR] += 50;
        if (other->client->ps.stats[STAT_ARMOR] > 100)
            other->client->ps.stats[STAT_ARMOR] = 100;
        other->client->ps.stats[STAT_ARMORTYPE] = 0;
    } else {  // Armor shard
        if (other->client->ps.stats[STAT_ARMOR] < 1)
            other->client->ps.stats[STAT_ARMORTYPE] = 0;
        // [QL] tiered-armor shard adds a fixed 2 (binary literal at 0x1004e6b8),
        // NOT ent->item->quantity (that path is only the non-tiered branch above)
        other->client->ps.stats[STAT_ARMOR] += 2;
    }

    if (other->client->ps.stats[STAT_ARMOR] > max) {
        other->client->ps.stats[STAT_ARMOR] = max;
    }
    if (giTag == 1) {
        G_ItemPickupAnnounce(ent, other, qfalse);
    }
    other->s.armor = other->client->ps.stats[STAT_ARMOR];

    return RESPAWN_ARMOR;
}

//======================================================================

/*
===============
QuadHog_Touch

[QL] 0x1004d290. Whoever takes the quad of the Quad Hog is announced to everybody.
===============
*/
static void QuadHog_Touch(gentity_t* ent, gentity_t* other, trace_t* trace) {
    if (!other->client) {
        return;
    }
    trap_SendServerCommand(-1, va("cp \"%s is the ^5Quad Hog!\"", other->client->pers.netname));
    Touch_Item(ent, other, trace);
}

/*
===============
QuadHog_Launch

[QL] 0x1004d2d0. The quad of the Quad Hog, thrown like a dropped item: it announces whoever
takes it and goes away after g_quadHogIdle seconds on the ground.
===============
*/
static gentity_t* QuadHog_Launch(gitem_t* item, vec3_t origin, vec3_t velocity) {
    gentity_t* ent;

    ent = G_Spawn();
    ent->s.eType = ET_ITEM;
    ent->s.modelindex2 = 1;
    ent->s.modelindex = item - bg_itemlist;
    ent->classname = item->classname;
    ent->item = item;
    VectorSet(ent->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
    VectorSet(ent->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
    ent->r.contents = CONTENTS_TRIGGER;
    ent->touch = QuadHog_Touch;

    G_SetOrigin(ent, origin);
    ent->s.pos.trType = TR_GRAVITY;
    ent->s.pos.trTime = level.time;
    VectorCopy(velocity, ent->s.pos.trDelta);
    ent->s.eFlags |= EF_BOUNCE_HALF;
    ent->flags = FL_DROPPED_ITEM;

    ent->think = G_FreeEntity;
    ent->nextthink = level.time + g_quadHogIdle.integer * 1000;

    trap_LinkEntity(ent);
    return ent;
}

/*
===============
QuadHog_Spawn

[QL] 0x1004d420. The quad comes out at one of the first sixteen deathmatch spawns, with the
powerup respawn sound for everybody. Retail looks for a spawn forever on a map with none;
here it gives up.
===============
*/
void QuadHog_Spawn(void) {
    gentity_t* spot;
    gentity_t* te;
    int n;

    if (level.intermissionQueued || level.intermissionTime) {
        return;
    }

    spot = NULL;
    for (n = (int)((rand() & 0x7fff) / 32767.0 * 16.0); n > 0; n--) {
        spot = G_Find(spot, FOFS(classname), "info_player_deathmatch");
    }
    if (!spot) {
        spot = G_Find(NULL, FOFS(classname), "info_player_deathmatch");
        if (!spot) {
            return;
        }
    }

    QuadHog_Launch(BG_FindItemForPowerup(PW_QUAD), spot->s.origin, vec3_origin);
    te = G_TempEntity(spot->s.origin, EV_GLOBAL_SOUND);
    te->s.eventParm = G_SoundIndex("sound/items/poweruprespawn.wav");
    te->r.svFlags |= SVF_BROADCAST;
    trap_SendServerCommand(-1, "cp \" ^5Quad ^7respawned!\"");
}

/*
===============
QuadHog_Check

[QL] 0x1004d500. Each frame of an FFA with g_quadHog: when nobody holds the quad and none
lies in the map, a new one comes out.
===============
*/
void QuadHog_Check(void) {
    gentity_t* ent;
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (g_entities[i].client && g_entities[i].client->ps.powerups[PW_QUAD]) {
            return;
        }
    }
    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), "item_quad")) != NULL) {
        if (ent->r.linked) {
            return;
        }
    }
    QuadHog_Spawn();
}

/*
===============
G_CountSpawnSpots

[QL] 0x10055000. The deathmatch, red and blue spawn points of the map, 25 at most of each,
which is what the runes are placed with.
===============
*/
void G_CountSpawnSpots(void) {
    gentity_t* spot;

    level.numSpawnSpotsFFA = 0;
    level.numSpawnSpotsRed = 0;
    level.numSpawnSpotsBlue = 0;

    spot = NULL;
    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        if (level.numSpawnSpotsFFA < 25) {
            level.numSpawnSpotsFFA++;
        }
    }
    spot = NULL;
    while ((spot = G_Find(spot, FOFS(classname), "team_CTF_redspawn")) != NULL) {
        if (level.numSpawnSpotsRed < 25) {
            level.numSpawnSpotsRed++;
        }
    }
    spot = NULL;
    while ((spot = G_Find(spot, FOFS(classname), "team_CTF_bluespawn")) != NULL) {
        if (level.numSpawnSpotsBlue < 25) {
            level.numSpawnSpotsBlue++;
        }
    }
}

/*
===============
G_SpawnRune

[QL] 0x1004d650. A rune falls in at a spawn point no other rune stands on: a red or a blue one
when that team has five or more, a deathmatch one otherwise. The spot is kept until somebody
takes it. Retail looks for a spot forever when none is left; here the rune stays out.
===============
*/
static void G_SpawnRune(int rune, int team) {
    const char* classname;
    gentity_t* ent;
    gentity_t* spot;
    gitem_t* item;
    int spots[16];
    int total, count, i, j;

    ent = G_Spawn();

    if (team == TEAM_RED && level.numSpawnSpotsRed >= 5) {
        classname = "team_CTF_redspawn";
        total = level.numSpawnSpotsRed;
    } else if (team == TEAM_BLUE && level.numSpawnSpotsBlue >= 5) {
        classname = "team_CTF_bluespawn";
        total = level.numSpawnSpotsBlue;
    } else {
        classname = "info_player_deathmatch";
        total = (level.numSpawnSpotsFFA > 4) ? level.numSpawnSpotsFFA : 0;
    }

    count = 0;
    spot = NULL;
    for (i = 0; i < total && count < 16; i++) {
        spot = G_Find(spot, FOFS(classname), classname);
        if (!spot) {
            break;
        }
        for (j = 1; j < 5; j++) {
            if (level.runeSpawnSpot[j] == spot->s.number) {
                break;
            }
        }
        if (j == 5) {
            spots[count++] = spot->s.number;
        }
    }
    if (!count) {
        G_FreeEntity(ent);
        return;
    }

    level.runeSpawnSpot[rune] = spots[rand() % count];
    spot = &g_entities[level.runeSpawnSpot[rune]];

    item = BG_FindItemForPowerup((powerup_t)(PW_INVULNERABILITY + rune));
    ent->s.eType = ET_ITEM;
    ent->s.modelindex2 = 1;
    ent->s.modelindex = item - bg_itemlist;
    ent->item = item;
    ent->classname = item->classname;
    VectorSet(ent->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
    VectorSet(ent->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
    ent->r.contents = CONTENTS_TRIGGER;
    ent->touch = Touch_Item;

    G_SetOrigin(ent, spot->s.origin);
    ent->s.pos.trType = TR_GRAVITY;
    ent->s.pos.trTime = level.time;
    ent->s.eFlags |= EF_BOUNCE_HALF;

    trap_LinkEntity(ent);
}

/*
===============
G_SpawnRunes

[QL] 0x1004d950. With g_runes, outside race, the four runes come out when the level starts.
A map with five red and five blue spawns alternates them between the two sides.
===============
*/
void G_SpawnRunes(void) {
    qboolean alternate = qfalse;
    int team = TEAM_FREE;
    int rune;

    if (!g_runes.integer || g_gametype.integer == GT_RACE || level.intermissionQueued ||
        level.intermissionTime) {
        return;
    }

    if (level.numSpawnSpotsRed < 5) {
        if (level.numSpawnSpotsBlue < 5 && level.numSpawnSpotsFFA < 5) {
            G_Printf("WARNING: not enough available spawns for rune spawning\n");
            return;
        }
    } else if (level.numSpawnSpotsBlue > 4) {
        alternate = qtrue;
        team = (rand() & 1) ? TEAM_BLUE : TEAM_RED;
    }

    for (rune = 1; rune < 5; rune++) {
        if (alternate) {
            team = (team == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
        }
        G_SpawnRune(rune, team);
    }
}

/*
===============
G_RuneRespawn

[QL] 0x1004da10. A rune left on the ground goes back to a spawn point, a team's one when the
map has five of each.
===============
*/
static void G_RuneRespawn(gentity_t* ent) {
    int rune = ent->item->giTag;
    int team = TEAM_FREE;

    if (level.numSpawnSpotsRed >= 5 && level.numSpawnSpotsBlue >= 5) {
        team = (rand() & 1) ? TEAM_BLUE : TEAM_RED;
    }
    G_FreeEntity(ent);
    G_SpawnRune(rune, team);
}

/*
===============
Item_NotifySpectator

[QL] 0x1004eac0. Tells one spectator (EF_SPECTATOR) about an item with a timer, in an
EV_ITEM_PICKUP_SPEC event only that client gets: otherEntityNum the item entity,
modelindex its kind, time when it comes back, modelindex2 whether it belongs to a
random-respawn team, groundEntityNum whether it has just respawned.

Dropped items, green armor, holdables and items with spawnflag 0x100 are left out. In
CTF an item with duplicates carries the base it is nearer to in otherEntityNum2 (1
red, 2 blue) with the squared distance in time2; one with no duplicate carries 3.
===============
*/
void Item_NotifySpectator(gentity_t *ent, int clientNum, qboolean isRespawn) {
    gclient_t *cl = &level.clients[clientNum];
    gentity_t *te;
    vec3_t delta;
    float distRed, distBlue;

    if (cl->pers.connected != CON_CONNECTED || !(cl->ps.eFlags & EF_SPECTATOR))
        return;
    if (ent->flags & FL_DROPPED_ITEM)
        return;
    if (ent->item->giType == IT_ARMOR && ent->item->giTag == 3)
        return;
    if (ent->item->giType == IT_HOLDABLE)
        return;
    if ((ent->spawnflags & 0x100) || !ent->item->itemTimer)
        return;

    te = G_TempEntity(ent->s.pos.trBase, EV_ITEM_PICKUP_SPEC);
    te->s.time = ent->nextthink;
    te->s.otherEntityNum = ent->s.number;
    te->s.modelindex = ent->s.modelindex;
    te->s.modelindex2 = (ent->team != NULL);
    te->r.svFlags |= SVF_BROADCAST | SVF_SINGLECLIENT;
    te->r.singleClient = g_entities[clientNum].s.number;
    te->s.groundEntityNum = (isRespawn != qfalse);

    if (g_gametype.integer == GT_CTF) {
        if (level.itemTimerCounts[ent->item - bg_itemlist] < 2) {
            te->s.otherEntityNum2 = 3;
            return;
        }
        VectorSubtract(level.itemTimerRedBase, ent->s.origin, delta);
        distRed = DotProduct(delta, delta);
        VectorSubtract(level.itemTimerBlueBase, ent->s.origin, delta);
        distBlue = DotProduct(delta, delta);
        te->s.otherEntityNum2 = (distBlue <= distRed) ? TEAM_BLUE : TEAM_RED;
        te->s.time2 = (int)(te->s.otherEntityNum2 == TEAM_RED ? distRed : distBlue);
    }
}

/*
===============
Item_NotifySpectators

[QL] qagamex86.dll 0x1004ed10: an item's timer to every spectator. Touch_Item sends it with a
new respawn time (0x1004f4a3), RespawnItem as the item comes back (0x1004f000) and G_SpawnItem
as the item appears (0x10050b9f).
===============
*/
void Item_NotifySpectators(gentity_t *ent, qboolean isRespawn) {
    int i;

    // [QL] gate is g_specItemTimers, NOT g_itemTimers - verified from QLDS qagamex86.dll
    // (g_specItemTimers.integer, 0x10598d8c)
    if (!g_specItemTimers.integer)
        return;
    if (!ent || !ent->item)
        return;
    // [QL] only items that have a timer (item+0x3c)
    if (!ent->item->itemTimer)
        return;
    if (ent->item->giType == IT_ARMOR && ent->item->giTag == 3)
        return;
    if (ent->item->giType == IT_HOLDABLE)
        return;

    for (i = 0; i < level.maxclients; i++) {
        Item_NotifySpectator(ent, i, isRespawn);
    }
}

/*
===============
G_SendItemTimersToSpectator

[QL] qagamex86.dll 0x1004ed70: every item timer to one spectator, from "specresp" (0x100462e5)
and from SetTeam when the client becomes a spectator (0x1004056f)
===============
*/
void G_SendItemTimersToSpectator(int clientNum) {
    gentity_t *ent;
    int i;

    if (!g_specItemTimers.integer || !(level.clients[clientNum].ps.eFlags & EF_SPECTATOR) ||
        level.maxclients >= MAX_GENTITIES) {
        return;
    }
    for (i = level.maxclients; i < MAX_GENTITIES; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || !ent->item || !ent->item->itemTimer) {
            continue;
        }
        if ((ent->item->giType == IT_ARMOR && ent->item->giTag == 3) || ent->item->giType == IT_HOLDABLE) {
            continue;
        }
        Item_NotifySpectator(ent, clientNum, qfalse);
    }
}

/*
===============
G_FindTeamFlag

[QL] qagamex86.dll 0x100690f0: a team's flag on the map, the last one that wasn't dropped
===============
*/
static gentity_t *G_FindTeamFlag(int team) {
    const char *classname;
    gentity_t *ent, *found = NULL;
    int i;

    switch (team) {
    case TEAM_FREE:
        classname = "team_CTF_neutralflag";
        break;
    case TEAM_RED:
        classname = "team_CTF_redflag";
        break;
    case TEAM_BLUE:
        classname = "team_CTF_blueflag";
        break;
    default:
        return NULL;
    }
    for (i = 0; i < level.num_entities; i++) {
        ent = &g_entities[i];
        if (ent->inuse && ent->classname && !Q_stricmp(classname, ent->classname) && !(ent->flags & FL_DROPPED_ITEM)) {
            found = ent;
        }
    }
    return found;
}

/*
===============
G_InitItemTimerBases

[QL] qagamex86.dll 0x10069190, from G_InitGame in CTF: where the two flags stand and how many
items of each kind with a timer the map has, which decide the side of each spectator timer
===============
*/
void G_InitItemTimerBases(void) {
    gentity_t *red = G_FindTeamFlag(TEAM_RED);
    gentity_t *blue = G_FindTeamFlag(TEAM_BLUE);
    gentity_t *ent;
    int i;

    if (red) {
        VectorCopy(red->s.origin, level.itemTimerRedBase);
    }
    if (blue) {
        VectorCopy(blue->s.origin, level.itemTimerBlueBase);
    }
    memset(level.itemTimerCounts, 0, sizeof(level.itemTimerCounts));
    for (i = 0; i < level.num_entities; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || !ent->item) {
            continue;
        }
        if ((ent->item->giType == IT_ARMOR && ent->item->giTag == 3) || ent->item->giType == IT_HOLDABLE) {
            continue;
        }
        if (!ent->item->itemTimer) {
            continue;
        }
        level.itemTimerCounts[ent->item - bg_itemlist]++;
    }
}

/*
===============
RespawnItem
===============
*/
void RespawnItem(gentity_t* ent) {
    if (!ent) {
        return;
    }

    // randomly select from teamed entities
    if (ent->team) {
        gentity_t* master;
        int count;
        int choice;

        if (!ent->teammaster) {
            G_Error("RespawnItem: bad teammaster");
        }
        master = ent->teammaster;

        for (count = 0, ent = master; ent; ent = ent->teamchain, count++)
            ;

        choice = rand() % count;

        for (count = 0, ent = master; ent && count < choice; ent = ent->teamchain, count++)
            ;
    }

    if (!ent) {
        return;
    }

    ent->r.contents = CONTENTS_TRIGGER;
    ent->s.eFlags &= ~EF_NODRAW;
    ent->r.svFlags &= ~SVF_NOCLIENT;

    // [QL] Clear item timer fields on respawn
    ent->s.time = 0;
    ent->s.time2 = 0;

    trap_LinkEntity(ent);

    if (ent->item->giType == IT_POWERUP) {
        // play powerup spawn sound to all clients
        gentity_t* te;

        // if the powerup respawn sound should Not be global
        if (ent->speed) {
            te = G_TempEntity(ent->s.pos.trBase, EV_GENERAL_SOUND);
        } else {
            te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_SOUND);
        }
        te->s.eventParm = G_SoundIndex("sound/items/poweruprespawn.wav");
        te->r.svFlags |= SVF_BROADCAST;
    }

    if (ent->item->giType == IT_HOLDABLE && ent->item->giTag == HI_KAMIKAZE) {
        // play powerup spawn sound to all clients
        gentity_t* te;

        // if the powerup respawn sound should Not be global
        if (ent->speed) {
            te = G_TempEntity(ent->s.pos.trBase, EV_GENERAL_SOUND);
        } else {
            te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_SOUND);
        }
        te->s.eventParm = G_SoundIndex("sound/items/kamikazerespawn.wav");
        te->r.svFlags |= SVF_BROADCAST;
    }

    // play the normal respawn sound only to nearby clients
    G_AddEvent(ent, EV_ITEM_RESPAWN, 0);

    // [QL] mark when the item became available again, for time-held item stats
    ent->spawnTime = level.time;

    ent->nextthink = 0;

    // [QL] Notify spectators that this item has respawned
    Item_NotifySpectators(ent, qtrue);
}

/*
===============
G_RespawnKey
[QL] When a player dies or disconnects, respawn any keys they were holding
===============
*/
void G_RespawnKey(int keyTag) {
    const char* classname;
    gentity_t* ent;

    switch (keyTag) {
        case KEY_SILVER: classname = "item_key_silver"; break;
        case KEY_GOLD:   classname = "item_key_gold";   break;
        case KEY_MASTER: classname = "item_key_master"; break;
        default: return;
    }

    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), classname)) != NULL) {
        // [QL] fixed (map-placed) keys respawn in place; dropped keys are removed.
        // Binary distinguishes on FL_DROPPED_ITEM (0x1004d61c) and inlines G_FreeEntity.
        if (!(ent->flags & FL_DROPPED_ITEM)) {
            RespawnItem(ent);
        } else {
            G_FreeEntity(ent);
        }
    }
}

/*
===============
Touch_Item
===============
*/
void Touch_Item(gentity_t* ent, gentity_t* other, trace_t* trace) {
    int respawn;
    qboolean predict;

    if (!other->client)
        return;
    if (other->health < 1)
        return;  // dead people can't pickup

    // the same pickup rules are used for client side and server side
    if (!BG_CanItemBeGrabbed(level.time, level.warmupTime, armor_tiered.integer,
                             g_weaponRespawn.integer, g_gametype.integer,
                             &ent->s, &other->client->ps)) {
        return;
    }

    // [QL] 0x1004f06c: nothing is taken in the countdown of a match, nor in that of a round but
    // in Freeze Tag without g_freezeRoundDelay; nor by a player kept from items, and no powerup
    // by one kept from powerups
    if (level.warmupTime > 0) {
        return;
    }
    if (level.roundState.eCurrent == RS_COUNTDOWN &&
        (g_gametype.integer != GT_FREEZE || g_freezeRoundDelay.integer)) {
        return;
    }
    if (other->flags & FL_NOPICKUP) {
        return;
    }
    if ((other->flags & FL_NOPOWERUP) && ent->item->giType == IT_POWERUP) {
        return;
    }

    G_LogPrintf("Item: %i %s\n", other->s.number, ent->item->classname);

    predict = other->client->pers.predictItemPickup;

    // call the item-specific pickup function
    switch (ent->item->giType) {
        case IT_WEAPON:
            respawn = Pickup_Weapon(ent, other);
            //		predict = qfalse;
            break;
        case IT_AMMO:
            respawn = Pickup_Ammo(ent, other);
            //		predict = qfalse;
            break;
        case IT_ARMOR:
            respawn = Pickup_Armor(ent, other);
            break;
        case IT_HEALTH:
            respawn = Pickup_Health(ent, other);
            break;
        case IT_POWERUP:
            respawn = Pickup_Powerup(ent, other);
            predict = qfalse;
            break;
        case IT_PERSISTANT_POWERUP:
            respawn = Pickup_PersistantPowerup(ent, other);
            break;
        case IT_TEAM:
            respawn = Pickup_Team(ent, other);
            break;
        case IT_HOLDABLE:
            respawn = Pickup_Holdable(ent, other);
            break;
        case IT_KEY:
            // [QL] keys are stored as bitmask in stats[STAT_KEY]
            if (ent->item->giTag) {
                other->client->ps.stats[STAT_KEY] |= ent->item->giTag;
            }
            respawn = -1;  // keys don't auto-respawn
            break;
        default:
            return;
    }

    // [QL] count every touch that reached a pickup handler; drives the
    // "first pickup" and time-held item stats
    ent->pickupCount++;

    // dropped items will not respawn
    if (ent->flags & FL_DROPPED_ITEM) {
        ent->freeAfterEvent = qtrue;
    }

    if (!respawn) {
        return;
    }

    // play the normal pickup sound
    if (predict) {
        G_AddPredictableEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
    } else {
        G_AddEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
    }

    // [QL] 0x1004f1f5: taking the quad makes you the hog, and the marker goes up at once
    if (g_gametype.integer == GT_FFA && g_quadHog.integer && other->client->ps.powerups[PW_QUAD]) {
        G_PointOfInterest(other, g_quadHogPingRate.integer, other->client->ps.clientNum);
    }
    // [QL] 0x1004f24e: same for picking the neutral flag up
    if (g_gametype.integer == GT_1FCTF && other->client->ps.powerups[PW_NEUTRALFLAG]) {
        G_PointOfInterest(other, g_neutralFlagPingRate.integer,
                          other->client->ps.persistant[PERS_TEAM]);
    }

    // powerup pickups are global broadcasts
    if (ent->item->giType == IT_POWERUP || ent->item->giType == IT_TEAM) {
        // if we want the global sound to play
        if (!ent->speed) {
            gentity_t* te;

            te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP);
            te->s.eventParm = ent->s.modelindex;
            te->s.otherEntityNum = other->s.number;  // [QL] 0x1004f2f2: who took it
            te->r.svFlags |= SVF_BROADCAST;
        } else {
            gentity_t* te;

            te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP);
            te->s.eventParm = ent->s.modelindex;
            te->s.otherEntityNum = other->s.number;
            // only send this temp entity to a single client
            te->r.svFlags |= SVF_SINGLECLIENT;
            te->r.singleClient = other->s.number;
        }
    }

    // fire item targets
    G_UseTargets(ent, other);

    // wait of -1 will not respawn
    if (ent->wait == -1) {
        ent->r.svFlags |= SVF_NOCLIENT;
        ent->s.eFlags |= EF_NODRAW;
        ent->r.contents = 0;
        ent->unlinkAfterEvent = qtrue;
        return;
    }

    // non zero wait overrides respawn time
    if (ent->wait) {
        respawn = ent->wait;
    }

    // random can be used to vary the respawn time
    if (ent->random) {
        respawn += crandom() * ent->random;
        if (respawn < 1) {
            respawn = 1;
        }
    }

    // [QL] 0x1004f33c: in the gauntlet-only training the items take bot_itemDelayTime longer
    if (bot_gauntletOnly.integer) {
        respawn += bot_itemDelayTime.integer;
    }

    // picked up items still stay around, they just don't
    // draw anything.  This allows respawnable items
    // to be placed on movers.
    ent->r.svFlags |= SVF_NOCLIENT;
    ent->s.eFlags |= EF_NODRAW;
    ent->r.contents = 0;

    // ZOID
    // A negative respawn times means to never respawn this item (but don't
    // delete it).  This is used by items that are respawned by third party
    // events such as ctf flags
    if (respawn <= 0) {
        ent->nextthink = 0;
        ent->think = 0;
    } else {
        // [QL] Item timer: broadcast respawn time to spectators/clients. Gated on the
        // per-item itemTimer field (binary item+0x3c) and g_itemTimers, and marks the
        // entity FL_ITEM_TIMER. Binary: Touch_Item @0x1004f090.
        if (g_itemTimers.integer && ent->item && ent->item->itemTimer) {
            ent->s.time2 = respawn * 1000;
            ent->s.time = level.time + respawn * 1000;
            ent->r.svFlags &= ~SVF_NOCLIENT;
            ent->s.generic1 = (ent->team != NULL) ? 1 : 0;
            ent->flags |= FL_ITEM_TIMER;
        }
        ent->nextthink = level.time + respawn * 1000;
        ent->think = RespawnItem;
    }
    // [QL] 0x1004f4a3: spectators get the item's new timer
    Item_NotifySpectators(ent, qfalse);
    trap_LinkEntity(ent);
}

//======================================================================

/*
================
LaunchItem

Spawns an item and tosses it forward
================
*/
gentity_t* LaunchItem(gitem_t* item, vec3_t origin, vec3_t velocity) {
    gentity_t* dropped;
    qboolean flag = item->giType == IT_TEAM && item->giTag >= PW_REDFLAG && item->giTag <= PW_NEUTRALFLAG;

    dropped = G_Spawn();

    dropped->s.eType = ET_ITEM;
    dropped->s.modelindex = item - bg_itemlist;  // store item number in modelindex
    dropped->s.modelindex2 = 1;                  // This is non-zero is it's a dropped item

    dropped->classname = item->classname;
    dropped->item = item;
    // [QL] 0x1004f5bd: a flag stands 35 high
    VectorSet(dropped->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
    VectorSet(dropped->r.maxs, ITEM_RADIUS, ITEM_RADIUS, flag ? 35 : ITEM_RADIUS);
    dropped->r.contents = CONTENTS_TRIGGER;

    dropped->touch = Touch_Item;

    G_SetOrigin(dropped, origin);
    dropped->s.pos.trType = TR_GRAVITY;
    dropped->s.pos.trTime = level.time;
    VectorCopy(velocity, dropped->s.pos.trDelta);

    dropped->s.eFlags |= EF_BOUNCE_HALF;
    dropped->flags = FL_DROPPED_ITEM;

    // [QL] 0x1004f63f: a flag waits to go home, a rune goes back to a spawn after 30 seconds, and
    // the rest are gone in 30, or in 10 when the health bleeds out of the wounded
    if ((g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF) && item->giType == IT_TEAM) {
        dropped->think = Team_DroppedFlagThink;
        dropped->nextthink = level.time + 30000;
        if (flag) {
            Team_CheckDroppedItem(dropped);
        }
    } else if (g_gametype.integer == GT_AD && item->giType == IT_TEAM) {
        dropped->think = Team_DroppedFlagThink;
        if (flag) {
            Team_CheckDroppedItem(dropped);
        }
    } else if (item->giType == IT_PERSISTANT_POWERUP) {
        dropped->think = G_RuneRespawn;
        dropped->nextthink = level.time + 30000;
    } else {
        dropped->think = G_FreeEntity;
        dropped->nextthink = level.time + (g_dropDamagedHealth.integer ? 10000 : 30000);
    }

    trap_LinkEntity(dropped);

    return dropped;
}

/*
================
Drop_Item

Spawns an item and tosses it forward

[QL] 0x1004f740: the quad of an FFA with g_quadHog goes out as the Quad Hog's.
================
*/
gentity_t* Drop_Item(gentity_t* ent, gitem_t* item, float angle) {
    vec3_t velocity;
    vec3_t angles;

    VectorCopy(ent->s.apos.trBase, angles);
    angles[YAW] += angle;
    angles[PITCH] = 0;  // always forward

    AngleVectors(angles, velocity, NULL, NULL);
    VectorScale(velocity, 150, velocity);
    velocity[2] += 200 + crandom() * 50;

    if (g_gametype.integer == GT_FFA && g_quadHog.integer && item->giType == IT_POWERUP &&
        item->giTag == PW_QUAD) {
        return QuadHog_Launch(item, ent->s.pos.trBase, velocity);
    }
    return LaunchItem(item, ent->s.pos.trBase, velocity);
}

/*
================
Use_Item

Respawn the item
================
*/
void Use_Item(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    RespawnItem(ent);
}

//======================================================================

/*
================
FinishSpawningItem

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
void FinishSpawningItem(gentity_t* ent) {
    trace_t tr;
    vec3_t dest;

    VectorSet(ent->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
    VectorSet(ent->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);

    ent->s.eType = ET_ITEM;
    ent->s.modelindex = ent->item - bg_itemlist;  // store item number in modelindex
    ent->s.modelindex2 = 0;                       // zero indicates this isn't a dropped item

    ent->r.contents = CONTENTS_TRIGGER;
    ent->touch = Touch_Item;
    // using an item causes it to respawn
    ent->use = Use_Item;

    if (ent->spawnflags & 1) {
        // suspended
        G_SetOrigin(ent, ent->s.origin);
    } else {
        // drop to floor
        VectorSet(dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096);
        trap_Trace(&tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID);
        if (tr.startsolid) {
            G_Printf("FinishSpawningItem: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
            G_FreeEntity(ent);
            return;
        }

        // allow to ride movers
        ent->s.groundEntityNum = tr.entityNum;

        G_SetOrigin(ent, tr.endpos);
    }

    // team slaves and targeted items aren't present at start
    if ((ent->flags & FL_TEAMSLAVE) || ent->targetname) {
        ent->s.eFlags |= EF_NODRAW;
        ent->r.contents = 0;
        return;
    }

    // powerups don't spawn in for a while
    if (ent->item->giType == IT_POWERUP) {
        float respawn;

        respawn = 45 + crandom() * 15;
        ent->s.eFlags |= EF_NODRAW;
        ent->r.contents = 0;
        ent->nextthink = level.time + respawn * 1000;
        ent->think = RespawnItem;
        return;
    }

    trap_LinkEntity(ent);
}

qboolean itemRegistered[MAX_ITEMS];

/*
==================
G_CheckTeamItems
==================
*/
void G_CheckTeamItems(void) {
    // Set up team stuff
    Team_InitGame();

    // Gametype-specific entity checks (implementations in g_gametype_*.c)
    switch (g_gametype.integer) {
        case GT_CTF:        CTF_CheckTeamItems(); break;
        case GT_1FCTF:      OneFCTF_CheckTeamItems(); break;
        case GT_OBELISK:    Obelisk_CheckTeamItems(); break;
        case GT_HARVESTER:  Harvester_CheckTeamItems(); break;
        default: break;
    }
}

/*
==============
ClearRegisteredItems
==============
*/
void ClearRegisteredItems(void) {
    memset(itemRegistered, 0, sizeof(itemRegistered));

    // [QL] 0x1004ffd0: the skulls of harvester, the quad of the Quad Hog, the runes (also on
    // a com_build server) and the healths the wounded bleed; ClientSpawn registers the weapons
    if (g_gametype.integer == GT_HARVESTER) {
        Harvester_RegisterItems();
    }
    if (g_gametype.integer == GT_FFA && g_quadHog.integer) {
        RegisterItem(BG_FindItemForPowerup(PW_QUAD));
    }
    if (g_runes.integer || trap_Cvar_VariableValue("com_build") != 0) {
        RegisterItem(BG_FindItemForPowerup(PW_AMMOREGEN));
        RegisterItem(BG_FindItemForPowerup(PW_DOUBLER));
        RegisterItem(BG_FindItemForPowerup(PW_GUARD));
        RegisterItem(BG_FindItemForPowerup(PW_SCOUT));
    }
    if (g_dropDamagedHealth.integer) {
        RegisterItem(BG_FindItem("5 Health"));
        RegisterItem(BG_FindItem("25 Health"));
    }
}

/*
===============
RegisterItem

The item will be added to the precache list
===============
*/
void RegisterItem(gitem_t* item) {
    if (!item) {
        G_Error("RegisterItem: NULL");
    }
    itemRegistered[item - bg_itemlist] = qtrue;
}

/*
===============
SaveRegisteredItems

Write the needed items to a config string
so the client will know which ones to precache
===============
*/
void SaveRegisteredItems(void) {
    char string[MAX_ITEMS + 1];
    int i;
    int count;

    count = 0;
    for (i = 0; i < bg_numItems; i++) {
        if (itemRegistered[i]) {
            count++;
            string[i] = '1';
        } else {
            string[i] = '0';
        }
    }
    string[bg_numItems] = 0;

    G_Printf("%i items registered\n", count);
    trap_SetConfigstring(CS_ITEMS, string);
}

/*
============
G_ItemDisabled
============
*/
int G_ItemDisabled(gitem_t* item) {
    char name[128];

    Com_sprintf(name, sizeof(name), "disable_%s", item->classname);
    // [QL] any non-zero value, fractions included (0x100508e3)
    return trap_Cvar_VariableValue(name) != 0;
}

/*
============
G_FindAmmoPack

[QL] The ammo pack is the ammo item tagged WP_NUM_WEAPONS (qagamex86.dll 0x1002cb10).
============
*/
static gitem_t* G_FindAmmoPack(void) {
    gitem_t* it;

    for (it = bg_itemlist + 1; it->classname; it++) {
        if (it->giType == IT_AMMO && it->giTag == WP_NUM_WEAPONS) {
            return it;
        }
    }
    Com_Error(ERR_DROP, "Couldn't find item for type %i tag %i", IT_AMMO, WP_NUM_WEAPONS);
    return NULL;
}

/*
============
G_ItemFiltered

[QL] Whether a map item stays out of the game (0x10050843). Instagib keeps only the
flags; flags spawn in CTF, 1-Flag, Overload and Attack & Defend, which keeps nothing
else; race, CA, Domination and Red Rover spawn no items at all. Then g_spawnItem* by
type, g_infiniteAmmo and the ammo pack cvars, g_quadHog for FFA powerups, and
disable_<classname>. Persistant powerups never spawn from the map.
============
*/
static qboolean G_ItemFiltered(gentity_t* ent, gitem_t* item) {
    int gt = g_gametype.integer;
    qboolean flag = item->giType == IT_TEAM && item->giTag >= PW_REDFLAG && item->giTag <= PW_NEUTRALFLAG;

    if (g_instaGib.integer && !flag) {
        return qtrue;
    }
    if (flag && (gt == GT_FFA || gt == GT_DUEL || gt == GT_TEAM || gt == GT_CA || gt == GT_HARVESTER ||
                 gt == GT_FREEZE || gt == GT_RR)) {
        return qtrue;
    }
    if (gt == GT_AD && !(item->giType == IT_TEAM && (item->giTag == PW_REDFLAG || item->giTag == PW_BLUEFLAG))) {
        return qtrue;
    }
    if (gt == GT_RACE || gt == GT_CA || gt == GT_DOMINATION || gt == GT_RR) {
        return qtrue;
    }

    switch (item->giType) {
        case IT_WEAPON:
            if (!g_spawnItemWeapons.integer) {
                return qtrue;
            }
            break;
        case IT_AMMO:
            if (!g_spawnItemAmmo.integer || g_infiniteAmmo.integer) {
                return qtrue;
            }
            // with g_ammoPackHack every ammo item gets through and G_SpawnItem swaps it for the pack
            if (!g_ammoPackHack.integer &&
                (g_ammoPack.integer ? item->giTag != WP_NUM_WEAPONS : item->giTag == WP_NUM_WEAPONS)) {
                return qtrue;
            }
            break;
        case IT_ARMOR:
            if (!g_spawnItemArmor.integer) {
                return qtrue;
            }
            break;
        case IT_HEALTH:
            if (!g_spawnItemHealth.integer) {
                return qtrue;
            }
            break;
        case IT_POWERUP:
            if (!g_spawnItemPowerup.integer || (gt == GT_FFA && g_quadHog.integer)) {
                return qtrue;
            }
            break;
        case IT_HOLDABLE:
            if (!g_spawnItemHoldable.integer) {
                return qtrue;
            }
            break;
        case IT_PERSISTANT_POWERUP:
            return qtrue;
        case IT_TEAM:
            if (flag && !ent->s.modelindex2) {
                if (item->giTag == PW_REDFLAG) {
                    trap_SetConfigstring(CS_REDTEAMBASE, va("\\x\\%f\\y\\%f\\z\\%f", ent->s.pos.trBase[0],
                                                            ent->s.pos.trBase[1], ent->s.pos.trBase[2]));
                } else if (item->giTag == PW_BLUEFLAG) {
                    trap_SetConfigstring(CS_BLUETEAMBASE, va("\\x\\%f\\y\\%f\\z\\%f", ent->s.pos.trBase[0],
                                                             ent->s.pos.trBase[1], ent->s.pos.trBase[2]));
                }
            }
            break;
        default:
            break;
    }

    return G_ItemDisabled(item);
}

/*
============
G_SpawnItem
Address: 0x100507e0

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.

[QL] give forces the item past G_ItemFiltered, and so does, in race, a
"gametype" key that names race.
============
*/
void G_SpawnItem(gentity_t* ent, gitem_t* item, qboolean forced) {
    char* value;

    if (!forced && !(g_gametype.integer == GT_RACE && G_SpawnString("gametype", "", &value) &&
                     G_GametypeInList(value))) {
        if (G_ItemFiltered(ent, item)) {
            return;
        }
    }

    G_SpawnFloat("random", "0", &ent->random);
    G_SpawnFloat("wait", "0", &ent->wait);

    // [QL] g_ammoPackHack puts an ammo pack where every ammo item was, and drops the map's own
    // packs (0x10050a95)
    if (g_ammoPackHack.integer && item->giType == IT_AMMO) {
        gitem_t* pack = G_FindAmmoPack();

        if (item->giTag == WP_NUM_WEAPONS) {
            return;
        }
        RegisterItem(pack);
        ent->item = pack;
    } else {
        RegisterItem(item);
        ent->item = item;
    }

    // some movers spawn on the second frame, so delay item
    // spawns until the third frame so they can ride trains
    ent->nextthink = level.time + 2 * (int)(1000.0 / sv_fps.integer);  // [QL] two server frames
    ent->think = FinishSpawningItem;

    ent->physicsBounce = 0.50;  // items are bouncy

    if (item->giType == IT_POWERUP) {
        G_SoundIndex("sound/items/poweruprespawn.wav");
        G_SpawnFloat("noglobalsound", "0", &ent->speed);
    }

    if (item->giType == IT_PERSISTANT_POWERUP) {
        ent->s.generic1 = ent->spawnflags;
    }

    // [QL] spectators get the item's timer (0x10050ba1)
    Item_NotifySpectators(ent, qfalse);
}

/*
================
G_BounceItem

================
*/
void G_BounceItem(gentity_t* ent, trace_t* trace) {
    vec3_t velocity;
    float dot;
    int hitTime;

    // reflect the velocity on the trace plane
    hitTime = ( level.time - level.frametime ) + (level.time - ( level.time - level.frametime )) * trace->fraction;
    BG_EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity);
    dot = DotProduct(velocity, trace->plane.normal);
    VectorMA(velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta);

    // cut the velocity to keep from bouncing forever
    VectorScale(ent->s.pos.trDelta, ent->physicsBounce, ent->s.pos.trDelta);

    // check for stop
    if (trace->plane.normal[2] > 0 && ent->s.pos.trDelta[2] < 40) {
        trace->endpos[2] += 1.0;  // make sure it is off ground
        SnapVector(trace->endpos);
        G_SetOrigin(ent, trace->endpos);
        ent->s.groundEntityNum = trace->entityNum;
        return;
    }

    VectorAdd(ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
    VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
    ent->s.pos.trTime = level.time;
}

/*
================
G_RunItem

================
*/
void G_RunItem(gentity_t* ent) {
    vec3_t origin;
    trace_t tr;
    int contents;
    int mask;

    // if its groundentity has been set to none, it may have been pushed off an edge
    if (ent->s.groundEntityNum == ENTITYNUM_NONE) {
        if (ent->s.pos.trType != TR_GRAVITY) {
            ent->s.pos.trType = TR_GRAVITY;
            ent->s.pos.trTime = level.time;
        }
    }

    if (ent->s.pos.trType == TR_STATIONARY) {
        // check think function
        G_RunThink(ent);
        return;
    }

    // get current position
    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);

    // trace a line from the previous position to the current position
    if (ent->clipmask) {
        mask = ent->clipmask;
    } else {
        mask = MASK_PLAYERSOLID & ~CONTENTS_BODY;  // MASK_SOLID;
    }
    trap_Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin,
               ent->r.ownerNum, mask);

    VectorCopy(tr.endpos, ent->r.currentOrigin);

    if (tr.startsolid) {
        tr.fraction = 0;
    }

    trap_LinkEntity(ent);  // FIXME: avoid this for stationary?

    // check think function
    G_RunThink(ent);

    if (tr.fraction == 1) {
        return;
    }

    // if it is in a nodrop volume, remove it
    contents = trap_PointContents(ent->r.currentOrigin, -1);
    if (contents & CONTENTS_NODROP) {
        if (ent->item && ent->item->giType == IT_TEAM) {
            Team_FreeEntity(ent);
        } else {
            G_FreeEntity(ent);
        }
        return;
    }

    G_BounceItem(ent, &tr);
}
