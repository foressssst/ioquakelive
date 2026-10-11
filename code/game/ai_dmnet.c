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
#include "ai_team.h"
#include "chars.h"
#include "inv.h"
#include "syn.h"
#include "match.h"

#include "../../ui/menudef.h"

#define GFL_AIR 128

int numnodeswitches;
char nodeswitch[MAX_NODESWITCHES + 1][144];

#define LOOKAHEAD_DISTANCE 300

void BotResetNodeSwitches(void) {
    numnodeswitches = 0;
}

void BotDumpNodeSwitches(bot_state_t* bs) {
    int i;
    char netname[MAX_NETNAME];

    ClientName(bs->client, netname, sizeof(netname));
    BotAI_Print(PRT_MESSAGE, "%s at %1.1f switched more than %d AI nodes\n", netname, FloatTime(), MAX_NODESWITCHES);
    for (i = 0; i < numnodeswitches; i++) {
        BotAI_Print(PRT_MESSAGE, "%s", nodeswitch[i]);
    }
    BotAI_Print(PRT_FATAL, "");
}

void BotRecordNodeSwitch(bot_state_t* bs, char* node, char* str, char* s) {
    char netname[MAX_NETNAME];

    ClientName(bs->client, netname, sizeof(netname));
    Com_sprintf(nodeswitch[numnodeswitches], 144, "%s at %2.1f entered %s: %s from %s\n", netname, FloatTime(), node, str, s);
    Com_sprintf(bs->ainodename, sizeof(bs->ainodename), "%s", node);
#ifdef DEBUG
    if (0) {
        BotAI_Print(PRT_MESSAGE, "%s", nodeswitch[numnodeswitches]);
    }
#endif
    numnodeswitches++;
}

int BotGetAirGoal(bot_state_t* bs, bot_goal_t* goal) {
    bsp_trace_t bsptrace;
    vec3_t end, mins = {-15, -15, -2}, maxs = {15, 15, 2};
    int areanum;

    VectorCopy(bs->origin, end);
    end[2] += 1000;
    BotAI_Trace(&bsptrace, bs->origin, mins, maxs, end, bs->entitynum, CONTENTS_SOLID | CONTENTS_PLAYERCLIP);
    VectorCopy(bsptrace.endpos, end);
    BotAI_Trace(&bsptrace, end, mins, maxs, bs->origin, bs->entitynum, CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
    if (bsptrace.fraction > 0) {
        areanum = BotPointAreaNum(bsptrace.endpos);
        if (areanum) {
            VectorCopy(bsptrace.endpos, goal->origin);
            goal->origin[2] -= 2;
            goal->areanum = areanum;
            goal->mins[0] = -15;
            goal->mins[1] = -15;
            goal->mins[2] = -1;
            goal->maxs[0] = 15;
            goal->maxs[1] = 15;
            goal->maxs[2] = 1;
            goal->flags = GFL_AIR;
            goal->number = 0;
            goal->iteminfo = 0;
            goal->entitynum = 0;
            return qtrue;
        }
    }
    return qfalse;
}

int BotGoForAir(bot_state_t* bs, int tfl, bot_goal_t* ltg, float range) {
    bot_goal_t goal;

    if (bs->lastair_time < FloatTime() - 6) {
#ifdef DEBUG
#endif
        if (BotGetAirGoal(bs, &goal)) {
            trap_BotPushGoal(bs->gs, &goal);
            return qtrue;
        } else {
            while (trap_BotChooseNBGItem(bs->gs, bs->origin, bs->inventory, tfl, ltg, range)) {
                trap_BotGetTopGoal(bs->gs, &goal);
                if (!(trap_AAS_PointContents(goal.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))) {
                    return qtrue;
                }
                trap_BotPopGoal(bs->gs);
            }
            trap_BotResetAvoidGoals(bs->gs);
        }
    }
    return qfalse;
}

int BotNearbyGoal(bot_state_t* bs, int tfl, bot_goal_t* ltg, float range) {
    int ret;

    if (BotGoForAir(bs, tfl, ltg, range))
        return qtrue;
    if (BotCTFCarryingFlag(bs)) {
        if (trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin,
                                              bs->teamgoal.areanum, TFL_DEFAULT) < 300) {
            range = 50;
        }
    }
    ret = trap_BotChooseNBGItem(bs->gs, bs->origin, bs->inventory, tfl, ltg, range);
    return ret;
}

int BotReachedGoal(bot_state_t* bs, bot_goal_t* goal) {
    if (goal->flags & GFL_ITEM) {
        if (trap_BotTouchingGoal(bs->origin, goal)) {
            if (!(goal->flags & GFL_DROPPED)) {
                trap_BotSetAvoidGoalTime(bs->gs, goal->number, -1);
            }
            return qtrue;
        }
        if (trap_BotItemGoalInVisButNotVisible(bs->entitynum, bs->eye, bs->viewangles, goal)) {
            return qtrue;
        }
        if (bs->areanum == goal->areanum) {
            if (bs->origin[0] > goal->origin[0] + goal->mins[0] && bs->origin[0] < goal->origin[0] + goal->maxs[0]) {
                if (bs->origin[1] > goal->origin[1] + goal->mins[1] && bs->origin[1] < goal->origin[1] + goal->maxs[1]) {
                    if (!trap_AAS_Swimming(bs->origin)) {
                        return qtrue;
                    }
                }
            }
        }
    } else if (goal->flags & GFL_AIR) {
        if (trap_BotTouchingGoal(bs->origin, goal))
            return qtrue;
        if (bs->lastair_time > FloatTime() - 1)
            return qtrue;
    } else {
        if (trap_BotTouchingGoal(bs->origin, goal))
            return qtrue;
    }
    return qfalse;
}

int BotGetItemLongTermGoal(bot_state_t* bs, int tfl, bot_goal_t* goal) {
    if (!trap_BotGetTopGoal(bs->gs, goal)) {
        bs->ltg_time = 0;
    }
    else if (BotReachedGoal(bs, goal)) {
        BotChooseWeapon(bs);
        bs->ltg_time = 0;
    }
    if (bs->ltg_time < FloatTime()) {
        trap_BotPopGoal(bs->gs);
        if (trap_BotChooseLTGItem(bs->gs, bs->origin, bs->inventory, tfl)) {
            bs->ltg_time = FloatTime() + 20;
        } else {
#ifdef DEBUG
            char netname[128];

            BotAI_Print(PRT_MESSAGE, "%s: no valid ltg (probably stuck)\n", ClientName(bs->client, netname, sizeof(netname)));
#endif
            trap_BotResetAvoidGoals(bs->gs);
            trap_BotResetAvoidReach(bs->ms);
        }
        return trap_BotGetTopGoal(bs->gs, goal);
    }
    return qtrue;
}

static int BotUpdateKillGoal(bot_state_t* bs) {
    aas_entityinfo_t entinfo;

    if (bs->ltg_time < FloatTime()) {
        return qfalse;
    }
    BotEntityInfo(bs->teamgoal.entitynum, &entinfo);
    if (!entinfo.valid) {
        return qfalse;
    }
    if (EntityIsDead(&entinfo)) {
        bs->ltg_time = 0;
        return qfalse;
    }
    bs->ltg_time = FloatTime() + 5;
    VectorCopy(entinfo.origin, bs->teamgoal.origin);
    bs->teamgoal.areanum = BotPointAreaNum(entinfo.origin);
    VectorSet(bs->teamgoal.mins, -8, -8, -8);
    VectorSet(bs->teamgoal.maxs, 8, 8, 8);
    bs->teamgoal.flags = 0;
    return qtrue;
}

static int BotGetRoundGoal(bot_state_t* bs, bot_goal_t* goal) {
    aas_entityinfo_t entinfo;
    gentity_t* best = NULL;
    int i;

    if (!BotUpdateKillGoal(bs)) {
        for (i = 0; i < level.maxclients; i++) {
            if (BotSameTeam(bs, i)) {
                continue;
            }
            BotEntityInfo(i, &entinfo);
            if (!entinfo.valid) {
                continue;
            }
            if (entinfo.number < MAX_CLIENTS) {
                gentity_t* ent = &g_entities[entinfo.number];

                if (!ent->inuse || !ent->client || ent->client->ps.pm_type != PM_NORMAL) {
                    continue;
                }
            }
            if (best && DistanceSquared(bs->origin, best->r.currentOrigin) <=
                            DistanceSquared(bs->origin, entinfo.origin)) {
                continue;
            }
            best = &g_entities[i];
        }
        if (!best) {
            return qfalse;
        }
        bs->ltgtype = LTG_KILL;
        bs->ltg_time = FloatTime() + 5;
        bs->teamgoal_time = FloatTime() + 60;
        bs->teamgoal.entitynum = best->s.number;
        BotUpdateKillGoal(bs);
    }
    memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
    return qtrue;
}

