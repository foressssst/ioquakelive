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

#define AREACONTENTS_MOVER 1024
#define AREACONTENTS_MODELNUMSHIFT 24
#define AREACONTENTS_MAXMODELNUM 0xFF
#define AREACONTENTS_MODELNUM (AREACONTENTS_MAXMODELNUM << AREACONTENTS_MODELNUMSHIFT)

#define IDEAL_ATTACKDIST 140

#define MAX_WAYPOINTS 128
bot_waypoint_t botai_waypoints[MAX_WAYPOINTS];
bot_waypoint_t* botai_freewaypoints;

int gametype;

vmCvar_t bot_grapple;
vmCvar_t bot_rocketjump;
vmCvar_t bot_fastchat;
vmCvar_t bot_nochat;
vmCvar_t bot_testrchat;
vmCvar_t bot_challenge;
vmCvar_t bot_predictobstacles;
vmCvar_t g_spSkill;

extern vmCvar_t bot_developer;

vec3_t lastteleport_origin;
float lastteleport_time;
int max_bspmodelindex;

bot_goal_t ctf_redflag;
bot_goal_t ctf_blueflag;
bot_goal_t ctf_neutralflag;
bot_goal_t redobelisk;
bot_goal_t blueobelisk;
bot_goal_t neutralobelisk;

#define MAX_ALTROUTEGOALS 32

int altroutegoals_setup;
aas_altroutegoal_t red_altroutegoals[MAX_ALTROUTEGOALS];
int red_numaltroutegoals;
aas_altroutegoal_t blue_altroutegoals[MAX_ALTROUTEGOALS];
int blue_numaltroutegoals;

int BotCTFCarryingFlag(bot_state_t* bs) {
    if (gametype != GT_CTF && gametype != GT_AD)
        return CTF_FLAG_NONE;

    if (bs->inventory[INVENTORY_REDFLAG] > 0)
        return CTF_FLAG_RED;
    else if (bs->inventory[INVENTORY_BLUEFLAG] > 0)
        return CTF_FLAG_BLUE;
    return CTF_FLAG_NONE;
}

int BotTeam(bot_state_t* bs) {
    if (bs->client < 0 || bs->client >= MAX_CLIENTS) {
        return qfalse;
    }

    if (level.clients[bs->client].sess.sessionTeam == TEAM_RED) {
        return TEAM_RED;
    } else if (level.clients[bs->client].sess.sessionTeam == TEAM_BLUE) {
        return TEAM_BLUE;
    }

    return TEAM_FREE;
}

int BotOppositeTeam(bot_state_t* bs) {
    switch (BotTeam(bs)) {
        case TEAM_RED:
            return TEAM_BLUE;
        case TEAM_BLUE:
            return TEAM_RED;
        default:
            return TEAM_FREE;
    }
}

bot_goal_t* BotEnemyFlag(bot_state_t* bs) {
    if (BotTeam(bs) == TEAM_RED) {
        return &ctf_blueflag;
    } else {
        return &ctf_redflag;
    }
}

bot_goal_t* BotTeamFlag(bot_state_t* bs) {
    if (BotTeam(bs) == TEAM_RED) {
        return &ctf_redflag;
    } else {
        return &ctf_blueflag;
    }
}

qboolean EntityIsDead(aas_entityinfo_t* entinfo) {
    playerState_t ps;

    if (entinfo->number >= 0 && entinfo->number < MAX_CLIENTS) {
        if (!BotAI_GetClientState(entinfo->number, &ps)) {
            return qfalse;
        }

        if (ps.pm_type != PM_NORMAL)
            return qtrue;
    }
    return qfalse;
}

qboolean EntityCarriesFlag(aas_entityinfo_t* entinfo) {
    if (entinfo->powerups & (1 << PW_REDFLAG))
        return qtrue;
    if (entinfo->powerups & (1 << PW_BLUEFLAG))
        return qtrue;
    if (entinfo->powerups & (1 << PW_NEUTRALFLAG))
        return qtrue;
    return qfalse;
}

qboolean EntityIsInvisible(aas_entityinfo_t* entinfo) {
    if (EntityCarriesFlag(entinfo)) {
        return qfalse;
    }
    if (entinfo->powerups & (1 << PW_INVIS)) {
        return qtrue;
    }
    return qfalse;
}

qboolean EntityIsShooting(aas_entityinfo_t* entinfo) {
    if (entinfo->flags & EF_FIRING) {
        return qtrue;
    }
    return qfalse;
}

qboolean EntityIsChatting(aas_entityinfo_t* entinfo) {
    if (entinfo->flags & EF_TALK) {
        return qtrue;
    }
    return qfalse;
}

qboolean EntityHasQuad(aas_entityinfo_t* entinfo) {
    if (entinfo->powerups & (1 << PW_QUAD)) {
        return qtrue;
    }
    return qfalse;
}

qboolean EntityHasKamikaze(aas_entityinfo_t* entinfo) {
    if (entinfo->flags & EF_KAMIKAZE) {
        return qtrue;
    }
    return qfalse;
}

qboolean EntityCarriesCubes(aas_entityinfo_t* entinfo) {
    entityState_t state;

    if (gametype != GT_HARVESTER)
        return qfalse;
    BotAI_GetEntityState(entinfo->number, &state);
    if (state.generic1 > 0)
        return qtrue;
    return qfalse;
}

int Bot1FCTFCarryingFlag(bot_state_t* bs) {
    if (gametype != GT_1FCTF)
        return qfalse;

    if (bs->inventory[INVENTORY_NEUTRALFLAG] > 0)
        return qtrue;
    return qfalse;
}

int BotHarvesterCarryingCubes(bot_state_t* bs) {
    if (gametype != GT_HARVESTER)
        return qfalse;

    if (bs->inventory[INVENTORY_REDCUBE] > 0)
        return qtrue;
    if (bs->inventory[INVENTORY_BLUECUBE] > 0)
        return qtrue;
    return qfalse;
}

void BotRememberLastOrderedTask(bot_state_t* bs) {
    if (!bs->ordered) {
        return;
    }
    bs->lastgoal_decisionmaker = bs->decisionmaker;
    bs->lastgoal_ltgtype = bs->ltgtype;
    memcpy(&bs->lastgoal_teamgoal, &bs->teamgoal, sizeof(bot_goal_t));
    bs->lastgoal_teammate = bs->teammate;
}

void BotSetTeamStatus(bot_state_t* bs) {
}

int BotSetLastOrderedTask(bot_state_t* bs) {
    if (gametype == GT_CTF || gametype == GT_AD) {
        if (bs->lastgoal_ltgtype == LTG_RETURNFLAG) {
            if (BotTeam(bs) == TEAM_RED) {
                if (bs->redflagstatus == 0) {
                    bs->lastgoal_ltgtype = 0;
                }
            } else {
                if (bs->blueflagstatus == 0) {
                    bs->lastgoal_ltgtype = 0;
                }
            }
        }
    }

    if (bs->lastgoal_ltgtype) {
        bs->decisionmaker = bs->lastgoal_decisionmaker;
        bs->ordered = qtrue;
        bs->ltgtype = bs->lastgoal_ltgtype;
        memcpy(&bs->teamgoal, &bs->lastgoal_teamgoal, sizeof(bot_goal_t));
        bs->teammate = bs->lastgoal_teammate;
        bs->teamgoal_time = FloatTime() + 300;
        BotSetTeamStatus(bs);
        if (gametype == GT_CTF || gametype == GT_AD) {
            if (bs->ltgtype == LTG_GETFLAG) {
                bot_goal_t *tb, *eb;
                int tt, et;

                tb = BotTeamFlag(bs);
                eb = BotEnemyFlag(bs);
                tt = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, tb->areanum, TFL_DEFAULT);
                et = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, eb->areanum, TFL_DEFAULT);
                if (et > tt) {
                    BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
                }
            }
        }
        return qtrue;
    }
    return qfalse;
}

void BotRefuseOrder(bot_state_t* bs) {
    if (!bs->ordered)
        return;
    if (bs->order_time && bs->order_time > FloatTime() - 10) {
        trap_EA_Action(bs->client, ACTION_NEGATIVE);
        BotVoiceChat(bs, bs->decisionmaker, VOICECHAT_NO);
        bs->order_time = 0;
    }
}

void BotCTFSeekGoals(bot_state_t* bs) {
    float rnd, l1, l2;
    int flagstatus, c;
    vec3_t dir;
    aas_entityinfo_t entinfo;

    if (BotCTFCarryingFlag(bs)) {
        if (BotTeam(bs) == TEAM_RED) {
            flagstatus = bs->redflagstatus * 2 + bs->blueflagstatus;
            VectorSubtract(bs->origin, ctf_redflag.origin, dir);
        } else {
            flagstatus = bs->blueflagstatus * 2 + bs->redflagstatus;
            VectorSubtract(bs->origin, ctf_blueflag.origin, dir);
        }
        if (flagstatus > 1 && VectorLength(dir) < 1000)
            return;
        if (bs->ltgtype != LTG_RUSHBASE) {
            BotRefuseOrder(bs);
            bs->ltgtype = LTG_RUSHBASE;
            bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
            bs->rushbaseaway_time = 0;
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            switch (BotTeam(bs)) {
                case TEAM_RED:
                    VectorSubtract(bs->origin, ctf_blueflag.origin, dir);
                    break;
                case TEAM_BLUE:
                    VectorSubtract(bs->origin, ctf_redflag.origin, dir);
                    break;
                default:
                    VectorSet(dir, 999, 999, 999);
                    break;
            }
            if (VectorLength(dir) < 128) {
                BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
            } else {
                bs->altroutegoal.areanum = 0;
            }
            BotVoiceChat(bs, -1, VOICECHAT_IHAVEFLAG);
        } else if (bs->rushbaseaway_time > FloatTime()) {
            if (BotTeam(bs) == TEAM_RED)
                flagstatus = bs->redflagstatus;
            else
                flagstatus = bs->blueflagstatus;
            if (flagstatus == 0) {
                bs->rushbaseaway_time = 0;
            }
        }
        return;
    }
    if (bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered) {
        BotEntityInfo(bs->teammate, &entinfo);
        if (!EntityCarriesFlag(&entinfo)) {
            bs->ltgtype = 0;
        }
    }
    if (BotTeam(bs) == TEAM_RED)
        flagstatus = bs->redflagstatus * 2 + bs->blueflagstatus;
    else
        flagstatus = bs->blueflagstatus * 2 + bs->redflagstatus;
    if (flagstatus == 1) {
        if (bs->owndecision_time < FloatTime()) {
            if (!(bs->ltgtype == LTG_DEFENDKEYAREA &&
                  (bs->teamgoal.number == ctf_redflag.number ||
                   bs->teamgoal.number == ctf_blueflag.number))) {
                c = BotTeamFlagCarrierVisible(bs);
                if (c >= 0 &&
                    (bs->ltgtype != LTG_TEAMACCOMPANY || bs->teammate != c)) {
                    BotRefuseOrder(bs);
                    bs->decisionmaker = bs->client;
                    bs->ordered = qfalse;
                    bs->teammate = c;
                    bs->teammatevisible_time = FloatTime();
                    bs->teammessage_time = 0;
                    bs->arrive_time = 1;
                    BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
                    bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
                    bs->ltgtype = LTG_TEAMACCOMPANY;
                    bs->formation_dist = 3.5 * 32;
                    BotSetTeamStatus(bs);
                    bs->owndecision_time = FloatTime() + 5;
                }
            }
        }
        return;
    }
    else if (flagstatus == 2) {
        if (bs->owndecision_time < FloatTime()) {
            c = BotEnemyFlagCarrierVisible(bs);
            if (c >= 0) {
            }
            if (bs->ltgtype != LTG_GETFLAG &&
                bs->ltgtype != LTG_RETURNFLAG &&
                bs->ltgtype != LTG_TEAMHELP &&
                bs->ltgtype != LTG_TEAMACCOMPANY &&
                bs->ltgtype != LTG_CAMPORDER &&
                bs->ltgtype != LTG_PATROL &&
                bs->ltgtype != LTG_GETITEM) {
                BotRefuseOrder(bs);
                bs->decisionmaker = bs->client;
                bs->ordered = qfalse;
                if (random() < 0.5) {
                    bs->ltgtype = LTG_GETFLAG;
                } else {
                    bs->ltgtype = LTG_RETURNFLAG;
                }
                bs->teammessage_time = 0;
                bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
                BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
                BotSetTeamStatus(bs);
                bs->owndecision_time = FloatTime() + 5;
            }
        }
        return;
    }
    else if (flagstatus == 3) {
        if (bs->owndecision_time < FloatTime()) {
            if (bs->ltgtype != LTG_RETURNFLAG && bs->ltgtype != LTG_TEAMACCOMPANY) {
                c = BotTeamFlagCarrierVisible(bs);
                if (c >= 0) {
                    BotRefuseOrder(bs);
                    bs->decisionmaker = bs->client;
                    bs->ordered = qfalse;
                    bs->teammate = c;
                    bs->teammatevisible_time = FloatTime();
                    bs->teammessage_time = 0;
                    bs->arrive_time = 1;
                    BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
                    bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
                    bs->ltgtype = LTG_TEAMACCOMPANY;
                    bs->formation_dist = 3.5 * 32;
                    BotSetTeamStatus(bs);
                    bs->owndecision_time = FloatTime() + 5;
                } else {
                    BotRefuseOrder(bs);
                    bs->decisionmaker = bs->client;
                    bs->ordered = qfalse;
                    bs->teammessage_time = FloatTime() + 2 * random();
                    bs->ltgtype = LTG_RETURNFLAG;
                    bs->teamgoal_time = FloatTime() + CTF_RETURNFLAG_TIME;
                    BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
                    BotSetTeamStatus(bs);
                    bs->owndecision_time = FloatTime() + 5;
                }
            }
        }
        return;
    }
    if (BotTeamLeader(bs)) {
        return;
    }
    if (bs->lastgoal_ltgtype) {
        bs->teamgoal_time += 60;
    }
    if (!bs->ordered && bs->lastgoal_ltgtype) {
        bs->ltgtype = 0;
    }
    if (bs->ltgtype == LTG_TEAMHELP ||
        bs->ltgtype == LTG_TEAMACCOMPANY ||
        bs->ltgtype == LTG_DEFENDKEYAREA ||
        bs->ltgtype == LTG_GETFLAG ||
        bs->ltgtype == LTG_RUSHBASE ||
        bs->ltgtype == LTG_RETURNFLAG ||
        bs->ltgtype == LTG_CAMPORDER ||
        bs->ltgtype == LTG_PATROL ||
        bs->ltgtype == LTG_GETITEM ||
        bs->ltgtype == LTG_MAKELOVE_UNDER ||
        bs->ltgtype == LTG_MAKELOVE_ONTOP) {
        return;
    }
    if (BotSetLastOrderedTask(bs))
        return;
    if (bs->owndecision_time > FloatTime())
        return;
    ;
    if (bs->ctfroam_time > FloatTime())
        return;
    if (BotAggression(bs) < 50)
        return;
    bs->teammessage_time = FloatTime() + 2 * random();
    if (bs->teamtaskpreference & (TEAMTP_ATTACKER | TEAMTP_DEFENDER)) {
        if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
            l1 = 0.7f;
        } else {
            l1 = 0.2f;
        }
        l2 = 0.9f;
    } else {
        l1 = 0.4f;
        l2 = 0.7f;
    }
    rnd = random();
    if (rnd < l1 && ctf_redflag.areanum && ctf_blueflag.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        bs->ltgtype = LTG_GETFLAG;
        bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
        BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
        BotSetTeamStatus(bs);
    } else if (rnd < l2 && ctf_redflag.areanum && ctf_blueflag.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        if (BotTeam(bs) == TEAM_RED)
            memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
        else
            memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
        bs->ltgtype = LTG_DEFENDKEYAREA;
        bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
        bs->defendaway_time = 0;
        BotSetTeamStatus(bs);
    } else {
        bs->ltgtype = 0;
        bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
        BotSetTeamStatus(bs);
    }
    bs->owndecision_time = FloatTime() + 5;
#ifdef DEBUG
    BotPrintTeamGoal(bs);
#endif
}

void BotCTFRetreatGoals(bot_state_t* bs) {
    if (BotCTFCarryingFlag(bs)) {
        if (bs->ltgtype != LTG_RUSHBASE) {
            BotRefuseOrder(bs);
            bs->ltgtype = LTG_RUSHBASE;
            bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
            bs->rushbaseaway_time = 0;
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            BotSetTeamStatus(bs);
        }
    }
}

