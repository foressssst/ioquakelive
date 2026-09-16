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
    ST_HIT_RGBCLOSE,
    ST_HIT_MODEL,
    ST_HIT_MODELITEM,
    ST_HIT_MODELPANE,
    ST_HIT_MODELDONE
} stHitType_t;

#define ST_MAX_MODELS MAX_PLAYERMODELS
#define ST_MODEL_COLS 5
#define ST_MODEL_CELL 104.0f
// UI_DrawPlayer sets its field of view from the box's width alone, so how big the
// model comes out is the pane's width and nothing else
#define ST_MODEL_PANE 380.0f

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

    int model;            // the row whose model chooser is open, else -1
    int modelCount;
    int modelNames[ST_MAX_MODELS];  // into uiInfo.q3HeadNames, -1 for "None"
    float modelScroll;
    playerInfo_t modelInfo;
    char modelShown[MAX_QPATH];     // what modelInfo was built from

    playerInfo_t preview[2];        // the Team page's two, teammates then enemies
    char previewShown[2][MAX_QPATH];

    int entered;                    // realTime the open tab began arriving
    qboolean scrollDrag;
    float scrollGrab;

    float page, pageW, colW, valX, valW;  // the frame, worked out once a frame
    float listTop, view, content;
    const char* tip;
    const char* tipCvar;
    qboolean fromIngame;
} st;

// Over a match the page keeps Quake Club's own background rather than retail's blue
// tint: at 0.7 over a lit scene the text loses its contrast, and the site's pages are
// opaque. A little transparency is left so you can still tell there is a match behind.
static const vec4_t stIngameTint = {23 / 255.0f, 22 / 255.0f, 21 / 255.0f, 0.94f};
static const vec4_t stBlack = {0.0f, 0.0f, 0.0f, 1.0f};
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

    SM_Round(x, y, st.valW, 34, SM_RADIUS_SM, (!open && SM_Over(ST_HIT_SELECT, i)) ? smLighter : smGrey, clip);
    SM_Round(x + 1, y + 1, st.valW - 2, 32, SM_RADIUS_SM, open ? smDark : smInput, clip);
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

    SM_Round(x, y, 54, 34, SM_RADIUS_SM, over ? smLighter : smGrey, clip);
    SM_Round(x + 1, y + 1, 52, 32, SM_RADIUS_SM, smInput, clip);
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

    SM_Round(x, y, st.valW, 34, SM_RADIUS_SM, editing ? smFocus : smGrey, clip);
    SM_Round(x + 1, y + 1, st.valW - 2, 32, SM_RADIUS_SM, smInput, clip);
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

    SM_Round(x, y, st.valW, 34, SM_RADIUS_SM, SM_Over(ST_HIT_RGB, i) ? smLighter : smGrey, clip);
    SM_Round(x + 1, y + 1, st.valW - 2, 32, SM_RADIUS_SM, smInput, clip);
    SM_Fill(x + 5, y + 5, 46, 24, color, clip);
    Com_sprintf(text, sizeof(text), "%i, %i, %i", rgb[0], rgb[1], rgb[2]);
    SM_Text(x + 60, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smSelectText, text, -1, qfalse);
    SM_Hit(x, y, st.valW, 34, ST_HIT_RGB, i, clip);
}

/*
===============================================================================

THE MODEL CHOOSER

Retail's is a modal of the model pictures web.pak ships, which we cannot. We have
something better to hand: the player model itself. So the chooser lists pak00's
own model icons and, beside them, draws the one picked as a model, turning, in
the colours the row it came from will give it.

===============================================================================
*/

// the model part of "model" or "model/skin"
static void ST_ModelBase(const char* name, char* out, int size) {
    const char* slash = strchr(name, '/');

    if (slash) {
        int len = (int)(slash - name);

        if (len > size - 1) {
            len = size - 1;
        }
        memcpy(out, name, len);
        out[len] = '\0';
        return;
    }
    Q_strncpyz(out, name, size);
}

