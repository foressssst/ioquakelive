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
// ui_training.c -- [QL] the Training Center page
//
// Retail's is web.pak's src/training.js, reached from Play > Training Center
// (#play/training): an h1 and three sections, each an image, a title, a line under
// it, a paragraph, and the buttons the section ends with. A section launches its
// map, and two of them also open a video. The words, the order, the maps and the
// commands here are that file's; how it looks is quakeclub.com's, see ui_web.h.
//
// What each one sends, from its launchTraining (bot_minplayers and sv_serverType
// are set to 0 first, as it does):
//
//     Crash Course  map qztraining _training
//     Accelerate    map qzpractice2 _sj; wait 2; team f
//     Elevate       map qzpractice1 _rj; wait 2; team f
//
// The three factories are pak00's hidden ones (scripts/factories.txt: "_training",
// "_sj", "_rj"), which set g_training 1 and, for the practice maps, infinite ammo
// and one weapon. The maps carry the trainingMap worldspawn key, so the game puts
// Crash in and takes the training spawns (g_spawn.c, g_client.c).
//
// The image is each map's own levelshot, from pak00. Retail's cards used three
// screenshots of its own in web.pak, which is not ours to ship.
//

#include "ui_web.h"

#define TR_PAD 14.0f
#define TR_CARD_H 420.0f
#define TR_PIC_ASPECT (9.0f / 16.0f)

typedef enum {
    TR_HIT_NONE,
    TR_HIT_CARD,
    TR_HIT_START,
    TR_HIT_VIDEO
} trHitType_t;

typedef struct {
    const char* map;
    const char* title;     // the section's h2
    const char* subtitle;  // its h3
    const char* text;      // its paragraph
    const char* command;   // what launchTraining sends for it
    const char* video;     // what its Watch Video opens, NULL when it has none
} trCard_t;

/*
The videos are each section's own: Accelerate's button opens HnIeHqBk8v4 and
Elevate's opens pVs1KGK0VLY. Retail names the two handlers the other way round
(watchRocketVideo hangs off Accelerate), which reads like a slip of its own; the
links are left on the sections retail leaves them on. Neither title could be
checked -- YouTube answers 403 to anything but a browser.
*/
static const trCard_t trCards[] = {
    {"qztraining", "Crash Course", "Welcome to the Arena",
     "Meet Crash, our QUAKE LIVE trainer, and get a guided tour of some of the basic items of combat.",
     "map qztraining _training", NULL},
    {"qzpractice2", "Accelerate", "Travelling at High Speed",
     "Practice the basics of Strafe Jumping, an essential skill to learning how to move faster than other arena "
     "combatants.",
     "map qzpractice2 _sj; wait 2; team f", "https://youtu.be/HnIeHqBk8v4"},
    {"qzpractice1", "Elevate", "Reaching Great Heights",
     "Use your projectile weapons to perform Rocket Jumps, and access areas of the arena in entirely different ways.",
     "map qzpractice1 _rj; wait 2; team f", "https://youtu.be/pVs1KGK0VLY"},
};

#define TR_NUM_CARDS ((int)ARRAY_LEN(trCards))

static struct {
    qboolean loaded;
    qhandle_t shot[TR_NUM_CARDS];
} tr;

/*
===============
TR_Launch

What retail's launchTraining does, in its order: the two cvars first, then the
map. Training is an offline match, so the server type goes back to 0 and no bots
are filled in.
===============
*/
static void TR_Launch(int i) {
    trap_Cvar_Set("bot_minplayers", "0");
    trap_Cvar_Set("sv_serverType", "0");
    trap_Cmd_ExecuteText(EXEC_APPEND, va("%s\n", trCards[i].command));
}

