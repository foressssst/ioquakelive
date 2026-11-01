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
#include "ui_hudview.h"
#include "ui_huddoc.h"

void Item_UpdatePosition(itemDef_t* item);

#define HE_MAX_DOCS 3
#define HE_MAX_ITEMS 4096
#define HE_MAX_UNDO 1024
#define HE_GRID 10.0f
#define HE_FONT FONT_SANS
#define HE_G 16.0f
#define HE_BAR 64.0f
#define HE_LEFT_W 320.0f
#define HE_RIGHT_W 340.0f
#define HE_STATUS_H 48.0f
#define HE_HEAD_H 56.0f
#define HE_ROW_H 34.0f
#define HE_GROUP_H 32.0f
#define HE_R_PANEL 20.0f
#define HE_R 10.0f
#define HE_SPACING 1.4f
#define HE_CTRL_H 40.0f

#define HE_FA_UNDO 0xf0e2
#define HE_FA_REDO 0xf01e
#define HE_FA_GRID 0xf00a
#define HE_FA_SAVE 0xf0c7
#define HE_FA_CLOSE 0xf00d
#define HE_FA_EYE 0xf06e
#define HE_FA_EYE_OFF 0xf070
#define HE_FA_IMAGE 0xf03e
#define HE_FA_TEXT 0xf031
#define HE_FA_BOX 0xf096
#define HE_FA_MOVE 0xf047
#define HE_FA_LOCK 0xf023
#define HE_FA_INFO 0xf05a
#define HE_FA_CHECK 0xf00c
#define HE_FA_MINUS 0xf068
#define HE_FA_PLUS 0xf067
#define HE_FA_MAP 0xf108
#define HE_FA_CHEVRON 0xf078
#define HE_FA_IMPORT 0xf093
#define HE_FA_EXPORT 0xf019
#define HE_FA_SLIDERS 0xf1de
#define HE_FA_LAYERS 0xf0c9
#define HE_FA_CROSSHAIR 0xf05b
#define HE_FA_CHART 0xf201
#define HE_FA_CHAT 0xf0e5
#define HE_FA_BOLT 0xf0e7
#define HE_FA_FOLDER 0xf07c
#define HE_FA_TRASH 0xf1f8
#define HE_FA_COPY 0xf0c5
#define HE_FA_FILE 0xf15b

#define HE_MAX_HUDS 48

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
    int doc;
    int docItem;
    menuDef_t* menu;
    itemDef_t* item;
    qboolean locked;
    int edited;
} heItem_t;

typedef struct {
    int item;
    int key;
    int group;
    float values[4];
} heUndo_t;

typedef enum {
    HE_HIT_NONE,
    HE_HIT_PANEL,
    HE_HIT_MODE,
    HE_HIT_TEAM,
    HE_HIT_UNDO,
    HE_HIT_REDO,
    HE_HIT_GRID,
    HE_HIT_ALL,
    HE_HIT_MAP,
    HE_HIT_SAVE,
    HE_HIT_CLOSE,
    HE_HIT_LAYER,
    HE_HIT_FIELD,
    HE_HIT_STEP,
    HE_HIT_ANCHOR,
    HE_HIT_SWATCH,
    HE_HIT_PALETTE,
    HE_HIT_ALPHA,
    HE_HIT_SCALE,
    HE_HIT_POPUP,
    HE_HIT_CONFIRM,
    HE_HIT_HUD,
    HE_HIT_HUDITEM
} heHit_t;

typedef enum {
    HE_DRAG_NONE,
    HE_DRAG_MOVE,
    HE_DRAG_SIZE,
    HE_DRAG_ALPHA,
    HE_DRAG_SCALE
} heDrag_t;

enum { HF_X, HF_Y, HF_W, HF_H, HF_SCALE, HF_BORDER, HF_COUNT };

typedef struct {
    int gametype;
    const char* label;
} heMode_t;

static const heMode_t heModes[] = {
    {GT_FFA, "FFA"}, {GT_DUEL, "Duel"}, {GT_TEAM, "TDM"}, {GT_CA, "CA"}, {GT_CTF, "CTF"}, {GT_FREEZE, "FT"},
};

static const struct {
    const char* label;
    const char* list;
} heRetailHuds[] = {{"Normal", "ui/hud.txt"}, {"Small", "ui/hud2.txt"}, {"Large", "ui/hud3.txt"}};

static const char* heMaps[] = {"campgrounds", "bloodrun", "aerowalk", "toxicity", "furiousheights", "lostworld", "sinister", "almostlost"};
static const char* heMapNames[] = {"Campgrounds", "Blood Run", "Aerowalk", "Toxicity", "Furious Heights", "Lost World", "Sinister", "Almost Lost"};

static struct {
    qboolean open;
    char list[MAX_QPATH];
    char files[HV_MAX_FILES][MAX_QPATH];
    int numFiles;
    int menusPerFile[HV_MAX_FILES];
    int docFile[HE_MAX_DOCS];
    int numDocs;
    heItem_t items[HE_MAX_ITEMS];
    int numItems;
    int lockedFiles;
    int selected;
    int drag;
    int handle;
    float grabX, grabY;
    float start[4];
    float boxOff[4];
    float startScale;
    float startBox[4];
    float offX, offY;
    qboolean moved;
    float guideX, guideY;
    heUndo_t undo[HE_MAX_UNDO];
    int numUndo;
    heUndo_t redo[HE_MAX_UNDO];
    int numRedo;
    int group;
    qboolean dirty;
    float scroll;
    float scrollMax;
    qboolean grid;
    int map;
    int field;
    char fieldBuf[32];
    int colorKey;
    float popup[4];
    float track[2];
    qboolean confirm;
    char pending[MAX_QPATH];
    qboolean hudMenu;
    float hudMenuX;
    char huds[HE_MAX_HUDS][MAX_QPATH];
    int numHuds;
    char status[160];
    int statusTime;
    float cv[4];
    const char* tip;
    float tipX, tipY;
    qhandle_t round;
    qhandle_t hole;
} he;

static hdDoc_t heDocs[HE_MAX_DOCS];
static char heText[HD_MAX_TEXT];
static char heOut[HD_MAX_TEXT + 65536];

static const vec4_t heBg = {0.042f, 0.039f, 0.036f, 1.0f};
static const vec4_t hePanel = {0.077f, 0.072f, 0.068f, 1.0f};
static const vec4_t heCard = {0.104f, 0.098f, 0.094f, 1.0f};
static const vec4_t heRow = {0.122f, 0.117f, 0.112f, 1.0f};
static const vec4_t heRowHover = {0.169f, 0.164f, 0.159f, 1.0f};
static const vec4_t heEdge = {0.165f, 0.159f, 0.154f, 1.0f};
static const vec4_t heGold = {0.897f, 0.675f, 0.297f, 1.0f};
static const vec4_t heGoldHover = {0.963f, 0.738f, 0.364f, 1.0f};
static const vec4_t heGoldText = {0.078f, 0.075f, 0.063f, 1.0f};
static const vec4_t heHi = {0.925f, 0.902f, 0.859f, 1.0f};
static const vec4_t heMd = {0.647f, 0.612f, 0.553f, 1.0f};
static const vec4_t heLo = {0.486f, 0.459f, 0.408f, 1.0f};
static const vec4_t heGhost = {0.294f, 0.275f, 0.243f, 1.0f};
static const vec4_t heWarn = {0.86f, 0.42f, 0.38f, 1.0f};
static const vec4_t heShade = {0.0f, 0.0f, 0.0f, 0.6f};
static const vec4_t heShadow = {0.0f, 0.0f, 0.0f, 0.3f};
static const vec4_t heHoverLine = {1.0f, 1.0f, 1.0f, 0.55f};
static const vec4_t heGuide = {1.0f, 0.25f, 0.65f, 0.95f};
static const vec4_t heGridLine = {1.0f, 1.0f, 1.0f, 0.06f};
static const vec4_t heGridMid = {1.0f, 1.0f, 1.0f, 0.16f};
static const vec4_t heHiddenLine = {0.6f, 0.6f, 0.65f, 0.7f};
static const vec4_t heTeamRed = {0.9f, 0.3f, 0.25f, 1.0f};
static const vec4_t heTeamBlue = {0.3f, 0.5f, 0.95f, 1.0f};

static const unsigned int hePalette[32] = {
    0x000000, 0x333333, 0x666666, 0x808080, 0x999999, 0xbbbbbb, 0xdddddd, 0xffffff,
    0xff3b30, 0xff9500, 0xffcc00, 0x34c759, 0x00c7be, 0x0a84ff, 0x5e5ce6, 0xff2d92,
    0xb3261e, 0xb36200, 0xb38f00, 0x248a3d, 0x008c85, 0x005ec4, 0x3d3aa8, 0xb31f66,
    0xff8a80, 0xffc680, 0xffe680, 0x8fe3a4, 0x80e3de, 0x80bfff, 0xafaef2, 0xff96c8,
};

static int HE_ReadFile(const char* path, char* buf, int size) {
    fileHandle_t f;
    int len;

    len = trap_FS_FOpenFile(path, &f, FS_READ);
    if (!f) {
        return -1;
    }
    if (len < 0 || len > size) {
        trap_FS_FCloseFile(f);
        return -1;
    }
    trap_FS_Read(buf, len, f);
    trap_FS_FCloseFile(f);
    return len;
}

static qboolean HE_WriteFile(const char* path, const char* buf, int len) {
    fileHandle_t f;

    trap_FS_FOpenFile(path, &f, FS_WRITE);
    if (!f) {
        return qfalse;
    }
    trap_FS_Write(buf, len, f);
    trap_FS_FCloseFile(f);
    return qtrue;
}

static void HE_Status(const char* text) {
    Q_strncpyz(he.status, text, sizeof(he.status));
    he.statusTime = uiInfo.uiDC.realTime;
}

static void HE_PropString(const hdDoc_t* d, int kind, int owner, const char* key, char* out, int size) {
    int p = HD_FindProp(d, kind, owner, key);

    out[0] = 0;
    if (p >= 0 && d->props[p].xValues < d->props[p].xEnd) {
        HD_TokenString(d, d->xtoks[d->props[p].xValues].tok, out, size);
    }
}

