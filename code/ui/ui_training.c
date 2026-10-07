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
// ui_training.c -- [QL] the Training Center page
//
// Retail's is web.pak's module 267, reached from Play > Training Center: the page's
// title and three sections side by side, each a square picture, a yellow title, a
// line under it, a justified paragraph and the buttons it ends with. Drawn as
// css/main.css lays it out (.training): each section 30% of the page wide with 3% of
// margin, 675 high, black at 80% that goes solid under the cursor while the picture
// comes up from 40%, both over a second.
//
// What each one sends (bot_minplayers and sv_serverType go to 0 first):
//
//     Crash Course  map qztraining _training
//     Accelerate    map qzpractice2 _sj; wait 2; team f
//     Elevate       map qzpractice1 _rj; wait 2; team f
//
// [ioquakelive] Under them, the advanced room, and the page scrolls to it:
//
//     Tracking      map thunderstruck _lgtrack; wait 2; team f; addbot crash 5
//
// _lgtrack is pak01's (scripts/ioql.factories).
//
// The three factories are pak00's hidden ones, which set g_training 1 and, for
// the practice maps, infinite ammo and one weapon. The pictures are the page's own,
// read from the player's web.pak.
//

#include "ui_web.h"

#define TR_SECTION_H 675.0f
#define TR_RETAIL_CARDS 3   // retail's row; the advanced room's go under "Advanced Training"
#define TR_HEADING_H 54.4f  // that heading, as UI_Home_Header draws the page's
#define TR_BAR 8.0f  // the page is overflow-y: scroll, so its bar's gutter is always there

typedef enum {
    TR_HIT_NONE,
    TR_HIT_CARD,
    TR_HIT_START,
    TR_HIT_VIDEO
} trHitType_t;

typedef struct {
    const char* map;
    const char* picture;   // the section's img
    const char* title;     // its h2
    const char* subtitle;  // its h3
    const char* text;      // its paragraph, the em between underscores
    const char* command;   // what launchTraining sends for it
    const char* video;     // what its Watch Video opens, NULL when it has none
} trCard_t;

/*
The videos are wired as retail wires them: Accelerate's button calls watchRocketVideo,
which opens HnIeHqBk8v4, and Elevate's calls watchStrafeVideo, which opens pVs1KGK0VLY.
The names say each opens the other section's video.
*/
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
    // [ioquakelive] the advanced room; no picture of its own, so the map's levelshot
    {"thunderstruck", "", "Tracking", "Keeping the Beam On",
     "Keep the _Lightning Gun_ on a bot that does nothing but dodge. It never fights back and never goes "
     "down.",
     "map thunderstruck _lgtrack; wait 2; team f; addbot crash 5", NULL},
};
#define TR_NUM_CARDS ((int)ARRAY_LEN(trCards))

static struct {
    qboolean loaded;
    qhandle_t picture[TR_NUM_CARDS];
    float hover[TR_NUM_CARDS];  // how far each section's transition has gone, 0 to 1
    int lastTime;
    float scroll;   // [ioquakelive] how far the sections are scrolled
    float content;  // and how tall they are
} tr;

static const vec4_t trYellow = SM_RGB(247, 209, 0);
static const vec4_t trBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};

/*
===============
TR_Launch

What retail's launchTraining does, in its order: the two cvars first, then the
map. Training is an offline match, so the server type goes back to 0 and no bots
are filled in.
===============
*/
static void TR_Launch(int i) {
    trap_Cvar_Set("bot_minplayers", "0");
    trap_Cvar_Set("sv_serverType", "0");
    trap_Cmd_ExecuteText(EXEC_APPEND, va("%s\n", trCards[i].command));
}

/*
===============
The paragraph

text-align: justify with an em inside: the words go on a line while they fit, and every
line but the last spreads its spaces to fill the width. A word is one or more pieces, the
em's in italics, and the underscores say where the em starts and ends.
===============
*/
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

    // the words, each measured with the italics it carries
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

    // the lines: as many words as fit, spread to the width unless it is the last
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
            TR_DrawWord(cx, SM_Baseline(FONT_SANS, TR_TEXT_SIZE, y, 24), words[i].text, &drawItalic, clip);
            cx += words[i].width + gap;
        }
        y += 24;
        first = last + 1;
    }
}

// CSS's "ease" over the second the section takes to change
static float TR_Ease(float t) {
    return t * t * (3 - 2 * t);
}

