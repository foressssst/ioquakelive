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
#include "ui_local.h"

uiInfo_t uiInfo;

static char* netnames[] = {
    "???",
    "UDP",
    "UDP6"};

static int gamecodetoui[] = {4, 2, 3, 0, 5, 1, 6};
static int uitogamecode[] = {4, 6, 2, 3, 1, 5, 7};

static const char *skillLevels[] = {
    "I Can Win",
    "Bring It On",
    "Hurt Me Plenty",
    "Hardcore",
    "Nightmare"
};
static const int numSkillLevels = ARRAY_LEN(skillLevels);

static void UI_FeederSelection(float feederID, int index);
static const char* UI_GetGameTypeString(int gametype);
static int UI_MapCountByGameType(qboolean singlePlayer);
static int UI_MapCountByCallvoteGameType(void);
static int UI_HeadCountByTeam(void);
static void UI_ParseGameInfo(const char* teamFile);
static void UI_ParseTeamInfo(const char* teamFile);
static const char* UI_SelectedMap(int index, int* actual);
static const char* UI_SelectedHead(int index, int* actual);
static int UI_GetIndexFromSelection(int actual);
static void UI_DrawCinematic(int handle, float x, float y, float w, float h);

static void UI_InsertServerIntoDisplayList(int num, int position);
static void UI_RemoveServerFromDisplayList(int num);
static void UI_BinaryServerInsert(int num);
static void UI_BuildServerDisplayList(int force);
static void UI_SortServerStatusInfo(serverStatusInfo_t *info);
static qboolean UI_GetServerStatusInfo(const char *serverAddress, serverStatusInfo_t *info);
static void UI_BuildServerStatus(qboolean force);
static void UI_BuildFindPlayerList(qboolean force);
static void UI_ServersSort(int column, qboolean force);
static void UI_DoServerRefresh(void);
static void UI_ServerRefreshComplete(void);
static void UI_StartServerRefresh(qboolean full);

static void UI_DrawSkill(rectDef_t *rect, float scale, vec4_t color, int textStyle);
static void UI_DrawNetSource(rectDef_t *rect, float scale, vec4_t color, int textStyle);
static void UI_DrawServerFilter(rectDef_t *rect, float scale, vec4_t color, int textStyle);
static void UI_DrawMOTD(rectDef_t *rect, float scale, vec4_t color, int textStyle);
static qboolean UI_Skill_HandleKey(int flags, float *special, int key);
static qboolean UI_NetSource_HandleKey(int flags, float *special, int key);
static qboolean UI_ServerFilter_HandleKey(int flags, float *special, int key);

static const char *netSourceNames[] = {
    "Local",
    "Mplayer",
    "Internet",
    "Favorites",
    "FFA"
};

static const char *monthAbbrev[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static serverFilter_t serverFilters[] = {
    { "All",                   "" },
    { "Quake Live",            "" },
    { "Team Arena",            "missionpack" },
    { "Rocket Arena",          "arena" },
    { "Alliance",              "alliance20" },
    { "Weapons Factory Arena", "wfa" },
    { "OSP",                   "osp" }
};

vmCvar_t ui_debug;
vmCvar_t ui_initialized;

void _UI_Init(qboolean);
void _UI_Shutdown(void);
void _UI_KeyEvent(int key, qboolean down);
void _UI_MouseEvent(int dx, int dy);
void _UI_MousePosition(int x, int y);
void _UI_Refresh(int realtime);
qboolean _UI_IsFullscreen(void);
qboolean UI_CheckActiveMenu(void);
void UI_WalkMenus(void (*callback)(const char* mapLoadName));
static void UI_SetScoreBoardCvars(const postGameInfo_t* info, qboolean redBlue);
static void UI_SPPostgameMenu_f(const char* mapname, int gameType);
static void UI_CheckModelBright(const char* modelCvar, const char* skinCvar, const char* targetCvar);
static void UI_OnAnnouncerChanged(void);
Q_EXPORT intptr_t vmMain(int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11) {
    switch (command) {
        case UI_GETAPIVERSION:
            return UI_API_VERSION;

        case UI_INIT:
            _UI_Init(arg0);
            return 0;

        case UI_SHUTDOWN:
            _UI_Shutdown();
            return 0;

        case UI_KEY_EVENT:
            _UI_KeyEvent(arg0, arg1);
            return 0;

        case UI_MOUSE_EVENT:
            _UI_MouseEvent(arg0, arg1);
            return 0;

        case UI_MOUSE_POSITION:
            _UI_MousePosition(arg0, arg1);
            return 0;

        case UI_REFRESH:
            _UI_Refresh(arg0);
            return 0;

        case UI_IS_FULLSCREEN:
            return _UI_IsFullscreen();

        case UI_SET_ACTIVE_MENU:
            _UI_SetActiveMenu(arg0);
            return 0;

        case UI_CONSOLE_COMMAND:
            return UI_ConsoleCommand(arg0);

        case UI_DRAW_CONNECT_SCREEN:
            UI_DrawConnectScreen(arg0);
            return 0;
        case UI_HASUNIQUECDKEY:
            return qtrue;

        case UI_REGISTER_CVARS:
            UI_RegisterCvars();
            return 0;

        case UI_CHECK_ACTIVE_MENU:
            return UI_CheckActiveMenu();

        case UI_WALK_MENUS:
            UI_WalkMenus((void (*)(const char*))arg0);
            return 0;

        case UI_DRAW_ADVERTISEMENT:
            return 0;
    }

    return -1;
}

void AssetCache(void) {
    uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(ASSET_GRADIENTBAR);
    uiInfo.uiDC.Assets.fxBasePic = trap_R_RegisterShaderNoMip(ART_FX_BASE);
    uiInfo.uiDC.Assets.fxPic[0] = trap_R_RegisterShaderNoMip(ART_FX_RED);
    uiInfo.uiDC.Assets.fxPic[1] = trap_R_RegisterShaderNoMip(ART_FX_YELLOW);
    uiInfo.uiDC.Assets.fxPic[2] = trap_R_RegisterShaderNoMip(ART_FX_GREEN);
    uiInfo.uiDC.Assets.fxPic[3] = trap_R_RegisterShaderNoMip(ART_FX_TEAL);
    uiInfo.uiDC.Assets.fxPic[4] = trap_R_RegisterShaderNoMip(ART_FX_BLUE);
    uiInfo.uiDC.Assets.fxPic[5] = trap_R_RegisterShaderNoMip(ART_FX_CYAN);
    uiInfo.uiDC.Assets.fxPic[6] = trap_R_RegisterShaderNoMip(ART_FX_WHITE);
    uiInfo.uiDC.Assets.scrollBar = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR);
    uiInfo.uiDC.Assets.scrollBarArrowDown = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
    uiInfo.uiDC.Assets.scrollBarArrowUp = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
    uiInfo.uiDC.Assets.scrollBarArrowLeft = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
    uiInfo.uiDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
    uiInfo.uiDC.Assets.scrollBarThumb = trap_R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);
    uiInfo.uiDC.Assets.sliderBar = trap_R_RegisterShaderNoMip(ASSET_SLIDER_BAR);
    uiInfo.uiDC.Assets.sliderThumb = trap_R_RegisterShaderNoMip(ASSET_SLIDER_THUMB);

    for (int n = 1; n < NUM_CROSSHAIRS; n++) {
        uiInfo.uiDC.Assets.crosshairShader[n] = trap_R_RegisterShaderNoMip(va("gfx/2d/crosshair%i", n));
    }

    {
        fileHandle_t f;
        const int len = trap_FS_FOpenFile("sound/feedback/voc_newhighscore.ogg", &f, FS_READ);

        if (f) {
            trap_FS_FCloseFile(f);
        }
        if (len > 0) {
            uiInfo.newHighScoreSound = trap_S_RegisterSound("sound/feedback/voc_newhighscore.ogg", qfalse);
        }
    }
}

void _UI_DrawSides(float x, float y, float w, float h, float size) {
    UI_AdjustFrom640(&x, &y, &w, &h);
    size *= uiInfo.uiDC.xscale;
    trap_R_DrawStretchPic(x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_DrawStretchPic(x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

void _UI_DrawTopBottom(float x, float y, float w, float h, float size) {
    UI_AdjustFrom640(&x, &y, &w, &h);
    size *= uiInfo.uiDC.yscale;
    trap_R_DrawStretchPic(x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_DrawStretchPic(x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

void _UI_DrawRect(float x, float y, float width, float height, float size, const float* color) {
    trap_R_SetColor(color);

    _UI_DrawTopBottom(x, y, width, height, size);
    _UI_DrawSides(x, y, width, height, size);

    trap_R_SetColor(NULL);
}

static int UI_EngineFont(int fontIndex) {
    if (fontIndex >= 0 && fontIndex < 3)
        return fontIndex;
    return 0;
}

static int UI_IndexForFontPtr(const fontInfo_t* font) {
    int i;
    for (i = 0; i < 3; i++) {
        if (font == &uiInfo.uiDC.Assets.extraFonts[i])
            return i;
    }
    return 0;
}

static float UI_FontPixelSize(float scale) {
    return (((float)uiInfo.uiDC.glconfig.vidHeight / 768.0f) * 96.0f) * scale;
}

static void UI_TextToScreen(float* x, float* y) {
    float w = 0.0f, h = 0.0f;
    UI_AdjustFrom640(x, y, &w, &h);
}

static void UI_PaintText(float x, float y, int fontIndex, float scale, const vec4_t color,
                         const char* text, int limit, int style) {
    float sx = x, sy = y;
    float size = UI_FontPixelSize(scale);
    int efont = UI_EngineFont(fontIndex);
    int lim = (limit > 0) ? limit : -1;

    if (!text || !*text)
        return;

    UI_TextToScreen(&sx, &sy);

    if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
        float ofs = (style == ITEM_TEXTSTYLE_SHADOWED) ? 1.0f : 2.0f;
        float shx = x + ofs, shy = y + ofs;
        UI_TextToScreen(&shx, &shy);
        trap_R_SetColor(colorBlack);
        trap_R_Font_DrawString((int)shx, (int)shy, text, efont, size, lim, NULL, TEXT_NORECOLOR);
    }

    trap_R_SetColor(color);
    trap_R_Font_DrawString((int)sx, (int)sy, text, efont, size, lim, NULL, 0);
    trap_R_SetColor(NULL);
}

static void UI_MeasureText(const char* text, float scale, int fontIndex, int limit, int* w640, int* h640) {
    float extents[5];
    float xscale = uiInfo.uiDC.xscale;
    float yscale = uiInfo.uiDC.yscale;
    float size = UI_FontPixelSize(scale);
    int lim = (limit > 0) ? limit : -1;

    trap_R_Font_TextExtents(text, 0, UI_EngineFont(fontIndex), size, lim, extents);

    if (xscale <= 0.0f)
        xscale = 1.0f;
    if (yscale <= 0.0f)
        yscale = 1.0f;
    if (w640)
        *w640 = (int)((extents[2] - extents[0]) / xscale);
    if (h640)
        *h640 = (int)(extents[4] / yscale);
}

int Text_Width(const char* text, float scale, int limit) {
    int w = 0;
    UI_MeasureText(text, scale, 0, limit, &w, NULL);
    return w;
}

int Text_Height(const char* text, float scale, int limit) {
    int h = 0;
    UI_MeasureText(text, scale, 0, limit, NULL, &h);
    return h;
}

void Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
    float w, h;
    w = width * scale;
    h = height * scale;
    UI_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style) {
    (void)adjust;
    UI_PaintText(x, y, 0, scale, color, text, limit, style);
}

void Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style) {
    char cbuf[2];

    if (!text)
        return;

    UI_PaintText(x, y, 0, scale, color, text, limit, style);

    if (!((uiInfo.uiDC.realTime / BLINK_DIVISOR) & 1)) {
        int w640 = 0;
        int tl = (int)strlen(text);
        int cp = cursorPos;
        if (cp < 0)
            cp = 0;
        if (cp > tl)
            cp = tl;
        if (cp > 0)
            UI_MeasureText(text, scale, 0, cp, &w640, NULL);
        cbuf[0] = cursor;
        cbuf[1] = '\0';
        UI_PaintText(x + (float)w640, y, 0, scale, color, cbuf, 1, style);
    }
}

static void Text_Paint_Limit(float* maxX, float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit) {
    int w640;
    float avail = *maxX - x;
    (void)adjust;

    if (!text || !*text)
        return;

    w640 = Text_Width(text, scale, limit);
    if (avail > 0.0f && (float)w640 > avail) {
        int full = (limit > 0) ? limit : (int)strlen(text);
        int fit = (int)((float)full * (avail / (float)w640));
        if (fit < 0)
            fit = 0;
        limit = fit;
        w640 = Text_Width(text, scale, limit);
    }

    Text_Paint(x, y, scale, color, text, 0, limit, 0);
    *maxX = x + (float)w640;
}

static void Text_Paint_Font(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, fontInfo_t* font) {
    (void)adjust;
    UI_PaintText(x, y, UI_IndexForFontPtr(font), scale, color, text, limit, style);
}

static float Text_Width_Font(const char* text, float scale, int limit, fontInfo_t* font) {
    int w = 0;
    UI_MeasureText(text, scale, UI_IndexForFontPtr(font), limit, &w, NULL);
    return (float)w;
}

static float Text_Height_Font(const char* text, float scale, int limit, fontInfo_t* font) {
    int h = 0;
    UI_MeasureText(text, scale, UI_IndexForFontPtr(font), limit, NULL, &h);
    return (float)h;
}

static void UI_DrawText_DC(float x, float y, float scale, vec4_t color, const char* text,
                           float adjust, int limit, int style, int fontIndex) {
    (void)adjust;
    UI_PaintText(x, y, fontIndex, scale, color, text, limit, style);
}

static float UI_TextWidth_DC(const char* text, float scale, int limit, int fontIndex) {
    int w = 0;
    UI_MeasureText(text, scale, fontIndex, limit, &w, NULL);
    return (float)w;
}

static float UI_TextHeight_DC(const char* text, float scale, int limit, int fontIndex) {
    int h = 0;
    UI_MeasureText(text, scale, fontIndex, limit, NULL, &h);
    return (float)h;
}

static void UI_DrawTextWithCursor_DC(float x, float y, float scale, vec4_t color, const char* text,
                                     int cursorPos, char cursor, int limit, int style, int fontIndex) {
    (void)cursorPos;
    (void)cursor;
    UI_PaintText(x, y, fontIndex, scale, color, text, limit, style);
}

static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle);

static void UI_OwnerDraw_DC(float x, float y, float w, float h, float text_x, float text_y,
                            int ownerDraw, int ownerDrawFlags, int align, float special,
                            float scale, vec4_t color, qhandle_t shader, int textStyle, int fontIndex) {
    UI_OwnerDraw(x, y, w, h, text_x, text_y, ownerDraw, ownerDrawFlags, align, special, scale, color, shader, textStyle);
}

void UI_ShowPostGame(qboolean newHigh) {
    trap_Cvar_Set("cg_cameraOrbit", "0");
    trap_Cvar_Set("cg_thirdPerson", "0");
    uiInfo.soundHighScore = newHigh;
    _UI_SetActiveMenu(UIMENU_POSTGAME);
}

void UI_DrawCenteredPic(qhandle_t image, int w, int h) {
    int x, y;
    x = (SCREEN_WIDTH - w) / 2;
    y = (SCREEN_HEIGHT - h) / 2;
    UI_DrawHandlePic(x, y, w, h, image);
}

static void UI_ReadResolution(void) {
    trap_GetGlconfig(&uiInfo.uiDC.glconfig);

    trap_Cvar_Set("ui_videomode", va("%dx%d", uiInfo.uiDC.glconfig.vidWidth, uiInfo.uiDC.glconfig.vidHeight));

    uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0 / (float)SCREEN_HEIGHT);
    uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0 / (float)SCREEN_WIDTH);
    if (uiInfo.uiDC.glconfig.vidWidth * SCREEN_HEIGHT > uiInfo.uiDC.glconfig.vidHeight * SCREEN_WIDTH) {
        uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - (uiInfo.uiDC.glconfig.vidHeight * ((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)));
        uiInfo.uiDC.xscale = uiInfo.uiDC.yscale;
    } else {
        uiInfo.uiDC.bias = 0;
    }
}

int frameCount = 0;
int startTime;

#define UI_FPS_FRAMES 125
void _UI_Refresh(int realtime) {
    static int index;
    static int previousTimes[UI_FPS_FRAMES];
    static int lastResize = -1;
    int resizeCount = (int)trap_Cvar_VariableValue("vid_resizeCount");

    if (lastResize != resizeCount) {
        lastResize = resizeCount;
        UI_ReadResolution();
    }

    uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
    uiInfo.uiDC.realTime = realtime;

    previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
    index++;
    if (index > UI_FPS_FRAMES) {
        int i, total;
        total = 0;
        for (i = 0; i < UI_FPS_FRAMES; i++) {
            total += previousTimes[i];
        }
        if (!total) {
            total = 1;
        }
        uiInfo.uiDC.FPS = 1000 * UI_FPS_FRAMES / total;
    }

    UI_UpdateCvars();

    if (Menu_Count() > 0) {
        Menu_PaintAll();
        UI_DoServerRefresh();
        UI_BuildServerStatus(qfalse);
        UI_BuildFindPlayerList(qfalse);
    }

    UI_SetColor(NULL);
}

void _UI_Shutdown(void) {
    UI_Home_Leave();
}

char* defaultMenu = NULL;

char* GetMenuBuffer(const char* filename) {
    int len;
    fileHandle_t f;
    static char buf[MAX_MENUFILE];

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        trap_Print(va(S_COLOR_RED "menu file not found: %s, using default\n", filename));
        return defaultMenu;
    }
    if (len >= MAX_MENUFILE) {
        trap_Print(va(S_COLOR_RED "menu file too large: %s is %i, max allowed is %i\n", filename, len, MAX_MENUFILE));
        trap_FS_FCloseFile(f);
        return defaultMenu;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);
    return buf;
}

qboolean Asset_Parse(int handle) {
    pc_token_t token;
    const char* tempStr;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;
    if (Q_stricmp(token.string, "{") != 0) {
        return qfalse;
    }

    while (1) {
        memset(&token, 0, sizeof(pc_token_t));

        if (!trap_PC_ReadToken(handle, &token))
            return qfalse;

        if (Q_stricmp(token.string, "}") == 0) {
            return qtrue;
        }

        if (Q_stricmp(token.string, "font") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(DEFAULT_SANS_FONT, pointSize, &uiInfo.uiDC.Assets.textFont);
            uiInfo.uiDC.Assets.fontRegistered = qtrue;
            continue;
        }

        if (Q_stricmp(token.string, "smallFont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(DEFAULT_MONO_FONT, pointSize, &uiInfo.uiDC.Assets.smallFont);
            continue;
        }

        if (Q_stricmp(token.string, "bigFont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(DEFAULT_FONT, pointSize, &uiInfo.uiDC.Assets.bigFont);
            continue;
        }

        if (Q_stricmp(token.string, "gradientbar") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
            continue;
        }

        if (Q_stricmp(token.string, "menuEnterSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "menuExitSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "itemFocusSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "menuBuzzSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "cursor") == 0) {
            if (!PC_String_Parse(handle, &uiInfo.uiDC.Assets.cursorStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.cursor = trap_R_RegisterShaderNoMip(uiInfo.uiDC.Assets.cursorStr);
            continue;
        }

        if (Q_stricmp(token.string, "fadeClamp") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeClamp)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeCycle") == 0) {
            if (!PC_Int_Parse(handle, &uiInfo.uiDC.Assets.fadeCycle)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeAmount") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeAmount)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowX") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowX)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowY") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowY)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowColor") == 0) {
            if (!PC_Color_Parse(handle, &uiInfo.uiDC.Assets.shadowColor)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
            continue;
        }
    }
    return qfalse;
}

void Font_Report(void) {
    int i;
    Com_Printf("Font Info\n");
    Com_Printf("=========\n");
    for (i = 32; i < 96; i++) {
        Com_Printf("  Glyph handle %i: %i\n", i, uiInfo.uiDC.Assets.textFont.glyphs[i].glyph);
    }
}

void UI_Report(void) {
    String_Report();
    Font_Report();
}

void UI_ParseMenu(const char* menuFile) {
    int handle;
    pc_token_t token;

#ifdef _DEBUG
    Com_Printf("Parsing menu file: %s\n", menuFile);
#endif

    handle = trap_PC_LoadSource(menuFile);
    if (!handle) {
        return;
    }

    while (1) {
        memset(&token, 0, sizeof(pc_token_t));
        if (!trap_PC_ReadToken(handle, &token)) {
            break;
        }

        if (token.string[0] == '}') {
            break;
        }

        if (Q_stricmp(token.string, "assetGlobalDef") == 0) {
            if (Asset_Parse(handle)) {
                memcpy(&uiInfo.uiDC.Assets.extraFonts[0], &uiInfo.uiDC.Assets.textFont, sizeof(fontInfo_t));
                memcpy(&uiInfo.uiDC.Assets.extraFonts[1], &uiInfo.uiDC.Assets.bigFont, sizeof(fontInfo_t));
                memcpy(&uiInfo.uiDC.Assets.extraFonts[2], &uiInfo.uiDC.Assets.smallFont, sizeof(fontInfo_t));
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token.string, "menudef") == 0) {
            Menu_New(handle);
        }
    }
    trap_PC_FreeSource(handle);
}

static const char* UI_MenuFile(const char* file) {
    static const char* const replaced[][2] = {
        {"ui/main.menu", "ui/ioql/main.menu"},
        {"ui/demo.menu", "ui/ioql/demo.menu"},
    };
    int i;

    for (i = 0; i < (int)ARRAY_LEN(replaced); i++) {
        if (!Q_stricmp(file, replaced[i][0])) {
            return replaced[i][1];
        }
    }
    return file;
}

qboolean Load_Menu(int handle) {
    pc_token_t token;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;
    if (token.string[0] != '{') {
        return qfalse;
    }

    while (1) {
        if (!trap_PC_ReadToken(handle, &token))
            return qfalse;

        if (token.string[0] == 0) {
            return qfalse;
        }

        if (token.string[0] == '}') {
            return qtrue;
        }

        UI_ParseMenu(UI_MenuFile(token.string));
    }
    return qfalse;
}

void UI_LoadMenus(const char* menuFile, qboolean reset) {
    pc_token_t token;
    int handle;
    int start;

    start = trap_Milliseconds();

    handle = trap_PC_LoadSource(menuFile);
    if (!handle) {
        Com_Printf(S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile);
        handle = trap_PC_LoadSource("ui/menus.txt");
        if (!handle) {
            Com_Printf(S_COLOR_RED "default menu file not found: ui/menus.txt, unable to continue!\n");
        }
    }

    if (reset) {
        Menu_Reset();
    }

    while (1) {
        if (!trap_PC_ReadToken(handle, &token))
            break;
        if (token.string[0] == 0 || token.string[0] == '}') {
            break;
        }

        if (token.string[0] == '}') {
            break;
        }

        if (Q_stricmp(token.string, "loadmenu") == 0) {
            if (Load_Menu(handle)) {
                continue;
            } else {
                break;
            }
        }
    }

    Com_Printf("UI menu load time = %d milli seconds\n", trap_Milliseconds() - start);

    trap_PC_FreeSource(handle);
}

void UI_Load(void) {
    char lastName[1024];
    menuDef_t* menu = Menu_GetFocused();
    char* menuSet = UI_Cvar_VariableString("ui_menuFiles");
    if (menu && menu->window.name) {
        Q_strncpyz(lastName, menu->window.name, sizeof(lastName));
    }
    if (menuSet == NULL || menuSet[0] == '\0') {
        menuSet = "ui/menus.txt";
    }

    String_Init();

    UI_ParseGameInfo("gameinfo.txt");
    UI_LoadArenas();

    UI_LoadMenus(menuSet, qtrue);
    Menus_CloseAll();
    Menus_ActivateByName(lastName);
}

static const char* handicapValues[] = {"None", "95", "90", "85", "80", "75", "70", "65", "60", "55", "50", "45", "40", "35", "30", "25", "20", "15", "10", "5", NULL};

static void UI_DrawHandicap(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int i, h;

    h = Com_Clamp(5, 100, trap_Cvar_VariableValue("handicap"));
    i = 20 - h / 5;

    Text_Paint(rect->x, rect->y, scale, color, handicapValues[i], 0, 0, textStyle);
}

static void UI_SetCapFragLimits(qboolean uiVars) {
    int cap = 5;
    int frag = 10;
    if (uiInfo.gameTypes[ui_gameType.integer].gtEnum == GT_OBELISK) {
        cap = 4;
    } else if (uiInfo.gameTypes[ui_gameType.integer].gtEnum == GT_HARVESTER) {
        cap = 15;
    }
    if (uiVars) {
        trap_Cvar_Set("ui_captureLimit", va("%d", cap));
        trap_Cvar_Set("ui_fragLimit", va("%d", frag));
    } else {
        trap_Cvar_Set("capturelimit", va("%d", cap));
        trap_Cvar_Set("fraglimit", va("%d", frag));
    }
}
static void UI_DrawGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_gameType.integer].gameType, 0, 0, textStyle);
}

