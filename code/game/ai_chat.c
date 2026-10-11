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
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "chars.h"
#include "inv.h"
#include "syn.h"
#include "match.h"

#include "../../ui/menudef.h"

#define TIME_BETWEENCHATTING 25

int BotNumActivePlayers(void) {
    int i, num;
    char buf[MAX_INFO_STRING];

    num = 0;
    for (i = 0; i < level.maxclients; i++) {
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
            continue;
        num++;
    }
    return num;
}

int BotIsFirstInRankings(bot_state_t* bs) {
    int i, score;
    char buf[MAX_INFO_STRING];
    playerState_t ps;

    score = bs->cur_ps.persistant[PERS_SCORE];
    for (i = 0; i < level.maxclients; i++) {
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
            continue;
        if (BotAI_GetClientState(i, &ps) && score < ps.persistant[PERS_SCORE])
            return qfalse;
    }
    return qtrue;
}

int BotIsLastInRankings(bot_state_t* bs) {
    int i, score;
    char buf[MAX_INFO_STRING];
    playerState_t ps;

    score = bs->cur_ps.persistant[PERS_SCORE];
    for (i = 0; i < level.maxclients; i++) {
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
            continue;
        if (BotAI_GetClientState(i, &ps) && score > ps.persistant[PERS_SCORE])
            return qfalse;
    }
    return qtrue;
}

char* BotFirstClientInRankings(void) {
    int i, bestscore, bestclient;
    char buf[MAX_INFO_STRING];
    static char name[32];
    playerState_t ps;

    bestscore = -999999;
    bestclient = 0;
    for (i = 0; i < level.maxclients; i++) {
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
            continue;
        if (BotAI_GetClientState(i, &ps) && ps.persistant[PERS_SCORE] > bestscore) {
            bestscore = ps.persistant[PERS_SCORE];
            bestclient = i;
        }
    }
    EasyClientName(bestclient, name, 32);
    return name;
}

char* BotLastClientInRankings(void) {
    int i, worstscore, bestclient;
    char buf[MAX_INFO_STRING];
    static char name[32];
    playerState_t ps;

    worstscore = 999999;
    bestclient = 0;
    for (i = 0; i < level.maxclients; i++) {
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
            continue;
        if (BotAI_GetClientState(i, &ps) && ps.persistant[PERS_SCORE] < worstscore) {
            worstscore = ps.persistant[PERS_SCORE];
            bestclient = i;
        }
    }
    EasyClientName(bestclient, name, 32);
    return name;
}

char* BotRandomOpponentName(bot_state_t* bs) {
    int i, count;
    char buf[MAX_INFO_STRING];
    int opponents[MAX_CLIENTS], numopponents;
    static char name[32];

    numopponents = 0;
    opponents[0] = 0;
    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n")))
            continue;
        if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
            continue;
        if (BotSameTeam(bs, i))
            continue;
        opponents[numopponents] = i;
        numopponents++;
    }
    count = random() * numopponents;
    for (i = 0; i < numopponents; i++) {
        count--;
        if (count <= 0) {
            EasyClientName(opponents[i], name, sizeof(name));
            return name;
        }
    }
    EasyClientName(opponents[0], name, sizeof(name));
    return name;
}

char* BotMapTitle(void) {
    char info[1024];
    static char mapname[128];

    trap_GetServerinfo(info, sizeof(info));

    strncpy(mapname, Info_ValueForKey(info, "mapname"), sizeof(mapname) - 1);
    mapname[sizeof(mapname) - 1] = '\0';

    return mapname;
}

