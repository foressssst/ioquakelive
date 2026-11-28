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
    TR_HIT_VIDEO,
    TR_HIT_VIDEO_BLOCK,
    TR_HIT_VIDEO_CLOSE,
    TR_HIT_VIDEO_SCREEN,
    TR_HIT_VIDEO_PLAY
} trHitType_t;

typedef struct {
    const char* map;
    const char* picture;
    const char* title;
    const char* subtitle;
    const char* text;
    const char* command;
    const char* video;
    float length;
} trCard_t;

static const trCard_t trCards[] = {
    {"qztraining", "webui/img/training/training.jpg", "Crash Course", "Welcome to the Arena",
     "Meet _Crash_, our QUAKE LIVE trainer, and get a guided tour of some of the basic items of combat.",
     "map qztraining _training", NULL, 0},
    {"qzpractice2", "webui/img/training/strafe.jpg", "Accelerate", "Travelling at High Speed",
     "Practice the basics of _Strafe Jumping_, an essential skill to learning how to move faster than other arena "
     "combatants.",
     "map qzpractice2 _sj; wait 2; team f", "strafe.mpg", 110.65f},
    {"qzpractice1", "webui/img/training/rocket.jpg", "Elevate", "Reaching Great Heights",
     "Use your projectile weapons to perform _Rocket Jumps_, and access areas of the arena in entirely different "
     "ways.",
     "map qzpractice1 _rj; wait 2; team f", "rocket.mpg", 104.83f},
    {"thunderstruck", "", "Tracking", "Keeping the Beam On",
     "Keep the _Lightning Gun_ on a bot that does nothing but dodge. It never fights back and never goes "
     "down.",
     "map thunderstruck _lgtrack; wait 2; team f; addbot crash 5", NULL, 0},
};
#define TR_NUM_CARDS ((int)ARRAY_LEN(trCards))

static struct {
    qboolean loaded;
    qhandle_t picture[TR_NUM_CARDS];
    float hover[TR_NUM_CARDS];
    int lastTime;
    float scroll;
    float content;
    qboolean videoOpen;
    int video;
    qboolean cinOn;
    int cin;
    int extents[4];
    qboolean paused;
    qboolean ended;
    qboolean failed;
    float played;
    float progress;
} tr;

static const vec4_t trYellow = SM_RGB(247, 209, 0);
static const vec4_t trBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};

#define TR_VIDEO_W 1280.0f
#define TR_VIDEO_PAD 15.0f
#define TR_VIDEO_TITLE 57.6f
#define TR_VIDEO_ROW 24.0f
#define TR_VIDEO_GAP 15.0f
#define TR_VIDEO_GLYPH 14.0f
#define TR_VIDEO_BIG 36.0f
#define TR_VIDEO_STALL 0.25f

static const vec4_t trDialog = SM_RGB(30, 30, 30);

static void TR_VideoClose(void) {
    if (tr.cinOn) {
        trap_CIN_StopCinematic(tr.cin);
    }
    tr.cinOn = qfalse;
    tr.videoOpen = qfalse;
}

static void TR_VideoStart(void) {
    tr.cin = trap_CIN_PlayCinematic(trCards[tr.video].video, 0, 0, 0, 0, CIN_hold);
    tr.cinOn = tr.cin >= 0 ? qtrue : qfalse;
    tr.failed = !tr.cinOn;
    tr.extents[2] = 0;
    tr.played = 0;
    tr.paused = qfalse;
    tr.ended = qfalse;
}

static void TR_VideoOpen(int i) {
    TR_VideoClose();
    tr.videoOpen = qtrue;
    tr.video = i;
    tr.failed = qfalse;
    tr.paused = qfalse;
    tr.ended = qfalse;
    tr.played = 0;
    tr.progress = 0;
    trap_Cmd_ExecuteText(EXEC_NOW, va("videofetch %s\n", trCards[i].video));
}

static void TR_VideoToggle(void) {
    if (!tr.cinOn) {
        return;
    }
    if (tr.ended) {
        trap_CIN_StopCinematic(tr.cin);
        TR_VideoStart();
        return;
    }
    tr.paused = !tr.paused;
}

static void TR_VideoUpdate(float dt) {
    if (!tr.cinOn) {
        if (!tr.failed) {
            tr.progress = trap_Cvar_VariableValue("cl_videoProgress");
            if (tr.progress < 0) {
                tr.failed = qtrue;
            } else if (tr.progress >= 100) {
                TR_VideoStart();
            }
        }
        return;
    }
    if (tr.paused || tr.ended) {
        return;
    }
    switch (trap_CIN_RunCinematic(tr.cin)) {
        case FMV_IDLE:
            tr.ended = qtrue;
            tr.played = trCards[tr.video].length;
            break;
        case FMV_EOF:
            tr.cinOn = qfalse;
            tr.failed = qtrue;
            break;
        default:
            if (dt < TR_VIDEO_STALL) {
                tr.played += dt;
            }
            break;
    }
    if (tr.played > trCards[tr.video].length) {
        tr.played = trCards[tr.video].length;
    }
}

