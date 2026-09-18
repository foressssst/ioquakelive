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
// cg_weapons.c -- events and effects dealing with weapons
#include "cg_local.h"
#include "../../ui/menudef.h"

/*
==========================
CG_MachineGunEjectBrass
==========================
*/
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

/*
==========================
CG_ShotgunEjectBrass
==========================
*/
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

/*
==========================
CG_NailgunEjectBrass
==========================
*/
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
    // use the optimized local entity add
    smoke->leType = LE_SCALE_FADE;
}

/*
==========================
CG_WeaponColorForced

[QL] The choice shared by the forced weapon colours (0x1004f280, 0x1004f420): nothing is forced
unless cg_forceTeamWeaponColor or cg_forceEnemyWeaponColor is exactly 1, and never for the
viewer's own rail. In FFA everybody else is an enemy. Returns the packed upper colour of the
owner's side, or qfalse when that side is not forced.
==========================
*/
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

/*
==========================
CG_GetRailColorFloat

[QL] 0x1004f280: the forced rail colour as three quarters of the side's upper colour. The ring
colour is the team's upper colour even for enemies (0x1004f3e7).
==========================
*/
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

/*
==========================
CG_GetRailColorByte

[QL] 0x1004f420: the same colour for the ring sprites, three quarters of each byte truncated.
==========================
*/
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

/*
==========================
CG_RailTrailCore
Address: 0x1004f670
[QL] The rings-only disc beam drawn for any cg_railStyle other than 2 (the default
is 1). One local entity, reType RT_RAIL_RINGS with railRingsShader; despite the
"core" symbol name the binary sets reType = 5 (RT_RAIL_RINGS), not RT_RAIL_CORE.
Unforced, it takes the owner's second colour (0x1004f725).
==========================
*/
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
    re->reType = RT_RAIL_RINGS;  // [QL] binary offset 0xa0 = 5 (RT_RAIL_RINGS)
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

/*
==========================
CG_RailTrail
==========================
*/
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
    re->radius = 256;  // [QL] wide core beam (binary sets refEntity offset 0x124 = 256.0)

    VectorCopy(start, re->origin);
    VectorCopy(end, re->oldorigin);

    // [QL] the core beam LE is LE_FADE_RGB driven by le->color; the binary does
    // NOT write re->shaderRGBA for the core entity.

    // [QL] team rail-colour override for the core beam
    if (!CG_GetRailColorFloat(ci, le->color, NULL)) {
        le->color[0] = ci->color1[0] * 0.75;
        le->color[1] = ci->color1[1] * 0.75;
        le->color[2] = ci->color1[2] * 0.75;
    }
    le->color[3] = 1.0f;

    AxisClear(re->axis);

    // [QL] view-relative origin adjustment (binary CG_RailTrail @0x1004f7b0).
    // Nudges the beam start (and therefore the spiral start) toward the drawn muzzle.
    {
        int viewer = cg.predictedPlayerState.clientNum;
        int owner = ci - cgs.clientinfo;

        if (viewer != owner) {
            // another player's rail (seen in third person)
            VectorMA(re->origin, 5.0f, cg.refdef.viewaxis[1], re->origin);
            VectorMA(re->origin, -4.0f, cg.refdef.viewaxis[2], re->origin);
        } else if (!cg.renderingThirdPerson) {
            // own rail, first person: reconstruct the view-model muzzle offset
            // (mirrors CG_AddViewWeapon's gun positioning; +4/-4/*0.75 are QL tweaks)
            float fovOffset = (cg_fov.integer > 90) ? -0.2f * (cg_fov.integer - 90) : 0.0f;
            VectorMA(re->origin, cg_gun_x.value + 4.0f, cg.refdef.viewaxis[0], re->origin);
            VectorMA(re->origin, cg_gun_y.value * 0.75f, cg.refdef.viewaxis[1], re->origin);
            VectorMA(re->origin, fovOffset + cg_gun_z.value - 4.0f, cg.refdef.viewaxis[2], re->origin);
        } else {
            // own rail, third person: the muzzle height depends on the player model (0x1004facf)
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

    // [QL] 0x1004fcb7: cg_railStyle 2 draws the spiral ring trail; any other value draws the
    // rings-only disc beam from the adjusted origin (CG_RailTrailCore, called at 0x1005015d) and
    // stops.
    if (cg_railStyle.integer != 2) {
        CG_RailTrailCore(ci, re->origin, re->oldorigin);
        return;
    }

    // [QL] the spiral starts 4 units below the beam (0x1004fcd4)
    VectorCopy(re->origin, move);
    move[2] -= 4;
    VectorSubtract(end, move, vec);
    len = VectorNormalize(vec);
    PerpendicularVector(temp, vec);
    for (i = 0; i < 36; i++) {
        RotatePointAroundVector(axis[i], vec, temp, i * 10);  // banshee 2.4 was 10
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

            // [QL] team rail-colour override for the ring sprites
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

/*
==========================
CG_SpawnRailTrail
Address: 0x10052200

Origin will be the exact tag point, which is slightly
different than the muzzle point used for determining hits.

[QL] Kept from Q3 unchanged: the trail runs from the flash tag to pe.railgunImpact while
pe.railgunFlash is set, and the only write to that flag is the qtrue below (0x10052230), so
it never draws. Rail trails come from EV_RAILTRAIL and the predicted local shot.
==========================
*/
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

// [QL] forward decl: CG_RocketTrail compares wi->missileTrailFunc against
// CG_GrenadeTrail (defined below) to choose which trail-radius cvar to use.
static void CG_GrenadeTrail(centity_t* ent, const weaponInfo_t* wi);

/*
==========================
CG_RocketTrail
==========================
*/
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

    // if object (e.g. grenade) is stationary, don't toss up smoke
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

    // [QL] cgamex86.dll 0x10050170: the radius comes from the smoke cvars, and 0 leaves no trail.
    // The binary replaced Q3's cg_noProjectileTrail early-return with this.
    if (wi->missileTrailFunc == CG_RocketTrail) {
        trailRadius = cg_smokeRadius_RL.value;
    }
    if (wi->missileTrailFunc == CG_GrenadeTrail) {
        trailRadius = cg_smokeRadius_GL.value;
    }
    if (trailRadius == 0.0f) {
        return;
    }

    // [ioquakelive] the smoke travels with the projectile (CG_ProjectileLeadOffset)
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
        // use the optimized local entity add
        smoke->leType = LE_SCALE_FADE;
    }
}

/*
==========================
CG_NailTrail
==========================
*/
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

    // [QL] the nail's radius is cg_smokeRadius_NG; 0 leaves no trail
    // (binary replaced the cg_noProjectileTrail early-return with this).
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

    // if object (e.g. grenade) is stationary, don't toss up smoke
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
        // use the optimized local entity add
        smoke->leType = LE_SCALE_FADE;
    }
}

