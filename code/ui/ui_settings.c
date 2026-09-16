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
// ui_settings.c -- [QL] the Settings page
//
// Retail's Settings button hands the screen to the embedded browser
// ("web_changeHash /settings", ingame.menu's navbtn6), so the real settings page
// is in web.pak and there is nothing native to port. It is drawn here instead,
// with ui_web.c, the way Start Match already is.
//
// Everything it offers is retail's: the ten sections in the order its nav lists
// them, every cvar in the order its page lists it, and the title, widget, range
// and value list the web UI gives each one, all generated into ui_settings_table.h
// straight out of web.pak. The look follows web.pak's own stylesheet -- Handel
// Gothic headings, Noto Sans body, #f7d100 for what is chosen, #242121 and #1C1919
// rows, #3C3939 under the cursor -- with the layout brought up to date: the page
// is centred, the sections are one row of buttons, and each row says what it does.
//

#include "ui_web.h"

#define ST_MAX_INPUT 200

#define ST_PAGE_MAX_W 1360.0f  // wider than this and the two columns read as two pages
#define ST_PAGE_MARGIN 60.0f
#define ST_PAGE_TOP 96.0f
#define ST_PAGE_BOTTOM 40.0f

#define ST_TITLE_H 56.0f
#define ST_NAV_H 38.0f
#define ST_NAV_GAP 6.0f
#define ST_LIST_GAP 22.0f
#define ST_TIP_H 34.0f

#define ST_ROW_H 50.0f     // .cvar
#define ST_BIND_H 38.0f    // .bind
#define ST_HEAD_H 46.0f
#define ST_COL_GAP 24.0f
#define ST_VAL_FRAC 0.46f  // of the column, where the widget starts

typedef enum {
    ST_TAB_INPUT,
    ST_TAB_GAMEPAD,
    ST_TAB_BASIC,
    ST_TAB_GAME,
    ST_TAB_HUD,
    ST_TAB_TEAM,
    ST_TAB_WEAPONS,
    ST_TAB_VIDEO,
    ST_TAB_SOUND,
    ST_TAB_SPECTATING,
    ST_NUM_TABS
} stTab_t;

typedef enum {
    ST_HEADING,    // a section title, not an option
    ST_BOOL,       // Off / On
    ST_SELECT,     // a value out of a list
    ST_RANGE,      // a slider
    ST_INPUT,      // a line of text
    ST_PALETTE,    // one of the 26 Quake Live colours
    ST_RGB,        // a colour of its own, as 0xRRGGBBAA
    ST_MODEL,      // a player model
    ST_MODELSKIN,  // a player model and its skin
    ST_CROSSHAIR,  // the crosshair itself
    ST_BIND        // a key bound to a command
} stWidget_t;

typedef struct {
    const char* label;
    const char* value;
} stOption_t;

typedef struct {
    int tab;
    const char* title;
    const char* cvar;   // NULL on a heading
    int widget;
    float min, max, step;
    const stOption_t* options;
    int numOptions;
    const char* tip;
    int column;         // which of the page's two columns the row sits in
} stCvar_t;

#include "ui_settings_table.h"

#define ST_NUM_CVARS ((int)ARRAY_LEN(stCvars))

/*
===============================================================================

WHAT THE CURSOR IS OVER

===============================================================================
*/

typedef enum {
    ST_HIT_NONE,
    ST_HIT_TAB,
    ST_HIT_SELECT,
    ST_HIT_MENU,
    ST_HIT_OPTION,
    ST_HIT_RANGE,
    ST_HIT_INPUT,
    ST_HIT_PALETTE,
    ST_HIT_RGB,
    ST_HIT_BOOL,
    ST_HIT_BIND,
    ST_HIT_PREV,
    ST_HIT_NEXT,
    ST_HIT_ROW,
    ST_HIT_TRACK,
    ST_HIT_THUMB,
    ST_HIT_CLOSE,
    ST_HIT_RESET,
    ST_HIT_RGBSLIDER,
    ST_HIT_RGBCLOSE
} stHitType_t;

/*
===============================================================================

STATE

===============================================================================
*/

static struct {
    qboolean loaded;
    int tab;
    float scroll[ST_NUM_TABS];

    char values[ST_NUM_CVARS][MAX_CVAR_VALUE_STRING];

    int select, selectFocus;
    smRect_t selectControl, selectMenu;
    float selectScroll;
    int drag;
    int edit, editCursor, editStart, editTime;
    int binding;
    int rgb;              // the row whose colour picker is open
    int rgbDrag;          // 0..2 while a channel is being dragged, else -1
    qboolean scrollDrag;
    float scrollGrab;

    float page, pageW, colW, valX, valW;  // the frame, worked out once a frame
    float listTop, view, content;
    const char* tip;
    const char* tipCvar;
    qboolean fromIngame;
} st;

