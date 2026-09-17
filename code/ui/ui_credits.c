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
// ui_credits.c -- [ioquakelive] the Credits page
//
// Who made what this is built on: id Software for the game itself, ioquake3 for the
// engine, Thomas Jones for the fork this branched from, and the community projects whose
// ideas and material this uses. The player asked for it so the debt is stated plainly
// rather than assumed.
//
// Same shape as Patch Notes - an index on the left, the section beside it - because the
// front end reads as one design. What this page adds is the link: a line that starts with
// > is drawn in gold and opens in the player's browser when clicked.
//
// The content is content/pak01/credits.txt, so it can be corrected without a build. One
// mark per line:
//
//     #  a section's title
//     @  the line under it
//     ## a sub-heading
//     -  a bullet
//     >  a link, "text | url"
//
#include "ui_web.h"

#define CR_PAD 25.0f
#define CR_INDEX_W 260.0f
#define CR_ROW_H 46.0f
#define CR_LINE_H 21.0f
#define CR_HEAD_H 34.0f
#define CR_LINK_H 26.0f
#define CR_GAP 10.0f
#define CR_BULLET_GAP 6.0f

#define CR_MAX_SECTIONS 32
#define CR_MAX_LINES 512
#define CR_MAX_TEXT 16384
#define CR_MAX_LINKS 32

typedef enum {
    CR_HIT_NONE,
    CR_HIT_SECTION,
    CR_HIT_LINK,
    CR_HIT_TRACK,
    CR_HIT_THUMB
} crHitType_t;

typedef enum {
    CR_LINE_HEAD,
    CR_LINE_BULLET,
    CR_LINE_LINK
} crLineType_t;

typedef struct {
    int type;
    const char* text;  // on a link, the label; the url is in links[]
    int link;          // index into cr.links, -1 otherwise
} crLine_t;

typedef struct {
    const char* title;
    const char* lead;
    int firstLine;
    int numLines;
} crSection_t;

static struct {
    char text[CR_MAX_TEXT];
    crSection_t sections[CR_MAX_SECTIONS];
    crLine_t lines[CR_MAX_LINES];
    const char* links[CR_MAX_LINKS];
    int numSections, numLines, numLinks;
    qboolean loaded;

    int open;
    float listTop, view, content;
    float scroll;
    qboolean scrollDrag;
    float scrollGrab;
} cr;

/*
===============
CR_BulletLines

Cuantas lineas ocupa una vi�eta al ancho de la columna, y donde empieza cada una. El ** de la
negrita no se cuenta al medir, asi que una linea con negrita no queda corta.
===============
*/
static int CR_BulletLines(const char* text, float width, int* starts, int* lens) {
    char plain[512];
    const char* s = text;
    int n = 0;
    int i;

    // el texto sin los ** para medir, guardando a que letra del original corresponde cada una
    static int map[512];

    while (*s && n < (int)sizeof(plain) - 1) {
        if (s[0] == '*' && s[1] == '*') {
            s += 2;
            continue;
        }
        map[n] = (int)(s - text);
        plain[n++] = *s++;
    }
    plain[n] = '\0';
    map[n] = (int)(s - text);

    {
        int count = SM_Wrap(FONT_SANS, 14, plain, width, starts, lens);

        // de vuelta a posiciones del texto original, que es el que se dibuja
        for (i = 0; i < count; i++) {
            int from = starts[i];
            int to = starts[i] + lens[i];

            starts[i] = map[from];
            lens[i] = map[to] - map[from];
        }
        return count;
    }
}

