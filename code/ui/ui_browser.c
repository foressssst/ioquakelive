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
// Retail's browser is a web page fed by Steam (web.pak's modules 257 to 260): the
// client asks Steam for the servers of the app (0x00462eb0, one filter "gamedir" =
// "baseq3") and shows what Steam answers for each (0x00462a50). The page filters and
// sorts on its own, and asks Steam for the rules and the players of the server that is
// clicked, which the server publishes itself (qzeroded.x64 0x4468a0).
//
// Drawn here the way the page lays it out at 1080 lines (css/main.css): the list takes
// 67% of the content and the details of the selected match 32% beside it. A screen
// narrower than 3:2 hides the details and shows them in a dialog, as the page does.
//

#include "ui_web.h"

// the page's boxes, in its CSS pixels
#define BR_TITLE_H 54.4f
#define BR_SECTION_H 50.0f
#define BR_FILTERS_H 154.0f
#define BR_HEAD_H 17.1f
#define BR_ROW_H 50.5f
#define BR_ROW_STEP 51.4f
#define BR_SCROLLBAR 8.0f  // *::-webkit-scrollbar, and the column is overflow-y: scroll
#define BR_BUTTON_H 30.0f
#define BR_SELECT_H 36.0f
#define BR_DETAILS_MAP_H 220.0f

#define BR_MAX_TAGS 12        // a server's
#define BR_MAX_FILTER_TAGS 16
#define BR_MAX_TAGLIST 128    // every tag seen, for Common Tags
#define BR_TAG_LEN 32

typedef enum {
    BR_HIT_NONE,
    BR_HIT_TOGGLE,
    BR_HIT_REFRESH_ALL,
    BR_HIT_QUICK_REFRESH,
    BR_HIT_SORT,
    BR_HIT_ROW,
    BR_HIT_PLAY,
    BR_HIT_TAG,
    BR_HIT_TRACK,
    BR_HIT_THUMB,
    BR_HIT_CHECK,
    BR_HIT_SELECT,
    BR_HIT_OPTION,
    BR_HIT_TAGBOX,
    BR_HIT_TAGREMOVE,
    BR_HIT_JOIN,
    BR_HIT_PLAYERS,
    BR_HIT_MENU_ITEM,
    BR_HIT_OVERLAY,
    BR_HIT_DIALOG,
    BR_HIT_MODAL_CLOSE,
    BR_HIT_MODAL_JOIN,
    BR_HIT_PASS_FIELD,
    BR_HIT_PASS_OK,
    BR_HIT_PASS_CANCEL
} brHitType_t;

// what the page sorts by (filters.sort)
typedef enum { BR_SORT_NAME, BR_SORT_GAMETYPE, BR_SORT_MAP, BR_SORT_PLAYERS, BR_SORT_PING } brSort_t;

// the checkboxes of the filters (filters.vac and so on)
typedef enum { BR_CHECK_VAC, BR_CHECK_PASSWORD, BR_CHECK_EMPTY, BR_CHECK_FULL } brCheck_t;

// the three react-selects
typedef enum { BR_DROP_NONE, BR_DROP_TYPE, BR_DROP_PING, BR_DROP_TAGS } brDrop_t;

typedef enum { BR_MODAL_NONE, BR_MODAL_INFO, BR_MODAL_PASSWORD } brModal_t;

// the serverfilters module's options
static const char* const brTypes[] = {"Internet", "LAN", "Friends", "Favorites", "History"};
static const int brPings[] = {0, 25, 50, 75, 100, 150, 200};

// web.pak's gametypes module (237): the team ones get a scoreboard and their limit is named
// by the condition. Overload is not there, so it is the unknown one.
static const struct {
    qboolean team;
    const char* condition;
} brGametypes[] = {
    {qfalse, "fraglimit"},    {qfalse, "fraglimit"},    {qfalse, NULL},        {qtrue, "fraglimit"},
    {qtrue, "roundlimit"},    {qtrue, "capturelimit"},  {qtrue, "capturelimit"}, {qfalse, NULL},
    {qtrue, "capturelimit"},  {qtrue, "roundlimit"},    {qtrue, "scorelimit"}, {qtrue, NULL},
    {qfalse, "roundlimit"},
};

// a server as the page keeps it (serverlist's server object)
typedef struct {
    int index;  // in the engine's list
    char address[MAX_ADDRESSLENGTH];
    char name[64];
    char map[64];
    char gametype[64];  // the description the server publishes: "Clan Arena"
    char tags[BR_MAX_TAGS][BR_TAG_LEN];
    int numTags;
    int players;  // without the bots, as the page counts them
    int maxPlayers;
    int ping;
    qboolean password;
    qboolean vac;
} brServer_t;

static brServer_t brServers[MAX_DISPLAY_SERVERS];
static int brNumServers;
static int brOrder[MAX_DISPLAY_SERVERS];  // the ones that pass the filters, sorted
static int brNumOrder;
static char brTagList[BR_MAX_TAGLIST][BR_TAG_LEN];
static int brNumTagList;

// filters, which the page keeps in localStorage ("browser_filters") and here in a cvar
static struct {
    char tags[BR_MAX_FILTER_TAGS][BR_TAG_LEN];
    int numTags;
    int type;
    qboolean empty, full, vac, password;
    int ping;
    int sort;
    qboolean reverse;
} brf;

static struct {
    qboolean loaded;
    qboolean showFilters;

    float scroll;
    float view, content;
    qboolean scrollDrag;
    float scrollGrab;
    int lastBuild;
    int shownRows;

    // the selected match and what Steam said about it
    qboolean hasSelected;
    brServer_t selected;
    int details;  // UI_DETAILS_*
    char rules[8192];
    char players[4096];
    float playersScroll;
    qboolean detailsForModal;  // who the request going on is for

    int dropdown;
    float dropScroll;
    char tagInput[BR_TAG_LEN];
    qboolean tagFocus;

    qboolean menuOpen;
    float menuX, menuY;
    brServer_t menuServer;

    int modal;
    brServer_t modalServer;
    int modalDetails;
    char modalRules[8192];
    char modalPlayers[4096];
    int modalRefresh;
    char password[64];
} br;

static const vec4_t brBlack80 = {0, 0, 0, 0.8f};
static const vec4_t brBlack75 = {0, 0, 0, 0.75f};
static const vec4_t brBlack60 = {0, 0, 0, 0.6f};
static const vec4_t brOverlay20 = {0, 0, 0, 0.2f};
static const vec4_t brSection = SM_RGB(25, 25, 25);
static const vec4_t brHead = SM_RGB(17, 17, 17);
static const vec4_t brHeadText = SM_RGB(238, 238, 238);
static const vec4_t brRowText = SM_RGB(204, 204, 204);
static const vec4_t brRowHover = SM_RGB(30, 30, 30);
static const vec4_t brRowSelected = SM_RGB(50, 50, 50);
static const vec4_t brDesc = SM_RGB(85, 85, 85);
static const vec4_t brPingText = SM_RGB(221, 221, 221);
static const vec4_t brLink = SM_RGB(30, 174, 219);
static const vec4_t brLinkHover = SM_RGB(15, 160, 206);
static const vec4_t brButton = SM_RGB(61, 61, 61);
static const vec4_t brButtonBorder = SM_RGB(77, 77, 77);
static const vec4_t brButtonHover = SM_RGB(93, 93, 93);
static const vec4_t brButtonHoverBorder = SM_RGB(153, 153, 153);
static const vec4_t brDisabled = SM_RGB(34, 34, 34);
static const vec4_t brDisabledBorder = SM_RGB(17, 17, 17);
static const vec4_t brDisabledText = SM_RGB(51, 51, 51);
static const vec4_t brYellow = SM_RGB(247, 209, 0);
static const vec4_t brPlayHover = {75 / 255.0f, 75 / 255.0f, 75 / 255.0f, 0.8f};
static const vec4_t brSelect = SM_RGB(51, 51, 51);
static const vec4_t brSelectHover = SM_RGB(68, 68, 68);
static const vec4_t brSelectText = SM_RGB(170, 170, 170);
static const vec4_t brOption = SM_RGB(187, 187, 187);
static const vec4_t brOptionFocus = SM_RGB(34, 34, 34);
static const vec4_t brField = SM_RGB(15, 15, 15);
static const vec4_t brFieldBorder = SM_RGB(51, 51, 51);
static const vec4_t brPlaceholder = SM_RGB(119, 119, 119);
static const vec4_t brTagRemove = SM_RGB(134, 32, 32);
static const vec4_t brCheckOn = SM_RGB(0, 117, 255);
static const vec4_t brCheckOff = SM_RGB(118, 118, 118);
static const vec4_t brWhite = {1, 1, 1, 1};
static const vec4_t brBlack = {0, 0, 0, 1};
static const vec4_t brThumb = SM_RGB(51, 51, 51);
static const vec4_t brThumbHover = SM_RGB(153, 153, 153);
static const vec4_t brInfo = SM_RGB(238, 238, 238);
static const vec4_t brRed = SM_RGB(204, 51, 51);
static const vec4_t brBlue = SM_RGB(0, 102, 153);
static const vec4_t brDialog = SM_RGB(30, 30, 30);

#define SM_FA_FILTER_ICON 0xf0b0
#define SM_FA_SORT_ASC 0xf0de
#define SM_FA_SORT_DESC 0xf0dd
#define SM_FA_CHEVRON_CIRCLE_RIGHT 0xf138
#define SM_FA_ANGLE_DOUBLE_RIGHT 0xf101
#define SM_FA_ANGLE_DOUBLE_LEFT 0xf100
#define SM_FA_TIMES_CIRCLE 0xf057
#define SM_FA_CHECK 0xf00c

/*
===============================================================================

PICTURES

The map shots and the ping bars come out of web.pak. Registering a shader per row per
frame is expensive even when the engine has it cached, so the ones asked for are kept;
a map with no shot of its own falls back to the page's default one.

===============================================================================
*/

#define BR_MAX_SHOTS 128

static struct {
    char path[80];
    qhandle_t shot;
} brShots[BR_MAX_SHOTS];
static int brNumShots;