// in a match the browser tinted the game rather than hiding it
static const vec4_t stIngameTint = {19 / 255.0f, 68 / 255.0f, 92 / 255.0f, 0.7f};
static const vec4_t stBlack = {0.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t stNavBg = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t stOff = {0.6f, 0.6f, 0.6f, 1.0f};  // #999, an option that is off

extern const vec4_t* UI_CrosshairPalette(void);

/*
===============================================================================

THE CVARS

===============================================================================
*/

static void ST_ReadBind(int i) {
    const char* command = stCvars[i].cvar;
    int count = 0;
    int k;

    st.values[i][0] = '\0';
    for (k = 0; k < K_LAST_KEY && count < 2; k++) {
        char buf[MAX_STRING_CHARS];
        char name[32];

        trap_Key_GetBindingBuf(k, buf, sizeof(buf));
        if (!buf[0] || Q_stricmp(buf, command)) {
            continue;
        }
        trap_Key_KeynumToStringBuf(k, name, sizeof(name));
        Q_strupr(name);
        if (count) {
            Q_strcat(st.values[i], sizeof(st.values[i]), ", ");
        }
        Q_strcat(st.values[i], sizeof(st.values[i]), name);
        count++;
    }
    if (!count) {
        Q_strncpyz(st.values[i], "None", sizeof(st.values[i]));
    }
}

static void ST_Read(int i) {
    if (!stCvars[i].cvar) {
        return;
    }
    if (stCvars[i].widget == ST_BIND) {
        ST_ReadBind(i);
        return;
    }
    trap_Cvar_VariableStringBuffer(stCvars[i].cvar, st.values[i], sizeof(st.values[i]));
}

static void ST_ReadAll(void) {
    int i;

    for (i = 0; i < ST_NUM_CVARS; i++) {
        ST_Read(i);
    }
}

static void ST_Set(int i, const char* value) {
    trap_Cvar_Set(stCvars[i].cvar, value);
    Q_strncpyz(st.values[i], value, sizeof(st.values[i]));
}

// a slider's value written the way its step implies
static void ST_FormatStep(const stCvar_t* cv, float value, char* out, int size) {
    float step = cv->step > 0 ? cv->step : 1.0f;

    if (step >= 1.0f) {
        Com_sprintf(out, size, "%i", (int)(value + (value < 0 ? -0.5f : 0.5f)));
    } else if (step >= 0.1f) {
        Com_sprintf(out, size, "%.2f", value);
    } else {
        Com_sprintf(out, size, "%.3f", value);
    }
}

static float ST_Snap(const stCvar_t* cv, float value) {
    float step = cv->step;

    if (step > 0) {
        value = cv->min + step * (float)floor((value - cv->min) / step + 0.5f);
    }
    if (value < cv->min) {
        value = cv->min;
    } else if (value > cv->max) {
        value = cv->max;
    }
    return value;
}

static float ST_Fraction(const stCvar_t* cv, float value) {
    float frac;

    if (cv->max <= cv->min) {
        return 0;
    }
    frac = (value - cv->min) / (cv->max - cv->min);
    return frac < 0 ? 0 : frac > 1 ? 1 : frac;
}

// what the row shows on the right
static const char* ST_Shown(int i) {
    const stCvar_t* cv = &stCvars[i];
    int k;

    if (cv->widget == ST_SELECT) {
        for (k = 0; k < cv->numOptions; k++) {
            if (!Q_stricmp(cv->options[k].value, st.values[i])) {
                return cv->options[k].label;
            }
        }
        return st.values[i][0] ? st.values[i] : "Custom";
    }
    return st.values[i];
}

// 0xRRGGBBAA, the way the web UI writes a colour of its own
static void ST_ParseRGB(const char* text, int* rgb) {
    unsigned int value = 0;

    rgb[0] = rgb[1] = rgb[2] = 255;
    if (!text || !text[0]) {
        return;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }
    while (*text) {
        int digit;

        if (*text >= '0' && *text <= '9') {
            digit = *text - '0';
        } else if (*text >= 'a' && *text <= 'f') {
            digit = *text - 'a' + 10;
        } else if (*text >= 'A' && *text <= 'F') {
            digit = *text - 'A' + 10;
        } else {
            break;
        }
        value = value * 16 + digit;
        text++;
    }
    rgb[0] = (value >> 24) & 0xff;
    rgb[1] = (value >> 16) & 0xff;
    rgb[2] = (value >> 8) & 0xff;
}

static void ST_SetRGB(int i, const int* rgb) {
    char text[16];

    Com_sprintf(text, sizeof(text), "0x%02X%02X%02XFF", rgb[0] & 0xff, rgb[1] & 0xff, rgb[2] & 0xff);
    ST_Set(i, text);
}

/*
===============================================================================

THE LIST

Each row says which of the two columns it belongs to; a heading spans both and
lines them up again. Drawing and hit testing read the same layout, worked out
once a frame, so they can never disagree about where a row is.

===============================================================================
*/

static struct {
    float x, y, w, h;
    int nth;
} stLayout[ST_NUM_CVARS];

static qboolean ST_InTab(int i) {
    return stCvars[i].tab == st.tab;
}

static float ST_RowHeight(int i) {
    switch (stCvars[i].widget) {
        case ST_HEADING:
            return ST_HEAD_H;
        case ST_BIND:
            return ST_BIND_H;
        default:
            return ST_ROW_H;
    }
}

static void ST_Layout(void) {
    float y[2];
    float pairTop = -1;
    int nth[2];
    int i;

    y[0] = y[1] = 0;
    nth[0] = nth[1] = 1;
    for (i = 0; i < ST_NUM_CVARS; i++) {
        int c;

        if (!ST_InTab(i)) {
            continue;
        }
        if (stCvars[i].widget != ST_HEADING) {
            pairTop = -1;
        }
        if (stCvars[i].widget == ST_HEADING) {
            float top = y[0] > y[1] ? y[0] : y[1];
            int col = stCvars[i].column;

            if (top > 0) {
                top += 12.0f;
            }
            // two column headings in a row are the two panels' titles: same line
            if (col >= 0 && pairTop >= 0) {
                top = pairTop;
            }
            pairTop = col >= 0 ? top : -1;
            // a heading with a column of its own sits over that column only, the way
            // the Team and Weapons pages head each of their two panels
            stLayout[i].x = col < 0 ? st.page : st.page + col * (st.colW + ST_COL_GAP);
            stLayout[i].y = top;
            stLayout[i].w = col < 0 ? st.pageW : st.colW;
            stLayout[i].h = ST_HEAD_H;
            stLayout[i].nth = 0;
            y[0] = y[1] = top + ST_HEAD_H;
            nth[0] = nth[1] = 1;
            continue;
        }
        c = stCvars[i].column ? 1 : 0;
        stLayout[i].x = st.page + c * (st.colW + ST_COL_GAP);
        stLayout[i].y = y[c];
        stLayout[i].w = st.colW;
        stLayout[i].h = ST_RowHeight(i);
        stLayout[i].nth = nth[c]++;
        y[c] += stLayout[i].h;
    }
    st.content = y[0] > y[1] ? y[0] : y[1];
}

static float ST_ClampScroll(float scroll) {
    if (scroll > st.content - st.view) {
        scroll = st.content - st.view;
    }
    return scroll < 0 ? 0 : scroll;
}

/*
===============================================================================

DRAWING

===============================================================================
*/

static void ST_DrawSelect(int i, float x, float y, const smRect_t* clip) {
    qboolean open = st.select == i;
    const char* label = ST_Shown(i);

    SM_Fill(x, y, st.valW, 34, (!open && SM_Over(ST_HIT_SELECT, i)) ? smLighter : smGrey, clip);
    SM_Fill(x + 1, y + 1, st.valW - 2, 32, open ? smDark : smInput, clip);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smSelectText, label,
            SM_Fit(FONT_SANS, 15, label, st.valW - 40), qfalse);
    SM_Arrow(x + st.valW - 24, y + 16, open);
    SM_Hit(x, y, st.valW, 34, ST_HIT_SELECT, i, clip);
    if (open) {
        st.selectControl = SM_R(x, y, st.valW, 34);
    }
}

