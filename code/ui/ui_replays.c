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
// Same list, same click, drawn with ui_web.c under the same bar as the rest.
//

#include "ui_web.h"

#define RP_ROW_H 42.0f
#define RP_PAGE_MAX_W 1100.0f
#define RP_PAGE_MARGIN 60.0f
#define RP_TOP 118.0f
#define RP_BOTTOM 46.0f

typedef enum {
    RP_HIT_NONE,
    RP_HIT_ROW,
    RP_HIT_TRACK,
    RP_HIT_THUMB
} rpHitType_t;

static struct {
    float page, pageW;
    float listTop, view, content;
    float scroll;
    qboolean scrollDrag;
    float scrollGrab;
    int chosen;
} rp;

static float RP_ClampScroll(float scroll) {
    if (scroll > rp.content - rp.view) {
        scroll = rp.content - rp.view;
    }
    return scroll < 0 ? 0 : scroll;
}

static void RP_Frame(void) {
    rp.pageW = smVW - 2 * RP_PAGE_MARGIN;
    if (rp.pageW > RP_PAGE_MAX_W) {
        rp.pageW = RP_PAGE_MAX_W;
    }
    rp.page = (smVW - rp.pageW) * 0.5f;
    rp.listTop = RP_TOP + 62;
    rp.view = (float)SM_VIEW_H - RP_BOTTOM - rp.listTop;
    rp.content = uiInfo.demoCount * RP_ROW_H;
}

static void RP_DrawScrollbar(void) {
    float x = rp.page + rp.pageW + 12;
    float thumbH = rp.view * rp.view / rp.content;
    float thumbY;

    if (thumbH < 24) {
        thumbH = 24;
    }
    thumbY = rp.listTop + (rp.view - thumbH) * rp.scroll / (rp.content - rp.view);
    SM_Fill(x, rp.listTop, 6, rp.view, smRowEven, NULL);
    SM_Hit(x, rp.listTop, 6, rp.view, RP_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, 6, thumbH, (SM_Over(RP_HIT_THUMB, 0) || rp.scrollDrag) ? smArrow : smGrey, NULL);
    SM_Hit(x, thumbY, 6, thumbH, RP_HIT_THUMB, 0, NULL);
}

static void RP_DrawPage(void) {
    smRect_t clip;
    int i;

    RP_Frame();
    rp.scroll = RP_ClampScroll(rp.scroll);

    UI_Home_Backdrop();
    UI_Home_Nav(1);  // HM_NAV_REPLAYS

    SM_SpacedText(rp.page, SM_Baseline(FONT_DEFAULT, 30, RP_TOP, 38), FONT_DEFAULT, 30, smText, "Replays", -1, qfalse);
    SM_Round(rp.page, RP_TOP + 42, 56, 3, 1.5f, smYellow, NULL);
    SM_Round(rp.page - 6, rp.listTop - 10, rp.pageW + 12, rp.content + 20 < rp.view ? rp.content + 20 : rp.view,
             SM_RADIUS_LG, smRowEven, NULL);

    if (!uiInfo.demoCount) {
        SM_Text(rp.page, SM_Baseline(FONT_SANS, 16, rp.listTop + 8, 24), FONT_SANS, 16, smSelectText,
                "Nothing recorded yet. Bind a key to record, or turn on Auto Record Actions in Settings > Game.", -1,
                qfalse);
        return;
    }

    clip = SM_R(rp.page - 4, rp.listTop, rp.pageW + 8, rp.view);
    for (i = 0; i < uiInfo.demoCount; i++) {
        float y = rp.listTop + i * RP_ROW_H - rp.scroll;
        qboolean over = SM_Over(RP_HIT_ROW, i);
        smRect_t row;

        if (y + RP_ROW_H < rp.listTop || y > rp.listTop + rp.view) {
            continue;
        }
        row = SM_R(rp.page, y, rp.pageW, RP_ROW_H);
        if (over) {
            row.y -= SM_LIFT;  // the row lifts under the cursor
            SM_Round(row.x, row.y, row.w, row.h, SM_RADIUS_MD, smCardHover, &clip);
            SM_Round(row.x, row.y + 6, 2, row.h - 12, 1, smYellow, &clip);
        }
        SM_Text(row.x + 16, SM_Baseline(FONT_SANS, 15, row.y + 10, 22), FONT_SANS, 15, over ? smText : smSelectText,
                uiInfo.demoList[i], SM_Fit(FONT_SANS, 15, uiInfo.demoList[i], rp.pageW - 120), qfalse);
        if (over) {
            SM_Text(row.x + row.w - 66, SM_Baseline(FONT_SANS, 14, row.y + 11, 22), FONT_SANS, 14, smYellow, "PLAY",
                    -1, qtrue);
        }
        SM_Hit(row.x, row.y, row.w, row.h, RP_HIT_ROW, i, &clip);
    }
    if (rp.content > rp.view) {
        RP_DrawScrollbar();
    }
}

void UI_Replays_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    rp.scroll = 0;
    rp.scrollDrag = qfalse;
    rp.chosen = -1;
    UI_LoadDemos();

    menu = Menus_FindByName("demo");
    item = menu ? Menu_FindItemByName(menu, "replays") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Replays_Paint(void) {
    const smHit_t* under;

    SM_Frame();
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;

    if (rp.scrollDrag && rp.content > rp.view) {
        float thumbH = rp.view * rp.view / rp.content;

        if (thumbH < 24) {
            thumbH = 24;
        }
        rp.scroll = RP_ClampScroll(rp.scroll + (smMY - rp.scrollGrab) * (rp.content - rp.view) / (rp.view - thumbH));
        rp.scrollGrab = smMY;
    }

    RP_DrawPage();

    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
}

qboolean UI_Replays_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MWHEELUP:
            rp.scroll = RP_ClampScroll(rp.scroll - 90);
            return qtrue;
        case K_MWHEELDOWN:
            rp.scroll = RP_ClampScroll(rp.scroll + 90);
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
    if (hit->type == RP_HIT_ROW && hit->index >= 0 && hit->index < uiInfo.demoCount) {
        // retail sends exactly this
        trap_Cmd_ExecuteText(EXEC_APPEND, va("demo \"%s\"\n", uiInfo.demoList[hit->index]));
    } else if (hit->type == RP_HIT_THUMB) {
        rp.scrollDrag = qtrue;
        rp.scrollGrab = smMY;
    }
    return qtrue;
}

void UI_Replays_MouseUp(void) {
    rp.scrollDrag = qfalse;
}
