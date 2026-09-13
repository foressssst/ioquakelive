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

// [CNQ3] con_style 1 line width cap, keeps the per-line scratch buffers small
#define CON_MAXLINEWIDTH 1000

// [CNQ3] text buffer codes of the console-only colours, after Quake Live's 0-7
#define CON_COLOR_CVAR 8
#define CON_COLOR_CMD 9
#define CON_COLOR_VAL 10
#define CON_COLOR_HELP 11

// [CNQ3] con_drawHelp bits
#define DRAWHELP_ENABLE_BIT 1
#define DRAWHELP_NOTFOUND_BIT 2
#define DRAWHELP_MODULES_BIT 4
#define DRAWHELP_ATTRIBS_BIT 8
#define DRAWHELP_MAX 15

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
    float cw, ch;     // character size in pixels
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

// [CNQ3] Console ported from CNQ3 (code/client/cl_console.cpp). con_style 0 is
// Quake Live's console, untouched; con_style 1 is CNQ3's look with its colours,
// scalable font and help panel. Mark mode and console search work in both.
cvar_t* con_style;
cvar_t* con_scaleMode;
cvar_t* con_drawHelp;
cvar_t* con_history;

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

// CNQ3's defaults
static conColor_t con_colors[CC_COUNT] = {
    {"con_colBG", "101013F6", qtrue, "RGBA color of the background"},
    {"con_colBorder", "4778B2FF", qtrue, "RGBA color of the border"},
    {"con_colArrow", "4778B2FF", qtrue, "RGBA color of backscroll arrows"},
    {"con_colShadow", "000000FF", qtrue, "RGBA color of text shadows"},
    {"con_colMkBG", "BFBFBFFF", qtrue, "RGBA color of the mark background"},
    {"con_colMkShadow", "FFFFFF00", qtrue, "RGBA color of the mark text shadows"},
    {"con_colText", "E2E2E2", qfalse, "RGB color of text"},
    {"con_colMkText", "000000", qfalse, "RGB color of mark text"},
    {"con_colCVar", "4778B2", qfalse, "RGB color of variable names"},
    {"con_colCmd", "4FA7BD", qfalse, "RGB color of command names"},
    {"con_colValue", "E5BC39", qfalse, "RGB color of variable values"},
    {"con_colHelp", "ABC1C6", qfalse, "RGB color of help text"},
    {"con_colSearch", "FFFF00", qfalse, "RGB color of search result marker"},
    {"con_colHL", "303033FF", qtrue, "RGBA color of auto-completion highlights\n"
                                     "This requires " S_COLOR_CVAR "con_completionStyle " S_COLOR_VAL "1" S_COLOR_HELP "."},
};

/*
================
Con_CharW / Con_CharH

Returns scaled console character dimensions based on con_scale
================
*/
static int Con_CharW(void) {
    if (con_scale) {
        int w = (int)(SMALLCHAR_WIDTH * con_scale->value);
        return w > 0 ? w : 1;
    }
    return SMALLCHAR_WIDTH;
}

static int Con_CharH(void) {
    if (con_scale) {
        int h = (int)(SMALLCHAR_HEIGHT * con_scale->value);
        return h > 0 ? h : 1;
    }
    return SMALLCHAR_HEIGHT;
}

/*
================
Con_DrawCharScaled

Draws a character at native screen resolution with specified dimensions.
Like SCR_DrawSmallChar but with variable width/height.
================
*/
static void Con_DrawCharScaled(int x, int y, int w, int h, int ch) {
    int row, col;
    float frow, fcol;
    float size;

    ch &= 255;

    if (ch == ' ') {
        return;
    }

    if (y < -h) {
        return;
    }

    row = ch >> 4;
    col = ch & 15;

    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;

    re.DrawStretchPic(x, y, w, h,
                      fcol, frow,
                      fcol + size, frow + size,
                      cls.charSetShader);
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

    code = ColorIndexForNumber(code);
    if (code == ColorIndex(COLOR_WHITE)) {
        return con_colors[CC_TEXT].color;
    }
    return g_color_table[code];
}

