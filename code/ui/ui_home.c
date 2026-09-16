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
// app frame: a black column down the left with the QUAKE LIVE logo over a
// vertical menu, and a character standing at the bottom of the screen beside it
// (/img/characters/ranger.png). Its menu opens into itself -- Play gives Match
// Browser, Start Match, Training Center and Back -- and the pages it leads to
// (src/appframe.js, "#app-frame") are the ones with the nav bar across the top.
//
// So the menu is retail's, down to the column at 4% of the screen, 20% wide, the
// logo at 90% of it and the character at 24%; the character is pak00's ranger
// drawn as a model rather than a picture of one, since retail's picture lives in
// web.pak. What it is made of is quakeclub.com's: its surfaces, its type and its
// gold, and nothing moves.
//
// The bar the other pages carry is here too, since they share it, along with the
// background (a random map under the page's colour, the way retail's App picked
// one) and the green banner a match puts at the bottom of every page.
//

#include "ui_web.h"

#define HM_NAV_ITEM_SIZE 15.0f
#define HM_NAV_ITEM_SPACING 0.6f
#define HM_NAV_ITEM_GAP 26.0f
#define HM_NAV_INSET 39.0f

// the main menu, in the fractions of the screen retail's CSS gives it
#define HM_COL_LEFT 0.04f
#define HM_COL_W 0.20f
#define HM_CHAR_LEFT 0.24f
#define HM_LOGO_W 0.90f    // of the column
#define HM_LOGO_TOP 0.15f  // of the column, as its margin
#define HM_ITEM_SIZE 27.0f
#define HM_ITEM_H 58.0f
#define HM_ITEM_PAD 26.0f  // from the column's right edge

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
    int nav;       // the page it opens, or -1
    int submenu;   // the list it opens instead, or -1
} hmItem_t;

#define HM_MENU_MAIN 0
#define HM_MENU_PLAY 1
#define HM_NUM_MENUS 2
#define HM_MAX_ITEMS 5

static const hmItem_t hmMenus[HM_NUM_MENUS][HM_MAX_ITEMS] = {
    {
        {"Play", -1, HM_MENU_PLAY},
        {"Replays", HM_NAV_REPLAYS, -1},
        {"Settings", HM_NAV_SETTINGS, -1},
        {"Quit", HM_NAV_QUIT, -1},
        {NULL, -1, -1},
    },
    {
        {"Start Match", HM_NAV_PLAY, -1},
        {"Training Center", HM_NAV_TRAINING, -1},
        {"Back", -1, HM_MENU_MAIN},
        {NULL, -1, -1},
        {NULL, -1, -1},
    },
};

// iconic arenas whose shots pak00 carries, one of which is the page's background
static const char* const hmBackdrops[] = {
    "bloodrun", "campgrounds", "toxicity", "aerowalk", "furiousheights", "sinister", "lostworld",
    "hektik", "silence", "cure", "almostlost", "asylum", "battleforged", "trinity", "elder", "overkill",
};

static struct {
    qboolean loaded;
    qhandle_t backdrop;
    char backdropMap[MAX_QPATH];
} hm;

float UI_Home_Left(void) {
    return (smVW - HM_CONTAINER_W) * 0.5f;
}

