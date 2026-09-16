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
// ("web_changeHash /settings", ingame.menu's navbtn6), so the settings live in
// web.pak and there is no native page to port. pak00 still carries the pages QL
// used before the browser, ui/ingame_options_*.menu and ui/ingame_controls.menu,
// and every option in them is real and still wired to a live cvar.
//
// So the options come from there -- retail's own labels, value lists, ranges and
// tooltips, generated into ui_settings_table.h -- and they are drawn here the way
// the web UI draws Start Match, with ui_web.c. Nothing about the look is invented:
// it is the stylesheet the rest of retail's UI uses.
//

#include "ui_web.h"

#define ST_MAX_INPUT 200

// the page, the same frame Start Match uses
#define ST_PAGE_W 1460.0f
#define ST_PAGE_TOP 110.0f
#define ST_PAGE_H 940.0f
#define ST_NAV_Y 54.39f    // under the h1 and its margins
#define ST_NAV_H 40.0f
#define ST_LIST_Y (ST_NAV_Y + ST_NAV_H + 16.0f)
#define ST_ROW_H 50.0f
#define ST_HEAD_H 44.0f
#define ST_COL_W 715.0f    // .one-half.column, the two the page lays its rows out in
#define ST_COL_GAP 20.0f
#define ST_VAL_X 418.3f    // .val in a row, as Start Match places it
#define ST_VAL_W 266.34f
#define ST_TIP_H 30.0f     // the tooltip line under the list

typedef enum {
    ST_TAB_GAME,
    ST_TAB_PLAYER,
    ST_TAB_HUD,
    ST_TAB_TEAM,
    ST_TAB_WEAPONS,
    ST_TAB_VIDEO,
    ST_TAB_SOUND,
    ST_TAB_SPECTATING,
    ST_TAB_CONTROLS,
    ST_NUM_TABS
} stTab_t;

static const char* stTabNames[ST_NUM_TABS] = {
    "GAME", "PLAYER", "HUD", "TEAM", "WEAPONS", "VIDEO", "SOUND", "SPECTATING", "CONTROLS"};

typedef enum {
    ST_HEADING,  // a section title, not an option
    ST_SELECT,   // a value out of a list
    ST_RANGE,    // a slider
    ST_INPUT,    // a text field
    ST_COLOR,    // one of the 26 crosshair/rail colours
    ST_BIND,     // a key bound to a command
    ST_PRESET,   // a list whose entries each set several cvars
    ST_CROSSHAIR // the crosshair itself, stepped through with arrows
} stWidget_t;

typedef struct {
    const char* label;
    const char* value;
} stOption_t;

typedef struct {
    const char* cvar;
    const char* value;
} stAssign_t;

typedef struct {
    const char* label;
    const stAssign_t* assigns;
    int numAssigns;
} stPreset_t;

typedef struct {
    int tab;
    const char* title;
    const char* cvar;   // NULL on a heading
    int widget;
    float min, max;
    const void* options;  // stOption_t[] or stPreset_t[]
    int numOptions;
    const char* tip;
} stCvar_t;

#include "ui_settings_table.h"

#define ST_NUM_CVARS ((int)ARRAY_LEN(stCvars))

/*
===============================================================================

WHAT THE CURSOR IS OVER

===============================================================================
*/

typedef enum {
    ST_HIT_BLOCK,
    ST_HIT_TAB,
    ST_HIT_RESET,
    ST_HIT_SELECT,
    ST_HIT_MENU,
    ST_HIT_OPTION,
    ST_HIT_RANGE,
    ST_HIT_INPUT,
    ST_HIT_COLOR,
    ST_HIT_BIND,
    ST_HIT_CROSSPREV,
    ST_HIT_CROSSNEXT,
    ST_HIT_ROW,
    ST_HIT_TRACK,
    ST_HIT_THUMB,
    ST_HIT_CLOSE
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

    int select, selectFocus;   // an open dropdown and the option under the cursor
    smRect_t selectControl, selectMenu;
    int drag;                  // a slider being dragged
    int edit, editCursor, editStart, editTime;
    int binding;               // a row waiting for a key
    qboolean scrollDrag;
    float scrollGrab;
    float content, view;       // the list's height and what shows of it
    const char* tip;           // the tooltip of whatever the cursor is on
    const char* tipCvar;       // and its cvar, shown beside it
    qboolean fromIngame;       // opened over a match, so Escape goes back to its menu
} st;

