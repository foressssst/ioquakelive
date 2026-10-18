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
static const vec4_t loadTrack = {1.0f, 1.0f, 1.0f, 0.16f};
static const vec4_t loadSoft = {0.86f, 0.87f, 0.89f, 1.0f};
static const vec4_t loadMuted = {0.62f, 0.63f, 0.66f, 1.0f};

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

static float CG_LoadingSpacedWidth(const char* text, int font, float scale, float spacing) {
    char c[2];
    float w = 0;

    c[1] = '\0';
    for (; *text; text++) {
        c[0] = *text;
        w += CG_DrawTextWidth(c, scale, 0, font) + spacing;
    }
    return w > 0 ? w - spacing : 0;
}

static void CG_LoadingSpaced(float x, float y, int font, float scale, const float* color, const char* text,
                             float spacing) {
    char c[2];

    c[1] = '\0';
    for (; *text; text++) {
        c[0] = *text;
        CG_DrawText(x, y, font, scale, (float*)color, c, 0, 0, ITEM_TEXTSTYLE_NORMAL);
        x += CG_DrawTextWidth(c, scale, 0, font) + spacing;
    }
}

static int CG_LoadingPictures(qhandle_t* pics, int max) {
    char model[MAX_QPATH];
    const char* info;
    const char* v;
    char* skin;
    uint64_t steamId;
    int i, n = 0;

    for (i = 0; i < MAX_CLIENTS && n < max; i++) {
        info = CG_ConfigString(CS_PLAYERS + i);
        if (!info[0] || !Info_ValueForKey(info, "n")[0] || atoi(Info_ValueForKey(info, "t")) == TEAM_SPECTATOR) {
            continue;
        }
        steamId = 0;
        for (v = Info_ValueForKey(info, "st"); *v >= '0' && *v <= '9'; v++) {
            steamId = steamId * 10 + (uint64_t)(*v - '0');
        }
        pics[n] = (cg_drawProfileImages.integer && steamId) ? trap_SteamAvatar(steamId) : 0;
        if (!pics[n]) {
            Q_strncpyz(model, Info_ValueForKey(info, "model"), sizeof(model));
            skin = strrchr(model, '/');
            if (skin) {
                *skin++ = '\0';
            } else {
                skin = "default";
            }
            pics[n] = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_%s.tga", model, skin));
        }
        if (pics[n]) {
            n++;
        }
    }
    return n;
}

static const char* CG_LoadingStageName(void) {
    static const char* const names[NUM_LOADING_STAGES + 1] = {"MAP", "SOUNDS", "GRAPHICS", "PLAYERS", "MATCH"};
    int stage = cg_loadingPreview.integer ? 2 : cg.loadingStage;

    return names[stage < 0 ? 0 : stage > NUM_LOADING_STAGES ? NUM_LOADING_STAGES : stage];
}

static int loadFont = -2;

static int CG_LoadingFont(void) {
    if (loadFont == -2) {
        loadFont = trap_R_Font_Register("fonts/Barlow-Regular.ttf");
    }
    return loadFont >= 0 ? loadFont : 1;
}

static void CG_LoadingText(float x, float y, int font, float scale, const float* color, const char* text,
                           float spacing, int align) {
    char c[2];
    const char* p;
    float w = 0.0f, cw = 0.0f;

    if (spacing <= 0.0f) {
        CG_MeasureTextInFont(text, font, scale, &w, NULL);
        CG_DrawTextInFont(x - w * align * 0.5f, y, font, scale, color, text, ITEM_TEXTSTYLE_NORMAL);
        return;
    }
    c[1] = '\0';
    for (p = text; *p; p++) {
        c[0] = *p;
        CG_MeasureTextInFont(c, font, scale, &cw, NULL);
        w += cw + spacing;
    }
    x -= (w - spacing) * align * 0.5f;
    for (p = text; *p; p++) {
        c[0] = *p;
        CG_DrawTextInFont(x, y, font, scale, color, c, ITEM_TEXTSTYLE_NORMAL);
        CG_MeasureTextInFont(c, font, scale, &cw, NULL);
        x += cw + spacing;
    }
}

static void CG_LoadingSpan(const float* box, float x, float y, float w, float h, qhandle_t shader) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    trap_R_DrawStretchPic(x, y, w, h, box[4] + (x - box[0]) / box[2] * (box[6] - box[4]),
                          box[5] + (y - box[1]) / box[3] * (box[7] - box[5]),
                          box[4] + (x + w - box[0]) / box[2] * (box[6] - box[4]),
                          box[5] + (y + h - box[1]) / box[3] * (box[7] - box[5]), shader);
}

