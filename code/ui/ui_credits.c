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
// ui_credits.c -- [ioquakelive] los creditos
//
// No es una pagina del front end, a proposito. Vive abajo en el menu principal, al
// lado del aviso legal, y se abre como una capa sobre el menu. Se cierra con Escape,
// con la cruz, o pinchando fuera.
//
// Es corto a proposito: una linea por credito, a quien y por que, y nada mas. El
// contenido es content/pak01/credits.txt, asi que se corrige sin compilar. Una marca
// por linea:
//
//     -  un credito, "a quien | por que | url". La url es opcional: con ella el nombre
//        se dibuja en oro y abre en el navegador.
//     =  una nota al pie, chica y gris, que se reparte sola si no cabe
//     .  una linea en blanco
//
#include "ui_web.h"

// La columna es angosta para que cada credito entre en su linea, y el cuerpo va en 15:
// el usuario pidio que se leyera comodo, no que entrara todo de una.
#define CR_W 660.0f
#define CR_PAD 36.0f
#define CR_SIZE 15.0f
#define CR_NOTE_SIZE 12.0f
#define CR_ROW_H 34.0f    // lo que ocupa un credito
#define CR_NOTE_H 19.0f   // una linea de una nota
#define CR_NOTE_GAP 8.0f
#define CR_BLANK_H 16.0f
#define CR_HEAD_H 60.0f
#define CR_MARGIN 70.0f   // lo que la columna deja arriba y abajo

#define CR_MAX_LINES 64
#define CR_MAX_TEXT 8192
#define CR_MAX_LINKS 32

typedef enum {
    CR_HIT_NONE,
    CR_HIT_VEIL,   // el fondo: pinchar ahi cierra
    CR_HIT_PANEL,  // la columna: pinchar ahi no hace nada, pero tampoco cierra
    CR_HIT_CLOSE,
    CR_HIT_LINK,
    CR_HIT_TRACK,
    CR_HIT_THUMB
} crHitType_t;

typedef enum {
    CR_LINE_CREDIT,
    CR_LINE_NOTE,
    CR_LINE_BLANK
} crLineType_t;

typedef struct {
    int type;
    const char* who;  // en un credito, a quien; en una nota, el texto
    const char* why;  // en un credito, por que
    int link;         // indice en cr.links, o -1
} crLine_t;

static const vec4_t crVeil = {0.0f, 0.0f, 0.0f, 0.74f};

static struct {
    char text[CR_MAX_TEXT];
    crLine_t lines[CR_MAX_LINES];
    const char* links[CR_MAX_LINKS];
    int numLines, numLinks;
    qboolean loaded;

    qboolean showing;
    float listTop, view, content;
    float scroll;
    qboolean scrollDrag;
    float scrollGrab;
} cr;

// corta la cadena en la primera barra y devuelve lo que sigue, ya sin espacios a los lados
static char* CR_SplitBar(char* s) {
    char* bar = strchr(s, '|');
    char* rest;

    if (!bar) {
        return NULL;
    }
    rest = bar + 1;
    while (bar > s && (bar[-1] == ' ' || bar[-1] == '\t')) {
        bar--;
    }
    *bar = '\0';
    while (*rest == ' ' || *rest == '\t') {
        rest++;
    }
    return *rest ? rest : NULL;
}

/*
===============
CR_Load

Lee el archivo una vez y lo corta en lineas, en el mismo buffer.
===============
*/
static void CR_Load(void) {
    fileHandle_t f;
    int len;
    char* p;

    cr.loaded = qtrue;
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
    while (*p && cr.numLines < CR_MAX_LINES) {
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

        if (line[0] == '.' && line[1] == '\0') {
            // un hueco entre bloques; dos seguidos no separan mas que uno
            if (cr.numLines > 0 && cr.lines[cr.numLines - 1].type != CR_LINE_BLANK) {
                cr.lines[cr.numLines].type = CR_LINE_BLANK;
                cr.lines[cr.numLines].who = "";
                cr.lines[cr.numLines].why = "";
                cr.lines[cr.numLines].link = -1;
                cr.numLines++;
            }
        } else if (line[0] == '-' && line[1] == ' ') {
            char* why = CR_SplitBar(line + 2);
            char* url = why ? CR_SplitBar(why) : NULL;

            cr.lines[cr.numLines].type = CR_LINE_CREDIT;
            cr.lines[cr.numLines].who = line + 2;
            cr.lines[cr.numLines].why = why ? why : "";
            cr.lines[cr.numLines].link = -1;
            if (url && cr.numLinks < CR_MAX_LINKS) {
                cr.links[cr.numLinks] = url;
                cr.lines[cr.numLines].link = cr.numLinks;
                cr.numLinks++;
            }
            cr.numLines++;
        } else if (line[0] == '=' && line[1] == ' ') {
            cr.lines[cr.numLines].type = CR_LINE_NOTE;
            cr.lines[cr.numLines].who = line + 2;
            cr.lines[cr.numLines].why = "";
            cr.lines[cr.numLines].link = -1;
            cr.numLines++;
        }
    }

    // ni la primera ni la ultima linea son un hueco
    while (cr.numLines > 0 && cr.lines[cr.numLines - 1].type == CR_LINE_BLANK) {
        cr.numLines--;
    }
}

