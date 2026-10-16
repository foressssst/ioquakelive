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
#include "../../ui/menudef.h"

static void CG_MachineGunEjectBrass(centity_t* cent) {
    localEntity_t* le;
    refEntity_t* re;
    vec3_t velocity, xvelocity;
    vec3_t offset, xoffset;
    float waterScale = 1.0f;
    vec3_t v[3];

    if (cg_brassTime.integer <= 0) {
        return;
    }

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    velocity[0] = 0;
    velocity[1] = -50 + 40 * crandom();
    velocity[2] = 100 + 50 * crandom();

    le->leType = LE_FRAGMENT;
    le->startTime = cg.time;
    le->endTime = le->startTime + cg_brassTime.integer + (cg_brassTime.integer / 4) * random();

    le->pos.trType = TR_GRAVITY;
    le->pos.trTime = cg.time - (rand() & 15);

    AnglesToAxis(cent->lerpAngles, v);

    offset[0] = 8;
    offset[1] = -4;
    offset[2] = 24;

    xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
    xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
    xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
    VectorAdd(cent->lerpOrigin, xoffset, re->origin);

    VectorCopy(re->origin, le->pos.trBase);

    if (CG_PointContents(re->origin, -1) & CONTENTS_WATER) {
        waterScale = 0.10f;
    }

    xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
    xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
    xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
    VectorScale(xvelocity, waterScale, le->pos.trDelta);

    AxisCopy(axisDefault, re->axis);
    re->hModel = cgs.media.machinegunBrassModel;

    le->bounceFactor = 0.4 * waterScale;

    le->angles.trType = TR_LINEAR;
    le->angles.trTime = cg.time;
    le->angles.trBase[0] = rand() & 31;
    le->angles.trBase[1] = rand() & 31;
    le->angles.trBase[2] = rand() & 31;
    le->angles.trDelta[0] = 2;
    le->angles.trDelta[1] = 1;
    le->angles.trDelta[2] = 0;

    le->leFlags = LEF_TUMBLE;
    le->leBounceSoundType = LEBS_BRASS;
    le->leMarkType = LEMT_NONE;
}

static void CG_ShotgunEjectBrass(centity_t* cent) {
    localEntity_t* le;
    refEntity_t* re;
    vec3_t velocity, xvelocity;
    vec3_t offset, xoffset;
    vec3_t v[3];
    int i;

    if (cg_brassTime.integer <= 0) {
        return;
    }

    for (i = 0; i < 2; i++) {
        float waterScale = 1.0f;

        le = CG_AllocLocalEntity();
        re = &le->refEntity;

        velocity[0] = 60 + 60 * crandom();
        if (i == 0) {
            velocity[1] = 40 + 10 * crandom();
        } else {
            velocity[1] = -40 + 10 * crandom();
        }
        velocity[2] = 100 + 50 * crandom();

        le->leType = LE_FRAGMENT;
        le->startTime = cg.time;
        le->endTime = le->startTime + cg_brassTime.integer * 3 + cg_brassTime.integer * random();

        le->pos.trType = TR_GRAVITY;
        le->pos.trTime = cg.time;

        AnglesToAxis(cent->lerpAngles, v);

        offset[0] = 8;
        offset[1] = 0;
        offset[2] = 24;

        xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
        xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
        xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
        VectorAdd(cent->lerpOrigin, xoffset, re->origin);
        VectorCopy(re->origin, le->pos.trBase);
        if (CG_PointContents(re->origin, -1) & CONTENTS_WATER) {
            waterScale = 0.10f;
        }

        xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
        xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
        xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
        VectorScale(xvelocity, waterScale, le->pos.trDelta);

        AxisCopy(axisDefault, re->axis);
        re->hModel = cgs.media.shotgunBrassModel;
        le->bounceFactor = 0.3f;

        le->angles.trType = TR_LINEAR;
        le->angles.trTime = cg.time;
        le->angles.trBase[0] = rand() & 31;
        le->angles.trBase[1] = rand() & 31;
        le->angles.trBase[2] = rand() & 31;
        le->angles.trDelta[0] = 1;
        le->angles.trDelta[1] = 0.5;
        le->angles.trDelta[2] = 0;

        le->leFlags = LEF_TUMBLE;
        le->leBounceSoundType = LEBS_BRASS;
        le->leMarkType = LEMT_NONE;
    }
}

static void CG_NailgunEjectBrass(centity_t* cent) {
    localEntity_t* smoke;
    vec3_t origin;
    vec3_t v[3];
    vec3_t offset;
    vec3_t xoffset;
    vec3_t up;

    AnglesToAxis(cent->lerpAngles, v);

    offset[0] = 0;
    offset[1] = -12;
    offset[2] = 24;

    xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
    xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
    xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
    VectorAdd(cent->lerpOrigin, xoffset, origin);

    VectorSet(up, 0, 0, 64);

    smoke = CG_SmokePuff(origin, up, 32, 1, 1, 1, 0.33f, 700, cg.time, 0, 0, cgs.media.smokePuffShader);
    smoke->leType = LE_SCALE_FADE;
}

static qboolean CG_WeaponColorForced(clientInfo_t* ci, int* packedColor) {
    int viewer, owner;
    qboolean teammate;

    if (cg_forceTeamWeaponColor.integer != 1 && cg_forceEnemyWeaponColor.integer != 1) {
        return qfalse;
    }

    viewer = cg.predictedPlayerState.clientNum;
    owner = ci - cgs.clientinfo;

    teammate = CG_ShouldForceTeamSkin(cgs.clientinfo[viewer].team, ci->team);
    if (cgs.gametype < GT_TEAM) {
        teammate = (viewer == owner);
    }
    if (viewer == owner) {
        return qfalse;
    }

    if (teammate) {
        if (cg_forceTeamWeaponColor.integer != 1) {
            return qfalse;
        }
        *packedColor = cg_teamUpperColor.integer;
    } else {
        if (cg_forceEnemyWeaponColor.integer != 1) {
            return qfalse;
        }
        *packedColor = cg_enemyUpperColor.integer;
    }
    return qtrue;
}

static qboolean CG_GetRailColorFloat(clientInfo_t* ci, float* coreColor, byte* ringColor) {
    int color;

    if (!CG_WeaponColorForced(ci, &color)) {
        return qfalse;
    }

    if (coreColor) {
        coreColor[0] = ((color >> 24) & 0xff) / 255.0 * 0.75;
        coreColor[1] = ((color >> 16) & 0xff) / 255.0 * 0.75;
        coreColor[2] = ((color >> 8) & 0xff) / 255.0 * 0.75;
    }
    if (ringColor) {
        ringColor[0] = (cg_teamUpperColor.integer >> 24) & 0xff;
        ringColor[1] = (cg_teamUpperColor.integer >> 16) & 0xff;
        ringColor[2] = (cg_teamUpperColor.integer >> 8) & 0xff;
    }
    return qtrue;
}

static qboolean CG_GetRailColorByte(clientInfo_t* ci, byte* rgb) {
    int color;

    if (!CG_WeaponColorForced(ci, &color)) {
        return qfalse;
    }

    if (rgb) {
        rgb[0] = (int)(((color >> 24) & 0xff) * 0.75);
        rgb[1] = (int)(((color >> 16) & 0xff) * 0.75);
        rgb[2] = (int)(((color >> 8) & 0xff) * 0.75);
    }
    return qtrue;
}

static void CG_RailTrailCore(clientInfo_t* ci, vec3_t start, vec3_t end) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leType = LE_FADE_RGB;
    le->startTime = cg.time;
    le->endTime = cg.time + cg_railTrailTime.value;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    re->shaderTime = cg.time / 1000.0f;
    re->reType = RT_RAIL_RINGS;
    re->customShader = cgs.media.railRingsShader;

    VectorCopy(start, re->origin);
    VectorCopy(end, re->oldorigin);

    if (!CG_GetRailColorFloat(ci, le->color, NULL)) {
        le->color[0] = ci->color2[0] * 0.75;
        le->color[1] = ci->color2[1] * 0.75;
        le->color[2] = ci->color2[2] * 0.75;
    }
    le->color[3] = 1.0f;

    AxisClear(re->axis);
}

void CG_RailTrail(clientInfo_t* ci, vec3_t start, vec3_t end) {
    vec3_t axis[36], move, move2, vec, temp;
    float len;
    int i, j, skip;

    localEntity_t* le;
    refEntity_t* re;

#define RADIUS 4
#define ROTATION 1
#define SPACING 5

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leType = LE_FADE_RGB;
    le->startTime = cg.time;
    le->endTime = cg.time + cg_railTrailTime.value;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    re->shaderTime = cg.time / 1000.0f;
    re->reType = RT_RAIL_CORE;
    re->customShader = cgs.media.railCoreShader;
    re->radius = 256;

    VectorCopy(start, re->origin);
    VectorCopy(end, re->oldorigin);

    if (!CG_GetRailColorFloat(ci, le->color, NULL)) {
        le->color[0] = ci->color1[0] * 0.75;
        le->color[1] = ci->color1[1] * 0.75;
        le->color[2] = ci->color1[2] * 0.75;
    }
    le->color[3] = 1.0f;

    AxisClear(re->axis);

    {
        int viewer = cg.predictedPlayerState.clientNum;
        int owner = ci - cgs.clientinfo;

        if (viewer != owner) {
            VectorMA(re->origin, 5.0f, cg.refdef.viewaxis[1], re->origin);
            VectorMA(re->origin, -4.0f, cg.refdef.viewaxis[2], re->origin);
        } else if (!cg.renderingThirdPerson) {
            float fovOffset = (cg_fov.integer > 90) ? -0.2f * (cg_fov.integer - 90) : 0.0f;
            VectorMA(re->origin, cg_gun_x.value + 4.0f, cg.refdef.viewaxis[0], re->origin);
            VectorMA(re->origin, cg_gun_y.value * 0.75f, cg.refdef.viewaxis[1], re->origin);
            VectorMA(re->origin, fovOffset + cg_gun_z.value - 4.0f, cg.refdef.viewaxis[2], re->origin);
        } else {
            static const char* const shortModels[] = {
                "bitterman", "major", "orbb", "razor", "sarge", "slash", "visor", "xaero",
            };
            float zOffset = -3.5f;

            for (i = 0; i < (int)ARRAY_LEN(shortModels); i++) {
                if (!Q_stricmp(ci->modelName, shortModels[i])) {
                    zOffset = 1.0f;
                    break;
                }
            }
            if (i == (int)ARRAY_LEN(shortModels)) {
                if (!Q_stricmp(ci->modelName, "ranger")) {
                    zOffset = -4.0f;
                } else if (!Q_stricmp(ci->modelName, "uriel")) {
                    zOffset = -8.0f;
                }
            }
            VectorMA(re->origin, -5.5f, cg.refdef.viewaxis[1], re->origin);
            VectorMA(re->origin, zOffset, cg.refdef.viewaxis[2], re->origin);
        }
    }

    if (cg_railStyle.integer != 2) {
        CG_RailTrailCore(ci, re->origin, re->oldorigin);
        return;
    }

    VectorCopy(re->origin, move);
    move[2] -= 4;
    VectorSubtract(end, move, vec);
    len = VectorNormalize(vec);
    PerpendicularVector(temp, vec);
    for (i = 0; i < 36; i++) {
        RotatePointAroundVector(axis[i], vec, temp, i * 10);
    }

    VectorMA(move, 20, vec, move);
    VectorScale(vec, SPACING, vec);

    skip = -1;

    j = 18;
    for (i = 0; i < len; i += SPACING) {
        if (i != skip) {
            skip = i + SPACING;
            le = CG_AllocLocalEntity();
            re = &le->refEntity;
            le->leFlags = LEF_PUFF_DONT_SCALE;
            le->leType = LE_MOVE_SCALE_FADE;
            le->startTime = cg.time;
            le->endTime = cg.time + (i >> 1) + 600;
            le->lifeRate = 1.0 / (le->endTime - le->startTime);

            re->shaderTime = cg.time / 1000.0f;
            re->reType = RT_SPRITE;
            re->radius = 1.1f;
            re->customShader = cgs.media.railRingsShader;

            if (!CG_GetRailColorByte(ci, re->shaderRGBA)) {
                re->shaderRGBA[0] = ci->color2[0] * 255;
                re->shaderRGBA[1] = ci->color2[1] * 255;
                re->shaderRGBA[2] = ci->color2[2] * 255;
            }
            re->shaderRGBA[3] = 255;

            if (!CG_GetRailColorFloat(ci, le->color, NULL)) {
                le->color[0] = ci->color2[0] * 0.75;
                le->color[1] = ci->color2[1] * 0.75;
                le->color[2] = ci->color2[2] * 0.75;
            }
            le->color[3] = 1.0f;

            le->pos.trType = TR_LINEAR;
            le->pos.trTime = cg.time;

            VectorCopy(move, move2);
            VectorMA(move2, RADIUS, axis[j], move2);
            VectorCopy(move2, le->pos.trBase);

            le->pos.trDelta[0] = axis[j][0] * 6;
            le->pos.trDelta[1] = axis[j][1] * 6;
            le->pos.trDelta[2] = axis[j][2] * 6;
        }

        VectorAdd(move, vec, move);

        j = (j + ROTATION) % 36;
    }
}

