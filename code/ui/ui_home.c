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
#include "ui_web.h"

typedef enum {
    HM_HIT_NONE = SM_HIT_SHARED,
    HM_HIT_NAV,
    HM_HIT_BANNER,
    HM_HIT_ITEM,
    HM_HIT_CARD,
    HM_HIT_QUIT,
    HM_HIT_TAB,
    HM_HIT_SOCIAL,
    HM_HIT_FRIEND,
    HM_HIT_CONTEXT,
    HM_HIT_LEGAL,
    HM_HIT_CONTEXT_BACK,
    HM_HIT_MENU,
    HM_HIT_ERROR,
    HM_HIT_ERROR_OK,
    HM_HIT_NOTES,
    HM_HIT_NOTES_LATER,
    HM_HIT_NOTES_UPDATE,
    HM_HIT_NEWS
} hmHitType_t;

#define HM_MENU_MAIN 0
#define HM_MENU_PLAY 1
#define HM_MENU_STATS 2
#define HM_MENU_COMMUNITY 3
#define HM_NUM_MENUS 4
#define HM_MAX_ITEMS 6

static struct {
    qboolean loadedMenu;
    int menu;
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

void UI_Home_RetailBackdrop(void);

void UI_Home_Backdrop(void) {
    UI_Home_RetailBackdrop();
}

#define HV_MAX_FRIENDS 32

static struct {
    uiFriend_t list[HV_MAX_FRIENDS];
    int index[HV_MAX_FRIENDS];
    int count;
    int updated;
} hvf;

#define HM_FRIEND_PAD SM_Len(5.0f)
#define HM_FRIEND_EDGE SM_LenBorder(2.0f)
#define HM_FRIEND_PIC SM_Len(48.0f)
#define HM_FRIEND_H (2 * HM_FRIEND_PAD + 2 * HM_FRIEND_EDGE + HM_FRIEND_PIC)

static void UI_Home_LoadRetail(void);
static void UI_Home_FriendRefresh(void);
static void UI_Home_DrawFriends(float x, float y, float w, float h);

#define HF_BAR_H SM_Len(75.0f)
#define HF_ITEM_W SM_Len(75.0f)
#define HF_WIDE_W SM_Len(250.0f)
#define HF_FIRST_X (HF_COL_X + HF_COL_W + SM_Len(35.0f))
#define HF_NAV_SIZE 24.0f
#define HF_MENU_SIZE (14.0f * 4.0f / 3.0f)
#define HF_MENU_ROW 48.5f
#define HF_COL_X SM_Len(30.0f)
#define HF_COL_W SM_Len(370.0f)
#define HF_SCROLL_W SM_Len(8.0f)
#define HF_LIST_W (HF_COL_W - HF_SCROLL_W)

static const vec4_t hfBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t hfBlack75 = {0.0f, 0.0f, 0.0f, 0.75f};
static const vec4_t hfBlack30 = {0.0f, 0.0f, 0.0f, 0.3f};
static const vec4_t hfBlack20 = {0.0f, 0.0f, 0.0f, 0.2f};
static const vec4_t hfWhite = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t hfMenuHoverText = {17 / 255.0f, 17 / 255.0f, 17 / 255.0f, 1.0f};
static const vec4_t hfButtonBorder = {77 / 255.0f, 77 / 255.0f, 77 / 255.0f, 1.0f};

typedef struct {
    const char* title;
    int nav;
    const char* url;
} hfMenuItem_t;

static const hfMenuItem_t hfMenus[3][3] = {
    {{"Match Browser", HM_NAV_BROWSER, NULL}, {"Start Match", HM_NAV_PLAY, NULL}, {"Training Center", HM_NAV_TRAINING, NULL}},
    {{"Career Stats", HM_NAV_STATS, NULL}, {"Recent Matches", HM_NAV_MATCHES, NULL}, {"Replays", HM_NAV_REPLAYS, NULL}},
    {{"Steam Workshop", -1, "http://steamcommunity.com/app/282440/workshop"},
     {"Steam Community", -1, "http://steamcommunity.com/app/282440"}},
};
static const char* const hfTabs[3] = {"Play", "Statistics", "Community"};

static int hfOpen = -1;

float UI_Home_Left(void) {
    return SM_Len(430.0f);
}

float UI_Home_ContentW(void) {
    return smVW - SM_Len(430.0f) - SM_Len(30.0f);
}

static void UI_Home_NavIcon(float x, float top, int glyph, const float* color) {
    float w = SM_IconWidth(HF_NAV_SIZE, glyph);

    SM_Icon(x + SM_Center(HF_ITEM_W, w), SM_IconBaseline(HF_NAV_SIZE, top, HF_BAR_H), HF_NAV_SIZE, color, glyph);
}

static const float* UI_Home_NavBox(float x, float top, float w, qboolean chosen, qboolean over) {
    if (chosen || over) {
        SM_Fill(x, top, w, HF_BAR_H, hfBlack30, NULL);
    }
    if (chosen) {
        SM_Fill(x, top + HF_BAR_H - SM_LenBorder(3.0f), w, SM_LenBorder(3.0f), smGold, NULL);
    }
    return (chosen || over) ? smGold : hfWhite;
}

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
    int index;
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

    SM_Fill(0, top, smVW - SM_Len(3.0f), HF_BAR_H, hfBlack80, NULL);

    face = UI_Home_Face();
    x = HF_COL_X;
    if (face) {
        SM_Pic(x, top + 5.0f, 65.0f, 65.0f, face, NULL);
        x += 65.0f + 7.5f;
    }
    trap_Cvar_VariableStringBuffer("name", name, sizeof(name));
    SM_Text(x, SM_Baseline(FONT_SANS, 15.0f, top + 5.0f, 24.0f), FONT_SANS, 15.0f, smText, name[0] ? name : "Player",
            SM_Fit(FONT_SANS, 15.0f, name, HF_COL_X + HF_COL_W - x), qfalse);
    UI_Stats_CardFor(active);
    UI_Stats_DrawCardLine(x, top + 29.0f, HF_COL_X + HF_COL_W - x);

    x = HF_FIRST_X;
    UI_Home_NavIcon(x, top, SM_FA_HOME,
                    UI_Home_NavBox(x, top, HF_ITEM_W, qfalse, SM_Over(HM_HIT_NAV, HM_NAV_HOME)));
    SM_Hit(x, top, HF_ITEM_W, HF_BAR_H, HM_HIT_NAV, HM_NAV_HOME, NULL);
    x += HF_ITEM_W;

