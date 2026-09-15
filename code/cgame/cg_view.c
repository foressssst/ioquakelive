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
// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"
#include "../ui/ui_shared.h"

// [QL] cvars defined in cg_main.c; extern here since cg_local.h doesn't carry
// these externs yet.
extern vmCvar_t cg_filter_angles;
extern vmCvar_t cg_levelTimerDirection;

/*
=============================================================================

  MODEL TESTING

The viewthing and gun positioning tools from Q2 have been integrated and
enhanced into a single model testing facility.

Model viewing can begin with either "testmodel <modelname>" or "testgun <modelname>".

The names must be the full pathname after the basedir, like
"models/weapons/v_launch/tris.md3" or "players/male/tris.md3"

Testmodel will create a fake entity 100 units in front of the current view
position, directly facing the viewer.  It will remain immobile, so you can
move around it to view it from different angles.

Testgun will cause the model to follow the player around and suppress the real
view weapon model.  The default frame 0 of most guns is completely off screen,
so you will probably have to cycle a couple frames to see it.

"nextframe", "prevframe", "nextskin", and "prevskin" commands will change the
frame or skin of the testmodel.  These are bound to F5, F6, F7, and F8 in
q3default.cfg.

If a gun is being tested, the "gun_x", "gun_y", and "gun_z" variables will let
you adjust the positioning.

Note that none of the model testing features update while the game is paused, so
it may be convenient to test with deathmatch set to 1 so that bringing down the
console doesn't pause the game.

=============================================================================
*/

/*
=================
CG_TestModel_f

Creates an entity in front of the current position, which
can then be moved around
=================
*/
void CG_TestModel_f(void) {
    vec3_t angles;

    cg.testGun = qfalse;
    memset(&cg.testModelEntity, 0, sizeof(cg.testModelEntity));
    if (trap_Argc() < 2) {
        return;
    }

    Q_strncpyz(cg.testModelName, CG_Argv(1), MAX_QPATH);
    cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);

    if (trap_Argc() == 3) {
        cg.testModelEntity.backlerp = atof(CG_Argv(2));
        cg.testModelEntity.frame = 1;
        cg.testModelEntity.oldframe = 0;
    }
    if (!cg.testModelEntity.hModel) {
        CG_Printf("Can't register model\n");
        return;
    }

    VectorMA(cg.refdef.vieworg, 100, cg.refdef.viewaxis[0], cg.testModelEntity.origin);

    angles[PITCH] = 0;
    angles[YAW] = 180 + cg.refdefViewAngles[1];
    angles[ROLL] = 0;

    AnglesToAxis(angles, cg.testModelEntity.axis);
}

/*
=================
CG_TestGun_f

Replaces the current view weapon with the given model
=================
*/
void CG_TestGun_f(void) {
    CG_TestModel_f();

    if (!cg.testModelEntity.hModel) {
        return;
    }

    cg.testGun = qtrue;
    cg.testModelEntity.renderfx = RF_MINLIGHT | RF_DEPTHHACK | RF_FIRST_PERSON;
}

void CG_TestModelNextFrame_f(void) {
    cg.testModelEntity.frame++;
    CG_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelPrevFrame_f(void) {
    cg.testModelEntity.frame--;
    if (cg.testModelEntity.frame < 0) {
        cg.testModelEntity.frame = 0;
    }
    CG_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelNextSkin_f(void) {
    cg.testModelEntity.skinNum++;
    CG_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

void CG_TestModelPrevSkin_f(void) {
    cg.testModelEntity.skinNum--;
    if (cg.testModelEntity.skinNum < 0) {
        cg.testModelEntity.skinNum = 0;
    }
    CG_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

static void CG_AddTestModel(void) {
    int i;

    // re-register the model, because the level may have changed
    cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);
    if (!cg.testModelEntity.hModel) {
        CG_Printf("Can't register model\n");
        return;
    }

    // if testing a gun, set the origin relative to the view origin
    if (cg.testGun) {
        VectorCopy(cg.refdef.vieworg, cg.testModelEntity.origin);
        VectorCopy(cg.refdef.viewaxis[0], cg.testModelEntity.axis[0]);
        VectorCopy(cg.refdef.viewaxis[1], cg.testModelEntity.axis[1]);
        VectorCopy(cg.refdef.viewaxis[2], cg.testModelEntity.axis[2]);

        // allow the position to be adjusted
        for (i = 0; i < 3; i++) {
            cg.testModelEntity.origin[i] += cg.refdef.viewaxis[0][i] * cg_gun_x.value;
            cg.testModelEntity.origin[i] += cg.refdef.viewaxis[1][i] * cg_gun_y.value;
            cg.testModelEntity.origin[i] += cg.refdef.viewaxis[2][i] * cg_gun_z.value;
        }
    }

    trap_R_AddRefEntityToScene(&cg.testModelEntity);
}

//============================================================================

/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/
static void CG_CalcVrect(void) {
    int size;

    // the intermission should allways be full screen
    if (cg.snap->ps.pm_type == PM_INTERMISSION) {
        size = 100;
    } else {
        // bound normal viewsize
        if (cg_viewsize.integer < 30) {
            trap_Cvar_Set("cg_viewsize", "30");
            size = 30;
        } else if (cg_viewsize.integer > 100) {
            trap_Cvar_Set("cg_viewsize", "100");
            size = 100;
        } else {
            size = cg_viewsize.integer;
        }
    }
    cg.refdef.width = cgs.glconfig.vidWidth * size / 100;
    cg.refdef.width &= ~1;

    cg.refdef.height = cgs.glconfig.vidHeight * size / 100;
    cg.refdef.height &= ~1;

    cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width) / 2;
    cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height) / 2;
}

//==============================================================================

/*
===============
CG_OffsetThirdPersonView

===============
*/
#define FOCUS_DISTANCE 512
static void CG_OffsetThirdPersonView(void) {
    vec3_t forward, right, up;
    vec3_t view;
    vec3_t focusAngles;
    trace_t trace;
    static vec3_t mins = {-4, -4, -4};
    static vec3_t maxs = {4, 4, 4};
    vec3_t focusPoint;
    float focusDist;
    float forwardScale, sideScale;

    cg.refdef.vieworg[2] += cg.predictedPlayerState.viewheight;

    VectorCopy(cg.refdefViewAngles, focusAngles);

    // if dead, look at killer
    if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        focusAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
        cg.refdefViewAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
    }

    if (focusAngles[PITCH] > 45) {
        focusAngles[PITCH] = 45;  // don't go too far overhead
    }
    AngleVectors(focusAngles, forward, NULL, NULL);

    VectorMA(cg.refdef.vieworg, FOCUS_DISTANCE, forward, focusPoint);

    VectorCopy(cg.refdef.vieworg, view);

    view[2] += 8;

    cg.refdefViewAngles[PITCH] *= 0.5;

    AngleVectors(cg.refdefViewAngles, forward, right, up);

    forwardScale = cos(cg_thirdPersonAngle.value / 180 * M_PI);
    sideScale = sin(cg_thirdPersonAngle.value / 180 * M_PI);
    VectorMA(view, -cg_thirdPersonRange.value * forwardScale, forward, view);
    VectorMA(view, -cg_thirdPersonRange.value * sideScale, right, view);

    // trace a ray from the origin to the viewpoint to make sure the view isn't
    // in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

    if (!cg_cameraMode.integer) {
        CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);

        if (trace.fraction != 1.0) {
            VectorCopy(trace.endpos, view);
            view[2] += (1.0 - trace.fraction) * 32;
            // try another trace to this position, because a tunnel may have the ceiling
            // close enough that this is poking out

            CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);
            VectorCopy(trace.endpos, view);
        }
    }

    VectorCopy(view, cg.refdef.vieworg);

    // select pitch to look at focus point from vieword
    VectorSubtract(focusPoint, cg.refdef.vieworg, focusPoint);
    focusDist = sqrt(focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1]);
    if (focusDist < 1) {
        focusDist = 1;  // should never happen
    }
    cg.refdefViewAngles[PITCH] = -180 / M_PI * atan2(focusPoint[2], focusDist);
    cg.refdefViewAngles[YAW] -= cg_thirdPersonAngle.value;
}

// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset(void) {
    int timeDelta;

    // smooth out stair climbing
    timeDelta = cg.time - cg.stepTime;
    if (timeDelta < STEP_TIME) {
        cg.refdef.vieworg[2] -= cg.stepChange * (STEP_TIME - timeDelta) / STEP_TIME;
    }
}

/*
===============
CG_OffsetFirstPersonView

===============
*/
static void CG_OffsetFirstPersonView(void) {
    float* origin;
    float* angles;
    float bob;
    float ratio;
    float delta;
    float speed;
    float f;
    vec3_t predictedVelocity;
    int timeDelta;

    if (cg.snap->ps.pm_type == PM_INTERMISSION) {
        return;
    }

    origin = cg.refdef.vieworg;
    angles = cg.refdefViewAngles;

    // if dead, fix the angle and don't add any kick
    if (cg.snap->ps.stats[STAT_HEALTH] <= 0) {
        angles[ROLL] = 40;
        angles[PITCH] = -15;
        angles[YAW] = cg.snap->ps.stats[STAT_DEAD_YAW];
        origin[2] += cg.predictedPlayerState.viewheight;
        return;
    }

    // add angles based on damage kick
    if (cg.damageTime) {
        ratio = cg.time - cg.damageTime;
        if (ratio < DAMAGE_DEFLECT_TIME) {
            ratio /= DAMAGE_DEFLECT_TIME;
            angles[PITCH] += ratio * cg.v_dmg_pitch;
            angles[ROLL] += ratio * cg.v_dmg_roll;
        } else {
            ratio = 1.0 - (ratio - DAMAGE_DEFLECT_TIME) / DAMAGE_RETURN_TIME;
            if (ratio > 0) {
                angles[PITCH] += ratio * cg.v_dmg_pitch;
                angles[ROLL] += ratio * cg.v_dmg_roll;
            }
        }
    }

    // add pitch based on fall kick
#if 0
	ratio = (cg.time - cg.landTime) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	angles[PITCH] += ratio * cg.fall_value;
#endif

    // add angles based on velocity
    VectorCopy(cg.predictedPlayerState.velocity, predictedVelocity);

    delta = DotProduct(predictedVelocity, cg.refdef.viewaxis[0]);
    angles[PITCH] += delta * cg_runpitch.value;

    delta = DotProduct(predictedVelocity, cg.refdef.viewaxis[1]);
    angles[ROLL] -= delta * cg_runroll.value;

    // add angles based on bob

    // make sure the bob is visible even at low speeds
    speed = cg.xyspeed > 200 ? cg.xyspeed : 200;

    delta = cg.bobfracsin * cg_bobpitch.value * speed;
    if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
        delta *= 3;  // crouching
    angles[PITCH] += delta;
    delta = cg.bobfracsin * cg_bobroll.value * speed;
    if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
        delta *= 3;  // crouching accentuates roll
    if (cg.bobcycle & 1)
        delta = -delta;
    angles[ROLL] += delta;

    //===================================

    // add view height
    origin[2] += cg.predictedPlayerState.viewheight;

    // smooth out duck height changes
    timeDelta = cg.time - cg.duckTime;
    if (timeDelta < DUCK_TIME) {
        cg.refdef.vieworg[2] -= cg.duckChange * (DUCK_TIME - timeDelta) / DUCK_TIME;
    }

    // add bob height
    bob = cg.bobfracsin * cg.xyspeed * cg_bobup.value;
    if (bob > 6) {
        bob = 6;
    }

    origin[2] += bob;

    // add fall height
    delta = cg.time - cg.landTime;
    if (delta < LAND_DEFLECT_TIME) {
        f = delta / LAND_DEFLECT_TIME;
        cg.refdef.vieworg[2] += cg.landChange * f;
    } else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME) {
        delta -= LAND_DEFLECT_TIME;
        f = 1.0 - (delta / LAND_RETURN_TIME);
        cg.refdef.vieworg[2] += cg.landChange * f;
    }

    // add step offset
    CG_StepOffset();

    // pivot the eye based on a neck length