// lo que ocupa todo a ese ancho
static float CR_ContentHeight(float width) {
    float total = 0;
    int i;

    for (i = 0; i < cr.numLines; i++) {
        if (cr.lines[i].type == CR_LINE_BLANK) {
            total += CR_BLANK_H;
        } else if (cr.lines[i].type == CR_LINE_NOTE) {
            int st[SM_MAX_LINES], ln[SM_MAX_LINES];

            total += SM_Wrap(FONT_SANS, CR_NOTE_SIZE, cr.lines[i].who, width, st, ln) * CR_NOTE_H + CR_NOTE_GAP;
        } else {
            total += CR_ROW_H;
        }
    }
    return total;
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
    SM_Pill(x, thumbY, 4, thumbH, (SM_Over(CR_HIT_THUMB, 0) || cr.scrollDrag) ? smMuted : smBorder, NULL);
    SM_Hit(x - 5, thumbY, 14, thumbH, CR_HIT_THUMB, 0, NULL);
}

// un credito: el punto, a quien -- en oro si lleva link -- y por que, mas apagado
static void CR_DrawCredit(const crLine_t* ln, float x, float y, const smRect_t* clip) {
    qboolean over = ln->link >= 0 && SM_Over(CR_HIT_LINK, ln->link);
    float baseline = SM_Baseline(FONT_SANS, CR_SIZE, y, CR_ROW_H);
    float nameW = SM_Advance(FONT_SANS, CR_SIZE, ln->who, -1);
    const float* nameColor = ln->link < 0 ? smText : (over ? smText : smGold);

    if (!SM_TextInside(baseline, CR_SIZE, clip)) {
        return;
    }
    SM_Pill(x + 2, y + CR_ROW_H * 0.5f - 2, 4, 4, smBorder, clip);
    SM_Text(x + 16, baseline, FONT_SANS, CR_SIZE, nameColor, ln->who, -1, qfalse);
    if (over) {
        SM_Fill(x + 16, y + CR_ROW_H * 0.5f + CR_SIZE * 0.5f + 3, nameW, 1, smText, clip);
    }
    SM_Text(x + 16 + nameW + 12, baseline, FONT_SANS, CR_SIZE, smTextSoft, ln->why, -1, qfalse);
    if (ln->link >= 0) {
        SM_Hit(x + 16, y + 4, nameW + 6, CR_ROW_H - 8, CR_HIT_LINK, ln->link, clip);
    }
}