    for (i = 0; i < 3; i++) {
        char upper[32];
        const float* color = UI_Home_NavBox(x, top, HF_WIDE_W, tab == i || hfOpen == i, SM_Over(HM_HIT_TAB, i));
        int k;

        for (k = 0; hfTabs[i][k] && k < (int)sizeof(upper) - 1; k++) {
            upper[k] = (char)toupper((unsigned char)hfTabs[i][k]);
        }
        upper[k] = '\0';
        SM_Text(x + SM_Center(HF_WIDE_W, SM_Advance(FONT_DEFAULT, HF_NAV_SIZE, upper, -1)),
                SM_Baseline(FONT_DEFAULT, HF_NAV_SIZE, top, HF_BAR_H), FONT_DEFAULT, HF_NAV_SIZE, color, upper, -1,
                qfalse);
        SM_Hit(x, top, HF_WIDE_W, HF_BAR_H, HM_HIT_TAB, i, NULL);
        x += HF_WIDE_W;
    }

    x = smVW - SM_Len(3.0f) - HF_ITEM_W;
    UI_Home_NavIcon(x, top, SM_FA_POWER, UI_Home_NavBox(x, top, HF_ITEM_W, qfalse, SM_Over(HM_HIT_NAV, HM_NAV_QUIT)));
    SM_Hit(x, top, HF_ITEM_W, HF_BAR_H, HM_HIT_NAV, HM_NAV_QUIT, NULL);
    x -= HF_ITEM_W;
    UI_Home_NavIcon(x, top, SM_FA_GEAR,
                    UI_Home_NavBox(x, top, HF_ITEM_W, active == HM_NAV_SETTINGS, SM_Over(HM_HIT_NAV, HM_NAV_SETTINGS)));
    SM_Hit(x, top, HF_ITEM_W, HF_BAR_H, HM_HIT_NAV, HM_NAV_SETTINGS, NULL);

    {
        float colTop = top + HM_NAV_H;
        float colH = smBottom - colTop;
        float lobbyBottom = colTop + colH * 0.49f;
        float bw = 88.2f;
        const char* create = "CREATE";

        SM_Fill(HF_COL_X, colTop, HF_LIST_W, 25.6f, hfBlack75, NULL);
        SM_Text(HF_COL_X + 10.0f, SM_Baseline(FONT_DEFAULT, 16.0f, colTop, 25.6f), FONT_DEFAULT, 16.0f, smGold, "Lobby", -1,
                qfalse);
        SM_Fill(HF_COL_X, colTop + 25.6f, HF_LIST_W, lobbyBottom - colTop - 25.6f, hfBlack80, NULL);

        SM_Fill(HF_COL_X + (HF_LIST_W - bw) * 0.5f, colTop + 65.0f, bw, 1, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_LIST_W - bw) * 0.5f, colTop + 94.0f, bw, 1, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_LIST_W - bw) * 0.5f, colTop + 65.0f, 1, 30, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_LIST_W + bw) * 0.5f - 1, colTop + 65.0f, 1, 30, hfButtonBorder, NULL);
        SM_SpacedText(HF_COL_X + (HF_LIST_W - SM_SpacedWidth(FONT_SANS, 11.0f, create, 1.0f, qtrue) + 1.0f) * 0.5f,
                      SM_Baseline(FONT_SANS, 11.0f, colTop + 65.0f, 30.0f), FONT_SANS, 11.0f, smText, create, 1.0f,
                      qtrue);
        SM_Centered(HF_COL_X + HF_LIST_W * 0.5f, SM_Baseline(FONT_SANS, 15.0f, colTop + 105.0f, 24.0f), 15.0f, smText,
                    "Not in a lobby.", qfalse);

        UI_Home_FriendRefresh();
        if (hvf.count > 0) {
            float fTop = colTop + colH * 0.51f;
            float fH = smBottom - 30.0f - fTop;
            float rows = hvf.count * HM_FRIEND_H;

            SM_Fill(HF_COL_X, fTop, HF_LIST_W, rows < fH ? rows : fH, hfBlack80, NULL);
            UI_Home_DrawFriends(HF_COL_X, fTop, HF_LIST_W, fH);
        }
    }
}

static void UI_Home_DrawNavMenu(void) {
    float top = -smTop / smScale;
    float x = HF_FIRST_X + HF_ITEM_W + hfOpen * HF_WIDE_W;
    float y = top + HF_BAR_H;
    int i;

    if (hfOpen < 0) {
        return;
    }
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

float UI_Home_Header(float y, const char* eyebrow, const char* title) {
    float left = UI_Home_Left();

    (void)eyebrow;
    SM_Fill(left, y, HM_CONTAINER_W, SM_TITLE_H, hfBlack80, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(left + SM_Len(6.4f), SM_Baseline(FONT_DEFAULT, 32.0f, y + SM_H1_PAD, SM_H1_LINE), FONT_DEFAULT, 32.0f,
            smText, title, -1, qfalse);
    SM_SetSpacing(0);
    return y + SM_TITLE_H + SM_Len(2.0f);
}

float UI_Home_FrameBottom(void) {
    return smBottom - 30.0f;
}

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
            trap_Cmd_ExecuteText(EXEC_APPEND, "quit\n");
            break;
        default:
            break;
    }
}

qboolean UI_Home_InMatch(void) {
    uiClientState_t cs;

    trap_GetClientState(&cs);
    return cs.connState == CA_ACTIVE;
}

qboolean UI_Home_NavClick(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);

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
        if (hit->index == 1) {
            trap_Cmd_ExecuteText(EXEC_APPEND, trap_Cvar_VariableValue("cl_updateAuto") ? "updatedownload\n" : "openupdate\n");
        } else if (hit->index == 2) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "updaterestart\n");
        } else if (hit->index == 3) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "openupdate\n");
        } else {
            trap_Cmd_ExecuteText(EXEC_APPEND, "web_hideBrowser\n");
        }
        return qtrue;
    }
    return qfalse;
}

