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
// ("web_changeHash /settings", ingame.menu's navbtn6), so the real page is in
// web.pak and there is nothing native to port. It is drawn here instead.
//
// Everything it offers is retail's: the ten sections in its nav's order, every
// cvar in its page's order with the title, widget, range and value list the web
// UI gives it, generated into ui_settings_table.h out of web.pak, and the buttons
// its pages end with. How it looks is quakeclub.com's, see ui_web.h. The Team
// section also keeps the model preview the pre-browser pages had.
//

#include "ui_web.h"

#define ST_MAX_INPUT 200

#define ST_PAD 25.0f         // inside a panel
#define ST_COL_GAP 32.0f
#define ST_HEADER_H 124.0f
#define ST_CONTROL_W 224.0f  // what a row's control takes at its right

#define ST_ROW_H 46.0f
#define ST_ROW_TIP_H 64.0f   // a row that says what it does under its title
#define ST_TIP_SIZE 12.5f
#define ST_TIP_LINE_H 17.0f
#define ST_TIP_LINES 2       // every retail tip fits two lines at this width
#define ST_BIND_H 42.0f
#define ST_HEAD_H 44.0f
#define ST_CARD_PAD 10.0f   // the room the card leaves below its last row

// Team's two blocks fit the shortest panel whole (773 high on a 16:9 screen), since
// a model is drawn only while all of it shows
#define ST_TEAM_ROW_H 41.0f
#define ST_TEAM_GAP 12.0f

#define ST_TEAM_MODEL_W 360.0f
#define ST_FIELD_H 36.0f
#define ST_OPTION_H 34.0f
#define ST_MENU_ROWS 8

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
    ST_HIT_ACTION,
    ST_HIT_DONE,
    ST_HIT_RGBSLIDER,
    ST_HIT_RGBPANEL,
    ST_HIT_MODEL,
    ST_HIT_MODELITEM,
    ST_HIT_MODELPANE,
    ST_HIT_SKINITEM,
    ST_HIT_MODELOK,
    ST_HIT_MODELCANCEL,
    ST_HIT_MODELCLEAR,
    ST_HIT_PREVIEW
} stHitType_t;

#define ST_MAX_SKINS 10  // the most a model of retail's table offers (biker)

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

    int select;                // the row whose dropdown is open
    smRect_t selectControl;
    float selectScroll;
    int drag;                  // the row whose slider is held
    int edit, editCursor, editStart, editTime;
    int binding;               // the row waiting for a key
    int rgb;                   // the row whose colour picker is open
    int rgbDrag;               // 0..2 while a channel is held
    smRect_t rgbPanel;

    int model;                 // the row whose model chooser is open
    int pickModel;             // what it shows: one of stModels
    int pickSkin;              // and one of its skins, or of the forced skin row's options
    int pickSkinRow;           // that forced skin row, or -1
    playerInfo_t modelInfo;
    char modelShown[MAX_QPATH];     // what modelInfo was built from

    playerInfo_t preview[2];        // the Team section's, teammates then enemies
    qhandle_t weaponIcon[10];      // [ioquakelive] each weapon's icon, registered on first use
    char previewShown[2][MAX_QPATH];
    float previewYaw[2];
    int previewDrag;                // which one is being turned
    float previewGrab;

    qboolean scrollDrag;
    float scrollGrab;

    // the frame, worked out once a frame
    float left, top;           // the content panel's
    float panelH;
    float colW;
    float listTop, view, content;
    qboolean fromIngame;
} st;

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
            Q_strcat(st.values[i], sizeof(st.values[i]), "\n");
        }
        Q_strcat(st.values[i], sizeof(st.values[i]), name);
        count++;
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

// what a list shows for its value: the option's name, or the raw value
static const char* ST_Shown(int i) {
    const stCvar_t* cv = &stCvars[i];
    int k;

    for (k = 0; k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, st.values[i])) {
            return cv->options[k].label;
        }
    }
    return st.values[i][0] ? st.values[i] : "Custom";
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

// 0xRRGGBBAA into the packed int the model tint takes, the way ui_main.c reads them
static int ST_ModelTint(const char* cvarName) {
    char buf[64];
    int rgb[3];

    trap_Cvar_VariableStringBuffer(cvarName, buf, sizeof(buf));
    ST_ParseRGB(buf, rgb);
    return (rgb[0] << 24) | (rgb[1] << 16) | (rgb[2] << 8) | 0xff;
}

/*
===============================================================================

THE SECTIONS

===============================================================================
*/

static qboolean ST_TabShown(int tab) {
    // retail's nav: {this.state.cvars.ui_joyavail > 0 ? Link to Gamepad : null}
    if (tab == ST_TAB_GAMEPAD) {
        return trap_Cvar_VariableValue("ui_joyavail") > 0;
    }
    return qtrue;
}

static const char* ST_TabAction(int tab) {
    switch (tab) {
        case ST_TAB_INPUT:
            return "Reset default bindings";
        case ST_TAB_BASIC:
        case ST_TAB_VIDEO:
            return "Apply video settings";
        case ST_TAB_SOUND:
            return "Apply sound settings";
        default:
            return NULL;
    }
}

static void ST_DoTabAction(int tab) {
    switch (tab) {
        case ST_TAB_INPUT:
            // retail's execDefault
            trap_Cmd_ExecuteText(EXEC_APPEND, "exec default.cfg\n");
            break;
        case ST_TAB_BASIC:
        case ST_TAB_VIDEO:
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
            break;
        case ST_TAB_SOUND:
            trap_Cmd_ExecuteText(EXEC_APPEND, "snd_restart\n");
            break;
        default:
            break;
    }
}

/*
===============================================================================

THE LIST

Where every row of the open section sits, worked out once a frame, so drawing and
clicking can never disagree about it. A heading lines both columns up again; one
with a column of its own heads only that column, the way Weapons heads its two.
The Team section is laid out apart: teammates, then enemies, each a block of rows
with its model beside it.

===============================================================================
*/

static struct {
    float x, y, w, h;
} stLayout[ST_NUM_CVARS];

static struct {
    float x, y, w, h;  // relative to the list, like the rows
} stModelBox[2];

static qboolean ST_InTab(int i) {
    return stCvars[i].tab == st.tab;
}

// the width a row's title and tip have, left of its control
static float ST_TitleWidth(float rowW) {
    return rowW - 12 - ST_CONTROL_W - 36;
}

static qboolean ST_HasTip(int i) {
    return stCvars[i].tip && stCvars[i].tab != ST_TAB_TEAM && stCvars[i].widget != ST_BIND;
}

/*
===============
ST_Tip

A tip wraps rather than being cut mid-word. Where its lines break is measured the
first time the row is laid out and kept, since the column never changes width.
===============
*/
typedef struct {
    float scale;  // the zoom it was measured at, 0 until then
    int lines;
    int starts[ST_TIP_LINES];
    int lens[ST_TIP_LINES];
} stTip_t;

static stTip_t stTips[ST_NUM_CVARS];

static const stTip_t* ST_Tip(int i) {
    stTip_t* tip = &stTips[i];
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    int n, k;

    if (tip->scale == smScale) {
        return tip;
    }
    n = SM_Wrap(FONT_SANS, ST_TIP_SIZE, stCvars[i].tip, ST_TitleWidth(st.colW), starts, lens);
    tip->scale = smScale;
    tip->lines = n < ST_TIP_LINES ? n : ST_TIP_LINES;
    for (k = 0; k < tip->lines; k++) {
        tip->starts[k] = starts[k];
        tip->lens[k] = lens[k];
    }
    // anything longer runs on in the last line, cut where the width ends
    if (n > ST_TIP_LINES) {
        const char* last = stCvars[i].tip + tip->starts[ST_TIP_LINES - 1];

        tip->lens[ST_TIP_LINES - 1] = SM_Fit(FONT_SANS, ST_TIP_SIZE, last, ST_TitleWidth(st.colW));
    }
    return tip;
}

static float ST_RowHeight(int i) {
    const stCvar_t* cv = &stCvars[i];

    if (cv->widget == ST_HEADING) {
        return ST_HEAD_H;
    }
    if (cv->widget == ST_BIND) {
        return ST_BIND_H;
    }
    if (st.tab == ST_TAB_TEAM) {
        return ST_TEAM_ROW_H;
    }
    if (!ST_HasTip(i)) {
        return ST_ROW_H;
    }
    return ST_ROW_TIP_H + (ST_Tip(i)->lines - 1) * ST_TIP_LINE_H;
}