static void CG_LoadingCard(float x, float y, float w, float h, float radius, int corners, qhandle_t shader, float s1,
                           float t1, float s2, float t2) {
    float box[8], r, e, in;
    int i, rows;

    CG_AdjustFrom640(&x, &y, &w, &h);
    r = radius * cgs.screenYScale;
    box[0] = x;
    box[1] = y;
    box[2] = w;
    box[3] = h;
    box[4] = s1;
    box[5] = t1;
    box[6] = s2;
    box[7] = t2;
    rows = (int)ceil(r);
    if (rows > (int)(h * 0.5f)) {
        rows = (int)(h * 0.5f);
    }
    for (i = 0; i < rows; i++) {
        e = i + 0.5f;
        in = e < r ? r - (float)sqrt(r * r - (r - e) * (r - e)) : 0.0f;
        CG_LoadingSpan(box, x + (corners & 1 ? in : 0.0f), y + i, w - (corners & 1 ? in * 2.0f : 0.0f), 1.0f, shader);
        CG_LoadingSpan(box, x + (corners & 2 ? in : 0.0f), y + h - i - 1.0f, w - (corners & 2 ? in * 2.0f : 0.0f), 1.0f,
                       shader);
    }
    CG_LoadingSpan(box, x, y + rows, w, h - rows * 2, shader);
}

static void CG_DrawLoadingCard(qhandle_t levelshot, float s1, float s2) {
    const char* info = CG_ConfigString(CS_SERVERINFO);
    char title[MAX_QPATH], host[MAX_QPATH], mode[MAX_QPATH], map[MAX_QPATH];
    vec4_t dim = {0.5f, 0.5f, 0.5f, 1.0f};
    qhandle_t blurred;
    float top = 146.0f, x = 260.0f, y = top, w = 120.0f, h = 30.0f;

    Q_strncpyz(title, CG_ConfigString(CS_MESSAGE)[0] ? CG_ConfigString(CS_MESSAGE) : cgs.mapname, sizeof(title));
    Q_strncpyz(host, Info_ValueForKey(info, "sv_hostname"), sizeof(host));
    Q_CleanStr(host);
    Q_strncpyz(mode, Info_ValueForKey(info, "g_factoryTitle"), sizeof(mode));
    if (!mode[0] && cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        Q_strncpyz(mode, gametypeDisplayNames[cgs.gametype], sizeof(mode));
    }
    Q_strupr(mode);
    Q_strncpyz(map, Info_ValueForKey(info, "mapname"), sizeof(map));
    blurred = trap_R_RegisterShaderNoMip(va("levelshots/%s@blur", map));

    trap_R_SetColor(dim);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                          blurred ? blurred : levelshot);
    trap_R_SetColor(NULL);

    CG_SetWidescreen(WIDESCREEN_CENTER);
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 56.0f / 512.0f, 16.0f / 128.0f, 449.0f / 512.0f, 114.0f / 128.0f,
                          trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo.png"));
    CG_LoadingText(320.0f, top + 61.5f, 0, 0.3f, colorWhite, title, 0.0f, 1);
    if (host[0]) {
        CG_LoadingText(320.0f, top + 78.0f, CG_LoadingFont(), 0.2f, loadMuted, host, 0.0f, 1);
    }
    CG_LoadingCard(212.0f, top + 94.0f, 216.0f, 72.0f, 6.0f, 3, levelshot, 0.0f, 0.2037f, 1.0f, 0.7963f);
    CG_LoadingText(320.0f, top + 187.0f, CG_LoadingFont(), 0.16f, loadAccent, mode, 1.5f, 1);
    CG_SetWidescreen(WIDESCREEN_STRETCH);
}

