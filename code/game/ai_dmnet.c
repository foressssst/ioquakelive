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

/*****************************************************************************
 * name:		ai_dmnet.c
 *
 * desc:		Quake3 bot AI
 *
 *****************************************************************************/

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
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_team.h"
// data file headers
#include "chars.h"  //characteristics
#include "inv.h"    //indexes into the inventory
#include "syn.h"    //synonyms
#include "match.h"  //string matching types and vars

// for the voice chats
#include "../../ui/menudef.h"

// goal flag, see ../botlib/be_ai_goal.h for the other GFL_*
#define GFL_AIR 128

int numnodeswitches;
char nodeswitch[MAX_NODESWITCHES + 1][144];

#define LOOKAHEAD_DISTANCE 300

/*
==================
BotResetNodeSwitches
==================
*/
void BotResetNodeSwitches(void) {
    numnodeswitches = 0;
}

/*
==================
BotDumpNodeSwitches
==================
*/
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

/*
==================
BotRecordNodeSwitch
==================
*/
void BotRecordNodeSwitch(bot_state_t* bs, char* node, char* str, char* s) {
    char netname[MAX_NETNAME];

    ClientName(bs->client, netname, sizeof(netname));
    Com_sprintf(nodeswitch[numnodeswitches], 144, "%s at %2.1f entered %s: %s from %s\n", netname, FloatTime(), node, str, s);
#ifdef DEBUG
    if (0) {
        BotAI_Print(PRT_MESSAGE, "%s", nodeswitch[numnodeswitches]);
    }
#endif  // DEBUG
    numnodeswitches++;
}