// the browser covered the screen whole, so the page is opaque and the match is not drawn
static const vec4_t stBackdrop = {0.043f, 0.039f, 0.039f, 1.0f};

// [QL] the same 26 colours the crosshair and the rail use (uix86.dll 0x1002bbf0)
extern const vec4_t* UI_CrosshairPalette(void);

/*
===============================================================================

THE CVARS

===============================================================================
*/

static const stOption_t* ST_Options(const stCvar_t* cv) {
    return (const stOption_t*)cv->options;
}

static const stPreset_t* ST_Presets(const stCvar_t* cv) {
    return (const stPreset_t*)cv->options;
}

static void ST_Read(int i) {
    const stCvar_t* cv = &stCvars[i];

    if (!cv->cvar) {
        return;
    }
    if (cv->widget == ST_BIND) {
        int keys[2];
        int count = 0;
        int k;

        st.values[i][0] = '\0';
        keys[0] = keys[1] = -1;
        for (k = 0; k < K_LAST_KEY && count < 2; k++) {
            char buf[MAX_STRING_CHARS];

            trap_Key_GetBindingBuf(k, buf, sizeof(buf));
            if (buf[0] && !Q_stricmp(buf, cv->cvar)) {
                keys[count++] = k;
            }
        }
        for (k = 0; k < count; k++) {
            char name[32];

            trap_Key_KeynumToStringBuf(keys[k], name, sizeof(name));
            Q_strupr(name);
            if (k) {
                Q_strcat(st.values[i], sizeof(st.values[i]), ", ");
            }
            Q_strcat(st.values[i], sizeof(st.values[i]), name);
        }
        return;
    }
    trap_Cvar_VariableStringBuffer(cv->cvar, st.values[i], sizeof(st.values[i]));
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

// a number the way a cvar holds it: no trailing zeroes on a whole value
static void ST_FormatValue(float value, float range, char* out, int size) {
    if (range >= 20.0f) {
        Com_sprintf(out, size, "%i", (int)(value + (value < 0 ? -0.5f : 0.5f)));
    } else if (range >= 2.0f) {
        Com_sprintf(out, size, "%.2f", value);
    } else {
        Com_sprintf(out, size, "%.3f", value);
    }
}

static float ST_Fraction(const stCvar_t* cv, float value) {
    float frac;

    if (cv->max <= cv->min) {
        return 0;
    }
    frac = (value - cv->min) / (cv->max - cv->min);
    return frac < 0 ? 0 : frac > 1 ? 1 : frac;
}

/*
===============
ST_PresetIndex

Which of a preset list's entries every cvar currently matches, or -1. Retail keeps
the answer in a ui_ cvar instead; reading the cvars back means the page is right
even after the player changed one of them by hand.
===============
*/
static int ST_PresetIndex(const stCvar_t* cv) {
    const stPreset_t* presets = ST_Presets(cv);
    int k, n;

    for (k = 0; k < cv->numOptions; k++) {
        qboolean all = qtrue;

        for (n = 0; n < presets[k].numAssigns; n++) {
            char buf[MAX_CVAR_VALUE_STRING];

            trap_Cvar_VariableStringBuffer(presets[k].assigns[n].cvar, buf, sizeof(buf));
            if (Q_stricmp(buf, presets[k].assigns[n].value) != 0) {
                all = qfalse;
                break;
            }
        }
        if (all && presets[k].numAssigns) {
            return k;
        }
    }
    return -1;
}

static void ST_ApplyPreset(const stCvar_t* cv, int index) {
    const stPreset_t* presets = ST_Presets(cv);
    int n;

    if (index < 0 || index >= cv->numOptions) {
        return;
    }
    for (n = 0; n < presets[index].numAssigns; n++) {
        trap_Cvar_Set(presets[index].assigns[n].cvar, presets[index].assigns[n].value);
    }
    if (cv->cvar) {
        trap_Cvar_Set(cv->cvar, presets[index].label);
    }
}

// what a row shows on the right: the option's label, the preset's, or the raw value
static const char* ST_Shown(int i) {
    const stCvar_t* cv = &stCvars[i];
    int k;

    if (cv->widget == ST_PRESET) {
        k = ST_PresetIndex(cv);
        return k < 0 ? "Custom" : ST_Presets(cv)[k].label;
    }
    if (cv->widget == ST_SELECT) {
        for (k = 0; k < cv->numOptions; k++) {
            if (!Q_stricmp(ST_Options(cv)[k].value, st.values[i])) {
                return ST_Options(cv)[k].label;
            }
        }
        return st.values[i][0] ? st.values[i] : "Default";
    }
    return st.values[i];
}

/*
===============================================================================

THE LIST

Only the rows of the open tab are drawn, in the table's order, and the scroll is
kept per tab so moving between them comes back where you were.

===============================================================================
*/

static qboolean ST_InTab(int i) {
    return stCvars[i].tab == st.tab;
}

// where each row of the open tab sits, in page coordinates before the scroll
static struct {
    float x, y;
    int nth;
} stLayout[ST_NUM_CVARS];

/*
===============
ST_Layout

A heading takes a line of its own; the rows under it flow into two columns, the
way .one-half.column lays the web UI out. Drawing and hit testing both read this,
so they can never disagree about where a row is.
===============
*/
static void ST_Layout(float left) {
    float y = 0;
    int i, column = 0, nth = 1;

    for (i = 0; i < ST_NUM_CVARS; i++) {
        if (!ST_InTab(i)) {
            continue;
        }
        if (stCvars[i].widget == ST_HEADING) {
            if (column) {
                y += ST_ROW_H;  // close the half-filled line above
                column = 0;
            }
            stLayout[i].x = left;
            stLayout[i].y = y;
            stLayout[i].nth = 0;
            y += ST_HEAD_H;
            nth = 1;
            continue;
        }
        stLayout[i].x = left + column * (ST_COL_W + ST_COL_GAP);
        stLayout[i].y = y;
        stLayout[i].nth = nth++;
        if (column) {
            y += ST_ROW_H;
        }
        column ^= 1;
    }
    st.content = y + (column ? ST_ROW_H : 0);
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

    SM_Fill(x, y, ST_VAL_W, 36, (!open && SM_Over(ST_HIT_SELECT, i)) ? smLighter : smGrey, clip);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 8, 24), FONT_SANS, 15, smSelectText, label,
            SM_Fit(FONT_SANS, 15, label, ST_VAL_W - 62), qfalse);
    SM_Arrow(x + 246.34f, y + 18, open);
    SM_Hit(x, y, ST_VAL_W, 36, ST_HIT_SELECT, i, clip);
    if (open) {
        st.selectControl = SM_R(x, y, ST_VAL_W, 36);
    }
}