int BotGetLongTermGoal(bot_state_t* bs, int tfl, int retreat, bot_goal_t* goal) {
    vec3_t target, dir, dir2;
    char netname[MAX_NETNAME];
    char buf[MAX_MESSAGE_SIZE];
    int areanum;
    float croucher;
    aas_entityinfo_t entinfo, botinfo;
    bot_waypoint_t* wp;

    if (gametype == GT_CA || gametype == GT_RR) {
        return BotGetRoundGoal(bs, goal);
    }

    if (bs->ltgtype == LTG_TEAMHELP && !retreat) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "help_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        if (bs->teamgoal_time < FloatTime())
            bs->ltgtype = 0;
        if (bs->teammatevisible_time < FloatTime() - 10)
            bs->ltgtype = 0;
        BotEntityInfo(bs->teammate, &entinfo);
        if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->teammate)) {
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(100)) {
                trap_BotResetAvoidReach(bs->ms);
                return qfalse;
            }
        } else {
            bs->teammatevisible_time = FloatTime();
        }
        if (entinfo.valid) {
            areanum = BotPointAreaNum(entinfo.origin);
            if (areanum && trap_AAS_AreaReachability(areanum)) {
                bs->teamgoal.entitynum = bs->teammate;
                bs->teamgoal.areanum = areanum;
                VectorCopy(entinfo.origin, bs->teamgoal.origin);
                VectorSet(bs->teamgoal.mins, -8, -8, -8);
                VectorSet(bs->teamgoal.maxs, 8, 8, 8);
            }
        }
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        return qtrue;
    }
    if (bs->ltgtype == LTG_TEAMACCOMPANY && !retreat) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "accompany_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        if (bs->teamgoal_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "accompany_stop", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->ltgtype = 0;
        }
        BotEntityInfo(bs->teammate, &entinfo);
        if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->teammate)) {
            bs->teammatevisible_time = FloatTime();
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(bs->formation_dist)) {
                BotEntityInfo(bs->entitynum, &botinfo);
                if (botinfo.origin[2] + botinfo.maxs[2] > entinfo.origin[2] + entinfo.mins[2]) {
                    if (botinfo.origin[0] + botinfo.maxs[0] > entinfo.origin[0] + entinfo.mins[0] - 4 &&
                        botinfo.origin[0] + botinfo.mins[0] < entinfo.origin[0] + entinfo.maxs[0] + 4) {
                        if (botinfo.origin[1] + botinfo.maxs[1] > entinfo.origin[1] + entinfo.mins[1] - 4 &&
                            botinfo.origin[1] + botinfo.mins[1] < entinfo.origin[1] + entinfo.maxs[1] + 4) {
                            if (botinfo.origin[2] + botinfo.maxs[2] > entinfo.origin[2] + entinfo.mins[2] - 4 &&
                                botinfo.origin[2] + botinfo.mins[2] < entinfo.origin[2] + entinfo.maxs[2] + 4) {
                                AngleVectors(entinfo.angles, dir, NULL, NULL);
                                dir[2] = 0;
                                VectorNormalize(dir);
                                VectorSubtract(bs->origin, entinfo.origin, dir2);
                                VectorNormalize(dir2);
                                if (DotProduct(dir, dir2) > 0.7) {
                                    BotSetupForMovement(bs);
                                    trap_BotMoveInDirection(bs->ms, dir2, 400, MOVE_WALK);
                                }
                            }
                        }
                    }
                }
                if (bs->attackcrouch_time < FloatTime() - 5) {
                    croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
                    if (random() < bs->thinktime * croucher) {
                        bs->attackcrouch_time = FloatTime() + 5 + croucher * 15;
                    }
                }
                if (trap_AAS_Swimming(bs->origin))
                    bs->attackcrouch_time = FloatTime() - 1;
                if (bs->arrive_time < FloatTime() - 2) {
                    if (!bs->arrive_time) {
                        trap_EA_Gesture(bs->client);
                        BotAI_BotInitialChat(bs, "accompany_arrive", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
                        trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
                        bs->arrive_time = FloatTime();
                    }
                    else if (bs->attackcrouch_time > FloatTime()) {
                        trap_EA_Crouch(bs->client);
                    }
                    else if (random() < bs->thinktime * 0.05) {
                        trap_EA_Gesture(bs->client);
                    }
                }
                if (bs->arrive_time > FloatTime() - 2) {
                    VectorSubtract(entinfo.origin, bs->origin, dir);
                    vectoangles(dir, bs->ideal_viewangles);
                    bs->ideal_viewangles[2] *= 0.5;
                }
                else if (random() < bs->thinktime * 0.8) {
                    BotRoamGoal(bs, target);
                    VectorSubtract(target, bs->origin, dir);
                    vectoangles(dir, bs->ideal_viewangles);
                    bs->ideal_viewangles[2] *= 0.5;
                }
                if (BotGoForAir(bs, bs->tfl, &bs->teamgoal, 400)) {
                    trap_BotResetLastAvoidReach(bs->ms);
                    bs->nbg_time = FloatTime() + 8;
                    AIEnter_Seek_NBG(bs, "BotLongTermGoal: go for air");
                    return qfalse;
                }
                trap_BotResetAvoidReach(bs->ms);
                return qfalse;
            }
        }
        if (entinfo.valid) {
            areanum = BotPointAreaNum(entinfo.origin);
            if (areanum && trap_AAS_AreaReachability(areanum)) {
                bs->teamgoal.entitynum = bs->teammate;
                bs->teamgoal.areanum = areanum;
                VectorCopy(entinfo.origin, bs->teamgoal.origin);
                VectorSet(bs->teamgoal.mins, -8, -8, -8);
                VectorSet(bs->teamgoal.maxs, 8, 8, 8);
            }
        }
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        if (bs->teammatevisible_time < FloatTime() - 60) {
            BotAI_BotInitialChat(bs, "accompany_cannotfind", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->ltgtype = 0;
            bs->teammatevisible_time = FloatTime();
        }
        return qtrue;
    }
    if (bs->ltgtype == LTG_DEFENDKEYAREA) {
        if (trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin,
                                              bs->teamgoal.areanum, TFL_DEFAULT) > bs->defendaway_range) {
            bs->defendaway_time = 0;
        }
    }
    if (bs->ltgtype == LTG_DEFENDKEYAREA && !retreat &&
        bs->defendaway_time < FloatTime()) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "defend_start", buf, NULL);
            trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
            BotVoiceChatOnly(bs, -1, VOICECHAT_ONDEFENSE);
            bs->teammessage_time = 0;
        }
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        if (bs->teamgoal_time < FloatTime()) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "defend_stop", buf, NULL);
            trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
            bs->ltgtype = 0;
        }
        VectorSubtract(goal->origin, bs->origin, dir);
        if (VectorLengthSquared(dir) < Square(70)) {
            trap_BotResetAvoidReach(bs->ms);
            bs->defendaway_time = FloatTime() + 3 + 3 * random();
            if (BotHasPersistantPowerupAndWeapon(bs)) {
                bs->defendaway_range = 100;
            } else {
                bs->defendaway_range = 350;
            }
        }
        return qtrue;
    }
    if (bs->ltgtype == LTG_KILL && !retreat) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            EasyClientName(bs->teamgoal.entitynum, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "kill_start", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->teammessage_time = 0;
        }
        if (bs->killedenemy_time > bs->teamgoal_time - TEAM_KILL_SOMEONE && bs->lastkilledplayer == bs->teamgoal.entitynum) {
            EasyClientName(bs->teamgoal.entitynum, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "kill_done", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->ltgtype = 0;
        }
        if (bs->teamgoal_time < FloatTime()) {
            bs->ltgtype = 0;
        }
        return BotGetItemLongTermGoal(bs, tfl, goal);
    }
    if (bs->ltgtype == LTG_GETITEM && !retreat) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "getitem_start", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        if (bs->teamgoal_time < FloatTime()) {
            bs->ltgtype = 0;
        }
        if (trap_BotItemGoalInVisButNotVisible(bs->entitynum, bs->eye, bs->viewangles, goal)) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "getitem_notthere", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->ltgtype = 0;
        } else if (BotReachedGoal(bs, goal)) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "getitem_gotit", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->ltgtype = 0;
        }
        return qtrue;
    }
    if ((bs->ltgtype == LTG_CAMP || bs->ltgtype == LTG_CAMPORDER) && !retreat) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            if (bs->ltgtype == LTG_CAMPORDER) {
                BotAI_BotInitialChat(bs, "camp_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
                trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
                BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
                trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            }
            bs->teammessage_time = 0;
        }
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        if (bs->teamgoal_time < FloatTime()) {
            if (bs->ltgtype == LTG_CAMPORDER) {
                BotAI_BotInitialChat(bs, "camp_stop", NULL);
                trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            }
            bs->ltgtype = 0;
        }
        VectorSubtract(goal->origin, bs->origin, dir);
        if (VectorLengthSquared(dir) < Square(60)) {
            if (!bs->arrive_time) {
                if (bs->ltgtype == LTG_CAMPORDER) {
                    BotAI_BotInitialChat(bs, "camp_arrive", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
                    trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
                    BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_INPOSITION);
                }
                bs->arrive_time = FloatTime();
            }
            if (random() < bs->thinktime * 0.8) {
                BotRoamGoal(bs, target);
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
                bs->ideal_viewangles[2] *= 0.5;
            }
            if (bs->attackcrouch_time < FloatTime() - 5) {
                croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
                if (random() < bs->thinktime * croucher) {
                    bs->attackcrouch_time = FloatTime() + 5 + croucher * 15;
                }
            }
            if (bs->attackcrouch_time > FloatTime()) {
                trap_EA_Crouch(bs->client);
            }
            if (trap_AAS_Swimming(bs->origin))
                bs->attackcrouch_time = FloatTime() - 1;
            if (trap_PointContents(bs->eye, bs->entitynum) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
                if (bs->ltgtype == LTG_CAMPORDER) {
                    BotAI_BotInitialChat(bs, "camp_stop", NULL);
                    trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
                    if (bs->lastgoal_ltgtype == LTG_CAMPORDER) {
                        bs->lastgoal_ltgtype = 0;
                    }
                }
                bs->ltgtype = 0;
            }
            trap_BotResetAvoidReach(bs->ms);
            return qfalse;
        }
        return qtrue;
    }
    if (bs->ltgtype == LTG_PATROL && !retreat) {
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            strcpy(buf, "");
            for (wp = bs->patrolpoints; wp; wp = wp->next) {
                strcat(buf, wp->name);
                if (wp->next)
                    strcat(buf, " to ");
            }
            BotAI_BotInitialChat(bs, "patrol_start", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        if (!bs->curpatrolpoint) {
            bs->ltgtype = 0;
            return qfalse;
        }
        if (trap_BotTouchingGoal(bs->origin, &bs->curpatrolpoint->goal)) {
            if (bs->patrolflags & PATROL_BACK) {
                if (bs->curpatrolpoint->prev) {
                    bs->curpatrolpoint = bs->curpatrolpoint->prev;
                } else {
                    bs->curpatrolpoint = bs->curpatrolpoint->next;
                    bs->patrolflags &= ~PATROL_BACK;
                }
            } else {
                if (bs->curpatrolpoint->next) {
                    bs->curpatrolpoint = bs->curpatrolpoint->next;
                } else {
                    bs->curpatrolpoint = bs->curpatrolpoint->prev;
                    bs->patrolflags |= PATROL_BACK;
                }
            }
        }
        if (bs->teamgoal_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "patrol_stop", NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->ltgtype = 0;
        }
        if (!bs->curpatrolpoint) {
            bs->ltgtype = 0;
            return qfalse;
        }
        memcpy(goal, &bs->curpatrolpoint->goal, sizeof(bot_goal_t));
        return qtrue;
    }
#ifdef CTF
    if (gametype == GT_CTF || gametype == GT_AD) {
        if (bs->ltgtype == LTG_GETFLAG) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "captureflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONGETFLAG);
                bs->teammessage_time = 0;
            }
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &ctf_redflag, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                switch (BotTeam(bs)) {
                    case TEAM_RED:
                        bs->blueflagstatus = 1;
                        break;
                    case TEAM_BLUE:
                        bs->redflagstatus = 1;
                        break;
                }
                bs->ltgtype = 0;
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        if (bs->ltgtype == LTG_RUSHBASE && bs->rushbaseaway_time < FloatTime()) {
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &ctf_redflag, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (!BotCTFCarryingFlag(bs))
                bs->ltgtype = 0;
            if (bs->teamgoal_time < FloatTime())
                bs->ltgtype = 0;
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                if (BotCTFCarryingFlag(bs)) {
                    trap_BotResetAvoidReach(bs->ms);
                    bs->rushbaseaway_time = FloatTime() + 5 + 10 * random();
                } else {
                    bs->ltgtype = 0;
                }
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        if (bs->ltgtype == LTG_RETURNFLAG) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "returnflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONRETURNFLAG);
                bs->teammessage_time = 0;
            }
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &ctf_redflag, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (trap_BotTouchingGoal(bs->origin, goal))
                bs->ltgtype = 0;
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
    }
#endif
    else if (gametype == GT_1FCTF) {
        if (bs->ltgtype == LTG_GETFLAG) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "captureflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONGETFLAG);
                bs->teammessage_time = 0;
            }
            memcpy(goal, &ctf_neutralflag, sizeof(bot_goal_t));
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->ltgtype = 0;
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            return qtrue;
        }
        if (bs->ltgtype == LTG_RUSHBASE) {
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &ctf_redflag, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (!Bot1FCTFCarryingFlag(bs)) {
                bs->ltgtype = 0;
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
            bs->attackaway_time < FloatTime()) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "attackenemybase_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
                bs->teammessage_time = 0;
            }
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &ctf_redflag, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->attackaway_time = FloatTime() + 2 + 5 * random();
            }
            return qtrue;
        }
        if (bs->ltgtype == LTG_RETURNFLAG) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "returnflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONRETURNFLAG);
                bs->teammessage_time = 0;
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            return BotGetItemLongTermGoal(bs, tfl, goal);
        }
    } else if (gametype == GT_OBELISK) {
        if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
            bs->attackaway_time < FloatTime()) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "attackenemybase_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
                bs->teammessage_time = 0;
            }
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &blueobelisk, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &redobelisk, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (BotFeelingBad(bs) > 50) {
                return BotGetItemLongTermGoal(bs, tfl, goal);
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->attackaway_time = FloatTime() + 3 + 5 * random();
            }
            VectorSubtract(bs->origin, goal->origin, dir);
            if (VectorLengthSquared(dir) < Square(60)) {
                bs->attackaway_time = FloatTime() + 3 + 5 * random();
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
    } else if (gametype == GT_HARVESTER) {
        if (bs->ltgtype == LTG_RUSHBASE) {
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &blueobelisk, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &redobelisk, sizeof(bot_goal_t));
                    break;
                default:
                    BotGoHarvest(bs);
                    return qfalse;
            }
            if (!BotHarvesterCarryingCubes(bs)) {
                BotGoHarvest(bs);
                return qfalse;
            }
            if (bs->teamgoal_time < FloatTime()) {
                BotGoHarvest(bs);
                return qfalse;
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                BotGoHarvest(bs);
                return qfalse;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
            bs->attackaway_time < FloatTime()) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "attackenemybase_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
                bs->teammessage_time = 0;
            }
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    memcpy(goal, &blueobelisk, sizeof(bot_goal_t));
                    break;
                case TEAM_BLUE:
                    memcpy(goal, &redobelisk, sizeof(bot_goal_t));
                    break;
                default:
                    bs->ltgtype = 0;
                    return qfalse;
            }
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->attackaway_time = FloatTime() + 2 + 5 * random();
            }
            return qtrue;
        }
        if (bs->ltgtype == LTG_HARVEST &&
            bs->harvestaway_time < FloatTime()) {
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "harvest_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
                bs->teammessage_time = 0;
            }
            memcpy(goal, &neutralobelisk, sizeof(bot_goal_t));
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->harvestaway_time = FloatTime() + 4 + 3 * random();
            }
            return qtrue;
        }
    }
    return BotGetItemLongTermGoal(bs, tfl, goal);
}

int BotLongTermGoal(bot_state_t* bs, int tfl, int retreat, bot_goal_t* goal) {
    aas_entityinfo_t entinfo;
    char teammate[MAX_MESSAGE_SIZE];
    float squaredist;
    int areanum;
    vec3_t dir;

    if (bs->lead_time > 0 && !retreat) {
        if (bs->lead_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "lead_stop", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->lead_time = 0;
            return BotGetLongTermGoal(bs, tfl, retreat, goal);
        }
        if (bs->leadmessage_time < 0 && -bs->leadmessage_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->leadmessage_time = FloatTime();
        }
        BotEntityInfo(bs->lead_teammate, &entinfo);
        if (entinfo.valid) {
            areanum = BotPointAreaNum(entinfo.origin);
            if (areanum && trap_AAS_AreaReachability(areanum)) {
                bs->lead_teamgoal.entitynum = bs->lead_teammate;
                bs->lead_teamgoal.areanum = areanum;
                VectorCopy(entinfo.origin, bs->lead_teamgoal.origin);
                VectorSet(bs->lead_teamgoal.mins, -8, -8, -8);
                VectorSet(bs->lead_teamgoal.maxs, 8, 8, 8);
            }
        }
        if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->lead_teammate)) {
            bs->leadvisible_time = FloatTime();
        }
        if (bs->leadvisible_time < FloatTime() - 1) {
            bs->leadbackup_time = FloatTime() + 2;
        }
        VectorSubtract(bs->origin, bs->lead_teamgoal.origin, dir);
        squaredist = VectorLengthSquared(dir);
        if (bs->leadbackup_time > FloatTime()) {
            if (bs->leadmessage_time < FloatTime() - 20) {
                BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
                trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
                bs->leadmessage_time = FloatTime();
            }
            if (squaredist < Square(100)) {
                bs->leadbackup_time = 0;
            }
            memcpy(goal, &bs->lead_teamgoal, sizeof(bot_goal_t));
            return qtrue;
        } else {
            if (squaredist > Square(500)) {
                if (bs->leadmessage_time < FloatTime() - 20) {
                    BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
                    trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
                    bs->leadmessage_time = FloatTime();
                }
                VectorSubtract(entinfo.origin, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
                bs->ideal_viewangles[2] *= 0.5;
                return qfalse;
            }
        }
    }
    return BotGetLongTermGoal(bs, tfl, retreat, goal);
}

