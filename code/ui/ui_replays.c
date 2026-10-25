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
#include "../qcommon/json.h"

#include <time.h>

#define RP_MAX_DEMOS 1024
#define RP_ROW_H 39.0f
#define RP_BAR_H 32.0f
#define RP_SEARCH_LEN 64

typedef enum {
    RP_HIT_NONE,
    RP_HIT_ROW,
    RP_HIT_INFO,
    RP_HIT_RANGE,
    RP_HIT_SORT,
    RP_HIT_SEARCH
} rpHitType_t;

typedef enum {
    RP_RANGE_ALL,
    RP_RANGE_TODAY,
    RP_RANGE_YESTERDAY,
    RP_RANGE_WEEK,
    RP_NUM_RANGES
} rpRange_t;

typedef enum {
    RP_SORT_NEWEST,
    RP_SORT_OLDEST,
    RP_SORT_MAP,
    RP_SORT_MODE,
    RP_SORT_FILE,
    RP_NUM_SORTS
} rpSort_t;

typedef struct {
    char file[MAX_QPATH];
    char guid[64];
    char date[64];
    char label[256];
    char mode[64];
    char map[64];
    char search[1024];
    int time;
} rpDemo_t;

static struct {
    int count;
    rpDemo_t demo[RP_MAX_DEMOS];
    int order[RP_MAX_DEMOS];
    int shown;
    int range;
    int sort;
    char search[RP_SEARCH_LEN];
    qboolean searchFocus;
    float scroll;
    float content;
} rp;

static const char* const rpRanges[RP_NUM_RANGES] = {"All", "Today", "Yesterday", "Last 7 Days"};
static const char* const rpSorts[RP_NUM_SORTS] = {"Newest", "Oldest", "Map", "Game Type", "File Name"};
static const char* const rpGametypes[] = {"Free For All", "Duel",       "Race",      "Team Deathmatch", "Clan Arena",
                                          "Capture The Flag", "One Flag CTF", "Overload", "Harvester", "Freeze Tag",
                                          "Domination",   "Attack & Defend", "Red Rover"};

static const vec4_t rpBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t rpOdd = SM_RGB(36, 33, 33);
static const vec4_t rpEven = SM_RGB(28, 25, 25);
static const vec4_t rpHover = SM_RGB(60, 57, 57);
static const vec4_t rpYellow = SM_RGB(247, 209, 0);
static const vec4_t rpGrey = SM_RGB(153, 153, 153);
static const vec4_t rpField = SM_RGB(15, 15, 15);
static const vec4_t rpFieldBorder = SM_RGB(51, 51, 51);
static const vec4_t rpPlaceholder = SM_RGB(119, 119, 119);

static int RP_CompareText(const char* a, const char* b, const rpDemo_t* x, const rpDemo_t* y) {
    int c = Q_stricmp(a, b);

    return c ? c : y->time - x->time;
}

static int RP_Compare(const void* a, const void* b) {
    const rpDemo_t* x = &rp.demo[*(const int*)a];
    const rpDemo_t* y = &rp.demo[*(const int*)b];

    switch (rp.sort) {
        case RP_SORT_OLDEST:
            return x->time != y->time ? x->time - y->time : Q_stricmp(x->file, y->file);
        case RP_SORT_MAP:
            return RP_CompareText(x->map, y->map, x, y);
        case RP_SORT_MODE:
            return RP_CompareText(x->mode, y->mode, x, y);
        case RP_SORT_FILE:
            return Q_stricmp(x->file, y->file);
        default:
            return x->time != y->time ? y->time - x->time : Q_stricmp(x->file, y->file);
    }
}

static int RP_DayStart(time_t t) {
    struct tm* lt = localtime(&t);
    struct tm day;

    if (!lt) {
        return 0;
    }
    day = *lt;
    day.tm_hour = 0;
    day.tm_min = 0;
    day.tm_sec = 0;
    return (int)mktime(&day);
}

