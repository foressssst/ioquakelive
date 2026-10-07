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
// Retail's main menu (web.pak src/home.js, "#main-menu") is a screen of its own,
// outside the app frame: the logo over a vertical menu down the left, with art
// beside it, and a menu that opens into itself. This keeps that shape and dresses
// it in this build's own surfaces, type and gold.
//
// The bar the other pages carry is here too, since they share it, along with the
// background and the banner a running match puts at the bottom of every page.
//

#include "ui_web.h"

typedef enum {
    HM_HIT_NONE = SM_HIT_SHARED,
    HM_HIT_NAV,
    HM_HIT_BANNER,
    HM_HIT_ITEM,
    HM_HIT_CARD,  // [ioquakelive] a card of the main menu
    HM_HIT_QUIT,
    HM_HIT_TAB,   // [ioquakelive] a section of the top bar
    HM_HIT_SOCIAL, // [ioquakelive] the social strip's background: clicks do not pass through
    HM_HIT_FRIEND, // [ioquakelive] a row of the friends list
    HM_HIT_CONTEXT,      // an item of a friend's context menu
    HM_HIT_LEGAL,        // the main menu's Legal Notices link
    HM_HIT_CONTEXT_BACK, // the screen under it, which closes it
    HM_HIT_MENU    // [ioquakelive] a row of the PLAY dropdown
} hmHitType_t;

// which of the lists is showing
#define HM_MENU_MAIN 0
#define HM_MENU_PLAY 1
#define HM_MENU_STATS 2
#define HM_MENU_COMMUNITY 3
#define HM_NUM_MENUS 4
#define HM_MAX_ITEMS 6

static struct {
    qboolean loadedMenu;
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
}


/*
===============
UI_Home_Backdrop

[QL] What the other pages sit on: the #app's background, the same as the main menu's
===============
*/
void UI_Home_RetailBackdrop(void);

void UI_Home_Backdrop(void) {
    UI_Home_RetailBackdrop();
}

/*
===============================================================================

THE FRAME

[QL] web.pak src/appframe.js: a 75px bar black at 0.8, with the ProfileCard at the left and the
navItems -- the house, PLAY, STATISTICS and COMMUNITY 250px each, the power button and the gear at
the right -- in Handel Gothic 18pt upper case; the chosen one yellow over black 0.3 with a 3px
yellow line under it. PLAY, STATISTICS and COMMUNITY open a menu under them, as wide as they are.
Down the left, the Lobby and under it the friends

===============================================================================
*/

#define HV_MAX_FRIENDS 32

// the list, re-read every so often: asking Steam for it every frame would be hundreds of calls
// for nothing, since a friend does not change state that often
static struct {
    uiFriend_t list[HV_MAX_FRIENDS];
    int index[HV_MAX_FRIENDS];  // their place in Steam's list, which is what actions go by
    int count;
    int updated;
} hvf;

#define HM_FRIEND_H 62.0f

static void UI_Home_LoadRetail(void);
static void UI_Home_FriendRefresh(void);
static void UI_Home_DrawFriends(float x, float y, float w, float h);

#define HF_BAR_H 75.0f
#define HF_ITEM_W 75.0f
#define HF_WIDE_W 250.0f
#define HF_FIRST_X 435.0f     // after the profile card: 30 + 370 + 35
#define HF_NAV_SIZE 24.0f     // 18pt
#define HF_MENU_SIZE (14.0f * 4.0f / 3.0f)  // 14pt
#define HF_MENU_ROW 48.5f     // li: 0.5em over and under a 1.6 line
#define HF_COL_X 30.0f
#define HF_COL_W 370.0f

static const vec4_t hfBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t hfBlack75 = {0.0f, 0.0f, 0.0f, 0.75f};
static const vec4_t hfBlack30 = {0.0f, 0.0f, 0.0f, 0.3f};
static const vec4_t hfBlack20 = {0.0f, 0.0f, 0.0f, 0.2f};
static const vec4_t hfWhite = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t hfMenuHoverText = {17 / 255.0f, 17 / 255.0f, 17 / 255.0f, 1.0f};
static const vec4_t hfButtonBorder = {77 / 255.0f, 77 / 255.0f, 77 / 255.0f, 1.0f};

typedef struct {
    const char* title;
    int nav;          // the page it opens, or -1
    const char* url;  // or what it opens where Steam's overlay would
} hfMenuItem_t;

// appframe.js's playContext, statsContext and communityContext
static const hfMenuItem_t hfMenus[3][3] = {
    {{"Match Browser", HM_NAV_BROWSER, NULL}, {"Start Match", HM_NAV_PLAY, NULL}, {"Training Center", HM_NAV_TRAINING, NULL}},
    {{"Career Stats", HM_NAV_STATS, NULL}, {"Recent Matches", HM_NAV_MATCHES, NULL}, {"Replays", HM_NAV_REPLAYS, NULL}},
    {{"Steam Workshop", -1, "http://steamcommunity.com/app/282440/workshop"},
     {"Steam Community", -1, "http://steamcommunity.com/app/282440"}},
};
static const char* const hfTabs[3] = {"Play", "Statistics", "Community"};

static int hfOpen = -1;  // the menu that is down, -1 none

float UI_Home_Left(void) {
    return 430.0f;
}

float UI_Home_ContentW(void) {
    return smVW - 430.0f - 30.0f;
}

// a FontAwesome glyph in the middle of a navItem
static void UI_Home_NavIcon(float x, float top, int glyph, const float* color) {
    float w = SM_IconWidth(HF_NAV_SIZE, glyph);

    SM_Icon(x + (HF_ITEM_W - w) * 0.5f, top + HF_BAR_H * 0.5f + HF_NAV_SIZE * SM_AWESOME_BASE, HF_NAV_SIZE, color,
            glyph);
}

