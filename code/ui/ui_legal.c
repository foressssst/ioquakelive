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
#define LG_WIDTH 960.0f
#define LG_PAD 15.0f
#define LG_LINE 24.0f

typedef enum {
    LG_BLOCK_H1,
    LG_BLOCK_P,
    LG_BLOCK_HR
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

typedef enum {
    LG_HIT_NONE,
    LG_HIT_BACK
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
    float scroll;
    float content;
} lg;

static const vec4_t lgBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t lgText = SM_RGB(238, 238, 238);
static const vec4_t lgRule = SM_RGB(225, 225, 225);

static const char* lgP;

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

static void LG_AddRun(const char* text, qboolean bold, qboolean underline) {
    if (lg.numRuns < LG_MAX_RUNS) {
        lg.run[lg.numRuns].text = text;
        lg.run[lg.numRuns].bold = bold;
        lg.run[lg.numRuns].underline = underline;
        lg.numRuns++;
    }
}

static void LG_Element(qboolean bold, qboolean underline);

static void LG_Children(qboolean bold, qboolean underline) {
    for (;;) {
        LG_Space();
        if (!*lgP || *lgP == ')') {
            if (*lgP) {
                lgP++;
            }
            return;
        }
        if (*lgP == '\'' || *lgP == '"') {
            LG_AddRun(LG_String(), bold, underline);
        } else if (!Q_strncmp(lgP, "React.createElement(", 20)) {
            lgP += 20;
            LG_Element(bold, underline);
        } else if (!Q_strncmp(lgP, "null", 4)) {
            lgP += 4;
        } else {
            lgP++;
        }
    }
}

static void LG_Element(qboolean bold, qboolean underline) {
    char tag[16];
    qboolean center = qfalse;
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
        center = (qboolean)(Q_strncmp(start, "{ style: { textAlign: 'center' } }", 34) == 0);
    } else if (!Q_strncmp(lgP, "null", 4)) {
        lgP += 4;
    }

    if (!strcmp(tag, "br")) {
        LG_AddRun(NULL, bold, underline);
        LG_Children(bold, underline);
        return;
    }
    if (!strcmp(tag, "h1") || !strcmp(tag, "p") || !strcmp(tag, "hr")) {
        lgBlock_t* b = lg.numBlocks < LG_MAX_BLOCKS ? &lg.block[lg.numBlocks++] : NULL;

        if (b) {
            b->type = tag[0] == 'h' && tag[1] == '1' ? LG_BLOCK_H1 : tag[0] == 'h' ? LG_BLOCK_HR : LG_BLOCK_P;
            b->center = center;
            b->firstRun = lg.numRuns;
        }
        LG_Children(bold, underline);
        if (b) {
            b->numRuns = lg.numRuns - b->firstRun;
        }
        return;
    }
    if (!strcmp(tag, "a")) {
        int runs = lg.numRuns;

        LG_Children(bold, underline);
        lg.numRuns = runs;
        return;
    }
    LG_Children((qboolean)(bold || !strcmp(tag, "b")), (qboolean)(underline || !strcmp(tag, "u")));
}

static void LG_Load(void) {
    const char* marker = "className: 'body orow scroll-y legal-notices' }";

    lg.loaded = qtrue;
    lg.numBlocks = lg.numRuns = lg.stringsLen = 0;
    if (trap_WebPakExcerpt("webui/bundle.js", marker, lg.code, sizeof(lg.code)) <= 0) {
        return;
    }
    lgP = lg.code + strlen(marker);
    LG_Children(qfalse, qfalse);
}

