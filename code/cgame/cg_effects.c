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
// cg_effects.c -- these functions generate localentities, usually as a result
// of event processing

#include "cg_local.h"

/*
==================
CG_BubbleTrail

Bullets shot underwater
==================
*/
void CG_BubbleTrail(vec3_t start, vec3_t end, float spacing) {
    vec3_t move;
    vec3_t vec;
    float len;
    int i;

    // [QL] cg_bubbleTrail controls bubble trail rendering (binary: 0x10012c53)
    // Note: Q3 used cg_noProjectileTrail here (disable), QL uses cg_bubbleTrail (enable)
    if (!cg_bubbleTrail.integer) {
        return;
    }

    VectorCopy(start, move);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    // advance a random amount first
    i = rand() % (int)spacing;
    VectorMA(move, i, vec, move);

    VectorScale(vec, spacing, vec);

    for (; i < len; i += spacing) {
        localEntity_t* le;
        refEntity_t* re;

        le = CG_AllocLocalEntity();
        le->leFlags = LEF_PUFF_DONT_SCALE;
        le->leType = LE_MOVE_SCALE_FADE;
        le->startTime = cg.time;
        le->endTime = cg.time + 1000 + random() * 250;
        le->lifeRate = 1.0 / (le->endTime - le->startTime);

        re = &le->refEntity;
        re->shaderTime = cg.time / 1000.0f;

        re->reType = RT_SPRITE;
        re->rotation = 0;
        re->radius = 3;
        re->customShader = cgs.media.waterBubbleShader;
        re->shaderRGBA[0] = 0xff;
        re->shaderRGBA[1] = 0xff;
        re->shaderRGBA[2] = 0xff;
        re->shaderRGBA[3] = 0xff;

        le->color[3] = 1.0;

        le->pos.trType = TR_LINEAR;
        le->pos.trTime = cg.time;
        VectorCopy(move, le->pos.trBase);
        le->pos.trDelta[0] = crandom() * 5;
        le->pos.trDelta[1] = crandom() * 5;
        le->pos.trDelta[2] = crandom() * 5 + 6;

        VectorAdd(move, vec, move);
    }
}

/*
=====================
CG_SmokePuff

Adds a smoke puff or blood trail localEntity.
=====================
*/
localEntity_t* CG_SmokePuff(const vec3_t p, const vec3_t vel, float radius, float r, float g, float b, float a, float duration, int startTime, int fadeInTime, int leFlags, qhandle_t hShader) {
    static int seed = 0x92;
    localEntity_t* le;
    refEntity_t* re;
    //	int fadeInTime = startTime + duration / 2;

    le = CG_AllocLocalEntity();
    le->leFlags = leFlags;
    le->radius = radius;

    re = &le->refEntity;
    re->rotation = Q_random(&seed) * 360;
    re->radius = radius;
    re->shaderTime = startTime / 1000.0f;

    le->leType = LE_MOVE_SCALE_FADE;
    le->startTime = startTime;
    le->fadeInTime = fadeInTime;
    le->endTime = startTime + duration;
    if (fadeInTime > startTime) {
        le->lifeRate = 1.0 / (le->endTime - le->fadeInTime);
    } else {
        le->lifeRate = 1.0 / (le->endTime - le->startTime);
    }
    le->color[0] = r;
    le->color[1] = g;
    le->color[2] = b;
    le->color[3] = a;

    le->pos.trType = TR_LINEAR;
    le->pos.trTime = startTime;
    VectorCopy(vel, le->pos.trDelta);
    VectorCopy(p, le->pos.trBase);

    VectorCopy(p, re->origin);
    re->customShader = hShader;

    // rage pro can't alpha fade, so use a different shader
    if (cgs.glconfig.hardwareType == GLHW_RAGEPRO) {
        re->customShader = cgs.media.smokePuffRageProShader;
        re->shaderRGBA[0] = 0xff;
        re->shaderRGBA[1] = 0xff;
        re->shaderRGBA[2] = 0xff;
        re->shaderRGBA[3] = 0xff;
    } else {
        re->shaderRGBA[0] = le->color[0] * 0xff;
        re->shaderRGBA[1] = le->color[1] * 0xff;
        re->shaderRGBA[2] = le->color[2] * 0xff;
        re->shaderRGBA[3] = 0xff;
    }

    re->reType = RT_SPRITE;
    re->radius = le->radius;

    return le;
}

/*
==================
CG_SmokePuff_ql

[wolfcam] cg_localents.c. CG_SmokePuff with one line taken out: the puff is
not given a random rotation. A smoke cloud wants one so that neighbouring
puffs do not look stamped from the same die; a 3.5 unit spark is round and a
rotation on it costs a multiply for nothing.

Both callers overwrite rotation with 0 straight afterwards, so this is not
strictly load-bearing - but it is the function wolfcam's gib code calls, and
keeping them apart means a later change to one cannot quietly alter the
other.
==================
*/
localEntity_t* CG_SmokePuff_ql(const vec3_t p, const vec3_t vel, float radius, float r, float g, float b, float a, float duration, int startTime, int fadeInTime, int leFlags, qhandle_t hShader) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    le->leFlags = leFlags;
    le->radius = radius;

    re = &le->refEntity;
    re->radius = radius;
    re->shaderTime = startTime / 1000.0f;

    le->leType = LE_MOVE_SCALE_FADE;
    le->startTime = startTime;
    le->fadeInTime = fadeInTime;
    le->endTime = startTime + duration;
    if (fadeInTime > startTime) {
        le->lifeRate = 1.0 / (le->endTime - le->fadeInTime);
    } else {
        le->lifeRate = 1.0 / (le->endTime - le->startTime);
    }
    le->color[0] = r;
    le->color[1] = g;
    le->color[2] = b;
    le->color[3] = a;

    le->pos.trType = TR_LINEAR;
    le->pos.trTime = startTime;
    VectorCopy(vel, le->pos.trDelta);
    VectorCopy(p, le->pos.trBase);

    VectorCopy(p, re->origin);
    re->customShader = hShader;

    // rage pro can't alpha fade, so use a different shader
    if (cgs.glconfig.hardwareType == GLHW_RAGEPRO) {
        re->customShader = cgs.media.smokePuffRageProShader;
        re->shaderRGBA[0] = 0xff;
        re->shaderRGBA[1] = 0xff;
        re->shaderRGBA[2] = 0xff;
        re->shaderRGBA[3] = 0xff;
    } else {
        re->shaderRGBA[0] = le->color[0] * 0xff;
        re->shaderRGBA[1] = le->color[1] * 0xff;
        re->shaderRGBA[2] = le->color[2] * 0xff;
        re->shaderRGBA[3] = 0xff;
    }

    re->reType = RT_SPRITE;
    re->radius = le->radius;

    return le;
}