// retail's checkbox, which writes Off in grey and On in white
static void ST_DrawBool(int i, float x, float y, const smRect_t* clip) {
    qboolean on = atoi(st.values[i]) != 0;
    qboolean over = SM_Over(ST_HIT_BOOL, i);

    SM_Fill(x, y, 54, 34, over ? smLighter : smGrey, clip);
    SM_Fill(x + 1, y + 1, 52, 32, smInput, clip);
    SM_Text(x + 12, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, on ? smText : stOff, on ? "On" : "Off", -1,
            on);
    SM_Hit(x, y, 54, 34, ST_HIT_BOOL, i, clip);
}

static void ST_DrawRange(int i, float x, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float bw = st.valW - 62;
    float value = ST_Snap(cv, (float)atof(st.values[i]));
    float frac = ST_Fraction(cv, value);
    char text[32];

    ST_FormatStep(cv, value, text, sizeof(text));
    SM_Text(x, SM_Baseline(FONT_SANS, 15, y + 8, 18), FONT_SANS, 15, smText, text, -1, qfalse);
    SM_Fill(x + 56, y + 9, bw, 16, smGrey, clip);
    SM_Fill(x + 57, y + 10, bw - 2, 14, (SM_Over(ST_HIT_RANGE, i) || st.drag == i) ? smLighter : smDark, clip);
    SM_Fill(x + 57 + frac * (bw - 16), y + 10, 14, 14, smYellow, clip);
    SM_Hit(x + 56, y + 5, bw, 24, ST_HIT_RANGE, i, clip);
}

static void ST_DrawInput(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i];
    float width = st.valW - 20;
    int len = (int)strlen(value);
    int start = 0;
    qboolean editing = st.edit == i;

    SM_Fill(x, y, st.valW, 34, editing ? smFocus : smGrey, clip);
    SM_Fill(x + 1, y + 1, st.valW - 2, 32, smInput, clip);
    if (editing) {
        if (st.editCursor > len) {
            st.editCursor = len;
        }
        if (st.editStart > st.editCursor) {
            st.editStart = st.editCursor;
        }
        while (st.editStart < st.editCursor &&
               SM_Advance(FONT_SANS, 15, value + st.editStart, st.editCursor - st.editStart) > width) {
            st.editStart++;
        }
        start = st.editStart;
    }
    SM_Text(x + 9, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smText, value + start,
            SM_Fit(FONT_SANS, 15, value + start, width), qfalse);
    if (editing && ((uiInfo.uiDC.realTime - st.editTime) / 500) % 2 == 0) {
        SM_Fill(x + 9 + SM_Advance(FONT_SANS, 15, value + start, st.editCursor - start), y + 7, 1, 20, smText, clip);
    }
    SM_Hit(x, y, st.valW, 34, ST_HIT_INPUT, i, clip);
}

// the 26 Quake Live colours in a bar, the one in use marked
static void ST_DrawPalette(int i, float x, float y, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();
    int value = atoi(st.values[i]);
    float sw = st.valW / 26.0f;
    int k;

    for (k = 0; k < 26; k++) {
        SM_Fill(x + k * sw, y + 9, sw, 18, palette[k], clip);
    }
    if (value >= 1 && value <= 26) {
        SM_Fill(x + (value - 1) * sw - 1, y + 5, sw + 2, 3, smText, clip);
        SM_Fill(x + (value - 1) * sw - 1, y + 27, sw + 2, 3, smText, clip);
    }
    SM_Hit(x, y + 5, st.valW, 28, ST_HIT_PALETTE, i, clip);
}

