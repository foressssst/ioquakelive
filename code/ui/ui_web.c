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

/*
The Quake Club palette, its dark theme (app/globals.css, ".dark"). Retail's web UI
and Quake Club are built out of nearly the same greys -- #242121 against #232120,
#1C1919 against #1c1b1a -- but Quake Club's are warm and its text is #ece6db rather
than #DDD, which is the whole difference in feel. The accent is its gold, #c9a961,
where retail used #f7d100.
*/
const vec4_t smText = SM_RGB(236, 230, 219);        // --qc-text-primary
const vec4_t smYellow = SM_RGB(201, 169, 97);       // --qc-chip-border, the accent
const vec4_t smDim = {23 / 255.0f, 22 / 255.0f, 21 / 255.0f, 0.94f};  // --qc-bg-page
const vec4_t smCard = SM_RGB(44, 43, 42);           // --qc-bg-medium
const vec4_t smCardHover = SM_RGB(54, 53, 52);      // --qc-bg-elevated
const vec4_t smRowOdd = SM_RGB(35, 33, 32);         // --qc-bg-dark
const vec4_t smRowEven = SM_RGB(28, 27, 26);        // --qc-bg-pure
const vec4_t smInput = SM_RGB(58, 57, 56);          // --input
const vec4_t smGrey = SM_RGB(67, 65, 64);           // --border
const vec4_t smFocus = SM_RGB(215, 201, 177);       // --ring
const vec4_t smLighter = SM_RGB(67, 65, 64);
const vec4_t smSelectText = SM_RGB(211, 202, 189);  // --qc-text-secondary
const vec4_t smOptionText = SM_RGB(165, 156, 141);  // --qc-text-muted
const vec4_t smDark = SM_RGB(15, 15, 14);           // --qc-bg-darkest
const vec4_t smArrow = SM_RGB(165, 156, 141);
const vec4_t smLink = SM_RGB(201, 169, 97);
const vec4_t smLinkHover = SM_RGB(225, 216, 202);   // --qc-gold
const vec4_t smButton = SM_RGB(54, 53, 52);
const vec4_t smButtonHover = SM_RGB(67, 65, 64);
const vec4_t smButtonBorder = SM_RGB(67, 65, 64);
const vec4_t smDialog = SM_RGB(44, 43, 42);
const vec4_t smShadow = {0.0f, 0.0f, 0.0f, 0.48f};  // --shadow-opacity
const vec4_t smOnAccent = SM_RGB(23, 18, 10);       // --qc-chip-text, over the gold
const vec4_t smSubtle = SM_RGB(123, 115, 102);      // --qc-text-subtle

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

/*
===============
SM_Round

A filled rectangle with Quake Club's rounded corners (--qc-radius-md is 10px and
--qc-radius-lg 14). There is no rounded-corner texture to hand, so each corner is
drawn as a few horizontal slices along the arc; the slice is two pixels so a 10px
corner costs five of them and not ten, which on a laptop is the difference between
free and not. Only the surfaces that read as pieces are rounded -- cards, panels,
buttons, the row under the cursor -- never the whole striped list.
===============
*/
void SM_Round(float x, float y, float w, float h, float r, const float* color, const smRect_t* clip) {
    float step = 2.0f;
    int slices, k;

    if (r > w * 0.5f) {
        r = w * 0.5f;
    }
    if (r > h * 0.5f) {
        r = h * 0.5f;
    }
    if (r < 1.0f) {
        SM_Fill(x, y, w, h, color, clip);
        return;
    }
    SM_Fill(x, y + r, w, h - 2 * r, color, clip);

    slices = (int)(r / step + 0.5f);
    if (slices < 1) {
        slices = 1;
    }
    step = r / slices;
    for (k = 0; k < slices; k++) {
        float mid = r - (k + 0.5f) * step;          // distance from the straight edge
        float inset = r - (float)sqrt(r * r - mid * mid);
        float sy = k * step;

        SM_Fill(x + inset, y + sy, w - 2 * inset, step, color, clip);
        SM_Fill(x + inset, y + h - sy - step, w - 2 * inset, step, color, clip);
    }
}

