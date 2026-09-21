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

// cg_localents.c -- every frame, generate renderer commands for locally
// processed entities, like smoke puffs, gibs, shells, etc.

#include "cg_local.h"

#define MAX_LOCAL_ENTITIES 512
localEntity_t cg_localEntities[MAX_LOCAL_ENTITIES];
localEntity_t cg_activeLocalEntities;  // double linked list
localEntity_t* cg_freeLocalEntities;   // single linked list

/*
===================
CG_InitLocalEntities

This is called at startup and for tournement restarts
===================
*/
void CG_InitLocalEntities(void) {
    int i;

    memset(cg_localEntities, 0, sizeof(cg_localEntities));
    cg_activeLocalEntities.next = &cg_activeLocalEntities;
    cg_activeLocalEntities.prev = &cg_activeLocalEntities;
    cg_freeLocalEntities = cg_localEntities;
    for (i = 0; i < MAX_LOCAL_ENTITIES - 1; i++) {
        cg_localEntities[i].next = &cg_localEntities[i + 1];
    }
}

/*
==================
CG_FreeLocalEntity
==================
*/
void CG_FreeLocalEntity(localEntity_t* le) {
    if (!le->prev) {
        CG_Error("CG_FreeLocalEntity: not active");
    }

    // remove from the doubly linked active list
    le->prev->next = le->next;
    le->next->prev = le->prev;

    // the free list is only singly linked
    le->next = cg_freeLocalEntities;
    cg_freeLocalEntities = le;
}

/*
===================
CG_AllocLocalEntity

Will always succeed, even if it requires freeing an old active entity
===================
*/
localEntity_t* CG_AllocLocalEntity(void) {
    localEntity_t* le;

    if (!cg_freeLocalEntities) {
        // no free entities, so free the one at the end of the chain
        // remove the oldest active entity
        CG_FreeLocalEntity(cg_activeLocalEntities.prev);
    }

    le = cg_freeLocalEntities;
    cg_freeLocalEntities = cg_freeLocalEntities->next;

    memset(le, 0, sizeof(*le));

    // link into the active list
    le->next = cg_activeLocalEntities.next;
    le->prev = &cg_activeLocalEntities;
    cg_activeLocalEntities.next->prev = le;
    cg_activeLocalEntities.next = le;
    return le;
}

/*
====================================================================================

FRAGMENT PROCESSING

A fragment localentity interacts with the environment in some way (hitting walls),
or generates more localentities along a trail.

====================================================================================
*/

/*
================
CG_BloodTrail

Leave expanding blood puffs behind gibs
================
*/
void CG_BloodTrail(localEntity_t* le) {
    int t;
    int t2;
    int step;
    vec3_t newOrigin;
    localEntity_t* blood;

    // [QL] 0x1001d6a3 opens with `cmp [bloodSpray[0]], 0; je return`. In
    // retail that shader does not resolve, so the trail is dead code and a
    // flying gib leaves nothing behind. See CG_Bleed for the whole story.
    if (!cgs.media.bloodSprayShaders[0]) {
        return;
    }

    // [QL] cgamex86.dll 0x1001d6c7 rounds on 0x32, not Q3's 150: the trail
    // drops a puff every 50 ms, so a flying gib leaves three times the blood.
    // The shorter puff lifetime below is what pays for that - the two have to
    // move together or the local-entity pool overruns.
    step = 50;
    t = step * ((cg.time - cg.frametime + step) / step);
    t2 = step * (cg.time / step);

    for (; t <= t2; t += step) {
        BG_EvaluateTrajectory(&le->pos, t, newOrigin);

        blood = CG_SmokePuff(newOrigin, vec3_origin,
                             20,          // radius
                             1, 1, 1, 1,  // color
                             500,         // trailTime - [QL] 0x1001d724 pushes
                                          // 500.0 (0x10073d68), the same
                                          // constant CG_Bleed uses, not Q3's
                                          // 2000. At the 50 ms step this is
                                          // what keeps the puff count bounded:
                                          // fifteen gibs at 20 puffs/s live
                                          // 0.5 s each is ~150 entities, where
                                          // Q3's 2 s would be ~600 against a
                                          // MAX_LOCAL_ENTITIES of 512.
                             t,           // startTime
                             0,           // fadeInTime
                             0,           // flags
                             cgs.media.bloodSprayShaders[rand() & 3]);
        // use the optimized version
        blood->leType = LE_FALL_SCALE_FADE;
        // drop a total of 40 units over its lifetime
        blood->pos.trDelta[2] = 40;
    }
}

