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
#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

pmove_t* pm;
pml_t pml;

float pm_stopspeed = 100.0f;
float pm_duckScale = 0.25f;
float pm_swimScale = 0.50f;

float pm_accelerate = 10.0f;
float pm_airaccelerate = 1.0f;
float pm_wateraccelerate = 4.0f;
float pm_flyaccelerate = 8.0f;

float pm_friction = 6.0f;
float pm_waterfriction = 1.0f;
float pm_flightfriction = 3.0f;
float pm_spectatorfriction = 5.0f;

float pm_jumpVelocity = 275.0f;
float pm_jumpVelocityMax = 700.0f;
float pm_jumpVelocityScaleAdd = 0.4f;
float pm_jumpVelocityTimeThreshold = 500.0f;
float pm_jumpVelocityTimeThresholdOffset = 0.6f;
float pm_chainJumpVelocity = 110.0f;
float pm_stepJumpVelocity = 48.0f;
float pm_jumpTimeDeltaMin = 100.0f;
int pm_chainJump = 1;
int pm_rampJump = 0;
float pm_rampJumpScale = 1.0f;
int pm_stepJump = 0;
int pm_crouchStepJump = 1;
int pm_autoHop = 1;
int pm_bunnyHop = 1;
float pm_bunnyHopOveraccel = 0.55f;
float pm_airAccel = 1.0f;
float pm_airStopAccel = 1.0f;
float pm_airControl = 0.0f;
float pm_strafeAccel = 1.0f;
float pm_walkAccel = 10.0f;
float pm_walkFriction = 6.0f;
float pm_circleStrafeFriction = 6.0f;
float pm_crouchSlideFriction = 0.5f;
int pm_crouchSlideTime = 2000;
int pm_airSteps = 1;
float pm_airStepFriction = 0.03f;
float pm_stepHeight = 22.0f;
float pm_waterSwimScale = 0.559f;
float pm_waterWadeScale = 0.75f;
float pm_wishSpeed = 400.0f;
int pm_weaponDropTime = 200;
int pm_weaponRaiseTime = 200;
int pm_noPlayerClip = 0;
int pm_doubleJump = 0;
int pm_crouchSlide = 0;
float pm_velocityGH = 800.0f;

int c_pmove = 0;

void PM_SetAirAccel(float value)                      { pm_airAccel = value; }
void PM_SetAirControl(float value)                    { pm_airControl = value; }
void PM_SetAirStepFriction(float value)               { pm_airStepFriction = value; }
void PM_SetAirSteps(int value)                        { pm_airSteps = value; }
void PM_SetAirStopAccel(float value)                  { pm_airStopAccel = value; }
void PM_SetAutoHop(int value)                         { pm_autoHop = value; }
void PM_SetBunnyHop(int value)                        { pm_bunnyHop = value; }
void PM_SetChainJump(int value)                       { pm_chainJump = value; }
void PM_SetChainJumpVelocity(float value)             { pm_chainJumpVelocity = value; }
void PM_SetCircleStrafeFriction(float value)          { pm_circleStrafeFriction = value; }
void PM_SetCrouchSlideFriction(float value)           { pm_crouchSlideFriction = value; }
void PM_SetCrouchSlideTime(int value)                 { pm_crouchSlideTime = value; }
void PM_SetCrouchStepJump(int value)                  { pm_crouchStepJump = value; }
void PM_SetJumpTimeDeltaMin(float value)              { pm_jumpTimeDeltaMin = value; }
void PM_SetJumpVelocity(float value)                  { pm_jumpVelocity = value; }
void PM_SetJumpVelocityMax(float value)               { pm_jumpVelocityMax = value; }
void PM_SetJumpVelocityScaleAdd(float value)          { pm_jumpVelocityScaleAdd = value; }
void PM_SetJumpVelocityTimeThreshold(float value)     { pm_jumpVelocityTimeThreshold = value; }
void PM_SetJumpVelocityTimeThresholdOffset(float value) { pm_jumpVelocityTimeThresholdOffset = value; }
void PM_SetNoPlayerClip(int value)                    { pm_noPlayerClip = value; }
void PM_SetRampJump(int value)                        { pm_rampJump = value; }
void PM_SetRampJumpScale(float value)                 { pm_rampJumpScale = value; }
void PM_SetStepHeight(float value)                    { pm_stepHeight = value; }
void PM_SetStepJump(int value)                        { pm_stepJump = value; }
void PM_SetStepJumpVelocity(float value)              { pm_stepJumpVelocity = value; }
void PM_SetStrafeAccel(float value)                   { pm_strafeAccel = value; }
void PM_SetWalkAccel(float value)                     { pm_walkAccel = value; }
void PM_SetWalkFriction(float value)                  { pm_walkFriction = value; }
void PM_SetWaterSwimScale(float value)                { pm_waterSwimScale = value; }
void PM_SetWaterWadeScale(float value)                { pm_waterWadeScale = value; }
void PM_SetWeaponDropTime(int value)                  { pm_weaponDropTime = value; }
void PM_SetWeaponRaiseTime(int value)                 { pm_weaponRaiseTime = value; }
void PM_SetWishSpeed(float value)                     { pm_wishSpeed = value; }
void PM_SetDoubleJump(int value)                      { pm_doubleJump = value; }
void PM_SetCrouchSlide(int value)                     { pm_crouchSlide = value; }
void PM_SetVelocityGH(float value)                    { pm_velocityGH = value; }

void PM_SetMovementConfig(void) {
    if (pm->ps->pm_flags & PMF_PROMODE) {
        pm_airControl = 150.0f;
        pm_airAccel = 1.1f;
        pm_airStopAccel = 2.5f;
        pm_chainJump = 3;
        pm_circleStrafeFriction = 5.5f;
        pm_strafeAccel = 70.0f;
        pm_walkAccel = 15.0f;
        pm_wishSpeed = 35.0f;
    } else {
        pm_airControl = 0.0f;
        pm_airAccel = 1.0f;
        pm_airStopAccel = 1.0f;
        pm_strafeAccel = 1.0f;
        pm_chainJump = 1;
        pm_walkAccel = pm_accelerate;
        pm_circleStrafeFriction = pm_friction;
        pm_wishSpeed = 400.0f;
    }
}

void PM_AddEvent(int newEvent) {
    BG_AddPredictableEventToPlayerstate(newEvent, 0, pm->ps);
}

void PM_AddTouchEnt(int entityNum) {
    int i;

    if (entityNum == ENTITYNUM_WORLD) {
        return;
    }
    if (pm->numtouch == MAXTOUCH) {
        return;
    }

    for (i = 0; i < pm->numtouch; i++) {
        if (pm->touchents[i] == entityNum) {
            return;
        }
    }

    pm->touchents[pm->numtouch] = entityNum;
    pm->numtouch++;
}

