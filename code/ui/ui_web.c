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
#include "ui_web.h"

const vec4_t smPage = SM_RGB(0, 0, 0);
const vec4_t smPanel = {0.0f, 0.0f, 0.0f, 0.8f};
const vec4_t smRaised = SM_RGB(15, 15, 15);
const vec4_t smRaisedHover = SM_RGB(30, 30, 30);
const vec4_t smBorder = SM_RGB(51, 51, 51);
const vec4_t smField = SM_RGB(34, 34, 34);
const vec4_t smText = SM_RGB(221, 221, 221);
const vec4_t smTextSoft = SM_RGB(204, 204, 204);
const vec4_t smMuted = SM_RGB(153, 153, 153);
const vec4_t smSubtle = SM_RGB(102, 102, 102);
const vec4_t smPrimary = SM_RGB(247, 209, 0);
const vec4_t smOnPrimary = SM_RGB(0, 0, 0);
const vec4_t smGold = SM_RGB(247, 209, 0);
const vec4_t smRing = SM_RGB(247, 209, 0);
const vec4_t smDanger = SM_RGB(204, 51, 51);
const vec4_t smSuccess = SM_RGB(32, 181, 51);
const vec4_t smShadow = {0.0f, 0.0f, 0.0f, 0.48f};

smLook_t smLook;
float smScale;
float smTop;
float smVW;
float smBottom;
float smMX, smMY;
static float smSpacing;

void SM_Frame(void) {
    float w = (float)uiInfo.uiDC.glconfig.vidWidth;
    float h = (float)uiInfo.uiDC.glconfig.vidHeight;

    smSpacing = 0;
    smScale = h / SM_VIEW_H;
    smVW = w / smScale;
    smTop = (h - SM_VIEW_H * smScale) * 0.5f;
    smBottom = (h - smTop) / smScale;
    smMX = (uiInfo.uiDC.cursorx * uiInfo.uiDC.xscale + uiInfo.uiDC.bias) / smScale;
    smMY = (uiInfo.uiDC.cursory * uiInfo.uiDC.yscale - smTop) / smScale;
    SM_LoadFonts();
}

smRect_t SM_R(float x, float y, float w, float h) {
    smRect_t r;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

qboolean SM_In(const smRect_t* r, float x, float y) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

qboolean SM_Clip(smRect_t* r, const smRect_t* clip) {
    float x1, y1, x2, y2;

    if (r->w <= 0 || r->h <= 0) {
        return qfalse;
    }
    if (!clip) {
        return qtrue;
    }
    x1 = r->x > clip->x ? r->x : clip->x;
    y1 = r->y > clip->y ? r->y : clip->y;
    x2 = r->x + r->w < clip->x + clip->w ? r->x + r->w : clip->x + clip->w;
    y2 = r->y + r->h < clip->y + clip->h ? r->y + r->h : clip->y + clip->h;
    if (x2 <= x1 || y2 <= y1) {
        return qfalse;
    }
    *r = SM_R(x1, y1, x2 - x1, y2 - y1);
    return qtrue;
}

void SM_Fill(float x, float y, float w, float h, const float* color, const smRect_t* clip) {
    smRect_t r = SM_R(x, y, w, h);

    if (!SM_Clip(&r, clip)) {
        return;
    }
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(r.x * smScale, smTop + r.y * smScale, r.w * smScale, r.h * smScale, 0, 0, 0, 0,
                          uiInfo.uiDC.whiteShader);
    trap_R_SetColor(NULL);
}

void SM_Round(float x, float y, float w, float h, float r, const float* color, const smRect_t* clip) {
    (void)r;
    SM_Fill(x, y, w, h, color, clip);
}

void SM_Pill(float x, float y, float w, float h, const float* color, const smRect_t* clip) {
    SM_Round(x, y, w, h, h * 0.5f, color, clip);
}

void SM_Shadow(float x, float y, float w, float h, float r) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)r;
}