static qboolean HE_MapFile(int f, int base, int doc) {
    hdDoc_t* d = &heDocs[doc];
    const char* file = he.files[f];
    int len, m, j, first = -1;

    len = HE_ReadFile(file, heText, sizeof(heText) - 1);
    if (len < 0) {
        return qfalse;
    }
    heText[len] = 0;
    if (!HD_Load(d, file, heText, len, HE_ReadFile)) {
        return qfalse;
    }
    if (d->numMenus != he.menusPerFile[f]) {
        return qfalse;
    }
    for (m = 0; m < d->numMenus; m++) {
        const hdMenu_t* dm = &d->menus[m];
        menuDef_t* gm = HV_Menu(base + m);

        if (!gm || Q_stricmp(dm->name, gm->window.name ? gm->window.name : "") || dm->numItems != gm->itemCount) {
            return qfalse;
        }
        for (j = 0; j < dm->numItems; j++) {
            char name[64];
            const char* gname = gm->items[j]->window.name ? gm->items[j]->window.name : "";

            HE_PropString(d, HDK_ITEM, dm->firstItem + j, "name", name, sizeof(name));
            if (Q_stricmp(name, gname)) {
                return qfalse;
            }
        }
    }
    for (j = 0; j < he.numItems; j++) {
        heItem_t* e = &he.items[j];
        int mi;

        for (mi = 0; mi < d->numMenus; mi++) {
            if (e->menu == HV_Menu(base + mi)) {
                int k;

                for (k = 0; k < e->menu->itemCount; k++) {
                    if (e->menu->items[k] == e->item) {
                        e->doc = doc;
                        e->docItem = d->menus[mi].firstItem + k;
                        e->locked = !d->items[e->docItem].local;
                        if (first < 0) {
                            first = j;
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
    return qtrue;
}

static void HE_BuildItems(void) {
    int m, j;

    he.numItems = 0;
    for (m = 0; m < HV_MenuCount(); m++) {
        menuDef_t* menu = HV_Menu(m);

        for (j = 0; j < menu->itemCount && he.numItems < HE_MAX_ITEMS; j++) {
            heItem_t* e = &he.items[he.numItems++];

            e->doc = -1;
            e->docItem = -1;
            e->menu = menu;
            e->item = menu->items[j];
            e->locked = qtrue;
            e->edited = 0;
        }
    }
}

static qboolean HE_Load(const char* listFile) {
    int f, base = 0;

    Q_strncpyz(he.list, listFile, sizeof(he.list));
    he.numFiles = HV_ReadList(listFile, he.files, HV_MAX_FILES);
    if (he.numFiles <= 0 && Q_stricmp(listFile, "ui/hud.txt")) {
        Q_strncpyz(he.list, "ui/hud.txt", sizeof(he.list));
        he.numFiles = HV_ReadList(he.list, he.files, HV_MAX_FILES);
    }
    he.numDocs = 0;
    he.numItems = 0;
    he.lockedFiles = 0;
    he.selected = -1;
    he.drag = HE_DRAG_NONE;
    he.numUndo = he.numRedo = 0;
    he.dirty = qfalse;
    he.field = -1;
    he.colorKey = -1;
    he.confirm = qfalse;
    he.scroll = 0;
    if (he.numFiles <= 0) {
        HV_Unload();
        return qfalse;
    }
    HV_Load(he.files, he.numFiles, he.menusPerFile);
    HE_BuildItems();
    for (f = 0; f < he.numFiles; f++) {
        if (he.menusPerFile[f] < 0) {
            continue;
        }
        if (he.numDocs < HE_MAX_DOCS && HE_MapFile(f, base, he.numDocs)) {
            he.docFile[he.numDocs++] = f;
        } else {
            he.lockedFiles++;
        }
        base += he.menusPerFile[f];
    }
    return qtrue;
}

static const char* HE_HudName(const char* list) {
    static char name[MAX_QPATH];
    const char* s;
    int i;

    for (i = 0; i < (int)ARRAY_LEN(heRetailHuds); i++) {
        if (!Q_stricmp(list, heRetailHuds[i].list)) {
            return heRetailHuds[i].label;
        }
    }
    s = strrchr(list, '/');
    Q_strncpyz(name, s ? s + 1 : list, sizeof(name));
    COM_StripExtension(name, name, sizeof(name));
    return name;
}

static void HE_ScanHuds(void) {
    static char buf[8192];
    const char* p;
    int i, n;

    he.numHuds = 0;
    for (i = 0; i < (int)ARRAY_LEN(heRetailHuds); i++) {
        Q_strncpyz(he.huds[he.numHuds++], heRetailHuds[i].list, MAX_QPATH);
    }
    n = trap_FS_GetFileList("hud", ".txt", buf, sizeof(buf));
    for (p = buf, i = 0; i < n && he.numHuds < HE_MAX_HUDS; i++, p += strlen(p) + 1) {
        if (p[0]) {
            Com_sprintf(he.huds[he.numHuds++], MAX_QPATH, "hud/%s", p);
        }
    }
}

static void HE_SwitchHud(const char* list) {
    he.hudMenu = qfalse;
    if (!Q_stricmp(list, he.list)) {
        return;
    }
    if (he.dirty && he.numDocs > 0) {
        Q_strncpyz(he.pending, list, sizeof(he.pending));
        he.confirm = qtrue;
        return;
    }
    HE_Load(list);
}

static float* HE_Field(itemDef_t* it, int key) {
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

static void HE_Get(int i, int key, float* v) {
    itemDef_t* it = he.items[i].item;
    float* f;
    int n;

    if (key == HEK_WIDESCREEN) {
        v[0] = it->widescreenFlag ? (float)it->widescreen : -1.0f;
        return;
    }
    f = HE_Field(it, key);
    for (n = 0; n < heKeys[key].count; n++) {
        v[n] = f[n];
    }
}

static void HE_Set(int i, int key, const float* v) {
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
        f = HE_Field(it, key);
        for (n = 0; n < heKeys[key].count; n++) {
            f[n] = v[n];
        }
    }
    if (key == HEK_RECT || key == HEK_BORDERSIZE) {
        Item_UpdatePosition(it);
    }
    if (key == HEK_TEXTSCALE || key == HEK_TEXTALIGNX || key == HEK_TEXTALIGNY) {
        it->textRect.w = 0;
        it->textRect.h = 0;
    }
    he.items[i].edited |= 1 << key;
    he.dirty = qtrue;
}

static void HE_PushStack(heUndo_t* stack, int* count, int i, int key, int group) {
    heUndo_t* u;

    if (*count >= HE_MAX_UNDO) {
        memmove(stack, stack + 1, sizeof(stack[0]) * (HE_MAX_UNDO - 1));
        (*count)--;
    }
    u = &stack[(*count)++];
    u->item = i;
    u->key = key;
    u->group = group;
    HE_Get(i, key, u->values);
}

static void HE_BeginUndo(void) {
    he.group++;
    he.numRedo = 0;
}

static void HE_PushUndo(int i, int key) {
    HE_PushStack(he.undo, &he.numUndo, i, key, he.group);
}

static void HE_Change(int i, int key, const float* v) {
    HE_BeginUndo();
    HE_PushUndo(i, key);
    HE_Set(i, key, v);
}

static void HE_Swap(heUndo_t* from, int* fromCount, heUndo_t* to, int* toCount) {
    int group;

    if (*fromCount <= 0) {
        return;
    }
    group = from[*fromCount - 1].group;
    while (*fromCount > 0 && from[*fromCount - 1].group == group) {
        heUndo_t u = from[--(*fromCount)];

        HE_PushStack(to, toCount, u.item, u.key, group);
        HE_Set(u.item, u.key, u.values);
        he.selected = u.item;
    }
}

static void HE_Undo(void) {
    if (he.numUndo <= 0) {
        HE_Status("Nothing to undo");
        return;
    }
    HE_Swap(he.undo, &he.numUndo, he.redo, &he.numRedo);
}

static void HE_Redo(void) {
    if (he.numRedo <= 0) {
        HE_Status("Nothing to redo");
        return;
    }
    HE_Swap(he.redo, &he.numRedo, he.undo, &he.numUndo);
}

static qboolean HE_Editable(int i) {
    return i >= 0 && i < he.numItems && !he.items[i].locked;
}

static int HE_ItemAnchor(int i) {
    return HV_ItemAnchor(he.items[i].item);
}

static void HE_SetAnchor(int i, int anchor) {
    itemDef_t* it = he.items[i].item;
    float cur[1], ws = (float)anchor;
    float r[4], center, x, y;
    int target = anchor < 0 ? he.items[i].menu->widescreen : anchor;

    HE_Get(i, HEK_WIDESCREEN, cur);
    if (cur[0] == ws) {
        return;
    }
    x = it->window.rect.x + it->window.rect.w * 0.5f;
    y = 0;
    HV_ToScreen(HV_ItemAnchor(it), &x, &y, NULL, NULL);
    center = x;
    HV_FromScreen(target, &center, &y);
    HE_Get(i, HEK_RECT, r);
    r[0] += (float)floor(center - it->window.rect.w * 0.5f - it->window.rect.x + 0.5f);
    HE_BeginUndo();
    HE_PushUndo(i, HEK_RECT);
    HE_PushUndo(i, HEK_WIDESCREEN);
    HE_Set(i, HEK_RECT, r);
    HE_Set(i, HEK_WIDESCREEN, &ws);
}

static qboolean HE_KeyChanged(int i, int key, float* v) {
    const heItem_t* e = &he.items[i];
    const hdDoc_t* d = &heDocs[e->doc];
    int p, n;

    if (key == HEK_WIDESCREEN && !e->item->widescreenFlag) {
        p = HD_FindProp(d, HDK_ITEM, e->docItem, heKeys[key].name);
        HE_Get(i, key, v);
        return qfalse;
    }
    HE_Get(i, key, v);
    p = HD_FindProp(d, HDK_ITEM, e->docItem, heKeys[key].name);
    if (p < 0) {
        if (key != HEK_RECT) {
            return qtrue;
        }
        return v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f || v[3] != 0.0f;
    }
    for (n = 0; n < heKeys[key].count; n++) {
        if (fabs(d->props[p].values[n] - v[n]) > 0.0001f) {
            return qtrue;
        }
    }
    return qfalse;
}

static qboolean HE_ValidName(const char* s) {
    int n = 0;

    for (; *s; s++, n++) {
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' ||
              *s == '-')) {
            return qfalse;
        }
    }
    return n > 0 && n <= 32;
}

static void HE_DefaultName(char* out, int size) {
    const char* s = he.list[0] ? he.list : "hud";
    const char* slash = strrchr(s, '/');
    char* dot;

    Q_strncpyz(out, slash ? slash + 1 : s, size);
    dot = strrchr(out, '.');
    if (dot) {
        *dot = 0;
    }
    if (!HE_ValidName(out)) {
        Q_strncpyz(out, "hud", size);
    }
}

static qboolean HE_Save(void) {
    char name[64], path[MAX_QPATH], listPath[MAX_QPATH], list[MAX_MENUDEFFILE];
    char paths[HV_MAX_FILES][MAX_QPATH];
    qboolean changed[HE_MAX_DOCS];
    int i, k, len, written = 0, keepSel = he.selected;

    HE_DefaultName(name, sizeof(name));
    for (k = 0; k < he.numDocs; k++) {
        HD_ClearEdits(&heDocs[k]);
        changed[k] = qfalse;
    }
    for (i = 0; i < he.numItems; i++) {
        heItem_t* e = &he.items[i];
        int key;

        if (e->locked || e->doc < 0 || !e->edited) {
            continue;
        }
        for (key = 0; key < HEK_COUNT; key++) {
            float v[4];

            if (!(e->edited & (1 << key)) || !HE_KeyChanged(i, key, v)) {
                continue;
            }
            if (!HD_SetValues(&heDocs[e->doc], HDK_ITEM, e->docItem, heKeys[key].name, v, heKeys[key].count)) {
                HE_Status("Could not save");
                return qfalse;
            }
            changed[e->doc] = qtrue;
        }
    }
    for (i = 0; i < he.numFiles; i++) {
        Q_strncpyz(paths[i], he.files[i], MAX_QPATH);
    }
    for (k = 0; k < he.numDocs; k++) {
        if (!changed[k]) {
            continue;
        }
        if (written == 0) {
            Com_sprintf(path, sizeof(path), "hud/%s.menu", name);
        } else {
            Com_sprintf(path, sizeof(path), "hud/%s_%d.menu", name, written + 1);
        }
        len = HD_Write(&heDocs[k], heOut, sizeof(heOut));
        if (len < 0 || !HE_WriteFile(path, heOut, len)) {
            HE_Status("Could not save");
            return qfalse;
        }
        Q_strncpyz(paths[he.docFile[k]], path, MAX_QPATH);
        written++;
    }
    if (!written) {
        he.dirty = qfalse;
        HE_Status("Nothing to save");
        return qtrue;
    }
    Q_strncpyz(list, "{\n\tloadMenu {\n", sizeof(list));
    for (i = 0; i < he.numFiles; i++) {
        Q_strcat(list, sizeof(list), va("\t\t\"%s\"\n", paths[i]));
    }
    Q_strcat(list, sizeof(list), "\t}\n}\n");
    Com_sprintf(listPath, sizeof(listPath), "hud/%s.txt", name);
    if (!HE_WriteFile(listPath, list, (int)strlen(list))) {
        HE_Status("Could not save");
        return qfalse;
    }
    trap_Cvar_Set("cg_hudFiles", listPath);
    HE_Load(listPath);
    if (keepSel < he.numItems) {
        he.selected = keepSel;
    }
    HE_Status(va("Saved as %s", listPath));
    return qtrue;
}

static float HE_PxX(float x) {
    return x * smScale;
}

static float HE_PxY(float y) {
    return smTop + y * smScale;
}

static void HE_PxFill(float x, float y, float w, float h, const float* color) {
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_SetColor(NULL);
}

static void HE_PxFrame(float x, float y, float w, float h, float t, const float* color) {
    HE_PxFill(x - t, y - t, w + 2 * t, t, color);
    HE_PxFill(x - t, y + h, w + 2 * t, t, color);
    HE_PxFill(x - t, y, t, h, color);
    HE_PxFill(x + w, y, t, h, color);
}

static void HE_MousePx(float* x, float* y) {
    *x = smMX * smScale;
    *y = smTop + smMY * smScale;
}

static qboolean HE_ItemPickable(int i) {
    if (!(he.items[i].item->window.flags & WINDOW_VISIBLE) && i != he.selected) {
        return qfalse;
    }
    return HV_ItemShown(he.items[i].item) || hvScene.showAll || i == he.selected;
}

static qboolean HE_InCanvas(float px, float py) {
    return px >= HE_PxX(he.cv[0]) && px < HE_PxX(he.cv[0] + he.cv[2]) && py >= HE_PxY(he.cv[1]) &&
           py < HE_PxY(he.cv[1] + he.cv[3]);
}

static int HE_ItemIcon(const itemDef_t* it);

static qboolean HE_TextLike(int i) {
    return HE_ItemIcon(he.items[i].item) == HE_FA_TEXT;
}

static void HE_ItemBox(int i, float* out) {
    itemDef_t* it = he.items[i].item;

    if (HE_TextLike(i) && HV_ItemPaintRect(it, out) && out[2] > 0.0f && out[3] > 0.0f) {
        return;
    }
    HV_ItemHitRect(it, out);
}

static float HE_HandlePx(void) {
    float s = 9.0f * smScale;

    return s < 6.0f ? 6.0f : s;
}

static void HE_HandlePos(const float* r, int h, float* x, float* y) {
    static const float hx[8] = {0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f};
    static const float hy[8] = {0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.5f};

    *x = r[0] + r[2] * hx[h];
    *y = r[1] + r[3] * hy[h];
}

static int HE_HandleAt(float px, float py) {
    float r[4], s = HE_HandlePx();
    int h;

    if (!HE_Editable(he.selected)) {
        return -1;
    }
    HE_ItemBox(he.selected, r);
    for (h = 0; h < 8; h++) {
        float hx, hy;

        HE_HandlePos(r, h, &hx, &hy);
        if (fabs(px - hx) <= s && fabs(py - hy) <= s) {
            return h;
        }
    }
    return -1;
}

static int HE_PickAt(float px, float py) {
    float r[4];
    int i;

    if (he.selected >= 0 && HE_ItemPickable(he.selected)) {
        HE_ItemBox(he.selected, r);
        if (px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3]) {
            int best = -1;
            float bestArea = r[2] * r[3];

            for (i = he.numItems - 1; i >= 0; i--) {
                float q[4];

                if (i == he.selected || !HE_ItemPickable(i)) {
                    continue;
                }
                HE_ItemBox(i, q);
                if (px >= q[0] && px < q[0] + q[2] && py >= q[1] && py < q[1] + q[3] && q[2] * q[3] < bestArea * 0.5f) {
                    best = i;
                    bestArea = q[2] * q[3];
                }
            }
            return best >= 0 ? best : he.selected;
        }
    }
    for (i = he.numItems - 1; i >= 0; i--) {
        float area;
        int j, best;

        if (!HE_ItemPickable(i)) {
            continue;
        }
        HE_ItemBox(i, r);
        if (!(px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3])) {
            continue;
        }
        best = i;
        area = r[2] * r[3];
        for (j = i - 1; j >= 0; j--) {
            float q[4];

            if (!HE_ItemPickable(j)) {
                continue;
            }
            HE_ItemBox(j, q);
            if (px >= q[0] && px < q[0] + q[2] && py >= q[1] && py < q[1] + q[3] && q[2] * q[3] < area * 0.5f) {
                best = j;
                area = q[2] * q[3];
            }
        }
        return best;
    }
    return -1;
}

static float HE_SnapAxis(const float* edges, int numEdges, qboolean vertical, float* guide) {
    float px, py, best = 7.0f * (smScale > 0.6f ? smScale : 0.6f), delta = 0.0f;
    int i, e, k;
    float cands[3];

    *guide = -1.0f;
    if (vertical) {
        cands[0] = HE_PxY(he.cv[1]);
        cands[1] = HE_PxY(he.cv[1] + he.cv[3] * 0.5f);
        cands[2] = HE_PxY(he.cv[1] + he.cv[3]);
    } else {
        cands[0] = HE_PxX(he.cv[0]);
        cands[1] = HE_PxX(he.cv[0] + he.cv[2] * 0.5f);
        cands[2] = HE_PxX(he.cv[0] + he.cv[2]);
    }
    for (e = 0; e < numEdges; e++) {
        for (k = 0; k < 3; k++) {
            float d = cands[k] - edges[e];

            if (fabs(d) < best) {
                best = (float)fabs(d);
                delta = d;
                *guide = cands[k];
            }
        }
    }
    for (i = 0; i < he.numItems; i++) {
        float r[4], lines[3];

        if (i == he.selected || !HE_ItemPickable(i) || !HV_ItemShown(he.items[i].item)) {
            continue;
        }
        HE_ItemBox(i, r);
        px = vertical ? r[1] : r[0];
        py = vertical ? r[3] : r[2];
        lines[0] = px;
        lines[1] = px + py * 0.5f;
        lines[2] = px + py;
        for (e = 0; e < numEdges; e++) {
            for (k = 0; k < 3; k++) {
                float d = lines[k] - edges[e];

                if (fabs(d) < best) {
                    best = (float)fabs(d);
                    delta = d;
                    *guide = lines[k];
                }
            }
        }
    }
    return delta;
}

static void HE_DragUpdate(void) {
    heItem_t* e = &he.items[he.selected];
    int anchor = HE_ItemAnchor(he.selected);
    float mx, my, ux, uy, dx, dy, r[4];
    qboolean snap = !trap_Key_IsDown(K_ALT);

    HE_MousePx(&mx, &my);
    ux = mx;
    uy = my;
    HV_FromScreen(anchor, &ux, &uy);
    dx = (float)floor(ux - he.grabX + 0.5f);
    dy = (float)floor(uy - he.grabY + 0.5f);
    if (!he.moved && fabs(dx) < 1.0f && fabs(dy) < 1.0f) {
        return;
    }
    he.moved = qtrue;
    r[0] = he.start[0];
    r[1] = he.start[1];
    r[2] = he.start[2];
    r[3] = he.start[3];
    he.guideX = he.guideY = -1.0f;
    if (he.drag == HE_DRAG_SIZE && HE_TextLike(he.selected)) {
        float sx, sy, size, grow, scale;
        int h = he.handle;

        HE_MousePx(&sx, &sy);
        sx -= he.startBox[0];
        sy -= he.startBox[1];
        if (h == 3 || h == 7) {
            size = he.startBox[2];
            grow = h == 3 ? sx - size : -sx;
        } else {
            size = he.startBox[3];
            grow = (h == 4 || h == 5 || h == 6) ? sy - size : -sy;
        }
        scale = size > 1.0f ? he.startScale * (size + grow) / size : he.startScale;
        scale = (float)floor(scale * 100.0f + 0.5f) / 100.0f;
        if (scale < 0.05f) {
            scale = 0.05f;
        }
        if (scale > 3.0f) {
            scale = 3.0f;
        }
        HE_Set(he.selected, HEK_TEXTSCALE, &scale);
        return;
    }
    if (he.drag == HE_DRAG_MOVE) {
        r[0] += dx;
        r[1] += dy;
        if (snap && he.grid) {
            r[0] = (float)floor((r[0] + he.offX) / HE_GRID + 0.5f) * HE_GRID - he.offX;
            r[1] = (float)floor((r[1] + he.offY) / HE_GRID + 0.5f) * HE_GRID - he.offY;
        }
    } else {
        int h = he.handle;
        float x0 = r[0], y0 = r[1], x1 = r[0] + r[2], y1 = r[1] + r[3];

        if (h == 0 || h == 6 || h == 7) {
            x0 += dx;
        }
        if (h == 2 || h == 3 || h == 4) {
            x1 += dx;
        }
        if (h == 0 || h == 1 || h == 2) {
            y0 += dy;
        }
        if (h == 4 || h == 5 || h == 6) {
            y1 += dy;
        }
        if (snap && he.grid) {
            if (h == 0 || h == 6 || h == 7) {
                x0 = (float)floor((x0 + he.offX) / HE_GRID + 0.5f) * HE_GRID - he.offX;
            }
            if (h == 2 || h == 3 || h == 4) {
                x1 = (float)floor((x1 + he.offX) / HE_GRID + 0.5f) * HE_GRID - he.offX;
            }
            if (h == 0 || h == 1 || h == 2) {
                y0 = (float)floor((y0 + he.offY) / HE_GRID + 0.5f) * HE_GRID - he.offY;
            }
            if (h == 4 || h == 5 || h == 6) {
                y1 = (float)floor((y1 + he.offY) / HE_GRID + 0.5f) * HE_GRID - he.offY;
            }
        }
        if (x1 - x0 < 1.0f) {
            if (h == 0 || h == 6 || h == 7) {
                x0 = x1 - 1.0f;
            } else {
                x1 = x0 + 1.0f;
            }
        }
        if (y1 - y0 < 1.0f) {
            if (h == 0 || h == 1 || h == 2) {
                y0 = y1 - 1.0f;
            } else {
                y1 = y0 + 1.0f;
            }
        }
        r[0] = x0;
        r[1] = y0;
        r[2] = x1 - x0;
        r[3] = y1 - y0;
    }
    HE_Set(he.selected, HEK_RECT, r);
    if (he.drag == HE_DRAG_MOVE && snap && !he.grid) {
        float pr[4], edges[3], d;
        int k;

        HV_ItemScreenRect(e->item, pr);
        for (k = 0; k < 4; k++) {
            pr[k] += he.boxOff[k];
        }
        edges[0] = pr[0];
        edges[1] = pr[0] + pr[2] * 0.5f;
        edges[2] = pr[0] + pr[2];
        d = HE_SnapAxis(edges, 3, qfalse, &he.guideX);
        if (he.guideX >= 0.0f) {
            r[0] += (float)floor(d * HV_UnitsPerPixel(anchor) + 0.5f);
        }
        edges[0] = pr[1];
        edges[1] = pr[1] + pr[3] * 0.5f;
        edges[2] = pr[1] + pr[3];
        d = HE_SnapAxis(edges, 3, qtrue, &he.guideY);
        if (he.guideY >= 0.0f) {
            r[1] += (float)floor(d * 480.0f / (HE_PxY(he.cv[1] + he.cv[3]) - HE_PxY(he.cv[1])) + 0.5f);
        }
        HE_Set(he.selected, HEK_RECT, r);
    }
}

static void HE_StartDrag(int mode, int handle) {
    heItem_t* e = &he.items[he.selected];
    float mx, my;

    HE_MousePx(&mx, &my);
    HV_FromScreen(HE_ItemAnchor(he.selected), &mx, &my);
    he.grabX = mx;
    he.grabY = my;
    HE_Get(he.selected, HEK_RECT, he.start);
    he.offX = e->item->window.rect.x - e->item->window.rectClient.x;
    he.offY = e->item->window.rect.y - e->item->window.rectClient.y;
    he.drag = mode;
    he.handle = handle;
    he.moved = qfalse;
    {
        float sr[4];
        int k;

        HV_ItemScreenRect(e->item, sr);
        HE_ItemBox(he.selected, he.startBox);
        for (k = 0; k < 4; k++) {
            he.boxOff[k] = he.startBox[k] - sr[k];
        }
    }
    HE_BeginUndo();
    if (mode == HE_DRAG_SIZE && HE_TextLike(he.selected)) {
        HE_Get(he.selected, HEK_TEXTSCALE, &he.startScale);
        HE_PushUndo(he.selected, HEK_TEXTSCALE);
        return;
    }
    HE_PushUndo(he.selected, HEK_RECT);
}

static void HE_EndDrag(void) {
    if ((he.drag == HE_DRAG_MOVE || he.drag == HE_DRAG_SIZE) && !he.moved && he.numUndo > 0) {
        he.numUndo--;
    }
    he.drag = HE_DRAG_NONE;
    he.guideX = he.guideY = -1.0f;
}

static void HE_Nudge(float dx, float dy, float dw, float dh) {
    float r[4];

    if (!HE_Editable(he.selected)) {
        return;
    }
    HE_Get(he.selected, HEK_RECT, r);
    r[0] += dx;
    r[1] += dy;
    r[2] += dw;
    r[3] += dh;
    if (dw != 0.0f && r[2] < 1.0f) {
        r[2] = 1.0f;
    }
    if (dh != 0.0f && r[3] < 1.0f) {
        r[3] = 1.0f;
    }
    HE_Change(he.selected, HEK_RECT, r);
}

static float HE_ColX0(void) {
    return HE_G + HE_LEFT_W + HE_G;
}

static float HE_ColX1(void) {
    return smVW - HE_G - HE_RIGHT_W - HE_G;
}

static float HE_PanelH(void) {
    return smBottom - HE_BAR - HE_G;
}

static void HE_Layout(void) {
    float x0 = HE_ColX0(), x1 = HE_ColX1();
    float y0 = HE_BAR, y1 = smBottom - HE_G - HE_STATUS_H - HE_G;
    float aspect = (float)uiInfo.uiDC.glconfig.vidWidth / (float)uiInfo.uiDC.glconfig.vidHeight;
    float w = x1 - x0, h = w / aspect;

    if (h > y1 - y0) {
        h = y1 - y0;
        w = h * aspect;
    }
    he.cv[0] = x0 + (x1 - x0 - w) * 0.5f;
    he.cv[1] = y0 + (y1 - y0 - h) * 0.5f;
    he.cv[2] = w;
    he.cv[3] = h;
    he.cv[0] = SM_Px(he.cv[0]) / smScale;
    he.cv[1] = SM_Px(he.cv[1]) / smScale;
    he.cv[2] = SM_Px(he.cv[2]) / smScale;
    he.cv[3] = SM_Px(he.cv[3]) / smScale;
    HV_SetCanvas(HE_PxX(he.cv[0]), HE_PxY(he.cv[1]), he.cv[2] * smScale, he.cv[3] * smScale);
}

static void HE_RoundPx(float x, float y, float w, float h, float r, const float* color) {
    float x0 = (float)floor(x + 0.5f), y0 = (float)floor(y + 0.5f);
    float x1 = (float)floor(x + w + 0.5f), y1 = (float)floor(y + h + 0.5f);
    qhandle_t white = uiInfo.uiDC.whiteShader;
    float rr;

    w = x1 - x0;
    h = y1 - y0;
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    rr = (float)floor(r + 0.5f);
    if (rr * 2.0f > w) {
        rr = (float)floor(w * 0.5f);
    }
    if (rr * 2.0f > h) {
        rr = (float)floor(h * 0.5f);
    }
    trap_R_SetColor(color);
    if (rr < 1.0f || !he.round) {
        trap_R_DrawStretchPic(x0, y0, w, h, 0, 0, 0, 0, white);
        trap_R_SetColor(NULL);
        return;
    }
    trap_R_DrawStretchPic(x0, y0, rr, rr, 0, 0, 0.5f, 0.5f, he.round);
    trap_R_DrawStretchPic(x1 - rr, y0, rr, rr, 0.5f, 0, 1, 0.5f, he.round);
    trap_R_DrawStretchPic(x0, y1 - rr, rr, rr, 0, 0.5f, 0.5f, 1, he.round);
    trap_R_DrawStretchPic(x1 - rr, y1 - rr, rr, rr, 0.5f, 0.5f, 1, 1, he.round);
    if (w > 2.0f * rr) {
        trap_R_DrawStretchPic(x0 + rr, y0, w - 2.0f * rr, h, 0, 0, 0, 0, white);
    }
    if (h > 2.0f * rr) {
        trap_R_DrawStretchPic(x0, y0 + rr, rr, h - 2.0f * rr, 0, 0, 0, 0, white);
        trap_R_DrawStretchPic(x1 - rr, y0 + rr, rr, h - 2.0f * rr, 0, 0, 0, 0, white);
    }
    trap_R_SetColor(NULL);
}

static void HE_Round(float x, float y, float w, float h, float r, const float* color) {
    HE_RoundPx(x * smScale, smTop + y * smScale, w * smScale, h * smScale, r * smScale, color);
}

static void HE_RoundIn(float x, float y, float w, float h, float r, const smRect_t* clip, const float* color) {
    smRect_t c = SM_R(x, y, w, h);

    if (!clip || (x >= clip->x && y >= clip->y && x + w <= clip->x + clip->w && y + h <= clip->y + clip->h)) {
        HE_Round(x, y, w, h, r, color);
    } else if (SM_Clip(&c, clip)) {
        SM_Fill(x, y, w, h, color, clip);
    }
}

static void HE_Corners(float x, float y, float w, float h, float r, const float* color) {
    float x0 = (float)floor(x + 0.5f), y0 = (float)floor(y + 0.5f);
    float x1 = (float)floor(x + w + 0.5f), y1 = (float)floor(y + h + 0.5f);
    float rr = (float)floor(r + 0.5f);

    if (!he.hole || rr < 1.0f) {
        return;
    }
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(x0, y0, rr, rr, 0, 0, 0.5f, 0.5f, he.hole);
    trap_R_DrawStretchPic(x1 - rr, y0, rr, rr, 0.5f, 0, 1, 0.5f, he.hole);
    trap_R_DrawStretchPic(x0, y1 - rr, rr, rr, 0, 0.5f, 0.5f, 1, he.hole);
    trap_R_DrawStretchPic(x1 - rr, y1 - rr, rr, rr, 0.5f, 0.5f, 1, 1, he.hole);
    trap_R_SetColor(NULL);
}

static void HE_Floating(float x, float y, float w, float h, float r, const float* color) {
    HE_Round(x - 1.0f, y + 6.0f, w + 2.0f, h + 4.0f, r + 2.0f, heShadow);
    HE_Round(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, r + 1.0f, heEdge);
    HE_Round(x, y, w, h, r, color);
}

static float HE_Base(float size, float top, float height) {
    return SM_Baseline(HE_FONT, size, top, height);
}

static void HE_Text(float x, float top, float height, float size, const float* color, const char* text,
                    qboolean bold) {
    SM_Text(x, HE_Base(size, top, height), HE_FONT, size, color, text, -1, bold);
}

static float HE_TextW(const char* text, float size, qboolean bold) {
    return SM_AdvanceBold(HE_FONT, size, text, -1, bold);
}

static void HE_TextCenter(float x, float y, float w, float h, float size, const float* color, const char* text,
                          qboolean bold) {
    HE_Text(x + (w - HE_TextW(text, size, bold)) * 0.5f, y, h, size, color, text, bold);
}

static const char* HE_Upper(const char* s) {
    static char buf[4][96];
    static int which;
    char* o = buf[which = (which + 1) & 3];
    int i;

    for (i = 0; s[i] && i < (int)sizeof(buf[0]) - 1; i++) {
        o[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 'a' + 'A') : s[i];
    }
    o[i] = 0;
    return o;
}

static float HE_CapsW(const char* text, float size) {
    const char* up = HE_Upper(text);

    return SM_SpacedWidth(HE_FONT, size, up, HE_SPACING, qtrue) - SM_LetterSpacing(HE_SPACING);
}

static void HE_Caps(float x, float top, float height, float size, const float* color, const char* text) {
    SM_SpacedText(x, HE_Base(size, top, height), HE_FONT, size, color, HE_Upper(text), HE_SPACING, qtrue);
}

static void HE_CapsCenter(float x, float y, float w, float h, float size, const float* color, const char* text) {
    HE_Caps(x + (w - HE_CapsW(text, size)) * 0.5f, y, h, size, color, text);
}

static float HE_IconW(float size, int icon) {
    return SM_IconWidth(size, icon);
}

static void HE_Icon(float x, float top, float height, float size, const float* color, int icon) {
    SM_Icon(x, SM_IconBaseline(size, top, height), size, color, icon);
}

static void HE_IconCenter(float x, float y, float w, float h, float size, const float* color, int icon) {
    HE_Icon(x + (w - HE_IconW(size, icon)) * 0.5f, y, h, size, color, icon);
}

static void HE_Tip(float x, float y, float w, float h, const char* text) {
    he.tip = text;
    he.tipX = x + w * 0.5f;
    he.tipY = y + h + 8.0f;
}

static void HE_PaintTip(void) {
    float w, h = 30.0f, x, y;

    if (!he.tip || he.confirm || he.drag != HE_DRAG_NONE) {
        return;
    }
    w = HE_TextW(he.tip, 14.0f, qfalse) + 24.0f;
    x = he.tipX - w * 0.5f;
    y = he.tipY;
    if (x < 8.0f) {
        x = 8.0f;
    }
    if (x + w > smVW - 8.0f) {
        x = smVW - 8.0f - w;
    }
    HE_Floating(x, y, w, h, 8.0f, heRowHover);
    HE_TextCenter(x, y, w, h, 14.0f, heHi, he.tip, qfalse);
}

static void HE_IconButton(float x, float y, float s, int icon, int hit, int index, qboolean on, qboolean disabled,
                          const char* tip) {
    qboolean over = SM_Over(hit, index);
    const float* fg = disabled ? heGhost : on ? heGold : over ? heHi : heMd;

    if (on) {
        HE_Round(x, y, s, s, HE_R, heRowHover);
    } else if (over && !disabled) {
        HE_Round(x, y, s, s, HE_R, heRow);
    }
    HE_IconCenter(x, y, s, s, 16.0f, fg, icon);
    SM_Hit(x, y, s, s, hit, index, NULL);
    if (over && tip) {
        HE_Tip(x, y, s, s, tip);
    }
}

enum { HE_PILL_PRIMARY, HE_PILL_SECONDARY, HE_PILL_GHOST };

static float HE_PillW(const char* text, int icon) {
    return HE_CapsW(text, 12.0f) + 36.0f + (icon ? HE_IconW(14.0f, icon) + 10.0f : 0.0f);
}

static float HE_Pill(float x, float y, float h, const char* text, int icon, const float* dot, int style, int hit,
                     int index, const char* tip) {
    float w = HE_PillW(text, icon) + (dot ? 18.0f : 0.0f);
    qboolean over = SM_Over(hit, index);
    const float* bg = NULL;
    const float* fg = heHi;
    float tx = x + 18.0f;

    if (style == HE_PILL_PRIMARY) {
        bg = over ? heGoldHover : heGold;
        fg = heGoldText;
    } else if (style == HE_PILL_SECONDARY) {
        bg = over ? heRowHover : heCard;
    } else {
        bg = over ? heRow : NULL;
        fg = over ? heHi : heMd;
    }
    if (bg) {
        HE_Round(x, y, w, h, h * 0.5f, bg);
    }
    if (dot) {
        HE_Round(tx, y + (h - 10.0f) * 0.5f, 10.0f, 10.0f, 5.0f, dot);
        tx += 18.0f;
    }
    if (icon) {
        HE_Icon(tx, y, h, 14.0f, style == HE_PILL_PRIMARY ? fg : heMd, icon);
        tx += HE_IconW(14.0f, icon) + 10.0f;
    }
    HE_Caps(tx, y, h, 12.0f, fg, text);
    SM_Hit(x, y, w, h, hit, index, NULL);
    if (over && tip) {
        HE_Tip(x, y, w, h, tip);
    }
    return w;
}

static float HE_SegmentW(const char** labels, int n, float minW) {
    float w = 8.0f;
    int i;

    for (i = 0; i < n; i++) {
        float sw = HE_CapsW(labels[i], 12.0f) + 26.0f;

        w += sw > minW ? sw : minW;
    }
    return w;
}

static void HE_Segment(float x, float y, float w, float h, const char** labels, int n, int sel, int hit,
                       qboolean enabled, qboolean stretch) {
    float natural = HE_SegmentW(labels, n, 0.0f), extra = stretch ? (w - natural) / n : 0.0f;
    float sx = x + 4.0f;
    int i;

    HE_Round(x, y, w, h, h * 0.5f, heCard);
    for (i = 0; i < n; i++) {
        float sw = HE_CapsW(labels[i], 12.0f) + 26.0f + extra;
        qboolean on = i == sel;
        qboolean over = enabled && SM_Over(hit, i);

        if (on) {
            HE_Round(sx, y + 4.0f, sw, h - 8.0f, (h - 8.0f) * 0.5f, enabled ? heGold : heRowHover);
        } else if (over) {
            HE_Round(sx, y + 4.0f, sw, h - 8.0f, (h - 8.0f) * 0.5f, heRow);
        }
        HE_CapsCenter(sx, y + 4.0f, sw, h - 8.0f, 12.0f,
                      on ? (enabled ? heGoldText : heHi) : over ? heHi : enabled ? heMd : heGhost, labels[i]);
        if (enabled) {
            SM_Hit(sx, y + 4.0f, sw, h - 8.0f, hit, i, NULL);
        }
        sx += sw;
    }
}

static void HE_PaintTop(void) {
    static const char* modes[ARRAY_LEN(heModes)];
    float y = (HE_BAR - HE_CTRL_H) * 0.5f, x, right, gw;
    int i, sel = 0;

    for (i = 0; i < (int)ARRAY_LEN(heModes); i++) {
        modes[i] = heModes[i].label;
        if (hvScene.gametype == heModes[i].gametype) {
            sel = i;
        }
    }
    x = HE_G + 6.0f;
    HE_Caps(x, 0, HE_BAR, 15.0f, heHi, "HUD Editor");
    x += HE_CapsW("HUD Editor", 15.0f) + 14.0f;
    he.hudMenuX = x;
    {
        const char* name = HE_HudName(he.list);
        float pw = HE_TextW(name, 14.0f, qtrue) + 54.0f;

        HE_Round(x, y, pw, HE_CTRL_H, HE_CTRL_H * 0.5f, SM_Over(HE_HIT_HUD, 0) || he.hudMenu ? heRowHover : heCard);
        HE_Text(x + 16.0f, y, HE_CTRL_H, 14.0f, heHi, name, qtrue);
        HE_Icon(x + pw - 28.0f, y, HE_CTRL_H, 11.0f, heMd, HE_FA_CHEVRON);
        SM_Hit(x, y, pw, HE_CTRL_H, HE_HIT_HUD, 0, NULL);
        if (SM_Over(HE_HIT_HUD, 0) && !he.hudMenu) {
            HE_Tip(x, y, pw, HE_CTRL_H, "Choose a HUD");
        }
        x += pw + 8.0f;
    }
    if (he.dirty) {
        HE_Round(x, HE_BAR * 0.5f - 4.0f, 8.0f, 8.0f, 4.0f, heGold);
    }

    x = HE_ColX0();
    gw = HE_SegmentW(modes, ARRAY_LEN(heModes), 0.0f);
    HE_Segment(x, y, gw, HE_CTRL_H, modes, ARRAY_LEN(heModes), sel, HE_HIT_MODE, qtrue, qfalse);
    x += gw + 10.0f;
    x += HE_Pill(x, y, HE_CTRL_H, hvScene.team == TEAM_BLUE ? "Blue" : "Red", 0,
                 hvScene.team == TEAM_BLUE ? heTeamBlue : heTeamRed, HE_PILL_SECONDARY, HE_HIT_TEAM, 0,
                 "Switch team") +
         10.0f;
    HE_Pill(x, y, HE_CTRL_H, heMapNames[he.map], HE_FA_MAP, NULL, HE_PILL_SECONDARY, HE_HIT_MAP, 0,
            "Change the background");

    right = smVW - HE_G;
    right -= HE_CTRL_H;
    HE_IconButton(right, y, HE_CTRL_H, HE_FA_CLOSE, HE_HIT_CLOSE, 0, qfalse, qfalse, "Close (Esc)");
    right -= 8.0f + HE_PillW("Save", HE_FA_SAVE);
    HE_Pill(right, y, HE_CTRL_H, "Save", HE_FA_SAVE, NULL, he.dirty ? HE_PILL_PRIMARY : HE_PILL_SECONDARY,
            HE_HIT_SAVE, 0, "Save (Ctrl+S)");
    right -= 12.0f + 4.0f * 36.0f + 8.0f + 13.0f;
    HE_Round(right, y, 4.0f * 36.0f + 8.0f + 13.0f, HE_CTRL_H, HE_CTRL_H * 0.5f, heCard);
    HE_IconButton(right + 4.0f, y + 2.0f, 36.0f, HE_FA_UNDO, HE_HIT_UNDO, 0, qfalse, he.numUndo == 0,
                  "Undo (Ctrl+Z)");
    HE_IconButton(right + 40.0f, y + 2.0f, 36.0f, HE_FA_REDO, HE_HIT_REDO, 0, qfalse, he.numRedo == 0,
                  "Redo (Ctrl+Y)");
    SM_Fill(right + 82.0f, y + 11.0f, 1.0f, HE_CTRL_H - 22.0f, heEdge, NULL);
    HE_IconButton(right + 89.0f, y + 2.0f, 36.0f, HE_FA_GRID, HE_HIT_GRID, 0, he.grid, qfalse, "Snap to grid (G)");
    HE_IconButton(right + 125.0f, y + 2.0f, 36.0f, hvScene.showAll ? HE_FA_EYE : HE_FA_EYE_OFF, HE_HIT_ALL, 0,
                  hvScene.showAll, qfalse, "Show hidden elements");
}

static int HE_ItemIcon(const itemDef_t* it) {
    switch (it->window.ownerDraw) {
    case CG_PLAYER_ARMOR_ICON:
    case CG_PLAYER_ARMOR_ICON2D:
    case CG_PLAYER_AMMO_ICON:
    case CG_PLAYER_AMMO_ICON2D:
    case CG_PLAYER_ITEM:
    case CG_PLAYER_HEAD:
    case CG_GAME_TYPE_ICON:
    case CG_TEAM_COLORIZED:
    case CG_ARMORTIERED_COLORIZED:
    case CG_HEALTH_COLORIZED:
    case CG_CTF_POWERUP:
    case CG_AREA_POWERUP:
    case CG_PLAYER_HASFLAG:
    case CG_PLAYER_HASFLAG2D:
    case CG_RED_FLAGSTATUS:
    case CG_BLUE_FLAGSTATUS:
    case CG_RED_BASESTATUS:
    case CG_BLUE_BASESTATUS:
    case CG_ONEFLAG_STATUS:
    case CG_IMPRESSIVE:
    case CG_EXCELLENT:
    case CG_GAUNTLET:
    case CG_PLAYER_HASKEY:
        return HE_FA_IMAGE;
    case CG_TEAM_COLOR:
    case CG_SELECTED_PLYR_TEAM_COLOR:
    case CG_PLAYER_HEALTH_BAR_100:
    case CG_PLAYER_HEALTH_BAR_200:
    case CG_PLAYER_ARMOR_BAR_100:
    case CG_PLAYER_ARMOR_BAR_200:
        return HE_FA_BOX;
    default:
        break;
    }
    if (it->window.ownerDraw && !it->window.background) {
        return HE_FA_TEXT;
    }
    if (it->text && it->text[0]) {
        return HE_FA_TEXT;
    }
    if (it->window.background) {
        return HE_FA_IMAGE;
    }
    return HE_FA_BOX;
}

static float HE_ListTop(void) {
    return HE_BAR + HE_HEAD_H;
}

static float HE_ListH(void) {
    return HE_PanelH() - HE_HEAD_H - 8.0f;
}

static void HE_Thumb(float x, float y, float size, const itemDef_t* it, qboolean selected) {
    const char* text;
    qhandle_t shader = HV_ItemThumb(it, &text);

    HE_Round(x, y, size, size, 6.0f, selected ? heEdge : heCard);
    if (shader) {
        float pad = 3.0f;

        trap_R_SetColor(NULL);
        trap_R_DrawStretchPic(HE_PxX(x + pad), HE_PxY(y + pad), (size - pad * 2.0f) * smScale,
                              (size - pad * 2.0f) * smScale, 0, 0, 1, 1, shader);
        return;
    }
    if (text && text[0] && strlen(text) <= 5) {
        float ts = 13.0f;

        while (ts > 7.0f && HE_TextW(text, ts, qtrue) > size - 4.0f) {
            ts -= 1.0f;
        }
        HE_TextCenter(x, y, size, size, ts, selected ? heHi : heMd, text, qtrue);
        return;
    }
    HE_IconCenter(x, y, size, size, 12.0f, selected ? heGold : heLo, HE_ItemIcon(it));
}

static void HE_PaintLayers(void) {
    float px = HE_G, py = HE_BAR, pw = HE_LEFT_W, ph = HE_PanelH();
    float top = HE_ListTop(), bottom = top + HE_ListH(), y;
    smRect_t clip = SM_R(px + 8.0f, top, pw - 16.0f, bottom - top);
    menuDef_t* last = NULL;
    const char* count = va("%d", he.numItems);
    int i;

    HE_Round(px, py, pw, ph, HE_R_PANEL, hePanel);
    SM_Hit(px, py, pw, ph, HE_HIT_PANEL, 0, NULL);
    HE_Caps(px + 20.0f, py, HE_HEAD_H, 12.0f, heMd, "Layers");
    HE_Text(px + pw - 20.0f - HE_TextW(count, 13.0f, qfalse), py, HE_HEAD_H, 13.0f, heLo, count, qfalse);

    y = top - he.scroll;
    for (i = 0; i < he.numItems; i++) {
        heItem_t* e = &he.items[i];
        qboolean shown = HV_ItemShown(e->item) && (e->item->window.flags & WINDOW_VISIBLE);
        qboolean selected = i == he.selected;

        if (e->menu != last) {
            last = e->menu;
            if (y + HE_GROUP_H > top && y < bottom &&
                SM_TextInside(HE_Base(11.0f, y + 6.0f, HE_GROUP_H - 6.0f), 11.0f, &clip)) {
                HE_Caps(px + 20.0f, y + 6.0f, HE_GROUP_H - 6.0f, 11.0f, heLo, HV_MenuLabel(e->menu));
            }
            y += HE_GROUP_H;
        }
        if (y + HE_ROW_H > top && y < bottom) {
            float rx = px + 8.0f, rw = pw - 16.0f, base = HE_Base(14.0f, y, HE_ROW_H);
            const float* fg = selected ? heHi : shown ? heMd : heGhost;
            float rightX = rx + rw - 12.0f;
            int fit;

            if (selected) {
                HE_RoundIn(rx, y, rw, HE_ROW_H, HE_R, &clip, heRowHover);
            } else if (SM_Over(HE_HIT_LAYER, i)) {
                HE_RoundIn(rx, y, rw, HE_ROW_H, HE_R, &clip, heRow);
            }
            if (SM_TextInside(base, 14.0f, &clip)) {
                if (e->locked) {
                    rightX -= HE_IconW(12.0f, HE_FA_LOCK);
                    HE_Icon(rightX, y, HE_ROW_H, 12.0f, heGhost, HE_FA_LOCK);
                    rightX -= 8.0f;
                }
                if (!shown) {
                    rightX -= HE_IconW(12.0f, HE_FA_EYE_OFF);
                    HE_Icon(rightX, y, HE_ROW_H, 12.0f, heGhost, HE_FA_EYE_OFF);
                    rightX -= 8.0f;
                }
                if (y >= top && y + HE_ROW_H <= bottom) {
                    HE_Thumb(rx + 5.0f, y + 4.0f, HE_ROW_H - 8.0f, e->item, selected);
                }
                fit = SM_Fit(HE_FONT, 14.0f, HV_ItemLabel(e->item), rightX - (rx + 44.0f) - 4.0f);
                SM_Text(rx + 44.0f, base, HE_FONT, 14.0f, fg, HV_ItemLabel(e->item), fit, selected);
            }
            SM_Hit(rx, y, rw, HE_ROW_H, HE_HIT_LAYER, i, &clip);
        }
        y += HE_ROW_H;
    }
    he.scrollMax = y + he.scroll - top - (bottom - top) + 8.0f;
    if (he.scrollMax < 0) {
        he.scrollMax = 0;
    }
    if (he.scroll > he.scrollMax) {
        he.scroll = he.scrollMax;
    }
}

static void HE_ScrollTo(int i) {
    float y = 0, view = HE_ListH();
    menuDef_t* last = NULL;
    int k;

    for (k = 0; k <= i && k < he.numItems; k++) {
        if (he.items[k].menu != last) {
            last = he.items[k].menu;
            y += HE_GROUP_H;
        }
        if (k < i) {
            y += HE_ROW_H;
        }
    }
    if (y - HE_GROUP_H < he.scroll) {
        he.scroll = y - HE_GROUP_H;
    } else if (y + HE_ROW_H > he.scroll + view) {
        he.scroll = y + HE_ROW_H * 2.0f - view;
    }
    if (he.scroll < 0) {
        he.scroll = 0;
    }
}

static const char* HE_FieldText(int field) {
    itemDef_t* it;

    if (he.field == field) {
        return va("%s_", he.fieldBuf);
    }
    if (he.selected < 0) {
        return "";
    }
    it = he.items[he.selected].item;
    switch (field) {
        case HF_X:
            return va("%g", it->window.rect.x);
        case HF_Y:
            return va("%g", it->window.rect.y);
        case HF_W:
            return va("%g", it->window.rect.w);
        case HF_H:
            return va("%g", it->window.rect.h);
        case HF_SCALE:
            return va("%.2f", it->textscale);
        case HF_BORDER:
            return va("%g", it->window.borderSize);
        default:
            return "";
    }
}

static void HE_ApplyField(int field, float value) {
    float r[4];
    int i = he.selected;
    itemDef_t* it;

    if (!HE_Editable(i)) {
        return;
    }
    it = he.items[i].item;
    if (field <= HF_H) {
        HE_Get(i, HEK_RECT, r);
        switch (field) {
            case HF_X:
                r[0] += value - it->window.rect.x;
                break;
            case HF_Y:
                r[1] += value - it->window.rect.y;
                break;
            case HF_W:
                r[2] = value < 0 ? 0 : value;
                break;
            default:
                r[3] = value < 0 ? 0 : value;
                break;
        }
        HE_Change(i, HEK_RECT, r);
    } else if (field == HF_SCALE) {
        if (value < 0.05f) {
            value = 0.05f;
        }
        HE_Change(i, HEK_TEXTSCALE, &value);
    } else if (field == HF_BORDER) {
        if (value < 0) {
            value = 0;
        }
        HE_Change(i, HEK_BORDERSIZE, &value);
    }
}

static void HE_StepField(int field, int dir) {
    itemDef_t* it;
    float step = trap_Key_IsDown(K_SHIFT) ? 10.0f : 1.0f;

    if (!HE_Editable(he.selected)) {
        return;
    }
    it = he.items[he.selected].item;
    switch (field) {
        case HF_X:
            HE_ApplyField(field, it->window.rect.x + dir * step);
            break;
        case HF_Y:
            HE_ApplyField(field, it->window.rect.y + dir * step);
            break;
        case HF_W:
            HE_ApplyField(field, it->window.rect.w + dir * step);
            break;
        case HF_H:
            HE_ApplyField(field, it->window.rect.h + dir * step);
            break;
        case HF_SCALE:
            HE_ApplyField(field, (float)floor((it->textscale + dir * 0.01f * step) * 100.0f + 0.5f) / 100.0f);
            break;
        case HF_BORDER:
            HE_ApplyField(field, it->window.borderSize + dir);
            break;
        default:
            break;
    }
}

static void HE_CommitField(void) {
    int field = he.field;

    he.field = -1;
    if (field >= 0 && he.fieldBuf[0]) {
        HE_ApplyField(field, (float)atof(he.fieldBuf));
    }
}

static void HE_NumberField(float x, float y, float w, const char* label, int field, qboolean editable) {
    float h = 38.0f, step = 26.0f, lw = HE_CapsW(label, 11.0f);
    qboolean focused = he.field == field;
    qboolean over = editable && SM_Over(HE_HIT_FIELD, field);

    if (focused) {
        HE_Round(x, y, w, h, HE_R, heGold);
        HE_Round(x + 1.5f, y + 1.5f, w - 3.0f, h - 3.0f, HE_R - 1.5f, heRowHover);
    } else {
        HE_Round(x, y, w, h, HE_R, over ? heRowHover : heRow);
    }
    HE_Caps(x + 12.0f, y, h, 11.0f, heLo, label);
    HE_Text(x + 12.0f + lw + 10.0f, y, h, 15.0f, editable ? heHi : heMd, HE_FieldText(field), qfalse);
    if (!editable) {
        return;
    }
    SM_Hit(x, y, w - 2.0f * step - 8.0f, h, HE_HIT_FIELD, field, NULL);
    {
        float bx = x + w - 6.0f - 2.0f * step - 2.0f, by = y + (h - step) * 0.5f;
        int k;

        for (k = 0; k < 2; k++) {
            qboolean o = SM_Over(HE_HIT_STEP, field * 2 + k);

            HE_Round(bx, by, step, step, 7.0f, o ? heRowHover : heCard);
            HE_IconCenter(bx, by, step, step, 10.0f, o ? heHi : heMd, k ? HE_FA_PLUS : HE_FA_MINUS);
            SM_Hit(bx, by, step, step, HE_HIT_STEP, field * 2 + k, NULL);
            bx += step + 2.0f;
        }
    }
}

static void HE_Swatch(float x, float y, float w, const char* label, int key, const float* color, qboolean editable) {
    float h = 38.0f;
    qboolean open = he.colorKey == key;
    qboolean over = editable && SM_Over(HE_HIT_SWATCH, key);
    const char* hex = va("#%02X%02X%02X  %d%%", (int)(Com_Clamp(0, 1, color[0]) * 255.0f + 0.5f),
                         (int)(Com_Clamp(0, 1, color[1]) * 255.0f + 0.5f),
                         (int)(Com_Clamp(0, 1, color[2]) * 255.0f + 0.5f),
                         (int)(Com_Clamp(0, 1, color[3]) * 100.0f + 0.5f));

    if (open) {
        HE_Round(x, y, w, h, HE_R, heGold);
        HE_Round(x + 1.5f, y + 1.5f, w - 3.0f, h - 3.0f, HE_R - 1.5f, heRowHover);
    } else {
        HE_Round(x, y, w, h, HE_R, over ? heRowHover : heRow);
    }
    HE_Round(x + 9.0f, y + 9.0f, 20.0f, 20.0f, 6.0f, heEdge);
    HE_Round(x + 10.0f, y + 10.0f, 18.0f, 18.0f, 5.0f, heGhost);
    HE_Round(x + 10.0f, y + 10.0f, 18.0f, 18.0f, 5.0f, color);
    HE_Text(x + 40.0f, y, h, 14.0f, editable ? heHi : heMd, label, qfalse);
    HE_Text(x + w - 12.0f - HE_TextW(hex, 13.0f, qfalse), y, h, 13.0f, heLo, hex, qfalse);
    if (editable) {
        SM_Hit(x, y, w, h, HE_HIT_SWATCH, key, NULL);
        if (open) {
            he.popup[0] = x;
            he.popup[1] = y + h + 8.0f;
        }
    }
}

static qboolean HE_HasText(const itemDef_t* it) {
    return (it->text && it->text[0]) || (it->window.ownerDraw && !it->window.background);
}

static void HE_PaintEmpty(float x, float y, float w) {
    HE_IconCenter(x, y + 40.0f, w, 50.0f, 34.0f, heGhost, HE_FA_MOVE);
    HE_TextCenter(x, y + 104.0f, w, 26.0f, 17.0f, heHi, "Select an element", qtrue);
    HE_TextCenter(x, y + 134.0f, w, 22.0f, 14.0f, heMd, "Click it on the HUD or pick it", qfalse);
    HE_TextCenter(x, y + 156.0f, w, 22.0f, 14.0f, heMd, "from the layers list.", qfalse);
}

static void HE_PaintProps(void) {
    static const char* anchors[5] = {"Menu", "Stretch", "Left", "Center", "Right"};
    float px = smVW - HE_G - HE_RIGHT_W, py = HE_BAR, ph = HE_PanelH();
    float x = px + 20.0f, w = HE_RIGHT_W - 40.0f, y = py + HE_HEAD_H, half = (w - 8.0f) * 0.5f;
    heItem_t* e;
    itemDef_t* it;
    qboolean editable;
    int anchor;

    HE_Round(px, py, HE_RIGHT_W, ph, HE_R_PANEL, hePanel);
    SM_Hit(px, py, HE_RIGHT_W, ph, HE_HIT_PANEL, 0, NULL);
    HE_Caps(px + 20.0f, py, HE_HEAD_H, 12.0f, heMd, "Properties");

    if (he.selected < 0) {
        HE_PaintEmpty(x, y, w);
        return;
    }
    e = &he.items[he.selected];
    it = e->item;
    editable = !e->locked;
    {
        float tx = x + 56.0f, tw = w - 56.0f;
        int fit = SM_Fit(HE_FONT, 18.0f, HV_ItemLabel(it), tw);
        int fit2 = SM_Fit(HE_FONT, 13.0f, va("%s  -  %s", HV_ItemKind(it), HV_MenuLabel(e->menu)), tw);

        HE_Thumb(x, y - 4.0f, 46.0f, it, qtrue);
        SM_Text(tx, HE_Base(18.0f, y - 6.0f, 30.0f), HE_FONT, 18.0f, heHi, HV_ItemLabel(it), fit, qtrue);
        SM_Text(tx, HE_Base(13.0f, y + 20.0f, 22.0f), HE_FONT, 13.0f, heLo,
                va("%s  -  %s", HV_ItemKind(it), HV_MenuLabel(e->menu)), fit2, qfalse);
    }
    y += 58.0f;
    if (!editable) {
        HE_Round(x, y, w, 60.0f, 12.0f, heCard);
        HE_Icon(x + 14.0f, y, 60.0f, 15.0f, heWarn, HE_FA_LOCK);
        HE_Text(x + 40.0f, y + 9.0f, 20.0f, 13.0f, heHi, "Read only", qtrue);
        HE_Text(x + 40.0f, y + 29.0f, 20.0f, 13.0f, heMd, "It comes from another HUD file.", qfalse);
        y += 72.0f;
    }

    HE_Caps(x, y, 18.0f, 11.0f, heLo, "Position and size");
    y += 26.0f;
    HE_NumberField(x, y, half, "X", HF_X, editable);
    HE_NumberField(x + half + 8.0f, y, half, "Y", HF_Y, editable);
    y += 46.0f;
    HE_NumberField(x, y, half, "W", HF_W, editable);
    HE_NumberField(x + half + 8.0f, y, half, "H", HF_H, editable);
    y += 58.0f;

    HE_Caps(x, y, 18.0f, 11.0f, heLo, "Screen anchor");
    y += 26.0f;
    anchor = it->widescreenFlag ? it->widescreen + 1 : 0;
    HE_Segment(x, y, w, 40.0f, anchors, 5, anchor, HE_HIT_ANCHOR, editable, qtrue);
    y += 58.0f;

    if (HE_HasText(it)) {
        HE_Caps(x, y, 18.0f, 11.0f, heLo, "Text");
        y += 26.0f;
        HE_NumberField(x, y, w, "Size", HF_SCALE, editable);
        y += 58.0f;
    }

    HE_Caps(x, y, 18.0f, 11.0f, heLo, "Colors");
    y += 26.0f;
    HE_Swatch(x, y, w, "Color", HEK_FORECOLOR, it->window.foreColor, editable);
    y += 44.0f;
    HE_Swatch(x, y, w, "Background", HEK_BACKCOLOR, it->window.backColor, editable);
    y += 44.0f;
    HE_Swatch(x, y, w, "Border", HEK_BORDERCOLOR, it->window.borderColor, editable);
    y += 46.0f;
    HE_NumberField(x, y, w, "Border size", HF_BORDER, editable);
}

static void HE_PaintPopup(void) {
    float cell = 30.0f, gap = 6.0f, pad = 16.0f;
    float w = 8.0f * cell + 7.0f * gap + 2.0f * pad, h = 4.0f * cell + 3.0f * gap + 2.0f * pad + 58.0f;
    float x = he.popup[0], y = he.popup[1], ty;
    float* color;
    int i;

    if (he.colorKey < 0 || he.selected < 0) {
        return;
    }
    color = HE_Field(he.items[he.selected].item, he.colorKey);
    if (x + w > smVW - 8.0f) {
        x = smVW - 8.0f - w;
    }
    if (y + h > smBottom - 8.0f) {
        y = he.popup[1] - 8.0f - 38.0f - 8.0f - h;
    }
    he.popup[2] = w;
    he.popup[3] = h;
    HE_Floating(x, y, w, h, 16.0f, heCard);
    SM_Hit(x, y, w, h, HE_HIT_POPUP, 0, NULL);
    for (i = 0; i < 32; i++) {
        float cx = x + pad + (i % 8) * (cell + gap), cy = y + pad + (float)(i / 8) * (cell + gap);
        vec4_t c;
        qboolean same;

        c[0] = ((hePalette[i] >> 16) & 255) / 255.0f;
        c[1] = ((hePalette[i] >> 8) & 255) / 255.0f;
        c[2] = (hePalette[i] & 255) / 255.0f;
        c[3] = 1.0f;
        same = fabs(c[0] - color[0]) < 0.003f && fabs(c[1] - color[1]) < 0.003f && fabs(c[2] - color[2]) < 0.003f;
        if (same || SM_Over(HE_HIT_PALETTE, i)) {
            HE_Round(cx - 3.0f, cy - 3.0f, cell + 6.0f, cell + 6.0f, 10.0f, same ? heGold : heHi);
            HE_Round(cx - 1.0f, cy - 1.0f, cell + 2.0f, cell + 2.0f, 8.0f, heCard);
        }
        HE_Round(cx, cy, cell, cell, 7.0f, c);
        SM_Hit(cx, cy, cell, cell, HE_HIT_PALETTE, i, NULL);
    }
    ty = y + pad + 4.0f * (cell + gap) + 4.0f;
    HE_Caps(x + pad, ty, 18.0f, 11.0f, heLo, "Opacity");
    {
        const char* pct = va("%d%%", (int)(color[3] * 100.0f + 0.5f));

        HE_Text(x + w - pad - HE_TextW(pct, 13.0f, qfalse), ty, 18.0f, 13.0f, heHi, pct, qfalse);
    }
    ty += 28.0f;
    {
        float tx = x + pad + 8.0f, tw = w - 2.0f * pad - 16.0f;
        vec4_t c;

        he.track[0] = tx;
        he.track[1] = tw;
        HE_Round(tx, ty + 7.0f, tw, 6.0f, 3.0f, heRow);
        c[0] = color[0];
        c[1] = color[1];
        c[2] = color[2];
        c[3] = 1.0f;
        HE_Round(tx, ty + 7.0f, tw * color[3], 6.0f, 3.0f, c);
        HE_Round(tx + tw * color[3] - 9.0f, ty + 1.0f, 18.0f, 18.0f, 9.0f, heHi);
        SM_Hit(tx - 8.0f, ty - 4.0f, tw + 16.0f, 28.0f, HE_HIT_ALPHA, 0, NULL);
    }
}

static void HE_PaintHudMenu(void) {
    float w = 300.0f, row = 36.0f, head = 30.0f, pad = 8.0f, x = he.hudMenuX, y = HE_BAR + 2.0f, h, ry;
    int retail = (int)ARRAY_LEN(heRetailHuds), i;

    if (!he.hudMenu) {
        return;
    }
    h = pad * 2.0f + head * 2.0f + row * (float)retail + row * (float)(he.numHuds > retail ? he.numHuds - retail : 1);
    if (y + h > smBottom - 8.0f) {
        h = smBottom - 8.0f - y;
    }
    SM_Hit(0, 0, smVW, smBottom, HE_HIT_POPUP, 1, NULL);
    HE_Floating(x, y, w, h, 14.0f, heCard);
    SM_Hit(x, y, w, h, HE_HIT_POPUP, 0, NULL);
    ry = y + pad;
    for (i = 0; i < he.numHuds; i++) {
        qboolean current = !Q_stricmp(he.huds[i], he.list);

        if (i == 0 || i == retail) {
            HE_Caps(x + 16.0f, ry, head, 11.0f, heLo, i == 0 ? "Quake Live" : "Your HUDs");
            ry += head;
        }
        if (ry + row > y + h - pad) {
            break;
        }
        if (SM_Over(HE_HIT_HUDITEM, i)) {
            HE_Round(x + 6.0f, ry, w - 12.0f, row, 9.0f, heRowHover);
        }
        HE_Icon(x + 18.0f, ry, row, 13.0f, current ? heGold : heLo, i < retail ? HE_FA_FILE : HE_FA_FOLDER);
        HE_Text(x + 44.0f, ry, row, 14.0f, current ? heHi : heMd, HE_HudName(he.huds[i]), current);
        if (current) {
            HE_Icon(x + w - 34.0f, ry, row, 12.0f, heGold, HE_FA_CHECK);
        }
        SM_Hit(x + 6.0f, ry, w - 12.0f, row, HE_HIT_HUDITEM, i, NULL);
        ry += row;
    }
    if (he.numHuds <= retail) {
        if (retail == he.numHuds) {
            HE_Caps(x + 16.0f, ry, head, 11.0f, heLo, "Your HUDs");
            ry += head;
        }
        HE_Text(x + 18.0f, ry, row, 13.0f, heGhost, "HUDs you save appear here.", qfalse);
    }
}

static void HE_PaintConfirm(void) {
    float w = 500.0f, h = 196.0f, x = (smVW - w) * 0.5f, y = (smBottom - h) * 0.5f, bx, by;

    if (!he.confirm) {
        return;
    }
    SM_Fill(0, 0, smVW, smBottom, heShade, NULL);
    SM_Hit(0, 0, smVW, smBottom, HE_HIT_POPUP, 0, NULL);
    HE_Floating(x, y, w, h, HE_R_PANEL, hePanel);
    SM_Hit(x, y, w, h, HE_HIT_POPUP, 0, NULL);
    HE_Text(x + 28.0f, y + 24.0f, 30.0f, 20.0f, heHi, "Save your changes?", qtrue);
    HE_Text(x + 28.0f, y + 62.0f, 22.0f, 15.0f, heMd, "Your HUD has changes that are not saved yet.", qfalse);
    by = y + h - 28.0f - HE_CTRL_H;
    bx = x + w - 28.0f - HE_PillW("Save", HE_FA_SAVE);
    HE_Pill(bx, by, HE_CTRL_H, "Save", HE_FA_SAVE, NULL, HE_PILL_PRIMARY, HE_HIT_CONFIRM, 0, NULL);
    bx -= 8.0f + HE_PillW("Don't save", 0);
    HE_Pill(bx, by, HE_CTRL_H, "Don't save", 0, NULL, HE_PILL_SECONDARY, HE_HIT_CONFIRM, 1, NULL);
    bx -= 8.0f + HE_PillW("Cancel", 0);
    HE_Pill(bx, by, HE_CTRL_H, "Cancel", 0, NULL, HE_PILL_GHOST, HE_HIT_CONFIRM, 2, NULL);
}

static void HE_PaintGrid(void) {
    float x, y;
    int n = 0;

    for (x = 0; x <= 640.0f; x += HE_GRID, n++) {
        float px = x, py = 0, pw = 0, ph = 480.0f;

        HV_ToScreen(WIDESCREEN_STRETCH, &px, &py, &pw, &ph);
        HE_PxFill(px, py, 1.0f, ph, n % 8 == 0 ? heGridMid : heGridLine);
    }
    n = 0;
    for (y = 0; y <= 480.0f; y += HE_GRID, n++) {
        float px = 0, py = y, pw = 640.0f, ph = 0;

        HV_ToScreen(WIDESCREEN_STRETCH, &px, &py, &pw, &ph);
        HE_PxFill(px, py, pw, 1.0f, n % 8 == 0 ? heGridMid : heGridLine);
    }
}

static void HE_CanvasLabel(const float* r, const char* text, qboolean selected) {
    float size = 12.0f, h = 22.0f, w = HE_TextW(text, size, qtrue) + 16.0f;
    float x = r[0] / smScale, y = (r[1] - smTop) / smScale - h - 6.0f;

    if (y < he.cv[1] + 4.0f) {
        y = (r[1] + r[3] - smTop) / smScale + 6.0f;
    }
    if (x + w > he.cv[0] + he.cv[2] - 4.0f) {
        x = he.cv[0] + he.cv[2] - 4.0f - w;
    }
    if (x < he.cv[0] + 4.0f) {
        x = he.cv[0] + 4.0f;
    }
    HE_Round(x, y, w, h, 6.0f, selected ? heGold : heCard);
    HE_Text(x + 8.0f, y, h, size, selected ? heGoldText : heHi, text, qtrue);
}

static void HE_PaintCanvas(void) {
    float x = HE_PxX(he.cv[0]), y = HE_PxY(he.cv[1]), w = he.cv[2] * smScale, h = he.cv[3] * smScale;
    float aspect = w / h, s1 = 0, t1 = 0, s2 = 1, t2 = 1, mx, my;
    qhandle_t shot = trap_R_RegisterShaderNoMip(va("gfx/hudeditor/%s", heMaps[he.map]));
    int hover = -1, i;

    if (aspect > 16.0f / 9.0f) {
        t1 = (1.0f - (16.0f / 9.0f) / aspect) * 0.5f;
        t2 = 1.0f - t1;
    } else {
        s1 = (1.0f - aspect / (16.0f / 9.0f)) * 0.5f;
        s2 = 1.0f - s1;
    }
    HE_PxFill(x, y, w, h, colorBlack);
    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(x, y, w, h, s1, t1, s2, t2, shot);
    HV_Paint();
    if (he.selected >= 0 && !HV_ItemShown(he.items[he.selected].item)) {
        HV_PaintItem(he.items[he.selected].item);
    }
    if (hvScene.showAll) {
        for (i = 0; i < he.numItems; i++) {
            if (!HV_ItemShown(he.items[i].item) && (he.items[i].item->window.flags & WINDOW_VISIBLE) &&
                i != he.selected) {
                HV_PaintItem(he.items[i].item);
            }
        }
    }
    if (he.grid) {
        HE_PaintGrid();
    }
    HE_Corners(x, y, w, h, 16.0f * smScale, heBg);

    HE_MousePx(&mx, &my);
    if (!he.confirm && !he.hudMenu && he.colorKey < 0 && he.drag == HE_DRAG_NONE && HE_InCanvas(mx, my) &&
        !smHasUnder) {
        hover = HE_PickAt(mx, my);
    }
    if (hover >= 0 && hover != he.selected) {
        float r[4];

        HE_ItemBox(hover, r);
        HE_PxFrame(r[0] - 1.0f, r[1] - 1.0f, r[2] + 2.0f, r[3] + 2.0f, 1.0f, heHoverLine);
        HE_CanvasLabel(r, HV_ItemLabel(he.items[hover].item), qfalse);
    }
    if (he.selected >= 0) {
        float r[4], s = HE_HandlePx();
        qboolean editable = HE_Editable(he.selected);

        HE_ItemBox(he.selected, r);
        HE_PxFrame(r[0] - 1.0f, r[1] - 1.0f, r[2] + 2.0f, r[3] + 2.0f, 1.0f, editable ? heGold : heHiddenLine);
        if (editable) {
            int hnd;

            for (hnd = 0; hnd < 8; hnd++) {
                float hx, hy;

                if ((hnd == 1 || hnd == 5) && r[2] < s * 4.0f) {
                    continue;
                }
                if ((hnd == 3 || hnd == 7) && r[3] < s * 4.0f) {
                    continue;
                }
                HE_HandlePos(r, hnd, &hx, &hy);
                HE_PxFill(hx - s * 0.5f, hy - s * 0.5f, s, s, heGold);
                HE_PxFill(hx - s * 0.5f + 1.0f, hy - s * 0.5f + 1.0f, s - 2.0f, s - 2.0f, heHi);
            }
        }
        if (he.drag == HE_DRAG_NONE) {
            HE_CanvasLabel(r, HV_ItemLabel(he.items[he.selected].item), qtrue);
        }
    }
    if (he.guideX >= 0.0f) {
        HE_PxFill(he.guideX, y, 1.0f, h, heGuide);
    }
    if (he.guideY >= 0.0f) {
        HE_PxFill(x, he.guideY, w, 1.0f, heGuide);
    }
}

static float HE_KeyHint(float x, float y, float h, const char* key, const char* what, float limit) {
    float kw = HE_CapsW(key, 10.0f) + 14.0f, ww = HE_TextW(what, 13.0f, qfalse);

    if (x + kw + 8.0f + ww > limit) {
        return -1.0f;
    }
    HE_Round(x, y + (h - 24.0f) * 0.5f, kw, 24.0f, 6.0f, heRow);
    HE_CapsCenter(x, y + (h - 24.0f) * 0.5f, kw, 24.0f, 10.0f, heMd, key);
    HE_Text(x + kw + 8.0f, y, h, 13.0f, heLo, what, qfalse);
    return x + kw + 8.0f + ww + 18.0f;
}

static void HE_PaintStatus(void) {
    static const char* hints[][2] = {
        {"Drag", "Move"}, {"Corners", "Resize"}, {"Arrows", "Nudge"}, {"Shift", "x10"}, {"Ctrl", "Resize"},
        {"Alt", "No snapping"}, {"G", "Grid"},
    };
    float x0 = HE_ColX0(), x1 = HE_ColX1(), y = smBottom - HE_G - HE_STATUS_H, x = x0 + 18.0f, limit = x1 - 18.0f;
    const char* msg = NULL;
    const float* msgColor = heHi;
    int icon = HE_FA_CHECK;
    const float* iconColor = heGold;
    int i;

    HE_Round(x0, y, x1 - x0, HE_STATUS_H, 16.0f, hePanel);
    if (he.status[0] && uiInfo.uiDC.realTime - he.statusTime < 4000) {
        msg = he.status;
    } else if (he.numFiles <= 0) {
        msg = "The HUD file could not be read.";
        icon = HE_FA_INFO;
        iconColor = heWarn;
    } else if (he.numDocs == 0) {
        msg = "This HUD can be viewed but not edited.";
        icon = HE_FA_INFO;
        iconColor = heWarn;
        msgColor = heMd;
    }
    if (msg) {
        float mw = HE_TextW(msg, 14.0f, qfalse), iw = HE_IconW(14.0f, icon);
        float mx = x1 - 18.0f - mw;

        HE_Text(mx, y, HE_STATUS_H, 14.0f, msgColor, msg, qfalse);
        HE_Icon(mx - 10.0f - iw, y, HE_STATUS_H, 14.0f, iconColor, icon);
        limit = mx - iw - 30.0f;
    }
    for (i = 0; i < (int)ARRAY_LEN(hints) && x >= 0.0f; i++) {
        x = HE_KeyHint(x, y, HE_STATUS_H, hints[i][0], hints[i][1], limit);
    }
}

void UI_HudEditor_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;
    char list[MAX_QPATH];

    trap_Cvar_VariableStringBuffer("cg_hudFiles", list, sizeof(list));
    if (!list[0]) {
        Q_strncpyz(list, "ui/hud.txt", sizeof(list));
    }
    memset(&he.undo, 0, sizeof(he.undo));
    he.guideX = he.guideY = -1.0f;
    he.open = qtrue;
    he.round = trap_R_RegisterShaderNoMip("gfx/hudeditor/round");
    he.hole = trap_R_RegisterShaderNoMip("gfx/hudeditor/hole");
    HE_Load(list);
    menu = Menus_FindByName("hudeditor");
    item = menu ? Menu_FindItemByName(menu, "hudeditor") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_HudEditor_Start(void) {
    Menus_CloseAll();
    Menus_ActivateByName("hudeditor");
}

static void HE_Close(void) {
    he.open = qfalse;
    he.confirm = qfalse;
    he.colorKey = -1;
    he.field = -1;
    he.drag = HE_DRAG_NONE;
    HV_Unload();
    Menus_CloseAll();
    Menus_ActivateByName("settings");
}

static void HE_RequestClose(void) {
    if (he.dirty && he.numDocs > 0) {
        he.confirm = qtrue;
        return;
    }
    HE_Close();
}

void UI_HudEditor_Paint(void) {
    SM_BeginFrame();
    if (!he.open) {
        UI_HudEditor_Open();
    }
    he.tip = NULL;
    HE_Layout();
    if (he.drag != HE_DRAG_NONE && !trap_Key_IsDown(K_MOUSE1)) {
        HE_EndDrag();
    }
    if ((he.drag == HE_DRAG_MOVE || he.drag == HE_DRAG_SIZE) && HE_Editable(he.selected)) {
        HE_DragUpdate();
    } else if (he.drag == HE_DRAG_ALPHA && he.selected >= 0 && he.colorKey >= 0) {
        float* c = HE_Field(he.items[he.selected].item, he.colorKey);
        float tx = he.track[0], tw = he.track[1] > 1.0f ? he.track[1] : 1.0f;
        float v[4];

        v[0] = c[0];
        v[1] = c[1];
        v[2] = c[2];
        v[3] = (float)floor(Com_Clamp(0, 1, (smMX - tx) / tw) * 100.0f + 0.5f) / 100.0f;
        HE_Set(he.selected, he.colorKey, v);
    }
    SM_FillScreen(heBg);
    HE_PaintCanvas();
    HE_PaintLayers();
    HE_PaintProps();
    HE_PaintTop();
    HE_PaintStatus();
    HE_PaintPopup();
    HE_PaintHudMenu();
    HE_PaintTip();
    HE_PaintConfirm();
    SM_EndFrame();
}

static void HE_Select(int i) {
    if (he.field >= 0) {
        HE_CommitField();
    }
    he.colorKey = -1;
    he.selected = i;
    if (i >= 0) {
        HE_ScrollTo(i);
    }
}

static void HE_AfterConfirm(void) {
    he.confirm = qfalse;
    if (he.pending[0]) {
        char list[MAX_QPATH];

        Q_strncpyz(list, he.pending, sizeof(list));
        he.pending[0] = 0;
        HE_Load(list);
        return;
    }
    HE_Close();
}

static void HE_ClickConfirm(int index) {
    if (index == 0) {
        if (HE_Save()) {
            HE_AfterConfirm();
        }
    } else if (index == 1) {
        HE_AfterConfirm();
    } else {
        he.confirm = qfalse;
        he.pending[0] = 0;
    }
}

static qboolean HE_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);
    float mx, my;
    int pick, handle;

    if (he.confirm) {
        if (hit && hit->type == HE_HIT_CONFIRM) {
            HE_ClickConfirm(hit->index);
        }
        return qtrue;
    }
    if (he.hudMenu) {
        if (hit && hit->type == HE_HIT_HUDITEM && hit->index < he.numHuds) {
            HE_SwitchHud(he.huds[hit->index]);
        } else if (!(hit && hit->type == HE_HIT_POPUP && hit->index == 0)) {
            he.hudMenu = qfalse;
        }
        return qtrue;
    }
    if (he.colorKey >= 0) {
        if (hit && hit->type == HE_HIT_PALETTE) {
            float* c = HE_Field(he.items[he.selected].item, he.colorKey);
            float v[4];

            v[0] = ((hePalette[hit->index] >> 16) & 255) / 255.0f;
            v[1] = ((hePalette[hit->index] >> 8) & 255) / 255.0f;
            v[2] = (hePalette[hit->index] & 255) / 255.0f;
            v[3] = c[3] > 0.0f ? c[3] : 1.0f;
            HE_Change(he.selected, he.colorKey, v);
            return qtrue;
        }
        if (hit && hit->type == HE_HIT_ALPHA) {
            HE_BeginUndo();
            HE_PushUndo(he.selected, he.colorKey);
            he.drag = HE_DRAG_ALPHA;
            return qtrue;
        }
        if (hit && hit->type == HE_HIT_POPUP) {
            return qtrue;
        }
        if (!(hit && hit->type == HE_HIT_SWATCH)) {
            he.colorKey = -1;
            return qtrue;
        }
    }
    if (he.field >= 0 && !(hit && hit->type == HE_HIT_FIELD && hit->index == he.field)) {
        HE_CommitField();
    }
    if (hit) {
        switch (hit->type) {
            case HE_HIT_MODE:
                hvScene.gametype = heModes[hit->index].gametype;
                return qtrue;
            case HE_HIT_TEAM:
                hvScene.team = hvScene.team == TEAM_BLUE ? TEAM_RED : TEAM_BLUE;
                return qtrue;
            case HE_HIT_MAP:
                he.map = (he.map + 1) % (int)ARRAY_LEN(heMaps);
                return qtrue;
            case HE_HIT_HUD:
                HE_ScanHuds();
                he.colorKey = -1;
                he.hudMenu = qtrue;
                return qtrue;
            case HE_HIT_UNDO:
                HE_Undo();
                return qtrue;
            case HE_HIT_REDO:
                HE_Redo();
                return qtrue;
            case HE_HIT_GRID:
                he.grid = !he.grid;
                return qtrue;
            case HE_HIT_ALL:
                hvScene.showAll = !hvScene.showAll;
                return qtrue;
            case HE_HIT_SAVE:
                HE_Save();
                return qtrue;
            case HE_HIT_CLOSE:
                HE_RequestClose();
                return qtrue;
            case HE_HIT_LAYER:
                HE_Select(hit->index);
                return qtrue;
            case HE_HIT_FIELD:
                if (he.field != hit->index) {
                    he.field = hit->index;
                    he.fieldBuf[0] = 0;
                }
                return qtrue;
            case HE_HIT_STEP:
                HE_StepField(hit->index / 2, hit->index & 1 ? 1 : -1);
                return qtrue;
            case HE_HIT_ANCHOR:
                if (HE_Editable(he.selected)) {
                    HE_SetAnchor(he.selected, hit->index - 1);
                }
                return qtrue;
            case HE_HIT_SWATCH:
                he.colorKey = he.colorKey == hit->index ? -1 : hit->index;
                return qtrue;
            default:
                return qtrue;
        }
    }
    HE_MousePx(&mx, &my);
    if (!HE_InCanvas(mx, my)) {
        return qtrue;
    }
    handle = HE_HandleAt(mx, my);
    if (handle >= 0) {
        HE_StartDrag(HE_DRAG_SIZE, handle);
        return qtrue;
    }
    pick = HE_PickAt(mx, my);
    HE_Select(pick);
    if (pick >= 0 && HE_Editable(pick)) {
        HE_StartDrag(HE_DRAG_MOVE, -1);
    }
    return qtrue;
}