void AIEnter_Intermission(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "intermission", "", s);
    BotResetState(bs);
    if (BotChat_EndLevel(bs)) {
        trap_BotEnterChat(bs->cs, 0, bs->chatto);
    }
    bs->ainode = AINode_Intermission;
}

int AINode_Intermission(bot_state_t* bs) {
    if (!BotIntermission(bs)) {
        if (BotChat_StartLevel(bs)) {
            bs->stand_time = FloatTime() + BotChatTime(bs);
        } else {
            bs->stand_time = FloatTime() + 2;
        }
        AIEnter_Stand(bs, "intermission: chat");
    }
    return qtrue;
}

void AIEnter_Observer(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "observer", "", s);
    BotResetState(bs);
    bs->ainode = AINode_Observer;
}

int AINode_Observer(bot_state_t* bs) {
    if (!BotIsObserver(bs)) {
        AIEnter_Stand(bs, "observer: left observer");
    }
    return qtrue;
}

void AIEnter_Stand(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "stand", "", s);
    bs->standfindenemy_time = FloatTime() + 1;
    bs->ainode = AINode_Stand;
}

int AINode_Stand(bot_state_t* bs) {
    if (bs->lastframe_health > bs->inventory[INVENTORY_HEALTH]) {
        if (BotChat_HitTalking(bs)) {
            bs->standfindenemy_time = FloatTime() + BotChatTime(bs) + 0.1;
            bs->stand_time = FloatTime() + BotChatTime(bs) + 0.1;
        }
    }
    if (bs->standfindenemy_time < FloatTime()) {
        if (BotFindEnemy(bs, -1)) {
            AIEnter_Battle_Fight(bs, "stand: found enemy");
            return qfalse;
        }
        bs->standfindenemy_time = FloatTime() + 1;
    }
    trap_EA_Talk(bs->client);
    if (bs->stand_time < FloatTime()) {
        trap_BotEnterChat(bs->cs, 0, bs->chatto);
        AIEnter_Seek_LTG(bs, "stand: time out");
        return qfalse;
    }
    return qtrue;
}

void AIEnter_Respawn(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "respawn", "", s);
    trap_BotResetMoveState(bs->ms);
    trap_BotResetGoalState(bs->gs);
    trap_BotResetAvoidGoals(bs->gs);
    trap_BotResetAvoidReach(bs->ms);
    if (BotChat_Death(bs)) {
        bs->respawn_time = FloatTime() + BotChatTime(bs);
        bs->respawnchat_time = FloatTime();
    } else {
        bs->respawn_time = FloatTime() + 1 + random();
        bs->respawnchat_time = 0;
    }
    bs->respawn_wait = qfalse;
    bs->ainode = AINode_Respawn;
}

int AINode_Respawn(bot_state_t* bs) {
    if (bs->respawn_wait) {
        if (!BotIsDead(bs)) {
            AIEnter_Seek_LTG(bs, "respawn: respawned");
        } else {
            trap_EA_Respawn(bs->client);
        }
    } else if (bs->respawn_time < FloatTime()) {
        bs->respawn_wait = qtrue;
        trap_EA_Respawn(bs->client);
        if (bs->respawnchat_time) {
            trap_BotEnterChat(bs->cs, 0, bs->chatto);
            bs->enemy = -1;
        }
    }
    if (bs->respawnchat_time && bs->respawnchat_time < FloatTime() - 0.5) {
        trap_EA_Talk(bs->client);
    }
    return qtrue;
}

int BotSelectActivateWeapon(bot_state_t* bs) {
    if (bs->inventory[INVENTORY_MACHINEGUN] > 0 && bs->inventory[INVENTORY_BULLETS] > 0)
        return WEAPONINDEX_MACHINEGUN;
    else if (bs->inventory[INVENTORY_SHOTGUN] > 0 && bs->inventory[INVENTORY_SHELLS] > 0)
        return WEAPONINDEX_SHOTGUN;
    else if (bs->inventory[INVENTORY_PLASMAGUN] > 0 && bs->inventory[INVENTORY_CELLS] > 0)
        return WEAPONINDEX_PLASMAGUN;
    else if (bs->inventory[INVENTORY_LIGHTNING] > 0 && bs->inventory[INVENTORY_LIGHTNINGAMMO] > 0)
        return WEAPONINDEX_LIGHTNING;
    else if (bs->inventory[INVENTORY_CHAINGUN] > 0 && bs->inventory[INVENTORY_BELT] > 0)
        return WEAPONINDEX_CHAINGUN;
    else if (bs->inventory[INVENTORY_NAILGUN] > 0 && bs->inventory[INVENTORY_NAILS] > 0)
        return WEAPONINDEX_NAILGUN;
    else if (bs->inventory[INVENTORY_PROXLAUNCHER] > 0 && bs->inventory[INVENTORY_MINES] > 0)
        return WEAPONINDEX_PROXLAUNCHER;
    else if (bs->inventory[INVENTORY_GRENADELAUNCHER] > 0 && bs->inventory[INVENTORY_GRENADES] > 0)
        return WEAPONINDEX_GRENADE_LAUNCHER;
    else if (bs->inventory[INVENTORY_RAILGUN] > 0 && bs->inventory[INVENTORY_SLUGS] > 0)
        return WEAPONINDEX_RAILGUN;
    else if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 && bs->inventory[INVENTORY_ROCKETS] > 0)
        return WEAPONINDEX_ROCKET_LAUNCHER;
    else if (bs->inventory[INVENTORY_BFG10K] > 0 && bs->inventory[INVENTORY_BFGAMMO] > 0)
        return WEAPONINDEX_BFG;
    else {
        return -1;
    }
}

void BotClearPath(bot_state_t* bs, bot_moveresult_t* moveresult) {
    int i, bestmine;
    float dist, bestdist;
    vec3_t target, dir;
    bsp_trace_t bsptrace;
    entityState_t state;

    if (bs->kamikazebody) {
        if (!(moveresult->flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_MOVEMENTWEAPON))) {
            BotAI_GetEntityState(bs->kamikazebody, &state);
            VectorCopy(state.pos.trBase, target);
            target[2] += 8;
            VectorSubtract(target, bs->eye, dir);
            vectoangles(dir, moveresult->ideal_viewangles);
            moveresult->weapon = BotSelectActivateWeapon(bs);
            if (moveresult->weapon == -1) {
                moveresult->weapon = 0;
            }
            if (moveresult->weapon) {
                moveresult->flags |= MOVERESULT_MOVEMENTWEAPON | MOVERESULT_MOVEMENTVIEW;
                if (bs->cur_ps.weapon == moveresult->weapon) {
                    if (InFieldOfVision(bs->viewangles, 20, moveresult->ideal_viewangles)) {
                        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, target, bs->entitynum, MASK_SHOT);
                        if (bsptrace.fraction >= 1.0 || bsptrace.ent == state.number) {
                            trap_EA_Attack(bs->client);
                        }
                    }
                }
            }
        }
    }
    if (moveresult->flags & MOVERESULT_BLOCKEDBYAVOIDSPOT) {
        bs->blockedbyavoidspot_time = FloatTime() + 5;
    }
    if (bs->blockedbyavoidspot_time > FloatTime() &&
        !(moveresult->flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_MOVEMENTWEAPON))) {
        bestdist = 300;
        bestmine = -1;
        for (i = 0; i < bs->numproxmines; i++) {
            BotAI_GetEntityState(bs->proxmines[i], &state);
            VectorSubtract(state.pos.trBase, bs->origin, dir);
            dist = VectorLength(dir);
            if (dist < bestdist) {
                bestdist = dist;
                bestmine = i;
            }
        }
        if (bestmine != -1) {
            BotAI_GetEntityState(bs->proxmines[bestmine], &state);
            VectorCopy(state.pos.trBase, target);
            target[2] += 2;
            VectorSubtract(target, bs->eye, dir);
            vectoangles(dir, moveresult->ideal_viewangles);
            if (bs->inventory[INVENTORY_PLASMAGUN] > 0 && bs->inventory[INVENTORY_CELLS] > 0)
                moveresult->weapon = WEAPONINDEX_PLASMAGUN;
            else if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 && bs->inventory[INVENTORY_ROCKETS] > 0)
                moveresult->weapon = WEAPONINDEX_ROCKET_LAUNCHER;
            else if (bs->inventory[INVENTORY_BFG10K] > 0 && bs->inventory[INVENTORY_BFGAMMO] > 0)
                moveresult->weapon = WEAPONINDEX_BFG;
            else {
                moveresult->weapon = 0;
            }
            if (moveresult->weapon) {
                moveresult->flags |= MOVERESULT_MOVEMENTWEAPON | MOVERESULT_MOVEMENTVIEW;
                if (bs->cur_ps.weapon == moveresult->weapon) {
                    if (InFieldOfVision(bs->viewangles, 20, moveresult->ideal_viewangles)) {
                        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, target, bs->entitynum, MASK_SHOT);
                        if (bsptrace.fraction >= 1.0 || bsptrace.ent == state.number) {
                            trap_EA_Attack(bs->client);
                        }
                    }
                }
            }
        }
    }
}

void AIEnter_Seek_ActivateEntity(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "activate entity", "", s);
    bs->ainode = AINode_Seek_ActivateEntity;
}

int AINode_Seek_ActivateEntity(bot_state_t* bs) {
    bot_goal_t* goal;
    vec3_t target, dir, ideal_viewangles;
    bot_moveresult_t moveresult;
    int targetvisible;
    bsp_trace_t bsptrace;
    aas_entityinfo_t entinfo;

    if (BotIsObserver(bs)) {
        BotClearActivateGoalStack(bs);
        AIEnter_Observer(bs, "active entity: observer");
        return qfalse;
    }
    if (BotIntermission(bs)) {
        BotClearActivateGoalStack(bs);
        AIEnter_Intermission(bs, "activate entity: intermission");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        BotClearActivateGoalStack(bs);
        AIEnter_Respawn(bs, "activate entity: bot dead");
        return qfalse;
    }
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    BotMapScripts(bs);
    bs->enemy = -1;
    if (!bs->activatestack) {
        BotClearActivateGoalStack(bs);
        AIEnter_Seek_NBG(bs, "activate entity: no goal");
        return qfalse;
    }
    goal = &bs->activatestack->goal;
    targetvisible = qfalse;
    if (bs->activatestack->shoot) {
        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->activatestack->target, bs->entitynum, MASK_SHOT);
        if (bsptrace.fraction >= 1.0 || bsptrace.ent == goal->entitynum) {
            targetvisible = qtrue;
            if (bs->cur_ps.weapon == bs->activatestack->weapon) {
                VectorSubtract(bs->activatestack->target, bs->eye, dir);
                vectoangles(dir, ideal_viewangles);
                if (InFieldOfVision(bs->viewangles, 20, ideal_viewangles)) {
                    trap_EA_Attack(bs->client);
                }
            }
        }
    }
    if (targetvisible) {
        BotEntityInfo(goal->entitynum, &entinfo);
        if (!VectorCompare(bs->activatestack->origin, entinfo.origin)) {
#ifdef DEBUG
            BotAI_Print(PRT_MESSAGE, "hit shootable button or trigger\n");
#endif
            bs->activatestack->time = 0;
        }
        if (bs->activatestack->time < FloatTime()) {
            BotPopFromActivateGoalStack(bs);
            if (bs->activatestack) {
                bs->activatestack->time = FloatTime() + 10;
                return qfalse;
            }
            AIEnter_Seek_NBG(bs, "activate entity: time out");
            return qfalse;
        }
        memset(&moveresult, 0, sizeof(bot_moveresult_t));
    } else {
        if (!goal) {
            bs->activatestack->time = 0;
        }
        else if (!bs->activatestack->shoot) {
            if (trap_BotTouchingGoal(bs->origin, goal)) {
#ifdef DEBUG
                BotAI_Print(PRT_MESSAGE, "touched button or trigger\n");
#endif
                bs->activatestack->time = 0;
            }
        }
        if (bs->activatestack->time < FloatTime()) {
            BotPopFromActivateGoalStack(bs);
            if (bs->activatestack) {
                bs->activatestack->time = FloatTime() + 10;
                return qfalse;
            }
            AIEnter_Seek_NBG(bs, "activate entity: activated");
            return qfalse;
        }
        if (BotAIPredictObstacles(bs, goal))
            return qfalse;
        BotSetupForMovement(bs);
        trap_BotMoveToGoal(&moveresult, bs->ms, goal, bs->tfl);
        if (moveresult.failure) {
            trap_BotResetAvoidReach(bs->ms);
            bs->activatestack->time = 0;
        }
        BotAIBlocked(bs, &moveresult, qtrue);
    }
    BotClearPath(bs, &moveresult);
    if (bs->activatestack->shoot) {
        if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEW)) {
            VectorSubtract(bs->activatestack->target, bs->eye, dir);
            vectoangles(dir, moveresult.ideal_viewangles);
            moveresult.flags |= MOVERESULT_MOVEMENTVIEW;
        }
        if (!(moveresult.flags & MOVERESULT_MOVEMENTWEAPON)) {
            moveresult.flags |= MOVERESULT_MOVEMENTWEAPON;
            bs->activatestack->weapon = BotSelectActivateWeapon(bs);
            if (bs->activatestack->weapon == -1) {
                bs->activatestack->weapon = 0;
            }
            moveresult.weapon = bs->activatestack->weapon;
        }
    }
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    }
    else if (moveresult.flags & MOVERESULT_WAITING) {
        if (random() < bs->thinktime * 0.8) {
            BotRoamGoal(bs, target);
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
            bs->ideal_viewangles[2] *= 0.5;
        }
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (trap_BotMovementViewTarget(bs->ms, goal, bs->tfl, 300, target)) {
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
        } else {
            vectoangles(moveresult.movedir, bs->ideal_viewangles);
        }
        bs->ideal_viewangles[2] *= 0.5;
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    if (BotFindEnemy(bs, -1)) {
        if (BotWantsToRetreat(bs)) {
            AIEnter_Battle_NBG(bs, "activate entity: found enemy");
        } else {
            trap_BotResetLastAvoidReach(bs->ms);
            trap_BotEmptyGoalStack(bs->gs);
            AIEnter_Battle_Fight(bs, "activate entity: found enemy");
        }
        BotClearActivateGoalStack(bs);
    }
    return qtrue;
}