void Bot1FCTFSeekGoals(bot_state_t* bs) {
    aas_entityinfo_t entinfo;
    float rnd, l1, l2;
    int c;

    if (Bot1FCTFCarryingFlag(bs)) {
        if (bs->ltgtype != LTG_RUSHBASE) {
            BotRefuseOrder(bs);
            bs->ltgtype = LTG_RUSHBASE;
            bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
            bs->rushbaseaway_time = 0;
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
            BotSetTeamStatus(bs);
            BotVoiceChat(bs, -1, VOICECHAT_IHAVEFLAG);
        }
        return;
    }
    if (bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered) {
        BotEntityInfo(bs->teammate, &entinfo);
        if (!EntityCarriesFlag(&entinfo)) {
            bs->ltgtype = 0;
        }
    }
    if (bs->neutralflagstatus == 1) {
        if (bs->owndecision_time < FloatTime()) {
            if (bs->ltgtype != LTG_TEAMACCOMPANY) {
                c = BotTeamFlagCarrierVisible(bs);
                if (c >= 0) {
                    BotRefuseOrder(bs);
                    bs->decisionmaker = bs->client;
                    bs->ordered = qfalse;
                    bs->teammate = c;
                    bs->teammatevisible_time = FloatTime();
                    bs->teammessage_time = 0;
                    bs->arrive_time = 1;
                    BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
                    bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
                    bs->ltgtype = LTG_TEAMACCOMPANY;
                    bs->formation_dist = 3.5 * 32;
                    BotSetTeamStatus(bs);
                    bs->owndecision_time = FloatTime() + 5;
                    return;
                }
            }
            if (bs->ltgtype == LTG_TEAMHELP ||
                bs->ltgtype == LTG_TEAMACCOMPANY ||
                bs->ltgtype == LTG_DEFENDKEYAREA ||
                bs->ltgtype == LTG_GETFLAG ||
                bs->ltgtype == LTG_RUSHBASE ||
                bs->ltgtype == LTG_CAMPORDER ||
                bs->ltgtype == LTG_PATROL ||
                bs->ltgtype == LTG_ATTACKENEMYBASE ||
                bs->ltgtype == LTG_GETITEM ||
                bs->ltgtype == LTG_MAKELOVE_UNDER ||
                bs->ltgtype == LTG_MAKELOVE_ONTOP) {
                return;
            }
            if (bs->ltgtype != LTG_ATTACKENEMYBASE) {
                BotRefuseOrder(bs);
                bs->decisionmaker = bs->client;
                bs->ordered = qfalse;
                if (BotTeam(bs) == TEAM_RED)
                    memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
                else
                    memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
                bs->ltgtype = LTG_ATTACKENEMYBASE;
                bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
                BotSetTeamStatus(bs);
                bs->owndecision_time = FloatTime() + 5;
            }
        }
        return;
    }
    else if (bs->neutralflagstatus == 2) {
        if (bs->owndecision_time < FloatTime()) {
            c = BotEnemyFlagCarrierVisible(bs);
            if (c >= 0) {
            }
            if (bs->ltgtype == LTG_TEAMHELP ||
                bs->ltgtype == LTG_TEAMACCOMPANY ||
                bs->ltgtype == LTG_CAMPORDER ||
                bs->ltgtype == LTG_PATROL ||
                bs->ltgtype == LTG_GETITEM) {
                return;
            }
            if (bs->ltgtype != LTG_DEFENDKEYAREA) {
                BotRefuseOrder(bs);
                bs->decisionmaker = bs->client;
                bs->ordered = qfalse;
                if (BotTeam(bs) == TEAM_RED)
                    memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
                else
                    memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
                bs->ltgtype = LTG_DEFENDKEYAREA;
                bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
                bs->defendaway_time = 0;
                BotSetTeamStatus(bs);
                bs->owndecision_time = FloatTime() + 5;
            }
        }
        return;
    }
    if (BotTeamLeader(bs)) {
        return;
    }
    if (bs->lastgoal_ltgtype) {
        bs->teamgoal_time += 60;
    }
    if (!bs->ordered && bs->lastgoal_ltgtype) {
        bs->ltgtype = 0;
    }
    if (bs->ltgtype == LTG_TEAMHELP ||
        bs->ltgtype == LTG_TEAMACCOMPANY ||
        bs->ltgtype == LTG_DEFENDKEYAREA ||
        bs->ltgtype == LTG_GETFLAG ||
        bs->ltgtype == LTG_RUSHBASE ||
        bs->ltgtype == LTG_RETURNFLAG ||
        bs->ltgtype == LTG_CAMPORDER ||
        bs->ltgtype == LTG_PATROL ||
        bs->ltgtype == LTG_ATTACKENEMYBASE ||
        bs->ltgtype == LTG_GETITEM ||
        bs->ltgtype == LTG_MAKELOVE_UNDER ||
        bs->ltgtype == LTG_MAKELOVE_ONTOP) {
        return;
    }
    if (BotSetLastOrderedTask(bs))
        return;
    if (bs->owndecision_time > FloatTime())
        return;
    ;
    if (bs->ctfroam_time > FloatTime())
        return;
    if (BotAggression(bs) < 50)
        return;
    bs->teammessage_time = FloatTime() + 2 * random();
    if (bs->teamtaskpreference & (TEAMTP_ATTACKER | TEAMTP_DEFENDER)) {
        if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
            l1 = 0.7f;
        } else {
            l1 = 0.2f;
        }
        l2 = 0.9f;
    } else {
        l1 = 0.4f;
        l2 = 0.7f;
    }
    rnd = random();
    if (rnd < l1 && ctf_neutralflag.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        bs->ltgtype = LTG_GETFLAG;
        bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
        BotSetTeamStatus(bs);
    } else if (rnd < l2 && ctf_redflag.areanum && ctf_blueflag.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        if (BotTeam(bs) == TEAM_RED)
            memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
        else
            memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
        bs->ltgtype = LTG_DEFENDKEYAREA;
        bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
        bs->defendaway_time = 0;
        BotSetTeamStatus(bs);
    } else {
        bs->ltgtype = 0;
        bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
        BotSetTeamStatus(bs);
    }
    bs->owndecision_time = FloatTime() + 5;
#ifdef DEBUG
    BotPrintTeamGoal(bs);
#endif
}

void Bot1FCTFRetreatGoals(bot_state_t* bs) {
    if (Bot1FCTFCarryingFlag(bs)) {
        if (bs->ltgtype != LTG_RUSHBASE) {
            BotRefuseOrder(bs);
            bs->ltgtype = LTG_RUSHBASE;
            bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
            bs->rushbaseaway_time = 0;
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
            BotSetTeamStatus(bs);
        }
    }
}

void BotObeliskSeekGoals(bot_state_t* bs) {
    float rnd, l1, l2;

    if (BotTeamLeader(bs)) {
        return;
    }
    if (bs->lastgoal_ltgtype) {
        bs->teamgoal_time += 60;
    }
    if (bs->ltgtype == LTG_TEAMHELP ||
        bs->ltgtype == LTG_TEAMACCOMPANY ||
        bs->ltgtype == LTG_DEFENDKEYAREA ||
        bs->ltgtype == LTG_GETFLAG ||
        bs->ltgtype == LTG_RUSHBASE ||
        bs->ltgtype == LTG_RETURNFLAG ||
        bs->ltgtype == LTG_CAMPORDER ||
        bs->ltgtype == LTG_PATROL ||
        bs->ltgtype == LTG_ATTACKENEMYBASE ||
        bs->ltgtype == LTG_GETITEM ||
        bs->ltgtype == LTG_MAKELOVE_UNDER ||
        bs->ltgtype == LTG_MAKELOVE_ONTOP) {
        return;
    }
    if (BotSetLastOrderedTask(bs))
        return;
    if (bs->ctfroam_time > FloatTime())
        return;
    if (BotAggression(bs) < 50)
        return;
    bs->teammessage_time = FloatTime() + 2 * random();
    if (bs->teamtaskpreference & (TEAMTP_ATTACKER | TEAMTP_DEFENDER)) {
        if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
            l1 = 0.7f;
        } else {
            l1 = 0.2f;
        }
        l2 = 0.9f;
    } else {
        l1 = 0.4f;
        l2 = 0.7f;
    }
    rnd = random();
    if (rnd < l1 && redobelisk.areanum && blueobelisk.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        if (BotTeam(bs) == TEAM_RED)
            memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
        else
            memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
        bs->ltgtype = LTG_ATTACKENEMYBASE;
        bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
        BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
        BotSetTeamStatus(bs);
    } else if (rnd < l2 && redobelisk.areanum && blueobelisk.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        if (BotTeam(bs) == TEAM_RED)
            memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
        else
            memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
        bs->ltgtype = LTG_DEFENDKEYAREA;
        bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
        bs->defendaway_time = 0;
        BotSetTeamStatus(bs);
    } else {
        bs->ltgtype = 0;
        bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
        BotSetTeamStatus(bs);
    }
}

void BotGoHarvest(bot_state_t* bs) {
    if (BotTeam(bs) == TEAM_RED)
        memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
    else
        memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
    bs->ltgtype = LTG_HARVEST;
    bs->teamgoal_time = FloatTime() + TEAM_HARVEST_TIME;
    bs->harvestaway_time = 0;
    BotSetTeamStatus(bs);
}

void BotObeliskRetreatGoals(bot_state_t* bs) {
}

void BotHarvesterSeekGoals(bot_state_t* bs) {
    aas_entityinfo_t entinfo;
    float rnd, l1, l2;
    int c;

    if (BotHarvesterCarryingCubes(bs)) {
        if (bs->ltgtype != LTG_RUSHBASE) {
            BotRefuseOrder(bs);
            bs->ltgtype = LTG_RUSHBASE;
            bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
            bs->rushbaseaway_time = 0;
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
            BotSetTeamStatus(bs);
        }
        return;
    }
    if (BotTeamLeader(bs)) {
        return;
    }
    if (bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered) {
        BotEntityInfo(bs->teammate, &entinfo);
        if (!EntityCarriesCubes(&entinfo)) {
            bs->ltgtype = 0;
        }
    }
    if (bs->lastgoal_ltgtype) {
        bs->teamgoal_time += 60;
    }
    if (bs->ltgtype == LTG_TEAMHELP ||
        bs->ltgtype == LTG_TEAMACCOMPANY ||
        bs->ltgtype == LTG_DEFENDKEYAREA ||
        bs->ltgtype == LTG_GETFLAG ||
        bs->ltgtype == LTG_CAMPORDER ||
        bs->ltgtype == LTG_PATROL ||
        bs->ltgtype == LTG_ATTACKENEMYBASE ||
        bs->ltgtype == LTG_HARVEST ||
        bs->ltgtype == LTG_GETITEM ||
        bs->ltgtype == LTG_MAKELOVE_UNDER ||
        bs->ltgtype == LTG_MAKELOVE_ONTOP) {
        return;
    }
    if (BotSetLastOrderedTask(bs))
        return;
    if (bs->ctfroam_time > FloatTime())
        return;
    if (BotAggression(bs) < 50)
        return;
    bs->teammessage_time = FloatTime() + 2 * random();
    c = BotEnemyCubeCarrierVisible(bs);
    if (c >= 0) {
    }
    if (bs->ltgtype != LTG_TEAMACCOMPANY) {
        c = BotTeamCubeCarrierVisible(bs);
        if (c >= 0) {
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            bs->teammate = c;
            bs->teammatevisible_time = FloatTime();
            bs->teammessage_time = 0;
            bs->arrive_time = 1;
            BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
            bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
            bs->ltgtype = LTG_TEAMACCOMPANY;
            bs->formation_dist = 3.5 * 32;
            BotSetTeamStatus(bs);
            return;
        }
    }
    if (bs->teamtaskpreference & (TEAMTP_ATTACKER | TEAMTP_DEFENDER)) {
        if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
            l1 = 0.7f;
        } else {
            l1 = 0.2f;
        }
        l2 = 0.9f;
    } else {
        l1 = 0.4f;
        l2 = 0.7f;
    }
    rnd = random();
    if (rnd < l1 && redobelisk.areanum && blueobelisk.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        BotGoHarvest(bs);
    } else if (rnd < l2 && redobelisk.areanum && blueobelisk.areanum) {
        bs->decisionmaker = bs->client;
        bs->ordered = qfalse;
        if (BotTeam(bs) == TEAM_RED)
            memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
        else
            memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
        bs->ltgtype = LTG_DEFENDKEYAREA;
        bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
        bs->defendaway_time = 0;
        BotSetTeamStatus(bs);
    } else {
        bs->ltgtype = 0;
        bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
        BotSetTeamStatus(bs);
    }
}

void BotHarvesterRetreatGoals(bot_state_t* bs) {
    if (BotHarvesterCarryingCubes(bs)) {
        if (bs->ltgtype != LTG_RUSHBASE) {
            BotRefuseOrder(bs);
            bs->ltgtype = LTG_RUSHBASE;
            bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
            bs->rushbaseaway_time = 0;
            bs->decisionmaker = bs->client;
            bs->ordered = qfalse;
            BotSetTeamStatus(bs);
        }
        return;
    }
}

void BotTeamGoals(bot_state_t* bs, int retreat) {
    if (retreat) {
        if (gametype == GT_CTF || gametype == GT_AD) {
            BotCTFRetreatGoals(bs);
        } else if (gametype == GT_1FCTF) {
            Bot1FCTFRetreatGoals(bs);
        } else if (gametype == GT_OBELISK) {
            BotObeliskRetreatGoals(bs);
        } else if (gametype == GT_HARVESTER) {
            BotHarvesterRetreatGoals(bs);
        }
    } else {
        if (gametype == GT_CTF || gametype == GT_AD) {
            BotCTFSeekGoals(bs);
        } else if (gametype == GT_1FCTF) {
            Bot1FCTFSeekGoals(bs);
        } else if (gametype == GT_OBELISK) {
            BotObeliskSeekGoals(bs);
        } else if (gametype == GT_HARVESTER) {
            BotHarvesterSeekGoals(bs);
        }
    }
    bs->order_time = 0;
}

int BotPointAreaNum(vec3_t origin) {
    int areanum, numareas, areas[10];
    vec3_t end;

    areanum = trap_AAS_PointAreaNum(origin);
    if (areanum)
        return areanum;
    VectorCopy(origin, end);
    end[2] += 10;
    numareas = trap_AAS_TraceAreas(origin, end, areas, NULL, 10);
    if (numareas > 0)
        return areas[0];
    return 0;
}

char* ClientName(int client, char* name, int size) {
    char buf[MAX_INFO_STRING];

    if (client < 0 || client >= MAX_CLIENTS) {
        BotAI_Print(PRT_ERROR, "ClientName: client out of range\n");
        return "[client out of range]";
    }
    trap_GetConfigstring(CS_PLAYERS + client, buf, sizeof(buf));
    strncpy(name, Info_ValueForKey(buf, "n"), size - 1);
    name[size - 1] = '\0';
    Q_CleanStr(name);
    return name;
}

char* ClientSkin(int client, char* skin, int size) {
    char buf[MAX_INFO_STRING];

    if (client < 0 || client >= MAX_CLIENTS) {
        BotAI_Print(PRT_ERROR, "ClientSkin: client out of range\n");
        return "[client out of range]";
    }
    trap_GetConfigstring(CS_PLAYERS + client, buf, sizeof(buf));
    strncpy(skin, Info_ValueForKey(buf, "model"), size - 1);
    skin[size - 1] = '\0';
    return skin;
}

int ClientFromName(char* name) {
    int i;
    char buf[MAX_INFO_STRING];

    for (i = 0; i < level.maxclients; i++) {
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        Q_CleanStr(buf);
        if (!Q_stricmp(Info_ValueForKey(buf, "n"), name))
            return i;
    }
    return -1;
}

int ClientOnSameTeamFromName(bot_state_t* bs, char* name) {
    int i;
    char buf[MAX_INFO_STRING];

    for (i = 0; i < level.maxclients; i++) {
        if (!BotSameTeam(bs, i))
            continue;
        trap_GetConfigstring(CS_PLAYERS + i, buf, sizeof(buf));
        Q_CleanStr(buf);
        if (!Q_stricmp(Info_ValueForKey(buf, "n"), name))
            return i;
    }
    return -1;
}

char* stristr(char* str, char* charset) {
    int i;

    while (*str) {
        for (i = 0; charset[i] && str[i]; i++) {
            if (toupper(charset[i]) != toupper(str[i]))
                break;
        }
        if (!charset[i])
            return str;
        str++;
    }
    return NULL;
}

