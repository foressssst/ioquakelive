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

#define ST_MAX_INPUT 200

#define ST_TEAM_GAP 12.0f
#define ST_TEAM_MODEL_W 360.0f

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
    ST_HEADING,
    ST_TEXT,
    ST_BREAK,
    ST_BOOL,
    ST_SELECT,
    ST_RANGE,
    ST_INPUT,
    ST_PALETTE,
    ST_RGB,
    ST_MODEL,
    ST_MODELSKIN,
    ST_CROSSHAIR,
    ST_BIND
} stWidget_t;

typedef struct {
    const char* label;
    const char* value;
} stOption_t;

typedef struct {
    int tab;
    const char* title;
    const char* cvar;
    int widget;
    float min, max, step;
    const stOption_t* options;
    int numOptions;
    const char* tip;
    int column;
} stCvar_t;

#include "ui_settings_table.h"

#define ST_NUM_CVARS ((int)ARRAY_LEN(stCvars))

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
    ST_HIT_PREVIEW,
    ST_HIT_RESET,
    ST_HIT_BLOCK,
    ST_HIT_RGBOK,
    ST_HIT_RGBCANCEL
} stHitType_t;

#define ST_MAX_SKINS 10

static struct {
    qboolean loaded;
    int tab;
    float scroll[ST_NUM_TABS];

    char values[ST_NUM_CVARS][MAX_CVAR_VALUE_STRING];

    int select;
    smRect_t selectControl;
    float selectScroll;
    int drag;
    int edit, editCursor, editStart, editTime;
    int binding;
    int rgb;
    int rgbDrag;
    int rgbPick[3];
    smRect_t rgbSlider[3];

    int model;
    int pickModel;
    int pickSkin;
    int pickSkinRow;
    playerInfo_t modelInfo;
    char modelShown[MAX_QPATH];
    char bodyShown[MAX_QPATH];
    qhandle_t body;
    int bodyW, bodyH;

    playerInfo_t preview[2];
    qhandle_t weaponIcon[10];
    char previewShown[2][MAX_QPATH];
    float previewYaw[2];
    int previewDrag;
    float previewGrab;

    qboolean scrollDrag;
    float scrollGrab;

    float left, top;
    float colW;
    float listTop, view, content;
    float listBottom;
    int selectFocus;
    qboolean fromIngame;
} st;

extern const vec4_t* UI_CrosshairPalette(void);

static const vec4_t strBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t strRowOdd = SM_RGB(36, 33, 33);
static const vec4_t strRowEven = SM_RGB(28, 25, 25);
static const vec4_t strBindHover = SM_RGB(60, 57, 57);
static const vec4_t strYellow = SM_RGB(247, 209, 0);
static const vec4_t strSelect = SM_RGB(51, 51, 51);
static const vec4_t strSelectHover = SM_RGB(68, 68, 68);
static const vec4_t strSelectValue = SM_RGB(204, 204, 204);
static const vec4_t strPlaceholder = SM_RGB(170, 170, 170);
static const vec4_t strOption = SM_RGB(187, 187, 187);
static const vec4_t strOptionFocus = SM_RGB(34, 34, 34);
static const vec4_t strInput = SM_RGB(15, 15, 15);
static const vec4_t strBorder = SM_RGB(51, 51, 51);
static const vec4_t strRange = SM_RGB(34, 34, 34);
static const vec4_t strHover = SM_RGB(68, 68, 68);
static const vec4_t strLink = SM_RGB(30, 174, 219);
static const vec4_t strLinkHover = SM_RGB(15, 160, 206);
static const vec4_t strOff = SM_RGB(153, 153, 153);
static const vec4_t strOn = SM_RGB(221, 221, 221);
static const vec4_t strSwatchEdge = {0.0f, 0.0f, 0.0f, 0.2f};
static const vec4_t strDialog = SM_RGB(30, 30, 30);
static const vec4_t strThumb = SM_RGB(51, 51, 51);
static const vec4_t strThumbHover = SM_RGB(153, 153, 153);
static const vec4_t strMenuThumb = SM_RGB(17, 17, 17);

#define STR_TITLE_H SM_TITLE_H
#define STR_NAV_H SM_Len(30.0f)
#define STR_NAV_PAD SM_Len(20.0f)
#define STR_NAV_GAP SM_Len(15.0f)
#define STR_SECTION_PAD SM_Len(5.0f)
#define STR_ROW_PAD SM_Len(7.5f)
#define STR_TEXT_LINE SM_LineHeight(15.0f, 1.6f)
#define STR_BAR 8.0f
#define STR_OPTION_H 40.0f
#define STR_MENU_H 198.0f