static void RP_Date(int seconds, char* out, int size) {
    static const char* const months[12] = {"January", "February", "March",     "April",   "May",      "June",
                                           "July",    "August",   "September", "October", "November", "December"};
    time_t t = (time_t)seconds;
    struct tm* now = localtime(&t);
    struct tm lt;
    int d, h, today;
    const char* th;

    if (!seconds || !now) {
        out[0] = '\0';
        return;
    }
    lt = *now;
    d = lt.tm_mday;
    h = lt.tm_hour % 12;
    today = RP_DayStart(time(NULL));
    if (seconds >= today) {
        Com_sprintf(out, size, "Today, %i:%02i %s", h ? h : 12, lt.tm_min, lt.tm_hour < 12 ? "am" : "pm");
        return;
    }
    if (seconds >= RP_DayStart((time_t)today - 43200)) {
        Com_sprintf(out, size, "Yesterday, %i:%02i %s", h ? h : 12, lt.tm_min, lt.tm_hour < 12 ? "am" : "pm");
        return;
    }
    th = (d % 10 == 1 && d != 11) ? "st" : (d % 10 == 2 && d != 12) ? "nd" : (d % 10 == 3 && d != 13) ? "rd" : "th";
    Com_sprintf(out, size, "%s %i%s %i, %i:%02i %s", months[lt.tm_mon], d, th, lt.tm_year + 1900, h ? h : 12,
                lt.tm_min, lt.tm_hour < 12 ? "am" : "pm");
}

static int RP_ReadFile(const char* name, char* buf, int size) {
    return trap_MatchFile(name, buf, size);
}

static void RP_String(const char* obj, const char* end, const char* name, char* out, int size) {
    const char* v = obj ? JSON_ObjectGetNamedValue(obj, end, name) : NULL;

    out[0] = '\0';
    if (v && JSON_ValueGetType(v, end) == JSONTYPE_STRING) {
        JSON_ValueGetString(v, end, out, (unsigned int)size);
    }
}

static const char* RP_MapName(const char* map) {
    const char* info = UI_GetArenaInfoByMap(map);
    const char* name = info ? Info_ValueForKey(info, "longname") : "";

    return name[0] ? name : map;
}

static void RP_Describe(rpDemo_t* d) {
    static char buf[0x10000];
    char map[64], type[32], server[128];
    const char* end;
    int len = RP_ReadFile(va("%s.json", d->guid), buf, sizeof(buf));

    if (len <= 0) {
        d->guid[0] = '\0';
        return;
    }
    end = buf + len;
    RP_String(buf, end, "MAP", map, sizeof(map));
    RP_String(buf, end, "GAME_TYPE", type, sizeof(type));
    RP_String(buf, end, "SERVER_TITLE", server, sizeof(server));
    Com_sprintf(d->label, sizeof(d->label), " %s on %s: %s", type, RP_MapName(map), server);
}

static void RP_ReadDemo(rpDemo_t* d) {
    char info[MAX_INFO_STRING];
    char map[64], host[128], players[MAX_STRING_CHARS];
    const char* factory;
    int gametype;

    trap_DemoInfo(d->file, info, sizeof(info));
    d->time = atoi(Info_ValueForKey(info, "time"));
    Q_strncpyz(map, Info_ValueForKey(info, "map"), sizeof(map));
    if (map[0]) {
        Q_strncpyz(d->map, RP_MapName(map), sizeof(d->map));
    }
    factory = Info_ValueForKey(info, "factory");
    gametype = atoi(Info_ValueForKey(info, "gametype"));
    if (factory[0]) {
        Q_strncpyz(d->mode, factory, sizeof(d->mode));
    } else if (Info_ValueForKey(info, "gametype")[0] && gametype >= 0 && gametype < (int)ARRAY_LEN(rpGametypes)) {
        Q_strncpyz(d->mode, rpGametypes[gametype], sizeof(d->mode));
    }
    Q_strncpyz(host, Info_ValueForKey(info, "host"), sizeof(host));
    Q_CleanStr(host);
    Q_strncpyz(players, Info_ValueForKey(info, "players"), sizeof(players));
    Q_CleanStr(players);
    if (d->map[0]) {
        Com_sprintf(d->label, sizeof(d->label), " %s on %s: %s", d->mode[0] ? d->mode : "Match", d->map, host);
    }
    Com_sprintf(d->search, sizeof(d->search), "%s %s %s %s %s", d->file, d->mode, d->map, host, players);
}