char* EasyClientName(int client, char* buf, int size) {
    int i;
    char *str1, *str2, *ptr, c;
    char name[128] = {0};

    ClientName(client, name, sizeof(name));

    for (i = 0; name[i]; i++)
        name[i] &= 127;
    for (ptr = strstr(name, " "); ptr; ptr = strstr(name, " ")) {
        memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
    }
    str1 = strstr(name, "[");
    str2 = strstr(name, "]");
    if (str1 && str2) {
        if (str2 > str1)
            memmove(str1, str2 + 1, strlen(str2 + 1) + 1);
        else
            memmove(str2, str1 + 1, strlen(str1 + 1) + 1);
    }
    if ((name[0] == 'm' || name[0] == 'M') &&
        (name[1] == 'r' || name[1] == 'R')) {
        memmove(name, name + 2, strlen(name + 2) + 1);
    }
    ptr = name;
    while (*ptr) {
        c = *ptr;
        if ((c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_') {
            ptr++;
        } else if (c >= 'A' && c <= 'Z') {
            *ptr += 'a' - 'A';
            ptr++;
        } else {
            memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
        }
    }
    strncpy(buf, name, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

int BotSynonymContext(bot_state_t* bs) {
    int context;

    context = CONTEXT_NORMAL | CONTEXT_NEARBYITEM | CONTEXT_NAMES;
    if (gametype == GT_CTF || gametype == GT_1FCTF || gametype == GT_AD) {
        if (BotTeam(bs) == TEAM_RED)
            context |= CONTEXT_CTFREDTEAM;
        else
            context |= CONTEXT_CTFBLUETEAM;
    } else if (gametype == GT_OBELISK) {
        if (BotTeam(bs) == TEAM_RED)
            context |= CONTEXT_OBELISKREDTEAM;
        else
            context |= CONTEXT_OBELISKBLUETEAM;
    } else if (gametype == GT_HARVESTER) {
        if (BotTeam(bs) == TEAM_RED)
            context |= CONTEXT_HARVESTERREDTEAM;
        else
            context |= CONTEXT_HARVESTERBLUETEAM;
    }
    return context;
}

void BotChooseWeapon(bot_state_t* bs) {
    int newweaponnum;

    if (bs->cur_ps.weaponstate == WEAPON_RAISING ||
        bs->cur_ps.weaponstate == WEAPON_DROPPING) {
        trap_EA_SelectWeapon(bs->client, bs->weaponnum);
    } else if (bot_gauntlet.integer) {
        bs->weaponnum = WP_GAUNTLET;
        trap_EA_SelectWeapon(bs->client, bs->weaponnum);
    } else {
        newweaponnum = trap_BotChooseBestFightWeapon(bs->ws, bs->inventory);
        if (bs->weaponnum != newweaponnum)
            bs->weaponchange_time = FloatTime();
        bs->weaponnum = newweaponnum;
        trap_EA_SelectWeapon(bs->client, bs->weaponnum);
    }
}

void BotSetupForMovement(bot_state_t* bs) {
    bot_initmove_t initmove;

    memset(&initmove, 0, sizeof(bot_initmove_t));
    VectorCopy(bs->cur_ps.origin, initmove.origin);
    VectorCopy(bs->cur_ps.velocity, initmove.velocity);
    VectorClear(initmove.viewoffset);
    initmove.viewoffset[2] += bs->cur_ps.viewheight;
    initmove.entitynum = bs->entitynum;
    initmove.client = bs->client;
    initmove.thinktime = bs->thinktime;
    if (bs->cur_ps.groundEntityNum != ENTITYNUM_NONE)
        initmove.or_moveflags |= MFL_ONGROUND;
    if ((bs->cur_ps.pm_flags & PMF_TIME_KNOCKBACK) && (bs->cur_ps.pm_time > 0)) {
        initmove.or_moveflags |= MFL_TELEPORTED;
    }
    if ((bs->cur_ps.pm_flags & PMF_TIME_WATERJUMP) && (bs->cur_ps.pm_time > 0)) {
        initmove.or_moveflags |= MFL_WATERJUMP;
    }
    if (bs->cur_ps.pm_flags & PMF_DUCKED)
        initmove.presencetype = PRESENCE_CROUCH;
    else
        initmove.presencetype = PRESENCE_NORMAL;
    if (bs->walker > 0.5)
        initmove.or_moveflags |= MFL_WALK;
    VectorCopy(bs->viewangles, initmove.viewangles);
    trap_BotInitMoveState(bs->ms, &initmove);
}

void BotCheckItemPickup(bot_state_t* bs, int* oldinventory) {
    int offence, leader;

    if (gametype <= GT_TEAM)
        return;

    offence = -1;
    if (!oldinventory[INVENTORY_KAMIKAZE] && bs->inventory[INVENTORY_KAMIKAZE] >= 1) {
        offence = qtrue;
    }
    if (!oldinventory[INVENTORY_INVULNERABILITY] && bs->inventory[INVENTORY_INVULNERABILITY] >= 1) {
        offence = qtrue;
    }
    if (!bs->inventory[INVENTORY_KAMIKAZE] && !bs->inventory[INVENTORY_INVULNERABILITY]) {
        if (!oldinventory[INVENTORY_SCOUT] && bs->inventory[INVENTORY_SCOUT] >= 1) {
            offence = qtrue;
        }
        if (!oldinventory[INVENTORY_GUARD] && bs->inventory[INVENTORY_GUARD] >= 1) {
            offence = qtrue;
        }
        if (!oldinventory[INVENTORY_DOUBLER] && bs->inventory[INVENTORY_DOUBLER] >= 1) {
            offence = qfalse;
        }
        if (!oldinventory[INVENTORY_AMMOREGEN] && bs->inventory[INVENTORY_AMMOREGEN] >= 1) {
            offence = qfalse;
        }
    }

    if (offence >= 0) {
        leader = ClientFromName(bs->teamleader);
        if (offence) {
            if (!(bs->teamtaskpreference & TEAMTP_ATTACKER)) {
                if (BotTeamLeader(bs)) {
                    BotVoiceChat(bs, leader, VOICECHAT_WANTONOFFENSE);
                } else if (g_spSkill.integer <= 3) {
                    if (bs->ltgtype != LTG_GETFLAG &&
                        bs->ltgtype != LTG_ATTACKENEMYBASE &&
                        bs->ltgtype != LTG_HARVEST) {
                        if ((gametype != GT_CTF || (bs->redflagstatus == 0 && bs->blueflagstatus == 0)) &&
                            (gametype != GT_1FCTF || bs->neutralflagstatus == 0)) {
                            BotVoiceChat(bs, leader, VOICECHAT_WANTONOFFENSE);
                        }
                    }
                    bs->teamtaskpreference |= TEAMTP_ATTACKER;
                }
            }
            bs->teamtaskpreference &= ~TEAMTP_DEFENDER;
        } else {
            if (!(bs->teamtaskpreference & TEAMTP_DEFENDER)) {
                if (BotTeamLeader(bs)) {
                    BotVoiceChat(bs, -1, VOICECHAT_WANTONDEFENSE);
                } else if (g_spSkill.integer <= 3) {
                    if (bs->ltgtype != LTG_DEFENDKEYAREA) {
                        if ((gametype != GT_CTF || (bs->redflagstatus == 0 && bs->blueflagstatus == 0)) &&
                            (gametype != GT_1FCTF || bs->neutralflagstatus == 0)) {
                            BotVoiceChat(bs, -1, VOICECHAT_WANTONDEFENSE);
                        }
                    }
                }
                bs->teamtaskpreference |= TEAMTP_DEFENDER;
            }
            bs->teamtaskpreference &= ~TEAMTP_ATTACKER;
        }
    }
}

void BotUpdateInventory(bot_state_t* bs) {
    int oldinventory[MAX_ITEMS];

    memcpy(oldinventory, bs->inventory, sizeof(oldinventory));
    bs->inventory[INVENTORY_ARMOR] = bs->cur_ps.stats[STAT_ARMOR];
    bs->inventory[INVENTORY_GAUNTLET] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_GAUNTLET)) != 0;
    bs->inventory[INVENTORY_SHOTGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_SHOTGUN)) != 0;
    bs->inventory[INVENTORY_MACHINEGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_MACHINEGUN)) != 0;
    bs->inventory[INVENTORY_GRENADELAUNCHER] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_GRENADE_LAUNCHER)) != 0;
    bs->inventory[INVENTORY_ROCKETLAUNCHER] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)) != 0;
    bs->inventory[INVENTORY_LIGHTNING] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_LIGHTNING)) != 0;
    bs->inventory[INVENTORY_RAILGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_RAILGUN)) != 0;
    bs->inventory[INVENTORY_PLASMAGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_PLASMAGUN)) != 0;
    bs->inventory[INVENTORY_BFG10K] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_BFG)) != 0;
    bs->inventory[INVENTORY_GRAPPLINGHOOK] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_GRAPPLING_HOOK)) != 0;
    bs->inventory[INVENTORY_NAILGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_NAILGUN)) != 0;
    ;
    bs->inventory[INVENTORY_PROXLAUNCHER] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_PROX_LAUNCHER)) != 0;
    ;
    bs->inventory[INVENTORY_CHAINGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_CHAINGUN)) != 0;
    ;
    bs->inventory[INVENTORY_SHELLS] = bs->cur_ps.ammo[WP_SHOTGUN];
    bs->inventory[INVENTORY_BULLETS] = bs->cur_ps.ammo[WP_MACHINEGUN];
    bs->inventory[INVENTORY_GRENADES] = bs->cur_ps.ammo[WP_GRENADE_LAUNCHER];
    bs->inventory[INVENTORY_CELLS] = bs->cur_ps.ammo[WP_PLASMAGUN];
    bs->inventory[INVENTORY_LIGHTNINGAMMO] = bs->cur_ps.ammo[WP_LIGHTNING];
    bs->inventory[INVENTORY_ROCKETS] = bs->cur_ps.ammo[WP_ROCKET_LAUNCHER];
    bs->inventory[INVENTORY_SLUGS] = bs->cur_ps.ammo[WP_RAILGUN];
    bs->inventory[INVENTORY_BFGAMMO] = bs->cur_ps.ammo[WP_BFG];
    bs->inventory[INVENTORY_NAILS] = bs->cur_ps.ammo[WP_NAILGUN];
    bs->inventory[INVENTORY_MINES] = bs->cur_ps.ammo[WP_PROX_LAUNCHER];
    bs->inventory[INVENTORY_BELT] = bs->cur_ps.ammo[WP_CHAINGUN];
    bs->inventory[INVENTORY_HEALTH] = bs->cur_ps.stats[STAT_HEALTH];
    bs->inventory[INVENTORY_TELEPORTER] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_TELEPORTER;
    bs->inventory[INVENTORY_MEDKIT] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_MEDKIT;
    bs->inventory[INVENTORY_KAMIKAZE] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_KAMIKAZE;
    bs->inventory[INVENTORY_PORTAL] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_PORTAL;
    bs->inventory[INVENTORY_INVULNERABILITY] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_INVULNERABILITY;
    bs->inventory[INVENTORY_QUAD] = bs->cur_ps.powerups[PW_QUAD] != 0;
    bs->inventory[INVENTORY_ENVIRONMENTSUIT] = bs->cur_ps.powerups[PW_BATTLESUIT] != 0;
    bs->inventory[INVENTORY_HASTE] = bs->cur_ps.powerups[PW_HASTE] != 0;
    bs->inventory[INVENTORY_INVISIBILITY] = bs->cur_ps.powerups[PW_INVIS] != 0;
    bs->inventory[INVENTORY_REGEN] = bs->cur_ps.powerups[PW_REGEN] != 0;
    bs->inventory[INVENTORY_FLIGHT] = bs->cur_ps.powerups[PW_FLIGHT] != 0;
    bs->inventory[INVENTORY_SCOUT] = bs->cur_ps.stats[STAT_RUNE] == 1;
    bs->inventory[INVENTORY_GUARD] = bs->cur_ps.stats[STAT_RUNE] == 2;
    bs->inventory[INVENTORY_DOUBLER] = bs->cur_ps.stats[STAT_RUNE] == 3;
    bs->inventory[INVENTORY_AMMOREGEN] = bs->cur_ps.stats[STAT_RUNE] == 4;
    bs->inventory[INVENTORY_REDFLAG] = bs->cur_ps.powerups[PW_REDFLAG] != 0;
    bs->inventory[INVENTORY_BLUEFLAG] = bs->cur_ps.powerups[PW_BLUEFLAG] != 0;
    bs->inventory[INVENTORY_NEUTRALFLAG] = bs->cur_ps.powerups[PW_NEUTRALFLAG] != 0;
    if (BotTeam(bs) == TEAM_RED) {
        bs->inventory[INVENTORY_REDCUBE] = bs->cur_ps.generic1 & 0x3f;
        bs->inventory[INVENTORY_BLUECUBE] = 0;
    } else {
        bs->inventory[INVENTORY_REDCUBE] = 0;
        bs->inventory[INVENTORY_BLUECUBE] = bs->cur_ps.generic1 & 0x3f;
    }
    BotCheckItemPickup(bs, oldinventory);
    {
        static const int ammoSlots[] = {
            INVENTORY_SHELLS, INVENTORY_BULLETS, INVENTORY_GRENADES, INVENTORY_CELLS,
            INVENTORY_LIGHTNINGAMMO, INVENTORY_ROCKETS, INVENTORY_SLUGS, INVENTORY_BFGAMMO,
            INVENTORY_NAILS, INVENTORY_MINES, INVENTORY_BELT
        };
        int a;
        for (a = 0; a < (int)ARRAY_LEN(ammoSlots); a++) {
            if (bs->inventory[ammoSlots[a]] == -1) {
                bs->inventory[ammoSlots[a]] = 999;
            }
        }
    }
}

void BotUpdateBattleInventory(bot_state_t* bs, int enemy) {
    vec3_t dir;
    aas_entityinfo_t entinfo;

    BotEntityInfo(enemy, &entinfo);
    VectorSubtract(entinfo.origin, bs->origin, dir);
    bs->inventory[ENEMY_HEIGHT] = (int)dir[2];
    dir[2] = 0;
    bs->inventory[ENEMY_HORIZONTAL_DIST] = (int)VectorLength(dir);
}

#define KAMIKAZE_DIST 1024

void BotUseKamikaze(bot_state_t* bs) {
    int c, teammates, enemies;
    aas_entityinfo_t entinfo;
    vec3_t dir, target;
    bot_goal_t* goal;
    bsp_trace_t trace;

    if (bs->inventory[INVENTORY_KAMIKAZE] <= 0)
        return;
    if (bs->kamikaze_time > FloatTime())
        return;
    bs->kamikaze_time = FloatTime() + 0.2;
    if (gametype == GT_CTF) {
        if (BotCTFCarryingFlag(bs))
            return;
        c = BotTeamFlagCarrierVisible(bs);
        if (c >= 0) {
            BotEntityInfo(c, &entinfo);
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
                return;
        }
        c = BotEnemyFlagCarrierVisible(bs);
        if (c >= 0) {
            BotEntityInfo(c, &entinfo);
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    } else if (gametype == GT_1FCTF) {
        if (Bot1FCTFCarryingFlag(bs))
            return;
        c = BotTeamFlagCarrierVisible(bs);
        if (c >= 0) {
            BotEntityInfo(c, &entinfo);
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
                return;
        }
        c = BotEnemyFlagCarrierVisible(bs);
        if (c >= 0) {
            BotEntityInfo(c, &entinfo);
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    } else if (gametype == GT_OBELISK) {
        switch (BotTeam(bs)) {
            case TEAM_RED:
                goal = &blueobelisk;
                break;
            default:
                goal = &redobelisk;
                break;
        }
        VectorCopy(goal->origin, target);
        target[2] += 1;
        VectorSubtract(bs->origin, target, dir);
        if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST * 0.9)) {
            BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
            if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    } else if (gametype == GT_HARVESTER) {
        if (BotHarvesterCarryingCubes(bs))
            return;
        c = BotTeamCubeCarrierVisible(bs);
        if (c >= 0) {
            BotEntityInfo(c, &entinfo);
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
                return;
        }
        c = BotEnemyCubeCarrierVisible(bs);
        if (c >= 0) {
            BotEntityInfo(c, &entinfo);
            VectorSubtract(entinfo.origin, bs->origin, dir);
            if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    }
    BotVisibleTeamMatesAndEnemies(bs, &teammates, &enemies, KAMIKAZE_DIST);
    if (enemies > 2 && enemies > teammates + 1) {
        trap_EA_Use(bs->client);
        return;
    }
}

void BotUseInvulnerability(bot_state_t* bs) {
    int c;
    vec3_t dir, target;
    bot_goal_t* goal;
    bsp_trace_t trace;

    if (bs->inventory[INVENTORY_INVULNERABILITY] <= 0)
        return;
    if (bs->invulnerability_time > FloatTime())
        return;
    bs->invulnerability_time = FloatTime() + 0.2;
    if (gametype == GT_CTF) {
        if (BotCTFCarryingFlag(bs))
            return;
        c = BotEnemyFlagCarrierVisible(bs);
        if (c >= 0)
            return;
        switch (BotTeam(bs)) {
            case TEAM_RED:
                goal = &ctf_blueflag;
                break;
            default:
                goal = &ctf_redflag;
                break;
        }
        VectorCopy(goal->origin, target);
        target[2] += 1;
        VectorSubtract(bs->origin, target, dir);
        if (VectorLengthSquared(dir) < Square(200)) {
            BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
            if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    } else if (gametype == GT_1FCTF) {
        if (Bot1FCTFCarryingFlag(bs))
            return;
        c = BotEnemyFlagCarrierVisible(bs);
        if (c >= 0)
            return;
        switch (BotTeam(bs)) {
            case TEAM_RED:
                goal = &ctf_blueflag;
                break;
            default:
                goal = &ctf_redflag;
                break;
        }
        VectorCopy(goal->origin, target);
        target[2] += 1;
        VectorSubtract(bs->origin, target, dir);
        if (VectorLengthSquared(dir) < Square(200)) {
            BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
            if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    } else if (gametype == GT_OBELISK) {
        switch (BotTeam(bs)) {
            case TEAM_RED:
                goal = &blueobelisk;
                break;
            default:
                goal = &redobelisk;
                break;
        }
        VectorCopy(goal->origin, target);
        target[2] += 1;
        VectorSubtract(bs->origin, target, dir);
        if (VectorLengthSquared(dir) < Square(300)) {
            BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
            if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    } else if (gametype == GT_HARVESTER) {
        if (BotHarvesterCarryingCubes(bs))
            return;
        c = BotEnemyCubeCarrierVisible(bs);
        if (c >= 0)
            return;
        switch (BotTeam(bs)) {
            case TEAM_RED:
                goal = &blueobelisk;
                break;
            default:
                goal = &redobelisk;
                break;
        }
        VectorCopy(goal->origin, target);
        target[2] += 1;
        VectorSubtract(bs->origin, target, dir);
        if (VectorLengthSquared(dir) < Square(200)) {
            BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
            if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
                trap_EA_Use(bs->client);
                return;
            }
        }
    }
}

void BotBattleUseItems(bot_state_t* bs) {
    if (bs->inventory[INVENTORY_HEALTH] < 40) {
        if (bs->inventory[INVENTORY_TELEPORTER] > 0) {
            if (!BotCTFCarryingFlag(bs) && !Bot1FCTFCarryingFlag(bs) && !BotHarvesterCarryingCubes(bs)) {
                trap_EA_Use(bs->client);
            }
        }
    }
    if (bs->inventory[INVENTORY_HEALTH] < 60) {
        if (bs->inventory[INVENTORY_MEDKIT] > 0) {
            trap_EA_Use(bs->client);
        }
    }
    BotUseKamikaze(bs);
    BotUseInvulnerability(bs);
}

void BotSetTeleportTime(bot_state_t* bs) {
    if ((bs->cur_ps.eFlags ^ bs->last_eFlags) & EF_TELEPORT_BIT) {
        bs->teleport_time = FloatTime();
    }
    bs->last_eFlags = bs->cur_ps.eFlags;
}

qboolean BotIsDead(bot_state_t* bs) {
    return (bs->cur_ps.pm_type == PM_DEAD);
}

qboolean BotIsObserver(bot_state_t* bs) {
    char buf[MAX_INFO_STRING];
    if (bs->cur_ps.pm_type == PM_SPECTATOR)
        return qtrue;
    trap_GetConfigstring(CS_PLAYERS + bs->client, buf, sizeof(buf));
    if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR)
        return qtrue;
    return qfalse;
}

qboolean BotIntermission(bot_state_t* bs) {
    if (level.intermissionTime)
        return qtrue;
    return (bs->cur_ps.pm_type == PM_FREEZE || bs->cur_ps.pm_type == PM_INTERMISSION);
}

qboolean BotInLavaOrSlime(bot_state_t* bs) {
    vec3_t feet;

    VectorCopy(bs->origin, feet);
    feet[2] -= 23;
    return (trap_AAS_PointContents(feet) & (CONTENTS_LAVA | CONTENTS_SLIME));
}

bot_waypoint_t* BotCreateWayPoint(char* name, vec3_t origin, int areanum) {
    bot_waypoint_t* wp;
    vec3_t waypointmins = {-8, -8, -8}, waypointmaxs = {8, 8, 8};

    wp = botai_freewaypoints;
    if (!wp) {
        BotAI_Print(PRT_WARNING, "BotCreateWayPoint: Out of waypoints\n");
        return NULL;
    }
    botai_freewaypoints = botai_freewaypoints->next;

    Q_strncpyz(wp->name, name, sizeof(wp->name));
    VectorCopy(origin, wp->goal.origin);
    VectorCopy(waypointmins, wp->goal.mins);
    VectorCopy(waypointmaxs, wp->goal.maxs);
    wp->goal.areanum = areanum;
    wp->next = NULL;
    wp->prev = NULL;
    return wp;
}

bot_waypoint_t* BotFindWayPoint(bot_waypoint_t* waypoints, char* name) {
    bot_waypoint_t* wp;

    for (wp = waypoints; wp; wp = wp->next) {
        if (!Q_stricmp(wp->name, name))
            return wp;
    }
    return NULL;
}

void BotFreeWaypoints(bot_waypoint_t* wp) {
    bot_waypoint_t* nextwp;

    for (; wp; wp = nextwp) {
        nextwp = wp->next;
        wp->next = botai_freewaypoints;
        botai_freewaypoints = wp;
    }
}

void BotInitWaypoints(void) {
    int i;

    botai_freewaypoints = NULL;
    for (i = 0; i < MAX_WAYPOINTS; i++) {
        botai_waypoints[i].next = botai_freewaypoints;
        botai_freewaypoints = &botai_waypoints[i];
    }
}

int TeamPlayIsOn(void) {
    return (gametype >= GT_TEAM);
}

float BotAggression(bot_state_t* bs) {
    if (bot_gauntlet.integer)
        return 100;
    if (bs->inventory[INVENTORY_QUAD]) {
        if (bs->weaponnum != WP_GAUNTLET ||
            bs->inventory[ENEMY_HORIZONTAL_DIST] < 80) {
            return 70;
        }
    }
    if (bs->inventory[ENEMY_HEIGHT] > 200)
        return 0;
    if (bs->inventory[INVENTORY_HEALTH] < 60)
        return 0;
    if (bs->inventory[INVENTORY_HEALTH] < 80) {
        if (bs->inventory[INVENTORY_ARMOR] < 40)
            return 0;
    }
    if (bs->inventory[INVENTORY_BFG10K] > 0 &&
        (bs->inventory[INVENTORY_BFGAMMO] > 7 || bs->inventory[INVENTORY_BFGAMMO] == -1))
        return 100;
    if (bs->inventory[INVENTORY_RAILGUN] > 0 &&
        (bs->inventory[INVENTORY_SLUGS] > 5 || bs->inventory[INVENTORY_SLUGS] == -1))
        return 95;
    if (bs->inventory[INVENTORY_LIGHTNING] > 0 &&
        (bs->inventory[INVENTORY_LIGHTNINGAMMO] > 50 || bs->inventory[INVENTORY_LIGHTNINGAMMO] == -1))
        return 90;
    if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 &&
        (bs->inventory[INVENTORY_ROCKETS] > 5 || bs->inventory[INVENTORY_ROCKETS] == -1))
        return 90;
    if (bs->inventory[INVENTORY_PLASMAGUN] > 0 &&
        (bs->inventory[INVENTORY_CELLS] > 40 || bs->inventory[INVENTORY_CELLS] == -1))
        return 85;
    if (bs->inventory[INVENTORY_GRENADELAUNCHER] > 0 &&
        (bs->inventory[INVENTORY_GRENADES] > 10 || bs->inventory[INVENTORY_GRENADES] == -1))
        return 80;
    if (bs->inventory[INVENTORY_SHOTGUN] > 0 &&
        (bs->inventory[INVENTORY_SHELLS] > 10 || bs->inventory[INVENTORY_SHELLS] == -1))
        return 50;
    return 0;
}

float BotFeelingBad(bot_state_t* bs) {
    if (bs->weaponnum == WP_GAUNTLET && !bot_gauntlet.integer) {
        return 100;
    }
    if (bs->inventory[INVENTORY_HEALTH] < 40) {
        return 100;
    }
    if (bs->weaponnum == WP_MACHINEGUN) {
        return 90;
    }
    if (bs->inventory[INVENTORY_HEALTH] < 60) {
        return 80;
    }
    return 0;
}

int BotWantsToRetreat(bot_state_t* bs) {
    aas_entityinfo_t entinfo;

    if (bot_gauntlet.integer && BotFeelingBad(bs) > 50)
        return qfalse;
    if (gametype == GT_CTF) {
        if (BotCTFCarryingFlag(bs))
            return qtrue;
    } else if (gametype == GT_1FCTF) {
        if (Bot1FCTFCarryingFlag(bs))
            return qtrue;
    } else if (gametype == GT_OBELISK) {
        if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
            if (bs->enemy != redobelisk.entitynum ||
                bs->enemy != blueobelisk.entitynum) {
                return qtrue;
            }
        }
        if (BotFeelingBad(bs) > 50) {
            return qtrue;
        }
        return qfalse;
    } else if (gametype == GT_HARVESTER) {
        if (BotHarvesterCarryingCubes(bs))
            return qtrue;
    }
    if (bs->enemy >= 0) {
        BotEntityInfo(bs->enemy, &entinfo);
        if (EntityCarriesFlag(&entinfo))
            return qfalse;
    }
    if (bs->ltgtype == LTG_GETFLAG)
        return qtrue;
    if (BotAggression(bs) < 50)
        return qtrue;
    return qfalse;
}

int BotWantsToChase(bot_state_t* bs) {
    aas_entityinfo_t entinfo;

    if (gametype == GT_CTF) {
        if (BotCTFCarryingFlag(bs))
            return qfalse;
        if (bs->enemy > -1) {
            BotEntityInfo(bs->enemy, &entinfo);
            if (EntityCarriesFlag(&entinfo))
                return qtrue;
        }
    } else if (gametype == GT_1FCTF) {
        if (Bot1FCTFCarryingFlag(bs))
            return qfalse;
        if (bs->enemy > -1) {
            BotEntityInfo(bs->enemy, &entinfo);
            if (EntityCarriesFlag(&entinfo))
                return qtrue;
        }
    } else if (gametype == GT_OBELISK) {
        if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
            if (bs->enemy != redobelisk.entitynum ||
                bs->enemy != blueobelisk.entitynum) {
                return qfalse;
            }
        }
    } else if (gametype == GT_HARVESTER) {
        if (BotHarvesterCarryingCubes(bs))
            return qfalse;
    }
    if (bs->ltgtype == LTG_GETFLAG)
        return qfalse;
    if (BotAggression(bs) > 50)
        return qtrue;
    return qfalse;
}

