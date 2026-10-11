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

#define MAX_CLIP_PLANES 5
qboolean PM_SlideMove(qboolean gravity) {
    int bumpcount, numbumps;
    vec3_t dir;
    float d;
    int numplanes;
    vec3_t planes[MAX_CLIP_PLANES];
    vec3_t primal_velocity;
    vec3_t clipVelocity;
    int i, j, k;
    trace_t trace;
    vec3_t end;
    float time_left;
    float into;
    vec3_t endVelocity;
    vec3_t endClipVelocity;

    numbumps = 4;

    VectorCopy(pm->ps->velocity, primal_velocity);

    if (gravity) {
        VectorCopy(pm->ps->velocity, endVelocity);
        endVelocity[2] -= pm->ps->gravity * pml.frametime;
        pm->ps->velocity[2] = (pm->ps->velocity[2] + endVelocity[2]) * 0.5;
        primal_velocity[2] = endVelocity[2];
        if (pml.groundPlane) {
            PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                            pm->ps->velocity, OVERCLIP);
        }
    }

    time_left = pml.frametime;

    if (pml.groundPlane) {
        numplanes = 1;
        VectorCopy(pml.groundTrace.plane.normal, planes[0]);
    } else {
        numplanes = 0;
    }

    VectorNormalize2(pm->ps->velocity, planes[numplanes]);
    numplanes++;

    for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
        VectorMA(pm->ps->origin, time_left, pm->ps->velocity, end);

        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask);

        if (trace.allsolid) {
            pm->ps->velocity[2] = 0;
            return qtrue;
        }

        if (trace.fraction > 0) {
            VectorCopy(trace.endpos, pm->ps->origin);
        }

        if (trace.fraction == 1) {
            break;
        }

        PM_AddTouchEnt(trace.entityNum);

        time_left -= time_left * trace.fraction;

        if (numplanes >= MAX_CLIP_PLANES) {
            VectorClear(pm->ps->velocity);
            return qtrue;
        }

        for (i = 0; i < numplanes; i++) {
            if (DotProduct(trace.plane.normal, planes[i]) > 0.99) {
                VectorAdd(trace.plane.normal, pm->ps->velocity, pm->ps->velocity);
                break;
            }
        }
        if (i < numplanes) {
            continue;
        }
        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

        for (i = 0; i < numplanes; i++) {
            into = DotProduct(pm->ps->velocity, planes[i]);
            if (into >= 0.1) {
                continue;
            }

            if (-into > pml.impactSpeed) {
                pml.impactSpeed = -into;
            }

            PM_ClipVelocity(pm->ps->velocity, planes[i], clipVelocity, OVERCLIP);

            if (gravity) {
                PM_ClipVelocity(endVelocity, planes[i], endClipVelocity, OVERCLIP);
            }

            for (j = 0; j < numplanes; j++) {
                if (j == i) {
                    continue;
                }
                if (DotProduct(clipVelocity, planes[j]) >= 0.1) {
                    continue;
                }

                PM_ClipVelocity(clipVelocity, planes[j], clipVelocity, OVERCLIP);

                if (gravity) {
                    PM_ClipVelocity(endClipVelocity, planes[j], endClipVelocity, OVERCLIP);
                }

                if (DotProduct(clipVelocity, planes[i]) >= 0) {
                    continue;
                }

                CrossProduct(planes[i], planes[j], dir);
                VectorNormalize(dir);
                d = DotProduct(dir, pm->ps->velocity);
                VectorScale(dir, d, clipVelocity);

                if (gravity) {
                    CrossProduct(planes[i], planes[j], dir);
                    VectorNormalize(dir);
                    d = DotProduct(dir, endVelocity);
                    VectorScale(dir, d, endClipVelocity);
                }

                for (k = 0; k < numplanes; k++) {
                    if (k == i || k == j) {
                        continue;
                    }
                    if (DotProduct(clipVelocity, planes[k]) >= 0.1) {
                        continue;
                    }

                    VectorClear(pm->ps->velocity);
                    return qtrue;
                }
            }

            VectorCopy(clipVelocity, pm->ps->velocity);

            if (gravity) {
                VectorCopy(endClipVelocity, endVelocity);
            }

            break;
        }
    }

    if (gravity) {
        VectorCopy(endVelocity, pm->ps->velocity);
    }

    if (pm->ps->pm_time) {
        VectorCopy(primal_velocity, pm->ps->velocity);
    }

    return (bumpcount != 0);
}

static qboolean PM_WouldCrouchStepJump(void) {
    if (!(pm->ps->pm_flags & PMF_DUCKED)) {
        return qfalse;
    }
    if (pml.groundPlane) {
        return qfalse;
    }
    if (pm->ps->velocity[2] < 0.0f) {
        return qfalse;
    }
    if ((float)(pm->cmd.serverTime - pm->ps->jumpTime) < pm_jumpVelocityTimeThreshold) {
        return qfalse;
    }
    return qtrue;
}