/*
==========================
CG_PlasmaTrail
==========================
*/
static void CG_PlasmaTrail(centity_t* cent, const weaponInfo_t* wi) {
    localEntity_t* le;
    refEntity_t* re;
    entityState_t* es;
    vec3_t velocity, xvelocity, origin;
    vec3_t offset, xoffset;
    vec3_t v[3];

    float waterScale = 1.0f;

    // QL binary: cg_plasmaStyle.integer == 2 enables plasma trail (vmCvar 0x10ABB0A0);
    // the binary gates ONLY on cg_plasmaStyle (no cg_noProjectileTrail check).
    if (cg_plasmaStyle.integer != 2) {
        return;
    }

    es = &cent->currentState;

    BG_EvaluateTrajectory(&es->pos, cg.time, origin);
    // [ioquakelive] the plasma's trail travels with it (CG_ProjectileLeadOffset)
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
/*
==========================
CG_GrappleTrail
==========================
*/
void CG_GrappleTrail(centity_t* ent, const weaponInfo_t* wi) {
    vec3_t origin;
    entityState_t* es;
    refEntity_t beam;

    es = &ent->currentState;

    // [QL] only draw the chain for the grapple weapon (binary CG_GrappleTrail @0x10050990)
    if (es->weapon != WP_GRAPPLING_HOOK) {
        return;
    }

    BG_EvaluateTrajectory(&es->pos, cg.time, origin);

    memset(&beam, 0, sizeof(beam));

    // [QL] origin is the owner's lerp origin directly (no muzzle +26/-6 offset),
    // endpoint is the hook position. Now called from CG_AddPlayerWeapon via
    // tag_chain (not as a missileTrailFunc), so it doesn't touch ent->trailTime and
    // has no Distance<16 early-out.
    VectorCopy(cg_entities[es->otherEntityNum].lerpOrigin, beam.origin);
    VectorCopy(origin, beam.oldorigin);

    beam.radius = 64;  // [QL]

    // [QL] use RT_RAIL_CORE for single tiled quad (RT_LIGHTNING draws 4 overlapping rotated quads)
    beam.reType = RT_RAIL_CORE;
    beam.customShader = cgs.media.grapplingChainShader;

    AxisClear(beam.axis);
    beam.shaderRGBA[0] = 0xff;
    beam.shaderRGBA[1] = 0xff;
    beam.shaderRGBA[2] = 0xff;
    beam.shaderRGBA[3] = 0xff;
    trap_R_AddRefEntityToScene(&beam);
}

/*
==========================
CG_GrenadeTrail
==========================
*/
static void CG_GrenadeTrail(centity_t* ent, const weaponInfo_t* wi) {
    CG_RocketTrail(ent, wi);
}

/*
=================
CG_RegisterWeapon

The server says this item is used on this level
=================
*/
void CG_RegisterWeapon(int weaponNum) {
    weaponInfo_t* weaponInfo;
    gitem_t *item, *ammo;
    char path[MAX_QPATH];
    vec3_t mins, maxs;
    int i;

    weaponInfo = &cg_weapons[weaponNum];

    // [QL] weapon 0 and 15 (0xf) are not registered; grapple (10) IS registered.
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

    // load cmodel before model so filecache works
    weaponInfo->weaponModel = trap_R_RegisterModel(item->world_model[0]);

    // calc midpoint for rotation
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

    // [QL] the binary tries a barrel model for every weapon, but only these ship a
    // *_barrel.md3. Loading the rest spams "RegisterMD3: couldn't load" with no
    // barrel, so only load the ones that have one.
    if (weaponNum == WP_GAUNTLET || weaponNum == WP_MACHINEGUN || weaponNum == WP_BFG || weaponNum == WP_CHAINGUN) {
        COM_StripExtension(item->world_model[0], path, sizeof(path));
        Q_strcat(path, sizeof(path), "_barrel.md3");
        weaponInfo->barrelModel = trap_R_RegisterModel(path);
    }

    COM_StripExtension(item->world_model[0], path, sizeof(path));
    Q_strcat(path, sizeof(path), "_hand.md3");
    weaponInfo->handsModel = trap_R_RegisterModel(path);

    // [QL] the grapple carries its hook/ammo on tag_ammo (drawn in CG_AddPlayerWeapon)
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
            // [QL] binary: missileSound=grfire, NO missileTrailFunc (the chain is
            // drawn via tag_chain in CG_AddPlayerWeapon), NO missileDlight,
            // readySound=grhang, and grreset is precached.
            MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
            weaponInfo->missileModel = trap_R_RegisterModel("models/weapons2/grapple/grapple_hook.md3");
            cgs.media.grapplingChainShader = trap_R_RegisterShader("grapplingChain");
            weaponInfo->readySound = trap_S_RegisterSound("sound/weapons/grapple/grhang.ogg", qfalse);
            weaponInfo->firingSound = trap_S_RegisterSound("sound/weapons/grapple/grfire.ogg", qfalse);
            weaponInfo->missileTrailFunc = NULL;
            weaponInfo->missileSound = trap_S_RegisterSound("sound/weapons/grapple/grfire.ogg", qfalse);
            trap_S_RegisterSound("sound/weapons/grapple/grreset.ogg", qfalse);
            break;

        case WP_CHAINGUN:
            weaponInfo->firingSound = trap_S_RegisterSound("sound/weapons/vulcan/wvulfire.ogg", qfalse);
            MAKERGB(weaponInfo->flashDlightColor, 1, 1, 0);
            // [QL] 0x100510fd: its whine does not replace the shots
            weaponInfo->loopFireSound = qtrue;
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
            cgs.media.bulletExplosionShader = trap_R_RegisterShader("bulletExplosion");  // [QL]
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
            //		weaponInfo->missileSound = trap_S_RegisterSound( "sound/weapons/nailgun/wnalflit.ogg", qfalse );
            weaponInfo->trailRadius = 16;
            weaponInfo->wiTrailTime = 250;
            weaponInfo->missileModel = trap_R_RegisterModel("models/weaphits/nail.md3");
            MAKERGB(weaponInfo->flashDlightColor, 1, 0.75f, 0);
            weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/nailgun/wnalfire.ogg", qfalse);
            break;

        case WP_PLASMAGUN:
            //		weaponInfo->missileModel = cgs.media.invulnerabilityPowerupModel;
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

/*
=================
CG_RegisterItemVisuals

The server says this item is used on this level
=================
*/
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

    //
    // powerups have an accompanying ring or sphere
    //
    if (item->giType == IT_POWERUP || item->giType == IT_HEALTH ||
        item->giType == IT_ARMOR || item->giType == IT_HOLDABLE) {
        if (item->world_model[1]) {
            itemInfo->models[1] = trap_R_RegisterModel(item->world_model[1]);
        }
    }
}

/*
========================================================================================

VIEW WEAPON

========================================================================================
*/

/*
=================
CG_MapTorsoToWeaponFrame

=================
*/
static int CG_MapTorsoToWeaponFrame(clientInfo_t* ci, int frame) {
    // change weapon
    if (frame >= ci->animations[TORSO_DROP].firstFrame && frame < ci->animations[TORSO_DROP].firstFrame + 9) {
        return frame - ci->animations[TORSO_DROP].firstFrame + 6;
    }

    // stand attack
    if (frame >= ci->animations[TORSO_ATTACK].firstFrame && frame < ci->animations[TORSO_ATTACK].firstFrame + 6) {
        return 1 + frame - ci->animations[TORSO_ATTACK].firstFrame;
    }

    // stand attack 2
    if (frame >= ci->animations[TORSO_ATTACK2].firstFrame && frame < ci->animations[TORSO_ATTACK2].firstFrame + 6) {
        return 1 + frame - ci->animations[TORSO_ATTACK2].firstFrame;
    }

    return 0;
}

/*
==============
CG_CalculateWeaponPosition
==============
*/
static void CG_CalculateWeaponPosition(vec3_t origin, vec3_t angles) {
    float scale;
    int delta;
    float fracsin;

    VectorCopy(cg.refdef.vieworg, origin);
    VectorCopy(cg.refdefViewAngles, angles);

    // [QL] only the standard gun (cg_drawGun == 1) gets bob/land/idle motion;
    // left-handed/centered modes use the raw view origin and angles.
    if (cg_drawGun.integer != 1) {
        return;
    }

    // on odd legs, invert some angles
    if (cg.bobcycle & 1) {
        scale = -cg.xyspeed;
    } else {
        scale = cg.xyspeed;
    }

    // gun angles from bobbing
    angles[ROLL] += scale * cg.bobfracsin * 0.005;
    angles[YAW] += scale * cg.bobfracsin * 0.01;
    angles[PITCH] += cg.xyspeed * cg.bobfracsin * 0.005;

    // drop the weapon when landing
    delta = cg.time - cg.landTime;
    if (delta < LAND_DEFLECT_TIME) {
        origin[2] += cg.landChange * 0.25 * delta / LAND_DEFLECT_TIME;
    } else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME) {
        origin[2] += cg.landChange * 0.25 *
                     (LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
    }

#if 0
	// drop the weapon when stair climbing
	delta = cg.time - cg.stepTime;
	if (delta < STEP_TIME / 2) {
		origin[2] -= cg.stepChange * 0.25 * delta / (STEP_TIME / 2);
	} else if (delta < STEP_TIME) {
		origin[2] -= cg.stepChange * 0.25 * (STEP_TIME - delta) / (STEP_TIME / 2);
	}
#endif

    // idle drift
    scale = cg.xyspeed + 40;
    fracsin = sin(cg.time * 0.001);
    angles[ROLL] += scale * fracsin * 0.01;
    angles[YAW] += scale * fracsin * 0.01;
    angles[PITCH] += scale * fracsin * 0.01;

    // [QL] extra sway while the chaingun is firing
    if (cg.snap->ps.weapon == WP_CHAINGUN && cg.snap->ps.weaponstate == WEAPON_FIRING) {
        float fireScale = 1.0 - (1000.0 - (float)cg.snap->ps.weaponTime) / 1000.0;
        float fireSway = fireScale * (float)sin(cg.time * 0.001) * 0.01;
        angles[ROLL] += fireSway * 100.0;
        angles[YAW] += fireSway * 300.0;
        angles[PITCH] += fireSway * 100.0;
    }
}

/*
===============
CG_LightningBolt

Origin will be the exact tag point, which is slightly
different than the muzzle point used for determining hits.
The cent should be the non-predicted cent if it is from the player,
so the endpoint will reflect the simulated strike (lagging the predicted
angle)
===============
*/
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

    // CPMA "true" lightning
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
        // [QL] true-lightning starts from the actual view origin (not lerpOrigin + viewheight)
        VectorCopy(cg.refdef.vieworg, muzzlePoint);
    } else {
        // !CPMA
        anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
        AngleVectors(cent->lerpAngles, forward, NULL, NULL);
        VectorCopy(cent->lerpOrigin, muzzlePoint);

        if (anim == LEGS_WALKCR || anim == LEGS_IDLECR) {
            muzzlePoint[2] += CROUCH_VIEWHEIGHT;
        } else {
            muzzlePoint[2] += DEFAULT_VIEWHEIGHT;
        }
    }

    // [QL] muzzle forward offset is 5 (Q3 used 14)
    VectorMA(muzzlePoint, 5, forward, muzzlePoint);

    // project forward by the lightning range
    VectorMA(muzzlePoint, LIGHTNING_RANGE, forward, endPoint);

    // see if it hit a wall (capsule player hulls when g_playerCylinders is set)
    if (cgs.playerCylinders) {
        CG_CapsuleTrace(&trace, muzzlePoint, vec3_origin, vec3_origin, endPoint,
                        cent->currentState.number, MASK_SHOT);
    } else {
        CG_Trace(&trace, muzzlePoint, vec3_origin, vec3_origin, endPoint,
                 cent->currentState.number, MASK_SHOT);
    }

    // NOTE [QL]: the binary skips the bolt only when a render flag AND a submerged
    // muzzle are BOTH true (draw if !(flag & 0x8000000) || !(inWater)). The earlier
    // "EF_NODRAW || inWater" skip used the wrong flag and inverted logic, suppressing
    // the beam, so it's removed pending identification of that flag. Beam still
    // draws (as before), correct in all non-underwater cases.

    // this is the endpoint
    VectorCopy(trace.endpos, beam.oldorigin);

    // use the provided origin, even though it may be slightly
    // different than the muzzle origin
    VectorCopy(origin, beam.origin);

    beam.reType = RT_LIGHTNING;
    beam.radius = 256;  // [QL]

    if (cg_lightningStyle.integer < 1 || cg_lightningStyle.integer > NUM_LIGHTNING_STYLES) {
        trap_Cvar_Set("cg_lightningStyle", "1");  // don't allow invalid values, default to 1
        cg_lightningStyle.integer = 1;            // just in case
    }

    beam.customShader = cgs.media.lightningShader[cg_lightningStyle.integer - 1];

    trap_R_AddRefEntityToScene(&beam);

    // [QL] add the impact flare if it hit something and cg_lightningImpact is set
    if (cg_lightningImpact.integer && trace.fraction < 1.0) {
        vec3_t angles;
        vec3_t dir;
        float dist;

        VectorSubtract(beam.oldorigin, beam.origin, dir);
        dist = VectorNormalize(dir);

        memset(&beam, 0, sizeof(beam));
        beam.hModel = cgs.media.lightningExplosionModel;

        VectorMA(trace.endpos, -16, dir, beam.origin);

        // make a random orientation
        angles[0] = rand() % 360;
        angles[1] = rand() % 360;
        angles[2] = rand() % 360;
        AnglesToAxis(angles, beam.axis);

        // [QL] shrink the crackle model when the impact is close (cg_lightningImpactCap)
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

/*
======================
CG_MachinegunSpinAngle
======================
*/
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
            trap_S_StartSound(NULL, cent->currentState.number, CHAN_WEAPON, trap_S_RegisterSound("sound/weapons/vulcan/wvulwind.ogg", qfalse));
        }
    }

    return angle;
}