int BotWantsToHelp(bot_state_t* bs) {
    return qtrue;
}

int BotCanAndWantsToRocketJump(bot_state_t* bs) {
    float rocketjumper;

    if (bot_gauntlet.integer || bot_gauntletOnly.integer)
        return qfalse;
    if (!bot_rocketjump.integer)
        return qfalse;
    if (bs->inventory[INVENTORY_ROCKETLAUNCHER] <= 0)
        return qfalse;
    if (bs->inventory[INVENTORY_ROCKETS] < 3)
        bs->inventory[INVENTORY_ROCKETS] = 0;
    if (bs->inventory[INVENTORY_QUAD])
        return qfalse;
    if (bs->inventory[INVENTORY_HEALTH] < 60)
        return qfalse;
    if (bs->inventory[INVENTORY_HEALTH] < 90) {
        if (bs->inventory[INVENTORY_ARMOR] < 40)
            return qfalse;
    }
    rocketjumper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_WEAPONJUMPING, 0, 1);
    if (rocketjumper < 0.5)
        return qfalse;
    return qtrue;
}

int BotHasPersistantPowerupAndWeapon(bot_state_t* bs) {
    if (!bs->inventory[INVENTORY_SCOUT] &&
        !bs->inventory[INVENTORY_GUARD] &&
        !bs->inventory[INVENTORY_DOUBLER] &&
        !bs->inventory[INVENTORY_AMMOREGEN]) {
        return qfalse;
    }
    if (bs->inventory[INVENTORY_HEALTH] < 60)
        return qfalse;
    if (bs->inventory[INVENTORY_HEALTH] < 80) {
        if (bs->inventory[INVENTORY_ARMOR] < 40)
            return qfalse;
    }
    if (bs->inventory[INVENTORY_BFG10K] > 0 &&
        bs->inventory[INVENTORY_BFGAMMO] > 7)
        return qtrue;
    if (bs->inventory[INVENTORY_RAILGUN] > 0 &&
        bs->inventory[INVENTORY_SLUGS] > 5)
        return qtrue;
    if (bs->inventory[INVENTORY_LIGHTNING] > 0 &&
        bs->inventory[INVENTORY_LIGHTNINGAMMO] > 50)
        return qtrue;
    if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 &&
        bs->inventory[INVENTORY_ROCKETS] > 5)
        return qtrue;
    if (bs->inventory[INVENTORY_NAILGUN] > 0 &&
        bs->inventory[INVENTORY_NAILS] > 5)
        return qtrue;
    if (bs->inventory[INVENTORY_PROXLAUNCHER] > 0 &&
        bs->inventory[INVENTORY_MINES] > 5)
        return qtrue;
    if (bs->inventory[INVENTORY_CHAINGUN] > 0 &&
        bs->inventory[INVENTORY_BELT] > 40)
        return qtrue;
    if (bs->inventory[INVENTORY_PLASMAGUN] > 0 &&
        bs->inventory[INVENTORY_CELLS] > 20)
        return qtrue;
    return qfalse;
}

void BotGoCamp(bot_state_t* bs, bot_goal_t* goal) {
    float camper;

    bs->decisionmaker = bs->client;
    bs->teammessage_time = 0;
    bs->ltgtype = LTG_CAMP;
    memcpy(&bs->teamgoal, goal, sizeof(bot_goal_t));
    camper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
    if (camper > 0.99)
        bs->teamgoal_time = FloatTime() + 99999;
    else
        bs->teamgoal_time = FloatTime() + 120 + 180 * camper + random() * 15;
    bs->camp_time = FloatTime();
    bs->teammate = 0;
    bs->arrive_time = 1;
}

int BotWantsToCamp(bot_state_t* bs) {
    float camper;
    int cs, traveltime, besttraveltime;
    bot_goal_t goal, bestgoal;

    camper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
    if (camper < 0.1)
        return qfalse;
    if (bs->ltgtype == LTG_TEAMHELP ||
        bs->ltgtype == LTG_TEAMACCOMPANY ||
        bs->ltgtype == LTG_DEFENDKEYAREA ||
        bs->ltgtype == LTG_GETFLAG ||
        bs->ltgtype == LTG_RUSHBASE ||
        bs->ltgtype == LTG_CAMP ||
        bs->ltgtype == LTG_CAMPORDER ||
        bs->ltgtype == LTG_PATROL) {
        return qfalse;
    }
    if (bs->camp_time > FloatTime() - 60 + 300 * (1 - camper))
        return qfalse;
    if (random() > camper) {
        bs->camp_time = FloatTime();
        return qfalse;
    }
    if (BotAggression(bs) < 50)
        return qfalse;
    if ((bs->inventory[INVENTORY_ROCKETLAUNCHER] <= 0 || bs->inventory[INVENTORY_ROCKETS] < 10) &&
        (bs->inventory[INVENTORY_RAILGUN] <= 0 || bs->inventory[INVENTORY_SLUGS] < 10) &&
        (bs->inventory[INVENTORY_BFG10K] <= 0 || bs->inventory[INVENTORY_BFGAMMO] < 10)) {
        return qfalse;
    }
    besttraveltime = 99999;
    for (cs = trap_BotGetNextCampSpotGoal(0, &goal); cs; cs = trap_BotGetNextCampSpotGoal(cs, &goal)) {
        traveltime = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, goal.areanum, TFL_DEFAULT);
        if (traveltime && traveltime < besttraveltime) {
            besttraveltime = traveltime;
            memcpy(&bestgoal, &goal, sizeof(bot_goal_t));
        }
    }
    if (besttraveltime > 150)
        return qfalse;
    BotGoCamp(bs, &bestgoal);
    bs->ordered = qfalse;
    return qtrue;
}

void BotDontAvoid(bot_state_t* bs, char* itemname) {
    bot_goal_t goal;
    int num;

    num = trap_BotGetLevelItemGoal(-1, itemname, &goal);
    while (num >= 0) {
        trap_BotRemoveFromAvoidGoals(bs->gs, goal.number);
        num = trap_BotGetLevelItemGoal(num, itemname, &goal);
    }
}

void BotGoForPowerups(bot_state_t* bs) {
    BotDontAvoid(bs, "Quad Damage");
    BotDontAvoid(bs, "Regeneration");
    BotDontAvoid(bs, "Battle Suit");
    BotDontAvoid(bs, "Speed");
    BotDontAvoid(bs, "Invisibility");
    bs->ltg_time = 0;
}

void BotRoamGoal(bot_state_t* bs, vec3_t goal) {
    int pc, i;
    float len, rnd;
    vec3_t dir, bestorg, belowbestorg;
    bsp_trace_t trace;

    for (i = 0; i < 10; i++) {
        VectorCopy(bs->origin, bestorg);
        rnd = random();
        if (rnd > 0.25) {
            if (random() < 0.5)
                bestorg[0] -= 800 * random() + 100;
            else
                bestorg[0] += 800 * random() + 100;
        }
        if (rnd < 0.75) {
            if (random() < 0.5)
                bestorg[1] -= 800 * random() + 100;
            else
                bestorg[1] += 800 * random() + 100;
        }
        bestorg[2] += 2 * 48 * crandom();
        BotAI_Trace(&trace, bs->origin, NULL, NULL, bestorg, bs->entitynum, MASK_SOLID);
        VectorSubtract(trace.endpos, bs->origin, dir);
        len = VectorNormalize(dir);
        if (len > 200) {
            VectorScale(dir, len * trace.fraction - 40, dir);
            VectorAdd(bs->origin, dir, bestorg);
            belowbestorg[0] = bestorg[0];
            belowbestorg[1] = bestorg[1];
            belowbestorg[2] = bestorg[2] - 800;
            BotAI_Trace(&trace, bestorg, NULL, NULL, belowbestorg, bs->entitynum, MASK_SOLID);
            if (!trace.startsolid) {
                trace.endpos[2]++;
                pc = trap_PointContents(trace.endpos, bs->entitynum);
                if (!(pc & (CONTENTS_LAVA | CONTENTS_SLIME))) {
                    VectorCopy(bestorg, goal);
                    return;
                }
            }
        }
    }
    VectorCopy(bestorg, goal);
}

bot_moveresult_t BotAttackMove(bot_state_t* bs, int tfl) {
    int movetype, i, attackentity;
    float attack_skill, jumper, croucher, dist, strafechange_time;
    float attack_dist, attack_range;
    vec3_t forward, backward, sideward, hordir, up = {0, 0, 1};
    aas_entityinfo_t entinfo;
    bot_moveresult_t moveresult;
    bot_goal_t goal;

    attackentity = bs->enemy;
    if (bs->attackchase_time > FloatTime()) {
        goal.entitynum = attackentity;
        goal.areanum = bs->lastenemyareanum;
        VectorCopy(bs->lastenemyorigin, goal.origin);
        VectorSet(goal.mins, -8, -8, -8);
        VectorSet(goal.maxs, 8, 8, 8);
        BotSetupForMovement(bs);
        trap_BotMoveToGoal(&moveresult, bs->ms, &goal, tfl);
        return moveresult;
    }
    memset(&moveresult, 0, sizeof(bot_moveresult_t));
    attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
    jumper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_JUMPER, 0, 1);
    croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
    if (attack_skill < 0.2)
        return moveresult;
    BotSetupForMovement(bs);
    BotEntityInfo(attackentity, &entinfo);
    VectorSubtract(entinfo.origin, bs->origin, forward);
    dist = VectorNormalize(forward);
    VectorNegate(forward, backward);
    movetype = MOVE_WALK;
    if (bs->attackcrouch_time < FloatTime() - 1) {
        if (random() < jumper) {
            movetype = MOVE_JUMP;
        }
        else if (bs->attackcrouch_time < FloatTime() - 1 && random() < croucher) {
            bs->attackcrouch_time = FloatTime() + croucher * 5;
        }
    }
    if (bs->attackcrouch_time > FloatTime())
        movetype = MOVE_CROUCH;
    if (movetype == MOVE_JUMP) {
        if (bs->attackjump_time > FloatTime()) {
            movetype = MOVE_WALK;
        } else {
            bs->attackjump_time = FloatTime() + 1;
        }
    }
    if (bs->cur_ps.weapon == WP_GAUNTLET) {
        attack_dist = 0;
        attack_range = 0;
    } else {
        attack_dist = IDEAL_ATTACKDIST;
        attack_range = 40;
    }
    if (attack_skill <= 0.4) {
        if (dist > attack_dist + attack_range) {
            if (trap_BotMoveInDirection(bs->ms, forward, 400, movetype))
                return moveresult;
        }
        if (dist < attack_dist - attack_range) {
            if (trap_BotMoveInDirection(bs->ms, backward, 400, movetype))
                return moveresult;
        }
        return moveresult;
    }
    bs->attackstrafe_time += bs->thinktime;
    strafechange_time = 0.4 + (1 - attack_skill) * 0.2;
    if (attack_skill > 0.7)
        strafechange_time += crandom() * 0.2;
    if (bs->attackstrafe_time > strafechange_time) {
        if (random() > 0.935) {
            bs->flags ^= BFL_STRAFERIGHT;
            bs->attackstrafe_time = 0;
        }
    }
    for (i = 0; i < 2; i++) {
        hordir[0] = forward[0];
        hordir[1] = forward[1];
        hordir[2] = 0;
        VectorNormalize(hordir);
        CrossProduct(hordir, up, sideward);
        if (bs->flags & BFL_STRAFERIGHT)
            VectorNegate(sideward, sideward);
        if (random() > 0.9) {
            VectorAdd(sideward, backward, sideward);
        } else {
            if (dist > attack_dist + attack_range) {
                VectorAdd(sideward, forward, sideward);
            } else if (dist < attack_dist - attack_range) {
                VectorAdd(sideward, backward, sideward);
            }
        }
        if (trap_BotMoveInDirection(bs->ms, sideward, 400, movetype))
            return moveresult;
        bs->flags ^= BFL_STRAFERIGHT;
        bs->attackstrafe_time = 0;
    }
    return moveresult;
}

int BotSameTeam(bot_state_t* bs, int entnum) {
    if (bot_teamkill.integer) {
        return qfalse;
    }

    if (bs->client < 0 || bs->client >= MAX_CLIENTS) {
        return qfalse;
    }

    if (entnum < 0 || entnum >= MAX_CLIENTS) {
        return qfalse;
    }

    if (gametype >= GT_TEAM) {
        if (level.clients[bs->client].sess.sessionTeam == level.clients[entnum].sess.sessionTeam)
            return qtrue;
    }

    return qfalse;
}

qboolean InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles) {
    int i;
    float diff, angle;

    for (i = 0; i < 2; i++) {
        angle = AngleMod(viewangles[i]);
        angles[i] = AngleMod(angles[i]);
        diff = angles[i] - angle;
        if (angles[i] > angle) {
            if (diff > 180.0)
                diff -= 360.0;
        } else {
            if (diff < -180.0)
                diff += 360.0;
        }
        if (diff > 0) {
            if (diff > fov * 0.5)
                return qfalse;
        } else {
            if (diff < -fov * 0.5)
                return qfalse;
        }
    }
    return qtrue;
}

