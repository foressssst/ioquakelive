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
// ui_browser.c -- [QL] the Match Browser page
//
// Retail's browser is a web page fed by Steam: the client asks Steam for the
// servers of the app (0x00462eb0, one filter "gamedir" = "baseq3") and shows
// what Steam answers for each one (0x00462a50): name, map, players, bots, ping,
// password, VAC and the game's description, which is the gametype. Clicking a
// row connects.
//
// The same list, the same columns and the same click, drawn in the panel the
// rest of the front end uses. The list itself comes from the engine
// (cls.globalServers, filled by Steam) through the LAN_* traps, which is where
// the sorting and the filtering already live.
//

#include "ui_web.h"

#define BR_ROW_H 46.0f
#define BR_PAD 25.0f
#define BR_HEAD_H 34.0f
#define BR_SHOT 34.0f  // the little map shot, square: the previews are 256x256

// the columns, measured from the panel's left edge
#define BR_X_NAME 56.0f
#define BR_X_MODE 452.0f
#define BR_X_MAP 612.0f
#define BR_X_PLAYERS 828.0f
#define BR_X_PING 952.0f
#define BR_W_ROW (HM_CONTAINER_W - 2 * BR_PAD)

typedef enum {
    BR_HIT_NONE,
    BR_HIT_ROW,
    BR_HIT_TRACK,
    BR_HIT_THUMB,
    BR_HIT_REFRESH,
    BR_HIT_HEADER,
    BR_HIT_MODE,
    BR_HIT_EMPTY
} brHitType_t;

// [QL] 0x00560e50: the gametypes, in the game's own order
static const char* brGametypes[] = {"Free For All", "Duel",      "Race",       "Team Deathmatch",
                                    "Clan Arena",   "CTF",       "One Flag",   "Overload",
                                    "Harvester",    "Freeze Tag", "Domination", "Attack & Defend",
                                    "Red Rover"};
#define BR_NUM_GAMETYPES ((int)ARRAY_LEN(brGametypes))

// the filters along the top: the first one is everything
static const struct {
    const char* label;
    int gametype;
} brFilters[] = {{"All", -1},        {"Duel", GT_DUEL},  {"FFA", GT_FFA},    {"TDM", GT_TEAM},
                 {"CA", GT_CA},      {"CTF", GT_CTF},    {"FT", GT_FREEZE},  {"Race", GT_RACE}};
#define BR_NUM_FILTERS ((int)ARRAY_LEN(brFilters))

static struct {
    float listTop, view, content;
    float scroll;
    qboolean scrollDrag;
    float scrollGrab;
    int filter;        // indice en brFilters
    qboolean hideEmpty;
    int rows;          // how many passed the filter on the last frame
    int players;       // y cuanta gente habia en ellos
    int lastBuild;     // when the list was last rebuilt
    int shown[MAX_DISPLAY_SERVERS];  // each row's server index
} br;

static float BR_ClampScroll(float scroll) {
    if (scroll > br.content - br.view) {
        scroll = br.content - br.view;
    }
    return scroll < 0 ? 0 : scroll;
}

static void BR_DrawScrollbar(float x) {
    float thumbH = br.view * br.view / br.content;
    float thumbY;

    if (thumbH < 28) {
        thumbH = 28;
    }
    thumbY = br.listTop + (br.view - thumbH) * br.scroll / (br.content - br.view);
    SM_Hit(x - 4, br.listTop, 14, br.view, BR_HIT_TRACK, 0, NULL);
    SM_Pill(x, thumbY, 6, thumbH, (SM_Over(BR_HIT_THUMB, 0) || br.scrollDrag) ? smMuted : smBorder, NULL);
    SM_Hit(x - 4, thumbY, 14, thumbH, BR_HIT_THUMB, 0, NULL);
}

/*
===============
BR_Levelshot

The little map shot on each row. Registering a shader per row per frame is expensive
even when the engine has it cached, so the ones already asked for are kept here. A map
with no shot of its own uses the fallback.
===============
*/
#define BR_MAX_SHOTS 96

static struct {
    char map[32];
    qhandle_t shot;
} brShots[BR_MAX_SHOTS];
static int brNumShots;
static qhandle_t brDefaultShot;

