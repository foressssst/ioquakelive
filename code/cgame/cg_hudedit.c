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
#include "../ui/ui_huddoc.h"

extern menuDef_t Menus[];
extern int menuCount;
extern menuDef_t* menuJoinGame;
qboolean Item_EnableShowViaCvar(itemDef_t* item, int flag);
qboolean Item_EnableShowViaCvar2(itemDef_t* item, int flag);
qboolean Item_EnableShowViaCvar3(itemDef_t* item, int flag);
qboolean Item_EnableShowViaCvar4(itemDef_t* item, int flag);
void Item_UpdatePosition(itemDef_t* item);

#define HE_MAX_DOCS 4
#define HE_KEEP_DOCS (HE_MAX_DOCS - 1)
#define HE_MAX_FILES 32
#define HE_MAX_ITEMS 4096
#define HE_MAX_UNDO 512

typedef struct {
    int doc;
    int docItem;
    menuDef_t* menu;
    itemDef_t* item;
    qboolean locked;
    qboolean shown;
    int edited;
} heItem_t;

typedef struct {
    int item;
    int key;
    int group;
    float values[4];
} heUndo_t;

typedef enum {
    HEK_RECT,
    HEK_WIDESCREEN,
    HEK_FORECOLOR,
    HEK_BACKCOLOR,
    HEK_BORDERCOLOR,
    HEK_TEXTSCALE,
    HEK_TEXTALIGNX,
    HEK_TEXTALIGNY,
    HEK_BORDERSIZE,
    HEK_COUNT
} heKey_t;

static const struct {
    const char* name;
    int count;
} heKeys[HEK_COUNT] = {
    {"rect", 4},       {"widescreen", 1}, {"forecolor", 4},  {"backcolor", 4},  {"bordercolor", 4},
    {"textscale", 1},  {"textalignx", 1}, {"textaligny", 1}, {"bordersize", 1},
};

typedef struct {
    qboolean active;
    int mappedLoad;
    char files[HE_MAX_FILES][MAX_QPATH];
    int numFiles;
    int docFile[HE_MAX_DOCS];
    int numDocs;
    heItem_t items[HE_MAX_ITEMS];
    int numItems;
    int hover, selected;
    qboolean showHidden;
    int drag;
    float dragX, dragY;
    rectDef_t dragRect;
    float mouseX, mouseY;
    heUndo_t undo[HE_MAX_UNDO];
    int numUndo;
    int undoGroup;
    char status[128];
    int statusTime;
} hudEdit_t;

static hdDoc_t heDocs[HE_MAX_DOCS];
static hudEdit_t he = {qfalse, -1};
static char heText[HD_MAX_TEXT];
static char heOut[HD_MAX_TEXT + 65536];

static const vec4_t heYellow = {0.964f, 0.815f, 0.0f, 1.0f};
static const vec4_t heWhite = {1.0f, 1.0f, 1.0f, 0.85f};
static const vec4_t heFaint = {1.0f, 1.0f, 1.0f, 0.18f};
static const vec4_t heHidden = {0.55f, 0.55f, 0.55f, 0.55f};
static const vec4_t heHiddenFill = {0.55f, 0.55f, 0.55f, 0.08f};
static const vec4_t hePanel = {0.0f, 0.0f, 0.0f, 0.78f};
static const vec4_t heText1 = {0.9f, 0.9f, 0.9f, 1.0f};
static const vec4_t heText2 = {0.6f, 0.6f, 0.6f, 1.0f};

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

static qboolean CG_HudEdit_WriteFile(const char* path, const char* buf, int len) {
    fileHandle_t f;

    trap_FS_FOpenFile(path, &f, FS_WRITE);
    if (!f)
        return qfalse;
    trap_FS_Write(buf, len, f);
    trap_FS_FCloseFile(f);
    return qtrue;
}

static qboolean CG_HudEdit_SameRect(const float* a, const rectDef_t* b) {
    return fabs(a[0] - b->x) < 0.001f && fabs(a[1] - b->y) < 0.001f && fabs(a[2] - b->w) < 0.001f &&
           fabs(a[3] - b->h) < 0.001f;
}

static void CG_HudEdit_PropString(const hdDoc_t* d, int kind, int owner, const char* key, char* out, int size) {
    int p = HD_FindProp(d, kind, owner, key);

    out[0] = 0;
    if (p >= 0 && d->props[p].xValues < d->props[p].xEnd)
        HD_TokenString(d, d->xtoks[d->props[p].xValues].tok, out, size);
}

static int CG_HudEdit_ItemFlags(const hdDoc_t* d, int item) {
    const hdItem_t* it = &d->items[item];
    int i, flags = 0;

    for (i = it->firstProp; i < it->firstProp + it->numProps; i++) {
        if (!Q_stricmp(HD_Keyword(HDK_ITEM, d->props[i].keyword), "ownerdrawFlag"))
            flags |= (int)d->props[i].values[0];
    }
    return flags;
}

static void CG_HudEdit_Status(const char* text) {
    Q_strncpyz(he.status, text, sizeof(he.status));
    he.statusTime = cg.time;
}

static int CG_HudEdit_ReadList(const char* listFile, char (*files)[MAX_QPATH]) {
    static char list[MAX_MENUDEFFILE];
    char* p;
    char* token;
    int len, n = 0;

    len = CG_HudEdit_ReadFile(listFile, list, sizeof(list) - 1);
    if (len < 0)
        return -1;
    list[len] = 0;
    COM_Compress(list);

    p = list;
    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || !Q_stricmp(token, "}"))
            break;
        if (Q_stricmp(token, "loadmenu"))
            continue;
        token = COM_ParseExt(&p, qtrue);
        if (token[0] != '{')
            break;
        while (1) {
            token = COM_ParseExt(&p, qtrue);
            if (!token[0] || !Q_stricmp(token, "}"))
                break;
            if (n < HE_MAX_FILES)
                Q_strncpyz(files[n++], token, MAX_QPATH);
        }
    }
    return n;
}