// a navItem's box: black 0.3 when it is chosen or under the cursor, and the chosen one's line
static const float* UI_Home_NavBox(float x, float top, float w, qboolean chosen, qboolean over) {
    if (chosen || over) {
        SM_Fill(x, top, w, HF_BAR_H, hfBlack30, NULL);
    }
    if (chosen) {
        SM_Fill(x, top + HF_BAR_H - 3.0f, w, 3.0f, smGold, NULL);
    }
    return (chosen || over) ? smGold : hfWhite;
}

/*
===============
The friend's context menu

[QL] friendlist.js opens web.pak's context.js on a friend: a white list at the cursor, 5px
down and right, over the whole screen darkened to 0.2, whose item under the cursor goes
yellow; any click closes it, and one on an item does what it says. Join Match comes first
when the friend is on a server (it opens the server's game info); there are no lobbies.
===============
*/
typedef enum {
    HC_JOIN,
    HC_MESSAGE,
    HC_INVITE,
    HC_STATS,
    HC_ACHIEVEMENTS,
    HC_PROFILE
} hcItem_t;

static const char* const hcNames[] = {"Join Match", "Send Message", "Send Invite", "View Statistics",
                                      "View Achievements", "Show Steam Profile"};

static struct {
    qboolean open;
    float x, y;
    int index;  // the friend's place in Steam's list
    uiFriend_t fr;
    int items[6];
    int numItems;
} hfc;

static const vec4_t hcWhite = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t hcBlack = {0.0f, 0.0f, 0.0f, 1.0f};

static void UI_Home_ContextOpen(int index) {
    int i;

    for (i = 0; i < hvf.count; i++) {
        if (hvf.index[i] == index) {
            break;
        }
    }
    if (i == hvf.count) {
        return;
    }
    hfc.open = qtrue;
    hfc.index = index;
    hfc.fr = hvf.list[i];
    hfc.x = smMX + 5;
    hfc.y = smMY + 5;
    hfc.numItems = 0;
    if (hfc.fr.server && hfc.fr.address[0]) {
        hfc.items[hfc.numItems++] = HC_JOIN;
    }
    hfc.items[hfc.numItems++] = HC_MESSAGE;
    hfc.items[hfc.numItems++] = HC_INVITE;
    hfc.items[hfc.numItems++] = HC_STATS;
    hfc.items[hfc.numItems++] = HC_ACHIEVEMENTS;
    hfc.items[hfc.numItems++] = HC_PROFILE;
}

static void UI_Home_ContextDo(int item) {
    switch (item) {
        case HC_JOIN:
            UI_Browser_ShowGameInfo(hfc.fr.address, hfc.fr.name);
            break;
        case HC_MESSAGE:
            trap_SteamFriendAction(hfc.index, UI_FRIEND_MESSAGE);
            break;
        case HC_INVITE:
            trap_SteamFriendAction(hfc.index, UI_FRIEND_INVITE);
            break;
        case HC_STATS:
            UI_Stats_Show(hfc.fr.steamId);
            break;
        case HC_ACHIEVEMENTS:
            trap_SteamFriendAction(hfc.index, UI_FRIEND_ACHIEVEMENTS);
            break;
        case HC_PROFILE:
            trap_SteamFriendAction(hfc.index, UI_FRIEND_PROFILE);
            break;
        default:
            break;
    }
}

// #context li: 0.25em over and under, 1em aside, the page's 15px text
static void UI_Home_DrawContext(void) {
    float top = -smTop / smScale;
    float w = 0, rowH = 24 + 7.5f;
    int i;

    if (!hfc.open) {
        return;
    }
    SM_Fill(-1, top, smVW + 2, smBottom - top + 1, hfBlack20, NULL);
    SM_Hit(0, top, smVW, smBottom - top, HM_HIT_CONTEXT_BACK, 0, NULL);
    for (i = 0; i < hfc.numItems; i++) {
        float tw = SM_Advance(FONT_SANS, 15, hcNames[hfc.items[i]], -1) + 30;

        w = tw > w ? tw : w;
    }
    SM_Fill(hfc.x, hfc.y, w, rowH * hfc.numItems, hcWhite, NULL);
    for (i = 0; i < hfc.numItems; i++) {
        float ry = hfc.y + i * rowH;

        if (SM_Over(HM_HIT_CONTEXT, i)) {
            SM_Fill(hfc.x, ry, w, rowH, smGold, NULL);
        }
        SM_Text(hfc.x + 15, SM_Baseline(FONT_SANS, 15, ry + 3.75f, 24), FONT_SANS, 15, hcBlack, hcNames[hfc.items[i]],
                -1, qfalse);
        SM_Hit(hfc.x, ry, w, rowH, HM_HIT_CONTEXT, i, NULL);
    }
}

