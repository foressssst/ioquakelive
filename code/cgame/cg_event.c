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
// cg_event.c -- handle entity events at snapshot or playerstate transitions

#include "cg_local.h"

// for the voice chats
#include "../../ui/menudef.h"
//==========================================================================

/*
===================
CG_IsTeamGametype

[QL] cgamex86.dll 0x10001060: everything but FFA, duel, race and Red Rover
===================
*/
static qboolean CG_IsTeamGametype(int gametype) {
    return gametype < GT_FFA || (gametype > GT_RACE && gametype != GT_RR);
}

/*
===================
CG_RaceBeep

[QL] cgamex86.dll 0x1001ac32: race start and checkpoints beep with cg_killBeep's sounds,
picked by cg_raceBeep 1-8
===================
*/
static void CG_RaceBeep(void) {
    if (cg_raceBeep.integer >= 1 && cg_raceBeep.integer <= 8) {
        trap_S_StartLocalSound(cgs.media.killBeepSounds[cg_raceBeep.integer - 1], CHAN_LOCAL_SOUND);
    }
}

/*
===================
CG_PlaceString

Also called by scoreboard drawing
===================
*/
const char* CG_PlaceString(int rank) {
    static char str[64];
    char *s, *t;

    if (rank & RANK_TIED_FLAG) {
        rank &= ~RANK_TIED_FLAG;
        t = "Tied for ";
    } else {
        t = "";
    }

    if (rank == 1) {
        s = S_COLOR_BLUE "1st" S_COLOR_WHITE;  // draw in blue
    } else if (rank == 2) {
        s = S_COLOR_RED "2nd" S_COLOR_WHITE;  // draw in red
    } else if (rank == 3) {
        s = S_COLOR_YELLOW "3rd" S_COLOR_WHITE;  // draw in yellow
    } else if (rank == 11) {
        s = "11th";
    } else if (rank == 12) {
        s = "12th";
    } else if (rank == 13) {
        s = "13th";
    } else if (rank % 10 == 1) {
        s = va("%ist", rank);
    } else if (rank % 10 == 2) {
        s = va("%ind", rank);
    } else if (rank % 10 == 3) {
        s = va("%ird", rank);
    } else {
        s = va("%ith", rank);
    }

    Com_sprintf(str, sizeof(str), "%s%s", t, s);
    return str;
}

/*
=============
CG_ModToWeaponIcon

[QL] cgamex86.dll 0x10019130: the icon CG_Obituary hands the obituary list and keeps as the
killer's icon. Weapons map to their weapon icons, the grappling hook included, a thaw to
icons/thaw (registered from Freeze Tag on), and everything else to icons/icon_frag.
=============
*/
static qhandle_t CG_ModToWeaponIcon(int mod) {
    switch (mod) {
        case MOD_SHOTGUN:                                       return cg_weapons[WP_SHOTGUN].weaponIcon;
        case MOD_GAUNTLET:                                      return cg_weapons[WP_GAUNTLET].weaponIcon;
        case MOD_MACHINEGUN:                                    return cg_weapons[WP_MACHINEGUN].weaponIcon;
        case MOD_GRENADE:
        case MOD_GRENADE_SPLASH:                                return cg_weapons[WP_GRENADE_LAUNCHER].weaponIcon;
        case MOD_ROCKET:
        case MOD_ROCKET_SPLASH:                                 return cg_weapons[WP_ROCKET_LAUNCHER].weaponIcon;
        case MOD_PLASMA:
        case MOD_PLASMA_SPLASH:                                 return cg_weapons[WP_PLASMAGUN].weaponIcon;
        case MOD_RAILGUN:
        case MOD_RAILGUN_HEADSHOT:                              return cg_weapons[WP_RAILGUN].weaponIcon;
        case MOD_LIGHTNING:
        case MOD_LIGHTNING_DISCHARGE:                           return cg_weapons[WP_LIGHTNING].weaponIcon;
        case MOD_BFG:
        case MOD_BFG_SPLASH:                                    return cg_weapons[WP_BFG].weaponIcon;
        case MOD_NAIL:                                          return cg_weapons[WP_NAILGUN].weaponIcon;
        case MOD_CHAINGUN:                                      return cg_weapons[WP_CHAINGUN].weaponIcon;
        case MOD_PROXIMITY_MINE:                                return cg_weapons[WP_PROX_LAUNCHER].weaponIcon;
        case MOD_HMG:                                           return cg_weapons[WP_HMG].weaponIcon;
        case MOD_GRAPPLE:                                       return cg_weapons[WP_GRAPPLING_HOOK].weaponIcon;
        case MOD_THAW:                                          return cgs.media.thawIconShader;
        default:                                                return cgs.media.fragIconShader;
    }
}