static qboolean HE_FieldKey(int key) {
    if (key & K_CHAR_FLAG) {
        int ch = key & ~K_CHAR_FLAG;
        int len = (int)strlen(he.fieldBuf);

        if (((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') && len < (int)sizeof(he.fieldBuf) - 1) {
            he.fieldBuf[len] = (char)ch;
            he.fieldBuf[len + 1] = 0;
        }
        return qtrue;
    }
    if (key == K_BACKSPACE) {
        int len = (int)strlen(he.fieldBuf);

        if (len > 0) {
            he.fieldBuf[len - 1] = 0;
        }
        return qtrue;
    }
    if (key == K_ENTER || key == K_KP_ENTER || key == K_TAB) {
        int next = key == K_TAB ? (he.field + 1) % HF_COUNT : -1;

        HE_CommitField();
        if (next >= 0) {
            he.field = next;
            he.fieldBuf[0] = 0;
        }
        return qtrue;
    }
    if (key == K_ESCAPE) {
        he.field = -1;
        return qtrue;
    }
    return key != K_MOUSE1 && key != K_MOUSE2 && key != K_MWHEELUP && key != K_MWHEELDOWN;
}

qboolean UI_HudEditor_HandleKey(int key) {
    qboolean ctrl = trap_Key_IsDown(K_CTRL);
    qboolean shift = trap_Key_IsDown(K_SHIFT);
    float step = shift ? 10.0f : 1.0f;

    if (he.field >= 0 && HE_FieldKey(key)) {
        return qtrue;
    }
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
            return HE_Click();
        case K_MOUSE2:
            if (!he.confirm) {
                he.colorKey = -1;
                HE_Select(-1);
            }
            return qtrue;
        case K_MWHEELUP:
        case K_MWHEELDOWN:
            if (smMX >= HE_G && smMX < HE_G + HE_LEFT_W) {
                he.scroll += key == K_MWHEELUP ? -HE_ROW_H * 3 : HE_ROW_H * 3;
                if (he.scroll < 0) {
                    he.scroll = 0;
                }
            }
            return qtrue;
        case K_ESCAPE:
            if (he.confirm) {
                he.confirm = qfalse;
                he.pending[0] = 0;
            } else if (he.hudMenu) {
                he.hudMenu = qfalse;
            } else if (he.colorKey >= 0) {
                he.colorKey = -1;
            } else if (he.selected >= 0) {
                HE_Select(-1);
            } else {
                HE_RequestClose();
            }
            return qtrue;
        case K_LEFTARROW:
            if (ctrl) {
                HE_Nudge(0, 0, -step, 0);
            } else {
                HE_Nudge(-step, 0, 0, 0);
            }
            return qtrue;
        case K_RIGHTARROW:
            if (ctrl) {
                HE_Nudge(0, 0, step, 0);
            } else {
                HE_Nudge(step, 0, 0, 0);
            }
            return qtrue;
        case K_UPARROW:
            if (ctrl) {
                HE_Nudge(0, 0, 0, -step);
            } else {
                HE_Nudge(0, -step, 0, 0);
            }
            return qtrue;
        case K_DOWNARROW:
            if (ctrl) {
                HE_Nudge(0, 0, 0, step);
            } else {
                HE_Nudge(0, step, 0, 0);
            }
            return qtrue;
        case K_TAB:
            if (he.numItems > 0) {
                int i = he.selected, n;

                for (n = 0; n < he.numItems; n++) {
                    i = shift ? (i <= 0 ? he.numItems - 1 : i - 1) : (i + 1) % he.numItems;
                    if (HV_ItemShown(he.items[i].item) || hvScene.showAll) {
                        break;
                    }
                }
                HE_Select(i);
            }
            return qtrue;
        default:
            break;
    }
    if (ctrl && (key == 'z' || key == 'Z')) {
        if (shift) {
            HE_Redo();
        } else {
            HE_Undo();
        }
        return qtrue;
    }
    if (ctrl && (key == 'y' || key == 'Y')) {
        HE_Redo();
        return qtrue;
    }
    if (ctrl && (key == 's' || key == 'S')) {
        HE_Save();
        return qtrue;
    }
    if (!ctrl && (key == 'g' || key == 'G')) {
        he.grid = !he.grid;
        return qtrue;
    }
    return qtrue;
}

void UI_HudEditor_Escape(void) {
    UI_HudEditor_HandleKey(K_ESCAPE);
}