/*
===============
ST_LayoutTeam

Teammates, then enemies, one block under the other. The table lists both headings
before any row, the way retail's page gives its two panels, so each block takes
the heading and the rows of its own column.
===============
*/
static void ST_LayoutTeam(float inner) {
    float rowsW = inner - ST_TEAM_MODEL_W - 24;
    float y = 0;
    int block, i;

    for (block = 0; block < 2; block++) {
        float blockTop;

        if (block) {
            y += ST_TEAM_GAP;
        }
        for (i = 0; i < ST_NUM_CVARS; i++) {
            if (ST_InTab(i) && stCvars[i].widget == ST_HEADING && stCvars[i].column == block) {
                stLayout[i].x = st.left + ST_PAD;
                stLayout[i].y = y;
                stLayout[i].w = rowsW;
                stLayout[i].h = ST_HEAD_H;
                y += ST_HEAD_H;
            }
        }
        blockTop = y;
        for (i = 0; i < ST_NUM_CVARS; i++) {
            if (!ST_InTab(i) || stCvars[i].widget == ST_HEADING || stCvars[i].column != block) {
                continue;
            }
            stLayout[i].x = st.left + ST_PAD;
            stLayout[i].y = y;
            stLayout[i].w = rowsW;
            stLayout[i].h = ST_TEAM_ROW_H;
            y += ST_TEAM_ROW_H;
        }
        stModelBox[block].x = st.left + ST_PAD + rowsW + 24;
        stModelBox[block].y = blockTop;
        stModelBox[block].w = ST_TEAM_MODEL_W;
        stModelBox[block].h = y - blockTop;
    }
    st.content = y;
}