/*
===============
UI_Home_PageTop

[QL] The bar and the left column of every page but the main menu
===============
*/
void UI_Home_PageTop(int active) {
    char name[MAX_CVAR_VALUE_STRING];
    float top = -smTop / smScale;
    float x;
    int tab = (active == HM_NAV_REPLAYS || active == HM_NAV_STATS || active == HM_NAV_MATCHES) ? 1 : (active == HM_NAV_BROWSER || active == HM_NAV_PLAY ||
                                             active == HM_NAV_TRAINING) ? 0 : -1;
    qhandle_t face;
    int i;

    UI_Home_LoadMenu();
    UI_Home_LoadRetail();

    // .nav.orow: the whole width but the 3px the scrollbar kept
    SM_Fill(0, top, smVW - 3.0f, HF_BAR_H, hfBlack80, NULL);

    // the ProfileCard, 30px in: the avatar as tall as the card and the name beside it
    face = UI_Home_Face();
    x = HF_COL_X;
    if (face) {
        SM_Pic(x, top + 5.0f, 65.0f, 65.0f, face, NULL);
        x += 65.0f + 7.5f;
    }
    trap_Cvar_VariableStringBuffer("name", name, sizeof(name));
    SM_Text(x, SM_Baseline(FONT_SANS, 15.0f, top + 5.0f, 24.0f), FONT_SANS, 15.0f, smText, name[0] ? name : "Player",
            SM_Fit(FONT_SANS, 15.0f, name, HF_COL_X + HF_COL_W - x), qfalse);
    // and under it the wins and KDR, which the card asks Steam for each time it comes up
    UI_Stats_CardFor(active);
    UI_Stats_DrawCardLine(x, top + 29.0f, HF_COL_X + HF_COL_W - x);

    // the house
    x = HF_FIRST_X;
    UI_Home_NavIcon(x, top, SM_FA_HOME,
                    UI_Home_NavBox(x, top, HF_ITEM_W, qfalse, SM_Over(HM_HIT_NAV, HM_NAV_HOME)));
    SM_Hit(x, top, HF_ITEM_W, HF_BAR_H, HM_HIT_NAV, HM_NAV_HOME, NULL);
    x += HF_ITEM_W;

    // PLAY, STATISTICS, COMMUNITY
    for (i = 0; i < 3; i++) {
        char upper[32];
        const float* color = UI_Home_NavBox(x, top, HF_WIDE_W, tab == i || hfOpen == i, SM_Over(HM_HIT_TAB, i));
        int k;

        for (k = 0; hfTabs[i][k] && k < (int)sizeof(upper) - 1; k++) {
            upper[k] = (char)toupper((unsigned char)hfTabs[i][k]);
        }
        upper[k] = '\0';
        SM_Text(x + (HF_WIDE_W - SM_Advance(FONT_DEFAULT, HF_NAV_SIZE, upper, -1)) * 0.5f,
                SM_Baseline(FONT_DEFAULT, HF_NAV_SIZE, top, HF_BAR_H), FONT_DEFAULT, HF_NAV_SIZE, color, upper, -1,
                qfalse);
        SM_Hit(x, top, HF_WIDE_W, HF_BAR_H, HM_HIT_TAB, i, NULL);
        x += HF_WIDE_W;
    }

    // at the right: the power button, and the gear left of it
    x = smVW - 3.0f - HF_ITEM_W;
    UI_Home_NavIcon(x, top, SM_FA_POWER, UI_Home_NavBox(x, top, HF_ITEM_W, qfalse, SM_Over(HM_HIT_NAV, HM_NAV_QUIT)));
    SM_Hit(x, top, HF_ITEM_W, HF_BAR_H, HM_HIT_NAV, HM_NAV_QUIT, NULL);
    x -= HF_ITEM_W;
    UI_Home_NavIcon(x, top, SM_FA_GEAR,
                    UI_Home_NavBox(x, top, HF_ITEM_W, active == HM_NAV_SETTINGS, SM_Over(HM_HIT_NAV, HM_NAV_SETTINGS)));
    SM_Hit(x, top, HF_ITEM_W, HF_BAR_H, HM_HIT_NAV, HM_NAV_SETTINGS, NULL);

    // the left column: the Lobby (header black 0.75, its body black 0.8 down to 51%)
    {
        float colTop = top + HM_NAV_H;
        float colH = smBottom - colTop;
        float lobbyBottom = colTop + colH * 0.49f;
        float bw = 88.2f;
        const char* create = "CREATE";

        SM_Fill(HF_COL_X, colTop, HF_COL_W, 25.6f, hfBlack75, NULL);
        SM_Text(HF_COL_X + 10.0f, SM_Baseline(FONT_DEFAULT, 16.0f, colTop, 25.6f), FONT_DEFAULT, 16.0f, smGold, "Lobby", -1,
                qfalse);
        SM_Fill(HF_COL_X, colTop + 25.6f, HF_COL_W, lobbyBottom - colTop - 25.6f, hfBlack80, NULL);

        // skeleton's button: 30px high, a #4d4d4d line, 11px bold upper case spaced 1px
        SM_Fill(HF_COL_X + (HF_COL_W - bw) * 0.5f, colTop + 65.0f, bw, 1, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_COL_W - bw) * 0.5f, colTop + 94.0f, bw, 1, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_COL_W - bw) * 0.5f, colTop + 65.0f, 1, 30, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_COL_W + bw) * 0.5f - 1, colTop + 65.0f, 1, 30, hfButtonBorder, NULL);
        SM_SpacedText(HF_COL_X + (HF_COL_W - SM_SpacedWidth(FONT_SANS, 11.0f, create, 1.0f) + 1.0f) * 0.5f,
                      SM_Baseline(FONT_SANS, 11.0f, colTop + 65.0f, 30.0f), FONT_SANS, 11.0f, smText, create, 1.0f,
                      qtrue);
        SM_Centered(HF_COL_X + HF_COL_W * 0.5f, SM_Baseline(FONT_SANS, 15.0f, colTop + 105.0f, 24.0f), 15.0f, smText,
                    "Not in a lobby.", qfalse);

        // the friends, from 51% down to 30px off the bottom
        UI_Home_FriendRefresh();
        if (hvf.count > 0) {
            float fTop = colTop + colH * 0.51f;
            float fH = smBottom - 30.0f - fTop;
            float rows = hvf.count * HM_FRIEND_H;

            SM_Fill(HF_COL_X, fTop, HF_COL_W, rows < fH ? rows : fH, hfBlack80, NULL);
            UI_Home_DrawFriends(HF_COL_X, fTop, HF_COL_W, fH);
        }
    }
}