static void UI_DrawNetGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (ui_netGameType.integer < 0 || ui_netGameType.integer > uiInfo.numGameTypes) {
        trap_Cvar_Set("ui_netGametype", "0");
        trap_Cvar_Set("ui_actualNetGametype", "0");
    }
    Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_netGameType.integer].gameType, 0, 0, textStyle);
}

static void UI_DrawJoinGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (ui_joinGameType.integer < 0 || ui_joinGameType.integer > uiInfo.numJoinGameTypes) {
        trap_Cvar_Set("ui_joinGametype", "0");
    }
    Text_Paint(rect->x, rect->y, scale, color, uiInfo.joinGameTypes[ui_joinGameType.integer].gameType, 0, 0, textStyle);
}

static int UI_TeamIndexFromName(const char* name) {
    int i;

    if (name && *name) {
        for (i = 0; i < uiInfo.teamCount; i++) {
            if (Q_stricmp(name, uiInfo.teamList[i].teamName) == 0) {
                return i;
            }
        }
    }

    return 0;
}

static void UI_DrawClanCinematic(rectDef_t* rect, float scale, vec4_t color) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (i >= 0 && i < uiInfo.teamCount) {
        if (uiInfo.teamList[i].cinematic >= -2) {
            if (uiInfo.teamList[i].cinematic == -1) {
                uiInfo.teamList[i].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.teamList[i].imageName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
            }
            if (uiInfo.teamList[i].cinematic >= 0) {
                trap_CIN_RunCinematic(uiInfo.teamList[i].cinematic);
                UI_DrawCinematic(uiInfo.teamList[i].cinematic, rect->x, rect->y, rect->w, rect->h);
            } else {
                trap_R_SetColor(color);
                UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
                trap_R_SetColor(NULL);
                uiInfo.teamList[i].cinematic = -2;
            }
        } else {
            trap_R_SetColor(color);
            UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
            trap_R_SetColor(NULL);
        }
    }
}

static void UI_DrawPreviewCinematic(rectDef_t* rect, float scale, vec4_t color) {
    if (uiInfo.previewMovie > -2) {
        uiInfo.previewMovie = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.movieList[uiInfo.movieIndex]), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        if (uiInfo.previewMovie >= 0) {
            trap_CIN_RunCinematic(uiInfo.previewMovie);
            UI_DrawCinematic(uiInfo.previewMovie, rect->x, rect->y, rect->w, rect->h);
        } else {
            uiInfo.previewMovie = -2;
        }
    }
}

static void UI_DrawTeamName(rectDef_t* rect, float scale, vec4_t color, qboolean blue, int textStyle) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueteam" : "ui_redteam"));
    if (i >= 0 && i < uiInfo.teamCount) {
        Text_Paint(rect->x, rect->y, scale, color, va("%s: %s", (blue) ? "Blue" : "Red", uiInfo.teamList[i].teamName), 0, 0, textStyle);
    }
}

static void UI_DrawEffects(rectDef_t* rect, float scale, vec4_t color) {
    UI_DrawHandlePic(rect->x, rect->y - 14, 128, 8, uiInfo.uiDC.Assets.fxBasePic);
    UI_DrawHandlePic(rect->x + uiInfo.effectsColor * 16 + 8, rect->y - 16, 16, 12, uiInfo.uiDC.Assets.fxPic[uiInfo.effectsColor]);
}

static void UI_DrawMapPreview(rectDef_t* rect, float scale, vec4_t color, qboolean net) {
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map < 0 || map > uiInfo.mapCount) {
        if (net) {
            ui_currentNetMap.integer = 0;
            trap_Cvar_Set("ui_currentNetMap", "0");
        } else {
            ui_currentMap.integer = 0;
            trap_Cvar_Set("ui_currentMap", "0");
        }
        map = 0;
    }

    if (uiInfo.mapList[map].levelShot == -1) {
        uiInfo.mapList[map].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[map].imageName);
    }

    if (uiInfo.mapList[map].levelShot > 0) {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.mapList[map].levelShot);
    } else {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
    }
}

static void UI_DrawMapTimeToBeat(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int minutes, seconds, time;
    if (ui_currentMap.integer < 0 || ui_currentMap.integer > uiInfo.mapCount) {
        ui_currentMap.integer = 0;
        trap_Cvar_Set("ui_currentMap", "0");
    }

    time = uiInfo.mapList[ui_currentMap.integer].timeToBeat[uiInfo.gameTypes[ui_gameType.integer].gtEnum];

    minutes = time / 60;
    seconds = time % 60;

    Text_Paint(rect->x, rect->y, scale, color, va("%02i:%02i", minutes, seconds), 0, 0, textStyle);
}

static void UI_DrawMapCinematic(rectDef_t* rect, float scale, vec4_t color, qboolean net) {
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map < 0 || map > uiInfo.mapCount) {
        if (net) {
            ui_currentNetMap.integer = 0;
            trap_Cvar_Set("ui_currentNetMap", "0");
        } else {
            ui_currentMap.integer = 0;
            trap_Cvar_Set("ui_currentMap", "0");
        }
        map = 0;
    }

    if (uiInfo.mapList[map].cinematic >= -1) {
        if (uiInfo.mapList[map].cinematic == -1) {
            uiInfo.mapList[map].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[map].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        }
        if (uiInfo.mapList[map].cinematic >= 0) {
            trap_CIN_RunCinematic(uiInfo.mapList[map].cinematic);
            UI_DrawCinematic(uiInfo.mapList[map].cinematic, rect->x, rect->y, rect->w, rect->h);
        } else {
            uiInfo.mapList[map].cinematic = -2;
        }
    } else {
        UI_DrawMapPreview(rect, scale, color, net);
    }
}

static qboolean updateModel = qtrue;

static const int uiWeaponLoadoutSelectable[WP_NUM_WEAPONS] = {
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
};

static void UI_DrawPlayerModel(rectDef_t* rect) {
    static playerInfo_t info;
    char model[MAX_QPATH];
    char team[256];
    char head[256];
    vec3_t viewangles;
    vec3_t moveangles;
    int weapon;

    Q_strncpyz(model, UI_Cvar_VariableString("model"), sizeof(model));
    Q_strncpyz(head, UI_Cvar_VariableString("headmodel"), sizeof(head));
    team[0] = '\0';

    if (trap_Cvar_VariableValue("cg_loadout") != 0) {
        weapon = (int)trap_Cvar_VariableValue("cg_weaponPrimary");
    } else {
        weapon = WP_MACHINEGUN;
    }
    if (weapon < 0 || weapon >= WP_NUM_WEAPONS || uiWeaponLoadoutSelectable[weapon] != 1) {
        weapon = WP_HMG;
    }

    if (updateModel) {
        memset(&info, 0, sizeof(playerInfo_t));
        viewangles[PITCH] = 5;
        viewangles[YAW] = 210;
        viewangles[ROLL] = 0;
        VectorClear(moveangles);
        UI_PlayerInfo_SetModel(&info, model, head, team);
        UI_PlayerInfo_SetInfo(&info, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, weapon, qfalse);
        updateModel = qfalse;
    }

    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &info, uiInfo.uiDC.realTime / 2);
}

static void UI_DrawNetMapPreview(rectDef_t* rect, float scale, vec4_t color) {
    if (uiInfo.serverStatus.currentServerPreview > 0) {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.serverStatus.currentServerPreview);
    } else {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
    }
}

static void UI_DrawNetMapCinematic(rectDef_t* rect, float scale, vec4_t color) {
    if (ui_currentNetMap.integer < 0 || ui_currentNetMap.integer > uiInfo.mapCount) {
        ui_currentNetMap.integer = 0;
        trap_Cvar_Set("ui_currentNetMap", "0");
    }

    if (uiInfo.serverStatus.currentServerCinematic >= 0) {
        trap_CIN_RunCinematic(uiInfo.serverStatus.currentServerCinematic);
        UI_DrawCinematic(uiInfo.serverStatus.currentServerCinematic, rect->x, rect->y, rect->w, rect->h);
    } else {
        UI_DrawNetMapPreview(rect, scale, color);
    }
}

static const char* UI_EnglishMapName(const char* map) {
    int i;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (Q_stricmp(map, uiInfo.mapList[i].mapLoadName) == 0) {
            return uiInfo.mapList[i].mapName;
        }
    }
    return "";
}

static const char* UI_AIFromName(const char* name) {
    int j;
    for (j = 0; j < uiInfo.aliasCount; j++) {
        if (Q_stricmp(uiInfo.aliasList[j].name, name) == 0) {
            return uiInfo.aliasList[j].ai;
        }
    }
    return "James";
}

static qboolean updateOpponentModel = qtrue;
static void UI_DrawOpponent(rectDef_t* rect) {
    static playerInfo_t info2;
    char model[MAX_QPATH];
    char headmodel[MAX_QPATH];
    char team[256];
    vec3_t viewangles;
    vec3_t moveangles;

    if (updateOpponentModel) {
        Q_strncpyz(model, UI_Cvar_VariableString("ui_opponentModel"), sizeof(model));
        Q_strncpyz(headmodel, UI_Cvar_VariableString("ui_opponentModel"), sizeof(headmodel));
        team[0] = '\0';

        memset(&info2, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 180 - 10;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        VectorClear(moveangles);
        UI_PlayerInfo_SetModel(&info2, model, headmodel, "");
        UI_PlayerInfo_SetInfo(&info2, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
        UI_RegisterClientModelname(&info2, model, headmodel, team);
        updateOpponentModel = qfalse;
    }

    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &info2, uiInfo.uiDC.realTime / 2);
}

static void UI_NextOpponent(void) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    i++;
    if (i >= uiInfo.teamCount) {
        i = 0;
    }
    if (i == j) {
        i++;
        if (i >= uiInfo.teamCount) {
            i = 0;
        }
    }
    trap_Cvar_Set("ui_opponentName", uiInfo.teamList[i].teamName);
}

static void UI_PriorOpponent(void) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    i--;
    if (i < 0) {
        i = uiInfo.teamCount - 1;
    }
    if (i == j) {
        i--;
        if (i < 0) {
            i = uiInfo.teamCount - 1;
        }
    }
    trap_Cvar_Set("ui_opponentName", uiInfo.teamList[i].teamName);
}

static void UI_DrawAllMapsSelection(rectDef_t* rect, float scale, vec4_t color, int textStyle, qboolean net) {
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map >= 0 && map < uiInfo.mapCount) {
        Text_Paint(rect->x, rect->y, scale, color, uiInfo.mapList[map].mapName, 0, 0, textStyle);
    }
}

static void UI_DrawOpponentName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_opponentName"), 0, 0, textStyle);
}

static int UI_OwnerDrawWidth(int ownerDraw, float scale) {
    int i, h;
    const char* s = NULL;

    switch (ownerDraw) {
        case UI_HANDICAP:
            h = Com_Clamp(5, 100, trap_Cvar_VariableValue("handicap"));
            i = 20 - h / 5;
            s = handicapValues[i];
            break;
        case UI_GAMETYPE:
            s = uiInfo.gameTypes[ui_gameType.integer].gameType;
            break;
        case UI_ALLMAPS_SELECTION:
            break;
        case UI_OPPONENT_NAME:
            break;
        case UI_KEYBINDSTATUS:
            if (Display_KeyBindPending()) {
                s = "Waiting for new key... Press ESCAPE to cancel";
            } else {
                s = "Press ENTER or CLICK to change, Press BACKSPACE to clear";
            }
            break;
        default:
            break;
    }

    if (s) {
        return Text_Width(s, scale, 0);
    }
    return 0;
}

static void UI_DrawBotName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int value = uiInfo.botIndex;
    Text_Paint(rect->x, rect->y, scale, color, UI_GetBotNameByNumber(value), 0, 0, textStyle);
}

static void UI_DrawRedBlue(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, (uiInfo.redBlue == 0) ? "Red" : "Blue", 0, 0, textStyle);
}