void SM_FillScreen(const float* color) {
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(0, 0, (float)uiInfo.uiDC.glconfig.vidWidth, (float)uiInfo.uiDC.glconfig.vidHeight, 0, 0, 0, 0,
                          uiInfo.uiDC.whiteShader);
    trap_R_SetColor(NULL);
}

void SM_Pic(float x, float y, float w, float h, qhandle_t shader, const smRect_t* clip) {
    smRect_t r = SM_R(x, y, w, h);

    if (!shader || !SM_Clip(&r, clip)) {
        return;
    }
    trap_R_DrawStretchPic(r.x * smScale, smTop + r.y * smScale, r.w * smScale, r.h * smScale, (r.x - x) / w,
                          (r.y - y) / h, (r.x + r.w - x) / w, (r.y + r.h - y) / h, shader);
}

void SM_PicColor(float x, float y, float w, float h, qhandle_t shader, const float* color,
                 const smRect_t* clip) {
    trap_R_SetColor(color);
    SM_Pic(x, y, w, h, shader, clip);
    trap_R_SetColor(NULL);
}

void SM_Arrow(float x, float y, qboolean up) {
    int k;

    for (k = 0; k < 5; k++) {
        float w = up ? 2.0f * (k + 1) : 10.0f - 2.0f * k;

        SM_Fill(x + (10.0f - w) * 0.5f, y + k, w, 1, smMuted, NULL);
    }
}

void SM_Chevron(float x, float y, qboolean up, const float* color) {
    int k;

    for (k = 0; k < 5; k++) {
        float yy = up ? y + 4 - k : y + k;

        SM_Fill(x + k, yy, 1.6f, 1.6f, color, NULL);
        SM_Fill(x + 8.4f - k, yy, 1.6f, 1.6f, color, NULL);
    }
}

void SM_Cross(float x, float y, float size, const float* color) {
    int n = (int)size;
    int k;

    for (k = 0; k < n; k++) {
        SM_Fill(x + k, y + k, 1.8f, 1.8f, color, NULL);
        SM_Fill(x + size - 1 - k, y + k, 1.8f, 1.8f, color, NULL);
    }
}

void SM_To640(float x, float y, float w, float h, float* out) {
    float xscale = uiInfo.uiDC.xscale;
    float yscale = uiInfo.uiDC.yscale;

    if (xscale <= 0 || yscale <= 0) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    out[0] = (x * smScale - uiInfo.uiDC.bias) / xscale;
    out[1] = (smTop + y * smScale) / yscale;
    out[2] = w * smScale / xscale;
    out[3] = h * smScale / yscale;
}

void SM_DrawPlayerIn(playerInfo_t* pi, float x, float y, float w, float h) {
    float viewW = h * 0.86f / 0.54f;
    float r[4];

    SM_To640(x + (w - viewW) * 0.5f, y, viewW, h, r);
    UI_DrawPlayer(r[0], r[1], r[2], r[3], pi, uiInfo.uiDC.realTime / 2);
}

void SM_TurnPlayer(playerInfo_t* pi, float yaw) {
    pi->viewAngles[YAW] = yaw;
    pi->legs.yawAngle = yaw;
    pi->torso.yawAngle = yaw;
    pi->legs.yawing = qfalse;
    pi->torso.yawing = qfalse;
}

void SM_BuildPlayer(playerInfo_t* pi, const char* composed, float yaw, int weapon) {
    vec3_t viewangles;

    memset(pi, 0, sizeof(*pi));
    viewangles[YAW] = yaw;
    viewangles[PITCH] = 0;
    viewangles[ROLL] = 0;
    UI_PlayerInfo_SetModel(pi, (char*)composed, (char*)composed, "");
    UI_PlayerInfo_SetInfo(pi, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, weapon, qfalse);
}

static struct {
    qboolean tried;
    int regular, strong;
    int sans, sansBold, handel, handelBold, awesome;
    int sansItalic;
    int arial;
} smFonts = {qfalse, -1, -1, -1, -1, -1, -1, -1, -1, -1};