static void PM_StartTorsoAnim(int anim) {
    if (pm->ps->pm_type >= PM_DEAD) {
        return;
    }
    pm->ps->torsoAnim = ((pm->ps->torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
}
static void PM_StartLegsAnim(int anim) {
    if (pm->ps->pm_type >= PM_DEAD) {
        return;
    }
    if (pm->ps->legsTimer > 0) {
        return;
    }
    pm->ps->legsAnim = ((pm->ps->legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
}

static void PM_ContinueLegsAnim(int anim) {
    if ((pm->ps->legsAnim & ~ANIM_TOGGLEBIT) == anim) {
        return;
    }
    if (pm->ps->legsTimer > 0) {
        return;
    }
    PM_StartLegsAnim(anim);
}

static void PM_ContinueTorsoAnim(int anim) {
    if ((pm->ps->torsoAnim & ~ANIM_TOGGLEBIT) == anim) {
        return;
    }
    if (pm->ps->torsoTimer > 0) {
        return;
    }
    PM_StartTorsoAnim(anim);
}

static void PM_ForceLegsAnim(int anim) {
    pm->ps->legsTimer = 0;
    PM_StartLegsAnim(anim);
}

void PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce) {
    float backoff;
    float change;
    int i;

    backoff = DotProduct(in, normal);

    if (backoff < 0) {
        backoff *= overbounce;
    } else {
        backoff /= overbounce;
    }

    for (i = 0; i < 3; i++) {
        change = normal[i] * backoff;
        out[i] = in[i] - change;
    }
}

static void PM_Friction(void) {
    vec3_t vec;
    float* vel;
    float speed, newspeed, control;
    float drop;

    vel = pm->ps->velocity;

    VectorCopy(vel, vec);
    if (pml.walking) {
        vec[2] = 0;
    }
    speed = VectorLength(vec);

    if (speed < 1) {
        vel[0] = 0;
        vel[1] = 0;
        return;
    }

    drop = 0;

    if (pm->waterlevel < 2 && pml.walking
        && !(pml.groundTrace.surfaceFlags & SURF_SLICK)
        && !(pm->ps->pm_flags & PMF_TIME_KNOCKBACK)) {
        control = speed < pm_stopspeed ? pm_stopspeed : speed;

        if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE)
            && pm->cmd.upmove < 0 && pm->ps->crouchSlideTime > 0) {
            drop += control * pm_crouchSlideFriction * pml.frametime;
        } else if ((pm->ps->pm_flags & PMF_PROMODE)
                   && pm->cmd.forwardmove != 0
                   && pm->cmd.rightmove != 0
                   && (pm->ps->movementDir & 0xFFFFFFF9) == 1) {
            drop += control * pm_circleStrafeFriction * pml.frametime;
        } else {
            drop += control * pm_walkFriction * pml.frametime;
        }
    }

    if (pm->waterlevel > 0 && !pml.ladder) {
        drop += speed * (float)pm->waterlevel * pml.frametime;
    }

    if (pm->ps->pm_type == PM_SPECTATOR) {
        drop += speed * pm_spectatorfriction * pml.frametime;
    }

    if (pm->ps->pm_type == PM_FREEZE || (pm->ps->pm_flags & PMF_HOOKED)) {
        drop *= 0.25f;
    }

    if (pml.ladder) {
        drop += speed * 6.0f * pml.frametime;
    }

    newspeed = speed - drop;
    if (newspeed < 0) {
        newspeed = 0;
    }
    newspeed /= speed;

    vel[0] *= newspeed;
    vel[1] *= newspeed;
    vel[2] *= newspeed;
}

static void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel) {
    float addspeed, accelspeed, currentspeed;
    float speed2d, maxspeed, maxspeed2;

    currentspeed = DotProduct(pm->ps->velocity, wishdir);
    addspeed = wishspeed - currentspeed;

    speed2d = sqrt(pm->ps->velocity[0] * pm->ps->velocity[0]
                 + pm->ps->velocity[1] * pm->ps->velocity[1]);
    maxspeed = (float)pm->ps->speed;
    maxspeed2 = maxspeed + maxspeed;

    if (addspeed > 0) {
        accelspeed = accel * pml.frametime * wishspeed;
        if (accelspeed > addspeed) {
            accelspeed = addspeed;
        }

        pm->ps->velocity[0] += accelspeed * wishdir[0];
        pm->ps->velocity[1] += accelspeed * wishdir[1];
        pm->ps->velocity[2] += accelspeed * wishdir[2];
        return;
    }

    if (pm_bunnyHop && pm->ps->movementDir == 0 && speed2d <= maxspeed2) {
        float overaccel = pm_bunnyHopOveraccel;
        if (speed2d > maxspeed) {
            if (speed2d > maxspeed2) {
                speed2d = maxspeed2;
            }
            overaccel = ((maxspeed2 - speed2d) / maxspeed) * 0.55f;
        }

        pm->ps->velocity[0] += overaccel * wishdir[0];
        pm->ps->velocity[1] += overaccel * wishdir[1];
        pm->ps->velocity[2] += overaccel * wishdir[2];
    }
}

static float PM_CmdScale(usercmd_t* cmd) {
    int max;
    float total;
    float scale;

    max = abs(cmd->forwardmove);
    if (abs(cmd->rightmove) > max) {
        max = abs(cmd->rightmove);
    }
    if (abs(cmd->upmove) > max) {
        max = abs(cmd->upmove);
    }
    if (!max) {
        return 0;
    }

    total = sqrt(cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove + cmd->upmove * cmd->upmove);
    scale = (float)pm->ps->speed * max / (127.0 * total);

    return scale;
}

static qboolean PM_WishVelocity(vec3_t wishdir, float* wishspeed) {
    float scale;
    float fmove, smove;
    int i;

    scale = PM_CmdScale(&pm->cmd);
    if (scale == 0) {
        VectorClear(wishdir);
        *wishspeed = 0;
        return qfalse;
    }

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;
    for (i = 0; i < 3; i++) {
        wishdir[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    }
    wishdir[2] += pm->cmd.upmove;

    *wishspeed = VectorNormalize(wishdir) * scale;
    return qtrue;
}

static void PM_SetMovementDir(void) {
    if (pm->cmd.forwardmove || pm->cmd.rightmove) {
        if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 0;
        } else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 1;
        } else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0) {
            pm->ps->movementDir = 2;
        } else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 3;
        } else if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 4;
        } else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 5;
        } else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0) {
            pm->ps->movementDir = 6;
        } else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 7;
        }
    } else {
        if (pm->ps->movementDir == 2) {
            pm->ps->movementDir = 1;
        } else if (pm->ps->movementDir == 6) {
            pm->ps->movementDir = 7;
        }
    }
}

static void PM_CheckLadder(void) {
    vec3_t flatforward;
    vec3_t spot;
    trace_t trace;

    pml.ladder = qfalse;

    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0;
    VectorNormalize(flatforward);

    VectorMA(pm->ps->origin, 1, flatforward, spot);

    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, spot,
              pm->ps->clientNum, MASK_PLAYERSOLID);

    if (trace.fraction < 1.0 && (trace.surfaceFlags & SURF_LADDER)) {
        pml.ladder = qtrue;
    }
}

static void PM_LadderMove(void) {
    int i;
    vec3_t wishvel;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    float fmove, smove;
    float vel;

    PM_Friction();

    scale = PM_CmdScale(&pm->cmd);

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    if (scale != 0) {
        for (i = 0; i < 3; i++) {
            wishvel[i] = scale * pml.forward[i] * fmove + scale * pml.right[i] * smove;
        }
        wishvel[2] += scale * pm->cmd.upmove;

        if (pm->cmd.upmove > 0) {
            wishvel[2] = pm->ps->speed * 0.66f;
        } else if (pm->cmd.upmove < 0) {
            pm->ps->pm_flags &= ~PMF_DUCKED;
            pm->ps->viewheight = DEFAULT_VIEWHEIGHT;
            wishvel[2] = -pm->ps->speed * 0.66f;
        }
    } else {
        wishvel[0] = 0;
        wishvel[1] = 0;
        wishvel[2] = 0;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    vel = pm->ps->speed * 0.66f;
    if (wishspeed > vel) {
        wishspeed = vel;
    }

    PM_Accelerate(wishdir, wishspeed, pm_accelerate);

    if (pml.groundPlane && DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal) < 0.0f) {
        vel = VectorLength(pm->ps->velocity);
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                        pm->ps->velocity, OVERCLIP);
        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
    }

    PM_SlideMove(qfalse);
}

