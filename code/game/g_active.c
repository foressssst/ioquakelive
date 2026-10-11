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

void P_DamageFeedback(gentity_t* player) {
    gclient_t* client;
    float count;
    vec3_t angles;

    client = player->client;
    if (client->ps.pm_type == PM_DEAD) {
        return;
    }

    count = client->damage_blood + client->damage_armor;
    if (count == 0) {
        return;
    }

    if (count > 255) {
        count = 255;
    }

    if (client->damage_fromWorld) {
        client->ps.damagePitch = 255;
        client->ps.damageYaw = 255;

        client->damage_fromWorld = qfalse;
    } else {
        vectoangles(client->damage_from, angles);
        client->ps.damagePitch = angles[PITCH] / 360.0 * 256;
        client->ps.damageYaw = angles[YAW] / 360.0 * 256;
    }

    if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE)) {
        int painValue;
        player->pain_debounce_time = level.time + 700;
        if (player->health < 25) {
            painValue = 20;
        } else if (player->health < 50) {
            painValue = 40;
        } else if (player->health < 75) {
            painValue = 60;
        } else {
            painValue = 80;
        }
        G_AddEvent(player, EV_PAIN, painValue);
    }

    client->ps.damageEvent++;
    client->ps.damageCount = count;

    client->damage_blood = 0;
    client->damage_armor = 0;
}

void P_WorldEffects(gentity_t* ent) {
    qboolean envirosuit;
    int waterlevel;

    if (ent->client->noclip) {
        ent->client->airOutTime = level.time + 12000;
        return;
    }

    waterlevel = ent->waterlevel;

    envirosuit = ent->client->ps.powerups[PW_BATTLESUIT] > level.time;

    if (ent->client->noclip) {
        ent->client->airOutTime = level.time + 12000;
        return;
    }

    if (waterlevel == 3) {
        if (envirosuit) {
            ent->client->airOutTime = level.time + 10000;
        }

        if (ent->client->airOutTime < level.time) {
            ent->client->airOutTime += 1000;
            if (ent->health > 0) {
                ent->damage += 2;
                if (ent->damage > 15)
                    ent->damage = 15;

                if (ent->damage < ent->health) {
                    G_Sound(ent, CHAN_VOICE,
                            G_SoundIndex((rand() & 1) ? "sound/player/gurp1.wav"
                                                      : "sound/player/gurp2.wav"));
                }

                ent->pain_debounce_time = level.time + 200;

                G_Damage(ent, NULL, NULL, NULL, NULL,
                         ent->damage, DAMAGE_NO_ARMOR, MOD_WATER);
            }
        }
    } else {
        ent->client->airOutTime = level.time + 12000;
        ent->damage = 2;
        if (waterlevel == 0) {
            return;
        }
    }

    if (waterlevel &&
        (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
        if (ent->health > 0 && ent->pain_debounce_time <= level.time) {
            if (envirosuit) {
                G_AddEvent(ent, EV_POWERUP_BATTLESUIT, 0);
            } else {
                if (ent->watertype & CONTENTS_LAVA) {
                    G_Damage(ent, NULL, NULL, NULL, NULL,
                             30 * waterlevel, 0, MOD_LAVA);
                }

                if (ent->watertype & CONTENTS_SLIME) {
                    G_Damage(ent, NULL, NULL, NULL, NULL,
                             10 * waterlevel, 0, MOD_SLIME);
                }
            }
        }
    }
}

void G_SetClientSound(gentity_t* ent) {
    if (ent->s.eFlags & EF_TICKING) {
        ent->client->ps.loopSound = G_SoundIndex("sound/weapons/proxmine/wstbtick.wav");
    } else {
        if (ent->waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
            ent->client->ps.loopSound = level.snd_fry;
        } else {
            ent->client->ps.loopSound = 0;
        }
    }
}

void ClientImpacts(gentity_t* ent, pmove_t* pm) {
    int i, j;
    trace_t trace;
    gentity_t* other;

    memset(&trace, 0, sizeof(trace));
    for (i = 0; i < pm->numtouch; i++) {
        for (j = 0; j < i; j++) {
            if (pm->touchents[j] == pm->touchents[i]) {
                break;
            }
        }
        if (j != i) {
            continue;
        }
        other = &g_entities[pm->touchents[i]];

        if ((ent->r.svFlags & SVF_BOT) && (ent->touch)) {
            ent->touch(ent, other, &trace);
        }

        if (!other->touch) {
            continue;
        }

        other->touch(other, ent, &trace);
    }
}

void G_TouchTriggers(gentity_t* ent) {
    int i, num;
    int touch[MAX_GENTITIES];
    gentity_t* hit;
    trace_t trace;
    vec3_t mins, maxs;
    static vec3_t range = {40, 40, 52};

    if (!ent->client) {
        return;
    }

    if (ent->client->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }

    VectorSubtract(ent->client->ps.origin, range, mins);
    VectorAdd(ent->client->ps.origin, range, maxs);

    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    VectorAdd(ent->client->ps.origin, ent->r.mins, mins);
    VectorAdd(ent->client->ps.origin, ent->r.maxs, maxs);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];

        if (!hit->touch && !ent->touch) {
            continue;
        }
        if (!(hit->r.contents & CONTENTS_TRIGGER)) {
            continue;
        }

        if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
            if (hit->s.eType != ET_TELEPORT_TRIGGER &&
                hit->touch != Touch_DoorTrigger) {
                continue;
            }
        }

        if (hit->s.eType == ET_ITEM) {
            if (!BG_PlayerTouchesItem(&ent->client->ps, &hit->s, level.time, g_itemHeight.integer)) {
                continue;
            }
        } else {
            if (!trap_EntityContact(mins, maxs, hit)) {
                continue;
            }
        }

        memset(&trace, 0, sizeof(trace));

        if (hit->touch) {
            hit->touch(hit, ent, &trace);
        }

        if ((ent->r.svFlags & SVF_BOT) && (ent->touch)) {
            ent->touch(ent, hit, &trace);
        }
    }

    if (ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount) {
        ent->client->ps.jumppad_frame = 0;
        ent->client->ps.jumppad_ent = 0;
    }
}