#if 0
	{
#define NECK_LENGTH 8
		vec3_t			forward, up;

		cg.refdef.vieworg[2] -= NECK_LENGTH;
		AngleVectors(cg.refdefViewAngles, forward, NULL, up);
		VectorMA(cg.refdef.vieworg, 3, forward, cg.refdef.vieworg);
		VectorMA(cg.refdef.vieworg, NECK_LENGTH, up, cg.refdef.vieworg);
	}
#endif
}

//======================================================================

// [CPMA] cg_zoomAnimationTime, 0-500 ms; Quake 3's ZOOM_TIME was a fixed 150
static int CG_ZoomAnimationTime(void) {
    int t = cg_zoomAnimationTime.integer;

    return (t < 0) ? 0 : (t > 500) ? 500 : t;
}

// [CPMA] reversing a zoom halfway goes on from the current FOV instead of jumping
static void CG_ZoomRestart(void) {
    int duration = CG_ZoomAnimationTime();
    int elapsed = cg.time - cg.zoomTime;

    if (elapsed > duration) {
        elapsed = duration;
    }
    cg.zoomTime = cg.time + elapsed - duration;
}

void CG_ZoomDown_f(void) {
    // QL binary: cg_zoomToggle.integer makes zoom a toggle (vmCvar 0x10A61E60)
    if (cg_zoomToggle.integer) {
        cg.zoomed = !cg.zoomed;
        CG_ZoomRestart();
        return;
    }
    if (cg.zoomed) {
        return;
    }
    cg.zoomed = qtrue;
    CG_ZoomRestart();
}

void CG_ZoomUp_f(void) {
    // QL binary: cg_zoomToggle.integer - ZoomUp is a no-op in toggle mode
    if (cg_zoomToggle.integer) {
        return;
    }
    if (!cg.zoomed) {
        return;
    }
    cg.zoomed = qfalse;
    CG_ZoomRestart();
}

/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
#define WAVE_AMPLITUDE 1
#define WAVE_FREQUENCY 0.4

static int CG_CalcFov(void) {
    float x;
    float phase;
    float v;
    int contents;
    float fov_x, fov_y;
    float zoomFov;
    float f;
    int zoomDuration;
    int inwater;

    // [QL] cgamex86.dll 0x1004d640: the fov the server is told about is the one being zoomed to, with
    // no interpolation and before the underwater warp
    if (cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
        // if in intermission, use a fixed value
        fov_x = 90;
        cg.fovTarget = 90;
    } else if (cg.snap->ps.fov && cg_specFov.integer &&
               ((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback)) {
        // [QL] binary 0x1004d731: with cg_specFov, a followed player (or a demo)
        // is viewed with that player's own FOV, ps.fov (snap +0xc8, ps +0x9c).
        // This used to read stats[STAT_HEALTH], so the view took the followed
        // player's health as its FOV (161 health -> 161 degrees).
        fov_x = (float)cg.snap->ps.fov;
        cg.fovTarget = cg.snap->ps.fov;
    } else {
        fov_x = cg_fov.value;
        if (fov_x < 1) {
            fov_x = 1;
        } else if (fov_x > 130) {
            fov_x = 130;
        }

        // account for zooms
        zoomFov = cg_zoomFov.value;
        if (zoomFov < 1) {
            zoomFov = 1;
        } else if (zoomFov > 130) {
            zoomFov = 130;
        }

        zoomDuration = CG_ZoomAnimationTime();
        f = zoomDuration ? (cg.time - cg.zoomTime) / (float)zoomDuration : 1.0f;
        cg.fovTarget = (int)zoomFov;

        if (cg.zoomed) {
            if (f > 1.0) {
                fov_x = zoomFov;
            } else {
                // QL binary: cg_zoomScaling.integer gates interpolation (vmCvar 0x10A69120)
                if (cg_zoomScaling.integer) {
                    fov_x = fov_x + f * (zoomFov - fov_x);
                } else {
                    fov_x = zoomFov;
                }
            }
        } else {
            // [QL] while the zoom animates out the server is still told the zoomed fov
            if (f <= 1.0 && cg_zoomScaling.integer) {
                fov_x = zoomFov + f * (fov_x - zoomFov);
            } else {
                cg.fovTarget = (int)fov_x;
            }
        }
    }

    x = cg.refdef.width / tan(fov_x / 360 * M_PI);
    fov_y = atan2(cg.refdef.height, x);
    fov_y = fov_y * 360 / M_PI;

    // warp if underwater
    contents = CG_PointContents(cg.refdef.vieworg, -1);
    if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
        v = WAVE_AMPLITUDE * sin(phase);
        fov_x += v;
        fov_y -= v;
        inwater = qtrue;
    } else {
        inwater = qfalse;
    }

    // set it
    cg.refdef.fov_x = fov_x;
    cg.refdef.fov_y = fov_y;

    // [QL] cgamex86.dll 0x1004d88a: zoomed, the sensitivity falls with the view's height, in the same
    // proportion the zoom narrows it, and cg_zoomSensitivity scales that. Without it (0 or less) it is
    // Quake 3's fov_y over 75. The fork ignored cg_zoomSensitivity, so the zoom always turned as fast
    // as Quake 3's.
    if (!cg.zoomed) {
        cg.zoomSensitivity = 1;
    } else if (cg_zoomSensitivity.value > 0) {
        float unzoomedX = cg.refdef.width / tan(cg_fov.value / 360 * M_PI);
        float unzoomedY = atan2(cg.refdef.height, unzoomedX) * 360 / M_PI;

        cg.zoomSensitivity = (cg.refdef.fov_y / unzoomedY) * cg_zoomSensitivity.value;
    } else {
        cg.zoomSensitivity = cg.refdef.fov_y / 75.0;
    }

    return inwater;
}

