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
// ui_home.c -- [QL] the main menu, and the frame the other pages sit in
//
// Retail's front end is its browser, and it is two different things. The main
// menu (web.pak src/home.js, "#main-menu") is a screen of its own, outside the
// app frame: the QUAKE LIVE logo over a vertical menu down the left, with art
// beside it. Its menu opens into itself -- Play gives Match Browser, Start
// Match, Training Center and Back -- and the pages it leads to (src/appframe.js,
// "#app-frame") are the ones with the nav bar across the top.
//
// The menu keeps that shape: the logo, the list down the left, Play opening into
// its own list with Back under it. Its look is this build's own, in Quake Club's
// surfaces, type and gold: a painting covers the screen, its figure standing on
// the right, and the page's colour comes in from the left under the list. Each
// entry says in a line what it leads to, and the one under the cursor turns gold
// on a raised plate. Nothing moves. The painting is ui/ioql/menu_background,
// which pak01 carries from assets\pak01 (it is not in git); a build without it
// falls back to a map's levelshot.
//
// The bar the other pages carry is here too, since they share it, along with the
// background, which lies under their panels thickly enough for them to read, and
// the green banner a match puts at the bottom of every page.
//

#include "ui_web.h"

#define HM_NAV_ITEM_SIZE 15.0f
#define HM_NAV_ITEM_SPACING 0.6f
#define HM_NAV_ITEM_GAP 26.0f
#define HM_NAV_INSET 39.0f

// the main menu
#define HM_COL_LEFT 0.065f   // of the screen's width
#define HM_COL_MIN_X 80.0f
#define HM_COL_W 470.0f      // what an entry takes, rule to chevron
#define HM_RULE_GAP 26.0f    // the hover rule stands this far left of the text
#define HM_LOGO_Y 132.0f
#define HM_LOGO_W 520.0f
#define HM_LIST_Y 340.0f
#define HM_ITEM_H 78.0f
#define HM_ITEM_SIZE 27.0f

// [ioquakelive] la lista: una entrada por fila, con aire de sobra entre ellas. La fila mide lo
// mismo con descripcion y sin ella, asi la lista no se mueve cuando el cursor la recorre.
#define HM_ROW_H 64.0f
#define HM_HEADING_H 40.0f
#define HM_ITEM_SPACING 0.7f
#define HM_NOTE_SIZE 14.0f
#define HM_BACK_SIZE 17.0f
#define HM_BACK_H 46.0f

// the player and their friends, down the right
#define HM_SIDE_W 306.0f     // retail's column is 370; a little under it reads calmer
#define HM_SIDE_RIGHT 64.0f
#define HV_SOCIAL_W 330.0f  // [ioquakelive] la columna social del menu principal

// the painting's shape
#define HM_PAINTING_ASPECT (3122.0f / 1344.0f)

typedef enum {
    HM_HIT_NONE = SM_HIT_SHARED,
    HM_HIT_NAV,
    HM_HIT_BANNER,
    HM_HIT_ITEM,
    HM_HIT_CARD,  // [ioquakelive] una tarjeta del menu principal
    HM_HIT_QUIT,
    HM_HIT_TAB,   // [ioquakelive] una seccion de la barra de arriba
    HM_HIT_MODE   // [ioquakelive] un modo destacado
} hmHitType_t;

// arenas whose shots pak00 carries, one of which is the background when the painting is not there
static const char* const hmBackdrops[] = {
    "bloodrun", "campgrounds", "toxicity", "aerowalk", "furiousheights", "sinister", "lostworld",
    "hektik", "silence", "cure", "almostlost", "asylum", "battleforged", "trinity", "elder", "overkill",
};

static struct {
    qboolean loaded;
    qhandle_t backdrop;
    qboolean painting;  // the backdrop is the menu's painting, not a levelshot
} hm;

// which of the lists is showing
#define HM_MENU_MAIN 0
#define HM_MENU_PLAY 1
#define HM_MENU_STATS 2
#define HM_MENU_COMMUNITY 3
#define HM_NUM_MENUS 4
#define HM_MAX_ITEMS 6

static struct {
    qboolean loadedMenu;
    qhandle_t logo;
    int menu;  // which of hmMenus is showing
    qhandle_t face;
    char faceModel[MAX_QPATH];
} hm2;

static void UI_Home_LoadMenu(void) {
    if (hm2.loadedMenu) {
        return;
    }
    hm2.loadedMenu = qtrue;
    hm2.menu = HM_MENU_MAIN;
    hm2.logo = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo");
}

float UI_Home_Left(void) {
    return (smVW - HM_CONTAINER_W) * 0.5f;
}

/*
===============
UI_Home_Load

The background: the menu's painting, or, when pak01 has not got it, one of the
arenas above whose levelshot pak00 has, picked once per session the way retail's
App picked a random map for its page.
===============
*/
static void UI_Home_Load(void) {
    int count = (int)ARRAY_LEN(hmBackdrops);
    int start, k;

    if (hm.loaded) {
        return;
    }
    hm.loaded = qtrue;
    hm.backdrop = trap_R_RegisterShaderNoMip("ui/ioql/menu_background");
    if (hm.backdrop) {
        hm.painting = qtrue;
        return;
    }
    start = (int)((unsigned int)trap_Milliseconds() % (unsigned int)count);
    for (k = 0; k < count; k++) {
        const char* map = hmBackdrops[(start + k) % count];
        fileHandle_t f;
        int len = trap_FS_FOpenFile(va("levelshots/%s.jpg", map), &f, FS_READ);

        if (len > 0) {
            trap_FS_FCloseFile(f);
            hm.backdrop = trap_R_RegisterShaderNoMip(va("levelshots/%s", map));
            if (hm.backdrop) {
                return;
            }
        }
    }
}

/*
===============
UI_Home_Wash

A vertical wash: a colour whose alpha runs from a0 at the top to a1 at the bottom.
It is drawn in bands a few screen pixels high whose edges are whole pixels, so
two bands meet without a row between them drawn twice -- with bands in page
units, a row where they overlapped came out as a dark line across the screen.
UI_Home_WashAcross is the same from left to right, eased so it thins out
without an edge.
===============
*/
#define HM_WASH_STEP 4