static void TR_DrawCard(int i, float x, float y, float w) {
    const trCard_t* card = &trCards[i];
    float inner = w - 2 * TR_PAD;
    float picH = inner * TR_PIC_ASPECT;
    smRect_t box = SM_R(x, y, w, TR_CARD_H);
    qboolean over = SM_In(&box, smMX, smMY);
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    float by, bw;
    vec4_t tint = {1.0f, 1.0f, 1.0f, 1.0f};
    int lines, k;

    SM_Round(x, y, w, TR_CARD_H, SM_RADIUS_MD, over ? smRaised : smPanel, NULL);

    // retail dims the image to 40% and brings it up under the cursor
    tint[3] = over ? 1.0f : 0.4f;
    SM_Round(x + TR_PAD, y + TR_PAD, inner, picH, SM_RADIUS_SM, smPage, NULL);
    trap_R_SetColor(tint);
    SM_Pic(x + TR_PAD, y + TR_PAD, inner, picH, tr.shot[i], NULL);
    trap_R_SetColor(NULL);

    by = y + TR_PAD + picH + 22;
    SM_Label(x + TR_PAD + 4, SM_Baseline(FONT_SANS, 11, by, 16), 11, smGold, card->subtitle, 1.6f);
    SM_Label(x + TR_PAD + 4, SM_Baseline(FONT_SANS, 24, by + 20, 32), 24, smText, card->title, 0.2f);

    lines = SM_Wrap(FONT_SANS, 14, card->text, inner - 8, starts, lens);
    for (k = 0; k < lines && k < 4; k++) {
        SM_Text(x + TR_PAD + 4, SM_Baseline(FONT_SANS, 14, by + 60 + k * 21, 21), FONT_SANS, 14, smMuted,
                card->text + starts[k], lens[k], qfalse);
    }

    // the buttons the section ends with: Watch Video first, as retail orders them
    by = y + TR_CARD_H - TR_PAD - SM_BUTTON_H;
    if (card->video) {
        bw = (inner - 10) * 0.5f;
        SM_Button(x + TR_PAD, by, bw, SM_BUTTON_H, "Watch Video", SM_BUTTON_SECONDARY, SM_Over(TR_HIT_VIDEO, i));
        SM_Hit(x + TR_PAD, by, bw, SM_BUTTON_H, TR_HIT_VIDEO, i, NULL);
        SM_Button(x + TR_PAD + bw + 10, by, bw, SM_BUTTON_H, "Start Training", SM_BUTTON_PRIMARY,
                  SM_Over(TR_HIT_START, i));
        SM_Hit(x + TR_PAD + bw + 10, by, bw, SM_BUTTON_H, TR_HIT_START, i, NULL);
    } else {
        SM_Button(x + TR_PAD, by, inner, SM_BUTTON_H, "Start Training", SM_BUTTON_PRIMARY, SM_Over(TR_HIT_START, i));
        SM_Hit(x + TR_PAD, by, inner, SM_BUTTON_H, TR_HIT_START, i, NULL);
    }
    // the whole section launches it too, the way retail's does
    SM_Hit(x, y, w, TR_CARD_H, TR_HIT_CARD, i, NULL);
}

void UI_Training_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;
    int i;

    if (!tr.loaded) {
        tr.loaded = qtrue;
        for (i = 0; i < TR_NUM_CARDS; i++) {
            tr.shot[i] = trap_R_RegisterShaderNoMip(va("levelshots/%s", trCards[i].map));
        }
    }
    // the page takes keys before the mouse moves over it
    menu = Menus_FindByName("training");
    item = menu ? Menu_FindItemByName(menu, "training") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Training_Paint(void) {
    float left = UI_Home_Left();
    float w = (HM_CONTAINER_W - (TR_NUM_CARDS - 1) * HM_PANEL_GAP) / TR_NUM_CARDS;
    float y;
    int i;

    SM_BeginFrame();
    if (!tr.loaded) {
        UI_Training_Open();
    }

    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_TRAINING);
    // [ioquakelive] The page keeps the frame every other one has: the header right under the top
    // row, as Settings does, and the cards centred in what is left. Hanging the block off the
    // middle of the screen made the header jump from page to page, which is what the user saw as
    // "los 3 centers no estan CENTRADOS como contenedor".
    y = UI_Home_Header(HM_NAV_H, NULL, "Training Center") + HM_PANEL_GAP;
    y += (UI_Home_FrameBottom() - y - TR_CARD_H) * 0.5f;

    for (i = 0; i < TR_NUM_CARDS; i++) {
        TR_DrawCard(i, left + i * (w + HM_PANEL_GAP), y, w);
    }

    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Training_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;  // Escape goes back through the menu's onESC
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    switch (hit->type) {
        case TR_HIT_CARD:
        case TR_HIT_START:
            TR_Launch(hit->index);
            break;
        case TR_HIT_VIDEO:
            // retail opens it in the Steam overlay; without Steam it goes to the
            // browser, which is what OpenSteamOverlayURL falls back to anyway
            trap_Sys_OpenURL(trCards[hit->index].video);
            break;
        default:
            break;
    }
    return qtrue;
}
