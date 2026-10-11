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
#include "../ui/ui_shared.h"

extern vmCvar_t cg_filter_angles;
extern vmCvar_t cg_levelTimerDirection;

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

    cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);
    if (!cg.testModelEntity.hModel) {
        CG_Printf("Can't register model\n");
        return;
    }

    if (cg.testGun) {
        VectorCopy(cg.refdef.vieworg, cg.testModelEntity.origin);
        VectorCopy(cg.refdef.viewaxis[0], cg.testModelEntity.axis[0]);
        VectorCopy(cg.refdef.viewaxis[1], cg.testModelEntity.axis[1]);
        VectorCopy(cg.refdef.viewaxis[2], cg.testModelEntity.axis[2]);

        for (i = 0; i < 3; i++) {
            cg.testModelEntity.origin[i] += cg.refdef.viewaxis[0][i] * cg_gun_x.value;
            cg.testModelEntity.origin[i] += cg.refdef.viewaxis[1][i] * cg_gun_y.value;
            cg.testModelEntity.origin[i] += cg.refdef.viewaxis[2][i] * cg_gun_z.value;
        }
    }

    trap_R_AddRefEntityToScene(&cg.testModelEntity);
}

static void CG_CalcVrect(void) {
    int size;

    if (cg.snap->ps.pm_type == PM_INTERMISSION) {
        size = 100;
    } else {
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

static void CG_ThirdPersonOrbit(void) {
    vec3_t forward;

    cg.refdef.vieworg[2] += cg.predictedPlayerState.viewheight;
    if (cg_thirdPersonAngle.value != 0) {
        cg.refdefViewAngles[YAW] += cg_thirdPersonAngle.value;
    }
    if (cg_thirdPersonPitch.value != -1) {
        cg.refdefViewAngles[PITCH] = cg_thirdPersonPitch.value;
    }
    AngleVectors(cg.refdefViewAngles, forward, NULL, NULL);
    VectorMA(cg.refdef.vieworg, -cg_thirdPersonRange.value, forward, cg.refdef.vieworg);
}

static float CG_ThirdPersonCameraRadius(void) {
    char buf[MAX_CVAR_VALUE_STRING];
    float znear, fovY, x, w, h;
    int aspect;

    trap_Cvar_VariableStringBuffer("r_znear", buf, sizeof(buf));
    znear = atof(buf);
    x = 640 / tan(cg_fov.value / 360 * M_PI);
    fovY = atan2(480, x) * 360 / M_PI;

    trap_Cvar_VariableStringBuffer("r_aspectRatio", buf, sizeof(buf));
    aspect = atoi(buf);
    if (aspect) {
        if (aspect == 1) {
            w = 16;
            h = 9;
        } else if (aspect == 2) {
            w = 16;
            h = 10;
        } else {
            w = 5;
            h = 4;
        }
        x = h / tan(fovY / 360 * M_PI);
        fovY = atan2(w, x) * 360 / M_PI;
    }

    x = znear / tan((90 - fovY * 0.5f) * M_PI / 180);
    return sqrt(znear * znear + x * x);
}

#define FOCUS_DISTANCE 512
static void CG_StepOffset(void);

static void CG_OffsetThirdPersonView(void) {
    vec3_t forward, right, up;
    vec3_t view, start;
    vec3_t focusAngles;
    trace_t trace;
    vec3_t mins = {-4, -4, -4};
    vec3_t maxs = {4, 4, 4};
    vec3_t focusPoint;
    float focusDist;
    float forwardScale, sideScale;
    int timeDelta;

    if (cg_thirdPerson.integer == 2) {
        CG_ThirdPersonOrbit();
        return;
    }

    CG_StepOffset();

    if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }

    cg.refdef.vieworg[2] += cg.predictedPlayerState.viewheight;

    VectorCopy(cg.refdefViewAngles, focusAngles);

    if (cg.snap->ps.stats[STAT_HEALTH] <= 0 && focusAngles[PITCH] > 45) {
        focusAngles[PITCH] = 45;
    }

    timeDelta = cg.time - cg.duckTime;
    if (timeDelta < DUCK_TIME) {
        cg.refdef.vieworg[2] -= cg.duckChange * (DUCK_TIME - timeDelta) / DUCK_TIME;
    }

    AngleVectors(focusAngles, forward, NULL, NULL);

    VectorMA(cg.refdef.vieworg, FOCUS_DISTANCE, forward, focusPoint);

    VectorCopy(cg.refdef.vieworg, view);

    view[2] += 8;

    cg.refdefViewAngles[PITCH] *= 0.5;

    AngleVectors(cg.refdefViewAngles, forward, right, up);

    if (cg_thirdPerson.integer || cg_cameraSmartMode.integer || cg.snap->ps.stats[STAT_HEALTH] <= 0) {
        forwardScale = cos(cg_thirdPersonAngle.value / 180 * M_PI);
        sideScale = sin(cg_thirdPersonAngle.value / 180 * M_PI);
        VectorMA(view, -cg_thirdPersonRange.value * forwardScale, forward, view);
        VectorMA(view, -cg_thirdPersonRange.value * sideScale, right, view);
    }

    if (!cg_cameraMode.integer) {
        if ((!cg_thirdPerson.integer || !cg_cameraThirdPersonSmartMode.integer) && !cg_cameraSmartMode.integer &&
            cg.snap->ps.stats[STAT_HEALTH] > 0) {
            CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);

            if (trace.fraction != 1.0) {
                VectorCopy(trace.endpos, view);
                view[2] += (1.0 - trace.fraction) * 32;

                CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);
                VectorCopy(trace.endpos, view);
            }
        } else {
            char buf[MAX_CVAR_VALUE_STRING];
            float radius = CG_ThirdPersonCameraRadius();
            float znear;

            VectorSet(mins, -radius, -radius, -radius);
            VectorSet(maxs, radius, radius, radius);
            VectorMA(view, cg_thirdPersonRange.value, forward, start);

            CG_Trace(&trace, start, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);
            if (trace.fraction != 1.0) {
                trap_Cvar_VariableStringBuffer("r_znear", buf, sizeof(buf));
                znear = atof(buf);
                VectorMA(trace.endpos, znear, forward, view);
                view[2] += (1.0 - trace.fraction) * 32;

                CG_Trace(&trace, start, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);
                if (trace.fraction != 1.0) {
                    VectorMA(trace.endpos, znear, forward, view);
                }
            }
        }
    }

    VectorCopy(view, cg.refdef.vieworg);

    VectorSubtract(focusPoint, cg.refdef.vieworg, focusPoint);
    focusDist = sqrt(focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1]);
    if (focusDist < 1) {
        focusDist = 1;
    }
    cg.refdefViewAngles[PITCH] = -180 / M_PI * atan2(focusPoint[2], focusDist);
    cg.refdefViewAngles[YAW] -= cg_thirdPersonAngle.value;
}