float BotEntityVisible(int viewer, vec3_t eye, vec3_t viewangles, float fov, int ent) {
    int i, contents_mask, passent, hitent, infog, inwater, otherinfog, pc;
    float squaredfogdist, waterfactor, vis, bestvis;
    bsp_trace_t trace;
    aas_entityinfo_t entinfo;
    vec3_t dir, entangles, start, end, middle;

    BotEntityInfo(ent, &entinfo);
    if (!entinfo.valid) {
        return 0;
    }

    VectorAdd(entinfo.mins, entinfo.maxs, middle);
    VectorScale(middle, 0.5, middle);
    VectorAdd(entinfo.origin, middle, middle);
    VectorSubtract(middle, eye, dir);
    vectoangles(dir, entangles);
    if (!InFieldOfVision(viewangles, fov, entangles))
        return 0;
    pc = trap_AAS_PointContents(eye);
    infog = (pc & CONTENTS_FOG);
    inwater = (pc & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER));
    bestvis = 0;
    for (i = 0; i < 3; i++) {
        contents_mask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
        passent = viewer;
        hitent = ent;
        VectorCopy(eye, start);
        VectorCopy(middle, end);
        if (trap_AAS_PointContents(middle) & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER)) {
            contents_mask |= (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER);
        }
        if (inwater) {
            if (!(contents_mask & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER))) {
                passent = ent;
                hitent = viewer;
                VectorCopy(middle, start);
                VectorCopy(eye, end);
            }
            contents_mask ^= (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER);
        }
        BotAI_Trace(&trace, start, NULL, NULL, end, passent, contents_mask);
        waterfactor = 1.0;
        if (trace.contents & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER)) {
            if (1) {
                contents_mask &= ~(CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER);
                BotAI_Trace(&trace, trace.endpos, NULL, NULL, end, passent, contents_mask);
                waterfactor = 0.5;
            }
        }
        if (trace.fraction >= 1 || trace.ent == hitent) {
            otherinfog = (trap_AAS_PointContents(middle) & CONTENTS_FOG);
            if (infog && otherinfog) {
                VectorSubtract(trace.endpos, eye, dir);
                squaredfogdist = VectorLengthSquared(dir);
            } else if (infog) {
                VectorCopy(trace.endpos, start);
                BotAI_Trace(&trace, start, NULL, NULL, eye, viewer, CONTENTS_FOG);
                VectorSubtract(eye, trace.endpos, dir);
                squaredfogdist = VectorLengthSquared(dir);
            } else if (otherinfog) {
                VectorCopy(trace.endpos, end);
                BotAI_Trace(&trace, eye, NULL, NULL, end, viewer, CONTENTS_FOG);
                VectorSubtract(end, trace.endpos, dir);
                squaredfogdist = VectorLengthSquared(dir);
            } else {
                squaredfogdist = 0;
            }
            vis = 1 / ((squaredfogdist * 0.001) < 1 ? 1 : (squaredfogdist * 0.001));
            vis *= waterfactor;
            if (vis > bestvis)
                bestvis = vis;
            if (bestvis >= 0.95)
                return bestvis;
        }
        if (i == 0)
            middle[2] += entinfo.mins[2];
        else if (i == 1)
            middle[2] += entinfo.maxs[2] - entinfo.mins[2];
    }
    return bestvis;
}

static int BotHearsEnemy(bot_state_t* bs, int client) {
    bot_goal_t goal;
    aas_entityinfo_t entinfo;
    int i;

    if (bs->enemy != -1)
        return qfalse;
    if (BotWantsToRetreat(bs))
        return qfalse;
    if (!BotWantsToChase(bs))
        return qfalse;
    for (i = 0; i < 2; i++) {
        if (!(i == 0 ? trap_BotGetTopGoal(bs->gs, &goal) : trap_BotGetSecondGoal(bs->gs, &goal)))
            continue;
        BotEntityInfo(goal.entitynum, &entinfo);
        if (((entinfo.itemType == IT_HEALTH || entinfo.itemType == IT_ARMOR) && entinfo.itemQuantity >= 25) ||
            entinfo.itemType == IT_POWERUP)
            return qfalse;
    }
    if (bs->settings.skill < 4)
        return qfalse;
    return (bs->heardClients & (1 << client)) != 0;
}

int BotFindEnemy(bot_state_t* bs, int curenemy) {
    int i, healthdecrease, heard;
    float f, alertness, easyfragger, vis;
    float squaredist, cursquaredist;
    aas_entityinfo_t entinfo, curenemyinfo;
    vec3_t dir, angles;

    alertness = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ALERTNESS, 0, 1);
    if (bs->settings.skill > 1)
        easyfragger = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_EASY_FRAGGER, 0, 1);
    else
        easyfragger = 0;
    healthdecrease = bs->lasthealth > bs->inventory[INVENTORY_HEALTH];
    bs->lasthealth = bs->inventory[INVENTORY_HEALTH];
    if (curenemy >= 0) {
        BotEntityInfo(curenemy, &curenemyinfo);
        if (EntityCarriesFlag(&curenemyinfo))
            return qfalse;
        VectorSubtract(curenemyinfo.origin, bs->origin, dir);
        cursquaredist = VectorLengthSquared(dir);
    } else {
        cursquaredist = 0;
    }
    if (gametype == GT_OBELISK) {
        vec3_t target;
        bot_goal_t* goal;
        bsp_trace_t trace;

        if (BotTeam(bs) == TEAM_RED)
            goal = &blueobelisk;
        else
            goal = &redobelisk;
        VectorCopy(goal->origin, target);
        target[2] += 1;
        BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
        if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
            if (goal->entitynum == bs->enemy) {
                return qfalse;
            }
            bs->enemy = goal->entitynum;
            bs->enemysight_time = FloatTime();
            bs->enemysuicide = qfalse;
            bs->enemydeath_time = 0;
            bs->enemyvisible_time = FloatTime();
            return qtrue;
        }
    }
    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        if (i == curenemy)
            continue;
        if (g_entities[i].flags & FL_NOTARGET) {
            continue;
        }
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum)
            continue;
        if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
            continue;
        }
        if (easyfragger < 0.5 && EntityIsChatting(&entinfo))
            continue;
        if (lastteleport_time > FloatTime() - 3) {
            VectorSubtract(entinfo.origin, lastteleport_origin, dir);
            if (VectorLengthSquared(dir) < Square(70))
                continue;
        }
        VectorSubtract(entinfo.origin, bs->origin, dir);
        squaredist = VectorLengthSquared(dir);
        if (!EntityCarriesFlag(&entinfo)) {
            if (curenemy >= 0 && squaredist > cursquaredist)
                continue;
        }
        if (squaredist > Square(900.0 + alertness * 4000.0))
            continue;
        if (BotSameTeam(bs, i))
            continue;
        if (curenemy < 0 && (healthdecrease || EntityIsShooting(&entinfo)))
            f = 360;
        else
            f = 90 + 90 - (90 - (squaredist > Square(810) ? Square(810) : squaredist) / (810 * 9));
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, f, i);
        heard = qfalse;
        if (vis <= 0) {
            if (!BotHearsEnemy(bs, i))
                continue;
            heard = qtrue;
        }
        if (curenemy < 0 && squaredist > Square(100) && !healthdecrease && !EntityIsShooting(&entinfo)) {
            VectorSubtract(bs->origin, entinfo.origin, dir);
            vectoangles(dir, angles);
            if (!InFieldOfVision(entinfo.angles, 90, angles)) {
                BotUpdateBattleInventory(bs, i);
                if (BotWantsToRetreat(bs))
                    continue;
            }
        }
        bs->enemy = entinfo.number;
        if (curenemy >= 0)
            bs->enemysight_time = FloatTime() - 2;
        else
            bs->enemysight_time = FloatTime();
        bs->enemyHeard = heard;
        bs->enemysuicide = qfalse;
        bs->enemydeath_time = 0;
        bs->enemyvisible_time = FloatTime();
        return qtrue;
    }
    return qfalse;
}

int BotTeamFlagCarrierVisible(bot_state_t* bs) {
    int i;
    float vis;
    aas_entityinfo_t entinfo;

    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (!EntityCarriesFlag(&entinfo))
            continue;
        if (!BotSameTeam(bs, i))
            continue;
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
        if (vis <= 0)
            continue;
        return i;
    }
    return -1;
}

int BotTeamFlagCarrier(bot_state_t* bs) {
    int i;
    aas_entityinfo_t entinfo;

    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (!EntityCarriesFlag(&entinfo))
            continue;
        if (!BotSameTeam(bs, i))
            continue;
        return i;
    }
    return -1;
}

int BotEnemyFlagCarrierVisible(bot_state_t* bs) {
    int i;
    float vis;
    aas_entityinfo_t entinfo;

    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (!EntityCarriesFlag(&entinfo))
            continue;
        if (BotSameTeam(bs, i))
            continue;
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
        if (vis <= 0)
            continue;
        return i;
    }
    return -1;
}

void BotVisibleTeamMatesAndEnemies(bot_state_t* bs, int* teammates, int* enemies, float range) {
    int i;
    float vis;
    aas_entityinfo_t entinfo;
    vec3_t dir;

    if (teammates)
        *teammates = 0;
    if (enemies)
        *enemies = 0;
    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (!EntityCarriesFlag(&entinfo))
            continue;
        VectorSubtract(entinfo.origin, bs->origin, dir);
        if (VectorLengthSquared(dir) > Square(range))
            continue;
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
        if (vis <= 0)
            continue;
        if (BotSameTeam(bs, i)) {
            if (teammates)
                (*teammates)++;
        } else {
            if (enemies)
                (*enemies)++;
        }
    }
}

int BotTeamCubeCarrierVisible(bot_state_t* bs) {
    int i;
    float vis;
    aas_entityinfo_t entinfo;

    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (!EntityCarriesCubes(&entinfo))
            continue;
        if (!BotSameTeam(bs, i))
            continue;
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
        if (vis <= 0)
            continue;
        return i;
    }
    return -1;
}

int BotEnemyCubeCarrierVisible(bot_state_t* bs) {
    int i;
    float vis;
    aas_entityinfo_t entinfo;

    for (i = 0; i < level.maxclients; i++) {
        if (i == bs->client)
            continue;
        BotEntityInfo(i, &entinfo);
        if (!entinfo.valid)
            continue;
        if (!EntityCarriesCubes(&entinfo))
            continue;
        if (BotSameTeam(bs, i))
            continue;
        vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
        if (vis <= 0)
            continue;
        return i;
    }
    return -1;
}

void BotAimAtEnemy(bot_state_t* bs) {
    int i, enemyvisible;
    float dist, f, aim_skill, aim_accuracy, speed, reactiontime;
    vec3_t dir, bestorigin, end, start, groundtarget, cmdmove, enemyvelocity;
    vec3_t mins = {-4, -4, -4}, maxs = {4, 4, 4};
    weaponinfo_t wi;
    aas_entityinfo_t entinfo;
    bot_goal_t goal;
    bsp_trace_t trace;
    vec3_t target;

    if (bs->enemy < 0) {
        return;
    }
    BotEntityInfo(bs->enemy, &entinfo);
    if (bs->enemy >= MAX_CLIENTS) {
        VectorCopy(entinfo.origin, target);
        if (bs->enemy == redobelisk.entitynum ||
            bs->enemy == blueobelisk.entitynum) {
            target[2] += 32;
        }
        VectorSubtract(target, bs->eye, dir);
        vectoangles(dir, bs->ideal_viewangles);
        VectorCopy(target, bs->aimtarget);
        return;
    }
    aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL, 0, 1);
    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
    if (aim_skill > 0.95) {
        reactiontime = 0.5 * trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
        if (bs->enemysight_time > FloatTime() - reactiontime)
            return;
        if (bs->teleport_time > FloatTime() - reactiontime)
            return;
    }

    wi.number = -1;
    if (bs->weaponnum > 0) {
        trap_BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
    }
    if (wi.number == -1) {
        bs->weaponnum = WP_MACHINEGUN;
        trap_EA_SelectWeapon(bs->client, WP_MACHINEGUN);
        return;
    }
    if (wi.number == WP_MACHINEGUN) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_MACHINEGUN, 0, 1);
    } else if (wi.number == WP_SHOTGUN) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_SHOTGUN, 0, 1);
    } else if (wi.number == WP_GRENADE_LAUNCHER) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_GRENADELAUNCHER, 0, 1);
        aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_GRENADELAUNCHER, 0, 1);
    } else if (wi.number == WP_ROCKET_LAUNCHER) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_ROCKETLAUNCHER, 0, 1);
        aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_ROCKETLAUNCHER, 0, 1);
    } else if (wi.number == WP_LIGHTNING) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_LIGHTNING, 0, 1);
    } else if (wi.number == WP_RAILGUN) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_RAILGUN, 0, 1);
    } else if (wi.number == WP_PLASMAGUN) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_PLASMAGUN, 0, 1);
        aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_PLASMAGUN, 0, 1);
    } else if (wi.number == WP_BFG) {
        aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_BFG10K, 0, 1);
        aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_BFG10K, 0, 1);
    }
    if (g_instaGib.integer) {
        aim_accuracy = bot_instaGibAimSkill.value;
        aim_skill = bot_instaGibAimSkill.value;
    }
    if (aim_accuracy <= 0)
        aim_accuracy = 0.0001f;
    BotEntityInfo(bs->enemy, &entinfo);
    if (EntityIsInvisible(&entinfo)) {
        if (random() > 0.1)
            aim_accuracy *= 0.4f;
    }
    VectorSubtract(entinfo.origin, entinfo.lastvisorigin, enemyvelocity);
    VectorScale(enemyvelocity, 1 / entinfo.update_time, enemyvelocity);
    if (bs->enemyposition_time < FloatTime()) {
        bs->enemyposition_time = FloatTime() + 0.5;
        VectorCopy(enemyvelocity, bs->enemyvelocity);
        VectorCopy(entinfo.origin, bs->enemyorigin);
    }
    if (aim_skill < 0.9) {
        VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
        if (VectorLengthSquared(dir) > Square(48)) {
            if (DotProduct(bs->enemyvelocity, enemyvelocity) < 0) {
                aim_accuracy *= 0.7f;
            }
        }
    }
    enemyvisible = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy);
    if (enemyvisible) {
        VectorCopy(entinfo.origin, bestorigin);
        bestorigin[2] += 8;
        VectorCopy(bs->origin, start);
        start[2] += bs->cur_ps.viewheight;
        start[2] += wi.offset[2];
        BotAI_Trace(&trace, start, mins, maxs, bestorigin, bs->entitynum, MASK_SHOT);
        if (trace.fraction <= 1 && trace.ent != entinfo.number) {
            bestorigin[2] += 16;
        }
        if (wi.speed) {
            VectorSubtract(bestorigin, bs->origin, dir);
            dist = VectorLength(dir);
            VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
            if (!(dist > 100 && VectorLengthSquared(dir) < Square(32))) {
                if (aim_skill > 0.8 &&
                    bs->cur_ps.weaponstate == WEAPON_READY) {
                    aas_clientmove_t move;
                    vec3_t origin;

                    VectorSubtract(entinfo.origin, bs->origin, dir);
                    dist = VectorLength(dir);
                    VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
                    VectorScale(dir, 1 / entinfo.update_time, dir);
                    VectorCopy(entinfo.origin, origin);
                    origin[2] += 1;
                    VectorClear(cmdmove);
                    trap_AAS_PredictClientMovement(&move, bs->enemy, origin,
                                                   PRESENCE_CROUCH, qfalse,
                                                   dir, cmdmove, 0,
                                                   dist * 10 / wi.speed, 0.1f, 0, 0, qfalse);
                    VectorCopy(move.endpos, bestorigin);
                }
                else if (aim_skill > 0.4) {
                    VectorSubtract(entinfo.origin, bs->origin, dir);
                    dist = VectorLength(dir);
                    VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
                    dir[2] = 0;
                    speed = VectorNormalize(dir) / entinfo.update_time;
                    VectorMA(entinfo.origin, (dist / wi.speed) * speed, dir, bestorigin);
                }
            }
        }
        if (aim_skill > 0.6 && wi.proj.damagetype & DAMAGETYPE_RADIAL) {
            if (entinfo.origin[2] < bs->origin[2] + 16) {
                VectorCopy(entinfo.origin, end);
                end[2] -= 64;
                BotAI_Trace(&trace, entinfo.origin, NULL, NULL, end, entinfo.number, MASK_SHOT);
                VectorCopy(bestorigin, groundtarget);
                if (trace.startsolid)
                    groundtarget[2] = entinfo.origin[2] - 16;
                else
                    groundtarget[2] = trace.endpos[2] - 8;
                BotAI_Trace(&trace, start, NULL, NULL, groundtarget, bs->entitynum, MASK_SHOT);
                if (fabs(trace.endpos[2] - groundtarget[2]) < 50) {
                    VectorSubtract(trace.endpos, groundtarget, dir);
                    if (VectorLengthSquared(dir) < Square(60)) {
                        VectorSubtract(trace.endpos, start, dir);
                        if (VectorLengthSquared(dir) > Square(100)) {
                            trace.endpos[2] += 1;
                            BotAI_Trace(&trace, trace.endpos, NULL, NULL, entinfo.origin, entinfo.number, MASK_SHOT);
                            if (trace.fraction >= 1) {
                                VectorCopy(groundtarget, bestorigin);
                            }
                        }
                    }
                }
            }
        }
        bestorigin[0] += 20 * crandom() * (1 - aim_accuracy);
        bestorigin[1] += 20 * crandom() * (1 - aim_accuracy);
        bestorigin[2] += 10 * crandom() * (1 - aim_accuracy);
    } else {
        VectorCopy(bs->lastenemyorigin, bestorigin);
        bestorigin[2] += 8;
        if (aim_skill > 0.5) {
            if (wi.number == WP_BFG ||
                wi.number == WP_ROCKET_LAUNCHER ||
                wi.number == WP_GRENADE_LAUNCHER) {
                goal.entitynum = bs->client;
                goal.areanum = bs->areanum;
                VectorCopy(bs->eye, goal.origin);
                VectorSet(goal.mins, -8, -8, -8);
                VectorSet(goal.maxs, 8, 8, 8);
                if (trap_BotPredictVisiblePosition(bs->lastenemyorigin, bs->lastenemyareanum, &goal, TFL_DEFAULT, target)) {
                    VectorSubtract(target, bs->eye, dir);
                    if (VectorLengthSquared(dir) > Square(80)) {
                        VectorCopy(target, bestorigin);
                        bestorigin[2] -= 20;
                    }
                }
                aim_accuracy = 1;
            }
        }
    }
    if (enemyvisible) {
        BotAI_Trace(&trace, bs->eye, NULL, NULL, bestorigin, bs->entitynum, MASK_SHOT);
        VectorCopy(trace.endpos, bs->aimtarget);
    } else {
        VectorCopy(bestorigin, bs->aimtarget);
    }
    VectorSubtract(bestorigin, bs->eye, dir);
    if (wi.number == WP_MACHINEGUN ||
        wi.number == WP_SHOTGUN ||
        wi.number == WP_LIGHTNING ||
        wi.number == WP_RAILGUN) {
        dist = VectorLength(dir);
        if (dist > 150)
            dist = 150;
        f = 0.6 + dist / 150 * 0.4;
        aim_accuracy *= f;
    }
    if (aim_accuracy < 0.8) {
        VectorNormalize(dir);
        for (i = 0; i < 3; i++)
            dir[i] += 0.3 * crandom() * (1 - aim_accuracy);
    }
    vectoangles(dir, bs->ideal_viewangles);
    bs->ideal_viewangles[PITCH] += 6 * wi.vspread * crandom() * (1 - aim_accuracy);
    bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
    bs->ideal_viewangles[YAW] += 6 * wi.hspread * crandom() * (1 - aim_accuracy);
    bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
    if (bot_challenge.integer) {
        if (aim_accuracy > 0.9 && bs->enemysight_time < FloatTime() - 1) {
            if (bs->ideal_viewangles[PITCH] > 180)
                bs->ideal_viewangles[PITCH] -= 360;
            VectorCopy(bs->ideal_viewangles, bs->viewangles);
            trap_EA_View(bs->client, bs->viewangles);
        }
    }
}

