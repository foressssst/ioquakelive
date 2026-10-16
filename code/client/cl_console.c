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
#include "client.h"

int g_console_field_width = 119;

#define NUM_CON_TIMES 4

#define CON_TEXTSIZE 524288

#define CON_FONT_MONO 2
#define CON_CELL_WIDTH 12
#define CON_CELL_HEIGHT 24
#define CON_FONT_SCALE 2.1597f
#define CON_TEXT_LITERAL 0x40000000
#define CON_LINEBUFFER 0x800

#define CON_MAXLINEWIDTH 1000

#define CON_COLOR_CVAR 40
#define CON_COLOR_CMD 41
#define CON_COLOR_VAL 42
#define CON_COLOR_HELP 43

#define DRAWHELP_ENABLE_BIT 1
#define DRAWHELP_NOTFOUND_BIT 2
#define DRAWHELP_MODULES_BIT 4
#define DRAWHELP_ATTRIBS_BIT 8
#define DRAWHELP_MAX 15

#define CON_MAX_LINKS 64
#define CON_MAX_URL 512

#define CON_ANIM_ROWS 256

typedef struct {
    float x, y, w, h;
    char url[CON_MAX_URL];
} conLink_t;

typedef struct {
    qboolean initialized;

    short text[CON_TEXTSIZE];
    int current;
    int x;
    int display;

    int linewidth;
    int totallines;

    float xadjust;

    float finalFrac;

    int vislines;
    float displayFrac;

    int times[NUM_CON_TIMES];
    vec4_t color;

    int rowsVisible;

    char helpText[MAXPRINTMSG];
    int helpX;
    float helpY;
    int helpWidth;
    int helpLines;
    qboolean helpDraw;
    float helpXAdjust;

    char searchPattern[256];
    int searchLine;
    qboolean searchStarted;

    qboolean markMode;
    int markX;
    int markY;
    int markStartX;
    int markStartY;
    qboolean swallowCtrlC;

    conLink_t links[CON_MAX_LINKS];
    int numLinks;
    float mouseX, mouseY;
    qboolean mouseSeen;
    char linkAsked[CON_MAX_URL];
    vec4_t promptOpen;
    vec4_t promptCancel;

    float scrollPos;
    int animDrawn;
    int lineBorn[CON_ANIM_ROWS];
} console_t;

console_t con;

cvar_t* con_background;
cvar_t* con_height;
cvar_t* con_matchlimit;
cvar_t* con_noprint;
cvar_t* con_opacity;
cvar_t* con_scale;
cvar_t* con_speed;
cvar_t* con_animate;
cvar_t* con_timestamps;

cvar_t* con_style;
cvar_t* con_drawHelp;
cvar_t* con_history;

cvar_t* cl_extendedColors;

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