static const vec4_t ui_crosshairPalette[26] = {
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

const vec4_t* UI_CrosshairPalette(void) {
    return ui_crosshairPalette;
}

static void UI_DrawCrosshair(rectDef_t* rect, float scale, vec4_t color) {
    vec4_t crosshairColor;
    float size;
    int index;

    if (!uiInfo.currentCrosshair) {
        return;
    }

    index = (int)trap_Cvar_VariableValue("cg_crosshairColor") - 1;
    if (index < 0 || index > 25) {
        index = 0;
    }
    VectorScale(ui_crosshairPalette[index], trap_Cvar_VariableValue("cg_crosshairBrightness"), crosshairColor);
    crosshairColor[3] = 1.0f;

    size = trap_Cvar_VariableValue("cg_crosshairSize");
    if (size > 40.0f) {
        size = 40.0f;
    } else if (size < 24.0f) {
        size = 24.0f;
    }

    trap_R_SetColor(crosshairColor);
    UI_DrawHandlePic(rect->x - 2, rect->y - rect->h + 2, size, size, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
    trap_R_SetColor(NULL);
}

static void UI_DrawBotSkill(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (uiInfo.skillIndex >= 0 && uiInfo.skillIndex < numSkillLevels) {
        Text_Paint(rect->x, rect->y, scale, color, skillLevels[uiInfo.skillIndex], 0, 0, textStyle);
    }
}

static void UI_DrawVoteString(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (ui_votestring.string[0]) {
        float x = rect->x;
        int width = Text_Width(ui_votestring.string, scale, 0);
        if (rect->x > 0 && width > 0) {
            x = rect->x - width / 2;
        }
        Text_Paint(x, rect->y, scale, color, ui_votestring.string, 0, 0, textStyle);
    }
}

static void UI_DrawSkill(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int skill = (int)trap_Cvar_VariableValue("g_spSkill");
    if (skill < 1 || skill > 5) {
        skill = 1;
    }
    Text_Paint(rect->x, rect->y, scale, color, skillLevels[skill - 1], 0, 0, textStyle);
}

static void UI_DrawNetSource(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (ui_netSource.integer < 0 || ui_netSource.integer > 4) {
        ui_netSource.integer = 0;
    }
    Text_Paint(rect->x, rect->y, scale, color,
               va("Source: %s", netSourceNames[ui_netSource.integer]), 0, 0, textStyle);
}

static void UI_DrawServerFilter(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (ui_serverFilterType.integer < 0 || ui_serverFilterType.integer > 7) {
        ui_serverFilterType.integer = 0;
    }
    Text_Paint(rect->x, rect->y, scale, color,
               va("Filter: %s", serverFilters[ui_serverFilterType.integer].description), 0, 0, textStyle);
}

static void UI_DrawMOTD(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    char buf[1024];
    trap_GetConfigString(CS_NEXTMAP, buf, sizeof(buf));
    if (buf[0]) {
        Text_Paint(rect->x, rect->y, scale, color, buf, 0, 0, textStyle);
    }
}

static int UI_ParseModelColor(const char* cvarName);

static qboolean UI_ComposeForcedModel(const char* modelCvar, const char* skinCvar, char* out, int outSize) {
    char model[MAX_QPATH];
    char skin[MAX_QPATH];
    char* slash;

    Q_strncpyz(model, UI_Cvar_VariableString(modelCvar), sizeof(model));
    Q_strncpyz(skin, UI_Cvar_VariableString(skinCvar), sizeof(skin));

    if (!model[0]) {
        if (!skin[0]) {
            return qfalse;
        }
        Q_strncpyz(model, "sarge", sizeof(model));
    }

    Q_strncpyz(out, model, outSize);
    if (skin[0]) {
        slash = strchr(out, '/');
        if (slash) {
            *slash = '\0';
        }
        Q_strcat(out, outSize, "/");
        Q_strcat(out, outSize, skin);
    }
    return qtrue;
}

static void UI_DrawTeamPlayerModel(rectDef_t *rect) {
    static playerInfo_t teamInfo;
    static char teamModelCached[MAX_QPATH];
    char composed[MAX_QPATH];
    vec3_t viewangles;

    if (!UI_ComposeForcedModel("cg_forceTeamModel", "cg_forceTeamSkin", composed, sizeof(composed))) {
        return;
    }
    if (Q_stricmp(composed, teamModelCached)) {
        Q_strncpyz(teamModelCached, composed, sizeof(teamModelCached));
        memset(&teamInfo, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 170;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        UI_PlayerInfo_SetModel(&teamInfo, composed, composed, "");
        UI_PlayerInfo_SetInfo(&teamInfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
    }
    teamInfo.customColor = qtrue;
    teamInfo.headColor = UI_ParseModelColor("cg_teamHeadColor");
    teamInfo.torsoColor = UI_ParseModelColor("cg_teamUpperColor");
    teamInfo.legsColor = UI_ParseModelColor("cg_teamLowerColor");
    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &teamInfo, uiInfo.uiDC.realTime / 2);
}

static void UI_DrawEnemyPlayerModel(rectDef_t *rect) {
    static playerInfo_t enemyInfo;
    static char enemyModelCached[MAX_QPATH];
    char composed[MAX_QPATH];
    vec3_t viewangles;

    if (!UI_ComposeForcedModel("cg_forceEnemyModel", "cg_forceEnemySkin", composed, sizeof(composed))) {
        return;
    }
    if (Q_stricmp(composed, enemyModelCached)) {
        Q_strncpyz(enemyModelCached, composed, sizeof(enemyModelCached));
        memset(&enemyInfo, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 170;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        UI_PlayerInfo_SetModel(&enemyInfo, composed, composed, "");
        UI_PlayerInfo_SetInfo(&enemyInfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
    }
    enemyInfo.customColor = qtrue;
    enemyInfo.headColor = UI_ParseModelColor("cg_enemyHeadColor");
    enemyInfo.torsoColor = UI_ParseModelColor("cg_enemyUpperColor");
    enemyInfo.legsColor = UI_ParseModelColor("cg_enemyLowerColor");
    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &enemyInfo, uiInfo.uiDC.realTime / 2);
}

static void UI_DrawServerSettings(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    static const char *gametypeNames[] = {
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
        "Red Rover",
    };
    char serverinfo[MAX_INFO_STRING];
    char pmoveinfo[MAX_INFO_STRING];
    char armorinfo[MAX_INFO_STRING];
    char custombuf[MAX_INFO_STRING];
    unsigned int custom;
    int gametype, gravity, mercy, quad;
    float x, y;
    int col;
    const char *s;

    trap_GetConfigString(CS_SERVERINFO, serverinfo, sizeof(serverinfo));
    trap_GetConfigString(CS_PMOVEINFO, pmoveinfo, sizeof(pmoveinfo));
    trap_GetConfigString(CS_ARMORINFO, armorinfo, sizeof(armorinfo));
    trap_GetConfigString(CS_CUSTOM_SETTINGS, custombuf, sizeof(custombuf));
    custom = (unsigned int)atoi(custombuf);
    gametype = atoi(Info_ValueForKey(serverinfo, "g_gametype"));

    x = rect->x;
    y = rect->y;

    s = ((unsigned int)gametype <= 12u) ? gametypeNames[gametype] : "Unknown Gametype";
    Text_Paint(x, y, scale, color, s, 0, 0, textStyle);
    y += 12.0f;

    Text_Paint(x, y, scale, color, va("Time Limit: %i", atoi(Info_ValueForKey(serverinfo, "timelimit"))), 0, 0, textStyle);
    y += 12.0f;
    col = 2;

    if (gametype < 4) {
        Text_Paint(x, y, scale, color, va("Frag Limit: %i", atoi(Info_ValueForKey(serverinfo, "fraglimit"))), 0, 0, textStyle);
        y += 12.0f;
        col = 3;
    }

    mercy = atoi(Info_ValueForKey(serverinfo, "mercylimit"));
    if (mercy != 0 && gametype > 2) {
        Text_Paint(x, y, scale, color, va("Mercy Limit: %i", mercy), 0, 0, textStyle);
        y += 12.0f;
        col++;
    }

    if (gametype == 5) {
        Text_Paint(x, y, scale, color, va("Capture Limit: %i", atoi(Info_ValueForKey(serverinfo, "capturelimit"))), 0, 0, textStyle);
        y += 12.0f;
        col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }

    switch (gametype) {
    case 4: case 9: case 11: case 12:
        if (gametype < 10 || gametype > 11) {
            Text_Paint(x, y, scale, color, va("Round Limit: %i", atoi(Info_ValueForKey(serverinfo, "roundlimit"))), 0, 0, textStyle);
            y += 12.0f;
            col++;
        }
        break;
    default:
        break;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }

    if (gametype > 9 && gametype < 12) {
        Text_Paint(x, y, scale, color, va("Score Limit: %i", atoi(Info_ValueForKey(serverinfo, "scorelimit"))), 0, 0, textStyle);
        y += 12.0f;
        col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }

    if (custom & 0x2000) {
        Text_Paint(x, y, scale, color, "Air Control", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x4000) {
        Text_Paint(x, y, scale, color, "Ramp Jumping", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (atoi(Info_ValueForKey(armorinfo, "armor_tiered")) != 0) {
        Text_Paint(x, y, scale, color, "Tiered Armor", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x10000) {
        Text_Paint(x, y, scale, color, "Weapon Switching", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    quad = atoi(Info_ValueForKey(serverinfo, "g_quadDamageFactor"));
    if (quad != 3) {
        Text_Paint(x, y, scale, color, va("%ix Quad", quad), 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x8000) {
        Text_Paint(x, y, scale, color, "Physics", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    gravity = atoi(Info_ValueForKey(serverinfo, "g_gravity"));
    if (gravity != 800) {
        Text_Paint(x, y, scale, color, va("Gravity %i", gravity), 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x20000) {
        Text_Paint(x, y, scale, color, "InstaGib", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x40000) {
        Text_Paint(x, y, scale, color, "Quad Hog", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (gametype == 12 && (custom & 0x4000000)) {
        Text_Paint(x, y, scale, color, "Infected", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x80000) {
        Text_Paint(x, y, scale, color, "Regen Health", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if ((custom & 0x100000) && !(custom & 0x20000)) {
        Text_Paint(x, y, scale, color, "Drop Health", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x200000) {
        Text_Paint(x, y, scale, color, "Vampiric Damage", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x400000) {
        Text_Paint(x, y, scale, color, "Item Spawning", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x800000) {
        Text_Paint(x, y, scale, color, "Headshots", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }
    if (custom & 0x1000000) {
        Text_Paint(x, y, scale, color, "Rail Jumping", 0, 0, textStyle);
        y += 12.0f; col++;
    }
    if (col > 7) { col = 0; x += 110.0f; y -= 84.0f; }

    if ((custom & 1) || (custom & 0xfe) || (custom & 0x1f00)) {
        Text_Paint(x, y, scale, color, "MODIFIED WEAPONS:", 0, 0, textStyle);
        if (col >= 9) {
            return;
        }
        y += 6.0f;
        trap_R_SetColor(color);
        if (custom & 1) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_gauntlet.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col = 1;
        }
        if (custom & 2) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_machinegun.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
        }
        if ((custom & 4) && gravity != 2) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_shotgun.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
        }
        if ((custom & 8) && gravity != 2) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_grenade.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
        }
        if ((custom & 0x10) && gravity != 2) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_rocket.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
        }
        if (custom & 0x20) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_lightning.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
        }
        if ((custom & 0x40) && gravity != 2) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_railgun.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
        }
        if ((custom & 0x80) && gravity != 2) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_plasma.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            col++;
            x += 12.0f;
            if (col > 7) { y += 12.0f; col = 0; x -= 96.0f; }
        }
        if (custom & 0x100) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_bfg.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            col++;
            x += 12.0f;
            if (col > 7) { y += 12.0f; col = 0; x -= 96.0f; }
        }
        if (custom & 0x200) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_grapple.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            col++;
            x += 12.0f;
            if (col > 7) { y += 12.0f; col = 0; x -= 96.0f; }
        }
        if (custom & 0x400) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_nailgun.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            col++;
            x += 12.0f;
            if (col > 7) { y += 12.0f; col = 0; x -= 96.0f; }
        }
        if (custom & 0x800) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_proxlauncher.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            col++;
            x += 12.0f;
            if (col > 7) { y += 12.0f; col = 0; x -= 96.0f; }
        }
        if (custom & 0x1000) {
            UI_DrawHandlePic(x, y, 8, 8, trap_R_RegisterShaderNoMip("icons/iconw_chaingun.tga"));
            UI_DrawHandlePic(x + 6, y + 4, 4, 4, trap_R_RegisterShaderNoMip("icons/modified.tga"));
            x += 12.0f;
            col++;
            if (col > 7) { y += 12.0f; x -= 96.0f; }
        }
        trap_R_SetColor(NULL);
    }

    if (y == rect->y) {
        Text_Paint(x, y, scale, color, "Default Settings", 0, 0, textStyle);
    }
}

void UI_BuildPlayerList(void) {
    uiClientState_t cs;
    int n, count, team, team2, playerTeamNumber;
    char info[MAX_INFO_STRING];

    trap_GetClientState(&cs);
    trap_GetConfigString(CS_PLAYERS + cs.clientNum, info, MAX_INFO_STRING);
    uiInfo.playerNumber = cs.clientNum;
    uiInfo.teamLeader = atoi(Info_ValueForKey(info, "tl"));
    team = atoi(Info_ValueForKey(info, "t"));
    trap_GetConfigString(CS_SERVERINFO, info, sizeof(info));
    count = atoi(Info_ValueForKey(info, "sv_maxclients"));
    uiInfo.playerCount = 0;
    uiInfo.myTeamCount = 0;
    playerTeamNumber = 0;
    for (n = 0; n < count; n++) {
        trap_GetConfigString(CS_PLAYERS + n, info, MAX_INFO_STRING);

        if (info[0]) {
            Q_strncpyz(uiInfo.playerNames[uiInfo.playerCount], Info_ValueForKey(info, "n"), MAX_NAME_LENGTH);
            Q_CleanStr(uiInfo.playerNames[uiInfo.playerCount]);
            uiInfo.clientNums[uiInfo.playerCount] = n;
            uiInfo.playerCount++;
            team2 = atoi(Info_ValueForKey(info, "t"));
            if (team2 == team) {
                Q_strncpyz(uiInfo.teamNames[uiInfo.myTeamCount], Info_ValueForKey(info, "n"), MAX_NAME_LENGTH);
                Q_CleanStr(uiInfo.teamNames[uiInfo.myTeamCount]);
                uiInfo.teamClientNums[uiInfo.myTeamCount] = n;
                if (uiInfo.playerNumber == n) {
                    playerTeamNumber = uiInfo.myTeamCount;
                }
                uiInfo.myTeamCount++;
            }
        }
    }

    if (!uiInfo.teamLeader) {
        trap_Cvar_Set("cg_selectedPlayer", va("%d", playerTeamNumber));
    }

    n = trap_Cvar_VariableValue("cg_selectedPlayer");
    if (n < 0 || n > uiInfo.myTeamCount) {
        n = 0;
    }
    if (n < uiInfo.myTeamCount) {
        trap_Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[n]);
    }
}

static void UI_DrawSelectedPlayer(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
        uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
        UI_BuildPlayerList();
    }
    Text_Paint(rect->x, rect->y, scale, color, (uiInfo.teamLeader) ? UI_Cvar_VariableString("cg_selectedPlayerName") : UI_Cvar_VariableString("name"), 0, 0, textStyle);
}

static void UI_DrawServerMOTD(rectDef_t* rect, float scale, vec4_t color) {
    if (uiInfo.serverStatus.motdLen) {
        float maxX;

        if (uiInfo.serverStatus.motdWidth == -1) {
            uiInfo.serverStatus.motdWidth = 0;
            uiInfo.serverStatus.motdPaintX = rect->x + 1;
            uiInfo.serverStatus.motdPaintX2 = -1;
        }

        if (uiInfo.serverStatus.motdOffset > uiInfo.serverStatus.motdLen) {
            uiInfo.serverStatus.motdOffset = 0;
            uiInfo.serverStatus.motdPaintX = rect->x + 1;
            uiInfo.serverStatus.motdPaintX2 = -1;
        }

        if (uiInfo.uiDC.realTime > uiInfo.serverStatus.motdTime) {
            uiInfo.serverStatus.motdTime = uiInfo.uiDC.realTime + 10;
            if (uiInfo.serverStatus.motdPaintX <= rect->x + 2) {
                if (uiInfo.serverStatus.motdOffset < uiInfo.serverStatus.motdLen) {
                    uiInfo.serverStatus.motdPaintX += Text_Width(&uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], scale, 1) - 1;
                    uiInfo.serverStatus.motdOffset++;
                } else {
                    uiInfo.serverStatus.motdOffset = 0;
                    if (uiInfo.serverStatus.motdPaintX2 >= 0) {
                        uiInfo.serverStatus.motdPaintX = uiInfo.serverStatus.motdPaintX2;
                    } else {
                        uiInfo.serverStatus.motdPaintX = rect->x + rect->w - 2;
                    }
                    uiInfo.serverStatus.motdPaintX2 = -1;
                }
            } else {
                uiInfo.serverStatus.motdPaintX -= 2;
                if (uiInfo.serverStatus.motdPaintX2 >= 0) {
                    uiInfo.serverStatus.motdPaintX2 -= 2;
                }
            }
        }

        maxX = rect->x + rect->w - 2;
        Text_Paint_Limit(&maxX, uiInfo.serverStatus.motdPaintX, rect->y + rect->h - 3, scale, color, &uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], 0, 0);
        if (uiInfo.serverStatus.motdPaintX2 >= 0) {
            float maxX2 = rect->x + rect->w - 2;
            Text_Paint_Limit(&maxX2, uiInfo.serverStatus.motdPaintX2, rect->y + rect->h - 3, scale, color, uiInfo.serverStatus.motd, 0, uiInfo.serverStatus.motdOffset);
        }
        if (uiInfo.serverStatus.motdOffset && maxX > 0) {
            if (uiInfo.serverStatus.motdPaintX2 == -1) {
                uiInfo.serverStatus.motdPaintX2 = rect->x + rect->w - 2;
            }
        } else {
            uiInfo.serverStatus.motdPaintX2 = -1;
        }
    }
}

static void UI_DrawKeyBindStatus(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (Display_KeyBindPending()) {
        Text_Paint(rect->x, rect->y, scale, color, "Waiting for new key... Press ESCAPE to cancel", 0, 0, textStyle);
    } else {
        Text_Paint(rect->x, rect->y, scale, color, "Press ENTER or CLICK to change, Press BACKSPACE to clear", 0, 0, textStyle);
    }
}

static void UI_DrawCrosshairColor(rectDef_t* rect, vec4_t color) {
    qboolean health = trap_Cvar_VariableValue("cg_crosshairHealth") != 0;
    int colorIndex = (int)trap_Cvar_VariableValue("cg_crosshairColor");

    if (colorIndex < 0 || colorIndex > 25) {
        colorIndex = 0;
    }

    if (health) {
        vec4_t dim = {0.25f, 0.25f, 0.25f, 1.0f};
        trap_R_SetColor(dim);
    }
    UI_DrawHandlePic(rect->x, rect->y - 14, 128, 8, uiInfo.uiDC.Assets.fxBasePic);
    UI_DrawHandlePic(rect->x + (colorIndex << 4) + 8, rect->y - 16, 16, 12, uiInfo.uiDC.Assets.fxPic[colorIndex]);
    if (health) {
        trap_R_SetColor(color);
    }
}

static void UI_DrawServerRefreshDate(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (uiInfo.serverStatus.refreshActive) {
        vec4_t lowLight, newColor;
        lowLight[0] = 0.8f * color[0];
        lowLight[1] = 0.8f * color[1];
        lowLight[2] = 0.8f * color[2];
        lowLight[3] = 0.8f * color[3];
        LerpColor(color, lowLight, newColor, 0.5 + 0.5 * sin(uiInfo.uiDC.realTime / PULSE_DIVISOR));
        Text_Paint(rect->x, rect->y, scale, newColor,
                   va("Getting info for %d servers (ESC to cancel)", trap_LAN_GetServerCount(ui_netSource.integer)),
                   0, 0, textStyle);
    } else {
        char buff[64];
        Q_strncpyz(buff, UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer)), sizeof(buff));
        Text_Paint(rect->x, rect->y, scale, color, va("Refresh Time: %s", buff), 0, 0, textStyle);
    }
}

static void UI_DrawGLInfo(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    char buff[1024];
    char* eptr;
    const char* lines[64];
    int i, numLines;
    float y;

    Text_Paint(rect->x + 2, rect->y, scale, color, va("VENDOR: %s", uiInfo.uiDC.glconfig.vendor_string), 0, 30, textStyle);
    Text_Paint(rect->x + 2, rect->y + 15, scale, color, va("VERSION: %s: %s", uiInfo.uiDC.glconfig.version_string, uiInfo.uiDC.glconfig.renderer_string), 0, 30, textStyle);
    Text_Paint(rect->x + 2, rect->y + 30, scale, color, va("PIXELFORMAT: color(%d-bits) Z(%d-bits) stencil(%d-bits)", uiInfo.uiDC.glconfig.colorBits, uiInfo.uiDC.glconfig.depthBits, uiInfo.uiDC.glconfig.stencilBits), 0, 30, textStyle);

    Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, sizeof(buff));
    numLines = 0;
    eptr = buff;
    while (numLines < (int)ARRAY_LEN(lines) && *eptr) {
        while (*eptr && *eptr == ' ') {
            *eptr++ = '\0';
        }
        if (*eptr && *eptr != ' ') {
            lines[numLines++] = eptr;
        }
        while (*eptr && *eptr != ' ') {
            eptr++;
        }
    }

    y = rect->y + 45;
    i = 0;
    while (i < numLines) {
        Text_Paint(rect->x + 2, y, scale, color, lines[i++], 0, 0, textStyle);
        if (i < numLines) {
            Text_Paint(rect->x + rect->w / 2, y, scale, color, lines[i++], 0, 0, textStyle);
        }
        y += 10;
        if (y > rect->y + rect->h - 11) {
            break;
        }
    }
}

static int UI_ParseModelColor(const char* cvarName) {
    char buf[64];
    const char* p;
    int value = 0;

    trap_Cvar_VariableStringBuffer(cvarName, buf, sizeof(buf));
    p = strstr(buf, "0x");
    if (p) {
        for (p += 2; *p; p++) {
            char ch = *p;
            int digit;
            if (ch >= '0' && ch <= '9') {
                digit = ch - '0';
            } else if (ch >= 'a' && ch <= 'f') {
                digit = ch - 'a' + 10;
            } else if (ch >= 'A' && ch <= 'F') {
                digit = ch - 'A' + 10;
            } else {
                break;
            }
            value = (value << 4) | digit;
        }
        return value;
    }
    return atoi(buf);
}

static void UI_DrawRedTeamModel(rectDef_t* rect) {
    static playerInfo_t redInfo;
    static char redModelCached[MAX_QPATH];
    char model[MAX_QPATH];
    vec3_t viewangles;

    Q_strncpyz(model, UI_Cvar_VariableString("cg_forceRedTeamModel"), sizeof(model));
    if (!model[0]) {
        return;
    }
    if (Q_stricmp(model, redModelCached)) {
        Q_strncpyz(redModelCached, model, sizeof(redModelCached));
        memset(&redInfo, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 170;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        UI_PlayerInfo_SetModel(&redInfo, model, model, "");
        UI_PlayerInfo_SetInfo(&redInfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
    }
    redInfo.customColor = qtrue;
    redInfo.headColor = UI_ParseModelColor("cg_teamHeadColor");
    redInfo.torsoColor = UI_ParseModelColor("cg_teamUpperColor");
    redInfo.legsColor = UI_ParseModelColor("cg_teamLowerColor");
    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &redInfo, uiInfo.uiDC.realTime / 2);
}

static void UI_DrawBlueTeamModel(rectDef_t* rect) {
    static playerInfo_t blueInfo;
    static char blueModelCached[MAX_QPATH];
    char model[MAX_QPATH];
    vec3_t viewangles;

    Q_strncpyz(model, UI_Cvar_VariableString("cg_forceBlueTeamModel"), sizeof(model));
    if (!model[0]) {
        return;
    }
    if (Q_stricmp(model, blueModelCached)) {
        Q_strncpyz(blueModelCached, model, sizeof(blueModelCached));
        memset(&blueInfo, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 170;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        UI_PlayerInfo_SetModel(&blueInfo, model, model, "");
        UI_PlayerInfo_SetInfo(&blueInfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
    }
    blueInfo.customColor = qtrue;
    blueInfo.headColor = UI_ParseModelColor("cg_enemyHeadColor");
    blueInfo.torsoColor = UI_ParseModelColor("cg_enemyUpperColor");
    blueInfo.legsColor = UI_ParseModelColor("cg_enemyLowerColor");
    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &blueInfo, uiInfo.uiDC.realTime / 2);
}

static qboolean UI_IsWebPage(int ownerDraw) {
    switch (ownerDraw) {
        case UI_STARTMATCH:
        case UI_SETTINGS:
        case UI_HOME:
        case UI_REPLAYS:
        case UI_BROWSER:
        case UI_TRAINING:
        case UI_STATS:
        case UI_MATCHES:
        case UI_LEGAL:
            return qtrue;
        default:
            return qfalse;
    }
}

static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    rectDef_t rect;

    rect.x = x + text_x;
    rect.y = y + text_y;
    rect.w = w;
    rect.h = h;

    switch (ownerDraw) {
        case UI_HANDICAP:
            UI_DrawHandicap(&rect, scale, color, textStyle);
            break;
        case UI_PLAYERMODEL:
            UI_DrawPlayerModel(&rect);
            break;
        case UI_CLANCINEMATIC:
            UI_DrawClanCinematic(&rect, scale, color);
            break;
        case UI_PREVIEWCINEMATIC:
            UI_DrawPreviewCinematic(&rect, scale, color);
            break;
        case UI_GAMETYPE:
            UI_DrawGameType(&rect, scale, color, textStyle);
            break;
        case UI_NETGAMETYPE:
            UI_DrawNetGameType(&rect, scale, color, textStyle);
            break;
        case UI_JOINGAMETYPE:
            UI_DrawJoinGameType(&rect, scale, color, textStyle);
            break;
        case UI_MAPPREVIEW:
            UI_DrawMapPreview(&rect, scale, color, qtrue);
            break;
        case UI_MAP_TIMETOBEAT:
            UI_DrawMapTimeToBeat(&rect, scale, color, textStyle);
            break;
        case UI_MAPCINEMATIC:
            UI_DrawMapCinematic(&rect, scale, color, qfalse);
            break;
        case UI_STARTMAPCINEMATIC:
            UI_DrawMapCinematic(&rect, scale, color, qtrue);
            break;
        case UI_NETMAPPREVIEW:
            UI_DrawNetMapPreview(&rect, scale, color);
            break;
        case UI_NETMAPCINEMATIC:
            UI_DrawNetMapCinematic(&rect, scale, color);
            break;
        case UI_OPPONENTMODEL:
            UI_DrawOpponent(&rect);
            break;
        case UI_ALLMAPS_SELECTION:
            UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qtrue);
            break;
        case UI_MAPS_SELECTION:
            UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qfalse);
            break;
        case UI_OPPONENT_NAME:
            UI_DrawOpponentName(&rect, scale, color, textStyle);
            break;
        case UI_BOTNAME:
            UI_DrawBotName(&rect, scale, color, textStyle);
            break;
        case UI_REDBLUE:
            UI_DrawRedBlue(&rect, scale, color, textStyle);
            break;
        case UI_CROSSHAIR:
            UI_DrawCrosshair(&rect, scale, color);
            break;
        case UI_SELECTEDPLAYER:
            UI_DrawSelectedPlayer(&rect, scale, color, textStyle);
            break;
        case UI_SERVERMOTD:
            UI_DrawServerMOTD(&rect, scale, color);
            break;
        case UI_KEYBINDSTATUS:
            UI_DrawKeyBindStatus(&rect, scale, color, textStyle);
            break;
        case UI_BOTSKILL:
            UI_DrawBotSkill(&rect, scale, color, textStyle);
            break;
        case UI_SKILL:
            UI_DrawSkill(&rect, scale, color, textStyle);
            break;
        case UI_NETSOURCE:
            UI_DrawNetSource(&rect, scale, color, textStyle);
            break;
        case UI_NETFILTER:
            UI_DrawServerFilter(&rect, scale, color, textStyle);
            break;
        case UI_NEXTMAP:
            UI_DrawMOTD(&rect, scale, color, textStyle);
            break;
        case UI_VOTESTRING:
            UI_DrawVoteString(&rect, scale, color, textStyle);
            break;
        case UI_TEAMPLAYERMODEL:
            UI_DrawTeamPlayerModel(&rect);
            break;
        case UI_ENEMYPLAYERMODEL:
            UI_DrawEnemyPlayerModel(&rect);
            break;
        case UI_REDTEAMMODEL:
            UI_DrawRedTeamModel(&rect);
            break;
        case UI_BLUETEAMMODEL:
            UI_DrawBlueTeamModel(&rect);
            break;
        case UI_SERVER_SETTINGS:
            UI_DrawServerSettings(&rect, scale, color, textStyle);
            break;
        case UI_SERVERREFRESHDATE:
            UI_DrawServerRefreshDate(&rect, scale, color, textStyle);
            break;
        case UI_GLINFO:
            UI_DrawGLInfo(&rect, scale, color, textStyle);
            break;
        case UI_CROSSHAIR_COLOR:
            UI_DrawCrosshairColor(&rect, color);
            break;
        case UI_ADVERT:
            trap_R_SetColor(color);
            UI_DrawHandlePic(rect.x, rect.y, rect.w, rect.h, shader);
            trap_R_SetColor(NULL);
            break;
        case UI_STARTMATCH:
            UI_StartMatch_Paint();
            break;
        case UI_SETTINGS:
            UI_Settings_Paint();
            break;
        case UI_HOME:
            UI_Home_Paint();
            break;
        case UI_REPLAYS:
            UI_Replays_Paint();
            break;
        case UI_BROWSER:
            UI_Browser_Paint();
            break;
        case UI_TRAINING:
            UI_Training_Paint();
            break;
        case UI_STATS:
            UI_Stats_Paint();
            break;
        case UI_MATCHES:
            UI_Matches_Paint();
            break;
        case UI_LEGAL:
            UI_Legal_Paint();
            break;
        default:
            break;
    }
    if (UI_IsWebPage(ownerDraw)) {
        UI_Home_ErrorPaint();
    }
}

static qboolean UI_OwnerDrawVisible(int flags, int flags2) {
    qboolean vis = qtrue;

    while (flags) {
        if (flags & UI_SHOW_FFA) {
            if (trap_Cvar_VariableValue("g_gametype") != GT_FFA) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_FFA;
        }

        if (flags & UI_SHOW_NOTFFA) {
            if (trap_Cvar_VariableValue("g_gametype") == GT_FFA) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NOTFFA;
        }

        if (flags & UI_SHOW_LEADER) {
            if (!uiInfo.teamLeader) {
                vis = qfalse;
            } else {
                if (ui_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber) {
                    vis = qfalse;
                }
            }
            flags &= ~UI_SHOW_LEADER;
        }
        if (flags & UI_SHOW_NOTLEADER) {
            if (uiInfo.teamLeader) {
                if (!(ui_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber)) {
                    vis = qfalse;
                }
            }
            flags &= ~UI_SHOW_NOTLEADER;
        }
        if (flags & UI_SHOW_ANYTEAMGAME) {
            if (uiInfo.gameTypes[ui_gameType.integer].gtEnum <= GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_ANYTEAMGAME;
        }
        if (flags & UI_SHOW_ANYNONTEAMGAME) {
            if (uiInfo.gameTypes[ui_gameType.integer].gtEnum > GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_ANYNONTEAMGAME;
        }
        if (flags & UI_SHOW_NETANYTEAMGAME) {
            if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum <= GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NETANYTEAMGAME;
        }
        if (flags & UI_SHOW_NETANYNONTEAMGAME) {
            if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum > GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NETANYNONTEAMGAME;
        }
        if (flags & UI_SHOW_NEWHIGHSCORE) {
            if (uiInfo.newHighScoreTime < uiInfo.uiDC.realTime) {
                vis = qfalse;
            } else {
                if (uiInfo.soundHighScore) {
                    if (trap_Cvar_VariableValue("sv_killserver") == 0) {
                        trap_S_StartLocalSound(uiInfo.newHighScoreSound, CHAN_ANNOUNCER);
                        uiInfo.soundHighScore = qfalse;
                    }
                }
            }
            flags &= ~UI_SHOW_NEWHIGHSCORE;
        }
        if (flags & UI_SHOW_NEWBESTTIME) {
            if (uiInfo.newBestTime < uiInfo.uiDC.realTime) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NEWBESTTIME;
        }
        if (flags & UI_SHOW_DEMOAVAILABLE) {
            if (!uiInfo.demoAvailable) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_DEMOAVAILABLE;
        }
        if (flags & UI_SHOW_FAVORITESERVERS) {
            if (ui_netSource.integer != AS_FAVORITES) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_FAVORITESERVERS;
        }
        if (flags & UI_SHOW_NOTFAVORITESERVERS) {
            if (ui_netSource.integer == AS_FAVORITES) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NOTFAVORITESERVERS;
        }
        if (flags & UI_SHOW_IF_WARMUP) {
            if (trap_Cvar_VariableValue("ui_warmup") >= 0) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_IF_WARMUP;
        }
        if (flags & UI_SHOW_IF_NOT_WARMUP) {
            if (trap_Cvar_VariableValue("ui_warmup") < 0) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_IF_NOT_WARMUP;
        }
        if (flags & UI_SHOW_IF_LOADOUT_ENABLED) {
            if (trap_Cvar_VariableValue("cg_loadout") != 1) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_IF_LOADOUT_ENABLED;
        }
        if (flags & UI_SHOW_IF_LOADOUT_DISABLED) {
            if (trap_Cvar_VariableValue("cg_loadout") == 1) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_IF_LOADOUT_DISABLED;
        }
        if (flags & UI_SHOW_IF_NOT_INTERMISSION) {
            if (trap_Cvar_VariableValue("ui_intermission") == 1) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_IF_NOT_INTERMISSION;
        }
        if (!(flags & (UI_SHOW_FFA | UI_SHOW_NOTFFA | UI_SHOW_LEADER | UI_SHOW_NOTLEADER |
                       UI_SHOW_ANYTEAMGAME | UI_SHOW_ANYNONTEAMGAME | UI_SHOW_NETANYTEAMGAME |
                       UI_SHOW_NETANYNONTEAMGAME | UI_SHOW_NEWHIGHSCORE | UI_SHOW_NEWBESTTIME |
                       UI_SHOW_DEMOAVAILABLE | UI_SHOW_FAVORITESERVERS | UI_SHOW_NOTFAVORITESERVERS |
                       UI_SHOW_IF_WARMUP | UI_SHOW_IF_NOT_WARMUP |
                       UI_SHOW_IF_LOADOUT_ENABLED | UI_SHOW_IF_LOADOUT_DISABLED |
                       UI_SHOW_IF_NOT_INTERMISSION))) {
            flags = 0;
        }
    }
    return vis;
}

static qboolean UI_Handicap_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int h;

        h = Com_Clamp(5, 100, trap_Cvar_VariableValue("handicap"));
        h += 5 * select;

        if (h > 100) {
            h = 5;
        } else if (h < 0) {
            h = 100;
        }

        trap_Cvar_SetValue("handicap", h);
        return qtrue;
    }
    return qfalse;
}

static void UI_SetScoreBoardCvars(const postGameInfo_t* info, qboolean redBlue) {
    trap_Cvar_Set("ui_scoreAccuracy", va("%i%%", info->accuracy));
    trap_Cvar_Set("ui_scoreImpressives", va("%i", info->impressives));
    trap_Cvar_Set("ui_scoreExcellents", va("%i", info->excellents));
    trap_Cvar_Set("ui_scoreDefends", va("%i", info->defends));
    trap_Cvar_Set("ui_scoreAssists", va("%i", info->assists));
    trap_Cvar_Set("ui_scoreGauntlets", va("%i", info->gauntlets));
    trap_Cvar_Set("ui_scoreScore", va("%i", info->score));
    trap_Cvar_Set("ui_scorePerfect", va("%i", info->perfects));
    trap_Cvar_Set("ui_scoreTeam", va("%i to %i", info->redScore, info->blueScore));
    trap_Cvar_Set("ui_scoreBase", va("%i", info->baseScore));
    trap_Cvar_Set("ui_scoreTimeBonus", va("%i", info->timeBonus));
    trap_Cvar_Set("ui_scoreSkillBonus", va("%i", info->skillBonus));
    trap_Cvar_Set("ui_scoreShutoutBonus", va("%i", info->shutoutBonus));
    trap_Cvar_Set("ui_scoreTime", va("%02i:%02i", info->time / 60, info->time % 60));
    trap_Cvar_Set("ui_scoreCaptures", va("%i", info->captures));
    if (redBlue) {
        trap_Cvar_Set("ui_scoreAccuracy2", va("%i%%", info->accuracy));
        trap_Cvar_Set("ui_scoreImpressives2", va("%i", info->impressives));
        trap_Cvar_Set("ui_scoreExcellents2", va("%i", info->excellents));
        trap_Cvar_Set("ui_scoreDefends2", va("%i", info->defends));
        trap_Cvar_Set("ui_scoreAssists2", va("%i", info->assists));
        trap_Cvar_Set("ui_scoreGauntlets2", va("%i", info->gauntlets));
        trap_Cvar_Set("ui_scoreScore2", va("%i", info->score));
        trap_Cvar_Set("ui_scorePerfect2", va("%i", info->perfects));
        trap_Cvar_Set("ui_scoreTeam2", va("%i to %i", info->redScore, info->blueScore));
        trap_Cvar_Set("ui_scoreBase2", va("%i", info->baseScore));
        trap_Cvar_Set("ui_scoreTimeBonus2", va("%i", info->timeBonus));
        trap_Cvar_Set("ui_scoreSkillBonus2", va("%i", info->skillBonus));
        trap_Cvar_Set("ui_scoreShutoutBonus2", va("%i", info->shutoutBonus));
        trap_Cvar_Set("ui_scoreTime2", va("%02i:%02i", info->time / 60, info->time % 60));
        trap_Cvar_Set("ui_scoreCaptures2", va("%i", info->captures));
    }
}

static void UI_SPPostgameMenu_f(const char* mapname, int gameType) {
    postGameInfo_t info;
    char fileName[MAX_QPATH];
    fileHandle_t f;
    int size;

    memset(&info, 0, sizeof(info));
    Com_sprintf(fileName, sizeof(fileName), "games/%s_%i.game", mapname, gameType);
    if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0) {
        size = 0;
        trap_FS_Read(&size, sizeof(int), f);
        if (size == sizeof(postGameInfo_t)) {
            trap_FS_Read(&info, sizeof(postGameInfo_t), f);
        }
        trap_FS_FCloseFile(f);
    }
    UI_SetScoreBoardCvars(&info, qfalse);
}

static qboolean UI_Effects_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        uiInfo.effectsColor += select;

        if (uiInfo.effectsColor > 6) {
            uiInfo.effectsColor = 0;
        } else if (uiInfo.effectsColor < 0) {
            uiInfo.effectsColor = 6;
        }

        trap_Cvar_SetValue("color1", uitogamecode[uiInfo.effectsColor]);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_GameType_HandleKey(int flags, float* special, int key, qboolean resetMap) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int oldCount = UI_MapCountByGameType(qtrue);

        if (select < 0) {
            ui_gameType.integer--;
            if (ui_gameType.integer == 2) {
                ui_gameType.integer = 1;
            } else if (ui_gameType.integer < 2) {
                ui_gameType.integer = uiInfo.numGameTypes - 1;
            }
        } else {
            ui_gameType.integer++;
            if (ui_gameType.integer >= uiInfo.numGameTypes) {
                ui_gameType.integer = 1;
            } else if (ui_gameType.integer == 2) {
                ui_gameType.integer = 3;
            }
        }

        trap_Cvar_SetValue("ui_gameType", ui_gameType.integer);
        UI_SetCapFragLimits(qtrue);
        UI_SPPostgameMenu_f(uiInfo.mapList[ui_currentMap.integer].mapLoadName,
                            uiInfo.gameTypes[ui_gameType.integer].gtEnum);
        if (resetMap && oldCount != UI_MapCountByGameType(qtrue)) {
            trap_Cvar_SetValue("ui_currentMap", 0);
            Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, NULL);
        }
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_NetGameType_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        ui_netGameType.integer += select;

        while (1) {
            int gtEnum;
            if (ui_netGameType.integer < 0) {
                ui_netGameType.integer = uiInfo.numGameTypes - 1;
            } else if (ui_netGameType.integer >= uiInfo.numGameTypes) {
                ui_netGameType.integer = 0;
            }
            gtEnum = uiInfo.gameTypes[ui_netGameType.integer].gtEnum;
            if (gtEnum != 6 && gtEnum != 7 && gtEnum != 8) {
                break;
            }
            ui_netGameType.integer++;
        }

        trap_Cvar_SetValue("ui_netGametype", ui_netGameType.integer);
        trap_Cvar_SetValue("ui_actualNetGametype", uiInfo.gameTypes[ui_netGameType.integer].gtEnum);
        trap_Cvar_SetValue("ui_currentNetMap", 0);
        UI_MapCountByGameType(qfalse);
        Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, NULL);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_JoinGameType_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        ui_joinGameType.integer += select;

        if (ui_joinGameType.integer < 0) {
            ui_joinGameType.integer = uiInfo.numJoinGameTypes - 1;
        } else if (ui_joinGameType.integer >= uiInfo.numJoinGameTypes) {
            ui_joinGameType.integer = 0;
        }

        trap_Cvar_SetValue("ui_joinGametype", ui_joinGameType.integer);
        UI_BuildServerDisplayList(qtrue);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_TeamName_HandleKey(int flags, float* special, int key, qboolean blue) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int i;

        i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueteam" : "ui_redteam"));
        i += select;

        if (i >= uiInfo.teamCount) {
            i = 0;
        } else if (i < 0) {
            i = uiInfo.teamCount - 1;
        }

        trap_Cvar_Set((blue) ? "ui_blueteam" : "ui_redteam", uiInfo.teamList[i].teamName);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_OpponentName_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        if (select < 0) {
            UI_PriorOpponent();
        } else {
            UI_NextOpponent();
        }
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_BotName_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int numBots = UI_GetNumBots();

        uiInfo.botIndex += select;

        if (uiInfo.botIndex >= numBots + 2) {
            uiInfo.botIndex = 0;
        } else if (uiInfo.botIndex < 0) {
            uiInfo.botIndex = numBots + 1;
        }
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_RedBlue_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        uiInfo.redBlue ^= 1;
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_Crosshair_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        uiInfo.currentCrosshair += select;

        if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
            uiInfo.currentCrosshair = 0;
        } else if (uiInfo.currentCrosshair < 0) {
            uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
        }
        trap_Cvar_SetValue("cg_drawCrosshair", uiInfo.currentCrosshair);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_SelectedPlayer_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int selected;

        UI_BuildPlayerList();
        if (!uiInfo.teamLeader) {
            return qfalse;
        }
        selected = trap_Cvar_VariableValue("cg_selectedPlayer");

        selected += select;

        if (selected > uiInfo.myTeamCount) {
            selected = 0;
        } else if (selected < 0) {
            selected = uiInfo.myTeamCount;
        }

        if (selected == uiInfo.myTeamCount) {
            trap_Cvar_Set("cg_selectedPlayerName", "Everyone");
        } else {
            trap_Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[selected]);
        }
        trap_Cvar_SetValue("cg_selectedPlayer", selected);
    }
    return qfalse;
}