void BotCheckAttack(bot_state_t* bs) {
    float points, reactiontime, fov, firethrottle;
    int attackentity;
    bsp_trace_t bsptrace;
    vec3_t forward, right, start, end, dir, angles;
    weaponinfo_t wi;
    bsp_trace_t trace;
    aas_entityinfo_t entinfo;
    vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

    attackentity = bs->enemy;
    BotEntityInfo(attackentity, &entinfo);
    if (attackentity >= MAX_CLIENTS) {
        if (entinfo.number == redobelisk.entitynum ||
            entinfo.number == blueobelisk.entitynum) {
            if (g_entities[entinfo.number].activator &&
                g_entities[entinfo.number].activator->s.frame == 2) {
                return;
            }
        }
    }
    reactiontime = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
    if (bs->enemysight_time > FloatTime() - reactiontime)
        return;
    if (bs->teleport_time > FloatTime() - reactiontime)
        return;
    if (bs->weaponchange_time > FloatTime() - 0.1)
        return;
    if (bs->firethrottlewait_time > FloatTime())
        return;
    firethrottle = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_FIRETHROTTLE, 0, 1);
    if (bs->firethrottleshoot_time < FloatTime()) {
        if (random() > firethrottle) {
            bs->firethrottlewait_time = FloatTime() + firethrottle;
            bs->firethrottleshoot_time = 0;
        } else {
            bs->firethrottleshoot_time = FloatTime() + 1 - firethrottle;
            bs->firethrottlewait_time = 0;
        }
    }
    VectorSubtract(bs->aimtarget, bs->eye, dir);
    if (bs->weaponnum == WP_GAUNTLET) {
        if (VectorLengthSquared(dir) > Square(60)) {
            return;
        }
    }
    if (VectorLengthSquared(dir) < Square(100))
        fov = 120;
    else
        fov = 50;
    vectoangles(dir, angles);
    if (!InFieldOfVision(bs->viewangles, fov, angles))
        return;
    BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->aimtarget, bs->client, CONTENTS_SOLID | CONTENTS_PLAYERCLIP);
    if (bsptrace.fraction < 1 && bsptrace.ent != attackentity)
        return;

    if (bs->weaponnum <= 0) {
        return;
    }
    trap_BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
    VectorCopy(bs->origin, start);
    start[2] += bs->cur_ps.viewheight;
    AngleVectors(bs->viewangles, forward, right, NULL);
    start[0] += forward[0] * wi.offset[0] + right[0] * wi.offset[1];
    start[1] += forward[1] * wi.offset[0] + right[1] * wi.offset[1];
    start[2] += forward[2] * wi.offset[0] + right[2] * wi.offset[1] + wi.offset[2];
    VectorMA(start, 1000, forward, end);
    VectorMA(start, -12, forward, start);
    BotAI_Trace(&trace, start, mins, maxs, end, bs->entitynum, MASK_SHOT);
    if (trace.ent >= 0 && trace.ent < MAX_CLIENTS) {
        if (trace.ent != attackentity) {
            if (BotSameTeam(bs, trace.ent))
                return;
        }
    }
    if (trace.ent != attackentity || attackentity >= MAX_CLIENTS) {
        if (wi.proj.damagetype & DAMAGETYPE_RADIAL) {
            if (trace.fraction * 1000 < wi.proj.radius) {
                points = (wi.proj.damage - 0.5 * trace.fraction * 1000) * 0.5;
                if (points > 0) {
                    return;
                }
            }
        }
    }
    if (wi.flags & WFL_FIRERELEASED) {
        if (bs->flags & BFL_ATTACKED) {
            trap_EA_Attack(bs->client);
        }
    } else {
        trap_EA_Attack(bs->client);
    }
    bs->flags ^= BFL_ATTACKED;
}

void BotMapScripts(bot_state_t* bs) {
    char info[1024];
    char mapname[128];
    int i, shootbutton;
    float aim_accuracy;
    aas_entityinfo_t entinfo;
    vec3_t dir;

    trap_GetServerinfo(info, sizeof(info));

    strncpy(mapname, Info_ValueForKey(info, "mapname"), sizeof(mapname) - 1);
    mapname[sizeof(mapname) - 1] = '\0';

    if (!Q_stricmp(mapname, "q3tourney6") || !Q_stricmp(mapname, "q3tourney6_ctf") || !Q_stricmp(mapname, "mpq3tourney6")) {
        vec3_t mins = {694, 200, 480}, maxs = {968, 472, 680};
        vec3_t buttonorg = {304, 352, 920};
        bs->tfl &= ~TFL_FUNCBOB;
        if (!Q_stricmp(mapname, "mpq3tourney6")) {
            mins[2] += 64;
            maxs[2] += 64;
        }
        if (bs->origin[0] > mins[0] && bs->origin[0] < maxs[0]) {
            if (bs->origin[1] > mins[1] && bs->origin[1] < maxs[1]) {
                if (bs->origin[2] > mins[2] && bs->origin[2] < maxs[2]) {
                    return;
                }
            }
        }
        shootbutton = qfalse;
        for (i = 0; i < level.maxclients; i++) {
            if (i == bs->client)
                continue;
            BotEntityInfo(i, &entinfo);
            if (!entinfo.valid)
                continue;
            if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum)
                continue;
            if (entinfo.origin[0] > mins[0] && entinfo.origin[0] < maxs[0]) {
                if (entinfo.origin[1] > mins[1] && entinfo.origin[1] < maxs[1]) {
                    if (entinfo.origin[2] > mins[2] && entinfo.origin[2] < maxs[2]) {
                        if (BotSameTeam(bs, i)) {
                            shootbutton = qfalse;
                            break;
                        } else if (gametype < GT_CTF || bs->enemy == i) {
                            shootbutton = qtrue;
                        }
                    }
                }
            }
        }
        if (shootbutton) {
            bs->flags |= BFL_IDEALVIEWSET;
            VectorSubtract(buttonorg, bs->eye, dir);
            vectoangles(dir, bs->ideal_viewangles);
            aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
            bs->ideal_viewangles[PITCH] += 8 * crandom() * (1 - aim_accuracy);
            bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
            bs->ideal_viewangles[YAW] += 8 * crandom() * (1 - aim_accuracy);
            bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
            if (InFieldOfVision(bs->viewangles, 20, bs->ideal_viewangles)) {
                trap_EA_Attack(bs->client);
            }
        }
    }
}

static vec3_t VEC_UP = {0, -1, 0};
static vec3_t MOVEDIR_UP = {0, 0, 1};
static vec3_t VEC_DOWN = {0, -2, 0};
static vec3_t MOVEDIR_DOWN = {0, 0, -1};

void BotSetMovedir(vec3_t angles, vec3_t movedir) {
    if (VectorCompare(angles, VEC_UP)) {
        VectorCopy(MOVEDIR_UP, movedir);
    } else if (VectorCompare(angles, VEC_DOWN)) {
        VectorCopy(MOVEDIR_DOWN, movedir);
    } else {
        AngleVectors(angles, movedir, NULL, NULL);
    }
}

int BotModelMinsMaxs(int modelindex, int eType, int contents, vec3_t mins, vec3_t maxs) {
    gentity_t* ent;
    int i;

    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (eType && ent->s.eType != eType) {
            continue;
        }
        if (contents && ent->r.contents != contents) {
            continue;
        }
        if (ent->s.modelindex == modelindex) {
            if (mins)
                VectorAdd(ent->r.currentOrigin, ent->r.mins, mins);
            if (maxs)
                VectorAdd(ent->r.currentOrigin, ent->r.maxs, maxs);
            return i;
        }
    }
    if (mins)
        VectorClear(mins);
    if (maxs)
        VectorClear(maxs);
    return 0;
}

int BotFuncButtonActivateGoal(bot_state_t* bs, int bspent, bot_activategoal_t* activategoal) {
    int i, areas[10], numareas, modelindex, entitynum;
    char model[128];
    float lip, dist, health, angle;
    vec3_t size, start, end, mins, maxs, angles, points[10];
    vec3_t movedir, origin, goalorigin, bboxmins, bboxmaxs;
    vec3_t extramins = {1, 1, 1}, extramaxs = {-1, -1, -1};
    bsp_trace_t bsptrace;

    activategoal->shoot = qfalse;
    VectorClear(activategoal->target);
    trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
    if (!*model)
        return qfalse;
    modelindex = atoi(model + 1);
    if (!modelindex)
        return qfalse;
    entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
    trap_AAS_FloatForBSPEpairKey(bspent, "lip", &lip);
    if (!lip)
        lip = 4;
    trap_AAS_FloatForBSPEpairKey(bspent, "angle", &angle);
    VectorSet(angles, 0, angle, 0);
    BotSetMovedir(angles, movedir);
    VectorSubtract(maxs, mins, size);
    VectorAdd(mins, maxs, origin);
    VectorScale(origin, 0.5, origin);
    dist = fabs(movedir[0]) * size[0] + fabs(movedir[1]) * size[1] + fabs(movedir[2]) * size[2];
    dist *= 0.5;
    trap_AAS_FloatForBSPEpairKey(bspent, "health", &health);
    if (health) {
        VectorMA(origin, -dist, movedir, goalorigin);
        VectorCopy(goalorigin, activategoal->target);
        activategoal->shoot = qtrue;
        BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, goalorigin, bs->entitynum, MASK_SHOT);
        if (bsptrace.fraction >= 1.0 || bsptrace.ent == entitynum) {
            activategoal->goal.entitynum = entitynum;
            activategoal->goal.number = 0;
            activategoal->goal.flags = 0;
            VectorCopy(bs->origin, activategoal->goal.origin);
            activategoal->goal.areanum = bs->areanum;
            VectorSet(activategoal->goal.mins, -8, -8, -8);
            VectorSet(activategoal->goal.maxs, 8, 8, 8);
            return qtrue;
        } else {
            trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
            for (i = 0; i < 3; i++) {
                if (movedir[i] < 0)
                    dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
                else
                    dist += fabs(movedir[i]) * fabs(bboxmins[i]);
            }
            VectorMA(origin, -dist, movedir, goalorigin);
            VectorCopy(goalorigin, start);
            start[2] += 24;
            VectorCopy(start, end);
            end[2] -= 512;
            numareas = trap_AAS_TraceAreas(start, end, areas, points, 10);
            for (i = numareas - 1; i >= 0; i--) {
                if (trap_AAS_AreaReachability(areas[i])) {
                    break;
                }
            }
            if (i < 0) {
            }
            if (i >= 0) {
                VectorCopy(points[i], activategoal->goal.origin);
                activategoal->goal.areanum = areas[i];
                VectorSet(activategoal->goal.mins, 8, 8, 8);
                VectorSet(activategoal->goal.maxs, -8, -8, -8);
                for (i = 0; i < 3; i++) {
                    if (movedir[i] < 0)
                        activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
                    else
                        activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
                }
                activategoal->goal.entitynum = entitynum;
                activategoal->goal.number = 0;
                activategoal->goal.flags = 0;
                return qtrue;
            }
        }
        return qfalse;
    } else {
        trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
        for (i = 0; i < 3; i++) {
            if (movedir[i] < 0)
                dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
            else
                dist += fabs(movedir[i]) * fabs(bboxmins[i]);
        }
        VectorMA(origin, -dist, movedir, goalorigin);
        VectorCopy(goalorigin, start);
        start[2] += 24;
        VectorCopy(start, end);
        end[2] -= 100;
        numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
        for (i = 0; i < numareas; i++) {
            if (trap_AAS_AreaReachability(areas[i])) {
                break;
            }
        }
        if (i < numareas) {
            VectorCopy(origin, activategoal->goal.origin);
            activategoal->goal.areanum = areas[i];
            VectorSubtract(mins, origin, activategoal->goal.mins);
            VectorSubtract(maxs, origin, activategoal->goal.maxs);
            for (i = 0; i < 3; i++) {
                if (movedir[i] < 0)
                    activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
                else
                    activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
            }
            activategoal->goal.entitynum = entitynum;
            activategoal->goal.number = 0;
            activategoal->goal.flags = 0;
            return qtrue;
        }
    }
    return qfalse;
}

int BotFuncDoorActivateGoal(bot_state_t* bs, int bspent, bot_activategoal_t* activategoal) {
    int modelindex, entitynum;
    char model[MAX_INFO_STRING];
    vec3_t mins, maxs, origin;

    trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
    if (!*model)
        return qfalse;
    modelindex = atoi(model + 1);
    if (!modelindex)
        return qfalse;
    entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
    VectorAdd(mins, maxs, origin);
    VectorScale(origin, 0.5, origin);
    VectorCopy(origin, activategoal->target);
    activategoal->shoot = qtrue;
    activategoal->goal.entitynum = entitynum;
    activategoal->goal.number = 0;
    activategoal->goal.flags = 0;
    VectorCopy(bs->origin, activategoal->goal.origin);
    activategoal->goal.areanum = bs->areanum;
    VectorSet(activategoal->goal.mins, -8, -8, -8);
    VectorSet(activategoal->goal.maxs, 8, 8, 8);
    return qtrue;
}

int BotTriggerMultipleActivateGoal(bot_state_t* bs, int bspent, bot_activategoal_t* activategoal) {
    int i, areas[10], numareas, modelindex, entitynum;
    char model[128];
    vec3_t start, end, mins, maxs;
    vec3_t origin, goalorigin;

    activategoal->shoot = qfalse;
    VectorClear(activategoal->target);
    trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
    if (!*model)
        return qfalse;
    modelindex = atoi(model + 1);
    if (!modelindex)
        return qfalse;
    entitynum = BotModelMinsMaxs(modelindex, 0, CONTENTS_TRIGGER, mins, maxs);
    VectorAdd(mins, maxs, origin);
    VectorScale(origin, 0.5, origin);
    VectorCopy(origin, goalorigin);
    VectorCopy(goalorigin, start);
    start[2] += 24;
    VectorCopy(start, end);
    end[2] -= 100;
    numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
    for (i = 0; i < numareas; i++) {
        if (trap_AAS_AreaReachability(areas[i])) {
            break;
        }
    }
    if (i < numareas) {
        VectorCopy(origin, activategoal->goal.origin);
        activategoal->goal.areanum = areas[i];
        VectorSubtract(mins, origin, activategoal->goal.mins);
        VectorSubtract(maxs, origin, activategoal->goal.maxs);
        activategoal->goal.entitynum = entitynum;
        activategoal->goal.number = 0;
        activategoal->goal.flags = 0;
        return qtrue;
    }
    return qfalse;
}

int BotPopFromActivateGoalStack(bot_state_t* bs) {
    if (!bs->activatestack)
        return qfalse;
    BotEnableActivateGoalAreas(bs->activatestack, qtrue);
    bs->activatestack->inuse = qfalse;
    bs->activatestack->justused_time = FloatTime();
    bs->activatestack = bs->activatestack->next;
    return qtrue;
}

int BotPushOntoActivateGoalStack(bot_state_t* bs, bot_activategoal_t* activategoal) {
    int i, best;
    float besttime;

    best = -1;
    besttime = FloatTime() + 9999;
    for (i = 0; i < MAX_ACTIVATESTACK; i++) {
        if (!bs->activategoalheap[i].inuse) {
            if (bs->activategoalheap[i].justused_time < besttime) {
                besttime = bs->activategoalheap[i].justused_time;
                best = i;
            }
        }
    }
    if (best != -1) {
        memcpy(&bs->activategoalheap[best], activategoal, sizeof(bot_activategoal_t));
        bs->activategoalheap[best].inuse = qtrue;
        bs->activategoalheap[best].next = bs->activatestack;
        bs->activatestack = &bs->activategoalheap[best];
        return qtrue;
    }
    return qfalse;
}

void BotClearActivateGoalStack(bot_state_t* bs) {
    while (bs->activatestack)
        BotPopFromActivateGoalStack(bs);
}

void BotEnableActivateGoalAreas(bot_activategoal_t* activategoal, int enable) {
    int i;

    if (activategoal->areasdisabled == !enable)
        return;
    for (i = 0; i < activategoal->numareas; i++)
        trap_AAS_EnableRoutingArea(activategoal->areas[i], enable);
    activategoal->areasdisabled = !enable;
}

int BotIsGoingToActivateEntity(bot_state_t* bs, int entitynum) {
    bot_activategoal_t* a;
    int i;

    for (a = bs->activatestack; a; a = a->next) {
        if (a->time < FloatTime())
            continue;
        if (a->goal.entitynum == entitynum)
            return qtrue;
    }
    for (i = 0; i < MAX_ACTIVATESTACK; i++) {
        if (bs->activategoalheap[i].inuse)
            continue;
        if (bs->activategoalheap[i].goal.entitynum == entitynum) {
            if (bs->activategoalheap[i].justused_time > FloatTime() - 2)
                return qtrue;
        }
    }
    return qfalse;
}

