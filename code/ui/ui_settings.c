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
    smRect_t rgbPanel;
    int rgbPick[3];
    smRect_t rgbSlider[3];

    int model;
    int pickModel;
    int pickSkin;
    int pickSkinRow;
    playerInfo_t modelInfo;
    char modelShown[MAX_QPATH];

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

#define STR_TITLE_H 54.4f
#define STR_NAV_H 30.0f
#define STR_NAV_PAD 20.0f
#define STR_NAV_GAP 15.0f
#define STR_SECTION_PAD 5.0f
#define STR_BAR 8.0f
#define STR_OPTION_H 40.0f
#define STR_MENU_H 198.0f
#define STR_SWATCH_OPTION_H 37.0f
#define STR_CROSSHAIR_OPTION_H 72.0f
#define STR_LINE_MENU_H 300.0f

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

static float ST_RowHeight(int i) {
    switch (stCvars[i].widget) {
        case ST_BIND:
            return 47.0f;
        case ST_INPUT:
            return 53.0f;
        case ST_SELECT:
        case ST_BOOL:
        case ST_PALETTE:
            return 51.0f;
        case ST_MODEL:
        case ST_MODELSKIN:
            return 63.0f;
        case ST_RGB:
            return 56.0f;
        case ST_CROSSHAIR:
            return 79.0f;
        default:
            return 50.0f;
    }
}

static float ST_HeadingHeight(qboolean first) {
    return (first ? 8.0f : 24.0f) + 38.4f + 8.0f;
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
    float inner = HM_CONTAINER_W - STR_BAR - 2 * STR_SECTION_PAD;
    float x = st.left + STR_SECTION_PAD;
    float y[2];
    int n[2];
    float pairTop = -1, pairH = 0;
    int i;

    st.colW = inner * 0.48f;
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
            stLayout[i].x = x + (col > 0 ? st.colW + inner * 0.04f : 0);
            stLayout[i].y = top;
            stLayout[i].w = col < 0 ? inner : st.colW;
            stLayout[i].h = h;
            y[0] = y[1] = top + h;
            n[0] = n[1] = 0;
            continue;
        }
        c = stCvars[i].column ? 1 : 0;
        stLayout[i].x = x + c * (st.colW + inner * 0.04f);
        stLayout[i].y = y[c];
        stLayout[i].w = st.colW;
        stLayout[i].h = ST_RowHeight(i);
        stLayout[i].odd = !(n[c]++ & 1);
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
    return stLayout[i].x + 7.5f + (stLayout[i].w - 15) * (0.566667f + 0.04f);
}

static float ST_ControlW(int i) {
    return (stLayout[i].w - 15) * 0.393333f;
}

static void ST_DrawSelectBox(int i, float x, float y, float w, float h, const smRect_t* clip) {
    qboolean open = st.select == i;

    SM_Fill(x, y, w, h, SM_Over(ST_HIT_SELECT, i) || open ? strSelectHover : strSelect, clip);
    if (y + h * 0.5f >= clip->y && y + h * 0.5f <= clip->y + clip->h) {
        SM_Arrow(x + w - 20, y + h * 0.5f, open);
    }
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
    float base = SM_Baseline(FONT_SANS, 15, y + 8, 24);
    int k;

    for (k = 0; k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, st.values[i])) {
            label = cv->options[k].label;
            chosen = qtrue;
        }
    }
    ST_DrawSelectBox(i, x, y, w, 36, clip);
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + 10, base, FONT_SANS, 15, chosen ? strSelectValue : strPlaceholder, label,
                SM_Fit(FONT_SANS, 15, label, w - 52), qfalse);
    }
}

static void ST_DrawBool(int i, float x, float y, const smRect_t* clip) {
    qboolean on = atoi(st.values[i]) != 0;
    float base = SM_Baseline(FONT_SANS, 15, y + 9, 18);
    float tw = SM_AdvanceBold(FONT_SANS, 15, on ? "On" : "Off", -1, on);

    if (SM_Over(ST_HIT_BOOL, i)) {
        SM_Fill(x, y, 21, 36, strHover, clip);
    }
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + 8, base, FONT_SANS, 15, on ? strOn : strOff, on ? "On" : "Off", -1, on);
    }
    SM_Hit(x, y, 8 + tw, 36, ST_HIT_BOOL, i, clip);
}