void AIEnter_Seek_NBG(bot_state_t* bs, char* s) {
    bot_goal_t goal;
    char buf[144];

    if (trap_BotGetTopGoal(bs->gs, &goal)) {
        trap_BotGoalName(goal.number, buf, 144);
        BotRecordNodeSwitch(bs, "seek NBG", buf, s);
    } else {
        BotRecordNodeSwitch(bs, "seek NBG", "no goal", s);
    }
    bs->ainode = AINode_Seek_NBG;
}

int AINode_Seek_NBG(bot_state_t* bs) {
    bot_goal_t goal;
    vec3_t target, dir;
    bot_moveresult_t moveresult;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "seek nbg: observer");
        return qfalse;
    }
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "seek nbg: intermision");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "seek nbg: bot dead");
        return qfalse;
    }
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    BotMapScripts(bs);
    bs->enemy = -1;
    if (!trap_BotGetTopGoal(bs->gs, &goal))
        bs->nbg_time = 0;
    else if (BotReachedGoal(bs, &goal)) {
        BotChooseWeapon(bs);
        bs->nbg_time = 0;
    }
    if (bs->nbg_time < FloatTime()) {
        trap_BotPopGoal(bs->gs);
        bs->check_time = FloatTime() + 0.05;
        AIEnter_Seek_LTG(bs, "seek nbg: time out");
        return qfalse;
    }
    if (BotAIPredictObstacles(bs, &goal))
        return qfalse;
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        bs->nbg_time = 0;
    }
    BotAIBlocked(bs, &moveresult, qtrue);
    BotClearPath(bs, &moveresult);
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    }
    else if (moveresult.flags & MOVERESULT_WAITING) {
        if (random() < bs->thinktime * 0.8) {
            BotRoamGoal(bs, target);
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
            bs->ideal_viewangles[2] *= 0.5;
        }
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (!trap_BotGetSecondGoal(bs->gs, &goal))
            trap_BotGetTopGoal(bs->gs, &goal);
        if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
        }
        else
            vectoangles(moveresult.movedir, bs->ideal_viewangles);
        bs->ideal_viewangles[2] *= 0.5;
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    if (BotFindEnemy(bs, -1)) {
        if (BotWantsToRetreat(bs)) {
            AIEnter_Battle_NBG(bs, "seek nbg: found enemy");
        } else {
            trap_BotResetLastAvoidReach(bs->ms);
            trap_BotEmptyGoalStack(bs->gs);
            AIEnter_Battle_Fight(bs, "seek nbg: found enemy");
        }
    }
    return qtrue;
}

void AIEnter_Seek_LTG(bot_state_t* bs, char* s) {
    bot_goal_t goal;
    char buf[144];

    if (trap_BotGetTopGoal(bs->gs, &goal)) {
        trap_BotGoalName(goal.number, buf, 144);
        BotRecordNodeSwitch(bs, "seek LTG", buf, s);
    } else {
        BotRecordNodeSwitch(bs, "seek LTG", "no goal", s);
    }
    bs->ainode = AINode_Seek_LTG;
}

int AINode_Seek_LTG(bot_state_t* bs) {
    bot_goal_t goal;
    vec3_t target, dir;
    bot_moveresult_t moveresult;
    int range;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "seek ltg: observer");
        return qfalse;
    }
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "seek ltg: intermission");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "seek ltg: bot dead");
        return qfalse;
    }
    if (BotChat_Random(bs)) {
        bs->stand_time = FloatTime() + BotChatTime(bs);
        AIEnter_Stand(bs, "seek ltg: random chat");
        return qfalse;
    }
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    BotMapScripts(bs);
    bs->enemy = -1;
    if (bs->killedenemy_time > FloatTime() - 2) {
        if (random() < bs->thinktime * 1) {
            trap_EA_Gesture(bs->client);
        }
    }
    if (BotFindEnemy(bs, -1)) {
        if (BotWantsToRetreat(bs)) {
            AIEnter_Battle_Retreat(bs, "seek ltg: found enemy");
            return qfalse;
        } else {
            trap_BotResetLastAvoidReach(bs->ms);
            trap_BotEmptyGoalStack(bs->gs);
            AIEnter_Battle_Fight(bs, "seek ltg: found enemy");
            return qfalse;
        }
    }
    BotTeamGoals(bs, qfalse);
    if (!BotLongTermGoal(bs, bs->tfl, qfalse, &goal)) {
        return qtrue;
    }
    if (bs->check_time < FloatTime()) {
        bs->check_time = FloatTime() + 0.5;
        BotWantsToCamp(bs);
        if (bs->ltgtype == LTG_DEFENDKEYAREA)
            range = 400;
        else
            range = 150;
#ifdef CTF
        if (gametype == GT_CTF) {
            if (BotCTFCarryingFlag(bs))
                range = 50;
        }
#endif
        else if (gametype == GT_1FCTF) {
            if (Bot1FCTFCarryingFlag(bs))
                range = 50;
        } else if (gametype == GT_HARVESTER) {
            if (BotHarvesterCarryingCubes(bs))
                range = 80;
        }
        if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
            trap_BotResetLastAvoidReach(bs->ms);
            bs->nbg_time = FloatTime() + 4 + range * 0.01;
            AIEnter_Seek_NBG(bs, "ltg seek: nbg");
            return qfalse;
        }
    }
    if (BotAIPredictObstacles(bs, &goal))
        return qfalse;
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        bs->ltg_time = 0;
    }
    BotAIBlocked(bs, &moveresult, qtrue);
    BotClearPath(bs, &moveresult);
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    }
    else if (moveresult.flags & MOVERESULT_WAITING) {
        if (random() < bs->thinktime * 0.8) {
            BotRoamGoal(bs, target);
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
            bs->ideal_viewangles[2] *= 0.5;
        }
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
        }
        else if (VectorLengthSquared(moveresult.movedir)) {
            vectoangles(moveresult.movedir, bs->ideal_viewangles);
        } else if (random() < bs->thinktime * 0.8) {
            BotRoamGoal(bs, target);
            VectorSubtract(target, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
            bs->ideal_viewangles[2] *= 0.5;
        }
        bs->ideal_viewangles[2] *= 0.5;
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    return qtrue;
}

void AIEnter_Battle_Fight(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle fight", "", s);
    trap_BotResetLastAvoidReach(bs->ms);
    bs->ainode = AINode_Battle_Fight;
    bs->flags &= ~BFL_FIGHTSUICIDAL;
}

void AIEnter_Battle_SuicidalFight(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle fight", "", s);
    trap_BotResetLastAvoidReach(bs->ms);
    bs->ainode = AINode_Battle_Fight;
    bs->flags |= BFL_FIGHTSUICIDAL;
}

int AINode_Battle_Fight(bot_state_t* bs) {
    int areanum;
    vec3_t target;
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "battle fight: observer");
        return qfalse;
    }

    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle fight: intermission");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle fight: bot dead");
        return qfalse;
    }
    if (BotFindEnemy(bs, bs->enemy)) {
#ifdef DEBUG
        BotAI_Print(PRT_MESSAGE, "found new better enemy\n");
#endif
    }
    if (bs->enemy < 0) {
        AIEnter_Seek_LTG(bs, "battle fight: no enemy");
        return qfalse;
    }
    BotEntityInfo(bs->enemy, &entinfo);
    if (bs->enemydeath_time) {
        if (bs->enemydeath_time < FloatTime() - 1.0) {
            bs->enemydeath_time = 0;
            if (bs->enemysuicide) {
                BotChat_EnemySuicide(bs);
            }
            if (bs->lastkilledplayer == bs->enemy && BotChat_Kill(bs)) {
                bs->stand_time = FloatTime() + BotChatTime(bs);
                AIEnter_Stand(bs, "battle fight: enemy dead");
            } else {
                bs->ltg_time = 0;
                AIEnter_Seek_LTG(bs, "battle fight: enemy dead");
            }
            return qfalse;
        }
    } else {
        if (EntityIsDead(&entinfo)) {
            bs->enemydeath_time = FloatTime();
        }
    }
    if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
        if (random() < 0.2) {
            AIEnter_Seek_LTG(bs, "battle fight: invisible");
            return qfalse;
        }
    }
    VectorCopy(entinfo.origin, target);
    if (bs->enemy >= MAX_CLIENTS) {
        if (bs->enemy == redobelisk.entitynum ||
            bs->enemy == blueobelisk.entitynum) {
            target[2] += 16;
        }
    }
    areanum = BotPointAreaNum(target);
    if (areanum && trap_AAS_AreaReachability(areanum)) {
        VectorCopy(target, bs->lastenemyorigin);
        bs->lastenemyareanum = areanum;
    }
    BotUpdateBattleInventory(bs, bs->enemy);
    if (bs->lastframe_health > bs->inventory[INVENTORY_HEALTH]) {
        if (BotChat_HitNoDeath(bs)) {
            bs->stand_time = FloatTime() + BotChatTime(bs);
            AIEnter_Stand(bs, "battle fight: chat health decreased");
            return qfalse;
        }
    }
    if (bs->cur_ps.persistant[PERS_HITS] > bs->lasthitcount) {
        if (BotChat_HitNoKill(bs)) {
            bs->stand_time = FloatTime() + BotChatTime(bs);
            AIEnter_Stand(bs, "battle fight: chat hit someone");
            return qfalse;
        }
    }
    if (!BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        if (bs->enemy == redobelisk.entitynum || bs->enemy == blueobelisk.entitynum) {
            AIEnter_Battle_Chase(bs, "battle fight: obelisk out of sight");
            return qfalse;
        }
        if (BotWantsToChase(bs)) {
            AIEnter_Battle_Chase(bs, "battle fight: enemy out of sight");
            return qfalse;
        } else {
            AIEnter_Seek_LTG(bs, "battle fight: enemy out of sight");
            return qfalse;
        }
    }
    BotBattleUseItems(bs);
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    BotChooseWeapon(bs);
    moveresult = BotAttackMove(bs, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        bs->ltg_time = 0;
    }
    BotAIBlocked(bs, &moveresult, qfalse);
    BotAimAtEnemy(bs);
    BotCheckAttack(bs);
    if (!(bs->flags & BFL_FIGHTSUICIDAL)) {
        if (BotWantsToRetreat(bs)) {
            AIEnter_Battle_Retreat(bs, "battle fight: wants to retreat");
            return qtrue;
        }
    }
    return qtrue;
}

void AIEnter_Battle_Chase(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle chase", "", s);
    bs->chase_time = FloatTime();
    bs->ainode = AINode_Battle_Chase;
}

int AINode_Battle_Chase(bot_state_t* bs) {
    bot_goal_t goal;
    vec3_t target, dir;
    bot_moveresult_t moveresult;
    float range;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "battle chase: observer");
        return qfalse;
    }
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle chase: intermission");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle chase: bot dead");
        return qfalse;
    }
    if (bs->enemy < 0) {
        AIEnter_Seek_LTG(bs, "battle chase: no enemy");
        return qfalse;
    }
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        AIEnter_Battle_Fight(bs, "battle chase");
        return qfalse;
    }
    if (BotFindEnemy(bs, -1)) {
        AIEnter_Battle_Fight(bs, "battle chase: better enemy");
        return qfalse;
    }
    if (!bs->lastenemyareanum) {
        AIEnter_Seek_LTG(bs, "battle chase: no enemy area");
        return qfalse;
    }
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    BotMapScripts(bs);
    memset(&goal, 0, sizeof(goal));
    goal.entitynum = bs->enemy;
    if (!bs->enemyHeard) {
        goal.areanum = bs->lastenemyareanum;
        VectorCopy(bs->lastenemyorigin, goal.origin);
        VectorSet(goal.mins, -8, -8, -8);
        VectorSet(goal.maxs, 8, 8, 8);
    } else {
        aas_entityinfo_t heard;

        BotEntityInfo(bs->enemy, &heard);
        goal.areanum = BotPointAreaNum(heard.origin);
        VectorCopy(heard.origin, goal.origin);
        VectorCopy(heard.mins, goal.mins);
        VectorCopy(heard.maxs, goal.maxs);
        bs->lastenemyareanum = goal.areanum;
    }
    if (trap_BotTouchingGoal(bs->origin, &goal))
        bs->chase_time = 0;
    if (!bs->chase_time || bs->chase_time < FloatTime() - 10) {
        AIEnter_Seek_LTG(bs, "battle chase: time out");
        return qfalse;
    }
    if (bs->check_time < FloatTime()) {
        bs->check_time = FloatTime() + 1;
        range = 150;
        if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
            bs->nbg_time = FloatTime() + 0.1 * range + 1;
            trap_BotResetLastAvoidReach(bs->ms);
            AIEnter_Battle_NBG(bs, "battle chase: nbg");
            return qfalse;
        }
    }
    BotUpdateBattleInventory(bs, bs->enemy);
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        bs->ltg_time = 0;
    }
    BotAIBlocked(bs, &moveresult, qfalse);
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (bs->chase_time > FloatTime() - 2) {
            BotAimAtEnemy(bs);
        } else {
            if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl,
                                           bs->cur_ps.groundEntityNum == ENTITYNUM_NONE ? 2048 : 300, target)) {
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
            } else {
                vectoangles(moveresult.movedir, bs->ideal_viewangles);
            }
        }
        bs->ideal_viewangles[2] *= 0.5;
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    if (bs->areanum == bs->lastenemyareanum)
        bs->chase_time = 0;
    if (BotWantsToRetreat(bs)) {
        AIEnter_Battle_Retreat(bs, "battle chase: wants to retreat");
        return qtrue;
    }
    return qtrue;
}

