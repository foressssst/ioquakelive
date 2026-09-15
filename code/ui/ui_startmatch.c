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
// ui_startmatch.c -- [QL] the Start Match page
//
// Retail Quake Live has no native menu to start a local match. Its web UI does
// it, with the Start Match page of web.pak (module 262, its options in module
// 263): the page reads the engine's factories and arenas through GetFactoryList
// and GetMapList (quakelive_steam.exe 0x432edc and 0x432c2c) and launches the
// match with "map <map> <factory>". This draws that page with the renderer and
// runs it the way the page's code does, quirks included.
//
// Sizes are the page's CSS pixels at the 1920x1080 it was laid out for (it zooms
// by the window height / 1080), measured on the page itself, and the colours are
// its stylesheet's. The web UI's own art is in web.pak, which is not shipped:
// pak00's gametype icons and fonts stand in for it, and a second pass one pixel
// to the right stands in for Noto Sans Bold.
//

#include "ui_local.h"

#define SM_MAX_FACTORIES 1024  // MAX_FACTORIES in the engine
#define SM_MAX_ARENAS 1024     // MAX_ARENA_INFOS
#define SM_MAX_HITS 256
#define SM_MAX_LINES 8
#define SM_MAX_INPUT 200       // the Cvar widget's maxLength

#define SM_VIEW_H 1080.0f      // the page zooms by innerHeight / 1080
#define SM_MIN_VIEW_W 1500.0f  // narrower screens scale it down to keep it whole

// A CSS font-size is the em, and fontstash sizes a font by ascender minus
// descender. For pak00's Noto Sans and Handel Gothic these are that height per
// em and, for the baseline, (ascender - (ascender - descender) / 2) per em: a
// line box lh high has its baseline lh / 2 + size * that below its top.
#define SM_SANS_EM (2789.0f / 2048.0f)
#define SM_SANS_BASE (794.5f / 2048.0f)
#define SM_HANDEL_EM (2289.0f / 2048.0f)
#define SM_HANDEL_BASE (694.5f / 2048.0f)

// The page, centred. Retail draws it in the right column of its 1920x1080 frame,
// 1460 wide from y 110 to 1050, the lobby and friends list on its left.
#define SM_PAGE_W 1460.0f
#define SM_PAGE_TOP 110.0f
#define SM_PAGE_H 940.0f
#define SM_COL_W 692.16f     // .one-half.column: 48% of the 1442 inside the page
#define SM_COLS_Y 54.39f     // under the h1 and its margins
#define SM_ROWS_X 754.83f    // the second column: 5 + 692.16 + 4% of 1442
#define SM_VAL_X 418.3f      // .val in a row: 7.5 + 56.67% + 4% of 677.16
#define SM_VAL_W 266.34f     // 39.33% of 677.16
#define SM_CARD_H 200.0f
#define SM_CARD_STEP 221.33f // a card and its 1em margin
#define SM_LEFT_H 442.66f    // both cards with their margins

#define SM_RGB(r, g, b) {(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, 1.0f}

static const vec4_t smText = SM_RGB(221, 221, 221);        // body
static const vec4_t smYellow = SM_RGB(247, 209, 0);        // h2, what is selected
static const vec4_t smDim = {0.0f, 0.0f, 0.0f, 0.8f};      // .start-match, .widemodal-content, the overlay
static const vec4_t smCard = SM_RGB(44, 41, 41);           // button.card
static const vec4_t smCardHover = SM_RGB(60, 57, 57);
static const vec4_t smRowOdd = SM_RGB(36, 33, 33);         // .cvar:nth-child(odd)
static const vec4_t smRowEven = SM_RGB(28, 25, 25);        // .cvar:nth-child(even)
static const vec4_t smInput = SM_RGB(15, 15, 15);          // input
static const vec4_t smGrey = SM_RGB(51, 51, 51);           // borders, .Select-control, selections, scrollbar
static const vec4_t smFocus = SM_RGB(51, 195, 240);        // input:focus
static const vec4_t smLighter = SM_RGB(68, 68, 68);        // .Select-control:hover, input[type=range]:hover
static const vec4_t smSelectText = SM_RGB(204, 204, 204);  // .Select-placeholder
static const vec4_t smOption = SM_RGB(187, 187, 187);      // .Select-option
static const vec4_t smDark = SM_RGB(34, 34, 34);           // .Select-option.is-focused, input[type=range]
static const vec4_t smArrow = SM_RGB(153, 153, 153);       // .Select-arrow; the thumb and a button hovered
static const vec4_t smLink = SM_RGB(30, 174, 219);         // a
static const vec4_t smLinkHover = SM_RGB(15, 160, 206);    // a:hover
static const vec4_t smButton = SM_RGB(61, 61, 61);         // button
static const vec4_t smButtonHover = SM_RGB(93, 93, 93);
static const vec4_t smButtonBorder = SM_RGB(77, 77, 77);
static const vec4_t smDialog = SM_RGB(30, 30, 30);         // the modal
static const vec4_t smShadow = {0.0f, 0.0f, 0.0f, 0.5f};   // .Select-menu's box-shadow

typedef struct {
    float x, y, w, h;
} smRect_t;

/*
===============================================================================

WHAT THE PAGE OFFERS

===============================================================================
*/

typedef struct {
    int gametype;
    const char* title;
    const char* factory;  // the one the page picks with the gametype
    const char* map;      // the one it falls back on
    const char* icon;     // pak00's HUD icon; the page's own are in web.pak
} smGametype_t;

// module 237's gametypes, in the order the page lists them (it has no Overload)
static const smGametype_t smGametypes[] = {
    {GT_FFA, "Free For All", "ffa", "campgrounds", "ui/assets/hud/ffa.png"},
    {GT_DUEL, "Duel", "duel", "bloodrun", "ui/assets/hud/duel.png"},
    {GT_RACE, "Race", "race", "foolishlegacy", "ui/assets/hud/race.png"},
    {GT_TEAM, "Team Deathmatch", "tdm", "campgrounds", "ui/assets/hud/tdm.png"},
    {GT_CA, "Clan Arena", "ca", "repent", "ui/assets/hud/ca.png"},
    {GT_CTF, "Capture The Flag", "ctf", "spidercrossings", "ui/assets/hud/ctf.png"},
    {GT_1FCTF, "1-Flag CTF", "oneflag", "spidercrossings", "ui/assets/hud/1f.png"},
    {GT_HARVESTER, "Harvester", "har", "spidercrossings", "ui/assets/hud/har.png"},
    {GT_FREEZE, "Freeze Tag", "ft", "coldwar", "ui/assets/hud/ft.png"},
    {GT_DOMINATION, "Domination", "dom", "chemicalreaction", "ui/assets/hud/dom.png"},
    {GT_AD, "Attack & Defend", "ad", "stonekeep", "ui/assets/hud/ad.png"},
    {GT_RR, "Red Rover", "rr", "devilish", "ui/assets/hud/rr.png"},
};

#define SM_NUM_GAMETYPES ((int)ARRAY_LEN(smGametypes))

// getById for anything else: "Unknown Gametype", with no factory or map of its own
static const smGametype_t smUnknownGametype = {-1, "Unknown Gametype", NULL, NULL, NULL};

typedef struct {
    const char* label;
    const char* value;
} smOption_t;

// module 230
static const smOption_t smServerTypes[] = {{"Offline", "0"}, {"LAN", "1"}, {"Internet", "2"}};
static const smOption_t smSkills[] = {
    {"I Can Win", "0"}, {"Bring it On", "1"}, {"Hurt Me Plenty", "2"}, {"Hardcore", "3"}, {"Nightmare", "4"}};
static const smOption_t smMapPools[] = {
    {"Mixed", "mappool.txt"}, {"Clan Arena", "mappool_ca.txt"}, {"Capture the Flag", "mappool_ctf.txt"},
    {"Duel", "mappool_duel.txt"}, {"Free For All", "mappool_ffa.txt"}, {"Race", "mappool_race.txt"},
    {"Team Deathmatch", "mappool_tdm.txt"}};

typedef enum { SM_INPUT, SM_SELECT, SM_RANGE } smWidget_t;

typedef struct {
    const char* cvar;   // as the page names it, in lower case
    const char* title;  // module 229
    smWidget_t widget;
    float min, max, step;
    int decimals;       // the step's
    const smOption_t* options;
    int numOptions;
} smCvar_t;

enum {
    SM_HOSTNAME,
    SM_SERVERTYPE,
    SM_MAXCLIENTS,
    SM_MINPLAYERS,
    SM_SKILL,
    SM_TEAMSIZE,
    SM_PASSWORD,
    SM_READY,
    SM_MAPPOOL,
    SM_NUM_CVARS
};