static qboolean UI_CrosshairColor_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int color = (int)trap_Cvar_VariableValue("cg_crosshairColor");
        color += select;
        if (color >= 27) {
            color = 1;
        } else if (color < 1) {
            color = 26;
        }
        trap_Cvar_SetValue("cg_crosshairColor", color);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_Skill_HandleKey(int flags, float *special, int key) {
    int skill;

    if (key != K_MOUSE1 && key != K_MOUSE2 && key != K_ENTER && key != K_JOY1) {
        return qfalse;
    }
    skill = (int)trap_Cvar_VariableValue("g_spSkill");
    if (key == K_MOUSE2) {
        skill--;
    } else {
        skill++;
    }
    if (skill < 1) {
        skill = 5;
    } else if (skill > 5) {
        skill = 1;
    }
    trap_Cvar_Set("g_spSkill", va("%i", skill));
    return qtrue;
}

static qboolean UI_NetSource_HandleKey(int flags, float *special, int key) {
    qboolean skipped = qfalse;

    if (key == K_MOUSE1 || key == K_ENTER || key == K_JOY1) {
        ui_netSource.integer++;
        if (ui_netSource.integer == 1) {
            ui_netSource.integer = 2;
            skipped = qtrue;
        }
    } else if (key == K_MOUSE2) {
        ui_netSource.integer--;
        if (ui_netSource.integer == 1) {
            ui_netSource.integer = 0;
            skipped = qtrue;
        }
    } else {
        return qfalse;
    }

    if (!skipped) {
        if (ui_netSource.integer >= 4) {
            ui_netSource.integer = 0;
        } else if (ui_netSource.integer < 0) {
            ui_netSource.integer = 3;
        }
    }

    UI_BuildServerDisplayList(qtrue);
    if (ui_netSource.integer != AS_GLOBAL) {
        UI_StartServerRefresh(qtrue);
    }
    trap_Cvar_Set("ui_netSource", va("%d", ui_netSource.integer));
    return qtrue;
}

static qboolean UI_ServerFilter_HandleKey(int flags, float *special, int key) {
    if (key == K_MOUSE1 || key == K_ENTER || key == K_JOY1) {
        ui_serverFilterType.integer++;
    } else if (key == K_MOUSE2) {
        ui_serverFilterType.integer--;
    } else {
        return qfalse;
    }
    if (ui_serverFilterType.integer > 6) {
        ui_serverFilterType.integer = 0;
    } else if (ui_serverFilterType.integer < 0) {
        ui_serverFilterType.integer = 6;
    }
    UI_BuildServerDisplayList(qtrue);
    return qtrue;
}

static qboolean UI_OwnerDrawHandleKey(int ownerDraw, int flags, float* special, int key) {
    if (UI_IsWebPage(ownerDraw) && UI_Home_ErrorActive()) {
        return UI_Home_ErrorKey(key);
    }
    switch (ownerDraw) {
        case UI_STARTMATCH:
            return UI_StartMatch_HandleKey(key);
        case UI_SETTINGS:
            return UI_Settings_HandleKey(key);
        case UI_HOME:
            return UI_Home_HandleKey(key);
        case UI_REPLAYS:
            return UI_Replays_HandleKey(key);
        case UI_BROWSER:
            return UI_Browser_HandleKey(key);
        case UI_TRAINING:
            return UI_Training_HandleKey(key);
        case UI_STATS:
            return UI_Stats_HandleKey(key);
        case UI_MATCHES:
            return UI_Matches_HandleKey(key);
        case UI_LEGAL:
            return UI_Legal_HandleKey(key);
        case UI_HANDICAP:
            return UI_Handicap_HandleKey(flags, special, key);
            break;
        case UI_GAMETYPE:
            return UI_GameType_HandleKey(flags, special, key, qtrue);
            break;
        case UI_NETGAMETYPE:
            return UI_NetGameType_HandleKey(flags, special, key);
            break;
        case UI_JOINGAMETYPE:
            return UI_JoinGameType_HandleKey(flags, special, key);
            break;
        case UI_OPPONENT_NAME:
            UI_OpponentName_HandleKey(flags, special, key);
            break;
        case UI_BOTNAME:
            return UI_BotName_HandleKey(flags, special, key);
            break;
        case UI_REDBLUE:
            UI_RedBlue_HandleKey(flags, special, key);
            break;
        case UI_CROSSHAIR:
            UI_Crosshair_HandleKey(flags, special, key);
            break;
        case UI_CROSSHAIR_COLOR:
            if (trap_Cvar_VariableValue("cg_crosshairHealth") == 0) {
                return UI_CrosshairColor_HandleKey(flags, special, key);
            }
            break;
        case UI_SELECTEDPLAYER:
            UI_SelectedPlayer_HandleKey(flags, special, key);
            break;
        case UI_BOTSKILL: {
            int select = UI_SelectForKey(key);
            if (select != 0) {
                uiInfo.skillIndex += select;
                if (uiInfo.skillIndex >= numSkillLevels) {
                    uiInfo.skillIndex = 0;
                } else if (uiInfo.skillIndex < 0) {
                    uiInfo.skillIndex = numSkillLevels - 1;
                }
                return qtrue;
            }
            break;
        }
        case UI_SKILL:
            return UI_Skill_HandleKey(flags, special, key);
            break;
        case UI_NETSOURCE:
            return UI_NetSource_HandleKey(flags, special, key);
            break;
        case UI_NETFILTER:
            return UI_ServerFilter_HandleKey(flags, special, key);
            break;
        default:
            break;
    }

    return qfalse;
}

static float UI_GetValue(int ownerDraw) {
    return 0;
}

static void UI_LoadMods(void) {
    int numdirs;
    char dirlist[2048];
    char* dirptr;
    char* descptr;
    int i;
    int dirlen;

    uiInfo.modCount = 0;
    numdirs = trap_FS_GetFileList("$modlist", "", dirlist, sizeof(dirlist));
    dirptr = dirlist;
    for (i = 0; i < numdirs; i++) {
        dirlen = strlen(dirptr) + 1;
        descptr = dirptr + dirlen;
        uiInfo.modList[uiInfo.modCount].modName = String_Alloc(dirptr);
        uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
        dirptr += dirlen + strlen(descptr) + 1;
        uiInfo.modCount++;
        if (uiInfo.modCount >= MAX_MODS) {
            break;
        }
    }
}

static void UI_LoadTeams(void) {
    char teamList[4096];
    char* teamName;
    int i, len, count;

    count = trap_FS_GetFileList("", "team", teamList, 4096);

    if (count) {
        teamName = teamList;
        for (i = 0; i < count; i++) {
            len = strlen(teamName);
            UI_ParseTeamInfo(teamName);
            teamName += len + 1;
        }
    }
}

static void UI_LoadMovies(void) {
    char movielist[4096];
    char* moviename;
    int i, len;

    uiInfo.movieCount = trap_FS_GetFileList("video", "roq", movielist, 4096);

    if (uiInfo.movieCount) {
        if (uiInfo.movieCount > MAX_MOVIES) {
            uiInfo.movieCount = MAX_MOVIES;
        }
        moviename = movielist;
        for (i = 0; i < uiInfo.movieCount; i++) {
            len = strlen(moviename);
            if (!Q_stricmp(moviename + len - 4, ".roq")) {
                moviename[len - 4] = '\0';
            }
            Q_strupr(moviename);
            uiInfo.movieList[i] = String_Alloc(moviename);
            moviename += len + 1;
        }
    }
}

#define NAMEBUFSIZE (MAX_DEMOS * 32)

void UI_LoadDemos(void) {
    char demolist[NAMEBUFSIZE];
    char demoExt[32];
    char demoSuffix[32];
    char* demoname;
    int i, len;
    int protocol;

    protocol = (int)trap_Cvar_VariableValue("protocol");

    Com_sprintf(demoExt, sizeof(demoExt), "dm_%d", protocol);
    uiInfo.demoCount = trap_FS_GetFileList("demos", demoExt, demolist, ARRAY_LEN(demolist));
    Com_sprintf(demoSuffix, sizeof(demoSuffix), ".dm_%d", protocol);

    if (uiInfo.demoCount > 256)
        uiInfo.demoCount = 256;

    demoname = demolist;
    for (i = 0; i < uiInfo.demoCount; i++) {
        len = strlen(demoname);
        if (len >= (int)strlen(demoSuffix)
            && Q_stricmp(demoname + len - strlen(demoSuffix), demoSuffix) == 0) {
            demoname[len - strlen(demoSuffix)] = '\0';
        }
        Q_strupr(demoname);
        uiInfo.demoList[i] = String_Alloc(demoname);
        demoname += len + 1;
    }
}

static qboolean UI_SetNextMap(int actual, int index) {
    int i;
    for (i = actual + 1; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            Menu_SetFeederSelection(NULL, FEEDER_MAPS, index + 1, "skirmish");
            return qtrue;
        }
    }
    return qfalse;
}

static const char* UI_DefaultFactory(int gametype) {
    static const char* factories[GT_MAX_GAME_TYPE] = {
        "ffa", "duel", "race", "tdm", "ca", "ctf", "oneflag", "", "har", "ft", "dom", "ad", "rr"};

    if (gametype < 0 || gametype >= GT_MAX_GAME_TYPE) {
        return "";
    }
    return factories[gametype];
}

static void UI_StartSkirmish(qboolean next) {
    int i, k, g, delay, temp;
    float skill;
    char buff[MAX_STRING_CHARS];

    if (next) {
        int actual;
        int index = trap_Cvar_VariableValue("ui_mapIndex");
        UI_MapCountByGameType(qtrue);
        UI_SelectedMap(index, &actual);
        if (UI_SetNextMap(actual, index)) {
        } else {
            UI_GameType_HandleKey(0, NULL, K_MOUSE1, qfalse);
            UI_MapCountByGameType(qtrue);
            Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, "skirmish");
        }
    }

    g = uiInfo.gameTypes[ui_gameType.integer].gtEnum;
    trap_Cmd_ExecuteText(EXEC_APPEND, va("wait ; wait ; map %s %s\n", uiInfo.mapList[ui_currentMap.integer].mapLoadName, UI_DefaultFactory(g)));
    skill = trap_Cvar_VariableValue("g_spSkill");
    trap_Cvar_Set("ui_scoreMap", uiInfo.mapList[ui_currentMap.integer].mapName);

    k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));

    temp = trap_Cvar_VariableValue("capturelimit");
    trap_Cvar_Set("ui_saveCaptureLimit", va("%i", temp));
    temp = trap_Cvar_VariableValue("fraglimit");
    trap_Cvar_Set("ui_saveFragLimit", va("%i", temp));

    UI_SetCapFragLimits(qfalse);

    temp = trap_Cvar_VariableValue("cg_drawTimer");
    trap_Cvar_Set("ui_drawTimer", va("%i", temp));
    temp = trap_Cvar_VariableValue("g_doWarmup");
    trap_Cvar_Set("ui_doWarmup", va("%i", temp));
    temp = trap_Cvar_VariableValue("g_friendlyFire");
    trap_Cvar_Set("ui_friendlyFire", va("%i", temp));
    temp = trap_Cvar_VariableValue("sv_maxclients");
    trap_Cvar_Set("ui_maxClients", va("%i", temp));
    temp = trap_Cvar_VariableValue("g_warmup");
    trap_Cvar_Set("ui_warmup", va("%i", temp));
    temp = trap_Cvar_VariableValue("sv_pure");
    trap_Cvar_Set("ui_pure", va("%i", temp));

    trap_Cvar_Set("cg_cameraOrbit", "0");
    trap_Cvar_Set("cg_thirdPerson", "0");
    trap_Cvar_Set("cg_drawTimer", "1");
    trap_Cvar_Set("g_doWarmup", "1");
    trap_Cvar_Set("g_warmup", "15");
    trap_Cvar_Set("sv_pure", "0");
    trap_Cvar_Set("g_friendlyFire", "0");
    trap_Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
    trap_Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));

    if (trap_Cvar_VariableValue("ui_recordSPDemo")) {
        Com_sprintf(buff, MAX_STRING_CHARS, "%s_%i", uiInfo.mapList[ui_currentMap.integer].mapLoadName, g);
        trap_Cvar_Set("ui_recordSPDemoName", buff);
    }

    delay = 500;

    if (g == GT_DUEL) {
        trap_Cvar_Set("sv_maxclients", "2");
        Com_sprintf(buff, sizeof(buff),
                    "wait ; addbot %s %f "
                    ", %i \n",
                    uiInfo.mapList[ui_currentMap.integer].opponentName, skill, delay);
        trap_Cmd_ExecuteText(EXEC_APPEND, buff);
    } else {
        temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
        trap_Cvar_Set("sv_maxclients", va("%d", temp));
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers; i++) {
            Com_sprintf(buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_FFA) ? "" : "Blue", delay, uiInfo.teamList[k].teamMembers[i]);
            trap_Cmd_ExecuteText(EXEC_APPEND, buff);
            delay += 500;
        }
        k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers - 1; i++) {
            Com_sprintf(buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_FFA) ? "" : "Red", delay, uiInfo.teamList[k].teamMembers[i]);
            trap_Cmd_ExecuteText(EXEC_APPEND, buff);
            delay += 500;
        }
    }
    if (g >= GT_TEAM) {
        trap_Cmd_ExecuteText(EXEC_APPEND, "wait 5; team Red\n");
    }
}

static void UI_Update(const char* name) {
    int val = trap_Cvar_VariableValue(name);

    if (Q_stricmp(name, "ui_SetName") == 0) {
        trap_Cvar_Set("name", UI_Cvar_VariableString("ui_Name"));
    } else if (Q_stricmp(name, "ui_setRate") == 0) {
        float rate = trap_Cvar_VariableValue("rate");
        if (rate >= 5000) {
            trap_Cvar_Set("cl_maxpackets", "30");
            trap_Cvar_Set("cl_packetdup", "1");
        } else if (rate >= 4000) {
            trap_Cvar_Set("cl_maxpackets", "15");
            trap_Cvar_Set("cl_packetdup", "2");
        } else {
            trap_Cvar_Set("cl_maxpackets", "15");
            trap_Cvar_Set("cl_packetdup", "1");
        }
    } else if (Q_stricmp(name, "ui_GetName") == 0) {
        trap_Cvar_Set("ui_Name", UI_Cvar_VariableString("name"));
    } else if (Q_stricmp(name, "r_colorbits") == 0) {
        switch (val) {
            case 0:
                trap_Cvar_Set("r_depthbits", "0");
                trap_Cvar_Set("r_stencilbits", "0");
                break;
            case 16:
                trap_Cvar_Set("r_depthbits", "16");
                trap_Cvar_Set("r_stencilbits", "0");
                break;
            case 32:
                trap_Cvar_Set("r_depthbits", "24");
                break;
        }
    } else if (Q_stricmp(name, "ui_mousePitch") == 0) {
        if (val == 0) {
            trap_Cvar_Set("m_pitch", "0.022");
        } else {
            trap_Cvar_Set("m_pitch", "-0.022");
        }
    }
}

static const char* UI_GetGameTypeString(int gametype) {
    switch (gametype) {
        case 0: return "ffa";
        case 1: return "duel";
        case 2: return "race";
        case 3: return "tdm";
        case 4: return "ca";
        case 5: return "ctf";
        case 6: return "oneflag";
        case 8: return "har";
        case 9: return "ft";
        case 10: return "dom";
        case 11: return "ad";
        case 12: return "rr";
        default: return "";
    }
}