void AIEnter_Battle_Retreat(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle retreat", "", s);
    bs->ainode = AINode_Battle_Retreat;
}

int AINode_Battle_Retreat(bot_state_t* bs) {
    bot_goal_t goal;
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;
    vec3_t target, dir;
    float attack_skill, range;
    int areanum;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "battle retreat: observer");
        return qfalse;
    }
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle retreat: intermission");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle retreat: bot dead");
        return qfalse;
    }
    if (bs->enemy < 0) {
        AIEnter_Seek_LTG(bs, "battle retreat: no enemy");
        return qfalse;
    }
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsDead(&entinfo)) {
        AIEnter_Seek_LTG(bs, "battle retreat: enemy dead");
        return qfalse;
    }
    if (BotFindEnemy(bs, bs->enemy)) {
#ifdef DEBUG
        BotAI_Print(PRT_MESSAGE, "found new better enemy\n");
#endif
    }
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    BotMapScripts(bs);
    BotUpdateBattleInventory(bs, bs->enemy);
    if (BotWantsToChase(bs)) {
        trap_BotEmptyGoalStack(bs->gs);
        AIEnter_Battle_Chase(bs, "battle retreat: wants to chase");
        return qfalse;
    }
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        bs->enemyvisible_time = FloatTime();
        VectorCopy(entinfo.origin, target);
        if (bs->enemy >= MAX_CLIENTS) {
            if (bs->enemy == redobelisk.entitynum ||
                bs->enemy == blueobelisk.entitynum) {
                target[2] += 16;
            }
        }
        areanum = BotPointAreaNum(target);
        if (areanum && trap_AAS_AreaReachability(areanum)) {
            VectorCopy(target, bs->lastenemyorigin);
            bs->lastenemyareanum = areanum;
        }
    }
    if (bs->enemyvisible_time < FloatTime() - 4) {
        AIEnter_Seek_LTG(bs, "battle retreat: lost enemy");
        return qfalse;
    }
    else if (bs->enemyvisible_time < FloatTime()) {
        if (BotFindEnemy(bs, -1)) {
            AIEnter_Battle_Fight(bs, "battle retreat: another enemy");
            return qfalse;
        }
    }
    BotTeamGoals(bs, qtrue);
    BotBattleUseItems(bs);
    if (!BotLongTermGoal(bs, bs->tfl, qtrue, &goal)) {
        AIEnter_Battle_SuicidalFight(bs, "battle retreat: no way out");
        return qfalse;
    }
    if (bs->check_time < FloatTime()) {
        bs->check_time = FloatTime() + 1;
        range = 150;
        if (gametype == GT_CTF || gametype == GT_AD) {
            if (BotCTFCarryingFlag(bs))
                range = 50;
        }
        else if (gametype == GT_1FCTF) {
            if (Bot1FCTFCarryingFlag(bs))
                range = 50;
        } else if (gametype == GT_HARVESTER) {
            if (BotHarvesterCarryingCubes(bs))
                range = 80;
        }
        if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
            trap_BotResetLastAvoidReach(bs->ms);
            bs->nbg_time = FloatTime() + range / 100 + 1;
            AIEnter_Battle_NBG(bs, "battle retreat: nbg");
            return qfalse;
        }
    }
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        bs->ltg_time = 0;
    }
    BotAIBlocked(bs, &moveresult, qfalse);
    BotChooseWeapon(bs);
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEWSET) && !(bs->flags & BFL_IDEALVIEWSET)) {
        attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
        if (attack_skill > 0.3) {
            BotAimAtEnemy(bs);
        } else {
            if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl,
                                           bs->cur_ps.groundEntityNum == ENTITYNUM_NONE ? 2048 : 300, target)) {
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
            } else {
                vectoangles(moveresult.movedir, bs->ideal_viewangles);
            }
            bs->ideal_viewangles[2] *= 0.5;
        }
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    BotCheckAttack(bs);
    return qtrue;
}

void AIEnter_Battle_NBG(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle NBG", "", s);
    bs->ainode = AINode_Battle_NBG;
}

int AINode_Battle_NBG(bot_state_t* bs) {
    int areanum;
    bot_goal_t goal;
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;
    float attack_skill;
    vec3_t target, dir;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "battle nbg: observer");
        return qfalse;
    }
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle nbg: intermission");
        return qfalse;
    }
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle nbg: bot dead");
        return qfalse;
    }
    if (bs->enemy < 0) {
        AIEnter_Seek_NBG(bs, "battle nbg: no enemy");
        return qfalse;
    }
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsDead(&entinfo)) {
        AIEnter_Seek_NBG(bs, "battle nbg: enemy dead");
        return qfalse;
    }
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    BotMapScripts(bs);
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        bs->enemyvisible_time = FloatTime();
        VectorCopy(entinfo.origin, target);
        if (bs->enemy >= MAX_CLIENTS) {
            if (bs->enemy == redobelisk.entitynum ||
                bs->enemy == blueobelisk.entitynum) {
                target[2] += 16;
            }
        }
        areanum = BotPointAreaNum(target);
        if (areanum && trap_AAS_AreaReachability(areanum)) {
            VectorCopy(target, bs->lastenemyorigin);
            bs->lastenemyareanum = areanum;
        }
    }
    if (!trap_BotGetTopGoal(bs->gs, &goal)) {
        bs->nbg_time = 0;
    } else if (BotReachedGoal(bs, &goal)) {
        bs->nbg_time = 0;
    }
    if (bs->nbg_time < FloatTime()) {
        trap_BotPopGoal(bs->gs);
        if (trap_BotGetTopGoal(bs->gs, &goal))
            AIEnter_Battle_Retreat(bs, "battle nbg: time out");
        else
            AIEnter_Battle_Fight(bs, "battle nbg: time out");
        return qfalse;
    }
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        bs->nbg_time = 0;
    }
    BotAIBlocked(bs, &moveresult, qfalse);
    BotUpdateBattleInventory(bs, bs->enemy);
    BotChooseWeapon(bs);
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEWSET) && !(bs->flags & BFL_IDEALVIEWSET)) {
        attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
        if (attack_skill > 0.3) {
            BotAimAtEnemy(bs);
        } else {
            if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl,
                                           bs->cur_ps.groundEntityNum == ENTITYNUM_NONE ? 2048 : 300, target)) {
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
            } else {
                vectoangles(moveresult.movedir, bs->ideal_viewangles);
            }
            bs->ideal_viewangles[2] *= 0.5;
        }
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    BotCheckAttack(bs);
    return qtrue;
}

int FindInstaGibTarget(bot_state_t* bs) {
    int i, besttarget, area, numareas;
    float bestdist, dist;
    vec3_t dir, end;
    aas_entityinfo_t entinfo;
    gentity_t* ent;
    int areas[10];

    bestdist = 100000000.0;
    besttarget = -1;

    if (bs->enemy != -1 &&
        BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        BotEntityInfo(bs->enemy, &entinfo);
        VectorSubtract(bs->origin, entinfo.origin, dir);
        area = BotPointAreaNum(entinfo.origin);
        if (area > 0 &&
            trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, area, TFL_DEFAULT) > 0) {
            bestdist = VectorLength(dir);
            besttarget = bs->enemy;
        }
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (i == bs->entitynum) {
            continue;
        }
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid) {
            continue;
        }
        if (entinfo.number < MAX_CLIENTS) {
            ent = &g_entities[entinfo.number];
            if (ent->inuse && ent->client && ent->client->ps.pm_type != PM_NORMAL) {
                continue;
            }
        }
        VectorSubtract(bs->origin, entinfo.origin, dir);
        dist = VectorLength(dir);
        if (dist < bestdist) {
            area = BotPointAreaNum(entinfo.origin);
            if (area != 0 &&
                trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, area, TFL_DEFAULT) != 0) {
                bestdist = dist;
                besttarget = i;
            }
        }
    }

    if (besttarget == -1) {
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (i == bs->entitynum) {
                continue;
            }
            BotEntityInfo(i, &entinfo);
            if (!entinfo.valid) {
                continue;
            }
            VectorSubtract(bs->origin, entinfo.origin, dir);
            dist = VectorLength(dir);
            if (dist < bestdist) {
                area = trap_AAS_PointAreaNum(entinfo.origin);
                if (area == 0) {
                    VectorCopy(entinfo.origin, end);
                    end[2] += 10;
                    numareas = trap_AAS_TraceAreas(entinfo.origin, end, areas, NULL, 10);
                    if (numareas < 1 || !areas[0]) {
                        continue;
                    }
                    area = areas[0];
                }
                if (trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, area, TFL_DEFAULT) != 0) {
                    bestdist = dist;
                    besttarget = i;
                }
            }
        }
    }
    return besttarget;
}

void AIEnter_InstaGib(bot_state_t* bs, char* s) {
    bs->ltg_time = FloatTime() + 99999;
    bs->ltgtype = 21;
    bs->ainode = AINode_InstaGib;
    BotRecordNodeSwitch(bs, "insta gib!", "", s);
}

int AINode_InstaGib(bot_state_t* bs) {
    int target;
    bot_goal_t goal;
    vec3_t dir, target_org;
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;

    target = FindInstaGibTarget(bs);

    if (BotIsObserver(bs)) {
        bs->ltgtype = 0;
        AIEnter_Observer(bs, "insta gib!: observer");
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_INTERMISSION) {
        g_entities[bs->client].flags &= ~FL_NOPICKUP;
        g_entities[bs->client].client->ps.powerups[6] = 0;
        g_entities[bs->client].client->ps.powerups[7] = 0;
        bs->ltgtype = 0;
        AIEnter_Intermission(bs, "insta gib!: intermission");
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_DEAD) {
        g_entities[bs->client].flags &= ~FL_NOPICKUP;
        g_entities[bs->client].client->ps.powerups[6] = 0;
        g_entities[bs->client].client->ps.powerups[7] = 0;
        bs->ltgtype = 0;
        AIEnter_Respawn(bs, "insta gib!: bot dead");
        return qfalse;
    }
    if (!g_instaGib.integer) {
        bs->ltgtype = 0;
        AIEnter_Seek_LTG(bs, "insta gib!: no ai node");
        return qfalse;
    }
    if (BotFindEnemy(bs, -1)) {
        bs->ltgtype = 0;
        trap_BotResetLastAvoidReach(bs->ms);
        trap_BotEmptyGoalStack(bs->gs);
        AIEnter_Battle_Fight(bs, "insta gib!: found enemy");
        return qfalse;
    }
    if (target == -1) {
        if (random() < bs->thinktime * 0.8) {
            BotAimAtEnemy(bs);
        }
        return qtrue;
    }
    bs->tfl = TFL_DEFAULT;
    BotEntityInfo(target, &entinfo);
    trap_BotEmptyGoalStack(bs->gs);
    goal.areanum = BotPointAreaNum(entinfo.origin);
    VectorCopy(entinfo.origin, goal.origin);
    goal.entitynum = target;
    VectorSet(goal.mins, 0, 0, 0);
    VectorSet(goal.maxs, 0, 0, 0);
    goal.number = 0;
    goal.flags = 0;
    goal.iteminfo = 0;
    trap_BotPushGoal(bs->gs, &goal);
    VectorSubtract(bs->origin, entinfo.origin, dir);
    if (VectorLength(dir) < 50) {
        if (random() < bs->thinktime * 0.8) {
            BotAimAtEnemy(bs);
            return qtrue;
        }
    }
    if (BotInLavaOrSlime(bs)) {
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    }
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    if (BotAIPredictObstacles(bs, &goal)) {
        bs->ltgtype = 0;
        return qfalse;
    }
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        BotAimAtEnemy(bs);
        return qtrue;
    }
    BotAIBlocked(bs, &moveresult, qtrue);
    BotClearPath(bs, &moveresult);
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (moveresult.flags & MOVERESULT_WAITING) {
        if (random() < bs->thinktime * 0.8) {
            BotAimAtEnemy(bs);
        }
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target_org)) {
            VectorSubtract(target_org, bs->origin, dir);
            vectoangles(dir, bs->ideal_viewangles);
        } else if (VectorLengthSquared(moveresult.movedir)) {
            vectoangles(moveresult.movedir, bs->ideal_viewangles);
        } else if (random() < bs->thinktime * 0.8) {
            BotAimAtEnemy(bs);
        }
    }
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) {
        bs->weaponnum = moveresult.weapon;
    }
    return qtrue;
}

void BotSetFreezeState(bot_state_t* bs, int freeze) {
    gentity_t* ent;
    gclient_t* client;

    ent = &g_entities[bs->client];
    client = ent->client;
    if (freeze) {
        ent->flags |= FL_GODMODE | FL_NOPICKUP;
        client->ps.ammo[5] = 9999;
        client->ps.stats[STAT_WEAPONS] |= 0x20;
        ent->flags |= FL_TRAINER | FL_NO_KNOCKBACK;
        client->ps.stats[STAT_WEAPONS] |= 0x8000;
    } else {
        ent->flags &= ~(FL_GODMODE | FL_NOPICKUP);
        client->ps.ammo[3] = 10;
        ent->flags &= ~(FL_TRAINER | FL_NO_KNOCKBACK);
    }
}