static qhandle_t BR_Levelshot(const char* map) {
    int i;

    if (!brDefaultShot) {
        brDefaultShot = trap_R_RegisterShaderNoMip("levelshots/preview/default.jpg");
    }
    if (!map || !map[0]) {
        return brDefaultShot;
    }
    for (i = 0; i < brNumShots; i++) {
        if (!Q_stricmp(brShots[i].map, map)) {
            return brShots[i].shot ? brShots[i].shot : brDefaultShot;
        }
    }
    if (brNumShots >= BR_MAX_SHOTS) {
        brNumShots = 0;  // the batch on screen fits with room to spare; start over
    }
    Q_strncpyz(brShots[brNumShots].map, map, sizeof(brShots[brNumShots].map));
    brShots[brNumShots].shot = trap_R_RegisterShaderNoMip(va("levelshots/preview/%s.jpg", map));
    brNumShots++;
    return brShots[brNumShots - 1].shot ? brShots[brNumShots - 1].shot : brDefaultShot;
}

// the gametype, under the name the server publishes it by
static const char* BR_GametypeName(int gametype) {
    if (gametype < 0 || gametype >= BR_NUM_GAMETYPES) {
        return "Unknown";
    }
    return brGametypes[gametype];
}

/*
===============
BR_Collect

Walks the list the engine builds and keeps the rows that pass the filter along the
top. The order comes in already set: the sort column decides it.
===============
*/
static void BR_Collect(void) {
    char info[MAX_STRING_CHARS];
    int i;

    br.rows = 0;
    br.players = 0;
    for (i = 0; i < uiInfo.serverStatus.numDisplayServers && br.rows < MAX_DISPLAY_SERVERS; i++) {
        int index = uiInfo.serverStatus.displayServers[i];
        int clients;

        trap_LAN_GetServerInfo(ui_netSource.integer, index, info, sizeof(info));
        clients = atoi(Info_ValueForKey(info, "clients"));
        br.players += clients;

        if (br.hideEmpty && clients == 0) {
            continue;
        }
        if (brFilters[br.filter].gametype >= 0 &&
            atoi(Info_ValueForKey(info, "gametype")) != brFilters[br.filter].gametype) {
            continue;
        }
        br.shown[br.rows++] = index;
    }
}

// a text cell, clipped to what fits in its column
static void BR_Cell(float x, float baseline, float width, const char* text, const vec4_t color,
                    const smRect_t* clip) {
    SM_Text(x, baseline, FONT_SANS, 14, color, text, SM_Fit(FONT_SANS, 14, text, width), qfalse);
    (void)clip;
}

static void BR_DrawHeader(float left, float y, const char* label, float x, int column) {
    qboolean active = (qboolean)(uiInfo.serverStatus.sortKey == column);
    char text[64];

    if (active) {
        Com_sprintf(text, sizeof(text), "%s %s", label, uiInfo.serverStatus.sortDir ? "^" : "v");
    } else {
        Q_strncpyz(text, label, sizeof(text));
    }
    SM_Label(left + x, SM_Baseline(FONT_SANS, 11, y, BR_HEAD_H), 11,
             (active || SM_Over(BR_HIT_HEADER, column)) ? smText : smMuted, text, 1.4f);
    SM_Hit(left + x - 6, y, 160, BR_HEAD_H, BR_HIT_HEADER, column, NULL);
}