static qboolean CG_HudEdit_MapFile(const char* file, hdDoc_t* d, int docIndex, int* base, int* totalItems,
                                   qboolean report) {
    static int order[HD_MAX_ITEMS];
    const char* used = file;
    int len, i, j, count;

    len = CG_HudEdit_ReadFile(file, heText, sizeof(heText) - 1);
    if (len < 0) {
        used = "ui/testhud.menu";
        len = CG_HudEdit_ReadFile(used, heText, sizeof(heText) - 1);
    }
    if (len < 0) {
        if (report)
            Com_Printf("hudedit_check: %s: not found\n", file);
        return qtrue;
    }
    heText[len] = 0;

    if (!HD_Load(d, used, heText, len, CG_HudEdit_ReadFile)) {
        Com_Printf(S_COLOR_RED "hudedit: %s: %s\n", used, d->error);
        return qfalse;
    }

    for (i = 0; i < d->numMenus; i++) {
        const hdMenu_t* dm = &d->menus[i];
        menuDef_t* gm;

        if (*base + i >= menuCount) {
            Com_Printf(S_COLOR_RED "hudedit: %s: menu %d missing in game\n", used, i);
            return qfalse;
        }
        gm = &Menus[*base + i];
        if (Q_stricmp(dm->name, gm->window.name ? gm->window.name : "")) {
            Com_Printf(S_COLOR_RED "hudedit: %s: menu %d name %s, game %s\n", used, i, dm->name,
                       gm->window.name ? gm->window.name : "");
            return qfalse;
        }
        count = 0;
        for (j = 0; j < dm->numItems; j++) {
            if (gm == menuJoinGame && (CG_HudEdit_ItemFlags(d, dm->firstItem + j) & CG_SHOW_IF_NOT_WARMUP))
                continue;
            order[count++] = dm->firstItem + j;
        }
        for (j = 0; j < count && j < gm->itemCount; j++) {
            char name[64];
            const char* gname = gm->items[j]->window.name ? gm->items[j]->window.name : "";

            CG_HudEdit_PropString(d, HDK_ITEM, order[j], "name", name, sizeof(name));
            if (Q_stricmp(name, gname)) {
                Com_Printf(S_COLOR_RED "hudedit: %s: menu %s item %d name %s, game %s\n", used, dm->name, j, name,
                           gname);
                return qfalse;
            }
        }
        if (count != gm->itemCount) {
            Com_Printf(S_COLOR_RED "hudedit: %s: menu %s items %d, game %d\n", used, dm->name, count,
                       gm->itemCount);
            return qfalse;
        }
        if (dm->hasRect && !gm->fullScreen && !CG_HudEdit_SameRect(dm->rect, &gm->window.rect)) {
            Com_Printf(S_COLOR_RED "hudedit: %s: menu %s rect %g %g %g %g, game %g %g %g %g\n", used, dm->name,
                       dm->rect[0], dm->rect[1], dm->rect[2], dm->rect[3], gm->window.rect.x, gm->window.rect.y,
                       gm->window.rect.w, gm->window.rect.h);
            return qfalse;
        }
        for (j = 0; j < count; j++) {
            const hdItem_t* di = &d->items[order[j]];
            itemDef_t* gi = gm->items[j];
            static const float zero[4] = {0, 0, 0, 0};

            if (report && !CG_HudEdit_SameRect(di->hasRect ? di->rect : zero, &gi->window.rectClient)) {
                Com_Printf(S_COLOR_RED "hudedit: %s: menu %s item %d rect %g %g %g %g, game %g %g %g %g\n", used,
                           dm->name, j, di->rect[0], di->rect[1], di->rect[2], di->rect[3], gi->window.rectClient.x,
                           gi->window.rectClient.y, gi->window.rectClient.w, gi->window.rectClient.h);
                return qfalse;
            }
            if (!report && he.numItems < HE_MAX_ITEMS) {
                heItem_t* e = &he.items[he.numItems++];
                e->doc = docIndex;
                e->docItem = order[j];
                e->menu = gm;
                e->item = gi;
                e->locked = docIndex < 0 || !di->local;
                e->shown = qfalse;
                e->edited = qfalse;
            }
        }
        *totalItems += count;
    }
    *base += d->numMenus;
    if (report)
        Com_Printf("hudedit_check: %s: %d menus, %d items, %d unknown, %d unsupported\n", used, d->numMenus,
                   d->numItems, d->unknownKeywords, d->unsupported);
    return qtrue;
}

static qboolean CG_HudEdit_Check(void) {
    static char files[HE_MAX_FILES][MAX_QPATH];
    int f, n, base = 0, items = 0;
    qboolean ok = qtrue;

    n = CG_HudEdit_ReadList(cg_hudListFile, files);
    if (n < 0) {
        Com_Printf("hudedit_check: %s: not found\n", cg_hudListFile);
        return qfalse;
    }
    for (f = 0; f < n && ok; f++)
        ok = CG_HudEdit_MapFile(files[f], &heDocs[HE_KEEP_DOCS], -1, &base, &items, qtrue);
    if (ok && base != cg_hudMenuCount) {
        Com_Printf(S_COLOR_RED "hudedit: %d menus, game %d\n", base, cg_hudMenuCount);
        ok = qfalse;
    }
    if (ok)
        Com_Printf("hudedit_check: OK %s: %d menus, %d items\n", cg_hudListFile, base, items);
    else
        Com_Printf(S_COLOR_RED "hudedit_check: FAILED %s\n", cg_hudListFile);
    return ok;
}