/*
================
CG_ElectroGibTrail

The stream of sparks a sphere drags behind it while it is still in the air.

[wolfcam] CG_BloodTrail_ql: Quake 3's CG_BloodTrail with every number changed - a
cg_gibStepTime of 50 ms instead of 150, a 3.5 unit spark on "gfx/misc/tracer"
instead of a 20 unit blood puff, and a third of the gib's own lifetime instead of a
flat 2000 ms. That last one matters: a sphere emits twenty sparks over its 1000 ms
life, and the short step with Quake 3's long lifetime overruns MAX_LOCAL_ENTITIES.

CG_AddFragment only calls this while the sphere is in free fall, and
CG_FragmentBounceMark clears leMarkType on the first impact.
================
*/
static void CG_ElectroGibTrail(localEntity_t* le) {
    int t;
    int t2;
    int step;
    int duration;
    vec3_t newOrigin;
    localEntity_t* spark;
    qhandle_t shader;
    vec4_t color;

    shader = CG_ElectroGibSparkShader(color);

    // a step of 0 would spin this loop forever; a duration of 0 gives an
    // infinite lifeRate and a spark that never fades
    step = cg_gibStepTime.integer;
    if (step < 1) {
        step = 1;
    }
    duration = cg_gibTime.integer / 3;
    if (duration < 1) {
        duration = 1;
    }

    t = step * ((cg.time - cg.frametime + step) / step);
    t2 = step * (cg.time / step);

    for (; t <= t2; t += step) {
        BG_EvaluateTrajectory(&le->pos, t, newOrigin);

        spark = CG_SmokePuff_ql(newOrigin, vec3_origin,
                                cg_gibSparksSize.value,                  // radius
                                color[0], color[1], color[2], color[3],  // color
                                duration,                                // duration
                                t,                                       // startTime
                                0,                                       // fadeInTime
                                0,                                       // flags
                                shader);

        spark->leType = LE_MOVE_SCALE_FADE;
        spark->leFlags = LEF_PUFF_DONT_SCALE_NOT_KIDDING_MAN | LEF_PUFF_DONT_SCALE;
        // drift up a total of 40 units over its lifetime
        spark->pos.trDelta[2] = 40;

        spark->refEntity.reType = RT_SPRITE;
        VectorCopy(newOrigin, spark->refEntity.origin);
        VectorCopy(newOrigin, spark->refEntity.oldorigin);
        AxisCopy(axisDefault, spark->refEntity.axis);
        spark->refEntity.rotation = 0;
    }
}

/*
================
CG_FragmentBounceMark
================
*/
void CG_FragmentBounceMark(localEntity_t* le, trace_t* trace) {
    int radius;

    if (le->leMarkType == LEMT_BLOOD) {
        radius = 16 + (rand() & 31);
        CG_ImpactMark(cgs.media.bloodSprayShaders[rand() & 3], trace->endpos, trace->plane.normal, random() * 360,
                      1, 1, 1, 1, qtrue, radius, qfalse);
    } else if (le->leMarkType == LEMT_ELECTROGIB) {
        // [wolfcam] CG_FragmentBounceMark: a ql gib scorches instead of
        // bleeding - burnMarkShader at a fixed radius of 5, small enough
        // that fifteen of them read as scorch specks and not as craters.
        CG_ImpactMark(cgs.media.burnMarkShader, trace->endpos, trace->plane.normal,
                      random() * 360, 1, 1, 1, 1, qtrue, 5, qfalse);
    } else if (le->leMarkType == LEMT_BURN) {
        radius = 8 + (rand() & 15);
        CG_ImpactMark(cgs.media.burnMarkShader, trace->endpos, trace->plane.normal, random() * 360,
                      1, 1, 1, 1, qtrue, radius, qfalse);
    }

    // don't allow a fragment to make multiple marks, or they
    // pile up while settling
    le->leMarkType = LEMT_NONE;
}