static qhandle_t BR_Shot(const char* size, const char* map) {
    char path[80];
    int i;

    Com_sprintf(path, sizeof(path), "webui/img/levelshots/%s/%s.jpg", size, map && map[0] ? map : "default");
    Q_strlwr(path);
    for (i = 0; i < brNumShots; i++) {
        if (!strcmp(brShots[i].path, path)) {
            break;
        }
    }
    if (i == brNumShots) {
        if (brNumShots == BR_MAX_SHOTS) {
            brNumShots = 0;  // the batch on screen fits with room to spare; start over
        }
        i = brNumShots++;
        Q_strncpyz(brShots[i].path, path, sizeof(brShots[i].path));
        brShots[i].shot = trap_R_RegisterShaderNoMip(path);
    }
    if (!brShots[i].shot && map && map[0]) {
        return BR_Shot(size, NULL);
    }
    return brShots[i].shot;
}

// the ping-component: three bars up to 60, two up to 100, one above
static qhandle_t BR_Quality(int ping) {
    static qhandle_t shots[4];
    int q = ping <= 60 ? 3 : ping <= 100 ? 2 : 1;

    if (!shots[q]) {
        shots[q] = trap_R_RegisterShaderNoMip(va("webui/img/quality/%i.png", q));
    }
    return shots[q];
}

/*
===============================================================================

THE MAPS' NAMES

The page's mapdb: the name each arena goes by, which the engine hands the page
(GetMapList) and the UI reads the same way Start Match does.

===============================================================================
*/

#define BR_MAX_ARENAS 512

static struct {
    char sysname[64];
    char name[64];
} brArenas[BR_MAX_ARENAS];
static int brNumArenas = -1;

static int BR_ArenaCompare(const void* a, const void* b) {
    return Q_stricmp((const char*)a, (const char*)b);
}

static const char* BR_MapName(const char* map) {
    const char* found;

    if (brNumArenas < 0) {
        int count = trap_Arena_Count();
        int i;

        brNumArenas = 0;
        for (i = 0; i < count && brNumArenas < BR_MAX_ARENAS; i++) {
            if (trap_Arena_Value(i, "sysname", brArenas[brNumArenas].sysname, sizeof(brArenas[0].sysname)) &&
                brArenas[brNumArenas].sysname[0]) {
                trap_Arena_Value(i, "name", brArenas[brNumArenas].name, sizeof(brArenas[0].name));
                brNumArenas++;
            }
        }
        qsort(brArenas, brNumArenas, sizeof(brArenas[0]), BR_ArenaCompare);
    }
    found = (const char*)bsearch(map, brArenas, brNumArenas, sizeof(brArenas[0]), BR_ArenaCompare);
    return found && found[sizeof(brArenas[0].sysname)] ? found + sizeof(brArenas[0].sysname) : map;
}

/*
===============================================================================

THE FILTERS

The page's defaults: every type of list shows the Internet's, VAC servers only, no
passwords, sorted by ping.

===============================================================================
*/

static void BR_DefaultFilters(void) {
    Com_Memset(&brf, 0, sizeof(brf));
    brf.vac = qtrue;
    brf.password = qtrue;
    brf.sort = BR_SORT_PING;
}

static void BR_SaveFilters(void) {
    char info[MAX_INFO_STRING];
    char tags[BR_MAX_FILTER_TAGS * BR_TAG_LEN];
    int i;

    info[0] = '\0';
    tags[0] = '\0';
    for (i = 0; i < brf.numTags; i++) {
        if (i) {
            Q_strcat(tags, sizeof(tags), ",");
        }
        Q_strcat(tags, sizeof(tags), brf.tags[i]);
    }
    Info_SetValueForKey(info, "type", va("%i", brf.type));
    Info_SetValueForKey(info, "empty", va("%i", brf.empty));
    Info_SetValueForKey(info, "full", va("%i", brf.full));
    Info_SetValueForKey(info, "vac", va("%i", brf.vac));
    Info_SetValueForKey(info, "password", va("%i", brf.password));
    Info_SetValueForKey(info, "ping", va("%i", brf.ping));
    Info_SetValueForKey(info, "sort", va("%i", brf.sort));
    Info_SetValueForKey(info, "reverse", va("%i", brf.reverse));
    Info_SetValueForKey(info, "tags", tags);
    trap_Cvar_Set("ui_browserFilters", info);
}

static void BR_LoadFilters(void) {
    char info[MAX_INFO_STRING];
    char tags[MAX_INFO_VALUE];
    char* p;

    BR_DefaultFilters();
    trap_Cvar_Register(NULL, "ui_browserFilters", "", CVAR_ARCHIVE);
    trap_Cvar_VariableStringBuffer("ui_browserFilters", info, sizeof(info));
    if (!info[0]) {
        return;
    }
    brf.type = (int)Com_Clamp(0, 4, atoi(Info_ValueForKey(info, "type")));
    brf.empty = (qboolean)(atoi(Info_ValueForKey(info, "empty")) != 0);
    brf.full = (qboolean)(atoi(Info_ValueForKey(info, "full")) != 0);
    brf.vac = (qboolean)(atoi(Info_ValueForKey(info, "vac")) != 0);
    brf.password = (qboolean)(atoi(Info_ValueForKey(info, "password")) != 0);
    brf.ping = atoi(Info_ValueForKey(info, "ping"));
    brf.sort = (int)Com_Clamp(BR_SORT_NAME, BR_SORT_PING, atoi(Info_ValueForKey(info, "sort")));
    brf.reverse = (qboolean)(atoi(Info_ValueForKey(info, "reverse")) != 0);
    Q_strncpyz(tags, Info_ValueForKey(info, "tags"), sizeof(tags));
    for (p = strtok(tags, ","); p && brf.numTags < BR_MAX_FILTER_TAGS; p = strtok(NULL, ",")) {
        Q_strncpyz(brf.tags[brf.numTags++], p, BR_TAG_LEN);
    }
}

// the page's addTag: a union, so a tag already there is not added twice
static void BR_AddFilterTag(const char* tag) {
    int i;

    if (!tag || !tag[0] || brf.numTags == BR_MAX_FILTER_TAGS) {
        return;
    }
    for (i = 0; i < brf.numTags; i++) {
        if (!strcmp(brf.tags[i], tag)) {
            return;
        }
    }
    Q_strncpyz(brf.tags[brf.numTags++], tag, BR_TAG_LEN);
    BR_SaveFilters();
}

static void BR_RemoveFilterTag(int index) {
    if (index < 0 || index >= brf.numTags) {
        return;
    }
    memmove(brf.tags[index], brf.tags[index + 1], (brf.numTags - index - 1) * sizeof(brf.tags[0]));
    brf.numTags--;
    BR_SaveFilters();
}

// the page's filterTests: any one that answers true takes the server out
static qboolean BR_Filtered(const brServer_t* s) {
    int i, j;

    if (brf.empty && s->players == 0) {
        return qtrue;
    }
    if (brf.full && s->players >= s->maxPlayers) {
        return qtrue;
    }
    if (brf.ping > 0 && s->ping > brf.ping) {
        return qtrue;
    }
    if (brf.vac && !s->vac) {
        return qtrue;
    }
    if (brf.password && s->password) {
        return qtrue;
    }
    // every tag asked for has to be one of the server's, or its map
    for (i = 0; i < brf.numTags; i++) {
        for (j = 0; j < s->numTags; j++) {
            if (!Q_stricmp(s->tags[j], brf.tags[i])) {
                break;
            }
        }
        if (j == s->numTags && strcmp(s->map, brf.tags[i])) {
            return qtrue;
        }
    }
    return qfalse;
}

// lodash's sortBy: stable, so equals keep the order Steam answered in
static int BR_SortCompare(const void* a, const void* b) {
    const brServer_t* x = &brServers[*(const int*)a];
    const brServer_t* y = &brServers[*(const int*)b];
    int c = 0;

    switch (brf.sort) {
        case BR_SORT_NAME:
            c = strcmp(x->name, y->name);
            break;
        case BR_SORT_GAMETYPE:
            c = strcmp(x->gametype, y->gametype);
            break;
        case BR_SORT_MAP:
            c = strcmp(x->map, y->map);
            break;
        case BR_SORT_PLAYERS:
            c = x->players - y->players;
            break;
        default:
            c = x->ping - y->ping;
            break;
    }
    if (!c) {
        c = *(const int*)a - *(const int*)b;
    }
    return c;
}