static qboolean CG_HudEdit_Map(qboolean keepState) {
    int f, base = 0, items = 0;
    qboolean ok = qtrue;

    he.mappedLoad = -1;
    he.numDocs = 0;
    he.numItems = 0;
    he.drag = 0;
    if (!keepState) {
        he.hover = he.selected = -1;
        he.numUndo = 0;
    }

    he.numFiles = CG_HudEdit_ReadList(cg_hudListFile, he.files);
    if (he.numFiles < 0) {
        he.numFiles = 0;
        Com_Printf("hudedit: %s: not found\n", cg_hudListFile);
        return qfalse;
    }
    for (f = 0; f < he.numFiles && ok; f++) {
        int doc = he.numDocs < HE_KEEP_DOCS ? he.numDocs : -1;

        ok = CG_HudEdit_MapFile(he.files[f], &heDocs[doc >= 0 ? doc : HE_KEEP_DOCS], doc, &base, &items, qfalse);
        if (ok && doc >= 0)
            he.docFile[he.numDocs++] = f;
    }
    if (ok && base != cg_hudMenuCount) {
        Com_Printf(S_COLOR_RED "hudedit: %d menus, game %d\n", base, cg_hudMenuCount);
        ok = qfalse;
    }
    if (!ok) {
        he.numItems = 0;
        he.numDocs = 0;
        he.selected = he.hover = -1;
        he.numUndo = 0;
        return qfalse;
    }
    if (he.selected >= he.numItems)
        he.selected = -1;
    he.mappedLoad = cg_hudLoadCount;
    return qtrue;
}

static qboolean CG_HudEdit_EnsureMap(void) {
    if (he.mappedLoad == cg_hudLoadCount)
        return qtrue;
    return CG_HudEdit_Map(qfalse);
}

static int CG_HudEdit_Anchor(const heItem_t* e) {
    return e->item->widescreenFlag ? e->item->widescreen : e->menu->widescreen;
}

static void CG_HudEdit_Transform(int anchor, float* scale, float* offset) {
    if (cgs.widescreenBias <= 0.0f || anchor == WIDESCREEN_STRETCH) {
        *scale = cgs.screenXScale;
        *offset = 0.0f;
        return;
    }
    *scale = (4.0f * cgs.glconfig.vidHeight / 3.0f) / (float)SCREEN_WIDTH;
    if (anchor == WIDESCREEN_CENTER)
        *offset = cgs.widescreenBias;
    else if (anchor == WIDESCREEN_RIGHT)
        *offset = cgs.widescreenBias * 2.0f;
    else
        *offset = 0.0f;
}

static void CG_HudEdit_PixelRect(const heItem_t* e, float* out) {
    float scale, offset;
    const rectDef_t* r = &e->item->window.rect;

    CG_HudEdit_Transform(CG_HudEdit_Anchor(e), &scale, &offset);
    out[0] = r->x * scale + offset;
    out[1] = r->y * cgs.screenYScale;
    out[2] = r->w * scale;
    out[3] = r->h * cgs.screenYScale;
}

static qboolean CG_HudEdit_ItemShown(const heItem_t* e) {
    const menuDef_t* m = e->menu;
    itemDef_t* it = e->item;

    if (!(m->window.flags & WINDOW_VISIBLE))
        return qfalse;
    if ((m->window.ownerDrawFlags || m->window.ownerDrawFlags2) &&
        !CG_OwnerDrawVisible(m->window.ownerDrawFlags, m->window.ownerDrawFlags2))
        return qfalse;
    if (it->cvarFlags & (CVAR_SHOW | CVAR_HIDE)) {
        if (!Item_EnableShowViaCvar(it, CVAR_SHOW) || !Item_EnableShowViaCvar2(it, CVAR_SHOW) ||
            !Item_EnableShowViaCvar3(it, CVAR_SHOW) || !Item_EnableShowViaCvar4(it, CVAR_SHOW))
            return qfalse;
    }
    return (it->window.flags & WINDOW_VISIBLE) ? qtrue : qfalse;
}

static float CG_HudEdit_HandleSize(void) {
    float s = cgs.glconfig.vidHeight / 90.0f;
    return s < 6.0f ? 6.0f : s;
}

static qboolean CG_HudEdit_OnHandle(int i) {
    float r[4], s = CG_HudEdit_HandleSize();

    CG_HudEdit_PixelRect(&he.items[i], r);
    return he.mouseX >= r[0] + r[2] - s && he.mouseX <= r[0] + r[2] + s * 0.5f && he.mouseY >= r[1] + r[3] - s &&
           he.mouseY <= r[1] + r[3] + s * 0.5f;
}

static qboolean CG_HudEdit_Under(int i) {
    float r[4];

    CG_HudEdit_PixelRect(&he.items[i], r);
    if (r[2] < 1.0f || r[3] < 1.0f)
        return qfalse;
    return he.mouseX >= r[0] && he.mouseX < r[0] + r[2] && he.mouseY >= r[1] && he.mouseY < r[1] + r[3];
}

static qboolean CG_HudEdit_Pickable(int i) {
    return he.items[i].shown || he.showHidden;
}

static int CG_HudEdit_HitTest(void) {
    int i;

    for (i = he.numItems - 1; i >= 0; i--) {
        if (CG_HudEdit_Pickable(i) && CG_HudEdit_Under(i))
            return i;
    }
    return -1;
}