static void RP_Load(void) {
    static char list[0x40000];
    static char media[0x40000];
    char ext[32];
    char* p;
    int n, i, len;

    rp.count = 0;
    Com_sprintf(ext, sizeof(ext), "dm_%d", (int)trap_Cvar_VariableValue("protocol"));
    n = trap_FS_GetFileList("demos", ext, list, sizeof(list));
    for (p = list, i = 0; i < n && rp.count < RP_MAX_DEMOS; i++, p += strlen(p) + 1) {
        const char* c;

        for (c = p; *c && *c != '"' && (unsigned char)*c >= ' '; c++) {
        }
        if (*c) {
            continue;
        }
        Com_Memset(&rp.demo[rp.count], 0, sizeof(rp.demo[0]));
        Q_strncpyz(rp.demo[rp.count].file, p, sizeof(rp.demo[0].file));
        RP_ReadDemo(&rp.demo[rp.count]);
        rp.count++;
    }

    len = RP_ReadFile("media.txt", media, sizeof(media));
    for (p = len > 0 ? media : NULL; p && *p;) {
        char* eol = strchr(p, '\n');
        char* space;

        if (eol) {
            *eol = '\0';
        }
        space = strchr(p, ' ');
        if (space) {
            *space = '\0';
            for (i = 0; i < rp.count; i++) {
                if (!Q_stricmp(rp.demo[i].file, p)) {
                    Q_strncpyz(rp.demo[i].guid, space + 1, sizeof(rp.demo[i].guid));
                }
            }
        }
        p = eol ? eol + 1 : NULL;
    }
    for (i = 0; i < rp.count; i++) {
        char* cr = strchr(rp.demo[i].guid, '\r');

        if (cr) {
            *cr = '\0';
        }
        if (rp.demo[i].guid[0]) {
            RP_Describe(&rp.demo[i]);
        }
        RP_Date(rp.demo[i].time, rp.demo[i].date, sizeof(rp.demo[i].date));
    }
}

static qboolean RP_InRange(const rpDemo_t* d) {
    int today;

    if (rp.range == RP_RANGE_ALL) {
        return qtrue;
    }
    if (rp.range == RP_RANGE_WEEK) {
        return (qboolean)(d->time >= (int)time(NULL) - 7 * 86400);
    }
    today = RP_DayStart(time(NULL));
    if (rp.range == RP_RANGE_TODAY) {
        return (qboolean)(d->time >= today);
    }
    return (qboolean)(d->time < today && d->time >= RP_DayStart((time_t)today - 43200));
}

static void RP_Filter(void) {
    int i;

    rp.shown = 0;
    for (i = 0; i < rp.count; i++) {
        if (RP_InRange(&rp.demo[i]) && (!rp.search[0] || Q_stristr(rp.demo[i].search, rp.search))) {
            rp.order[rp.shown++] = i;
        }
    }
    qsort(rp.order, rp.shown, sizeof(rp.order[0]), RP_Compare);
}

void UI_Replays_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    rp.scroll = 0;
    rp.searchFocus = qfalse;
    RP_Load();
    RP_Filter();
    menu = Menus_FindByName("demo");
    item = menu ? Menu_FindItemByName(menu, "replays") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

