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
    HM_HIT_MENU
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

#define HM_FRIEND_H 62.0f

static void UI_Home_LoadRetail(void);
static void UI_Home_FriendRefresh(void);
static void UI_Home_DrawFriends(float x, float y, float w, float h);

#define HF_BAR_H 75.0f
#define HF_ITEM_W 75.0f
#define HF_WIDE_W 250.0f
#define HF_FIRST_X 435.0f
#define HF_NAV_SIZE 24.0f
#define HF_MENU_SIZE (14.0f * 4.0f / 3.0f)
#define HF_MENU_ROW 48.5f
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
    return 430.0f;
}

float UI_Home_ContentW(void) {
    return smVW - 430.0f - 30.0f;
}

static void UI_Home_NavIcon(float x, float top, int glyph, const float* color) {
    float w = SM_IconWidth(HF_NAV_SIZE, glyph);

    SM_Icon(x + (HF_ITEM_W - w) * 0.5f, top + HF_BAR_H * 0.5f + HF_NAV_SIZE * SM_AWESOME_BASE, HF_NAV_SIZE, color,
            glyph);
}

static const float* UI_Home_NavBox(float x, float top, float w, qboolean chosen, qboolean over) {
    if (chosen || over) {
        SM_Fill(x, top, w, HF_BAR_H, hfBlack30, NULL);
    }
    if (chosen) {
        SM_Fill(x, top + HF_BAR_H - 3.0f, w, 3.0f, smGold, NULL);
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

    SM_Fill(0, top, smVW - 3.0f, HF_BAR_H, hfBlack80, NULL);

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
        SM_Text(x + (HF_WIDE_W - SM_Advance(FONT_DEFAULT, HF_NAV_SIZE, upper, -1)) * 0.5f,
                SM_Baseline(FONT_DEFAULT, HF_NAV_SIZE, top, HF_BAR_H), FONT_DEFAULT, HF_NAV_SIZE, color, upper, -1,
                qfalse);
        SM_Hit(x, top, HF_WIDE_W, HF_BAR_H, HM_HIT_TAB, i, NULL);
        x += HF_WIDE_W;
    }

    x = smVW - 3.0f - HF_ITEM_W;
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

        SM_Fill(HF_COL_X, colTop, HF_COL_W, 25.6f, hfBlack75, NULL);
        SM_Text(HF_COL_X + 10.0f, SM_Baseline(FONT_DEFAULT, 16.0f, colTop, 25.6f), FONT_DEFAULT, 16.0f, smGold, "Lobby", -1,
                qfalse);
        SM_Fill(HF_COL_X, colTop + 25.6f, HF_COL_W, lobbyBottom - colTop - 25.6f, hfBlack80, NULL);

        SM_Fill(HF_COL_X + (HF_COL_W - bw) * 0.5f, colTop + 65.0f, bw, 1, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_COL_W - bw) * 0.5f, colTop + 94.0f, bw, 1, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_COL_W - bw) * 0.5f, colTop + 65.0f, 1, 30, hfButtonBorder, NULL);
        SM_Fill(HF_COL_X + (HF_COL_W + bw) * 0.5f - 1, colTop + 65.0f, 1, 30, hfButtonBorder, NULL);
        SM_SpacedText(HF_COL_X + (HF_COL_W - SM_SpacedWidth(FONT_SANS, 11.0f, create, 1.0f) + 1.0f) * 0.5f,
                      SM_Baseline(FONT_SANS, 11.0f, colTop + 65.0f, 30.0f), FONT_SANS, 11.0f, smText, create, 1.0f,
                      qtrue);
        SM_Centered(HF_COL_X + HF_COL_W * 0.5f, SM_Baseline(FONT_SANS, 15.0f, colTop + 105.0f, 24.0f), 15.0f, smText,
                    "Not in a lobby.", qfalse);

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
    SM_Fill(left, y, HM_CONTAINER_W, 54.4f, hfBlack80, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(left + 6.4f, SM_Baseline(FONT_DEFAULT, 32.0f, y + 8.0f, 38.4f), FONT_DEFAULT, 32.0f, smText, title, -1,
            qfalse);
    SM_SetSpacing(0);
    return y + 54.4f + 2.0f;
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
            char url[512];

            trap_Cvar_VariableStringBuffer("cl_updateURL", url, sizeof(url));
            trap_Sys_OpenURL(url);
        } else {
            trap_Cmd_ExecuteText(EXEC_APPEND, "web_hideBrowser\n");
        }
        return qtrue;
    }
    return qfalse;
}