static void UI_Home_VersionColor(vec4_t color) {
    char hex[16];
    unsigned int rgb = 0xFFCC33;
    int i, n = 0;

    trap_Cvar_VariableStringBuffer("con_colBorder", hex, sizeof(hex));
    for (i = 0; i < 6 && hex[i]; i++) {
        int c = tolower((unsigned char)hex[i]);
        if (c >= '0' && c <= '9') {
            n = n * 16 + (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            n = n * 16 + (c - 'a' + 10);
        } else {
            break;
        }
    }
    if (i == 6) {
        rgb = (unsigned int)n;
    }
    color[0] = ((rgb >> 16) & 0xFF) / 255.0f;
    color[1] = ((rgb >> 8) & 0xFF) / 255.0f;
    color[2] = (rgb & 0xFF) / 255.0f;
    color[3] = 0.45f;
}

void UI_Home_Banner(void) {
    static const char* const text = "You are currently in-game. Click here to return to your match.";
    float h = 31.5f;
    float y = smBottom - h;
    char version[32];
    char status[16];
    vec4_t fill;
    int index = 0;

    UI_Home_DrawNavMenu();
    UI_Home_DrawContext();
    if (UI_Home_InMatch()) {
        SM_Fill(-1, y, smVW + 2, h, smSuccess, NULL);
        SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 15, y + 3.75f, 24.0f), 15, smText, text, qfalse);
        SM_Hit(0, y, smVW, h, HM_HIT_BANNER, index, NULL);
        return;
    }
    trap_Cvar_VariableStringBuffer("cl_updateVersion", version, sizeof(version));
    trap_Cvar_VariableStringBuffer("cl_updateStatus", status, sizeof(status));
    if (!version[0] || !status[0]) {
        return;
    }
    UI_Home_VersionColor(fill);
    if (!Q_stricmp(status, "latest")) {
        SM_Fill(-1, y, smVW + 2, h, fill, NULL);
        SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 18, y + 3.75f, 24.0f), 18, smText,
                    va("You are running the latest version of ioquakelive (%s).", version), qfalse);
        return;
    }
    if (!Q_stricmp(status, "downloading") || !Q_stricmp(status, "installing")) {
        static const vec4_t grey = {0.5f, 0.5f, 0.5f, 0.45f};
        int percent = (int)trap_Cvar_VariableValue("cl_updateProgress");

        percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
        SM_Fill(-1, y, smVW + 2, h, grey, NULL);
        SM_Fill(-1, y, (smVW + 2) * percent / 100.0f, h, fill, NULL);
        SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 18, y + 3.75f, 24.0f), 18, smText,
                    Q_stricmp(status, "installing") ? va("Downloading ioquakelive %s... %i%%", version, percent)
                                                    : va("Installing ioquakelive %s...", version),
                    qfalse);
        return;
    }
    SM_Fill(-1, y, smVW + 2, h, fill, NULL);
    if (!Q_stricmp(status, "ready")) {
        index = 2;
        SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 18, y + 3.75f, 24.0f), 18, smText,
                    va("ioquakelive %s is installed. Click here to restart the game.", version), qfalse);
    } else if (!Q_stricmp(status, "failed")) {
        index = 3;
        SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 18, y + 3.75f, 24.0f), 18, smText,
                    "The update could not be installed. Click here to download it.", qfalse);
    } else {
        index = 1;
        SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 18, y + 3.75f, 24.0f), 18, smText,
                    va(Q_stricmp(status, "prerelease") ? "ioquakelive %s is available. Click here to %s."
                                                       : "ioquakelive %s (experimental) is available. Click here to %s.",
                       version, trap_Cvar_VariableValue("cl_updateAuto") ? "update" : "download it"),
                    qfalse);
    }
    SM_Hit(0, y, smVW, h, HM_HIT_BANNER, index, NULL);
}

#define HM_ITEM_FONT (SM_Narrow() ? 24.0f : 32.0f)
#define HM_ITEM_PAD SM_Len(HM_ITEM_FONT * 0.5f)
#define HM_ITEM_LINE SM_LineHeight(HM_ITEM_FONT, 1.6f)
#define HM_ITEM_H (HM_ITEM_LINE + 2.0f * HM_ITEM_PAD)
#define HM_ITEM_BORDER SM_LenBorder(3.0f)
#define HM_BACK_GAP SM_Len(HM_ITEM_FONT)
#define HM_LOGO_GAP SM_Len(6.0f)
#define HM_CARD_TOP SM_Len(75.0f)
#define HM_CARD_H SM_Len(75.0f)
#define HM_CARD_PAD SM_Len(7.5f)
#define HM_SELECTOR_TOP SM_Len(155.0f)
#define HM_SELECTOR_H SM_Len(25.0f)
#define HM_LIST_TOP SM_Len(183.0f)
#define HM_LEGALS_MS 5000
#define HM_LEGALS_FADE 500
#define HM_TEXT 15.0f

static const vec4_t hmBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t hmBlack30 = {0.0f, 0.0f, 0.0f, 0.3f};
static const vec4_t hmWhite = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t hmNameGrey = {106 / 255.0f, 106 / 255.0f, 106 / 255.0f, 1.0f};

typedef struct {
    const char* title;
    int nav;
    int submenu;
    const char* url;
    qboolean back;
} hmItem_t;

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

#define HM_GRADIENT_SIDE 256
#define HM_GRADIENT_CACHE 16

static struct {
    qboolean loaded;
    qhandle_t bg, gradient, logo, ranger, idLogo, bethLogo;
    char bgColor[MAX_CVAR_VALUE_STRING];
    int numGradients;
    byte gradientColor[HM_GRADIENT_CACHE][4];
    qhandle_t gradients[HM_GRADIENT_CACHE];
    qboolean legalsViewed;
    int legalsStart;
    int legalsLeave;
    int panel;
} hr;

static qboolean UI_Home_ParseBgColor(const char* s, byte* out) {
    float v[4];
    char* end;
    int i;

    for (i = 0; i < 4; i++) {
        while (*s == ' ') {
            s++;
        }
        v[i] = (float)strtod(s, &end);
        if (end == s) {
            return qfalse;
        }
        s = end;
        while (*s == ' ') {
            s++;
        }
        if (i < 3 && *s == '%') {
            v[i] *= 2.55f;
            s++;
            while (*s == ' ') {
                s++;
            }
        }
        if (i < 3) {
            if (*s != ',') {
                return qfalse;
            }
            s++;
        }
    }
    if (*s) {
        return qfalse;
    }
    for (i = 0; i < 3; i++) {
        out[i] = (byte)Com_Clamp(0, 255, floorf(v[i] + 0.5f));
    }
    out[3] = (byte)floorf(Com_Clamp(0, 1, v[3]) * 255.0f + 0.5f);
    return qtrue;
}

static qhandle_t UI_Home_Gradient(const byte* inner) {
    static byte rgba[HM_GRADIENT_SIDE * HM_GRADIENT_SIDE * 4];
    const float outerAlpha = 0.8f;
    float innerAlpha = inner[3] / 255.0f;
    int i, x, y;

    for (i = 0; i < hr.numGradients; i++) {
        if (!memcmp(hr.gradientColor[i], inner, 4)) {
            return hr.gradients[i];
        }
    }
    if (hr.numGradients == HM_GRADIENT_CACHE) {
        return hr.gradient;
    }

    for (y = 0; y < HM_GRADIENT_SIDE; y++) {
        for (x = 0; x < HM_GRADIENT_SIDE; x++) {
            float u = ((x + 0.5f) / HM_GRADIENT_SIDE * 2.0f - 1.0f) / 1.41421356f;
            float v = ((y + 0.5f) / HM_GRADIENT_SIDE * 2.0f - 1.0f) / 1.41421356f;
            float t = sqrtf(u * u + v * v);
            float f = t <= 0.1f ? 0.0f : MIN(1.0f, (t - 0.1f) / 0.9f);
            byte* p = &rgba[(y * HM_GRADIENT_SIDE + x) * 4];

            p[0] = (byte)(inner[0] * (1.0f - f) + 0.5f);
            p[1] = (byte)(inner[1] * (1.0f - f) + 0.5f);
            p[2] = (byte)(inner[2] * (1.0f - f) + 0.5f);
            p[3] = (byte)((innerAlpha + (outerAlpha - innerAlpha) * f) * 255.0f + 0.5f);
        }
    }

    i = hr.numGradients++;
    memcpy(hr.gradientColor[i], inner, 4);
    hr.gradients[i] = trap_R_RegisterShaderFromRaw(va("*uibg_%d_%d_%d_%d", inner[0], inner[1], inner[2], inner[3]), rgba,
                                                   HM_GRADIENT_SIDE, HM_GRADIENT_SIDE);
    return hr.gradients[i];
}