void PM_StepSlideMove(qboolean gravity) {
    vec3_t start_o, start_v;
    vec3_t pred;
    trace_t trace;
    vec3_t up, down;
    float stepSize;
    float actualStepHeight = pm_stepHeight;

    VectorCopy(pm->ps->origin, start_o);
    VectorCopy(pm->ps->velocity, start_v);

    if (PM_SlideMove(gravity) == 0) {
        return;
    }

    VectorMA(start_o, pml.frametime, start_v, pred);

    if (!pm_airSteps) {
        vec3_t predDown;

        pm->trace(&trace, start_o, pm->mins, pm->maxs, pred, pm->ps->clientNum, pm->tracemask);

        VectorCopy(trace.endpos, pred);

        VectorCopy(pred, predDown);
        predDown[2] -= actualStepHeight;
        pm->trace(&trace, pred, pm->mins, pm->maxs, predDown, pm->ps->clientNum, pm->tracemask);

        if (start_v[2] > 0 && (trace.fraction == 1.0 ||
                               trace.plane.normal[2] < 0.7)) {
            return;
        }
    }

    VectorCopy(start_o, up);
    up[2] += actualStepHeight;

    pm->trace(&trace, start_o, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        if (pm->debugLevel) {
            Com_Printf("%i:bend can't step\n", c_pmove);
        }
        return;
    }

    stepSize = trace.endpos[2] - start_o[2];
    VectorCopy(trace.endpos, pm->ps->origin);
    VectorCopy(start_v, pm->ps->velocity);

    PM_SlideMove(gravity);

    VectorCopy(pm->ps->origin, down);
    down[2] -= stepSize;
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
    if (!trace.allsolid) {
        VectorCopy(trace.endpos, pm->ps->origin);
    }

    if (trace.fraction < 1.0) {
        float dot = DotProduct(pm->ps->velocity, trace.plane.normal);
        if (dot < 0.0f || (dot >= 0.0f && dot < 0.001f)) {
            PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
        }
    }

    pm->trace(&trace, start_o, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);

    if (trace.fraction < 1.0) {
        float delta = pm->ps->origin[2] - start_o[2];

        if (delta > 2) {
            pm->stepHeight = delta;
            pm->stepTime = pm->cmd.serverTime;
        }

        if (!pml.groundPlane && delta > 0 && start_v[2] > 0) {
            float dampen = 1.0f - pm_airStepFriction;
            pm->ps->velocity[0] *= dampen;
            pm->ps->velocity[1] *= dampen;
        }

        if (pm_stepJump && pm->ps->pm_type == PM_NORMAL
            && delta > 0 && pm->waterlevel < 2) {
            if (PM_WouldJump() || (pm_crouchStepJump && PM_WouldCrouchStepJump())) {
                vec3_t sjUp, sjDown;
                trace_t sjTrace;

                VectorCopy(pred, sjUp);
                sjUp[2] += actualStepHeight;
                VectorCopy(pred, sjDown);
                sjDown[2] -= actualStepHeight;
                pm->trace(&sjTrace, sjUp, pm->mins, pm->maxs, sjDown,
                          pm->ps->clientNum, pm->tracemask);

                if (!sjTrace.startsolid && !sjTrace.allsolid
                    && sjTrace.plane.normal[2] >= 0.7) {
                    if (PM_WouldJump()) {
                        pml.stepJumpFlag = 1;
                        PM_DoJump();
                        pml.stepJumpFlag = 0;
                    } else if (pm_crouchStepJump && PM_WouldCrouchStepJump()) {
                        vec3_t sjMins, sjMaxs, crouchDown;
                        trace_t crouchTrace;

                        sjMins[0] = pm->mins[0] + 1;
                        sjMins[1] = pm->mins[1] + 1;
                        sjMins[2] = pm->mins[2];
                        sjMaxs[0] = pm->maxs[0] - 1;
                        sjMaxs[1] = pm->maxs[1] - 1;
                        sjMaxs[2] = pm->maxs[2];
                        VectorCopy(pm->ps->origin, crouchDown);
                        crouchDown[2] -= 64;
                        pm->trace(&crouchTrace, pm->ps->origin, sjMins, sjMaxs,
                                  crouchDown, pm->ps->clientNum, pm->tracemask);
                        if (crouchTrace.fraction == 1.0) {
                            pml.crouchStepJumpFlag = 1;
                            PM_DoJump();
                            pml.crouchStepJumpFlag = 0;
                        }
                    }
                }
            }
        }

        if (pm->debugLevel) {
            Com_Printf("%i:stepped %f\n", c_pmove, (double)delta);
        }
    }
}