static void BR_DrawPage(void) {
    float left = UI_Home_Left();
    float y = HM_NAV_H;
    float panelTop, panelH, filterY, headY;
    char count[96];
    float cw, fx;
    smRect_t clip;
    int i;

    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_BROWSER);

    // the header, with the count and the refresh button on the right
    y = UI_Home_Header(y, NULL, "Match Browser");
    Com_sprintf(count, sizeof(count), br.rows == 1 ? "%i server, %i playing" : "%i servers, %i playing",
                br.rows, br.players);
    cw = SM_ButtonWidth("Refresh", SM_PILL_H);
    SM_Button(left + HM_CONTAINER_W - BR_PAD - cw, y - 72 + (72 - SM_PILL_H) * 0.5f, cw, SM_PILL_H, "Refresh",
              SM_BUTTON_SECONDARY, SM_Over(BR_HIT_REFRESH, 0));
    SM_Hit(left + HM_CONTAINER_W - BR_PAD - cw, y - 72 + (72 - SM_PILL_H) * 0.5f, cw, SM_PILL_H, BR_HIT_REFRESH, 0,
           NULL);
    SM_Label(left + HM_CONTAINER_W - BR_PAD - cw - 20 - SM_LabelWidth(11, count, 1.4f),
             SM_Baseline(FONT_SANS, 11, y - 72, 72), 11, smMuted, count, 1.4f);

    panelTop = y + HM_PANEL_GAP;
    panelH = smBottom - 40 - panelTop;
    SM_Round(left, panelTop, HM_CONTAINER_W, panelH, SM_RADIUS_MD, smPanel, NULL);

    // the filter row, inside the panel's edge
    filterY = panelTop + 16;
    fx = left + BR_PAD;
    for (i = 0; i < BR_NUM_FILTERS; i++) {
        float w = SM_ButtonWidth(brFilters[i].label, SM_PILL_H);

        SM_Button(fx, filterY, w, SM_PILL_H, brFilters[i].label,
                  br.filter == i ? SM_BUTTON_PRIMARY : SM_BUTTON_SECONDARY, SM_Over(BR_HIT_MODE, i));
        SM_Hit(fx, filterY, w, SM_PILL_H, BR_HIT_MODE, i, NULL);
        fx += w + 8;
    }
    {
        const char* label = br.hideEmpty ? "Hiding empty" : "Showing empty";
        float w = SM_ButtonWidth(label, SM_PILL_H);

        SM_Button(left + HM_CONTAINER_W - BR_PAD - w, filterY, w, SM_PILL_H, label,
                  br.hideEmpty ? SM_BUTTON_PRIMARY : SM_BUTTON_SECONDARY, SM_Over(BR_HIT_EMPTY, 0));
        SM_Hit(left + HM_CONTAINER_W - BR_PAD - w, filterY, w, SM_PILL_H, BR_HIT_EMPTY, 0, NULL);
    }

    // the column headers, which sort when clicked
    headY = filterY + SM_PILL_H + 14;
    BR_DrawHeader(left + BR_PAD, headY, "Server", BR_X_NAME, SORT_HOST);
    BR_DrawHeader(left + BR_PAD, headY, "Mode", BR_X_MODE, SORT_GAME);
    BR_DrawHeader(left + BR_PAD, headY, "Map", BR_X_MAP, SORT_MAP);
    BR_DrawHeader(left + BR_PAD, headY, "Players", BR_X_PLAYERS, SORT_CLIENTS);
    BR_DrawHeader(left + BR_PAD, headY, "Ping", BR_X_PING, SORT_PING);

    br.listTop = headY + BR_HEAD_H;
    br.view = panelTop + panelH - BR_PAD - br.listTop;
    br.content = br.rows * BR_ROW_H;
    br.scroll = BR_ClampScroll(br.scroll);

    if (!br.rows) {
        SM_Text(left + BR_PAD + BR_X_NAME, SM_Baseline(FONT_SANS, 15, br.listTop + 4, 24), FONT_SANS, 15, smMuted,
                trap_LAN_GetServerCount(ui_netSource.integer) > 0
                    ? "No server matches these filters."
                    : "Looking for servers... they show up as Steam answers for each one.",
                -1, qfalse);
        return;
    }

    clip = SM_R(left, br.listTop, HM_CONTAINER_W, br.view);
    for (i = 0; i < br.rows; i++) {
        float ry = br.listTop + i * BR_ROW_H - br.scroll;
        float rx = left + BR_PAD;
        qboolean over = SM_Over(BR_HIT_ROW, i);
        char info[MAX_STRING_CHARS];
        char players[32];
        char ping[16];
        char address[MAX_ADDRESSLENGTH];
        const char* name;
        const vec4_t* color;
        float baseline;

        if (ry + BR_ROW_H < br.listTop || ry > br.listTop + br.view) {
            continue;
        }
        if (over) {
            SM_Round(rx, ry, BR_W_ROW, BR_ROW_H, SM_RADIUS_SM, smRaised, &clip);
        }
        baseline = SM_Baseline(FONT_SANS, 14, ry, BR_ROW_H);
        if (!SM_TextInside(baseline, 14, &clip)) {
            SM_Hit(rx, ry, BR_W_ROW, BR_ROW_H, BR_HIT_ROW, i, &clip);
            continue;
        }

        trap_LAN_GetServerInfo(ui_netSource.integer, br.shown[i], info, sizeof(info));
        color = over ? &smText : &smTextSoft;

        // the map, to the left of the server name
        SM_Pic(rx + 10, ry + (BR_ROW_H - BR_SHOT) * 0.5f, BR_SHOT, BR_SHOT,
               BR_Levelshot(Info_ValueForKey(info, "mapname")), &clip);
        Com_sprintf(players, sizeof(players), "%s/%s", Info_ValueForKey(info, "clients"),
                    Info_ValueForKey(info, "sv_maxclients"));
        Com_sprintf(ping, sizeof(ping), "%s", Info_ValueForKey(info, "ping"));

        // a server that publishes no name is shown by its address, which is all that is known
        name = Info_ValueForKey(info, "hostname");
        if (!name[0]) {
            trap_LAN_GetServerAddressString(ui_netSource.integer, br.shown[i], address, sizeof(address));
            name = address;
        }
        BR_Cell(rx + BR_X_NAME, baseline, BR_X_MODE - BR_X_NAME - 20, name, *color, &clip);
        BR_Cell(rx + BR_X_MODE, baseline, BR_X_MAP - BR_X_MODE - 20,
                BR_GametypeName(atoi(Info_ValueForKey(info, "gametype"))), smTextSoft, &clip);
        BR_Cell(rx + BR_X_MAP, baseline, BR_X_PLAYERS - BR_X_MAP - 20, Info_ValueForKey(info, "mapname"),
                smTextSoft, &clip);
        BR_Cell(rx + BR_X_PLAYERS, baseline, BR_X_PING - BR_X_PLAYERS - 20, players, smTextSoft, &clip);
        BR_Cell(rx + BR_X_PING, baseline, 60, ping, smTextSoft, &clip);

        if (over) {
            float bw = SM_ButtonWidth("Join", SM_PILL_H);

            SM_Button(rx + BR_W_ROW - 8 - bw, ry + (BR_ROW_H - SM_PILL_H) * 0.5f, bw, SM_PILL_H, "Join",
                      SM_BUTTON_PRIMARY, qfalse);
        }
        SM_Hit(rx, ry, BR_W_ROW, BR_ROW_H, BR_HIT_ROW, i, &clip);
    }
    if (br.content > br.view) {
        BR_DrawScrollbar(left + HM_CONTAINER_W - 14);
    }
}