void SpectatorThink(gentity_t* ent, usercmd_t* ucmd) {
    pmove_t pm;
    gclient_t* client;
    static signed char oldUpmove[MAX_CLIENTS];
    int clientNum;
    qboolean jumpPressed;

    client = ent->client;
    clientNum = (int)(client - level.clients);

    if (ent->r.linked) {
        trap_UnlinkEntity(ent);
    }
    if (client->pendingFollow) {
        client->sess.spectatorClient = client->pendingFollowClient;
        if (client->sess.spectatorState != SPECTATOR_FOLLOW) {
            Cmd_Score_f(ent);
        }
        client->sess.spectatorState = SPECTATOR_FOLLOW;
        client->pendingFollow = qfalse;
    }

    if (client->sess.spectatorState != SPECTATOR_FOLLOW) {
        client->ps.pm_type = PM_SPECTATOR;
        client->ps.speed = 480;

        memset(&pm, 0, sizeof(pm));
        pm.ps = &client->ps;
        pm.cmd = *ucmd;
        pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
        pm.trace = trap_Trace;
        pm.pointcontents = trap_PointContents;

        Pmove(&pm);
        VectorCopy(client->ps.origin, ent->s.origin);

        G_TouchTriggers(ent);
        trap_UnlinkEntity(ent);
    }

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;

    jumpPressed = (ucmd->upmove > 0 && oldUpmove[clientNum] <= 0);
    oldUpmove[clientNum] = ucmd->upmove;

    if ((client->buttons & BUTTON_ATTACK) && !(client->oldbuttons & BUTTON_ATTACK)) {
        FollowCycle(ent, 1);
        return;
    }

    if (jumpPressed &&
        (client->sess.sessionTeam == TEAM_SPECTATOR ||
         (g_teamSpecFreeCam.integer == 1 && client->sess.spectatorState == SPECTATOR_FOLLOW))) {
        StopFollowing(ent);
    }
}

qboolean G_FloodCheck(gclient_t* client) {
    if (g_floodprot_maxcount.integer != 0 &&
        g_floodprot_maxcount.integer < client->pers.userInfoFloodInfractions) {
        trap_DropClient(client - level.clients, "Dropped for flooding the server");
        return qfalse;
    }

    if (g_floodprot_decay.integer < level.time - client->pers.lastUserinfoUpdate) {
        client->pers.lastUserinfoUpdate = level.time;
        if (client->pers.userInfoFloodInfractions > 0) {
            client->pers.userInfoFloodInfractions--;
        }
    }
    return qtrue;
}