int BotGetActivateGoal(bot_state_t* bs, int entitynum, bot_activategoal_t* activategoal) {
    int i, ent, cur_entities[10], spawnflags, modelindex, areas[MAX_ACTIVATEAREAS * 2], numareas, t;
    char model[MAX_INFO_STRING], tmpmodel[128];
    char target[128], classname[128];
    float health;
    char targetname[10][128];
    aas_entityinfo_t entinfo;
    aas_areainfo_t areainfo;
    vec3_t origin, absmins, absmaxs;

    memset(activategoal, 0, sizeof(bot_activategoal_t));
    BotEntityInfo(entitynum, &entinfo);
    Com_sprintf(model, sizeof(model), "*%d", entinfo.modelindex);
    for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
        if (!trap_AAS_ValueForBSPEpairKey(ent, "model", tmpmodel, sizeof(tmpmodel)))
            continue;
        if (!strcmp(model, tmpmodel))
            break;
    }
    if (!ent) {
        BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity found with model %s\n", model);
        return 0;
    }
    trap_AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname));
    if (!*classname) {
        BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model %s has no classname\n", model);
        return 0;
    }
    if (!strcmp(classname, "func_door")) {
        if (trap_AAS_FloatForBSPEpairKey(ent, "health", &health)) {
            if (health) {
                BotFuncDoorActivateGoal(bs, ent, activategoal);
                return ent;
            }
        }
        trap_AAS_IntForBSPEpairKey(ent, "spawnflags", &spawnflags);
        if (spawnflags & 1)
            return 0;
        if (!trap_AAS_VectorForBSPEpairKey(ent, "origin", origin)) {
            VectorClear(origin);
        }
        if (!VectorCompare(origin, entinfo.origin))
            return 0;
        trap_AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model));
        if (*model) {
            modelindex = atoi(model + 1);
            if (modelindex) {
                BotModelMinsMaxs(modelindex, ET_MOVER, 0, absmins, absmaxs);
                numareas = trap_AAS_BBoxAreas(absmins, absmaxs, areas, MAX_ACTIVATEAREAS * 2);
                for (i = 0; i < numareas; i++) {
                    if (activategoal->numareas >= MAX_ACTIVATEAREAS)
                        break;
                    if (!trap_AAS_AreaReachability(areas[i])) {
                        continue;
                    }
                    trap_AAS_AreaInfo(areas[i], &areainfo);
                    if (areainfo.contents & AREACONTENTS_MOVER) {
                        activategoal->areas[activategoal->numareas++] = areas[i];
                    }
                }
                for (i = 0; i < numareas; i++) {
                    if (activategoal->numareas >= MAX_ACTIVATEAREAS)
                        break;
                    if (trap_AAS_AreaReachability(areas[i])) {
                        continue;
                    }
                    trap_AAS_AreaInfo(areas[i], &areainfo);
                    if (areainfo.contents & AREACONTENTS_MOVER) {
                        activategoal->areas[activategoal->numareas++] = areas[i];
                    }
                }
            }
        }
    }
    if (!strcmp(classname, "func_button")) {
        return 0;
    }
    if (!trap_AAS_ValueForBSPEpairKey(ent, "targetname", targetname[0], sizeof(targetname[0]))) {
        if (bot_developer.integer) {
            BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model \"%s\" has no targetname\n", model);
        }
        return 0;
    }
    cur_entities[0] = trap_AAS_NextBSPEntity(0);
    for (i = 0; i >= 0 && i < 10;) {
        for (ent = cur_entities[i]; ent; ent = trap_AAS_NextBSPEntity(ent)) {
            if (!trap_AAS_ValueForBSPEpairKey(ent, "target", target, sizeof(target)))
                continue;
            if (!strcmp(targetname[i], target)) {
                cur_entities[i] = trap_AAS_NextBSPEntity(ent);
                break;
            }
        }
        if (!ent) {
            if (bot_developer.integer) {
                BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity with target \"%s\"\n", targetname[i]);
            }
            i--;
            continue;
        }
        if (!trap_AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname))) {
            if (bot_developer.integer) {
                BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with target \"%s\" has no classname\n", targetname[i]);
            }
            continue;
        }
        if (!strcmp(classname, "func_button")) {
            if (!BotFuncButtonActivateGoal(bs, ent, activategoal))
                continue;
            if (bs->activatestack && bs->activatestack->inuse &&
                bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
                bs->activatestack->time > FloatTime() &&
                bs->activatestack->start_time < FloatTime() - 2)
                continue;
            if (trap_AAS_AreaReachability(bs->areanum)) {
                BotEnableActivateGoalAreas(activategoal, qfalse);
                t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
                if (!t) {
                    continue;
                }
                activategoal->time = FloatTime() + t * 0.01 + 5;
            }
            return ent;
        }
        else if (!strcmp(classname, "trigger_multiple")) {
            if (!BotTriggerMultipleActivateGoal(bs, ent, activategoal))
                continue;
            if (bs->activatestack && bs->activatestack->inuse &&
                bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
                bs->activatestack->time > FloatTime() &&
                bs->activatestack->start_time < FloatTime() - 2)
                continue;
            if (trap_AAS_AreaReachability(bs->areanum)) {
                BotEnableActivateGoalAreas(activategoal, qfalse);
                t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
                if (!t) {
                    continue;
                }
                activategoal->time = FloatTime() + t * 0.01 + 5;
            }
            return ent;
        } else if (!strcmp(classname, "func_timer")) {
            continue;
        }
        else if (!strcmp(classname, "target_relay") || !strcmp(classname, "target_delay")) {
            if (trap_AAS_ValueForBSPEpairKey(ent, "targetname", targetname[i + 1], sizeof(targetname[0]))) {
                i++;
                cur_entities[i] = trap_AAS_NextBSPEntity(0);
            }
        }
    }
#ifdef OBSTACLEDEBUG
    BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no valid activator for entity with target \"%s\"\n", targetname[0]);
#endif
    return 0;
}

int BotGoForActivateGoal(bot_state_t* bs, bot_activategoal_t* activategoal) {
    aas_entityinfo_t activateinfo;

    activategoal->inuse = qtrue;
    if (!activategoal->time)
        activategoal->time = FloatTime() + 10;
    activategoal->start_time = FloatTime();
    BotEntityInfo(activategoal->goal.entitynum, &activateinfo);
    VectorCopy(activateinfo.origin, activategoal->origin);
    if (BotPushOntoActivateGoalStack(bs, activategoal)) {
        AIEnter_Seek_ActivateEntity(bs, "BotGoForActivateGoal");
        return qtrue;
    } else {
        BotEnableActivateGoalAreas(activategoal, qtrue);
        return qfalse;
    }
}

void BotPrintActivateGoalInfo(bot_state_t* bs, bot_activategoal_t* activategoal, int bspent) {
    char netname[MAX_NETNAME];
    char classname[128];
    char buf[128];

    ClientName(bs->client, netname, sizeof(netname));
    trap_AAS_ValueForBSPEpairKey(bspent, "classname", classname, sizeof(classname));
    if (activategoal->shoot) {
        Com_sprintf(buf, sizeof(buf), "%s: I have to shoot at a %s from %1.1f %1.1f %1.1f in area %d\n",
                    netname, classname,
                    activategoal->goal.origin[0],
                    activategoal->goal.origin[1],
                    activategoal->goal.origin[2],
                    activategoal->goal.areanum);
    } else {
        Com_sprintf(buf, sizeof(buf), "%s: I have to activate a %s at %1.1f %1.1f %1.1f in area %d\n",
                    netname, classname,
                    activategoal->goal.origin[0],
                    activategoal->goal.origin[1],
                    activategoal->goal.origin[2],
                    activategoal->goal.areanum);
    }
    trap_EA_Say(bs->client, buf);
}

void BotRandomMove(bot_state_t* bs, bot_moveresult_t* moveresult) {
    vec3_t dir, angles;

    angles[0] = 0;
    angles[1] = random() * 360;
    angles[2] = 0;
    AngleVectors(angles, dir, NULL, NULL);

    trap_BotMoveInDirection(bs->ms, dir, 400, MOVE_WALK);

    moveresult->failure = qfalse;
    VectorCopy(dir, moveresult->movedir);
}

void BotAIBlocked(bot_state_t* bs, bot_moveresult_t* moveresult, int activate) {
#ifdef OBSTACLEDEBUG
    char netname[MAX_NETNAME];
#endif
    int movetype, bspent;
    vec3_t hordir, sideward, angles, up = {0, 0, 1};
    aas_entityinfo_t entinfo;
    bot_activategoal_t activategoal;

    if (!moveresult->blocked) {
        bs->notblocked_time = FloatTime();
        return;
    }
    if (moveresult->type == RESULTTYPE_INSOLIDAREA) {
        BotRandomMove(bs, moveresult);
        return;
    }
    BotEntityInfo(moveresult->blockentity, &entinfo);
#ifdef OBSTACLEDEBUG
    ClientName(bs->client, netname, sizeof(netname));
    BotAI_Print(PRT_MESSAGE, "%s: I'm blocked by model %d\n", netname, entinfo.modelindex);
#endif
    if (activate && entinfo.modelindex > 0 && entinfo.modelindex <= max_bspmodelindex) {
        bspent = BotGetActivateGoal(bs, entinfo.number, &activategoal);
        if (bspent) {
            if (bs->activatestack && !bs->activatestack->inuse)
                bs->activatestack = NULL;
            if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
                BotGoForActivateGoal(bs, &activategoal);
            }
            if (!(moveresult->flags & MOVERESULT_ONTOPOFOBSTACLE) &&
                trap_AAS_AreaReachability(bs->areanum))
                return;
        } else {
            BotEnableActivateGoalAreas(&activategoal, qtrue);
        }
    }
    hordir[0] = moveresult->movedir[0];
    hordir[1] = moveresult->movedir[1];
    hordir[2] = 0;
    if (VectorNormalize(hordir) < 0.1) {
        VectorSet(angles, 0, 360 * random(), 0);
        AngleVectors(angles, hordir, NULL, NULL);
    }
    movetype = MOVE_WALK;
    CrossProduct(hordir, up, sideward);
    if (bs->flags & BFL_AVOIDRIGHT)
        VectorNegate(sideward, sideward);
    if (movetype != MOVE_CROUCH || !trap_BotMoveInDirection(bs->ms, hordir, 400, movetype)) {
        if (!trap_BotMoveInDirection(bs->ms, sideward, 400, movetype)) {
            bs->flags ^= BFL_AVOIDRIGHT;
            VectorMA(sideward, -1, hordir, sideward);
            trap_BotMoveInDirection(bs->ms, sideward, 400, movetype);
        }
    }
    if (bs->notblocked_time < FloatTime() - 0.4) {
        if (bs->ainode == AINode_Seek_NBG)
            bs->nbg_time = 0;
        else if (bs->ainode == AINode_Seek_LTG)
            bs->ltg_time = 0;
    }
}

int BotAIPredictObstacles(bot_state_t* bs, bot_goal_t* goal) {
    int modelnum, entitynum, bspent;
    bot_activategoal_t activategoal;
    aas_predictroute_t route;

    if (!bot_predictobstacles.integer)
        return qfalse;

    if (bs->predictobstacles_goalareanum == goal->areanum &&
        bs->predictobstacles_time > FloatTime() - 6) {
        return qfalse;
    }
    bs->predictobstacles_goalareanum = goal->areanum;
    bs->predictobstacles_time = FloatTime();

    trap_AAS_PredictRoute(&route, bs->areanum, bs->origin,
                          goal->areanum, bs->tfl, 100, 1000,
                          RSE_USETRAVELTYPE | RSE_ENTERCONTENTS,
                          AREACONTENTS_MOVER, TFL_BRIDGE, 0);
    if (route.stopevent & RSE_ENTERCONTENTS) {
        if (route.endcontents & AREACONTENTS_MOVER) {
            modelnum = (route.endcontents & AREACONTENTS_MODELNUM) >> AREACONTENTS_MODELNUMSHIFT;
            if (modelnum) {
                entitynum = BotModelMinsMaxs(modelnum, ET_MOVER, 0, NULL, NULL);
                if (entitynum) {
                    bspent = BotGetActivateGoal(bs, entitynum, &activategoal);
                    if (bspent) {
                        if (bs->activatestack && !bs->activatestack->inuse)
                            bs->activatestack = NULL;
                        if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
                            BotGoForActivateGoal(bs, &activategoal);
                            return qtrue;
                        } else {
                            BotEnableActivateGoalAreas(&activategoal, qtrue);
                        }
                    }
                }
            }
        }
    } else if (route.stopevent & RSE_USETRAVELTYPE) {
        if (route.endtravelflags & TFL_BRIDGE) {
        }
    }
    return qfalse;
}

void BotCheckConsoleMessages(bot_state_t* bs) {
    char botname[MAX_NETNAME], message[MAX_MESSAGE_SIZE], netname[MAX_NETNAME], *ptr;
    float chat_reply;
    int context, handle;
    bot_consolemessage_t m;
    bot_match_t match;

    ClientName(bs->client, botname, sizeof(botname));
    while ((handle = trap_BotNextConsoleMessage(bs->cs, &m)) != 0) {
        if (trap_BotNumConsoleMessages(bs->cs) < 10) {
            if (m.type == CMS_CHAT && m.time > FloatTime() - (1 + random()))
                break;
        }
        ptr = m.message;
        if (m.type == CMS_CHAT) {
            if (trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
                ptr = m.message + match.variables[MESSAGE].offset;
            }
        }
        trap_UnifyWhiteSpaces(ptr);
        context = BotSynonymContext(bs);
        trap_BotReplaceSynonyms(ptr, context);
        if (!BotMatchMessage(bs, m.message)) {
            if (m.type == CMS_CHAT && !bot_nochat.integer && !g_instaGib.integer) {
                if (!trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
                    trap_BotRemoveConsoleMessage(bs->cs, handle);
                    continue;
                }
                if (match.subtype & ST_TEAM) {
                    trap_BotRemoveConsoleMessage(bs->cs, handle);
                    continue;
                }
                trap_BotMatchVariable(&match, NETNAME, netname, sizeof(netname));
                trap_BotMatchVariable(&match, MESSAGE, message, sizeof(message));
                if (bs->client == ClientFromName(netname)) {
                    trap_BotRemoveConsoleMessage(bs->cs, handle);
                    continue;
                }
                trap_UnifyWhiteSpaces(message);
                trap_Cvar_Update(&bot_testrchat);
                if (bot_testrchat.integer) {
                    trap_BotLibVarSet("bot_testrchat", "1");
                    if (trap_BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
                                          NULL, NULL,
                                          NULL, NULL,
                                          NULL, NULL,
                                          botname, netname)) {
                        BotAI_Print(PRT_MESSAGE, "------------------------\n");
                    } else {
                        BotAI_Print(PRT_MESSAGE, "**** no valid reply ****\n");
                    }
                }
                else if (bs->ainode != AINode_Stand && BotValidChatPosition(bs) && !TeamPlayIsOn()) {
                    chat_reply = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_REPLY, 0, 1);
                    if (random() < 1.5 / (NumBots() + 1) && random() < chat_reply) {
                        if (trap_BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
                                              NULL, NULL,
                                              NULL, NULL,
                                              NULL, NULL,
                                              botname, netname)) {
                            trap_BotRemoveConsoleMessage(bs->cs, handle);
                            bs->stand_time = FloatTime() + BotChatTime(bs);
                            AIEnter_Stand(bs, "BotCheckConsoleMessages: reply chat");
                            break;
                        }
                    }
                }
            }
        }
        trap_BotRemoveConsoleMessage(bs->cs, handle);
    }
}

void BotCheckForGrenades(bot_state_t* bs, entityState_t* state) {
    if (state->eType != ET_MISSILE || state->weapon != WP_GRENADE_LAUNCHER)
        return;
    trap_BotAddAvoidSpot(bs->ms, state->pos.trBase, 160, AVOID_ALWAYS);
}

void BotCheckForProxMines(bot_state_t* bs, entityState_t* state) {
    if (state->eType != ET_MISSILE || state->weapon != WP_PROX_LAUNCHER)
        return;
    if (state->generic1 == BotTeam(bs))
        return;
    if (!(bs->inventory[INVENTORY_PLASMAGUN] > 0 && bs->inventory[INVENTORY_CELLS] > 0) &&
        !(bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 && bs->inventory[INVENTORY_ROCKETS] > 0) &&
        !(bs->inventory[INVENTORY_BFG10K] > 0 && bs->inventory[INVENTORY_BFGAMMO] > 0)) {
        return;
    }
    trap_BotAddAvoidSpot(bs->ms, state->pos.trBase, 160, AVOID_ALWAYS);
    if (bs->numproxmines >= MAX_PROXMINES)
        return;
    bs->proxmines[bs->numproxmines] = state->number;
    bs->numproxmines++;
}

void BotCheckForKamikazeBody(bot_state_t* bs, entityState_t* state) {
    if (!(state->eFlags & EF_KAMIKAZE))
        return;
    if (!(state->eFlags & EF_DEAD))
        return;
    bs->kamikazebody = state->number;
}

