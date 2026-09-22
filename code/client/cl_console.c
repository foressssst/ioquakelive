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
// console.c

#include "client.h"

int g_console_field_width = 119;

#define NUM_CON_TIMES 4

#define CON_TEXTSIZE 524288

// [QL] Quake Live draws the console with DroidSansMono, font index 2 of the
// TrueType renderer, in 12x24 cells scaled by con_scale: Con_CheckResize
// (quakelive_steam.exe 0x4b3a50) writes the cell size, the text calls
// (0x4bdd40, 0x4bdd90, 0x4b46e8) ask for a font size of cell width * 2.1597
// (float at 0x56732c), which gives the font's advance.
#define CON_FONT_MONO 2
#define CON_CELL_WIDTH 12
#define CON_CELL_HEIGHT 24
#define CON_FONT_SCALE 2.1597f
#define CON_TEXT_LITERAL 0x40000000  // renderer FONS_TEXT_LITERAL: '^' drawn as a glyph
#define CON_LINEBUFFER 0x800         // retail's row buffer size

// [CNQ3] con_style 1 line width cap, keeps the per-line scratch buffers small
#define CON_MAXLINEWIDTH 1000

// [CNQ3] text buffer codes of the console-only colours, after the 36 colour
// codes (Quake Live's 0-7 and CPMA's 8-35)
#define CON_COLOR_CVAR 40
#define CON_COLOR_CMD 41
#define CON_COLOR_VAL 42
#define CON_COLOR_HELP 43

// [CNQ3] con_drawHelp bits
#define DRAWHELP_ENABLE_BIT 1
#define DRAWHELP_NOTFOUND_BIT 2
#define DRAWHELP_MODULES_BIT 4
#define DRAWHELP_ATTRIBS_BIT 8
#define DRAWHELP_MAX 15

// [ioquakelive] a link the console drew this frame, and where it drew it
#define CON_MAX_LINKS 64
#define CON_MAX_URL 512

typedef struct {
    float x, y, w, h;
    char url[CON_MAX_URL];
} conLink_t;

typedef struct {
    qboolean initialized;

    short text[CON_TEXTSIZE];
    int current;  // line where next message will be printed
    int x;        // offset in current line for next print
    int display;  // bottom of console displays this line

    int linewidth;   // characters across screen
    int totallines;  // total lines in console scrollback

    float xadjust;  // for wide aspect screens

    float finalFrac;  // 0.0 to 1.0 lines of console to display

    int vislines;       // in scanlines
    float displayFrac;  // aproaches finalFrac at con_speed

    int times[NUM_CON_TIMES];  // cls.realtime time the line was generated
    // for transparent notify lines
    vec4_t color;

    // [CNQ3] con_style 1
    int rowsVisible;  // full text rows drawn by the last frame

    char helpText[MAXPRINTMSG];
    int helpX;           // char index in the current help line
    float helpY;         // top of the current help line
    int helpWidth;       // char count of the longest help line
    int helpLines;       // help line count
    qboolean helpDraw;   // qfalse while measuring
    float helpXAdjust;

    // [CNQ3] searchconsole, both styles
    char searchPattern[256];
    int searchLine;  // absolute line of the last match
    qboolean searchStarted;

    // [CNQ3] mark mode, both styles
    qboolean markMode;
    int markX;       // cursor column
    int markY;       // cursor line, absolute
    int markStartX;  // selection start
    int markStartY;
    qboolean swallowCtrlC;  // the copy key's char event must not clear the input line

    // [ioquakelive] links
    conLink_t links[CON_MAX_LINKS];
    int numLinks;                 // filled while drawing, read by a click
    float mouseX, mouseY;         // the system pointer, in screen pixels
    qboolean mouseSeen;
    char linkAsked[CON_MAX_URL];  // the link the prompt is asking about
    vec4_t promptOpen;            // x, y, w, h of each answer
    vec4_t promptCancel;
} console_t;

console_t con;

cvar_t* con_background;
cvar_t* con_height;
cvar_t* con_matchlimit;
cvar_t* con_noprint;
cvar_t* con_opacity;
cvar_t* con_scale;
cvar_t* con_speed;
cvar_t* con_timestamps;

// [CNQ3] Console features ported from CNQ3 (code/client/cl_console.cpp).
// con_style 0 is Quake Live's console; con_style 1 adds CNQ3's panel look,
// shadows, colours and help panel, drawn with Quake Live's own font and a
// Quake Live palette. Mark mode and console search work in both.
cvar_t* con_style;
cvar_t* con_drawHelp;
cvar_t* con_history;

cvar_t* cl_extendedColors;  // [CPMA] ^8, ^9 and ^a-^z colour codes

typedef enum {
    CC_BG,
    CC_BORDER,
    CC_ARROW,
    CC_SHADOW,
    CC_MKBG,
    CC_MKSHADOW,
    CC_TEXT,
    CC_MKTEXT,
    CC_CVAR,
    CC_CMD,
    CC_VALUE,
    CC_HELP,
    CC_SEARCH,
    CC_HL,
    CC_COUNT
} conColorIndex_t;

typedef struct {
    const char* name;
    const char* defaultValue;
    qboolean hasAlpha;
    const char* help;
    cvar_t* cvar;
    int modificationCount;
    vec4_t color;
} conColor_t;

// a Quake Live palette: black, reds and ambers, and QuakeClub's green around it
static conColor_t con_colors[CC_COUNT] = {
    {"con_colBG", "0B0B0DEB", qtrue, "RGBA color of the background"},
    {"con_colBorder", "16A34AFF", qtrue, "RGBA color of the border"},
    {"con_colArrow", "E03C3CFF", qtrue, "RGBA color of backscroll arrows"},
    {"con_colShadow", "000000FF", qtrue, "RGBA color of text shadows"},
    {"con_colMkBG", "D8D8D8FF", qtrue, "RGBA color of the mark background"},
    {"con_colMkShadow", "FFFFFF00", qtrue, "RGBA color of the mark text shadows"},
    {"con_colText", "EAEAEA", qfalse, "RGB color of text"},
    {"con_colMkText", "000000", qfalse, "RGB color of mark text"},
    {"con_colCVar", "FF9F1C", qfalse, "RGB color of variable names"},
    {"con_colCmd", "F25C54", qfalse, "RGB color of command names"},
    {"con_colValue", "FFE66D", qfalse, "RGB color of variable values"},
    {"con_colHelp", "A7A7A7", qfalse, "RGB color of help text"},
    {"con_colSearch", "FF9F1C", qfalse, "RGB color of search result marker"},
    {"con_colHL", "5A1A1AFF", qtrue, "RGBA color of auto-completion highlights\n"
                                     "This requires " S_COLOR_CVAR "con_completionStyle " S_COLOR_VAL "1" S_COLOR_HELP "."},
};

/*
================
Con_Scale

[QL] Quake Live reads con_scale->value straight in every console call.
================
*/
static float Con_Scale(void) {
    return con_scale ? con_scale->value : 1.0f;
}

/*
================
Con_DrawStringTTF

[QL] quakelive_steam.exe 0x4bdd90: the colour when given, then DroidSansMono
at a size of cellWidth * 2.1597, the baseline yOffset pixels below y.
================
*/
static void Con_DrawStringTTF(int x, int y, const char* text, const float* color, int cellWidth, int yOffset, int flags) {
    if (!text[0]) {
        return;
    }
    if (color) {
        re.SetColor(color);
    }
    re.Font_DrawString(x, y + yOffset, text, CON_FONT_MONO, cellWidth * CON_FONT_SCALE, -1, NULL, flags);
}

/*
================
Con_DrawCharTTF

[QL] quakelive_steam.exe 0x4bdd40: one character in the current colour.
================
*/
static void Con_DrawCharTTF(int x, int y, int cellWidth, int yOffset, int ch) {
    char str[2];

    str[0] = (char)ch;
    str[1] = '\0';
    re.Font_DrawString(x, y + yOffset, str, CON_FONT_MONO, cellWidth * CON_FONT_SCALE, -1, NULL, 1);
}

/*
================
Con_UpdateColors

[CNQ3] Parses the con_col* cvars when they change. An invalid colour keeps
the default.
================
*/
static void Con_UpdateColors(void) {
    int i;

    for (i = 0; i < CC_COUNT; i++) {
        conColor_t* const c = &con_colors[i];
        const char* s;

        if (!c->cvar || c->cvar->modificationCount == c->modificationCount) {
            continue;
        }
        c->modificationCount = c->cvar->modificationCount;

        s = c->cvar->string;
        if (!Com_IsValidHexColor(s, c->hasAlpha)) {
            s = c->cvar->resetString;
            if (!Com_IsValidHexColor(s, c->hasAlpha)) {
                continue;
            }
        }
        Com_ParseHexColor(c->color, s, c->hasAlpha);
    }
}

/*
================
Con_ColorFromCode

[CNQ3] Colour of a text buffer code in con_style 1: white is con_colText and
the console-only codes have their own cvars.
================
*/
static const float* Con_ColorFromCode(int code) {
    switch (code) {
        case CON_COLOR_CVAR:
            return con_colors[CC_CVAR].color;
        case CON_COLOR_CMD:
            return con_colors[CC_CMD].color;
        case CON_COLOR_VAL:
            return con_colors[CC_VALUE].color;
        case CON_COLOR_HELP:
            return con_colors[CC_HELP].color;
        default:
            break;
    }

    if (code == ColorIndex(COLOR_WHITE)) {
        return con_colors[CC_TEXT].color;
    }
    return Q_ColorFromCodeIndex(code);
}

/*
================
Con_ExtendedColors / Con_ColorCode

[CPMA] cl_extendedColors 1 adds CPMA's ^8, ^9 and ^a-^z to Quake Live's
^0-^7. Con_ColorCode returns the colour code index of "^x" at p, or -1.
================
*/
static qboolean Con_ExtendedColors(void) {
    return (cl_extendedColors && cl_extendedColors->integer) ? qtrue : qfalse;
}