// the menu under PLAY, STATISTICS or COMMUNITY: drawn last, over the page
static void UI_Home_DrawNavMenu(void) {
    float top = -smTop / smScale;
    float x = HF_FIRST_X + HF_ITEM_W + hfOpen * HF_WIDE_W;
    float y = top + HF_BAR_H;
    int i;

    if (hfOpen < 0) {
        return;
    }
    // #context-wrapper: the whole screen at black 0.2, and a click on it closes the menu
    SM_Fill(-1, top, smVW + 2, smBottom - top + 1, hfBlack20, NULL);
    SM_Hit(0, top, smVW, smBottom - top, HM_HIT_SOCIAL, 0, NULL);
    for (i = 0; i < 3 && hfMenus[hfOpen][i].title; i++) {
        qboolean over = SM_Over(HM_HIT_MENU, i);

        SM_Fill(x, y, HF_WIDE_W, HF_MENU_ROW, over ? smGold : smRaised, NULL);
        SM_Text(x + HF_MENU_SIZE, SM_Baseline(FONT_DEFAULT, HF_MENU_SIZE, y, HF_MENU_ROW), FONT_DEFAULT, HF_MENU_SIZE,
                over ? hfMenuHoverText : smText, hfMenus[hfOpen][i].title, -1, qfalse);
        SM_Hit(x, y, HF_WIDE_W, HF_MENU_ROW, HM_HIT_MENU, i, NULL);
        y += HF_MENU_ROW;
    }
}

/*
===============
UI_Home_Header

[QL] A page's h1.title: a black 0.8 bar the column's width, the title in Handel Gothic 24pt with
0.25em over and under it and 0.2em in, and 2px under the bar
===============
*/
float UI_Home_Header(float y, const char* eyebrow, const char* title) {
    float left = UI_Home_Left();

    (void)eyebrow;
    SM_Fill(left, y, HM_CONTAINER_W, 54.4f, hfBlack80, NULL);
    SM_Text(left + 6.4f, SM_Baseline(FONT_DEFAULT, 32.0f, y + 8.0f, 38.4f), FONT_DEFAULT, 32.0f, smText, title, -1,
            qfalse);
    return y + 54.4f + 2.0f;
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
    return smBottom - 30.0f;  // .body.orow: bottom 30px
}

// [ioquakelive] the block's middle is the screen's middle, not the middle of what the top row
// leaves, which sat it lower; it still never climbs over the top row, nor under the match
// banner
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
        case HM_NAV_BROWSER:
            Menus_CloseAll();
            Menus_ActivateByName("browser");
            break;
        case HM_NAV_REPLAYS:
            Menus_CloseAll();
            Menus_ActivateByName("demo");
            break;
        case HM_NAV_STATS:
            UI_Stats_Show("");
            break;
        case HM_NAV_MATCHES:
            Menus_CloseAll();
            Menus_ActivateByName("matches");
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

    // a friend's menu takes the click, and closes either way
    if (hfc.open) {
        hfc.open = qfalse;
        if (hit && hit->type == HM_HIT_CONTEXT) {
            UI_Home_ContextDo(hfc.items[hit->index]);
        }
        return qtrue;
    }
    if (hit && hit->type == HM_HIT_FRIEND) {
        UI_Home_ContextOpen(hit->index);
        return qtrue;
    }

    // a menu that is down takes the click: a row of it, or anywhere else to close it
    if (hfOpen >= 0) {
        int open = hfOpen;

        hfOpen = -1;
        if (hit && hit->type == HM_HIT_MENU) {
            const hfMenuItem_t* item = &hfMenus[open][hit->index];

            if (item->nav >= 0) {
                UI_Home_Go(item->nav);
            } else if (item->url) {
                trap_Sys_OpenURL(item->url);
            }
        }
        return qtrue;
    }
    if (!hit) {
        return qfalse;
    }
    if (hit->type == HM_HIT_TAB) {
        hfOpen = hit->index;
        return qtrue;
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
your match." and sends web_hideBrowser when clicked. Its padding is 0.25em around one 24px line
of the page's 15px, and nothing changes under the mouse
===============
*/
void UI_Home_Banner(void) {
    static const char* const text = "You are currently in-game. Click here to return to your match.";
    float h = 31.5f;
    float y = smBottom - h;

    UI_Home_DrawNavMenu();
    UI_Home_DrawContext();
    if (!UI_Home_InMatch()) {
        return;
    }
    SM_Fill(-1, y, smVW + 2, h, smSuccess, NULL);
    SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 15, y + 3.75f, 24.0f), 15, smText, text, qfalse);
    SM_Hit(0, y, smVW, h, HM_HIT_BANNER, 0, NULL);
}

/*
===============================================================================

THE MAIN MENU

[QL] Retail's #main-menu (web.pak src/home.js and css/main.css), in its page's pixels at 1920x1080:
a column 20% wide at 4% from the left, black at 0.8, with the logo and the menu in Handel Gothic
24pt set to the right; the Ranger standing at 24%; on the right the player's card, the Friends and
Lobby switch and the list under it; and the legal notice the first time the menu is up.

===============================================================================
*/

#define HM_ITEM_FONT 32.0f    // 24pt
#define HM_ITEM_PAD 16.0f     // padding: 0.5em
#define HM_ITEM_LINE 51.2f    // skeleton's line-height, 1.6
#define HM_ITEM_H (HM_ITEM_LINE + 2.0f * HM_ITEM_PAD)
#define HM_ITEM_BORDER 3.0f
#define HM_BACK_GAP 32.0f     // a.back: margin-top 1em
#define HM_LOGO_GAP 4.5f      // the logo is inline: its line runs 4.5px past it, measured
#define HM_CARD_TOP 75.0f
#define HM_CARD_H 75.0f
#define HM_CARD_PAD 7.5f      // padding: 0.5em
#define HM_SELECTOR_TOP 155.0f
#define HM_SELECTOR_H 25.0f
#define HM_LIST_TOP 183.0f
#define HM_LEGALS_MS 5000     // setTimeout(this.skipLegals, 5000)
#define HM_LEGALS_FADE 500    // .legals-leave: opacity .5s
#define HM_TEXT 15.0f