static void UI_Home_UpdateGradient(void) {
    static const byte retail[4] = {0, 102, 153, 51};
    char color[MAX_CVAR_VALUE_STRING];
    byte inner[4];

    trap_Cvar_VariableStringBuffer("ui_bgcolor", color, sizeof(color));
    if (hr.gradient && !strcmp(color, hr.bgColor)) {
        return;
    }
    Q_strncpyz(hr.bgColor, color, sizeof(hr.bgColor));
    if (!color[0]) {
        hr.gradient = UI_Home_Gradient(retail);
    } else if (UI_Home_ParseBgColor(color, inner)) {
        hr.gradient = UI_Home_Gradient(inner);
    } else if (!hr.gradient) {
        hr.gradient = UI_Home_Gradient(retail);
    }
}

static void UI_Home_LoadRetail(void) {
    if (hr.loaded) {
        return;
    }
    hr.loaded = qtrue;
    hr.bg = trap_R_RegisterShaderNoMip("webui/img/bg.jpg");
    hr.logo = trap_R_RegisterShaderNoMip("webui/img/logo.png");
    hr.ranger = trap_R_RegisterShaderNoMip("webui/img/characters/ranger.png");
    hr.idLogo = trap_R_RegisterShaderNoMip("webui/img/id-logo-footer.png");
    hr.bethLogo = trap_R_RegisterShaderNoMip("webui/img/beth-logo-footer.png");

    trap_Cvar_Register(NULL, "ui_legalsViewed", "0", CVAR_TEMP | CVAR_NORESTART);
    trap_Cvar_Register(NULL, "ui_updateNotesSeen", "", CVAR_TEMP | CVAR_NORESTART);
    hr.legalsViewed = trap_Cvar_VariableValue("ui_legalsViewed") != 0 ||
                      trap_Cvar_VariableValue("web_skipLegals") > 0;
}

static void UI_Home_SkipLegals(int now) {
    hr.legalsLeave = now ? now : 1;
    trap_Cvar_Set("ui_legalsViewed", "1");
}

void UI_Home_Leave(void) {
    if (hr.legalsStart) {
        trap_Cvar_Set("ui_legalsViewed", "1");
    }
}

static qhandle_t UI_Home_CustomBackground(void) {
    static char name[MAX_QPATH];
    static qhandle_t pic;

    if (strcmp(name, ui_background.string)) {
        Q_strncpyz(name, ui_background.string, sizeof(name));
        pic = name[0] ? trap_R_RegisterShaderNoMip(name) : 0;
    }
    return pic;
}

void UI_Home_RetailBackdrop(void) {
    static const vec4_t ingame = {19 / 255.0f, 68 / 255.0f, 92 / 255.0f, 0.7f};
    float top = -smTop / smScale;
    float x, y, w, h, crop;
    qhandle_t custom;

    UI_Home_LoadRetail();
    if (UI_Home_InMatch()) {
        SM_Fill(-1, top, smVW + 2, smBottom - top + 1, ingame, NULL);
        return;
    }
    SM_FillScreen(smPage);
    custom = UI_Home_CustomBackground();
    if (custom) {
        w = smVW;
        h = smBottom - top;
        if (w / h > 16.0f / 9.0f) {
            crop = (1.0f - (16.0f / 9.0f) / (w / h)) * 0.5f;
            SM_PicPart(0, top, w, h, 0, crop, 1, 1.0f - crop, custom);
        } else {
            crop = (1.0f - (w / h) / (16.0f / 9.0f)) * 0.5f;
            SM_PicPart(0, top, w, h, crop, 0, 1.0f - crop, 1, custom);
        }
        return;
    }
    if (hr.bg) {
        for (y = top; y < smBottom; y += 1080.0f) {
            for (x = 0; x < smVW; x += 1920.0f) {
                SM_Pic(x, y, 1920.0f, 1080.0f, hr.bg, NULL);
            }
        }
    }
    UI_Home_UpdateGradient();
    if (hr.gradient) {
        SM_Pic(0, top, smVW, smBottom - top, hr.gradient, NULL);
    }
}

static float UI_Home_ColX(void) {
    return SM_Len(smVW * 0.04f);
}

static float UI_Home_ColW(void) {
    return SM_Len(smVW * 0.20f);
}

static float UI_Home_LogoW(void) {
    return SM_Len(UI_Home_ColW() * 0.9f);
}

static float UI_Home_LogoH(void) {
    return SM_Len(UI_Home_LogoW() * (485.0f / 1920.0f));
}