static void CG_StepOffset(void) {
    int timeDelta;

    timeDelta = cg.time - cg.stepTime;
    if (timeDelta < STEP_TIME) {
        cg.refdef.vieworg[2] -= cg.stepChange * (STEP_TIME - timeDelta) / STEP_TIME;
    }
}

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

    if (cg.snap->ps.stats[STAT_HEALTH] <= 0) {
        angles[ROLL] = 40;
        angles[PITCH] = -15;
        origin[2] += cg.predictedPlayerState.viewheight;
        return;
    }

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

#if 0
	ratio = (cg.time - cg.landTime) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	angles[PITCH] += ratio * cg.fall_value;
#endif

    VectorCopy(cg.predictedPlayerState.velocity, predictedVelocity);

    if (cg_bob.value != 0.0f) {
        speed = cg.xyspeed < 200 ? 200 : cg.xyspeed;

        delta = cg.bobfracsin * cg_bob.value * speed;
        if (cg.predictedPlayerState.pm_flags & PMF_DUCKED) {
            delta += delta;
        }
        bob = delta * 0.005f;
        delta *= 0.002f;

        angles[PITCH] += delta + DotProduct(predictedVelocity, cg.refdef.viewaxis[0]) * 0.002f;
        if (cg.bobcycle & 1) {
            delta = -delta;
        }
        angles[ROLL] += delta - DotProduct(predictedVelocity, cg.refdef.viewaxis[1]) * 0.005f;

        if (bob < 0) {
            bob = 0;
        } else if (bob > 5) {
            bob = 5;
        }
        origin[2] += bob;
    }

    origin[2] += cg.predictedPlayerState.viewheight;

    timeDelta = cg.time - cg.duckTime;
    if (timeDelta < DUCK_TIME) {
        cg.refdef.vieworg[2] -= cg.duckChange * (DUCK_TIME - timeDelta) / DUCK_TIME;
    }

    delta = cg.time - cg.landTime;
    if (delta < LAND_DEFLECT_TIME) {
        f = delta / LAND_DEFLECT_TIME;
        cg.refdef.vieworg[2] += cg.landChange * f;
    } else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME) {
        delta -= LAND_DEFLECT_TIME;
        f = 1.0 - (delta / LAND_RETURN_TIME);
        cg.refdef.vieworg[2] += cg.landChange * f;
    }

    CG_StepOffset();

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

