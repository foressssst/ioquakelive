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
// ui_replays.c -- [QL] the Replays page
//
// Retail's is a plain list: an h1 that says Replays and a <ul> of the files
// GetDemoList returns, sorted, where clicking one sends `demo "<file>"`. Each row
// carries the match it came from when the browser has it in localStorage, which
// is a thing only its stats service fills in, so here a row is the demo.
//
// Same list, same click, in a panel the way quakeclub.com lays out its tables.
//

#include "ui_web.h"

#define RP_ROW_H 44.0f
#define RP_PAD 25.0f

typedef enum {
    RP_HIT_NONE,
    RP_HIT_ROW,
    RP_HIT_TRACK,
    RP_HIT_THUMB,
    RP_HIT_REFRESH
} rpHitType_t;

static struct {
    float listTop, view, content;
    float scroll;
    qboolean scrollDrag;
    float scrollGrab;
} rp;

static float RP_ClampScroll(float scroll) {
    if (scroll > rp.content - rp.view) {
        scroll = rp.content - rp.view;
    }
    return scroll < 0 ? 0 : scroll;
}

static void RP_DrawScrollbar(float x) {
    float thumbH = rp.view * rp.view / rp.content;
    float thumbY;

    if (thumbH < 28) {
        thumbH = 28;
    }
    thumbY = rp.listTop + (rp.view - thumbH) * rp.scroll / (rp.content - rp.view);
    SM_Hit(x - 4, rp.listTop, 14, rp.view, RP_HIT_TRACK, 0, NULL);
    SM_Pill(x, thumbY, 6, thumbH, (SM_Over(RP_HIT_THUMB, 0) || rp.scrollDrag) ? smMuted : smBorder, NULL);
    SM_Hit(x - 4, thumbY, 14, thumbH, RP_HIT_THUMB, 0, NULL);
}