/*
===============
ST_BuildModelList

A row that forces a model takes the model alone and may take none at all; the
player's own model takes a skin with it, so it lists every model and skin.
===============
*/
static void ST_BuildModelList(int i) {
    qboolean skins = stCvars[i].widget == ST_MODELSKIN;
    int k, n;

    st.modelCount = 0;
    if (!skins) {
        st.modelNames[st.modelCount++] = -1;  // None
    }
    for (k = 0; k < uiInfo.q3HeadCount && st.modelCount < ST_MAX_MODELS; k++) {
        if (!skins) {
            char base[MAX_QPATH], other[MAX_QPATH];
            qboolean seen = qfalse;

            ST_ModelBase(uiInfo.q3HeadNames[k], base, sizeof(base));
            for (n = 0; n < st.modelCount; n++) {
                if (st.modelNames[n] < 0) {
                    continue;
                }
                ST_ModelBase(uiInfo.q3HeadNames[st.modelNames[n]], other, sizeof(other));
                if (!Q_stricmp(base, other)) {
                    seen = qtrue;
                    break;
                }
            }
            if (seen) {
                continue;
            }
        }
        st.modelNames[st.modelCount++] = k;
    }
}

static const char* ST_ModelEntry(int slot, char* out, int size) {
    int k = st.modelNames[slot];

    if (k < 0) {
        Q_strncpyz(out, "None", size);
        return out;
    }
    if (stCvars[st.model].widget == ST_MODELSKIN) {
        Q_strncpyz(out, uiInfo.q3HeadNames[k], size);
    } else {
        ST_ModelBase(uiInfo.q3HeadNames[k], out, size);
    }
    return out;
}

// 0xRRGGBBAA into the packed int the model tint takes, the way ui_main.c reads them
static int UI_SettingsModelColor(const char* cvarName) {
    char buf[64];
    int rgb[3];

    trap_Cvar_VariableStringBuffer(cvarName, buf, sizeof(buf));
    ST_ParseRGB(buf, rgb);
    return (rgb[0] << 24) | (rgb[1] << 16) | (rgb[2] << 8) | 0xff;
}

// the colours the row's model is shown in: a team or enemy row tints its own
static void ST_ModelColors(int i, playerInfo_t* pi) {
    const char* cvar = stCvars[i].cvar;
    const char* head = NULL;
    const char* upper = NULL;
    const char* lower = NULL;

    if (!Q_stricmp(cvar, "cg_forceTeamModel")) {
        head = "cg_teamHeadColor";
        upper = "cg_teamUpperColor";
        lower = "cg_teamLowerColor";
    } else if (!Q_stricmp(cvar, "cg_forceEnemyModel")) {
        head = "cg_enemyHeadColor";
        upper = "cg_enemyUpperColor";
        lower = "cg_enemyLowerColor";
    }
    if (!head) {
        pi->customColor = qfalse;
        return;
    }
    // the colour cvars hold 0xRRGGBBAA, which is not a number trap_Cvar_VariableValue reads
    pi->customColor = qtrue;
    pi->headColor = UI_SettingsModelColor(head);
    pi->torsoColor = UI_SettingsModelColor(upper);
    pi->legsColor = UI_SettingsModelColor(lower);
}

