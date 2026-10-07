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
// Retail's is web.pak's demos.js: an h1 that says Replays and a list of the files
// GetDemoList returns, sorted. A demo recorded during a match the player kept (the media
// index, see client/cl_matches.c) is shown as the match -- when, the mode on the map,
// and the server -- with an info mark in front that opens the match's page; any other
// is its file name. Clicking a row sends `demo "<file>"`.
//

#include "ui_web.h"
#include "../qcommon/json.h"

#include <time.h>

#define RP_MAX_DEMOS 256
#define RP_ROW_H 39.0f  // 0.5em over and under a 24px line

typedef enum {
    RP_HIT_NONE,
    RP_HIT_ROW,
    RP_HIT_INFO
} rpHitType_t;

typedef struct {
    char file[MAX_QPATH];
    char guid[64];     // the match it belongs to, "" when none was kept
    char date[64];     // "September 28th 2026, 12:28 pm"
    char label[256];   // " FFA on Campgrounds: server"
} rpDemo_t;

static struct {
    int count;
    rpDemo_t demo[RP_MAX_DEMOS];
    float scroll;
    float content;
} rp;

static const vec4_t rpBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t rpOdd = SM_RGB(36, 33, 33);
static const vec4_t rpEven = SM_RGB(28, 25, 25);
static const vec4_t rpHover = SM_RGB(60, 57, 57);
static const vec4_t rpYellow = SM_RGB(247, 209, 0);
static const vec4_t rpGrey = SM_RGB(153, 153, 153);

static int RP_Compare(const void* a, const void* b) {
    return strcmp(((const rpDemo_t*)a)->file, ((const rpDemo_t*)b)->file);
}

// moment's "MMMM Do YYYY, h:mm a"
static void RP_Date(double ms, char* out, int size) {
    static const char* const months[12] = {"January", "February", "March",     "April",   "May",      "June",
                                           "July",    "August",   "September", "October", "November", "December"};
    time_t t = (time_t)(ms / 1000);
    struct tm* lt = localtime(&t);
    int d, h;
    const char* th;

    if (!lt) {
        out[0] = '\0';
        return;
    }
    d = lt->tm_mday;
    th = (d % 10 == 1 && d != 11) ? "st" : (d % 10 == 2 && d != 12) ? "nd" : (d % 10 == 3 && d != 13) ? "rd" : "th";
    h = lt->tm_hour % 12;
    Com_sprintf(out, size, "%s %i%s %i, %i:%02i %s", months[lt->tm_mon], d, th, lt->tm_year + 1900, h ? h : 12,
                lt->tm_min, lt->tm_hour < 12 ? "am" : "pm");
}

static int RP_ReadFile(const char* name, char* buf, int size) {
    fileHandle_t f;
    int len = trap_FS_FOpenFile(name, &f, FS_READ);

    if (!f) {
        return -1;
    }
    if (len <= 0 || len >= size) {
        trap_FS_FCloseFile(f);
        return -1;
    }
    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);
    return len;
}

static void RP_String(const char* obj, const char* end, const char* name, char* out, int size) {
    const char* v = obj ? JSON_ObjectGetNamedValue(obj, end, name) : NULL;

    out[0] = '\0';
    if (v && JSON_ValueGetType(v, end) == JSONTYPE_STRING) {
        JSON_ValueGetString(v, end, out, (unsigned int)size);
    }
}

// the match a demo belongs to, as its row shows it
static void RP_Describe(rpDemo_t* d) {
    static char buf[0x10000];
    char map[64], type[32], server[128];
    const char* info;
    const char* v;
    const char* end;
    const char* mapName;
    int len = RP_ReadFile(va("matches/%s.json", d->guid), buf, sizeof(buf));

    if (len <= 0) {
        d->guid[0] = '\0';
        return;
    }
    end = buf + len;
    RP_String(buf, end, "MAP", map, sizeof(map));
    RP_String(buf, end, "GAME_TYPE", type, sizeof(type));
    RP_String(buf, end, "SERVER_TITLE", server, sizeof(server));
    v = JSON_ObjectGetNamedValue(buf, end, "TIME_PLAYED");
    RP_Date(v ? JSON_ValueGetDouble(v, end) : 0, d->date, sizeof(d->date));
    info = UI_GetArenaInfoByMap(map);
    mapName = info ? Info_ValueForKey(info, "longname") : "";
    Com_sprintf(d->label, sizeof(d->label), " %s on %s: %s", type, mapName[0] ? mapName : map, server);
}

static void RP_Load(void) {
    static char list[0x10000];
    static char media[0x40000];
    char ext[32];
    char* p;
    int n, i, len;

    rp.count = 0;
    Com_sprintf(ext, sizeof(ext), "dm_%d", (int)trap_Cvar_VariableValue("protocol"));
    n = trap_FS_GetFileList("demos", ext, list, sizeof(list));
    for (p = list, i = 0; i < n && rp.count < RP_MAX_DEMOS; i++) {
        Com_Memset(&rp.demo[rp.count], 0, sizeof(rp.demo[0]));
        Q_strncpyz(rp.demo[rp.count].file, p, sizeof(rp.demo[0].file));
        rp.count++;
        p += strlen(p) + 1;
    }
    qsort(rp.demo, rp.count, sizeof(rp.demo[0]), RP_Compare);

    // the media index: the last line for a file says its match
    len = RP_ReadFile("matches/media.txt", media, sizeof(media));
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
    }
}

void UI_Replays_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    rp.scroll = 0;
    RP_Load();
    menu = Menus_FindByName("demo");
    item = menu ? Menu_FindItemByName(menu, "replays") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
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

    // .replays: black at 0.8 the whole height, the h1, and the list 5px in
    SM_Fill(left, HM_NAV_H, width, view, rpBlack80, NULL);
    {
        float b = SM_Baseline(FONT_DEFAULT, 32, top + 8, 38.4f);

        if (SM_TextInside(b, 32, &clip)) {
            SM_SetSpacing(SM_SPACING_H1);
            SM_Text(left + 11.4f, b, FONT_DEFAULT, 32, smText, "Replays", -1, qfalse);
            SM_SetSpacing(0);
        }
    }
    y = top + 54.4f;
    for (i = 0; i < rp.count; i++) {
        const rpDemo_t* d = &rp.demo[i];
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
                    SM_TextRaw(nx, baseline, FONT_SANS, 15, rpGrey, va("%s:", d->date), -1, qfalse);
                    nx += SM_AdvanceRaw(FONT_SANS, 15, va("%s:", d->date), -1, qfalse);
                    SM_TextRaw(nx, baseline, FONT_SANS, 15, smText, d->label, -1, qfalse);
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

qboolean UI_Replays_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        rp.scroll += key == K_MWHEELUP ? -60.0f : 60.0f;
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
        case RP_HIT_ROW:
            // retail sends exactly this
            trap_Cmd_ExecuteText(EXEC_APPEND, va("demo \"%s\"\n", rp.demo[hit->index].file));
            break;
        case RP_HIT_INFO:
            UI_Matches_ShowMatch(rp.demo[hit->index].guid);
            break;
        default:
            break;
    }
    return qtrue;
}