static void ST_ReadBind(int i) {
    const char* command = stCvars[i].cvar;
    int count = 0;
    int k;

    st.values[i][0] = '\0';
    for (k = 0; k < K_LAST_KEY; k++) {
        char buf[MAX_STRING_CHARS];
        char name[32];

        trap_Key_GetBindingBuf(k, buf, sizeof(buf));
        if (!buf[0] || Q_stricmp(buf, command)) {
            continue;
        }
        trap_Key_KeynumToStringBuf(k, name, sizeof(name));
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

static int ST_StepDecimals(const stCvar_t* cv) {
    double scaled = cv->step;
    int decimals;

    if (cv->step <= 0) {
        return 0;
    }
    for (decimals = 0; decimals < 4; decimals++) {
        if (fabs(scaled - floor(scaled + 0.5)) < 0.0001) {
            break;
        }
        scaled *= 10.0;
    }
    return decimals;
}

static void ST_FormatShown(const stCvar_t* cv, float value, char* out, int size) {
    Com_sprintf(out, size, "%.*f", ST_StepDecimals(cv), value);
}

static void ST_FormatStep(const stCvar_t* cv, float value, char* out, int size) {
    int len;

    ST_FormatShown(cv, value, out, size);
    if (!strchr(out, '.')) {
        return;
    }
    len = (int)strlen(out);
    while (len > 0 && out[len - 1] == '0') {
        out[--len] = '\0';
    }
    if (len > 0 && out[len - 1] == '.') {
        out[--len] = '\0';
    }
    if (!strcmp(out, "-0")) {
        Q_strncpyz(out, "0", size);
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

static int ST_ModelTint(const char* cvarName) {
    char buf[64];
    int rgb[3];

    trap_Cvar_VariableStringBuffer(cvarName, buf, sizeof(buf));
    ST_ParseRGB(buf, rgb);
    return (rgb[0] << 24) | (rgb[1] << 16) | (rgb[2] << 8) | 0xff;
}

static qboolean ST_TabShown(int tab) {
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

static struct {
    float x, y, w, h;
    qboolean odd;
} stLayout[ST_NUM_CVARS];

static struct {
    float x, y, w, h;
} stModelBox[2];

static qboolean ST_InTab(int i) {
    return stCvars[i].tab == st.tab;
}

static float ST_InputHeight(void) {
    return 2 * SM_Len(8.0f) + 2 * SM_LenBorder(1.0f) + SM_NormalLine(FONT_SANS, 15);
}

static float ST_SelectHeight(void) {
    return 2 * SM_Len(6.0f) + STR_TEXT_LINE;
}

static float ST_ContentHeight(int widget) {
    switch (widget) {
        case ST_BIND:
            return STR_TEXT_LINE;
        case ST_INPUT:
            return ST_InputHeight();
        case ST_SELECT:
            return ST_SelectHeight();
        case ST_BOOL:
        case ST_PALETTE:
            return SM_Len(36.0f);
        case ST_MODEL:
        case ST_MODELSKIN:
            return SM_Len(48.0f);
        case ST_RGB:
            return SM_Len(3.0f) + SM_Len(38.0f);
        case ST_CROSSHAIR:
            return SM_Len(64.0f);
        default:
            return 0;
    }
}

static int ST_BindKeys(int i, char* keys, int size) {
    const char* p = st.values[i];
    int n = 0;

    keys[0] = '\0';
    while (*p) {
        const char* end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);

        Q_strcat(keys, size, n++ ? ", " : "");
        if ((int)strlen(keys) + len < size) {
            strncat(keys, p, len);
        }
        p += len;
        if (*p == '\n') {
            p++;
        }
    }
    if (!n) {
        Q_strncpyz(keys, "None", size);
    }
    return n;
}

static int ST_WordSpaced(float x, float base, const char* text, float width, const smRect_t* clip, qboolean draw) {
    float left = x, right = x + width, gap = SM_Advance(FONT_SANS, 15, " ", 1) + SM_LetterSpacing(5.0f);
    int lines = 1;

    while (*text) {
        const char* end = strchr(text, ' ');
        int len = end ? (int)(end - text) : (int)strlen(text);
        float advance = SM_Advance(FONT_SANS, 15, text, len);

        if (x > left && x + advance > right) {
            x = left;
            base += STR_TEXT_LINE;
            lines++;
        }
        if (draw && SM_TextInside(base, 15, clip)) {
            if (x + advance > right) {
                char word[MAX_CVAR_VALUE_STRING];

                Q_strncpyz(word, text, MIN(len + 1, (int)sizeof(word)));
                SM_Text(x, base, FONT_SANS, 15, smText, word, SM_Fit(FONT_SANS, 15, word, right - x), qfalse);
            } else {
                SM_Text(x, base, FONT_SANS, 15, smText, text, len, qfalse);
            }
        }
        x += advance + gap;
        text += len;
        while (*text == ' ') {
            text++;
        }
    }
    return lines;
}

static float ST_BindWidth(int i) {
    return SM_Percent(stLayout[i].w - 2 * STR_ROW_PAD, 40.0f);
}

static float ST_RowHeight(int i) {
    int widget = stCvars[i].widget;
    float minH = SM_Len(widget == ST_BIND ? 32.0f : 50.0f);
    float h = 2 * STR_ROW_PAD + ST_ContentHeight(widget);

    if (widget == ST_BIND) {
        char keys[128];

        ST_BindKeys(i, keys, sizeof(keys));
        h += (ST_WordSpaced(0, 0, keys, ST_BindWidth(i), NULL, qfalse) - 1) * STR_TEXT_LINE;
    }
    return h > minH ? h : minH;
}

static float ST_TextHeight(int i, float w) {
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];

    if (stCvars[i].widget == ST_BREAK) {
        return STR_TEXT_LINE;
    }
    return SM_Wrap(FONT_SANS, 15, stCvars[i].title, w, starts, lens) * STR_TEXT_LINE;
}

static float ST_HeadingHeight(qboolean first) {
    return SM_Len(first ? 8.0f : 24.0f) + SM_H1_LINE + SM_H1_PAD;
}

static void ST_LayoutTeam(float inner) {
    float rowsW = inner - ST_TEAM_MODEL_W - 24;
    float x = st.left + STR_SECTION_PAD;
    float y = 0;
    int block, i;

    for (block = 0; block < 2; block++) {
        float blockTop;
        int n = 0;

        if (block) {
            y += ST_TEAM_GAP;
        }
        for (i = 0; i < ST_NUM_CVARS; i++) {
            if (ST_InTab(i) && stCvars[i].widget == ST_HEADING && stCvars[i].column == block) {
                stLayout[i].x = x;
                stLayout[i].y = y;
                stLayout[i].w = rowsW;
                stLayout[i].h = ST_HeadingHeight(y == 0);
                y += stLayout[i].h;
            }
        }
        blockTop = y;
        for (i = 0; i < ST_NUM_CVARS; i++) {
            if (!ST_InTab(i) || stCvars[i].widget == ST_HEADING || stCvars[i].column != block) {
                continue;
            }
            stLayout[i].x = x;
            stLayout[i].y = y;
            stLayout[i].w = rowsW;
            stLayout[i].h = ST_RowHeight(i);
            stLayout[i].odd = !(n++ & 1);
            y += stLayout[i].h;
        }
        stModelBox[block].x = x + rowsW + 24;
        stModelBox[block].y = blockTop;
        stModelBox[block].w = ST_TEAM_MODEL_W;
        stModelBox[block].h = y - blockTop;
    }
    st.listBottom = y;
}

static void ST_Layout(void) {
    float inner = HM_CONTAINER_W - SM_Len(STR_BAR) - 2 * STR_SECTION_PAD;
    float x = st.left + STR_SECTION_PAD;
    float gap = SM_Percent(inner, 4.0f);
    float y[2];
    int n[2];
    float pairTop = -1, pairH = 0;
    int i;

    st.colW = SM_Percent(inner, 48.0f);
    if (st.tab == ST_TAB_TEAM) {
        ST_LayoutTeam(inner);
        return;
    }

    y[0] = y[1] = 0;
    n[0] = n[1] = 0;
    for (i = 0; i < ST_NUM_CVARS; i++) {
        int c;

        if (!ST_InTab(i)) {
            continue;
        }
        if (stCvars[i].widget == ST_BREAK && stCvars[i].column < 0) {
            float top = y[0] > y[1] ? y[0] : y[1];

            stLayout[i].x = x;
            stLayout[i].y = top;
            stLayout[i].w = inner;
            stLayout[i].h = STR_TEXT_LINE;
            y[0] = y[1] = top + STR_TEXT_LINE;
            n[0] = n[1] = 0;
            pairTop = -1;
            continue;
        }
        if (stCvars[i].widget != ST_HEADING) {
            pairTop = -1;
        } else {
            float top = y[0] > y[1] ? y[0] : y[1];
            int col = stCvars[i].column;
            float h = ST_HeadingHeight(top == 0);

            if (col >= 0 && pairTop >= 0) {
                top = pairTop;
                h = pairH;
            }
            pairTop = col >= 0 ? top : -1;
            pairH = h;
            stLayout[i].x = x + (col > 0 ? st.colW + gap : 0);
            stLayout[i].y = top;
            stLayout[i].w = col < 0 ? inner : st.colW;
            stLayout[i].h = h;
            y[0] = y[1] = top + h;
            n[0] = n[1] = col >= 0 ? 1 : 0;
            continue;
        }
        c = stCvars[i].column ? 1 : 0;
        stLayout[i].x = x + c * (st.colW + gap);
        stLayout[i].y = y[c];
        stLayout[i].w = st.colW;
        if (stCvars[i].widget == ST_TEXT || stCvars[i].widget == ST_BREAK) {
            stLayout[i].h = ST_TextHeight(i, st.colW);
            n[c]++;
        } else {
            stLayout[i].h = ST_RowHeight(i);
            stLayout[i].odd = !(n[c]++ & 1);
        }
        y[c] += stLayout[i].h;
    }
    st.listBottom = y[0] > y[1] ? y[0] : y[1];
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

static float ST_ControlX(int i) {
    float inner = stLayout[i].w - 2 * STR_ROW_PAD;

    return stLayout[i].x + STR_ROW_PAD + SM_Percent(inner, 56.6666666667f) + SM_Percent(inner, 4.0f);
}

static float ST_ControlW(int i) {
    return SM_Percent(stLayout[i].w - 2 * STR_ROW_PAD, 39.3333333333f);
}

static void ST_SelectArrow(float right, float y, qboolean up, const smRect_t* clip) {
    int n = SM_Px(SM_LenBorder(5.0f));
    float px = 1.0f / smScale;
    float x = right - SM_Len(10.0f) - 2 * n * px;
    int k;

    for (k = 0; k < n; k++) {
        int inset = up ? n - 1 - k : k;

        SM_Fill(x + (inset + 1) * px, y + k * px, (2 * n - 2 * inset - 1) * px, px, smMuted, clip);
    }
}

static void ST_DrawSelectBox(int i, float x, float y, float w, float h, const smRect_t* clip) {
    qboolean open = st.select == i;

    SM_Fill(x, y, w, h, SM_Over(ST_HIT_SELECT, i) || open ? strSelectHover : strSelect, clip);
    ST_SelectArrow(x + w, y + SM_Len(18.0f), open, clip);
    SM_Hit(x, y, w, h, ST_HIT_SELECT, i, clip);
    if (open) {
        st.selectControl = SM_R(x, y, w, h);
    }
}

static void ST_DrawSelect(int i, float x, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    const char* label = "Select...";
    qboolean chosen = qfalse;
    float w = ST_ControlW(i);
    float base = SM_Baseline(FONT_SANS, 15, y + SM_Len(8.0f), STR_TEXT_LINE);
    int k;

    for (k = 0; k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, st.values[i])) {
            label = cv->options[k].label;
            chosen = qtrue;
        }
    }
    ST_DrawSelectBox(i, x, y, w, ST_SelectHeight(), clip);
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + SM_Len(10.0f), base, FONT_SANS, 15, chosen ? strSelectValue : strPlaceholder, label,
                SM_Fit(FONT_SANS, 15, label, w + SM_Len(15.0f) - SM_Len(10.0f) - SM_Len(52.0f)), qfalse);
    }
}