qboolean ClientInactivityTimer(gclient_t* client, int msec) {
    int clientNum = client - level.clients;

    if (g_inactivity.integer == 0 ||
        (client->pers.cmd.buttons & BUTTON_ATTACK) ||
        client->pers.cmd.forwardmove != 0 ||
        client->pers.cmd.rightmove != 0 ||
        client->pers.cmd.upmove != 0 ||
        client->sess.sessionTeam == TEAM_SPECTATOR ||
        client->sess.privileges > 1) {
        client->pers.inactivityTime = 0;
        client->pers.inactivityWarning = qfalse;
    } else {
        qboolean isRoundBased = (g_gametype.integer == GT_CA ||
                                 g_gametype.integer == GT_FREEZE ||
                                 g_gametype.integer == GT_AD ||
                                 g_gametype.integer == GT_RR);
        if ((!isRoundBased || client->ps.pm_type == PM_NORMAL) && !level.restarted) {
            client->pers.inactivityTime += msec;

            if (!client->pers.localClient) {
                if (client->pers.inactivityTime > g_inactivity.integer * 1000) {
                    if (g_dropInactive.integer) {
                        trap_DropClient(clientNum, "Dropped due to inactivity");
                        return qfalse;
                    }
                    SetTeam(&g_entities[clientNum], "spectator");
                    FollowCycle(&g_entities[clientNum], 1);
                    return qfalse;
                }

                if (client->pers.inactivityTime >
                        (g_inactivity.integer - g_inactivityWarning.integer) * 1000 &&
                    !client->pers.inactivityWarning) {
                    const char* plural = (g_inactivityWarning.integer != 1) ? "s" : "";
                    client->pers.inactivityWarning = qtrue;
                    trap_SendServerCommand(clientNum,
                        va("cp \"%i second%s until dropped for inactivity!\n\"",
                           g_inactivityWarning.integer, plural));
                }
            }
        }
    }

    if (client->ps.pm_type != PM_SPECTATOR) {
        if (client->pers.cmd.forwardmove != 0 ||
            client->pers.cmd.rightmove != 0 ||
            client->pers.cmd.upmove != 0 ||
            (client->pers.cmd.buttons & BUTTON_ATTACK)) {
            client->lastUserCmdTime = level.time;
        }
    }

    if (g_debugInactivity.integer) {
        G_Printf("client:%i inactivity:%i\n", clientNum, client->pers.inactivityTime);
    }

    return qtrue;
}

void G_PointOfInterest(gentity_t* ent, int duration, int payload) {
    gentity_t* te = G_TempEntity(ent->client->ps.origin, EV_POI);

    te->r.svFlags |= SVF_BROADCAST;
    te->s.powerups = duration;
    te->s.generic1 = payload;
    ent->client->timeResidualPingPOI = 0;
}