static int Con_ColorCode(const char* p) {
    if (p[0] != Q_COLOR_ESCAPE || p[1] == '\0') {
        return -1;
    }
    return Q_ColorCodeIndex((unsigned char)p[1], Con_ExtendedColors());
}

/*
================
Con_RetailColor

[CNQ3] Colour index of a text buffer code in con_style 0: Quake Live has no
console-only colours, so they are white there.
================
*/
static int Con_RetailColor(int code) {
    if (code >= Q_NUM_COLOR_CODES || (code >= 8 && !Con_ExtendedColors())) {
        return ColorIndex(COLOR_WHITE);
    }
    return code;
}

/*
================
Con_FillRectPx

[CNQ3] Native pixel coordinates.
================
*/
static void Con_FillRectPx(float x, float y, float w, float h, const float* color) {
    re.SetColor(color);
    re.DrawStretchPic(x, y, w, h, 0, 0, 0, 0, cls.whiteShader);
}

/*
================
Con_GetMetrics

[CNQ3] Pixel metrics of the console font for the mark, the search marker,
the completion highlight and the help panel, which need column positions:
Quake Live's cells, plus the font's real advance and ascender measured by the
renderer.
================
*/
typedef struct {
    float cellW, cellH;  // Quake Live's cell
    float fontSize;      // cellW * 2.1597
    float adv;           // pixels per column
    float asc;           // baseline to the top of the row
} conMetrics_t;

static void Con_GetMetrics(conMetrics_t* m) {
    float one[5], two[5];
    const float s = Con_Scale();

    m->cellW = CON_CELL_WIDTH * s;
    m->cellH = CON_CELL_HEIGHT * s;
    m->fontSize = m->cellW * CON_FONT_SCALE;

    re.TextBounds("00", 0, CON_FONT_MONO, m->fontSize, 2, two);
    re.TextBounds("0", 0, CON_FONT_MONO, m->fontSize, 1, one);
    m->adv = two[2] - one[2];
    if (m->adv < 1.0f) {
        m->adv = m->cellW;
    }
    m->asc = one[4] > 0.0f ? one[4] : m->cellH * 0.8f;
}

/*
===============================================================================

[ioquakelive] LINKS IN THE CONSOLE

Retail has no links: its front end was a browser and its console was only a
console. Here an https link printed to the console is underlined and can be
clicked, and the click asks before leaving the game, because the address goes to
the system's browser and the line it came from may have been written by a server.

Only https is marked, which is all CL_OpenURL opens. The rows are monospace, so
a column is one advance wide and a link's rectangle is its first column, its
width in columns and the row's height.

===============================================================================
*/

// what a link may carry once "https://" has been seen
static qboolean Con_LinkChar(int c) {
    return (c > ' ' && c < 127 && c != '"' && c != '\'' && c != '\\' && c != '<' && c != '>') ? qtrue : qfalse;
}

/*
================
Con_LinkLength

How many columns from the given one are a link, or 0. Trailing punctuation is
left out, so a link at the end of a sentence keeps the full stop out of the
address.
================
*/
static int Con_LinkLength(const short* text, int width, int start) {
    static const char scheme[] = "https://";
    int i, len;

    for (i = 0; scheme[i]; i++) {
        if (start + i >= width || (text[start + i] & 0xff) != scheme[i]) {
            return 0;
        }
    }
    len = i;
    while (start + len < width && Con_LinkChar(text[start + len] & 0xff)) {
        len++;
    }
    while (len > i && strchr(".,;:!?)]}", text[start + len - 1] & 0xff) != NULL) {
        len--;
    }
    return len > i ? len : 0;
}

/*
================
Con_RowLinks

Underlines the links of one drawn row and remembers where they landed. x0 is the
row's left edge and y its baseline, both in pixels.
================
*/
static void Con_RowLinks(const short* text, int width, float x0, float y, float adv, float asc, float cellH) {
    static const vec4_t underline = {0.86f, 0.86f, 0.90f, 0.90f};
    int x, len, i;

    for (x = 0; x < width; x++) {
        len = Con_LinkLength(text, width, x);
        if (!len) {
            continue;
        }
        if (con.numLinks < CON_MAX_LINKS) {
            conLink_t* l = &con.links[con.numLinks++];

            l->x = x0 + x * adv;
            l->y = y - asc;
            l->w = len * adv;
            l->h = cellH;
            for (i = 0; i < len && i < CON_MAX_URL - 1; i++) {
                l->url[i] = (char)(text[x + i] & 0xff);
            }
            l->url[i] = '\0';
        }
        Con_FillRectPx(x0 + x * adv, y + 2.0f, len * adv, 1.0f, underline);
        x += len - 1;
    }
}

/*
================
Con_MousePosition

Where the system pointer is while the console is down. The console does not grab
the mouse, so the arrow on screen is the system's and this is where it is.
================
*/
void Con_MousePosition(int x, int y) {
    con.mouseX = (float)x;
    con.mouseY = (float)y;
    con.mouseSeen = qtrue;
}

static void Con_SetRect(vec4_t r, float x, float y, float w, float h) {
    r[0] = x;
    r[1] = y;
    r[2] = w;
    r[3] = h;
}

static qboolean Con_MouseInRect(float x, float y, float w, float h) {
    return (con.mouseSeen && con.mouseX >= x && con.mouseX < x + w &&
            con.mouseY >= y && con.mouseY < y + h)
               ? qtrue
               : qfalse;
}

qboolean Con_LinkPromptActive(void) {
    return con.linkAsked[0] ? qtrue : qfalse;
}

static void Con_OpenAskedLink(void) {
    char url[CON_MAX_URL];

    Q_strncpyz(url, con.linkAsked, sizeof(url));
    con.linkAsked[0] = '\0';
    CL_OpenURL(url);
}

/*
================
Con_LinkPromptKey

The prompt takes every key while it is up: enter or a click on Open leaves the
game, anything else closes it.
================
*/
qboolean Con_LinkPromptKey(int key) {
    if (!Con_LinkPromptActive()) {
        return qfalse;
    }
    if (key == K_ENTER || key == K_KP_ENTER || key == 'y') {
        Con_OpenAskedLink();
    } else if (key == K_MOUSE1) {
        if (Con_MouseInRect(con.promptOpen[0], con.promptOpen[1], con.promptOpen[2], con.promptOpen[3])) {
            Con_OpenAskedLink();
        } else if (Con_MouseInRect(con.promptCancel[0], con.promptCancel[1], con.promptCancel[2], con.promptCancel[3])) {
            con.linkAsked[0] = '\0';
        }
    } else {
        con.linkAsked[0] = '\0';
    }
    return qtrue;
}

/*
================
Con_MouseClick

A click on a link raises the prompt. Nothing opens from here.
================
*/
qboolean Con_MouseClick(void) {
    int i;

    for (i = 0; i < con.numLinks; i++) {
        const conLink_t* l = &con.links[i];

        if (Con_MouseInRect(l->x, l->y, l->w, l->h)) {
            Q_strncpyz(con.linkAsked, l->url, sizeof(con.linkAsked));
            return qtrue;
        }
    }
    return qfalse;
}

/*
================
Con_DrawLinkPrompt

The question a clicked link opens, over the console: the address, and the two
answers. The one under the pointer is lit.
================
*/
static void Con_DrawLinkPrompt(void) {
    static const vec4_t dim = {0.0f, 0.0f, 0.0f, 0.55f};
    conMetrics_t m;
    float w, h, x, y, row, bw, bh, by;
    int i;

    if (!Con_LinkPromptActive()) {
        return;
    }
    Con_GetMetrics(&m);

    row = m.cellH;
    w = cls.glconfig.vidWidth * 0.72f;
    h = row * 7.0f;
    x = (cls.glconfig.vidWidth - w) * 0.5f;
    y = (cls.glconfig.vidHeight - h) * 0.5f;

    Con_FillRectPx(0, 0, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight, dim);
    Con_FillRectPx(x, y, w, h, con_colors[CC_BG].color);
    Con_FillRectPx(x, y, w, 2.0f, con_colors[CC_BORDER].color);
    Con_FillRectPx(x, y + h - 2.0f, w, 2.0f, con_colors[CC_BORDER].color);

    re.SetColor(con_colors[CC_TEXT].color);
    re.Font_DrawString((int)(x + m.adv), (int)(y + row * 1.6f), "You are about to leave the game and open this link.",
                       CON_FONT_MONO, m.fontSize, -1, NULL, CON_TEXT_LITERAL);
    re.SetColor(con_colors[CC_VALUE].color);
    re.Font_DrawString((int)(x + m.adv), (int)(y + row * 3.0f), con.linkAsked, CON_FONT_MONO, m.fontSize,
                       (int)((w - m.adv * 2.0f) / m.adv), NULL, CON_TEXT_LITERAL);

    bw = m.adv * 10.0f;
    bh = row * 1.2f;
    by = y + h - row * 2.0f;
    Con_SetRect(con.promptOpen, x + w - bw * 2.0f - m.adv * 3.0f, by, bw, bh);
    Con_SetRect(con.promptCancel, x + w - bw - m.adv, by, bw, bh);

    for (i = 0; i < 2; i++) {
        const float* r = i ? con.promptCancel : con.promptOpen;
        const char* label = i ? "  CANCEL" : "   OPEN";
        const qboolean lit = Con_MouseInRect(r[0], r[1], r[2], r[3]);

        Con_FillRectPx(r[0], r[1], r[2], r[3], lit ? con_colors[CC_HL].color : con_colors[CC_MKBG].color);
        re.SetColor(lit ? con_colors[CC_MKTEXT].color : con_colors[CC_TEXT].color);
        re.Font_DrawString((int)r[0], (int)(r[1] + m.asc + (bh - m.cellH) * 0.5f), label, CON_FONT_MONO, m.fontSize,
                           -1, NULL, CON_TEXT_LITERAL);
    }

    re.SetColor(con_colors[CC_HELP].color);
    re.Font_DrawString((int)(x + m.adv), (int)(by + m.asc + (bh - m.cellH) * 0.5f), "enter opens it, escape cancels",
                       CON_FONT_MONO, m.fontSize, -1, NULL, CON_TEXT_LITERAL);
    re.SetColor(NULL);
}