/*
================
CG_FragmentBounceSound
================
*/
void CG_FragmentBounceSound(localEntity_t* le, trace_t* trace) {
    if (le->leBounceSoundType == LEBS_BLOOD) {
        // [QL] 0x1001ec16 tests the gore pack's handle and bails out before
        // the sound when it is absent. That guard is the reason retail never
        // plays a silent handle here: a body-part gib can only exist when the
        // pack that carries its impact sounds is installed too.
        //
        // half the gibs will make splat sounds
        if (cgs.media.gibSkull && (rand() & 1)) {
            // [QL] 0x1001ec27: `call rand; and eax, 3`, then a chain over the
            // three flesh impacts.
            int r = rand() & 3;
            sfxHandle_t s;

            if (r == 0) {
                s = cgs.media.gibBounce1Sound;
            } else if (r == 1) {
                s = cgs.media.gibBounce2Sound;
            } else {
                s = cgs.media.gibBounce3Sound;
            }

            trap_S_StartSound(trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, s);
        }
    } else if (le->leBounceSoundType == LEBS_ELECTROGIB) {
        // [wolfcam] CG_FragmentBounceSound: same half-the-time gate as the
        // Quake 3 gib, then an even pick over the four electrogib_bounce
        // samples. The fifth one this fork registers is never reached -
        // wolfcam only knows four, and it is kept loaded rather than
        // guessed at.
        if (rand() & 1) {
            trap_S_StartSound(trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO,
                              cgs.media.electroGibBounceSound[rand() % 4]);
        }
    } else if (le->leBounceSoundType == LEBS_BRASS) {
    }

    // don't allow a fragment to make multiple bounce sounds,
    // or it gets too noisy as they settle
    le->leBounceSoundType = LEBS_NONE;
}

/*
================
CG_ReflectVelocity
================
*/
void CG_ReflectVelocity(localEntity_t* le, trace_t* trace) {
    vec3_t velocity;
    float dot;
    int hitTime;

    // reflect the velocity on the trace plane
    hitTime = cg.time - cg.frametime + cg.frametime * trace->fraction;
    BG_EvaluateTrajectoryDelta(&le->pos, hitTime, velocity);
    dot = DotProduct(velocity, trace->plane.normal);
    VectorMA(velocity, -2 * dot, trace->plane.normal, le->pos.trDelta);

    VectorScale(le->pos.trDelta, le->bounceFactor, le->pos.trDelta);

    VectorCopy(trace->endpos, le->pos.trBase);
    le->pos.trTime = cg.time;

    // check for stop, making sure that even on low FPS systems it doesn't bobble
    if (trace->allsolid ||
        (trace->plane.normal[2] > 0 &&
         (le->pos.trDelta[2] < 40 || le->pos.trDelta[2] < -cg.frametime * le->pos.trDelta[2]))) {
        le->pos.trType = TR_STATIONARY;
    } else {
    }
}