static void ST_Layout(void) {
    float inner = HM_CONTAINER_W - 2 * ST_PAD;
    float y[2];
    float pairTop = -1;
    int i;

    st.colW = (inner - ST_COL_GAP) * 0.5f;
    if (st.tab == ST_TAB_TEAM) {
        ST_LayoutTeam(inner);
        return;
    }

    y[0] = y[1] = 0;
    for (i = 0; i < ST_NUM_CVARS; i++) {
        int c;

        if (!ST_InTab(i)) {
            continue;
        }
        if (stCvars[i].widget != ST_HEADING) {
            pairTop = -1;
        } else {
            float top = y[0] > y[1] ? y[0] : y[1];
            int col = stCvars[i].column;

            if (top > 0) {
                top += 16.0f;
            }
            // two column headings in a row are the two panels' titles: same line
            if (col >= 0 && pairTop >= 0) {
                top = pairTop;
            }
            pairTop = col >= 0 ? top : -1;
            stLayout[i].x = st.left + ST_PAD + (col > 0 ? st.colW + ST_COL_GAP : 0);
            stLayout[i].y = top;
            stLayout[i].w = col < 0 ? inner : st.colW;
            stLayout[i].h = ST_HEAD_H;
            y[0] = y[1] = top + ST_HEAD_H;
            continue;
        }
        c = stCvars[i].column ? 1 : 0;
        stLayout[i].x = st.left + ST_PAD + c * (st.colW + ST_COL_GAP);
        stLayout[i].y = y[c];
        stLayout[i].w = st.colW;
        stLayout[i].h = ST_RowHeight(i);
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

static float ST_RowY(int i) {
    return st.listTop + stLayout[i].y - st.scroll[st.tab];
}

// where a row's control starts
static float ST_ControlX(int i) {
    return stLayout[i].x + stLayout[i].w - 12 - ST_CONTROL_W;
}

/*
===============================================================================

THE CONTROLS

===============================================================================
*/

static void ST_DrawSelect(int i, float x, float y, const smRect_t* clip) {
    qboolean open = st.select == i;
    qboolean over = SM_Over(ST_HIT_SELECT, i);
    const char* label = ST_Shown(i);

    SM_FieldBox(x, y, ST_CONTROL_W, ST_FIELD_H, open, over, clip);
    SM_Text(x + 12, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, smText, label,
            SM_Fit(FONT_SANS, 15, label, ST_CONTROL_W - 44), qfalse);
    SM_Chevron(x + ST_CONTROL_W - 22, y + ST_FIELD_H * 0.5f - 2, open, over || open ? smText : smMuted);
    SM_Hit(x, y, ST_CONTROL_W, ST_FIELD_H, ST_HIT_SELECT, i, clip);
    if (open) {
        st.selectControl = SM_R(x, y, ST_CONTROL_W, ST_FIELD_H);
    }
}

static void ST_DrawBool(int i, float x, float y, const smRect_t* clip) {
    qboolean on = atoi(st.values[i]) != 0;
    float sx = x + ST_CONTROL_W - SM_SWITCH_W;
    float sy = y + (ST_FIELD_H - SM_SWITCH_H) * 0.5f;

    SM_Switch(sx, sy, on, SM_Over(ST_HIT_BOOL, i));
    SM_Hit(sx - 6, y, SM_SWITCH_W + 12, ST_FIELD_H, ST_HIT_BOOL, i, clip);
}

static void ST_DrawRange(int i, float x, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float value = ST_Snap(cv, (float)atof(st.values[i]));
    float trackX = x + 58;
    float trackW = ST_CONTROL_W - 58;
    char text[32];

    ST_FormatStep(cv, value, text, sizeof(text));
    SM_Text(x, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, smText, text, -1, qfalse);
    SM_SliderTrack(trackX, y + (ST_FIELD_H - SM_SLIDER_H) * 0.5f, trackW, ST_Fraction(cv, value),
                   SM_Over(ST_HIT_RANGE, i) || st.drag == i);
    SM_Hit(trackX - 6, y, trackW + 12, ST_FIELD_H, ST_HIT_RANGE, i, clip);
}

static void ST_DrawInput(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i];
    float width = ST_CONTROL_W - 24;
    int len = (int)strlen(value);
    int start = 0;
    qboolean editing = st.edit == i;

    SM_FieldBox(x, y, ST_CONTROL_W, ST_FIELD_H, editing, SM_Over(ST_HIT_INPUT, i), clip);
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
    if (!value[0] && !editing) {
        SM_Text(x + 12, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, smSubtle, "Empty", -1, qfalse);
    } else {
        SM_Text(x + 12, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, smText, value + start,
                SM_Fit(FONT_SANS, 15, value + start, width), qfalse);
    }
    // the caret; a steady one, since nothing on the page moves
    if (editing) {
        SM_Fill(x + 12 + SM_Advance(FONT_SANS, 15, value + start, st.editCursor - start), y + 9, 1.5f,
                ST_FIELD_H - 18, smText, clip);
    }
    SM_Hit(x, y, ST_CONTROL_W, ST_FIELD_H, ST_HIT_INPUT, i, clip);
}

// the 26 Quake Live colours in a strip with round ends, the one in use raised and ringed
static void ST_DrawPalette(int i, float x, float y, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();
    int value = atoi(st.values[i]);
    float h = 14.0f;
    float sy = y + (ST_FIELD_H - h) * 0.5f;
    float sw = (ST_CONTROL_W - h) / 26.0f;
    int k;

    SM_Pill(x, sy, h, h, palette[0], clip);
    SM_Pill(x + ST_CONTROL_W - h, sy, h, h, palette[25], clip);
    for (k = 0; k < 26; k++) {
        SM_Fill(x + h * 0.5f + k * sw, sy, sw + 0.3f, h, palette[k], clip);
    }
    if (value >= 1 && value <= 26) {
        float cx = x + h * 0.5f + (value - 1) * sw;

        SM_Round(cx - 3, sy - 5, sw + 6, h + 10, 5, smPrimary, clip);
        SM_Round(cx - 1, sy - 3, sw + 2, h + 6, 4, palette[value - 1], clip);
    }
    SM_Hit(x, y, ST_CONTROL_W, ST_FIELD_H, ST_HIT_PALETTE, i, clip);
}

// a colour of the row's own: its swatch and its channels; a click opens the picker
static void ST_DrawRGB(int i, float x, float y, const smRect_t* clip) {
    int rgb[3];
    vec4_t color;
    char text[32];

    ST_ParseRGB(st.values[i], rgb);
    color[0] = rgb[0] / 255.0f;
    color[1] = rgb[1] / 255.0f;
    color[2] = rgb[2] / 255.0f;
    color[3] = 1.0f;

    SM_FieldBox(x, y, ST_CONTROL_W, ST_FIELD_H, st.rgb == i, SM_Over(ST_HIT_RGB, i), clip);
    SM_Round(x + 7, y + 7, 22, ST_FIELD_H - 14, 5, color, clip);
    Com_sprintf(text, sizeof(text), "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
    SM_Text(x + 40, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, smText, text, -1, qfalse);
    SM_Chevron(x + ST_CONTROL_W - 22, y + ST_FIELD_H * 0.5f - 2, st.rgb == i, smMuted);
    SM_Hit(x, y, ST_CONTROL_W, ST_FIELD_H, ST_HIT_RGB, i, clip);
}

// a small round button with a chevron pointing left or right
static void ST_DrawStep(float x, float y, qboolean right, qboolean over) {
    float s = 26.0f;
    int k;

    SM_Pill(x, y, s, s, over ? smRaisedHover : smRaised, NULL);
    for (k = 0; k < 5; k++) {
        float xx = right ? x + 11 + k : x + 15 - k;

        SM_Fill(xx, y + 8 + k, 1.6f, 1.6f, over ? smText : smMuted, NULL);
        SM_Fill(xx, y + 16.4f - k, 1.6f, 1.6f, over ? smText : smMuted, NULL);
    }
}

static void ST_DrawCrosshair(int i, float x, float y, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();
    int shape = atoi(st.values[i]);
    int index = (int)trap_Cvar_VariableValue("cg_crosshairColor") - 1;
    float fieldW = ST_CONTROL_W - 2 * 32;
    vec4_t color;
    char text[16];

    SM_FieldBox(x, y, fieldW, ST_FIELD_H, qfalse, qfalse, clip);
    Com_sprintf(text, sizeof(text), shape ? "%i" : "None", shape);
    SM_Text(x + 12, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, smText, text, -1, qfalse);
    if (shape > 0 && shape < NUM_CROSSHAIRS) {
        if (index < 0 || index > 25) {
            index = 0;
        }
        VectorScale(palette[index], trap_Cvar_VariableValue("cg_crosshairBrightness"), color);
        color[3] = 1.0f;
        trap_R_SetColor(color);
        SM_Pic(x + fieldW - 36, y + 4, 28, 28, uiInfo.uiDC.Assets.crosshairShader[shape], clip);
        trap_R_SetColor(NULL);
    }
    // the whole field opens the grid; the arrows it had before meant stepping through one by one
    SM_Hit(x, y, fieldW, ST_FIELD_H, ST_HIT_SELECT, i, clip);
    if (st.select == i) {
        st.selectControl = SM_R(x, y, fieldW, ST_FIELD_H);
    }
}

// the keys bound to the row's command, as key caps
static void ST_DrawBind(int i, float right, float y, const smRect_t* clip) {
    qboolean waiting = st.binding == i;
    qboolean over = SM_Over(ST_HIT_BIND, i);
    char keys[2][64];
    int count = 0;
    float x = right;
    float h = 24.0f;
    float ky = y + (ST_BIND_H - h) * 0.5f;
    const char* p = st.values[i];
    int k;

    if (waiting) {
        float w = SM_LabelWidth(12, "Press a key", 0.6f) + 16;

        SM_Kbd(right - w, ky, "Press a key", qtrue, clip);
        SM_Hit(right - ST_CONTROL_W, y, ST_CONTROL_W, ST_BIND_H, ST_HIT_BIND, i, clip);
        return;
    }
    while (*p && count < 2) {
        const char* end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);

        Q_strncpyz(keys[count], p, len + 1 < (int)sizeof(keys[count]) ? len + 1 : sizeof(keys[count]));
        count++;
        p += len;
        if (*p == '\n') {
            p++;
        }
    }
    if (!count) {
        float w = SM_LabelWidth(11, "Unbound", 1.2f);

        SM_Label(right - w, SM_Baseline(FONT_SANS, 11, y, ST_BIND_H), 11, over ? smMuted : smSubtle, "Unbound", 1.2f);
    }
    for (k = count - 1; k >= 0; k--) {
        float w = SM_LabelWidth(12, keys[k], 0.6f) + 16;

        if (w < h) {
            w = h;
        }
        x -= w;
        SM_Kbd(x, ky, keys[k], qfalse, clip);
        x -= 6;
    }
    SM_Hit(right - ST_CONTROL_W, y, ST_CONTROL_W, ST_BIND_H, ST_HIT_BIND, i, clip);
}

/*
===============
ST_DrawTeamPreview

The model teammates or enemies are forced to, in the colours the rows set. Retail
composes "model/skin" the same way (uix86.dll UI_DrawTeamModel @0x10005850): a
model alone takes its default skin, a skin alone falls back to sarge. Dragging it
turns it.
===============
*/
static void ST_DrawTeamPreview(int slot, float x, float y, float w, float h) {
    static const char* const models[2] = {"cg_forceTeamModel", "cg_forceEnemyModel"};
    static const char* const skins[2] = {"cg_forceTeamSkin", "cg_forceEnemySkin"};
    static const char* const head[2] = {"cg_teamHeadColor", "cg_enemyHeadColor"};
    static const char* const upper[2] = {"cg_teamUpperColor", "cg_enemyUpperColor"};
    static const char* const lower[2] = {"cg_teamLowerColor", "cg_enemyLowerColor"};
    char model[MAX_QPATH], skin[MAX_QPATH], composed[MAX_QPATH];
    playerInfo_t* pi = &st.preview[slot];
    char* slash;
    const char* hint;
    float hw;

    SM_Round(x, y, w, h, SM_RADIUS_MD, smRaised, NULL);

    trap_Cvar_VariableStringBuffer(models[slot], model, sizeof(model));
    trap_Cvar_VariableStringBuffer(skins[slot], skin, sizeof(skin));
    if (!model[0] && !skin[0]) {
        const char* text = slot ? "Enemies keep their own model" : "Teammates keep their own model";

        SM_Centered(x + w * 0.5f, SM_Baseline(FONT_SANS, 14, y, h), 14, smSubtle, text, qfalse);
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
        Q_strncpyz(st.previewShown[slot], composed, sizeof(st.previewShown[slot]));
        SM_BuildPlayer(pi, composed, st.previewYaw[slot], WP_MACHINEGUN);
    }
    SM_TurnPlayer(pi, st.previewYaw[slot]);
    pi->customColor = qtrue;
    pi->headColor = ST_ModelTint(head[slot]);
    pi->torsoColor = ST_ModelTint(upper[slot]);
    pi->legsColor = ST_ModelTint(lower[slot]);

    SM_DrawPlayerIn(pi, x, y + 14, w, h - 44);

    hint = "Drag to turn";
    hw = SM_LabelWidth(10, hint, 1.4f);
    SM_Label(x + (w - hw) * 0.5f, SM_Baseline(FONT_SANS, 10, y + h - 30, 22), 10, smSubtle, hint, 1.4f);
    SM_Hit(x, y, w, h, ST_HIT_PREVIEW, slot, NULL);
}

/*
===============================================================================

PLAYER MODELS

Retail's chooser (web.pak src/modelchooser.js) lists its own table of models,
src/models.js, not what is on disk: 26 models, the skins each one offers in that
order, and the names it shows. A skin is shown by its icon,
models/players/<model>/icon_<skin>, which pak00 has for every pair but two
(ranger's sport and uriel's zael); those show their name instead.

The chooser holds what it shows until OK, the way retail's does, and Enter and
Escape are its OK and Cancel. The player's own model takes a skin with it
(modelskin). A forced model is the model alone in retail, whose skin is the row
under it; here the chooser offers that row's skins as well, Bright among them,
so a forced model and its skin are picked and seen together.

===============================================================================
*/

typedef struct {
    const char* model;
    const char* name;
    const char* skins[ST_MAX_SKINS + 1];
} stModel_t;

static const stModel_t stModels[] = {
    {"anarki", "Anarki", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"biker", "Biker",
     {"default", "red", "blue", "bright", "hossman", "slammer", "sport", "sport_blue", "sport_red", "stroggo"}},
    {"bitterman", "Bitterman", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"bones", "Bones", {"default", "red", "blue", "bright", "bones", "sport", "sport_blue", "sport_red"}},
    {"crash", "Crash", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"doom", "Doom", {"default", "red", "blue", "bright", "phobos", "sport", "sport_blue", "sport_red"}},
    {"grunt", "Grunt", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red", "stripe"}},
    {"hunter", "Hunter", {"default", "red", "blue", "bright", "harpy", "sport", "sport_blue", "sport_red"}},
    {"james", "James", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"janet", "Janet", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"keel", "Keel", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"klesk", "Klesk", {"default", "red", "blue", "bright", "flisk", "sport", "sport_blue", "sport_red"}},
    {"lucy", "Lucy", {"default", "red", "blue", "bright", "angel", "sport", "sport_blue", "sport_red"}},
    {"major", "Major", {"default", "red", "blue", "bright", "daemia", "sport", "sport_blue", "sport_red"}},
    {"mynx", "Mynx", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"orbb", "Orbb", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"ranger", "Ranger", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red", "wrack"}},
    {"razor", "Razor", {"default", "red", "blue", "bright", "id", "patriot", "sport", "sport_blue", "sport_red"}},
    {"santa", "Santa", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"sarge", "Sarge",
     {"default", "red", "blue", "bright", "krusade", "roderic", "sport", "sport_blue", "sport_red"}},
    {"slash", "Slash", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red", "yuriko"}},
    {"sorlag", "Sorlag", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"tankjr", "Tank Jr.", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
    {"uriel", "Uriel", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red", "zael"}},
    {"visor", "Visor", {"default", "red", "blue", "bright", "gorre", "sport", "sport_blue", "sport_red"}},
    {"xaero", "Xaero", {"default", "red", "blue", "bright", "sport", "sport_blue", "sport_red"}},
};

#define ST_NUM_MODELS ((int)ARRAY_LEN(stModels))
#define ST_DEFAULT_MODEL 19  // sarge, what retail's sanitizeModel falls back to

// the chooser's grid and its skin row
#define ST_CHOOSER_W 1120.0f
#define ST_CHOOSER_H 690.0f
#define ST_GRID_COLS 5
#define ST_GRID_CELL 86.0f
#define ST_SKIN_CELL 58.0f

static qhandle_t stIcons[ST_NUM_MODELS][ST_MAX_SKINS];  // 0 not asked for yet, -1 not there

static qhandle_t ST_IconFor(int model, const char* skin) {
    int k;

    for (k = 0; k < ST_MAX_SKINS && stModels[model].skins[k]; k++) {
        if (!Q_stricmp(stModels[model].skins[k], skin)) {
            qhandle_t* h = &stIcons[model][k];

            if (!*h) {
                // a missing image registers as 0, and asking again costs nothing
                *h = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_%s", stModels[model].model, skin));
                if (!*h) {
                    *h = -1;
                }
            }
            return *h > 0 ? *h : 0;
        }
    }
    return 0;
}

// what a skin is called: the forced skin rows' names, else the skin with a capital
static const char* ST_SkinName(const char* skin) {
    static char name[MAX_QPATH];
    const stCvar_t* cv = NULL;
    int k;

    if (!skin[0]) {
        return "None";
    }
    if (!Q_stricmp(skin, "default")) {
        return "Default";
    }
    if (!Q_stricmp(skin, "id")) {
        return "id";
    }
    for (k = 0; k < ST_NUM_CVARS; k++) {
        if (stCvars[k].cvar && !Q_stricmp(stCvars[k].cvar, "cg_forceteamskin")) {
            cv = &stCvars[k];
            break;
        }
    }
    for (k = 0; cv && k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, skin)) {
            return cv->options[k].label;
        }
    }
    Q_strncpyz(name, skin, sizeof(name));
    name[0] = (char)toupper((unsigned char)name[0]);
    return name;
}

/*
===============
ST_ParseModel

Retail's sanitizeModel: "model/skin" into the table, a model it does not list
becoming sarge and a skin its model does not offer becoming default. The skin
comes back as the text after the slash, or "" when there is none.
===============
*/
static int ST_ParseModel(const char* value, char* skin, int size) {
    char base[MAX_QPATH];
    const char* slash = strchr(value, '/');
    int len = slash ? (int)(slash - value) + 1 : (int)sizeof(base);
    int model = ST_DEFAULT_MODEL;
    int k;

    Q_strncpyz(base, value, len < (int)sizeof(base) ? len : (int)sizeof(base));
    for (k = 0; k < ST_NUM_MODELS; k++) {
        if (!Q_stricmp(stModels[k].model, base)) {
            model = k;
            break;
        }
    }
    skin[0] = '\0';
    if (slash) {
        for (k = 0; k < ST_MAX_SKINS && stModels[model].skins[k]; k++) {
            if (!Q_stricmp(stModels[model].skins[k], slash + 1)) {
                Q_strncpyz(skin, stModels[model].skins[k], size);
                break;
            }
        }
    }
    return model;
}

// the skin row that goes with a forced model row, or -1
static int ST_SkinRowFor(int i) {
    const char* want = NULL;
    int k;

    if (!Q_stricmp(stCvars[i].cvar, "cg_forceteammodel")) {
        want = "cg_forceteamskin";
    } else if (!Q_stricmp(stCvars[i].cvar, "cg_forceenemymodel")) {
        want = "cg_forceenemyskin";
    }
    for (k = 0; want && k < ST_NUM_CVARS; k++) {
        if (stCvars[k].cvar && !Q_stricmp(stCvars[k].cvar, want)) {
            return k;
        }
    }
    return -1;
}

// how many skins the chooser offers, and the value of each
static int ST_PickSkinCount(void) {
    int k;

    if (st.pickSkinRow >= 0) {
        return stCvars[st.pickSkinRow].numOptions;
    }
    for (k = 0; k < ST_MAX_SKINS && stModels[st.pickModel].skins[k]; k++) {
    }
    return k;
}

static const char* ST_PickSkinValue(int k) {
    if (st.pickSkinRow >= 0) {
        return stCvars[st.pickSkinRow].options[k].value;
    }
    return stModels[st.pickModel].skins[k];
}

static void ST_OpenChooser(int i) {
    char skin[MAX_QPATH];
    int k;

    st.model = i;
    st.pickModel = ST_ParseModel(st.values[i], skin, sizeof(skin));
    st.pickSkinRow = stCvars[i].widget == ST_MODEL ? ST_SkinRowFor(i) : -1;
    st.pickSkin = 0;
    // a forced skin is its row's; a skin written into the model ("keel/bright") counts when that row is empty
    if (st.pickSkinRow >= 0 && st.values[st.pickSkinRow][0]) {
        Q_strncpyz(skin, st.values[st.pickSkinRow], sizeof(skin));
    }
    for (k = 0; k < ST_PickSkinCount(); k++) {
        if (skin[0] && !Q_stricmp(ST_PickSkinValue(k), skin)) {
            st.pickSkin = k;
            break;
        }
    }
    st.modelShown[0] = '\0';
}

static void ST_ConfirmChooser(void) {
    const char* model = stModels[st.pickModel].model;

    if (stCvars[st.model].widget == ST_MODELSKIN) {
        ST_Set(st.model, va("%s/%s", model, ST_PickSkinValue(st.pickSkin)));
    } else {
        ST_Set(st.model, model);
        if (st.pickSkinRow >= 0) {
            ST_Set(st.pickSkinRow, ST_PickSkinValue(st.pickSkin));
        }
    }
    st.model = -1;
}

// the model in use with its icon; a click opens the chooser
static void ST_DrawModelButton(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i][0] ? st.values[i] : "None";
    qhandle_t icon = 0;
    float tx = x + 12;

    if (st.values[i][0]) {
        char skin[MAX_QPATH];
        int model = ST_ParseModel(st.values[i], skin, sizeof(skin));

        icon = ST_IconFor(model, skin[0] ? skin : "default");
    }

    SM_FieldBox(x, y, ST_CONTROL_W, ST_FIELD_H, st.model == i, SM_Over(ST_HIT_MODEL, i), clip);
    if (icon) {
        SM_Pic(x + 5, y + 4, ST_FIELD_H - 8, ST_FIELD_H - 8, icon, clip);
        tx = x + ST_FIELD_H + 6;
    }
    SM_Text(tx, SM_Baseline(FONT_SANS, 15, y, ST_FIELD_H), FONT_SANS, 15, st.values[i][0] ? smText : smTextSoft,
            value, SM_Fit(FONT_SANS, 15, value, x + ST_CONTROL_W - 30 - tx), qfalse);
    SM_Chevron(x + ST_CONTROL_W - 22, y + ST_FIELD_H * 0.5f - 2, qfalse, smMuted);
    SM_Hit(x, y, ST_CONTROL_W, ST_FIELD_H, ST_HIT_MODEL, i, clip);
}

// the chooser's model, in the skin it has picked and in the colours of the row it came from
static void ST_DrawChooserPreview(float x, float y, float w, float h) {
    const char* cvar = stCvars[st.model].cvar;
    const char* skin = ST_PickSkinValue(st.pickSkin);
    char composed[MAX_QPATH];

    Com_sprintf(composed, sizeof(composed), "%s/%s", stModels[st.pickModel].model, skin[0] ? skin : "default");
    if (Q_stricmp(composed, st.modelShown)) {
        Q_strncpyz(st.modelShown, composed, sizeof(st.modelShown));
        SM_BuildPlayer(&st.modelInfo, composed, 150, WP_MACHINEGUN);
    }
    st.modelInfo.customColor = qfalse;
    if (!Q_stricmp(cvar, "cg_forceteammodel")) {
        st.modelInfo.customColor = qtrue;
        st.modelInfo.headColor = ST_ModelTint("cg_teamHeadColor");
        st.modelInfo.torsoColor = ST_ModelTint("cg_teamUpperColor");
        st.modelInfo.legsColor = ST_ModelTint("cg_teamLowerColor");
    } else if (!Q_stricmp(cvar, "cg_forceenemymodel")) {
        st.modelInfo.customColor = qtrue;
        st.modelInfo.headColor = ST_ModelTint("cg_enemyHeadColor");
        st.modelInfo.torsoColor = ST_ModelTint("cg_enemyUpperColor");
        st.modelInfo.legsColor = ST_ModelTint("cg_enemyLowerColor");
    }
    SM_DrawPlayerIn(&st.modelInfo, x, y, w, h);
}

/*
===============
ST_DrawModelChooser

Retail's is a wide dialog: every model down the left, and on the right the name
of the one picked, its skins and a picture of it. Here the picture is the model
itself. A tile is the model's default icon; the one picked has the cream ring the
site gives a chosen pill.
===============
*/
static void ST_DrawModelChooser(void) {
    float w = ST_CHOOSER_W, h = ST_CHOOSER_H;
    float x = (smVW - w) * 0.5f;
    float y = (SM_VIEW_H - h) * 0.5f;
    float bodyTop = y + 104;
    float footY = y + h - ST_PAD - SM_BUTTON_H;
    float gridX = x + ST_PAD;
    float paneX = gridX + ST_GRID_COLS * ST_GRID_CELL + 22;
    float paneW = x + w - ST_PAD - paneX;
    float skinY = bodyTop + 50;
    float previewY = skinY + ST_SKIN_CELL + 18;
    qboolean forced = st.pickSkinRow >= 0;
    vec4_t scrim = {0.0f, 0.0f, 0.0f, 0.62f};
    const char* skinValue = ST_PickSkinValue(st.pickSkin);
    const char* title = stModels[st.pickModel].name;
    float bx;
    int count = ST_PickSkinCount();
    int k;

    SM_FillScreen(scrim);
    SM_Shadow(x, y, w, h, SM_RADIUS_LG);
    SM_Round(x, y, w, h, SM_RADIUS_LG, smPanel, NULL);
    SM_Hit(x, y, w, h, ST_HIT_MODELPANE, 0, NULL);
    SM_Label(x + ST_PAD, SM_Baseline(FONT_SANS, 11, y + 24, 16), 11, smGold, "Choose a model", 1.6f);
    SM_Label(x + ST_PAD, SM_Baseline(FONT_SANS, 24, y + 44, 32), 24, smText, stCvars[st.model].title, 0.2f);
    SM_Fill(x + ST_PAD, y + 88, w - 2 * ST_PAD, 1, smBorder, NULL);

    // every model, in its default skin
    for (k = 0; k < ST_NUM_MODELS; k++) {
        float cx = gridX + (k % ST_GRID_COLS) * ST_GRID_CELL;
        float cy = bodyTop + (k / ST_GRID_COLS) * ST_GRID_CELL;
        float cs = ST_GRID_CELL - 8;
        qboolean chosen = k == st.pickModel;
        qboolean over = SM_Over(ST_HIT_MODELITEM, k);
        qhandle_t icon = ST_IconFor(k, "default");
        const char* name = stModels[k].name;
        float tw;

        if (chosen) {
            SM_Round(cx - 2, cy - 2, cs + 4, cs + 4, SM_RADIUS_MD + 2, smPrimary, NULL);
        }
        SM_Round(cx, cy, cs, cs, SM_RADIUS_MD, chosen ? smRaisedHover : over ? smRaisedHover : smRaised, NULL);
        if (icon) {
            SM_Pic(cx + (cs - 46) * 0.5f, cy + 6, 46, 46, icon, NULL);
        }
        tw = SM_Advance(FONT_SANS, 11, name, -1);
        if (tw > cs - 8) {
            tw = cs - 8;
        }
        SM_Text(cx + (cs - tw) * 0.5f, SM_Baseline(FONT_SANS, 11, cy + cs - 22, 16), FONT_SANS, 11,
                chosen || over ? smText : smMuted, name, SM_Fit(FONT_SANS, 11, name, cs - 8), qfalse);
        SM_Hit(cx, cy, cs, cs, ST_HIT_MODELITEM, k, NULL);
    }

    // the one picked: its name and skin, its skins, and the model
    {
        float nw = SM_LabelWidth(26, title, 0.2f);
        const char* skinName = ST_SkinName(skinValue);

        SM_Label(paneX, SM_Baseline(FONT_SANS, 26, bodyTop, 34), 26, smText, title, 0.2f);
        SM_Text(paneX + nw + 14, SM_Baseline(FONT_SANS, 15, bodyTop + 4, 34), FONT_SANS, 15, smGold, skinName, -1,
                qfalse);
    }
    for (k = 0; k < count; k++) {
        const char* value = ST_PickSkinValue(k);
        float cx = paneX + k * ST_SKIN_CELL;
        float cs = ST_SKIN_CELL - 6;
        qboolean chosen = k == st.pickSkin;
        qboolean over = SM_Over(ST_HIT_SKINITEM, k);
        qhandle_t icon = ST_IconFor(st.pickModel, value[0] ? value : "default");

        if (chosen) {
            SM_Round(cx - 2, skinY - 2, cs + 4, cs + 4, SM_RADIUS_MD + 2, smPrimary, NULL);
        }
        SM_Round(cx, skinY, cs, cs, SM_RADIUS_MD, chosen || over ? smRaisedHover : smRaised, NULL);
        if (icon && value[0]) {
            SM_Pic(cx + 5, skinY + 5, cs - 10, cs - 10, icon, NULL);
        } else {
            // None (the players' own skin), or a skin pak00 has no icon for
            const char* name = ST_SkinName(value);

            SM_Text(cx + (cs - SM_Advance(FONT_SANS, 10, name, -1)) * 0.5f, SM_Baseline(FONT_SANS, 10, skinY, cs),
                    FONT_SANS, 10, chosen || over ? smText : smMuted, name, SM_Fit(FONT_SANS, 10, name, cs - 6),
                    qfalse);
        }
        SM_Hit(cx, skinY, cs, cs, ST_HIT_SKINITEM, k, NULL);
    }

    SM_Round(paneX, previewY, paneW, footY - 18 - previewY, SM_RADIUS_MD, smRaised, NULL);
    ST_DrawChooserPreview(paneX, previewY + 10, paneW, footY - 18 - previewY - 20);
    if (forced) {
        const char* note = va("The skin is %s's too", stCvars[st.pickSkinRow].title);

        SM_Text(paneX + 14, SM_Baseline(FONT_SANS, 12, footY - 18 - 28, 20), FONT_SANS, 12, smSubtle, note, -1,
                qfalse);
    }

    // Clear (a forced model may be none), Cancel and OK, the way retail's ends
    bx = x + w - ST_PAD;
    {
        static const char* const labels[3] = {"OK", "Cancel", "Clear"};
        static const int hits[3] = {ST_HIT_MODELOK, ST_HIT_MODELCANCEL, ST_HIT_MODELCLEAR};

        for (k = 0; k < (forced ? 3 : 2); k++) {
            float bw = SM_ButtonWidth(labels[k], SM_BUTTON_H);

            bx -= bw;
            SM_Button(bx, footY, bw, SM_BUTTON_H, labels[k], k ? SM_BUTTON_SECONDARY : SM_BUTTON_PRIMARY,
                      SM_Over(hits[k], 0));
            SM_Hit(bx, footY, bw, SM_BUTTON_H, hits[k], 0, NULL);
            bx -= 10;
        }
    }
}

/*
===============
ST_DrawCrosshairGrid

All thirty crosshairs on show, in a grid, over the control. Retail offers them in a dropdown
rather than making you step through them one arrow at a time, which is what was here: reaching
number 24 meant passing the 23 before it. In a grid you see each one's shape instead of its
number, which is the only thing that matters when choosing.
===============
*/
#define ST_XH_CELL 40.0f
#define ST_XH_COLS 6

static void ST_DrawCrosshairGrid(void) {
    const vec4_t* palette = UI_CrosshairPalette();
    int index = (int)trap_Cvar_VariableValue("cg_crosshairColor") - 1;
    int rows = (NUM_CROSSHAIRS + ST_XH_COLS - 1) / ST_XH_COLS;
    float pad = 8.0f;
    float w = ST_XH_COLS * ST_XH_CELL + 2 * pad;
    float h = rows * ST_XH_CELL + 2 * pad;
    float x = st.selectControl.x;
    float y = st.selectControl.y + st.selectControl.h + 6;
    int chosen = atoi(st.values[st.select]);
    vec4_t color;
    int k;

    if (y + h > smBottom - 20) {
        y = st.selectControl.y - 6 - h;
    }
    if (index < 0 || index > 25) {
        index = 0;
    }
    VectorScale(palette[index], trap_Cvar_VariableValue("cg_crosshairBrightness"), color);
    color[3] = 1.0f;

    SM_Round(x, y, w, h, SM_RADIUS_MD, smRaised, NULL);
    SM_Hit(x, y, w, h, ST_HIT_MENU, st.select, NULL);

    for (k = 0; k < NUM_CROSSHAIRS; k++) {
        float cxx = x + pad + (k % ST_XH_COLS) * ST_XH_CELL;
        float cyy = y + pad + (k / ST_XH_COLS) * ST_XH_CELL;
        qboolean over = SM_Over(ST_HIT_OPTION, k);

        if (k == chosen || over) {
            SM_Round(cxx + 2, cyy + 2, ST_XH_CELL - 4, ST_XH_CELL - 4, SM_RADIUS_SM,
                     k == chosen ? smRaisedHover : smPanel, NULL);
        }
        if (k == 0) {
            // zero is "none", and has no shape to show
            SM_Label(cxx + 6, SM_Baseline(FONT_SANS, 10, cyy, ST_XH_CELL), 10,
                     (k == chosen || over) ? smText : smMuted, "OFF", 1.2f);
            SM_Hit(cxx, cyy, ST_XH_CELL, ST_XH_CELL, ST_HIT_OPTION, k, NULL);
            continue;
        }
        trap_R_SetColor(color);
        SM_Pic(cxx + 6, cyy + 6, ST_XH_CELL - 12, ST_XH_CELL - 12, uiInfo.uiDC.Assets.crosshairShader[k], NULL);
        trap_R_SetColor(NULL);
        if (k == chosen) {
            SM_Pill(cxx + ST_XH_CELL * 0.5f - 3, cyy + ST_XH_CELL - 7, 6, 3, smGold, NULL);
        }
        SM_Hit(cxx, cyy, ST_XH_CELL, ST_XH_CELL, ST_HIT_OPTION, k, NULL);
    }
}

/*
===============================================================================

POPUPS

===============================================================================
*/

// the list under a select: the site's popover, raised, with a shadow
static void ST_DrawDropdown(void) {
    const stCvar_t* cv = &stCvars[st.select];
    float x = st.selectControl.x;
    float y = st.selectControl.y + st.selectControl.h + 6;
    float pad = 6.0f;
    int shown = cv->numOptions < ST_MENU_ROWS ? cv->numOptions : ST_MENU_ROWS;
    float h = shown * ST_OPTION_H + 2 * pad;
    float full = cv->numOptions * ST_OPTION_H;
    smRect_t box;
    int k;

    if (y + h > smBottom - 20) {
        y = st.selectControl.y - 6 - h;
    }
    if (st.selectScroll > full - shown * ST_OPTION_H) {
        st.selectScroll = full - shown * ST_OPTION_H;
    }
    if (st.selectScroll < 0) {
        st.selectScroll = 0;
    }
    SM_Shadow(x, y, ST_CONTROL_W, h, SM_RADIUS_MD);
    SM_Round(x, y, ST_CONTROL_W, h, SM_RADIUS_MD, smRaised, NULL);
    SM_Hit(x, y, ST_CONTROL_W, h, ST_HIT_MENU, st.select, NULL);
    box = SM_R(x, y + pad, ST_CONTROL_W, h - 2 * pad);
    for (k = 0; k < cv->numOptions; k++) {
        float oy = box.y + k * ST_OPTION_H - st.selectScroll;
        qboolean chosen = !Q_stricmp(cv->options[k].value, st.values[st.select]);
        qboolean over = SM_Over(ST_HIT_OPTION, k);

        if (oy + ST_OPTION_H < box.y || oy > box.y + box.h) {
            continue;
        }
        if (over) {
            SM_Round(x + pad, oy, ST_CONTROL_W - 2 * pad, ST_OPTION_H, SM_RADIUS_SM, smRaisedHover, &box);
        }
        if (SM_TextInside(SM_Baseline(FONT_SANS, 14, oy, ST_OPTION_H), 14, &box)) {
            SM_Text(x + 16, SM_Baseline(FONT_SANS, 14, oy, ST_OPTION_H), FONT_SANS, 14,
                    chosen || over ? smText : smTextSoft, cv->options[k].label,
                    SM_Fit(FONT_SANS, 14, cv->options[k].label, ST_CONTROL_W - 48), chosen);
            if (chosen) {
                SM_Pill(x + ST_CONTROL_W - 22, oy + ST_OPTION_H * 0.5f - 3, 6, 6, smGold, &box);
            }
        }
        SM_Hit(x + pad, oy, ST_CONTROL_W - 2 * pad, ST_OPTION_H, ST_HIT_OPTION, k, &box);
    }
}

// the three channels of a colour of the row's own
static void ST_DrawRGBPicker(void) {
    static const char* const names[3] = {"Red", "Green", "Blue"};
    float w = 300, h = 196;
    float x = ST_ControlX(st.rgb) + ST_CONTROL_W - w;
    float y = ST_RowY(st.rgb) + stLayout[st.rgb].h - 4;
    int rgb[3];
    vec4_t color;
    int k;

    if (y + h > smBottom - 20) {
        y = ST_RowY(st.rgb) - h + 4;
    }
    ST_ParseRGB(st.values[st.rgb], rgb);
    st.rgbPanel = SM_R(x, y, w, h);

    SM_Shadow(x, y, w, h, SM_RADIUS_MD);
    SM_Round(x, y, w, h, SM_RADIUS_MD, smRaised, NULL);
    SM_Hit(x, y, w, h, ST_HIT_RGBPANEL, 0, NULL);

    color[0] = rgb[0] / 255.0f;
    color[1] = rgb[1] / 255.0f;
    color[2] = rgb[2] / 255.0f;
    color[3] = 1.0f;
    SM_Round(x + 16, y + 16, w - 32, 44, SM_RADIUS_SM, color, NULL);

    for (k = 0; k < 3; k++) {
        float sy = y + 78 + k * 36;
        float tx = x + 76;
        float tw = w - 76 - 56;
        char text[8];

        SM_Label(x + 16, SM_Baseline(FONT_SANS, 11, sy, SM_SLIDER_H), 11, smMuted, names[k], 1.2f);
        SM_SliderTrack(tx, sy, tw, rgb[k] / 255.0f, SM_Over(ST_HIT_RGBSLIDER, k) || st.rgbDrag == k);
        SM_Hit(tx - 6, sy - 8, tw + 12, SM_SLIDER_H + 16, ST_HIT_RGBSLIDER, k, NULL);
        Com_sprintf(text, sizeof(text), "%i", rgb[k]);
        SM_Text(x + w - 16 - SM_Advance(FONT_SANS, 14, text, -1), SM_Baseline(FONT_SANS, 14, sy, SM_SLIDER_H),
                FONT_SANS, 14, smText, text, -1, qfalse);
    }
}

/*
===============
ST_GroupRect

Where a whole subsection lives: from its header down to the end of the last row belonging to it.
A full width header (column < 0) takes the rows of both columns; a single column one takes only
its own, because headers come in pairs on the same line and each rules its own side.
===============
*/
static qboolean ST_GroupRect(int head, smRect_t* out) {
    int col = stCvars[head].column;
    float bottom = stLayout[head].y + stLayout[head].h;
    int i;

    for (i = head + 1; i < ST_NUM_CVARS; i++) {
        float rowBottom;

        if (!ST_InTab(i)) {
            continue;
        }
        if (stCvars[i].widget == ST_HEADING) {
            // the paired header in the other column does not end this group
            if (col >= 0 && stCvars[i].column >= 0 && stLayout[i].y == stLayout[head].y) {
                continue;
            }
            break;
        }
        if (col >= 0 && (stCvars[i].column ? 1 : 0) != col) {
            continue;
        }
        rowBottom = stLayout[i].y + stLayout[i].h;
        if (rowBottom > bottom) {
            bottom = rowBottom;
        }
    }

    if (bottom <= stLayout[head].y + stLayout[head].h) {
        return qfalse;  // a header with no rows gets no card
    }
    *out = SM_R(stLayout[head].x, stLayout[head].y, stLayout[head].w,
                bottom - stLayout[head].y + ST_CARD_PAD);
    return qtrue;
}

/*
===============
ST_WeaponIcon

The icon of the weapon a "weapon N" row binds, or 0 when the row is not a weapon's. The names
are pak00's; the order is the keys' order, which is the weapon bar's.
===============
*/
static qhandle_t ST_WeaponIcon(int i) {
    static const char* const names[] = {
        NULL,                    // 0: there is no weapon 0
        "icons/iconw_gauntlet",  // 1
        "icons/iconw_machinegun",
        "icons/iconw_shotgun",
        "icons/iconw_grenade",
        "icons/iconw_rocket",
        "icons/iconw_lightning",
        "icons/iconw_railgun",
        "icons/iconw_plasma",
        "icons/iconw_bfg",       // 9
    };
    const char* cvar = stCvars[i].cvar;
    int n;

    if (stCvars[i].widget != ST_BIND || !cvar || Q_strncmp(cvar, "weapon ", 7)) {
        return 0;
    }
    n = atoi(cvar + 7);
    if (n < 1 || n >= (int)ARRAY_LEN(names) || !names[n]) {
        return 0;
    }
    if (!st.weaponIcon[n]) {
        st.weaponIcon[n] = trap_R_RegisterShaderNoMip(names[n]);
    }
    return st.weaponIcon[n];
}

/*
===============================================================================

THE PAGE

===============================================================================
*/

static void ST_DrawRow(int i, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float x = stLayout[i].x;
    float w = stLayout[i].w;
    float h = stLayout[i].h;
    float cx = ST_ControlX(i);
    float titleW = ST_TitleWidth(w);
    // [ioquakelive] a weapon is recognised faster by its icon than by its name, so a "weapon N"
    // row shifts its title along to make room for one
    qhandle_t weapIcon = ST_WeaponIcon(i);
    float titleX = x + (weapIcon ? 10 + 22 + 10 : 12);
    smRect_t row = SM_R(x, y, w, h);
    qboolean hovered;
    float cy;

    // the header is its card's title; the background was drawn before the rows
    if (cv->widget == ST_HEADING) {
        if (SM_TextInside(SM_Baseline(FONT_SANS, 13, y + 16, 20), 13, clip)) {
            SM_Label(x + ST_CARD_PAD + 12, SM_Baseline(FONT_SANS, 13, y + 16, 20), 13, smGold,
                     cv->title, 0.8f);
        }
        return;
    }

    // rows are told apart by the background under the cursor
    hovered = SM_In(&row, smMX, smMY) && st.select < 0 && st.rgb < 0 && st.model < 0 && st.drag < 0 &&
              st.previewDrag < 0;
    if (hovered || st.binding == i || st.edit == i) {
        SM_Round(x, y, w, h, SM_RADIUS_SM, smRaised, clip);
    }
    if (weapIcon) {
        SM_Pic(x + 10, y + (h - 22.0f) * 0.5f, 22.0f, 22.0f, weapIcon, clip);
    }

    if (ST_HasTip(i)) {
        const stTip_t* tip = ST_Tip(i);
        float tb = SM_Baseline(FONT_SANS, 15, y + 8, 24);
        int k;

        if (SM_TextInside(tb, 15, clip)) {
            SM_Text(titleX, tb, FONT_SANS, 15, smText, cv->title,
                    SM_Fit(FONT_SANS, 15, cv->title, titleW - (titleX - x - 12)), qfalse);
        }
        for (k = 0; k < tip->lines; k++) {
            tb = SM_Baseline(FONT_SANS, ST_TIP_SIZE, y + 32 + k * ST_TIP_LINE_H, ST_TIP_LINE_H);
            if (SM_TextInside(tb, ST_TIP_SIZE, clip)) {
                SM_Text(x + 12, tb, FONT_SANS, ST_TIP_SIZE, smMuted, cv->tip + tip->starts[k], tip->lens[k], qfalse);
            }
        }
    } else {
        float tb = SM_Baseline(FONT_SANS, 15, y, h);

        if (SM_TextInside(tb, 15, clip)) {
            SM_Text(titleX, tb, FONT_SANS, 15, smText, cv->title,
                    SM_Fit(FONT_SANS, 15, cv->title, titleW - (titleX - x - 12)), qfalse);
        }
    }
    SM_Hit(x, y, w, h, ST_HIT_ROW, i, clip);

    // controls are drawn only while wholly in view, so none spills past the panel
    cy = y + (h - ST_FIELD_H) * 0.5f;
    if (cv->widget != ST_BIND && (cy < clip->y || cy + ST_FIELD_H > clip->y + clip->h)) {
        return;
    }
    switch (cv->widget) {
        case ST_BOOL:
            ST_DrawBool(i, cx, cy, clip);
            break;
        case ST_SELECT:
            ST_DrawSelect(i, cx, cy, clip);
            break;
        case ST_RANGE:
            ST_DrawRange(i, cx, cy, clip);
            break;
        case ST_INPUT:
            ST_DrawInput(i, cx, cy, clip);
            break;
        case ST_PALETTE:
            ST_DrawPalette(i, cx, cy, clip);
            break;
        case ST_RGB:
            ST_DrawRGB(i, cx, cy, clip);
            break;
        case ST_MODEL:
        case ST_MODELSKIN:
            ST_DrawModelButton(i, cx, cy, clip);
            break;
        case ST_CROSSHAIR:
            ST_DrawCrosshair(i, cx, cy, clip);
            break;
        case ST_BIND:
            if (y >= clip->y && y + h <= clip->y + clip->h) {
                ST_DrawBind(i, x + w - 12, y, clip);
            }
            break;
        default:
            break;
    }
}

static void ST_DrawScrollbar(float x) {
    float thumbH = st.view * st.view / st.content;
    float thumbY;

    if (thumbH < 28) {
        thumbH = 28;
    }
    thumbY = st.listTop + (st.view - thumbH) * st.scroll[st.tab] / (st.content - st.view);
    SM_Hit(x - 4, st.listTop, 14, st.view, ST_HIT_TRACK, 0, NULL);
    SM_Pill(x, thumbY, 6, thumbH, (SM_Over(ST_HIT_THUMB, 0) || st.scrollDrag) ? smMuted : smBorder, NULL);
    SM_Hit(x - 4, thumbY, 14, thumbH, ST_HIT_THUMB, 0, NULL);
}

// the page's header: its title, the action its section ends with, and the sections
static void ST_DrawHeader(float y) {
    float left = UI_Home_Left();
    float right = left + HM_CONTAINER_W - ST_PAD;
    const char* action = ST_TabAction(st.tab);
    float x = left + ST_PAD;
    float pillY = y + ST_HEADER_H - 22 - SM_PILL_H;
    int i;

    SM_Round(left, y, HM_CONTAINER_W, ST_HEADER_H, SM_RADIUS_MD, smPanel, NULL);
    SM_Label(left + ST_PAD, SM_Baseline(FONT_SANS, 22, y + 16, 40), 22, smText, "Game Settings", 0.2f);

    if (st.fromIngame) {
        float bw = SM_ButtonWidth("Done", SM_BUTTON_H);

        SM_Button(right - bw, y + 18, bw, SM_BUTTON_H, "Done", SM_BUTTON_PRIMARY, SM_Over(ST_HIT_DONE, 0));
        SM_Hit(right - bw, y + 18, bw, SM_BUTTON_H, ST_HIT_DONE, 0, NULL);
        right -= bw + 10;
    }
    if (action) {
        float bw = SM_ButtonWidth(action, SM_BUTTON_H);

        SM_Button(right - bw, y + 18, bw, SM_BUTTON_H, action, SM_BUTTON_SECONDARY, SM_Over(ST_HIT_ACTION, 0));
        SM_Hit(right - bw, y + 18, bw, SM_BUTTON_H, ST_HIT_ACTION, 0, NULL);
    }

    for (i = 0; i < ST_NUM_TABS; i++) {
        float w;
        qboolean on = i == st.tab;
        qboolean over = SM_Over(ST_HIT_TAB, i);

        if (!ST_TabShown(i)) {
            continue;
        }
        w = SM_ButtonWidth(stTabNames[i], SM_PILL_H);
        SM_Button(x, pillY, w, SM_PILL_H, stTabNames[i], on ? SM_BUTTON_PRIMARY : SM_BUTTON_SECONDARY, over && !on);
        SM_Hit(x, pillY, w, SM_PILL_H, ST_HIT_TAB, i, NULL);
        x += w + 6;
    }
}

static void ST_DrawPage(void) {
    float headerY;
    float bottom;
    smRect_t clip;
    int i;

    // over a match the page lies on the match, darkened; otherwise it is the front end's
    if (st.fromIngame) {
        vec4_t tint = {smPage[0], smPage[1], smPage[2], 0.92f};

        SM_FillScreen(tint);
        headerY = 40;
    } else {
        UI_Home_Backdrop();
        UI_Home_PageTop(HM_NAV_SETTINGS);
        headerY = HM_NAV_H;
    }
    if (!ST_TabShown(st.tab)) {
        st.tab = ST_TAB_BASIC;
    }

    ST_DrawHeader(headerY);

    st.left = UI_Home_Left();
    st.top = headerY + ST_HEADER_H + HM_PANEL_GAP;
    bottom = smBottom - 40 - (UI_Home_InMatch() ? 38 : 0);
    st.panelH = bottom - st.top;
    SM_Round(st.left, st.top, HM_CONTAINER_W, st.panelH, SM_RADIUS_MD, smPanel, NULL);

    st.listTop = st.top + ST_PAD - 6;
    st.view = st.panelH - 2 * ST_PAD + 12;
    ST_Layout();
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab]);

    clip = SM_R(st.left, st.listTop, HM_CONTAINER_W, st.view);

    // [ioquakelive] the subsections no longer sit in a black card inside the panel. What
    // separates them is their gold title and a thin line above each one, except for the ones
    // at the very top, which have nothing to separate them from.
    if (st.tab != ST_TAB_TEAM) {
        float firstY = -1;

        for (i = 0; i < ST_NUM_CVARS; i++) {
            if (ST_InTab(i) && stCvars[i].widget == ST_HEADING && (firstY < 0 || stLayout[i].y < firstY)) {
                firstY = stLayout[i].y;
            }
        }
        for (i = 0; i < ST_NUM_CVARS; i++) {
            smRect_t card;
            float y;

            if (!ST_InTab(i) || stCvars[i].widget != ST_HEADING || !ST_GroupRect(i, &card)) {
                continue;
            }
            if (stLayout[i].y <= firstY) {
                continue;
            }
            y = ST_RowY(i);
            if (y < st.listTop || y >= st.listTop + st.view) {
                continue;
            }
            SM_Fill(card.x + 12, y, card.w - 24, 1, smBorder, &clip);
        }
    }

    for (i = 0; i < ST_NUM_CVARS; i++) {
        float y;

        if (!ST_InTab(i)) {
            continue;
        }
        y = ST_RowY(i);
        if (y + stLayout[i].h <= st.listTop || y >= st.listTop + st.view) {
            continue;
        }
        ST_DrawRow(i, y, &clip);
    }

    // the Team section's models, beside their blocks, drawn only while wholly in view
    if (st.tab == ST_TAB_TEAM) {
        int k;

        for (k = 0; k < 2; k++) {
            float y = st.listTop + stModelBox[k].y - st.scroll[st.tab];

            if (stModelBox[k].h <= 0 || y < st.listTop - 1 || y + stModelBox[k].h > st.listTop + st.view + 1) {
                continue;
            }
            ST_DrawTeamPreview(k, stModelBox[k].x, y, stModelBox[k].w, stModelBox[k].h);
        }
    }

    if (st.content > st.view) {
        ST_DrawScrollbar(st.left + HM_CONTAINER_W - 12);
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
    st.selectScroll = 0;
    for (k = 0; k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, st.values[i])) {
            // the chosen option shows, a couple of rows down if the list is long
            st.selectScroll = (k - 2) * ST_OPTION_H;
        }
    }
}