void BotCheckEvents(bot_state_t* bs, entityState_t* state) {
    int event;
    char buf[128];
    aas_entityinfo_t entinfo;

    if (bs->entityeventTime[state->number] == g_entities[state->number].eventTime) {
        return;
    }
    bs->entityeventTime[state->number] = g_entities[state->number].eventTime;
    bs->heardClients = 0;
    if (state->eType > ET_EVENTS) {
        event = (state->eType - ET_EVENTS) & ~EV_EVENT_BITS;
    } else {
        event = state->event & ~EV_EVENT_BITS;
    }
    switch (event) {
        case EV_FOOTSTEP:
        case EV_FOOTSTEP_METAL:
        case EV_FOOTSPLASH:
        case EV_FOOTWADE:
        case EV_SWIM:
        case EV_FALL_SHORT:
        case EV_FALL_MEDIUM:
        case EV_FALL_FAR:
        case EV_JUMP_PAD:
        case EV_JUMP:
        case EV_WATER_TOUCH:
        case EV_WATER_LEAVE:
        case EV_WATER_UNDER:
        case EV_WATER_CLEAR:
        case EV_ITEM_PICKUP:
        case EV_GLOBAL_ITEM_PICKUP:
        case EV_NOAMMO:
        case EV_CHANGE_WEAPON:
        case EV_FIRE_WEAPON:
        case EV_TAUNT: {
            if (state->clientNum != bs->client && (unsigned)state->clientNum < 32) {
                bs->heardClients |= 1 << state->clientNum;
            }
            break;
        }
        case EV_OBITUARY: {
            int target, attacker, mod;

            target = state->otherEntityNum;
            attacker = state->otherEntityNum2;
            mod = state->eventParm;
            if (target == bs->client) {
                bs->botdeathtype = mod;
                bs->lastkilledby = attacker;
                if (target == attacker ||
                    target == ENTITYNUM_NONE ||
                    target == ENTITYNUM_WORLD)
                    bs->botsuicide = qtrue;
                else
                    bs->botsuicide = qfalse;
                bs->num_deaths++;
            }
            else if (attacker == bs->client) {
                bs->enemydeathtype = mod;
                bs->lastkilledplayer = target;
                bs->killedenemy_time = FloatTime();
                bs->num_kills++;
            } else if (attacker == bs->enemy && target == attacker) {
                bs->enemysuicide = qtrue;
            }
            if (gametype == GT_1FCTF) {
                BotEntityInfo(target, &entinfo);
                if (entinfo.powerups & (1 << PW_NEUTRALFLAG)) {
                    if (!BotSameTeam(bs, target)) {
                        bs->neutralflagstatus = 3;
                        bs->flagstatuschanged = qtrue;
                    }
                }
            }
            break;
        }
        case EV_GLOBAL_SOUND: {
            if (state->eventParm < 0 || state->eventParm >= MAX_SOUNDS) {
                BotAI_Print(PRT_ERROR, "EV_GLOBAL_SOUND: eventParm (%d) out of range\n", state->eventParm);
                break;
            }
            trap_GetConfigstring(CS_SOUNDS + state->eventParm, buf, sizeof(buf));
            if (!strcmp(buf, "sound/items/kamikazerespawn.wav")) {
                BotDontAvoid(bs, "Kamikaze");
            } else if (!strcmp(buf, "sound/items/poweruprespawn.wav")) {
                BotGoForPowerups(bs);
            }
            break;
        }
        case EV_GLOBAL_TEAM_SOUND: {
            if (gametype == GT_CTF) {
                switch (state->eventParm) {
                    case GTS_RED_CAPTURE:
                        bs->blueflagstatus = 0;
                        bs->redflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_BLUE_CAPTURE:
                        bs->blueflagstatus = 0;
                        bs->redflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_RED_RETURN:
                        bs->blueflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_BLUE_RETURN:
                        bs->redflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_RED_TAKEN:
                        bs->blueflagstatus = 1;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_BLUE_TAKEN:
                        bs->redflagstatus = 1;
                        bs->flagstatuschanged = qtrue;
                        break;
                }
            } else if (gametype == GT_1FCTF) {
                switch (state->eventParm) {
                    case GTS_RED_CAPTURE:
                        bs->neutralflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_BLUE_CAPTURE:
                        bs->neutralflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_RED_RETURN:
                        bs->neutralflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_BLUE_RETURN:
                        bs->neutralflagstatus = 0;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_RED_TAKEN:
                        bs->neutralflagstatus = BotTeam(bs) == TEAM_RED ? 2 : 1;
                        bs->flagstatuschanged = qtrue;
                        break;
                    case GTS_BLUE_TAKEN:
                        bs->neutralflagstatus = BotTeam(bs) == TEAM_BLUE ? 2 : 1;
                        bs->flagstatuschanged = qtrue;
                        break;
                }
            }
            break;
        }
        case EV_PLAYER_TELEPORT_IN: {
            VectorCopy(state->origin, lastteleport_origin);
            lastteleport_time = FloatTime();
            break;
        }
        case EV_GENERAL_SOUND: {
            if (state->number == bs->client) {
                if (state->eventParm < 0 || state->eventParm >= MAX_SOUNDS) {
                    BotAI_Print(PRT_ERROR, "EV_GENERAL_SOUND: eventParm (%d) out of range\n", state->eventParm);
                    break;
                }
                trap_GetConfigstring(CS_SOUNDS + state->eventParm, buf, sizeof(buf));
                if (!strcmp(buf, "*falling1.wav")) {
                    if (bs->inventory[INVENTORY_TELEPORTER] > 0) {
                        trap_EA_Use(bs->client);
                    }
                }
            }
            break;
        }
        case EV_USE_ITEM0:
        case EV_USE_ITEM1:
        case EV_USE_ITEM2:
        case EV_USE_ITEM3:
        case EV_USE_ITEM4:
        case EV_USE_ITEM5:
        case EV_USE_ITEM6:
        case EV_USE_ITEM7:
        case EV_USE_ITEM8:
        case EV_USE_ITEM9:
        case EV_USE_ITEM10:
        case EV_USE_ITEM11:
        case EV_USE_ITEM12:
        case EV_USE_ITEM13:
        case EV_USE_ITEM14:
        case EV_USE_ITEM15:
            break;
    }
}

void BotCheckSnapshot(bot_state_t* bs) {
    int ent;
    entityState_t state;

    trap_BotAddAvoidSpot(bs->ms, vec3_origin, 0, AVOID_CLEAR);
    bs->kamikazebody = 0;
    bs->numproxmines = 0;
    ent = 0;
    while ((ent = BotAI_GetSnapshotEntity(bs->client, ent, &state)) != -1) {
        BotCheckEvents(bs, &state);
        BotCheckForGrenades(bs, &state);
        BotCheckForProxMines(bs, &state);
        BotCheckForKamikazeBody(bs, &state);
    }
    BotAI_GetEntityState(bs->client, &state);
    state.event = bs->cur_ps.externalEvent;
    state.eventParm = bs->cur_ps.externalEventParm;
    BotCheckEvents(bs, &state);
}

void BotCheckAir(bot_state_t* bs) {
    if (bs->inventory[INVENTORY_ENVIRONMENTSUIT] <= 0) {
        if (trap_AAS_PointContents(bs->eye) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
            return;
        }
    }
    bs->lastair_time = FloatTime();
}

bot_goal_t* BotAlternateRoute(bot_state_t* bs, bot_goal_t* goal) {
    int t;

    if (bs->altroutegoal.areanum) {
        if (bs->reachedaltroutegoal_time)
            return goal;
        t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, bs->altroutegoal.areanum, bs->tfl);
        if (t && t < 20) {
            bs->reachedaltroutegoal_time = FloatTime();
        }
        memcpy(goal, &bs->altroutegoal, sizeof(bot_goal_t));
        return &bs->altroutegoal;
    }
    return goal;
}

int BotGetAlternateRouteGoal(bot_state_t* bs, int base) {
    aas_altroutegoal_t* altroutegoals;
    bot_goal_t* goal;
    int numaltroutegoals, rnd;

    if (base == TEAM_RED) {
        altroutegoals = red_altroutegoals;
        numaltroutegoals = red_numaltroutegoals;
    } else {
        altroutegoals = blue_altroutegoals;
        numaltroutegoals = blue_numaltroutegoals;
    }
    if (!numaltroutegoals)
        return qfalse;
    rnd = (float)random() * numaltroutegoals;
    if (rnd >= numaltroutegoals)
        rnd = numaltroutegoals - 1;
    goal = &bs->altroutegoal;
    goal->areanum = altroutegoals[rnd].areanum;
    VectorCopy(altroutegoals[rnd].origin, goal->origin);
    VectorSet(goal->mins, -8, -8, -8);
    VectorSet(goal->maxs, 8, 8, 8);
    goal->entitynum = 0;
    goal->iteminfo = 0;
    goal->number = 0;
    goal->flags = 0;
    bs->reachedaltroutegoal_time = 0;
    return qtrue;
}

void BotSetupAlternativeRouteGoals(void) {
    if (altroutegoals_setup)
        return;
    if (gametype == GT_CTF || gametype == GT_AD) {
        if (trap_BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
            BotAI_Print(PRT_WARNING, "no alt routes without Neutral Flag\n");
        if (ctf_neutralflag.areanum) {
            red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                ctf_neutralflag.origin, ctf_neutralflag.areanum,
                ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
                red_altroutegoals, MAX_ALTROUTEGOALS,
                ALTROUTEGOAL_CLUSTERPORTALS |
                    ALTROUTEGOAL_VIEWPORTALS);
            blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                ctf_neutralflag.origin, ctf_neutralflag.areanum,
                ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
                blue_altroutegoals, MAX_ALTROUTEGOALS,
                ALTROUTEGOAL_CLUSTERPORTALS |
                    ALTROUTEGOAL_VIEWPORTALS);
        }
    } else if (gametype == GT_1FCTF) {
        red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
            ctf_neutralflag.origin, ctf_neutralflag.areanum,
            ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
            red_altroutegoals, MAX_ALTROUTEGOALS,
            ALTROUTEGOAL_CLUSTERPORTALS |
                ALTROUTEGOAL_VIEWPORTALS);
        blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
            ctf_neutralflag.origin, ctf_neutralflag.areanum,
            ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
            blue_altroutegoals, MAX_ALTROUTEGOALS,
            ALTROUTEGOAL_CLUSTERPORTALS |
                ALTROUTEGOAL_VIEWPORTALS);
    } else if (gametype == GT_OBELISK) {
        if (trap_BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
            BotAI_Print(PRT_WARNING, "Harvester without neutral obelisk\n");
        red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
            neutralobelisk.origin, neutralobelisk.areanum,
            redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
            red_altroutegoals, MAX_ALTROUTEGOALS,
            ALTROUTEGOAL_CLUSTERPORTALS |
                ALTROUTEGOAL_VIEWPORTALS);
        blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
            neutralobelisk.origin, neutralobelisk.areanum,
            blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
            blue_altroutegoals, MAX_ALTROUTEGOALS,
            ALTROUTEGOAL_CLUSTERPORTALS |
                ALTROUTEGOAL_VIEWPORTALS);
    } else if (gametype == GT_HARVESTER) {
        red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
            neutralobelisk.origin, neutralobelisk.areanum,
            redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
            red_altroutegoals, MAX_ALTROUTEGOALS,
            ALTROUTEGOAL_CLUSTERPORTALS |
                ALTROUTEGOAL_VIEWPORTALS);
        blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
            neutralobelisk.origin, neutralobelisk.areanum,
            blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
            blue_altroutegoals, MAX_ALTROUTEGOALS,
            ALTROUTEGOAL_CLUSTERPORTALS |
                ALTROUTEGOAL_VIEWPORTALS);
    }
    altroutegoals_setup = qtrue;
}

void BotDeathmatchAI(bot_state_t* bs, float thinktime) {
    char gender[144], name[144], buf[144];
    char userinfo[MAX_INFO_STRING];
    int i;

    if (bs->setupcount > 0) {
        bs->setupcount--;
        if (bs->setupcount > 0)
            return;
        trap_Characteristic_String(bs->character, CHARACTERISTIC_GENDER, gender, sizeof(gender));
        trap_GetUserinfo(bs->client, userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "sex", gender);
        trap_SetUserinfo(bs->client, userinfo);
        if (!bs->map_restart && gametype != GT_DUEL) {
            Com_sprintf(buf, sizeof(buf), "team %s", bs->settings.team);
            trap_EA_Command(bs->client, buf);
        }
        if (gender[0] == 'm')
            trap_BotSetChatGender(bs->cs, CHAT_GENDERMALE);
        else if (gender[0] == 'f')
            trap_BotSetChatGender(bs->cs, CHAT_GENDERFEMALE);
        else
            trap_BotSetChatGender(bs->cs, CHAT_GENDERLESS);
        ClientName(bs->client, name, sizeof(name));
        trap_BotSetChatName(bs->cs, name, bs->client);
        bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];
        bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
        bs->setupcount = 0;
        BotSetupAlternativeRouteGoals();
    }
    bs->flags &= ~BFL_IDEALVIEWSET;
    if (!BotIntermission(bs)) {
        BotSetTeleportTime(bs);
        BotUpdateInventory(bs);
        BotCheckSnapshot(bs);
        BotCheckAir(bs);
    }
    BotCheckConsoleMessages(bs);
    if (!BotIntermission(bs) && !BotIsObserver(bs)) {
        BotTeamAI(bs);
    }
    if (!BotIntermission(bs) && !BotIsObserver(bs) && !BotIsDead(bs)) {
        if (g_advancedTraining.integer == 1) {
            if (bs->ltgtype != LTG_DODGETRAINER) {
                AIEnter_DodgeTrainer(bs);
            }
        } else if ((bot_training.integer == 1 || bot_training.integer == 2) && bs->ltgtype != LTG_TRAINING) {
            AIEnter_Training(bs);
        } else if (bot_followMe.integer && bs->ltgtype != LTG_FOLLOWME) {
            AIEnter_FollowMe(bs);
        } else if (g_instaGib.integer && bs->ltgtype != 21 && bs->enemy == -1) {
            AIEnter_InstaGib(bs, "insta gib");
        }
    }
    if (!bs->ainode) {
        AIEnter_Seek_LTG(bs, "BotDeathmatchAI: no ai node");
    }
    if (!bs->entergamechat && bs->entergame_time > FloatTime() - 8) {
        if (BotChat_EnterGame(bs)) {
            bs->stand_time = FloatTime() + BotChatTime(bs);
            AIEnter_Stand(bs, "BotDeathmatchAI: chat enter game");
        }
        bs->entergamechat = qtrue;
    }
    BotResetNodeSwitches();
    for (i = 0; i < MAX_NODESWITCHES; i++) {
        if (bs->ainode(bs))
            break;
    }
    if (!bs->inuse)
        return;
    if (i >= MAX_NODESWITCHES) {
        trap_BotDumpGoalStack(bs->gs);
        trap_BotDumpAvoidGoals(bs->gs);
        BotDumpNodeSwitches(bs);
        ClientName(bs->client, name, sizeof(name));
        BotAI_Print(PRT_ERROR, "%s at %1.1f switched more than %d AI nodes\n", name, FloatTime(), MAX_NODESWITCHES);
    }
    bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];
    bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
}

void BotSetEntityNumForGoalWithModel(bot_goal_t* goal, int eType, char* modelname) {
    gentity_t* ent;
    int i, modelindex;
    vec3_t dir;

    modelindex = G_ModelIndex(modelname);
    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (eType && ent->s.eType != eType) {
            continue;
        }
        if (ent->s.modelindex != modelindex) {
            continue;
        }
        VectorSubtract(goal->origin, ent->s.origin, dir);
        if (VectorLengthSquared(dir) < Square(10)) {
            goal->entitynum = i;
            return;
        }
    }
}

void BotSetEntityNumForGoal(bot_goal_t* goal, char* classname) {
    gentity_t* ent;
    int i;
    vec3_t dir;

    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (Q_stricmp(ent->classname, classname) != 0) {
            continue;
        }
        VectorSubtract(goal->origin, ent->s.origin, dir);
        if (VectorLengthSquared(dir) < Square(10)) {
            goal->entitynum = i;
            return;
        }
    }
}

void BotSetEntityNumForGoalWithActivator(bot_goal_t* goal, char* classname) {
    gentity_t* ent;
    int i;
    vec3_t dir;

    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse || !ent->activator) {
            continue;
        }
        if (Q_stricmp(ent->activator->classname, classname) != 0) {
            continue;
        }
        VectorSubtract(goal->origin, ent->s.origin, dir);
        if (VectorLengthSquared(dir) < Square(10)) {
            goal->entitynum = i;
            return;
        }
    }
}

int BotGoalForBSPEntity(char* classname, bot_goal_t* goal) {
    char value[MAX_INFO_STRING];
    vec3_t origin, start, end;
    int ent, numareas, areas[10];

    memset(goal, 0, sizeof(bot_goal_t));
    for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
        if (!trap_AAS_ValueForBSPEpairKey(ent, "classname", value, sizeof(value)))
            continue;
        if (!strcmp(value, classname)) {
            if (!trap_AAS_VectorForBSPEpairKey(ent, "origin", origin))
                return qfalse;
            VectorCopy(origin, goal->origin);
            VectorCopy(origin, start);
            start[2] -= 32;
            VectorCopy(origin, end);
            end[2] += 32;
            numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
            if (!numareas)
                return qfalse;
            goal->areanum = areas[0];
            return qtrue;
        }
    }
    return qfalse;
}

void BotSetupDeathmatchAI(void) {
    int ent, modelnum;
    char model[128];

    gametype = trap_Cvar_VariableIntegerValue("g_gametype");

    trap_Cvar_Register(&bot_rocketjump, "bot_rocketjump", "1", 0);
    trap_Cvar_Register(&bot_grapple, "bot_grapple", "0", 0);
    trap_Cvar_Register(&bot_fastchat, "bot_fastchat", "0", 0);
    trap_Cvar_Register(&bot_nochat, "bot_nochat", "0", CVAR_USERSAVE);
    trap_Cvar_Register(&bot_testrchat, "bot_testrchat", "0", 0);
    trap_Cvar_Register(&bot_challenge, "bot_challenge", "0", CVAR_USERSAVE);
    trap_Cvar_Register(&bot_predictobstacles, "bot_predictobstacles", "1", 0);
    trap_Cvar_Register(&g_spSkill, "g_spSkill", "2", 0);
    if (gametype == GT_CTF || gametype == GT_AD) {
        if (trap_BotGetLevelItemGoal(-1, "Red Flag", &ctf_redflag) < 0)
            BotAI_Print(PRT_WARNING, "CTF without Red Flag\n");
        if (trap_BotGetLevelItemGoal(-1, "Blue Flag", &ctf_blueflag) < 0)
            BotAI_Print(PRT_WARNING, "CTF without Blue Flag\n");
    } else if (gametype == GT_1FCTF) {
        if (trap_BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
            BotAI_Print(PRT_WARNING, "One Flag CTF without Neutral Flag\n");
        if (trap_BotGetLevelItemGoal(-1, "Red Flag", &ctf_redflag) < 0)
            BotAI_Print(PRT_WARNING, "CTF without Red Flag\n");
        if (trap_BotGetLevelItemGoal(-1, "Blue Flag", &ctf_blueflag) < 0)
            BotAI_Print(PRT_WARNING, "CTF without Blue Flag\n");
    } else if (gametype == GT_OBELISK) {
        if (trap_BotGetLevelItemGoal(-1, "Red Obelisk", &redobelisk) < 0)
            BotAI_Print(PRT_WARNING, "Obelisk without red obelisk\n");
        BotSetEntityNumForGoalWithActivator(&redobelisk, "team_redobelisk");
        if (trap_BotGetLevelItemGoal(-1, "Blue Obelisk", &blueobelisk) < 0)
            BotAI_Print(PRT_WARNING, "Obelisk without blue obelisk\n");
        BotSetEntityNumForGoalWithActivator(&blueobelisk, "team_blueobelisk");
    } else if (gametype == GT_HARVESTER) {
        if (trap_BotGetLevelItemGoal(-1, "Red Obelisk", &redobelisk) < 0)
            BotAI_Print(PRT_WARNING, "Harvester without red obelisk\n");
        BotSetEntityNumForGoalWithActivator(&redobelisk, "team_redobelisk");
        if (trap_BotGetLevelItemGoal(-1, "Blue Obelisk", &blueobelisk) < 0)
            BotAI_Print(PRT_WARNING, "Harvester without blue obelisk\n");
        BotSetEntityNumForGoalWithActivator(&blueobelisk, "team_blueobelisk");
        if (trap_BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
            BotAI_Print(PRT_WARNING, "Harvester without neutral obelisk\n");
        BotSetEntityNumForGoalWithActivator(&neutralobelisk, "team_neutralobelisk");
    }

    max_bspmodelindex = 0;
    for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
        if (!trap_AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model)))
            continue;
        if (model[0] == '*') {
            modelnum = atoi(model + 1);
            if (modelnum > max_bspmodelindex)
                max_bspmodelindex = modelnum;
        }
    }
    BotInitWaypoints();
}

void BotShutdownDeathmatchAI(void) {
    altroutegoals_setup = qfalse;
}
