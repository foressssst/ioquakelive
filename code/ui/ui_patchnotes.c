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
// ui_patchnotes.c -- [ioquakelive] the Patch Notes page
//
// Retail has no such page: this is ours. The layout follows the one
// quakeclub.com uses for its own patch notes, which is an index on the left and
// the whole note on the right: a narrow column with the versions, each a title
// and its short date, and beside it the version that is open, with its date and
// a gold "Latest" pill on the newest one. Sections inside a note are a small
// gold heading with its bullets under it.
//
// That shape solves the same three things it solves on the site - the page never
// changes height, a version can be reached without scrolling past the others,
// and every block is a panel of the system.
//
// The content is content/pak01/changelog.txt, so a release can change it without
// touching code. One mark per line:
//
//     #  the version's title
//     @  its date, YYYY-MM-DD
//     ## a section heading
//     -  a bullet
//
// A bullet wraps to the column and carries on indented. **bold** turns the words
// between the stars bright, which is how the site marks the important part of a
// line.
//
#include "ui_web.h"

#define PN_PAD 25.0f
#define PN_INDEX_W 260.0f
#define PN_ROW_H 46.0f      // a version in the index
#define PN_LINE_H 21.0f     // a wrapped line of a bullet
#define PN_HEAD_H 34.0f     // a section heading
#define PN_GAP 10.0f        // between a section and the one above
#define PN_BULLET_GAP 6.0f  // between bullets

#define PN_MAX_VERSIONS 64
#define PN_MAX_LINES 1024
#define PN_MAX_TEXT 16384

typedef enum {
    PN_HIT_NONE,
    PN_HIT_VERSION,
    PN_HIT_TRACK,
    PN_HIT_THUMB
} pnHitType_t;

typedef enum {
    PN_LINE_HEAD,
    PN_LINE_BULLET
} pnLineType_t;

typedef struct {
    int type;
    const char* text;
} pnLine_t;

typedef struct {
    const char* title;
    const char* date;   // as it came, YYYY-MM-DD
    int firstLine;
    int numLines;
} pnVersion_t;

static struct {
    char text[PN_MAX_TEXT];  // the file, cut into strings in place
    pnVersion_t versions[PN_MAX_VERSIONS];
    pnLine_t lines[PN_MAX_LINES];
    int numVersions;
    int numLines;
    qboolean loaded;

    int open;  // the version being shown
    float listTop, view, content;
    float scroll;
    qboolean scrollDrag;
    float scrollGrab;
} pn;

/*
===============
PN_Load

Reads the changelog once and cuts it into versions and lines, in place: every
string points into pn.text, so nothing is allocated per line.
===============
*/
static void PN_Load(void) {
    fileHandle_t f;
    int len;
    char* p;

    pn.loaded = qtrue;
    pn.numVersions = 0;
    pn.numLines = 0;

    len = trap_FS_FOpenFile("changelog.txt", &f, FS_READ);
    if (!f || len <= 0 || len >= PN_MAX_TEXT) {
        if (f) {
            trap_FS_FCloseFile(f);
        }
        return;
    }
    trap_FS_Read(pn.text, len, f);
    trap_FS_FCloseFile(f);
    pn.text[len] = '\0';

    p = pn.text;
    while (*p) {
        char* line = p;
        char* end;

        // one line, terminated in place
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
            continue;  // a comment
        }

        if (line[0] == '#' && line[1] == '#' && line[2] == ' ') {
            if (pn.numVersions > 0 && pn.numLines < PN_MAX_LINES) {
                pn.lines[pn.numLines].type = PN_LINE_HEAD;
                pn.lines[pn.numLines].text = line + 3;
                pn.versions[pn.numVersions - 1].numLines++;
                pn.numLines++;
            }
        } else if (line[0] == '#' && line[1] == ' ') {
            if (pn.numVersions < PN_MAX_VERSIONS) {
                pn.versions[pn.numVersions].title = line + 2;
                pn.versions[pn.numVersions].date = "";
                pn.versions[pn.numVersions].firstLine = pn.numLines;
                pn.versions[pn.numVersions].numLines = 0;
                pn.numVersions++;
            }
        } else if (line[0] == '@' && line[1] == ' ') {
            if (pn.numVersions > 0) {
                pn.versions[pn.numVersions - 1].date = line + 2;
            }
        } else if (line[0] == '-' && line[1] == ' ') {
            if (pn.numVersions > 0 && pn.numLines < PN_MAX_LINES) {
                pn.lines[pn.numLines].type = PN_LINE_BULLET;
                pn.lines[pn.numLines].text = line + 2;
                pn.versions[pn.numVersions - 1].numLines++;
                pn.numLines++;
            }
        } else if (line[0] && pn.numLines > 0 && pn.numVersions > 0 &&
                   pn.lines[pn.numLines - 1].type == PN_LINE_BULLET) {
            // a bullet continued on the next line: the file wraps its own text, and
            // the parser does not care where. Joining here would need a buffer, so
            // instead the line becomes a bullet of its own with no mark, which the
            // drawing indents to look like the one above.
            pn.lines[pn.numLines].type = PN_LINE_BULLET;
            pn.lines[pn.numLines].text = line;
            pn.versions[pn.numVersions - 1].numLines++;
            pn.numLines++;
        }
    }
}