// a colour of the row's own: the swatch, and clicking it opens the three channels
static void ST_DrawRGB(int i, float x, float y, const smRect_t* clip) {
    int rgb[3];
    vec4_t color;
    char text[16];

    ST_ParseRGB(st.values[i], rgb);
    color[0] = rgb[0] / 255.0f;
    color[1] = rgb[1] / 255.0f;
    color[2] = rgb[2] / 255.0f;
    color[3] = 1.0f;

    SM_Fill(x, y, st.valW, 34, SM_Over(ST_HIT_RGB, i) ? smLighter : smGrey, clip);
    SM_Fill(x + 1, y + 1, st.valW - 2, 32, smInput, clip);
    SM_Fill(x + 5, y + 5, 46, 24, color, clip);
    Com_sprintf(text, sizeof(text), "%i, %i, %i", rgb[0], rgb[1], rgb[2]);
    SM_Text(x + 60, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smSelectText, text, -1, qfalse);
    SM_Hit(x, y, st.valW, 34, ST_HIT_RGB, i, clip);
}

// a value stepped through with arrows: the model, its skin, the crosshair
static void ST_DrawStepper(int i, float x, float y, const char* shown, const smRect_t* clip) {
    SM_Fill(x, y, st.valW, 34, smGrey, clip);
    SM_Fill(x + 1, y + 1, st.valW - 2, 32, smInput, clip);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smSelectText, shown,
            SM_Fit(FONT_SANS, 15, shown, st.valW - 70), qfalse);
    SM_Text(x + st.valW - 44, SM_Baseline(FONT_SANS, 17, y + 6, 22), FONT_SANS, 17,
            SM_Over(ST_HIT_PREV, i) ? smYellow : smArrow, "<", -1, qtrue);
    SM_Text(x + st.valW - 22, SM_Baseline(FONT_SANS, 17, y + 6, 22), FONT_SANS, 17,
            SM_Over(ST_HIT_NEXT, i) ? smYellow : smArrow, ">", -1, qtrue);
    SM_Hit(x + st.valW - 50, y, 25, 34, ST_HIT_PREV, i, clip);
    SM_Hit(x + st.valW - 25, y, 25, 34, ST_HIT_NEXT, i, clip);
}

static void ST_DrawCrosshair(int i, float x, float y, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();
    int shape = atoi(st.values[i]);
    int index = (int)trap_Cvar_VariableValue("cg_crosshairColor") - 1;
    vec4_t color;
    char text[16];

    Com_sprintf(text, sizeof(text), shape ? "%i" : "None", shape);
    ST_DrawStepper(i, x, y, text, clip);

    if (shape <= 0 || shape >= NUM_CROSSHAIRS) {
        return;
    }
    if (index < 0 || index > 25) {
        index = 0;
    }
    VectorScale(palette[index], trap_Cvar_VariableValue("cg_crosshairBrightness"), color);
    color[3] = 1.0f;
    trap_R_SetColor(color);
    SM_Pic(x + st.valW * 0.5f - 14, y + 3, 28, 28, uiInfo.uiDC.Assets.crosshairShader[shape], clip);
    trap_R_SetColor(NULL);
}

static void ST_DrawBind(int i, float x, float y, const smRect_t* clip) {
    qboolean waiting = st.binding == i;
    const char* text = waiting ? "Press a key" : st.values[i];
    qboolean none = !waiting && !Q_stricmp(text, "None");

    SM_Fill(x, y, st.valW, 26, waiting ? smYellow : SM_Over(ST_HIT_BIND, i) ? smLighter : smGrey, clip);
    SM_Fill(x + 1, y + 1, st.valW - 2, 24, waiting ? smYellow : smInput, clip);
    SM_Text(x + 9, SM_Baseline(FONT_SANS, 14, y + 3, 20), FONT_SANS, 14,
            waiting ? smDark : none ? stOff : smText, text, SM_Fit(FONT_SANS, 14, text, st.valW - 18), qfalse);
    SM_Hit(x, y, st.valW, 26, ST_HIT_BIND, i, clip);
}

static void ST_DrawRow(int i, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float x = stLayout[i].x;
    float h = stLayout[i].h;
    float valX = x + st.valX;
    smRect_t row = SM_R(x, y, stLayout[i].w, h);
    qboolean hovered;

    if (cv->widget == ST_HEADING) {
        float tw = SM_SpacedWidth(FONT_DEFAULT, 17, cv->title, 1);

        SM_Text(x, SM_Baseline(FONT_DEFAULT, 17, y + 14, 22), FONT_DEFAULT, 17, smYellow, cv->title, -1, qfalse);
        SM_Fill(x + tw + 14, y + 25, stLayout[i].w - tw - 14, 1, smGrey, clip);
        return;
    }

    hovered = SM_In(&row, smMX, smMY) && st.select < 0 && st.rgb < 0;
    SM_Fill(x, y, stLayout[i].w, h, hovered ? smCardHover : (stLayout[i].nth & 1) ? smRowOdd : smRowEven, clip);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + (h - 22) * 0.5f, 22), FONT_SANS, 15,
            hovered ? smText : smSelectText, cv->title, SM_Fit(FONT_SANS, 15, cv->title, st.valX - 20), qfalse);
    SM_Hit(x, y, stLayout[i].w, h, ST_HIT_ROW, i, clip);
    if (hovered) {
        st.tip = cv->tip;
        st.tipCvar = cv->cvar;
    }

    switch (cv->widget) {
        case ST_BOOL:
            ST_DrawBool(i, valX, y + 8, clip);
            break;
        case ST_SELECT:
            ST_DrawSelect(i, valX, y + 8, clip);
            break;
        case ST_RANGE:
            ST_DrawRange(i, valX, y + 8, clip);
            break;
        case ST_INPUT:
            ST_DrawInput(i, valX, y + 8, clip);
            break;
        case ST_PALETTE:
            ST_DrawPalette(i, valX, y + 8, clip);
            break;
        case ST_RGB:
            ST_DrawRGB(i, valX, y + 8, clip);
            break;
        case ST_MODEL:
        case ST_MODELSKIN:
            ST_DrawStepper(i, valX, y + 8, st.values[i][0] ? st.values[i] : "Default", clip);
            break;
        case ST_CROSSHAIR:
            ST_DrawCrosshair(i, valX, y + 8, clip);
            break;
        case ST_BIND:
            ST_DrawBind(i, valX, y + 6, clip);
            break;
        default:
            break;
    }
}