static void UI_RunMenuScript(char** args) {
    const char *name, *name2;
    char buff[1024];

    if (String_Parse(args, &name)) {
        if (Q_stricmp(name, "StartServer") == 0) {
            int i, clients, count, g, v;
            float skill;
            char botBuff[1024];
            trap_Cvar_Set("cg_thirdPerson", "0");
            trap_Cvar_Set("cg_cameraOrbit", "0");
            trap_Cvar_Set("ui_singlePlayerActive", "0");
            trap_Cvar_SetValue("dedicated", Com_Clamp(0, 2, ui_dedicated.integer));
            g = (int)Com_Clamp(0, GT_MAX_GAME_TYPE, uiInfo.gameTypes[ui_netGameType.integer].gtEnum);
            trap_Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
            trap_Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));
            trap_Cmd_ExecuteText(EXEC_APPEND, va("wait ; wait ; map %s %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName, UI_DefaultFactory(g)));
            skill = trap_Cvar_VariableValue("g_spSkill");
            clients = (int)trap_Cvar_VariableValue("sv_maxclients");
            count = 0;
            for (i = 1; i <= 5; i++) {
                if ((int)trap_Cvar_VariableValue(va("ui_blueteam%i", i)) >= 0) {
                    count++;
                }
                if ((int)trap_Cvar_VariableValue(va("ui_redteam%i", i)) >= 0) {
                    count++;
                }
            }
            if (count == 0) {
                count = 8;
            }
            if (count < clients) {
                count = clients;
            }
            trap_Cvar_Set("sv_maxclients", va("%d", count));
            for (i = 1; i <= 5; i++) {
                v = (int)trap_Cvar_VariableValue(va("ui_blueteam%i", i));
                if (v > 1) {
                    if (g < GT_TEAM) {
                        Com_sprintf(botBuff, sizeof(botBuff), "addbot %s %f \n", UI_GetBotNameByNumber(v - 2), skill);
                    } else {
                        Com_sprintf(botBuff, sizeof(botBuff), "addbot %s %f %s\n", uiInfo.characterList[v].base, skill, "Blue");
                    }
                    trap_Cmd_ExecuteText(EXEC_APPEND, botBuff);
                }
                v = (int)trap_Cvar_VariableValue(va("ui_redteam%i", i));
                if (v > 1) {
                    if (g < GT_TEAM) {
                        Com_sprintf(botBuff, sizeof(botBuff), "addbot %s %f \n", UI_GetBotNameByNumber(v - 2), skill);
                    } else {
                        Com_sprintf(botBuff, sizeof(botBuff), "addbot %s %f %s\n", uiInfo.characterList[v].base, skill, "Red");
                    }
                    trap_Cmd_ExecuteText(EXEC_APPEND, botBuff);
                }
            }
        } else if (Q_stricmp(name, "resetDefaults") == 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "exec default.cfg\n");
            trap_Cmd_ExecuteText(EXEC_APPEND, "cvar_restart\n");
            Controls_SetDefaults();
            trap_Cvar_Set("com_introPlayed", "1");
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
        } else if (Q_stricmp(name, "startMatchOpen") == 0) {
            UI_StartMatch_Open();
        } else if (Q_stricmp(name, "replaysOpen") == 0) {
            UI_Replays_Open();
        } else if (Q_stricmp(name, "browserOpen") == 0) {
            UI_Browser_Open();
        } else if (Q_stricmp(name, "trainingOpen") == 0) {
            UI_Training_Open();
        } else if (Q_stricmp(name, "statsOpen") == 0) {
            UI_Stats_Open();
        } else if (Q_stricmp(name, "matchesOpen") == 0) {
            UI_Matches_Open();
        } else if (Q_stricmp(name, "legalOpen") == 0) {
            UI_Legal_Open();
        } else if (Q_stricmp(name, "homeEscape") == 0) {
            if (UI_Home_InMatch()) {
                _UI_SetActiveMenu(UIMENU_INGAME);
            } else {
                Menus_ActivateByName("quit_popmenu");
            }
        } else if (Q_stricmp(name, "homeOpen") == 0) {
            menuDef_t* home = Menus_FindByName("main");
            itemDef_t* item = home ? Menu_FindItemByName(home, "home") : NULL;

            if (item) {
                item->window.flags |= WINDOW_HASFOCUS;
            }
            UI_Home_Open();
        } else if (Q_stricmp(name, "settingsOpen") == 0) {
            UI_Settings_Open();
        } else if (Q_stricmp(name, "settingsClose") == 0) {
            UI_Settings_Close();
        } else if (Q_stricmp(name, "loadArenas") == 0) {
            UI_LoadArenasIntoMapList();
            UI_MapCountByGameType(qfalse);
            Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, "createserver");
        } else if (Q_stricmp(name, "updateCallvoteMapPreview") == 0) {
            UI_MapCountByCallvoteGameType();
            Menu_SetFeederSelection(NULL, FEEDER_CVMAPS, 0, "ingame_callvote");
        } else if (Q_stricmp(name, "saveControls") == 0) {
            Controls_SetConfig(qtrue);
        } else if (Q_stricmp(name, "loadControls") == 0) {
            Controls_GetConfig();
        } else if (Q_stricmp(name, "clearError") == 0) {
            trap_Cvar_Set("com_errorMessage", "");
        } else if (Q_stricmp(name, "loadGameInfo") == 0) {
            UI_ParseGameInfo("gameinfo.txt");
        } else if (Q_stricmp(name, "LoadDemos") == 0) {
            UI_LoadDemos();
        } else if (Q_stricmp(name, "LoadMovies") == 0) {
            UI_LoadMovies();
        } else if (Q_stricmp(name, "LoadMods") == 0) {
            UI_LoadMods();
        } else if (Q_stricmp(name, "RunMod") == 0) {
            trap_Cvar_Set("fs_game", uiInfo.modList[uiInfo.modIndex].modName);
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
        } else if (Q_stricmp(name, "RunDemo") == 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, va("demo %s\n", uiInfo.demoList[uiInfo.demoIndex]));
        } else if (Q_stricmp(name, "closeJoin") == 0) {
            if (uiInfo.serverStatus.refreshActive) {
                UI_ServerRefreshComplete();
                uiInfo.serverStatus.nextDisplayRefresh = 0;
                uiInfo.nextServerStatusRefresh = 0;
                uiInfo.nextFindPlayerRefresh = 0;
            } else {
                Menus_CloseByName("joinserver");
                Menus_OpenByName("main");
            }
        } else if (Q_stricmp(name, "UpdateFilter") == 0) {
            if (ui_netSource.integer == AS_LOCAL) {
                UI_StartServerRefresh(qtrue);
            }
            UI_BuildServerDisplayList(qtrue);
            UI_FeederSelection(FEEDER_SERVERS, 0);
        } else if (Q_stricmp(name, "RefreshServers") == 0) {
            UI_StartServerRefresh(qtrue);
            UI_BuildServerDisplayList(qtrue);
        } else if (Q_stricmp(name, "RefreshFilter") == 0) {
            UI_StartServerRefresh(qfalse);
            UI_BuildServerDisplayList(qtrue);
        } else if (Q_stricmp(name, "StopRefresh") == 0) {
            UI_ServerRefreshComplete();
            uiInfo.serverStatus.nextDisplayRefresh = 0;
            uiInfo.nextServerStatusRefresh = 0;
            uiInfo.nextFindPlayerRefresh = 0;
        } else if (Q_stricmp(name, "ServerSort") == 0) {
            int sortColumn;
            if (Int_Parse(args, &sortColumn)) {
                if (sortColumn == uiInfo.serverStatus.sortKey) {
                    uiInfo.serverStatus.sortDir = !uiInfo.serverStatus.sortDir;
                }
                UI_ServersSort(sortColumn, qtrue);
            }
        } else if (Q_stricmp(name, "FindPlayer") == 0) {
            UI_BuildFindPlayerList(qtrue);
            uiInfo.serverStatusInfo.numLines = 0;
            Menu_SetFeederSelection(NULL, FEEDER_FINDPLAYER, 0, NULL);
        } else if (Q_stricmp(name, "ServerStatus") == 0) {
            trap_LAN_GetServerAddressString(ui_netSource.integer,
                uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer],
                uiInfo.serverStatusAddress, sizeof(uiInfo.serverStatusAddress));
            UI_BuildServerStatus(qtrue);
        } else if (Q_stricmp(name, "FoundPlayerServerStatus") == 0) {
            Q_strncpyz(uiInfo.serverStatusAddress,
                uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer],
                sizeof(uiInfo.serverStatusAddress));
            UI_BuildServerStatus(qtrue);
            Menu_SetFeederSelection(NULL, FEEDER_FINDPLAYER, 0, NULL);
        } else if (Q_stricmp(name, "addFavorite") == 0) {
            if (ui_netSource.integer != AS_FAVORITES) {
                char favName[MAX_NAME_LENGTH];
                char favAddr[MAX_NAME_LENGTH];
                int res;
                char info[MAX_STRING_CHARS];
                trap_LAN_GetServerInfo(ui_netSource.integer,
                    uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer],
                    info, sizeof(info));
                favName[0] = favAddr[0] = '\0';
                Q_strncpyz(favName, Info_ValueForKey(info, "hostname"), sizeof(favName));
                Q_strncpyz(favAddr, Info_ValueForKey(info, "addr"), sizeof(favAddr));
                if (strlen(favName) > 0 && strlen(favAddr) > 0) {
                    res = trap_LAN_AddServer(AS_FAVORITES, favName, favAddr);
                    if (res == 0) {
                        Com_Printf("Favorite already in list\n");
                    } else if (res == -1) {
                        Com_Printf("Favorite list full\n");
                    } else {
                        Com_Printf("Added favorite server %s\n", favAddr);
                    }
                }
            }
        } else if (Q_stricmp(name, "deleteFavorite") == 0) {
            if (ui_netSource.integer == AS_FAVORITES) {
                char favAddr[MAX_NAME_LENGTH];
                char info[MAX_STRING_CHARS];
                trap_LAN_GetServerInfo(ui_netSource.integer,
                    uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer],
                    info, sizeof(info));
                favAddr[0] = '\0';
                Q_strncpyz(favAddr, Info_ValueForKey(info, "addr"), sizeof(favAddr));
                if (strlen(favAddr) > 0) {
                    trap_LAN_RemoveServer(AS_FAVORITES, favAddr);
                }
            }
        } else if (Q_stricmp(name, "createFavorite") == 0) {
            if (ui_netSource.integer == AS_FAVORITES) {
                char favName[MAX_NAME_LENGTH];
                char favAddr[MAX_NAME_LENGTH];
                int res;
                favName[0] = favAddr[0] = '\0';
                Q_strncpyz(favName, UI_Cvar_VariableString("ui_favoriteName"), sizeof(favName));
                Q_strncpyz(favAddr, UI_Cvar_VariableString("ui_favoriteAddress"), sizeof(favAddr));
                if (strlen(favName) > 0 && strlen(favAddr) > 0) {
                    res = trap_LAN_AddServer(AS_FAVORITES, favName, favAddr);
                    if (res == 0) {
                        Com_Printf("Favorite already in list\n");
                    } else if (res == -1) {
                        Com_Printf("Favorite list full\n");
                    } else {
                        Com_Printf("Added favorite server %s\n", favAddr);
                    }
                }
            }
        } else if (Q_stricmp(name, "JoinServer") == 0) {
            trap_Cvar_Set("cg_thirdPerson", "0");
            trap_Cvar_Set("cg_cameraOrbit", "0");
            trap_Cvar_Set("ui_singlePlayerActive", "0");
            if (uiInfo.serverStatus.currentServer >= 0 && uiInfo.serverStatus.currentServer < uiInfo.serverStatus.numDisplayServers) {
                trap_LAN_GetServerAddressString(ui_netSource.integer,
                    uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer],
                    buff, sizeof(buff));
                trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", buff));
            }
        } else if (Q_stricmp(name, "FoundPlayerJoinServer") == 0) {
            if (uiInfo.currentFoundPlayerServer >= 0 && uiInfo.currentFoundPlayerServer < uiInfo.numFoundPlayerServers) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer]));
            }
        } else if (Q_stricmp(name, "Quit") == 0) {
            trap_Cmd_ExecuteText(EXEC_NOW, "quit");
        } else if (Q_stricmp(name, "Controls") == 0) {
            trap_Cvar_Set("cl_paused", "1");
            trap_Key_SetCatcher(KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("setup_menu2");
        } else if (Q_stricmp(name, "Leave") == 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "disconnect\n");
            trap_Key_SetCatcher(KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("main");
        } else if (Q_stricmp(name, "closeingame") == 0) {
            trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
            trap_Key_ClearStates();
            trap_Cvar_Set("cl_paused", "0");
            Menus_CloseAll();
        } else if (Q_stricmp(name, "voteMap") == 0) {
            if (ui_currentNetMap.integer >= 0 && ui_currentNetMap.integer < uiInfo.mapCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote map %s %s\n",
                    uiInfo.mapList[ui_currentNetMap.integer].mapLoadName,
                    (ui_cvGameType.integer == -1) ? "" : UI_GetGameTypeString(ui_cvGameType.integer)));
            }
        } else if (Q_stricmp(name, "voteKick") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote kick %s\n", uiInfo.playerNames[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "voteGame") == 0) {
            if (ui_netGameType.integer >= 0 && ui_netGameType.integer < uiInfo.numGameTypes) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote g_gametype %i\n", uiInfo.gameTypes[ui_netGameType.integer].gtEnum));
            }
        } else if (Q_stricmp(name, "addBot") == 0) {
            if (trap_Cvar_VariableValue("g_gametype") >= GT_TEAM) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("addbot %s %i %s\n", uiInfo.characterList[uiInfo.botIndex].base, uiInfo.skillIndex + 1, (uiInfo.redBlue == 0) ? "Red" : "Blue"));
            } else {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("addbot %s %i %s\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex + 1, (uiInfo.redBlue == 0) ? "Red" : "Blue"));
            }
        } else if (Q_stricmp(name, "orders") == 0) {
            const char* orders;
            if (String_Parse(args, &orders)) {
                int selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");
                if (selectedPlayer < uiInfo.myTeamCount) {
                    Com_sprintf(buff, sizeof(buff), orders, uiInfo.teamClientNums[selectedPlayer]);
                    trap_Cmd_ExecuteText(EXEC_APPEND, buff);
                    trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
                } else {
                    int i;
                    for (i = 0; i < uiInfo.myTeamCount; i++) {
                        if (uiInfo.playerNumber == uiInfo.teamClientNums[i]) {
                            continue;
                        }
                        Com_sprintf(buff, sizeof(buff), orders, uiInfo.teamClientNums[i]);
                        trap_Cmd_ExecuteText(EXEC_APPEND, buff);
                        trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
                    }
                }
                trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
                trap_Key_ClearStates();
                trap_Cvar_Set("cl_paused", "0");
                Menus_CloseAll();
            }
        } else if (Q_stricmp(name, "glCustom") == 0) {
            trap_Cvar_Set("ui_glCustom", "4");
        } else if (Q_stricmp(name, "update") == 0) {
            if (String_Parse(args, &name2)) {
                UI_Update(name2);
            }
        } else if (Q_stricmp(name, "setPbClStatus") == 0) {
            int stat;
            if (Int_Parse(args, &stat))
                trap_SetPbClStatus(stat);
        } else if (Q_stricmp(name, "clearComError") == 0) {
			trap_Cvar_Set("com_errorMessage", "");
        } else if (Q_stricmp(name, "clientViewProfile") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("clientviewprofile %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "clientFriendInvite") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("clientfriendinvite %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "clientMutePlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("clientmute %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "modPlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("addmod %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "adminPlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("addadmin %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "deopPlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("demote %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "putred") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("put %i r\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "putblue") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("put %i b\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "putspec") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("put %i s\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "mutePlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("mute %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "unmutePlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("unmute %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "tempbanPlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("tempban %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "banPlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("ban %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "kickPlayer") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("clientkick %i\n",
                    uiInfo.clientNums[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "teamModelChanged") == 0) {
            updateModel = qtrue;
        } else if (Q_stricmp(name, "enemyModelChanged") == 0) {
            updateModel = qtrue;
        } else if (Q_stricmp(name, "teamColorDefaults") == 0) {
            trap_Cvar_Set("ui_teamHeadColor", "96");
            trap_Cvar_Set("ui_teamUpperColor", "23");
            trap_Cvar_Set("ui_teamLowerColor", "23");
        } else if (Q_stricmp(name, "enemyColorDefaults") == 0) {
            trap_Cvar_Set("ui_enemyHeadColor", "27");
            trap_Cvar_Set("ui_enemyUpperColor", "2");
            trap_Cvar_Set("ui_enemyLowerColor", "2");
        } else {
            Com_Printf("unknown UI script %s\n", name);
        }
    }
}

static void UI_GetTeamColor(vec4_t* color) {
}

static int UI_MapCountByGameType(qboolean singlePlayer) {
    int i, c, game;
    char info[MAX_STRING_CHARS];
    c = 0;
    game = 0;
    if (trap_GetConfigString(CS_SERVERINFO, info, sizeof(info))) {
        game = atoi(Info_ValueForKey(info, "gametype"));
        if (game < 0) {
            game = 0;
        }
    }

    for (i = 0; i < uiInfo.mapCount; i++) {
        uiInfo.mapList[i].active = qfalse;
        if ((uiInfo.mapList[i].typeBits & (1 << game))
            && (!singlePlayer || (uiInfo.mapList[i].typeBits & 4))) {
            c++;
            uiInfo.mapList[i].active = qtrue;
        }
    }
    return c;
}

static int UI_MapCountByCallvoteGameType(void) {
    int i, c, game;
    char info[MAX_STRING_CHARS];
    c = 0;
    game = 0;
    if (trap_GetConfigString(CS_SERVERINFO, info, sizeof(info))) {
        game = atoi(Info_ValueForKey(info, "gametype"));
        if (game < 0) {
            game = 0;
        }
    }
    if (ui_actualNetGameType.integer != -1) {
        game = ui_actualNetGameType.integer;
    }
    for (i = 0; i < uiInfo.mapCount; i++) {
        uiInfo.mapList[i].active = qfalse;
        if (uiInfo.mapList[i].typeBits & (1 << game)) {
            c++;
            uiInfo.mapList[i].active = qtrue;
        }
    }
    return c;
}

qboolean UI_hasSkinForBase(const char* base, const char* team) {
    char test[MAX_QPATH];

    Com_sprintf(test, sizeof(test), "models/players/%s/%s/lower_default.skin", base, team);

    if (trap_FS_FOpenFile(test, NULL, FS_READ)) {
        return qtrue;
    }
    Com_sprintf(test, sizeof(test), "models/players/characters/%s/%s/lower_default.skin", base, team);

    if (trap_FS_FOpenFile(test, NULL, FS_READ)) {
        return qtrue;
    }
    return qfalse;
}

static int UI_HeadCountByTeam(void) {
    static int init = 0;
    int i, j, k, c, tIndex;

    c = 0;
    if (!init) {
        for (i = 0; i < uiInfo.characterCount; i++) {
            uiInfo.characterList[i].reference = 0;
            for (j = 0; j < uiInfo.teamCount; j++) {
                if (UI_hasSkinForBase(uiInfo.characterList[i].base, uiInfo.teamList[j].teamName)) {
                    uiInfo.characterList[i].reference |= (1 << j);
                }
            }
        }
        init = 1;
    }

    tIndex = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));

    for (i = 0; i < uiInfo.characterCount; i++) {
        uiInfo.characterList[i].active = qfalse;
        for (j = 0; j < TEAM_MEMBERS; j++) {
            if (uiInfo.teamList[tIndex].teamMembers[j] != NULL) {
                if (uiInfo.characterList[i].reference & (1 << tIndex)) {
                    uiInfo.characterList[i].active = qtrue;
                    c++;
                    break;
                }
            }
        }
    }

    for (j = 0; j < TEAM_MEMBERS; j++) {
        for (k = 0; k < uiInfo.aliasCount; k++) {
            if (uiInfo.aliasList[k].name != NULL) {
                if (Q_stricmp(uiInfo.teamList[tIndex].teamMembers[j], uiInfo.aliasList[k].name) == 0) {
                    for (i = 0; i < uiInfo.characterCount; i++) {
                        if (uiInfo.characterList[i].headImage != -1 && uiInfo.characterList[i].reference & (1 << tIndex) && Q_stricmp(uiInfo.aliasList[k].ai, uiInfo.characterList[i].name) == 0) {
                            if (uiInfo.characterList[i].active == qfalse) {
                                uiInfo.characterList[i].active = qtrue;
                                c++;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    return c;
}

static char* stristr(char* str, char* charset) {
    int i;

    while (*str) {
        for (i = 0; charset[i] && str[i]; i++) {
            if (toupper(charset[i]) != toupper(str[i]))
                break;
        }
        if (!charset[i])
            return str;
        str++;
    }
    return NULL;
}

static int QDECL UI_ServerSortCompare(const void *arg1, const void *arg2) {
    return trap_LAN_CompareServers(ui_netSource.integer, uiInfo.serverStatus.sortKey,
                                   uiInfo.serverStatus.sortDir,
                                   *(const int *)arg1, *(const int *)arg2);
}

static void UI_ServersSort(int column, qboolean force) {
    if (!force && uiInfo.serverStatus.sortKey == column) {
        return;
    }
    uiInfo.serverStatus.sortKey = column;
    qsort(&uiInfo.serverStatus.displayServers[0], uiInfo.serverStatus.numDisplayServers,
          sizeof(int), UI_ServerSortCompare);
}

static void UI_InsertServerIntoDisplayList(int num, int position) {
    int i;

    if (position < 0 || position > uiInfo.serverStatus.numDisplayServers) {
        return;
    }
    uiInfo.serverStatus.numDisplayServers++;
    for (i = uiInfo.serverStatus.numDisplayServers; i > position; i--) {
        uiInfo.serverStatus.displayServers[i] = uiInfo.serverStatus.displayServers[i - 1];
    }
    uiInfo.serverStatus.displayServers[position] = num;
}

static void UI_RemoveServerFromDisplayList(int num) {
    int i, j;

    for (i = 0; i < uiInfo.serverStatus.numDisplayServers; i++) {
        if (uiInfo.serverStatus.displayServers[i] == num) {
            uiInfo.serverStatus.numDisplayServers--;
            for (j = i; j < uiInfo.serverStatus.numDisplayServers; j++) {
                uiInfo.serverStatus.displayServers[j] = uiInfo.serverStatus.displayServers[j + 1];
            }
            return;
        }
    }
}

static void UI_BinaryServerInsert(int num) {
    int mid, offset, res, len;

    len = uiInfo.serverStatus.numDisplayServers;
    offset = 0;
    if (len <= 0) {
        UI_InsertServerIntoDisplayList(num, 0);
        return;
    }

    res = 0;
    do {
        mid = len >> 1;
        res = trap_LAN_CompareServers(ui_netSource.integer, uiInfo.serverStatus.sortKey,
                                      uiInfo.serverStatus.sortDir, num,
                                      uiInfo.serverStatus.displayServers[offset + mid]);
        if (res == 0) {
            UI_InsertServerIntoDisplayList(num, offset + mid);
            return;
        }
        len -= mid;
        if (res == 1) {
            offset += mid;
        }
    } while (mid > 0);

    if (res == 1) {
        offset++;
    }
    UI_InsertServerIntoDisplayList(num, offset);
}

static void UI_BuildServerDisplayList(int force) {
    int i, count, clients, maxClients, ping, game, len, visible;
    char info[MAX_STRING_CHARS];
    static int numinvisible;

    if (!force && uiInfo.uiDC.realTime <= uiInfo.serverStatus.nextDisplayRefresh) {
        return;
    }
    if (force == 2) {
        force = 0;
    }

    trap_Cvar_VariableStringBuffer("cl_motdString", uiInfo.serverStatus.motd,
                                   sizeof(uiInfo.serverStatus.motd));
    len = strlen(uiInfo.serverStatus.motd);
    if (len == 0) {
        strcpy(uiInfo.serverStatus.motd, "Welcome to Team Arena!");
        len = strlen(uiInfo.serverStatus.motd);
    }
    if (len != uiInfo.serverStatus.motdLen) {
        uiInfo.serverStatus.motdLen = len;
        uiInfo.serverStatus.motdWidth = -1;
    }

    if (force) {
        numinvisible = 0;
        uiInfo.serverStatus.numDisplayServers = 0;
        uiInfo.serverStatus.numPlayersOnServers = 0;
        Menu_SetFeederSelection(NULL, FEEDER_SERVERS, 0, NULL);
        trap_LAN_MarkServerVisible(ui_netSource.integer, -1, qtrue);
    }

    count = trap_LAN_GetServerCount(ui_netSource.integer);
    if (count == -1 || (ui_netSource.integer == AS_LOCAL && count == 0)) {
        uiInfo.serverStatus.numDisplayServers = 0;
        uiInfo.serverStatus.numPlayersOnServers = 0;
        uiInfo.serverStatus.nextDisplayRefresh = uiInfo.uiDC.realTime + 500;
        return;
    }

    visible = qfalse;
    for (i = 0; i < count; i++) {
        if (!trap_LAN_ServerIsVisible(ui_netSource.integer, i)) {
            continue;
        }
        visible = qtrue;
        ping = trap_LAN_GetServerPing(ui_netSource.integer, i);
        if (ping > 0 || ui_netSource.integer == AS_FAVORITES) {
            trap_LAN_GetServerInfo(ui_netSource.integer, i, info, sizeof(info));

            clients = atoi(Info_ValueForKey(info, "clients"));
            uiInfo.serverStatus.numPlayersOnServers += clients;

            if (ui_browserShowEmpty.integer == 0 && clients == 0) {
                trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
                continue;
            }
            if (ui_browserShowFull.integer == 0) {
                maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
                if (clients == maxClients) {
                    trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
                    continue;
                }
            }
            if (uiInfo.numJoinGameTypes > 0 && uiInfo.joinGameTypes[ui_joinGameType.integer].gtEnum != -1) {
                game = atoi(Info_ValueForKey(info, "gametype"));
                if (game != uiInfo.joinGameTypes[ui_joinGameType.integer].gtEnum) {
                    trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
                    continue;
                }
            }
            if (ui_serverFilterType.integer > 0) {
                if (Q_stricmp(Info_ValueForKey(info, "game"),
                              serverFilters[ui_serverFilterType.integer].basedir) != 0) {
                    trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
                    continue;
                }
            }
            if (ui_netSource.integer == AS_FAVORITES) {
                UI_RemoveServerFromDisplayList(i);
            }
            UI_BinaryServerInsert(i);
            if (ping > 0) {
                trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
                numinvisible++;
            }
        }
    }

    uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime;
}

static void UI_SortServerStatusInfo(serverStatusInfo_t *info) {
    int i, j, index;
    char *tmp1, *tmp2;
    static const char *sortKeys[] = {
        "sv_hostname", "Address", "gamename", "g_gametype", "mapname",
        "version", "protocol", "timelimit", "fraglimit", NULL
    };
    static const char *sortKeyNames[] = {
        "Name", "", "Game name", "Game type", "Map",
        "", "", "", "", NULL
    };

    index = 0;
    for (i = 0; sortKeys[i]; i++) {
        for (j = 0; j < info->numLines; j++) {
            if (info->lines[j][1] && *info->lines[j][1] == '\0'
                && info->lines[j][0] && !Q_stricmp(info->lines[j][0], sortKeys[i])) {
                tmp1 = info->lines[index][0];
                tmp2 = info->lines[index][3];
                info->lines[index][0] = info->lines[j][0];
                info->lines[index][3] = info->lines[j][3];
                info->lines[j][0] = tmp1;
                info->lines[j][3] = tmp2;
                if (strlen(sortKeyNames[i])) {
                    info->lines[index][0] = (char *)sortKeyNames[i];
                }
                index++;
            }
        }
    }
}

static qboolean UI_GetServerStatusInfo(const char *serverAddress, serverStatusInfo_t *info) {
    char *p, *score, *ping, *name;
    int i, len;

    if (!info) {
        trap_LAN_ServerStatus(serverAddress, NULL, 0);
        return qfalse;
    }
    memset(info, 0, sizeof(*info));
    if (trap_LAN_ServerStatus(serverAddress, info->text, sizeof(info->text))) {
        Q_strncpyz(info->address, serverAddress, sizeof(info->address));
        p = info->text;
        info->numLines = 0;
        info->lines[info->numLines][0] = "Address";
        info->lines[info->numLines][1] = "";
        info->lines[info->numLines][2] = "";
        info->lines[info->numLines][3] = info->address;
        info->numLines++;
        while (p && *p) {
            p = strchr(p, '\\');
            if (!p) break;
            *p++ = '\0';
            if (*p == '\\') break;
            info->lines[info->numLines][0] = p;
            info->lines[info->numLines][1] = "";
            info->lines[info->numLines][2] = "";
            p = strchr(p, '\\');
            if (!p) break;
            *p++ = '\0';
            info->lines[info->numLines][3] = p;
            info->numLines++;
            if (info->numLines >= MAX_SERVERSTATUS_LINES) break;
        }
        if (info->numLines < MAX_SERVERSTATUS_LINES - 3) {
            info->lines[info->numLines][0] = "";
            info->lines[info->numLines][1] = "";
            info->lines[info->numLines][2] = "";
            info->lines[info->numLines][3] = "";
            info->numLines++;
            info->lines[info->numLines][0] = "num";
            info->lines[info->numLines][1] = "score";
            info->lines[info->numLines][2] = "ping";
            info->lines[info->numLines][3] = "name";
            info->numLines++;
            i = 0;
            len = 0;
            while (p && *p) {
                if (*p == '\\') *p++ = '\0';
                if (!p) break;
                score = p;
                p = strchr(p, ' ');
                if (!p) break;
                *p++ = '\0';
                ping = p;
                p = strchr(p, ' ');
                if (!p) break;
                *p++ = '\0';
                name = p;
                Com_sprintf(&info->pings[len], sizeof(info->pings) - len, "%d", i);
                info->lines[info->numLines][0] = &info->pings[len];
                len += strlen(&info->pings[len]) + 1;
                info->lines[info->numLines][1] = score;
                info->lines[info->numLines][2] = ping;
                info->lines[info->numLines][3] = name;
                info->numLines++;
                if (info->numLines >= MAX_SERVERSTATUS_LINES) break;
                p = strchr(p, '\\');
                if (!p) break;
                *p++ = '\0';
                i++;
            }
        }
        UI_SortServerStatusInfo(info);
        return qtrue;
    }
    return qfalse;
}

static void UI_BuildServerStatus(qboolean force) {
    if (uiInfo.nextFindPlayerRefresh) {
        return;
    }
    if (!force) {
        if (uiInfo.nextServerStatusRefresh == 0
            || uiInfo.uiDC.realTime < uiInfo.nextServerStatusRefresh) {
            return;
        }
    } else {
        Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
        uiInfo.serverStatusInfo.numLines = 0;
        trap_LAN_ServerStatus(NULL, NULL, 0);
    }
    if (uiInfo.serverStatus.currentServer >= 0
        && uiInfo.serverStatus.currentServer <= uiInfo.serverStatus.numDisplayServers
        && uiInfo.serverStatus.numDisplayServers != 0) {
        if (UI_GetServerStatusInfo(uiInfo.serverStatusAddress, &uiInfo.serverStatusInfo)) {
            uiInfo.nextServerStatusRefresh = 0;
            UI_GetServerStatusInfo(uiInfo.serverStatusAddress, NULL);
        } else {
            uiInfo.nextServerStatusRefresh = uiInfo.uiDC.realTime + 500;
        }
    }
}

static void UI_BuildFindPlayerList(qboolean force) {
    static int numFound, numTimeOuts;
    int i, j, resend;
    serverStatusInfo_t info;
    char name[MAX_NAME_LENGTH + 2];
    char infoString[MAX_STRING_CHARS];
    pendingServer_t *pendingServer;

    if (!force) {
        if (!uiInfo.nextFindPlayerRefresh || uiInfo.nextFindPlayerRefresh > uiInfo.uiDC.realTime) {
            return;
        }
    } else {
        memset(&uiInfo.pendingServerStatus, 0, sizeof(uiInfo.pendingServerStatus));
        uiInfo.numFoundPlayerServers = 0;
        uiInfo.currentFoundPlayerServer = 0;
        trap_Cvar_VariableStringBuffer("ui_findPlayer", uiInfo.findPlayerName,
                                       sizeof(uiInfo.findPlayerName));
        Q_CleanStr(uiInfo.findPlayerName);
        if (!strlen(uiInfo.findPlayerName)) {
            uiInfo.nextFindPlayerRefresh = 0;
            return;
        }
        resend = ui_serverStatusTimeOut.integer / 2 - 10;
        if (resend < 50) {
            resend = 50;
        }
        trap_Cvar_Set("cl_serverStatusResendTime", va("%d", resend));
        trap_LAN_ServerStatus(NULL, NULL, 0);
        uiInfo.numFoundPlayerServers = 1;
        Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers],
                    sizeof(uiInfo.foundPlayerServerNames[0]), "searching %d...",
                    uiInfo.pendingServerStatus.num);
        numFound = 0;
        numTimeOuts++;
    }
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        pendingServer = &uiInfo.pendingServerStatus.server[i];
        if (pendingServer->valid) {
            if (UI_GetServerStatusInfo(pendingServer->adrstr, &info)) {
                numFound++;
                for (j = 0; j < info.numLines; j++) {
                    if (!info.lines[j][2] || !*info.lines[j][2]) {
                        continue;
                    }
                    Q_strncpyz(name, info.lines[j][3], sizeof(name));
                    Q_CleanStr(name);
                    if (stristr(name, uiInfo.findPlayerName)) {
                        if (uiInfo.numFoundPlayerServers < MAX_FOUNDPLAYER_SERVERS - 1) {
                            Q_strncpyz(uiInfo.foundPlayerServerAddresses[uiInfo.numFoundPlayerServers],
                                       pendingServer->adrstr,
                                       sizeof(uiInfo.foundPlayerServerAddresses[0]));
                            Q_strncpyz(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers],
                                       pendingServer->name,
                                       sizeof(uiInfo.foundPlayerServerNames[0]));
                            uiInfo.numFoundPlayerServers++;
                        } else {
                            uiInfo.pendingServerStatus.num = uiInfo.serverStatus.numDisplayServers;
                            break;
                        }
                    }
                }
                Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers],
                            sizeof(uiInfo.foundPlayerServerNames[0]), "searching %d/%d...",
                            uiInfo.pendingServerStatus.num, numFound);
                pendingServer->valid = qfalse;
            }
            if (!pendingServer->valid
                || pendingServer->startTime < uiInfo.uiDC.realTime - ui_serverStatusTimeOut.integer) {
                if (pendingServer->valid) {
                    numTimeOuts++;
                }
                trap_LAN_ServerStatus(pendingServer->adrstr, NULL, 0);
                pendingServer->valid = qfalse;
                if (uiInfo.pendingServerStatus.num < uiInfo.serverStatus.numDisplayServers) {
                    pendingServer->startTime = uiInfo.uiDC.realTime;
                    trap_LAN_GetServerAddressString(ui_netSource.integer,
                        uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num],
                        pendingServer->adrstr, sizeof(pendingServer->adrstr));
                    trap_LAN_GetServerInfo(ui_netSource.integer,
                        uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num],
                        infoString, sizeof(infoString));
                    Q_strncpyz(pendingServer->name, Info_ValueForKey(infoString, "hostname"),
                               sizeof(pendingServer->name));
                    pendingServer->valid = qtrue;
                    uiInfo.pendingServerStatus.num++;
                    Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1],
                                sizeof(uiInfo.foundPlayerServerNames[0]), "searching %d/%d...",
                                uiInfo.pendingServerStatus.num, numFound);
                }
            }
        }
    }
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (uiInfo.pendingServerStatus.server[i].valid) {
            break;
        }
    }
    if (i < MAX_SERVERSTATUSREQUESTS
        || uiInfo.pendingServerStatus.num < uiInfo.serverStatus.numDisplayServers) {
        uiInfo.nextFindPlayerRefresh = uiInfo.uiDC.realTime + 25;
        return;
    }
    if (!uiInfo.numFoundPlayerServers) {
        Com_sprintf(uiInfo.foundPlayerServerNames[0], sizeof(uiInfo.foundPlayerServerNames[0]),
                    "no servers found");
    } else {
        Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers],
                    sizeof(uiInfo.foundPlayerServerNames[0]),
                    "%d server%s found with player %s",
                    uiInfo.numFoundPlayerServers - 1,
                    (uiInfo.numFoundPlayerServers == 2) ? "" : "s", uiInfo.findPlayerName);
    }
    uiInfo.nextFindPlayerRefresh = 0;
    UI_FeederSelection(FEEDER_FINDPLAYER, uiInfo.currentFoundPlayerServer);
}

