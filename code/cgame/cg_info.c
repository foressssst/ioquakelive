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

#define OSU(u) ((u) * 0.625f)

static const vec4_t osuShade = {0.0f, 0.0f, 0.0f, 0.3f};
static const vec4_t osuBox = {0.0f, 0.0f, 0.0f, 0.7f};

static int osuFonts[2] = {-2, -2};
static float osuK = 1.0f, osuAlpha = 1.0f;

static int CG_OsuFont(int bold) {
    if (osuFonts[0] == -2) {
        osuFonts[0] = trap_R_Font_Register("fonts/Barlow-Regular.ttf");
        osuFonts[1] = trap_R_Font_Register("fonts/Barlow-Bold.ttf");
    }
    return osuFonts[bold] >= 0 ? osuFonts[bold] : 1;
}

static float CG_OsuEase(float t, float delay, float duration) {
    t = (t - delay) / duration;
    t = 1.0f - (t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t);
    return 1.0f - t * t * t * t * t;
}

static float CG_OsuInOutExpo(float t) {
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    if (t == 0.0f || t == 1.0f) {
        return t;
    }
    return t < 0.5f ? 0.5f * (float)pow(2.0, 20.0 * t - 10.0) : 1.0f - 0.5f * (float)pow(2.0, -20.0 * t + 10.0);
}

static void CG_OsuPoint(float* x, float* y) {
    *x = 320.0f + (*x - 320.0f) * osuK;
    *y = 240.0f + (*y - 240.0f) * osuK;
}

static void CG_OsuText(float x, float y, float size, const char* text) {
    vec4_t c = {1.0f, 1.0f, 1.0f, 1.0f};
    float w = 0;

    CG_MeasureTextInFont(text, CG_OsuFont(0), size / 96.0f, &w, NULL);
    x -= w * 0.5f;
    CG_OsuPoint(&x, &y);
    c[3] = osuAlpha;
    CG_DrawTextInFont(x, y, CG_OsuFont(0), size * osuK / 96.0f, c, text, ITEM_TEXTSTYLE_NORMAL);
}

static void CG_OsuQuad(const float* box, float x, float y, float w, float h, qhandle_t shader) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    trap_R_DrawStretchPic(x, y, w, h, box[4] + (x - box[0]) / box[2] * (box[6] - box[4]),
                          box[5] + (y - box[1]) / box[3] * (box[7] - box[5]),
                          box[4] + (x + w - box[0]) / box[2] * (box[6] - box[4]),
                          box[5] + (y + h - box[1]) / box[3] * (box[7] - box[5]), shader);
}

static void CG_OsuRow(const float* box, float y, float h, float e, float radius, qhandle_t shader) {
    float in = 0.0f;

    if (e < radius) {
        in = radius - (float)sqrt(radius * radius - (radius - e) * (radius - e));
    }
    CG_OsuQuad(box, box[0] + in, y, box[2] - in * 2.0f, h, shader);
}

static void CG_OsuShape(float x, float y, float w, float h, float radius, const float* color, qhandle_t shader,
                        float s1, float t1, float s2, float t2) {
    float box[8];
    vec4_t c;
    int i, rows;

    CG_OsuPoint(&x, &y);
    w *= osuK;
    h *= osuK;
    radius *= osuK;
    CG_AdjustFrom640(&x, &y, &w, &h);
    radius *= cgs.screenYScale;
    if (radius > h * 0.5f) {
        radius = h * 0.5f;
    }
    if (radius > w * 0.5f) {
        radius = w * 0.5f;
    }
    box[0] = x;
    box[1] = y;
    box[2] = w;
    box[3] = h;
    box[4] = s1;
    box[5] = t1;
    box[6] = s2;
    box[7] = t2;
    Vector4Copy(color, c);
    c[3] *= osuAlpha;
    trap_R_SetColor(c);
    rows = (int)ceil(radius);
    if (rows > (int)(h * 0.5f)) {
        rows = (int)(h * 0.5f);
    }
    for (i = 0; i < rows; i++) {
        CG_OsuRow(box, y + i, 1.0f, i + 0.5f, radius, shader);
        CG_OsuRow(box, y + h - i - 1.0f, 1.0f, i + 0.5f, radius, shader);
    }
    if (h - rows * 2 > 0.0f) {
        CG_OsuRow(box, y + rows, h - rows * 2, h, radius, shader);
    }
    trap_R_SetColor(NULL);
}

