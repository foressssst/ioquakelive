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
#define HM_COL_W 430.0f      // what an entry's plate spans past its text
#define HM_PLATE_PAD 22.0f
#define HM_LOGO_Y 236.0f
#define HM_LOGO_W 340.0f
#define HM_LIST_Y 376.0f
#define HM_ITEM_H 76.0f
#define HM_ITEM_SIZE 30.0f
#define HM_ITEM_SPACING 0.7f
#define HM_NOTE_SIZE 14.0f
#define HM_BACK_SIZE 17.0f
#define HM_BACK_H 46.0f

// the painting's shape
#define HM_PAINTING_ASPECT (3122.0f / 1344.0f)

typedef enum {
    HM_HIT_NONE = SM_HIT_SHARED,
    HM_HIT_NAV,
    HM_HIT_BANNER,
    HM_HIT_ITEM
} hmHitType_t;

static const char* const hmNavNames[HM_NUM_NAV] = {"Home", "Play", "Training", "Replays", "Settings", "Quit"};

/*
The menu, as src/home.js lists it. Its "main" has Play, Statistics, Steam
Workshop, Steam Community, Settings and Quit, and Play and Statistics open into
their own lists. What this build has of those is Start Match, the Training
Center, Replays and Settings; the rest of retail's are its lobby and its stats
service, so Statistics, which would hold Replays alone, is not worth a list of
its own and Replays comes up to the main one.
*/
typedef struct {
    const char* title;
    const char* note;  // the line under it
    int nav;           // the page it opens, or -1
    int submenu;       // the list it opens instead, or -1
} hmItem_t;

#define HM_MENU_MAIN 0
#define HM_MENU_PLAY 1
#define HM_NUM_MENUS 2
#define HM_MAX_ITEMS 5

static const hmItem_t hmMenus[HM_NUM_MENUS][HM_MAX_ITEMS] = {
    {
        {"Play", "Start a match, or train with Crash", -1, HM_MENU_PLAY},
        {"Replays", "Watch the matches you recorded", HM_NAV_REPLAYS, -1},
        {"Settings", "Controls, video, sound and your player", HM_NAV_SETTINGS, -1},
        {"Quit", "Leave QUAKE LIVE", HM_NAV_QUIT, -1},
        {NULL, NULL, -1, -1},
    },
    {
        {"Start Match", "Pick an arena, a game type and bots", HM_NAV_PLAY, -1},
        {"Training Center", "Crash Course, strafe and rocket jump practice", HM_NAV_TRAINING, -1},
        {"Back", NULL, -1, HM_MENU_MAIN},
        {NULL, NULL, -1, -1},
        {NULL, NULL, -1, -1},
    },
};

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
UI_Home_Nav

quakeclub.com's bar: the container's width, labels 15px bold upper case, and a
2px gold rule under the page you are on. Quit sits alone at the right, where the
site has Sign In and retail had its power button.
===============
*/
void UI_Home_Nav(int active) {
    float left = UI_Home_Left();
    float x = left + HM_NAV_INSET;
    float base = SM_Baseline(FONT_SANS, HM_NAV_ITEM_SIZE, 47, 22);
    int i;

    for (i = 0; i < HM_NUM_NAV; i++) {
        float w = SM_LabelWidth(HM_NAV_ITEM_SIZE, hmNavNames[i], HM_NAV_ITEM_SPACING);
        float ix = i == HM_NAV_QUIT ? left + HM_CONTAINER_W - HM_NAV_INSET - w : x;
        qboolean on = i == active;
        qboolean over = SM_Over(HM_HIT_NAV, i);

        SM_Label(ix, base, HM_NAV_ITEM_SIZE, (on || over) ? smText : smTextSoft, hmNavNames[i],
                 HM_NAV_ITEM_SPACING);
        if (on) {
            SM_Pill(ix, base + 8, w, 2, smGold, NULL);
        }
        SM_Hit(ix - HM_NAV_ITEM_GAP * 0.5f, 30, w + HM_NAV_ITEM_GAP, 46, HM_HIT_NAV, i, NULL);
        if (i != HM_NAV_QUIT) {
            x += w + HM_NAV_ITEM_GAP;
        }
    }
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

The logo, and under it the list that opens into itself, over the painting.

===============================================================================
*/

static struct {
    qboolean loadedMenu;
    qhandle_t logo;
    int menu;  // which of hmMenus is showing
} hm2;

static void UI_Home_LoadMenu(void) {
    if (hm2.loadedMenu) {
        return;
    }
    hm2.loadedMenu = qtrue;
    hm2.menu = HM_MENU_MAIN;
    hm2.logo = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo");
}

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
    shade[3] = 0.90f;
    SM_Fill(-1, top, solid + 1, full, shade, NULL);
    UI_Home_WashAcross(solid, 560.0f, smPage, 0.90f, 0.0f);
    UI_Home_Wash(top, 160.0f, smPage, 0.60f, 0.0f);
    UI_Home_Wash(smBottom - 200.0f, 200.0f, smPage, 0.0f, 0.88f);
}