/*
==================
BotGetAirGoal
==================
*/
int BotGetAirGoal(bot_state_t* bs, bot_goal_t* goal) {
    bsp_trace_t bsptrace;
    vec3_t end, mins = {-15, -15, -2}, maxs = {15, 15, 2};
    int areanum;

    // trace up until we hit solid
    VectorCopy(bs->origin, end);
    end[2] += 1000;
    BotAI_Trace(&bsptrace, bs->origin, mins, maxs, end, bs->entitynum, CONTENTS_SOLID | CONTENTS_PLAYERCLIP);
    // trace down until we hit water
    VectorCopy(bsptrace.endpos, end);
    BotAI_Trace(&bsptrace, end, mins, maxs, bs->origin, bs->entitynum, CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
    // if we found the water surface
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

/*
==================
BotGoForAir
==================
*/
int BotGoForAir(bot_state_t* bs, int tfl, bot_goal_t* ltg, float range) {
    bot_goal_t goal;

    // if the bot needs air
    if (bs->lastair_time < FloatTime() - 6) {
        //
#ifdef DEBUG
        // BotAI_Print(PRT_MESSAGE, "going for air\n");
#endif  // DEBUG
        // if we can find an air goal
        if (BotGetAirGoal(bs, &goal)) {
            trap_BotPushGoal(bs->gs, &goal);
            return qtrue;
        } else {
            // get a nearby goal outside the water
            while (trap_BotChooseNBGItem(bs->gs, bs->origin, bs->inventory, tfl, ltg, range)) {
                trap_BotGetTopGoal(bs->gs, &goal);
                // if the goal is not in water
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

/*
==================
BotNearbyGoal
==================
*/
int BotNearbyGoal(bot_state_t* bs, int tfl, bot_goal_t* ltg, float range) {
    int ret;

    // check if the bot should go for air
    if (BotGoForAir(bs, tfl, ltg, range))
        return qtrue;
    // if the bot is carrying a flag or cubes
    if (BotCTFCarryingFlag(bs) || Bot1FCTFCarryingFlag(bs) || BotHarvesterCarryingCubes(bs)) {
        // if the bot is just a few secs away from the base
        if (trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin,
                                              bs->teamgoal.areanum, TFL_DEFAULT) < 300) {
            // make the range really small
            range = 50;
        }
    }
    //
    ret = trap_BotChooseNBGItem(bs->gs, bs->origin, bs->inventory, tfl, ltg, range);
    /*
    if (ret)
    {
        char buf[128];
        //get the goal at the top of the stack
        trap_BotGetTopGoal(bs->gs, &goal);
        trap_BotGoalName(goal.number, buf, sizeof(buf));
        BotAI_Print(PRT_MESSAGE, "%1.1f: new nearby goal %s\n", FloatTime(), buf);
    }
    */
    return ret;
}

/*
==================
BotReachedGoal
==================
*/
int BotReachedGoal(bot_state_t* bs, bot_goal_t* goal) {
    if (goal->flags & GFL_ITEM) {
        // if touching the goal
        if (trap_BotTouchingGoal(bs->origin, goal)) {
            if (!(goal->flags & GFL_DROPPED)) {
                trap_BotSetAvoidGoalTime(bs->gs, goal->number, -1);
            }
            return qtrue;
        }
        // if the goal isn't there
        if (trap_BotItemGoalInVisButNotVisible(bs->entitynum, bs->eye, bs->viewangles, goal)) {
            /*
            float avoidtime;
            int t;

            avoidtime = trap_BotAvoidGoalTime(bs->gs, goal->number);
            if (avoidtime > 0) {
                t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, goal->areanum, bs->tfl);
                if ((float) t * 0.009 < avoidtime)
                    return qtrue;
            }
            */
            return qtrue;
        }
        // if in the goal area and below or above the goal and not swimming
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
        // if touching the goal
        if (trap_BotTouchingGoal(bs->origin, goal))
            return qtrue;
        // if the bot got air
        if (bs->lastair_time > FloatTime() - 1)
            return qtrue;
    } else {
        // if touching the goal
        if (trap_BotTouchingGoal(bs->origin, goal))
            return qtrue;
    }
    return qfalse;
}

/*
==================
BotGetItemLongTermGoal
==================
*/
int BotGetItemLongTermGoal(bot_state_t* bs, int tfl, bot_goal_t* goal) {
    // if the bot has no goal
    if (!trap_BotGetTopGoal(bs->gs, goal)) {
        // BotAI_Print(PRT_MESSAGE, "no ltg on stack\n");
        bs->ltg_time = 0;
    }
    // if the bot touches the current goal
    else if (BotReachedGoal(bs, goal)) {
        BotChooseWeapon(bs);
        bs->ltg_time = 0;
    }
    // if it is time to find a new long term goal
    if (bs->ltg_time < FloatTime()) {
        // pop the current goal from the stack
        trap_BotPopGoal(bs->gs);
        // BotAI_Print(PRT_MESSAGE, "%s: choosing new ltg\n", ClientName(bs->client, netname, sizeof(netname)));
        // choose a new goal
        // BotAI_Print(PRT_MESSAGE, "%6.1f client %d: BotChooseLTGItem\n", FloatTime(), bs->client);
        if (trap_BotChooseLTGItem(bs->gs, bs->origin, bs->inventory, tfl)) {
            /*
            char buf[128];
            //get the goal at the top of the stack
            trap_BotGetTopGoal(bs->gs, goal);
            trap_BotGoalName(goal->number, buf, sizeof(buf));
            BotAI_Print(PRT_MESSAGE, "%1.1f: new long term goal %s\n", FloatTime(), buf);
            */
            bs->ltg_time = FloatTime() + 20;
        } else {  // the bot gets sorta stuck with all the avoid timings, shouldn't happen though
            //
#ifdef DEBUG
            char netname[128];

            BotAI_Print(PRT_MESSAGE, "%s: no valid ltg (probably stuck)\n", ClientName(bs->client, netname, sizeof(netname)));
#endif
            // trap_BotDumpAvoidGoals(bs->gs);
            // reset the avoid goals and the avoid reach
            trap_BotResetAvoidGoals(bs->gs);
            trap_BotResetAvoidReach(bs->ms);
        }
        // get the goal at the top of the stack
        return trap_BotGetTopGoal(bs->gs, goal);
    }
    return qtrue;
}

/*
==================
BotGetLongTermGoal

we could also create a separate AI node for every long term goal type
however this saves us a lot of code
==================
*/
int BotGetLongTermGoal(bot_state_t* bs, int tfl, int retreat, bot_goal_t* goal) {
    vec3_t target, dir, dir2;
    char netname[MAX_NETNAME];
    char buf[MAX_MESSAGE_SIZE];
    int areanum;
    float croucher;
    aas_entityinfo_t entinfo, botinfo;
    bot_waypoint_t* wp;

    if (bs->ltgtype == LTG_TEAMHELP && !retreat) {
        // check for bot typing status message
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "help_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        // if trying to help the team mate for more than a minute
        if (bs->teamgoal_time < FloatTime())
            bs->ltgtype = 0;
        // if the team mate IS visible for quite some time
        if (bs->teammatevisible_time < FloatTime() - 10)
            bs->ltgtype = 0;
        // get entity information of the companion
        BotEntityInfo(bs->teammate, &entinfo);
        // if the team mate is visible
        if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->teammate)) {
            // if close just stand still there
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(100)) {
                trap_BotResetAvoidReach(bs->ms);
                return qfalse;
            }
        } else {
            // last time the bot was NOT visible
            bs->teammatevisible_time = FloatTime();
        }
        // if the entity information is valid (entity in PVS)
        if (entinfo.valid) {
            areanum = BotPointAreaNum(entinfo.origin);
            if (areanum && trap_AAS_AreaReachability(areanum)) {
                // update team goal
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
    // if the bot accompanies someone
    if (bs->ltgtype == LTG_TEAMACCOMPANY && !retreat) {
        // check for bot typing status message
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "accompany_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        // if accompanying the companion for 3 minutes
        if (bs->teamgoal_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "accompany_stop", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->ltgtype = 0;
        }
        // get entity information of the companion
        BotEntityInfo(bs->teammate, &entinfo);
        // if the companion is visible
        if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->teammate)) {
            // update visible time
            bs->teammatevisible_time = FloatTime();
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(bs->formation_dist)) {
                //
                // if the client being followed bumps into this bot then
                // the bot should back up
                BotEntityInfo(bs->entitynum, &botinfo);
                // if the followed client is not standing ontop of the bot
                if (botinfo.origin[2] + botinfo.maxs[2] > entinfo.origin[2] + entinfo.mins[2]) {
                    // if the bounding boxes touch each other
                    if (botinfo.origin[0] + botinfo.maxs[0] > entinfo.origin[0] + entinfo.mins[0] - 4 &&
                        botinfo.origin[0] + botinfo.mins[0] < entinfo.origin[0] + entinfo.maxs[0] + 4) {
                        if (botinfo.origin[1] + botinfo.maxs[1] > entinfo.origin[1] + entinfo.mins[1] - 4 &&
                            botinfo.origin[1] + botinfo.mins[1] < entinfo.origin[1] + entinfo.maxs[1] + 4) {
                            if (botinfo.origin[2] + botinfo.maxs[2] > entinfo.origin[2] + entinfo.mins[2] - 4 &&
                                botinfo.origin[2] + botinfo.mins[2] < entinfo.origin[2] + entinfo.maxs[2] + 4) {
                                // if the followed client looks in the direction of this bot
                                AngleVectors(entinfo.angles, dir, NULL, NULL);
                                dir[2] = 0;
                                VectorNormalize(dir);
                                // VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
                                VectorSubtract(bs->origin, entinfo.origin, dir2);
                                VectorNormalize(dir2);
                                if (DotProduct(dir, dir2) > 0.7) {
                                    // back up
                                    BotSetupForMovement(bs);
                                    trap_BotMoveInDirection(bs->ms, dir2, 400, MOVE_WALK);
                                }
                            }
                        }
                    }
                }
                // check if the bot wants to crouch
                // don't crouch if crouched less than 5 seconds ago
                if (bs->attackcrouch_time < FloatTime() - 5) {
                    croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
                    if (random() < bs->thinktime * croucher) {
                        bs->attackcrouch_time = FloatTime() + 5 + croucher * 15;
                    }
                }
                // don't crouch when swimming
                if (trap_AAS_Swimming(bs->origin))
                    bs->attackcrouch_time = FloatTime() - 1;
                // if not arrived yet or arived some time ago
                if (bs->arrive_time < FloatTime() - 2) {
                    // if not arrived yet
                    if (!bs->arrive_time) {
                        trap_EA_Gesture(bs->client);
                        BotAI_BotInitialChat(bs, "accompany_arrive", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
                        trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
                        bs->arrive_time = FloatTime();
                    }
                    // if the bot wants to crouch
                    else if (bs->attackcrouch_time > FloatTime()) {
                        trap_EA_Crouch(bs->client);
                    }
                    // else do some model taunts
                    else if (random() < bs->thinktime * 0.05) {
                        // do a gesture :)
                        trap_EA_Gesture(bs->client);
                    }
                }
                // if just arrived look at the companion
                if (bs->arrive_time > FloatTime() - 2) {
                    VectorSubtract(entinfo.origin, bs->origin, dir);
                    vectoangles(dir, bs->ideal_viewangles);
                    bs->ideal_viewangles[2] *= 0.5;
                }
                // else look strategically around for enemies
                else if (random() < bs->thinktime * 0.8) {
                    BotRoamGoal(bs, target);
                    VectorSubtract(target, bs->origin, dir);
                    vectoangles(dir, bs->ideal_viewangles);
                    bs->ideal_viewangles[2] *= 0.5;
                }
                // check if the bot wants to go for air
                if (BotGoForAir(bs, bs->tfl, &bs->teamgoal, 400)) {
                    trap_BotResetLastAvoidReach(bs->ms);
                    // get the goal at the top of the stack
                    // trap_BotGetTopGoal(bs->gs, &tmpgoal);
                    // trap_BotGoalName(tmpgoal.number, buf, 144);
                    // BotAI_Print(PRT_MESSAGE, "new nearby goal %s\n", buf);
                    // time the bot gets to pick up the nearby goal item
                    bs->nbg_time = FloatTime() + 8;
                    AIEnter_Seek_NBG(bs, "BotLongTermGoal: go for air");
                    return qfalse;
                }
                //
                trap_BotResetAvoidReach(bs->ms);
                return qfalse;
            }
        }
        // if the entity information is valid (entity in PVS)
        if (entinfo.valid) {
            areanum = BotPointAreaNum(entinfo.origin);
            if (areanum && trap_AAS_AreaReachability(areanum)) {
                // update team goal
                bs->teamgoal.entitynum = bs->teammate;
                bs->teamgoal.areanum = areanum;
                VectorCopy(entinfo.origin, bs->teamgoal.origin);
                VectorSet(bs->teamgoal.mins, -8, -8, -8);
                VectorSet(bs->teamgoal.maxs, 8, 8, 8);
            }
        }
        // the goal the bot should go for
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        // if the companion is NOT visible for too long
        if (bs->teammatevisible_time < FloatTime() - 60) {
            BotAI_BotInitialChat(bs, "accompany_cannotfind", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->ltgtype = 0;
            // just to make sure the bot won't spam this message
            bs->teammatevisible_time = FloatTime();
        }
        return qtrue;
    }
    //
    if (bs->ltgtype == LTG_DEFENDKEYAREA) {
        if (trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin,
                                              bs->teamgoal.areanum, TFL_DEFAULT) > bs->defendaway_range) {
            bs->defendaway_time = 0;
        }
    }
    // if defending a key area
    if (bs->ltgtype == LTG_DEFENDKEYAREA && !retreat &&
        bs->defendaway_time < FloatTime()) {
        // check for bot typing status message
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "defend_start", buf, NULL);
            trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
            BotVoiceChatOnly(bs, -1, VOICECHAT_ONDEFENSE);
            bs->teammessage_time = 0;
        }
        // set the bot goal
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        // stop after 2 minutes
        if (bs->teamgoal_time < FloatTime()) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "defend_stop", buf, NULL);
            trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
            bs->ltgtype = 0;
        }
        // if very close... go away for some time
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
    // going to kill someone
    if (bs->ltgtype == LTG_KILL && !retreat) {
        // check for bot typing status message
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            EasyClientName(bs->teamgoal.entitynum, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "kill_start", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->teammessage_time = 0;
        }
        //
        if (bs->killedenemy_time > bs->teamgoal_time - TEAM_KILL_SOMEONE && bs->lastkilledplayer == bs->teamgoal.entitynum) {
            EasyClientName(bs->teamgoal.entitynum, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "kill_done", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            bs->ltgtype = 0;
        }
        //
        if (bs->teamgoal_time < FloatTime()) {
            bs->ltgtype = 0;
        }
        // just roam around
        return BotGetItemLongTermGoal(bs, tfl, goal);
    }
    // get an item
    if (bs->ltgtype == LTG_GETITEM && !retreat) {
        // check for bot typing status message
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
            BotAI_BotInitialChat(bs, "getitem_start", buf, NULL);
            trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
            trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            bs->teammessage_time = 0;
        }
        // set the bot goal
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        // stop after some time
        if (bs->teamgoal_time < FloatTime()) {
            bs->ltgtype = 0;
        }
        //
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
    // if camping somewhere
    if ((bs->ltgtype == LTG_CAMP || bs->ltgtype == LTG_CAMPORDER) && !retreat) {
        // check for bot typing status message
        if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
            if (bs->ltgtype == LTG_CAMPORDER) {
                BotAI_BotInitialChat(bs, "camp_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
                trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
                BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
                trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
            }
            bs->teammessage_time = 0;
        }
        // set the bot goal
        memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
        //
        if (bs->teamgoal_time < FloatTime()) {
            if (bs->ltgtype == LTG_CAMPORDER) {
                BotAI_BotInitialChat(bs, "camp_stop", NULL);
                trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
            }
            bs->ltgtype = 0;
        }
        // if really near the camp spot
        VectorSubtract(goal->origin, bs->origin, dir);
        if (VectorLengthSquared(dir) < Square(60)) {
            // if not arrived yet
            if (!bs->arrive_time) {
                if (bs->ltgtype == LTG_CAMPORDER) {
                    BotAI_BotInitialChat(bs, "camp_arrive", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
                    trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
                    BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_INPOSITION);
                }
                bs->arrive_time = FloatTime();
            }
            // look strategically around for enemies
            if (random() < bs->thinktime * 0.8) {
                BotRoamGoal(bs, target);
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
                bs->ideal_viewangles[2] *= 0.5;
            }
            // check if the bot wants to crouch
            // don't crouch if crouched less than 5 seconds ago
            if (bs->attackcrouch_time < FloatTime() - 5) {
                croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
                if (random() < bs->thinktime * croucher) {
                    bs->attackcrouch_time = FloatTime() + 5 + croucher * 15;
                }
            }
            // if the bot wants to crouch
            if (bs->attackcrouch_time > FloatTime()) {
                trap_EA_Crouch(bs->client);
            }
            // don't crouch when swimming
            if (trap_AAS_Swimming(bs->origin))
                bs->attackcrouch_time = FloatTime() - 1;
            // make sure the bot is not gonna drown
            if (trap_PointContents(bs->eye, bs->entitynum) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
                if (bs->ltgtype == LTG_CAMPORDER) {
                    BotAI_BotInitialChat(bs, "camp_stop", NULL);
                    trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
                    //
                    if (bs->lastgoal_ltgtype == LTG_CAMPORDER) {
                        bs->lastgoal_ltgtype = 0;
                    }
                }
                bs->ltgtype = 0;
            }
            //
            // FIXME: move around a bit
            //
            trap_BotResetAvoidReach(bs->ms);
            return qfalse;
        }
        return qtrue;
    }
    // patrolling along several waypoints
    if (bs->ltgtype == LTG_PATROL && !retreat) {
        // check for bot typing status message
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
        //
        if (!bs->curpatrolpoint) {
            bs->ltgtype = 0;
            return qfalse;
        }
        // if the bot touches the current goal
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
        // stop after 5 minutes
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
    if (gametype == GT_CTF) {
        // if going for enemy flag
        if (bs->ltgtype == LTG_GETFLAG) {
            // check for bot typing status message
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "captureflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONGETFLAG);
                bs->teammessage_time = 0;
            }
            //
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
            // if touching the flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                // make sure the bot knows the flag isn't there anymore
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
            // stop after 3 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        // if rushing to the base
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
            // if not carrying the flag anymore
            if (!BotCTFCarryingFlag(bs))
                bs->ltgtype = 0;
            // quit rushing after 2 minutes
            if (bs->teamgoal_time < FloatTime())
                bs->ltgtype = 0;
            // if touching the base flag the bot should loose the enemy flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                // if the bot is still carrying the enemy flag then the
                // base flag is gone, now just walk near the base a bit
                if (BotCTFCarryingFlag(bs)) {
                    trap_BotResetAvoidReach(bs->ms);
                    bs->rushbaseaway_time = FloatTime() + 5 + 10 * random();
                    // FIXME: add chat to tell the others to get back the flag
                } else {
                    bs->ltgtype = 0;
                }
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        // returning flag
        if (bs->ltgtype == LTG_RETURNFLAG) {
            // check for bot typing status message
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "returnflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONRETURNFLAG);
                bs->teammessage_time = 0;
            }
            //
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
            // if touching the flag
            if (trap_BotTouchingGoal(bs->origin, goal))
                bs->ltgtype = 0;
            // stop after 3 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
    }
#endif  // CTF
    else if (gametype == GT_1FCTF) {
        if (bs->ltgtype == LTG_GETFLAG) {
            // check for bot typing status message
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "captureflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONGETFLAG);
                bs->teammessage_time = 0;
            }
            memcpy(goal, &ctf_neutralflag, sizeof(bot_goal_t));
            // if touching the flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->ltgtype = 0;
            }
            // stop after 3 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            return qtrue;
        }
        // if rushing to the base
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
            // if not carrying the flag anymore
            if (!Bot1FCTFCarryingFlag(bs)) {
                bs->ltgtype = 0;
            }
            // quit rushing after 2 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            // if touching the base flag the bot should loose the enemy flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        // attack the enemy base
        if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
            bs->attackaway_time < FloatTime()) {
            // check for bot typing status message
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
            // quit rushing after 2 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            // if touching the base flag the bot should loose the enemy flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->attackaway_time = FloatTime() + 2 + 5 * random();
            }
            return qtrue;
        }
        // returning flag
        if (bs->ltgtype == LTG_RETURNFLAG) {
            // check for bot typing status message
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "returnflag_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONRETURNFLAG);
                bs->teammessage_time = 0;
            }
            //
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            // just roam around
            return BotGetItemLongTermGoal(bs, tfl, goal);
        }
    } else if (gametype == GT_OBELISK) {
        if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
            bs->attackaway_time < FloatTime()) {
            // check for bot typing status message
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
            // if the bot no longer wants to attack the obelisk
            if (BotFeelingBad(bs) > 50) {
                return BotGetItemLongTermGoal(bs, tfl, goal);
            }
            // if touching the obelisk
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->attackaway_time = FloatTime() + 3 + 5 * random();
            }
            // or very close to the obelisk
            VectorSubtract(bs->origin, goal->origin, dir);
            if (VectorLengthSquared(dir) < Square(60)) {
                bs->attackaway_time = FloatTime() + 3 + 5 * random();
            }
            // quit rushing after 2 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            BotAlternateRoute(bs, goal);
            // just move towards the obelisk
            return qtrue;
        }
    } else if (gametype == GT_HARVESTER) {
        // if rushing to the base
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
            // if not carrying any cubes
            if (!BotHarvesterCarryingCubes(bs)) {
                BotGoHarvest(bs);
                return qfalse;
            }
            // quit rushing after 2 minutes
            if (bs->teamgoal_time < FloatTime()) {
                BotGoHarvest(bs);
                return qfalse;
            }
            // if touching the base flag the bot should loose the enemy flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                BotGoHarvest(bs);
                return qfalse;
            }
            BotAlternateRoute(bs, goal);
            return qtrue;
        }
        // attack the enemy base
        if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
            bs->attackaway_time < FloatTime()) {
            // check for bot typing status message
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
            // quit rushing after 2 minutes
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            // if touching the base flag the bot should loose the enemy flag
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->attackaway_time = FloatTime() + 2 + 5 * random();
            }
            return qtrue;
        }
        // harvest cubes
        if (bs->ltgtype == LTG_HARVEST &&
            bs->harvestaway_time < FloatTime()) {
            // check for bot typing status message
            if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
                BotAI_BotInitialChat(bs, "harvest_start", NULL);
                trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
                BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
                bs->teammessage_time = 0;
            }
            memcpy(goal, &neutralobelisk, sizeof(bot_goal_t));
            //
            if (bs->teamgoal_time < FloatTime()) {
                bs->ltgtype = 0;
            }
            //
            if (trap_BotTouchingGoal(bs->origin, goal)) {
                bs->harvestaway_time = FloatTime() + 4 + 3 * random();
            }
            return qtrue;
        }
    }
    // normal goal stuff
    return BotGetItemLongTermGoal(bs, tfl, goal);
}