void SM_SetLook(smLook_t look) {
    smLook = look;
}

void SM_LoadFonts(void) {
    if (smFonts.tried) {
        return;
    }
    smFonts.tried = qtrue;
    smFonts.regular = trap_R_Font_Register("fonts/Barlow-Regular.ttf");
    smFonts.strong = trap_R_Font_Register("fonts/Barlow-Bold.ttf");
    if (smFonts.regular < 0 || smFonts.strong < 0) {
        Com_Printf("^3front end: Barlow not found in pak01, falling back to pak00's fonts\n");
        smFonts.regular = -1;
        smFonts.strong = -1;
    }

    smFonts.sans = trap_R_Font_Register("webui/fonts/NotoSans-Regular.ttf");
    smFonts.sansBold = trap_R_Font_Register("webui/fonts/NotoSans-Bold.ttf");
    smFonts.handel = trap_R_Font_Register("webui/fonts/handelgothic-medium-webfont.ttf");
    smFonts.handelBold = trap_R_Font_Register("webui/fonts/handelgothic-bold-webfont.ttf");
    smFonts.awesome = trap_R_Font_Register("webui/fonts/fontawesome-webfont.ttf");
    smFonts.sansItalic = trap_R_Font_Register("webui/fonts/NotoSans-BoldItalic.ttf");
    smFonts.arial = trap_R_Font_Register("fonts/LiberationSans-Regular.ttf");
    if (smFonts.sans < 0 || smFonts.sansBold < 0 || smFonts.handel < 0 || smFonts.handelBold < 0) {
        Com_Printf("^3front end: no web.pak, so pak00's fonts stand in for retail's web UI\n");
        smFonts.sans = smFonts.sansBold = smFonts.handel = smFonts.handelBold = -1;
    }
}

static int SM_Face(int font, qboolean bold) {
    if (font != FONT_SANS && font != FONT_DEFAULT) {
        return font;
    }
    if (smLook == SM_LOOK_OURS && smFonts.regular >= 0) {
        return (bold || font == FONT_DEFAULT) ? smFonts.strong : smFonts.regular;
    }
    if (smLook == SM_LOOK_RETAIL && smFonts.sans >= 0) {
        if (font == FONT_DEFAULT) {
            return bold ? smFonts.handelBold : smFonts.handel;
        }
        return bold ? smFonts.sansBold : smFonts.sans;
    }
    return font;
}

static qboolean SM_HasBold(int face) {
    return face != FONT_SANS && face != FONT_DEFAULT;
}

static qboolean SM_IsWebHandel(int face) {
    return face >= 0 && (face == smFonts.handel || face == smFonts.handelBold);
}

static qboolean SM_IsBarlow(int face) {
    return face >= 0 && (face == smFonts.regular || face == smFonts.strong);
}

static float SM_FaceEm(int face) {
    if (face >= 0 && face == smFonts.arial) {
        return SM_ARIAL_EM;
    }
    if (SM_IsBarlow(face)) {
        return SM_BARLOW_EM;
    }
    if (SM_IsWebHandel(face)) {
        return face == smFonts.handelBold ? SM_WEB_HANDEL_BOLD_EM : SM_WEB_HANDEL_EM;
    }
    return face == FONT_DEFAULT ? SM_HANDEL_EM : SM_SANS_EM;
}

int SM_ItalicFace(void) {
    return smFonts.sansItalic >= 0 ? smFonts.sansItalic : FONT_SANS;
}

float SM_Em(int font) {
    return SM_FaceEm(SM_Face(font, qfalse));
}