/*
===============
CR_Load

Reads the file once and cuts it into sections and lines, in place. A link's line is
"text | url": the bar is terminated so the label ends there, and the url goes to links[].
===============
*/
static void CR_Load(void) {
    fileHandle_t f;
    int len;
    char* p;

    cr.loaded = qtrue;
    cr.numSections = 0;
    cr.numLines = 0;
    cr.numLinks = 0;

    len = trap_FS_FOpenFile("credits.txt", &f, FS_READ);
    if (!f || len <= 0 || len >= CR_MAX_TEXT) {
        if (f) {
            trap_FS_FCloseFile(f);
        }
        return;
    }
    trap_FS_Read(cr.text, len, f);
    trap_FS_FCloseFile(f);
    cr.text[len] = '\0';

    p = cr.text;
    while (*p) {
        char* line = p;
        char* end;

        while (*p && *p != '\n' && *p != '\r') {
            p++;
        }
        end = p;
        while (*p == '\n' || *p == '\r') {
            p++;
        }
        *end = '\0';

        while (*line == ' ' || *line == '\t') {
            line++;
        }
        if (line[0] == '/' && line[1] == '/') {
            continue;
        }

        if (line[0] == '#' && line[1] == '#' && line[2] == ' ') {
            if (cr.numSections > 0 && cr.numLines < CR_MAX_LINES) {
                cr.lines[cr.numLines].type = CR_LINE_HEAD;
                cr.lines[cr.numLines].text = line + 3;
                cr.lines[cr.numLines].link = -1;
                cr.sections[cr.numSections - 1].numLines++;
                cr.numLines++;
            }
        } else if (line[0] == '#' && line[1] == ' ') {
            if (cr.numSections < CR_MAX_SECTIONS) {
                cr.sections[cr.numSections].title = line + 2;
                cr.sections[cr.numSections].lead = "";
                cr.sections[cr.numSections].firstLine = cr.numLines;
                cr.sections[cr.numSections].numLines = 0;
                cr.numSections++;
            }
        } else if (line[0] == '@' && line[1] == ' ') {
            if (cr.numSections > 0) {
                cr.sections[cr.numSections - 1].lead = line + 2;
            }
        } else if (line[0] == '>' && line[1] == ' ') {
            if (cr.numSections > 0 && cr.numLines < CR_MAX_LINES && cr.numLinks < CR_MAX_LINKS) {
                char* bar = strchr(line, '|');

                cr.lines[cr.numLines].type = CR_LINE_LINK;
                cr.lines[cr.numLines].text = line + 2;
                cr.lines[cr.numLines].link = -1;
                if (bar) {
                    char* url = bar + 1;

                    while (bar > line && (bar[-1] == ' ' || bar[-1] == '\t')) {
                        bar--;
                    }
                    *bar = '\0';
                    while (*url == ' ' || *url == '\t') {
                        url++;
                    }
                    cr.links[cr.numLinks] = url;
                    cr.lines[cr.numLines].link = cr.numLinks;
                    cr.numLinks++;
                }
                cr.sections[cr.numSections - 1].numLines++;
                cr.numLines++;
            }
        } else if (line[0] == '-' && line[1] == ' ') {
            if (cr.numSections > 0 && cr.numLines < CR_MAX_LINES) {
                cr.lines[cr.numLines].type = CR_LINE_BULLET;
                cr.lines[cr.numLines].text = line + 2;
                cr.lines[cr.numLines].link = -1;
                cr.sections[cr.numSections - 1].numLines++;
                cr.numLines++;
            }
        } else if (line[0] && cr.numLines > 0 && cr.numSections > 0 &&
                   cr.lines[cr.numLines - 1].type == CR_LINE_BULLET) {
            // la vi�eta sigue en esta linea del archivo: se empalma sobre el terminador que se
            // acaba de escribir, asi la vi�eta queda en una sola cadena y se reparte al dibujar
            char* prev = (char*)cr.lines[cr.numLines - 1].text;
            int used = (int)strlen(prev);

            if (prev + used == end) {
                prev[used] = ' ';  // el '\0' de esta linea vuelve a ser el espacio que era
            }
        }
    }
}

// a bullet's text, with what sits between ** bright
static void CR_DrawRichRange(float x, float baseline, const char* text, int from, int count,
                             const smRect_t* clip) {
    char chunk[256];
    qboolean bold = qfalse;
    const char* s = text + from;
    const char* stop = s + count;
    float cx = x;

    // la negrita de una vi�eta larga no se reinicia por linea: se cuenta desde el principio
    {
        const char* q;

        for (q = text; q < text + from; q++) {
            if (q[0] == '*' && q[1] == '*') {
                bold = !bold;
                q++;
            }
        }
    }

    while (s < stop) {
        const char* star = strstr(s, "**");
        int n;

        if (star == s) {
            bold = !bold;
            s += 2;
            continue;
        }
        n = star && star < stop ? (int)(star - s) : (int)(stop - s);
        if (n > (int)sizeof(chunk) - 1) {
            n = (int)sizeof(chunk) - 1;
        }
        memcpy(chunk, s, n);
        chunk[n] = '\0';

        if (SM_TextInside(baseline, 14, clip)) {
            SM_Text(cx, baseline, FONT_SANS, 14, bold ? smText : smTextSoft, chunk, -1, qfalse);
        }
        cx += SM_Advance(FONT_SANS, 14, chunk, -1);
        s += n;
    }
}

static float CR_ClampScroll(float scroll) {
    if (scroll > cr.content - cr.view) {
        scroll = cr.content - cr.view;
    }
    return scroll < 0 ? 0 : scroll;
}