void UI_Browser_Open(void) {
    br.scroll = 0;
    br.scrollDrag = qfalse;
    br.rows = 0;
    br.players = 0;
    br.lastBuild = 0;
    // the list is the global one, which is what Steam gives. The cached value is updated by
    // hand: UI_UpdateCvars does not run until the next frame and the refresh goes out now.
    trap_Cvar_Set("ui_netSource", "2");  // AS_GLOBAL
    trap_Cvar_Update(&ui_netSource);
    UI_Home_ServerList(qtrue);
}

void UI_Browser_Paint(void) {
    SM_BeginFrame();

    // the list grows while Steam answers, so it is rebuilt every so often
    if (uiInfo.uiDC.realTime - br.lastBuild > 500) {
        br.lastBuild = uiInfo.uiDC.realTime;
        UI_Home_ServerList(qfalse);
    }
    BR_Collect();

    if (br.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1)) {
            br.scrollDrag = qfalse;
        } else if (br.content > br.view) {
            float thumbH = br.view * br.view / br.content;

            if (thumbH < 28) {
                thumbH = 28;
            }
            br.scroll =
                BR_ClampScroll(br.scroll + (smMY - br.scrollGrab) * (br.content - br.view) / (br.view - thumbH));
            br.scrollGrab = smMY;
        }
    }

    BR_DrawPage();
    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Browser_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MWHEELUP:
            br.scroll = BR_ClampScroll(br.scroll - 3 * BR_ROW_H);
            return qtrue;
        case K_MWHEELDOWN:
            br.scroll = BR_ClampScroll(br.scroll + 3 * BR_ROW_H);
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
        case BR_HIT_ROW:
            if (hit->index >= 0 && hit->index < br.rows) {
                char address[MAX_ADDRESSLENGTH];

                trap_LAN_GetServerAddressString(ui_netSource.integer, br.shown[hit->index], address,
                                                sizeof(address));
                trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", address));
            }
            break;
        case BR_HIT_THUMB:
            br.scrollDrag = qtrue;
            br.scrollGrab = smMY;
            break;
        case BR_HIT_REFRESH:
            br.scroll = 0;
            UI_Home_ServerList(qtrue);
            break;
        case BR_HIT_HEADER:
            UI_Browser_Sort(hit->index);
            break;
        case BR_HIT_MODE:
            br.filter = hit->index;
            br.scroll = 0;
            break;
        case BR_HIT_EMPTY:
            br.hideEmpty = (qboolean)!br.hideEmpty;
            br.scroll = 0;
            break;
        default:
            break;
    }
    return qtrue;
}