static int BR_TagCompare(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

// a server's tags as the page splits them: by comma, trimmed, no empty ones and no repeats
static void BR_ParseTags(brServer_t* s, const char* tags) {
    char buf[MAX_INFO_VALUE];
    char* p;
    int i;

    s->numTags = 0;
    Q_strncpyz(buf, tags, sizeof(buf));
    for (p = strtok(buf, ","); p && s->numTags < BR_MAX_TAGS; p = strtok(NULL, ",")) {
        char* end;

        while (*p == ' ') {
            p++;
        }
        end = p + strlen(p);
        while (end > p && end[-1] == ' ') {
            *--end = '\0';
        }
        if (!p[0]) {
            continue;
        }
        for (i = 0; i < s->numTags; i++) {
            if (!strcmp(s->tags[i], p)) {
                break;
            }
        }
        if (i == s->numTags) {
            Q_strncpyz(s->tags[s->numTags++], p, BR_TAG_LEN);
        }
    }
}

static void BR_AddToTagList(const char* tag) {
    int i;

    for (i = 0; i < brNumTagList; i++) {
        if (!strcmp(brTagList[i], tag)) {
            return;
        }
    }
    if (brNumTagList < BR_MAX_TAGLIST) {
        Q_strncpyz(brTagList[brNumTagList++], tag, BR_TAG_LEN);
    }
}


/*
===============
BR_Collect

What the page does as each server answers (updateServer) and when the filters change:
every server of the engine's list read again, the tags gathered for Common Tags, and the
ones that pass the filters sorted.
===============
*/
static void BR_Collect(void) {
    char info[MAX_STRING_CHARS];
    int count = trap_LAN_GetServerCount(AS_GLOBAL);
    int i, j;

    brNumServers = 0;
    brNumTagList = 0;
    for (i = 0; i < count && brNumServers < MAX_DISPLAY_SERVERS; i++) {
        brServer_t* s = &brServers[brNumServers];

        trap_LAN_GetServerInfo(AS_GLOBAL, i, info, sizeof(info));
        if (!info[0]) {
            continue;
        }
        s->index = i;
        trap_LAN_GetServerAddressString(AS_GLOBAL, i, s->address, sizeof(s->address));
        trap_LAN_ServerName(AS_GLOBAL, i, s->name, sizeof(s->name));
        Q_strncpyz(s->map, Info_ValueForKey(info, "mapname"), sizeof(s->map));
        Q_strncpyz(s->gametype, Info_ValueForKey(info, "gamedesc"), sizeof(s->gametype));
        s->players = atoi(Info_ValueForKey(info, "g_humanplayers"));
        if (s->players < 0) {
            s->players = 0;
        }
        s->maxPlayers = atoi(Info_ValueForKey(info, "sv_maxclients"));
        s->ping = atoi(Info_ValueForKey(info, "ping"));
        s->password = (qboolean)(atoi(Info_ValueForKey(info, "g_needpass")) != 0);
        s->vac = (qboolean)(atoi(Info_ValueForKey(info, "sv_vac")) != 0);
        BR_ParseTags(s, Info_ValueForKey(info, "tags"));
        for (j = 0; j < s->numTags; j++) {
            BR_AddToTagList(s->tags[j]);
        }
        brNumServers++;
    }
    qsort(brTagList, brNumTagList, sizeof(brTagList[0]), BR_TagCompare);

    brNumOrder = 0;
    for (i = 0; i < brNumServers; i++) {
        if (!BR_Filtered(&brServers[i])) {
            brOrder[brNumOrder++] = i;
        }
    }
    qsort(brOrder, brNumOrder, sizeof(brOrder[0]), BR_SortCompare);
    if (brf.reverse) {
        for (i = 0; i < brNumOrder / 2; i++) {
            int t = brOrder[i];

            brOrder[i] = brOrder[brNumOrder - 1 - i];
            brOrder[brNumOrder - 1 - i] = t;
        }
    }
}

/*
===============================================================================

ACTIONS

===============================================================================
*/

// the page's requestNewList: everything forgotten, Steam asked again for this type of list
static void BR_RequestNewList(void) {
    brNumServers = 0;
    brNumOrder = 0;
    br.hasSelected = qfalse;
    br.details = UI_DETAILS_NONE;
    br.scroll = 0;
    UI_Browser_SetSource(brf.type);
    UI_Home_ServerList(qtrue);
}

static qboolean BR_Refreshing(void) {
    return trap_LAN_SteamRefreshing();
}

// the password dialog's tryGameJoin: a match with a password asks for it first
static void BR_Join(const brServer_t* s) {
    if (s->password) {
        br.modal = BR_MODAL_PASSWORD;
        br.modalServer = *s;
        br.password[0] = '\0';
        return;
    }
    trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", s->address));
}

static void BR_ShowDetails(const brServer_t* s) {
    br.hasSelected = qtrue;
    br.selected = *s;
    br.details = UI_DETAILS_LOADING;
    br.rules[0] = '\0';
    br.players[0] = '\0';
    br.playersScroll = 0;
    br.detailsForModal = qfalse;
    if (!trap_LAN_SteamDetails(s->address)) {
        br.details = UI_DETAILS_FAILED;
    }
}

// the game info dialog asks on its own, and again every five seconds while it is open
static void BR_ShowGameInfo(const brServer_t* s) {
    br.modal = BR_MODAL_INFO;
    br.modalServer = *s;
    br.modalDetails = UI_DETAILS_LOADING;
    br.modalRules[0] = '\0';
    br.modalPlayers[0] = '\0';
    br.modalRefresh = uiInfo.uiDC.realTime + 5000;
    br.detailsForModal = qtrue;
    if (!trap_LAN_SteamDetails(s->address)) {
        br.modalDetails = UI_DETAILS_FAILED;
    }
}

// what Steam has answered so far goes to whoever asked
static void BR_PollDetails(void) {
    static char rules[8192];
    static char players[4096];
    int state = trap_LAN_SteamDetailsGet(rules, sizeof(rules), players, sizeof(players));

    if (br.detailsForModal) {
        if (br.modal != BR_MODAL_INFO) {
            return;
        }
        if (state == UI_DETAILS_DONE) {
            Q_strncpyz(br.modalRules, rules, sizeof(br.modalRules));
            br.modalDetails = UI_DETAILS_DONE;
        } else if (state == UI_DETAILS_FAILED && br.modalDetails != UI_DETAILS_DONE) {
            br.modalDetails = UI_DETAILS_FAILED;
        }
        if (players[0]) {
            Q_strncpyz(br.modalPlayers, players, sizeof(br.modalPlayers));
        }
        if (uiInfo.uiDC.realTime > br.modalRefresh) {
            br.modalRefresh = uiInfo.uiDC.realTime + 5000;
            trap_LAN_SteamDetails(br.modalServer.address);
        }
        return;
    }
    if (br.details != UI_DETAILS_LOADING) {
        if (br.details == UI_DETAILS_DONE && players[0] && !br.players[0]) {
            Q_strncpyz(br.players, players, sizeof(br.players));
        }
        return;
    }
    if (state == UI_DETAILS_DONE) {
        Q_strncpyz(br.rules, rules, sizeof(br.rules));
        Q_strncpyz(br.players, players, sizeof(br.players));
        br.details = UI_DETAILS_DONE;
    } else if (state == UI_DETAILS_FAILED) {
        br.details = UI_DETAILS_FAILED;
    }
}

/*
===============================================================================

DRAWING

===============================================================================
*/

static float BR_Width(float size, const char* text, qboolean bold) {
    return SM_AdvanceRaw(FONT_SANS, size, text, -1, bold);
}

static void BR_TextIn(float x, float lineTop, float lineHeight, float size, const float* color, const char* text,
                      qboolean bold, const smRect_t* clip) {
    float baseline = SM_Baseline(FONT_SANS, size, lineTop, lineHeight);

    if (clip && !SM_TextInside(baseline, size, clip)) {
        return;
    }
    SM_TextRaw(x, baseline, FONT_SANS, size, color, text, -1, bold);
}

// text cut to a width, with the page's text-overflow: ellipsis
static void BR_TextFit(float x, float baseline, float size, const float* color, const char* text, float width,
                       qboolean bold, int font) {
    char buf[256];
    int n;

    if (SM_AdvanceRaw(font, size, text, -1, bold) <= width) {
        SM_TextRaw(x, baseline, font, size, color, text, -1, bold);
        return;
    }
    width -= SM_AdvanceRaw(font, size, "...", -1, bold);
    for (n = (int)strlen(text); n > 0 && SM_AdvanceRaw(font, size, text, n, bold) > width; n--) {
    }
    Q_strncpyz(buf, text, n + 1 < (int)sizeof(buf) ? n + 1 : (int)sizeof(buf));
    Q_strcat(buf, sizeof(buf), "...");
    SM_TextRaw(x, baseline, font, size, color, buf, -1, bold);
}

static void BR_Border(float x, float y, float w, float h, float t, const float* color, const smRect_t* clip) {
    SM_Fill(x, y, w, t, color, clip);
    SM_Fill(x, y + h - t, w, t, color, clip);
    SM_Fill(x, y + t, t, h - 2 * t, color, clip);
    SM_Fill(x + w - t, y + t, t, h - 2 * t, color, clip);
}

// skeleton's button: #3d3d3d, a #4d4d4d border, 11px semibold upper case spaced by a pixel
static float BR_ButtonWidth(const char* label) {
    char upper[64];

    Q_strncpyz(upper, label, sizeof(upper));
    Q_strupr(upper);
    return SM_SpacedWidth(FONT_SANS, 11, upper, 1.0f) + 40 + 2;
}

static void BR_Button(float x, float y, const char* label, qboolean disabled, int hit, int index,
                      const smRect_t* clip) {
    char upper[64];
    float w = BR_ButtonWidth(label);
    qboolean over = (qboolean)(!disabled && SM_Over(hit, index));

    Q_strncpyz(upper, label, sizeof(upper));
    Q_strupr(upper);
    SM_Fill(x, y, w, BR_BUTTON_H, disabled ? brDisabled : over ? brButtonHover : brButton, clip);
    BR_Border(x, y, w, BR_BUTTON_H, 1, disabled ? brDisabledBorder : over ? brButtonHoverBorder : brButtonBorder,
              clip);
    {
        float baseline = SM_Baseline(FONT_SANS, 11, y, BR_BUTTON_H);

        if (!clip || SM_TextInside(baseline, 11, clip)) {
            SM_SpacedText(x + 21, baseline, FONT_SANS, 11, disabled ? brDisabledText : over ? brYellow : smText,
                          upper, 1.0f, qtrue);
        }
    }
    if (!disabled) {
        SM_Hit(x, y, w, BR_BUTTON_H, hit, index, clip);
    }
}

// i.tag: #111 behind, a black border, 8pt, three pixels of padding
static float BR_TagWidth(const char* tag) {
    return BR_Width(10.6667f, tag, qfalse) + 6 + 2;
}

static void BR_Tag(float x, float y, const char* tag, const smRect_t* clip) {
    float w = BR_TagWidth(tag);

    SM_Fill(x, y, w, 16, brHead, clip);
    BR_Border(x, y, w, 16, 1, brBlack, clip);
    BR_TextIn(x + 4, y, 16, 10.6667f, brHeadText, tag, qfalse, clip);
}

// a native checkbox, 13 square, and its label (the input's :after content)
static void BR_Check(float x, float y, const char* label, qboolean on, int check, const smRect_t* clip) {
    if (on) {
        SM_Fill(x, y, 13, 13, brCheckOn, clip);
        SM_Icon(x + 1.5f, y + 11, 10, brWhite, SM_FA_CHECK);
    } else {
        SM_Fill(x, y, 13, 13, brWhite, clip);
        BR_Border(x, y, 13, 13, 1, brCheckOff, clip);
    }
    BR_TextIn(x + 20, y - 3, 20, 15, brHeadText, label, qfalse, clip);
    SM_Hit(x, y - 3, 20 + BR_Width(15, label, qfalse), 20, BR_HIT_CHECK, check, clip);
}

// react-select's control: #333 (#444 under the cursor), the value in #aaa and its arrow
static void BR_SelectBox(float x, float y, float w, const char* text, qboolean disabled, int drop,
                         const smRect_t* clip) {
    qboolean over = (qboolean)(!disabled && SM_Over(BR_HIT_SELECT, drop));

    SM_Fill(x, y, w, BR_SELECT_H, disabled ? brDisabled : over ? brSelectHover : brSelect, clip);
    {
        float baseline = SM_Baseline(FONT_SANS, 15, y + 8, 24);

        if (!clip || SM_TextInside(baseline, 15, clip)) {
            BR_TextFit(x + 10, baseline, 15, disabled ? brDisabledText : brSelectText, text, w - 52, qfalse,
                       FONT_SANS);
        }
    }
    if (!clip || (y + 18 >= clip->y && y + 23 <= clip->y + clip->h)) {
        SM_Arrow(x + w - 20, y + 18, br.dropdown == drop);
    }
    if (!disabled) {
        SM_Hit(x, y, w, BR_SELECT_H, BR_HIT_SELECT, drop, clip);
    }
}

// the scrolling list's bar: the webkit one, a #333 thumb in an 8 pixel gutter
static void BR_Scrollbar(float x, float top, float view, float content, float scroll) {
    float thumbH, thumbY;

    if (content <= view) {
        return;
    }
    thumbH = view * view / content;
    if (thumbH < 16) {
        thumbH = 16;
    }
    thumbY = top + (view - thumbH) * scroll / (content - view);
    SM_Hit(x, top, BR_SCROLLBAR, view, BR_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, BR_SCROLLBAR, thumbH,
            (SM_Over(BR_HIT_THUMB, 0) || br.scrollDrag) ? brThumbHover : brThumb, NULL);
    SM_Hit(x, thumbY, BR_SCROLLBAR, thumbH, BR_HIT_THUMB, 0, NULL);
}

static float BR_ClampScroll(float scroll) {
    if (scroll > br.content - br.view) {
        scroll = br.content - br.view;
    }
    return scroll < 0 ? 0 : scroll;
}

// the sort key's arrow after a column's name
static float BR_HeadLabel(float x, float lineTop, const char* label, int sort, const smRect_t* clip) {
    float w = BR_Width(10.6667f, va("%s ", label), qfalse);
    float baseline = SM_Baseline(FONT_SANS, 10.6667f, lineTop, 14);

    if (SM_TextInside(baseline, 10.6667f, clip)) {
        SM_TextRaw(x, baseline, FONT_SANS, 10.6667f, brHeadText, label, -1, qfalse);
        if (brf.sort == sort) {
            SM_Icon(x + w, baseline, 10.6667f, brHeadText, brf.reverse ? SM_FA_SORT_DESC : SM_FA_SORT_ASC);
        }
    }
    if (brf.sort == sort) {
        w += SM_IconWidth(10.6667f, SM_FA_SORT_ASC);
    }
    SM_Hit(x, lineTop, w, 14, BR_HIT_SORT, sort, clip);
    return w;
}

static void BR_DrawFilters(float left, float y, float width, const smRect_t* clip) {
    float colB = left + 10 + (width - 20) * 0.52f;
    float x;
    int i;

    SM_Fill(left, y, width, BR_FILTERS_H, brSection, clip);

    // the first half: the type of list and the two checks under it
    BR_TextIn(left + 10, y + 30, 20, 15, smText, "Type", qfalse, clip);
    BR_SelectBox(left + 10 + BR_Width(15, "Type ", qfalse), y + 10, 150, brTypes[brf.type], BR_Refreshing(),
                 BR_DROP_TYPE, clip);
    BR_Check(left + 10, y + 54, "VAC Enabled", brf.vac, BR_CHECK_VAC, clip);
    BR_Check(left + 10, y + 78, "Hide Passworded Servers", brf.password, BR_CHECK_PASSWORD, clip);

    // the second: empty, full and the ping
    BR_Check(colB, y + 12, "Hide Empty Servers", brf.empty, BR_CHECK_EMPTY, clip);
    BR_Check(colB, y + 36, "Hide Full Servers", brf.full, BR_CHECK_FULL, clip);
    BR_TextIn(colB, y + 78, 20, 15, smText, "Maximum Ping", qfalse, clip);
    for (i = 0; i < (int)ARRAY_LEN(brPings) && brPings[i] != brf.ping; i++) {
    }
    BR_SelectBox(colB + BR_Width(15, "Maximum Ping ", qfalse), y + 58, 100,
                 i < (int)ARRAY_LEN(brPings) && brPings[i] ? va("%i", brPings[i]) : "None", qfalse, BR_DROP_PING,
                 clip);

    // the tags: the ones asked for, the field to add one and the list of the ones seen
    {
        float boxW = width - 20 - 200 - 3.75f;
        float boxY = y + 108;

        SM_Fill(left + 10, boxY, boxW, 32, brField, clip);
        BR_Border(left + 10, boxY, boxW, 32, 1, brFieldBorder, clip);
        SM_Hit(left + 10, boxY, boxW, 32, BR_HIT_TAGBOX, 0, clip);
        x = left + 10 + 4;
        for (i = 0; i < brf.numTags; i++) {
            float w = BR_TagWidth(brf.tags[i]) + 14;

            SM_Fill(x, boxY + 6, w, 18, brHead, clip);
            BR_Border(x, boxY + 6, w, 18, 1, brBlack, clip);
            BR_TextIn(x + 4, boxY + 6, 18, 10.6667f, brHeadText, brf.tags[i], qfalse, clip);
            SM_Icon(x + w - 13, SM_Baseline(FONT_SANS, 10.6667f, boxY + 6, 18), 11, brTagRemove, SM_FA_TIMES_CIRCLE);
            SM_Hit(x + w - 14, boxY + 6, 14, 18, BR_HIT_TAGREMOVE, i, clip);
            x += w + 5;
        }
        if (br.tagInput[0] || br.tagFocus) {
            float baseline = SM_Baseline(FONT_SANS, 15, boxY + 6, 20);

            SM_TextRaw(x + 3.75f, baseline, FONT_SANS, 15, brPlaceholder, br.tagInput, -1, qfalse);
            if (br.tagFocus && (uiInfo.uiDC.realTime / 500) % 2 == 0) {
                SM_Fill(x + 3.75f + BR_Width(15, br.tagInput, qfalse) + 1, boxY + 8, 1, 16, brHeadText, clip);
            }
        } else if (!brf.numTags) {
            BR_TextIn(x + 3.75f, boxY + 6, 20, 15, brPlaceholder, "Add a tag or map name", qfalse, clip);
        }
        BR_SelectBox(left + width - 10 - 200, boxY, 200, "Common Tags", brNumTagList == 0, BR_DROP_TAGS, clip);
    }
}

// the dropdown of a react-select, over everything under it
static void BR_DrawDropdown(float x, float y, float w) {
    int count, i;
    float rowH = br.dropdown == BR_DROP_TAGS ? 24.0f : 40.0f;
    float maxH = br.dropdown == BR_DROP_TAGS ? 400.0f : 200.0f;
    float h, content;
    smRect_t clip;

    count = br.dropdown == BR_DROP_TYPE ? (int)ARRAY_LEN(brTypes)
            : br.dropdown == BR_DROP_PING ? (int)ARRAY_LEN(brPings)
                                           : brNumTagList;
    content = count * rowH;
    h = content < maxH ? content : maxH;
    if (br.dropScroll > content - h) {
        br.dropScroll = content - h;
    }
    if (br.dropScroll < 0) {
        br.dropScroll = 0;
    }
    y -= 1;  // margin-top: -1px
    SM_Fill(x, y, w, h, brSelect, NULL);
    clip = SM_R(x, y, w, h);
    for (i = 0; i < count; i++) {
        float ry = y + i * rowH - br.dropScroll;
        const char* label;

        if (ry + rowH < y || ry > y + h) {
            continue;
        }
        label = br.dropdown == BR_DROP_TYPE ? brTypes[i]
                : br.dropdown == BR_DROP_PING ? (brPings[i] ? va("%i", brPings[i]) : "None")
                                               : brTagList[i];
        if (SM_Over(BR_HIT_OPTION, i)) {
            SM_Fill(x, ry, w, rowH, brOptionFocus, &clip);
        }
        BR_TextIn(x + 10, ry + (rowH - 24) * 0.5f, 24, 15, SM_Over(BR_HIT_OPTION, i) ? brRowText : brOption, label,
                  qfalse, &clip);
        SM_Hit(x, ry, w, rowH, BR_HIT_OPTION, i, &clip);
    }
}

/*
===============
BR_DrawRow

One of serverlist's rows: the map's shot, which turns into a play button under the cursor,
the name with a lock when there is a password, the gametype and the arena, the tags, the
players and the ping.
===============
*/
static void BR_DrawRow(const brServer_t* s, int row, float x, float y, float w, const smRect_t* clip) {
    qboolean over = SM_Over(BR_HIT_ROW, row) || SM_Over(BR_HIT_PLAY, row) || SM_Over(BR_HIT_TAG, row);
    qboolean selected = (qboolean)(br.hasSelected && !strcmp(br.selected.address, s->address));
    float mainW = w * 0.8f;
    float colW = w * 0.1f;
    float tx, tagsW, tagsX;
    char text[64];
    int i;

    SM_Fill(x, y, w, BR_ROW_H, selected ? brRowSelected : over ? brRowHover : brBlack80, clip);
    SM_Hit(x, y, w, BR_ROW_H, BR_HIT_ROW, row, clip);

    // the shot, its width filling 79 pixels and cut at 40
    {
        smRect_t shot = SM_R(x + 5, y + 5, 79, 40);

        if (clip) {
            SM_Clip(&shot, clip);
        }
        SM_Pic(x + 5, y + 5, 79, 79 * 270.0f / 480.0f, BR_Shot("sm", s->map), &shot);
        if (over) {
            SM_Fill(x + 5, y + 5, 79, 40, SM_Over(BR_HIT_PLAY, row) ? brPlayHover : brBlack80, &shot);
            if (SM_TextInside(y + 35, 28, clip)) {
                SM_Icon(x + 5 + 28, y + 35, 28, brYellow, SM_FA_CHEVRON_CIRCLE_RIGHT);
            }
            SM_Hit(x + 5, y + 5, 79, 40, BR_HIT_PLAY, row, &shot);
        }
    }

    // the name, with the lock of a match that asks for a password
    tx = x + 89;
    if (s->password) {
        float baseline = SM_Baseline(FONT_SANS, 14.6667f, y, 23.4667f);

        if (SM_TextInside(baseline, 14.6667f, clip)) {
            SM_Icon(tx, baseline, 14.6667f, brYellow, SM_FA_LOCK);
        }
        tx += SM_IconWidth(14.6667f, SM_FA_LOCK) + BR_Width(14.6667f, " ", qtrue);
    }
    {
        float baseline = SM_Baseline(FONT_SANS, 14.6667f, y, 23.4667f);

        if (SM_TextInside(baseline, 14.6667f, clip)) {
            BR_TextFit(tx, baseline, 14.6667f, brRowText, s->name, x + mainW - tx - 5, qtrue, FONT_SANS);
        }
    }

    // the tags, to the right of the first column and never wider than 40% of it
    tagsW = 0;
    for (i = 0; i < s->numTags; i++) {
        tagsW += BR_TagWidth(s->tags[i]) + 6;
    }
    if (tagsW > mainW * 0.4f) {
        tagsW = mainW * 0.4f;
    }
    tagsX = x + mainW - 5 - tagsW;
    {
        smRect_t tagClip = SM_R(tagsX, y, tagsW, BR_ROW_H);
        float t = tagsX + 3;

        if (clip) {
            SM_Clip(&tagClip, clip);
        }
        for (i = 0; i < s->numTags && t < tagsX + tagsW; i++) {
            float tw = BR_TagWidth(s->tags[i]);

            if (t + tw <= tagsX + tagsW) {
                BR_Tag(t, y + 32.4f, s->tags[i], &tagClip);
            } else {
                // overflow: hidden and text-overflow: ellipsis, the tag cut where the box ends
                float baseline = SM_Baseline(FONT_SANS, 10.6667f, y + 32.4f, 16);

                SM_Fill(t, y + 32.4f, tw, 16, brHead, &tagClip);
                BR_Border(t, y + 32.4f, tw, 16, 1, brBlack, &tagClip);
                if (SM_TextInside(baseline, 10.6667f, clip)) {
                    BR_TextFit(t + 4, baseline, 10.6667f, brHeadText, s->tags[i], tagsX + tagsW - t - 4, qfalse,
                               FONT_SANS);
                }
            }
            SM_Hit(t, y + 32.4f, tw, 16, BR_HIT_TAG, row * BR_MAX_TAGS + i, &tagClip);
            t += tw + 6;
        }
    }

    // the gametype and the arena, what serverdesc says under the name
    Com_sprintf(text, sizeof(text), "%s: %s", s->gametype, BR_MapName(s->map));
    {
        float baseline = SM_Baseline(FONT_SANS, 12, y + 26.4f, 19.2f);

        if (SM_TextInside(baseline, 12, clip)) {
            BR_TextFit(x + 89, baseline, 12, brDesc, text, tagsX - x - 89 - 4, qfalse, FONT_SANS);
        }
    }

    // players and ping, in the middle of their columns
    Com_sprintf(text, sizeof(text), "%i / %i", s->players, s->maxPlayers);
    BR_TextIn(x + mainW + (colW - BR_Width(13.3333f, text, qfalse)) * 0.5f, y + (BR_ROW_H - 21.3333f) * 0.5f,
              21.3333f, 13.3333f, brRowText, text, qfalse, clip);
    if (s->ping > 0) {
        float pw = 16 + BR_Width(13.3333f, va(" %i", s->ping), qfalse);
        float px = x + mainW + colW + (colW - pw) * 0.5f;

        SM_Pic(px, y + 17.5f, 16, 16, BR_Quality(s->ping), clip);
        BR_TextIn(px + 16, y + (BR_ROW_H - 21.3333f) * 0.5f, 21.3333f, 13.3333f, brPingText, va(" %i", s->ping),
                  qfalse, clip);
    }
}

/*
===============
BR_DrawList

The left column, which scrolls as a whole: the title, the actions, the filters when they
are shown, the table's head and the rows.
===============
*/
static void BR_DrawList(float left, float top, float width, float height) {
    float inner = width - BR_SCROLLBAR;
    float y = top - br.scroll;
    float sectionY, tableY, headY, rowsY;
    smRect_t clip = SM_R(left, top, inner, height);
    int i;

    // the title
    SM_Fill(left, y, inner, BR_TITLE_H, brBlack80, &clip);
    {
        float baseline = SM_Baseline(FONT_DEFAULT, 32.0f, y + 8.0f, 38.4f);

        if (SM_TextInside(baseline, 32, &clip)) {
            SM_TextRaw(left + 6.4f, baseline, FONT_DEFAULT, 32.0f, smText, "Match Browser", -1, qfalse);
        }
    }

    // the actions: the filters on the left, the two refreshes on the right
    sectionY = y + BR_TITLE_H + 2;
    SM_Fill(left, sectionY, inner, BR_SECTION_H, brSection, &clip);
    {
        qboolean over = SM_Over(BR_HIT_TOGGLE, 0);
        float baseline = SM_Baseline(FONT_SANS, 15, sectionY + 10, 24);
        float tw = SM_IconWidth(15, SM_FA_FILTER_ICON) + BR_Width(15, " Change Filters", qfalse);

        if (SM_TextInside(baseline, 15, &clip)) {
            SM_Icon(left + 15, baseline, 15, over ? brLinkHover : brLink, SM_FA_FILTER_ICON);
            SM_TextRaw(left + 15 + SM_IconWidth(15, SM_FA_FILTER_ICON), baseline, FONT_SANS, 15,
                    over ? brLinkHover : brLink, " Change Filters", -1, qfalse);
        }
        SM_Hit(left + 10, sectionY + 10, tw + 10, 24, BR_HIT_TOGGLE, 0, &clip);
    }
    {
        float quickW = BR_ButtonWidth("Quick Refresh");
        float allW = BR_ButtonWidth("Refresh All");
        float quickX = left + inner - 20 - quickW;
        qboolean busy = BR_Refreshing();

        BR_Button(quickX - 10 - allW, sectionY + 10, "Refresh All", busy, BR_HIT_REFRESH_ALL, 0, &clip);
        BR_Button(quickX, sectionY + 10, "Quick Refresh", busy, BR_HIT_QUICK_REFRESH, 0, &clip);
    }

    tableY = sectionY + BR_SECTION_H;
    if (br.showFilters) {
        BR_DrawFilters(left, tableY, inner, &clip);
        tableY += BR_FILTERS_H;
    }

    // the head, whose names sort
    headY = tableY + 1;
    SM_Fill(left, headY, inner, BR_HEAD_H, brHead, &clip);
    {
        float x = left + 3;
        float lineTop = headY + 1;

        x += BR_HeadLabel(x, lineTop, "Match Name", BR_SORT_NAME, &clip);
        BR_TextIn(x, lineTop, 14, 10.6667f, brHeadText, ",", qfalse, &clip);
        x += BR_Width(10.6667f, ",", qfalse) + 8;
        x += BR_HeadLabel(x, lineTop, "Gametype", BR_SORT_GAMETYPE, &clip);
        BR_TextIn(x, lineTop, 14, 10.6667f, brHeadText, ",", qfalse, &clip);
        x += BR_Width(10.6667f, ",", qfalse) + 8;
        BR_HeadLabel(x, lineTop, "Arena", BR_SORT_MAP, &clip);

        for (i = 0; i < 2; i++) {
            const char* label = i ? "Ping" : "Players";
            int sort = i ? BR_SORT_PING : BR_SORT_PLAYERS;
            float colX = left + inner * (i ? 0.9f : 0.8f);
            float w = BR_Width(10.6667f, va("%s ", label), qfalse) +
                      (brf.sort == sort ? SM_IconWidth(10.6667f, SM_FA_SORT_ASC) : 0);

            BR_HeadLabel(colX + 3 + (inner * 0.1f - 3 - w) * 0.5f, lineTop, label, sort, &clip);
        }
    }

    // the rows
    rowsY = headY + BR_HEAD_H + 1;
    br.shownRows = brNumOrder;
    for (i = 0; i < brNumOrder; i++) {
        float ry = rowsY + i * BR_ROW_STEP;

        if (ry + BR_ROW_H < top || ry > top + height) {
            continue;
        }
        BR_DrawRow(&brServers[brOrder[i]], i, left, ry, inner, &clip);
    }

    br.view = height;
    br.content = (rowsY + brNumOrder * BR_ROW_STEP) - (top - br.scroll);
    BR_Scrollbar(left + inner, top, height, br.content, br.scroll);

    // an open react-select goes over the rows, inside the same column
    if (br.dropdown != BR_DROP_NONE && br.showFilters) {
        float fy = sectionY + BR_SECTION_H;
        float colB = left + 10 + (inner - 20) * 0.52f;

        if (br.dropdown == BR_DROP_TYPE) {
            BR_DrawDropdown(left + 10 + BR_Width(15, "Type ", qfalse), fy + 10 + BR_SELECT_H, 150);
        } else if (br.dropdown == BR_DROP_PING) {
            BR_DrawDropdown(colB + BR_Width(15, "Maximum Ping ", qfalse), fy + 58 + BR_SELECT_H, 100);
        } else {
            BR_DrawDropdown(left + inner - 10 - 200, fy + 108 + BR_SELECT_H, 200);
        }
    }
}

// halogen's ClipLoader: a 24 pixel ring with its bottom quarter missing, turning every 0.75s
static void BR_Loader(float x, float y) {
    float cx = x + 12, cy = y + 12;
    float turn = (uiInfo.uiDC.realTime % 750) / 750.0f * 2 * M_PI;
    int i;

    for (i = 0; i < 36; i++) {
        float a = i * (2 * M_PI / 36);
        float rel = a - turn;

        rel = rel - 2 * M_PI * floor(rel / (2 * M_PI));
        // the transparent bottom border is the quarter around straight down
        if (rel > M_PI * 0.25f && rel < M_PI * 0.75f) {
            continue;
        }
        SM_Fill(cx + cos(a) * 10 - 2, cy + sin(a) * 10 - 2, 4, 4, brYellow, NULL);
    }
}

// the page's stripColors: ^ and a digit out, then any ^ left
static void BR_StripColors(const char* in, char* out, int size) {
    int n = 0;

    for (; *in && n < size - 1; in++) {
        if (in[0] == '^' && in[1] >= '0' && in[1] <= '9') {
            in++;
            continue;
        }
        if (*in == '^') {
            continue;
        }
        out[n++] = *in;
    }
    out[n] = '\0';
}

// moment-duration-format's "d[d] h:mm:ss", the zero units on the left trimmed
static void BR_Duration(int seconds, char* out, int size) {
    int d = seconds / 86400, h = (seconds / 3600) % 24, m = (seconds / 60) % 60, s = seconds % 60;

    if (d) {
        Com_sprintf(out, size, "%id %i:%02i:%02i", d, h, m, s);
    } else if (h) {
        Com_sprintf(out, size, "%i:%02i:%02i", h, m, s);
    } else if (m) {
        Com_sprintf(out, size, "%02i:%02i", m, s);
    } else {
        Com_sprintf(out, size, "%02i", s);
    }
}

// Utils.generateNiceServerDetails: the rules the page puts in words, in its order
static int BR_NiceDetails(const char* rules, char out[4][64]) {
    int gametype = atoi(Info_ValueForKey(rules, "g_gametype"));
    qboolean known = (qboolean)(gametype >= 0 && gametype < (int)ARRAY_LEN(brGametypes));
    qboolean team = known && brGametypes[gametype].team;
    const char* condition = known ? brGametypes[gametype].condition : NULL;
    const char* v;
    int n = 0;

    Com_sprintf(out[n++], 64, "Game State: %s",
                !Q_stricmp(Info_ValueForKey(rules, "g_gamestate"), "IN_PROGRESS") ? "In Progress" : "Warmup");
    v = Info_ValueForKey(rules, "teamsize");
    Com_sprintf(out[n++], 64, "%s: %s", team ? "Team Size" : "Match Size", atoi(v) > 0 ? v : "Unlimited");
    if (condition && Info_ValueForKey(rules, condition)[0]) {
        const char* name = !strcmp(condition, "capturelimit") ? "Capture Limit"
                           : !strcmp(condition, "roundlimit")  ? "Round Limit"
                           : !strcmp(condition, "scorelimit")  ? "Score Limit"
                                                               : "Frag Limit";

        v = Info_ValueForKey(rules, condition);
        Com_sprintf(out[n++], 64, "%s: %s", name, atof(v) != 0 ? v : "Unlimited");
    }
    v = Info_ValueForKey(rules, "timelimit");
    Com_sprintf(out[n++], 64, "Time Limit: %s", atoi(v) > 0 ? v : "Unlimited");
    return n;
}

// one line of the players: the name without colours and the time on the server
static const char* BR_NextPlayer(const char* p, char* line, int size) {
    char name[64];
    const char* tab;
    int seconds;
    char time[32];

    if (!p || !*p) {
        return NULL;
    }
    tab = strchr(p, '\t');
    if (!tab) {
        return NULL;
    }
    Q_strncpyz(name, p, (int)(tab - p) + 1 < (int)sizeof(name) ? (int)(tab - p) + 1 : (int)sizeof(name));
    tab = strchr(tab + 1, '\t');
    seconds = tab ? atoi(tab + 1) : 0;
    BR_StripColors(name, name, sizeof(name));
    BR_Duration(seconds, time, sizeof(time));
    Com_sprintf(line, size, "%s (Time: %s)", name, time);
    p = strchr(p, '\n');
    return p ? p + 1 : NULL;
}

// the scorebox of a team match: red on the left, aligned right, and blue on the right
static float BR_Scorebox(float x, float y, float w, const char* rules) {
    float h = 18.6667f * 1.6f + 2 * 4.6667f;
    float baseline = SM_Baseline(FONT_DEFAULT, 18.6667f, y + 4.6667f, 18.6667f * 1.6f);
    const char* red = Info_ValueForKey(rules, "g_redScore");
    const char* blue = Info_ValueForKey(rules, "g_blueScore");
    float half = w * 0.5f;
    float bx;

    SM_Fill(x, y, half, h, brBlack75, NULL);
    SM_Fill(x + half, y, w - half, h, brBlack75, NULL);
    // "Red " and a 30 pixel box with the score, all against the right edge
    bx = x + half - 14 - 30;
    SM_TextRaw(bx + 30 - SM_AdvanceBold(FONT_DEFAULT, 18.6667f, red, -1, qtrue), baseline, FONT_DEFAULT, 18.6667f, brRed,
            red, -1, qtrue);
    SM_TextRaw(bx - SM_Advance(FONT_DEFAULT, 18.6667f, "Red ", -1), baseline, FONT_DEFAULT, 18.6667f, brRed, "Red ", -1,
            qfalse);
    bx = x + half + 14;
    SM_TextRaw(bx, baseline, FONT_DEFAULT, 18.6667f, brBlue, blue, -1, qtrue);
    SM_TextRaw(bx + 30, baseline, FONT_DEFAULT, 18.6667f, brBlue, " Blue", -1, qfalse);
    return h;
}

/*
===============
BR_DrawDetails

serverlistdetails: the match's shot with its name over a shade, the score of a team match,
the button that joins, what the rules say and who is playing.
===============
*/
static void BR_DrawDetails(float x, float top, float w) {
    const brServer_t* s = &br.selected;
    float y = top;
    char text[128];

    if (!br.hasSelected) {
        return;
    }
    if (br.details == UI_DETAILS_FAILED) {
        BR_TextIn(x, top, 24, 15, smText, "Failed to retrieve server details.", qfalse, NULL);
        return;
    }
    if (br.details == UI_DETAILS_LOADING) {
        BR_Loader(x, top);
        return;
    }

    // the shot, covering the box from its top edge, and the shade under the name
    {
        smRect_t box = SM_R(x, y, w, BR_DETAILS_MAP_H);
        float shotW = w, shotH = w * 270.0f / 480.0f;
        int i;

        if (shotH < BR_DETAILS_MAP_H) {
            shotH = BR_DETAILS_MAP_H;
            shotW = shotH * 480.0f / 270.0f;
        }
        SM_Fill(x, y, w, BR_DETAILS_MAP_H, brHead, NULL);
        SM_Pic(x, y, shotW, shotH, BR_Shot("sm", s->map), &box);
        for (i = 0; i < 93; i += 3) {
            float f = (i + 1.5f) / 93.0f;
            vec4_t shade = {0, 0, 0, f < 0.45f ? 0.8f * f / 0.45f : 0.8f};

            SM_Fill(x, y + BR_DETAILS_MAP_H - 93 + i, w, 3, shade, NULL);
        }
        BR_TextFit(x + 7.5f, SM_Baseline(FONT_DEFAULT, 24, y + BR_DETAILS_MAP_H - 93 + 7.5f, 28.8f), 24, brWhite,
                   s->name, w - 15, qfalse, FONT_DEFAULT);
        Com_sprintf(text, sizeof(text), "%s: %s", s->gametype, BR_MapName(s->map));
        BR_TextFit(x + 7.5f, SM_Baseline(FONT_SANS, 18.6667f, y + BR_DETAILS_MAP_H - 93 + 36.3f, 25.2f), 18.6667f,
                   brWhite, text, w - 15, qfalse, FONT_SANS);
        Com_sprintf(text, sizeof(text), "%i / %i players", s->players, s->maxPlayers);
        BR_TextIn(x + 7.5f, y + BR_DETAILS_MAP_H - 93 + 61.5f, 24, 15, brWhite, text, qfalse, NULL);
        y += BR_DETAILS_MAP_H;
    }

    {
        int gametype = atoi(Info_ValueForKey(br.rules, "g_gametype"));

        if (gametype >= 0 && gametype < (int)ARRAY_LEN(brGametypes) && brGametypes[gametype].team) {
            y += BR_Scorebox(x, y, w, br.rules);
        }
    }

    // >> JOIN MATCH <<
    {
        float h = 44.7f;
        qboolean over = SM_Over(BR_HIT_JOIN, 0);
        float iw = SM_IconWidth(21.3333f, SM_FA_ANGLE_DOUBLE_RIGHT);
        float tw = SM_SpacedWidth(FONT_DEFAULT, 21.3333f, " JOIN MATCH ", 1.0f);
        float bx = x + (w - (iw * 2 + tw)) * 0.5f;
        float baseline = SM_Baseline(FONT_DEFAULT, 21.3333f, y, h);

        SM_Fill(x, y, w, h, over ? brPlayHover : brBlack80, NULL);
        BR_Border(x, y, w, h, 2, brYellow, NULL);
        SM_Icon(bx, baseline, 21.3333f, brYellow, SM_FA_ANGLE_DOUBLE_RIGHT);
        SM_SpacedText(bx + iw, baseline, FONT_DEFAULT, 21.3333f, brYellow, " JOIN MATCH ", 1.0f, qfalse);
        SM_Icon(bx + iw + tw, baseline, 21.3333f, brYellow, SM_FA_ANGLE_DOUBLE_LEFT);
        SM_Hit(x, y, w, h, BR_HIT_JOIN, 0, NULL);
        y += h + 7.5f;
    }

    // the rules in words
    {
        char lines[4][64];
        int n = BR_NiceDetails(br.rules, lines);
        float h = 30 + n * 24;
        int i;

        SM_Fill(x, y, w, h, brBlack75, NULL);
        for (i = 0; i < n; i++) {
            BR_TextIn(x + 22.5f, y + 15 + i * 24, 24, 15, brInfo, lines[i], qfalse, NULL);
        }
        y += h + 7.5f;
    }

    // who is playing, in a box of its own that scrolls past 300 pixels
    if (br.players[0]) {
        const char* p = br.players;
        char line[128];
        float content, h;
        smRect_t clip;
        int n = 0;

        for (; *p; p++) {
            if (*p == '\n') {
                n++;
            }
        }
        content = 30 + n * 27.75f;
        h = content < 300 ? content : 300;
        if (br.playersScroll > content - h) {
            br.playersScroll = content - h;
        }
        if (br.playersScroll < 0) {
            br.playersScroll = 0;
        }
        SM_Fill(x, y, w, h, brBlack75, NULL);
        SM_Hit(x, y, w, h, BR_HIT_PLAYERS, 0, NULL);
        clip = SM_R(x, y, w, h);
        p = br.players;
        n = 0;
        while (p && *p) {
            const char* next = BR_NextPlayer(p, line, sizeof(line));

            BR_TextIn(x + 22.5f, y + 15 + n * 27.75f - br.playersScroll, 24, 15, brInfo, line, qfalse, &clip);
            n++;
            p = next;
        }
    }
}

// the context menu of a row (showServerContext): white, black, yellow under the cursor
static const char* BR_MenuItem(int i) {
    if (i == 0) {
        return "View Game Info";
    }
    return brf.type == 3 ? "Remove Favorite" : "Add Favorite";
}

static void BR_DrawMenu(void) {
    float w = 0, h = 2 * (15 * 1.6f + 7.5f);
    int i;

    SM_Fill(0, 0, smVW, smBottom, brOverlay20, NULL);
    SM_Hit(0, 0, smVW, smBottom, BR_HIT_OVERLAY, 0, NULL);
    for (i = 0; i < 2; i++) {
        float iw = BR_Width(15, BR_MenuItem(i), qfalse) + 30;

        if (iw > w) {
            w = iw;
        }
    }
    SM_Fill(br.menuX, br.menuY, w, h, brWhite, NULL);
    for (i = 0; i < 2; i++) {
        float iy = br.menuY + i * (15 * 1.6f + 7.5f);

        if (SM_Over(BR_HIT_MENU_ITEM, i)) {
            SM_Fill(br.menuX, iy, w, 15 * 1.6f + 7.5f, brYellow, NULL);
        }
        BR_TextIn(br.menuX + 15, iy + 3.75f, 24, 15, brBlack, BR_MenuItem(i), qfalse, NULL);
        SM_Hit(br.menuX, iy, w, 15 * 1.6f + 7.5f, BR_HIT_MENU_ITEM, i, NULL);
    }
}

/*
===============
BR_DrawGameInfo

The game info dialog, for a screen too narrow for the details column and for the menu's
"View Game Info": the whole width of the screen, 600 pixels high over the arena's large
shot, and everything the column shows plus the tags and the ping.
===============
*/
static void BR_DrawGameInfo(void) {
    const brServer_t* s = &br.modalServer;
    float top = smBottom * 0.5f - 300;
    float x = 10 + 15, y = top + 10 + 15;
    float w = smVW - 20 - 30;
    char text[128];

    SM_Fill(0, 0, smVW, smBottom, brBlack80, NULL);
    SM_Hit(0, 0, smVW, smBottom, BR_HIT_DIALOG, 0, NULL);
    SM_Fill(0, top, smVW, 620, brDialog, NULL);
    if (br.modalDetails == UI_DETAILS_DONE) {
        smRect_t box = SM_R(0, top, smVW, 620);
        float shotH = smVW * 1080.0f / 1920.0f;

        SM_Pic(0, top + 310 - shotH * 0.5f, smVW, shotH, BR_Shot("lg", s->map), &box);
    }
    SM_Fill(10, top + 10, smVW - 20, 600, brBlack80, NULL);

    if (br.modalDetails != UI_DETAILS_DONE) {
        BR_Loader(x, y);
        BR_Button(x + 24, y, "Close", qfalse, BR_HIT_MODAL_CLOSE, 0, NULL);
        return;
    }

    // the close cross, and the name and what is played where
    SM_TextRaw(x + w - SM_Advance(FONT_SANS, 32, "\xc3\x97", -1), SM_Baseline(FONT_SANS, 32, y, 18.6667f), FONT_SANS,
            32, brYellow, "\xc3\x97", -1, qfalse);
    SM_Hit(x + w - 24, y - 8, 24, 32, BR_HIT_MODAL_CLOSE, 0, NULL);
    BR_TextFit(x, SM_Baseline(FONT_DEFAULT, 48, y, 57.6f), 48, smText, s->name, w - 40, qfalse, FONT_DEFAULT);
    y += 57.6f;
    Com_sprintf(text, sizeof(text), "%s on %s", s->gametype, BR_MapName(s->map));
    BR_TextIn(x, y, 39.6f, 29.3333f, smText, text, qfalse, NULL);
    y += 39.6f;
    {
        float t = x;
        int i;

        for (i = 0; i < s->numTags; i++) {
            BR_Tag(t + 3, y + 4, s->tags[i], NULL);
            t += BR_TagWidth(s->tags[i]) + 6;
        }
        y += 24 + 15;
    }
    if (s->ping > 0) {
        SM_Pic(x, y + 4, 16, 16, BR_Quality(s->ping), NULL);
        BR_TextIn(x + 16, y, 24, 15, smText, va(" %ims", s->ping), qfalse, NULL);
    }
    Com_sprintf(text, sizeof(text), "%i / %i players", s->players, s->maxPlayers);
    BR_TextIn(x + 75 + BR_Width(15, " ", qfalse), y, 24, 15, smText, text, qfalse, NULL);
    y += 24 + 30 + 15;
    {
        int gametype = atoi(Info_ValueForKey(br.modalRules, "g_gametype"));

        if (gametype >= 0 && gametype < (int)ARRAY_LEN(brGametypes) && brGametypes[gametype].team) {
            y += BR_Scorebox(x, y, w, br.modalRules);
        }
    }

    // the rules on the left, the players on the right
    {
        char lines[4][64];
        int n = BR_NiceDetails(br.modalRules, lines);
        float half = w * 0.48f;
        const char* p = br.modalPlayers;
        char line[128];
        smRect_t clip = SM_R(x + w * 0.52f, y, half, 350);
        int i;

        for (i = 0; i < n; i++) {
            char* colon = strchr(lines[i], ':');

            if (colon) {
                *colon = '\0';
                SM_TextRaw(x, SM_Baseline(FONT_SANS, 15, y + i * 34, 24), FONT_SANS, 15, smText, lines[i], -1, qtrue);
                SM_TextRaw(x + 150 + BR_Width(15, " ", qfalse), SM_Baseline(FONT_SANS, 15, y + i * 34, 24), FONT_SANS, 15,
                        smText, colon + 2, -1, qfalse);
            }
        }
        for (i = 0; p && *p; i++) {
            const char* next = BR_NextPlayer(p, line, sizeof(line));

            BR_TextIn(clip.x, y + i * 34, 24, 15, smText, line, qfalse, &clip);
            p = next;
        }
    }

    // the one button, at the bottom right
    BR_Button(x + w - BR_ButtonWidth("Join Match"), top + 10 + 600 - 15 - BR_BUTTON_H, "Join Match", qfalse,
              BR_HIT_MODAL_JOIN, 0, NULL);
}

// the password dialog: a thin band in the middle of the screen with the field and its buttons
static void BR_DrawPassword(void) {
    float top = smBottom * 0.5f - 150;
    float x = 10 + 15, y = top + 10 + 15;
    float w = smVW - 20 - 30;
    float by = top + 10 + 300 - 15 - BR_BUTTON_H;
    float okW = BR_ButtonWidth("OK");

    SM_Fill(0, 0, smVW, smBottom, brBlack80, NULL);
    SM_Hit(0, 0, smVW, smBottom, BR_HIT_DIALOG, 0, NULL);
    SM_Fill(0, top, smVW, 320, brDialog, NULL);
    SM_Fill(10, top + 10, smVW - 20, 300, brBlack80, NULL);
    SM_TextRaw(x + 6.4f, SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), FONT_DEFAULT, 32, smText, "Match Requires Password",
            -1, qfalse);
    y += 8 + 38.4f + 8;
    BR_TextIn(x, y, 24, 15, smText, "This match is password protected. Enter password to continue.", qfalse, NULL);
    y += 24 + 15 + 24;
    SM_Fill(x, y, 200, 28, brField, NULL);
    BR_Border(x, y, 200, 28, 1, brFieldBorder, NULL);
    BR_TextIn(x + 6, y + 2, 24, 15, smText, br.password, qfalse, NULL);
    if ((uiInfo.uiDC.realTime / 500) % 2 == 0) {
        SM_Fill(x + 6 + BR_Width(15, br.password, qfalse) + 1, y + 6, 1, 16, smText, NULL);
    }
    SM_Hit(x, y, 200, 28, BR_HIT_PASS_FIELD, 0, NULL);
    BR_Button(x + w - okW, by, "OK", qfalse, BR_HIT_PASS_OK, 0, NULL);
    BR_Button(x + w - okW - BR_Width(15, " ", qfalse) - BR_ButtonWidth("Cancel"), by, "Cancel", qfalse,
              BR_HIT_PASS_CANCEL, 0, NULL);
}