/*
=============
CG_ObitStripColors
[QL] cgamex86.dll 0x10019065: team games strip the obituary names in place,
dropping "^0".."^7" and any character below a space.
=============
*/
static void CG_ObitStripColors(char *s) {
    const char *src = s;
    char *dst = s;

    while (*src) {
        if (src[0] == '^' && src[1] >= '0' && src[1] <= '7') {
            src += 2;
            continue;
        }
        if ((unsigned char)*src >= ' ' || (signed char)*src < 0) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

/*
=============
CG_AddObituary
[QL] cgamex86.dll 0x10018f60. The feed is 16 fixed slots filled in order:
the entry takes the first inactive slot (when all 16 are taken the array
shifts up one first). Past cg_obituaryRowSize the oldest entry is dropped and
everything from that row on is cleared, so only the newest rows remain.
[CPMA] With ch_animateScroll a dropped entry goes at once and the ones left
skip their fade-in, as in CPMA 1.53 (cgame.qvm func 158881).
=============
*/
static void CG_AddObituary(const char *victimName, const char *attackerName,
                            int victimTeam, int attackerTeam,
                            int hasAttacker, qhandle_t weaponIcon) {
    int idx;
    int rowSize;
    qboolean dropped = qfalse;

    for (idx = 0; idx < MAX_OBITUARIES; idx++) {
        if (!cg.obituaries[idx].active) {
            break;
        }
    }
    if (idx == MAX_OBITUARIES) {
        memmove(&cg.obituaries[0], &cg.obituaries[1], (MAX_OBITUARIES - 1) * sizeof(cg.obituaries[0]));
        memset(&cg.obituaries[MAX_OBITUARIES - 1], 0, sizeof(cg.obituaries[0]));
        dropped = qtrue;
        // retail keeps idx at 16 here and writes one slot past the array
        idx = MAX_OBITUARIES - 1;
    }

    cg.obituaries[idx].active = 1;
    Q_strncpyz(cg.obituaries[idx].victimName, victimName, sizeof(cg.obituaries[idx].victimName));
    Q_strncpyz(cg.obituaries[idx].attackerName, attackerName, sizeof(cg.obituaries[idx].attackerName));
    cg.obituaries[idx].hasAttacker = hasAttacker;
    cg.obituaries[idx].victimTeam = victimTeam;
    cg.obituaries[idx].attackerTeam = attackerTeam;
    cg.obituaries[idx].weaponIcon = weaponIcon;
    cg.obituaries[idx].time = cg.time;

    if (cgs.gametype >= GT_TEAM) {
        CG_ObitStripColors(cg.obituaries[idx].attackerName);
        CG_ObitStripColors(cg.obituaries[idx].victimName);
    }

    // 0x100190e1. Clamped only so an out-of-range cvar cannot move memory
    // outside the array.
    rowSize = cg_obituaryRowSize.integer;
    if (rowSize < 0) {
        rowSize = 0;
    } else if (rowSize > MAX_OBITUARIES - 1) {
        rowSize = MAX_OBITUARIES - 1;
    }
    if (idx >= cg_obituaryRowSize.integer) {
        memmove(&cg.obituaries[0], &cg.obituaries[1], rowSize * sizeof(cg.obituaries[0]));
        memset(&cg.obituaries[rowSize], 0, (MAX_OBITUARIES - rowSize) * sizeof(cg.obituaries[0]));
        dropped = qtrue;
    }

    if (dropped && ch_animateScroll.integer) {
        cg.obitSlideStart = 0;
        for (idx = 0; idx < MAX_OBITUARIES; idx++) {
            if (cg.obituaries[idx].active) {
                cg.obituaries[idx].time -= OBIT_FADEIN;
            }
        }
    }
}

/*
=============
CG_ShowFragMessage

[QL] cgamex86.dll 0x100195d8: the frag message, only with cg_drawFragMessages.
Race and Red Rover: plain. FFA/duel: place and score, or "Good Job!" in a
training warmup. Freeze Tag: thawed/froze. Team games: telefragging a teammate
is just noted; any other teamkill adds "Watch your fire!". "Teammate" means the
victim is on the local player's team. Centred at y 90 (144 during a warmup),
scale 0.3, unless cg_cameraOrbit is on.
[ioquakelive] A deliberate divergence: every frag but a teamkill or a Freeze Tag one
reads just "You fragged <name>", on one line with no place or score.
CG_DrawCenterString lays it out.
=============
*/
static void CG_ShowFragMessage(int target, int mod, const char* targetName) {
    const char* s;
    qboolean sameTeam = cgs.gametype >= GT_TEAM &&
                        cg.snap->ps.persistant[PERS_TEAM] == cgs.clientinfo[target].team;

    if (cgs.gametype == GT_FREEZE) {
        if (mod == MOD_THAW) {
            s = va("You thawed %s.", targetName);
        } else if (mod == MOD_TELEFRAG) {
            s = sameTeam ? va("You froze %s, your teammate.", targetName) : va("You froze %s.", targetName);
        } else {
            s = sameTeam ? va("You froze %s, your teammate.\n\n^1Watch your fire!^7", targetName)
                         : va("You froze %s.", targetName);
        }
    } else if (sameTeam && cgs.gametype != GT_RR) {
        s = (mod == MOD_TELEFRAG) ? va("You fragged %s, your teammate.", targetName)
                                  : va("You fragged %s, your teammate.\n\n^1Watch your fire!^7", targetName);
    } else {
        s = va("You fragged %s", targetName);
    }

    if (!cg_cameraOrbit.integer) {
        CG_CenterPrint(s, cg.warmup ? 144 : 90, 0.3f);
        cg.centerPrintFrag = qtrue;
    }
}

/*
=============
CG_GameEventIcon / CG_AddGameEvent

[CPMA] the GameEvents element of CPMA 1.53 also carries what happens with the flag and,
while watching, what the players pick up. They go in the same feed as the kills, in the
same shape: who, what, and the icon between them
=============
*/
static qhandle_t CG_FlagIcon(int powerup) {
    gitem_t* item = BG_FindItemForPowerup((powerup_t)powerup);

    return item ? cg_items[ITEM_INDEX(item)].icon : 0;
}

void CG_AddGameEvent(int clientNum, qhandle_t icon, const char* what) {
    if (!ch_gameEvents.integer || clientNum < 0 || clientNum >= MAX_CLIENTS || !icon) {
        return;
    }
    CG_AddObituary(what, cgs.clientinfo[clientNum].name, TEAM_FREE, cgs.clientinfo[clientNum].team, 1, icon);
}

// whether what is watched is watched from outside: a demo, a spectator, or following a player
static qboolean CG_Watching(void) {
    return (cg.demoPlayback || cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ||
            (cg.snap->ps.pm_flags & PMF_FOLLOW)) ? qtrue : qfalse;
}

/*
=============
CG_Obituary
=============
*/
static void CG_Obituary(entityState_t* ent) {
    int mod;
    int target, attacker;
    char* message;
    char* message2;
    const char* targetInfo;
    const char* attackerInfo;
    char targetName[32];
    char attackerName[32];
    gender_t gender;
    clientInfo_t* ci;
    qhandle_t weaponIcon;
    int victimTeam, attackerTeam;

    target = ent->otherEntityNum;
    attacker = ent->otherEntityNum2;
    mod = ent->eventParm;

    if (target < 0 || target >= MAX_CLIENTS) {
        CG_Error("CG_Obituary: target out of range");
    }

    // [QL] 0x1001917e: the local player's death clears the scoreboard's "Fragged by" line,
    // which a killer then sets again below. Retail clears it nowhere else.
    if (target == cg.clientNum) {
        cg.killerName[0] = 0;
    }

    // [QL] 0x1001924c: a red marker where a teammate died, 16 units under the event, for
    // cg_drawDeadFriendTime; not in Red Rover or Freeze Tag
    if (cg_drawDeadFriendTime.value != 0.0f && cgs.gametype >= GT_TEAM &&
        cg.snap->ps.persistant[PERS_TEAM] == cgs.clientinfo[target].team && target != cg.clientNum &&
        cgs.gametype != GT_RR && cgs.gametype != GT_FREEZE) {
        floatingEffect_t* fx = CG_AllocFloatingEffect();

        if (fx) {
            fx->type = FE_DAMAGE_NUMBER;  // retail's type 0; the record draws its shader
            VectorCopy(ent->pos.trBase, fx->origin);
            fx->origin[2] -= 16;
            fx->color[0] = 1.0f;
            fx->color[1] = 0.0f;
            fx->color[2] = 0.0f;
            fx->color[3] = 1.0f;
            fx->zOffset = 64.0f;
            fx->worldSize = cg_teammatePOIsMaxWidth.value;
            fx->minPixels = cg_teammatePOIsMinWidth.value;
            fx->shader = cgs.media.friendDeadShader;
            fx->lifeRate = cg_drawDeadFriendTime.value;
            // [ioquakelive] retail lets it go out at once; it fades over its last quarter, the
            // way the powerup mark of the binary fades over what is left of its own life
            fx->fadeStart = fx->lifeRate * 0.75f;
            fx->doFade = 1;
        }
    }

    // [QL] Resolve weapon icon and add to obituary display buffer
    weaponIcon = CG_ModToWeaponIcon(mod);
    victimTeam = cgs.clientinfo[target].team;
    if (attacker >= 0 && attacker < MAX_CLIENTS && attacker != target) {
        attackerTeam = cgs.clientinfo[attacker].team;
        CG_AddObituary(cgs.clientinfo[target].name, cgs.clientinfo[attacker].name,
                       victimTeam, attackerTeam, 1, weaponIcon);
        CG_SpecFollowKiller(attacker);
    } else {
        // retail's adder copies the attacker name unconditionally, so it
        // never gets a NULL
        CG_AddObituary(cgs.clientinfo[target].name, "",
                       victimTeam, 0, 0, weaponIcon);
    }
    ci = &cgs.clientinfo[target];

    if (attacker < 0 || attacker >= MAX_CLIENTS) {
        attacker = ENTITYNUM_WORLD;
        attackerInfo = NULL;
    } else {
        attackerInfo = CG_ConfigString(CS_PLAYERS + attacker);
    }

    targetInfo = CG_ConfigString(CS_PLAYERS + target);
    if (!targetInfo) {
        return;
    }
    Q_strncpyz(targetName, Info_ValueForKey(targetInfo, "n"), sizeof(targetName) - 2);
    strcat(targetName, S_COLOR_WHITE);

    message2 = "";

    // check for single client messages

    switch (mod) {
        case MOD_SUICIDE:
            message = "suicides";
            break;
        case MOD_FALLING:
            message = "cratered";
            break;
        case MOD_CRUSH:
            message = "was squished";
            break;
        case MOD_WATER:
            message = "sank like a rock";
            break;
        case MOD_SLIME:
            message = "melted";
            break;
        case MOD_LAVA:
            message = "does a back flip into the lava";
            break;
        case MOD_TARGET_LASER:
            message = "saw the light";
            break;
        case MOD_TRIGGER_HURT:
            message = "was in the wrong place";
            break;
        // [QL] new environment/solo means-of-death
        case MOD_SWITCH_TEAMS:
            message = "switched teams";
            break;
        case MOD_THAW:
            // [QL] auto-thaw (no attacker) shows a solo message
            if (attacker == ENTITYNUM_WORLD || attacker < 0 || attacker >= MAX_CLIENTS) {
                message = "was auto-thawed";
            }
            break;
        default:
            message = NULL;
            break;
    }

    if (attacker == target) {
        gender = ci->gender;
        switch (mod) {
            case MOD_KAMIKAZE:
                message = "goes out with a bang";
                break;
            case MOD_GRENADE_SPLASH:
                if (gender == GENDER_FEMALE)
                    message = "tripped on her own grenade";
                else if (gender == GENDER_NEUTER)
                    message = "tripped on its own grenade";
                else
                    message = "tripped on his own grenade";
                break;
            case MOD_ROCKET_SPLASH:
                if (gender == GENDER_FEMALE)
                    message = "blew herself up";
                else if (gender == GENDER_NEUTER)
                    message = "blew itself up";
                else
                    message = "blew himself up";
                break;
            case MOD_PLASMA_SPLASH:
                if (gender == GENDER_FEMALE)
                    message = "melted herself";
                else if (gender == GENDER_NEUTER)
                    message = "melted itself";
                else
                    message = "melted himself";
                break;
            case MOD_BFG_SPLASH:
                message = "should have used a smaller gun";
                break;
            case MOD_PROXIMITY_MINE:
                if (gender == GENDER_FEMALE) {
                    message = "found her prox mine";
                } else if (gender == GENDER_NEUTER) {
                    message = "found its prox mine";
                } else {
                    message = "found his prox mine";
                }
                break;
            // [QL] lightning-gun water discharge self-kill
            case MOD_LIGHTNING_DISCHARGE:
                if (gender == GENDER_FEMALE)
                    message = "discharged herself";
                else if (gender == GENDER_NEUTER)
                    message = "discharged itself";
                else
                    message = "discharged himself";
                break;
            default:
                if (gender == GENDER_FEMALE)
                    message = "killed herself";
                else if (gender == GENDER_NEUTER)
                    message = "killed itself";
                else
                    message = "killed himself";
                break;
        }
    }

    if (message) {
        CG_Printf("%s %s.\n", targetName, message);
        return;
    }

    if (cg_drawFragMessages.integer && attacker == cg.snap->ps.clientNum) {
        CG_ShowFragMessage(target, mod, targetName);
    }

    // check for double client messages
    if (!attackerInfo) {
        attacker = ENTITYNUM_WORLD;
        strcpy(attackerName, "noname");
    } else {
        Q_strncpyz(attackerName, Info_ValueForKey(attackerInfo, "n"), sizeof(attackerName) - 2);
        strcat(attackerName, S_COLOR_WHITE);
        // check for kill messages about the current clientNum
        if (target == cg.snap->ps.clientNum) {
            Q_strncpyz(cg.killerName, attackerName, sizeof(cg.killerName));
            cg.killerIcon = weaponIcon;  // [QL] 0x10019824
        }
    }

    if (attacker != ENTITYNUM_WORLD) {
        switch (mod) {
            case MOD_GRAPPLE:
                message = "was caught by";
                break;
            case MOD_GAUNTLET:
                message = "was pummeled by";
                break;
            case MOD_MACHINEGUN:
                message = "was machinegunned by";
                break;
            case MOD_SHOTGUN:
                message = "was gunned down by";
                break;
            case MOD_GRENADE:
                message = "ate";
                message2 = "'s grenade";
                break;
            case MOD_GRENADE_SPLASH:
                message = "was shredded by";
                message2 = "'s shrapnel";
                break;
            case MOD_ROCKET:
                message = "ate";
                message2 = "'s rocket";
                break;
            case MOD_ROCKET_SPLASH:
                message = "almost dodged";
                message2 = "'s rocket";
                break;
            case MOD_PLASMA:
                message = "was melted by";
                message2 = "'s plasmagun";
                break;
            case MOD_PLASMA_SPLASH:
                message = "was melted by";
                message2 = "'s plasmagun";
                break;
            case MOD_RAILGUN:
            case MOD_RAILGUN_HEADSHOT:              // [QL]
                message = "was railed by";
                break;
            case MOD_LIGHTNING:
                message = "was electrocuted by";
                break;
            case MOD_BFG:
            case MOD_BFG_SPLASH:
                message = "was blasted by";
                message2 = "'s BFG";
                break;
            case MOD_NAIL:
                message = "was nailed by";
                break;
            case MOD_CHAINGUN:
                message = "got lead poisoning from";
                message2 = "'s Chaingun";
                break;
            case MOD_PROXIMITY_MINE:
                message = "was too close to";
                message2 = "'s Prox Mine";
                break;
            case MOD_KAMIKAZE:
                message = "falls to";
                message2 = "'s Kamikaze blast";
                break;
            case MOD_JUICED:
                message = "was juiced by";
                break;
            case MOD_TELEFRAG:
                message = "tried to invade";
                message2 = "'s personal space";
                break;
            // [QL] new means-of-death obituary lines
            case MOD_THAW:
                message = "was thawed by";
                break;
            case MOD_LIGHTNING_DISCHARGE:
                message = "was discharged by";
                break;
            case MOD_HMG:
                message = "was ripped up by";
                message2 = "'s HMG";
                break;
            default:
                message = "was killed by";
                break;
        }

        if (message) {
            CG_Printf("%s %s %s%s\n",
                      targetName, message, attackerName, message2);
            return;
        }
    }

    // we don't know what it was
    CG_Printf("%s died.\n", targetName);

    // QL binary: cg_followKiller.integer auto-follows the killer (vmCvar 0x10A64800)
    if (cg_followKiller.integer && target == cg.snap->ps.clientNum &&
        attacker != target && attacker >= 0 && attacker < MAX_CLIENTS) {
        trap_SendConsoleCommand(va("follow %d\n", attacker));
    }
}

//==========================================================================

/*
===============
CG_UseItem

[QL] cgamex86.dll 0x10019af0. An item number past the holdables is an error, not an empty
use. cg_useItemWarning sizes the "No item to use" print and cg_useItemMessage the "Use %s"
one: 1 is scale 0.5, 2 is 0.25, anything else prints nothing.
===============
*/
static void CG_UseItem(centity_t* cent) {
    int itemNum, clientNum;
    gitem_t* item;
    entityState_t* es;

    es = &cent->currentState;

    itemNum = (es->event & ~EV_EVENT_BITS) - EV_USE_ITEM0;
    if (itemNum < 0 || itemNum >= HI_NUM_HOLDABLE) {
        CG_Error("CG_UseItem: invalid item %d", itemNum);
    }

    // print a message if the local player
    if (es->number == cg.snap->ps.clientNum) {
        if (!itemNum) {
            if (cg_useItemWarning.integer == 1) {
                CG_CenterPrint("No item to use", 144, 0.5f);
            } else if (cg_useItemWarning.integer == 2) {
                CG_CenterPrint("No item to use", 144, 0.25f);
            }
        } else {
            item = BG_FindItemForHoldable(itemNum);
            if (cg_useItemMessage.integer == 1) {
                CG_CenterPrint(va("Use %s", item->pickup_name), 144, 0.5f);
            } else if (cg_useItemMessage.integer == 2) {
                CG_CenterPrint(va("Use %s", item->pickup_name), 144, 0.25f);
            }
        }
    }

    switch (itemNum) {
        case HI_NONE:
            trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.useNothingSound);
            break;

        case HI_MEDKIT:
            clientNum = es->clientNum;
            if ((unsigned)clientNum < MAX_CLIENTS) {
                cgs.clientinfo[clientNum].medkitUsageTime = cg.time;
            }
            trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.medkitSound);
            break;

        case HI_INVULNERABILITY:
            trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.useInvulnerabilitySound);
            break;
    }
}