// a small chevron pointing right, what an entry that leads somewhere shows under the cursor
static void UI_Home_Chevron(float x, float y, const float* color) {
    int k;

    for (k = 0; k < 6; k++) {
        SM_Fill(x + k, y + k, 2.2f, 2.2f, color, NULL);
        SM_Fill(x + k, y + 10.0f - k, 2.2f, 2.2f, color, NULL);
    }
}

/*
===============
UI_Home_DrawColumn

The logo, then the list with its heading: the name you play under on the main
list, "Play" on Play's. An entry is its title in the site's bold upper case and a
line of what it does; under the cursor the title turns gold on a raised plate
with a chevron at its end. Back sits a little apart and smaller, as retail's
a.back does.
===============
*/
static void UI_Home_DrawColumn(void) {
    float x = UI_Home_ColumnX();
    float plateX = x - HM_PLATE_PAD;
    float plateW = HM_COL_W + 2 * HM_PLATE_PAD;
    float y = HM_LIST_Y;
    char heading[MAX_CVAR_VALUE_STRING];
    vec4_t plate;
    int i;

    VectorCopy(smRaised, plate);
    plate[3] = 0.78f;

    if (hm2.logo) {
        SM_Pic(x - 8, HM_LOGO_Y, HM_LOGO_W, HM_LOGO_W * 0.25f, hm2.logo, NULL);
    }

    if (hm2.menu == HM_MENU_PLAY) {
        Q_strncpyz(heading, "Play", sizeof(heading));
    } else {
        char name[MAX_CVAR_VALUE_STRING];

        trap_Cvar_VariableStringBuffer("name", name, sizeof(name));
        Q_CleanStr(name);
        Com_sprintf(heading, sizeof(heading), "Playing as %s", name);
    }
    SM_Label(x, SM_Baseline(FONT_SANS, 12, y, 18), 12, smGold, heading, 1.8f);
    y += 36;

    for (i = 0; i < HM_MAX_ITEMS; i++) {
        const hmItem_t* item = &hmMenus[hm2.menu][i];
        qboolean over;

        if (!item->title) {
            break;
        }
        over = SM_Over(HM_HIT_ITEM, i);

        if (item->submenu == HM_MENU_MAIN && hm2.menu != HM_MENU_MAIN) {
            // Back
            y += 14;
            if (over) {
                SM_Round(plateX, y, plateW, HM_BACK_H, SM_RADIUS_MD, plate, NULL);
            }
            SM_Label(x, SM_Baseline(FONT_SANS, HM_BACK_SIZE, y, HM_BACK_H), HM_BACK_SIZE,
                     over ? smGold : smTextSoft, item->title, 1.2f);
            SM_Hit(plateX, y, plateW, HM_BACK_H, HM_HIT_ITEM, i, NULL);
            y += HM_BACK_H;
            continue;
        }

        if (over) {
            SM_Round(plateX, y, plateW, HM_ITEM_H - 8, SM_RADIUS_MD, plate, NULL);
            UI_Home_Chevron(plateX + plateW - 34, y + (HM_ITEM_H - 8) * 0.5f - 6, smGold);
        }
        SM_Label(x, SM_Baseline(FONT_SANS, HM_ITEM_SIZE, y + 7, 36), HM_ITEM_SIZE, over ? smGold : smText,
                 item->title, HM_ITEM_SPACING);
        if (item->note) {
            SM_Text(x, SM_Baseline(FONT_SANS, HM_NOTE_SIZE, y + 42, 20), FONT_SANS, HM_NOTE_SIZE,
                    over ? smTextSoft : smMuted, item->note, SM_Fit(FONT_SANS, HM_NOTE_SIZE, item->note, HM_COL_W),
                    qfalse);
        }
        SM_Hit(plateX, y, plateW, HM_ITEM_H - 8, HM_HIT_ITEM, i, NULL);
        y += HM_ITEM_H;
    }
}

/*
===============
UI_Home_DrawLegal

The version under the list and, at the bottom right, the notice retail's splash
shows. There is no legal-notices page in this build, so the line is the notice
itself.
===============
*/
static void UI_Home_DrawLegal(void) {
    static const char* const notice =
        "QUAKE LIVE (c) 2007-2015, Id Software LLC, a ZeniMax Media company. All Rights Reserved.";
    char version[MAX_CVAR_VALUE_STRING];
    float h = 26.0f;
    float y = smBottom - 30 - h - (UI_Home_InMatch() ? 38.0f : 0.0f);
    float w = SM_Advance(FONT_SANS, 12, notice, -1);

    SM_Text(smVW - UI_Home_ColumnX() * 0.5f - w, SM_Baseline(FONT_SANS, 12, y, h), FONT_SANS, 12, smSubtle, notice,
            -1, qfalse);
    trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
    SM_Text(UI_Home_ColumnX(), SM_Baseline(FONT_SANS, 12, y, h), FONT_SANS, 12, smSubtle, version, -1, qfalse);
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
    UI_Home_DrawShade();
    UI_Home_DrawColumn();
    UI_Home_DrawLegal();

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