float SM_Baseline(int font, float size, float lineTop, float lineHeight) {
    int face = SM_Face(font, qfalse);
    float base;

    if (face >= 0 && face == smFonts.arial) {
        base = SM_ARIAL_BASE;
    } else if (SM_IsBarlow(face)) {
        base = SM_BARLOW_BASE;
    } else if (SM_IsWebHandel(face)) {
        base = face == smFonts.handelBold ? SM_WEB_HANDEL_BOLD_BASE : SM_WEB_HANDEL_BASE;
    } else {
        base = face == FONT_DEFAULT ? SM_HANDEL_BASE : SM_SANS_BASE;
    }
    return lineTop + lineHeight * 0.5f + size * base;
}

qboolean SM_TextInside(float baseline, float size, const smRect_t* clip) {
    return baseline - size * 1.07f >= clip->y - 1.0f && baseline + size * 0.3f <= clip->y + clip->h + 1.0f;
}

void SM_SetSpacing(float spacing) {
    smSpacing = spacing;
}

static int SM_CharBytes(const char* s) {
    unsigned char c = (unsigned char)*s;

    return c < 0xc0 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
}

static int SM_CountChars(const char* text, int len) {
    int n = 0;
    int i = 0;

    while (i < len && text[i]) {
        i += SM_CharBytes(text + i);
        n++;
    }
    return n;
}

static float SM_AdvanceFace(int face, float size, const char* text, int len) {
    char buf[MAX_STRING_CHARS];
    float withBar[5], bar[5];
    float px = size * SM_FaceEm(face) * smScale;

    if (len < 0) {
        len = (int)strlen(text);
    }
    if (len <= 0) {
        return 0;
    }
    if (len > (int)sizeof(buf) - 2) {
        len = sizeof(buf) - 2;
    }
    memcpy(buf, text, len);
    buf[len] = '|';
    buf[len + 1] = '\0';
    trap_R_Font_TextExtents(buf, 0, face, px, -1, withBar);
    trap_R_Font_TextExtents("|", 0, face, px, -1, bar);
    return (withBar[2] - bar[2]) / smScale + smSpacing * SM_CountChars(text, len);
}

static void SM_DrawFace(float x, float baseline, int face, float size, const float* color, const char* text, int limit,
                        qboolean bold, int flags) {
    float px = size * SM_FaceEm(face) * smScale;
    int sy = (int)(smTop + baseline * smScale + 0.5f);
    int shift = (int)(smScale + 0.5f);
    int len = (int)strlen(text);
    int i = 0;

    trap_R_SetColor(color);
    if (!smSpacing) {
        int sx = (int)(x * smScale + 0.5f);

        trap_R_Font_DrawString(sx, sy, text, face, px, limit, NULL, flags);
        if (bold && !SM_HasBold(face)) {
            trap_R_Font_DrawString(sx + (shift < 1 ? 1 : shift), sy, text, face, px, limit, NULL, flags);
        }
        trap_R_SetColor(NULL);
        return;
    }
    if (limit >= 0 && limit < len) {
        len = limit;
    }
    while (i < len) {
        int n = SM_CharBytes(text + i);
        int sx = (int)(x * smScale + 0.5f);
        float spacing = smSpacing;

        if (i + n > len) {
            n = len - i;
        }
        trap_R_Font_DrawString(sx, sy, text + i, face, px, n, NULL, flags);
        if (bold && !SM_HasBold(face)) {
            trap_R_Font_DrawString(sx + (shift < 1 ? 1 : shift), sy, text + i, face, px, n, NULL, flags);
        }
        smSpacing = 0;
        x += SM_AdvanceFace(face, size, text + i, n) + spacing;
        smSpacing = spacing;
        i += n;
    }
    trap_R_SetColor(NULL);
}

float SM_Advance(int font, float size, const char* text, int len) {
    return SM_AdvanceFace(SM_Face(font, qfalse), size, text, len);
}

float SM_AdvanceBold(int font, float size, const char* text, int len, qboolean bold) {
    return SM_AdvanceFace(SM_Face(font, bold), size, text, len);
}