/*
================
Con_RetailColor

[CNQ3] Colour index of a text buffer code in con_style 0: Quake Live has no
console-only colours, so they are white there.
================
*/
static int Con_RetailColor(int code) {
    if (code >= CON_COLOR_CVAR) {
        return ColorIndex(COLOR_WHITE);
    }
    return ColorIndexForNumber(code);
}

/*
================
Con_DrawCharF / Con_FillRectPx

[CNQ3] Native pixel coordinates.
================
*/
static void Con_DrawCharF(float x, float y, float w, float h, int ch) {
    const float size = 0.0625f;
    float frow, fcol;

    ch &= 255;
    if (ch == ' ') {
        return;
    }

    frow = (ch >> 4) * size;
    fcol = (ch & 15) * size;
    re.DrawStretchPic(x, y, w, h, fcol, frow, fcol + size, frow + size, cls.charSetShader);
}

static void Con_FillRectPx(float x, float y, float w, float h, const float* color) {
    re.SetColor(color);
    re.DrawStretchPic(x, y, w, h, 0, 0, 0, 0, cls.whiteShader);
}

/*
================
Con_ResizeFont

[CNQ3] con_style 1 character size: 8x12 scaled by con_scale, and by the
resolution with con_scaleMode 1. Whole pixels, so the line width computed
from it matches what gets drawn.
================
*/
static void Con_ResizeFont(void) {
    con.cw = 8.0f;
    con.ch = 12.0f;

    if (cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 || !con_scale || !con_scaleMode) {
        return;
    }

    if (con_scaleMode->integer != 2) {
        if (con_scaleMode->integer == 1) {
            SCR_AdjustFrom640(&con.cw, &con.ch, NULL, NULL);
        }
        con.cw *= con_scale->value;
        con.ch *= con_scale->value;
        if (cls.glconfig.vidWidth * SCREEN_HEIGHT > cls.glconfig.vidHeight * SCREEN_WIDTH) {
            // the console distorts horribly on widescreens
            con.cw = con.ch * SCREEN_HEIGHT / SCREEN_WIDTH;
        }
    }

    con.cw = (float)ceil(con.cw);
    con.ch = (float)ceil(con.ch);
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
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f(void) {
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

    // [QL] calculate linewidth from vidWidth and con_scale
    con.xadjust = 0;
    if (Con_GetStyle() == 1) {
        // [CNQ3] con_style 1 font, one character of margin on each side
        Con_ResizeFont();
        width = cls.glconfig.vidWidth > 0 ? (int)(cls.glconfig.vidWidth / con.cw) - 2 : 0;
        if (width > CON_MAXLINEWIDTH) {
            width = CON_MAXLINEWIDTH;
        }
    } else if (con_scale && cls.glconfig.vidWidth > 0) {
        width = (int)(cls.glconfig.vidWidth / (con_scale->value * SMALLCHAR_WIDTH)) - 2;
    } else {
        width = cls.glconfig.vidWidth / SMALLCHAR_WIDTH - 2;
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
    char rawText[CON_MAXLINEWIDTH * 2 + 1];
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

    // [CNQ3] console style, all defaults keep Quake Live's console
    con_style = Cvar_Get("con_style", "0", CVAR_ARCHIVE);
    Cvar_CheckRange(con_style, 0, 1, qtrue);

    con_scaleMode = Cvar_Get("con_scaleMode", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_scaleMode, 0, 2, qtrue);

    con_drawHelp = Cvar_Get("con_drawHelp", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_drawHelp, 0, DRAWHELP_MAX, qtrue);

    con_history = Cvar_Get("con_history", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_history, 0, 1, qtrue);

    for (i = 0; i < CC_COUNT; i++) {
        conColor_t* const c = &con_colors[i];

        Com_ParseHexColor(c->color, c->defaultValue, c->hasAlpha);
        c->cvar = Cvar_Get(c->name, c->defaultValue, CVAR_ARCHIVE);
        c->modificationCount = -1;
        Cvar_SetHelp(c->name, c->help);
    }
    Con_UpdateColors();

    Cvar_SetHelp("con_style", "console style\n"
                 S_COLOR_VAL "    0 " S_COLOR_HELP "= Quake Live\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= CNQ3: colors, scalable font, help panel and CNQ3 scrolling");
    Cvar_SetHelp("con_scaleMode", "console text scaling mode\n"
                 S_COLOR_VAL "    0 " S_COLOR_HELP "= Text size scales with con_scale but not the resolution\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= Text size scales with con_scale and the resolution\n"
                 S_COLOR_VAL "    2 " S_COLOR_HELP "= Text size is always 8x12\n"
                 "Only used by " S_COLOR_CVAR "con_style " S_COLOR_VAL "1" S_COLOR_HELP ".");
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

        if (Q_IsColorString(txt)) {
            color = ColorIndex(*(txt + 1));
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
Con_DrawShadowedChar

[CNQ3]
================
*/
static void Con_DrawShadowedChar(float x, float y, int c, const float* color) {
    if (con_colors[CC_SHADOW].color[3] > 0.0f) {
        re.SetColor(con_colors[CC_SHADOW].color);
        Con_DrawCharF(x + 1, y + 1, con.cw, con.ch, c);
    }
    re.SetColor(color);
    Con_DrawCharF(x, y, con.cw, con.ch, c);
}

/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput(void) {
    int y;

    if (clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    int charW = Con_CharW();
    int charH = Con_CharH();

    y = con.vislines - (charH * 2);

    g_consoleField.widthInChars = g_console_field_width;  // [CNQ3] con_style 1 changes it

    re.SetColor(con.color);

    Con_DrawCharScaled(con.xadjust + 1 * charW, y, charW, charH, ']');

    Field_Draw(&g_consoleField, con.xadjust + 2 * charW, y,
               SCREEN_WIDTH - 3 * charW, !con.markMode, qtrue);
}

/*
================
Con_DrawInputCNQ3

[CNQ3] con_style 1 edit line: shadowed, colour codes shown in their colour,
and the match cycled through by the completion highlighted.
================
*/
static void Con_DrawInputCNQ3(void) {
    field_t* const edit = &g_consoleField;
    const float cw = con.cw;
    const float ch = con.ch;
    const float y = con.vislines - ch * 1.5f;
    const float textX = con.xadjust + cw;
    const float* color;
    int len, drawLen, prestep, i;

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

    if (edit->acLength > 0) {
        const int start = MAX(edit->acOffset, prestep);
        const int end = MIN(edit->acOffset + edit->acLength, prestep + drawLen);
        if (end > start) {
            Con_FillRectPx(textX + (start - prestep) * cw, y, (end - start) * cw, ch, con_colors[CC_HL].color);
        }
    }

    Con_DrawShadowedChar(con.xadjust, y, ']', con_colors[CC_TEXT].color);

    color = con_colors[CC_TEXT].color;
    for (i = prestep - 1; i >= 0; i--) {
        if (Q_IsColorString(edit->buffer + i)) {
            color = Con_ColorFromCode(ColorIndex(edit->buffer[i + 1]));
            break;
        }
    }

    for (i = 0; i < drawLen; i++) {
        const char* const s = edit->buffer + prestep + i;
        if (Q_IsColorString(s)) {
            color = Con_ColorFromCode(ColorIndex(s[1]));
        }
        Con_DrawShadowedChar(textX + i * cw, y, *s, color);
    }

    if (!con.markMode && Sys_Milliseconds() % 500 >= 250) {
        re.SetColor(con_colors[CC_TEXT].color);
        Con_DrawCharF(textX + (edit->cursor - prestep) * cw, y, cw, ch, key_overstrikeMode ? 11 : 10);
    }
}

/*
================
Con_DrawNotify

[QL] Only the chat input field. Quake Live's engine (quakelive_steam.exe
0x4b4100) draws the "say:" / "say team:" / "reply:" field here and nothing else:
there is no loop over recent console lines, so console output never shows over
the game. Chat and obituaries are drawn by cgame's HUD ownerdraws instead.
================
*/
void Con_DrawNotify(void) {
    int v;
    int skip;

    v = 0;

    if (Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME)) {
        return;
    }

    // draw the chat line
    if (Key_GetCatcher() & KEYCATCH_MESSAGE) {
        if (chat_team) {
            SCR_DrawBigString(8, v, "say_team:", 1.0f, qfalse);
            skip = 10;
        } else {
            SCR_DrawBigString(8, v, "say:", 1.0f, qfalse);
            skip = 5;
        }

        Field_BigDraw(&chatField, skip * BIGCHAR_WIDTH, v,
                      SCREEN_WIDTH - (skip + 1) * BIGCHAR_WIDTH, qtrue, qtrue);
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
            con.helpY += con.ch;
            con.helpLines++;
            continue;
        }

        if (Q_IsConsoleColorString(c)) {
            color = Con_ColorFromCode(CON_COLOR_CVAR + (c[1] - COLOR_CON_CVAR));
            c += 2;
            continue;
        }
        if (Q_IsColorString(c)) {
            color = Con_ColorFromCode(ColorIndex(c[1]));
            c += 2;
            continue;
        }

        if (con.helpDraw && *c != ' ') {
            Con_DrawShadowedChar(con.helpXAdjust + con.helpX * con.cw, con.helpY, *c, color);
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
static void Con_DrawHelp(void) {
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
    con.helpDraw = qfalse;
    con.helpX = 0;
    con.helpWidth = 0;
    con.helpLines = 0;
    con.helpXAdjust = con.xadjust + 2 * con.cw;
    result = Com_PrintHelp(name, Con_HelpPrintf, qfalse, printModules, printAttribs);
    if (result == PHR_NOTFOUND || (result == PHR_NOHELP && !printAlways)) {
        return;
    }

    x = (int)(con.helpXAdjust - con.cw);
    y = (int)(cls.glconfig.vidHeight * con.displayFrac);
    w = (int)((con.helpWidth + 2) * con.cw);
    h = (int)((con.helpLines + 1) * con.ch);
    con.helpDraw = qtrue;
    con.helpX = 0;
    con.helpY = y + 1.5f * con.ch;
    yh = (int)(con.helpY - con.ch / 2.0f);

    // CNQ3's triangle joining the panel to the console, one strip per pixel row
    for (i = 0; i < (int)con.ch; i++) {
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

[CNQ3] con_style 1: con_col* colours, shadowed text, scalable font.
================
*/
static void Con_DrawSolidConsoleCNQ3(float frac) {
    const float cw = con.cw;
    const float ch = con.ch;
    int i, x, row, rows, scanlines, len;
    int markStartX, markStartY, markEndX, markEndY;
    qboolean drawMark;
    float y;

    scanlines = (int)(cls.glconfig.vidHeight * frac);
    if (scanlines <= 0) {
        return;
    }
    if (scanlines > cls.glconfig.vidHeight) {
        scanlines = cls.glconfig.vidHeight;
    }

    con.xadjust = cw;  // one character of margin

    // background and border
    Con_FillRectPx(0, 0, cls.glconfig.vidWidth, scanlines - 2, con_colors[CC_BG].color);
    Con_FillRectPx(0, scanlines - 2, cls.glconfig.vidWidth, 2, con_colors[CC_BORDER].color);

    // version
    re.SetColor(con_colors[CC_TEXT].color);
    len = strlen(FULL_PRODUCT_VERSION);
    for (x = 0; x < len; x++) {
        Con_DrawCharF(cls.glconfig.vidWidth - (len - x + 1) * SMALLCHAR_WIDTH, scanlines - SMALLCHAR_HEIGHT * 1.5f,
                      SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, FULL_PRODUCT_VERSION[x]);
    }

    con.vislines = scanlines;
    rows = (int)((scanlines - ch) / ch);
    y = scanlines - ch * 3;

    // draw arrows to show the buffer is backscrolled
    if (con.display != con.current) {
        const int xEnd = (int)((cls.glconfig.vidWidth - con.xadjust) / cw);
        re.SetColor(con_colors[CC_ARROW].color);
        for (x = 0; x < xEnd; x += 4) {
            Con_DrawCharF(con.xadjust + x * cw, y, cw, ch, '^');
        }
        y -= ch;
        rows--;
    }

    row = con.display;
    if (con.x == 0) {
        row--;
    }

    drawMark = Con_MarkBounds(&markStartX, &markStartY, &markEndX, &markEndY);

    con.rowsVisible = 0;
    for (i = 0; i < rows; i++, y -= ch, row--) {
        const short* text;
        qboolean markRow;
        int color;

        if (row < 0) {
            break;
        }
        if (con.current - row >= con.totallines) {
            // past scrollback wrap point
            continue;
        }
        if (y >= 0) {
            con.rowsVisible++;
        }

        text = con.text + (row % con.totallines) * con.linewidth;
        markRow = (drawMark && row >= markStartY && row <= markEndY) ? qtrue : qfalse;

        if (markRow) {
            Con_FillRectPx(con.xadjust + markStartX * cw, y, (markEndX - markStartX + 1) * cw, ch, con_colors[CC_MKBG].color);
        }

        // shadows
        color = -1;
        for (x = 0; x < con.linewidth; x++) {
            const int newColor = (markRow && x >= markStartX && x <= markEndX) ? CC_MKSHADOW : CC_SHADOW;
            if ((text[x] & 0xff) == ' ' || con_colors[newColor].color[3] <= 0.0f) {
                continue;
            }
            if (newColor != color) {
                color = newColor;
                re.SetColor(con_colors[color].color);
            }
            Con_DrawCharF(con.xadjust + x * cw + 1, y + 1, cw, ch, text[x] & 0xff);
        }

        if (con.searchStarted && row == con.searchLine) {
            re.SetColor(con_colors[CC_SEARCH].color);
            Con_DrawCharF(con.xadjust - cw, y, cw, ch, 141);
        }

        // text
        color = -1;
        for (x = 0; x < con.linewidth; x++) {
            const int newColor = (markRow && x >= markStartX && x <= markEndX) ? -2 : (text[x] >> 8);
            if ((text[x] & 0xff) == ' ') {
                continue;
            }
            if (newColor != color) {
                color = newColor;
                re.SetColor(color == -2 ? con_colors[CC_MKTEXT].color : Con_ColorFromCode(color));
            }
            Con_DrawCharF(con.xadjust + x * cw, y, cw, ch, text[x] & 0xff);
        }
    }

    Con_DrawInputCNQ3();
    Con_DrawHelp();

    re.SetColor(NULL);
}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole(float frac) {
    int i, x, y;
    int rows;
    short* text;
    int row;
    int lines;
    int currentColor;
    vec4_t backgroundColor;
    vec4_t color;
    int charW = Con_CharW();
    int charH = Con_CharH();
    int markStartX, markStartY, markEndX, markEndY;
    qboolean drawMark;

    if (Con_GetStyle() == 1) {
        Con_DrawSolidConsoleCNQ3(frac);  // [CNQ3]
        return;
    }

    lines = cls.glconfig.vidHeight * frac;
    if (lines <= 0)
        return;

    if (lines > cls.glconfig.vidHeight)
        lines = cls.glconfig.vidHeight;

    // on wide screens, we will center the text
    con.xadjust = 0;
    SCR_AdjustFrom640(&con.xadjust, NULL, NULL, NULL);

    // console background
    y = frac * SCREEN_HEIGHT;
    if (y < 1) {
        y = 0;
    } else {
        if (con_background->integer) {
            // draw the background shader
            SCR_DrawPic(0, 0, SCREEN_WIDTH, y, cls.consoleShader);
        } else {
            // draw the background color
            backgroundColor[0] = 0;                   // red
            backgroundColor[1] = 0;                   // green
            backgroundColor[2] = 0;                   // blue
            backgroundColor[3] = con_opacity->value;   // alpha
            SCR_FillRect(0, 0, SCREEN_WIDTH, y, backgroundColor);
        }
    }

    // draw the border line
    color[0] = 1;
#ifdef _DEBUG
    color[1] = 1;
#else
    color[1] = 0;
#endif
    color[2] = 0;
    color[3] = 1;

    SCR_FillRect(0, y, SCREEN_WIDTH, 2, color);

    // draw the version number
#ifdef _DEBUG
    re.SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
#else
    re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
#endif

    i = strlen(FULL_PRODUCT_VERSION);

    for (x = 0; x < i; x++) {
        Con_DrawCharScaled(cls.glconfig.vidWidth - (i - x + 1) * charW,
                           lines - charH, charW, charH, FULL_PRODUCT_VERSION[x]);
    }

    // draw the text
    con.vislines = lines;
    rows = (lines - charH) / charH;  // rows of text to draw

    y = lines - (charH * 3);

    // draw from the bottom up
    if (con.display != con.current) {
        // draw arrows to show the buffer is backscrolled
        re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
        for (x = 0; x < con.linewidth; x += 4)
            Con_DrawCharScaled(con.xadjust + (x + 1) * charW, y, charW, charH, '^');
        y -= charH;
        rows--;
    }

    row = con.display;

    if (con.x == 0) {
        row--;
    }

    currentColor = 7;
    re.SetColor(g_color_table[currentColor]);

    // [CNQ3] mark mode and search marker, only drawn once used
    drawMark = Con_MarkBounds(&markStartX, &markStartY, &markEndX, &markEndY);
    con.rowsVisible = 0;

    for (i = 0; i < rows; i++, y -= charH, row--) {
        qboolean markRow;

        if (row < 0)
            break;
        if (con.current - row >= con.totallines) {
            // past scrollback wrap point
            continue;
        }
        if (y >= 0) {
            con.rowsVisible++;
        }

        text = con.text + (row % con.totallines) * con.linewidth;
        markRow = (drawMark && row >= markStartY && row <= markEndY) ? qtrue : qfalse;

        if (markRow) {
            Con_FillRectPx(con.xadjust + (markStartX + 1) * charW, y, (markEndX - markStartX + 1) * charW, charH,
                           con_colors[CC_MKBG].color);
            currentColor = -1;
        }

        if (con.searchStarted && row == con.searchLine) {
            re.SetColor(con_colors[CC_SEARCH].color);
            Con_DrawCharScaled(con.xadjust, y, charW, charH, 141);
            currentColor = -1;
        }

        for (x = 0; x < con.linewidth; x++) {
            const int newColor = (markRow && x >= markStartX && x <= markEndX) ? -2 : Con_RetailColor(text[x] >> 8);

            if ((text[x] & 0xff) == ' ') {
                continue;
            }

            if (newColor != currentColor) {
                currentColor = newColor;
                re.SetColor(currentColor == -2 ? con_colors[CC_MKTEXT].color : g_color_table[currentColor]);
            }
            Con_DrawCharScaled(con.xadjust + (x + 1) * charW, y, charW, charH, text[x] & 0xff);
        }
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

    // if disconnected, render console full screen
    if (clc.state == CA_DISCONNECTED) {
        if (!(Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME))) {
            Con_DrawSolidConsole(1.0);
            return;
        }
    }

    if (con.displayFrac) {
        Con_DrawSolidConsole(con.displayFrac);
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