void PM_DoJump(void) {
    float jumpVel = pm_jumpVelocity;
    float timeDelta;

    pml.groundPlane = qfalse;
    pml.walking = qfalse;
    pm->ps->pm_flags |= PMF_JUMP_HELD;

    timeDelta = (float)(pm->cmd.serverTime - pm->ps->jumpTime);
    if (timeDelta > 0 && timeDelta < pm_jumpVelocityTimeThreshold) {
        if (pm_chainJump == 2) {
            if (pml.stepJumpFlag == 1) {
                jumpVel += pm_stepJumpVelocity;
            } else {
                jumpVel += pm_chainJumpVelocity;
            }
        } else if (pm_chainJump == 3) {
            float threshold = pm_jumpVelocityTimeThreshold * pm_jumpVelocityTimeThresholdOffset;
            if (timeDelta < threshold) {
                if (pml.stepJumpFlag == 1) {
                    jumpVel += pm_stepJumpVelocity;
                } else {
                    jumpVel += pm_chainJumpVelocity;
                }
            } else {
                jumpVel += pm_chainJumpVelocity + ((threshold - timeDelta) / threshold) * pm_chainJumpVelocity;
            }
        } else if (pm_chainJump == 1) {
            float factor = ((pm_jumpVelocityTimeThreshold - timeDelta) / pm_jumpVelocityTimeThreshold) * pm_jumpVelocityScaleAdd + 1.0f;
            jumpVel *= factor;
        }
    }

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    if (pm_rampJump && pml.crouchStepJumpFlag != 1) {
        pm->ps->velocity[2] *= pm_rampJumpScale;
        pm->ps->velocity[2] += jumpVel;
        if (pm->ps->velocity[2] < jumpVel) {
            pm->ps->velocity[2] = jumpVel;
        }
        if (pm->ps->velocity[2] > pm_jumpVelocityMax) {
            pm->ps->velocity[2] = pm_jumpVelocityMax;
        }
    } else {
        pm->ps->velocity[2] = jumpVel;
    }

    PM_AddEvent(EV_JUMP);

    if (pm->cmd.forwardmove >= 0) {
        PM_ForceLegsAnim(LEGS_JUMP);
        pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
    } else {
        PM_ForceLegsAnim(LEGS_JUMPB);
        pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
    }

    pm->ps->jumpTime = pm->cmd.serverTime;
}

qboolean PM_WouldJump(void) {
    if (pm_autoHop && !(pm->ps->pm_flags & PMF_NO_AUTOHOP)) {
    } else if (pm->ps->pm_flags & PMF_ZOOM) {
        return qfalse;
    }

    if (pm->cmd.upmove <= 9) {
        return qfalse;
    }

    if ((float)(pm->cmd.serverTime - pm->ps->jumpTime) < pm_jumpTimeDeltaMin) {
        return qfalse;
    }

    if ((pm_autoHop || !(pm->ps->pm_flags & PMF_JUMP_HELD))
        && (!(pm->ps->pm_flags & PMF_NO_AUTOHOP) || !(pm->ps->pm_flags & PMF_JUMP_HELD))) {
        return qtrue;
    }

    pm->cmd.upmove = 0;
    return qfalse;
}

static qboolean PM_WouldCrouchStepJump(void) {
    if (!(pm->ps->pm_flags & PMF_DUCKED)) {
        return qfalse;
    }
    if (pml.groundPlane) {
        return qfalse;
    }
    if (pm->ps->velocity[2] < 0) {
        return qfalse;
    }
    if ((float)(pm->cmd.serverTime - pm->ps->jumpTime) < pm_jumpVelocityTimeThreshold) {
        return qfalse;
    }
    return qtrue;
}

static qboolean PM_CheckJump(qboolean doubleJump) {
    if (!PM_WouldJump()) {
        return qfalse;
    }

    if (doubleJump && (pm->ps->pm_flags & PMF_DOUBLE_JUMPED)) {
        if (pm->ps->pm_flags & PMF_JUMP_HELD) {
            pm->cmd.upmove = 0;
            return qfalse;
        }
        if (pm->ps->doubleJumped) {
            return qfalse;
        }
    }

    if (pm->ps->pm_flags & PMF_CROUCH_SLIDE) {
        pm->ps->crouchSlideTime = 0;
    }

    PM_DoJump();

    if (doubleJump && (pm->ps->pm_flags & PMF_DOUBLE_JUMPED)) {
        pm->ps->doubleJumped = 1;
    }

    return qtrue;
}

static qboolean PM_CheckWaterJump(void) {
    vec3_t spot;
    int cont;
    vec3_t flatforward;

    if (pm->ps->pm_time) {
        return qfalse;
    }

    if (pm->waterlevel != 2) {
        return qfalse;
    }

    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0;
    VectorNormalize(flatforward);

    VectorMA(pm->ps->origin, 30, flatforward, spot);
    spot[2] += 4;
    cont = pm->pointcontents(spot, pm->ps->clientNum);
    if (!(cont & CONTENTS_SOLID)) {
        return qfalse;
    }

    spot[2] += 16;
    cont = pm->pointcontents(spot, pm->ps->clientNum);
    if (cont) {
        return qfalse;
    }

    VectorScale(pml.forward, 200, pm->ps->velocity);
    pm->ps->velocity[2] = 350;

    pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
    pm->ps->pm_time = 2000;

    return qtrue;
}

static void PM_WaterJumpMove(void) {

    PM_StepSlideMove(qtrue);

    pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
    if (pm->ps->velocity[2] < 0) {
        pm->ps->pm_flags &= ~PMF_ALL_TIMES;
        pm->ps->pm_time = 0;
    }
}

static void PM_WaterMove(void) {
    vec3_t wishdir;
    float wishspeed;
    float maxspeed;

    if (PM_CheckWaterJump()) {
        PM_WaterJumpMove();
        return;
    }
#if 0
	if (pm->cmd.upmove >= 10) {
		if (pm->ps->velocity[2] > -300) {
			if (pm->watertype & CONTENTS_WATER) {
				pm->ps->velocity[2] = 100;
			} else if (pm->watertype & CONTENTS_SLIME) {
				pm->ps->velocity[2] = 80;
			} else {
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
    PM_Friction();

    if (!PM_WishVelocity(wishdir, &wishspeed)) {
        VectorSet(wishdir, 0, 0, -60);
        wishspeed = VectorNormalize(wishdir);
    }

    if (pm->waterlevel == 1) {
        maxspeed = pm->ps->speed * pm_waterWadeScale;
    } else {
        maxspeed = pm->ps->speed * pm_waterSwimScale;
    }
    if (wishspeed > maxspeed) {
        wishspeed = maxspeed;
    }

    PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);

    if (pml.groundPlane && DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal) < 0) {
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                        pm->ps->velocity, OVERCLIP);
    }

    PM_StepSlideMove(qfalse);
}

static void PM_InvulnerabilityMove(void) {
    pm->cmd.forwardmove = 0;
    pm->cmd.rightmove = 0;
    pm->cmd.upmove = 0;
    VectorClear(pm->ps->velocity);
}

static void PM_FlyMove(void) {
    vec3_t wishdir;
    float wishspeed;

    PM_Friction();

    PM_WishVelocity(wishdir, &wishspeed);

    PM_Accelerate(wishdir, wishspeed, pm_flyaccelerate);

    PM_StepSlideMove(qfalse);
}