static void CR_DrawScrollbar(float x) {
    float thumbH = cr.view * cr.view / cr.content;
    float thumbY;

    if (thumbH < 28) {
        thumbH = 28;
    }
    thumbY = cr.listTop + (cr.view - thumbH) * cr.scroll / (cr.content - cr.view);
    SM_Hit(x - 4, cr.listTop, 14, cr.view, CR_HIT_TRACK, 0, NULL);
    SM_Pill(x, thumbY, 6, thumbH, (SM_Over(CR_HIT_THUMB, 0) || cr.scrollDrag) ? smMuted : smBorder, NULL);
    SM_Hit(x - 4, thumbY, 14, thumbH, CR_HIT_THUMB, 0, NULL);
}

static void CR_DrawPage(void) {
    float left = UI_Home_Left();
    float y = HM_NAV_H;
    float panelTop, panelH, noteX, noteW;
    smRect_t clip;
    int i;

    UI_Home_Backdrop();
    UI_Home_PageTop(-1);

    y = UI_Home_Header(y, NULL, "Credits");

    panelTop = y + HM_PANEL_GAP;
    panelH = smBottom - 40 - panelTop;

    if (cr.numSections == 0) {
        SM_Round(left, panelTop, HM_CONTAINER_W, panelH, SM_RADIUS_MD, smPanel, NULL);
        SM_Text(left + CR_PAD + 12, SM_Baseline(FONT_SANS, 15, panelTop + 30, 24), FONT_SANS, 15, smMuted,
                "No se pudo leer credits.txt.", -1, qfalse);
        return;
    }
    if (cr.open < 0 || cr.open >= cr.numSections) {
        cr.open = 0;
    }

    // ── the index ──
    SM_Round(left, panelTop, CR_INDEX_W, panelH, SM_RADIUS_MD, smPanel, NULL);
    SM_Label(left + 16, SM_Baseline(FONT_SANS, 11, panelTop + 22, 16), 11, smMuted, "Secciones", 1.4f);

    for (i = 0; i < cr.numSections; i++) {
        float ry = panelTop + 50 + i * CR_ROW_H;
        qboolean over = SM_Over(CR_HIT_SECTION, i);
        qboolean open = (i == cr.open);

        if (ry + CR_ROW_H > panelTop + panelH - 12) {
            break;
        }
        if (open || over) {
            SM_Round(left + 10, ry, CR_INDEX_W - 20, CR_ROW_H - 4, SM_RADIUS_SM, open ? smRaised : smPanel, NULL);
        }
        SM_Text(left + 22, SM_Baseline(FONT_SANS, 13, ry, 26), FONT_SANS, 13, open ? smText : smTextSoft,
                cr.sections[i].title, SM_Fit(FONT_SANS, 13, cr.sections[i].title, CR_INDEX_W - 48), qfalse);
        SM_Text(left + 22, SM_Baseline(FONT_SANS, 11, ry + 22, 16), FONT_SANS, 11, smMuted, cr.sections[i].lead,
                SM_Fit(FONT_SANS, 11, cr.sections[i].lead, CR_INDEX_W - 48), qfalse);
        SM_Hit(left + 10, ry, CR_INDEX_W - 20, CR_ROW_H - 4, CR_HIT_SECTION, i, NULL);
    }

    // ── the open section ──
    noteX = left + CR_INDEX_W + HM_PANEL_GAP;
    noteW = HM_CONTAINER_W - CR_INDEX_W - HM_PANEL_GAP;
    SM_Round(noteX, panelTop, noteW, panelH, SM_RADIUS_MD, smPanel, NULL);

    SM_Text(noteX + CR_PAD, SM_Baseline(FONT_SANS, 17, panelTop + 20, 26), FONT_SANS, 17, smText,
            cr.sections[cr.open].title, -1, qfalse);
    SM_Label(noteX + CR_PAD, SM_Baseline(FONT_SANS, 11, panelTop + 48, 16), 11, smMuted, cr.sections[cr.open].lead,
             1.4f);

    cr.listTop = panelTop + 78;
    cr.view = panelTop + panelH - CR_PAD - cr.listTop;

    {
        float ly = 0;
        int n;

        for (n = 0; n < cr.sections[cr.open].numLines; n++) {
            const crLine_t* ln = &cr.lines[cr.sections[cr.open].firstLine + n];

            if (ln->type == CR_LINE_HEAD) {
                ly += (n ? CR_GAP : 0) + CR_HEAD_H;
            } else if (ln->type == CR_LINE_LINK) {
                ly += CR_LINK_H;
            } else {
                int st[SM_MAX_LINES], ln2[SM_MAX_LINES];
                int rows = CR_BulletLines(ln->text, noteW - 2 * CR_PAD - 34, st, ln2);

                ly += rows * CR_LINE_H + CR_BULLET_GAP;
            }
        }
        cr.content = ly;
    }
    cr.scroll = CR_ClampScroll(cr.scroll);

    clip = SM_R(noteX, cr.listTop, noteW, cr.view);
    {
        float ly = cr.listTop - cr.scroll;
        int n;

        for (n = 0; n < cr.sections[cr.open].numLines; n++) {
            const crLine_t* ln = &cr.lines[cr.sections[cr.open].firstLine + n];
            qboolean shown = (ly + CR_LINE_H > cr.listTop && ly < cr.listTop + cr.view);

            if (ln->type == CR_LINE_HEAD) {
                ly += (n ? CR_GAP : 0);
                if (ly + CR_HEAD_H > cr.listTop && ly < cr.listTop + cr.view &&
                    SM_TextInside(SM_Baseline(FONT_SANS, 12, ly + 8, 18), 12, &clip)) {
                    SM_Label(noteX + CR_PAD, SM_Baseline(FONT_SANS, 12, ly + 8, 18), 12, smGold, ln->text, 1.4f);
                }
                ly += CR_HEAD_H;
                continue;
            }
            if (ln->type == CR_LINE_LINK) {
                if (shown) {
                    qboolean over = SM_Over(CR_HIT_LINK, ln->link);
                    float tb = SM_Baseline(FONT_SANS, 14, ly, CR_LINK_H);
                    float tw = SM_Advance(FONT_SANS, 14, ln->text, -1);

                    if (SM_TextInside(tb, 14, &clip)) {
                        SM_Text(noteX + CR_PAD + 14, tb, FONT_SANS, 14, over ? smText : smGold, ln->text, -1, qfalse);
                        // underlined while the cursor is on it, the way a link behaves
                        if (over) {
                            SM_Fill(noteX + CR_PAD + 14, ly + CR_LINK_H - 6, tw, 1, smText, &clip);
                        }
                    }
                    if (ln->link >= 0) {
                        SM_Hit(noteX + CR_PAD, ly, tw + 28, CR_LINK_H, CR_HIT_LINK, ln->link, &clip);
                    }
                }
                ly += CR_LINK_H;
                continue;
            }
            {
                int st[SM_MAX_LINES], ln2[SM_MAX_LINES];
                int rows = CR_BulletLines(ln->text, noteW - 2 * CR_PAD - 34, st, ln2);
                int r;

                for (r = 0; r < rows; r++) {
                    float ry = ly + r * CR_LINE_H;

                    if (ry + CR_LINE_H <= cr.listTop || ry >= cr.listTop + cr.view) {
                        continue;
                    }
                    if (r == 0) {
                        SM_Pill(noteX + CR_PAD + 2, ry + CR_LINE_H * 0.5f - 2, 4, 4, smMuted, &clip);
                    }
                    CR_DrawRichRange(noteX + CR_PAD + 16, SM_Baseline(FONT_SANS, 14, ry, CR_LINE_H), ln->text,
                                     st[r], ln2[r], &clip);
                }
                ly += rows * CR_LINE_H + CR_BULLET_GAP;
            }
        }
    }

    if (cr.content > cr.view) {
        CR_DrawScrollbar(noteX + noteW - 14);
    }
}

