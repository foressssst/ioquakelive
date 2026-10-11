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

extern displayContextDef_t cgDC;

static int cg_currentFontIndex = 0;

static void CG_OwnerDrawText(float x, float y, float scale, vec4_t color,
                              const char *text, float adjust, int limit, int style) {
    CG_DrawText(x, y, cg_currentFontIndex, scale, color, text, adjust, limit, style);
}

static int CG_OwnerDrawTextWidth(const char *text, float scale, int limit) {
    return CG_DrawTextWidth(text, scale, limit, cg_currentFontIndex);
}

static int CG_OwnerDrawTextHeight(const char *text, float scale, int limit) {
    return CG_DrawTextHeight(text, scale, limit, cg_currentFontIndex);
}

void CG_InitTeamChat(void) {
    memset(teamChat1, 0, sizeof(teamChat1));
    memset(teamChat2, 0, sizeof(teamChat2));
    memset(systemChat, 0, sizeof(systemChat));
}

void CG_SetPrintString(int type, const char* p) {
    if (type == SYSTEM_PRINT) {
        strcpy(systemChat, p);
    } else {
        strcpy(teamChat2, teamChat1);
        strcpy(teamChat1, p);
    }
}

void CG_CheckOrderPending(void) {
    if (cgs.gametype < GT_CTF) {
        return;
    }
    if (cgs.orderPending) {
        const char *p1, *p2, *b;
        p1 = p2 = b = NULL;
        switch (cgs.currentOrder) {
            case TEAMTASK_OFFENSE:
                p1 = VOICECHAT_ONOFFENSE;
                p2 = VOICECHAT_OFFENSE;
                b = "+button7; wait; -button7";
                break;
            case TEAMTASK_DEFENSE:
                p1 = VOICECHAT_ONDEFENSE;
                p2 = VOICECHAT_DEFEND;
                b = "+button8; wait; -button8";
                break;
            case TEAMTASK_PATROL:
                p1 = VOICECHAT_ONPATROL;
                p2 = VOICECHAT_PATROL;
                b = "+button9; wait; -button9";
                break;
            case TEAMTASK_FOLLOW:
                p1 = VOICECHAT_ONFOLLOW;
                p2 = VOICECHAT_FOLLOWME;
                b = "+button10; wait; -button10";
                break;
            case TEAMTASK_CAMP:
                p1 = VOICECHAT_ONCAMPING;
                p2 = VOICECHAT_CAMP;
                break;
            case TEAMTASK_RETRIEVE:
                p1 = VOICECHAT_ONGETFLAG;
                p2 = VOICECHAT_RETURNFLAG;
                break;
            case TEAMTASK_ESCORT:
                p1 = VOICECHAT_ONFOLLOWCARRIER;
                p2 = VOICECHAT_FOLLOWFLAGCARRIER;
                break;
        }

        if (cg_currentSelectedPlayer.integer == numSortedTeamPlayers) {
            trap_SendConsoleCommand(va("cmd vsay_team %s\n", p2));
        } else {
            if (sortedTeamPlayers[cg_currentSelectedPlayer.integer] == cg.snap->ps.clientNum && p1) {
                trap_SendConsoleCommand(va("teamtask %i\n", cgs.currentOrder));
                trap_SendConsoleCommand(va("cmd vsay_team %s\n", p1));
            } else if (p2) {
                trap_SendConsoleCommand(va("cmd vtell %d %s\n", sortedTeamPlayers[cg_currentSelectedPlayer.integer], p2));
            }
        }
        if (b) {
            trap_SendConsoleCommand(b);
        }
        cgs.orderPending = qfalse;
    }
}

static void CG_SetSelectedPlayerName(void) {
    if (cg_currentSelectedPlayer.integer >= 0 && cg_currentSelectedPlayer.integer < numSortedTeamPlayers) {
        clientInfo_t* ci = cgs.clientinfo + sortedTeamPlayers[cg_currentSelectedPlayer.integer];
        if (ci) {
            trap_Cvar_Set("cg_selectedPlayerName", ci->name);
            trap_Cvar_Set("cg_selectedPlayer", va("%d", sortedTeamPlayers[cg_currentSelectedPlayer.integer]));
            cgs.currentOrder = ci->teamTask;
        }
    } else {
        trap_Cvar_Set("cg_selectedPlayerName", "Everyone");
    }
}
int CG_GetSelectedPlayer(void) {
    if (cg_currentSelectedPlayer.integer < 0 || cg_currentSelectedPlayer.integer >= numSortedTeamPlayers) {
        cg_currentSelectedPlayer.integer = 0;
    }
    return cg_currentSelectedPlayer.integer;
}

void CG_SelectNextPlayer(void) {
    CG_CheckOrderPending();
    if (cg_currentSelectedPlayer.integer >= 0 && cg_currentSelectedPlayer.integer < numSortedTeamPlayers) {
        cg_currentSelectedPlayer.integer++;
    } else {
        cg_currentSelectedPlayer.integer = 0;
    }
    CG_SetSelectedPlayerName();
}

void CG_SelectPrevPlayer(void) {
    CG_CheckOrderPending();
    if (cg_currentSelectedPlayer.integer > 0 && cg_currentSelectedPlayer.integer <= numSortedTeamPlayers) {
        cg_currentSelectedPlayer.integer--;
    } else {
        cg_currentSelectedPlayer.integer = numSortedTeamPlayers;
    }
    CG_SetSelectedPlayerName();
}

static void CG_DrawPlayerArmorIcon(rectDef_t* rect, qboolean draw2D) {
    vec3_t angles;
    vec3_t origin;

    if (cg_drawStatus.integer == 0) {
        return;
    }

    if (draw2D || (!cg_draw3dIcons.integer && cg_drawIcons.integer)) {
        CG_DrawPic(rect->x, rect->y + rect->h / 2 + 1, rect->w, rect->h, cgs.media.armorIcon);
    } else if (cg_draw3dIcons.integer) {
        VectorClear(angles);
        origin[0] = 90;
        origin[1] = 0;
        origin[2] = -10;
        angles[YAW] = (cg.time & 2047) * 360 / 2048.0f;
        CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, cgs.media.armorModel, 0, origin, angles, NULL);
    }
}

static void CG_DrawPlayerArmorValue(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle, int align) {
    char num[16];
    int value;
    float tx, ty;
    playerState_t* ps;

    ps = &cg.snap->ps;

    value = ps->stats[STAT_ARMOR];

    if (shader) {
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
        trap_R_SetColor(NULL);
    } else {
        Com_sprintf(num, sizeof(num), "%i", value);
        tx = rect->x;
        if (align == ITEM_ALIGN_CENTER) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0) * 0.5f;
        } else if (align == ITEM_ALIGN_RIGHT) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0);
        }
        ty = rect->y + CG_OwnerDrawTextHeight(num, scale, 0);
        CG_OwnerDrawText(tx, ty, scale, color, num, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerAmmoIcon(rectDef_t* rect, qboolean draw2D) {
    centity_t* cent;
    vec3_t angles;
    vec3_t origin;

    cent = &cg_entities[cg.snap->ps.clientNum];

    if (draw2D || (!cg_draw3dIcons.integer && cg_drawIcons.integer)) {
        qhandle_t icon;
        icon = cg_weapons[cg.predictedPlayerState.weapon].ammoIcon;
        if (icon) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, icon);
        }
    } else if (cg_draw3dIcons.integer) {
        if (cent->currentState.weapon && cg_weapons[cent->currentState.weapon].ammoModel) {
            VectorClear(angles);
            origin[0] = 70;
            origin[1] = 0;
            origin[2] = 0;
            angles[YAW] = 90 + 20 * sin(cg.time / 1000.0);
            CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, cg_weapons[cent->currentState.weapon].ammoModel, 0, origin, angles, NULL);
        }
    }
}

static void CG_DrawPlayerAmmoValue(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle, int align) {
    char num[16];
    int value;
    float tx, ty;
    int weapon;
    playerState_t* ps;

    ps = &cg.snap->ps;
    weapon = cg_entities[ps->clientNum].currentState.weapon;

    if (weapon == WP_NONE || weapon == WP_GAUNTLET || weapon == WP_NAILGUN) {
        return;
    }

    value = ps->ammo[weapon];
    if (value >= 0) {
        if (shader) {
            trap_R_SetColor(color);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
            trap_R_SetColor(NULL);
        } else {
            Com_sprintf(num, sizeof(num), "%i", value);
            tx = rect->x;
            if (align == ITEM_ALIGN_CENTER) {
                tx -= CG_OwnerDrawTextWidth(num, scale, 0) * 0.5f;
            } else if (align == ITEM_ALIGN_RIGHT) {
                tx -= CG_OwnerDrawTextWidth(num, scale, 0);
            }
            ty = rect->y + CG_OwnerDrawTextHeight(num, scale, 0);
            CG_OwnerDrawText(tx, ty, scale, color, num, 0, 0, textStyle);
        }
    } else if (value == -1) {
        tx = rect->x;
        if (align == ITEM_ALIGN_CENTER) {
            tx -= rect->h * 0.5f;
        } else if (align == ITEM_ALIGN_RIGHT) {
            tx -= rect->h;
        }
        trap_R_SetColor(color);
        CG_DrawPic(tx, rect->y, rect->h, rect->h, cgs.media.infiniteAmmoShader);
        trap_R_SetColor(NULL);
    }
}

static void CG_DrawPlayerHead(rectDef_t* rect, qboolean draw2D) {
    vec3_t angles;
    float size, stretch;
    float frac;
    float x = rect->x;

    VectorClear(angles);

    if (cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME) {
        frac = (float)(cg.time - cg.damageTime) / DAMAGE_TIME;
        size = rect->w * 1.25 * (1.5 - frac * 0.5);

        stretch = size - rect->w * 1.25;
        x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

        cg.headStartYaw = 180 + cg.damageX * 45;

        cg.headEndYaw = 180 + 20 * cos(crandom() * M_PI);
        cg.headEndPitch = 5 * cos(crandom() * M_PI);

        cg.headStartTime = cg.time;
        cg.headEndTime = cg.time + 100 + random() * 2000;
    } else {
        if (cg.time >= cg.headEndTime) {
            cg.headStartYaw = cg.headEndYaw;
            cg.headStartPitch = cg.headEndPitch;
            cg.headStartTime = cg.headEndTime;
            cg.headEndTime = cg.time + 100 + random() * 2000;

            cg.headEndYaw = 180 + 20 * cos(crandom() * M_PI);
            cg.headEndPitch = 5 * cos(crandom() * M_PI);
        }
    }

    if (cg.headStartTime > cg.time) {
        cg.headStartTime = cg.time;
    }

    frac = (cg.time - cg.headStartTime) / (float)(cg.headEndTime - cg.headStartTime);
    frac = frac * frac * (3 - 2 * frac);
    angles[YAW] = cg.headStartYaw + (cg.headEndYaw - cg.headStartYaw) * frac;
    angles[PITCH] = cg.headStartPitch + (cg.headEndPitch - cg.headStartPitch) * frac;

    CG_DrawHead(x, rect->y, rect->w, rect->h, cg.snap->ps.clientNum, angles, NULL);
}

static void CG_DrawSelectedPlayerHealth(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    clientInfo_t* ci;
    int value;
    char num[16];

    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        if (shader) {
            trap_R_SetColor(color);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
            trap_R_SetColor(NULL);
        } else {
            Com_sprintf(num, sizeof(num), "%i", ci->health);
            value = CG_OwnerDrawTextWidth(num, scale, 0);
            CG_OwnerDrawText(rect->x + (rect->w - value) / 2, rect->y, scale, color, num, 0, 0, textStyle);
        }
    }
}

static void CG_DrawSelectedPlayerArmor(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    clientInfo_t* ci;
    int value;
    char num[16];
    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        if (ci->armor > 0) {
            if (shader) {
                trap_R_SetColor(color);
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
                trap_R_SetColor(NULL);
            } else {
                Com_sprintf(num, sizeof(num), "%i", ci->armor);
                value = CG_OwnerDrawTextWidth(num, scale, 0);
                CG_OwnerDrawText(rect->x + (rect->w - value) / 2, rect->y, scale, color, num, 0, 0, textStyle);
            }
        }
    }
}

qhandle_t CG_StatusHandle(int task) {
    qhandle_t h;
    switch (task) {
        case TEAMTASK_OFFENSE:
            h = cgs.media.assaultShader;
            break;
        case TEAMTASK_DEFENSE:
            h = cgs.media.defendShader;
            break;
        case TEAMTASK_PATROL:
            h = cgs.media.patrolShader;
            break;
        case TEAMTASK_FOLLOW:
            h = cgs.media.followShader;
            break;
        case TEAMTASK_CAMP:
            h = cgs.media.campShader;
            break;
        case TEAMTASK_RETRIEVE:
            h = cgs.media.retrieveShader;
            break;
        case TEAMTASK_ESCORT:
            h = cgs.media.escortShader;
            break;
        default:
            h = cgs.media.assaultShader;
            break;
    }
    return h;
}

static void CG_DrawSelectedPlayerStatus(rectDef_t* rect) {
    clientInfo_t* ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        qhandle_t h;
        if (cgs.orderPending) {
            if (cg.time > cgs.orderTime - 2500 && (cg.time >> 9) & 1) {
                return;
            }
            h = CG_StatusHandle(cgs.currentOrder);
        } else {
            h = CG_StatusHandle(ci->teamTask);
        }
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, h);
    }
}

static void CG_DrawPlayerStatus(rectDef_t* rect) {
    clientInfo_t* ci = &cgs.clientinfo[cg.snap->ps.clientNum];
    if (ci) {
        qhandle_t h = CG_StatusHandle(ci->teamTask);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, h);
    }
}

static void CG_DrawSelectedPlayerName(rectDef_t* rect, float scale, vec4_t color, qboolean voice, int textStyle) {
    clientInfo_t* ci;
    ci = cgs.clientinfo + ((voice) ? cgs.currentVoiceClient : sortedTeamPlayers[CG_GetSelectedPlayer()]);
    if (ci) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, ci->name, 0, 0, textStyle);
    }
}

static void CG_DrawSelectedPlayerLocation(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    clientInfo_t* ci;
    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        const char* p = CG_ConfigString(CS_LOCATIONS + ci->location);
        if (!p || !*p) {
            p = "unknown";
        }
        CG_OwnerDrawText(rect->x, rect->y, scale, color, p, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerLocation(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    clientInfo_t* ci = &cgs.clientinfo[cg.snap->ps.clientNum];
    if (ci) {
        const char* p = CG_ConfigString(CS_LOCATIONS + ci->location);
        if (!p || !*p) {
            p = "unknown";
        }
        CG_OwnerDrawText(rect->x, rect->y, scale, color, p, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerScore(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    char num[16];
    int value = cg.snap->ps.persistant[PERS_SCORE];

    if (shader) {
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
        trap_R_SetColor(NULL);
    } else {
        Com_sprintf(num, sizeof(num), "%i", value);
        value = CG_OwnerDrawTextWidth(num, scale, 0);
        CG_OwnerDrawText(rect->x + (rect->w - value) / 2, rect->y, scale, color, num, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerItem(rectDef_t* rect, float scale, qboolean draw2D) {
    int value;
    vec3_t origin, angles;

    value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
    if (value) {
        CG_RegisterItemVisuals(value);

        if (qtrue) {
            CG_RegisterItemVisuals(value);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cg_items[value].icon);
        } else {
            VectorClear(angles);
            origin[0] = 90;
            origin[1] = 0;
            origin[2] = -10;
            angles[YAW] = (cg.time & 2047) * 360 / 2048.0;
            CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, cg_items[value].models[0], 0, origin, angles, NULL);
        }
    }
}

static void CG_DrawSelectedPlayerPowerup(rectDef_t* rect, qboolean draw2D) {
    clientInfo_t* ci;
    int j;
    float x, y;

    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        x = rect->x;
        y = rect->y;

        for (j = 0; j < PW_NUM_POWERUPS; j++) {
            if ((ci->powerups & (1 << j)) && j != PW_FLIGHT && j != PW_FREEZE) {
                gitem_t* item;
                item = BG_FindItemForPowerup(j);
                if (item) {
                    CG_DrawPic(x, y, rect->w, rect->h, trap_R_RegisterShader(item->icon));
                    return;
                }
            }
        }
    }
}

static void CG_DrawSelectedPlayerHead(rectDef_t* rect, qboolean draw2D, qboolean voice) {
    clipHandle_t cm;
    clientInfo_t* ci;
    float len;
    vec3_t origin;
    vec3_t mins, maxs, angles;

    ci = cgs.clientinfo + ((voice) ? cgs.currentVoiceClient : sortedTeamPlayers[CG_GetSelectedPlayer()]);

    if (ci) {
        if (cg_draw3dIcons.integer) {
            cm = ci->headModel;
            if (!cm) {
                return;
            }

            trap_R_ModelBounds(cm, mins, maxs);

            origin[2] = -0.5 * (mins[2] + maxs[2]);
            origin[1] = 0.5 * (mins[1] + maxs[1]);

            len = 0.7 * (maxs[2] - mins[2]);
            origin[0] = len / 0.268;

            VectorAdd(origin, ci->headOffset, origin);

            angles[PITCH] = 0;
            angles[YAW] = 180;
            angles[ROLL] = 0;

            CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, ci->headModel, ci->headSkin, origin, angles, NULL);
        } else if (cg_drawIcons.integer) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, ci->modelIcon);
        }

        if (ci->deferred) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.deferShader);
        }
    }
}

static void CG_DrawPlayerHealth(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle, int align) {
    playerState_t* ps;
    int value;
    float tx, ty;
    char num[16];

    ps = &cg.snap->ps;

    value = ps->stats[STAT_HEALTH];

    if (shader) {
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
        trap_R_SetColor(NULL);
    } else {
        Com_sprintf(num, sizeof(num), "%i", value);
        tx = rect->x;
        if (align == ITEM_ALIGN_CENTER) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0) * 0.5f;
        } else if (align == ITEM_ALIGN_RIGHT) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0);
        }
        ty = rect->y + CG_OwnerDrawTextHeight(num, scale, 0);
        CG_OwnerDrawText(tx, ty, scale, color, num, 0, 0, textStyle);
    }
}

static qboolean CG_PlaceIsIndividual(void) {
    return cgs.gametype >= GT_FFA && (cgs.gametype <= GT_RACE || cgs.gametype == GT_RR);
}

static void CG_DrawPlaceLine(rectDef_t* rect, float scale, vec4_t color, int textStyle,
                             const char* place, const char* name, const char* score) {
    int placeW, scoreW, dotsW;
    int x, limit;
    float maxX;

    placeW = CG_DrawTextWidth(place, scale, 0, 0);
    scoreW = CG_DrawTextWidth(score, scale, 0, 0);
    dotsW = CG_DrawTextWidth("...", scale, 0, 0);
    limit = (int)(rect->w - placeW - scoreW - dotsW);

    x = (int)rect->x;
    CG_DrawText((float)x, rect->y, 0, scale, color, place, 0, 0, textStyle);

    x += placeW;
    limit += x;
    maxX = (float)limit;
    CG_DrawTextClipped(&maxX, (float)x, rect->y, scale, color, name);

    if (maxX == (float)limit) {
        color[3] = 0.0f;
    }
    CG_DrawText((float)limit, rect->y, 0, scale, color, "...", 0, 0, textStyle);
    color[3] = 1.0f;
    CG_DrawText((float)(limit + dotsW), rect->y, 0, scale, color, score, 0, 0, textStyle);
}

static void CG_DrawPlaceTeamLine(rectDef_t* rect, float scale, vec4_t color, int textStyle,
                                 const char* teamName, int score) {
    char text[64];
    char num[4];

    Com_sprintf(text, sizeof(text), "      %s", teamName);
    CG_DrawText(rect->x, rect->y, 0, scale, color, text, 0, 0, textStyle);

    Q_strncpyz(num, va("%d", score), sizeof(num));
    CG_DrawText(rect->x + (rect->w - CG_DrawTextWidth(num, scale, 0, 0)), rect->y, 0, scale, color, num, 0, 0,
                textStyle);
}