static void RP_DrawToolbar(float left, float y, float width, const smRect_t* clip) {
    const char* sort = va("Sort: %s", rpSorts[rp.sort]);
    float sortW = SM_WebButtonWidth(sort);
    float baseline = SM_Baseline(FONT_SANS, 15, y + 6, 20);
    float x = left + 11.4f;
    float right = left + width - 10;
    float fieldX, fieldW;
    int i;

    for (i = 0; i < RP_NUM_RANGES; i++) {
        float tw = SM_AdvanceRaw(FONT_SANS, 15, rpRanges[i], -1, qfalse);
        qboolean on = (qboolean)(rp.range == i);

        if (SM_TextInside(baseline, 15, clip)) {
            SM_TextRaw(x, baseline, FONT_SANS, 15, on ? rpYellow : SM_Over(RP_HIT_RANGE, i) ? smText : rpGrey,
                       rpRanges[i], -1, qfalse);
            if (on) {
                SM_Fill(x, y + RP_BAR_H - 5, tw, 2, rpYellow, clip);
            }
        }
        SM_Hit(x - 6, y, tw + 12, RP_BAR_H, RP_HIT_RANGE, i, clip);
        x += tw + 24;
    }

    SM_WebButton(right - sortW, y + (RP_BAR_H - SM_WEB_BUTTON_H) * 0.5f, sortW, sort, qfalse, SM_Over(RP_HIT_SORT, 0),
                 clip);
    SM_Hit(right - sortW, y + (RP_BAR_H - SM_WEB_BUTTON_H) * 0.5f, sortW, SM_WEB_BUTTON_H, RP_HIT_SORT, 0, clip);

    fieldX = x + 6;
    fieldW = right - sortW - 10 - fieldX;
    if (fieldW < 120) {
        return;
    }
    SM_Fill(fieldX, y, fieldW, RP_BAR_H, rpField, clip);
    SM_Fill(fieldX, y, fieldW, 1, rpFieldBorder, clip);
    SM_Fill(fieldX, y + RP_BAR_H - 1, fieldW, 1, rpFieldBorder, clip);
    SM_Fill(fieldX, y + 1, 1, RP_BAR_H - 2, rpFieldBorder, clip);
    SM_Fill(fieldX + fieldW - 1, y + 1, 1, RP_BAR_H - 2, rpFieldBorder, clip);
    SM_Hit(fieldX, y, fieldW, RP_BAR_H, RP_HIT_SEARCH, 0, clip);
    if (SM_TextInside(baseline, 15, clip)) {
        if (rp.search[0] || rp.searchFocus) {
            SM_TextRaw(fieldX + 8, baseline, FONT_SANS, 15, smText, rp.search, -1, qfalse);
            if (rp.searchFocus && (uiInfo.uiDC.realTime / 500) % 2 == 0) {
                SM_Fill(fieldX + 8 + SM_AdvanceRaw(FONT_SANS, 15, rp.search, -1, qfalse) + 1, y + 8, 1, 16, smText,
                        clip);
            }
        } else {
            SM_TextRaw(fieldX + 8, baseline, FONT_SANS, 15, rpPlaceholder, "Search map, mode, server or player", -1,
                       qfalse);
        }
    }
}

void UI_Replays_Paint(void) {
    float left = UI_Home_Left();
    float width = HM_CONTAINER_W;
    float view = smBottom - 30 - HM_NAV_H;
    float x0 = left + 5, w0 = width - 10;
    float infoW = w0 * 0.03f;
    float top, y;
    smRect_t clip;
    int i;

    SM_BeginFrame();
    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_REPLAYS);

    if (rp.scroll > rp.content - view) {
        rp.scroll = rp.content - view;
    }
    if (rp.scroll < 0) {
        rp.scroll = 0;
    }
    clip = SM_R(left, HM_NAV_H - 8, width, view + 8);
    top = HM_NAV_H - rp.scroll;

    SM_Fill(left, HM_NAV_H, width, view, rpBlack80, NULL);
    {
        float b = SM_Baseline(FONT_DEFAULT, 32, top + 8, 38.4f);

        if (SM_TextInside(b, 32, &clip)) {
            float tx = left + 11.4f;

            SM_SetSpacing(SM_SPACING_H1);
            SM_Text(tx, b, FONT_DEFAULT, 32, smText, "Replays", -1, qfalse);
            tx += SM_Advance(FONT_DEFAULT, 32, "Replays", -1);
            SM_SetSpacing(0);
            SM_TextRaw(tx + 14, b, FONT_SANS, 15, rpGrey,
                       rp.shown == rp.count ? va("%i", rp.count) : va("%i of %i", rp.shown, rp.count), -1, qfalse);
        }
    }
    RP_DrawToolbar(left, top + 54.4f, width, &clip);
    y = top + 54.4f + RP_BAR_H + 10;
    for (i = 0; i < rp.shown; i++) {
        const rpDemo_t* d = &rp.demo[rp.order[i]];
        float baseline = SM_Baseline(FONT_SANS, 15, y + 9.5f, 20);
        qboolean over = SM_Over(RP_HIT_ROW, i) || SM_Over(RP_HIT_INFO, i);

        if (y + RP_ROW_H >= clip.y && y <= clip.y + clip.h) {
            SM_Fill(x0, y, w0, RP_ROW_H, over ? rpHover : (i & 1) ? rpEven : rpOdd, &clip);
            SM_Hit(x0 + infoW, y, w0 - infoW, RP_ROW_H, RP_HIT_ROW, i, &clip);
            if (SM_TextInside(baseline, 15, &clip)) {
                float nx = x0 + infoW;

                if (d->guid[0]) {
                    float iw = SM_IconWidth(15, 0xf05a) + SM_Advance(FONT_SANS, 15, "  ", -1);
                    float ix = x0 + (infoW - iw) * 0.5f;

                    SM_Icon(ix, baseline, 15, rpYellow, 0xf05a);
                    SM_Hit(ix, y, SM_IconWidth(15, 0xf05a), RP_ROW_H, RP_HIT_INFO, i, &clip);
                }
                if (d->label[0]) {
                    if (d->date[0]) {
                        SM_TextRaw(nx, baseline, FONT_SANS, 15, rpGrey, va("%s:", d->date), -1, qfalse);
                        nx += SM_AdvanceRaw(FONT_SANS, 15, va("%s:", d->date), -1, qfalse);
                    }
                    SM_TextRaw(nx, baseline, FONT_SANS, 15, smText, d->label, -1, qfalse);
                    SM_TextRaw(x0 + w0 - 10 - SM_AdvanceRaw(FONT_SANS, 15, d->file, -1, qfalse), baseline, FONT_SANS,
                               15, rpGrey, d->file, -1, qfalse);
                } else {
                    SM_TextRaw(nx, baseline, FONT_SANS, 15, smText, d->file, -1, qfalse);
                }
            }
        }
        y += RP_ROW_H;
    }
    rp.content = y - top + 5;

    UI_Home_Banner();
    SM_EndFrame();
}