static void CG_SpawnRailTrail(centity_t* cent, vec3_t origin) {
    clientInfo_t* ci;

    if (cent->currentState.weapon != WP_RAILGUN) {
        return;
    }
    if (!cent->pe.railgunFlash) {
        return;
    }
    cent->pe.railgunFlash = qtrue;
    ci = &cgs.clientinfo[cent->currentState.clientNum];
    CG_RailTrail(ci, origin, cent->pe.railgunImpact);
}

static void CG_GrenadeTrail(centity_t* ent, const weaponInfo_t* wi);

static void CG_RocketTrail(centity_t* ent, const weaponInfo_t* wi) {
    int step;
    vec3_t origin, lastPos;
    int t;
    int startTime, contents;
    int lastContents;
    entityState_t* es;
    vec3_t up;
    localEntity_t* smoke;
    float trailRadius;
    vec3_t leadOffset;

    trailRadius = wi->trailRadius;

    up[0] = 0;
    up[1] = 0;
    up[2] = 0;

    step = 50;

    es = &ent->currentState;
    startTime = ent->trailTime;
    t = step * ((startTime + step) / step);

    BG_EvaluateTrajectory(&es->pos, cg.time, origin);
    contents = CG_PointContents(origin, -1);

    if (es->pos.trType == TR_STATIONARY) {
        ent->trailTime = cg.time;
        return;
    }

    BG_EvaluateTrajectory(&es->pos, ent->trailTime, lastPos);
    lastContents = CG_PointContents(lastPos, -1);

    ent->trailTime = cg.time;

    if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        if (contents & lastContents & CONTENTS_WATER) {
            CG_BubbleTrail(lastPos, origin, 8);
        }
        return;
    }

    if (wi->missileTrailFunc == CG_RocketTrail) {
        trailRadius = cg_smokeRadius_RL.value;
    }
    if (wi->missileTrailFunc == CG_GrenadeTrail) {
        trailRadius = cg_smokeRadius_GL.value;
    }
    if (trailRadius == 0.0f) {
        return;
    }

    CG_ProjectileLeadOffset(ent, leadOffset);
    for (; t <= ent->trailTime; t += step) {
        BG_EvaluateTrajectory(&es->pos, t, lastPos);
        VectorAdd(lastPos, leadOffset, lastPos);

        smoke = CG_SmokePuff(lastPos, up,
                             trailRadius,
                             1, 1, 1, 0.33f,
                             wi->wiTrailTime,
                             t,
                             0,
                             0,
                             cgs.media.smokePuffShader);
        smoke->leType = LE_SCALE_FADE;
    }
}

static void CG_NailTrail(centity_t* ent, const weaponInfo_t* wi) {
    int step;
    vec3_t origin, lastPos;
    int t;
    int startTime, contents;
    int lastContents;
    entityState_t* es;
    vec3_t up;
    localEntity_t* smoke;
    float trailRadius;
    vec3_t leadOffset;

    trailRadius = cg_smokeRadius_NG.value;
    if (trailRadius == 0.0f) {
        return;
    }

    up[0] = 0;
    up[1] = 0;
    up[2] = 0;

    step = 50;

    es = &ent->currentState;
    startTime = ent->trailTime;
    t = step * ((startTime + step) / step);

    BG_EvaluateTrajectory(&es->pos, cg.time, origin);
    contents = CG_PointContents(origin, -1);

    if (es->pos.trType == TR_STATIONARY) {
        ent->trailTime = cg.time;
        return;
    }

    BG_EvaluateTrajectory(&es->pos, ent->trailTime, lastPos);
    lastContents = CG_PointContents(lastPos, -1);

    ent->trailTime = cg.time;

    if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        if (contents & lastContents & CONTENTS_WATER) {
            CG_BubbleTrail(lastPos, origin, 8);
        }
        return;
    }

    CG_ProjectileLeadOffset(ent, leadOffset);
    for (; t <= ent->trailTime; t += step) {
        BG_EvaluateTrajectory(&es->pos, t, lastPos);
        VectorAdd(lastPos, leadOffset, lastPos);

        smoke = CG_SmokePuff(lastPos, up,
                             trailRadius,
                             1, 1, 1, 0.33f,
                             wi->wiTrailTime,
                             t,
                             0,
                             0,
                             cgs.media.nailPuffShader);
        smoke->leType = LE_SCALE_FADE;
    }
}

static void CG_PlasmaTrail(centity_t* cent, const weaponInfo_t* wi) {
    localEntity_t* le;
    refEntity_t* re;
    entityState_t* es;
    vec3_t velocity, xvelocity, origin;
    vec3_t offset, xoffset;
    vec3_t v[3];

    float waterScale = 1.0f;

    if (cg_plasmaStyle.integer != 2) {
        return;
    }

    es = &cent->currentState;

    BG_EvaluateTrajectory(&es->pos, cg.time, origin);
    {
        vec3_t leadOffset;

        CG_ProjectileLeadOffset(cent, leadOffset);
        VectorAdd(origin, leadOffset, origin);
    }

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    velocity[0] = 60 - 120 * crandom();
    velocity[1] = 40 - 80 * crandom();
    velocity[2] = 100 - 200 * crandom();

    le->leType = LE_MOVE_SCALE_FADE;
    le->leFlags = LEF_TUMBLE;
    le->leBounceSoundType = LEBS_NONE;
    le->leMarkType = LEMT_NONE;

    le->startTime = cg.time;
    le->endTime = le->startTime + 600;

    le->pos.trType = TR_GRAVITY;
    le->pos.trTime = cg.time;

    AnglesToAxis(cent->lerpAngles, v);

    offset[0] = 2;
    offset[1] = 2;
    offset[2] = 2;

    xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
    xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
    xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];

    VectorAdd(origin, xoffset, re->origin);
    VectorCopy(re->origin, le->pos.trBase);

    if (CG_PointContents(re->origin, -1) & CONTENTS_WATER) {
        waterScale = 0.10f;
    }

    xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
    xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
    xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
    VectorScale(xvelocity, waterScale, le->pos.trDelta);

    AxisCopy(axisDefault, re->axis);
    re->shaderTime = cg.time / 1000.0f;
    re->reType = RT_SPRITE;
    re->radius = 0.25f;
    re->customShader = cgs.media.railRingsShader;
    le->bounceFactor = 0.3f;

    re->shaderRGBA[0] = wi->flashDlightColor[0] * 63;
    re->shaderRGBA[1] = wi->flashDlightColor[1] * 63;
    re->shaderRGBA[2] = wi->flashDlightColor[2] * 63;
    re->shaderRGBA[3] = 63;

    le->color[0] = wi->flashDlightColor[0] * 0.2;
    le->color[1] = wi->flashDlightColor[1] * 0.2;
    le->color[2] = wi->flashDlightColor[2] * 0.2;
    le->color[3] = 0.25f;

    le->angles.trType = TR_LINEAR;
    le->angles.trTime = cg.time;
    le->angles.trBase[0] = rand() & 31;
    le->angles.trBase[1] = rand() & 31;
    le->angles.trBase[2] = rand() & 31;
    le->angles.trDelta[0] = 1;
    le->angles.trDelta[1] = 0.5;
    le->angles.trDelta[2] = 0;
}
void CG_GrappleTrail(centity_t* owner, const centity_t* hook, const vec3_t start) {
    refEntity_t beam;

    if (owner->currentState.weapon != WP_GRAPPLING_HOOK) {
        return;
    }

    memset(&beam, 0, sizeof(beam));
    BG_EvaluateTrajectory(&hook->currentState.pos, cg.time, beam.oldorigin);
    VectorCopy(start, beam.origin);

    beam.radius = 64;

    beam.reType = RT_RAIL_CORE;
    beam.customShader = cgs.media.grapplingChainShader;

    AxisClear(beam.axis);
    beam.shaderRGBA[0] = 0xff;
    beam.shaderRGBA[1] = 0xff;
    beam.shaderRGBA[2] = 0xff;
    beam.shaderRGBA[3] = 0xff;
    trap_R_AddRefEntityToScene(&beam);
}

static void CG_GrenadeTrail(centity_t* ent, const weaponInfo_t* wi) {
    CG_RocketTrail(ent, wi);
}

int CG_WeaponForShortName(const char* name) {
    static const char* shortNames[] = {
        "g", "mg", "sg", "gl", "rl", "lg", "rg", "pg", "bfg", "gh", "ng", "pl", "cg", "hmg",
    };
    int i;

    for (i = 0; i < (int)ARRAY_LEN(shortNames); i++) {
        if (!Q_stricmp(name, shortNames[i])) {
            return WP_GAUNTLET + i;
        }
    }
    return 0;
}

