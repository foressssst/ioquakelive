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

#define LG_MAX_TEXT 0x30000
#define LG_MAX_BLOCKS 256
#define LG_MAX_RUNS 4096
#define LG_MAX_WORDS 1024
#define LG_WIDTH SM_Len(960.0f)
#define LG_MARGIN SM_Len(15.0f)
#define LG_PAD SM_Len(15.0f)
#define LG_BAR SM_Len(8.0f)
#define LG_LINE SM_LineHeight(15.0f, 1.6f)
#define LG_H1_LEFT SM_Len(6.4f)

typedef enum {
    LG_BLOCK_H1,
    LG_BLOCK_P,
    LG_BLOCK_HR,
    LG_BLOCK_TEXT,
    LG_BLOCK_LI,
    LG_BLOCK_UL
} lgBlockType_t;

typedef struct {
    const char* text;
    qboolean bold;
    qboolean underline;
} lgRun_t;

typedef struct {
    lgBlockType_t type;
    qboolean center;
    int firstRun, numRuns;
} lgBlock_t;

typedef struct {
    const char* text;
    int len;
    float w;
    qboolean bold, underline, space, spaceBold, spaceUnderline;
} lgWord_t;

typedef enum {
    LG_HIT_NONE,
    LG_HIT_BACK,
    LG_HIT_TRACK,
    LG_HIT_THUMB
} lgHitType_t;

static struct {
    qboolean loaded;
    char code[LG_MAX_TEXT];
    char strings[LG_MAX_TEXT];
    int stringsLen;
    lgBlock_t block[LG_MAX_BLOCKS];
    int numBlocks;
    lgRun_t run[LG_MAX_RUNS];
    int numRuns;
    lgWord_t word[LG_MAX_WORDS];
    float scroll;
    float content;
    qboolean drag;
    float grab;
} lg;

static const vec4_t lgBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t lgText = SM_RGB(238, 238, 238);
static const vec4_t lgRule = SM_RGB(225, 225, 225);
static const vec4_t lgThumb = SM_RGB(51, 51, 51);
static const vec4_t lgThumbHover = SM_RGB(153, 153, 153);

static const char* lgP;
static lgBlock_t* lgCur;

static void LG_Space(void) {
    while (*lgP == ' ' || *lgP == '\t' || *lgP == '\r' || *lgP == '\n' || *lgP == ',') {
        lgP++;
    }
}

