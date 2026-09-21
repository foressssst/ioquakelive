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

// the main menu
// [ioquakelive] the column in fixed grid units (U = 8) rather than a percentage of the
// screen: it lands in the same place every time and the rest aligns against it
#define HM_COL_X 96.0f       // 12 U
#define HM_COL_BAND 672.0f   // 84 U: how far its band reaches
#define HM_COL_W 480.0f      // what an entry takes, rule to chevron (60 U)
#define HM_RULE_GAP 26.0f    // the hover rule stands this far left of the text
#define HM_LOGO_Y 128.0f     // 16 U
#define HM_LOGO_W 400.0f     // 50 U
#define HM_LIST_Y 320.0f     // 40 U
#define HM_ITEM_H 78.0f
#define HM_ITEM_SIZE 27.0f

// [ioquakelive] the list: one entry per row, with room to spare between them. A row is the
// same height with and without its description, so the list does not shift as the cursor
// runs down it.
#define HM_ROW_H 64.0f
#define HM_HEADING_H 40.0f
#define HM_ITEM_SPACING 0.7f
#define HM_NOTE_SIZE 14.0f
#define HM_BACK_SIZE 17.0f
#define HM_BACK_H 46.0f

// the player and their friends, down the right
#define HM_SIDE_W 306.0f     // retail's column is 370; a little under it reads calmer
#define HM_SIDE_RIGHT 64.0f
#define HV_SOCIAL_W 80.0f  // [ioquakelive] the main menu's social strip, folded (10 U)

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

Retail's #main-menu, with the friends column moved to the right of the menu where
the painting leaves room. The list is retail's whole: what this build cannot do
yet is listed all the same, marked "soon" and inert.

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
        {"Match Browser", "Find a server to play on", HM_NAV_BROWSER, -1, qfalse},
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
    return HM_COL_X;
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
    // [ioquakelive] The footer is a flat band now that it carries the marks strip: the old
    // gradient faded out exactly where they sit, leaving them half lit over the artwork. The
    // gradient ends above them and what is left below is even.
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