/*
===============
UI_Credits_Overlay

La capa, dibujada al final del menu principal para que quede encima de todo y se
quede con el cursor. El menu sigue detras, apagado.
===============
*/
void UI_Credits_Overlay(void) {
    float screenTop, bottom, maxH;
    float x, top, h, textW;
    smRect_t clip;

    if (!cr.showing) {
        return;
    }
    if (!cr.loaded) {
        CR_Load();
    }

    screenTop = -smTop / smScale;
    bottom = smBottom - (UI_Home_InMatch() ? 38.0f : 0.0f);
    // Se centra a mano y no con UI_Home_BlockTop, que nunca sube mas arriba de la barra de
    // las paginas: la capa no la tiene, y con ella la columna se iba por abajo de la pantalla.
    // El margen deja ver el pie de donde se abrio.
    maxH = bottom - screenTop - 2 * CR_MARGIN;
    textW = CR_W - 2 * CR_PAD;

    cr.content = CR_ContentHeight(textW);
    h = cr.content + CR_HEAD_H + CR_PAD;
    if (h > maxH) {
        h = maxH;
    }
    x = (smVW - CR_W) * 0.5f;
    top = screenTop + (bottom - screenTop - h) * 0.5f;

    // el fondo se apaga y se queda con el cursor: nada de abajo se ilumina ni recibe el clic
    SM_Hit(-20, screenTop - 20, smVW + 40, bottom - screenTop + 40, CR_HIT_VEIL, 0, NULL);
    SM_FillScreen(crVeil);

    SM_Shadow(x, top, CR_W, h, SM_RADIUS_MD);
    SM_Round(x, top, CR_W, h, SM_RADIUS_MD, smPanel, NULL);
    SM_Hit(x, top, CR_W, h, CR_HIT_PANEL, 0, NULL);

    SM_Label(x + CR_PAD, SM_Baseline(FONT_SANS, 12, top + 24, 18), 12, smMuted, "Creditos a", 1.8f);
    SM_Fill(x + CR_PAD, top + CR_HEAD_H - 14, CR_W - 2 * CR_PAD, 1, smBorder, NULL);
    {
        qboolean over = SM_Over(CR_HIT_CLOSE, 0);

        SM_Cross(x + CR_W - CR_PAD - 11, top + 26, 11, over ? smText : smMuted);
        SM_Hit(x + CR_W - CR_PAD - 22, top + 14, 36, 36, CR_HIT_CLOSE, 0, NULL);
    }

    cr.listTop = top + CR_HEAD_H;
    cr.view = top + h - CR_PAD * 0.5f - cr.listTop;
    cr.scroll = CR_ClampScroll(cr.scroll);

    clip = SM_R(x, cr.listTop, CR_W, cr.view);
    {
        float ly = cr.listTop - cr.scroll;
        int i;

        for (i = 0; i < cr.numLines; i++) {
            const crLine_t* ln = &cr.lines[i];

            if (ln->type == CR_LINE_BLANK) {
                ly += CR_BLANK_H;
                continue;
            }
            if (ln->type == CR_LINE_NOTE) {
                int st[SM_MAX_LINES], ln2[SM_MAX_LINES];
                int rows = SM_Wrap(FONT_SANS, CR_NOTE_SIZE, ln->who, textW, st, ln2);
                int r;

                for (r = 0; r < rows; r++) {
                    float ry = ly + r * CR_NOTE_H;
                    float baseline = SM_Baseline(FONT_SANS, CR_NOTE_SIZE, ry, CR_NOTE_H);

                    if (SM_TextInside(baseline, CR_NOTE_SIZE, &clip)) {
                        SM_Text(x + CR_PAD, baseline, FONT_SANS, CR_NOTE_SIZE, smSubtle, ln->who + st[r], ln2[r],
                                qfalse);
                    }
                }
                ly += rows * CR_NOTE_H + CR_NOTE_GAP;
                continue;
            }
            CR_DrawCredit(ln, x + CR_PAD, ly, &clip);
            ly += CR_ROW_H;
        }
    }

    if (cr.content > cr.view) {
        CR_DrawScrollbar(x + CR_W - 16);
    }

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
}

void UI_Credits_Open(void) {
    if (!cr.loaded) {
        CR_Load();
    }
    cr.showing = qtrue;
    cr.scroll = 0;
    cr.scrollDrag = qfalse;
}

void UI_Credits_Close(void) {
    cr.showing = qfalse;
    cr.scrollDrag = qfalse;
}

qboolean UI_Credits_IsOpen(void) {
    return cr.showing;
}

/*
===============
UI_Credits_HandleKey

Mientras la capa esta abierta se queda con todo, para que no llegue nada al menu de
atras. Devuelve qfalse solo cuando esta cerrada.
===============
*/
qboolean UI_Credits_HandleKey(int key) {
    const smHit_t* hit;

    if (!cr.showing) {
        return qfalse;
    }
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_ESCAPE:
            UI_Credits_Close();
            return qtrue;
        case K_MWHEELUP:
            cr.scroll = CR_ClampScroll(cr.scroll - 3 * CR_ROW_H);
            return qtrue;
        case K_MWHEELDOWN:
            cr.scroll = CR_ClampScroll(cr.scroll + 3 * CR_ROW_H);
            return qtrue;
        case K_MOUSE1:
            break;
        default:
            return qtrue;
    }

    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    switch (hit->type) {
        case CR_HIT_VEIL:
        case CR_HIT_CLOSE:
            UI_Credits_Close();
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
