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
// app frame, and the pages it leads to (src/appframe.js, "#app-frame") carry the
// frame.
//
// Here both share one frame: a column the width of quakeclub.com's container,
// centred, with the QUAKE LIVE logo at the top left and the way out at the top
// right. The main menu is a dashboard in it (see THE MAIN MENU below); the pages
// put their header and panels in it. The painting (ui/ioql/menu_background, which
// pak01 carries from assets\pak01, not in git) lies under everything, darkened;
// a build without it falls back to a map's levelshot.
//
// The green banner a match puts at the bottom of every page is here too.
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
    HM_HIT_ACTION
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

static struct {
    qboolean loadedMenu;
    qhandle_t logo;
    qhandle_t face;
    char faceModel[MAX_QPATH];
} hm2;

static void UI_Home_LoadMenu(void) {
    if (hm2.loadedMenu) {
        return;
    }
    hm2.loadedMenu = qtrue;
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

float UI_Home_BlockTop(float blockHeight) {
    float first = HM_NAV_H;
    float bottom = smBottom - 44.0f - (UI_Home_InMatch() ? 38.0f : 0.0f);
    float y = first + (bottom - first - blockHeight) * 0.5f;

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

Retail's #main-menu, as a dashboard in the frame the other pages sit in. Its
options and their order are retail's -- Play, Statistics, Community, Settings,
Quit -- and so are the ones each opens into; what changes is that nothing is a
level down any more: every option is on the screen, one click away.

  - Play is the large tile, a map's picture with Start Match on it, and Training
    Center and Match Browser beside it (retail's Play list).
  - Statistics, Community and Settings are the tiles under it, each with its own
    list: Replays, Career Stats, Recent Matches; Steam Workshop, Steam Community;
    and Controls, Video, Sound and Your Player, which open Settings on that tab.
  - The player and their friends are the column at the right, as retail's app
    frame had them (web.pak src/home.js: ProfileCard and FriendList).
  - Quit is where the other pages have Back, at the right of the top row.

What this build cannot do yet is listed all the same and marked "soon", so
nothing of retail's is missing. The play tile's picture is one of the arenas
whose shot pak00 has, picked once a session, and drifts very slowly.

===============================================================================
*/

#define HM_SIDE_COL_W 300.0f   // the player's column
#define HM_TILE_GAP HM_PANEL_GAP
#define HM_HERO_H 476.0f
#define HM_TILE_H 276.0f
#define HM_TILE_PAD 26.0f
#define HM_ROW_H 34.0f

typedef enum {
    HM_ACT_START_MATCH,
    HM_ACT_TRAINING,
    HM_ACT_MATCH_BROWSER,
    HM_ACT_REPLAYS,
    HM_ACT_CAREER,
    HM_ACT_RECENT,
    HM_ACT_WORKSHOP,
    HM_ACT_STEAM,
    HM_ACT_CONTROLS,
    HM_ACT_VIDEO,
    HM_ACT_SOUND,
    HM_ACT_PLAYER,
    HM_ACT_QC_PROFILE,
    HM_ACT_QC_LADDERS,
    HM_NUM_ACTIONS
} hmAction_t;

typedef struct {
    const char* label;
    qboolean soon;   // listed, but this build has not got it
    int nav;         // the page it opens
    const char* tab; // for Settings, the tab it opens on
} hmActionDef_t;

static const hmActionDef_t hmActions[HM_NUM_ACTIONS] = {
    {"Start Match", qfalse, HM_NAV_PLAY, NULL},
    {"Training Center", qfalse, HM_NAV_TRAINING, NULL},
    {"Match Browser", qtrue, -1, NULL},
    {"Replays", qfalse, HM_NAV_REPLAYS, NULL},
    {"Career Stats", qtrue, -1, NULL},
    {"Recent Matches", qtrue, -1, NULL},
    {"Steam Workshop", qtrue, -1, NULL},
    {"Steam Community", qtrue, -1, NULL},
    {"Controls", qfalse, HM_NAV_SETTINGS, "INPUT"},
    {"Video", qfalse, HM_NAV_SETTINGS, "VIDEO"},
    {"Sound", qfalse, HM_NAV_SETTINGS, "SOUND"},
    {"Your Player", qfalse, HM_NAV_SETTINGS, "TEAM"},
    {"Your Profile", qtrue, -1, NULL},
    {"Ladders", qtrue, -1, NULL},
};

typedef struct {
    const char* title;
    const char* note;
    int actions[4];
    int numActions;
} hmTile_t;

static const hmTile_t hmTiles[4] = {
    {"Statistics", "Your career, your matches, your replays", {HM_ACT_REPLAYS, HM_ACT_CAREER, HM_ACT_RECENT}, 3},
    {"Community", "The Workshop and the Steam community", {HM_ACT_WORKSHOP, HM_ACT_STEAM}, 2},
    {"Settings", "Controls, video, sound and your player", {HM_ACT_CONTROLS, HM_ACT_VIDEO, HM_ACT_SOUND, HM_ACT_PLAYER},
     4},
    // [ioquakelive] not retail's: where quakeclub.com's profiles and ladders will come in
    {"Quake Club", "Profiles, ladders and news from the community", {HM_ACT_QC_PROFILE, HM_ACT_QC_LADDERS}, 2},
};

// the panels sit over the painting, so they are the page colour with a little of it showing
static const float* UI_Home_SidePanel(void) {
    static vec4_t panel;

    VectorCopy(smPanel, panel);
    panel[3] = 0.90f;
    return panel;
}

// a small chevron pointing right, what a row that leads somewhere carries
static void UI_Home_Chevron(float x, float y, const float* color) {
    int k;

    for (k = 0; k < 5; k++) {
        SM_Fill(x + k, y + k, 1.8f, 1.8f, color, NULL);
        SM_Fill(x + k, y + 8.0f - k, 1.8f, 1.8f, color, NULL);
    }
}

/*
===============
UI_Home_Shade

A colour whose alpha runs from a0 to a1 across a rectangle of the page, downwards or rightwards,
eased. Like UI_Home_Wash it is drawn in bands whose edges are whole screen pixels, so two bands
never share a row and no line shows where they meet.
===============
*/
static void UI_Home_Shade(float x, float y, float w, float h, const float* color, float a0, float a1,
                          qboolean across) {
    float x0 = x * smScale, x1 = (x + w) * smScale;
    float y0 = smTop + y * smScale, y1 = smTop + (y + h) * smScale;
    float from = across ? x0 : y0, to = across ? x1 : y1;
    int first = (int)(from + 0.5f), last = (int)(to + 0.5f);
    int p;
    vec4_t c;

    if (to <= from) {
        return;
    }
    VectorCopy(color, c);
    for (p = first; p < last; p += 2) {
        int end = p + 2 < last ? p + 2 : last;
        float t = ((p + end) * 0.5f - from) / (to - from);

        t = t * t * (3.0f - 2.0f * t);
        c[3] = a0 + (a1 - a0) * t;
        trap_R_SetColor(c);
        if (across) {
            trap_R_DrawStretchPic((float)p, (float)(int)(y0 + 0.5f), (float)(end - p),
                                  (float)((int)(y1 + 0.5f) - (int)(y0 + 0.5f)), 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
        } else {
            trap_R_DrawStretchPic((float)(int)(x0 + 0.5f), (float)p, (float)((int)(x1 + 0.5f) - (int)(x0 + 0.5f)),
                                  (float)(end - p), 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
        }
    }
    trap_R_SetColor(NULL);
}

// the play tile's arena: one of the backdrops' shots, picked once a session
static qhandle_t UI_Home_HeroShot(const char** name) {
    static qhandle_t shot;
    static const char* shotName;
    static qboolean picked;

    if (!picked) {
        int count = (int)ARRAY_LEN(hmBackdrops);
        int start = (int)((unsigned int)(trap_Milliseconds() / 7 + 3) % (unsigned int)count);
        int k;

        picked = qtrue;
        for (k = 0; k < count && !shot; k++) {
            const char* map = hmBackdrops[(start + k) % count];
            fileHandle_t f;
            int len = trap_FS_FOpenFile(va("levelshots/%s.jpg", map), &f, FS_READ);

            if (len > 0) {
                trap_FS_FCloseFile(f);
                shot = trap_R_RegisterShaderNoMip(va("levelshots/%s", map));
                shotName = map;
            }
        }
    }
    *name = shotName;
    return shot;
}

// one of a tile's rows: the option, and a chevron, or "soon" when this build has not got it
static void UI_Home_DrawRow(float x, float y, float w, int action) {
    const hmActionDef_t* def = &hmActions[action];
    qboolean over = !def->soon && SM_Over(HM_HIT_ACTION, action);
    float size = 14.0f;

    SM_Label(x, SM_Baseline(SM_FONT_TITLE, size, y, HM_ROW_H), size,
             def->soon ? smSubtle : over ? smGold : smText, def->label, 0.7f);
    if (def->soon) {
        float sw = SM_LabelWidth(9, "soon", 1.4f);

        SM_Label(x + w - sw, SM_Baseline(SM_FONT_TITLE, 9, y + 1, HM_ROW_H), 9, smSubtle, "soon", 1.4f);
        return;
    }
    UI_Home_Chevron(x + w - 8, y + HM_ROW_H * 0.5f - 5, over ? smGold : smSubtle);
    SM_Hit(x - 10, y, w + 20, HM_ROW_H, HM_HIT_ACTION, action, NULL);
}

/*
===============
UI_Home_DrawHero

Play: the arena's picture filling the tile inside its margin, drifting slowly,
darkened towards the bottom left where the words are; the map's name in the
corner; and the three ways to play, Start Match as the page's one solid button.
===============
*/
static void UI_Home_DrawHero(float x, float y, float w, float h) {
    const char* map = NULL;
    qhandle_t shot = UI_Home_HeroShot(&map);
    float ix = x + 12, iy = y + 12, iw = w - 24, ih = h - 24;
    smRect_t clip = SM_R(ix, iy, iw, ih);
    float bx, by, bw;

    SM_Round(x, y, w, h, SM_RADIUS_MD, UI_Home_SidePanel(), NULL);

    if (shot) {
        // cover the frame, and drift: a slow breath in and out of about four percent
        float t = (float)(uiInfo.uiDC.realTime % 40000) / 40000.0f;
        float zoom = 1.04f + 0.04f * (float)sin(t * M_PI * 2.0);
        float pw = iw * zoom, ph = pw * 1080.0f / 1920.0f;

        if (ph < ih * zoom) {
            ph = ih * zoom;
            pw = ph * 1920.0f / 1080.0f;
        }
        SM_Pic(ix + (iw - pw) * 0.5f, iy + (ih - ph) * 0.5f, pw, ph, shot, &clip);
    } else {
        SM_Fill(ix, iy, iw, ih, smRaised, NULL);
    }

    // the words sit on the picture's darker half: it darkens from below and from the left, and a
    // little along the top for the map's name
    UI_Home_Shade(ix, iy + ih * 0.28f, iw, ih * 0.72f, smPage, 0.0f, 0.94f, qfalse);
    UI_Home_Shade(ix, iy, iw * 0.72f, ih, smPage, 0.55f, 0.0f, qtrue);
    UI_Home_Shade(ix, iy, iw, 70.0f, smPage, 0.45f, 0.0f, qfalse);
    if (map) {
        float mw = SM_LabelWidth(11, map, 1.6f);

        SM_Label(ix + iw - 22 - mw, SM_Baseline(SM_FONT_TITLE, 11, iy + 18, 20), 11, smTextSoft, map, 1.6f);
    }

    SM_Label(ix + 30, SM_Baseline(SM_FONT_TITLE, 12, iy + ih - 222, 18), 12, smGold, "Play", 2.0f);
    SM_Label(ix + 28, SM_Baseline(SM_FONT_TITLE, 44, iy + ih - 196, 56), 44, smText, "Into the arena", 0.6f);
    SM_Text(ix + 30, SM_Baseline(FONT_SANS, 16, iy + ih - 134, 24), FONT_SANS, 16, smTextSoft,
            "Start a match with your own rules, or learn the ropes with Crash.",
            SM_Fit(FONT_SANS, 16, "Start a match with your own rules, or learn the ropes with Crash.", iw - 60), qfalse);

    bx = ix + 30;
    by = iy + ih - 30 - 46;
    bw = SM_ButtonWidth(hmActions[HM_ACT_START_MATCH].label, 46) + 24;
    SM_Button(bx, by, bw, 46, hmActions[HM_ACT_START_MATCH].label, SM_BUTTON_PRIMARY,
              SM_Over(HM_HIT_ACTION, HM_ACT_START_MATCH));
    SM_Hit(bx, by, bw, 46, HM_HIT_ACTION, HM_ACT_START_MATCH, NULL);
    bx += bw + 14;

    bw = SM_ButtonWidth(hmActions[HM_ACT_TRAINING].label, 46);
    SM_Button(bx, by, bw, 46, hmActions[HM_ACT_TRAINING].label, SM_BUTTON_SECONDARY,
              SM_Over(HM_HIT_ACTION, HM_ACT_TRAINING));
    SM_Hit(bx, by, bw, 46, HM_HIT_ACTION, HM_ACT_TRAINING, NULL);
    bx += bw + 6;

    // Match Browser, which this build has not got
    {
        const char* label = hmActions[HM_ACT_MATCH_BROWSER].label;
        float lw = SM_LabelWidth(12, label, 1.0f);

        SM_Label(bx + 20, SM_Baseline(SM_FONT_TITLE, 12, by, 46), 12, smSubtle, label, 1.0f);
        SM_Label(bx + 20 + lw + 10, SM_Baseline(SM_FONT_TITLE, 9, by + 1, 44), 9, smSubtle, "soon", 1.4f);
    }
}

// one of the tiles under Play: its title, a line, and its options
static void UI_Home_DrawTile(float x, float y, float w, float h, const hmTile_t* tile) {
    float ry;
    int i;

    SM_Round(x, y, w, h, SM_RADIUS_MD, UI_Home_SidePanel(), NULL);
    SM_Label(x + HM_TILE_PAD, SM_Baseline(SM_FONT_TITLE, 21, y + 22, 30), 21, smText, tile->title, 0.5f);
    {
        int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
        int lines = SM_Wrap(FONT_SANS, 13, tile->note, w - 2 * HM_TILE_PAD, starts, lens);

        for (i = 0; i < lines && i < 2; i++) {
            SM_Text(x + HM_TILE_PAD, SM_Baseline(FONT_SANS, 13, y + 54 + i * 19, 20), FONT_SANS, 13, smMuted,
                    tile->note + starts[i], lens[i], qfalse);
        }
    }

    ry = y + h - HM_TILE_PAD + 6 - tile->numActions * HM_ROW_H;
    for (i = 0; i < tile->numActions; i++) {
        UI_Home_DrawRow(x + HM_TILE_PAD, ry, w - 2 * HM_TILE_PAD, tile->actions[i]);
        ry += HM_ROW_H;
    }
}

/*
===============
UI_Home_DrawPlayer

Retail's ProfileCard: the avatar, the name you play under and a line of what will
fill it. There is no Steam session here yet, so the avatar is the icon of the
model you wear.
===============
*/
static float UI_Home_DrawPlayer(float x, float y, float w) {
    char name[MAX_CVAR_VALUE_STRING];
    char model[MAX_QPATH];
    char* slash;
    float pic = 64.0f;
    float h = 112.0f;

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

    SM_Round(x, y, w, h, SM_RADIUS_MD, UI_Home_SidePanel(), NULL);
    if (hm2.face) {
        SM_Pic(x + 22, y + (h - pic) * 0.5f, pic, pic, hm2.face, NULL);
    }
    SM_Label(x + 22 + pic + 18, SM_Baseline(SM_FONT_TITLE, 18, y + 30, 26), 18, smText, name[0] ? name : "Player",
             0.4f);
    {
        static const char* const line = "Steam and quakeclub.com profiles soon";
        int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
        int lines = SM_Wrap(FONT_SANS, 12, line, w - pic - 62, starts, lens);
        int i;

        for (i = 0; i < lines && i < 2; i++) {
            SM_Text(x + 22 + pic + 18, SM_Baseline(FONT_SANS, 12, y + 56 + i * 17, 18), FONT_SANS, 12, smSubtle,
                    line + starts[i], lens[i], qfalse);
        }
    }
    return h;
}

// retail's FriendList: who is on, what they are doing; nothing yet without Steam
static void UI_Home_DrawFriends(float x, float y, float w, float h) {
    static const char* const rows[3] = {"Your Steam friends", "Who is in a match", "Who is in a lobby"};
    float ry = y + 70.0f;
    float cw = SM_LabelWidth(10, "0 online", 1.4f);
    int i;

    SM_Round(x, y, w, h, SM_RADIUS_MD, UI_Home_SidePanel(), NULL);
    SM_Label(x + 22, SM_Baseline(SM_FONT_TITLE, 13, y + 22, 20), 13, smText, "Friends", 1.4f);
    SM_Label(x + w - 22 - cw, SM_Baseline(SM_FONT_TITLE, 10, y + 24, 18), 10, smSubtle, "0 online", 1.4f);

    for (i = 0; i < 3 && ry + 44 < y + h - 50; i++) {
        SM_Round(x + 22, ry + 6, 32, 32, 16, smRaised, NULL);
        SM_Text(x + 68, SM_Baseline(FONT_SANS, 14, ry + 12, 20), FONT_SANS, 14, smMuted, rows[i], -1, qfalse);
        ry += 54.0f;
    }
    SM_Text(x + 22, SM_Baseline(FONT_SANS, 12, y + h - 40, 20), FONT_SANS, 12, smSubtle, "Once Steam is wired up", -1,
            qfalse);
}

// the version at the left under the dashboard, and the notice retail's splash shows at the right
static void UI_Home_DrawLegal(float left, float y) {
    static const char* const notice =
        "QUAKE LIVE (c) 2007-2015, Id Software LLC, a ZeniMax Media company. All Rights Reserved.";
    char version[MAX_CVAR_VALUE_STRING];
    float w = SM_Advance(FONT_SANS, 11, notice, -1);

    trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
    SM_Text(left, SM_Baseline(FONT_SANS, 11, y, 20), FONT_SANS, 11, smSubtle, version,
            SM_Fit(FONT_SANS, 11, version, HM_CONTAINER_W - w - 40), qfalse);
    SM_Text(left + HM_CONTAINER_W - w, SM_Baseline(FONT_SANS, 11, y, 20), FONT_SANS, 11, smSubtle, notice, -1, qfalse);
}

// the top row, as the other pages have it, with Quit where they have Back
static void UI_Home_DrawTop(float left) {
    float w = SM_LabelWidth(HM_NAV_ITEM_SIZE, "Quit", HM_NAV_ITEM_SPACING);
    float qx = left + HM_CONTAINER_W - w;
    qboolean over = SM_Over(HM_HIT_NAV, HM_NAV_QUIT);

    if (hm2.logo) {
        SM_Pic(left, 26.0f, 196.0f, 49.0f, hm2.logo, NULL);
    }
    SM_Label(qx, SM_Baseline(FONT_SANS, HM_NAV_ITEM_SIZE, 47, 22), HM_NAV_ITEM_SIZE, over ? smGold : smTextSoft, "Quit",
             HM_NAV_ITEM_SPACING);
    SM_Hit(qx - 20.0f, 30.0f, w + 40.0f, 46.0f, HM_HIT_NAV, HM_NAV_QUIT, NULL);
}

void UI_Home_Open(void) {
    menuDef_t* menu = Menus_FindByName("main");
    itemDef_t* item = menu ? Menu_FindItemByName(menu, "home") : NULL;

    UI_Home_LoadMenu();
    // the page takes keys before the mouse moves over it
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Home_Paint(void) {
    float left = UI_Home_Left();
    float mainW = HM_CONTAINER_W - HM_SIDE_COL_W - HM_TILE_GAP;
    float sideX = left + mainW + HM_TILE_GAP;
    float legal = UI_Home_FrameBottom() - 6;
    float room = legal - 14 - HM_NAV_H;
    float heroH = HM_HERO_H, tileH = HM_TILE_H;
    float top, tileW, playerH;
    int i;

    SM_BeginFrame();
    UI_Home_LoadMenu();

    // the painting, darkened enough for the panels, lighter in the middle than a page's
    {
        float t = -smTop / smScale;
        float full = smBottom - t;

        UI_Home_DrawBackground();
        UI_Home_Wash(t, full * 0.5f, smPage, 0.90f, 0.62f);
        UI_Home_Wash(t + full * 0.5f, full * 0.5f, smPage, 0.62f, 0.94f);
    }

    // a short screen gives the tiles less height, keeping their proportions
    if (heroH + HM_TILE_GAP + tileH > room) {
        float k = room / (heroH + HM_TILE_GAP + tileH);

        heroH *= k;
        tileH *= k;
    }
    top = HM_NAV_H + (room - (heroH + HM_TILE_GAP + tileH)) * 0.5f;

    UI_Home_DrawTop(left);
    UI_Home_DrawHero(left, top, mainW, heroH);

    tileW = (mainW - 2 * HM_TILE_GAP) / 3.0f;
    for (i = 0; i < 3; i++) {
        UI_Home_DrawTile(left + i * (tileW + HM_TILE_GAP), top + heroH + HM_TILE_GAP, tileW, tileH, &hmTiles[i]);
    }
    UI_Home_DrawTile(sideX, top + heroH + HM_TILE_GAP, HM_SIDE_COL_W, tileH, &hmTiles[3]);

    playerH = UI_Home_DrawPlayer(sideX, top, HM_SIDE_COL_W);
    UI_Home_DrawFriends(sideX, top + playerH + HM_TILE_GAP, HM_SIDE_COL_W, heroH - playerH - HM_TILE_GAP);

    UI_Home_DrawLegal(left, legal);
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
    if (!hit || hit->type != HM_HIT_ACTION || hit->index < 0 || hit->index >= HM_NUM_ACTIONS) {
        return qtrue;
    }
    {
        const hmActionDef_t* def = &hmActions[hit->index];

        if (def->soon || def->nav < 0) {
            return qtrue;
        }
        if (def->tab) {
            UI_Settings_SetTab(def->tab);
        }
        UI_Home_Go(def->nav);
    }
    return qtrue;
}