static const vec4_t hmBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t hmBlack30 = {0.0f, 0.0f, 0.0f, 0.3f};
static const vec4_t hmWhite = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t hmNameGrey = {106 / 255.0f, 106 / 255.0f, 106 / 255.0f, 1.0f};  // .name i

typedef struct {
    const char* title;
    int nav;          // the page it opens, or -1
    int submenu;      // the list it opens, or -1
    const char* url;  // what it opens where Steam's overlay would
    qboolean back;    // a.back, a line further down
} hmItem_t;

// src/home.js's menu
static const hmItem_t hmMenus[HM_NUM_MENUS][HM_MAX_ITEMS] = {
    {
        {"Play", -1, HM_MENU_PLAY, NULL, qfalse},
        {"Statistics", -1, HM_MENU_STATS, NULL, qfalse},
        {"Steam Workshop", -1, -1, "http://steamcommunity.com/app/282440/workshop", qfalse},
        {"Steam Community", -1, -1, "http://steamcommunity.com/app/282440", qfalse},
        {"Settings", HM_NAV_SETTINGS, -1, NULL, qfalse},
        {"Quit", HM_NAV_QUIT, -1, NULL, qfalse},
    },
    {
        {"Match Browser", HM_NAV_BROWSER, -1, NULL, qfalse},
        {"Start Match", HM_NAV_PLAY, -1, NULL, qfalse},
        {"Training Center", HM_NAV_TRAINING, -1, NULL, qfalse},
        {"Back", -1, HM_MENU_MAIN, NULL, qtrue},
    },
    {
        {"Career Stats", HM_NAV_STATS, -1, NULL, qfalse},
        {"Recent Matches", HM_NAV_MATCHES, -1, NULL, qfalse},
        {"Replays", HM_NAV_REPLAYS, -1, NULL, qfalse},
        {"Back", -1, HM_MENU_MAIN, NULL, qtrue},
    },
};

static struct {
    qboolean loaded;
    qhandle_t bg, vignette, logo, ranger, idLogo, bethLogo;
    qboolean legalsViewed;  // window.legals_viewed: once a session
    int legalsStart;
    int legalsLeave;        // when it started to fade, 0 while it has not
    int panel;              // 0 friends, 1 lobby
} hr;

static void UI_Home_LoadRetail(void) {
    if (hr.loaded) {
        return;
    }
    hr.loaded = qtrue;
    hr.bg = trap_R_RegisterShaderNoMip("webui/img/bg.jpg");
    hr.vignette = trap_R_RegisterShaderNoMip("ui/ioql/vignette.png");
    hr.logo = trap_R_RegisterShaderNoMip("webui/img/logo.png");
    hr.ranger = trap_R_RegisterShaderNoMip("webui/img/characters/ranger.png");
    hr.idLogo = trap_R_RegisterShaderNoMip("webui/img/id-logo-footer.png");
    hr.bethLogo = trap_R_RegisterShaderNoMip("webui/img/beth-logo-footer.png");

    // [ioquakelive] window.legals_viewed lives as long as retail's web view, the whole session;
    // this module is reloaded on every disconnect and map change, so the engine keeps it
    trap_Cvar_Register(NULL, "ui_legalsViewed", "0", CVAR_TEMP | CVAR_NORESTART);
    hr.legalsViewed = trap_Cvar_VariableValue("ui_legalsViewed") != 0 ||
                      trap_Cvar_VariableValue("web_skipLegals") > 0;  // componentWillMount
}

/*
===============
UI_Home_SkipLegals

[QL] skipLegals: the notice starts to go, and for this session it is seen
===============
*/
static void UI_Home_SkipLegals(int now) {
    hr.legalsLeave = now ? now : 1;
    trap_Cvar_Set("ui_legalsViewed", "1");
}

/*
===============
UI_Home_Leave

[QL] componentWillUnmount: leaving the home page counts as having seen its notice
===============
*/
void UI_Home_Leave(void) {
    if (hr.legalsStart) {
        trap_Cvar_Set("ui_legalsViewed", "1");
    }
}

/*
===============
UI_Home_RetailBackdrop

[QL] The #app's background: bg.jpg from the page's corner at its own size, repeated, under the
radial gradient; over a match, body.ingame's rgba(19,68,92,0.7) and the game behind it
===============
*/
void UI_Home_RetailBackdrop(void) {
    static const vec4_t ingame = {19 / 255.0f, 68 / 255.0f, 92 / 255.0f, 0.7f};
    float top = -smTop / smScale;
    float x, y;

    UI_Home_LoadRetail();
    if (UI_Home_InMatch()) {
        SM_Fill(-1, top, smVW + 2, smBottom - top + 1, ingame, NULL);
        return;
    }
    SM_FillScreen(smPage);
    if (hr.bg) {
        for (y = top; y < smBottom; y += 1080.0f) {
            for (x = 0; x < smVW; x += 1920.0f) {
                SM_Pic(x, y, 1920.0f, 1080.0f, hr.bg, NULL);
            }
        }
    }
    if (hr.vignette) {
        SM_Pic(0, top, smVW, smBottom - top, hr.vignette, NULL);
    }
}

static float UI_Home_ColX(void) {
    return smVW * 0.04f;
}

static float UI_Home_ColW(void) {
    return smVW * 0.20f;
}

// where the first entry of the list starts: under the logo and its 15% margins
static float UI_Home_ItemsTop(void) {
    float colW = UI_Home_ColW();

    return -smTop / smScale + colW * 0.15f + colW * 0.9f * (485.0f / 1920.0f) + colW * 0.15f + HM_LOGO_GAP;
}