static void PM_FlightMove(void) {
    playerState_t* ps = pm->ps;

    if (bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag != HI_FLIGHT
        || !(ps->pm_flags & PMF_USE_ITEM_HELD)
        || ps->pm_type == PM_DEAD
        || (ps->stats[STAT_FLIGHT_REFUEL] && !ps->stats[STAT_MAX_FLIGHT_FUEL])) {
        ps->powerups[PW_FLIGHT] = 0;
        return;
    }

    ps->powerups[PW_FLIGHT] = 0x7fffffff;
    ps->velocity[2] += (float)(int)(ps->stats[STAT_FLIGHT_THRUST] * pml.frametime);

    if (ps->stats[STAT_MAX_FLIGHT_FUEL] <= 32000) {
        ps->stats[STAT_MAX_FLIGHT_FUEL] -= pml.msec;
        if (ps->stats[STAT_MAX_FLIGHT_FUEL] <= 0) {
            ps->stats[STAT_MAX_FLIGHT_FUEL] = 0;
            if (!ps->stats[STAT_FLIGHT_REFUEL]) {
                ps->stats[STAT_HOLDABLE_ITEM] = 0;
            }
        }
    }

    if (ps->pm_flags & PMF_DUCKED) {
        PM_ForceLegsAnim(LEGS_IDLECR);
    } else if ((ps->legsAnim & ~ANIM_TOGGLEBIT) != LEGS_JUMP) {
        PM_ForceLegsAnim(LEGS_JUMP);
    }
}

static void PM_AirControl(vec3_t wishdir, float wishspeed) {
    float zspeed, speed, dot, k;
    int i;
    if ((pm->ps->movementDir != 0 && pm->ps->movementDir != 4) || wishspeed == 0) {
        return;
    }

    zspeed = pm->ps->velocity[2];
    pm->ps->velocity[2] = 0;
    speed = VectorNormalize(pm->ps->velocity);

    dot = DotProduct(pm->ps->velocity, wishdir);
    k = 32.0f * pm_airControl * dot * dot * pml.frametime;

    if (dot > 0) {
        for (i = 0; i < 2; i++) {
            pm->ps->velocity[i] = pm->ps->velocity[i] * speed + wishdir[i] * k;
        }
        VectorNormalize(pm->ps->velocity);
    }

    for (i = 0; i < 2; i++) {
        pm->ps->velocity[i] *= speed;
    }
    pm->ps->velocity[2] = zspeed;
}

static void PM_AirMove(void) {
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    float accel;
    usercmd_t cmd;

    if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pm->cmd.upmove >= 0) {
        pm->ps->crouchSlideTime += pml.msec * 5;
        if (pm->ps->crouchSlideTime > pm_crouchSlideTime) {
            pm->ps->crouchSlideTime = pm_crouchSlideTime;
        }
    }

    PM_Friction();

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    scale = PM_CmdScale(&cmd);

    PM_SetMovementDir();

    pml.forward[2] = 0;
    pml.right[2] = 0;
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    for (i = 0; i < 2; i++) {
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    }
    wishvel[2] = 0;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    accel = pm_airAccel;
    if (pm->ps->pm_flags & PMF_PROMODE) {
        if (DotProduct(pm->ps->velocity, wishdir) < 0) {
            accel = pm_airStopAccel;
        }
        if (pm->ps->movementDir == 2 || pm->ps->movementDir == 6) {
            if (wishspeed > pm_wishSpeed) {
                wishspeed = pm_wishSpeed;
            }
            accel = pm_strafeAccel;
        }
    }

    PM_Accelerate(wishdir, wishspeed, accel);

    if (pm->ps->pm_flags & PMF_PROMODE) {
        PM_AirControl(wishdir, wishspeed);
    }

    if (pml.groundPlane) {
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                        pm->ps->velocity, OVERCLIP);
    }

    PM_StepSlideMove(qtrue);

    if (pm->ps->pm_flags & PMF_DUCKED) {
        PM_ForceLegsAnim(LEGS_IDLECR);
    } else if ((pm->ps->legsAnim & ~ANIM_TOGGLEBIT) == LEGS_IDLECR) {
        PM_ForceLegsAnim(LEGS_IDLE);
    }
    PM_FlightMove();

    if (pm->ps->pm_flags & PMF_DOUBLE_JUMPED) {
        PM_CheckJump(qtrue);
    }
}

static void PM_GrappleMove(void) {
    vec3_t vel, v;
    float vlen;

    if (pm_velocityGH == 0.0f) {
        return;
    }

    VectorScale(pml.forward, -16, v);
    VectorAdd(pm->ps->grapplePoint, v, v);
    VectorSubtract(v, pm->ps->origin, vel);
    vlen = VectorLength(vel);
    VectorNormalize(vel);

    VectorScale(vel, pm_velocityGH, vel);

    if (vlen <= pm_velocityGH * 0.1f) {
        float scale = (vlen * 10.0f) / pm_velocityGH;
        VectorScale(vel, scale, vel);
    }

    if (pm->hookEnemy) {
        VectorScale(vel, 0.25f, vel);
    } else {
        pml.groundPlane = qfalse;
    }

    if (pm->waterlevel > 1) {
        VectorScale(vel, 0.75f, vel);
    }

    VectorCopy(vel, pm->ps->velocity);
}

static void PM_WalkMove(void) {
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    usercmd_t cmd;
    float accelerate;

    if (pm->waterlevel > 2 && DotProduct(pml.forward, pml.groundTrace.plane.normal) > 0) {
        PM_WaterMove();
        return;
    }

    if (bg_itemlist[pm->ps->stats[STAT_HOLDABLE_ITEM]].giTag == HI_FLIGHT &&
        (pm->ps->pm_flags & PMF_USE_ITEM_HELD) &&
        pm->ps->pm_type != PM_DEAD &&
        (!pm->ps->stats[STAT_FLIGHT_REFUEL] || pm->ps->stats[STAT_MAX_FLIGHT_FUEL])) {
        PM_StepSlideMove(qfalse);
        PM_FlightMove();
        return;
    }

    if (PM_CheckJump(qfalse)) {
        if (pm->waterlevel > 1) {
            PM_WaterMove();
        } else {
            PM_AirMove();
        }
        return;
    }

    PM_Friction();

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    scale = PM_CmdScale(&cmd);

    PM_SetMovementDir();

    pml.forward[2] = 0;
    pml.right[2] = 0;

    PM_ClipVelocity(pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP);
    PM_ClipVelocity(pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP);
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    for (i = 0; i < 3; i++) {
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && (pm->ps->pm_flags & PMF_DUCKED)) {
        if (wishspeed > pm->ps->speed * 0.75f) {
            wishspeed = pm->ps->speed * 0.75f;
        }
    } else if (pm->ps->pm_flags & PMF_DUCKED) {
        if (wishspeed > pm->ps->speed * pm_duckScale) {
            wishspeed = pm->ps->speed * pm_duckScale;
        }
    }

    if (pm->waterlevel) {
        float waterScale;
        if (pm->waterlevel == 1) {
            waterScale = pm_waterWadeScale;
        } else {
            waterScale = pm_waterSwimScale;
        }
        float scaledSpeed = 1.0f - ((float)pm->waterlevel / 3.0f) * (1.0f - waterScale);
        if (wishspeed > pm->ps->speed * scaledSpeed) {
            wishspeed = pm->ps->speed * scaledSpeed;
        }
    }

    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK) {
        accelerate = pm_airaccelerate;
        if ((pm->ps->pm_flags & PMF_DUCKED) && (pml.groundTrace.surfaceFlags & SURF_SLICK)) {
            accelerate = 1.4f;
        }
    } else {
        accelerate = pm_walkAccel;
    }

    PM_Accelerate(wishdir, wishspeed, accelerate);

    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK) {
        pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
    }

    PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                    pm->ps->velocity, OVERCLIP);

    if (!pm->ps->velocity[0] && !pm->ps->velocity[1]) {
        return;
    }

    PM_StepSlideMove(qfalse);
}