static conColor_t con_colors[CC_COUNT] = {
    {"con_colBG", "0B0B0DFF", qtrue, "RGBA color of the background; its alpha is multiplied by " S_COLOR_CVAR "con_opacity"},
    {"con_colBorder", "FFCC33FF", qtrue, "RGBA color of the border"},
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

static float Con_Scale(void) {
    return con_scale ? con_scale->value : 1.0f;
}

static void Con_DrawStringTTF(int x, int y, const char* text, const float* color, int cellWidth, int yOffset, int flags) {
    if (!text[0]) {
        return;
    }
    if (color) {
        re.SetColor(color);
    }
    re.Font_DrawString(x, y + yOffset, text, CON_FONT_MONO, cellWidth * CON_FONT_SCALE, -1, NULL, flags);
}

static void Con_DrawCharTTF(int x, int y, int cellWidth, int yOffset, int ch) {
    char str[2];

    str[0] = (char)ch;
    str[1] = '\0';
    re.Font_DrawString(x, y + yOffset, str, CON_FONT_MONO, cellWidth * CON_FONT_SCALE, -1, NULL, 1);
}

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

static qboolean Con_ExtendedColors(void) {
    return (cl_extendedColors && cl_extendedColors->integer) ? qtrue : qfalse;
}

static int Con_ColorCode(const char* p) {
    if (p[0] != Q_COLOR_ESCAPE || p[1] == '\0') {
        return -1;
    }
    return Q_ColorCodeIndex((unsigned char)p[1], Con_ExtendedColors());
}

static int Con_RetailColor(int code) {
    if (code >= Q_NUM_COLOR_CODES || (code >= 8 && !Con_ExtendedColors())) {
        return ColorIndex(COLOR_WHITE);
    }
    return code;
}

static void Con_FillRectPx(float x, float y, float w, float h, const float* color) {
    re.SetColor(color);
    re.DrawStretchPic(x, y, w, h, 0, 0, 0, 0, cls.whiteShader);
}

typedef struct {
    float cellW, cellH;
    float fontSize;
    float adv;
    float asc;
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

static qboolean Con_LinkChar(int c) {
    return (c > ' ' && c < 127 && c != '"' && c != '\'' && c != '\\' && c != '<' && c != '>') ? qtrue : qfalse;
}

static const char* const conLinkTlds[] = {
    "com", "net", "org", "info", "io", "gg", "tv", "me", "dev", "app", "xyz", "live", "link", "site", "online",
    "club", "store", "tech", "games", "gl", "ly", "to", "edu", "gov", "cl", "ar", "br", "mx", "pe", "co", "uy",
    "py", "bo", "ec", "ve", "es", "pt", "uk", "de", "fr", "it", "nl", "be", "se", "no", "fi", "dk", "pl", "ru",
    "ua", "cz", "eu", "us", "ca", "au", "nz", "jp", "kr", "cn", "in", NULL
};

static int Con_TextChar(const short* text, int width, int i) {
    return (i >= 0 && i < width) ? (text[i] & 0xff) : 0;
}

static qboolean Con_HostChar(int c) {
    return (isalnum(c) || c == '-') ? qtrue : qfalse;
}

static int Con_SchemeLength(const short* text, int width, int start) {
    static const char* const schemes[] = {"https://", "http://"};
    int i, n;

    for (n = 0; n < (int)ARRAY_LEN(schemes); n++) {
        for (i = 0; schemes[n][i]; i++) {
            if (tolower(Con_TextChar(text, width, start + i)) != schemes[n][i]) {
                break;
            }
        }
        if (!schemes[n][i]) {
            return i;
        }
    }
    return 0;
}

static qboolean Con_BareHost(const short* text, int width, int start, int end) {
    char tld[16];
    int i, k, labelStart = start, dots = 0, tldLen;

    for (i = start; i < end; i++) {
        int c = Con_TextChar(text, width, i);

        if (c == '/' || c == ':' || c == '?' || c == '#') {
            break;
        }
        if (c == '.') {
            if (i == labelStart) {
                return qfalse;
            }
            dots++;
            labelStart = i + 1;
        } else if (!Con_HostChar(c)) {
            return qfalse;
        }
    }
    tldLen = i - labelStart;
    if (!dots || tldLen < 2 || tldLen >= (int)sizeof(tld)) {
        return qfalse;
    }
    for (k = 0; k < tldLen; k++) {
        tld[k] = (char)tolower(Con_TextChar(text, width, labelStart + k));
        if (!isalpha((unsigned char)tld[k])) {
            return qfalse;
        }
    }
    tld[tldLen] = '\0';
    if (dots >= 2 && tolower(Con_TextChar(text, width, start)) == 'w' &&
        tolower(Con_TextChar(text, width, start + 1)) == 'w' &&
        tolower(Con_TextChar(text, width, start + 2)) == 'w' && Con_TextChar(text, width, start + 3) == '.') {
        return qtrue;
    }
    for (k = 0; conLinkTlds[k]; k++) {
        if (!strcmp(tld, conLinkTlds[k])) {
            return qtrue;
        }
    }
    return qfalse;
}

static int Con_LinkLength(const short* text, int width, int start, qboolean* bare) {
    int prev = Con_TextChar(text, width, start - 1);
    int scheme, len;

    *bare = qfalse;
    if (isalnum(prev) || (prev && strchr("@.-_/:%#", prev))) {
        return 0;
    }
    scheme = Con_SchemeLength(text, width, start);
    if (!scheme && !isalnum(Con_TextChar(text, width, start))) {
        return 0;
    }
    len = scheme;
    while (start + len < width && Con_LinkChar(text[start + len] & 0xff)) {
        len++;
    }
    while (len > scheme && strchr(".,;:!?)]}", text[start + len - 1] & 0xff) != NULL) {
        len--;
    }
    if (len <= scheme) {
        return 0;
    }
    if (scheme) {
        return len;
    }
    if (!Con_BareHost(text, width, start, start + len)) {
        return 0;
    }
    *bare = qtrue;
    return len;
}

static int Con_CellColumn(const short* text, int x) {
    int i, col = 0;

    for (i = 1; i <= x; i++) {
        if (((text[i] & 0xff) & 0xc0) != 0x80) {
            col++;
        }
    }
    return col;
}

static void Con_RowLinks(const short* text, int width, float x0, float y, float adv, float asc, float cellH) {
    static const vec4_t underline = {0.86f, 0.86f, 0.90f, 0.90f};
    int x, len, i, n;
    qboolean bare;

    for (x = 0; x < width; x++) {
        float lx, lw;

        len = Con_LinkLength(text, width, x, &bare);
        if (!len) {
            continue;
        }
        lx = x0 + Con_CellColumn(text, x) * adv;
        lw = (Con_CellColumn(text, x + len - 1) - Con_CellColumn(text, x) + 1) * adv;
        if (con.numLinks < CON_MAX_LINKS) {
            conLink_t* l = &con.links[con.numLinks++];

            l->x = lx;
            l->y = y - asc;
            l->w = lw;
            l->h = cellH;
            l->url[0] = '\0';
            if (bare) {
                Q_strncpyz(l->url, "https://", sizeof(l->url));
            }
            n = (int)strlen(l->url);
            for (i = 0; i < len && n < CON_MAX_URL - 1; i++) {
                l->url[n++] = (char)(text[x + i] & 0xff);
            }
            l->url[n] = '\0';
        }
        Con_FillRectPx(lx, y + 2.0f, lw, 1.0f, underline);
        x += len - 1;
    }
}

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

        Con_FillRectPx(r[0], r[1], r[2], r[3], lit ? con_colors[CC_BORDER].color : con_colors[CC_HL].color);
        re.SetColor(lit ? con_colors[CC_MKTEXT].color : con_colors[CC_TEXT].color);
        re.Font_DrawString((int)r[0], (int)(r[1] + m.asc + (bh - m.cellH) * 0.5f), label, CON_FONT_MONO, m.fontSize,
                           -1, NULL, CON_TEXT_LITERAL);
    }

    re.SetColor(con_colors[CC_HELP].color);
    re.Font_DrawString((int)(x + m.adv), (int)(by + m.asc + (bh - m.cellH) * 0.5f), "enter opens it, escape cancels",
                       CON_FONT_MONO, m.fontSize, -1, NULL, CON_TEXT_LITERAL);
    re.SetColor(NULL);
}