static float* CG_HudEdit_Field(itemDef_t* it, int key) {
    switch (key) {
        case HEK_RECT:
            return &it->window.rectClient.x;
        case HEK_FORECOLOR:
            return it->window.foreColor;
        case HEK_BACKCOLOR:
            return it->window.backColor;
        case HEK_BORDERCOLOR:
            return it->window.borderColor;
        case HEK_TEXTSCALE:
            return &it->textscale;
        case HEK_TEXTALIGNX:
            return &it->textalignx;
        case HEK_TEXTALIGNY:
            return &it->textaligny;
        case HEK_BORDERSIZE:
            return &it->window.borderSize;
        default:
            return NULL;
    }
}

static void CG_HudEdit_Get(int i, int key, float* v) {
    itemDef_t* it = he.items[i].item;
    float* f;
    int n;

    if (key == HEK_WIDESCREEN) {
        v[0] = it->widescreenFlag ? (float)it->widescreen : -1.0f;
        return;
    }
    f = CG_HudEdit_Field(it, key);
    for (n = 0; n < heKeys[key].count; n++)
        v[n] = f[n];
}

static void CG_HudEdit_Set(int i, int key, const float* v) {
    itemDef_t* it = he.items[i].item;
    float* f;
    int n;

    if (key == HEK_WIDESCREEN) {
        if (v[0] < 0.0f) {
            it->widescreenFlag = 0;
        } else {
            it->widescreen = (int)v[0] & 3;
            it->widescreenFlag = 1;
        }
    } else {
        f = CG_HudEdit_Field(it, key);
        for (n = 0; n < heKeys[key].count; n++)
            f[n] = v[n];
    }
    if (key == HEK_RECT || key == HEK_BORDERSIZE)
        Item_UpdatePosition(it);
    if (key == HEK_TEXTSCALE || key == HEK_TEXTALIGNX || key == HEK_TEXTALIGNY) {
        it->textRect.w = 0;
        it->textRect.h = 0;
    }
    he.items[i].edited |= 1 << key;
}

static void CG_HudEdit_BeginUndo(void) {
    he.undoGroup++;
}

static void CG_HudEdit_PushUndo(int i, int key) {
    heUndo_t* u;

    if (he.numUndo >= HE_MAX_UNDO) {
        memmove(he.undo, he.undo + 1, sizeof(he.undo[0]) * (HE_MAX_UNDO - 1));
        he.numUndo--;
    }
    u = &he.undo[he.numUndo++];
    u->item = i;
    u->key = key;
    u->group = he.undoGroup;
    CG_HudEdit_Get(i, key, u->values);
}

static void CG_HudEdit_Undo(void) {
    int group;

    if (he.numUndo <= 0) {
        CG_HudEdit_Status("Nothing to undo");
        return;
    }
    group = he.undo[he.numUndo - 1].group;
    while (he.numUndo > 0 && he.undo[he.numUndo - 1].group == group) {
        heUndo_t* u = &he.undo[--he.numUndo];
        CG_HudEdit_Set(u->item, u->key, u->values);
        he.selected = u->item;
    }
}

static void CG_HudEdit_Change(int i, int key, const float* v) {
    CG_HudEdit_BeginUndo();
    CG_HudEdit_PushUndo(i, key);
    CG_HudEdit_Set(i, key, v);
}

static void CG_HudEdit_Nudge(int i, float dx, float dy, float dw, float dh) {
    float r[4];

    CG_HudEdit_Get(i, HEK_RECT, r);
    r[0] += dx;
    r[1] += dy;
    r[2] += dw;
    r[3] += dh;
    if (dw != 0.0f && r[2] < 1.0f)
        r[2] = 1.0f;
    if (dh != 0.0f && r[3] < 1.0f)
        r[3] = 1.0f;
    CG_HudEdit_Change(i, HEK_RECT, r);
}

static void CG_HudEdit_SetAnchor(int i, int anchor) {
    heItem_t* e = &he.items[i];
    float scale, offset, nscale, noffset, center, r[4], ws = (float)anchor;

    CG_HudEdit_Transform(CG_HudEdit_Anchor(e), &scale, &offset);
    CG_HudEdit_Transform(anchor, &nscale, &noffset);
    center = (e->item->window.rect.x + e->item->window.rect.w * 0.5f) * scale + offset;
    CG_HudEdit_Get(i, HEK_RECT, r);
    r[0] += (float)floor((center - noffset) / nscale - e->item->window.rect.w * 0.5f - e->item->window.rect.x + 0.5f);
    CG_HudEdit_BeginUndo();
    CG_HudEdit_PushUndo(i, HEK_RECT);
    CG_HudEdit_PushUndo(i, HEK_WIDESCREEN);
    CG_HudEdit_Set(i, HEK_RECT, r);
    CG_HudEdit_Set(i, HEK_WIDESCREEN, &ws);
}

static void CG_HudEdit_TextScale(int i, float delta) {
    float s;

    CG_HudEdit_Get(i, HEK_TEXTSCALE, &s);
    s = (float)floor((s + delta) * 100.0f + 0.5f) / 100.0f;
    if (s < 0.05f)
        s = 0.05f;
    CG_HudEdit_Change(i, HEK_TEXTSCALE, &s);
}

static const char* CG_HudEdit_AnchorName(int anchor) {
    switch (anchor) {
        case WIDESCREEN_LEFT:
            return "LEFT";
        case WIDESCREEN_CENTER:
            return "CENTER";
        case WIDESCREEN_RIGHT:
            return "RIGHT";
        default:
            return "STRETCH";
    }
}