// module 263's list, in its order
static const smCvar_t smCvars[SM_NUM_CVARS] = {
    {"sv_hostname", "Match Name", SM_INPUT, 0, 0, 0, 0, NULL, 0},
    {"sv_servertype", "Match Type", SM_SELECT, 0, 0, 0, 0, smServerTypes, (int)ARRAY_LEN(smServerTypes)},
    {"sv_maxclients", "Maximum Players", SM_RANGE, 1, 24, 1, 0, NULL, 0},
    {"bot_minplayers", "Minimum Players", SM_RANGE, 0, 8, 1, 0, NULL, 0},
    {"g_spskill", "Bot Skill", SM_SELECT, 0, 0, 0, 0, smSkills, (int)ARRAY_LEN(smSkills)},
    {"teamsize", "Team Size", SM_RANGE, 0, 20, 1, 0, NULL, 0},
    {"g_password", "Match Password", SM_INPUT, 0, 0, 0, 0, NULL, 0},
    {"sv_warmupreadypercentage", "Warmup Ready Percentage", SM_RANGE, 0, 1, 0.01f, 2, NULL, 0},
    {"sv_mappoolfile", "Map Pool", SM_SELECT, 0, 0, 0, 0, smMapPools, (int)ARRAY_LEN(smMapPools)},
};

/*
===============================================================================

STATE

===============================================================================
*/

typedef struct {
    char id[64];
    char title[64];
    char author[64];
    char description[256];
    int basegt;
    int slot;  // the engine's, to read its cvars
} smFactory_t;

typedef struct {
    char sysname[64];
    char name[64];
    int gametypes;  // a bit per base gametype
    qhandle_t preview;
    qboolean previewTried;
    float nameWidth;  // in bold, at nameScale
    float nameScale;
} smArena_t;

static smFactory_t smFactories[SM_MAX_FACTORIES];  // by id, as the page's factories object
static int smNumFactories;
static smArena_t smArenas[SM_MAX_ARENAS];          // by sysname, as its mapdb object
static int smNumArenas;
static int smMapList[SM_MAX_ARENAS];               // the maps dialog's, by name
static int smNumMapList;
static qhandle_t smIcons[SM_NUM_GAMETYPES];
static qhandle_t smDefaultPreview;

typedef enum { SM_DIALOG_NONE, SM_DIALOG_FACTORIES, SM_DIALOG_MAPS } smDialog_t;

static struct {
    qboolean loaded;

    // module 262's state, which it keeps in sessionStorage between visits
    int selectedGametype;
    char selectedFactory[64];
    char selectedMap[64];
    char tempFactory[64];
    char tempMap[64];
    char filter;  // mapStartsWithFilter: 0 for "all"

    // module 263's: the cvars as it last heard of them
    char values[SM_NUM_CVARS][MAX_CVAR_VALUE_STRING];
    char heard[SM_NUM_CVARS][MAX_CVAR_VALUE_STRING];
    char netPort[16];
    qboolean locked[SM_NUM_CVARS];
    char lockedValue[SM_NUM_CVARS][MAX_CVAR_VALUE_STRING];
    char locksFor[64];
    qboolean locksValid;

    // on screen
    smDialog_t dialog;
    float factoryScroll, mapScroll, optionScroll;
    int edit, editCursor, editStart, editTime;
    int select, selectFocus;
    smRect_t selectControl, selectMenu;
    int drag;
    float dragValue;
    smRect_t dragRect;
    int held;  // a slider let go off itself keeps showing where it was left
    char heldValue[32];
    qboolean scrollDrag;
    float scrollGrab;
    smRect_t article;
    float articleContent;
    int mapListGametype;
    int mapListFilter;
} sm;

/*
===============================================================================

DRAWING

===============================================================================
*/

static float smScale;     // screen pixels per CSS pixel
static float smTop;       // the screen y of the frame's top
static float smVW;        // the frame's width in CSS pixels
static float smMX, smMY;  // the cursor in the frame

static void SM_Frame(void) {
    float w = (float)uiInfo.uiDC.glconfig.vidWidth;
    float h = (float)uiInfo.uiDC.glconfig.vidHeight;

    smScale = h / SM_VIEW_H;
    if (w / smScale < SM_MIN_VIEW_W) {
        smScale = w / SM_MIN_VIEW_W;
    }
    smVW = w / smScale;
    smTop = (h - SM_VIEW_H * smScale) * 0.5f;
    smMX = (uiInfo.uiDC.cursorx * uiInfo.uiDC.xscale + uiInfo.uiDC.bias) / smScale;
    smMY = (uiInfo.uiDC.cursory * uiInfo.uiDC.yscale - smTop) / smScale;
}