static void PM_DeadMove(void) {
    float forward;

    if (!pml.walking) {
        return;
    }

    forward = VectorLength(pm->ps->velocity);
    forward -= 20;
    if (forward <= 0) {
        VectorClear(pm->ps->velocity);
    } else {
        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, forward, pm->ps->velocity);
    }
}

static void PM_NoclipMove(void) {
    float speed, drop, friction, control, newspeed;
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;

    pm->ps->viewheight = DEFAULT_VIEWHEIGHT;

    speed = VectorLength(pm->ps->velocity);
    if (speed < 1) {
        VectorCopy(vec3_origin, pm->ps->velocity);
    } else {
        drop = 0;

        friction = 10.0f;
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
        drop += control * friction * pml.frametime;

        newspeed = speed - drop;
        if (newspeed < 0)
            newspeed = 0;
        newspeed /= speed;

        VectorScale(pm->ps->velocity, newspeed, pm->ps->velocity);
    }

    scale = PM_CmdScale(&pm->cmd);

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    for (i = 0; i < 3; i++)
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    wishvel[2] += pm->cmd.upmove;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    PM_Accelerate(wishdir, wishspeed, pm_walkAccel);

    VectorMA(pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}

static int PM_FootstepForSurface(void) {
    if (pm->noFootsteps || (pml.groundTrace.surfaceFlags & SURF_NOSTEPS)) {
        return 0;
    }
    if (pml.groundTrace.surfaceFlags & SURF_METALSTEPS) {
        return EV_FOOTSTEP_METAL;
    }
    if (pml.groundTrace.surfaceFlags & SURF_SNOWSTEPS) {
        return EV_FOOTSTEP_SNOW;
    }
    if (pml.groundTrace.surfaceFlags & SURF_WOODSTEPS) {
        return EV_FOOTSTEP_WOOD;
    }
    return EV_FOOTSTEP;
}

static void PM_CrashLand(void) {
    float delta;
    float dist;
    float vel, acc;
    float t;
    float a, b, c, den;

    if (pm->ps->pm_flags & PMF_BACKWARDS_JUMP) {
        PM_ForceLegsAnim(LEGS_LANDB);
    } else {
        PM_ForceLegsAnim(LEGS_LAND);
    }

    pm->ps->legsTimer = TIMER_LAND;

    dist = pm->ps->origin[2] - pml.previous_origin[2];
    vel = pml.previous_velocity[2];
    acc = -pm->ps->gravity;

    a = acc / 2;
    b = vel;
    c = -dist;

    den = b * b - 4 * a * c;
    if (den < 0) {
        return;
    }
    t = (-b - sqrt(den)) / (2 * a);

    delta = vel + t * acc;
    delta = delta * delta * 0.0001;

    if (pm->waterlevel == 3) {
        return;
    }

    if (pm->waterlevel == 2) {
        delta *= 0.25;
    }
    if (pm->waterlevel == 1) {
        delta *= 0.5;
    }

    if (delta < 1) {
        return;
    }

    if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE) && pm->ps->stats[STAT_HEALTH] > -40) {
        if (delta > 62) {
            PM_AddEvent(EV_FALL_FAR);
        } else if (delta > 42) {
            if (pm->ps->stats[STAT_HEALTH] > 0) {
                PM_AddEvent(EV_FALL_MEDIUM);
            }
        } else if (delta > 7) {
            PM_AddEvent(EV_FALL_SHORT);
        } else if (pm->ps->stats[STAT_RUNE] != 1) {
            PM_AddEvent(PM_FootstepForSurface());
        }
    }

    pm->ps->bobCycle = 0;

    if (pm->ps->pm_flags & PMF_DOUBLE_JUMPED) {
        pm->ps->doubleJumped = 0;
    }
}

static int PM_CorrectAllSolid(trace_t* trace) {
    int i, j, k;
    vec3_t point;

    if (pm->debugLevel) {
        Com_Printf("%i:allsolid\n", c_pmove);
    }

    for (i = -1; i <= 1; i++) {
        for (j = -1; j <= 1; j++) {
            for (k = -1; k <= 1; k++) {
                point[0] = pm->ps->origin[0] + (float)i;
                point[1] = pm->ps->origin[0] + (float)j;
                point[2] = pm->ps->origin[0] + (float)k;
                pm->trace(trace, point, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
                if (!trace->allsolid) {
                    point[0] = pm->ps->origin[0];
                    point[1] = pm->ps->origin[1];
                    point[2] = pm->ps->origin[2] - 0.25;

                    pm->trace(trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
                    pml.groundTrace = *trace;
                    return qtrue;
                }
            }
        }
    }

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;

    return qfalse;
}

static void PM_GroundTraceMissed(void) {
    trace_t trace;
    vec3_t point;

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {
        if (pm->debugLevel) {
            Com_Printf("%i:lift\n", c_pmove);
        }

        VectorCopy(pm->ps->origin, point);
        point[2] -= 64;

        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
        if (trace.fraction == 1.0) {
            if (pm->cmd.forwardmove >= 0) {
                PM_ForceLegsAnim(LEGS_JUMP);
                pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
            } else {
                PM_ForceLegsAnim(LEGS_JUMPB);
                pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
            }
        }
    }

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
}

static void PM_GroundTrace(void) {
    vec3_t point;
    trace_t trace;

    point[0] = pm->ps->origin[0];
    point[1] = pm->ps->origin[1];
    point[2] = pm->ps->origin[2] - 0.25;

    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
    pml.groundTrace = trace;

    if (trace.allsolid) {
        if (!PM_CorrectAllSolid(&trace))
            return;
    }

    if (trace.fraction == 1.0) {
        PM_GroundTraceMissed();
        pml.groundPlane = qfalse;
        pml.walking = qfalse;
        return;
    }

    if (pm->ps->velocity[2] > 0 && DotProduct(pm->ps->velocity, trace.plane.normal) > 10) {
        if (pm->debugLevel) {
            Com_Printf("%i:kickoff\n", c_pmove);
        }
        if (pm->cmd.forwardmove >= 0) {
            PM_ForceLegsAnim(LEGS_JUMP);
            pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
        } else {
            PM_ForceLegsAnim(LEGS_JUMPB);
            pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
        }

        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = qfalse;
        pml.walking = qfalse;
        return;
    }

    if (trace.plane.normal[2] < MIN_WALK_NORMAL) {
        if (pm->debugLevel) {
            Com_Printf("%i:steep\n", c_pmove);
        }
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = qtrue;
        pml.walking = qfalse;
        return;
    }

    pml.groundPlane = qtrue;
    pml.walking = qtrue;

    if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
        pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
        pm->ps->pm_time = 0;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (pm->debugLevel) {
            Com_Printf("%i:Land\n", c_pmove);
        }

        PM_CrashLand();

        if (pml.previous_velocity[2] < -200) {
            pm->ps->pm_flags |= PMF_TIME_LAND;
            pm->ps->pm_time = 250;
        }
    }

    pm->ps->groundEntityNum = trace.entityNum;

    PM_AddTouchEnt(trace.entityNum);
}

static void PM_SetWaterLevel(void) {
    vec3_t point;
    int cont;

    pm->waterlevel = 0;

    point[0] = pm->ps->origin[0];
    point[1] = pm->ps->origin[1];
    point[2] = pm->ps->origin[2] + MINS_Z + 1;
    cont = pm->pointcontents(point, pm->ps->clientNum);

    pm->watertype = cont;

    if (cont & MASK_WATER) {
        pm->waterlevel = 1;
        point[2] = pm->ps->origin[2];
        cont = pm->pointcontents(point, pm->ps->clientNum);
        if (cont & MASK_WATER) {
            pm->waterlevel = 2;
            point[2] = pm->ps->origin[2] + pm->ps->viewheight;
            cont = pm->pointcontents(point, pm->ps->clientNum);
            if (cont & MASK_WATER) {
                pm->waterlevel = 3;
            }
        }
    }
}

static void PM_CheckDuck(void) {
    trace_t trace;

    if (pm->ps->powerups[PW_INVULNERABILITY]) {
        if (pm->ps->pm_flags & PMF_INVULEXPAND) {
            VectorSet(pm->mins, -42, -42, -42);
            VectorSet(pm->maxs, 42, 42, 42);
        } else {
            VectorSet(pm->mins, -15, -15, MINS_Z);
            VectorSet(pm->maxs, 15, 15, 16);
        }
        pm->ps->pm_flags |= PMF_DUCKED;
        pm->ps->viewheight = CROUCH_VIEWHEIGHT;
        return;
    }
    pm->ps->pm_flags &= ~PMF_INVULEXPAND;

    pm->mins[0] = -15;
    pm->mins[1] = -15;

    pm->maxs[0] = 15;
    pm->maxs[1] = 15;

    pm->mins[2] = MINS_Z;

    if (pm->ps->pm_type == PM_DEAD) {
        pm->maxs[2] = -8;
        pm->ps->viewheight = DEAD_VIEWHEIGHT;
        return;
    }

    if (pm->cmd.upmove < 0) {
        pm->ps->pm_flags |= PMF_DUCKED;
        if (pm->ps->crouchTime == 0) {
            pm->ps->crouchTime = pm->cmd.serverTime;
        }
    } else {
        if (pm->ps->pm_flags & PMF_DUCKED) {
            pm->maxs[2] = 32;
            pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);
            if (!trace.allsolid)
                pm->ps->pm_flags &= ~PMF_DUCKED;
        }
    }

    if (pm->ps->pm_flags & PMF_DUCKED) {
        if (pm->ps->crouchSlideTime == 0 && (pm->ps->pm_flags & PMF_CROUCH_SLIDE)
            && pm->ps->speed < 400) {
            pm->ps->speed = 400;
        }
        pm->maxs[2] = 16;
        pm->ps->viewheight = CROUCH_VIEWHEIGHT;
    } else {
        pm->maxs[2] = 32;
        pm->ps->viewheight = DEFAULT_VIEWHEIGHT;

        if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pm->ps->crouchSlideTime != 0
            && pml.groundPlane) {
            pm->ps->crouchSlideTime = 0;
        }
    }
}