static void UI_Home_Wash(float y, float h, const float* color, float a0, float a1) {
    float top = smTop + y * smScale;
    float bottom = smTop + (y + h) * smScale;
    int last = (int)(bottom + 0.5f);
    int py;
    vec4_t c;

    if (bottom <= top) {
        return;
    }
    VectorCopy(color, c);
    for (py = (int)(top + 0.5f); py < last; py += HM_WASH_STEP) {
        int end = py + HM_WASH_STEP < last ? py + HM_WASH_STEP : last;

        c[3] = a0 + (a1 - a0) * ((py + end) * 0.5f - top) / (bottom - top);
        trap_R_SetColor(c);
        trap_R_DrawStretchPic(0, (float)py, (float)uiInfo.uiDC.glconfig.vidWidth, (float)(end - py), 0, 0, 0, 0,
                              uiInfo.uiDC.whiteShader);
    }
    trap_R_SetColor(NULL);
}

static void UI_Home_WashAcross(float x, float w, const float* color, float a0, float a1) {
    float left = x * smScale;
    float right = (x + w) * smScale;
    int last = (int)(right + 0.5f);
    int px;
    vec4_t c;

    if (right <= left) {
        return;
    }
    VectorCopy(color, c);
    for (px = (int)(left + 0.5f); px < last; px += HM_WASH_STEP) {
        int end = px + HM_WASH_STEP < last ? px + HM_WASH_STEP : last;
        float t = ((px + end) * 0.5f - left) / (right - left);

        t = t * t * (3.0f - 2.0f * t);
        c[3] = a0 + (a1 - a0) * t;
        trap_R_SetColor(c);
        trap_R_DrawStretchPic((float)px, 0, (float)(end - px), (float)uiInfo.uiDC.glconfig.vidHeight, 0, 0, 0, 0,
                              uiInfo.uiDC.whiteShader);
    }
    trap_R_SetColor(NULL);
}

/*
===============
UI_Home_DrawBackground

The background covers the screen whatever its shape. The painting keeps its right
edge on the screen's, where its figure stands, and gives up its left, which is
haze; a levelshot is centred.
===============
*/
static void UI_Home_DrawBackground(void) {
    float top = -smTop / smScale;
    float full = smBottom - top;
    float aspect = hm.painting ? HM_PAINTING_ASPECT : 1920.0f / 1080.0f;
    float w = smVW;
    float h = full;

    UI_Home_Load();
    SM_FillScreen(smPage);
    if (!hm.backdrop) {
        return;
    }
    if (w / h > aspect) {
        h = w / aspect;  // cover: overflow the height rather than letterbox
    } else {
        w = h * aspect;
    }
    // the painting covers the screen whole: pulled off the right edge it left a band of
    // page showing, so it stays anchored there and the panels over it are seen through
    SM_Pic(hm.painting ? smVW - w : (smVW - w) * 0.5f, top + (full - h) * 0.5f, w, h, hm.backdrop, NULL);
}

/*
===============
UI_Home_Backdrop

What the other pages sit on: the background, and the page's colour over it thick
enough for the panels to read -- heavier at the top where the bar is and at the
bottom, lighter in the middle, which is what retail's radial gradient did.
===============
*/
void UI_Home_Backdrop(void) {
    float top = -smTop / smScale;
    float full = smBottom - top;

    UI_Home_DrawBackground();
    UI_Home_Wash(top, full * 0.5f, smPage, 0.96f, 0.80f);
    UI_Home_Wash(top + full * 0.5f, full * 0.5f, smPage, 0.80f, 0.97f);
}

/*
===============
UI_Home_PageTop

What a page carries instead of the bar of links the front end used to have: this
is a game's front end and not a web site (the player's words), so there is the
QUAKE LIVE logo at the left and, at the right, the one way back to the menu.
Escape does the same.
===============
*/
void UI_Home_PageTop(int active) {
    float left = UI_Home_Left();
    float w = SM_LabelWidth(HM_NAV_ITEM_SIZE, "Back", HM_NAV_ITEM_SPACING);
    float bx = left + HM_CONTAINER_W - w;
    qboolean over = SM_Over(HM_HIT_NAV, HM_NAV_HOME);

    (void)active;
    UI_Home_LoadMenu();
    if (hm2.logo) {
        SM_Pic(left, 26.0f, 196.0f, 49.0f, hm2.logo, NULL);
    }
    SM_Label(bx, SM_Baseline(FONT_SANS, HM_NAV_ITEM_SIZE, 47, 22), HM_NAV_ITEM_SIZE,
             over ? smGold : smTextSoft, "Back", HM_NAV_ITEM_SPACING);
    SM_Hit(bx - 20.0f, 30.0f, w + 40.0f, 46.0f, HM_HIT_NAV, HM_NAV_HOME, NULL);
}

/*
===============
UI_Home_Header

A page's title panel, as the site heads its pages: a panel the container's width
with the title in bold upper case, and, over it, the small gold line some pages
carry. Returns where the panel ends.
===============
*/
float UI_Home_Header(float y, const char* eyebrow, const char* title) {
    float left = UI_Home_Left();
    float h = eyebrow ? 92.0f : 72.0f;

    SM_Round(left, y, HM_CONTAINER_W, h, SM_RADIUS_MD, smPanel, NULL);
    if (eyebrow) {
        SM_Label(left + 25, SM_Baseline(FONT_SANS, 11, y + 18, 16), 11, smGold, eyebrow, 1.6f);
        SM_Label(left + 25, SM_Baseline(FONT_SANS, 24, y + 38, 32), 24, smText, title, 0.2f);
    } else {
        SM_Label(left + 25, SM_Baseline(FONT_SANS, 22, y, h), 22, smText, title, 0.2f);
    }
    return y + h;
}