static qboolean CG_HudEdit_ValidName(const char* s) {
    int n = 0;

    for (; *s; s++, n++) {
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' ||
              *s == '-'))
            return qfalse;
    }
    return n > 0 && n <= 32;
}

static void CG_HudEdit_DefaultName(char* out, int size) {
    const char* s = he.numFiles > 0 ? he.files[0] : "hud";
    const char* slash = strrchr(s, '/');
    char* dot;

    Q_strncpyz(out, slash ? slash + 1 : s, size);
    dot = strrchr(out, '.');
    if (dot)
        *dot = 0;
    if (!CG_HudEdit_ValidName(out))
        Q_strncpyz(out, "hud", size);
}

static qboolean CG_HudEdit_KeyChanged(int i, int key, float* v) {
    const heItem_t* e = &he.items[i];
    const hdDoc_t* d = &heDocs[e->doc];
    int p, n;

    if (key == HEK_WIDESCREEN && !e->item->widescreenFlag)
        return qfalse;
    CG_HudEdit_Get(i, key, v);
    p = HD_FindProp(d, HDK_ITEM, e->docItem, heKeys[key].name);
    if (p < 0) {
        if (key != HEK_RECT)
            return qtrue;
        return v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f || v[3] != 0.0f;
    }
    for (n = 0; n < heKeys[key].count; n++) {
        if (fabs(d->props[p].values[n] - v[n]) > 0.0001f)
            return qtrue;
    }
    return qfalse;
}

static void CG_HudEdit_Save(const char* requested) {
    char name[64], path[MAX_QPATH], listPath[MAX_QPATH], list[MAX_MENUDEFFILE], msg[128];
    char paths[HE_MAX_FILES][MAX_QPATH];
    qboolean changed[HE_MAX_DOCS];
    int i, k, len, written = 0;

    if (!CG_HudEdit_EnsureMap())
        return;
    if (requested && requested[0]) {
        if (!CG_HudEdit_ValidName(requested)) {
            CG_HudEdit_Status("Name: letters, digits, - and _ only");
            return;
        }
        Q_strncpyz(name, requested, sizeof(name));
    } else {
        CG_HudEdit_DefaultName(name, sizeof(name));
    }

    for (k = 0; k < he.numDocs; k++) {
        HD_ClearEdits(&heDocs[k]);
        changed[k] = qfalse;
    }
    for (i = 0; i < he.numItems; i++) {
        heItem_t* e = &he.items[i];
        int key;

        if (e->locked || !e->edited)
            continue;
        for (key = 0; key < HEK_COUNT; key++) {
            float v[4];

            if (!(e->edited & (1 << key)) || !CG_HudEdit_KeyChanged(i, key, v))
                continue;
            if (!HD_SetValues(&heDocs[e->doc], HDK_ITEM, e->docItem, heKeys[key].name, v, heKeys[key].count)) {
                CG_HudEdit_Status("Could not save");
                return;
            }
            changed[e->doc] = qtrue;
        }
    }

    for (i = 0; i < he.numFiles; i++)
        Q_strncpyz(paths[i], he.files[i], MAX_QPATH);
    for (k = 0; k < he.numDocs; k++) {
        if (!changed[k])
            continue;
        if (written == 0)
            Com_sprintf(path, sizeof(path), "hud/%s.menu", name);
        else
            Com_sprintf(path, sizeof(path), "hud/%s_%d.menu", name, written + 1);
        len = HD_Write(&heDocs[k], heOut, sizeof(heOut));
        if (len < 0 || !CG_HudEdit_WriteFile(path, heOut, len)) {
            CG_HudEdit_Status("Could not save");
            return;
        }
        Q_strncpyz(paths[he.docFile[k]], path, MAX_QPATH);
        written++;
    }
    if (!written) {
        CG_HudEdit_Status("Nothing to save");
        return;
    }

    Q_strncpyz(list, "{\n\tloadMenu {\n", sizeof(list));
    for (i = 0; i < he.numFiles; i++)
        Q_strcat(list, sizeof(list), va("\t\t\"%s\"\n", paths[i]));
    Q_strcat(list, sizeof(list), "\t}\n}\n");
    Com_sprintf(listPath, sizeof(listPath), "hud/%s.txt", name);
    if (!CG_HudEdit_WriteFile(listPath, list, strlen(list))) {
        CG_HudEdit_Status("Could not save");
        return;
    }
    trap_Cvar_Set("cg_hudFiles", listPath);
    Q_strncpyz(cg_hudListFile, listPath, sizeof(cg_hudListFile));

    if (!CG_HudEdit_Map(qtrue)) {
        CG_HudEdit_Status("Saved, but the file does not match the game");
        return;
    }
    Com_sprintf(msg, sizeof(msg), "Saved %s", listPath);
    CG_HudEdit_Status(msg);
    Com_Printf("hudedit: saved %s\n", listPath);
}

static void CG_HudEdit_Start(void) {
    int catcher;

    if (!CG_HudEdit_EnsureMap()) {
        Com_Printf("hudedit: this HUD cannot be edited\n");
        return;
    }
    he.active = qtrue;
    he.drag = 0;
    he.hover = -1;
    catcher = trap_Key_GetCatcher();
    trap_Key_SetCatcher(catcher | KEYCATCH_CGAME);
}

static void CG_HudEdit_Stop(void) {
    int catcher;

    if (!he.active)
        return;
    he.active = qfalse;
    he.drag = 0;
    catcher = trap_Key_GetCatcher();
    if (catcher & KEYCATCH_CGAME)
        trap_Key_SetCatcher(catcher & ~KEYCATCH_CGAME);
}