// the X at the top right
static void ST_DrawClose(void) {
    float x = st.page + st.pageW - 34;
    float y = ST_PAGE_TOP + 12;
    qboolean over = SM_Over(ST_HIT_CLOSE, 0);
    int k;

    for (k = 0; k < 20; k++) {
        SM_Fill(x + k, y + k, 2, 2, over ? smYellow : smArrow, NULL);
        SM_Fill(x + 19 - k, y + k, 2, 2, over ? smYellow : smArrow, NULL);
    }
    SM_Hit(x - 12, y - 12, 44, 44, ST_HIT_CLOSE, 0, NULL);
}

/*
===============
ST_DrawNav

The sections, as the buttons the web UI's own .button-row draws them with: what
is open takes the yellow it uses for everything chosen, in black text.
===============
*/
static void ST_DrawNav(float y) {
    float x = st.page;
    int i;

    for (i = 0; i < ST_NUM_TABS; i++) {
        float w = SM_SpacedWidth(FONT_DEFAULT, 14, stTabNames[i], 1) + 30;
        qboolean on = i == st.tab;
        qboolean over = SM_Over(ST_HIT_TAB, i);

        SM_Fill(x, y, w, ST_NAV_H, on ? smYellow : over ? smButtonHover : smButton, NULL);
        if (!on) {
            SM_Fill(x + 1, y + 1, w - 2, ST_NAV_H - 2, over ? smButtonHover : smButton, NULL);
        }
        SM_SpacedText(x + 15, SM_Baseline(FONT_DEFAULT, 14, y + 8, 22), FONT_DEFAULT, 14, on ? stBlack : smText,
                      stTabNames[i], 1, on);
        SM_Hit(x, y, w, ST_NAV_H, ST_HIT_TAB, i, NULL);
        x += w + ST_NAV_GAP;
    }
}

// .Select-menu-outer, under the control
static void ST_DrawDropdown(void) {
    const stCvar_t* cv = &stCvars[st.select];
    float x = st.selectControl.x;
    float y = st.selectControl.y + st.selectControl.h;
    float rowH = 32.0f;
    float h = cv->numOptions * rowH;
    float pageBottom = (float)SM_VIEW_H - ST_PAGE_BOTTOM;
    smRect_t box;
    int k;

    if (h > 300) {
        h = 300;
    }
    if (y + h > pageBottom) {
        y = st.selectControl.y - h;
        if (y < 0) {
            y = pageBottom - h;
        }
    }
    box = SM_R(x, y, st.valW, h);
    SM_Fill(x - 2, y - 2, st.valW + 4, h + 4, smShadow, NULL);
    SM_Fill(x, y, st.valW, h, smInput, NULL);
    SM_Hit(x, y, st.valW, h, ST_HIT_MENU, st.select, NULL);
    st.selectMenu = box;
    for (k = 0; k < cv->numOptions; k++) {
        float oy = y + k * rowH - st.selectScroll;
        float base = SM_Baseline(FONT_SANS, 15, oy + 5, 22);
        qboolean chosen = !Q_stricmp(cv->options[k].value, st.values[st.select]);

        if (oy + rowH < y || oy > y + h) {
            continue;
        }
        if (SM_Over(ST_HIT_OPTION, k)) {
            SM_Fill(x, oy, st.valW, rowH, smGrey, &box);
        }
        if (SM_TextInside(base, 15, &box)) {
            SM_Text(x + 10, base, FONT_SANS, 15, chosen ? smYellow : smOptionText, cv->options[k].label,
                    SM_Fit(FONT_SANS, 15, cv->options[k].label, st.valW - 20), chosen);
        }
        SM_Hit(x, oy, st.valW, rowH, ST_HIT_OPTION, k, &box);
    }
}

// the three channels of a colour of the row's own
static void ST_DrawRGBPicker(void) {
    static const char* const names[3] = {"R", "G", "B"};
    float w = 300, h = 150;
    float x = stLayout[st.rgb].x + st.valX;
    float y = stLayout[st.rgb].y + st.listTop - st.scroll[st.tab] + 44;
    int rgb[3];
    vec4_t color;
    int k;

    if (x + w > st.page + st.pageW) {
        x = st.page + st.pageW - w;
    }
    if (y + h > (float)SM_VIEW_H - ST_PAGE_BOTTOM) {
        y = (float)SM_VIEW_H - ST_PAGE_BOTTOM - h;
    }
    ST_ParseRGB(st.values[st.rgb], rgb);

    SM_Fill(x - 2, y - 2, w + 4, h + 4, smShadow, NULL);
    SM_Fill(x, y, w, h, smDialog, NULL);
    SM_Hit(x, y, w, h, ST_HIT_RGBCLOSE, -1, NULL);  // clicks inside do not close it

    color[0] = rgb[0] / 255.0f;
    color[1] = rgb[1] / 255.0f;
    color[2] = rgb[2] / 255.0f;
    color[3] = 1.0f;
    SM_Fill(x + 12, y + 12, 64, 40, color, NULL);

    for (k = 0; k < 3; k++) {
        float sy = y + 66 + k * 26;
        float bw = w - 74;
        float frac = rgb[k] / 255.0f;
        char text[8];

        SM_Text(x + 12, SM_Baseline(FONT_SANS, 14, sy - 2, 18), FONT_SANS, 14, smText, names[k], -1, qtrue);
        SM_Fill(x + 30, sy, bw, 14, smGrey, NULL);
        SM_Fill(x + 31, sy + 1, bw - 2, 12, smDark, NULL);
        SM_Fill(x + 31 + frac * (bw - 14), sy + 1, 12, 12, smYellow, NULL);
        SM_Hit(x + 30, sy - 4, bw, 22, ST_HIT_RGBSLIDER, k, NULL);
        Com_sprintf(text, sizeof(text), "%i", rgb[k]);
        SM_Text(x + w - 34, SM_Baseline(FONT_SANS, 14, sy - 2, 18), FONT_SANS, 14, smSelectText, text, -1, qfalse);
    }
}

