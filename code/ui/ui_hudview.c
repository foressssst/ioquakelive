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
#include "ui_hudview.h"
#include "ui_settings.h"

void Item_Paint(itemDef_t* item);

qboolean Item_EnableShowViaCvar(itemDef_t* item, int flag);

hvScene_t hvScene = {GT_CA, TEAM_RED, qfalse, qtrue};

static menuDef_t hvMenu[HV_MAX_MENUS];
static int hvCount;
static qboolean hvLoaded;
static uiMemMark_t hvMark;
static uiMemMark_t hvEnd;
static displayContextDef_t hvDC;
static cachedAssets_t hvAssets;
static float hvX, hvY, hvW = 640.0f, hvH = 480.0f;
static int hvAnchor;
static displayContextDef_t* hvSaved;

#define HV_MAX_BOUNDS 1024

static struct {
    const itemDef_t* item;
    float b[4];
    qhandle_t shader;
    char text[16];
} hvBounds[HV_MAX_BOUNDS + 1];
static int hvNumBounds;
static int hvCur = -1;
static qboolean hvLoose;

static void HV_NoteShader(qhandle_t shader) {
    if (hvCur >= 0 && !hvBounds[hvCur].shader && shader && shader != uiInfo.uiDC.whiteShader) {
        hvBounds[hvCur].shader = shader;
    }
}

static void HV_NoteText(const char* text) {
    if (hvCur >= 0 && !hvBounds[hvCur].text[0] && text) {
        Q_strncpyz(hvBounds[hvCur].text, text, sizeof(hvBounds[hvCur].text));
        Q_CleanStr(hvBounds[hvCur].text);
    }
}

static void HV_Extend(float x, float y, float w, float h) {
    float* b;

    if (hvCur < 0 || w <= 0.0f || h <= 0.0f) {
        return;
    }
    b = hvBounds[hvCur].b;
    if (b[2] <= b[0]) {
        b[0] = x;
        b[1] = y;
        b[2] = x + w;
        b[3] = y + h;
        return;
    }
    if (x < b[0]) {
        b[0] = x;
    }
    if (y < b[1]) {
        b[1] = y;
    }
    if (x + w > b[2]) {
        b[2] = x + w;
    }
    if (y + h > b[3]) {
        b[3] = y + h;
    }
}

static void HV_TrackBegin(int ownerDraw, float y, float w, float h) {
    int m, i;

    hvCur = -1;
    if (hvLoose) {
        hvCur = HV_MAX_BOUNDS;
        return;
    }
    if (hvNumBounds >= HV_MAX_BOUNDS) {
        return;
    }
    for (m = 0; m < hvCount; m++) {
        for (i = 0; i < hvMenu[m].itemCount; i++) {
            const itemDef_t* it = hvMenu[m].items[i];

            if (it->window.ownerDraw == ownerDraw && it->window.rect.y == y && it->window.rect.w == w &&
                it->window.rect.h == h) {
                int k;

                for (k = 0; k < hvNumBounds; k++) {
                    if (hvBounds[k].item == it) {
                        hvCur = k;
                        return;
                    }
                }
                hvCur = hvNumBounds++;
                hvBounds[hvCur].item = it;
                hvBounds[hvCur].b[0] = hvBounds[hvCur].b[1] = hvBounds[hvCur].b[2] = hvBounds[hvCur].b[3] = 0.0f;
                hvBounds[hvCur].shader = 0;
                hvBounds[hvCur].text[0] = 0;
                return;
            }
        }
    }
}

static qboolean HV_PaintedRect(const itemDef_t* item, float* out) {
    int k;

    for (k = 0; k < hvNumBounds; k++) {
        if (hvBounds[k].item == item && hvBounds[k].b[2] > hvBounds[k].b[0]) {
            out[0] = hvBounds[k].b[0];
            out[1] = hvBounds[k].b[1];
            out[2] = hvBounds[k].b[2] - hvBounds[k].b[0];
            out[3] = hvBounds[k].b[3] - hvBounds[k].b[1];
            return qtrue;
        }
    }
    return qfalse;
}

static const vec3_t hvTeamFree = {1.0f, 0.8f, 0.2f};
static const vec3_t hvTeamRed = {1.0f, 0.2f, 0.1f};
static const vec3_t hvTeamBlue = {0.2f, 0.4f, 1.0f};

static int HV_ArmorType(void) {
    return 1;
}

static int HV_Health(void) {
    return 125;
}

static int HV_Armor(void) {
    return 100;
}

static int HV_Ammo(void) {
    return 25;
}

static qboolean HV_TeamGame(void) {
    return hvScene.gametype >= GT_TEAM ? qtrue : qfalse;
}

static qboolean HV_RoundGame(void) {
    return hvScene.gametype == GT_CA || hvScene.gametype == GT_FREEZE || hvScene.gametype == GT_AD ||
           hvScene.gametype == GT_RR;
}

static qboolean HV_FlagGame(void) {
    return hvScene.gametype == GT_CTF || hvScene.gametype == GT_1FCTF || hvScene.gametype == GT_AD;
}

static int HV_Score(int which) {
    switch (hvScene.gametype) {
        case GT_TEAM:
            return which ? 48 : 63;
        case GT_CTF:
        case GT_1FCTF:
            return which ? 2 : 3;
        case GT_CA:
        case GT_FREEZE:
        case GT_AD:
        case GT_RR:
            return which ? 5 : 7;
        default:
            return which ? 17 : 23;
    }
}

static int HV_PlayerScore(void) {
    return hvScene.gametype == GT_TEAM || hvScene.gametype >= GT_CA ? 17 : 23;
}

static const float* HV_TeamRGB(void) {
    switch (hvScene.team) {
        case TEAM_RED:
            return hvTeamRed;
        case TEAM_BLUE:
            return hvTeamBlue;
        default:
            return hvTeamFree;
    }
}

static float HV_Bias(void) {
    float bias = (hvW - hvH * 4.0f / 3.0f) * 0.5f;

    return bias > 0.5f ? bias : 0.0f;
}

static float HV_XScale(int anchor) {
    if (anchor != WIDESCREEN_STRETCH && HV_Bias() > 0.0f) {
        return hvH * 4.0f / 3.0f / 640.0f;
    }
    return hvW / 640.0f;
}

static float HV_XOffset(int anchor) {
    float bias = HV_Bias();

    if (bias <= 0.0f) {
        return 0.0f;
    }
    if (anchor == WIDESCREEN_CENTER) {
        return bias;
    }
    if (anchor == WIDESCREEN_RIGHT) {
        return bias * 2.0f;
    }
    return 0.0f;
}

void HV_ToScreen(int anchor, float* x, float* y, float* w, float* h) {
    float xs = HV_XScale(anchor);
    float ys = hvH / 480.0f;

    if (x) {
        *x = hvX + HV_XOffset(anchor) + *x * xs;
    }
    if (y) {
        *y = hvY + *y * ys;
    }
    if (w) {
        *w *= xs;
    }
    if (h) {
        *h *= ys;
    }
}

void HV_FromScreen(int anchor, float* x, float* y) {
    float xs = HV_XScale(anchor);
    float ys = hvH / 480.0f;

    if (x) {
        *x = (*x - hvX - HV_XOffset(anchor)) / xs;
    }
    if (y) {
        *y = (*y - hvY) / ys;
    }
}

float HV_UnitsPerPixel(int anchor) {
    return 1.0f / HV_XScale(anchor);
}

void HV_SetCanvas(float x, float y, float w, float h) {
    hvX = x;
    hvY = y;
    hvW = w > 1.0f ? w : 1.0f;
    hvH = h > 1.0f ? h : 1.0f;
}

static void HV_DrawPic(float x, float y, float w, float h, qhandle_t asset) {
    HV_ToScreen(hvAnchor, &x, &y, &w, &h);
    HV_Extend(x, y, w, h);
    HV_NoteShader(asset);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, 1, 1, asset);
}

static void HV_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2,
                              qhandle_t shader) {
    float sx = hvW / (float)uiInfo.uiDC.glconfig.vidWidth;
    float sy = hvH / (float)uiInfo.uiDC.glconfig.vidHeight;

    HV_Extend(hvX + x * sx, hvY + y * sy, w * sx, h * sy);
    HV_NoteShader(shader);
    trap_R_DrawStretchPic(hvX + x * sx, hvY + y * sy, w * sx, h * sy, s1, t1, s2, t2, shader);
}

static void HV_FillRect(float x, float y, float w, float h, const vec4_t color) {
    trap_R_SetColor(color);
    HV_ToScreen(hvAnchor, &x, &y, &w, &h);
    HV_Extend(x, y, w, h);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_SetColor(NULL);
}