int Con_GetStyle(void) {
    return (con_style && con_style->integer == 1) ? 1 : 0;
}

int Con_GetLineWidth(void) {
    return con.linewidth;
}

static int Con_FirstLine(void) {
    const int first = con.current - con.totallines + 1;
    return first > 0 ? first : 0;
}

static int Con_BottomRow(void) {
    return con.x == 0 ? con.display - 1 : con.display;
}

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

void Con_ToggleConsole_Forced(void) {
    if (clc.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE) {
        return;
    }

    Field_ResetCompletionCycle();
    Field_Clear(&g_consoleField);

    g_consoleField.widthInChars = g_console_field_width;

    Con_ClearNotify();
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_CONSOLE);
    con.markMode = qfalse;
}

void Con_ToggleConsole_f(void) {
    if (com_allowConsole && com_allowConsole->integer != 1) {
        Com_Printf("com_allowConsole won't allow toggleconsole command\n");
        return;
    }

    Con_ToggleConsole_Forced();
}

void Con_ToggleMenu_f(void) {
    CL_KeyEvent(K_ESCAPE, qtrue, Sys_Milliseconds());
    CL_KeyEvent(K_ESCAPE, qfalse, Sys_Milliseconds());
}

void Con_MessageMode_f(void) {
    chat_playerNum = -1;
    chat_team = qfalse;
    Field_Clear(&chatField);
    chatField.widthInChars = 30;

    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

void Con_MessageMode2_f(void) {
    chat_playerNum = -1;
    chat_team = qtrue;
    Field_Clear(&chatField);
    chatField.widthInChars = 25;
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

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

void Con_Clear_f(void) {
    int i;

    for (i = 0; i < CON_TEXTSIZE; i++) {
        con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
    }

    Con_Bottom();
    Com_Memset(con.lineBorn, 0, sizeof(con.lineBorn));
    con.scrollPos = con.display;

    con.searchStarted = qfalse;
    con.markMode = qfalse;
}

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

void Con_ClearNotify(void) {
    int i;

    for (i = 0; i < NUM_CON_TIMES; i++) {
        con.times[i] = 0;
    }
}

void Con_CheckResize(void) {
    int i, j, width, oldwidth, oldtotallines, numlines, numchars;
    short tbuf[CON_TEXTSIZE];

    con.xadjust = 0;
    if (con_scale) {
        width = (int)(cls.glconfig.vidWidth / con_scale->value / (double)CON_CELL_WIDTH - 2.0);
    } else {
        width = cls.glconfig.vidWidth / CON_CELL_WIDTH - 2;
    }
    if (width > CON_MAXLINEWIDTH) {
        width = CON_MAXLINEWIDTH;
    }
    if (width < 1) {
        width = 78;
    }

    if (width == con.linewidth)
        return;

    if (width < 1)
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
    Com_Memset(con.lineBorn, 0, sizeof(con.lineBorn));
    con.scrollPos = con.display;

    con.searchStarted = qfalse;
    con.markMode = qfalse;
}

void Cmd_CompleteTxtName(char* args, int argNum) {
    if (argNum == 2) {
        Field_CompleteFilename("", "txt", qfalse, qtrue);
    }
}

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

        x = 0;
        for (i = 0; i < con.linewidth && x < (int)sizeof(text) - 1; i++) {
            text[x++] = line[i] & 0xff;
        }
        text[x] = '\0';

        while (x > 0 && text[x - 1] == ' ') {
            text[--x] = '\0';
        }

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

        if (rawText[0] == ']' && (rawText[1] == '/' || rawText[1] == '\\') &&
            !Q_stricmpn(rawText + 2, "searchconsole ", 14)) {
            continue;
        }

        if (Com_Filter(con.searchPattern, rawText, qfalse)) {
            const int bottom = Con_BottomRow();

            con.searchLine = line;
            if (line > bottom || line <= bottom - con.rowsVisible) {
                con.display = line + (con.x == 0 ? 1 : 0);
                Con_FixPosition();
            }
            return;
        }
    }
}

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

    con_animate = Cvar_Get("con_animate", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_animate, 0, 1, qtrue);

    con_timestamps = Cvar_Get("con_timestamps", "0", CVAR_ARCHIVE);

    con_style = Cvar_Get("con_style", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_style, 0, 1, qtrue);

    con_drawHelp = Cvar_Get("con_drawHelp", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_drawHelp, 0, DRAWHELP_MAX, qtrue);

    con_history = Cvar_Get("con_history", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(con_history, 0, 1, qtrue);

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
    if (!Q_stricmp(con_colors[CC_BORDER].cvar->string, "C01A1AFF")) {
        Cvar_Set(con_colors[CC_BORDER].name, con_colors[CC_BORDER].defaultValue);
    }
    if (!Q_stricmp(con_colors[CC_BG].cvar->string, "0B0B0DEB")) {
        Cvar_Set(con_colors[CC_BG].name, con_colors[CC_BG].defaultValue);
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
                 S_COLOR_VAL "    0 " S_COLOR_HELP "= A color, with the " S_COLOR_CVAR "con_opacity" S_COLOR_HELP " opacity: black, or " S_COLOR_CVAR "con_colBG" S_COLOR_HELP " with " S_COLOR_CVAR "con_style " S_COLOR_VAL "1\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= Console image");
    Cvar_SetHelp("con_height", "console height, as a fraction of the screen");
    Cvar_SetHelp("con_matchlimit", "max. number of matches printed by " S_COLOR_CMD "find");
    Cvar_SetHelp("con_noprint", "disables console printing");
    Cvar_SetHelp("con_opacity", "opacity of the console background");
    Cvar_SetHelp("con_scale", "console text scale");
    Cvar_SetHelp("con_speed", "console opening/closing speed");
    Cvar_SetHelp("con_animate", "the text of con_style 1 fades in line by line and slides as it scrolls");
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

    Cmd_AddCommand("find", Con_Find_f);

    Cmd_AddCommand("searchconsole", Con_BeginSearch_f);

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

void Con_Linefeed(qboolean skipnotify) {
    int i;

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

void CL_ConsolePrint(char* txt) {
    int y, l;
    unsigned char c;
    unsigned short color;
    qboolean skipnotify = qfalse;
    int prev;

    if (!Q_strncmp(txt, "[skipnotify]", 12)) {
        skipnotify = qtrue;
        txt += 12;
    }

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
            if (con.x == 0) {
                con.lineBorn[con.current % CON_ANIM_ROWS] = cls.realtime;
            }
            y = con.current % con.totallines;
            con.text[y * con.linewidth + con.x] = (ColorIndex(COLOR_WHITE) << 8) | *p;
            con.x++;
            p++;
        }
    }

    color = ColorIndex(COLOR_WHITE);

    while ((c = *((unsigned char*)txt)) != 0) {
        if (Q_IsConsoleColorString(txt)) {
            color = CON_COLOR_CVAR + (txt[1] - COLOR_CON_CVAR);
            txt += 2;
            continue;
        }

        if (Con_ColorCode(txt) >= 0) {
            color = Con_ColorCode(txt);
            txt += 2;
            continue;
        }

        for (l = 0; l < con.linewidth; l++) {
            if (txt[l] <= ' ') {
                break;
            }
        }

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
            default:
                if (con.x == 0) {
                    con.lineBorn[con.current % CON_ANIM_ROWS] = cls.realtime;
                }
                y = con.current % con.totallines;
                con.text[y * con.linewidth + con.x] = (color << 8) | c;
                con.x++;
                if (con.x >= con.linewidth)
                    Con_Linefeed(skipnotify);
                break;
        }
    }

    if (con.current >= 0) {
        if (skipnotify) {
            prev = con.current % NUM_CON_TIMES - 1;
            if (prev < 0)
                prev = NUM_CON_TIMES - 1;
            con.times[prev] = 0;
        } else
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
    }
}

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