qboolean CG_HudEdit_Active(void) {
    return he.active;
}

void CG_HudEdit_Escape(void) {
    CG_HudEdit_Stop();
}

qboolean CG_HudEdit_MouseEvent(int x, int y) {
    float scale, offset, dx, dy, r[4];
    heItem_t* e;

    if (!he.active)
        return qfalse;
    he.mouseX = (float)x;
    he.mouseY = (float)y;
    if (!he.drag || he.selected < 0)
        return qtrue;

    e = &he.items[he.selected];
    CG_HudEdit_Transform(CG_HudEdit_Anchor(e), &scale, &offset);
    dx = (he.mouseX - he.dragX) / scale;
    dy = (he.mouseY - he.dragY) / cgs.screenYScale;
    r[0] = he.dragRect.x;
    r[1] = he.dragRect.y;
    r[2] = he.dragRect.w;
    r[3] = he.dragRect.h;
    if (he.drag == 1) {
        r[0] = (float)floor(he.dragRect.x + dx + 0.5f);
        r[1] = (float)floor(he.dragRect.y + dy + 0.5f);
    } else {
        r[2] = (float)floor(he.dragRect.w + dx + 0.5f);
        r[3] = (float)floor(he.dragRect.h + dy + 0.5f);
        if (r[2] < 1.0f)
            r[2] = 1.0f;
        if (r[3] < 1.0f)
            r[3] = 1.0f;
    }
    CG_HudEdit_Set(he.selected, HEK_RECT, r);
    return qtrue;
}

static void CG_HudEdit_Cycle(int dir) {
    int i, n = he.numItems, start = he.selected;

    if (n <= 0)
        return;
    if (start < 0)
        start = dir > 0 ? n - 1 : 0;
    for (i = 1; i <= n; i++) {
        int j = ((start + dir * i) % n + n) % n;
        if (CG_HudEdit_Pickable(j)) {
            he.selected = j;
            return;
        }
    }
}

qboolean CG_HudEdit_KeyEvent(int key, qboolean down) {
    qboolean ctrl, shift;
    float step;
    int sel;

    if (!he.active)
        return qfalse;

    if (key == K_MOUSE1) {
        if (down) {
            int hit;

            if (he.selected >= 0 && CG_HudEdit_Pickable(he.selected) && CG_HudEdit_OnHandle(he.selected)) {
                hit = he.selected;
                he.drag = 2;
            } else if (he.selected >= 0 && CG_HudEdit_Pickable(he.selected) && CG_HudEdit_Under(he.selected)) {
                hit = he.selected;
                he.drag = 1;
            } else {
                hit = CG_HudEdit_HitTest();
                he.drag = 1;
            }
            he.selected = hit;
            if (hit < 0) {
                he.drag = 0;
                return qtrue;
            }
            if (he.items[hit].locked) {
                he.drag = 0;
                CG_HudEdit_Status("Locked: this element comes from an included file");
                return qtrue;
            }
            CG_HudEdit_BeginUndo();
            CG_HudEdit_PushUndo(hit, HEK_RECT);
            he.dragX = he.mouseX;
            he.dragY = he.mouseY;
            he.dragRect = he.items[hit].item->window.rectClient;
        } else if (he.drag) {
            const rectDef_t* now = &he.items[he.selected].item->window.rectClient;
            he.drag = 0;
            if (he.numUndo > 0 && now->x == he.dragRect.x && now->y == he.dragRect.y && now->w == he.dragRect.w &&
                now->h == he.dragRect.h)
                he.numUndo--;
        }
        return qtrue;
    }

    if (!down)
        return qtrue;

    {
        char binding[32];

        trap_Key_GetBindingBuf(key, binding, sizeof(binding));
        if (!Q_strncmp(binding, "screenshot", 10)) {
            trap_SendConsoleCommand(va("%s\n", binding));
            return qtrue;
        }
    }

    ctrl = trap_Key_IsDown(K_CTRL);
    shift = trap_Key_IsDown(K_SHIFT);
    step = shift ? 10.0f : 1.0f;
    sel = he.selected;

    if (ctrl && key == 'z') {
        CG_HudEdit_Undo();
        return qtrue;
    }
    if (ctrl && key == 's') {
        CG_HudEdit_Save(NULL);
        return qtrue;
    }
    if (key == K_TAB) {
        CG_HudEdit_Cycle(shift ? -1 : 1);
        return qtrue;
    }
    if (key == 'h') {
        he.showHidden = !he.showHidden;
        if (sel >= 0 && !CG_HudEdit_Pickable(sel))
            he.selected = -1;
        return qtrue;
    }
    if (sel < 0 || he.drag)
        return qtrue;
    if (he.items[sel].locked) {
        if (key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW || key == 'a')
            CG_HudEdit_Status("Locked: this element comes from an included file");
        return qtrue;
    }

    switch (key) {
        case K_LEFTARROW:
            CG_HudEdit_Nudge(sel, ctrl ? 0 : -step, 0, ctrl ? -step : 0, 0);
            break;
        case K_RIGHTARROW:
            CG_HudEdit_Nudge(sel, ctrl ? 0 : step, 0, ctrl ? step : 0, 0);
            break;
        case K_UPARROW:
            CG_HudEdit_Nudge(sel, 0, ctrl ? 0 : -step, 0, ctrl ? -step : 0);
            break;
        case K_DOWNARROW:
            CG_HudEdit_Nudge(sel, 0, ctrl ? 0 : step, 0, ctrl ? step : 0);
            break;
        case 'a':
            CG_HudEdit_SetAnchor(sel, (CG_HudEdit_Anchor(&he.items[sel]) + 1) % 4);
            break;
        case '=':
        case K_KP_PLUS:
            CG_HudEdit_TextScale(sel, shift ? 0.05f : 0.01f);
            break;
        case '-':
        case K_KP_MINUS:
            CG_HudEdit_TextScale(sel, shift ? -0.05f : -0.01f);
            break;
        default:
            break;
    }
    return qtrue;
}