static void ST_DrawRange(int i, float x, float y, const smRect_t* clip) {
    const stCvar_t* cv = &stCvars[i];
    float w = ST_ControlW(i);
    float bx = x + w * 0.2f, bw = w * 0.8f;
    float value = ST_Snap(cv, (float)atof(st.values[i]));
    float base = SM_Baseline(FONT_SANS, 15, y + 1, 16);
    char text[32];

    ST_FormatStep(cv, value, text, sizeof(text));
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + 8, base, FONT_SANS, 15, smText, text, -1, qfalse);
    }
    SM_Fill(bx, y, bw, 18, SM_Over(ST_HIT_RANGE, i) || st.drag == i ? strHover : strRange, clip);
    SM_Border(bx, y, bw, 18, 1, strBorder, clip);
    SM_Fill(bx + 1 + ST_Fraction(cv, value) * (bw - 2 - 16), y + 1, 16, 16, strYellow, clip);
    SM_Hit(bx, y, bw, 18, ST_HIT_RANGE, i, clip);
}

static void ST_DrawInput(int i, float x, float y, const smRect_t* clip) {
    const char* value = st.values[i];
    float w = ST_ControlW(i), h = 38;
    float width = w - 17;
    float base = SM_Baseline(FONT_SANS, 15, y, h);
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
        SM_TextRaw(x + 9, base, FONT_SANS, 15, smText, value + start, SM_Fit(FONT_SANS, 15, value + start, width),
                   qfalse);
        if (editing && ((uiInfo.uiDC.realTime - st.editTime) / 500) % 2 == 0) {
            SM_Fill(x + 9 + SM_AdvanceRaw(FONT_SANS, 15, value + start, st.editCursor - start, qfalse), y + 10, 1,
                    h - 20, smText, clip);
        }
    }
    SM_Hit(x, y, w, h, ST_HIT_INPUT, i, clip);
}

static void ST_DrawSwatch(float x, float y, float w, int value, const smRect_t* clip) {
    const vec4_t* palette = UI_CrosshairPalette();

    if (value < 1 || value > 26) {
        return;
    }
    SM_Fill(x, y, w, 21, palette[value - 1], clip);
    SM_Border(x, y, w, 21, 3, strSwatchEdge, clip);
}

static void ST_DrawPalette(int i, float x, float y, const smRect_t* clip) {
    float w = ST_ControlW(i);

    ST_DrawSelectBox(i, x, y, w, 36, clip);
    ST_DrawSwatch(x + 10, y + 7.5f, w - 62, atoi(st.values[i]), clip);
}

static void ST_DrawRGB(int i, float x, float y, const smRect_t* clip) {
    int rgb[3];
    vec4_t color;
    float w = ST_ControlW(i);

    ST_ParseRGB(st.values[i], rgb);
    color[0] = rgb[0] / 255.0f;
    color[1] = rgb[1] / 255.0f;
    color[2] = rgb[2] / 255.0f;
    color[3] = 1.0f;
    SM_Fill(x, y + 3, w, 38, color, clip);
    SM_Border(x, y + 3, w, 38, 5, strSwatchEdge, clip);
    SM_Hit(x, y + 3, w, 38, ST_HIT_RGB, i, clip);
}

static void ST_DrawCrosshair(int i, float x, float y, const smRect_t* clip) {
    int shape = atoi(st.values[i]);
    float w = ST_ControlW(i);

    ST_DrawSelectBox(i, x, y, w, 64, clip);
    if (shape > 0 && shape < NUM_CROSSHAIRS) {
        SM_Pic(x + 10 + (w - 62 - 64) * 0.5f, y, 64, 64, uiInfo.uiDC.Assets.crosshairShader[shape], clip);
    } else if (SM_TextInside(SM_Baseline(FONT_SANS, 15, y + 20, 24), 15, clip)) {
        SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 20, 24), FONT_SANS, 15, strPlaceholder, "None", -1, qfalse);
    }
}