void CG_RegisterWeapon(int weaponNum) {
    weaponInfo_t* weaponInfo;
    gitem_t *item, *ammo;
    char path[MAX_QPATH];
    vec3_t mins, maxs;
    int i;

    weaponInfo = &cg_weapons[weaponNum];

    if (weaponNum == 0 || weaponNum == 15) {
        return;
    }

    if (weaponInfo->registered) {
        return;
    }

    memset(weaponInfo, 0, sizeof(*weaponInfo));
    weaponInfo->registered = qtrue;

    for (item = bg_itemlist + 1; item->classname; item++) {
        if (item->giType == IT_WEAPON && item->giTag == weaponNum) {
            weaponInfo->item = item;
            break;
        }
    }
    if (!item->classname) {
        CG_Error("Couldn't find weapon %i", weaponNum);
    }
    CG_RegisterItemVisuals(item - bg_itemlist);

    weaponInfo->weaponModel = trap_R_RegisterModel(item->world_model[0]);

    trap_R_ModelBounds(weaponInfo->weaponModel, mins, maxs);
    for (i = 0; i < 3; i++) {
        weaponInfo->weaponMidpoint[i] = mins[i] + 0.5 * (maxs[i] - mins[i]);
    }

    weaponInfo->weaponIcon = trap_R_RegisterShader(item->icon);
    weaponInfo->ammoIcon = trap_R_RegisterShader(item->icon);

    for (ammo = bg_itemlist + 1; ammo->classname; ammo++) {
        if (ammo->giType == IT_AMMO && ammo->giTag == weaponNum) {
            break;
        }
    }
    if (ammo->classname && ammo->world_model[0]) {
        weaponInfo->ammoModel = trap_R_RegisterModel(ammo->world_model[0]);
    }

    COM_StripExtension(item->world_model[0], path, sizeof(path));
    Q_strcat(path, sizeof(path), "_flash.md3");
    weaponInfo->flashModel = trap_R_RegisterModel(path);

    if (weaponNum == WP_GAUNTLET || weaponNum == WP_MACHINEGUN || weaponNum == WP_BFG || weaponNum == WP_CHAINGUN) {
        COM_StripExtension(item->world_model[0], path, sizeof(path));
        Q_strcat(path, sizeof(path), "_barrel.md3");
        weaponInfo->barrelModel = trap_R_RegisterModel(path);
    }

    COM_StripExtension(item->world_model[0], path, sizeof(path));
    Q_strcat(path, sizeof(path), "_hand.md3");
    weaponInfo->handsModel = trap_R_RegisterModel(path);

    if (weaponNum == WP_GRAPPLING_HOOK) {
        COM_StripExtension(item->world_model[0], path, sizeof(path));
        Q_strcat(path, sizeof(path), "_ammo.md3");
        weaponInfo->ammoModel = trap_R_RegisterModel(path);
    }

    if (!weaponInfo->handsModel) {
        weaponInfo->handsModel = trap_R_RegisterModel("models/weapons2/shotgun/shotgun_hand.md3");
    }

    switch (weaponNum) {
        case WP_GAUNTLET:
            MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
            weaponInfo->firingSound = trap_S_RegisterSound("sound/weapons/melee/fstrun.ogg", qfalse);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/melee/fstatck.ogg", qfalse);
            break;

        case WP_LIGHTNING:
            MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
            weaponInfo->readySound = trap_S_RegisterSound("sound/weapons/melee/fsthum.ogg", qfalse);
            weaponInfo->firingSound = trap_S_RegisterSound("sound/weapons/lightning/lg_hum.ogg", qfalse);

            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/lightning/lg_fire.ogg", qfalse);

            for (i = 0; i < NUM_LIGHTNING_STYLES; i++) {
                cgs.media.lightningShader[i] = trap_R_RegisterShader(va("lightningBolt%i", i + 1));
            }

            cgs.media.lightningExplosionModel = trap_R_RegisterModel("models/weaphits/crackle.md3");
            cgs.media.sfx_lghit1 = trap_S_RegisterSound("sound/weapons/lightning/lg_hit.ogg", qfalse);
            cgs.media.sfx_lghit2 = trap_S_RegisterSound("sound/weapons/lightning/lg_hit2.ogg", qfalse);
            cgs.media.sfx_lghit3 = trap_S_RegisterSound("sound/weapons/lightning/lg_hit3.ogg", qfalse);
            break;

        case WP_GRAPPLING_HOOK:
            MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
            weaponInfo->missileModel = trap_R_RegisterModel("models/weapons2/grapple/grapple_hook.md3");
            cgs.media.grapplingChainShader = trap_R_RegisterShader("grapplingChain");
            weaponInfo->readySound = trap_S_RegisterSound("sound/weapons/grapple/grhang.ogg", qfalse);
            weaponInfo->firingSound = trap_S_RegisterSound("sound/weapons/grapple/grfire.ogg", qfalse);
            weaponInfo->pullSound = trap_S_RegisterSound("sound/weapons/grapple/grpull.ogg", qfalse);
            weaponInfo->missileTrailFunc = NULL;
            weaponInfo->missileSound = trap_S_RegisterSound("sound/weapons/grapple/grfire.ogg", qfalse);
            trap_S_RegisterSound("sound/weapons/grapple/grreset.ogg", qfalse);
            break;

        case WP_CHAINGUN:
            weaponInfo->firingSound = trap_S_RegisterSound("sound/weapons/vulcan/wvulfire.ogg", qfalse);
            MAKERGB(weaponInfo->flashDlightColor, 1, 1, 0);
            weaponInfo->flashSound[0] =trap_S_RegisterSound("sound/weapons/vulcan/vulcanf1b.ogg", qfalse);
            weaponInfo->flashSound[1] = trap_S_RegisterSound("sound/weapons/vulcan/vulcanf2b.ogg", qfalse);
            weaponInfo->flashSound[2] = trap_S_RegisterSound("sound/weapons/vulcan/vulcanf3b.ogg", qfalse);
            weaponInfo->flashSound[3] = trap_S_RegisterSound("sound/weapons/vulcan/vulcanf4b.ogg", qfalse);
            weaponInfo->ejectBrassFunc = CG_MachineGunEjectBrass;
            cgs.media.bulletExplosionShader = trap_R_RegisterShader("bulletExplosion");
            break;

        case WP_HMG:
            MAKERGB(weaponInfo->flashDlightColor, 1, 1, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/hmg/machgf1b.ogg", qfalse);
            weaponInfo->flashSound[1] = trap_S_RegisterSound("sound/weapons/hmg/machgf2b.ogg", qfalse);
            weaponInfo->flashSound[2] = trap_S_RegisterSound("sound/weapons/hmg/machgf3b.ogg", qfalse);
            weaponInfo->flashSound[3] = trap_S_RegisterSound("sound/weapons/hmg/machgf4b.ogg", qfalse);
            weaponInfo->ejectBrassFunc = CG_MachineGunEjectBrass;
            cgs.media.bulletExplosionShader = trap_R_RegisterShader("bulletExplosion");
            break;

        case WP_MACHINEGUN:
            MAKERGB(weaponInfo->flashDlightColor, 1, 1, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/machinegun/machgf1b.ogg", qfalse);
            weaponInfo->flashSound[1] = trap_S_RegisterSound("sound/weapons/machinegun/machgf2b.ogg", qfalse);
            weaponInfo->flashSound[2] = trap_S_RegisterSound("sound/weapons/machinegun/machgf3b.ogg", qfalse);
            weaponInfo->flashSound[3] = trap_S_RegisterSound("sound/weapons/machinegun/machgf4b.ogg", qfalse);
            weaponInfo->ejectBrassFunc = CG_MachineGunEjectBrass;
            cgs.media.bulletExplosionShader = trap_R_RegisterShader("bulletExplosion");
            break;

        case WP_SHOTGUN:
            MAKERGB(weaponInfo->flashDlightColor, 1, 1, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/shotgun/sshotf1b.ogg", qfalse);
            weaponInfo->ejectBrassFunc = CG_ShotgunEjectBrass;
            cgs.media.bulletExplosionShader = trap_R_RegisterShader("bulletExplosion");
            break;

        case WP_ROCKET_LAUNCHER:
            weaponInfo->missileModel = trap_R_RegisterModel("models/ammo/rocket/rocket.md3");
            weaponInfo->missileSound = trap_S_RegisterSound("sound/weapons/rocket/rockfly.ogg", qfalse);
            weaponInfo->missileTrailFunc = CG_RocketTrail;
            weaponInfo->missileDlight = 200;
            weaponInfo->wiTrailTime = 2000;
            weaponInfo->trailRadius = 64;

            MAKERGB(weaponInfo->missileDlightColor, 1, 0.75f, 0);
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.75f, 0);

            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/rocket/rocklf1a.ogg", qfalse);
            cgs.media.rocketExplosionShader = trap_R_RegisterShader("rocketExplosion");
            break;

        case WP_PROX_LAUNCHER:
            weaponInfo->missileModel = trap_R_RegisterModel("models/weaphits/proxmine.md3");
            weaponInfo->missileTrailFunc = CG_GrenadeTrail;
            weaponInfo->wiTrailTime = 700;
            weaponInfo->trailRadius = 32;
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.70f, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/proxmine/wstbfire.ogg", qfalse);
            cgs.media.grenadeExplosionShader = trap_R_RegisterShader("grenadeExplosion");
            break;

        case WP_GRENADE_LAUNCHER:
            weaponInfo->missileModel = trap_R_RegisterModel("models/ammo/grenade1.md3");
            weaponInfo->missileTrailFunc = CG_GrenadeTrail;
            weaponInfo->wiTrailTime = 700;
            weaponInfo->trailRadius = 32;
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.70f, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/grenade/grenlf1a.ogg", qfalse);
            cgs.media.grenadeExplosionShader = trap_R_RegisterShader("grenadeExplosion");
            break;

        case WP_NAILGUN:
            weaponInfo->ejectBrassFunc = CG_NailgunEjectBrass;
            weaponInfo->missileTrailFunc = CG_NailTrail;
            weaponInfo->trailRadius = 16;
            weaponInfo->wiTrailTime = 250;
            weaponInfo->missileModel = trap_R_RegisterModel("models/weaphits/nail.md3");
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.75f, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/nailgun/wnalfire.ogg", qfalse);
            break;

        case WP_PLASMAGUN:
            weaponInfo->missileTrailFunc = CG_PlasmaTrail;
            weaponInfo->missileSound = trap_S_RegisterSound("sound/weapons/plasma/lasfly.ogg", qfalse);
            MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/plasma/hyprbf1a.ogg", qfalse);
            cgs.media.plasmaExplosionShader = trap_R_RegisterShader("plasmaExplosion");
            cgs.media.railRingsShader = trap_R_RegisterShader("railDisc");
            break;

        case WP_RAILGUN:
            weaponInfo->readySound = trap_S_RegisterSound("sound/weapons/railgun/rg_hum.ogg", qfalse);
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.5f, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/railgun/railgf1a.ogg", qfalse);
            cgs.media.railExplosionShader = trap_R_RegisterShader("railExplosion");
            cgs.media.railRingsShader = trap_R_RegisterShader("railDisc");
            cgs.media.railCoreShader = trap_R_RegisterShader("railCore");
            break;

        case WP_BFG:
            weaponInfo->readySound = trap_S_RegisterSound("sound/weapons/bfg/bfg_hum.ogg", qfalse);
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.7f, 1);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/bfg/bfg_fire.ogg", qfalse);
            cgs.media.bfgExplosionShader = trap_R_RegisterShader("bfgExplosion");
            weaponInfo->missileModel = trap_R_RegisterModel("models/weaphits/bfg.md3");
            weaponInfo->missileSound = trap_S_RegisterSound("sound/weapons/rocket/rockfly.ogg", qfalse);
            break;

        default:
            MAKERGB(weaponInfo->flashDlightColor, 1, 1, 1);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/rocket/rocklf1a.ogg", qfalse);
            break;
    }
}

void CG_RegisterItemVisuals(int itemNum) {
    itemInfo_t* itemInfo;
    gitem_t* item;

    if (itemNum < 0 || itemNum >= bg_numItems) {
        CG_Error("CG_RegisterItemVisuals: itemNum %d out of range [0-%d]", itemNum, bg_numItems - 1);
    }

    itemInfo = &cg_items[itemNum];
    if (itemInfo->registered) {
        return;
    }

    item = &bg_itemlist[itemNum];

    memset(itemInfo, 0, sizeof(*itemInfo));
    itemInfo->registered = qtrue;

    itemInfo->models[0] = trap_R_RegisterModel(item->world_model[0]);

    itemInfo->icon = trap_R_RegisterShader(item->icon);

    if (item->giType == IT_WEAPON) {
        CG_RegisterWeapon(item->giTag);
    }

    if (item->giType == IT_POWERUP || item->giType == IT_HEALTH ||
        item->giType == IT_ARMOR || item->giType == IT_HOLDABLE) {
        if (item->world_model[1]) {
            itemInfo->models[1] = trap_R_RegisterModel(item->world_model[1]);
        }
    }
}

static int CG_MapTorsoToWeaponFrame(clientInfo_t* ci, int frame) {
    if (frame >= ci->animations[TORSO_DROP].firstFrame && frame < ci->animations[TORSO_DROP].firstFrame + 9) {
        return frame - ci->animations[TORSO_DROP].firstFrame + 6;
    }

    if (frame >= ci->animations[TORSO_ATTACK].firstFrame && frame < ci->animations[TORSO_ATTACK].firstFrame + 6) {
        return 1 + frame - ci->animations[TORSO_ATTACK].firstFrame;
    }

    if (frame >= ci->animations[TORSO_ATTACK2].firstFrame && frame < ci->animations[TORSO_ATTACK2].firstFrame + 6) {
        return 1 + frame - ci->animations[TORSO_ATTACK2].firstFrame;
    }

    return 0;
}

static void CG_CalculateWeaponPosition(vec3_t origin, vec3_t angles) {
    float scale;
    int delta;
    float fracsin;

    VectorCopy(cg.refdef.vieworg, origin);
    VectorCopy(cg.refdefViewAngles, angles);

    if (cg_drawGun.integer != 1) {
        return;
    }

    if (cg.bobcycle & 1) {
        scale = -cg.xyspeed;
    } else {
        scale = cg.xyspeed;
    }

    angles[ROLL] += scale * cg.bobfracsin * 0.005;
    angles[YAW] += scale * cg.bobfracsin * 0.01;
    angles[PITCH] += cg.xyspeed * cg.bobfracsin * 0.005;

    delta = cg.time - cg.landTime;
    if (delta < LAND_DEFLECT_TIME) {
        origin[2] += cg.landChange * 0.25 * delta / LAND_DEFLECT_TIME;
    } else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME) {
        origin[2] += cg.landChange * 0.25 *
                     (LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
    }

#if 0
	delta = cg.time - cg.stepTime;
	if (delta < STEP_TIME / 2) {
		origin[2] -= cg.stepChange * 0.25 * delta / (STEP_TIME / 2);
	} else if (delta < STEP_TIME) {
		origin[2] -= cg.stepChange * 0.25 * (STEP_TIME - delta) / (STEP_TIME / 2);
	}
#endif

    scale = cg.xyspeed + 40;
    fracsin = sin(cg.time * 0.001);
    angles[ROLL] += scale * fracsin * 0.01;
    angles[YAW] += scale * fracsin * 0.01;
    angles[PITCH] += scale * fracsin * 0.01;

    if (cg.snap->ps.weapon == WP_CHAINGUN && cg.snap->ps.weaponstate == WEAPON_FIRING) {
        float fireScale = 1.0 - (1000.0 - (float)cg.snap->ps.weaponTime) / 1000.0;
        float fireSway = fireScale * (float)sin(cg.time * 0.001) * 0.01;
        angles[ROLL] += fireSway * 100.0;
        angles[YAW] += fireSway * 300.0;
        angles[PITCH] += fireSway * 100.0;
    }
}

static void CG_LightningBolt(centity_t* cent, vec3_t origin) {
    trace_t trace;
    refEntity_t beam;
    vec3_t forward;
    vec3_t muzzlePoint, endPoint;
    int anim;

    if (cent->currentState.weapon != WP_LIGHTNING) {
        return;
    }

    memset(&beam, 0, sizeof(beam));

    if ((cent->currentState.number == cg.predictedPlayerState.clientNum) &&
        (cg_trueLightning.value != 0) && !cg.renderingThirdPerson) {
        vec3_t angle;
        int i;

        for (i = 0; i < 3; i++) {
            float a = cent->lerpAngles[i] - cg.refdefViewAngles[i];
            if (a > 180) {
                a -= 360;
            }
            if (a < -180) {
                a += 360;
            }

            angle[i] = cg.refdefViewAngles[i] + a * (1.0 - cg_trueLightning.value);
            if (angle[i] < 0) {
                angle[i] += 360;
            }
            if (angle[i] > 360) {
                angle[i] -= 360;
            }
        }

        AngleVectors(angle, forward, NULL, NULL);
        VectorCopy(cg.refdef.vieworg, muzzlePoint);
    } else {
        anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
        AngleVectors(cent->lerpAngles, forward, NULL, NULL);
        VectorCopy(cent->lerpOrigin, muzzlePoint);

        if (anim == LEGS_WALKCR || anim == LEGS_IDLECR) {
            muzzlePoint[2] += CROUCH_VIEWHEIGHT;
        } else {
            muzzlePoint[2] += DEFAULT_VIEWHEIGHT;
        }
    }

    VectorMA(muzzlePoint, 5, forward, muzzlePoint);

    VectorMA(muzzlePoint, LIGHTNING_RANGE, forward, endPoint);

    if (cgs.playerCylinders) {
        CG_CapsuleTrace(&trace, muzzlePoint, vec3_origin, vec3_origin, endPoint,
                        cent->currentState.number, MASK_SHOT);
    } else {
        CG_Trace(&trace, muzzlePoint, vec3_origin, vec3_origin, endPoint,
                 cent->currentState.number, MASK_SHOT);
    }

    VectorCopy(trace.endpos, beam.oldorigin);

    VectorCopy(origin, beam.origin);

    beam.reType = RT_LIGHTNING;
    beam.radius = 256;

    if (cg_lightningStyle.integer < 1 || cg_lightningStyle.integer > NUM_LIGHTNING_STYLES) {
        trap_Cvar_Set("cg_lightningStyle", "1");
        cg_lightningStyle.integer = 1;
    }

    beam.customShader = cgs.media.lightningShader[cg_lightningStyle.integer - 1];

    trap_R_AddRefEntityToScene(&beam);

    if (cg_lightningImpact.integer && trace.fraction < 1.0) {
        vec3_t angles;
        vec3_t dir;
        float dist;

        VectorSubtract(beam.oldorigin, beam.origin, dir);
        dist = VectorNormalize(dir);

        memset(&beam, 0, sizeof(beam));
        beam.hModel = cgs.media.lightningExplosionModel;

        VectorMA(trace.endpos, -16, dir, beam.origin);

        angles[0] = rand() % 360;
        angles[1] = rand() % 360;
        angles[2] = rand() % 360;
        AnglesToAxis(angles, beam.axis);

        if (cg_lightningImpactCap.integer && dist < cg_lightningImpactCap.integer) {
            float scale = dist / (float)cg_lightningImpactCap.integer;
            if (scale < 0.125f) {
                scale = 0.125f;
            }
            beam.nonNormalizedAxes = qtrue;
            VectorScale(beam.axis[0], scale, beam.axis[0]);
            VectorScale(beam.axis[1], scale, beam.axis[1]);
            VectorScale(beam.axis[2], scale, beam.axis[2]);
        }

        trap_R_AddRefEntityToScene(&beam);
    }
}

#define SPIN_SPEED 0.9
#define COAST_TIME 1000
static float CG_MachinegunSpinAngle(centity_t* cent) {
    int delta;
    float angle;
    float speed;

    delta = cg.time - cent->pe.barrelTime;
    if (cent->pe.barrelSpinning) {
        angle = cent->pe.barrelAngle + delta * SPIN_SPEED;
    } else {
        if (delta > COAST_TIME) {
            delta = COAST_TIME;
        }

        speed = 0.5 * (SPIN_SPEED + (float)(COAST_TIME - delta) / COAST_TIME);
        angle = cent->pe.barrelAngle + delta * speed;
    }

    if (cent->pe.barrelSpinning == !(cent->currentState.eFlags & EF_FIRING)) {
        cent->pe.barrelTime = cg.time;
        cent->pe.barrelAngle = AngleMod(angle);
        cent->pe.barrelSpinning = !!(cent->currentState.eFlags & EF_FIRING);
        if (cent->currentState.weapon == WP_CHAINGUN && !cent->pe.barrelSpinning) {
            trap_S_StartSound(NULL, cent->currentState.number, CHAN_WEAPON,
                              cgs.media.chaingunWindSound);
        }
    }

    return angle;
}

static void CG_AddWeaponWithPowerups(refEntity_t* gun, int powerups, qboolean own) {
    if (powerups & (1 << PW_INVIS)) {
        gun->customShader = cgs.media.invisShader;
        trap_R_AddRefEntityToScene(gun);
        return;
    }
    if (own && cg_drawGun.integer == 3) {
        gun->customShader = cgs.media.ghostWeaponShader;
        gun->shaderRGBA[0] = 255;
        gun->shaderRGBA[1] = 255;
        gun->shaderRGBA[2] = 255;
        gun->shaderRGBA[3] = 255;
        trap_R_AddRefEntityToScene(gun);
        return;
    }

    trap_R_AddRefEntityToScene(gun);

    if (powerups & (1 << PW_BATTLESUIT)) {
        gun->customShader = cgs.media.battleWeaponShader;
        trap_R_AddRefEntityToScene(gun);
    }
    if (powerups & (1 << PW_QUAD)) {
        gun->customShader = cgs.media.quadWeaponShader;
        trap_R_AddRefEntityToScene(gun);
    }
}

static const byte cg_brightWeaponColors[WP_NUM_WEAPONS][3] = {
    { 255, 255, 255 },
    { 0, 215, 255 },
    { 255, 255, 0 },
    { 255, 125, 0 },
    { 1, 143, 17 },
    { 255, 0, 0 },
    { 255, 255, 176 },
    { 2, 232, 1 },
    { 196, 0, 255 },
    { 0, 90, 255 },
    { 81, 170, 220 },
    { 3, 252, 178 },
    { 255, 0, 124 },
    { 182, 182, 182 },
    { 206, 165, 0 },
};

void CG_BrightWeapon(refEntity_t* ent, int weaponNum, int owner, brightWeapon_t kind) {
    int bit, viewerTeam, ownerTeam;

    if (!cg_drawBrightWeapons.integer || weaponNum <= WP_NONE || weaponNum >= WP_NUM_WEAPONS) {
        return;
    }

    if (kind == BRIGHTWEAPON_ON_MAP) {
        bit = 8;
    } else if (kind == BRIGHTWEAPON_OWN_VIEW) {
        bit = 1;
    } else {
        viewerTeam = cgs.clientinfo[cg.predictedPlayerState.clientNum].team;
        ownerTeam = (owner >= 0 && owner < MAX_CLIENTS) ? cgs.clientinfo[owner].team : TEAM_FREE;
        if (viewerTeam == TEAM_SPECTATOR) {
            bit = 16;
        } else if (ownerTeam == viewerTeam && (ownerTeam == TEAM_RED || ownerTeam == TEAM_BLUE)) {
            bit = 2;
        } else {
            bit = 4;
        }
    }
    if (!(cg_drawBrightWeapons.integer & bit)) {
        return;
    }

    ent->customShader = cgs.media.brightWeaponShader;
    ent->shaderRGBA[0] = cg_brightWeaponColors[weaponNum][0];
    ent->shaderRGBA[1] = cg_brightWeaponColors[weaponNum][1];
    ent->shaderRGBA[2] = cg_brightWeaponColors[weaponNum][2];
    ent->shaderRGBA[3] = 255;
}

void CG_AddPlayerWeapon(refEntity_t* parent, playerState_t* ps, centity_t* cent, int team) {
    refEntity_t gun;
    refEntity_t barrel;
    refEntity_t flash;
    vec3_t angles;
    weapon_t weaponNum;
    weaponInfo_t* weapon;
    centity_t* nonPredictedCent;
    orientation_t lerped;
    qboolean own = (cent->currentState.clientNum == cg.snap->ps.clientNum);

    weaponNum = cent->currentState.weapon;

    CG_RegisterWeapon(weaponNum);
    weapon = &cg_weapons[weaponNum];

    if (!ps || ps->pm_type == PM_SPECTATOR) {
        cent->pe.lightningFiring = qfalse;
        cent->pe.continuousFiring = qfalse;
        if ((cent->currentState.eFlags & EF_FIRING) && weapon->firingSound) {
            if (VectorCompare(cg.snap->ps.grapplePoint, vec3_origin)) {
                trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin,
                                       weapon->firingSound);
            } else {
                trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin,
                                       weapon->pullSound);
            }
            if (weaponNum == WP_LIGHTNING) {
                cent->pe.lightningFiring = qtrue;
            }
            if (cg.predictedPlayerState.weapon == WP_GRAPPLING_HOOK) {
                cent->pe.lightningFiring = qtrue;
            }
        } else if (weapon->readySound) {
            trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->readySound);
        }
        if (cent->currentState.eFlags & EF_FIRING) {
            switch (cg.predictedPlayerState.weapon) {
                case WP_MACHINEGUN:
                case WP_PLASMAGUN:
                case WP_CHAINGUN:
                case WP_HMG:
                    cent->pe.continuousFiring = qtrue;
                    break;
                default:
                    break;
            }
        }
    }

    memset(&gun, 0, sizeof(gun));
    VectorCopy(parent->lightingOrigin, gun.lightingOrigin);
    gun.shadowPlane = parent->shadowPlane;
    gun.renderfx = parent->renderfx;

    if (weaponNum == WP_RAILGUN) {
        clientInfo_t* ci = &cgs.clientinfo[cent->currentState.clientNum];
        if (!ps) {
            gun.shaderRGBA[0] = 0;
            gun.shaderRGBA[1] = 255;
            gun.shaderRGBA[2] = 0;
        } else if (ps->weaponstate == WEAPON_FIRING) {
            float fscale = (1.0f - (float)ps->weaponTime / (float)bg_weaponReloadTime[WP_RAILGUN]) * 255.0f;
            gun.shaderRGBA[0] = (byte)(ci->color1[0] * fscale);
            gun.shaderRGBA[1] = (byte)(ci->color1[1] * fscale);
            gun.shaderRGBA[2] = (byte)(ci->color1[2] * fscale);
        } else {
            Byte4Copy(ci->c1RGBA, gun.shaderRGBA);
        }
        gun.shaderRGBA[3] = 255;
    }

    gun.hModel = weapon->weaponModel;
    if (!gun.hModel) {
        return;
    }

    trap_R_LerpTag(&lerped, parent->hModel, parent->oldframe, parent->frame,
                   1.0 - parent->backlerp, "tag_weapon");
    VectorCopy(parent->origin, gun.origin);

    VectorMA(gun.origin, lerped.origin[0], parent->axis[0], gun.origin);
    VectorMA(gun.origin, lerped.origin[1], parent->axis[1], gun.origin);
    VectorMA(gun.origin, lerped.origin[2], parent->axis[2], gun.origin);

    MatrixMultiply(lerped.axis, ((refEntity_t*)parent)->axis, gun.axis);
    gun.backlerp = parent->backlerp;

    CG_BrightWeapon(&gun, weaponNum, cent->currentState.clientNum, ps ? BRIGHTWEAPON_OWN_VIEW : BRIGHTWEAPON_CARRIED);
    CG_AddWeaponWithPowerups(&gun, cent->currentState.powerups, own);

    if (weapon->barrelModel) {
        memset(&barrel, 0, sizeof(barrel));
        VectorCopy(parent->lightingOrigin, barrel.lightingOrigin);
        barrel.shadowPlane = parent->shadowPlane;
        barrel.renderfx = parent->renderfx;

        barrel.hModel = weapon->barrelModel;
        angles[YAW] = 0;
        angles[PITCH] = 0;
        angles[ROLL] = CG_MachinegunSpinAngle(cent);
        AnglesToAxis(angles, barrel.axis);

        CG_PositionRotatedEntityOnTag(&barrel, &gun, weapon->weaponModel, "tag_barrel");

        CG_BrightWeapon(&barrel, weaponNum, cent->currentState.clientNum, ps ? BRIGHTWEAPON_OWN_VIEW : BRIGHTWEAPON_CARRIED);
        CG_AddWeaponWithPowerups(&barrel, cent->currentState.powerups, own);
    }

    if (weapon->ammoModel && !cent->pe.lightningFiring && weaponNum == WP_GRAPPLING_HOOK) {
        memset(&barrel, 0, sizeof(barrel));
        VectorCopy(parent->lightingOrigin, barrel.lightingOrigin);
        barrel.shadowPlane = parent->shadowPlane;
        barrel.renderfx = parent->renderfx;
        angles[YAW] = 0;
        angles[PITCH] = 0;
        angles[ROLL] = 0;
        barrel.hModel = weapon->ammoModel;
        AnglesToAxis(angles, barrel.axis);
        CG_PositionRotatedEntityOnTag(&barrel, &gun, weapon->weaponModel, "tag_ammo");
        CG_BrightWeapon(&barrel, weaponNum, cent->currentState.clientNum, ps ? BRIGHTWEAPON_OWN_VIEW : BRIGHTWEAPON_CARRIED);
        CG_AddWeaponWithPowerups(&barrel, cent->currentState.powerups, own);
    }

    nonPredictedCent = &cg_entities[cent->currentState.number];

    if ((nonPredictedCent - cg_entities) != cent->currentState.clientNum) {
        nonPredictedCent = cent;
    }

    if ((weaponNum == WP_LIGHTNING || weaponNum == WP_GAUNTLET || weaponNum == WP_GRAPPLING_HOOK) && (nonPredictedCent->currentState.eFlags & EF_FIRING)) {
    } else {
        if (cg.time - cent->muzzleFlashTime > MUZZLE_FLASH_TIME && !cent->pe.railgunFlash) {
            return;
        }
    }

    memset(&flash, 0, sizeof(flash));
    VectorCopy(parent->lightingOrigin, flash.lightingOrigin);
    flash.shadowPlane = parent->shadowPlane;
    flash.renderfx = parent->renderfx;

    flash.hModel = weapon->flashModel;
    if (!flash.hModel) {
        return;
    }
    angles[YAW] = 0;
    angles[PITCH] = 0;
    angles[ROLL] = crandom() * 10;
    AnglesToAxis(angles, flash.axis);

    if (weaponNum == WP_RAILGUN) {
        clientInfo_t* ci;

        ci = &cgs.clientinfo[cent->currentState.clientNum];
        if (!CG_GetRailColorFloat(ci, NULL, flash.shaderRGBA)) {
            flash.shaderRGBA[0] = 255 * ci->color1[0];
            flash.shaderRGBA[1] = 255 * ci->color1[1];
            flash.shaderRGBA[2] = 255 * ci->color1[2];
        }
    }

    CG_PositionRotatedEntityOnTag(&flash, &gun, weapon->weaponModel, "tag_flash");
    if (!ps || (cg_muzzleFlash.integer && cg_drawGun.integer)) {
        trap_R_AddRefEntityToScene(&flash);
    }

    if (ps || cg.renderingThirdPerson ||
        cent->currentState.number != cg.predictedPlayerState.clientNum) {
        centity_t* hook = cg.hookEntities[cent->currentState.clientNum];

        if (hook && hook->currentValid) {
            CG_PositionRotatedEntityOnTag(&flash, &gun, weapon->weaponModel, "tag_chain");
            CG_GrappleTrail(nonPredictedCent, hook, flash.origin);
        }

        CG_LightningBolt(nonPredictedCent, flash.origin);

        CG_SpawnRailTrail(nonPredictedCent, flash.origin);

        if ((!ps || cg_muzzleFlash.integer) &&
            (weapon->flashDlightColor[0] || weapon->flashDlightColor[1] || weapon->flashDlightColor[2])) {
            CG_AddEffectLight(flash.origin, 300 + (rand() & 31), weapon->flashDlightColor[0],
                                   weapon->flashDlightColor[1], weapon->flashDlightColor[2]);
        }
    }
}