static void CG_HudEdit_Box(const float* r, float t, const vec4_t color) {
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(r[0], r[1], r[2], t, 0, 0, 0, 0, cgs.media.whiteShader);
    trap_R_DrawStretchPic(r[0], r[1] + r[3] - t, r[2], t, 0, 0, 0, 0, cgs.media.whiteShader);
    trap_R_DrawStretchPic(r[0], r[1] + t, t, r[3] - 2 * t, 0, 0, 0, 0, cgs.media.whiteShader);
    trap_R_DrawStretchPic(r[0] + r[2] - t, r[1] + t, t, r[3] - 2 * t, 0, 0, 0, 0, cgs.media.whiteShader);
    trap_R_SetColor(NULL);
}

static void CG_HudEdit_Fill(float x, float y, float w, float h, const vec4_t color) {
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, 0, 0, cgs.media.whiteShader);
    trap_R_SetColor(NULL);
}

static void CG_HudEdit_DrawPanel(void) {
    char line1[256], line2[256], item[64], menu[64];
    const char* help1 = "Click: select   Drag: move   Corner: resize   Arrows: move   Ctrl+Arrows: resize   Shift: x10";
    const char* help2 = "A: anchor   +/-: text size   H: hidden   Tab: next   Ctrl+Z: undo   Ctrl+S: save   Esc: exit";
    float y, w = 600.0f, h = 46.0f, band = (h + 8.0f) * cgs.screenYScale, r[4];
    qboolean top = qtrue;

    if (he.mouseY < band * 1.5f)
        top = qfalse;
    if (he.selected >= 0) {
        CG_HudEdit_PixelRect(&he.items[he.selected], r);
        if (r[1] < band)
            top = qfalse;
        if (!top && r[1] + r[3] > cgs.glconfig.vidHeight - band && he.mouseY >= band * 1.5f)
            top = qtrue;
    }

    if (he.selected >= 0) {
        heItem_t* e = &he.items[he.selected];
        const rectDef_t* rc = &e->item->window.rectClient;
        char scale[32];

        Q_strncpyz(menu, e->menu->window.name ? e->menu->window.name : "", sizeof(menu));
        if (e->item->window.name && e->item->window.name[0])
            Q_strncpyz(item, e->item->window.name, sizeof(item));
        else
            Com_sprintf(item, sizeof(item), "#%d", e->docItem);
        scale[0] = 0;
        if (e->item->textscale > 0.0f)
            Com_sprintf(scale, sizeof(scale), "   TEXT %g", e->item->textscale);
        Com_sprintf(line2, sizeof(line2), "%s / %s     X %g   Y %g   W %g   H %g%s     %s%s%s", menu, item, rc->x,
                    rc->y, rc->w, rc->h, scale, CG_HudEdit_AnchorName(CG_HudEdit_Anchor(e)),
                    e->shown ? "" : "     HIDDEN", e->locked ? "     LOCKED" : "");
    } else {
        Q_strncpyz(line2, "No element selected", sizeof(line2));
    }
    Com_sprintf(line1, sizeof(line1), "HUD EDITOR     %s%s", cg_hudListFile, he.showHidden ? "     showing hidden" : "");

    y = top ? 4.0f : SCREEN_HEIGHT - h - 4.0f;
    CG_SetWidescreen(WIDESCREEN_CENTER);
    CG_FillRect((SCREEN_WIDTH - w) * 0.5f, y, w, h, hePanel);
    CG_DrawText((SCREEN_WIDTH - w) * 0.5f + 8, y + 11, 1, 0.20f, (float*)heYellow, line1, 0, 0, 0);
    CG_DrawText((SCREEN_WIDTH - w) * 0.5f + 8, y + 22, 1, 0.20f, (float*)heText1, line2, 0, 0, 0);
    if (he.status[0] && cg.time - he.statusTime < 3000)
        CG_DrawText((SCREEN_WIDTH - w) * 0.5f + 8, y + 33, 1, 0.18f, (float*)heYellow, he.status, 0, 0, 0);
    else
        CG_DrawText((SCREEN_WIDTH - w) * 0.5f + 8, y + 33, 1, 0.16f, (float*)heText2, help1, 0, 0, 0);
    CG_DrawText((SCREEN_WIDTH - w) * 0.5f + 8, y + 42, 1, 0.16f, (float*)heText2, help2, 0, 0, 0);
    CG_SetWidescreen(WIDESCREEN_STRETCH);
}