static void TR_Triangle(float x, float y, float size, const vec4_t color) {
    int rows = (int)(size * smScale + 0.5f);
    int r;

    for (r = 0; r < rows; r++) {
        float t = (r + 0.5f) / rows;

        SM_Fill(x, y + r / smScale, (t < 0.5f ? t : 1.0f - t) * 2.0f * size * 0.866f, 1.0f / smScale, color, NULL);
    }
}

static void TR_Glyph(float x, float y, float size, qboolean pause, const vec4_t color) {
    if (pause) {
        SM_Fill(x + size * 0.15f, y, size * 0.25f, size, color, NULL);
        SM_Fill(x + size * 0.6f, y, size * 0.25f, size, color, NULL);
    } else {
        TR_Triangle(x + size * 0.15f, y, size, color);
    }
}

static void TR_Loader(float x, float y) {
    float size = SM_Len(24.0f), thick = SM_LenBorder(4.0f);
    float r = (size - thick) * 0.5f;
    float cx = x + size * 0.5f, cy = y + size * 0.5f;
    float turn = (uiInfo.uiDC.realTime % 750) / 750.0f * 2 * M_PI;
    int i;

    for (i = 0; i < 36; i++) {
        float a = i * (2 * M_PI / 36);
        float rel = a - turn;

        rel = rel - 2 * M_PI * floor(rel / (2 * M_PI));
        if (rel > M_PI * 0.25f && rel < M_PI * 0.75f) {
            continue;
        }
        SM_Fill(cx + cos(a) * r - thick * 0.5f, cy + sin(a) * r - thick * 0.5f, thick, thick, trYellow, NULL);
    }
}

