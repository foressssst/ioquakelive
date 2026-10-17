/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

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

const minqlxHooks_t* g_minqlxHooks;

static const int mqPowerups[MINQLX_POWERUPS] = {PW_QUAD, PW_BATTLESUIT, PW_HASTE, PW_INVIS, PW_REGEN,
                                                PW_INVULNERABILITY};

static gclient_t* MQ_Client(int clientNum) {
    if (clientNum < 0 || clientNum >= level.maxclients) {
        return NULL;
    }
    return g_entities[clientNum].client;
}

static qboolean MQ_PlayerInfo(int clientNum, char* name, int nameSize, int* team, int* privileges) {
    gclient_t* client = MQ_Client(clientNum);

    name[0] = '\0';
    *team = TEAM_SPECTATOR;
    *privileges = -1;
    if (!client) {
        return qfalse;
    }
    if (client->pers.connected != CON_DISCONNECTED) {
        Q_strncpyz(name, client->pers.netname, nameSize);
        *team = client->sess.sessionTeam;
    }
    *privileges = client->sess.privileges;
    return qtrue;
}

static qboolean MQ_PlayerState(int clientNum, minqlxPlayerState_t* state) {
    gclient_t* client = MQ_Client(clientNum);
    int i;

    if (!client) {
        return qfalse;
    }
    state->alive = client->ps.pm_type == PM_NORMAL;
    state->frozen = client->ps.pm_type == PM_FREEZE;
    state->noclip = client->noclip;
    VectorCopy(client->ps.origin, state->origin);
    VectorCopy(client->ps.velocity, state->velocity);
    state->health = g_entities[clientNum].health;
    state->armor = client->ps.stats[STAT_ARMOR];
    state->weapon = client->ps.weapon;
    state->weapons = client->ps.stats[STAT_WEAPONS];
    for (i = 0; i < MINQLX_WEAPONS; i++) {
        state->ammo[i] = client->ps.ammo[i + 1];
    }
    for (i = 0; i < MINQLX_POWERUPS; i++) {
        int end = client->ps.powerups[mqPowerups[i]];

        state->powerups[i] = end ? end - level.time : 0;
    }
    state->holdable = client->ps.stats[STAT_HOLDABLE_ITEM];
    state->flight[0] = client->ps.stats[STAT_CUR_FLIGHT_FUEL];
    state->flight[1] = client->ps.stats[STAT_MAX_FLIGHT_FUEL];
    state->flight[2] = client->ps.stats[STAT_FLIGHT_THRUST];
    state->flight[3] = client->ps.stats[STAT_FLIGHT_REFUEL];
    return qtrue;
}

static qboolean MQ_PlayerStats(int clientNum, minqlxPlayerStats_t* stats) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    stats->score = client->sess.sessionTeam == TEAM_SPECTATOR ? 0 : client->ps.persistant[PERS_SCORE];
    stats->kills = client->expandedStats.numKills;
    stats->deaths = client->expandedStats.numDeaths;
    stats->damageDealt = client->expandedStats.totalDamageDealt;
    stats->damageTaken = client->expandedStats.totalDamageTaken;
    stats->time = level.time - client->pers.enterTime;
    stats->ping = client->ps.ping;
    return qtrue;
}

static qboolean MQ_SetPosition(int clientNum, const float* origin) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    VectorCopy(origin, client->ps.origin);
    return qtrue;
}

static qboolean MQ_SetVelocity(int clientNum, const float* velocity) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    VectorCopy(velocity, client->ps.velocity);
    return qtrue;
}

static int MQ_NoClip(int clientNum, qboolean activate) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client || !client->noclip == !activate) {
        return qfalse;
    }
    client->noclip = activate ? qtrue : qfalse;
    return qtrue;
}

static qboolean MQ_SetHealth(int clientNum, int health) {
    if (!MQ_Client(clientNum)) {
        return qfalse;
    }
    g_entities[clientNum].health = health;
    return qtrue;
}

static qboolean MQ_SetArmor(int clientNum, int armor) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->ps.stats[STAT_ARMOR] = armor;
    return qtrue;
}

static qboolean MQ_SetWeapons(int clientNum, int weapons) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->ps.stats[STAT_WEAPONS] = weapons;
    return qtrue;
}

static qboolean MQ_SetWeapon(int clientNum, int weapon) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->ps.weapon = weapon;
    return qtrue;
}

static qboolean MQ_SetAmmo(int clientNum, const int* ammo) {
    gclient_t* client = MQ_Client(clientNum);
    int i;

    if (!client) {
        return qfalse;
    }
    for (i = 0; i < MINQLX_WEAPONS; i++) {
        client->ps.ammo[i + 1] = ammo[i];
    }
    return qtrue;
}