static float LG_Lines(const lgBlock_t* b, float x, float y, float width, qboolean draw, const smRect_t* clip) {
    typedef struct {
        const char* text;
        int len;
        float w;
        qboolean bold, underline, space;
    } lgWord_t;
    static lgWord_t words[1024];
    int numWords = 0, i, lines = 0;
    float size = b->type == LG_BLOCK_H1 ? 32.0f : 15.0f;
    float lineH = b->type == LG_BLOCK_H1 ? 38.4f : LG_LINE;
    int font = b->type == LG_BLOCK_H1 ? FONT_DEFAULT : FONT_SANS;
    float spaceW = SM_Advance(font, size, " ", -1);
    qboolean pendingSpace = qfalse;

    for (i = 0; i < b->numRuns && numWords < (int)ARRAY_LEN(words); i++) {
        const lgRun_t* r = &lg.run[b->firstRun + i];
        const char* p = r->text;

        if (!p) {
            words[numWords].text = NULL;
            numWords++;
            pendingSpace = qfalse;
            continue;
        }
        while (*p && numWords < (int)ARRAY_LEN(words)) {
            const char* s = p;

            if (*p == ' ' || *p == '\n') {
                pendingSpace = qtrue;
                p++;
                continue;
            }
            while (*p && *p != ' ' && *p != '\n') {
                p++;
            }
            words[numWords].text = s;
            words[numWords].len = (int)(p - s);
            words[numWords].bold = r->bold;
            words[numWords].underline = r->underline;
            words[numWords].space = pendingSpace;
            words[numWords].w = SM_AdvanceBold(font, size, s, (int)(p - s), r->bold);
            numWords++;
            pendingSpace = qfalse;
        }
    }

    for (i = 0; i < numWords || (i == 0 && !numWords);) {
        int first = i, last;
        float lw = 0;

        while (i < numWords && words[i].text) {
            float add = (i > first && words[i].space ? spaceW : 0) + words[i].w;

            if (i > first && lw + add > width) {
                break;
            }
            lw += add;
            i++;
        }
        last = i;
        if (i < numWords && !words[i].text) {
            i++;
        }
        if (draw) {
            float lx = b->center ? x + (width - lw) * 0.5f : x;
            float baseline = SM_Baseline(font, size, y + lines * lineH, lineH);
            int k;

            if (SM_TextInside(baseline, size, clip)) {
                for (k = first; k < last; k++) {
                    if (k > first && words[k].space) {
                        if (words[k].underline && words[k - 1].underline) {
                            SM_Fill(lx, baseline + 2, spaceW, 1, lgText, clip);
                        }
                        lx += spaceW;
                    }
                    SM_TextRaw(lx, baseline, font, size, lgText, words[k].text, words[k].len, words[k].bold);
                    if (words[k].underline) {
                        SM_Fill(lx, baseline + 2, words[k].w, 1, lgText, clip);
                    }
                    lx += words[k].w;
                }
            }
        }
        lines++;
        if (!numWords) {
            break;
        }
    }
    return lines * lineH;
}

static float LG_Paragraph(const lgBlock_t* b, float x, float y, float width, qboolean draw, const smRect_t* clip) {
    float h;

    SM_SetSpacing(b->type == LG_BLOCK_H1 ? SM_SPACING_H1 : 0);
    h = LG_Lines(b, x, y, width, draw, clip);
    SM_SetSpacing(0);
    return h;
}

void UI_Legal_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    if (!lg.loaded) {
        LG_Load();
    }
    lg.scroll = 0;
    menu = Menus_FindByName("legal");
    item = menu ? Menu_FindItemByName(menu, "legal") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Legal_Paint(void) {
    float top = -smTop / smScale;
    float boxX = (smVW - LG_WIDTH) * 0.5f;
    float boxY = top + 15;
    float boxH = smBottom - top - 30;
    float x = boxX + LG_PAD, w = LG_WIDTH - 2 * LG_PAD;
    float y, bw;
    smRect_t clip;
    int i;

    SM_BeginFrame();
    if (!lg.loaded) {
        LG_Load();
    }
    UI_Home_Backdrop();
    SM_Fill(boxX, boxY, LG_WIDTH, boxH, lgBlack80, NULL);

    if (lg.scroll > lg.content - boxH) {
        lg.scroll = lg.content - boxH;
    }
    if (lg.scroll < 0) {
        lg.scroll = 0;
    }
    clip = SM_R(boxX, boxY, LG_WIDTH, boxH);
    y = boxY + LG_PAD - lg.scroll;

    bw = SM_WebButtonWidth("Back to Main Menu");
    SM_WebButton(boxX + (LG_WIDTH - bw) * 0.5f, y, bw, "Back to Main Menu", qfalse, SM_Over(LG_HIT_BACK, 0), &clip);
    SM_Hit(boxX + (LG_WIDTH - bw) * 0.5f, y, bw, SM_WEB_BUTTON_H, LG_HIT_BACK, 0, &clip);
    y += SM_WEB_BUTTON_H;

    for (i = 0; i < lg.numBlocks; i++) {
        const lgBlock_t* b = &lg.block[i];

        if (b->type == LG_BLOCK_H1) {
            y += 8;
            y += LG_Paragraph(b, x + 6.4f, y, w - 6.4f, qtrue, &clip);
            y += 8;
        } else if (b->type == LG_BLOCK_HR) {
            y += 15;
            SM_Fill(x, y, w, 1, lgRule, &clip);
            y += 1 + 35;
        } else {
            y += LG_Paragraph(b, x, y, w, qtrue, &clip);
            y += 15;
        }
    }
    lg.content = y + lg.scroll - boxY + LG_PAD;

    if (!lg.numBlocks) {
        SM_Text(x, SM_Baseline(FONT_SANS, 15, boxY + 70, LG_LINE), FONT_SANS, 15, lgText,
                "The notices are read from Quake Live's web.pak, which was not found.", -1, qfalse);
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
    if (hit && hit->type == LG_HIT_BACK) {
        UI_Home_Go(HM_NAV_HOME);
    }
    return qtrue;
}