static int CG_ZoomAnimationTime(void) {
    int t = cg_zoomAnimationTime.integer;

    return (t < 0) ? 0 : (t > 500) ? 500 : t;
}

static void CG_ZoomRestart(void) {
    int duration = CG_ZoomAnimationTime();
    int elapsed = cg.time - cg.zoomTime;

    if (elapsed > duration) {
        elapsed = duration;
    }
    cg.zoomTime = cg.time + elapsed - duration;
}

void CG_ZoomDown_f(void) {
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
    if (cg_zoomToggle.integer) {
        return;
    }
    if (!cg.zoomed) {
        return;
    }
    cg.zoomed = qfalse;
    CG_ZoomRestart();
}

static float CG_AspectFovX(float fov_x, float fov_y) {
    char buf[1024];
    float width, height;
    float x;

    trap_Cvar_VariableStringBuffer("r_aspectRatio", buf, sizeof(buf));
    switch (atoi(buf)) {
    case 0:
        return fov_x;
    case 1:
        width = 16;
        height = 9;
        break;
    case 2:
        width = 16;
        height = 10;
        break;
    default:
        width = 5;
        height = 4;
        break;
    }

    x = height / tan(fov_y / 360 * M_PI);
    return atan2(width, x) * 360 / M_PI;
}

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

    if (cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
        fov_x = 90;
        cg.fovTarget = 90;
    } else if (cg.snap->ps.fov && cg_specFov.integer &&
               ((cg.snap->ps.pm_flags & PMF_FOLLOW) || cg.demoPlayback)) {
        fov_x = (float)cg.snap->ps.fov;
        cg.fovTarget = cg.snap->ps.fov;
    } else {
        fov_x = cg_fov.value;
        if (fov_x < 1) {
            fov_x = 1;
        } else if (fov_x > 130) {
            fov_x = 130;
        }

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
                if (cg_zoomScaling.integer) {
                    fov_x = fov_x + f * (zoomFov - fov_x);
                } else {
                    fov_x = zoomFov;
                }
            }
        } else {
            if (f <= 1.0 && cg_zoomScaling.integer) {
                fov_x = zoomFov + f * (fov_x - zoomFov);
            } else {
                cg.fovTarget = (int)fov_x;
            }
        }
    }

    x = 640.0f / tan(fov_x / 360 * M_PI);
    fov_y = atan2(480.0f, x);
    fov_y = fov_y * 360 / M_PI;

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

    cg.refdef.fov_x = CG_AspectFovX(fov_x, fov_y);
    cg.refdef.fov_y = fov_y;

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