// the details column only fits a screen wider than 3:2 (the page's max-aspect-ratio: 9/6)
static qboolean BR_Wide(void) {
    return (qboolean)(uiInfo.uiDC.glconfig.vidWidth * 6 > uiInfo.uiDC.glconfig.vidHeight * 9);
}

static void BR_DrawPage(void) {
    float left = UI_Home_Left();
    float width = HM_CONTAINER_W;
    float top = HM_NAV_H;
    float bottom = UI_Home_FrameBottom();
    qboolean wide = BR_Wide();

    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_BROWSER);

    BR_DrawList(left, top, wide ? width * 0.67f : width, bottom - top);
    if (wide) {
        BR_DrawDetails(left + width * 0.68f, top, width * 0.32f);
    }
}

void UI_Browser_Open(void) {
    if (!br.loaded) {
        br.loaded = qtrue;
        BR_LoadFilters();
    }
    br.scroll = 0;
    br.scrollDrag = qfalse;
    br.dropdown = BR_DROP_NONE;
    br.tagFocus = qfalse;
    br.menuOpen = qfalse;
    br.modal = BR_MODAL_NONE;
    br.lastBuild = 0;
    // the list is the global one, which is what Steam gives. The cached value is updated by
    // hand: UI_UpdateCvars does not run until the next frame and the refresh goes out now.
    trap_Cvar_Set("ui_netSource", "2");  // AS_GLOBAL
    trap_Cvar_Update(&ui_netSource);
    // the page asks for a new list only when it has none (componentDidMount)
    UI_Browser_SetSource(brf.type);
    if (trap_LAN_GetServerCount(AS_GLOBAL) <= 0) {
        BR_RequestNewList();
    } else {
        UI_Home_ServerList(qfalse);
    }
}