/*
================
CG_AddFragment
================
*/
void CG_AddFragment(localEntity_t* le) {
    vec3_t newOrigin;
    trace_t trace;

    if (le->pos.trType == TR_STATIONARY) {
        // sink into the ground if near the removal time
        int t;
        float oldZ;

        t = le->endTime - cg.time;
        if (t < SINK_TIME) {
            // we must use an explicit lighting origin, otherwise the
            // lighting would be lost as soon as the origin went
            // into the ground
            VectorCopy(le->refEntity.origin, le->refEntity.lightingOrigin);
            le->refEntity.renderfx |= RF_LIGHTING_ORIGIN;
            oldZ = le->refEntity.origin[2];
            le->refEntity.origin[2] -= 16 * (1.0 - (float)t / SINK_TIME);
            trap_R_AddRefEntityToScene(&le->refEntity);
            le->refEntity.origin[2] = oldZ;
        } else {
            trap_R_AddRefEntityToScene(&le->refEntity);
        }

        return;
    }

    // calculate new position
    BG_EvaluateTrajectory(&le->pos, cg.time, newOrigin);

    // trace a line from previous position to new position
    CG_Trace(&trace, le->refEntity.origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID);
    if (trace.fraction == 1.0) {
        // still in free fall
        VectorCopy(newOrigin, le->refEntity.origin);

        if (le->leFlags & LEF_TUMBLE) {
            vec3_t angles;

            BG_EvaluateTrajectory(&le->angles, cg.time, angles);
            AnglesToAxis(angles, le->refEntity.axis);
        }

        trap_R_AddRefEntityToScene(&le->refEntity);

        // [wolfcam] CG_AddFragment: the trail is keyed off leMarkType, not
        // leBounceSoundType, so CG_FragmentBounceMark clearing the mark on
        // impact also shuts the trail off.
        if (le->leMarkType == LEMT_ELECTROGIB) {
            CG_ElectroGibTrail(le);
        } else if (le->leMarkType == LEMT_ICE) {
            // [QL] 0x1001efd6
            CG_IceTrail(le);
        } else if (le->leBounceSoundType == LEBS_BLOOD) {
            CG_BloodTrail(le);
        }

        return;
    }

    // if it is in a nodrop zone, remove it
    // this keeps gibs from waiting at the bottom of pits of death
    // and floating levels
    if (CG_PointContents(trace.endpos, 0) & CONTENTS_NODROP) {
        CG_FreeLocalEntity(le);
        return;
    }

    // leave a mark
    CG_FragmentBounceMark(le, &trace);

    // do a bouncy sound
    CG_FragmentBounceSound(le, &trace);

    // reflect the velocity on the trace plane
    CG_ReflectVelocity(le, &trace);

    trap_R_AddRefEntityToScene(&le->refEntity);
}

/*
=====================================================================

TRIVIAL LOCAL ENTITIES

These only do simple scaling or modulation before passing to the renderer
=====================================================================
*/

/*
====================
CG_AddFadeRGB
====================
*/
void CG_AddFadeRGB(localEntity_t* le) {
    refEntity_t* re;
    float c;

    re = &le->refEntity;

    c = (le->endTime - cg.time) * le->lifeRate;
    c *= 0xff;

    re->shaderRGBA[0] = le->color[0] * c;
    re->shaderRGBA[1] = le->color[1] * c;
    re->shaderRGBA[2] = le->color[2] * c;
    re->shaderRGBA[3] = le->color[3] * c;

    trap_R_AddRefEntityToScene(re);
}

/*
==================
CG_SmokeNearView

[ioquakelive] Keeps a smoke puff (and a blood mist) out of the view's way. Quake 3 and retail
(cgamex86.dll 0x1001f4e0) free a puff once the view is within the radius it started with and
draw it until then; a puff grows 8 units past that radius (16 a mist), so near the end of its
life the view could sit inside one that was still drawn, and the screen went grey. wolfcamql
keeps the same test but, by default (cg_allowSpritePassThrough 1), does not free the puff: it
only goes undrawn while the view is inside it.

Here a puff fades out over the radius it is drawn at before the view reaches it, is not drawn
while the view is inside it, and is not freed, so it is still there once the view has gone
through. Scales the puff's alpha; returns qfalse when it is not to be drawn.
==================
*/
static qboolean CG_SmokeNearView(refEntity_t* re) {
    vec3_t delta;
    float len, near;

    VectorSubtract(re->origin, cg.refdef.vieworg, delta);
    len = VectorLength(delta);
    if (len <= re->radius) {
        return qfalse;
    }
    if (len < 2.0f * re->radius) {
        near = (len - re->radius) / re->radius;
        re->shaderRGBA[3] = (byte)(re->shaderRGBA[3] * near);
    }
    return qtrue;
}

/*
==================
CG_AddMoveScaleFade
==================
*/
static void CG_AddMoveScaleFade(localEntity_t* le) {
    refEntity_t* re;
    float c;

    re = &le->refEntity;

    if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime) {
        // fade / grow time
        c = 1.0 - (float)(le->fadeInTime - cg.time) / (le->fadeInTime - le->startTime);
    } else {
        // fade / grow time
        c = (le->endTime - cg.time) * le->lifeRate;
    }

    re->shaderRGBA[3] = 0xff * c * le->color[3];

    if (!(le->leFlags & LEF_PUFF_DONT_SCALE)) {
        re->radius = le->radius * (1.0 - c) + 8;
    }

    BG_EvaluateTrajectory(&le->pos, cg.time, re->origin);

    // [ioquakelive] out of the view's way, and kept (CG_SmokeNearView)
    if (!CG_SmokeNearView(re)) {
        return;
    }

    trap_R_AddRefEntityToScene(re);
}