// .actions button: on a screen no wider than 3:2 its padding is 0 0.25em 1em 0
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
    float ty = y + w + 6;
    int buttons = card->video ? 2 : 1;
    float bw = w * 0.45f;
    float bx = x + (w - (buttons * bw + w * 0.04f)) * 0.5f;
    float by = y + TR_SECTION_H - 15 - SM_WEB_BUTTON_H;

    SM_Fill(x, y, w, TR_SECTION_H, back, clip);
    // the picture's opacity, as black over it: the shader of a plain image takes no alpha
    SM_Pic(x, y, w, w, tr.picture[i], clip);
    SM_Fill(x, y, w, w, shade, clip);

    // the h2 in Handel Gothic and yellow, the h3 under it in bold, both centred
    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 36, ty, 45), 36, clip)) {
        SM_Text(x + (w - SM_Advance(FONT_DEFAULT, 36, card->title, -1)) * 0.5f, SM_Baseline(FONT_DEFAULT, 36, ty, 45),
                FONT_DEFAULT, 36, trYellow, card->title, -1, qfalse);
    }
    ty += 45;
    if (SM_TextInside(SM_Baseline(FONT_SANS, 18.6667f, ty, 24.3f), 18.6667f, clip)) {
        SM_Text(x + (w - SM_AdvanceBold(FONT_SANS, 18.6667f, card->subtitle, -1, qtrue)) * 0.5f,
                SM_Baseline(FONT_SANS, 18.6667f, ty, 24.3f), FONT_SANS, 18.6667f, smText, card->subtitle, -1, qtrue);
    }
    ty += 24.3f + 20;
    TR_DrawParagraph(x + 15, ty + 15, w - 30, card->text, clip);

    // the whole section launches it, and so do its buttons; Watch Video does not go on to it
    SM_Hit(x, y, w, TR_SECTION_H, TR_HIT_CARD, i, clip);
    if (card->video) {
        TR_Button(bx, by, bw, "Watch Video", SM_Over(TR_HIT_VIDEO, i), clip);
        SM_Hit(bx, by, bw, SM_WEB_BUTTON_H, TR_HIT_VIDEO, i, clip);
        bx += bw;
    }
    // the first of them keeps a margin of 4% on its right
    if (card->video) {
        bx += w * 0.04f;
    }
    TR_Button(bx, by, bw, "Start Training", SM_Over(TR_HIT_START, i), clip);
    SM_Hit(bx, by, bw, SM_WEB_BUTTON_H, TR_HIT_START, i, clip);
}

// [ioquakelive] retail's row first, then the advanced room's under its heading
static void TR_SectionAt(int i, float left, float top, float w, float margin, float* x, float* y) {
    if (i < TR_RETAIL_CARDS) {
        *x = left + i * (w + margin);
        *y = top;
    } else {
        *x = left + (i - TR_RETAIL_CARDS) * (w + margin);
        *y = top + TR_SECTION_H + margin + TR_HEADING_H + margin;
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

    // the page takes keys before the mouse moves over it
    menu = Menus_FindByName("training");
    item = menu ? Menu_FindItemByName(menu, "training") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Training_Paint(void) {
    float left = UI_Home_Left();
    float width = HM_CONTAINER_W - TR_BAR;
    float w = width * 0.3f;
    float margin = width * 0.03f;
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

    // [ioquakelive] the sections scroll under the header
    view = UI_Home_FrameBottom() - top;
    tr.content = margin + 2 * TR_SECTION_H + TR_HEADING_H + 2 * margin;
    if (tr.scroll > tr.content - view) {
        tr.scroll = tr.content - view;
    }
    if (tr.scroll < 0) {
        tr.scroll = 0;
    }
    clip = SM_R(left, top, HM_CONTAINER_W, view);
    y = top + margin - tr.scroll;

    // each section eases toward what the cursor says over a second
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

    // the advanced room's heading, as the page's
    sy = y + TR_SECTION_H + margin;
    SM_Fill(left, sy, HM_CONTAINER_W, TR_HEADING_H, trBlack80, &clip);
    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 32.0f, sy + 8.0f, 38.4f), 32.0f, &clip)) {
        SM_Text(left + 6.4f, SM_Baseline(FONT_DEFAULT, 32.0f, sy + 8.0f, 38.4f), FONT_DEFAULT, 32.0f, smText,
                "Advanced Training", -1, qfalse);
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
        return qfalse;  // Escape goes back through the menu's onESC
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
            // retail opens it in the Steam overlay; without Steam it goes to the
            // browser, which is what OpenSteamOverlayURL falls back to anyway
            trap_Sys_OpenURL(trCards[hit->index].video);
            break;
        default:
            break;
    }
    return qtrue;
}