static void ST_DrawScrollbar(void) {
    float x = st.page + st.pageW + 10;
    float thumbH = st.view * st.view / st.content;
    float thumbY;

    if (thumbH < 24) {
        thumbH = 24;
    }
    thumbY = st.listTop + (st.view - thumbH) * st.scroll[st.tab] / (st.content - st.view);
    SM_Fill(x, st.listTop, 6, st.view, smRowEven, NULL);
    SM_Hit(x, st.listTop, 6, st.view, ST_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, 6, thumbH, (SM_Over(ST_HIT_THUMB, 0) || st.scrollDrag) ? smArrow : smGrey, NULL);
    SM_Hit(x, thumbY, 6, thumbH, ST_HIT_THUMB, 0, NULL);
}

static void ST_Frame(void) {
    st.pageW = smVW - 2 * ST_PAGE_MARGIN;
    if (st.pageW > ST_PAGE_MAX_W) {
        st.pageW = ST_PAGE_MAX_W;
    }
    st.page = (smVW - st.pageW) * 0.5f;
    st.colW = (st.pageW - ST_COL_GAP) * 0.5f;
    st.valX = st.colW * ST_VAL_FRAC;
    st.valW = st.colW - st.valX - 12;
    st.listTop = ST_PAGE_TOP + ST_TITLE_H + ST_NAV_H + ST_LIST_GAP;
    st.view = (float)SM_VIEW_H - ST_PAGE_BOTTOM - ST_TIP_H - st.listTop;
}

static void ST_DrawPage(void) {
    smRect_t clip;
    int i;

    ST_Frame();
    ST_Layout();
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab]);
    st.tip = NULL;
    st.tipCvar = NULL;

    SM_FillScreen(st.fromIngame ? stIngameTint : stBlack);
    SM_Fill(0, 0, smVW, ST_PAGE_TOP - 20, stNavBg, NULL);

    SM_SpacedText(st.page, SM_Baseline(FONT_DEFAULT, 30, ST_PAGE_TOP - 8, 38), FONT_DEFAULT, 30, smText,
                  "Game Settings", -1, qfalse);
    SM_Fill(st.page, ST_PAGE_TOP + 34, 60, 3, smYellow, NULL);
    ST_DrawClose();
    ST_DrawNav(ST_PAGE_TOP + ST_TITLE_H);

    clip = SM_R(st.page - 4, st.listTop, st.pageW + 8, st.view);
    for (i = 0; i < ST_NUM_CVARS; i++) {
        float y;

        if (!ST_InTab(i)) {
            continue;
        }
        y = st.listTop + stLayout[i].y - st.scroll[st.tab];
        if (y + stLayout[i].h > st.listTop && y < st.listTop + st.view) {
            ST_DrawRow(i, y, &clip);
        }
    }
    if (st.content > st.view) {
        ST_DrawScrollbar();
    }

    SM_Fill(st.page, (float)SM_VIEW_H - ST_PAGE_BOTTOM - ST_TIP_H, st.pageW, 1, smGrey, NULL);
    if (st.tip) {
        SM_Text(st.page, SM_Baseline(FONT_SANS, 14, (float)SM_VIEW_H - ST_PAGE_BOTTOM - ST_TIP_H + 6, 22), FONT_SANS,
                14, smSelectText, st.tip, -1, qfalse);
    }
    if (st.tipCvar) {
        float w = SM_Advance(FONT_SANS, 14, st.tipCvar, -1);

        SM_Text(st.page + st.pageW - w, SM_Baseline(FONT_SANS, 14, (float)SM_VIEW_H - ST_PAGE_BOTTOM - ST_TIP_H + 6, 22),
                FONT_SANS, 14, smYellow, st.tipCvar, -1, qfalse);
    }
}

/*
===============================================================================

WHAT IT DOES

===============================================================================
*/

static void ST_OpenSelect(int i) {
    const stCvar_t* cv = &stCvars[i];
    int k;

    st.select = i;
    st.selectFocus = -1;
    st.selectScroll = 0;
    for (k = 0; k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, st.values[i])) {
            st.selectFocus = k;
        }
    }
}

static void ST_DragRange(int i) {
    const stCvar_t* cv = &stCvars[i];
    float bx = stLayout[i].x + st.valX + 56;
    float bw = st.valW - 62;
    float frac = (smMX - bx - 7) / (bw - 14);
    char text[32];

    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    ST_FormatStep(cv, ST_Snap(cv, cv->min + frac * (cv->max - cv->min)), text, sizeof(text));
    ST_Set(i, text);
}