/*
===================
CG_AddScaleFade

For rocket smokes that hang in place, fade out, and are
removed if the view passes through them.
There are often many of these, so it needs to be simple.
===================
*/
static void CG_AddScaleFade(localEntity_t* le) {
    refEntity_t* re;
    float c;

    re = &le->refEntity;

    // fade / grow time
    c = (le->endTime - cg.time) * le->lifeRate;

    re->shaderRGBA[3] = 0xff * c * le->color[3];
    re->radius = le->radius * (1.0 - c) + 8;

    // [ioquakelive] out of the view's way, and kept (CG_SmokeNearView)
    if (!CG_SmokeNearView(re)) {
        return;
    }

    trap_R_AddRefEntityToScene(re);
}

/*
=================
CG_AddFallScaleFade

This is just an optimized CG_AddMoveScaleFade
For blood mists that drift down, fade out, and are
removed if the view passes through them.
There are often 100+ of these, so it needs to be simple.
=================
*/
static void CG_AddFallScaleFade(localEntity_t* le) {
    refEntity_t* re;
    float c;

    re = &le->refEntity;

    // fade time
    c = (le->endTime - cg.time) * le->lifeRate;

    re->shaderRGBA[3] = 0xff * c * le->color[3];

    re->origin[2] = le->pos.trBase[2] - (1.0 - c) * le->pos.trDelta[2];

    // [wolfcam] This function is a hand-optimised copy of
    // CG_AddMoveScaleFade and never learned about LEF_PUFF_DONT_SCALE, so
    // wolfcam gave it a flag of its own. Without it a spark routed through
    // here would swell to 16 units and read as smoke.
    if (le->leFlags & LEF_PUFF_DONT_SCALE_NOT_KIDDING_MAN) {
        re->radius = le->radius;
    } else {
        re->radius = le->radius * (1.0 - c) + 16;
    }

    // [ioquakelive] out of the view's way, and kept (CG_SmokeNearView)
    if (!CG_SmokeNearView(re)) {
        return;
    }

    trap_R_AddRefEntityToScene(re);
}

/*
================
CG_AddExplosion
================
*/
static void CG_AddExplosion(localEntity_t* ex) {
    refEntity_t* ent;

    ent = &ex->refEntity;

    // add the entity
    trap_R_AddRefEntityToScene(ent);

    // add the dlight
    if (ex->light) {
        float light;

        light = (float)(cg.time - ex->startTime) / (ex->endTime - ex->startTime);
        if (light < 0.5) {
            light = 1.0;
        } else {
            light = 1.0 - (light - 0.5) * 2;
        }
        light = ex->light * light;
        CG_AddEffectLight(ent->origin, light, ex->lightColor[0], ex->lightColor[1], ex->lightColor[2]);
    }
}

/*
================
CG_AddSpriteExplosion
================
*/
static void CG_AddSpriteExplosion(localEntity_t* le) {
    refEntity_t re;
    float c;

    re = le->refEntity;

    c = (le->endTime - cg.time) / (float)(le->endTime - le->startTime);
    if (c > 1) {
        c = 1.0;  // can happen during connection problems
    }

    re.shaderRGBA[0] = 0xff;
    re.shaderRGBA[1] = 0xff;
    re.shaderRGBA[2] = 0xff;
    re.shaderRGBA[3] = 0xff * c * 0.33;

    re.reType = RT_SPRITE;
    re.radius = 42 * (1.0 - c) + 30;

    trap_R_AddRefEntityToScene(&re);

    // add the dlight
    if (le->light) {
        float light;

        light = (float)(cg.time - le->startTime) / (le->endTime - le->startTime);
        if (light < 0.5) {
            light = 1.0;
        } else {
            light = 1.0 - (light - 0.5) * 2;
        }
        light = le->light * light;
        CG_AddEffectLight(re.origin, light, le->lightColor[0], le->lightColor[1], le->lightColor[2]);
    }
}