/*
===============
UI_Home_BlockTop

Where a page of that height starts so that it sits in the middle of the screen
rather than hanging from the top, which is what the player asked for. It never
goes over the top row, and a page taller than the screen starts right under it.
===============
*/
/*
===============
UI_Home_FrameBottom

[ioquakelive] Where a page's frame ends, the same line Settings stops its panel at, so every
page fills the same box.
===============
*/
float UI_Home_FrameBottom(void) {
    return smBottom - 40.0f - (UI_Home_InMatch() ? 38.0f : 0.0f);
}

// [ioquakelive] the block's middle is the screen's middle, as the user asked ("es centrarla"),
// not the middle of what the top row leaves, which sat it lower; it still never climbs over the
// top row, nor under the match banner
float UI_Home_BlockTop(float blockHeight) {
    float first = HM_NAV_H;
    float screenTop = -smTop / smScale;
    float bottom = smBottom - (UI_Home_InMatch() ? 38.0f : 0.0f);
    float y = screenTop + (bottom - screenTop - blockHeight) * 0.5f;

    if (y + blockHeight > bottom - 20.0f) {
        y = bottom - 20.0f - blockHeight;
    }
    return y < first ? first : y;
}

void UI_Home_Go(int nav) {
    switch (nav) {
        case HM_NAV_HOME:
            Menus_CloseAll();
            Menus_ActivateByName("main");
            break;
        case HM_NAV_PLAY:
            Menus_CloseAll();
            Menus_ActivateByName("startmatch");
            break;
        case HM_NAV_TRAINING:
            Menus_CloseAll();
            Menus_ActivateByName("training");
            break;
        case HM_NAV_REPLAYS:
            Menus_CloseAll();
            Menus_ActivateByName("demo");
            break;
        case HM_NAV_SETTINGS:
            Menus_CloseAll();
            Menus_ActivateByName("settings");
            break;
        case HM_NAV_QUIT:
            // retail's power button (App's quitGame) asks nothing: it sends quit
            trap_Cmd_ExecuteText(EXEC_APPEND, "quit\n");
            break;
        default:
            break;
    }
}

// whether the front end is up over a match, which is when retail's showed its banner
qboolean UI_Home_InMatch(void) {
    uiClientState_t cs;

    trap_GetClientState(&cs);
    return cs.connState == CA_ACTIVE;
}

// what the bar and the banner do, shared by every page that draws them
qboolean UI_Home_NavClick(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);

    if (!hit) {
        return qfalse;
    }
    if (hit->type == HM_HIT_NAV) {
        UI_Home_Go(hit->index);
        return qtrue;
    }
    if (hit->type == HM_HIT_BANNER) {
        // retail's App.returnToGame sends exactly this
        trap_Cmd_ExecuteText(EXEC_APPEND, "web_hideBrowser\n");
        return qtrue;
    }
    return qfalse;
}

/*
===============
UI_Home_Banner

Retail's App draws an AlertBanner while you are in a match, on every page: a bar
across the bottom of the screen (.banner: fixed, bottom 0, full width, text
centred) in green that says "You are currently in-game. Click here to return to
your match." and sends web_hideBrowser when clicked.
===============
*/
void UI_Home_Banner(void) {
    static const char* const text = "You are currently in-game. Click here to return to your match.";
    float h = 38.0f;
    float y = smBottom - h;
    qboolean over;

    if (!UI_Home_InMatch()) {
        return;
    }
    over = SM_Over(HM_HIT_BANNER, 0);
    SM_Fill(-1, y, smVW + 2, h, over ? smSuccessHover : smSuccess, NULL);
    SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 15, y, h), 15, smText, text, qtrue);
    SM_Hit(0, y, smVW, h, HM_HIT_BANNER, 0, NULL);
}

/*
===============================================================================

THE MAIN MENU

Retail's #main-menu: the QUAKE LIVE logo over a list that opens into itself, and
beside it the player and their friends, which its app frame keeps in a column of
its own (web.pak src/home.js: ProfileCard, Lobby and FriendList). Retail has that
column on the left of the pages; here it sits on the right of the menu, where the
painting leaves room.

The list is retail's, whole: Play, Statistics, Community, Settings and Quit, with
the three that open into their own lists doing so. What this build cannot do yet
is listed all the same, marked "soon" and inert, so nothing of retail's is
missing from the menu.

===============================================================================
*/

typedef struct {
    const char* title;
    const char* note;  // the line under it
    int nav;           // the page it opens, or -1
    int submenu;       // the list it opens instead, or -1
    qboolean soon;     // listed, but this build has not got it
} hmItem_t;


static const hmItem_t hmMenus[HM_NUM_MENUS][HM_MAX_ITEMS] = {
    {
        {"Play", "Start a match, or train with Crash", -1, HM_MENU_PLAY, qfalse},
        {"Statistics", "Your career, your matches, your replays", -1, HM_MENU_STATS, qfalse},
        {"Community", "The Workshop and the Steam community", -1, HM_MENU_COMMUNITY, qfalse},
        {"Settings", "Controls, video, sound and your player", HM_NAV_SETTINGS, -1, qfalse},
        {"Quit", "Leave QUAKE LIVE", HM_NAV_QUIT, -1, qfalse},
    },
    {
        {"Match Browser", "Find a server to play on", -1, -1, qtrue},
        {"Start Match", "Pick an arena, a game type and bots", HM_NAV_PLAY, -1, qfalse},
        {"Training Center", "Crash Course, strafe and rocket jump practice", HM_NAV_TRAINING, -1, qfalse},
        {"Back", NULL, -1, HM_MENU_MAIN, qfalse},
        {NULL, NULL, -1, -1, qfalse},
        {NULL, NULL, -1, -1, qfalse},
    },
    {
        {"Career Stats", "Everything you have played", -1, -1, qtrue},
        {"Recent Matches", "How the last ones went", -1, -1, qtrue},
        {"Replays", "Watch the matches you recorded", HM_NAV_REPLAYS, -1, qfalse},
        {"Back", NULL, -1, HM_MENU_MAIN, qfalse},
        {NULL, NULL, -1, -1, qfalse},
        {NULL, NULL, -1, -1, qfalse},
    },
    {
        {"Steam Workshop", "Maps and models other players made", -1, -1, qtrue},
        {"Steam Community", "The game's hub", -1, -1, qtrue},
        {"Back", NULL, -1, HM_MENU_MAIN, qfalse},
        {NULL, NULL, -1, -1, qfalse},
        {NULL, NULL, -1, -1, qfalse},
    },
};



