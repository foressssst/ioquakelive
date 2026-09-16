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
// ui_home.c -- [QL] the main menu, and the frame every page of the front end sits in
//
// Retail's front end is its browser: a nav bar with Home, Play, Statistics and
// Community on the left and the settings gear and power button on the right, a
// random map as the page's background under a dark radial gradient (App's
// componentDidUpdate), and, over a match, a green bar at the bottom that takes you
// back to it. None of the lobby, friends or stats apply here, so the bar carries
// what this build has: home, play, training, replays, settings and quit.
//
// It looks like quakeclub.com: the bar is its centred one, labels in upper case
// with a gold rule under the page you are on, and the page is panels on the dark
// warm background, with the map behind them.
//

#include "ui_web.h"

#define HM_NAV_ITEM_SIZE 15.0f
#define HM_NAV_ITEM_SPACING 0.6f
#define HM_NAV_ITEM_GAP 26.0f
#define HM_NAV_INSET 39.0f

#define HM_HERO_H 300.0f
#define HM_CARD_H 176.0f

typedef enum {
    HM_HIT_NONE = SM_HIT_SHARED,
    HM_HIT_NAV,
    HM_HIT_CARD,
    HM_HIT_BANNER,
    HM_HIT_HERO
} hmHitType_t;

static const char* const hmNavNames[HM_NUM_NAV] = {"Home", "Play", "Training", "Replays", "Settings", "Quit"};

typedef struct {
    const char* eyebrow;
    const char* title;
    const char* note;
    int nav;
} hmCard_t;

static const hmCard_t hmCards[] = {
    {"Learn", "Training Center", "Crash Course and the training matches, with a coach who talks you through them.",
     HM_NAV_TRAINING},
    {"Watch", "Replays", "Every demo you have recorded, one click away.", HM_NAV_REPLAYS},
    {"Configure", "Settings", "Input, video, sound, HUD, teams and weapons -- everything retail offers.",
     HM_NAV_SETTINGS},
};

#define HM_NUM_CARDS ((int)ARRAY_LEN(hmCards))

// the hero's buttons
typedef enum {
    HM_HERO_START,
    HM_HERO_TRAINING,
    HM_HERO_RETURN,
    HM_HERO_LEAVE
} hmHero_t;

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

THE HOME PAGE

===============================================================================
*/

static const char* UI_Home_GametypeName(int gametype) {
    static const char* const names[] = {
        "Free For All", "Duel", "Race", "Team Deathmatch", "Clan Arena", "Capture the Flag", "One Flag CTF",
        "Overload", "Harvester", "Freeze Tag", "Domination", "Attack and Defend", "Red Rover"};

    return ((unsigned int)gametype <= 12u) ? names[gametype] : "Unknown Gametype";
}

static void UI_Home_HeroButton(float* x, float y, const char* text, int style, int which) {
    float w = SM_ButtonWidth(text, SM_BUTTON_H);

    SM_Button(*x, y, w, SM_BUTTON_H, text, style, SM_Over(HM_HIT_HERO, which));
    SM_Hit(*x, y, w, SM_BUTTON_H, HM_HIT_HERO, which, NULL);
    *x += w + 10;
}

static void UI_Home_DrawHero(float y) {
    float left = UI_Home_Left();
    float x = left + 40;
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    const char* eyebrow;
    const char* title;
    char note[256];
    int lines, k;

    SM_Round(left, y, HM_CONTAINER_W, HM_HERO_H, SM_RADIUS_LG, smPanel, NULL);

    if (UI_Home_InMatch()) {
        char info[MAX_INFO_STRING];
        char map[MAX_QPATH];

        trap_GetConfigString(CS_SERVERINFO, info, sizeof(info));
        Q_strncpyz(map, Info_ValueForKey(info, "mapname"), sizeof(map));
        eyebrow = "Match in progress";
        title = "Back to the fight";
        Com_sprintf(note, sizeof(note), "%s on %s. The match goes on while you are here.",
                    UI_Home_GametypeName(atoi(Info_ValueForKey(info, "g_gametype"))), map);
    } else {
        eyebrow = "Quake Live";
        title = "Start a match";
        Q_strncpyz(note, "Pick a game type and an arena, fill it with bots, and play it right here.",
                   sizeof(note));
    }

    SM_Label(x, SM_Baseline(FONT_SANS, 12, y + 56, 18), 12, smGold, eyebrow, 2.0f);
    SM_Label(x, SM_Baseline(FONT_SANS, 44, y + 80, 56), 44, smText, title, 0.4f);
    lines = SM_Wrap(FONT_SANS, 17, note, 560, starts, lens);
    for (k = 0; k < lines; k++) {
        SM_Text(x, SM_Baseline(FONT_SANS, 17, y + 150 + k * 26, 26), FONT_SANS, 17, smMuted, note + starts[k], lens[k],
                qfalse);
    }

    y += HM_HERO_H - 40 - SM_BUTTON_H;
    if (UI_Home_InMatch()) {
        UI_Home_HeroButton(&x, y, "Return to match", SM_BUTTON_PRIMARY, HM_HERO_RETURN);
        UI_Home_HeroButton(&x, y, "Leave match", SM_BUTTON_DANGER, HM_HERO_LEAVE);
    } else {
        UI_Home_HeroButton(&x, y, "Start match", SM_BUTTON_PRIMARY, HM_HERO_START);
        UI_Home_HeroButton(&x, y, "Training center", SM_BUTTON_SECONDARY, HM_HERO_TRAINING);
    }

    // the arena behind the page, named at the hero's corner
    if (hm.backdropMap[0]) {
        const char* label = va("Arena  %s", hm.backdropMap);
        float w = SM_LabelWidth(11, label, 1.4f);

        SM_Label(left + HM_CONTAINER_W - 40 - w, SM_Baseline(FONT_SANS, 11, y + 10, 16), 11, smSubtle, label, 1.4f);
    }
}