/*
==================
CG_SpawnEffect

Player teleporting in or out
==================
*/
void CG_SpawnEffect(vec3_t org) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_FADE_RGB;
    le->startTime = cg.time;
    le->endTime = cg.time + 500;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

    re = &le->refEntity;

    re->reType = RT_MODEL;
    re->shaderTime = cg.time / 1000.0f;

    re->hModel = cgs.media.teleportEffectModel;
    AxisClear(re->axis);

    VectorCopy(org, re->origin);
    re->origin[2] += 16;
}

/*
==================
CG_SpawnIceShard

[QL] Launches a single ice-shard fragment (same shape as CG_LaunchGib):
LE_FRAGMENT, axisDefault orientation, TR_GRAVITY trajectory (value 5 in the
binary), bounceFactor 0.6, endTime = startTime + 5000 + random()*3000.

The binary sets leBounceSoundType = leMarkType = 4 (QL bullet-burn/nail types);
ioquakelive's fragment handlers only render BLOOD/BURN, so value 4 leaves no
mark or bounce sound.

Calling convention (binary): hModel is the single stack argument, origin comes
in EBX and velocity in EDI; expressed here as a normal 3-arg C function.
// Address: 0x10014670
==================
*/
void CG_SpawnIceShard(const vec3_t origin, const vec3_t velocity, qhandle_t hModel) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leType = LE_FRAGMENT;  // 3 in binary
    le->startTime = cg.time;
    le->endTime = le->startTime + 5000 + random() * 3000;

    VectorCopy(origin, re->origin);
    AxisCopy(axisDefault, re->axis);
    re->hModel = hModel;

    le->pos.trType = TR_GRAVITY;  // 5 in binary
    VectorCopy(origin, le->pos.trBase);
    VectorCopy(velocity, le->pos.trDelta);
    le->pos.trTime = cg.time;

    le->bounceFactor = 0.6f;

    // [QL] binary value 4 (LEBS_NAIL / LEMT_BULLETBURN); no ioquakelive equivalent
    le->leBounceSoundType = 4;
    le->leMarkType = 4;
}

/*
==================
CG_ThawPlayer

[QL] Spawns ice shards when a frozen player is thawed. Takes the thaw origin as
a vec3_t (passed in ESI in the binary), NOT a centity_t. Spawns exactly 7 shards
at the same origin, each with an independent random velocity (crandom()*250 per
component). The two shard models alternate white,blue,white,... (indices 0..6).
// Address: 0x100147d0
==================
*/
void CG_ThawPlayer(const vec3_t origin) {
    qhandle_t whiteIce = cgs.media.iceWhiteModel;  // 0x10a5f428
    qhandle_t blueIce = cgs.media.iceBlueModel;    // 0x10a5f42c
    vec3_t vel;
    int i;
    qhandle_t model;

    for (i = 0; i < 7; i++) {
        vel[0] = crandom() * 250.0f;
        vel[1] = crandom() * 250.0f;
        vel[2] = crandom() * 250.0f;

        model = (i & 1) ? blueIce : whiteIce;  // white, blue, white, ...
        CG_SpawnIceShard(origin, vel, model);
    }
}

/*
===============
CG_LightningBoltBeam
===============
*/
void CG_LightningBoltBeam(vec3_t start, vec3_t end) {
    localEntity_t* le;
    refEntity_t* beam;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_SHOWREFENTITY;
    le->startTime = cg.time;
    le->endTime = cg.time + 50;

    beam = &le->refEntity;

    VectorCopy(start, beam->origin);
    // this is the end point
    VectorCopy(end, beam->oldorigin);

    beam->reType = RT_LIGHTNING;
    beam->customShader = cgs.media.lightningShader[0];
}

/*
==================
CG_LightningDischargeEffect

[QL] Brief electrical-discharge sprite for the lightning gun's water/self
discharge. Takes the discharge intensity (no origin): the sprite is spawned
at the viewer (cg.refdef.vieworg). Final leType is LE_SCALE_FADE, handled by
CG_AddScaleFade.

  radius   = (intensity * 10 + 48) >> 4
  duration = intensity + 300

The binary calls CG_SpawnParticleEffect (0x10012f30), which is a
CG_SmokePuff-equivalent spawner (fadeInTime = 0, zero velocity). Since
LE_SCALE_FADE uses only re->origin and radius, CG_SmokePuff does the
job.
// Address: 0x10013310
==================
*/
void CG_LightningDischargeEffect(int intensity) {
    qhandle_t shader;
    localEntity_t* le;
    float radius;
    float duration;

    shader = trap_R_RegisterShader("models/weaphits/electric.tga");

    radius = (float)((intensity * 10 + 0x30) >> 4);
    duration = (float)(intensity + 300);

    le = CG_SmokePuff(cg.refdef.vieworg, vec3_origin,
                      radius,
                      1.0f, 1.0f, 1.0f, 1.0f,
                      duration,
                      cg.time,
                      0,   // fadeInTime
                      0,   // leFlags
                      shader);

    le->leType = LE_SCALE_FADE;  // 8 in binary (rendered by CG_AddScaleFade)
}

/*
==================
CG_KamikazeEffect
==================
*/
void CG_KamikazeEffect(vec3_t org) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_KAMIKAZE;
    le->startTime = cg.time;
    le->endTime = cg.time + 3000;  // 2250;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

    VectorClear(le->angles.trBase);

    re = &le->refEntity;

    re->reType = RT_MODEL;
    re->shaderTime = cg.time / 1000.0f;

    re->hModel = cgs.media.kamikazeEffectModel;

    VectorCopy(org, re->origin);
}

/*
==================
CG_ObeliskExplode
==================
*/
void CG_ObeliskExplode(vec3_t org, int entityNum) {
    localEntity_t* le;
    vec3_t origin;

    // create an explosion
    VectorCopy(org, origin);
    origin[2] += 64;
    le = CG_MakeExplosion(origin, vec3_origin,
                          cgs.media.dishFlashModel,
                          cgs.media.rocketExplosionShader,
                          600, qtrue);
    le->light = 300;
    le->lightColor[0] = 1;
    le->lightColor[1] = 0.75;
    le->lightColor[2] = 0.0;
}

/*
==================
CG_ObeliskPain
==================
*/
void CG_ObeliskPain(vec3_t org) {
    float r;
    sfxHandle_t sfx;

    // hit sound
    r = rand() & 3;
    if (r < 2) {
        sfx = cgs.media.obeliskHitSound1;
    } else if (r == 2) {
        sfx = cgs.media.obeliskHitSound2;
    } else {
        sfx = cgs.media.obeliskHitSound3;
    }
    trap_S_StartSound(org, ENTITYNUM_NONE, CHAN_BODY, sfx);
}

