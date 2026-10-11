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

void CG_PositionEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName) {
    int i;
    orientation_t lerped;

    trap_R_LerpTag(&lerped, parentModel, parent->oldframe, parent->frame,
                   1.0 - parent->backlerp, tagName);

    VectorCopy(parent->origin, entity->origin);
    for (i = 0; i < 3; i++) {
        VectorMA(entity->origin, lerped.origin[i], parent->axis[i], entity->origin);
    }

    MatrixMultiply(lerped.axis, ((refEntity_t*)parent)->axis, entity->axis);
    entity->backlerp = parent->backlerp;
}

void CG_PositionRotatedEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName) {
    int i;
    orientation_t lerped;
    vec3_t tempAxis[3];

    trap_R_LerpTag(&lerped, parentModel, parent->oldframe, parent->frame,
                   1.0 - parent->backlerp, tagName);

    VectorCopy(parent->origin, entity->origin);
    for (i = 0; i < 3; i++) {
        VectorMA(entity->origin, lerped.origin[i], parent->axis[i], entity->origin);
    }

    MatrixMultiply(entity->axis, lerped.axis, tempAxis);
    MatrixMultiply(tempAxis, ((refEntity_t*)parent)->axis, entity->axis);
}

void CG_SetEntitySoundPosition(centity_t* cent) {
    if (cent->currentState.solid == SOLID_BMODEL) {
        vec3_t origin;
        float* v;

        v = cgs.inlineModelMidpoints[cent->currentState.modelindex];
        VectorAdd(cent->lerpOrigin, v, origin);
        trap_S_UpdateEntityPosition(cent->currentState.number, origin);
    } else {
        trap_S_UpdateEntityPosition(cent->currentState.number, cent->lerpOrigin);
    }
}

void CG_AddEffectLight(const vec3_t org, float intensity, float r, float g, float b) {
    if (r_dynamicLight.integer == 2) {
        return;
    }
    trap_R_AddLightToScene(org, intensity, r, g, b);
}

static void CG_EntityEffects(centity_t* cent) {
    CG_SetEntitySoundPosition(cent);

    if (cent->currentState.loopSound && cg_ambient.integer) {
        if (cent->currentState.eType != ET_SPEAKER) {
            trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin,
                                   cgs.gameSounds[cent->currentState.loopSound]);
        } else {
            trap_S_AddRealLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin,
                                       cgs.gameSounds[cent->currentState.loopSound]);
        }
    }

    if (cent->currentState.constantLight) {
        int cl;
        float i, r, g, b;

        cl = cent->currentState.constantLight;
        r = (float)(cl & 0xFF) / 255.0;
        g = (float)((cl >> 8) & 0xFF) / 255.0;
        b = (float)((cl >> 16) & 0xFF) / 255.0;
        i = (float)((cl >> 24) & 0xFF) * 4.0;
        CG_AddEffectLight(cent->lerpOrigin, i, r, g, b);
    }
}

static void CG_General(centity_t* cent) {
    refEntity_t ent;
    entityState_t* s1;
    qboolean hideFlag = qfalse;

    s1 = &cent->currentState;

    if (!s1->modelindex) {
        return;
    }

    memset(&ent, 0, sizeof(ent));

    ent.frame = s1->frame;
    ent.oldframe = ent.frame;
    ent.backlerp = 0;

    VectorCopy(cent->lerpOrigin, ent.origin);
    VectorCopy(cent->lerpOrigin, ent.oldorigin);

    ent.hModel = cgs.gameModels[s1->modelindex];

    if (s1->number == cg.snap->ps.clientNum) {
        ent.renderfx |= RF_THIRD_PERSON;
    }

    AnglesToAxis(cent->lerpAngles, ent.axis);

    if (cgs.gametype == GT_RACE) {
        qboolean following = !(cg.snap->ps.eFlags & EF_SPECTATOR) || (cg.snap->ps.pm_flags & PMF_FOLLOW);

        if (s1->number == cg.race.nextCheckpointEnt ||
            (ent.hModel == cgs.media.raceFlagG && cg.race.nextCheckpointEnt == -1)) {
            floatingEffect_t *fx = CG_AllocFloatingEffect();

            if (s1->number == cg.race.nextCheckpointEnt) {
                ent.hModel = cgs.media.raceFlagF;
                if (!s1->modelindex2) {
                    ent.hModel = following ? cgs.media.raceFlagB : cgs.media.raceFlagD;
                }
            }
            if (fx) {
                fx->type = 0;
                fx->owner = cent;
                VectorCopy(cent->lerpOrigin, fx->origin);
                fx->zOffset = 64.0f;
                fx->worldSize = cg_poiMaxWidth.value;
                fx->minPixels = cg_poiMinWidth.value;
                if (ent.hModel == cgs.media.raceFlagG) {
                    fx->color[0] = 0.0f;
                    fx->color[1] = 1.0f;
                    fx->color[2] = 0.0f;
                    fx->shader = cgs.media.raceMarkerStart;
                } else if (!s1->modelindex2) {
                    fx->color[0] = 0.2f;
                    fx->color[1] = 0.6f;
                    fx->color[2] = 1.0f;
                    fx->shader = cgs.media.raceMarkerCheckpoint;
                } else {
                    fx->color[0] = 1.0f;
                    fx->color[1] = 0.2f;
                    fx->color[2] = 0.2f;
                    fx->shader = cgs.media.raceMarkerFinish;
                }
                fx->color[3] = 1.0f;
            }
        } else if (s1->number != cg.race.nextNextCheckpointEnt &&
                   (ent.hModel == cgs.media.raceFlagD || ent.hModel == cgs.media.raceFlagF) &&
                   cg.race.active == 1) {
            hideFlag = qtrue;
        }

        if (ent.hModel == cgs.media.raceFlagD || ent.hModel == cgs.media.raceFlagB ||
            ent.hModel == cgs.media.raceFlagF || ent.hModel == cgs.media.raceFlagG) {
            if (cg_itemFx.integer & 1) {
                ent.origin[2] += sin((double)(float)(cg.time & 0x3ff) * 6.2831854820251465 * 0.0009765625) * 6.0 + 4.0;
            }
            if (cg_itemFx.integer & 2) {
                AxisCopy(cg.autoAxisFast, ent.axis);
            } else {
                AxisCopy(cg.autoAxis, ent.axis);
            }

            if (!(s1->generic1 & 1)) {
                refEntity_t pedestal;

                memset(&pedestal, 0, sizeof(pedestal));
                VectorCopy(cent->lerpOrigin, pedestal.lightingOrigin);
                VectorCopy(cent->lerpOrigin, pedestal.origin);
                pedestal.origin[2] -= 16.0f;
                AxisClear(pedestal.axis);
                pedestal.hModel = cgs.media.domPointModel;
                pedestal.customSkin = cgs.media.domSkinRed;
                if (ent.hModel != cgs.media.raceFlagF) {
                    pedestal.customSkin = cgs.media.domSkinNeutral;
                    if (following && ent.hModel == cgs.media.raceFlagB) {
                        pedestal.customSkin = cgs.media.domSkinBlue;
                    }
                }
                trap_R_AddRefEntityToScene(&pedestal);
            }
        }
    }

    if (cgs.gametype == GT_DOMINATION && s1->modelindex >= 1 && s1->modelindex <= 3) {
        int team = s1->modelindex2;
        float flicker = s1->weapon ? (float)(rand() & 0x1f) : (float)(rand() & 3);
        refEntity_t pedestal;
        vec3_t lightOrigin;
        vec3_t up = {0, 0, 1};
        vec3_t color, markColor;

        VectorCopy(cent->lerpOrigin, lightOrigin);
        lightOrigin[2] += 16.0f;

        CG_DomPointMarker(cent);

        memset(&pedestal, 0, sizeof(pedestal));
        VectorCopy(cent->lerpOrigin, pedestal.lightingOrigin);
        VectorCopy(cent->lerpOrigin, pedestal.origin);
        pedestal.origin[2] -= 16.0f;
        AxisClear(pedestal.axis);
        pedestal.hModel = cgs.media.domPointModel;
        if (team == TEAM_RED) {
            pedestal.customSkin = cgs.media.domSkinRed;
        } else if (team == TEAM_BLUE) {
            pedestal.customSkin = cgs.media.domSkinBlue;
        } else {
            pedestal.customSkin = cgs.media.domSkinNeutral;
        }
        trap_R_AddRefEntityToScene(&pedestal);

        if (cg_itemFx.integer & 1) {
            ent.origin[2] += sin((double)(float)(cg.time & 0x3ff) * 6.2831854820251465 * 0.0009765625) * 6.0 + 4.0;
        }
        if (cg_itemFx.integer & 2) {
            AxisCopy(cg.autoAxisFast, ent.axis);
        } else {
            AxisCopy(cg.autoAxis, ent.axis);
        }

        if (team == TEAM_RED) {
            ent.shaderRGBA[0] = 0xff;
            ent.shaderRGBA[1] = 0;
            ent.shaderRGBA[2] = 0;
            VectorSet(color, 1.0f, 0.2f, 0.2f);
            VectorSet(markColor, 1.0f, 0.2f, 0.2f);
        } else if (team == TEAM_BLUE) {
            ent.shaderRGBA[0] = 0;
            ent.shaderRGBA[1] = 0x80;
            ent.shaderRGBA[2] = 0xff;
            VectorSet(color, 0.2f, 0.6f, 1.0f);
            VectorSet(markColor, 0.2f, 0.6f, 1.0f);
        } else {
            ent.shaderRGBA[0] = team ? 0 : 0xff;
            ent.shaderRGBA[1] = 0xff;
            ent.shaderRGBA[2] = team ? 0 : 0xff;
            VectorSet(color, 1.0f, 1.0f, 1.0f);
            VectorSet(markColor, 0.8f, 0.8f, 0.8f);
        }
        ent.shaderRGBA[3] = 0xff;

        trap_R_AddLightToScene(lightOrigin, (float)((int)flicker + s1->groundEntityNum * 2),
                               color[0], color[1], color[2]);
        lightOrigin[2] -= 15.0f;
        CG_ImpactMark(cgs.media.domShadowShader, lightOrigin, up, 0, markColor[0], markColor[1],
                      markColor[2], 1.0f, qfalse, (float)s1->groundEntityNum, qtrue);
    }

    if (!hideFlag) {
        trap_R_AddRefEntityToScene(&ent);
    }
}

