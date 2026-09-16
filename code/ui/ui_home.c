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
// ui_home.c -- [QL] the main menu, and the bar every page of the web UI sits under
//
// Retail's front end is the browser: a nav bar 75 CSS pixels tall over rgba(0,0,0,0.8)
// with the items in Handel Gothic uppercase, white, yellow when hovered, and a 3px
// yellow rule under the one you are on (web.pak's css/main.css, .nav and .navItem).
// Under it a 370px column of lobby and friends, and the page in what is left.
//
// None of the lobby applies here -- there is no master server, no friends list and no
// Steam profile to draw -- so the bar keeps the shape and the page takes the whole
// width. What retail reaches through Play and Statistics comes down to the three things
// this build has: start a match, watch a replay, change the settings.
//

#include "ui_web.h"

#define HM_NAV_H 75.0f
#define HM_NAV_PAD 28.0f

#define HM_CARD_W 380.0f
#define HM_CARD_H 190.0f
#define HM_CARD_GAP 28.0f

typedef enum {
    HM_HIT_NONE,
    HM_HIT_NAV,
    HM_HIT_CARD
} hmHitType_t;

typedef enum {
    HM_NAV_PLAY,
    HM_NAV_REPLAYS,
    HM_NAV_SETTINGS,
    HM_NAV_QUIT,
    HM_NUM_NAV
} hmNav_t;

typedef struct {
    const char* title;
    const char* note;
    int nav;  // what the card does, as one of the nav items
} hmCard_t;

static const hmCard_t hmCards[] = {
    {"Start Match", "Pick a game type, an arena and the rules, then play it here", HM_NAV_PLAY},
    {"Replays", "Watch a demo you recorded", HM_NAV_REPLAYS},
    {"Settings", "Everything: input, video, sound, HUD, teams and weapons", HM_NAV_SETTINGS},
};

#define HM_NUM_CARDS ((int)ARRAY_LEN(hmCards))

static struct {
    qboolean loaded;
    qhandle_t backdrop;
    qhandle_t logo;
} hm;

// #f7d100 over black, the two colours the front end is built out of
static const vec4_t hmNavBg = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t hmWash = {0.0f, 0.0f, 0.0f, 0.72f};
static const vec4_t hmCardBg = {36 / 255.0f, 33 / 255.0f, 33 / 255.0f, 0.92f};
static const vec4_t hmCardOver = {60 / 255.0f, 57 / 255.0f, 57 / 255.0f, 0.96f};

/*
===============
UI_Home_Load

pak00's own art: the smoke plate the menu has always used and the Quake Live mark.
===============
*/
static void UI_Home_Load(void) {
    if (hm.loaded) {
        return;
    }
    hm.backdrop = trap_R_RegisterShaderNoMip("ui/assets/backscreen_smoke");
    hm.logo = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo");
    hm.loaded = qtrue;
}

/*
===============
UI_Home_Backdrop

The smoke plate covering the screen whatever its shape, washed down so text over it
stays readable, with the bottom fading to black the way the pages do.
===============
*/
void UI_Home_Backdrop(void) {
    float aspect = 1920.0f / 1080.0f;
    float w = smVW;
    float h = SM_VIEW_H;
    float k;

    UI_Home_Load();
    if (w / h > aspect) {
        h = w / aspect;  // cover: overflow the height rather than letterbox
    } else {
        w = h * aspect;
    }
    SM_Pic((smVW - w) * 0.5f, ((float)SM_VIEW_H - h) * 0.5f, w, h, hm.backdrop, NULL);
    SM_Fill(0, 0, smVW, (float)SM_VIEW_H, hmWash, NULL);
    for (k = 0; k < 16; k++) {
        vec4_t fade = {0.0f, 0.0f, 0.0f, 0.04f};

        SM_Fill(0, (float)SM_VIEW_H - (k + 1) * 14.0f, smVW, 14.0f, fade, NULL);
    }
}