void Con_DrawInput(void) {
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    const float s = Con_Scale();
    int y;

    if (clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    y = (int)(con.vislines - CON_CELL_HEIGHT * s * 2);

    g_consoleField.widthInChars = g_console_field_width;

    re.SetColor(con.color);
    Con_DrawCharTTF((int)((CON_CELL_WIDTH + con.xadjust) * s), y, (int)(CON_CELL_WIDTH * s), (int)(CON_CELL_HEIGHT * s), ']');

    Con_DrawFieldTTF(&g_consoleField, (int)((2 * CON_CELL_WIDTH + con.xadjust) * s), y,
                     (int)(CON_CELL_WIDTH * s), (int)(CON_CELL_HEIGHT * s), !con.markMode, white);
}

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

static void Con_TextColumns(const char* chars, int count, short* cols) {
    int i, col = -1;

    for (i = 0; i < count; i++) {
        if ((((byte)chars[i]) & 0xc0) != 0x80 || col < 0) {
            col++;
        }
        cols[i] = (short)col;
    }
}
#define CON_RUN_SHADOW 100
#define CON_RUN_MKSHADOW 101
#define CON_RUN_MKTEXT 102

static const conMetrics_t* con_helpMetrics;

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

static float conRowAlpha = 1.0f;

static void Con_DrawRuns(const conMetrics_t* m, float x, float baseline, const char* chars, const short* ids, const short* cols, int count, qboolean shadow) {
    char run[CON_MAXLINEWIDTH + 1];
    const int offset = shadow ? 1 : 0;
    int i = 0;

    while (i < count) {
        const float* color;
        int start, len, id;

        if (chars[i] == ' ' || (((byte)chars[i]) & 0xc0) == 0x80) {
            i++;
            continue;
        }

        id = ids[i];
        start = i;
        len = 0;
        if ((byte)chars[i] >= 0x80) {
            do {
                run[len++] = chars[i++];
            } while (i < count && len < 4 && (((byte)chars[i]) & 0xc0) == 0x80);
        } else {
            while (i < count && len < CON_MAXLINEWIDTH && (byte)chars[i] < 0x80 && (chars[i] == ' ' || ids[i] == id)) {
                run[len++] = chars[i++];
            }
            while (len > 0 && run[len - 1] == ' ') {
                len--;
            }
        }
        run[len] = '\0';

        color = Con_RunColor(id);
        if (color[3] <= 0.0f) {
            continue;
        }
        if (conRowAlpha < 1.0f) {
            vec4_t faded;

            Vector4Copy(color, faded);
            faded[3] *= conRowAlpha;
            re.SetColor(faded);
        } else {
            re.SetColor(color);
        }
        re.Font_DrawString((int)(x + cols[start] * m->adv) + offset, (int)baseline + offset, run, CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
    }
}

static void Con_DrawInputCNQ3(const conMetrics_t* m) {
    field_t* const edit = &g_consoleField;
    const float baseline = con.vislines - m->cellH;
    const float promptX = m->cellW;
    const float textX = promptX + m->adv;
    char chars[MAX_EDIT_LINE + 1];
    short ids[MAX_EDIT_LINE];
    short shadows[MAX_EDIT_LINE];
    short cols[MAX_EDIT_LINE + 1];
    int len, drawLen, prestep, code, i;

    if (clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    edit->widthInChars = con.linewidth > 2 ? con.linewidth - 1 : 1;

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

    Com_Memcpy(chars, edit->buffer + prestep, drawLen);
    chars[drawLen] = ' ';
    Con_TextColumns(chars, drawLen + 1, cols);

    if (edit->acLength > 0) {
        const int start = MAX(edit->acOffset, prestep);
        const int end = MIN(edit->acOffset + edit->acLength, prestep + drawLen);
        if (end > start) {
            Con_FillRectPx(textX + cols[start - prestep] * m->adv, baseline - m->asc, (cols[end - prestep] - cols[start - prestep]) * m->adv, m->cellH, con_colors[CC_HL].color);
        }
    }

    chars[0] = ']';
    ids[0] = ColorIndex(COLOR_WHITE);
    shadows[0] = CON_RUN_SHADOW;
    {
        static const short col0 = 0;
        Con_DrawRuns(m, promptX, baseline, chars, shadows, &col0, 1, qtrue);
        Con_DrawRuns(m, promptX, baseline, chars, ids, &col0, 1, qfalse);
    }

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
    Con_DrawRuns(m, textX, baseline, chars, shadows, cols, drawLen, qtrue);
    Con_DrawRuns(m, textX, baseline, chars, ids, cols, drawLen, qfalse);

    if (!con.markMode && !(cls.realtime & 0x100)) {
        const int at = MIN(MAX(edit->cursor - prestep, 0), drawLen);
        float cursorX = textX + cols[at] * m->adv;
        if (!key_overstrikeMode) {
            cursorX -= m->adv / 2;
        }
        re.SetColor(con_colors[CC_TEXT].color);
        re.Font_DrawString((int)cursorX, (int)baseline, key_overstrikeMode ? "_" : "|", CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
    }
}

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

    for (i = 0; i < (int)m->cellH; i++) {
        Con_FillRectPx(x, y + i, i + 1, 1, bg);
    }
    Con_FillRectPx(x, yh, w, h, bg);
    Con_FillRectPx(x + 1, yh + h + 0, w - 1, 1, border);
    Con_FillRectPx(x + 2, yh + h + 1, w - 2, 1, border);
    Con_FillRectPx(x + w + 0, yh + 1, 1, h + 1, border);
    Con_FillRectPx(x + w + 1, yh + 2, 1, h + 0, border);

    Com_PrintHelp(name, Con_HelpPrintf, qfalse, printModules, printAttribs);
}

static float Con_ScrollOffset(const conMetrics_t* m) {
    const float target = (float)con.display;

    if (!con_animate->integer || cls.realtime - con.animDrawn > 100) {
        con.scrollPos = target;
    }
    con.animDrawn = cls.realtime;
    if (con.scrollPos < target - 4) {
        con.scrollPos = target - 4;
    } else if (con.scrollPos > target + 4) {
        con.scrollPos = target + 4;
    }
    con.scrollPos += (target - con.scrollPos) * MIN(1.0f, cls.realFrametime * 0.03f);
    if (fabs(target - con.scrollPos) < 0.01f) {
        con.scrollPos = target;
    }
    return (target - con.scrollPos) * m->cellH;
}

static void Con_DrawRowCNQ3(const conMetrics_t* m, float x0, float y, int row, qboolean drawMark, int markStartX,
                            int markStartY, int markEndX, int markEndY) {
    char chars[CON_MAXLINEWIDTH];
    short ids[CON_MAXLINEWIDTH];
    short shadows[CON_MAXLINEWIDTH];
    short cols[CON_MAXLINEWIDTH];
    const int count = MIN(con.linewidth, CON_MAXLINEWIDTH);
    const short* text = con.text + (row % con.totallines) * con.linewidth;
    const qboolean markRow = (drawMark && row >= markStartY && row <= markEndY) ? qtrue : qfalse;
    int x;

    conRowAlpha = 1.0f;
    if (con_animate->integer && con.current - row < CON_ANIM_ROWS && con.lineBorn[row % CON_ANIM_ROWS]) {
        conRowAlpha = (cls.realtime - con.lineBorn[row % CON_ANIM_ROWS]) / 150.0f;
        conRowAlpha = conRowAlpha < 0 ? 0 : conRowAlpha > 1 ? 1 : conRowAlpha;
    }

    for (x = 0; x < count; x++) {
        const qboolean marked = (markRow && x >= markStartX && x <= markEndX) ? qtrue : qfalse;
        chars[x] = (char)(text[x] & 0xff);
        ids[x] = marked ? CON_RUN_MKTEXT : (short)(text[x] >> 8);
        shadows[x] = marked ? CON_RUN_MKSHADOW : CON_RUN_SHADOW;
    }
    Con_TextColumns(chars, count, cols);

    if (markRow) {
        const int c0 = cols[MIN(MAX(markStartX, 0), count - 1)];
        const int c1 = cols[MIN(MAX(markEndX, 0), count - 1)];
        Con_FillRectPx(x0 + c0 * m->adv, y - m->asc, (c1 - c0 + 1) * m->adv, m->cellH, con_colors[CC_MKBG].color);
    }

    Con_DrawRuns(m, x0, y, chars, shadows, cols, count, qtrue);

    if (con.searchStarted && row == con.searchLine) {
        re.SetColor(con_colors[CC_SEARCH].color);
        re.Font_DrawString((int)(x0 - m->cellW), (int)y, ">", CON_FONT_MONO, m->fontSize, -1, NULL, CON_TEXT_LITERAL);
    }

    Con_DrawRuns(m, x0, y, chars, ids, cols, count, qfalse);

    Con_RowLinks(text, count, x0, y, m->adv, m->asc, m->cellH);
    conRowAlpha = 1.0f;
}

static void Con_DrawSolidConsoleCNQ3(float frac) {
    conMetrics_t m;
    int i, k, x, row, rows, lines, len;
    int markStartX, markStartY, markEndX, markEndY;
    qboolean drawMark;
    float y, x0, off, limit;

    lines = (int)(cls.glconfig.vidHeight * frac);
    if (lines <= 0) {
        return;
    }
    if (lines > cls.glconfig.vidHeight) {
        lines = cls.glconfig.vidHeight;
    }

    Con_GetMetrics(&m);
    con.xadjust = 0;
    x0 = m.cellW;

    if (con_background->value != 0) {
        re.SetColor(NULL);
        re.DrawStretchPic(0, 0, cls.glconfig.vidWidth, lines - 2, 0, 0, 1, 1, cls.consoleShader);
    } else {
        vec4_t bg;

        Vector4Copy(con_colors[CC_BG].color, bg);
        bg[3] *= con_opacity->value;
        Con_FillRectPx(0, 0, cls.glconfig.vidWidth, lines - 2, bg);
    }
    Con_FillRectPx(0, lines - 2, cls.glconfig.vidWidth, 2, con_colors[CC_BORDER].color);

    len = strlen(FULL_PRODUCT_VERSION);
    Con_DrawStringTTF(cls.glconfig.vidWidth - CON_CELL_WIDTH * len, lines - CON_CELL_HEIGHT / 2 - CON_CELL_HEIGHT,
                      FULL_PRODUCT_VERSION, con_colors[CC_BORDER].color, CON_CELL_WIDTH, CON_CELL_HEIGHT, CON_TEXT_LITERAL);

    con.vislines = lines;
    rows = (int)((lines - m.cellH) / m.cellH);
    y = lines - m.cellH * 3;

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

    off = Con_ScrollOffset(&m);
    limit = y + (con.display == con.current ? m.cellH : 0);
    for (k = 1; off < 0 && k <= 4; k++) {
        const float by = y + k * m.cellH + off;

        if (row + k > con.current || (row + k == con.current && con.x == 0) || by > limit + 0.5f) {
            break;
        }
        Con_DrawRowCNQ3(&m, x0, by, row + k, drawMark, markStartX, markStartY, markEndX, markEndY);
    }

    con.rowsVisible = 0;
    for (i = 0; i < rows + (off > 0 ? 1 : 0); i++, y -= m.cellH, row--) {
        if (row < 0) {
            break;
        }
        if (con.current - row >= con.totallines) {
            continue;
        }
        if (i < rows && y - m.asc >= 0) {
            con.rowsVisible++;
        }
        if (y + off > limit + 0.5f) {
            continue;
        }
        Con_DrawRowCNQ3(&m, x0, y + off, row, drawMark, markStartX, markStartY, markEndX, markEndY);
    }

    Con_DrawInputCNQ3(&m);
    Con_DrawHelp(&m);

    re.SetColor(NULL);
}

void Con_DrawSolidConsole(float frac) {
    const float s = Con_Scale();
    char line[CON_LINEBUFFER];
    conMetrics_t m = {0};
    vec4_t color;
    int i, x, y, lines, rows, row, n, last, currentColor, len;
    int markStartX, markStartY, markEndX, markEndY;
    qboolean drawMark, haveMetrics = qfalse;

    if (Con_GetStyle() == 1) {
        Con_DrawSolidConsoleCNQ3(frac);
        return;
    }

    lines = (int)(cls.glconfig.vidHeight * frac);
    if (lines <= 0) {
        return;
    }
    if (lines > cls.glconfig.vidHeight) {
        lines = cls.glconfig.vidHeight;
    }

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

    color[0] = 1;
    color[1] = color[2] = 0;
    color[3] = con_opacity->value;
    SCR_FillRect(0, y, SCREEN_WIDTH, 2, color);

    re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
    len = strlen(FULL_PRODUCT_VERSION);
    Con_DrawStringTTF(cls.glconfig.vidWidth - CON_CELL_WIDTH * len, lines - CON_CELL_HEIGHT / 2 - CON_CELL_HEIGHT,
                      FULL_PRODUCT_VERSION, NULL, CON_CELL_WIDTH, CON_CELL_HEIGHT, 0);

    con.vislines = lines;
    rows = (int)((lines - CON_CELL_WIDTH * s) / (CON_CELL_WIDTH * s));
    y = (int)(lines - CON_CELL_HEIGHT * s * 3);

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
            continue;
        }
        if (y - CON_CELL_HEIGHT * s >= 0) {
            con.rowsVisible++;
        }

        text = con.text + (row % con.totallines) * con.linewidth;
        markRow = (drawMark && row >= markStartY && row <= markEndY) ? qtrue : qfalse;

        if (haveMetrics && markRow) {
            const int c0 = Con_CellColumn(text, MAX(markStartX, 0));
            const int c1 = Con_CellColumn(text, MIN(markEndX, con.linewidth - 1));
            Con_FillRectPx(CON_CELL_WIDTH * s + con.xadjust + c0 * m.adv, y - m.asc,
                           (c1 - c0 + 1) * m.adv, m.cellH, con_colors[CC_MKBG].color);
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
                code = 0;
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

    Con_DrawInput();

    re.SetColor(NULL);
}

void Con_DrawConsole(void) {
    Con_CheckResize();

    con.numLinks = 0;

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
        if (clc.state == CA_ACTIVE) {
            Con_DrawNotify();
        }
    }
}

void Con_RunConsole(void) {
    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        con.finalFrac = con_height->value;
    } else {
        con.finalFrac = 0;
        con.linkAsked[0] = '\0';
    }

    if (con.finalFrac < con.displayFrac) {
        con.displayFrac -= con_speed->value * cls.realFrametime * 0.001;
        if (con.finalFrac > con.displayFrac)
            con.displayFrac = con.finalFrac;

    } else if (con.finalFrac > con.displayFrac) {
        con.displayFrac += con_speed->value * cls.realFrametime * 0.001;
        if (con.finalFrac < con.displayFrac)
            con.displayFrac = con.finalFrac;
    }

    Con_UpdateColors();
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
    con.finalFrac = 0;
    con.displayFrac = 0;
    con.markMode = qfalse;
}

static int Con_LastMarkLine(void) {
    const int last = con.x == 0 ? con.current - 1 : con.current;
    return last > 0 ? last : 0;
}

static void Con_AutoScrollMarkPosition(void) {
    const int bottom = Con_BottomRow();
    int visible = con.rowsVisible;

    if (con.display == con.current) {
        visible--;
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
                return qfalse;
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

    return qtrue;
}

qboolean Con_IsInMarkMode(void) {
    return con.markMode;
}

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