static void CG_Speaker(centity_t* cent) {
    if (!cent->currentState.clientNum) {
        return;
    }

    if (cg.time < cent->miscTime) {
        return;
    }

    trap_S_StartSound(NULL, cent->currentState.number, CHAN_ITEM, cgs.gameSounds[cent->currentState.eventParm]);

    cent->miscTime = cg.time + cent->currentState.frame * 100 + cent->currentState.clientNum * 100 * crandom();
}

#define ITEM_TIMER_RADIUS 16.0f

#define ITEM_TIMER_ROTATION(angle) (180.0f - (float)(angle))

static void CG_ItemTimerSprite(const vec3_t origin, qhandle_t shader, float rotation,
                               const vec3_t color, float alpha) {
    refEntity_t re;

    if (!shader) {
        return;
    }
    memset(&re, 0, sizeof(re));
    re.reType = RT_SPRITE;
    VectorCopy(origin, re.origin);
    re.radius = ITEM_TIMER_RADIUS;
    re.rotation = rotation;
    re.customShader = shader;
    re.shaderRGBA[0] = (byte)(color[0] * 255.0f);
    re.shaderRGBA[1] = (byte)(color[1] * 255.0f);
    re.shaderRGBA[2] = (byte)(color[2] * 255.0f);
    re.shaderRGBA[3] = (byte)(alpha * 255.0f);
    trap_R_AddRefEntityToScene(&re);
}

static void CG_ItemTimerFace(const gitem_t* item, qboolean teamItem, qhandle_t* shader,
                             vec3_t color) {
    if (teamItem) {
        *shader = cgs.media.timerIcons[TIMER_ICON_UNKNOWN];
        VectorSet(color, 1.0f, 1.0f, 1.0f);
        return;
    }

    if (item->giType == IT_HEALTH && item->giTag == 1) {
        *shader = cgs.media.timerIcons[TIMER_ICON_HEALTH];
        VectorSet(color, 0.375f, 0.75f, 1.0f);
        return;
    }

    if (item->giType == IT_POWERUP) {
        switch (item->giTag) {
        case PW_QUAD:
            *shader = cgs.media.timerIcons[TIMER_ICON_QUAD];
            VectorSet(color, 0.0f, 0.886f, 0.94f);
            return;
        case PW_BATTLESUIT:
            *shader = cgs.media.timerIcons[TIMER_ICON_BS];
            VectorSet(color, 1.0f, 0.65f, 0.0f);
            return;
        case PW_HASTE:
            *shader = cgs.media.timerIcons[TIMER_ICON_HASTE];
            VectorSet(color, 0.714f, 0.88f, 0.0f);
            return;
        case PW_INVIS:
            *shader = cgs.media.timerIcons[TIMER_ICON_INVIS];
            VectorSet(color, 0.0f, 0.0f, 1.0f);
            return;
        case PW_REGEN:
            *shader = cgs.media.timerIcons[TIMER_ICON_REGEN];
            VectorSet(color, 1.0f, 0.0f, 0.0f);
            return;
        default:
            break;
        }
    }

    if (item->giType == IT_HOLDABLE && item->giTag == HI_MEDKIT) {
        *shader = cgs.media.timerIcons[TIMER_ICON_MEDKIT];
        VectorSet(color, 1.0f, 0.0f, 0.0f);
        return;
    }

    *shader = cgs.media.timerIcons[TIMER_ICON_ARMOR];
    switch (item->giTag) {
    case 1:
        VectorSet(color, 1.0f, 0.0f, 0.0f);
        break;
    case 2:
        VectorSet(color, 1.0f, 1.0f, 0.0f);
        break;
    case 3:
        VectorSet(color, 0.4f, 0.6875f, 0.285f);
        break;
    default:
        VectorSet(color, 1.0f, 1.0f, 1.0f);
        break;
    }
}

