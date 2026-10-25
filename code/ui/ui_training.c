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

#define TR_SECTION_H SM_Len(675.0f)
#define TR_RETAIL_CARDS 3
#define TR_BAR 8.0f
#define TR_LINE SM_LineHeight(TR_TEXT_SIZE, 1.6f)

typedef enum {
    TR_HIT_NONE,
    TR_HIT_CARD,
    TR_HIT_START,
    TR_HIT_VIDEO
} trHitType_t;

typedef struct {
    const char* map;
    const char* picture;
    const char* title;
    const char* subtitle;
    const char* text;
    const char* command;
    const char* video;
} trCard_t;

static const trCard_t trCards[] = {
    {"qztraining", "webui/img/training/training.jpg", "Crash Course", "Welcome to the Arena",
     "Meet _Crash_, our QUAKE LIVE trainer, and get a guided tour of some of the basic items of combat.",
     "map qztraining _training", NULL},
    {"qzpractice2", "webui/img/training/strafe.jpg", "Accelerate", "Travelling at High Speed",
     "Practice the basics of _Strafe Jumping_, an essential skill to learning how to move faster than other arena "
     "combatants.",
     "map qzpractice2 _sj; wait 2; team f", "https://youtu.be/HnIeHqBk8v4"},
    {"qzpractice1", "webui/img/training/rocket.jpg", "Elevate", "Reaching Great Heights",
     "Use your projectile weapons to perform _Rocket Jumps_, and access areas of the arena in entirely different "
     "ways.",
     "map qzpractice1 _rj; wait 2; team f", "https://youtu.be/pVs1KGK0VLY"},
    {"thunderstruck", "", "Tracking", "Keeping the Beam On",
     "Keep the _Lightning Gun_ on a bot that does nothing but dodge. It never fights back and never goes "
     "down.",
     "map thunderstruck _lgtrack; wait 2; team f; addbot crash 5", NULL},
};
#define TR_NUM_CARDS ((int)ARRAY_LEN(trCards))

static struct {
    qboolean loaded;
    qhandle_t picture[TR_NUM_CARDS];
    float hover[TR_NUM_CARDS];
    int lastTime;
    float scroll;
    float content;
} tr;

static const vec4_t trYellow = SM_RGB(247, 209, 0);
static const vec4_t trBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};

static void TR_Launch(int i) {
    trap_Cvar_Set("bot_minplayers", "0");
    trap_Cvar_Set("sv_serverType", "0");
    trap_Cmd_ExecuteText(EXEC_APPEND, va("%s\n", trCards[i].command));
}

#define TR_MAX_WORDS 48
#define TR_TEXT_SIZE 15.0f

typedef struct {
    char text[32];
    float width;
} trWord_t;

static float TR_WordWidth(const char* word, qboolean* italic) {
    char piece[32];
    float w = 0;
    int n = 0;

    for (;; word++) {
        if (*word == '_' || !*word) {
            piece[n] = '\0';
            if (n) {
                w += SM_Advance(*italic ? SM_ItalicFace() : FONT_SANS, TR_TEXT_SIZE, piece, -1);
            }
            n = 0;
            if (!*word) {
                return w;
            }
            *italic = (qboolean)!*italic;
            continue;
        }
        if (n < (int)sizeof(piece) - 1) {
            piece[n++] = *word;
        }
    }
}

static float TR_DrawWord(float x, float baseline, const char* word, qboolean* italic, const smRect_t* clip) {
    char piece[32];
    int n = 0;

    for (;; word++) {
        if (*word == '_' || !*word) {
            piece[n] = '\0';
            if (n) {
                int font = *italic ? SM_ItalicFace() : FONT_SANS;

                if (SM_TextInside(baseline, TR_TEXT_SIZE, clip)) {
                    SM_Text(x, baseline, font, TR_TEXT_SIZE, smText, piece, -1, qfalse);
                }
                x += SM_Advance(font, TR_TEXT_SIZE, piece, -1);
            }
            n = 0;
            if (!*word) {
                return x;
            }
            *italic = (qboolean)!*italic;
            continue;
        }
        if (n < (int)sizeof(piece) - 1) {
            piece[n++] = *word;
        }
    }
}