static smRect_t SM_R(float x, float y, float w, float h) {
    smRect_t r;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static qboolean SM_In(const smRect_t* r, float x, float y) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static qboolean SM_Clip(smRect_t* r, const smRect_t* clip) {
    float x1, y1, x2, y2;

    if (r->w <= 0 || r->h <= 0) {
        return qfalse;
    }
    if (!clip) {
        return qtrue;
    }
    x1 = r->x > clip->x ? r->x : clip->x;
    y1 = r->y > clip->y ? r->y : clip->y;
    x2 = r->x + r->w < clip->x + clip->w ? r->x + r->w : clip->x + clip->w;
    y2 = r->y + r->h < clip->y + clip->h ? r->y + r->h : clip->y + clip->h;
    if (x2 <= x1 || y2 <= y1) {
        return qfalse;
    }
    *r = SM_R(x1, y1, x2 - x1, y2 - y1);
    return qtrue;
}

static void SM_Fill(float x, float y, float w, float h, const float* color, const smRect_t* clip) {
    smRect_t r = SM_R(x, y, w, h);

    if (!SM_Clip(&r, clip)) {
        return;
    }
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(r.x * smScale, smTop + r.y * smScale, r.w * smScale, r.h * smScale, 0, 0, 0, 0,
                          uiInfo.uiDC.whiteShader);
    trap_R_SetColor(NULL);
}

static void SM_FillScreen(const float* color) {
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(0, 0, (float)uiInfo.uiDC.glconfig.vidWidth, (float)uiInfo.uiDC.glconfig.vidHeight, 0, 0, 0, 0,
                          uiInfo.uiDC.whiteShader);
    trap_R_SetColor(NULL);
}

static void SM_Pic(float x, float y, float w, float h, qhandle_t shader, const smRect_t* clip) {
    smRect_t r = SM_R(x, y, w, h);

    if (!shader || !SM_Clip(&r, clip)) {
        return;
    }
    // what the clip leaves, in texture coordinates too
    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(r.x * smScale, smTop + r.y * smScale, r.w * smScale, r.h * smScale, (r.x - x) / w,
                          (r.y - y) / h, (r.x + r.w - x) / w, (r.y + r.h - y) / h, shader);
}

static float SM_Em(int font) {
    return font == FONT_DEFAULT ? SM_HANDEL_EM : SM_SANS_EM;
}

static float SM_Baseline(int font, float size, float lineTop, float lineHeight) {
    return lineTop + lineHeight * 0.5f + size * (font == FONT_DEFAULT ? SM_HANDEL_BASE : SM_SANS_BASE);
}

// whether a line of text at this baseline shows whole inside the clip
static qboolean SM_TextInside(float baseline, float size, const smRect_t* clip) {
    return baseline - size * 1.07f >= clip->y - 1.0f && baseline + size * 0.3f <= clip->y + clip->h + 1.0f;
}

/*
===============
SM_Advance

A string's advance in CSS pixels. fontstash's box ends at the last glyph's ink,
so a bar is measured after the text and taken off again: that keeps trailing
spaces and the side bearings.
===============
*/
static float SM_Advance(int font, float size, const char* text, int len) {
    char buf[MAX_STRING_CHARS];
    float withBar[5], bar[5];
    float px = size * SM_Em(font) * smScale;

    if (len < 0) {
        len = (int)strlen(text);
    }
    if (len <= 0) {
        return 0;
    }
    if (len > (int)sizeof(buf) - 2) {
        len = sizeof(buf) - 2;
    }
    memcpy(buf, text, len);
    buf[len] = '|';
    buf[len + 1] = '\0';
    trap_R_Font_TextExtents(buf, 0, font, px, -1, withBar);
    trap_R_Font_TextExtents("|", 0, font, px, -1, bar);
    return (withBar[2] - bar[2]) / smScale;
}

// the characters of text that fit in width
static int SM_Fit(int font, float size, const char* text, float width) {
    int len = (int)strlen(text);
    int n;

    if (SM_Advance(font, size, text, len) <= width) {
        return len;
    }
    for (n = 1; n < len; n++) {
        if (SM_Advance(font, size, text, n) > width) {
            return n - 1;
        }
    }
    return len;
}

// pak00 has no Noto Sans Bold: a second pass a pixel to the right stands in for it
static int SM_BoldShift(void) {
    int shift = (int)(smScale + 0.5f);

    return shift < 1 ? 1 : shift;
}

static void SM_Text(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
                    qboolean bold) {
    float px = size * SM_Em(font) * smScale;
    int sx = (int)(x * smScale + 0.5f);
    int sy = (int)(smTop + baseline * smScale + 0.5f);

    if (!text || !text[0] || limit == 0) {
        return;
    }
    trap_R_SetColor(color);
    trap_R_Font_DrawString(sx, sy, text, font, px, limit, NULL, 0);
    if (bold) {
        trap_R_Font_DrawString(sx + SM_BoldShift(), sy, text, font, px, limit, NULL, 0);
    }
    trap_R_SetColor(NULL);
}

static void SM_Centered(float middle, float baseline, float size, const float* color, const char* text, qboolean bold) {
    SM_Text(middle - SM_Advance(FONT_SANS, size, text, -1) * 0.5f, baseline, FONT_SANS, size, color, text, -1, bold);
}

// letter-spacing comes after every character, the last one too
static float SM_SpacedWidth(int font, float size, const char* text, float spacing) {
    float w = 0;

    for (; *text; text++) {
        w += SM_Advance(font, size, text, 1) + spacing;
    }
    return w;
}

static void SM_SpacedText(float x, float baseline, int font, float size, const float* color, const char* text,
                          float spacing, qboolean bold) {
    char c[2];

    c[1] = '\0';
    for (; *text; text++) {
        c[0] = *text;
        SM_Text(x, baseline, font, size, color, c, -1, bold);
        x += SM_Advance(font, size, c, 1) + spacing;
    }
}

/*
===============
SM_Wrap

Breaks text into the lines a block of this width shows, at spaces. Returns how
many, with where each starts and how long it is.
===============
*/
static int SM_Wrap(int font, float size, const char* text, float width, int* starts, int* lens) {
    int total = (int)strlen(text);
    int pos = 0, lines = 0;

    if (SM_Advance(font, size, text, total) <= width) {
        starts[0] = 0;
        lens[0] = total;
        return total ? 1 : 0;
    }
    while (pos < total && lines < SM_MAX_LINES) {
        int end, fit = -1;

        // a line does not start with the spaces it broke at
        while (pos < total && text[pos] == ' ') {
            pos++;
        }
        if (pos >= total) {
            break;
        }
        end = pos;
        while (end < total) {
            int next = end;

            while (next < total && text[next] == ' ') {
                next++;
            }
            while (next < total && text[next] != ' ') {
                next++;
            }
            if (fit >= 0 && SM_Advance(font, size, text + pos, next - pos) > width) {
                break;
            }
            fit = next;
            end = next;
        }
        starts[lines] = pos;
        lens[lines] = fit - pos;
        lines++;
        pos = fit;
    }
    return lines;
}

// a CSS border triangle, 10 wide and 5 high
static void SM_Arrow(float x, float y, qboolean up) {
    int k;

    for (k = 0; k < 5; k++) {
        float w = up ? 2.0f * (k + 1) : 10.0f - 2.0f * k;

        SM_Fill(x + (10.0f - w) * 0.5f, y + k, w, 1, smArrow, NULL);
    }
}

/*
===============================================================================

WHAT THE CURSOR IS OVER

Each frame lists what takes the mouse, in the order it is drawn; the last one
under the cursor is on top. Clicks and hovering use the list of the frame
before, which is what was on screen.

===============================================================================
*/

typedef enum {
    SM_HIT_BLOCK,  // takes the mouse off what is under it
    SM_HIT_GAMETYPE_CARD,
    SM_HIT_ARENA_CARD,
    SM_HIT_RESET,
    SM_HIT_INPUT,
    SM_HIT_SELECT,
    SM_HIT_MENU,
    SM_HIT_OPTION,
    SM_HIT_RANGE,
    SM_HIT_LAUNCH,
    SM_HIT_GAMETYPE,
    SM_HIT_FACTORY,
    SM_HIT_FILTER,
    SM_HIT_MAP,
    SM_HIT_CANCEL,
    SM_HIT_OK,
    SM_HIT_TRACK,
    SM_HIT_THUMB
} smHitType_t;

typedef struct {
    smRect_t r;
    smHitType_t type;
    int index;
} smHit_t;

static smHit_t smHits[SM_MAX_HITS], smLastHits[SM_MAX_HITS];
static int smNumHits, smNumLastHits;
static smHit_t smUnder;
static qboolean smHasUnder;

static void SM_Hit(float x, float y, float w, float h, smHitType_t type, int index, const smRect_t* clip) {
    smRect_t r = SM_R(x, y, w, h);

    if (smNumHits >= SM_MAX_HITS || !SM_Clip(&r, clip)) {
        return;
    }
    smHits[smNumHits].r = r;
    smHits[smNumHits].type = type;
    smHits[smNumHits].index = index;
    smNumHits++;
}

static const smHit_t* SM_HitAt(float x, float y) {
    int i;

    for (i = smNumLastHits - 1; i >= 0; i--) {
        if (SM_In(&smLastHits[i].r, x, y)) {
            return &smLastHits[i];
        }
    }
    return NULL;
}

static qboolean SM_Over(smHitType_t type, int index) {
    return smHasUnder && smUnder.type == type && smUnder.index == index;
}

/*
===============================================================================

THE ENGINE'S FACTORIES AND ARENAS

===============================================================================
*/

static void SM_LoadFactories(void) {
    char id[64], basegt[16];
    int count = trap_Factory_Count();
    int i, pos;
    smFactory_t* f;

    smNumFactories = 0;
    for (i = 0; i < count; i++) {
        // GetFactoryList leaves out the hidden ones (0x432f32)
        if (!trap_Factory_Value(i, "id", id, sizeof(id)) || id[0] == '_') {
            continue;
        }
        // and keys the rest by id: sorted, a repeated id keeping the later one
        for (pos = 0; pos < smNumFactories && strcmp(smFactories[pos].id, id) < 0; pos++) {
        }
        if (pos == smNumFactories || strcmp(smFactories[pos].id, id)) {
            if (smNumFactories == SM_MAX_FACTORIES) {
                continue;
            }
            memmove(&smFactories[pos + 1], &smFactories[pos], (smNumFactories - pos) * sizeof(smFactories[0]));
            smNumFactories++;
        }
        f = &smFactories[pos];
        memset(f, 0, sizeof(*f));
        Q_strncpyz(f->id, id, sizeof(f->id));
        trap_Factory_Value(i, "title", f->title, sizeof(f->title));
        trap_Factory_Value(i, "author", f->author, sizeof(f->author));
        trap_Factory_Value(i, "description", f->description, sizeof(f->description));
        trap_Factory_Value(i, "basegt", basegt, sizeof(basegt));
        f->basegt = atoi(basegt);
        f->slot = i;
    }
}

static void SM_LoadArenas(void) {
    char sysname[64], bits[16];
    int count = trap_Arena_Count();
    int i, pos;
    smArena_t* a;

    smNumArenas = 0;
    for (i = 0; i < count; i++) {
        if (!trap_Arena_Value(i, "sysname", sysname, sizeof(sysname)) || !sysname[0]) {
            continue;
        }
        // GetMapList keys them by map: sorted, a repeated map keeping the later one
        for (pos = 0; pos < smNumArenas && strcmp(smArenas[pos].sysname, sysname) < 0; pos++) {
        }
        if (pos == smNumArenas || strcmp(smArenas[pos].sysname, sysname)) {
            if (smNumArenas == SM_MAX_ARENAS) {
                continue;
            }
            memmove(&smArenas[pos + 1], &smArenas[pos], (smNumArenas - pos) * sizeof(smArenas[0]));
            smNumArenas++;
        }
        a = &smArenas[pos];
        memset(a, 0, sizeof(*a));
        Q_strncpyz(a->sysname, sysname, sizeof(a->sysname));
        trap_Arena_Value(i, "name", a->name, sizeof(a->name));
        trap_Arena_Value(i, "gametypes", bits, sizeof(bits));
        a->gametypes = atoi(bits);
    }
}

static smFactory_t* SM_Factory(const char* id) {
    int lo = 0, hi = smNumFactories - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(smFactories[mid].id, id);

        if (!c) {
            return &smFactories[mid];
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

static smArena_t* SM_Arena(const char* sysname) {
    int lo = 0, hi = smNumArenas - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(smArenas[mid].sysname, sysname);

        if (!c) {
            return &smArenas[mid];
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

static const smGametype_t* SM_Gametype(int gametype) {
    int i;

    for (i = 0; i < SM_NUM_GAMETYPES; i++) {
        if (smGametypes[i].gametype == gametype) {
            return &smGametypes[i];
        }
    }
    return &smUnknownGametype;
}

// mapdb[map].gametypes[gametype]
static qboolean SM_Supports(const char* map, int gametype) {
    const smArena_t* arena = SM_Arena(map);

    return arena && gametype >= 0 && gametype < 31 && (arena->gametypes & (1 << gametype));
}

static qhandle_t SM_GametypeIcon(const smGametype_t* gt) {
    int i = (int)(gt - smGametypes);

    if (i < 0 || i >= SM_NUM_GAMETYPES) {
        return 0;
    }
    if (!smIcons[i]) {
        smIcons[i] = trap_R_RegisterShaderNoMip(gt->icon);
    }
    return smIcons[i];
}

// url(sm/<map>.jpg) over url(sm/default.jpg): the default shows where a map has none
static qhandle_t SM_Preview(smArena_t* arena, const char* map) {
    qhandle_t shot;

    if (!smDefaultPreview) {
        smDefaultPreview = trap_R_RegisterShaderNoMip("levelshots/preview/default.jpg");
    }
    if (arena && arena->previewTried) {
        return arena->preview ? arena->preview : smDefaultPreview;
    }
    shot = trap_R_RegisterShaderNoMip(va("levelshots/preview/%s.jpg", map));
    if (arena) {
        arena->preview = shot;
        arena->previewTried = qtrue;
    }
    return shot ? shot : smDefaultPreview;
}

// url(lg/<map>.jpg), with no fallback
static qhandle_t SM_Levelshot(const char* map) {
    static char last[64];
    static qhandle_t shot;
    static qboolean tried;

    if (!tried || strcmp(last, map)) {
        Q_strncpyz(last, map, sizeof(last));
        shot = trap_R_RegisterShaderNoMip(va("levelshots/%s.jpg", map));
        tried = qtrue;
    }
    return shot;
}

static float SM_NameWidth(smArena_t* arena) {
    if (arena->nameScale != smScale) {
        arena->nameWidth = SM_Advance(FONT_SANS, 24, arena->name, -1) + SM_BoldShift() / smScale;
        arena->nameScale = smScale;
    }
    return arena->nameWidth;
}

/*
===============================================================================

THE PAGE'S STATE (module 262)

===============================================================================
*/

static void SM_Defaults(void) {
    // getInitialState
    sm.selectedGametype = GT_FFA;
    Q_strncpyz(sm.selectedFactory, "ffa", sizeof(sm.selectedFactory));
    Q_strncpyz(sm.selectedMap, "campgrounds", sizeof(sm.selectedMap));
    Q_strncpyz(sm.tempFactory, "ffa", sizeof(sm.tempFactory));
    Q_strncpyz(sm.tempMap, "campgrounds", sizeof(sm.tempMap));
    sm.filter = 0;
    sm.mapListGametype = -1;
}

// selectGametype: the gametype takes effect at once; the dialog's factory and map follow it
static void SM_SelectGametype(int gametype) {
    const smGametype_t* gt = SM_Gametype(gametype);

    if (!SM_Supports(sm.tempMap, gametype) && gt->map) {
        Q_strncpyz(sm.tempMap, gt->map, sizeof(sm.tempMap));
    }
    if (gt->factory) {
        Q_strncpyz(sm.tempFactory, gt->factory, sizeof(sm.tempFactory));
    }
    sm.selectedGametype = gametype;
}

static void SM_SelectFactory(const char* id) {
    const smFactory_t* f = SM_Factory(id);

    Q_strncpyz(sm.tempFactory, id, sizeof(sm.tempFactory));
    if (f) {
        const smGametype_t* gt = SM_Gametype(f->basegt);

        if (!SM_Supports(sm.tempMap, f->basegt) && gt->map) {
            Q_strncpyz(sm.tempMap, gt->map, sizeof(sm.tempMap));
        }
    }
}

// confirmFactoryDialog: the map is checked against the selected one, not the dialog's
static void SM_ConfirmFactories(void) {
    const smFactory_t* f;

    Q_strncpyz(sm.selectedFactory, sm.tempFactory, sizeof(sm.selectedFactory));
    f = SM_Factory(sm.selectedFactory);
    if (f) {
        const smGametype_t* gt = SM_Gametype(f->basegt);

        if (!SM_Supports(sm.selectedMap, f->basegt) && gt->map) {
            Q_strncpyz(sm.selectedMap, gt->map, sizeof(sm.selectedMap));
        }
    }
    sm.dialog = SM_DIALOG_NONE;
}

static void SM_CancelFactories(void) {
    Q_strncpyz(sm.tempFactory, sm.selectedFactory, sizeof(sm.tempFactory));
    Q_strncpyz(sm.tempMap, sm.selectedMap, sizeof(sm.tempMap));
    sm.dialog = SM_DIALOG_NONE;
}

static void SM_ConfirmMaps(void) {
    Q_strncpyz(sm.selectedMap, sm.tempMap, sizeof(sm.selectedMap));
    sm.dialog = SM_DIALOG_NONE;
}

static void SM_CancelMaps(void) {
    Q_strncpyz(sm.tempMap, sm.selectedMap, sizeof(sm.tempMap));
    sm.dialog = SM_DIALOG_NONE;
}

// launchMatch: SendGameCommand("map <map> <factory>")
static void SM_Launch(void) {
    trap_Cmd_ExecuteText(EXEC_APPEND, va("map %s %s\n", sm.selectedMap, sm.selectedFactory));
}

// the letters menu: ALL, A to Z and #
static char SM_FilterChar(int k) {
    if (k <= 0) {
        return 0;
    }
    return k == 27 ? '#' : (char)('A' + k - 1);
}

static char SM_Upper(char c) {
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

/*
===============
SM_BuildMapList

The maps of the selected gametype whose name starts with the letter, sorted by
name (lodash's sortBy keeps the mapdb order among equals). "#" means to take
names that start with anything but a letter, but the page's test for it can
never be true: it matches only a name that starts with "#".
===============
*/
static void SM_BuildMapList(void) {
    int i, j;

    if (sm.mapListGametype == sm.selectedGametype && sm.mapListFilter == sm.filter) {
        return;
    }
    sm.mapListGametype = sm.selectedGametype;
    sm.mapListFilter = sm.filter;
    smNumMapList = 0;
    for (i = 0; i < smNumArenas; i++) {
        const smArena_t* a = &smArenas[i];

        if (sm.selectedGametype < 0 || sm.selectedGametype >= 31 || !(a->gametypes & (1 << sm.selectedGametype))) {
            continue;
        }
        if (sm.filter && SM_Upper(a->name[0]) != sm.filter) {
            continue;
        }
        // insertion keeps equal names in mapdb order
        for (j = smNumMapList; j > 0 && strcmp(smArenas[smMapList[j - 1]].name, a->name) > 0; j--) {
            smMapList[j] = smMapList[j - 1];
        }
        smMapList[j] = i;
        smNumMapList++;
    }
}

// the factories dialog's list: the gametype's in id order, its default one first
static int SM_FactoryList(int* list) {
    const smGametype_t* gt = SM_Gametype(sm.selectedGametype);
    const smFactory_t* def = gt->factory ? SM_Factory(gt->factory) : NULL;
    int n = 0, i;

    if (def) {
        list[n++] = (int)(def - smFactories);
    }
    for (i = 0; i < smNumFactories; i++) {
        if (smFactories[i].basegt != sm.selectedGametype) {
            continue;
        }
        if (gt->factory && !strcmp(smFactories[i].id, gt->factory)) {
            continue;
        }
        list[n++] = i;
    }
    return n;
}

/*
===============================================================================

THE OPTIONS (module 263)

===============================================================================
*/

// JavaScript's value == n for a string: its number, "" being 0
static qboolean SM_Equals(const char* value, float n) {
    char* end;
    double d;

    while (*value == ' ' || *value == '\t') {
        value++;
    }
    if (!*value) {
        return n == 0;
    }
    d = strtod(value, &end);
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    return *end == '\0' && d == n;
}

// the value a row shows: a factory's own for a cvar it sets
static const char* SM_Value(int i) {
    return sm.locked[i] ? sm.lockedValue[i] : sm.values[i];
}

// cvarChanged: the match type moves the port, and a map pool reloads
static void SM_Heard(int i) {
    if (i == SM_SERVERTYPE) {
        trap_Cvar_Set("net_port", SM_Equals(sm.values[i], 1) ? "27015" : "27960");
        trap_Cmd_ExecuteText(EXEC_APPEND, "net_restart\n");
    } else if (i == SM_MAPPOOL) {
        trap_Cmd_ExecuteText(EXEC_APPEND, "reload_mappool\n");
    }
}

/*
===============
SM_ReadCvars

The page learns a cvar's value from GetConfig when it opens and from the
engine's event when it changes; both give the latched value while one waits.
A change is an event: it replaces what the page shows, but for a text input
being typed in.
===============
*/
static void SM_ReadCvars(qboolean events) {
    char value[MAX_CVAR_VALUE_STRING];
    int i;

    for (i = 0; i < SM_NUM_CVARS; i++) {
        trap_Cvar_LatchedVariableStringBuffer(smCvars[i].cvar, value, sizeof(value));
        if (events && !strcmp(value, sm.heard[i])) {
            continue;
        }
        Q_strncpyz(sm.heard[i], value, sizeof(sm.heard[i]));
        if (i != sm.edit) {
            Q_strncpyz(sm.values[i], value, sizeof(sm.values[i]));
        }
        if (events) {
            sm.held = -1;
            SM_Heard(i);
        }
    }
    trap_Cvar_LatchedVariableStringBuffer("net_port", sm.netPort, sizeof(sm.netPort));
}

// renderCvar: a cvar the selected factory sets is disabled
static void SM_UpdateLocks(void) {
    char name[MAX_CVAR_VALUE_STRING], value[MAX_CVAR_VALUE_STRING];
    const smFactory_t* f;
    int i, j;

    if (sm.locksValid && !strcmp(sm.locksFor, sm.selectedFactory)) {
        return;
    }
    Q_strncpyz(sm.locksFor, sm.selectedFactory, sizeof(sm.locksFor));
    sm.locksValid = qtrue;
    memset(sm.locked, 0, sizeof(sm.locked));
    f = SM_Factory(sm.selectedFactory);
    if (!f) {
        return;
    }
    for (j = 0; trap_Factory_Cvar(f->slot, j, name, sizeof(name), value, sizeof(value)); j++) {
        for (i = 0; i < SM_NUM_CVARS; i++) {
            if (!strcmp(name, smCvars[i].cvar)) {
                sm.locked[i] = qtrue;
                Q_strncpyz(sm.lockedValue[i], value, sizeof(sm.lockedValue[i]));
            }
        }
    }
}

// onBlurInput: what was typed goes to the cvar
static void SM_EndEdit(void) {
    if (sm.edit < 0) {
        return;
    }
    trap_Cvar_Set(smCvars[sm.edit].cvar, sm.values[sm.edit]);
    sm.edit = -1;
}

// the gap between characters nearest to x, in the text shown from start
static int SM_CaretAt(const char* text, int start, float x) {
    int len = (int)strlen(text);
    int n, best = start;
    float bestDist = 1e9f;

    for (n = start; n <= len; n++) {
        float dist = (float)fabs(SM_Advance(FONT_SANS, 15, text + start, n - start) - x);

        if (dist >= bestDist) {
            break;
        }
        bestDist = dist;
        best = n;
    }
    return best;
}

static void SM_OpenSelect(int i) {
    const smCvar_t* cv = &smCvars[i];
    int k;

    sm.select = i;
    sm.selectFocus = 0;
    for (k = 0; k < cv->numOptions; k++) {
        if (!strcmp(cv->options[k].value, sm.values[i])) {
            sm.selectFocus = k;
        }
    }
    // the menu shows its focused option
    sm.optionScroll = sm.selectFocus * 40.0f + 40.0f - 198.0f;
    if (sm.optionScroll < 0) {
        sm.optionScroll = 0;
    }
}

// where a slider's thumb goes for the cursor at x: the step's nearest value
static float SM_RangeAt(int i, float x) {
    const smCvar_t* cv = &smCvars[i];
    float frac = (x - (sm.dragRect.x + 1) - 8) / (sm.dragRect.w - 2 - 16);
    float value;

    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    value = cv->min + frac * (cv->max - cv->min);
    value = cv->min + (float)floor((value - cv->min) / cv->step + 0.5f) * cv->step;
    return value > cv->max ? cv->max : value;
}

static void SM_FormatRange(int i, float value, char* buf, int size) {
    Com_sprintf(buf, size, smCvars[i].decimals ? "%.2f" : "%.0f", value);
}

/*
===============================================================================

THE PAGE

===============================================================================
*/

static void SM_DrawGametypeCard(float x, float y) {
    const smFactory_t* factory = SM_Factory(sm.selectedFactory);
    const smGametype_t* gt = SM_Gametype(factory ? factory->basegt : -1);
    // a button centres what it holds: here 161 pixels in the 178.67 inside its padding
    float top = y + 10.67f + (178.67f - 161.0f) * 0.5f;
    float middle = x + SM_COL_W * 0.5f;
    const char* line = "";

    SM_Fill(x, y, SM_COL_W, SM_CARD_H, SM_Over(SM_HIT_GAMETYPE_CARD, 0) ? smCardHover : smCard, NULL);
    SM_SpacedText(x + 10.67f, SM_Baseline(FONT_SANS, 24, top, 30), FONT_SANS, 24, smYellow, "Game Type", -1, qfalse);
    SM_Pic(middle - 30, top + 42, 60, 60, SM_GametypeIcon(gt), NULL);
    SM_Centered(middle, SM_Baseline(FONT_SANS, 21.33f, top + 107.67f, 26.67f), 21.33f, smText, gt->title, qfalse);
    // the factory's title, or "Default" when it is the gametype's
    if (factory) {
        line = strcmp(gt->title, factory->title) ? factory->title : "Default";
    }
    SM_Centered(middle, SM_Baseline(FONT_SANS, 21.33f, top + 134.34f, 26.67f), 21.33f, smText, line, qfalse);
    SM_Hit(x, y, SM_COL_W, SM_CARD_H, SM_HIT_GAMETYPE_CARD, 0, NULL);
}

static void SM_DrawArenaCard(float x, float y) {
    smArena_t* arena = SM_Arena(sm.selectedMap);
    float top = y + 10.67f + (178.67f - 174.34f) * 0.5f;
    float middle = x + SM_COL_W * 0.5f;

    SM_Fill(x, y, SM_COL_W, SM_CARD_H, SM_Over(SM_HIT_ARENA_CARD, 0) ? smCardHover : smCard, NULL);
    SM_SpacedText(x + 10.67f, SM_Baseline(FONT_SANS, 24, top, 30), FONT_SANS, 24, smYellow, "Current Arena", -1, qfalse);
    SM_Pic(middle - 89.5f, top + 42, 179, 100, SM_Preview(arena, sm.selectedMap), NULL);
    SM_Centered(middle, SM_Baseline(FONT_SANS, 21.33f, top + 147.67f, 26.67f), 21.33f, smText, arena ? arena->name : "",
                qfalse);
    SM_Hit(x, y, SM_COL_W, SM_CARD_H, SM_HIT_ARENA_CARD, 0, NULL);
}

// a text input: its 1-pixel border, the text from 8 in, and the caret while typing
static void SM_DrawInput(int i, float x, float y) {
    const char* value = sm.values[i];
    float width = SM_VAL_W - 10;
    int len = (int)strlen(value);
    int start = 0;
    qboolean editing = sm.edit == i;

    SM_Fill(x, y, SM_VAL_W, 38, editing ? smFocus : smGrey, NULL);
    SM_Fill(x + 1, y + 1, SM_VAL_W - 2, 36, smInput, NULL);
    if (editing) {
        if (sm.editCursor > len) {
            sm.editCursor = len;
        }
        if (sm.editStart > sm.editCursor) {
            sm.editStart = sm.editCursor;
        }
        while (sm.editStart < sm.editCursor &&
               SM_Advance(FONT_SANS, 15, value + sm.editStart, sm.editCursor - sm.editStart) > width) {
            sm.editStart++;
        }
        start = sm.editStart;
    }
    SM_Text(x + 9, y + 24.82f, FONT_SANS, 15, smText, value + start, SM_Fit(FONT_SANS, 15, value + start, width), qfalse);
    if (editing && ((uiInfo.uiDC.realTime - sm.editTime) / 500) % 2 == 0) {
        SM_Fill(x + 9 + SM_Advance(FONT_SANS, 15, value + start, sm.editCursor - start), y + 9, 1, 20, smText, NULL);
    }
    SM_Hit(x, y, SM_VAL_W, 38, SM_HIT_INPUT, i, NULL);
}

// react-select: the label of the value's option, or its "Default" placeholder
static void SM_DrawSelect(int i, float x, float y) {
    const smCvar_t* cv = &smCvars[i];
    const char* label = "Default";
    qboolean open = sm.select == i;
    int k;

    for (k = 0; k < cv->numOptions; k++) {
        if (!strcmp(cv->options[k].value, sm.values[i])) {
            label = cv->options[k].label;
        }
    }
    SM_Fill(x, y, SM_VAL_W, 36, (!open && SM_Over(SM_HIT_SELECT, i)) ? smLighter : smGrey, NULL);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 8, 24), FONT_SANS, 15, smSelectText, label,
            SM_Fit(FONT_SANS, 15, label, SM_VAL_W - 62), qfalse);
    SM_Arrow(x + 246.34f, y + 18, open);
    SM_Hit(x, y, SM_VAL_W, 36, SM_HIT_SELECT, i, NULL);
    if (open) {
        sm.selectControl = SM_R(x, y, SM_VAL_W, 36);
    }
}

// the value with the step's decimals, then the slider: its border, track and thumb
static void SM_DrawRange(int i, float x, float y) {
    const smCvar_t* cv = &smCvars[i];
    float bx = x + 53.27f, by = y + 9, bw = 213.07f;
    float value, frac;
    char text[32];

    if (sm.drag == i) {
        value = sm.dragValue;
    } else if (sm.held == i) {
        value = (float)atof(sm.heldValue);
    } else {
        value = (float)atof(sm.values[i]);
    }
    SM_FormatRange(i, value, text, sizeof(text));
    SM_Text(x + 8, SM_Baseline(FONT_SANS, 15, y + 10, 16), FONT_SANS, 15, smText, text, -1, qfalse);
    SM_Fill(bx, by, bw, 18, smGrey, NULL);
    SM_Fill(bx + 1, by + 1, bw - 2, 16, SM_Over(SM_HIT_RANGE, i) ? smLighter : smDark, NULL);
    frac = (value - cv->min) / (cv->max - cv->min);
    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    SM_Fill(bx + 1 + frac * (bw - 2 - 16), by + 1, 16, 16, smYellow, NULL);
    SM_Hit(bx, by, bw, 18, SM_HIT_RANGE, i, NULL);
}

// the row itself and the dropdown it opened count as hovering it
static qboolean SM_RowHovered(int i, float x, float y, float h) {
    smRect_t row = SM_R(x, y, SM_COL_W, h);

    if (sm.dialog != SM_DIALOG_NONE) {
        return qfalse;
    }
    if (sm.select >= 0 && SM_In(&sm.selectMenu, smMX, smMY)) {
        return i == sm.select;
    }
    return SM_In(&row, smMX, smMY);
}

static float SM_DrawRow(int i, float x, float y, int nth) {
    const smCvar_t* cv = &smCvars[i];
    float h = sm.locked[i] ? 50.0f : cv->widget == SM_INPUT ? 53.0f : cv->widget == SM_SELECT ? 51.0f : 50.0f;
    float base = SM_Baseline(FONT_SANS, 15, y + 13.5f, 24);
    float after = x + 7.5f + SM_Advance(FONT_SANS, 15, cv->title, -1) + SM_Advance(FONT_SANS, 15, " ", 1);

    SM_Fill(x, y, SM_COL_W, h, (nth & 1) ? smRowOdd : smRowEven, NULL);
    SM_Text(x + 7.5f, base, FONT_SANS, 15, smText, cv->title, -1, qfalse);
    if (sm.locked[i]) {
        // disabled: "(Locked)" and no value, as getWidget reads it from a
        // variable declared further down, still undefined there
        SM_Text(after, base, FONT_SANS, 15, smText, "(Locked)", -1, qfalse);
        return h;
    }
    // a.resetLink shows while the row is hovered
    if (SM_RowHovered(i, x, y, h)) {
        SM_Text(after, base, FONT_SANS, 15, SM_Over(SM_HIT_RESET, i) ? smLinkHover : smLink, "(Reset)", -1, qfalse);
        SM_Hit(after, y + 13.5f, SM_Advance(FONT_SANS, 15, "(Reset)", -1), 24, SM_HIT_RESET, i, NULL);
    }
    if (cv->widget == SM_INPUT) {
        SM_DrawInput(i, x + SM_VAL_X, y + 7.5f);
    } else if (cv->widget == SM_SELECT) {
        SM_DrawSelect(i, x + SM_VAL_X, y + 7.5f);
    } else {
        SM_DrawRange(i, x + SM_VAL_X, y + 7.5f);
    }
    return h;
}

static void SM_DrawLaunch(float x, float y) {
    // .launch: a button half the page wide, centred
    float bx = x + 360.5f, bw = 721, bh = 51.31f;
    float tw = SM_SpacedWidth(FONT_SANS, 21.33f, "Launch Match", 1);
    qboolean over = SM_Over(SM_HIT_LAUNCH, 0);

    SM_Fill(bx, y, bw, bh, over ? smButtonHover : smButton, NULL);
    SM_SpacedText(bx + (bw - tw) * 0.5f, SM_Baseline(FONT_SANS, 21.33f, y + 10.67f, 30), FONT_SANS, 21.33f,
                  over ? smYellow : smText, "Launch Match", 1, qtrue);
    SM_Hit(bx, y, bw, bh, SM_HIT_LAUNCH, 0, NULL);
}

static void SM_DrawPage(void) {
    float page = (smVW - SM_PAGE_W) * 0.5f;
    float cols = SM_PAGE_TOP + SM_COLS_Y;
    float rowsX = page + SM_ROWS_X;
    float y = cols, h;
    int i, nth = 1;

    SM_Fill(page, SM_PAGE_TOP, SM_PAGE_W, SM_PAGE_H, smDim, NULL);
    SM_SpacedText(page + 11.39f, SM_Baseline(FONT_DEFAULT, 32, SM_PAGE_TOP + 8, 38.4f), FONT_DEFAULT, 32, smText,
                  "Start Match", -1, qfalse);
    SM_DrawGametypeCard(page + 5, cols);
    SM_DrawArenaCard(page + 5, cols + SM_CARD_STEP);
    for (i = 0; i < SM_NUM_CVARS; i++) {
        y += SM_DrawRow(i, rowsX, y, nth++);
        // p.lan follows the match type when it is Internet, one more of the rows' siblings
        if (i == SM_SERVERTYPE && SM_Equals(SM_Value(i), 2)) {
            char note[128];

            Com_sprintf(note, sizeof(note),
                        "To host public Internet servers, you will need to forward UDP port %s to your machine.",
                        sm.netPort);
            SM_Text(rowsX + 11.25f, SM_Baseline(FONT_SANS, 15, y + 7.5f, 24), FONT_SANS, 15, smText, note, -1, qfalse);
            y += 39;
            nth++;
        }
    }
    h = y - cols;
    SM_DrawLaunch(page + 5, cols + (h > SM_LEFT_H ? h : SM_LEFT_H) + 15 + 5.33f);
}

// .Select-menu-outer, under the control: options 40 high, at most 198 of them shown
static void SM_DrawDropdown(void) {
    const smCvar_t* cv = &smCvars[sm.select];
    float x = sm.selectControl.x, y = sm.selectControl.y + 35;
    float full = cv->numOptions * 40.0f;
    float h = full < 198 ? full : 198;
    smRect_t box = SM_R(x, y, SM_VAL_W, h);
    int k;

    if (sm.optionScroll > full - h) {
        sm.optionScroll = full - h;
    }
    if (sm.optionScroll < 0) {
        sm.optionScroll = 0;
    }
    SM_Fill(x + 3, y + 3, SM_VAL_W, h, smShadow, NULL);
    SM_Fill(x, y, SM_VAL_W, h, smGrey, NULL);
    SM_Hit(x, y, SM_VAL_W, h, SM_HIT_MENU, sm.select, NULL);
    for (k = 0; k < cv->numOptions; k++) {
        float oy = y + k * 40.0f - sm.optionScroll;
        float base = SM_Baseline(FONT_SANS, 15, oy + 8, 24);

        // the option under the mouse takes the focus
        if (SM_Over(SM_HIT_OPTION, k)) {
            sm.selectFocus = k;
        }
        if (k == sm.selectFocus) {
            SM_Fill(x, oy, SM_VAL_W, 40, smDark, &box);
        }
        if (SM_TextInside(base, 15, &box)) {
            SM_Text(x + 10, base, FONT_SANS, 15, k == sm.selectFocus ? smSelectText : smOption, cv->options[k].label,
                    -1, qfalse);
        }
        SM_Hit(x, oy, SM_VAL_W, 40, SM_HIT_OPTION, k, &box);
    }
    sm.selectMenu = box;
}

/*
===============================================================================

THE DIALOGS

===============================================================================
*/

// the overlay and the modal, 100% wide with the content in its 15% side padding
static smRect_t SM_DrawDialog(float height, qhandle_t background) {
    float top = (SM_VIEW_H - height) * 0.5f;
    float cx = smVW * 0.15f, cw = smVW * 0.7f;
    smRect_t box = SM_R(0, top, smVW, height);

    // the overlay takes the mouse off the page
    SM_FillScreen(smDim);
    SM_Hit(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, SM_HIT_BLOCK, 0, NULL);
    SM_Fill(0, top, smVW, height, smDialog, NULL);
    if (background) {
        // background-size 100%, centred: the levelshots are 16:9
        float imgH = smVW * 9.0f / 16.0f;

        SM_Pic(0, top + (height - imgH) * 0.5f, smVW, imgH, background, &box);
    }
    SM_Fill(cx, top, cw, height, smDim, NULL);
    return SM_R(cx + 15, top + 15, cw - 30, height - 30);
}

static void SM_DrawButton(float x, float y, float w, const char* text, smHitType_t type) {
    qboolean over = SM_Over(type, 0);

    // Skeleton's button: a 1-pixel border, 11-pixel capitals spaced by 1
    SM_Fill(x, y, w, 30, over ? smArrow : smButtonBorder, NULL);
    SM_Fill(x + 1, y + 1, w - 2, 28, over ? smButtonHover : smButton, NULL);
    SM_SpacedText(x + (w - SM_SpacedWidth(FONT_SANS, 11, text, 1)) * 0.5f, SM_Baseline(FONT_SANS, 11, y, 30), FONT_SANS,
                  11, over ? smYellow : smText, text, 1, qtrue);
    SM_Hit(x, y, w, 30, type, 0, NULL);
}

// Cancel and OK at the right, a space apart
static void SM_DrawButtons(float right, float y) {
    float okW = SM_SpacedWidth(FONT_SANS, 11, "OK", 1) + 42;
    float cancelW = SM_SpacedWidth(FONT_SANS, 11, "CANCEL", 1) + 42;
    float okX = right - okW;

    SM_DrawButton(okX - SM_Advance(FONT_SANS, 15, " ", 1) - cancelW, y, cancelW, "CANCEL", SM_HIT_CANCEL);
    SM_DrawButton(okX, y, okW, "OK", SM_HIT_OK);
}

// an 8-pixel scrollbar with only its thumb drawn
static void SM_DrawScrollbar(float x, float y, float h, float content, float scroll) {
    float thumbH = h * h / content;
    float thumbY;

    if (thumbH < 16) {
        thumbH = 16;
    }
    thumbY = y + (h - thumbH) * scroll / (content - h);
    SM_Hit(x, y, 8, h, SM_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, 8, thumbH, (SM_Over(SM_HIT_THUMB, 0) || sm.scrollDrag) ? smArrow : smGrey, NULL);
    SM_Hit(x, thumbY, 8, thumbH, SM_HIT_THUMB, 0, NULL);
}

static float SM_ClampScroll(float scroll, float content, float view) {
    if (scroll > content - view) {
        scroll = content - view;
    }
    return scroll < 0 ? 0 : scroll;
}

// the height of a factory's entry: its title, author and description, wrapped
static float SM_FactoryHeight(const smFactory_t* f, float width) {
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    float h = 7.5f;

    h += SM_Wrap(FONT_SANS, 24, f->title, width, starts, lens) * 38.39f;
    h += SM_Wrap(FONT_SANS, 15, f->author[0] ? f->author : "No author provided", width, starts, lens) * 24.0f;
    h += SM_Wrap(FONT_SANS, 15, f->description[0] ? f->description : "No description.", width, starts, lens) * 24.0f;
    return h;
}

static float SM_DrawLines(float x, float y, float size, float lineHeight, const float* color, const char* text,
                          float width, qboolean bold, const smRect_t* clip) {
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    int n = SM_Wrap(FONT_SANS, size, text, width, starts, lens);
    int k;

    for (k = 0; k < n; k++) {
        float base = SM_Baseline(FONT_SANS, size, y, lineHeight);

        if (SM_TextInside(base, size, clip)) {
            SM_Text(x, base, FONT_SANS, size, color, text + starts[k], lens[k], bold);
        }
        y += lineHeight;
    }
    return y;
}

static void SM_DrawFactories(void) {
    smRect_t in = SM_DrawDialog(600, SM_Levelshot(sm.tempMap));
    smRect_t clip;
    float space = SM_Advance(FONT_SANS, 18.67f, " ", 1);
    float itemW = 0, menuW, ax, aw, ah = in.h * 0.95f, ew, content, y;
    int list[SM_MAX_FACTORIES];
    int n, k;

    // menu: the gametypes, as wide as the widest with its icon and padding
    for (k = 0; k < SM_NUM_GAMETYPES; k++) {
        float w = SM_Advance(FONT_SANS, 18.67f, smGametypes[k].title, -1);

        if (w > itemW) {
            itemW = w;
        }
    }
    itemW += 3 + 28 + 5 + space + 50;
    menuW = itemW + 18.67f;
    for (k = 0; k < SM_NUM_GAMETYPES; k++) {
        float ry = in.y + k * 36.0f;

        if (smGametypes[k].gametype == sm.selectedGametype) {
            SM_Fill(in.x, ry, itemW, 36, smGrey, NULL);
        }
        SM_Pic(in.x + 3, ry + 5, 28, 28, SM_GametypeIcon(&smGametypes[k]), NULL);
        SM_Text(in.x + 36 + space, SM_Baseline(FONT_SANS, 18.67f, ry + 3, 29.87f), FONT_SANS, 18.67f, smText,
                smGametypes[k].title, -1, qfalse);
        SM_Hit(in.x, ry, itemW, 36, SM_HIT_GAMETYPE, k, NULL);
    }

    // article: the factories, scrolling when they do not fit
    ax = in.x + menuW;
    aw = in.w - menuW;
    clip = SM_R(ax, in.y, aw, ah);
    n = SM_FactoryList(list);
    ew = aw;
    content = 0;
    for (k = 0; k < n; k++) {
        content += SM_FactoryHeight(&smFactories[list[k]], ew - 15) + 15;
    }
    if (content > ah) {
        ew = aw - 8;
        content = 0;
        for (k = 0; k < n; k++) {
            content += SM_FactoryHeight(&smFactories[list[k]], ew - 15) + 15;
        }
    }
    sm.factoryScroll = SM_ClampScroll(sm.factoryScroll, content, ah);
    y = in.y - sm.factoryScroll;
    for (k = 0; k < n; k++) {
        const smFactory_t* f = &smFactories[list[k]];
        float eh = SM_FactoryHeight(f, ew - 15);

        if (y < clip.y + clip.h && y + eh > clip.y) {
            qboolean selected = !strcmp(f->id, sm.tempFactory);
            const float* color = selected ? smYellow : smText;
            float ty = y + 3.75f;

            if (selected) {
                SM_Fill(ax, y, ew, eh, smGrey, &clip);
            }
            ty = SM_DrawLines(ax + 15, ty, 24, 38.39f, color, f->title, ew - 15, qtrue, &clip);
            ty = SM_DrawLines(ax + 15, ty, 15, 24, color, f->author[0] ? f->author : "No author provided", ew - 15,
                              qfalse, &clip);
            SM_DrawLines(ax + 15, ty, 15, 24, color, f->description[0] ? f->description : "No description.", ew - 15,
                         qfalse, &clip);
            SM_Hit(ax, y, ew, eh, SM_HIT_FACTORY, list[k], &clip);
        }
        y += eh + 15;
    }
    if (content > ah) {
        SM_DrawScrollbar(ax + aw - 8, in.y, ah, content, sm.factoryScroll);
    }
    sm.article = clip;
    sm.articleContent = content;
    SM_DrawButtons(in.x + in.w, in.y + ah + 10);
}

// lays out (and draws, given a clip) the map tiles as inline blocks; returns their height
static float SM_MapTiles(float x, float y, float width, const smRect_t* clip) {
    float cx = 0, cy = 0;
    int k;

    for (k = 0; k < smNumMapList; k++) {
        smArena_t* a = &smArenas[smMapList[k]];
        // as wide as the levelshot or the name, with the padding
        float tw = SM_NameWidth(a);

        tw = (tw > 250 ? tw : 250) + 12;
        if (cx > 0 && cx + tw + 24 > width) {
            cx = 0;
            cy += 214.39f;
        }
        if (clip && y + cy < clip->y + clip->h && y + cy + 190.39f > clip->y) {
            float tx = x + cx, ty = y + cy;
            float base = SM_Baseline(FONT_SANS, 24, ty + 146, 38.4f);
            qboolean selected = !strcmp(a->sysname, sm.tempMap);

            if (selected) {
                SM_Fill(tx, ty, tw, 190.39f, smGrey, clip);
            }
            SM_Pic(tx + 6, ty + 6, 250, 140, SM_Preview(a, a->sysname), clip);
            if (SM_TextInside(base, 24, clip)) {
                SM_Centered(tx + tw * 0.5f, base, 24, selected ? smYellow : smText, a->name, qtrue);
            }
            SM_Hit(tx, ty, tw, 190.39f, SM_HIT_MAP, smMapList[k], clip);
        }
        cx += tw + 24;
    }
    return smNumMapList ? cy + 214.39f : 0;
}

static void SM_DrawMaps(void) {
    smRect_t in = SM_DrawDialog(980, 0);
    smRect_t clip;
    float itemW = SM_Advance(FONT_SANS, 18.67f, "ALL", -1) + 18.67f;
    float menuW = itemW + 18.67f;
    float ax = in.x + menuW, aw = in.w - menuW, ah = in.h * 0.95f, width, content;
    int k;

    // menu: ALL, A to Z and #, one under the other
    for (k = 0; k < 28; k++) {
        const char* label = k == 0 ? "ALL" : va("%c", SM_FilterChar(k));
        float ry = in.y + k * 33.59f;

        if (SM_FilterChar(k) == sm.filter) {
            SM_Fill(in.x, ry, itemW, 33.59f, smGrey, NULL);
        }
        SM_Text(in.x + (itemW - SM_Advance(FONT_SANS, 18.67f, label, -1)) * 0.5f,
                SM_Baseline(FONT_SANS, 18.67f, ry + 1.87f, 29.87f), FONT_SANS, 18.67f, smText, label, -1, qfalse);
        SM_Hit(in.x, ry, itemW, 33.59f, SM_HIT_FILTER, k, NULL);
    }

    // article: the maps, scrolling when they do not fit
    SM_BuildMapList();
    clip = SM_R(ax, in.y, aw, ah);
    width = aw;
    content = SM_MapTiles(ax, in.y, width, NULL);
    if (content > ah) {
        width = aw - 8;
        content = SM_MapTiles(ax, in.y, width, NULL);
    }
    sm.mapScroll = SM_ClampScroll(sm.mapScroll, content, ah);
    SM_MapTiles(ax, in.y - sm.mapScroll, width, &clip);
    if (content > ah) {
        SM_DrawScrollbar(ax + aw - 8, in.y, ah, content, sm.mapScroll);
    }
    sm.article = clip;
    sm.articleContent = content;
    SM_DrawButtons(in.x + in.w, in.y + ah + 10);
}

/*
===============================================================================

INPUT

===============================================================================
*/

static void SM_CancelDialog(void) {
    if (sm.dialog == SM_DIALOG_FACTORIES) {
        SM_CancelFactories();
    } else if (sm.dialog == SM_DIALOG_MAPS) {
        SM_CancelMaps();
    }
}

static void SM_ConfirmDialog(void) {
    if (sm.dialog == SM_DIALOG_FACTORIES) {
        SM_ConfirmFactories();
    } else if (sm.dialog == SM_DIALOG_MAPS) {
        SM_ConfirmMaps();
    }
}

static float* SM_DialogScroll(void) {
    return sm.dialog == SM_DIALOG_FACTORIES ? &sm.factoryScroll : &sm.mapScroll;
}

static void SM_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);

    // a click redraws the page, which lets go of a slider's leftover value
    sm.held = -1;
    // the text input loses the focus to a click elsewhere
    if (sm.edit >= 0 && !(hit && hit->type == SM_HIT_INPUT && hit->index == sm.edit)) {
        SM_EndEdit();
    }
    // and an open dropdown closes, unless the click is on it
    if (sm.select >= 0 && !(hit && (hit->type == SM_HIT_MENU || hit->type == SM_HIT_OPTION ||
                                    (hit->type == SM_HIT_SELECT && hit->index == sm.select)))) {
        sm.select = -1;
    }
    if (!hit) {
        return;
    }
    switch (hit->type) {
        case SM_HIT_GAMETYPE_CARD:
            // the dialog opens scrolled to the top: a hidden element loses its scroll
            sm.dialog = SM_DIALOG_FACTORIES;
            sm.factoryScroll = 0;
            break;
        case SM_HIT_ARENA_CARD:
            sm.dialog = SM_DIALOG_MAPS;
            sm.mapScroll = 0;
            break;
        case SM_HIT_RESET:
            trap_Cmd_ExecuteText(EXEC_APPEND, va("reset %s\n", smCvars[hit->index].cvar));
            break;
        case SM_HIT_INPUT:
            if (sm.edit != hit->index) {
                sm.edit = hit->index;
                sm.editStart = 0;
            }
            sm.editCursor = SM_CaretAt(sm.values[sm.edit], sm.editStart, smMX - (hit->r.x + 9));
            sm.editTime = uiInfo.uiDC.realTime;
            break;
        case SM_HIT_SELECT:
            if (sm.select == hit->index) {
                sm.select = -1;
            } else {
                SM_OpenSelect(hit->index);
            }
            break;
        case SM_HIT_OPTION:
            // onSelectChanged: the cvar changes, and the page shows it when it hears so
            trap_Cvar_Set(smCvars[sm.select].cvar, smCvars[sm.select].options[hit->index].value);
            sm.select = -1;
            break;
        case SM_HIT_RANGE:
            sm.drag = hit->index;
            sm.dragRect = hit->r;
            sm.dragValue = SM_RangeAt(sm.drag, smMX);
            break;
        case SM_HIT_LAUNCH:
            SM_Launch();
            break;
        case SM_HIT_GAMETYPE:
            SM_SelectGametype(smGametypes[hit->index].gametype);
            break;
        case SM_HIT_FACTORY:
            SM_SelectFactory(smFactories[hit->index].id);
            break;
        case SM_HIT_FILTER:
            sm.filter = SM_FilterChar(hit->index);
            break;
        case SM_HIT_MAP:
            Q_strncpyz(sm.tempMap, smArenas[hit->index].sysname, sizeof(sm.tempMap));
            break;
        case SM_HIT_CANCEL:
            SM_CancelDialog();
            break;
        case SM_HIT_OK:
            SM_ConfirmDialog();
            break;
        case SM_HIT_THUMB:
            sm.scrollDrag = qtrue;
            sm.scrollGrab = smMY - hit->r.y;
            break;
        case SM_HIT_TRACK:
            // a click on the track pages up or down
            if (smMY < sm.article.y + sm.article.h * 0.5f) {
                *SM_DialogScroll() -= sm.article.h * 0.875f;
            } else {
                *SM_DialogScroll() += sm.article.h * 0.875f;
            }
            break;
        default:
            break;
    }
}

static void SM_Wheel(float amount) {
    if (sm.select >= 0 && SM_In(&sm.selectMenu, smMX, smMY)) {
        sm.optionScroll += amount;
    } else if (sm.dialog != SM_DIALOG_NONE && SM_In(&sm.article, smMX, smMY)) {
        *SM_DialogScroll() += amount;
    }
}

// what is held down with the mouse follows it, and is let go when the button is
static void SM_UpdateDrags(void) {
    if (sm.drag >= 0) {
        if (trap_Key_IsDown(K_MOUSE1)) {
            // onInputRange: the value shown follows the thumb
            sm.dragValue = SM_RangeAt(sm.drag, smMX);
        } else {
            // onMouseUp: only a release over the slider sets the cvar
            char value[32];

            SM_FormatRange(sm.drag, sm.dragValue, value, sizeof(value));
            if (SM_In(&sm.dragRect, smMX, smMY)) {
                trap_Cvar_Set(smCvars[sm.drag].cvar, value);
            } else {
                sm.held = sm.drag;
                Q_strncpyz(sm.heldValue, value, sizeof(sm.heldValue));
            }
            sm.drag = -1;
        }
    }
    if (sm.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1) || sm.dialog == SM_DIALOG_NONE) {
            sm.scrollDrag = qfalse;
        } else if (sm.articleContent > sm.article.h) {
            float thumbH = sm.article.h * sm.article.h / sm.articleContent;

            if (thumbH < 16) {
                thumbH = 16;
            }
            *SM_DialogScroll() = (smMY - sm.scrollGrab - sm.article.y) / (sm.article.h - thumbH) *
                                 (sm.articleContent - sm.article.h);
        }
    }
}