static void CG_DrawItemTimer(centity_t* cent, const gitem_t* item) {
    entityState_t* es = &cent->currentState;
    qhandle_t icon, slice, current;
    vec3_t origin, delta, color;
    float alpha, dist;
    int total, lit, art, angle, i;

    if (!cg_itemTimers.integer || !item->itemTimer || cg.time >= es->time || es->time2 <= 0) {
        return;
    }

    total = es->time2 / 5000;
    if (total < 1) {
        return;
    }

    CG_ItemTimerFace(item, (qboolean)(es->generic1 != 0), &icon, color);

    VectorCopy(es->pos.trBase, origin);
    origin[2] += 8.0f;
    VectorSubtract(origin, cg.refdef.vieworg, delta);
    dist = VectorLength(delta);
    if (dist <= 256.0f) {
        alpha = 1.0f;
    } else if (dist < 768.0f) {
        alpha = (768.0f - dist) / 512.0f;
    } else {
        return;
    }

    CG_ItemTimerSprite(origin, icon, ITEM_TIMER_ROTATION(180), color, alpha);

    lit = (es->time2 - (es->time - cg.time)) / 5000 + 1;

    art = (total <= 5) ? 0 : (total <= 7) ? 1 : (total <= 12) ? 2 : 3;
    slice = cgs.media.timerSlices[art][0];
    current = cgs.media.timerSlices[art][1];

    angle = -(180 / total);
    for (i = 1; i <= lit; i++) {
        angle += 360 / total;
        CG_ItemTimerSprite(origin, (i == lit) ? current : slice, ITEM_TIMER_ROTATION(angle),
                           colorWhite, alpha);
    }
}

static qhandle_t CG_PowerupPoiShader(int giTag, qboolean volviendo) {
    switch (giTag) {
        case PW_QUAD:
        case PW_BATTLESUIT:
        case PW_HASTE:
        case PW_INVIS:
        case PW_REGEN:
            break;
        default:
            return 0;
    }
    if (volviendo) {
        return cgs.media.powerupIncomingShader;
    }
    switch (giTag) {
        case PW_QUAD: return cgs.media.powerupPoi[0];
        case PW_BATTLESUIT: return cgs.media.powerupPoi[1];
        case PW_HASTE: return cgs.media.powerupPoi[2];
        case PW_INVIS: return cgs.media.powerupPoi[3];
        default: return cgs.media.powerupPoi[4];
    }
}

static void CG_DrawPowerupPoi(centity_t* cent, const gitem_t* item) {
    entityState_t* es = &cent->currentState;
    floatingEffect_t* fx;
    qboolean volviendo;
    qhandle_t shader;
    vec3_t origin;

    if (!cg_powerupPOIs.integer || item->giType != IT_POWERUP) {
        return;
    }

    volviendo = (qboolean)(cg.time < es->time);
    if (volviendo && cg.time < es->time - 10000) {
        return;
    }

    shader = CG_PowerupPoiShader(item->giTag, volviendo);
    if (!shader) {
        return;
    }

    VectorCopy(es->pos.trBase, origin);

    if (cg_powerupPOIs.integer == 1) {
        trace_t tr;

        CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, origin, -1, CONTENTS_SOLID);
        if (tr.fraction < 1.0f) {
            return;
        }
    }

    fx = CG_AllocFloatingEffect();
    if (!fx) {
        return;
    }

    fx->type = FE_SPRITE;
    fx->owner = cent;
    VectorCopy(origin, fx->origin);
    fx->shader = shader;
    fx->color[0] = fx->color[1] = fx->color[2] = fx->color[3] = 1.0f;
    fx->clientNum = -1;
    fx->followNum = -1;
    fx->zOffset = 48.0f;
    fx->worldSize = cg_poiMaxWidth.value;
    fx->minPixels = cg_poiMinWidth.value;
    fx->lifeRate = 1.0f;
    fx->doFade = 0;
}

static void CG_FlagMarkerAt(centity_t* cent, const vec4_t color, qhandle_t shader) {
    floatingEffect_t* fx = CG_AllocFloatingEffect();

    if (!fx) {
        return;
    }
    fx->type = FE_POI;
    fx->owner = cent;
    VectorCopy(cent->lerpOrigin, fx->origin);
    Vector4Copy(color, fx->color);
    fx->zOffset = 64.0f;
    fx->worldSize = cg_poiMaxWidth.value;
    fx->minPixels = cg_poiMinWidth.value;
    fx->shader = shader;
}

static void CG_FlagMarker(centity_t* cent, const gitem_t* item) {
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    static const vec4_t red = {1.0f, 0.2f, 0.2f, 1.0f};
    static const vec4_t blue = {0.2f, 0.6f, 1.0f, 1.0f};
    int tag = item->giTag;
    int team;
    qboolean marked;

    if (cgs.gametype == GT_AD) {
        marked = cg.warmupState >= 1;
    } else {
        marked = cgs.gametype == GT_CTF && cent->currentState.modelindex2 != 1;
    }
    if (!marked) {
        if (tag == PW_NEUTRALFLAG) {
            CG_FlagMarkerAt(cent, white, cgs.media.adCaptureShader);
        }
        return;
    }

    if (tag < PW_REDFLAG || tag > PW_NEUTRALFLAG) {
        return;
    }
    team = cgs.clientinfo[cg.predictedPlayerState.clientNum].team;

    if (cent->currentState.modelindex2 == 0) {
        if (!cg_flagPOIs.integer) {
            return;
        }
        if (team == TEAM_RED) {
            if (cg_gametype.integer == GT_AD) {
                if (cg.warmupTurn == 0) {
                    if (tag == PW_BLUEFLAG) {
                        CG_FlagMarkerAt(cent, blue, cgs.media.adAttackShader);
                    }
                } else if (cg.warmupTurn == 1 && tag == PW_REDFLAG) {
                    CG_FlagMarkerAt(cent, red, cgs.media.adDefendShader);
                }
            } else if (cg_gametype.integer == GT_CTF) {
                if (tag == PW_BLUEFLAG) {
                    CG_FlagMarkerAt(cent, blue, cgs.media.adAttackShader);
                } else if (tag == PW_REDFLAG && !cg.snap->ps.powerups[PW_BLUEFLAG]) {
                    CG_FlagMarkerAt(cent, red, cgs.media.adDefendShader);
                }
            }
        } else if (team == TEAM_BLUE) {
            if (cg_gametype.integer == GT_AD) {
                if (cg.warmupTurn == 1) {
                    if (tag == PW_REDFLAG) {
                        CG_FlagMarkerAt(cent, red, cgs.media.adAttackShader);
                    }
                } else if (cg.warmupTurn == 0 && tag == PW_BLUEFLAG) {
                    CG_FlagMarkerAt(cent, blue, cgs.media.adDefendShader);
                }
            } else if (cg_gametype.integer == GT_CTF) {
                if (tag == PW_REDFLAG) {
                    CG_FlagMarkerAt(cent, red, cgs.media.adAttackShader);
                } else if (tag == PW_BLUEFLAG && !cg.snap->ps.powerups[PW_REDFLAG]) {
                    CG_FlagMarkerAt(cent, blue, cgs.media.adDefendShader);
                }
            }
        }
        return;
    }

    if (!cg_flagPOIs.integer) {
        return;
    }
    if (tag == PW_REDFLAG) {
        if (team == TEAM_RED) {
            CG_FlagMarkerAt(cent, red, cgs.media.adDefendShader);
        } else if (team == TEAM_BLUE) {
            CG_FlagMarkerAt(cent, red, cgs.media.adAttackShader);
        } else {
            CG_FlagMarkerAt(cent, white, cgs.media.flagStatusHandles[2 + TEAM_RED * 4]);
        }
    } else if (tag == PW_BLUEFLAG) {
        if (team == TEAM_RED) {
            CG_FlagMarkerAt(cent, blue, cgs.media.adAttackShader);
        } else if (team == TEAM_BLUE) {
            CG_FlagMarkerAt(cent, blue, cgs.media.adDefendShader);
        } else {
            CG_FlagMarkerAt(cent, white, cgs.media.flagStatusHandles[2 + TEAM_BLUE * 4]);
        }
    } else {
        CG_FlagMarkerAt(cent, white, cgs.media.flagStatusHandles[2]);
    }
}