static void CG_DrawLoadingPoster(qhandle_t levelshot, float s1, float s2) {
    const char* info = CG_ConfigString(CS_SERVERINFO);
    char title[MAX_QPATH], host[MAX_QPATH], mode[MAX_QPATH], map[MAX_QPATH], line[MAX_QPATH], more[16];
    vec4_t dim = {0.45f, 0.45f, 0.45f, 1.0f};
    vec4_t band = {0.0f, 0.0f, 0.0f, 0.7f};
    qhandle_t blurred, pics[16];
    float cx = 176.0f, cw = 288.0f, ch = 162.0f, top = 126.0f, cy = top + 40.0f, row = cy + ch + 10.0f;
    float x = 276.0f, y = top, w = 88.0f, h = 22.0f, hostW = 0.0f, moreW = 0.0f;
    int count, shown, fit, i, n;
    unsigned char cut;

    Q_strncpyz(title, CG_ConfigString(CS_MESSAGE)[0] ? CG_ConfigString(CS_MESSAGE) : cgs.mapname, sizeof(title));
    Q_strncpyz(host, Info_ValueForKey(info, "sv_hostname"), sizeof(host));
    Q_CleanStr(host);
    Q_strncpyz(mode, Info_ValueForKey(info, "g_factoryTitle"), sizeof(mode));
    if (!mode[0] && cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        Q_strncpyz(mode, gametypeDisplayNames[cgs.gametype], sizeof(mode));
    }
    Q_strupr(mode);
    Q_strncpyz(map, Info_ValueForKey(info, "mapname"), sizeof(map));
    blurred = trap_R_RegisterShaderNoMip(va("levelshots/%s@blur", map));
    count = CG_LoadingPictures(pics, ARRAY_LEN(pics));

    CG_SetWidescreen(WIDESCREEN_CENTER);
    CG_MeasureTextInFont(host, CG_LoadingFont(), 0.2f, &hostW, NULL);
    if (hostW > cw * 0.6f) {
        n = (int)strlen(host);
        while (n > 0 && hostW > cw * 0.6f) {
            do {
                cut = (unsigned char)host[--n];
                host[n] = '\0';
            } while (n > 0 && (cut & 0xC0) == 0x80);
            Com_sprintf(line, sizeof(line), "%s...", host);
            CG_MeasureTextInFont(line, CG_LoadingFont(), 0.2f, &hostW, NULL);
        }
        Q_strncpyz(host, line, sizeof(host));
    }
    fit = (int)((cw - (host[0] ? hostW + 12.0f : 0.0f) + 4.0f) / 20.0f);
    shown = count <= fit ? count : fit - 1;
    if (shown < 0) {
        shown = 0;
    }
    more[0] = '\0';
    if (shown < count) {
        Com_sprintf(more, sizeof(more), "+%i", count - shown);
        CG_MeasureTextInFont(more, CG_LoadingFont(), 0.17f, &moreW, NULL);
    }

    trap_R_SetColor(dim);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                          blurred ? blurred : levelshot);
    trap_R_SetColor(NULL);

    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 56.0f / 512.0f, 16.0f / 128.0f, 449.0f / 512.0f, 114.0f / 128.0f,
                          trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo.png"));
    CG_LoadingCard(cx, cy, cw, ch, 8.0f, 3, levelshot, 0.0f, 0.0f, 1.0f, 1.0f);
    trap_R_SetColor(band);
    CG_LoadingCard(cx, cy + ch - 48.0f, cw, 48.0f, 8.0f, 2, cgs.media.whiteShader, 0.0f, 0.0f, 1.0f, 1.0f);
    trap_R_SetColor(NULL);
    CG_LoadingText(cx + 14.0f, cy + ch - 30.5f, CG_LoadingFont(), 0.15f, loadAccent, mode, 1.5f, 0);
    CG_LoadingText(cx + 13.0f, cy + ch - 11.0f, 0, 0.3f, colorWhite, title, 0.0f, 0);

    if (host[0]) {
        CG_LoadingText(cx, row + 12.2f, CG_LoadingFont(), 0.2f, loadMuted, host, 0.0f, 0);
    }
    x = cx + cw - (shown * 20.0f - (shown ? 4.0f : 0.0f) + (more[0] ? (shown ? 5.0f : 0.0f) + moreW : 0.0f));
    for (i = 0; i < shown; i++, x += 20.0f) {
        CG_LoadingCard(x, row, 16.0f, 16.0f, 3.0f, 3, pics[i], 0.0f, 0.0f, 1.0f, 1.0f);
    }
    if (more[0]) {
        CG_LoadingText(x + (shown ? 1.0f : 0.0f), row + 11.7f, CG_LoadingFont(), 0.17f, loadMuted, more, 0.0f, 0);
    }
    CG_SetWidescreen(WIDESCREEN_STRETCH);
}