void ClientTimerActions(gentity_t* ent, int msec) {
    gclient_t* client;
    int maxHealth;

    client = ent->client;
    client->timeResidual += msec;
    client->pers.teamState.flagsince += msec;

    while (client->timeResidual >= 1000) {
        client->timeResidual -= 1000;

        if (g_gametype.integer > GT_RACE && g_gametype.integer != GT_CA) {
            int team = client->sess.sessionTeam;

            if (client->ps.powerups[PW_QUAD]) {
                level.quadDamagePossessionTime[team]++;
            }
            if (client->ps.powerups[PW_BATTLESUIT]) {
                level.battleSuitPossessionTime[team]++;
            }
            if (client->ps.powerups[PW_REGEN]) {
                level.regenerationPossessionTime[team]++;
            }
            if (client->ps.powerups[PW_HASTE]) {
                level.hastePossessionTime[team]++;
            }
            if (client->ps.powerups[PW_INVIS]) {
                level.invisibilityPossessionTime[team]++;
            }
            if (client->ps.powerups[PW_REDFLAG] || client->ps.powerups[PW_BLUEFLAG] ||
                client->ps.powerups[PW_NEUTRALFLAG]) {
                level.flagPossessionTime[team]++;
            }
        }

        if (client->ps.powerups[PW_REGEN]) {
            maxHealth = client->ps.stats[STAT_MAX_HEALTH];
            if (ent->health < maxHealth) {
                ent->health += 15;
                if (ent->health > maxHealth * 1.1) {
                    ent->health = maxHealth * 1.1;
                }
                G_AddEvent(ent, EV_POWERUP_REGEN, 0);
            } else if (ent->health < maxHealth * 2) {
                ent->health += 5;
                if (ent->health > maxHealth * 2) {
                    ent->health = maxHealth * 2;
                }
                G_AddEvent(ent, EV_POWERUP_REGEN, 0);
            }
        } else if (ent->health > client->ps.stats[STAT_MAX_HEALTH]) {
            int roundState = RS_PLAYING;

            switch (g_gametype.integer) {
                case GT_CA:
                    roundState = CA_GetRoundState();
                    break;
                case GT_FREEZE:
                    roundState = Freeze_GetRoundState();
                    break;
                case GT_AD:
                    roundState = AD_GetRoundState();
                    break;
                case GT_RR:
                    roundState = RR_GetRoundState();
                    break;
                default:
                    break;
            }
            if (roundState == RS_PLAYING) {
                ent->health--;
            }
        }

        if (client->ps.stats[STAT_RUNE] == 4 && client->ps.stats[STAT_ARMOR] < client->ps.stats[STAT_MAX_HEALTH]) {
            client->ps.stats[STAT_ARMOR] += 10;
            if (client->ps.stats[STAT_ARMOR] > client->ps.stats[STAT_MAX_HEALTH] && !armor_tiered.integer) {
                client->ps.stats[STAT_ARMOR] = client->ps.stats[STAT_MAX_HEALTH];
            }
            ent->s.armor = client->ps.stats[STAT_ARMOR];
            G_AddEvent(ent, EV_POWERUP_ARMORREGEN, 0);
        }

        if (client->ps.stats[STAT_ARMOR] > client->ps.stats[STAT_MAX_HEALTH]) {
            qboolean countDown;

            if (g_gametype.integer == GT_CA) {
                countDown = armor_tiered.integer != 0;
            } else {
                countDown = !armor_tiered.integer && client->ps.stats[STAT_RUNE] != 1;
            }
            if (countDown) {
                client->ps.stats[STAT_ARMOR]--;
                ent->s.armor = client->ps.stats[STAT_ARMOR];
            }
        }
    }

    if (g_gametype.integer == GT_FFA && g_quadHog.integer && client->ps.powerups[PW_QUAD]) {
        client->timeResidualPingPOI += msec;
        if (client->timeResidualPingPOI >= g_quadHogPingRate.integer) {
            G_PointOfInterest(ent, g_quadHogPingRate.integer, client->ps.clientNum);
        }
    }

    if (client->ps.powerups[PW_NEUTRALFLAG]) {
        client->timeResidualPingPOI += msec;
        if (client->timeResidualPingPOI >= g_neutralFlagPingRate.integer) {
            G_PointOfInterest(ent, g_neutralFlagPingRate.integer,
                              client->ps.persistant[PERS_TEAM]);
        }
    }

    if (g_gametype.integer != GT_CA && client->ps.stats[STAT_RUNE] == 1) {
        client->timeResidualScout += msec;
        while (client->timeResidualScout >= 500) {
            client->timeResidualScout -= 500;
            if (client->ps.stats[STAT_ARMOR] > 50) {
                client->ps.stats[STAT_ARMOR]--;
            }
        }
    }

    if (g_regenHealth.integer) {
        if (ent->health >= client->ps.stats[STAT_MAX_HEALTH]) {
            client->healthRegenActive = qfalse;
        }
        if (client->healthRegenActive && client->lasthurt_time[0] &&
            g_regenHealth.integer < level.time - client->lasthurt_time[0]) {
            client->timeResidualHealth += msec;
        }
        while (client->timeResidualHealth >= g_regenHealthRate.integer) {
            ent->health += 1;
            client->timeResidualHealth -= g_regenHealthRate.integer;
        }
    }

    if (g_regenArmor.integer) {
        qboolean atCap;
        if (armor_tiered.integer) {
            int tier = client->ps.stats[STAT_ARMORTYPE];
            atCap = (tier == 2 && client->ps.stats[STAT_ARMOR] >= 150) ||
                    (tier == 1 && client->ps.stats[STAT_ARMOR] >= 100) ||
                    (tier == 0 && client->ps.stats[STAT_ARMOR] >= 50);
        } else {
            atCap = client->ps.stats[STAT_ARMOR] >= client->ps.stats[STAT_MAX_HEALTH];
        }
        if (atCap) {
            client->armorRegenActive = qfalse;
        }
        if (client->armorRegenActive &&
            g_regenArmor.integer < level.time - client->lasthurt_time[0] &&
            (!g_regenArmorAfterHealth.integer ||
             ent->health >= client->ps.stats[STAT_MAX_HEALTH])) {
            client->timeResidualArmor += msec;
        }
        while (client->timeResidualArmor >= g_regenArmorRate.integer) {
            client->ps.stats[STAT_ARMOR] += 1;
            ent->s.armor = client->ps.stats[STAT_ARMOR];
            client->timeResidualArmor -= g_regenArmorRate.integer;
        }
    }
}