/*
==================
BotLongTermGoal
==================
*/
int BotLongTermGoal(bot_state_t* bs, int tfl, int retreat, bot_goal_t* goal) {
    aas_entityinfo_t entinfo;
    char teammate[MAX_MESSAGE_SIZE];
    float squaredist;
    int areanum;
    vec3_t dir;

    // FIXME: also have air long term goals?
    //
    // if the bot is leading someone and not retreating
    if (bs->lead_time > 0 && !retreat) {
        if (bs->lead_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "lead_stop", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->lead_time = 0;
            return BotGetLongTermGoal(bs, tfl, retreat, goal);
        }
        //
        if (bs->leadmessage_time < 0 && -bs->leadmessage_time < FloatTime()) {
            BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
            bs->leadmessage_time = FloatTime();
        }
        // get entity information of the companion
        BotEntityInfo(bs->lead_teammate, &entinfo);
        //
        if (entinfo.valid) {
            areanum = BotPointAreaNum(entinfo.origin);
            if (areanum && trap_AAS_AreaReachability(areanum)) {
                // update team goal
                bs->lead_teamgoal.entitynum = bs->lead_teammate;
                bs->lead_teamgoal.areanum = areanum;
                VectorCopy(entinfo.origin, bs->lead_teamgoal.origin);
                VectorSet(bs->lead_teamgoal.mins, -8, -8, -8);
                VectorSet(bs->lead_teamgoal.maxs, 8, 8, 8);
            }
        }
        // if the team mate is visible
        if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->lead_teammate)) {
            bs->leadvisible_time = FloatTime();
        }
        // if the team mate is not visible for 1 seconds
        if (bs->leadvisible_time < FloatTime() - 1) {
            bs->leadbackup_time = FloatTime() + 2;
        }
        // distance towards the team mate
        VectorSubtract(bs->origin, bs->lead_teamgoal.origin, dir);
        squaredist = VectorLengthSquared(dir);
        // if backing up towards the team mate
        if (bs->leadbackup_time > FloatTime()) {
            if (bs->leadmessage_time < FloatTime() - 20) {
                BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
                trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
                bs->leadmessage_time = FloatTime();
            }
            // if very close to the team mate
            if (squaredist < Square(100)) {
                bs->leadbackup_time = 0;
            }
            // the bot should go back to the team mate
            memcpy(goal, &bs->lead_teamgoal, sizeof(bot_goal_t));
            return qtrue;
        } else {
            // if quite distant from the team mate
            if (squaredist > Square(500)) {
                if (bs->leadmessage_time < FloatTime() - 20) {
                    BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
                    trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
                    bs->leadmessage_time = FloatTime();
                }
                // look at the team mate
                VectorSubtract(entinfo.origin, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
                bs->ideal_viewangles[2] *= 0.5;
                // just wait for the team mate
                return qfalse;
            }
        }
    }
    return BotGetLongTermGoal(bs, tfl, retreat, goal);
}

/*
==================
AIEnter_Intermission
==================
*/
void AIEnter_Intermission(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "intermission", "", s);
    // reset the bot state
    BotResetState(bs);
    // check for end level chat
    if (BotChat_EndLevel(bs)) {
        trap_BotEnterChat(bs->cs, 0, bs->chatto);
    }
    bs->ainode = AINode_Intermission;
}