static void BotTrainerRelease(bot_state_t* bs) {
    gentity_t* ent = &g_entities[bs->client];

    ent->flags &= ~(FL_GODMODE | FL_NOPICKUP);
    ent->client->ps.ammo[WP_SHOTGUN] = 10;
    ent->flags &= ~(FL_TRAINER | FL_NO_KNOCKBACK);
}

static void BotTourLookAt(bot_state_t* bs, vec3_t point) {
    vec3_t dir;

    VectorSubtract(point, bs->origin, dir);
    vectoangles(dir, bs->ideal_viewangles);
    bs->ideal_viewangles[2] *= 0.5f;
}

static qboolean BotTourTouching(int client, int other) {
    aas_entityinfo_t a, b;

    trap_AAS_EntityInfo(client, &a);
    trap_AAS_EntityInfo(other, &b);
    return b.origin[2] + b.mins[2] < a.origin[2] + a.maxs[2] &&
           b.origin[0] + b.mins[0] - 4 < a.origin[0] + a.maxs[0] &&
           a.origin[0] + a.mins[0] < b.origin[0] + b.maxs[0] + 4 &&
           b.origin[1] + b.mins[1] - 4 < a.origin[1] + a.maxs[1] &&
           a.origin[1] + a.mins[1] < b.origin[1] + b.maxs[1] + 4 &&
           b.origin[2] + b.mins[2] - 4 < a.origin[2] + a.maxs[2] &&
           a.origin[2] + a.mins[2] < b.origin[2] + b.maxs[2] + 4;
}

static void BotTourSaveGoal(bot_state_t* bs) {
    VectorCopy(bs->origin, bs->tourSavedGoal.origin);
    bs->tourSavedGoal.areanum = bs->areanum;
    VectorSet(bs->tourSavedGoal.mins, -16, -16, -24);
    VectorSet(bs->tourSavedGoal.maxs, 16, 16, 32);
}

static int BotTourGoal(int point, vec3_t look, vec3_t floor) {
    gentity_t* ent = NULL;
    gentity_t* target;
    trace_t tr;
    vec3_t end;

    VectorCopy(vec3_origin, look);
    if ((unsigned)(point - MAX_CLIENTS) < (unsigned)(MAX_GENTITIES - MAX_CLIENTS)) {
        gentity_t* from = &g_entities[point];

        if (!from->inuse) {
            return -1;
        }
        do {
            ent = G_Find(ent, FOFS(tourPointTargetName), from->tourPointTarget);
            if (!ent) {
                return -1;
            }
        } while (Q_stricmp(ent->classname, "info_tour_point"));
        if (ent == from) {
            return -1;
        }
    } else {
        do {
            ent = G_Find(ent, FOFS(classname), "info_tour_point");
            if (!ent) {
                return -1;
            }
        } while (!(ent->spawnflags & 1));
    }

    VectorCopy(ent->r.currentOrigin, end);
    end[2] -= 8192;
    trap_Trace(&tr, ent->r.currentOrigin, NULL, NULL, end, 0, CONTENTS_SOLID);
    VectorCopy(tr.endpos, floor);
    floor[2] = tr.endpos[2] + 16;

    target = NULL;
    while ((target = G_Find(target, FOFS(targetname), ent->target)) != NULL) {
        if (!Q_stricmp(target->classname, "info_notnull")) {
            VectorCopy(target->s.origin, look);
            break;
        }
    }
    return ent->s.number;
}

void AIEnter_Training(bot_state_t* bs) {
    gentity_t* ent = &g_entities[bs->client];

    bs->ltg_time = FloatTime() + 99999;
    bs->tourFollowDist = 150;
    bs->ltgtype = LTG_TRAINING;
    bs->nodeEnterTime = 0;
    bs->tourStopReached = 0;
    bs->tourNextTime = 0;
    bs->tourLookUntil = 0;
    bs->tourSavedLookSet = 0;
    bs->tourGoBack = 0;
    bs->tourLostPlayer = 0;
    bs->tourReturning = 0;
    bs->tourPlayerNear = 0;
    bs->tourSaidCareful = 0;
    bs->ainode = AINode_Training;

    ent->flags |= FL_GODMODE | FL_NOPICKUP;
    ent->client->ps.ammo[WP_ROCKET_LAUNCHER] = 9999;
    ent->client->ps.stats[STAT_WEAPONS] |= 1 << WP_ROCKET_LAUNCHER;
    ent->flags |= FL_TRAINER | FL_NO_KNOCKBACK;
    ent->client->ps.stats[STAT_WEAPONS] |= 1 << 15;

    bs->tourAnnounced = 0;
    bs->tourWait = 0;
    bs->tourWaitForPlayer = 0;
    bs->tourPoint = BotTourGoal(bs->tourPrevPoint, bs->tourLookTarget, bs->tourGoal);
    memset(&bs->tourSavedGoal, 0, sizeof(bs->tourSavedGoal));
    VectorCopy(bs->tourGoal, bs->tourPrevGoal);
    BotRecordNodeSwitch(bs, "lead teammate", "", "");
}

static void BotTourGoalOnEntity(bot_goal_t* goal, aas_entityinfo_t* info, int entitynum) {
    VectorCopy(info->origin, goal->origin);
    goal->areanum = BotPointAreaNum(info->origin);
    VectorCopy(info->mins, goal->mins);
    VectorCopy(info->maxs, goal->maxs);
    goal->entitynum = entitynum;
    goal->number = info->number;
    goal->flags = 0;
    goal->iteminfo = 0;
}

static void BotTourMoveView(bot_state_t* bs, bot_moveresult_t* moveresult, bot_goal_t* goal, float lookahead) {
    vec3_t target;

    BotAIBlocked(bs, moveresult, qtrue);
    BotClearPath(bs, moveresult);
    if (moveresult->flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult->ideal_viewangles, bs->ideal_viewangles);
    } else if (moveresult->flags & MOVERESULT_WAITING) {
        if (random() < bs->thinktime * 0.8) {
            BotRoamGoal(bs, target);
            BotTourLookAt(bs, target);
        }
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (trap_BotMovementViewTarget(bs->ms, goal, bs->tfl, lookahead, target)) {
            BotTourLookAt(bs, target);
        } else if (VectorLengthSquared(moveresult->movedir) != 0) {
            vectoangles(moveresult->movedir, bs->ideal_viewangles);
        } else if (random() < bs->thinktime * 0.8) {
            BotRoamGoal(bs, target);
            BotTourLookAt(bs, target);
        }
    }
    if (moveresult->flags & MOVERESULT_MOVEMENTWEAPON) {
        bs->weaponnum = moveresult->weapon;
    }
}

int AINode_Training(bot_state_t* bs) {
    int human = G_TrainingHumanClient();
    int keepGoing, announced = 0, botFarther, canSee, ignore, flag2, flag4;
    gentity_t* point;
    entityState_t hstate;
    aas_entityinfo_t hinfo, binfo;
    bot_goal_t goal;
    bot_moveresult_t moveresult;
    bsp_trace_t tr;
    vec3_t pointOrigin, d, end;
    float radius, wait, cvarValue, chatTime, distBotHuman, distBotGoal, distHumanGoal;
    char* message;
    char* cvar;

    if (BotIsObserver(bs)) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        bs->tourPoint = -1;
        BotRecordNodeSwitch(bs, "observer", "", "lead teammate: observer");
        BotResetState(bs);
        bs->ainode = AINode_Observer;
        bs->ltgtype = 0;
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_INTERMISSION) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        bs->tourPoint = -1;
        AIEnter_Intermission(bs, "lead teammate: intermission");
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_DEAD) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        AIEnter_Respawn(bs, "lead teammate: bot dead");
        return qfalse;
    }
    if (bot_training.integer != 1 && bot_training.integer != 2) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        bs->tourPoint = -1;
        AIEnter_Seek_LTG(bs, "lead teammate: no ai node");
        return qfalse;
    }
    if (human == -1) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        trap_Cvar_Set("bot_training", "0");
        AIEnter_Seek_LTG(bs, "lead teammate: no client");
        return qfalse;
    }
    if (bs->tourPoint == -1) {
        trap_Cvar_Set("bot_training", "0");
        trap_Cvar_Set("bot_dynamicSkill", "1");
        trap_Cvar_Set("bot_followMe", "0");
        g_entities[human].client->pers.ready = qtrue;
        BotTrainerRelease(bs);
        g_entities[human].flags &= ~FL_GODMODE;
        return qtrue;
    }
    point = &g_entities[bs->tourPoint];
    if (!point->inuse) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        trap_Cvar_Set("bot_training", "0");
        trap_Cvar_Set("bot_followMe", "1");
        AIEnter_FollowMe(bs);
        return qfalse;
    }

    flag4 = (point->spawnflags >> 2) & 1;
    flag2 = (point->spawnflags >> 1) & 1;
    radius = point->tourRadius;
    wait = point->wait;
    cvarValue = point->tourCvarValue;
    ignore = point->tourIgnorePlayer == 1;
    VectorCopy(point->r.currentOrigin, pointOrigin);
    message = point->message;
    cvar = point->cvar;
    chatTime = point->tourChatTime;

    bs->tfl = TFL_DEFAULT;
    bs->weaponnum = bot_gauntlet.integer ? 15 : WP_ROCKET_LAUNCHER;
    g_entities[human].flags |= FL_GODMODE;
    BotAI_GetEntityState(human, &hstate);
    trap_AAS_EntityInfo(human, &hinfo);
    trap_AAS_EntityInfo(bs->client, &binfo);

    if (hstate.eFlags & EF_DEAD) {
        if (!bs->tourSaidCareful && hstate.groundEntityNum != ENTITYNUM_NONE) {
            trap_EA_Command(bs->client, va("botSay %i %i %s", human, 5, G_TrainingText("You've got to be careful in the Arena!")));
            bs->tourSaidCareful = 1;
        }
        BotTourLookAt(bs, hinfo.origin);
        return qtrue;
    }
    bs->tourSaidCareful = 0;

    if (FloatTime() < (float)bs->tourNextTime) {
        BotTourLookAt(bs, hinfo.origin);
        return qtrue;
    }
    if (FloatTime() < bs->tourLookUntil) {
        VectorCopy(bs->tourPrevGoal, d);
        d[2] -= 32;
        BotTourLookAt(bs, d);
        if (FloatTime() + 0.3 > bs->tourLookUntil) {
            VectorCopy(bs->tourPrevGoal, end);
            end[2] += 32;
            BotAI_Trace(&tr, bs->eye, NULL, NULL, end, bs->client, 0x6000001);
            if (tr.fraction == 1.0f) {
                trap_EA_Attack(bs->client);
            }
        }
        return qtrue;
    }

    VectorSubtract(hinfo.origin, bs->origin, d);
    distBotHuman = VectorLength(d);
    VectorSubtract(bs->tourGoal, bs->origin, d);
    if (!(point->spawnflags & 16)) {
        d[2] = 0;
    }
    distBotGoal = VectorLength(d);
    VectorSubtract(bs->tourGoal, hinfo.origin, d);
    if (!(point->spawnflags & 16)) {
        d[2] = 0;
    }
    distHumanGoal = VectorLength(d);
    keepGoing = 1;
    if (radius > distBotGoal && bs->tourPoint != 0) {
        keepGoing = 0;
    }
    botFarther = distBotGoal > distHumanGoal && distHumanGoal > radius;

    canSee = (int)BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, human);
    BotAI_Trace(&tr, bs->eye, NULL, NULL, pointOrigin, bs->client, CONTENTS_SOLID);
    if (tr.fraction != 1.0f) {
        botFarther = 1;
    }
    if (!bs->tourGoBack) {
        canSee = 1;
    }

    trap_BotEmptyGoalStack(bs->gs);
    if (bs->tourReturning) {
        VectorSubtract(bs->tourSavedGoal.origin, bs->origin, d);
        d[2] = 0;
        if (VectorLength(d) > 16) {
            goal = bs->tourSavedGoal;
            goto pushgoal;
        }
        bs->tourReturning = 0;
    }
    if (!ignore && (!canSee || !bs->tourPlayerNear || bs->tourLostPlayer ||
                    (bs->tourPoint == 0 && distBotHuman > 128))) {
        keepGoing = 1;
        BotTourGoalOnEntity(&goal, &hinfo, human);
    } else {
        if ((distBotGoal > radius || botFarther) && bs->tourPoint != 0) {
            announced = 1;
        }
        if (ignore && distBotGoal > radius) {
            announced = 1;
            botFarther = 1;
        }
        keepGoing = 0;
        goal.areanum = BotPointAreaNum(bs->tourGoal);
        if (!goal.areanum) {
            goal.areanum = BotPointAreaNum(pointOrigin);
        }
        VectorCopy(bs->tourGoal, goal.origin);
        VectorCopy(point->r.mins, goal.mins);
        VectorCopy(point->r.maxs, goal.maxs);
        goal.entitynum = bs->tourPoint;
        goal.number = bs->tourPoint;
        goal.flags = 0;
        goal.iteminfo = 0;
    }