/*
===============
CG_DamageBlendBlob

===============
*/
static void CG_DamageBlendBlob(void) {
    int t;
    int maxTime;
    refEntity_t ent;

    if (!cg_blood.integer) {
        return;
    }

    if (!cg_screenDamage.integer) {
        return;  // the user disabled this effect
    }

    if (!cg.damageValue) {
        return;
    }

    // if (cg.cameraMode) {
    //	return;
    // }

    // ragePro systems can't fade blends, so don't obscure the screen
    if (cgs.glconfig.hardwareType == GLHW_RAGEPRO) {
        return;
    }

    maxTime = DAMAGE_TIME;
    t = cg.time - cg.damageTime;
    if (t <= 0 || t >= maxTime) {
        return;
    }

    memset(&ent, 0, sizeof(ent));
    ent.reType = RT_SPRITE;
    ent.renderfx = RF_FIRST_PERSON;

    VectorMA(cg.refdef.vieworg, 8, cg.refdef.viewaxis[0], ent.origin);
    VectorMA(ent.origin, cg.damageX * -8, cg.refdef.viewaxis[1], ent.origin);
    VectorMA(ent.origin, cg.damageY * 8, cg.refdef.viewaxis[2], ent.origin);

    ent.radius = cg.damageValue * 3;
    // [QL] 0x1004da79: the shader is chosen, not fixed. The binary tests bloodSpray[0]
    // (0x10a5f46c - the same handle CG_Bleed opens with at 0x10013da0) and takes
    // viewBloodBlend (0x10a5f4f0) when it resolved, viewDamageBlend (0x10a5f4f4) when it
    // did not. bloodSpray1 only resolves with the Workshop gore pack installed, so the
    // damage tint is what a stock Quake Live actually draws, and the blood version is the
    // opt-in. This is the same asset-presence switch that decides gibs in CG_GibPlayer.
    ent.customShader = cgs.media.bloodSprayShaders[0] ? cgs.media.viewBloodShader : cgs.media.viewDamageShader;
    ent.shaderRGBA[0] = 255;
    ent.shaderRGBA[1] = 255;
    ent.shaderRGBA[2] = 255;
    ent.shaderRGBA[3] = 200 * (1.0 - ((float)t / maxTime));
    trap_R_AddRefEntityToScene(&ent);
}

// Address: 0x1004db20
// [QL] CG_FilterAngles -- smooth view angles over a ring buffer window.
// Controlled by cg_filter_angles (<0 clamps to 0, >32 clamps to 31).
// Averages pitch/yaw over the last N frames, handling 350deg yaw wraparound.
#define FILTER_ANGLES_SIZE 32
static void CG_FilterAngles(void) {
    static float lastYaw;
    static float yawHistory[FILTER_ANGLES_SIZE];
    static float pitchHistory[FILTER_ANGLES_SIZE];
    static int historyCount;
    static int historyIndex;
    static int lastFilterSetting;
    float* angles;
    float yawSum, pitchSum;
    int i, idx, count;

    angles = cg.refdefViewAngles;

    if (cg_filter_angles.integer < 0) {
        trap_Cvar_Set("cg_filter_angles", "0");
        return;
    }
    if (cg_filter_angles.integer > FILTER_ANGLES_SIZE) {
        trap_Cvar_Set("cg_filter_angles", va("%d", FILTER_ANGLES_SIZE - 1));
    }

    count = cg_filter_angles.integer;

    // reset the window when the cvar changes
    if (cg_filter_angles.integer != lastFilterSetting) {
        memset(yawHistory, 0, sizeof(yawHistory));
        memset(pitchHistory, 0, sizeof(pitchHistory));
        lastYaw = 0.0f;
        historyCount = 0;
        historyIndex = 0;
        lastFilterSetting = cg_filter_angles.integer;
    }

    // handle yaw wraparound (350+ degree discontinuity)
    if (fabs(angles[YAW] - lastYaw) > 350.0f && historyCount > 1) {
        for (i = 1; i < historyCount; i++) {
            idx = historyIndex - i;
            if (idx < 0) {
                idx += FILTER_ANGLES_SIZE;
            }
            if (angles[YAW] - lastYaw >= 0.0f) {
                yawHistory[idx] += 360.0f;
            } else {
                yawHistory[idx] -= 360.0f;
            }
        }
    }

    lastYaw = angles[YAW];

    yawHistory[historyIndex] = angles[YAW];
    pitchHistory[historyIndex] = angles[PITCH];

    historyCount++;
    if (historyCount > count) {
        historyCount = count;
    }

    yawSum = 0.0f;
    pitchSum = 0.0f;
    idx = historyIndex;
    for (i = historyCount; i > 0; i--) {
        if (idx < 0) {
            idx += FILTER_ANGLES_SIZE;
        }
        yawSum += yawHistory[idx];
        pitchSum += pitchHistory[idx];
        idx--;
    }

    angles[YAW] = yawSum / (float)historyCount;
    angles[PITCH] = pitchSum / (float)historyCount;

    historyIndex = (historyIndex + 1) % FILTER_ANGLES_SIZE;
}

// Address: 0x1004e2e0
// [QL] CG_DrawDuelWeaponStats -- duel spectator weapon-accuracy overlay for the
// 2nd duel player. Reads per-weapon accuracy keys out of that player's
// configstring and draws a two-column table starting at y=300.
// The 14 weapon key strings and the "Enemy"/"You" column labels live in a
// pointer table at 0x10076568 spanning several .rdata regions. The guard and
// configstring fetch are done; the per-weapon draw loop is still a stub.
static void CG_DrawDuelWeaponStats(void) {
    const char* info;

    if (cg.snap->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }

    info = CG_ConfigString(CS_PLAYERS + cgs.clientNum2ndPlayer);
    if (!info[0]) {
        return;
    }

    // TODO: iterate the 14 weapon accuracy keys (table @0x10076568),
    // Info_ValueForKey into a 15x30 buffer, then CG_DrawText two columns per row
    // ("Enemy | You"), y = 300 + row*16. Key/label strings still to be extracted.
}