/*
==================
AINode_Intermission
==================
*/
int AINode_Intermission(bot_state_t* bs) {
    // if the intermission ended
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

/*
==================
AIEnter_Observer
==================
*/
void AIEnter_Observer(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "observer", "", s);
    // reset the bot state
    BotResetState(bs);
    bs->ainode = AINode_Observer;
}

/*
==================
AINode_Observer
==================
*/
int AINode_Observer(bot_state_t* bs) {
    // if the bot left observer mode
    if (!BotIsObserver(bs)) {
        AIEnter_Stand(bs, "observer: left observer");
    }
    return qtrue;
}

/*
==================
AIEnter_Stand
==================
*/
void AIEnter_Stand(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "stand", "", s);
    bs->standfindenemy_time = FloatTime() + 1;
    bs->ainode = AINode_Stand;
}

/*
==================
AINode_Stand
==================
*/
int AINode_Stand(bot_state_t* bs) {
    // if the bot's health decreased
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
    // put up chat icon
    trap_EA_Talk(bs->client);
    // when done standing
    if (bs->stand_time < FloatTime()) {
        trap_BotEnterChat(bs->cs, 0, bs->chatto);
        AIEnter_Seek_LTG(bs, "stand: time out");
        return qfalse;
    }
    //
    return qtrue;
}

/*
==================
AIEnter_Respawn
==================
*/
void AIEnter_Respawn(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "respawn", "", s);
    // reset some states
    trap_BotResetMoveState(bs->ms);
    trap_BotResetGoalState(bs->gs);
    trap_BotResetAvoidGoals(bs->gs);
    trap_BotResetAvoidReach(bs->ms);
    // if the bot wants to chat
    if (BotChat_Death(bs)) {
        bs->respawn_time = FloatTime() + BotChatTime(bs);
        bs->respawnchat_time = FloatTime();
    } else {
        bs->respawn_time = FloatTime() + 1 + random();
        bs->respawnchat_time = 0;
    }
    // set respawn state
    bs->respawn_wait = qfalse;
    bs->ainode = AINode_Respawn;
}

/*
==================
AINode_Respawn
==================
*/
int AINode_Respawn(bot_state_t* bs) {
    // if waiting for the actual respawn
    if (bs->respawn_wait) {
        if (!BotIsDead(bs)) {
            AIEnter_Seek_LTG(bs, "respawn: respawned");
        } else {
            trap_EA_Respawn(bs->client);
        }
    } else if (bs->respawn_time < FloatTime()) {
        // wait until respawned
        bs->respawn_wait = qtrue;
        // elementary action respawn
        trap_EA_Respawn(bs->client);
        //
        if (bs->respawnchat_time) {
            trap_BotEnterChat(bs->cs, 0, bs->chatto);
            bs->enemy = -1;
        }
    }
    if (bs->respawnchat_time && bs->respawnchat_time < FloatTime() - 0.5) {
        trap_EA_Talk(bs->client);
    }
    //
    return qtrue;
}

/*
==================
BotSelectActivateWeapon
==================
*/
int BotSelectActivateWeapon(bot_state_t* bs) {
    //
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

/*
==================
BotClearPath

 try to deactivate obstacles like proximity mines on the bot's path
==================
*/
void BotClearPath(bot_state_t* bs, bot_moveresult_t* moveresult) {
    int i, bestmine;
    float dist, bestdist;
    vec3_t target, dir;
    bsp_trace_t bsptrace;
    entityState_t state;

    // if there is a dead body wearing kamikze nearby
    if (bs->kamikazebody) {
        // if the bot's view angles and weapon are not used for movement
        if (!(moveresult->flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_MOVEMENTWEAPON))) {
            //
            BotAI_GetEntityState(bs->kamikazebody, &state);
            VectorCopy(state.pos.trBase, target);
            target[2] += 8;
            VectorSubtract(target, bs->eye, dir);
            vectoangles(dir, moveresult->ideal_viewangles);
            //
            moveresult->weapon = BotSelectActivateWeapon(bs);
            if (moveresult->weapon == -1) {
                // FIXME: run away!
                moveresult->weapon = 0;
            }
            if (moveresult->weapon) {
                //
                moveresult->flags |= MOVERESULT_MOVEMENTWEAPON | MOVERESULT_MOVEMENTVIEW;
                // if holding the right weapon
                if (bs->cur_ps.weapon == moveresult->weapon) {
                    // if the bot is pretty close with its aim
                    if (InFieldOfVision(bs->viewangles, 20, moveresult->ideal_viewangles)) {
                        //
                        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, target, bs->entitynum, MASK_SHOT);
                        // if the mine is visible from the current position
                        if (bsptrace.fraction >= 1.0 || bsptrace.ent == state.number) {
                            // shoot at the mine
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
    // if blocked by an avoid spot and the view angles and weapon are used for movement
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
            //
            // state->generic1 == TEAM_RED || state->generic1 == TEAM_BLUE
            //
            // deactivate prox mines in the bot's path by shooting
            // rockets or plasma cells etc. at them
            BotAI_GetEntityState(bs->proxmines[bestmine], &state);
            VectorCopy(state.pos.trBase, target);
            target[2] += 2;
            VectorSubtract(target, bs->eye, dir);
            vectoangles(dir, moveresult->ideal_viewangles);
            // if the bot has a weapon that does splash damage
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
                //
                moveresult->flags |= MOVERESULT_MOVEMENTWEAPON | MOVERESULT_MOVEMENTVIEW;
                // if holding the right weapon
                if (bs->cur_ps.weapon == moveresult->weapon) {
                    // if the bot is pretty close with its aim
                    if (InFieldOfVision(bs->viewangles, 20, moveresult->ideal_viewangles)) {
                        //
                        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, target, bs->entitynum, MASK_SHOT);
                        // if the mine is visible from the current position
                        if (bsptrace.fraction >= 1.0 || bsptrace.ent == state.number) {
                            // shoot at the mine
                            trap_EA_Attack(bs->client);
                        }
                    }
                }
            }
        }
    }
}

/*
==================
AIEnter_Seek_ActivateEntity
==================
*/
void AIEnter_Seek_ActivateEntity(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "activate entity", "", s);
    bs->ainode = AINode_Seek_ActivateEntity;
}

/*
==================
AINode_Seek_Activate_Entity
==================
*/
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
    // if in the intermission
    if (BotIntermission(bs)) {
        BotClearActivateGoalStack(bs);
        AIEnter_Intermission(bs, "activate entity: intermission");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        BotClearActivateGoalStack(bs);
        AIEnter_Respawn(bs, "activate entity: bot dead");
        return qfalse;
    }
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    // map specific code
    BotMapScripts(bs);
    // no enemy
    bs->enemy = -1;
    // if the bot has no activate goal
    if (!bs->activatestack) {
        BotClearActivateGoalStack(bs);
        AIEnter_Seek_NBG(bs, "activate entity: no goal");
        return qfalse;
    }
    //
    goal = &bs->activatestack->goal;
    // initialize target being visible to false
    targetvisible = qfalse;
    // if the bot has to shoot at a target to activate something
    if (bs->activatestack->shoot) {
        //
        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->activatestack->target, bs->entitynum, MASK_SHOT);
        // if the shootable entity is visible from the current position
        if (bsptrace.fraction >= 1.0 || bsptrace.ent == goal->entitynum) {
            targetvisible = qtrue;
            // if holding the right weapon
            if (bs->cur_ps.weapon == bs->activatestack->weapon) {
                VectorSubtract(bs->activatestack->target, bs->eye, dir);
                vectoangles(dir, ideal_viewangles);
                // if the bot is pretty close with its aim
                if (InFieldOfVision(bs->viewangles, 20, ideal_viewangles)) {
                    trap_EA_Attack(bs->client);
                }
            }
        }
    }
    // if the shoot target is visible
    if (targetvisible) {
        // get the entity info of the entity the bot is shooting at
        BotEntityInfo(goal->entitynum, &entinfo);
        // if the entity the bot shoots at moved
        if (!VectorCompare(bs->activatestack->origin, entinfo.origin)) {
#ifdef DEBUG
            BotAI_Print(PRT_MESSAGE, "hit shootable button or trigger\n");
#endif  // DEBUG
            bs->activatestack->time = 0;
        }
        // if the activate goal has been activated or the bot takes too long
        if (bs->activatestack->time < FloatTime()) {
            BotPopFromActivateGoalStack(bs);
            // if there are more activate goals on the stack
            if (bs->activatestack) {
                bs->activatestack->time = FloatTime() + 10;
                return qfalse;
            }
            AIEnter_Seek_NBG(bs, "activate entity: time out");
            return qfalse;
        }
        memset(&moveresult, 0, sizeof(bot_moveresult_t));
    } else {
        // if the bot has no goal
        if (!goal) {
            bs->activatestack->time = 0;
        }
        // if the bot does not have a shoot goal
        else if (!bs->activatestack->shoot) {
            // if the bot touches the current goal
            if (trap_BotTouchingGoal(bs->origin, goal)) {
#ifdef DEBUG
                BotAI_Print(PRT_MESSAGE, "touched button or trigger\n");
#endif  // DEBUG
                bs->activatestack->time = 0;
            }
        }
        // if the activate goal has been activated or the bot takes too long
        if (bs->activatestack->time < FloatTime()) {
            BotPopFromActivateGoalStack(bs);
            // if there are more activate goals on the stack
            if (bs->activatestack) {
                bs->activatestack->time = FloatTime() + 10;
                return qfalse;
            }
            AIEnter_Seek_NBG(bs, "activate entity: activated");
            return qfalse;
        }
        // predict obstacles
        if (BotAIPredictObstacles(bs, goal))
            return qfalse;
        // initialize the movement state
        BotSetupForMovement(bs);
        // move towards the goal
        trap_BotMoveToGoal(&moveresult, bs->ms, goal, bs->tfl);
        // if the movement failed
        if (moveresult.failure) {
            // reset the avoid reach, otherwise bot is stuck in current area
            trap_BotResetAvoidReach(bs->ms);
            //
            bs->activatestack->time = 0;
        }
        // check if the bot is blocked
        BotAIBlocked(bs, &moveresult, qtrue);
    }
    //
    BotClearPath(bs, &moveresult);
    // if the bot has to shoot to activate
    if (bs->activatestack->shoot) {
        // if the view angles aren't yet used for the movement
        if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEW)) {
            VectorSubtract(bs->activatestack->target, bs->eye, dir);
            vectoangles(dir, moveresult.ideal_viewangles);
            moveresult.flags |= MOVERESULT_MOVEMENTVIEW;
        }
        // if there's no weapon yet used for the movement
        if (!(moveresult.flags & MOVERESULT_MOVEMENTWEAPON)) {
            moveresult.flags |= MOVERESULT_MOVEMENTWEAPON;
            //
            bs->activatestack->weapon = BotSelectActivateWeapon(bs);
            if (bs->activatestack->weapon == -1) {
                // FIXME: find a decent weapon first
                bs->activatestack->weapon = 0;
            }
            moveresult.weapon = bs->activatestack->weapon;
        }
    }
    // if the ideal view angles are set for movement
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    }
    // if waiting for something
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
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    // if there is an enemy
    if (BotFindEnemy(bs, -1)) {
        if (BotWantsToRetreat(bs)) {
            // keep the current long term goal and retreat
            AIEnter_Battle_NBG(bs, "activate entity: found enemy");
        } else {
            trap_BotResetLastAvoidReach(bs->ms);
            // empty the goal stack
            trap_BotEmptyGoalStack(bs->gs);
            // go fight
            AIEnter_Battle_Fight(bs, "activate entity: found enemy");
        }
        BotClearActivateGoalStack(bs);
    }
    return qtrue;
}

