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

#define HM_NAV_ITEM_SIZE 15.0f
#define HM_NAV_ITEM_SPACING 0.6f
#define HM_NAV_ITEM_GAP 26.0f
#define HM_NAV_INSET 39.0f

// the painting's shape
#define HM_PAINTING_ASPECT (3122.0f / 1344.0f)

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
    HM_HIT_MENU    // [ioquakelive] a row of the PLAY dropdown
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
The bands are a few screen pixels high with whole-pixel edges, so two of them meet
without a row drawn twice - in page units that overlap came out as a dark line
across the screen. UI_Home_WashAcross is the same left to right, eased.
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
#define HM_FRIEND_H 42.0f
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

// src/home.js's menu. Career Stats and Recent Matches came off Quake Live's stats servers, which
// are gone, so the Statistics list keeps Replays
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
static qhandle_t UI_Home_Face(void) {
    char model[MAX_QPATH];
    char* slash;

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
}

// CS2's order, which Steam's list has no opinion on: whoever can be joined, then the rest
static int UI_Home_FriendRank(const uiFriend_t* f) {
    if (f->server) {
        return 0;
    }
    if (f->sameGame) {
        return 1;
    }
    if (f->state == 1) {
        return 2;
    }
    return 3;
}

#define HV_MAX_FRIENDS 32

// the list, re-read every so often: asking Steam for it every frame would be hundreds of calls
// for nothing, since a friend does not change state that often
static struct {
    uiFriend_t list[HV_MAX_FRIENDS];
    int index[HV_MAX_FRIENDS];  // their place in Steam's list, which is what actions go by
    int count;
    int updated;
} hvf;

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
        int rank;
        int j;

        if (!trap_SteamFriendGet(i, &fr) || fr.state == 0) {
            continue;
        }
        rank = UI_Home_FriendRank(&fr);
        if (hvf.count == HV_MAX_FRIENDS) {
            if (rank >= UI_Home_FriendRank(&hvf.list[hvf.count - 1])) {
                continue;
            }
            hvf.count--;
        }
        for (j = hvf.count; j > 0 && UI_Home_FriendRank(&hvf.list[j - 1]) > rank; j--) {
            hvf.list[j] = hvf.list[j - 1];
            hvf.index[j] = hvf.index[j - 1];
        }
        hvf.list[j] = fr;
        hvf.index[j] = i;
        hvf.count++;
    }
}

// what they are doing, in Steam's own words
static const char* UI_Home_FriendDoing(const uiFriend_t* f) {
    if (f->server) {
        return "In a match";
    }
    if (f->sameGame) {
        return "In Quake Live";
    }
    if (f->playing) {
        return "In another game";
    }
    switch (f->state) {
        case 2:
            return "Busy";
        case 3:
            return "Away";
        case 4:
            return "Snooze";
        case 5:
            return "Looking to trade";
        case 6:
            return "Looking to play";
        default:
            return "Online";
    }
}

/*
===============
UI_Home_DrawFriends

[QL] FriendList (css .friend-list): a section per friend, padded 5px, a darker row under the
cursor, the avatar with a black border at the left and the name with what they are doing in
grey. Steam hands avatars over as raw pixels the renderer cannot take, so the box carries the
name's initial
===============
*/
static void UI_Home_DrawFriends(float x, float y, float w, float h) {
    static const vec4_t rowOver = {30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f};
    int max = (int)(h / HM_FRIEND_H);
    int i;

    UI_Home_FriendRefresh();
    if (max > hvf.count) {
        max = hvf.count;
    }
    for (i = 0; i < max; i++) {
        const uiFriend_t* f = &hvf.list[i];
        float ry = y + i * HM_FRIEND_H;
        float box = HM_FRIEND_H - 10.0f;
        qboolean over = SM_Over(HM_HIT_FRIEND, hvf.index[i]);
        char initial[8];
        int len = 1;

        if (over) {
            SM_Fill(x, ry, w, HM_FRIEND_H, rowOver, NULL);
        }
        SM_Fill(x + 5, ry + 5, box, box, smPage, NULL);
        SM_Fill(x + 7, ry + 7, box - 4, box - 4, smField, NULL);
        if ((unsigned char)f->name[0] >= 0xf0) {
            len = 4;
        } else if ((unsigned char)f->name[0] >= 0xe0) {
            len = 3;
        } else if ((unsigned char)f->name[0] >= 0xc0) {
            len = 2;
        }
        Q_strncpyz(initial, f->name[0] ? f->name : "?", len + 1);
        SM_Text(x + 5 + (box - SM_Advance(FONT_SANS, HM_TEXT, initial, -1)) * 0.5f,
                SM_Baseline(FONT_SANS, HM_TEXT, ry + 5, box), FONT_SANS, HM_TEXT, smText, initial, -1, qtrue);
        SM_Text(x + 5 + box + 8, SM_Baseline(FONT_SANS, HM_TEXT, ry + 2, 20.0f), FONT_SANS, HM_TEXT, smText, f->name,
                SM_Fit(FONT_SANS, HM_TEXT, f->name, w - box - 23), qfalse);
        SM_Text(x + 5 + box + 8, SM_Baseline(FONT_SANS, 13.0f, ry + 20, 18.0f), FONT_SANS, 13.0f, hmNameGrey,
                UI_Home_FriendDoing(f), -1, qfalse);
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
        hr.legalsLeave = now;
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
    // .character: position fixed, bottom 0, left 24%, at its own 1350x959
    if (hr.ranger && !UI_Home_InMatch()) {
        SM_Pic(smVW * 0.24f, smBottom - 959.0f, 1350.0f, 959.0f, hr.ranger, NULL);
    }
    UI_Home_DrawColumn();
    UI_Home_DrawSide();
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
            hr.legalsLeave = uiInfo.uiDC.realTime ? uiInfo.uiDC.realTime : 1;
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