// Address: 0x1004e480
// [QL] CG_CheckAutoFollow -- once the auto-follow delay elapses, issue a
// "follow <client>" command if the local client is in follow mode, then clear.
static void CG_CheckAutoFollow(void) {
    if (cg.autoFollowClient < 0) {
        return;
    }
    if (cg.autoFollowTime > cg.time) {
        return;
    }

    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        trap_SendConsoleCommand(va("follow %d", cg.autoFollowClient));
    }

    cg.autoFollowClient = -1;
}

// Address: 0x10029770
// [QL] CG_GetLevelTimerMsec -- remaining (countdown) or elapsed (countup) match
// time in msec via *msec. Returns qtrue when a timer is shown. During a warmup
// (cg.warmup, 0x10ab8f4c) there is no timer, except in the round-based modes
// (CA/FT/AD/RR) once a round number has arrived (cg.warmupRound, 0x10ab8f58).
qboolean CG_GetLevelTimerMsec(int* msec) {
    int serverTime;
    int remaining;

    // [QL] 0x10a403e0: freeze/timeout end time, else current time.
    serverTime = cgs.freezeEnd ? cgs.freezeEnd : cg.time;

    if (cg.warmup) {
        switch (cgs.gametype) {
        case GT_CA:
        case GT_FREEZE:
        case GT_AD:
        case GT_RR:
            if (cg.warmupRound != 0) {
                break;
            }
            // fall through
        default:
            *msec = 0;
            return qfalse;
        }
    }

    remaining = (cgs.timelimit * 60000 - serverTime) + cgs.levelStartTime;

    if (cgs.timelimit != 0) {
        if (remaining < 0) {
            // past the limit: report elapsed overtime
            *msec = (serverTime + cgs.timelimit * -60000) - cgs.levelStartTime;
            return qtrue;
        }
        // [QL] 0x10a6e6ac == 1: countdown direction (cg_levelTimerDirection).
        if (cg_levelTimerDirection.integer == 1) {
            *msec = remaining;
            return qtrue;
        }
    }

    // no limit, or count-up mode: report elapsed
    *msec = serverTime - cgs.levelStartTime;
    return qtrue;
}

/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
static int CG_CalcViewValues(void) {
    playerState_t* ps;

    memset(&cg.refdef, 0, sizeof(cg.refdef));

    // strings for in game rendering
    // Q_strncpyz( cg.refdef.text[0], "Park Ranger", sizeof(cg.refdef.text[0]) );
    // Q_strncpyz( cg.refdef.text[1], "19", sizeof(cg.refdef.text[1]) );

    // calculate size of 3D view
    CG_CalcVrect();

    ps = &cg.predictedPlayerState;
    /*
        if (cg.cameraMode) {
            vec3_t origin, angles;
            if (trap_getCameraInfo(cg.time, &origin, &angles)) {
                VectorCopy(origin, cg.refdef.vieworg);
                angles[ROLL] = 0;
                VectorCopy(angles, cg.refdefViewAngles);
                AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
                return CG_CalcFov();
            } else {
                cg.cameraMode = qfalse;
            }
        }
    */
    // intermission view
    if (ps->pm_type == PM_INTERMISSION) {
        VectorCopy(ps->origin, cg.refdef.vieworg);
        VectorCopy(ps->viewangles, cg.refdefViewAngles);
        AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);
        return CG_CalcFov();
    }

    cg.bobcycle = (ps->bobCycle & 128) >> 7;
    cg.bobfracsin = fabs(sin((ps->bobCycle & 127) / 127.0 * M_PI));
    cg.xyspeed = sqrt(ps->velocity[0] * ps->velocity[0] +
                      ps->velocity[1] * ps->velocity[1]);

    // [QL] update speedometer history ring buffer
    if (cg.speedHistoryCount < SPEED_HISTORY_SIZE - 1) {
        cg.speedHistoryCount++;
    }
    cg.speedHistoryIndex++;
    if (cg.speedHistoryIndex > SPEED_HISTORY_SIZE - 1) {
        cg.speedHistoryIndex = 0;
    }
    cg.speedHistory[cg.speedHistoryIndex] = cg.xyspeed;

    VectorCopy(ps->origin, cg.refdef.vieworg);
    VectorCopy(ps->viewangles, cg.refdefViewAngles);

    // [QL] apply angle-smoothing filter if enabled
    if (cg_filter_angles.integer != 0) {
        CG_FilterAngles();
    }

    if (cg_cameraOrbit.integer) {
        if (cg.time > cg.nextOrbitTime) {
            cg.nextOrbitTime = cg.time + cg_cameraOrbitDelay.integer;
            cg_thirdPersonAngle.value += cg_cameraOrbit.value;
        }
    }
    // add error decay
    if (cg_errorDecay.value > 0) {
        int t;
        float f;

        t = cg.time - cg.predictedErrorTime;
        f = (cg_errorDecay.value - t) / cg_errorDecay.value;
        if (f > 0 && f < 1) {
            VectorMA(cg.refdef.vieworg, f, cg.predictedError, cg.refdef.vieworg);
        } else {
            cg.predictedErrorTime = 0;
        }
    }

    if (cg.renderingThirdPerson) {
        // back away from character
        CG_OffsetThirdPersonView();
    } else {
        // offset for local bobbing and kicks
        CG_OffsetFirstPersonView();
    }

    // position eye relative to origin
    AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);

    if (cg.hyperspace) {
        cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
    }

    // field of view
    return CG_CalcFov();
}