static float UI_Home_ColumnX(void) {
    float x = smVW * HM_COL_LEFT;

    return x < HM_COL_MIN_X ? HM_COL_MIN_X : x;
}

/*
===============
UI_Home_DrawShade

The page's colour under the list, thinning out towards the figure, and a little
of it along the top and, for the notice, along the bottom.
===============
*/
static void UI_Home_DrawShade(void) {
    float top = -smTop / smScale;
    float full = smBottom - top;
    float solid = UI_Home_ColumnX() + HM_COL_W - 40.0f;
    vec4_t shade;

    VectorCopy(smPage, shade);
    shade[3] = 0.92f;
    SM_Fill(-1, top, solid + 1, full, shade, NULL);
    UI_Home_WashAcross(solid, 520.0f, smPage, 0.92f, 0.0f);
    UI_Home_Wash(top, 150.0f, smPage, 0.55f, 0.0f);
    // [ioquakelive] El pie es una banda desde que lleva la franja de marcas: el degradado
    // de antes se apagaba justo donde estan, y sobre la figura del cuadro quedaban a media
    // luz. Ahora el degradado termina arriba de ellas y lo que queda abajo es parejo.
    UI_Home_Wash(smBottom - 300.0f, 190.0f, smPage, 0.0f, 0.9f);
    {
        vec4_t band;

        VectorCopy(smPage, band);
        band[3] = 0.9f;
        SM_Fill(-1, smBottom - 110.0f, smVW + 2, 111.0f, band, NULL);
    }
}

/*
===============
UI_Home_DrawColumn

The logo, then the list with its heading: the name you play under on the main
list, the list's own name on the others. An entry is its title in bold upper case
and a line of what it does; under the cursor the title turns Quake Live yellow
and a 3px rule stands at the left, which is what .nav-column a:hover does with
its border. What this build has not got is dimmed and says so. Back sits a little
apart and smaller, as retail's a.back does.
===============
*/
static void UI_Home_DrawColumn(void) {
    float x = UI_Home_ColumnX();
    float y = HM_LIST_Y;
    float panelTop, panelH;
    char heading[MAX_CVAR_VALUE_STRING];
    int i, rows = 0;

    if (hm2.logo) {
        SM_Pic(x - 6, HM_LOGO_Y, HM_LOGO_W, HM_LOGO_W * 0.25f, hm2.logo, NULL);
    }

    (void)rows;
    (void)panelTop;
    (void)panelH;

    // la lista de un submenu dice cual es; la principal no lo necesita, que ya lo dice el
    // panel del jugador a la derecha
    if (hm2.menu != HM_MENU_MAIN) {
        Q_strncpyz(heading,
                   hmMenus[HM_MENU_MAIN][hm2.menu == HM_MENU_PLAY ? 0 : hm2.menu == HM_MENU_STATS ? 1 : 2].title,
                   sizeof(heading));
        SM_Label(x, SM_Baseline(FONT_SANS, 12, y, 18), 12, smMuted, heading, 1.8f);
        y += HM_HEADING_H;
    }

    for (i = 0; i < HM_MAX_ITEMS; i++) {
        const hmItem_t* item = &hmMenus[hm2.menu][i];
        qboolean over;

        if (!item->title) {
            break;
        }
        over = SM_Over(HM_HIT_ITEM, i) && !item->soon;

        // [ioquakelive] Lo unico que marca donde estas es una barra fina al canto izquierdo y el
        // titulo en dorado. Sin plato y sin caja: el jugador pidio algo mas minimalista, y un
        // menu de seis entradas sobre un cuadro no necesita mas.
        if (over) {
            SM_Fill(x - HM_RULE_GAP, y + 4, 3, HM_ITEM_SIZE + 4, smGold, NULL);
        }

        // [ioquakelive] the user asked for the arrow that used to stand at the right of the entry
        // under the cursor to go ("quitar el < del ui"): the gold of the title says enough
        SM_Label(x, SM_Baseline(FONT_SANS, HM_ITEM_SIZE, y, HM_ITEM_SIZE + 12), HM_ITEM_SIZE,
                 item->soon ? smSubtle : over ? smGold : smText, item->title, HM_ITEM_SPACING);
        if (item->soon) {
            float tw = SM_LabelWidth(HM_ITEM_SIZE, item->title, HM_ITEM_SPACING);

            SM_Label(x + tw + 16, SM_Baseline(FONT_SANS, 11, y + 10, 18), 11, smSubtle, "soon", 1.6f);
        }

        // La descripcion solo sale en la entrada bajo el cursor. Seis descripciones encendidas a
        // la vez eran mucho texto para nombres que ya se explican solos, y el hueco que dejan
        // reservado no se nota porque la fila mide siempre lo mismo.
        if (item->note && over) {
            SM_Text(x, SM_Baseline(FONT_SANS, HM_NOTE_SIZE, y + HM_ITEM_SIZE + 10, 18), FONT_SANS, HM_NOTE_SIZE,
                    smTextSoft, item->note, SM_Fit(FONT_SANS, HM_NOTE_SIZE, item->note, HM_COL_W), qfalse);
        }
        if (!item->soon) {
            SM_Hit(x - HM_RULE_GAP, y, HM_COL_W, HM_ROW_H - 8, HM_HIT_ITEM, i, NULL);
        }
        y += HM_ROW_H;
    }
}

// the panels at the right sit over the painting, so they are the page colour with a
// little of it left showing rather than a flat block
static const float* UI_Home_SidePanel(void) {
    static vec4_t panel;

    VectorCopy(smPanel, panel);
    panel[3] = 0.88f;
    return panel;
}

