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
#include "cg_local.h"

static pmove_t cg_pmove;

static int cg_numSolidEntities;
static centity_t* cg_solidEntities[MAX_ENTITIES_IN_SNAPSHOT];
static int cg_numTriggerEntities;
static centity_t* cg_triggerEntities[MAX_ENTITIES_IN_SNAPSHOT];

static int cg_numSkipEntities;
static int cg_skipEntities[4];

void CG_BuildSolidList(void) {
    int i;
    centity_t* cent;
    snapshot_t* snap;
    entityState_t* ent;

    cg_numSolidEntities = 0;
    cg_numTriggerEntities = 0;
    cg_numSkipEntities = 0;

    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        snap = cg.nextSnap;
    } else {
        snap = cg.snap;
    }

    for (i = 0; i < snap->numEntities; i++) {
        cent = &cg_entities[snap->entities[i].number];
        ent = &cent->currentState;

        if (ent->eType == ET_ITEM || ent->eType == ET_PUSH_TRIGGER || ent->eType == ET_TELEPORT_TRIGGER) {
            cg_triggerEntities[cg_numTriggerEntities] = cent;
            cg_numTriggerEntities++;
            continue;
        }

        if (cent->nextState.solid) {
            cg_solidEntities[cg_numSolidEntities] = cent;
            cg_numSolidEntities++;
            continue;
        }
    }
}

static void CG_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask, trace_t* tr, qboolean capsule) {
    int i, x, zd, zu;
    trace_t trace;
    entityState_t* ent;
    clipHandle_t cmodel;
    vec3_t bmins, bmaxs;
    vec3_t origin, angles;
    centity_t* cent;

    for (i = 0; i < cg_numSolidEntities; i++) {
        cent = cg_solidEntities[i];
        ent = &cent->currentState;

        if (ent->number == skipNumber) {
            continue;
        }

        {
            int j;
            qboolean skip = qfalse;
            for (j = 0; j < cg_numSkipEntities; j++) {
                if (ent->number == cg_skipEntities[j]) {
                    skip = qtrue;
                    break;
                }
            }
            if (skip) {
                continue;
            }
        }

        if (ent->solid == SOLID_BMODEL) {
            cmodel = trap_CM_InlineModel(ent->modelindex);
            VectorCopy(cent->lerpAngles, angles);
            BG_EvaluateTrajectory(&cent->currentState.pos, cg.physicsTime, origin);
        } else {
            x = (ent->solid & 255);
            zd = ((ent->solid >> 8) & 255);
            zu = ((ent->solid >> 16) & 255) - 32;

            bmins[0] = bmins[1] = -x;
            bmaxs[0] = bmaxs[1] = x;
            bmins[2] = -zd;
            bmaxs[2] = zu;

            cmodel = capsule ? trap_CM_TempCapsuleModel(bmins, bmaxs)
                             : trap_CM_TempBoxModel(bmins, bmaxs);
            VectorCopy(vec3_origin, angles);
            VectorCopy(cent->lerpOrigin, origin);
        }

        if (capsule) {
            trap_CM_TransformedCapsuleTrace(&trace, start, end,
                                            mins, maxs, cmodel, mask, origin, angles);
        } else {
            trap_CM_TransformedBoxTrace(&trace, start, end,
                                        mins, maxs, cmodel, mask, origin, angles);
        }

        if (trace.allsolid || trace.fraction < tr->fraction) {
            trace.entityNum = ent->number;
            *tr = trace;
        } else if (trace.startsolid) {
            tr->startsolid = qtrue;
        }
        if (tr->allsolid) {
            return;
        }
    }
}

void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask) {
    trace_t t;

    trap_CM_BoxTrace(&t, start, end, mins, maxs, 0, mask);
    t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
    CG_ClipMoveToEntities(start, mins, maxs, end, skipNumber, mask, &t, qfalse);

    *result = t;
}

void CG_CapsuleTrace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask) {
    trace_t t;

    trap_CM_CapsuleTrace(&t, start, end, mins, maxs, 0, mask);
    t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
    CG_ClipMoveToEntities(start, mins, maxs, end, skipNumber, mask, &t, qtrue);

    *result = t;
}