static float UI_Home_ItemsTop(void) {
    float margin = SM_Len(UI_Home_ColW() * 0.15f);

    return -smTop / smScale + margin + UI_Home_LogoH() + margin + HM_LOGO_GAP;
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
        SM_Pic(colX + SM_Len(colW * 0.05f), top + SM_Len(colW * 0.15f), UI_Home_LogoW(), UI_Home_LogoH(), hr.logo,
               NULL);
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

static void UI_Home_DrawFriends(float x, float y, float w, float h) {
    static const vec4_t rowOver = {30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f};
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
        float pic = x + HM_FRIEND_PAD + HM_FRIEND_EDGE;
        float picY = ry + HM_FRIEND_PAD + HM_FRIEND_EDGE;
        float tx = pic + HM_FRIEND_PIC + HM_FRIEND_EDGE + SM_Len(8.0f);
        float room = x + w - HM_FRIEND_PAD - tx;
        qboolean over = SM_Over(HM_HIT_FRIEND, hvf.index[i]);
        int nameLen;
        float nameW;

        if (over) {
            SM_Fill(x, ry, w, HM_FRIEND_H, rowOver, NULL);
        }
        SM_Fill(x + HM_FRIEND_PAD, ry + HM_FRIEND_PAD, HM_FRIEND_PIC + 2 * HM_FRIEND_EDGE,
                HM_FRIEND_PIC + 2 * HM_FRIEND_EDGE, color, NULL);
        SM_Fill(pic, picY, HM_FRIEND_PIC, HM_FRIEND_PIC, smField, NULL);
        if (SM_Avatar(f->steamId)) {
            SM_Pic(pic, picY, HM_FRIEND_PIC, HM_FRIEND_PIC, SM_Avatar(f->steamId), NULL);
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
            SM_Text(pic + (HM_FRIEND_PIC - SM_Advance(FONT_SANS, HM_TEXT, initial, -1)) * 0.5f,
                    SM_Baseline(FONT_SANS, HM_TEXT, picY, HM_FRIEND_PIC), FONT_SANS, HM_TEXT, smText, initial, -1,
                    qtrue);
        }

        nameLen = SM_Fit(FONT_SANS, HM_TEXT, f->name, room);
        nameW = SM_Advance(FONT_SANS, HM_TEXT, f->name, nameLen);
        SM_Text(tx, SM_Baseline(FONT_SANS, HM_TEXT, ry + HM_FRIEND_PAD, SM_LINE_NORMAL), FONT_SANS, HM_TEXT, color,
                f->name, nameLen, qfalse);
        if (f->nickname[0]) {
            const char* nick = va(" (%s)", f->nickname);
            float nx = tx + nameW;

            SM_Text(nx, SM_Baseline(FONT_SANS, HM_TEXT, ry + HM_FRIEND_PAD, SM_LINE_NORMAL), FONT_SANS, HM_TEXT,
                    nickGrey, nick, SM_Fit(FONT_SANS, HM_TEXT, nick, x + w - HM_FRIEND_PAD - nx), qfalse);
        }
        SM_Text(tx,
                SM_Baseline(FONT_SANS, HM_TEXT, ry + HM_FRIEND_PAD + SM_NormalLine(FONT_SANS, HM_TEXT), SM_LINE_NORMAL),
                FONT_SANS, HM_TEXT, color,
                UI_Home_FriendDoing(f), SM_Fit(FONT_SANS, HM_TEXT, UI_Home_FriendDoing(f), room), qfalse);
        SM_Hit(x, ry, w, HM_FRIEND_H, HM_HIT_FRIEND, hvf.index[i], NULL);
    }
}

#define HW_MAX 6

static struct {
    int checked;
    int count;
    char date[HW_MAX][16];
    char title[HW_MAX][256];
    char slug[HW_MAX][128];
    char image[HW_MAX][MAX_QPATH];
    int aspect[HW_MAX];
    qhandle_t pic[HW_MAX];
    qboolean asked[HW_MAX];
    qboolean thumbs;
} hw;

#define HW_THUMB_W SM_Len(85.0f)

static void UI_Home_NewsRefresh(void) {
    static char buf[4096];
    char* p;

    if (hw.count || (hw.checked && uiInfo.uiDC.realTime - hw.checked < 1000)) {
        return;
    }
    hw.checked = uiInfo.uiDC.realTime ? uiInfo.uiDC.realTime : 1;
    if (trap_ClubNews(buf, sizeof(buf)) <= 0) {
        return;
    }
    for (p = buf; *p && hw.count < HW_MAX;) {
        char* date = p;
        char* eol = strchr(p, '\n');
        char* title;
        char* slug;
        char* image;

        if (!eol) {
            break;
        }
        *eol = '\0';
        p = eol + 1;
        title = strchr(date, '\t');
        slug = title ? strchr(title + 1, '\t') : NULL;
        if (!slug || strlen(date) < 10) {
            continue;
        }
        *title++ = '\0';
        *slug++ = '\0';
        image = strchr(slug, '\t');
        if (image) {
            char* aspect = strchr(image + 1, '\t');

            *image++ = '\0';
            if (aspect) {
                *aspect++ = '\0';
                hw.aspect[hw.count] = atoi(aspect);
            }
            if (image[0] && hw.aspect[hw.count] > 0) {
                Q_strncpyz(hw.image[hw.count], image, sizeof(hw.image[0]));
                hw.thumbs = qtrue;
            }
        }
        Com_sprintf(hw.date[hw.count], sizeof(hw.date[0]), "%.2s/%.2s/%.4s", date + 8, date + 5, date);
        Q_strncpyz(hw.title[hw.count], title, sizeof(hw.title[0]));
        Q_strncpyz(hw.slug[hw.count], slug, sizeof(hw.slug[0]));
        hw.count++;
    }
}

static void UI_Home_NewsLine(float x, float base, float room, const float* color, const char* text) {
    char cut[256];
    int n = SM_Fit(FONT_SANS, HM_TEXT, text, room);

    if (text[n]) {
        n = SM_Fit(FONT_SANS, HM_TEXT, text, room - SM_Advance(FONT_SANS, HM_TEXT, "...", -1));
        while (n > 0 && ((unsigned char)text[n] & 0xc0) == 0x80) {
            n--;
        }
        while (n > 0 && text[n - 1] == ' ') {
            n--;
        }
        if (n > (int)sizeof(cut) - 4) {
            n = (int)sizeof(cut) - 4;
        }
        memcpy(cut, text, n);
        memcpy(cut + n, "...", 4);
        text = cut;
    }
    SM_Text(x, base, FONT_SANS, HM_TEXT, color, text, -1, qfalse);
}

static void UI_Home_NewsThumb(int i, float x, float y) {
    float boxW = HW_THUMB_W;
    float boxH = HM_FRIEND_PIC;
    float box = boxW / boxH;
    float img = hw.aspect[i] / 1000.0f;
    float s = 0.0f;
    float t = 0.0f;

    SM_Fill(x, y, boxW, boxH, smField, NULL);
    if (!hw.image[i][0]) {
        return;
    }
    if (!hw.asked[i]) {
        static int lastAsk;

        if (lastAsk == uiInfo.uiDC.realTime) {
            return;
        }
        lastAsk = uiInfo.uiDC.realTime;
        hw.asked[i] = qtrue;
        hw.pic[i] = trap_R_RegisterShaderNoMip(hw.image[i]);
    }
    if (!hw.pic[i]) {
        return;
    }
    if (img > box) {
        s = (1.0f - box / img) * 0.5f;
    } else if (img < box) {
        t = (1.0f - img / box) * 0.5f;
    }
    SM_PicPart(x, y, boxW, boxH, s, t, 1.0f - s, 1.0f - t, hw.pic[i]);
}