int SM_Fit(int font, float size, const char* text, float width) {
    int len = (int)strlen(text);
    int n;

    if (SM_Advance(font, size, text, len) <= width) {
        return len;
    }
    for (n = 1; n < len; n++) {
        if (SM_Advance(font, size, text, n) > width) {
            return n - 1;
        }
    }
    return len;
}

void SM_Text(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
             qboolean bold) {
    if (!text || !text[0] || limit == 0) {
        return;
    }
    SM_DrawFace(x, baseline, SM_Face(font, bold), size, color, text, limit, bold, 0);
}

const vec4_t smWebButton = SM_RGB(61, 61, 61);
static const vec4_t smWebButtonBorder = SM_RGB(77, 77, 77);
const vec4_t smWebButtonHover = SM_RGB(93, 93, 93);
static const vec4_t smWebButtonHoverBorder = SM_RGB(153, 153, 153);
static const vec4_t smWebButtonOff = SM_RGB(34, 34, 34);
static const vec4_t smWebButtonOffBorder = SM_RGB(17, 17, 17);
static const vec4_t smWebButtonOffText = SM_RGB(51, 51, 51);
const vec4_t smWebYellow = SM_RGB(247, 209, 0);

void SM_Border(float x, float y, float w, float h, float t, const float* color, const smRect_t* clip) {
    SM_Fill(x, y, w, t, color, clip);
    SM_Fill(x, y + h - t, w, t, color, clip);
    SM_Fill(x, y + t, t, h - 2 * t, color, clip);
    SM_Fill(x + w - t, y + t, t, h - 2 * t, color, clip);
}

float SM_WebButtonWidth(const char* label) {
    char upper[64];

    Q_strncpyz(upper, label, sizeof(upper));
    Q_strupr(upper);
    return SM_SpacedWidth(FONT_SANS, 11, upper, 1.0f) + 40 + 2;
}

void SM_WebButton(float x, float y, float w, const char* label, qboolean disabled, qboolean over,
                  const smRect_t* clip) {
    SM_WebButtonPadded(x, y, w, 20, 20, label, disabled, over, clip);
}

void SM_WebButtonPadded(float x, float y, float w, float padLeft, float padRight, const char* label,
                        qboolean disabled, qboolean over, const smRect_t* clip) {
    char upper[64];
    float baseline = SM_Baseline(FONT_SANS, 11, y, SM_WEB_BUTTON_H);
    float room = w - 2 - padLeft - padRight;
    float tw;

    Q_strncpyz(upper, label, sizeof(upper));
    Q_strupr(upper);
    if (disabled) {
        over = qfalse;
    }
    SM_Fill(x, y, w, SM_WEB_BUTTON_H, disabled ? smWebButtonOff : over ? smWebButtonHover : smWebButton, clip);
    SM_Border(x, y, w, SM_WEB_BUTTON_H, 1,
              disabled ? smWebButtonOffBorder : over ? smWebButtonHoverBorder : smWebButtonBorder, clip);
    tw = SM_SpacedWidth(FONT_SANS, 11, upper, 1.0f);
    if (!clip || SM_TextInside(baseline, 11, clip)) {
        SM_SpacedText(x + 1 + padLeft + (tw < room ? (room - tw) * 0.5f : 0), baseline, FONT_SANS, 11,
                      disabled ? smWebButtonOffText : over ? smWebYellow : smText, upper, 1.0f, qtrue);
    }
}

int SM_SansSerifFace(void) {
    return smFonts.arial >= 0 ? smFonts.arial : FONT_SANS;
}

qboolean SM_Narrow(void) {
    return (qboolean)(uiInfo.uiDC.glconfig.vidWidth * 6 <= uiInfo.uiDC.glconfig.vidHeight * 9);
}