int CG_PointContents(const vec3_t point, int passEntityNum) {
    int i;
    entityState_t* ent;
    centity_t* cent;
    clipHandle_t cmodel;
    int contents;

    contents = trap_CM_PointContents(point, 0);

    for (i = 0; i < cg_numSolidEntities; i++) {
        cent = cg_solidEntities[i];

        ent = &cent->currentState;

        if (ent->number == passEntityNum) {
            continue;
        }

        if (ent->solid != SOLID_BMODEL) {
            continue;
        }

        cmodel = trap_CM_InlineModel(ent->modelindex);
        if (!cmodel) {
            continue;
        }

        contents |= trap_CM_TransformedPointContents(point, cmodel, cent->lerpOrigin, cent->lerpAngles);
    }

    return contents;
}

static void CG_InterpolatePlayerState(qboolean grabAngles) {
    float f;
    int i;
    playerState_t* out;
    snapshot_t *prev, *next;

    out = &cg.predictedPlayerState;
    prev = cg.snap;
    next = cg.nextSnap;

    *out = cg.snap->ps;

    if (grabAngles) {
        usercmd_t cmd;
        int cmdNum;

        cmdNum = trap_GetCurrentCmdNumber();
        trap_GetUserCmd(cmdNum, &cmd);

        PM_UpdateViewAngles(out, &cmd);
    }

    if (cg.nextFrameTeleport) {
        return;
    }

    if (!next || next->serverTime <= prev->serverTime) {
        return;
    }

    f = (float)(cg.time - prev->serverTime) / (next->serverTime - prev->serverTime);

    i = next->ps.bobCycle;
    if (i < prev->ps.bobCycle) {
        i += 256;
    }
    out->bobCycle = prev->ps.bobCycle + f * (i - prev->ps.bobCycle);

    for (i = 0; i < 3; i++) {
        out->origin[i] = prev->ps.origin[i] + f * (next->ps.origin[i] - prev->ps.origin[i]);
        if (!grabAngles) {
            out->viewangles[i] = LerpAngle(
                prev->ps.viewangles[i], next->ps.viewangles[i], f);
        }
        out->velocity[i] = prev->ps.velocity[i] +
                           f * (next->ps.velocity[i] - prev->ps.velocity[i]);
    }
}

static void CG_TouchItem(centity_t* cent) {
    gitem_t* item;

    if (!cg_predictItems.integer || cg.warmup > 0) {
        return;
    }
    if (!BG_PlayerTouchesItem(&cg.predictedPlayerState, &cent->currentState, cg.time, cgs.itemHeight)) {
        return;
    }

    if (cent->miscTime == cg.time) {
        return;
    }

    if (!BG_CanItemBeGrabbed(cg.time, cg.warmup, cgs.armorTiered, cgs.weaponRespawn,
                             cgs.gametype, &cent->currentState, &cg.predictedPlayerState)) {
        return;
    }

    item = &bg_itemlist[cent->currentState.modelindex];

    if (item->giType == IT_TEAM && item->giTag >= PW_REDFLAG && item->giTag <= PW_NEUTRALFLAG) {
        return;
    }
    if (item->itemTimer) {
        return;
    }

    BG_AddPredictableEventToPlayerstate(EV_ITEM_PICKUP, cent->currentState.modelindex, &cg.predictedPlayerState);

    cent->currentState.eFlags |= EF_NODRAW;

    cent->miscTime = cg.time;

    if (item->giType == IT_WEAPON) {
        cg.predictedPlayerState.stats[STAT_WEAPONS] |= 1 << item->giTag;
        if (!cg.predictedPlayerState.ammo[item->giTag]) {
            cg.predictedPlayerState.ammo[item->giTag] = item->quantity;
        }
    }
}

void CG_UpdateStepChange(void) {
    int timeDelta;
    float oldChange;

    if (cg_pmove.stepHeight == 0.0f || cg.stepTime >= cg_pmove.stepTime) {
        return;
    }
    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    timeDelta = cg.time - cg.stepTime;
    if (timeDelta < STEP_TIME) {
        oldChange = cg.stepChange * (float)(STEP_TIME - timeDelta) / (float)STEP_TIME;
    } else {
        oldChange = 0.0f;
    }

    cg.stepChange = cg_pmove.stepHeight + oldChange;
    if (cg.stepChange > MAX_STEP_CHANGE) {
        cg.stepChange = MAX_STEP_CHANGE;
    }
    cg.stepTime = cg.time;
}