char* BotWeaponNameForMeansOfDeath(int mod) {
    switch (mod) {
        case MOD_SHOTGUN:
            return "Shotgun";
        case MOD_GAUNTLET:
            return "Gauntlet";
        case MOD_MACHINEGUN:
            return "Machine Gun";
        case MOD_GRENADE:
        case MOD_GRENADE_SPLASH:
            return "Grenade Launcher";
        case MOD_ROCKET:
        case MOD_ROCKET_SPLASH:
            return "Rocket Launcher";
        case MOD_PLASMA:
        case MOD_PLASMA_SPLASH:
            return "Plasmagun";
        case MOD_RAILGUN:
        case MOD_RAILGUN_HEADSHOT:
            return "Railgun";
        case MOD_LIGHTNING:
        case MOD_LIGHTNING_DISCHARGE:
            return "Lightning Gun";
        case MOD_BFG:
        case MOD_BFG_SPLASH:
            return "BFG10K";
        case MOD_NAIL:
            return "Nailgun";
        case MOD_CHAINGUN:
            return "Chaingun";
        case MOD_PROXIMITY_MINE:
            return "Proximity Launcher";
        case MOD_KAMIKAZE:
            return "Kamikaze";
        case MOD_JUICED:
            return "Prox mine";
        case MOD_GRAPPLE:
            return "Grapple";
        case MOD_HMG:
            return "Heavy Machine Gun";
        default:
            return "[unknown weapon]";
    }
}

char* BotRandomWeaponName(void) {
    int rnd;

    rnd = random() * 11.9;
    switch (rnd) {
        case 0:
            return "Gauntlet";
        case 1:
            return "Shotgun";
        case 2:
            return "Machine Gun";
        case 3:
            return "Grenade Launcher";
        case 4:
            return "Rocket Launcher";
        case 5:
            return "Plasmagun";
        case 6:
            return "Railgun";
        case 7:
            return "Lightning Gun";
        case 8:
            return "Nailgun";
        case 9:
            return "Chaingun";
        case 10:
            return "Proximity Launcher";
        default:
            return "BFG10K";
    }
}

int BotVisibleEnemies(bot_state_t* bs) {
    float vis;
    int i;
    aas_entityinfo_t entinfo;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum)
            continue;
        if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
            continue;
        }
        if (BotSameTeam(bs, i))
            continue;
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
        if (vis > 0)
            return qtrue;
    }
    return qfalse;
}

int BotValidChatPosition(bot_state_t* bs) {
    vec3_t point, start, end, mins, maxs;
    bsp_trace_t trace;

    if (BotIsDead(bs))
        return qtrue;
    if (bs->inventory[INVENTORY_QUAD] ||
        bs->inventory[INVENTORY_ENVIRONMENTSUIT] ||
        bs->inventory[INVENTORY_HASTE] ||
        bs->inventory[INVENTORY_INVISIBILITY] ||
        bs->inventory[INVENTORY_REGEN] ||
        bs->inventory[INVENTORY_FLIGHT])
        return qfalse;
    VectorCopy(bs->origin, point);
    point[2] -= 24;
    if (trap_PointContents(point, bs->entitynum) & (CONTENTS_LAVA | CONTENTS_SLIME))
        return qfalse;
    VectorCopy(bs->origin, point);
    point[2] += 32;
    if (trap_PointContents(point, bs->entitynum) & MASK_WATER)
        return qfalse;
    VectorCopy(bs->origin, start);
    VectorCopy(bs->origin, end);
    start[2] += 1;
    end[2] -= 10;
    trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, mins, maxs);
    BotAI_Trace(&trace, start, mins, maxs, end, bs->client, MASK_SOLID);
    if (trace.ent != ENTITYNUM_WORLD)
        return qfalse;
    return qtrue;
}