/*
==================
AIEnter_Seek_NBG
==================
*/
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

/*
==================
AINode_Seek_NBG
==================
*/
int AINode_Seek_NBG(bot_state_t* bs) {
    bot_goal_t goal;
    vec3_t target, dir;
    bot_moveresult_t moveresult;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "seek nbg: observer");
        return qfalse;
    }
    // if in the intermission
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "seek nbg: intermision");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "seek nbg: bot dead");
        return qfalse;
    }
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    //
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    // map specific code
    BotMapScripts(bs);
    // no enemy
    bs->enemy = -1;
    // if the bot has no goal
    if (!trap_BotGetTopGoal(bs->gs, &goal))
        bs->nbg_time = 0;
    // if the bot touches the current goal
    else if (BotReachedGoal(bs, &goal)) {
        BotChooseWeapon(bs);
        bs->nbg_time = 0;
    }
    //
    if (bs->nbg_time < FloatTime()) {
        // pop the current goal from the stack
        trap_BotPopGoal(bs->gs);
        // check for new nearby items right away
        // NOTE: we canNOT reset the check_time to zero because it would create an endless loop of node switches
        bs->check_time = FloatTime() + 0.05;
        // go back to seek ltg
        AIEnter_Seek_LTG(bs, "seek nbg: time out");
        return qfalse;
    }
    // predict obstacles
    if (BotAIPredictObstacles(bs, &goal))
        return qfalse;
    // initialize the movement state
    BotSetupForMovement(bs);
    // move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    // if the movement failed
    if (moveresult.failure) {
        // reset the avoid reach, otherwise bot is stuck in current area
        trap_BotResetAvoidReach(bs->ms);
        bs->nbg_time = 0;
    }
    // check if the bot is blocked
    BotAIBlocked(bs, &moveresult, qtrue);
    //
    BotClearPath(bs, &moveresult);
    // if the viewangles are used for the movement
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    }
    // if waiting for something
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
        // FIXME: look at cluster portals?
        else
            vectoangles(moveresult.movedir, bs->ideal_viewangles);
        bs->ideal_viewangles[2] *= 0.5;
    }
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    // if there is an enemy
    if (BotFindEnemy(bs, -1)) {
        if (BotWantsToRetreat(bs)) {
            // keep the current long term goal and retreat
            AIEnter_Battle_NBG(bs, "seek nbg: found enemy");
        } else {
            trap_BotResetLastAvoidReach(bs->ms);
            // empty the goal stack
            trap_BotEmptyGoalStack(bs->gs);
            // go fight
            AIEnter_Battle_Fight(bs, "seek nbg: found enemy");
        }
    }
    return qtrue;
}

/*
==================
AIEnter_Seek_LTG
==================
*/
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

/*
==================
AINode_Seek_LTG
==================
*/
int AINode_Seek_LTG(bot_state_t* bs) {
    bot_goal_t goal;
    vec3_t target, dir;
    bot_moveresult_t moveresult;
    int range;
    // char buf[128];
    // bot_goal_t tmpgoal;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "seek ltg: observer");
        return qfalse;
    }
    // if in the intermission
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "seek ltg: intermission");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "seek ltg: bot dead");
        return qfalse;
    }
    //
    if (BotChat_Random(bs)) {
        bs->stand_time = FloatTime() + BotChatTime(bs);
        AIEnter_Stand(bs, "seek ltg: random chat");
        return qfalse;
    }
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    //
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    // map specific code
    BotMapScripts(bs);
    // no enemy
    bs->enemy = -1;
    //
    if (bs->killedenemy_time > FloatTime() - 2) {
        if (random() < bs->thinktime * 1) {
            trap_EA_Gesture(bs->client);
        }
    }
    // if there is an enemy
    if (BotFindEnemy(bs, -1)) {
        if (BotWantsToRetreat(bs)) {
            // keep the current long term goal and retreat
            AIEnter_Battle_Retreat(bs, "seek ltg: found enemy");
            return qfalse;
        } else {
            trap_BotResetLastAvoidReach(bs->ms);
            // empty the goal stack
            trap_BotEmptyGoalStack(bs->gs);
            // go fight
            AIEnter_Battle_Fight(bs, "seek ltg: found enemy");
            return qfalse;
        }
    }
    //
    BotTeamGoals(bs, qfalse);
    // get the current long term goal
    if (!BotLongTermGoal(bs, bs->tfl, qfalse, &goal)) {
        return qtrue;
    }
    // check for nearby goals periodicly
    if (bs->check_time < FloatTime()) {
        bs->check_time = FloatTime() + 0.5;
        // check if the bot wants to camp
        BotWantsToCamp(bs);
        //
        if (bs->ltgtype == LTG_DEFENDKEYAREA)
            range = 400;
        else
            range = 150;
        //
#ifdef CTF
        if (gametype == GT_CTF) {
            // if carrying a flag the bot shouldn't be distracted too much
            if (BotCTFCarryingFlag(bs))
                range = 50;
        }
#endif  // CTF
        else if (gametype == GT_1FCTF) {
            if (Bot1FCTFCarryingFlag(bs))
                range = 50;
        } else if (gametype == GT_HARVESTER) {
            if (BotHarvesterCarryingCubes(bs))
                range = 80;
        }
        //
        if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
            trap_BotResetLastAvoidReach(bs->ms);
            // get the goal at the top of the stack
            // trap_BotGetTopGoal(bs->gs, &tmpgoal);
            // trap_BotGoalName(tmpgoal.number, buf, 144);
            // BotAI_Print(PRT_MESSAGE, "new nearby goal %s\n", buf);
            // time the bot gets to pick up the nearby goal item
            bs->nbg_time = FloatTime() + 4 + range * 0.01;
            AIEnter_Seek_NBG(bs, "ltg seek: nbg");
            return qfalse;
        }
    }
    // predict obstacles
    if (BotAIPredictObstacles(bs, &goal))
        return qfalse;
    // initialize the movement state
    BotSetupForMovement(bs);
    // move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    // if the movement failed
    if (moveresult.failure) {
        // reset the avoid reach, otherwise bot is stuck in current area
        trap_BotResetAvoidReach(bs->ms);
        // BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
        bs->ltg_time = 0;
    }
    //
    BotAIBlocked(bs, &moveresult, qtrue);
    //
    BotClearPath(bs, &moveresult);
    // if the viewangles are used for the movement
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    }
    // if waiting for something
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
        // FIXME: look at cluster portals?
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
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    //
    return qtrue;
}

/*
==================
AIEnter_Battle_Fight
==================
*/
void AIEnter_Battle_Fight(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle fight", "", s);
    trap_BotResetLastAvoidReach(bs->ms);
    bs->ainode = AINode_Battle_Fight;
    bs->flags &= ~BFL_FIGHTSUICIDAL;
}

/*
==================
AIEnter_Battle_SuicidalFight
==================
*/
void AIEnter_Battle_SuicidalFight(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle fight", "", s);
    trap_BotResetLastAvoidReach(bs->ms);
    bs->ainode = AINode_Battle_Fight;
    bs->flags |= BFL_FIGHTSUICIDAL;
}