/*
=====================
CG_PowerupTimerSounds

[QL] cgamex86.dll 0x1004e050: silent while the match is paused, and never for PW_NONE
=====================
*/
static void CG_PowerupTimerSounds(void) {
    int i;
    int t;

    if (cgs.freezeEnd) {
        return;
    }

    // powerup timers going away
    for (i = 1; i < MAX_POWERUPS; i++) {
        t = cg.snap->ps.powerups[i];
        if (t <= cg.time) {
            continue;
        }
        if (t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME) {
            continue;
        }
        if ((t - cg.time) / POWERUP_BLINK_TIME != (t - cg.oldTime) / POWERUP_BLINK_TIME) {
            trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_ITEM, cgs.media.wearOffSound);
        }
    }
}

/*
=====================
CG_AddBufferedSound

[QL] cgamex86.dll 0x1004e110. Once the match result has been announced nothing else
queues. A new entry plays right away unless the one before it set a later time.
=====================
*/
void CG_AddBufferedSound(sfxHandle_t sfx) {
    if (!sfx || cg.soundBufferLocked) {
        return;
    }
    cg.soundBuffer[cg.soundBufferIn] = sfx;
    cg.soundBufferDelay[cg.soundBufferIn] = SOUNDBUFFER_DELAY;
    if (!cg.soundBufferTime[cg.soundBufferIn]) {
        cg.soundBufferTime[cg.soundBufferIn] = cg.time;
    }
    cg.soundBufferIn = (cg.soundBufferIn + 1) % MAX_SOUNDBUFFER;
    if (cg.soundBufferIn == cg.soundBufferOut) {
        cg.soundBufferOut++;
    }
}

/*
=====================
CG_ResetAnnouncements

[QL] cgamex86.dll 0x1004e180. Drops the pending medals and every queued announcement, but
an announcement that just played still holds back the next one.
=====================
*/
void CG_ResetAnnouncements(void) {
    int pending = 0;

    cg.rewardStack = 0;
    cg.rewardTime = 0;
    memset(cg.rewardCount, 0, sizeof(cg.rewardCount));
    memset(cg.rewardShader, 0, sizeof(cg.rewardShader));
    memset(cg.rewardSound, 0, sizeof(cg.rewardSound));

    if (cg.soundBufferTime[cg.soundBufferOut] > cg.time) {
        pending = cg.soundBufferTime[cg.soundBufferOut];
    }
    memset(cg.soundBuffer, 0, sizeof(cg.soundBuffer));
    memset(cg.soundBufferDelay, 0, sizeof(cg.soundBufferDelay));
    memset(cg.soundBufferTime, 0, sizeof(cg.soundBufferTime));
    cg.soundBufferIn = 0;
    cg.soundBufferOut = 0;
    cg.soundBufferTime[0] = pending;
}

/*
=====================
CG_PlayBufferedSounds

[QL] cgamex86.dll 0x1004e220. One announcement per frame at most, at s_announcerVolume; the
next one waits for this one's delay.
=====================
*/
static void CG_PlayBufferedSounds(void) {
    int out = cg.soundBufferOut;
    int next;

    if (cg.soundBufferTime[out] > cg.time || !cg.soundBuffer[out] || out == cg.soundBufferIn) {
        return;
    }

    next = (out + 1) % MAX_SOUNDBUFFER;
    trap_S_StartLocalSoundVolume(cg.soundBuffer[out], CHAN_ANNOUNCER, s_announcerVolume.value);
    cg.soundBufferTime[next] = cg.soundBufferDelay[out] + cg.time;
    cg.soundBufferTime[out] = 0;
    cg.soundBuffer[out] = 0;
    cg.soundBufferDelay[out] = 0;
    cg.soundBufferOut = next;
}

/*
=================
CG_AddPOIMarkers

[QL] Draw point-of-interest markers as distance-scaled sprites.
POI events mark flag carriers, powerup holders, etc. for team modes.
=================
*/
#define ICON_SCALE_DISTANCE 400.0f

void CG_AddPOIMarkers(void) {
    int i, j;
    refEntity_t ent;
    int team;

    if (!cg_drawSprites.integer) {
        return;
    }

    team = cg.snap->ps.persistant[PERS_TEAM];

    for (i = 0; i < cg.numPoiPics; i++) {
        int endTime = cg.poiPics[i].startTime + cg.poiPics[i].length;

        if (cg.time >= cg.poiPics[i].startTime && cg.time < endTime) {
            // Only show POIs for the opposing team (enemy markers)
            if (cg.poiPics[i].team != team) {
                float dist;
                float minWidth, maxWidth, radius;

                memset(&ent, 0, sizeof(ent));
                VectorCopy(cg.poiPics[i].origin, ent.origin);
                ent.origin[2] += 48;  // float above entity
                ent.reType = RT_SPRITE;
                ent.customShader = cgs.media.poiShader;
                ent.renderfx = RF_DEPTHHACK;

                // Distance-scaled sizing using cg_poiMinWidth / cg_poiMaxWidth
                maxWidth = cg_poiMaxWidth.value;
                minWidth = cg_poiMinWidth.value;
                radius = maxWidth / 2.0f;
                dist = ICON_SCALE_DISTANCE * (maxWidth / 16.0f);

                if (minWidth > 0.1f) {
                    dist *= (16.0f / minWidth);
                }

                ent.radius = radius;
                if (Distance(ent.origin, cg.refdef.vieworg) > dist && minWidth > 0.1f) {
                    ent.radius = radius * (Distance(ent.origin, cg.refdef.vieworg) / dist);
                }

                ent.origin[2] += ent.radius;

                // White color, fade out in last second
                ent.shaderRGBA[0] = 255;
                ent.shaderRGBA[1] = 255;
                ent.shaderRGBA[2] = 255;
                if (endTime - cg.time <= 1000) {
                    ent.shaderRGBA[3] = (byte)(255.0f * (endTime - cg.time) / 1000.0f);
                } else {
                    ent.shaderRGBA[3] = 255;
                }

                trap_R_AddRefEntityToScene(&ent);
            }
        } else {
            // Expired - remove by shifting array down
            for (j = i + 1; j < cg.numPoiPics; j++) {
                cg.poiPics[j - 1] = cg.poiPics[j];
            }
            cg.numPoiPics--;
            i--;
        }
    }
}