void ClientIntermissionThink(gclient_t* client) {
    client->ps.eFlags &= ~EF_TALK;
    client->ps.eFlags &= ~EF_FIRING;

    client->oldbuttons = client->buttons;
    client->buttons = client->pers.cmd.buttons;
    if (client->buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE) & (client->oldbuttons ^ client->buttons)) {
        client->pers.ready = !client->pers.ready;
    }
}

void ClientEvents(gentity_t* ent, int oldEventSequence) {
    int i, j;
    int event;
    gclient_t* client;
    int damage;
    vec3_t origin, angles;
    gitem_t* item;
    gentity_t* drop;

    client = ent->client;

    if (oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS) {
        oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
    }
    for (i = oldEventSequence; i < client->ps.eventSequence; i++) {
        event = client->ps.events[i & (MAX_PS_EVENTS - 1)];

        switch (event) {
            case EV_FALL_MEDIUM:
            case EV_FALL_FAR:
                if (ent->s.eType != ET_PLAYER) {
                    break;
                }
                if (g_dmflags.integer & DF_NO_FALLING_DAMAGE) {
                    break;
                }
                if (event == EV_FALL_FAR) {
                    damage = 10;
                } else {
                    damage = 5;
                }
                ent->pain_debounce_time = level.time + 200;
                G_Damage(ent, NULL, NULL, NULL, NULL, damage, 0, MOD_FALLING);
                break;

            case EV_FIRE_WEAPON:
                FireWeapon(ent);
                break;

            case EV_USE_ITEM1:
                item = NULL;
                j = 0;

                if (ent->client->ps.powerups[PW_REDFLAG]) {
                    item = BG_FindItemForPowerup(PW_REDFLAG);
                    j = PW_REDFLAG;
                } else if (ent->client->ps.powerups[PW_BLUEFLAG]) {
                    item = BG_FindItemForPowerup(PW_BLUEFLAG);
                    j = PW_BLUEFLAG;
                } else if (ent->client->ps.powerups[PW_NEUTRALFLAG]) {
                    item = BG_FindItemForPowerup(PW_NEUTRALFLAG);
                    j = PW_NEUTRALFLAG;
                }

                if (item) {
                    drop = Drop_Item(ent, item, 0);
                    drop->count = (ent->client->ps.powerups[j] - level.time) / 1000;
                    if (drop->count < 1) {
                        drop->count = 1;
                    }

                    ent->client->ps.powerups[j] = 0;
                }

                if (g_gametype.integer == GT_HARVESTER) {
                    if (ent->client->ps.generic1 > 0) {
                        if (ent->client->sess.sessionTeam == TEAM_RED) {
                            item = BG_FindItem("Blue Skull");
                        } else {
                            item = BG_FindItem("Red Skull");
                        }
                        if (item) {
                            for (j = 0; j < ent->client->ps.generic1; j++) {
                                drop = Drop_Item(ent, item, 0);
                                if (ent->client->sess.sessionTeam == TEAM_RED) {
                                    drop->spawnflags = TEAM_BLUE;
                                } else {
                                    drop->spawnflags = TEAM_RED;
                                }
                            }
                        }
                        ent->client->ps.generic1 = 0;
                    }
                }

                SelectSpawnPoint(ent->client->ps.origin, origin, angles, qfalse);
                TeleportPlayer(ent, origin, angles);
                break;

            case EV_USE_ITEM2:
                ent->health = ent->client->ps.stats[STAT_MAX_HEALTH] + 25;

                break;

            case EV_USE_ITEM3:
                ent->client->invulnerabilityTime = 0;
                G_StartKamikaze(ent);
                break;

            case EV_USE_ITEM4:
                if (ent->client->portalID) {
                    DropPortalSource(ent);
                } else {
                    DropPortalDestination(ent);
                }
                break;
            case EV_USE_ITEM5:
                ent->client->invulnerabilityTime = level.time + 10000;
                break;

            default:
                break;
        }
    }
}