/*
==================
AINode_Battle_Fight
==================
*/
int AINode_Battle_Fight(bot_state_t* bs) {
    int areanum;
    vec3_t target;
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "battle fight: observer");
        return qfalse;
    }

    // if in the intermission
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle fight: intermission");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle fight: bot dead");
        return qfalse;
    }
    // if there is another better enemy
    if (BotFindEnemy(bs, bs->enemy)) {
#ifdef DEBUG
        BotAI_Print(PRT_MESSAGE, "found new better enemy\n");
#endif
    }
    // if no enemy
    if (bs->enemy < 0) {
        AIEnter_Seek_LTG(bs, "battle fight: no enemy");
        return qfalse;
    }
    //
    BotEntityInfo(bs->enemy, &entinfo);
    // if the enemy is dead
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
    // if the enemy is invisible and not shooting the bot looses track easily
    if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
        if (random() < 0.2) {
            AIEnter_Seek_LTG(bs, "battle fight: invisible");
            return qfalse;
        }
    }
    //
    VectorCopy(entinfo.origin, target);
    // if not a player enemy
    if (bs->enemy >= MAX_CLIENTS) {
        // if attacking an obelisk
        if (bs->enemy == redobelisk.entitynum ||
            bs->enemy == blueobelisk.entitynum) {
            target[2] += 16;
        }
    }
    // update the reachability area and origin if possible
    areanum = BotPointAreaNum(target);
    if (areanum && trap_AAS_AreaReachability(areanum)) {
        VectorCopy(target, bs->lastenemyorigin);
        bs->lastenemyareanum = areanum;
    }
    // update the attack inventory values
    BotUpdateBattleInventory(bs, bs->enemy);
    // if the bot's health decreased
    if (bs->lastframe_health > bs->inventory[INVENTORY_HEALTH]) {
        if (BotChat_HitNoDeath(bs)) {
            bs->stand_time = FloatTime() + BotChatTime(bs);
            AIEnter_Stand(bs, "battle fight: chat health decreased");
            return qfalse;
        }
    }
    // if the bot hit someone
    if (bs->cur_ps.persistant[PERS_HITS] > bs->lasthitcount) {
        if (BotChat_HitNoKill(bs)) {
            bs->stand_time = FloatTime() + BotChatTime(bs);
            AIEnter_Stand(bs, "battle fight: chat hit someone");
            return qfalse;
        }
    }
    // if the enemy is not visible
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
    // use holdable items
    BotBattleUseItems(bs);
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    //
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    // choose the best weapon to fight with
    BotChooseWeapon(bs);
    // do attack movements
    moveresult = BotAttackMove(bs, bs->tfl);
    // if the movement failed
    if (moveresult.failure) {
        // reset the avoid reach, otherwise bot is stuck in current area
        trap_BotResetAvoidReach(bs->ms);
        // BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
        bs->ltg_time = 0;
    }
    //
    BotAIBlocked(bs, &moveresult, qfalse);
    // aim at the enemy
    BotAimAtEnemy(bs);
    // attack the enemy if possible
    BotCheckAttack(bs);
    // if the bot wants to retreat
    if (!(bs->flags & BFL_FIGHTSUICIDAL)) {
        if (BotWantsToRetreat(bs)) {
            AIEnter_Battle_Retreat(bs, "battle fight: wants to retreat");
            return qtrue;
        }
    }
    return qtrue;
}

/*
==================
AIEnter_Battle_Chase
==================
*/
void AIEnter_Battle_Chase(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle chase", "", s);
    bs->chase_time = FloatTime();
    bs->ainode = AINode_Battle_Chase;
}

/*
==================
AINode_Battle_Chase
==================
*/
int AINode_Battle_Chase(bot_state_t* bs) {
    bot_goal_t goal;
    vec3_t target, dir;
    bot_moveresult_t moveresult;
    float range;

    if (BotIsObserver(bs)) {
        AIEnter_Observer(bs, "battle chase: observer");
        return qfalse;
    }
    // if in the intermission
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle chase: intermission");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle chase: bot dead");
        return qfalse;
    }
    // if no enemy
    if (bs->enemy < 0) {
        AIEnter_Seek_LTG(bs, "battle chase: no enemy");
        return qfalse;
    }
    // if the enemy is visible
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        AIEnter_Battle_Fight(bs, "battle chase");
        return qfalse;
    }
    // if there is another enemy
    if (BotFindEnemy(bs, -1)) {
        AIEnter_Battle_Fight(bs, "battle chase: better enemy");
        return qfalse;
    }
    // there is no last enemy area
    if (!bs->lastenemyareanum) {
        AIEnter_Seek_LTG(bs, "battle chase: no enemy area");
        return qfalse;
    }
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    //
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    // map specific code
    BotMapScripts(bs);
    // create the chase goal
    goal.entitynum = bs->enemy;
    goal.areanum = bs->lastenemyareanum;
    VectorCopy(bs->lastenemyorigin, goal.origin);
    VectorSet(goal.mins, -8, -8, -8);
    VectorSet(goal.maxs, 8, 8, 8);
    // if the last seen enemy spot is reached the enemy could not be found
    if (trap_BotTouchingGoal(bs->origin, &goal))
        bs->chase_time = 0;
    // if there's no chase time left
    if (!bs->chase_time || bs->chase_time < FloatTime() - 10) {
        AIEnter_Seek_LTG(bs, "battle chase: time out");
        return qfalse;
    }
    // check for nearby goals periodicly
    if (bs->check_time < FloatTime()) {
        bs->check_time = FloatTime() + 1;
        range = 150;
        //
        if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
            // the bot gets 5 seconds to pick up the nearby goal item
            bs->nbg_time = FloatTime() + 0.1 * range + 1;
            trap_BotResetLastAvoidReach(bs->ms);
            AIEnter_Battle_NBG(bs, "battle chase: nbg");
            return qfalse;
        }
    }
    //
    BotUpdateBattleInventory(bs, bs->enemy);
    // initialize the movement state
    BotSetupForMovement(bs);
    // move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    // if the movement failed
    if (moveresult.failure) {
        // reset the avoid reach, otherwise bot is stuck in current area
        trap_BotResetAvoidReach(bs->ms);
        // BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
        bs->ltg_time = 0;
    }
    //
    BotAIBlocked(bs, &moveresult, qfalse);
    //
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET | MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (!(bs->flags & BFL_IDEALVIEWSET)) {
        if (bs->chase_time > FloatTime() - 2) {
            BotAimAtEnemy(bs);
        } else {
            if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
            } else {
                vectoangles(moveresult.movedir, bs->ideal_viewangles);
            }
        }
        bs->ideal_viewangles[2] *= 0.5;
    }
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    // if the bot is in the area the enemy was last seen in
    if (bs->areanum == bs->lastenemyareanum)
        bs->chase_time = 0;
    // if the bot wants to retreat (the bot could have been damage during the chase)
    if (BotWantsToRetreat(bs)) {
        AIEnter_Battle_Retreat(bs, "battle chase: wants to retreat");
        return qtrue;
    }
    return qtrue;
}

/*
==================
AIEnter_Battle_Retreat
==================
*/
void AIEnter_Battle_Retreat(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle retreat", "", s);
    bs->ainode = AINode_Battle_Retreat;
}