static void ST_DrawRange(int i, float x, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float bx = x + 53.27f, by = y + 9, bw = 213.07f;
    float value = (float)atof(st.values[i]);
    float frac = ST_Fraction(cv, value);
    char text[32];

    ST_FormatValue(value, cv->max - cv->min, text, sizeof(text));
    SM_Text(x + 8, SM_Baseline(FONT_SANS, 15, y + 10, 16), FONT_SANS, 15, smText, text, -1, qfalse);
    SM_Fill(bx, by, bw, 18, smGrey, clip);
    SM_Fill(bx + 1, by + 1, bw - 2, 16, (SM_Over(ST_HIT_RANGE, i) || st.drag == i) ? smLighter : smDark, clip);
    SM_Fill(bx + 1 + frac * (bw - 2 - 16), by + 1, 16, 16, smYellow, clip);
    SM_Hit(bx, by, bw, 18, ST_HIT_RANGE, i, clip);
}

static void ST_DrawInput(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i];
    float width = ST_VAL_W - 20;
    int len = (int)strlen(value);
    int start = 0;
    qboolean editing = st.edit == i;

    SM_Fill(x, y, ST_VAL_W, 38, editing ? smFocus : smGrey, clip);
    SM_Fill(x + 1, y + 1, ST_VAL_W - 2, 36, smInput, clip);
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
    SM_Text(x + 9, y + 24.82f, FONT_SANS, 15, smText, value + start, SM_Fit(FONT_SANS, 15, value + start, width),
            qfalse);
    if (editing && ((uiInfo.uiDC.realTime - st.editTime) / 500) % 2 == 0) {
        SM_Fill(x + 9 + SM_Advance(FONT_SANS, 15, value + start, st.editCursor - start), y + 9, 1, 20, smText, clip);
    }
    SM_Hit(x, y, ST_VAL_W, 38, ST_HIT_INPUT, i, clip);
}