/*
====================
CG_AddKamikaze
====================
*/
void CG_AddKamikaze(localEntity_t* le) {
    refEntity_t* re;
    refEntity_t shockwave;
    float c;
    vec3_t test, axis[3];
    int t;

    re = &le->refEntity;

    t = cg.time - le->startTime;
    VectorClear(test);
    AnglesToAxis(test, axis);

    if (t > KAMI_SHOCKWAVE_STARTTIME && t < KAMI_SHOCKWAVE_ENDTIME) {
        if (!(le->leFlags & LEF_SOUND1)) {
            //			trap_S_StartSound (re->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.kamikazeExplodeSound );
            trap_S_StartLocalSound(cgs.media.kamikazeExplodeSound, CHAN_AUTO);
            le->leFlags |= LEF_SOUND1;
        }
        // 1st kamikaze shockwave
        memset(&shockwave, 0, sizeof(shockwave));
        shockwave.hModel = cgs.media.kamikazeShockWave;
        shockwave.reType = RT_MODEL;
        shockwave.shaderTime = re->shaderTime;
        VectorCopy(re->origin, shockwave.origin);

        c = (float)(t - KAMI_SHOCKWAVE_STARTTIME) / (float)(KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME);
        VectorScale(axis[0], c * KAMI_SHOCKWAVE_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[0]);
        VectorScale(axis[1], c * KAMI_SHOCKWAVE_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[1]);
        VectorScale(axis[2], c * KAMI_SHOCKWAVE_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[2]);
        shockwave.nonNormalizedAxes = qtrue;

        if (t > KAMI_SHOCKWAVEFADE_STARTTIME) {
            c = (float)(t - KAMI_SHOCKWAVEFADE_STARTTIME) / (float)(KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVEFADE_STARTTIME);
        } else {
            c = 0;
        }
        c *= 0xff;
        shockwave.shaderRGBA[0] = 0xff - c;
        shockwave.shaderRGBA[1] = 0xff - c;
        shockwave.shaderRGBA[2] = 0xff - c;
        shockwave.shaderRGBA[3] = 0xff - c;

        trap_R_AddRefEntityToScene(&shockwave);
    }

    if (t > KAMI_EXPLODE_STARTTIME && t < KAMI_IMPLODE_ENDTIME) {
        // explosion and implosion
        c = (le->endTime - cg.time) * le->lifeRate;
        c *= 0xff;
        re->shaderRGBA[0] = le->color[0] * c;
        re->shaderRGBA[1] = le->color[1] * c;
        re->shaderRGBA[2] = le->color[2] * c;
        re->shaderRGBA[3] = le->color[3] * c;

        if (t < KAMI_IMPLODE_STARTTIME) {
            c = (float)(t - KAMI_EXPLODE_STARTTIME) / (float)(KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME);
        } else {
            if (!(le->leFlags & LEF_SOUND2)) {
                //				trap_S_StartSound (re->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.kamikazeImplodeSound );
                trap_S_StartLocalSound(cgs.media.kamikazeImplodeSound, CHAN_AUTO);
                le->leFlags |= LEF_SOUND2;
            }
            c = (float)(KAMI_IMPLODE_ENDTIME - t) / (float)(KAMI_IMPLODE_ENDTIME - KAMI_IMPLODE_STARTTIME);
        }
        VectorScale(axis[0], c * KAMI_BOOMSPHERE_MAXRADIUS / KAMI_BOOMSPHEREMODEL_RADIUS, re->axis[0]);
        VectorScale(axis[1], c * KAMI_BOOMSPHERE_MAXRADIUS / KAMI_BOOMSPHEREMODEL_RADIUS, re->axis[1]);
        VectorScale(axis[2], c * KAMI_BOOMSPHERE_MAXRADIUS / KAMI_BOOMSPHEREMODEL_RADIUS, re->axis[2]);
        re->nonNormalizedAxes = qtrue;

        trap_R_AddRefEntityToScene(re);
        // add the dlight
        CG_AddEffectLight(re->origin, c * 1000.0, 1.0, 1.0, c);
    }

    if (t > KAMI_SHOCKWAVE2_STARTTIME && t < KAMI_SHOCKWAVE2_ENDTIME) {
        // 2nd kamikaze shockwave
        if (le->angles.trBase[0] == 0 &&
            le->angles.trBase[1] == 0 &&
            le->angles.trBase[2] == 0) {
            le->angles.trBase[0] = random() * 360;
            le->angles.trBase[1] = random() * 360;
            le->angles.trBase[2] = random() * 360;
        }
        memset(&shockwave, 0, sizeof(shockwave));
        shockwave.hModel = cgs.media.kamikazeShockWave;
        shockwave.reType = RT_MODEL;
        shockwave.shaderTime = re->shaderTime;
        VectorCopy(re->origin, shockwave.origin);

        test[0] = le->angles.trBase[0];
        test[1] = le->angles.trBase[1];
        test[2] = le->angles.trBase[2];
        AnglesToAxis(test, axis);

        c = (float)(t - KAMI_SHOCKWAVE2_STARTTIME) / (float)(KAMI_SHOCKWAVE2_ENDTIME - KAMI_SHOCKWAVE2_STARTTIME);
        VectorScale(axis[0], c * KAMI_SHOCKWAVE2_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[0]);
        VectorScale(axis[1], c * KAMI_SHOCKWAVE2_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[1]);
        VectorScale(axis[2], c * KAMI_SHOCKWAVE2_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[2]);
        shockwave.nonNormalizedAxes = qtrue;

        if (t > KAMI_SHOCKWAVE2FADE_STARTTIME) {
            c = (float)(t - KAMI_SHOCKWAVE2FADE_STARTTIME) / (float)(KAMI_SHOCKWAVE2_ENDTIME - KAMI_SHOCKWAVE2FADE_STARTTIME);
        } else {
            c = 0;
        }
        c *= 0xff;
        shockwave.shaderRGBA[0] = 0xff - c;
        shockwave.shaderRGBA[1] = 0xff - c;
        shockwave.shaderRGBA[2] = 0xff - c;
        shockwave.shaderRGBA[3] = 0xff - c;

        trap_R_AddRefEntityToScene(&shockwave);
    }
}