static void ST_DrawBool(int i, float x, float y, const smRect_t* clip) {
    qboolean on = atoi(st.values[i]) != 0;
    float pad = SM_Len(8.0f), h = SM_Len(36.0f);
    float base = y + SM_Len(9.0f) + SM_Ascent(FONT_SANS, 15);
    float tw = SM_AdvanceBold(FONT_SANS, 15, on ? "On" : "Off", -1, on);

    if (SM_Over(ST_HIT_BOOL, i)) {
        SM_Fill(x, y, pad + SM_Len(13.0f), h, strHover, clip);
    }
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + pad, base, FONT_SANS, 15, on ? strOn : strOff, on ? "On" : "Off", -1, on);
    }
    SM_Hit(x, y, pad + tw, h, ST_HIT_BOOL, i, clip);
}

static void ST_DrawRange(int i, float x, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float w = ST_ControlW(i);
    float border = SM_LenBorder(1.0f), thumb = SM_Len(16.0f);
    float bx = x + SM_Percent(w, 20.0f), bw = SM_Percent(w, 80.0f), bh = thumb + 2 * border;
    float value = ST_Snap(cv, (float)atof(st.values[i]));
    float base = SM_Baseline(FONT_SANS, 15, y + SM_Len(1.0f), SM_Len(16.0f));
    char text[32];

    ST_FormatShown(cv, (float)atof(st.values[i]), text, sizeof(text));
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + SM_Len(8.0f), base, FONT_SANS, 15, smText, text, -1, qfalse);
    }
    SM_Fill(bx, y, bw, bh, SM_Over(ST_HIT_RANGE, i) || st.drag == i ? strHover : strRange, clip);
    SM_Border(bx, y, bw, bh, 1, strBorder, clip);
    SM_Fill(bx + border + (float)(int)(ST_Fraction(cv, value) * SM_Px(bw - 2 * border - thumb)) / smScale,
            y + border, thumb, thumb, strYellow, clip);
    SM_Hit(bx, y, bw, bh, ST_HIT_RANGE, i, clip);
}

static void ST_DrawInput(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i];
    float w = ST_ControlW(i), h = ST_InputHeight();
    float border = SM_LenBorder(1.0f), pad = SM_Len(8.0f);
    float inner = 1.0f / smScale;
    float tx = x + border + pad + inner, ty = y + border + pad;
    float width = w - 2 * border - pad - 2 * inner;
    float base = ty + SM_Ascent(FONT_SANS, 15);
    int len = (int)strlen(value);
    int start = 0;
    qboolean editing = st.edit == i;

    SM_Fill(x, y, w, h, strInput, clip);
    SM_Border(x, y, w, h, 1, strBorder, clip);
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
    if (SM_TextInside(base, 15, clip)) {
        SM_TextRaw(tx, base, FONT_SANS, 15, smText, value + start, SM_Fit(FONT_SANS, 15, value + start, width),
                   qfalse);
        if (editing && ((uiInfo.uiDC.realTime - st.editTime) / 500) % 2 == 0) {
            SM_Fill(tx + SM_AdvanceRaw(FONT_SANS, 15, value + start, st.editCursor - start, qfalse), ty,
                    1.0f / smScale, SM_NormalLine(FONT_SANS, 15), smText, clip);
        }
    }
    SM_Hit(x, y, w, h, ST_HIT_INPUT, i, clip);
}

static void ST_DrawSwatch(float x, float y, float w, int value, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();

    if (value < 1 || value > 26) {
        return;
    }
    SM_Fill(x, y, w, SM_Len(21.0f), palette[value - 1], clip);
    SM_Border(x, y, w, SM_Len(21.0f), SM_LenBorder(3.0f), strSwatchEdge, clip);
}

static float ST_LineRoom(float w) {
    return w + SM_Len(15.0f) - SM_Len(10.0f) - SM_Len(52.0f);
}

static void ST_DrawPalette(int i, float x, float y, const smRect_t* clip) {
    float w = ST_ControlW(i);
    float sw = w - SM_Len(10.0f) - SM_Len(52.0f);

    ST_DrawSelectBox(i, x, y, w, SM_Len(36.0f), clip);
    ST_DrawSwatch(x + SM_Len(10.0f) + SM_CenterLine(ST_LineRoom(w), sw), y + SM_Len(8.0f), sw, atoi(st.values[i]),
                  clip);
}

static void ST_DrawRGB(int i, float x, float y, const smRect_t* clip) {
    int rgb[3];
    vec4_t color;
    float w = ST_ControlW(i), top = y + SM_Len(3.0f), h = SM_Len(38.0f);

    ST_ParseRGB(st.values[i], rgb);
    color[0] = rgb[0] / 255.0f;
    color[1] = rgb[1] / 255.0f;
    color[2] = rgb[2] / 255.0f;
    color[3] = 1.0f;
    SM_Fill(x, top, w, h, color, clip);
    SM_Border(x, top, w, h, 5, strSwatchEdge, clip);
    SM_Hit(x, top, w, h, ST_HIT_RGB, i, clip);
}