static void CG_DrawFirstPlaceScore(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    char scoreText[15];
    char teamName[64];
    int score = cgs.scores1;

    if (score == SCORE_NOT_PRESENT) {
        if (CG_PlaceIsIndividual()) {
            CG_DrawText(rect->x, rect->y, 0, scale, color, "1.", 0, 0, textStyle);
        } else {
            Com_sprintf(teamName, sizeof(teamName), "      %s",
                        cg_redTeamName.string[0] ? "Red Team" : cg_redTeamName.string);
            CG_DrawText(rect->x, rect->y, 0, scale, color, teamName, 0, 0, textStyle);
        }
        return;
    }

    if (CG_PlaceIsIndividual()) {
        if (cgs.gametype == GT_RACE) {
            if (score != 0x7fffffff && score >= 0) {
                Q_strncpyz(scoreText, va(" %s", CG_FormatRaceTime(score)), sizeof(scoreText));
            } else {
                Q_strncpyz(scoreText, " -", sizeof(scoreText));
            }
        } else {
            Q_strncpyz(scoreText, va(" %d", score), sizeof(scoreText));
        }
        CG_DrawPlaceLine(rect, scale, color, textStyle, "1. ", cgs.firstPlaceName, scoreText);
        return;
    }

    if (cgs.scores1 >= cgs.scores2) {
        if (cg_redTeamName.string[0]) {
            Q_strncpyz(teamName, cg_redTeamName.string, sizeof(teamName));
        } else {
            Com_sprintf(teamName, sizeof(teamName), "%s^7", "Red Team");
        }
    } else {
        if (cg_blueTeamName.string[0]) {
            Q_strncpyz(teamName, cg_blueTeamName.string, sizeof(teamName));
        } else {
            Com_sprintf(teamName, sizeof(teamName), "%s^7", "Blue Team");
        }
        score = cgs.scores2;
    }
    CG_DrawPlaceTeamLine(rect, scale, color, textStyle, teamName, score);
}

static void CG_DrawSecondPlaceScore(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    char scoreText[15];
    char placeText[5];
    char name[40];
    char teamName[64];
    int score = cgs.scores2;
    int clientNum, rank, place;

    if (score == SCORE_NOT_PRESENT) {
        if (!CG_PlaceIsIndividual()) {
            Com_sprintf(teamName, sizeof(teamName), "      %s",
                        cg_blueTeamName.string[0] ? "Blue Team" : cg_blueTeamName.string);
            CG_DrawText(rect->x, rect->y, 0, scale, color, teamName, 0, 0, textStyle);
        }
        return;
    }

    if (CG_PlaceIsIndividual()) {
        clientNum = cg.snap->ps.clientNum;
        rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;

        if (!CG_LocalPlayerLeads() && cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
            Q_strncpyz(name, cgs.clientinfo[clientNum].name, sizeof(name));
            place = rank + 1;
            if (cgs.gametype == GT_RACE) {
                score = cgs.clientinfo[clientNum].score;
            } else {
                score = cg.snap->ps.persistant[PERS_SCORE];
            }
        } else {
            Q_strncpyz(name, cgs.secondPlaceName, sizeof(name));
            place = (cgs.scores1 != cgs.scores2) ? 2 : 1;
            score = cgs.scores2;
        }

        Q_strncpyz(placeText, va("%d. ", place), sizeof(placeText));
        if (cgs.gametype == GT_RACE) {
            if (score != 0x7fffffff && score > 0) {
                Q_strncpyz(scoreText, va(" %s", CG_FormatRaceTime(score)), sizeof(scoreText));
            } else {
                Q_strncpyz(scoreText, " -", sizeof(scoreText));
            }
        } else {
            Q_strncpyz(scoreText, va(" %d", score), sizeof(scoreText));
        }
        CG_DrawPlaceLine(rect, scale, color, textStyle, placeText, name, scoreText);
        return;
    }

    if (cgs.scores1 < cgs.scores2) {
        if (cg_redTeamName.string[0]) {
            Q_strncpyz(teamName, cg_redTeamName.string, sizeof(teamName));
        } else {
            Com_sprintf(teamName, sizeof(teamName), "%s^7           ", "Red Team");
        }
        score = cgs.scores1;
    } else {
        if (cg_blueTeamName.string[0]) {
            Q_strncpyz(teamName, cg_blueTeamName.string, sizeof(teamName));
        } else {
            Com_sprintf(teamName, sizeof(teamName), "%s^7           ", "Blue Team");
        }
    }
    CG_DrawPlaceTeamLine(rect, scale, color, textStyle, teamName, score);
}

static void CG_DrawBlueFlagName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_RED && cgs.clientinfo[i].powerups & (1 << PW_BLUEFLAG)) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color, cgs.clientinfo[i].name, 0, 0, textStyle);
            return;
        }
    }
}

static void CG_DrawBlueFlagHead(rectDef_t* rect) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_RED && cgs.clientinfo[i].powerups & (1 << PW_BLUEFLAG)) {
            vec3_t angles;
            VectorClear(angles);
            angles[YAW] = 180 + 20 * sin(cg.time / 650.0);
            ;
            CG_DrawHead(rect->x, rect->y, rect->w, rect->h, 0, angles, NULL);
            return;
        }
    }
}

static void CG_DrawRedFlagName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_BLUE && cgs.clientinfo[i].powerups & (1 << PW_REDFLAG)) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color, cgs.clientinfo[i].name, 0, 0, textStyle);
            return;
        }
    }
}

static void CG_DrawRedFlagHead(rectDef_t* rect) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_BLUE && cgs.clientinfo[i].powerups & (1 << PW_REDFLAG)) {
            vec3_t angles;
            VectorClear(angles);
            angles[YAW] = 180 + 20 * sin(cg.time / 650.0);
            ;
            CG_DrawHead(rect->x, rect->y, rect->w, rect->h, 0, angles, NULL);
            return;
        }
    }
}

static void CG_HarvesterSkulls(rectDef_t* rect, float scale, vec4_t color, qboolean force2D, int textStyle) {
    char num[16];
    vec3_t origin, angles;
    qhandle_t handle;
    int value = cg.snap->ps.generic1;

    if (cgs.gametype != GT_HARVESTER) {
        return;
    }

    if (value > 99) {
        value = 99;
    }

    Com_sprintf(num, sizeof(num), "%i", value);
    value = CG_OwnerDrawTextWidth(num, scale, 0);
    CG_OwnerDrawText(rect->x + (rect->w - value), rect->y, scale, color, num, 0, 0, textStyle);

    if (cg_drawIcons.integer) {
        if (!force2D && cg_draw3dIcons.integer) {
            VectorClear(angles);
            origin[0] = 90;
            origin[1] = 0;
            origin[2] = -10;
            angles[YAW] = (cg.time & 2047) * 360 / 2048.0;
            if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
                handle = cgs.media.redCubeModel;
            } else {
                handle = cgs.media.blueCubeModel;
            }
            CG_Draw3DModel(rect->x, rect->y, 35, 35, handle, 0, origin, angles, NULL);
        } else {
            if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
                handle = cgs.media.redCubeIcon;
            } else {
                handle = cgs.media.blueCubeIcon;
            }
            CG_DrawPic(rect->x + 3, rect->y + 16, 20, 20, handle);
        }
    }
}

static void CG_OneFlagStatus(rectDef_t* rect) {
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    int icon = cgs.flagStatus;
    float offset = 0.0f;

    if (cgs.gametype != GT_1FCTF || !BG_FindItemForPowerup(PW_NEUTRALFLAG) || cgs.flagStatus > FLAG_TAKEN_BLUE) {
        return;
    }
    if (cgs.flagStatus == FLAG_TAKEN_RED) {
        icon = 3;
        offset = (cgs.scores1 == SCORE_NOT_PRESENT || cgs.scores1 < cgs.scores2) ? 9.0f : -9.0f;
    } else if (cgs.flagStatus == FLAG_TAKEN_BLUE) {
        icon = 3;
        offset = (cgs.scores2 == SCORE_NOT_PRESENT || cgs.scores2 <= cgs.scores1) ? 9.0f : -9.0f;
    }
    trap_R_SetColor(white);
    CG_DrawPic(rect->x, rect->y + offset, rect->w, rect->h, cgs.media.flagStatusHandles[icon]);
    trap_R_SetColor(NULL);
}

static gitem_t* CG_FindItemByTypeTag(int type, int tag) {
    int i;

    for (i = 1; i < bg_numItems; i++) {
        if (bg_itemlist[i].giType == type && bg_itemlist[i].giTag == tag) {
            return &bg_itemlist[i];
        }
    }
    return NULL;
}

static void CG_DrawPersistantPowerUp(rectDef_t* rect) {
    gitem_t* item;
    int index;

    if (!cg.snap->ps.stats[STAT_RUNE]) {
        return;
    }
    item = CG_FindItemByTypeTag(IT_PERSISTANT_POWERUP, cg.snap->ps.stats[STAT_RUNE]);
    if (!item) {
        return;
    }
    index = (int)(item - bg_itemlist);
    CG_RegisterItemVisuals(index);
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cg_items[index].icon);
}

static void CG_DrawPlayerHasKey(rectDef_t* rect) {
    static const int keyBits[3] = {1, 2, 4};
    float xoff = 0.0f;
    gitem_t* item;
    int i, index;

    for (i = 0; i < 3; i++) {
        if (!(cg.snap->ps.stats[STAT_KEY] & keyBits[i])) {
            continue;
        }
        item = CG_FindItemByTypeTag(IT_KEY, keyBits[i]);
        if (!item) {
            continue;
        }
        index = (int)(item - bg_itemlist);
        CG_RegisterItemVisuals(index);
        CG_DrawPic(rect->x + xoff, rect->y, rect->w, rect->h, cg_items[index].icon);
        xoff += rect->w * 0.5f;
    }
}

static void CG_DrawTeamColor(rectDef_t* rect, vec4_t color) {
    CG_DrawTeamBackground(rect->x, rect->y, rect->w, rect->h, color[3], cg.snap->ps.persistant[PERS_TEAM]);
}

static void CG_DrawAreaPowerUp(rectDef_t* rect, int align, float special, float scale, vec4_t color) {
    char num[16];
    int sorted[MAX_POWERUPS];
    int sortedTime[MAX_POWERUPS];
    int i, j, k;
    int active;
    playerState_t* ps;
    int t;
    gitem_t* item;
    float f;
    rectDef_t r2;
    float* inc;
    r2.x = rect->x;
    r2.y = rect->y;
    r2.w = rect->w;
    r2.h = rect->h;

    inc = (align == HUD_VERTICAL) ? &r2.y : &r2.x;

    ps = &cg.snap->ps;

    if (ps->stats[STAT_HEALTH] <= 0) {
        return;
    }

    active = 0;
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (!ps->powerups[i] || i == PW_FLIGHT || i == PW_FREEZE) {
            continue;
        }

        if (ps->powerups[i] == INT_MAX) {
            continue;
        }

        t = ps->powerups[i] - cg.time;
        if (t <= 0) {
            continue;
        }

        for (j = 0; j < active; j++) {
            if (sortedTime[j] >= t) {
                for (k = active - 1; k >= j; k--) {
                    sorted[k + 1] = sorted[k];
                    sortedTime[k + 1] = sortedTime[k];
                }
                break;
            }
        }
        sorted[j] = i;
        sortedTime[j] = t;
        active++;
    }

    for (i = 0; i < active; i++) {
        item = BG_FindItemForPowerup(sorted[i]);

        if (item) {
            t = ps->powerups[sorted[i]];
            if (t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME) {
                trap_R_SetColor(NULL);
            } else {
                vec4_t modulate;

                f = (float)(t - cg.time) / POWERUP_BLINK_TIME;
                f -= (int)f;
                modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
                trap_R_SetColor(modulate);
            }

            CG_DrawPic(r2.x, r2.y, r2.w * .75, r2.h, trap_R_RegisterShader(item->icon));

            Com_sprintf(num, sizeof(num), "%i", sortedTime[i] / 1000);
            CG_OwnerDrawText(r2.x + (r2.w * .75) + 3, r2.y + r2.h, scale, color, num, 0, 0, 0);
            *inc += r2.w + special;
        }
    }
    trap_R_SetColor(NULL);
}

float CG_GetValue(int ownerDraw) {
    centity_t* cent;
    playerState_t* ps;

    cent = &cg_entities[cg.snap->ps.clientNum];
    ps = &cg.snap->ps;

    switch (ownerDraw) {
        case CG_PLAYER_ARMOR_VALUE:
            return ps->stats[STAT_ARMOR];
            break;
        case CG_PLAYER_AMMO_VALUE:
            if (cent->currentState.weapon) {
                return ps->ammo[cent->currentState.weapon];
            }
            break;
        case CG_PLAYER_SCORE:
            return cg.snap->ps.persistant[PERS_SCORE];
            break;
        case CG_PLAYER_HEALTH:
            return ps->stats[STAT_HEALTH];
            break;
        case CG_RED_SCORE:
            return cgs.scores1;
            break;
        case CG_BLUE_SCORE:
            return cgs.scores2;
            break;
        default:
            break;
    }
    return -1;
}

qboolean CG_OtherTeamHasFlag(void) {
    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_AD) {
        int team = cg.snap->ps.persistant[PERS_TEAM];
        if (cgs.gametype == GT_1FCTF) {
            if (team == TEAM_RED && cgs.flagStatus == FLAG_TAKEN_BLUE) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.flagStatus == FLAG_TAKEN_RED) {
                return qtrue;
            } else {
                return qfalse;
            }
        } else {
            if (team == TEAM_RED && cgs.redflag == FLAG_TAKEN) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.blueflag == FLAG_TAKEN) {
                return qtrue;
            } else {
                return qfalse;
            }
        }
    }
    return qfalse;
}

qboolean CG_YourTeamHasFlag(void) {
    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_AD) {
        int team = cg.snap->ps.persistant[PERS_TEAM];
        if (cgs.gametype == GT_1FCTF) {
            if (team == TEAM_RED && cgs.flagStatus == FLAG_TAKEN_RED) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.flagStatus == FLAG_TAKEN_BLUE) {
                return qtrue;
            } else {
                return qfalse;
            }
        } else {
            if (team == TEAM_RED && cgs.blueflag == FLAG_TAKEN) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.redflag == FLAG_TAKEN) {
                return qtrue;
            } else {
                return qfalse;
            }
        }
    }
    return qfalse;
}

static qboolean CG_PlayerIsFirstPlace(void) {
    const playerState_t* ps;

    if (!cg.snap) {
        return qfalse;
    }
    ps = &cg.snap->ps;

    if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return qfalse;
    }

    if (cgs.gametype < GT_TEAM || cgs.gametype == GT_RR) {
        return (ps->persistant[PERS_RANK] == 0) ? qtrue : qfalse;
    }

    switch (cgs.clientinfo[ps->clientNum].team) {
        case TEAM_RED:
            return (cgs.scores1 >= cgs.scores2) ? qtrue : qfalse;
        case TEAM_BLUE:
            return (cgs.scores2 > cgs.scores1) ? qtrue : qfalse;
        default:
            return qfalse;
    }
}

static const struct {
    int flag;
    int slot;
} cg_duelFiredFlags[] = {
    {CG_SHOW_IF_MG_FIRED, 1},  {CG_SHOW_IF_SG_FIRED, 2},  {CG_SHOW_IF_GL_FIRED, 3},
    {CG_SHOW_IF_RL_FIRED, 4},  {CG_SHOW_IF_LG_FIRED, 5},  {CG_SHOW_IF_RG_FIRED, 6},
    {CG_SHOW_IF_PG_FIRED, 7},  {CG_SHOW_IF_BFG_FIRED, 8}, {CG_SHOW_IF_CG_FIRED, 12},
    {CG_SHOW_IF_NG_FIRED, 10}, {CG_SHOW_IF_PL_FIRED, 11}, {CG_SHOW_IF_HMG_FIRED, 13},
};