// typing in a text input; false for what is not for it
static qboolean SM_EditKey(int key) {
    char* value = sm.values[sm.edit];
    int len = (int)strlen(value);

    if (key & K_CHAR_FLAG) {
        key &= ~K_CHAR_FLAG;
        if (key == 'h' - 'a' + 1) {  // backspace comes as ctrl-h
            if (sm.editCursor > 0) {
                memmove(&value[sm.editCursor - 1], &value[sm.editCursor], len + 1 - sm.editCursor);
                sm.editCursor--;
            }
        } else if (key >= 32 && key < 256 && len < SM_MAX_INPUT) {
            memmove(&value[sm.editCursor + 1], &value[sm.editCursor], len + 1 - sm.editCursor);
            value[sm.editCursor++] = (char)key;
        }
        sm.editTime = uiInfo.uiDC.realTime;
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
        case K_MOUSE2:
        case K_MOUSE3:
        case K_MWHEELUP:
        case K_MWHEELDOWN:
            return qfalse;
        case K_DEL:
        case K_KP_DEL:
            if (sm.editCursor < len) {
                memmove(&value[sm.editCursor], &value[sm.editCursor + 1], len - sm.editCursor);
            }
            break;
        case K_LEFTARROW:
        case K_KP_LEFTARROW:
            if (sm.editCursor > 0) {
                sm.editCursor--;
            }
            break;
        case K_RIGHTARROW:
        case K_KP_RIGHTARROW:
            if (sm.editCursor < len) {
                sm.editCursor++;
            }
            break;
        case K_HOME:
        case K_KP_HOME:
            sm.editCursor = 0;
            break;
        case K_END:
        case K_KP_END:
            sm.editCursor = len;
            break;
        case K_ESCAPE:
        case K_TAB:
            // leaving the field: onBlur
            SM_EndEdit();
            break;
        default:
            // Enter does nothing in the page's inputs; typed keys come again as characters
            break;
    }
    sm.editTime = uiInfo.uiDC.realTime;
    return qtrue;
}