static qboolean MQ_SetPowerups(int clientNum, const int* powerups) {
    gclient_t* client = MQ_Client(clientNum);
    int i;

    if (!client) {
        return qfalse;
    }
    for (i = 0; i < MINQLX_POWERUPS; i++) {
        client->ps.powerups[mqPowerups[i]] = powerups[i] ? level.time - level.time % 1000 + powerups[i] : 0;
    }
    return qtrue;
}

static qboolean MQ_IsKamikaze(int item) {
    return item > 0 && item < bg_numItems && bg_itemlist[item].giType == IT_HOLDABLE &&
           bg_itemlist[item].giTag == HI_KAMIKAZE;
}

static qboolean MQ_SetHoldable(int clientNum, int holdable) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client || holdable < 0 || holdable >= bg_numItems) {
        return qfalse;
    }
    if (MQ_IsKamikaze(holdable)) {
        client->ps.eFlags |= EF_KAMIKAZE;
    } else {
        client->ps.eFlags &= ~EF_KAMIKAZE;
    }
    client->ps.stats[STAT_HOLDABLE_ITEM] = holdable;
    return qtrue;
}

static void MQ_DroppedHoldableTouch(gentity_t* ent, gentity_t* other, trace_t* trace) {
    if (ent->parent == other) {
        return;
    }
    Touch_Item(ent, other, trace);
}

static void MQ_DroppedHoldableThink(gentity_t* ent) {
    ent->touch = Touch_Item;
    ent->think = G_FreeEntity;
    ent->nextthink = level.time + 29000;
}

static qboolean MQ_DropHoldable(int clientNum) {
    gclient_t* client = MQ_Client(clientNum);
    gentity_t* ent = &g_entities[clientNum];
    gentity_t* drop;
    vec3_t velocity;
    float angle;
    int item;

    if (!client) {
        return qfalse;
    }
    client->ps.eFlags &= ~EF_KAMIKAZE;
    item = client->ps.stats[STAT_HOLDABLE_ITEM];
    if (item <= 0 || item >= bg_numItems) {
        return qfalse;
    }
    angle = DEG2RAD(ent->s.apos.trBase[YAW]);
    velocity[0] = 150.0f * cos(angle);
    velocity[1] = 150.0f * sin(angle);
    velocity[2] = 250.0f;
    drop = LaunchItem(bg_itemlist + item, ent->s.pos.trBase, velocity);
    drop->touch = MQ_DroppedHoldableTouch;
    drop->parent = ent;
    drop->think = MQ_DroppedHoldableThink;
    drop->nextthink = level.time + 1000;
    drop->s.pos.trTime = level.time - 500;
    client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
    return qtrue;
}

static qboolean MQ_SetFlight(int clientNum, const int* flight) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->ps.stats[STAT_CUR_FLIGHT_FUEL] = flight[0];
    client->ps.stats[STAT_MAX_FLIGHT_FUEL] = flight[1];
    client->ps.stats[STAT_FLIGHT_THRUST] = flight[2];
    client->ps.stats[STAT_FLIGHT_REFUEL] = flight[3];
    return qtrue;
}

static qboolean MQ_SetInvulnerability(int clientNum, int time) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->invulnerabilityTime = level.time + time;
    return qtrue;
}

static qboolean MQ_SetScore(int clientNum, int score) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->ps.persistant[PERS_SCORE] = score;
    return qtrue;
}

static qboolean MQ_SetPrivileges(int clientNum, int privileges) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->sess.privileges = privileges;
    return qtrue;
}

static qboolean MQ_PlayerSpawn(int clientNum) {
    gclient_t* client = MQ_Client(clientNum);

    if (!client) {
        return qfalse;
    }
    client->ps.pm_type = PM_NORMAL;
    ClientSpawn(&g_entities[clientNum]);
    return qtrue;
}

static qboolean MQ_ForceVote(qboolean pass) {
    int i;

    if (!level.voteTime) {
        return qfalse;
    }
    if (pass) {
        for (i = 0; i < level.maxclients; i++) {
            if (level.clients[i].pers.connected == CON_CONNECTED) {
                level.clients[i].pers.voteState = VOTE_YES;
            }
        }
    } else {
        level.voteTime -= 30000;
    }
    return qtrue;
}