/*
==================
AINode_Battle_Retreat
==================
*/
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
    // if in the intermission
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle retreat: intermission");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle retreat: bot dead");
        return qfalse;
    }
    // if no enemy
    if (bs->enemy < 0) {
        AIEnter_Seek_LTG(bs, "battle retreat: no enemy");
        return qfalse;
    }
    //
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsDead(&entinfo)) {
        AIEnter_Seek_LTG(bs, "battle retreat: enemy dead");
        return qfalse;
    }
    // if there is another better enemy
    if (BotFindEnemy(bs, bs->enemy)) {
#ifdef DEBUG
        BotAI_Print(PRT_MESSAGE, "found new better enemy\n");
#endif
    }
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    // map specific code
    BotMapScripts(bs);
    // update the attack inventory values
    BotUpdateBattleInventory(bs, bs->enemy);
    // if the bot doesn't want to retreat anymore... probably picked up some nice items
    if (BotWantsToChase(bs)) {
        // empty the goal stack, when chasing, only the enemy is the goal
        trap_BotEmptyGoalStack(bs->gs);
        // go chase the enemy
        AIEnter_Battle_Chase(bs, "battle retreat: wants to chase");
        return qfalse;
    }
    // update the last time the enemy was visible
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        bs->enemyvisible_time = FloatTime();
        VectorCopy(entinfo.origin, target);
        // if not a player enemy
        if (bs->enemy >= MAX_CLIENTS) {
            // if attacking an obelisk
            if (bs->enemy == redobelisk.entitynum ||
                bs->enemy == blueobelisk.entitynum) {
                target[2] += 16;
            }
        }
        // update the reachability area and origin if possible
        areanum = BotPointAreaNum(target);
        if (areanum && trap_AAS_AreaReachability(areanum)) {
            VectorCopy(target, bs->lastenemyorigin);
            bs->lastenemyareanum = areanum;
        }
    }
    // if the enemy is NOT visible for 4 seconds
    if (bs->enemyvisible_time < FloatTime() - 4) {
        AIEnter_Seek_LTG(bs, "battle retreat: lost enemy");
        return qfalse;
    }
    // else if the enemy is NOT visible
    else if (bs->enemyvisible_time < FloatTime()) {
        // if there is another enemy
        if (BotFindEnemy(bs, -1)) {
            AIEnter_Battle_Fight(bs, "battle retreat: another enemy");
            return qfalse;
        }
    }
    //
    BotTeamGoals(bs, qtrue);
    // use holdable items
    BotBattleUseItems(bs);
    // get the current long term goal while retreating
    if (!BotLongTermGoal(bs, bs->tfl, qtrue, &goal)) {
        AIEnter_Battle_SuicidalFight(bs, "battle retreat: no way out");
        return qfalse;
    }
    // check for nearby goals periodicly
    if (bs->check_time < FloatTime()) {
        bs->check_time = FloatTime() + 1;
        range = 150;
#ifdef CTF
        if (gametype == GT_CTF) {
            // if carrying a flag the bot shouldn't be distracted too much
            if (BotCTFCarryingFlag(bs))
                range = 50;
        }
#endif  // CTF
        else if (gametype == GT_1FCTF) {
            if (Bot1FCTFCarryingFlag(bs))
                range = 50;
        } else if (gametype == GT_HARVESTER) {
            if (BotHarvesterCarryingCubes(bs))
                range = 80;
        }
        //
        if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
            trap_BotResetLastAvoidReach(bs->ms);
            // time the bot gets to pick up the nearby goal item
            bs->nbg_time = FloatTime() + range / 100 + 1;
            AIEnter_Battle_NBG(bs, "battle retreat: nbg");
            return qfalse;
        }
    }
    // initialize the movement state
    BotSetupForMovement(bs);
    // move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    // if the movement failed
    if (moveresult.failure) {
        // reset the avoid reach, otherwise bot is stuck in current area
        trap_BotResetAvoidReach(bs->ms);
        // BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
        bs->ltg_time = 0;
    }
    //
    BotAIBlocked(bs, &moveresult, qfalse);
    // choose the best weapon to fight with
    BotChooseWeapon(bs);
    // if the view is fixed for the movement
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEWSET) && !(bs->flags & BFL_IDEALVIEWSET)) {
        attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
        // if the bot is skilled enough
        if (attack_skill > 0.3) {
            BotAimAtEnemy(bs);
        } else {
            if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
            } else {
                vectoangles(moveresult.movedir, bs->ideal_viewangles);
            }
            bs->ideal_viewangles[2] *= 0.5;
        }
    }
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    // attack the enemy if possible
    BotCheckAttack(bs);
    //
    return qtrue;
}

/*
==================
AIEnter_Battle_NBG
==================
*/
void AIEnter_Battle_NBG(bot_state_t* bs, char* s) {
    BotRecordNodeSwitch(bs, "battle NBG", "", s);
    bs->ainode = AINode_Battle_NBG;
}

/*
==================
AINode_Battle_NBG
==================
*/
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
    // if in the intermission
    if (BotIntermission(bs)) {
        AIEnter_Intermission(bs, "battle nbg: intermission");
        return qfalse;
    }
    // respawn if dead
    if (BotIsDead(bs)) {
        AIEnter_Respawn(bs, "battle nbg: bot dead");
        return qfalse;
    }
    // if no enemy
    if (bs->enemy < 0) {
        AIEnter_Seek_NBG(bs, "battle nbg: no enemy");
        return qfalse;
    }
    //
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsDead(&entinfo)) {
        AIEnter_Seek_NBG(bs, "battle nbg: enemy dead");
        return qfalse;
    }
    //
    bs->tfl = TFL_DEFAULT;
    if (bot_grapple.integer)
        bs->tfl |= TFL_GRAPPLEHOOK;
    // if in lava or slime the bot should be able to get out
    if (BotInLavaOrSlime(bs))
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    //
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    // map specific code
    BotMapScripts(bs);
    // update the last time the enemy was visible
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
        bs->enemyvisible_time = FloatTime();
        VectorCopy(entinfo.origin, target);
        // if not a player enemy
        if (bs->enemy >= MAX_CLIENTS) {
            // if attacking an obelisk
            if (bs->enemy == redobelisk.entitynum ||
                bs->enemy == blueobelisk.entitynum) {
                target[2] += 16;
            }
        }
        // update the reachability area and origin if possible
        areanum = BotPointAreaNum(target);
        if (areanum && trap_AAS_AreaReachability(areanum)) {
            VectorCopy(target, bs->lastenemyorigin);
            bs->lastenemyareanum = areanum;
        }
    }
    // if the bot has no goal or touches the current goal
    if (!trap_BotGetTopGoal(bs->gs, &goal)) {
        bs->nbg_time = 0;
    } else if (BotReachedGoal(bs, &goal)) {
        bs->nbg_time = 0;
    }
    //
    if (bs->nbg_time < FloatTime()) {
        // pop the current goal from the stack
        trap_BotPopGoal(bs->gs);
        // if the bot still has a goal
        if (trap_BotGetTopGoal(bs->gs, &goal))
            AIEnter_Battle_Retreat(bs, "battle nbg: time out");
        else
            AIEnter_Battle_Fight(bs, "battle nbg: time out");
        //
        return qfalse;
    }
    // initialize the movement state
    BotSetupForMovement(bs);
    // move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    // if the movement failed
    if (moveresult.failure) {
        // reset the avoid reach, otherwise bot is stuck in current area
        trap_BotResetAvoidReach(bs->ms);
        // BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
        bs->nbg_time = 0;
    }
    //
    BotAIBlocked(bs, &moveresult, qfalse);
    // update the attack inventory values
    BotUpdateBattleInventory(bs, bs->enemy);
    // choose the best weapon to fight with
    BotChooseWeapon(bs);
    // if the view is fixed for the movement
    if (moveresult.flags & (MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW)) {
        VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
    } else if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEWSET) && !(bs->flags & BFL_IDEALVIEWSET)) {
        attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
        // if the bot is skilled enough and the enemy is visible
        if (attack_skill > 0.3) {
            //&& BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)
            BotAimAtEnemy(bs);
        } else {
            if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
                VectorSubtract(target, bs->origin, dir);
                vectoangles(dir, bs->ideal_viewangles);
            } else {
                vectoangles(moveresult.movedir, bs->ideal_viewangles);
            }
            bs->ideal_viewangles[2] *= 0.5;
        }
    }
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
        bs->weaponnum = moveresult.weapon;
    // attack the enemy if possible
    BotCheckAttack(bs);
    //
    return qtrue;
}

/*
==================
FindInstaGibTarget

[QL] Pick the nearest visible, reachable, live player for the InstaGib
hunting node. Returns the target entity number or -1.
(binary FindInstaGibTarget 0x10020200)
==================
*/
int FindInstaGibTarget(bot_state_t* bs) {
    int i, besttarget, area, numareas;
    float bestdist, dist;
    vec3_t dir, end;
    aas_entityinfo_t entinfo;
    gentity_t* ent;
    int areas[10];

    bestdist = 100000000.0;
    besttarget = -1;

    // keep the current enemy if it is still visible and reachable
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

    // find the nearest visible, reachable, live client
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (i == bs->entitynum) {
            continue;
        }
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid) {
            continue;
        }
        // only hunt clients that are alive and playing
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

    // fall back to the nearest client we can trace an area to
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

/*
==================
AIEnter_InstaGib

[QL] (binary AIEnter_InstaGib 0x100133c0)
==================
*/
void AIEnter_InstaGib(bot_state_t* bs, char* s) {
    // never time out the long term goal while hunting
    bs->ltg_time = FloatTime() + 99999;
    // ltgtype 21 marks this node so BotDeathmatchAI does not re-enter it every frame
    bs->ltgtype = 21;
    bs->ainode = AINode_InstaGib;
    BotRecordNodeSwitch(bs, "insta gib!", "", s);
}