/*
===============
PN_DrawRich

A bullet's text, with the words between ** bright and the rest soft. Returns
where it ended, so a wrapped line knows its width.
===============
*/
static void PN_DrawRich(float x, float baseline, const char* text, float maxW, const smRect_t* clip) {
    char chunk[256];
    qboolean bold = qfalse;
    const char* s = text;
    float cx = x;

    while (*s && cx < x + maxW) {
        const char* star = strstr(s, "**");
        int len;

        if (star == s) {
            bold = !bold;
            s += 2;
            continue;
        }
        len = star ? (int)(star - s) : (int)strlen(s);
        if (len > (int)sizeof(chunk) - 1) {
            len = (int)sizeof(chunk) - 1;
        }
        memcpy(chunk, s, len);
        chunk[len] = '\0';

        if (SM_TextInside(baseline, 14, clip)) {
            SM_Text(cx, baseline, FONT_SANS, 14, bold ? smText : smTextSoft, chunk,
                    SM_Fit(FONT_SANS, 14, chunk, x + maxW - cx), qfalse);
        }
        cx += SM_Advance(FONT_SANS, 14, chunk, -1);
        s += len;
    }
}

/*
===============
PN_DateLong

"17 de septiembre de 2026" out of "2026-09-17". The site writes its dates like
this, and the file keeps them sortable.
===============
*/
static const char* PN_DateLong(const char* iso) {
    static const char* const months[12] = {"enero",      "febrero", "marzo",     "abril",
                                           "mayo",       "junio",   "julio",     "agosto",
                                           "septiembre", "octubre", "noviembre", "diciembre"};
    int year, month, day;

    if (!iso || strlen(iso) < 10) {
        return "";
    }
    year = atoi(iso);
    month = atoi(iso + 5);
    day = atoi(iso + 8);
    if (month < 1 || month > 12) {
        return iso;
    }
    return va("%i de %s de %i", day, months[month - 1], year);
}

static const char* PN_DateShort(const char* iso) {
    static const char* const months[12] = {"ene", "feb", "mar", "abr", "may", "jun",
                                           "jul", "ago", "sep", "oct", "nov", "dic"};
    int month, day;

    if (!iso || strlen(iso) < 10) {
        return "";
    }
    month = atoi(iso + 5);
    day = atoi(iso + 8);
    if (month < 1 || month > 12) {
        return iso;
    }
    return va("%02i %s", day, months[month - 1]);
}

static float PN_ClampScroll(float scroll) {
    if (scroll > pn.content - pn.view) {
        scroll = pn.content - pn.view;
    }
    return scroll < 0 ? 0 : scroll;
}

static void PN_DrawScrollbar(float x) {
    float thumbH = pn.view * pn.view / pn.content;
    float thumbY;

    if (thumbH < 28) {
        thumbH = 28;
    }
    thumbY = pn.listTop + (pn.view - thumbH) * pn.scroll / (pn.content - pn.view);
    SM_Hit(x - 4, pn.listTop, 14, pn.view, PN_HIT_TRACK, 0, NULL);
    SM_Pill(x, thumbY, 6, thumbH, (SM_Over(PN_HIT_THUMB, 0) || pn.scrollDrag) ? smMuted : smBorder, NULL);
    SM_Hit(x - 4, thumbY, 14, thumbH, PN_HIT_THUMB, 0, NULL);
}