/*
===============
UI_Home_Load

Picks the background once per session, the way retail's App picked a random map
for its page: one of the arenas above whose levelshot pak00 has.
===============
*/
static void UI_Home_Load(void) {
    int count = (int)ARRAY_LEN(hmBackdrops);
    int start, k;

    if (hm.loaded) {
        return;
    }
    hm.loaded = qtrue;
    start = (int)((unsigned int)trap_Milliseconds() % (unsigned int)count);
    for (k = 0; k < count; k++) {
        const char* map = hmBackdrops[(start + k) % count];
        fileHandle_t f;
        int len = trap_FS_FOpenFile(va("levelshots/%s.jpg", map), &f, FS_READ);

        if (len > 0) {
            trap_FS_FCloseFile(f);
            hm.backdrop = trap_R_RegisterShaderNoMip(va("levelshots/%s", map));
            Q_strncpyz(hm.backdropMap, map, sizeof(hm.backdropMap));
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

/*
===============
UI_Home_Backdrop

The map covers the screen whatever its shape, and the page's colour lies over it
thick enough for the panels to read: heavier at the top where the bar is and at
the bottom, lighter in the middle, which is what retail's radial gradient did.
===============
*/
void UI_Home_Backdrop(void) {
    float aspect = 1920.0f / 1080.0f;
    float top = -smTop / smScale;
    float full = smBottom - top;
    float w = smVW;
    float h = full;

    UI_Home_Load();
    SM_FillScreen(smPage);
    if (hm.backdrop) {
        if (w / h > aspect) {
            h = w / aspect;  // cover: overflow the height rather than letterbox
        } else {
            w = h * aspect;
        }
        SM_Pic((smVW - w) * 0.5f, top + (full - h) * 0.5f, w, h, hm.backdrop, NULL);
    }
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

Retail's #main-menu: the column, the logo, the menu that opens into itself, and
the character standing beside it.

===============================================================================
*/

static struct {
    qboolean loadedMenu;
    qhandle_t logo;
    int menu;  // which of hmMenus is showing
    playerInfo_t character;
    qboolean hasCharacter;
} hm2;

// the character retail's menu stands there: ranger, and pak00 has the model
static const char* const hmCharacters[] = {"ranger/default", "sarge/default"};

static void UI_Home_LoadMenu(void) {
    int k;

    if (hm2.loadedMenu) {
        return;
    }
    hm2.loadedMenu = qtrue;
    hm2.menu = HM_MENU_MAIN;
    hm2.logo = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo");

    for (k = 0; k < (int)ARRAY_LEN(hmCharacters); k++) {
        char model[MAX_QPATH];
        fileHandle_t f;
        const char* slash;
        int len;

        slash = strchr(hmCharacters[k], '/');
        Q_strncpyz(model, hmCharacters[k], slash ? (int)(slash - hmCharacters[k]) + 1 : sizeof(model));
        len = trap_FS_FOpenFile(va("models/players/%s/lower.md3", model), &f, FS_READ);
        if (len <= 0) {
            continue;
        }
        trap_FS_FCloseFile(f);
        // he holds the rocket launcher, the way the menu's picture has him armed
        SM_BuildPlayer(&hm2.character, hmCharacters[k], 165, WP_ROCKET_LAUNCHER);
        SM_TurnPlayer(&hm2.character, 165);
        hm2.hasCharacter = qtrue;
        return;
    }
}

/*
===============
UI_Home_DrawCharacter

The character stands on the bottom edge of the screen, its left at 24% of the
width, as .character does (fixed, bottom 0, left 24%). It is a model held at one
moment rather than an animation: retail's is a picture.
===============
*/
static void UI_Home_DrawCharacter(void) {
    float top = -smTop / smScale;
    float full = smBottom - top;
    float h = full * 0.74f;
    float w = h * 0.62f;
    float x = smVW * HM_CHAR_LEFT;

    if (!hm2.hasCharacter) {
        return;
    }
    // over a match the banner takes the bottom of the screen, so he stands on it
    SM_DrawPlayerStill(&hm2.character, x, smBottom - h - (UI_Home_InMatch() ? 38.0f : 0.0f), w, h);
}

/*
===============
UI_Home_DrawColumn

.nav-column: the full height of the screen from 4% to 24%, its text against the
right edge. Retail fills it with rgba(0,0,0,0.8); here it is the site's panel,
and the item under the cursor takes its gold and a rule down the column's edge,
the way .nav-column a:hover does.
===============
*/
static void UI_Home_DrawColumn(void) {
    float top = -smTop / smScale;
    float full = smBottom - top;
    float x = smVW * HM_COL_LEFT;
    float w = smVW * HM_COL_W;
    float right = x + w;
    float logoW = w * HM_LOGO_W;
    float y;
    int i;

    SM_Fill(x, top, w, full, smPanel, NULL);

    // the logo, as wide as the column allows, with retail's margin over it
    if (hm2.logo) {
        SM_Pic(x + (w - logoW) * 0.5f, top + w * HM_LOGO_TOP, logoW, logoW * 0.25f, hm2.logo, NULL);
    }

    y = top + w * HM_LOGO_TOP + logoW * 0.25f + w * HM_LOGO_TOP;
    for (i = 0; i < HM_MAX_ITEMS; i++) {
        const hmItem_t* item = &hmMenus[hm2.menu][i];
        qboolean over;
        float tw;

        if (!item->title) {
            break;
        }
        // "Back" sits a line below what it goes back from (a.back, margin-top 1em)
        if (item->submenu == HM_MENU_MAIN && hm2.menu != HM_MENU_MAIN) {
            y += HM_ITEM_SIZE;
        }
        over = SM_Over(HM_HIT_ITEM, i);
        if (over) {
            vec4_t shade = {smPage[0], smPage[1], smPage[2], 0.30f};

            SM_Fill(x, y, w, HM_ITEM_H, shade, NULL);
            SM_Fill(right - 3, y, 3, HM_ITEM_H, smGold, NULL);
        }
        tw = SM_LabelWidth(HM_ITEM_SIZE, item->title, 1.0f);
        SM_Label(right - HM_ITEM_PAD - tw, SM_Baseline(FONT_SANS, HM_ITEM_SIZE, y, HM_ITEM_H), HM_ITEM_SIZE,
                 over ? smGold : smText, item->title, 1.0f);
        SM_Hit(x, y, w, HM_ITEM_H, HM_HIT_ITEM, i, NULL);
        y += HM_ITEM_H;
    }
}

/*
===============
UI_Home_DrawLegal

.legal-links sits at the bottom right over black, and the notice retail's splash
shows is its own line. There is no legal-notices page in this build, so the line
is the notice itself.
===============
*/
static void UI_Home_DrawLegal(void) {
    static const char* const notice =
        "QUAKE LIVE (c) 2007-2015, Id Software LLC, a ZeniMax Media company. All Rights Reserved.";
    char version[MAX_CVAR_VALUE_STRING];
    float h = 26.0f;
    float y = smBottom - h - (UI_Home_InMatch() ? 38.0f : 0.0f);
    float w = SM_Advance(FONT_SANS, 12, notice, -1);

    SM_Text(smVW - 24 - w, SM_Baseline(FONT_SANS, 12, y, h), FONT_SANS, 12, smSubtle, notice, -1, qfalse);
    trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
    SM_Text(smVW * HM_COL_LEFT, SM_Baseline(FONT_SANS, 12, y, h), FONT_SANS, 12, smSubtle, version, -1, qfalse);
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

    UI_Home_Backdrop();
    UI_Home_DrawCharacter();
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
