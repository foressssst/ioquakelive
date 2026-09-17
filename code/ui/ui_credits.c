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
// Quien hizo aquello sobre lo que esto esta construido: id Software por el juego,
// ioquake3 por el motor, Thomas Jones por el fork del que salio esta rama, y los
// proyectos de la comunidad cuyo material o cuyas ideas se usan.
//
// No es una pagina del front end, a proposito. Vive abajo en el menu principal, al
// lado del aviso legal, y se abre como una capa sobre el menu: texto corrido en una
// sola columna, sin indice y sin clasificar por seccion. Se cierra con Escape, con
// la cruz, o pinchando fuera.
//
// El contenido es content/pak01/credits.txt, asi que se corrige sin compilar. Una
// marca por linea:
//
//     -  un parrafo, con **negrita** si hace falta
//     >  un link, "texto | url"
//     .  una linea en blanco
//
#include "ui_web.h"

// La columna es angosta para que el texto se lea de un tiron, y el cuerpo va en 15 con la
// linea en 26: el usuario pidio que se leyera comodo, no que entrara todo de una.
#define CR_W 780.0f
#define CR_PAD 38.0f
#define CR_SIZE 15.0f
#define CR_LINE_H 26.0f
#define CR_PARA_GAP 20.0f
#define CR_LINK_H 30.0f
#define CR_BLANK_H 14.0f
#define CR_HEAD_H 60.0f
#define CR_MARGIN 70.0f  // lo que la columna deja arriba y abajo

#define CR_MAX_LINES 128
#define CR_MAX_TEXT 16384
#define CR_MAX_LINKS 24

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
    CR_LINE_TEXT,
    CR_LINE_LINK,
    CR_LINE_BLANK
} crLineType_t;

typedef struct {
    int type;
    const char* text;  // en un link, la etiqueta; la url esta en links[]
    int link;          // indice en cr.links, o -1
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

/*
===============
CR_Wrap

Cuantas lineas ocupa un parrafo al ancho de la columna, y donde empieza cada una. El **
de la negrita no se cuenta al medir, asi que una linea con negrita no queda corta.
===============
*/
static int CR_Wrap(const char* text, float width, int* starts, int* lens) {
    char plain[1024];
    static int map[1024];
    const char* s = text;
    int n = 0;
    int i;

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
        int count = SM_Wrap(FONT_SANS, CR_SIZE, plain, width, starts, lens);

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

Lee el archivo una vez y lo corta en lineas, en el mismo buffer. La linea de un link es
"texto | url": la barra se termina para que la etiqueta acabe ahi, y la url va a links[].
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
            // una linea en blanco entre bloques; dos seguidas no separan mas que una
            if (cr.numLines > 0 && cr.lines[cr.numLines - 1].type != CR_LINE_BLANK) {
                cr.lines[cr.numLines].type = CR_LINE_BLANK;
                cr.lines[cr.numLines].text = "";
                cr.lines[cr.numLines].link = -1;
                cr.numLines++;
            }
        } else if (line[0] == '>' && line[1] == ' ') {
            char* bar = strchr(line, '|');

            cr.lines[cr.numLines].type = CR_LINE_LINK;
            cr.lines[cr.numLines].text = line + 2;
            cr.lines[cr.numLines].link = -1;
            if (bar && cr.numLinks < CR_MAX_LINKS) {
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
            cr.numLines++;
        } else if (line[0] == '-' && line[1] == ' ') {
            cr.lines[cr.numLines].type = CR_LINE_TEXT;
            cr.lines[cr.numLines].text = line + 2;
            cr.lines[cr.numLines].link = -1;
            cr.numLines++;
        } else if (line[0] && cr.numLines > 0 && cr.lines[cr.numLines - 1].type == CR_LINE_TEXT) {
            // el parrafo sigue en esta linea del archivo: se empalma sobre el terminador que se
            // acaba de escribir, asi queda en una sola cadena y se reparte al dibujar
            char* prev = (char*)cr.lines[cr.numLines - 1].text;
            int used = (int)strlen(prev);

            if (prev + used == end) {
                prev[used] = ' ';  // el '\0' de esta linea vuelve a ser el espacio que era
            }
        }
    }

    // ni la primera ni la ultima linea son un hueco
    while (cr.numLines > 0 && cr.lines[cr.numLines - 1].type == CR_LINE_BLANK) {
        cr.numLines--;
    }
}