static void CG_TouchTriggerPrediction(void) {
    int i;
    trace_t trace;
    entityState_t* ent;
    clipHandle_t cmodel;
    centity_t* cent;
    qboolean spectator;

    if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        return;
    }

    spectator = (cg.predictedPlayerState.pm_type == PM_SPECTATOR);

    if (cg.predictedPlayerState.pm_type != PM_NORMAL && !spectator) {
        return;
    }

    for (i = 0; i < cg_numTriggerEntities; i++) {
        cent = cg_triggerEntities[i];
        ent = &cent->currentState;

        if (ent->eType == ET_ITEM && !spectator) {
            CG_TouchItem(cent);
            continue;
        }

        if (ent->solid != SOLID_BMODEL) {
            continue;
        }

        cmodel = trap_CM_InlineModel(ent->modelindex);
        if (!cmodel) {
            continue;
        }

        trap_CM_BoxTrace(&trace, cg.predictedPlayerState.origin, cg.predictedPlayerState.origin,
                         cg_pmove.mins, cg_pmove.maxs, cmodel, -1);

        if (!trace.startsolid) {
            continue;
        }

        if (ent->eType == ET_TELEPORT_TRIGGER) {
            cg.hyperspace = qtrue;
        } else if (ent->eType == ET_PUSH_TRIGGER) {
            BG_TouchJumpPad(&cg.predictedPlayerState, ent);
        }
    }

    if (cg.predictedPlayerState.jumppad_frame != cg.predictedPlayerState.pmove_framecount) {
        cg.predictedPlayerState.jumppad_frame = 0;
        cg.predictedPlayerState.jumppad_ent = 0;
    }
}

static void CG_CheckAutoFire(void) {
    usercmd_t cmd;
    int cmdNum;
    vec3_t forward, right, up;
    vec3_t muzzle, end, start;
    trace_t trace;
    int clientNum;

    if (cg.predictedPlayerState.eFlags & EF_SPECTATOR) {
        return;
    }

    if (cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
        return;
    }

    if (cg.predictedPlayerState.pm_flags & PMF_FROZEN) {
        if (!cg.nextSnap || (cg.nextSnap->ps.pm_flags & PMF_FROZEN)) {
            return;
        }
        cg.predictedPlayerState.weaponTime = 0;
    }

    if (cg.predictedPlayerState.pm_flags & PMF_RESPAWNED) {
        return;
    }

    cmdNum = trap_GetCurrentCmdNumber();
    trap_GetUserCmd(cmdNum, &cmd);

    if (cmd.buttons & BUTTON_TALK) {
        return;
    }

    if (atoi(CG_ConfigString(CS_PAUSE_START_TIME)) != 0 ||
        atoi(CG_ConfigString(CS_PAUSE_END_TIME)) != 0) {
        return;
    }

    cg.predictedPlayerState.weaponTime -= cg.frametime;

    if ((cmd.buttons & BUTTON_ATTACK) &&
        cg.predictedPlayerState.weapon == WP_RAILGUN &&
        cg.predictedPlayerState.weaponTime < 1 &&
        cmd.weapon == WP_RAILGUN &&
        (cg.predictedPlayerState.ammo[WP_RAILGUN] > 0 ||
         cg.predictedPlayerState.ammo[WP_RAILGUN] == -1) &&
        cg.time - cg.lastAutoFireTime > 100) {

        clientNum = cg.predictedPlayerEntity.currentState.clientNum;

        AngleVectors(cg.predictedPlayerState.viewangles, forward, right, up);

        muzzle[0] = cg.predictedPlayerState.origin[0] + forward[0] * 5.0f;
        muzzle[1] = cg.predictedPlayerState.origin[1] + forward[1] * 5.0f;
        muzzle[2] = cg.predictedPlayerState.origin[2] +
                    cg.predictedPlayerState.viewheight + forward[2] * 5.0f;

        end[0] = muzzle[0] + forward[0] * 8192.0f;
        end[1] = muzzle[1] + forward[1] * 8192.0f;
        end[2] = muzzle[2] + forward[2] * 8192.0f;

        do {
            if (cgs.playerCylinders) {
                CG_CapsuleTrace(&trace, muzzle, NULL, NULL, end,
                                cg.predictedPlayerEntity.currentState.number,
                                CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE);
            } else {
                CG_Trace(&trace, muzzle, NULL, NULL, end,
                         cg.predictedPlayerEntity.currentState.number,
                         CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE);
            }

            if (trace.entityNum >= ENTITYNUM_MAX_NORMAL ||
                (trace.contents & CONTENTS_SOLID)) {
                break;
            }

            cg_skipEntities[cg_numSkipEntities] = trace.entityNum;
            cg_numSkipEntities++;
        } while (cg_numSkipEntities < 4);

        cg_numSkipEntities = 0;

        start[0] = muzzle[0] + right[0] * 4.0f - up[0];
        start[1] = muzzle[1] + right[1] * 4.0f - up[1];
        start[2] = muzzle[2] + right[2] * 4.0f - up[2];

        CG_RailTrail(&cgs.clientinfo[clientNum], start, trace.endpos);

        if (!(trace.surfaceFlags & SURF_NOIMPACT)) {
            CG_MissileHitWall(cg.predictedPlayerState.weapon,
                              cg.predictedPlayerEntity.currentState.clientNum,
                              trace.endpos, trace.plane.normal, IMPACTSOUND_DEFAULT);
        }

        cg.lastAutoFireTime = cg.time;
    }
}