static void CG_Item(centity_t* cent) {
    refEntity_t ent;
    entityState_t* es;
    gitem_t* item;
    int msec;
    float frac;
    float scale;
    weaponInfo_t* wi;

    es = &cent->currentState;
    if (es->modelindex >= bg_numItems) {
        CG_Error("Bad item index %i on entity", es->modelindex);
    }

    if (!es->modelindex) {
        return;
    }
    item = &bg_itemlist[es->modelindex];

    CG_DrawItemTimer(cent, item);

    CG_DrawPowerupPoi(cent, item);

    {
        char skip[32];

        trap_Cvar_VariableStringBuffer("cg_skipItems", skip, sizeof(skip));
        if (skip[0] == '1') {
            return;
        }
    }

    if (es->eFlags & EF_NODRAW) {
        return;
    }

    if (item->giType == IT_TEAM) {
        if (item->giTag == PW_REDFLAG) {
            VectorCopy(cent->lerpOrigin, cg.flagOrigin[TEAM_RED]);
        } else if (item->giTag == PW_BLUEFLAG) {
            VectorCopy(cent->lerpOrigin, cg.flagOrigin[TEAM_BLUE]);
        } else if (item->giTag == PW_NEUTRALFLAG) {
            VectorCopy(cent->lerpOrigin, cg.flagOrigin[TEAM_FREE]);
        }
        CG_FlagMarker(cent, item);
    }

    if (cg_simpleItems.integer && item->giType != IT_TEAM) {
        float z = cent->lerpOrigin[2];

        if (item->giType == IT_PERSISTANT_POWERUP) {
            z += 10.0f;
        } else {
            z += cg_simpleItemsHeightOffset.integer;
        }

        if (cg_simpleItemsBob.integer == 1 ||
            (cg_simpleItemsBob.integer == 2 && item->itemTimer)) {
            float phase = (cg.time & 1023) * (float)(M_PI * 2.0) / 1024.0f;

            cent->lerpOrigin[2] = cent->lerpOrigin[2] + 10.0f + sin(phase) * 6.0f;
            z = cent->lerpOrigin[2];
        }

        memset(&ent, 0, sizeof(ent));
        ent.reType = RT_SPRITE;
        VectorCopy(cent->lerpOrigin, ent.origin);
        ent.origin[2] = z + (cg_simpleItemsRadius.value - 15.0f);
        ent.radius = cg_simpleItemsRadius.integer;
        ent.customShader = cg_items[es->modelindex].icon;
        ent.shaderRGBA[0] = 255;
        ent.shaderRGBA[1] = 255;
        ent.shaderRGBA[2] = 255;
        ent.shaderRGBA[3] = 255;
        trap_R_AddRefEntityToScene(&ent);
        return;
    }

    scale = 0.005 + cent->currentState.number * 0.00001;
    cent->lerpOrigin[2] += 4 + cos((cg.time + 1000) * scale) * 4;

    memset(&ent, 0, sizeof(ent));

    if (item->giType == IT_HEALTH) {
        VectorCopy(cg.autoAnglesFast, cent->lerpAngles);
        AxisCopy(cg.autoAxisFast, ent.axis);
    } else {
        VectorCopy(cg.autoAngles, cent->lerpAngles);
        AxisCopy(cg.autoAxis, ent.axis);
    }

    wi = NULL;
    if (item->giType == IT_WEAPON) {
        wi = &cg_weapons[item->giTag];
        cent->lerpOrigin[0] -=
            wi->weaponMidpoint[0] * ent.axis[0][0] +
            wi->weaponMidpoint[1] * ent.axis[1][0] +
            wi->weaponMidpoint[2] * ent.axis[2][0];
        cent->lerpOrigin[1] -=
            wi->weaponMidpoint[0] * ent.axis[0][1] +
            wi->weaponMidpoint[1] * ent.axis[1][1] +
            wi->weaponMidpoint[2] * ent.axis[2][1];
        cent->lerpOrigin[2] -=
            wi->weaponMidpoint[0] * ent.axis[0][2] +
            wi->weaponMidpoint[1] * ent.axis[1][2] +
            wi->weaponMidpoint[2] * ent.axis[2][2];

        cent->lerpOrigin[2] += 8;
    }

    if (item->giType == IT_WEAPON && item->giTag == WP_RAILGUN) {
        clientInfo_t* ci = &cgs.clientinfo[cg.snap->ps.clientNum];
        Byte4Copy(ci->c1RGBA, ent.shaderRGBA);
    }

    ent.hModel = cg_items[es->modelindex].models[0];

    VectorCopy(cent->lerpOrigin, ent.origin);
    VectorCopy(cent->lerpOrigin, ent.oldorigin);

    ent.nonNormalizedAxes = qfalse;

    msec = cg.time - cent->miscTime;
    if (msec >= 0 && msec < ITEM_SCALEUP_TIME) {
        frac = (float)msec / ITEM_SCALEUP_TIME;
        VectorScale(ent.axis[0], frac, ent.axis[0]);
        VectorScale(ent.axis[1], frac, ent.axis[1]);
        VectorScale(ent.axis[2], frac, ent.axis[2]);
        ent.nonNormalizedAxes = qtrue;
    } else {
        frac = 1.0;
    }

    ent.renderfx |= RF_MINLIGHT;

    if (item->giType == IT_WEAPON) {
        VectorScale(ent.axis[0], 1.5, ent.axis[0]);
        VectorScale(ent.axis[1], 1.5, ent.axis[1]);
        VectorScale(ent.axis[2], 1.5, ent.axis[2]);
        ent.nonNormalizedAxes = qtrue;
        trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, cgs.media.weaponHoverSound);
    }

    if (item->giType == IT_HOLDABLE && item->giTag == HI_KAMIKAZE) {
        VectorScale(ent.axis[0], 2, ent.axis[0]);
        VectorScale(ent.axis[1], 2, ent.axis[1]);
        VectorScale(ent.axis[2], 2, ent.axis[2]);
        ent.nonNormalizedAxes = qtrue;
    }

    if (item->giType == IT_WEAPON) {
        CG_BrightWeapon(&ent, item->giTag, -1, BRIGHTWEAPON_ON_MAP);
    }

    trap_R_AddRefEntityToScene(&ent);

    if (item->giType == IT_WEAPON && wi && wi->barrelModel) {
        refEntity_t barrel;
        vec3_t angles;

        memset(&barrel, 0, sizeof(barrel));

        barrel.hModel = wi->barrelModel;

        VectorCopy(ent.lightingOrigin, barrel.lightingOrigin);
        barrel.shadowPlane = ent.shadowPlane;
        barrel.renderfx = ent.renderfx;

        angles[YAW] = 0;
        angles[PITCH] = 0;
        angles[ROLL] = 0;
        AnglesToAxis(angles, barrel.axis);

        CG_PositionRotatedEntityOnTag(&barrel, &ent, wi->weaponModel, "tag_barrel");

        barrel.nonNormalizedAxes = ent.nonNormalizedAxes;

        CG_BrightWeapon(&barrel, item->giTag, -1, BRIGHTWEAPON_ON_MAP);
        trap_R_AddRefEntityToScene(&barrel);
    }

    if (!cg_simpleItems.integer) {
        vec3_t spinAngles;

        VectorClear(spinAngles);

        if (item->giType == IT_HEALTH || item->giType == IT_POWERUP) {
            if ((ent.hModel = cg_items[es->modelindex].models[1]) != 0) {
                if (item->giType == IT_POWERUP) {
                    ent.origin[2] += 12;
                    spinAngles[1] = (cg.time & 1023) * 360 / -1024.0f;
                }
                AnglesToAxis(spinAngles, ent.axis);

                if (frac != 1.0) {
                    VectorScale(ent.axis[0], frac, ent.axis[0]);
                    VectorScale(ent.axis[1], frac, ent.axis[1]);
                    VectorScale(ent.axis[2], frac, ent.axis[2]);
                    ent.nonNormalizedAxes = qtrue;
                }
                trap_R_AddRefEntityToScene(&ent);
            }
        }
    }
}