/*
==================
CG_InvulnerabilityImpact
==================
*/
void CG_InvulnerabilityImpact(vec3_t org, vec3_t angles) {
    localEntity_t* le;
    refEntity_t* re;
    int r;
    sfxHandle_t sfx;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_INVULIMPACT;
    le->startTime = cg.time;
    le->endTime = cg.time + 1000;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

    re = &le->refEntity;

    re->reType = RT_MODEL;
    re->shaderTime = cg.time / 1000.0f;

    re->hModel = cgs.media.invulnerabilityImpactModel;

    VectorCopy(org, re->origin);
    AnglesToAxis(angles, re->axis);

    r = rand() & 3;
    if (r < 2) {
        sfx = cgs.media.invulnerabilityImpactSound1;
    } else if (r == 2) {
        sfx = cgs.media.invulnerabilityImpactSound2;
    } else {
        sfx = cgs.media.invulnerabilityImpactSound3;
    }
    trap_S_StartSound(org, ENTITYNUM_NONE, CHAN_BODY, sfx);
}

/*
==================
CG_InvulnerabilityJuiced

[QL] cgamex86.dll 0x10013920, unchanged from Team Arena; the model and the sound are the
gore pack's dlc_gibs/juicer.md3 and invul_juiced.ogg. EV_JUICED calls it when the pack is loaded.
==================
*/
void CG_InvulnerabilityJuiced(vec3_t org) {
    localEntity_t* le;
    refEntity_t* re;
    vec3_t angles;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_INVULJUICED;
    le->startTime = cg.time;
    le->endTime = cg.time + 10000;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

    re = &le->refEntity;

    re->reType = RT_MODEL;
    re->shaderTime = cg.time / 1000.0f;

    re->hModel = cgs.media.invulnerabilityJuicedModel;

    VectorCopy(org, re->origin);
    VectorClear(angles);
    AnglesToAxis(angles, re->axis);

    trap_S_StartSound(org, ENTITYNUM_NONE, CHAN_BODY, cgs.media.invulnerabilityJuicedSound);
}

/*
==================
CG_ScorePlum
==================
*/
void CG_ScorePlum(int client, vec3_t org, int score) {
    localEntity_t* le;
    refEntity_t* re;
    vec3_t angles;
    static vec3_t lastPos;

    // only visualize for the client that scored
    if (client != cg.predictedPlayerState.clientNum || cg_scorePlum.integer == 0) {
        return;
    }

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_SCOREPLUM;
    le->startTime = cg.time;
    le->endTime = cg.time + 4000;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;
    le->radius = score;

    VectorCopy(org, le->pos.trBase);
    if (org[2] >= lastPos[2] - 20 && org[2] <= lastPos[2] + 20) {
        le->pos.trBase[2] -= 20;
    }

    // CG_Printf( "Plum origin %i %i %i -- %i\n", (int)org[0], (int)org[1], (int)org[2], (int)Distance(org, lastPos));
    VectorCopy(org, lastPos);

    re = &le->refEntity;

    re->reType = RT_SPRITE;
    re->radius = 16;

    VectorClear(angles);
    AnglesToAxis(angles, re->axis);
}

// ==========================================================================
// [QL] Shared floating-effect pool
//
// cgamex86.dll keeps ONE pool of 32 screen-projected billboards (base
// DAT_10ab9978, 0x80 bytes each) used by damage numbers, player outlines,
// freeze/flag glows and head float-sprites. See floatingEffect_t in cg_local.h.
// The pool is a static array here; it is zero-initialised on cgame load and
// records self-expire in CG_UpdateFloatingEffects, so the binary keeps no
// per-frame full reset (records with a ~1ms lifeRate live a single frame).
// ==========================================================================

static floatingEffect_t cg_floatingEffects[MAX_FLOATING_EFFECTS];

/*
==================
CG_AllocFloatingEffect
// Address: 0x1002a0d0
[QL] Ghidra "CG_AllocMark" - a reused/mislabelled name; this is NOT the Q3 wall-mark
allocator (that lives in cg_marks.c). Returns the first free pool record cleared to sane
defaults (white, full alpha, spawnTime = cg.time, lifeRate 1ms), or NULL if all 32 slots
are busy. In race a spectator who isn't following anybody gets none.
==================
*/
floatingEffect_t* CG_AllocFloatingEffect(void) {
    int i;
    floatingEffect_t* fx;

    if (cgs.gametype == GT_RACE && (cg.snap->ps.eFlags & EF_VOTED) && !(cg.snap->ps.pm_flags & PMF_FOLLOW)) {
        return NULL;
    }

    for (i = 0; i < MAX_FLOATING_EFFECTS; i++) {
        fx = &cg_floatingEffects[i];
        if (!fx->active) {
            memset(fx, 0, sizeof(*fx));
            fx->active = 1;
            fx->spawnTime = cg.time;
            fx->lifeRate = 1.0f;  // 0x3f800000
            fx->color[0] = fx->color[1] = fx->color[2] = fx->color[3] = 1.0f;
            return fx;
        }
    }
    return NULL;
}

/*
==================
CG_ProjectFloatingEffect
[QL] World point -> 640x480 virtual screen (top-left origin). ioquakelive stand-in
for the engine projection pair the binary calls through its import table
(DAT_1074cccc +0x1e0 / +0x1e4); CG_DrawText / CG_DrawPic then map 640x480 to real pixels.
Returns qfalse when the point is behind the view (binary's forward-distance > 0 gate).
==================
*/
qboolean CG_ProjectFloatingEffect(const vec3_t point, float* x, float* y) {
    vec3_t trans;
    float xc, yc, px, py, z;

    VectorSubtract(point, cg.refdef.vieworg, trans);
    z = DotProduct(trans, cg.refdef.viewaxis[0]);  // forward distance
    if (z < 0.001f) {
        return qfalse;
    }
    px = tan(cg.refdef.fov_x * (M_PI / 360.0f));
    py = tan(cg.refdef.fov_y * (M_PI / 360.0f));
    xc = 640.0f * 0.5f;
    yc = 480.0f * 0.5f;
    // viewaxis[1] is left, viewaxis[2] is up (Q3 convention)
    *x = xc - xc * DotProduct(trans, cg.refdef.viewaxis[1]) / (z * px);
    *y = yc - yc * DotProduct(trans, cg.refdef.viewaxis[2]) / (z * py);
    return qtrue;
}