pushgoal:
    trap_BotPushGoal(bs->gs, &goal);
    if (BotInLavaOrSlime(bs)) {
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    }
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    bs->enemy = -1;
    if (BotAIPredictObstacles(bs, &goal)) {
        bs->ltgtype = 0;
        return qfalse;
    }
    if (bs->tourReturning) {
        goto movetogoal;
    }

    if (keepGoing) {
        if (bs->tourFollowDist > distBotHuman || (float)bs->tourWaitForPlayer > FloatTime()) {
            bs->tourPlayerNear = 1;
            BotTourLookAt(bs, hinfo.origin);
            if ((float)bs->tourWaitForPlayer > FloatTime() && FloatTime() > bs->tourNextSay) {
                if (FloatTime() > bs->tourLastNudge + 10 && random() < bs->thinktime * 1.9) {
                    trap_EA_Jump(bs->client);
                    bs->tourLastNudge = FloatTime();
                    trap_EA_Command(bs->client, G_TrainingSay("say This way! No time to waste!"));
                    trap_SendServerCommand(human, "playSound sound/vo/crash/41a.wav");
                }
                return qtrue;
            }
            if (BotTourTouching(bs->client, human)) {
                vec3_t forward, dir;

                AngleVectors(hinfo.angles, forward, NULL, NULL);
                forward[2] = 0;
                VectorNormalize(forward);
                VectorSubtract(bs->origin, hinfo.origin, dir);
                VectorNormalize(dir);
                if (DotProduct(forward, dir) > 0.7) {
                    BotSetupForMovement(bs);
                    trap_BotMoveInDirection(bs->ms, dir, 400, MOVE_WALK);
                }
            }
            return qtrue;
        }
        if (!announced) {
            goto movetogoal;
        }
    } else if (!announced) {
        goto atstop;
    }

    if (bs->tourPoint != bs->tourAnnounced) {
        if (bs->tourAnnounced && bs->tourWait && (g_entities[bs->tourAnnounced].spawnflags & 32)) {
            trap_EA_Gesture(bs->client);
            trap_EA_Command(bs->client, G_TrainingSay("say Let's continue on to the next stop in the tour! Follow me!"));
            bs->tourNextTime = (int)(FloatTime() + 3.0);
            bs->tourNextSay = FloatTime() + 7.0;
        }
        bs->tourAnnounced = bs->tourPoint;
        return qtrue;
    }
    if (distBotHuman > 320 && !botFarther && !ignore) {
        BotTourLookAt(bs, hinfo.origin);
        if (FloatTime() > bs->tourNextSay && FloatTime() > bs->tourLastNudge + 10 &&
            random() < bs->thinktime * 1.9) {
            bs->tourLastNudge = FloatTime();
            trap_EA_Command(bs->client, va("botSay %i %i %s", human, 5, G_TrainingText("This way! No time to waste!")));
            trap_SendServerCommand(human, "playSound sound/vo/crash/41a.wav");
            if (bs->tourGoBack) {
                bs->tourLostPlayer = 1;
                BotTourSaveGoal(bs);
            } else {
                trap_EA_Jump(bs->client);
            }
        }
        return qtrue;
    }
    if (FloatTime() > bs->tourNextSay && distBotHuman > 320 && distHumanGoal > distBotGoal && !ignore &&
        FloatTime() > bs->tourLastNudge + 10) {
        BotTourLookAt(bs, hinfo.origin);
        bs->tourLastNudge = FloatTime();
        trap_EA_Command(bs->client, G_TrainingSay("say This way! No time to waste!"));
        trap_SendServerCommand(human, "playSound sound/vo/crash/41a.wav");
        if (bs->tourGoBack) {
            bs->tourLostPlayer = 1;
            BotTourSaveGoal(bs);
        }
        return qtrue;
    }
    if (FloatTime() > bs->tourNextSay && !ignore) {
        int i;

        for (i = 0; i < 20; i++) {
            aas_predictroute_t route;

            trap_AAS_PredictRoute(&route, bs->areanum, bs->origin, goal.areanum, bs->tfl, 1,
                                  (int)(1000.0 / sv_fps.integer), 0, 0, 0, 0);
            if (BotEntityVisible(bs->entitynum, route.endpos, bs->viewangles, 360, human) != 0) {
                continue;
            }
            if ((route.endtravelflags & TFL_WALKOFFLEDGE) ||
                (route.endtravelflags & (TFL_FUNCBOB | TFL_JUMPPAD | TFL_TELEPORT))) {
                bs->tourWaitForPlayer = (int)(FloatTime() + 10);
                continue;
            }
            BotTourLookAt(bs, hinfo.origin);
            if (FloatTime() > bs->tourNextSay && FloatTime() > bs->tourLastNudge + 10 &&
                random() < bs->thinktime * 1.9) {
                trap_EA_Jump(bs->client);
                bs->tourLastNudge = FloatTime();
                trap_EA_Command(bs->client, G_TrainingSay("say This way! No time to waste!"));
            }
            return qtrue;
        }
    }
    goto movetogoal;

atstop:
    if (!bs->tourStopReached) {
        vec3_t mins, maxs;
        int touch[1024];
        int num, i;
        gentity_t* ent;

        VectorAdd(goal.origin, goal.mins, mins);
        VectorAdd(goal.origin, goal.maxs, maxs);
        num = trap_EntitiesInBox(mins, maxs, touch, 1024);
        for (i = 0; i < num; i++) {
            ent = &g_entities[touch[i]];
            if (ent->s.eType == ET_ITEM && (ent->s.eFlags & EF_NODRAW) && ent->item &&
                ent->item->giType != IT_TEAM && ent->item->giType != IT_PERSISTANT_POWERUP) {
                ent->use(ent, NULL, NULL);
            }
        }
        ent = NULL;
        while ((ent = G_Find(ent, FOFS(targetname), point->target)) != NULL) {
            if (ent->item && (ent->s.eFlags & EF_NODRAW)) {
                ent->use(ent, NULL, NULL);
            }
        }
        bs->tourStopReached = 1;
        bs->tourReachTime = FloatTime();
        ent = NULL;
        while ((ent = G_Find(ent, FOFS(targetname), point->target)) != NULL) {
            if (!Q_stricmp(ent->classname, "info_tour_point") || !Q_stricmp(ent->classname, "info_notnull")) {
                continue;
            }
            if (ent->item && !(ent->s.eFlags & EF_NODRAW)) {
                continue;
            }
            if (ent->use) {
                ent->use(ent, point, &g_entities[bs->client]);
            }
        }
        if (message && message[0]) {
            trap_EA_Command(bs->client, va("botSay %i %f %s", human, (double)chatTime, G_TrainingMessage(point)));
            trap_SendServerCommand(human, va("playSound %s", point->noise));
            bs->tourNextSay = FloatTime() + 7.0;
        }
    }

    if (FloatTime() > bs->tourNextSay && distBotHuman > 320 && !ignore && FloatTime() > bs->tourLastNudge + 10 &&
        random() < bs->thinktime * 1.9) {
        BotTourLookAt(bs, hinfo.origin);
        trap_EA_Jump(bs->client);
        bs->tourLastNudge = FloatTime();
        trap_EA_Command(bs->client, G_TrainingSay("say This way! No time to waste!"));
        trap_SendServerCommand(human, "playSound sound/vo/crash/41a.wav");
        if (bs->tourGoBack) {
            bs->tourLostPlayer = 1;
            BotTourSaveGoal(bs);
        }
    }
    if (!VectorCompare(bs->tourLookTarget, vec3_origin)) {
        BotTourLookAt(bs, bs->tourLookTarget);
    } else if (flag2) {
        BotTourLookAt(bs, flag4 ? bs->tourGoal : pointOrigin);
    } else {
        BotTourLookAt(bs, hinfo.origin);
    }

    if (FloatTime() <= bs->tourReachTime + wait) {
        return qtrue;
    }
    if ((point->spawnflags & 64) &&
        !(hinfo.powerups & ((1 << PW_REDFLAG) | (1 << PW_BLUEFLAG) | (1 << PW_NEUTRALFLAG)))) {
        if (FloatTime() > bs->tourLastNudge + 10 && random() < bs->thinktime * 1.9) {
            trap_EA_Jump(bs->client);
            bs->tourLastNudge = FloatTime();
            trap_EA_Command(bs->client, G_TrainingSay("say Grab the flag! No time to waste!"));
        }
        return qtrue;
    }
    if (point->spawnflags & 256) {
        gentity_t* item = NULL;

        while ((item = G_Find(item, FOFS(targetname), point->target)) != NULL) {
            if (item->item && !(item->s.eFlags & EF_NODRAW)) {
                break;
            }
        }
        if (item) {
            VectorSubtract(hinfo.origin, bs->origin, d);
            vectoangles(d, bs->ideal_viewangles);
            bs->ideal_viewangles[2] *= 0.5f;
            VectorCopy(item->s.origin, bs->tourPrevGoal);
            if (FloatTime() > bs->tourLastNudge + 10 && random() < bs->thinktime * 1.1) {
                const char* line;

                bs->tourLastNudge = FloatTime();
                if (random() < bs->thinktime) {
                    trap_EA_Jump(bs->client);
                }
                switch (item->item->giType) {
                    case IT_WEAPON:
                        line = "say Grab this weapon! No time to waste!";
                        break;
                    case IT_AMMO:
                        line = "say Grab this ammo! No time to waste!";
                        break;
                    case IT_ARMOR:
                        line = "say Grab this armor! No time to waste!";
                        break;
                    case IT_HEALTH:
                        line = "say Grab this health! No time to waste!";
                        break;
                    case IT_POWERUP:
                        line = "say Grab this powerup! No time to waste!";
                        break;
                    case IT_HOLDABLE:
                        line = "say Grab this holdable! No time to waste!";
                        break;
                    default:
                        line = "say Grab this item! No time to waste!";
                        break;
                }
                trap_EA_Command(bs->client, (char*)line);
                trap_SendServerCommand(human, "playSound sound/vo/crash/42b.wav");
                return qtrue;
            }
            if (FloatTime() > bs->tourLookUntil + 10 && random() < bs->thinktime * 1.1 &&
                FloatTime() > bs->tourNextSay) {
                bs->tourLookUntil = FloatTime() + 1.0;
            }
            return qtrue;
        }
    } else {
        bs->tourLookUntil = FloatTime();
        bs->tourLastNudge = FloatTime();
    }

    if (FloatTime() < bs->teleport_time + 30 && !bs->tourSavedLookSet &&
        !VectorCompare(bs->tourLookTarget, vec3_origin)) {
        VectorCopy(bs->tourLookTarget, bs->tourSavedLook);
        bs->tourSavedLookSet = 1;
        bs->tourGoBack = 1;
    }
    bs->tourPrevPoint = bs->tourPoint;
    bs->tourPoint = BotTourGoal(bs->tourPoint, bs->tourLookTarget, bs->tourGoal);
    VectorCopy(bs->tourGoal, bs->tourPrevGoal);
    bs->tourStopReached = 0;
    bs->tourWait = (int)wait;
    if (cvar && cvar[0] && cvarValue != -1.0f) {
        trap_Cvar_Set(cvar, va("%f", cvarValue));
    }
    return qtrue;

movetogoal:
    bs->tourLastNudge = FloatTime();
    bs->tourLookUntil = FloatTime();
    bs->nodeEnterTime = (int)FloatTime();
    if (600 > distBotHuman && canSee) {
        trap_EA_Action(bs->client, ACTION_WALK);
    }
    if (bs->tourLostPlayer && 170 > distBotHuman) {
        trap_EA_Command(bs->client, G_TrainingSay("say Follow me to continue the tour! Lets go!"));
        trap_SendServerCommand(human, "playSound sound/vo/crash/39a.wav");
        bs->tourLostPlayer = 0;
        bs->tourNextTime = (int)(FloatTime() + 2);
        bs->tourReturning = 1;
    }
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        if (hinfo.groundent == ENTITYNUM_NONE) {
            BotTourLookAt(bs, hinfo.origin);
            return qtrue;
        }
        if (!VectorCompare(bs->tourSavedLook, vec3_origin)) {
            BotTourLookAt(bs, bs->tourSavedLook);
            return qtrue;
        }
    }
    BotTourMoveView(bs, &moveresult, &goal, 300);
    return qtrue;
}

void AIEnter_FollowMe(bot_state_t* bs) {
    bs->ltg_time = FloatTime() + 99999;
    bs->ltgtype = LTG_FOLLOWME;
    bs->ainode = AINode_FollowMe;
    bs->nodeEnterTime = 0;
    trap_EA_Gesture(bs->client);
    if (g_training.integer) {
        gentity_t* ent = &g_entities[bs->client];

        trap_EA_Command(bs->client, G_TrainingSay("say Exit the tutorial whenever you're ready to fight me!"));
        ent->flags |= FL_GODMODE | FL_NOPICKUP;
        ent->client->ps.ammo[WP_ROCKET_LAUNCHER] = 9999;
        ent->client->ps.stats[STAT_WEAPONS] |= 1 << WP_ROCKET_LAUNCHER;
        ent->flags |= FL_TRAINER | FL_NO_KNOCKBACK;
        ent->client->ps.stats[STAT_WEAPONS] |= 1 << 15;
        bs->weaponnum = WP_ROCKET_LAUNCHER;
    }
    BotRecordNodeSwitch(bs, "seek teammate", "", "");
}