static void CG_GrenadeColor(const entityState_t* es, byte* color) {
    int viewer, owner, packed;
    qboolean teammate;

    viewer = cg.predictedPlayerState.clientNum;
    owner = es->clientNum;
    teammate = CG_ShouldForceTeamSkin(cgs.clientinfo[viewer].team, cgs.clientinfo[owner].team);

    if ((cgs.gametype < GT_TEAM && viewer == owner) || teammate) {
        if (cg_forceTeamWeaponColor.integer == 1 && viewer != owner) {
            packed = cg_teamUpperColor.integer;
        } else {
            packed = cg_weaponColor_grenade.integer;
        }
    } else if (cg_forceEnemyWeaponColor.integer == 1) {
        packed = cg_enemyUpperColor.integer;
    } else {
        packed = cg_weaponColor_grenade.integer;
    }

    color[0] = (packed >> 24) & 0xff;
    color[1] = (packed >> 16) & 0xff;
    color[2] = (packed >> 8) & 0xff;
    color[3] = packed & 0xff;
}

#define CG_PROJECTILE_LEAD 28.0f
#define CG_PROJECTILE_LEAD_TIME 150.0f

void CG_ProjectileLeadOffset(const centity_t* cent, vec3_t offset) {
    float age, lead;
    vec3_t velocity, origin, end;
    trace_t tr;

    VectorClear(offset);

    if (cent->currentState.pos.trTime != cent->leadTrTime) {
        return;
    }

    age = (float)(cg.time - cent->currentState.pos.trTime);
    if (age < 0.0f || age >= CG_PROJECTILE_LEAD_TIME) {
        return;
    }

    BG_EvaluateTrajectoryDelta(&cent->currentState.pos, cg.time, velocity);
    if (VectorNormalize(velocity) == 0.0f) {
        return;
    }

    lead = CG_PROJECTILE_LEAD * (1.0f - age / CG_PROJECTILE_LEAD_TIME);
    BG_EvaluateTrajectory(&cent->currentState.pos, cg.time, origin);
    VectorMA(origin, lead, velocity, end);
    CG_Trace(&tr, origin, NULL, NULL, end, cent->currentState.number, MASK_SOLID);
    VectorScale(velocity, lead * tr.fraction, offset);
}

static void CG_Missile(centity_t* cent) {
    refEntity_t ent;
    entityState_t* s1;
    const weaponInfo_t* weapon;
    vec3_t leadOffset;

    s1 = &cent->currentState;
    if (s1->weapon >= WP_NUM_WEAPONS) {
        s1->weapon = 0;
    }
    weapon = &cg_weapons[s1->weapon];

    VectorCopy(s1->angles, cent->lerpAngles);

    if (weapon->missileTrailFunc) {
        weapon->missileTrailFunc(cent, weapon);
    }
    if (weapon->missileDlight) {
        CG_AddEffectLight(cent->lerpOrigin, weapon->missileDlight,
                               weapon->missileDlightColor[0], weapon->missileDlightColor[1], weapon->missileDlightColor[2]);
    }

    if (weapon->missileSound) {
        vec3_t velocity;

        BG_EvaluateTrajectoryDelta(&cent->currentState.pos, cg.time, velocity);

        trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, velocity, weapon->missileSound);
    }

    memset(&ent, 0, sizeof(ent));
    CG_ProjectileLeadOffset(cent, leadOffset);
    VectorAdd(cent->lerpOrigin, leadOffset, ent.origin);
    VectorCopy(ent.origin, ent.oldorigin);

    if (cent->currentState.weapon == WP_PLASMAGUN) {
        ent.reType = RT_SPRITE;
        ent.radius = 16;
        ent.rotation = 0;
        ent.customShader = cgs.media.plasmaBallShader;
        trap_R_AddRefEntityToScene(&ent);
        return;
    }

    ent.skinNum = cg.clientFrame & 1;
    ent.hModel = weapon->missileModel;
    ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

    if (cent->currentState.weapon == WP_PROX_LAUNCHER) {
        if (s1->generic1 == TEAM_BLUE) {
            ent.hModel = cgs.media.blueProxMine;
        }
    }

    if (s1->weapon == WP_GRENADE_LAUNCHER) {
        if (cg_forceTeamWeaponColor.integer == 1 || cg_forceEnemyWeaponColor.integer == 1) {
            CG_GrenadeColor(s1, ent.shaderRGBA);
        } else {
            ent.shaderRGBA[0] = (cg_weaponColor_grenade.integer >> 24) & 0xff;
            ent.shaderRGBA[1] = (cg_weaponColor_grenade.integer >> 16) & 0xff;
            ent.shaderRGBA[2] = (cg_weaponColor_grenade.integer >> 8) & 0xff;
            ent.shaderRGBA[3] = cg_weaponColor_grenade.integer & 0xff;
        }
    }

    if (VectorNormalize2(s1->pos.trDelta, ent.axis[0]) == 0) {
        ent.axis[0][2] = 1;
    }

    if (s1->pos.trType != TR_STATIONARY) {
        RotateAroundDirection(ent.axis, cg.time / 4);
    } else {
        if (s1->weapon == WP_PROX_LAUNCHER) {
            AnglesToAxis(cent->lerpAngles, ent.axis);
        } else {
            RotateAroundDirection(ent.axis, s1->time);
        }
    }

    CG_AddRefEntityWithPowerups(&ent, s1, TEAM_FREE);
}