/*
==================
CG_UpdateFloatingEffects
// Address: 0x1002a190
[QL] Ghidra mislabels this "CG_UpdateConfigStrings"; not a config-string updater, it
ages the floating-effect pool. Every active record, once per frame:
frees it when cg.time >= spawnTime + lifeRate, and (for records with doFade set) fades
colour alpha from 1 to 0 across [spawnTime + fadeStart, spawnTime + lifeRate].
==================
*/
void CG_UpdateFloatingEffects(void) {
    int i;
    floatingEffect_t* fx;
    float fadeAt, a;

    for (i = 0; i < MAX_FLOATING_EFFECTS; i++) {
        fx = &cg_floatingEffects[i];
        if (!fx->active) {
            continue;
        }
        if (cg.time >= fx->spawnTime + fx->lifeRate) {
            fx->active = 0;  // expire
            continue;
        }
        if (fx->doFade && fx->lifeRate > 0.0f) {
            fadeAt = fx->spawnTime + fx->fadeStart;
            if (fadeAt <= cg.time) {
                a = 1.0f - (cg.time - fadeAt) / (fx->lifeRate - fx->fadeStart);
                if (a < 0.0f) {
                    a = 0.0f;
                } else if (a > 1.0f) {
                    a = 1.0f;
                }
                fx->color[3] = a;
            }
        }
    }
}

/*
==================
CG_DrawFloatingEffects
// Address: 0x10011680
[QL] Ghidra "CG_DrawDamagePlums" - renders the WHOLE pool, not only plums. Every active record
in front of the view, except a world sprite (FE_SPRITE) of the viewer's own team (team modes)
or, with custom setting 0x40000, of the viewer himself. Its size is the larger of minPixels and
twice the projected height of half worldSize, in 480-line units, and it drifts linearly plus
quadratically with the life fraction. The text goes through CG_DrawText in the 640x480 space;
the picture is drawn in real pixels, square, scaled by the vertical screen scale.
==================
*/
void CG_DrawFloatingEffects(void) {
    int i;
    floatingEffect_t* fx;
    vec3_t base, top, delta;
    float sx, sy, tx, ty;
    float size, life, arc, mx, my, scale;

    for (i = 0; i < MAX_FLOATING_EFFECTS; i++) {
        fx = &cg_floatingEffects[i];
        if (!fx->active) {
            continue;
        }
        if (fx->type == FE_SPRITE &&
            ((cgs.gametype >= GT_TEAM && fx->followNum == cg.snap->ps.persistant[PERS_TEAM]) ||
             ((cgs.customSettings & 0x40000) && fx->clientNum == cg.snap->ps.clientNum))) {
            continue;
        }

        // billboard base point: world origin lifted by zOffset, in front of the view
        VectorCopy(fx->origin, base);
        base[2] += fx->zOffset;
        VectorSubtract(base, cg.refdef.vieworg, delta);
        if (DotProduct(delta, cg.refdef.viewaxis[0]) <= 0) {
            continue;
        }
        if (!CG_ProjectFloatingEffect(base, &sx, &sy)) {
            continue;
        }

        trap_R_SetColor(fx->color);

        // on-screen size = max(minPixels, 2 * projected height of half worldSize)
        size = fx->minPixels;
        VectorMA(base, fx->worldSize * 0.5f, cg.refdef.viewaxis[2], top);
        if (CG_ProjectFloatingEffect(top, &tx, &ty) && size <= fabs(ty - sy) * 2.0f) {
            size = fabs(ty - sy) * 2.0f;
        }

        // life fraction and quadratic drift (binary uses pow(life, 2))
        life = (cg.time - fx->spawnTime) / fx->lifeRate;
        arc = life * life;
        mx = fx->velX * life + fx->accX * arc;
        my = fx->velY * life + fx->accY * arc;

        if (fx->text[0]) {
            CG_DrawText(sx - size * 0.5f + mx, sy - size + my, 0, 0.15f,
                        fx->color, fx->text, 0, 0, 0);
        }

        if (fx->shader) {
            scale = cgs.screenYScale;
            trap_R_DrawStretchPic(cgs.glconfig.vidWidth * sx / 640.0f - size * 0.5f * scale + mx * scale,
                                  cgs.glconfig.vidHeight * sy / 480.0f - size * scale + my * scale,
                                  size * scale, size * scale, 0, 0, 1, 1, fx->shader);
        }

        trap_R_SetColor(NULL);
    }
}

/*
==================
CG_DamagePlum
// Address: 0x10013660
[QL] Floating damage number. Allocates a shared floating-effect record (FE_DAMAGE_NUMBER)
carrying the damage as text; CG_DrawFloatingEffects projects and floats it. Replaces the
old LE_DAMAGEPLUM local-entity / number-sprite path. (The binary also filters by weapon via
a cvar weapon-flag mask; that filter is not reproduced here.)
==================
*/
void CG_DamagePlum(int damage, int weapon, vec3_t org) {
    floatingEffect_t* fx;

    (void)weapon;

    fx = CG_AllocFloatingEffect();
    if (!fx) {
        return;
    }

    fx->type = FE_DAMAGE_NUMBER;
    fx->lifeRate = 1000.0f;  // 0x447a0000
    fx->fadeStart = 600.0f;  // 0x44160000
    fx->doFade = 1;

    // colour style based on cg_damagePlumColorStyle (binary DAT_10b71fec)
    switch (cg_damagePlumColorStyle.integer) {
    case 2:  // damage tier colours
        if (damage >= 76) {
            fx->color[0] = 1.0f; fx->color[1] = 0.0f; fx->color[2] = 0.0f;
        } else if (damage >= 51) {
            fx->color[0] = 1.0f; fx->color[1] = 0.5f; fx->color[2] = 0.0f;
        } else if (damage >= 26) {
            fx->color[0] = 1.0f; fx->color[1] = 1.0f; fx->color[2] = 0.0f;
        } else {
            fx->color[0] = 0.25f; fx->color[1] = 0.5f; fx->color[2] = 1.0f;
        }
        fx->color[3] = 1.0f;
        break;
    default:  // style 1: white (binary calls CG_DamageColor(damage/100); not ported)
        fx->color[0] = fx->color[1] = fx->color[2] = fx->color[3] = 1.0f;
        break;
    }

    VectorCopy(org, fx->origin);
    // random world scatter (+/-10 on each axis)
    fx->origin[0] += crandom() * 10.0f;
    fx->origin[1] += crandom() * 10.0f;
    fx->origin[2] += crandom() * 10.0f;

    // screen drift: rises (screen -Y) and settles quadratically, small X jitter
    fx->velX = crandom() * 50.0f;           // 0x19: +/-50
    fx->velY = -120.0f - random() * 20.0f;  // 0x1a: -120 .. -140
    fx->accY = 150.0f;                      // 0x1c: * life^2

    Com_sprintf(fx->text, sizeof(fx->text), "%i", damage);
}