static void ST_DragRange(int i) {
    const stCvar_t* cv = &stCvars[i];
    float trackX = ST_ControlX(i) + 58;
    float trackW = ST_CONTROL_W - 58;
    float frac = (smMX - trackX - SM_SLIDER_H * 0.5f) / (trackW - SM_SLIDER_H);
    char text[32];

    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    ST_FormatStep(cv, ST_Snap(cv, cv->min + frac * (cv->max - cv->min)), text, sizeof(text));
    if (strcmp(text, st.values[i])) {
        ST_Set(i, text);
    }
}

static void ST_DragRGB(int channel) {
    float tx = st.rgbPanel.x + 76;
    float tw = st.rgbPanel.w - 76 - 56;
    float frac = (smMX - tx - SM_SLIDER_H * 0.5f) / (tw - SM_SLIDER_H);
    int rgb[3];

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
    int shape = atoi(st.values[i]) + delta;
    char text[16];

    if (shape < 0) {
        shape = NUM_CROSSHAIRS - 1;
    } else if (shape >= NUM_CROSSHAIRS) {
        shape = 0;
    }
    Com_sprintf(text, sizeof(text), "%i", shape);
    ST_Set(i, text);
    uiInfo.currentCrosshair = shape;
}

static void ST_ClosePopups(void) {
    st.select = -1;
    st.rgb = -1;
    st.edit = -1;
    st.binding = -1;
}