void UI_Credits_Open(void) {
    if (!cr.loaded) {
        CR_Load();
    }
    cr.open = 0;
    cr.scroll = 0;
    cr.scrollDrag = qfalse;
}

void UI_Credits_Paint(void) {
    SM_BeginFrame();

    if (cr.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1)) {
            cr.scrollDrag = qfalse;
        } else if (cr.content > cr.view) {
            float thumbH = cr.view * cr.view / cr.content;

            if (thumbH < 28) {
                thumbH = 28;
            }
            cr.scroll = CR_ClampScroll(cr.scroll +
                                       (smMY - cr.scrollGrab) * (cr.content - cr.view) / (cr.view - thumbH));
            cr.scrollGrab = smMY;
        }
    }

    CR_DrawPage();
    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Credits_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MWHEELUP:
            cr.scroll = CR_ClampScroll(cr.scroll - 3 * CR_LINE_H);
            return qtrue;
        case K_MWHEELDOWN:
            cr.scroll = CR_ClampScroll(cr.scroll + 3 * CR_LINE_H);
            return qtrue;
        case K_MOUSE1:
            break;
        default:
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
        case CR_HIT_SECTION:
            if (hit->index >= 0 && hit->index < cr.numSections) {
                cr.open = hit->index;
                cr.scroll = 0;
            }
            break;
        case CR_HIT_LINK:
            if (hit->index >= 0 && hit->index < cr.numLinks) {
                trap_Sys_OpenURL(cr.links[hit->index]);
            }
            break;
        case CR_HIT_THUMB:
            cr.scrollDrag = qtrue;
            cr.scrollGrab = smMY;
            break;
        default:
            break;
    }
    return qtrue;
}