/*
====================
CG_MakeExplosion
====================
*/
localEntity_t* CG_MakeExplosion(vec3_t origin, vec3_t dir, qhandle_t hModel, qhandle_t shader, int msec, qboolean isSprite) {
    float ang;
    localEntity_t* ex;
    int offset;
    vec3_t tmpVec, newOrigin;

    if (msec <= 0) {
        CG_Error("CG_MakeExplosion: msec = %i", msec);
    }

    // skew the time a bit so they aren't all in sync
    offset = rand() & 63;

    ex = CG_AllocLocalEntity();
    if (isSprite) {
        ex->leType = LE_SPRITE_EXPLOSION;

        // randomly rotate sprite orientation
        ex->refEntity.rotation = rand() % 360;
        VectorScale(dir, 16, tmpVec);
        VectorAdd(tmpVec, origin, newOrigin);
    } else {
        ex->leType = LE_EXPLOSION;
        VectorCopy(origin, newOrigin);

        // set axis with random rotate
        if (!dir) {
            AxisClear(ex->refEntity.axis);
        } else {
            ang = rand() % 360;
            VectorCopy(dir, ex->refEntity.axis[0]);
            RotateAroundDirection(ex->refEntity.axis, ang);
        }
    }

    ex->startTime = cg.time - offset;
    ex->endTime = ex->startTime + msec;

    // bias the time so all shader effects start correctly
    ex->refEntity.shaderTime = ex->startTime / 1000.0f;

    ex->refEntity.hModel = hModel;
    ex->refEntity.customShader = shader;

    // set origin
    VectorCopy(newOrigin, ex->refEntity.origin);
    VectorCopy(newOrigin, ex->refEntity.oldorigin);

    ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

    return ex;
}

/*
=================
CG_Bleed

The spurt of blood when a character gets hit.

[QL] cgamex86.dll 0x10013da0. Q3 built a rotating RT_SPRITE of radius 24 by
hand and left it as LE_EXPLOSION. QL routes the whole thing through
CG_SmokePuff (0x10012f30) and then overrides two fields, exactly the way
CG_BloodTrail does:

    0x10013e2e  radius   = 32.0     (0x10067224, was 24 in Q3)
    0x10013e02  duration = 500.0    (0x10073d68)
    0x10013dfb  shader   = bloodSpray[rand() & 3]
    0x10013e4d  leType   = 6        -> LE_FALL_SCALE_FADE, see note below
    0x10013e54  trDelta[2] = 40.0   (0x10073c68), so the puff sinks as it fades

On the leType: the binary's 6 is not the fork's 6. QL's leType enum carries one
extra entry between LE_MOVE_SCALE_FADE and LE_FALL_SCALE_FADE, which shifts
everything after it by one. Two independent anchors pin that down - the gib
launcher stores 3 (0x10014509) where both enums agree on LE_FRAGMENT, and
CG_SpawnEffect stores 7 where the fork uses LE_FADE_RGB (6). So QL 6 is the
fork's LE_FALL_SCALE_FADE (5), which is also what Q3's own blood trail used and
what makes the trDelta[2] drop mean anything. leType never leaves cgame, so the
symbol is what matters here, not the number.

Not replicated: at 0x10013daf the binary allocates a local entity, sets
RF_THIRD_PERSON on it for your own blood and gives it a random rotation, then
overwrites the register holding it with the origin argument and never uses it
again - dead work left behind when this was refactored onto CG_SmokePuff. The
self-blood check is kept here on the entity that actually gets drawn, which is
what Q3 intended and what the fork already did.
=================
*/
void CG_Bleed(vec3_t origin, int entityNum) {
    localEntity_t* blood;

    if (!cg_blood.integer) {
        return;
    }

    // [QL] 0x10013da0, the very first thing the function does:
    // `cmp [bloodSpray[0]], 0; je return`. That guard is not defensive
    // coding, it is the switch. Retail pak00 defines no bloodSpray1..4
    // shader and ships no image by that name, so RE_RegisterShader hands
    // back 0 and every blood path in the module turns itself off. Quake
    // Live as sold has no blood: the gore is DLC content (dlc_gibs), and
    // cg_blood still defaults to "1" because it never had to be the knob.
    if (!cgs.media.bloodSprayShaders[0]) {
        return;
    }

    blood = CG_SmokePuff(origin, vec3_origin,
                         32,          // radius
                         1, 1, 1, 1,  // color
                         500,         // duration
                         cg.time,     // startTime
                         0,           // fadeInTime
                         0,           // leFlags
                         cgs.media.bloodSprayShaders[rand() & 3]);

    blood->leType = LE_FALL_SCALE_FADE;
    blood->pos.trDelta[2] = 40;

    // don't show the player's own blood in first person
    if (entityNum == cg.snap->ps.clientNum) {
        blood->refEntity.renderfx |= RF_THIRD_PERSON;
    }
}

/*
==================
CG_BloodSplatEffect

The blood thrown off at an impact point. QL calls this twice per hit rather
than once (CG_MissileHitPlayer and the rail/lightning paths in cg_weapons.c),
which is where the heavier look comes from.

The binary has a dedicated splat routine around 0x1001da00 that picks from
bloodSpray[2 + (rand() & 1)] and randomises the radius over 16..47
(0x1001da52: `and esi, 0x1f; add esi, 0x10`), but it carries a 0x1794-byte
frame and a good deal of decal work that has not been decoded, so it is not
reproduced here. What is reproduced is the pair of blood puffs at the hit
point, which is the visible part.
==================
*/
void CG_BloodSplatEffect(vec3_t origin, int entityNum) {
    CG_Bleed(origin, entityNum);
}

/*
==================
CG_SpawnParticleEffect

[QL] cgamex86.dll 0x10012f30: one impact spark. A sprite that flies off along vel and fades as it
goes, which is what the four hit paths spawn while cg_impactSparks is on (0x10054ced, 0x10055577,
0x10055b67, 0x1005635f), each with size, lifetime and upward speed from their cvars.

The picture is all in the sprite: the shader is gfx/misc/tracer, it starts at a random rotation,
and the local entity is a LE_MOVE_SCALE_FADE, which CG_AddMoveScaleFade shrinks and fades over the
life. Retail keeps the origin in a register instead of an argument, so it is one here.

The RagePro branch (a 1998 card, glconfig.hardwareType 3) swaps the shader for smokePuffRagePro
and draws it white; this engine has no such hardware type, so only the sprite path is here.
==================
*/
void CG_SpawnParticleEffect(vec3_t origin, vec3_t vel, float size, float r, float g, float b, float a,
                            float lifetime, int startTime, int flags, qhandle_t shader) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leFlags = flags;
    le->leType = LE_MOVE_SCALE_FADE;
    le->startTime = startTime;
    le->endTime = (int)(startTime + lifetime);
    le->fadeInTime = 0;
    le->lifeRate = 1.0f / (le->endTime - le->startTime);

    le->color[0] = r;
    le->color[1] = g;
    le->color[2] = b;
    le->color[3] = a;
    le->radius = size;

    le->pos.trType = TR_LINEAR;
    le->pos.trTime = startTime;
    VectorCopy(origin, le->pos.trBase);
    VectorCopy(vel, le->pos.trDelta);

    re->reType = RT_SPRITE;
    re->radius = size;
    re->rotation = (rand() & 0xffff) / 65536.0f * 360.0f;
    re->customShader = shader;
    re->shaderTime = startTime / 1000.0f;
    VectorCopy(origin, re->origin);
    re->shaderRGBA[0] = (byte)(r * 255);
    re->shaderRGBA[1] = (byte)(g * 255);
    re->shaderRGBA[2] = (byte)(b * 255);
    re->shaderRGBA[3] = 255;
}

