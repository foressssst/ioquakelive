/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

This file is part of ioquakelive.

ioquakelive is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

ioquakelive is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ioquakelive; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "cg_local.h"
#include "../ui/ui_shared.h"
#include "cg_huddoc.h"

extern menuDef_t Menus[];
extern int menuCount;
extern menuDef_t* menuJoinGame;

static hdDoc_t hudDoc;
static char hudFileText[HD_MAX_TEXT];

static int CG_HudEdit_ReadFile(const char* path, char* buf, int size) {
    fileHandle_t f;
    int len;

    len = trap_FS_FOpenFile(path, &f, FS_READ);
    if (!f)
        return -1;
    if (len < 0 || len > size) {
        trap_FS_FCloseFile(f);
        return -1;
    }
    trap_FS_Read(buf, len, f);
    trap_FS_FCloseFile(f);
    return len;
}

static qboolean CG_HudEdit_SameRect(const float* a, const rectDef_t* b) {
    return fabs(a[0] - b->x) < 0.001f && fabs(a[1] - b->y) < 0.001f && fabs(a[2] - b->w) < 0.001f &&
           fabs(a[3] - b->h) < 0.001f;
}

static void CG_HudEdit_ItemName(int item, char* out, int size) {
    int p = HD_FindProp(&hudDoc, HDK_ITEM, item, "name");

    out[0] = 0;
    if (p >= 0 && hudDoc.props[p].xValues < hudDoc.props[p].xEnd)
        HD_TokenString(&hudDoc, hudDoc.xtoks[hudDoc.props[p].xValues].tok, out, size);
}

static int CG_HudEdit_ItemFlags(int item) {
    const hdItem_t* it = &hudDoc.items[item];
    int i, flags = 0;

    for (i = it->firstProp; i < it->firstProp + it->numProps; i++) {
        if (!Q_stricmp(HD_Keyword(HDK_ITEM, hudDoc.props[i].keyword), "ownerdrawFlag"))
            flags |= (int)hudDoc.props[i].values[0];
    }
    return flags;
}

static qboolean CG_HudEdit_CheckFile(const char* file, int* base, int* items) {
    static int order[HD_MAX_ITEMS];
    int len, i, j, count;
    const char* used = file;

    len = CG_HudEdit_ReadFile(file, hudFileText, sizeof(hudFileText) - 1);
    if (len < 0) {
        used = "ui/testhud.menu";
        len = CG_HudEdit_ReadFile(used, hudFileText, sizeof(hudFileText) - 1);
    }
    if (len < 0) {
        Com_Printf("hudedit_check: %s: not found\n", file);
        return qtrue;
    }
    hudFileText[len] = 0;

    if (!HD_Load(&hudDoc, used, hudFileText, len, CG_HudEdit_ReadFile)) {
        Com_Printf(S_COLOR_RED "hudedit_check: %s: %s\n", used, hudDoc.error);
        return qfalse;
    }

    for (i = 0; i < hudDoc.numMenus; i++) {
        const hdMenu_t* dm = &hudDoc.menus[i];
        const menuDef_t* gm;

        if (*base + i >= menuCount) {
            Com_Printf(S_COLOR_RED "hudedit_check: %s: menu %d missing in game\n", used, i);
            return qfalse;
        }
        gm = &Menus[*base + i];
        if (Q_stricmp(dm->name, gm->window.name ? gm->window.name : "")) {
            Com_Printf(S_COLOR_RED "hudedit_check: %s: menu %d name %s, game %s\n", used, i, dm->name,
                       gm->window.name ? gm->window.name : "");
            return qfalse;
        }
        count = 0;
        for (j = 0; j < dm->numItems; j++) {
            if (gm == menuJoinGame && (CG_HudEdit_ItemFlags(dm->firstItem + j) & CG_SHOW_IF_NOT_WARMUP))
                continue;
            order[count++] = dm->firstItem + j;
        }
        for (j = 0; j < count && j < gm->itemCount; j++) {
            char name[64];
            const char* gname = gm->items[j]->window.name ? gm->items[j]->window.name : "";

            CG_HudEdit_ItemName(order[j], name, sizeof(name));
            if (Q_stricmp(name, gname)) {
                Com_Printf(S_COLOR_RED "hudedit_check: %s: menu %s item %d name %s, game %s\n", used, dm->name, j,
                           name, gname);
                return qfalse;
            }
        }
        if (count != gm->itemCount) {
            Com_Printf(S_COLOR_RED "hudedit_check: %s: menu %s items %d, game %d\n", used, dm->name, count,
                       gm->itemCount);
            return qfalse;
        }
        if (dm->hasRect && !gm->fullScreen && !CG_HudEdit_SameRect(dm->rect, &gm->window.rect)) {
            Com_Printf(S_COLOR_RED "hudedit_check: %s: menu %s rect %g %g %g %g, game %g %g %g %g\n", used, dm->name,
                       dm->rect[0], dm->rect[1], dm->rect[2], dm->rect[3], gm->window.rect.x, gm->window.rect.y,
                       gm->window.rect.w, gm->window.rect.h);
            return qfalse;
        }
        for (j = 0; j < count; j++) {
            const hdItem_t* di = &hudDoc.items[order[j]];
            const itemDef_t* gi = gm->items[j];
            static const float zero[4] = {0, 0, 0, 0};

            if (!CG_HudEdit_SameRect(di->hasRect ? di->rect : zero, &gi->window.rectClient)) {
                Com_Printf(S_COLOR_RED "hudedit_check: %s: menu %s item %d rect %g %g %g %g, game %g %g %g %g\n", used,
                           dm->name, j, di->rect[0], di->rect[1], di->rect[2], di->rect[3], gi->window.rectClient.x,
                           gi->window.rectClient.y, gi->window.rectClient.w, gi->window.rectClient.h);
                return qfalse;
            }
        }
        *items += count;
    }
    *base += hudDoc.numMenus;
    Com_Printf("hudedit_check: %s: %d menus, %d items, %d unknown, %d unsupported\n", used, hudDoc.numMenus,
               hudDoc.numItems, hudDoc.unknownKeywords, hudDoc.unsupported);
    return qtrue;
}

void CG_HudEdit_Check_f(void) {
    static char list[MAX_MENUDEFFILE];
    char* p;
    char* token;
    int len, base = 0, items = 0;
    qboolean ok = qtrue;

    len = CG_HudEdit_ReadFile(cg_hudListFile, list, sizeof(list) - 1);
    if (len < 0) {
        Com_Printf("hudedit_check: %s: not found\n", cg_hudListFile);
        return;
    }
    list[len] = 0;
    COM_Compress(list);

    p = list;
    while (ok) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || !Q_stricmp(token, "}"))
            break;
        if (Q_stricmp(token, "loadmenu"))
            continue;
        token = COM_ParseExt(&p, qtrue);
        if (token[0] != '{')
            break;
        while (ok) {
            token = COM_ParseExt(&p, qtrue);
            if (!token[0] || !Q_stricmp(token, "}"))
                break;
            ok = CG_HudEdit_CheckFile(token, &base, &items);
        }
    }

    if (ok && base != cg_hudMenuCount) {
        Com_Printf(S_COLOR_RED "hudedit_check: %d menus, game %d\n", base, cg_hudMenuCount);
        ok = qfalse;
    }
    if (ok)
        Com_Printf("hudedit_check: OK %s: %d menus, %d items\n", cg_hudListFile, base, items);
    else
        Com_Printf(S_COLOR_RED "hudedit_check: FAILED %s\n", cg_hudListFile);
}
