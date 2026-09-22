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
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"

#include "../ui/ui_shared.h"

// used for scoreboard
extern displayContextDef_t cgDC;
menuDef_t* menuScoreboard = NULL;
menuDef_t* menuEndScoreboard = NULL;
// [QL] cgamex86.dll caches this one at menu-load time next to stats_menu and the
// spectator huds (0x10006ecc, stored at 0x10ab9680). It lives in ui/intro.menu,
// which cg_main.c already parses - id put the join menu in the intro file.
menuDef_t* menuJoinGame = NULL;

// [QL] the spectator HUD menus, cached where the binary caches them (0x10006eb8). Retail
// looks up only the comp_ variants: "spechud_menu" and "specfollowhud_menu" are nowhere in
// it, though both files are parsed.
//
//   0x10ab9684 comp_spechud_menu        0x10ab968c comp_specred_top
//   0x10ab9688 comp_specfollowhud_menu  0x10ab9690 comp_specred_bottom
//   0x10ab9694 comp_specblue_top        0x10ab9698 comp_specblue_bottom
menuDef_t* menuSpecHud = NULL;
menuDef_t* menuSpecFollowHud = NULL;
menuDef_t* menuSpecRedTop = NULL;
menuDef_t* menuSpecRedBottom = NULL;
menuDef_t* menuSpecBlueTop = NULL;
menuDef_t* menuSpecBlueBottom = NULL;

/*
=================
CG_CacheMenus

[QL] 0x10006eb8. Resolves every menu pointer cgame keeps, after the initial load and after
loadhud re-parses. Menus_Reset only zeroes menuCount, so a re-parse fills the same slots
with different menus and a stale pointer paints the wrong one.
=================
*/
void CG_CacheMenus(void) {
    // [QL] the scoreboards first (0x10006eb3, 0x1002917b), then the rest; CG_NewClientInfo
    // resolves them again for the local player. The fork only did that, and a player who
    // stayed a spectator never had a scoreboard to raise.
    CG_SetEndScoreboardMenu();
    menuJoinGame = Menus_FindByName("joingame_menu");
    menuSpecHud = Menus_FindByName("comp_spechud_menu");
    menuSpecFollowHud = Menus_FindByName("comp_specfollowhud_menu");
    menuSpecRedTop = Menus_FindByName("comp_specred_top");
    menuSpecRedBottom = Menus_FindByName("comp_specred_bottom");
    menuSpecBlueTop = Menus_FindByName("comp_specblue_top");
    menuSpecBlueBottom = Menus_FindByName("comp_specblue_bottom");
}

/*
=================
CG_DrawSpectatorHud

[QL] 0x1000dad0. The spectator overlay: a main HUD plus four team banners. PMF_FOLLOW picks
which main HUD (0x1000dae1); the banners follow unconditionally. Every paint is forced,
because these menus are never activated and so never carry WINDOW_VISIBLE.
=================
*/
void CG_DrawSpectatorHud(void) {
    menuDef_t* menu;

    // [QL] 0x1000dad3: the scoreboard wins the frame outright.
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

    // [QL] 0x1000db4d: the spectator menus carry no chat window, so the overlay
    // draws one itself in the rect the player HUD's chatWindow uses.
    if (!cg.scoreBoardShowing) {
        CG_DrawChat(5, 287, 634, 120, 1.0f);
    }
}

int sortedTeamPlayers[TEAM_MAXOVERLAY];
int numSortedTeamPlayers;

char systemChat[256];
char teamChat1[256];
char teamChat2[256];

/*
=================
CG_KeyNameForCommand

Resolves a command binding (e.g. "+attack") to its display key name (e.g. "MOUSE1").
If no key is bound, writes the fallback string "???" into buf.
=================
*/
void CG_KeyNameForCommand(const char* command, char* buf, int buflen) {
	int key;

	key = trap_Key_GetKey(command);
	if (key >= 0) {
		trap_Key_KeynumToStringBuf(key, buf, buflen);
	} else {
		Q_strncpyz(buf, "???", buflen);
	}
}

// [QL] Text is rendered by the engine glyph atlas (fontstash/stb_truetype) via
// trap_R_Font_DrawString / trap_R_Font_TextExtents.  The legacy per-glyph
// fontInfo_t path (FreeType) is gone; these functions convert the game's
// 640x480 virtual coordinates and text scale into the screen-pixel coordinates
// and pixel font size the engine expects, matching the QL binary.

// Font index convention (QL engine / menudef): 0 = FONT_DEFAULT (handelgothic,
// "normal"), 1 = FONT_SANS (notosans), 2 = FONT_MONO (droidsansmono).
static int CG_EngineFont(int fontIndex) {
	if (fontIndex >= 0 && fontIndex < 3)
		return fontIndex;
	return 0;
}

// Map a legacy fontInfo_t* back to a font index for the _Font variants.
static int CG_IndexForFontPtr(const fontInfo_t* font) {
	int i;
	for (i = 0; i < 3; i++) {
		if (font == &cgDC.Assets.extraFonts[i])
			return i;
	}
	return 0;
}

// Pixel font size for a text scale (QL: screenFontScale = (vidHeight/768)*96).
static float CG_FontPixelSize(float scale) {
	return (((float)cgs.glconfig.vidHeight / 768.0f) * 96.0f) * scale;
}

// Horizontal scale mapping 640-space x to screen pixels, honouring widescreen.
static float CG_TextXScale(void) {
	int ws = cg_currentWidescreen;
	if (cgs.widescreenBias > 0.0f && (ws != 0 || (trap_Key_GetCatcher() & KEYCATCH_CGAME)))
		return (cgs.glconfig.vidHeight * 4.0f / 3.0f) / 640.0f;
	return cgs.screenXScale;
}

// Convert 640x480 virtual coords to screen pixels with widescreen bias
// (matches QL cg_drawtools.c CG_Text_Paint).
static void CG_TextToScreen(float* x, float* y) {
	float xscale = CG_TextXScale();
	float xbias = 0.0f;
	int ws = cg_currentWidescreen;
	if (cgs.widescreenBias > 0.0f && (ws != 0 || (trap_Key_GetCatcher() & KEYCATCH_CGAME))) {
		if (ws == 2 || (trap_Key_GetCatcher() & KEYCATCH_CGAME))
			xbias = cgs.widescreenBias;
		else if (ws == 3)
			xbias = cgs.widescreenBias * 2.0f;
	}
	if (x)
		*x = *x * xscale + xbias;
	if (y)
		*y = *y * cgs.screenYScale;
}

// Core painter: virtual coords + scale -> screen pixels, engine glyph atlas.
// A shadow style draws the whole string offset in solid black first.
// [QL] cgamex86.dll 0x100084b3: the offset is 1 virtual unit for shadowed and 2 for
// shadowed more, added before the scaling to screen pixels, so the shadow grows with
// the resolution (4.5 pixels at 1080p for shadowed more). Black at the text's alpha,
// with colour codes ignored (flag 1 in retail, TEXT_NORECOLOR here).
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

// Text in an engine font, as trap_R_Font_Register returns, rather than a menu font index.
void CG_DrawTextInFont(float x, float y, int engineFont, float scale, const vec4_t color, const char* text, int style) {
	CG_PaintTextEngine(x, y, engineFont, scale, color, text, 0, style);
}

// 640-space width and ascender of text in an engine font, truncated as CG_MeasureText does.
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

// Core measure: 640-space width/height for the given text.
// [QL] cgamex86.dll 0x100082b0: width is the engine box's maxx - minx, height is
// the font's ascender (extents[4]), both kept as floats until the final
// division by the 640-space scale.
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

// [QL] Font-pointer variants - used by DC wrappers.
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

// [QL] DC wrapper functions - pass the font index straight to the engine.
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

// [QL] CG_DrawText - matching binary's 0x10008440 in cgamex86.dll. Renders text
// through the engine glyph atlas at screen coordinates with a full-string shadow
// pass; the widescreen coordinate transform lives in CG_PaintText/CG_TextToScreen.
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

