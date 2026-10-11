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

void CG_BubbleTrail(vec3_t start, vec3_t end, float spacing) {
    vec3_t move;
    vec3_t vec;
    float len;
    int i;

    if (!cg_bubbleTrail.integer) {
        return;
    }

    VectorCopy(start, move);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

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

localEntity_t* CG_SmokePuff(const vec3_t p, const vec3_t vel, float radius, float r, float g, float b, float a, float duration, int startTime, int fadeInTime, int leFlags, qhandle_t hShader) {
    static int seed = 0x92;
    localEntity_t* le;
    refEntity_t* re;

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

void CG_SpawnIceShard(const vec3_t origin, const vec3_t velocity, qhandle_t hModel) {
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

    le->leBounceSoundType = 4;
    le->leMarkType = LEMT_ICE;
}

void CG_ThawPlayer(const vec3_t origin) {
    qhandle_t whiteIce = cgs.media.iceWhiteModel;
    qhandle_t blueIce = cgs.media.iceBlueModel;
    vec3_t vel;
    int i;
    qhandle_t model;

    for (i = 0; i < 7; i++) {
        vel[0] = crandom() * 250.0f;
        vel[1] = crandom() * 250.0f;
        vel[2] = crandom() * 250.0f;

        model = (i & 1) ? blueIce : whiteIce;
        CG_SpawnIceShard(origin, vel, model);
    }
}

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
        puff->leType = LE_FADE_RGB;
        puff->pos.trDelta[2] = 40.0f;
    }
}

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
    VectorCopy(end, beam->oldorigin);

    beam->reType = RT_LIGHTNING;
    beam->customShader = cgs.media.lightningShader[0];
}

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
                      0,
                      0,
                      shader);

    le->leType = LE_SCALE_FADE;
}

void CG_KamikazeEffect(vec3_t org) {
    localEntity_t* le;
    refEntity_t* re;

    le = CG_AllocLocalEntity();
    le->leFlags = 0;
    le->leType = LE_KAMIKAZE;
    le->startTime = cg.time;
    le->endTime = cg.time + 3000;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

    VectorClear(le->angles.trBase);

    re = &le->refEntity;

    re->reType = RT_MODEL;
    re->shaderTime = cg.time / 1000.0f;

    re->hModel = cgs.media.kamikazeEffectModel;

    VectorCopy(org, re->origin);
}

void CG_ObeliskExplode(vec3_t org, int entityNum) {
    localEntity_t* le;
    vec3_t origin;

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

void CG_ObeliskPain(vec3_t org) {
    float r;
    sfxHandle_t sfx;

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

void CG_ScorePlum(int client, vec3_t org, int score) {
    localEntity_t* le;
    refEntity_t* re;
    vec3_t angles;
    static vec3_t lastPos;

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

    VectorCopy(org, lastPos);

    re = &le->refEntity;

    re->reType = RT_SPRITE;
    re->radius = 16;

    VectorClear(angles);
    AnglesToAxis(angles, re->axis);
}

static floatingEffect_t cg_floatingEffects[MAX_FLOATING_EFFECTS];

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
            fx->lifeRate = 1.0f;
            fx->color[0] = fx->color[1] = fx->color[2] = fx->color[3] = 1.0f;
            return fx;
        }
    }
    return NULL;
}

qboolean CG_ProjectFloatingEffect(const vec3_t point, float* x, float* y) {
    vec3_t trans;
    float xc, yc, px, py, z;

    VectorSubtract(point, cg.refdef.vieworg, trans);
    z = DotProduct(trans, cg.refdef.viewaxis[0]);
    if (z < 0.001f) {
        return qfalse;
    }
    px = tan(cg.refdef.fov_x * (M_PI / 360.0f));
    py = tan(cg.refdef.fov_y * (M_PI / 360.0f));
    xc = 640.0f * 0.5f;
    yc = 480.0f * 0.5f;
    *x = xc - xc * DotProduct(trans, cg.refdef.viewaxis[1]) / (z * px);
    *y = yc - yc * DotProduct(trans, cg.refdef.viewaxis[2]) / (z * py);
    return qtrue;
}

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
            fx->active = 0;
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

        size = fx->minPixels;
        VectorMA(base, fx->worldSize * 0.5f, cg.refdef.viewaxis[2], top);
        if (CG_ProjectFloatingEffect(top, &tx, &ty) && size <= fabs(ty - sy) * 2.0f) {
            size = fabs(ty - sy) * 2.0f;
        }

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