static void CG_OsuSpinner(float cx, float cy, float size, int time) {
    float x = cx, y = cy, w = size * osuK, h = size * osuK, r, th, a, base = time * (360.0f / 3150.0f);
    vec4_t c = {1.0f, 1.0f, 1.0f, 1.0f};
    int i;

    CG_OsuPoint(&x, &y);
    CG_AdjustFrom640(&x, &y, &w, &h);
    r = h * 0.5f;
    th = r * 0.24f;
    c[3] = osuAlpha;
    trap_R_SetColor(c);
    for (i = 0; i < 64; i++) {
        a = DEG2RAD(base + i * (290.0f / 63.0f));
        trap_R_DrawStretchPic(x + (float)cos(a) * (r - th * 0.5f) - th * 0.5f,
                              y + (float)sin(a) * (r - th * 0.5f) - th * 0.5f, th, th, 0, 0, 1, 1,
                              cgs.media.whiteShader);
    }
    trap_R_SetColor(NULL);
}

static void CG_DrawLoadingOsu(qhandle_t levelshot, float s1, float s2) {
    static int start, last;
    const char* info = CG_ConfigString(CS_SERVERINFO);
    char title[MAX_QPATH], host[MAX_QPATH], mode[MAX_QPATH], map[MAX_QPATH];
    vec4_t dim = {0.55f, 0.55f, 0.55f, 1.0f};
    qhandle_t blurred, logo;
    float top, fx, fy, p, ls;
    int now = trap_Milliseconds(), t;

    if (!start || (cg_loadingPreview.integer && now - last > 1000)) {
        start = now;
    }
    last = now;
    t = now - start;

    Q_strncpyz(title, CG_ConfigString(CS_MESSAGE)[0] ? CG_ConfigString(CS_MESSAGE) : cgs.mapname, sizeof(title));
    Q_strncpyz(host, Info_ValueForKey(info, "sv_hostname"), sizeof(host));
    Q_CleanStr(host);
    Q_strncpyz(mode, Info_ValueForKey(info, "g_factoryTitle"), sizeof(mode));
    if (!mode[0] && cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        Q_strncpyz(mode, gametypeDisplayNames[cgs.gametype], sizeof(mode));
    }
    Q_strncpyz(map, Info_ValueForKey(info, "mapname"), sizeof(map));
    blurred = trap_R_RegisterShaderNoMip(va("levelshots/%s@blur", map));

    trap_R_SetColor(dim);
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                          blurred ? blurred : levelshot);
    trap_R_SetColor(NULL);

    CG_SetWidescreen(WIDESCREEN_CENTER);
    top = 240.0f - OSU(395) * 0.5f;
    osuK = 0.7f + 0.3f * CG_OsuEase((float)t, 0, 650);
    osuAlpha = CG_OsuEase((float)t, 500, 500);

    CG_OsuText(320, top + OSU(154), 36, title);
    if (host[0]) {
        CG_OsuText(320, top + OSU(188), 26, host);
    }
    CG_OsuShape(320 - OSU(210), top + OSU(204), OSU(420), OSU(150), OSU(10), colorWhite, levelshot, 0.0f, 0.1825f,
                1.0f, 0.8175f);
    CG_OsuShape(320 - OSU(210), top + OSU(204), OSU(420), OSU(150), OSU(10), osuShade, cgs.media.whiteShader, 0.0f,
                0.0f, 1.0f, 1.0f);
    CG_OsuShape(320 - OSU(18.75f), top + OSU(260.25f), OSU(37.5f), OSU(37.5f), OSU(9.4f), osuBox,
                cgs.media.whiteShader, 0.0f, 0.0f, 1.0f, 1.0f);
    CG_OsuSpinner(320, top + OSU(279), OSU(22.5f), t);
    CG_OsuText(320, top + OSU(389), 26, mode);

    fx = 320.0f;
    fy = top + OSU(52.5f);
    CG_OsuPoint(&fx, &fy);
    p = CG_OsuInOutExpo(t / 500.0f);
    ls = (1.5f - 0.5f * CG_OsuEase((float)t, 0, 300)) * osuK;
    fx = 320.0f + (fx - 320.0f) * p;
    fy = 240.0f + (fy - 240.0f) * p;
    osuK = 1.0f;
    osuAlpha = t >= 350 ? 1.0f : t / 350.0f;
    logo = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo.png");
    CG_OsuShape(fx - OSU(210) * ls, fy - OSU(52.5f) * ls, OSU(420) * ls, OSU(105) * ls, 0.0f, colorWhite, logo,
                56.0f / 512.0f, 16.0f / 128.0f, 449.0f / 512.0f, 114.0f / 128.0f);
    osuAlpha = 1.0f;
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
        CG_DrawLoadingOsu(levelshot, s1, s2);
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