/*
==================
AINode_InstaGib

[QL] Per-frame InstaGib hunting node: find the nearest player, path to it and
shoot. (binary AINode_InstaGib 0x10013410)
==================
*/
int AINode_InstaGib(bot_state_t* bs) {
    int target;
    bot_goal_t goal;
    vec3_t dir, target_org;
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;

    // pick a target up front
    target = FindInstaGibTarget(bs);

    if (BotIsObserver(bs)) {
        bs->ltgtype = 0;
        AIEnter_Observer(bs, "insta gib!: observer");
        return qfalse;
    }
    // if in the intermission
    if (bs->cur_ps.pm_type == PM_INTERMISSION) {
        g_entities[bs->client].flags &= ~FL_NOPICKUP;
        g_entities[bs->client].client->ps.powerups[6] = 0;  // client+0x158
        g_entities[bs->client].client->ps.powerups[7] = 0;  // client+0x15c
        bs->ltgtype = 0;
        AIEnter_Intermission(bs, "insta gib!: intermission");
        return qfalse;
    }
    // respawn if dead
    if (bs->cur_ps.pm_type == PM_DEAD) {
        g_entities[bs->client].flags &= ~FL_NOPICKUP;
        g_entities[bs->client].client->ps.powerups[6] = 0;  // client+0x158
        g_entities[bs->client].client->ps.powerups[7] = 0;  // client+0x15c
        bs->ltgtype = 0;
        AIEnter_Respawn(bs, "insta gib!: bot dead");
        return qfalse;
    }
    // instagib turned off, hand back to the normal seek node
    if (!g_instaGib.integer) {
        bs->ltgtype = 0;
        AIEnter_Seek_LTG(bs, "insta gib!: no ai node");
        return qfalse;
    }
    // if there is an enemy go and fight it
    if (BotFindEnemy(bs, -1)) {
        bs->ltgtype = 0;
        trap_BotResetLastAvoidReach(bs->ms);
        trap_BotEmptyGoalStack(bs->gs);
        AIEnter_Battle_Fight(bs, "insta gib!: found enemy");
        return qfalse;
    }
    // no target found, aim around now and then
    if (target == -1) {
        if (random() < bs->thinktime * 0.8) {
            BotAimAtEnemy(bs);
        }
        return qtrue;
    }
    // hunt the chosen target
    bs->tfl = TFL_DEFAULT;
    BotEntityInfo(target, &entinfo);
    // build a fresh goal on the target and push it
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
    // very close, just aim
    VectorSubtract(bs->origin, entinfo.origin, dir);
    if (VectorLength(dir) < 50) {
        if (random() < bs->thinktime * 0.8) {
            BotAimAtEnemy(bs);
            return qtrue;
        }
    }
    // travel flags
    if (BotInLavaOrSlime(bs)) {
        bs->tfl |= TFL_LAVA | TFL_SLIME;
    }
    if (BotCanAndWantsToRocketJump(bs)) {
        bs->tfl |= TFL_ROCKETJUMP;
    }
    // [QL] binary also calls BotCheckAutoHopFromUserinfo(&bs->tfl) here (0x10024e10);
    //      that helper is not yet ported.
    // predict obstacles
    if (BotAIPredictObstacles(bs, &goal)) {
        bs->ltgtype = 0;
        return qfalse;
    }
    // initialise the movement state
    BotSetupForMovement(bs);
    // move towards the target
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
    if (moveresult.failure) {
        // reset the avoid reach and just face the target this frame
        trap_BotResetAvoidReach(bs->ms);
        BotAimAtEnemy(bs);
        return qtrue;
    }
    BotAIBlocked(bs, &moveresult, qtrue);
    BotClearPath(bs, &moveresult);
    // if the view angles are used for the movement
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
    // if the weapon is used for the bot movement
    if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) {
        bs->weaponnum = moveresult.weapon;
    }
    return qtrue;
}

/*
==================
BotSetFreezeState

[QL] Freeze or unfreeze a bot. Used by the freeze-tag / team-training nodes
(AINode_Seek_Teammate, AINode_Teach_Teammate_To_Attack) which are not yet
ported, so it has no in-tree callers yet.
(binary BotSetFreezeState 0x100245c0)
==================
*/
void BotSetFreezeState(bot_state_t* bs, int freeze) {
    gentity_t* ent;
    gclient_t* client;

    ent = &g_entities[bs->client];
    client = ent->client;
    if (freeze) {
        ent->flags |= FL_GODMODE | FL_NOPICKUP;      // 0x10010
        client->ps.ammo[5] = 9999;                    // client+0x194
        client->ps.stats[STAT_WEAPONS] |= 0x20;       // client+0xcc, grant weapon bit 5
        ent->flags |= FL_TRAINER | FL_NO_KNOCKBACK;   // 0x20800 (not FL_NOTARGET, which is 0x20)
        client->ps.stats[STAT_WEAPONS] |= 0x8000;     // client+0xcc, grant weapon bit 15
    } else {
        ent->flags &= ~(FL_GODMODE | FL_NOPICKUP);
        client->ps.ammo[3] = 10;                       // client+0x18c
        ent->flags &= ~(FL_TRAINER | FL_NO_KNOCKBACK);
    }
}

/*
===============================================================================

THE TRAINING BOT

Crash, leading the Crash Course's tour and then staying by the player until they
are ready to fight her, from qagamex86.dll. The tour itself is the map's
info_tour_point chain (g_training.c); these nodes walk it. The addresses in the
comments are retail's, to follow the port back to the binary.

===============================================================================
*/

// what the nodes clear off the bot whenever it stops leading (the inverse of
// BotSetFreezeState, inlined at every exit of the retail nodes)
static void BotTrainerRelease(bot_state_t* bs) {
    gentity_t* ent = &g_entities[bs->client];

    ent->flags &= ~(FL_GODMODE | FL_NOPICKUP);
    ent->client->ps.ammo[WP_SHOTGUN] = 10;
    ent->flags &= ~(FL_TRAINER | FL_NO_KNOCKBACK);
}

// 0x10024530: turn to a point, the roll halved as every node does
static void BotTourLookAt(bot_state_t* bs, vec3_t point) {
    vec3_t dir;

    VectorSubtract(point, bs->origin, dir);
    vectoangles(dir, bs->ideal_viewangles);
    bs->ideal_viewangles[2] *= 0.5f;
}

// 0x100243d0: whether two clients' boxes overlap, with 4 units to spare sideways
// and below (a is the bot, b the other client)
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

// 0x1000f7f0: where the bot stands, kept as a goal to come back to
static void BotTourSaveGoal(bot_state_t* bs) {
    VectorCopy(bs->origin, bs->tourSavedGoal.origin);
    bs->tourSavedGoal.areanum = bs->areanum;
    VectorSet(bs->tourSavedGoal.mins, -16, -16, -24);
    VectorSet(bs->tourSavedGoal.maxs, 16, 16, 32);
}

/*
===============
BotTourGoal

0x1001fec0. The tour point after `point`, a gentity number; one that is not an
entity of the level means the start, the point with spawnflag 1. The floor
16 units under it goes to `floor`, and to `look` the origin of the info_notnull
it targets, or vec3_origin. Returns its number, or -1 when there is none.
===============
*/
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

/*
===============
AIEnter_Training

0x1000f880, from BotDeathmatchAI while bot_training is 1 or 2. The bot becomes the
tour's guide -- invulnerable, unable to pick anything up, rockets to spare -- and
its goal the stop after the one it last left.
===============
*/
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

// the goal the nodes push when they go to the player
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

// what both nodes do with a movement result: where to look, and the weapon it asks for
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

/*
===============
AINode_Training

0x1000fa00. Each frame the guide leaves the tour if it died, went to spectate or
the tour was switched off, and ends it -- readying the player up for the match --
when there is no stop left. Otherwise it looks at the player while the tour holds,
takes as its goal the player (out of sight or not caught up) or the stop, and then
either arrives (reveals what the stop hides, fires its targets, says its line),
waits out the stop, moves on to the next, waits for the player, or walks.
===============
*/
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
    // no stop left: the tour is over, and the player is ready for the match
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
        // 0x10011871
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

    // the player died: a word once the body lands, and eyes on them
    if (hstate.eFlags & EF_DEAD) {
        if (!bs->tourSaidCareful && hstate.groundEntityNum != ENTITYNUM_NONE) {
            trap_EA_Command(bs->client, G_TrainingSay("botSay You've got to be careful in the Arena!"));
            bs->tourSaidCareful = 1;
        }
        BotTourLookAt(bs, hinfo.origin);
        return qtrue;
    }
    bs->tourSaidCareful = 0;

    // the tour holds: look at the player
    if (FloatTime() < (float)bs->tourNextTime) {
        BotTourLookAt(bs, hinfo.origin);
        return qtrue;
    }
    // showing a spot: aim just under it, and fire at it at the end if nothing is in the way
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

    // 0x1001002e
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
    // 0x100101c2: the bot is the one still on its way -- it is farther from the stop than the
    // player, who is not at it either. The fork had this the other way round, so the very first
    // stop (the player frozen 146 units from it) was announced instead of reached and Crash stood
    // there saying "This way!" for ever.
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
        // 0x10010359: the player first
        keepGoing = 1;
        BotTourGoalOnEntity(&goal, &hinfo, human);
    } else {
        // 0x10010457: the stop
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
    // 0x100105a7
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
        // 0x10010f25: the player is near, or something ahead needs them first
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
            // 0x10011032: step out of the way of a player walking into the bot
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

    // 0x10011147: on the way to a stop
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
            trap_EA_Command(bs->client, G_TrainingSay("botSay This way! No time to waste!"));
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
        // 0x100113c9: can the player follow the way ahead? a ledge, a pad or a teleport
        // out of their sight holds the tour for ten seconds
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
        // 0x1001066d: the first frame at the stop
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

    // 0x1001093e: at the stop
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
    // spawnflag 64: the stop waits for the player to carry a flag
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
        // spawnflag 256: the stop waits for the player to take what it showed
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

    // 0x10010dd9: on to the next stop. Just after a teleport the guide will go back
    // for a player who falls behind.
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
    // 0x10011554
    bs->tourLastNudge = FloatTime();
    bs->tourLookUntil = FloatTime();
    bs->nodeEnterTime = (int)FloatTime();
    if (600 > distBotHuman && canSee) {
        trap_EA_Action(bs->client, ACTION_WALK);  // retail's EA_Walk (botlib ea 0x10) sets just this
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

/*
===============
AIEnter_FollowMe

0x1000ecc0, from BotDeathmatchAI while bot_followMe is set, and when the tour has
no point left. In a training match the bot keeps the guide's protections and asks
the player to exit the tutorial.
===============
*/
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

/*
===============
AINode_FollowMe

0x1000eda0. Stays by the player: walks to them past bot_followDist, and near them
copies their crouch, steps aside when pushed from the front, looks around, waves
now and then, and in training reminds them to exit the tutorial.
===============
*/
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
        // 0x1000f2b4: by the player
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

    // 0x1000f532: walking to the player
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
        // in the air it looks much further along the path, unless the player is in view
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