void CG_AddViewWeapon(playerState_t* ps) {
    float gunX, gunY, gunZ;
    refEntity_t hand;
    centity_t* cent;
    clientInfo_t* ci;
    float fovOffset;
    vec3_t angles;
    weaponInfo_t* weapon;

    if (ps->pm_type == PM_SPECTATOR && !(ps->pm_flags & PMF_FOLLOW)) {
        return;
    }

    if (ps->pm_type == PM_INTERMISSION) {
        return;
    }

    if (ps->pm_type == PM_TUTORIAL) {
        return;
    }
    if (cg_letterboxDrawn) {
        return;
    }

    if (cg.renderingThirdPerson) {
        return;
    }

    if (!cg_drawGun.integer) {
        vec3_t origin;
        sfxHandle_t sfx;
        centity_t* hook;

        if (ps->pm_type == PM_SPECTATOR) {
            cg.predictedPlayerEntity.pe.lightningFiring = qfalse;
            cg.predictedPlayerEntity.pe.continuousFiring = qfalse;
        }

        if (cg.predictedPlayerState.eFlags & EF_FIRING) {
            VectorCopy(cg.refdef.vieworg, origin);
            VectorMA(origin, -8, cg.refdef.viewaxis[2], origin);
            CG_LightningBolt(&cg_entities[ps->clientNum], origin);
            hook = cg.hookEntities[cg.predictedPlayerEntity.currentState.clientNum];
            if (hook && hook->currentValid) {
                CG_GrappleTrail(&cg_entities[ps->clientNum], hook, origin);
            }
        }

        if (ps->pm_type != PM_SPECTATOR) {
            return;
        }
        CG_RegisterWeapon(ps->weapon);
        weapon = &cg_weapons[ps->weapon];
        if (cg.predictedPlayerState.eFlags & EF_FIRING) {
            cg.predictedPlayerEntity.pe.lightningFiring = qtrue;
            sfx = weapon->pullSound;
            if (!sfx || VectorCompare(cg.snap->ps.grapplePoint, vec3_origin)) {
                sfx = weapon->firingSound;
            }
        } else {
            sfx = weapon->readySound;
        }
        if (sfx) {
            trap_S_AddLoopingSound(cg.predictedPlayerEntity.currentState.number,
                                   cg.predictedPlayerEntity.lerpOrigin, vec3_origin, sfx);
        }
        return;
    }

    if (cg.testGun) {
        return;
    }

    if (cg_fov.integer > 90) {
        fovOffset = -0.2 * (cg_fov.integer - 90);
    } else {
        fovOffset = 0;
    }

    cent = &cg.predictedPlayerEntity;
    CG_RegisterWeapon(ps->weapon);
    weapon = &cg_weapons[ps->weapon];

    memset(&hand, 0, sizeof(hand));

    CG_CalculateWeaponPosition(hand.origin, angles);

    gunX = cg_gun_x.value;
    gunY = cg_gun_y.value;
    gunZ = cg_gun_z.value;
    if (ps->weapon == WP_GRAPPLING_HOOK) {
        gunX += 8.0f;
        gunY -= 2.0f;
        gunZ -= 1.0f;
    }
    VectorMA(hand.origin, gunX, cg.refdef.viewaxis[0], hand.origin);
    VectorMA(hand.origin, gunY, cg.refdef.viewaxis[1], hand.origin);
    VectorMA(hand.origin, (gunZ + fovOffset), cg.refdef.viewaxis[2], hand.origin);

    AnglesToAxis(angles, hand.axis);

    if (cg_gun_frame.integer) {
        hand.frame = hand.oldframe = cg_gun_frame.integer;
        hand.backlerp = 0;
    } else {
        ci = &cgs.clientinfo[cent->currentState.clientNum];
        hand.frame = CG_MapTorsoToWeaponFrame(ci, cent->pe.torso.frame);
        hand.oldframe = CG_MapTorsoToWeaponFrame(ci, cent->pe.torso.oldFrame);
        hand.backlerp = cent->pe.torso.backlerp;
    }

    hand.hModel = weapon->handsModel;
    hand.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;

    CG_AddPlayerWeapon(&hand, ps, &cg.predictedPlayerEntity, ps->persistant[PERS_TEAM]);
}