static void ST_DrawCrosshair(int i, float x, float y, const smRect_t* clip) {
    int shape = atoi(st.values[i]);
    float w = ST_ControlW(i), size = SM_Len(64.0f);
    float base = SM_Baseline(FONT_SANS, 15, y + SM_Len(8.0f), STR_TEXT_LINE);

    ST_DrawSelectBox(i, x, y, w, size, clip);
    if (shape > 0 && shape < NUM_CROSSHAIRS) {
        SM_Pic(x + SM_Len(10.0f) + SM_CenterLine(ST_LineRoom(w), size), y, size, size,
               uiInfo.uiDC.Assets.crosshairShader[shape], clip);
    } else if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + SM_Len(10.0f), base, FONT_SANS, 15, strPlaceholder, "None", -1, qfalse);
    }
}

static void ST_DrawBind(int i, float y, const smRect_t* clip) {
    float x = stLayout[i].x, w = stLayout[i].w, h = stLayout[i].h;
    float base = SM_Baseline(FONT_SANS, 15, y + STR_ROW_PAD, STR_TEXT_LINE);
    float nameW = SM_Percent(w - 2 * STR_ROW_PAD, 60.0f);
    float nameBase;
    char keys[128];
    int lines;

    ST_BindKeys(i, keys, sizeof(keys));
    SM_Fill(x, y, w, h, SM_Over(ST_HIT_BIND, i) ? strBindHover : stLayout[i].odd ? strRowOdd : strRowEven, clip);
    lines = ST_WordSpaced(x + STR_ROW_PAD + nameW, base, keys, ST_BindWidth(i), clip, qtrue);
    nameBase = base + (lines - 1) * STR_TEXT_LINE;
    if (SM_TextInside(nameBase, 15, clip)) {
        SM_Text(x + STR_ROW_PAD, nameBase, FONT_SANS, 15, smText, stCvars[i].title,
                SM_Fit(FONT_SANS, 15, stCvars[i].title, nameW), qfalse);
    }
    SM_Hit(x, y, w, h, ST_HIT_BIND, i, clip);
}

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
#define ST_DEFAULT_MODEL 19

static qhandle_t stIcons[ST_NUM_MODELS][ST_MAX_SKINS];

static qhandle_t ST_IconFor(int model, const char* skin) {
    int k;

    for (k = 0; k < ST_MAX_SKINS && stModels[model].skins[k]; k++) {
        if (!Q_stricmp(stModels[model].skins[k], skin)) {
            qhandle_t* h = &stIcons[model][k];

            if (!*h) {
                *h = trap_R_RegisterShaderNoMip(va("webui/img/models/icon/%s_%s.png", stModels[model].model, skin));
                if (!*h) {
                    *h = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_%s", stModels[model].model, skin));
                }
                if (!*h) {
                    *h = -1;
                }
            }
            return *h > 0 ? *h : 0;
        }
    }
    return 0;
}

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
    }
    st.model = -1;
}

static void ST_DrawModelButton(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i][0] ? st.values[i] : "None";
    qboolean narrow = SM_Narrow();
    qboolean over = SM_Over(ST_HIT_MODEL, i) || st.model == i;
    float w = ST_ControlW(i), h = SM_Len(48.0f);
    float pad = SM_Len(narrow ? 1.1f : 5.5f);
    float tx = x + SM_Len(10.0f), base;
    int lead = SM_Px(SM_Len(38.0f));
    int ascent = SM_Px(SM_Ascent(FONT_SANS, 11)), descent = SM_Px(SM_Descent(FONT_SANS, 11));
    int above = (lead - ascent - descent) / 2 + ascent, below = lead - above;
    int iconH = 0, iconAbove = 0;
    qhandle_t icon = 0;
    char upper[64];

    if (st.values[i][0]) {
        char skin[MAX_QPATH];
        int model = ST_ParseModel(st.values[i], skin, sizeof(skin));

        icon = ST_IconFor(model, skin[0] ? skin : "default");
    }
    if (icon) {
        int box;

        iconH = SM_Px(SM_Len(narrow ? 24.0f : 48.0f));
        box = iconH + (narrow ? SM_Px(SM_Len(4.0f)) : 0);
        iconAbove = box / 2 + (int)floorf(SM_Px(11.0f) * 0.535f + 0.5f) / 2;
        above = MAX(above, iconAbove);
        below = MAX(below, box - iconAbove);
    }
    base = y + ((SM_Px(h) - above - below) / 2 + above) / smScale;

    SM_Fill(x, y, w, h, over ? smWebButtonHover : smWebButton, clip);
    if (icon) {
        SM_Pic(tx, base - iconAbove / smScale, iconH / smScale, iconH / smScale, icon, clip);
        tx += iconH / smScale;
    } else {
        tx += pad;
    }
    tx += 2 * pad + SM_AdvanceBold(FONT_SANS, 11, " ", 1, qtrue) + SM_LetterSpacing(1.0f);
    Q_strncpyz(upper, value, sizeof(upper));
    Q_strupr(upper);
    if (SM_TextInside(base, 11, clip)) {
        SM_SpacedText(tx, base, FONT_SANS, 11, over ? smWebYellow : smText, upper, 1.0f, qtrue);
    }
    SM_Hit(x, y, w, h, ST_HIT_MODEL, i, clip);
}

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

static smRect_t ST_DrawDialog(float height, float side, int panel) {
    float h = SM_Len(height);
    float top = SM_Percent(smBottom, 50.0f) - SM_Len(height * 0.5f);
    float x = SM_Percent(smVW, side);
    float w = (float)(SM_Px(smVW) - 2 * SM_Px(x)) / smScale;
    float pad = SM_Len(15.0f);

    SM_Fill(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, strBlack80, NULL);
    SM_Hit(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, ST_HIT_BLOCK, 0, NULL);
    SM_Fill(0, top, smVW, h, strDialog, NULL);
    SM_Fill(x, top, w, h, strBlack80, NULL);
    SM_Hit(0, top, smVW, h, panel, 0, NULL);
    return SM_R(x + pad, top + pad, w - 2 * pad, h - 2 * pad);
}

static void ST_DrawActions(const smRect_t* c, int count, const char* const* labels, const int* hits) {
    float x = c->x + c->w;
    float y = c->y + c->h - SM_WEB_BUTTON_H;
    float space = SM_Advance(FONT_SANS, 15, " ", 1);
    int k;

    for (k = count - 1; k >= 0; k--) {
        float bw = SM_WebButtonWidth(labels[k]);

        x -= bw;
        SM_WebButton(x, y, bw, labels[k], qfalse, SM_Over(hits[k], 0), NULL);
        SM_Hit(x, y, bw, SM_WEB_BUTTON_H, hits[k], 0, NULL);
        x -= space;
    }
}

static void ST_DialogTitle(float x, float y, float w, const char* title, qboolean center) {
    float indent = SM_Len(6.4f);

    SM_SetSpacing(SM_SPACING_H1);
    if (center) {
        x += SM_Center(w - indent, SM_Advance(FONT_DEFAULT, 32, title, -1));
    }
    SM_Text(x + indent, SM_Baseline(FONT_DEFAULT, 32, y + SM_H1_PAD, SM_H1_LINE), FONT_DEFAULT, 32, smText, title, -1,
            qfalse);
    SM_SetSpacing(0);
}