void UI_Browser_Paint(void) {
    SM_BeginFrame();

    // the list grows while Steam answers; the page batches its updates every 250 ms
    if (uiInfo.uiDC.realTime - br.lastBuild > 250) {
        br.lastBuild = uiInfo.uiDC.realTime;
        UI_Home_ServerList(qfalse);
        BR_Collect();
    }
    BR_PollDetails();

    if (br.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1)) {
            br.scrollDrag = qfalse;
        } else if (br.content > br.view) {
            float thumbH = br.view * br.view / br.content;

            if (thumbH < 16) {
                thumbH = 16;
            }
            br.scroll =
                BR_ClampScroll(br.scroll + (smMY - br.scrollGrab) * (br.content - br.view) / (br.view - thumbH));
            br.scrollGrab = smMY;
        }
    }
    br.scroll = BR_ClampScroll(br.scroll);

    BR_DrawPage();
    UI_Home_Banner();
    if (br.menuOpen) {
        BR_DrawMenu();
    }
    if (br.modal == BR_MODAL_INFO) {
        BR_DrawGameInfo();
    } else if (br.modal == BR_MODAL_PASSWORD) {
        BR_DrawPassword();
    }
    SM_EndFrame();
}

/*
===============================================================================

INPUT

===============================================================================
*/