static void CG_Grapple(centity_t* cent) {
    refEntity_t ent;
    entityState_t* s1;
    const weaponInfo_t* weapon;

    s1 = &cent->currentState;
    if (s1->weapon >= WP_NUM_WEAPONS) {
        s1->weapon = 0;
    }
    weapon = &cg_weapons[s1->weapon];

    VectorCopy(s1->angles, cent->lerpAngles);

    if (weapon->missileSound) {
        trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->missileSound);
    }

    memset(&ent, 0, sizeof(ent));
    VectorCopy(cent->lerpOrigin, ent.origin);
    VectorCopy(cent->lerpOrigin, ent.oldorigin);

    ent.skinNum = cg.clientFrame & 1;
    ent.hModel = weapon->missileModel;
    ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

    if (VectorNormalize2(s1->pos.trDelta, ent.axis[0]) == 0) {
        ent.axis[0][2] = 1;
    }

    trap_R_AddRefEntityToScene(&ent);
}

static void CG_Mover(centity_t* cent) {
    refEntity_t ent;
    entityState_t* s1;

    s1 = &cent->currentState;

    memset(&ent, 0, sizeof(ent));
    VectorCopy(cent->lerpOrigin, ent.origin);
    VectorCopy(cent->lerpOrigin, ent.oldorigin);
    AnglesToAxis(cent->lerpAngles, ent.axis);

    ent.renderfx = RF_NOSHADOW;

    ent.skinNum = (cg.time >> 6) & 1;

    if (s1->solid == SOLID_BMODEL) {
        ent.hModel = cgs.inlineDrawModel[s1->modelindex];
    } else {
        ent.hModel = cgs.gameModels[s1->modelindex];
    }

    trap_R_AddRefEntityToScene(&ent);

    if (s1->modelindex2) {
        ent.skinNum = 0;
        ent.hModel = cgs.gameModels[s1->modelindex2];
        trap_R_AddRefEntityToScene(&ent);
    }
}

void CG_Beam(centity_t* cent) {
    localEntity_t* le;
    entityState_t* s1;

    s1 = &cent->currentState;

    le = CG_AllocLocalEntity();
    le->leType = LE_FADE_RGB;
    le->startTime = cg.time;
    le->endTime = cg.time + 500;
    le->lifeRate = 1.0 / (le->endTime - le->startTime);

    le->refEntity.reType = RT_RAIL_CORE;
    le->refEntity.customShader = cgs.media.railCoreShader;
    le->refEntity.shaderTime = cg.time / 1000.0f;
    le->refEntity.radius = 256.0;

    VectorCopy(s1->pos.trBase, le->refEntity.origin);
    VectorCopy(s1->origin2, le->refEntity.oldorigin);

    le->color[0] = 0.75;
    le->color[1] = 0.0;
    le->color[2] = 0.0;
    le->color[3] = 1.0;
}

static void CG_Portal(centity_t* cent) {
    refEntity_t ent;
    entityState_t* s1;

    s1 = &cent->currentState;

    memset(&ent, 0, sizeof(ent));
    VectorCopy(cent->lerpOrigin, ent.origin);
    VectorCopy(s1->origin2, ent.oldorigin);
    ByteToDir(s1->eventParm, ent.axis[0]);
    PerpendicularVector(ent.axis[1], ent.axis[0]);

    VectorSubtract(vec3_origin, ent.axis[1], ent.axis[1]);

    CrossProduct(ent.axis[0], ent.axis[1], ent.axis[2]);
    ent.reType = RT_PORTALSURFACE;
    ent.oldframe = s1->powerups;
    ent.frame = s1->frame;
    ent.skinNum = s1->clientNum / 256.0 * 360;

    trap_R_AddRefEntityToScene(&ent);
}

void CG_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3]) {
    AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
    VectorInverse(matrix[1]);
}

void CG_TransposeMatrix(vec3_t matrix[3], vec3_t transpose[3]) {
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            transpose[i][j] = matrix[j][i];
        }
    }
}

void CG_RotatePoint(vec3_t point, vec3_t matrix[3]) {
    vec3_t tvec;

    VectorCopy(point, tvec);
    point[0] = DotProduct(matrix[0], tvec);
    point[1] = DotProduct(matrix[1], tvec);
    point[2] = DotProduct(matrix[2], tvec);
}

void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out) {
    centity_t* cent;
    vec3_t oldOrigin, origin, deltaOrigin;
    vec3_t oldAngles, angles, deltaAngles;
    vec3_t matrix[3], transpose[3];
    vec3_t org, org2, move2;

    if (moverNum <= 0 || moverNum >= ENTITYNUM_MAX_NORMAL) {
        VectorCopy(in, out);
        VectorCopy(angles_in, angles_out);
        return;
    }

    cent = &cg_entities[moverNum];
    if (cent->currentState.eType != ET_MOVER) {
        VectorCopy(in, out);
        VectorCopy(angles_in, angles_out);
        return;
    }

    BG_EvaluateTrajectory(&cent->currentState.pos, fromTime, oldOrigin);
    BG_EvaluateTrajectory(&cent->currentState.apos, fromTime, oldAngles);

    BG_EvaluateTrajectory(&cent->currentState.pos, toTime, origin);
    BG_EvaluateTrajectory(&cent->currentState.apos, toTime, angles);

    VectorSubtract(origin, oldOrigin, deltaOrigin);
    VectorSubtract(angles, oldAngles, deltaAngles);

    CG_CreateRotationMatrix(deltaAngles, transpose);
    CG_TransposeMatrix(transpose, matrix);
    VectorSubtract(in, oldOrigin, org);
    VectorCopy(org, org2);
    CG_RotatePoint(org2, matrix);
    VectorSubtract(org2, org, move2);
    VectorAdd(deltaOrigin, move2, deltaOrigin);

    VectorAdd(in, deltaOrigin, out);
    VectorAdd(angles_in, deltaAngles, angles_out);
}