void UI_Home_Banner(void) {
    static const char* const text = "You are currently in-game. Click here to return to your match.";
    float h = 31.5f;
    float y = smBottom - h;
    char version[32];
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
    if (!version[0]) {
        return;
    }
    index = 1;
    SM_Fill(-1, y, smVW + 2, h, smSuccess, NULL);
    SM_Centered(smVW * 0.5f, SM_Baseline(FONT_SANS, 15, y + 3.75f, 24.0f), 15, smText,
                va("ioquakelive %s is available. Click here to download it.", version), qfalse);
    SM_Hit(0, y, smVW, h, HM_HIT_BANNER, index, NULL);
}

#define HM_ITEM_FONT (SM_Narrow() ? 24.0f : 32.0f)
#define HM_ITEM_PAD (HM_ITEM_FONT * 0.5f)
#define HM_ITEM_LINE (HM_ITEM_FONT * 1.6f)
#define HM_ITEM_H (HM_ITEM_LINE + 2.0f * HM_ITEM_PAD)
#define HM_ITEM_BORDER 3.0f
#define HM_BACK_GAP HM_ITEM_FONT
#define HM_LOGO_GAP 4.5f
#define HM_CARD_TOP 75.0f
#define HM_CARD_H 75.0f
#define HM_CARD_PAD 7.5f
#define HM_SELECTOR_TOP 155.0f
#define HM_SELECTOR_H 25.0f
#define HM_LIST_TOP 183.0f
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

static struct {
    qboolean loaded;
    qhandle_t bg, vignette, logo, ranger, idLogo, bethLogo;
    qboolean legalsViewed;
    int legalsStart;
    int legalsLeave;
    int panel;
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

    trap_Cvar_Register(NULL, "ui_legalsViewed", "0", CVAR_TEMP | CVAR_NORESTART);
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
        float tx = x + 5 + 52 + 8;
        float room = x + w - 5 - tx;
        qboolean over = SM_Over(HM_HIT_FRIEND, hvf.index[i]);
        int nameLen;
        float nameW;

        if (over) {
            SM_Fill(x, ry, w, HM_FRIEND_H, rowOver, NULL);
        }
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

    SM_Fill(x, top + HM_SELECTOR_TOP, w, HM_SELECTOR_H, hmBlack80, NULL);
    for (i = 0; i < 2; i++) {
        float tw = SM_Advance(FONT_SANS, HM_TEXT, tabs[i], -1);

        SM_Text(tx + 10, SM_Baseline(FONT_SANS, HM_TEXT, top + HM_SELECTOR_TOP + 2, 20.0f), FONT_SANS, HM_TEXT, smGold,
                tabs[i], -1, qfalse);
        SM_Hit(tx, top + HM_SELECTOR_TOP, tw + 20, HM_SELECTOR_H, HM_HIT_TAB, i, NULL);
        tx += tw + 20 + 3 + SM_Advance(FONT_SANS, HM_TEXT, " ", -1);
    }

    SM_Fill(x, top + HM_LIST_TOP, w, listH, hmBlack80, NULL);
    if (hr.panel == 0) {
        UI_Home_DrawFriends(x, top + HM_LIST_TOP, w, listH);
    } else {
        SM_Centered(x + w * 0.5f, SM_Baseline(FONT_SANS, HM_TEXT, top + HM_LIST_TOP + 44.0f, 24.0f), HM_TEXT, smText,
                    "Not in a lobby.", qfalse);
    }
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

void UI_Home_Paint(void) {
    float top;

    SM_BeginFrame();
    UI_Home_LoadMenu();
    UI_Home_LoadRetail();
    top = -smTop / smScale;

    UI_Home_RetailBackdrop();
    if (hr.ranger) {
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