static const char* LG_String(void) {
    char quote = *lgP++;
    char* out = lg.strings + lg.stringsLen;
    int n = 0;

    while (*lgP && *lgP != quote && lg.stringsLen + n < LG_MAX_TEXT - 8) {
        if (*lgP == '\\' && lgP[1]) {
            lgP++;
            if (*lgP == 'u') {
                unsigned int cp = 0;
                int k;

                for (k = 1; k <= 4 && lgP[k]; k++) {
                    char c = lgP[k];

                    cp = cp * 16 + (c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
                }
                lgP += 5;
                if (cp < 0x80) {
                    out[n++] = (char)cp;
                } else if (cp < 0x800) {
                    out[n++] = (char)(0xc0 | (cp >> 6));
                    out[n++] = (char)(0x80 | (cp & 0x3f));
                } else {
                    out[n++] = (char)(0xe0 | (cp >> 12));
                    out[n++] = (char)(0x80 | ((cp >> 6) & 0x3f));
                    out[n++] = (char)(0x80 | (cp & 0x3f));
                }
                continue;
            }
            out[n++] = *lgP == 'n' ? '\n' : *lgP;
            lgP++;
            continue;
        }
        out[n++] = *lgP++;
    }
    if (*lgP == quote) {
        lgP++;
    }
    out[n] = '\0';
    lg.stringsLen += n + 1;
    return out;
}

static lgBlock_t* LG_NewBlock(lgBlockType_t type, qboolean center) {
    lgBlock_t* b;

    if (lg.numBlocks >= LG_MAX_BLOCKS) {
        return NULL;
    }
    b = &lg.block[lg.numBlocks++];
    b->type = type;
    b->center = center;
    b->firstRun = lg.numRuns;
    b->numRuns = 0;
    return b;
}

static void LG_AddRun(const char* text, qboolean bold, qboolean underline, qboolean center) {
    if (!lgCur) {
        lgCur = LG_NewBlock(LG_BLOCK_TEXT, center);
        if (!lgCur) {
            return;
        }
    }
    if (lg.numRuns < LG_MAX_RUNS) {
        lg.run[lg.numRuns].text = text;
        lg.run[lg.numRuns].bold = bold;
        lg.run[lg.numRuns].underline = underline;
        lg.numRuns++;
        lgCur->numRuns = lg.numRuns - lgCur->firstRun;
    }
}

static void LG_Element(qboolean bold, qboolean underline, qboolean center);

static void LG_Children(qboolean bold, qboolean underline, qboolean center) {
    for (;;) {
        LG_Space();
        if (!*lgP || *lgP == ')') {
            if (*lgP) {
                lgP++;
            }
            return;
        }
        if (*lgP == '\'' || *lgP == '"') {
            LG_AddRun(LG_String(), bold, underline, center);
        } else if (!Q_strncmp(lgP, "React.createElement(", 20)) {
            lgP += 20;
            LG_Element(bold, underline, center);
        } else if (!Q_strncmp(lgP, "null", 4)) {
            lgP += 4;
        } else {
            lgP++;
        }
    }
}

static void LG_Block(lgBlockType_t type, qboolean center) {
    lgCur = LG_NewBlock(type, center);
    LG_Children(qfalse, qfalse, center);
    lgCur = NULL;
}

static void LG_Element(qboolean bold, qboolean underline, qboolean center) {
    char tag[16];
    int n = 0;

    LG_Space();
    if (*lgP == '\'') {
        lgP++;
        while (*lgP && *lgP != '\'' && n < (int)sizeof(tag) - 1) {
            tag[n++] = *lgP++;
        }
        if (*lgP) {
            lgP++;
        }
    }
    tag[n] = '\0';

    LG_Space();
    if (*lgP == '{') {
        int depth = 0;
        const char* start = lgP;

        do {
            if (*lgP == '{') {
                depth++;
            } else if (*lgP == '}') {
                depth--;
            }
            lgP++;
        } while (*lgP && depth);
        if (!Q_strncmp(start, "{ style: { textAlign: 'center' } }", 34)) {
            center = qtrue;
        }
    } else if (!Q_strncmp(lgP, "null", 4)) {
        lgP += 4;
    }

    if (!strcmp(tag, "br")) {
        LG_AddRun(NULL, bold, underline, center);
        LG_Children(bold, underline, center);
    } else if (!strcmp(tag, "p")) {
        LG_Block(LG_BLOCK_P, center);
    } else if (!strcmp(tag, "h1")) {
        LG_Block(LG_BLOCK_H1, center);
    } else if (!strcmp(tag, "li")) {
        LG_Block(LG_BLOCK_LI, center);
    } else if (!strcmp(tag, "hr")) {
        LG_NewBlock(LG_BLOCK_HR, center);
        lgCur = NULL;
        LG_Children(bold, underline, center);
    } else if (!strcmp(tag, "ul") || !strcmp(tag, "center") || !strcmp(tag, "div")) {
        lgCur = NULL;
        LG_Children(qfalse, qfalse, (qboolean)(center || !strcmp(tag, "center")));
        lgCur = NULL;
        if (tag[0] == 'u') {
            LG_NewBlock(LG_BLOCK_UL, center);
        }
    } else if (!strcmp(tag, "a")) {
        int runs = lg.numRuns, blocks = lg.numBlocks;
        lgBlock_t* cur = lgCur;

        LG_Children(bold, underline, center);
        lg.numRuns = runs;
        lg.numBlocks = blocks;
        lgCur = cur;
        if (cur) {
            cur->numRuns = runs - cur->firstRun;
        }
    } else {
        LG_Children((qboolean)(bold || !strcmp(tag, "b")), (qboolean)(underline || !strcmp(tag, "u")), center);
    }
}

static void LG_Load(void) {
    const char* marker = "className: 'body orow scroll-y legal-notices' }";

    lg.loaded = qtrue;
    lg.numBlocks = lg.numRuns = lg.stringsLen = 0;
    lgCur = NULL;
    if (trap_WebPakExcerpt("webui/bundle.js", marker, lg.code, sizeof(lg.code)) <= 0) {
        return;
    }
    lgP = lg.code + strlen(marker);
    LG_Children(qfalse, qfalse, qfalse);
    lgCur = NULL;
}

static int LG_Words(const lgBlock_t* b, int font, float size) {
    int numWords = 0, i;
    qboolean pendingSpace = qfalse, spaceBold = qfalse, spaceUnderline = qfalse;

    for (i = 0; i < b->numRuns && numWords < LG_MAX_WORDS; i++) {
        const lgRun_t* r = &lg.run[b->firstRun + i];
        const char* p = r->text;

        if (!p) {
            lg.word[numWords].text = NULL;
            numWords++;
            pendingSpace = qfalse;
            continue;
        }
        while (*p && numWords < LG_MAX_WORDS) {
            const char* s = p;
            lgWord_t* w;

            if (*p == ' ' || *p == '\n') {
                if (!pendingSpace) {
                    spaceBold = r->bold;
                    spaceUnderline = r->underline;
                }
                pendingSpace = qtrue;
                p++;
                continue;
            }
            while (*p && *p != ' ' && *p != '\n') {
                p++;
            }
            w = &lg.word[numWords++];
            w->text = s;
            w->len = (int)(p - s);
            w->bold = r->bold;
            w->underline = r->underline;
            w->space = pendingSpace;
            w->spaceBold = spaceBold;
            w->spaceUnderline = spaceUnderline;
            w->w = SM_AdvanceBold(font, size, s, w->len, r->bold);
            pendingSpace = qfalse;
        }
    }
    return numWords;
}

static float LG_Lines(const lgBlock_t* b, float x, float y, float width, qboolean draw, const smRect_t* clip) {
    qboolean h1 = (qboolean)(b->type == LG_BLOCK_H1);
    float size = h1 ? 32.0f : 15.0f;
    float lineH = h1 ? SM_H1_LINE : LG_LINE;
    int font = h1 ? FONT_DEFAULT : FONT_SANS;
    float spaceW = SM_AdvanceBold(font, size, " ", 1, qfalse);
    float spaceBoldW = SM_AdvanceBold(font, size, " ", 1, qtrue);
    int ascent = SM_Px(SM_Ascent(font, size));
    float marker = b->type == LG_BLOCK_LI ? ascent / smScale : 0;
    int numWords, i = 0, lines = 0;

    if (h1) {
        x += LG_H1_LEFT;
        width -= LG_H1_LEFT;
    }
    SM_SetSpacing(h1 ? SM_SPACING_H1 : 0);
    numWords = LG_Words(b, font, size);
    while (i < numWords) {
        int first = i, last, k;
        float lead = lines ? 0 : marker;
        float lw = lead;

        while (i < numWords && lg.word[i].text) {
            int end = i + 1;
            float add = lg.word[i].w;

            if (i > first && lg.word[i].space) {
                add += lg.word[i].spaceBold ? spaceBoldW : spaceW;
            }
            while (end < numWords && lg.word[end].text && !lg.word[end].space) {
                add += lg.word[end].w;
                end++;
            }
            if (i > first && lw + add > width) {
                break;
            }
            lw += add;
            i = end;
        }
        last = i;
        if (i < numWords && !lg.word[i].text) {
            i++;
        }
        if (draw) {
            float lineTop = y + lines * lineH;
            float baseline = SM_Baseline(font, size, lineTop, lineH);
            float lx = b->center ? x + SM_Center(width, lw) : x;

            if (SM_TextInside(baseline, size, clip)) {
                if (lead > 0) {
                    int bullet = (ascent * 2 / 3 + 1) / 2;
                    float radius = bullet * 0.5f / smScale;
                    float cy = baseline - ascent / smScale + (float)(3 * (ascent - ascent * 2 / 3) / 2) / smScale + radius;

                    SM_Circle(lx + radius, cy, radius, lgText, clip);
                    lx += lead;
                }
                for (k = first; k < last; k++) {
                    const lgWord_t* w = &lg.word[k];

                    if (k > first && w->space) {
                        float sw = w->spaceBold ? spaceBoldW : spaceW;

                        if (w->spaceUnderline) {
                            SM_Fill(lx, baseline + 1.0f / smScale, sw, 1.0f / smScale, lgText, clip);
                        }
                        lx += sw;
                    }
                    SM_TextRaw(lx, baseline, font, size, lgText, w->text, w->len, w->bold);
                    if (w->underline) {
                        SM_Fill(lx, baseline + 1.0f / smScale, w->w, 1.0f / smScale, lgText, clip);
                    }
                    lx += w->w;
                }
            }
        }
        lines++;
    }
    SM_SetSpacing(0);
    return lines * lineH;
}

static float LG_Layout(float x, float y, float width, qboolean draw, const smRect_t* clip) {
    float top = y;
    float pending = 0;
    int i;

    for (i = 0; i < lg.numBlocks; i++) {
        const lgBlock_t* b = &lg.block[i];
        float mt = 0, mb = 0, h;

        switch (b->type) {
            case LG_BLOCK_H1:
                mt = mb = SM_Len(8.0f);
                break;
            case LG_BLOCK_P:
                mb = SM_Len(15.0f);
                break;
            case LG_BLOCK_HR:
                mt = SM_Rem(30.0f);
                mb = SM_Rem(35.0f);
                break;
            case LG_BLOCK_LI:
                mb = SM_Rem(10.0f);
                break;
            case LG_BLOCK_UL:
                mb = SM_Len(15.0f);
                break;
            default:
                break;
        }
        if (mt > pending) {
            pending = mt;
        }
        h = b->type == LG_BLOCK_HR ? SM_LenBorder(1.0f)
            : b->type == LG_BLOCK_UL ? 0
                                     : LG_Lines(b, x, y + pending, width, qfalse, NULL);
        if (h <= 0) {
            if (mb > pending) {
                pending = mb;
            }
            continue;
        }
        y += pending;
        if (draw) {
            if (b->type == LG_BLOCK_HR) {
                SM_Fill(x, y, width, h, lgRule, clip);
            } else {
                LG_Lines(b, x, y, width, qtrue, clip);
            }
        }
        y += h;
        pending = mb;
    }
    return y + pending - top;
}

static void LG_Thumb(float x, float top, float view, float* thumbY, float* thumbH) {
    int track = SM_Px(view);
    int total = SM_Px(lg.content);
    int len = total > 0 ? (int)floorf((float)track * track / total + 0.5f) : track;
    int range = total - track;

    *thumbH = (float)len / smScale;
    *thumbY = top + (range > 0 ? (float)(int)floorf(SM_Px(lg.scroll) * (float)(track - len) / range + 0.5f) : 0) / smScale;
}

void UI_Legal_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    if (!lg.loaded) {
        LG_Load();
    }
    lg.scroll = 0;
    lg.drag = qfalse;
    menu = Menus_FindByName("legal");
    item = menu ? Menu_FindItemByName(menu, "legal") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Legal_Paint(void) {
    float top = -smTop / smScale;
    float boxW = LG_WIDTH;
    float boxX = (float)((SM_Px(smVW) - SM_Px(boxW)) / 2) / smScale;
    float boxY = top + LG_MARGIN;
    float boxH = smBottom - SM_Len(30.0f) - LG_MARGIN - boxY;
    float x = boxX + LG_PAD;
    float w = boxW - 2 * LG_PAD - LG_BAR;
    float y, bw, bx, thumbY, thumbH;
    smRect_t clip;

    SM_BeginFrame();
    if (!lg.loaded) {
        LG_Load();
    }
    UI_Home_Backdrop();
    SM_Fill(boxX, boxY, boxW, boxH, lgBlack80, NULL);

    lg.content = 2 * LG_PAD + SM_WEB_BUTTON_H + LG_Layout(x, 0, w, qfalse, NULL);
    if (lg.drag) {
        if (!trap_Key_IsDown(K_MOUSE1)) {
            lg.drag = qfalse;
        } else {
            LG_Thumb(boxX + boxW - LG_BAR, boxY, boxH, &thumbY, &thumbH);
            if (boxH > thumbH) {
                lg.scroll += (smMY - lg.grab) * (lg.content - boxH) / (boxH - thumbH);
            }
            lg.grab = smMY;
        }
    }
    if (lg.scroll > lg.content - boxH) {
        lg.scroll = lg.content - boxH;
    }
    if (lg.scroll < 0) {
        lg.scroll = 0;
    }
    lg.scroll = SM_Px(lg.scroll) / smScale;
    clip = SM_R(boxX, boxY, boxW - LG_BAR, boxH);
    y = boxY + LG_PAD - lg.scroll;

    bw = SM_WebButtonWidth("Back to Main Menu");
    bx = x + SM_Center(w, bw);
    SM_WebButton(bx, y, bw, "Back to Main Menu", qfalse, SM_Over(LG_HIT_BACK, 0), &clip);
    SM_Hit(bx, y, bw, SM_WEB_BUTTON_H, LG_HIT_BACK, 0, &clip);
    LG_Layout(x, y + SM_WEB_BUTTON_H, w, qtrue, &clip);

    if (!lg.numBlocks) {
        SM_Text(x, SM_Baseline(FONT_SANS, 15, boxY + SM_Len(70.0f), LG_LINE), FONT_SANS, 15, lgText,
                "The notices are read from Quake Live's web.pak, which was not found.", -1, qfalse);
    }

    if (lg.content > boxH) {
        float barX = boxX + boxW - LG_BAR;

        LG_Thumb(barX, boxY, boxH, &thumbY, &thumbH);
        SM_Hit(barX, boxY, LG_BAR, boxH, LG_HIT_TRACK, 0, NULL);
        SM_Fill(barX, thumbY, LG_BAR, thumbH, SM_Over(LG_HIT_THUMB, 0) || lg.drag ? lgThumbHover : lgThumb, NULL);
        SM_Hit(barX, thumbY, LG_BAR, thumbH, LG_HIT_THUMB, 0, NULL);
    }
    SM_EndFrame();
}

qboolean UI_Legal_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        lg.scroll += key == K_MWHEELUP ? -60.0f : 60.0f;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;
    }
    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    if (hit->type == LG_HIT_BACK) {
        UI_Home_Go(HM_NAV_HOME);
    } else if (hit->type == LG_HIT_THUMB) {
        lg.drag = qtrue;
        lg.grab = smMY;
    } else if (hit->type == LG_HIT_TRACK) {
        int track = SM_Px(hit->r.h);
        int page = (int)(track * 0.875f) > track - 40 ? (int)(track * 0.875f) : track - 40;
        float thumbY, thumbH;

        LG_Thumb(hit->r.x, hit->r.y, hit->r.h, &thumbY, &thumbH);
        lg.scroll += (smMY < thumbY ? -page : page) / smScale;
    }
    return qtrue;
}