int BotChat_EnterGame(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer || bot_training.integer || bot_gauntletOnly.integer ||
        bot_followMe.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (TeamPlayIsOn())
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_ENTEREXITGAME, 0, 1);
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
    }
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    if (!BotValidChatPosition(bs))
        return qfalse;
    BotAI_BotInitialChat(bs, "game_enter",
                         EasyClientName(bs->client, name, 32),
                         BotRandomOpponentName(bs),
                         "[invalid var]",
                         "[invalid var]",
                         BotMapTitle(),
                         NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_ExitGame(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (TeamPlayIsOn())
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_ENTEREXITGAME, 0, 1);
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
    }
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    BotAI_BotInitialChat(bs, "game_exit",
                         EasyClientName(bs->client, name, 32),
                         BotRandomOpponentName(bs),
                         "[invalid var]",
                         "[invalid var]",
                         BotMapTitle(),
                         NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_StartLevel(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer)
        return qfalse;
    if (BotIsObserver(bs))
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (TeamPlayIsOn()) {
        trap_EA_Command(bs->client, "vtaunt");
        return qfalse;
    }
    if (gametype == GT_DUEL)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_STARTENDLEVEL, 0, 1);
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
    }
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    BotAI_BotInitialChat(bs, "level_start",
                         EasyClientName(bs->client, name, 32),
                         NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_EndLevel(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer || bot_training.integer || bot_gauntletOnly.integer ||
        bot_followMe.integer)
        return qfalse;
    if (BotIsObserver(bs))
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (TeamPlayIsOn()) {
        if (BotIsFirstInRankings(bs)) {
            trap_EA_Command(bs->client, "vtaunt");
        }
        return qtrue;
    }
    if (gametype == GT_DUEL)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_STARTENDLEVEL, 0, 1);
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
    }
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    if (BotIsFirstInRankings(bs)) {
        BotAI_BotInitialChat(bs, "level_end_victory",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             "[invalid var]",
                             BotLastClientInRankings(),
                             BotMapTitle(),
                             NULL);
    } else if (BotIsLastInRankings(bs)) {
        BotAI_BotInitialChat(bs, "level_end_lose",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             BotFirstClientInRankings(),
                             "[invalid var]",
                             BotMapTitle(),
                             NULL);
    } else {
        BotAI_BotInitialChat(bs, "level_end",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             BotFirstClientInRankings(),
                             BotLastClientInRankings(),
                             BotMapTitle(),
                             NULL);
    }
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_Death(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer || bot_gauntletOnly.integer ||
        bot_training.integer == 1 || bot_training.integer == 2)
        return qfalse;
    if (!bot_training.integer) {
        if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
            return qfalse;
        rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_DEATH, 0, 1);
        if (gametype == GT_DUEL)
            return qfalse;
        if (!bot_fastchat.integer) {
            if (random() > rnd)
                return qfalse;
        }
    }
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    if (bs->lastkilledby >= 0 && bs->lastkilledby < MAX_CLIENTS)
        EasyClientName(bs->lastkilledby, name, 32);
    else
        strcpy(name, "[world]");
    if (TeamPlayIsOn() && BotSameTeam(bs, bs->lastkilledby) && gametype != GT_RR) {
        if (bs->lastkilledby == bs->client)
            return qfalse;
        BotAI_BotInitialChat(bs, "death_teammate", name, NULL);
        bs->chatto = CHAT_TEAM;
    } else {
        if (TeamPlayIsOn()) {
            trap_EA_Command(bs->client, "vtaunt");
            return qtrue;
        }
        if (bs->botdeathtype == MOD_WATER)
            BotAI_BotInitialChat(bs, "death_drown", BotRandomOpponentName(bs), NULL);
        else if (bs->botdeathtype == MOD_SLIME)
            BotAI_BotInitialChat(bs, "death_slime", BotRandomOpponentName(bs), NULL);
        else if (bs->botdeathtype == MOD_LAVA)
            BotAI_BotInitialChat(bs, "death_lava", BotRandomOpponentName(bs), NULL);
        else if (bs->botdeathtype == MOD_FALLING)
            BotAI_BotInitialChat(bs, "death_cratered", BotRandomOpponentName(bs), NULL);
        else if (bs->botsuicide ||
                 bs->botdeathtype == MOD_CRUSH ||
                 bs->botdeathtype == MOD_SUICIDE ||
                 bs->botdeathtype == MOD_TARGET_LASER ||
                 bs->botdeathtype == MOD_TRIGGER_HURT ||
                 bs->botdeathtype == MOD_UNKNOWN)
            BotAI_BotInitialChat(bs, "death_suicide", BotRandomOpponentName(bs), NULL);
        else if (bs->botdeathtype == MOD_TELEFRAG)
            BotAI_BotInitialChat(bs, "death_telefrag", name, NULL);
        else if (bs->botdeathtype == MOD_KAMIKAZE && trap_BotNumInitialChats(bs->cs, "death_kamikaze"))
            BotAI_BotInitialChat(bs, "death_kamikaze", name, NULL);
        else {
            if ((bs->botdeathtype == MOD_GAUNTLET ||
                 bs->botdeathtype == MOD_RAILGUN ||
                 bs->botdeathtype == MOD_BFG ||
                 bs->botdeathtype == MOD_BFG_SPLASH) &&
                random() < 0.5) {
                if (bs->botdeathtype == MOD_GAUNTLET)
                    BotAI_BotInitialChat(bs, "death_gauntlet",
                                         name,
                                         BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                                         NULL);
                else if (bs->botdeathtype == MOD_RAILGUN || bs->botdeathtype == MOD_RAILGUN_HEADSHOT)
                    BotAI_BotInitialChat(bs, "death_rail",
                                         name,
                                         BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                                         NULL);
                else
                    BotAI_BotInitialChat(bs, "death_bfg",
                                         name,
                                         BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                                         NULL);
            }
            else if (random() < trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_INSULT, 0, 1)) {
                BotAI_BotInitialChat(bs, "death_insult",
                                     name,
                                     BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                                     NULL);
            } else {
                BotAI_BotInitialChat(bs, "death_praise",
                                     name,
                                     BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                                     NULL);
            }
        }
        bs->chatto = CHAT_ALL;
    }
    bs->lastchat_time = FloatTime();
    return qtrue;
}