/*
===================
ELECTROGIBS

Quake Live's death effect, reimplemented from wolfcamql's *_ql gib functions.

wolfcamql is a Quake 3 engine that plays Quake Live demos, so its *_ql
functions are a working reconstruction of what the retail client draws,
written in the same local-entity vocabulary this fork already speaks. That
makes it a far better template than a decompiler listing: the shapes line up
one to one and nothing has to be guessed at.

There is no blood anywhere in it. Retail ships its gore as DLC that a stock
install does not have, and the retail code self-disables when those assets
fail to resolve, so this is the whole of what Quake Live actually shows.

Every number lives in a cg_gib* cvar carrying wolfcam's default. Retail has
no such cvars - it hardcodes all of this - so they are an addition rather
than a reconstruction, and the defaults reproduce wolfcam's picture exactly.

A few values are clamped here where wolfcam trusts the user. A cg_gibTime or
cg_gibStepTime of 0 divides by zero, and a large cg_gibCount overruns
MAX_LOCAL_ENTITIES - which does not merely drop entities but recycles the one
CG_AddLocalEntities is walking toward, and crashes the client. The clamps sit
far outside the useful range, so no reachable setting behaves differently
from wolfcam.
===================
*/

// A cg_gib*Color cvar holds a packed 0xRRGGBB. wolfcam's own cvar layer
// parses a leading 0x into vmCvar_t.integer; this engine's does not, so the
// hex spelling - the only one anybody writes a colour in - is read here.
static int CG_ElectroGibColorValue(const vmCvar_t* cvar) {
    const char* s = cvar->string;
    int value;

    if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) {
        return cvar->integer;
    }

    value = 0;
    for (s += 2; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            value = value * 16 + (*s - '0');
        } else if (*s >= 'a' && *s <= 'f') {
            value = value * 16 + (*s - 'a' + 10);
        } else if (*s >= 'A' && *s <= 'F') {
            value = value * 16 + (*s - 'A' + 10);
        } else {
            break;
        }
    }

    return value;
}

/*
===================
CG_ElectroGibTracerShader

[wolfcam] The coloured paths draw on "wc/tracer" and "wc/tracerHighlight",
two shaders wolfcamql ships in its own pk3. A Quake Live install has neither,
and a shader that cannot be found comes back as handle 0 from
trap_R_RegisterShader - so rather than draw the white default over every
spark, this falls back to "gfx/misc/tracer", which is the shader the
uncoloured path uses anyway and takes the tint just as well. Drop wolfcam's
pk3 in and the intended shaders are picked up with no code change.
===================
*/
static qhandle_t CG_ElectroGibTracerShader(qboolean highlight) {
    qhandle_t shader;

    shader = trap_R_RegisterShader(highlight ? "wc/tracerHighlight" : "wc/tracer");
    if (!shader) {
        shader = cgs.media.gibSparkShader;
    }

    return shader;
}

/*
===================
CG_ElectroGibSparkShader

Picks the shader and colour every spark is drawn with. wolfcamql repeats this
block verbatim in CG_LaunchGib_ql_floating and CG_BloodTrail_ql; it is one
function here so the two cannot drift apart. Shared with cg_localents.c,
which draws the trail.
===================
*/
qhandle_t CG_ElectroGibSparkShader(vec4_t color) {
    int packed;

    if (!*cg_gibSparksColor.string) {
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        return cgs.media.gibSparkShader;
    }

    packed = CG_ElectroGibColorValue(&cg_gibSparksColor);
    color[0] = ((packed >> 16) & 0xff) / 255.0f;
    color[1] = ((packed >> 8) & 0xff) / 255.0f;
    color[2] = (packed & 0xff) / 255.0f;
    color[3] = 1.0f;

    return CG_ElectroGibTracerShader(cg_gibSparksHighlight.integer ? qtrue : qfalse);
}

// cg_gibTime drives three lifetimes and a divisor; 0 would make lifeRate
// infinite and the sprite never fade.
static int CG_ElectroGibTime(void) {
    return cg_gibTime.integer < 1 ? 1 : cg_gibTime.integer;
}

/*
===================
CG_LaunchElectroGib

One of the spheres a player breaks into. [wolfcam] CG_LaunchGib_ql.

Two details carry the whole look, and both differ from Quake 3's CG_LaunchGib:

  - The lifetime is a flat cg_gibTime, 1000 ms, where Quake 3 rolls
    5000..8000. Retail gibs are gone in a second; they do not pile up.
  - leMarkType is LEMT_ELECTROGIB, and CG_AddFragment reads that to hang a
    spark trail off the sphere while it is in free fall (CG_ElectroGibTrail
    in cg_localents.c). Without the trail the spheres read as bare rocks.

wolfcam writes pos.gravity and selects TR_GRAVITY, which in this fork ignores
the field and uses DEFAULT_GRAVITY - the same 800 the cvar defaults to, but
only by coincidence. TR_CUSTOM_GRAVITY actually reads the field, so the cvar
means what it says at every value (bg_misc.c, BG_EvaluateTrajectory).
===================
*/
static void CG_LaunchElectroGib(vec3_t origin, vec3_t velocity) {
    localEntity_t* le;
    refEntity_t* re;
    int packed;

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leType = LE_FRAGMENT;
    le->startTime = cg.time;
    le->endTime = le->startTime + CG_ElectroGibTime();

    VectorCopy(origin, re->origin);
    AxisCopy(axisDefault, re->axis);
    re->hModel = cgs.media.gibSphere;

    if (*cg_gibColor.string) {
        packed = CG_ElectroGibColorValue(&cg_gibColor);
        re->customShader = CG_ElectroGibTracerShader(qfalse);
        re->shaderRGBA[0] = (packed >> 16) & 0xff;
        re->shaderRGBA[1] = (packed >> 8) & 0xff;
        re->shaderRGBA[2] = packed & 0xff;
        re->shaderRGBA[3] = 255;
    }

    if (!cg_gibGravity.integer) {
        le->pos.trType = TR_LINEAR;
    } else {
        le->pos.trType = TR_CUSTOM_GRAVITY;
    }
    le->pos.gravity = cg_gibGravity.integer;
    VectorCopy(origin, le->pos.trBase);
    VectorCopy(velocity, le->pos.trDelta);
    le->pos.trTime = cg.time;

    le->bounceFactor = cg_gibBounceFactor.value;

    le->leBounceSoundType = LEBS_ELECTROGIB;
    le->leMarkType = LEMT_ELECTROGIB;
}