static void PM_Footsteps(void) {
    float bobmove;
    int old;
    qboolean footstep;

    pm->xyspeed = sqrt(pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1]);

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (pm->ps->powerups[PW_INVULNERABILITY]) {
            PM_ContinueLegsAnim(LEGS_IDLECR);
        }
        if (pm->waterlevel > 1) {
            PM_ContinueLegsAnim(LEGS_SWIM);
        }
        return;
    }

    if (!pm->cmd.forwardmove && !pm->cmd.rightmove
        && !((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pm->cmd.upmove < 0
             && pm->ps->crouchSlideTime > 0)) {
        if (pm->xyspeed < 5) {
            pm->ps->bobCycle = 0;
            if (pm->ps->pm_flags & PMF_DUCKED) {
                PM_ContinueLegsAnim(LEGS_IDLECR);
            } else {
                PM_ContinueLegsAnim(LEGS_IDLE);
            }
        }
        return;
    }

    footstep = qfalse;

    if (pm->ps->pm_flags & PMF_DUCKED) {
        bobmove = 0.5;
        if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
            PM_ContinueLegsAnim(LEGS_BACKCR);
        } else {
            PM_ContinueLegsAnim(LEGS_WALKCR);
        }
    } else if (pm->cmd.buttons & BUTTON_WALKING) {
        bobmove = 0.3f;
        if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
            PM_ContinueLegsAnim(LEGS_BACKWALK);
        } else {
            PM_ContinueLegsAnim(LEGS_WALK);
        }
    } else if (pm->ps->velocity[0] < 32 && pm->ps->velocity[0] > -32
               && pm->ps->velocity[1] < 32 && pm->ps->velocity[1] > -32) {
        bobmove = 0.35f;
        if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
            PM_ContinueLegsAnim(LEGS_BACKWALK);
        } else {
            PM_ContinueLegsAnim(LEGS_WALK);
        }
    } else {
        bobmove = 0.4f;
        if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
            PM_ContinueLegsAnim(LEGS_BACK);
        } else {
            PM_ContinueLegsAnim(LEGS_RUN);
        }
        footstep = (pm->ps->stats[STAT_RUNE] != 1);
    }

    old = pm->ps->bobCycle & 255;
    if (pm->retailBobCycle) {
        pm->ps->bobCycle = (int)(old + bobmove * pml.msec) & 255;
    } else {
        int carry = (pm->ps->bobCycle >> 8) & 255;
        int units = (int)(bobmove * pml.msec * 256.0f) + carry;
        int cycle = (old + (units >> 8)) & 255;

        pm->ps->bobCycle = cycle | ((units & 255) << 8);
    }

    if (((old + 64) ^ ((pm->ps->bobCycle & 255) + 64)) & 128) {
        if (pm->waterlevel == 0) {
            if (footstep) {
                PM_AddEvent(PM_FootstepForSurface());
            }
        } else if (pm->waterlevel == 1) {
            PM_AddEvent(EV_FOOTSPLASH);
        } else if (pm->waterlevel == 2) {
            PM_AddEvent(EV_SWIM);
        } else if (pm->waterlevel == 3) {
        }
    }
}

static void PM_WaterEvents(void) {
    if (!pml.previous_waterlevel && pm->waterlevel) {
        PM_AddEvent(EV_WATER_TOUCH);
    }

    if (pml.previous_waterlevel && !pm->waterlevel) {
        PM_AddEvent(EV_WATER_LEAVE);
    }

    if (pm->ps->pm_type != PM_DEAD) {
        if (pml.previous_waterlevel != 3 && pm->waterlevel == 3) {
            PM_AddEvent(EV_WATER_UNDER);
        }

        if (!pm->ps->powerups[PW_BATTLESUIT] && pml.previous_waterlevel == 3 && pm->waterlevel != 3) {
            PM_AddEvent(EV_WATER_CLEAR);
        }
    }
}

static void PM_BeginWeaponChange(int weapon) {
    if (weapon <= WP_NONE || weapon > WP_NUM_WEAPONS) {
        return;
    }

    if (!(pm->ps->stats[STAT_WEAPONS] & (1 << weapon))) {
        return;
    }

    if (pm->ps->weaponstate == WEAPON_DROPPING) {
        return;
    }

    PM_AddEvent(EV_CHANGE_WEAPON);

    if (weapon == WP_NUM_WEAPONS) {
        PM_StartTorsoAnim(TORSO_STAND2);
        pm->ps->weaponstate = WEAPON_READY;
        pm->ps->weapon = weapon;
        return;
    }

    pm->ps->weaponstate = WEAPON_DROPPING;
    pm->ps->weaponTime += pm_weaponDropTime;
    PM_StartTorsoAnim(TORSO_DROP);
}

static void PM_FinishWeaponChange(void) {
    int weapon;

    weapon = pm->cmd.weapon;
    if (weapon < WP_NONE || weapon > WP_NUM_WEAPONS) {
        weapon = WP_NONE;
    }

    if (!(pm->ps->stats[STAT_WEAPONS] & (1 << weapon))) {
        weapon = WP_NONE;
    }

    pm->ps->weapon = weapon;
    pm->ps->stats[STAT_SPINUP] = 0;
    pm->ps->weaponstate = WEAPON_RAISING;
    pm->ps->weaponTime += pm_weaponRaiseTime;
    PM_StartTorsoAnim(TORSO_RAISE);
}