static void MQ_Callvote(const char* vote, const char* display, int seconds) {
    int i;

    Q_strncpyz(level.voteString, vote, sizeof(level.voteString));
    Q_strncpyz(level.voteDisplayString, display, sizeof(level.voteDisplayString));
    level.voteTime = level.time - 30000 + seconds * 1000;
    level.voteYes = 0;
    level.voteNo = 0;
    for (i = 0; i < level.maxclients; i++) {
        level.clients[i].pers.voteState = VOTE_PENDING;
    }
    trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
    trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
    trap_SetConfigstring(CS_VOTE_YES, "0");
    trap_SetConfigstring(CS_VOTE_NO, "0");
}

static void MQ_AllowSinglePlayer(qboolean allow) {
    level.mapIsTrainingMap = allow ? qtrue : qfalse;
}

static void MQ_DestroyKamikazeTimers(void) {
    gentity_t* ent;
    int i;

    for (i = 0, ent = g_entities; i < MAX_GENTITIES; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (ent->client && ent->health <= 0) {
            ent->client->ps.eFlags &= ~EF_KAMIKAZE;
        }
        if (ent->classname && !strcmp(ent->classname, "kamikaze timer")) {
            G_FreeEntity(ent);
        }
    }
}

static int MQ_NumItems(void) {
    return bg_numItems;
}

static void MQ_SpawnItem(int item, int x, int y, int z) {
    vec3_t origin, velocity;
    gentity_t* ent;

    VectorSet(origin, x, y, z);
    VectorClear(velocity);
    ent = LaunchItem(bg_itemlist + item, origin, velocity);
    ent->nextthink = 0;
    ent->think = NULL;
    G_AddEvent(ent, EV_ITEM_RESPAWN, 0);
}

static void MQ_RemoveDroppedItems(void) {
    gentity_t* ent;
    int i;

    for (i = 0, ent = g_entities; i < MAX_GENTITIES; i++, ent++) {
        if (ent->inuse && (ent->flags & FL_DROPPED_ITEM)) {
            G_FreeEntity(ent);
        }
    }
}

static qboolean MQ_SlayWithMod(int clientNum, int mod) {
    gclient_t* client = MQ_Client(clientNum);
    gentity_t* ent = &g_entities[clientNum];
    int damage;

    if (!client) {
        return qfalse;
    }
    if (ent->health <= 0) {
        return qtrue;
    }
    damage = ent->health + (mod == MOD_KAMIKAZE ? 100000 : 0);
    client->ps.stats[STAT_ARMOR] = 0;
    G_Damage(ent, ent, ent, NULL, NULL, damage * 2, DAMAGE_NO_PROTECTION, mod);
    return qtrue;
}

static void MQ_ReplaceItem(gentity_t* ent, int item) {
    char items[MAX_STRING_CHARS];

    if (!item) {
        G_FreeEntity(ent);
        return;
    }
    ent->s.modelindex = item;
    ent->classname = bg_itemlist[item].classname;
    ent->item = &bg_itemlist[item];
    trap_GetConfigstring(CS_ITEMS, items, sizeof(items));
    if (item < (int)sizeof(items) - 1) {
        if ((int)strlen(items) <= item) {
            memset(items + strlen(items), '0', item + 1 - strlen(items));
            items[item + 1] = '\0';
        }
        items[item] = '1';
    }
    trap_SetConfigstring(CS_ITEMS, items);
}

static int MQ_ReplaceItems(int entityNum, const char* entityClassname, int item, const char* itemClassname,
                           char* error, int errorSize) {
    gentity_t* ent;
    qboolean found = qfalse;
    int i;

    if (itemClassname) {
        item = 0;
        for (i = 1; i < bg_numItems; i++) {
            if (!strcmp(bg_itemlist[i].classname, itemClassname)) {
                item = i;
                break;
            }
        }
        if (!item) {
            Com_sprintf(error, errorSize, "invalid item classname: %s.", itemClassname);
            return -1;
        }
    } else if (item < 0 || item >= bg_numItems) {
        Com_sprintf(error, errorSize, "item_id needs to be between 0 and %d.", bg_numItems - 1);
        return -1;
    }

    if (!entityClassname) {
        if (entityNum < 0 || entityNum >= MAX_GENTITIES) {
            Com_sprintf(error, errorSize, "entity_id needs to be between 0 and %d.", MAX_GENTITIES - 1);
            return -1;
        }
        ent = &g_entities[entityNum];
        if (!ent->inuse) {
            Com_sprintf(error, errorSize, "entity #%d is not in use.", entityNum);
            return -1;
        }
        if (ent->s.eType != ET_ITEM) {
            Com_sprintf(error, errorSize, "entity #%d is not item. Cannot replace it.", entityNum);
            return -1;
        }
        G_Printf("%s\n", ent->classname);
        MQ_ReplaceItem(ent, item);
        return 1;
    }

    for (i = 0, ent = g_entities; i < MAX_GENTITIES; i++, ent++) {
        if (!ent->inuse || ent->s.eType != ET_ITEM || !ent->classname || strcmp(ent->classname, entityClassname)) {
            continue;
        }
        found = qtrue;
        MQ_ReplaceItem(ent, item);
    }
    return found ? 1 : 0;
}