static qhandle_t ST_BodyFor(const char* model, const char* skin) {
    char path[MAX_QPATH];
    char head[16];

    Com_sprintf(path, sizeof(path), "webui/img/models/body/%s_%s.png", model, skin);
    if (Q_stricmp(path, st.bodyShown)) {
        Q_strncpyz(st.bodyShown, path, sizeof(st.bodyShown));
        st.body = 0;
        st.bodyW = st.bodyH = 0;
        if (trap_WebPakExcerpt(path, "IHDR", head, sizeof(head)) >= 12) {
            const byte* b = (const byte*)head + 4;

            st.bodyW = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
            st.bodyH = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
            if (st.bodyW > 0 && st.bodyH > 0) {
                st.body = trap_R_RegisterShaderNoMip(path);
            }
        }
    }
    return st.body;
}

static float ST_DrawIcons(float x, float y, float w, int count, int selected, qboolean center, int model, int hit) {
    float icon = SM_Len(62.0f);
    float line = icon + SM_StrutBelow(15, STR_TEXT_LINE);
    float outline = SM_LenBorder(2.0f);
    float selX = 0, selY = 0;
    int perRow = SM_Px(w) / SM_Px(icon);
    int k;

    if (perRow < 1) {
        perRow = 1;
    }
    for (k = 0; k < count; k++) {
        int inRow = MIN(perRow, count - k / perRow * perRow);
        float rowX = center ? x + SM_Center(w, inRow * icon) : x;
        float cx = rowX + (k % perRow) * icon;
        float cy = y + (k / perRow) * line;
        qhandle_t pic;

        if (model < 0) {
            pic = ST_IconFor(k, "default");
        } else {
            const char* value = ST_PickSkinValue(k);

            pic = ST_IconFor(model, value[0] ? value : "default");
        }
        if (pic) {
            SM_Pic(cx, cy, icon, icon, pic, NULL);
        }
        if (k == selected) {
            selX = cx;
            selY = cy;
        }
        SM_Hit(cx, cy, icon, icon, hit, k, NULL);
    }
    if (selected >= 0 && selected < count) {
        SM_Border(selX - outline, selY - outline, icon + 2 * outline, icon + 2 * outline, outline, strYellow, NULL);
    }
    return count > 0 ? ((count + perRow - 1) / perRow) * line : 0;
}

static void ST_DrawModelChooser(void) {
    smRect_t c = ST_DrawDialog(600, 15, ST_HIT_MODELPANE);
    float listW = SM_Percent(c.w, 30.6666667f);
    float paneX = c.x + listW + SM_Percent(c.w, 4.0f);
    float paneW = SM_Percent(c.w, 65.3333333f);
    qboolean skins = stCvars[st.model].widget == ST_MODELSKIN;
    qboolean clear = stCvars[st.model].widget == ST_MODEL;
    const stModel_t* picked = &stModels[st.pickModel];
    float y = c.y + SM_TITLE_H;
    float bodyH = SM_Len(425.0f);
    const char* skin = "default";
    qhandle_t body;

    ST_DrawIcons(c.x, c.y, listW, ST_NUM_MODELS, st.pickModel, qtrue, -1, ST_HIT_MODELITEM);
    ST_DialogTitle(paneX, c.y, paneW, picked->name, qfalse);
    if (skins) {
        y += ST_DrawIcons(paneX, y, paneW, ST_PickSkinCount(), st.pickSkin, qfalse, st.pickModel, ST_HIT_SKINITEM);
        if (ST_PickSkinValue(st.pickSkin)[0]) {
            skin = ST_PickSkinValue(st.pickSkin);
        }
    }
    body = ST_BodyFor(picked->model, skin);
    if (body) {
        int hPx = SM_Px(bodyH);
        int wPx = (int)floorf((float)st.bodyW * hPx / st.bodyH + 0.5f);

        SM_Pic(paneX, y, wPx / smScale, hPx / smScale, body, NULL);
    } else {
        ST_DrawChooserPreview(paneX, y, paneW, bodyH);
    }

    {
        static const char* const labels[3] = {"Clear", "Cancel", "OK"};
        static const int hits[3] = {ST_HIT_MODELCLEAR, ST_HIT_MODELCANCEL, ST_HIT_MODELOK};

        ST_DrawActions(&c, clear ? 3 : 2, clear ? labels : labels + 1, clear ? hits : hits + 1);
    }
}

static int ST_OptionCount(int i);

static float ST_MenuLayout(int i, float w, float* cellW, float* cellH, int* perRow, float* view) {
    int widget = stCvars[i].widget, count = ST_OptionCount(i);
    float most = widget == ST_SELECT ? STR_MENU_H : SM_Len(300.0f);
    float strut = SM_StrutBelow(15, STR_TEXT_LINE);
    float full = 0;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        float room = pass ? w - SM_Len(STR_BAR) : w;

        if (widget == ST_PALETTE) {
            *cellW = SM_Len(32.0f);
            *cellH = 2 * SM_Len(4.0f) + SM_Len(21.0f) + strut;
        } else if (widget == ST_CROSSHAIR) {
            *cellW = 2 * SM_Len(10.0f) + SM_Len(64.0f);
            *cellH = SM_Len(8.0f) + SM_Len(64.0f) + strut + SM_Len(8.0f);
        } else {
            *cellW = room;
            *cellH = STR_OPTION_H;
        }
        *perRow = MAX(1, SM_Px(room) / MAX(1, SM_Px(*cellW)));
        full = ((count + *perRow - 1) / *perRow) * *cellH;
        if (full <= most) {
            break;
        }
    }
    *view = MIN(full, most);
    return full;
}

static int ST_OptionCount(int i) {
    switch (stCvars[i].widget) {
        case ST_PALETTE:
            return 26;
        case ST_CROSSHAIR:
            return NUM_CROSSHAIRS;
        default:
            return stCvars[i].numOptions;
    }
}

static void ST_DrawDropdown(void) {
    const stCvar_t* cv = &stCvars[st.select];
    float x = st.selectControl.x, y = st.selectControl.y + st.selectControl.h - SM_LenBorder(1.0f);
    float w = st.selectControl.w;
    float cellW, cellH, view;
    int perRow;
    float full = ST_MenuLayout(st.select, w, &cellW, &cellH, &perRow, &view);
    float room = full > view ? w - SM_Len(STR_BAR) : w;
    float pad = SM_Len(4.0f);
    int count = ST_OptionCount(st.select);
    smRect_t box;
    int k;

    if (y + view > smBottom - 30) {
        y = st.selectControl.y - view + SM_LenBorder(1.0f);
    }
    if (st.selectScroll > full - view) {
        st.selectScroll = full - view;
    }
    if (st.selectScroll < 0) {
        st.selectScroll = 0;
    }
    box = SM_R(x, y, w, view);
    SM_Fill(x, y, w, view, strSelect, NULL);
    SM_Hit(x, y, w, view, ST_HIT_MENU, st.select, NULL);
    for (k = 0; k < count; k++) {
        int row = k / perRow, inRow = MIN(perRow, count - row * perRow);
        float ox = x + (k % perRow) * cellW + (cv->widget == ST_SELECT ? 0 : SM_Center(room, inRow * cellW));
        float oy = y + row * cellH - st.selectScroll;

        if (oy + cellH < y || oy > y + view) {
            continue;
        }
        if (SM_Over(ST_HIT_OPTION, k)) {
            st.selectFocus = k;
        }
        if (k == st.selectFocus) {
            SM_Fill(ox, oy, cellW, cellH, strOptionFocus, &box);
        }
        if (cv->widget == ST_PALETTE) {
            ST_DrawSwatch(ox + pad, oy + pad, cellW - 2 * pad, k + 1, &box);
        } else if (cv->widget == ST_CROSSHAIR) {
            if (k > 0) {
                SM_Pic(ox + SM_Len(10.0f), oy, SM_Len(64.0f), SM_Len(64.0f), uiInfo.uiDC.Assets.crosshairShader[k],
                       &box);
            }
        } else if (SM_TextInside(SM_Baseline(FONT_SANS, 15, oy + 8, 24), 15, &box)) {
            SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, oy + 8, 24), FONT_SANS, 15,
                    k == st.selectFocus ? strSelectValue : strOption, cv->options[k].label,
                    SM_Fit(FONT_SANS, 15, cv->options[k].label, room - 20), qfalse);
        }
        SM_Hit(ox, oy, cellW, cellH, ST_HIT_OPTION, k, &box);
    }
    if (full > view) {
        float len = view * view / full;

        SM_Fill(x + room, y + st.selectScroll * (view - len) / (full - view), SM_Len(STR_BAR), len, strMenuThumb,
                NULL);
    }
}

