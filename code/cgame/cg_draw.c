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
menuDef_t* menuScoreboard = NULL;
menuDef_t* menuEndScoreboard = NULL;
menuDef_t* menuJoinGame = NULL;
menuDef_t* menuStats = NULL;

menuDef_t* menuSpecHud = NULL;
menuDef_t* menuSpecFollowHud = NULL;
menuDef_t* menuSpecRedTop = NULL;
menuDef_t* menuSpecRedBottom = NULL;
menuDef_t* menuSpecBlueTop = NULL;
menuDef_t* menuSpecBlueBottom = NULL;

void CG_CacheMenus(void) {
    CG_SetEndScoreboardMenu();
    menuStats = Menus_FindByName("stats_menu");
    menuJoinGame = Menus_FindByName("joingame_menu");
    menuSpecHud = Menus_FindByName("comp_spechud_menu");
    menuSpecFollowHud = Menus_FindByName("comp_specfollowhud_menu");
    menuSpecRedTop = Menus_FindByName("comp_specred_top");
    menuSpecRedBottom = Menus_FindByName("comp_specred_bottom");
    menuSpecBlueTop = Menus_FindByName("comp_specblue_top");
    menuSpecBlueBottom = Menus_FindByName("comp_specblue_bottom");
}

static void CG_DrawAccuracyPanel(void) {
    if ((cg.predictedPlayerState.pm_type == PM_SPECTATOR && !(cg.predictedPlayerState.pm_flags & PMF_FOLLOW)) ||
        !cg.accShowing) {
        return;
    }
    if (cg.accRequestTime + 1000 < cg.time) {
        cg.accRequestTime = cg.time;
        trap_SendClientCommand("acc");
    }
    Menu_Paint(menuStats, qtrue);
}

void CG_DrawSpectatorHud(void) {
    menuDef_t* menu;

    if (cg.scoreBoardShowing) {
        return;
    }

    menu = menuSpecFollowHud;
    if (!menu || !(cg.snap->ps.pm_flags & PMF_FOLLOW)) {
        menu = menuSpecHud;
    }
    Menu_Paint(menu, qtrue);

    Menu_Paint(menuSpecRedTop, qtrue);
    Menu_Paint(menuSpecRedBottom, qtrue);
    Menu_Paint(menuSpecBlueTop, qtrue);
    Menu_Paint(menuSpecBlueBottom, qtrue);

    if (!cg.scoreBoardShowing) {
        CG_DrawChat(5, 287, 634, 120, 1.0f);
    }
}

int sortedTeamPlayers[TEAM_MAXOVERLAY];
int numSortedTeamPlayers;

char systemChat[256];
char teamChat1[256];
char teamChat2[256];

void CG_KeyNameForCommand(const char* command, char* buf, int buflen) {
	int key;

	key = trap_Key_GetKey(command);
	if (key >= 0) {
		trap_Key_KeynumToStringBuf(key, buf, buflen);
	} else {
		Q_strncpyz(buf, "???", buflen);
	}
}

static int CG_EngineFont(int fontIndex) {
	if (fontIndex >= 0 && fontIndex < 3)
		return fontIndex;
	return 0;
}

static int CG_IndexForFontPtr(const fontInfo_t* font) {
	int i;
	for (i = 0; i < 3; i++) {
		if (font == &cgDC.Assets.extraFonts[i])
			return i;
	}
	return 0;
}

static float CG_FontPixelSize(float scale) {
	return (((float)cgs.glconfig.vidHeight / 768.0f) * 96.0f) * scale;
}

static float CG_TextXScale(void) {
	int ws = cg_currentWidescreen;
	if (cgs.widescreenBias > 0.0f && (ws != 0 || CG_MenuHoldsKeys()))
		return (cgs.glconfig.vidHeight * 4.0f / 3.0f) / 640.0f;
	return cgs.screenXScale;
}

static void CG_TextToScreen(float* x, float* y) {
	float xscale = CG_TextXScale();
	float xbias = 0.0f;
	int ws = cg_currentWidescreen;
	if (cgs.widescreenBias > 0.0f && (ws != 0 || CG_MenuHoldsKeys())) {
		if (ws == 2 || CG_MenuHoldsKeys())
			xbias = cgs.widescreenBias;
		else if (ws == 3)
			xbias = cgs.widescreenBias * 2.0f;
	}
	if (x)
		*x = *x * xscale + xbias;
	if (y)
		*y = *y * cgs.screenYScale;
}

static void CG_PaintTextEngine(float x, float y, int efont, float scale, const vec4_t color,
                               const char* text, int limit, int style) {
	float sx = x, sy = y;
	float size = CG_FontPixelSize(scale);
	int lim = (limit > 0) ? limit : -1;

	if (!text || !*text)
		return;

	CG_TextToScreen(&sx, &sy);

	if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
		float ofs = (style == ITEM_TEXTSTYLE_SHADOWED) ? 1.0f : 2.0f;
		float shx = x + ofs, shy = y + ofs;
		vec4_t shadow;
		shadow[0] = shadow[1] = shadow[2] = 0.0f;
		shadow[3] = color ? color[3] : 1.0f;
		CG_TextToScreen(&shx, &shy);
		trap_R_SetColor(shadow);
		trap_R_Font_DrawString((int)shx, (int)shy, text, efont, size, lim, NULL, TEXT_NORECOLOR);
	}

	trap_R_SetColor(color);
	trap_R_Font_DrawString((int)sx, (int)sy, text, efont, size, lim, NULL, 0);
	trap_R_SetColor(NULL);
}

static void CG_PaintText(float x, float y, int fontIndex, float scale, const vec4_t color,
                         const char* text, int limit, int style) {
	CG_PaintTextEngine(x, y, CG_EngineFont(fontIndex), scale, color, text, limit, style);
}

void CG_DrawTextInFont(float x, float y, int engineFont, float scale, const vec4_t color, const char* text, int style) {
	CG_PaintTextEngine(x, y, engineFont, scale, color, text, 0, style);
}

void CG_MeasureTextInFont(const char* text, int engineFont, float scale, float* w, float* h) {
	float extents[5];
	float xscale = CG_TextXScale();
	float yscale = cgs.screenYScale;

	trap_R_Font_TextExtents(text, 0, engineFont, CG_FontPixelSize(scale), -1, extents);
	if (xscale <= 0.0f)
		xscale = 1.0f;
	if (yscale <= 0.0f)
		yscale = 1.0f;
	if (w)
		*w = (float)(int)((extents[2] - extents[0]) / xscale);
	if (h)
		*h = (float)(int)(extents[4] / yscale);
}

static void CG_MeasureText(const char* text, float scale, int fontIndex, int limit, int* w640, int* h640) {
	float extents[5];
	float xscale, yscale;
	float size = CG_FontPixelSize(scale);
	int lim = (limit > 0) ? limit : -1;

	trap_R_Font_TextExtents(text, 0, CG_EngineFont(fontIndex), size, lim, extents);

	xscale = CG_TextXScale();
	if (xscale <= 0.0f)
		xscale = 1.0f;
	yscale = cgs.screenYScale;
	if (yscale <= 0.0f)
		yscale = 1.0f;

	if (w640)
		*w640 = (int)((extents[2] - extents[0]) / xscale);
	if (h640)
		*h640 = (int)(extents[4] / yscale);
}

float CG_Text_Width_Font(const char* text, float scale, int limit, fontInfo_t* font) {
	int w = 0;
	CG_MeasureText(text, scale, CG_IndexForFontPtr(font), limit, &w, NULL);
	return (float)w;
}

float CG_Text_Height_Font(const char* text, float scale, int limit, fontInfo_t* font) {
	int h = 0;
	CG_MeasureText(text, scale, CG_IndexForFontPtr(font), limit, NULL, &h);
	return (float)h;
}

void CG_Text_Paint_Font(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, fontInfo_t* font) {
	(void)adjust;
	CG_PaintText(x, y, CG_IndexForFontPtr(font), scale, color, text, limit, style);
}

void CG_DrawText_DC(float x, float y, float scale, vec4_t color, const char* text,
					float adjust, int limit, int style, int fontIndex) {
	(void)adjust;
	CG_PaintText(x, y, fontIndex, scale, color, text, limit, style);
}

float CG_TextWidth_DC(const char* text, float scale, int limit, int fontIndex) {
	int w = 0;
	CG_MeasureText(text, scale, fontIndex, limit, &w, NULL);
	return (float)w;
}

float CG_TextHeight_DC(const char* text, float scale, int limit, int fontIndex) {
	int h = 0;
	CG_MeasureText(text, scale, fontIndex, limit, NULL, &h);
	return (float)h;
}

void CG_DrawTextWithCursor_DC(float x, float y, float scale, vec4_t color, const char* text,
							  int cursorPos, char cursor, int limit, int style, int fontIndex) {
	(void)cursorPos;
	(void)cursor;
	CG_PaintText(x, y, fontIndex, scale, color, text, limit, style);
}

void CG_DrawText(float x, float y, int fontIndex, float scale, vec4_t color,
                 const char *text, float adjust, int maxChars, int textStyle) {
	(void)adjust;
	CG_PaintText(x, y, fontIndex, scale, color, text, maxChars, textStyle);
}

int CG_DrawTextWidth(const char *text, float scale, int limit, int fontIndex) {
	int w = 0;
	CG_MeasureText(text, scale, fontIndex, limit, &w, NULL);
	return w;
}

int CG_DrawTextHeight(const char *text, float scale, int limit, int fontIndex) {
	int h = 0;
	CG_MeasureText(text, scale, fontIndex, limit, NULL, &h);
	return h;
}

float CG_DrawTextCapHeight(float scale, int fontIndex) {
	float extents[5];
	float yscale = cgs.screenYScale;

	trap_R_Font_TextExtents("H", 0, CG_EngineFont(fontIndex), CG_FontPixelSize(scale), -1, extents);

	if (yscale <= 0.0f) {
		yscale = 1.0f;
	}

	return extents[1] < 0.0f ? -extents[1] / yscale : 0.0f;
}

int CG_DrawTextLineHeight(float scale, int fontIndex) {
	float extents[5];
	float yscale = cgs.screenYScale;
	float below;

	trap_R_Font_TextExtents("Ag", 0, CG_EngineFont(fontIndex), CG_FontPixelSize(scale), -1, extents);

	if (yscale <= 0.0f) {
		yscale = 1.0f;
	}

	below = extents[3] > 0.0f ? extents[3] : 0.0f;

	return (int)((extents[4] + below) / yscale);
}

void CG_DrawTextClipped(float *maxX, float x, float y, float scale, const vec4_t color, const char *text) {
	float sx = x, sy = y;
	float clip = 0.0f;

	if (maxX)
		clip = *maxX * CG_TextXScale();

	CG_TextToScreen(&sx, &sy);
	trap_R_SetColor(color);
	trap_R_Font_DrawString((int)sx, (int)sy, text, 0, CG_FontPixelSize(scale), -1, maxX ? &clip : NULL, 0);
	trap_R_SetColor(NULL);

	if (maxX) {
		if (cgs.widescreenBias > 0.0f)
			*maxX = clip / ((cgs.glconfig.vidHeight * 4.0f / 3.0f) / 640.0f);
		else
			*maxX = clip / cgs.screenXScale;
	}
}

int CG_Text_Width(const char* text, float scale, int limit) {
	int w = 0;
	CG_MeasureText(text, scale, 0, limit, &w, NULL);
	return w;
}

int CG_Text_Height(const char* text, float scale, int limit) {
	int h = 0;
	CG_MeasureText(text, scale, 0, limit, NULL, &h);
	return h;
}

void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
	float w, h;
	w = width * scale;
	h = height * scale;
	CG_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style) {
	(void)adjust;
	CG_PaintText(x, y, 0, scale, color, text, limit, style);
}

void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles,
					const byte* color) {
	refdef_t refdef;
	refEntity_t ent;

	if (!cg_draw3dIcons.integer || !cg_drawIcons.integer) {
		return;
	}

	CG_AdjustFrom640(&x, &y, &w, &h);

	memset(&refdef, 0, sizeof(refdef));

	memset(&ent, 0, sizeof(ent));
	AnglesToAxis(angles, ent.axis);
	VectorCopy(origin, ent.origin);
	ent.hModel = model;
	ent.customSkin = skin;
	ent.renderfx = RF_NOSHADOW;
	if (color) {
		memcpy(ent.shaderRGBA, color, sizeof(ent.shaderRGBA));
	}

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear(refdef.viewaxis);

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene(&ent);
	trap_R_RenderScene(&refdef);
}

void CG_DrawHead(float x, float y, float w, float h, int clientNum, vec3_t headAngles, const byte* color) {
	clipHandle_t cm;
	clientInfo_t* ci;
	float len;
	vec3_t origin;
	vec3_t mins, maxs;

	ci = &cgs.clientinfo[clientNum];

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

		CG_Draw3DModel(x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles, color);
	} else if (cg_drawIcons.integer) {
		CG_DrawPic(x, y, w, h, ci->modelIcon);
	}

	if (ci->deferred) {
		CG_DrawPic(x, y, w, h, cgs.media.deferShader);
	}
}

void CG_DrawFlagModel(float x, float y, float w, float h, int team, qboolean force2D) {
	qhandle_t cm;
	float len;
	vec3_t origin, angles;
	vec3_t mins, maxs;
	qhandle_t handle;

	if (!force2D && cg_draw3dIcons.integer) {
		VectorClear(angles);

		cm = cgs.media.redFlagModel;

		trap_R_ModelBounds(cm, mins, maxs);

		origin[2] = -0.5 * (mins[2] + maxs[2]);
		origin[1] = 0.5 * (mins[1] + maxs[1]);

		len = 0.5 * (maxs[2] - mins[2]);
		origin[0] = len / 0.268;

		angles[YAW] = 60 * sin(cg.time / 2000.0);
		;

		if (team == TEAM_RED) {
			handle = cgs.media.redFlagModel;
		} else if (team == TEAM_BLUE) {
			handle = cgs.media.blueFlagModel;
		} else if (team == TEAM_FREE) {
			handle = cgs.media.neutralFlagModel;
		} else {
			return;
		}
		CG_Draw3DModel(x, y, w, h, handle, 0, origin, angles, NULL);
	} else if (cg_drawIcons.integer) {
		gitem_t* item;

		if (team == TEAM_RED) {
			item = BG_FindItemForPowerup(PW_REDFLAG);
		} else if (team == TEAM_BLUE) {
			item = BG_FindItemForPowerup(PW_BLUEFLAG);
		} else if (team == TEAM_FREE) {
			item = BG_FindItemForPowerup(PW_NEUTRALFLAG);
		} else {
			return;
		}
		if (item) {
			CG_DrawPic(x, y, w, h, cg_items[ITEM_INDEX(item)].icon);
		}
	}
}

void CG_DrawTeamBackground(int x, int y, int w, int h, float alpha, int team) {
	vec4_t hcolor;

	hcolor[3] = alpha;
	if (team == TEAM_RED) {
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	} else if (team == TEAM_BLUE) {
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
	} else {
		return;
	}
	trap_R_SetColor(hcolor);
	CG_DrawPic(x, y, w, h, cgs.media.teamStatusBar);
	trap_R_SetColor(NULL);
}

static float CG_DrawAttacker(float y) {
	int t;
	float size;
	vec3_t angles;
	const char* info;
	const char* name;
	int clientNum;
	int packed;
	byte color[4];

	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
		return y;
	}

	if (!cg.attackerTime) {
		return y;
	}

	t = cg.time - cg.attackerTime;
	if (t > ATTACKER_HEAD_TIME) {
		return y;
	}

	clientNum = cg.predictedPlayerState.persistant[PERS_ATTACKER];
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || clientNum == cg.snap->ps.clientNum) {
		return y;
	}

	if (cgs.gametype >= GT_TEAM &&
		cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[clientNum].team) {
		packed = cg_teamHeadColor.integer;
	} else {
		packed = cg_enemyHeadColor.integer;
	}
	color[0] = (packed >> 24) & 0xff;
	color[1] = (packed >> 16) & 0xff;
	color[2] = (packed >> 8) & 0xff;
	color[3] = packed & 0xff;

	size = 36;

	angles[PITCH] = 0;
	angles[YAW] = 180;
	angles[ROLL] = 0;
	CG_DrawHead(SCREEN_WIDTH - size, y, size, size, clientNum, angles, color);

	info = CG_ConfigString(CS_PLAYERS + clientNum);
	name = Info_ValueForKey(info, "n");
	y += size;
	CG_DrawText((float)(SCREEN_WIDTH - 10 - CG_DrawTextWidth(name, 0.18f, 0, 1)), y + 10, 1, 0.18f, colorWhite, name, 0, 0,
				ITEM_TEXTSTYLE_NORMAL);

	return y + BIGCHAR_HEIGHT + 2;
}