void CG_DrawWeaponSelect(void) {
    int i;
    int bits;
    int count;
    int x, y;
    char* name;
    float* color;

    if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        return;
    }

    color = CG_FadeColor(cg.weaponSelectTime, WEAPON_SELECT_TIME);
    if (!color) {
        return;
    }
    trap_R_SetColor(color);

    cg.itemPickupTime = 0;

    bits = cg.snap->ps.stats[STAT_WEAPONS];
    count = 0;
    for (i = 1; i < MAX_WEAPONS; i++) {
        if (bits & (1 << i)) {
            count++;
        }
    }

    x = 320 - count * 20;
    y = 380;

    for (i = 1; i < MAX_WEAPONS; i++) {
        if (!(bits & (1 << i))) {
            continue;
        }

        CG_RegisterWeapon(i);

        CG_DrawPic(x, y, 32, 32, cg_weapons[i].weaponIcon);

        if (i == cg.weaponSelect) {
            CG_DrawPic(x - 4, y - 4, 40, 40, cgs.media.selectShader);
        }

        if (!cg.snap->ps.ammo[i]) {
            CG_DrawPic(x, y, 32, 32, cgs.media.noammoShader);
        }

        x += 40;
    }

    if (cg_weapons[cg.weaponSelect].item) {
        float scale = 0.4f;
        float tw;
        name = cg_weapons[cg.weaponSelect].item->pickup_name;
        if (name) {
            tw = CG_Text_Width(name, scale, 0);
            CG_Text_Paint(320 - tw / 2, y - 12, scale, color, name, 0, 0, 6 );
        }
    }

    trap_R_SetColor(NULL);
}