int AINode_FollowMe(bot_state_t* bs) {
    int human = G_TrainingHumanClient();
    aas_entityinfo_t hinfo, binfo;
    bot_goal_t goal;
    bot_moveresult_t moveresult;
    vec3_t target, dir;
    float dist;

    if (BotIsObserver(bs)) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        BotRecordNodeSwitch(bs, "observer", "", "seek teammate: observer");
        BotResetState(bs);
        bs->ainode = AINode_Observer;
        bs->ltgtype = 0;
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_INTERMISSION) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        AIEnter_Intermission(bs, "seek teammate: intermission");
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_DEAD) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        AIEnter_Respawn(bs, "seek teammate: bot dead");
        return qfalse;
    }
    if (!bot_followMe.integer) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        AIEnter_Seek_LTG(bs, "seek teammate: no ai node");
        return qfalse;
    }
    if (human == -1) {
        BotTrainerRelease(bs);
        bs->ltgtype = 0;
        trap_Cvar_Set("bot_followMe", "0");
        AIEnter_Seek_LTG(bs, "seek teammate: no client");
        return qfalse;
    }
    if (g_training.integer) {
        BotSetFreezeState(bs, 1);
        bs->weaponnum = bot_gauntlet.integer ? 15 : WP_ROCKET_LAUNCHER;
    }

    bs->tfl = TFL_DEFAULT;
    trap_AAS_EntityInfo(human, &hinfo);
    trap_AAS_EntityInfo(bs->client, &binfo);
    trap_BotEmptyGoalStack(bs->gs);
    BotTourGoalOnEntity(&goal, &hinfo, human);
    VectorCopy(hinfo.origin, target);
    trap_BotPushGoal(bs->gs, &goal);
    if (BotInLavaOrSlime(bs)) {
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    }
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    bs->enemy = -1;
    if (BotAIPredictObstacles(bs, &goal)) {
        bs->ltgtype = 0;
        return qfalse;
    }

    VectorSubtract(hinfo.origin, bs->origin, dir);
    dist = VectorLength(dir);
    if (bot_followDist.value > dist && binfo.groundent == ENTITYNUM_WORLD) {
        playerState_t ps;
        int ducked;

        BotAI_GetClientState(human, &ps);
        ducked = ps.pm_flags & PMF_DUCKED;
        if ((ducked && random() * 100 < 3.0) || ((bs->cur_ps.pm_flags & PMF_DUCKED) && ducked)) {
            trap_EA_Crouch(bs->client);
        }
        if (BotTourTouching(bs->client, human)) {
            vec3_t forward;

            AngleVectors(hinfo.angles, forward, NULL, NULL);
            forward[2] = 0;
            VectorNormalize(forward);
            VectorSubtract(bs->origin, hinfo.origin, dir);
            VectorNormalize(dir);
            if (DotProduct(forward, dir) > 0.7) {
                BotSetupForMovement(bs);
                trap_BotMoveInDirection(bs->ms, dir, 400, MOVE_WALK);
                return qtrue;
            }
        }
        if (FloatTime() < (float)(bs->nodeEnterTime + 5)) {
            BotTourLookAt(bs, hinfo.origin);
        } else if (random() < bs->thinktime * 0.5) {
            BotRoamGoal(bs, target);
            BotTourLookAt(bs, target);
        }
        if (random() * 100 < 0.5) {
            trap_EA_Gesture(bs->client);
        }
        if (g_training.integer && random() * 100 < 1.5) {
            trap_EA_Command(bs->client, G_TrainingSay("say Exit the tutorial whenever you're ready to fight me!"));
            bs->tourNextSay = FloatTime() + 3.0;
        }
        return qtrue;
    }

    bs->nodeEnterTime = (int)FloatTime();
    BotSetupForMovement(bs);
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        trap_BotResetAvoidReach(bs->ms);
        if (hinfo.groundent == ENTITYNUM_NONE) {
            BotTourLookAt(bs, hinfo.origin);
            return qtrue;
        }
    }
    {
        float lookahead = bs->cur_ps.groundEntityNum == ENTITYNUM_NONE ? 2048 : 300;

        if (!(moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW |
                                   MOVERESULT_WAITING)) &&
            !(bs->flags & BFL_IDEALVIEWSET) &&
            BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 90, human) == 0) {
            lookahead = 300;
        }
        BotTourMoveView(bs, &moveresult, &goal, lookahead);
    }
    return qtrue;
}

#define DODGE_HEALTH 9999
#define DODGE_LEASH 300.0f

#define DODGE_STRAFE 0
#define DODGE_CIRCLE 1
#define DODGE_CLOSE 2
#define DODGE_BACK 3

void AIEnter_DodgeTrainer(bot_state_t* bs) {
    float now = FloatTime();

    bs->ltgtype = LTG_DODGETRAINER;
    bs->ainode = AINode_DodgeTrainer;
    bs->enemy = -1;
    VectorCopy(bs->origin, bs->dodgeAnchor);
    bs->dodgeDir = random() < 0.5f ? 1 : -1;
    bs->dodgeRunStart = now;
    bs->dodgeTurnTime = now + 0.3f;
    bs->dodgeHitTime = 0;
    bs->dodgeJumpTime = now + 3.0f + random() * 3.0f;
    bs->dodgeMode = DODGE_STRAFE;
    bs->dodgeModeEnd = now + 2.0f;
    bs->dodgeRadial = 0;
    bs->dodgeHeat = 0;
    bs->dodgeThink = now;
    bs->dodgeJumped = 0;
    BotRecordNodeSwitch(bs, "dodge trainer", "", "");
}

static void BotDodgeNextMode(bot_state_t* bs, float dist, float now) {
    float r = random();

    if (dist > 650) {
        bs->dodgeMode = DODGE_CLOSE;
    } else if (dist < 220) {
        bs->dodgeMode = DODGE_BACK;
    } else if (r < 0.45f) {
        bs->dodgeMode = DODGE_STRAFE;
    } else if (r < 0.70f) {
        bs->dodgeMode = DODGE_CIRCLE;
    } else if (r < 0.85f) {
        bs->dodgeMode = DODGE_CLOSE;
    } else {
        bs->dodgeMode = DODGE_BACK;
    }
    switch (bs->dodgeMode) {
        case DODGE_CIRCLE:
            bs->dodgeRadial = 0;
            bs->dodgeModeEnd = now + 1.0f + random();
            break;
        case DODGE_CLOSE:
            bs->dodgeRadial = 0.6f;
            bs->dodgeModeEnd = now + 0.6f + random() * 0.6f;
            break;
        case DODGE_BACK:
            bs->dodgeRadial = -0.6f;
            bs->dodgeModeEnd = now + 0.6f + random() * 0.6f;
            break;
        default:
            bs->dodgeRadial = crandom() * 0.15f;
            bs->dodgeModeEnd = now + 1.5f + random() * 2.0f;
            break;
    }
}

static float BotDodgeRunTime(bot_state_t* bs) {
    float heat = bs->dodgeHeat;

    if (bs->dodgeMode == DODGE_CIRCLE) {
        return 0.8f + random() * 0.8f;
    }
    if (random() < 0.25f + 0.5f * heat) {
        return 0.1f + random() * 0.18f;
    }
    if (random() < 0.5f) {
        return 0.3f + random() * 0.35f;
    }
    return 0.7f + random() * 0.7f * (1.0f - 0.6f * heat);
}

static void BotDodgeDirection(bot_state_t* bs, const vec3_t toHuman, vec3_t dir) {
    vec3_t home;
    float away;

    VectorSet(dir, -toHuman[1] * bs->dodgeDir, toHuman[0] * bs->dodgeDir, 0);
    VectorMA(dir, bs->dodgeRadial, toHuman, dir);
    VectorSubtract(bs->dodgeAnchor, bs->origin, home);
    home[2] = 0;
    away = VectorNormalize(home);
    if (away > DODGE_LEASH) {
        VectorMA(dir, MIN((away - DODGE_LEASH) / 60.0f, 2.0f), home, dir);
    }
    dir[2] = 0;
    VectorNormalize(dir);
}
static qboolean BotDodgeClear(bot_state_t* bs, const vec3_t dir) {
    gentity_t* ent = &g_entities[bs->client];
    vec3_t end, down;
    trace_t tr;
    float dx, dy, out;

    VectorMA(bs->origin, 48, dir, end);
    dx = end[0] - bs->dodgeAnchor[0];
    dy = end[1] - bs->dodgeAnchor[1];
    out = dx * dx + dy * dy;
    dx = bs->origin[0] - bs->dodgeAnchor[0];
    dy = bs->origin[1] - bs->dodgeAnchor[1];
    if (out > DODGE_LEASH * DODGE_LEASH && out > dx * dx + dy * dy) {
        return qfalse;
    }
    trap_Trace(&tr, bs->origin, ent->r.mins, ent->r.maxs, end, bs->client, MASK_PLAYERSOLID);
    if (tr.fraction < 1.0f) {
        return qfalse;
    }
    VectorCopy(end, down);
    down[2] -= 80;
    trap_Trace(&tr, end, ent->r.mins, ent->r.maxs, down, bs->client, MASK_PLAYERSOLID | CONTENTS_LAVA | CONTENTS_SLIME);
    return tr.fraction < 1.0f && !(tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)) &&
           fabs(tr.endpos[2] - bs->dodgeAnchor[2]) <= 24;
}

static void BotDodgeTurn(bot_state_t* bs, float now) {
    bs->dodgeDir = -bs->dodgeDir;
    bs->dodgeRunStart = now;
    bs->dodgeTurnTime = now + BotDodgeRunTime(bs);
    bs->dodgeHitTime = 0;
}

int AINode_DodgeTrainer(bot_state_t* bs) {
    gentity_t* ent = &g_entities[bs->client];
    int human = G_TrainingHumanClient();
    float now = FloatTime();
    float dist, dt;
    vec3_t toHuman, dir;
    qboolean hit;
    qboolean onGround = (qboolean)(bs->cur_ps.groundEntityNum != ENTITYNUM_NONE);

    if (BotIsObserver(bs)) {
        ent->flags &= ~FL_NOPICKUP;
        bs->ltgtype = 0;
        BotRecordNodeSwitch(bs, "observer", "", "dodge trainer: observer");
        BotResetState(bs);
        bs->ainode = AINode_Observer;
        bs->ltgtype = 0;
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_INTERMISSION) {
        ent->flags &= ~FL_NOPICKUP;
        bs->ltgtype = 0;
        AIEnter_Intermission(bs, "dodge trainer: intermission");
        return qfalse;
    }
    if (bs->cur_ps.pm_type == PM_DEAD) {
        ent->flags &= ~FL_NOPICKUP;
        bs->ltgtype = 0;
        AIEnter_Respawn(bs, "dodge trainer: bot dead");
        return qfalse;
    }
    if (g_advancedTraining.integer != 1) {
        ent->flags &= ~FL_NOPICKUP;
        bs->ltgtype = 0;
        AIEnter_Seek_LTG(bs, "dodge trainer: no ai node");
        return qfalse;
    }

    hit = 2 * DODGE_HEALTH - ent->health - ent->client->ps.stats[STAT_ARMOR] > 3;
    ent->health = ent->client->ps.stats[STAT_HEALTH] = DODGE_HEALTH;
    ent->client->ps.stats[STAT_ARMOR] = DODGE_HEALTH;
    ent->client->ps.stats[STAT_WEAPONS] = 1 << WP_GAUNTLET;
    ent->flags |= FL_NOPICKUP;
    bs->weaponnum = WP_GAUNTLET;
    trap_EA_SelectWeapon(bs->client, WP_GAUNTLET);
    bs->enemy = -1;

    if (human == -1) {
        return qtrue;
    }

    VectorSubtract(g_entities[human].r.currentOrigin, bs->origin, toHuman);
    vectoangles(toHuman, bs->ideal_viewangles);
    toHuman[2] = 0;
    dist = VectorNormalize(toHuman);

    dt = now - bs->dodgeThink;
    if (dt < 0 || dt > 0.25f) {
        dt = 0.05f;
    }
    bs->dodgeThink = now;
    bs->dodgeHeat -= bs->dodgeHeat * MIN(dt * 0.6f, 1.0f);
    if (hit) {
        bs->dodgeHeat = MIN(1.0f, bs->dodgeHeat + 0.05f);
    }

    if (now >= bs->dodgeModeEnd || (dist > 650 && bs->dodgeMode != DODGE_CLOSE) ||
        (dist < 220 && bs->dodgeMode != DODGE_BACK)) {
        BotDodgeNextMode(bs, dist, now);
    }

    if (hit && !bs->dodgeHitTime) {
        bs->dodgeHitTime = now + 0.08f + random() * (0.17f - 0.08f * bs->dodgeHeat);
        if (bs->dodgeMode == DODGE_CIRCLE && random() < bs->dodgeHeat) {
            bs->dodgeMode = DODGE_STRAFE;
            bs->dodgeRadial = crandom() * 0.15f;
            bs->dodgeModeEnd = now + 1.0f + random() * 1.5f;
        }
    }
    if (now >= bs->dodgeTurnTime ||
        (bs->dodgeHitTime && now >= bs->dodgeHitTime && now - bs->dodgeRunStart > 0.12f)) {
        BotDodgeTurn(bs, now);
        if (onGround && bs->dodgeHeat > 0.5f && random() < 0.3f * bs->dodgeHeat && now - bs->dodgeJumped > 1.0f) {
            trap_EA_Jump(bs->client);
            bs->dodgeJumped = now;
        }
    }

    BotDodgeDirection(bs, toHuman, dir);
    if (!BotDodgeClear(bs, dir)) {
        if (bs->dodgeMode == DODGE_CLOSE || bs->dodgeMode == DODGE_BACK) {
            bs->dodgeMode = DODGE_STRAFE;
            bs->dodgeRadial = 0;
            bs->dodgeModeEnd = now + 1.0f + random();
        }
        BotDodgeTurn(bs, now);
        BotDodgeDirection(bs, toHuman, dir);
        if (!BotDodgeClear(bs, dir)) {
            VectorSubtract(bs->dodgeAnchor, bs->origin, dir);
            dir[2] = 0;
            VectorNormalize(dir);
        }
    }
    trap_EA_Move(bs->client, dir, 400);

    if (now >= bs->dodgeJumpTime) {
        if (onGround && now - bs->dodgeJumped > 1.0f) {
            trap_EA_Jump(bs->client);
            bs->dodgeJumped = now;
        }
        bs->dodgeJumpTime = now + 5.0f - 3.5f * bs->dodgeHeat + random() * (4.0f - 2.0f * bs->dodgeHeat);
    }
    return qtrue;
}
