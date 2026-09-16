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
// ui_web.c -- [QL] what every page of retail's web UI drawn here has in common
//
// See ui_web.h. The web UI's own art is in web.pak, which is not shipped: pak00's
// fonts stand in for it, and a second pass one pixel to the right stands in for
// Noto Sans Bold.
//

#include "ui_web.h"

const vec4_t smText = SM_RGB(221, 221, 221);
const vec4_t smYellow = SM_RGB(247, 209, 0);
const vec4_t smDim = {0.0f, 0.0f, 0.0f, 0.8f};
const vec4_t smCard = SM_RGB(44, 41, 41);
const vec4_t smCardHover = SM_RGB(60, 57, 57);
const vec4_t smRowOdd = SM_RGB(36, 33, 33);
const vec4_t smRowEven = SM_RGB(28, 25, 25);
const vec4_t smInput = SM_RGB(15, 15, 15);
const vec4_t smGrey = SM_RGB(51, 51, 51);
const vec4_t smFocus = SM_RGB(51, 195, 240);
const vec4_t smLighter = SM_RGB(68, 68, 68);
const vec4_t smSelectText = SM_RGB(204, 204, 204);
const vec4_t smOptionText = SM_RGB(187, 187, 187);
const vec4_t smDark = SM_RGB(34, 34, 34);
const vec4_t smArrow = SM_RGB(153, 153, 153);
const vec4_t smLink = SM_RGB(30, 174, 219);
const vec4_t smLinkHover = SM_RGB(15, 160, 206);
const vec4_t smButton = SM_RGB(61, 61, 61);
const vec4_t smButtonHover = SM_RGB(93, 93, 93);
const vec4_t smButtonBorder = SM_RGB(77, 77, 77);
const vec4_t smDialog = SM_RGB(30, 30, 30);
const vec4_t smShadow = {0.0f, 0.0f, 0.0f, 0.5f};

float smScale;
float smTop;
float smVW;
float smMX, smMY;

void SM_Frame(void) {
    float w = (float)uiInfo.uiDC.glconfig.vidWidth;
    float h = (float)uiInfo.uiDC.glconfig.vidHeight;

    smScale = h / SM_VIEW_H;
    if (w / smScale < SM_MIN_VIEW_W) {
        smScale = w / SM_MIN_VIEW_W;
    }
    smVW = w / smScale;
    smTop = (h - SM_VIEW_H * smScale) * 0.5f;
    smMX = (uiInfo.uiDC.cursorx * uiInfo.uiDC.xscale + uiInfo.uiDC.bias) / smScale;
    smMY = (uiInfo.uiDC.cursory * uiInfo.uiDC.yscale - smTop) / smScale;
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
    // what the clip leaves, in texture coordinates too
    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(r.x * smScale, smTop + r.y * smScale, r.w * smScale, r.h * smScale, (r.x - x) / w,
                          (r.y - y) / h, (r.x + r.w - x) / w, (r.y + r.h - y) / h, shader);
}

float SM_Em(int font) {
    return font == FONT_DEFAULT ? SM_HANDEL_EM : SM_SANS_EM;
}

float SM_Baseline(int font, float size, float lineTop, float lineHeight) {
    return lineTop + lineHeight * 0.5f + size * (font == FONT_DEFAULT ? SM_HANDEL_BASE : SM_SANS_BASE);
}

// whether a line of text at this baseline shows whole inside the clip
qboolean SM_TextInside(float baseline, float size, const smRect_t* clip) {
    return baseline - size * 1.07f >= clip->y - 1.0f && baseline + size * 0.3f <= clip->y + clip->h + 1.0f;
}

/*
===============
SM_Advance

A string's advance in CSS pixels. fontstash's box ends at the last glyph's ink,
so a bar is measured after the text and taken off again: that keeps trailing
spaces and the side bearings.
===============
*/
float SM_Advance(int font, float size, const char* text, int len) {
    char buf[MAX_STRING_CHARS];
    float withBar[5], bar[5];
    float px = size * SM_Em(font) * smScale;

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
    trap_R_Font_TextExtents(buf, 0, font, px, -1, withBar);
    trap_R_Font_TextExtents("|", 0, font, px, -1, bar);
    return (withBar[2] - bar[2]) / smScale;
}

// the characters of text that fit in width
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

// pak00 has no Noto Sans Bold: a second pass a pixel to the right stands in for it
int SM_BoldShift(void) {
    int shift = (int)(smScale + 0.5f);

    return shift < 1 ? 1 : shift;
}

void SM_Text(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
             qboolean bold) {
    float px = size * SM_Em(font) * smScale;
    int sx = (int)(x * smScale + 0.5f);
    int sy = (int)(smTop + baseline * smScale + 0.5f);

    if (!text || !text[0] || limit == 0) {
        return;
    }
    trap_R_SetColor(color);
    trap_R_Font_DrawString(sx, sy, text, font, px, limit, NULL, 0);
    if (bold) {
        trap_R_Font_DrawString(sx + SM_BoldShift(), sy, text, font, px, limit, NULL, 0);
    }
    trap_R_SetColor(NULL);
}

void SM_Centered(float middle, float baseline, float size, const float* color, const char* text, qboolean bold) {
    SM_Text(middle - SM_Advance(FONT_SANS, size, text, -1) * 0.5f, baseline, FONT_SANS, size, color, text, -1, bold);
}

// letter-spacing comes after every character, the last one too
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
        x += SM_Advance(font, size, c, 1) + spacing;
    }
}

/*
===============
SM_Wrap

Breaks text into the lines a block of this width shows, at spaces. Returns how
many, with where each starts and how long it is.
===============
*/
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

        // a line does not start with the spaces it broke at
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

// a CSS border triangle, 10 wide and 5 high
void SM_Arrow(float x, float y, qboolean up) {
    int k;

    for (k = 0; k < 5; k++) {
        float w = up ? 2.0f * (k + 1) : 10.0f - 2.0f * k;

        SM_Fill(x + (10.0f - w) * 0.5f, y + k, w, 1, smArrow, NULL);
    }
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