static void CG_InterpolateEntityPosition(centity_t* cent) {
    vec3_t current, next;
    float f;

    if (cg.nextSnap == NULL) {
        CG_Error("CG_InterpoateEntityPosition: cg.nextSnap == NULL");
    }

    f = cg.frameInterpolation;

    BG_EvaluateTrajectory(&cent->currentState.pos, cg.snap->serverTime, current);
    BG_EvaluateTrajectory(&cent->nextState.pos, cg.nextSnap->serverTime, next);

    cent->lerpOrigin[0] = current[0] + f * (next[0] - current[0]);
    cent->lerpOrigin[1] = current[1] + f * (next[1] - current[1]);
    cent->lerpOrigin[2] = current[2] + f * (next[2] - current[2]);

    BG_EvaluateTrajectory(&cent->currentState.apos, cg.snap->serverTime, current);
    BG_EvaluateTrajectory(&cent->nextState.apos, cg.nextSnap->serverTime, next);

    cent->lerpAngles[0] = LerpAngle(current[0], next[0], f);
    cent->lerpAngles[1] = LerpAngle(current[1], next[1], f);
    cent->lerpAngles[2] = LerpAngle(current[2], next[2], f);
}

static void CG_CalcEntityLerpPositions(centity_t* cent) {
    static int lastNudge;
    int frameMsec = cg_svFps.integer > 0 ? (int)(1000.0f / cg_svFps.integer) : 0;
    int timeshift = 0;

    if (cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE) {
        CG_InterpolateEntityPosition(cent);
        return;
    }

    if (cent->interpolate && cent->currentState.pos.trType == TR_LINEAR_STOP &&
        cent->currentState.number < MAX_CLIENTS) {
        CG_InterpolateEntityPosition(cent);
        return;
    }

    if (cent->currentState.number < MAX_CLIENTS &&
        cent->currentState.clientNum != cg.predictedPlayerState.clientNum) {
        cent->currentState.pos.trType = TR_LINEAR_STOP;
        cent->currentState.pos.trTime = cg.snap->serverTime;
        cent->currentState.pos.trDuration = frameMsec;
    }

    if (cent->currentState.eType == ET_MISSILE && cent->currentState.weapon != WP_GRAPPLING_HOOK) {
        if (cent->currentState.otherEntityNum == cg.clientNum) {
            timeshift = frameMsec;
        } else if (cent->currentState.weapon != WP_GRENADE_LAUNCHER &&
                   cent->currentState.weapon != WP_PROX_LAUNCHER) {
            int nudge;

            if (cg_autoProjectileNudge.integer) {
                nudge = cg.snap->ping;
                if (lastNudge != 0 && lastNudge != nudge) {
                    nudge = lastNudge;
                }
                if (nudge > 80) {
                    nudge = 80;
                } else if (nudge < 0) {
                    nudge = 0;
                }
            } else {
                nudge = cg_projectileNudge.integer;
            }
            timeshift = nudge + frameMsec;
            lastNudge = nudge;
        }
    }

    BG_EvaluateTrajectory(&cent->currentState.pos, cg.time + timeshift, cent->lerpOrigin);
    BG_EvaluateTrajectory(&cent->currentState.apos, cg.time + timeshift, cent->lerpAngles);

    if (timeshift != 0) {
        trace_t tr;
        vec3_t lastOrigin;

        BG_EvaluateTrajectory(&cent->currentState.pos, cg.time, lastOrigin);
        CG_Trace(&tr, lastOrigin, vec3_origin, vec3_origin, cent->lerpOrigin, cent->currentState.number, MASK_SHOT);

        if (tr.fraction < 1.0f) {
            cent->lerpOrigin[0] = lastOrigin[0] + tr.fraction * (cent->lerpOrigin[0] - lastOrigin[0]);
            cent->lerpOrigin[1] = lastOrigin[1] + tr.fraction * (cent->lerpOrigin[1] - lastOrigin[1]);
            cent->lerpOrigin[2] = lastOrigin[2] + tr.fraction * (cent->lerpOrigin[2] - lastOrigin[2]);
        }
    }

    if (cent != &cg.predictedPlayerEntity) {
        CG_AdjustPositionForMover(cent->lerpOrigin, cent->currentState.groundEntityNum,
                                  cg.snap->serverTime, cg.time, cent->lerpOrigin, cent->lerpAngles, cent->lerpAngles);
    }
}