static void TR_DrawVideo(void) {
    const trCard_t* card = &trCards[tr.video];
    float side = smVW * 0.15f, width = smVW * 0.7f;
    float inner = width - 2 * TR_VIDEO_PAD;
    float vw = inner < TR_VIDEO_W ? inner : TR_VIDEO_W;
    float vh = vw * 9.0f / 16.0f;
    float room = smBottom - 2 * TR_VIDEO_GAP - (2 * TR_VIDEO_PAD + TR_VIDEO_TITLE + TR_VIDEO_GAP + TR_VIDEO_ROW);
    float h, top, x, y, vx, vy, sx, sy, row, tw, tx, fill;
    int e[4];
    smRect_t band;
    char time[32];

    if (vh > room) {
        vh = room;
        vw = vh * 16.0f / 9.0f;
    }
    h = 2 * TR_VIDEO_PAD + TR_VIDEO_TITLE + vh + TR_VIDEO_GAP + TR_VIDEO_ROW;
    top = smBottom * 0.5f - h * 0.5f;
    x = side + TR_VIDEO_PAD;
    y = top + TR_VIDEO_PAD;
    band = SM_R(0, top, smVW, h);

    SM_Fill(0, 0, smVW, smBottom, trBlack80, NULL);
    SM_Hit(0, 0, smVW, smBottom, TR_HIT_VIDEO_BLOCK, 0, NULL);
    SM_Fill(0, top, smVW, h, trDialog, NULL);
    if (tr.picture[tr.video]) {
        SM_Pic(0, top + h * 0.5f - smVW * 0.5f, smVW, smVW, tr.picture[tr.video], &band);
    }
    SM_Fill(side, top, width, h, trBlack80, NULL);

    y += TR_VIDEO_TITLE;

    sx = 640.0f / uiInfo.uiDC.glconfig.vidWidth;
    sy = 480.0f / uiInfo.uiDC.glconfig.vidHeight;
    e[0] = (int)((x + (inner - vw) * 0.5f) * smScale * sx + 0.5f);
    e[1] = (int)((smTop + y * smScale) * sy + 0.5f);
    e[2] = (int)(vw * smScale * sx + 0.5f);
    e[3] = (int)(vh * smScale * sy + 0.5f);
    vx = e[0] / sx / smScale;
    vy = (e[1] / sy - smTop) / smScale;
    vw = e[2] / sx / smScale;
    vh = e[3] / sy / smScale;

    SM_TextRaw(vx + vw - SM_Advance(FONT_SANS, 32, "\xc3\x97", -1),
               SM_Baseline(FONT_SANS, 32, vy - TR_VIDEO_TITLE, 18.6667f), FONT_SANS, 32, trYellow, "\xc3\x97", -1, qfalse);
    SM_Hit(vx + vw - 24, vy - TR_VIDEO_TITLE - 8, 24, 32, TR_HIT_VIDEO_CLOSE, 0, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(vx, SM_Baseline(FONT_DEFAULT, 48, vy - TR_VIDEO_TITLE, TR_VIDEO_TITLE), FONT_DEFAULT, 48, smText, card->title,
            -1, qfalse);
    SM_SetSpacing(0);
    SM_Fill(vx, vy, vw, vh, smPage, NULL);
    if (tr.cinOn) {
        if (memcmp(e, tr.extents, sizeof(e))) {
            memcpy(tr.extents, e, sizeof(e));
            trap_CIN_SetExtents(tr.cin, e[0], e[1], e[2], e[3]);
        }
        trap_CIN_DrawCinematic(tr.cin);
        if (tr.paused || tr.ended) {
            float box = TR_VIDEO_BIG * 2;

            SM_Fill(vx + (vw - box) * 0.5f, vy + (vh - box) * 0.5f, box, box, trBlack80, NULL);
            TR_Glyph(vx + (vw - TR_VIDEO_BIG) * 0.5f + TR_VIDEO_BIG * 0.1f, vy + (vh - TR_VIDEO_BIG) * 0.5f,
                     TR_VIDEO_BIG, qfalse, smText);
        }
    } else if (tr.failed) {
        const char* text = "Video unavailable";

        SM_Text(vx + SM_Center(vw, SM_Advance(FONT_SANS, 21.3333f, text, -1)),
                SM_Baseline(FONT_SANS, 21.3333f, vy + vh * 0.5f - 17.0f, 34.0f), FONT_SANS, 21.3333f, smText, text, -1,
                qfalse);
    } else {
        TR_Loader(vx + vw * 0.5f - SM_Len(12.0f), vy + vh * 0.5f - SM_Len(12.0f));
    }
    SM_Hit(vx, vy, vw, vh, TR_HIT_VIDEO_SCREEN, 0, NULL);

    row = vy + vh + TR_VIDEO_GAP;
    if (tr.cinOn) {
        int now = (int)tr.played, all = (int)trCards[tr.video].length;

        Com_sprintf(time, sizeof(time), "%i:%02i / %i:%02i", now / 60, now % 60, all / 60, all % 60);
        fill = trCards[tr.video].length > 0 ? tr.played / trCards[tr.video].length : 0;
    } else {
        Com_sprintf(time, sizeof(time), "%i%%", tr.failed ? 0 : (int)tr.progress);
        fill = tr.failed ? 0 : tr.progress / 100.0f;
    }
    TR_Glyph(vx + (TR_VIDEO_ROW - TR_VIDEO_GLYPH) * 0.5f, row + (TR_VIDEO_ROW - TR_VIDEO_GLYPH) * 0.5f, TR_VIDEO_GLYPH,
             tr.cinOn && !tr.paused && !tr.ended, SM_Over(TR_HIT_VIDEO_PLAY, 0) ? trYellow : smText);
    SM_Hit(vx, row, TR_VIDEO_ROW, TR_VIDEO_ROW, TR_HIT_VIDEO_PLAY, 0, NULL);
    tw = SM_Advance(FONT_SANS, 15, time, -1);
    SM_Text(vx + vw - tw, SM_Baseline(FONT_SANS, 15, row, TR_VIDEO_ROW), FONT_SANS, 15, smText, time, -1, qfalse);
    tx = vx + TR_VIDEO_ROW + 12;
    tw = vw - (tx - vx) - tw - 12;
    SM_Fill(tx, row + TR_VIDEO_ROW * 0.5f - 2, tw, 4, smBorder, NULL);
    if (fill > 0) {
        SM_Fill(tx, row + TR_VIDEO_ROW * 0.5f - 2, tw * (fill > 1 ? 1 : fill), 4, trYellow, NULL);
    }
}

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
    TR_VideoClose();

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
        tr.hover[i] += !tr.videoOpen && SM_In(&box, smMX, smMY) && SM_In(&clip, smMX, smMY) ? dt : -dt;
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
    if (tr.videoOpen) {
        TR_VideoUpdate(dt);
        TR_DrawVideo();
    }
    SM_EndFrame();
}

static qboolean TR_VideoKey(int key) {
    const smHit_t* hit;

    if (key == K_ESCAPE) {
        TR_VideoClose();
    } else if (key == K_SPACE) {
        TR_VideoToggle();
    } else if (key == K_MOUSE1 && (hit = SM_HitAt(smMX, smMY)) != NULL) {
        if (hit->type == TR_HIT_VIDEO_CLOSE) {
            TR_VideoClose();
        } else if (hit->type == TR_HIT_VIDEO_SCREEN || hit->type == TR_HIT_VIDEO_PLAY) {
            TR_VideoToggle();
        }
    }
    return qtrue;
}

qboolean UI_Training_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (tr.videoOpen) {
        return TR_VideoKey(key);
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
            TR_VideoOpen(hit->index);
            break;
        default:
            break;
    }
    return qtrue;
}