static void ST_DragRGB(int channel) {
    float w = 300;
    float x = stLayout[st.rgb].x + st.valX;
    float bw = w - 74;
    float frac;
    int rgb[3];

    if (x + w > st.page + st.pageW) {
        x = st.page + st.pageW - w;
    }
    frac = (smMX - (x + 30) - 6) / (bw - 12);
    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    ST_ParseRGB(st.values[st.rgb], rgb);
    rgb[channel] = (int)(frac * 255.0f + 0.5f);
    ST_SetRGB(st.rgb, rgb);
}

static void ST_Step(int i, int delta) {
    const stCvar_t* cv = &stCvars[i];
    char text[MAX_CVAR_VALUE_STRING];

    if (cv->widget == ST_CROSSHAIR) {
        int shape = atoi(st.values[i]) + delta;

        if (shape < 0) {
            shape = NUM_CROSSHAIRS - 1;
        } else if (shape >= NUM_CROSSHAIRS) {
            shape = 0;
        }
        Com_sprintf(text, sizeof(text), "%i", shape);
        ST_Set(i, text);
        uiInfo.currentCrosshair = shape;
        return;
    }
    if (cv->widget == ST_MODEL || cv->widget == ST_MODELSKIN) {
        int count = uiInfo.q3HeadCount;
        int index = 0;
        int k;

        if (count <= 0) {
            return;
        }
        for (k = 0; k < count; k++) {
            if (!Q_stricmp(uiInfo.q3HeadNames[k], st.values[i])) {
                index = k;
                break;
            }
        }
        index = (index + delta + count) % count;
        ST_Set(i, uiInfo.q3HeadNames[index]);
        return;
    }
}

static void ST_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);
    int type = hit ? hit->type : ST_HIT_NONE;
    int index = hit ? hit->index : 0;

    if (st.rgb >= 0 && type != ST_HIT_RGBSLIDER && type != ST_HIT_RGBCLOSE) {
        st.rgb = -1;
        if (type == ST_HIT_RGB) {
            return;
        }
    }
    if (st.select >= 0 && type != ST_HIT_OPTION && type != ST_HIT_MENU) {
        int was = st.select;

        st.select = -1;
        if (type == ST_HIT_SELECT && index == was) {
            return;
        }
    }
    if (st.edit >= 0 && type != ST_HIT_INPUT) {
        st.edit = -1;
    }

    switch (type) {
        case ST_HIT_TAB:
            st.tab = index;
            st.select = -1;
            st.edit = -1;
            st.binding = -1;
            st.rgb = -1;
            break;
        case ST_HIT_SELECT:
            ST_OpenSelect(index);
            break;
        case ST_HIT_OPTION:
            ST_Set(st.select, stCvars[st.select].options[index].value);
            st.select = -1;
            break;
        case ST_HIT_BOOL:
            ST_Set(index, atoi(st.values[index]) ? "0" : "1");
            break;
        case ST_HIT_RANGE:
            st.drag = index;
            ST_DragRange(index);
            break;
        case ST_HIT_INPUT:
            st.edit = index;
            st.editCursor = (int)strlen(st.values[index]);
            st.editStart = 0;
            st.editTime = uiInfo.uiDC.realTime;
            break;
        case ST_HIT_PALETTE: {
            float sw = st.valW / 26.0f;
            int k = (int)((smMX - (stLayout[index].x + st.valX)) / sw) + 1;
            char text[16];

            if (k < 1) {
                k = 1;
            } else if (k > 26) {
                k = 26;
            }
            Com_sprintf(text, sizeof(text), "%i", k);
            ST_Set(index, text);
            break;
        }
        case ST_HIT_RGB:
            st.rgb = index;
            break;
        case ST_HIT_RGBSLIDER:
            st.rgbDrag = index;
            ST_DragRGB(index);
            break;
        case ST_HIT_PREV:
            ST_Step(index, -1);
            break;
        case ST_HIT_NEXT:
            ST_Step(index, 1);
            break;
        case ST_HIT_BIND:
            st.binding = index;
            break;
        case ST_HIT_THUMB:
            st.scrollDrag = qtrue;
            st.scrollGrab = smMY;
            break;
        case ST_HIT_CLOSE:
            UI_Settings_Close();
            break;
        default:
            break;
    }
}

static void ST_Wheel(float amount) {
    if (st.select >= 0) {
        const stCvar_t* cv = &stCvars[st.select];
        float h = cv->numOptions * 32.0f;

        if (h > 300) {
            st.selectScroll += amount;
            if (st.selectScroll > h - 300) {
                st.selectScroll = h - 300;
            }
            if (st.selectScroll < 0) {
                st.selectScroll = 0;
            }
        }
        return;
    }
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab] + amount);
}

/*
===============
ST_Bind

Binds the key to the row's command, taking it off whatever it held before and
dropping the older of the two the row already shows: two keys per command, the
way the pages have always done it. Backspace clears them, as the web UI says.
===============
*/
static void ST_Bind(int i, int key) {
    const char* command = stCvars[i].cvar;
    int found[2];
    int count = 0;
    int k;

    for (k = 0; k < K_LAST_KEY; k++) {
        char buf[MAX_STRING_CHARS];

        trap_Key_GetBindingBuf(k, buf, sizeof(buf));
        if (buf[0] && !Q_stricmp(buf, command)) {
            if (count < 2) {
                found[count] = k;
            }
            count++;
        }
    }
    if (key == K_BACKSPACE) {
        for (k = 0; k < count && k < 2; k++) {
            trap_Key_SetBinding(found[k], "");
        }
        ST_Read(i);
        return;
    }
    for (k = 0; k < count && k < 2; k++) {
        if (found[k] == key) {
            return;  // already bound here
        }
    }
    if (count >= 2) {
        trap_Key_SetBinding(found[0], "");
    }
    trap_Key_SetBinding(key, command);
    ST_Read(i);
}