static int StuckInOtherClient(gentity_t* ent) {
    int i;
    gentity_t* ent2;

    ent2 = &g_entities[0];
    for (i = 0; i < MAX_CLIENTS; i++, ent2++) {
        if (ent2 == ent) {
            continue;
        }
        if (!ent2->inuse) {
            continue;
        }
        if (!ent2->client) {
            continue;
        }
        if (ent2->health <= 0) {
            continue;
        }
        if (ent2->r.absmin[0] > ent->r.absmax[0])
            continue;
        if (ent2->r.absmin[1] > ent->r.absmax[1])
            continue;
        if (ent2->r.absmin[2] > ent->r.absmax[2])
            continue;
        if (ent2->r.absmax[0] < ent->r.absmin[0])
            continue;
        if (ent2->r.absmax[1] < ent->r.absmin[1])
            continue;
        if (ent2->r.absmax[2] < ent->r.absmin[2])
            continue;
        return qtrue;
    }
    return qfalse;
}

void BotTestSolid(vec3_t origin);

void SendPendingPredictableEvents(playerState_t* ps) {
    gentity_t* t;
    int event, seq;
    int extEvent, number;

    if (ps->entityEventSequence < ps->eventSequence) {
        seq = ps->entityEventSequence & (MAX_PS_EVENTS - 1);
        event = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
        extEvent = ps->externalEvent;
        ps->externalEvent = 0;
        t = G_TempEntity(ps->origin, event);
        number = t->s.number;
        BG_PlayerStateToEntityState(ps, &t->s, qtrue);
        t->s.number = number;
        t->s.eType = ET_EVENTS + event;
        t->s.eFlags |= EF_PLAYER_EVENT;
        t->s.otherEntityNum = ps->clientNum;
        t->r.svFlags |= SVF_NOTSINGLECLIENT;
        t->r.singleClient = ps->clientNum;
        ps->externalEvent = extEvent;
    }
}