static int CG_ScreenDamageColor(void) {
    int attacker = cg.snap->ps.persistant[PERS_ATTACKER];
    int color, alpha;

    if (attacker == cg.snap->ps.clientNum) {
        color = cg_screenDamage_Self.integer;
        alpha = cg_screenDamageAlpha.integer;
    } else if (attacker >= 0 && attacker < MAX_CLIENTS && cgs.gametype >= GT_TEAM &&
               cgs.clientinfo[attacker].team == cgs.clientinfo[cg.snap->ps.clientNum].team) {
        color = cg_screenDamage_Team.integer;
        alpha = cg_screenDamageAlpha_Team.integer;
    } else {
        color = cg_screenDamage.integer;
        alpha = cg_screenDamageAlpha.integer;
    }

    if (!color) {
        return 0;
    }
    return (color & ~0xff) | (alpha & 0xff);
}

static void CG_DamageBlendBlob(void) {
    int t;
    int maxTime;
    refEntity_t ent;

    if (!cg_blood.integer) {
        return;
    }

    if (!cg_screenDamage.integer) {
        return;
    }

    if (!cg.damageValue) {
        return;
    }

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
    ent.customShader = cgs.media.bloodSprayShaders[0] ? cgs.media.viewBloodShader : cgs.media.viewDamageShader;

    {
        int color = CG_ScreenDamageColor();

        if (!color) {
            return;
        }
        ent.shaderRGBA[0] = (byte)((color >> 24) & 0xff);
        ent.shaderRGBA[1] = (byte)((color >> 16) & 0xff);
        ent.shaderRGBA[2] = (byte)((color >> 8) & 0xff);
        ent.shaderRGBA[3] = (byte)((color & 0xff) * (1.0f - ((float)t / maxTime)));
    }
    trap_R_AddRefEntityToScene(&ent);
}

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

    if (cg_filter_angles.integer != lastFilterSetting) {
        memset(yawHistory, 0, sizeof(yawHistory));
        memset(pitchHistory, 0, sizeof(pitchHistory));
        lastYaw = 0.0f;
        historyCount = 0;
        historyIndex = 0;
        lastFilterSetting = cg_filter_angles.integer;
    }

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

static void CG_DrawBotHud(void) {
    static const char* keys[14][2] = {
        {"e", "Enemy   = "}, {"ed", "Dist    = "}, {"tg", "LTG     = "}, {"tgd", "Dist    = "},
        {"sg", "NBG     = "}, {"sgd", "Dist    = "}, {"ainode", "AI Node = "}, {"ltg", "LTGType = "},
        {"ban", "Area#   = "}, {"gan", "G_Area# = "}, {"bh", "Health  = "}, {"ba", "Armor   = "},
        {"sk", "Skill   = "}, {"eh", "Hear_E  = "},
    };
    static vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    char values[14][15];
    const char* info;
    float y;
    int i;

    if (cg.snap->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }
    info = CG_ConfigString(CS_BOTINFO);
    if (!info[0]) {
        return;
    }
    for (i = 0; i < 14; i++) {
        Q_strncpyz(values[i], Info_ValueForKey(info, keys[i][0]), sizeof(values[i]));
    }
    y = 300;
    for (i = 0; i < 14; i += 2) {
        y += 16;
        CG_DrawText(140, y, 0, 0.12f, white,
                    va("%s%15s | %s%15s", keys[i][1], values[i], keys[i + 1][1], values[i + 1]), 0, 0, 3);
    }
}

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

qboolean CG_GetLevelTimerMsec(int* msec) {
    int serverTime;
    int remaining;

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
        default:
            *msec = 0;
            return qfalse;
        }
    }

    remaining = (cgs.timelimit * 60000 - serverTime) + cgs.levelStartTime;

    if (cgs.timelimit != 0) {
        if (remaining < 0) {
            *msec = (serverTime + cgs.timelimit * -60000) - cgs.levelStartTime;
            return qtrue;
        }
        if (cg_levelTimerDirection.integer == 1) {
            *msec = remaining;
            return qtrue;
        }
    }

    *msec = serverTime - cgs.levelStartTime;
    return qtrue;
}