static void UI_Home_DrawColumn(void) {
    float top = -smTop / smScale;
    float colX = UI_Home_ColX();
    float colW = UI_Home_ColW();
    float right = colX + colW - HM_ITEM_PAD - HM_ITEM_BORDER;
    float y = UI_Home_ItemsTop();
    int i;

    SM_Fill(colX, top, colW, smBottom - top + 1, hmBlack80, NULL);
    if (hr.logo) {
        SM_Pic(colX + colW * 0.05f, top + colW * 0.15f, colW * 0.9f, colW * 0.9f * (485.0f / 1920.0f), hr.logo, NULL);
    }

    for (i = 0; i < HM_MAX_ITEMS; i++) {
        const hmItem_t* item = &hmMenus[hm2.menu][i];
        qboolean over;
        float w;

        if (!item->title) {
            break;
        }
        if (item->back) {
            y += HM_BACK_GAP;
        }
        over = SM_Over(HM_HIT_ITEM, i);
        if (over) {
            SM_Fill(colX, y, colW, HM_ITEM_H, hmBlack30, NULL);
            SM_Fill(colX + colW - HM_ITEM_BORDER, y, HM_ITEM_BORDER, HM_ITEM_H, smGold, NULL);
        }
        w = SM_Advance(FONT_DEFAULT, HM_ITEM_FONT, item->title, -1);
        SM_Text(right - w, SM_Baseline(FONT_DEFAULT, HM_ITEM_FONT, y + HM_ITEM_PAD, HM_ITEM_LINE), FONT_DEFAULT,
                HM_ITEM_FONT, over ? smGold : hmWhite, item->title, -1, qfalse);
        SM_Hit(colX, y, colW, HM_ITEM_H, HM_HIT_ITEM, i, NULL);
        y += HM_ITEM_H;
    }
}

// the model you wear stands in for the Steam avatar
qhandle_t UI_Home_Face(void) {
    char model[MAX_QPATH];
    char* slash;
    qhandle_t avatar = SM_Avatar("");

    if (avatar) {
        return avatar;
    }
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
    return hm2.face;
}

/*
===============
UI_Home_DrawCard

[QL] ProfileCard (css .profile-card): the avatar as tall as the card's inside and the name beside
it, in a black 0.8 box 75px high at 75px from the top, 20% wide at 4% from the right
===============
*/
static void UI_Home_DrawCard(float x, float y, float w) {
    char name[MAX_CVAR_VALUE_STRING];
    float pic = HM_CARD_H - 2.0f * HM_CARD_PAD;
    float tx = x + HM_CARD_PAD;
    qhandle_t face = UI_Home_Face();

    SM_Fill(x, y, w, HM_CARD_H, hmBlack80, NULL);
    if (face) {
        SM_Pic(x + HM_CARD_PAD, y + HM_CARD_PAD, pic, pic, face, NULL);
        tx += pic + HM_CARD_PAD;
    }
    trap_Cvar_VariableStringBuffer("name", name, sizeof(name));
    SM_Text(tx, SM_Baseline(FONT_SANS, HM_TEXT, y + HM_CARD_PAD, 24.0f), FONT_SANS, HM_TEXT, smText,
            name[0] ? name : "Player", SM_Fit(FONT_SANS, HM_TEXT, name, x + w - tx - HM_CARD_PAD), qfalse);
    UI_Stats_CardFor(-1);
    UI_Stats_DrawCardLine(tx, y + HM_CARD_PAD + 24.0f, x + w - tx - HM_CARD_PAD);
}

// [QL] FriendList.sortFriends: playing Quake Live, then another game, then the rest, by name
static int UI_Home_FriendRank(const uiFriend_t* f) {
    return f->sameGame ? 0 : f->playing ? 1 : 2;
}

static qboolean UI_Home_FriendBefore(const uiFriend_t* a, const uiFriend_t* b) {
    int ra = UI_Home_FriendRank(a);
    int rb = UI_Home_FriendRank(b);

    if (ra != rb) {
        return (qboolean)(ra < rb);
    }
    return (qboolean)(Q_stricmp(a->name, b->name) < 0);
}


static void UI_Home_FriendRefresh(void) {
    int total;
    int i;

    if (hvf.updated && uiInfo.uiDC.realTime - hvf.updated < 2000) {
        return;
    }
    hvf.updated = uiInfo.uiDC.realTime ? uiInfo.uiDC.realTime : 1;
    hvf.count = 0;

    total = trap_SteamFriendCount();
    for (i = 0; i < total; i++) {
        uiFriend_t fr;
        int j;

        if (!trap_SteamFriendGet(i, &fr) || fr.state == 0) {
            continue;
        }
        if (hvf.count == HV_MAX_FRIENDS) {
            if (!UI_Home_FriendBefore(&fr, &hvf.list[hvf.count - 1])) {
                continue;
            }
            hvf.count--;
        }
        for (j = hvf.count; j > 0 && UI_Home_FriendBefore(&fr, &hvf.list[j - 1]); j--) {
            hvf.list[j] = hvf.list[j - 1];
            hvf.index[j] = hvf.index[j - 1];
        }
        hvf.list[j] = fr;
        hvf.index[j] = i;
        hvf.count++;
    }
}

// [QL] what they are doing, as FriendList writes it under the name
static const char* UI_Home_FriendDoing(const uiFriend_t* f) {
    if (f->server) {
        return f->status[0] ? f->status : "In a match";
    }
    if (f->sameGame && f->lobby) {
        return "In a lobby";
    }
    if (f->playing) {
        return f->sameGame ? "Playing Quake Live" : "Playing other game";
    }
    switch (f->state) {
        case 2:
            return "Busy";
        case 3:
            return "Away";
        case 4:
            return "Snooze";
        case 5:
            return "Looking to Trade";
        case 6:
            return "Looking to Play";
        default:
            return "Online";
    }
}