void CG_DrawWeaponBar(void) {
    int i;
    int bits;
    int count;
    float x, y;
    float xStep, yStep;
    int ammoOffset;
    int textAlign;
    int selectedWeapon;
    int isSpectating;
    int lowAmmoThreshold;
    vec4_t colorNormal = { 1.0f, 1.0f, 1.0f, 1.0f };
    vec4_t colorLow    = { 1.0f, 0.8f, 0.2f, 0.8f };
    vec4_t colorEmpty   = { 1.0f, 0.0f, 0.0f, 0.8f };

    if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        return;
    }
    if (cg_weaponBar.integer == 0) {
        return;
    }

    isSpectating = (cg.predictedPlayerState.eFlags & EF_SPECTATOR) ||
                   cg.predictedPlayerState.pm_type == PM_SPECTATOR;
    if (isSpectating && cg.scoreBoardShowing) {
        return;
    }

    if (cg_weaponBar.integer == 4) {
        CG_DrawWeaponSelect();
        return;
    }

    bits = cg.snap->ps.stats[STAT_WEAPONS];
    selectedWeapon = isSpectating ? cg.snap->ps.weapon : cg.weaponSelect;

    count = 0;
    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        if (bits & (1 << i)) {
            count++;
        } else if (cg_drawFullWeaponBar.integer && cg_weapons[i].weaponIcon && !isSpectating) {
            count++;
        }
    }

    if (count == 0) {
        return;
    }

    if (!isSpectating && cg_weaponBar.integer != 3) {
        if (cg_weaponBar.integer == 2) {
            x = 623.0f;
            ammoOffset = -3;
            textAlign = 16;
        } else {
            x = 1.0f;
            ammoOffset = 20;
            textAlign = 2;
        }
        y = 94.0f;
        xStep = 0.0f;
        yStep = 22.0f;
    } else {
        x = 320.0f - (float)count * 55.0f * 0.5f;
        y = isSpectating ? 452.0f : 413.0f;
        xStep = 55.0f;
        yStep = 0.0f;
        ammoOffset = 20;
        textAlign = 8;
    }

    trap_R_SetColor(NULL);

    if (!isSpectating && cg_weaponBar.integer != 3) {
        CG_SetWidescreen(cg_weaponBar.integer == 2 ? WIDESCREEN_RIGHT : WIDESCREEN_LEFT);
    } else {
        CG_SetWidescreen(WIDESCREEN_CENTER);
    }

    lowAmmoThreshold = (int)cg_lowAmmoWeaponBarWarning.value;

    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        qboolean hasWeapon;
        qhandle_t icon;
        int ammo;

        hasWeapon = (bits & (1 << i)) != 0;

        if (!hasWeapon) {
            if (!cg_drawFullWeaponBar.integer || isSpectating) {
                continue;
            }
            CG_RegisterWeapon(i);
            icon = cg_weapons[i].weaponIcon;
            if (!icon) {
                continue;
            }
        } else {
            CG_RegisterWeapon(i);
            icon = cg_weapons[i].weaponIcon;
            if (!icon) {
                continue;
            }
        }

        if (i == selectedWeapon) {
            if (cg_weaponBar.integer == 2) {
                CG_DrawPic(x - 33.0f, y - 2.0f, 52.0f, 20.0f,
                           cgs.media.weaponBarHighlightShader);
            } else {
                CG_DrawPic(x - 4.0f, y - 2.0f, 52.0f, 20.0f,
                           cgs.media.weaponBarHighlightShader);
            }
        }

        if (hasWeapon || cg_drawFullWeaponBar.integer) {
            CG_DrawPic(x, y, 16.0f, 16.0f, icon);
        }

        if (hasWeapon) {
            ammo = cg.snap->ps.ammo[i];
            if (ammo < 0) {
                CG_DrawPic(x + (float)ammoOffset - (float)textAlign, y, 16.0f, 16.0f,
                           cgs.media.infiniteAmmoShader);
            } else {
                char ammoStr[8];
                vec4_t *ammoColor;
                float textX;

                Com_sprintf(ammoStr, sizeof(ammoStr), "%i", ammo);

                if (ammo == 0) {
                    ammoColor = &colorEmpty;
                } else if (lowAmmoThreshold > 0 && ammo < lowAmmoThreshold) {
                    ammoColor = &colorLow;
                } else {
                    ammoColor = &colorNormal;
                }

                textX = x + (float)ammoOffset;
                if (cg_weaponBar.integer == 2) {
                    float tw = CG_Text_Width(ammoStr, 0.24f, 0);
                    textX = x + (float)ammoOffset - tw;
                }

                CG_Text_Paint(textX, y + 13.0f, 0.24f, *ammoColor, ammoStr, 0, 0, 0);
            }

            if (ammo == 0) {
                CG_DrawPic(x, y, 16.0f, 16.0f, cgs.media.noammoShader);
            }
        }

        x += xStep;
        y += yStep;
    }

    CG_SetWidescreen(WIDESCREEN_STRETCH);
    trap_R_SetColor(NULL);
}

static qboolean CG_WeaponSelectable(int i) {
    if (!cg.snap->ps.ammo[i]) {
        return qfalse;
    }
    if (!(cg.snap->ps.stats[STAT_WEAPONS] & (1 << i))) {
        return qfalse;
    }

    return qtrue;
}

void CG_NextWeapon_f(void) {
    int i;
    int original;

    if (!cg.snap) {
        return;
    }
    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    cg.weaponSelectTime = cg.time;
    original = cg.weaponSelect;

    for (i = 0; i < MAX_WEAPONS; i++) {
        cg.weaponSelect++;
        if (cg.weaponSelect == MAX_WEAPONS) {
            cg.weaponSelect = 0;
        }
        if (cg.weaponSelect == WP_GAUNTLET) {
            continue;
        }
        if (CG_WeaponSelectable(cg.weaponSelect)) {
            break;
        }
    }
    if (i == MAX_WEAPONS) {
        cg.weaponSelect = original;
    }
}

void CG_PrevWeapon_f(void) {
    int i;
    int original;

    if (!cg.snap) {
        return;
    }
    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    cg.weaponSelectTime = cg.time;
    original = cg.weaponSelect;

    for (i = 0; i < MAX_WEAPONS; i++) {
        cg.weaponSelect--;
        if (cg.weaponSelect == -1) {
            cg.weaponSelect = MAX_WEAPONS - 1;
        }
        if (cg.weaponSelect == WP_GAUNTLET) {
            continue;
        }
        if (CG_WeaponSelectable(cg.weaponSelect)) {
            break;
        }
    }
    if (i == MAX_WEAPONS) {
        cg.weaponSelect = original;
    }
}