static void ST_DrawBind(int i, float y, const smRect_t* clip) {
    float x = stLayout[i].x, w = stLayout[i].w, h = stLayout[i].h;
    float base = SM_Baseline(FONT_SANS, 15, y + 7.5f, 32);
    char keys[128];
    const char* p = st.values[i];
    int n = 0;

    keys[0] = '\0';
    while (*p) {
        const char* end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);

        Q_strcat(keys, sizeof(keys), n++ ? ", " : "");
        if ((int)strlen(keys) + len < (int)sizeof(keys)) {
            strncat(keys, p, len);
        }
        p += len;
        if (*p == '\n') {
            p++;
        }
    }
    SM_Fill(x, y, w, h, SM_Over(ST_HIT_BIND, i) ? strBindHover : stLayout[i].odd ? strRowOdd : strRowEven, clip);
    if (SM_TextInside(base, 15, clip)) {
        SM_Text(x + 7.5f, base, FONT_SANS, 15, smText, stCvars[i].title,
                SM_Fit(FONT_SANS, 15, stCvars[i].title, (w - 15) * 0.6f), qfalse);
        SM_Text(x + 7.5f + (w - 15) * 0.6f, base, FONT_SANS, 15, smText, n ? keys : "None",
                SM_Fit(FONT_SANS, 15, n ? keys : "None", (w - 15) * 0.4f), qfalse);
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
    float w = ST_ControlW(i);
    float tx = x + 10;
    float base = SM_Baseline(FONT_SANS, 11, y + (narrow ? 15.7f : 15.0f), 15);
    char upper[64];

    SM_Fill(x, y, w, 48, over ? smWebButtonHover : smWebButton, clip);
    if (st.values[i][0]) {
        char skin[MAX_QPATH];
        int model = ST_ParseModel(st.values[i], skin, sizeof(skin));
        qhandle_t icon = ST_IconFor(model, skin[0] ? skin : "default");

        if (icon && narrow) {
            SM_Pic(x + 10, y + 10, 24, 24, icon, clip);
            tx = x + 10 + 24 + 5.0f;
        } else if (icon) {
            SM_Pic(x + 10, y, 48, 48, icon, clip);
            tx = x + 10 + 48 + 9.4f;
        }
    }
    Q_strncpyz(upper, value, sizeof(upper));
    Q_strupr(upper);
    if (SM_TextInside(base, 11, clip)) {
        SM_SpacedText(tx, base, FONT_SANS, 11, over ? smWebYellow : smText, upper, 1.0f, qtrue);
    }
    SM_Hit(x, y, w, 48, ST_HIT_MODEL, i, clip);
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

static smRect_t ST_DrawDialog(float height) {
    float top = (SM_VIEW_H - height) * 0.5f;
    float cx = smVW * 0.15f, cw = smVW * 0.7f;

    SM_Fill(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, strBlack80, NULL);
    SM_Hit(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, ST_HIT_BLOCK, 0, NULL);
    SM_Fill(0, top, smVW, height, strDialog, NULL);
    SM_Fill(cx, top, cw, height, strBlack80, NULL);
    return SM_R(cx + 15, top + 15, cw - 30, height - 30);
}

static void ST_DrawActions(const smRect_t* c, int count, const char* const* labels, const int* hits) {
    float x = c->x + c->w;
    float y = c->y + c->h - SM_WEB_BUTTON_H;
    int k;

    for (k = count - 1; k >= 0; k--) {
        float bw = SM_WebButtonWidth(labels[k]);

        x -= bw;
        SM_WebButton(x, y, bw, labels[k], qfalse, SM_Over(hits[k], 0), NULL);
        SM_Hit(x, y, bw, SM_WEB_BUTTON_H, hits[k], 0, NULL);
        x -= 10;
    }
}

static void ST_DrawModelChooser(void) {
    smRect_t c = ST_DrawDialog(600);
    float listW = c.w * 0.306667f;
    float paneX = c.x + listW + c.w * 0.04f;
    float paneW = c.w * 0.653333f;
    int perRow = (int)(listW / 62.0f);
    float listX = c.x + (listW - (perRow > 0 ? perRow : 1) * 62.0f) * 0.5f;
    qboolean skins = stCvars[st.model].widget == ST_MODELSKIN;
    qboolean clear = stCvars[st.model].widget == ST_MODEL;
    const char* title = stModels[st.pickModel].name;
    float y = c.y + 8 + 38.4f + 8;
    int count = ST_PickSkinCount();
    int k;

    if (perRow < 1) {
        perRow = 1;
    }
    SM_Hit(c.x - 15, c.y - 15, c.w + 30, c.h + 30, ST_HIT_MODELPANE, 0, NULL);

    for (k = 0; k < ST_NUM_MODELS; k++) {
        float cx = listX + (k % perRow) * 62.0f;
        float cy = c.y + (k / perRow) * 66.0f;
        qhandle_t icon = ST_IconFor(k, "default");

        if (icon) {
            SM_Pic(cx, cy, 62, 62, icon, NULL);
        }
        if (k == st.pickModel) {
            SM_Border(cx - 2, cy - 2, 66, 66, 2, strYellow, NULL);
        }
        SM_Hit(cx, cy, 62, 62, ST_HIT_MODELITEM, k, NULL);
    }

    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(paneX + 6.4f, SM_Baseline(FONT_DEFAULT, 32, c.y + 8, 38.4f), FONT_DEFAULT, 32, smText, title, -1, qfalse);
    SM_SetSpacing(0);
    if (skins) {
        for (k = 0; k < count; k++) {
            const char* value = ST_PickSkinValue(k);
            float cx = paneX + k * 62.0f;
            qhandle_t icon = ST_IconFor(st.pickModel, value[0] ? value : "default");

            if (icon) {
                SM_Pic(cx, y, 62, 62, icon, NULL);
            }
            if (k == st.pickSkin) {
                SM_Border(cx - 2, y - 2, 66, 66, 2, strYellow, NULL);
            }
            SM_Hit(cx, y, 62, 62, ST_HIT_SKINITEM, k, NULL);
        }
        y += 66;
    }
    ST_DrawChooserPreview(paneX, y + 15, paneW, c.y + c.h - SM_WEB_BUTTON_H - 15 - (y + 15));

    {
        static const char* const labels[3] = {"Clear", "Cancel", "OK"};
        static const int hits[3] = {ST_HIT_MODELCLEAR, ST_HIT_MODELCANCEL, ST_HIT_MODELOK};

        ST_DrawActions(&c, clear ? 3 : 2, clear ? labels : labels + 1, clear ? hits : hits + 1);
    }
}

static float ST_OptionHeight(int widget) {
    return widget == ST_PALETTE ? STR_SWATCH_OPTION_H : widget == ST_CROSSHAIR ? STR_CROSSHAIR_OPTION_H : STR_OPTION_H;
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
    float x = st.selectControl.x, y = st.selectControl.y + st.selectControl.h - 1;
    float w = st.selectControl.w;
    float oh = ST_OptionHeight(cv->widget);
    int count = ST_OptionCount(st.select);
    float full = count * oh;
    float most = cv->widget == ST_SELECT ? STR_MENU_H : STR_LINE_MENU_H;
    float view = full < most ? full : most;
    smRect_t box;
    int k;

    if (y + view > smBottom - 30) {
        y = st.selectControl.y - view + 1;
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
        float oy = y + k * oh - st.selectScroll;

        if (oy + oh < y || oy > y + view) {
            continue;
        }
        if (SM_Over(ST_HIT_OPTION, k)) {
            st.selectFocus = k;
        }
        if (k == st.selectFocus) {
            SM_Fill(x, oy, w, oh, strOptionFocus, &box);
        }
        if (cv->widget == ST_PALETTE) {
            ST_DrawSwatch(x + 10, oy + 8, w - 20, k + 1, &box);
        } else if (cv->widget == ST_CROSSHAIR) {
            if (k > 0) {
                SM_Pic(x + (w - 64) * 0.5f, oy, 64, 64, uiInfo.uiDC.Assets.crosshairShader[k], &box);
            } else if (SM_TextInside(SM_Baseline(FONT_SANS, 15, oy + 8, 24), 15, &box)) {
                SM_Text(x + (w - SM_Advance(FONT_SANS, 15, "None", -1)) * 0.5f, SM_Baseline(FONT_SANS, 15, oy + 8, 24),
                        FONT_SANS, 15, k == st.selectFocus ? strSelectValue : strOption, "None", -1, qfalse);
            }
        } else if (SM_TextInside(SM_Baseline(FONT_SANS, 15, oy + 8, 24), 15, &box)) {
            SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, oy + 8, 24), FONT_SANS, 15,
                    k == st.selectFocus ? strSelectValue : strOption, cv->options[k].label,
                    SM_Fit(FONT_SANS, 15, cv->options[k].label, w - 20), qfalse);
        }
        SM_Hit(x, oy, w, oh, ST_HIT_OPTION, k, &box);
    }
}