static void UI_ServerRefreshComplete(void) {
    int count;

    if (!uiInfo.serverStatus.refreshActive) {
        return;
    }
    uiInfo.serverStatus.refreshActive = qfalse;
    Com_Printf("%d servers listed in browser with %d players.\n",
               uiInfo.serverStatus.numDisplayServers,
               uiInfo.serverStatus.numPlayersOnServers);
    count = trap_LAN_GetServerCount(ui_netSource.integer);
    if (count - uiInfo.serverStatus.numDisplayServers > 0) {
        Com_Printf("%d servers not listed due to packet loss or pings higher than %d\n",
                   count - uiInfo.serverStatus.numDisplayServers,
                   (int)trap_Cvar_VariableValue("cl_maxPing"));
    }
}

static void UI_DoServerRefresh(void) {
    qboolean wait = qfalse;

    if (!uiInfo.serverStatus.refreshActive) {
        return;
    }
    if (ui_netSource.integer != AS_FAVORITES) {
        if (ui_netSource.integer == AS_LOCAL) {
            if (!trap_LAN_GetServerCount(ui_netSource.integer)) {
                wait = qtrue;
            }
        } else {
            if (trap_LAN_GetServerCount(ui_netSource.integer) < 0) {
                wait = qtrue;
            }
        }
    }

    if (uiInfo.uiDC.realTime < uiInfo.serverStatus.refreshtime && wait) {
        return;
    }

    if (trap_LAN_UpdateVisiblePings(ui_netSource.integer)) {
        uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
    } else if (!wait) {
        UI_BuildServerDisplayList(2);
        UI_ServerRefreshComplete();
    }
    UI_BuildServerDisplayList(qfalse);
}

void UI_Browser_Sort(int column) {
    if (column == uiInfo.serverStatus.sortKey) {
        uiInfo.serverStatus.sortDir = !uiInfo.serverStatus.sortDir;
    }
    UI_ServersSort(column, qtrue);
}

static int uiBrowserSource;

void UI_Browser_SetSource(int source) {
    uiBrowserSource = (source >= 0 && source <= 4) ? source : 0;
}

void UI_Home_ServerList(qboolean refresh) {
    if (refresh) {
        UI_StartServerRefresh(qtrue);
    }
    UI_BuildServerDisplayList(refresh ? 1 : 0);
}

static void UI_StartServerRefresh(qboolean full) {
    int i;
    char *ptr;
    qtime_t q;

    trap_RealTime(&q);
    trap_Cvar_Set(va("ui_lastServerRefresh_%i", ui_netSource.integer),
                  va("%s-%i, %i at %i:%i", monthAbbrev[q.tm_mon], q.tm_mday,
                     1900 + q.tm_year, q.tm_hour, q.tm_min));

    if (!full) {
        trap_LAN_ResetPings(ui_netSource.integer);
        uiInfo.serverStatus.refreshActive = qtrue;
        uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
        return;
    }

    uiInfo.serverStatus.refreshActive = qtrue;
    uiInfo.serverStatus.nextDisplayRefresh = uiInfo.uiDC.realTime + 1000;
    uiInfo.serverStatus.numDisplayServers = 0;
    uiInfo.serverStatus.numPlayersOnServers = 0;
    trap_LAN_MarkServerVisible(ui_netSource.integer, -1, qtrue);
    trap_LAN_ResetPings(ui_netSource.integer);

    if (ui_netSource.integer == AS_LOCAL) {
        trap_Cmd_ExecuteText(EXEC_APPEND, "localservers\n");
        uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
        return;
    }

    uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 5000;
    if (ui_netSource.integer == AS_GLOBAL || ui_netSource.integer == AS_MPLAYER) {
        i = (ui_netSource.integer == AS_GLOBAL) ? uiBrowserSource : 1;
        ptr = UI_Cvar_VariableString("debug_protocol");
        if (strlen(ptr)) {
            trap_Cmd_ExecuteText(EXEC_APPEND, va("globalservers %d %s full empty\n", i, ptr));
        } else {
            trap_Cmd_ExecuteText(EXEC_APPEND, va("globalservers %d %d full empty\n", i,
                                 (int)trap_Cvar_VariableValue("protocol")));
        }
    }
}

static qboolean UI_PlayerModelExists(int index) {
    char path[1024];

    if (index < 0 || index >= MAX_HEADS) {
        return qfalse;
    }

    Com_sprintf(path, sizeof(path), "models/players/%s/lower_%s.skin",
                uiInfo.characterList[index].base, uiInfo.characterList[index].name);

    return (trap_FS_FOpenFile(path, NULL, FS_READ) != 0) ? qtrue : qfalse;
}

static int UI_BuildPlayerModelList(int excludeSkins) {
    int i;
    int count;
    const char* skin;

    count = 0;
    for (i = 0; i < uiInfo.characterCount; i++) {
        if (excludeSkins == 1) {
            skin = uiInfo.characterList[i].name;
            if (skin && (Q_stricmp(skin, "blue") == 0 ||
                         Q_stricmp(skin, "bright") == 0 ||
                         Q_stricmp(skin, "red") == 0 ||
                         Q_stricmp(skin, "sport") == 0 ||
                         Q_stricmp(skin, "sport_blue") == 0 ||
                         Q_stricmp(skin, "sport_red") == 0)) {
                continue;
            }
        }

        if (!uiInfo.characterList[i].active) {
            if (!UI_PlayerModelExists(i)) {
                continue;
            }
            uiInfo.characterList[i].active = qtrue;
        }
        count++;
    }
    return count;
}

static int UI_FeederCount(float feederID) {
    if (feederID == FEEDER_HEADS) {
        return UI_BuildPlayerModelList(0);
    } else if (feederID == FEEDER_Q3HEADS) {
        return UI_BuildPlayerModelList(1);
    } else if (feederID == FEEDER_CINEMATICS) {
        return uiInfo.movieCount;
    } else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
        return UI_MapCountByGameType(feederID == FEEDER_MAPS ? qtrue : qfalse);
    } else if (feederID == FEEDER_CVMAPS) {
        return UI_MapCountByCallvoteGameType();
    } else if (feederID == FEEDER_FINDPLAYER) {
        return uiInfo.numFoundPlayerServers;
    } else if (feederID == FEEDER_PLAYER_LIST) {
        if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
            uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
            UI_BuildPlayerList();
        }
        return uiInfo.playerCount;
    } else if (feederID == FEEDER_TEAM_LIST) {
        if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
            uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
            UI_BuildPlayerList();
        }
        return uiInfo.myTeamCount;
    } else if (feederID == FEEDER_MODS) {
        return uiInfo.modCount;
    } else if (feederID == FEEDER_DEMOS) {
        return uiInfo.demoCount;
    } else if (feederID == FEEDER_SERVERS) {
        return uiInfo.serverStatus.numDisplayServers;
    }
    return 0;
}

static const char* UI_SelectedMap(int index, int* actual) {
    int i, c;
    c = 0;
    *actual = 0;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            if (c == index) {
                *actual = i;
                return uiInfo.mapList[i].mapName;
            } else {
                c++;
            }
        }
    }
    return "";
}

static const char* UI_SelectedHead(int index, int* actual) {
    int i, c;
    c = 0;
    *actual = 0;
    for (i = 0; i < uiInfo.characterCount; i++) {
        if (uiInfo.characterList[i].active) {
            if (c == index) {
                *actual = i;
                return uiInfo.characterList[i].name;
            } else {
                c++;
            }
        }
    }
    return "";
}

static int UI_GetIndexFromSelection(int actual) {
    int i, c;
    c = 0;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            if (i == actual) {
                return c;
            }
            c++;
        }
    }
    return 0;
}

static const char* UI_FeederItemText(float feederID, int index, int column, qhandle_t* handle) {
    static char info[MAX_STRING_CHARS];
    static char hostname[1024];
    static char clientBuff[32];
    static int lastColumn = -1;
    static int lastTime = 0;
    static const char *browserGameTypes[8] = {
        "FFA", "TOURNAMENT", "SP", "TEAM DM", "CTF", "1FCTF", "OVERLOAD", "HARVESTER"
    };
    *handle = -1;
    if (feederID == FEEDER_HEADS || feederID == FEEDER_Q3HEADS) {
        int actual;
        return UI_SelectedHead(index, &actual);
    } else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS || feederID == FEEDER_CVMAPS) {
        int actual;
        return UI_SelectedMap(index, &actual);
    } else if (feederID == FEEDER_FINDPLAYER) {
        if (index >= 0 && index < uiInfo.numFoundPlayerServers) {
            return uiInfo.foundPlayerServerNames[index];
        }
    } else if (feederID == FEEDER_PLAYER_LIST) {
        if (index >= 0 && index < uiInfo.playerCount) {
            return uiInfo.playerNames[index];
        }
    } else if (feederID == FEEDER_TEAM_LIST) {
        if (index >= 0 && index < uiInfo.myTeamCount) {
            return uiInfo.teamNames[index];
        }
    } else if (feederID == FEEDER_MODS) {
        if (index >= 0 && index < uiInfo.modCount) {
            if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr) {
                return uiInfo.modList[index].modDescr;
            } else {
                return uiInfo.modList[index].modName;
            }
        }
    } else if (feederID == FEEDER_CINEMATICS) {
        if (index >= 0 && index < uiInfo.movieCount) {
            return uiInfo.movieList[index];
        }
    } else if (feederID == FEEDER_DEMOS) {
        if (index >= 0 && index < uiInfo.demoCount) {
            return uiInfo.demoList[index];
        }
    } else if (feederID == FEEDER_SERVERS) {
        if (index >= 0 && index < uiInfo.serverStatus.numDisplayServers) {
            int ping, game;
            if (lastColumn != column || lastTime > uiInfo.uiDC.realTime + 5000) {
                trap_LAN_GetServerInfo(ui_netSource.integer,
                    uiInfo.serverStatus.displayServers[index], info, sizeof(info));
                lastColumn = column;
                lastTime = uiInfo.uiDC.realTime;
            }
            ping = atoi(Info_ValueForKey(info, "ping"));
            switch (column) {
            case SORT_HOST:
                if (ping <= 0) {
                    return Info_ValueForKey(info, "addr");
                }
                if (ui_netSource.integer == AS_LOCAL) {
                    Com_sprintf(hostname, sizeof(hostname), "%s [%s]",
                                Info_ValueForKey(info, "hostname"),
                                netnames[atoi(Info_ValueForKey(info, "nettype"))]);
                    return hostname;
                }
                Com_sprintf(hostname, sizeof(hostname), "%s", Info_ValueForKey(info, "hostname"));
                return hostname;
            case SORT_MAP:
                return Info_ValueForKey(info, "mapname");
            case SORT_CLIENTS:
                Com_sprintf(clientBuff, sizeof(clientBuff), "%s (%s)",
                            Info_ValueForKey(info, "clients"),
                            Info_ValueForKey(info, "sv_maxclients"));
                return clientBuff;
            case SORT_GAME:
                game = atoi(Info_ValueForKey(info, "gametype"));
                if (game >= 0 && game < 8) {
                    return browserGameTypes[game];
                }
                return "Unknown";
            case SORT_PING:
                if (ping <= 0) {
                    return "...";
                }
                return Info_ValueForKey(info, "ping");
            }
        }
    } else if (feederID == FEEDER_SERVERSTATUS) {
        if (index >= 0 && index < uiInfo.serverStatusInfo.numLines
            && column >= 0 && column < 4) {
            return uiInfo.serverStatusInfo.lines[index][column];
        }
    }
    return "";
}

static qhandle_t UI_FeederItemImage(float feederID, int index) {
    if (feederID == FEEDER_HEADS || feederID == FEEDER_Q3HEADS) {
        int actual;
        UI_SelectedHead(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.characterCount) {
            if (uiInfo.characterList[index].headImage == -1) {
                uiInfo.characterList[index].headImage = trap_R_RegisterShaderNoMip(uiInfo.characterList[index].imageName);
            }
            return uiInfo.characterList[index].headImage;
        }
    } else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS || feederID == FEEDER_CVMAPS) {
        int actual;
        UI_SelectedMap(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.mapCount) {
            if (uiInfo.mapList[index].levelShot == -1) {
                uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
            }
            return uiInfo.mapList[index].levelShot;
        }
    }
    return 0;
}

static void UI_FeederSelection(float feederID, int index) {
    static char info[MAX_STRING_CHARS];
    if (feederID == FEEDER_HEADS || feederID == FEEDER_Q3HEADS) {
        int actual;
        UI_SelectedHead(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.characterCount) {
            if (Q_stricmp(uiInfo.characterList[index].name, "default") == 0) {
                trap_Cvar_Set("model", uiInfo.characterList[index].base);
                trap_Cvar_Set("headmodel", uiInfo.characterList[index].base);
            } else {
                trap_Cvar_Set("model", va("%s/%s", uiInfo.characterList[index].base, uiInfo.characterList[index].name));
                trap_Cvar_Set("headmodel", va("%s/%s", uiInfo.characterList[index].base, uiInfo.characterList[index].name));
            }
            updateModel = qtrue;
        }
    } else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS || feederID == FEEDER_CVMAPS) {
        int actual, map;
        map = (feederID == FEEDER_ALLMAPS || feederID == FEEDER_CVMAPS) ? ui_currentNetMap.integer : ui_currentMap.integer;
        if (uiInfo.mapList[map].cinematic >= 0) {
            trap_CIN_StopCinematic(uiInfo.mapList[map].cinematic);
            uiInfo.mapList[map].cinematic = -1;
        }
        UI_SelectedMap(index, &actual);
        trap_Cvar_Set("ui_mapIndex", va("%d", index));
        ui_mapIndex.integer = index;

        if (feederID == FEEDER_MAPS) {
            ui_currentMap.integer = actual;
            trap_Cvar_Set("ui_currentMap", va("%d", actual));
            uiInfo.mapList[ui_currentMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
            trap_Cvar_Set("ui_opponentModel", uiInfo.mapList[ui_currentMap.integer].opponentName);
            updateOpponentModel = qtrue;
        } else {
            ui_currentNetMap.integer = actual;
            trap_Cvar_Set("ui_currentNetMap", va("%d", actual));
            uiInfo.mapList[ui_currentNetMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        }
    } else if (feederID == FEEDER_SERVERS) {
        const char* mapName;
        uiInfo.serverStatus.currentServer = index;
        trap_LAN_GetServerInfo(ui_netSource.integer,
            uiInfo.serverStatus.displayServers[index], info, sizeof(info));
        uiInfo.serverStatus.currentServerPreview = trap_R_RegisterShaderNoMip(
            va("levelshots/%s", Info_ValueForKey(info, "mapname")));
        if (uiInfo.serverStatus.currentServerCinematic >= 0) {
            trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
            uiInfo.serverStatus.currentServerCinematic = -1;
        }
        mapName = Info_ValueForKey(info, "mapname");
        if (mapName && *mapName) {
            uiInfo.serverStatus.currentServerCinematic = trap_CIN_PlayCinematic(
                va("%s.roq", mapName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        }
    } else if (feederID == FEEDER_SERVERSTATUS) {
    } else if (feederID == FEEDER_FINDPLAYER) {
        uiInfo.currentFoundPlayerServer = index;
        if (index < uiInfo.numFoundPlayerServers - 1) {
            Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
            Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
        }
    } else if (feederID == FEEDER_PLAYER_LIST) {
        uiInfo.playerIndex = index;
    } else if (feederID == FEEDER_TEAM_LIST) {
        uiInfo.teamIndex = index;
    } else if (feederID == FEEDER_MODS) {
        uiInfo.modIndex = index;
    } else if (feederID == FEEDER_CINEMATICS) {
        uiInfo.movieIndex = index;
        if (uiInfo.previewMovie >= 0) {
            trap_CIN_StopCinematic(uiInfo.previewMovie);
        }
        uiInfo.previewMovie = -1;
    } else if (feederID == FEEDER_DEMOS) {
        uiInfo.demoIndex = index;
    }
}

static qboolean Team_Parse(char** p) {
    char* token;
    const char* tempStr;
    int i;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (uiInfo.teamCount == MAX_TEAMS) {
                uiInfo.teamCount--;
                Com_Printf("Too many teams, last team replaced!\n");
            }

            if (!String_Parse(p, &uiInfo.teamList[uiInfo.teamCount].teamName) || !String_Parse(p, &tempStr)) {
                return qfalse;
            }

            uiInfo.teamList[uiInfo.teamCount].imageName = tempStr;
            uiInfo.teamList[uiInfo.teamCount].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[uiInfo.teamCount].imageName);
            uiInfo.teamList[uiInfo.teamCount].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[uiInfo.teamCount].imageName));
            uiInfo.teamList[uiInfo.teamCount].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[uiInfo.teamCount].imageName));

            uiInfo.teamList[uiInfo.teamCount].cinematic = -1;

            for (i = 0; i < TEAM_MEMBERS; i++) {
                uiInfo.teamList[uiInfo.teamCount].teamMembers[i] = NULL;
                if (!String_Parse(p, &uiInfo.teamList[uiInfo.teamCount].teamMembers[i])) {
                    return qfalse;
                }
            }

            Com_Printf("Loaded team %s with team icon %s.\n", uiInfo.teamList[uiInfo.teamCount].teamName, tempStr);
            uiInfo.teamCount++;

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }

    return qfalse;
}

static qboolean Character_Parse(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (uiInfo.characterCount == MAX_HEADS) {
                uiInfo.characterCount--;
                Com_Printf("Too many characters, last character replaced!\n");
            }

            if (!String_Parse(p, &uiInfo.characterList[uiInfo.characterCount].base) || !String_Parse(p, &uiInfo.characterList[uiInfo.characterCount].name)) {
                return qfalse;
            }

            uiInfo.characterList[uiInfo.characterCount].headImage = -1;
            uiInfo.characterList[uiInfo.characterCount].imageName = String_Alloc(va("models/players/%s/icon_%s.tga", uiInfo.characterList[uiInfo.characterCount].base, uiInfo.characterList[uiInfo.characterCount].name));

            uiInfo.characterCount++;

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }

    return qfalse;
}