void ClientThink_real(gentity_t* ent) {
    gclient_t* client;
    pmove_t pm;
    int oldEventSequence;
    int msec;
    usercmd_t* ucmd;

    client = ent->client;

    if (client->pers.connected != CON_CONNECTED) {
        return;
    }

    ucmd = &ent->client->pers.cmd;

    if (ucmd->serverTime > level.time + 200) {
        ucmd->serverTime = level.time + 200;
    }
    if (ucmd->serverTime < level.time - 1000) {
        ucmd->serverTime = level.time - 1000;
    }

    msec = ucmd->serverTime - client->ps.commandTime;
    if (msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW) {
        return;
    }

    if (msec > 200) {
        msec = 200;
    }

    if (level.intermissionTime) {
        ClientIntermissionThink(client);
        return;
    }

    client->ps.pm_flags &= ~PMF_PAUSED;
    if (level.timePauseBegin != 0) {
        client->ps.pm_flags |= PMF_PAUSED;
        client->ps.commandTime = ucmd->serverTime;
        return;
    }

    if (client->spawnRetryTime && client->spawnRetryTime < level.time) {
        client->spawnRetryTime = 0;
        client->ps.pm_type = PM_NORMAL;
        switch (g_gametype.integer) {
        case GT_CA:
        case GT_AD:
            ClientBegin_RoundBased(ent);
            break;
        case GT_FREEZE:
            Freeze_ClientBegin(ent);
            break;
        case GT_RR:
            RR_ClientBegin(ent);
            break;
        default:
            ClientSpawn(ent);
            break;
        }
        return;
    }

    if (client->ps.pm_type == PM_SPECTATOR) {
        if (client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
            return;
        }
        SpectatorThink(ent, ucmd);
        return;
    }

    if (!G_FloodCheck(client)) {
        return;
    }

    if (!ClientInactivityTimer(client, msec)) {
        return;
    }

    if (level.time > client->rewardTime) {
        client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP);
    }

    if (client->noclip) {
        client->ps.pm_type = PM_NOCLIP;
    } else if (client->ps.stats[STAT_HEALTH] <= 0) {
        if (client->ps.powerups[PW_FREEZE] <= 0) {
            client->ps.pm_type = PM_DEAD;
        }
    } else if (client->freezePlayer) {
        client->ps.pm_type = PM_TUTORIAL;
        client->pers.cmd.buttons = 0;
        ucmd->buttons = 0;
    } else if (g_gametype.integer == GT_AD && level.roundState.eCurrent == RS_COUNTDOWN) {
        client->ps.pm_type = PM_FREEZE;
    } else {
        client->ps.pm_type = PM_NORMAL;
    }

    client->ps.gravity = g_gravity.value;

    client->ps.speed = g_speed.value;

    if (client->ps.powerups[PW_HASTE]) {
        client->ps.speed = (int)(client->ps.speed * 1.3);
    } else if (client->ps.stats[STAT_RUNE] == 1) {
        client->ps.speed = (int)(client->ps.speed * 1.25);
    }

    if (client->ps.weapon == WP_GAUNTLET) {
        client->ps.speed = (int)((float)client->ps.speed * g_gauntletSpeedFactor.value);
    }

    if (g_gametype.integer == GT_RR && g_rrInfected.integer != 0
        && client->sess.sessionTeam == TEAM_RED) {
        client->ps.speed = (int)((float)client->ps.speed * g_rrInfectedZombieSpeed.value);
    }

    if (client->ps.weapon == WP_GRAPPLING_HOOK &&
        client->hook && !(ucmd->buttons & BUTTON_ATTACK)) {
        Weapon_HookFree(client->hook);
    }

    oldEventSequence = client->ps.eventSequence;

    memset(&pm, 0, sizeof(pm));

    if (client->ps.weapon == WP_GAUNTLET && !(ucmd->buttons & BUTTON_TALK) &&
        (ucmd->buttons & BUTTON_ATTACK) && client->ps.weaponTime <= 0) {
        pm.gauntletHit = CheckGauntletAttack(ent);
    }

    if (ent->flags & FL_FORCE_GESTURE) {
        ent->flags &= ~FL_FORCE_GESTURE;
        ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
    }

    if (client->ps.powerups[PW_INVULNERABILITY]) {
        if (!(client->ps.pm_flags & PMF_INVULEXPAND)) {
            vec3_t mins = {-42, -42, -42};
            vec3_t maxs = {42, 42, 42};
            vec3_t oldmins, oldmaxs;

            VectorCopy(ent->r.mins, oldmins);
            VectorCopy(ent->r.maxs, oldmaxs);
            VectorCopy(mins, ent->r.mins);
            VectorCopy(maxs, ent->r.maxs);
            trap_LinkEntity(ent);
            if (!StuckInOtherClient(ent)) {
                client->ps.pm_flags |= PMF_INVULEXPAND;
            }
            VectorCopy(oldmins, ent->r.mins);
            VectorCopy(oldmaxs, ent->r.maxs);
            trap_LinkEntity(ent);
        }
    }

    pm.ps = &client->ps;
    pm.cmd = *ucmd;
    if (pm.ps->pm_type == PM_DEAD) {
        pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
    } else if (ent->r.svFlags & SVF_BOT) {
        pm.tracemask = MASK_PLAYERSOLID | CONTENTS_BOTCLIP;
    } else {
        pm.tracemask = MASK_PLAYERSOLID;
    }
    pm.trace = trap_Trace;
    pm.pointcontents = trap_PointContents;
    pm.debugLevel = g_debugMove.integer;
    pm.noFootsteps = (g_dmflags.integer & DF_NO_FOOTSTEPS) > 0;
    pm.retailBobCycle = !client->pers.ioqlClient && !(ent->r.svFlags & SVF_BOT);
    pm.hookEnemy = (client->hook && client->hook->enemy && client->hook->enemy->client != NULL);

    Pmove(&pm);

    if (ent->client->ps.eventSequence != oldEventSequence) {
        ent->eventTime = level.time;
    }
    BG_PlayerStateToEntityStateExtraPolate(&ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue);
    SendPendingPredictableEvents(&ent->client->ps);

    if (!(ent->client->ps.eFlags & EF_FIRING)) {
        client->fireHeld = qfalse;
    }

    VectorCopy(ent->s.pos.trBase, ent->r.currentOrigin);

    VectorCopy(pm.mins, ent->r.mins);
    VectorCopy(pm.maxs, ent->r.maxs);

    ent->waterlevel = pm.waterlevel;
    ent->watertype = pm.watertype;

    ClientEvents(ent, oldEventSequence);

    trap_LinkEntity(ent);
    if (!ent->client->noclip) {
        G_TouchTriggers(ent);
    }

    VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);

    BotTestAAS(ent->r.currentOrigin);

    ClientImpacts(ent, &pm);

    if (ent->client->ps.eventSequence != oldEventSequence) {
        ent->eventTime = level.time;
    }

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;

    if (client->ps.powerups[PW_FREEZE] != 0) {
        Freeze_ClientThawCheck(ent, msec);
        return;
    }

    if (client->ps.stats[STAT_HEALTH] <= 0) {
        ClientRespawn(ent);
        return;
    }

    ClientTimerActions(ent, msec);

    STAT_UpdatePlayTime(ent);
}