static void RP_DrawPage(void) {
    float left = UI_Home_Left();
    float y = HM_NAV_H;
    float panelTop, panelH;
    char count[64];
    float cw;
    smRect_t clip;
    int i;

    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_REPLAYS);

    // the header, with the count and a refresh pill at its right
    y = UI_Home_Header(y, NULL, "Replays");
    Com_sprintf(count, sizeof(count), uiInfo.demoCount == 1 ? "%i demo" : "%i demos", uiInfo.demoCount);
    cw = SM_ButtonWidth("Refresh", SM_PILL_H);
    SM_Button(left + HM_CONTAINER_W - RP_PAD - cw, y - 72 + (72 - SM_PILL_H) * 0.5f, cw, SM_PILL_H, "Refresh",
              SM_BUTTON_SECONDARY, SM_Over(RP_HIT_REFRESH, 0));
    SM_Hit(left + HM_CONTAINER_W - RP_PAD - cw, y - 72 + (72 - SM_PILL_H) * 0.5f, cw, SM_PILL_H, RP_HIT_REFRESH, 0,
           NULL);
    SM_Label(left + HM_CONTAINER_W - RP_PAD - cw - 20 - SM_LabelWidth(11, count, 1.4f),
             SM_Baseline(FONT_SANS, 11, y - 72, 72), 11, smMuted, count, 1.4f);

    panelTop = y + HM_PANEL_GAP;
    panelH = smBottom - 40 - panelTop;
    SM_Round(left, panelTop, HM_CONTAINER_W, panelH, SM_RADIUS_MD, smPanel, NULL);

    // the table's head, as the site's: small muted labels
    SM_Label(left + RP_PAD + 12, SM_Baseline(FONT_SANS, 11, panelTop + 22, 16), 11, smMuted, "Demo", 1.4f);

    rp.listTop = panelTop + 50;
    rp.view = panelTop + panelH - RP_PAD - rp.listTop;
    rp.content = uiInfo.demoCount * RP_ROW_H;
    rp.scroll = RP_ClampScroll(rp.scroll);

    if (!uiInfo.demoCount) {
        SM_Text(left + RP_PAD + 12, SM_Baseline(FONT_SANS, 15, rp.listTop + 4, 24), FONT_SANS, 15, smMuted,
                "Nothing recorded yet. Record with \\record, or turn on Auto Record Actions in Settings > Game.", -1,
                qfalse);
        return;
    }

    clip = SM_R(left, rp.listTop, HM_CONTAINER_W, rp.view);
    for (i = 0; i < uiInfo.demoCount; i++) {
        float ry = rp.listTop + i * RP_ROW_H - rp.scroll;
        float rx = left + RP_PAD;
        float rw = HM_CONTAINER_W - 2 * RP_PAD;
        qboolean over = SM_Over(RP_HIT_ROW, i);

        if (ry + RP_ROW_H < rp.listTop || ry > rp.listTop + rp.view) {
            continue;
        }
        // rows are told apart by the background under the cursor, never by lines
        if (over) {
            SM_Round(rx, ry, rw, RP_ROW_H, SM_RADIUS_SM, smRaised, &clip);
        }
        if (SM_TextInside(SM_Baseline(FONT_SANS, 15, ry, RP_ROW_H), 15, &clip)) {
            SM_Text(rx + 12, SM_Baseline(FONT_SANS, 15, ry, RP_ROW_H), FONT_SANS, 15, over ? smText : smTextSoft,
                    uiInfo.demoList[i], SM_Fit(FONT_SANS, 15, uiInfo.demoList[i], rw - 140), qfalse);
        }
        if (over) {
            float bw = SM_ButtonWidth("Play", SM_PILL_H);

            SM_Button(rx + rw - 8 - bw, ry + (RP_ROW_H - SM_PILL_H) * 0.5f, bw, SM_PILL_H, "Play",
                      SM_BUTTON_PRIMARY, qfalse);
        }
        SM_Hit(rx, ry, rw, RP_ROW_H, RP_HIT_ROW, i, &clip);
    }
    if (rp.content > rp.view) {
        RP_DrawScrollbar(left + HM_CONTAINER_W - 14);
    }
}

void UI_Replays_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    rp.scroll = 0;
    rp.scrollDrag = qfalse;
    UI_LoadDemos();

    menu = Menus_FindByName("demo");
    item = menu ? Menu_FindItemByName(menu, "replays") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Replays_Paint(void) {
    SM_BeginFrame();

    // the thumb follows the mouse while the button is held, and lets go with it
    if (rp.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1)) {
            rp.scrollDrag = qfalse;
        } else if (rp.content > rp.view) {
            float thumbH = rp.view * rp.view / rp.content;

            if (thumbH < 28) {
                thumbH = 28;
            }
            rp.scroll =
                RP_ClampScroll(rp.scroll + (smMY - rp.scrollGrab) * (rp.content - rp.view) / (rp.view - thumbH));
            rp.scrollGrab = smMY;
        }
    }

    RP_DrawPage();
    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Replays_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MWHEELUP:
            rp.scroll = RP_ClampScroll(rp.scroll - 3 * RP_ROW_H);
            return qtrue;
        case K_MWHEELDOWN:
            rp.scroll = RP_ClampScroll(rp.scroll + 3 * RP_ROW_H);
            return qtrue;
        case K_MOUSE1:
            break;
        default:
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
        case RP_HIT_ROW:
            if (hit->index >= 0 && hit->index < uiInfo.demoCount) {
                // retail sends exactly this
                trap_Cmd_ExecuteText(EXEC_APPEND, va("demo \"%s\"\n", uiInfo.demoList[hit->index]));
            }
            break;
        case RP_HIT_THUMB:
            rp.scrollDrag = qtrue;
            rp.scrollGrab = smMY;
            break;
        case RP_HIT_REFRESH:
            UI_LoadDemos();
            break;
        default:
            break;
    }
    return qtrue;
}