// un trozo de parrafo, con lo que va entre ** mas claro que el resto
static void CR_DrawRichRange(float x, float baseline, const char* text, int from, int count,
                             const smRect_t* clip) {
    char chunk[256];
    qboolean bold = qfalse;
    const char* s = text + from;
    const char* stop = s + count;
    float cx = x;

    // la negrita de un parrafo largo no se reinicia por linea: se cuenta desde el principio
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

        if (SM_TextInside(baseline, CR_SIZE, clip)) {
            SM_Text(cx, baseline, FONT_SANS, CR_SIZE, bold ? smText : smTextSoft, chunk, -1, qfalse);
        }
        cx += SM_Advance(FONT_SANS, CR_SIZE, chunk, -1);
        s += n;
    }
}

// lo que ocupa todo el texto a ese ancho
static float CR_ContentHeight(float width) {
    float total = 0;
    int i;

    for (i = 0; i < cr.numLines; i++) {
        if (cr.lines[i].type == CR_LINE_BLANK) {
            total += CR_BLANK_H;
        } else if (cr.lines[i].type == CR_LINE_LINK) {
            total += CR_LINK_H;
        } else {
            int st[SM_MAX_LINES], ln[SM_MAX_LINES];

            total += CR_Wrap(cr.lines[i].text, width, st, ln) * CR_LINE_H + CR_PARA_GAP;
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

    SM_Label(x + CR_PAD, SM_Baseline(FONT_SANS, 12, top + 24, 18), 12, smMuted, "Credits", 1.8f);
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
            if (ln->type == CR_LINE_LINK) {
                if (ly + CR_LINK_H > cr.listTop && ly < cr.listTop + cr.view) {
                    qboolean over = SM_Over(CR_HIT_LINK, ln->link);
                    float tb = SM_Baseline(FONT_SANS, CR_SIZE, ly, CR_LINK_H);
                    float tw = SM_Advance(FONT_SANS, CR_SIZE, ln->text, -1);

                    if (SM_TextInside(tb, CR_SIZE, &clip)) {
                        SM_Text(x + CR_PAD, tb, FONT_SANS, CR_SIZE, over ? smText : smGold, ln->text, -1, qfalse);
                        // subrayado mientras el cursor esta encima, como se porta un link
                        if (over) {
                            SM_Fill(x + CR_PAD, ly + CR_LINK_H - 5, tw, 1, smText, &clip);
                        }
                    }
                    if (ln->link >= 0) {
                        SM_Hit(x + CR_PAD, ly, tw + 12, CR_LINK_H, CR_HIT_LINK, ln->link, &clip);
                    }
                }
                ly += CR_LINK_H;
                continue;
            }
            {
                int st[SM_MAX_LINES], ln2[SM_MAX_LINES];
                int rows = CR_Wrap(ln->text, textW, st, ln2);
                int r;

                for (r = 0; r < rows; r++) {
                    float ry = ly + r * CR_LINE_H;

                    if (ry + CR_LINE_H <= cr.listTop || ry >= cr.listTop + cr.view) {
                        continue;
                    }
                    CR_DrawRichRange(x + CR_PAD, SM_Baseline(FONT_SANS, CR_SIZE, ry, CR_LINE_H), ln->text, st[r],
                                     ln2[r], &clip);
                }
                ly += rows * CR_LINE_H + CR_PARA_GAP;
            }
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
            cr.scroll = CR_ClampScroll(cr.scroll - 3 * CR_LINE_H);
            return qtrue;
        case K_MWHEELDOWN:
            cr.scroll = CR_ClampScroll(cr.scroll + 3 * CR_LINE_H);
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