static void ST_DrawRGBPicker(void) {
    smRect_t c = ST_DrawDialog(300, 30, ST_HIT_RGBPANEL);
    float leftW = SM_Percent(c.w, 65.3333333f);
    float margin = SM_Len(5.0f), h = SM_Len(32.0f);
    float border = SM_LenBorder(1.0f);
    float thumbW = SM_Len(16.0f);
    float y = c.y + SM_TITLE_H;
    int innerW = SM_Px(leftW) - 2 * SM_Px(border), innerH = SM_Px(h) - 2 * SM_Px(border);
    vec4_t color;
    int k;

    ST_DialogTitle(c.x, c.y, leftW, "Choose color", qfalse);
    for (k = 0; k < 3; k++) {
        vec4_t tint = {0, 0, 0, 1};
        float sy = y + margin;
        int tx = (int)(st.rgbPick[k] / 255.0f * (innerW - SM_Px(thumbW)));
        int ty = (innerH - SM_Px(h)) / 2;

        tint[k] = st.rgbPick[k] / 255.0f;
        st.rgbSlider[k] = SM_R(c.x, sy, leftW, h);
        SM_Fill(c.x, sy, leftW, h, tint, NULL);
        SM_Border(c.x, sy, leftW, h, border, strBorder, NULL);
        SM_Fill(c.x + border + tx / smScale, sy + border + ty / smScale, thumbW, h, strYellow, NULL);
        SM_Hit(c.x, sy, leftW, h, ST_HIT_RGBSLIDER, k, NULL);
        y += margin + h + SM_StrutBelow(15, STR_TEXT_LINE);
    }
    color[0] = st.rgbPick[0] / 255.0f;
    color[1] = st.rgbPick[1] / 255.0f;
    color[2] = st.rgbPick[2] / 255.0f;
    color[3] = 1.0f;
    SM_Fill(c.x + leftW + SM_Percent(c.w, 4.0f), c.y + SM_Len(15.0f), SM_Len(200.0f), SM_Len(200.0f), color, NULL);

    {
        static const char* const labels[2] = {"Cancel", "OK"};
        static const int hits[2] = {ST_HIT_RGBCANCEL, ST_HIT_RGBOK};

        ST_DrawActions(&c, 2, labels, hits);
    }
}

static void ST_DrawRow(int i, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float x = stLayout[i].x;
    float w = stLayout[i].w;
    float h = stLayout[i].h;
    float cx = ST_ControlX(i);
    float base = SM_Baseline(FONT_SANS, 15, y + STR_ROW_PAD + SM_Len(6.0f), STR_TEXT_LINE);
    smRect_t row = SM_R(x, y, w, h);
    qboolean hovered;

    if (cv->widget == ST_BREAK) {
        return;
    }
    if (cv->widget == ST_TEXT) {
        int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
        int lines = SM_Wrap(FONT_SANS, 15, cv->title, w, starts, lens);
        int k;

        for (k = 0; k < lines; k++) {
            float tb = SM_Baseline(FONT_SANS, 15, y + k * STR_TEXT_LINE, STR_TEXT_LINE);

            if (SM_TextInside(tb, 15, clip)) {
                SM_TextRaw(x, tb, FONT_SANS, 15, smText, cv->title + starts[k], lens[k], qfalse);
            }
        }
        return;
    }
    if (cv->widget == ST_HEADING) {
        float hb = SM_Baseline(FONT_DEFAULT, 32, y + h - SM_H1_PAD - SM_H1_LINE, SM_H1_LINE);

        if (SM_TextInside(hb, 32, clip)) {
            SM_SetSpacing(SM_SPACING_H1);
            SM_Text(x + SM_Len(6.4f), hb, FONT_DEFAULT, 32, smText, cv->title, -1, qfalse);
            SM_SetSpacing(0);
        }
        return;
    }
    if (cv->widget == ST_BIND) {
        ST_DrawBind(i, y, clip);
        return;
    }

    hovered = SM_In(&row, smMX, smMY) && SM_In(clip, smMX, smMY) && st.select < 0 && st.rgb < 0 &&
              st.model < 0 && st.drag < 0 && st.previewDrag < 0;
    SM_Fill(x, y, w, h, stLayout[i].odd ? strRowOdd : strRowEven, clip);
    if (SM_TextInside(base, 15, clip)) {
        float tw = cx - x - STR_ROW_PAD - 10;

        SM_Text(x + STR_ROW_PAD, base, FONT_SANS, 15, smText, cv->title, SM_Fit(FONT_SANS, 15, cv->title, tw), qfalse);
        if (hovered) {
            float after = x + STR_ROW_PAD + SM_Advance(FONT_SANS, 15, va("%s ", cv->title), -1);

            SM_Text(after, base, FONT_SANS, 15, SM_Over(ST_HIT_RESET, i) ? strLinkHover : strLink, "(Reset)", -1,
                    qfalse);
            SM_Hit(after, y + STR_ROW_PAD + SM_Len(6.0f), SM_Advance(FONT_SANS, 15, "(Reset)", -1), STR_TEXT_LINE,
                   ST_HIT_RESET, i, clip);
        }
    }
    SM_Hit(x, y, w, h, ST_HIT_ROW, i, clip);

    switch (cv->widget) {
        case ST_BOOL:
            ST_DrawBool(i, cx, y + STR_ROW_PAD, clip);
            break;
        case ST_SELECT:
            ST_DrawSelect(i, cx, y + STR_ROW_PAD, clip);
            break;
        case ST_RANGE:
            ST_DrawRange(i, cx, y + STR_ROW_PAD + SM_Len(9.0f), clip);
            break;
        case ST_INPUT:
            ST_DrawInput(i, cx, y + STR_ROW_PAD, clip);
            break;
        case ST_PALETTE:
            ST_DrawPalette(i, cx, y + STR_ROW_PAD, clip);
            break;
        case ST_RGB:
            ST_DrawRGB(i, cx, y + STR_ROW_PAD, clip);
            break;
        case ST_MODEL:
        case ST_MODELSKIN:
            ST_DrawModelButton(i, cx, y + STR_ROW_PAD, clip);
            break;
        case ST_CROSSHAIR:
            ST_DrawCrosshair(i, cx, y + STR_ROW_PAD, clip);
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
    thumbY = st.top + (st.view - thumbH) * st.scroll[st.tab] / (st.content - st.view);
    SM_Hit(x, st.top, STR_BAR, st.view, ST_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, STR_BAR, thumbH, SM_Over(ST_HIT_THUMB, 0) || st.scrollDrag ? strThumbHover : strThumb, NULL);
    SM_Hit(x, thumbY, STR_BAR, thumbH, ST_HIT_THUMB, 0, NULL);
}

static void ST_DrawHeader(float y, const smRect_t* clip) {
    float left = st.left;
    float x = left;
    float navY = y + STR_TITLE_H;
    int i;

    SM_Fill(left, y, HM_CONTAINER_W - STR_BAR, STR_TITLE_H, strBlack80, clip);
    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 32, y + SM_H1_PAD, SM_H1_LINE), 32, clip)) {
        SM_SetSpacing(SM_SPACING_H1);
        SM_Text(left + 6.4f, SM_Baseline(FONT_DEFAULT, 32, y + SM_H1_PAD, SM_H1_LINE), FONT_DEFAULT, 32, smText,
                "Game Settings", -1, qfalse);
        SM_SetSpacing(0);
    }

    SM_Fill(left, navY, HM_CONTAINER_W - STR_BAR, STR_NAV_H, strBlack80, clip);
    for (i = 0; i < ST_NUM_TABS; i++) {
        qboolean on = i == st.tab;
        float w;

        if (!ST_TabShown(i)) {
            continue;
        }
        w = SM_NavTabWidth(stTabNames[i]);
        if (on) {
            SM_Fill(x, navY, w, STR_NAV_H, strYellow, clip);
        }
        if (SM_TextInside(SM_Baseline(FONT_SANS, 11, navY, STR_NAV_H), 11, clip)) {
            char upper[64];

            Q_strncpyz(upper, stTabNames[i], sizeof(upper));
            Q_strupr(upper);
            SM_SpacedText(x + STR_NAV_PAD, SM_Baseline(FONT_SANS, 11, navY, STR_NAV_H), FONT_SANS, 11,
                          on ? colorBlack : SM_Over(ST_HIT_TAB, i) ? smText : strOption, upper, 1.0f, qtrue);
        }
        SM_Hit(x, navY, w, STR_NAV_H, ST_HIT_TAB, i, clip);
        x += w;
    }
}