/*
===================
CG_AddInvulnerabilityImpact
===================
*/
void CG_AddInvulnerabilityImpact(localEntity_t* le) {
    trap_R_AddRefEntityToScene(&le->refEntity);
}

/*
===================
CG_AddInvulnerabilityJuiced
===================
*/
void CG_AddInvulnerabilityJuiced(localEntity_t* le) {
    int t;

    t = cg.time - le->startTime;
    if (t > 3000) {
        le->refEntity.axis[0][0] = (float)1.0 + 0.3 * (t - 3000) / 2000;
        le->refEntity.axis[1][1] = (float)1.0 + 0.3 * (t - 3000) / 2000;
        le->refEntity.axis[2][2] = (float)0.7 + 0.3 * (2000 - (t - 3000)) / 2000;
    }
    if (t > 5000) {
        le->endTime = 0;
        CG_GibPlayer(le->refEntity.origin, NULL);
    } else {
        trap_R_AddRefEntityToScene(&le->refEntity);
    }
}

/*
===================
CG_AddRefEntity
===================
*/
void CG_AddRefEntity(localEntity_t* le) {
    if (le->endTime < cg.time) {
        CG_FreeLocalEntity(le);
        return;
    }
    trap_R_AddRefEntityToScene(&le->refEntity);
}

/*
===================
CG_AddScorePlum
===================
*/
#define NUMBER_SIZE 8

