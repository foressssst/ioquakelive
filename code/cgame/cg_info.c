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
// cg_info.c -- display information while data is being loading

#include "cg_local.h"
#include "../../ui/menudef.h"

#define MAX_LOADING_PLAYER_ICONS 16
#define MAX_LOADING_ITEM_ICONS 26

// [QL] loading screen progress has 4 stages
#define NUM_LOADING_STAGES 4

static int loadingPlayerIconCount;
static int loadingItemIconCount;
static qhandle_t loadingPlayerIcons[MAX_LOADING_PLAYER_ICONS];
static qhandle_t loadingItemIcons[MAX_LOADING_ITEM_ICONS];
// [ioquakelive] each client's icon by slot, for the loading screen's player list
static qhandle_t loadingClientIcons[MAX_CLIENTS];

// [QL] use shared gametypeDisplayNames[] from bg_misc.c

/*
======================
CG_LoadingString

======================
*/
void CG_LoadingString(const char* s) {
    Q_strncpyz(cg.infoScreenText, s, sizeof(cg.infoScreenText));

    trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem(int itemNum) {
    gitem_t* item;

    item = &bg_itemlist[itemNum];

    if (item->icon && loadingItemIconCount < MAX_LOADING_ITEM_ICONS) {
        loadingItemIcons[loadingItemIconCount++] = trap_R_RegisterShaderNoMip(item->icon);
    }

    CG_LoadingString(item->pickup_name);
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient(int clientNum) {
    const char* info;
    char* skin;
    char personality[MAX_QPATH];
    char model[MAX_QPATH];
    char iconName[MAX_QPATH];

    info = CG_ConfigString(CS_PLAYERS + clientNum);

    if (loadingPlayerIconCount < MAX_LOADING_PLAYER_ICONS) {
        Q_strncpyz(model, Info_ValueForKey(info, "model"), sizeof(model));
        skin = strrchr(model, '/');
        if (skin) {
            *skin++ = '\0';
        } else {
            skin = "default";
        }

        Com_sprintf(iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", model, skin);

        loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip(iconName);
        if (!loadingPlayerIcons[loadingPlayerIconCount]) {
            Com_sprintf(iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", DEFAULT_MODEL, "default");
            loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip(iconName);
        }
        if (loadingPlayerIcons[loadingPlayerIconCount]) {
            if (clientNum >= 0 && clientNum < MAX_CLIENTS) {
                loadingClientIcons[clientNum] = loadingPlayerIcons[loadingPlayerIconCount];
            }
            loadingPlayerIconCount++;
        }
    }

    Q_strncpyz(personality, Info_ValueForKey(info, "n"), sizeof(personality));

    CG_LoadingString(personality);
}

/*
===============================================================================

THE LOADING SCREEN

[ioquakelive] Retail's (cgamex86.dll 0x1001ce10) is the levelshot under two red bars with the
logo, the gametype, the map and its authors, "Loading ..." and four boxes of progress. This one
carries the same and a little more, in the front end's language, laid out the way the user chose
on 2026-09-16 ("Jugadores entrando"): the levelshot over the whole screen; the logo at the top
left; the players who are in the match in a panel at the right, as they come in; the four steps
of the load under it, each marked as it finishes; and the gametype, the map and its authors at
the bottom left. A gold thread along the bottom edge keeps the overall progress.

Everything is laid out in 480-line units from the screen's left edge and drawn in screen pixels,
so it reaches both edges of any screen and no widescreen mode moves it.

===============================================================================
*/

static vec4_t loadPanel = {0.059f, 0.059f, 0.059f, 0.90f};
static vec4_t loadRaised = {0.102f, 0.102f, 0.102f, 1.0f};
static vec4_t loadPage = {0.031f, 0.031f, 0.031f, 1.0f};
static vec4_t loadText = {0.867f, 0.867f, 0.867f, 1.0f};
static vec4_t loadMuted = {0.600f, 0.600f, 0.600f, 1.0f};
static vec4_t loadSubtle = {0.416f, 0.416f, 0.416f, 1.0f};
static vec4_t loadGold = {0.969f, 0.820f, 0.0f, 1.0f};
static vec4_t loadTrack = {1.0f, 1.0f, 1.0f, 0.12f};

// a 480-line unit in screen pixels
static float CG_LoadUnit(void) {
    return (float)cgs.glconfig.vidHeight / 480.0f;
}

static void CG_LoadFill(float x, float y, float w, float h, const float* color) {
    float u = CG_LoadUnit();

    trap_R_SetColor(color);
    trap_R_DrawStretchPic(x * u, y * u, w * u, h * u, 0, 0, 0, 0, cgs.media.whiteShader);
    trap_R_SetColor(NULL);
}

static void CG_LoadPic(float x, float y, float w, float h, qhandle_t shader) {
    float u = CG_LoadUnit();

    trap_R_DrawStretchPic(x * u, y * u, w * u, h * u, 0, 0, 1, 1, shader);
}

// a rounded rectangle, the corners drawn in slices a screen pixel thick
static void CG_LoadRound(float x, float y, float w, float h, float r, const float* color) {
    float u = CG_LoadUnit();
    float step;
    int slices, k;

    if (r * 2.0f > h) {
        r = h * 0.5f;
    }
    if (r * 2.0f > w) {
        r = w * 0.5f;
    }
    CG_LoadFill(x, y + r, w, h - 2.0f * r, color);
    slices = (int)(r * u + 0.5f);
    if (slices < 1) {
        slices = 1;
    } else if (slices > 24) {
        slices = 24;
    }
    step = r / slices;
    for (k = 0; k < slices; k++) {
        float mid = r - (k + 0.5f) * step;
        float inset = r - (float)sqrt(r * r - mid * mid);

        CG_LoadFill(x + inset, y + k * step, w - 2.0f * inset, step, color);
        CG_LoadFill(x + inset, y + h - (k + 1) * step, w - 2.0f * inset, step, color);
    }
}

// a colour whose alpha runs from a0 to a1 over a rectangle, downwards or rightwards, eased, in bands
// whose edges are whole pixels so no line shows where two meet
static void CG_LoadWash(float x, float y, float w, float h, const float* color, float a0, float a1, qboolean across) {
    float u = CG_LoadUnit();
    float from = (across ? x : y) * u;
    float to = (across ? x + w : y + h) * u;
    int first = (int)(from + 0.5f), last = (int)(to + 0.5f);
    int p;
    vec4_t c;

    if (to <= from) {
        return;
    }
    VectorCopy(color, c);
    for (p = first; p < last; p += 2) {
        int end = p + 2 < last ? p + 2 : last;
        float t = ((p + end) * 0.5f - from) / (to - from);

        t = t * t * (3.0f - 2.0f * t);
        c[3] = a0 + (a1 - a0) * t;
        trap_R_SetColor(c);
        if (across) {
            trap_R_DrawStretchPic((float)p, y * u, (float)(end - p), h * u, 0, 0, 0, 0, cgs.media.whiteShader);
        } else {
            trap_R_DrawStretchPic(x * u, (float)p, w * u, (float)(end - p), 0, 0, 0, 0, cgs.media.whiteShader);
        }
    }
    trap_R_SetColor(NULL);
}

// text with its top at y (CG_DrawText takes the baseline), in the current widescreen mode
static void CG_LoadText(float x, float y, int font, float scale, vec4_t color, const char* text) {
    CG_DrawText(x, y + CG_DrawTextHeight(text, scale, 0, font), font, scale, color, (char*)text, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);
}

// a label as the front end draws them: upper case and letter-spaced
static float CG_LoadLabel(float x, float y, float scale, vec4_t color, const char* text, float spacing) {
    char c[2];
    const char* p;
    float h = 0.0f;

    c[1] = '\0';
    for (p = text; *p; p++) {
        int ch = *p;

        c[0] = (char)((ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch);
        if (!h) {
            h = (float)CG_DrawTextHeight("H", scale, 0, 0);
        }
        CG_DrawText(x, y + h, 0, scale, color, c, 0, 0, ITEM_TEXTSTYLE_NORMAL);
        x += CG_DrawTextWidth(c, scale, 0, 0) + spacing;
    }
    return x;
}

static float CG_LoadLabelWidth(float scale, const char* text, float spacing) {
    char c[2];
    const char* p;
    float w = 0.0f;

    c[1] = '\0';
    for (p = text; *p; p++) {
        int ch = *p;

        c[0] = (char)((ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch);
        w += CG_DrawTextWidth(c, scale, 0, 0) + spacing;
    }
    return w > 0.0f ? w - spacing : 0.0f;
}

// a small check mark, 9 by 7 units
static void CG_LoadCheck(float x, float y, const float* color) {
    int k;

    for (k = 0; k < 4; k++) {
        CG_LoadFill(x + k, y + 3.0f + k, 1.5f, 1.5f, color);
    }
    for (k = 0; k < 6; k++) {
        CG_LoadFill(x + 3.0f + k, y + 6.0f - k, 1.5f, 1.5f, color);
    }
}

/*
===============
CG_LoadDrawPlayers

Who is in the match, in the order of their slots, from the player configstrings, which the
gamestate brings before anything is loaded. A player's icon shows once their model has been
loaded (CG_LoadingClient); until then a plain square stands in for it. The local player is
marked, and so are the bots.
===============
*/
static void CG_LoadDrawPlayers(float x, float y, float w, float maxH) {
    const float rowH = 22.0f;
    const float head = 40.0f;
    int maxRows = (int)((maxH - head - 12.0f) / rowH);
    int count = 0, shown = 0;
    char name[MAX_QPATH];
    char line[32];
    float h, cw;
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (CG_ConfigString(CS_PLAYERS + i)[0]) {
            count++;
        }
    }
    if (maxRows < 1) {
        maxRows = 1;
    }
    h = head + (count < 1 ? 1 : count > maxRows ? maxRows : count) * rowH + 12.0f;
    CG_LoadRound(x, y, w, h, 6.0f, loadPanel);

    CG_LoadLabel(x + 14, y + 14, 0.15f, loadText, "Players", 1.4f);
    Com_sprintf(line, sizeof(line), "%d / %d", count, cgs.maxclients);
    cw = CG_DrawTextWidth(line, 0.15f, 0, 1);
    CG_LoadText(x + w - 14 - cw, y + 14, 1, 0.15f, loadSubtle, line);

    if (!count) {
        CG_LoadText(x + 14, y + head + 4, 1, 0.16f, loadSubtle, "Nobody yet");
        return;
    }
    for (i = 0; i < MAX_CLIENTS && shown < maxRows; i++) {
        const char* info = CG_ConfigString(CS_PLAYERS + i);
        float ry = y + head + shown * rowH;
        qboolean you = (i == cg.clientNum);
        qboolean bot = Info_ValueForKey(info, "skill")[0] != '\0';
        const char* tag = you ? "You" : bot ? "Bot" : NULL;
        float nameW = w - 44 - 14;

        if (!info[0]) {
            continue;
        }
        if (loadingClientIcons[i]) {
            CG_LoadPic(x + 14, ry + 2, 16, 16, loadingClientIcons[i]);
        } else {
            CG_LoadRound(x + 14, ry + 2, 16, 16, 3.0f, loadRaised);
        }
        if (tag) {
            float tw = CG_LoadLabelWidth(0.12f, tag, 1.2f);

            CG_LoadLabel(x + w - 14 - tw, ry + 7, 0.12f, you ? loadGold : loadSubtle, tag, 1.2f);
            nameW -= tw + 10;
        }
        Q_strncpyz(name, Info_ValueForKey(info, "n"), sizeof(name));
        Q_CleanStr(name);
        while (name[0] && CG_DrawTextWidth(name, 0.17f, 0, 1) > nameW) {
            name[strlen(name) - 1] = '\0';
        }
        CG_LoadText(x + 40, ry + 4, 1, 0.17f, you ? loadText : loadMuted, name);
        shown++;
    }
}

/*
===============
CG_LoadDrawSteps

The four steps of CG_Init's load, each with its state: a check when it is done, "loading" in gold
while it runs, nothing while it waits. Under them, what is being loaded right now.
===============
*/
static void CG_LoadDrawSteps(float x, float y, float w) {
    static const char* const steps[NUM_LOADING_STAGES] = {"Map", "Sounds", "Graphics", "Players"};
    int i;

    for (i = 0; i < NUM_LOADING_STAGES; i++) {
        float ry = y + i * 18.0f;
        qboolean done = cg.loadingStage > i;
        qboolean current = cg.loadingStage == i;

        CG_LoadLabel(x, ry, 0.14f, done ? loadMuted : current ? loadText : loadSubtle, steps[i], 1.3f);
        if (done) {
            CG_LoadCheck(x + w - 10, ry + 1, loadMuted);
        } else if (current) {
            float lw = CG_LoadLabelWidth(0.12f, "Loading", 1.2f);

            CG_LoadLabel(x + w - lw, ry + 1, 0.12f, loadGold, "Loading", 1.2f);
        }
    }
    if (cg.infoScreenText[0]) {
        char what[64];

        Q_strncpyz(what, cg.infoScreenText, sizeof(what));
        Q_CleanStr(what);
        while (what[0] && CG_DrawTextWidth(what, 0.15f, 0, 1) > w) {
            what[strlen(what) - 1] = '\0';
        }
        CG_LoadText(x, y + NUM_LOADING_STAGES * 18.0f + 4.0f, 1, 0.15f, loadSubtle, what);
    }
}

/*
====================
CG_DrawInformation
====================
*/
void CG_DrawInformation(void) {
    const char* s;
    const char* info;
    const char* sAuthor;
    const char* gametypeName;
    qhandle_t levelshot;
    float s1, s2;
    float full, side, sideX;
    int w;
    char host[64];

    // [QL] compute aspect-correct texture coords for levelshot (1920x1080 source)
    s1 = ((1920.0f - (1080.0f / (float)cgs.glconfig.vidHeight) * (float)cgs.glconfig.vidWidth) / 1920.0f) * 0.5f;
    s2 = 1.0f - s1;

    info = CG_ConfigString(CS_SERVERINFO);

    // [QL] retail reads the custom settings again here, on every call
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));

    // register levelshot
    s = Info_ValueForKey(info, "mapname");
    levelshot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", s));
    if (!levelshot) {
        levelshot = trap_R_RegisterShaderNoMip("menu/art/unknownmap");
    }
    trap_R_SetColor(NULL);

    // [QL] cgamex86.dll 0x1001ce10 (0x1001cf80): with a snapshot and nothing left to load, only
    // the smoke backdrop over the whole screen and "Awaiting gamestate..." centred at y 178, in
    // font 0 at 0.3, shadowed
    if (!cg.infoScreenText[0] && cg.snap) {
        CG_SetWidescreen(WIDESCREEN_CENTER);
        trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                              cgs.media.backscreenSmokeShader);
        s = "Awaiting gamestate...";
        w = CG_DrawTextWidth(s, 0.3f, 0, 0);
        CG_DrawText(320 - w * 0.5f, 178, 0, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
        CG_SetWidescreen(WIDESCREEN_STRETCH);
        return;
    }

    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                          levelshot);

    CG_SetWidescreen(WIDESCREEN_LEFT);
    full = (float)cgs.glconfig.vidWidth * 480.0f / (float)cgs.glconfig.vidHeight;
    side = 196.0f;
    sideX = full - 24.0f - side;

    // the picture keeps its middle; it darkens under the logo, towards the words at the bottom and
    // behind the column at the right
    CG_LoadWash(0, 0, full, 96, loadPage, 0.62f, 0.0f, qfalse);
    CG_LoadWash(0, 230, full, 250, loadPage, 0.0f, 0.94f, qfalse);
    CG_LoadWash(full * 0.45f, 0, full * 0.55f, 480, loadPage, 0.0f, 0.72f, qtrue);

    if (cgs.media.qlLogoShader) {
        CG_LoadPic(22, 20, 160, 40, cgs.media.qlLogoShader);
    }

    // the server's name at the top right, when it has one
    Q_strncpyz(host, Info_ValueForKey(info, "sv_hostname"), sizeof(host));
    Q_CleanStr(host);
    if (host[0] && Q_stricmp(host, "noname")) {
        while (host[0] && CG_DrawTextWidth(host, 0.16f, 0, 1) > side) {
            host[strlen(host) - 1] = '\0';
        }
        w = CG_DrawTextWidth(host, 0.16f, 0, 1);
        CG_LoadText(full - 24 - w, 34, 1, 0.16f, loadMuted, host);
    }

    CG_LoadDrawPlayers(sideX, 72, side, 290);
    CG_LoadDrawSteps(sideX + 2, 384, side - 4);

    // the gametype over the map's name, and its authors under it
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        gametypeName = gametypeDisplayNames[cgs.gametype];
    } else {
        gametypeName = "Unknown Gametype";
    }
    CG_LoadLabel(26, 384, 0.16f, loadGold, gametypeName, 1.6f);
    s = CG_ConfigString(CS_MESSAGE);
    CG_LoadText(24, 400, 0, 0.55f, loadText, s);
    sAuthor = CG_ConfigString(CS_AUTHOR);
    if (sAuthor[0]) {
        const char* sAuthor2 = CG_ConfigString(CS_AUTHOR2);

        if (sAuthor2[0]) {
            sAuthor = va("%s  %s", sAuthor, sAuthor2);
        }
        CG_LoadText(26, 446, 1, 0.17f, loadMuted, sAuthor);
    }

    // the overall progress along the very bottom
    CG_LoadFill(0, 477, full, 3, loadTrack);
    if (cg.loadingStage > 0) {
        float done = (float)cg.loadingStage / (float)NUM_LOADING_STAGES;

        CG_LoadFill(0, 477, full * (done > 1.0f ? 1.0f : done), 3, loadGold);
    }

    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