static const vec4_t cg_damagePlumWeaponColors[16] = {
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.847f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f},
    {1.0f, 0.49f, 0.0f, 1.0f},
    {0.007f, 0.564f, 0.07f, 1.0f},
    {1.0f, 0.003f, 0.003f, 1.0f},
    {1.0f, 1.0f, 0.729f, 1.0f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {0.772f, 0.0f, 1.0f, 1.0f},
    {0.003f, 0.352f, 1.0f, 1.0f},
    {0.317f, 0.666f, 0.862f, 1.0f},
    {0.0f, 1.0f, 0.701f, 1.0f},
    {1.0f, 0.0f, 0.486f, 1.0f},
    {0.721f, 0.721f, 0.721f, 1.0f},
    {0.807f, 0.647f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
};

void CG_DamagePlum(int damage, int weapon, vec3_t org) {
    floatingEffect_t* fx;

    if (weapon < 0 || weapon >= 16 || !(BG_ParseWeaponStringtoFlag(cg_damagePlum.string) & (1 << weapon))) {
        return;
    }

    fx = CG_AllocFloatingEffect();
    if (!fx) {
        return;
    }

    fx->type = FE_DAMAGE_NUMBER;
    fx->lifeRate = 1000.0f;
    fx->fadeStart = 600.0f;
    fx->doFade = 1;

    switch (cg_damagePlumColorStyle.integer) {
    case 3:
        Vector4Copy(cg_damagePlumWeaponColors[weapon], fx->color);
        break;
    case 2:
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
    default: {
        float t = damage / 100.0f;
        float gb = 1.0f - t;

        if (gb < 0.0f) {
            gb = 0.0f;
        } else if (gb > 1.0f) {
            gb = 1.0f;
        }
        fx->color[0] = 1.0f;
        fx->color[1] = fx->color[2] = gb;
        fx->color[3] = 1.0f;
        break;
    }
    }

    VectorCopy(org, fx->origin);
    fx->origin[0] += crandom() * 10.0f;
    fx->origin[1] += crandom() * 10.0f;
    fx->origin[2] += crandom() * 10.0f;

    fx->velX = crandom() * 50.0f;
    fx->velY = -120.0f - random() * 20.0f;
    fx->accY = 150.0f;

    Com_sprintf(fx->text, sizeof(fx->text), "%i", damage);
}

localEntity_t* CG_MakeExplosion(vec3_t origin, vec3_t dir, qhandle_t hModel, qhandle_t shader, int msec, qboolean isSprite) {
    float ang;
    localEntity_t* ex;
    int offset;
    vec3_t tmpVec, newOrigin;

    if (msec <= 0) {
        CG_Error("CG_MakeExplosion: msec = %i", msec);
    }

    offset = rand() & 63;

    ex = CG_AllocLocalEntity();
    if (isSprite) {
        ex->leType = LE_SPRITE_EXPLOSION;

        ex->refEntity.rotation = rand() % 360;
        VectorScale(dir, 16, tmpVec);
        VectorAdd(tmpVec, origin, newOrigin);
    } else {
        ex->leType = LE_EXPLOSION;
        VectorCopy(origin, newOrigin);

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

    ex->refEntity.shaderTime = ex->startTime / 1000.0f;

    ex->refEntity.hModel = hModel;
    ex->refEntity.customShader = shader;

    VectorCopy(newOrigin, ex->refEntity.origin);
    VectorCopy(newOrigin, ex->refEntity.oldorigin);

    ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

    return ex;
}

void CG_Bleed(vec3_t origin, int entityNum) {
    localEntity_t* blood;

    if (!cg_blood.integer) {
        return;
    }

    if (!cgs.media.bloodSprayShaders[0]) {
        return;
    }

    blood = CG_SmokePuff(origin, vec3_origin,
                         32,
                         1, 1, 1, 1,
                         500,
                         cg.time,
                         0,
                         0,
                         cgs.media.bloodSprayShaders[rand() & 3]);

    blood->leType = LE_FALL_SCALE_FADE;
    blood->pos.trDelta[2] = 40;

    if (entityNum == cg.snap->ps.clientNum) {
        blood->refEntity.renderfx |= RF_THIRD_PERSON;
    }
}

void CG_BloodSplatEffect(vec3_t origin, int entityNum) {
    CG_Bleed(origin, entityNum);
}

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

static qhandle_t CG_ElectroGibTracerShader(qboolean highlight) {
    qhandle_t shader;

    shader = trap_R_RegisterShader(highlight ? "wc/tracerHighlight" : "wc/tracer");
    if (!shader) {
        shader = cgs.media.gibSparkShader;
    }

    return shader;
}

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

static int CG_ElectroGibTime(void) {
    return cg_gibTime.integer < 1 ? 1 : cg_gibTime.integer;
}

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

static void CG_LaunchElectroGibSpark(vec3_t origin, vec3_t velocity) {
    localEntity_t* le;
    qhandle_t shader;
    vec4_t color;

    shader = CG_ElectroGibSparkShader(color);

    le = CG_SmokePuff_ql(origin, vec3_origin,
                         cg_gibSparksSize.value,
                         color[0], color[1], color[2], color[3],
                         CG_ElectroGibTime(),
                         cg.time,
                         0,
                         0,
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

#define GIB_VELOCITY 250
#define GIB_JUMP 250

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

void CG_GibPlayer(vec3_t playerOrigin, vec3_t dir) {
    vec3_t gibDir, origin, velocity;
    vec3_t aimedVelocity, looseVelocity;
    float gibVelocity, gibJump, mix;
    int count, i;

    if (cgs.media.gibSkull) {
        CG_GibPlayerBodyParts(playerOrigin);
        return;
    }

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
