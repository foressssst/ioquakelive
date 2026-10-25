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
#include "../../ui/menudef.h"

#define MAX_LOADING_PLAYER_ICONS 16
#define MAX_LOADING_ITEM_ICONS 26

#define NUM_LOADING_STAGES 4

static int loadingPlayerIconCount;
static int loadingItemIconCount;
static qhandle_t loadingPlayerIcons[MAX_LOADING_PLAYER_ICONS];
static qhandle_t loadingItemIcons[MAX_LOADING_ITEM_ICONS];

void CG_LoadingString(const char* s) {
    Q_strncpyz(cg.infoScreenText, s, sizeof(cg.infoScreenText));

    trap_UpdateScreen();
}

void CG_LoadingItem(int itemNum) {
    gitem_t* item;

    item = &bg_itemlist[itemNum];

    if (item->icon && loadingItemIconCount < MAX_LOADING_ITEM_ICONS) {
        loadingItemIcons[loadingItemIconCount++] = trap_R_RegisterShaderNoMip(item->icon);
    }

    CG_LoadingString(item->pickup_name);
}

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

void CG_DrawInformation(void) {
    const char *s;
    const char *info;
    const char *sAuthor, *sAuthor2;
    const char *gametypeName;
    qhandle_t levelshot;
    int i;
    int w;
    float s1, s2;

    static vec4_t colorBarFilled   = { 0.375f, 0.125f, 0.125f, 1.0f  };
    static vec4_t colorBarEmpty    = { 0.0f,   0.0f,   0.0f,   0.75f };
    static vec4_t colorBarBg       = { 0.375f, 0.125f, 0.125f, 0.75f };

    s1 = ((1920.0f - (1080.0f / (float)cgs.glconfig.vidHeight) * (float)cgs.glconfig.vidWidth)
          / 1920.0f) * 0.5f;
    s2 = 1.0f - s1;

    info = CG_ConfigString(CS_SERVERINFO);

    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));

    s = Info_ValueForKey(info, "mapname");
    levelshot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", s));
    if (!levelshot) {
        levelshot = trap_R_RegisterShaderNoMip("menu/art/unknownmap");
    }
    trap_R_SetColor(NULL);

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

    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight,
                          s1, 0, s2, 1.0f, levelshot);

    CG_SetWidescreen(WIDESCREEN_LEFT);

    CG_FillRect(-400, 0, 1440, 64, colorBarBg);

    if (cgs.media.logoBackgroundShader) {
        CG_DrawPic(0, 0, 256, 64, cgs.media.logoBackgroundShader);
    }

    if (cgs.media.qlLogoShader) {
        CG_DrawPic(0, 0, 256, 64, cgs.media.qlLogoShader);
    }

    CG_FillRect(-400, 400, 1440, 80, colorBarEmpty);

    s = CG_ConfigString(CS_MESSAGE);
    CG_DrawText(10, (float)(400 + CG_DrawTextHeight(s, 0.8f, 0, 0)), 0, 0.8f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_NORMAL);

    sAuthor = CG_ConfigString(CS_AUTHOR);
    if (sAuthor[0]) {
        sAuthor2 = CG_ConfigString(CS_AUTHOR2);
        CG_DrawText(10, (float)(447 + CG_DrawTextHeight(sAuthor, 0.25f, 0, 1)), 1, 0.25f, colorWhite, sAuthor, 0, 0,
                    ITEM_TEXTSTYLE_NORMAL);
        CG_DrawText(10, (float)(462 + CG_DrawTextHeight(sAuthor2, 0.25f, 0, 1)), 1, 0.25f, colorWhite, sAuthor2, 0, 0,
                    ITEM_TEXTSTYLE_NORMAL);
    }

    CG_SetWidescreen(WIDESCREEN_RIGHT);

    if (cgs.media.gtBackgroundShader) {
        CG_DrawPic(384, 0, 256, 64, cgs.media.gtBackgroundShader);
    }

    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        gametypeName = gametypeDisplayNames[cgs.gametype];
    } else {
        gametypeName = "Unknown Gametype";
    }
    w = CG_DrawTextWidth(gametypeName, 0.8f, 0, 0);
    CG_DrawText((float)(620 - w), (float)(10 + CG_DrawTextHeight(gametypeName, 0.8f, 0, 0)), 0, 0.8f, colorWhite,
                gametypeName, 0, 0, ITEM_TEXTSTYLE_NORMAL);

    CG_SetWidescreen(WIDESCREEN_CENTER);

    if (cg.infoScreenText[0]) {
        s = va("Loading %s", cg.infoScreenText);
    } else {
        s = "Awaiting Snapshot";
    }
    w = CG_DrawTextWidth(s, 0.3f, 0, 1);
    CG_DrawText((float)(320 - w / 2), 370, 1, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

    for (i = 0; i < NUM_LOADING_STAGES; i++) {
        if (cg.loadingStage > i) {
            CG_FillRect(292 + i * 16, 380, 8, 8, colorBarFilled);
        } else {
            CG_FillRect(292 + i * 16, 380, 8, 8, colorBarEmpty);
        }
    }

    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