// [QL] Height counterpart of CG_DrawTextWidth. cgamex86.dll's measuring routine
// (0x100082b0) hands back width and height through two out pointers; the numeric
// ownerdraws use the height to drop the text onto the rect (y + height).
int CG_DrawTextHeight(const char *text, float scale, int limit, int fontIndex) {
	int h = 0;
	CG_MeasureText(text, scale, fontIndex, limit, NULL, &h);
	return h;
}

// [QL] cgamex86.dll 0x100087c0: text that stops at *maxX (640 space), font 0,
// no text style. The engine skips the first glyph that would end past the bound
// and writes back where it stopped; a string that fits leaves *maxX unchanged,
// which is how the place ownerdraws tell a cut name from a whole one. Retail
// scales the bound to pixels but, unlike x, never adds the widescreen bias to
// it; kept as is.
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

// Scale-based versions - used by ~90 direct call sites in cg_newdraw.c, cg_info.c,
// etc.  These use the default font (index 0), matching QL's CG_Text_Paint.
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
	// QL: CG_Text_Paint uses the default font (index 0 = handelgothic).
	CG_PaintText(x, y, 0, scale, color, text, limit, style);
}

/*
================
CG_Draw3DModel

[QL] 0x100089f0: a colour, when there is one, goes to the model's shaderRGBA; that is how the
attacker's head wears the team or enemy head colour.
================
*/
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
	ent.renderfx = RF_NOSHADOW;  // no stencil shadows
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

/*
================
CG_DrawHead

Used for both the status bar and the scoreboard

[QL] 0x10009490: the head takes an optional colour (CG_Draw3DModel). Before the head, retail
shows the player's Steam profile picture when cg_drawProfileImages is on and there is one; the
fork has no Steam, so there never is.
================
*/
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

		// offset the origin y and z to center the head
		trap_R_ModelBounds(cm, mins, maxs);

		origin[2] = -0.5 * (mins[2] + maxs[2]);
		origin[1] = 0.5 * (mins[1] + maxs[1]);

		// calculate distance so the head nearly fills the box
		// assume heads are taller than wide
		len = 0.7 * (maxs[2] - mins[2]);
		origin[0] = len / 0.268;  // len / tan( fov/2 )

		// allow per-model tweaking
		VectorAdd(origin, ci->headOffset, origin);

		CG_Draw3DModel(x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles, color);
	} else if (cg_drawIcons.integer) {
		CG_DrawPic(x, y, w, h, ci->modelIcon);
	}

	// if they are deferred, draw a cross out
	if (ci->deferred) {
		CG_DrawPic(x, y, w, h, cgs.media.deferShader);
	}
}