/*
========================
CG_AddWeaponWithPowerups
========================
*/
/*
==============
CG_AddWeaponWithPowerups

[QL] cgamex86.dll 0x10052350. El arma se dibuja una sola vez con un shader aparte, o normal
con las capas de quad y battlesuit encima. Las tres razones para el shader aparte, en el
orden en que el binario las mira:

  invisibilidad   el portador la lleva puesta
  cg_drawGun 3    y es el arma del jugador que se esta viendo: ghostWeaponShader, aditivo y
                  con mapa de entorno (pak00, scripts/gfx.shader), que es como retail hace el
                  arma transparente

"own" es lo que el binario resuelve comparando el numero de cliente (0x10052357), asi que
tambien vale en tercera persona, no solo en la vista.
==============
*/
static void CG_AddWeaponWithPowerups(refEntity_t* gun, int powerups, qboolean own) {
    if (powerups & (1 << PW_INVIS)) {
        gun->customShader = cgs.media.invisShader;
        trap_R_AddRefEntityToScene(gun);
        return;
    }
    if (own && cg_drawGun.integer == 3) {
        // [QL] 0x100523a8. WolfcamQL hace exactamente esto mismo (cg_weapons.c, cg_drawGun > 2):
        // el shader de retail y el color en blanco opaco, que es lo que el jugador reconoce como
        // "el arma transparente". El shader es aditivo con mapa de entorno, asi que el arma
        // queda como un cristal: se ve la forma y se ve a traves.
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

/*
=============
CG_BrightWeapon

[CPMA] cg_drawBrightWeapons draws a weapon as a flat silhouette of one colour, as CPMA 1.53 does by
giving it the fullBright shader and a colour per weapon. The bits choose which weapons:
   1  your own gun in first person
   2  a teammate's (on red or blue, seen in first person)
   4  anybody else's
   8  the weapons lying on the map
  16  anybody's while you spectate with the free camera
The colours are those of Quake Live's weapon icons (icons/iconw_*, weap_hmg, nailgun128, proxmine and
chaingun128 in pak00), which are flat glyphs of a single colour each.
=============
*/
static const byte cg_brightWeaponColors[WP_NUM_WEAPONS][3] = {
    { 255, 255, 255 },  // WP_NONE
    { 0, 215, 255 },    // WP_GAUNTLET
    { 255, 255, 0 },    // WP_MACHINEGUN
    { 255, 125, 0 },    // WP_SHOTGUN
    { 1, 143, 17 },     // WP_GRENADE_LAUNCHER
    { 255, 0, 0 },      // WP_ROCKET_LAUNCHER
    { 255, 255, 176 },  // WP_LIGHTNING
    { 2, 232, 1 },      // WP_RAILGUN
    { 196, 0, 255 },    // WP_PLASMAGUN
    { 0, 90, 255 },     // WP_BFG
    { 81, 170, 220 },   // WP_GRAPPLING_HOOK
    { 3, 252, 178 },    // WP_NAILGUN
    { 255, 0, 124 },    // WP_PROX_LAUNCHER
    { 182, 182, 182 },  // WP_CHAINGUN
    { 206, 165, 0 },    // WP_HMG
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

/*
=============
CG_AddPlayerWeapon

Used for both the view weapon (ps is valid) and the world modelother character models (ps is NULL)
The main player will have this called for BOTH cases, so effects like light and
sound should only be done on the world model case.
=============
*/
void CG_AddPlayerWeapon(refEntity_t* parent, playerState_t* ps, centity_t* cent, int team) {
    refEntity_t gun;
    refEntity_t barrel;
    refEntity_t flash;
    vec3_t angles;
    weapon_t weaponNum;
    weaponInfo_t* weapon;
    centity_t* nonPredictedCent;
    orientation_t lerped;
    // [QL] 0x10052357: el binario resuelve "es el arma del que estoy viendo" comparando el
    // numero de cliente, no si viene playerState, asi que tambien vale en tercera persona
    qboolean own = (cent->currentState.clientNum == cg.snap->ps.clientNum);

    weaponNum = cent->currentState.weapon;

    CG_RegisterWeapon(weaponNum);
    weapon = &cg_weapons[weaponNum];

    // add the weapon
    memset(&gun, 0, sizeof(gun));
    VectorCopy(parent->lightingOrigin, gun.lightingOrigin);
    gun.shadowPlane = parent->shadowPlane;
    gun.renderfx = parent->renderfx;

    // [QL] railgun view-model tint tracks the refire cooldown via ps->weaponstate
    // and cg_railReloadTime (binary), not the Q3 railFireTime+1500 window. World
    // model gets a fixed green tint; local view model fades from black up to the
    // player's colour as the reload completes.
    if (weaponNum == WP_RAILGUN) {
        clientInfo_t* ci = &cgs.clientinfo[cent->currentState.clientNum];
        if (!ps) {
            gun.shaderRGBA[0] = 0;
            gun.shaderRGBA[1] = 255;
            gun.shaderRGBA[2] = 0;
        } else if (ps->weaponstate == WEAPON_FIRING) {
            float fscale = (1.0f - (float)ps->weaponTime / (float)cg_railReloadTime.integer) * 255.0f;
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

    // [ioquakelive] el flag se limpia siempre, tambien en primera persona. El bucle de sonido es
    // solo para los demas -uno no se escucha a si mismo desde afuera-, pero el flag lo lee
    // CG_FireWeapon para callar el disparo de un arma que ya suena sola, y si no se limpia ac� se
    // queda pegado desde la ultima vez que se vio a otro disparando. Ese era el motivo de que el
    // pu�etazo del gauntlet no sonara.
    cent->pe.lightningFiring = qfalse;
    if ((cent->currentState.eFlags & EF_FIRING) && weapon->firingSound) {
        // lightning gun and guantlet make a different sound when fire is held down
        if (!ps) {
            trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin,
                                   weapon->firingSound);
        }
        cent->pe.lightningFiring = qtrue;
    } else if (!ps && weapon->readySound) {
        trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->readySound);
    }

    // [ioquakelive] El arma va donde diga la etiqueta del modelo, y nada mas. Antes cg_drawGun
    // 2 la espejaba a la zurda y el 3 la centraba, que es de Quake 3: en el binario cg_drawGun
    // se lee en cuatro sitios y en ninguno mueve nada. La posicion es cosa de cg_gunX/Y/Z, que
    // CG_AddViewWeapon suma sobre los ejes de la vista (0x10052cf0), y esos se cambian desde
    // Ajustes.
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

    // add the spinning barrel
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

    // [QL] draw the grapple hook/ammo on tag_ammo while the hook is stowed (not firing)
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

    // make sure we aren't looking at cg.predictedPlayerEntity for LG
    nonPredictedCent = &cg_entities[cent->currentState.number];

    // if the index of the nonPredictedCent is not the same as the clientNum
    // then this is a fake player (like on the single player podiums), so
    // go ahead and use the cent
    if ((nonPredictedCent - cg_entities) != cent->currentState.clientNum) {
        nonPredictedCent = cent;
    }

    // add the flash
    if ((weaponNum == WP_LIGHTNING || weaponNum == WP_GAUNTLET || weaponNum == WP_GRAPPLING_HOOK) && (nonPredictedCent->currentState.eFlags & EF_FIRING)) {
        // continuous flash
    } else {
        // impulse flash
        if (cg.time - cent->muzzleFlashTime > MUZZLE_FLASH_TIME && !cent->pe.railgunFlash) {  // [QL] 0x10052949
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

    // colorize the railgun blast, with the forced ring colour when there is one (0x10052a90)
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
    // [QL] draw the flash sprite for world models, or for the view model when both
    // cg_muzzleFlash and cg_drawGun are on (binary: ps==NULL || (cg_muzzleFlash && cg_drawGun))
    if (!ps || (cg_muzzleFlash.integer && cg_drawGun.integer)) {
        trap_R_AddRefEntityToScene(&flash);
    }

    if (ps || cg.renderingThirdPerson ||
        cent->currentState.number != cg.predictedPlayerState.clientNum) {
        // [QL] draw the grapple chain (CG_GrappleTrail self-guards on WP_GRAPPLING_HOOK)
        // TODO [QL] the binary first repositions the flash onto its own tag_chain
        //   (guarded by clientinfo infoValid && legsModel) before this call. Omitted
        //   here, it would perturb the LG bolt origin for flash models without a
        //   tag_chain.
        CG_GrappleTrail(nonPredictedCent, weapon);

        // add lightning bolt
        CG_LightningBolt(nonPredictedCent, flash.origin);

        // add rail trail
        CG_SpawnRailTrail(nonPredictedCent, flash.origin);  // [QL] 0x10052c2a

        // QL binary: cg_muzzleFlash.integer also gates the dlight
        if ((!ps || cg_muzzleFlash.integer) &&
            (weapon->flashDlightColor[0] || weapon->flashDlightColor[1] || weapon->flashDlightColor[2])) {
            trap_R_AddLightToScene(flash.origin, 300 + (rand() & 31), weapon->flashDlightColor[0],
                                   weapon->flashDlightColor[1], weapon->flashDlightColor[2]);
        }
    }
}

/*
==============
CG_AddViewWeapon

Add the weapon, and flash for the player's view
==============
*/
void CG_AddViewWeapon(playerState_t* ps) {
    refEntity_t hand;
    centity_t* cent;
    clientInfo_t* ci;
    float fovOffset;
    vec3_t angles;
    weaponInfo_t* weapon;

    if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }

    if (ps->pm_type == PM_INTERMISSION) {
        return;
    }

    // [QL] 0x10052d28: nor while held for the training tour, or under its cinema bars
    if (ps->pm_type == PM_TUTORIAL) {
        return;
    }
    if (cg_letterboxDrawn) {
        return;
    }

    // no gun if in third person view or a camera is active
    // if ( cg.renderingThirdPerson || cg.cameraMode) {
    if (cg.renderingThirdPerson) {
        return;
    }

    // allow the gun to be completely removed
    if (!cg_drawGun.integer) {
        vec3_t origin;

        if (cg.predictedPlayerState.eFlags & EF_FIRING) {
            // special hack for lightning gun...
            VectorCopy(cg.refdef.vieworg, origin);
            VectorMA(origin, -8, cg.refdef.viewaxis[2], origin);
            CG_LightningBolt(&cg_entities[ps->clientNum], origin);
        }
        return;
    }

    // don't draw if testing a gun model
    if (cg.testGun) {
        return;
    }

    // drop gun lower at higher fov
    if (cg_fov.integer > 90) {
        fovOffset = -0.2 * (cg_fov.integer - 90);
    } else {
        fovOffset = 0;
    }

    cent = &cg.predictedPlayerEntity;  // &cg_entities[cg.snap->ps.clientNum];
    CG_RegisterWeapon(ps->weapon);
    weapon = &cg_weapons[ps->weapon];

    memset(&hand, 0, sizeof(hand));

    // set up gun position
    CG_CalculateWeaponPosition(hand.origin, angles);

    VectorMA(hand.origin, cg_gun_x.value, cg.refdef.viewaxis[0], hand.origin);
    VectorMA(hand.origin, cg_gun_y.value, cg.refdef.viewaxis[1], hand.origin);
    VectorMA(hand.origin, (cg_gun_z.value + fovOffset), cg.refdef.viewaxis[2], hand.origin);

    AnglesToAxis(angles, hand.axis);

    // map torso animations to weapon animations
    if (cg_gun_frame.integer) {
        // development tool
        hand.frame = hand.oldframe = cg_gun_frame.integer;
        hand.backlerp = 0;
    } else {
        // get clientinfo for animation map
        ci = &cgs.clientinfo[cent->currentState.clientNum];
        hand.frame = CG_MapTorsoToWeaponFrame(ci, cent->pe.torso.frame);
        hand.oldframe = CG_MapTorsoToWeaponFrame(ci, cent->pe.torso.oldFrame);
        hand.backlerp = cent->pe.torso.backlerp;
    }

    hand.hModel = weapon->handsModel;
    hand.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;

    // add everything onto the hand
    CG_AddPlayerWeapon(&hand, ps, &cg.predictedPlayerEntity, ps->persistant[PERS_TEAM]);
}

/*
==============================================================================

WEAPON SELECTION

==============================================================================
*/

/*
===================
CG_DrawWeaponSelect
===================
*/
void CG_DrawWeaponSelect(void) {
    int i;
    int bits;
    int count;
    int x, y;
    char* name;
    float* color;

    // don't display if dead
    if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        return;
    }

    color = CG_FadeColor(cg.weaponSelectTime, WEAPON_SELECT_TIME);
    if (!color) {
        return;
    }
    trap_R_SetColor(color);

    // showing weapon select clears pickup item display, but not the blend blob
    cg.itemPickupTime = 0;

    // count the number of weapons owned
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

        // draw weapon icon
        CG_DrawPic(x, y, 32, 32, cg_weapons[i].weaponIcon);

        // draw selection marker
        if (i == cg.weaponSelect) {
            CG_DrawPic(x - 4, y - 4, 40, 40, cgs.media.selectShader);
        }

        // no ammo cross on top
        if (!cg.snap->ps.ammo[i]) {
            CG_DrawPic(x, y, 32, 32, cgs.media.noammoShader);
        }

        x += 40;
    }

    // draw the selected name
    if (cg_weapons[cg.weaponSelect].item) {
        float scale = 0.4f;
        float tw;
        name = cg_weapons[cg.weaponSelect].item->pickup_name;
        if (name) {
            tw = CG_Text_Width(name, scale, 0);
            CG_Text_Paint(320 - tw / 2, y - 12, scale, color, name, 0, 0, 6 /*ITEM_TEXTSTYLE_SHADOWEDMORE*/);
        }
    }

    trap_R_SetColor(NULL);
}

/*
===================
CG_DrawWeaponBar
[QL] Draw the weapon bar in all modes (binary: FUN_10053960 + FUN_10053100)
  cg_weaponBar: 0=No, 1=Left, 2=Right, 3=Centered, 4=Classic
  cg_drawFullWeaponBar: 0=in inventory only, 1=all available weapons on map
===================
*/
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

    // don't display if dead or no weapon bar mode
    if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        return;
    }
    if (cg_weaponBar.integer == 0) {
        return;
    }

    // [QL] 0x10053129: "spectating" is the same test CG_Draw2D uses to pick its
    // spectator branch - EF_VOTED or PM_SPECTATOR on the predicted playerState - and a
    // spectator gets no weapon bar at all while the scoreboard is up (0x10053199).
    isSpectating = (cg.predictedPlayerState.eFlags & EF_VOTED) ||
                   cg.predictedPlayerState.pm_type == PM_SPECTATOR;
    if (isSpectating && cg.scoreBoardShowing) {
        return;
    }

    // Mode 4 = Classic - use Q3-style horizontal bar (only on weapon switch)
    if (cg_weaponBar.integer == 4) {
        CG_DrawWeaponSelect();
        return;
    }

    // For modes 1/2/3: always visible, no fade
    bits = cg.snap->ps.stats[STAT_WEAPONS];
    selectedWeapon = isSpectating ? cg.snap->ps.weapon : cg.weaponSelect;

    // Count weapons for centered mode
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

    // [QL] Layout, 0x10053208..0x100532af. The mode only picks the layout for a
    // player: a SPECTATOR always gets the horizontal centred bar, whatever cg_weaponBar
    // says, and it sits lower than a player's (y 452 against 413: 0x19d plus 0x27 when
    // spectating). The version kept before switched on cg_weaponBar alone, so a
    // spectator with the default left bar got a vertical bar down the side, and the
    // centred one was placed at 436.
    if (!isSpectating && cg_weaponBar.integer != 3) {
        if (cg_weaponBar.integer == 2) {  // Right
            x = 623.0f;
            ammoOffset = -3;
            textAlign = 16;
        } else {                          // Left, and any other value
            x = 1.0f;
            ammoOffset = 20;
            textAlign = 2;
        }
        y = 94.0f;
        xStep = 0.0f;
        yStep = 22.0f;
    } else {                              // Centred, or spectating
        x = 320.0f - (float)count * 55.0f * 0.5f;
        y = isSpectating ? 452.0f : 413.0f;
        xStep = 55.0f;
        yStep = 0.0f;
        ammoOffset = 20;
        textAlign = 8;
    }

    trap_R_SetColor(NULL);

    // Set widescreen mode
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
            // Show all available weapons (dimmed if not in inventory)
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

        // Draw selection highlight behind selected weapon
        if (i == selectedWeapon) {
            if (cg_weaponBar.integer == 2) {
                // Right mode: highlight extends left
                CG_DrawPic(x - 33.0f, y - 2.0f, 52.0f, 20.0f,
                           cgs.media.weaponBarHighlightShader);
            } else {
                CG_DrawPic(x - 4.0f, y - 2.0f, 52.0f, 20.0f,
                           cgs.media.weaponBarHighlightShader);
            }
        }

        // Draw weapon icon (16x16)
        if (hasWeapon || cg_drawFullWeaponBar.integer) {
            CG_DrawPic(x, y, 16.0f, 16.0f, icon);
        }

        // Draw ammo count
        if (hasWeapon) {
            ammo = cg.snap->ps.ammo[i];
            if (ammo < 0) {
                // Infinite ammo
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
                    // Right mode: ammo text left of icon
                    float tw = CG_Text_Width(ammoStr, 0.24f, 0);
                    textX = x + (float)ammoOffset - tw;
                }

                CG_Text_Paint(textX, y + 13.0f, 0.24f, *ammoColor, ammoStr, 0, 0, 0);
            }

            // Draw no-ammo overlay
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

/*
===============
CG_WeaponSelectable
===============
*/
static qboolean CG_WeaponSelectable(int i) {
    if (!cg.snap->ps.ammo[i]) {
        return qfalse;
    }
    if (!(cg.snap->ps.stats[STAT_WEAPONS] & (1 << i))) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
CG_NextWeapon_f
===============
*/
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
            continue;  // never cycle to gauntlet
        }
        if (CG_WeaponSelectable(cg.weaponSelect)) {
            break;
        }
    }
    if (i == MAX_WEAPONS) {
        cg.weaponSelect = original;
    }
}