static void UI_Home_DrawCard(int i, float x, float y, float w) {
    const hmCard_t* card = &hmCards[i];
    qboolean over = SM_Over(HM_HIT_CARD, i);
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    float bw = SM_ButtonWidth("Open", SM_PILL_H);
    int lines, k;

    SM_Round(x, y, w, HM_CARD_H, SM_RADIUS_MD, over ? smRaised : smPanel, NULL);
    SM_Label(x + 25, SM_Baseline(FONT_SANS, 11, y + 22, 16), 11, smGold, card->eyebrow, 1.6f);
    SM_Label(x + 25, SM_Baseline(FONT_SANS, 20, y + 44, 28), 20, smText, card->title, 0.2f);
    lines = SM_Wrap(FONT_SANS, 14, card->note, w - 50, starts, lens);
    for (k = 0; k < lines && k < 3; k++) {
        SM_Text(x + 25, SM_Baseline(FONT_SANS, 14, y + 82 + k * 21, 21), FONT_SANS, 14, smMuted, card->note + starts[k],
                lens[k], qfalse);
    }
    SM_Button(x + w - 25 - bw, y + HM_CARD_H - 22 - SM_PILL_H, bw, SM_PILL_H, "Open",
              over ? SM_BUTTON_PRIMARY : SM_BUTTON_SECONDARY, qfalse);
    SM_Hit(x, y, w, HM_CARD_H, HM_HIT_CARD, i, NULL);
}

void UI_Home_Open(void) {
    menuDef_t* menu = Menus_FindByName("main");
    itemDef_t* item = menu ? Menu_FindItemByName(menu, "home") : NULL;

    // the page takes keys before the mouse moves over it
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Home_Paint(void) {
    float left, y, w;
    char version[MAX_CVAR_VALUE_STRING];
    int i;

    SM_BeginFrame();
    left = UI_Home_Left();

    UI_Home_Backdrop();
    UI_Home_Nav(HM_NAV_HOME);

    y = HM_NAV_H;
    UI_Home_DrawHero(y);
    y += HM_HERO_H + HM_PANEL_GAP;

    w = (HM_CONTAINER_W - (HM_NUM_CARDS - 1) * HM_PANEL_GAP) / HM_NUM_CARDS;
    for (i = 0; i < HM_NUM_CARDS; i++) {
        UI_Home_DrawCard(i, left + i * (w + HM_PANEL_GAP), y, w);
    }

    trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
    SM_Text(left, SM_Baseline(FONT_SANS, 12, y + HM_CARD_H + 24, 18), FONT_SANS, 12, smSubtle, version, -1, qfalse);

    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Home_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;  // Escape and the rest fall through to the menu
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    if (hit->type == HM_HIT_CARD) {
        UI_Home_Go(hmCards[hit->index].nav);
    } else if (hit->type == HM_HIT_HERO) {
        switch (hit->index) {
            case HM_HERO_START:
                UI_Home_Go(HM_NAV_PLAY);
                break;
            case HM_HERO_TRAINING:
                UI_Home_Go(HM_NAV_TRAINING);
                break;
            case HM_HERO_RETURN:
                trap_Cmd_ExecuteText(EXEC_APPEND, "web_hideBrowser\n");
                break;
            case HM_HERO_LEAVE:
                // ingame.menu's Leave Match
                trap_Cmd_ExecuteText(EXEC_APPEND, "disconnect\n");
                break;
            default:
                break;
        }
    }
    return qtrue;
}