/*
================
CG_DrawFlagModel

Used for both the status bar and the scoreboard
================
*/
void CG_DrawFlagModel(float x, float y, float w, float h, int team, qboolean force2D) {
	qhandle_t cm;
	float len;
	vec3_t origin, angles;
	vec3_t mins, maxs;
	qhandle_t handle;

	if (!force2D && cg_draw3dIcons.integer) {
		VectorClear(angles);

		cm = cgs.media.redFlagModel;

		// offset the origin y and z to center the flag
		trap_R_ModelBounds(cm, mins, maxs);

		origin[2] = -0.5 * (mins[2] + maxs[2]);
		origin[1] = 0.5 * (mins[1] + maxs[1]);

		// calculate distance so the flag nearly fills the box
		// assume heads are taller than wide
		len = 0.5 * (maxs[2] - mins[2]);
		origin[0] = len / 0.268;  // len / tan( fov/2 )

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

/*
================
CG_DrawTeamBackground

================
*/
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

/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

/*
================
CG_DrawAttacker

================
*/
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

	// [QL] 0x10009943: retail keeps attackerTime once the head has gone; only a map restart
	// clears it
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

	// [QL] 0x10009998: a 36-unit head in the top-right corner, tinted with the team head colour
	// for a teammate and the enemy one otherwise, and under it the name, right-aligned 10 units
	// in, in font 1 at scale 0.18 with no shadow.
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

/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot(float y) {
	char* s;
	int w;

	s = va("time:%i snap:%i cmd:%i", cg.snap->serverTime,
		   cg.latestSnapshotNum, cgs.serverCommandSequence);
	// [QL] 0x10009b10: right-aligned on 635 in font 0, scale 0.25, shadowed, with its
	// baseline 16 below the whole-unit y + 2
	w = CG_DrawTextWidth(s, 0.25f, 0, 0);

	CG_DrawText((float)(635 - w), (float)(int)(y + 2) + 16, 0, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
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

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t = trap_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if (index > FPS_FRAMES) {
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for (i = 0; i < FPS_FRAMES; i++) {
			total += previousTimes[i];
		}
		if (!total) {
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;
		// [QL] 0x10009c97: within 1 of com_maxfps it reads com_maxfps
		if (fps - cg_comMaxfps.integer <= 1 && cg_comMaxfps.integer - fps <= 1) {
			fps = cg_comMaxfps.integer;
		}

		// [QL] 0x10009cc2: right-aligned on 635 in font 2 (DroidSansMono), scale 0.25,
		// shadowed, its baseline a text height below y
		s = va("%ifps", fps);
		w = CG_DrawTextWidth(s, 0.25f, 0, 2);
		h = CG_DrawTextHeight(s, 0.25f, 0, 2);

		CG_DrawText((float)(635 - w), y + h, 2, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	return y + h;
}

/*
=================
CG_DrawTimer
=================
*/
static float CG_DrawTimer(float y) {
	char* s;
	int w;
	int mins, seconds, tens;
	int msec;

	msec = cg.time - cgs.levelStartTime;

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	s = va("%i:%i%i", mins, tens, seconds);
	w = CG_Text_Width(s, 0.3f, 0);

	CG_Text_Paint(635 - w, y + 14, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
=================
CG_DrawTeamOverlay

[QL] 0x10009da0. Up to eight teammates from the server's sorted list: name, health and armor,
weapon and location. Every measure comes from the height of "O" in font 0. A dead teammate
reads "DEAD" and a frozen one "FROZEN" with no weapon. Returns where the next element goes.
=================
*/
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
	// a spectator, whom EF_SPECTATOR marks in QL, has no overlay
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
		// every text in the row sits on the name's height
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

		// the powerups run outwards from the anchored edge; flight and the freeze are left out
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

/*
=====================
CG_DrawSpeedometer

[QL] Draws the speedometer bar graph and text.
Modes: 1 = icon+bars+text (right), 2 = bars+text (center), 3 = text only (center)
Mode 4 is handled by the menu ownerdraw (cg_newdraw.c), not here.
=====================
*/
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

	// set positions based on mode
	if (mode < 2) {
		// mode 1: right-aligned, icon + bars + text
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
		// modes 2-3: centered
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

	// adjust coordinates to screen space
	CG_AdjustFrom640(&rectX, &rectY, &rectW, &rectH);

	halfHeight = rectH * 0.5f;
	barScale = (rectH - 5.0f) / ((float)baseSpeed * 3.0f);
	barWidth = rectW / (float)SPEED_HISTORY_SIZE;

	// mode 1: draw background (lagometer shader, same as binary)
	if (mode < 2) {
		trap_R_SetColor(NULL);
		CG_DrawPic((float)graphX, (float)graphY, 48.0f, 48.0f, cgs.media.lagometerShader);
	}

	// modes 1-2: draw bar graph
	if (mode < 3 && cg.speedHistoryCount > 0) {
		idx = cg.speedHistoryIndex + 1;
		if (idx > SPEED_HISTORY_SIZE - 1) idx = 0;

		for (i = 0; i < cg.speedHistoryCount; i++) {
			speed = cg.speedHistory[idx];
			if (speed > 0.0f) {
				barHeight = barScale * speed;
				if (barHeight > halfHeight) barHeight = halfHeight;

				// lower half bar (green)
				trap_R_SetColor(cg.speedBarColor1);
				barX = rectX + barWidth * (float)i;
				barY = (rectY + rectH) - barHeight;
				trap_R_DrawStretchPic(barX, barY, barWidth, barHeight,
									  0, 0, 0, 0, cgs.media.whiteShader);

				// overflow bar (yellow) - when speed exceeds half
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

	// text: draw current speed value below the bar area
	{
		int speedInt = (int)cg.speedHistory[cg.speedHistoryIndex];
		int textX, textY;
		float tw;

		text = va("%d", speedInt);
		tw = CG_Text_Width(text, 0.175f, 0);

		if (mode < 2) {
			// mode 1: right-aligned, text below 48px bar area
			CG_SetWidescreen(WIDESCREEN_RIGHT);
			textX = graphX + 2;
			textY = graphY + 8;
		} else {
			// modes 2-3: centered text below 32px bar area
			textX = 320 - (int)(tw / 2);
			textY = graphY + 32 + 1;
		}

		CG_Text_Paint((float)textX, (float)textY,
					  0.15f, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	trap_R_SetColor(NULL);
	CG_SetWidescreen(WIDESCREEN_STRETCH);
}

/*
=====================
CG_DrawUpperRight

=====================
*/
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
	if (cg_drawTimer.integer) {
		y = CG_DrawTimer(y);
	}
	if (cg_drawAttacker.integer) {
		CG_DrawAttacker(y);
	}
	// [QL] speedometer - bar graph + text drawn for modes 1-3
	// (mode 4 is text-only via menu ownerdraw, not drawn here)
	if (cg_speedometer.integer > 0 && cg_speedometer.integer < 4) {
		CG_DrawSpeedometer();
	}
}

/*
===========================================================================================

  LOWER LEFT CORNER

===========================================================================================
*/

/*
===================
CG_DrawPickupItem

[QL] 0x1000a800. The last item picked up, for 3 s and fading over the last 200 ms: the
pickup time, the icon, the name and, from the second of the same item, "x%i". Each bit is a
flag in cg_drawItemPickups.
[CPMA] ch_animateScroll fades it in over 150 ms and out on CPMA's curve; the 3 s are retail's.
===================
*/
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

/*
=====================
CG_DrawLowerLeft

[QL] cgamex86.dll 0x1000abf0. From y 408: the team overlay when cg_drawTeamOverlay is 3 in a
team mode, then the pickup display from the overlay's top. The cvar's range stops at 2, so the
overlay never takes this corner.
=====================
*/
static void CG_DrawLowerLeft(void) {
	float y = 408;

	if (cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 3) {
		y = CG_DrawTeamOverlay(y, qfalse, qfalse);
	}
	CG_DrawPickupItem((int)y);
}

/*
===========================================================================================

  LOWER RIGHT CORNER

===========================================================================================
*/

/*
===================
CG_DrawRewardIcon

[CPMA] ch_animateRewardIcons: CPMA 1.53 plays 24 frames of 40 ms for assist, capture,
defend, excellent, gauntlet and impressive, stopping on the last (1) or looping (2)
(cgame.qvm func 165605). The frames are CPMA's, shipped in pak01; without them, and for
every other medal, the icon stays still.
===================
*/
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

/*
===================
CG_DrawReward

[QL] 0x1000ac30. A medal shows for 2 s and fades over its last 200 ms. The next one is
popped and its sound queued before cg_drawRewards is tested, so hidden medals still play.
[ioquakelive] A deliberate divergence: 0.74 of retail's size, centred on 325 with its top
at y 128, so no message runs into another. Both of retail's branches are kept.
===================
*/
#define REWARD_ICON_SCALE 0.74f
#define REWARD_ICON_SIZE (32.0f * REWARD_ICON_SCALE)
#define REWARD_ICON_STEP (48.0f * REWARD_ICON_SCALE)
#define REWARD_ROW_CENTER 325.0f
#define REWARD_ICON_X (REWARD_ROW_CENTER - REWARD_ICON_STEP / 2.0f)
#define REWARD_ICON_Y 128.0f
// the count sits where retail puts it, at the same fraction of the icon: retail's icon is at
// (296, 42) and its count at (336, 66) in scale 0.2
#define REWARD_COUNT_X (REWARD_ICON_X + 40.0f * REWARD_ICON_SCALE)
#define REWARD_COUNT_Y (REWARD_ICON_Y + 24.0f * REWARD_ICON_SCALE)
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
		// [CPMA] RewardIcons in CPMA 1.53's hud.cfg: fadedelay 1800, time 200
		color[3] = CG_CpmaFade(cg.rewardTime, 0, REWARD_TIME - 200, 200);
	} else {
		color[3] = (remaining < 200) ? remaining / 200.0f : 1.0f;
	}
	trap_R_SetColor(color);
	CG_DrawRewardIcon(REWARD_ICON_X, REWARD_ICON_Y, REWARD_ICON_SIZE, cg.rewardShader[0]);
	// [QL] 0x1000ac96, deliberately half ported: past cg_drawRewardsRowSize medals at once, retail
	// drops to a single icon with the count written beside it, and below that it lays them side by
	// side. Only the count is wanted here, so the cvar is simply where it starts showing: 1 by
	// default, which is from the second medal on, and 9 never writes it.
	if (cg.rewardCount[0] > cg_drawRewardsRowSize.integer) {
		CG_DrawText(REWARD_COUNT_X, REWARD_COUNT_Y, 0, REWARD_COUNT_SCALE, color,
					va("%d", cg.rewardCount[0]), 0, 0, 0);
	}
	trap_R_SetColor(NULL);
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_SAMPLES 128

typedef struct {
	int frameSamples[LAG_SAMPLES];
	int frameCount;
	int snapshotFlags[LAG_SAMPLES];
	int snapshotSamples[LAG_SAMPLES];
	int snapshotCount;
} lagometer_t;

lagometer_t lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo(void) {
	int offset;

	offset = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = offset;
	lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo(snapshot_t* snap) {
	// dropped packet
	if (!snap) {
		lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->ping;
	lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->snapFlags;
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect(void) {
	float x, y;
	int cmdNum;
	usercmd_t cmd;
	const char* s;
	int w;

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd(cmdNum, &cmd);
	if (cmd.serverTime <= cg.snap->ps.commandTime || cmd.serverTime > cg.time) {  // special check for map_restart
		return;
	}

	// [QL] 0x1000aef0: nothing in the first 3 seconds of the level
	if (cg.time - cgs.levelStartTime < 3000) {
		return;
	}

	// also add text in center of screen: font 0, scale 0.25, shadowed, baseline at 116
	s = "Connection Interrupted";
	w = CG_DrawTextWidth(s, 0.25f, 0, 0);
	CG_DrawText((float)(320 - w / 2), 116, 0, 0.25f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

	// blink the icon
	if ((cg.time >> 9) & 1) {
		return;
	}

	// [QL] in the bottom-right corner
	x = SCREEN_WIDTH - 48;
	y = SCREEN_HEIGHT - 48;

	CG_DrawPic(x, y, 48, 48, trap_R_RegisterShader("gfx/2d/net.jpg"));
}

/*
==============
CG_DrawInputCmds

[QL] cgamex86.dll 0x1000b610. Arrows for the movement being pressed around cg_drawInputCmdsX
and cg_drawInputCmdsY, cg_drawInputCmdsSize apart: forward and back above and below, strafe to
the sides, jump and crouch up and down to the right, each 16 units square. A free-floating
spectator gets nothing, and at 2 only a spectator following somebody. The movement is this
client's last command, or the followed player's (and a demo's) from the playerState.
==============
*/
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

/*
==============
CG_DrawLagometer
==============
*/
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

	//
	// draw the graph
	//
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

	// draw the frame interpoalte / extrapolate graph
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

	// draw the snapshot latency / drop graph
	range = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for (a = 0; a < aw; a++) {
		i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if (v > 0) {
			if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED) {
				if (color != 5) {
					color = 5;  // YELLOW for rate delay
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
				color = 4;  // RED for dropped snapshots
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_RED)]);
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	trap_R_SetColor(NULL);

	// [QL] 0x1000b53f: font 0, scale 0.25, shadowed, 16 below the graph's top
	if (cg_nopredict.integer) {
		CG_DrawText(x, y + 16, 0, 0.25f, colorWhite, "snc", 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	// [QL] 0x1000b547: with cg_lagometer 2, the last ping a snapshot brought (a 0 keeps the one
	// before) at x 592 and y 337 plus its height measured at 0.175, in font 0 at 0.15, white and
	// shadowed
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

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint(const char* str, int y, float scale) {
	char* s;

	// [QL] cgamex86.dll 0x1000be10: a text scale, not a character width, and
	// race prints sit 10 units lower.
	Q_strncpyz(cg.centerPrint, str, sizeof(cg.centerPrint));

	cg.centerPrintTime = cg.time;
	cg.centerPrintFrag = qfalse;
	cg.centerPrintY = y + (cgs.gametype == GT_RACE ? 10 : 0);
	cg.centerPrintScale = scale;

	// count the number of lines for centering
	cg.centerPrintLines = 1;
	s = cg.centerPrint;
	while (*s) {
		if (*s == '\n')
			cg.centerPrintLines++;
		s++;
	}
}

/*
===================
CG_DrawCenterString

[QL] cgamex86.dll 0x1000beb0. Visible for cg_centertime seconds, fading in the
last 200 ms. The block starts scale * 16 * lines above the print's y; each
line (up to 50 characters) is centred on the 640 screen, drawn at y + its
height in font 0 with the print's scale and ITEM_TEXTSTYLE_SHADOWEDMORE, and
the next line starts 6 units below it.
===================
*/
// [ioquakelive] The frag message, deliberately laid out differently from retail: smaller and
// lower, under the medal, in Handel Gothic Ex from pak01 (cgs.media.fragMessageFont).
#define FRAG_MESSAGE_Y 165.0f
#define FRAG_MESSAGE_SCALE 0.15f

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
		// [CPMA] FragMessage and RankMessage in CPMA 1.53's hud.cfg: fadein 150,
		// fadedelay 1200, time 200
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
		y = FRAG_MESSAGE_Y;
		scale = FRAG_MESSAGE_SCALE;
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

/*
================================================================================

CROSSHAIR

================================================================================
*/

// [QL] cgamex86.dll 0x10075fe0: the crosshair colours for cg_crosshairColor and
// cg_crosshairHitColor 1-26; any other value takes the first
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

/*
=================
CG_CrosshairHitEffect

[QL] cgamex86.dll 0x1000c0a0. For cg_crosshairHitTime ms after a hit: styles 1, 4 and 7
colour the crosshair by hit type, 2, 5 and 8 with cg_crosshairHitColor; 3-5 grow it by
the hit type and 6-8 by one. The growth follows the elapsed fraction and snaps back at
the end. The colour replaces the base one, brightness not applied.
=================
*/
static void CG_CrosshairHitEffect(float* w, float* h) {
	static const vec4_t typeColors[4] = {
		{0.25f, 0.5f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
	};
	int style = cg_crosshairHitStyle.integer;
	int type = cg.crosshairHitType;
	float elapsed = (float)(cg.time - cg.crosshairHitTime);
	float t, grow = 0.0f;
	int colorBy = 0;  // 1 = hit type, 2 = cg_crosshairHitColor

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

/*
=================
CG_CrosshairCPMAPulse

[CPMA] ch_crosshairPulse 1-3: after an item pickup (200 ms), enemy damage or an
enemy frag (225 ms) the crosshair jumps to twice its size and eases back to it
with a cubic ease-out.
=================
*/
static void CG_CrosshairCPMAPulse(float* w, float* h) {
	static const float durations[3] = {200.0f, 225.0f, 225.0f};
	int mode = ch_crosshairPulse.integer;
	int eventTime = (mode == 1) ? cg.itemPickupBlendTime : (mode == 2) ? cg.crosshairHitTime : cg.crosshairFragTime;
	float elapsed = (float)(cg.time - eventTime);
	float t;

	if (!eventTime || elapsed <= 0.0f || elapsed >= durations[mode - 1]) {
		return;
	}
	t = 1.0f - elapsed / durations[mode - 1];
	*w *= 1.0f + t * t * t;
	*h *= 1.0f + t * t * t;
}

/*
=================
CG_DrawCrosshair

[QL] cgamex86.dll 0x1000c2a0. Nothing for cg_drawCrosshair <= 0 or a multiple of 30, or
in third person. The base colour is cg_crosshairColor from the palette, or the health
colour, times cg_crosshairBrightness. After an item pickup the cg_crosshairPulse growth
runs, otherwise the hit effect. Size and offset scale uniformly by vidWidth / 640, so
the crosshair stays square.
=================
*/
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

	if (ch_crosshairPulse.integer >= 1 && ch_crosshairPulse.integer <= 3) {
		CG_CrosshairCPMAPulse(&w, &h);
		if (cg.itemPickupBlendTime <= cg.crosshairHitTime) {
			CG_CrosshairHitEffect(&w, &h);
		}
	} else if (cg.itemPickupBlendTime > cg.crosshairHitTime) {
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

/*
=================
CG_DrawCrosshair3D
=================
*/
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

	// pulse the size of the crosshair when picking up items
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

	// Use a different method rendering the crosshair so players don't see two of them when
	// focusing their eyes at distant objects with high stereo separation
	// We are going to trace to the next shootable object and place the crosshair in front of it.

	// first get all the important renderer information
	trap_Cvar_VariableStringBuffer("r_zProj", rendererinfos, sizeof(rendererinfos));
	zProj = atof(rendererinfos);
	trap_Cvar_VariableStringBuffer("r_stereoSeparation", rendererinfos, sizeof(rendererinfos));
	stereoSep = zProj / atof(rendererinfos);

	xmax = zProj * tan(cg.refdef.fov_x * M_PI / 360.0f);

	// let the trace run through until a change in stereo separation of the crosshair becomes less than one pixel.
	maxdist = cgs.glconfig.vidWidth * stereoSep * zProj / (2 * xmax);
	VectorMA(cg.refdef.vieworg, maxdist, cg.refdef.viewaxis[0], endpos);
	CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, endpos, 0, MASK_SHOT);

	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_DEPTHHACK | RF_CROSSHAIR;

	VectorCopy(trace.endpos, ent.origin);

	// scale the crosshair so it appears the same size for all distances
	ent.radius = w / 640 * xmax * trace.fraction * maxdist / zProj;
	ent.customShader = hShader;

	trap_R_AddRefEntityToScene(&ent);
}

/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity(void) {
	trace_t trace;
	vec3_t start, end;
	int content;

	VectorCopy(cg.refdef.vieworg, start);
	VectorMA(start, 131072, cg.refdef.viewaxis[0], end);

	CG_Trace(&trace, start, vec3_origin, vec3_origin, end,
			 cg.snap->ps.clientNum, CONTENTS_SOLID | CONTENTS_BODY);
	if (trace.entityNum >= MAX_CLIENTS) {
		return;
	}

	// if the player is in fog, don't show it
	content = CG_PointContents(trace.endpos, 0);
	if (content & CONTENTS_FOG) {
		return;
	}

	// if the player is invisible, don't show it
	if (cg_entities[trace.entityNum].currentState.powerups & (1 << PW_INVIS)) {
		return;
	}

	// update the fade timer
	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}

/*
=====================
CG_CrosshairTeamVitalColor

[QL] cgamex86.dll 0x1000c7e9 and 0x1000c979: at cg_drawCrosshairTeamHealth 2, the tint of a
teammate's health or armor under the crosshair against the viewer's maximum health, red to yellow
below it and yellow to white up to twice it. At 1 the colour is left as it is.
=====================
*/
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

/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames(void) {
	float* color;
	char* name;
	float opacity;
	qboolean teammate;
	centity_t* cent;
	float size, y;
	char text[16];

	// [QL] cgamex86.dll 0x1000c670: the name under the crosshair fades over a second,
	// centred at y 190 in font 0, scale 0.26, shadowed. Enemies need cg_enemyCrosshairNames
	// and take cg_enemyCrosshairNamesOpacity; teammates need cg_teammateCrosshairNames
	// without cg_teammateNames (which names them overhead) and take
	// cg_teammateCrosshairNamesOpacity. Under a teammate's name cg_drawCrosshairTeamHealth
	// adds the health and armor.
	if (!cg_drawCrosshair.integer) {
		return;
	}
	if (cg.renderingThirdPerson) {
		return;
	}

	// scan the known entities to see if the crosshair is sighted on one
	CG_ScanForCrosshairEntity();

	// draw the name of the player being looked at
	color = CG_FadeColor(cg.crosshairClientTime, 1000);
	if (!color) {
		trap_R_SetColor(NULL);
		return;
	}

	teammate = cgs.gametype >= GT_TEAM &&
			   cg.snap->ps.persistant[PERS_TEAM] == cgs.clientinfo[cg.crosshairClientNum].team;
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

	// [QL] 0x1000c7c4: "health / armor" under a teammate's name in the name's alpha, at
	// cg_drawCrosshairTeamHealthSize, the health ending and the armor starting at the middle
	if (cg_drawCrosshairTeamHealth.value != 0.0f && teammate) {
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

/*
=====================
CG_OverheadTeamColor

[QL] cgamex86.dll 0x10012b50: the name colour of each team
=====================
*/
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

/*
=====================
CG_OverheadTextWidth

[QL] 0x100082b0 as retail measures: in 640 units, with the 4:3 scale whenever the screen is
wider than 4:3, whatever the widescreen mode
=====================
*/
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

/*
=====================
CG_DrawTeammateNames

[QL] 0x1000cb60. A box over each living player in front of the view, 40 units up and seen
through walls: the name, four health segments tinted like the HUD health (0x1000ca90), an
armor line, and the powerups or a teammate marker. cg_teammateNames shows it at 1 only near
the middle of the screen and at 2 always; cg_specNames does the same for a spectator.
=====================
*/
static void CG_DrawTeammateNames(void) {
	static const vec4_t boxColor = {0.0f, 0.0f, 0.0f, 0.5f};
	static const vec4_t armorColor = {0.0f, 0.625f, 1.0f, 1.0f};
	int i, j, len, count, boxWidth, nameWidth, nameHeight, xoff, armorWidth;
	int health, armor, maxHealth, localTeam;
	qboolean sameTeam, spectator, frozen, showName, showBars;
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

	if (cg.predictedPlayerState.pm_type == PM_SPECTATOR && !(cg.predictedPlayerState.pm_flags & PMF_FOLLOW) &&
		!cg_specNames.integer) {
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

		// the name without colours or control characters, cut to cg_overheadNamesWidth
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
		if (spectator && cgs.gametype >= GT_TEAM) {
			textColor = CG_OverheadTeamColor(ci->team);
		}

		// window position, 0-1 from the left and from the bottom
		CG_ProjectFloatingEffect(point, &sx, &sy);
		nx = sx / 640.0f;
		ny = 1.0f - sy / 480.0f;

		if (spectator && cg_specNames.integer == 2) {
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
			showBars = (sameTeam && cg_teammateNames.integer) || (spectator && cg_specNames.integer);
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

//==============================================================================

/*
=================
CG_DrawSpectator

[QL] 0x10034d70, reached only through the CG_SPEC_MESSAGES ownerdraw and nothing without
cg_drawSpecMessages. A player out of a CA or A&D round gets "Round In Progress"; otherwise
the spectator mode pair, unless the competitive follow HUD covers them (0x10034fbd), which
is where the duel and team-mode lines go instead. The texts are fixed, with no key names.
=================
*/
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

/*
=================
CG_DrawVote
=================
*/
static void CG_DrawVote(void) {
	char* s;
	int sec;

	// [QL] team-kill complaint prompt (binary CG_DrawVote 0x1000dc00). Only in team
	// gametypes (>= GT_TEAM, excluding RR); replaces the vote line while pending.
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
		// negative status codes (binary strings, incl. the "Comlaints" typo)
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

	// play a talk beep whenever it is modified
	if (cgs.voteModified) {
		cgs.voteModified = qfalse;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	// [QL] Vote countdown: binary uses (voteTime - cg.time + 30000) / 1000
	sec = (cgs.voteTime - cg.time + VOTE_TIME) / 1000;
	if (sec < 0) {
		sec = 0;
	}
	// [QL] Show key bindings in vote text, Y=300/312 (binary-verified)
	s = va("VOTE(%is):%s yes:%i no:%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo);
	CG_Text_Paint(4, 300, 0.22f, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	{
		char menuKey[32];
		CG_KeyNameForCommand("togglemenu", menuKey, sizeof(menuKey));
		s = va("or press %s then click Vote", menuKey);
	}
	CG_Text_Paint(8, 312, 0.22f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

/*
=================
CG_DrawTeamVote
=================
*/
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

	// play a talk beep whenever it is modified
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

// [QL] Set both in-game and end-of-game scoreboard menus by gametype
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

/*
=================
CG_StartMenuCursor

For a cgame menu about to take the keys. CG_Init memsets cgs, so the cursor sits at
(0,0) until the player moves the mouse, and every click before that hit-tests
outside the menu. Retail's engine hands cgame an absolute pointer position
(0x1003c4f0); ioquake3's sends deltas, so the starting point is chosen here.
=================
*/
static void CG_StartMenuCursor(void) {
	if (cgs.cursorX == 0 && cgs.cursorY == 0) {
		cgs.cursorX = SCREEN_WIDTH / 2;
		cgs.cursorY = SCREEN_HEIGHT / 2;
		cgDC.cursorx = cgs.cursorX;
		cgDC.cursory = cgs.cursorY;
	}
}

/*
=================
CG_DrawMenuCursor

The pointer over a cgame menu that holds the keys. Retail's engine draws it; this one does not,
and nothing in cgame read cgs.activeCursor, so the pointer was invisible on every cgame-owned
menu. Drawn with the same 16-pixel centring the UI module uses (ui_main.c:524) so the two look
alike. CG_DrawPic runs the coordinates through CG_AdjustFrom640, the mapping the menu itself
just used, so the arrow lands where the cursor actually is.
=================
*/
static void CG_DrawMenuCursor(void) {
	CG_DrawPic(cgs.cursorX - 16, cgs.cursorY - 16, 32, 32,
	           cgs.activeCursor ? cgs.activeCursor : cgs.media.selectCursor);
}

// [QL] the scoreboard menus CG_DrawScoreboard closes by name (0x10068fd8..0x100691f4)
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

/*
=================
CG_DrawScoreboard

[QL] 0x1000dfd0. Up unless the game is paused, on +scores, a dead player or the intermission
(0x1000dff6, 0x1000e00b). There is no Quake 3 fade: it goes the frame the condition stops.

The intermission board is activated rather than only painted (0x1000e02e), which is what lets
the mouse reach its tabs and the end-of-match vote, and the keys go back when it drops.
=================
*/
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
		// [QL] 0x1000dff6 - dying during warmup still raises the board
		if (!cg.showScores && pmType != PM_DEAD) {
			showing = qfalse;
		}
	} else {
		// [QL] 0x1000e00b
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

		if (menu == menuEndScoreboard && (trap_Key_GetCatcher() & KEYCATCH_CGAME)) {
			CG_DrawMenuCursor();
		}
	}

	// load any models that have been deferred
	if (++cg.deferredPlayerLoading > 10) {
		CG_LoadDeferredPlayers();
	}

	if (showing) {
		return qtrue;
	}

	// [QL] 0x1000e170
	cg.deferredPlayerLoading = 0;
	firstTime = qtrue;
	if (cg.scoreBoardShowing) {
		// the two generic pairs first, then the in-game boards, then the end boards
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

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission(void) {
	cg.scoreFadeTime = cg.time;
	cg.scoreBoardShowing = CG_DrawScoreboard();
}

/*
=================
CG_DrawAmmoWarning
=================
*/
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
	// [QL] 0x1000e390: cg_drawAmmoWarning 1 is scale 0.35 at y 55, 2 (the default) scale
	// 0.24 at y 52; font 0, centred, shadowed more
	scale = (cg_drawAmmoWarning.integer == 1) ? 0.35f : 0.24f;
	y = (cg_drawAmmoWarning.integer == 1) ? 55.0f : 52.0f;
	CG_DrawText((float)(320 - CG_DrawTextWidth(s, scale, 0, 0) / 2), y, 0, scale, colorWhite, s, 0, 0,
				ITEM_TEXTSTYLE_SHADOWEDMORE);
}

/*
=================
CG_DrawProxWarning
=================
*/
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

	// [QL] 0x1000e480: red, centred at y 112 in font 0, scale 0.25, shadowed
	CG_DrawText((float)(320 - CG_DrawTextWidth(s, 0.25f, 0, 0) / 2), 112, 0, 0.25f, colorRed, s, 0, 0,
				ITEM_TEXTSTYLE_SHADOWED);
}

// [QL] a warmup line, centred on 320 in whole units as retail's 320 - width / 2
static void CG_DrawWarmupLine(float y, int font, float scale, vec4_t color, const char* text, int style) {
	int w = CG_DrawTextWidth(text, scale, 0, font);

	CG_DrawText((float)(320 - w / 2), y, font, scale, color, text, 0, 0, style);
}

// [QL] the key bound to readyup, as the warmup messages name it ("<KEY NOT FOUND>" unbound)
static void CG_ReadyUpKeyName(char* buf, int size) {
	trap_Key_KeynumToStringBuf(trap_Key_GetKey("readyup"), buf, size);
}

/*
=================
CG_DrawWarmupTutorial

[QL] cgamex86.dll 0x1000e600: a training match before the player readies up. Without
g_skipTrainingEnable only the g_training 1 match shows it: "This match will determine" and
"your skill placement" at y 35 and 55 (scale 0.35) over "Press <key> to start the match"
at y 85. With it, only "Press <key> to skip the tutorial" at y 30. The key line is red at
scale 0.25; every line is font 0, centred, with ITEM_TEXTSTYLE_SHADOWEDMORE.
=================
*/
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

/*
=================
CG_DrawADRoundResults

[QL] 0x10011a90, over the count before an Attack & Defend round: a row per team and a column
per round, with the attacking team's row tinted. Past the tenth round the columns scroll to
keep the current one last. Near the score limit it adds a line saying whose match point it
is, or that the round is a last chance.
=================
*/
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

/*
=================
CG_DrawWarmupCountdown

[QL] 0x1000ec00. The count before a match or a round, under a header that names what is
starting: the duel pairing, the rule in force, whose turn it is in Attack & Defend, "Round
Begins in" once a round has been played, "Sudden Death" in overtime, or the gametype. Rounds
count bare; everything else says "Starts in: %i".
=================
*/
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

/*
=================
CG_DrawWarmup

[QL] 0x1000f090, and nothing in practice mode. The count ticks while a warmup time is set
and once more after it clears, with the counting sounds for 1 to 3. Before the match it says
what it is waiting for -- players, balance, the ready countdown -- and tells a player in the
match which key readies them.
=================
*/
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

	// [QL] 0x1000f1b0: not over the join menu (0x10a9c9ac) or last frame's scoreboard
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

//==================================================================================
/*
=================
CG_DrawTimedMenus
=================
*/
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

/*
=================
CG_PlayFightSound

[QL] cgamex86.dll 0x1000ea40. The start of play, with its voice and centre print: "GO!" in Race;
in A&D "ATTACK THE FLAG!" or "DEFEND THE FLAG!" by the player's team and the turn; in Red Rover
the respawn chime after the first round and "BITE!" for the infected; "FIGHT!" otherwise. The
announcer queue starts over.
=================
*/
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

/*
=================
CG_PlayCountdownSound

[QL] cgamex86.dll 0x1000eb30. The voice for a countdown second: 3, 2 and 1, "fight" at 0 in the
round modes, and at 5 "prepare to fight" while the match is paused or "round begins in" in
the round modes (in Freeze Tag only with a round delay of at least 5 s).
=================
*/
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

/*
=================
CG_DrawTimeout

[QL] cgamex86.dll 0x1000ef30, drawn while a pause is on or its resume time is still set. The
round clock stands still, a new pause sounds the klaxon, and once a second the countdown
voice for the seconds left plays. Shows "Match Paused", or "Match resuming in ^5N^7
seconds" while the resume countdown runs.
=================
*/
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
		CG_Text_Paint(320 - w / 2, 128, 0.5f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);  // [QL] 0x1000f04e
	}

	if (cg.time - cgs.pauseSoundTime >= 1000) {
		cgs.pauseSoundTime = cg.time;
		CG_PlayCountdownSound(sec);
	}
}

/*
=================
CG_Draw2D
=================
*/
/*
=================
CG_UpdateJoinMenu

[QL] cgamex86.dll 0x1000e9c0, single caller in CG_Draw2D at 0x10010fdd.

Holds the join menu open while the player is a spectator, and tears it down
the moment they are on a team. Quake 3 had none of this - it put you straight
into spectator and left the team menu to a key bind.
=================
*/
void CG_UpdateJoinMenu(void) {
	menuDef_t* menu;

	if (menuJoinGame == NULL) {
		menuJoinGame = Menus_FindByName("joingame_menu");
	}
	menu = menuJoinGame;
	if (menu == NULL) {
		return;
	}

	// [QL] 0x1000e9d0: once Escape has hidden the menu (event 5) it stays down while the
	// in-game menu keeps the game paused, and comes back when the pause ends
	if (!cg_paused.integer) {
		if (cgs.eventHandling == CGAME_EVENT_ESCAPE) {
			cgs.eventHandling = CGAME_EVENT_NONE;
		}
	} else if (cgs.eventHandling == CGAME_EVENT_ESCAPE) {
		return;
	}

	// [QL] 0x1000e9f2: open it on the first frame it is asked for, and take the
	// key catcher so clicks reach CG_KeyEvent/CG_MouseEvent instead of the game.
	if (!(menu->window.flags & WINDOW_VISIBLE)) {
		CG_StartMenuCursor();
		trap_Key_SetCatcher(KEYCATCH_CGAME);
		Menus_Activate(menu);
	}

	// [QL] 0x1000ea20: cmp dword ptr [cg.snap + 0x138], 3 - once the player is
	// no longer a spectator the menu closes itself and the HUD comes back.
	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		CG_CloseMenus();
		return;
	}

	// [QL] 0x1000ea30: Menu_Paint(menuJoinGame, qtrue). Forced, because the
	// normal Menu_PaintAll pass is skipped on this path.
	Menu_Paint(menu, qtrue);
	CG_DrawMenuCursor();
}

/*
=====================
CG_SpecTeamColor

[QL] cgamex86.dll 0x10012b10: the strong colour of each team, for the spectator item timers and
team vitals
=====================
*/
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

/*
=====================
CG_DrawSpecItemTimers

[QL] 0x1000f700, for a spectator (EF_SPECTATOR) with cg_specItemTimers, a bit mask
of what to show: 1 powerups, 2 mega health, 4 red armor, 8 yellow armor. Down from
cg_specItemTimersX and Y, each item's icon, cg_specItemTimersSize * 100 square,
with the seconds until it is back beside it. In CTF a band at a quarter opacity
marks the base an item belongs to, and a random-respawn team's items go half size.
=====================
*/
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

/*
=====================
CG_DrawSpecTeamVitals

[QL] 0x1000fab0, for a spectator in the team modes with cg_specTeamVitals: a row per
player from cg_specTeamVitalsY, cg_specTeamVitalsWidth wide, red down the left and
blue down the right. A row is the client number, the first powerup or else the
model's portrait where retail draws the weapon in use, the name cut with "..." to
fit, and a health and an armor bar against the player's maximum health.

Without cg_specTeamVitalsHealthColor the health bar fills the row behind white
text; with it the bars go thin under the name, the health one tinted by health and
the armor one cyan, and the text takes the team colour.
=====================
*/
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
			// the model's portrait where retail draws the weapon in use; the weapon stays for a player
			// whose portrait has not loaded
			CG_DrawPic(x, y - 16.0f, 16, 16,
					   ci->modelIcon ? ci->modelIcon : cg_weapons[cent->currentState.weapon].weaponIcon);
		}
		if (gradient) {
			*yp = y - step;
		}

		// the name without colours, cut with "..." to what is left of the width
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

/*
=================
CG_DrawLetterbox

[QL] 0x1000d940, drawn by CG_Draw2D in a training match (0x10010ed3): two black
bands 640 wide at the top and the bottom. The first frame the player is held for the
tour (PM_TUTORIAL) opens them from 1 to 84 units over 1000 ms; once pm_type leaves
PM_TUTORIAL they close again over another 1000 ms. cg_letterboxDrawn says whether
they were drawn this frame, and CG_AddViewWeapon (0x10052d33) leaves the gun out
while they are.
=================
*/
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

/*
=================
CG_DrawVoiceIndicator

[QL] 0x10011200. One row per talking player, with a box in their team's colour, the
icon and the name. cg_voiceChatIndicator turns it off at 0 and sends it left at 2.
This also closes the "is talking" flag: the binary clears it 500 ms after the last
packet, except your own, which "-voice" clears.
=================
*/
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

        // [QL] 0x1001143a: the flag clears itself half a second after the last packet;
        // your own does not, "-voice" clears that one
        if (ci->talkingTime + 500 < cg.time && i != cg.clientNum) {
            ci->talking = qfalse;
        }
    }

    CG_SetWidescreen(0);
}

/*
===============================================================================

DEMO CONTROLS

[QL] Retail draws its demo controls last in CG_Draw2D (cgamex86.dll 0x10011412) from
0x10010450: a half-black box with the time on top and a row of buttons, each label in white
over its key in yellow. ioquakelive draws them the same way and adds a timeline, the jumps
and the free camera, in two boxes: the playing one in the middle, and the speed and the
toggles apart over its right end. Retail's own box is in HALLAZGOS.md.

===============================================================================
*/

#define DEMOBAR_X	140		// the playing box, centred in the 4:3
#define DEMOBAR_W	360
#define DEMOBAR_Y	318
#define DEMOBAR_H	56

static const vec4_t demoBack = { 0, 0, 0, 0.5f };		// retail's 0x10076344
static const vec4_t demoTrack = { 1, 1, 1, 0.25f };

// a retail button: the label, yellow when it is on, over its key in yellow, both centred on x
static void CG_DrawDemoButton(float x, float y, const char *label, const char *key, qboolean on) {
	CG_DrawText(x - CG_DrawTextWidth(label, 0.25f, 0, 1) / 2, y, 1, 0.25f,
		on ? colorYellow : colorWhite, (char *)label, 0, 0, 3);
	CG_DrawText(x - CG_DrawTextWidth(key, 0.15f, 0, 1) / 2, y + 10, 1, 0.15f, colorYellow,
		(char *)key, 0, 0, 3);
}

/*
=================
CG_DrawDemoHUD

Frozen, the middle button is Play and Advance joins it, as in retail; running, Pause.
Advance thaws cl_freezeDemo for two frames, so it counts as frozen for the first three running
frames instead of flickering. The time is written as retail's 0x10001320 writes it. The
timeline comes from the engine, in demo time, which runs on when a server starts its clock over:
cl_demoPos, cl_demoLength and the match restarts in cl_demoMarks.
=================
*/
static void CG_DrawDemoHUD(void) {
	static int		running;
	char			buf[MAX_STRING_CHARS], speed[16];
	const char		*p;
	int				length, now, minutes, i, n;
	float			seconds, x, w, span = DEMOBAR_W - 20;
	qboolean		frozen;
	struct {
		const char	*label, *key;
		qboolean	on;
	} options[7];

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

	CG_SetWidescreen(WIDESCREEN_CENTER);

	// the playing box: the time, the timeline and the buttons
	CG_FillRect(DEMOBAR_X, DEMOBAR_Y, DEMOBAR_W, DEMOBAR_H, demoBack);

	seconds = now / 1000.0f;
	minutes = (int)seconds / 60;
	seconds -= minutes * 60.0f;
	Com_sprintf(buf, sizeof(buf), "%d:%s%.03f", minutes, seconds < 10.0f ? "0" : "", seconds);
	CG_DrawText(320 - CG_DrawTextWidth(buf, 0.3f, 0, 2) / 2, DEMOBAR_Y + 16, 2, 0.3f, colorWhite,
		buf, 0, 0, 3);

	if (length) {
		Com_sprintf(buf, sizeof(buf), "%d:%02d", length / 60000, length / 1000 % 60);
		CG_DrawText(DEMOBAR_X + DEMOBAR_W - 10 - CG_DrawTextWidth(buf, 0.15f, 0, 1), DEMOBAR_Y + 15,
			1, 0.15f, colorWhite, buf, 0, 0, 3);

		CG_FillRect(DEMOBAR_X + 10, DEMOBAR_Y + 21, span, 3, demoTrack);
		CG_FillRect(DEMOBAR_X + 10, DEMOBAR_Y + 21, span * now / length, 3, colorWhite);
		trap_Cvar_VariableStringBuffer("cl_demoMarks", buf, sizeof(buf));
		for (p = buf; *p; ) {
			x = (float)atoi(p);
			if (x > 0 && x < length) {
				CG_FillRect(DEMOBAR_X + 10 + span * x / length - 0.5f, DEMOBAR_Y + 19, 1, 7, colorYellow);
			}
			while (*p && *p != ' ') {
				p++;
			}
			while (*p == ' ') {
				p++;
			}
		}
	}

	if (frozen) {
		CG_DrawDemoButton(230, DEMOBAR_Y + 40, "Start", "[HOME]", qfalse);
		CG_DrawDemoButton(275, DEMOBAR_Y + 40, "-10s", "[PGDN]", qfalse);
		CG_DrawDemoButton(320, DEMOBAR_Y + 40, "Play", "[SPACE]", qfalse);
		CG_DrawDemoButton(365, DEMOBAR_Y + 40, "Advance", "[RIGHT]", qfalse);
		CG_DrawDemoButton(410, DEMOBAR_Y + 40, "+10s", "[PGUP]", qfalse);
	} else {
		CG_DrawDemoButton(245, DEMOBAR_Y + 40, "Start", "[HOME]", qfalse);
		CG_DrawDemoButton(295, DEMOBAR_Y + 40, "-10s", "[PGDN]", qfalse);
		CG_DrawDemoButton(345, DEMOBAR_Y + 40, "Pause", "[SPACE]", qfalse);
		CG_DrawDemoButton(395, DEMOBAR_Y + 40, "+10s", "[PGUP]", qfalse);
	}

	// apart, over the right end of the playing box: the speed and the toggles
	Com_sprintf(speed, sizeof(speed), "%0.1fx", cg_timescale.value);
	n = 0;
	options[n].label = "-", options[n].key = "[LEFT]", options[n++].on = qfalse;
	options[n].label = speed, options[n].key = "", options[n++].on = cg_timescale.value != 1.0f;
	options[n].label = "+", options[n].key = "[RIGHT]", options[n++].on = qfalse;
	options[n].label = "1.0x", options[n].key = "[DOWN]", options[n++].on = qfalse;
	options[n].label = "Cam", options[n].key = "[C]", options[n++].on = cg_freecam.integer ? qtrue : qfalse;
	options[n].label = "X-ray", options[n].key = "[X]", options[n++].on = cg_xray.integer ? qtrue : qfalse;
	options[n].label = "Hide", options[n].key = "[DEL]", options[n++].on = qfalse;

	w = 34.0f * n + 8;
	CG_FillRect(DEMOBAR_X + DEMOBAR_W - w, DEMOBAR_Y - 30, w, 26, demoBack);
	for (i = 0; i < n; i++) {
		CG_DrawDemoButton(DEMOBAR_X + DEMOBAR_W - w + 4 + 17 + 34 * i, DEMOBAR_Y - 17,
			options[i].label, options[i].key, options[i].on);
	}

	CG_SetWidescreen(WIDESCREEN_STRETCH);
}

static void CG_Draw2D(stereoFrame_t stereoFrame) {
	static qboolean tutorialStarted;

	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
	}

	// [QL] 0x10010dd8: the lastmsg recall timers run out here, which ends the
	// CG_SHOW_IF_MSG_PRESENT / CG_SHOW_IF_NOTICE_PRESENT blink
	if (cg.time > cg.lastChatTime) {
		cg.lastChatTime = 0;
	}
	if (cg.time > cg.lastChatTime2) {
		cg.lastChatTime2 = 0;
	}

	// if we are taking a levelshot for the menu, don't draw anything
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

	// [QL] shared floating-effect pool: damage numbers, player outlines, freeze/flag
	// glows and head float-sprites, all projected to screen (binary CG_DrawDamagePlums
	// called from CG_Draw2D @ 0x10010e9e), unless a demo is frozen (cl_freezeDemo); then
	// the boxes over teammates (0x10010ea3)
	CG_SetWidescreen(WIDESCREEN_STRETCH);
	if (!cg.demoPlayback || !(int)trap_Cvar_VariableValue("cl_freezeDemo")) {
		CG_DrawFloatingEffects();
	}
	CG_DrawTeammateNames();

	// [QL] 0x10010ec7: a training match draws the cinema bars. While the player is held for
	// the tour (PM_TUTORIAL) the frame is only the HUD's "chat area" menu, where Crash's lines
	// come in, the centre print and the skip line; nothing else. The mouse is ignored then
	// unless some menu is visible, and the first frame held opens the bars. Out of the hold
	// the mouse comes back.
	if (g_training.integer) {
		CG_DrawLetterbox();
		if (cg.snap->ps.pm_type == PM_TUTORIAL) {
			// [ioquakelive] the chat is drawn directly, not through its menu. Menus_FindByName
			// returns NULL for "chat area" in the cgame -- measured with a probe: menu = 0
			// while a chat line was alive -- so Menu_Paint painted nothing, forced or not, and
			// Crash's lines during the tour ended up in the console only. The rect is the
			// absolute one of chatWindow in ui/hud.menu (menu at x 5, item at y 287, 634x120),
			// the same one the spectator overlay already uses.
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

	/*
		if (cg.cameraMode) {
			return;
		}
	*/
	// [QL] 0x10010fdd, right after the floating effects: while the join menu owns the
	// frame it stands in for the HUD, unless last frame's scoreboard is up. The frame
	// goes on (0x10010ff2 jumps to 0x100110c5), so the vote, the corners, the warmup
	// text and the scoreboard still draw and +scores raises the board over the menu.
	if (cg.showJoinMenu && !cg.scoreBoardShowing) {
		CG_UpdateJoinMenu();
	}
	// [QL] cgamex86.dll 0x10010ff7..0x100110b5. Which branch runs is decided by the
	// predicted playerState, and the test is not Quake 3's:
	//   0x10010ff7  test [cg.predictedPlayerState.eFlags], EF_SPECTATOR
	//   0x10011003  cmp  [cg.predictedPlayerState.pm_type], PM_SPECTATOR
	// Both are what SpectatorClientEndFrame (qagamex86.dll 0x10035470) leaves on a
	// follower - pm_type forced back to PM_SPECTATOR, EF_SPECTATOR on a real spectator -
	// so a spectator following somebody gets this branch too. The old test on
	// persistant[PERS_TEAM] saw the FOLLOWED player's team and missed them.
	else if ((cg.predictedPlayerState.eFlags & EF_SPECTATOR) ||
		cg.predictedPlayerState.pm_type == PM_SPECTATOR) {
		// [QL] 0x10011081: the competitive spectator overlay.
		CG_DrawSpectatorHud();

		// [QL] 0x1001108c: crosshair, crosshair names and weapon bar are only drawn
		// while following somebody - a free-flying spectator has no weapon to aim.
		//
		// Retail does NOT draw the "SPECTATOR MODE / Press MOUSE1..." text from here.
		// CG_DrawSpectator is reached only through the CG_SPEC_MESSAGES ownerdraw
		// (0x1003c05b), which only ui/spectator.menu and ui/spectator_follow.menu use -
		// the two spectator HUDs retail never paints. Drawing it unconditionally here
		// was a visible difference from the retail spectator HUD.
		if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
			if (stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();

			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawCrosshairNames();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
			CG_DrawWeaponBar();
			// [QL] 0x100110aa: the medals, as in the playing branch. The widescreen mode
			// retail sets around the call is not verified; this keeps the playing one.
			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawReward();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
		}
		// [QL] 0x1001111a: the item timers down the left, then the team vitals
		CG_SetWidescreen(WIDESCREEN_LEFT);
		CG_DrawSpecItemTimers();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
		CG_DrawSpecTeamVitals();
	} else {
		// don't draw any status if dead or the scoreboard is being explicitly shown
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
			CG_DrawWeaponBar();  // [QL] handles all modes (Left/Right/Centered/Classic/No)
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
	// [QL] 0x100110f7: the movement arrows, centred
	CG_SetWidescreen(WIDESCREEN_CENTER);
	CG_DrawInputCmds();
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	if (!cg_paused.integer) {
		CG_SetWidescreen(WIDESCREEN_RIGHT);
		CG_DrawUpperRight(stereoFrame);
		// [QL] CG_Draw2D (0x10010d90): the team overlay in the lower right corner, then the
		// lower left corner with the pickup display
		if (cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 2) {
			CG_DrawTeamOverlay(408, qtrue, qfalse);
		}
		CG_SetWidescreen(WIDESCREEN_LEFT);
		CG_DrawLowerLeft();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}

	CG_SetWidescreen(WIDESCREEN_CENTER);
	// [QL] cgamex86.dll 0x10011154..0x1001115a: the warmup/centre messages are drawn
	// unconditionally. Quake 3's `if (!CG_DrawFollow()) CG_DrawWarmup();` is gone with
	// CG_DrawFollow itself: retail has no "following" string at all, and the followed
	// player's name is drawn by the spectator HUD through CG_FOLLOW_PLAYER_NAME_EX.
	CG_DrawWarmup();
	if (cgs.freezeEnd || cgs.pauseEnd) {
		CG_DrawTimeout();  // [QL] 0x10011165
	}
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	// [QL] race timer is drawn by menu system via CG_DrawRaceTimes ownerdraw
	// (removed hardcoded duplicate here)

	// [QL] no chat here: retail draws it only through the CG_AREA_NEW_CHAT
	// ownerdraw in the HUD menus and from the spectator overlay (0x1000db98).

	// don't draw center string if scoreboard is up
	// [QL] 0x10011190: nor over the join menu or while paused
	cg.scoreBoardShowing = CG_DrawScoreboard();
	if (!cg.scoreBoardShowing && !cg_paused.integer && !cg.showJoinMenu) {
		CG_SetWidescreen(WIDESCREEN_CENTER);
		CG_DrawCenterString();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}

	// [QL] 0x1001140a: the demo controls go last, over everything
	if (cg.demoPlayback && cg_drawDemoHUD.integer) {
		CG_DrawDemoHUD();
	}
}

/*
=====================
CG_DrawActive

CG_DrawFullScreenColor

QL binary: draws a fullscreen vignette overlay when cg_vignette is enabled (vmCvar 0x10A63960)
=====================
*/
static void CG_DrawFullScreenColor(void) {
	if (cg_vignette.integer) {
		CG_DrawPic(0, 0, 640, 480, cgs.media.vignetteShader);
	}
}

/*
=====================
Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive(stereoFrame_t stereoView) {
	// optionally draw the info screen instead
	if (!cg.snap) {
		CG_SetWidescreen(WIDESCREEN_STRETCH);  // [QL] 0x100114ab
		CG_DrawInformation();
		return;
	}

	// optionally draw the tournement scoreboard instead
	// Quake 3's scoreboard spectator test. In Quake Live bit 0x2000 is PMF_HOOKED, which a
	// spectator never has, so this does not run; not checked against retail's CG_DrawActive.
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		(cg.snap->ps.pm_flags & PMF_HOOKED)) {
		CG_DrawTourneyScoreboard();
		return;
	}

	// QL binary: cg_zoomOutOnDeath.integer resets zoom on death/freeze/intermission (vmCvar 0x10A61200)
	if (((cg.predictedPlayerState.pm_type == PM_DEAD ||
	      cg.predictedPlayerState.pm_type == PM_FREEZE) && cg_zoomOutOnDeath.integer) ||
	    cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
		cg.zoomed = qfalse;
	}

	// clear around the rendered view if sized down
	CG_TileClear();

	if (stereoView != STEREO_CENTER)
		CG_DrawCrosshair3D();

	// draw 3D view
	// [QL] 0x10011490: between the bloom's two calls, so only the view gets the bloom
	trap_R_BeginBloomScene();
	trap_R_RenderScene(&cg.refdef);
	trap_R_ApplyBloom();

	// draw status bar and other floating elements
	CG_Draw2D(stereoView);

	// QL binary: vignette overlay drawn after HUD
	if (!cg.renderingThirdPerson) {
		CG_DrawFullScreenColor();
	}
}