/*
===============
UI_Home_DrawProfile

Retail's ProfileCard: the Steam avatar, the name you play under and a line of
career numbers. There is no Steam session here yet, so the avatar is the icon of
the model you wear and the numbers say where they will come from. Under it, the
friends, as its FriendList has them: an avatar, a name and what they are doing.
===============
*/
static void UI_Home_DrawProfile(float x, float y, float w) {
    char name[MAX_CVAR_VALUE_STRING];
    char model[MAX_QPATH];
    char* slash;
    float pic = 52.0f;
    float h = 88.0f;

    trap_Cvar_VariableStringBuffer("name", name, sizeof(name));
    Q_CleanStr(name);
    trap_Cvar_VariableStringBuffer("model", model, sizeof(model));
    slash = strchr(model, '/');
    if (slash) {
        *slash = '\0';
    }
    if (!model[0]) {
        Q_strncpyz(model, "sarge", sizeof(model));
    }
    if (Q_stricmp(model, hm2.faceModel)) {
        Q_strncpyz(hm2.faceModel, model, sizeof(hm2.faceModel));
        hm2.face = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_default", model));
    }

    SM_Fill(x, y, w, h, UI_Home_SidePanel(), NULL);
    if (hm2.face) {
        SM_Pic(x + 16, y + (h - pic) * 0.5f, pic, pic, hm2.face, NULL);
    }
    SM_Label(x + 16 + pic + 14, SM_Baseline(FONT_SANS, 16, y + 18, 22), 16, smText,
             name[0] ? name : "Player", 0.4f);
    SM_Text(x + 16 + pic + 14, SM_Baseline(FONT_SANS, 12, y + 44, 18), FONT_SANS, 12, smSubtle,
            "Steam and quakeclub.com soon", -1, qfalse);
}

static float UI_Home_DrawFriends(float x, float y, float w) {
    static const char* const rows[3] = {"Your Steam friends", "Who is in a match", "Who is in a lobby"};
    float ry = y + 40.0f;
    float h = 40.0f + 3 * 44.0f + 34.0f;
    int i;

    SM_Fill(x, y, w, h, UI_Home_SidePanel(), NULL);
    SM_Label(x + 16, SM_Baseline(FONT_SANS, 11, y + 12, 16), 11, smMuted, "Friends", 1.8f);
    SM_Fill(x + 16, y + 33, w - 32, 1, smBorder, NULL);

    for (i = 0; i < 3; i++) {
        SM_Fill(x + 16, ry + 8, 28, 28, smRaised, NULL);
        SM_Text(x + 54, SM_Baseline(FONT_SANS, 13, ry + 12, 20), FONT_SANS, 13, smMuted, rows[i], -1, qfalse);
        ry += 44.0f;
    }
    SM_Text(x + 16, SM_Baseline(FONT_SANS, 11, y + h - 28, 18), FONT_SANS, 11, smSubtle,
            "Once Steam is wired up", -1, qfalse);
    return h;
}

static void UI_Home_DrawSide(void) {
    float w = HM_SIDE_W;
    float x = smVW - w - HM_SIDE_RIGHT;
    // [ioquakelive] arriba a la derecha, a la altura del logo: con las tarjetas abajo, a media
    // altura pisaba la ultima
    float y = 104.0f;

    if (x < UI_Home_ColumnX() + HM_COL_W + 60.0f) {
        return;  // a narrow screen keeps the menu alone
    }
    UI_Home_DrawProfile(x, y, w);
    UI_Home_DrawFriends(x, y + 88.0f + 14.0f, w);
}

/*
===============
UI_Home_DrawMarks

[ioquakelive] La franja del pie: los cuatro nombres sobre los que esto esta parado, uno
al lado del otro como en un pie de patrocinadores, porque tambien son creditos. En orden:
QUAKE III ARENA, que es el codigo que id libero; ioquake3, el motor que lo mantiene vivo;
QUAKE LIVE, el juego que esto reimplementa; y Quake Club, la comunidad desde la que se
empuja este trabajo.

Los cuatro son los logos que dibujo el jugador, pasados a blanco con alfa por
code/tools/make_menu_marks.py y guardados en pak01. Van todos tenidos al mismo gris, asi
la fila se lee como una sola cosa y no como cuatro logos pegados.

Las medidas no salen de la imagen: cada una se dibuja al alto que la hace pesar lo mismo
que las demas, que es lo que hace que una apilada y una palabra sola convivan en la misma
linea. Salen de la vista previa que deja ese mismo script.
===============
*/
typedef struct {
    const char* shader;
    float w, h;
} hmMark_t;

static const hmMark_t hmMarks[] = {
    {"ui/ioql/mark_quake3", 112.0f, 80.0f},
    {"ui/ioql/mark_ioquake3", 97.0f, 40.0f},
    {"ui/ioql/mark_quakelive", 149.0f, 38.0f},
    {"ui/ioql/mark_quakeclub", 96.0f, 75.0f},
};

#define HM_MARK_GAP 54.0f
#define HM_MARK_H 80.0f  // el mas alto: la franja se centra sobre esa banda

// [ioquakelive] lo que queda a la izquierda de la columna social
static float UI_Home_MainW(void) {
    return smVW - HV_SOCIAL_W;
}

static float UI_Home_DrawMarks(float bottom) {
    static const vec4_t tint = {1.0f, 1.0f, 1.0f, 0.72f};
    static qhandle_t shaders[ARRAY_LEN(hmMarks)];
    static qboolean loaded;
    float total = 0;
    float y = bottom - HM_MARK_H;
    float x;
    int i;

    if (!loaded) {
        loaded = qtrue;
        for (i = 0; i < (int)ARRAY_LEN(hmMarks); i++) {
            shaders[i] = trap_R_RegisterShaderNoMip(hmMarks[i].shader);
        }
    }
    for (i = 0; i < (int)ARRAY_LEN(hmMarks); i++) {
        total += hmMarks[i].w + (i ? HM_MARK_GAP : 0);
    }
    x = (UI_Home_MainW() - total) * 0.5f;

    for (i = 0; i < (int)ARRAY_LEN(hmMarks); i++) {
        if (shaders[i]) {
            SM_PicColor(x, y + (HM_MARK_H - hmMarks[i].h) * 0.5f, hmMarks[i].w, hmMarks[i].h, shaders[i], tint,
                        NULL);
        }
        x += hmMarks[i].w + HM_MARK_GAP;
    }
    return y;
}