void CG_Weapon_f(void) {
    int num;

    if (!cg.snap) {
        return;
    }
    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    num = atoi(CG_Argv(1));

    if (num < 1 || num > MAX_WEAPONS - 1) {
        return;
    }

    cg.weaponSelectTime = cg.time;

    if (!(cg.snap->ps.stats[STAT_WEAPONS] & (1 << num))) {
        return;
    }

    if (!cg.snap->ps.ammo[num] && !cg_switchToEmpty.integer) {
        return;
    }

    cg.weaponSelect = num;
}

void CG_OutOfAmmoChange(void) {
    int i;

    cg.weaponSelectTime = cg.time;

    for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
        if (CG_WeaponSelectable(i)) {
            cg.weaponSelect = i;
            break;
        }
    }
}

void CG_DropWeaponChange(int dropped) {
    int i;

    cg.weaponSelectTime = cg.time;

    for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
        if (i != dropped && CG_WeaponSelectable(i)) {
            cg.weaponSelect = i;
            break;
        }
    }
}

void CG_FireWeapon(centity_t* cent) {
    entityState_t* ent;
    int c;
    weaponInfo_t* weap;
    centity_t* nonPredictedCent;
    static int lastQuadSoundTime;

    ent = &cent->currentState;
    if (ent->weapon == WP_NONE) {
        return;
    }
    if (ent->weapon >= WP_NUM_WEAPONS) {
        CG_Error("CG_FireWeapon: ent->weapon >= WP_NUM_WEAPONS");
        return;
    }
    weap = &cg_weapons[ent->weapon];

    cent->muzzleFlashTime = cg.time;

    nonPredictedCent = cent;
    if (ent->eFlags & EF_PLAYER_EVENT) {
        nonPredictedCent = &cg_entities[ent->number];
    }
    if (nonPredictedCent->pe.lightningFiring) {
        return;
    }

    for (c = 0; c < 4; c++) {
        if (!weap->flashSound[c]) {
            break;
        }
    }
    if (c > 0) {
        c = rand() % c;
        if (weap->flashSound[c]) {
            trap_S_StartSound(NULL, ent->number, CHAN_WEAPON, weap->flashSound[c]);
        }
    }

    if (weap->ejectBrassFunc && cg_brassTime.integer > 0) {
        weap->ejectBrassFunc(cent);
    }

    if ((!nonPredictedCent->pe.continuousFiring || cg.time - lastQuadSoundTime > 999) &&
        (cent->currentState.powerups & (1 << PW_QUAD))) {
        trap_S_StartSound(NULL, cent->currentState.number, CHAN_ITEM, cgs.media.quadSound);
        lastQuadSoundTime = cg.time;
    }
}

void CG_MissileHitWall(int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType) {
    qhandle_t mod;
    qhandle_t mark;
    qhandle_t shader;
    sfxHandle_t sfx;
    float radius;
    float light;
    vec3_t lightColor;
    localEntity_t* le;
    int r;
    qboolean alphaFade;
    qboolean isSprite;
    int duration;
    vec3_t sprOrg;
    vec3_t sprVel;

    mod = 0;
    shader = 0;
    light = 0;
    lightColor[0] = 1;
    lightColor[1] = 1;
    lightColor[2] = 0;

    isSprite = qfalse;
    duration = 600;

    switch (weapon) {
        default:
        case WP_NAILGUN:
            if (soundType == IMPACTSOUND_FLESH) {
                sfx = cgs.media.sfx_nghitflesh;
            } else if (soundType == IMPACTSOUND_METAL) {
                sfx = cgs.media.sfx_nghitmetal;
            } else {
                sfx = cgs.media.sfx_nghit;
            }
            mark = cgs.media.holeMarkShader;
            radius = 12;
            break;
        case WP_LIGHTNING:
            r = rand() & 3;
            if (r < 2) {
                sfx = cgs.media.sfx_lghit2;
            } else if (r == 2) {
                sfx = cgs.media.sfx_lghit1;
            } else {
                sfx = cgs.media.sfx_lghit3;
            }
            mark = cgs.media.holeMarkShader;
            radius = 12;
            break;
        case WP_PROX_LAUNCHER:
            mod = cgs.media.dishFlashModel;
            shader = cgs.media.grenadeExplosionShader;
            sfx = cgs.media.sfx_proxexp;
            mark = cgs.media.burnMarkShader;
            radius = 64;
            light = 300;
            isSprite = qtrue;
            break;
        case WP_GRENADE_LAUNCHER:
            mod = cgs.media.dishFlashModel;
            shader = cgs.media.grenadeExplosionShader;
            sfx = cgs.media.sfx_rockexp;
            mark = cgs.media.burnMarkShader;
            radius = 64;
            light = 300;
            isSprite = qtrue;
            break;
        case WP_ROCKET_LAUNCHER:
            mod = cgs.media.dishFlashModel;
            shader = cgs.media.rocketExplosionShader;
            sfx = cgs.media.sfx_rockexp;
            mark = cgs.media.burnMarkShader;
            radius = 64;
            light = 300;
            isSprite = qtrue;
            duration = 1000;
            lightColor[0] = 1;
            lightColor[1] = 0.75;
            lightColor[2] = 0.0;
            if (cg_rocketStyle.integer == 2) {
                VectorMA(origin, 24, dir, sprOrg);
                VectorScale(dir, 64, sprVel);

                CG_ParticleExplosion("explode1", sprOrg, sprVel, 1400, 20, 30);
            }
            break;
        case WP_RAILGUN:
            mod = cgs.media.ringFlashModel;
            shader = cgs.media.railExplosionShader;
            sfx = cgs.media.sfx_plasmaexp;
            mark = cgs.media.energyMarkShader;
            radius = 24;
            break;
        case WP_PLASMAGUN:
            mod = cgs.media.ringFlashModel;
            shader = cgs.media.plasmaExplosionShader;
            sfx = cgs.media.sfx_plasmaexp;
            mark = cgs.media.energyMarkShader;
            radius = 16;
            break;
        case WP_BFG:
            mod = cgs.media.dishFlashModel;
            shader = cgs.media.bfgExplosionShader;
            sfx = cgs.media.sfx_rockexp;
            mark = cgs.media.burnMarkShader;
            radius = 32;
            isSprite = qtrue;
            break;
        case WP_SHOTGUN:
            mod = cgs.media.bulletFlashModel;
            shader = cgs.media.bulletExplosionShader;
            mark = cgs.media.bulletMarkShader;
            sfx = 0;
            radius = 4;
            break;
        case WP_GRAPPLING_HOOK:
            sfx = cgs.media.grappleHitSound;
            mark = cgs.media.crackedMarkShader;
            radius = 16;
            break;
        case WP_CHAINGUN:
            mod = cgs.media.bulletFlashModel;
            if (soundType == IMPACTSOUND_FLESH) {
                sfx = cgs.media.sfx_chghitflesh;
            } else if (soundType == IMPACTSOUND_METAL) {
                sfx = cgs.media.sfx_chghitmetal;
            } else {
                sfx = cgs.media.sfx_chghit;
            }
            mark = cgs.media.bulletMarkShader;

            radius = 8;
            break;
        case WP_MACHINEGUN:
        case WP_HMG:
            mod = cgs.media.bulletFlashModel;
            shader = cgs.media.bulletExplosionShader;
            mark = cgs.media.bulletMarkShader;

            r = rand() & 3;
            if (r == 0) {
                sfx = cgs.media.sfx_ric1;
            } else if (r == 1) {
                sfx = cgs.media.sfx_ric2;
            } else {
                sfx = cgs.media.sfx_ric3;
            }

            radius = 8;
            break;
    }

    if (sfx) {
        trap_S_StartSound(origin, ENTITYNUM_WORLD, CHAN_AUTO, sfx);
    }

    if (mod) {
        le = CG_MakeExplosion(origin, dir,
                              mod, shader,
                              duration, isSprite);
        le->light = light;
        VectorCopy(lightColor, le->lightColor);
        if (weapon == WP_RAILGUN) {
            VectorCopy(cgs.clientinfo[clientNum].color1, le->color);
            le->refEntity.shaderRGBA[0] = le->color[0] * 0xff;
            le->refEntity.shaderRGBA[1] = le->color[1] * 0xff;
            le->refEntity.shaderRGBA[2] = le->color[2] * 0xff;
            le->refEntity.shaderRGBA[3] = 0xff;
        }
    }

    alphaFade = (mark == cgs.media.energyMarkShader);
    if (weapon == WP_RAILGUN) {
        vec3_t teamColor;
        float* color;

        if (CG_GetRailColorFloat(&cgs.clientinfo[clientNum], teamColor, NULL)) {
            color = teamColor;
        } else {
            color = cgs.clientinfo[clientNum].color1;
        }
        CG_ImpactMark(mark, origin, dir, random() * 360, color[0], color[1], color[2], 1, alphaFade, radius, qfalse);
    } else {
        CG_ImpactMark(mark, origin, dir, random() * 360, 1, 1, 1, 1, alphaFade, radius, qfalse);
    }
}

void CG_MissileHitPlayer(int weapon, vec3_t origin, vec3_t dir, int entityNum) {
    if (entityNum != cg.snap->ps.clientNum) {
        int i;
        for (i = 0; i < 2; i++) {
            CG_BloodSplatEffect(origin, entityNum);
        }
        if (cg_impactSparks.integer) {
            vec3_t vel = { 0, 0, cg_impactSparksVelocity.value };
            CG_SpawnParticleEffect(origin, vel, (float)cg_impactSparksSize.integer,
                1.0f, 1.0f, 1.0f, 1.0f,
                (float)cg_impactSparksLifetime.integer,
                cg.time, 1, cgs.media.sparkParticleShader);
        }
    }

    switch (weapon) {
        case WP_GRENADE_LAUNCHER:
        case WP_ROCKET_LAUNCHER:
        case WP_BFG:
        case WP_PROX_LAUNCHER:
            CG_MissileHitWall(weapon, 0, origin, dir, IMPACTSOUND_FLESH);
            break;
        default:
            break;
    }
}

void CG_MissileHitWall_DmgThrough(vec3_t origin, vec3_t dir, int weapon) {
    trace_t trace;
    vec3_t end;
    int i;

    VectorMA(origin, cgs.dmgThroughDepth, dir, end);
    CG_Trace(&trace, end, NULL, NULL, origin, -1, CONTENTS_SOLID);

    if (trace.fraction != 1.0f && trace.surfaceFlags == 0) {
        CG_ImpactMark(cgs.media.burnMarkShader, trace.endpos, trace.plane.normal,
                      random() * 360, 1, 1, 1, 1, qfalse, 64, qfalse);

        for (i = 0; i < 10; i++) {
            vec3_t vel;
            float speed;

            speed = 250.0f + 150.0f * crandom();
            vel[0] = speed * dir[0] + (50.0f - 50.0f * crandom());
            vel[1] = speed * dir[1] + (50.0f - 50.0f * crandom());
            vel[2] = speed * dir[2] + (50.0f - 50.0f * crandom());

            CG_SpawnParticleEffect(origin, vel, 24.0f,
                                   0.8f, 0.8f, 0.7f, 1.0f,
                                   400.0f - (speed / 500.0f) * 200.0f,
                                   cg.time, 0, cgs.media.debrisPuffShader);
        }
    }

    CG_MissileHitWall(weapon, 0, origin, dir, IMPACTSOUND_DEFAULT);
}

