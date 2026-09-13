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

// [QL] The spectator HUD. cgamex86.dll caches these right after loading the menus
// (0x10006eb8..0x10006f12), in one run of Menus_FindByName calls whose results land in
// consecutive globals. Note that retail looks up ONLY the comp_ variants: the strings
// "spechud_menu" and "specfollowhud_menu" do not appear anywhere in the binary, even
// though ui/spectator.menu and ui/spectator_follow.menu are parsed. Quake Live ships the
// plain pair and uses the competitive one.
//
//   0x10ab9684  comp_spechud_menu          ui/comp_spectator.menu
//   0x10ab9688  comp_specfollowhud_menu    ui/comp_spectator_follow.menu
//   0x10ab968c  comp_specred_top           ui/comp_spectator.menu
//   0x10ab9690  comp_specred_bottom        ui/comp_spectator.menu
//   0x10ab9694  comp_specblue_top          ui/comp_spectator.menu
//   0x10ab9698  comp_specblue_bottom       ui/comp_spectator.menu
//
// (The same run also caches stats_menu into 0x10ab967c. Nothing in this fork opens it
// yet, so it is deliberately left out rather than added as a dead pointer.)
menuDef_t* menuSpecHud = NULL;
menuDef_t* menuSpecFollowHud = NULL;
menuDef_t* menuSpecRedTop = NULL;
menuDef_t* menuSpecRedBottom = NULL;
menuDef_t* menuSpecBlueTop = NULL;
menuDef_t* menuSpecBlueBottom = NULL;