static void HV_DrawSides(float x, float y, float w, float h, float size) {
    HV_ToScreen(hvAnchor, &x, &y, &w, &h);
    size *= hvW / 640.0f;
    trap_R_DrawStretchPic(x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_DrawStretchPic(x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

static void HV_DrawTopBottom(float x, float y, float w, float h, float size) {
    HV_ToScreen(hvAnchor, &x, &y, &w, &h);
    size *= hvH / 480.0f;
    trap_R_DrawStretchPic(x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_DrawStretchPic(x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

static void HV_DrawRect(float x, float y, float w, float h, float size, const vec4_t color) {
    trap_R_SetColor(color);
    HV_DrawTopBottom(x, y, w, h, size);
    HV_DrawSides(x, y + size, w, h - size * 2, size);
    trap_R_SetColor(NULL);
}

static float HV_FontPx(float scale) {
    return hvH / 768.0f * 96.0f * scale;
}

static int HV_Font(int fontIndex) {
    return fontIndex >= 0 && fontIndex < 3 ? fontIndex : 0;
}

static void HV_PaintText(float x, float y, int font, float scale, const float* color, const char* text, int limit,
                         int style) {
    float sx = x, sy = y;
    float size = HV_FontPx(scale);
    int lim = limit > 0 ? limit : -1;

    if (!text || !*text) {
        return;
    }
    font = HV_Font(font);
    HV_ToScreen(hvAnchor, &sx, &sy, NULL, NULL);
    if (hvCur >= 0) {
        float ext[5];

        HV_NoteText(text);
        trap_R_Font_TextExtents(text, 0, font, size, lim, ext);
        HV_Extend(sx + ext[0], sy + ext[1], ext[2] - ext[0], ext[3] - ext[1]);
    }
    if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
        float ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1.0f : 2.0f;
        float shx = x + ofs, shy = y + ofs;
        vec4_t shadow;

        shadow[0] = shadow[1] = shadow[2] = 0.0f;
        shadow[3] = color ? color[3] : 1.0f;
        HV_ToScreen(hvAnchor, &shx, &shy, NULL, NULL);
        trap_R_SetColor(shadow);
        trap_R_Font_DrawString((int)shx, (int)shy, text, font, size, lim, NULL, TEXT_NORECOLOR);
    }
    trap_R_SetColor(color);
    trap_R_Font_DrawString((int)sx, (int)sy, text, font, size, lim, NULL, 0);
    trap_R_SetColor(NULL);
}

static void HV_Measure(const char* text, float scale, int font, int limit, float* w, float* h) {
    float ext[5];
    float xs = HV_XScale(hvAnchor);
    float ys = hvH / 480.0f;

    trap_R_Font_TextExtents(text, 0, HV_Font(font), HV_FontPx(scale), limit > 0 ? limit : -1, ext);
    if (w) {
        *w = (float)(int)((ext[2] - ext[0]) / xs);
    }
    if (h) {
        *h = (float)(int)(ext[4] / ys);
    }
}

static float HV_Width(const char* text, float scale, int font) {
    float w = 0;

    HV_Measure(text, scale, font, 0, &w, NULL);
    return w;
}

static float HV_Height(const char* text, float scale, int font) {
    float h = 0;

    HV_Measure(text, scale, font, 0, NULL, &h);
    return h;
}

static float HV_LineHeight(float scale, int font) {
    float ext[5];
    float ys = hvH / 480.0f;

    trap_R_Font_TextExtents("Ag", 0, HV_Font(font), HV_FontPx(scale), -1, ext);
    return (float)(int)((ext[4] + (ext[3] > 0.0f ? ext[3] : 0.0f)) / ys);
}

static float HV_CapHeight(float scale, int font) {
    float ext[5];
    float ys = hvH / 480.0f;

    trap_R_Font_TextExtents("H", 0, HV_Font(font), HV_FontPx(scale), -1, ext);
    return ext[1] < 0.0f ? -ext[1] / ys : 0.0f;
}

static void HV_DrawText_DC(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit,
                           int style, int fontIndex) {
    (void)adjust;
    HV_PaintText(x, y, fontIndex, scale, color, text, limit, style);
}

static float HV_TextWidth_DC(const char* text, float scale, int limit, int fontIndex) {
    float w = 0;

    HV_Measure(text, scale, fontIndex, limit, &w, NULL);
    return w;
}

static float HV_TextHeight_DC(const char* text, float scale, int limit, int fontIndex) {
    float h = 0;

    HV_Measure(text, scale, fontIndex, limit, NULL, &h);
    return h;
}

static void HV_DrawTextWithCursor_DC(float x, float y, float scale, vec4_t color, const char* text, int cursorPos,
                                     char cursor, int limit, int style, int fontIndex) {
    (void)cursorPos;
    (void)cursor;
    HV_PaintText(x, y, fontIndex, scale, color, text, limit, style);
}

static float HV_AlignX(const rectDef_t* r, const char* s, float scale, int font, int align) {
    float x = r->x;

    if (align == ITEM_ALIGN_CENTER) {
        x -= HV_Width(s, scale, font) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= HV_Width(s, scale, font);
    }
    return x;
}

static void HV_Pic(float x, float y, float w, float h, const char* shader, const float* color) {
    trap_R_SetColor(color);
    HV_DrawPic(x, y, w, h, trap_R_RegisterShaderNoMip(shader));
    trap_R_SetColor(NULL);
}

static void HV_Value(const rectDef_t* r, const char* s, float scale, const float* color, qhandle_t shader, int style,
                     int align, int font) {
    float x;

    if (shader) {
        trap_R_SetColor(color);
        HV_DrawPic(r->x, r->y, r->w, r->h, shader);
        trap_R_SetColor(NULL);
        return;
    }
    x = HV_AlignX(r, s, scale, font, align);
    HV_PaintText(x, r->y + HV_Height(s, scale, font), font, scale, color, s, 0, style);
}

static void HV_Line(const rectDef_t* r, const char* s, float scale, const float* color, int style, int align,
                    int font) {
    HV_PaintText(HV_AlignX(r, s, scale, font, align), r->y, font, scale, color, s, 0, style);
}

static void HV_PlaceScore(const rectDef_t* r, int which, float scale, const float* color, int style) {
    int s1 = HV_Score(0), s2 = HV_Score(1);
    const char* num;

    if (!HV_TeamGame() || hvScene.gametype == GT_RR) {
        const char* place = which ? "2. " : "1. ";
        const char* score = va(" %d", which ? s2 : s1);
        float x = r->x + HV_Width(place, scale, 0);
        float right = r->x + r->w - HV_Width(score, scale, 0);

        HV_PaintText(r->x, r->y, 0, scale, color, place, 0, style);
        HV_PaintText(x, r->y, 0, scale, color, which ? "Sarge" : "Visor", 0, style);
        HV_PaintText(right, r->y, 0, scale, color, score, 0, style);
        return;
    }
    if ((which == 0) == (s1 >= s2)) {
        HV_PaintText(r->x, r->y, 0, scale, color, "      Red Team^7", 0, style);
        num = va("%d", s1);
    } else {
        HV_PaintText(r->x, r->y, 0, scale, color, "      Blue Team^7", 0, style);
        num = va("%d", s2);
    }
    HV_PaintText(r->x + r->w - HV_Width(num, scale, 0), r->y, 0, scale, color, num, 0, style);
}

static void HV_Tinted(const rectDef_t* r, const float* rgb, const float* color, qhandle_t shader) {
    vec4_t c;

    if (!shader) {
        return;
    }
    VectorCopy(rgb, c);
    c[3] = color[3];
    trap_R_SetColor(c);
    HV_DrawPic(r->x, r->y, r->w, r->h, shader);
    trap_R_SetColor(NULL);
}

static void HV_Bar(const rectDef_t* r, qhandle_t shader, float frac) {
    float x = r->x, y = r->y, w, h = r->h;
    vec4_t c;

    if (frac <= 0.0f || !shader) {
        return;
    }
    if (frac > 1.0f) {
        frac = 1.0f;
    }
    w = r->w * frac;
    VectorCopy(HV_TeamRGB(), c);
    c[3] = 1.0f;
    trap_R_SetColor(c);
    HV_ToScreen(hvAnchor, &x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, frac, 1.0f, shader);
    trap_R_SetColor(NULL);
}

static void HV_Obits(const rectDef_t* r, float scale) {
    static const char* rows[3][3] = {
        {"Visor", "icons/iconw_rocket", "Sarge"},
        {"Anarki", "icons/iconw_railgun", "Klesk"},
        {"Doom", "icons/iconw_lightning", "Major"},
    };
    static const vec3_t attackerRGB = {1.0f, 0.5f, 0.5f};
    static const vec3_t victimRGB = {0.5f, 0.75f, 1.0f};
    float asc, lineH, capH, bandH, pitch, iconSize, gapX, left, bandTop;
    int i;

    scale *= 0.85f;
    asc = HV_Height("H", scale, 0);
    lineH = HV_LineHeight(scale, 0);
    capH = HV_CapHeight(scale, 0);
    bandH = lineH + 5.0f;
    pitch = lineH + 6.5f;
    iconSize = asc * 1.3f;
    gapX = asc * 0.7f;
    left = (float)(int)r->x + 8.0f;
    bandTop = (float)(int)r->y + 14.0f - asc - 2.5f;

    for (i = 0; i < 3; i++) {
        vec4_t band = {0.0f, 0.0f, 0.0f, 0.45f};
        vec4_t c;
        float baseline = bandTop + (bandH + capH) * 0.5f;
        float attackerW = HV_Width(rows[i][0], scale, 0);
        float victimW = HV_Width(rows[i][2], scale, 0);
        float iconX = left + attackerW + gapX;

        HV_FillRect(left - 4.5f, bandTop, attackerW + gapX + iconSize + gapX + victimW + 9.0f, bandH, band);
        VectorCopy(attackerRGB, c);
        c[3] = 1.0f;
        HV_PaintText(left, baseline, 0, scale, c, rows[i][0], 0, 0);
        HV_Pic(iconX, bandTop + (bandH - iconSize) * 0.5f, iconSize, iconSize, rows[i][1], NULL);
        VectorCopy(victimRGB, c);
        HV_PaintText(iconX + iconSize + gapX, baseline, 0, scale, c, rows[i][2], 0, 0);
        bandTop += pitch;
    }
}

static void HV_Chat(const rectDef_t* r, float scale) {
    static const char* line = "^7foresssst^7: ^1x^3D^2D^5D^4D^6D^1D^3D^2D^5D^4D^6D^1D^3D^2D^5D";
    float bottom = r->y + r->h;
    int saved = hvAnchor;

    if (scale < 0.1f) {
        scale = 0.1f;
    } else if (scale > 0.22f) {
        scale = 0.22f;
    }
    hvAnchor = WIDESCREEN_LEFT;
    HV_PaintText(r->x, bottom, 0, scale, colorWhite, line, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
    hvAnchor = saved;
}

static void HV_Powerups(const rectDef_t* r, int align, float special, float scale, const float* color) {
    static const char* icons[2] = {"icons/quad", "icons/regen"};
    static const int secs[2] = {12, 24};
    rectDef_t r2 = *r;
    float* inc = align == HUD_VERTICAL ? &r2.y : &r2.x;
    int i;

    for (i = 0; i < (hvLoose ? 1 : 2); i++) {
        HV_Pic(r2.x, r2.y, r2.w * 0.75f, r2.h, icons[i], NULL);
        HV_PaintText(r2.x + r2.w * 0.75f + 3, r2.y + r2.h, 0, scale, color, va("%i", secs[i]), 0, 0);
        *inc += r2.w + special;
    }
}

static void HV_LocalTime(const rectDef_t* r, int font, float scale, const float* color, int style, int align) {
    static const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    qtime_t ct;
    const char* s;

    trap_RealTime(&ct);
    s = va("%02d:%02d (%s %02d, %d)", ct.tm_hour, ct.tm_min, months[ct.tm_mon], ct.tm_mday, ct.tm_year + 1900);
    HV_Line(r, s, scale, color, style, align, font);
}

static const char* HV_GametypeName(void) {
    switch (hvScene.gametype) {
        case GT_FFA:
            return "Free For All";
        case GT_DUEL:
            return "Duel";
        case GT_RACE:
            return "Race";
        case GT_TEAM:
            return "Team Deathmatch";
        case GT_CA:
            return "Clan Arena";
        case GT_CTF:
            return "Capture The Flag";
        case GT_1FCTF:
            return "One Flag CTF";
        case GT_HARVESTER:
            return "Harvester";
        case GT_FREEZE:
            return "Freeze Tag";
        case GT_DOMINATION:
            return "Domination";
        case GT_AD:
            return "Attack & Defend";
        case GT_RR:
            return "Red Rover";
        default:
            return "";
    }
}

static const char* HV_GametypeIcon(void) {
    switch (hvScene.gametype) {
        case GT_FFA:
            return "ui/assets/hud/ffa";
        case GT_DUEL:
            return "ui/assets/hud/duel";
        case GT_RACE:
            return "ui/assets/hud/race";
        case GT_TEAM:
            return "ui/assets/hud/tdm";
        case GT_CA:
            return "ui/assets/hud/ca";
        case GT_CTF:
            return "ui/assets/hud/ctf";
        case GT_1FCTF:
            return "ui/assets/hud/1f";
        case GT_HARVESTER:
            return "ui/assets/hud/har";
        case GT_FREEZE:
            return "ui/assets/hud/ft";
        case GT_DOMINATION:
            return "ui/assets/hud/dom";
        case GT_AD:
            return "ui/assets/hud/ad";
        default:
            return "ui/assets/hud/rr";
    }
}

static void HV_OwnerDrawBody(rectDef_t* rp, int ownerDraw, int ownerDrawFlags, int align, float special, float scale,
                             vec4_t color, qhandle_t shader, int textStyle, int font);

static void HV_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw,
                         int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader,
                         int textStyle, int fontIndex) {
    rectDef_t r;
    int font = fontIndex;

    (void)text_x;
    (void)text_y;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    HV_TrackBegin(ownerDraw, y, w, h);
    HV_OwnerDrawBody(&r, ownerDraw, ownerDrawFlags, align, special, scale, color, shader, textStyle, font);
    hvCur = hvLoose ? HV_MAX_BOUNDS : -1;
}

static void HV_OwnerDrawBody(rectDef_t* rp, int ownerDraw, int ownerDrawFlags, int align, float special, float scale,
                             vec4_t color, qhandle_t shader, int textStyle, int font) {
    rectDef_t r = *rp;
    char buf[64];

    switch (ownerDraw) {
        case CG_PLAYER_HEALTH:
            HV_Value(&r, va("%i", HV_Health()), scale, color, shader, textStyle, align, font);
            break;
        case CG_PLAYER_ARMOR_VALUE:
            HV_Value(&r, va("%i", HV_Armor()), scale, color, shader, textStyle, align, font);
            break;
        case CG_PLAYER_AMMO_VALUE:
            HV_Value(&r, va("%i", HV_Ammo()), scale, color, shader, textStyle, align, font);
            break;
        case CG_PLAYER_ARMOR_ICON:
        case CG_PLAYER_ARMOR_ICON2D:
            HV_Pic(r.x, r.y + r.h / 2 + 1, r.w, r.h, "icons/iconr_yellow", NULL);
            break;
        case CG_PLAYER_AMMO_ICON:
        case CG_PLAYER_AMMO_ICON2D:
            HV_Pic(r.x, r.y, r.w, r.h, "icons/icona_rocket", NULL);
            break;
        case CG_PLAYER_ITEM:
            HV_Pic(r.x, r.y, r.w, r.h, "icons/medkit", NULL);
            break;
        case CG_PLAYER_HEAD:
            HV_Pic(r.x, r.y, r.w, r.h, "models/players/sarge/icon_default", NULL);
            break;
        case CG_PLAYER_SCORE:
            HV_Line(&r, va("%i", HV_PlayerScore()), scale, color, textStyle, align, font);
            break;
        case CG_1STPLACE:
            HV_Line(&r, va("%i", HV_Score(0)), scale, color, textStyle, align, font);
            break;
        case CG_2NDPLACE:
            HV_Line(&r, va("%i", HV_Score(1)), scale, color, textStyle, align, font);
            break;
        case CG_1ST_PLACE_SCORE:
            HV_PlaceScore(&r, 0, scale, color, textStyle);
            break;
        case CG_2ND_PLACE_SCORE:
            HV_PlaceScore(&r, 1, scale, color, textStyle);
            break;
        case CG_RED_SCORE:
            HV_Line(&r, va("%i", HV_Score(0)), scale, color, textStyle, align, font);
            break;
        case CG_BLUE_SCORE:
            HV_Line(&r, va("%i", HV_Score(1)), scale, color, textStyle, align, font);
            break;
        case CG_RED_NAME:
            HV_Line(&r, "Red Team", scale, color, textStyle, align, 0);
            break;
        case CG_BLUE_NAME:
            HV_Line(&r, "Blue Team", scale, color, textStyle, align, 0);
            break;
        case CG_LEVELTIMER:
            HV_Line(&r, "4:27", scale, color, textStyle, ITEM_ALIGN_LEFT, 0);
            break;
        case CG_ROUNDTIMER:
            if (HV_RoundGame()) {
                HV_Line(&r, "1:12", scale, color, textStyle, ITEM_ALIGN_LEFT, 0);
            }
            break;
        case CG_ROUND:
            if (HV_RoundGame()) {
                HV_Line(&r, "Round 7", scale, color, textStyle, ITEM_ALIGN_LEFT, font);
            }
            break;
        case CG_CAPFRAGLIMIT:
            HV_Line(&r, HV_RoundGame() ? "10" : HV_FlagGame() ? "8" : "50", scale, color, textStyle, align, 0);
            break;
        case CG_LOCALTIME:
            HV_LocalTime(&r, font, scale, color, textStyle, align);
            break;
        case CG_MAP_NAME:
            HV_Line(&r, "Campgrounds", scale, color, textStyle, ITEM_ALIGN_LEFT, font);
            break;
        case CG_GAME_TYPE:
            HV_Line(&r, HV_GametypeName(), scale, color, textStyle, ITEM_ALIGN_LEFT, font);
            break;
        case CG_GAME_TYPE_ICON:
            HV_Pic(r.x, r.y, r.w, r.h, HV_GametypeIcon(), NULL);
            break;
        case CG_PLAYER_COUNTS:
            HV_Line(&r, "8 Players", scale, color, textStyle, ITEM_ALIGN_LEFT, font);
            break;
        case CG_RED_PLAYER_COUNT:
        case CG_BLUE_PLAYER_COUNT:
            HV_Line(&r, HV_TeamGame() ? "(4/4)" : "4 Players", scale, color, textStyle, align, 0);
            break;
        case CG_RED_AVG_PING:
            HV_Line(&r, "32", scale, color, textStyle, align, font);
            break;
        case CG_BLUE_AVG_PING:
            HV_Line(&r, "41", scale, color, textStyle, align, font);
            break;
        case CG_RED_CLAN_PLYRS:
            if (HV_RoundGame()) {
                HV_Line(&r, "4", scale, color, textStyle, ITEM_ALIGN_LEFT, 0);
            }
            break;
        case CG_BLUE_CLAN_PLYRS:
            if (HV_RoundGame()) {
                HV_Line(&r, "3", scale, color, textStyle, ITEM_ALIGN_LEFT, 0);
            }
            break;
        case CG_TEAM_PLYR_COUNT:
            if (HV_RoundGame()) {
                HV_Line(&r, hvScene.team == TEAM_BLUE ? "3" : "4", scale, color, textStyle, ITEM_ALIGN_LEFT, 0);
            }
            break;
        case CG_ENEMY_PLYR_COUNT:
            if (HV_RoundGame()) {
                HV_Line(&r, hvScene.team == TEAM_BLUE ? "4" : "3", scale, color, textStyle, ITEM_ALIGN_LEFT, 0);
            }
            break;
        case CG_RED_TIMEOUT_COUNT:
        case CG_BLUE_TIMEOUT_COUNT:
            HV_Line(&r, "1", scale, color, textStyle, align, 0);
            break;
        case CG_TEAM_COLORIZED:
            HV_Tinted(&r, HV_TeamRGB(), color, shader);
            break;
        case CG_ARMORTIERED_COLORIZED: {
            vec3_t rgb;

            rgb[0] = HV_ArmorType() == 0 ? 0.0f : 1.0f;
            rgb[1] = HV_ArmorType() == 2 ? 0.0f : 1.0f;
            rgb[2] = 0.0f;
            HV_Tinted(&r, rgb, color, shader);
            break;
        }
        case CG_HEALTH_COLORIZED:
            HV_Tinted(&r, color, color, shader);
            break;
        case CG_TEAM_COLOR:
        case CG_SELECTED_PLYR_TEAM_COLOR:
            if (hvScene.team == TEAM_RED || hvScene.team == TEAM_BLUE) {
                vec4_t c;

                c[0] = hvScene.team == TEAM_RED ? 1.0f : 0.0f;
                c[1] = 0.0f;
                c[2] = hvScene.team == TEAM_BLUE ? 1.0f : 0.0f;
                c[3] = ownerDraw == CG_TEAM_COLOR ? color[3] : 0.33f;
                HV_Pic(r.x, r.y, r.w, r.h, "gfx/2d/colorbar", c);
            }
            break;
        case CG_PLAYER_HEALTH_BAR_100:
            HV_Bar(&r, shader, HV_Health() / 100.0f);
            break;
        case CG_PLAYER_HEALTH_BAR_200:
            HV_Bar(&r, shader, (HV_Health() - 100) / 100.0f);
            break;
        case CG_PLAYER_ARMOR_BAR_100:
            HV_Bar(&r, shader, HV_Armor() / 100.0f);
            break;
        case CG_PLAYER_ARMOR_BAR_200:
            HV_Bar(&r, shader, (HV_Armor() - 100) / 100.0f);
            break;
        case CG_PLAYER_OBIT:
            HV_Obits(&r, scale);
            break;
        case CG_AREA_NEW_CHAT:
            HV_Chat(&r, scale);
            break;
        case CG_AREA_POWERUP:
            HV_Powerups(&r, align, special, scale, color);
            break;
        case CG_CTF_POWERUP:
            if (HV_FlagGame()) {
                HV_Pic(r.x, r.y, r.w, r.h, "icons/guard", NULL);
            }
            break;
        case CG_PLAYER_HASFLAG:
        case CG_PLAYER_HASFLAG2D:
            if (HV_FlagGame() && hvScene.hasFlag) {
                HV_Pic(r.x, r.y, r.w, r.h,
                       hvScene.team == TEAM_BLUE ? "gfx/2d/flag_status/red_flag_taken.tga"
                                                 : "gfx/2d/flag_status/blue_flag_taken.tga",
                       NULL);
            }
            break;
        case CG_RED_FLAGSTATUS:
        case CG_RED_BASESTATUS:
            if (hvScene.gametype == GT_CTF) {
                HV_Pic(r.x, r.y, r.w, r.h, "gfx/2d/flag_status/red_flag_at_base.tga", colorWhite);
            }
            break;
        case CG_BLUE_FLAGSTATUS:
        case CG_BLUE_BASESTATUS:
            if (hvScene.gametype == GT_CTF) {
                HV_Pic(r.x, r.y, r.w, r.h, "gfx/2d/flag_status/blue_flag_taken.tga", colorWhite);
            }
            break;
        case CG_ONEFLAG_STATUS:
            if (hvScene.gametype == GT_1FCTF) {
                HV_Pic(r.x, r.y, r.w, r.h, "gfx/2d/flag_status/flag_at_base.tga", colorWhite);
            }
            break;
        case CG_SPECTATORS:
            HV_PaintText(r.x, r.y + r.h - 3.0f, font, scale, color, "Spectators: Klesk Major", 0, 0);
            break;
        case CG_KILLER:
            Com_sprintf(buf, sizeof(buf), "Fragged by %s", "Sarge");
            HV_Line(&r, buf, scale, color, textStyle, align, 0);
            break;
        case CG_FOLLOW_PLAYER_NAME:
        case CG_FOLLOW_PLAYER_NAME_EX:
            HV_Line(&r, "Visor", scale, color, textStyle, align, font);
            break;
        case CG_ACCURACY:
        case CG_ASSISTS:
        case CG_CAPTURES:
        case CG_DEFEND:
        case CG_EXCELLENT:
        case CG_GAUNTLET:
        case CG_IMPRESSIVE:
        case CG_PERFECT: {
            vec4_t c;

            Vector4Copy(color, c);
            if (shader) {
                trap_R_SetColor(c);
                HV_DrawPic(r.x, r.y, r.w, r.h, shader);
                trap_R_SetColor(NULL);
            }
            if (ownerDraw != CG_PERFECT) {
                const char* s = ownerDraw == CG_ACCURACY ? "47%" : "3";
                HV_PaintText(r.x + (r.w - HV_Width(s, scale, 0)) * 0.5f, r.y + r.h + HV_Height(s, scale, 0) + 2, 0,
                             scale, c, s, 0, 0);
            }
            break;
        }
        case UI_ADVERT:
            if (shader) {
                trap_R_SetColor(color);
                HV_DrawPic(r.x, r.y, r.w, r.h, shader);
                trap_R_SetColor(NULL);
            }
            break;
        default:
            (void)ownerDrawFlags;
            break;
    }
}

static float HV_GetValue(int ownerDraw) {
    switch (ownerDraw) {
        case CG_PLAYER_ARMOR_VALUE:
            return (float)HV_Armor();
        case CG_PLAYER_AMMO_VALUE:
            return (float)HV_Ammo();
        case CG_PLAYER_SCORE:
            return (float)HV_PlayerScore();
        case CG_PLAYER_HEALTH:
            return (float)HV_Health();
        case CG_RED_SCORE:
            return (float)HV_Score(0);
        case CG_BLUE_SCORE:
            return (float)HV_Score(1);
        default:
            return -1.0f;
    }
}

static qboolean HV_OwnerDrawVisible(int flags, int flags2) {
    int gt = hvScene.gametype;

    if (flags & CG_SHOW_TEAMINFO) {
        return qfalse;
    }
    if (flags & CG_SHOW_NOTEAMINFO) {
        return qtrue;
    }
    if (flags & CG_SHOW_OTHERTEAMHASFLAG) {
        return HV_FlagGame() && gt != GT_1FCTF;
    }
    if (flags & CG_SHOW_YOURTEAMHASENEMYFLAG) {
        return HV_FlagGame() && hvScene.hasFlag;
    }
    if (flags & (CG_SHOW_BLUE_TEAM_HAS_REDFLAG | CG_SHOW_RED_TEAM_HAS_BLUEFLAG)) {
        if (gt == GT_CTF) {
            return (flags & CG_SHOW_RED_TEAM_HAS_BLUEFLAG) ? qtrue : qfalse;
        }
        if (gt != GT_1FCTF) {
            return qfalse;
        }
    }
    if ((flags & CG_SHOW_PLAYERS_REMAINING) && HV_RoundGame()) {
        return qtrue;
    }
    if ((flags & CG_SHOW_DUEL) && gt == GT_DUEL) {
        return qtrue;
    }
    if (flags & CG_SHOW_CLAN_ARENA) {
        return gt == GT_CA;
    }
    if ((flags & CG_SHOW_CTF) && gt == GT_CTF) {
        return qtrue;
    }
    if (flags & CG_SHOW_ONEFLAG) {
        return gt == GT_1FCTF;
    }
    if (flags & CG_SHOW_HARVESTER) {
        return gt == GT_HARVESTER;
    }
    if (flags & CG_SHOW_DOMINATION) {
        return gt == GT_DOMINATION;
    }
    if ((flags & CG_SHOW_ANYNONTEAMGAME) && gt < GT_TEAM) {
        return qtrue;
    }
    if ((flags & CG_SHOW_ANYTEAMGAME) && gt >= GT_TEAM) {
        return qtrue;
    }
    if ((flags & CG_SHOW_HEALTHCRITICAL) && HV_Health() < 25) {
        return qtrue;
    }
    if ((flags & CG_SHOW_IF_PLAYER_HAS_FLAG) && HV_FlagGame() && hvScene.hasFlag) {
        return qtrue;
    }
    if (flags & CG_SHOW_NOTINTERMISSION) {
        return qtrue;
    }
    if (flags & (CG_SHOW_IF_MSG_PRESENT | CG_SHOW_IF_NOTICE_PRESENT | CG_SHOW_IF_CHAT_VISIBLE)) {
        return qfalse;
    }
    if (flags & CG_SHOW_IF_PLYR_IS_FIRST_PLACE) {
        return qtrue;
    }
    if (flags & CG_SHOW_IF_PLYR_IS_NOT_FIRST_PLACE) {
        return qfalse;
    }
    if (flags & CG_SHOW_IF_RED_IS_FIRST_PLACE) {
        return qtrue;
    }
    if (flags & CG_SHOW_IF_BLUE_IS_FIRST_PLACE) {
        return qfalse;
    }
    if ((flags & CG_SHOW_IF_PLYR_IS_ON_RED) && hvScene.team == TEAM_RED) {
        return qtrue;
    }
    if ((flags & CG_SHOW_IF_PLYR_IS_ON_BLUE) && hvScene.team == TEAM_BLUE) {
        return qtrue;
    }
    if ((flags & CG_SHOW_IF_WARMUP) && hvScene.warmup) {
        return qtrue;
    }
    if ((flags & CG_SHOW_IF_NOT_WARMUP) && !hvScene.warmup) {
        return qtrue;
    }
    if ((flags2 & (CG_SHOW_IF_PLYR_IS_ON_RED_OR_SPEC | CG_SHOW_IF_PLYR_IS_ON_RED_NO_SPEC)) &&
        hvScene.team == TEAM_RED) {
        return qtrue;
    }
    if ((flags2 & (CG_SHOW_IF_PLYR_IS_ON_BLUE_OR_SPEC | CG_SHOW_IF_PLYR_IS_ON_BLUE_NO_SPEC)) &&
        hvScene.team == TEAM_BLUE) {
        return qtrue;
    }
    if ((flags2 & CG_SHOW_IF_PLYR1) && gt == GT_DUEL) {
        return qtrue;
    }
    if (flags2 & CG_SHOW_IF_LOADOUT_DISABLED) {
        return qtrue;
    }
    return qfalse;
}

static void HV_GetTeamColor(vec4_t* color) {
    (*color)[0] = hvScene.team == TEAM_RED ? 1.0f : 0.0f;
    (*color)[1] = hvScene.team == TEAM_RED || hvScene.team == TEAM_BLUE ? 0.0f : 0.17f;
    (*color)[2] = hvScene.team == TEAM_BLUE ? 1.0f : 0.0f;
    (*color)[3] = 0.25f;
}

static void HV_SetWidescreen(int mode) {
    hvAnchor = mode;
}

static int HV_OwnerDrawWidth(int ownerDraw, float scale) {
    (void)ownerDraw;
    (void)scale;
    return 0;
}

static void HV_RunScript(char** p) {
    (void)p;
}

static void HV_StartLocalSound(sfxHandle_t sfx, int channel) {
    (void)sfx;
    (void)channel;
}

static sfxHandle_t HV_RegisterSound(const char* name, qboolean compressed) {
    (void)name;
    (void)compressed;
    return 0;
}

static int HV_FeederCount(float feederID) {
    (void)feederID;
    return 0;
}

static const char* HV_FeederItemText(float feederID, int index, int column, qhandle_t* handle) {
    (void)feederID;
    (void)index;
    (void)column;
    if (handle) {
        *handle = -1;
    }
    return "";
}

static qhandle_t HV_FeederItemImage(float feederID, int index) {
    (void)feederID;
    (void)index;
    return 0;
}

static void HV_FeederSelection(float feederID, int index) {
    (void)feederID;
    (void)index;
}

static void HV_ExecuteText(int when, const char* text) {
    (void)when;
    (void)text;
}

static int HV_PlayCinematic(const char* name, float x, float y, float w, float h) {
    (void)name;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    return -1;
}

static void HV_StopCinematic(int handle) {
    (void)handle;
}

static void HV_DrawCinematic(int handle, float x, float y, float w, float h) {
    (void)handle;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

static void HV_RunCinematicFrame(int handle) {
    (void)handle;
}

static void HV_StartBackgroundTrack(const char* intro, const char* loop) {
    (void)intro;
    (void)loop;
}

static void HV_StopBackgroundTrack(void) {
}

static void HV_GetCVarString(const char* cvar, char* buffer, int bufsize) {
    if (!Q_stricmp(cvar, "cg_gametype") || !Q_stricmp(cvar, "g_gametype")) {
        Com_sprintf(buffer, bufsize, "%i", hvScene.gametype);
        return;
    }
    trap_Cvar_VariableStringBuffer(cvar, buffer, bufsize);
}

static void HV_Begin(void) {
    hvSaved = Display_GetContext();
    hvDC = *hvSaved;
    hvDC.drawHandlePic = HV_DrawPic;
    hvDC.drawStretchPic = HV_DrawStretchPic;
    hvDC.drawText = HV_DrawText_DC;
    hvDC.textWidth = HV_TextWidth_DC;
    hvDC.textHeight = HV_TextHeight_DC;
    hvDC.fillRect = HV_FillRect;
    hvDC.drawRect = HV_DrawRect;
    hvDC.drawSides = HV_DrawSides;
    hvDC.drawTopBottom = HV_DrawTopBottom;
    hvDC.ownerDrawItem = HV_OwnerDraw;
    hvDC.getValue = HV_GetValue;
    hvDC.getCVarString = HV_GetCVarString;
    hvDC.ownerDrawVisible = HV_OwnerDrawVisible;
    hvDC.runScript = HV_RunScript;
    hvDC.getTeamColor = HV_GetTeamColor;
    hvDC.drawTextWithCursor = HV_DrawTextWithCursor_DC;
    hvDC.startLocalSound = HV_StartLocalSound;
    hvDC.registerSound = HV_RegisterSound;
    hvDC.feederCount = HV_FeederCount;
    hvDC.feederItemText = HV_FeederItemText;
    hvDC.feederItemImage = HV_FeederItemImage;
    hvDC.feederSelection = HV_FeederSelection;
    hvDC.executeText = HV_ExecuteText;
    hvDC.ownerDrawWidth = HV_OwnerDrawWidth;
    hvDC.playCinematic = HV_PlayCinematic;
    hvDC.stopCinematic = HV_StopCinematic;
    hvDC.drawCinematic = HV_DrawCinematic;
    hvDC.runCinematicFrame = HV_RunCinematicFrame;
    hvDC.startBackgroundTrack = HV_StartBackgroundTrack;
    hvDC.stopBackgroundTrack = HV_StopBackgroundTrack;
    hvDC.setWidescreen = HV_SetWidescreen;
    hvDC.Assets = hvAssets;
    hvAnchor = WIDESCREEN_STRETCH;
    Init_Display(&hvDC);
}

static void HV_End(void) {
    hvAssets = hvDC.Assets;
    Init_Display(hvSaved);
}

int HV_ReadList(const char* listFile, char (*files)[MAX_QPATH], int max) {
    static char buf[MAX_MENUDEFFILE];
    fileHandle_t f;
    char* p;
    char* token;
    int len, count = 0;

    len = trap_FS_FOpenFile(listFile, &f, FS_READ);
    if (!f) {
        return 0;
    }
    if (len >= MAX_MENUDEFFILE) {
        trap_FS_FCloseFile(f);
        return 0;
    }
    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);
    COM_Compress(buf);
    p = buf;
    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || token[0] == '}') {
            break;
        }
        if (Q_stricmp(token, "loadmenu") != 0) {
            continue;
        }
        token = COM_ParseExt(&p, qtrue);
        if (token[0] != '{') {
            break;
        }
        while (1) {
            token = COM_ParseExt(&p, qtrue);
            if (!token[0] || token[0] == '}') {
                break;
            }
            if (count < max) {
                Q_strncpyz(files[count++], token, MAX_QPATH);
            }
        }
    }
    return count;
}

static qboolean HV_AssetParse(int handle) {
    pc_token_t token;
    const char* s;
    int n;

    if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{") != 0) {
        return qfalse;
    }
    while (1) {
        if (!trap_PC_ReadToken(handle, &token)) {
            return qfalse;
        }
        if (Q_stricmp(token.string, "}") == 0) {
            return qtrue;
        }
        if (!Q_stricmp(token.string, "font") || !Q_stricmp(token.string, "smallFont") ||
            !Q_stricmp(token.string, "bigfont")) {
            if (!PC_String_Parse(handle, &s) || !PC_Int_Parse(handle, &n)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "gradientbar")) {
            if (!PC_String_Parse(handle, &s)) {
                return qfalse;
            }
            hvDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(s);
        } else if (!Q_stricmp(token.string, "menuEnterSound") || !Q_stricmp(token.string, "menuExitSound") ||
                   !Q_stricmp(token.string, "itemFocusSound") || !Q_stricmp(token.string, "menuBuzzSound")) {
            if (!PC_String_Parse(handle, &s)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "cursor")) {
            if (!PC_String_Parse(handle, &s)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "fadeClamp")) {
            if (!PC_Float_Parse(handle, &hvDC.Assets.fadeClamp)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "fadeCycle")) {
            if (!PC_Int_Parse(handle, &hvDC.Assets.fadeCycle)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "fadeAmount")) {
            if (!PC_Float_Parse(handle, &hvDC.Assets.fadeAmount)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "shadowX")) {
            if (!PC_Float_Parse(handle, &hvDC.Assets.shadowX)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "shadowY")) {
            if (!PC_Float_Parse(handle, &hvDC.Assets.shadowY)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token.string, "shadowColor")) {
            if (!PC_Color_Parse(handle, &hvDC.Assets.shadowColor)) {
                return qfalse;
            }
            hvDC.Assets.shadowFadeClamp = hvDC.Assets.shadowColor[3];
        }
    }
}

static int HV_ParseFile(const char* file) {
    pc_token_t token;
    int handle, before = hvCount;

    handle = trap_PC_LoadSource(file);
    if (!handle) {
        return -1;
    }
    while (trap_PC_ReadToken(handle, &token)) {
        if (token.string[0] == '}') {
            break;
        }
        if (!Q_stricmp(token.string, "assetGlobalDef")) {
            if (!HV_AssetParse(handle)) {
                break;
            }
            continue;
        }
        if (!Q_stricmp(token.string, "menudef")) {
            if (hvCount >= HV_MAX_MENUS) {
                break;
            }
            if (Menu_NewInto(handle, &hvMenu[hvCount])) {
                hvCount++;
            }
        }
    }
    trap_PC_FreeSource(handle);
    return hvCount - before;
}

void HV_Unload(void) {
    if (hvLoaded) {
        UI_MemRelease(&hvMark, &hvEnd);
    }
    hvLoaded = qfalse;
    hvCount = 0;
}

qboolean HV_Load(char (*files)[MAX_QPATH], int numFiles, int* menusPerFile) {
    int i;

    HV_Unload();
    UI_MemBeginPrivate(&hvMark);
    memset(&hvAssets, 0, sizeof(hvAssets));
    hvAssets.fadeClamp = 1.0f;
    hvAssets.fadeCycle = 1;
    hvAssets.fadeAmount = 0.1f;
    hvAssets.shadowColor[3] = 0.25f;
    hvAssets.shadowX = 2;
    hvAssets.shadowY = 2;
    hvAssets.gradientBar = trap_R_RegisterShaderNoMip(ASSET_GRADIENTBAR);
    HV_Begin();
    for (i = 0; i < numFiles; i++) {
        int n = HV_ParseFile(files[i]);

        if (menusPerFile) {
            menusPerFile[i] = n;
        }
    }
    HV_End();
    UI_MemEndPrivate(&hvEnd);
    hvLoaded = qtrue;
    return hvCount > 0;
}

qboolean HV_Loaded(void) {
    return hvLoaded;
}

int HV_MenuCount(void) {
    return hvCount;
}

menuDef_t* HV_Menu(int index) {
    return index >= 0 && index < hvCount ? &hvMenu[index] : NULL;
}

menuDef_t* HV_FindMenu(const char* name) {
    int i;

    for (i = 0; i < hvCount; i++) {
        if (hvMenu[i].window.name && !Q_stricmp(hvMenu[i].window.name, name)) {
            return &hvMenu[i];
        }
    }
    return NULL;
}

menuDef_t* HV_AddMenu(const char* name) {
    static char added[32];
    menuDef_t* m;

    if (!hvLoaded || hvCount >= HV_MAX_MENUS) {
        return NULL;
    }
    m = &hvMenu[hvCount];
    HV_Begin();
    Menu_Init(m);
    HV_End();
    Q_strncpyz(added, name, sizeof(added));
    m->window.name = added;
    m->window.rect.w = m->window.rectClient.w = 640.0f;
    m->window.rect.h = m->window.rectClient.h = 480.0f;
    m->window.flags |= WINDOW_VISIBLE;
    m->widescreen = WIDESCREEN_STRETCH;
    hvCount++;
    return m;
}

static float HV_Cvar(const char* name) {
    return trap_Cvar_VariableValue(name);
}

static float HV_Noise(int i, int seed) {
    unsigned int h = (unsigned int)(i * 374761393 + seed * 668265263);

    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 1023) / 1023.0f;
}

static void HV_Line1(float x, float y, float w, float h) {
    if (h > 0.0f && w > 0.0f) {
        trap_R_DrawStretchPic(x, y, w, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    }
}

static void HV_PreviewCrosshair(void) {
    int n = (int)HV_Cvar("cg_drawCrosshair");
    float size = HV_Cvar("cg_crosshairSize"), bright = HV_Cvar("cg_crosshairBrightness"), scale = hvW / 640.0f;
    vec4_t color;
    float w, h, x, y;

    if (n <= 0 || n % NUM_CROSSHAIRS == 0) {
        return;
    }
    if (HV_Cvar("cg_crosshairHealth") != 0.0f) {
        color[0] = color[1] = color[2] = color[3] = 1.0f;
    } else {
        int c = (int)HV_Cvar("cg_crosshairColor");

        Vector4Copy(UI_CrosshairPalette()[c >= 1 && c <= 26 ? c - 1 : 0], color);
    }
    VectorScale(color, bright, color);
    color[3] = 1.0f;
    w = h = size * scale;
    x = hvX + HV_Cvar("cg_crosshairX") * scale + 0.5f * (hvW - w);
    y = hvY + HV_Cvar("cg_crosshairY") * scale + 0.5f * (hvH - h);
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, 1, 1, uiInfo.uiDC.Assets.crosshairShader[n % NUM_CROSSHAIRS]);
    trap_R_SetColor(NULL);
}

static void HV_PreviewLagometer(void) {
    static const vec4_t yellow = {1.0f, 1.0f, 0.0f, 1.0f};
    static const vec4_t blue = {0.0f, 0.0f, 1.0f, 1.0f};
    static const vec4_t green = {0.0f, 1.0f, 0.0f, 1.0f};
    int mode = (int)HV_Cvar("cg_lagometer");
    float ax = 592.0f, ay = 336.0f, aw = 48.0f, ah = 48.0f, range, mid, v;
    int a, n;

    if (mode <= 0) {
        return;
    }
    hvAnchor = WIDESCREEN_RIGHT;
    HV_DrawPic(592.0f, 336.0f, 48.0f, 48.0f, trap_R_RegisterShaderNoMip("lagometer"));
    HV_ToScreen(WIDESCREEN_RIGHT, &ax, &ay, &aw, &ah);
    n = (int)aw;
    range = ah / 3.0f;
    mid = ay + range;
    for (a = 0; a < n; a++) {
        v = (12.0f + 30.0f * HV_Noise(a, 1)) * range / 300.0f;
        if (HV_Noise(a, 2) > 0.93f) {
            trap_R_SetColor(yellow);
            HV_Line1(ax + aw - a, mid - v, 1.0f, v);
        } else {
            trap_R_SetColor(blue);
            HV_Line1(ax + aw - a, mid, 1.0f, v);
        }
    }
    range = ah / 2.0f;
    trap_R_SetColor(green);
    for (a = 0; a < n; a++) {
        v = (38.0f + 24.0f * HV_Noise(a, 3)) * range / 900.0f;
        HV_Line1(ax + aw - a, ay + ah - v, 1.0f, v);
    }
    trap_R_SetColor(NULL);
    if (mode == 2) {
        HV_PaintText(592.0f, HV_Height("48", 0.175f, 0) + 337.0f, 0, 0.15f, colorWhite, "48", 0,
                     ITEM_TEXTSTYLE_SHADOWED);
    }
}

static float HV_PreviewFPS(float y) {
    char s[16];
    float w, h, fps = HV_Cvar("com_maxfps");

    Com_sprintf(s, sizeof(s), "%ifps", fps > 0.0f ? (int)fps : 125);
    w = HV_Width(s, 0.25f, 2);
    h = HV_Height(s, 0.25f, 2);
    HV_PaintText(635.0f - w, y + h, 2, 0.25f, colorWhite, s, 0, ITEM_TEXTSTYLE_SHADOWED);
    return y + h;
}

static void HV_PreviewSpeedometer(void) {
    static const vec4_t bar1 = {0.0f, 1.0f, 0.0f, 1.0f};
    static const vec4_t bar2 = {1.0f, 1.0f, 0.0f, 1.0f};
    int mode = (int)HV_Cvar("cg_speedometer"), anchor, i, speed = 0;
    float rx, ry, rw, rh, gx, gy, barScale, barWidth, half, s, bh, full, over, total;
    vec4_t c1, c2;
    char text[16];

    if (mode <= 0 || mode >= 4) {
        return;
    }
    if (mode < 2) {
        rx = gx = 592.0f;
        ry = gy = 384.0f;
        rw = rh = 48.0f;
        anchor = WIDESCREEN_RIGHT;
    } else {
        rx = gx = 256.0f;
        ry = gy = (float)((int)HV_Cvar("cg_crosshairY") / 2 + 241);
        rw = 128.0f;
        rh = 32.0f;
        anchor = WIDESCREEN_CENTER;
    }
    Vector4Copy(bar1, c1);
    Vector4Copy(bar2, c2);
    c1[3] = c2[3] = mode < 2 ? 1.0f : 0.75f;
    hvAnchor = anchor;
    if (mode < 2) {
        HV_DrawPic(gx, gy, 48.0f, 48.0f, trap_R_RegisterShaderNoMip("lagometer"));
    }
    HV_ToScreen(anchor, &rx, &ry, &rw, &rh);
    half = rh * 0.5f;
    barScale = (rh - 5.0f) / (320.0f * 3.0f);
    barWidth = rw / 128.0f;
    for (i = 0; i < 128; i++) {
        s = 320.0f + 200.0f * (0.5f + 0.5f * (float)sin(i * 0.09f)) + 40.0f * HV_Noise(i, 4);
        speed = (int)s;
        if (mode >= 3) {
            continue;
        }
        bh = barScale * s;
        if (bh > half) {
            bh = half;
        }
        trap_R_SetColor(c1);
        trap_R_DrawStretchPic(rx + barWidth * i, ry + rh - bh, barWidth, bh, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
        full = barScale * s;
        if (full > half) {
            over = full - bh;
            if (over > half) {
                over = half;
            }
            total = full > rh ? rh : full;
            trap_R_SetColor(c2);
            trap_R_DrawStretchPic(rx + barWidth * i, ry + rh - total, barWidth, over, 0, 0, 0, 0,
                                  uiInfo.uiDC.whiteShader);
        }
    }
    trap_R_SetColor(NULL);
    Com_sprintf(text, sizeof(text), "%d", speed);
    if (mode < 2) {
        hvAnchor = WIDESCREEN_RIGHT;
        HV_PaintText(gx + 2.0f, gy + 8.0f, 0, 0.15f, colorWhite, text, 0, ITEM_TEXTSTYLE_SHADOWED);
    } else {
        HV_PaintText((float)(320 - (int)(HV_Width(text, 0.175f, 0) / 2.0f)), gy + 33.0f, 0, 0.15f, colorWhite, text,
                     0, ITEM_TEXTSTYLE_SHADOWED);
    }
}

static int HV_SampleAmmo(int weapon) {
    switch (weapon) {
        case WP_MACHINEGUN:
            return 100;
        case WP_SHOTGUN:
            return 25;
        case WP_GRENADE_LAUNCHER:
            return 10;
        case WP_ROCKET_LAUNCHER:
            return 25;
        case WP_LIGHTNING:
            return 150;
        case WP_RAILGUN:
            return 10;
        case WP_PLASMAGUN:
            return 50;
        default:
            return -1;
    }
}

static void HV_PreviewWeaponBar(void) {
    static const vec4_t normal = {1.0f, 1.0f, 1.0f, 1.0f};
    static const vec4_t low = {1.0f, 0.8f, 0.2f, 0.8f};
    static const vec4_t empty = {1.0f, 0.0f, 0.0f, 0.8f};
    int mode = (int)HV_Cvar("cg_weaponBar"), full = HV_Cvar("cg_drawFullWeaponBar") != 0.0f;
    int lowAmmo = (int)HV_Cvar("cg_lowAmmoWeaponBarWarning"), i, count = 0, ammoOffset;
    float x, y, xStep, yStep;

    if (mode <= 0 || mode >= 4) {
        return;
    }
    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        if (HV_SampleAmmo(i) >= 0 || (full && BG_FindItemForWeapon((weapon_t)i)->icon)) {
            count++;
        }
    }
    if (mode != 3) {
        x = mode == 2 ? 623.0f : 1.0f;
        ammoOffset = mode == 2 ? -3 : 20;
        y = 94.0f;
        xStep = 0.0f;
        yStep = 22.0f;
        hvAnchor = mode == 2 ? WIDESCREEN_RIGHT : WIDESCREEN_LEFT;
    } else {
        x = 320.0f - (float)count * 55.0f * 0.5f;
        y = 413.0f;
        xStep = 55.0f;
        yStep = 0.0f;
        ammoOffset = 20;
        hvAnchor = WIDESCREEN_CENTER;
    }
    trap_R_SetColor(NULL);
    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        int ammo = HV_SampleAmmo(i);
        const char* iconName = BG_FindItemForWeapon((weapon_t)i)->icon;
        qboolean has = ammo >= 0;

        if (!iconName || (!has && !full)) {
            continue;
        }
        if (i == WP_ROCKET_LAUNCHER) {
            HV_DrawPic(mode == 2 ? x - 33.0f : x - 4.0f, y - 2.0f, 52.0f, 20.0f,
                       trap_R_RegisterShaderNoMip("ui/assets/hud/weaplit2.tga"));
        }
        HV_DrawPic(x, y, 16.0f, 16.0f, trap_R_RegisterShaderNoMip(iconName));
        if (has) {
            char s[8];
            float tx = x + (float)ammoOffset;
            const float* c = ammo == 0 ? empty : lowAmmo > 0 && ammo < lowAmmo ? low : normal;

            Com_sprintf(s, sizeof(s), "%i", ammo);
            if (mode == 2) {
                tx -= HV_Width(s, 0.24f, 0);
            }
            HV_PaintText(tx, y + 13.0f, 0, 0.24f, c, s, 0, 0);
        }
        x += xStep;
        y += yStep;
    }
    trap_R_SetColor(NULL);
}

static void HV_PreviewFragMessage(void) {
    const char* s = "You fragged Visor.";
    float w, h, y;

    if (HV_Cvar("cg_drawFragMessages") == 0.0f) {
        return;
    }
    hvAnchor = WIDESCREEN_CENTER;
    y = (float)(int)((hvScene.warmup ? 144.0f : 125.0f) - 0.3f * 16.0f);
    HV_Measure(s, 0.3f, 0, 0, &w, &h);
    HV_PaintText((float)((640 - (int)w) / 2), y + h, 0, 0.3f, colorWhite, s, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

static void HV_PreviewGun(void) {
    int mode = (int)HV_Cvar("cg_drawGun");
    float fov = HV_Cvar("cg_fov"), x, fovY;
    const gitem_t* item = BG_FindItemForWeapon(WP_ROCKET_LAUNCHER);
    char path[MAX_QPATH];
    orientation_t tag;
    refEntity_t gun;
    refdef_t rd;
    qhandle_t hand;
    int i;

    if (mode <= 0 || !item->world_model[0]) {
        return;
    }
    fov = fov < 1.0f ? 1.0f : fov > 130.0f ? 130.0f : fov;
    memset(&rd, 0, sizeof(rd));
    rd.x = (int)hvX;
    rd.y = (int)hvY;
    rd.width = (int)hvW;
    rd.height = (int)hvH;
    x = 640.0f / (float)tan(fov / 360.0f * M_PI);
    fovY = (float)(atan2(480.0f, x) * 360.0 / M_PI);
    x = (float)rd.height / (float)tan(fovY / 360.0f * M_PI);
    rd.fov_x = (float)(atan2((float)rd.width, x) * 360.0 / M_PI);
    rd.fov_y = fovY;
    AxisClear(rd.viewaxis);
    rd.rdflags = RDF_NOWORLDMODEL;
    rd.time = uiInfo.uiDC.realTime;

    COM_StripExtension(item->world_model[0], path, sizeof(path));
    Q_strcat(path, sizeof(path), "_hand.md3");
    hand = trap_R_RegisterModel(path);
    trap_CM_LerpTag(&tag, hand, 0, 0, 1.0f, "tag_weapon");

    memset(&gun, 0, sizeof(gun));
    gun.origin[0] = HV_Cvar("cg_gunX");
    gun.origin[1] = HV_Cvar("cg_gunY");
    gun.origin[2] = HV_Cvar("cg_gunZ") + (fov > 90.0f ? -0.2f * (fov - 90.0f) : 0.0f);
    VectorCopy(gun.origin, gun.lightingOrigin);
    for (i = 0; i < 3; i++) {
        gun.origin[i] += tag.origin[i];
    }
    AxisCopy(tag.axis, gun.axis);
    gun.hModel = trap_R_RegisterModel(item->world_model[0]);
    gun.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;
    if (mode == 3) {
        gun.customShader = trap_R_RegisterShaderNoMip("ghostWeaponShader");
        gun.shaderRGBA[0] = gun.shaderRGBA[1] = gun.shaderRGBA[2] = gun.shaderRGBA[3] = 255;
    }
    trap_R_ClearScene();
    trap_R_AddRefEntityToScene(&gun);
    trap_R_RenderScene(&rd);
}

static void HV_PaintPreviews(void) {
    float y = 0.0f;

    HV_PreviewCrosshair();
    HV_PreviewWeaponBar();
    HV_PreviewLagometer();
    hvAnchor = WIDESCREEN_RIGHT;
    if (HV_Cvar("cg_drawFPS") != 0.0f) {
        y = HV_PreviewFPS(y);
    }
    HV_PreviewSpeedometer();
    HV_PreviewFragMessage();
    hvAnchor = WIDESCREEN_STRETCH;
}

void HV_Paint(void) {
    int i;

    if (!hvLoaded) {
        return;
    }
    hvNumBounds = 0;
    HV_PreviewGun();
    HV_Begin();
    for (i = 0; i < hvCount; i++) {
        Menu_Paint(&hvMenu[i], qfalse);
    }
    HV_PaintPreviews();
    HV_End();
}

void HV_PaintItem(itemDef_t* item) {
    int flags, odFlags, odFlags2, cvarFlags;

    if (!hvLoaded || !item) {
        return;
    }
    HV_Begin();
    flags = item->window.flags;
    odFlags = item->window.ownerDrawFlags;
    odFlags2 = item->window.ownerDrawFlags2;
    cvarFlags = item->cvarFlags;
    item->window.flags |= WINDOW_VISIBLE;
    item->window.ownerDrawFlags = 0;
    item->window.ownerDrawFlags2 = 0;
    item->cvarFlags &= ~(CVAR_SHOW | CVAR_HIDE);
    if (item->parent) {
        hvAnchor = ((menuDef_t*)item->parent)->widescreen;
    }
    Item_Paint(item);
    item->window.flags = flags;
    item->window.ownerDrawFlags = odFlags;
    item->window.ownerDrawFlags2 = odFlags2;
    item->cvarFlags = cvarFlags;
    HV_End();
}

qboolean HV_PaintLoose(itemDef_t* item, float* out) {
    float* b = hvBounds[HV_MAX_BOUNDS].b;

    b[0] = b[1] = b[2] = b[3] = 0.0f;
    hvBounds[HV_MAX_BOUNDS].item = item;
    hvBounds[HV_MAX_BOUNDS].shader = 0;
    hvBounds[HV_MAX_BOUNDS].text[0] = 0;
    hvLoose = qtrue;
    hvCur = HV_MAX_BOUNDS;
    HV_PaintItem(item);
    hvCur = -1;
    hvLoose = qfalse;
    if (!out || b[2] <= b[0] || b[3] <= b[1]) {
        return qfalse;
    }
    out[0] = b[0];
    out[1] = b[1];
    out[2] = b[2] - b[0];
    out[3] = b[3] - b[1];
    return qtrue;
}

int HV_ItemAnchor(const itemDef_t* item) {
    if (item->type == ITEM_TYPE_OWNERDRAW && item->window.ownerDraw == CG_AREA_NEW_CHAT) {
        return WIDESCREEN_LEFT;
    }
    if (item->widescreenFlag) {
        return item->widescreen;
    }
    return item->parent ? ((const menuDef_t*)item->parent)->widescreen : WIDESCREEN_STRETCH;
}

void HV_ItemScreenRect(itemDef_t* item, float* out) {
    float x = item->window.rect.x, y = item->window.rect.y;
    float w = item->window.rect.w, h = item->window.rect.h;

    if (w < 4.0f || h < 4.0f) {
        if (item->textRect.w > 0.0f && item->textRect.h > 0.0f) {
            x = item->textRect.x;
            y = item->textRect.y - item->textRect.h;
            w = item->textRect.w;
            h = item->textRect.h;
        }
        if (w < 4.0f) {
            w = 24.0f;
        }
        if (h < 4.0f) {
            y -= 12.0f;
            h = 16.0f;
        }
    }
    HV_ToScreen(HV_ItemAnchor(item), &x, &y, &w, &h);
    out[0] = x;
    out[1] = y;
    out[2] = w;
    out[3] = h;
}

qboolean HV_ItemPaintRect(const itemDef_t* item, float* out) {
    return HV_PaintedRect(item, out);
}

void HV_ItemHitRect(itemDef_t* item, float* out) {
    float p[4], x1, y1;

    HV_ItemScreenRect(item, out);
    if (!HV_PaintedRect(item, p)) {
        return;
    }
    x1 = out[0] + out[2] > p[0] + p[2] ? out[0] + out[2] : p[0] + p[2];
    y1 = out[1] + out[3] > p[1] + p[3] ? out[1] + out[3] : p[1] + p[3];
    out[0] = out[0] < p[0] ? out[0] : p[0];
    out[1] = out[1] < p[1] ? out[1] : p[1];
    out[2] = x1 - out[0];
    out[3] = y1 - out[1];
}

static qboolean HV_CvarShown(itemDef_t* item) {
    qboolean shown;

    if (!(item->cvarFlags & (CVAR_SHOW | CVAR_HIDE))) {
        return qtrue;
    }
    if (Display_GetContext() == &hvDC) {
        return Item_EnableShowViaCvar(item, CVAR_SHOW);
    }
    HV_Begin();
    shown = Item_EnableShowViaCvar(item, CVAR_SHOW);
    HV_End();
    return shown;
}

qboolean HV_ItemShown(const itemDef_t* item) {
    const menuDef_t* menu = (const menuDef_t*)item->parent;

    if (!(item->window.flags & WINDOW_VISIBLE)) {
        return qfalse;
    }
    if (menu && !(menu->window.flags & WINDOW_VISIBLE)) {
        return qfalse;
    }
    if (menu && (menu->window.ownerDrawFlags || menu->window.ownerDrawFlags2) &&
        !HV_OwnerDrawVisible(menu->window.ownerDrawFlags, menu->window.ownerDrawFlags2)) {
        return qfalse;
    }
    if ((item->window.ownerDrawFlags || item->window.ownerDrawFlags2) &&
        !HV_OwnerDrawVisible(item->window.ownerDrawFlags, item->window.ownerDrawFlags2)) {
        return qfalse;
    }
    return HV_CvarShown((itemDef_t*)item);
}

typedef struct {
    int id;
    const char* name;
} hvName_t;

static const hvName_t hvNames[] = {
    {CG_PLAYER_HEALTH, "Health"},
    {CG_PLAYER_ARMOR_VALUE, "Armor"},
    {CG_PLAYER_AMMO_VALUE, "Ammo"},
    {CG_PLAYER_ARMOR_ICON, "Armor icon"},
    {CG_PLAYER_ARMOR_ICON2D, "Armor icon"},
    {CG_PLAYER_AMMO_ICON, "Ammo icon"},
    {CG_PLAYER_AMMO_ICON2D, "Ammo icon"},
    {CG_PLAYER_ITEM, "Holdable item"},
    {CG_PLAYER_HEAD, "Player head"},
    {CG_PLAYER_SCORE, "Your score"},
    {CG_1STPLACE, "First place score"},
    {CG_2NDPLACE, "Second place score"},
    {CG_1ST_PLACE_SCORE, "First place score"},
    {CG_2ND_PLACE_SCORE, "Second place score"},
    {CG_RED_SCORE, "Red score"},
    {CG_BLUE_SCORE, "Blue score"},
    {CG_RED_NAME, "Red team name"},
    {CG_BLUE_NAME, "Blue team name"},
    {CG_LEVELTIMER, "Match timer"},
    {CG_ROUNDTIMER, "Round timer"},
    {CG_ROUND, "Round"},
    {CG_CAPFRAGLIMIT, "Score limit"},
    {CG_LOCALTIME, "Local time"},
    {CG_MAP_NAME, "Map name"},
    {CG_GAME_TYPE, "Game type"},
    {CG_GAME_TYPE_ICON, "Game type icon"},
    {CG_PLAYER_COUNTS, "Player count"},
    {CG_RED_PLAYER_COUNT, "Red players"},
    {CG_BLUE_PLAYER_COUNT, "Blue players"},
    {CG_RED_AVG_PING, "Red ping"},
    {CG_BLUE_AVG_PING, "Blue ping"},
    {CG_RED_CLAN_PLYRS, "Red alive"},
    {CG_BLUE_CLAN_PLYRS, "Blue alive"},
    {CG_TEAM_PLYR_COUNT, "Team alive"},
    {CG_ENEMY_PLYR_COUNT, "Enemies alive"},
    {CG_RED_TIMEOUT_COUNT, "Red timeouts"},
    {CG_BLUE_TIMEOUT_COUNT, "Blue timeouts"},
    {CG_TEAM_COLORIZED, "Team colored image"},
    {CG_ARMORTIERED_COLORIZED, "Armor colored image"},
    {CG_HEALTH_COLORIZED, "Health colored image"},
    {CG_TEAM_COLOR, "Team color"},
    {CG_SELECTED_PLYR_TEAM_COLOR, "Team color"},
    {CG_PLAYER_HEALTH_BAR_100, "Health bar"},
    {CG_PLAYER_HEALTH_BAR_200, "Health bar 200"},
    {CG_PLAYER_ARMOR_BAR_100, "Armor bar"},
    {CG_PLAYER_ARMOR_BAR_200, "Armor bar 200"},
    {CG_PLAYER_OBIT, "Kill feed"},
    {CG_AREA_NEW_CHAT, "Chat"},
    {CG_AREA_POWERUP, "Powerups"},
    {CG_CTF_POWERUP, "Rune"},
    {CG_PLAYER_HASFLAG, "Carried flag"},
    {CG_PLAYER_HASFLAG2D, "Carried flag"},
    {CG_RED_FLAGSTATUS, "Red flag"},
    {CG_BLUE_FLAGSTATUS, "Blue flag"},
    {CG_RED_BASESTATUS, "Red base"},
    {CG_BLUE_BASESTATUS, "Blue base"},
    {CG_ONEFLAG_STATUS, "Flag"},
    {CG_SPECTATORS, "Spectators"},
    {CG_KILLER, "Fragged by"},
    {CG_FOLLOW_PLAYER_NAME, "Followed player"},
    {CG_FOLLOW_PLAYER_NAME_EX, "Followed player"},
    {CG_ACCURACY, "Accuracy"},
    {CG_IMPRESSIVE, "Impressive"},
    {CG_EXCELLENT, "Excellent"},
    {CG_GAUNTLET, "Humiliation"},
    {CG_RED_OWNED_FLAGS, "Red points"},
    {CG_BLUE_OWNED_FLAGS, "Blue points"},
    {CG_HARVESTER_SKULLS, "Skulls"},
    {CG_PLAYER_HASKEY, "Keys"},
    {CG_SPEC_MESSAGES, "Spectator help"},
    {CG_RACE_STATUS, "Race status"},
    {CG_RACE_TIMES, "Race times"},
    {CG_OVERTIME, "Overtime"},
    {CG_MATCH_STATE, "Match state"},
    {UI_ADVERT, "Advert"},
};

const char* HV_OwnerDrawName(int ownerDraw) {
    int i;

    for (i = 0; i < (int)ARRAY_LEN(hvNames); i++) {
        if (hvNames[i].id == ownerDraw) {
            return hvNames[i].name;
        }
    }
    return NULL;
}

const char* HV_ItemKind(const itemDef_t* item) {
    const char* od;

    if (item->window.ownerDraw && (od = HV_OwnerDrawName(item->window.ownerDraw)) != NULL) {
        return od;
    }
    if (item->text && item->text[0]) {
        return "Text";
    }
    if (item->window.background) {
        return "Picture";
    }
    if (item->window.ownerDraw) {
        return "Game element";
    }
    if (item->window.border && item->window.style != WINDOW_STYLE_FILLED &&
        item->window.style != WINDOW_STYLE_GRADIENT) {
        return "Frame";
    }
    return "Box";
}

qboolean HV_ItemInvisible(const itemDef_t* item) {
    qboolean back = item->window.style == WINDOW_STYLE_FILLED || item->window.style == WINDOW_STYLE_GRADIENT;

    if (item->window.ownerDraw || (item->text && item->text[0]) || item->window.background) {
        return qfalse;
    }
    if (back && item->window.backColor[3] > 0.001f) {
        return qfalse;
    }
    if (item->window.border && item->window.borderSize > 0.0f && item->window.borderColor[3] > 0.001f) {
        return qfalse;
    }
    return qtrue;
}

static const struct {
    int flag;
    const char* text;
} hvWhen[] = {
    {CG_SHOW_CTF, "CTF"},
    {CG_SHOW_DUEL, "Duel"},
    {CG_SHOW_CLAN_ARENA, "CA"},
    {CG_SHOW_ONEFLAG, "1-Flag"},
    {CG_SHOW_HARVESTER, "Harvester"},
    {CG_SHOW_DOMINATION, "Domination"},
    {CG_SHOW_ANYNONTEAMGAME, "FFA and Duel"},
    {CG_SHOW_ANYTEAMGAME, "Team games"},
    {CG_SHOW_PLAYERS_REMAINING, "Round games"},
    {CG_SHOW_YOURTEAMHASENEMYFLAG, "When your team has the enemy flag"},
    {CG_SHOW_OTHERTEAMHASFLAG, "When the enemy has your flag"},
    {CG_SHOW_IF_PLAYER_HAS_FLAG, "Carrying the flag"},
    {CG_SHOW_IF_PLYR_IS_FIRST_PLACE, "When you lead"},
    {CG_SHOW_IF_PLYR_IS_NOT_FIRST_PLACE, "When you trail"},
    {CG_SHOW_IF_RED_IS_FIRST_PLACE, "Red leads"},
    {CG_SHOW_IF_BLUE_IS_FIRST_PLACE, "Blue leads"},
    {CG_SHOW_IF_CHAT_VISIBLE, "Chat open"},
    {CG_SHOW_HEALTHCRITICAL, "Low health"},
    {CG_SHOW_IF_WARMUP, "Warmup"},
    {CG_SHOW_IF_PLYR_IS_ON_RED, "On red"},
    {CG_SHOW_IF_PLYR_IS_ON_BLUE, "On blue"},
    {CG_SHOW_IF_MSG_PRESENT, "With a message"},
    {CG_SHOW_IF_NOTICE_PRESENT, "With a message"},
};

static void HV_WhenAdd(char* out, int size, const char* text) {
    if (out[0]) {
        Q_strcat(out, size, ", ");
    }
    Q_strcat(out, size, text);
}

const char* HV_ItemWhen(const itemDef_t* item) {
    static const char* modes[] = {"FFA", "Duel", "Race", "TDM", "CA", "CTF", "1-Flag", "Overload",
                                  "Harvester", "FT", "Domination", "A&D", "RR"};
    static char out[96];
    const menuDef_t* menu = (const menuDef_t*)item->parent;
    int flags = item->window.ownerDrawFlags | (menu ? menu->window.ownerDrawFlags : 0);
    int i, shown = 0;

    out[0] = 0;
    for (i = 0; i < (int)ARRAY_LEN(hvWhen) && shown < 3; i++) {
        if (flags & hvWhen[i].flag) {
            HV_WhenAdd(out, sizeof(out), hvWhen[i].text);
            shown++;
        }
    }
    if ((item->cvarFlags & (CVAR_SHOW | CVAR_HIDE)) && item->cvarTest && item->enableCvar &&
        !Q_stricmp(item->cvarTest, "cg_gametype")) {
        char script[256], names[64];
        char* p = script;
        const char* val;

        Q_strncpyz(script, item->enableCvar, sizeof(script));
        names[0] = 0;
        while (String_Parse(&p, &val)) {
            int n = atoi(val);

            if (val[0] >= '0' && val[0] <= '9' && n < (int)ARRAY_LEN(modes)) {
                HV_WhenAdd(names, sizeof(names), modes[n]);
            }
        }
        if (names[0]) {
            HV_WhenAdd(out, sizeof(out), item->cvarFlags & CVAR_SHOW ? names : va("Not in %s", names));
        }
    }
    return out;
}

const char* HV_ItemLabel(const itemDef_t* item) {
    static char out[2][64];
    static int which;
    char* o = out[which ^= 1];

    if (!item->window.ownerDraw && (item->window.ownerDrawFlags & CG_SHOW_YOURTEAMHASENEMYFLAG)) {
        Q_strncpyz(o, "You have the flag", sizeof(out[0]));
    } else if (!item->window.ownerDraw && (item->window.ownerDrawFlags & CG_SHOW_OTHERTEAMHASFLAG)) {
        Q_strncpyz(o, "They have your flag", sizeof(out[0]));
    } else {
        o[0] = 0;
    }
    if (o[0]) {
        return o;
    }
    if (item->text && item->text[0] && !item->window.ownerDraw) {
        Q_strncpyz(o, item->text, sizeof(out[0]));
        Q_CleanStr(o);
        if (o[0]) {
            return o;
        }
    }
    Q_strncpyz(o, HV_ItemKind(item), sizeof(out[0]));
    return o;
}