static void ST_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);
    int type = hit ? hit->type : ST_HIT_NONE;
    int index = hit ? hit->index : 0;

    // the model chooser takes the whole screen while it is up; a click off it is Cancel
    if (st.model >= 0) {
        switch (type) {
            case ST_HIT_MODELITEM:
                // a model starts on its default skin, as retail's changeModel has it; a forced
                // skin is the row's and stays
                if (st.pickSkinRow < 0) {
                    st.pickSkin = 0;
                }
                st.pickModel = index;
                break;
            case ST_HIT_SKINITEM:
                st.pickSkin = index;
                break;
            case ST_HIT_MODELOK:
                ST_ConfirmChooser();
                break;
            case ST_HIT_MODELCLEAR:
                ST_Set(st.model, "");
                st.model = -1;
                break;
            case ST_HIT_MODELPANE:
                break;
            default:
                st.model = -1;
                break;
        }
        return;
    }

    // an open list or picker takes the click, or closes
    if (st.select >= 0) {
        if (type == ST_HIT_OPTION) {
            if (stCvars[st.select].widget == ST_CROSSHAIR) {
                ST_Set(st.select, va("%i", index));  // in the grid the index IS the value
            } else {
                ST_Set(st.select, stCvars[st.select].options[index].value);
            }
            st.select = -1;
            return;
        }
        if (type == ST_HIT_MENU) {
            return;
        }
        if (type == ST_HIT_SELECT && index == st.select) {
            st.select = -1;
            return;
        }
        st.select = -1;
    }
    if (st.rgb >= 0) {
        if (type == ST_HIT_RGBSLIDER) {
            st.rgbDrag = index;
            ST_DragRGB(index);
            return;
        }
        if (type == ST_HIT_RGBPANEL) {
            return;
        }
        if (type == ST_HIT_RGB && index == st.rgb) {
            st.rgb = -1;
            return;
        }
        st.rgb = -1;
    }
    if (st.edit >= 0 && !(type == ST_HIT_INPUT && index == st.edit)) {
        ST_Set(st.edit, st.values[st.edit]);
        st.edit = -1;
    }
    if (st.binding >= 0) {
        st.binding = -1;
    }

    if (!st.fromIngame && UI_Home_NavClick()) {
        return;
    }
    if (st.fromIngame && UI_Home_NavClick()) {
        return;  // the banner
    }

    switch (type) {
        case ST_HIT_TAB:
            st.tab = index;
            ST_ClosePopups();
            break;
        case ST_HIT_ACTION:
            ST_DoTabAction(st.tab);
            break;
        case ST_HIT_DONE:
            UI_Settings_Close();
            break;
        case ST_HIT_SELECT:
            ST_OpenSelect(index);
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
            float h = 14.0f;
            float sw = (ST_CONTROL_W - h) / 26.0f;
            int k = (int)((smMX - (ST_ControlX(index) + h * 0.5f)) / sw) + 1;
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
            ST_OpenChooser(index);
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
        case ST_HIT_PREVIEW:
            st.previewDrag = index;
            st.previewGrab = smMX;
            break;
        case ST_HIT_THUMB:
            st.scrollDrag = qtrue;
            st.scrollGrab = smMY;
            break;
        default:
            break;
    }
}