/*
===============
SM_Shadow

What floats over the page carries one (0 20px 50px rgba(0,0,0,0.25) plus a tighter
0 4px 12px). Drawn as a few rounded plates, each a little larger and fainter than
the one above it, which is as close as stretch pics get to a blur.
===============
*/
void SM_Shadow(float x, float y, float w, float h, float r) {
    static const float spread[4] = {3.0f, 8.0f, 16.0f, 26.0f};
    static const float alpha[4] = {0.22f, 0.15f, 0.09f, 0.05f};
    int k;

    for (k = 3; k >= 0; k--) {
        vec4_t c = {0.0f, 0.0f, 0.0f, 0.0f};
        float s = spread[k];

        c[3] = alpha[k];
        SM_Round(x - s, y - s * 0.4f + 6.0f, w + 2 * s, h + 2 * s * 0.7f, r + s, c, NULL);
    }
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

/*
===============
SM_To640

Turns a rectangle of the page into the 640x480 one the rest of the UI works in,
for the few things drawn by code that expects those coordinates -- the player
model, which UI_DrawPlayer puts through UI_AdjustFrom640 itself.
===============
*/
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

/*
===============================================================================

MOVEMENT

Quake Club's vocabulary, from app/qc-motion.css, and the reasoning is its own:
four curves because a curve says what KIND of thing is moving, and more than four
stop meaning anything; three durations because 140ms was already what the site
used most, so the scale was built on the habit it had rather than a new one.

  suave    what arrives and stays. The default: when in doubt, this one.
  firme    a whole journey, where the travel is the thing being shown.
  corte    what leaves. Going out is not the curve of coming in: something that
           appears is settling into place, something that goes has decided.
  resorte  the accent, overshoots and comes back. Sparingly.

===============================================================================
*/

// the one-dimensional cubic Bezier the CSS curves describe, solved for y at x
static float SM_Bezier(float x, float x1, float y1, float x2, float y2) {
    float t = x;
    int k;

    // Newton on x(t) - x. Four passes is plenty for curves this tame, and the
    // ones here are all monotone in x so it cannot wander off.
    for (k = 0; k < 4; k++) {
        float mt = 1.0f - t;
        float xt = 3.0f * mt * mt * t * x1 + 3.0f * mt * t * t * x2 + t * t * t;
        float dx = 3.0f * mt * mt * x1 + 6.0f * mt * t * (x2 - x1) + 3.0f * t * t * (1.0f - x2);

        if (dx < 0.0001f && dx > -0.0001f) {
            break;
        }
        t -= (xt - x) / dx;
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
    }
    {
        float mt = 1.0f - t;

        return 3.0f * mt * mt * t * y1 + 3.0f * mt * t * t * y2 + t * t * t;
    }
}

float SM_Curve(int ease, float t) {
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    switch (ease) {
        case SM_EASE_FIRME:
            return SM_Bezier(t, 0.65f, 0.0f, 0.35f, 1.0f);
        case SM_EASE_CORTE:
            return SM_Bezier(t, 0.4f, 0.0f, 1.0f, 1.0f);
        case SM_EASE_RESORTE:
            return SM_Bezier(t, 0.34f, 1.4f, 0.64f, 1.0f);
        case SM_EASE_SUAVE:
        default:
            return SM_Bezier(t, 0.16f, 1.0f, 0.3f, 1.0f);
    }
}

/*
===============
SM_Progress

How far into an animation that started at startTime we are, after its share of the
stagger. The delay is 32ms per item and stops at the seventh: past that the wait
shows and the last one to arrive looks broken.
===============
*/
float SM_Progress(int startTime, int duration, int index) {
    int delay = index * SM_STAGGER;
    float t;

    if (index > SM_STAGGER_LAST) {
        delay = SM_STAGGER_LAST * SM_STAGGER;
    }
    if (index < 0) {
        delay = 0;
    }
    if (duration <= 0) {
        return 1.0f;
    }
    t = (float)(uiInfo.uiDC.realTime - startTime - delay) / (float)duration;
    return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}

/*
===============
SM_EnterRise

The gesture everything on a page arrives with: it fades in and rises the last few
pixels. The travel is short on purpose -- what says something arrived is the
movement, not the distance.
===============
*/
void SM_EnterRise(int startTime, int index, float* alpha, float* dy) {
    float t = SM_Curve(SM_EASE_SUAVE, SM_Progress(startTime, SM_DUR_2, index));

    *alpha = t;
    *dy = (1.0f - t) * 8.0f;
}

// what appears in the middle of the screen grows from 96%, not from nothing: a
// thing born out of nowhere reads as a trick, one that grows a little reads as
// having been there and come closer
void SM_EnterZoom(int startTime, float* alpha, float* scale) {
    float t = SM_Curve(SM_EASE_SUAVE, SM_Progress(startTime, SM_DUR_2, 0));

    *alpha = t;
    *scale = 0.96f + 0.04f * t;
}

// a colour faded by an animation's alpha
void SM_Fade(const float* color, float alpha, vec4_t out) {
    out[0] = color[0];
    out[1] = color[1];
    out[2] = color[2];
    out[3] = color[3] * alpha;
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