float SM_AdvanceRaw(int font, float size, const char* text, int len, qboolean bold) {
    int face = SM_Face(font, bold);
    float w = 0;
    int start = 0;
    int i;

    if (len < 0) {
        len = (int)strlen(text);
    }
    for (i = 0; i <= len; i++) {
        if (i == len || text[i] == '^') {
            if (i > start) {
                w += SM_AdvanceFace(face, size, text + start, i - start);
            }
            if (i < len) {
                w += SM_AdvanceFace(face, size, "^", 1);
            }
            start = i + 1;
        }
    }
    return w;
}

void SM_TextRaw(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
                qboolean bold) {
    if (!text || !text[0] || limit == 0) {
        return;
    }
    SM_DrawFace(x, baseline, SM_Face(font, bold), size, color, text, limit, bold, TEXT_LITERAL);
}

static int SM_Utf8(int cp, char* out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        out[1] = 0;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        out[2] = 0;
        return 2;
    }
    out[0] = (char)(0xe0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[2] = (char)(0x80 | (cp & 0x3f));
    out[3] = 0;
    return 3;
}

void SM_Icon(float x, float baseline, float size, const float* color, int codepoint) {
    char glyph[4];

    if (smFonts.awesome < 0) {
        return;
    }
    SM_Utf8(codepoint, glyph);
    trap_R_SetColor(color);
    trap_R_Font_DrawString((int)(x * smScale + 0.5f), (int)(smTop + baseline * smScale + 0.5f), glyph,
                           smFonts.awesome, size * SM_AWESOME_EM * smScale, -1, NULL, 0);
    trap_R_SetColor(NULL);
}

float SM_IconWidth(float size, int codepoint) {
    char glyph[4];

    if (smFonts.awesome < 0) {
        return 0;
    }
    SM_Utf8(codepoint, glyph);
    return SM_AdvanceFace(smFonts.awesome, size * SM_AWESOME_EM / SM_FaceEm(smFonts.awesome), glyph, -1);
}

void SM_Centered(float middle, float baseline, float size, const float* color, const char* text, qboolean bold) {
    SM_Text(middle - SM_AdvanceBold(FONT_SANS, size, text, -1, bold) * 0.5f, baseline, FONT_SANS, size, color, text, -1,
            bold);
}

float SM_SpacedWidth(int font, float size, const char* text, float spacing) {
    float w = 0;

    for (; *text; text++) {
        w += SM_Advance(font, size, text, 1) + spacing;
    }
    return w;
}

void SM_SpacedText(float x, float baseline, int font, float size, const float* color, const char* text, float spacing,
                   qboolean bold) {
    char c[2];

    c[1] = '\0';
    for (; *text; text++) {
        c[0] = *text;
        SM_Text(x, baseline, font, size, color, c, -1, bold);
        x += SM_AdvanceBold(font, size, c, 1, bold) + spacing;
    }
}