static void TR_DrawParagraph(float x, float y, float width, const char* text, const smRect_t* clip) {
    trWord_t words[TR_MAX_WORDS];
    float space = SM_Advance(FONT_SANS, TR_TEXT_SIZE, " ", -1);
    qboolean measureItalic = qfalse, drawItalic = qfalse;
    int count = 0, first = 0;
    const char* p = text;

    while (*p && count < TR_MAX_WORDS) {
        int n = 0;

        while (*p == ' ') {
            p++;
        }
        while (*p && *p != ' ' && n < (int)sizeof(words[0].text) - 1) {
            words[count].text[n++] = *p++;
        }
        words[count].text[n] = '\0';
        if (n) {
            words[count].width = TR_WordWidth(words[count].text, &measureItalic);
            count++;
        }
    }

    while (first < count) {
        float used = words[first].width;
        float gap = space;
        float cx = x;
        int last = first;
        int i;

        while (last + 1 < count && used + space + words[last + 1].width <= width) {
            last++;
            used += space + words[last].width;
        }
        if (last + 1 < count && last > first) {
            gap = space + (width - used) / (last - first);
        }
        for (i = first; i <= last; i++) {
            TR_DrawWord(cx, SM_Baseline(FONT_SANS, TR_TEXT_SIZE, y, TR_LINE), words[i].text, &drawItalic, clip);
            cx += words[i].width + gap;
        }
        y += TR_LINE;
        first = last + 1;
    }
}

static float TR_Ease(float t) {
    return t * t * (3 - 2 * t);
}

static void TR_Button(float x, float y, float w, const char* label, qboolean over, const smRect_t* clip) {
    if (SM_Narrow()) {
        SM_WebButtonPadded(x, y, w, 0, 2.75f, label, qfalse, over, clip);
    } else {
        SM_WebButton(x, y, w, label, qfalse, over, clip);
    }
}

static void TR_DrawSection(int i, float x, float y, float w, const smRect_t* clip) {
    const trCard_t* card = &trCards[i];
    float f = TR_Ease(tr.hover[i]);
    vec4_t back = {0, 0, 0, 0.8f + 0.2f * f};
    vec4_t shade = {0, 0, 0, 0.6f * (1 - f)};
    float titleLine = SM_LineHeight(36.0f, 1.25f), subLine = SM_LineHeight(18.6667f, 1.3f);
    float ty = y + w + SM_StrutBelow(TR_TEXT_SIZE, TR_LINE);
    float pad = SM_Len(15.0f);
    int buttons = card->video ? 2 : 1;
    float bw = SM_Percent(w, 45.0f), gap = SM_Percent(w, 4.0f);
    float bx = x + SM_Center(w, buttons * bw + gap);
    float by = y + TR_SECTION_H - pad - SM_WEB_BUTTON_H;

    SM_Fill(x, y, w, TR_SECTION_H, back, clip);
    SM_Pic(x, y, w, w, tr.picture[i], clip);
    SM_Fill(x, y, w, w, shade, clip);

    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 36, ty, titleLine), 36, clip)) {
        SM_SetSpacing(SM_SPACING_H1);
        SM_Text(x + SM_Center(w, SM_Advance(FONT_DEFAULT, 36, card->title, -1)),
                SM_Baseline(FONT_DEFAULT, 36, ty, titleLine), FONT_DEFAULT, 36, trYellow, card->title, -1, qfalse);
        SM_SetSpacing(0);
    }
    ty += titleLine;
    if (SM_TextInside(SM_Baseline(FONT_SANS, 18.6667f, ty, subLine), 18.6667f, clip)) {
        SM_SetSpacing(SM_SPACING_H1);
        SM_Text(x + SM_Center(w, SM_AdvanceBold(FONT_SANS, 18.6667f, card->subtitle, -1, qtrue)),
                SM_Baseline(FONT_SANS, 18.6667f, ty, subLine), FONT_SANS, 18.6667f, smText, card->subtitle, -1, qtrue);
        SM_SetSpacing(0);
    }
    ty += subLine + SM_Rem(20.0f);
    TR_DrawParagraph(x + pad, ty + pad, w - 2 * pad, card->text, clip);

    SM_Hit(x, y, w, TR_SECTION_H, TR_HIT_CARD, i, clip);
    if (card->video) {
        TR_Button(bx, by, bw, "Watch Video", SM_Over(TR_HIT_VIDEO, i), clip);
        SM_Hit(bx, by, bw, SM_WEB_BUTTON_H, TR_HIT_VIDEO, i, clip);
        bx += bw + gap;
    }
    TR_Button(bx, by, bw, "Start Training", SM_Over(TR_HIT_START, i), clip);
    SM_Hit(bx, by, bw, SM_WEB_BUTTON_H, TR_HIT_START, i, clip);
}