static void ST_DrawRGBPicker(void) {
    smRect_t c = ST_DrawDialog(400);
    float leftW = c.w * 0.653333f;
    float y = c.y + 8 + 38.4f + 8;
    vec4_t color;
    int k;

    st.rgbPanel = SM_R(c.x - 15, c.y - 15, c.w + 30, c.h + 30);
    SM_Hit(st.rgbPanel.x, st.rgbPanel.y, st.rgbPanel.w, st.rgbPanel.h, ST_HIT_RGBPANEL, 0, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(c.x + 6.4f, SM_Baseline(FONT_DEFAULT, 32, c.y + 8, 38.4f), FONT_DEFAULT, 32, smText, "Choose color", -1,
            qfalse);
    SM_SetSpacing(0);
    for (k = 0; k < 3; k++) {
        vec4_t tint = {0, 0, 0, 1};
        float sy = y + 5 + k * 57.0f;

        tint[k] = st.rgbPick[k] / 255.0f;
        st.rgbSlider[k] = SM_R(c.x, sy, leftW, 32);
        SM_Fill(c.x, sy, leftW, 32, tint, NULL);
        SM_Border(c.x, sy, leftW, 32, 1, strBorder, NULL);
        SM_Fill(c.x + 1 + st.rgbPick[k] / 255.0f * (leftW - 2 - 16), sy + 8, 16, 16, strYellow, NULL);
        SM_Hit(c.x, sy, leftW, 32, ST_HIT_RGBSLIDER, k, NULL);
    }
    color[0] = st.rgbPick[0] / 255.0f;
    color[1] = st.rgbPick[1] / 255.0f;
    color[2] = st.rgbPick[2] / 255.0f;
    color[3] = 1.0f;
    SM_Fill(c.x + leftW + c.w * 0.04f, c.y + 15, 200, 200, color, NULL);

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
    float base = SM_Baseline(FONT_SANS, 15, y + 13.5f, 24);
    smRect_t row = SM_R(x, y, w, h);
    qboolean hovered;

    if (cv->widget == ST_HEADING) {
        float hb = SM_Baseline(FONT_DEFAULT, 32, y + h - 8 - 38.4f, 38.4f);

        if (SM_TextInside(hb, 32, clip)) {
            SM_SetSpacing(SM_SPACING_H1);
            SM_Text(x + 6.4f, hb, FONT_DEFAULT, 32, smText, cv->title, -1, qfalse);
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
        float tw = cx - x - 7.5f - 10;

        SM_Text(x + 7.5f, base, FONT_SANS, 15, smText, cv->title, SM_Fit(FONT_SANS, 15, cv->title, tw), qfalse);
        if (hovered) {
            float after = x + 7.5f + SM_Advance(FONT_SANS, 15, va("%s ", cv->title), -1);

            SM_Text(after, base, FONT_SANS, 15, SM_Over(ST_HIT_RESET, i) ? strLinkHover : strLink, "(Reset)", -1,
                    qfalse);
            SM_Hit(after, y + 13.5f, SM_Advance(FONT_SANS, 15, "(Reset)", -1), 24, ST_HIT_RESET, i, clip);
        }
    }
    SM_Hit(x, y, w, h, ST_HIT_ROW, i, clip);

    switch (cv->widget) {
        case ST_BOOL:
            ST_DrawBool(i, cx, y + 7.5f, clip);
            break;
        case ST_SELECT:
            ST_DrawSelect(i, cx, y + 7.5f, clip);
            break;
        case ST_RANGE:
            ST_DrawRange(i, cx, y + 16.5f, clip);
            break;
        case ST_INPUT:
            ST_DrawInput(i, cx, y + 7.5f, clip);
            break;
        case ST_PALETTE:
            ST_DrawPalette(i, cx, y + 7.5f, clip);
            break;
        case ST_RGB:
            ST_DrawRGB(i, cx, y + 7.5f, clip);
            break;
        case ST_MODEL:
        case ST_MODELSKIN:
            ST_DrawModelButton(i, cx, y + 7.5f, clip);
            break;
        case ST_CROSSHAIR:
            ST_DrawCrosshair(i, cx, y + 7.5f, clip);
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

static float ST_TabWidth(const char* name) {
    char upper[64];
    char c[2];
    const char* p;
    float w = 0;

    Q_strncpyz(upper, name, sizeof(upper));
    Q_strupr(upper);
    c[1] = '\0';
    for (p = upper; *p; p++) {
        c[0] = *p;
        w += SM_AdvanceBold(FONT_SANS, 11, c, 1, qtrue) + 1.0f;
    }
    return w - 1.0f;
}

static void ST_DrawHeader(float y, const smRect_t* clip) {
    float left = st.left;
    float x = left;
    float navY = y + STR_TITLE_H;
    int i;

    SM_Fill(left, y, HM_CONTAINER_W - STR_BAR, STR_TITLE_H, strBlack80, clip);
    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), 32, clip)) {
        SM_SetSpacing(SM_SPACING_H1);
        SM_Text(left + 6.4f, SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), FONT_DEFAULT, 32, smText, "Game Settings", -1,
                qfalse);
        SM_SetSpacing(0);
    }

    SM_Fill(left, navY, HM_CONTAINER_W - STR_BAR, STR_NAV_H, strBlack80, clip);
    for (i = 0; i < ST_NUM_TABS; i++) {
        qboolean on = i == st.tab;
        float w;

        if (!ST_TabShown(i)) {
            continue;
        }
        w = ST_TabWidth(stTabNames[i]) + 2 * STR_NAV_PAD;
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
        x += w + 1.0f;
    }
}