static void ST_DrawAction(float y, const smRect_t* clip) {
    const char* action = ST_TabAction(st.tab);
    float bw;

    if (!action) {
        return;
    }
    bw = SM_WebButtonWidth(action);
    SM_WebButton(st.left + STR_SECTION_PAD + SM_Len(15.0f), y + SM_Len(15.0f), bw, action, qfalse,
                 SM_Over(ST_HIT_ACTION, 0), clip);
    SM_Hit(st.left + STR_SECTION_PAD + SM_Len(15.0f), y + SM_Len(15.0f), bw, SM_WEB_BUTTON_H, ST_HIT_ACTION, 0,
           clip);
}

static void ST_DrawPage(void) {
    float frameTop, bottom, y, sectionTop, sectionH;
    smRect_t clip;
    int i;

    if (!st.fromIngame) {
        UI_Home_Backdrop();
    }
    UI_Home_PageTop(HM_NAV_SETTINGS);
    frameTop = HM_NAV_H;
    if (!ST_TabShown(st.tab)) {
        st.tab = ST_TAB_BASIC;
    }

    st.left = UI_Home_Left();
    st.top = frameTop;
    bottom = UI_Home_FrameBottom() - (UI_Home_InMatch() ? 38 : 0);
    st.view = bottom - frameTop;
    ST_Layout();
    st.listTop = frameTop + STR_TITLE_H + STR_NAV_H + STR_NAV_GAP + STR_SECTION_PAD;
    sectionH = st.listBottom + 2 * STR_SECTION_PAD + (ST_TabAction(st.tab) ? 2 * SM_Len(15.0f) + SM_WEB_BUTTON_H : 0);
    st.content = STR_TITLE_H + STR_NAV_H + STR_NAV_GAP + sectionH;
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab]);

    clip = SM_R(st.left, frameTop, HM_CONTAINER_W, st.view);
    y = frameTop - st.scroll[st.tab];
    ST_DrawHeader(y, &clip);
    sectionTop = y + STR_TITLE_H + STR_NAV_H + STR_NAV_GAP;
    SM_Fill(st.left, sectionTop, HM_CONTAINER_W - STR_BAR, sectionH, strBlack80, &clip);

    for (i = 0; i < ST_NUM_CVARS; i++) {
        float ry;

        if (!ST_InTab(i)) {
            continue;
        }
        ry = ST_RowY(i);
        if (ry + stLayout[i].h <= clip.y || ry >= clip.y + clip.h) {
            continue;
        }
        ST_DrawRow(i, ry, &clip);
    }
    ST_DrawAction(st.listTop + st.listBottom - st.scroll[st.tab], &clip);

    if (st.tab == ST_TAB_TEAM) {
        int k;

        for (k = 0; k < 2; k++) {
            float my = st.listTop + stModelBox[k].y - st.scroll[st.tab];

            if (stModelBox[k].h <= 0 || my < clip.y - 1 || my + stModelBox[k].h > clip.y + clip.h + 1) {
                continue;
            }
            ST_DrawTeamPreview(k, stModelBox[k].x, my, stModelBox[k].w, stModelBox[k].h);
        }
    }

    if (st.content > st.view) {
        ST_DrawScrollbar(st.left + HM_CONTAINER_W - STR_BAR);
    }
}

static void ST_DrawBindDialog(void) {
    static const char* const notes[2] = {"ESC to exit", "BACKSPACE to unbind"};
    smRect_t c = ST_DrawDialog(300, 15, ST_HIT_BLOCK);
    float keyLine = SM_LineHeight(24, 1.25f), noteLine = SM_LineHeight(21.3333f, 1.6f);
    float y = c.y + SM_TITLE_H;
    float gap, width = 0, x;
    char keys[128];
    const char* p;
    int k;

    ST_DialogTitle(c.x, c.y, c.w, va("Press any key to bind %s", stCvars[st.binding].title), qtrue);
    ST_BindKeys(st.binding, keys, sizeof(keys));

    SM_SetSpacing(SM_SPACING_H1);
    gap = SM_Advance(FONT_SANS, 24, " ", 1) + SM_LetterSpacing(8.0f);
    for (p = keys; *p;) {
        const char* end = strchr(p, ' ');
        int len = end ? (int)(end - p) : (int)strlen(p);

        width += SM_Advance(FONT_SANS, 24, p, len) + (end ? gap : 0);
        p += len + (end ? 1 : 0);
    }
    x = c.x + SM_Center(c.w, width);
    for (p = keys; *p;) {
        const char* end = strchr(p, ' ');
        int len = end ? (int)(end - p) : (int)strlen(p);

        SM_Text(x, SM_Baseline(FONT_SANS, 24, y, keyLine), FONT_SANS, 24, strYellow, p, len, qfalse);
        x += SM_Advance(FONT_SANS, 24, p, len) + gap;
        p += len + (end ? 1 : 0);
    }
    SM_SetSpacing(0);
    y += keyLine + SM_Rem(20.0f);

    for (k = 0; k < 2; k++) {
        SM_Text(c.x + SM_Center(c.w, SM_Advance(FONT_SANS, 21.3333f, notes[k], -1)),
                SM_Baseline(FONT_SANS, 21.3333f, y, noteLine), FONT_SANS, 21.3333f, smText, notes[k], -1, qfalse);
        y += noteLine;
    }
}

