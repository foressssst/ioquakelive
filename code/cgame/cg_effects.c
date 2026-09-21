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

[wolfcam] CG_SmokePuff without the random rotation: a 3.5 unit spark is round,
so the rotation costs a multiply for nothing.
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

[QL] 0x10014670. One ice fragment, shaped like CG_LaunchGib: LE_FRAGMENT,
TR_GRAVITY, bounceFactor 0.6, endTime + 5000 + random() * 3000. Its
leBounceSoundType and leMarkType are 4, the ice type, which trails through
CG_IceTrail (0x1001d790).
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
    le->leMarkType = LEMT_ICE;  // [QL] 4: la estela de hielo mientras vuela
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
CG_IceTrail

[QL] 0x1001d790. The trail an ice shard leaves as it flies: one puff every 150 ms,
where it passed, fading out over two seconds while drifting slowly upwards.

The step is quantised to the clock, not to the frame, so the trail comes out equally
spaced at any framerate.
===============
*/
#define ICE_TRAIL_STEP 150

void CG_IceTrail(localEntity_t* le) {
    int t;
    int last;

    t = ((cg.oldTime - le->startTime + ICE_TRAIL_STEP) / ICE_TRAIL_STEP) * ICE_TRAIL_STEP;
    last = ((cg.time - le->startTime) / ICE_TRAIL_STEP) * ICE_TRAIL_STEP;

    for (; t <= last; t += ICE_TRAIL_STEP) {
        localEntity_t* puff;
        vec3_t at;

        BG_EvaluateTrajectory(&le->pos, le->startTime + t, at);
        puff = CG_SmokePuff(at, vec3_origin, 20.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2000.0f,
                            le->startTime + t, 0, 0, cgs.media.iceTrailShader);
        // [QL] 0x1001d7d0: the puff fades in colour and rises 40 a second
        puff->leType = LE_FADE_RGB;
        puff->pos.trDelta[2] = 40.0f;
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

[QL] 0x10013310. The lightning gun's discharge sprite, spawned at the viewer:
radius = (intensity * 10 + 48) >> 4, duration = intensity + 300. LE_SCALE_FADE
reads only origin and radius, so CG_SmokePuff does the job.
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

    if (cgs.gametype == GT_RACE && (cg.snap->ps.eFlags & EF_SPECTATOR) && !(cg.snap->ps.pm_flags & PMF_FOLLOW)) {
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

[QL] 0x10011680, which Ghidra names CG_DrawDamagePlums: it draws the whole pool,
not only plums. A world sprite of the viewer's own team is skipped, and so is one
of the viewer himself under custom setting 0x40000.
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

[QL] 0x10013da0. Routed through CG_SmokePuff and then overridden: radius 32,
duration 500, shader bloodSpray[rand() & 3], trDelta[2] 40 so the puff sinks.
The binary's leType 6 is this tree's LE_FALL_SCALE_FADE (5), QL's enum carrying
one extra entry earlier on.
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

The blood thrown off at an impact point, called twice per hit as QL does, which
is where the heavier look comes from. The binary's own splat routine
(0x1001da00) carries decal work that is not decoded, so only the pair of puffs
is reproduced.
==================
*/
void CG_BloodSplatEffect(vec3_t origin, int entityNum) {
    CG_Bleed(origin, entityNum);
}

/*
==================
CG_SpawnParticleEffect

[QL] 0x10012f30. One impact spark: a gfx/misc/tracer sprite at a random rotation,
LE_MOVE_SCALE_FADE, spawned by the four hit paths while cg_impactSparks is on
(0x10054ced, 0x10055577, 0x10055b67, 0x1005635f). The RagePro branch is left out:
this engine has no such hardware type.
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
wolfcamql plays Quake Live demos on a Quake 3 engine, so those functions are a
working reconstruction in the same local-entity vocabulary this fork speaks.

There is no blood in any of it. Retail ships its gore as DLC a stock install
does not have, and the retail code self-disables when those assets fail to
resolve, so this is the whole of what Quake Live shows.

Every number lives in a cg_gib* cvar carrying wolfcam's default. Retail
hardcodes all of it, so the cvars are an addition. A few are clamped where
wolfcam trusts the user: a cg_gibTime or cg_gibStepTime of 0 divides by zero,
and a large cg_gibCount overruns MAX_LOCAL_ENTITIES and recycles the entity
CG_AddLocalEntities is walking toward. The clamps sit far outside the useful
range.
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

[wolfcam] The coloured paths draw on "wc/tracer" and "wc/tracerHighlight", two
shaders wolfcamql ships in its own pk3 and a Quake Live install has neither. A
shader that cannot be found comes back as handle 0, which would draw the white
default over every spark, so this falls back to "gfx/misc/tracer" - the shader
the uncoloured path uses, and it takes the tint just as well.
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

One of the spheres a player breaks into. [wolfcam] CG_LaunchGib_ql. Two details
carry the look, and both differ from Quake 3's CG_LaunchGib: the lifetime is a
flat cg_gibTime where Quake 3 rolls 5000..8000, and leMarkType is
LEMT_ELECTROGIB, which CG_AddFragment reads to hang a spark trail off the sphere
while it falls.

wolfcam writes pos.gravity and selects TR_GRAVITY, which in this fork ignores
the field and uses DEFAULT_GRAVITY. TR_CUSTOM_GRAVITY reads it, so the cvar
means what it says at every value.
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
CG_LaunchGib_ql_floating - despite the name it is a smoke puff whose type is
rewritten afterwards, so it travels on CG_SmokePuff_ql's TR_LINEAR trajectory
and fades instead of bouncing. The don't-scale flags keep it a hard point of
light; without them CG_AddMoveScaleFade grows it into a smoke ball.
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
LE_SPRITE_EXPLOSION on the "deathEffect" shader, lifted 16 units so it sits on
the torso, and no dynamic light.

The radius set here is written over every frame - CG_AddSpriteExplosion
hardcodes `42 * (1 - c) + 30` - so the sprite swells from 30 to 72 over its life
at a third of full alpha.
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

[QL] 0x10014cad, the branch taken when the gore pack is installed. Eleven pieces
thrown from the body origin, with the head decided by a coin flip (0x10014cb9).
That is Quake 3's CG_GibPlayer to the constant, which is the point of the pack.
Unreachable, and so untestable, on a stock install.
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

[QL] 0x10014c80. There is no cvar: the switch is whether dlc_gibs/skull.md3
registered (the handle at 0x10a5f430, tested at 0x10014c94). With the gore pack
installed the body parts come out, without it the electrogibs do. The same
handle steers the death sound (0x1001bf80), the bounce sound (0x1001ec16) and
the juiced effect (0x1001ab39).
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