/*
=================
CG_CacheMenus

[QL] Resolves every menu cgame keeps a pointer to. The binary does this in one place,
right after loading the menu files (0x10006eb8), and so do we - both after the initial
load and after `loadhud` re-parses them.

That second call site matters: Menus_Reset only sets menuCount to 0, so a re-parse fills
the same Menus[] slots with different menus and every cached pointer is silently pointing
at the wrong one. menuScoreboard and menuEndScoreboard were already being cleared there;
menuJoinGame was not, which meant a `loadhud` while spectating left CG_UpdateJoinMenu
activating and painting whatever menu had landed in that slot.
=================
*/
void CG_CacheMenus(void) {
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

[QL] cgamex86.dll 0x1000dad0. The spectator overlay: a main HUD plus four team banners.

Which main HUD is chosen comes straight off PMF_FOLLOW (0x1000dae1: the binary tests
ps.pm_flags & 0x1000 at snap+0x38) - following a player gets the follow HUD, free-flying
gets the plain one. The four banners are painted unconditionally afterwards, with no null
test in the binary; Menu_Paint already returns on NULL, so this matches.

Every paint is forced (ecx = 1 at the call to Menu_Paint, 0x100609d0), because these menus
are never activated and so never carry WINDOW_VISIBLE - the normal Menu_PaintAll pass
would skip them. That is the same trick the join menu uses.

The menus themselves are pure ownerdraw: no literal text, 38 distinct ownerdraws between
the two files (player names, health/armor, scores, flag status, clan tags), all of which
this fork already implements. That is why the whole feature is this function plus a cache.
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
static void CG_PaintText(float x, float y, int fontIndex, float scale, const vec4_t color,
                         const char* text, int limit, int style) {
	float sx = x, sy = y;
	float size = CG_FontPixelSize(scale);
	int efont = CG_EngineFont(fontIndex);
	int lim = (limit > 0) ? limit : -1;

	if (!text || !*text)
		return;

	CG_TextToScreen(&sx, &sy);

	if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
		float ofs = (style == ITEM_TEXTSTYLE_SHADOWED) ? 1.0f : 2.0f;
		vec4_t shadow;
		shadow[0] = shadow[1] = shadow[2] = 0.0f;
		shadow[3] = color ? color[3] : 1.0f;
		trap_R_SetColor(shadow);
		trap_R_Font_DrawString((int)(sx + ofs), (int)(sy + ofs), text, efont, size, lim, NULL, TEXT_NORECOLOR);
	}

	trap_R_SetColor(color);
	trap_R_Font_DrawString((int)sx, (int)sy, text, efont, size, lim, NULL, 0);
	trap_R_SetColor(NULL);
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

================
*/
void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles) {
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
================
*/
void CG_DrawHead(float x, float y, float w, float h, int clientNum, vec3_t headAngles) {
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

		CG_Draw3DModel(x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles);
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
		CG_Draw3DModel(x, y, w, h, handle, 0, origin, angles);
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

	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
		return y;
	}

	if (!cg.attackerTime) {
		return y;
	}

	clientNum = cg.predictedPlayerState.persistant[PERS_ATTACKER];
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || clientNum == cg.snap->ps.clientNum) {
		return y;
	}

	if (!cgs.clientinfo[clientNum].infoValid) {
		cg.attackerTime = 0;
		return y;
	}

	t = cg.time - cg.attackerTime;
	if (t > ATTACKER_HEAD_TIME) {
		cg.attackerTime = 0;
		return y;
	}

	size = ICON_SIZE * 1.25;

	angles[PITCH] = 0;
	angles[YAW] = 180;
	angles[ROLL] = 0;
	CG_DrawHead(SCREEN_WIDTH - size, y, size, size, clientNum, angles);

	info = CG_ConfigString(CS_PLAYERS + clientNum);
	name = Info_ValueForKey(info, "n");
	y += size;
	{
		vec4_t halfAlpha = { 1, 1, 1, 0.5f };
		float tw = CG_Text_Width(name, 0.3f, 0);
		CG_Text_Paint(SCREEN_WIDTH - tw, y + 14, 0.3f, halfAlpha, name, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

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
	w = CG_Text_Width(s, 0.3f, 0);

	CG_Text_Paint(635 - w, y + 14, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

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

		s = va("%ifps", fps);
		w = CG_Text_Width(s, 0.3f, 0);

		CG_Text_Paint(635 - w, y + 14, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	return y + BIGCHAR_HEIGHT + 4;
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
=================
*/

static float CG_DrawTeamOverlay(float y, qboolean right, qboolean upper) {
	int x, w, h, xx;
	int i, j, len;
	const char* p;
	vec4_t hcolor;
	int pwidth, lwidth;
	int plyrs;
	char st[16];
	clientInfo_t* ci;
	gitem_t* item;
	int ret_y, count;

	if (!cg_drawTeamOverlay.integer) {
		return y;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_RED && cg.snap->ps.persistant[PERS_TEAM] != TEAM_BLUE) {
		return y;  // Not on any team
	}

	plyrs = 0;

	// max player name width
	pwidth = 0;
	count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
			plyrs++;
			len = CG_DrawStrlen(ci->name);
			if (len > pwidth)
				pwidth = len;
		}
	}

	if (!plyrs)
		return y;

	if (pwidth > TEAM_OVERLAY_MAXNAME_WIDTH)
		pwidth = TEAM_OVERLAY_MAXNAME_WIDTH;

	// max location name width
	lwidth = 0;
	for (i = 1; i < MAX_LOCATIONS; i++) {
		p = CG_ConfigString(CS_LOCATIONS + i);
		if (p && *p) {
			len = CG_DrawStrlen(p);
			if (len > lwidth)
				lwidth = len;
		}
	}

	if (lwidth > TEAM_OVERLAY_MAXLOCATION_WIDTH)
		lwidth = TEAM_OVERLAY_MAXLOCATION_WIDTH;

	w = (pwidth + lwidth + 4 + 7) * TINYCHAR_WIDTH;

	if (right)
		x = 640 - w;
	else
		x = 0;

	h = plyrs * TINYCHAR_HEIGHT;

	if (upper) {
		ret_y = y + h;
	} else {
		y -= h;
		ret_y = y;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
		hcolor[0] = 1.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 0.0f;
		hcolor[3] = 0.33f;
	} else {  // if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE )
		hcolor[0] = 0.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 1.0f;
		hcolor[3] = 0.33f;
	}
	trap_R_SetColor(hcolor);
	CG_DrawPic(x, y, w, h, cgs.media.teamStatusBar);
	trap_R_SetColor(NULL);

	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
			hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

			xx = x + TINYCHAR_WIDTH;

			CG_DrawStringExt(xx, y,
							 ci->name, hcolor, qfalse, qfalse,
							 TINYCHAR_WIDTH, TINYCHAR_HEIGHT, TEAM_OVERLAY_MAXNAME_WIDTH);

			if (lwidth) {
				p = CG_ConfigString(CS_LOCATIONS + ci->location);
				if (!p || !*p)
					p = "unknown";
				//				len = CG_DrawStrlen(p);
				//				if (len > lwidth)
				//					len = lwidth;

				//				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth +
				//					((lwidth/2 - len/2) * TINYCHAR_WIDTH);
				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth;
				CG_DrawStringExt(xx, y,
								 p, hcolor, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
								 TEAM_OVERLAY_MAXLOCATION_WIDTH);
			}

			CG_GetColorForHealth(ci->health, ci->armor, hcolor);

			Com_sprintf(st, sizeof(st), "%3i %3i", ci->health, ci->armor);

			xx = x + TINYCHAR_WIDTH * 3 +
				TINYCHAR_WIDTH * pwidth + TINYCHAR_WIDTH * lwidth;

			CG_DrawStringExt(xx, y,
							 st, hcolor, qfalse, qfalse,
							 TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0);

			// draw weapon icon
			xx += TINYCHAR_WIDTH * 3;

			if (cg_weapons[ci->curWeapon].weaponIcon) {
				CG_DrawPic(xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
						   cg_weapons[ci->curWeapon].weaponIcon);
			} else {
				CG_DrawPic(xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
						   cgs.media.deferShader);
			}

			// Draw powerup icons
			if (right) {
				xx = x;
			} else {
				xx = x + w - TINYCHAR_WIDTH;
			}
			for (j = 0; j <= PW_NUM_POWERUPS; j++) {
				if (ci->powerups & (1 << j)) {
					item = BG_FindItemForPowerup(j);

					if (item) {
						CG_DrawPic(xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
								   trap_R_RegisterShader(item->icon));
						if (right) {
							xx -= TINYCHAR_WIDTH;
						} else {
							xx += TINYCHAR_WIDTH;
						}
					}
				}
			}

			y += TINYCHAR_HEIGHT;
		}
	}

	return ret_y;
	// #endif
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

  LOWER RIGHT CORNER

===========================================================================================
*/

/*
===================
CG_DrawReward

[QL] cgamex86.dll 0x1000ac30. A medal shows for 2 s and fades over its last
200 ms. When it runs out the next one is popped and its sound queued before
cg_drawRewards is tested, so hidden medals still advance and play. The icon is
32x32 at y 42: above cg_drawRewardsRowSize one icon with the count to its right
in plain white, otherwise one icon per medal, 48 apart.
===================
*/
static void CG_DrawReward(void) {
	vec4_t color;
	int i, count, remaining;
	float x;

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
			CG_QueueAnnouncement(cg.rewardSound[0]);
		}
	}

	if (!cg_drawRewards.integer) {
		return;
	}

	color[0] = color[1] = color[2] = 1.0f;
	color[3] = (remaining < 200) ? remaining / 200.0f : 1.0f;
	trap_R_SetColor(color);

	count = cg.rewardCount[0];
	if (count > cg_drawRewardsRowSize.integer) {
		CG_DrawPic(296, 42, 32, 32, cg.rewardShader[0]);
		CG_DrawText(336, 66, 0, 0.2f, colorWhite, va("%d", count), 0, 0, 0);
	} else {
		x = (float)(320 - (count * 48) / 2);
		for (i = 0; i < count; i++) {
			CG_DrawPic(x, 42, 32, 32, cg.rewardShader[0]);
			x += 48;
		}
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

	// also add text in center of screen
	s = "Connection Interrupted";
	w = CG_Text_Width(s, 0.5f, 0);
	CG_Text_Paint(320 - w / 2, 116, 0.5f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	// blink the icon
	if ((cg.time >> 9) & 1) {
		return;
	}

	x = SCREEN_WIDTH - 48;
	y = SCREEN_HEIGHT - 144;

	CG_DrawPic(x, y, 48, 48, trap_R_RegisterShader("gfx/2d/net.jpg"));
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

	if (cg_nopredict.integer) {
		CG_Text_Paint(x, y + 14, 0.3f, colorWhite, "snc", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	// [QL] draw ping number when cg_lagometer == 2
	if (cg_lagometer.integer == 2 && cg.snap) {
		static int lastPing;
		const char* text;
		int textX, textY;
		float tw;

		if (cg.snap->ping) {
			lastPing = cg.snap->ping;
		}
		text = va("%d", lastPing);
		tw = CG_Text_Width(text, 0.175f, 0);
		textX = x + 2;
		textY = y + 8;
		CG_Text_Paint((float)textX, (float)textY,
					  0.15f, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
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
static void CG_DrawCenterString(void) {
	static vec4_t color;
	char* start;
	int l;
	int x, y, w, h;
	int duration, remaining;

	if (!cg.centerPrintTime) {
		return;
	}

	duration = (int)(cg_centertime.value * 1000.0);
	remaining = duration - (cg.time - cg.centerPrintTime);
	if (cg.time - cg.centerPrintTime >= duration) {
		return;
	}

	color[0] = color[1] = color[2] = 1.0f;
	color[3] = (remaining < 200) ? remaining / 200.0f : 1.0f;
	trap_R_SetColor(color);

	start = cg.centerPrint;

	y = (int)((float)cg.centerPrintY - cg.centerPrintScale * 16.0 * cg.centerPrintLines);

	while (1) {
		char linebuffer[1024];

		for (l = 0; l < 50; l++) {
			if (!start[l] || start[l] == '\n') {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = CG_DrawTextWidth(linebuffer, cg.centerPrintScale, 0, 0);
		h = CG_DrawTextHeight(linebuffer, cg.centerPrintScale, 0, 0);
		x = (SCREEN_WIDTH - w) / 2;
		CG_DrawText((float)x, (float)(y + h), 0, cg.centerPrintScale, color, linebuffer, 0, 0,
		            ITEM_TEXTSTYLE_SHADOWEDMORE);
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
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames(void) {
	float* color;
	char* name;
	float w;

	if (!cg_drawCrosshair.integer) {
		return;
	}
	if (!cg_drawCrosshairNames.integer) {
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

	name = cgs.clientinfo[cg.crosshairClientNum].name;
	color[3] *= 0.5f;
	w = CG_Text_Width(name, 0.3f, 0);
	CG_Text_Paint(320 - w / 2, 190, 0.3f, color, name, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	trap_R_SetColor(NULL);
}

//==============================================================================

/*
=================
CG_DrawSpectator

[QL] Rewritten to match binary (cgamex86.dll 0x10034d70).
- CA/AD eliminated players: "Round In Progress" centered at y=60
- Pure spectators: "SPECTATOR MODE" + "Press mouse button 1..." header,
  then left-aligned "waiting to play" (duel) or "press ESC..." (team)
=================
*/
void CG_DrawSpectator(void) {
	float scale, w, y;
	const char* s;
	vec4_t grayColor = {0.73f, 0.73f, 0.73f, 0.7f};
	char attackKey[32], menuKey[32];

	// [QL] CA/AD: show "Round In Progress" for eliminated players (on a team but spectating)
	if ((cgs.gametype == GT_CA || cgs.gametype == GT_AD) &&
		cg.snap->ps.pm_type == PM_SPECTATOR &&
		cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		scale = 0.35f;
		s = "Round In Progress";
		CG_SetWidescreen(WIDESCREEN_CENTER);
		w = CG_Text_Width(s, scale, 0);
		CG_Text_Paint(320 - w / 2, 60, scale, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		return;
	}

	CG_KeyNameForCommand("+attack", attackKey, sizeof(attackKey));
	CG_KeyNameForCommand("togglemenu", menuKey, sizeof(menuKey));

	// "SPECTATOR MODE" header
	scale = 0.22f;
	s = "SPECTATOR MODE";
	w = CG_Text_Width(s, scale, 0);
	CG_Text_Paint(320 - w / 2, 440, scale, colorWhite, s, 0, 0, 0);

	// "Press <key> to cycle through players"
	y = 452;
	s = va("Press %s to cycle through players", attackKey);
	w = CG_Text_Width(s, 0.18f, 0);
	CG_Text_Paint(320 - w / 2, y, 0.18f, grayColor, s, 0, 0, 0);

	// Bottom hints - left-aligned, skipped when the competitive follow HUD is covering
	// this. [QL] 0x10034fbd tests BOTH halves: the cached comp_specfollowhud_menu being
	// non-NULL *and* PMF_FOLLOW, and only then jumps to the epilogue (0x100350aa). Until
	// the spectator menus were cached the first half was always false, so the two
	// conditions were indistinguishable; they are not any more.
	if (!(menuSpecFollowHud && (cg.snap->ps.pm_flags & PMF_FOLLOW))) {
		CG_SetWidescreen(WIDESCREEN_LEFT);
		if (cgs.gametype == GT_DUEL) {
			CG_Text_Paint(20, 461, 0.28f, colorWhite, "waiting to play", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		} else if (cgs.gametype >= GT_TEAM) {
			s = va("press %s and use the JOIN buttons", menuKey);
			CG_Text_Paint(20, 453, 0.28f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
			CG_Text_Paint(20, 470, 0.28f, colorWhite, "to enter the game", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		}
	}
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
CG_DrawScoreboard

[QL] cgamex86.dll 0x1000dfd0. Three differences from the Quake 3 version this
fork inherited, all of them in the decision of whether to draw at all:

1. Quake 3 refuses to draw during warmup unless the player is holding the
   scoreboard key - `if (cg.warmup && !cg.showScores) return qfalse;`. Quake
   Live splits that test in two (0x1000dfe8) and keeps the death case in the
   warmup branch:

     0x1000dff6  warmup:     showScores || pm_type == PM_DEAD
     0x1000e00b  not warmup: showScores || pm_type == PM_DEAD ||
                             pm_type == PM_INTERMISSION

   So in retail the scoreboard comes up when you die during warmup, and in
   this fork it did not - which is most of the time on a freshly started
   server, where warmup runs until enough players join. PM_INTERMISSION is
   only tested outside warmup because the two cannot coexist.

2. Quake 3 keeps the board up for FADE_TIME after the key is released, on
   cg.scoreFadeTime. The binary has no such branch: 0x1000e004 and
   0x1000e01e clear the return value and jump straight to the tail, so the
   board disappears the frame the condition stops holding.

3. With no menu to paint the binary returns false (0x1000e0c8), where this
   fork returned true and drew nothing - which suppressed the centre-print
   string for a scoreboard that was not on screen.

The deferred-player counter is also advanced on the hidden path before being
reset, which is why it is stepped before the early-out rather than after.
=================
*/
static qboolean CG_DrawScoreboard(void) {
	static qboolean firstTime = qtrue;
	menuDef_t *activeMenu;
	qboolean showing;
	int pmType;

	if (menuScoreboard) {
		menuScoreboard->window.flags &= ~WINDOW_FORCED;
	}

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
		if (menuScoreboard == NULL) {
			CG_SetEndScoreboardMenu();
		}

		// [QL] Use end scoreboard during intermission, in-game scoreboard otherwise
		if (pmType == PM_INTERMISSION && menuEndScoreboard) {
			activeMenu = menuEndScoreboard;
		} else {
			activeMenu = menuScoreboard;
		}

		// [QL] 0x1000e0c8: nothing to paint means nothing is showing
		if (activeMenu == NULL) {
			return qfalse;
		}

		if (firstTime) {
			CG_SetScoreSelection(activeMenu);
			firstTime = qfalse;
		}
		Menu_Paint(activeMenu, qtrue);
	}

	// load any models that have been deferred
	if (++cg.deferredPlayerLoading > 10) {
		CG_LoadDeferredPlayers();
	}

	if (!showing) {
		// [QL] 0x1000e170
		cg.deferredPlayerLoading = 0;
		firstTime = qtrue;
		// next time scoreboard comes up, don't print killer
		cg.killerName[0] = 0;
		return qfalse;
	}

	return qtrue;
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
	{
		float tw = CG_Text_Width(s, 0.4f, 0);
		CG_Text_Paint(320 - tw / 2, 78, 0.4f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
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

	{
		float tw = CG_Text_Width(s, 0.4f, 0);
		CG_Text_Paint(320 - tw / 2, 96, 0.4f, g_color_table[ColorIndex(COLOR_RED)], s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
}

/*
=================
CG_DrawWarmupCountdown

[QL] Draw the countdown display during COUNT_DOWN phase.
Binary: FUN_1000ec00 in cgamex86.dll
=================
*/
static void CG_DrawWarmupCountdown(int gt) {
	int w;
	const char* s;
	const char* header = "";
	clientInfo_t* ci1;
	clientInfo_t* ci2;
	int i;

	// Gametype header line
	if (gt == GT_DUEL) {
		// Duel: show "X vs Y"
		ci1 = NULL;
		ci2 = NULL;
		for (i = 0; i < cgs.maxclients; i++) {
			if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE) {
				if (!ci1) {
					ci1 = &cgs.clientinfo[i];
				} else {
					ci2 = &cgs.clientinfo[i];
				}
			}
		}
		if (ci1 && ci2) {
			header = va("%s vs %s", ci1->name, ci2->name);
		}
	} else if (gt >= 0 && gt <= GT_RR) {
		// Round-based gametypes with active rounds: "Round Begins in"
		switch (gt) {
		case GT_CA:
		case GT_FREEZE:
		case GT_AD:
		case GT_RR:
			header = "Round Begins in";
			break;
		default:
			header = gametypeDisplayNames[gt];
			break;
		}
	} else {
		header = "Unknown Gametype";
	}

	w = CG_Text_Width(header, 0.6f, 0);
	CG_Text_Paint(320 - w / 2, 90, 0.6f, colorWhite, header, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	// Countdown number
	switch (gt) {
	case GT_CA:
	case GT_FREEZE:
	case GT_AD:
	case GT_RR:
		// Round-based: bare number
		s = va("%i", cg.warmupCount);
		break;
	default:
		s = va("Starts in: %i", cg.warmupCount);
		break;
	}

	w = CG_Text_Width(s, 0.45f, 0);
	CG_Text_Paint(320 - w / 2, 125, 0.45f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

/*
=================
CG_DrawWarmupMessages

[QL] Full warmup display: countdown tick sounds, PRE_GAME waiting messages,
and countdown display. Replaces the old CG_DrawWarmup.
Binary: CG_DrawWarmupMessages in cgamex86.dll
=================
*/
static void CG_DrawWarmup(void) {
	int sec;
	int i;
	int w;
	int gt;
	int teamCounts[4];
	const char* line1;
	const char* line2;

	// Handle countdown tick and sounds
	if (cg.warmup == 0) {
		if (cg.warmupCount == -1) {
			return;  // no warmup active
		}
		// warmup just ended, fall through to clear
	} else if (cg.warmup == -1) {
		cg.warmupCount = -1;
	} else if (cg.warmup > 0) {
		// Countdown active - calculate seconds remaining
		sec = (cg.warmup - cg.time + 1000) / 1000;
		if (sec < 0) {
			sec = 0;
		}

		if (sec != cg.warmupCount) {
			cg.warmupCount = sec;
			// Play countdown sounds
			switch (sec) {
			case 1:
				trap_S_StartLocalSound(cgs.media.count1Sound, CHAN_ANNOUNCER);
				break;
			case 2:
				trap_S_StartLocalSound(cgs.media.count2Sound, CHAN_ANNOUNCER);
				break;
			case 3:
				trap_S_StartLocalSound(cgs.media.count3Sound, CHAN_ANNOUNCER);
				break;
			}
		}
	}

	if (cg.showScores) {
		return;
	}

	// Determine effective gametype for display
	gt = (cg.warmupGametype >= 0) ? cg.warmupGametype : cgs.gametype;

	// Countdown display
	if (cg.warmupCount > 0) {
		CG_DrawWarmupCountdown(gt);
		return;
	}

	// PRE_GAME display (warmup == -1)
	if (cg.warmup >= 0) {
		return;
	}

	// Count players per team
	memset(teamCounts, 0, sizeof(teamCounts));
	for (i = 0; i < cgs.maxclients; i++) {
		if (cgs.clientinfo[i].infoValid) {
			teamCounts[cgs.clientinfo[i].team]++;
		}
	}

	line1 = NULL;
	line2 = NULL;

	if (gt == GT_RR) {
		// Red Rover
		if (cgs.teamSizeMin > 0 &&
			(teamCounts[TEAM_RED] < cgs.teamSizeMin || teamCounts[TEAM_BLUE] < cgs.teamSizeMin)) {
			line1 = "The match will begin";
			line2 = "when more players join.";
		} else {
			line1 = "The match will begin";
			line2 = "when more players are ready.";
		}
	} else if (gt < GT_TEAM) {
		// FFA / Duel / Race
		if (gt == GT_DUEL && teamCounts[TEAM_FREE] > 2) {
			line1 = "The match will begin when";
			line2 = "fewer players are in the match.";
		} else if (teamCounts[TEAM_FREE] < 2) {
			line1 = "The match will begin";
			line2 = "when more players join.";
		} else {
			line1 = "The match will begin";
			line2 = "when more players are ready.";
		}
	} else {
		// Team games (TDM, CA, CTF, 1FCTF, Overload, Harvester, FT, Dom, AD)
		if (teamCounts[TEAM_RED] < cgs.teamSizeMin) {
			if (teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
				// Both teams need players
				line1 = "Waiting for more players.";
				line2 = va("The match requires %i player%s per team.",
					cgs.teamSizeMin, cgs.teamSizeMin != 1 ? "s" : "");
			} else {
				// Only red needs players
				int need = cgs.teamSizeMin - teamCounts[TEAM_RED];
				line1 = va("Waiting for %i more player%s", need, need != 1 ? "s" : "");
				line2 = va("to join the %s.", "Red Team");
			}
		} else if (teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
			// Only blue needs players
			int need = cgs.teamSizeMin - teamCounts[TEAM_BLUE];
			line1 = va("Waiting for %i more player%s", need, need != 1 ? "s" : "");
			line2 = va("to join the %s.", "Blue Team");
		} else if (cgs.teamForceBalance &&
				   (teamCounts[TEAM_RED] > teamCounts[TEAM_BLUE] + 1 ||
				    teamCounts[TEAM_BLUE] > teamCounts[TEAM_RED] + 1)) {
			line1 = "The teams must be balanced";
			line2 = "before the match can begin.";
		} else {
			line1 = "The match will begin";
			line2 = "when more players are ready.";
		}
	}

	if (line1) {
		w = CG_Text_Width(line1, 0.35f, 0);
		CG_Text_Paint(320 - w / 2, 88, 0.35f, colorWhite, line1, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
	if (line2) {
		w = CG_Text_Width(line2, 0.35f, 0);
		CG_Text_Paint(320 - w / 2, 108, 0.35f, colorWhite, line2, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
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
CG_DrawTimeout

// Address: 0x1000ef30
[QL] Pause/timeout overlay. Shows "Match Paused" during an indefinite pause, or
"Match resuming in ^5N^7 seconds" once a timein countdown is running. Plays the pause
klaxon once when a pause begins. Called every frame from CG_Draw2D; self-gates on
cgs.freezeEnd (CS_PAUSE_START_TIME).
=================
*/
static void CG_DrawTimeout(void) {
	static int lastPause;     // cgs.freezeEnd of the pause we last announced
	static int lastCountSec;  // last resume-countdown second we voiced
	const char* s;
	float w;
	int sec;

	if (!cgs.freezeEnd) {
		lastPause = 0;
		return;
	}

	// announce a newly-started pause once (klaxon)
	if (lastPause != cgs.freezeEnd) {
		lastPause = cgs.freezeEnd;
		lastCountSec = -1;
		trap_S_StartLocalSound(cgs.media.pauseSound, CHAN_ANNOUNCER);
	}

	if (cgs.pauseEnd == 0) {
		s = "Match Paused";
	} else {
		sec = (cgs.pauseEnd - cg.time) / 1000;

		// [QL] resume-countdown announcer, once per second as the timein countdown ticks.
		// The binary calls CG_PlayMatchStateSound(secondsRemaining): 5 = prepare_to_fight,
		// 3 = "two", 2 = "one", 1 = "fight".
		if (sec != lastCountSec) {
			lastCountSec = sec;
			switch (sec) {
			case 5: trap_S_StartLocalSound(cgs.media.countPrepareSound, CHAN_ANNOUNCER); break;
			case 3: trap_S_StartLocalSound(cgs.media.count2Sound, CHAN_ANNOUNCER); break;
			case 2: trap_S_StartLocalSound(cgs.media.count1Sound, CHAN_ANNOUNCER); break;
			case 1: trap_S_StartLocalSound(cgs.media.countFightSound, CHAN_ANNOUNCER); break;
			}
		}

		if (sec <= 0) {
			return;  // countdown finished; server is about to resume
		}
		s = va("Match resuming in ^5%d^7 seconds", sec);
	}

	w = CG_Text_Width(s, 0.5f, 0);
	CG_Text_Paint(320 - w / 2, 128, 0.5f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
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

	// [QL] 0x1000e9f2: open it on the first frame it is asked for, and take the
	// key catcher so clicks reach CG_KeyEvent/CG_MouseEvent instead of the game.
	if (!(menu->window.flags & WINDOW_VISIBLE)) {
		// CG_Init memsets cgs, so the cursor sits at (0,0) - the top left corner, well
		// outside the menu - until the player happens to move the mouse. Every click
		// before that first movement hit-tests there and lands as an out-of-bounds
		// click. Retail never has this problem because its engine hands cgame an
		// absolute pointer position (0x1003c4f0); ioquake3's sends deltas, so the
		// starting point has to be chosen here. Centre of the 640x480 virtual screen.
		if (cgs.cursorX == 0 && cgs.cursorY == 0) {
			cgs.cursorX = SCREEN_WIDTH / 2;
			cgs.cursorY = SCREEN_HEIGHT / 2;
			cgDC.cursorx = cgs.cursorX;
			cgDC.cursory = cgs.cursorY;
		}
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

	// Nothing in cgame ever drew cgs.activeCursor - it is assigned in
	// CG_MouseEvent and then never read, so the pointer was invisible on every
	// cgame-owned menu. Drawn here with the same 16-pixel centring the UI
	// module uses (ui_main.c:524) so the two look alike. CG_DrawPic runs the
	// coordinates through CG_AdjustFrom640, the same mapping the menu itself
	// just used, so the arrow lands where the cursor actually is.
	CG_DrawPic(cgs.cursorX - 16, cgs.cursorY - 16, 32, 32,
	           cgs.activeCursor ? cgs.activeCursor : cgs.media.selectCursor);
}

static void CG_Draw2D(stereoFrame_t stereoFrame) {
	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
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
	// called from CG_Draw2D @ 0x10010e9e).
	CG_SetWidescreen(WIDESCREEN_STRETCH);
	CG_DrawFloatingEffects();

	// [QL] 0x10010fdd, right after the floating effects: while the join menu owns
	// the frame the HUD is skipped entirely. The scoreboard wins if it is up.
	if (cg.showJoinMenu && !cg.scoreBoardShowing) {
		CG_UpdateJoinMenu();
		return;
	}

	/*
		if (cg.cameraMode) {
			return;
		}
	*/
	// [QL] cgamex86.dll 0x10010ff7..0x100110b5. Which branch runs is decided by the
	// predicted playerState, and the test is not Quake 3's:
	//   0x10010ff7  test [cg.predictedPlayerState.eFlags], EF_VOTED
	//   0x10011003  cmp  [cg.predictedPlayerState.pm_type], PM_SPECTATOR
	// Both are what SpectatorClientEndFrame (qagamex86.dll 0x10035470) leaves on a
	// follower - pm_type forced back to PM_SPECTATOR, EF_VOTED on a real spectator -
	// so a spectator following somebody gets this branch too. The old test on
	// persistant[PERS_TEAM] saw the FOLLOWED player's team and missed them.
	if ((cg.predictedPlayerState.eFlags & EF_VOTED) ||
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
			// NOT PORTED: retail calls 0x1000f700 after this block for every spectator
			// (it tests EF_VOTED). Not identified yet; see bitacora/PENDIENTES.md.
		}
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
	CG_SetWidescreen(WIDESCREEN_RIGHT);
	CG_DrawTeamVote();
	CG_SetWidescreen(WIDESCREEN_RIGHT);
	CG_DrawLagometer();
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	if (!cg_paused.integer) {
		CG_SetWidescreen(WIDESCREEN_RIGHT);
		CG_DrawUpperRight(stereoFrame);
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}

	CG_SetWidescreen(WIDESCREEN_CENTER);
	// [QL] cgamex86.dll 0x10011154..0x1001115a: the warmup/centre messages are drawn
	// unconditionally. Quake 3's `if (!CG_DrawFollow()) CG_DrawWarmup();` is gone with
	// CG_DrawFollow itself: retail has no "following" string at all, and the followed
	// player's name is drawn by the spectator HUD through CG_FOLLOW_PLAYER_NAME_EX.
	CG_DrawWarmup();
	CG_DrawTimeout();  // [QL] pause/timeout overlay
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	// [QL] race timer is drawn by menu system via CG_DrawRaceTimes ownerdraw
	// (removed hardcoded duplicate here)

	// [QL] no chat here: retail draws it only through the CG_AREA_NEW_CHAT
	// ownerdraw in the HUD menus and from the spectator overlay (0x1000db98).

	// don't draw center string if scoreboard is up
	cg.scoreBoardShowing = CG_DrawScoreboard();
	if (!cg.scoreBoardShowing) {
		CG_SetWidescreen(WIDESCREEN_CENTER);
		CG_DrawCenterString();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
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
		CG_DrawInformation();
		return;
	}

	// optionally draw the tournement scoreboard instead
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		(cg.snap->ps.pm_flags & PMF_SCOREBOARD)) {
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
	trap_R_RenderScene(&cg.refdef);

	// draw status bar and other floating elements
	CG_Draw2D(stereoView);

	// QL binary: vignette overlay drawn after HUD
	if (!cg.renderingThirdPerson) {
		CG_DrawFullScreenColor();
	}
}
