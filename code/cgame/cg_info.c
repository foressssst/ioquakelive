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
            loadingPlayerIconCount++;
        }
    }

    Q_strncpyz(personality, Info_ValueForKey(info, "n"), sizeof(personality));

    CG_LoadingString(personality);
}

/*
====================
CG_DrawInformation

[QL] Rewritten to match Quake Live binary loading screen.
Draws levelshot, QL branding, gametype, loading text, and progress bar.
====================
*/
void CG_DrawInformation(void) {
    const char *s;
    const char *info;
    const char *sAuthor, *sAuthor2;
    const char *gametypeName;
    qhandle_t levelshot;
    int i;
    int w;
    float s1, s2;

    // the front end's palette (ui_web.c): the page's black, its gold, its muted grey
    static vec4_t colorLoadFloor = {0.031f, 0.031f, 0.031f, 0.86f};
    static vec4_t colorLoadGold  = {0.969f, 0.820f, 0.0f,   1.0f};
    static vec4_t colorLoadMuted = {0.600f, 0.600f, 0.600f, 1.0f};
    static vec4_t colorLoadTrack = {1.0f,   1.0f,   1.0f,   0.12f};
    char eyebrow[64];
    float full;

    // [QL] compute aspect-correct texture coords for levelshot (1920x1080 source)
    s1 = ((1920.0f - (1080.0f / (float)cgs.glconfig.vidHeight) * (float)cgs.glconfig.vidWidth)
          / 1920.0f) * 0.5f;
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
        trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight,
                              s1, 0, s2, 1.0f, cgs.media.backscreenSmokeShader);
        s = "Awaiting gamestate...";
        w = CG_DrawTextWidth(s, 0.3f, 0, 0);
        CG_DrawText(320 - w * 0.5f, 178, 0, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
        CG_SetWidescreen(WIDESCREEN_STRETCH);
        return;
    }

    // the levelshot fills the screen, with retail's aspect-correct texture coords
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight,
                          s1, 0, s2, 1.0f, levelshot);

    // [ioquakelive] Asked for on 2026-09-16: "ver la posibilidad de un nuevo diseno para LA
    // PANTALLA DE CARGA DEL MAPA". It carries exactly what retail's carried -- the levelshot, the
    // logo, the gametype, the map and its authors, what is loading and how far it has got -- in
    // the front end's own language instead of retail's red bars: a scrim that darkens towards the
    // bottom so the picture keeps its top, the gametype as a gold eyebrow over the map's name, and
    // the progress as a thread along the bottom edge. Retail's version is in the history, at the
    // commit this replaced.
    // Everything is laid out from the left edge in 480-line units, so it reaches both edges of any
    // screen: in WIDESCREEN_LEFT a unit is a 480th of the height and x 0 is the screen's left.
    CG_SetWidescreen(WIDESCREEN_LEFT);
    full = (float)cgs.glconfig.vidWidth * 480.0f / (float)cgs.glconfig.vidHeight;

    // the scrim: a light one under the logo, a deeper one towards the floor the text sits on
    for (i = 0; i < 4; i++) {
        vec4_t band = {0.031f, 0.031f, 0.031f, 0.36f - 0.09f * i};

        CG_FillRect(0, i * 22, full, 22, band);
    }
    for (i = 0; i < 8; i++) {
        vec4_t band = {0.031f, 0.031f, 0.031f, 0.10f * (i + 1)};

        CG_FillRect(0, 228 + i * 20, full, 20, band);
    }
    CG_FillRect(0, 388, full, 92, colorLoadFloor);

    // the logo, small, with nothing behind it
    if (cgs.media.qlLogoShader) {
        CG_DrawPic(22, 22, 180, 45, cgs.media.qlLogoShader);
    }

    // the gametype over the map's name, gold, the way a page's eyebrow reads
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        gametypeName = gametypeDisplayNames[cgs.gametype];
    } else {
        gametypeName = "Unknown Gametype";
    }
    Q_strncpyz(eyebrow, gametypeName, sizeof(eyebrow));
    Q_strupr(eyebrow);
    CG_DrawText(26, (float)(404 + CG_DrawTextHeight(eyebrow, 0.16f, 0, 1)), 1, 0.16f, colorLoadGold, eyebrow, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);

    // the map's name, big
    s = CG_ConfigString(CS_MESSAGE);
    CG_DrawText(24, (float)(416 + CG_DrawTextHeight(s, 0.5f, 0, 0)), 0, 0.5f, colorWhite, s, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);

    // along the bottom line: the authors at the left, what is loading at the right
    sAuthor = CG_ConfigString(CS_AUTHOR);
    if (sAuthor[0]) {
        sAuthor2 = CG_ConfigString(CS_AUTHOR2);
        if (sAuthor2[0]) {
            sAuthor = va("%s  %s", sAuthor, sAuthor2);
        }
        CG_DrawText(26, 466, 1, 0.17f, colorLoadMuted, sAuthor, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    }
    if (cg.infoScreenText[0]) {
        s = va("Loading %s", cg.infoScreenText);
    } else {
        s = "Awaiting Snapshot";
    }
    w = CG_DrawTextWidth(s, 0.17f, 0, 1);
    CG_DrawText(full - 24 - w, 466, 1, 0.17f, colorLoadMuted, s, 0, 0, ITEM_TEXTSTYLE_NORMAL);

    // the progress along the very bottom, a thread rather than four boxes
    CG_FillRect(0, 477, full, 3, colorLoadTrack);
    if (cg.loadingStage > 0) {
        float done = (float)cg.loadingStage / (float)NUM_LOADING_STAGES;

        CG_FillRect(0, 477, full * (done > 1.0f ? 1.0f : done), 3, colorLoadGold);
    }

    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