qboolean CG_OwnerDrawVisible(int flags, int flags2) {
    const playerState_t* ps;
    int pmType;
    int i;

    if (flags & CG_SHOW_TEAMINFO) {
        return (cg_currentSelectedPlayer.integer == numSortedTeamPlayers);
    }

    if (flags & CG_SHOW_NOTEAMINFO) {
        return !(cg_currentSelectedPlayer.integer == numSortedTeamPlayers);
    }

    if (flags & CG_SHOW_OTHERTEAMHASFLAG) {
        return CG_OtherTeamHasFlag();
    }

    if (flags & CG_SHOW_YOURTEAMHASENEMYFLAG) {
        return CG_YourTeamHasFlag();
    }

    if (flags & (CG_SHOW_BLUE_TEAM_HAS_REDFLAG | CG_SHOW_RED_TEAM_HAS_BLUEFLAG)) {
        if (cgs.gametype == GT_1FCTF) {
            if ((flags & CG_SHOW_BLUE_TEAM_HAS_REDFLAG) && cgs.flagStatus == FLAG_TAKEN_BLUE) {
                return qtrue;
            }
            if ((flags & CG_SHOW_RED_TEAM_HAS_BLUEFLAG) && cgs.flagStatus == FLAG_TAKEN_RED) {
                return qtrue;
            }
        } else {
            if ((flags & CG_SHOW_BLUE_TEAM_HAS_REDFLAG) &&
                (cgs.redflag == FLAG_TAKEN || cgs.flagStatus == FLAG_TAKEN_RED)) {
                return qtrue;
            }
            if ((flags & CG_SHOW_RED_TEAM_HAS_BLUEFLAG) &&
                (cgs.blueflag == FLAG_TAKEN || cgs.flagStatus == FLAG_TAKEN_BLUE)) {
                return qtrue;
            }
            return qfalse;
        }
    }

    if (flags & CG_SHOW_PLAYERS_REMAINING) {
        if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE || cgs.gametype == GT_AD ||
            cgs.gametype == GT_RR) {
            return qtrue;
        }
    }

    if ((flags & CG_SHOW_DUEL) && cgs.gametype == GT_DUEL) {
        return qtrue;
    }

    if (flags & CG_SHOW_CLAN_ARENA) {
        return (cgs.gametype == GT_CA) ? qtrue : qfalse;
    }

    if ((flags & CG_SHOW_CTF) && cgs.gametype == GT_CTF) {
        return qtrue;
    }

    if (flags & CG_SHOW_ONEFLAG) {
        return (cgs.gametype == GT_1FCTF) ? qtrue : qfalse;
    }

    if (flags & CG_SHOW_HARVESTER) {
        return (cgs.gametype == GT_HARVESTER) ? qtrue : qfalse;
    }

    if (flags & CG_SHOW_DOMINATION) {
        return (cgs.gametype == GT_DOMINATION) ? qtrue : qfalse;
    }

    if ((flags & CG_SHOW_ANYNONTEAMGAME) && cgs.gametype < GT_TEAM) {
        return qtrue;
    }

    if ((flags & CG_SHOW_ANYTEAMGAME) && cgs.gametype >= GT_TEAM) {
        return qtrue;
    }

    ps = cg.snap ? &cg.snap->ps : NULL;

    if ((flags & CG_SHOW_HEALTHCRITICAL) && ps && ps->stats[STAT_HEALTH] < 25) {
        return qtrue;
    }

    if ((flags & CG_SHOW_IF_PLAYER_HAS_FLAG) && ps &&
        (ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG] || ps->powerups[PW_NEUTRALFLAG])) {
        return qtrue;
    }

    pmType = cg.predictedPlayerState.pm_type;

    if ((flags & CG_SHOW_INTERMISSION) && pmType == PM_INTERMISSION) {
        return qtrue;
    }

    if ((flags & CG_SHOW_NOTINTERMISSION) && pmType != PM_INTERMISSION) {
        return qtrue;
    }

    if (flags & CG_SHOW_IF_MSG_PRESENT) {
        return (cg.lastChatTime && (cg.time & 0x200)) ? qtrue : qfalse;
    }

    if (flags & CG_SHOW_IF_NOTICE_PRESENT) {
        return (cg.lastChatTime2 && (cg.time & 0x200)) ? qtrue : qfalse;
    }

    if (flags & CG_SHOW_IF_CHAT_VISIBLE) {
        return (cg.chatHistoryShowing || cg.scoreBoardShowing) ? qtrue : qfalse;
    }

    if (flags & CG_SHOW_IF_PLYR_IS_FIRST_PLACE) {
        return CG_PlayerIsFirstPlace();
    }

    if (flags & CG_SHOW_IF_PLYR_IS_NOT_FIRST_PLACE) {
        return CG_PlayerIsFirstPlace() ? qfalse : qtrue;
    }

    if (flags & CG_SHOW_IF_RED_IS_FIRST_PLACE) {
        return (cgs.scores1 == SCORE_NOT_PRESENT || cgs.scores1 >= cgs.scores2) ? qtrue : qfalse;
    }

    if (flags & CG_SHOW_IF_BLUE_IS_FIRST_PLACE) {
        return (cgs.scores2 != SCORE_NOT_PRESENT && cgs.scores2 > cgs.scores1) ? qtrue : qfalse;
    }

    if ((flags & CG_SHOW_IF_PLYR_IS_ON_RED) && ps && ps->persistant[PERS_TEAM] == TEAM_RED) {
        return qtrue;
    }

    if ((flags & CG_SHOW_IF_PLYR_IS_ON_BLUE) && ps && ps->persistant[PERS_TEAM] == TEAM_BLUE) {
        return qtrue;
    }

    if ((flags & CG_SHOW_IF_WARMUP) && cg.warmup < 0) {
        return qtrue;
    }

    if ((flags & CG_SHOW_IF_NOT_WARMUP) && cg.warmup >= 0) {
        return qtrue;
    }

    if (ps) {
        if ((flags2 & CG_SHOW_IF_PLYR1) &&
            (ps->clientNum == cg.duelScores[0].clientNum || pmType == PM_SPECTATOR)) {
            return qtrue;
        }

        if ((flags2 & CG_SHOW_IF_PLYR2) &&
            (ps->clientNum == cg.duelScores[1].clientNum || pmType == PM_SPECTATOR)) {
            return qtrue;
        }

        if ((flags2 & CG_SHOW_IF_PLYR_IS_ON_RED_OR_SPEC) &&
            (ps->persistant[PERS_TEAM] == TEAM_RED || pmType == PM_SPECTATOR)) {
            return qtrue;
        }

        if ((flags2 & CG_SHOW_IF_PLYR_IS_ON_BLUE_OR_SPEC) &&
            (ps->persistant[PERS_TEAM] == TEAM_BLUE || pmType == PM_SPECTATOR)) {
            return qtrue;
        }

        if ((flags2 & CG_SHOW_IF_PLYR_IS_ON_RED_NO_SPEC) &&
            ps->persistant[PERS_TEAM] == TEAM_RED && pmType != PM_SPECTATOR) {
            return qtrue;
        }

        if ((flags2 & CG_SHOW_IF_PLYR_IS_ON_BLUE_NO_SPEC) &&
            ps->persistant[PERS_TEAM] == TEAM_BLUE && pmType != PM_SPECTATOR) {
            return qtrue;
        }
    }

    if ((flags2 & CG_SHOW_IF_LOADOUT_ENABLED) && cg_loadout.integer) {
        return qtrue;
    }

    if ((flags2 & CG_SHOW_IF_LOADOUT_DISABLED) && !cg_loadout.integer) {
        return qtrue;
    }

    for (i = 0; i < 2; i++) {
        int duelist = cg.duelScores[i].clientNum;

        if ((flags2 & (i == 0 ? CG_SHOW_IF_1ST_PLYR_FOLLOWED : CG_SHOW_IF_2ND_PLYR_FOLLOWED)) && ps &&
            duelist >= 0 && duelist < MAX_CLIENTS &&
            cg_entities[duelist].currentState.clientNum == ps->clientNum) {
            return qtrue;
        }
    }

    if ((flags2 & CG_SHOW_IF_G_FIRED) &&
        (cg.duelScores[0].weaponStats[0].damage || cg.duelScores[1].weaponStats[0].damage)) {
        return qtrue;
    }

    for (i = 0; i < (int)ARRAY_LEN(cg_duelFiredFlags); i++) {
        int slot = cg_duelFiredFlags[i].slot;

        if ((flags2 & cg_duelFiredFlags[i].flag) &&
            (cg.duelScores[0].weaponStats[slot].atts || cg.duelScores[1].weaponStats[slot].atts)) {
            return qtrue;
        }
    }

    return qfalse;
}

static void CG_DrawPlayerHasFlag(rectDef_t* rect, qboolean force2D) {
    int adj = (force2D) ? 0 : 2;
    if (cg.predictedPlayerState.powerups[PW_REDFLAG]) {
        CG_DrawFlagModel(rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_RED, force2D);
    } else if (cg.predictedPlayerState.powerups[PW_BLUEFLAG]) {
        CG_DrawFlagModel(rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_BLUE, force2D);
    } else if (cg.predictedPlayerState.powerups[PW_NEUTRALFLAG]) {
        CG_DrawFlagModel(rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_FREE, force2D);
    }
}

static void CG_DrawAreaSystemChat(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, systemChat, 0, 0, 0);
}

static void CG_DrawAreaTeamChat(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, teamChat1, 0, 0, 0);
}

const char* CG_GetKillerText(void) {
    const char* s = "";
    if (cg.killerName[0]) {
        s = va("Fragged by %s", cg.killerName);
    }
    return s;
}

static void CG_DrawKiller(rectDef_t* rect, float scale, vec4_t color, int textStyle, int align) {
    const char* s = CG_GetKillerText();
    int w = CG_DrawTextWidth(s, scale, 0, 0);
    float x = rect->x;

    if (align == ITEM_ALIGN_CENTER) {
        x -= w * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= w;
    }
    CG_DrawText(x, rect->y + rect->h, 0, scale, color, s, 0, 0, textStyle);
    CG_DrawPic(x + w + 8, rect->y - 4, 16, 16, cg.killerIcon);
}

static void CG_DrawCapFragLimit(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const char *s;
    int limit;
    float x = rect->x;

    switch (cgs.gametype) {
    case GT_RACE:
        limit = CG_RaceCheckpointsLeft();
        break;
    case GT_CTF:
    case GT_1FCTF:
    case GT_OBELISK:
    case GT_HARVESTER:
        limit = cgs.capturelimit;
        break;
    case GT_CA:
    case GT_FREEZE:
    case GT_RR:
        limit = cgs.roundlimit;
        break;
    case GT_DOMINATION:
    case GT_AD:
        limit = cgs.scorelimit;
        break;
    default:
        limit = cgs.fraglimit;
        break;
    }

    s = va("%i", limit);
    if (align == ITEM_ALIGN_CENTER) {
        x -= (float)CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= (float)CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_Draw1stPlace(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    if (cgs.scores1 != SCORE_NOT_PRESENT) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%2i", cgs.scores1), 0, 0, textStyle);
    }
}

static void CG_Draw2ndPlace(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    if (cgs.scores2 != SCORE_NOT_PRESENT) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%2i", cgs.scores2), 0, 0, textStyle);
    }
}

const char* CG_GetGameStatusText(void) {
    const char* s = "";

    if (cgs.gametype == GT_RACE) {
        return s;
    }

    if (cgs.gametype < GT_TEAM || cgs.gametype == GT_RR) {
        if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
            s = va("%s place with %i", CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1), cg.snap->ps.persistant[PERS_SCORE]);
        }
    } else {
        if (cg.teamScores[0] == cg.teamScores[1]) {
            s = va("Teams are tied at %i", cg.teamScores[0]);
        } else if (cg.teamScores[0] >= cg.teamScores[1]) {
            s = va("Red leads Blue, %i to %i", cg.teamScores[0], cg.teamScores[1]);
        } else {
            s = va("Blue leads Red, %i to %i", cg.teamScores[1], cg.teamScores[0]);
        }
    }
    return s;
}

const char* CG_GetMatchStatusText(void) {
    const char* prefix;

    if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        prefix = "MATCH SUMMARY";
    } else if (cg.warmup) {
        prefix = "MATCH WARMUP";
    } else {
        prefix = "MATCH IN PROGRESS";
    }

    if (cgs.gametype == GT_RACE) {
        return prefix;
    }

    if (cgs.gametype < GT_TEAM || cgs.gametype == GT_RR) {
        if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
            return va("%s - %s place with %i", prefix,
                CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
                cg.snap->ps.persistant[PERS_SCORE]);
        }
        return prefix;
    }

    if (cg.teamScores[0] == cg.teamScores[1]) {
        return va("%s - Teams are tied at %i", prefix, cg.teamScores[0]);
    } else if (cg.teamScores[0] > cg.teamScores[1]) {
        return va("%s - ^1Red^7 leads ^4Blue^7, %i to %i", prefix,
            cg.teamScores[0], cg.teamScores[1]);
    } else {
        return va("%s - ^4Blue^7 leads ^1Red^7, %i to %i", prefix,
            cg.teamScores[1], cg.teamScores[0]);
    }
}

static void CG_DrawGameStatus(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y + rect->h, scale, color, CG_GetGameStatusText(), 0, 0, textStyle);
}

const char* CG_GameTypeString(void) {
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        return gametypeDisplayNames[cgs.gametype];
    }
    return "Unknown Gametype";
}

static void CG_DrawGameType(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, CG_GameTypeString(), 0, 0, textStyle);
}

static void CG_Text_Paint_Limit(float* maxX, float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit) {
    int w640;
    float avail = *maxX - x;
    (void)adjust;

    if (!text || !*text)
        return;

    w640 = CG_Text_Width(text, scale, limit);
    if (avail > 0.0f && (float)w640 > avail) {
        int full = (limit > 0) ? limit : (int)strlen(text);
        int fit = (int)((float)full * (avail / (float)w640));
        if (fit < 0)
            fit = 0;
        limit = fit;
        w640 = CG_Text_Width(text, scale, limit);
    }

    CG_Text_Paint(x, y, scale, color, text, 0, limit, 0);
    *maxX = x + (float)w640;
}

#define PIC_WIDTH 12

void CG_DrawNewTeamInfo(rectDef_t* rect, float text_x, float text_y, float scale, vec4_t color, qhandle_t shader) {
    int xx;
    float y;
    int i, j, len, count;
    const char* p;
    vec4_t hcolor;
    float pwidth, lwidth, maxx, leftOver;
    clientInfo_t* ci;
    gitem_t* item;
    qhandle_t h;

    pwidth = 0;
    count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
    for (i = 0; i < count; i++) {
        ci = cgs.clientinfo + sortedTeamPlayers[i];
        if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
            len = CG_OwnerDrawTextWidth(ci->name, scale, 0);
            if (len > pwidth)
                pwidth = len;
        }
    }

    lwidth = 0;
    for (i = 1; i < MAX_LOCATIONS; i++) {
        p = CG_ConfigString(CS_LOCATIONS + i);
        if (p && *p) {
            len = CG_OwnerDrawTextWidth(p, scale, 0);
            if (len > lwidth)
                lwidth = len;
        }
    }

    y = rect->y;

    for (i = 0; i < count; i++) {
        ci = cgs.clientinfo + sortedTeamPlayers[i];
        if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
            xx = rect->x + 1;
            for (j = 0; j <= PW_NUM_POWERUPS; j++) {
                if ((ci->powerups & (1 << j)) && j != PW_FLIGHT && j != PW_FREEZE) {
                    item = BG_FindItemForPowerup(j);

                    if (item) {
                        CG_DrawPic(xx, y, PIC_WIDTH, PIC_WIDTH, trap_R_RegisterShader(item->icon));
                        xx += PIC_WIDTH;
                    }
                }
            }

            xx = rect->x + (PIC_WIDTH * 3) + 2;

            CG_GetColorForHealth(ci->health, ci->armor, hcolor);
            trap_R_SetColor(hcolor);
            CG_DrawPic(xx, y + 1, PIC_WIDTH - 2, PIC_WIDTH - 2, cgs.media.heartShader);

            xx += PIC_WIDTH + 1;

            trap_R_SetColor(NULL);
            if (cgs.orderPending) {
                if (cg.time > cgs.orderTime - 2500 && (cg.time >> 9) & 1) {
                    h = 0;
                } else {
                    h = CG_StatusHandle(cgs.currentOrder);
                }
            } else {
                h = CG_StatusHandle(ci->teamTask);
            }

            if (h) {
                CG_DrawPic(xx, y, PIC_WIDTH, PIC_WIDTH, h);
            }

            xx += PIC_WIDTH + 1;

            leftOver = rect->w - xx;
            maxx = xx + leftOver / 3;

            CG_Text_Paint_Limit(&maxx, xx, y + text_y, scale, color, ci->name, 0, 0);

            p = CG_ConfigString(CS_LOCATIONS + ci->location);
            if (!p || !*p) {
                p = "unknown";
            }

            xx += leftOver / 3 + 2;
            maxx = rect->w - 4;

            CG_Text_Paint_Limit(&maxx, xx, y + text_y, scale, color, p, 0, 0);
            y += text_y + 2;
            if (y + text_y + 2 > rect->y + rect->h) {
                break;
            }
        }
    }
}

void CG_DrawMedal(int ownerDraw, rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    score_t* score = &cg.scores[cg.selectedScore];
    float value = 0;
    char* text = NULL;
    color[3] = 0.25;

    switch (ownerDraw) {
        case CG_ACCURACY:
            value = score->accuracy;
            break;
        case CG_ASSISTS:
            value = score->assistCount;
            break;
        case CG_DEFEND:
            value = score->defendCount;
            break;
        case CG_EXCELLENT:
            value = score->excellentCount;
            break;
        case CG_IMPRESSIVE:
            value = score->impressiveCount;
            break;
        case CG_PERFECT:
            value = score->perfect;
            break;
        case CG_GAUNTLET:
            value = score->guantletCount;
            break;
        case CG_CAPTURES:
            value = score->captures;
            break;
    }

    if (value > 0) {
        if (ownerDraw != CG_PERFECT) {
            if (ownerDraw == CG_ACCURACY) {
                text = va("%i%%", (int)value);
                if (value > 50) {
                    color[3] = 1.0;
                }
            } else {
                text = va("%i", (int)value);
                color[3] = 1.0;
            }
        } else {
            if (value) {
                color[3] = 1.0;
            }
            text = "Wow";
        }
    }

    trap_R_SetColor(color);
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);

    if (text) {
        color[3] = 1.0;
        CG_OwnerDrawText(rect->x + rect->w + 2, rect->y + rect->h - 4, scale * 1.2f, color, text, 0, 0, 0);
    }
    trap_R_SetColor(NULL);
}

static void CG_DrawMapName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, CG_ConfigString(CS_MESSAGE), 0, 0, textStyle);
}

static void CG_AlignRectText(rectDef_t *rect, const char *text, float scale, int align) {
    if (align == ITEM_ALIGN_CENTER) {
        rect->x -= CG_DrawTextWidth(text, scale, 0, 0) * 0.5;
    } else if (align == ITEM_ALIGN_RIGHT) {
        rect->x -= CG_DrawTextWidth(text, scale, 0, 0);
    }
}

static void CG_DrawLevelTimer(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int msec = 0;
    int mins = 0, tens = 0, ones = 0;
    int secs;
    const char *s;

    if (CG_GetLevelTimerMsec(&msec) == qtrue) {
        secs = msec / 1000;
        mins = secs / 60;
        secs -= mins * 60;
        tens = secs / 10;
        ones = secs - tens * 10;
    }

    s = va("%i:%i%i", mins, tens, ones);
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawPlayerCounts(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const char *s;
    int count = 0, maxPlayers;
    int i;

    for (i = 0; i < cgs.maxclients && i < MAX_CLIENTS; i++) {
        if (cgs.clientinfo[i].infoValid) {
            count++;
        }
    }

    maxPlayers = cgs.maxclients;
    if (cgs.gametype != GT_DUEL && cgs.teamsize != 0) {
        maxPlayers = cgs.teamsize;
        if (cgs.gametype > GT_RACE) {
            maxPlayers = cgs.teamsize * 2;
            if (cgs.maxclients < maxPlayers) {
                maxPlayers = cgs.maxclients;
            }
        }
    }

    s = va("%d/%d Players", count, maxPlayers);
    if (align == ITEM_ALIGN_CENTER) {
        rect->x -= CG_OwnerDrawTextWidth(s, scale, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        rect->x -= CG_OwnerDrawTextWidth(s, scale, 0);
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawSelectedPlayerTeamColor(rectDef_t *rect) {
    vec4_t teamColor;

    if (cg.selectedScore >= 0 && cg.selectedScore < cg.numScores) {
        switch (cg.scores[cg.selectedScore].team) {
            case TEAM_RED:
                teamColor[0] = 1; teamColor[1] = 0.2f; teamColor[2] = 0.2f; teamColor[3] = 0.3f;
                break;
            case TEAM_BLUE:
                teamColor[0] = 0.2f; teamColor[1] = 0.2f; teamColor[2] = 1; teamColor[3] = 0.3f;
                break;
            default:
                teamColor[0] = 0.5f; teamColor[1] = 0.5f; teamColor[2] = 0.5f; teamColor[3] = 0.3f;
                break;
        }
        CG_FillRect(rect->x, rect->y, rect->w, rect->h, teamColor);
    }
}

static void CG_DrawSelectedPlayerAccuracy(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cg.selectedScore < 0 || cg.selectedScore >= MAX_CLIENTS) {
        return;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d%%", cg.scores[cg.selectedScore].accuracy), 0, 0,
                     textStyle);
}

static void CG_DrawBestWeaponName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    static const char* names[WP_NUM_WEAPONS] = {
        NULL, "Gauntlet", "Machine Gun", "Shotgun", "Grenade", "Rocket Launcher", "Lightning Gun",
        "Rail Gun", "Plasma Gun", "BFG", NULL, "Nail Gun", "Proximity Mines", "Chain Gun",
        "Heavy Machinegun"
    };
    int bw;

    if (cg.selectedScore < 0 || cg.selectedScore >= MAX_CLIENTS) {
        return;
    }
    bw = cg.scores[cg.selectedScore].bestWeapon;
    if (bw > 0 && bw < WP_NUM_WEAPONS && names[bw]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, names[bw], 0, 0, textStyle);
    }
}

static void CG_DrawGameLimit(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const char *s;
    float x = rect->x;

    switch (cgs.gametype) {
    case GT_CTF:
    case GT_1FCTF:
    case GT_OBELISK:
    case GT_HARVESTER:
        s = va("Cap Limit: %d", cgs.capturelimit);
        break;
    case GT_CA:
    case GT_FREEZE:
    case GT_RR:
        s = va("Round Limit: %d", cgs.roundlimit);
        break;
    case GT_DOMINATION:
    case GT_AD:
        s = va("Score Limit: %d", cgs.scorelimit);
        break;
    default:
        s = va("Frag Limit: %d", cgs.fraglimit);
        break;
    }

    if (align == ITEM_ALIGN_CENTER) {
        x -= (float)(CG_DrawTextWidth(s, scale, 0, 0) / 2);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawMatchDetails(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    static const char *const shortNames[GT_MAX_GAME_TYPE] = {
        "FFA", "DUEL", "RACE", "TDM", "CA", "CTF", "1F", "OB", "HAR", "FT", "DOM", "AD", "RR"};
    const char *state;

    if (cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
        state = "MATCH SUMMARY";
    } else if (cg.warmup) {
        state = "MATCH WARMUP";
    } else {
        state = "MATCH IN PROGRESS";
    }
    CG_DrawText(rect->x, rect->y, 0, scale, color,
                va("%s - %s - %s", state,
                   (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) ? shortNames[cgs.gametype] : "ERR",
                   CG_ConfigString(CS_MESSAGE)),
                0, 0, textStyle);
}

static void CG_DrawMatchStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, CG_GetMatchStatusText(), 0, 0, textStyle);
}

static void CG_DrawRound(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE || cgs.gametype == GT_RR) {
        if (cg.warmup) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color, "Warmup", 0, 0, textStyle);
        } else {
            CG_OwnerDrawText(rect->x, rect->y, scale, color,
                va("Round %d", cgs.roundNum ? cgs.roundNum : 1), 0, 0, textStyle);
        }
    }
}