void CG_HudEdit_Draw(void) {
    float t, r[4], s;
    int i, catcher;

    if (!he.active) {
        cg_hudPainted = qfalse;
        return;
    }
    catcher = trap_Key_GetCatcher();
    if (!(catcher & KEYCATCH_CGAME) || (catcher & KEYCATCH_UI)) {
        CG_HudEdit_Stop();
        cg_hudPainted = qfalse;
        return;
    }
    if (he.mappedLoad != cg_hudLoadCount && !CG_HudEdit_Map(qfalse)) {
        CG_HudEdit_Stop();
        cg_hudPainted = qfalse;
        return;
    }
    if (!cg_hudPainted && !cg.showScores) {
        CG_PaintHudMenus();
        cg_hudPainted = qtrue;
    }
    for (i = 0; i < he.numItems; i++)
        he.items[i].shown = cg_hudPainted && CG_HudEdit_ItemShown(&he.items[i]);
    cg_hudPainted = qfalse;

    if (he.selected >= he.numItems)
        he.selected = -1;
    he.hover = he.drag ? -1 : CG_HudEdit_HitTest();

    t = cgs.glconfig.vidHeight >= 1440 ? 2.0f : 1.0f;
    for (i = 0; i < he.numItems; i++) {
        if (i == he.selected || i == he.hover)
            continue;
        CG_HudEdit_PixelRect(&he.items[i], r);
        if (r[2] < 1.0f || r[3] < 1.0f)
            continue;
        if (he.items[i].shown) {
            CG_HudEdit_Box(r, t, heFaint);
        } else if (he.showHidden) {
            CG_HudEdit_Fill(r[0], r[1], r[2], r[3], heHiddenFill);
            CG_HudEdit_Box(r, t, heHidden);
        }
    }
    if (he.hover >= 0 && he.hover != he.selected) {
        CG_HudEdit_PixelRect(&he.items[he.hover], r);
        CG_HudEdit_Box(r, t, heWhite);
    }
    if (he.selected >= 0) {
        CG_HudEdit_PixelRect(&he.items[he.selected], r);
        if (r[2] < 1.0f)
            r[2] = 1.0f;
        if (r[3] < 1.0f)
            r[3] = 1.0f;
        CG_HudEdit_Box(r, t * 2.0f, heYellow);
        if (!he.items[he.selected].locked) {
            s = CG_HudEdit_HandleSize();
            CG_HudEdit_Fill(r[0] + r[2] - s * 0.75f, r[1] + r[3] - s * 0.75f, s, s, heYellow);
        }
    }
    CG_HudEdit_DrawPanel();
}

static int CG_HudEdit_Find(const char* menu, const char* item) {
    int i, n = 0, index = atoi(item);
    qboolean byIndex = item[0] >= '0' && item[0] <= '9';

    for (i = 0; i < he.numItems; i++) {
        const heItem_t* e = &he.items[i];

        if (Q_stricmp(e->menu->window.name ? e->menu->window.name : "", menu))
            continue;
        if (byIndex ? n == index : !Q_stricmp(e->item->window.name ? e->item->window.name : "", item))
            return i;
        n++;
    }
    return -1;
}

static int CG_HudEdit_FindArgs(void) {
    char menu[MAX_QPATH], item[MAX_QPATH];
    int i;

    if (!CG_HudEdit_EnsureMap())
        return -1;
    trap_Argv(1, menu, sizeof(menu));
    trap_Argv(2, item, sizeof(item));
    i = CG_HudEdit_Find(menu, item);
    if (i < 0) {
        Com_Printf("hudedit: %s / %s not found\n", menu, item);
        return -1;
    }
    if (he.items[i].locked) {
        Com_Printf("hudedit: %s / %s is locked\n", menu, item);
        return -1;
    }
    return i;
}

void CG_HudEdit_f(void) {
    if (he.active)
        CG_HudEdit_Stop();
    else
        CG_HudEdit_Start();
}

void CG_HudEdit_Save_f(void) {
    CG_HudEdit_Save(CG_Argv(1));
    Com_Printf("%s\n", he.status);
}

static float CG_HudEdit_ArgFloat(int n) {
    char buf[64];

    trap_Argv(n, buf, sizeof(buf));
    return (float)atof(buf);
}

void CG_HudEdit_Move_f(void) {
    float v[4];
    int i, n;

    if (trap_Argc() < 5) {
        Com_Printf("usage: hudedit_move <menu> <item> <dx> <dy> [<dw> <dh>]\n");
        return;
    }
    i = CG_HudEdit_FindArgs();
    if (i < 0)
        return;
    for (n = 0; n < 4; n++)
        v[n] = CG_HudEdit_ArgFloat(3 + n);
    CG_HudEdit_Nudge(i, v[0], v[1], v[2], v[3]);
    he.selected = i;
}

void CG_HudEdit_Anchor_f(void) {
    int i;

    if (trap_Argc() < 4) {
        Com_Printf("usage: hudedit_anchor <menu> <item> <0-3>\n");
        return;
    }
    i = CG_HudEdit_FindArgs();
    if (i < 0)
        return;
    CG_HudEdit_SetAnchor(i, (int)CG_HudEdit_ArgFloat(3) & 3);
    he.selected = i;
}

void CG_HudEdit_Set_f(void) {
    char key[64];
    float v[4];
    int i, k, n;

    trap_Argv(3, key, sizeof(key));
    for (k = 0; k < HEK_COUNT; k++) {
        if (!Q_stricmp(key, heKeys[k].name))
            break;
    }
    if (k == HEK_COUNT || k == HEK_WIDESCREEN || trap_Argc() < 4 + heKeys[k].count) {
        Com_Printf("usage: hudedit_set <menu> <item> <rect|forecolor|backcolor|bordercolor|textscale|textalignx|"
                   "textaligny|bordersize> <values>\n");
        return;
    }
    i = CG_HudEdit_FindArgs();
    if (i < 0)
        return;
    for (n = 0; n < heKeys[k].count; n++)
        v[n] = CG_HudEdit_ArgFloat(4 + n);
    CG_HudEdit_Change(i, k, v);
    he.selected = i;
}

void CG_HudEdit_Undo_f(void) {
    CG_HudEdit_Undo();
}

void CG_HudEdit_Check_f(void) {
    CG_HudEdit_Check();
}