/*
===================
CG_LaunchElectroGibSpark

One of the sparks that drift up out of the kill. [wolfcam]
CG_LaunchGib_ql_floating - despite that name it is not a fragment at all. It
is a smoke puff whose type is rewritten afterwards, so it travels on the
TR_LINEAR trajectory CG_SmokePuff_ql set up and fades instead of bouncing.
The don't-scale flags are what keep it a hard point of light; without them
CG_AddMoveScaleFade grows it into a smoke ball.

The velocity the caller passes is purely vertical, which is what makes the
sparks rise clear of the sphere burst instead of travelling with it.
===================
*/
static void CG_LaunchElectroGibSpark(vec3_t origin, vec3_t velocity) {
    localEntity_t* le;
    qhandle_t shader;
    vec4_t color;

    shader = CG_ElectroGibSparkShader(color);

    le = CG_SmokePuff_ql(origin, vec3_origin,
                         cg_gibSparksSize.value,                  // radius
                         color[0], color[1], color[2], color[3],  // color
                         CG_ElectroGibTime(),                     // duration
                         cg.time,                                 // startTime
                         0,                                       // fadeInTime
                         0,                                       // flags
                         shader);

    le->leType = LE_MOVE_SCALE_FADE;
    le->leFlags = LEF_PUFF_DONT_SCALE_NOT_KIDDING_MAN | LEF_PUFF_DONT_SCALE;
    VectorCopy(velocity, le->pos.trDelta);

    le->refEntity.reType = RT_SPRITE;
    VectorCopy(origin, le->refEntity.origin);
    VectorCopy(origin, le->refEntity.oldorigin);
    AxisCopy(axisDefault, le->refEntity.axis);
    le->refEntity.rotation = 0;
}

/*
===================
CG_DeathEffect

The flash the kill opens with. [wolfcam] CG_DeathEffect: a single
LE_SPRITE_EXPLOSION on the "deathEffect" shader, lifted 16 units so it sits
on the torso rather than the feet, and no dynamic light - le->light stays 0,
so CG_AddSpriteExplosion skips the trap_R_AddLightToScene call.

The radius set here is written over every frame: CG_AddSpriteExplosion
hardcodes `42 * (1 - c) + 30`, so the sprite always swells from 30 to 72 over
its life at a third of full alpha. That handler is identical in this fork and
in wolfcamql, so the flash comes out the same without touching it.
===================
*/
static void CG_DeathEffect(vec3_t org) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_SPRITE_EXPLOSION;
    le->startTime = cg.time;
    le->endTime = cg.time + 500;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

    re = &le->refEntity;

    re->reType = RT_SPRITE;
    re->rotation = 0;
    re->radius = 100;
    re->shaderTime = cg.time / 1000.0f;
    re->customShader = cgs.media.deathEffectShader;

    AxisClear(re->axis);

    VectorCopy(org, re->origin);
    re->origin[2] += 16;
}

// [QL] 0x10014ccf / 0x10014d2a, the same constants Quake 3 used.
#define GIB_VELOCITY 250
#define GIB_JUMP 250

/*
===================
CG_LaunchGib

One body part, for the gore branch. This is Quake 3's CG_LaunchGib
unchanged - five to eight seconds of tumbling under world gravity, bouncing
at 0.6, leaving blood marks and splat sounds - because that is what the
workshop pack restores. It shares nothing with CG_LaunchElectroGib beyond
the local-entity type.
===================
*/
static void CG_LaunchGib(vec3_t origin, vec3_t velocity, qhandle_t hModel) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leType = LE_FRAGMENT;
    le->startTime = cg.time;
    le->endTime = le->startTime + 5000 + random() * 3000;

    VectorCopy(origin, re->origin);
    AxisCopy(axisDefault, re->axis);
    re->hModel = hModel;

    le->pos.trType = TR_GRAVITY;
    VectorCopy(origin, le->pos.trBase);
    VectorCopy(velocity, le->pos.trDelta);
    le->pos.trTime = cg.time;

    le->bounceFactor = 0.6f;

    le->leBounceSoundType = LEBS_BLOOD;
    le->leMarkType = LEMT_BLOOD;
}

/*
===================
CG_GibPlayerBodyParts

[QL] cgamex86.dll 0x10014cad, the branch taken when the gore pack is
installed. Eleven pieces thrown from the body origin, each at
crandom() * 250 on x and y and 250 + crandom() * 250 upwards, with the head
decided by a coin flip (0x10014cb9: `call rand; test al, 1`). That is Quake
3's CG_GibPlayer to the constant, which is the point - the pack exists to
give Quake 3's gibs back.

Unreachable on a stock install, and untestable here for the same reason: the
models are workshop content and nothing in this tree may redistribute them.
It is written out anyway so that a player who installs the pack gets the
effect rather than eleven invisible fragments.
===================
*/
static void CG_GibPlayerBodyParts(vec3_t playerOrigin) {
    vec3_t origin, velocity;
    qhandle_t parts[10];
    int i;

    parts[0] = (rand() & 1) ? cgs.media.gibSkull : cgs.media.gibBrain;
    parts[1] = cgs.media.gibAbdomen;
    parts[2] = cgs.media.gibArm;
    parts[3] = cgs.media.gibChest;
    parts[4] = cgs.media.gibFist;
    parts[5] = cgs.media.gibFoot;
    parts[6] = cgs.media.gibForearm;
    parts[7] = cgs.media.gibIntestine;
    parts[8] = cgs.media.gibLeg;
    parts[9] = cgs.media.gibLeg;

    for (i = 0; i < 10; i++) {
        VectorCopy(playerOrigin, origin);
        velocity[0] = crandom() * GIB_VELOCITY;
        velocity[1] = crandom() * GIB_VELOCITY;
        velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

        CG_LaunchGib(origin, velocity, parts[i]);
    }
}