/*
================
Con_GetStyle / Con_GetLineWidth
================
*/
int Con_GetStyle(void) {
    return (con_style && con_style->integer == 1) ? 1 : 0;
}

int Con_GetLineWidth(void) {
    return con.linewidth;
}

/*
================
[CNQ3] Line helpers

Lines are absolute numbers, the text buffer row of line L is L % totallines.
================
*/
// oldest line still in the buffer
static int Con_FirstLine(void) {
    const int first = con.current - con.totallines + 1;
    return first > 0 ? first : 0;
}

// line drawn at the bottom of the console
static int Con_BottomRow(void) {
    return con.x == 0 ? con.display - 1 : con.display;
}

// text of a line without its colours and trailing spaces
static void Con_GetLineText(int line, char* out, int outSize) {
    const short* const text = con.text + (line % con.totallines) * con.linewidth;
    int i, n;

    n = con.linewidth < outSize - 1 ? con.linewidth : outSize - 1;
    for (i = 0; i < n; i++) {
        out[i] = text[i] & 0xff;
    }
    out[n] = '\0';

    while (n > 0 && out[n - 1] == ' ') {
        out[--n] = '\0';
    }
}

static void Con_FixPosition(void) {
    int minDisplay = con.current - con.totallines + 1;

    if (minDisplay < 1) {
        minDisplay = 1;
    }
    if (con.display < minDisplay) {
        con.display = minDisplay;
    }
    if (con.display > con.current) {
        con.display = con.current;
    }
}

/*
================
Con_ToggleConsole_Forced

Opens or closes the console whatever com_allowConsole says. Shift+Escape comes through
here so that turning the console off in the options can never lock a player out of it.
================
*/
void Con_ToggleConsole_Forced(void) {
    // Can't toggle the console when it's the only thing available
    if (clc.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE) {
        return;
    }

    Field_ResetCompletionCycle();  // [CNQ3]
    Field_Clear(&g_consoleField);

    g_consoleField.widthInChars = g_console_field_width;

    Con_ClearNotify();
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_CONSOLE);
    con.markMode = qfalse;  // [CNQ3]
}

/*
================
Con_ToggleConsole_f

[QL] quakelive_steam.exe 0x4b49d0: the command only goes through with com_allowConsole
at 1; anything else prints this line and stops.
================
*/
void Con_ToggleConsole_f(void) {
    if (com_allowConsole && com_allowConsole->integer != 1) {
        Com_Printf("com_allowConsole won't allow toggleconsole command\n");
        return;
    }

    Con_ToggleConsole_Forced();
}