/*
===============
UI_Home_DrawFriends

[QL] FriendList (css .friend-list): a section per friend in the colour of what they are doing
===============
*/
static void UI_Home_DrawFriends(float x, float y, float w, float h) {
    static const vec4_t rowOver = {30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f};
    // yellow playing Quake Live, #8FB93B another game, #53A4C4 online; the nickname #6A6A6A
    static const vec4_t inQuake = {1.0f, 1.0f, 0.0f, 1.0f};
    static const vec4_t inOther = {0x8f / 255.0f, 0xb9 / 255.0f, 0x3b / 255.0f, 1.0f};
    static const vec4_t online = {0x53 / 255.0f, 0xa4 / 255.0f, 0xc4 / 255.0f, 1.0f};
    static const vec4_t nickGrey = {0x6a / 255.0f, 0x6a / 255.0f, 0x6a / 255.0f, 1.0f};
    int max = (int)(h / HM_FRIEND_H);
    int i;

    UI_Home_FriendRefresh();
    if (max > hvf.count) {
        max = hvf.count;
    }
    for (i = 0; i < max; i++) {
        const uiFriend_t* f = &hvf.list[i];
        const float* color = f->playing ? (f->sameGame ? inQuake : inOther) : online;
        float ry = y + i * HM_FRIEND_H;
        float tx = x + 5 + 52 + 8;  // padding, the avatar and its border, margin-right
        float room = x + w - 5 - tx;
        qboolean over = SM_Over(HM_HIT_FRIEND, hvf.index[i]);
        int nameLen;
        float nameW;

        if (over) {
            SM_Fill(x, ry, w, HM_FRIEND_H, rowOver, NULL);
        }
        // the avatar, 48 across, in a 2px border of the row's colour
        SM_Fill(x + 5, ry + 5, 52, 52, color, NULL);
        SM_Fill(x + 7, ry + 7, 48, 48, smField, NULL);
        if (SM_Avatar(f->steamId)) {
            SM_Pic(x + 7, ry + 7, 48, 48, SM_Avatar(f->steamId), NULL);
        } else {
            char initial[8];
            int len = 1;

            if ((unsigned char)f->name[0] >= 0xf0) {
                len = 4;
            } else if ((unsigned char)f->name[0] >= 0xe0) {
                len = 3;
            } else if ((unsigned char)f->name[0] >= 0xc0) {
                len = 2;
            }
            Q_strncpyz(initial, f->name[0] ? f->name : "?", len + 1);
            SM_Text(x + 7 + (48 - SM_Advance(FONT_SANS, HM_TEXT, initial, -1)) * 0.5f,
                    SM_Baseline(FONT_SANS, HM_TEXT, ry + 7, 48), FONT_SANS, HM_TEXT, smText, initial, -1, qtrue);
        }

        // the name, with the nickname after it, and what they are doing under it
        nameLen = SM_Fit(FONT_SANS, HM_TEXT, f->name, room);
        nameW = SM_Advance(FONT_SANS, HM_TEXT, f->name, nameLen);
        SM_Text(tx, SM_Baseline(FONT_SANS, HM_TEXT, ry + 5, 20.0f), FONT_SANS, HM_TEXT, color, f->name, nameLen,
                qfalse);
        if (f->nickname[0]) {
            const char* nick = va(" (%s)", f->nickname);
            float nx = tx + nameW;

            SM_Text(nx, SM_Baseline(FONT_SANS, HM_TEXT, ry + 5, 20.0f), FONT_SANS, HM_TEXT, nickGrey, nick,
                    SM_Fit(FONT_SANS, HM_TEXT, nick, x + w - 5 - nx), qfalse);
        }
        SM_Text(tx, SM_Baseline(FONT_SANS, HM_TEXT, ry + 25, 20.0f), FONT_SANS, HM_TEXT, color,
                UI_Home_FriendDoing(f), SM_Fit(FONT_SANS, HM_TEXT, UI_Home_FriendDoing(f), room), qfalse);
        SM_Hit(x, ry, w, HM_FRIEND_H, HM_HIT_FRIEND, hvf.index[i], NULL);
    }
}

static void UI_Home_DrawSide(void) {
    static const char* const tabs[2] = {"Friends", "Lobby"};
    float top = -smTop / smScale;
    float w = smVW * 0.20f;
    float x = smVW - smVW * 0.04f - w;
    float tx = x;
    float listH = (smBottom - top) * 0.5f;
    int i;

    UI_Home_DrawCard(x, top + HM_CARD_TOP, w);

    // .panel-selector: the two links, 10px of padding each and 3px apart, and the space between
    SM_Fill(x, top + HM_SELECTOR_TOP, w, HM_SELECTOR_H, hmBlack80, NULL);
    for (i = 0; i < 2; i++) {
        float tw = SM_Advance(FONT_SANS, HM_TEXT, tabs[i], -1);

        SM_Text(tx + 10, SM_Baseline(FONT_SANS, HM_TEXT, top + HM_SELECTOR_TOP + 2, 20.0f), FONT_SANS, HM_TEXT, smGold,
                tabs[i], -1, qfalse);
        SM_Hit(tx, top + HM_SELECTOR_TOP, tw + 20, HM_SELECTOR_H, HM_HIT_TAB, i, NULL);
        tx += tw + 20 + 3 + SM_Advance(FONT_SANS, HM_TEXT, " ", -1);
    }

    // .friend-list and .lobby: black 0.8, half the page high
    SM_Fill(x, top + HM_LIST_TOP, w, listH, hmBlack80, NULL);
    if (hr.panel == 0) {
        UI_Home_DrawFriends(x, top + HM_LIST_TOP, w, listH);
    } else {
        SM_Centered(x + w * 0.5f, SM_Baseline(FONT_SANS, HM_TEXT, top + HM_LIST_TOP + 44.0f, 24.0f), HM_TEXT, smText,
                    "Not in a lobby.", qfalse);
    }
}