/*
================
CG_ItemPickup

A new item was picked up this frame
[QL] cgamex86.dll 0x10019c40. The same item as the one the pickup display drew last counts
up, shown as "x%i"; any other starts at 1. cg_autoswitch selects every weapon picked up,
the machinegun included, where Quake 3 skipped the machinegun.
================
*/
static void CG_ItemPickup(int itemNum) {
    cg.itemPickup = itemNum;
    cg.itemPickupTime = cg.time;
    cg.itemPickupBlendTime = cg.time;
    if (itemNum == cg.itemPickupLast) {
        cg.itemPickupCount++;
    } else {
        cg.itemPickupCount = 1;
    }
    // see if it should be the grabbed weapon
    if (bg_itemlist[itemNum].giType == IT_WEAPON && cg_autoswitch.integer) {
        // select it immediately
        cg.weaponSelectTime = cg.time;
        cg.weaponSelect = bg_itemlist[itemNum].giTag;
    }
}

/*
================
CG_WaterLevel

Returns waterlevel for entity origin
================
*/
int CG_WaterLevel(centity_t* cent) {
    vec3_t point;
    int contents, sample1, sample2, anim, waterlevel;
    int viewheight;

    anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;

    if (anim == LEGS_WALKCR || anim == LEGS_IDLECR) {
        viewheight = CROUCH_VIEWHEIGHT;
    } else {
        viewheight = DEFAULT_VIEWHEIGHT;
    }

    //
    // get waterlevel, accounting for ducking
    //
    waterlevel = 0;

    point[0] = cent->lerpOrigin[0];
    point[1] = cent->lerpOrigin[1];
    point[2] = cent->lerpOrigin[2] + MINS_Z + 1;
    contents = CG_PointContents(point, -1);

    if (contents & MASK_WATER) {
        sample2 = viewheight - MINS_Z;
        sample1 = sample2 / 2;
        waterlevel = 1;
        point[2] = cent->lerpOrigin[2] + MINS_Z + sample1;
        contents = CG_PointContents(point, -1);

        if (contents & MASK_WATER) {
            waterlevel = 2;
            point[2] = cent->lerpOrigin[2] + MINS_Z + sample2;
            contents = CG_PointContents(point, -1);

            if (contents & MASK_WATER) {
                waterlevel = 3;
            }
        }
    }

    return waterlevel;
}

/*
================
CG_PainEvent

[QL] cgamex86.dll 0x10019ca0. Retail has no underwater gurp: the pain sound plays either way.
================
*/
void CG_PainEvent(centity_t* cent, int health) {
    char* snd;

    // [QL] 0x10019ca6: every pain event, before the throttle, for the teammate markers
    cent->pe.lastPainTime = cg.time;

    // don't do more than two pain sounds a second
    if (cg.time - cent->pe.painTime < 500) {
        return;
    }

    if (health < 25) {
        snd = "*pain25_1.wav";
    } else if (health < 50) {
        snd = "*pain50_1.wav";
    } else if (health < 75) {
        snd = "*pain75_1.wav";
    } else {
        snd = "*pain100_1.wav";
    }
    trap_S_StartSound(NULL, cent->currentState.number, CHAN_VOICE, CG_CustomSound(cent->currentState.number, snd));

    // save pain time for programitic twitch animation
    cent->pe.painDirection ^= 1;
    cent->pe.painTime = cg.time;
}

/*
===============
CG_SpecItemTimerEvent

[QL] cgamex86.dll 0x10019d90, EV_ITEM_PICKUP_SPEC: the server's word on an item's timer, for a
spectator. A known item (otherEntityNum) takes the new values, its team mark doubled when it has
just respawned; a new one takes the first free record, up to ten.
===============
*/
void CG_SpecItemTimerEvent(entityState_t* es) {
    specItemTimer_t* timer;
    int i, freeSlot = -1;

    for (i = 0; i < MAX_SPEC_ITEM_TIMERS; i++) {
        timer = &cg.specItemTimers[i];
        if (freeSlot == -1 && !timer->entityNum) {
            freeSlot = i;
        }
        if (timer->entityNum == es->otherEntityNum) {
            timer->side = es->otherEntityNum2;
            timer->time = es->time - cg.time;
            timer->itemIndex = es->modelindex;
            timer->team = (es->groundEntityNum > 0) ? es->modelindex2 * 2 : es->modelindex2;
            timer->distance = es->time2;
            VectorCopy(es->pos.trBase, timer->origin);
            return;
        }
    }

    if (cg.numSpecItemTimers >= MAX_SPEC_ITEM_TIMERS) {
        cg.numSpecItemTimers = MAX_SPEC_ITEM_TIMERS;
        return;
    }
    if (freeSlot == -1) {
        return;
    }
    timer = &cg.specItemTimers[freeSlot];
    timer->entityNum = es->otherEntityNum;
    timer->side = es->otherEntityNum2;
    timer->itemIndex = es->modelindex;
    timer->team = es->modelindex2;
    timer->time = es->time - cg.time;
    timer->distance = es->time2;
    VectorCopy(es->pos.trBase, timer->origin);
    cg.numSpecItemTimers++;
}

/*
===============
CG_CompareSpecItemTimers

[QL] cgamex86.dll 0x1001ca10. In CTF by side (items with no duplicate first, then blue, then
red), nearer the base first, then by team mark or else higher first, then by entity number;
elsewhere higher first, then by y and by x.
===============
*/
static int QDECL CG_CompareSpecItemTimers(const void* a, const void* b) {
    const specItemTimer_t* ta = (const specItemTimer_t*)a;
    const specItemTimer_t* tb = (const specItemTimer_t*)b;
    int d;

    if (cgs.gametype == GT_CTF) {
        d = tb->side - ta->side;
        if (d) {
            return d;
        }
        d = ta->distance - tb->distance;
        if (d) {
            return d;
        }
        if (ta->team && tb->team) {
            d = ta->team - tb->team;
        } else {
            d = (int)tb->origin[2] - (int)ta->origin[2];
        }
        if (d) {
            return d;
        }
        return tb->entityNum - ta->entityNum;
    }
    d = (int)tb->origin[2] - (int)ta->origin[2];
    if (d) {
        return d;
    }
    d = (int)tb->origin[1] - (int)ta->origin[1];
    if (d) {
        return d;
    }
    return (int)tb->origin[0] - (int)ta->origin[0];
}

/*
===============
CG_UpdateSpecItemTimers

[QL] cgamex86.dll 0x1001ca90, once a frame while spectating: every timer counts down by the
frame time, held during a timeout, and the list is sorted
===============
*/
void CG_UpdateSpecItemTimers(void) {
    int i;

    if (!(cg.predictedPlayerState.eFlags & EF_SPECTATOR)) {
        return;
    }
    for (i = 0; i < MAX_SPEC_ITEM_TIMERS; i++) {
        if (cg.specItemTimers[i].entityNum && !cgs.freezeEnd) {
            if (cg.specItemTimers[i].time > 0) {
                cg.specItemTimers[i].time -= cg.frametime;
            } else {
                cg.specItemTimers[i].time = 0;
            }
        }
    }
    qsort(cg.specItemTimers, cg.numSpecItemTimers, sizeof(cg.specItemTimers[0]), CG_CompareSpecItemTimers);
}

/*
===============
CG_ResetSpecItemTimers

[QL] cgamex86.dll 0x1001c8d0, at the end of map_restart
===============
*/
void CG_ResetSpecItemTimers(void) {
    memset(cg.specItemTimers, 0, sizeof(cg.specItemTimers));
    cg.numSpecItemTimers = 0;
}