static void ST_Wheel(float amount) {
    if (st.model >= 0) {
        return;
    }
    if (st.select >= 0) {
        st.selectScroll += amount > 0 ? 2 * ST_OPTION_H : -2 * ST_OPTION_H;
        return;
    }
    if (st.rgb >= 0) {
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
        case K_BACKSPACE:
            if (st.editCursor > 0) {
                memmove(buffer + st.editCursor - 1, buffer + st.editCursor, len - st.editCursor + 1);
                st.editCursor--;
            }
            return qtrue;
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
===============
UI_Settings_Open

Retail's Settings button gives the browser the whole screen. Opened over a match
the page is the "settings_ingame" menu, which is not fullscreen so the match is
still drawn under it, and closing it goes back to the match's menu.
===============
*/
void UI_Settings_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    if (!st.loaded) {
        st.tab = ST_TAB_BASIC;
        st.previewYaw[0] = 150;
        st.previewYaw[1] = 210;
        st.loaded = qtrue;
    }
    menu = Menus_FindByName("settings_ingame");
    st.fromIngame = menu && (menu->window.flags & WINDOW_VISIBLE);
    ST_ClosePopups();
    st.drag = -1;
    st.rgbDrag = -1;
    st.model = -1;
    st.previewDrag = -1;
    st.scrollDrag = qfalse;
    ST_ReadAll();

    // the page takes keys before the mouse moves over it
    if (!st.fromIngame) {
        menu = Menus_FindByName("settings");
    }
    item = menu ? Menu_FindItemByName(menu, "settings") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Settings_Paint(void) {
    qboolean held;

    SM_BeginFrame();
    SM_SetLook(SM_LOOK_OURS);  // the settings page keeps its own type, in retail's colours
    if (!st.loaded) {
        UI_Settings_Open();
    }

    // what follows the mouse while the button is held, and lets go with it
    held = trap_Key_IsDown(K_MOUSE1);
    if (st.drag >= 0) {
        if (held) {
            ST_DragRange(st.drag);
        } else {
            st.drag = -1;
        }
    }
    if (st.rgbDrag >= 0) {
        if (held && st.rgb >= 0) {
            ST_DragRGB(st.rgbDrag);
        } else {
            st.rgbDrag = -1;
        }
    }
    if (st.previewDrag >= 0) {
        if (held) {
            st.previewYaw[st.previewDrag] += (smMX - st.previewGrab) * 0.9f;
            st.previewGrab = smMX;
        } else {
            st.previewDrag = -1;
        }
    }
    if (st.scrollDrag) {
        if (!held) {
            st.scrollDrag = qfalse;
        } else if (st.content > st.view) {
            float thumbH = st.view * st.view / st.content;

            if (thumbH < 28) {
                thumbH = 28;
            }
            st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab] +
                                               (smMY - st.scrollGrab) * (st.content - st.view) / (st.view - thumbH));
            st.scrollGrab = smMY;
        }
    }
    if (st.edit < 0 && st.binding < 0 && st.drag < 0 && st.rgbDrag < 0) {
        ST_ReadAll();
    }

    ST_DrawPage();
    if (st.select >= 0) {
        if (stCvars[st.select].widget == ST_CROSSHAIR) {
            ST_DrawCrosshairGrid();
        } else {
            ST_DrawDropdown();
        }
    }
    if (st.rgb >= 0) {
        ST_DrawRGBPicker();
    }
    if (st.model >= 0) {
        ST_DrawModelChooser();
    }
    UI_Home_Banner();
    SM_EndFrame();
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
            ST_Wheel(-120);
            return qtrue;
        case K_MWHEELDOWN:
            ST_Wheel(120);
            return qtrue;
        case K_ENTER:
        case K_KP_ENTER:
            if (st.model >= 0) {
                ST_ConfirmChooser();
                return qtrue;
            }
            return qfalse;
        case K_ESCAPE:
            if (st.model >= 0) {
                st.model = -1;
                return qtrue;
            }
            if (st.rgb >= 0 || st.select >= 0) {
                st.rgb = -1;
                st.select = -1;
                return qtrue;
            }
            return qfalse;  // the menu's onESC closes the page
        default:
            return qfalse;
    }
}

// Escape, or Done: back to the match's menu or to the front end
void UI_Settings_Close(void) {
    qboolean fromIngame = st.fromIngame;

    Menus_CloseByName("settings");
    Menus_CloseByName("settings_ingame");
    st.fromIngame = qfalse;
    if (fromIngame) {
        Menus_ActivateByName("ingame");
        Menus_ActivateByName("ingame_about");
    } else {
        Menus_CloseAll();
        Menus_ActivateByName("main");
    }
}
