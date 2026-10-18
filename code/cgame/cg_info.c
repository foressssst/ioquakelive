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

static const vec4_t loadAccent = {0.97f, 0.82f, 0.0f, 1.0f};
static const vec4_t loadDim = {0.03f, 0.035f, 0.045f, 0.62f};
static const vec4_t loadPanel = {0.05f, 0.055f, 0.063f, 0.88f};
static const vec4_t loadChip = {0.08f, 0.086f, 0.1f, 0.86f};
static const vec4_t loadLine = {0.16f, 0.17f, 0.2f, 1.0f};
static const vec4_t loadEdge = {1.0f, 1.0f, 1.0f, 0.18f};
static const vec4_t loadStrip = {0.03f, 0.035f, 0.045f, 0.72f};
static const vec4_t loadSoft = {0.79f, 0.8f, 0.82f, 1.0f};
static const vec4_t loadMuted = {0.56f, 0.57f, 0.6f, 1.0f};

static float CG_LoadingProgress(void) {
    static int lastStage = -1;
    static int calls;
    static char lastText[MAX_STRING_CHARS];
    float f;

    if (cg_loadingPreview.integer) {
        return cg_loadingPreview.integer / 100.0f;
    }
    if (cg.loadingStage != lastStage) {
        lastStage = cg.loadingStage;
        calls = 0;
    }
    if (strcmp(lastText, cg.infoScreenText)) {
        Q_strncpyz(lastText, cg.infoScreenText, sizeof(lastText));
        calls++;
    }
    f = 1.0f - 1.0f / (1.0f + calls * 0.08f);
    f = (cg.loadingStage + f) / NUM_LOADING_STAGES;
    return f > 1.0f ? 1.0f : f;
}

static void CG_LoadingPicST(float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader) {
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, s0, t0, s1, t1, shader);
}

static void CG_LoadingFrame(float x, float y, float w, float h, const float* color) {
    CG_FillRect(x, y, w, 0.5f, (float*)color);
    CG_FillRect(x, y + h - 0.5f, w, 0.5f, (float*)color);
    CG_FillRect(x, y, 0.5f, h, (float*)color);
    CG_FillRect(x + w - 0.5f, y, 0.5f, h, (float*)color);
}

static void CG_LoadingCentered(float y, int font, float scale, const float* color, const char* text) {
    CG_DrawText(320 - CG_DrawTextWidth(text, scale, 0, font) * 0.5f, y, font, scale, (float*)color, text, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);
}