// the 26 colours in a bar, the chosen one marked
static void ST_DrawColor(int i, float x, float y, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();
    int value = atoi(st.values[i]);
    float sw = ST_VAL_W / 26.0f;
    int k;

    for (k = 0; k < 26; k++) {
        SM_Fill(x + k * sw, y + 8, sw, 20, palette[k], clip);
    }
    if (value >= 1 && value <= 26) {
        SM_Fill(x + (value - 1) * sw - 1, y + 5, sw + 2, 3, smText, clip);
        SM_Fill(x + (value - 1) * sw - 1, y + 28, sw + 2, 3, smText, clip);
    }
    SM_Hit(x, y + 8, ST_VAL_W, 20, ST_HIT_COLOR, i, clip);
}

/*
===============
ST_DrawCrosshair

The crosshair you play with, in its own colour and at its own size, between the
arrows that step through the 29 of them. 0 is none.
===============
*/
static void ST_DrawCrosshair(int i, float x, float y, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();
    int shape = atoi(st.values[i]);
    int index = (int)trap_Cvar_VariableValue("cg_crosshairColor") - 1;
    vec4_t color;
    char text[16];
    float size;

    if (index < 0 || index > 25) {
        index = 0;
    }
    SM_Fill(x, y, ST_VAL_W, 36, smGrey, clip);
    SM_Fill(x + 1, y + 1, ST_VAL_W - 2, 34, smInput, clip);

    Com_sprintf(text, sizeof(text), shape ? "%i" : "None", shape);
    SM_Text(x + 12, SM_Baseline(FONT_SANS, 15, y + 8, 22), FONT_SANS, 15, smSelectText, text, -1, qfalse);

    if (shape > 0 && shape < NUM_CROSSHAIRS) {
        size = trap_Cvar_VariableValue("cg_crosshairSize");
        if (size > 32.0f) {
            size = 32.0f;
        } else if (size < 16.0f) {
            size = 16.0f;
        }
        VectorScale(palette[index], trap_Cvar_VariableValue("cg_crosshairBrightness"), color);
        color[3] = 1.0f;
        trap_R_SetColor(color);
        SM_Pic(x + (ST_VAL_W - size) * 0.5f, y + (36 - size) * 0.5f, size, size,
               uiInfo.uiDC.Assets.crosshairShader[shape], clip);
        trap_R_SetColor(NULL);
    }

    SM_Text(x + ST_VAL_W - 44, SM_Baseline(FONT_SANS, 17, y + 7, 22), FONT_SANS, 17,
            SM_Over(ST_HIT_CROSSPREV, i) ? smYellow : smArrow, "<", -1, qtrue);
    SM_Text(x + ST_VAL_W - 21, SM_Baseline(FONT_SANS, 17, y + 7, 22), FONT_SANS, 17,
            SM_Over(ST_HIT_CROSSNEXT, i) ? smYellow : smArrow, ">", -1, qtrue);
    SM_Hit(x + ST_VAL_W - 50, y, 25, 36, ST_HIT_CROSSPREV, i, clip);
    SM_Hit(x + ST_VAL_W - 25, y, 25, 36, ST_HIT_CROSSNEXT, i, clip);
}

static void ST_DrawBind(int i, float x, float y, const smRect_t* clip) {
    qboolean waiting = st.binding == i;
    const char* text = waiting ? "Press a key, Escape to cancel" : st.values[i][0] ? st.values[i] : "-";

    SM_Fill(x, y, ST_VAL_W, 36, waiting ? smFocus : SM_Over(ST_HIT_BIND, i) ? smLighter : smGrey, clip);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 8, 24), FONT_SANS, 15, waiting ? smDark : smSelectText, text,
            SM_Fit(FONT_SANS, 15, text, ST_VAL_W - 20), qfalse);
    SM_Hit(x, y, ST_VAL_W, 36, ST_HIT_BIND, i, clip);
}