static void PM_TorsoAnimation(void) {
    if (pm->ps->weaponstate == WEAPON_READY) {
        if (pm->ps->weapon == WP_GAUNTLET || pm->ps->weapon == WP_NUM_WEAPONS) {
            PM_ContinueTorsoAnim(TORSO_STAND2);
        } else {
            PM_ContinueTorsoAnim(TORSO_STAND);
        }
        return;
    }
}

static void PM_Weapon(void) {
    int addTime;

    if (pm->ps->pm_flags & PMF_ZOOM) {
        return;
    }

    if (pm->ps->pm_flags & PMF_RESPAWNED) {
        return;
    }

    if (pm->ps->stats[STAT_HEALTH] <= 0) {
        pm->ps->weapon = WP_NONE;
        return;
    }

    if ((pm->cmd.buttons & BUTTON_ATTACK)
        && !(pm->ps->pm_flags & PMF_FROZEN)
        && (pm->ps->ammo[pm->ps->weapon] != 0 || pm->ps->weapon == WP_GRAPPLING_HOOK)) {
        pm->ps->eFlags |= EF_FIRING;
    }

    if (pm->cmd.buttons & BUTTON_USE_HOLDABLE) {
        if (!(pm->ps->pm_flags & PMF_USE_ITEM_HELD)) {
            int tag = bg_itemlist[pm->ps->stats[STAT_HOLDABLE_ITEM]].giTag;

            pm->ps->pm_flags |= PMF_USE_ITEM_HELD;
            if (tag != HI_FLIGHT
                && !(tag == HI_MEDKIT && pm->ps->stats[STAT_HEALTH] >= (pm->ps->stats[STAT_MAX_HEALTH] + 25))) {
                PM_AddEvent(EV_USE_ITEM0 + tag);
                pm->ps->stats[STAT_HOLDABLE_ITEM] = 0;
            }
            return;
        }
    } else {
        pm->ps->pm_flags &= ~PMF_USE_ITEM_HELD;
    }

    if (pm->ps->weaponTime > 0) {
        pm->ps->weaponTime -= pml.msec;
    }

    if (pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING) {
        if (pm->ps->weapon != pm->cmd.weapon) {
            PM_BeginWeaponChange(pm->cmd.weapon);
        }
    }

    if (pm->ps->weaponTime > 0) {
        return;
    }

    if (pm->ps->weaponstate == WEAPON_DROPPING) {
        PM_FinishWeaponChange();
        return;
    }

    if (pm->ps->weaponstate == WEAPON_RAISING) {
        pm->ps->weaponstate = WEAPON_READY;
        if (pm->ps->weapon == WP_GAUNTLET || pm->ps->weapon == WP_NUM_WEAPONS) {
            PM_StartTorsoAnim(TORSO_STAND2);
        } else {
            PM_StartTorsoAnim(TORSO_STAND);
        }
        return;
    }

    if (pm->ps->weapon == WP_NUM_WEAPONS) {
        return;
    }

    if (!(pm->cmd.buttons & BUTTON_ATTACK) || (pm->ps->pm_flags & PMF_FROZEN)) {
        int spin;
        pm->ps->weaponTime = 0;
        pm->ps->weaponstate = WEAPON_READY;
        spin = pm->ps->stats[STAT_SPINUP] - pml.msec;
        if (spin < 0) {
            spin = 0;
        } else if (spin > 1000) {
            spin = 1000;
        }
        pm->ps->stats[STAT_SPINUP] = spin;
        return;
    }

    if (pm->ps->pm_flags & PMF_TIME_GRAPPLE) {
        pm->ps->weaponTime = 0;
        pm->ps->weaponstate = WEAPON_READY;
        pm->ps->eFlags &= ~EF_FIRING;
        pm->ps->pm_flags &= ~PMF_TIME_GRAPPLE;
        return;
    }

    if (pm->ps->weapon == WP_GAUNTLET) {
        if (!pm->gauntletHit) {
            pm->ps->weaponTime = 0;
            pm->ps->weaponstate = WEAPON_READY;
            return;
        }
        PM_StartTorsoAnim(TORSO_ATTACK2);
    } else {
        PM_StartTorsoAnim(TORSO_ATTACK);
    }

    pm->ps->weaponstate = WEAPON_FIRING;

    if (!pm->ps->ammo[pm->ps->weapon]) {
        PM_AddEvent(EV_NOAMMO);
        pm->ps->weaponTime += 500;
        return;
    }

    if (pm->ps->ammo[pm->ps->weapon] != -1) {
        pm->ps->ammo[pm->ps->weapon]--;
    }

    PM_AddEvent(EV_FIRE_WEAPON);

    addTime = bg_weaponReloadTime[pm->ps->weapon];

    if (pm->ps->weapon == WP_CHAINGUN) {
        int spin;
        if (pm->ps->stats[STAT_SPINUP] < 1000) {
            addTime *= 2;
        }
        spin = pm->ps->stats[STAT_SPINUP] + addTime;
        if (spin < 0) {
            spin = 0;
        } else if (spin > 1000) {
            spin = 1000;
        }
        pm->ps->stats[STAT_SPINUP] = spin;
    }

    if (pm->ps->powerups[PW_HASTE]) {
        addTime = (int)(addTime * (10.0 / 13.0));
    } else if (pm->ps->stats[STAT_RUNE] == 1) {
        addTime = (int)(addTime / 1.25);
    }

    pm->ps->weaponTime += addTime;
}

static void PM_Animate(void) {
    if (pm->cmd.buttons & BUTTON_GESTURE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_GESTURE);
            pm->ps->torsoTimer = TIMER_GESTURE;
            PM_AddEvent(EV_TAUNT);
        }
    } else if (pm->cmd.buttons & BUTTON_GETFLAG) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_GETFLAG);
            pm->ps->torsoTimer = 600;
        }
    } else if (pm->cmd.buttons & BUTTON_GUARDBASE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_GUARDBASE);
            pm->ps->torsoTimer = 600;
        }
    } else if (pm->cmd.buttons & BUTTON_PATROL) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_PATROL);
            pm->ps->torsoTimer = 600;
        }
    } else if (pm->cmd.buttons & BUTTON_FOLLOWME) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_FOLLOWME);
            pm->ps->torsoTimer = 600;
        }
    } else if (pm->cmd.buttons & BUTTON_AFFIRMATIVE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_AFFIRMATIVE);
            pm->ps->torsoTimer = 600;
        }
    } else if (pm->cmd.buttons & BUTTON_NEGATIVE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_NEGATIVE);
            pm->ps->torsoTimer = 600;
        }
    }
}

static void PM_DropTimers(void) {
    if (pm->ps->pm_time) {
        if (pml.msec >= pm->ps->pm_time) {
            pm->ps->pm_flags &= ~PMF_ALL_TIMES;
            pm->ps->pm_time = 0;
        } else {
            pm->ps->pm_time -= pml.msec;
        }
    }

    if (pm->ps->legsTimer > 0) {
        pm->ps->legsTimer -= pml.msec;
        if (pm->ps->legsTimer < 0) {
            pm->ps->legsTimer = 0;
        }
    }

    if (pm->ps->torsoTimer > 0) {
        pm->ps->torsoTimer -= pml.msec;
        if (pm->ps->torsoTimer < 0) {
            pm->ps->torsoTimer = 0;
        }
    }

    if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pml.groundPlane
        && pm->ps->crouchSlideTime != 0) {
        if (pm->ps->crouchSlideTime <= pml.msec) {
            pm->ps->crouchSlideTime = 0;
        } else {
            pm->ps->crouchSlideTime -= pml.msec;
        }
    }
}