/*
===============
UI_Home_DrawLegal

The version under the list and, at the bottom right, the notice retail's splash
shows. There is no legal-notices page in this build, so the line is the notice
itself.

[ioquakelive] Over it va la franja de marcas, que es lo que hace de creditos: el
jugador pidio que fueran los logos y nada mas, sin pagina ni lista.
===============
*/
static void UI_Home_DrawLegal(void) {
    static const char* const notice =
        "QUAKE LIVE (c) 2007-2015, Id Software LLC, a ZeniMax Media company. All Rights Reserved.";
    char version[MAX_CVAR_VALUE_STRING];
    float h = 26.0f;
    float y = smBottom - 30 - h - (UI_Home_InMatch() ? 38.0f : 0.0f);
    float baseline = SM_Baseline(FONT_SANS, 12, y, h);
    float w = SM_Advance(FONT_SANS, 12, notice, -1);

    UI_Home_DrawMarks(y - 8.0f);
    SM_Text(UI_Home_MainW() - 40.0f - w, baseline, FONT_SANS, 12, smSubtle, notice, -1, qfalse);
    trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
    SM_Text(40.0f, baseline, FONT_SANS, 12, smSubtle, version, -1, qfalse);
}


/*
===============================================================================

EL MENU PRINCIPAL

[ioquakelive] 2026-09-18, cuarto enfoque: una adaptacion del menu principal de Valorant, que el
jugador mostro como referencia ("no taan literal"). De ahi se toma la estructura: la barra de
navegacion arriba con PLAY al centro y destacado, la columna social a la derecha a todo lo
alto, un panel de modos destacados a la izquierda, y el logo grande al centro con una banda de
color. La paleta, la letra y el cuadro de fondo son los de Quake Live.

===============================================================================
*/

#define HV_BAR_H 58.0f
#define HV_PLAY_TOP_W 250.0f  // el trapecio de PLAY: ancho arriba, contra el borde
#define HV_PLAY_BOT_W 196.0f  // y abajo, donde se angosta
#define HV_PLAY_H 66.0f
#define HV_TAB_SIZE 15.0f
#define HV_TAB_SPACING 1.6f
#define HV_TAB_GAP 48.0f
#define HV_PANEL_X 110.0f
#define HV_PANEL_Y 190.0f
#define HV_PANEL_W 380.0f
#define HV_PANEL_HEAD 64.0f
#define HV_ROW_H 60.0f

typedef struct {
    const char* title;
    int nav;  // -1: la pagina en la que ya estas
} hvTab_t;

// a los lados de PLAY, dos y dos
static const hvTab_t hvTabsLeft[] = {{"Home", -1}, {"Training", HM_NAV_TRAINING}};
static const hvTab_t hvTabsRight[] = {{"Replays", HM_NAV_REPLAYS}, {"Settings", HM_NAV_SETTINGS}};

typedef struct {
    const char* title;
    const char* factory;
    const char* icon;
    const char* map;
} hvMode_t;

// los modos destacados: los cuatro que mas se juegan, con el mapa con el que abre cada uno
static const hvMode_t hvModes[] = {
    {"Free For All", "ffa", "ui/assets/hud/ffa.png", "Campgrounds"},
    {"Duel", "duel", "ui/assets/hud/duel.png", "Bloodrun"},
    {"Clan Arena", "ca", "ui/assets/hud/ca.png", "Repent"},
    {"Capture The Flag", "ctf", "ui/assets/hud/ctf.png", "Spider Crossings"},
};

static float UI_Home_Top(void) {
    return -smTop / smScale;
}

// la barra de arriba: el logo a la izquierda, y las secciones con PLAY al centro

// una seccion de la barra: la pagina actual en blanco con una marca dorada, las demas en gris que
// se encienden bajo el cursor
static void UI_Home_DrawTab(const hvTab_t* tab, float x, float top, float w) {
    qboolean current = tab->nav < 0;
    qboolean over = !current && SM_Over(HM_HIT_TAB, tab->nav);
    float baseline = SM_Baseline(FONT_SANS, HV_TAB_SIZE, top + (HV_BAR_H - 22) * 0.5f, 22);

    SM_Label(x, baseline, HV_TAB_SIZE, current || over ? smText : smTextSoft, tab->title, HV_TAB_SPACING);
    if (current || over) {
        SM_Fill(x + (w - 22.0f) * 0.5f, top + HV_BAR_H - 12, 22.0f, 2.0f, smGold, NULL);
    }
    if (!current) {
        SM_Hit(x - 14, top, w + 28, HV_BAR_H, HM_HIT_TAB, tab->nav, NULL);
    }
}