static void CG_DrawRoundTimer(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int remaining;
    const char *s;

    switch (cgs.gametype) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        break;
    default:
        return;
    }

    remaining = cgs.roundtimelimit * 1000 - cg.time + cgs.roundStartTime;
    if (cgs.roundStartTime <= 0 || remaining < 1 || remaining > 30000 || cg.warmup) {
        return;
    }

    s = va("%s%d", S_COLOR_RED, remaining / 1000);
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawOvertime(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int count;
    const char *s;

    switch (cgs.gametype) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        return;
    default:
        break;
    }

    if (!cgs.timelimit || !cgs.timelimit_overtime) {
        return;
    }

    count = CG_GetOvertimeCount();
    if (count <= 0) {
        return;
    }

    s = va("Overtime%s", count > 1 ? va(" x%i", count) : "");
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawLocalTime(rectDef_t *rect, int fontIndex, float scale, vec4_t color, int textStyle, int align) {
    static const char* months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    qtime_t ct;
    const char *s;
    float x;

    trap_RealTime(&ct);
    s = va("%02d:%02d (%s %02d, %d)", ct.tm_hour, ct.tm_min, months[ct.tm_mon], ct.tm_mday,
           ct.tm_year + 1900);

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex);
    }
    CG_DrawText(x, rect->y, fontIndex, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawMatchState(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;
    if (cg.warmup) {
        s = "MATCH WARMUP";
    } else if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        s = "MATCH SUMMARY";
    } else {
        s = "MATCH IN PROGRESS";
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawMatchWinner(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s = "";
    if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        if (cgs.gametype >= GT_TEAM) {
            if (cgs.scores1 > cgs.scores2) s = "Red Wins!";
            else if (cgs.scores2 > cgs.scores1) s = "Blue Wins!";
            else s = "Tie Game!";
        } else {
            s = CG_ConfigString(CS_SCORES1PLAYER);
        }
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawFollowPlayerName(rectDef_t *rect, int ownerDraw, int align, float scale, vec4_t color, int textStyle) {
    static vec4_t followRed   = {1.0f, 0.5f, 0.5f, 1.0f};
    static vec4_t followBlue  = {0.5f, 0.75f, 1.0f, 1.0f};
    static vec4_t followFree  = {1.0f, 1.0f, 1.0f, 1.0f};
    static vec4_t followSpec  = {0.85f, 0.85f, 0.85f, 1.0f};
    static vec4_t followOther = {1.0f, 0.8f, 0.2f, 1.0f};
    clientInfo_t* ci;
    float* nameColor;
    char text[80];
    float x;

    if (!cg.snap) {
        return;
    }
    ci = &cgs.clientinfo[cg.snap->ps.clientNum];

    nameColor = color;
    if (cgs.gametype >= GT_TEAM) {
        switch (ci->team) {
            case TEAM_FREE:      nameColor = followFree; break;
            case TEAM_RED:       nameColor = followRed; break;
            case TEAM_BLUE:      nameColor = followBlue; break;
            case TEAM_SPECTATOR: nameColor = followSpec; break;
            default:             nameColor = followOther; break;
        }
    }

    Q_strncpyz(text, va("%s%s", (ownerDraw == CG_FOLLOW_PLAYER_NAME) ? "Following - " : "", ci->name),
               sizeof(text));

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_OwnerDrawTextWidth(text, scale, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_OwnerDrawTextWidth(text, scale, 0);
    }

    CG_OwnerDrawText(x, rect->y, scale, nameColor, text, 0, 0, textStyle);
}

static void CG_DrawTeamName(int ownerDraw, rectDef_t* rect, float scale, vec4_t color, int textStyle, int align) {
    char name[40];
    float x;

    if (ownerDraw == CG_RED_NAME) {
        Q_strncpyz(name, cg_redTeamName.string, sizeof(name));
        if (!name[0]) {
            Q_strncpyz(name, "Red Team", sizeof(name));
        }
    } else {
        Q_strncpyz(name, cg_blueTeamName.string, sizeof(name));
        if (!name[0]) {
            Q_strncpyz(name, "Blue Team", sizeof(name));
        }
    }

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(name, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(name, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, name, 0, 0, textStyle);
}

static float CG_OwnerDrawAlignX(rectDef_t *rect, const char *s, float scale, int align);

static void CG_DrawTeamAvgPing(rectDef_t *rect, float scale, int textStyle, int align, int team) {
    int i, count = 0, total = 0;
    const char *s;
    vec4_t c;

    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == team) {
            total += cg.scores[i].ping;
            count++;
        }
    }
    if (count > 0) {
        total /= count;
    }
    if (total < 41) {
        c[0] = 0; c[1] = 1; c[2] = 0;
    } else if (total < 81) {
        c[0] = 1; c[1] = 1; c[2] = 0;
    } else {
        c[0] = 1; c[1] = 0; c[2] = 0;
    }
    c[3] = 0.8f;
    s = va("(%d)", total);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, c, s, 0, 0, textStyle);
}

static void CG_DrawPlaceScore(rectDef_t *rect, float scale, vec4_t color, int textStyle, int place) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        va("%d", place == 1 ? cgs.scores1 : cgs.scores2), 0, 0, textStyle);
}

static void CG_DrawPlayerEndGameScore(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cg.snap) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color,
            va("%d", cg.snap->ps.persistant[PERS_SCORE]), 0, 0, textStyle);
    }
}

static const float *CG_HudBarTeamColor(void) {
    static vec4_t colorFree = { 1.0f, 0.8f, 0.2f, 1.0f };
    static vec4_t colorRed  = { 1.0f, 0.2f, 0.1f, 1.0f };
    static vec4_t colorBlue = { 0.2f, 0.4f, 1.0f, 1.0f };
    static vec4_t colorSpec = { 0.75f, 0.75f, 0.75f, 1.0f };
    static vec4_t colorDef  = { 1.0f, 1.0f, 1.0f, 1.0f };

    switch (cg.snap->ps.persistant[PERS_TEAM]) {
    case TEAM_FREE:      return colorFree;
    case TEAM_RED:       return colorRed;
    case TEAM_BLUE:      return colorBlue;
    case TEAM_SPECTATOR: return colorSpec;
    default:             return colorDef;
    }
}

static void CG_DrawHealthBar100(rectDef_t *rect, qhandle_t shader) {
    float health, maxHealth, scaledWidth, s2;
    float x, y, w, h;

    if (!cg.snap) return;

    maxHealth = (float)cg.snap->ps.stats[STAT_MAX_HEALTH];
    if (maxHealth <= 0) return;
    health = (float)cg.snap->ps.stats[STAT_HEALTH];
    if (maxHealth < health) health = maxHealth;
    if (health <= 0) return;

    scaledWidth = health * (rect->w / maxHealth);
    s2 = scaledWidth / rect->w;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x; y = rect->y; w = scaledWidth; h = rect->h;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, s2, 1.0f, shader);
    trap_R_SetColor(NULL);
}

static void CG_DrawHealthBar200(rectDef_t *rect, qhandle_t shader) {
    float maxHealth, overhealth, scaledHeight, t1;
    float x, y, w, h;

    if (!cg.snap) return;

    maxHealth = (float)cg.snap->ps.stats[STAT_MAX_HEALTH];
    if (maxHealth <= 0) return;
    overhealth = (float)(cg.snap->ps.stats[STAT_HEALTH] - cg.snap->ps.stats[STAT_MAX_HEALTH]);
    if (overhealth <= 0) return;
    if (maxHealth < overhealth) overhealth = maxHealth;

    scaledHeight = overhealth * ((rect->h * 0.6875f) / maxHealth);
    t1 = 1.0f - scaledHeight / rect->h;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x; y = rect->y + rect->h - scaledHeight; w = rect->w; h = scaledHeight;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, t1, 1.0f, 1.0f, shader);
    trap_R_SetColor(NULL);
}

static void CG_DrawArmorBar100(rectDef_t *rect, qhandle_t shader) {
    float armor, scaledWidth, s1;
    float x, y, w, h;

    if (!cg.snap) return;

    armor = (float)cg.snap->ps.stats[STAT_ARMOR];
    if (armor <= 0) return;
    if (armor > 100.0f) armor = 100.0f;

    scaledWidth = armor * (rect->w / 100.0f);
    s1 = 1.0f - scaledWidth / rect->w;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x + rect->w - scaledWidth; y = rect->y; w = scaledWidth; h = rect->h;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, s1, 0, 1.0f, 1.0f, shader);
    trap_R_SetColor(NULL);
}

static void CG_DrawArmorBar200(rectDef_t *rect, qhandle_t shader) {
    float overarmor, scaledHeight, t1;
    float x, y, w, h;

    if (!cg.snap) return;

    overarmor = (float)(cg.snap->ps.stats[STAT_ARMOR] - 100);
    if (overarmor <= 0) return;
    if (overarmor > 100.0f) overarmor = 100.0f;

    scaledHeight = overarmor * ((rect->h * 0.6875f) / 100.0f);
    t1 = 1.0f - scaledHeight / rect->h;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x; y = rect->y + rect->h - scaledHeight; w = rect->w; h = scaledHeight;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, t1, 1.0f, 1.0f, shader);
    trap_R_SetColor(NULL);
}

static void CG_DrawRaceStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;

    if (!(cg.snap->ps.eFlags & EF_SPECTATOR) && !cg.demoPlayback) {
        int keynum = trap_Key_GetKey("kill");
        const char *hint = NULL;
        if (keynum > 0) {
            if (!cg.killRequested) {
                char keyName[32];
                trap_Key_KeynumToStringBuf(keynum, keyName, sizeof(keyName));
                hint = va("Press %s to respawn.", keyName);
            }
        } else {
            hint = "Bind 'kill' to respawn";
        }
        if (hint) {
            vec4_t hintColor = {1.0f, 1.0f, 1.0f, 0.5f};
            CG_DrawText(rect->x + 19.0f, rect->y + 27.0f, 0, 0.18f, hintColor, hint, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
        }
    }

    s = cg.race.active ? "CURRENT RUN" : "LAST TIME";
    CG_DrawText(rect->x, rect->y, 0, scale, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

static void CG_DrawRaceTimes(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int ms;
    const char *s;

    if (cg.race.active) {
        ms = (cgs.freezeEnd ? cgs.freezeEnd : cg.time) - cg.race.startTime;
    } else {
        ms = cg.race.lastTime;
    }
    s = CG_FormatRaceTime(ms);
    CG_DrawText(rect->x, rect->y, 0, scale, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

static void CG_DrawSpeedometer(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int speedInt;
    const char *text;

    if (!cg.snap) return;

    speedInt = (int)cg.speedHistory[cg.speedHistoryIndex];
    text = va("%d", speedInt);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, text, 0, 0, textStyle);
}

void CG_HealthGradientColor(int clientNum, int health, vec3_t out) {
    int handicap = 100;
    int maxHealth;
    float f;

    if (clientNum >= 0 && clientNum < MAX_CLIENTS) {
        handicap = cgs.clientinfo[clientNum].handicap;
    }
    maxHealth = (int)(handicap / 100.0 * cgs.startingHealth);
    if (maxHealth < 1 || maxHealth > cgs.startingHealth) {
        maxHealth = cgs.startingHealth;
    }

    out[0] = 1.0f;
    if (health >= maxHealth) {
        f = maxHealth > 0 ? health / (maxHealth * 2.0f) : 1.0f;
        out[1] = 1.0f;
        out[2] = (f > 1.0f) ? 1.0f : f;
    } else {
        f = maxHealth > 0 ? (float)health / maxHealth : 0.0f;
        out[1] = (f > 0.0f) ? f : 0.0f;
        out[2] = 0.0f;
    }
}

static void CG_DrawHealthColorized(rectDef_t *rect, vec4_t color, qhandle_t shader) {
    centity_t* cent;
    int health;
    vec4_t c;

    if (!cg.snap) {
        return;
    }
    cent = &cg_entities[cg.snap->ps.clientNum];
    health = (cent->currentState.eFlags & EF_DEAD) ? 0 : cg.snap->ps.stats[STAT_HEALTH];

    CG_HealthGradientColor(cent->currentState.clientNum, health, c);
    c[3] = color[3];

    trap_R_SetColor(c);
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    trap_R_SetColor(NULL);
}

static void CG_DrawTeamPlyrCount(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int count = 0, i, myTeam;
    if (!cg.snap) return;
    myTeam = cg.snap->ps.persistant[PERS_TEAM];
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == myTeam) count++;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d", count), 0, 0, textStyle);
}

static void CG_DrawEnemyPlyrCount(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int count = 0, i, myTeam, enemyTeam;
    if (!cg.snap) return;
    myTeam = cg.snap->ps.persistant[PERS_TEAM];
    enemyTeam = (myTeam == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == enemyTeam) count++;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d", count), 0, 0, textStyle);
}

static void CG_DrawVoteMapShot(rectDef_t *rect, int index, qhandle_t shader) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *mapname;
    qhandle_t pic;
    char key[8];

    Com_sprintf(key, sizeof(key), "map_%d", index);
    mapname = Info_ValueForKey(info, key);
    if (!mapname[0]) {
        mapname = "default";
    }
    pic = trap_R_RegisterShaderNoMip(va("levelshots/preview/%s", mapname));
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, pic);
}

static void CG_DrawVoteMapValue(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, const char *field, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *value;

    value = Info_ValueForKey(info, va("%s_%d", field, index));
    CG_DrawText(CG_OwnerDrawAlignX(rect, value, scale, align), rect->y, 0, scale, color, value, 0, 0, textStyle);
}

static void CG_DrawVoteGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *gt;

    gt = Info_ValueForKey(info, va("gt_%d", index));
    CG_DrawText(CG_OwnerDrawAlignX(rect, gt, scale, align), rect->y - 8.0f, 0, scale, color, gt, 0, 0, textStyle);
}