static qboolean Alias_Parse(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (uiInfo.aliasCount == MAX_ALIASES) {
                uiInfo.aliasCount--;
                Com_Printf("Too many aliases, last alias replaced!\n");
            }

            if (!String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].name) || !String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].ai) || !String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].action)) {
                return qfalse;
            }

            Com_Printf("Loaded character alias %s using character ai %s.\n", uiInfo.aliasList[uiInfo.aliasCount].name, uiInfo.aliasList[uiInfo.aliasCount].ai);
            uiInfo.aliasCount++;

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }

    return qfalse;
}

static void UI_ParseTeamInfo(const char* teamFile) {
    char* token;
    char* p;
    char* buff = NULL;

    buff = GetMenuBuffer(teamFile);
    if (!buff) {
        return;
    }

    p = buff;

    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || token[0] == '}') {
            break;
        }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "teams") == 0) {
            if (Team_Parse(&p)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token, "characters") == 0) {
            Character_Parse(&p);
        }

        if (Q_stricmp(token, "aliases") == 0) {
            Alias_Parse(&p);
        }
    }
}

static qboolean GameType_Parse(char** p, qboolean join) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    if (join) {
        uiInfo.numJoinGameTypes = 0;
    } else {
        uiInfo.numGameTypes = 0;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (join) {
                if (!String_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gameType) || !Int_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gtEnum)) {
                    return qfalse;
                }
            } else {
                if (!String_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gameType) || !Int_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gtEnum)) {
                    return qfalse;
                }
            }

            if (join) {
                if (uiInfo.numJoinGameTypes < MAX_GAMETYPES) {
                    uiInfo.numJoinGameTypes++;
                } else {
                    Com_Printf("Too many net game types, last one replace!\n");
                }
            } else {
                if (uiInfo.numGameTypes < MAX_GAMETYPES) {
                    uiInfo.numGameTypes++;
                } else {
                    Com_Printf("Too many game types, last one replace!\n");
                }
            }

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }
    return qfalse;
}

static qboolean MapList_Parse(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    uiInfo.mapCount = 0;

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapName) || !String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapLoadName) || !Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].teamMembers)) {
                return qfalse;
            }

            if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].opponentName)) {
                return qfalse;
            }

            uiInfo.mapList[uiInfo.mapCount].typeBits = 0;

            while (1) {
                token = COM_ParseExt(p, qtrue);
                if (token[0] >= '0' && token[0] <= '9') {
                    uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << (token[0] - 0x030));
                    if (!Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].timeToBeat[token[0] - 0x30])) {
                        return qfalse;
                    }
                } else {
                    break;
                }
            }

            uiInfo.mapList[uiInfo.mapCount].cinematic = -1;
            uiInfo.mapList[uiInfo.mapCount].levelShot = trap_R_RegisterShaderNoMip(va("levelshots/preview/%s", uiInfo.mapList[uiInfo.mapCount].mapLoadName));

            if (uiInfo.mapCount < MAX_MAPS) {
                uiInfo.mapCount++;
            } else {
                Com_Printf("Too many maps, last one replaced!\n");
            }
        }
    }
    return qfalse;
}

static void UI_ParseGameInfo(const char* teamFile) {
    char* token;
    char* p;
    char* buff = NULL;

    {
        fileHandle_t f;

        trap_FS_FOpenFile(teamFile, &f, FS_READ);
        if (!f) {
            return;
        }
        trap_FS_FCloseFile(f);
    }

    buff = GetMenuBuffer(teamFile);
    if (!buff) {
        return;
    }

    p = buff;

    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || token[0] == '}') {
            break;
        }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "gametypes") == 0) {
            if (GameType_Parse(&p, qfalse)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token, "joingametypes") == 0) {
            if (GameType_Parse(&p, qtrue)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token, "maps") == 0) {
            MapList_Parse(&p);
        }
    }
}

static void UI_Pause(qboolean b) {
    if (b) {
        trap_Cvar_Set("cl_paused", "1");
        trap_Key_SetCatcher(KEYCATCH_UI);
    } else {
        trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
        trap_Key_ClearStates();
        trap_Cvar_Set("cl_paused", "0");
    }
}

static int UI_PlayCinematic(const char* name, float x, float y, float w, float h) {
    return trap_CIN_PlayCinematic(name, x, y, w, h, (CIN_loop | CIN_silent));
}

static void UI_StopCinematic(int handle) {
    if (handle >= 0) {
        trap_CIN_StopCinematic(handle);
    } else {
        handle = abs(handle);
        if (handle == UI_MAPCINEMATIC) {
            if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0) {
                trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
                uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
            }
        } else if (handle == UI_NETMAPCINEMATIC) {
            if (uiInfo.serverStatus.currentServerCinematic >= 0) {
                trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
                uiInfo.serverStatus.currentServerCinematic = -1;
            }
        } else if (handle == UI_CLANCINEMATIC) {
            int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
            if (i >= 0 && i < uiInfo.teamCount) {
                if (uiInfo.teamList[i].cinematic >= 0) {
                    trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
                    uiInfo.teamList[i].cinematic = -1;
                }
            }
        }
    }
}

static void UI_DrawCinematic(int handle, float x, float y, float w, float h) {
    UI_AdjustFrom640(&x, &y, &w, &h);

    x *= SCREEN_WIDTH / (float)uiInfo.uiDC.glconfig.vidWidth;
    w *= SCREEN_WIDTH / (float)uiInfo.uiDC.glconfig.vidWidth;
    y *= SCREEN_HEIGHT / (float)uiInfo.uiDC.glconfig.vidHeight;
    h *= SCREEN_HEIGHT / (float)uiInfo.uiDC.glconfig.vidHeight;

    trap_CIN_SetExtents(handle, x, y, w, h);
    trap_CIN_DrawCinematic(handle);
}

static void UI_RunCinematicFrame(int handle) {
    trap_CIN_RunCinematic(handle);
}

static void UI_BuildQ3Model_List(void) {
    int numdirs;
    int numfiles;
    char dirlist[2048];
    char filelist[2048];
    char skinname[MAX_QPATH];
    char scratch[256];
    char* dirptr;
    char* fileptr;
    int i;
    int j, k, dirty;
    int dirlen;
    int filelen;

    uiInfo.q3HeadCount = 0;

    numdirs = trap_FS_GetFileList("models/players", "/", dirlist, 2048);
    dirptr = dirlist;
    for (i = 0; i < numdirs && uiInfo.q3HeadCount < MAX_PLAYERMODELS; i++, dirptr += dirlen + 1) {
        dirlen = strlen(dirptr);

        if (dirlen && dirptr[dirlen - 1] == '/')
            dirptr[dirlen - 1] = '\0';

        if (!strcmp(dirptr, ".") || !strcmp(dirptr, ".."))
            continue;

        numfiles = trap_FS_GetFileList(va("models/players/%s", dirptr), "tga", filelist, 2048);
        fileptr = filelist;
        for (j = 0; j < numfiles && uiInfo.q3HeadCount < MAX_PLAYERMODELS; j++, fileptr += filelen + 1) {
            filelen = strlen(fileptr);

            COM_StripExtension(fileptr, skinname, sizeof(skinname));

            if (Q_stricmpn(skinname, "icon_", 5) == 0 && !(Q_stricmp(skinname, "icon_blue") == 0 || Q_stricmp(skinname, "icon_red") == 0)) {
                if (Q_stricmp(skinname, "icon_default") == 0) {
                    Com_sprintf(scratch, sizeof(scratch), "%s", dirptr);
                } else {
                    Com_sprintf(scratch, sizeof(scratch), "%s/%s", dirptr, skinname + 5);
                }
                dirty = 0;
                for (k = 0; k < uiInfo.q3HeadCount; k++) {
                    if (!Q_stricmp(scratch, uiInfo.q3HeadNames[uiInfo.q3HeadCount])) {
                        dirty = 1;
                        break;
                    }
                }
                if (!dirty) {
                    Com_sprintf(uiInfo.q3HeadNames[uiInfo.q3HeadCount], sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount]), "%s", scratch);
                    uiInfo.q3HeadIcons[uiInfo.q3HeadCount++] = trap_R_RegisterShaderNoMip(va("models/players/%s/%s", dirptr, skinname));
                }
            }
        }
    }
}

void _UI_Init(qboolean inGameLoad) {
    const char* menuSet;

    UI_RegisterCvars();
    UI_InitMemory();

    trap_GetGlconfig(&uiInfo.uiDC.glconfig);

    trap_Cvar_Set("ui_videomode", va("%dx%d", uiInfo.uiDC.glconfig.vidWidth, uiInfo.uiDC.glconfig.vidHeight));

    uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0 / (float)SCREEN_HEIGHT);
    uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0 / (float)SCREEN_WIDTH);
    if (uiInfo.uiDC.glconfig.vidWidth * SCREEN_HEIGHT > uiInfo.uiDC.glconfig.vidHeight * SCREEN_WIDTH) {
        uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - (uiInfo.uiDC.glconfig.vidHeight * ((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)));
        uiInfo.uiDC.xscale = uiInfo.uiDC.yscale;
    } else {
        uiInfo.uiDC.bias = 0;
    }

    uiInfo.uiDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
    uiInfo.uiDC.setColor = &UI_SetColor;
    uiInfo.uiDC.drawHandlePic = &UI_DrawHandlePic;
    uiInfo.uiDC.drawStretchPic = &trap_R_DrawStretchPic;
    uiInfo.uiDC.drawText = &UI_DrawText_DC;
    uiInfo.uiDC.textWidth = &UI_TextWidth_DC;
    uiInfo.uiDC.textHeight = &UI_TextHeight_DC;
    uiInfo.uiDC.registerModel = &trap_R_RegisterModel;
    uiInfo.uiDC.modelBounds = &trap_R_ModelBounds;
    uiInfo.uiDC.fillRect = &UI_FillRect;
    uiInfo.uiDC.drawRect = &_UI_DrawRect;
    uiInfo.uiDC.drawSides = &_UI_DrawSides;
    uiInfo.uiDC.drawTopBottom = &_UI_DrawTopBottom;
    uiInfo.uiDC.clearScene = &trap_R_ClearScene;
    uiInfo.uiDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
    uiInfo.uiDC.renderScene = &trap_R_RenderScene;
    uiInfo.uiDC.registerFont = &trap_R_RegisterFont;
    uiInfo.uiDC.ownerDrawItem = &UI_OwnerDraw_DC;
    uiInfo.uiDC.getValue = &UI_GetValue;
    uiInfo.uiDC.ownerDrawVisible = &UI_OwnerDrawVisible;
    uiInfo.uiDC.runScript = &UI_RunMenuScript;
    uiInfo.uiDC.getTeamColor = &UI_GetTeamColor;
    uiInfo.uiDC.setCVar = trap_Cvar_Set;
    uiInfo.uiDC.getCVarString = trap_Cvar_VariableStringBuffer;
    uiInfo.uiDC.getCVarValue = trap_Cvar_VariableValue;
    uiInfo.uiDC.drawTextWithCursor = &UI_DrawTextWithCursor_DC;
    uiInfo.uiDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
    uiInfo.uiDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
    uiInfo.uiDC.startLocalSound = &trap_S_StartLocalSound;
    uiInfo.uiDC.ownerDrawHandleKey = &UI_OwnerDrawHandleKey;
    uiInfo.uiDC.feederCount = &UI_FeederCount;
    uiInfo.uiDC.feederItemImage = &UI_FeederItemImage;
    uiInfo.uiDC.feederItemText = &UI_FeederItemText;
    uiInfo.uiDC.feederSelection = &UI_FeederSelection;
    uiInfo.uiDC.setBinding = &trap_Key_SetBinding;
    uiInfo.uiDC.getBindingBuf = &trap_Key_GetBindingBuf;
    uiInfo.uiDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
    uiInfo.uiDC.executeText = &trap_Cmd_ExecuteText;
    uiInfo.uiDC.Error = &Com_Error;
    uiInfo.uiDC.Print = &Com_Printf;
    uiInfo.uiDC.Pause = &UI_Pause;
    uiInfo.uiDC.ownerDrawWidth = &UI_OwnerDrawWidth;
    uiInfo.uiDC.registerSound = &trap_S_RegisterSound;
    uiInfo.uiDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
    uiInfo.uiDC.stopBackgroundTrack = &trap_S_StopBackgroundTrack;
    uiInfo.uiDC.playCinematic = &UI_PlayCinematic;
    uiInfo.uiDC.stopCinematic = &UI_StopCinematic;
    uiInfo.uiDC.drawCinematic = &UI_DrawCinematic;
    uiInfo.uiDC.runCinematicFrame = &UI_RunCinematicFrame;

    Init_Display(&uiInfo.uiDC);

    String_Init();

    uiInfo.uiDC.cursor = trap_R_RegisterShaderNoMip("menu/art/3_cursor2");
    uiInfo.uiDC.whiteShader = trap_R_RegisterShaderNoMip("white");

    AssetCache();

    uiInfo.teamCount = 0;
    uiInfo.characterCount = 0;
    uiInfo.aliasCount = 0;

    UI_ParseTeamInfo("ui/teaminfo.txt");
    UI_LoadTeams();
    UI_LoadArenas();

    menuSet = UI_Cvar_VariableString("ui_menuFiles");
    if (menuSet == NULL || menuSet[0] == '\0') {
        menuSet = "ui/menus.txt";
    }

    UI_LoadMenus(menuSet, qtrue);
    UI_LoadMenus("ui/ingame.txt", qfalse);

    Menus_CloseAll();

    UI_BuildQ3Model_List();
    UI_LoadBots();

    uiInfo.effectsColor = gamecodetoui[(int)trap_Cvar_VariableValue("color1") - 1];
    uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair") % NUM_CROSSHAIRS;
    if (uiInfo.currentCrosshair < 0) {
        uiInfo.currentCrosshair = 0;
    }
    trap_Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");

    uiInfo.serverStatus.currentServerCinematic = -1;
    uiInfo.previewMovie = -1;

    trap_Cvar_Register(NULL, "debug_protocol", "", 0);

    trap_Cvar_Set("ui_actualNetGametype", va("%d", ui_netGameType.integer));
}

void _UI_KeyEvent(int key, qboolean down) {
    if (Menu_Count() > 0) {
        menuDef_t* menu = Menu_GetFocused();
        if (menu) {
            if (key == K_ESCAPE && down && !Menus_AnyFullScreenVisible()) {
                Menus_CloseAll();
            } else {
                Menu_HandleKey(menu, key, down);
            }
        } else {
            trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
            trap_Key_ClearStates();
            trap_Cvar_Set("cl_paused", "0");
        }
    }
}

void _UI_MouseEvent(int dx, int dy) {
    int bias;

    bias = uiInfo.uiDC.bias / uiInfo.uiDC.xscale;

    uiInfo.uiDC.cursorx += dx;
    if (uiInfo.uiDC.cursorx < -bias)
        uiInfo.uiDC.cursorx = -bias;
    else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH + bias)
        uiInfo.uiDC.cursorx = SCREEN_WIDTH + bias;

    uiInfo.uiDC.cursory += dy;
    if (uiInfo.uiDC.cursory < 0)
        uiInfo.uiDC.cursory = 0;
    else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
        uiInfo.uiDC.cursory = SCREEN_HEIGHT;

    if (Menu_Count() > 0) {
        Display_MouseMove(NULL, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
    }
}

void _UI_MousePosition(int x, int y) {
    int bias;

    if (uiInfo.uiDC.xscale <= 0 || uiInfo.uiDC.yscale <= 0) {
        return;
    }
    bias = uiInfo.uiDC.bias / uiInfo.uiDC.xscale;

    uiInfo.uiDC.cursorx = ((float)x - uiInfo.uiDC.bias) / uiInfo.uiDC.xscale;
    if (uiInfo.uiDC.cursorx < -bias)
        uiInfo.uiDC.cursorx = -bias;
    else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH + bias)
        uiInfo.uiDC.cursorx = SCREEN_WIDTH + bias;

    uiInfo.uiDC.cursory = (float)y / uiInfo.uiDC.yscale;
    if (uiInfo.uiDC.cursory < 0)
        uiInfo.uiDC.cursory = 0;
    else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
        uiInfo.uiDC.cursory = SCREEN_HEIGHT;

    if (Menu_Count() > 0) {
        Display_MouseMove(NULL, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
    }
}

void UI_LoadNonIngame(void) {
    const char* menuSet = UI_Cvar_VariableString("ui_menuFiles");
    if (menuSet == NULL || menuSet[0] == '\0') {
        menuSet = "ui/menus.txt";
    }
    UI_LoadMenus(menuSet, qfalse);
    uiInfo.inGameLoad = qfalse;
}

void _UI_SetActiveMenu(uiMenuCommand_t menu) {
    char buf[256];

    if (Menu_Count() > 0) {
        vec3_t v;
        v[0] = v[1] = v[2] = 0;
        switch (menu) {
            case UIMENU_NONE:
                UI_Home_Leave();
                trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
                trap_Key_ClearStates();
                Menus_CloseAll();
                trap_Cvar_Set("cl_paused", "0");
                return;
            case UIMENU_MAIN:
                trap_Key_SetCatcher(KEYCATCH_UI);
                uiInfo.effectsColor = gamecodetoui[(int)trap_Cvar_VariableValue("color1") - 1];
                uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair");
                if (uiInfo.inGameLoad) {
                    UI_LoadNonIngame();
                }
                Menus_CloseAll();
                Menus_ActivateByName("main");
                trap_Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof(buf));
                trap_S_StartBackgroundTrack("music/fla_mp05", NULL);
                if (strlen(buf)) {
                    UI_Home_ShowError(buf);
                }
                return;
            case UIMENU_MAIN_OPTIONS: {
                uiClientState_t cs;
                qboolean inMatch;

                trap_GetClientState(&cs);
                inMatch = (qboolean)(cs.connState >= CA_ACTIVE);

                trap_Cvar_Set("cl_paused", "1");
                trap_Key_SetCatcher(KEYCATCH_UI);
                UI_BuildPlayerList();
                uiInfo.effectsColor = gamecodetoui[(int)trap_Cvar_VariableValue("color1") - 1];
                uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair");

                Menus_CloseAll();
                if (inMatch && !Menus_FindByName("settings_ingame")) {
                    UI_ParseMenu("ui/ioql/settings_ingame.menu");
                }
                Menus_ActivateByName(inMatch ? "settings_ingame" : "settings");
                return;
            }
            case UIMENU_HOME:
                trap_Key_SetCatcher(KEYCATCH_UI);
                Menus_CloseAll();
                Menus_ActivateByName("main");
                return;
            case UIMENU_TEAM:
                trap_Key_SetCatcher(KEYCATCH_UI);
                Menus_ActivateByName("joingame_menu");
                return;
            case UIMENU_POSTGAME:
                trap_Cvar_Set("sv_killserver", "1");
                trap_Key_SetCatcher(KEYCATCH_UI);
                if (uiInfo.inGameLoad) {
                    UI_LoadNonIngame();
                }
                Menus_CloseAll();
                Menus_ActivateByName("endofgame");
                return;
            case UIMENU_INGAME:
                uiInfo.effectsColor = gamecodetoui[(int)trap_Cvar_VariableValue("color1") - 1];
                uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair");
                trap_Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");
                trap_Cvar_Set("ui_cvGameType", "-1");
                trap_Cvar_Set("cl_paused", "1");
                trap_Key_SetCatcher(KEYCATCH_UI);
                UI_BuildPlayerList();
                Menus_CloseAll();
                Menus_ActivateByName("ingame");
                Menus_ActivateByName("ingame_about");
                return;
        }
    }
}

qboolean _UI_IsFullscreen(void) {
    return Menus_AnyFullScreenVisible();
}

qboolean UI_CheckActiveMenu(void) {
    return Menus_AnyVisible();
}

void UI_WalkMenus(void (*callback)(const char* mapLoadName)) {
    int i;

    if (uiInfo.mapCount < 1) {
        UI_LoadArenas();
    }

    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].mapLoadName != NULL) {
            callback(uiInfo.mapList[i].mapLoadName);
        }
    }
}

static connstate_t lastConnState;
static char lastLoadingText[MAX_INFO_VALUE];

static void UI_ReadableSize(char* buf, int bufsize, int value) {
    if (value > 1024 * 1024 * 1024) {
        Com_sprintf(buf, bufsize, "%d", value / (1024 * 1024 * 1024));
        Com_sprintf(buf + strlen(buf), bufsize - strlen(buf), ".%02d GB",
                    (value % (1024 * 1024 * 1024)) * 100 / (1024 * 1024 * 1024));
    } else if (value > 1024 * 1024) {
        Com_sprintf(buf, bufsize, "%d", value / (1024 * 1024));
        Com_sprintf(buf + strlen(buf), bufsize - strlen(buf), ".%02d MB",
                    (value % (1024 * 1024)) * 100 / (1024 * 1024));
    } else if (value > 1024) {
        Com_sprintf(buf, bufsize, "%d KB", value / 1024);
    } else {
        Com_sprintf(buf, bufsize, "%d bytes", value);
    }
}

static void UI_PrintTime(char* buf, int bufsize, int time) {
    time /= 1000;

    if (time > 3600) {
        Com_sprintf(buf, bufsize, "%d hr %d min", time / 3600, (time % 3600) / 60);
    } else if (time > 60) {
        Com_sprintf(buf, bufsize, "%d min %d sec", time / 60, time % 60);
    } else {
        Com_sprintf(buf, bufsize, "%d sec", time);
    }
}

void Text_PaintCenter(float x, float y, float scale, vec4_t color, const char* text, float adjust) {
    int len = Text_Width(text, scale, 0);
    Text_Paint(x - len / 2, y, scale, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

void Text_PaintCenter_AutoWrapped(float x, float y, float xmax, float ystep, float scale, vec4_t color, const char* str, float adjust) {
    int width;
    char *s1, *s2, *s3;
    char c_bcp;
    char buf[1024];

    if (!str || str[0] == '\0')
        return;

    Q_strncpyz(buf, str, sizeof(buf));
    s1 = s2 = s3 = buf;

    while (1) {
        do {
            s3++;
        } while (*s3 != ' ' && *s3 != '\0');
        c_bcp = *s3;
        *s3 = '\0';
        width = Text_Width(s1, scale, 0);
        *s3 = c_bcp;
        if (width > xmax) {
            if (s1 == s2) {
                s2 = s3;
            }
            *s2 = '\0';
            Text_PaintCenter(x, y, scale, color, s1, adjust);
            y += ystep;
            if (c_bcp == '\0') {
                s2++;
                if (*s2 != '\0')
                    Text_PaintCenter(x, y, scale, color, s2, adjust);
                break;
            }
            s2++;
            s1 = s2;
            s3 = s2;
        } else {
            s2 = s3;
            if (c_bcp == '\0')
            {
                Text_PaintCenter(x, y, scale, color, s1, adjust);
                break;
            }
        }
    }
}

static void UI_DisplayDownloadInfo(const char* downloadName, float centerPoint, float yStart, float scale) {
    static char dlText[] = "Downloading:";
    static char etaText[] = "Estimated time left:";
    static char xferText[] = "Transfer rate:";

    int downloadSize, downloadCount, downloadTime;
    char item[64];
    uint64_t done, total;
    char dlSizeBuf[64], totalSizeBuf[64], xferRateBuf[64], dlTimeBuf[64];
    int xferRate;
    int leftWidth;
    const char* s;

    trap_Cvar_VariableStringBuffer("cl_downloadItem", item, sizeof(item));
    trap_WorkshopDownloadInfo(item, &done, &total);
    downloadSize = (int)total;
    downloadCount = (int)done;
    downloadTime = trap_Cvar_VariableValue("cl_downloadTime");

    leftWidth = 320;

    UI_SetColor(colorWhite);
    Text_PaintCenter(centerPoint, yStart + 112, scale, colorWhite, dlText, 0);
    Text_PaintCenter(centerPoint, yStart + 192, scale, colorWhite, etaText, 0);
    Text_PaintCenter(centerPoint, yStart + 248, scale, colorWhite, xferText, 0);

    if (downloadSize > 0) {
        s = va("%s (%d%%)", downloadName,
               (int)((float)downloadCount * 100.0f / downloadSize));
    } else {
        s = downloadName;
    }

    Text_PaintCenter(centerPoint, yStart + 136, scale, colorWhite, s, 0);

    UI_ReadableSize(dlSizeBuf, sizeof dlSizeBuf, downloadCount);
    UI_ReadableSize(totalSizeBuf, sizeof totalSizeBuf, downloadSize);

    if (downloadCount < 4096 || !downloadTime) {
        Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, "estimating", 0);
        Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
    } else {
        if ((uiInfo.uiDC.realTime - downloadTime) / 1000) {
            xferRate = downloadCount / ((uiInfo.uiDC.realTime - downloadTime) / 1000);
        } else {
            xferRate = 0;
        }
        UI_ReadableSize(xferRateBuf, sizeof xferRateBuf, xferRate);

        if (downloadSize && xferRate) {
            int n = downloadSize / xferRate;

            UI_PrintTime(dlTimeBuf, sizeof dlTimeBuf,
                         (n - (((downloadCount / 1024) * n) / (downloadSize / 1024))) * 1000);

            Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, dlTimeBuf, 0);
            Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
        } else {
            Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, "estimating", 0);
            if (downloadSize) {
                Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
            } else {
                Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s copied)", dlSizeBuf), 0);
            }
        }

        if (xferRate) {
            Text_PaintCenter(leftWidth, yStart + 272, scale, colorWhite, va("%s/Sec", xferRateBuf), 0);
        }
    }
}