static void TR_SectionAt(int i, float left, float top, float w, float margin, float* x, float* y) {
    if (i < TR_RETAIL_CARDS) {
        *x = left + i * (w + margin);
        *y = top;
    } else {
        *x = left + (i - TR_RETAIL_CARDS) * (w + margin);
        *y = top + TR_SECTION_H + margin + SM_TITLE_H + margin;
    }
}

void UI_Training_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;
    int i;

    if (!tr.loaded) {
        tr.loaded = qtrue;
        for (i = 0; i < TR_NUM_CARDS; i++) {
            tr.picture[i] = trCards[i].picture[0] ? trap_R_RegisterShaderNoMip(trCards[i].picture) : 0;
            if (!tr.picture[i]) {
                tr.picture[i] = trap_R_RegisterShaderNoMip(va("levelshots/%s", trCards[i].map));
            }
        }
    }
    for (i = 0; i < TR_NUM_CARDS; i++) {
        tr.hover[i] = 0;
    }
    tr.lastTime = uiInfo.uiDC.realTime;
    tr.scroll = 0;

    menu = Menus_FindByName("training");
    item = menu ? Menu_FindItemByName(menu, "training") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Training_Paint(void) {
    float left = UI_Home_Left();
    float width = HM_CONTAINER_W - SM_Len(TR_BAR);
    float w = SM_Percent(width, 30.0f);
    float margin = SM_Percent(width, 3.0f);
    float top, view, y, x, sy, dt;
    smRect_t clip;
    int i;

    SM_BeginFrame();
    if (!tr.loaded) {
        UI_Training_Open();
    }
    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_TRAINING);

    top = UI_Home_Header(HM_NAV_H, NULL, "Training Center");

    view = UI_Home_FrameBottom() - top;
    tr.content = margin + 2 * TR_SECTION_H + SM_TITLE_H + 2 * margin;
    if (tr.scroll > tr.content - view) {
        tr.scroll = tr.content - view;
    }
    if (tr.scroll < 0) {
        tr.scroll = 0;
    }
    clip = SM_R(left, top, HM_CONTAINER_W, view);
    y = top + margin - tr.scroll;

    dt = (uiInfo.uiDC.realTime - tr.lastTime) / 1000.0f;
    tr.lastTime = uiInfo.uiDC.realTime;
    for (i = 0; i < TR_NUM_CARDS; i++) {
        smRect_t box;

        TR_SectionAt(i, left, y, w, margin, &x, &sy);
        box = SM_R(x, sy, w, TR_SECTION_H);
        tr.hover[i] += SM_In(&box, smMX, smMY) && SM_In(&clip, smMX, smMY) ? dt : -dt;
        if (tr.hover[i] < 0) {
            tr.hover[i] = 0;
        } else if (tr.hover[i] > 1) {
            tr.hover[i] = 1;
        }
    }

    for (i = 0; i < TR_NUM_CARDS; i++) {
        TR_SectionAt(i, left, y, w, margin, &x, &sy);
        TR_DrawSection(i, x, sy, w, &clip);
    }

    sy = y + TR_SECTION_H + margin;
    SM_Fill(left, sy, HM_CONTAINER_W, SM_TITLE_H, trBlack80, &clip);
    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 32.0f, sy + SM_H1_PAD, SM_H1_LINE), 32.0f, &clip)) {
        SM_SetSpacing(SM_SPACING_H1);
        SM_Text(left + SM_Len(6.4f), SM_Baseline(FONT_DEFAULT, 32.0f, sy + SM_H1_PAD, SM_H1_LINE), FONT_DEFAULT, 32.0f,
                smText, "Advanced Training", -1, qfalse);
        SM_SetSpacing(0);
    }

    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Training_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        tr.scroll += key == K_MWHEELUP ? -60.0f : 60.0f;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    switch (hit->type) {
        case TR_HIT_CARD:
        case TR_HIT_START:
            TR_Launch(hit->index);
            break;
        case TR_HIT_VIDEO:
            trap_Sys_OpenURL(trCards[hit->index].video);
            break;
        default:
            break;
    }
    return qtrue;
}