static void ST_DrawModelPreview(float x, float y, float w, float h) {
    const char* value = st.values[st.model];
    char composed[MAX_QPATH];
    float r[4];

    if (!value[0] || !Q_stricmp(value, "None")) {
        SM_Text(x + w * 0.5f - SM_Advance(FONT_SANS, 15, "No model forced", -1) * 0.5f,
                SM_Baseline(FONT_SANS, 15, y + h * 0.5f, 22), FONT_SANS, 15, smSelectText, "No model forced", -1,
                qfalse);
        return;
    }
    // a forced model takes the default skin, the way retail's chooser does
    if (stCvars[st.model].widget == ST_MODELSKIN || strchr(value, '/')) {
        Q_strncpyz(composed, value, sizeof(composed));
    } else {
        Com_sprintf(composed, sizeof(composed), "%s/default", value);
    }
    if (Q_stricmp(composed, st.modelShown)) {
        vec3_t viewangles;

        Q_strncpyz(st.modelShown, composed, sizeof(st.modelShown));
        memset(&st.modelInfo, 0, sizeof(st.modelInfo));
        viewangles[YAW] = 180 - 30;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        UI_PlayerInfo_SetModel(&st.modelInfo, composed, composed, "");
        UI_PlayerInfo_SetInfo(&st.modelInfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
    }
    ST_ModelColors(st.model, &st.modelInfo);
    SM_To640(x, y, w, h, r);
    UI_DrawPlayer(r[0], r[1], r[2], r[3], &st.modelInfo, uiInfo.uiDC.realTime / 2);
}

/*
===============
ST_DrawTeamModels

The Team page's own preview, which is what the old .menu pages had and the web UI
never did: the model your teammates will wear and the one your enemies will, under
their column, in the colours the rows above set. Retail composes "model/skin" the
same way -- the model alone takes its default skin, a skin alone falls back to
sarge (uix86.dll UI_DrawTeamModel @0x10005850).
===============
*/
static void ST_DrawTeamPreview(int slot, const char* modelCvar, const char* skinCvar, const char* headCvar,
                               const char* upperCvar, const char* lowerCvar, float x, float y, float w, float h) {
    char model[MAX_QPATH], skin[MAX_QPATH], composed[MAX_QPATH];
    playerInfo_t* pi = &st.preview[slot];
    float r[4];
    char* slash;

    trap_Cvar_VariableStringBuffer(modelCvar, model, sizeof(model));
    trap_Cvar_VariableStringBuffer(skinCvar, skin, sizeof(skin));
    if (!model[0] && !skin[0]) {
        SM_Text(x + w * 0.5f - SM_Advance(FONT_SANS, 14, "Not forced", -1) * 0.5f,
                SM_Baseline(FONT_SANS, 14, y + h * 0.5f, 22), FONT_SANS, 14, smSubtle, "Not forced", -1, qfalse);
        return;
    }
    if (!model[0]) {
        Q_strncpyz(model, "sarge", sizeof(model));
    }
    slash = strchr(model, '/');
    if (slash) {
        *slash = '\0';
    }
    Com_sprintf(composed, sizeof(composed), "%s/%s", model, skin[0] ? skin : "default");

    if (Q_stricmp(composed, st.previewShown[slot])) {
        vec3_t viewangles;

        Q_strncpyz(st.previewShown[slot], composed, sizeof(st.previewShown[slot]));
        memset(pi, 0, sizeof(*pi));
        viewangles[YAW] = 150;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        UI_PlayerInfo_SetModel(pi, composed, composed, "");
        UI_PlayerInfo_SetInfo(pi, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
    }
    pi->customColor = qtrue;
    pi->headColor = UI_SettingsModelColor(headCvar);
    pi->torsoColor = UI_SettingsModelColor(upperCvar);
    pi->legsColor = UI_SettingsModelColor(lowerCvar);

    SM_To640(x, y, w, h, r);
    UI_DrawPlayer(r[0], r[1], r[2], r[3], pi, uiInfo.uiDC.realTime / 2);
}

static void ST_DrawTeamModels(void) {
    float top = st.listTop + 2 * 12.0f + ST_HEAD_H + 8 * ST_ROW_H;
    float h = st.listTop + st.view - top - 10;
    float w = st.colW;

    if (h < 150) {
        return;  // no room under the rows
    }
    SM_Round(st.page, top, w, h, SM_RADIUS_LG, smRowEven, NULL);
    SM_Round(st.page + w + ST_COL_GAP, top, w, h, SM_RADIUS_LG, smRowEven, NULL);
    ST_DrawTeamPreview(0, "cg_forceTeamModel", "cg_forceTeamSkin", "cg_teamHeadColor", "cg_teamUpperColor",
                       "cg_teamLowerColor", st.page + 10, top + 10, w - 20, h - 20);
    ST_DrawTeamPreview(1, "cg_forceEnemyModel", "cg_forceEnemySkin", "cg_enemyHeadColor", "cg_enemyUpperColor",
                       "cg_enemyLowerColor", st.page + w + ST_COL_GAP + 10, top + 10, w - 20, h - 20);
}

static void ST_DrawModelChooser(void) {
    float w = 22 + ST_MODEL_COLS * ST_MODEL_CELL + 16 + ST_MODEL_PANE + 22;
    float h = 600;
    float x = (smVW - w) * 0.5f;
    float y = ((float)SM_VIEW_H - h) * 0.5f;
    float listW = ST_MODEL_COLS * ST_MODEL_CELL;
    float rows = (float)((st.modelCount + ST_MODEL_COLS - 1) / ST_MODEL_COLS);
    float listH = h - 96;
    smRect_t box;
    char name[MAX_QPATH];
    int k;

    SM_FillScreen(smDim);
    SM_Fill(x, y, w, h, smDialog, NULL);
    SM_Hit(x, y, w, h, ST_HIT_MODELPANE, 0, NULL);
    SM_SpacedText(x + 22, SM_Baseline(FONT_DEFAULT, 20, y + 14, 28), FONT_DEFAULT, 20, smText,
                  stCvars[st.model].title, -1, qfalse);
    SM_Fill(x + 22, y + 52, w - 44, 1, smGrey, NULL);

    box = SM_R(x + 22, y + 66, listW, listH);
    if (rows * ST_MODEL_CELL < listH) {
        st.modelScroll = 0;
    } else if (st.modelScroll > rows * ST_MODEL_CELL - listH) {
        st.modelScroll = rows * ST_MODEL_CELL - listH;
    }
    if (st.modelScroll < 0) {
        st.modelScroll = 0;
    }

    for (k = 0; k < st.modelCount; k++) {
        float cx = box.x + (k % ST_MODEL_COLS) * ST_MODEL_CELL;
        float cy = box.y + (k / ST_MODEL_COLS) * ST_MODEL_CELL - st.modelScroll;
        qboolean chosen;
        int slot = st.modelNames[k];

        if (cy + ST_MODEL_CELL < box.y || cy > box.y + box.h) {
            continue;
        }
        ST_ModelEntry(k, name, sizeof(name));
        chosen = !Q_stricmp(name, st.values[st.model]) ||
                 (slot < 0 && !st.values[st.model][0]);
        SM_Round(cx, cy, ST_MODEL_CELL - 8, ST_MODEL_CELL - 8, SM_RADIUS_MD,
                 chosen ? smYellow : SM_Over(ST_HIT_MODELITEM, k) ? smCardHover : smCard, &box);
        if (slot >= 0) {
            SM_Pic(cx + 10, cy + 6, ST_MODEL_CELL - 28, ST_MODEL_CELL - 44, uiInfo.q3HeadIcons[slot], &box);
        }
        SM_Text(cx + (ST_MODEL_CELL - 8) * 0.5f - SM_Advance(FONT_SANS, 13, name, -1) * 0.5f,
                SM_Baseline(FONT_SANS, 13, cy + ST_MODEL_CELL - 34, 18), FONT_SANS, 13,
                chosen ? smOnAccent : smText, name, SM_Fit(FONT_SANS, 13, name, ST_MODEL_CELL - 14), qfalse);
        SM_Hit(cx, cy, ST_MODEL_CELL - 8, ST_MODEL_CELL - 8, ST_HIT_MODELITEM, k, &box);
    }

    // the one in use, as a model, turning
    SM_Round(x + w - 22 - ST_MODEL_PANE, y + 66, ST_MODEL_PANE, listH, SM_RADIUS_MD, smDark, NULL);
    ST_DrawModelPreview(x + w - 22 - ST_MODEL_PANE + 6, y + 72, ST_MODEL_PANE - 12, listH - 12);

    SM_Round(x + w - 130, y + h - 60, 108, 36, SM_RADIUS_SM, SM_Over(ST_HIT_MODELDONE, 0) ? smButtonHover : smButton, NULL);
    SM_Centered(x + w - 76, SM_Baseline(FONT_SANS, 15, y + h - 53, 22), 15, smText, "Done", qtrue);
    SM_Hit(x + w - 130, y + h - 60, 108, 36, ST_HIT_MODELDONE, 0, NULL);
}

// a value stepped through with arrows: the model, its skin, the crosshair
static void ST_DrawStepper(int i, float x, float y, const char* shown, const smRect_t* clip) {
    SM_Round(x, y, st.valW, 34, SM_RADIUS_SM, smGrey, clip);
    SM_Round(x + 1, y + 1, st.valW - 2, 32, SM_RADIUS_SM, smInput, clip);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smSelectText, shown,
            SM_Fit(FONT_SANS, 15, shown, st.valW - 70), qfalse);
    SM_Text(x + st.valW - 44, SM_Baseline(FONT_SANS, 17, y + 6, 22), FONT_SANS, 17,
            SM_Over(ST_HIT_PREV, i) ? smYellow : smArrow, "<", -1, qtrue);
    SM_Text(x + st.valW - 22, SM_Baseline(FONT_SANS, 17, y + 6, 22), FONT_SANS, 17,
            SM_Over(ST_HIT_NEXT, i) ? smYellow : smArrow, ">", -1, qtrue);
    SM_Hit(x + st.valW - 50, y, 25, 34, ST_HIT_PREV, i, clip);
    SM_Hit(x + st.valW - 25, y, 25, 34, ST_HIT_NEXT, i, clip);
}

// the model in use with its icon, which opens the chooser
static void ST_DrawModelButton(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i][0] ? st.values[i] : "None";
    qboolean over = SM_Over(ST_HIT_MODEL, i);
    char base[MAX_QPATH];
    qhandle_t icon = 0;
    int k;

    ST_ModelBase(value, base, sizeof(base));
    for (k = 0; k < uiInfo.q3HeadCount; k++) {
        char other[MAX_QPATH];

        if (!Q_stricmp(uiInfo.q3HeadNames[k], value)) {
            icon = uiInfo.q3HeadIcons[k];
            break;
        }
        ST_ModelBase(uiInfo.q3HeadNames[k], other, sizeof(other));
        if (!icon && !Q_stricmp(other, base)) {
            icon = uiInfo.q3HeadIcons[k];
        }
    }

    SM_Round(x, y, st.valW, 34, SM_RADIUS_SM, over ? smLighter : smGrey, clip);
    SM_Round(x + 1, y + 1, st.valW - 2, 32, SM_RADIUS_SM, smInput, clip);
    if (icon) {
        SM_Pic(x + 4, y + 3, 28, 28, icon, clip);
    }
    SM_Text(x + 38, SM_Baseline(FONT_SANS, 15, y + 7, 22), FONT_SANS, 15, smSelectText, value,
            SM_Fit(FONT_SANS, 15, value, st.valW - 48), qfalse);
    SM_Hit(x, y, st.valW, 34, ST_HIT_MODEL, i, clip);
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

    SM_Round(x, y, st.valW, 26, SM_RADIUS_SM, waiting ? smYellow : SM_Over(ST_HIT_BIND, i) ? smLighter : smGrey, clip);
    SM_Round(x + 1, y + 1, st.valW - 2, 24, SM_RADIUS_SM, waiting ? smYellow : smInput, clip);
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

    // Quake Club heads a section with the title alone: no rule under it, and no
    // divider between the rows either -- what tells them apart is the background
    // under the cursor (app/qc-design-system.css, "sin subrayado" and "se
    // distinguen por el fondo al pasar el cursor")
    if (cv->widget == ST_HEADING) {
        SM_SpacedText(x + 2, SM_Baseline(FONT_DEFAULT, 16, y + 16, 22), FONT_DEFAULT, 16, smYellow, cv->title, 1.5f,
                      qfalse);
        return;
    }

    hovered = SM_In(&row, smMX, smMY) && st.select < 0 && st.rgb < 0;
    if (hovered) {
        y -= SM_LIFT;  // the row lifts under the cursor, as its rows do
        SM_Round(x, y, stLayout[i].w, h, SM_RADIUS_MD, smCardHover, clip);
        SM_Round(x, y + 6, 2, h - 12, 1, smYellow, clip);
    }
    SM_Text(x + 14, SM_Baseline(FONT_SANS, 15, y + (h - 22) * 0.5f, 22), FONT_SANS, 15,
            hovered ? smText : smSelectText, cv->title, SM_Fit(FONT_SANS, 15, cv->title, st.valX - 24), qfalse);
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
            ST_DrawModelButton(i, valX, y + 8, clip);
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

        SM_Round(x, y, w, ST_NAV_H, SM_RADIUS_MD, on ? smYellow : over ? smButtonHover : smButton, NULL);
        SM_SpacedText(x + 15, SM_Baseline(FONT_DEFAULT, 14, y + 8, 22), FONT_DEFAULT, 14, on ? smOnAccent : smText,
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
    SM_Shadow(x, y, st.valW, h, SM_RADIUS_MD);
    SM_Round(x, y, st.valW, h, SM_RADIUS_MD, smDark, NULL);
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

    SM_Shadow(x, y, w, h, SM_RADIUS_LG);
    SM_Round(x, y, w, h, SM_RADIUS_LG, smDialog, NULL);
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
    int i, nth;

    ST_Frame();
    ST_Layout();
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab]);
    st.tip = NULL;
    st.tipCvar = NULL;

    // out of a match the page sits under the front end's bar; in one there is nothing
    // to navigate to, so the browser's tint over the game is all there is
    if (st.fromIngame) {
        SM_FillScreen(stIngameTint);
    } else {
        UI_Home_Backdrop();
        UI_Home_Nav(2);  // HM_NAV_SETTINGS
    }

    SM_SpacedText(st.page, SM_Baseline(FONT_DEFAULT, 30, ST_PAGE_TOP - 8, 38), FONT_DEFAULT, 30, smText,
                  "Game Settings", -1, qfalse);
    SM_Round(st.page, ST_PAGE_TOP + 34, 56, 3, 1.5f, smYellow, NULL);
    ST_DrawClose();
    ST_DrawNav(ST_PAGE_TOP + ST_TITLE_H);

    // Each run of rows under a heading is a piece of its own: its own surface,
    // rounded, with space around it instead of a line. That is how the site
    // separates blocks (app/qc-design-system.css, "para que se lea como una pieza
    // suelta sobre el fondo de la pagina").
    clip = SM_R(st.page - 4, st.listTop, st.pageW + 8, st.view);
    {
        float top[2] = {-1.0f, -1.0f};
        float bottom[2] = {0.0f, 0.0f};
        int c;

        for (i = 0; i <= ST_NUM_CVARS; i++) {
            qboolean flush = i == ST_NUM_CVARS;

            if (!flush && (!ST_InTab(i) )) {
                continue;
            }
            if (flush || stCvars[i].widget == ST_HEADING) {
                for (c = 0; c < 2; c++) {
                    if (top[c] >= 0) {
                        float py = st.listTop + top[c] - st.scroll[st.tab] - 8;

                        SM_Round(st.page + c * (st.colW + ST_COL_GAP) - 6, py, st.colW + 12,
                                 bottom[c] - top[c] + 16, SM_RADIUS_LG, smRowEven, &clip);
                    }
                    top[c] = -1.0f;
                }
                continue;
            }
            c = stCvars[i].column ? 1 : 0;
            if (top[c] < 0) {
                top[c] = stLayout[i].y;
            }
            bottom[c] = stLayout[i].y + stLayout[i].h;
        }
    }

    for (i = 0, nth = 0; i < ST_NUM_CVARS; i++) {
        float y, alpha, dy;

        if (!ST_InTab(i)) {
            continue;
        }
        y = st.listTop + stLayout[i].y - st.scroll[st.tab];
        if (y + stLayout[i].h <= st.listTop || y >= st.listTop + st.view) {
            continue;
        }
        SM_EnterRise(st.entered, nth++, &alpha, &dy);
        if (alpha <= 0.01f) {
            continue;
        }
        ST_DrawRow(i, y + dy, &clip);
    }
    if (st.tab == ST_TAB_TEAM) {
        ST_DrawTeamModels();
    }
    if (st.content > st.view) {
        ST_DrawScrollbar();
    }

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

    // the model chooser takes the whole screen while it is up
    if (st.model >= 0) {
        if (type == ST_HIT_MODELITEM) {
            char name[MAX_QPATH];

            ST_ModelEntry(index, name, sizeof(name));
            ST_Set(st.model, st.modelNames[index] < 0 ? "" : name);
        } else if (type == ST_HIT_MODELDONE || type == ST_HIT_NONE) {
            st.model = -1;
        }
        return;
    }

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
    if (!st.fromIngame && st.select < 0 && UI_Home_NavClick()) {
        return;
    }

    switch (type) {
        case ST_HIT_TAB:
            st.tab = index;
            st.entered = uiInfo.uiDC.realTime;
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
        case ST_HIT_MODEL:
            st.model = index;
            st.modelScroll = 0;
            ST_BuildModelList(index);
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
    if (st.model >= 0) {
        st.modelScroll += amount;
        return;
    }
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
    menu = Menus_FindByName("settings_ingame");
    st.fromIngame = menu && (menu->window.flags & WINDOW_VISIBLE);
    st.select = -1;
    st.edit = -1;
    st.drag = -1;
    st.binding = -1;
    st.rgb = -1;
    st.rgbDrag = -1;
    st.model = -1;
    st.scrollDrag = qfalse;
    ST_ReadAll();

    // the page takes keys before the mouse moves over it
    if (!menu) {
        menu = Menus_FindByName("settings");
    }
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
    if (st.model >= 0) {
        ST_DrawModelChooser();
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
            if (st.model >= 0) {
                st.model = -1;
                return qtrue;
            }
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
    Menus_CloseByName("settings_ingame");
    if (st.fromIngame) {
        st.fromIngame = qfalse;
        Menus_ActivateByName("ingame");
        Menus_ActivateByName("ingame_about");
    } else {
        Menus_CloseAll();
        Menus_ActivateByName("main");
    }
}