static float CG_DrawSnapshot(float y) {
	char* s;
	int w;

	s = va("time:%i snap:%i cmd:%i", cg.snap->serverTime,
		   cg.latestSnapshotNum, cgs.serverCommandSequence);
	w = CG_DrawTextWidth(s, 0.25f, 0, 0);

	CG_DrawText((float)(635 - w), (float)(int)(y + 2) + 16, 0, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	return y + BIGCHAR_HEIGHT + 4;
}

#define FPS_FRAMES 4
static float CG_DrawFPS(float y) {
	char* s;
	int w;
	int h = 0;
	static int previousTimes[FPS_FRAMES];
	static int index;
	int i, total;
	int fps;
	static int previous;
	int t, frameTime;

	t = trap_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if (index > FPS_FRAMES) {
		total = 0;
		for (i = 0; i < FPS_FRAMES; i++) {
			total += previousTimes[i];
		}
		if (!total) {
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;
		if (fps - cg_comMaxfps.integer <= 1 && cg_comMaxfps.integer - fps <= 1) {
			fps = cg_comMaxfps.integer;
		}

		s = va("%ifps", fps);
		w = CG_DrawTextWidth(s, 0.25f, 0, 2);
		h = CG_DrawTextHeight(s, 0.25f, 0, 2);

		CG_DrawText((float)(635 - w), y + h, 2, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	return y + h;
}

static float CG_DrawTeamOverlay(float y, qboolean right, qboolean upper) {
	float size, h, textY;
	int unit, lineHeight, skipClient;
	int count, plyrs, nameWidth, locWidth;
	int x, w, xx, ret;
	int i, j, len;
	int clientNum, health, armor, location;
	const char* p;
	vec4_t hcolor;
	clientInfo_t* ci;
	centity_t* cent;
	gitem_t* item;
	char name[40];
	char cut[20];
	char healthStr[16], armorStr[16];

	size = cg_drawTeamOverlaySize.value;
	unit = CG_DrawTextHeight("O", size, 0, 0);
	skipClient = cg_selfOnTeamOverlay.integer ? -1 : cg.clientNum;

	if (!cg_drawTeamOverlay.integer) {
		return y;
	}
	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_RED && cg.snap->ps.persistant[PERS_TEAM] != TEAM_BLUE) {
		return y;
	}
	if (cg.snap->ps.eFlags & EF_SPECTATOR) {
		return y;
	}

	count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
	plyrs = 0;
	nameWidth = 0;
	for (i = 0; i < count; i++) {
		clientNum = sortedTeamPlayers[i];
		ci = cgs.clientinfo + clientNum;
		if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM] && clientNum != skipClient) {
			len = CG_DrawTextWidth(ci->name, size, 20, 1);
			plyrs++;
			if (len > nameWidth) {
				nameWidth = len;
			}
		}
	}
	if (!plyrs) {
		return y;
	}

	locWidth = 0;
	for (i = 1; i < MAX_LOCATIONS; i++) {
		p = CG_ConfigString(CS_LOCATIONS + i);
		if (p && *p) {
			len = CG_DrawTextWidth(p, size, 0, 1);
			if (len > locWidth) {
				locWidth = len;
			}
		}
	}
	if (locWidth > 64) {
		locWidth = 64;
	}

	w = unit * 13 + locWidth + nameWidth;
	if (right) {
		x = (int)((float)(640 - w) + cg_drawTeamOverlayX.value);
	} else {
		x = (int)cg_drawTeamOverlayX.value;
	}

	lineHeight = unit + 3;
	h = (float)(lineHeight * plyrs);
	if (upper) {
		ret = (int)(y + h);
		y -= cg_drawTeamOverlayY.value;
	} else {
		y -= h + cg_drawTeamOverlayY.value;
		ret = (int)y;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
		hcolor[0] = 0.75f;
		hcolor[1] = 0.5f;
		hcolor[2] = 0.5f;
	} else {
		hcolor[0] = 0.5f;
		hcolor[1] = 0.5f;
		hcolor[2] = 0.75f;
	}
	hcolor[3] = cg_drawTeamOverlayOpacity.value;
	trap_R_SetColor(hcolor);
	CG_DrawPic(x, y, w, h, cgs.media.teamStatusBar);
	trap_R_SetColor(NULL);

	for (i = 0; i < count; i++) {
		cent = &cg_entities[sortedTeamPlayers[i]];
		clientNum = cent->currentState.clientNum;
		if (clientNum == cg.snap->ps.clientNum) {
			health = cg.snap->ps.stats[STAT_HEALTH];
			armor = cg.snap->ps.stats[STAT_ARMOR];
			location = cg.snap->ps.location;
		} else {
			health = cent->currentState.health;
			armor = cent->currentState.armor;
			location = cent->currentState.location;
		}
		ci = cgs.clientinfo + clientNum;
		if (!ci->infoValid || ci->team != cg.snap->ps.persistant[PERS_TEAM] || clientNum == skipClient) {
			continue;
		}

		hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0f;
		Q_strncpyz(name, ci->name, sizeof(name));
		if (CG_DrawStrlen(name) > 20) {
			Q_strncpyz(cut, name, sizeof(cut));
			Q_strncpyz(name, va("%s%s", cut, "..."), sizeof(name));
		}
		textY = y + CG_DrawTextHeight(name, size, 0, 1);
		CG_DrawText(x + unit, textY, 1, size, hcolor, name, 0, 20, ITEM_TEXTSTYLE_SHADOWED);

		xx = x + unit * 2 + nameWidth;
		if (cent->currentState.powerups & (1 << PW_FREEZE)) {
			hcolor[0] = 0.2f;
			hcolor[1] = 0.2f;
			hcolor[2] = 0.75f;
			hcolor[3] = 1.0f;
			Com_sprintf(healthStr, sizeof(healthStr), "FROZEN");
			Com_sprintf(armorStr, sizeof(armorStr), "");
		} else if (health > 0 && !(cent->currentState.eFlags & EF_DEAD)) {
			CG_GetColorForHealth(health, armor, hcolor);
			Com_sprintf(healthStr, sizeof(healthStr), "%3i", health);
			Com_sprintf(armorStr, sizeof(armorStr), "%3i", armor);
		} else {
			hcolor[0] = 1.0f;
			hcolor[1] = 0.0f;
			hcolor[2] = 0.0f;
			hcolor[3] = 1.0f;
			Com_sprintf(healthStr, sizeof(healthStr), "DEAD");
			Com_sprintf(armorStr, sizeof(armorStr), "");
		}
		CG_DrawText(xx, textY, 2, size, hcolor, healthStr, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		CG_DrawText(xx + unit * 3, textY, 2, size, hcolor, armorStr, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

		xx += unit * 6;
		if (!(cent->currentState.powerups & (1 << PW_FREEZE))) {
			if (cg_weapons[cent->currentState.weapon].weaponIcon) {
				CG_DrawPic(xx, y, unit, unit, cg_weapons[cent->currentState.weapon].weaponIcon);
			} else {
				CG_DrawPic(xx, y, unit, unit, cgs.media.deferShader);
			}
		}

		hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0f;
		if (locWidth) {
			p = CG_ConfigString(CS_LOCATIONS + location);
			if (!p || !*p) {
				p = "unknown";
			}
			CG_DrawText(xx + unit * 2, textY, 1, size, hcolor, p, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		}

		if (right) {
			xx = x;
		} else {
			xx = x + w - unit;
		}
		for (j = 1; j <= PW_NUM_POWERUPS; j++) {
			if (!(cent->currentState.powerups & (1 << j)) || j == PW_FLIGHT || j == PW_FREEZE) {
				continue;
			}
			item = BG_FindItemForPowerup(j);
			if (item) {
				CG_DrawPic(xx, y, unit, unit, trap_R_RegisterShader(item->icon));
				if (right) {
					xx -= unit;
				} else {
					xx += unit;
				}
			}
		}

		y += lineHeight;
	}

	return ret;
}

static void CG_DrawSpeedometer(void) {
	int mode = cg_speedometer.integer;
	int i, idx;
	float rectX, rectY, rectW, rectH;
	float barScale, barWidth, halfHeight;
	float speed, barHeight, barX, barY, fullBarH, overflowH, totalH;
	int graphX, graphY;
	int baseSpeed;
	const char* text;

	if (!cg.snap) return;

	baseSpeed = cg.snap->ps.speed;
	if (baseSpeed <= 0) baseSpeed = 320;

	if (mode < 2) {
		rectX = 592.0f;
		rectY = 384.0f;
		rectW = 48.0f;
		rectH = 48.0f;
		graphX = 592;
		graphY = 384;
		cg.speedBarColor1[3] = 1.0f;
		cg.speedBarColor2[3] = 1.0f;
		CG_SetWidescreen(WIDESCREEN_RIGHT);
	} else {
		rectX = 256.0f;
		graphY = cg_crosshairY.integer / 2 + 241;
		rectY = (float)graphY;
		rectW = 128.0f;
		rectH = 32.0f;
		graphX = 256;
		cg.speedBarColor1[3] = 0.75f;
		cg.speedBarColor2[3] = 0.75f;
		CG_SetWidescreen(WIDESCREEN_CENTER);
	}

	CG_AdjustFrom640(&rectX, &rectY, &rectW, &rectH);

	halfHeight = rectH * 0.5f;
	barScale = (rectH - 5.0f) / ((float)baseSpeed * 3.0f);
	barWidth = rectW / (float)SPEED_HISTORY_SIZE;

	if (mode < 2) {
		trap_R_SetColor(NULL);
		CG_DrawPic((float)graphX, (float)graphY, 48.0f, 48.0f, cgs.media.lagometerShader);
	}

	if (mode < 3 && cg.speedHistoryCount > 0) {
		idx = cg.speedHistoryIndex + 1;
		if (idx > SPEED_HISTORY_SIZE - 1) idx = 0;

		for (i = 0; i < cg.speedHistoryCount; i++) {
			speed = cg.speedHistory[idx];
			if (speed > 0.0f) {
				barHeight = barScale * speed;
				if (barHeight > halfHeight) barHeight = halfHeight;

				trap_R_SetColor(cg.speedBarColor1);
				barX = rectX + barWidth * (float)i;
				barY = (rectY + rectH) - barHeight;
				trap_R_DrawStretchPic(barX, barY, barWidth, barHeight,
									  0, 0, 0, 0, cgs.media.whiteShader);

				fullBarH = barScale * speed;
				if (fullBarH > halfHeight) {
					overflowH = fullBarH - barHeight;
					if (overflowH > halfHeight) overflowH = halfHeight;
					totalH = fullBarH;
					if (totalH > rectH) totalH = rectH;
					trap_R_SetColor(cg.speedBarColor2);
					barY = (rectY + rectH) - totalH;
					trap_R_DrawStretchPic(barX, barY, barWidth, overflowH,
										  0, 0, 0, 0, cgs.media.whiteShader);
				}
			}

			idx++;
			if (idx > SPEED_HISTORY_SIZE - 1) idx = 0;
		}
	}

	{
		int speedInt = (int)cg.speedHistory[cg.speedHistoryIndex];
		int textX, textY;
		float tw;

		text = va("%d", speedInt);
		tw = CG_Text_Width(text, 0.175f, 0);

		if (mode < 2) {
			CG_SetWidescreen(WIDESCREEN_RIGHT);
			textX = graphX + 2;
			textY = graphY + 8;
		} else {
			textX = 320 - (int)(tw / 2);
			textY = graphY + 32 + 1;
		}

		CG_Text_Paint((float)textX, (float)textY,
					  0.15f, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	trap_R_SetColor(NULL);
	CG_SetWidescreen(WIDESCREEN_STRETCH);
}

static float CG_DrawLocalTime(float y) {
	qtime_t now;
	const char* s;
	int w, h, hour;

	trap_RealTime(&now);
	hour = now.tm_hour;
	if (ch_drawLocalTime.integer == 2) {
		const char* suffix = (hour < 12) ? "am" : "pm";

		hour %= 12;
		if (!hour) {
			hour = 12;
		}
		s = va("%i:%02i%s", hour, now.tm_min, suffix);
	} else {
		s = va("%i:%02i", hour, now.tm_min);
	}
	w = CG_DrawTextWidth(s, 0.25f, 0, 2);
	h = CG_DrawTextHeight(s, 0.25f, 0, 2);

	CG_DrawText((float)(635 - w), y + h, 2, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	return y + h;
}

static float CG_DrawRecording(float y) {
	const char* s;
	int w, h;

	if (!cg_demoRecording.integer || !ch_recordingDemo.integer) {
		return y;
	}
	s = cg_demoName.string[0] ? va("REC %s", cg_demoName.string) : "REC";
	w = CG_DrawTextWidth(s, 0.25f, 0, 2);
	h = CG_DrawTextHeight(s, 0.25f, 0, 2);

	CG_DrawText((float)(635 - w), y + h, 2, 0.25f, colorRed, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	return y + h;
}

#define KEY_FORWARD 1
#define KEY_BACK 2
#define KEY_LEFT 4
#define KEY_RIGHT 8
#define KEY_JUMP 16
#define KEY_CROUCH 32
#define KEY_ATTACK 64

static int CG_KeysOf(const playerState_t* ps) {
	usercmd_t cmd;
	vec3_t forward, right;
	float f, s;
	int keys = 0;

	if (!cg.demoPlayback && ps->clientNum == cg.clientNum && !(ps->pm_flags & PMF_FOLLOW)) {
		if (!trap_GetUserCmd(trap_GetCurrentCmdNumber(), &cmd)) {
			return 0;
		}
		if (cmd.forwardmove > 0) {
			keys |= KEY_FORWARD;
		} else if (cmd.forwardmove < 0) {
			keys |= KEY_BACK;
		}
		if (cmd.rightmove > 0) {
			keys |= KEY_RIGHT;
		} else if (cmd.rightmove < 0) {
			keys |= KEY_LEFT;
		}
		if (cmd.upmove > 0) {
			keys |= KEY_JUMP;
		} else if (cmd.upmove < 0) {
			keys |= KEY_CROUCH;
		}
		if (cmd.buttons & BUTTON_ATTACK) {
			keys |= KEY_ATTACK;
		}
		return keys;
	}

	AngleVectors(ps->viewangles, forward, right, NULL);
	forward[2] = 0;
	right[2] = 0;
	VectorNormalize(forward);
	VectorNormalize(right);
	f = DotProduct(ps->velocity, forward);
	s = DotProduct(ps->velocity, right);
	if (f > 40.0f) {
		keys |= KEY_FORWARD;
	} else if (f < -40.0f) {
		keys |= KEY_BACK;
	}
	if (s > 40.0f) {
		keys |= KEY_RIGHT;
	} else if (s < -40.0f) {
		keys |= KEY_LEFT;
	}
	if (ps->groundEntityNum == ENTITYNUM_NONE && ps->velocity[2] > 100.0f) {
		keys |= KEY_JUMP;
	}
	if (ps->pm_flags & PMF_DUCKED) {
		keys |= KEY_CROUCH;
	}
	if (ps->weaponstate == WEAPON_FIRING) {
		keys |= KEY_ATTACK;
	}

	return keys;
}

static void CG_DrawKey(float x, float y, float size, const char* label, qboolean down) {
	vec4_t box = {0.0f, 0.0f, 0.0f, 0.35f};
	vec4_t text = {1.0f, 1.0f, 1.0f, 0.45f};
	float scale = size * 0.014f;

	if (down) {
		box[0] = box[1] = box[2] = 0.85f;
		box[3] = 0.65f;
		text[0] = text[1] = text[2] = 0.0f;
		text[3] = 1.0f;
	}
	CG_FillRect(x, y, size, size, box);
	CG_DrawText(x + (size - CG_DrawTextWidth(label, scale, 0, 0)) * 0.5f,
				y + (size + CG_DrawTextHeight(label, scale, 0, 0)) * 0.5f, 0, scale, text, label, 0, 0, 0);
}

static void CG_DrawKeys(void) {
	const float size = 16.0f;
	const float gap = 2.0f;
	float x = 24.0f;
	float y = 330.0f;
	int keys;

	if (!cg.snap) {
		return;
	}
	keys = CG_KeysOf(&cg.snap->ps);

	CG_DrawKey(x + size + gap, y, size, "W", (keys & KEY_FORWARD) ? qtrue : qfalse);
	y += size + gap;
	CG_DrawKey(x, y, size, "A", (keys & KEY_LEFT) ? qtrue : qfalse);
	CG_DrawKey(x + size + gap, y, size, "S", (keys & KEY_BACK) ? qtrue : qfalse);
	CG_DrawKey(x + (size + gap) * 2.0f, y, size, "D", (keys & KEY_RIGHT) ? qtrue : qfalse);
	y += size + gap;
	CG_DrawKey(x, y, size, "C", (keys & KEY_CROUCH) ? qtrue : qfalse);
	CG_DrawKey(x + size + gap, y, size, "J", (keys & KEY_JUMP) ? qtrue : qfalse);
	CG_DrawKey(x + (size + gap) * 2.0f, y, size, "F", (keys & KEY_ATTACK) ? qtrue : qfalse);
}

static void CG_DrawUpperRight(stereoFrame_t stereoFrame) {
	float y;

	y = 0;

	if (cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1) {
		y = CG_DrawTeamOverlay(y, qtrue, qtrue);
	}
	if (cg_drawSnapshot.integer) {
		y = CG_DrawSnapshot(y);
	}
	if (cg_drawFPS.integer && (stereoFrame == STEREO_CENTER || stereoFrame == STEREO_RIGHT)) {
		y = CG_DrawFPS(y);
	}
	if (ch_drawKeys.integer) {
		CG_DrawKeys();
	}
	if (ch_recordMessage.integer) {
		y = CG_DrawRecording(y);
	}
	if (ch_drawLocalTime.integer) {
		y = CG_DrawLocalTime(y);
	}
	if (cg_drawAttacker.integer) {
		CG_DrawAttacker(y);
	}
	if (cg_speedometer.integer > 0 && cg_speedometer.integer < 4) {
		CG_DrawSpeedometer();
	}
}

#define PICKUP_ITEM_TIME 3000
#define PICKUP_ITEM_FADEIN 150
#define PICKUP_ITEM_FADEOUT 200
#define PICKUP_ITEM_SCALE 0.28f

static int CG_DrawPickupItem(int y) {
	float* fadeColor;
	vec4_t color;
	const char* s;
	int value, x, now, msec, mins, secs;

	if (!cg_drawItemPickups.integer || cg.scoreBoardShowing || cg.snap->ps.stats[STAT_HEALTH] <= 0) {
		return y;
	}
	y -= 48;
	value = cg.itemPickup;
	if (!value) {
		return y;
	}
	fadeColor = CG_FadeColor(cg.itemPickupTime, PICKUP_ITEM_TIME);
	if (!fadeColor) {
		cg.itemPickupLast = 0;
		cg.itemPickupCount = 1;
		return y;
	}
	Vector4Copy(fadeColor, color);
	if (ch_animateScroll.integer) {
		color[3] = CG_CpmaFade(cg.itemPickupTime, PICKUP_ITEM_FADEIN,
		                       PICKUP_ITEM_TIME - PICKUP_ITEM_FADEIN - PICKUP_ITEM_FADEOUT, PICKUP_ITEM_FADEOUT);
	}

	CG_RegisterItemVisuals(value);
	trap_R_SetColor(color);

	x = 5;
	if (cg_drawItemPickups.integer & 4) {
		if (cg.warmup == 0) {
			now = cgs.freezeEnd ? cgs.freezeEnd : cg.time;
			if (!cgs.timelimit) {
				msec = cg.itemPickupTime - cgs.levelStartTime;
			} else if (cgs.timelimit * 60000 - now + cgs.levelStartTime < 0) {
				msec = cg.itemPickupTime - cgs.timelimit * 60000 - cgs.levelStartTime;
			} else if (cg_levelTimerDirection.integer == 1) {
				msec = cgs.levelStartTime + cgs.timelimit * 60000 - cg.itemPickupTime;
			} else {
				msec = cg.itemPickupTime - cgs.levelStartTime;
			}
		} else {
			msec = 0;
		}
		secs = msec / 1000;
		mins = secs / 60;
		secs -= mins * 60;
		s = va("%i:%i%i", mins, secs / 10, secs % 10);
		CG_DrawText(5.0f, (float)(y + CG_DrawTextHeight(s, PICKUP_ITEM_SCALE, 0, 1)), 1, PICKUP_ITEM_SCALE,
		            color, s, 0, 0, 0);
		x = CG_DrawTextWidth(s, PICKUP_ITEM_SCALE, 0, 1) + 13;
	}
	if (cg_drawItemPickups.integer & 1) {
		CG_DrawPic((float)x, (float)y, 16, 16, cg_items[value].icon);
		x += 24;
	}
	if (cg_drawItemPickups.integer & 2) {
		s = bg_itemlist[value].pickup_name;
		CG_DrawText((float)x, (float)(y + CG_DrawTextHeight(s, PICKUP_ITEM_SCALE, 0, 1)), 1, PICKUP_ITEM_SCALE,
		            color, s, 0, 0, 0);
		x += CG_DrawTextWidth(s, PICKUP_ITEM_SCALE, 0, 1) + 8;
	}
	if (cg.itemPickupCount > 1 && bg_itemlist[value].giType != IT_POWERUP && bg_itemlist[value].giType != IT_TEAM) {
		s = va("x%i", cg.itemPickupCount);
		CG_DrawText((float)x, (float)(y + CG_DrawTextHeight(s, PICKUP_ITEM_SCALE, 0, 1)), 1, PICKUP_ITEM_SCALE,
		            color, s, 0, 0, 0);
	}

	cg.itemPickupLast = cg.itemPickup;
	trap_R_SetColor(NULL);
	return y;
}

static void CG_DrawLowerLeft(void) {
	float y = 408;

	if (cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 3) {
		y = CG_DrawTeamOverlay(y, qfalse, qfalse);
	}
	CG_DrawPickupItem((int)y);
}

static void CG_DrawRewardIcon(float x, float y, float size, qhandle_t shader) {
	const qhandle_t animated[6] = {cgs.media.medalAssist, cgs.media.medalCapture, cgs.media.medalDefend,
								   cgs.media.medalExcellent, cgs.media.medalGauntlet, cgs.media.medalImpressive};
	int mode = ch_animateRewardIcons.integer;
	int medal, frame;

	for (medal = 0; mode >= 1 && mode <= 2 && medal < 6; medal++) {
		if (shader != animated[medal] || !cgs.media.medalFrames[medal][0]) {
			continue;
		}
		frame = (cg.time - cg.rewardTime) / 40;
		if (frame < 0) {
			frame = 0;
		}
		if (mode == 1) {
			frame = (frame > 23) ? 23 : frame;
		} else {
			frame %= 24;
		}
		shader = cgs.media.medalFrames[medal][frame];
		break;
	}
	CG_DrawPic(x, y, size, size, shader);
}

#define REWARD_ICON_SCALE 0.74f
#define REWARD_ICON_SIZE (32.0f * REWARD_ICON_SCALE)
#define REWARD_ICON_STEP (48.0f * REWARD_ICON_SCALE)
#define REWARD_ROW_CENTER 325.0f
#define REWARD_ICON_X (REWARD_ROW_CENTER - REWARD_ICON_STEP / 2.0f)
#define REWARD_COUNT_X (REWARD_ICON_X + 40.0f * REWARD_ICON_SCALE)
#define REWARD_COUNT_DY (24.0f * REWARD_ICON_SCALE)
#define REWARD_COUNT_SCALE (0.2f * REWARD_ICON_SCALE)

static void CG_DrawReward(void) {
	vec4_t color;
	int i, remaining;

	remaining = REWARD_TIME - (cg.time - cg.rewardTime);
	if (!cg.rewardTime || remaining <= 0) {
		if (cg.rewardStack <= 0) {
			return;
		}
		for (i = 0; i < cg.rewardStack; i++) {
			cg.rewardSound[i] = cg.rewardSound[i + 1];
			cg.rewardShader[i] = cg.rewardShader[i + 1];
			cg.rewardCount[i] = cg.rewardCount[i + 1];
		}
		cg.rewardStack--;
		cg.rewardTime = cg.time;
		remaining = REWARD_TIME;
		if (cg_announcerRewardsVO.integer) {
			CG_AddBufferedSound(cg.rewardSound[0]);
		}
	}

	if (!cg_drawRewards.integer) {
		return;
	}

	color[0] = color[1] = color[2] = 1.0f;
	if (ch_animateRewardIcons.integer) {
		color[3] = CG_CpmaFade(cg.rewardTime, 0, REWARD_TIME - 200, 200);
	} else {
		color[3] = (remaining < 200) ? remaining / 200.0f : 1.0f;
	}
	trap_R_SetColor(color);
	CG_DrawRewardIcon(REWARD_ICON_X, ch_rewardY.value, REWARD_ICON_SIZE, cg.rewardShader[0]);
	if (cg.rewardCount[0] > cg_drawRewardsRowSize.integer) {
		CG_DrawText(REWARD_COUNT_X, ch_rewardY.value + REWARD_COUNT_DY, 0, REWARD_COUNT_SCALE, color,
					va("%d", cg.rewardCount[0]), 0, 0, 0);
	}
	trap_R_SetColor(NULL);
}

#define LAG_SAMPLES 128

typedef struct {
	int frameSamples[LAG_SAMPLES];
	int frameCount;
	int snapshotFlags[LAG_SAMPLES];
	int snapshotSamples[LAG_SAMPLES];
	int snapshotCount;
} lagometer_t;

lagometer_t lagometer;

void CG_AddLagometerFrameInfo(void) {
	int offset;

	offset = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = offset;
	lagometer.frameCount++;
}

void CG_AddLagometerSnapshotInfo(snapshot_t* snap) {
	if (!snap) {
		lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = -1;
		lagometer.snapshotCount++;
		return;
	}

	lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->ping;
	lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->snapFlags;
	lagometer.snapshotCount++;
}

static void CG_DrawDisconnect(void) {
	float x, y;
	int cmdNum;
	usercmd_t cmd;
	const char* s;
	int w;

	if (cg.demoPlayback) {
		return;
	}

	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd(cmdNum, &cmd);
	if (cmd.serverTime <= cg.snap->ps.commandTime || cmd.serverTime > cg.time) {
		return;
	}

	if (cg.time - cgs.levelStartTime < 3000) {
		return;
	}

	s = "Connection Interrupted";
	w = CG_DrawTextWidth(s, 0.25f, 0, 0);
	CG_DrawText((float)(320 - w / 2), 116, 0, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	if ((cg.time >> 9) & 1) {
		return;
	}

	x = SCREEN_WIDTH - 48;
	y = SCREEN_HEIGHT - 48;

	CG_DrawPic(x, y, 48, 48, trap_R_RegisterShader("gfx/2d/net.jpg"));
}

static void CG_DrawInputCmds(void) {
	float size, x, y;
	signed char forward, right, up;
	usercmd_t cmd;

	if (!cg_drawInputCmds.integer) {
		return;
	}
	if (cg.predictedPlayerState.pm_type == PM_SPECTATOR && !(cg.predictedPlayerState.pm_flags & PMF_FOLLOW)) {
		return;
	}
	if (cg_drawInputCmds.integer == 2 && !(cg.predictedPlayerState.pm_flags & PMF_FOLLOW)) {
		return;
	}

	if (!(cg.snap->ps.pm_flags & PMF_FOLLOW) && !cg.demoPlayback) {
		trap_GetUserCmd(trap_GetCurrentCmdNumber(), &cmd);
		forward = cmd.forwardmove;
		right = cmd.rightmove;
		up = cmd.upmove;
	} else {
		forward = cg.snap->ps.forwardmove;
		right = cg.snap->ps.rightmove;
		up = cg.snap->ps.upmove;
	}

	size = cg_drawInputCmdsSize.value;
	x = cg_drawInputCmdsX.integer;
	y = cg_drawInputCmdsY.integer;

	if (forward > 0) {
		CG_DrawPic(x - 8, y - size - 16, 16, 16, cgs.media.cmdUpShader);
	}
	if (forward < 0) {
		CG_DrawPic(x - 8, y + size, 16, 16, cgs.media.cmdDownShader);
	}
	if (right > 0) {
		CG_DrawPic(x + size, y - 8, 16, 16, cgs.media.cmdRightShader);
	}
	if (right < 0) {
		CG_DrawPic(x - size - 16, y - 8, 16, 16, cgs.media.cmdLeftShader);
	}
	if (up > 0) {
		CG_DrawPic(x + size + 16, y - size - 16, 16, 16, cgs.media.cmdUpShader);
	}
	if (up < 0) {
		CG_DrawPic(x + size + 16, y + size, 16, 16, cgs.media.cmdDownShader);
	}
}

#define MAX_LAGOMETER_PING 900
#define MAX_LAGOMETER_RANGE 300

static void CG_DrawLagometer(void) {
	int a, x, y, i;
	float v;
	float ax, ay, aw, ah, mid, range;
	int color;
	float vscale;

	if (!cg_lagometer.integer || cg.demoPlayback) {
		CG_DrawDisconnect();
		return;
	}

	x = SCREEN_WIDTH - 48;
	y = SCREEN_HEIGHT - 144;

	trap_R_SetColor(NULL);
	CG_DrawPic(x, y, 48, 48, cgs.media.lagometerShader);

	ax = x;
	ay = y;
	aw = 48;
	ah = 48;
	CG_AdjustFrom640(&ax, &ay, &aw, &ah);

	color = -1;
	range = ah / 3;
	mid = ay + range;

	vscale = range / MAX_LAGOMETER_RANGE;

	for (a = 0; a < aw; a++) {
		i = (lagometer.frameCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.frameSamples[i];
		v *= vscale;
		if (v > 0) {
			if (color != 1) {
				color = 1;
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
			}
			if (v > range) {
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		} else if (v < 0) {
			if (color != 2) {
				color = 2;
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_BLUE)]);
			}
			v = -v;
			if (v > range) {
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	range = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for (a = 0; a < aw; a++) {
		i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if (v > 0) {
			if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED) {
				if (color != 5) {
					color = 5;
					trap_R_SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
				}
			} else {
				if (color != 3) {
					color = 3;
					trap_R_SetColor(g_color_table[ColorIndex(COLOR_GREEN)]);
				}
			}
			v = v * vscale;
			if (v > range) {
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		} else if (v < 0) {
			if (color != 4) {
				color = 4;
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_RED)]);
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	trap_R_SetColor(NULL);

	if (cg_nopredict.integer) {
		CG_DrawText(x, y + 16, 0, 0.25f, colorWhite, "snc", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	if (cg_lagometer.integer == 2 && cg.snap) {
		static int lastPing;
		char text[16];

		if (cg.snap->ping) {
			lastPing = cg.snap->ping;
		}
		Com_sprintf(text, sizeof(text), "%d", lastPing);
		CG_DrawText(592, CG_DrawTextHeight(text, 0.175f, 0, 0) + 337, 0, 0.15f, colorWhite, text, 0, 0,
					ITEM_TEXTSTYLE_SHADOWED);
	}

	CG_DrawDisconnect();
}

void CG_CenterPrint(const char* str, int y, float scale) {
	char* s;

	Q_strncpyz(cg.centerPrint, str, sizeof(cg.centerPrint));

	cg.centerPrintTime = cg.time;
	cg.centerPrintFrag = qfalse;
	cg.centerPrintY = y + (cgs.gametype == GT_RACE ? 10 : 0);
	cg.centerPrintScale = scale;

	cg.centerPrintLines = 1;
	s = cg.centerPrint;
	while (*s) {
		if (*s == '\n')
			cg.centerPrintLines++;
		s++;
	}
}

static void CG_DrawCenterString(void) {
	static vec4_t color;
	char* start;
	int l, font;
	float x, y, w, h, scale;
	int duration, remaining;

	if (!cg.centerPrintTime) {
		return;
	}

	color[0] = color[1] = color[2] = 1.0f;
	if (cg.centerPrintFrag && ch_animateScroll.integer) {
		color[3] = CG_CpmaFade(cg.centerPrintTime, 150, 1200, 200);
		if (color[3] <= 0.0f) {
			return;
		}
	} else {
		duration = (int)(cg_centertime.value * 1000.0);
		remaining = duration - (cg.time - cg.centerPrintTime);
		if (cg.time - cg.centerPrintTime >= duration) {
			return;
		}
		color[3] = (remaining < 200) ? remaining / 200.0f : 1.0f;
	}
	trap_R_SetColor(color);

	if (cg.centerPrintFrag) {
		y = ch_fragMessageY.value;
		scale = ch_fragMessageScale.value;
		font = cgs.media.fragMessageFont;
	} else {
		y = (float)cg.centerPrintY;
		scale = cg.centerPrintScale;
		font = 0;
	}

	start = cg.centerPrint;

	y = (float)(int)(y - scale * 16.0 * cg.centerPrintLines);

	while (1) {
		char linebuffer[1024];

		for (l = 0; l < 50; l++) {
			if (!start[l] || start[l] == '\n') {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		CG_MeasureTextInFont(linebuffer, font, scale, &w, &h);
		x = (float)((SCREEN_WIDTH - (int)w) / 2);
		CG_DrawTextInFont(x, y + h, font, scale, color, linebuffer, ITEM_TEXTSTYLE_SHADOWEDMORE);
		y += h + 6;

		while (*start && (*start != '\n')) {
			start++;
		}
		if (!*start) {
			break;
		}
		start++;
	}

	trap_R_SetColor(NULL);
}

static const vec4_t cg_crosshairPalette[26] = {
	{1.0f, 0.0f, 0.0f, 1.0f},       {1.0f, 0.2734375f, 0.0f, 1.0f}, {1.0f, 0.5f, 0.0f, 1.0f},
	{1.0f, 0.734375f, 0.0f, 1.0f},  {1.0f, 1.0f, 0.0f, 1.0f},       {0.734375f, 1.0f, 0.0f, 1.0f},
	{0.5f, 1.0f, 0.0f, 1.0f},       {0.2734375f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
	{0.0f, 1.0f, 0.2734375f, 1.0f}, {0.0f, 1.0f, 0.5f, 1.0f},       {0.0f, 1.0f, 0.734375f, 1.0f},
	{0.0f, 1.0f, 1.0f, 1.0f},       {0.0f, 0.734375f, 1.0f, 1.0f},  {0.0f, 0.5f, 1.0f, 1.0f},
	{0.0f, 0.2734375f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},       {0.2734375f, 0.0f, 1.0f, 1.0f},
	{0.5f, 0.0f, 1.0f, 1.0f},       {0.734375f, 0.0f, 1.0f, 1.0f},  {1.0f, 0.0f, 1.0f, 1.0f},
	{1.0f, 0.0f, 0.734375f, 1.0f},  {1.0f, 0.0f, 0.5f, 1.0f},       {1.0f, 0.0f, 0.2734375f, 1.0f},
	{1.0f, 1.0f, 1.0f, 1.0f},       {0.5f, 0.5f, 0.5f, 1.0f},
};

static const float* CG_CrosshairPaletteColor(int value) {
	if (value < 1 || value > 26) {
		return cg_crosshairPalette[0];
	}
	return cg_crosshairPalette[value - 1];
}

static void CG_CrosshairHitEffect(float* w, float* h) {
	static const vec4_t typeColors[4] = {
		{0.25f, 0.5f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
	};
	int style = cg_crosshairHitStyle.integer;
	int type = cg.crosshairHitType;
	float elapsed = (float)(cg.time - cg.crosshairHitTime);
	float t, grow = 0.0f;
	int colorBy = 0;

	if (style < 1 || style > 8 || elapsed <= 0.0f || elapsed >= cg_crosshairHitTime.value) {
		return;
	}
	t = elapsed / cg_crosshairHitTime.value;

	switch (style) {
	case 1: colorBy = 1; break;
	case 2: colorBy = 2; break;
	case 3: grow = (float)type; break;
	case 4: grow = (float)type; colorBy = 1; break;
	case 5: grow = (float)type; colorBy = 2; break;
	case 6: grow = 1.0f; break;
	case 7: grow = 1.0f; colorBy = 1; break;
	case 8: grow = 1.0f; colorBy = 2; break;
	}

	if (grow != 0.0f) {
		*w *= 1.0f + grow * t;
		*h *= 1.0f + grow * t;
	}
	if (colorBy == 1) {
		trap_R_SetColor(typeColors[(type >= 2 && type <= 4) ? type - 1 : 0]);
	} else if (colorBy == 2) {
		trap_R_SetColor(CG_CrosshairPaletteColor(cg_crosshairHitColor.integer));
	}
}

static void CG_DrawCrosshair(void) {
	vec4_t color;
	float w, h, x, y, scale, f;
	int n = cg_drawCrosshair.integer;

	if (n <= 0 || n % NUM_CROSSHAIRS == 0 || cg.renderingThirdPerson) {
		return;
	}

	if (cg_crosshairHealth.integer) {
		CG_ColorForHealth(color);
	} else {
		Vector4Copy(CG_CrosshairPaletteColor(cg_crosshairColor.integer), color);
	}
	VectorScale(color, cg_crosshairBrightness.value, color);
	color[3] = 1.0f;
	trap_R_SetColor(color);

	w = h = cg_crosshairSize.value;

	if (cg.itemPickupBlendTime > cg.crosshairHitTime) {
		f = (float)(cg.time - cg.itemPickupBlendTime);
		if (cg_crosshairPulse.integer && f > 0.0f && f < ITEM_BLOB_TIME) {
			w = h = cg_crosshairSize.value * (1.0f + f / ITEM_BLOB_TIME);
		}
	} else {
		CG_CrosshairHitEffect(&w, &h);
	}

	scale = cgs.glconfig.vidWidth / 640.0f;
	x = cg_crosshairX.integer * scale;
	y = cg_crosshairY.integer * scale;
	w *= scale;
	h *= scale;

	trap_R_DrawStretchPic(cg.refdef.x + x + 0.5f * (cg.refdef.width - w),
						  cg.refdef.y + y + 0.5f * (cg.refdef.height - h),
						  w, h, 0, 0, 1, 1, cgs.media.crosshairShader[n % NUM_CROSSHAIRS]);

	trap_R_SetColor(NULL);
}

static void CG_DrawCrosshair3D(void) {
	float w;
	qhandle_t hShader;
	float f;
	int ca;

	trace_t trace;
	vec3_t endpos;
	float stereoSep, zProj, maxdist, xmax;
	char rendererinfos[128];
	refEntity_t ent;

	if (!cg_drawCrosshair.integer) {
		return;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if (cg.renderingThirdPerson) {
		return;
	}

	w = cg_crosshairSize.value;

	f = cg.time - cg.itemPickupBlendTime;
	if (f > 0 && f < ITEM_BLOB_TIME) {
		f /= ITEM_BLOB_TIME;
		w *= (1 + f);
	}

	ca = cg_drawCrosshair.integer;
	if (ca < 0) {
		ca = 0;
	}
	hShader = cgs.media.crosshairShader[ca % NUM_CROSSHAIRS];

	trap_Cvar_VariableStringBuffer("r_zProj", rendererinfos, sizeof(rendererinfos));
	zProj = atof(rendererinfos);
	trap_Cvar_VariableStringBuffer("r_stereoSeparation", rendererinfos, sizeof(rendererinfos));
	stereoSep = zProj / atof(rendererinfos);

	xmax = zProj * tan(cg.refdef.fov_x * M_PI / 360.0f);

	maxdist = cgs.glconfig.vidWidth * stereoSep * zProj / (2 * xmax);
	VectorMA(cg.refdef.vieworg, maxdist, cg.refdef.viewaxis[0], endpos);
	CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, endpos, 0, MASK_SHOT);

	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_DEPTHHACK | RF_CROSSHAIR;

	VectorCopy(trace.endpos, ent.origin);

	ent.radius = w / 640 * xmax * trace.fraction * maxdist / zProj;
	ent.customShader = hShader;

	trap_R_AddRefEntityToScene(&ent);
}

static void CG_ScanForCrosshairEntity(void) {
	trace_t trace;
	vec3_t start, end;

	VectorCopy(cg.refdef.vieworg, start);
	VectorMA(start, 131072, cg.refdef.viewaxis[0], end);

	CG_Trace(&trace, start, vec3_origin, vec3_origin, end,
			 cg.snap->ps.clientNum, CONTENTS_SOLID | CONTENTS_BODY);
	if (trace.entityNum >= MAX_CLIENTS) {
		return;
	}

	if ((cg_entities[trace.entityNum].currentState.powerups & (1 << PW_INVIS)) &&
		!(cgs.gametype >= GT_TEAM &&
		  cg.snap->ps.persistant[PERS_TEAM] == cgs.clientinfo[trace.entityNum].team)) {
		return;
	}

	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}

static void CG_CrosshairTeamVitalColor(int value, float* color) {
	int maxHealth = cg.snap->ps.stats[STAT_MAX_HEALTH];
	float green, blue;

	if (value >= maxHealth) {
		green = 1.0f;
		blue = value / (maxHealth * 2.0f);
		if (blue > 1.0f) {
			blue = 1.0f;
		}
	} else {
		green = (float)value / maxHealth;
		if (green < 0.0f) {
			green = 0.0f;
		}
		blue = 0.0f;
	}
	if (cg_drawCrosshairTeamHealth.value == 2.0f) {
		color[0] = 1.0f;
		color[1] = green;
		color[2] = blue;
	}
}

static void CG_DrawCrosshairNames(void) {
	float* color;
	char* name;
	float opacity;
	qboolean teammate;
	qboolean watching;
	centity_t* cent;
	float size, y;
	char text[16];

	if (!cg_drawCrosshair.integer) {
		return;
	}
	if (cg.renderingThirdPerson) {
		return;
	}

	CG_ScanForCrosshairEntity();

	color = CG_FadeColor(cg.crosshairClientTime, 1000);
	if (!color) {
		trap_R_SetColor(NULL);
		return;
	}

	teammate = cgs.gametype >= GT_TEAM &&
			   cg.snap->ps.persistant[PERS_TEAM] == cgs.clientinfo[cg.crosshairClientNum].team;
	watching = ch_drawTargetStatus.integer &&
			   (cg.demoPlayback || cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR);
	if (teammate) {
		if (cg_teammateNames.integer || !cg_teammateCrosshairNames.integer) {
			return;
		}
		opacity = cg_teammateCrosshairNamesOpacity.value;
	} else {
		if (!cg_enemyCrosshairNames.integer) {
			return;
		}
		opacity = cg_enemyCrosshairNamesOpacity.value;
	}

	name = cgs.clientinfo[cg.crosshairClientNum].name;
	color[3] *= opacity;
	CG_DrawText(320 - CG_DrawTextWidth(name, 0.26f, 0, 0) * 0.5f, 190, 0, 0.26f, color, name, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	if (cg_drawCrosshairTeamHealth.value != 0.0f && (teammate || watching) &&
		cg_entities[cg.crosshairClientNum].currentState.health > 0) {
		cent = &cg_entities[cg.crosshairClientNum];
		size = cg_drawCrosshairTeamHealthSize.value;
		y = size * 16.0f + 198.0f;

		CG_CrosshairTeamVitalColor(cent->currentState.health, color);
		Com_sprintf(text, sizeof(text), "%i ", cent->currentState.health);
		CG_DrawText(320 - CG_DrawTextWidth(text, size, 0, 0), y, 0, size, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		color[0] = color[1] = color[2] = 1.0f;
		CG_DrawText(320, y, 0, size, color, "/", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		CG_CrosshairTeamVitalColor(cent->currentState.armor, color);
		Com_sprintf(text, sizeof(text), " %i", cent->currentState.armor);
		CG_DrawText(325, y, 0, size, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}
	trap_R_SetColor(NULL);
}

static const float* CG_OverheadTeamColor(int team) {
	static const vec4_t freeColor = {1.0f, 1.0f, 1.0f, 1.0f};
	static const vec4_t redColor = {1.0f, 0.5f, 0.5f, 1.0f};
	static const vec4_t blueColor = {0.5f, 0.75f, 1.0f, 1.0f};
	static const vec4_t spectatorColor = {0.85f, 0.85f, 0.85f, 1.0f};
	static const vec4_t otherColor = {1.0f, 0.8f, 0.2f, 1.0f};

	switch (team) {
	case TEAM_FREE:
		return freeColor;
	case TEAM_RED:
		return redColor;
	case TEAM_BLUE:
		return blueColor;
	case TEAM_SPECTATOR:
		return spectatorColor;
	default:
		return otherColor;
	}
}

static int CG_OverheadTextWidth(const char* text, int fontIndex, float scale, int limit) {
	float extents[5];
	float xscale;

	trap_R_Font_TextExtents(text, 0, fontIndex, CG_FontPixelSize(scale), limit > 0 ? limit : -1, extents);
	if (cgs.widescreenBias > 0.0f) {
		xscale = cgs.glconfig.vidHeight / 3.0f * 4.0f / 640.0f;
	} else {
		xscale = cgs.screenXScale;
	}
	return (int)((extents[2] - extents[0]) / xscale);
}

static void CG_DrawTeammateNames(void) {
	static const vec4_t boxColor = {0.0f, 0.0f, 0.0f, 0.5f};
	static const vec4_t armorColor = {0.0f, 0.625f, 1.0f, 1.0f};
	int i, j, len, count, boxWidth, nameWidth, nameHeight, xoff, armorWidth;
	int health, armor, maxHealth, localTeam;
	qboolean sameTeam, spectator, frozen, showName, showBars, xrayNames;
	float scale, fontSize, sx, sy, nx, ny, x, y, extra, barX, barY, iconX, iconY;
	const float* textColor;
	vec3_t point, delta;
	vec4_t tint;
	qhandle_t shader;
	centity_t* cent;
	clientInfo_t* ci;
	gitem_t* item;
	char name[40], cut[40];
	char* in;
	char* out;

	xrayNames = cg.demoPlayback && CG_XrayAllowed();

	if (cg.predictedPlayerState.pm_type == PM_SPECTATOR && !(cg.predictedPlayerState.pm_flags & PMF_FOLLOW) &&
		!cg_specNames.integer && !xrayNames) {
		return;
	}

	scale = cgs.screenYScale;
	fontSize = CG_FontPixelSize(0.12f);
	localTeam = cg.snap->ps.persistant[PERS_TEAM];

	for (i = 0; i < cgs.maxclients; i++) {
		cent = &cg_entities[i];
		if (!cent->currentValid || cent->currentState.clientNum == cg.snap->ps.clientNum ||
			cent->currentState.eType != ET_PLAYER || (cent->currentState.eFlags & EF_DEAD)) {
			continue;
		}

		VectorCopy(cent->lerpOrigin, point);
		point[2] += 40;
		VectorSubtract(point, cg.refdef.vieworg, delta);
		if (DotProduct(delta, cg.refdef.viewaxis[0]) <= 0) {
			continue;
		}
		ci = &cgs.clientinfo[cent->currentState.clientNum];
		if (!ci->infoValid) {
			continue;
		}

		Q_strncpyz(name, ci->name, sizeof(name));
		for (in = out = name; *in; in++) {
			if (Q_IsColorString(in)) {
				in++;
			} else if ((unsigned char)*in >= ' ') {
				*out++ = *in;
			}
		}
		*out = 0;
		len = strlen(name);
		if (CG_OverheadTextWidth(name, 0, 0.12f, len) > cg_overheadNamesWidth.integer) {
			do {
				len--;
			} while (len > 1 && CG_OverheadTextWidth(name, 0, 0.12f, len) > cg_overheadNamesWidth.integer);
			Q_strncpyz(cut, name, len);
			Q_strncpyz(name, va("%s%s", cut, "..."), sizeof(name));
			len += 3;
		}
		nameWidth = CG_OverheadTextWidth(name, 1, 0.12f, 0);
		nameHeight = CG_DrawTextHeight(name, 0.12f, 0, 1);
		boxWidth = nameWidth > 11 ? nameWidth : 11;

		frozen = qfalse;
		count = 0;
		for (j = 1; j <= PW_NUM_POWERUPS; j++) {
			if ((cent->currentState.powerups & (1 << j)) && j != PW_FLIGHT) {
				if (j == PW_FREEZE) {
					frozen = qtrue;
				} else {
					count++;
				}
			}
		}

		sameTeam = cgs.gametype >= GT_TEAM && localTeam == ci->team;
		spectator = localTeam == TEAM_SPECTATOR;
		textColor = CG_OverheadTeamColor(localTeam);
		if ((spectator || xrayNames) && cgs.gametype >= GT_TEAM) {
			textColor = CG_OverheadTeamColor(ci->team);
		}

		CG_ProjectFloatingEffect(point, &sx, &sy);
		nx = sx / 640.0f;
		ny = 1.0f - sy / 480.0f;

		if (xrayNames || (spectator && cg_specNames.integer == 2)) {
			showName = qtrue;
		} else if (sameTeam && cg_teammateNames.integer == 2) {
			showName = qtrue;
		} else if ((spectator && cg_specNames.integer) || (sameTeam && cg_teammateNames.integer)) {
			showName = nx > 0.4f && nx < 0.6f && ny > 0.4f && ny < 0.7f;
		} else {
			showName = qfalse;
		}

		if (frozen) {
			showBars = qfalse;
		} else {
			showBars = (xrayNames && cent->currentState.health > 0) ||
					   (sameTeam && cg_teammateNames.integer) ||
					   (spectator && cg_specNames.integer);
		}

		if (showName) {
			xoff = boxWidth > nameWidth ? (boxWidth - nameWidth) / 2 : 0;
			x = nx * cgs.glconfig.vidWidth - boxWidth * 0.5f * scale;
			y = cgs.glconfig.vidHeight - ny * cgs.glconfig.vidHeight - nameHeight * scale - 8 * scale;
			extra = showBars ? 6 * scale : 0;

			trap_R_SetColor(boxColor);
			trap_R_DrawStretchPic(x - scale, y - scale - nameHeight * scale, boxWidth * scale + 2 * scale,
								  nameHeight * scale + 2 * scale + extra, 0, 0, 0, 0, cgs.media.whiteShader);
			trap_R_SetColor(colorBlack);
			trap_R_Font_DrawString((int)(x + scale + xoff * scale), (int)(y + scale), name, 1, fontSize, len, NULL, 0);
			trap_R_SetColor(textColor);
			trap_R_Font_DrawString((int)(x + xoff * scale), (int)y, name, 1, fontSize, len, NULL, 0);
			trap_R_SetColor(NULL);
		}

		if (!showBars) {
			continue;
		}

		health = cent->currentState.health;
		armor = cent->currentState.armor;
		maxHealth = cg.snap->ps.stats[STAT_MAX_HEALTH];
		barX = nx * cgs.glconfig.vidWidth - 5.5f * scale;
		barY = cgs.glconfig.vidHeight - ny * cgs.glconfig.vidHeight - nameHeight * scale - 6 * scale;
		CG_HealthGradientColor(i, health, tint);
		tint[3] = 1.0f;

		trap_R_SetColor(tint);
		trap_R_DrawStretchPic(barX, barY, 2 * scale, 2 * scale, 0, 0, 1, 1, cgs.media.healthSegmentShader);
		trap_R_SetColor(NULL);
		if (health > maxHealth * 0.25) {
			trap_R_SetColor(tint);
			trap_R_DrawStretchPic(barX + 3 * scale, barY, 2 * scale, 2 * scale, 0, 0, 1, 1, cgs.media.healthSegmentShader);
			trap_R_SetColor(NULL);
		}
		if (health > maxHealth * 0.5) {
			trap_R_SetColor(tint);
			trap_R_DrawStretchPic(barX + 6 * scale, barY, 2 * scale, 2 * scale, 0, 0, 1, 1, cgs.media.healthSegmentShader);
			trap_R_SetColor(NULL);
		}
		if (health > maxHealth * 0.75) {
			trap_R_SetColor(tint);
			trap_R_DrawStretchPic(barX + 9 * scale, barY, 2 * scale, 2 * scale, 0, 0, 1, 1, cgs.media.healthSegmentShader);
			trap_R_SetColor(NULL);
		}

		armorWidth = 11;
		if (armor < maxHealth) {
			armorWidth = (int)((double)armor / maxHealth * 11.0);
		}
		if (armor) {
			trap_R_SetColor(armorColor);
			trap_R_DrawStretchPic(barX, barY + 3 * scale, armorWidth * scale, scale, 0, 0, 0, 0, cgs.media.whiteShader);
			trap_R_SetColor(NULL);
		}

		iconY = cgs.glconfig.vidHeight - ny * cgs.glconfig.vidHeight - nameHeight * scale - scale;
		if (count) {
			iconX = nx * cgs.glconfig.vidWidth - count * 8 * 0.5f * scale - (count * 2 - 2) * 0.5f * scale;
			for (j = 1; j <= PW_NUM_POWERUPS; j++) {
				if (!(cent->currentState.powerups & (1 << j)) || j == PW_FLIGHT || j == PW_FREEZE) {
					continue;
				}
				item = BG_FindItemForPowerup(j);
				if (!item) {
					continue;
				}
				trap_R_SetColor(colorWhite);
				trap_R_DrawStretchPic(iconX, iconY, 8 * scale, 8 * scale, 0, 0, 1, 1, trap_R_RegisterShader(item->icon));
				trap_R_SetColor(NULL);
				iconX += 10 * scale;
			}
		} else if (sameTeam) {
			iconX = nx * cgs.glconfig.vidWidth - 4 * scale;
			if (cg.time - cent->pe.lastPainTime < cg_drawHitFriendTime.integer) {
				trap_R_SetColor(colorRed);
				shader = cgs.media.friendHitShader;
			} else {
				trap_R_SetColor(tint);
				shader = cgs.media.friendShader;
			}
			trap_R_DrawStretchPic(iconX, iconY, 8 * scale, 8 * scale, 0, 0, 1, 1, shader);
			trap_R_SetColor(NULL);
		}
	}
}

void CG_DrawSpectator(float x, float y) {
	static const vec4_t grayColor = {0.73f, 0.73f, 0.73f, 0.7f};
	int saved = cg_currentWidescreen;
	int w;

	if (!cg_drawSpecMessages.integer) {
		return;
	}
	if ((cgs.gametype == GT_CA || cgs.gametype == GT_AD) && cg.snap->ps.pm_type == PM_SPECTATOR &&
		cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		CG_SetWidescreen(WIDESCREEN_CENTER);
		w = CG_DrawTextWidth("Round In Progress", 0.35f, 0, 0);
		CG_DrawText(320 - w / 2, 60, 0, 0.35f, colorWhite, "Round In Progress", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		CG_SetWidescreen(saved);
		return;
	}
	if (!cg.snap->ps.eFlags || cg.scoreBoardShowing) {
		return;
	}

	CG_DrawText(x, y, 0, 0.22f, colorWhite, "SPECTATOR MODE", 0, 0, ITEM_TEXTSTYLE_NORMAL);
	CG_DrawText(x, y + 12, 0, 0.18f, (float*)grayColor, "Press mouse button 1 to cycle through players", 0, 0,
				ITEM_TEXTSTYLE_NORMAL);

	if (menuSpecFollowHud && (cg.snap->ps.pm_flags & PMF_FOLLOW)) {
		return;
	}
	CG_SetWidescreen(WIDESCREEN_LEFT);
	if (cgs.gametype == GT_DUEL) {
		if (!cgs.clientinfo[0].specOnly) {
			CG_DrawText(20, 461, 0, 0.28f, colorWhite, "waiting to play", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		}
	} else if (cgs.gametype >= GT_TEAM) {
		CG_DrawText(20, 453, 0, 0.28f, colorWhite, "press ESC and use the JOIN buttons", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		CG_DrawText(20, 470, 0, 0.28f, colorWhite, "to enter the game", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}
	CG_SetWidescreen(saved);
}

static void CG_DrawVote(void) {
	char* s;
	int sec;

	if (cgs.gametype >= GT_TEAM && cgs.gametype != GT_RR &&
	    cg_complaintWarning.integer && cg.complaintEndTime > cg.time && !cg.demoPlayback) {
		if (cg.complaintClient >= 0 && cg.complaintClient < MAX_CLIENTS) {
			char yesKey[32], noKey[32];
			int csec = (cg.complaintEndTime - cg.time) / 1000;
			if (csec < 0) {
				csec = 0;
			}
			CG_KeyNameForCommand("vote yes", yesKey, sizeof(yesKey));
			CG_KeyNameForCommand("vote no", noKey, sizeof(noKey));
			s = va("File complaint against %s for team-killing?", cgs.clientinfo[cg.complaintClient].name);
			CG_Text_Paint(4, 300, 0.22f, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
			s = va("Press '%s' for Yes, or '%s' for No (%is)", yesKey, noKey, csec);
			CG_Text_Paint(8, 312, 0.22f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
			return;
		}
		s = NULL;
		switch (cg.complaintClient) {
			case -1: s = "Your complaint has been filed."; break;
			case -2: s = "Your complaint has been dismissed."; break;
			case -3: s = "Comlaints cannot be filed against server admins."; break;
			case -4: s = "You received friendly fire from a server admin."; break;
			default: break;
		}
		if (s) {
			CG_Text_Paint(3, 300, 0.22f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
			return;
		}
	}

	if (!cgs.voteTime) {
		return;
	}

	if (cgs.voteModified) {
		cgs.voteModified = qfalse;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	sec = (cgs.voteTime - cg.time + VOTE_TIME) / 1000;
	if (sec < 0) {
		sec = 0;
	}
	s = va("VOTE(%is):%s yes:%i no:%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo);
	CG_Text_Paint(4, 300, 0.22f, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	{
		char menuKey[32];
		CG_KeyNameForCommand("togglemenu", menuKey, sizeof(menuKey));
		s = va("or press %s then click Vote", menuKey);
	}
	CG_Text_Paint(8, 312, 0.22f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

static void CG_DrawTeamVote(void) {
	char* s;
	int sec, cs_offset;

	if (cgs.clientinfo[cg.clientNum].team == TEAM_RED)
		cs_offset = 0;
	else if (cgs.clientinfo[cg.clientNum].team == TEAM_BLUE)
		cs_offset = 1;
	else
		return;

	if (!cgs.teamVoteTime[cs_offset]) {
		return;
	}

	if (cgs.teamVoteModified[cs_offset]) {
		cgs.teamVoteModified[cs_offset] = qfalse;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	sec = (VOTE_TIME - (cg.time - cgs.teamVoteTime[cs_offset])) / 1000;
	if (sec < 0) {
		sec = 0;
	}
	s = va("TEAMVOTE(%is):%s yes:%i no:%i", sec, cgs.teamVoteString[cs_offset],
		   cgs.teamVoteYes[cs_offset], cgs.teamVoteNo[cs_offset]);
	CG_Text_Paint(4, 324, 0.22f, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

void CG_SetEndScoreboardMenu(void) {
	switch (cgs.gametype) {
	case GT_FFA:        menuScoreboard = Menus_FindByName("score_menu_ffa");         menuEndScoreboard = Menus_FindByName("endscore_menu_ffa"); break;
	case GT_DUEL:       menuScoreboard = Menus_FindByName("score_menu_duel");        menuEndScoreboard = Menus_FindByName("endscore_menu_duel"); break;
	case GT_RACE:       menuScoreboard = Menus_FindByName("score_menu_race");        menuEndScoreboard = Menus_FindByName("endscore_menu_race"); break;
	case GT_RR:         menuScoreboard = Menus_FindByName("score_menu_rr");          menuEndScoreboard = Menus_FindByName("endscore_menu_rr"); break;
	case GT_TEAM:       menuScoreboard = Menus_FindByName("teamscore_menu_tdm");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_tdm"); break;
	case GT_CA:         menuScoreboard = Menus_FindByName("teamscore_menu_ca");      menuEndScoreboard = Menus_FindByName("endteamscore_menu_ca"); break;
	case GT_CTF:        menuScoreboard = Menus_FindByName("teamscore_menu_ctf");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_ctf"); break;
	case GT_1FCTF:      menuScoreboard = Menus_FindByName("teamscore_menu_1fctf");   menuEndScoreboard = Menus_FindByName("endteamscore_menu_1fctf"); break;
	case GT_HARVESTER:  menuScoreboard = Menus_FindByName("teamscore_menu_har");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_har"); break;
	case GT_FREEZE:     menuScoreboard = Menus_FindByName("teamscore_menu_ft");      menuEndScoreboard = Menus_FindByName("endteamscore_menu_ft"); break;
	case GT_DOMINATION: menuScoreboard = Menus_FindByName("teamscore_menu_dom");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_dom"); break;
	case GT_AD:         menuScoreboard = Menus_FindByName("teamscore_menu_ad");      menuEndScoreboard = Menus_FindByName("endteamscore_menu_ad"); break;
	default:            menuScoreboard = Menus_FindByName("teamscore_menu");         menuEndScoreboard = Menus_FindByName("endteamscore_menu"); break;
	}
}

static void CG_StartMenuCursor(void) {
	if (cgs.cursorX == 0 && cgs.cursorY == 0) {
		cgs.cursorX = SCREEN_WIDTH / 2;
		cgs.cursorY = SCREEN_HEIGHT / 2;
		cgDC.cursorx = cgs.cursorX;
		cgDC.cursory = cgs.cursorY;
	}
}

static const char *cg_inGameScoreboards[] = {
	"score_menu", "teamscore_menu", "score_menu_ffa", "score_menu_duel", "score_menu_race",
	"score_menu_rr", "teamscore_menu_tdm", "teamscore_menu_ca", "teamscore_menu_ctf",
	"teamscore_menu_1fctf", "teamscore_menu_har", "teamscore_menu_ft", "teamscore_menu_dom",
	"teamscore_menu_ad"
};
static const char *cg_endScoreboards[] = {
	"endscore_menu", "endteamscore_menu", "endscore_menu_ffa", "endscore_menu_duel",
	"endscore_menu_race", "endscore_menu_rr", "endteamscore_menu_tdm", "endteamscore_menu_ca",
	"endteamscore_menu_ctf", "endteamscore_menu_1fctf", "endteamscore_menu_har",
	"endteamscore_menu_ft", "endteamscore_menu_dom", "endteamscore_menu_ad"
};

static qboolean CG_DrawScoreboard(void) {
	static qboolean firstTime = qtrue;
	menuDef_t *menu;
	qboolean showing;
	int pmType;
	int i;

	pmType = cg.predictedPlayerState.pm_type;
	showing = qtrue;

	if (cg_paused.integer) {
		showing = qfalse;
	} else if (cg.warmup) {
		if (!cg.showScores && pmType != PM_DEAD) {
			showing = qfalse;
		}
	} else {
		if (!cg.showScores && pmType != PM_DEAD && pmType != PM_INTERMISSION) {
			showing = qfalse;
		}
	}

	if (showing) {
		if (pmType == PM_INTERMISSION) {
			menu = menuEndScoreboard;
			for (i = 0; i < (int)ARRAY_LEN(cg_inGameScoreboards); i++) {
				Menus_CloseByName(cg_inGameScoreboards[i]);
			}
		} else {
			menu = menuScoreboard;
		}

		if (menu == NULL) {
			return qfalse;
		}

		if (menu == menuEndScoreboard) {
			if (!(menu->window.flags & WINDOW_VISIBLE)) {
				Menus_CloseByName("joingame_menu");
				cg.showJoinMenu = qfalse;
				CG_StartMenuCursor();
				Menus_Activate(menu);
			}
			if (!(trap_Key_GetCatcher() & KEYCATCH_UI)) {
				trap_Key_SetCatcher(trap_Key_GetCatcher() | KEYCATCH_CGAME);
			}
		}

		if (firstTime) {
			CG_SetScoreSelection(menu);
			firstTime = qfalse;
		}
		Menu_Paint(menu, qtrue);
	}

	if (++cg.deferredPlayerLoading > 10) {
		CG_LoadDeferredPlayers();
	}

	if (showing) {
		return qtrue;
	}

	cg.deferredPlayerLoading = 0;
	firstTime = qtrue;
	if (cg.scoreBoardShowing) {
		for (i = 0; i < 2; i++) {
			Menus_CloseByName(cg_inGameScoreboards[i]);
			Menus_CloseByName(cg_endScoreboards[i]);
		}
		for (i = 2; i < (int)ARRAY_LEN(cg_inGameScoreboards); i++) {
			Menus_CloseByName(cg_inGameScoreboards[i]);
		}
		for (i = 2; i < (int)ARRAY_LEN(cg_endScoreboards); i++) {
			Menus_CloseByName(cg_endScoreboards[i]);
		}
		if (!cg.showJoinMenu) {
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_CGAME);
		}
	}
	trap_Key_SetCatcher(trap_Key_GetCatcher() & ~0x10);
	return qfalse;
}

static void CG_DrawIntermission(void) {
	cg.scoreFadeTime = cg.time;
	cg.scoreBoardShowing = CG_DrawScoreboard();
}

static void CG_DrawAmmoWarning(void) {
	const char* s;
	float scale, y;

	if (cg_drawAmmoWarning.integer == 0) {
		return;
	}

	if (!cg.lowAmmoWarning) {
		return;
	}

	if (cg.lowAmmoWarning == 2) {
		s = "OUT OF AMMO";
	} else {
		s = "LOW AMMO WARNING";
	}
	scale = (cg_drawAmmoWarning.integer == 1) ? 0.35f : 0.24f;
	y = (cg_drawAmmoWarning.integer == 1) ? 55.0f : 52.0f;
	CG_DrawText((float)(320 - CG_DrawTextWidth(s, scale, 0, 0) / 2), y, 0, scale, colorWhite, s, 0, 0,
				ITEM_TEXTSTYLE_SHADOWEDMORE);
}

static void CG_DrawProxWarning(void) {
	char s[32];
	static int proxTime;
	int proxTick;

	if (!(cg.snap->ps.eFlags & EF_TICKING)) {
		proxTime = 0;
		return;
	}

	if (proxTime == 0) {
		proxTime = cg.time;
	}

	proxTick = 10 - ((cg.time - proxTime) / 1000);

	if (proxTick > 0 && proxTick <= 5) {
		Com_sprintf(s, sizeof(s), "INTERNAL COMBUSTION IN: %i", proxTick);
	} else {
		Com_sprintf(s, sizeof(s), "YOU HAVE BEEN MINED");
	}

	CG_DrawText((float)(320 - CG_DrawTextWidth(s, 0.25f, 0, 0) / 2), 112, 0, 0.25f, colorRed, s, 0, 0,
				ITEM_TEXTSTYLE_SHADOWED);
}

static void CG_DrawWarmupLine(float y, int font, float scale, vec4_t color, const char* text, int style) {
	int w = CG_DrawTextWidth(text, scale, 0, font);

	CG_DrawText((float)(320 - w / 2), y, font, scale, color, text, 0, 0, style);
}

static void CG_ReadyUpKeyName(char* buf, int size) {
	trap_Key_KeynumToStringBuf(trap_Key_GetKey("readyup"), buf, size);
}

static void CG_DrawWarmupTutorial(void) {
	char key[32];
	const char* s;
	float y;

	if (!g_skipTrainingEnable.integer) {
		if (g_training.integer != 1) {
			return;
		}
		CG_DrawWarmupLine(35, 0, 0.35f, colorWhite, "This match will determine", ITEM_TEXTSTYLE_SHADOWEDMORE);
		CG_DrawWarmupLine(55, 0, 0.35f, colorWhite, "your skill placement", ITEM_TEXTSTYLE_SHADOWEDMORE);
		CG_ReadyUpKeyName(key, sizeof(key));
		s = va("Press %s to start the match", key);
		y = 85;
	} else {
		CG_ReadyUpKeyName(key, sizeof(key));
		s = va("Press %s to skip the tutorial", key);
		y = 30;
	}
	CG_DrawWarmupLine(y, 0, 0.25f, colorRed, s, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

static void CG_DrawADRoundResults(void) {
	static const vec4_t panelColor = {0.1f, 0.1f, 0.1f, 0.8f};
	static const vec4_t redRowColor = {0.5f, 0.0f, 0.0f, 0.5f};
	static const vec4_t redCellColor = {1.0f, 0.0f, 0.0f, 0.5f};
	static const vec4_t blueRowColor = {0.0f, 0.25f, 0.5f, 0.5f};
	static const vec4_t blueCellColor = {0.0f, 0.5f, 1.0f, 0.5f};
	static const vec4_t labelColor = {1.0f, 1.0f, 0.5f, 1.0f};
	static const vec4_t blueTextColor = {0.0f, 0.5f, 1.0f, 1.0f};
	const char* s = NULL;
	const float* color = colorRed;
	int round, cell, turnPoints, threshold, red, blue, i, col, w;
	char text[16];

	round = cg.warmupRound - 1;
	cell = (round < 9) ? round : 9;
	turnPoints = (cgs.adCaptureScoreBonus > cgs.adElimScoreBonus ? cgs.adCaptureScoreBonus : cgs.adElimScoreBonus) +
				 cgs.adtouchScoreBonus;
	CG_FillRect(200, 150, 240, 48, panelColor);
	if (cg.warmupTurn == 0) {
		CG_FillRect(200, 166, 204, 16, redRowColor);
		CG_FillRect((cell + 15) * 16, 166, 16, 16, redCellColor);
	} else if (cg.warmupTurn == 1) {
		CG_FillRect(200, 182, 204, 16, blueRowColor);
		CG_FillRect((cell + 15) * 16, 182, 16, 16, blueCellColor);
	}

	red = cg.teamScores[0];
	blue = cg.teamScores[1];
	threshold = cgs.scorelimit - turnPoints;
	if (cgs.scorelimit && (red > blue ? red : blue) >= threshold) {
		if (cg.warmupTurn == 0) {
			if (red >= threshold) {
				s = "Match Point";
			}
		} else if (red >= cgs.scorelimit && red - blue > turnPoints) {
			s = "Red Wins! Good Game";
		} else if (red >= cgs.scorelimit && red != blue) {
			s = "Last Chance";
			color = blueTextColor;
		} else if (blue >= threshold) {
			s = "Match Point";
			color = blueTextColor;
		}
		if (s) {
			w = CG_DrawTextWidth(s, 0.3f, 0, 0);
			CG_DrawText(320 - w * 0.5f, 214, 0, 0.3f, (float*)color, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		}
	}

	CG_DrawText(204, 162, 0, 0.2f, (float*)labelColor, "Round", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	CG_DrawText(204, 178, 0, 0.25f, colorRed, "Red", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	CG_DrawText(204, 194, 0, 0.25f, (float*)blueTextColor, "Blue", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	for (i = 0; i < 20; i++) {
		col = i / 2;
		if (i % 2 == 0) {
			Com_sprintf(text, sizeof(text), "%d", (round > 9) ? col + round - 8 : col + 1);
			w = CG_DrawTextWidth(text, 0.2f, 0, 0);
			CG_DrawText((col + 15) * 16 + (16 - w) * 0.5f, 162, 0, 0.2f, (float*)labelColor, text, 0, 0,
						ITEM_TEXTSTYLE_SHADOWED);
		}
		if (cg.adScores[i] > -1) {
			Com_sprintf(text, sizeof(text), "%d", cg.adScores[i]);
		} else {
			text[0] = '\0';
		}
		w = CG_DrawTextWidth(text, 0.25f, 0, 0);
		CG_DrawText(col * 16 + 240 + (16 - w) * 0.5f, i % 2 * 16 + 178, 0, 0.25f, colorWhite, text, 0, 0,
					ITEM_TEXTSTYLE_SHADOWED);
	}

	CG_DrawText(407, 162, 0, 0.2f, (float*)labelColor, "Score", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	Com_sprintf(text, sizeof(text), "%d", red);
	w = CG_DrawTextWidth(text, 0.25f, 0, 0);
	CG_DrawText(415 + (16 - w) * 0.5f, 178, 0, 0.25f, colorRed, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	Com_sprintf(text, sizeof(text), "%d", blue);
	w = CG_DrawTextWidth(text, 0.25f, 0, 0);
	CG_DrawText(415 + (16 - w) * 0.5f, 194, 0, 0.25f, (float*)blueTextColor, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

static void CG_DrawWarmupCountdown(int gt) {
	float y = (cgs.gametype == GT_RACE) ? 10.0f : 0.0f;
	qboolean rounds = (gt == GT_CA || gt == GT_FREEZE || gt == GT_AD || gt == GT_RR) && cg.warmupRound > 0;
	const char* header = "";
	int i, first = -1, last = -1, team;

	if (gt == GT_DUEL) {
		for (i = 0; i < cgs.maxclients; i++) {
			if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE) {
				if (first < 0) {
					first = i;
				} else {
					last = i;
				}
			}
		}
		if (first >= 0 && last >= 0) {
			header = va("%s vs %s", cgs.clientinfo[first].name, cgs.clientinfo[last].name);
		}
	} else if (gt == GT_FFA && (cgs.customSettings & 0x40000)) {
		header = "Quad Hog";
	} else if (gt == GT_RR && (cgs.customSettings & 0x4000000) && cg.warmupRound == 0) {
		header = "Infected";
	} else if (gt == GT_AD && cg.warmupState == 1) {
		CG_DrawADRoundResults();
		team = cgs.clientinfo[cg.clientNum].team;
		if (team == TEAM_RED) {
			header = cg.warmupTurn ? "DEFEND THE FLAG" : "ATTACK THE FLAG";
		} else if (team == TEAM_BLUE) {
			header = cg.warmupTurn ? "ATTACK THE FLAG" : "DEFEND THE FLAG";
		} else {
			header = cg.warmupTurn ? "Blue is on offense" : "Red is on offense";
		}
	} else if (gt == GT_FREEZE && !cgs.freezeRoundDelay) {
		header = gametypeDisplayNames[GT_FREEZE];
	} else if (rounds) {
		header = (cgs.timelimit && (cg.timelimitWarnings & 4) && cg.time - cgs.levelStartTime > cgs.timelimit * 60000)
					 ? "Sudden Death"
					 : "Round Begins in";
	} else if (gt < 0 || gt > GT_RR) {
		header = "Unknown Gametype";
	} else {
		header = gametypeDisplayNames[gt];
	}

	CG_DrawWarmupLine(y + 90, 0, 0.6f, colorWhite, header, ITEM_TEXTSTYLE_SHADOWEDMORE);
	CG_DrawWarmupLine(y + 125, 0, 0.45f, colorWhite, va(rounds ? "%i" : "Starts in: %i", cg.warmupCount),
					  ITEM_TEXTSTYLE_SHADOWEDMORE);
}

static void CG_DrawWarmup(void) {
	float y = (cgs.gametype == GT_RACE) ? 10.0f : 0.0f;
	int teamCounts[TEAM_NUM_TEAMS];
	const char* line1;
	const char* line2;
	char key[32];
	qboolean sound;
	int sec, i, gt;

	if (cgs.practice) {
		return;
	}

	if (cg.warmup == -1) {
		cg.warmupCount = -1;
	} else if (cg.warmup != 0 || cg.warmupCount != -1) {
		sec = cg.warmup - cg.time + 1000;
		sec = (sec < 0) ? 0 : sec / 1000;
		if (sec != cg.warmupCount) {
			sound = !BG_IsRoundBasedGameType(cgs.gametype) || (cgs.gametype == GT_FREEZE && !cgs.freezeRoundDelay);
			cg.warmupCount = sec;
			if (sound || (BG_IsRoundBasedGameType(cgs.gametype) && cg.warmupRound > 0)) {
				CG_PlayCountdownSound(sec);
			}
		}
	} else {
		return;
	}

	if (cg.showJoinMenu || cg.scoreBoardShowing) {
		return;
	}
	if (g_training.integer && !cgs.clientinfo[cg.clientNum].ready) {
		CG_DrawWarmupTutorial();
		return;
	}

	gt = (cg.warmupGametype == -1) ? cgs.gametype : cg.warmupGametype;
	if (cg.warmupCount > 0) {
		CG_DrawWarmupCountdown(gt);
		return;
	}
	if (cg.warmup >= 0) {
		return;
	}

	memset(teamCounts, 0, sizeof(teamCounts));
	for (i = 0; i < cgs.maxclients; i++) {
		if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team >= 0 && cgs.clientinfo[i].team < TEAM_NUM_TEAMS) {
			teamCounts[cgs.clientinfo[i].team]++;
		}
	}

	if (cg_drawPregameMessages.integer) {
		line1 = "The match will begin";
		line2 = "when more players are ready.";
		if (gt == GT_RR) {
			if ((cgs.customSettings & 0x4000000)
					? teamCounts[TEAM_RED] + teamCounts[TEAM_BLUE] < cgs.teamSizeMin
					: teamCounts[TEAM_RED] < cgs.teamSizeMin || teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
				line2 = "when more players join.";
			}
		} else if (gt < GT_TEAM) {
			if (gt == GT_DUEL && teamCounts[TEAM_FREE] > 2) {
				line1 = "The match will begin when";
				line2 = "fewer players are in the match.";
			} else if (teamCounts[TEAM_FREE] < 2) {
				line2 = "when more players join.";
			} else if (cgs.allReadyTime) {
				sec = (cgs.allReadyTime - cg.time) / 1000 + 1;
				if (sec < 0) {
					sec = 0;
				}
				line1 = "Players must ready";
				line2 = va("within %i second%s.", sec, sec != 1 ? "s" : "");
			}
		} else if (teamCounts[TEAM_RED] < cgs.teamSizeMin && teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
			line1 = "Waiting for more players.";
			line2 = va("The match requires %i player%s per team.", cgs.teamSizeMin, cgs.teamSizeMin != 1 ? "s" : "");
		} else if (teamCounts[TEAM_RED] < cgs.teamSizeMin || teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
			i = (teamCounts[TEAM_RED] < cgs.teamSizeMin) ? TEAM_RED : TEAM_BLUE;
			sec = cgs.teamSizeMin - teamCounts[i];
			line1 = va("Waiting for %i more player%s", sec, sec != 1 ? "s" : "");
			line2 = va("to join the %s.", (i == TEAM_RED) ? "Red Team" : "Blue Team");
		} else if (cgs.teamForceBalance && (teamCounts[TEAM_RED] > teamCounts[TEAM_BLUE] + 1 ||
											 teamCounts[TEAM_BLUE] > teamCounts[TEAM_RED] + 1)) {
			line1 = "The teams must be balanced";
			line2 = "before the match can begin.";
		}
		CG_DrawWarmupLine(y + 88, 1, 0.35f, colorWhite, line1, ITEM_TEXTSTYLE_SHADOWED);
		CG_DrawWarmupLine(y + 108, 1, 0.35f, colorWhite, line2, ITEM_TEXTSTYLE_SHADOWED);
	}

	if (cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) {
		return;
	}
	if (cgs.gametype < GT_TEAM ? teamCounts[TEAM_FREE] < 2
							   : teamCounts[TEAM_RED] < cgs.teamSizeMin || teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
		if (cgs.gametype != GT_RR || !(cgs.customSettings & 0x4000000) ||
			teamCounts[TEAM_RED] + teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
			return;
		}
	}
	CG_ReadyUpKeyName(key, sizeof(key));
	if (!cgs.clientinfo[cg.clientNum].ready) {
		CG_DrawWarmupLine(y + 124, 0, 0.25f, colorRed, va("Press %s to ready yourself", key), ITEM_TEXTSTYLE_SHADOWEDMORE);
	} else {
		CG_DrawWarmupLine(y + 124, 0, 0.25f, colorGreen, va("Press %s to unready yourself", key), ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
}

void CG_DrawTimedMenus(void) {
	if (cg.voiceTime) {
		int t = cg.time - cg.voiceTime;
		if (t > 2500) {
			Menus_CloseByName("voiceMenu");
			trap_Cvar_Set("cl_conXOffset", "0");
			cg.voiceTime = 0;
		}
	}
}

void CG_PlayFightSound(void) {
	sfxHandle_t sfx = cgs.media.goSound;
	const char* text = "GO!";
	int team;

	if (cgs.gametype != GT_RACE) {
		sfx = cgs.media.countFightSound;
		text = "FIGHT!";
		if (cgs.gametype == GT_AD) {
			team = cg.snap->ps.persistant[PERS_TEAM];
			if (team == TEAM_RED) {
				if (cg.warmupTurn) {
					sfx = cgs.media.defendTheFlagSound;
					text = "DEFEND THE FLAG!";
				} else {
					sfx = cgs.media.attackTheFlagSound;
					text = "ATTACK THE FLAG!";
				}
			} else if (team == TEAM_BLUE) {
				if (cg.warmupTurn) {
					sfx = cgs.media.attackTheFlagSound;
					text = "ATTACK THE FLAG!";
				} else {
					sfx = cgs.media.defendTheFlagSound;
					text = "DEFEND THE FLAG!";
				}
			}
		} else if (cgs.gametype == GT_RR) {
			if (cg.warmupRound > 0) {
				trap_S_StartLocalSound(cgs.media.kamikazeRespawnSound, CHAN_LOCAL_SOUND);
			}
			if (cg.snap->ps.pm_flags & PMF_LOADOUT_FORCED) {
				sfx = cgs.media.biteSound;
				text = "BITE!";
			}
		}
	}

	CG_CenterPrint(text, 120, 0.5f);
	CG_ResetAnnouncements();
	trap_S_StartLocalSoundVolume(sfx, CHAN_ANNOUNCER, s_announcerVolume.value);
	cg.warmupCount = -1;
}

void CG_PlayCountdownSound(int count) {
	sfxHandle_t sfx;

	switch (count) {
		case 0:
			if (BG_IsRoundBasedGameType(cgs.gametype)) {
				CG_PlayFightSound();
			}
			return;
		case 1:
			sfx = cgs.media.count1Sound;
			break;
		case 2:
			sfx = cgs.media.count2Sound;
			break;
		case 3:
			sfx = cgs.media.count3Sound;
			break;
		case 5:
			if (cgs.freezeEnd) {
				sfx = cgs.media.countPrepareSound;
			} else {
				if (!BG_IsRoundBasedGameType(cgs.gametype)) {
					return;
				}
				if (cgs.gametype == GT_FREEZE && cgs.freezeRoundDelay < 5000) {
					return;
				}
				sfx = cgs.media.roundBeginsInSound;
			}
			break;
		default:
			return;
	}
	trap_S_StartLocalSoundVolume(sfx, CHAN_ANNOUNCER, s_announcerVolume.value);
}

static void CG_DrawTimeout(void) {
	const char* s;
	float w;
	int sec;

	if (!cgs.freezeEnd) {
		if (cgs.pauseEnd) {
			cgs.roundStartTime += (cgs.pauseEnd - cg.time - 1000) / 1000;
			cgs.pauseEnd = 0;
			return;
		}
		cgs.roundStartTime += cg.frametime;
	} else if (!cgs.pauseEnd) {
		cgs.roundStartTime += cg.frametime;
	}

	if (cgs.pauseSoundTime < cgs.freezeEnd) {
		cgs.pauseSoundTime = cgs.freezeEnd;
		trap_S_StartLocalSound(cgs.media.klaxon1Sound, CHAN_LOCAL_SOUND);
	}

	if (cgs.pauseEnd == 0) {
		sec = -1;
		s = "Match Paused";
	} else {
		sec = (cgs.pauseEnd - cg.time) / 1000;
		s = va("Match resuming in ^5%d^7 seconds", sec);
	}

	if (sec != 0) {
		w = CG_Text_Width(s, 0.5f, 0);
		CG_Text_Paint(320 - w / 2, 128, 0.5f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	if (cg.time - cgs.pauseSoundTime >= 1000) {
		cgs.pauseSoundTime = cg.time;
		CG_PlayCountdownSound(sec);
	}
}

void CG_UpdateJoinMenu(void) {
	menuDef_t* menu;

	if (menuJoinGame == NULL) {
		menuJoinGame = Menus_FindByName("joingame_menu");
	}
	menu = menuJoinGame;
	if (menu == NULL) {
		return;
	}

	if (!cg_paused.integer) {
		if (cgs.eventHandling == CGAME_EVENT_ESCAPE) {
			cgs.eventHandling = CGAME_EVENT_NONE;
		}
	} else if (cgs.eventHandling == CGAME_EVENT_ESCAPE) {
		return;
	}

	if (!(menu->window.flags & WINDOW_VISIBLE)) {
		CG_StartMenuCursor();
		trap_Key_SetCatcher(KEYCATCH_CGAME);
		Menus_Activate(menu);
	}

	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		CG_CloseMenus();
		return;
	}

	Menu_Paint(menu, qtrue);
}

static const float* CG_SpecTeamColor(int team) {
	static const vec4_t freeColor = {1.0f, 0.8f, 0.2f, 1.0f};
	static const vec4_t redColor = {1.0f, 0.2f, 0.1f, 1.0f};
	static const vec4_t blueColor = {0.2f, 0.4f, 1.0f, 1.0f};
	static const vec4_t spectatorColor = {0.75f, 0.75f, 0.75f, 1.0f};

	switch (team) {
	case TEAM_FREE:
		return freeColor;
	case TEAM_RED:
		return redColor;
	case TEAM_BLUE:
		return blueColor;
	case TEAM_SPECTATOR:
		return spectatorColor;
	default:
		return colorWhite;
	}
}

static void CG_DrawSpecItemTimers(void) {
	specItemTimer_t* timer;
	gitem_t* item;
	vec4_t band;
	float size, x, y, step;
	int mask, i, teamed, red, blue, last, seconds;

	mask = cg_specItemTimers.integer;
	if (!mask || cg.showJoinMenu || !cg.numSpecItemTimers || !(cg.predictedPlayerState.eFlags & EF_SPECTATOR)) {
		return;
	}

	size = cg_specItemTimersSize.value * 100.0f;
	x = cg_specItemTimersX.integer;
	y = cg_specItemTimersY.integer;

	teamed = 0;
	for (i = 0; i < MAX_SPEC_ITEM_TIMERS; i++) {
		timer = &cg.specItemTimers[i];
		if (timer->entityNum && timer->side != TEAM_RED && timer->team) {
			teamed++;
		}
	}

	red = blue = last = 0;
	for (i = 0; i < MAX_SPEC_ITEM_TIMERS; i++) {
		timer = &cg.specItemTimers[i];
		if (!timer->entityNum) {
			continue;
		}
		item = &bg_itemlist[timer->itemIndex];
		if (!(mask & 1) && item->giType == IT_POWERUP) {
			continue;
		}
		if (!(mask & 2) && item->giType == IT_HEALTH && item->giTag == 1) {
			continue;
		}
		if (!(mask & 4) && item->giType == IT_ARMOR && item->giTag == 1) {
			continue;
		}
		if (!(mask & 8) && item->giType == IT_ARMOR && item->giTag == 2) {
			continue;
		}

		seconds = (int)ceil(timer->time / 1000.0);
		if (timer->side == TEAM_RED || timer->side == TEAM_BLUE) {
			Vector4Copy(CG_SpecTeamColor(timer->side), band);
			band[3] = 0.25f;
			CG_FillRect(x, y, size * 2.25f, size, band);
		}
		if (timer->team) {
			if (timer->side == TEAM_RED) {
				last = ++red;
			} else {
				last = ++blue;
			}
			step = size * 0.25f;
			CG_DrawPic(x + last * step - 1.0f - step, y + last * step - 1.0f - step, size * 0.5f, size * 0.5f,
					   trap_R_RegisterShader(item->icon));
		} else {
			CG_DrawPic(x, y, size, size, trap_R_RegisterShader(item->icon));
		}
		if (seconds > 0) {
			CG_DrawText(x + size + 3.0f, y + size - 5.0f, 0, cg_specItemTimersSize.value, colorWhite, va("%d", seconds), 0,
						0, ITEM_TEXTSTYLE_SHADOWED);
		}
		if (!timer->team || last >= teamed) {
			y += size + 2.0f;
		}
	}
}

static void CG_DrawSpecTeamVitals(void) {
	static const vec4_t numberColor = {1.0f, 1.0f, 1.0f, 0.25f};
	static const vec4_t gradientBack = {0.0f, 0.0f, 0.0f, 0.25f};
	static const vec4_t armorCyan = {0.0f, 1.0f, 1.0f, 1.0f};
	float yTeam[TEAM_NUM_TEAMS];
	float* yp;
	float y, step, numberWidth, barHeight, armorY, armorHeight;
	int i, j, clientNum, team, health, armor, maxHealth, width, x, len, nameHeight, barY;
	qboolean self, dead, gradient;
	vec4_t color, armorColor;
	const float* textColor;
	centity_t* cent;
	clientInfo_t* ci;
	gitem_t* item;
	char name[40], cut[40];

	if (!cg_specTeamVitals.integer || cg.showJoinMenu || cgs.gametype < GT_TEAM) {
		return;
	}
	for (i = 0; i < TEAM_NUM_TEAMS; i++) {
		yTeam[i] = cg_specTeamVitalsY.integer;
	}

	for (i = 0; i < cgs.maxclients; i++) {
		cent = &cg_entities[i];
		clientNum = cent->currentState.clientNum;
		ci = &cgs.clientinfo[clientNum];
		if (!(cent->currentState.eType & 0xb) || !ci->infoValid) {
			continue;
		}

		width = cg_specTeamVitalsWidth.integer;
		self = (clientNum == cg.snap->ps.clientNum);
		if (self) {
			health = cg.snap->ps.stats[STAT_HEALTH];
			armor = cg.snap->ps.stats[STAT_ARMOR];
		} else {
			health = cent->currentState.health;
			armor = cent->currentState.armor;
		}
		gradient = (cg_specTeamVitalsHealthColor.integer != 0);
		step = gradient ? 5.0f : 1.0f;
		team = ci->team;
		x = (team == TEAM_RED) ? 5 : (int)(640.0f - width - 5.0f - 16.0f - 2.0f);
		numberWidth = (cg_specTeamVitals.integer == 2) ? 21.0f : 0.0f;
		if (team == TEAM_SPECTATOR) {
			continue;
		}
		if (team == TEAM_RED) {
			CG_SetWidescreen(WIDESCREEN_LEFT);
		} else if (team == TEAM_BLUE) {
			CG_SetWidescreen(WIDESCREEN_RIGHT);
		}
		dead = (cent->currentState.eFlags & EF_DEAD) != 0;
		maxHealth = (int)(ci->handicap / 100.0 * cgs.startingHealth);
		if (maxHealth < 1 || maxHealth > cgs.startingHealth) {
			maxHealth = cgs.startingHealth;
		}

		yp = &yTeam[team];
		y = *yp;
		CG_DrawText(x - 5.0f, y, 1, 0.12f, (float*)numberColor, va("%d", clientNum), 0, 0, ITEM_TEXTSTYLE_SHADOWED);
		if (self) {
			CG_DrawPic(x - 1.0f, y - 16.0f - 1.0f, 18, 18, cgs.media.voiceWindowShader);
		}
		if (cent->currentState.powerups) {
			for (j = 1; j <= PW_NUM_POWERUPS; j++) {
				if ((cent->currentState.powerups & (1 << j)) && j != PW_FLIGHT && j != PW_FREEZE &&
					(item = BG_FindItemForPowerup(j)) != NULL) {
					CG_DrawPic(x, y - 16.0f, 16, 16, trap_R_RegisterShader(item->icon));
					break;
				}
			}
		} else if (dead) {
			CG_DrawPic(x, y - 16.0f, 16, 16, cgs.media.friendDeadShader);
		} else {
			CG_DrawPic(x, y - 16.0f, 16, 16,
					   ci->modelIcon ? ci->modelIcon : cg_weapons[cent->currentState.weapon].weaponIcon);
		}
		if (gradient) {
			*yp = y - step;
		}

		Q_strncpyz(name, ci->name, sizeof(name));
		Q_CleanStr(name);
		len = strlen(name);
		if (CG_OverheadTextWidth(name, 0, 0.18f, len) > width - numberWidth) {
			do {
				len--;
			} while (len > 1 && CG_OverheadTextWidth(name, 0, 0.18f, len) > width - numberWidth);
			Q_strncpyz(cut, name, len);
			len += 3;
			Q_strncpyz(name, va("%s%s", cut, "..."), sizeof(name));
		}
		nameHeight = CG_DrawTextHeight(name, 0.18f, len, 0);

		barY = 0;
		if (!gradient) {
			barY = (int)(*yp - 16.0f);
			color[0] = (team == TEAM_RED) ? 1.0f : 0.0f;
			color[1] = 0.0f;
			color[2] = (team == TEAM_RED) ? 0.0f : 1.0f;
			color[3] = 0.25f;
			CG_FillRect(x + 18.0f, barY, width, nameHeight + 2, color);
		}
		if (dead) {
			health = 0;
		}
		if (gradient) {
			CG_HealthGradientColor(clientNum, health, color);
			color[3] = 1.0f;
			CG_FillRect(x + 18.0f, *yp, width, 4, gradientBack);
			barY = (int)*yp;
			barHeight = 4.0f;
		} else {
			if (health > maxHealth) {
				Vector4Copy(CG_SpecTeamColor(team), color);
				color[3] = 0.5f;
			} else {
				color[0] = (team == TEAM_RED) ? 1.0f : 0.0f;
				color[1] = 0.0f;
				color[2] = (team == TEAM_RED) ? 0.0f : 1.0f;
				color[3] = 0.75f;
			}
			barHeight = nameHeight + 2;
		}
		CG_FillRect(x + 18.0f, barY, health > maxHealth ? width : (float)health / maxHealth * width, barHeight, color);

		if (gradient) {
			*yp += step;
			textColor = CG_OverheadTeamColor(team);
		} else {
			*yp -= step;
			textColor = colorWhite;
		}
		if (numberWidth != 0.0f) {
			CG_DrawText(x + 18.0f + 1.0f, *yp - nameHeight + 1.0f, 1, 0.18f, (float*)textColor, va("%d", health), 0, 0,
						ITEM_TEXTSTYLE_SHADOWED);
		}
		CG_DrawText(x + 18.0f + numberWidth + 1.0f, *yp - nameHeight + 1.0f, 0, 0.18f, (float*)textColor, name, 0, len,
					ITEM_TEXTSTYLE_SHADOWED);

		if (gradient) {
			Vector4Copy(armorCyan, armorColor);
			armorY = *yp - 1.0f;
			armorHeight = 1.0f;
		} else {
			Vector4Copy(CG_SpecTeamColor(team), armorColor);
			armorColor[3] = 1.0f;
			armorY = *yp - 5.0f;
			armorHeight = 6.0f;
		}
		CG_FillRect(x + 18.0f, armorY, armor > maxHealth ? width : (float)armor / maxHealth * width, armorHeight,
					armorColor);
		CG_SetWidescreen(WIDESCREEN_STRETCH);
		*yp += 22.0f;
	}
}

qboolean cg_letterboxDrawn;
static int letterboxTime;
static int letterboxDuration;
static qboolean letterboxOpening;

static void CG_DrawLetterbox(void) {
	int height;

	cg_letterboxDrawn = qfalse;
	if (cg.showJoinMenu || cg.scoreBoardShowing || !letterboxDuration) {
		return;
	}

	if (cg.snap->ps.pm_type != PM_TUTORIAL && letterboxOpening == qtrue) {
		letterboxOpening = qfalse;
		letterboxDuration = 1000;
		letterboxTime = cg.time;
	} else if (!letterboxTime) {
		letterboxTime = cg.time;
	}

	if (letterboxOpening == qtrue) {
		if (cg.time > letterboxTime + letterboxDuration) {
			height = 84;
		} else {
			height = (int)((double)(cg.time - letterboxTime) / letterboxDuration * 84.0);
			if (!height) {
				height = 1;
			}
		}
	} else if (cg.time > letterboxTime + letterboxDuration) {
		height = 0;
	} else {
		height = (int)(84.0 - (double)(cg.time - letterboxTime) / letterboxDuration * 84.0);
	}

	if (height <= 0) {
		letterboxOpening = qtrue;
		letterboxTime = 0;
		letterboxDuration = 0;
		return;
	}

	CG_FillRect(0, 0, 640, height, colorBlack);
	CG_FillRect(0, 480 - height, 640, height, colorBlack);
	cg_letterboxDrawn = qtrue;
}

static void CG_DrawVoiceIndicator(void) {
    static const vec4_t red = {0.5f, 0.0f, 0.0f, 0.5f};
    static const vec4_t blue = {0.0f, 0.25f, 0.5f, 0.5f};
    static const vec4_t neutral = {0.5f, 0.5f, 0.5f, 0.5f};
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    float y = 150.0f;
    int i;

    if (!cg_voiceChatIndicator.integer || cg.snap->ps.pm_type == PM_INTERMISSION) {
        return;
    }

    CG_SetWidescreen(cg_voiceChatIndicator.integer == 2 ? 1 : 3);

    for (i = 0; i < cgs.maxclients; i++) {
        clientInfo_t* ci = &cgs.clientinfo[i];
        const float* color;
        float width;
        float x;

        if (!ci->infoValid || !ci->talking) {
            continue;
        }

        trap_R_SetColor(NULL);
        width = (float)(CG_DrawStrlen(ci->name) * SMALLCHAR_WIDTH) + 25.0f;
        x = (cg_voiceChatIndicator.integer == 2) ? 0.0f : 640.0f - width;
        color = (ci->team == TEAM_RED) ? red : (ci->team == TEAM_BLUE) ? blue : neutral;

        CG_FillRect(x, y - 10.0f, width, 15.0f, color);
        CG_DrawPic(x + 5.0f, y - 8.0f, 10.0f, 10.0f, cgs.media.scoreSpeakingShader);
        CG_DrawSmallStringColor((int)(x + 20.0f), (int)(y + 1.0f), ci->name, (vec_t*)white);
        y += 15.0f;

        if (ci->talkingTime + 500 < cg.time && i != cg.clientNum) {
            ci->talking = qfalse;
        }
    }

    CG_SetWidescreen(0);
}

#define DEMOBAR_X	140
#define DEMOBAR_W	360
#define DEMOBAR_Y	318
#define DEMOBAR_H	56
#define DEMOBAR_SPAN	(DEMOBAR_W - 20)
#define DEMOBAR_LINE	(DEMOBAR_Y + 21)

static const vec4_t demoBack = { 0, 0, 0, 0.5f };
static const vec4_t demoTrack = { 1, 1, 1, 0.25f };
static const vec4_t demoRestart = { 1, 1, 1, 0.5f };
static const vec4_t demoGhost = { 1, 1, 1, 0.7f };

#define MAX_DEMO_BUTTONS	16
static struct {
	float		x, y, w, h;
	const char	*cmd;
} demoButtons[MAX_DEMO_BUTTONS];
static int		demoButtonCount;
static int		demoLength;
qboolean		cg_demoCursor;
static qboolean	demoDragging;

static void CG_DrawDemoButton(float x, float y, const char *label, const char *key, qboolean on,
	const char *cmd) {
	CG_DrawText(x - CG_DrawTextWidth(label, 0.25f, 0, 1) / 2, y, 1, 0.25f,
		on ? colorYellow : colorWhite, (char *)label, 0, 0, 3);
	CG_DrawText(x - CG_DrawTextWidth(key, 0.15f, 0, 1) / 2, y + 10, 1, 0.15f, colorYellow,
		(char *)key, 0, 0, 3);
	if (cmd && demoButtonCount < MAX_DEMO_BUTTONS) {
		demoButtons[demoButtonCount].x = x - 17;
		demoButtons[demoButtonCount].y = y - 9;
		demoButtons[demoButtonCount].w = 34;
		demoButtons[demoButtonCount].h = 22;
		demoButtons[demoButtonCount].cmd = cmd;
		demoButtonCount++;
	}
}

static void CG_DrawDemoMarks(const char *cvar, int length, float y, float w, float h, const float *color) {
	char		buf[MAX_STRING_CHARS];
	const char	*p;
	int			t;

	trap_Cvar_VariableStringBuffer(cvar, buf, sizeof(buf));
	for (p = buf; *p; ) {
		t = atoi(p);
		if (t > 0 && t < length) {
			CG_FillRect(DEMOBAR_X + 10 + (float)DEMOBAR_SPAN * t / length - w / 2, y, w, h, color);
		}
		while (*p && *p != ' ') {
			p++;
		}
		while (*p == ' ') {
			p++;
		}
	}
}

static void CG_DemoTimeString(int ms, char *buf, int size) {
	Com_sprintf(buf, size, "%d:%02d", ms / 60000, ms / 1000 % 60);
}

static int CG_DemoCursorTime(void) {
	float f = (cgs.cursorX - (DEMOBAR_X + 10)) / (float)DEMOBAR_SPAN;

	return (int)(Com_Clamp(0, 1, f) * demoLength);
}

void CG_DemoCursorFrame(void) {
	qboolean	want = cg.demoPlayback && cg_drawDemoHUD.integer && cg_draw2D.integer &&
		!cg_freecam.integer ? qtrue : qfalse;
	int			catcher = trap_Key_GetCatcher();

	if (want && !cg_demoCursor && !(catcher & ~KEYCATCH_CGAME)) {
		if (!(catcher & KEYCATCH_CGAME)) {
			trap_Key_SetCatcher(catcher | KEYCATCH_CGAME);
		}
		cg_demoCursor = qtrue;
	} else if (!want && cg_demoCursor) {
		if (catcher & KEYCATCH_CGAME) {
			trap_Key_SetCatcher(catcher & ~KEYCATCH_CGAME);
		}
		cg_demoCursor = qfalse;
		demoDragging = qfalse;
	} else if (cg_demoCursor && !(catcher & KEYCATCH_CGAME)) {
		cg_demoCursor = qfalse;
		demoDragging = qfalse;
	}
}

qboolean CG_DemoHUDKey(int key, qboolean down) {
	int i;

	if (!cg_demoCursor || key != K_MOUSE1) {
		return qfalse;
	}
	if (!down) {
		if (demoDragging) {
			demoDragging = qfalse;
			trap_SendConsoleCommand(va("seekdemo %.3f\n", CG_DemoCursorTime() / 1000.0f));
		}
		return qtrue;
	}
	if (demoLength && cgs.cursorX >= DEMOBAR_X + 4 && cgs.cursorX <= DEMOBAR_X + DEMOBAR_W - 4 &&
		cgs.cursorY >= DEMOBAR_LINE - 9 && cgs.cursorY <= DEMOBAR_LINE + 9) {
		demoDragging = qtrue;
		return qtrue;
	}
	for (i = 0; i < demoButtonCount; i++) {
		if (cgs.cursorX >= demoButtons[i].x && cgs.cursorX <= demoButtons[i].x + demoButtons[i].w &&
			cgs.cursorY >= demoButtons[i].y && cgs.cursorY <= demoButtons[i].y + demoButtons[i].h) {
			trap_SendConsoleCommand(demoButtons[i].cmd);
			return qtrue;
		}
	}
	return qtrue;
}

static void CG_DrawDemoKeys(void) {
	static const struct {
		const char *key, *what;
	} keys[] = {
		{"SPACE", "Pause, or play again"},
		{"RIGHT", "One frame on, or faster while it plays"},
		{"LEFT", "Slower"},
		{"DOWN", "Back to normal speed"},
		{"PGUP", "Ten seconds on"},
		{"PGDN", "Ten seconds back"},
		{"HOME", "Back to the start"},
		{"F", "To just before the next frag"},
		{"C", "The free camera"},
		{"X", "The X-ray"},
		{"DEL", "Hides the controls"},
		{"F1", "This list"},
		{"MOUSE", "Drags the playhead and works the buttons"},
	};
	const float x = 150.0f;
	const float w = 340.0f;
	const float step = 13.0f;
	float y = 60.0f;
	int i;

	CG_SetWidescreen(WIDESCREEN_CENTER);
	CG_FillRect(x, y - 14.0f, w, step * ARRAY_LEN(keys) + 22.0f, demoBack);
	CG_DrawText(x + 10.0f, y - 2.0f, 1, 0.22f, colorWhite, "DEMO KEYS", 0, 0, 3);
	y += 12.0f;
	for (i = 0; i < (int)ARRAY_LEN(keys); i++) {
		CG_DrawText(x + 10.0f, y, 1, 0.18f, colorYellow, (char *)keys[i].key, 0, 0, 3);
		CG_DrawText(x + 80.0f, y, 1, 0.18f, colorWhite, (char *)keys[i].what, 0, 0, 3);
		y += step;
	}
	CG_SetWidescreen(WIDESCREEN_STRETCH);
}

static void CG_DrawDemoHUD(void) {
	static int		running;
	char			buf[MAX_STRING_CHARS], speed[16];
	int				length, now, minutes, i, n;
	float			seconds, x, w, step;
	qboolean		frozen;
	struct {
		const char	*label, *key, *cmd;
		qboolean	on;
	} row[8];

	if (trap_Cvar_VariableValue("cl_freezeDemo") <= 0) {
		running++;
		frozen = (running < 4) ? qtrue : qfalse;
	} else {
		running = 0;
		frozen = qtrue;
	}

	trap_Cvar_VariableStringBuffer("cl_demoLength", buf, sizeof(buf));
	length = atoi(buf);
	trap_Cvar_VariableStringBuffer("cl_demoPos", buf, sizeof(buf));
	now = Com_Clamp(0, length, atoi(buf));
	demoLength = length;
	demoButtonCount = 0;

	CG_SetWidescreen(WIDESCREEN_CENTER);

	CG_FillRect(DEMOBAR_X, DEMOBAR_Y, DEMOBAR_W, DEMOBAR_H, demoBack);

	seconds = now / 1000.0f;
	minutes = (int)seconds / 60;
	seconds -= minutes * 60.0f;
	Com_sprintf(buf, sizeof(buf), "%d:%s%.03f", minutes, seconds < 10.0f ? "0" : "", seconds);
	CG_DrawText(320 - CG_DrawTextWidth(buf, 0.3f, 0, 2) / 2, DEMOBAR_Y + 16, 2, 0.3f, colorWhite,
		buf, 0, 0, 3);

	if (length) {
		CG_DemoTimeString(length, buf, sizeof(buf));
		CG_DrawText(DEMOBAR_X + DEMOBAR_W - 10 - CG_DrawTextWidth(buf, 0.15f, 0, 1), DEMOBAR_Y + 15,
			1, 0.15f, colorWhite, buf, 0, 0, 3);

		CG_FillRect(DEMOBAR_X + 10, DEMOBAR_LINE, DEMOBAR_SPAN, 3, demoTrack);
		x = (float)DEMOBAR_SPAN * now / length;
		CG_FillRect(DEMOBAR_X + 10, DEMOBAR_LINE, x, 3, colorWhite);
		CG_DrawDemoMarks("cl_demoMarks", length, DEMOBAR_LINE - 2, 1, 7, demoRestart);
		CG_DrawDemoMarks("cl_demoFrags", length, DEMOBAR_LINE + 4, 2, 3, colorRed);
		CG_FillRect(DEMOBAR_X + 10 + x - 1, DEMOBAR_LINE - 4, 2, 11, colorYellow);

		if (demoDragging) {
			x = (float)DEMOBAR_SPAN * CG_DemoCursorTime() / length;
			CG_FillRect(DEMOBAR_X + 10 + x - 1, DEMOBAR_LINE - 4, 2, 11, demoGhost);
			CG_DemoTimeString(CG_DemoCursorTime(), buf, sizeof(buf));
			CG_DrawText(DEMOBAR_X + 10 + x - CG_DrawTextWidth(buf, 0.15f, 0, 1) / 2, DEMOBAR_LINE - 6,
				1, 0.15f, colorWhite, buf, 0, 0, 3);
		}
	}

	n = 0;
	row[n].label = "Start", row[n].key = "[HOME]", row[n].cmd = "seekdemo 0\n", row[n++].on = qfalse;
	row[n].label = "-10s", row[n].key = "[PGDN]", row[n].cmd = "rewind 10\n", row[n++].on = qfalse;
	row[n].label = frozen ? "Play" : "Pause", row[n].key = "[SPACE]", row[n].cmd = "toggle cl_freezeDemo\n",
		row[n++].on = qfalse;
	if (frozen) {
		row[n].label = "Advance", row[n].key = "[RIGHT]",
			row[n].cmd = "timescale 1; cl_freezeDemo 0; wait; wait; cl_freezeDemo 1\n", row[n++].on = qfalse;
	}
	row[n].label = "+10s", row[n].key = "[PGUP]", row[n].cmd = "fastforward 10\n", row[n++].on = qfalse;
	row[n].label = "Frag", row[n].key = "[F]", row[n].cmd = "fragforward\n", row[n++].on = qfalse;

	step = frozen ? 45.0f : 50.0f;
	for (i = 0; i < n; i++) {
		CG_DrawDemoButton(320 + (i - (n - 1) / 2.0f) * step, DEMOBAR_Y + 40, row[i].label, row[i].key,
			row[i].on, row[i].cmd);
	}

	Com_sprintf(speed, sizeof(speed), "%0.1fx", cg_timescale.value);
	n = 0;
	row[n].label = "-", row[n].key = "[LEFT]", row[n].cmd = "cvarAdd timescale -0.1\n", row[n++].on = qfalse;
	row[n].label = speed, row[n].key = "", row[n].cmd = NULL, row[n++].on = cg_timescale.value != 1.0f;
	row[n].label = "+", row[n].key = "[RIGHT]", row[n].cmd = "cvarAdd timescale 0.1\n", row[n++].on = qfalse;
	row[n].label = "1.0x", row[n].key = "[DOWN]", row[n].cmd = "timescale 1\n", row[n++].on = qfalse;
	row[n].label = "Cam", row[n].key = "[C]", row[n].cmd = "toggle cg_freecam\n",
		row[n++].on = cg_freecam.integer ? qtrue : qfalse;
	row[n].label = "X-ray", row[n].key = "[X]", row[n].cmd = "toggle cg_xray\n",
		row[n++].on = cg_xray.integer ? qtrue : qfalse;
	row[n].label = "Hide", row[n].key = "[DEL]", row[n].cmd = "toggle cg_drawDemoHUD\n", row[n++].on = qfalse;
	row[n].label = "Keys", row[n].key = "[F1]", row[n].cmd = "toggle cg_demoKeys\n",
		row[n++].on = cg_demoKeys.integer ? qtrue : qfalse;

	w = 34.0f * n + 8;
	CG_FillRect(320 - w / 2, DEMOBAR_Y - 30, w, 26, demoBack);
	for (i = 0; i < n; i++) {
		CG_DrawDemoButton(320 - w / 2 + 4 + 17 + 34 * i, DEMOBAR_Y - 17, row[i].label, row[i].key,
			row[i].on, row[i].cmd);
	}

	CG_SetWidescreen(WIDESCREEN_STRETCH);

	if (cg_demoKeys.integer) {
		CG_DrawDemoKeys();
	}
}

static void CG_Draw2D(stereoFrame_t stereoFrame) {
	static qboolean tutorialStarted;

	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
	}

	if (cg.time > cg.lastChatTime) {
		cg.lastChatTime = 0;
	}
	if (cg.time > cg.lastChatTime2) {
		cg.lastChatTime2 = 0;
	}

	if (cg.levelShot) {
		return;
	}

	if (cg_draw2D.integer == 0) {
		return;
	}

	if (cg.snap->ps.pm_type == PM_INTERMISSION) {
		CG_DrawIntermission();
		return;
	}

	CG_SetWidescreen(WIDESCREEN_STRETCH);
	if (!cg.demoPlayback || !(int)trap_Cvar_VariableValue("cl_freezeDemo")) {
		CG_DrawFloatingEffects();
	}
	CG_DrawTeammateNames();

	if (g_training.integer) {
		CG_DrawLetterbox();
		if (cg.snap->ps.pm_type == PM_TUTORIAL) {
			CG_DrawChat(5, 287, 634, 120, 1.0f);
			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawCenterString();
			CG_DrawWarmupTutorial();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
			if (cg_ignoreMouseInput.integer != 1) {
				trap_Cvar_Set("cg_ignoreMouseInput", Menus_AnyVisible() ? "0" : "1");
			}
			if (tutorialStarted != qtrue) {
				tutorialStarted = qtrue;
				letterboxTime = 0;
				letterboxOpening = qtrue;
				letterboxDuration = 1000;
			}
			return;
		}
		if (cg_ignoreMouseInput.integer) {
			trap_Cvar_Set("cg_ignoreMouseInput", "0");
		}
		if (tutorialStarted) {
			tutorialStarted = qfalse;
		}
	}

	if (cg.showJoinMenu && !cg.scoreBoardShowing) {
		CG_UpdateJoinMenu();
	}
	else if ((cg.predictedPlayerState.eFlags & EF_SPECTATOR) ||
		cg.predictedPlayerState.pm_type == PM_SPECTATOR) {
		CG_DrawSpectatorHud();

		if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
			if (stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();

			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawCrosshairNames();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
			CG_DrawWeaponBar();
			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawReward();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
		}
		CG_SetWidescreen(WIDESCREEN_LEFT);
		CG_DrawSpecItemTimers();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
		CG_DrawSpecTeamVitals();
	} else {
		if (!cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0) {
			if (cg_drawStatus.integer) {
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}

			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawAmmoWarning();
			CG_DrawProxWarning();
			CG_SetWidescreen(WIDESCREEN_STRETCH);

			if (stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();

			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawCrosshairNames();
			CG_DrawWeaponBar();
			CG_DrawReward();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
		}
	}

	CG_SetWidescreen(WIDESCREEN_LEFT);
	CG_DrawVote();
	CG_DrawVoiceIndicator();
	CG_SetWidescreen(WIDESCREEN_RIGHT);
	CG_DrawTeamVote();
	CG_SetWidescreen(WIDESCREEN_RIGHT);
	CG_DrawLagometer();
	CG_SetWidescreen(WIDESCREEN_CENTER);
	CG_DrawInputCmds();
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	if (!cg_paused.integer) {
		CG_SetWidescreen(WIDESCREEN_RIGHT);
		CG_DrawUpperRight(stereoFrame);
		if (cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 2) {
			CG_DrawTeamOverlay(408, qtrue, qfalse);
		}
		CG_SetWidescreen(WIDESCREEN_LEFT);
		CG_DrawLowerLeft();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}

	CG_SetWidescreen(WIDESCREEN_CENTER);
	CG_DrawWarmup();
	if (cgs.freezeEnd || cgs.pauseEnd) {
		CG_DrawTimeout();
	}
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	cg.scoreBoardShowing = CG_DrawScoreboard();
	if (!cg.scoreBoardShowing) {
		CG_DrawAccuracyPanel();
	}
	if (!cg.scoreBoardShowing && !cg_paused.integer && !cg.showJoinMenu) {
		CG_SetWidescreen(WIDESCREEN_CENTER);
		CG_DrawCenterString();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}

	if (cg.demoPlayback && cg_drawDemoHUD.integer) {
		CG_DrawDemoHUD();
	}
}

static void CG_DrawFullScreenColor(void) {
	if (cg_vignette.integer) {
		CG_DrawPic(0, 0, 640, 480, cgs.media.vignetteShader);
	}
}

void CG_DrawActive(stereoFrame_t stereoView) {
	if (!cg.snap) {
		CG_SetWidescreen(WIDESCREEN_STRETCH);
		CG_DrawInformation();
		return;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		(cg.snap->ps.pm_flags & PMF_HOOKED)) {
		CG_DrawTourneyScoreboard();
		return;
	}

	if (((cg.predictedPlayerState.pm_type == PM_DEAD ||
	      cg.predictedPlayerState.pm_type == PM_FREEZE) && cg_zoomOutOnDeath.integer) ||
	    cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
		cg.zoomed = qfalse;
	}

	CG_TileClear();

	if (stereoView != STEREO_CENTER)
		CG_DrawCrosshair3D();

	trap_R_BeginBloomScene();
	trap_R_RenderScene(&cg.refdef);
	trap_R_ApplyBloom();

	CG_Draw2D(stereoView);

	if (!cg.renderingThirdPerson) {
		CG_DrawFullScreenColor();
	}
}