static void BR_SetCheck(int check) {
    switch (check) {
        case BR_CHECK_VAC:
            brf.vac = (qboolean)!brf.vac;
            break;
        case BR_CHECK_PASSWORD:
            brf.password = (qboolean)!brf.password;
            break;
        case BR_CHECK_EMPTY:
            brf.empty = (qboolean)!brf.empty;
            break;
        default:
            brf.full = (qboolean)!brf.full;
            break;
    }
    BR_SaveFilters();
    BR_Collect();
}

static void BR_ChooseOption(int option) {
    if (br.dropdown == BR_DROP_TYPE) {
        if (option != brf.type) {
            brf.type = option;
            BR_SaveFilters();
            BR_RequestNewList();  // a new type is a new list (updateFilters)
        }
    } else if (br.dropdown == BR_DROP_PING) {
        brf.ping = brPings[option];
        BR_SaveFilters();
    } else if (option < brNumTagList) {
        BR_AddFilterTag(brTagList[option]);
    }
    br.dropdown = BR_DROP_NONE;
    BR_Collect();
}

// the sortClicked of the list's head: the same column again reverses it, and players go
// from the most first
static void BR_Sort(int sort) {
    if (sort == brf.sort) {
        brf.reverse = (qboolean)!brf.reverse;
    } else {
        brf.sort = sort;
        brf.reverse = (qboolean)(sort == BR_SORT_PLAYERS);
    }
    BR_SaveFilters();
    BR_Collect();
}