/*
==============
CG_PointOfInterest

[QL] case 0x5a of CG_EntityEvent. The server broadcasts this where a player who has to
be given away is standing - the Quad Hog, or whoever carries the neutral flag - and it
becomes one sprite of the floating-effect pool, which already keeps a marker on screen
through the walls and fades it with distance.

es->powerups carries how long the marker lives, and the fade starts at a quarter of
that. es->generic1 is the client for the Quad Hog and the team for a flag, which is why
the two go into different fields: followNum hides the marker from its own team,
clientNum from its own player. Which of the three it is follows CS_CUSTOM_SETTINGS,
because a match only ever emits one kind.
==============
*/
static void CG_PointOfInterest(centity_t* cent) {
    entityState_t* es = &cent->currentState;
    floatingEffect_t* fx = CG_AllocFloatingEffect();

    if (!fx) {
        return;
    }

    fx->type = FE_SPRITE;
    fx->owner = cent;
    VectorCopy(es->pos.trBase, fx->origin);
    fx->lifeRate = (float)es->powerups;
    fx->fadeStart = (float)(es->powerups / 4);
    fx->doFade = 1;
    fx->color[3] = 1.0f;

    if (cgs.customSettings & 0x40000) {
        // the hog is gold, and hidden from the hog
        fx->color[0] = 1.0f;
        fx->color[1] = 0.75f;
        fx->color[2] = 0.0f;
        fx->shader = cgs.media.quadHogPoiShader;
        fx->clientNum = es->generic1;
        fx->zOffset = 48.0f;
        fx->worldSize = cg_poiMaxWidth.value;
        fx->minPixels = cg_poiMinWidth.value;
    } else if (cgs.customSettings & 0x4000000) {
        fx->color[0] = 1.0f;
        fx->color[1] = 1.0f;
        fx->color[2] = 1.0f;
        fx->shader = cgs.media.infectedPoiShader;
        fx->followNum = es->generic1;
        fx->zOffset = 48.0f;
        fx->worldSize = cg_poiMaxWidth.value;
        fx->minPixels = cg_poiMinWidth.value;
    } else {
        // the neutral flag carrier: higher up and three quarters the size
        fx->color[0] = 1.0f;
        fx->color[1] = 1.0f;
        fx->color[2] = 1.0f;
        fx->shader = cgs.media.poiShader;
        fx->followNum = es->generic1;
        fx->zOffset = 64.0f;
        fx->worldSize = cg_poiMaxWidth.value * 0.75f;
        fx->minPixels = cg_poiMinWidth.value * 0.75f;
    }
}

/*
==============
CG_EntityEvent

An entity has an event value
also called by CG_CheckPlayerstateEvents
==============
*/
#define DEBUGNAME(x)              \
    if (cg_debugEvents.integer) { \
        CG_Printf(x "\n");        \
    }