static void CG_DrawLoadingRemaster(qhandle_t levelshot, float s1, float s2) {
    const char* info = CG_ConfigString(CS_SERVERINFO);
    const char* mode;
    char title[MAX_QPATH], line[MAX_STRING_CHARS], pct[16];
    qhandle_t pics[16];
    float progress = CG_LoadingProgress();
    float x, w, total;
    vec4_t dim = {0.02f, 0.025f, 0.03f, 0.0f};
    int maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
    int count, i;

    if (cg_remaster.integer == 3) {
        CG_DrawLoadingCard(levelshot, s1, s2);
        return;
    }
    if (cg_remaster.integer == 4) {
        CG_DrawLoadingPoster(levelshot, s1, s2);
        return;
    }

    Q_strncpyz(title, CG_ConfigString(CS_MESSAGE)[0] ? CG_ConfigString(CS_MESSAGE) : cgs.mapname, sizeof(title));
    Q_strupr(title);
    mode = Info_ValueForKey(info, "g_factoryTitle");
    if (!mode[0]) {
        mode = (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) ? gametypeDisplayNames[cgs.gametype] : "";
    }
    Q_strncpyz(line, mode, sizeof(line));
    Q_strupr(line);
    count = CG_LoadingPictures(pics, ARRAY_LEN(pics));
    Com_sprintf(pct, sizeof(pct), "%i%%", (int)(progress * 100));

    dim[3] = cg_remaster.integer == 2 ? 0.58f : 0.42f;
    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f, levelshot);
    trap_R_SetColor(dim);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, 0, 0, 1, 1,
                          cgs.media.whiteShader);
    trap_R_SetColor(NULL);

    if (cg_remaster.integer == 2) {
        CG_SetWidescreen(WIDESCREEN_CENTER);
        w = CG_LoadingSpacedWidth(line, 1, 0.19f, 2.5f);
        CG_LoadingSpaced(320 - w * 0.5f, 196, 1, 0.19f, loadAccent, line, 2.5f);
        w = CG_DrawTextWidth(title, 0.82f, 0, 0);
        CG_DrawText(320 - w * 0.5f, 238, 0, 0.82f, colorWhite, title, 0, 0, ITEM_TEXTSTYLE_NORMAL);
        Q_strncpyz(line, Info_ValueForKey(info, "sv_hostname"), sizeof(line));
        w = CG_DrawTextWidth(line, 0.19f, 0, 1);
        CG_DrawText(320 - w * 0.5f, 257, 1, 0.19f, (float*)loadMuted, line, 0, 0, ITEM_TEXTSTYLE_NORMAL);
        CG_FillRect(220, 274, 200, 1, (float*)loadTrack);
        CG_FillRect(220, 274, 200 * progress, 1, (float*)loadAccent);
        w = CG_DrawTextWidth(pct, 0.17f, 0, 0);
        CG_DrawText(320 - w * 0.5f, 290, 0, 0.17f, (float*)loadSoft, pct, 0, 0, ITEM_TEXTSTYLE_NORMAL);
        total = count * 18 - (count ? 4 : 0);
        for (i = 0, x = 320 - total * 0.5f; i < count; i++, x += 18) {
            CG_DrawPic(x, 430, 14, 14, pics[i]);
        }
        CG_SetWidescreen(WIDESCREEN_STRETCH);
        return;
    }

    CG_SetWidescreen(WIDESCREEN_LEFT);
    CG_LoadingSpaced(26, 378, 1, 0.19f, loadAccent, line, 2.5f);
    CG_DrawText(24, 420, 0, 0.9f, colorWhite, title, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    Com_sprintf(line, sizeof(line), "%s   %i / %i", Info_ValueForKey(info, "sv_hostname"), count, maxClients);
    CG_DrawText(26, 441, 1, 0.19f, (float*)loadMuted, line, 0, 0, ITEM_TEXTSTYLE_NORMAL);

    CG_SetWidescreen(WIDESCREEN_RIGHT);
    w = CG_DrawTextWidth(pct, 0.3f, 0, 0);
    CG_DrawText(614 - w, 441, 0, 0.3f, colorWhite, pct, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    w = CG_LoadingSpacedWidth(CG_LoadingStageName(), 1, 0.15f, 2.0f);
    CG_LoadingSpaced(614 - w, 414, 1, 0.15f, loadMuted, CG_LoadingStageName(), 2.0f);
    for (i = 0, x = 614 - 14; i < count; i++, x -= 18) {
        CG_DrawPic(x, 382, 14, 14, pics[i]);
    }

    CG_SetWidescreen(WIDESCREEN_STRETCH);
    trap_R_SetColor(loadTrack);
    trap_R_DrawStretchPic(0, (float)cgs.glconfig.vidHeight - 3, (float)cgs.glconfig.vidWidth, 3, 0, 0, 1, 1,
                          cgs.media.whiteShader);
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