static const brServer_t* BR_RowServer(int row) {
    if (row < 0 || row >= brNumOrder) {
        return NULL;
    }
    return &brServers[brOrder[row]];
}

// react-tagsinput: Tab, Enter, space and comma add what was typed; a backspace in an empty
// field takes the last tag away
static qboolean BR_TagKey(int key) {
    int len = (int)strlen(br.tagInput);

    if (key & K_CHAR_FLAG) {
        int c = key & ~K_CHAR_FLAG;

        if (c == 'h' - 'a' + 1) {
            if (len) {
                br.tagInput[len - 1] = '\0';
            } else if (brf.numTags) {
                BR_RemoveFilterTag(brf.numTags - 1);
                BR_Collect();
            }
            return qtrue;
        }
        if (c == ' ' || c == ',' || c == '\t' || c == '\r') {
            if (br.tagInput[0]) {
                BR_AddFilterTag(br.tagInput);
                br.tagInput[0] = '\0';
                BR_Collect();
            }
            return qtrue;
        }
        if (c < 32 || len >= BR_TAG_LEN - 1) {
            return qtrue;
        }
        br.tagInput[len] = (char)c;
        br.tagInput[len + 1] = '\0';
        return qtrue;
    }
    if (key == K_ENTER || key == K_KP_ENTER || key == K_TAB) {
        if (br.tagInput[0]) {
            BR_AddFilterTag(br.tagInput);
            br.tagInput[0] = '\0';
            BR_Collect();
        }
        return qtrue;
    }
    if (key == K_ESCAPE) {
        br.tagFocus = qfalse;
        return qtrue;
    }
    return qfalse;
}