static float CG_LoadingChip(float x, float y, const char* text) {
    float w = CG_DrawTextWidth(text, 0.16f, 0, 0) + 12;

    CG_FillRect(x, y, w, 15, (float*)loadChip);
    CG_LoadingFrame(x, y, w, 15, loadLine);
    CG_DrawText(x + 6, y + 11, 0, 0.16f, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    return w;
}

static float CG_LoadingInfoLine(float y, const char* label, const char* value) {
    if (!value[0]) {
        return y;
    }
    CG_DrawText(316 - CG_DrawTextWidth(label, 0.18f, 0, 1), y, 1, 0.18f, (float*)loadMuted, label, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);
    CG_DrawText(324, y, 1, 0.18f, colorWhite, value, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    return y + 14;
}

static int CG_LoadingPlayerCount(void) {
    const char* info;
    int i, n = 0;

    for (i = 0; i < MAX_CLIENTS; i++) {
        info = CG_ConfigString(CS_PLAYERS + i);
        if (info[0] && Info_ValueForKey(info, "n")[0] && atoi(Info_ValueForKey(info, "t")) != TEAM_SPECTATOR) {
            n++;
        }
    }
    return n;
}

static void CG_LoadingPlayers(void) {
    char model[MAX_QPATH], name[MAX_QPATH];
    const char* info;
    const char* v;
    char* skin;
    uint64_t steamId;
    qhandle_t pic;
    float y = 48;
    int i, shown = 0;

    CG_SetWidescreen(WIDESCREEN_RIGHT);
    CG_FillRect(475, -10, 200, 500, (float*)loadPanel);
    CG_FillRect(475, -10, 0.5f, 500, (float*)loadLine);
    CG_DrawText(488, 30, 0, 0.16f, (float*)loadAccent, "IN THIS MATCH", 0, 0, ITEM_TEXTSTYLE_NORMAL);
    for (i = 0; i < MAX_CLIENTS && shown < 20; i++) {
        info = CG_ConfigString(CS_PLAYERS + i);
        if (!info[0] || !Info_ValueForKey(info, "n")[0]) {
            continue;
        }
        Q_strncpyz(name, Info_ValueForKey(info, "n"), sizeof(name));
        steamId = 0;
        for (v = Info_ValueForKey(info, "st"); *v >= '0' && *v <= '9'; v++) {
            steamId = steamId * 10 + (uint64_t)(*v - '0');
        }
        pic = (cg_drawProfileImages.integer && steamId) ? trap_SteamAvatar(steamId) : 0;
        if (!pic) {
            Q_strncpyz(model, Info_ValueForKey(info, "model"), sizeof(model));
            skin = strrchr(model, '/');
            if (skin) {
                *skin++ = '\0';
            } else {
                skin = "default";
            }
            pic = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_%s.tga", model, skin));
        }
        if (pic) {
            CG_DrawPic(488, y, 16, 16, pic);
        }
        CG_DrawText(510, y + 12, 1, 0.19f, colorWhite, name, 0, 0, ITEM_TEXTSTYLE_NORMAL);
        if (atoi(Info_ValueForKey(info, "t")) == TEAM_SPECTATOR) {
            CG_DrawText(610, y + 12, 1, 0.15f, (float*)loadMuted, "spec", 0, 0, ITEM_TEXTSTYLE_NORMAL);
        } else if (Info_ValueForKey(info, "skill")[0]) {
            CG_DrawText(610, y + 12, 1, 0.15f, (float*)loadMuted, "bot", 0, 0, ITEM_TEXTSTYLE_NORMAL);
        }
        y += 21;
        shown++;
    }
}

static void CG_DrawLoadingRemaster(qhandle_t levelshot, float s1, float s2) {
    const char* info = CG_ConfigString(CS_SERVERINFO);
    const char* gametypeName;
    const char* item;
    char title[MAX_QPATH];
    char chip[64];
    float progress = CG_LoadingProgress();
    float x, y, w, h;
    int limit;

    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f, levelshot);
    trap_R_SetColor(loadDim);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, 0, 0, 1, 1,
                          cgs.media.whiteShader);
    trap_R_SetColor(NULL);

    CG_SetWidescreen(WIDESCREEN_CENTER);
    if (cgs.media.qlLogoShader) {
        CG_DrawPic(256, 46, 128, 32, cgs.media.qlLogoShader);
    }

    Q_strncpyz(title, CG_ConfigString(CS_MESSAGE)[0] ? CG_ConfigString(CS_MESSAGE) : cgs.mapname, sizeof(title));
    Q_strupr(title);
    CG_LoadingCentered(120, 0, 0.46f, colorWhite, title);

    gametypeName = Info_ValueForKey(info, "g_factoryTitle");
    if (!gametypeName[0]) {
        gametypeName = (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) ? gametypeDisplayNames[cgs.gametype]
                                                                               : "Unknown Gametype";
    }
    CG_LoadingCentered(139, 1, 0.22f, loadSoft, gametypeName);

    x = 190;
    y = 150;
    w = 260;
    h = 98;
    CG_LoadingPicST(x, y, w, h, 0.0f, 0.25f, 1.0f, 0.75f, levelshot);
    CG_LoadingFrame(x, y, w, h, loadEdge);
    CG_FillRect(x, y + h - 22, w, 22, (float*)loadStrip);
    item = cg_loadingPreview.integer ? "models/players/sarge" : cg.infoScreenText;
    CG_DrawText(x + 7, y + h - 7.5f, 1, 0.17f, (float*)loadSoft, va("Loading %s", item), 0, 0, ITEM_TEXTSTYLE_NORMAL);
    Com_sprintf(chip, sizeof(chip), "%i%%", (int)(progress * 100));
    CG_DrawText(x + w - 7 - CG_DrawTextWidth(chip, 0.2f, 0, 0), y + h - 7, 0, 0.2f, (float*)loadAccent, chip, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);
    CG_FillRect(x, y + h - 1.5f, w * progress, 1.5f, (float*)loadAccent);

    {
        char chips[3][32];
        int n = 0, i;
        float total = 0;

        limit = atoi(Info_ValueForKey(info, "fraglimit"));
        if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
            limit = atoi(Info_ValueForKey(info, "capturelimit"));
            Com_sprintf(chips[n++], sizeof(chips[0]), "CAPTURE %i", limit);
        } else if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE || cgs.gametype == GT_AD) {
            Com_sprintf(chips[n++], sizeof(chips[0]), "ROUNDS %i", atoi(Info_ValueForKey(info, "roundlimit")));
        } else if (limit > 0) {
            Com_sprintf(chips[n++], sizeof(chips[0]), "FRAG %i", limit);
        }
        if (atoi(Info_ValueForKey(info, "timelimit")) > 0) {
            Com_sprintf(chips[n++], sizeof(chips[0]), "TIME %i", atoi(Info_ValueForKey(info, "timelimit")));
        }
        limit = atoi(Info_ValueForKey(info, "sv_maxclients"));
        Com_sprintf(chips[n++], sizeof(chips[0]), "%i / %i PLAYERS", CG_LoadingPlayerCount(), limit);
        for (i = 0; i < n; i++) {
            total += CG_DrawTextWidth(chips[i], 0.16f, 0, 0) + 12 + (i ? 4 : 0);
        }
        x = 320 - total * 0.5f;
        for (i = 0; i < n; i++) {
            x += CG_LoadingChip(x, 258, chips[i]) + 4;
        }
    }

    y = CG_LoadingInfoLine(296, "Server", Info_ValueForKey(info, "sv_hostname"));
    y = CG_LoadingInfoLine(y, "Author", CG_ConfigString(CS_AUTHOR));
    Q_strncpyz(title, Info_ValueForKey(CG_ConfigString(CS_SERVERINFO), "mapname"), sizeof(title));
    CG_LoadingInfoLine(y, "Map", title);

    CG_LoadingPlayers();

    CG_SetWidescreen(WIDESCREEN_STRETCH);
    trap_R_SetColor(loadAccent);
    trap_R_DrawStretchPic(0, (float)cgs.glconfig.vidHeight - 3, cgs.glconfig.vidWidth * progress, 3, 0, 0, 1, 1,
                          cgs.media.whiteShader);
    trap_R_SetColor(NULL);
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

    if (cg_remaster.integer && (cg.infoScreenText[0] || cg_loadingPreview.integer)) {
        CG_DrawLoadingRemaster(levelshot, s1, s2);
        return;
    }

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