/*
==============
CG_HideJoinMenu

[QL] cgamex86.dll 0x1000e930, event 5 of CG_EventHandling (the engine's Escape). The join
menu comes down and cgame gives up the keys, but cg.showJoinMenu stays set, so
CG_UpdateJoinMenu brings the menu back once the in-game menu Escape opened is closed.
==============
*/
void CG_HideJoinMenu(void) {
    extern void Item_RunScript(itemDef_t *item, const char *s);
    menuDef_t *menu;

    // [QL] the binary names "joingame_menu" here (0x1000e897 loads 0x10068804).
    // The string "ingame" does not appear anywhere in cgamex86.dll - that menu
    // belongs to the UI module, and looking it up from cgame only ever returned
    // NULL, so this whole block used to be dead.
    menu = Menus_FindByName("joingame_menu");
    if (menu) {
        if ((menu->window.flags & WINDOW_VISIBLE) && menu->onClose) {
            itemDef_t item;
            memset(&item, 0, sizeof(item));
            Item_RunScript(&item, menu->onClose);
        }
        // [QL] 0x1000e8cb: and flags, 0xffeffff9 - that is
        // ~(WINDOW_FORCED | WINDOW_VISIBLE | WINDOW_HASFOCUS). It clears FORCED,
        // which matters because Menu_Paint sets it, and leaves MOUSEOVER alone.
        menu->window.flags &= ~(WINDOW_FORCED | WINDOW_VISIBLE | WINDOW_HASFOCUS);
    }

    trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_CGAME);
}

/*
==============
CG_CloseMenus

[QL] Binary: cgamex86.dll 0x1000e880
Closes the join menu for good: the same as CG_HideJoinMenu, and cg.showJoinMenu goes
down too. Reached from CG_Team_f (0x100078d9) once the player has picked a side, and
from CG_UpdateJoinMenu (0x1000ea29) as soon as the player stops being a spectator. Retail
leaves cg.showScores alone.
==============
*/
void CG_CloseMenus(void) {
    CG_HideJoinMenu();
    cg.showJoinMenu = qfalse;
    cg.joinMenuCloseTime = cg.time;
}

//=========================================================================

void CG_DrawAdvertisements(void) {
    float* vt;
    int i, j;
    polyVert_t verts[4];

    if (!cgs.adsLoaded) {
        trap_Get_Advertisements(&cgs.numAds, cgs.adverts, cgs.adShaders);
        cgs.adsLoaded = qtrue;
        Com_Printf("ads: %d\n", cgs.numAds);
        for (i = 0; i < cgs.numAds; i++) {
            Com_Printf("ad %d: '%s'\n", i + 1, cgs.adShaders[i]);
        }
    }

    if (cg.hyperspace || cgs.numAds <= 0) {
        return;
    }

    vt = cgs.adverts;

    for (i = 0; i < cgs.numAds; i++) {
        qhandle_t shader;

        // use the BSP shader name directly (e.g. "textures/ad_content/2x1")
        if (!cgs.adShaders[i][0]) {
            continue;
        }
        shader = trap_R_RegisterShader(cgs.adShaders[i]);
        if (!shader) {
            continue;
        }

        // BSP lump provides 4 corner vertices (12 floats) + normal (3) + cellId (1) = 16 per ad
        verts[3].xyz[0] = vt[i * 16 + 0];
        verts[3].xyz[1] = vt[i * 16 + 1];
        verts[3].xyz[2] = vt[i * 16 + 2];

        verts[2].xyz[0] = vt[i * 16 + 3];
        verts[2].xyz[1] = vt[i * 16 + 4];
        verts[2].xyz[2] = vt[i * 16 + 5];

        verts[1].xyz[0] = vt[i * 16 + 6];
        verts[1].xyz[1] = vt[i * 16 + 7];
        verts[1].xyz[2] = vt[i * 16 + 8];

        verts[0].xyz[0] = vt[i * 16 + 9];
        verts[0].xyz[1] = vt[i * 16 + 10];
        verts[0].xyz[2] = vt[i * 16 + 11];

        for (j = 0; j < 4; j++) {
            verts[j].modulate[0] = 255;
            verts[j].modulate[1] = 255;
            verts[j].modulate[2] = 255;
            verts[j].modulate[3] = 255;
        }

        verts[0].st[0] = 0;
        verts[0].st[1] = 1;
        verts[1].st[0] = 0;
        verts[1].st[1] = 0;
        verts[2].st[0] = 1;
        verts[2].st[1] = 0;
        verts[3].st[0] = 1;
        verts[3].st[1] = 1;

        trap_R_AddPolyToScene(shader, 4, verts);
    }
}

/*
=================
CG_WeaponPrimaryFromCvar

[QL] Translate cg_weaponPrimaryQueued cvar string ("hmg", "rl", ...) into a
WP_* index so it can be passed through trap_SetUserCmdValue into the usercmd.
Keyed off the same short-names used by weaponShortName[] in g_items.c:205.
Returns 0 (unchanged) if the cvar is empty or unrecognized.
=================
*/
static int CG_WeaponPrimaryFromCvar(void) {
    static const struct {
        const char* name;
        int wp;
    } map[] = {
        {"g",   WP_GAUNTLET},
        {"mg",  WP_MACHINEGUN},
        {"sg",  WP_SHOTGUN},
        {"gl",  WP_GRENADE_LAUNCHER},
        {"rl",  WP_ROCKET_LAUNCHER},
        {"lg",  WP_LIGHTNING},
        {"rg",  WP_RAILGUN},
        {"pg",  WP_PLASMAGUN},
        {"bfg", WP_BFG},
        {"gh",  WP_GRAPPLING_HOOK},
        {"ng",  WP_NAILGUN},
        {"pl",  WP_PROX_LAUNCHER},
        {"cg",  WP_CHAINGUN},
        {"hmg", WP_HMG},
    };
    char buf[16];
    int i;
    trap_Cvar_VariableStringBuffer("cg_weaponPrimaryQueued", buf, sizeof(buf));
    if (!buf[0]) {
        return 0;
    }
    for (i = 0; i < (int)ARRAY_LEN(map); i++) {
        if (!Q_stricmp(buf, map[i].name)) {
            return map[i].wp;
        }
    }
    return 0;
}