/*
===============================================================================

ENTRY POINTS

===============================================================================
*/

static void SM_Load(void) {
    if (sm.loaded) {
        return;
    }
    // the page's factories and mapdb modules read the engine once, when the web UI loads
    SM_LoadFactories();
    SM_LoadArenas();
    SM_Defaults();
    sm.edit = -1;
    sm.select = -1;
    sm.drag = -1;
    sm.held = -1;
    sm.loaded = qtrue;
}

/*
===============
UI_StartMatch_Open

The "startmatch" menu's onOpen: the page mounts again with the state it saved,
its dialogs closed.
===============
*/
void UI_StartMatch_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    SM_Load();
    sm.dialog = SM_DIALOG_NONE;
    sm.edit = -1;
    sm.select = -1;
    sm.drag = -1;
    sm.held = -1;
    sm.scrollDrag = qfalse;
    SM_ReadCvars(qfalse);
    // StartMatchCvars' componentWillMount: the match type picks the port, and the network restarts on it
    if (sm.values[SM_SERVERTYPE][0]) {
        trap_Cvar_Set("net_port", SM_Equals(sm.values[SM_SERVERTYPE], 1) ? "27015" : "27960");
        trap_Cmd_ExecuteText(EXEC_APPEND, "net_restart\n");
    }
    // the page takes keys before the mouse moves over it
    menu = Menus_FindByName("startmatch");
    item = menu ? Menu_FindItemByName(menu, "startmatch") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_StartMatch_Paint(void) {
    const smHit_t* under;

    SM_Frame();
    if (!sm.loaded) {
        SM_Load();
        SM_ReadCvars(qfalse);
    }
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;

    SM_ReadCvars(qtrue);
    SM_UpdateLocks();
    SM_UpdateDrags();
    SM_DrawPage();
    if (sm.select >= 0) {
        SM_DrawDropdown();
    }
    if (sm.dialog == SM_DIALOG_FACTORIES) {
        SM_DrawFactories();
    } else if (sm.dialog == SM_DIALOG_MAPS) {
        SM_DrawMaps();
    }

    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
}

qboolean UI_StartMatch_HandleKey(int key) {
    if (sm.edit >= 0 && SM_EditKey(key)) {
        return qtrue;
    }
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
            SM_Click();
            return qtrue;
        case K_MWHEELUP:
            SM_Wheel(-100);
            return qtrue;
        case K_MWHEELDOWN:
            SM_Wheel(100);
            return qtrue;
        case K_ENTER:
        case K_KP_ENTER:
            // the dialogs' keydown handlers: Enter confirms
            SM_ConfirmDialog();
            return qtrue;
        case K_ESCAPE:
            // and Escape cancels; an open dropdown closes first
            if (sm.select >= 0) {
                sm.select = -1;
                return qtrue;
            }
            if (sm.dialog != SM_DIALOG_NONE) {
                SM_CancelDialog();
                return qtrue;
            }
            // the menu's onESC goes back to the main menu
            return qfalse;
        default:
            return qfalse;
    }
}