void ClientThink(int clientNum) {
    gentity_t* ent;

    ent = g_entities + clientNum;
    trap_GetUsercmd(clientNum, &ent->client->pers.cmd);

    ent->client->lastCmdTime = level.time;

    if (!(ent->r.svFlags & SVF_BOT)) {
        ClientThink_real(ent);
    }
}

void G_RunClient(gentity_t* ent) {
    if (!(ent->r.svFlags & SVF_BOT)) {
        return;
    }
    ent->client->pers.cmd.serverTime = level.time;
    ClientThink_real(ent);
}

void SpectatorClientEndFrame(gentity_t* ent) {
    gclient_t* client;
    gclient_t* cl;

    client = ent->client;

    if (client->sess.spectatorState != SPECTATOR_FOLLOW) {
        return;
    }

    {
        int clientNum = client->sess.spectatorClient;

        if (clientNum == -1) {
            clientNum = level.follow1;
        } else if (clientNum == -2) {
            clientNum = level.follow2;
        }

        if (clientNum >= 0) {
            cl = &level.clients[clientNum];
            if (cl->pers.connected == CON_CONNECTED && cl->ps.pm_type != PM_SPECTATOR) {
                int savedPersistant[MAX_PERSISTANT];
                int savedPing = client->ps.ping;

                memcpy(savedPersistant, client->ps.persistant, sizeof(savedPersistant));
                client->ps = cl->ps;
                client->ps.pm_type = PM_SPECTATOR;
                client->ps.pm_flags |= PMF_FOLLOW;
                client->ps.ping = savedPing;

                if (client->sess.sessionTeam == TEAM_SPECTATOR) {
                    client->ps.eFlags |= EF_SPECTATOR;
                } else {
                    memcpy(client->ps.persistant, savedPersistant, sizeof(savedPersistant));
                    client->ps.persistant[PERS_HITS] = cl->ps.persistant[PERS_HITS];
                }
                return;
            }
        }

        if (client->sess.spectatorClient >= 0) {
            int redAlive, blueAlive;

            Team_LivingTeamCounts(&redAlive, &blueAlive);
            if (client->sess.sessionTeam != TEAM_SPECTATOR && !g_teamSpecFreeCam.integer &&
                redAlive > 0 && blueAlive > 0) {
                FollowCycle(ent, 1);
                return;
            }
            StopFollowing(ent);
        }
    }
}

void ClientEndFrame(gentity_t* ent) {
    int i;

    if (ent->client->ps.pm_type == PM_SPECTATOR) {
        SpectatorClientEndFrame(ent);
        return;
    }

    if (ent->client->ps.weaponPrimary != 0) {
        ent->client->sess.weaponPrimary = ent->client->ps.weaponPrimary;
    }

    if (level.time - ent->client->lastCmdTime > 1000) {
        ent->client->ps.eFlags |= EF_CONNECTION;
        ent->s.eFlags |= EF_CONNECTION;
    } else {
        ent->client->ps.eFlags &= ~EF_CONNECTION;
        ent->s.eFlags &= ~EF_CONNECTION;
    }

    if (ent->client->ps.pm_flags & PMF_PAUSED) {
        return;
    }

    for (i = 0; i < MAX_POWERUPS; i++) {
        if (ent->client->ps.powerups[i] < level.time) {
            ent->client->ps.powerups[i] = 0;
        }
    }

    if (ent->client->invulnerabilityTime > level.time) {
        ent->client->ps.powerups[PW_INVULNERABILITY] = level.time;
    }

    if (level.intermissionTime) {
        return;
    }

    if (g_gametype.integer >= GT_TEAM) {
        gentity_t* loc = Team_GetLocation(ent);

        ent->s.location = loc ? loc->health : 0;
        ent->client->ps.location = ent->s.location;
    }

    P_WorldEffects(ent);

    P_DamageFeedback(ent);

    ent->s.health = ent->health;
    ent->client->ps.stats[STAT_HEALTH] = ent->health;

    G_SetClientSound(ent);

    BG_PlayerStateToEntityStateExtraPolate(&ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue);
    SendPendingPredictableEvents(&ent->client->ps);

    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Update(ent);
    }

}