static void ST_DrawAction(float y, const smRect_t* clip) {
    const char* action = ST_TabAction(st.tab);
    float bw;

    if (!action) {
        return;
    }
    bw = SM_WebButtonWidth(action);
    SM_WebButton(st.left + STR_SECTION_PAD + 15, y + 15, bw, action, qfalse, SM_Over(ST_HIT_ACTION, 0), clip);
    SM_Hit(st.left + STR_SECTION_PAD + 15, y + 15, bw, SM_WEB_BUTTON_H, ST_HIT_ACTION, 0, clip);
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
    sectionH = st.listBottom + 2 * STR_SECTION_PAD + (ST_TabAction(st.tab) ? 30 + SM_WEB_BUTTON_H : 0);
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
    const stCvar_t* cv = &stCvars[st.binding];
    float h = 300;
    float top = (SM_VIEW_H - h) * 0.5f;
    float cx = smVW * 0.15f, cw = smVW * 0.7f;
    const char* lines[4];
    float sizes[4] = {32, 24, 21.3333f, 21.3333f};
    int fonts[4] = {FONT_DEFAULT, FONT_SANS, FONT_SANS, FONT_SANS};
    const float* colors[4] = {smText, strYellow, smText, smText};
    char keys[128];
    const char* p = st.values[st.binding];
    float y = top + 30;
    int k, n = 0;

    keys[0] = '\0';
    while (*p) {
        const char* end = strchr(p, '\n');
        int len = end ? (int)(end - p) : (int)strlen(p);

        Q_strcat(keys, sizeof(keys), n++ ? ", " : "");
        if ((int)strlen(keys) + len < (int)sizeof(keys)) {
            strncat(keys, p, len);
        }
        p += len;
        if (*p == '\n') {
            p++;
        }
    }
    lines[0] = va("Press any key to bind %s", cv->title);
    lines[1] = n ? keys : "None";
    lines[2] = "ESC to exit";
    lines[3] = "BACKSPACE to unbind";

    SM_Fill(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, strBlack80, NULL);
    SM_Hit(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, ST_HIT_BLOCK, 0, NULL);
    SM_Fill(0, top, smVW, h, strDialog, NULL);
    SM_Fill(cx, top, cw, h, strBlack80, NULL);
    for (k = 0; k < 4; k++) {
        const char* text = lines[k];
        float lh = sizes[k] * 1.5f;

        SM_SetSpacing(k < 2 ? SM_SPACING_H1 : 0);
        SM_Text(cx + (cw - SM_Advance(fonts[k], sizes[k], text, -1)) * 0.5f, SM_Baseline(fonts[k], sizes[k], y, lh),
                fonts[k], sizes[k], colors[k], text, -1, qfalse);
        SM_SetSpacing(0);
        y += lh + 12;
    }
}

static void ST_OpenSelect(int i) {
    const stCvar_t* cv = &stCvars[i];
    float oh = ST_OptionHeight(cv->widget);
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
    st.selectFocus = chosen;
    if (chosen > 0) {
        st.selectScroll = (chosen - 2) * oh;
    }
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
    float frac = (smMX - r->x - 1 - 8) / (r->w - 2 - 16);

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
            st.edit = index;
            st.editCursor = (int)strlen(st.values[index]);
            st.editStart = 0;
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
        float oh = ST_OptionHeight(stCvars[st.select].widget);

        st.selectScroll += amount > 0 ? 2 * oh : -2 * oh;
        return;
    }
    if (st.rgb >= 0) {
        return;
    }
    st.scroll[st.tab] = ST_ClampScroll(st.scroll[st.tab] + amount);
}

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
            return;
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