/*
===============
UI_Home_Nav

.nav from web.pak: 75 tall over rgba(0,0,0,0.8), Handel Gothic uppercase, white,
yellow under the cursor, and a 3px yellow rule under the page you are on. The mark
takes the place retail gives the profile card.
===============
*/
void UI_Home_Nav(int active) {
    static const char* const names[HM_NUM_NAV] = {"Play", "Replays", "Settings", "Quit"};
    float w[HM_NUM_NAV];
    float at[HM_NUM_NAV];
    float x = HM_NAV_PAD;
    float right = smVW - HM_NAV_PAD;
    int i;

    UI_Home_Load();
    SM_Fill(0, 0, smVW, HM_NAV_H, hmNavBg, NULL);
    SM_Fill(0, HM_NAV_H, smVW, 1, smGrey, NULL);
    SM_Pic(x, 14, 47 * 2.6f, 47, hm.logo, NULL);
    x += 47 * 2.6f + 44;

    for (i = 0; i < HM_NUM_NAV; i++) {
        w[i] = SM_SpacedWidth(FONT_DEFAULT, 18, names[i], 2) + 44;
    }
    // Play and Replays run from the left; Settings and Quit are floated right, and
    // like retail's gear and power the last one declared ends up furthest out
    at[HM_NAV_PLAY] = x;
    at[HM_NAV_REPLAYS] = x + w[HM_NAV_PLAY];
    at[HM_NAV_QUIT] = right - w[HM_NAV_QUIT];
    at[HM_NAV_SETTINGS] = at[HM_NAV_QUIT] - w[HM_NAV_SETTINGS];

    for (i = 0; i < HM_NUM_NAV; i++) {
        qboolean on = i == active;
        qboolean over = SM_Over(HM_HIT_NAV, i);
        float ix = at[i];

        if (on || over) {
            vec4_t tint = {0.0f, 0.0f, 0.0f, 0.3f};

            SM_Fill(ix, 0, w[i], HM_NAV_H, tint, NULL);
        }
        SM_SpacedText(ix + 22, SM_Baseline(FONT_DEFAULT, 18, 0, HM_NAV_H), FONT_DEFAULT, 18,
                      (on || over) ? smYellow : smText, names[i], 2, qfalse);
        if (on) {
            SM_Fill(ix, HM_NAV_H - 3, w[i], 3, smYellow, NULL);
        }
        SM_Hit(ix, 0, w[i], HM_NAV_H, HM_HIT_NAV, i, NULL);
    }
}

static void UI_Home_Go(int nav) {
    switch (nav) {
        case HM_NAV_PLAY:
            Menus_CloseAll();
            Menus_ActivateByName("startmatch");
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
            Menus_ActivateByName("quit_popmenu");
            break;
        default:
            break;
    }
}

// what the nav bar does, shared by every page that draws it
qboolean UI_Home_NavClick(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);

    if (hit && hit->type == HM_HIT_NAV) {
        UI_Home_Go(hit->index);
        return qtrue;
    }
    return qfalse;
}

static void UI_Home_DrawCard(int i, float x, float y) {
    const hmCard_t* card = &hmCards[i];
    qboolean over = SM_Over(HM_HIT_CARD, i);
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    int lines, k;

    SM_Fill(x, y, HM_CARD_W, HM_CARD_H, over ? hmCardOver : hmCardBg, NULL);
    SM_Fill(x, y, HM_CARD_W, 3, over ? smYellow : smGrey, NULL);
    SM_SpacedText(x + 26, SM_Baseline(FONT_DEFAULT, 26, y + 40, 34), FONT_DEFAULT, 26, over ? smYellow : smText,
                  card->title, 1, qfalse);

    lines = SM_Wrap(FONT_SANS, 15, card->note, HM_CARD_W - 52, starts, lens);
    for (k = 0; k < lines; k++) {
        SM_Text(x + 26, SM_Baseline(FONT_SANS, 15, y + 104 + k * 24, 24), FONT_SANS, 15, smSelectText,
                card->note + starts[k], lens[k], qfalse);
    }
    SM_Hit(x, y, HM_CARD_W, HM_CARD_H, HM_HIT_CARD, i, NULL);
}

void UI_Home_Paint(void) {
    const smHit_t* under;
    float total = HM_NUM_CARDS * HM_CARD_W + (HM_NUM_CARDS - 1) * HM_CARD_GAP;
    float x = (smVW - total) * 0.5f;
    float y = ((float)SM_VIEW_H - HM_CARD_H) * 0.5f + 20;
    char version[MAX_CVAR_VALUE_STRING];
    int i;

    SM_Frame();
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;

    UI_Home_Backdrop();
    UI_Home_Nav(-1);

    for (i = 0; i < HM_NUM_CARDS; i++) {
        UI_Home_DrawCard(i, x + i * (HM_CARD_W + HM_CARD_GAP), y);
    }

    trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
    SM_Text(HM_NAV_PAD, SM_Baseline(FONT_SANS, 13, (float)SM_VIEW_H - 40, 24), FONT_SANS, 13, smSelectText, version, -1,
            qfalse);

    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
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
    if (hit && hit->type == HM_HIT_CARD) {
        UI_Home_Go(hmCards[hit->index].nav);
    }
    return qtrue;
}