/*
===============
CG_PrevWeapon_f
===============
*/
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
            continue;  // never cycle to gauntlet
        }
        if (CG_WeaponSelectable(cg.weaponSelect)) {
            break;
        }
    }
    if (i == MAX_WEAPONS) {
        cg.weaponSelect = original;
    }
}

/*
===============
CG_Weapon_f
===============
*/
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
        return;  // don't have the weapon
    }

    // QL binary: cg_switchToEmpty.integer allows selecting weapons with no ammo (vmCvar 0x10B6FD00)
    if (!cg.snap->ps.ammo[num] && !cg_switchToEmpty.integer) {
        return;  // no ammo and switching to empty is disabled
    }

    cg.weaponSelect = num;
}

/*
===================
CG_OutOfAmmoChange

The current weapon has just run out of ammo
===================
*/
void CG_OutOfAmmoChange(void) {
    int i;

    // [QL] 0x10053d50: starts at the last real weapon
    cg.weaponSelectTime = cg.time;

    for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
        if (CG_WeaponSelectable(i)) {
            cg.weaponSelect = i;
            break;
        }
    }
}

/*
===================
CG_DropWeaponChange

[QL] cgamex86.dll 0x10053d90. The same walk as CG_OutOfAmmoChange, skipping the weapon
that was just dropped.
===================
*/
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