/*
=================
CG_DrawActiveFrame

Generates and draws a game scene and status information at the given time.
=================
*/
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback) {
    cg.time = serverTime;
    cg.demoPlayback = demoPlayback;

    // [QL] refresh the level timer (result also recomputed by the HUD owner-draw)
    {
        int levelTimerMsec;
        CG_GetLevelTimerMsec(&levelTimerMsec);
    }

    // update cvars
    CG_UpdateCvars();

    // if we are only updating the screen as a loading
    // pacifier, don't even try to read snapshots
    if (cg.infoScreenText[0] != 0) {
        CG_DrawInformation();
        return;
    }

    // any looped sounds will be respecified as entities
    // are added to the render list
    trap_S_ClearLoopingSounds(qfalse);

    // clear all the render lists
    trap_R_ClearScene();

    // [QL] age/expire the shared floating-effect pool (damage numbers, outlines,
    // freeze/flag glows, head sprites) before this frame re-adds them during the
    // entity phase. Binary calls this from CG_DrawActiveFrame @ 0x1004e536.
    CG_UpdateFloatingEffects();

    // set up cg.snap and possibly cg.nextSnap
    CG_ProcessSnapshots();

    // if we haven't received any snapshots yet, all
    // we can draw is the information screen
    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        CG_DrawInformation();
        return;
    }

    // let the client system know what our weapon and zoom settings are
    // [QL] 4-arg call matches cgamex86.dll CG_DrawActiveFrame: also passes
    //      queued loadout primary weapon (-> cmd.weaponPrimary) and effective
    //      fov (-> cmd.fov), so the engine can stamp them each frame.
    trap_SetUserCmdValue(cg.weaponSelect, CG_WeaponPrimaryFromCvar(),
                         cg.zoomSensitivity, cg.fovTarget);

    // this counter will be bumped for every valid scene we generate
    cg.clientFrame++;

    // update cg.predictedPlayerState
    CG_PredictPlayerState();

    // [QL] track spectator state transitions for menu visibility
    {
        static int wasSpectating = 0;
        int isSpectating = (cg.predictedPlayerState.pm_type == PM_SPECTATOR);
        if (!wasSpectating && isSpectating) {
            trap_Cvar_Set("cg_spectating", "1");
            trap_Cvar_Update(&cg_spectating);
        } else if (wasSpectating && !isSpectating) {
            trap_Cvar_Set("cg_spectating", "0");
            trap_Cvar_Update(&cg_spectating);
        }
        wasSpectating = isSpectating;
    }

    // decide on third person view
    cg.renderingThirdPerson = cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR && (cg_thirdPerson.integer || (cg.snap->ps.stats[STAT_HEALTH] <= 0));

    // build cg.refdef
    CG_CalcViewValues();

    // first person blend blobs, done after AnglesToAxis
    if (!cg.renderingThirdPerson) {
        CG_DamageBlendBlob();
    }

    // build the render lists
    if (!cg.hyperspace) {
        CG_AddPacketEntities();  // adter calcViewValues, so predicted player state is correct
        CG_AddMarks();
        CG_AddParticles();
        CG_AddLocalEntities();
        CG_AddPOIMarkers();
    }
    CG_AddViewWeapon(&cg.predictedPlayerState);

    // add buffered sounds
    CG_PlayBufferedSounds();
    // [QL] drain buffered voice chats (inert unless a .voice file loaded, matching QL)
    CG_PlayBufferedVoiceChats();

    // finish up the rest of the refdef
    if (cg.testModelEntity.hModel) {
        CG_AddTestModel();
    }
    cg.refdef.time = cg.time;
    memcpy(cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));

    // warning sounds when powerup is wearing off
    CG_PowerupTimerSounds();

    // [QL] spectator auto-follow check
    CG_CheckAutoFollow();

    // update audio positions
    // [QL] 0x1004e7c2 never reports the view as under water, so the local player's own sounds
    // always keep their start volume
    trap_S_Respatialize(cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, 0);

    // make sure the lagometerSample and frame timing isn't done twice when in stereo
    if (stereoView != STEREO_RIGHT) {
        cg.frametime = cg.time - cg.oldTime;
        if (cg.frametime < 0) {
            cg.frametime = 0;
        }
        cg.oldTime = cg.time;
        CG_AddLagometerFrameInfo();
        // [QL] 0x1004e80b: the spectator item timers count down
        CG_UpdateSpecItemTimers();
    }
    if (cg_timescale.value != cg_timescaleFadeEnd.value) {
        if (cg_timescale.value < cg_timescaleFadeEnd.value) {
            cg_timescale.value += cg_timescaleFadeSpeed.value * ((float)cg.frametime) / 1000;
            if (cg_timescale.value > cg_timescaleFadeEnd.value)
                cg_timescale.value = cg_timescaleFadeEnd.value;
        } else {
            cg_timescale.value -= cg_timescaleFadeSpeed.value * ((float)cg.frametime) / 1000;
            if (cg_timescale.value < cg_timescaleFadeEnd.value)
                cg_timescale.value = cg_timescaleFadeEnd.value;
        }
        if (cg_timescaleFadeSpeed.value) {
            trap_Cvar_Set("timescale", va("%f", cg_timescale.value));
        }
    }

    // actually issue the rendering calls
    CG_DrawActive(stereoView);

    // [QL] duel spectator weapon-stats overlay
    CG_DrawDuelWeaponStats();

    if (cg_stats.integer) {
        CG_Printf("cg.clientFrame:%i\n", cg.clientFrame);
    }
}