static qboolean RP_SearchKey(int key) {
    int len = (int)strlen(rp.search);

    if (key & K_CHAR_FLAG) {
        int c = key & ~K_CHAR_FLAG;

        if (c == 'h' - 'a' + 1) {
            if (len) {
                rp.search[len - 1] = '\0';
                RP_Filter();
            }
            return qtrue;
        }
        if ((unsigned char)c < 32 || len >= RP_SEARCH_LEN - 1) {
            return qtrue;
        }
        rp.search[len] = (char)c;
        rp.search[len + 1] = '\0';
        rp.scroll = 0;
        RP_Filter();
        return qtrue;
    }
    if (key == K_ENTER || key == K_KP_ENTER) {
        if (rp.shown) {
            trap_Cmd_ExecuteText(EXEC_APPEND, va("demo \"%s\"\n", rp.demo[rp.order[0]].file));
        }
        return qtrue;
    }
    if (key == K_ESCAPE) {
        if (rp.search[0]) {
            rp.search[0] = '\0';
            RP_Filter();
        }
        rp.searchFocus = qfalse;
        return qtrue;
    }
    return qfalse;
}

qboolean UI_Replays_HandleKey(int key) {
    const smHit_t* hit;

    if (rp.searchFocus && RP_SearchKey(key)) {
        return qtrue;
    }
    if (key & K_CHAR_FLAG) {
        int c = key & ~K_CHAR_FLAG;

        if ((unsigned char)c > 32) {
            rp.searchFocus = qtrue;
            return RP_SearchKey(key);
        }
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        rp.scroll += key == K_MWHEELUP ? -60.0f : 60.0f;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    rp.searchFocus = (qboolean)(hit && hit->type == RP_HIT_SEARCH);
    if (!hit) {
        return qtrue;
    }
    switch (hit->type) {
        case RP_HIT_ROW:
            trap_Cmd_ExecuteText(EXEC_APPEND, va("demo \"%s\"\n", rp.demo[rp.order[hit->index]].file));
            break;
        case RP_HIT_INFO:
            UI_Matches_ShowMatch(rp.demo[rp.order[hit->index]].guid);
            break;
        case RP_HIT_RANGE:
            rp.range = hit->index;
            rp.scroll = 0;
            RP_Filter();
            break;
        case RP_HIT_SORT:
            rp.sort = (rp.sort + 1) % RP_NUM_SORTS;
            rp.scroll = 0;
            RP_Filter();
            break;
        default:
            break;
    }
    return qtrue;
}