static qboolean BR_PasswordKey(int key) {
    int len = (int)strlen(br.password);

    if (key & K_CHAR_FLAG) {
        int c = key & ~K_CHAR_FLAG;

        if (c == 'h' - 'a' + 1) {
            if (len) {
                br.password[len - 1] = '\0';
            }
        } else if (c >= 32 && len < (int)sizeof(br.password) - 1) {
            br.password[len] = (char)c;
            br.password[len + 1] = '\0';
        }
        return qtrue;
    }
    return qfalse;
}

static void BR_PasswordConfirm(void) {
    trap_Cvar_Set("password", br.password);
    trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", br.modalServer.address));
    br.modal = BR_MODAL_NONE;
}

static void BR_ModalClick(const smHit_t* hit) {
    switch (hit ? hit->type : BR_HIT_NONE) {
        case BR_HIT_MODAL_CLOSE:
            br.modal = BR_MODAL_NONE;
            break;
        case BR_HIT_MODAL_JOIN: {
            brServer_t s = br.modalServer;

            br.modal = BR_MODAL_NONE;
            BR_Join(&s);
            break;
        }
        case BR_HIT_PASS_OK:
            BR_PasswordConfirm();
            break;
        case BR_HIT_PASS_CANCEL:
            br.modal = BR_MODAL_NONE;
            break;
        default:
            break;
    }
}

qboolean UI_Browser_HandleKey(int key) {
    const smHit_t* hit;

    // a dialog takes every key: Enter does what it is for, Escape closes it
    if (br.modal == BR_MODAL_PASSWORD) {
        if (BR_PasswordKey(key)) {
            return qtrue;
        }
        if (key == K_ENTER || key == K_KP_ENTER) {
            BR_PasswordConfirm();
        } else if (key == K_ESCAPE) {
            br.modal = BR_MODAL_NONE;
        } else if (key == K_MOUSE1) {
            BR_ModalClick(SM_HitAt(smMX, smMY));
        }
        return qtrue;
    }
    if (br.modal == BR_MODAL_INFO) {
        if (key == K_ENTER || key == K_KP_ENTER) {
            brServer_t s = br.modalServer;

            br.modal = BR_MODAL_NONE;
            BR_Join(&s);
        } else if (key == K_ESCAPE) {
            br.modal = BR_MODAL_NONE;
        } else if (key == K_MOUSE1) {
            BR_ModalClick(SM_HitAt(smMX, smMY));
        }
        return qtrue;
    }

    if (br.tagFocus && BR_TagKey(key)) {
        return qtrue;
    }
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }

    if (br.menuOpen) {
        if (key == K_MOUSE1 || key == K_MOUSE2 || key == K_ESCAPE) {
            hit = SM_HitAt(smMX, smMY);
            br.menuOpen = qfalse;
            if (key == K_MOUSE1 && hit && hit->type == BR_HIT_MENU_ITEM) {
                if (hit->index == 0) {
                    BR_ShowGameInfo(&br.menuServer);
                } else {
                    trap_LAN_SteamFavorite(br.menuServer.address, (qboolean)(brf.type != 3));
                }
            }
        }
        return qtrue;
    }

    switch (key) {
        case K_MWHEELUP:
        case K_MWHEELDOWN: {
            float step = key == K_MWHEELUP ? -3 * BR_ROW_STEP : 3 * BR_ROW_STEP;

            hit = SM_HitAt(smMX, smMY);
            if (br.dropdown != BR_DROP_NONE && hit && hit->type == BR_HIT_OPTION) {
                br.dropScroll += step * 0.5f;
            } else if (hit && hit->type == BR_HIT_PLAYERS) {
                br.playersScroll += step * 0.5f;
            } else {
                br.scroll = BR_ClampScroll(br.scroll + step);
            }
            return qtrue;
        }
        case K_MOUSE1:
        case K_MOUSE2:
            break;
        case K_ESCAPE:
            if (br.dropdown != BR_DROP_NONE) {
                br.dropdown = BR_DROP_NONE;
                return qtrue;
            }
            return qfalse;  // Escape goes back through the menu's onESC
        default:
            return qfalse;
    }

    if (key == K_MOUSE1 && UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);

    // the right button opens the row's menu, where the cursor is
    if (key == K_MOUSE2) {
        const brServer_t* s;

        if (hit && (hit->type == BR_HIT_ROW || hit->type == BR_HIT_PLAY) && (s = BR_RowServer(hit->index)) != NULL) {
            br.menuServer = *s;
            br.menuOpen = qtrue;
            br.menuX = smMX + 5;
            br.menuY = smMY + 5;
        }
        return qtrue;
    }

    // a click anywhere else closes an open select and takes the focus off the tags
    if (!hit || hit->type != BR_HIT_OPTION) {
        int wasOpen = br.dropdown;

        br.dropdown = BR_DROP_NONE;
        if (hit && hit->type == BR_HIT_SELECT && hit->index != wasOpen) {
            br.dropdown = hit->index;
            br.dropScroll = 0;
            return qtrue;
        }
        if (hit && hit->type == BR_HIT_SELECT) {
            return qtrue;
        }
    }
    br.tagFocus = (qboolean)(hit && (hit->type == BR_HIT_TAGBOX || hit->type == BR_HIT_TAGREMOVE));
    if (!hit) {
        return qtrue;
    }

    switch (hit->type) {
        case BR_HIT_TOGGLE:
            br.showFilters = (qboolean)!br.showFilters;
            break;
        case BR_HIT_REFRESH_ALL:
            BR_RequestNewList();
            break;
        case BR_HIT_QUICK_REFRESH:
            trap_LAN_SteamRefresh();
            break;
        case BR_HIT_SORT:
            BR_Sort(hit->index);
            break;
        case BR_HIT_ROW: {
            const brServer_t* s = BR_RowServer(hit->index);

            if (s) {
                BR_ShowDetails(s);
                if (!BR_Wide()) {
                    BR_ShowGameInfo(s);
                }
            }
            break;
        }
        case BR_HIT_PLAY: {
            const brServer_t* s = BR_RowServer(hit->index);

            if (s) {
                BR_Join(s);
            }
            break;
        }
        case BR_HIT_TAG: {
            const brServer_t* s = BR_RowServer(hit->index / BR_MAX_TAGS);

            if (s && hit->index % BR_MAX_TAGS < s->numTags) {
                BR_AddFilterTag(s->tags[hit->index % BR_MAX_TAGS]);
                BR_Collect();
            }
            break;
        }
        case BR_HIT_THUMB:
            br.scrollDrag = qtrue;
            br.scrollGrab = smMY;
            break;
        case BR_HIT_CHECK:
            BR_SetCheck(hit->index);
            break;
        case BR_HIT_OPTION:
            BR_ChooseOption(hit->index);
            break;
        case BR_HIT_TAGREMOVE:
            BR_RemoveFilterTag(hit->index);
            BR_Collect();
            break;
        case BR_HIT_JOIN:
            if (br.hasSelected) {
                BR_Join(&br.selected);
            }
            break;
        default:
            break;
    }
    return qtrue;
}