static void ST_DrawRow(int i, float x, float y, int nth, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float base = SM_Baseline(FONT_SANS, 15, y + 13.5f, 24);
    smRect_t row = SM_R(x, y, ST_COL_W, ST_ROW_H);
    qboolean hovered;

    if (cv->widget == ST_HEADING) {
        float tw = SM_SpacedWidth(FONT_SANS, 13, cv->title, 2);

        SM_Fill(x, y + 14, 3, 16, smYellow, clip);
        SM_SpacedText(x + 13, SM_Baseline(FONT_SANS, 13, y + 12, 20), FONT_SANS, 13, smYellow, cv->title, 2, qtrue);
        SM_Fill(x + 21 + tw, y + 21, ST_COL_W * 2 + ST_COL_GAP - 21 - tw, 1, smGrey, clip);
        return;
    }

    hovered = SM_In(&row, smMX, smMY) && st.select < 0;
    SM_Fill(x, y, ST_COL_W, ST_ROW_H, hovered ? smCard : (nth & 1) ? smRowOdd : smRowEven, clip);
    if (hovered) {
        SM_Fill(x, y, 2, ST_ROW_H, smYellow, clip);
    }
    SM_Text(x + 12, base, FONT_SANS, 15, hovered ? smText : smSelectText, cv->title,
            SM_Fit(FONT_SANS, 15, cv->title, ST_VAL_X - 20), qfalse);
    SM_Hit(x, y, ST_COL_W, ST_ROW_H, ST_HIT_ROW, i, clip);
    if (hovered) {
        st.tip = cv->tip;
        st.tipCvar = cv->cvar;
    }

    switch (cv->widget) {
        case ST_SELECT:
        case ST_PRESET:
            ST_DrawSelect(i, x + ST_VAL_X, y + 7.5f, clip);
            break;
        case ST_RANGE:
            ST_DrawRange(i, x + ST_VAL_X, y + 7.5f, clip);
            break;
        case ST_INPUT:
            ST_DrawInput(i, x + ST_VAL_X, y + 6.5f, clip);
            break;
        case ST_COLOR:
            ST_DrawColor(i, x + ST_VAL_X, y + 7.5f, clip);
            break;
        case ST_BIND:
            ST_DrawBind(i, x + ST_VAL_X, y + 7.5f, clip);
            break;
        case ST_CROSSHAIR:
            ST_DrawCrosshair(i, x + ST_VAL_X, y + 7.5f, clip);
            break;
        default:
            break;
    }
}

// the X at the top right, the way every panel of the web UI closes
static void ST_DrawClose(float page) {
    float x = page + ST_PAGE_W - 52;
    float y = ST_PAGE_TOP + 14;
    qboolean over = SM_Over(ST_HIT_CLOSE, 0);
    int k;

    for (k = 0; k < 18; k++) {
        SM_Fill(x + k, y + k, 2, 2, over ? smYellow : smArrow, NULL);
        SM_Fill(x + 17 - k, y + k, 2, 2, over ? smYellow : smArrow, NULL);
    }
    SM_Hit(x - 10, y - 10, 40, 40, ST_HIT_CLOSE, 0, NULL);
}

/*
===============
ST_DrawNav

The tabs, flat: what is open carries a yellow rule under it and the rest only
brighten under the cursor. Boxed tabs fight the rows below them for attention.
===============
*/
static void ST_DrawNav(float page) {
    float x = page + 11.39f;
    float y = ST_PAGE_TOP + ST_NAV_Y;
    int i;

    SM_Fill(page + 5, y + ST_NAV_H - 1, ST_PAGE_W - 10, 1, smGrey, NULL);
    for (i = 0; i < ST_NUM_TABS; i++) {
        float w = SM_SpacedWidth(FONT_SANS, 15, stTabNames[i], 1.5f) + 36;
        qboolean on = i == st.tab;
        qboolean over = SM_Over(ST_HIT_TAB, i);

        if (over && !on) {
            SM_Fill(x, y, w, ST_NAV_H - 1, smRowOdd, NULL);
        }
        SM_SpacedText(x + 18, SM_Baseline(FONT_SANS, 15, y + 8, 24), FONT_SANS, 15,
                      on ? smYellow : over ? smText : smSelectText, stTabNames[i], 1.5f, on);
        if (on) {
            SM_Fill(x, y + ST_NAV_H - 3, w, 3, smYellow, NULL);
        }
        SM_Hit(x, y, w, ST_NAV_H, ST_HIT_TAB, i, NULL);
        x += w;
    }
}