/*
===================
CG_GibPlayer

[QL] cgamex86.dll 0x10014c80. There is no cvar here, and that is the whole
design. The first thing the function does is

  0x10014c94  cmp dword ptr [0x10a5f430], 0
  0x10014ca7  je  0x10014d99

and 0x10a5f430 is not a flag - it is the qhandle_t that 0x10024343 filled in
from trap_R_RegisterModel("dlc_gibs/skull.md3"). Install the Steam Workshop
gore pack and the handle resolves, so the body parts come out; leave it off
and the handle is 0 and the electrogibs do. That one test is Quake Live's
entire enable/disable, and it is what the player is choosing when they
subscribe to the pack.

cg_blood exists in the retail cvar table (0x10076b50: default "1", range 0
to 1, archived) but nothing in cgamex86.dll ever reads it back - there is not
one reference to its vmCvar_t anywhere in .text. It survives so that old
configs keep parsing, not because it does anything.

The same handle steers the other three places gore and electrogibs differ:
0x1001bf80 picks the death sound, 0x1001ec16 picks the bounce sound, and
0x1001ab39 picks the juiced-player effect.

On the direction: a Quake Live server packs the direction of the last hit
into EV_GIB_PLAYER's eventParm, and since GibEntity (0x10046d80) was ported
this fork's does too. Retail's CG_EntityEvent does not seem to decode it: its
EV_GIB_PLAYER case hands 0x10014c80 a local vector the function sets to
(0, 0, -1) on entry (0x10019eea) and a flag of 1. That reading is not
confirmed yet, so for now the callers pass a direction explicitly, and a null
or zero one means straight up.
===================
*/
void CG_GibPlayer(vec3_t playerOrigin, vec3_t dir) {
    vec3_t gibDir, origin, velocity;
    vec3_t aimedVelocity, looseVelocity;
    float gibVelocity, gibJump, mix;
    int count, i;

    // [QL] 0x10014c94
    if (cgs.media.gibSkull) {
        CG_GibPlayerBodyParts(playerOrigin);
        return;
    }

    // allow the sphere count to be turned down for speed
    count = cg_gibCount.integer;
    if (count < 1) {
        return;
    }
    if (count > MAX_ELECTROGIBS) {
        count = MAX_ELECTROGIBS;
    }

    if (dir && (dir[0] || dir[1] || dir[2])) {
        VectorCopy(dir, gibDir);
        VectorNormalize(gibDir);
    } else {
        VectorSet(gibDir, 0, 0, 1);
    }

    CG_DeathEffect(playerOrigin);

    gibVelocity = cg_gibVelocity.value;
    gibJump = cg_gibJump.value;

    mix = cg_gibDirScale.value;
    if (mix > 1.0) {
        mix = 1.0;
    }
    if (mix < 0.0) {
        mix = 0.0;
    }

    for (i = 0; i < count; i++) {
        VectorScale(gibDir,
                    gibVelocity + crandom() * cg_gibVelocityRandomness.value,
                    aimedVelocity);

        aimedVelocity[0] += crandom() * cg_gibRandomness.value;
        aimedVelocity[1] += crandom() * cg_gibRandomness.value;
        aimedVelocity[2] += crandom() * cg_gibRandomnessZ.value + gibJump;

        looseVelocity[0] = crandom() * gibVelocity;
        looseVelocity[1] = crandom() * gibVelocity;
        looseVelocity[2] = gibJump + crandom() * gibVelocity;

        velocity[0] = aimedVelocity[0] * mix + looseVelocity[0] * (1.0 - mix);
        velocity[1] = aimedVelocity[1] * mix + looseVelocity[1] * (1.0 - mix);
        velocity[2] = aimedVelocity[2] * mix + looseVelocity[2] * (1.0 - mix);

        VectorCopy(playerOrigin, origin);
        origin[0] += crandom() * cg_gibOriginOffset.value;
        origin[1] += crandom() * cg_gibOriginOffset.value;
        origin[2] += crandom() * cg_gibOriginOffsetZ.value;

        CG_LaunchElectroGib(origin, velocity);
    }

    for (i = 0; i < count; i++) {
        VectorCopy(playerOrigin, origin);
        origin[0] += crandom() * cg_gibFloatingOriginOffset.value;
        origin[1] += crandom() * cg_gibFloatingOriginOffset.value;
        origin[2] += crandom() * cg_gibFloatingOriginOffsetZ.value;

        velocity[0] = 0;
        velocity[1] = 0;
        velocity[2] = cg_gibFloatingVelocity.value +
                      crandom() * cg_gibFloatingRandomness.value;

        CG_LaunchElectroGibSpark(origin, velocity);
    }
}

/*
==================
CG_LaunchExplode
==================
*/
void CG_LaunchExplode(vec3_t origin, vec3_t velocity, qhandle_t hModel) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    re = &le->refEntity;

    le->leType = LE_FRAGMENT;
    le->startTime = cg.time;
    le->endTime = le->startTime + 10000 + random() * 6000;

    VectorCopy(origin, re->origin);
    AxisCopy(axisDefault, re->axis);
    re->hModel = hModel;

    le->pos.trType = TR_GRAVITY;
    VectorCopy(origin, le->pos.trBase);
    VectorCopy(velocity, le->pos.trDelta);
    le->pos.trTime = cg.time;

    le->bounceFactor = 0.1f;

    le->leBounceSoundType = LEBS_BRASS;
    le->leMarkType = LEMT_NONE;
}

#define EXP_VELOCITY 100
#define EXP_JUMP 150
/*
===================
CG_BigExplode

Generated a bunch of gibs launching out from the bodies location
===================
*/
void CG_BigExplode(vec3_t playerOrigin) {
    vec3_t origin, velocity;

    if (!cg_blood.integer) {
        return;
    }

    VectorCopy(playerOrigin, origin);
    velocity[0] = crandom() * EXP_VELOCITY;
    velocity[1] = crandom() * EXP_VELOCITY;
    velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
    CG_LaunchExplode(origin, velocity, cgs.media.smoke2);

    VectorCopy(playerOrigin, origin);
    velocity[0] = crandom() * EXP_VELOCITY;
    velocity[1] = crandom() * EXP_VELOCITY;
    velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
    CG_LaunchExplode(origin, velocity, cgs.media.smoke2);

    VectorCopy(playerOrigin, origin);
    velocity[0] = crandom() * EXP_VELOCITY * 1.5;
    velocity[1] = crandom() * EXP_VELOCITY * 1.5;
    velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
    CG_LaunchExplode(origin, velocity, cgs.media.smoke2);

    VectorCopy(playerOrigin, origin);
    velocity[0] = crandom() * EXP_VELOCITY * 2.0;
    velocity[1] = crandom() * EXP_VELOCITY * 2.0;
    velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
    CG_LaunchExplode(origin, velocity, cgs.media.smoke2);

    VectorCopy(playerOrigin, origin);
    velocity[0] = crandom() * EXP_VELOCITY * 2.5;
    velocity[1] = crandom() * EXP_VELOCITY * 2.5;
    velocity[2] = EXP_JUMP + crandom() * EXP_VELOCITY;
    CG_LaunchExplode(origin, velocity, cgs.media.smoke2);
}