/*
===================
Con_ToggleMenu_f
===================
*/
void Con_ToggleMenu_f(void) {
    CL_KeyEvent(K_ESCAPE, qtrue, Sys_Milliseconds());
    CL_KeyEvent(K_ESCAPE, qfalse, Sys_Milliseconds());
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f(void) {
    chat_playerNum = -1;
    chat_team = qfalse;
    Field_Clear(&chatField);
    chatField.widthInChars = 30;

    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f(void) {
    chat_playerNum = -1;
    chat_team = qtrue;
    Field_Clear(&chatField);
    chatField.widthInChars = 25;
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f(void) {
    chat_playerNum = VM_Call(cgvm, CG_CROSSHAIR_PLAYER);
    if (chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS) {
        chat_playerNum = -1;
        return;
    }
    chat_team = qfalse;
    Field_Clear(&chatField);
    chatField.widthInChars = 30;
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

/*
================
Con_MessageMode4_f
================
*/
void Con_MessageMode4_f(void) {
    chat_playerNum = VM_Call(cgvm, CG_LAST_ATTACKER);
    if (chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS) {
        chat_playerNum = -1;
        return;
    }
    chat_team = qfalse;
    Field_Clear(&chatField);
    chatField.widthInChars = 30;
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f(void) {
    int i;

    for (i = 0; i < CON_TEXTSIZE; i++) {
        con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
    }

    Con_Bottom();  // go to end

    // [CNQ3] nothing left to search or mark
    con.searchStarted = qfalse;
    con.markMode = qfalse;
}

/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f(void) {
    int l, x, i;
    short* line;
    fileHandle_t f;
    int bufferlen;
    char* buffer;
    char filename[MAX_QPATH];

    if (Cmd_Argc() != 2) {
        Com_Printf("usage: condump <filename>\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
    COM_DefaultExtension(filename, sizeof(filename), ".txt");

    if (!COM_CompareExtension(filename, ".txt")) {
        Com_Printf("Con_Dump_f: Only the \".txt\" extension is supported by this command!\n");
        return;
    }

    f = FS_FOpenFileWrite(filename);
    if (!f) {
        Com_Printf("ERROR: couldn't open %s.\n", filename);
        return;
    }

    Com_Printf("Dumped console text to %s.\n", filename);

    // skip empty lines
    for (l = con.current - con.totallines + 1; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;
        for (x = 0; x < con.linewidth; x++)
            if ((line[x] & 0xff) != ' ')
                break;
        if (x != con.linewidth)
            break;
    }

#ifdef _WIN32
    bufferlen = con.linewidth + 3 * sizeof(char);
#else
    bufferlen = con.linewidth + 2 * sizeof(char);
#endif

    buffer = Hunk_AllocateTempMemory(bufferlen);

    // write the remaining lines
    buffer[bufferlen - 1] = 0;
    for (; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;
        for (i = 0; i < con.linewidth; i++)
            buffer[i] = line[i] & 0xff;
        for (x = con.linewidth - 1; x >= 0; x--) {
            if (buffer[x] == ' ')
                buffer[x] = 0;
            else
                break;
        }
#ifdef _WIN32
        Q_strcat(buffer, bufferlen, "\r\n");
#else
        Q_strcat(buffer, bufferlen, "\n");
#endif
        FS_Write(buffer, strlen(buffer), f);
    }

    Hunk_FreeTempMemory(buffer);
    FS_FCloseFile(f);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify(void) {
    int i;

    for (i = 0; i < NUM_CON_TIMES; i++) {
        con.times[i] = 0;
    }
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize(void) {
    int i, j, width, oldwidth, oldtotallines, numlines, numchars;
    short tbuf[CON_TEXTSIZE];

    // [QL] quakelive_steam.exe 0x4b3a50: 12 pixel cells scaled by con_scale,
    // one cell of margin on each side
    con.xadjust = 0;
    if (con_scale) {
        width = (int)(cls.glconfig.vidWidth / con_scale->value / (double)CON_CELL_WIDTH - 2.0);
    } else {
        width = cls.glconfig.vidWidth / CON_CELL_WIDTH - 2;
    }
    if (width > CON_MAXLINEWIDTH) {
        width = CON_MAXLINEWIDTH;  // [CNQ3] scratch buffers
    }
    if (width < 1) {
        width = 78;  // fallback before video init
    }

    if (width == con.linewidth)
        return;

    if (width < 1)  // video hasn't been initialized yet
    {
        con.linewidth = width;
        con.totallines = CON_TEXTSIZE / con.linewidth;
        for (i = 0; i < CON_TEXTSIZE; i++)

            con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
    } else {
        oldwidth = con.linewidth;
        con.linewidth = width;
        oldtotallines = con.totallines;
        con.totallines = CON_TEXTSIZE / con.linewidth;
        numlines = oldtotallines;

        if (con.totallines < numlines)
            numlines = con.totallines;

        numchars = oldwidth;

        if (con.linewidth < numchars)
            numchars = con.linewidth;

        Com_Memcpy(tbuf, con.text, CON_TEXTSIZE * sizeof(short));
        for (i = 0; i < CON_TEXTSIZE; i++)

            con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';

        for (i = 0; i < numlines; i++) {
            for (j = 0; j < numchars; j++) {
                con.text[(con.totallines - 1 - i) * con.linewidth + j] =
                    tbuf[((con.current - i + oldtotallines) %
                          oldtotallines) *
                             oldwidth +
                         j];
            }
        }

        Con_ClearNotify();
    }

    con.current = con.totallines - 1;
    con.display = con.current;

    // [CNQ3] the lines were renumbered
    con.searchStarted = qfalse;
    con.markMode = qfalse;
}

/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName(char* args, int argNum) {
    if (argNum == 2) {
        Field_CompleteFilename("", "txt", qfalse, qtrue);
    }
}

/*
================
[QL] Con_Find_f - search console buffer for a substring
================
*/
static void Con_Find_f(void) {
    char* search;
    int l, x, i;
    short* line;
    char text[1024];
    int matchCount = 0;
    int matchLimit;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: find <string>\n");
        return;
    }

    search = Cmd_Argv(1);

    if (!con.initialized || !search[0]) {
        return;
    }

    matchLimit = (con_matchlimit && con_matchlimit->integer > 0) ? con_matchlimit->integer : 16;

    // skip empty lines at the start
    for (l = con.current - con.totallines + 1; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;
        for (x = 0; x < con.linewidth; x++) {
            if ((line[x] & 0xff) != ' ')
                break;
        }
        if (x != con.linewidth)
            break;
    }

    for (; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;

        // Extract text from this line
        x = 0;
        for (i = 0; i < con.linewidth && x < (int)sizeof(text) - 1; i++) {
            text[x++] = line[i] & 0xff;
        }
        text[x] = '\0';

        // Strip trailing spaces
        while (x > 0 && text[x - 1] == ' ') {
            text[--x] = '\0';
        }

        // [QL] Search with match limiting and output filtering
        if (text[0] && strstr(text, search) &&
            !strstr(text, "\\find") && !strstr(text, "\\##")) {
            matchCount++;
            if (matchCount <= matchLimit) {
                if (matchCount == 1) {
                    Com_Printf("\\## MATCH LIST:\n");
                }
                Com_Printf("\\## %s\n", text);
            }
        }
    }

    if (matchCount == 0) {
        Com_Printf("%d matches found.\n", 0);
    } else if (matchCount >= matchLimit) {
        Com_Printf("%d matches found. (Displaying the first %d matches.)\n", matchCount, matchLimit);
    } else {
        Com_Printf("%d %s found.\n", matchCount, matchCount < 2 ? "match" : "matches");
    }
}

/*
================
Con_BeginSearch_f

[CNQ3] searchconsole <pattern>
================
*/
static void Con_BeginSearch_f(void) {
    if (Cmd_Argc() != 2) {
        Com_Printf("usage: %s <pattern>\n", Cmd_Argv(0));
        return;
    }

    Q_strncpyz(con.searchPattern, Cmd_Argv(1), sizeof(con.searchPattern));
    con.searchLine = con.current;
    con.searchStarted = qtrue;
    Con_ContinueSearch(qtrue);
}

/*
================
Con_ContinueSearch

[CNQ3] Finds the next line matching the pattern, going up towards the older
lines when forward and down otherwise, wrapping around. As with Com_Filter the
pattern matches the start of the line and '*' matches anything.
================
*/
void Con_ContinueSearch(qboolean forward) {
    char rawText[CON_MAXLINEWIDTH + 1];
    int first, count, k;

    if (!con.searchStarted || !con.initialized) {
        return;
    }

    first = Con_FirstLine();
    count = con.current - first + 1;
    if (con.searchLine < first || con.searchLine > con.current) {
        con.searchLine = con.current;
    }

    for (k = 1; k <= count; k++) {
        int line = forward ? con.searchLine - k : con.searchLine + k;

        if (line < first) {
            line += count;
        } else if (line > con.current) {
            line -= count;
        }

        Con_GetLineText(line, rawText, sizeof(rawText));
        if (!rawText[0]) {
            continue;
        }

        // ignore the searchconsole calls
        if (rawText[0] == ']' && (rawText[1] == '/' || rawText[1] == '\\') &&
            !Q_stricmpn(rawText + 2, "searchconsole ", 14)) {
            continue;
        }

        if (Com_Filter(con.searchPattern, rawText, qfalse)) {
            const int bottom = Con_BottomRow();

            con.searchLine = line;
            // scroll only when the match isn't on screen
            if (line > bottom || line <= bottom - con.rowsVisible) {
                con.display = line + (con.x == 0 ? 1 : 0);
                Con_FixPosition();
            }
            return;
        }
    }
}

/*
================
Con_ColorList_f

[CPMA] colorlist: every colour code, drawn in its colour.
================
*/
static void Con_ColorList_f(void) {
    const int count = Con_ExtendedColors() ? Q_NUM_COLOR_CODES : 8;
    char line[128];
    int i;

    line[0] = '\0';
    for (i = 0; i < count; i++) {
        const int c = Q_ColorCodeChar(i);
        Q_strcat(line, sizeof(line), va("%c%c%c ", Q_COLOR_ESCAPE, c, c));
        if (i % 12 == 11 || i == count - 1) {
            Com_Printf("%s\n", line);
            line[0] = '\0';
        }
    }

    if (count == 8) {
        Com_Printf(S_COLOR_WHITE "Set " S_COLOR_CVAR "cl_extendedColors " S_COLOR_VAL "1" S_COLOR_WHITE " for 8, 9 and a to z.\n");
    }
}

/*
================
Con_Init
================
*/
void Con_Init(void) {
    int i;

    con_background = Cvar_Get("con_background", "0", CVAR_ARCHIVE);
    Cvar_CheckRange(con_background, 0, 1, qtrue);

    con_height = Cvar_Get("con_height", "0.5", CVAR_ARCHIVE);
    Cvar_CheckRange(con_height, 0.1f, 1.0f, qfalse);

    con_matchlimit = Cvar_Get("con_matchlimit", "16", 0);

    con_noprint = Cvar_Get("con_noprint", "0", 0);

    con_opacity = Cvar_Get("con_opacity", "0.9", CVAR_ARCHIVE);
    Cvar_CheckRange(con_opacity, 0.1f, 1.0f, qfalse);

    con_scale = Cvar_Get("con_scale", "0.5", CVAR_ARCHIVE);
    Cvar_CheckRange(con_scale, 0.5f, 1.0f, qfalse);

    con_speed = Cvar_Get("con_speed", "3", CVAR_ARCHIVE);
    Cvar_CheckRange(con_speed, 0.1f, 1000.0f, qfalse);

    con_timestamps = Cvar_Get("con_timestamps", "0", CVAR_ARCHIVE);

    // [CNQ3] console style; 1 by default at the user's request, 0 keeps Quake Live's console
    con_style = Cvar_Get("con_style", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_style, 0, 1, qtrue);

    con_drawHelp = Cvar_Get("con_drawHelp", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_drawHelp, 0, DRAWHELP_MAX, qtrue);

    con_history = Cvar_Get("con_history", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_history, 0, 1, qtrue);

    // [CPMA] extended colour codes, always on at the user's request; CVAR_ROM also
    // overrides an archived "0" left by an older build
    cl_extendedColors = Cvar_Get("cl_extendedColors", "1", CVAR_ROM);
    Cvar_SetHelp("cl_extendedColors", "colour codes in text: Quake Live's 0 to 7 plus CPMA's 8, 9 and a to z, always on\n"
                 "The list in their colours: " S_COLOR_CMD "colorlist");

    for (i = 0; i < CC_COUNT; i++) {
        conColor_t* const c = &con_colors[i];

        Com_ParseHexColor(c->color, c->defaultValue, c->hasAlpha);
        c->cvar = Cvar_Get(c->name, c->defaultValue, CVAR_ARCHIVE);
        c->modificationCount = -1;
        Cvar_SetHelp(c->name, c->help);
    }
    // [ioquakelive] the border was red up to 0.2.1; a config that kept that default follows the new one
    if (!Q_stricmp(con_colors[CC_BORDER].cvar->string, "C01A1AFF")) {
        Cvar_Set(con_colors[CC_BORDER].name, con_colors[CC_BORDER].defaultValue);
    }
    Con_UpdateColors();

    Cvar_SetHelp("con_style", "console style\n"
                 S_COLOR_VAL "    0 " S_COLOR_HELP "= Quake Live\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= Panel with shadows, the con_col* colors, help panel and faster scrolling");
    Cvar_SetHelp("con_drawHelp", "draws help text below the console\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= Enables the help panel below the console\n"
                 S_COLOR_VAL "    2 " S_COLOR_HELP "= Draws the help panel even if the cvar/cmd has no help text\n"
                 S_COLOR_VAL "    4 " S_COLOR_HELP "= Draws the module\n"
                 S_COLOR_VAL "    8 " S_COLOR_HELP "= Draws the list of attributes (cvars only)\n"
                 "Only used by " S_COLOR_CVAR "con_style " S_COLOR_VAL "1" S_COLOR_HELP ".");
    Cvar_SetHelp("con_history", "writes the command history to consolehistory.txt");
    Cvar_SetHelp("con_background", "console background\n"
                 S_COLOR_VAL "    0 " S_COLOR_HELP "= Black, with the " S_COLOR_CVAR "con_opacity" S_COLOR_HELP " opacity\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= Console image\n"
                 "Only used by " S_COLOR_CVAR "con_style " S_COLOR_VAL "0" S_COLOR_HELP ".");
    Cvar_SetHelp("con_height", "console height, as a fraction of the screen");
    Cvar_SetHelp("con_matchlimit", "max. number of matches printed by " S_COLOR_CMD "find");
    Cvar_SetHelp("con_noprint", "disables console printing");
    Cvar_SetHelp("con_opacity", "opacity of the black console background\n"
                 "Only used by " S_COLOR_CVAR "con_style " S_COLOR_VAL "0" S_COLOR_HELP ".");
    Cvar_SetHelp("con_scale", "console text scale");
    Cvar_SetHelp("con_speed", "console opening/closing speed");
    Cvar_SetHelp("con_timestamps", "prefixes new console lines with the time");

    Field_Clear(&g_consoleField);
    g_consoleField.widthInChars = g_console_field_width;
    for (i = 0; i < COMMAND_HISTORY; i++) {
        Field_Clear(&historyEditLines[i]);
        historyEditLines[i].widthInChars = g_console_field_width;
    }
    CL_LoadConsoleHistory();

    Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
    Cmd_AddCommand("messagemode", Con_MessageMode_f);
    Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
    Cmd_AddCommand("clear", Con_Clear_f);
    Cmd_AddCommand("condump", Con_Dump_f);
    Cmd_SetCommandCompletionFunc("condump", Cmd_CompleteTxtName);

    // [QL] console search
    Cmd_AddCommand("find", Con_Find_f);

    // [CNQ3] console search with scrolling, ctrl-F and (shift-)F3
    Cmd_AddCommand("searchconsole", Con_BeginSearch_f);

    // [CPMA]
    Cmd_AddCommand("colorlist", Con_ColorList_f);
    Cmd_SetHelp("colorlist", "prints every colour code in its colour\n"
                "See " S_COLOR_CVAR "cl_extendedColors" S_COLOR_HELP ".");

    Cmd_SetHelp("toggleconsole", "toggles console display");
    Cmd_SetHelp("messagemode", "chat with everyone");
    Cmd_SetHelp("messagemode2", "chat with teammates");
    Cmd_SetHelp("clear", "clears the console");
    Cmd_SetHelp("condump", "dumps the console text to a file\n"
                "Usage: " S_COLOR_CMD "condump " S_COLOR_VAL "<filename>");
    Cmd_SetHelp("find", "prints the console lines containing a text\n"
                "Usage: " S_COLOR_CMD "find " S_COLOR_VAL "<text>\n" S_COLOR_HELP
                "At most " S_COLOR_CVAR "con_matchlimit" S_COLOR_HELP " matches are printed.");
    Cmd_SetHelp("searchconsole", "begins a new console search\n"
                "Press ctrl-F when the console is down to bring up the command.\n"
                "Press (shift-)F3 to find the next match going up or down.\n"
                "The star character (*) will match 0, 1 or more characters.\n"
                "\n"
                "         Argument   Matches\n"
                "Example: init       lines starting with 'init'\n"
                "Example: *init      lines containing 'init'\n"
                "Example: >*net      lines starting with '>' AND containing 'net'");
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown(void) {
    Cmd_RemoveCommand("toggleconsole");
    Cmd_RemoveCommand("messagemode");
    Cmd_RemoveCommand("messagemode2");
    Cmd_RemoveCommand("clear");
    Cmd_RemoveCommand("condump");
    Cmd_RemoveCommand("find");
    Cmd_RemoveCommand("searchconsole");
    Cmd_RemoveCommand("colorlist");
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed(qboolean skipnotify) {
    int i;

    // mark time for transparent overlay
    if (con.current >= 0) {
        if (skipnotify)
            con.times[con.current % NUM_CON_TIMES] = 0;
        else
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
    }

    con.x = 0;
    if (con.display == con.current)
        con.display++;
    con.current++;
    for (i = 0; i < con.linewidth; i++)
        con.text[(con.current % con.totallines) * con.linewidth + i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint(char* txt) {
    int y, l;
    unsigned char c;
    unsigned short color;
    qboolean skipnotify = qfalse;  // NERVE - SMF
    int prev;                      // NERVE - SMF

    // TTimo - prefix for text that shows up in console but not in notify
    // backported from RTCW
    if (!Q_strncmp(txt, "[skipnotify]", 12)) {
        skipnotify = qtrue;
        txt += 12;
    }

    // [QL] con_noprint suppresses all console output
    if (con_noprint && con_noprint->integer) {
        return;
    }

    if (!con.initialized) {
        con.color[0] =
            con.color[1] =
                con.color[2] =
                    con.color[3] = 1.0f;
        con.linewidth = -1;
        Con_CheckResize();
        con.initialized = qtrue;
    }

    // [QL] prepend timestamp at start of line
    if (con_timestamps && con_timestamps->integer && con.x == 0) {
        char tsbuf[64];
        int time_ms;
        float time_sec;
        int minutes, seconds, millis;
        char *p;

        time_ms = cls.realtime;
        time_sec = time_ms / 1000.0f;
        minutes = (int)time_sec / 60;
        seconds = (int)(time_sec - minutes * 60.0f);
        millis = (int)((time_sec - minutes * 60.0f - seconds) * 1000.0f);
        Com_sprintf(tsbuf, sizeof(tsbuf), "[%d:%02d.%03d] ", minutes, seconds, millis);

        p = tsbuf;
        while (*p) {
            y = con.current % con.totallines;
            con.text[y * con.linewidth + con.x] = (ColorIndex(COLOR_WHITE) << 8) | *p;
            con.x++;
            p++;
        }
    }

    color = ColorIndex(COLOR_WHITE);

    while ((c = *((unsigned char*)txt)) != 0) {
        if (Q_IsConsoleColorString(txt)) {
            // [CNQ3] cvar, command, value and help colours
            color = CON_COLOR_CVAR + (txt[1] - COLOR_CON_CVAR);
            txt += 2;
            continue;
        }

        // [QL] only ^0-^7 are colour codes (quakelive_steam.exe 0x4b3e90), '^'
        // before anything else is printed; [CPMA] cl_extendedColors adds more
        if (Con_ColorCode(txt) >= 0) {
            color = Con_ColorCode(txt);
            txt += 2;
            continue;
        }

        // count word length
        for (l = 0; l < con.linewidth; l++) {
            if (txt[l] <= ' ') {
                break;
            }
        }

        // word wrap
        if (l != con.linewidth && (con.x + l >= con.linewidth)) {
            Con_Linefeed(skipnotify);
        }

        txt++;

        switch (c) {
            case '\n':
                Con_Linefeed(skipnotify);
                break;
            case '\r':
                con.x = 0;
                break;
            default:  // display character and advance
                y = con.current % con.totallines;
                con.text[y * con.linewidth + con.x] = (color << 8) | c;
                con.x++;
                if (con.x >= con.linewidth)
                    Con_Linefeed(skipnotify);
                break;
        }
    }

    // mark time for transparent overlay
    if (con.current >= 0) {
        // NERVE - SMF
        if (skipnotify) {
            prev = con.current % NUM_CON_TIMES - 1;
            if (prev < 0)
                prev = NUM_CON_TIMES - 1;
            con.times[prev] = 0;
        } else
            // -NERVE - SMF
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
    }
}

/*
==============================================================================

DRAWING

==============================================================================
*/

/*
================
Con_MarkBounds

[CNQ3] The selection, and whether to draw it: a lone character blinks.
================
*/
static qboolean Con_MarkBounds(int* startX, int* startY, int* endX, int* endY) {
    *startX = MIN(con.markStartX, con.markX);
    *startY = MIN(con.markStartY, con.markY);
    *endX = MAX(con.markStartX, con.markX);
    *endY = MAX(con.markStartY, con.markY);

    if (!con.markMode) {
        return qfalse;
    }
    return (*endX != *startX || *endY != *startY || Sys_Milliseconds() % 1000 < 500) ? qtrue : qfalse;
}

/*
================
Con_DrawFieldTTF

[QL] quakelive_steam.exe 0x4b6630. The visible part is found walking back from
the end of the line one UTF-8 character at a time, dropping characters from the
end while the cursor is left of the window. The cursor is measured on the drawn
text: '|' half a cell left of the next character, '_' in overstrike mode.
================
*/
static void Con_DrawFieldTTF(field_t* edit, int x, int y, int cellWidth, int cellHeight, qboolean showCursor, const float* color) {
    const char* const buf = edit->buffer;
    char str[MAX_EDIT_LINE];
    int end = strlen(buf);
    int start = end;
    int count = 0;
    int len;

    while (start > 0 && count < edit->widthInChars) {
        if (--start <= 0) {
            break;
        }
        while (start > 0 && (buf[start] & 0xc0) == 0x80) {
            start--;
        }
        count++;
        if (count == edit->widthInChars && edit->cursor < start) {
            unsigned char removed;
            do {
                removed = (unsigned char)buf[--end];
            } while (end > 0 && (removed & 0xc0) == 0x80);
            count--;
        }
    }

    len = end - start;
    Com_Memcpy(str, buf + start, len);
    str[len] = '\0';
    Con_DrawStringTTF(x, y, str, color, cellWidth, cellHeight, 0);

    if (showCursor && !(cls.realtime & 0x100)) {
        const int off = edit->cursor - start;
        float extents[5] = {0, 0, 0, 0, 0};
        float cursorX;

        if (off > 0 && off <= len) {
            re.TextBounds(str, 0, CON_FONT_MONO, cellWidth * CON_FONT_SCALE, off, extents);
        }
        cursorX = x + extents[2] - extents[0];
        if (!key_overstrikeMode) {
            cursorX -= cellWidth / 2;
        }
        if (off >= 1 && off <= len && str[off - 1] == ' ') {
            cursorX += cellWidth;
        }
        Con_DrawCharTTF((int)cursorX, y, cellWidth, cellHeight, key_overstrikeMode ? '_' : '|');
    }
}

/*
================
Con_DrawInput

[QL] quakelive_steam.exe 0x4b3ff0: the ']' prompt and the edit line in
con_scale'd cells, two cells above the bottom of the console.
================
*/
void Con_DrawInput(void) {
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    const float s = Con_Scale();
    int y;

    if (clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    y = (int)(con.vislines - CON_CELL_HEIGHT * s * 2);

    g_consoleField.widthInChars = g_console_field_width;  // [CNQ3] con_style 1 changes it

    re.SetColor(con.color);
    Con_DrawCharTTF((int)((CON_CELL_WIDTH + con.xadjust) * s), y, (int)(CON_CELL_WIDTH * s), (int)(CON_CELL_HEIGHT * s), ']');

    Con_DrawFieldTTF(&g_consoleField, (int)((2 * CON_CELL_WIDTH + con.xadjust) * s), y,
                     (int)(CON_CELL_WIDTH * s), (int)(CON_CELL_HEIGHT * s), !con.markMode, white);
}

/*
================
Con_DrawNotify

[QL] Only the chat input field. Quake Live's engine (quakelive_steam.exe 0x4b4100)
draws the "say:" / "say team:" / "reply:" field here and nothing else: there is no loop
over recent console lines, so console output never shows over the game.

The field sits on a translucent bar near the bottom, in amber DroidSansMono at the
console's unscaled 12x24 cells. With cgame loaded the bar's y and width and the field's
width in characters come from cgame's exports 17 to 19 (0x4b416b): 413, 640 and 73, or
455, 300 and 30 during the intermission, team chat taking 5 fewer.
================
*/
void Con_DrawNotify(void) {
    static const vec4_t textColor = {1.0f, 0.8f, 0.0f, 1.0f};
    static const vec4_t barColor = {0.0f, 0.0f, 0.0f, 0.6f};
    float x = 8.0f;
    float y;
    float barY = 413.0f;
    float barWidth = SCREEN_WIDTH;
    floatint_t fi;
    const char* label;
    int skip;

    if (Key_GetCatcher() & KEYCATCH_UI) {
        return;
    }
    if (!(Key_GetCatcher() & KEYCATCH_MESSAGE)) {
        return;
    }

    if (cgvm) {
        fi.i = (int)VM_Call(cgvm, CG_CHAT_BAR_Y);
        barY = fi.f;
        fi.i = (int)VM_Call(cgvm, CG_CHAT_BAR_WIDTH);
        barWidth = fi.f;
        chatField.widthInChars = (int)VM_Call(cgvm, CG_CHAT_FIELD_CHARS);
        if (chat_team) {
            chatField.widthInChars -= 5;
        }
    }

    y = barY;
    SCR_AdjustFrom640(&x, &y, NULL, NULL);
    SCR_FillRect(6.0f, barY - 3.0f, barWidth - 12.0f, 22.0f, barColor);

    if (chat_playerNum != -1) {
        label = "reply:";
        skip = 7;
    } else if (chat_team) {
        label = "say team:";
        skip = 11;
    } else {
        label = "say:";
        skip = 6;
    }

    Con_DrawStringTTF((int)x, (int)y, label, textColor, CON_CELL_WIDTH, CON_CELL_HEIGHT, 1);
    Con_DrawFieldTTF(&chatField, (int)(CON_CELL_WIDTH * skip + con.xadjust), (int)y,
                     CON_CELL_WIDTH, CON_CELL_HEIGHT, qtrue, textColor);
}

/*
================
[CNQ3] con_style 1 text runs

A row is drawn as runs of one colour, shadow first. Columns are fixed, column
c starting at x + c * the font's advance, so the mark, the search marker and
the completion highlight line up with the text.
================
*/
#define CON_RUN_SHADOW 100
#define CON_RUN_MKSHADOW 101
#define CON_RUN_MKTEXT 102

static const conMetrics_t* con_helpMetrics;  // metrics for Con_HelpPrintf

static const float* Con_RunColor(int id) {
    switch (id) {
        case CON_RUN_SHADOW:
            return con_colors[CC_SHADOW].color;
        case CON_RUN_MKSHADOW:
            return con_colors[CC_MKSHADOW].color;
        case CON_RUN_MKTEXT:
            return con_colors[CC_MKTEXT].color;
        default:
            return Con_ColorFromCode(id);
    }
}

static void Con_DrawRuns(const conMetrics_t* m, float x, float baseline, const char* chars, const short* ids, int count, qboolean shadow) {
    char run[CON_MAXLINEWIDTH + 1];
    const int offset = shadow ? 1 : 0;
    int i = 0;

    while (i < count) {
        const float* color;
        int start, len, id;

        if (chars[i] == ' ') {
            i++;
            continue;
        }

        id = ids[i];
        start = i;
        len = 0;
        while (i < count && len < CON_MAXLINEWIDTH && (chars[i] == ' ' || ids[i] == id)) {
            run[len++] = chars[i++];
        }
        while (len > 0 && run[len - 1] == ' ') {
            len--;
        }
        run[len] = '\0';

        color = Con_RunColor(id);
        if (color[3] <= 0.0f) {
            continue;
        }
        re.SetColor(color);
        re.Font_DrawString((int)(x + start * m->adv) + offset, (int)baseline + offset, run, CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
    }
}

/*
================
Con_DrawInputCNQ3

[CNQ3] con_style 1 edit line: shadowed, colour codes shown in their colour,
the match cycled through by the completion highlighted, and Quake Live's
cursor.
================
*/
static void Con_DrawInputCNQ3(const conMetrics_t* m) {
    field_t* const edit = &g_consoleField;
    const float baseline = con.vislines - m->cellH;
    const float promptX = m->cellW;
    const float textX = promptX + m->adv;
    char chars[MAX_EDIT_LINE];
    short ids[MAX_EDIT_LINE];
    short shadows[MAX_EDIT_LINE];
    int len, drawLen, prestep, code, i;

    if (clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    edit->widthInChars = con.linewidth > 2 ? con.linewidth - 1 : 1;

    // the visible part, keeping the cursor in view like Field_VariableSizeDraw
    drawLen = edit->widthInChars - 1;
    len = strlen(edit->buffer);
    if (len <= drawLen) {
        prestep = 0;
    } else {
        if (edit->scroll + drawLen > len) {
            edit->scroll = len - drawLen;
            if (edit->scroll < 0) {
                edit->scroll = 0;
            }
        }
        prestep = edit->scroll;
    }
    if (prestep + drawLen > len) {
        drawLen = len - prestep;
    }
    if (drawLen > MAX_EDIT_LINE) {
        drawLen = MAX_EDIT_LINE;
    }

    if (edit->acLength > 0) {
        const int start = MAX(edit->acOffset, prestep);
        const int end = MIN(edit->acOffset + edit->acLength, prestep + drawLen);
        if (end > start) {
            Con_FillRectPx(textX + (start - prestep) * m->adv, baseline - m->asc, (end - start) * m->adv, m->cellH, con_colors[CC_HL].color);
        }
    }

    chars[0] = ']';
    ids[0] = ColorIndex(COLOR_WHITE);
    shadows[0] = CON_RUN_SHADOW;
    Con_DrawRuns(m, promptX, baseline, chars, shadows, 1, qtrue);
    Con_DrawRuns(m, promptX, baseline, chars, ids, 1, qfalse);

    code = ColorIndex(COLOR_WHITE);
    for (i = prestep - 1; i >= 0; i--) {
        if (Con_ColorCode(edit->buffer + i) >= 0) {
            code = Con_ColorCode(edit->buffer + i);
            break;
        }
    }
    for (i = 0; i < drawLen; i++) {
        const char* const s = edit->buffer + prestep + i;
        if (Con_ColorCode(s) >= 0) {
            code = Con_ColorCode(s);
        }
        chars[i] = *s;
        ids[i] = (short)code;
        shadows[i] = CON_RUN_SHADOW;
    }
    Con_DrawRuns(m, textX, baseline, chars, shadows, drawLen, qtrue);
    Con_DrawRuns(m, textX, baseline, chars, ids, drawLen, qfalse);

    if (!con.markMode && !(cls.realtime & 0x100)) {
        float cursorX = textX + (edit->cursor - prestep) * m->adv;
        if (!key_overstrikeMode) {
            cursorX -= m->adv / 2;
        }
        re.SetColor(con_colors[CC_TEXT].color);
        re.Font_DrawString((int)cursorX, (int)baseline, key_overstrikeMode ? "_" : "|", CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
    }
}

/*
================
Con_HelpPrintf

[CNQ3] Com_PrintHelp output for the help panel: measures it first, then
draws it, wrapping the words.
================
*/
static void QDECL Con_HelpPrintf(const char* fmt, ...) {
    const conMetrics_t* const m = con_helpMetrics;
    const int wrap = MIN(MAX(con.linewidth - 4, 20), 100);
    const float* color = con_colors[CC_TEXT].color;
    va_list argptr;
    const char* c;

    va_start(argptr, fmt);
    Q_vsnprintf(con.helpText, sizeof(con.helpText), fmt, argptr);
    va_end(argptr);

    c = con.helpText;
    while (*c != '\0') {
        qboolean wordBreak, forcedBreak;
        int wl = 0;

        while (c[wl] != '\0' && c[wl] > ' ') {
            wl++;
        }

        wordBreak = (wl > 0) && (con.helpX + wl >= wrap) && (wl < wrap);
        forcedBreak = con.helpX >= wrap;
        if (*c == '\n' || forcedBreak || wordBreak) {
            if (!forcedBreak && !wordBreak) {
                c++;
            }
            con.helpWidth = MAX(con.helpWidth, con.helpX);
            con.helpX = 0;
            con.helpY += m->cellH;
            con.helpLines++;
            continue;
        }

        if (Q_IsConsoleColorString(c)) {
            color = Con_ColorFromCode(CON_COLOR_CVAR + (c[1] - COLOR_CON_CVAR));
            c += 2;
            continue;
        }
        if (Con_ColorCode(c) >= 0) {
            color = Con_ColorFromCode(Con_ColorCode(c));
            c += 2;
            continue;
        }

        if (con.helpDraw && *c != ' ') {
            const int x = (int)(con.helpXAdjust + con.helpX * m->adv);
            const int y = (int)(con.helpY + m->asc);
            char str[2];

            str[0] = *c;
            str[1] = '\0';
            if (con_colors[CC_SHADOW].color[3] > 0.0f) {
                re.SetColor(con_colors[CC_SHADOW].color);
                re.Font_DrawString(x + 1, y + 1, str, CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
            }
            re.SetColor(color);
            re.Font_DrawString(x, y, str, CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
        }
        c++;
        con.helpX++;
    }
}

/*
================
Con_DrawHelp

[CNQ3] Help panel below the console for the cvar or command being typed.
================
*/
static void Con_DrawHelp(const conMetrics_t* m) {
    const float* const bg = con_colors[CC_BG].color;
    const float* const border = con_colors[CC_BORDER].color;
    char name[MAX_TOKEN_CHARS];
    const char* arg;
    printHelpResult_t result;
    qboolean printAlways, printModules, printAttribs;
    float x, y, w, h, yh;
    int i;

    if (!con_drawHelp || !(con_drawHelp->integer & DRAWHELP_ENABLE_BIT)) {
        return;
    }
    if (g_consoleField.buffer[0] == '\0') {
        return;
    }
    if (con.displayFrac == 0.0f || con.displayFrac < con.finalFrac) {
        return;
    }

    Cmd_TokenizeString(g_consoleField.buffer);
    if (Cmd_Argc() < 1) {
        return;
    }
    arg = Cmd_Argv(0);
    if (*arg == '/' || *arg == '\\') {
        arg++;
    }
    if (*arg == '\0') {
        return;
    }
    Q_strncpyz(name, arg, sizeof(name));

    printAlways = (con_drawHelp->integer & DRAWHELP_NOTFOUND_BIT) ? qtrue : qfalse;
    printModules = (con_drawHelp->integer & DRAWHELP_MODULES_BIT) ? qtrue : qfalse;
    printAttribs = (con_drawHelp->integer & DRAWHELP_ATTRIBS_BIT) ? qtrue : qfalse;

    // measure
    con_helpMetrics = m;
    con.helpDraw = qfalse;
    con.helpX = 0;
    con.helpY = 0;
    con.helpWidth = 0;
    con.helpLines = 0;
    con.helpXAdjust = m->cellW + 2 * m->adv;
    result = Com_PrintHelp(name, Con_HelpPrintf, qfalse, printModules, printAttribs);
    if (result == PHR_NOTFOUND || (result == PHR_NOHELP && !printAlways)) {
        return;
    }

    x = (int)(con.helpXAdjust - m->adv);
    y = (int)(cls.glconfig.vidHeight * con.displayFrac);
    w = (int)((con.helpWidth + 2) * m->adv);
    h = (int)((con.helpLines + 1) * m->cellH);
    con.helpDraw = qtrue;
    con.helpX = 0;
    con.helpY = y + 1.5f * m->cellH;
    yh = (int)(con.helpY - m->cellH / 2.0f);

    // CNQ3's triangle joining the panel to the console, one strip per pixel row
    for (i = 0; i < (int)m->cellH; i++) {
        Con_FillRectPx(x, y + i, i + 1, 1, bg);
    }
    Con_FillRectPx(x, yh, w, h, bg);
    Con_FillRectPx(x + 1, yh + h + 0, w - 1, 1, border);
    Con_FillRectPx(x + 2, yh + h + 1, w - 2, 1, border);
    Con_FillRectPx(x + w + 0, yh + 1, 1, h + 1, border);
    Con_FillRectPx(x + w + 1, yh + 2, 1, h + 0, border);

    // draw
    Com_PrintHelp(name, Con_HelpPrintf, qfalse, printModules, printAttribs);
}

/*
================
Con_DrawSolidConsoleCNQ3

[CNQ3] con_style 1: the con_col* panel with shadowed text, in Quake Live's
font and cells, rows on the same baselines as Quake Live's console.
================
*/
static void Con_DrawSolidConsoleCNQ3(float frac) {
    char chars[CON_MAXLINEWIDTH];
    short ids[CON_MAXLINEWIDTH];
    short shadows[CON_MAXLINEWIDTH];
    conMetrics_t m;
    int i, x, row, rows, lines, count, len;
    int markStartX, markStartY, markEndX, markEndY;
    qboolean drawMark;
    float y, x0;

    lines = (int)(cls.glconfig.vidHeight * frac);
    if (lines <= 0) {
        return;
    }
    if (lines > cls.glconfig.vidHeight) {
        lines = cls.glconfig.vidHeight;
    }

    Con_GetMetrics(&m);
    con.xadjust = 0;
    x0 = m.cellW;  // one cell of margin

    Con_FillRectPx(0, 0, cls.glconfig.vidWidth, lines - 2, con_colors[CC_BG].color);
    Con_FillRectPx(0, lines - 2, cls.glconfig.vidWidth, 2, con_colors[CC_BORDER].color);

    // version, where Quake Live puts it
    len = strlen(FULL_PRODUCT_VERSION);
    Con_DrawStringTTF(cls.glconfig.vidWidth - CON_CELL_WIDTH * len, lines - CON_CELL_HEIGHT / 2 - CON_CELL_HEIGHT,
                      FULL_PRODUCT_VERSION, con_colors[CC_ARROW].color, CON_CELL_WIDTH, CON_CELL_HEIGHT, CON_TEXT_LITERAL);

    con.vislines = lines;
    rows = (int)((lines - m.cellH) / m.cellH);
    y = lines - m.cellH * 3;  // baseline of the bottom row

    // backscroll arrows, on the empty row above the edit line
    if (con.display != con.current) {
        re.SetColor(con_colors[CC_ARROW].color);
        for (x = 0; x < con.linewidth; x += 4) {
            re.Font_DrawString((int)(x0 + x * m.adv), (int)(y + m.cellH), "^", CON_FONT_MONO, m.fontSize, -1, NULL, CON_TEXT_LITERAL);
        }
        y -= m.cellH;
        rows--;
    }

    row = con.display;
    if (con.x == 0) {
        row--;
    }

    drawMark = Con_MarkBounds(&markStartX, &markStartY, &markEndX, &markEndY);
    count = MIN(con.linewidth, CON_MAXLINEWIDTH);

    con.rowsVisible = 0;
    for (i = 0; i < rows; i++, y -= m.cellH, row--) {
        const short* text;
        qboolean markRow;

        if (row < 0) {
            break;
        }
        if (con.current - row >= con.totallines) {
            // past scrollback wrap point
            continue;
        }
        if (y - m.asc >= 0) {
            con.rowsVisible++;
        }

        text = con.text + (row % con.totallines) * con.linewidth;
        markRow = (drawMark && row >= markStartY && row <= markEndY) ? qtrue : qfalse;

        if (markRow) {
            Con_FillRectPx(x0 + markStartX * m.adv, y - m.asc, (markEndX - markStartX + 1) * m.adv, m.cellH, con_colors[CC_MKBG].color);
        }

        for (x = 0; x < count; x++) {
            const qboolean marked = (markRow && x >= markStartX && x <= markEndX) ? qtrue : qfalse;
            chars[x] = (char)(text[x] & 0xff);
            ids[x] = marked ? CON_RUN_MKTEXT : (short)(text[x] >> 8);
            shadows[x] = marked ? CON_RUN_MKSHADOW : CON_RUN_SHADOW;
        }

        Con_DrawRuns(&m, x0, y, chars, shadows, count, qtrue);

        if (con.searchStarted && row == con.searchLine) {
            re.SetColor(con_colors[CC_SEARCH].color);
            re.Font_DrawString((int)(x0 - m.cellW), (int)y, ">", CON_FONT_MONO, m.fontSize, -1, NULL, CON_TEXT_LITERAL);
        }

        Con_DrawRuns(&m, x0, y, chars, ids, count, qfalse);

        Con_RowLinks(text, count, x0, y, m.adv, m.asc, m.cellH);
    }

    Con_DrawInputCNQ3(&m);
    Con_DrawHelp(&m);

    re.SetColor(NULL);
}

/*
================
Con_DrawSolidConsole

[QL] quakelive_steam.exe 0x4b4300. The background and its red border line
are in 640x480 coordinates, both with the con_opacity alpha; the text is
DroidSansMono in con_scale'd 12x24 cells, one string per row with the colours
as '^' codes. As in retail, the row count divides by the cell width, and the
colour of the previous row carries into the next one's codes.
================
*/
void Con_DrawSolidConsole(float frac) {
    const float s = Con_Scale();
    char line[CON_LINEBUFFER];
    conMetrics_t m = {0};
    vec4_t color;
    int i, x, y, lines, rows, row, n, last, currentColor, len;
    int markStartX, markStartY, markEndX, markEndY;
    qboolean drawMark, haveMetrics = qfalse;

    if (Con_GetStyle() == 1) {
        Con_DrawSolidConsoleCNQ3(frac);  // [CNQ3]
        return;
    }

    lines = (int)(cls.glconfig.vidHeight * frac);
    if (lines <= 0) {
        return;
    }
    if (lines > cls.glconfig.vidHeight) {
        lines = cls.glconfig.vidHeight;
    }

    // background
    y = (int)(frac * SCREEN_HEIGHT - 2.0);
    if (y < 1) {
        y = 0;
    } else if (con_background->value != 0) {
        SCR_DrawPic(0, 0, SCREEN_WIDTH, y, cls.consoleShader);
    } else {
        color[0] = color[1] = color[2] = 0;
        color[3] = con_opacity->value;
        SCR_FillRect(0, 0, SCREEN_WIDTH, y, color);
    }

    // border line
    color[0] = 1;
    color[1] = color[2] = 0;
    color[3] = con_opacity->value;
    SCR_FillRect(0, y, SCREEN_WIDTH, 2, color);

    // version, in unscaled cells
    re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
    len = strlen(FULL_PRODUCT_VERSION);
    Con_DrawStringTTF(cls.glconfig.vidWidth - CON_CELL_WIDTH * len, lines - CON_CELL_HEIGHT / 2 - CON_CELL_HEIGHT,
                      FULL_PRODUCT_VERSION, NULL, CON_CELL_WIDTH, CON_CELL_HEIGHT, 0);

    con.vislines = lines;
    rows = (int)((lines - CON_CELL_WIDTH * s) / (CON_CELL_WIDTH * s));
    y = (int)(lines - CON_CELL_HEIGHT * s * 3);

    // draw arrows to show the buffer is backscrolled
    if (con.display != con.current) {
        re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
        for (x = 0; x < con.linewidth / s; x += 4) {
            Con_DrawCharTTF((int)(((x + 1) * CON_CELL_WIDTH + con.xadjust) * s), y,
                            (int)(CON_CELL_WIDTH * s), (int)(CON_CELL_HEIGHT * s), '^');
        }
        y = (int)(y - CON_CELL_HEIGHT * s);
        rows--;
    }

    row = con.display;
    if (con.x == 0) {
        row--;
    }

    re.SetColor(g_color_table[ColorIndex(COLOR_WHITE)]);
    currentColor = ColorIndex(COLOR_WHITE);

    // [CNQ3] mark mode and the search marker, only drawn once used
    // [ioquakelive] the metrics are wanted for the links too, so they are always taken
    Con_GetMetrics(&m);
    haveMetrics = qtrue;
    drawMark = Con_MarkBounds(&markStartX, &markStartY, &markEndX, &markEndY);
    con.rowsVisible = 0;

    for (i = 0; i < rows; i++, y = (int)(y - CON_CELL_HEIGHT * s), row--) {
        const short* text;
        qboolean markRow;

        if (row < 0) {
            break;
        }
        if (con.current - row >= con.totallines) {
            // past scrollback wrap point
            continue;
        }
        if (y - CON_CELL_HEIGHT * s >= 0) {
            con.rowsVisible++;
        }

        text = con.text + (row % con.totallines) * con.linewidth;
        markRow = (drawMark && row >= markStartY && row <= markEndY) ? qtrue : qfalse;

        if (haveMetrics && markRow) {
            Con_FillRectPx(CON_CELL_WIDTH * s + con.xadjust + markStartX * m.adv, y - m.asc,
                           (markEndX - markStartX + 1) * m.adv, m.cellH, con_colors[CC_MKBG].color);
            re.SetColor(g_color_table[ColorIndex(COLOR_WHITE)]);
        }
        if (haveMetrics && con.searchStarted && row == con.searchLine) {
            re.SetColor(con_colors[CC_SEARCH].color);
            re.Font_DrawString((int)con.xadjust, y, ">", CON_FONT_MONO, m.fontSize, -1, NULL, CON_TEXT_LITERAL);
            re.SetColor(g_color_table[ColorIndex(COLOR_WHITE)]);
        }

        n = 0;
        last = 0;
        for (x = 0; x < con.linewidth && n < CON_LINEBUFFER - 2; x++) {
            const int ch = text[x] & 0xff;
            int code;

            if (ch == ' ') {
                line[n++] = ' ';
                continue;
            }

            code = Con_RetailColor(text[x] >> 8);
            if (markRow && x >= markStartX && x <= markEndX) {
                code = 0;  // [CNQ3] black on the mark
            }
            if (code != currentColor) {
                currentColor = code;
                line[n++] = Q_COLOR_ESCAPE;
                line[n++] = (char)Q_ColorCodeChar(currentColor);
            }
            if (n < CON_LINEBUFFER - 1) {
                line[n++] = ch;
            }
            last = n;
        }
        line[last] = '\0';

        re.Font_DrawString((int)(CON_CELL_WIDTH * s + con.xadjust), y, line, CON_FONT_MONO,
                           CON_CELL_WIDTH * s * CON_FONT_SCALE, -1, NULL, 0);

        Con_RowLinks(text, con.linewidth, CON_CELL_WIDTH * s + con.xadjust, (float)y, m.adv, m.asc, m.cellH);
        re.SetColor(g_color_table[ColorIndex(currentColor)]);
    }

    // draw the input prompt, user text, and cursor if desired
    Con_DrawInput();

    re.SetColor(NULL);
}

/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole(void) {
    // check for console width changes from a vid mode change
    Con_CheckResize();

    // [ioquakelive] the link table is this frame's
    con.numLinks = 0;

    // if disconnected, render console full screen
    if (clc.state == CA_DISCONNECTED) {
        if (!(Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME))) {
            Con_DrawSolidConsole(1.0);
            Con_DrawLinkPrompt();
            return;
        }
    }

    if (con.displayFrac) {
        Con_DrawSolidConsole(con.displayFrac);
        Con_DrawLinkPrompt();
    } else {
        // draw notify lines
        if (clc.state == CA_ACTIVE) {
            Con_DrawNotify();
        }
    }
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole(void) {
    // decide on the destination height of the console
    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        con.finalFrac = con_height->value;
    } else {
        con.finalFrac = 0;
        con.linkAsked[0] = '\0';  // [ioquakelive] a closed console asks nothing
    }

    // scroll towards the destination height
    if (con.finalFrac < con.displayFrac) {
        con.displayFrac -= con_speed->value * cls.realFrametime * 0.001;
        if (con.finalFrac > con.displayFrac)
            con.displayFrac = con.finalFrac;

    } else if (con.finalFrac > con.displayFrac) {
        con.displayFrac += con_speed->value * cls.realFrametime * 0.001;
        if (con.finalFrac < con.displayFrac)
            con.displayFrac = con.finalFrac;
    }

    Con_UpdateColors();  // [CNQ3]
}

void Con_PageUp(void) {
    con.display -= 2;
    if (con.current - con.display >= con.totallines) {
        con.display = con.current - con.totallines + 1;
    }
}

void Con_PageDown(void) {
    con.display += 2;
    if (con.display > con.current) {
        con.display = con.current;
    }
}

void Con_Top(void) {
    con.display = con.totallines;
    if (con.current - con.display >= con.totallines) {
        con.display = con.current - con.totallines + 1;
    }
}

void Con_Bottom(void) {
    con.display = con.current;
}

/*
==================
Con_ScrollLines / Con_ScrollPages

[CNQ3] con_style 1 scrolling, negative goes up. Pages keep one line of
overlap for readability.
==================
*/
void Con_ScrollLines(int lines) {
    if (lines == 0) {
        return;
    }

    con.display += lines;
    Con_FixPosition();
}

void Con_ScrollPages(int pages) {
    if (pages == 0) {
        return;
    }

    con.display += pages * MAX(con.rowsVisible - 1, 1);
    Con_FixPosition();
}

void Con_Close(void) {
    if (!com_cl_running->integer) {
        return;
    }
    Field_Clear(&g_consoleField);
    Con_ClearNotify();
    Key_SetCatcher(Key_GetCatcher() & ~KEYCATCH_CONSOLE);
    con.finalFrac = 0;  // none visible
    con.displayFrac = 0;
    con.markMode = qfalse;  // [CNQ3]
}

/*
==============================================================================

[CNQ3] MARK MODE

ctrl-M enters it, the arrows, home and end move the cursor (with shift they
extend the selection), shift-page up/down move it 6 lines and ctrl-C or enter
copy the selection to the clipboard.

==============================================================================
*/

// last line with text
static int Con_LastMarkLine(void) {
    const int last = con.x == 0 ? con.current - 1 : con.current;
    return last > 0 ? last : 0;
}

// keeps the mark cursor on screen
static void Con_AutoScrollMarkPosition(void) {
    const int bottom = Con_BottomRow();
    int visible = con.rowsVisible;

    if (con.display == con.current) {
        visible--;  // scrolling up brings the backscroll arrows
    }
    if (visible < 1) {
        visible = 1;
    }

    if (con.markY > bottom) {
        con.display += con.markY - bottom;
    } else if (con.markY < bottom - visible + 1) {
        con.display -= (bottom - visible + 1) - con.markY;
    }
    Con_FixPosition();
}

static void Con_CopyMark(void) {
    static char markText[CON_TEXTSIZE * 2 + 1];
    const char* const end = markText + sizeof(markText) - 3;
    int startX, startY, endX, endY, x, y;
    char* dst = markText;

    Con_MarkBounds(&startX, &startY, &endX, &endY);

    for (y = startY; y <= endY && dst < end; y++) {
        const short* const text = con.text + (y % con.totallines) * con.linewidth;
        char* lineStart;

        if (y > startY) {
            *dst++ = '\r';
            *dst++ = '\n';
        }

        lineStart = dst;
        for (x = startX; x <= endX && x < con.linewidth && dst < end; x++) {
            const int c = text[x] & 0xff;
            *dst++ = (c >= ' ' && c < 127) ? (char)c : ' ';
        }
        while (dst > lineStart && dst[-1] == ' ') {
            dst--;
        }
    }
    *dst = '\0';

    Sys_SetClipboardData(markText);
}

static void Con_MarkMoved(qboolean shiftDown) {
    if (!shiftDown) {
        con.markStartX = con.markX;
        con.markStartY = con.markY;
    }
    Con_AutoScrollMarkPosition();
}

qboolean Con_HandleMarkMode(qboolean ctrlDown, qboolean shiftDown, int key) {
    const int firstLine = Con_FirstLine();
    const int lastLine = Con_LastMarkLine();

    // ctrl-m toggles mark mode
    if (ctrlDown && (key == 'm' || key == 'M')) {
        con.markMode = !con.markMode;
        if (con.markMode) {
            int line = Con_BottomRow();
            if (line > lastLine) {
                line = lastLine;
            }
            if (line < firstLine) {
                line = firstLine;
            }
            con.markX = 0;
            con.markY = line;
            con.markStartX = 0;
            con.markStartY = line;
        }
        return qtrue;
    }

    if (!con.markMode) {
        return qfalse;
    }

    switch (key) {
        case K_HOME:
            if (ctrlDown) {
                return qfalse;
            }
            con.markX = 0;
            Con_MarkMoved(shiftDown);
            return qtrue;

        case K_END:
            if (ctrlDown) {
                return qfalse;
            }
            con.markX = con.linewidth - 1;
            if (shiftDown) {
                // avoid selecting trailing columns of whitespace
                const int startY = MIN(con.markStartY, con.markY);
                const int endY = MAX(con.markStartY, con.markY);
                while (con.markX > 0) {
                    qboolean skip = qtrue;
                    int y;
                    for (y = startY; y <= endY; y++) {
                        const short* const text = con.text + (y % con.totallines) * con.linewidth;
                        if ((text[con.markX] & 0xff) != ' ') {
                            skip = qfalse;
                            break;
                        }
                    }
                    if (!skip) {
                        break;
                    }
                    con.markX--;
                }
            } else {
                // avoid jumping into whitespace
                const short* const text = con.text + (con.markY % con.totallines) * con.linewidth;
                while (con.markX > 0 && (text[con.markX] & 0xff) == ' ') {
                    con.markX--;
                }
            }
            Con_MarkMoved(shiftDown);
            return qtrue;

        case K_LEFTARROW:
            con.markX = MAX(con.markX - 1, 0);
            Con_MarkMoved(shiftDown);
            return qtrue;

        case K_RIGHTARROW:
            con.markX = MIN(con.markX + 1, con.linewidth - 1);
            Con_MarkMoved(shiftDown);
            return qtrue;

        case K_UPARROW:
            con.markY = MAX(con.markY - 1, firstLine);
            Con_MarkMoved(shiftDown);
            return qtrue;

        case K_DOWNARROW:
            con.markY = MIN(con.markY + 1, lastLine);
            Con_MarkMoved(shiftDown);
            return qtrue;

        case K_PGUP:
        case K_PGDN:
            if (!shiftDown) {
                return qfalse;  // the usual scrolling
            }
            con.markY = key == K_PGUP ? MAX(con.markY - 6, firstLine) : MIN(con.markY + 6, lastLine);
            Con_AutoScrollMarkPosition();
            return qtrue;

        case K_MWHEELUP:
        case K_MWHEELDOWN:
            return qfalse;

        case K_ENTER:
        case K_KP_ENTER:
            Con_CopyMark();
            con.markMode = qfalse;
            return qtrue;

        default:
            break;
    }

    if (ctrlDown && (key == 'c' || key == 'C')) {
        Con_CopyMark();
        con.markMode = qfalse;
        con.swallowCtrlC = qtrue;
        return qtrue;
    }

    // block everything else
    return qtrue;
}

qboolean Con_IsInMarkMode(void) {
    return con.markMode;
}

/*
==================
Con_SwallowCharEvent

[CNQ3] Typing is blocked in mark mode, and the char event of the ctrl-C that
copied the selection must not clear the edit line.
==================
*/
qboolean Con_SwallowCharEvent(int ch) {
    if (con.markMode) {
        return qtrue;
    }

    if (con.swallowCtrlC) {
        con.swallowCtrlC = qfalse;
        return ch == 'c' - 'a' + 1 ? qtrue : qfalse;
    }

    return qfalse;
}