The logo, then the list with its heading. An entry is its title in bold upper case
and a line of what it does; under the cursor the title turns Quake Live yellow and
a 3px rule stands at the left, which is what .nav-column a:hover does with its
border. What this build has not got is dimmed and says so.
===============
*/
static void UI_Home_DrawColumn(void) {
    float x = UI_Home_ColumnX();
    float y = HM_LIST_Y;
    float panelTop, panelH;
    char heading[MAX_CVAR_VALUE_STRING];
    int i, rows = 0;

    if (hm2.logo) {
        SM_Pic(x - 56.0f * (HM_LOGO_W / 512.0f), HM_LOGO_Y, HM_LOGO_W, HM_LOGO_W * 0.25f, hm2.logo, NULL);
    }

    (void)rows;
    (void)panelTop;
    (void)panelH;

    // a submenu's list names itself; the main one does not need to, the player panel on the
    // right already says it
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

        // [ioquakelive] All that marks where you are is a thin bar at the left edge and the
        // title in gold. No plate, no box: a six entry menu over artwork needs nothing more.
        if (over) {
            SM_Fill(x - HM_RULE_GAP, y + 4, 3, HM_ITEM_SIZE + 4, smText, NULL);
        }

        // [ioquakelive] no arrow at the right of the entry under the cursor: the gold of the
        // title says enough
        SM_Label(x, SM_Baseline(FONT_SANS, HM_ITEM_SIZE, y, HM_ITEM_SIZE + 12), HM_ITEM_SIZE,
                 item->soon ? smSubtle : over ? smText : smTextSoft, item->title, HM_ITEM_SPACING);
        if (item->soon) {
            float tw = SM_LabelWidth(HM_ITEM_SIZE, item->title, HM_ITEM_SPACING);

            SM_Label(x + tw + 16, SM_Baseline(FONT_SANS, 11, y + 10, 18), 11, smSubtle, "soon", 1.6f);
        }

        // The description only shows on the entry under the cursor. Six of them lit at once was
        // a lot of text for names that explain themselves, and the space held for it is not
        // noticeable because a row is always the same height.
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
    // [ioquakelive] top right, level with the logo: with the cards below, halfway down it
    // overlapped the last one
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

[ioquakelive] The footer strip: QUAKE III ARENA, ioquake3, QUAKE LIVE and Quake
Club, side by side the way a sponsor footer runs, because they are credits too.
All four are logos turned white with alpha by code/tools/make_menu_marks.py and
tinted to the same grey, so the row reads as one thing.

The sizes do not come from the images: each is drawn at the height that makes it
weigh the same as the others, taken from that script's preview.
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
#define HM_MARK_H 80.0f  // the tallest one: the strip centres on that band

// [ioquakelive] what is left to the left of the social column
static float UI_Home_MainW(void) {
    return smVW - HV_SOCIAL_W;
}

// [ioquakelive] the content axis: what is left between the menu column and the friends strip.
// The server row and the credit logos centre on that, not on the screen, because the column
// takes up the left side.
static float UI_Home_ContentX(void) {
    return HM_COL_BAND;
}

static float UI_Home_ContentCx(void) {
    return UI_Home_ContentX() + (UI_Home_MainW() - UI_Home_ContentX()) * 0.5f;
}

// [ioquakelive] centred on cx, not always in the main area: they now sit over the notice
static void UI_Home_DrawMarks(float cx, float y, float h) {
    static const vec4_t tint = {1.0f, 1.0f, 1.0f, 0.55f};
    static qhandle_t shaders[ARRAY_LEN(hmMarks)];
    static qboolean loaded;
    float scale = (h - 18.0f) / HM_MARK_H;
    float total = 0;
    float x;
    int i;

    if (!loaded) {
        loaded = qtrue;
        for (i = 0; i < (int)ARRAY_LEN(hmMarks); i++) {
            shaders[i] = trap_R_RegisterShaderNoMip(hmMarks[i].shader);
        }
    }
    for (i = 0; i < (int)ARRAY_LEN(hmMarks); i++) {
        total += (hmMarks[i].w + (i ? HM_MARK_GAP : 0)) * scale;
    }
    x = cx - total * 0.5f;

    for (i = 0; i < (int)ARRAY_LEN(hmMarks); i++) {
        float mw = hmMarks[i].w * scale;
        float mh = hmMarks[i].h * scale;

        if (shaders[i]) {
            SM_PicColor(x, y + (h - mh) * 0.5f, mw, mh, shaders[i], tint, NULL);
        }
        x += mw + HM_MARK_GAP * scale;
    }
}

/*
===============================================================================

THE MAIN MENU

[ioquakelive] Minimal and laid out on a grid: the logo on top, the sections and, in
the middle, Quake Live's emblem cropped by the screen edge, which is the play
button; a social strip on the right; the credits below. No solid bars: the backdrop
darkens at top and bottom so what sits over it can be read.

The grid is an equal margin on both sides (HV_EDGE), two strips of the same height
top and bottom (HV_BAR_H), which are also the social strip's profile and Quit rows,
and the centre of the screen as the one axis.

===============================================================================
*/

#define HV_U 8.0f             // the grid's unit: everything is a multiple of it
#define HV_SOCIAL_OPEN_W 360.0f
#define HV_SOCIAL_MS 55.0f    // the transitions' time constant
#define HV_AVATAR 52.0f
#define HV_BAR_H (10.0f * HV_U)   // the bar and the footer are the same height
#define HV_FOOT_H (10.0f * HV_U)
#define HV_GUTTER (40.0f * HV_U)  // the room either side of the bar: logo and Quit
#define HV_TAB_SLOT (17.0f * HV_U)
#define HV_PLAY_SLOT (24.0f * HV_U)
#define HV_TABS_W (4.0f * HV_TAB_SLOT + HV_PLAY_SLOT)
#define HV_BAR_W (HV_TABS_W + 2.0f * HV_GUTTER)
#define HV_BAR_R (3.0f * HV_U)    // the bar's bottom corners
#define HV_PAD (5.0f * HV_U)      // the room inside the bar, and the footer's margins
#define HV_EDGE (5.0f * HV_U)
#define HV_LOGO_H (7.0f * HV_U)
#define HV_TAB_SIZE 17.0f
#define HV_TAB_SPACING 2.2f
#define HV_PLAY_TOP_W (23.0f * HV_U)  // the trapezium: almost all of its 24 U cell
#define HV_PLAY_BOT_W (20.0f * HV_U)
#define HV_PLAY_DROP (1.0f * HV_U)
#define HV_PLAY_SIZE 17.0f
#define HV_PLAY_SPACING 4.0f
#define HV_MARKS_H (9.0f * HV_U)  // the logo strip, above the notice
#define HV_TEXT 14.0f         // the small type: legal notice and the strip's text

// [ioquakelive] the grey of the pieces is smPanel, the palette's own: the front page has none
// of its own. A veil goes over the artwork, so the backdrop stays dark and what is on top
// reads.
static const vec4_t hvVeil = {0.0f, 0.0f, 0.0f, 0.42f};

typedef struct {
    const char* title;
    int nav;  // -1: the page you are already on
} hvTab_t;

// either side of the emblem, two and two
static const hvTab_t hvTabsLeft[] = {{"Home", -1}, {"Training", HM_NAV_TRAINING}};
static const hvTab_t hvTabsRight[] = {{"Replays", HM_NAV_REPLAYS}, {"Settings", HM_NAV_SETTINGS}};

static struct {
    float social;  // 0 plegada, 1 desplegada
    float play;    // 0 quieto, 1 bajo el cursor
    int lastTime;
} hv;

static float UI_Home_Top(void) {
    return -smTop / smScale;
}

// the page's bottom edge: it rises when the match-in-progress notice is up
static float UI_Home_Bottom(void) {
    return smBottom - (UI_Home_InMatch() ? 38.0f : 0.0f);
}

// eases a value towards its target on an exponential curve, the same at any framerate
static float UI_Home_Ease(float value, float target, float dt) {
    return value + (target - value) * (1.0f - (float)exp(-dt / HV_SOCIAL_MS));
}

static float UI_Home_Smooth(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// the baseline of text centred in an HV_BAR_H tall strip starting at y
static float UI_Home_BarBaseline(float size, float y) {
    return SM_Baseline(FONT_SANS, size, y + (HV_BAR_H - size * 1.4f) * 0.5f, size * 1.4f);
}

// the flat band behind the column, so its text reads over the artwork
static void UI_Home_DrawColumnBand(void) {
    vec4_t band;

    VectorCopy(smPanel, band);
    band[3] = 0.88f;
    SM_Fill(-1, UI_Home_Top(), HM_COL_BAND, UI_Home_Bottom() - UI_Home_Top() + 1, band, NULL);
}

// right at the bottom, over the artwork and with no container: the version on the left, the
// logos in the middle and the legal notice on the right, all three on one line. The logos used
// to sit in a band of their own further up, which is what did not work.
static void UI_Home_DrawFooter(void) {
    static const char* const notice =
        "QUAKE LIVE (c) 2007-2015, Id Software LLC, a ZeniMax Media company. All Rights Reserved.";
    float bottom = UI_Home_Bottom();
    float y = bottom - HV_FOOT_H;
    float baseline = SM_Baseline(FONT_SANS, HV_TEXT, y + (HV_FOOT_H - HV_TEXT * 1.4f) * 0.5f, HV_TEXT * 1.4f);
    float w = SM_Advance(FONT_SANS, HV_TEXT, notice, -1);

    // the logos sit right above the notice and centred on it, so the footer is one block on the
    // right instead of three loose things
    SM_Text(UI_Home_MainW() - HV_EDGE - w, baseline, FONT_SANS, HV_TEXT, smSubtle, notice, -1, qfalse);
    UI_Home_DrawMarks(UI_Home_MainW() - HV_EDGE - w * 0.5f, y - HV_MARKS_H, HV_MARKS_H);
}

/*
===============================================================================

THE SOCIAL STRIP

[ioquakelive] The friends list, modelled on CS2's: folded, a thin column on the
right with your own avatar and the friends' slots; with the cursor over it, it
unfolds leftwards over the page, which darkens, and the names appear.

The friends are Steam's. In retail this list was a web page, so there is nothing to
mirror: the data comes from the Steam API and the shape is ours. Each one shows an
initial, a dot with their state and, open, their name and what they are doing,
sorted the way CS2 sorts them. A click joins the friend's server, or opens their
Steam profile when they are on none.

The avatars are missing: Steam hands them over as raw pixels and the renderer
cannot take an image built in memory, so each carries its initial instead.

===============================================================================
*/
#define HV_MAX_FRIENDS 32
#define HV_FRIEND_ROW (HV_AVATAR + 14.0f)
#define HV_FRIEND_HEAD 34.0f  // the height of the "Friends" title, above the first row

// the list, re-read every so often: asking Steam for it every frame would be hundreds of
// calls for nothing, since a friend does not change state that often
static struct {
    uiFriend_t list[HV_MAX_FRIENDS];
    int index[HV_MAX_FRIENDS];  // their place in Steam's list, which is what actions go by
    int count;                  // how many made it into the list
    int online;                 // how many are online in total
    int updated;
} hvf;

// CS2's order: first whoever can be joined, last whoever is not looking
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

static void UI_Home_FriendRefresh(void) {
    int total;
    int i;

    if (hvf.updated && uiInfo.uiDC.realTime - hvf.updated < 2000) {
        return;
    }
    hvf.updated = uiInfo.uiDC.realTime ? uiInfo.uiDC.realTime : 1;
    hvf.count = 0;
    hvf.online = 0;

    total = trap_SteamFriendCount();
    for (i = 0; i < total; i++) {
        uiFriend_t fr;
        int rank;
        int j;

        if (!trap_SteamFriendGet(i, &fr) || fr.state == 0) {
            continue;  // offline friends are not shown, same as on the console
        }
        hvf.online++;

        rank = UI_Home_FriendRank(&fr);
        if (hvf.count == HV_MAX_FRIENDS) {
            // full: it only gets in by displacing one that matters less
            if (rank >= UI_Home_FriendRank(&hvf.list[hvf.count - 1])) {
                continue;
            }
            hvf.count--;
        }

        // sorted insert: the list is short, so it needs no sorting afterwards
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

static const float* UI_Home_FriendDot(const uiFriend_t* f) {
    if (f->sameGame) {
        return smSuccess;
    }
    if (f->playing) {
        return smGold;
    }
    if (f->state == 1) {
        return smText;
    }
    return smMuted;
}

/*
===============
UI_Home_FriendInitial

The name's initial, which is what goes in the box where the avatar would be. A Steam
name can start with a letter of more than one byte, so the whole character is copied
and not just its first byte, which would draw garbage.
===============
*/
static void UI_Home_FriendInitial(const char* name, char* out, int size) {
    unsigned char c = (unsigned char)name[0];
    int len = 1;

    if (!c) {
        Q_strncpyz(out, "?", size);
        return;
    }
    if (c >= 0xf0) {
        len = 4;
    } else if (c >= 0xe0) {
        len = 3;
    } else if (c >= 0xc0) {
        len = 2;
    }
    if (len >= size) {
        len = size - 1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
    if (out[0] >= 'a' && out[0] <= 'z') {
        out[0] -= 'a' - 'A';
    }
}

/*
===============
UI_Home_DrawFriendRows

One row per friend: the box with their initial and the dot with their state, which is
all that shows while the strip is folded, and on opening it their name, what they are
doing and what a click would do.
===============
*/
static void UI_Home_DrawFriendRows(float x, float y, float w, float icon, float textX, float open, float bottom) {
    int max = (int)((bottom - 20.0f - y) / HV_FRIEND_ROW);
    int i;

    if (max > hvf.count) {
        max = hvf.count;
    }
    for (i = 0; i < max; i++) {
        const uiFriend_t* f = &hvf.list[i];
        float ry = y + i * HV_FRIEND_ROW;
        qboolean over = SM_Over(HM_HIT_FRIEND, hvf.index[i]);
        char initial[8];
        vec4_t c;

        SM_Hit(x, ry - 7.0f, w + 1, HV_FRIEND_ROW, HM_HIT_FRIEND, hvf.index[i], NULL);
        if (over && open > 0.05f) {
            VectorCopy(smRaised, c);
            c[3] = 0.9f * open;
            SM_Fill(x + 1, ry - 7.0f, w, HV_FRIEND_ROW, c, NULL);
        }

        // the box with the initial, where the avatar would go
        SM_Round(icon, ry, HV_AVATAR, HV_AVATAR, SM_RADIUS_SM, over ? smRaisedHover : smRaised, NULL);
        UI_Home_FriendInitial(f->name, initial, sizeof(initial));
        SM_Centered(icon + HV_AVATAR * 0.5f, SM_Baseline(FONT_SANS, 22.0f, ry + 13.0f, 26.0f), 22.0f,
                    f->sameGame ? smText : smMuted, initial, qfalse);

        // the state dot, the same one the player carries above
        SM_Fill(icon + HV_AVATAR - 11, ry + HV_AVATAR - 11, 11, 11, smPage, NULL);
        SM_Fill(icon + HV_AVATAR - 9, ry + HV_AVATAR - 9, 9, 9, UI_Home_FriendDot(f), NULL);

        if (open <= 0.05f) {
            continue;
        }

        VectorCopy(f->sameGame ? smText : smTextSoft, c);
        c[3] = open;
        SM_Text(textX, SM_Baseline(FONT_SANS, 15.0f, ry + 4.0f, 20.0f), FONT_SANS, 15.0f, c, f->name,
                SM_Fit(FONT_SANS, 15.0f, f->name, w - HV_SOCIAL_W - 88.0f), qfalse);
        VectorCopy(f->server ? smSuccess : smSubtle, c);
        c[3] = open;
        SM_Text(textX, SM_Baseline(FONT_SANS, HV_TEXT, ry + 26.0f, 20.0f), FONT_SANS, HV_TEXT, c,
                UI_Home_FriendDoing(f), -1, qfalse);

        // what the click does, shown only under the cursor: the column reads clean and the
        // action appears where it is about to be used
        if (over) {
            const char* label = f->server ? "JOIN" : "PROFILE";
            float qw = SM_LabelWidth(11.0f, label, 1.6f);

            VectorCopy(f->server ? smGold : smMuted, c);
            c[3] = open;
            SM_Label(x + w - 16.0f - qw, SM_Baseline(FONT_SANS, 11.0f, ry + 20.0f, 14.0f), 11.0f, c, label, 1.6f);
        }
    }

    // the ones that did not fit in the column
    if (max < hvf.count && open > 0.05f) {
        vec4_t c;

        VectorCopy(smSubtle, c);
        c[3] = open;
        SM_Text(textX, SM_Baseline(FONT_SANS, HV_TEXT, y + max * HV_FRIEND_ROW, 20.0f), FONT_SANS, HV_TEXT, c,
                va("+%i more", hvf.count - max), -1, qfalse);
    }
}

/*
===============
UI_Home_DrawSocial

[ioquakelive] The social strip, like CS2's: folded it is a thin column with your avatar on top,
the friends' slots and the exit below; when the cursor enters it unfolds leftwards over the
page, which darkens, and the text appears. The avatars travel with the edge; the text fades in
behind them.
===============
*/
static void UI_Home_DrawSocial(float dt) {
    char name[MAX_CVAR_VALUE_STRING];
    char model[MAX_QPATH];
    char* slash;
    float top = UI_Home_Top();
    float bottom = UI_Home_Bottom();
    float open;
    float w;
    float x;
    float y;
    float icon;
    float textX;
    qboolean inside;
    vec4_t c;

    // it stays open while the cursor is over the strip, at whatever width it has at that moment
    w = HV_SOCIAL_W + (HV_SOCIAL_OPEN_W - HV_SOCIAL_W) * UI_Home_Smooth(hv.social);
    inside = smMX >= smVW - w && smMY >= top && smMY <= bottom;
    hv.social = UI_Home_Ease(hv.social, inside ? 1.0f : 0.0f, dt);
    open = UI_Home_Smooth(hv.social);
    w = HV_SOCIAL_W + (HV_SOCIAL_OPEN_W - HV_SOCIAL_W) * open;
    x = smVW - w;
    icon = x + (HV_SOCIAL_W - HV_AVATAR) * 0.5f;
    textX = x + HV_SOCIAL_W;

    // the page darkens while the strip is open
    if (open > 0.01f) {
        c[0] = c[1] = c[2] = 0.0f;
        c[3] = 0.55f * open;
        SM_Fill(-1, top, x + 1, bottom - top + 1, c, NULL);
    }

    SM_Fill(x, top, w + 1, bottom - top + 1, smPanel, NULL);
    SM_Fill(x, top, 1, bottom - top, smBorder, NULL);
    SM_Hit(x, top, w + 1, bottom - top + 1, HM_HIT_SOCIAL, 0, NULL);

    // at the top, on the sections' line: your avatar and, once open, your name
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
    y = top + (HV_BAR_H - HV_AVATAR) * 0.5f;
    if (hm2.face) {
        SM_Pic(icon, y, HV_AVATAR, HV_AVATAR, hm2.face, NULL);
    }
    SM_Fill(icon + HV_AVATAR - 11, y + HV_AVATAR - 11, 11, 11, smPage, NULL);
    SM_Fill(icon + HV_AVATAR - 9, y + HV_AVATAR - 9, 9, 9, smText, NULL);  // en linea
    if (open > 0.05f) {
        VectorCopy(smText, c);
        c[3] = open;
        SM_Label(textX, SM_Baseline(FONT_SANS, 17, y + 3, 24), 17, c, name[0] ? name : "Player", 0.4f);
        VectorCopy(smTextSoft, c);
        c[3] = open;
        SM_Text(textX, SM_Baseline(FONT_SANS, HV_TEXT, y + 29, 20), FONT_SANS, HV_TEXT, c, "Online", -1, qfalse);
    }

    // the friends, as Steam gives them
    y = top + HV_BAR_H + 12.0f;
    UI_Home_FriendRefresh();

    // the header: how many are online, to the right of the title
    if (open > 0.05f) {
        VectorCopy(smMuted, c);
        c[3] = open;
        SM_Label(textX, SM_Baseline(FONT_SANS, HV_TEXT, y, 20), HV_TEXT, c, "Friends", 2.0f);
        if (hvf.online > 0) {
            char text[32];

            Com_sprintf(text, sizeof(text), "%i online", hvf.online);
            VectorCopy(smSubtle, c);
            c[3] = open;
            SM_Text(x + w - 16.0f - SM_Advance(FONT_SANS, HV_TEXT, text, -1),
                    SM_Baseline(FONT_SANS, HV_TEXT, y, 20), FONT_SANS, HV_TEXT, c, text, -1, qfalse);
        }
    }

    y += HV_FRIEND_HEAD;
    if (hvf.count > 0) {
        UI_Home_DrawFriendRows(x, y, w, icon, textX, open, bottom);
    } else if (open > 0.05f) {
        VectorCopy(smSubtle, c);
        c[3] = open;
        SM_Text(textX, SM_Baseline(FONT_SANS, HV_TEXT, y + 2, 20), FONT_SANS, HV_TEXT, c,
                hvf.online ? "Nobody online right now" : "No Steam friends here", -1, qfalse);
        SM_Text(textX, SM_Baseline(FONT_SANS, HV_TEXT, y + 22, 20), FONT_SANS, HV_TEXT, c,
                trap_SteamFriendCount() ? "" : "Steam is not running", -1, qfalse);
    }
}

/*
===============
UI_Home_FriendClick

A click on a friend: if they are on a Quake Live server it joins there, and if not it
opens their Steam profile, which is all that can be done with them.
===============
*/
static void UI_Home_FriendClick(int index) {
    int i;

    for (i = 0; i < hvf.count; i++) {
        if (hvf.index[i] == index) {
            trap_SteamFriendAction(index, hvf.list[i].server ? UI_FRIEND_JOIN : UI_FRIEND_PROFILE);
            return;
        }
    }
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

    // the frame's time, for the animations; clamped so that a stall does not make them jump
    {
        int now = uiInfo.uiDC.realTime;
        float dt = hv.lastTime ? (float)(now - hv.lastTime) : 0.0f;

        hv.lastTime = now;
        dt = dt < 0 ? 0 : dt > 100.0f ? 100.0f : dt;

        UI_Home_DrawBackground();
        SM_Fill(-1, UI_Home_Top(), smVW + 2, smBottom - UI_Home_Top() + 1, hvVeil, NULL);
        UI_Home_DrawColumnBand();
        UI_Home_DrawColumn();
        UI_Home_DrawFooter();
        UI_Home_DrawSocial(dt);
    }

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
    // [ioquakelive] the map row, the bar and the main menu's exit
    if (hit && hit->type == HM_HIT_QUIT) {
        UI_Home_Go(HM_NAV_QUIT);
        return qtrue;
    }
    if (hit && hit->type == HM_HIT_FRIEND) {
        UI_Home_FriendClick(hit->index);
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