/*
===================================================================================================

WEAPON EVENTS

===================================================================================================
*/

/*
================
CG_FireWeapon

Caused by an EV_FIRE_WEAPON event
================
*/
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

    // mark the entity as muzzle flashing, so when it is added it will
    // append the flash to the weapon model
    cent->muzzleFlashTime = cg.time;

    if (ent->weapon == WP_RAILGUN) {
        cent->pe.railFireTime = cg.time;
    }

    // [QL] resolve the non-predicted centity (EF_NODRAW players are drawn from
    // cg_entities[number]). A weapon whose looping sound replaces its shots, the lightning gun and the
    // gauntlet, plays nothing else while it fires; the chaingun's whine sounds under its shots
    // (loopFireSound, 0x100510fd), so it keeps them, and its brass and quad sound with them.
    nonPredictedCent = cent;
    if (ent->eFlags & EF_NODRAW) {
        nonPredictedCent = &cg_entities[ent->number];
    }
    // [QL] 0x10053e2a: retail mira solo este flag. El gauntlet no entra porque su golpe es un
    // disparo con su propio sonido, no el zumbido del lightning gun.
    if (nonPredictedCent->pe.lightningFiring && ent->weapon != WP_GAUNTLET) {
        return;
    }

    // play a sound
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

    // do brass ejection
    if (weap->ejectBrassFunc && cg_brassTime.integer > 0) {
        weap->ejectBrassFunc(cent);
    }

    // play quad sound if needed, rate-limited so continuous-fire weapons don't spam it
    if (cent->currentState.powerups & (1 << PW_QUAD)) {
        // [QL] binary gates this on (pe.barrelSpinning2 == 0 || cg.time - lastQuad > 999).
        // ioquakelive's playerEntity_t has no barrelSpinning2 flag (binary pe offset
        // 0x298), so approximate it: continuous-fire weapons (MG/plasma/CG/HMG while
        // firing) get the 999 ms cooldown; every other weapon plays unthrottled.
        qboolean continuousFire =
            (nonPredictedCent->currentState.eFlags & EF_FIRING) &&
            (ent->weapon == WP_MACHINEGUN || ent->weapon == WP_PLASMAGUN ||
             ent->weapon == WP_CHAINGUN || ent->weapon == WP_HMG);
        if (!continuousFire || cg.time - lastQuadSoundTime > 999) {
            trap_S_StartSound(NULL, cent->currentState.number, CHAN_ITEM, cgs.media.quadSound);
            lastQuadSoundTime = cg.time;
        }
    }
}