static void MQ_DevPrintItems(void) {
    char buffer[1024];
    gentity_t* ent;
    qboolean full = qfalse;
    int used = 0;
    int i;

    Q_strncpyz(buffer, "No items found in the map", sizeof(buffer));
    for (i = 0, ent = g_entities; i < MAX_GENTITIES; i++, ent++) {
        const char* line;
        int len;

        if (!ent->inuse || ent->s.eType != ET_ITEM) {
            continue;
        }
        line = va("%d %s\n", i, ent->classname ? ent->classname : "");
        len = (int)strlen(line);
        if (!full && used + len >= (int)sizeof(buffer)) {
            full = qtrue;
            trap_SendServerCommand(-1, va("print \"%s\"", buffer));
            trap_SendServerCommand(-1, "print \"Check server console for other items\n\"\n");
        }
        if (full) {
            G_Printf("%s", line);
            continue;
        }
        Q_strncpyz(buffer + used, line, sizeof(buffer) - used);
        used += len;
    }
    if (!full) {
        trap_SendServerCommand(-1, va("print \"%s\"", buffer));
    }
}

static void MQ_ForceWeaponRespawnTime(int respawnTime) {
    gentity_t* ent;
    int i;

    for (i = 0, ent = g_entities; i < MAX_GENTITIES; i++, ent++) {
        if (ent->inuse && ent->s.eType == ET_ITEM && ent->item && ent->item->giType == IT_WEAPON) {
            ent->wait = respawnTime;
        }
    }
}

static qboolean MQ_Slap(int clientNum, int damage) {
    gentity_t* ent = &g_entities[clientNum];

    if (!MQ_Client(clientNum) || !ent->inuse || ent->health <= 0) {
        return qfalse;
    }
    ent->client->ps.velocity[0] += crandom() * 200.0f;
    ent->client->ps.velocity[1] += crandom() * 200.0f;
    ent->client->ps.velocity[2] += 300.0f;
    ent->health -= damage;
    if (ent->health > 0) {
        G_AddEvent(ent, EV_PAIN, 99);
    } else {
        G_AddEvent(ent, EV_DEATH1, ent->s.number);
    }
    return qtrue;
}

static qboolean MQ_Slay(int clientNum) {
    gentity_t* ent = &g_entities[clientNum];

    if (!MQ_Client(clientNum) || !ent->inuse || ent->health <= 0) {
        return qfalse;
    }
    ent->health = -40;
    G_AddEvent(ent, EV_GIB_PLAYER, ent->s.number);
    return qtrue;
}

static qboolean MQ_IsAlive(int clientNum) {
    gentity_t* ent = &g_entities[clientNum];

    return MQ_Client(clientNum) && ent->inuse && ent->health > 0;
}

static const minqlxGame_t mqGame = {
    MQ_PlayerInfo,
    MQ_PlayerState,
    MQ_PlayerStats,
    MQ_SetPosition,
    MQ_SetVelocity,
    MQ_NoClip,
    MQ_SetHealth,
    MQ_SetArmor,
    MQ_SetWeapons,
    MQ_SetWeapon,
    MQ_SetAmmo,
    MQ_SetPowerups,
    MQ_SetHoldable,
    MQ_DropHoldable,
    MQ_SetFlight,
    MQ_SetInvulnerability,
    MQ_SetScore,
    MQ_SetPrivileges,
    MQ_PlayerSpawn,
    MQ_ForceVote,
    MQ_Callvote,
    MQ_AllowSinglePlayer,
    MQ_DestroyKamikazeTimers,
    MQ_NumItems,
    MQ_SpawnItem,
    MQ_RemoveDroppedItems,
    MQ_SlayWithMod,
    MQ_ReplaceItems,
    MQ_DevPrintItems,
    MQ_ForceWeaponRespawnTime,
    MQ_IsAlive,
    MQ_Slap,
    MQ_Slay,
};

const minqlxGame_t* G_Minqlx(const minqlxHooks_t* hooks) {
    g_minqlxHooks = hooks;
    return &mqGame;
}