static void CG_FreeCamera(void) {
    static vec3_t origin, offset;
    static int lastTime;
    usercmd_t cmd;
    vec3_t angles, axis[3];
    int now = trap_Milliseconds();
    float seconds = (now - lastTime) * 0.001f;
    int i;

    trap_GetUserCmd(trap_GetCurrentCmdNumber(), &cmd);
    for (i = 0; i < 3; i++) {
        angles[i] = SHORT2ANGLE(cmd.angles[i]);
    }
    if (seconds > 0.5f) {
        VectorCopy(cg.refdef.vieworg, origin);
        VectorSubtract(cg.refdefViewAngles, angles, offset);
        seconds = 0;
    }
    lastTime = now;

    VectorAdd(angles, offset, angles);
    AnglesToAxis(angles, axis);
    VectorMA(origin, cmd.forwardmove * 3.5f * seconds, axis[0], origin);
    VectorMA(origin, -cmd.rightmove * 3.5f * seconds, axis[1], origin);
    VectorMA(origin, cmd.upmove * 3.5f * seconds, axis[2], origin);

    VectorCopy(origin, cg.refdef.vieworg);
    VectorCopy(angles, cg.refdefViewAngles);
}

static int CG_CalcViewValues(void) {
    playerState_t* ps;

    memset(&cg.refdef, 0, sizeof(cg.refdef));

    CG_CalcVrect();

    ps = &cg.predictedPlayerState;
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

    if (cg_filter_angles.integer != 0) {
        CG_FilterAngles();
    }

    if (cg_cameraOrbit.integer) {
        if (cg.time > cg.nextOrbitTime) {
            cg.nextOrbitTime = cg.time + cg_cameraOrbitDelay.integer;
            cg_thirdPersonAngle.value += cg_cameraOrbit.value;
        }
    }
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
        CG_OffsetThirdPersonView();
    } else {
        CG_OffsetFirstPersonView();
    }

    if (cg.demoPlayback && cg_freecam.integer) {
        CG_FreeCamera();
    }

    AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);

    if (cg.hyperspace) {
        cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
    }

    return CG_CalcFov();
}

static void CG_PowerupTimerSounds(void) {
    int i;
    int t;

    if (cgs.freezeEnd) {
        return;
    }

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

void CG_HideJoinMenu(void) {
    extern void Item_RunScript(itemDef_t *item, const char *s);
    menuDef_t *menu;

    menu = Menus_FindByName("joingame_menu");
    if (menu) {
        if ((menu->window.flags & WINDOW_VISIBLE) && menu->onClose) {
            itemDef_t item;
            memset(&item, 0, sizeof(item));
            Item_RunScript(&item, menu->onClose);
        }
        menu->window.flags &= ~(WINDOW_FORCED | WINDOW_VISIBLE | WINDOW_HASFOCUS);
    }

    trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_CGAME);
}

void CG_CloseMenus(void) {
    CG_HideJoinMenu();
    cg.showJoinMenu = qfalse;
    cg.joinMenuCloseTime = cg.time;
}

static int CG_QueuedPrimaryWeapon(void) {
    char* p = cg_weaponPrimaryQueued.string;
    const char* token = COM_ParseExt(&p, qtrue);

    if (!token[0]) {
        return 0;
    }
    return CG_WeaponForShortName(token);
}

static void CG_WeaponConfig(void) {
    static vmCvar_t* const configs[] = {
        &cg_weaponConfig_g, &cg_weaponConfig_mg, &cg_weaponConfig_sg, &cg_weaponConfig_gl,
        &cg_weaponConfig_rl, &cg_weaponConfig_lg, &cg_weaponConfig_rg, &cg_weaponConfig_pg,
        &cg_weaponConfig_bfg, &cg_weaponConfig_gh, &cg_weaponConfig_ng, &cg_weaponConfig_pl,
        &cg_weaponConfig_cg, &cg_weaponConfig_hmg,
    };
    const char* config;

    if (cg.weaponSelect == cg.weaponConfigSelect) {
        return;
    }
    cg.weaponConfigSelect = cg.weaponSelect;
    if (cg.weaponSelect < WP_GAUNTLET || cg.weaponSelect > WP_HMG) {
        return;
    }
    config = configs[cg.weaponSelect - WP_GAUNTLET]->string;
    if (!config[0]) {
        config = cg_weaponConfig.string;
    }
    if (config[0]) {
        trap_SendConsoleCommand(va("%s\n", config));
    }
}