void CG_EntityEvent(centity_t* cent, vec3_t position) {
    entityState_t* es;
    int event;
    vec3_t dir;
    const char* s;
    int clientNum;
    clientInfo_t* ci;

    es = &cent->currentState;
    event = es->event & ~EV_EVENT_BITS;

    if (cg_debugEvents.integer) {
        CG_Printf("ent:%3i  event:%3i ", es->number, event);
    }

    if (!event) {
        DEBUGNAME("ZEROEVENT");
        return;
    }

    clientNum = es->clientNum;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = 0;
    }
    ci = &cgs.clientinfo[clientNum];

    switch (event) {
        //
        // movement generated events
        //
        case EV_FOOTSTEP:
            DEBUGNAME("EV_FOOTSTEP");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                  cgs.media.footsteps[ci->footsteps][rand() & 3]);
            }
            break;
        case EV_FOOTSTEP_METAL:
            DEBUGNAME("EV_FOOTSTEP_METAL");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                  cgs.media.footsteps[FOOTSTEP_METAL][rand() & 3]);
            }
            break;
        case EV_FOOTSPLASH:
            DEBUGNAME("EV_FOOTSPLASH");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                  cgs.media.footsteps[FOOTSTEP_SPLASH][rand() & 3]);
            }
            break;
        case EV_FOOTWADE:
            DEBUGNAME("EV_FOOTWADE");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                  cgs.media.footsteps[FOOTSTEP_SPLASH][rand() & 3]);
            }
            break;
        case EV_SWIM:
            DEBUGNAME("EV_SWIM");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                  cgs.media.footsteps[FOOTSTEP_SPLASH][rand() & 3]);
            }
            break;

        case EV_FALL_SHORT:
            DEBUGNAME("EV_FALL_SHORT");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.landSound);
            if (clientNum == cg.predictedPlayerState.clientNum) {
                // smooth landing z changes
                cg.landChange = -8;
                cg.landTime = cg.time;
            }
            break;
        case EV_FALL_MEDIUM:
            DEBUGNAME("EV_FALL_MEDIUM");
            // use normal pain sound
            trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, "*pain100_1.wav"));
            if (clientNum == cg.predictedPlayerState.clientNum) {
                // smooth landing z changes
                cg.landChange = -16;
                cg.landTime = cg.time;
            }
            break;
        case EV_FALL_FAR:
            DEBUGNAME("EV_FALL_FAR");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, CG_CustomSound(es->number, "*fall1.wav"));
            cent->pe.painTime = cg.time;  // don't play a pain sound right after this
            if (clientNum == cg.predictedPlayerState.clientNum) {
                // smooth landing z changes
                cg.landChange = -24;
                cg.landTime = cg.time;
            }
            break;

        // [QL] EV_STEP_4/8/12/16 removed - QL uses playerState_t for step smoothing.
        // Step smoothing is now handled in CG_PlayerStateToEntityState or CG_TransitionPlayerState.

        case EV_JUMP_PAD:
            DEBUGNAME("EV_JUMP_PAD");
            //		CG_Printf( "EV_JUMP_PAD w/effect #%i\n", es->eventParm );
            {
                vec3_t up = {0, 0, 1};

                CG_SmokePuff(cent->lerpOrigin, up,
                             32,
                             1, 1, 1, 0.33f,
                             1000,
                             cg.time, 0,
                             LEF_PUFF_DONT_SCALE,
                             cgs.media.smokePuffShader);
            }

            // boing sound at origin, jump sound on player
            // [QL] 0x1001a1f0: the pad sound belongs to the world
            trap_S_StartSound(cent->lerpOrigin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.jumpPadSound);
            trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, "*jump1.wav"));
            break;

        case EV_JUMP:
            DEBUGNAME("EV_JUMP");
            trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, "*jump1.wav"));
            break;
        case EV_TAUNT:
            DEBUGNAME("EV_TAUNT");
            // [QL] cg_allowTaunt gates the taunt sound (binary tests DAT_10a68eec, exact-nonzero)
            if (cg_allowTaunt.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, "*taunt.wav"));
            }
            break;

        // [QL] Voice-taunt events (0x4b-0x50). Each enqueues a voice chat for the
        // taunting client via CG_VoiceChatLocal(client, token). Binary passes the
        // client in es->number (0x0, [EBP]) NOT es->clientNum. Binary gates
        // EV_TAUNT_YES/EV_TAUNT_NO on cg_allowTaunt; the four order taunts are
        // never gated. Tokens are the literal voice-chat command names.
        case EV_TAUNT_YES:                      // 0x4b
            DEBUGNAME("EV_TAUNT_YES");
            if (cg_allowTaunt.integer) {
                CG_VoiceChatLocal(es->number, "yes");
            }
            break;
        case EV_TAUNT_NO:                       // 0x4c
            DEBUGNAME("EV_TAUNT_NO");
            if (cg_allowTaunt.integer) {
                CG_VoiceChatLocal(es->number, "no");
            }
            break;
        case EV_TAUNT_FOLLOWME:                 // 0x4d
            DEBUGNAME("EV_TAUNT_FOLLOWME");
            CG_VoiceChatLocal(es->number, "followme");
            break;
        case EV_TAUNT_GETFLAG:                  // 0x4e
            DEBUGNAME("EV_TAUNT_GETFLAG");
            CG_VoiceChatLocal(es->number, "ongetflag");
            break;
        case EV_TAUNT_GUARDBASE:                // 0x4f
            DEBUGNAME("EV_TAUNT_GUARDBASE");
            CG_VoiceChatLocal(es->number, "ondefense");
            break;
        case EV_TAUNT_PATROL:                   // 0x50
            DEBUGNAME("EV_TAUNT_PATROL");
            CG_VoiceChatLocal(es->number, "onpatrol");
            break;
        case EV_WATER_TOUCH:
            DEBUGNAME("EV_WATER_TOUCH");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.watrInSound);
            break;
        case EV_WATER_LEAVE:
            DEBUGNAME("EV_WATER_LEAVE");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.watrOutSound);
            break;
        case EV_WATER_UNDER:
            DEBUGNAME("EV_WATER_UNDER");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.watrUnSound);
            break;
        case EV_WATER_CLEAR:
            DEBUGNAME("EV_WATER_CLEAR");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, CG_CustomSound(es->number, "*gasp.wav"));
            break;

        case EV_ITEM_PICKUP:
            DEBUGNAME("EV_ITEM_PICKUP");
            {
                gitem_t* item;
                int index;

                index = es->eventParm;  // player predicted

                if (index < 1 || index >= bg_numItems) {
                    CG_Error("CG_EntityEvent: invalid item index %d\n", index);
                }
                item = &bg_itemlist[index];

                // powerups and team items will have a separate global sound, this one
                // will be played at prediction time
                if (item->giType == IT_POWERUP || item->giType == IT_TEAM) {
                    trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.n_healthSound);
                } else {
                    trap_S_StartSound(NULL, es->number, CHAN_AUTO, trap_S_RegisterSound(item->pickup_sound, qfalse));
                }

                // [QL] 0x1001a553: a rune's name is announced, by the rune's number
                if (item->giType == IT_PERSISTANT_POWERUP) {
                    switch (item->giTag) {
                        case 1:
                            CG_AddBufferedSound(cgs.media.scoutVoSound);
                            break;
                        case 2:
                            CG_AddBufferedSound(cgs.media.guardVoSound);
                            break;
                        case 3:
                            CG_AddBufferedSound(cgs.media.damageVoSound);
                            break;
                        case 4:
                            CG_AddBufferedSound(cgs.media.armorRegenVoSound);
                            break;
                    }
                }

                // [CPMA] watching, what the others pick up goes in the feed, and only what a
                // match turns on: the armours that are not a shard, the mega health and the
                // powerups, runes included. The small healths and the shards stay out
                if (es->number != cg.snap->ps.clientNum && CG_Watching() &&
                    (item->giType == IT_POWERUP || item->giType == IT_PERSISTANT_POWERUP ||
                     (item->giType == IT_ARMOR && item->giTag >= 1 && item->giTag <= 3) ||
                     (item->giType == IT_HEALTH && item->quantity >= 100))) {
                    CG_AddGameEvent(es->number, cg_items[index].icon, "+");
                }

                // show icon and name on status bar
                if (es->number == cg.snap->ps.clientNum) {
                    CG_ItemPickup(index);
                }
            }
            break;

        case EV_GLOBAL_ITEM_PICKUP:
            DEBUGNAME("EV_GLOBAL_ITEM_PICKUP");
            {
                gitem_t* item;
                int index;

                index = es->eventParm;  // player predicted

                if (index < 1 || index >= bg_numItems) {
                    CG_Error("CG_EntityEvent: invalid item index %d\n", index);
                }
                item = &bg_itemlist[index];
                // powerup pickups are global
                if (item->pickup_sound) {
                    trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_AUTO, trap_S_RegisterSound(item->pickup_sound, qfalse));
                }

                // [QL] 0x1001a61d: the powerup's name is announced unless the server's custom
                // settings turn that off, and a spectator set to follow powerups follows the
                // player who took it
                if (cgs.customSettings & 0x40000) {
                    break;
                }
                switch (item->giTag) {
                    case PW_QUAD:
                        CG_AddBufferedSound(cgs.media.quadDamageVoSound);
                        break;
                    case PW_BATTLESUIT:
                        CG_AddBufferedSound(cgs.media.battleSuitVoSound);
                        break;
                    case PW_HASTE:
                        CG_AddBufferedSound(cgs.media.hasteVoSound);
                        break;
                    case PW_INVIS:
                        CG_AddBufferedSound(cgs.media.invisibilityVoSound);
                        break;
                    case PW_REGEN:
                        CG_AddBufferedSound(cgs.media.regenerationVoSound);
                        break;
                }
                if (item->giType == IT_POWERUP) {
                    CG_SpecAutoFollow(es->otherEntityNum, 0);
                }

                // show icon and name on status bar
                if (es->number == cg.snap->ps.clientNum) {
                    CG_ItemPickup(index);
                }
            }
            break;

        //
        // weapon events
        //
        case EV_NOAMMO:
            DEBUGNAME("EV_NOAMMO");
            if (es->number == cg.snap->ps.clientNum) {
                // QL binary: cg_switchOnEmpty.integer gates auto-switch (vmCvar 0x10A673E0)
                if (cg_switchOnEmpty.integer) {
                    CG_OutOfAmmoChange();
                } else {
                    trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.noAmmoSound);
                }
            }
            break;
        case EV_CHANGE_WEAPON:
            DEBUGNAME("EV_CHANGE_WEAPON");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.selectSound);
            break;
        case EV_DROP_WEAPON:
            DEBUGNAME("EV_DROP_WEAPON");
            // [QL] 0x1001a723: eventParm is the weapon that was dropped
            if (es->clientNum == cg.clientNum) {
                CG_DropWeaponChange(es->eventParm);
            }
            break;
        case EV_FIRE_WEAPON:
            DEBUGNAME("EV_FIRE_WEAPON");
            CG_FireWeapon(cent);
            break;

        case EV_USE_ITEM0:
            DEBUGNAME("EV_USE_ITEM0");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM1:
            DEBUGNAME("EV_USE_ITEM1");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM2:
            DEBUGNAME("EV_USE_ITEM2");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM3:
            DEBUGNAME("EV_USE_ITEM3");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM4:
            DEBUGNAME("EV_USE_ITEM4");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM5:
            DEBUGNAME("EV_USE_ITEM5");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM6:
            DEBUGNAME("EV_USE_ITEM6");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM7:
            DEBUGNAME("EV_USE_ITEM7");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM8:
            DEBUGNAME("EV_USE_ITEM8");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM9:
            DEBUGNAME("EV_USE_ITEM9");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM10:
            DEBUGNAME("EV_USE_ITEM10");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM11:
            DEBUGNAME("EV_USE_ITEM11");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM12:
            DEBUGNAME("EV_USE_ITEM12");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM13:
            DEBUGNAME("EV_USE_ITEM13");
            CG_UseItem(cent);
            break;
        case EV_USE_ITEM14:
            DEBUGNAME("EV_USE_ITEM14");
            CG_UseItem(cent);
            break;
        // [QL] retail has no EV_USE_ITEM15 case: it lands on "Unknown event"

        //=================================================================

        //
        // other events
        //
        case EV_PLAYER_TELEPORT_IN:
            DEBUGNAME("EV_PLAYER_TELEPORT_IN");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.teleInSound);
            CG_SpawnEffect(position);
            break;

        case EV_PLAYER_TELEPORT_OUT:
            DEBUGNAME("EV_PLAYER_TELEPORT_OUT");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.teleOutSound);
            CG_SpawnEffect(position);
            break;

        case EV_ITEM_POP:
            DEBUGNAME("EV_ITEM_POP");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.respawnSound);
            break;
        case EV_ITEM_RESPAWN:
            DEBUGNAME("EV_ITEM_RESPAWN");
            cent->miscTime = cg.time;  // scale up from this
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.respawnSound);
            break;

        case EV_GRENADE_BOUNCE:
            DEBUGNAME("EV_GRENADE_BOUNCE");
            if (rand() & 1) {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.hgrenb1aSound);
            } else {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.hgrenb2aSound);
            }
            break;

        case EV_PROXIMITY_MINE_STICK:
            DEBUGNAME("EV_PROXIMITY_MINE_STICK");
            if (es->eventParm & SURF_FLESH) {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.wstbimplSound);
            } else if (es->eventParm & SURF_METALSTEPS) {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.wstbimpmSound);
            } else {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.wstbimpdSound);
            }
            break;

        case EV_PROXIMITY_MINE_TRIGGER:
            DEBUGNAME("EV_PROXIMITY_MINE_TRIGGER");
            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.wstbactvSound);
            break;
        case EV_KAMIKAZE:
            DEBUGNAME("EV_KAMIKAZE");
            CG_KamikazeEffect(cent->lerpOrigin);
            break;
        case EV_OBELISKEXPLODE:
            DEBUGNAME("EV_OBELISKEXPLODE");
            CG_ObeliskExplode(cent->lerpOrigin, es->eventParm);
            break;
        case EV_OBELISKPAIN:
            DEBUGNAME("EV_OBELISKPAIN");
            CG_ObeliskPain(cent->lerpOrigin);
            break;
        case EV_INVUL_IMPACT:
            DEBUGNAME("EV_INVUL_IMPACT");
            CG_InvulnerabilityImpact(cent->lerpOrigin, cent->currentState.angles);
            break;
        case EV_JUICED:
            DEBUGNAME("EV_JUICED");
            // [QL] 0x1001ab39: the gore pack's skull handle picks the effect, as in
            // CG_GibPlayer. With the pack the player swells in the juicer model
            // (0x10013920) and bursts after five seconds; without it they gib now.
            if (cgs.media.gibSkull) {
                CG_InvulnerabilityJuiced(cent->lerpOrigin);
            } else {
                CG_GibPlayer(cent->lerpOrigin, NULL);
            }
            break;
        case EV_LIGHTNINGBOLT:
            DEBUGNAME("EV_LIGHTNINGBOLT");
            CG_LightningBoltBeam(es->origin2, es->pos.trBase);
            break;
        case EV_SCOREPLUM:
            DEBUGNAME("EV_SCOREPLUM");
            // [QL] binary guards on the local player before spawning the plum
            // (CG_EntityEvent 0x40: snap ps.clientNum == es->clientNum 0xb0).
            // Score value rides es->time (0x5c).
            if (cg.snap->ps.clientNum == es->clientNum) {
                CG_ScorePlum(cent->currentState.otherEntityNum, cent->lerpOrigin, cent->currentState.time);
            }
            break;

        //
        // missile impacts
        //
        case EV_MISSILE_HIT:
            DEBUGNAME("EV_MISSILE_HIT");
            ByteToDir(es->eventParm, dir);
            CG_MissileHitPlayer(es->weapon, position, dir, es->otherEntityNum);
            break;

        case EV_MISSILE_MISS:
            DEBUGNAME("EV_MISSILE_MISS");
            ByteToDir(es->eventParm, dir);
            CG_MissileHitWall(es->weapon, 0, position, dir, IMPACTSOUND_DEFAULT);
            break;

        case EV_MISSILE_MISS_METAL:
            DEBUGNAME("EV_MISSILE_MISS_METAL");
            ByteToDir(es->eventParm, dir);
            CG_MissileHitWall(es->weapon, 0, position, dir, IMPACTSOUND_METAL);
            break;

        case EV_RAILTRAIL:
            DEBUGNAME("EV_RAILTRAIL");
            cent->currentState.weapon = WP_RAILGUN;

            // [QL] 0x1001b0bd: CG_CheckAutoFire already drew the local player's own shot,
            // trail and impact, when it was predicted; never in a demo (0x1001b0c6), where
            // nothing predicts it
            if (!cg_nopredict.integer && !cg.demoPlayback && !cg.renderingThirdPerson &&
                cg_predictLocalRailshots.integer &&
                !(cg.snap && cg.snap->ps.pm_type == PM_SPECTATOR) &&
                cg.predictedPlayerEntity.currentState.clientNum == es->clientNum) {
                break;
            }

            CG_RailTrail(ci, es->origin2, es->pos.trBase);

            // if the end was on a nomark surface, don't make an explosion
            if (es->eventParm != 255) {
                ByteToDir(es->eventParm, dir);
                CG_MissileHitWall(es->weapon, es->clientNum, es->pos.trBase, dir, IMPACTSOUND_DEFAULT);
            }
            break;

        case EV_BULLET_HIT_WALL:
            DEBUGNAME("EV_BULLET_HIT_WALL");
            ByteToDir(es->eventParm, dir);
            CG_Bullet(es->pos.trBase, es->otherEntityNum, dir, qfalse, ENTITYNUM_WORLD);
            break;

        case EV_BULLET_HIT_FLESH:
            DEBUGNAME("EV_BULLET_HIT_FLESH");
            CG_Bullet(es->pos.trBase, es->otherEntityNum, dir, qtrue, es->eventParm);
            break;

        case EV_SHOTGUN:
            DEBUGNAME("EV_SHOTGUN");
            CG_ShotgunFire(es);
            break;

        case EV_GENERAL_SOUND:
            DEBUGNAME("EV_GENERAL_SOUND");
            if (cgs.gameSounds[es->eventParm]) {
                trap_S_StartSound(NULL, es->number, CHAN_VOICE, cgs.gameSounds[es->eventParm]);
            } else {
                s = CG_ConfigString(CS_SOUNDS + es->eventParm);
                trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, s));
            }
            break;

        case EV_GLOBAL_SOUND:  // play from the player's head so it never diminishes
            DEBUGNAME("EV_GLOBAL_SOUND");
            if (cgs.gameSounds[es->eventParm]) {
                trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[es->eventParm]);
            } else {
                s = CG_ConfigString(CS_SOUNDS + es->eventParm);
                trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound(es->number, s));
            }
            break;

        case EV_GLOBAL_TEAM_SOUND:  // play from the player's head so it never diminishes
        {
            // [QL] 0x1001b33a. "team" is the local client's team as the scoreboard knows it.
            int team = cgs.clientinfo[cg.clientNum].team;

            DEBUGNAME("EV_GLOBAL_TEAM_SOUND");
            switch (es->eventParm) {
                case GTS_RED_CAPTURE:  // CTF: red team captured the blue flag, 1FCTF: red team captured the neutral flag
                    trap_S_StartLocalSound(team == TEAM_RED ? cgs.media.captureYourTeamSound : cgs.media.captureOpponentSound,
                                           CHAN_LOCAL_SOUND);
                    // the number comes one up, and a zero means the server did not send it
                    CG_AddGameEvent(es->otherEntityNum - 1,
                                    CG_FlagIcon(cgs.gametype == GT_1FCTF ? PW_NEUTRALFLAG : PW_BLUEFLAG), "captured");
                    break;
                case GTS_BLUE_CAPTURE:  // CTF: blue team captured the red flag, 1FCTF: blue team captured the neutral flag
                    trap_S_StartLocalSound(team == TEAM_BLUE ? cgs.media.captureYourTeamSound : cgs.media.captureOpponentSound,
                                           CHAN_LOCAL_SOUND);
                    CG_AddGameEvent(es->otherEntityNum - 1,
                                    CG_FlagIcon(cgs.gametype == GT_1FCTF ? PW_NEUTRALFLAG : PW_REDFLAG), "captured");
                    break;
                case GTS_RED_RETURN:  // CTF: blue flag returned, 1FCTF: never used
                    if (team == TEAM_BLUE) {
                        trap_S_StartLocalSound(cgs.media.returnOpponentSound, CHAN_LOCAL_SOUND);
                        CG_AddBufferedSound(cgs.media.yourFlagReturnedSound);
                    } else if (team == TEAM_RED) {
                        trap_S_StartLocalSound(cgs.media.returnYourTeamSound, CHAN_LOCAL_SOUND);
                        CG_AddBufferedSound(cgs.media.enemyFlagReturnedSound);
                    } else {
                        CG_AddBufferedSound(cgs.media.blueFlagReturnedSound);
                    }
                    break;
                case GTS_BLUE_RETURN:  // CTF red flag returned, 1FCTF: neutral flag returned
                    if (team == TEAM_RED) {
                        trap_S_StartLocalSound(cgs.media.returnOpponentSound, CHAN_LOCAL_SOUND);
                        CG_AddBufferedSound(cgs.media.yourFlagReturnedSound);
                    } else if (team == TEAM_BLUE) {
                        trap_S_StartLocalSound(cgs.media.returnYourTeamSound, CHAN_LOCAL_SOUND);
                        CG_AddBufferedSound(cgs.media.enemyFlagReturnedSound);
                    } else {
                        CG_AddBufferedSound(cgs.media.redFlagReturnedSound);
                    }
                    break;

                case GTS_RED_TAKEN:  // CTF: red team took blue flag, 1FCTF: blue team took the neutral flag
                    CG_SpecAutoFollow(es->otherEntityNum, 1);
                    CG_AddGameEvent(es->otherEntityNum,
                                    CG_FlagIcon(cgs.gametype == GT_1FCTF ? PW_NEUTRALFLAG : PW_BLUEFLAG),
                                    "took the flag");
                    // if this player picked up the flag then a sound is played in CG_CheckLocalSounds
                    if (cg.snap->ps.powerups[PW_BLUEFLAG] || cg.snap->ps.powerups[PW_NEUTRALFLAG]) {
                        break;
                    }
                    if (team == TEAM_BLUE) {
                        if (cgs.gametype == GT_1FCTF) {
                            trap_S_StartLocalSound(cgs.media.returnYourTeamSound, CHAN_LOCAL_SOUND);
                        } else {
                            CG_AddBufferedSound(cgs.media.enemyTookYourFlagSound);
                        }
                    } else if (team == TEAM_RED) {
                        if (cgs.gametype == GT_1FCTF) {
                            trap_S_StartLocalSound(cgs.media.returnOpponentSound, CHAN_LOCAL_SOUND);
                        } else {
                            CG_AddBufferedSound(cgs.media.yourTeamTookEnemyFlagSound);
                        }
                    } else {
                        CG_AddBufferedSound(cgs.media.blueFlagTakenSound);
                    }
                    break;
                case GTS_BLUE_TAKEN:  // CTF: blue team took the red flag, 1FCTF red team took the neutral flag
                    CG_SpecAutoFollow(es->otherEntityNum, 1);
                    CG_AddGameEvent(es->otherEntityNum,
                                    CG_FlagIcon(cgs.gametype == GT_1FCTF ? PW_NEUTRALFLAG : PW_REDFLAG),
                                    "took the flag");
                    // if this player picked up the flag then a sound is played in CG_CheckLocalSounds
                    if (cg.snap->ps.powerups[PW_REDFLAG] || cg.snap->ps.powerups[PW_NEUTRALFLAG]) {
                        break;
                    }
                    if (team == TEAM_RED) {
                        if (cgs.gametype == GT_1FCTF) {
                            trap_S_StartLocalSound(cgs.media.returnYourTeamSound, CHAN_LOCAL_SOUND);
                        } else {
                            CG_AddBufferedSound(cgs.media.enemyTookYourFlagSound);
                        }
                    } else if (team == TEAM_BLUE) {
                        if (cgs.gametype == GT_1FCTF) {
                            trap_S_StartLocalSound(cgs.media.returnOpponentSound, CHAN_LOCAL_SOUND);
                        } else {
                            CG_AddBufferedSound(cgs.media.yourTeamTookEnemyFlagSound);
                        }
                    } else {
                        CG_AddBufferedSound(cgs.media.redFlagTakenSound);
                    }
                    break;

                case GTS_REDOBELISK_ATTACKED:  // Overload: red obelisk is being attacked
                    if (team == TEAM_RED) {
                        CG_AddBufferedSound(cgs.media.yourBaseIsUnderAttackSound);
                    }
                    break;
                case GTS_BLUEOBELISK_ATTACKED:  // Overload: blue obelisk is being attacked
                    if (team == TEAM_BLUE) {
                        CG_AddBufferedSound(cgs.media.yourBaseIsUnderAttackSound);
                    }
                    break;

                case GTS_REDTEAM_SCORED:
                    if (cgs.gametype == GT_FREEZE) {
                        CG_CenterPrint("Red Scores", 90, 0.5f);
                    }
                    if (cgs.gametype == GT_AD) {
                        break;
                    }
                    if (team == TEAM_RED) {
                        CG_AddBufferedSound(cgs.media.yourTeamScoredSound);
                    } else if (team == TEAM_BLUE) {
                        CG_AddBufferedSound(cgs.media.enemyScoredSound);
                    } else {
                        CG_AddBufferedSound(cgs.media.redScoredSound);
                    }
                    break;
                case GTS_BLUETEAM_SCORED:
                    if (cgs.gametype == GT_FREEZE) {
                        CG_CenterPrint("Blue Scores", 90, 0.5f);
                    }
                    if (cgs.gametype == GT_AD) {
                        break;
                    }
                    if (team == TEAM_BLUE) {
                        CG_AddBufferedSound(cgs.media.yourTeamScoredSound);
                    } else if (team == TEAM_RED) {
                        CG_AddBufferedSound(cgs.media.enemyScoredSound);
                    } else {
                        CG_AddBufferedSound(cgs.media.blueScoredSound);
                    }
                    break;
                case GTS_REDTEAM_TOOK_LEAD:
                    if (cgs.gametype == GT_AD || !cg_announcerLeadsVO.integer) {
                        break;
                    }
                    if (team == TEAM_RED) {
                        CG_AddBufferedSound(cgs.media.takenLeadSound);
                    } else if (team == TEAM_BLUE) {
                        CG_AddBufferedSound(cgs.media.lostLeadSound);
                    } else {
                        CG_AddBufferedSound(cgs.media.redLeadsSound);
                    }
                    break;
                case GTS_BLUETEAM_TOOK_LEAD:
                    if (cgs.gametype == GT_AD || !cg_announcerLeadsVO.integer) {
                        break;
                    }
                    if (team == TEAM_BLUE) {
                        CG_AddBufferedSound(cgs.media.takenLeadSound);
                    } else if (team == TEAM_RED) {
                        CG_AddBufferedSound(cgs.media.lostLeadSound);
                    } else {
                        CG_AddBufferedSound(cgs.media.blueLeadsSound);
                    }
                    break;
                case GTS_TEAMS_ARE_TIED:
                    if (cgs.gametype != GT_AD && cg_announcerTiesVO.integer) {
                        CG_AddBufferedSound(cgs.media.teamsTiedSound);
                    }
                    break;
                case GTS_KAMIKAZE:
                    trap_S_StartLocalSound(cgs.media.kamikazeFarSound, CHAN_LOCAL_SOUND);
                    break;

                // [QL] the match and round results. Once a match result is announced the
                // announcer queue takes nothing more until the map restarts.
                case GTS_REDTEAM_WON:  // 14
                    CG_ResetAnnouncements();
                    if (cg_buzzerSound.integer) {
                        trap_S_StartLocalSound(cgs.media.buzzerSound, CHAN_LOCAL_SOUND);
                    }
                    if (team == TEAM_RED) {
                        trap_S_StartBackgroundTrack("music/win", NULL);
                    } else if (team == TEAM_BLUE) {
                        trap_S_StartBackgroundTrack("music/loss", NULL);
                    }
                    CG_AddBufferedSound(cgs.media.redWinsSound);
                    cg.soundBufferLocked = qtrue;
                    break;
                case GTS_BLUETEAM_WON:  // 15
                    CG_ResetAnnouncements();
                    if (cg_buzzerSound.integer) {
                        trap_S_StartLocalSound(cgs.media.buzzerSound, CHAN_LOCAL_SOUND);
                    }
                    if (team == TEAM_BLUE) {
                        trap_S_StartBackgroundTrack("music/win", NULL);
                    } else if (team == TEAM_RED) {
                        trap_S_StartBackgroundTrack("music/loss", NULL);
                    }
                    CG_AddBufferedSound(cgs.media.blueWinsSound);
                    cg.soundBufferLocked = qtrue;
                    break;
                case GTS_RED_WINS_ROUND:  // 16
                    CG_CenterPrint("Red wins the round", 90, 0.5f);
                    CG_AddBufferedSound(cgs.media.redWinsRoundSound);
                    break;
                case GTS_BLUE_WINS_ROUND:  // 17
                    CG_CenterPrint("Blue wins the round", 90, 0.5f);
                    CG_AddBufferedSound(cgs.media.blueWinsRoundSound);
                    break;
                case GTS_DRAW_ROUND:  // 18
                    CG_ResetAnnouncements();
                    if (cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000)) {
                        // [QL] 0x1001b999: infected Red Rover names the survivors from
                        // CS_ROUND_WINNER
                        const char* p = CG_ConfigString(CS_ROUND_WINNER);
                        const char* token;
                        char names[1024];
                        int i, count = 0, local = 0, client;

                        names[0] = '\0';
                        for (i = 0; i < cgs.maxclients; i++) {
                            token = COM_ParseExt((char**)&p, qtrue);
                            if (!token[0]) {
                                if (i == 0) {
                                    CG_CenterPrint("Round Draw", 90, 0.5f);
                                }
                                break;
                            }
                            count++;
                            client = atoi(token);
                            if (client == cg.clientNum) {
                                local = 1;
                            }
                            if (client >= 0 && client < MAX_CLIENTS && cgs.clientinfo[client].infoValid) {
                                Q_strcat(names, sizeof(names), va("%s\n", cgs.clientinfo[client].name));
                            }
                        }
                        CG_CenterPrint(va("^1Last Survivor%s:^7\n%s", count == 1 ? "" : "s", names), 90, 0.5f);
                        CG_AddBufferedSound(local ? cgs.media.winnerSound : cgs.media.roundOverSound);
                    } else {
                        CG_CenterPrint("Round Draw", 90, 0.5f);
                        CG_AddBufferedSound(cgs.media.roundDrawSound);
                    }
                    break;
                case GTS_LAST_STANDING:  // 19
                    if (cg_announcerLastStandingVO.integer && team == es->modelindex2) {
                        CG_AddBufferedSound(cgs.media.lastStandingSound);
                    }
                    break;
                case GTS_ROUND_OVER:  // 20
                    if (cgs.gametype == GT_RR) {
                        // [QL] 0x1001bb7d: Red Rover names the round's winners from CS_ROUND_WINNER
                        const char* p = CG_ConfigString(CS_ROUND_WINNER);
                        const char* token;
                        char names[1024];
                        int i, count = 0, client = -1;

                        names[0] = '\0';
                        for (i = 0; i < cgs.maxclients; i++) {
                            token = COM_ParseExt((char**)&p, qtrue);
                            if (!token[0]) {
                                break;
                            }
                            count++;
                            client = atoi(token);
                            if (client >= 0 && client < MAX_CLIENTS && cgs.clientinfo[client].infoValid) {
                                Q_strcat(names, sizeof(names), va("%s\n", cgs.clientinfo[client].name));
                            }
                        }
                        if (cgs.customSettings & 0x4000000) {
                            if (cg.clientNum == client) {
                                CG_CenterPrint(va("^7%s Wins!^7\n", names), 90, 0.5f);
                                CG_AddBufferedSound(cgs.media.winnerSound);
                                break;
                            }
                            CG_CenterPrint(va("^1Last Survivor%s:^7\n%s", count == 1 ? "" : "s", names), 90, 0.5f);
                        } else {
                            CG_CenterPrint(va("Round Winner%s:\n%s", count == 1 ? "" : "s", names), 90, 0.5f);
                        }
                    } else {
                        CG_CenterPrint("Round Over", 90, 0.5f);
                    }
                    CG_AddBufferedSound(cgs.media.roundOverSound);
                    break;
                case GTS_DOMINATION_POINT_CAPTURE:  // 23
                    if (es->modelindex2 == 0) {
                        trap_S_StartLocalSound(cgs.media.pointNeutralizedSound, CHAN_LOCAL_SOUND);
                        break;
                    }
                    if (es->powerups) {
                        static const char* pointNames[] = {"A", "B", "C", "D", "E"};
                        const char* point = (es->powerups >= 1 && es->powerups <= 5) ? pointNames[es->powerups - 1] : "?";

                        CG_CenterPrint(va("%s captured point %s!", es->modelindex2 == TEAM_RED ? "Red" : "Blue", point), 90, 0.5f);
                    }
                    if (cg_announcerLeadsVO.integer && es->powerups >= 0 && es->powerups <= 5) {
                        if (team == es->modelindex2) {
                            CG_AddBufferedSound(cgs.media.pointCapturedSounds[es->powerups]);
                        } else if (team != TEAM_SPECTATOR) {
                            CG_AddBufferedSound(cgs.media.pointLostSounds[es->powerups]);
                        }
                    }
                    break;
                case GTS_SURVIVOR:  // 24
                    if (team == es->modelindex2) {
                        trap_S_StartLocalSound(cgs.media.survivorSound, CHAN_LOCAL_SOUND);
                    }
                    break;

                default:
                    break;
            }
            break;
        }

        case EV_PAIN:
            // [QL] 0x1001bdb9: the local player's pain sounds come from here too
            DEBUGNAME("EV_PAIN");
            CG_PainEvent(cent, es->eventParm);
            break;

        case EV_DEATH1:
        case EV_DEATH2:
        case EV_DEATH3:
            DEBUGNAME("EV_DEATHx");
            trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, va("*death%i.wav", event - EV_DEATH1 + 1)));
            break;

        case EV_DROWN:
            DEBUGNAME("EV_DROWN");
            trap_S_StartSound(NULL, es->number, CHAN_VOICE, CG_CustomSound(es->number, "*drown.wav"));
            break;

        case EV_OBITUARY:
            DEBUGNAME("EV_OBITUARY");
            CG_Obituary(es);
            break;

        //
        // powerup events
        //
        case EV_POWERUP_QUAD:
            DEBUGNAME("EV_POWERUP_QUAD");
            if (es->number == cg.snap->ps.clientNum) {
                cg.powerupActive = PW_QUAD;
                cg.powerupTime = cg.time;
            }
            trap_S_StartSound(NULL, es->number, CHAN_ITEM, cgs.media.quadSound);
            break;
        case EV_POWERUP_BATTLESUIT:
            DEBUGNAME("EV_POWERUP_BATTLESUIT");
            if (es->number == cg.snap->ps.clientNum) {
                cg.powerupActive = PW_BATTLESUIT;
                cg.powerupTime = cg.time;
            }
            trap_S_StartSound(NULL, es->number, CHAN_ITEM, cgs.media.protectSound);
            break;
        case EV_POWERUP_REGEN:
            DEBUGNAME("EV_POWERUP_REGEN");
            if (es->number == cg.snap->ps.clientNum) {
                cg.powerupActive = PW_REGEN;
                cg.powerupTime = cg.time;
            }
            trap_S_StartSound(NULL, es->number, CHAN_ITEM, cgs.media.regenSound);
            break;
        case EV_POWERUP_ARMORREGEN:
            DEBUGNAME("EV_POWERUP_ARMORREGEN");
            if (es->number == cg.snap->ps.clientNum) {
                cg.powerupActive = PW_AMMOREGEN;
                cg.powerupTime = cg.time;
            }
            trap_S_StartSound(NULL, es->number, CHAN_ITEM, cgs.media.armorPickupSound);
            break;

        case EV_GIB_PLAYER:
            DEBUGNAME("EV_GIB_PLAYER");
            // don't play gib sound when using the kamikaze because it interferes
            // with the kamikaze sound, downside is that the gib sound will also
            // not be played when someone is gibbed while just carrying the kamikaze
            if (!(es->eFlags & EF_KAMIKAZE)) {
                // [QL] 0x1001bf80 picks the death sound off the same gore-pack
                // handle CG_GibPlayer branches on: the pack's single splat when
                // it is installed, otherwise the first or the fourth electrogib
                // sample, since retail masks rand() with 4 where it meant 3
                if (cgs.media.gibSkull) {
                    trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                      cgs.media.gibSound);
                } else {
                    trap_S_StartSound(NULL, es->number, CHAN_BODY,
                                      cgs.media.electroGibSound[(rand() & 4) ? 3 : 0]);
                }
            }
            // The direction the spray leaves along. See CG_GibPlayer: the
            // server packs the last hit's direction into eventParm like
            // Quake Live, but retail's case does not seem to decode it, so up
            // stays until that reading is confirmed.
            VectorSet(dir, 0, 0, 1);
            CG_GibPlayer(cent->lerpOrigin, dir);
            break;

        // [QL] EV_STOPLOOPINGSOUND removed from QL event enum.

        case EV_DEBUG_LINE:
            DEBUGNAME("EV_DEBUG_LINE");
            CG_Beam(cent);
            break;

        // ================================================================
        // [QL] New events - ported from cgamex86.dll binary analysis
        // ================================================================
        case EV_FOOTSTEP_SNOW:
            DEBUGNAME("EV_FOOTSTEP_SNOW");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.footsteps[FOOTSTEP_SNOW][rand() & 3]);
            }
            break;
        case EV_FOOTSTEP_WOOD:
            DEBUGNAME("EV_FOOTSTEP_WOOD");
            if (cg_footsteps.integer) {
                trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.footsteps[FOOTSTEP_WOOD][rand() & 3]);
            }
            break;
        case EV_ITEM_PICKUP_SPEC:
            DEBUGNAME("EV_ITEM_PICKUP_SPEC");
            CG_SpecItemTimerEvent(es);  // [QL] 0x1001c0b2
            break;
        case EV_OVERTIME:
            DEBUGNAME("EV_OVERTIME");
            // [QL] 0x1001b2bb: the klaxon, how much time was added, and the "overtime" voice
            // for the first period only
            trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.media.klaxon2Sound);
            CG_CenterPrint(va("Overtime! %d seconds added", cgs.timelimit_overtime), 90, 0.5f);
            if (!CG_GetOvertimeCount()) {
                CG_AddBufferedSound(cgs.media.overtimeVoSound);
            }
            break;
        case EV_GAMEOVER:
            DEBUGNAME("EV_GAMEOVER");
            // [QL] 0x1001c0cd: the buzzer, then the win or loss voice (individual modes
            // only) and music; the announcer queue stays locked until the map restarts
            CG_ResetAnnouncements();
            if (cg_buzzerSound.integer) {
                trap_S_StartLocalSound(cgs.media.buzzerSound, CHAN_LOCAL_SOUND);
            }
            if (CG_LocalPlayerLeads()) {
                if (!CG_IsTeamGametype(cgs.gametype)) {
                    CG_AddBufferedSound(cgs.media.winnerSound);
                }
                trap_S_StartBackgroundTrack("music/win", NULL);
            } else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
                trap_S_StartBackgroundTrack("music/win", NULL);
            } else {
                if (!CG_IsTeamGametype(cgs.gametype)) {
                    CG_AddBufferedSound(cgs.media.loserSound);
                }
                trap_S_StartBackgroundTrack("music/loss", NULL);
            }
            cg.soundBufferLocked = qtrue;
            break;
        case EV_MISSILE_MISS_DMGTHROUGH:
            DEBUGNAME("EV_MISSILE_MISS_DMGTHROUGH");
            // [QL] wallbang impact -> dedicated damage-through effect (binary 0x56
            // calls CG_MissileHitWall_DmgThrough, not the plain CG_MissileHitWall)
            ByteToDir(es->eventParm, dir);
            CG_MissileHitWall_DmgThrough(position, dir, es->weapon);
            break;
        case EV_THAW_PLAYER:
            DEBUGNAME("EV_THAW_PLAYER");
            // [QL] freeze-tag thaw complete (binary 0x57): spawn the ice-shard
            // burst. The shard spawning lives in cg_effects.c CG_ThawPlayer.
            CG_ThawPlayer(cent->lerpOrigin);
            break;
        case EV_THAW_TICK:
            DEBUGNAME("EV_THAW_TICK");
            trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.timPumpSound);
            break;
        case EV_SHOTGUN_KILL:
            DEBUGNAME("EV_SHOTGUN_KILL");
            // [QL] delayed shotgun-kill blood burst (binary 0x59). The blood-puff
            // logic (count = es->generic1/5, at es->otherEntityNum skin) lives in
            // cg_weapons.c CG_ShotgunKillEffect.
            CG_ShotgunKillEffect(cent);
            break;
        case EV_POI:
            DEBUGNAME("EV_POI");
            CG_PointOfInterest(cent);
            break;
        case EV_LIGHTNING_DISCHARGE:
            DEBUGNAME("EV_LIGHTNING_DISCHARGE");
            // [QL] LG water-discharge electric burst (binary 0x5c). Intensity is
            // taken from the event parameter; the sprite effect lives in
            // cg_effects.c CG_LightningDischargeEffect.
            CG_LightningDischargeEffect(es->eventParm);
            break;
        case EV_RACE_START:
            DEBUGNAME("EV_RACE_START");
            // [QL] 0x1001abfd: the start counts as the first point passed
            if (cg.snap->ps.clientNum == es->clientNum) {
                cg.race.active = qtrue;
                cg.race.startTime = es->time;
                cg.race.checkpointDiff = 0;
                cg.race.hasDiff = qfalse;
                cg.race.nextCheckpointEnt = es->otherEntityNum2;
                cg.race.nextNextCheckpointEnt = es->powerups;
                cg.race.checkpointsPassed = 1;
                CG_RaceBeep();
            }
            break;
        case EV_RACE_CHECKPOINT:
            DEBUGNAME("EV_RACE_CHECKPOINT");
            // [QL] 0x1001ad3a: the split against the best run when there is one, and how many
            // points are left when fewer than four
            if (cg.snap->ps.clientNum == es->clientNum) {
                char remaining[32];
                const char *color;

                cg.race.nextCheckpointEnt = es->otherEntityNum2;
                cg.race.nextNextCheckpointEnt = es->powerups;
                cg.race.checkpointsPassed++;
                cg.race.checkpointDiff = es->generic1 ? es->time : 0;
                cg.race.hasDiff = (es->generic1 != 0);
                CG_RaceBeep();

                remaining[0] = 0;
                if (CG_RaceCheckpointsLeft() < 4 && cg_drawCheckpointRemaining.integer) {
                    Q_strncpyz(remaining, va("^7%i remaining", CG_RaceCheckpointsLeft()), sizeof(remaining));
                }
                if (cg.race.hasDiff == 1) {
                    if (cg.race.checkpointDiff < 0) {
                        color = "^2";
                    } else {
                        color = cg.race.checkpointDiff != 0 ? "^1" : "^7";
                    }
                    CG_CenterPrint(va("%s%s\n%s", color, CG_FormatRaceTime(cg.race.checkpointDiff), remaining),
                                   320, 0.3f);
                } else {
                    CG_CenterPrint(va("Checkpoint\n%s", remaining), 320, 0.3f);
                }
            }
            break;
        case EV_RACE_FINISH:
            DEBUGNAME("EV_RACE_FINISH");
            // [QL] 0x1001aee3: the run is dropped; a valid finish shows the difference with the
            // best one and keeps the better of the two
            if (cg.snap->ps.clientNum == es->clientNum) {
                CG_RaceInit(0);
                if (!es->generic1) {
                    cg.race.hasDiff = qfalse;
                    cg.race.checkpointDiff = 0;
                } else {
                    cg.race.lastTime = es->time;
                    if (cg.race.bestTime != 0) {
                        int diff = cg.race.lastTime - cg.race.bestTime;
                        const char *color;

                        if (diff < 0) {
                            color = "^2";
                        } else {
                            color = diff != 0 ? "^1" : "^7";
                        }
                        CG_CenterPrint(va("%s%s\n", color, CG_FormatRaceTime(diff)), 320, 0.3f);
                    }
                    if (cg.race.bestTime == 0 || cg.race.lastTime < cg.race.bestTime) {
                        cg.race.bestTime = cg.race.lastTime;
                    }
                }
                trap_S_StartLocalSound(cgs.media.klaxon1Sound, CHAN_LOCAL_SOUND);  // [QL] 0x1001af7f
            }
            break;
        case EV_DAMAGEPLUM:
            DEBUGNAME("EV_DAMAGEPLUM");
            // [QL] floating damage number - only show for the local player's hits.
            // The server (DamagePlum 0x10046680 / the shotgun flush) packs the amount
            // in s.time, the recipient clientNum in s.clientNum, and the weapon in
            // s.generic1 - NOT pos.trTime/otherEntityNum/weapon.
            // [QL] 0x1001c3db only checks whose hit it is: which weapons show is up to
            // CG_DamagePlum and the cg_damagePlum list
            if (cg.snap->ps.clientNum == es->clientNum) {
                CG_DamagePlum(es->time, es->generic1, cent->lerpOrigin);
            }
            break;
        case EV_AWARD:
            DEBUGNAME("EV_AWARD");
            // [QL] award notification - 10 types, uses existing reward display system.
            // Binary (CG_EntityEvent 0x61) guards on es->clientNum (0xb0), selects
            // the medal from es->generic1 (0xe0) and takes the count from
            // es->modelindex2 (0xac) - NOT otherEntityNum/weapon/generic1.
            // [QL] 0x1001c437: combo kill, rampage, midair and revenge pick one of three voices
            if (cg.snap->ps.clientNum == es->clientNum) {
                int count = es->modelindex2;
                switch (es->generic1) {
                case 0: CG_PushReward(cgs.media.comboKillSounds[rand() % 3], cgs.media.medalComboKill, count); break;
                case 1: CG_PushReward(cgs.media.rampageSounds[rand() % 3], cgs.media.medalRampage, count); break;
                case 2: CG_PushReward(cgs.media.midAirSounds[rand() % 3], cgs.media.medalMidair, count); break;
                case 3: CG_PushReward(cgs.media.revengeSounds[rand() % 3], cgs.media.medalRevenge, count); break;
                case 4: CG_PushReward(cgs.media.perforatedSound, cgs.media.medalPerforated, count); break;
                case 5: CG_PushReward(cgs.media.headshotSound, cgs.media.medalHeadshot, count); break;
                case 6: CG_PushReward(cgs.media.accuracySound, cgs.media.medalAccuracy, count); break;
                case 7: CG_PushReward(cgs.media.quadGodSound, cgs.media.medalQuadGod, count); break;
                case 8: CG_PushReward(cgs.media.firstFragSound, cgs.media.medalFirstFrag, 1); break;
                case 9: CG_PushReward(cgs.media.perfectSound, cgs.media.medalPerfect, count); break;
                }
            }
            break;
        case EV_INFECTED:
            DEBUGNAME("EV_INFECTED");
            // [QL] 0x1001c5b7: only the announcer
            CG_AddBufferedSound(cgs.media.infectedSound);
            break;
        case EV_NEW_HIGH_SCORE:
            DEBUGNAME("EV_NEW_HIGH_SCORE");
            // [QL] new high score announce (binary CG_EntityEvent 0x1001c5c3 queues
            // new_high_score.ogg via CG_QueueAnnouncement; routed through the buffered
            // announcer ring here).
            CG_AddBufferedSound(cgs.media.newHighScoreSound);
            break;

        default:
            DEBUGNAME("UNKNOWN");
            CG_Error("Unknown event: %i", event);
            break;
    }
}

/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents(centity_t* cent) {
    // check for event-only entities
    if (cent->currentState.eType > ET_EVENTS) {
        if (cent->previousEvent) {
            return;  // already fired
        }
        // if this is a player event set the entity number of the client entity number
        if (cent->currentState.eFlags & EF_PLAYER_EVENT) {
            cent->currentState.number = cent->currentState.otherEntityNum;
        }

        cent->previousEvent = 1;

        cent->currentState.event = cent->currentState.eType - ET_EVENTS;
    } else {
        // check for events riding with another entity
        if (cent->currentState.event == cent->previousEvent) {
            return;
        }
        cent->previousEvent = cent->currentState.event;
        if ((cent->currentState.event & ~EV_EVENT_BITS) == 0) {
            return;
        }
    }

    // calculate the position at exactly the frame time
    BG_EvaluateTrajectory(&cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin);
    CG_SetEntitySoundPosition(cent);

    CG_EntityEvent(cent, cent->lerpOrigin);
}