static void SM_Upper(const char* text, char* out, int size) {
    int i;

    for (i = 0; text[i] && i < size - 1; i++) {
        char c = text[i];

        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[i] = '\0';
}

float SM_LabelWidth(float size, const char* text, float spacing) {
    char upper[256];
    char c[2];
    const char* p;
    float w = 0;

    SM_Upper(text, upper, sizeof(upper));
    c[1] = '\0';
    for (p = upper; *p; p++) {
        c[0] = *p;
        w += SM_AdvanceBold(SM_FONT_TITLE, size, c, 1, qfalse) + spacing;
    }
    return w - spacing;
}

void SM_Label(float x, float baseline, float size, const float* color, const char* text, float spacing) {
    char upper[256];

    SM_Upper(text, upper, sizeof(upper));
    if (spacing <= 0) {
        SM_Text(x, baseline, SM_FONT_TITLE, size, color, upper, -1, qfalse);
        return;
    }
    SM_SpacedText(x, baseline, SM_FONT_TITLE, size, color, upper, spacing, qfalse);
}

int SM_Wrap(int font, float size, const char* text, float width, int* starts, int* lens) {
    int total = (int)strlen(text);
    int pos = 0, lines = 0;

    if (SM_Advance(font, size, text, total) <= width) {
        starts[0] = 0;
        lens[0] = total;
        return total ? 1 : 0;
    }
    while (pos < total && lines < SM_MAX_LINES) {
        int end, fit = -1;

        while (pos < total && text[pos] == ' ') {
            pos++;
        }
        if (pos >= total) {
            break;
        }
        end = pos;
        while (end < total) {
            int next = end;

            while (next < total && text[next] == ' ') {
                next++;
            }
            while (next < total && text[next] != ' ') {
                next++;
            }
            if (fit >= 0 && SM_Advance(font, size, text + pos, next - pos) > width) {
                break;
            }
            fit = next;
            end = next;
        }
        starts[lines] = pos;
        lens[lines] = fit - pos;
        lines++;
        pos = fit;
    }
    return lines;
}

#define SM_BUTTON_SPACING 1.0f

static float SM_ButtonText(float h) {
    return h >= SM_BUTTON_H + 4.0f ? 13.0f : 12.0f;
}

static void SM_Upper(const char* text, char* out, int size);

float SM_ButtonWidth(const char* text, float height) {
    char upper[256];

    SM_Upper(text, upper, sizeof(upper));
    return SM_SpacedWidth(SM_FONT_TITLE, SM_ButtonText(height), upper, SM_BUTTON_SPACING) +
           (height >= SM_BUTTON_H ? 44.0f : 28.0f);
}

void SM_Button(float x, float y, float w, float h, const char* text, int style, qboolean over) {
    float size = SM_ButtonText(h);
    char upper[256];
    const float* fg;
    vec4_t hover;

    SM_Upper(text, upper, sizeof(upper));

    switch (style) {
        case SM_BUTTON_PRIMARY:
            hover[0] = smPrimary[0] * 0.88f;
            hover[1] = smPrimary[1] * 0.88f;
            hover[2] = smPrimary[2] * 0.88f;
            hover[3] = 1.0f;
            SM_Fill(x, y, w, h, over ? hover : smPrimary, NULL);
            fg = smOnPrimary;
            break;
        case SM_BUTTON_DANGER:
            fg = over ? smDanger : smText;
            break;
        case SM_BUTTON_SECONDARY:
        default:
            fg = over ? smGold : smText;
            break;
    }
    SM_SpacedText(x + w * 0.5f - SM_SpacedWidth(SM_FONT_TITLE, size, upper, SM_BUTTON_SPACING) * 0.5f,
                  SM_Baseline(SM_FONT_TITLE, size, y, h), SM_FONT_TITLE, size, fg, upper, SM_BUTTON_SPACING, qfalse);
}

void SM_Switch(float x, float y, qboolean on, qboolean over) {
    float pad = 2.0f;
    float thumb = SM_SWITCH_H - 2 * pad;
    vec4_t track;

    if (on) {
        Vector4Copy(smText, track);
    } else {
        Vector4Copy(over ? smBorder : smField, track);
    }
    SM_Pill(x, y, SM_SWITCH_W, SM_SWITCH_H, track, NULL);
    SM_Pill(on ? x + SM_SWITCH_W - pad - thumb : x + pad, y + pad, thumb, thumb, on ? smOnPrimary : smText, NULL);
}

void SM_SliderTrack(float x, float y, float w, float frac, qboolean over) {
    float track = 6.0f;
    float ty = y + (SM_SLIDER_H - track) * 0.5f;
    float tx;

    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    tx = x + frac * (w - SM_SLIDER_H);
    SM_Pill(x, ty, w, track, smField, NULL);
    if (frac > 0) {
        SM_Pill(x, ty, tx - x + SM_SLIDER_H * 0.5f, track, smTextSoft, NULL);
    }
    if (over) {
        vec4_t ring = {smRing[0], smRing[1], smRing[2], 0.25f};

        SM_Pill(tx - 4, y - 4, SM_SLIDER_H + 8, SM_SLIDER_H + 8, ring, NULL);
    }
    SM_Pill(tx, y, SM_SLIDER_H, SM_SLIDER_H, over ? smGold : smText, NULL);
    SM_Pill(tx + 1.5f, y + 1.5f, SM_SLIDER_H - 3, SM_SLIDER_H - 3, smText, NULL);
}

void SM_FieldBox(float x, float y, float w, float h, qboolean focused, qboolean over, const smRect_t* clip) {
    SM_Round(x, y, w, h, SM_RADIUS_SM, focused ? smRing : over ? smBorder : smField, clip);
    SM_Round(x + 1, y + 1, w - 2, h - 2, SM_RADIUS_SM - 1, smRaised, clip);
}

float SM_Kbd(float x, float y, const char* text, qboolean waiting, const smRect_t* clip) {
    float size = 12.0f;
    float w = SM_LabelWidth(size, text, 0.6f) + 16;
    float h = 24.0f;
    smRect_t whole;

    if (w < h) {
        w = h;
    }
    whole = SM_R(x, y, w, h);

    SM_Round(x, y, w, h, SM_RADIUS_SM, waiting ? smGold : smBorder, clip);
    SM_Round(x, y, w, h - 2, SM_RADIUS_SM, waiting ? smGold : smRaisedHover, clip);
    if (SM_TextInside(SM_Baseline(FONT_SANS, size, y, h - 2), size, clip ? clip : &whole)) {
        SM_Label(x + 8, SM_Baseline(FONT_SANS, size, y, h - 2), size, waiting ? smOnPrimary : smText, text, 0.6f);
    }
    return w;
}

smHit_t smHits[SM_MAX_HITS], smLastHits[SM_MAX_HITS];
int smNumHits, smNumLastHits;
smHit_t smUnder;
qboolean smHasUnder;

void SM_Hit(float x, float y, float w, float h, int type, int index, const smRect_t* clip) {
    smRect_t r = SM_R(x, y, w, h);

    if (smNumHits >= SM_MAX_HITS || !SM_Clip(&r, clip)) {
        return;
    }
    smHits[smNumHits].r = r;
    smHits[smNumHits].type = type;
    smHits[smNumHits].index = index;
    smNumHits++;
}

const smHit_t* SM_HitAt(float x, float y) {
    int i;

    for (i = smNumLastHits - 1; i >= 0; i--) {
        if (SM_In(&smLastHits[i].r, x, y)) {
            return &smLastHits[i];
        }
    }
    return NULL;
}

qboolean SM_Over(int type, int index) {
    return smHasUnder && smUnder.type == type && smUnder.index == index;
}

void SM_BeginFrame(void) {
    const smHit_t* under;

    smLook = SM_LOOK_RETAIL;
    SM_Frame();
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;
}

void SM_EndFrame(void) {
    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
}

#define SM_MAX_AVATARS 64

qhandle_t SM_Avatar(const char* steamId) {
    static struct {
        char id[24];
        qhandle_t image;
        int asked;
    } cache[SM_MAX_AVATARS];
    static int next;
    char path[MAX_QPATH];
    int i;

    for (i = 0; i < SM_MAX_AVATARS; i++) {
        if (cache[i].asked && !strcmp(cache[i].id, steamId)) {
            break;
        }
    }
    if (i == SM_MAX_AVATARS) {
        i = next;
        next = (next + 1) % SM_MAX_AVATARS;
        Q_strncpyz(cache[i].id, steamId, sizeof(cache[i].id));
        cache[i].image = 0;
        cache[i].asked = uiInfo.uiDC.realTime - 1000;
    }
    if (!cache[i].image && uiInfo.uiDC.realTime - cache[i].asked >= 1000) {
        cache[i].asked = uiInfo.uiDC.realTime;
        if (trap_SteamAvatar(steamId, path, sizeof(path))) {
            cache[i].image = trap_R_RegisterShaderNoMip(path);
        }
    }
    return cache[i].image;
}