static void CG_TeamBase(centity_t* cent) {
    refEntity_t model;
    vec3_t angles;
    int t, h;
    float c;

    if (cgs.gametype == GT_CTF || cgs.gametype == GT_AD) {
        memset(&model, 0, sizeof(model));
        model.reType = RT_MODEL;
        VectorCopy(cent->lerpOrigin, model.lightingOrigin);
        VectorCopy(cent->lerpOrigin, model.origin);
        AnglesToAxis(cent->currentState.angles, model.axis);
        if (cent->currentState.modelindex == TEAM_RED) {
            model.hModel = cgs.media.redFlagBaseModel;
        } else if (cent->currentState.modelindex == TEAM_BLUE) {
            model.hModel = cgs.media.blueFlagBaseModel;
        } else {
            model.hModel = cgs.media.neutralFlagBaseModel;
        }
        trap_R_AddRefEntityToScene(&model);
    } else if (cgs.gametype == GT_1FCTF) {
        memset(&model, 0, sizeof(model));
        model.reType = RT_MODEL;
        VectorCopy(cent->lerpOrigin, model.lightingOrigin);
        VectorCopy(cent->lerpOrigin, model.origin);
        AnglesToAxis(cent->currentState.angles, model.axis);
        if (cent->currentState.modelindex == TEAM_RED) {
            model.hModel = cgs.media.harvesterModel;
            model.customSkin = cgs.media.harvesterRedSkin;
        } else if (cent->currentState.modelindex == TEAM_BLUE) {
            model.hModel = cgs.media.harvesterModel;
            model.customSkin = cgs.media.harvesterBlueSkin;
        } else {
            model.hModel = cgs.media.neutralFlagBaseModel;
        }
        trap_R_AddRefEntityToScene(&model);
    } else if (cgs.gametype == GT_OBELISK) {
        memset(&model, 0, sizeof(model));
        model.reType = RT_MODEL;
        VectorCopy(cent->lerpOrigin, model.lightingOrigin);
        VectorCopy(cent->lerpOrigin, model.origin);
        AnglesToAxis(cent->currentState.angles, model.axis);

        model.hModel = cgs.media.overloadBaseModel;
        trap_R_AddRefEntityToScene(&model);
        if (cent->currentState.frame == 1) {
            c = cent->currentState.modelindex2;
            model.shaderRGBA[0] = 0xff;
            model.shaderRGBA[1] = c;
            model.shaderRGBA[2] = c;
            model.shaderRGBA[3] = 0xff;
            model.hModel = cgs.media.overloadEnergyModel;
            trap_R_AddRefEntityToScene(&model);
        }
        if (cent->currentState.frame == 2) {
            if (!cent->miscTime) {
                cent->miscTime = cg.time;
            }
            t = cg.time - cent->miscTime;
            h = (cg_obeliskRespawnDelay.integer - 5) * 1000;
            if (t > h) {
                c = (float)(t - h) / h;
                if (c > 1)
                    c = 1;
            } else {
                c = 0;
            }
            AnglesToAxis(cent->currentState.angles, model.axis);
            model.shaderRGBA[0] = (byte)(int)(c * 255.0f + 0.5f);
            model.shaderRGBA[1] = model.shaderRGBA[0];
            model.shaderRGBA[2] = model.shaderRGBA[0];
            model.shaderRGBA[3] = model.shaderRGBA[0];

            model.hModel = cgs.media.overloadLightsModel;
            trap_R_AddRefEntityToScene(&model);
            if (t > h) {
                if (!cent->muzzleFlashTime) {
                    trap_S_StartSound(cent->lerpOrigin, ENTITYNUM_NONE, CHAN_BODY, cgs.media.obeliskRespawnSound);
                    cent->muzzleFlashTime = 1;
                }
                VectorCopy(cent->currentState.angles, angles);
                angles[YAW] += (float)16 * acos(1 - c) * 180 / M_PI;
                AnglesToAxis(angles, model.axis);

                VectorScale(model.axis[0], c, model.axis[0]);
                VectorScale(model.axis[1], c, model.axis[1]);
                VectorScale(model.axis[2], c, model.axis[2]);

                model.shaderRGBA[0] = 0xff;
                model.shaderRGBA[1] = 0xff;
                model.shaderRGBA[2] = 0xff;
                model.shaderRGBA[3] = 0xff;
                model.origin[2] += 56;
                model.hModel = cgs.media.overloadTargetModel;
                trap_R_AddRefEntityToScene(&model);
            } else {
            }
        } else {
            cent->miscTime = 0;
            cent->muzzleFlashTime = 0;
            c = cent->currentState.modelindex2;
            model.shaderRGBA[0] = 0xff;
            model.shaderRGBA[1] = c;
            model.shaderRGBA[2] = c;
            model.shaderRGBA[3] = 0xff;
            model.hModel = cgs.media.overloadLightsModel;
            trap_R_AddRefEntityToScene(&model);
            model.origin[2] += 56;
            model.hModel = cgs.media.overloadTargetModel;
            trap_R_AddRefEntityToScene(&model);
        }
    } else if (cgs.gametype == GT_HARVESTER) {
        vec3_t receptacle;

        memset(&model, 0, sizeof(model));
        model.reType = RT_MODEL;
        VectorCopy(cent->lerpOrigin, model.lightingOrigin);
        VectorCopy(cent->lerpOrigin, model.origin);
        AnglesToAxis(cent->currentState.angles, model.axis);
        VectorCopy(cent->lerpOrigin, receptacle);
        receptacle[2] += 16.0f;

        if (cent->currentState.modelindex == TEAM_RED) {
            model.hModel = cgs.media.harvesterModel;
            model.customSkin = cgs.media.harvesterRedSkin;
            VectorCopy(receptacle, cg.flagOrigin[TEAM_RED]);
        } else if (cent->currentState.modelindex == TEAM_BLUE) {
            model.hModel = cgs.media.harvesterModel;
            model.customSkin = cgs.media.harvesterBlueSkin;
            VectorCopy(receptacle, cg.flagOrigin[TEAM_BLUE]);
        } else {
            model.hModel = cgs.media.harvesterNeutralModel;
            model.customSkin = 0;
            VectorCopy(receptacle, cg.flagOrigin[TEAM_FREE]);
        }
        trap_R_AddRefEntityToScene(&model);
    }
}

static void CG_AddCEntity(centity_t* cent) {
    if (cent->currentState.eType >= ET_EVENTS) {
        return;
    }

    CG_CalcEntityLerpPositions(cent);

    CG_EntityEffects(cent);

    switch (cent->currentState.eType) {
        default:
            CG_Error("Bad entity type: %i", cent->currentState.eType);
            break;
        case ET_INVISIBLE:
        case ET_PUSH_TRIGGER:
        case ET_TELEPORT_TRIGGER:
            break;
        case ET_GENERAL:
            CG_General(cent);
            break;
        case ET_PLAYER:
            CG_Player(cent);
            break;
        case ET_ITEM:
            CG_Item(cent);
            break;
        case ET_MISSILE:
            CG_Missile(cent);
            break;
        case ET_MOVER:
            CG_Mover(cent);
            break;
        case ET_BEAM:
            CG_Beam(cent);
            break;
        case ET_PORTAL:
            CG_Portal(cent);
            break;
        case ET_SPEAKER:
            if (cg_ambient.integer) {
                CG_Speaker(cent);
            }
            break;
        case ET_GRAPPLE:
            CG_Grapple(cent);
            break;
        case ET_TEAM:
            CG_TeamBase(cent);
            break;
    }
}

void CG_AddPacketEntities(void) {
    int num;
    centity_t* cent;
    playerState_t* ps;

    if (cg.nextSnap) {
        int delta;

        delta = (cg.nextSnap->serverTime - cg.snap->serverTime);
        if (delta == 0) {
            cg.frameInterpolation = 0;
        } else {
            cg.frameInterpolation = (float)(cg.time - cg.snap->serverTime) / delta;
        }
    } else {
        cg.frameInterpolation = 0;
    }

    cg.autoAngles[0] = 0;
    cg.autoAngles[1] = (cg.time & 2047) * 360 / 2048.0;
    cg.autoAngles[2] = 0;

    cg.autoAnglesFast[0] = 0;
    cg.autoAnglesFast[1] = (cg.time & 1023) * 360 / 1024.0f;
    cg.autoAnglesFast[2] = 0;

    AnglesToAxis(cg.autoAngles, cg.autoAxis);
    AnglesToAxis(cg.autoAnglesFast, cg.autoAxisFast);

    ps = &cg.predictedPlayerState;
    BG_PlayerStateToEntityState(ps, &cg.predictedPlayerEntity.currentState, qfalse);
    if ((ps->pm_flags & PMF_FOLLOW) && ps->stats[STAT_HEALTH] > GIB_HEALTH) {
        CG_CalcEntityLerpPositions(&cg.predictedPlayerEntity);
        CG_EntityEffects(&cg.predictedPlayerEntity);
        CG_Player(&cg.predictedPlayerEntity);
    } else {
        CG_AddCEntity(&cg.predictedPlayerEntity);
    }

    CG_CalcEntityLerpPositions(&cg_entities[cg.snap->ps.clientNum]);

    for (num = 0; num < cg.snap->numEntities; num++) {
        cent = &cg_entities[cg.snap->entities[num].number];
        CG_AddCEntity(cent);
    }
}