static void UI_Home_DrawBar(void) {
    float top = UI_Home_Top();
    float mainW = UI_Home_MainW();
    float cx = mainW * 0.5f;
    float x;
    vec4_t bar;
    int i;

    VectorCopy(smPage, bar);
    bar[3] = 0.78f;
    SM_Fill(-1, top, mainW + 1, HV_BAR_H, bar, NULL);
    SM_Fill(-1, top + HV_BAR_H - 1, mainW + 1, 1, smBorder, NULL);

    if (hm2.logo) {
        SM_Pic(28.0f, top + (HV_BAR_H - 34.0f) * 0.5f, 136.0f, 34.0f, hm2.logo, NULL);
    }

    // PLAY: un trapecio claro contra el borde de arriba, mas ancho arriba que abajo. Se arma
    // con filas de un punto, que es lo que hay para dibujar una figura que no es un rectangulo.
    {
        qboolean over = SM_Over(HM_HIT_TAB, HM_NAV_PLAY);
        const float* fill = over ? smGold : smPrimary;
        float k;

        for (k = 0; k < HV_PLAY_H; k += 1.0f) {
            float w = HV_PLAY_TOP_W + (HV_PLAY_BOT_W - HV_PLAY_TOP_W) * (k / HV_PLAY_H);

            SM_Fill(cx - w * 0.5f, top + k, w, 1.05f, fill, NULL);
        }
        SM_Label(cx - SM_LabelWidth(24, "Play", 4.0f) * 0.5f, SM_Baseline(FONT_SANS, 24, top + 12, 34), 24,
                 smOnPrimary, "Play", 4.0f);
        SM_Hit(cx - HV_PLAY_TOP_W * 0.5f, top, HV_PLAY_TOP_W, HV_PLAY_H, HM_HIT_TAB, HM_NAV_PLAY, NULL);
    }

    // las demas, a los lados: la de la pagina actual en blanco con una marca dorada debajo
    x = cx - HV_PLAY_TOP_W * 0.5f - HV_TAB_GAP;
    for (i = (int)ARRAY_LEN(hvTabsLeft) - 1; i >= 0; i--) {
        float w = SM_LabelWidth(HV_TAB_SIZE, hvTabsLeft[i].title, HV_TAB_SPACING);

        x -= w;
        UI_Home_DrawTab(&hvTabsLeft[i], x, top, w);
        x -= HV_TAB_GAP;
    }
    x = cx + HV_PLAY_TOP_W * 0.5f + HV_TAB_GAP;
    for (i = 0; i < (int)ARRAY_LEN(hvTabsRight); i++) {
        float w = SM_LabelWidth(HV_TAB_SIZE, hvTabsRight[i].title, HV_TAB_SPACING);

        UI_Home_DrawTab(&hvTabsRight[i], x, top, w);
        x += w + HV_TAB_GAP;
    }
}

// la columna social: el perfil, los amigos y, abajo, la salida
static void UI_Home_DrawSocial(void) {
    static const char* const rows[3] = {"Your Steam friends", "Who is in a match", "Who is in a lobby"};
    char name[MAX_CVAR_VALUE_STRING];
    char model[MAX_QPATH];
    char* slash;
    float top = UI_Home_Top();
    float x = UI_Home_MainW();
    float w = smVW - x;
    float y = top;
    vec4_t panel;
    int i;

    VectorCopy(smPanel, panel);
    panel[3] = 0.95f;
    SM_Fill(x, top, w + 1, smBottom - top + 1, panel, NULL);
    SM_Fill(x, top, 1, smBottom - top, smBorder, NULL);

    // la cabecera, a la altura de la barra
    SM_Label(x + 26, SM_Baseline(FONT_SANS, 14, y + (HV_BAR_H - 20) * 0.5f, 20), 14, smText, "Social", 1.8f);
    SM_Fill(x, y + HV_BAR_H - 1, w, 1, smBorder, NULL);
    y += HV_BAR_H;

    // el perfil: la cara del modelo que llevas, tu nombre y de donde saldra lo demas
    trap_Cvar_VariableStringBuffer("name", name, sizeof(name));
    Q_CleanStr(name);
    trap_Cvar_VariableStringBuffer("model", model, sizeof(model));
    slash = strchr(model, '/');
    if (slash) {
        *slash = '\0';
    }
    if (!model[0]) {
        Q_strncpyz(model, "sarge", sizeof(model));
    }
    if (Q_stricmp(model, hm2.faceModel)) {
        Q_strncpyz(hm2.faceModel, model, sizeof(hm2.faceModel));
        hm2.face = trap_R_RegisterShaderNoMip(va("models/players/%s/icon_default", model));
    }
    if (hm2.face) {
        SM_Pic(x + 22, y + 16, 48, 48, hm2.face, NULL);
    }
    SM_Label(x + 84, SM_Baseline(FONT_SANS, 16, y + 18, 22), 16, smText, name[0] ? name : "Player", 0.4f);
    SM_Text(x + 84, SM_Baseline(FONT_SANS, 12, y + 42, 18), FONT_SANS, 12, smSubtle,
            "Steam and quakeclub.com soon", SM_Fit(FONT_SANS, 12, "Steam and quakeclub.com soon", w - 100), qfalse);
    y += 80.0f;
    SM_Fill(x, y, w, 1, smBorder, NULL);

    // los amigos
    SM_Label(x + 26, SM_Baseline(FONT_SANS, 12, y + 14, 18), 12, smMuted, "Friends", 1.8f);
    y += 46.0f;
    for (i = 0; i < 3; i++) {
        SM_Fill(x + 22, y + 6, 32, 32, smBorder, NULL);
        SM_Text(x + 68, SM_Baseline(FONT_SANS, 13, y + 12, 20), FONT_SANS, 13, smMuted, rows[i], -1, qfalse);
        y += 46.0f;
    }
    SM_Text(x + 26, SM_Baseline(FONT_SANS, 11, y + 4, 18), FONT_SANS, 11, smSubtle, "Once Steam is wired up", -1,
            qfalse);

    // la salida, al pie de la columna
    {
        float qy = smBottom - 64.0f;
        qboolean over = SM_Over(HM_HIT_QUIT, 0);

        SM_Fill(x, qy, w, 1, smBorder, NULL);
        if (over) {
            SM_Fill(x + 1, qy + 1, w - 1, 63, smRaised, NULL);
        }
        SM_Label(x + 26, SM_Baseline(FONT_SANS, 14, qy + 20, 22), 14, over ? smGold : smTextSoft, "Quit", 1.8f);
        SM_Hit(x, qy, w, 64, HM_HIT_QUIT, 0, NULL);
    }
}