// .Select-menu-outer, under the control: options 40 high, at most 198 of them shown
static void ST_DrawDropdown(void) {
    const stCvar_t* cv = &stCvars[st.select];
    float x = st.selectControl.x;
    float y = st.selectControl.y + st.selectControl.h;
    float h = cv->numOptions * 40.0f;
    smRect_t box;
    int k;

    if (h > 198) {
        h = 198;
    }
    if (y + h > ST_PAGE_TOP + ST_PAGE_H) {
        y = st.selectControl.y - h;
    }
    box = SM_R(x, y, ST_VAL_W, h);
    SM_Fill(x - 1, y, ST_VAL_W + 2, h + 1, smShadow, NULL);
    SM_Fill(x, y, ST_VAL_W, h, smGrey, NULL);
    SM_Hit(x, y, ST_VAL_W, h, ST_HIT_MENU, st.select, NULL);
    st.selectMenu = box;
    for (k = 0; k < cv->numOptions; k++) {
        float oy = y + k * 40.0f;
        float base = SM_Baseline(FONT_SANS, 15, oy + 8, 24);
        const char* label = cv->widget == ST_PRESET ? ST_Presets(cv)[k].label : ST_Options(cv)[k].label;

        if (k == st.selectFocus) {
            SM_Fill(x, oy, ST_VAL_W, 40, smDark, &box);
        }
        if (SM_TextInside(base, 15, &box)) {
            SM_Text(x + 10, base, FONT_SANS, 15, k == st.selectFocus ? smSelectText : smOptionText, label,
                    SM_Fit(FONT_SANS, 15, label, ST_VAL_W - 20), qfalse);
        }
        SM_Hit(x, oy, ST_VAL_W, 40, ST_HIT_OPTION, k, &box);
    }
}

static void ST_DrawScrollbar(float x, float y) {
    float thumbH = st.view * st.view / st.content;
    float thumbY;

    if (thumbH < 16) {
        thumbH = 16;
    }
    thumbY = y + (st.view - thumbH) * st.scroll[st.tab] / (st.content - st.view);
    SM_Hit(x, y, 8, st.view, ST_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, 8, thumbH, (SM_Over(ST_HIT_THUMB, 0) || st.scrollDrag) ? smArrow : smGrey, NULL);
    SM_Hit(x, thumbY, 8, thumbH, ST_HIT_THUMB, 0, NULL);
}