static void UI_Home_DrawNews(float x, float y, float w, float bottom) {
    static const vec4_t rowOver = {30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f};
    float line = SM_NormalLine(FONT_SANS, HM_TEXT);
    float rowH;
    float listTop = y + HM_SELECTOR_H + SM_Len(3.0f);
    int rows, i;

    UI_Home_NewsRefresh();
    rowH = hw.thumbs ? HM_FRIEND_H : 2 * HM_FRIEND_PAD + 2 * line;
    rows = (int)((bottom - listTop) / rowH);
    if (rows > hw.count) {
        rows = hw.count;
    }
    if (rows <= 0) {
        return;
    }
    SM_Fill(x, y, w, HM_SELECTOR_H, hmBlack80, NULL);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, HM_TEXT, y + 1.0f / smScale, SM_LINE_NORMAL), FONT_SANS, HM_TEXT, smGold,
            "Quake Club News", -1, qfalse);
    SM_Fill(x, listTop, w, rows * rowH, hmBlack80, NULL);
    for (i = 0; i < rows; i++) {
        float ry = listTop + i * rowH;
        float tx = x + 10;
        float ty = ry + HM_FRIEND_PAD;
        qboolean over = SM_Over(HM_HIT_NEWS, i);

        if (over) {
            SM_Fill(x, ry, w, rowH, rowOver, NULL);
        }
        if (hw.thumbs) {
            float px = x + HM_FRIEND_PAD + HM_FRIEND_EDGE;

            UI_Home_NewsThumb(i, px, ry + HM_FRIEND_PAD + HM_FRIEND_EDGE);
            tx = px + HW_THUMB_W + SM_Len(8.0f);
            ty = ry + (rowH - 2 * line) * 0.5f;
        }
        UI_Home_NewsLine(tx, SM_Baseline(FONT_SANS, HM_TEXT, ty, SM_LINE_NORMAL), x + w - 10 - tx,
                         over ? smGold : smText, hw.title[i]);
        SM_Text(tx, SM_Baseline(FONT_SANS, HM_TEXT, ty + line, SM_LINE_NORMAL), FONT_SANS, HM_TEXT, hmNameGrey,
                hw.date[i], -1, qfalse);
        SM_Hit(x, ry, w, rowH, HM_HIT_NEWS, i, NULL);
    }
}

static void UI_Home_DrawSide(void) {
    static const char* const tabs[2] = {"Friends", "Lobby"};
    float top = -smTop / smScale;
    float w = SM_Len(smVW * 0.20f);
    float x = smVW - SM_Len(smVW * 0.04f) - w;
    float tx = x;
    float listH = SM_Len((smBottom - top) * 0.5f);
    int i;

    UI_Home_DrawCard(x, top + HM_CARD_TOP, w);

    SM_Fill(x, top + HM_SELECTOR_TOP, w, HM_SELECTOR_H, hmBlack80, NULL);
    for (i = 0; i < 2; i++) {
        float tw = SM_Advance(FONT_SANS, HM_TEXT, tabs[i], -1);

        SM_Text(tx + 10, SM_Baseline(FONT_SANS, HM_TEXT, top + HM_SELECTOR_TOP + 1.0f / smScale, SM_LINE_NORMAL),
                FONT_SANS, HM_TEXT, smGold, tabs[i], -1, qfalse);
        SM_Hit(tx, top + HM_SELECTOR_TOP, tw + 20, HM_SELECTOR_H, HM_HIT_TAB, i, NULL);
        tx += tw + 20 + 3 + SM_Advance(FONT_SANS, HM_TEXT, " ", -1);
    }

    SM_Fill(x, top + HM_LIST_TOP, w, listH, hmBlack80, NULL);
    if (hr.panel == 0) {
        UI_Home_DrawFriends(x, top + HM_LIST_TOP, w - HF_SCROLL_W, listH);
    } else {
        SM_Centered(x + w * 0.5f, SM_Baseline(FONT_SANS, HM_TEXT, top + HM_LIST_TOP + 44.0f, 24.0f), HM_TEXT, smText,
                    "Not in a lobby.", qfalse);
    }
    UI_Home_DrawNews(x, top + HM_LIST_TOP + listH + SM_Len(5.0f), w, smBottom - 31.5f - SM_Len(5.0f));
}

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
        alpha = 0.99f - 0.98f * t * t;
    }

    black[3] = alpha;
    tint[3] = alpha;
    Vector4Copy(smSubtle, grey);
    grey[3] = alpha;

    SM_Fill(-1, top, smVW + 2, smBottom - top + 1, black, NULL);
    if (hr.logo) {
        SM_PicColor((smVW - lw) * 0.5f, top + smVW * 0.2f, lw, lw * (485.0f / 1920.0f), hr.logo, tint, NULL);
    }
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
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

static void UI_Home_NotesPaint(void);
static qboolean UI_Home_NotesWanted(void);
static qboolean UI_Home_NotesKey(int key);

void UI_Home_Paint(void) {
    float top;

    SM_BeginFrame();
    UI_Home_LoadMenu();
    UI_Home_LoadRetail();
    top = -smTop / smScale;

    UI_Home_RetailBackdrop();
    if (hr.ranger && !UI_Home_CustomBackground()) {
        SM_Pic(smVW * 0.24f, smBottom - 959.0f, 1350.0f, 959.0f, hr.ranger, NULL);
    }
    UI_Home_DrawColumn();
    UI_Home_DrawSide();

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
    UI_Home_NotesPaint();
    SM_EndFrame();
}