void PM_UpdateViewAngles(playerState_t* ps, const usercmd_t* cmd) {
    short temp;
    int i;

    if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_TUTORIAL) {
        return;
    }

    if (ps->pm_type != PM_SPECTATOR && ps->pm_type != PM_FREEZE && ps->stats[STAT_HEALTH] <= 0) {
        return;
    }

    for (i = 0; i < 3; i++) {
        temp = cmd->angles[i] + ps->delta_angles[i];
        if (i == PITCH) {
            if (temp > 16000) {
                ps->delta_angles[i] = 16000 - cmd->angles[i];
                temp = 16000;
            } else if (temp < -16000) {
                ps->delta_angles[i] = -16000 - cmd->angles[i];
                temp = -16000;
            }
        }
        ps->viewangles[i] = SHORT2ANGLE(temp);
    }
}

void PmoveSingle(pmove_t* pmove) {
    pm = pmove;

    c_pmove++;

    pm->numtouch = 0;
    pm->watertype = 0;
    pm->waterlevel = 0;

    PM_SetMovementConfig();

    if (pm->ps->stats[STAT_HEALTH] <= 0 && !pm->ps->powerups[PW_FREEZE]) {
        pm->tracemask &= ~CONTENTS_BODY;
    }

    if (pm_noPlayerClip == 1) {
        pm->tracemask &= ~CONTENTS_BODY;
    }

    if (abs(pm->cmd.forwardmove) > 64 || abs(pm->cmd.rightmove) > 64) {
        pm->cmd.buttons &= ~BUTTON_WALKING;
    }

    if (pm->cmd.buttons & BUTTON_TALK) {
        pm->ps->eFlags |= EF_TALK;
    } else {
        pm->ps->eFlags &= ~EF_TALK;
    }

    pm->ps->eFlags &= ~EF_FIRING;

    if (pm->ps->stats[STAT_HEALTH] > 0 &&
        !(pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE))) {
        pm->ps->pm_flags &= ~PMF_RESPAWNED;
        pm->ps->pm_flags &= ~PMF_ZOOM;
    }

    if (pmove->cmd.buttons & BUTTON_TALK) {
        pmove->cmd.buttons = BUTTON_TALK;
        pmove->cmd.forwardmove = 0;
        pmove->cmd.rightmove = 0;
        pmove->cmd.upmove = 0;
    }

    memset(&pml, 0, sizeof(pml));

    pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
    if (pml.msec < 1) {
        pml.msec = 1;
    } else if (pml.msec > 200) {
        pml.msec = 200;
    }
    pm->ps->commandTime = pmove->cmd.serverTime;

    if (pm->ps->pm_flags & PMF_PAUSED) {
        return;
    }

    VectorCopy(pm->ps->origin, pml.previous_origin);

    VectorCopy(pm->ps->velocity, pml.previous_velocity);

    pml.frametime = pml.msec * 0.001;

    PM_UpdateViewAngles(pm->ps, &pm->cmd);

    if (pm->cmd.weaponPrimary >= 1 && pm->cmd.weaponPrimary <= 14 &&
        pm->ps->weaponPrimary != pm->cmd.weaponPrimary) {
        pm->ps->weaponPrimary = pm->cmd.weaponPrimary;
    }

    if (pm->cmd.fov >= 10 && pm->cmd.fov <= 130 &&
        pm->ps->fov != pm->cmd.fov) {
        pm->ps->fov = pm->cmd.fov;
    }

    if (pm->ps->forwardmove != pm->cmd.forwardmove) {
        pm->ps->forwardmove = pm->cmd.forwardmove;
    }
    if (pm->ps->rightmove != (char)pm->cmd.rightmove) {
        pm->ps->rightmove = pm->cmd.rightmove;
    }
    if (pm->ps->upmove != pm->cmd.upmove) {
        pm->ps->upmove = pm->cmd.upmove;
    }

    AngleVectors(pm->ps->viewangles, pml.forward, pml.right, pml.up);

    if (pm->cmd.upmove < 10) {
        pm->ps->pm_flags &= ~PMF_JUMP_HELD;
    }

    if (pm->cmd.forwardmove < 0) {
        pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
    } else if (pm->cmd.forwardmove > 0 || (pm->cmd.forwardmove == 0 && pm->cmd.rightmove)) {
        pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
    }

    if (pm->ps->pm_type >= PM_DEAD) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
    }

    if (pm->ps->pm_type == PM_SPECTATOR) {
        PM_CheckDuck();
        PM_FlyMove();
        PM_DropTimers();
        return;
    }

    if (pm->ps->pm_type == PM_NOCLIP) {
        PM_NoclipMove();
        PM_DropTimers();
        return;
    }

    if (pm->ps->pm_type == PM_FREEZE) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
    }

    if (pm->ps->pm_type == PM_TUTORIAL) {
        return;
    }

    if (pm->ps->pm_type == PM_INTERMISSION) {
        return;
    }

    PM_SetWaterLevel();
    pml.previous_waterlevel = pmove->waterlevel;

    PM_CheckDuck();

    PM_GroundTrace();

    if (pm->ps->pm_type == PM_DEAD) {
        PM_DeadMove();
    }

    PM_DropTimers();

    PM_CheckLadder();

    pm->ps->powerups[PW_FLIGHT] = 0;

    if (pm->ps->powerups[PW_INVULNERABILITY]) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
        VectorClear(pm->ps->velocity);
        goto postMovement;
    }

    if (pm->ps->pm_flags & PMF_GRAPPLE_PULL) {
        PM_GrappleMove();
        PM_AirMove();
    } else if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
        PM_WaterJumpMove();
    } else if (pm->waterlevel > 1) {
        PM_WaterMove();
    } else if (pml.ladder) {
        PM_LadderMove();
    } else if (pml.walking) {
        PM_WalkMove();
    } else {
        PM_AirMove();
    }

postMovement:
    if (pm->ps->stats[STAT_FLIGHT_REFUEL] != 0 && !(pm->ps->pm_flags & PMF_USE_ITEM_HELD)) {
        int fuel = (int)((float)pm->ps->stats[STAT_FLIGHT_REFUEL] * pml.frametime)
                 + pm->ps->stats[STAT_MAX_FLIGHT_FUEL];
        if (fuel > pm->ps->stats[STAT_CUR_FLIGHT_FUEL]) {
            fuel = pm->ps->stats[STAT_CUR_FLIGHT_FUEL];
        }
        pm->ps->stats[STAT_MAX_FLIGHT_FUEL] = fuel;
    }

    PM_Animate();

    PM_GroundTrace();
    PM_SetWaterLevel();

    PM_Weapon();

    PM_TorsoAnimation();

    PM_Footsteps();

    PM_WaterEvents();
}

void Pmove(pmove_t* pmove) {
    int finalTime;

    finalTime = pmove->cmd.serverTime;

    if (finalTime < pmove->ps->commandTime) {
        return;
    }

    if (finalTime > pmove->ps->commandTime + 1000) {
        pmove->ps->commandTime = finalTime - 1000;
    }

    pmove->ps->pmove_framecount = (pmove->ps->pmove_framecount + 1) & ((1 << PS_PMOVEFRAMECOUNTBITS) - 1);

    while (pmove->ps->commandTime != finalTime) {
        int msec;

        msec = finalTime - pmove->ps->commandTime;

        if (msec > 66) {
            msec = 66;
        }
        pmove->cmd.serverTime = pmove->ps->commandTime + msec;
        PmoveSingle(pmove);

        if (pmove->ps->pm_flags & PMF_JUMP_HELD) {
            pmove->cmd.upmove = 20;
        }
    }

}