static void CG_ShotgunPellet(vec3_t start, vec3_t end, int skipNum) {
    trace_t tr;
    int sourceContentType, destContentType;

    if (cgs.playerCylinders) {
        CG_CapsuleTrace(&tr, start, NULL, NULL, end, skipNum, MASK_SHOT);
    } else {
        CG_Trace(&tr, start, NULL, NULL, end, skipNum, MASK_SHOT);
    }

    sourceContentType = CG_PointContents(start, 0);
    destContentType = CG_PointContents(tr.endpos, 0);

    if (sourceContentType == destContentType) {
        if (sourceContentType & CONTENTS_WATER) {
            CG_BubbleTrail(start, tr.endpos, 32);
        }
    } else if (sourceContentType & CONTENTS_WATER) {
        trace_t trace;

        trap_CM_BoxTrace(&trace, end, start, NULL, NULL, 0, CONTENTS_WATER);
        CG_BubbleTrail(start, trace.endpos, 32);
    } else if (destContentType & CONTENTS_WATER) {
        trace_t trace;

        trap_CM_BoxTrace(&trace, start, end, NULL, NULL, 0, CONTENTS_WATER);
        CG_BubbleTrail(tr.endpos, trace.endpos, 32);
    }

    if (tr.surfaceFlags & SURF_NOIMPACT) {
        return;
    }

    if (cg_entities[tr.entityNum].currentState.eType == ET_PLAYER) {
        CG_MissileHitPlayer(WP_SHOTGUN, tr.endpos, tr.plane.normal, tr.entityNum);
    } else {
        if (tr.surfaceFlags & SURF_NOIMPACT) {
            return;
        }
        if (tr.surfaceFlags & SURF_METALSTEPS) {
            CG_MissileHitWall(WP_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_METAL);
        } else {
            CG_MissileHitWall(WP_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_DEFAULT);
        }
    }
}

static void CG_ShotgunPattern(vec3_t origin, vec3_t origin2, int seed, int otherEntNum) {
    int i;
    float r, u, angle, ringRadius, jitter;
    vec3_t end;
    vec3_t forward, right, up;

    VectorSubtract(origin2, origin, forward);
    VectorNormalize(forward);
    PerpendicularVector(right, forward);
    CrossProduct(forward, right, up);

    for (i = 0; i < DEFAULT_SHOTGUN_COUNT; i++) {
        if (i < 6) {
            jitter = 0.4f;
            ringRadius = 4000;
            angle = (float)(i - 20) * 1.0471976f;
        } else if (i < 12) {
            jitter = 0.3f;
            ringRadius = 8000;
            angle = (float)i * 1.0471976f + 30.0f;
        } else {
            jitter = 0.2f;
            ringRadius = 12000;
            angle = (float)i * 0.7853982f;
        }

        if (cg_trueShotgun.integer) {
            jitter = 0.0f;
        }

        seed = (seed * 0xDCD + 1) & 0xFFFF;
        r = (cos(angle) + ((float)seed * 1.5258789e-05f - 0.5f) * 2.0f * jitter) * ringRadius;

        seed = (seed * 0xDCD + 1) & 0xFFFF;
        u = (sin(angle) + ((float)seed * 1.5258789e-05f - 0.5f) * 2.0f * jitter) * ringRadius;

        VectorMA(origin, 8192 * 16, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        CG_ShotgunPellet(origin, end, otherEntNum);
    }
}

void CG_ShotgunFire(entityState_t* es) {
    vec3_t v;
    int contents;

    CG_ShotgunPattern(es->pos.trBase, es->origin2, es->eventParm, es->otherEntityNum);
    if (cg_smoke_SG.integer) {
        contents = CG_PointContents(es->pos.trBase, 0);
        if (!(contents & CONTENTS_WATER) && cgs.glconfig.hardwareType != GLHW_RAGEPRO) {
            vec3_t up;
            VectorSubtract(es->origin2, es->pos.trBase, v);
            VectorNormalize(v);
            VectorSet(up, 0, 0, 8);
            CG_SmokePuff(v, up, 32, 1, 1, 1, 0.33f, 900, cg.time, 0, LEF_PUFF_DONT_SCALE, cgs.media.shotgunSmokePuffShader);
        }
    }
}

void CG_ShotgunKillEffect(centity_t* cent) {
    int i, count;
    vec3_t origin;

    count = cent->currentState.generic1 / 5;

    for (i = 0; i < count; i++) {
        int j;

        VectorCopy(cent->currentState.pos.trBase, origin);
        origin[0] += (rand() & 31) - 16;
        origin[1] += (rand() & 31) - 16;
        origin[2] += rand() % 24 + 8;

        for (j = 0; j < 2; j++) {
            CG_BloodSplatEffect(origin, cent->currentState.otherEntityNum2);
        }

        if (cg_impactSparks.integer) {
            vec3_t vel = { 0, 0, cg_impactSparksVelocity.value };
            CG_SpawnParticleEffect(origin, vel, (float)cg_impactSparksSize.integer,
                                   1.0f, 1.0f, 1.0f, 1.0f,
                                   (float)cg_impactSparksLifetime.integer,
                                   cg.time, 1, cgs.media.sparkParticleShader);
        }
    }
}

void CG_Tracer(vec3_t source, vec3_t dest) {
    vec3_t forward, right;
    polyVert_t verts[4];
    vec3_t line;
    float len, begin, end;
    vec3_t start, finish;
    vec3_t midpoint;

    VectorSubtract(dest, source, forward);
    len = VectorNormalize(forward);

    if (len < 100) {
        return;
    }
    begin = 50 + random() * (len - 60);
    end = begin + cg_tracerLength.value;
    if (end > len) {
        end = len;
    }
    VectorMA(source, begin, forward, start);
    VectorMA(source, end, forward, finish);

    line[0] = DotProduct(forward, cg.refdef.viewaxis[1]);
    line[1] = DotProduct(forward, cg.refdef.viewaxis[2]);

    VectorScale(cg.refdef.viewaxis[1], line[1], right);
    VectorMA(right, -line[0], cg.refdef.viewaxis[2], right);
    VectorNormalize(right);

    VectorMA(finish, cg_tracerWidth.value, right, verts[0].xyz);
    verts[0].st[0] = 0;
    verts[0].st[1] = 1;
    verts[0].modulate[0] = 255;
    verts[0].modulate[1] = 255;
    verts[0].modulate[2] = 255;
    verts[0].modulate[3] = 255;

    VectorMA(finish, -cg_tracerWidth.value, right, verts[1].xyz);
    verts[1].st[0] = 1;
    verts[1].st[1] = 0;
    verts[1].modulate[0] = 255;
    verts[1].modulate[1] = 255;
    verts[1].modulate[2] = 255;
    verts[1].modulate[3] = 255;

    VectorMA(start, -cg_tracerWidth.value, right, verts[2].xyz);
    verts[2].st[0] = 1;
    verts[2].st[1] = 1;
    verts[2].modulate[0] = 255;
    verts[2].modulate[1] = 255;
    verts[2].modulate[2] = 255;
    verts[2].modulate[3] = 255;

    VectorMA(start, cg_tracerWidth.value, right, verts[3].xyz);
    verts[3].st[0] = 0;
    verts[3].st[1] = 0;
    verts[3].modulate[0] = 255;
    verts[3].modulate[1] = 255;
    verts[3].modulate[2] = 255;
    verts[3].modulate[3] = 255;

    trap_R_AddPolyToScene(cgs.media.tracerShader, 4, verts);

    midpoint[0] = (start[0] + finish[0]) * 0.5;
    midpoint[1] = (start[1] + finish[1]) * 0.5;
    midpoint[2] = (start[2] + finish[2]) * 0.5;

    trap_S_StartSound(midpoint, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.tracerSound);
}

static qboolean CG_CalcMuzzlePoint(int entityNum, vec3_t muzzle) {
    vec3_t forward;
    centity_t* cent;
    int anim;

    if (entityNum == cg.snap->ps.clientNum) {
        VectorCopy(cg.snap->ps.origin, muzzle);
        muzzle[2] += cg.snap->ps.viewheight;
        AngleVectors(cg.snap->ps.viewangles, forward, NULL, NULL);
    VectorMA(muzzle, (cg.snap->ps.pm_flags & PMF_DUCKED) ? 3 : 5, forward, muzzle);
        return qtrue;
    }

    cent = &cg_entities[entityNum];
    if (!cent->currentValid) {
        return qfalse;
    }

    VectorCopy(cent->currentState.pos.trBase, muzzle);

    AngleVectors(cent->currentState.apos.trBase, forward, NULL, NULL);
    anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
    if (anim == LEGS_WALKCR || anim == LEGS_IDLECR) {
        muzzle[2] += CROUCH_VIEWHEIGHT;
    } else {
        muzzle[2] += DEFAULT_VIEWHEIGHT;
    }

    VectorMA(muzzle, (cg.snap->ps.pm_flags & PMF_DUCKED) ? 3 : 5, forward, muzzle);

    return qtrue;
}

void CG_Bullet(vec3_t end, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum) {
    trace_t trace;
    int sourceContentType, destContentType;
    vec3_t start;

    if (sourceEntityNum >= 0 && cg_tracerChance.value > 0) {
        if (CG_CalcMuzzlePoint(sourceEntityNum, start)) {
            sourceContentType = CG_PointContents(start, 0);
            destContentType = CG_PointContents(end, 0);

            if ((sourceContentType == destContentType) && (sourceContentType & CONTENTS_WATER)) {
                CG_BubbleTrail(start, end, 32);
            }
            else if ((sourceContentType & CONTENTS_WATER)) {
                trap_CM_BoxTrace(&trace, end, start, NULL, NULL, 0, CONTENTS_WATER);
                CG_BubbleTrail(start, trace.endpos, 32);
            }
            else if ((destContentType & CONTENTS_WATER)) {
                trap_CM_BoxTrace(&trace, start, end, NULL, NULL, 0, CONTENTS_WATER);
                CG_BubbleTrail(trace.endpos, end, 32);
            }

            if (random() < cg_tracerChance.value) {
                CG_Tracer(start, end);
            }
        }
    }

    if (fleshEntityNum != ENTITYNUM_WORLD) {
        if (fleshEntityNum != cg.snap->ps.clientNum) {
            int i;
            for (i = 0; i < 2; i++) {
                CG_BloodSplatEffect(end, fleshEntityNum);
            }
            if (cg_impactSparks.integer) {
                vec3_t vel = { 0, 0, cg_impactSparksVelocity.value };
                CG_SpawnParticleEffect(end, vel, (float)cg_impactSparksSize.integer,
                    1.0f, 1.0f, 1.0f, 1.0f,
                    (float)cg_impactSparksLifetime.integer,
                    cg.time, 1, cgs.media.sparkParticleShader);
            }
        }
    } else {
        CG_MissileHitWall(WP_MACHINEGUN, 0, end, normal, IMPACTSOUND_DEFAULT);
    }
}