static void PN_DrawPage(void) {
    float left = UI_Home_Left();
    float y = HM_NAV_H;
    float panelTop, panelH, noteX, noteW;
    smRect_t clip;
    int i;

    UI_Home_Backdrop();
    UI_Home_PageTop(-1);

    y = UI_Home_Header(y, NULL, "Patch Notes");

    // the newest version's date at the header's right, as the site's "last updated"
    if (pn.numVersions > 0) {
        const char* when = va("Ultima actualizacion  %s", PN_DateLong(pn.versions[0].date));
        SM_Label(left + HM_CONTAINER_W - PN_PAD - SM_LabelWidth(11, when, 1.4f),
                 SM_Baseline(FONT_SANS, 11, y - 72, 72), 11, smMuted, when, 1.4f);
    }

    panelTop = y + HM_PANEL_GAP;
    panelH = smBottom - 40 - panelTop;

    if (pn.numVersions == 0) {
        SM_Round(left, panelTop, HM_CONTAINER_W, panelH, SM_RADIUS_MD, smPanel, NULL);
        SM_Text(left + PN_PAD + 12, SM_Baseline(FONT_SANS, 15, panelTop + 30, 24), FONT_SANS, 15, smMuted,
                "No hay notas todavia.", -1, qfalse);
        return;
    }
    if (pn.open < 0 || pn.open >= pn.numVersions) {
        pn.open = 0;
    }

    // ── the index ──
    SM_Round(left, panelTop, PN_INDEX_W, panelH, SM_RADIUS_MD, smPanel, NULL);
    SM_Label(left + 16, SM_Baseline(FONT_SANS, 11, panelTop + 22, 16), 11, smMuted, "Versiones", 1.4f);

    for (i = 0; i < pn.numVersions; i++) {
        float ry = panelTop + 50 + i * PN_ROW_H;
        qboolean over = SM_Over(PN_HIT_VERSION, i);
        qboolean open = (i == pn.open);

        if (ry + PN_ROW_H > panelTop + panelH - 12) {
            break;  // the index does not scroll: it is a handful of versions
        }
        if (open || over) {
            SM_Round(left + 10, ry, PN_INDEX_W - 20, PN_ROW_H - 4, SM_RADIUS_SM,
                     open ? smRaised : smPanel, NULL);
        }
        SM_Text(left + 22, SM_Baseline(FONT_SANS, 13, ry, 26), FONT_SANS, 13, open ? smText : smTextSoft,
                pn.versions[i].title, SM_Fit(FONT_SANS, 13, pn.versions[i].title, PN_INDEX_W - 48), qfalse);
        SM_Label(left + 22, SM_Baseline(FONT_SANS, 10, ry + 22, 16), 10, smMuted,
                 PN_DateShort(pn.versions[i].date), 1.4f);
        SM_Hit(left + 10, ry, PN_INDEX_W - 20, PN_ROW_H - 4, PN_HIT_VERSION, i, NULL);
    }

    // ── the version that is open ──
    noteX = left + PN_INDEX_W + HM_PANEL_GAP;
    noteW = HM_CONTAINER_W - PN_INDEX_W - HM_PANEL_GAP;
    SM_Round(noteX, panelTop, noteW, panelH, SM_RADIUS_MD, smPanel, NULL);

    SM_Text(noteX + PN_PAD, SM_Baseline(FONT_SANS, 17, panelTop + 20, 26), FONT_SANS, 17, smText,
            pn.versions[pn.open].title, SM_Fit(FONT_SANS, 17, pn.versions[pn.open].title, noteW - 2 * PN_PAD - 90),
            qfalse);
    if (pn.open == 0) {
        float bw = SM_LabelWidth(10, "ULTIMA", 1.6f) + 20;
        SM_Pill(noteX + noteW - PN_PAD - bw, panelTop + 22, bw, 20, smGold, NULL);
        SM_Label(noteX + noteW - PN_PAD - bw + 10, SM_Baseline(FONT_SANS, 10, panelTop + 22, 20), 10, smOnPrimary,
                 "ULTIMA", 1.6f);
    }
    SM_Label(noteX + PN_PAD, SM_Baseline(FONT_SANS, 11, panelTop + 48, 16), 11, smMuted,
             PN_DateLong(pn.versions[pn.open].date), 1.4f);

    pn.listTop = panelTop + 78;
    pn.view = panelTop + panelH - PN_PAD - pn.listTop;

    // how tall the open version is, and then its lines
    {
        float ly = 0;
        int n;

        for (n = 0; n < pn.versions[pn.open].numLines; n++) {
            const pnLine_t* ln = &pn.lines[pn.versions[pn.open].firstLine + n];

            if (ln->type == PN_LINE_HEAD) {
                ly += (n ? PN_GAP : 0) + PN_HEAD_H;
            } else {
                ly += PN_LINE_H + PN_BULLET_GAP;
            }
        }
        pn.content = ly;
    }
    pn.scroll = PN_ClampScroll(pn.scroll);

    clip = SM_R(noteX, pn.listTop, noteW, pn.view);
    {
        float ly = pn.listTop - pn.scroll;
        int n;

        for (n = 0; n < pn.versions[pn.open].numLines; n++) {
            const pnLine_t* ln = &pn.lines[pn.versions[pn.open].firstLine + n];

            if (ln->type == PN_LINE_HEAD) {
                ly += (n ? PN_GAP : 0);
                if (ly + PN_HEAD_H > pn.listTop && ly < pn.listTop + pn.view &&
                    SM_TextInside(SM_Baseline(FONT_SANS, 12, ly + 8, 18), 12, &clip)) {
                    SM_Label(noteX + PN_PAD, SM_Baseline(FONT_SANS, 12, ly + 8, 18), 12, smGold, ln->text, 1.4f);
                }
                ly += PN_HEAD_H;
                continue;
            }
            if (ly + PN_LINE_H > pn.listTop && ly < pn.listTop + pn.view) {
                // a line that starts with a mark is a new bullet; one that does not is
                // the one above carrying on, so it lines up past the dot
                qboolean cont = (n > 0 && pn.lines[pn.versions[pn.open].firstLine + n - 1].type == PN_LINE_BULLET &&
                                 ln->text[0] >= 'a' && ln->text[0] <= 'z');
                float tx = noteX + PN_PAD + (cont ? 16 : 0);

                if (!cont && SM_TextInside(SM_Baseline(FONT_SANS, 14, ly, PN_LINE_H), 14, &clip)) {
                    SM_Pill(noteX + PN_PAD + 2, ly + PN_LINE_H * 0.5f - 2, 4, 4, smMuted, &clip);
                }
                PN_DrawRich(tx + (cont ? 0 : 14), SM_Baseline(FONT_SANS, 14, ly, PN_LINE_H), ln->text,
                            noteW - 2 * PN_PAD - 20, &clip);
            }
            ly += PN_LINE_H + PN_BULLET_GAP;
        }
    }

    if (pn.content > pn.view) {
        PN_DrawScrollbar(noteX + noteW - 14);
    }
}