static void CG_AutoAction(void) {
    qboolean live;
    qtime_t now;

    if (cg.demoPlayback || !cg.snap || !ch_autoStats.integer) {
        return;
    }
    live = (!cg.warmup && !cg.intermissionStarted && cg.snap->ps.pm_type != PM_INTERMISSION);

    if (live && !cg.autoActionLive) {
        cg.autoActionLive = qtrue;
        cg.autoActionEnded = qfalse;
        return;
    }
    if (!live) {
        cg.autoActionLive = qfalse;
    }
    if (!cg.intermissionStarted || cg.autoActionEnded) {
        return;
    }
    cg.autoActionEnded = qtrue;
    trap_RealTime(&now);
    trap_SendConsoleCommand(va("condump stats/%s-%04i%02i%02i-%02i%02i%02i.txt\n", cgs.mapname,
                               now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour,
                               now.tm_min, now.tm_sec));
}

void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback) {
    cg.time = serverTime;
    cg.demoPlayback = demoPlayback;

    {
        int levelTimerMsec;
        CG_GetLevelTimerMsec(&levelTimerMsec);
    }

    CG_UpdateCvars();

    if (cg.infoScreenText[0] != 0) {
        CG_DrawInformation();
        return;
    }

    trap_S_ClearLoopingSounds(qfalse);

    trap_R_ClearScene();

    CG_UpdateFloatingEffects();

    CG_ProcessSnapshots();

    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        CG_DrawInformation();
        return;
    }

    trap_SetUserCmdValue(cg.weaponSelect, cg.weaponPrimaryQueued,
                         cg.zoomSensitivity, cg.fovTarget);

    cg.clientFrame++;

    CG_PredictPlayerState();

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

    cg.renderingThirdPerson = ((cgs.svCheats || cg.demoPlayback) && cg_thirdPerson.integer) ||
                              cg.snap->ps.stats[STAT_HEALTH] <= 0;
    if (cg.demoPlayback && cg_freecam.integer) {
        cg.renderingThirdPerson = qtrue;
    }

    CG_CalcViewValues();

    if (!cg.renderingThirdPerson) {
        CG_DamageBlendBlob();
    }

    if (!cg.hyperspace) {
        CG_AddPacketEntities();
        CG_AddMarks();
        CG_AddParticles();
        CG_AddLocalEntities();
    }
    CG_AddViewWeapon(&cg.predictedPlayerState);

    cg.weaponPrimaryQueued = CG_QueuedPrimaryWeapon();

    CG_WeaponConfig();
    CG_AutoAction();

    CG_PlayBufferedSounds();
    CG_PlayBufferedVoiceChats();

    if (cg.testModelEntity.hModel) {
        CG_AddTestModel();
    }
    cg.refdef.time = cg.time;
    memcpy(cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));

    CG_PowerupTimerSounds();

    CG_CheckAutoFollow();

    trap_S_Respatialize(cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, 0);

    if (stereoView != STEREO_RIGHT) {
        cg.frametime = cg.time - cg.oldTime;
        if (cg.frametime < 0) {
            cg.frametime = 0;
        }
        cg.oldTime = cg.time;
        CG_AddLagometerFrameInfo();
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

    CG_DrawActive(stereoView);

    CG_DrawBotHud();

    CG_DemoCursorFrame();

    if (cg_stats.integer) {
        CG_Printf("cg.clientFrame:%i\n", cg.clientFrame);
    }
}