static qboolean ST_EditKey(int key) {
    char* buffer = st.values[st.edit];
    int len = (int)strlen(buffer);

    st.editTime = uiInfo.uiDC.realTime;
    if (key & K_CHAR_FLAG) {
        int c = key & ~K_CHAR_FLAG;

        if (c == 'h' - 'a' + 1) {  // ctrl-h is backspace
            if (st.editCursor > 0) {
                memmove(buffer + st.editCursor - 1, buffer + st.editCursor, len - st.editCursor + 1);
                st.editCursor--;
            }
            return qtrue;
        }
        if (c < 32 || len >= ST_MAX_INPUT || len >= (int)sizeof(st.values[0]) - 1) {
            return qtrue;
        }
        memmove(buffer + st.editCursor + 1, buffer + st.editCursor, len - st.editCursor + 1);
        buffer[st.editCursor++] = (char)c;
        return qtrue;
    }
    switch (key) {
        case K_DEL:
            if (st.editCursor < len) {
                memmove(buffer + st.editCursor, buffer + st.editCursor + 1, len - st.editCursor);
            }
            return qtrue;
        case K_LEFTARROW:
            if (st.editCursor > 0) {
                st.editCursor--;
            }
            return qtrue;
        case K_RIGHTARROW:
            if (st.editCursor < len) {
                st.editCursor++;
            }
            return qtrue;
        case K_HOME:
            st.editCursor = 0;
            return qtrue;
        case K_END:
            st.editCursor = len;
            return qtrue;
        case K_ENTER:
        case K_KP_ENTER:
        case K_ESCAPE:
            ST_Set(st.edit, buffer);
            st.edit = -1;
            return qtrue;
        default:
            return qfalse;
    }
}

/*
===============================================================================

THE PAGE

===============================================================================
*/

/*
===============
UI_Settings_Open

Retail's Settings button gives the browser the whole screen, so the in-game menu
goes away while the page is up and comes back when it closes.
===============
*/
void UI_Settings_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    if (!st.loaded) {
        st.tab = ST_TAB_BASIC;
        st.loaded = qtrue;
    }
    menu = Menus_FindByName("ingame");
    if (menu && (menu->window.flags & WINDOW_VISIBLE)) {
        st.fromIngame = qtrue;
        Menus_CloseByName("ingame");
    }
    st.select = -1;
    st.edit = -1;
    st.drag = -1;
    st.binding = -1;
    st.rgb = -1;
    st.rgbDrag = -1;
    st.scrollDrag = qfalse;
    ST_ReadAll();

    // the page takes keys before the mouse moves over it
    menu = Menus_FindByName("settings");
    item = menu ? Menu_FindItemByName(menu, "settings") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Settings_Paint(void) {
    const smHit_t* under;

    SM_Frame();
    if (!st.loaded) {
        UI_Settings_Open();
    }
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;

    if (st.drag >= 0) {
        ST_DragRange(st.drag);
    }
    if (st.rgbDrag >= 0 && st.rgb >= 0) {
        ST_DragRGB(st.rgbDrag);
    }
    if (st.scrollDrag && st.content > st.view) {
        float thumbH = st.view * st.view / st.content;

        if (thumbH < 24) {
            thumbH = 24;
        }
        st.scroll[st.tab] =
            ST_ClampScroll(st.scroll[st.tab] + (smMY - st.scrollGrab) * (st.content - st.view) / (st.view - thumbH));
        st.scrollGrab = smMY;
    }
    if (st.edit < 0 && st.binding < 0 && st.drag < 0 && st.rgbDrag < 0) {
        ST_ReadAll();
    }

    ST_DrawPage();
    if (st.select >= 0) {
        ST_DrawDropdown();
    }
    if (st.rgb >= 0) {
        ST_DrawRGBPicker();
    }

    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
}

qboolean UI_Settings_HandleKey(int key) {
    // a row waiting for a key takes everything but Escape
    if (st.binding >= 0) {
        if (key == K_ESCAPE) {
            st.binding = -1;
            return qtrue;
        }
        if (!(key & K_CHAR_FLAG)) {
            ST_Bind(st.binding, key);
            st.binding = -1;
        }
        return qtrue;
    }
    if (st.edit >= 0 && ST_EditKey(key)) {
        return qtrue;
    }
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
            ST_Click();
            return qtrue;
        case K_MWHEELUP:
            ST_Wheel(-90);
            return qtrue;
        case K_MWHEELDOWN:
            ST_Wheel(90);
            return qtrue;
        case K_ESCAPE:
            if (st.rgb >= 0) {
                st.rgb = -1;
                return qtrue;
            }
            if (st.select >= 0) {
                st.select = -1;
                return qtrue;
            }
            return qfalse;  // the menu's onESC closes the page
        default:
            return qfalse;
    }
}

// the mouse button coming up: a slider and the scrollbar stop following it
void UI_Settings_MouseUp(void) {
    st.drag = -1;
    st.rgbDrag = -1;
    st.scrollDrag = qfalse;
}

// Escape, or the page's close button: back to the in-game menu or the main menu
void UI_Settings_Close(void) {
    Menus_CloseByName("settings");
    if (st.fromIngame) {
        st.fromIngame = qfalse;
        Menus_ActivateByName("ingame");
        Menus_ActivateByName("ingame_about");
    } else {
        Menus_CloseAll();
        Menus_ActivateByName("main");
    }
}