void CG_AddScorePlum(localEntity_t* le) {
    refEntity_t* re;
    vec3_t origin, delta, dir, vec, up = {0, 0, 1};
    float c, len;
    int i, score, digits[10], numdigits, negative;

    re = &le->refEntity;

    c = (le->endTime - cg.time) * le->lifeRate;

    score = le->radius;
    if (score < 0) {
        re->shaderRGBA[0] = 0xff;
        re->shaderRGBA[1] = 0x11;
        re->shaderRGBA[2] = 0x11;
    } else {
        re->shaderRGBA[0] = 0xff;
        re->shaderRGBA[1] = 0xff;
        re->shaderRGBA[2] = 0xff;
        if (score >= 50) {
            re->shaderRGBA[1] = 0;
        } else if (score >= 20) {
            re->shaderRGBA[0] = re->shaderRGBA[1] = 0;
        } else if (score >= 10) {
            re->shaderRGBA[2] = 0;
        } else if (score >= 2) {
            re->shaderRGBA[0] = re->shaderRGBA[2] = 0;
        }
    }
    if (c < 0.25)
        re->shaderRGBA[3] = 0xff * 4 * c;
    else
        re->shaderRGBA[3] = 0xff;

    re->radius = NUMBER_SIZE / 2;

    VectorCopy(le->pos.trBase, origin);
    origin[2] += 110 - c * 100;

    VectorSubtract(cg.refdef.vieworg, origin, dir);
    CrossProduct(dir, up, vec);
    VectorNormalize(vec);

    VectorMA(origin, -10 + 20 * sin(c * 2 * M_PI), vec, origin);

    // if the view would be "inside" the sprite, kill the sprite
    // so it doesn't add too much overdraw
    VectorSubtract(origin, cg.refdef.vieworg, delta);
    len = VectorLength(delta);
    if (len < 20) {
        CG_FreeLocalEntity(le);
        return;
    }

    negative = qfalse;
    if (score < 0) {
        negative = qtrue;
        score = -score;
    }

    for (numdigits = 0; !(numdigits && !score); numdigits++) {
        digits[numdigits] = score % 10;
        score = score / 10;
    }

    if (negative) {
        digits[numdigits] = 10;
        numdigits++;
    }

    for (i = 0; i < numdigits; i++) {
        VectorMA(origin, (float)(((float)numdigits / 2) - i) * NUMBER_SIZE, vec, re->origin);
        re->customShader = cgs.media.numberShaders[digits[numdigits - 1 - i]];
        trap_R_AddRefEntityToScene(re);
    }
}

// [QL] CG_AddDamagePlum removed: damage numbers now use the shared floating-effect
// pool (CG_DamagePlum -> CG_AllocFloatingEffect, rendered by CG_DrawFloatingEffects),
// matching the binary. The old LE_DAMAGEPLUM number-sprite path is gone.

//==============================================================================

/*
===================
CG_AddLocalEntities

===================
*/
void CG_AddLocalEntities(void) {
    localEntity_t *le, *next;

    // walk the list backwards, so any new local entities generated
    // (trails, marks, etc) will be present this frame
    le = cg_activeLocalEntities.prev;
    for (; le != &cg_activeLocalEntities; le = next) {
        // grab next now, so if the local entity is freed we
        // still have it
        next = le->prev;

        if (cg.time >= le->endTime) {
            CG_FreeLocalEntity(le);
            continue;
        }
        switch (le->leType) {
            default:
                CG_Error("Bad leType: %i", le->leType);
                break;

            case LE_MARK:
                break;

            case LE_SPRITE_EXPLOSION:
                CG_AddSpriteExplosion(le);
                break;

            case LE_EXPLOSION:
                CG_AddExplosion(le);
                break;

            case LE_FRAGMENT:  // gibs and brass
                CG_AddFragment(le);
                break;

            case LE_MOVE_SCALE_FADE:  // water bubbles
                CG_AddMoveScaleFade(le);
                break;

            case LE_FADE_RGB:  // teleporters, railtrails
                CG_AddFadeRGB(le);
                break;

            case LE_FALL_SCALE_FADE:  // gib blood trails
                CG_AddFallScaleFade(le);
                break;

            case LE_SCALE_FADE:  // rocket trails
                CG_AddScaleFade(le);
                break;

            case LE_SCOREPLUM:
                CG_AddScorePlum(le);
                break;

            case LE_KAMIKAZE:
                CG_AddKamikaze(le);
                break;

            case LE_INVULIMPACT:
                CG_AddInvulnerabilityImpact(le);
                break;

            case LE_INVULJUICED:
                CG_AddInvulnerabilityJuiced(le);
                break;

            case LE_SHOWREFENTITY:
                CG_AddRefEntity(le);
                break;
        }
    }
}