void UI_DrawConnectScreen(qboolean overlay) {
    char* s;
    uiClientState_t cstate;
    char info[MAX_INFO_VALUE];
    char text[256];
    float centerPoint, yStart, scale;

    menuDef_t* menu = Menus_FindByName("Connect");

    if (!overlay && menu) {
        Menu_Paint(menu, qtrue);
    }

    if (!overlay) {
        centerPoint = 320;
        yStart = 130;
        scale = 0.5f;
    } else {
        return;
    }

    trap_GetClientState(&cstate);

    info[0] = '\0';
    if (trap_GetConfigString(CS_SERVERINFO, info, sizeof(info))) {
        Text_PaintCenter(centerPoint, yStart, scale, colorWhite, va("Loading %s", Info_ValueForKey(info, "mapname")), 0);
    }

    if (!Q_stricmp(cstate.servername, "localhost")) {
        Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, "Starting up...", ITEM_TEXTSTYLE_SHADOWEDMORE);
    } else {
        Com_sprintf(text, sizeof(text), "Connecting to %s", cstate.servername);
        Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, text, ITEM_TEXTSTYLE_SHADOWEDMORE);
    }

    Text_PaintCenter(centerPoint, 600, scale, colorWhite, Info_ValueForKey(cstate.updateInfoString, "motd"), 0);
    if (cstate.connState < CA_CONNECTED) {
        Text_PaintCenter_AutoWrapped(centerPoint, yStart + 176, 630, 20, scale, colorWhite, cstate.messageString, 0);
    }

    if (lastConnState > cstate.connState) {
        lastLoadingText[0] = '\0';
    }
    lastConnState = cstate.connState;

    switch (cstate.connState) {
        case CA_CONNECTING:
            s = va("Awaiting connection...%i", cstate.connectPacketCount);
            break;
        case CA_CHALLENGING:
            s = va("Awaiting challenge...%i", cstate.connectPacketCount);
            break;
        case CA_CONNECTED: {
            char downloadName[MAX_INFO_VALUE];

            trap_Cvar_VariableStringBuffer("cl_downloadName", downloadName, sizeof(downloadName));
            if (*downloadName) {
                UI_DisplayDownloadInfo(downloadName, centerPoint, yStart, scale);
                return;
            }
        }
            s = "Awaiting gamestate...";
            break;
        case CA_LOADING:
            s = "Game initialising...";
            break;
        case CA_PRIMED:
            s = "Awaiting initial frame...";
            break;
        default:
            return;
    }

    if (Q_stricmp(cstate.servername, "localhost")) {
        Text_PaintCenter(centerPoint, yStart + 80, scale, colorWhite, s, 0);
    }

    if (cstate.connState <= CA_CONNECTED) {
        Text_PaintCenter(centerPoint, 440, scale, colorWhite, "Press ESC to cancel", 0);
    }

}

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
    void (*update)(void);
} cvarTable_t;

vmCvar_t ui_ffa_fraglimit;
vmCvar_t ui_ffa_timelimit;

vmCvar_t ui_tourney_fraglimit;
vmCvar_t ui_tourney_timelimit;

vmCvar_t ui_team_fraglimit;
vmCvar_t ui_team_timelimit;
vmCvar_t ui_team_friendly;

vmCvar_t ui_ctf_capturelimit;
vmCvar_t ui_ctf_timelimit;
vmCvar_t ui_ctf_friendly;

vmCvar_t ui_arenasFile;
vmCvar_t ui_botsFile;
vmCvar_t ui_spAwards;
vmCvar_t ui_spVideos;
vmCvar_t ui_spSkill;

vmCvar_t ui_spSelection;

vmCvar_t ui_brassTime;
vmCvar_t ui_drawCrosshair;
vmCvar_t ui_drawCrosshairNames;
vmCvar_t ui_marks;

vmCvar_t ui_redteam;
vmCvar_t ui_blueteam;
vmCvar_t ui_teamName;
vmCvar_t ui_dedicated;
vmCvar_t ui_gameType;
vmCvar_t ui_netGameType;
vmCvar_t ui_actualNetGameType;
vmCvar_t ui_joinGameType;
vmCvar_t ui_netSource;
vmCvar_t ui_serverFilterType;
vmCvar_t ui_opponentName;
vmCvar_t ui_menuFiles;
vmCvar_t ui_currentMap;
vmCvar_t ui_currentNetMap;
vmCvar_t ui_mapIndex;
vmCvar_t ui_currentOpponent;
vmCvar_t ui_selectedPlayer;
vmCvar_t ui_selectedPlayerName;
vmCvar_t ui_scoreAccuracy;
vmCvar_t ui_scoreImpressives;
vmCvar_t ui_scoreExcellents;
vmCvar_t ui_scoreCaptures;
vmCvar_t ui_scoreDefends;
vmCvar_t ui_scoreAssists;
vmCvar_t ui_scoreGauntlets;
vmCvar_t ui_scoreScore;
vmCvar_t ui_scorePerfect;
vmCvar_t ui_scoreTeam;
vmCvar_t ui_scoreBase;
vmCvar_t ui_scoreTimeBonus;
vmCvar_t ui_scoreSkillBonus;
vmCvar_t ui_scoreShutoutBonus;
vmCvar_t ui_scoreTime;
vmCvar_t ui_captureLimit;
vmCvar_t ui_fragLimit;
vmCvar_t ui_smallFont;
vmCvar_t ui_bigFont;
vmCvar_t ui_findPlayer;
vmCvar_t ui_hudFiles;
vmCvar_t ui_recordSPDemo;
vmCvar_t ui_realCaptureLimit;
vmCvar_t ui_realWarmUp;
vmCvar_t ui_serverStatusTimeOut;

vmCvar_t ui_scoreAccuracy2;
vmCvar_t ui_scoreImpressives2;
vmCvar_t ui_scoreExcellents2;
vmCvar_t ui_scoreDefends2;
vmCvar_t ui_scoreAssists2;
vmCvar_t ui_scoreGauntlets2;
vmCvar_t ui_scoreScore2;
vmCvar_t ui_scorePerfect2;
vmCvar_t ui_scoreTeam2;
vmCvar_t ui_scoreBase2;
vmCvar_t ui_scoreTimeBonus2;
vmCvar_t ui_scoreSkillBonus2;
vmCvar_t ui_scoreShutoutBonus2;
vmCvar_t ui_scoreTime2;
vmCvar_t ui_scoreCaptures2;

vmCvar_t ui_forceTeamModel;
vmCvar_t ui_forceTeamSkin;
vmCvar_t ui_forceEnemyModel;
vmCvar_t ui_forceEnemySkin;
vmCvar_t ui_forceTeamModelBright;
vmCvar_t ui_forceEnemyModelBright;
vmCvar_t ui_teamColor;
vmCvar_t ui_enemyColor;
vmCvar_t ui_teamHeadColor;
vmCvar_t ui_teamUpperColor;
vmCvar_t ui_teamLowerColor;
vmCvar_t ui_enemyHeadColor;
vmCvar_t ui_enemyUpperColor;
vmCvar_t ui_enemyLowerColor;

vmCvar_t ui_doWarmup;
vmCvar_t ui_warmup;
vmCvar_t ui_pure;
vmCvar_t ui_friendlyFire;
vmCvar_t ui_cvGameType;
vmCvar_t ui_matchStartTime;
vmCvar_t ui_saveCaptureLimit;
vmCvar_t ui_saveFragLimit;
vmCvar_t ui_votestring;
vmCvar_t ui_intermission;

vmCvar_t ui_browserSortKey;
vmCvar_t ui_browserShowFull;
vmCvar_t ui_browserShowEmpty;
vmCvar_t ui_browserMaster;
vmCvar_t ui_browserGameType;

vmCvar_t ui_screenDamage;
vmCvar_t ui_screenDamage_Team;
vmCvar_t ui_bloomPreset;

vmCvar_t ui_version;
vmCvar_t ui_gibs;
vmCvar_t ui_announcer;
vmCvar_t ui_mainmenu;
vmCvar_t ui_currentTier;
vmCvar_t ui_opponentModel;
vmCvar_t ui_mousePitch;
vmCvar_t ui_favoriteName;
vmCvar_t ui_favoriteAddress;

vmCvar_t ui_cg_announcer;
vmCvar_t ui_cdkeychecked;
vmCvar_t ui_singlePlayerActive;
static cvarTable_t cvarTable[] = {
    {&ui_ffa_fraglimit, "ui_ffa_fraglimit", "20", CVAR_ARCHIVE},
    {&ui_ffa_timelimit, "ui_ffa_timelimit", "0", CVAR_ARCHIVE},

    {&ui_tourney_fraglimit, "ui_tourney_fraglimit", "0", CVAR_ARCHIVE},
    {&ui_tourney_timelimit, "ui_tourney_timelimit", "15", CVAR_ARCHIVE},

    {&ui_team_fraglimit, "ui_team_fraglimit", "0", CVAR_ARCHIVE},
    {&ui_team_timelimit, "ui_team_timelimit", "20", CVAR_ARCHIVE},
    {&ui_team_friendly, "ui_team_friendly", "1", CVAR_ARCHIVE},

    {&ui_ctf_capturelimit, "ui_ctf_capturelimit", "8", CVAR_ARCHIVE},
    {&ui_ctf_timelimit, "ui_ctf_timelimit", "30", CVAR_ARCHIVE},
    {&ui_ctf_friendly, "ui_ctf_friendly", "0", CVAR_ARCHIVE},

    {&ui_arenasFile, "g_arenasFile", "", CVAR_ROM | CVAR_INIT},
    {&ui_botsFile, "g_botsFile", "", CVAR_ROM | CVAR_INIT},

    {&ui_brassTime, "cg_brassTime", "2500", CVAR_ARCHIVE},
    {&ui_drawCrosshair, "cg_drawCrosshair", "2", CVAR_ARCHIVE},
    {&ui_drawCrosshairNames, "cg_enemyCrosshairNames", "1", CVAR_ARCHIVE},
    {&ui_marks, "cg_marks", "1", CVAR_ARCHIVE},

    {&ui_debug, "ui_debug", "0", CVAR_TEMP},
    {&ui_initialized, "ui_initialized", "0", CVAR_TEMP},
    {&ui_dedicated, "ui_dedicated", "0", CVAR_ARCHIVE},
    {&ui_gameType, "ui_gametype", "3", CVAR_ARCHIVE},
    {&ui_joinGameType, "ui_joinGametype", "0", CVAR_ARCHIVE},
    {&ui_netGameType, "ui_netGametype", "3", CVAR_ARCHIVE},
    {&ui_actualNetGameType, "ui_actualNetGametype", "3", CVAR_ARCHIVE},
    {&ui_blueteam, "ui_blueteam", "Stroggs", CVAR_ARCHIVE},
    {&ui_redteam, "ui_redteam", "Pagans", CVAR_ARCHIVE},
    {&ui_netSource, "ui_netSource", "0", CVAR_ARCHIVE},
    {&ui_menuFiles, "ui_menuFiles", "ui/menus.txt", CVAR_ARCHIVE},
    {&ui_currentMap, "ui_currentMap", "0", CVAR_ARCHIVE},
    {&ui_currentNetMap, "ui_currentNetMap", "0", CVAR_ARCHIVE},
    {&ui_mapIndex, "ui_mapIndex", "0", CVAR_ARCHIVE},
    {&ui_currentOpponent, "ui_currentOpponent", "0", CVAR_ARCHIVE},
    {&ui_selectedPlayer, "cg_selectedPlayer", "0", CVAR_ARCHIVE},
    {&ui_selectedPlayerName, "cg_selectedPlayerName", "", CVAR_ARCHIVE},
    {&ui_scoreAccuracy, "ui_scoreAccuracy", "0", CVAR_ARCHIVE},
    {&ui_scoreImpressives, "ui_scoreImpressives", "0", CVAR_ARCHIVE},
    {&ui_scoreExcellents, "ui_scoreExcellents", "0", CVAR_ARCHIVE},
    {&ui_scoreCaptures, "ui_scoreCaptures", "0", CVAR_ARCHIVE},
    {&ui_scoreDefends, "ui_scoreDefends", "0", CVAR_ARCHIVE},
    {&ui_scoreAssists, "ui_scoreAssists", "0", CVAR_ARCHIVE},
    {&ui_scoreGauntlets, "ui_scoreGauntlets", "0", CVAR_ARCHIVE},
    {&ui_scoreScore, "ui_scoreScore", "0", CVAR_ARCHIVE},
    {&ui_scorePerfect, "ui_scorePerfect", "0", CVAR_ARCHIVE},
    {&ui_scoreTeam, "ui_scoreTeam", "0 to 0", CVAR_ARCHIVE},
    {&ui_scoreBase, "ui_scoreBase", "0", CVAR_ARCHIVE},
    {&ui_scoreTime, "ui_scoreTime", "00:00", CVAR_ARCHIVE},
    {&ui_scoreTimeBonus, "ui_scoreTimeBonus", "0", CVAR_ARCHIVE},
    {&ui_scoreSkillBonus, "ui_scoreSkillBonus", "0", CVAR_ARCHIVE},
    {&ui_scoreShutoutBonus, "ui_scoreShutoutBonus", "0", CVAR_ARCHIVE},
    {&ui_fragLimit, "ui_fragLimit", "10", 0},
    {&ui_captureLimit, "ui_captureLimit", "5", 0},
    {&ui_smallFont, "ui_smallFont", "0.25", CVAR_REPLICATE | CVAR_ARCHIVE},
    {&ui_bigFont, "ui_bigFont", "0.4", CVAR_REPLICATE | CVAR_ARCHIVE},
    {&ui_findPlayer, "ui_findPlayer", "Sarge", CVAR_ARCHIVE},
    {&ui_hudFiles, "cg_hudFiles", "ui/hud.txt", CVAR_ARCHIVE},
    {&ui_recordSPDemo, "ui_recordSPDemo", "0", CVAR_ARCHIVE},
    {&ui_realWarmUp, "g_warmup", "10", CVAR_ARCHIVE},
    {&ui_realCaptureLimit, "capturelimit", "8", CVAR_NORESTART | CVAR_SERVERINFO | CVAR_ARCHIVE},
    {&ui_serverStatusTimeOut, "ui_serverStatusTimeOut", "7000", CVAR_ARCHIVE},

    {&ui_scoreAccuracy2, "ui_scoreAccuracy2", "0", CVAR_ARCHIVE},
    {&ui_scoreImpressives2, "ui_scoreImpressives2", "0", CVAR_ARCHIVE},
    {&ui_scoreExcellents2, "ui_scoreExcellents2", "0", CVAR_ARCHIVE},
    {&ui_scoreDefends2, "ui_scoreDefends2", "0", CVAR_ARCHIVE},
    {&ui_scoreAssists2, "ui_scoreAssists2", "0", CVAR_ARCHIVE},
    {&ui_scoreGauntlets2, "ui_scoreGauntlets2", "0", CVAR_ARCHIVE},
    {&ui_scoreScore2, "ui_scoreScore2", "0", CVAR_ARCHIVE},
    {&ui_scorePerfect2, "ui_scorePerfect2", "0", CVAR_ARCHIVE},
    {&ui_scoreTeam2, "ui_scoreTeam2", "0 to 0", CVAR_ARCHIVE},
    {&ui_scoreBase2, "ui_scoreBase2", "0", CVAR_ARCHIVE},
    {&ui_scoreTimeBonus2, "ui_scoreTimeBonus2", "0", CVAR_ARCHIVE},
    {&ui_scoreSkillBonus2, "ui_scoreSkillBonus2", "0", CVAR_ARCHIVE},
    {&ui_scoreShutoutBonus2, "ui_scoreShutoutBonus2", "0", CVAR_ARCHIVE},
    {&ui_scoreTime2, "ui_scoreTime2", "00:00", CVAR_ARCHIVE},
    {&ui_scoreCaptures2, "ui_scoreCaptures2", "0", CVAR_ARCHIVE},

    {&ui_forceTeamModel, "ui_forceTeamModel", "", CVAR_TEMP},
    {&ui_forceTeamSkin, "ui_forceTeamSkin", "", CVAR_TEMP},
    {&ui_forceEnemyModel, "ui_forceEnemyModel", "", CVAR_TEMP},
    {&ui_forceEnemySkin, "ui_forceEnemySkin", "", CVAR_TEMP},
    {&ui_forceTeamModelBright, "ui_forceTeamModelBright", "0", CVAR_ROM},
    {&ui_forceEnemyModelBright, "ui_forceEnemyModelBright", "0", CVAR_ROM},
    {&ui_teamColor, "ui_teamColor", "0", CVAR_ARCHIVE},
    {&ui_enemyColor, "ui_enemyColor", "0", CVAR_ARCHIVE},
    {&ui_teamHeadColor, "ui_teamHeadColor", "96", CVAR_ARCHIVE},
    {&ui_teamUpperColor, "ui_teamUpperColor", "96", CVAR_ARCHIVE},
    {&ui_teamLowerColor, "ui_teamLowerColor", "96", CVAR_ARCHIVE},
    {&ui_enemyHeadColor, "ui_enemyHeadColor", "27", CVAR_ARCHIVE},
    {&ui_enemyUpperColor, "ui_enemyUpperColor", "27", CVAR_ARCHIVE},
    {&ui_enemyLowerColor, "ui_enemyLowerColor", "27", CVAR_ARCHIVE},

    {&ui_doWarmup, "ui_doWarmup", "0", CVAR_ARCHIVE},
    {&ui_warmup, "ui_warmup", "0", CVAR_ARCHIVE},
    {&ui_pure, "ui_pure", "1", CVAR_ARCHIVE},
    {&ui_friendlyFire, "ui_friendlyFire", "1", CVAR_ARCHIVE},
    {&ui_cvGameType, "ui_cvGameType", "-1", 0},
    {&ui_matchStartTime, "ui_matchStartTime", "0", CVAR_ROM},
    {&ui_saveCaptureLimit, "ui_saveCaptureLimit", "5", CVAR_ARCHIVE},
    {&ui_saveFragLimit, "ui_saveFragLimit", "10", CVAR_ARCHIVE},
    {&ui_votestring, "ui_votestring", "", CVAR_TEMP},
    {&ui_intermission, "ui_intermission", "0", CVAR_ROM},

    {&ui_browserSortKey, "ui_browserSortKey", "4", CVAR_ARCHIVE},
    {&ui_browserShowFull, "ui_browserShowFull", "1", CVAR_ARCHIVE},
    {&ui_browserShowEmpty, "ui_browserShowEmpty", "1", CVAR_ARCHIVE},
    {&ui_browserMaster, "ui_browserMaster", "0", CVAR_ARCHIVE},
    {&ui_browserGameType, "ui_browserGameType", "0", CVAR_ARCHIVE},

    {&ui_screenDamage, "ui_screenDamage", "0", CVAR_ARCHIVE},
    {&ui_screenDamage_Team, "ui_screenDamage_Team", "0", CVAR_ARCHIVE},
    {&ui_bloomPreset, "ui_bloomPreset", "Default", CVAR_ARCHIVE},

    {&ui_version, "ui_version", "1069 win-x86 May 25 2016 15:18:22", CVAR_ROM},
    {&ui_gibs, "ui_gibs", "1", CVAR_ROM},
    {&ui_announcer, "ui_announcer", "1", CVAR_ARCHIVE},
    {&ui_mainmenu, "ui_mainmenu", "1", CVAR_ROM},
    {&ui_currentTier, "ui_currentTier", "0", CVAR_ARCHIVE},
    {&ui_opponentModel, "ui_opponentModel", "sarge", CVAR_ARCHIVE},
    {&ui_mousePitch, "ui_mousePitch", "0", CVAR_ARCHIVE},
    {&ui_favoriteName, "ui_favoriteName", "", CVAR_ARCHIVE},
    {&ui_favoriteAddress, "ui_favoriteAddress", "", CVAR_ARCHIVE},

    {NULL, "server1", "", CVAR_ARCHIVE},
    {NULL, "server2", "", CVAR_ARCHIVE},
    {NULL, "server3", "", CVAR_ARCHIVE},
    {NULL, "server4", "", CVAR_ARCHIVE},
    {NULL, "server5", "", CVAR_ARCHIVE},
    {NULL, "server6", "", CVAR_ARCHIVE},
    {NULL, "server7", "", CVAR_ARCHIVE},
    {NULL, "server8", "", CVAR_ARCHIVE},
    {NULL, "server9", "", CVAR_ARCHIVE},
    {NULL, "server10", "", CVAR_ARCHIVE},
    {NULL, "server11", "", CVAR_ARCHIVE},
    {NULL, "server12", "", CVAR_ARCHIVE},
    {NULL, "server13", "", CVAR_ARCHIVE},
    {NULL, "server14", "", CVAR_ARCHIVE},
    {NULL, "server15", "", CVAR_ARCHIVE},
    {NULL, "server16", "", CVAR_ARCHIVE},

    {NULL, "ui_blueteam1", "0", CVAR_ARCHIVE},
    {NULL, "ui_blueteam2", "0", CVAR_ARCHIVE},
    {NULL, "ui_blueteam3", "0", CVAR_ARCHIVE},
    {NULL, "ui_blueteam4", "0", CVAR_ARCHIVE},
    {NULL, "ui_blueteam5", "0", CVAR_ARCHIVE},
    {NULL, "ui_redteam1", "0", CVAR_ARCHIVE},
    {NULL, "ui_redteam2", "0", CVAR_ARCHIVE},
    {NULL, "ui_redteam3", "0", CVAR_ARCHIVE},
    {NULL, "ui_redteam4", "0", CVAR_ARCHIVE},
    {NULL, "ui_redteam5", "0", CVAR_ARCHIVE},

    {NULL, "ui_lastServerRefresh_0", "", CVAR_ARCHIVE},
    {NULL, "ui_lastServerRefresh_1", "", CVAR_ARCHIVE},
    {NULL, "ui_lastServerRefresh_2", "", CVAR_ARCHIVE},
    {NULL, "ui_lastServerRefresh_3", "", CVAR_ARCHIVE},

    {NULL, "ui_teamName", "Pagans", CVAR_ARCHIVE},
    {NULL, "ui_new", "0", CVAR_TEMP},
    {NULL, "ui_priv", "0", CVAR_ROM},
    {NULL, "g_spAwards", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "g_spSkill", "2", CVAR_ARCHIVE},
    {NULL, "g_spScores1", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "g_spScores2", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "g_spScores3", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "g_spScores4", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "g_spScores5", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "g_spVideos", "", CVAR_ROM | CVAR_ARCHIVE},
    {NULL, "ui_spSelection", "", CVAR_ROM},

    {NULL, "ui_videomode", "", CVAR_ROM},
    {&ui_cg_announcer, "cg_announcer", "1", CVAR_TEMP, UI_OnAnnouncerChanged},
    {&ui_cdkeychecked, "ui_cdkeychecked", "0", CVAR_ROM},
    {&ui_opponentName, "ui_opponentName", "Stroggs", CVAR_ARCHIVE},
    {&ui_singlePlayerActive, "ui_singlePlayerActive", "0", 0},
};

static int cvarTableSize = ARRAY_LEN(cvarTable);

static void UI_CheckModelBright(const char* modelCvar, const char* skinCvar, const char* targetCvar) {
    char model[1024];
    char skin[1024];
    int i;

    trap_Cvar_VariableStringBuffer(modelCvar, model, sizeof(model));
    trap_Cvar_VariableStringBuffer(skinCvar, skin, sizeof(skin));

    for (i = 0; i < 0x40 && model[i]; i++) {
        model[i] = (char)tolower(model[i]);
    }
    for (i = 0; i < 0x40 && skin[i]; i++) {
        skin[i] = (char)tolower(skin[i]);
    }

    if (strstr(skin, "bright") || (strstr(model, "bright") && skin[0] == '\0')) {
        trap_Cvar_Set(targetCvar, "1");
    } else {
        trap_Cvar_Set(targetCvar, "0");
    }
}

static void UI_OnAnnouncerChanged(void) {
    sfxHandle_t s;

    if (ui_cg_announcer.integer == 2) {
        s = trap_S_RegisterSound("sound/misc/vo_evil.wav", 7);
    } else if (ui_cg_announcer.integer == 3) {
        s = trap_S_RegisterSound("sound/misc/vo_female.wav", 7);
    } else {
        s = trap_S_RegisterSound("sound/misc/vo_default.wav", 7);
    }
    trap_S_StartLocalSound(s, CHAN_ANNOUNCER);
    trap_Cvar_Set("ui_announcer", va("%i", ui_cg_announcer.integer));
}

void UI_RegisterCvars(void) {
    int i;
    cvarTable_t* cv;

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        trap_Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
    }

    UI_CheckModelBright("cg_forceTeamModel", "cg_forceTeamSkin", "ui_forceTeamModelBright");
    UI_CheckModelBright("cg_forceEnemyModel", "cg_forceEnemySkin", "ui_forceEnemyModelBright");
}

void UI_UpdateCvars(void) {
    int i;
    cvarTable_t* cv;
    int oldMod;

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        if (!cv->vmCvar) {
            continue;
        }

        oldMod = cv->vmCvar->modificationCount;
        trap_Cvar_Update(cv->vmCvar);
        if (cv->update && oldMod != 0 && oldMod != cv->vmCvar->modificationCount) {
            cv->update();
        }
    }
}