int BotChat_Kill(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_KILL, 0, 1);
    if (gametype == GT_DUEL)
        return qfalse;
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
    }
    if (bs->lastkilledplayer == bs->client)
        return qfalse;
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    if (!BotValidChatPosition(bs))
        return qfalse;
    if (BotVisibleEnemies(bs))
        return qfalse;
    EasyClientName(bs->lastkilledplayer, name, 32);
    bs->chatto = CHAT_ALL;
    if (TeamPlayIsOn() && BotSameTeam(bs, bs->lastkilledplayer)) {
        BotAI_BotInitialChat(bs, "kill_teammate", name, NULL);
        bs->chatto = CHAT_TEAM;
    } else {
        if (TeamPlayIsOn()) {
            trap_EA_Command(bs->client, "vtaunt");
            return qfalse;
        }
        if (bs->enemydeathtype == MOD_GAUNTLET) {
            BotAI_BotInitialChat(bs, "kill_gauntlet", name, NULL);
        } else if (bs->enemydeathtype == MOD_RAILGUN ||
                   bs->enemydeathtype == MOD_RAILGUN_HEADSHOT) {
            BotAI_BotInitialChat(bs, "kill_rail", name, NULL);
        } else if (bs->enemydeathtype == MOD_TELEFRAG) {
            BotAI_BotInitialChat(bs, "kill_telefrag", name, NULL);
        } else if (bs->botdeathtype == MOD_KAMIKAZE && trap_BotNumInitialChats(bs->cs, "kill_kamikaze"))
            BotAI_BotInitialChat(bs, "kill_kamikaze", name, NULL);
        else if (random() < trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_INSULT, 0, 1)) {
            BotAI_BotInitialChat(bs, "kill_insult", name, NULL);
        } else {
            BotAI_BotInitialChat(bs, "kill_praise", name, NULL);
        }
    }
    bs->lastchat_time = FloatTime();
    return qtrue;
}