static void ST_DrawPage(void) {
    float page = (smVW - ST_PAGE_W) * 0.5f;
    float listTop = ST_PAGE_TOP + ST_LIST_Y;
    smRect_t clip;
    int i;

    st.view = ST_PAGE_H - ST_LIST_Y - ST_TIP_H - 10;
    ST_Layout(page + 5);
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab]);
    st.tip = NULL;
    st.tipCvar = NULL;

    SM_FillScreen(stBackdrop);
    SM_Fill(page, ST_PAGE_TOP, ST_PAGE_W, ST_PAGE_H, smDim, NULL);
    // the title in Handel Gothic, as every heading in Quake Live is, over a yellow rule
    SM_SpacedText(page + 11.39f, SM_Baseline(FONT_DEFAULT, 32, ST_PAGE_TOP + 8, 38.4f), FONT_DEFAULT, 32, smText,
                  "Settings", -1, qfalse);
    SM_Fill(page + 11.39f, ST_PAGE_TOP + 46, 54, 3, smYellow, NULL);
    ST_DrawClose(page);
    ST_DrawNav(page);

    clip = SM_R(page + 5, listTop, ST_PAGE_W - 10, st.view);
    for (i = 0; i < ST_NUM_CVARS; i++) {
        float y;

        if (!ST_InTab(i)) {
            continue;
        }
        y = listTop + stLayout[i].y - st.scroll[st.tab];
        if (y + ST_ROW_H > listTop && y < listTop + st.view) {
            ST_DrawRow(i, stLayout[i].x, y, stLayout[i].nth, &clip);
        }
    }
    if (st.content > st.view) {
        ST_DrawScrollbar(page + ST_PAGE_W - 13, listTop);
    }
    // the bar along the bottom: what the row under the cursor does, and the cvar it is
    SM_Fill(page + 5, ST_PAGE_TOP + ST_PAGE_H - ST_TIP_H - 6, ST_PAGE_W - 10, 1, smGrey, NULL);
    if (st.tip) {
        SM_Text(page + 12.5f, SM_Baseline(FONT_SANS, 14, ST_PAGE_TOP + ST_PAGE_H - ST_TIP_H, 24), FONT_SANS, 14,
                smSelectText, st.tip, -1, qfalse);
    }
    if (st.tipCvar) {
        float w = SM_Advance(FONT_SANS, 14, st.tipCvar, -1);

        SM_Text(page + ST_PAGE_W - 12.5f - w, SM_Baseline(FONT_SANS, 14, ST_PAGE_TOP + ST_PAGE_H - ST_TIP_H, 24),
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

    st.select = i;
    st.selectFocus = cv->widget == ST_PRESET ? ST_PresetIndex(cv) : -1;
    if (cv->widget == ST_SELECT) {
        int k;

        for (k = 0; k < cv->numOptions; k++) {
            if (!Q_stricmp(ST_Options(cv)[k].value, st.values[i])) {
                st.selectFocus = k;
            }
        }
    }
}

static void ST_Choose(int k) {
    const stCvar_t* cv = &stCvars[st.select];

    if (k < 0 || k >= cv->numOptions) {
        return;
    }
    if (cv->widget == ST_PRESET) {
        ST_ApplyPreset(cv, k);
        ST_ReadAll();
    } else {
        ST_Set(st.select, ST_Options(cv)[k].value);
    }
    st.select = -1;
}

static void ST_DragRange(int i) {
    const stCvar_t* cv = &stCvars[i];
    float bx, bw = 213.07f;
    float frac, value;
    char text[32];

    bx = stLayout[i].x + ST_VAL_X + 53.27f;
    frac = (smMX - bx - 9) / (bw - 18);
    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    value = cv->min + frac * (cv->max - cv->min);
    ST_FormatValue(value, cv->max - cv->min, text, sizeof(text));
    ST_Set(i, text);
}

static void ST_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);
    int type = hit ? hit->type : -1;
    int index = hit ? hit->index : 0;

    // a click anywhere else closes an open dropdown, as react-select's blur does
    if (st.select >= 0 && type != ST_HIT_OPTION && type != ST_HIT_MENU) {
        st.select = -1;
        if (type == ST_HIT_SELECT && index == st.select) {
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
            break;
        case ST_HIT_SELECT:
            if (st.select == index) {
                st.select = -1;
            } else {
                ST_OpenSelect(index);
            }
            break;
        case ST_HIT_OPTION:
            ST_Choose(index);
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
        case ST_HIT_COLOR: {
            float sw = ST_VAL_W / 26.0f;
            int k = (int)((smMX - (stLayout[index].x + ST_VAL_X)) / sw) + 1;
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
        case ST_HIT_BIND:
            st.binding = index;
            break;
        case ST_HIT_CROSSPREV:
        case ST_HIT_CROSSNEXT: {
            int shape = atoi(st.values[index]) + (type == ST_HIT_CROSSNEXT ? 1 : -1);
            char text[16];

            if (shape < 0) {
                shape = NUM_CROSSHAIRS - 1;
            } else if (shape >= NUM_CROSSHAIRS) {
                shape = 0;
            }
            Com_sprintf(text, sizeof(text), "%i", shape);
            ST_Set(index, text);
            uiInfo.currentCrosshair = shape;
            break;
        }
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
        return;
    }
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab] + amount);
}

/*
===============
ST_Bind

Binds the key to the row's command, taking it off whatever it was bound to before
and dropping the oldest of the two the row already shows: the same two-binding
rule the .menu pages had.
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
            if (k == key) {
                return;  // already bound here
            }
            if (count < 2) {
                found[count] = k;
            }
            count++;
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
        st.tab = ST_TAB_GAME;
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
    if (st.scrollDrag && st.content > st.view) {
        float thumbH = st.view * st.view / st.content;

        if (thumbH < 16) {
            thumbH = 16;
        }
        st.scroll[st.tab] =
            ST_ClampScroll(st.scroll[st.tab] + (smMY - st.scrollGrab) * (st.content - st.view) / (st.view - thumbH));
        st.scrollGrab = smMY;
    }
    if (st.edit < 0 && st.binding < 0) {
        ST_ReadAll();
    }

    ST_DrawPage();
    if (st.select >= 0) {
        ST_DrawDropdown();
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
        case K_MOUSE2:
            // the mouse let go of a slider or the scrollbar
            st.drag = -1;
            st.scrollDrag = qfalse;
            return qtrue;
        case K_MWHEELUP:
            ST_Wheel(-100);
            return qtrue;
        case K_MWHEELDOWN:
            ST_Wheel(100);
            return qtrue;
        case K_ESCAPE:
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