void CG_PredictPlayerState(void) {
    int cmdNum, current;
    playerState_t oldPlayerState;
    qboolean moved;
    usercmd_t oldestCmd;
    usercmd_t latestCmd;
    int following;

    cg.hyperspace = qfalse;

    if (!cg.validPPS) {
        cg.validPPS = qtrue;
        cg.predictedPlayerState = cg.snap->ps;
    }

    following = (cg.predictedPlayerState.pm_flags & PMF_FOLLOW) != 0;
    if (cg.reloadFollowing != following ||
        cg_playerTeam.integer != cg.predictedPlayerState.persistant[PERS_TEAM]) {
        cg.reloadOtherClients = qtrue;
    }
    cg.reloadFollowing = following;
    if (following && cg.reloadFollowedClient != cg.predictedPlayerState.clientNum) {
        cg.reloadFollowedClient = cg.predictedPlayerState.clientNum;
        cg.reloadOtherClients = qtrue;
    }
    cg_playerTeam.integer = cg.predictedPlayerState.persistant[PERS_TEAM];

    if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW)) {
        CG_InterpolatePlayerState(qfalse);
        return;
    }

    if (cg_nopredict.integer) {
        CG_InterpolatePlayerState(qtrue);
        return;
    }

    cg_pmove.ps = &cg.predictedPlayerState;
    cg_pmove.trace = CG_Trace;
    cg_pmove.pointcontents = CG_PointContents;
    if (cg_pmove.ps->pm_type == PM_DEAD) {
        cg_pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
    } else {
        cg_pmove.tracemask = MASK_PLAYERSOLID;
    }
    if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        cg_pmove.tracemask &= ~CONTENTS_BODY;
    }
    cg_pmove.noFootsteps = (cgs.dmflags & DF_NO_FOOTSTEPS) > 0;
    cg_pmove.retailBobCycle = !cgs.ioqlServer;

    if (cg_autoHop.integer == 0) {
        cg.predictedPlayerState.pm_flags |= PMF_NO_AUTOHOP;
    } else {
        cg.predictedPlayerState.pm_flags &= ~PMF_NO_AUTOHOP;
    }

    oldPlayerState = cg.predictedPlayerState;

    current = trap_GetCurrentCmdNumber();

    cmdNum = current - CMD_BACKUP + 1;
    trap_GetUserCmd(cmdNum, &oldestCmd);
    if (oldestCmd.serverTime > cg.snap->ps.commandTime && oldestCmd.serverTime < cg.time) {
        if (cg_showmiss.integer) {
            CG_Printf("exceeded PACKET_BACKUP on commands\n");
        }
        return;
    }

    trap_GetUserCmd(current, &latestCmd);

    if (cg_predictLocalRailshots.integer) {
        CG_CheckAutoFire();
    }

    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        cg.predictedPlayerState = cg.nextSnap->ps;
        cg.physicsTime = cg.nextSnap->serverTime;
    } else {
        cg.predictedPlayerState = cg.snap->ps;
        cg.physicsTime = cg.snap->serverTime;
    }

    moved = qfalse;
    for (cmdNum = current - CMD_BACKUP + 1; cmdNum <= current; cmdNum++) {
        trap_GetUserCmd(cmdNum, &cg_pmove.cmd);

        if (cg_pmove.cmd.serverTime <= cg.predictedPlayerState.commandTime) {
            continue;
        }

        if (cg_pmove.cmd.serverTime > latestCmd.serverTime) {
            continue;
        }

        if (cg.predictedPlayerState.commandTime == oldPlayerState.commandTime) {
            vec3_t delta;
            float len;

            if (cg.thisFrameTeleport) {
                VectorClear(cg.predictedError);
                if (cg_showmiss.integer) {
                    CG_Printf("PredictionTeleport\n");
                }
                cg.thisFrameTeleport = qfalse;
            } else {
                vec3_t adjusted, new_angles;
                CG_AdjustPositionForMover(cg.predictedPlayerState.origin,
                                          cg.predictedPlayerState.groundEntityNum, cg.physicsTime, cg.oldTime, adjusted, cg.predictedPlayerState.viewangles, new_angles);

                if (cg_showmiss.integer) {
                    if (!VectorCompare(oldPlayerState.origin, adjusted)) {
                        CG_Printf("prediction error\n");
                    }
                }
                VectorSubtract(oldPlayerState.origin, adjusted, delta);
                len = VectorLength(delta);
                if (len > 0.1) {
                    if (cg_showmiss.integer) {
                        CG_Printf("Prediction miss: %f\n", len);
                    }
                    if (cg_errorDecay.integer) {
                        int t;
                        float f;

                        t = cg.time - cg.predictedErrorTime;
                        f = (cg_errorDecay.value - t) / cg_errorDecay.value;
                        if (f < 0) {
                            f = 0;
                        }
                        if (f > 0 && cg_showmiss.integer) {
                            CG_Printf("Double prediction decay: %f\n", f);
                        }
                        VectorScale(cg.predictedError, f, cg.predictedError);
                    } else {
                        VectorClear(cg.predictedError);
                    }
                    VectorAdd(delta, cg.predictedError, cg.predictedError);
                    cg.predictedErrorTime = cg.oldTime;
                }
            }
        }

        cg_pmove.gauntletHit = qfalse;

        Pmove(&cg_pmove);

        moved = qtrue;

        CG_UpdateStepChange();

        CG_TouchTriggerPrediction();

    }

    if (cg_showmiss.integer > 1) {
        CG_Printf("[%i : %i] ", cg_pmove.cmd.serverTime, cg.time);
    }

    if (!moved) {
        if (cg_showmiss.integer) {
            CG_Printf("not moved\n");
        }
        return;
    }

    CG_AdjustPositionForMover(cg.predictedPlayerState.origin,
                              cg.predictedPlayerState.groundEntityNum,
                              cg.physicsTime, cg.time, cg.predictedPlayerState.origin, cg.predictedPlayerState.viewangles, cg.predictedPlayerState.viewangles);

    if (cg_showmiss.integer) {
        if (cg.predictedPlayerState.eventSequence > oldPlayerState.eventSequence + MAX_PS_EVENTS) {
            CG_Printf("WARNING: dropped event\n");
        }
    }

    CG_TransitionPlayerState(&cg.predictedPlayerState, &oldPlayerState);

    if (cg_showmiss.integer) {
        if (cg.eventSequence > cg.predictedPlayerState.eventSequence) {
            CG_Printf("WARNING: double event\n");
            cg.eventSequence = cg.predictedPlayerState.eventSequence;
        }
    }
}

void CG_SpecAutoFollow(int clientNum, int mode) {
    const char* suffix;

    if (!cg_followPowerup.integer) {
        return;
    }
    if (cg.snap->ps.pm_type != PM_SPECTATOR) {
        return;
    }
    if (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR) {
        return;
    }

    if (cg_followPowerup.integer == 2 && mode == 0) {
        suffix = " pw";
    } else {
        suffix = "";
    }

    trap_SendConsoleCommand(va("follow %d%s", clientNum, suffix));
}

void CG_SpecFollowKiller(int clientNum) {
    if (!ch_followKiller.integer || clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    if (!cg.snap || cg.snap->ps.pm_type != PM_SPECTATOR) {
        return;
    }
    if (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR) {
        return;
    }
    if (cg.time < cg.followKillerTime + 2000) {
        return;
    }
    cg.followKillerTime = cg.time;
    trap_SendConsoleCommand(va("follow %d\n", clientNum));
}