static void CG_DrawVoteTimer(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const int sec = (cgs.voteTime - cg.time + 20000) / 1000;
    const char *s;

    if (sec < 1) {
        s = "Voting has ended.";
    } else if (sec == 1) {
        s = "Voting ends in 1 second.";
    } else {
        s = va("Voting ends in %i seconds.", sec);
    }
    CG_DrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawTimeoutCount(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int csIndex) {
    const char *s = CG_ConfigString(csIndex);
    const char *text;
    float x = rect->x;

    if (cgs.timeoutCount <= 0) {
        return;
    }
    text = va("TO: %d", s[0] ? atoi(s) : cgs.timeoutCount);
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_OwnerDrawTextWidth(text, scale, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_OwnerDrawTextWidth(text, scale, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, text, 0, 0, textStyle);
}

static int CG_DuelPlayerIndex(int ownerDraw) {
    return (ownerDraw >= CG_2ND_PLYR && ownerDraw <= CG_2ND_PLYR_TIER) ? 1 : 0;
}

static qboolean CG_DuelStatShown(const duelScore_t *ds) {
    if (!cg.duelScoresValid) {
        return qfalse;
    }
    if (cg.warmup) {
        return qtrue;
    }
    if (ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS) {
        return qfalse;
    }
    return cgs.clientinfo[ds->clientNum].infoValid;
}

static float CG_OwnerDrawAlignX(rectDef_t *rect, const char *s, float scale, int align) {
    float x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_OwnerDrawTextWidth(s, scale, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_OwnerDrawTextWidth(s, scale, 0);
    }
    return x;
}

static int CG_OwnerDrawWeaponIndex(int ownerDraw) {
    switch (ownerDraw) {
    case 0x75: case 0x9a: case 0xcf: case 0xf4:
        return 1;
    case 0x76: case 0x82: case 0x8e: case 0x9b: case 0xa7:
    case 0xd0: case 0xdc: case 0xe8: case 0xf5: case 0x101:
        return 2;
    case 0x77: case 0x83: case 0x8f: case 0x9c: case 0xa8:
    case 0xd1: case 0xdd: case 0xe9: case 0xf6: case 0x102:
        return 3;
    case 0x78: case 0x84: case 0x90: case 0x9d: case 0xa9:
    case 0xd2: case 0xde: case 0xea: case 0xf7: case 0x103:
        return 4;
    case 0x79: case 0x85: case 0x91: case 0x9e: case 0xaa:
    case 0xd3: case 0xdf: case 0xeb: case 0xf8: case 0x104:
        return 5;
    case 0x7a: case 0x86: case 0x92: case 0x9f: case 0xab:
    case 0xd4: case 0xe0: case 0xec: case 0xf9: case 0x105:
        return 6;
    case 0x7b: case 0x87: case 0x93: case 0xa0: case 0xac:
    case 0xd5: case 0xe1: case 0xed: case 0xfa: case 0x106:
        return 7;
    case 0x7c: case 0x88: case 0x94: case 0xa1: case 0xad:
    case 0xd6: case 0xe2: case 0xee: case 0xfb: case 0x107:
        return 8;
    case 0x7d: case 0x89: case 0x95: case 0xa2: case 0xae:
    case 0xd7: case 0xe3: case 0xef: case 0xfc: case 0x108:
        return 9;
    case 0x7e: case 0x8a: case 0x96: case 0xa3: case 0xaf:
    case 0xd8: case 0xe4: case 0xf0: case 0xfd: case 0x109:
        return 13;
    case 0x7f: case 0x8b: case 0x97: case 0xa4: case 0xb0:
    case 0xd9: case 0xe5: case 0xf1: case 0xfe: case 0x10a:
        return 11;
    case 0x80: case 0x8c: case 0x98: case 0xa5: case 0xb1:
    case 0xda: case 0xe6: case 0xf2: case 0xff: case 0x10b:
        return 12;
    case 0x81: case 0x8d: case 0x99: case 0xa6: case 0xb2:
    case 0xdb: case 0xe7: case 0xf3: case 0x100: case 0x10c:
        return 14;
    default:
        return 0;
    }
}

static void CG_DrawDuelPlayerName(int ownerDraw, rectDef_t *rect, int fontIndex, float scale, vec4_t color, int textStyle, int align) {
    char name[40];
    char shortName[40];
    const char *text;
    int len, i, w;

    if (ownerDraw >= 0xc1 && ownerDraw <= 0x11a) {
        Q_strncpyz(name, cgs.name2ndPlayer, sizeof(name));
    } else {
        Q_strncpyz(name, cgs.name1stPlayer, sizeof(name));
    }

    if (CG_DrawTextWidth(name, scale, 0, fontIndex) > 140) {
        Q_strncpyz(shortName, name, sizeof(shortName));
        len = strlen(shortName);
        for (i = 0; i < len; i++) {
            Q_strncpyz(shortName, name, len - i);
            if (CG_DrawTextWidth(shortName, scale, 0, fontIndex) < 132) {
                break;
            }
        }
        Q_strncpyz(name, va("%s%s", shortName, "..."), sizeof(name));
    }

    text = (cg.intermissionStarted == 1 && !name[0]) ? "-" : name;

    w = CG_DrawTextWidth(text, scale, 0, fontIndex);
    if (align == ITEM_ALIGN_CENTER) {
        rect->x -= w * 0.5;
    } else if (align == ITEM_ALIGN_RIGHT) {
        rect->x -= w;
    }
    CG_DrawText(rect->x, rect->y, fontIndex, scale, color, text, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerScore(int ownerDraw, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int score = (ownerDraw == CG_1ST_PLYR_SCORE) ? cgs.score1stPlayer : cgs.score2ndPlayer;
    const char *s;

    if (score == SCORE_NOT_PRESENT || score == -999) {
        return;
    }
    s = va("%d", score);
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerFrags(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i", ds->kills);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerDeaths(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i", ds->deaths);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerDmg(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i", ds->damage);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerPing(rectDef_t *rect, float scale, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    vec4_t c;
    if (!CG_DuelStatShown(ds)) return;
    if (ds->ping < 41) {
        c[0] = 0; c[1] = 1; c[2] = 0;
    } else if (ds->ping < 81) {
        c[0] = 1; c[1] = 1; c[2] = 0;
    } else {
        c[0] = 1; c[1] = 0; c[2] = 0;
    }
    c[3] = 0.8f;
    s = va("%i", ds->ping);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, c, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerWins(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    if (ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS) return;
    s = va("%i", cgs.clientinfo[ds->clientNum].wins);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerAcc(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i%%", ds->accuracy);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelPlayerFlag(rectDef_t *rect, int ownerDraw) {
    duelScore_t *ds = &cg.duelScores[CG_DuelPlayerIndex(ownerDraw)];

    if (!cg.duelScoresValid || ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS ||
        !cgs.clientinfo[ds->clientNum].infoValid) {
        return;
    }
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, CG_CountryFlagShader(cgs.clientinfo[ds->clientNum].country));
}

static void CG_DrawDuelPlayerAvatar(rectDef_t *rect, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    int clientNum;
    if (!cg.duelScoresValid) return;
    clientNum = ds->clientNum;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid) return;
    if (cgs.clientinfo[clientNum].modelIcon) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.clientinfo[clientNum].modelIcon);
    }
}

static void CG_DrawDuelHealthArmorBar(rectDef_t *rect, int ownerDraw) {
    static int lastHealth[2];
    static vec4_t barGold      = {1.0f, 0.8f, 0.2f, 1.0f};
    static vec4_t trailRed     = {1.0f, 0.0f, 0.0f, 1.0f};
    static vec4_t trailRedHalf = {1.0f, 0.0f, 0.0f, 0.5f};
    static vec4_t armorCyan    = {0.0f, 1.0f, 1.0f, 1.0f};
    qboolean second;
    centity_t* cent;
    int cl, health, armor, maxHealth, damageTime;
    qboolean local, recentHit;
    double handicapFrac;
    float scale, hf, df, af, barW, trailW, armorW, width;
    vec4_t gradient;
    float* healthColor;

    second = (ownerDraw >= 193 && ownerDraw <= 282);
    cent = &cg_entities[second ? cgs.clientNum2ndPlayer : cgs.clientNum1stPlayer];
    cl = cent->currentState.clientNum;
    local = (cl == cg.snap->ps.clientNum);

    if (!cg_specDuelHealthArmor.integer || !(cent->currentState.eType & 0xb)) {
        return;
    }

    if (local) {
        health = cg.snap->ps.stats[STAT_HEALTH];
        armor = cg.snap->ps.stats[STAT_ARMOR];
        damageTime = (int)cg.damageTime;
    } else {
        health = cent->currentState.health;
        armor = cent->currentState.armor;
        damageTime = cent->pe.painTime;
    }
    width = rect->w;
    recentHit = (cg.time - damageTime) < 2000;

    handicapFrac = cgs.clientinfo[cl].handicap / 100.0;
    maxHealth = (int)(handicapFrac * cgs.startingHealth);
    if (maxHealth < 1 || maxHealth > cgs.startingHealth) {
        maxHealth = cgs.startingHealth;
    }

    if (cent->currentState.eFlags & EF_DEAD) {
        return;
    }

    scale = (float)(maxHealth * 2);

    hf = health / scale;
    if (hf < 0.0f) {
        hf = 0.0f;
    }
    barW = hf * width;

    if (cg_specDuelHealthColor.integer) {
        CG_HealthGradientColor(cl, health, gradient);
        gradient[3] = 1.0f;
        healthColor = gradient;
    } else {
        healthColor = barGold;
    }
    CG_FillRect(rect->x + (second ? 0.0f : width - barW), rect->y + 2, barW, 8, healthColor);

    if (recentHit) {
        df = (lastHealth[second] - health) / scale;
        if (df < 0.0f) {
            df = 0.0f;
        }
        if (df > hf) {
            df = hf;
        }
        trailW = df * width;
        CG_FillRect(rect->x + (second ? barW : (width - barW) - trailW), rect->y + 2, trailW, 8,
                    cg_specDuelHealthColor.integer ? trailRedHalf : trailRed);
    } else {
        lastHealth[second] = health;
    }

    if (armor) {
        af = armor / scale;
        if (af < 0.0f) {
            af = 0.0f;
        }
        armorW = (float)(af * handicapFrac) * width;
        CG_FillRect(rect->x + (second ? 0.0f : width - armorW), rect->y + 10, armorW, 2, armorCyan);
    }

    trap_R_SetColor(NULL);
}

static duelWeaponStats_t *CG_DuelWeaponStats(int player, int ownerDraw) {
    int w = CG_OwnerDrawWeaponIndex(ownerDraw);

    if (w <= 0 || w > 14) {
        return NULL;
    }
    return &cg.duelScores[player].weaponStats[w - 1];
}

static void CG_DrawDuelWeaponFrags(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    duelWeaponStats_t *ws = CG_DuelWeaponStats(CG_DuelPlayerIndex(ownerDraw), ownerDraw);
    const char *s;
    if (!ws) return;
    s = va("%d", ws->kills);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelWeaponHits(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    duelWeaponStats_t *ws = CG_DuelWeaponStats(CG_DuelPlayerIndex(ownerDraw), ownerDraw);
    const char *s;
    if (!ws) return;
    s = va("%d", ws->hits);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelWeaponShots(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    duelWeaponStats_t *ws = CG_DuelWeaponStats(CG_DuelPlayerIndex(ownerDraw), ownerDraw);
    const char *s;
    if (!ws) return;
    s = va("%d", ws->atts);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelWeaponDmg(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    duelWeaponStats_t *ws = CG_DuelWeaponStats(CG_DuelPlayerIndex(ownerDraw), ownerDraw);
    const char *s;
    if (!ws) return;
    s = va("%d", ws->damage);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelWeaponAcc(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int player = CG_DuelPlayerIndex(ownerDraw);
    duelWeaponStats_t *mine = CG_DuelWeaponStats(player, ownerDraw);
    duelWeaponStats_t *theirs = CG_DuelWeaponStats(player ^ 1, ownerDraw);
    const char *s;
    vec4_t c;
    if (!mine || !theirs) return;
    Vector4Copy(color, c);
    if (theirs->accuracy < mine->accuracy) {
        c[0] = c[1] = c[2] = 1.0f; c[3] = 0.8f;
    }
    s = va("%d%%", mine->accuracy);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, c, s, 0, 0, textStyle);
}

static void CG_DuelPickupSlot(duelScore_t *ds, int slot, int *count, float *avgTime) {
    switch (slot) {
    case 0: *count = ds->redArmorPickups;    *avgTime = ds->redArmorTime;    break;
    case 1: *count = ds->yellowArmorPickups; *avgTime = ds->yellowArmorTime; break;
    case 2: *count = ds->greenArmorPickups;  *avgTime = ds->greenArmorTime;  break;
    default:*count = ds->megaHealthPickups;  *avgTime = ds->megaHealthTime;  break;
    }
}

static void CG_DrawDuelPlayerPickups(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int player = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[player];
    int base = player ? CG_2ND_PLYR_PICKUPS : CG_1ST_PLYR_PICKUPS;
    int off = ownerDraw - base;
    qhandle_t icons[4];
    rectDef_t text;
    const char *s;
    int count, i;
    float avgTime, x, y;

    if (off == 0) {
        icons[0] = cgs.media.pickupRA;
        icons[1] = cgs.media.pickupYA;
        icons[2] = cgs.media.pickupGA;
        icons[3] = cgs.media.pickupMH;
        x = rect->x;
        y = rect->y;
        for (i = 0; i < 4; i++) {
            CG_DuelPickupSlot(ds, i, &count, &avgTime);
            if (!count) {
                continue;
            }
            CG_DrawPic(x, y, rect->w, rect->h, icons[i]);
            if (!player) {
                x += 15;
            }
            y += 15;

            s = va("%d", count);
            text.x = x;
            text.y = y;
            CG_AlignRectText(&text, s, scale, align);
            CG_DrawText(text.x, text.y, 0, scale, color, s, 0, 0, textStyle);
            x += player ? -15 : 15;

            s = va("%3.2f", avgTime);
            text.x = x;
            text.y = y;
            CG_AlignRectText(&text, s, scale, align);
            CG_DrawText(text.x, text.y, 0, scale, color, s, 0, 0, textStyle);
            x += player ? 15 : -30;
        }
        return;
    }

    if (off < 1 || off > 4) {
        return;
    }
    CG_DuelPickupSlot(ds, off - 1, &count, &avgTime);
    s = va("%d", count);
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelAvgPickupTime(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int player = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[player];
    int base = player ? CG_2ND_PLYR_AVG_PICKUP_TIME_RA : CG_1ST_PLYR_AVG_PICKUP_TIME_RA;
    int slot = ownerDraw - base;
    const char *s;
    int count;
    float avgTime;

    if (slot < 0 || slot > 3) {
        return;
    }
    CG_DuelPickupSlot(ds, slot, &count, &avgTime);
    s = count ? va("%3.2f", avgTime) : "-";
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawDuelAwards(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int player = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[player];
    int base = player ? CG_2ND_PLYR_EXCELLENT : CG_1ST_PLYR_EXCELLENT;
    const char *s;
    float x;

    if ((ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS || !cgs.clientinfo[ds->clientNum].infoValid) &&
        cg.intermissionStarted != 1) {
        return;
    }
    switch (ownerDraw - base) {
    case 0: s = va("%d", ds->awardExcellent); break;
    case 1: s = va("%d", ds->awardImpressive); break;
    case 2: s = va("%d", ds->awardHumiliation); break;
    default: return;
    }

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static const char* CG_ClockFromSeconds(int seconds) {
    int mins = seconds / 60;

    seconds -= mins * 60;
    return va("%i:%i%i", mins, seconds / 10, seconds % 10);
}

static void CG_DrawTeamPickupItem(int* x, int y, const rectDef_t* rect, qhandle_t icon, int count, int countX,
                                  float scale, vec4_t color, int textStyle) {
    if (count <= 0) {
        return;
    }
    CG_DrawPic(*x, y, rect->w, rect->h, icon);
    CG_OwnerDrawText(*x + countX, y + 15, scale, color, va("%d", count), 0, 0, textStyle);
    *x += 25;
}

static void CG_DrawTeamPickupPowerup(int* x, int y, const rectDef_t* rect, qhandle_t icon, int count, int seconds,
                                     float scale, vec4_t color, int textStyle) {
    if (count <= 0) {
        return;
    }
    if (*x < rect->x + 75.0f) {
        *x = (int)(rect->x + 75.0f);
    }
    CG_DrawPic(*x, y - 7, rect->w, rect->h, icon);
    CG_OwnerDrawText(*x + 14, y + 8, scale, color, va("%d", count), 0, 0, textStyle);
    CG_OwnerDrawText(*x + 2, y + 15, scale, color, seconds ? CG_ClockFromSeconds(seconds) : "-", 0, 0, textStyle);
    *x += 28;
}

static void CG_DrawTeamPickups(rectDef_t* rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    const teamPickups_t* p;
    qhandle_t flag;
    const char* s;
    int blue, x, y, count;

    blue = (ownerDraw >= CG_BLUE_TEAM_MAP_PICKUPS);
    p = &cg.teamPickups[blue];

    switch (ownerDraw) {
    case CG_RED_TEAM_MAP_PICKUPS:
    case CG_BLUE_TEAM_MAP_PICKUPS:
        x = (int)rect->x;
        y = (int)rect->y;
        CG_DrawTeamPickupItem(&x, y, rect, cgs.media.pickupRA, p->ra, 14, scale, color, textStyle);
        CG_DrawTeamPickupItem(&x, y, rect, cgs.media.pickupYA, p->ya, blue ? 14 : 11, scale, color, textStyle);
        CG_DrawTeamPickupItem(&x, y, rect, cgs.media.pickupGA, p->ga, 14, scale, color, textStyle);
        CG_DrawTeamPickupItem(&x, y, rect, cgs.media.pickupMH, p->mh, 14, scale, color, textStyle);
        CG_DrawTeamPickupItem(&x, y, rect, cgs.media.pickupMK, p->medkit, 14, scale, color, textStyle);

        if (cgs.gametype == GT_1FCTF) {
            flag = cgs.media.pickupNeutralFlag;
        } else {
            flag = blue ? cgs.media.pickupRedFlag : cgs.media.pickupBlueFlag;
        }
        CG_DrawTeamPickupPowerup(&x, y, rect, flag, p->flag, p->flagTime, scale, color, textStyle);
        CG_DrawTeamPickupPowerup(&x, y, rect, cgs.media.pickupQuad, p->quad, p->quadTime, scale, color, textStyle);
        CG_DrawTeamPickupPowerup(&x, y, rect, cgs.media.pickupBS, p->bs, p->bsTime, scale, color, textStyle);
        CG_DrawTeamPickupPowerup(&x, y, rect, cgs.media.pickupRegen, p->regen, p->regenTime, scale, color, textStyle);
        CG_DrawTeamPickupPowerup(&x, y, rect, cgs.media.pickupHaste, p->haste, p->hasteTime, scale, color, textStyle);
        CG_DrawTeamPickupPowerup(&x, y, rect, cgs.media.pickupInvis, p->invis, p->invisTime, scale, color, textStyle);
        return;
    case CG_RED_TEAM_PICKUPS_RA:
    case CG_BLUE_TEAM_PICKUPS_RA:
        count = p->ra;
        break;
    case CG_RED_TEAM_PICKUPS_YA:
    case CG_BLUE_TEAM_PICKUPS_YA:
        count = p->ya;
        break;
    case CG_RED_TEAM_PICKUPS_GA:
    case CG_BLUE_TEAM_PICKUPS_GA:
        count = p->ga;
        break;
    case CG_RED_TEAM_PICKUPS_MH:
    case CG_BLUE_TEAM_PICKUPS_MH:
        count = p->mh;
        break;
    case CG_RED_TEAM_PICKUPS_QUAD:
    case CG_BLUE_TEAM_PICKUPS_QUAD:
        count = p->quad;
        break;
    case CG_RED_TEAM_PICKUPS_BS:
    case CG_BLUE_TEAM_PICKUPS_BS:
        count = p->bs;
        break;
    case CG_RED_TEAM_PICKUPS_FLAG:
    case CG_BLUE_TEAM_PICKUPS_FLAG:
        count = p->flag;
        break;
    case CG_RED_TEAM_PICKUPS_MEDKIT:
    case CG_BLUE_TEAM_PICKUPS_MEDKIT:
        count = p->medkit;
        break;
    case CG_RED_TEAM_PICKUPS_REGEN:
    case CG_BLUE_TEAM_PICKUPS_REGEN:
        count = p->regen;
        break;
    case CG_RED_TEAM_PICKUPS_HASTE:
    case CG_BLUE_TEAM_PICKUPS_HASTE:
        count = p->haste;
        break;
    case CG_RED_TEAM_PICKUPS_INVIS:
    case CG_BLUE_TEAM_PICKUPS_INVIS:
        count = p->invis;
        break;
    default:
        return;
    }

    s = va("%d", count);
    CG_AlignRectText(rect, s, scale, align);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawTeamTimeHeld(rectDef_t* rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    const teamPickups_t* p = &cg.teamPickups[ownerDraw >= CG_BLUE_TEAM_MAP_PICKUPS];
    const char* s;
    int seconds;

    switch (ownerDraw) {
    case CG_RED_TEAM_TIMEHELD_QUAD:
    case CG_BLUE_TEAM_TIMEHELD_QUAD:
        seconds = p->quadTime;
        break;
    case CG_RED_TEAM_TIMEHELD_BS:
    case CG_BLUE_TEAM_TIMEHELD_BS:
        seconds = p->bsTime;
        break;
    case CG_RED_TEAM_TIMEHELD_FLAG:
    case CG_BLUE_TEAM_TIMEHELD_FLAG:
        seconds = p->flagTime;
        break;
    case CG_RED_TEAM_TIMEHELD_REGEN:
    case CG_BLUE_TEAM_TIMEHELD_REGEN:
        seconds = p->regenTime;
        break;
    case CG_RED_TEAM_TIMEHELD_HASTE:
    case CG_BLUE_TEAM_TIMEHELD_HASTE:
        seconds = p->hasteTime;
        break;
    case CG_RED_TEAM_TIMEHELD_INVIS:
    case CG_BLUE_TEAM_TIMEHELD_INVIS:
        seconds = p->invisTime;
        break;
    default:
        return;
    }

    s = seconds ? CG_ClockFromSeconds(seconds) : "-";
    CG_AlignRectText(rect, s, scale, align);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawGameTypeIcon(rectDef_t *rect) {
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        qhandle_t icon = cgs.media.gametypeIcon[cgs.gametype];
        if (icon) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, icon);
        }
    }
}

static void CG_DrawTeamColorized(rectDef_t *rect, vec4_t color, qhandle_t shader) {
    static const vec3_t teamFree = {1.0f, 0.8f, 0.2f};
    static const vec3_t teamRed  = {1.0f, 0.2f, 0.1f};
    static const vec3_t teamBlue = {0.2f, 0.4f, 1.0f};
    static const vec3_t teamSpec = {0.75f, 0.75f, 0.75f};
    static const vec3_t teamNone = {1.0f, 1.0f, 1.0f};
    const float* rgb;
    vec4_t c;

    switch (cg.snap->ps.persistant[PERS_TEAM]) {
        case TEAM_FREE:      rgb = teamFree; break;
        case TEAM_RED:       rgb = teamRed; break;
        case TEAM_BLUE:      rgb = teamBlue; break;
        case TEAM_SPECTATOR: rgb = teamSpec; break;
        default:             rgb = teamNone; break;
    }
    VectorCopy(rgb, c);
    c[3] = color[3];

    trap_R_SetColor(c);
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    trap_R_SetColor(NULL);
}

static void CG_DrawArmorTieredColorized(rectDef_t *rect, vec4_t color, qhandle_t shader) {
    vec4_t c;

    Vector4Copy(color, c);
    switch (cg.snap->ps.stats[STAT_ARMORTYPE]) {
        case 0: c[0] = 0.0f; c[1] = 1.0f; c[2] = 0.0f; break;
        case 1: c[0] = 1.0f; c[1] = 1.0f; c[2] = 0.0f; break;
        case 2: c[0] = 1.0f; c[1] = 0.0f; c[2] = 0.0f; break;
        default: break;
    }

    trap_R_SetColor(c);
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    trap_R_SetColor(NULL);
}

static vec3_t obitTeamColors[] = {
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 0.5f, 0.5f },
    { 0.5f, 0.75f, 1.0f },
    { 0.85f, 0.85f, 0.85f },
    { 1.0f, 0.8f, 0.2f }
};

static const float *CG_ObitTeamColor(int team) {
    switch (team) {
    case TEAM_FREE:
    case TEAM_RED:
    case TEAM_BLUE:
    case TEAM_SPECTATOR:
        return obitTeamColors[team];
    default:
        return obitTeamColors[4];
    }
}

#define OBIT_SCALE 0.85f
#define OBIT_X 8.0f
#define OBIT_Y 14.0f
#define OBIT_ROW_GAP 6.5f
#define OBIT_PAD_X 4.5f
#define OBIT_PAD_Y 2.5f
#define OBIT_ICON_SCALE 1.3f
#define OBIT_ICON_GAP 0.7f
#define OBIT_BAND_ALPHA 0.45f

static void CG_DrawPlayerObit(rectDef_t *rect, float scale) {
    static vec4_t obitColor;
    int i;
    int remaining;
    float asc, lineH, capH, padX, padY, bandH, pitch, iconSize, gapX;
    float left, bandTop, baseline, x, iconX, iconY, iconW, iconH, attackerW, victimW;
    const float *team;
    int lifetime = ch_animateScroll.integer ? OBIT_FADEIN + OBIT_FADEDELAY + OBIT_FADEOUT : 2000;
    float slide = 0.0f;

    scale *= OBIT_SCALE;

    asc = (float)CG_DrawTextHeight("H", scale, 0, 0);
    lineH = (float)CG_DrawTextLineHeight(scale, 0);
    capH = CG_DrawTextCapHeight(scale, 0);
    padX = OBIT_PAD_X;
    padY = OBIT_PAD_Y;
    bandH = lineH + 2.0f * padY;
    pitch = lineH + OBIT_ROW_GAP;
    iconSize = asc * OBIT_ICON_SCALE;
    gapX = asc * OBIT_ICON_GAP;

    for (i = 0; i < MAX_OBITUARIES; i++) {
        if (cg.obituaries[i].time > 0 && cg.time >= cg.obituaries[i].time + lifetime) {
            memmove(&cg.obituaries[0], &cg.obituaries[1], (MAX_OBITUARIES - 1) * sizeof(cg.obituaries[0]));
            memset(&cg.obituaries[MAX_OBITUARIES - 1], 0, sizeof(cg.obituaries[0]));
            i--;
        }
    }

    if (ch_animateScroll.integer) {
        if (cg.obitSlideStart && (cg.time >= cg.obitSlideStart + OBIT_FADEOUT || !cg.obituaries[0].active ||
                                  cg.obituaries[0].time != cg.obitSlideKey)) {
            cg.obitSlideStart = 0;
        }
        if (!cg.obitSlideStart && cg.obituaries[0].active &&
            CG_CpmaFadingOut(cg.obituaries[0].time, OBIT_FADEIN, OBIT_FADEDELAY, OBIT_FADEOUT)) {
            cg.obitSlideStart = cg.time;
            cg.obitSlideKey = cg.obituaries[0].time;
        }
        if (cg.obitSlideStart) {
            slide = CG_CpmaSlide(cg.obitSlideStart, OBIT_FADEOUT) * pitch;
        }
    }

    left = (float)(int)rect->x + OBIT_X;
    bandTop = (float)(int)rect->y + OBIT_Y - asc - padY - slide;

    for (i = 0; i < MAX_OBITUARIES; i++) {
        if (!cg.obituaries[i].active) {
            continue;
        }

        remaining = lifetime - (cg.time - cg.obituaries[i].time);
        if (!cg.obituaries[i].time || remaining <= 0) {
            continue;
        }
        if (ch_animateScroll.integer) {
            obitColor[3] = CG_CpmaFade(cg.obituaries[i].time, OBIT_FADEIN, OBIT_FADEDELAY, OBIT_FADEOUT);
        } else {
            obitColor[3] = (remaining < 200) ? remaining / 200.0f : 1.0f;
        }

        baseline = bandTop + (bandH + capH) * 0.5f;
        iconY = bandTop + (bandH - iconSize) * 0.5f;

        attackerW = 0.0f;
        if (cg.obituaries[i].hasAttacker == 1) {
            attackerW = (float)CG_DrawTextWidth(cg.obituaries[i].attackerName, scale, 0, 0);
        }
        victimW = (float)CG_DrawTextWidth(cg.obituaries[i].victimName, scale, 0, 0);
        iconX = left + attackerW + gapX;

        {
            vec4_t band = {0.0f, 0.0f, 0.0f, 0.0f};
            float rowW = attackerW + gapX + iconSize + gapX + victimW;

            band[3] = obitColor[3] * OBIT_BAND_ALPHA;
            CG_FillRect(left - padX, bandTop, rowW + 2.0f * padX, bandH, band);
        }

        if (cg.obituaries[i].hasAttacker == 1) {
            team = CG_ObitTeamColor(cg.obituaries[i].attackerTeam);
            VectorCopy(team, obitColor);
            CG_DrawText(left, baseline, 0, scale, obitColor, cg.obituaries[i].attackerName, 0, 0, 0);
        }

        if (cg.obituaries[i].weaponIcon) {
            obitColor[0] = obitColor[1] = obitColor[2] = 1.0f;
            trap_R_SetColor(obitColor);
            x = iconX;
            iconH = iconY;
            iconW = iconSize;
            {
                float w = iconSize, h = iconSize;

                CG_AdjustFrom640(&x, &iconH, &w, &h);
                trap_R_DrawStretchPic(x, iconH, w, h, 0, 0, 1, 1, cg.obituaries[i].weaponIcon);
            }
            trap_R_SetColor(NULL);
        }

        team = CG_ObitTeamColor(cg.obituaries[i].victimTeam);
        VectorCopy(team, obitColor);
        CG_DrawText(iconX + iconSize + gapX, baseline, 0, scale, obitColor, cg.obituaries[i].victimName, 0, 0, 0);

        bandTop += pitch;
    }
}

static void CG_DrawWeaponVertical(rectDef_t *rect, vec4_t color) {
    int i;
    int count = 0;

    trap_R_SetColor(color);

    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        qhandle_t icon;

        if (i == WP_BFG) {
            continue;
        }

        icon = cg_weapons[i].weaponIcon;
        if (icon == 0) {
            continue;
        }

        CG_DrawPic(rect->x, rect->h * count + rect->y, rect->w, rect->w, icon);
        count++;
    }

    trap_R_SetColor(NULL);
}

static void CG_DrawAccuracyVertical(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int i;
    int count = 0;

    trap_R_SetColor(color);

    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        if (i == WP_BFG) {
            continue;
        }

        if (cg_weapons[i].weaponIcon == 0) {
            continue;
        }

        if (cg.accuracyStats.valid && cg.accuracyStats.accuracy[i] > 0) {
            const char *s = va("%i%%", cg.accuracyStats.accuracy[i]);
            CG_OwnerDrawText(rect->x, rect->h * count + rect->y, scale, color, s, 0, 0, textStyle);
        }
        count++;
    }

    trap_R_SetColor(NULL);
}

static const char *CG_GameTypeFullName(void) {
    static const char *names[] = {
        "Free For All",
        "Duel",
        "Race",
        "Team Deathmatch",
        "Clan Arena",
        "Capture the Flag",
        "One Flag CTF",
        "Overload",
        "Harvester",
        "Freeze Tag",
        "Domination",
        "Attack and Defend",
        "Red Rover"
    };
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        return names[cgs.gametype];
    }
    return "Unknown Gametype";
}

static void CG_DrawGameTypeMap(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const char *s = va("%s - %s", CG_GameTypeFullName(), CG_ConfigString(CS_MESSAGE));
    float x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawWinCondition(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;

    if (cgs.gametype == GT_RACE) {
        s = "Fastest race time within the time limit";
    } else if (cgs.timelimit == 0 ||
               (cg.time - cgs.levelStartTime) < cgs.timelimit * 60000) {
        if (cgs.gametype == GT_CTF) {
            if (cgs.capturelimit == 0 ||
                (cgs.scores1 < cgs.capturelimit && cgs.scores2 < cgs.capturelimit)) {
                s = "First to reach the mercy limit";
            } else {
                s = "First to reach the capture limit";
            }
        } else if (cgs.gametype == GT_CA) {
            s = "First to reach the round limit";
        } else if (cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
            s = "First to reach the score limit";
        } else {
            s = "Highest score at the end of the game";
        }
    } else if (cgs.gametype == GT_CTF) {
        s = "Most flag captures within the time limit";
    } else if (cgs.gametype == GT_CA) {
        s = "Most rounds won within the time limit";
    } else {
        s = "Highest score within the time limit";
    }
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawTeamAliveCount(rectDef_t *rect, int team, float scale, vec4_t color, int textStyle) {
    const char *s = va("%d", cgs.teamAliveCount[team]);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawPlayerCount2(rectDef_t *rect, int team, float scale, vec4_t color, int textStyle, int align) {
    int i, count = 0;
    const char *s;
    float x;

    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == team) {
            count++;
        }
    }

    if (cgs.gametype == GT_TEAM || cgs.gametype == GT_CA || cgs.gametype == GT_CTF ||
        cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER ||
        cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
        if (cgs.teamsize < 1) {
            s = va("(%d)", count);
        } else {
            s = va("(%d/%d)", count, cgs.teamsize);
        }
    } else if (cgs.teamsize < 1 || cgs.maxclients < cgs.teamsize * 2) {
        s = va("%d Player%s", count, (count == 1) ? "" : "s");
    } else {
        s = va("%d/%d Players", count, cgs.teamsize);
    }

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawScrollingNotify(rectDef_t *rect, int fontIndex, float scale, vec4_t color) {
    int i, shown = 0;
    float x, used = 0.0f;
    const int cap = (int)ARRAY_LEN(cg.notifyMessages);
    int count = cg.notifyCount;

    if (count > cap) {
        count = cap;
    }

    for (i = cg.notifyScrollStart; i < count; i++) {
        float w = (float)CG_DrawTextWidth(cg.notifyMessages[i], scale, 0, fontIndex);
        if (rect->w < used + w) {
            break;
        }
        used += w + 10.0f;
        shown++;
    }

    x = rect->x;
    for (i = cg.notifyScrollStart; i < cg.notifyScrollStart + shown && i < count; i++) {
        float w = (float)CG_DrawTextWidth(cg.notifyMessages[i], scale, 0, fontIndex);
        CG_DrawText(x, (rect->y + rect->h) - 3.0f, fontIndex, scale, color,
                    cg.notifyMessages[i], 0, 0, 0);
        x += w + 10.0f;
    }

    if (cg.notifyScrollTime < cg.time) {
        cg.notifyScrollStart += shown;
        cg.notifyScrollTime = cg.time + 4000;
        if (cg.notifyScrollStart >= count) {
            cg.notifyScrollStart = 0;
        }
    }
}

static void CG_DrawVotes(rectDef_t *rect, int ownerDraw, int fontIndex, float scale, vec4_t color, int textStyle, int align) {
    const char *info = CG_ConfigString(CS_ROTATIONVOTES);
    const char *count;
    const char *s;
    char key[4];
    float x;

    Com_sprintf(key, sizeof(key), "%d", ownerDraw - CG_VOTECOUNT1);
    count = Info_ValueForKey(info, key);
    if (!count || !count[0]) {
        return;
    }
    s = va("Votes: %s", count);

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex);
    }
    CG_DrawText(x, rect->y, fontIndex, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawFlagStatus(rectDef_t *rect, int ownerDraw) {
    int team, iconIndex, flagState;
    qboolean isBase;
    gitem_t *item;

    team = (ownerDraw == CG_RED_FLAGSTATUS || ownerDraw == CG_RED_BASESTATUS) ? 1 : 2;
    isBase = (ownerDraw == CG_RED_BASESTATUS || ownerDraw == CG_BLUE_BASESTATUS);

    switch (cgs.gametype) {
        case GT_CTF: case GT_1FCTF: case GT_OBELISK:
        case GT_HARVESTER: case GT_DOMINATION: case GT_AD:
            break;
        default:
            return;
    }

    item = BG_FindItemForPowerup(cgs.gametype == GT_1FCTF ? PW_NEUTRALFLAG
                                 : (team == 1 ? PW_REDFLAG : PW_BLUEFLAG));
    if (!item) {
        return;
    }

    trap_R_SetColor(colorWhite);

    flagState = (team == 1) ? cgs.redflag : cgs.blueflag;

    if (cgs.gametype == GT_1FCTF) {
        if (team == 1) {
            if (cgs.flagStatus != FLAG_TAKEN_RED) { trap_R_SetColor(NULL); return; }
        } else {
            if (cgs.flagStatus != FLAG_TAKEN_BLUE) { trap_R_SetColor(NULL); return; }
        }
        iconIndex = 3;
        team = 0;
    } else if (flagState == FLAG_ATBASE) {
        iconIndex = 0;
    } else if (flagState == FLAG_TAKEN || isBase) {
        iconIndex = 1;
    } else if (flagState == FLAG_DROPPED) {
        iconIndex = 2;
    } else {
        iconIndex = 0;
    }

    CG_DrawPic(rect->x, rect->y, rect->w, rect->h,
               cgs.media.flagStatusHandles[iconIndex + team * 4]);
    trap_R_SetColor(NULL);
}

static float CG_TeamBaseDistance(const vec3_t base, const vec3_t flag, float span) {
    vec3_t delta;
    float d;

    VectorSubtract(base, flag, delta);
    d = VectorLength(delta);
    return d > span ? span : d;
}

static void CG_ReadTeamBase(int index, vec3_t origin) {
    const char* info = CG_ConfigString(index);

    origin[0] = atof(Info_ValueForKey(info, "x"));
    origin[1] = atof(Info_ValueForKey(info, "y"));
    origin[2] = atof(Info_ValueForKey(info, "z"));
}

static void CG_DrawFlagStatusBar(rectDef_t *rect) {
    static const vec4_t redHalf = {1.0f, 0.0f, 0.0f, 0.5f};
    static const vec4_t blueHalf = {0.0f, 0.0f, 1.0f, 0.5f};
    vec3_t redBase, blueBase, redFlag, blueFlag, delta;
    float size, barHeight, span, x;
    int i;

    size = rect->h;
    barHeight = (float)(int)(rect->h * 0.125);

    switch (cgs.gametype) {
        case GT_CTF:
        case GT_1FCTF:
        case GT_OBELISK:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            break;
        default:
            return;
    }
    if (!(cg.predictedPlayerState.eFlags & EF_SPECTATOR) || cgs.gametype == GT_1FCTF) {
        return;
    }

    CG_ReadTeamBase(CS_REDTEAMBASE, redBase);
    CG_ReadTeamBase(CS_BLUETEAMBASE, blueBase);
    VectorCopy(cg.flagOrigin[TEAM_RED], redFlag);
    VectorCopy(cg.flagOrigin[TEAM_BLUE], blueFlag);
    for (i = 0; i < cgs.maxclients; i++) {
        const centity_t* cent = &cg_entities[i];

        if (!cgs.clientinfo[cent->currentState.clientNum].infoValid) {
            continue;
        }
        if (cent->currentState.powerups & (1 << PW_REDFLAG)) {
            VectorCopy(cent->lerpOrigin, redFlag);
        }
        if (cent->currentState.powerups & (1 << PW_BLUEFLAG)) {
            VectorCopy(cent->lerpOrigin, blueFlag);
        }
    }

    VectorSubtract(blueBase, redBase, delta);
    span = VectorLength(delta);
    if (span <= 0.0f || (!cgs.redflag && !cgs.blueflag)) {
        return;
    }

    CG_FillRect(rect->x - rect->w * 0.5f, rect->y, rect->w * 0.5f, barHeight, redHalf);
    CG_FillRect(rect->x, rect->y, rect->w * 0.5f, barHeight, blueHalf);

    if (cgs.redflag) {
        x = rect->x - size * 0.5f + rect->w * 0.5f -
            (CG_TeamBaseDistance(blueBase, redFlag, span) / span) * rect->w;
        CG_DrawPic(x + size * 0.25f, rect->y, size * 0.5f, size * 0.5f, cgs.media.flagTrackPointerShader);
        CG_DrawPic(x, rect->y + barHeight + size * 0.25f, size, size,
                   cgs.media.flagStatusHandles[TEAM_RED * 4 + (cgs.redflag == 1 ? 3 : 2)]);
    }
    if (cgs.blueflag) {
        x = rect->x - size * 0.5f - rect->w * 0.5f +
            (CG_TeamBaseDistance(redBase, blueFlag, span) / span) * rect->w;
        CG_DrawPic(x + size * 0.25f, rect->y, size * 0.5f, size * 0.5f, cgs.media.flagTrackPointerShader);
        CG_DrawPic(x, rect->y + barHeight + size * 0.25f, size, size,
                   cgs.media.flagStatusHandles[TEAM_BLUE * 4 + (cgs.blueflag == 1 ? 3 : 2)]);
    }
}

static void CG_DrawPlayerModel(rectDef_t *rect) {
    static const vec3_t angles = {5.0f, 210.0f, 0.0f};
    int weapon = cg_loadout.integer ? cg.snap->ps.weapon : cg.weaponSelect;

    CG_Draw3DPlayerModel(cg.scores[0].client, TORSO_ATTACK, rect->x, rect->y, rect->w, rect->h, weapon, 0.0f,
                         angles);
}

static void CG_Draw1stPlacePlayerModel(rectDef_t *rect) {
    static const vec3_t angles = {5.0f, 160.0f, 0.0f};
    int weapon = cg.scores[0].bestWeapon;

    CG_Draw3DPlayerModel(cg.scores[0].client, (weapon == WP_NONE || weapon == WP_GAUNTLET) ? TORSO_ATTACK2 : TORSO_ATTACK,
                         rect->x, rect->y, rect->w, rect->h, weapon, 0.0f, angles);
}

static void CG_DrawDuelStatusPlate(rectDef_t *rect, int player) {
    int clientNum = cg.duelScores[player].clientNum;
    int mine, theirs;
    const char *text;
    qhandle_t plate;
    float x;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid) {
        if (cg.intermissionStarted != 1) {
            return;
        }
        plate = cgs.media.duelPlateNotReady[player];
        text = "";
    } else if (!cg.warmup && cg.snap->ps.pm_type != PM_INTERMISSION) {
        mine = player ? cgs.score2ndPlayer : cgs.score1stPlayer;
        theirs = player ? cgs.score1stPlayer : cgs.score2ndPlayer;
        if (mine > theirs) {
            plate = cgs.media.duelPlateLeads[player];
            text = "LEADS";
        } else if (mine < theirs) {
            plate = cgs.media.duelPlateTrails[player];
            text = "TRAILS";
        } else {
            plate = cgs.media.duelPlateTied[player];
            text = "TIED";
        }
    } else if (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << clientNum)) {
        plate = cgs.media.duelPlateReady[player];
        text = "READY";
    } else {
        plate = cgs.media.duelPlateNotReady[player];
        text = "NOT READY";
    }

    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, plate);
    x = player ? rect->x - CG_DrawTextWidth(text, 0.16f, 0, 0) + 164.0f : rect->x + 16.0f;
    CG_DrawText(x, rect->y + 16.0f, 0, 0.16f, colorWhite, text, 0, 0, 3);
}

static void CG_DrawPlayerStatusLeft(rectDef_t *rect) {
    CG_DrawDuelStatusPlate(rect, 0);
}

static void CG_DrawPlayerStatusRight(rectDef_t *rect) {
    CG_DrawDuelStatusPlate(rect, 1);
}

static void CG_DrawModifiers(rectDef_t *rect, float scale, vec4_t color) {
    static const char *const weaponIcons[] = {
        "icons/iconw_gauntlet.tga", "icons/iconw_machinegun.tga", "icons/iconw_shotgun.tga",
        "icons/iconw_grenade.tga", "icons/iconw_rocket.tga", "icons/iconw_lightning.tga",
        "icons/iconw_railgun.tga", "icons/iconw_plasma.tga", "icons/iconw_bfg.tga",
        "icons/iconw_grapple.tga", "icons/iconw_nailgun.tga", "icons/iconw_proxlauncher.tga",
        "icons/iconw_chaingun.tga",
    };
    const char *info = CG_ConfigString(CS_SERVERINFO);
    int cs = cgs.customSettings;
    float x = rect->x;
    float y = rect->y;
    int lines = 0;

#define MODIFIER_LINE(text) do { CG_DrawText(x, y, 0, scale, color, (text), 0, 0, 0); y += 12.0f; lines++; } while (0)

    if (cs & 0x2000) {
        MODIFIER_LINE("AIR CONTROL");
    }
    if (cs & 0x4000) {
        MODIFIER_LINE("RAMP JUMPING");
    }
    if (cgs.armorTiered) {
        MODIFIER_LINE("TIERED ARMOR");
    }
    if (cs & 0x10000) {
        MODIFIER_LINE("MODIFIED WEAPON SWITCH");
    }
    if (cgs.quadDamageFactor != 3) {
        MODIFIER_LINE(va("%ix QUAD", atoi(Info_ValueForKey(info, "g_quadDamageFactor"))));
    }
    if (cs & 0x8000) {
        MODIFIER_LINE("MODIFIED PHYSICS");
    }
    if (cgs.gravity != 800) {
        MODIFIER_LINE(va("GRAVITY %i", atoi(Info_ValueForKey(info, "g_gravity"))));
    }
    if (cs & 0x20000) {
        MODIFIER_LINE("INSTAGIB");
    }
    if (cs & 0x40000) {
        MODIFIER_LINE("QUAD HOG");
    }
    if (cs & 0x80000) {
        MODIFIER_LINE("REGEN HEALTH");
    }
    if ((cs & 0x100000) && !(cs & 0x20000)) {
        MODIFIER_LINE("DROP DAMAGED HEALTH");
    }
    if (cs & 0x200000) {
        MODIFIER_LINE("VAMPIRIC DAMAGE");
    }
    if (cs & 0x400000) {
        MODIFIER_LINE("MODIFIED ITEM SPAWNING");
    }
    if (cs & 0x800000) {
        MODIFIER_LINE("HEADSHOTS ENABLED");
    }
    if (cs & 0x1000000) {
        MODIFIER_LINE("RAIL JUMPING");
    }
#undef MODIFIER_LINE

    if ((cs & 0xff) || (cs & 0x1f00)) {
        float ox = 0.0f;
        int n = 0;
        int i;

        CG_DrawText(x, y, 0, scale, color, "MODIFIED WEAPONS", 0, 0, 0);
        if (lines >= 15) {
            return;
        }
        y += 12.0f;
        for (i = 0; i < (int)ARRAY_LEN(weaponIcons); i++) {
            if (!(cs & (1 << i))) {
                continue;
            }
            CG_DrawPic(445.0f + ox, y, 16.0f, 16.0f, trap_R_RegisterShaderNoMip(weaponIcons[i]));
            CG_DrawPic(457.0f + ox, y + 8.0f, 8.0f, 8.0f, cgs.media.modifiedShader);
            ox += 24.0f;
            if (i < 11) {
                n++;
                if (i >= 7 && n > 7) {
                    y += 12.0f;
                    ox = 0.0f;
                    n = 0;
                }
            } else if (i == 11 && n + 1 > 7) {
                y += 12.0f;
                ox = 0.0f;
            }
        }
    }
}

static void CG_DrawWeaponHorizontal(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int starting = atoi(CG_ConfigString(CS_STARTING_WEAPONS));
    float x = 0.0f;
    qhandle_t icon;
    int i, h;

    for (i = WP_GAUNTLET; i < WP_NUM_WEAPONS; i++) {
        if (!(starting & (1 << (i - 1)))) {
            continue;
        }
        CG_RegisterWeapon(i);
        icon = cg_weapons[i].weaponIcon;
        if (!icon) {
            continue;
        }
        trap_R_SetColor(colorWhite);
        CG_DrawPic(rect->x + x, rect->y, rect->w, rect->h, icon);
        x += rect->w * 1.5f;
        trap_R_SetColor(NULL);
    }

    if (!cg_loadout.integer) {
        return;
    }
    h = CG_DrawTextHeight("+", scale, 0, 0);
    CG_DrawText(rect->x + x, rect->y + rect->h * 0.5f + 0.5f * h, 0, scale, color, "+", 0, 0, textStyle);
    x += rect->w;

    i = cg.weaponPrimaryQueued;
    if (i < 1 || i > 14) {
        i = WP_HMG;
    }
    CG_RegisterWeapon(i);
    icon = cg_weapons[i].weaponIcon;
    if (icon) {
        trap_R_SetColor(colorWhite);
        CG_DrawPic(rect->x + x, rect->y, rect->w, rect->h, icon);
        trap_R_SetColor(NULL);
    }
}

static void CG_DrawScoreByOwnerDraw(int ownerDraw, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int score;
    char buf[16];
    float x;

    if (ownerDraw == CG_PLAYER_SCORE) {
        if (cgs.gametype == GT_RACE) {
            score = cgs.clientinfo[cg.snap->ps.clientNum].score;
        } else {
            score = cg.snap->ps.persistant[PERS_SCORE];
        }
    } else if (ownerDraw == CG_1STPLACE) {
        score = cgs.scores1;
    } else if (ownerDraw == CG_2NDPLACE) {
        score = cgs.scores2;
    } else {
        return;
    }

    if (score == SCORE_NOT_PRESENT) {
        return;
    }

    if (cgs.gametype == GT_RACE) {
        if (score != 0x7fffffff && score >= 0) {
            Q_strncpyz(buf, CG_FormatRaceTime(score), sizeof(buf));
        } else {
            Q_strncpyz(buf, "-", sizeof(buf));
        }
    } else {
        Com_sprintf(buf, sizeof(buf), "%i", score);
    }

    x = CG_OwnerDrawAlignX(rect, buf, scale, align);
    CG_OwnerDrawText(x, rect->y, scale, color, buf, 0, 0, textStyle);
}

static void CG_DrawScoreboardPlayerHead(rectDef_t *rect, int ownerDraw) {
    int csIndex, clientNum;
    const char *s;

    switch (ownerDraw) {
        case CG_MOST_VALUABLE_OFFENSIVE_PLYR: csIndex = 697; break;
        case CG_MOST_VALUABLE_DEFENSIVE_PLYR: csIndex = 698; break;
        case CG_MOST_VALUABLE_PLYR:           csIndex = 699; break;
        case CG_BEST_ITEMCONTROL_PLYR:        csIndex = 696; break;
        case CG_MOST_ACCURATE_PLYR:           csIndex = 693; break;
        case CG_MOST_DAMAGEDEALT_PLYR:        csIndex = 692; break;
        default: return;
    }

    s = CG_ConfigString(csIndex);
    if (!s || !s[0]) {
        return;
    }
    clientNum = atoi(s);
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid) {
        return;
    }
    if (cgs.clientinfo[clientNum].modelIcon) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.clientinfo[clientNum].modelIcon);
    }
}