// el panel de modos destacados, a la izquierda: cada uno abre Start Match con ese modo elegido
static void UI_Home_DrawModes(void) {
    static qhandle_t icons[ARRAY_LEN(hvModes)];
    static qboolean loaded;
    float x = HV_PANEL_X;
    float y = UI_Home_Top() + HV_PANEL_Y;
    float h = HV_PANEL_HEAD + ARRAY_LEN(hvModes) * HV_ROW_H;
    vec4_t panel;
    int i;

    if (!loaded) {
        for (i = 0; i < (int)ARRAY_LEN(hvModes); i++) {
            icons[i] = trap_R_RegisterShaderNoMip(hvModes[i].icon);
        }
        loaded = qtrue;
    }

    VectorCopy(smPanel, panel);
    panel[3] = 0.88f;
    SM_Fill(x, y, HV_PANEL_W, h, panel, NULL);

    // la cabecera: el titulo centrado y, debajo, contra quien se juega
    SM_Label(x + (HV_PANEL_W - SM_LabelWidth(15, "Featured Modes", 1.8f)) * 0.5f,
             SM_Baseline(FONT_SANS, 15, y + 14, 20), 15, smText, "Featured Modes", 1.8f);
    SM_Label(x + (HV_PANEL_W - SM_LabelWidth(11, "Against bots", 1.8f)) * 0.5f,
             SM_Baseline(FONT_SANS, 11, y + 38, 16), 11, smGold, "Against bots", 1.8f);
    y += HV_PANEL_HEAD;

    for (i = 0; i < (int)ARRAY_LEN(hvModes); i++) {
        qboolean over = SM_Over(HM_HIT_MODE, i);
        float mw = SM_Advance(FONT_SANS, 12, hvModes[i].map, -1);

        SM_Fill(x, y, HV_PANEL_W, 1, smBorder, NULL);
        if (over) {
            SM_Fill(x, y + 1, HV_PANEL_W, HV_ROW_H - 1, smRaised, NULL);
            SM_Fill(x, y + 1, 3, HV_ROW_H - 1, smGold, NULL);
        }
        if (icons[i]) {
            SM_Pic(x + 18, y + (HV_ROW_H - 32) * 0.5f, 32, 32, icons[i], NULL);
        }
        SM_Label(x + 64, SM_Baseline(FONT_SANS, 16, y + (HV_ROW_H - 22) * 0.5f, 22), 16, over ? smGold : smText,
                 hvModes[i].title, 0.6f);
        SM_Text(x + HV_PANEL_W - 18 - mw, SM_Baseline(FONT_SANS, 12, y + (HV_ROW_H - 18) * 0.5f, 18), FONT_SANS, 12,
                smMuted, hvModes[i].map, -1, qfalse);
        SM_Hit(x, y, HV_PANEL_W, HV_ROW_H, HM_HIT_MODE, i, NULL);
        y += HV_ROW_H;
    }
}

// al centro, el logo grande sobre el cuadro y una banda roja con la version
static void UI_Home_DrawCenter(void) {
    static const vec4_t red = {0.70f, 0.07f, 0.07f, 0.94f};
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    const char* band = "ioquakelive : beta";
    float bandW = 400.0f;  // la imagen del logo trae margen; la banda sigue al dibujo, no a la imagen
    float left = HV_PANEL_X + HV_PANEL_W;
    float cx = left + (UI_Home_MainW() - left) * 0.5f;
    float w = 520.0f;
    float y = UI_Home_Top() + (smBottom - UI_Home_Top()) * 0.36f;

    if (hm2.logo) {
        SM_Pic(cx - w * 0.5f, y, w, w * 0.25f, hm2.logo, NULL);
    }
    y += w * 0.25f + 6.0f;
    SM_Fill(cx - bandW * 0.5f, y, bandW, 44.0f, red, NULL);
    SM_Label(cx - SM_LabelWidth(19, band, 6.0f) * 0.5f, SM_Baseline(FONT_SANS, 19, y + 10, 24), 19, white, band, 6.0f);
}

// la sombra: bajo la barra, detras del panel de la izquierda y abajo, donde van los creditos
static void UI_Home_DrawValoShade(void) {
    float top = UI_Home_Top();
    vec4_t band;

    UI_Home_Wash(top, 200.0f, smPage, 0.6f, 0.0f);
    UI_Home_WashAcross(-1, 720.0f, smPage, 0.7f, 0.0f);
    UI_Home_Wash(smBottom - 290.0f, 180.0f, smPage, 0.0f, 0.85f);
    VectorCopy(smPage, band);
    band[3] = 0.85f;
    SM_Fill(-1, smBottom - 110.0f, smVW + 2, 111.0f, band, NULL);
}

void UI_Home_Open(void) {
    menuDef_t* menu = Menus_FindByName("main");
    itemDef_t* item = menu ? Menu_FindItemByName(menu, "home") : NULL;

    UI_Home_LoadMenu();
    hm2.menu = HM_MENU_MAIN;
    // the page takes keys before the mouse moves over it
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Home_Paint(void) {
    SM_BeginFrame();
    UI_Home_LoadMenu();

    UI_Home_DrawBackground();
    UI_Home_DrawValoShade();
    UI_Home_DrawCenter();
    UI_Home_DrawModes();
    UI_Home_DrawLegal();
    UI_Home_DrawSocial();
    UI_Home_DrawBar();

    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Home_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key == K_ESCAPE && hm2.menu != HM_MENU_MAIN) {
        hm2.menu = HM_MENU_MAIN;  // Back, the way the list's own Back does
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;  // Escape and the rest fall through to the menu
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    // [ioquakelive] la barra, los modos destacados y la salida del menu principal
    if (hit && hit->type == HM_HIT_TAB) {
        UI_Home_Go(hit->index);
        return qtrue;
    }
    if (hit && hit->type == HM_HIT_MODE) {
        UI_StartMatch_Preselect(hvModes[hit->index].factory);
        UI_Home_Go(HM_NAV_PLAY);
        return qtrue;
    }
    if (hit && hit->type == HM_HIT_QUIT) {
        UI_Home_Go(HM_NAV_QUIT);
        return qtrue;
    }
    if (!hit || hit->type != HM_HIT_ITEM) {
        return qtrue;
    }
    {
        const hmItem_t* item = &hmMenus[hm2.menu][hit->index];

        if (item->submenu >= 0) {
            hm2.menu = item->submenu;
        } else if (item->nav >= 0) {
            UI_Home_Go(item->nav);
        }
    }
    return qtrue;
}