int BotChat_EnemySuicide(bot_state_t* bs) {
    char name[32];
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_ENEMYSUICIDE, 0, 1);
    if (TeamPlayIsOn())
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
    }
    if (!BotValidChatPosition(bs))
        return qfalse;
    if (BotVisibleEnemies(bs))
        return qfalse;
    if (bs->enemy >= 0)
        EasyClientName(bs->enemy, name, 32);
    else
        strcpy(name, "");
    BotAI_BotInitialChat(bs, "enemy_suicide", name, NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_HitTalking(bot_state_t* bs) {
    char name[32], *weap;
    int lasthurt_client;
    float rnd;

    if (bot_nochat.integer || g_instaGib.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    lasthurt_client = g_entities[bs->client].client->lasthurt_client[0];
    if (!lasthurt_client)
        return qfalse;
    if (lasthurt_client == bs->client)
        return qfalse;
    if (lasthurt_client < 0 || lasthurt_client >= MAX_CLIENTS)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_HITTALKING, 0, 1);
    if (TeamPlayIsOn())
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    if (!bot_fastchat.integer) {
        if (random() > rnd * 0.5)
            return qfalse;
    }
    if (!BotValidChatPosition(bs))
        return qfalse;
    ClientName(g_entities[bs->client].client->lasthurt_client[0], name, sizeof(name));
    weap = BotWeaponNameForMeansOfDeath(g_entities[bs->client].client->lasthurt_mod[0]);
    BotAI_BotInitialChat(bs, "hit_talking", name, weap, NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_HitNoDeath(bot_state_t* bs) {
    char name[32], *weap;
    float rnd;
    int lasthurt_client;
    aas_entityinfo_t entinfo;

    lasthurt_client = g_entities[bs->client].client->lasthurt_client[0];
    if (!lasthurt_client)
        return qfalse;
    if (lasthurt_client == bs->client)
        return qfalse;
    if (lasthurt_client < 0 || lasthurt_client >= MAX_CLIENTS)
        return qfalse;
    if (bot_nochat.integer || g_instaGib.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_HITNODEATH, 0, 1);
    if (TeamPlayIsOn())
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    if (!bot_fastchat.integer) {
        if (random() > rnd * 0.5)
            return qfalse;
    }
    if (!BotValidChatPosition(bs))
        return qfalse;
    if (BotVisibleEnemies(bs))
        return qfalse;
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsShooting(&entinfo))
        return qfalse;
    ClientName(lasthurt_client, name, sizeof(name));
    weap = BotWeaponNameForMeansOfDeath(g_entities[bs->client].client->lasthurt_mod[0]);
    BotAI_BotInitialChat(bs, "hit_nodeath", name, weap, NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_HitNoKill(bot_state_t* bs) {
    char name[32], *weap;
    float rnd;
    aas_entityinfo_t entinfo;

    if (bot_nochat.integer || g_instaGib.integer)
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_HITNOKILL, 0, 1);
    if (TeamPlayIsOn())
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    if (!bot_fastchat.integer) {
        if (random() > rnd * 0.5)
            return qfalse;
    }
    if (!BotValidChatPosition(bs))
        return qfalse;
    if (BotVisibleEnemies(bs))
        return qfalse;
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsShooting(&entinfo))
        return qfalse;
    ClientName(bs->enemy, name, sizeof(name));
    weap = BotWeaponNameForMeansOfDeath(g_entities[bs->enemy].client->lasthurt_mod[0]);
    BotAI_BotInitialChat(bs, "hit_nokill", name, weap, NULL);
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

int BotChat_Random(bot_state_t* bs) {
    float rnd;
    char name[32];

    if (bot_nochat.integer || g_instaGib.integer || bot_training.integer || bot_gauntletOnly.integer ||
        bot_followMe.integer || g_training.integer)
        return qfalse;
    if (BotIsObserver(bs))
        return qfalse;
    if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING)
        return qfalse;
    if (gametype == GT_DUEL)
        return qfalse;
    if (bs->ltgtype == LTG_TEAMHELP ||
        bs->ltgtype == LTG_TEAMACCOMPANY ||
        bs->ltgtype == LTG_RUSHBASE)
        return qfalse;
    rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_RANDOM, 0, 1);
    if (random() > bs->thinktime * 0.1)
        return qfalse;
    if (!bot_fastchat.integer) {
        if (random() > rnd)
            return qfalse;
        if (random() > 0.25)
            return qfalse;
    }
    if (BotNumActivePlayers() <= 1)
        return qfalse;
    if (!BotValidChatPosition(bs))
        return qfalse;
    if (BotVisibleEnemies(bs))
        return qfalse;
    if (bs->lastkilledplayer == bs->client) {
        strcpy(name, BotRandomOpponentName(bs));
    } else {
        EasyClientName(bs->lastkilledplayer, name, sizeof(name));
    }
    if (TeamPlayIsOn()) {
        trap_EA_Command(bs->client, "vtaunt");
        return qfalse;
    }
    if (random() < trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_MISC, 0, 1)) {
        BotAI_BotInitialChat(bs, "random_misc",
                             BotRandomOpponentName(bs),
                             name,
                             "[invalid var]",
                             "[invalid var]",
                             BotMapTitle(),
                             BotRandomWeaponName(),
                             NULL);
    } else {
        BotAI_BotInitialChat(bs, "random_insult",
                             BotRandomOpponentName(bs),
                             name,
                             "[invalid var]",
                             "[invalid var]",
                             BotMapTitle(),
                             BotRandomWeaponName(),
                             NULL);
    }
    bs->lastchat_time = FloatTime();
    bs->chatto = CHAT_ALL;
    return qtrue;
}