static void CG_DrawEndGameResult(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    playerState_t *ps = &cg.snap->ps;
    int score = ps->persistant[PERS_SCORE];
    const char *s;

    if (cgs.gametype == GT_RACE) {
        return;
    }
    if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }

    if (cgs.gametype < GT_TEAM) {
        if (score == SCORE_NOT_PRESENT) {
            s = "";
        } else if (score == -999) {
            s = "You forfeited the match.";
        } else {
            s = va("You finished %s with a score of %d",
                   CG_PlaceString(ps->persistant[PERS_RANK] + 1), score);
        }
    } else {
        int captures = ps->persistant[PERS_CAPTURES];
        int assists  = ps->persistant[PERS_ASSIST_COUNT];
        int defends  = ps->persistant[PERS_DEFEND_COUNT];

        if (captures > 0) {
            const char *plural = (captures == 1) ? "" : "s";
            if (cgs.gametype == GT_HARVESTER) {
                s = va("You captured %d skull%s.", captures, plural);
            } else {
                s = va("You had %d flag capture%s.", captures, plural);
            }
        } else if (assists > 0) {
            s = va("You had %d assist%s.", assists, (assists == 1) ? "" : "s");
        } else if (defends > 0) {
            s = va("You had %d defend%s.", defends, (defends == 1) ? "" : "s");
        } else if (score == SCORE_NOT_PRESENT) {
            s = "";
        } else if (score == -999) {
            s = "You forfeited the match.";
        } else {
            s = va("You finished with a score of %d.", score);
        }
    }

    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawTeamPlayersLeft(int team, rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int left = 0;

    if (team == TEAM_RED) {
        left = cgs.teamCountRed;
    } else if (team == TEAM_BLUE) {
        left = cgs.teamCountBlue;
    }
    CG_DrawText(rect->x, rect->y, 0, scale, color, va("%d", left), 0, 0, textStyle);
}