/*
===============
UI_Home_DrawLegals

[QL] #legals: black over everything, the logo half the page wide 20% down, and at 84% the id and
Bethesda logos with the notice in #666 under them. A click skips it, and it goes by itself after
five seconds, fading out in half of one
===============
*/
static void UI_Home_DrawLegals(void) {
    static const char* const notice =
        "QUAKE LIVE\xe2\x84\xa2 \xc2\xa9 2007-2015, Id Software LLC, a ZeniMax Media company. All Rights Reserved.";
    float top = -smTop / smScale;
    float alpha = 1.0f;
    float lw = smVW * 0.5f;
    float body = top + (smBottom - top) * 0.84f;
    vec4_t black = {0, 0, 0, 1};
    vec4_t tint = {1, 1, 1, 1};
    vec4_t grey;
    int now = uiInfo.uiDC.realTime;

    if (hr.legalsViewed) {
        return;
    }
    if (!hr.legalsStart) {
        hr.legalsStart = now ? now : 1;
    }
    if (!hr.legalsLeave && now - hr.legalsStart >= HM_LEGALS_MS) {
        UI_Home_SkipLegals(now);
    }
    if (hr.legalsLeave) {
        float t = (float)(now - hr.legalsLeave) / HM_LEGALS_FADE;

        if (t >= 1.0f) {
            hr.legalsViewed = qtrue;
            return;
        }
        alpha = 0.99f - 0.98f * t * t;  // ease-in
    }

    black[3] = alpha;
    tint[3] = alpha;
    Vector4Copy(smSubtle, grey);
    grey[3] = alpha;

    SM_Fill(-1, top, smVW + 2, smBottom - top + 1, black, NULL);
    if (hr.logo) {
        SM_PicColor((smVW - lw) * 0.5f, top + smVW * 0.2f, lw, lw * (485.0f / 1920.0f), hr.logo, tint, NULL);
    }
    // the two logos sit on one line with 12px each side and 30px over them, the id one 80x80 and
    // Bethesda's 144x46, both on the line's bottom
    if (hr.idLogo) {
        SM_PicColor(smVW * 0.5f - 138.0f + 12.0f, body + 30.0f, 80.0f, 80.0f, hr.idLogo, tint, NULL);
    }
    if (hr.bethLogo) {
        SM_PicColor(smVW * 0.5f - 30.0f + 12.0f, body + 64.0f, 144.0f, 46.0f, hr.bethLogo, tint, NULL);
    }
    SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, HM_TEXT, body + 146.0f, 24.0f), HM_TEXT, grey, notice, qfalse);
    SM_Hit(0, top, smVW, smBottom - top, HM_HIT_SOCIAL, 0, NULL);
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
    float top;

    SM_BeginFrame();
    UI_Home_LoadMenu();
    UI_Home_LoadRetail();
    top = -smTop / smScale;

    UI_Home_RetailBackdrop();
    // .character: position fixed, bottom 0, left 24%, at its own 1350x959, over a match too
    if (hr.ranger) {
        SM_Pic(smVW * 0.24f, smBottom - 959.0f, 1350.0f, 959.0f, hr.ranger, NULL);
    }
    UI_Home_DrawColumn();
    UI_Home_DrawSide();

    // .legal-links: the link to the notices, over black at the bottom right
    {
        static const vec4_t link = {30 / 255.0f, 174 / 255.0f, 219 / 255.0f, 1.0f};
        float lw = SM_Advance(FONT_SANS, 15, "Legal Notices", -1);
        float ly = smBottom - 24;
        float b = SM_Baseline(FONT_SANS, 15, ly + 2, 20);

        SM_Fill(smVW - lw, ly, lw, 24, smPage, NULL);
        SM_Text(smVW - lw, b, FONT_SANS, 15, link, "Legal Notices", -1, qfalse);
        SM_Fill(smVW - lw, b + 2, lw, 1, link, NULL);
        SM_Hit(smVW - lw, ly, lw, 24, HM_HIT_LEGAL, 0, NULL);
    }
    (void)top;

    UI_Home_Banner();
    UI_Home_DrawLegals();
    SM_EndFrame();
}

qboolean UI_Home_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    // the legal notice takes the click that skips it, and nothing else goes through it
    if (!hr.legalsViewed && hr.legalsStart) {
        if (key == K_MOUSE1 && !hr.legalsLeave) {
            UI_Home_SkipLegals(uiInfo.uiDC.realTime);
        }
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
    if (!hit) {
        return qtrue;
    }
    if (hit->type == HM_HIT_FRIEND) {
        int i;

        for (i = 0; i < hvf.count; i++) {
            if (hvf.index[i] == hit->index) {
                trap_SteamFriendAction(hit->index, hvf.list[i].server ? UI_FRIEND_JOIN : UI_FRIEND_PROFILE);
                break;
            }
        }
        return qtrue;
    }
    if (hit->type == HM_HIT_TAB) {
        hr.panel = hit->index;
        return qtrue;
    }
    if (hit->type == HM_HIT_LEGAL) {
        Menus_CloseAll();
        Menus_ActivateByName("legal");
        return qtrue;
    }
    if (hit->type == HM_HIT_ITEM) {
        const hmItem_t* item = &hmMenus[hm2.menu][hit->index];

        if (item->submenu >= 0) {
            hm2.menu = item->submenu;
        } else if (item->nav >= 0) {
            UI_Home_Go(item->nav);
        } else if (item->url) {
            trap_Sys_OpenURL(item->url);
        }
    }
    return qtrue;
}