void UI_PatchNotes_Open(void) {
    if (!pn.loaded) {
        PN_Load();
    }
    pn.open = 0;
    pn.scroll = 0;
    pn.scrollDrag = qfalse;
}

void UI_PatchNotes_Paint(void) {
    SM_BeginFrame();

    if (pn.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1)) {
            pn.scrollDrag = qfalse;
        } else if (pn.content > pn.view) {
            float thumbH = pn.view * pn.view / pn.content;

            if (thumbH < 28) {
                thumbH = 28;
            }
            pn.scroll = PN_ClampScroll(pn.scroll +
                                       (smMY - pn.scrollGrab) * (pn.content - pn.view) / (pn.view - thumbH));
            pn.scrollGrab = smMY;
        }
    }

    PN_DrawPage();
    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_PatchNotes_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MWHEELUP:
            pn.scroll = PN_ClampScroll(pn.scroll - 3 * PN_LINE_H);
            return qtrue;
        case K_MWHEELDOWN:
            pn.scroll = PN_ClampScroll(pn.scroll + 3 * PN_LINE_H);
            return qtrue;
        case K_MOUSE1:
            break;
        default:
            return qfalse;  // Escape goes back through the menu's onESC
    }

    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    switch (hit->type) {
        case PN_HIT_VERSION:
            if (hit->index >= 0 && hit->index < pn.numVersions) {
                pn.open = hit->index;
                pn.scroll = 0;
            }
            break;
        case PN_HIT_THUMB:
            pn.scrollDrag = qtrue;
            pn.scrollGrab = smMY;
            break;
        default:
            break;
    }
    return qtrue;
}