static void CG_DrawOwnTeamPlayersLeft(int useOwnTeam, rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int team = cg.snap->ps.persistant[PERS_TEAM];

    if (!useOwnTeam) {
        if (team == TEAM_RED) {
            team = TEAM_BLUE;
        } else if (team == TEAM_BLUE) {
            team = TEAM_RED;
        }
    }
    CG_DrawTeamPlayersLeft(team, rect, scale, color, textStyle);
}

static void CG_DrawTeamScore(int which, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int score = (which == CG_RED_SCORE) ? cgs.scores1 : cgs.scores2;
    const char *s;
    float x;

    if (score == SCORE_NOT_PRESENT || score == -999) {
        return;
    }
    s = va("%i", score);
    x = CG_OwnerDrawAlignX(rect, s, scale, align);
    CG_OwnerDrawText(x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawAdvert(rectDef_t *rect, vec4_t color, qhandle_t iconShader, int ownerDrawFlags) {
    if (ownerDrawFlags && !CG_OwnerDrawVisible(ownerDrawFlags, 0)) {
        return;
    }
    trap_R_SetColor(color);
    if (iconShader) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, iconShader);
    }
    trap_R_SetColor(NULL);
}

void CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int fontIndex) {
    rectDef_t rect;

    if (cg_drawStatus.integer == 0) {
        return;
    }

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    cg_currentFontIndex = fontIndex;

    switch (ownerDraw) {
        case CG_SERVER_SETTINGS:
            CG_DrawModifiers(&rect, scale, color);
            break;
        case CG_STARTING_WEAPONS:
            CG_DrawWeaponHorizontal(&rect, scale, color, textStyle);
            break;
        case CG_GAME_LIMIT:
            CG_DrawGameLimit(&rect, scale, color, textStyle, align);
            break;
        case CG_GAME_TYPE:
            CG_DrawGameType(&rect, scale, color, shader, textStyle);
            break;
        case CG_GAME_TYPE_ICON:
            CG_DrawGameTypeIcon(&rect);
            break;
        case CG_GAME_TYPE_MAP:
            CG_DrawGameTypeMap(&rect, scale, color, textStyle, align);
            break;
        case CG_GAME_STATUS:
            CG_DrawGameStatus(&rect, scale, color, shader, textStyle);
            break;
        case CG_MATCH_DETAILS:
            CG_DrawMatchDetails(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_END_CONDITION:
            CG_DrawWinCondition(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_STATUS:
            CG_DrawMatchStatus(&rect, scale, color, textStyle);
            break;
        case CG_CAPFRAGLIMIT:
            CG_DrawCapFragLimit(&rect, scale, color, textStyle, align);
            break;
        case CG_LEVELTIMER:
            CG_DrawLevelTimer(&rect, scale, color, textStyle, align);
            break;
        case CG_ROUND:
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawRound(&rect, scale, color, textStyle);
            }
            break;
        case CG_ROUNDTIMER:
            CG_DrawRoundTimer(&rect, scale, color, textStyle, align);
            break;
        case CG_OVERTIME:
            if (!cg.warmup) {
                CG_DrawOvertime(&rect, scale, color, textStyle, align);
            }
            break;
        case CG_LOCALTIME:
            CG_DrawLocalTime(&rect, fontIndex, scale, color, textStyle, align);
            break;
        case CG_PLAYER_COUNTS:
            CG_DrawPlayerCounts(&rect, scale, color, textStyle, align);
            break;
        case CG_MAP_NAME:
            CG_DrawMapName(&rect, scale, color, textStyle);
            break;
        case CG_VOTEGAMETYPE1:
            CG_DrawVoteGameType(&rect, scale, color, textStyle, align, 0);
            break;
        case CG_VOTEGAMETYPE2:
            CG_DrawVoteGameType(&rect, scale, color, textStyle, align, 1);
            break;
        case CG_VOTEGAMETYPE3:
            CG_DrawVoteGameType(&rect, scale, color, textStyle, align, 2);
            break;
        case CG_VOTEMAP1:
            CG_DrawVoteMapValue(&rect, scale, color, textStyle, align, "map", 0);
            break;
        case CG_VOTEMAP2:
            CG_DrawVoteMapValue(&rect, scale, color, textStyle, align, "map", 1);
            break;
        case CG_VOTEMAP3:
            CG_DrawVoteMapValue(&rect, scale, color, textStyle, align, "map", 2);
            break;
        case CG_VOTESHOT1:
            CG_DrawVoteMapShot(&rect, 0, shader);
            break;
        case CG_VOTESHOT2:
            CG_DrawVoteMapShot(&rect, 1, shader);
            break;
        case CG_VOTESHOT3:
            CG_DrawVoteMapShot(&rect, 2, shader);
            break;
        case CG_VOTENAME1:
            CG_DrawVoteMapValue(&rect, scale, color, textStyle, align, "title", 0);
            break;
        case CG_VOTENAME2:
            CG_DrawVoteMapValue(&rect, scale, color, textStyle, align, "title", 1);
            break;
        case CG_VOTENAME3:
            CG_DrawVoteMapValue(&rect, scale, color, textStyle, align, "title", 2);
            break;
        case CG_VOTECOUNT1:
        case CG_VOTECOUNT2:
        case CG_VOTECOUNT3:
            CG_DrawVotes(&rect, ownerDraw, fontIndex, scale, color, textStyle, align);
            break;
        case CG_VOTETIMER:
            CG_DrawVoteTimer(&rect, scale, color, textStyle, align);
            break;
        case CG_SPEC_MESSAGES:
            CG_DrawSpectator(rect.x, rect.y);
            break;
        case CG_PLAYER_HEAD:
            CG_DrawPlayerHead(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYERMODEL:
            CG_DrawPlayerModel(&rect);
            break;
        case CG_PLAYER_ARMOR_ICON:
            CG_DrawPlayerArmorIcon(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_ARMOR_ICON2D:
            CG_DrawPlayerArmorIcon(&rect, qtrue);
            break;
        case CG_PLAYER_ARMOR_VALUE:
            CG_DrawPlayerArmorValue(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_ARMOR_BAR_100:
            CG_DrawArmorBar100(&rect, shader);
            break;
        case CG_PLAYER_ARMOR_BAR_200:
            CG_DrawArmorBar200(&rect, shader);
            break;
        case CG_ARMORTIERED_COLORIZED:
            CG_DrawArmorTieredColorized(&rect, color, shader);
            break;
        case CG_PLAYER_HEALTH:
            CG_DrawPlayerHealth(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_HEALTH_BAR_100:
            CG_DrawHealthBar100(&rect, shader);
            break;
        case CG_PLAYER_HEALTH_BAR_200:
            CG_DrawHealthBar200(&rect, shader);
            break;
        case CG_PLAYER_AMMO_ICON:
            CG_DrawPlayerAmmoIcon(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_AMMO_ICON2D:
            CG_DrawPlayerAmmoIcon(&rect, qtrue);
            break;
        case CG_PLAYER_AMMO_VALUE:
            CG_DrawPlayerAmmoValue(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_ITEM:
            CG_DrawPlayerItem(&rect, scale, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_SCORE:
        case CG_1STPLACE:
        case CG_2NDPLACE:
            CG_DrawScoreByOwnerDraw(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_ONEFLAG_STATUS:
            CG_OneFlagStatus(&rect);
            break;
        case CG_PLAYER_HASFLAG:
        case CG_PLAYER_HASFLAG2D:
            CG_DrawPlayerHasFlag(&rect, ownerDraw == CG_PLAYER_HASFLAG2D);
            break;
        case CG_HARVESTER_SKULLS:
            CG_HarvesterSkulls(&rect, scale, color, qfalse, textStyle);
            break;
        case CG_HARVESTER_SKULLS2D:
            CG_HarvesterSkulls(&rect, scale, color, qtrue, textStyle);
            break;
        case CG_PLAYER_HASKEY:
            CG_DrawPlayerHasKey(&rect);
            break;
        case CG_CTF_POWERUP:
            CG_DrawPersistantPowerUp(&rect);
            break;
        case CG_AREA_POWERUP:
            CG_DrawAreaPowerUp(&rect, align, special, scale, color);
            break;
        case CG_TEAM_COLOR:
            CG_DrawTeamColor(&rect, color);
            break;
        case CG_KILLER:
            if (cg.killerName[0]) {
                CG_DrawKiller(&rect, scale, color, textStyle, align);
            }
            break;
        case CG_ACCURACY:
        case CG_ASSISTS:
        case CG_CAPTURES:
        case CG_DEFEND:
        case CG_EXCELLENT:
        case CG_GAUNTLET:
        case CG_IMPRESSIVE:
        case CG_PERFECT:
            CG_DrawMedal(ownerDraw, &rect, scale, color, shader);
            break;
        case CG_MOST_VALUABLE_OFFENSIVE_PLYR:
        case CG_MOST_VALUABLE_DEFENSIVE_PLYR:
        case CG_MOST_VALUABLE_PLYR:
        case CG_BEST_ITEMCONTROL_PLYR:
        case CG_MOST_ACCURATE_PLYR:
        case CG_MOST_DAMAGEDEALT_PLYR:
            CG_DrawScoreboardPlayerHead(&rect, ownerDraw);
            break;
        case CG_SPECTATORS:
            if (cg.notifyCount > 0) {
                CG_DrawScrollingNotify(&rect, fontIndex, scale, color);
            }
            break;
        case CG_MATCH_WINNER:
            CG_DrawMatchWinner(&rect, scale, color, textStyle);
            break;
        case CG_1ST_PLACE_SCORE:
            CG_DrawFirstPlaceScore(&rect, scale, color, textStyle);
            break;
        case CG_1STPLACE_PLYR_MODEL:
            CG_Draw1stPlacePlayerModel(&rect);
            break;
        case CG_2ND_PLACE_SCORE:
            CG_DrawSecondPlaceScore(&rect, scale, color, textStyle);
            break;
        case CG_PLAYER_OBIT:
            CG_DrawPlayerObit(&rect, scale);
            break;
        case CG_AREA_NEW_CHAT:
            CG_DrawChat(rect.x, rect.y, rect.w, rect.h, scale);
            break;
        case CG_PLYR_END_GAME_SCORE:
            CG_DrawEndGameResult(&rect, scale, color, textStyle);
            break;
        case CG_PLYR_BEST_WEAPON_NAME:
            CG_DrawBestWeaponName(&rect, scale, color, textStyle);
            break;
        case CG_SELECTED_PLYR_TEAM_COLOR:
            CG_DrawSelectedPlayerTeamColor(&rect);
            break;
        case CG_SELECTED_PLYR_ACCURACY:
            CG_DrawSelectedPlayerAccuracy(&rect, scale, color, textStyle);
            break;
        case CG_FOLLOW_PLAYER_NAME:
        case CG_FOLLOW_PLAYER_NAME_EX:
            CG_DrawFollowPlayerName(&rect, ownerDraw, align, scale, color, textStyle);
            break;
        case CG_SPEEDOMETER:
            CG_DrawSpeedometer(&rect, scale, color, textStyle);
            break;
        case CG_WP_VERTICAL:
            CG_DrawWeaponVertical(&rect, color);
            break;
        case CG_ACC_VERTICAL:
            CG_DrawAccuracyVertical(&rect, scale, color, textStyle);
            break;
        case CG_TEAM_COLORIZED:
            CG_DrawTeamColorized(&rect, color, shader);
            break;
        case CG_TEAM_PLYR_COUNT:
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawOwnTeamPlayersLeft(1, &rect, scale, color, textStyle);
            }
            break;
        case CG_ENEMY_PLYR_COUNT:
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawOwnTeamPlayersLeft(0, &rect, scale, color, textStyle);
            }
            break;
        case CG_1STPLACE_PLYR_MODEL_ACTIVE:
            break;

        case CG_1ST_PLYR: case CG_2ND_PLYR:
            CG_DrawDuelPlayerName(ownerDraw, &rect, fontIndex, scale, color, textStyle, align);
            break;
        case CG_1ST_PLYR_READY:
            CG_DrawPlayerStatusLeft(&rect);
            break;
        case CG_2ND_PLYR_READY:
            CG_DrawPlayerStatusRight(&rect);
            break;
        case CG_1ST_PLYR_SCORE: case CG_2ND_PLYR_SCORE:
            CG_DrawDuelPlayerScore(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_1ST_PLYR_FRAGS: case CG_2ND_PLYR_FRAGS:
            CG_DrawDuelPlayerFrags(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_DEATHS: case CG_2ND_PLYR_DEATHS:
            CG_DrawDuelPlayerDeaths(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_DMG: case CG_2ND_PLYR_DMG:
            CG_DrawDuelPlayerDmg(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_PING: case CG_2ND_PLYR_PING:
            CG_DrawDuelPlayerPing(&rect, scale, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_WINS: case CG_2ND_PLYR_WINS:
            CG_DrawDuelPlayerWins(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_ACC: case CG_2ND_PLYR_ACC:
            CG_DrawDuelPlayerAcc(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_FLAG: case CG_2ND_PLYR_FLAG:
            CG_DrawDuelPlayerFlag(&rect, ownerDraw);
            break;
        case CG_1ST_PLYR_AVATAR: case CG_2ND_PLYR_AVATAR:
            CG_DrawDuelPlayerAvatar(&rect, ownerDraw);
            break;
        case CG_1ST_PLYR_HEALTH_ARMOR: case CG_2ND_PLYR_HEALTH_ARMOR:
            CG_DrawDuelHealthArmorBar(&rect, ownerDraw);
            break;
        case CG_1ST_PLYR_FRAGS_G: case CG_1ST_PLYR_FRAGS_MG: case CG_1ST_PLYR_FRAGS_SG:
        case CG_1ST_PLYR_FRAGS_GL: case CG_1ST_PLYR_FRAGS_RL: case CG_1ST_PLYR_FRAGS_LG:
        case CG_1ST_PLYR_FRAGS_RG: case CG_1ST_PLYR_FRAGS_PG: case CG_1ST_PLYR_FRAGS_BFG:
        case CG_1ST_PLYR_FRAGS_CG: case CG_1ST_PLYR_FRAGS_NG: case CG_1ST_PLYR_FRAGS_PL:
        case CG_1ST_PLYR_FRAGS_HMG:
        case CG_2ND_PLYR_FRAGS_G: case CG_2ND_PLYR_FRAGS_MG: case CG_2ND_PLYR_FRAGS_SG:
        case CG_2ND_PLYR_FRAGS_GL: case CG_2ND_PLYR_FRAGS_RL: case CG_2ND_PLYR_FRAGS_LG:
        case CG_2ND_PLYR_FRAGS_RG: case CG_2ND_PLYR_FRAGS_PG: case CG_2ND_PLYR_FRAGS_BFG:
        case CG_2ND_PLYR_FRAGS_CG: case CG_2ND_PLYR_FRAGS_NG: case CG_2ND_PLYR_FRAGS_PL:
        case CG_2ND_PLYR_FRAGS_HMG:
            CG_DrawDuelWeaponFrags(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_HITS_MG: case CG_1ST_PLYR_HITS_SG: case CG_1ST_PLYR_HITS_GL:
        case CG_1ST_PLYR_HITS_RL: case CG_1ST_PLYR_HITS_LG: case CG_1ST_PLYR_HITS_RG:
        case CG_1ST_PLYR_HITS_PG: case CG_1ST_PLYR_HITS_BFG: case CG_1ST_PLYR_HITS_CG:
        case CG_1ST_PLYR_HITS_NG: case CG_1ST_PLYR_HITS_PL: case CG_1ST_PLYR_HITS_HMG:
        case CG_2ND_PLYR_HITS_MG: case CG_2ND_PLYR_HITS_SG: case CG_2ND_PLYR_HITS_GL:
        case CG_2ND_PLYR_HITS_RL: case CG_2ND_PLYR_HITS_LG: case CG_2ND_PLYR_HITS_RG:
        case CG_2ND_PLYR_HITS_PG: case CG_2ND_PLYR_HITS_BFG: case CG_2ND_PLYR_HITS_CG:
        case CG_2ND_PLYR_HITS_NG: case CG_2ND_PLYR_HITS_PL: case CG_2ND_PLYR_HITS_HMG:
            CG_DrawDuelWeaponHits(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_SHOTS_MG: case CG_1ST_PLYR_SHOTS_SG: case CG_1ST_PLYR_SHOTS_GL:
        case CG_1ST_PLYR_SHOTS_RL: case CG_1ST_PLYR_SHOTS_LG: case CG_1ST_PLYR_SHOTS_RG:
        case CG_1ST_PLYR_SHOTS_PG: case CG_1ST_PLYR_SHOTS_BFG: case CG_1ST_PLYR_SHOTS_CG:
        case CG_1ST_PLYR_SHOTS_NG: case CG_1ST_PLYR_SHOTS_PL: case CG_1ST_PLYR_SHOTS_HMG:
        case CG_2ND_PLYR_SHOTS_MG: case CG_2ND_PLYR_SHOTS_SG: case CG_2ND_PLYR_SHOTS_GL:
        case CG_2ND_PLYR_SHOTS_RL: case CG_2ND_PLYR_SHOTS_LG: case CG_2ND_PLYR_SHOTS_RG:
        case CG_2ND_PLYR_SHOTS_PG: case CG_2ND_PLYR_SHOTS_BFG: case CG_2ND_PLYR_SHOTS_CG:
        case CG_2ND_PLYR_SHOTS_NG: case CG_2ND_PLYR_SHOTS_PL: case CG_2ND_PLYR_SHOTS_HMG:
            CG_DrawDuelWeaponShots(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_DMG_G: case CG_1ST_PLYR_DMG_MG: case CG_1ST_PLYR_DMG_SG:
        case CG_1ST_PLYR_DMG_GL: case CG_1ST_PLYR_DMG_RL: case CG_1ST_PLYR_DMG_LG:
        case CG_1ST_PLYR_DMG_RG: case CG_1ST_PLYR_DMG_PG: case CG_1ST_PLYR_DMG_BFG:
        case CG_1ST_PLYR_DMG_CG: case CG_1ST_PLYR_DMG_NG: case CG_1ST_PLYR_DMG_PL:
        case CG_1ST_PLYR_DMG_HMG:
        case CG_2ND_PLYR_DMG_G: case CG_2ND_PLYR_DMG_MG: case CG_2ND_PLYR_DMG_SG:
        case CG_2ND_PLYR_DMG_GL: case CG_2ND_PLYR_DMG_RL: case CG_2ND_PLYR_DMG_LG:
        case CG_2ND_PLYR_DMG_RG: case CG_2ND_PLYR_DMG_PG: case CG_2ND_PLYR_DMG_BFG:
        case CG_2ND_PLYR_DMG_CG: case CG_2ND_PLYR_DMG_NG: case CG_2ND_PLYR_DMG_PL:
        case CG_2ND_PLYR_DMG_HMG:
            CG_DrawDuelWeaponDmg(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_ACC_MG: case CG_1ST_PLYR_ACC_SG: case CG_1ST_PLYR_ACC_GL:
        case CG_1ST_PLYR_ACC_RL: case CG_1ST_PLYR_ACC_LG: case CG_1ST_PLYR_ACC_RG:
        case CG_1ST_PLYR_ACC_PG: case CG_1ST_PLYR_ACC_BFG: case CG_1ST_PLYR_ACC_CG:
        case CG_1ST_PLYR_ACC_NG: case CG_1ST_PLYR_ACC_PL: case CG_1ST_PLYR_ACC_HMG:
        case CG_2ND_PLYR_ACC_MG: case CG_2ND_PLYR_ACC_SG: case CG_2ND_PLYR_ACC_GL:
        case CG_2ND_PLYR_ACC_RL: case CG_2ND_PLYR_ACC_LG: case CG_2ND_PLYR_ACC_RG:
        case CG_2ND_PLYR_ACC_PG: case CG_2ND_PLYR_ACC_BFG: case CG_2ND_PLYR_ACC_CG:
        case CG_2ND_PLYR_ACC_NG: case CG_2ND_PLYR_ACC_PL: case CG_2ND_PLYR_ACC_HMG:
            CG_DrawDuelWeaponAcc(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_PICKUPS: case CG_1ST_PLYR_PICKUPS_RA: case CG_1ST_PLYR_PICKUPS_YA:
        case CG_1ST_PLYR_PICKUPS_GA: case CG_1ST_PLYR_PICKUPS_MH:
        case CG_2ND_PLYR_PICKUPS: case CG_2ND_PLYR_PICKUPS_RA: case CG_2ND_PLYR_PICKUPS_YA:
        case CG_2ND_PLYR_PICKUPS_GA: case CG_2ND_PLYR_PICKUPS_MH:
            CG_DrawDuelPlayerPickups(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_AVG_PICKUP_TIME_RA: case CG_1ST_PLYR_AVG_PICKUP_TIME_YA:
        case CG_1ST_PLYR_AVG_PICKUP_TIME_GA: case CG_1ST_PLYR_AVG_PICKUP_TIME_MH:
        case CG_2ND_PLYR_AVG_PICKUP_TIME_RA: case CG_2ND_PLYR_AVG_PICKUP_TIME_YA:
        case CG_2ND_PLYR_AVG_PICKUP_TIME_GA: case CG_2ND_PLYR_AVG_PICKUP_TIME_MH:
            CG_DrawDuelAvgPickupTime(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_EXCELLENT: case CG_1ST_PLYR_IMPRESSIVE: case CG_1ST_PLYR_HUMILIATION:
        case CG_2ND_PLYR_EXCELLENT: case CG_2ND_PLYR_IMPRESSIVE: case CG_2ND_PLYR_HUMILIATION:
            CG_DrawDuelAwards(&rect, scale, color, textStyle, align, ownerDraw);
            break;

        case CG_RED_FLAGSTATUS:
        case CG_RED_BASESTATUS:
        case CG_BLUE_FLAGSTATUS:
        case CG_BLUE_BASESTATUS:
            CG_DrawFlagStatus(&rect, ownerDraw);
            break;
        case CG_RED_SCORE:
        case CG_BLUE_SCORE:
            CG_DrawTeamScore(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_RED_NAME:
        case CG_BLUE_NAME:
            CG_DrawTeamName(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_RED_OWNED_FLAGS:
            if (cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
                CG_DrawTeamAliveCount(&rect, TEAM_RED, scale, color, textStyle);
            }
            break;
        case CG_BLUE_OWNED_FLAGS:
            if (cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
                CG_DrawTeamAliveCount(&rect, TEAM_BLUE, scale, color, textStyle);
            }
            break;
        case CG_RED_AVG_PING:
            CG_DrawTeamAvgPing(&rect, scale, textStyle, align, TEAM_RED);
            break;
        case CG_BLUE_AVG_PING:
            CG_DrawTeamAvgPing(&rect, scale, textStyle, align, TEAM_BLUE);
            break;
        case CG_RED_PLAYER_COUNT:
            CG_DrawPlayerCount2(&rect, TEAM_RED, scale, color, textStyle, align);
            break;
        case CG_BLUE_PLAYER_COUNT:
            CG_DrawPlayerCount2(&rect, TEAM_BLUE, scale, color, textStyle, align);
            break;
        case CG_RED_CLAN_PLYRS:
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawTeamPlayersLeft(TEAM_RED, &rect, scale, color, textStyle);
            }
            break;
        case CG_BLUE_CLAN_PLYRS:
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawTeamPlayersLeft(TEAM_BLUE, &rect, scale, color, textStyle);
            }
            break;
        case CG_RED_TIMEOUT_COUNT:
            CG_DrawTimeoutCount(&rect, scale, color, textStyle, align, CS_TIMEOUTS_RED);
            break;
        case CG_BLUE_TIMEOUT_COUNT:
            CG_DrawTimeoutCount(&rect, scale, color, textStyle, align, CS_TIMEOUTS_BLUE);
            break;

        case CG_RED_TEAM_MAP_PICKUPS:
        case CG_RED_TEAM_PICKUPS_RA: case CG_RED_TEAM_PICKUPS_YA:
        case CG_RED_TEAM_PICKUPS_GA: case CG_RED_TEAM_PICKUPS_MH:
        case CG_RED_TEAM_PICKUPS_QUAD: case CG_RED_TEAM_PICKUPS_BS:
        case CG_RED_TEAM_PICKUPS_FLAG: case CG_RED_TEAM_PICKUPS_MEDKIT:
        case CG_RED_TEAM_PICKUPS_REGEN: case CG_RED_TEAM_PICKUPS_HASTE:
        case CG_RED_TEAM_PICKUPS_INVIS:
        case CG_BLUE_TEAM_MAP_PICKUPS:
        case CG_BLUE_TEAM_PICKUPS_RA: case CG_BLUE_TEAM_PICKUPS_YA:
        case CG_BLUE_TEAM_PICKUPS_GA: case CG_BLUE_TEAM_PICKUPS_MH:
        case CG_BLUE_TEAM_PICKUPS_QUAD: case CG_BLUE_TEAM_PICKUPS_BS:
        case CG_BLUE_TEAM_PICKUPS_FLAG: case CG_BLUE_TEAM_PICKUPS_MEDKIT:
        case CG_BLUE_TEAM_PICKUPS_REGEN: case CG_BLUE_TEAM_PICKUPS_HASTE:
        case CG_BLUE_TEAM_PICKUPS_INVIS:
            CG_DrawTeamPickups(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_RED_TEAM_TIMEHELD_QUAD:
        case CG_RED_TEAM_TIMEHELD_BS:
        case CG_RED_TEAM_TIMEHELD_FLAG: case CG_RED_TEAM_TIMEHELD_REGEN:
        case CG_RED_TEAM_TIMEHELD_HASTE: case CG_RED_TEAM_TIMEHELD_INVIS:
        case CG_BLUE_TEAM_TIMEHELD_QUAD: case CG_BLUE_TEAM_TIMEHELD_BS:
        case CG_BLUE_TEAM_TIMEHELD_FLAG: case CG_BLUE_TEAM_TIMEHELD_REGEN:
        case CG_BLUE_TEAM_TIMEHELD_HASTE: case CG_BLUE_TEAM_TIMEHELD_INVIS:
            CG_DrawTeamTimeHeld(&rect, scale, color, textStyle, align, ownerDraw);
            break;

        case CG_FLAG_STATUS:
            CG_DrawFlagStatusBar(&rect);
            break;
        case CG_HEALTH_COLORIZED:
            CG_DrawHealthColorized(&rect, color, shader);
            break;
        case CG_MATCH_STATE:
            cg_currentFontIndex = fontIndex ? fontIndex : 1;
            CG_DrawMatchState(&rect, scale, color, textStyle);
            break;

        case CG_RACE_STATUS:
            CG_DrawRaceStatus(&rect, scale, color, textStyle);
            break;
        case CG_RACE_TIMES:
            CG_DrawRaceTimes(&rect, scale, color, textStyle);
            break;

        case UI_ADVERT:
            CG_DrawAdvert(&rect, color, shader, ownerDrawFlags);
            break;

        default:
            break;
    }
}

void CG_MouseEvent(int x, int y) {
    float xscale43;
    int n;
    int bias;

    xscale43 = (4.0f * cgs.glconfig.vidHeight / 3.0f) / (float)SCREEN_WIDTH;
    if (xscale43 <= 0.0f || cgs.screenYScale <= 0.0f) {
        return;
    }
    bias = (int)(cgs.widescreenBias / xscale43);

    cgs.cursorX = (int)((x - cgs.widescreenBias) / xscale43);
    if (cgs.cursorX < -bias)
        cgs.cursorX = -bias;
    else if (cgs.cursorX > SCREEN_WIDTH + bias)
        cgs.cursorX = SCREEN_WIDTH + bias;

    cgs.cursorY = (int)(y / cgs.screenYScale);
    if (cgs.cursorY < 0)
        cgs.cursorY = 0;
    else if (cgs.cursorY > SCREEN_HEIGHT)
        cgs.cursorY = SCREEN_HEIGHT;

    n = Display_CursorType(cgs.cursorX, cgs.cursorY);
    cgs.activeCursor = 0;
    if (n == CURSOR_ARROW) {
        cgs.activeCursor = cgs.media.selectCursor;
    } else if (n == CURSOR_SIZER) {
        cgs.activeCursor = cgs.media.sizeCursor;
    }

    cgDC.cursorx = cgs.cursorX;
    cgDC.cursory = cgs.cursorY;

    if (Menu_Count() > 0) {
        Display_MouseMove(NULL, cgs.cursorX, cgs.cursorY);
    }
}

void CG_HideTeamMenu(void) {
    Menus_CloseByName("teamMenu");
    Menus_CloseByName("getMenu");
}

void CG_ShowTeamMenu(void) {
    Menus_OpenByName("teamMenu");
}

void CG_EventHandling(int type) {
    cgs.eventHandling = type;
    if (type == CGAME_EVENT_NONE) {
        CG_HideTeamMenu();
    } else if (type == CGAME_EVENT_RESOLUTION) {
        CG_ReadResolution();
    } else if (type == CGAME_EVENT_ESCAPE) {
        CG_HideJoinMenu();
    }
}

void CG_KeyEvent(int key, qboolean down) {
    menuDef_t* menu;

    if (CG_DemoHUDKey(key, down)) {
        return;
    }

    if (down) {
        char binding[32];

        trap_Key_GetBindingBuf(key, binding, sizeof(binding));
        if (!Q_strncmp(binding, "messagemode", 11) || !Q_strncmp(binding, "screenshot", 10)) {
            trap_SendConsoleCommand(va("%s\n", binding));
            return;
        }
        if (!Q_strncmp(binding, "+voice", 6)) {
            trap_SendConsoleCommand("+voice\n");
            return;
        }
        if (!Q_strncmp(binding, "+scores", 7)) {
            cg.showScores = qtrue;
            CG_ScoresDown_f();
            return;
        }
    }

    if (Menu_Count() <= 0) {
        return;
    }

    menu = Menu_GetFocused();
    if (menu == NULL) {
        return;
    }

    Menu_HandleKey(menu, key, down);
}

int CG_ClientNumFromName(const char* p) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && Q_stricmp(cgs.clientinfo[i].name, p) == 0) {
            return i;
        }
    }
    return -1;
}

void CG_ShowResponseHead(void) {
    float x, y, w, h;

    x = 72;
    y = w = h = 0;
    CG_AdjustFrom640(&x, &y, &w, &h);

    Menus_OpenByName("voiceMenu");
    trap_Cvar_Set("cl_conXOffset", va("%d", (int)x));
    cg.voiceTime = cg.time;
}

void CG_RunMenuScript(char** args) {
}

void CG_GetTeamColor(vec4_t* color) {
    if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
        (*color)[0] = 1.0f;
        (*color)[3] = 0.25f;
        (*color)[1] = (*color)[2] = 0.0f;
    } else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
        (*color)[0] = (*color)[1] = 0.0f;
        (*color)[2] = 1.0f;
        (*color)[3] = 0.25f;
    } else {
        (*color)[0] = (*color)[2] = 0.0f;
        (*color)[1] = 0.17f;
        (*color)[3] = 0.25f;
    }
}