float BotChatTime(bot_state_t* bs) {

    return 2.0;
}

void BotChatTest(bot_state_t* bs) {
    char name[32];
    char* weap;
    int num, i;

    num = trap_BotNumInitialChats(bs->cs, "game_enter");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "game_enter",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             "[invalid var]",
                             "[invalid var]",
                             BotMapTitle(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "game_exit");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "game_exit",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             "[invalid var]",
                             "[invalid var]",
                             BotMapTitle(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "level_start");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "level_start",
                             EasyClientName(bs->client, name, 32),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "level_end_victory");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "level_end_victory",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             BotFirstClientInRankings(),
                             BotLastClientInRankings(),
                             BotMapTitle(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "level_end_lose");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "level_end_lose",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             BotFirstClientInRankings(),
                             BotLastClientInRankings(),
                             BotMapTitle(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "level_end");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "level_end",
                             EasyClientName(bs->client, name, 32),
                             BotRandomOpponentName(bs),
                             BotFirstClientInRankings(),
                             BotLastClientInRankings(),
                             BotMapTitle(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    EasyClientName(bs->lastkilledby, name, sizeof(name));
    num = trap_BotNumInitialChats(bs->cs, "death_drown");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_drown", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_slime");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_slime", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_lava");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_lava", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_cratered");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_cratered", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_suicide");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_suicide", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_telefrag");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_telefrag", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_gauntlet");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_gauntlet",
                             name,
                             BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_rail");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_rail",
                             name,
                             BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_bfg");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_bfg",
                             name,
                             BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_insult");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_insult",
                             name,
                             BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "death_praise");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "death_praise",
                             name,
                             BotWeaponNameForMeansOfDeath(bs->botdeathtype),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    EasyClientName(bs->lastkilledplayer, name, 32);
    num = trap_BotNumInitialChats(bs->cs, "kill_gauntlet");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "kill_gauntlet", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "kill_rail");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "kill_rail", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "kill_telefrag");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "kill_telefrag", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "kill_insult");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "kill_insult", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "kill_praise");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "kill_praise", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "enemy_suicide");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "enemy_suicide", name, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    ClientName(g_entities[bs->client].client->lasthurt_client[0], name, sizeof(name));
    weap = BotWeaponNameForMeansOfDeath(g_entities[bs->client].client->lasthurt_client[0]);
    num = trap_BotNumInitialChats(bs->cs, "hit_talking");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "hit_talking", name, weap, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "hit_nodeath");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "hit_nodeath", name, weap, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "hit_nokill");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "hit_nokill", name, weap, NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    if (bs->lastkilledplayer == bs->client) {
        strcpy(name, BotRandomOpponentName(bs));
    } else {
        EasyClientName(bs->lastkilledplayer, name, sizeof(name));
    }
    num = trap_BotNumInitialChats(bs->cs, "random_misc");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "random_misc",
                             BotRandomOpponentName(bs),
                             name,
                             "[invalid var]",
                             "[invalid var]",
                             BotMapTitle(),
                             BotRandomWeaponName(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
    num = trap_BotNumInitialChats(bs->cs, "random_insult");
    for (i = 0; i < num; i++) {
        BotAI_BotInitialChat(bs, "random_insult",
                             BotRandomOpponentName(bs),
                             name,
                             "[invalid var]",
                             "[invalid var]",
                             BotMapTitle(),
                             BotRandomWeaponName(),
                             NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
    }
}