qboolean UI_Home_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (!hr.legalsViewed && hr.legalsStart) {
        if (key == K_MOUSE1 && !hr.legalsLeave) {
            UI_Home_SkipLegals(uiInfo.uiDC.realTime);
        }
        return qtrue;
    }
    if (UI_Home_NotesWanted()) {
        return UI_Home_NotesKey(key);
    }
    if (key == K_ESCAPE && hm2.menu != HM_MENU_MAIN) {
        hm2.menu = HM_MENU_MAIN;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;
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
    if (hit->type == HM_HIT_NEWS) {
        if (hit->index >= 0 && hit->index < hw.count) {
            trap_Sys_OpenURL(va("https://quakeclub.com/noticias/%s", hw.slug[hit->index]));
        }
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

#define HM_ERROR_H 300.0f
#define HM_ERROR_SIDE 0.15f

static const vec4_t hmErrorDialog = SM_RGB(30, 30, 30);
static char hmError[1024];

void UI_Home_ShowError(const char* text) {
    char* c;

    Q_strncpyz(hmError, text, sizeof(hmError));
    for (c = hmError; *c; c++) {
        if (*c == '\n' || *c == '\r' || *c == '\t') {
            *c = ' ';
        }
    }
}

qboolean UI_Home_ErrorActive(void) {
    return (qboolean)(hmError[0] != '\0');
}

void UI_Home_ErrorPaint(void) {
    float top = smBottom * 0.5f - HM_ERROR_H * 0.5f;
    float x = smVW * HM_ERROR_SIDE;
    float w = smVW * (1.0f - 2.0f * HM_ERROR_SIDE);
    float okW = SM_WebButtonWidth("OK");
    float okX = x + w - okW;
    float okY = top + HM_ERROR_H - 25.0f - SM_WEB_BUTTON_H;
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    int lines, i;

    if (!hmError[0]) {
        return;
    }
    SM_Fill(0, 0, smVW, smBottom, hmBlack80, NULL);
    SM_Fill(0, top, smVW, HM_ERROR_H, hmErrorDialog, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_TextRaw(x + 6.4f, SM_Baseline(FONT_DEFAULT, 32, top + 24.0f, 38.4f), FONT_DEFAULT, 32, smText,
               "Quake Live Error", -1, qfalse);
    SM_SetSpacing(0);
    lines = SM_Wrap(FONT_SANS, HM_TEXT, hmError, w, starts, lens);
    for (i = 0; i < lines; i++) {
        SM_TextRaw(x, SM_Baseline(FONT_SANS, HM_TEXT, top + 88.4f + i * 20.0f, 20.0f), FONT_SANS, HM_TEXT, smText,
                   hmError + starts[i], lens[i], qfalse);
    }
    SM_Hit(0, 0, smVW, smBottom, HM_HIT_ERROR, 0, NULL);
    SM_WebButton(okX, okY, okW, "OK", qfalse, SM_Over(HM_HIT_ERROR_OK, 0), NULL);
    SM_Hit(okX, okY, okW, SM_WEB_BUTTON_H, HM_HIT_ERROR_OK, 0, NULL);
    SM_EndFrame();
}

qboolean UI_Home_ErrorKey(int key) {
    const smHit_t* hit;

    if (key != K_MOUSE1) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (hit && hit->type == HM_HIT_ERROR_OK) {
        hmError[0] = '\0';
        trap_Cvar_Set("com_errorMessage", "");
    }
    return qtrue;
}

#define HN_TEXT 8192
#define HN_PARAS 128
#define HN_ROWS 384
#define HN_LINE 20.0f
#define HN_HEAD 18.0f
#define HN_HEAD_LINE 28.0f
#define HN_GAP 10.0f
#define HN_INDENT 18.0f
#define HN_BODY_MAX 0.55f

enum { HN_PLAIN, HN_HEADING, HN_BULLET };

static struct {
    char version[32];
    char text[HN_TEXT];
    int numParas;
    int paraStart[HN_PARAS];
    byte paraKind[HN_PARAS];
    byte paraGap[HN_PARAS];
    float width;
    int numRows;
    int rowStart[HN_ROWS];
    int rowLen[HN_ROWS];
    byte rowKind[HN_ROWS];
    byte rowMark[HN_ROWS];
    float rowY[HN_ROWS];
    float height;
    float scroll;
} hn;

static int UI_Home_NotesInline(const char* in, int len, char* out, int size) {
    int i, n = 0;

    for (i = 0; i < len && n < size - 1; i++) {
        char c = in[i];

        if (c == '`') {
            continue;
        }
        if ((c == '*' || c == '_') && i + 1 < len && in[i + 1] == c) {
            i++;
            continue;
        }
        if (c == '\\' && i + 1 < len && strchr("\\`*_[]()#+-.!", in[i + 1])) {
            out[n++] = in[++i];
            continue;
        }
        if (c == '!' && i + 1 < len && in[i + 1] == '[') {
            int j = i + 2;

            while (j < len && in[j] != ']') {
                j++;
            }
            if (j + 1 < len && in[j + 1] == '(') {
                while (j < len && in[j] != ')') {
                    j++;
                }
                i = j;
                continue;
            }
        }
        if (c == '[') {
            int j = i + 1;

            while (j < len && in[j] != ']') {
                j++;
            }
            if (j + 1 < len && in[j + 1] == '(') {
                int k = j + 2;

                while (k < len && in[k] != ')') {
                    k++;
                }
                if (k < len) {
                    n += UI_Home_NotesInline(in + i + 1, j - i - 1, out + n, size - n);
                    i = k;
                    continue;
                }
            }
        }
        out[n++] = c;
    }
    out[n] = '\0';
    return n;
}

static qboolean UI_Home_NotesRule(const char* s, int len) {
    int i;

    if (len < 3) {
        return qfalse;
    }
    for (i = 0; i < len; i++) {
        if (s[i] != '-' && s[i] != '*' && s[i] != '_' && s[i] != ' ') {
            return qfalse;
        }
    }
    return qtrue;
}

static void UI_Home_NotesLoad(void) {
    static char raw[HN_TEXT];
    const char* p;
    qboolean gap = qfalse;
    int n = 0;

    hn.numParas = 0;
    hn.width = -1.0f;
    hn.scroll = 0.0f;
    if (trap_UpdateNotes(raw, sizeof(raw)) <= 0) {
        return;
    }
    for (p = raw; *p;) {
        const char* eol = strchr(p, '\n');
        const char* s = p;
        int len = eol ? (int)(eol - p) : (int)strlen(p);
        int kind = HN_PLAIN;

        p += len + (eol ? 1 : 0);
        while (len > 0 && *s == ' ') {
            s++;
            len--;
        }
        while (len > 0 && s[len - 1] == ' ') {
            len--;
        }
        if (!len || UI_Home_NotesRule(s, len)) {
            gap = (qboolean)(hn.numParas > 0);
            continue;
        }
        if (*s == '#') {
            while (len > 0 && *s == '#') {
                s++;
                len--;
            }
            while (len > 0 && *s == ' ') {
                s++;
                len--;
            }
            kind = HN_HEADING;
        } else if (len >= 2 && (*s == '-' || *s == '*' || *s == '+') && s[1] == ' ') {
            s += 2;
            len -= 2;
            kind = HN_BULLET;
        } else if (!gap && hn.numParas > 0 && hn.paraKind[hn.numParas - 1] != HN_HEADING && n < HN_TEXT - 2) {
            hn.text[n - 1] = ' ';
            n += UI_Home_NotesInline(s, len, hn.text + n, HN_TEXT - n) + 1;
            continue;
        }
        if (!len || hn.numParas == HN_PARAS || n >= HN_TEXT - 2) {
            continue;
        }
        hn.paraStart[hn.numParas] = n;
        hn.paraKind[hn.numParas] = (byte)kind;
        hn.paraGap[hn.numParas] = (byte)gap;
        hn.numParas++;
        gap = qfalse;
        n += UI_Home_NotesInline(s, len, hn.text + n, HN_TEXT - n) + 1;
    }
}

static void UI_Home_NotesLayout(float width) {
    float y = 0.0f;
    int i, j;

    if (width == hn.width) {
        return;
    }
    hn.width = width;
    hn.numRows = 0;
    for (i = 0; i < hn.numParas; i++) {
        const char* text = hn.text + hn.paraStart[i];
        int kind = hn.paraKind[i];
        float size = kind == HN_HEADING ? HN_HEAD : HM_TEXT;
        float line = kind == HN_HEADING ? HN_HEAD_LINE : HN_LINE;
        int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
        int lines = SM_Wrap(FONT_SANS, size, text, width - (kind == HN_BULLET ? HN_INDENT : 0.0f), starts, lens);

        if (hn.paraGap[i]) {
            y += HN_GAP;
        }
        for (j = 0; j < lines && hn.numRows < HN_ROWS; j++) {
            hn.rowStart[hn.numRows] = hn.paraStart[i] + starts[j];
            hn.rowLen[hn.numRows] = lens[j];
            hn.rowKind[hn.numRows] = (byte)kind;
            hn.rowMark[hn.numRows] = (byte)(kind == HN_BULLET && j == 0);
            hn.rowY[hn.numRows] = y;
            hn.numRows++;
            y += line;
        }
    }
    hn.height = y;
}

static qboolean UI_Home_NotesWanted(void) {
    char status[16];
    char version[32];
    char seen[32];

    if (!hr.legalsViewed || UI_Home_InMatch() || UI_Home_ErrorActive()) {
        return qfalse;
    }
    trap_Cvar_VariableStringBuffer("cl_updateStatus", status, sizeof(status));
    if (Q_stricmp(status, "available") && Q_stricmp(status, "prerelease")) {
        return qfalse;
    }
    trap_Cvar_VariableStringBuffer("cl_updateVersion", version, sizeof(version));
    trap_Cvar_VariableStringBuffer("ui_updateNotesSeen", seen, sizeof(seen));
    if (!version[0] || !Q_stricmp(seen, version)) {
        return qfalse;
    }
    if (strcmp(hn.version, version)) {
        Q_strncpyz(hn.version, version, sizeof(hn.version));
        UI_Home_NotesLoad();
    }
    return (qboolean)(hn.numParas > 0);
}

static void UI_Home_NotesClose(qboolean update) {
    trap_Cvar_Set("ui_updateNotesSeen", hn.version);
    if (update) {
        trap_Cmd_ExecuteText(EXEC_APPEND,
                             trap_Cvar_VariableValue("cl_updateAuto") ? "updatedownload\n" : "openupdate\n");
    }
}

static float UI_Home_NotesBodyH(void) {
    float top = -smTop / smScale;
    float room = (smBottom - top) * HN_BODY_MAX;

    return hn.height < room ? hn.height : room;
}

static void UI_Home_NotesPaint(void) {
    static const char* const later = "Update later";
    static const char* const update = "Update";
    float top = -smTop / smScale;
    float x = smVW * HM_ERROR_SIDE;
    float w = smVW * (1.0f - 2.0f * HM_ERROR_SIDE);
    float bodyH, h, band, bodyTop, by, updW, laterW, updX, laterX;
    char status[16];
    int i;

    if (!UI_Home_NotesWanted()) {
        return;
    }
    UI_Home_NotesLayout(w - SM_Len(16.0f));
    bodyH = UI_Home_NotesBodyH();
    if (hn.scroll > hn.height - bodyH) {
        hn.scroll = hn.height - bodyH;
    }
    if (hn.scroll < 0.0f) {
        hn.scroll = 0.0f;
    }
    h = 88.4f + bodyH + 25.0f + SM_WEB_BUTTON_H + 25.0f;
    band = top + (smBottom - top - h) * 0.5f;
    bodyTop = band + 88.4f;

    SM_Fill(-1, top, smVW + 2, smBottom - top + 1, hmBlack80, NULL);
    SM_Fill(-1, band, smVW + 2, h, hmErrorDialog, NULL);
    SM_Hit(0, top, smVW, smBottom - top, HM_HIT_NOTES, 0, NULL);

    trap_Cvar_VariableStringBuffer("cl_updateStatus", status, sizeof(status));
    SM_SetSpacing(SM_SPACING_H1);
    SM_TextRaw(x + 6.4f, SM_Baseline(FONT_DEFAULT, 32, band + 24.0f, 38.4f), FONT_DEFAULT, 32, smText,
               va(Q_stricmp(status, "prerelease") ? "A new version is out: %s" : "A new version is out: %s (experimental)",
                  hn.version),
               -1, qfalse);
    SM_SetSpacing(0);

    for (i = 0; i < hn.numRows; i++) {
        int kind = hn.rowKind[i];
        float size = kind == HN_HEADING ? HN_HEAD : HM_TEXT;
        float line = kind == HN_HEADING ? HN_HEAD_LINE : HN_LINE;
        float ry = bodyTop + hn.rowY[i] - hn.scroll;
        float base = SM_Baseline(FONT_SANS, size, ry, line);
        float tx = x + (kind == HN_BULLET ? HN_INDENT : 0.0f);

        if (ry < bodyTop - 0.5f || ry + line > bodyTop + bodyH + 0.5f) {
            continue;
        }
        if (hn.rowMark[i]) {
            SM_Fill(x + 5.0f, base - 6.0f, 4.0f, 4.0f, smText, NULL);
        }
        SM_Text(tx, base, FONT_SANS, size, kind == HN_HEADING ? smGold : smText, hn.text + hn.rowStart[i],
                hn.rowLen[i], qfalse);
    }
    if (hn.height > bodyH) {
        float thumb = bodyH * bodyH / hn.height;
        float pos = (bodyH - thumb) * hn.scroll / (hn.height - bodyH);

        SM_Fill(x + w - SM_Len(4.0f), bodyTop, SM_Len(4.0f), bodyH, smWebButton, NULL);
        SM_Fill(x + w - SM_Len(4.0f), bodyTop + pos, SM_Len(4.0f), thumb, smWebButtonHover, NULL);
    }

    by = band + h - 25.0f - SM_WEB_BUTTON_H;
    updW = SM_WebButtonWidth(update);
    laterW = SM_WebButtonWidth(later);
    updX = x + w - updW;
    laterX = updX - SM_Len(10.0f) - laterW;
    SM_WebButton(laterX, by, laterW, later, qfalse, SM_Over(HM_HIT_NOTES_LATER, 0), NULL);
    SM_Hit(laterX, by, laterW, SM_WEB_BUTTON_H, HM_HIT_NOTES_LATER, 0, NULL);
    SM_WebButton(updX, by, updW, update, qfalse, SM_Over(HM_HIT_NOTES_UPDATE, 0), NULL);
    SM_Hit(updX, by, updW, SM_WEB_BUTTON_H, HM_HIT_NOTES_UPDATE, 0, NULL);
}

static qboolean UI_Home_NotesKey(int key) {
    const smHit_t* hit;

    if (key == K_ESCAPE) {
        UI_Home_NotesClose(qfalse);
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        hn.scroll += (key == K_MWHEELUP ? -3.0f : 3.0f) * HN_LINE;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (hit && hit->type == HM_HIT_NOTES_LATER) {
        UI_Home_NotesClose(qfalse);
    } else if (hit && hit->type == HM_HIT_NOTES_UPDATE) {
        UI_Home_NotesClose(qtrue);
    }
    return qtrue;
}