/*
=================
CG_MissileHitWall

Caused by an EV_MISSILE_MISS event, or directly by local bullet tracing
=================
*/
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

    // set defaults
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
            // no explosion at LG impact, it is added with the beam
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
            // QL binary: uses cg_rocketStyle.integer == 2 (vmCvar 0x10A38300)
            if (cg_rocketStyle.integer == 2) {
                // explosion sprite animation
                VectorMA(origin, 24, dir, sprOrg);
                VectorScale(dir, 64, sprVel);

                CG_ParticleExplosion("explode1", sprOrg, sprVel, 1400, 20, 30);
            }
            break;
        case WP_RAILGUN:
            mod = cgs.media.ringFlashModel;
            shader = cgs.media.railExplosionShader;
            // sfx = cgs.media.sfx_railg;
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

    //
    // create the explosion
    //
    if (mod) {
        le = CG_MakeExplosion(origin, dir,
                              mod, shader,
                              duration, isSprite);
        le->light = light;
        VectorCopy(lightColor, le->lightColor);
        if (weapon == WP_RAILGUN) {
            // colorize with client color
            VectorCopy(cgs.clientinfo[clientNum].color1, le->color);
            le->refEntity.shaderRGBA[0] = le->color[0] * 0xff;
            le->refEntity.shaderRGBA[1] = le->color[1] * 0xff;
            le->refEntity.shaderRGBA[2] = le->color[2] * 0xff;
            le->refEntity.shaderRGBA[3] = 0xff;
        }
    }

    //
    // impact mark
    //
    alphaFade = (mark == cgs.media.energyMarkShader);  // plasma fades alpha, all others fade color
    if (weapon == WP_RAILGUN) {
        vec3_t teamColor;
        float* color;

        // [QL] apply the team rail-colour override to the mark, else the client's colour1
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

/*
=================
CG_MissileHitPlayer
=================
*/
void CG_MissileHitPlayer(int weapon, vec3_t origin, vec3_t dir, int entityNum) {
    // QL binary: calls CG_BloodSplatEffect twice (not CG_Bleed), skips self
    if (entityNum != cg.snap->ps.clientNum) {
        int i;
        for (i = 0; i < 2; i++) {
            CG_BloodSplatEffect(origin, entityNum);
        }
        // QL binary: impact sparks (vmCvar 0x10B6FBE0, 0x10A63CC0, 0x10A67AA0, 0x10A66D20)
        if (cg_impactSparks.integer) {
            vec3_t vel = { 0, 0, cg_impactSparksVelocity.value };
            CG_SpawnParticleEffect(origin, vel, (float)cg_impactSparksSize.integer,
                1.0f, 1.0f, 1.0f, 1.0f,
                (float)cg_impactSparksLifetime.integer,
                cg.time, 1, cgs.media.sparkParticleShader);
        }
    }

    // some weapons will make an explosion with the blood, while
    // others will just make the blood
    switch (weapon) {
        // [QL] binary splash set is GL, RL, BFG, PROX (weapons 4/5/9/12).
        // Plasma does NOT make a wall explosion on players; prox does.
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

/*
=================
CG_MissileHitWall_DmgThrough
Address: 0x10054db0
[QL] New "damage-through-surface" (wallbang) impact effect. Projects forward
from the impact point by cgs.dmgThroughDepth (server-sent surface depth, parsed
in cg_servercmds.c), traces back to the entry surface, lays a scorch mark there,
throws 10 debris particles, then calls the normal CG_MissileHitWall for the
visible impact. Called from CG_EntityEvent (cg_event.c) on
EV_MISSILE_MISS_DMGTHROUGH.
Verified against Ghidra:
  - depth is cgs.dmgThroughDepth (_DAT_10a5fda8), NOT a cvar.
  - trace back from projected point with mask CONTENTS_SOLID, skipNum -1.
  - debris use CG_SpawnParticleEffect (type 0, debrisPuffShader), NOT CG_SmokePuff.
  - debris velocity direction is the incoming missile dir.
=================
*/
void CG_MissileHitWall_DmgThrough(vec3_t origin, vec3_t dir, int weapon) {
    trace_t trace;
    vec3_t end;
    int i;

    // Project forward through the surface by the server-sent depth, then trace back
    VectorMA(origin, cgs.dmgThroughDepth, dir, end);
    CG_Trace(&trace, end, NULL, NULL, origin, -1, CONTENTS_SOLID);

    if (trace.fraction != 1.0f && trace.surfaceFlags == 0) {
        // scorch mark at the exit surface (binary builds a white mark, radius 64)
        CG_ImpactMark(cgs.media.burnMarkShader, trace.endpos, trace.plane.normal,
                      random() * 360, 1, 1, 1, 1, qfalse, 64, qfalse);

        // 10 debris particles thrown along the missile direction
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

/*
============================================================================

SHOTGUN TRACING

============================================================================
*/

/*
================
CG_ShotgunPellet
================
*/
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

    // FIXME: should probably move this cruft into CG_BubbleTrail
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
            // SURF_NOIMPACT will not make a flame puff or a mark
            return;
        }
        if (tr.surfaceFlags & SURF_METALSTEPS) {
            CG_MissileHitWall(WP_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_METAL);
        } else {
            CG_MissileHitWall(WP_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_DEFAULT);
        }
    }
}

/*
================
CG_ShotgunPattern

Perform the same traces the server did to locate the
hit splashes
================
*/
static void CG_ShotgunPattern(vec3_t origin, vec3_t origin2, int seed, int otherEntNum) {
    int i;
    float r, u, angle, ringRadius, jitter;
    vec3_t end;
    vec3_t forward, right, up;

    // derive the right and up vectors from the forward vector, because
    // the client won't have any other information
    VectorNormalize2(origin2, forward);
    PerpendicularVector(right, forward);
    CrossProduct(forward, right, up);

    // [QL] Concentric ring pattern (binary-verified from cgamex86.dll 0x10055650)
    // 3 rings: inner(6 pellets, r=4), middle(6, r=8), outer(8, r=12) = 20 total
    for (i = 0; i < DEFAULT_SHOTGUN_COUNT; i++) {
        if (i < 6) {
            jitter = 0.4f;
            ringRadius = 4000;   // [QL] binary FILD 0xfa0 (server spread baked in)
            angle = (float)(i - 20) * 1.0471976f;   // pi/3 = 60 degree spacing
        } else if (i < 12) {
            jitter = 0.3f;
            ringRadius = 8000;   // [QL] binary FILD 0x1f40
            angle = (float)i * 1.0471976f + 30.0f;  // 30 radian offset (binary value)
        } else {
            jitter = 0.2f;
            ringRadius = 12000;  // [QL] binary FILD 0x2ee0
            angle = (float)i * 0.7853982f;           // pi/4 = 45 degree spacing
        }

        // [QL] jitter gate is cg_trueShotgun (DAT_10a63f0c), NOT gametype.
        // When set, the concentric ring pattern is perfectly tight (no seed spread).
        if (cg_trueShotgun.integer) {
            jitter = 0.0f;
        }

        // LCG PRNG: seed = (seed * 0xDCD + 1) & 0xFFFF - 16-bit wrap
        // (binary uses int but 1/65536 normalizer implies 16-bit range)
        seed = (seed * 0xDCD + 1) & 0xFFFF;
        r = (cos(angle) + ((float)seed * 1.5258789e-05f - 0.5f) * 2.0f * jitter) * ringRadius;

        seed = (seed * 0xDCD + 1) & 0xFFFF;
        u = (sin(angle) + ((float)seed * 1.5258789e-05f - 0.5f) * 2.0f * jitter) * ringRadius;

        VectorMA(origin, 8192 * 16, forward, end);
        // [QL] r/u already include the (large) ring radius; no DEFAULT_SHOTGUN_SPREAD scale
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        CG_ShotgunPellet(origin, end, otherEntNum);
    }
}

/*
==============
CG_ShotgunFire
==============
*/
void CG_ShotgunFire(entityState_t* es) {
    vec3_t v;
    int contents;

    CG_ShotgunPattern(es->pos.trBase, es->origin2, es->eventParm, es->otherEntityNum);
    // QL binary: cg_smoke_SG.integer gates shotgun smoke puff (vmCvar 0x10A62D00)
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

/*
==============
CG_ShotgunKillEffect
Address: 0x10055aa0
[QL] New. Spawns blood splats at randomised offsets around the victim's origin
for the shotgun multi-pellet kill visual. Count = currentState.generic1 / 5.
Called from CG_EntityEvent (cg_event.c) on EV_SHOTGUN_KILL. Uses the impact-spark
effect (gate DAT_10b6fbec = cg_impactSparks), mirroring CG_MissileHitPlayer,
NOT a cg_blood puff.
==============
*/
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
            // [QL] blood colour comes from the VICTIM in otherEntityNum2 (0x98);
            // otherEntityNum (0x94) is the attacker. Binary CG_ShotgunKillEffect 0x10055aa0.
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

/*
============================================================================

BULLETS

============================================================================
*/

/*
===============
CG_Tracer
===============
*/
void CG_Tracer(vec3_t source, vec3_t dest) {
    vec3_t forward, right;
    polyVert_t verts[4];
    vec3_t line;
    float len, begin, end;
    vec3_t start, finish;
    vec3_t midpoint;

    // tracer
    VectorSubtract(dest, source, forward);
    len = VectorNormalize(forward);

    // start at least a little ways from the muzzle
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

    // add the tracer sound
    trap_S_StartSound(midpoint, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.tracerSound);
}

/*
======================
CG_CalcMuzzlePoint
======================
*/
static qboolean CG_CalcMuzzlePoint(int entityNum, vec3_t muzzle) {
    vec3_t forward;
    centity_t* cent;
    int anim;

    if (entityNum == cg.snap->ps.clientNum) {
        VectorCopy(cg.snap->ps.origin, muzzle);
        muzzle[2] += cg.snap->ps.viewheight;
        AngleVectors(cg.snap->ps.viewangles, forward, NULL, NULL);
        // [QL] muzzle forward offset is 3 when the local player is ducked, else 5
    // (binary keys both branches off cg.snap->ps.pm_flags & PMF_DUCKED)
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

    // [QL] muzzle forward offset is 3 when the local player is ducked, else 5
    // (binary keys both branches off cg.snap->ps.pm_flags & PMF_DUCKED)
    VectorMA(muzzle, (cg.snap->ps.pm_flags & PMF_DUCKED) ? 3 : 5, forward, muzzle);

    return qtrue;
}

/*
======================
CG_Bullet

Renders bullet effects.
======================
*/
void CG_Bullet(vec3_t end, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum) {
    trace_t trace;
    int sourceContentType, destContentType;
    vec3_t start;

    // if the shooter is currently valid, calc a source point and possibly
    // do trail effects
    if (sourceEntityNum >= 0 && cg_tracerChance.value > 0) {
        if (CG_CalcMuzzlePoint(sourceEntityNum, start)) {
            sourceContentType = CG_PointContents(start, 0);
            destContentType = CG_PointContents(end, 0);

            // do a complete bubble trail if necessary
            if ((sourceContentType == destContentType) && (sourceContentType & CONTENTS_WATER)) {
                CG_BubbleTrail(start, end, 32);
            }
            // bubble trail from water into air
            else if ((sourceContentType & CONTENTS_WATER)) {
                trap_CM_BoxTrace(&trace, end, start, NULL, NULL, 0, CONTENTS_WATER);
                CG_BubbleTrail(start, trace.endpos, 32);
            }
            // bubble trail from air into water
            else if ((destContentType & CONTENTS_WATER)) {
                trap_CM_BoxTrace(&trace, start, end, NULL, NULL, 0, CONTENTS_WATER);
                CG_BubbleTrail(trace.endpos, end, 32);
            }

            // draw a tracer
            if (random() < cg_tracerChance.value) {
                CG_Tracer(start, end);
            }
        }
    }

    // impact splash and mark
    // [QL] flesh vs wall is decided by the hit entity index, not the 'flesh' param
    if (fleshEntityNum != ENTITYNUM_WORLD) {
        // QL binary: CG_BloodSplatEffect x2 + spark effect (not CG_Bleed), skip self
        if (fleshEntityNum != cg.snap->ps.clientNum) {
            int i;
            for (i = 0; i < 2; i++) {
                CG_BloodSplatEffect(end, fleshEntityNum);
            }
            // QL binary: impact sparks (vmCvar 0x10B6FBE0)
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