static void ST_OpenSelect(int i) {
    const stCvar_t* cv = &stCvars[i];
    int k, chosen = -1;

    st.select = i;
    st.selectScroll = 0;
    if (cv->widget == ST_PALETTE) {
        chosen = atoi(st.values[i]) - 1;
    } else if (cv->widget == ST_CROSSHAIR) {
        chosen = atoi(st.values[i]);
    } else {
        for (k = 0; k < cv->numOptions; k++) {
            if (!Q_stricmp(cv->options[k].value, st.values[i])) {
                chosen = k;
            }
        }
    }
    st.selectFocus = chosen >= 0 && chosen < ST_OptionCount(i) ? chosen : 0;
}

static void ST_DragRange(int i) {
    const stCvar_t* cv = &stCvars[i];
    float bx = ST_ControlX(i) + ST_ControlW(i) * 0.2f;
    float bw = ST_ControlW(i) * 0.8f;
    float frac = (smMX - bx - 1 - 8) / (bw - 2 - 16);
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
    const smRect_t* r = &st.rgbSlider[channel];
    float border = SM_LenBorder(1.0f), thumbW = SM_Len(16.0f);
    float frac = (smMX - r->x - border - thumbW * 0.5f) / (r->w - 2 * border - thumbW);

    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    st.rgbPick[channel] = (int)(frac * 255.0f + 0.5f);
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

static int ST_CaretAt(int i, float textX, int start) {
    const char* value = st.values[i];
    int len = (int)strlen(value);
    float prev = 0, adv;
    int n;

    for (n = start; n < len; n++) {
        adv = SM_AdvanceRaw(FONT_SANS, 15, value + start, n + 1 - start, qfalse);
        if (smMX < textX + (prev + adv) * 0.5f) {
            return n;
        }
        prev = adv;
    }
    return len;
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

    if (st.model >= 0) {
        switch (type) {
            case ST_HIT_MODELITEM:
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

    if (st.select >= 0) {
        if (type == ST_HIT_OPTION) {
            if (stCvars[st.select].widget == ST_CROSSHAIR) {
                ST_Set(st.select, va("%i", index));
            } else if (stCvars[st.select].widget == ST_PALETTE) {
                ST_Set(st.select, va("%i", index + 1));
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
        if (type == ST_HIT_RGBOK) {
            ST_SetRGB(st.rgb, st.rgbPick);
        }
        st.rgb = -1;
        return;
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
        return;
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
            if (st.edit != index) {
                st.edit = index;
                st.editStart = 0;
            }
            st.editCursor = ST_CaretAt(index, hit->r.x + SM_LenBorder(1.0f) + SM_Len(8.0f) + 1.0f / smScale, st.editStart);
            st.editTime = uiInfo.uiDC.realTime;
            break;
        case ST_HIT_RESET:
            if (stCvars[index].cvar) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("reset %s\n", stCvars[index].cvar));
            }
            break;
        case ST_HIT_RGB:
            st.rgb = index;
            ST_ParseRGB(st.values[index], st.rgbPick);
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
        float cellW, cellH, view;
        int perRow;

        ST_MenuLayout(st.select, st.selectControl.w, &cellW, &cellH, &perRow, &view);
        st.selectScroll += amount > 0 ? 2 * cellH : -2 * cellH;
        return;
    }
    if (st.rgb >= 0) {
        return;
    }
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab] + amount);
}

static void ST_Bind(int i, int key) {
    const char* command = stCvars[i].cvar;
    int k;

    if (key == K_BACKSPACE) {
        for (k = 0; k < K_LAST_KEY; k++) {
            char buf[MAX_STRING_CHARS];

            trap_Key_GetBindingBuf(k, buf, sizeof(buf));
            if (buf[0] && !Q_stricmp(buf, command)) {
                trap_Key_SetBinding(k, "");
            }
        }
    } else {
        trap_Key_SetBinding(key, command);
    }
    ST_Read(i);
}

static void ST_Paste(void) {
    char clip[1024];
    char* buffer = st.values[st.edit];
    int len = (int)strlen(buffer);
    int room = MIN(ST_MAX_INPUT, (int)sizeof(st.values[0]) - 1) - len;
    int i, n = 0;

    trap_GetClipboardData(clip, sizeof(clip));
    for (i = 0; clip[i] && n < room; i++) {
        if ((unsigned char)clip[i] >= ' ') {
            clip[n++] = clip[i];
        }
    }
    if (n <= 0) {
        return;
    }
    memmove(buffer + st.editCursor + n, buffer + st.editCursor, len - st.editCursor + 1);
    memcpy(buffer + st.editCursor, clip, n);
    st.editCursor += n;
}

static void ST_EditNext(int dir) {
    int i;

    ST_Set(st.edit, st.values[st.edit]);
    for (i = st.edit + dir; i >= 0 && i < ST_NUM_CVARS; i += dir) {
        if (stCvars[i].tab == st.tab && stCvars[i].widget == ST_INPUT) {
            st.edit = i;
            st.editCursor = (int)strlen(st.values[i]);
            st.editStart = 0;
            return;
        }
    }
    st.edit = -1;
}

static qboolean ST_EditKey(int key) {
    char* buffer = st.values[st.edit];
    int len = (int)strlen(buffer);

    st.editTime = uiInfo.uiDC.realTime;
    if (key & K_CHAR_FLAG) {
        int c = key & ~K_CHAR_FLAG;

        if (c == 'v' - 'a' + 1) {
            ST_Paste();
            return qtrue;
        }
        if (c == 'h' - 'a' + 1) {
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
        case K_INS:
            if (trap_Key_IsDown(K_SHIFT)) {
                ST_Paste();
            }
            return qtrue;
        case K_TAB:
            ST_EditNext(trap_Key_IsDown(K_SHIFT) ? -1 : 1);
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
    SM_SetLook(SM_LOOK_RETAIL);
    if (!st.loaded) {
        UI_Settings_Open();
    }

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
        ST_DrawDropdown();
    }
    if (st.binding >= 0) {
        ST_DrawBindDialog();
    }
    if (st.rgb >= 0) {
        ST_DrawRGBPicker();
    }
    if (st.model >= 0) {
        ST_DrawModelChooser();
    }
    SM_SetLook(SM_LOOK_RETAIL);
    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Settings_HandleKey(int key) {
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
            if (st.rgb >= 0) {
                ST_SetRGB(st.rgb, st.rgbPick);
                st.rgb = -1;
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
            return qfalse;
        default:
            return qfalse;
    }
}

void UI_Settings_Close(void) {
    qboolean fromIngame = st.fromIngame;

    if (st.edit >= 0) {
        ST_Set(st.edit, st.values[st.edit]);
        st.edit = -1;
    }
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
