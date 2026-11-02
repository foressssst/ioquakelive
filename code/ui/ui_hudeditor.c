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
#include "ui_settings.h"

void Item_UpdatePosition(itemDef_t* item);

#define HE_MAX_DOCS 3
#define HE_MAX_ITEMS 4096
#define HE_MAX_UNDO 1024
#define HE_GRID 10.0f
#define HE_FONT FONT_SANS
#define HE_G 0.0f
#define HE_M 12.0f
#define HE_BAR 60.0f
#define HE_LEFT_W 330.0f
#define HE_RIGHT_W 360.0f
#define HE_STATUS_H 44.0f
#define HE_HEAD_H 56.0f
#define HE_ROW_H 36.0f
#define HE_GROUP_H 34.0f
#define HE_R_PANEL 0.0f
#define HE_R 4.0f
#define HE_R_MAX 4.0f
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
    qboolean removed;
    int fresh;
    int slot;
} heItem_t;

#define HEK_ALIVE HEK_COUNT
#define HE_MAX_NEW 64
#define HE_CUSTOM "custom"

typedef struct {
    const char* label;
    const char* group;
    const char* name;
    int ownerDraw;
    float w, h;
    float scale;
    int textStyle;
    int align;
    int winStyle;
    const char* background;
    float color[4];
    float special;
} hePal_t;

static const hePal_t hePal[] = {
    {"Health", "Player", "health", CG_PLAYER_HEALTH, 63, 22, 0.62f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_CENTER, 0, NULL, {1, 1, 1, 1}, 0},
    {"Armor", "Player", "armor", CG_PLAYER_ARMOR_VALUE, 63, 22, 0.62f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_CENTER, 0, NULL, {1, 1, 1, 1}, 0},
    {"Ammo", "Player", "ammo", CG_PLAYER_AMMO_VALUE, 64, 32, 0.62f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_CENTER, 0, NULL, {1, 1, 1, 1}, 0},
    {"Ammo icon", "Player", "ammoicon", CG_PLAYER_AMMO_ICON2D, 22, 22, 0, 0, 0, 0, NULL, {1, 1, 1, 1}, 0},
    {"Armor icon", "Player", "armoricon", CG_PLAYER_ARMOR_ICON2D, 22, 22, 0, 0, 0, 0, NULL, {1, 1, 1, 1}, 0},
    {"Health bar", "Player", "healthbar", CG_PLAYER_HEALTH_BAR_100, 128, 16, 0, 0, 0, WINDOW_STYLE_FILLED, "ui/assets/hud/h100.tga", {1, 1, 1, 1}, 0},
    {"Armor bar", "Player", "armorbar", CG_PLAYER_ARMOR_BAR_100, 128, 16, 0, 0, 0, WINDOW_STYLE_FILLED, "ui/assets/hud/a100.tga", {1, 1, 1, 1}, 0},
    {"Holdable", "Player", "holdable", CG_PLAYER_ITEM, 25, 25, 0, 0, 0, 0, NULL, {1, 1, 1, 1}, 0},
    {"Powerups", "Player", "powerups", CG_AREA_POWERUP, 35, 35, 0, 0, 0, 0, NULL, {1, 1, 1, 1}, 4},
    {"Timer", "Match", "timer", CG_LEVELTIMER, 50, 12, 0.28f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_LEFT, 0, NULL, {1, 1, 1, 1}, 0},
    {"Round timer", "Match", "roundtimer", CG_ROUNDTIMER, 50, 12, 0.22f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_LEFT, 0, NULL, {0.73f, 0.73f, 0.73f, 0.7f}, 0},
    {"Your score", "Match", "score", CG_PLAYER_SCORE, 48, 20, 0.4f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_CENTER, 0, NULL, {1, 1, 1, 1}, 0},
    {"Red score", "Match", "redscore", CG_RED_SCORE, 48, 20, 0.4f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_CENTER, 0, NULL, {1, 1, 1, 1}, 0},
    {"Blue score", "Match", "bluescore", CG_BLUE_SCORE, 48, 20, 0.4f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_CENTER, 0, NULL, {1, 1, 1, 1}, 0},
    {"Clock", "Match", "clock", CG_LOCALTIME, 60, 12, 0.22f, ITEM_TEXTSTYLE_SHADOWED, ITEM_ALIGN_LEFT, 0, NULL, {1, 1, 1, 1}, 0},
    {"Kill feed", "Messages", "killfeed", CG_PLAYER_OBIT, 65, 12, 0.22f, 0, 0, 0, NULL, {1, 1, 1, 1}, 0},
    {"Chat", "Messages", "chat", CG_AREA_NEW_CHAT, 300, 60, 0.22f, 0, 0, 0, NULL, {1, 1, 1, 1}, 0},
    {"Box", "Shapes", "box", 0, 64, 32, 0, 0, 0, WINDOW_STYLE_FILLED, NULL, {0, 0, 0, 0.5f}, 0},
};

static const char* hePalDesc[ARRAY_LEN(hePal)] = {
    "Your health number",
    "Your armor number",
    "Ammo left in your weapon",
    "Icon of your current ammo",
    "Icon of your armor",
    "Your health as a bar",
    "Your armor as a bar",
    "The medkit or teleporter you carry",
    "Quad, regen and others, with their time",
    "The match clock",
    "Time left in the round",
    "Your points in the match",
    "Points of the red team",
    "Points of the blue team",
    "The time on your computer",
    "Who fragged who",
    "What players write",
    "A rectangle to put behind other things",
};

typedef struct {
    itemDef_t item;
    editFieldDef_t field;
    char name[32];
    int pal;
} heNew_t;

static heNew_t heNew[HE_MAX_NEW];
static int heNumNew;
static itemDef_t heGone;
static itemDef_t heScratch;
static editFieldDef_t heScratchField;
static menuDef_t heScratchMenu;

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
    HE_HIT_HIDDEN,
    HE_HIT_MAPITEM,
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
    HE_HIT_HUDITEM,
    HE_HIT_RTAB,
    HE_HIT_OPT,
    HE_HIT_OPTSTEP,
    HE_HIT_OPTROW,
    HE_HIT_LTAB,
    HE_HIT_ADD,
    HE_HIT_DELETE,
    HE_HIT_HUDACT,
    HE_HIT_SAVEAS
} heHit_t;

typedef enum {
    HE_DRAG_NONE,
    HE_DRAG_MOVE,
    HE_DRAG_SIZE,
    HE_DRAG_ALPHA,
    HE_DRAG_SCALE,
    HE_DRAG_NEW
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
    qboolean mapMenu;
    float mapMenuX;
    qboolean hiddenOpen;
    int layerHover;
    qboolean saveAs;
    qboolean afterSave;
    char saveName[33];
    char huds[HE_MAX_HUDS][MAX_QPATH];
    char hudNote[HE_MAX_HUDS][MAX_QPATH];
    int numHuds;
    int numPresets;
    int rightTab;
    float optScroll;
    float optScrollMax;
    int leftTab;
    float addScroll;
    float addScrollMax;
    float addStops[48];
    int addNumStops;
    int dragPal;
    float pressX, pressY;
    char status[160];
    int statusTime;
    float cv[4];
    const char* tip;
    float tipX, tipY;
    qhandle_t round;
} he;

static hdDoc_t heDocs[HE_MAX_DOCS];
static char heText[HD_MAX_TEXT];
static char heOut[HD_MAX_TEXT + 65536];

static const vec4_t heBg = {0.086f, 0.09f, 0.098f, 1.0f};
static const vec4_t hePanel = {0.133f, 0.137f, 0.149f, 1.0f};
static const vec4_t heCard = {0.188f, 0.196f, 0.212f, 1.0f};
static const vec4_t heRow = {0.204f, 0.212f, 0.231f, 1.0f};
static const vec4_t heRowHover = {0.275f, 0.286f, 0.31f, 1.0f};
static const vec4_t heEdge = {0.243f, 0.251f, 0.271f, 1.0f};
static const vec4_t heGold = {0.949f, 0.647f, 0.149f, 1.0f};
static const vec4_t heGoldHover = {1.0f, 0.725f, 0.275f, 1.0f};
static const vec4_t heGoldText = {0.086f, 0.09f, 0.098f, 1.0f};
static const vec4_t heHi = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t heMd = {0.851f, 0.863f, 0.882f, 1.0f};
static const vec4_t heLo = {0.651f, 0.667f, 0.694f, 1.0f};
static const vec4_t heGhost = {0.455f, 0.471f, 0.498f, 1.0f};
static const vec4_t heWarn = {0.86f, 0.42f, 0.38f, 1.0f};
static const vec4_t heShade = {0.0f, 0.0f, 0.0f, 0.6f};
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
            e->removed = qfalse;
            e->fresh = -1;
            e->slot = j;
        }
    }
}

static qboolean HE_Load(const char* listFile) {
    int f, base = 0;

    Q_strncpyz(he.list, listFile, sizeof(he.list));
    if (!Q_stricmp(COM_GetExtension(listFile), "menu")) {
        Q_strncpyz(he.files[0], listFile, MAX_QPATH);
        he.numFiles = 1;
    } else {
        he.numFiles = HV_ReadList(listFile, he.files, HV_MAX_FILES);
    }
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
    heNumNew = 0;
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

static const char* HE_RetailLabel(const char* list) {
    int i;

    for (i = 0; i < (int)ARRAY_LEN(heRetailHuds); i++) {
        if (!Q_stricmp(list, heRetailHuds[i].list)) {
            return heRetailHuds[i].label;
        }
    }
    return NULL;
}

static const char* HE_HudName(const char* list) {
    static char name[MAX_QPATH];
    const char* s = HE_RetailLabel(list);

    if (s) {
        return s;
    }
    s = strrchr(list, '/');
    Q_strncpyz(name, s ? s + 1 : list, sizeof(name));
    COM_StripExtension(name, name, sizeof(name));
    return name;
}

static qboolean HE_Loose(const char* path) {
    return trap_FS_FileTime(path) > 0 ? qtrue : qfalse;
}

static qboolean HE_Exists(const char* path) {
    fileHandle_t f;

    trap_FS_FOpenFile(path, &f, FS_READ);
    if (!f) {
        return qfalse;
    }
    trap_FS_FCloseFile(f);
    return qtrue;
}

static qboolean HE_IsHudMenu(const char* path) {
    static char buf[16384];
    fileHandle_t f;
    char* s;
    int len;

    len = trap_FS_FOpenFile(path, &f, FS_READ);
    if (!f) {
        return qfalse;
    }
    if (len > (int)sizeof(buf) - 1) {
        len = (int)sizeof(buf) - 1;
    }
    trap_FS_Read(buf, len, f);
    trap_FS_FCloseFile(f);
    buf[len] = 0;
    Q_strlwr(buf);
    for (s = strstr(buf, "fullscreen"); s; s = strstr(s + 10, "fullscreen")) {
        const char* v = s + 10;

        while (*v == ' ' || *v == '\t') {
            v++;
        }
        if (*v == '1' || !Q_strncmp(v, "menu_true", 9)) {
            return qfalse;
        }
    }
    return strstr(buf, "ownerdraw") ? qtrue : qfalse;
}

static qboolean HE_AnyLoose(char (*files)[MAX_QPATH], int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (HE_Loose(files[i])) {
            return qtrue;
        }
    }
    return qfalse;
}

static int HE_ListKey(const char* list, char (*files)[MAX_QPATH]) {
    if (!Q_stricmp(COM_GetExtension(list), "menu")) {
        Q_strncpyz(files[0], list, MAX_QPATH);
        return 1;
    }
    return HV_ReadList(list, files, HV_MAX_FILES);
}

static qboolean HE_SameHud(const char* a, const char* b) {
    char fa[HV_MAX_FILES][MAX_QPATH], fb[HV_MAX_FILES][MAX_QPATH];
    int na, nb, i;

    if (!Q_stricmp(a, b)) {
        return qtrue;
    }
    na = HE_ListKey(a, fa);
    nb = HE_ListKey(b, fb);
    if (na <= 0 || na != nb) {
        return qfalse;
    }
    for (i = 0; i < na; i++) {
        if (Q_stricmp(fa[i], fb[i])) {
            return qfalse;
        }
    }
    return qtrue;
}

static int HE_FindHud(const char* list) {
    int i;

    for (i = 0; i < he.numHuds; i++) {
        if (HE_SameHud(he.huds[i], list)) {
            return i;
        }
    }
    return -1;
}

static void HE_AddHud(const char* path, const char* note) {
    if (he.numHuds >= HE_MAX_HUDS) {
        return;
    }
    Q_strncpyz(he.huds[he.numHuds], path, MAX_QPATH);
    Q_strncpyz(he.hudNote[he.numHuds], note, MAX_QPATH);
    he.numHuds++;
}

static void HE_ReplacedNote(char (*files)[MAX_QPATH], int n, const char* path, char* note, int size) {
    char rf[HV_MAX_FILES][MAX_QPATH];
    int i, j, k, m;

    Q_strncpyz(note, path, size);
    for (i = 0; i < (int)ARRAY_LEN(heRetailHuds); i++) {
        m = HV_ReadList(heRetailHuds[i].list, rf, HV_MAX_FILES);
        for (j = 0; j < m; j++) {
            if (!HE_Loose(rf[j])) {
                continue;
            }
            for (k = 0; k < n && Q_stricmp(files[k], rf[j]); k++) {
            }
            if (k < n) {
                Q_strncpyz(note, va("%s - replaces %s", path, heRetailHuds[i].label), size);
                return;
            }
        }
    }
}

static void HE_ScanHuds(void) {
    static char buf[16384];
    static const char* exts[2] = {".cfg", ".txt"};
    static char used[HE_MAX_HUDS * 4][MAX_QPATH];
    char files[HV_MAX_FILES][MAX_QPATH];
    char path[MAX_QPATH], note[MAX_QPATH];
    const char* p;
    int i, k, e, n, m, numUsed = 0;

    he.numHuds = 0;
    for (i = 0; i < (int)ARRAY_LEN(heRetailHuds); i++) {
        m = HV_ReadList(heRetailHuds[i].list, files, HV_MAX_FILES);
        if (m > 0 && !HE_AnyLoose(files, m)) {
            HE_AddHud(heRetailHuds[i].list, "");
        }
    }
    he.numPresets = he.numHuds;
    for (e = 0; e < 2; e++) {
        n = trap_FS_GetFileList("ui", exts[e], buf, sizeof(buf));
        for (p = buf, i = 0; i < n; i++, p += strlen(p) + 1) {
            Com_sprintf(path, sizeof(path), "ui/%s", p);
            if (!p[0] || !HE_Loose(path) || HE_FindHud(path) >= 0) {
                continue;
            }
            m = HV_ReadList(path, files, HV_MAX_FILES);
            if (m <= 0 || !HE_IsHudMenu(files[0])) {
                continue;
            }
            HE_ReplacedNote(files, m, path, note, sizeof(note));
            HE_AddHud(path, note);
            for (k = 0; k < m && numUsed < (int)ARRAY_LEN(used); k++) {
                Q_strncpyz(used[numUsed++], files[k], MAX_QPATH);
            }
        }
    }
    for (i = 0; i < (int)ARRAY_LEN(heRetailHuds); i++) {
        m = HV_ReadList(heRetailHuds[i].list, files, HV_MAX_FILES);
        if (m <= 0 || !HE_AnyLoose(files, m) || HE_FindHud(heRetailHuds[i].list) >= 0) {
            continue;
        }
        HE_AddHud(heRetailHuds[i].list, va("%s - your version", files[0]));
        for (k = 0; k < m && numUsed < (int)ARRAY_LEN(used); k++) {
            Q_strncpyz(used[numUsed++], files[k], MAX_QPATH);
        }
    }
    n = trap_FS_GetFileList("ui", ".menu", buf, sizeof(buf));
    for (p = buf, i = 0; i < n; i++, p += strlen(p) + 1) {
        Com_sprintf(path, sizeof(path), "ui/%s", p);
        if (!p[0] || !HE_Loose(path)) {
            continue;
        }
        for (k = 0; k < numUsed && Q_stricmp(used[k], path); k++) {
        }
        if (k == numUsed && HE_IsHudMenu(path)) {
            HE_AddHud(path, path);
        }
    }
}

static qboolean HE_SafeMenuPath(const char* p) {
    return !Q_stricmpn(p, "ui/", 3) && !strstr(p, "..") && !strchr(p, ':') && !strchr(p, '\\') &&
                   !Q_stricmp(COM_GetExtension(p), "menu")
               ? qtrue
               : qfalse;
}

static qboolean HE_IsMine(void) {
    int k;

    if (he.numDocs <= 0) {
        return qfalse;
    }
    for (k = 0; k < he.numDocs; k++) {
        if (!HE_SafeMenuPath(he.files[he.docFile[k]]) || !HE_Loose(he.files[he.docFile[k]])) {
            return qfalse;
        }
    }
    return qtrue;
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

    if (key == HEK_ALIVE) {
        v[0] = he.items[i].removed ? 0.0f : 1.0f;
        return;
    }
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

    if (key == HEK_ALIVE) {
        heItem_t* e = &he.items[i];

        e->removed = v[0] == 0.0f;
        heGone.parent = e->menu;
        e->menu->items[e->slot] = e->removed ? &heGone : e->item;
        if (e->removed && he.selected == i) {
            he.selected = -1;
        }
        e->edited |= 1 << key;
        he.dirty = qtrue;
        return;
    }
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
        he.selected = he.items[u.item].removed ? -1 : u.item;
    }
}

static void HE_Undo(void) {
    if (he.numUndo <= 0) {
        HE_Status("Nothing to undo");
        return;
    }
    HE_Swap(he.undo, &he.numUndo, he.redo, &he.numRedo);
    he.status[0] = 0;
}

static void HE_Redo(void) {
    if (he.numRedo <= 0) {
        HE_Status("Nothing to redo");
        return;
    }
    HE_Swap(he.redo, &he.numRedo, he.undo, &he.numUndo);
    he.status[0] = 0;
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

static qboolean HE_NameFree(const char* name) {
    return !HE_Exists(va("ui/%s.menu", name)) && !HE_Exists(va("ui/%s.cfg", name)) ? qtrue : qfalse;
}

static void HE_DefaultName(char* out, int size) {
    const char* s = he.list[0] && !HE_RetailLabel(he.list) ? he.list : "my_hud";
    const char* slash = strrchr(s, '/');
    char base[33];
    char* dot;
    int i;

    Q_strncpyz(base, slash ? slash + 1 : s, sizeof(base) - 2);
    dot = strrchr(base, '.');
    if (dot) {
        *dot = 0;
    }
    if (!HE_ValidName(base)) {
        Q_strncpyz(base, "my_hud", sizeof(base));
    }
    Q_strncpyz(out, base, size);
    for (i = 2; i < 100 && !HE_NameFree(out); i++) {
        Com_sprintf(out, size, "%s%d", base, i);
    }
}

static void HE_Num(char* out, int size, float v) {
    Com_sprintf(out, size, "%.4f", v);
    while (strchr(out, '.') && (out[strlen(out) - 1] == '0' || out[strlen(out) - 1] == '.')) {
        out[strlen(out) - 1] = 0;
    }
    if (!strcmp(out, "-0")) {
        Q_strncpyz(out, "0", size);
    }
}

static void HE_NumList(char* out, int size, const float* v, int n) {
    char num[32];
    int k;

    out[0] = 0;
    for (k = 0; k < n; k++) {
        HE_Num(num, sizeof(num), v[k]);
        Q_strcat(out, size, k ? va(" %s", num) : num);
    }
}

static void HE_ItemText(const heNew_t* n, char* out, int size) {
    const itemDef_t* it = &n->item;
    const hePal_t* p = &hePal[n->pal];
    char v[96];

    Com_sprintf(out, size, "\t\titemDef {\n\t\t\tname \"%s\"\n", n->name);
    HE_NumList(v, sizeof(v), &it->window.rectClient.x, 4);
    Q_strcat(out, size, va("\t\t\trect %s\n", v));
    if (it->widescreenFlag) {
        Q_strcat(out, size, va("\t\t\twidescreen %d\n", it->widescreen));
    }
    Q_strcat(out, size, "\t\t\tvisible 1\n\t\t\tdecoration\n");
    if (it->window.style) {
        Q_strcat(out, size, va("\t\t\tstyle %d\n", it->window.style));
    }
    if (p->background) {
        Q_strcat(out, size, va("\t\t\tbackground \"%s\"\n", p->background));
    }
    if (it->window.ownerDraw) {
        Q_strcat(out, size, va("\t\t\townerdraw %d\n", it->window.ownerDraw));
    }
    if (p->scale > 0.0f) {
        HE_Num(v, sizeof(v), it->textscale);
        Q_strcat(out, size, va("\t\t\ttextscale %s\n\t\t\ttextstyle %d\n\t\t\talign %d\n", v, it->textStyle,
                               it->alignment));
    }
    if (it->special != 0.0f) {
        HE_Num(v, sizeof(v), it->special);
        Q_strcat(out, size, va("\t\t\tspecial %s\n", v));
    }
    if (it->textalignx != 0.0f || it->textaligny != 0.0f) {
        HE_NumList(v, sizeof(v), &it->textalignx, 1);
        Q_strcat(out, size, va("\t\t\ttextalignx %s\n", v));
        HE_NumList(v, sizeof(v), &it->textaligny, 1);
        Q_strcat(out, size, va("\t\t\ttextaligny %s\n", v));
    }
    HE_NumList(v, sizeof(v), it->window.foreColor, 4);
    Q_strcat(out, size, va("\t\t\tforecolor %s\n", v));
    HE_NumList(v, sizeof(v), it->window.backColor, 4);
    Q_strcat(out, size, va("\t\t\tbackcolor %s\n", v));
    if (it->window.border && it->window.borderSize > 0.0f) {
        Q_strcat(out, size, va("\t\t\tborder %d\n", it->window.border));
        HE_Num(v, sizeof(v), it->window.borderSize);
        Q_strcat(out, size, va("\t\t\tbordersize %s\n", v));
        HE_NumList(v, sizeof(v), it->window.borderColor, 4);
        Q_strcat(out, size, va("\t\t\tbordercolor %s\n", v));
    }
    Q_strcat(out, size, "\t\t}\n");
}

static int HE_SaveNew(void) {
    static char block[32768];
    static char wrap[33024];
    char one[2048];
    int i, k, doc = -1, at = -1;
    qboolean any = qfalse;

    block[0] = 0;
    for (i = 0; i < he.numItems; i++) {
        const heItem_t* e = &he.items[i];

        if (e->fresh < 0 || e->removed) {
            continue;
        }
        HE_ItemText(&heNew[e->fresh], one, sizeof(one));
        if (strlen(block) + strlen(one) + 1 >= sizeof(block)) {
            return -2;
        }
        Q_strcat(block, sizeof(block), one);
        any = qtrue;
    }
    if (!any) {
        return -1;
    }
    for (k = 0; k < he.numDocs && doc < 0; k++) {
        for (i = 0; i < heDocs[k].numMenus; i++) {
            if (heDocs[k].menus[i].local && !Q_stricmp(heDocs[k].menus[i].name, HE_CUSTOM)) {
                doc = k;
                at = heDocs[k].menus[i].closeFrom;
                break;
            }
        }
    }
    if (doc >= 0) {
        Com_sprintf(wrap, sizeof(wrap), "%s\t", block + 1);
        return HD_Insert(&heDocs[doc], at, wrap) ? doc : -2;
    }
    if (he.numDocs <= 0 || heDocs[0].endFrom < 0) {
        return -2;
    }
    Com_sprintf(wrap, sizeof(wrap),
                "\tmenuDef {\n\t\tname \"%s\"\n\t\tfullScreen 0\n\t\tvisible 1\n\t\trect 0 0 640 480\n\n%s\t}\n",
                HE_CUSTOM, block);
    return HD_Insert(&heDocs[0], heDocs[0].endFrom, wrap) ? 0 : -2;
}

static void HE_OpenSaveAs(void);

static qboolean HE_UseList(const char* menuPath, char* listPath, int size) {
    char files[HV_MAX_FILES][MAX_QPATH];
    char list[MAX_QPATH * 2];

    Q_strncpyz(listPath, menuPath, size);
    COM_StripExtension(listPath, listPath, size);
    Q_strcat(listPath, size, ".cfg");
    if (!HE_Exists(listPath)) {
        Com_sprintf(list, sizeof(list), "{\n\tloadMenu {\n\t\t\"%s\"\n\t}\n}\n", menuPath);
        return HE_WriteFile(listPath, list, (int)strlen(list));
    }
    return HV_ReadList(listPath, files, HV_MAX_FILES) == 1 && !Q_stricmp(files[0], menuPath) ? qtrue : qfalse;
}

static qboolean HE_SaveNamed(const char* as) {
    char name[64], path[MAX_QPATH], listPath[MAX_QPATH], list[MAX_MENUDEFFILE];
    char paths[HV_MAX_FILES][MAX_QPATH];
    qboolean changed[HE_MAX_DOCS];
    int i, k, len, written = 0, keepSel = he.selected;

    if (!as && !HE_IsMine()) {
        HE_OpenSaveAs();
        return qfalse;
    }
    if (as) {
        Q_strncpyz(name, as, sizeof(name));
    }
    for (k = 0; k < he.numDocs; k++) {
        HD_ClearEdits(&heDocs[k]);
        changed[k] = as != NULL;
    }
    for (i = 0; i < he.numItems; i++) {
        heItem_t* e = &he.items[i];
        int key;

        if (e->locked || e->doc < 0 || !e->edited) {
            continue;
        }
        if (e->removed) {
            if (!HD_RemoveItem(&heDocs[e->doc], e->docItem)) {
                HE_Status("Could not save");
                return qfalse;
            }
            changed[e->doc] = qtrue;
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
    k = HE_SaveNew();
    if (k == -2) {
        HE_Status("Could not save");
        return qfalse;
    }
    if (k >= 0) {
        changed[k] = qtrue;
    }
    for (i = 0; i < he.numFiles; i++) {
        Q_strncpyz(paths[i], he.files[i], MAX_QPATH);
    }
    for (k = 0; k < he.numDocs; k++) {
        if (!changed[k]) {
            continue;
        }
        if (!as) {
            Q_strncpyz(path, he.files[he.docFile[k]], sizeof(path));
        } else if (written == 0) {
            Com_sprintf(path, sizeof(path), "ui/%s.menu", name);
        } else {
            Com_sprintf(path, sizeof(path), "ui/%s_%d.menu", name, written + 1);
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
    if (!as) {
        if (Q_stricmp(COM_GetExtension(he.list), "menu")) {
            Q_strncpyz(listPath, he.list, sizeof(listPath));
        } else if (!HE_UseList(he.list, listPath, sizeof(listPath))) {
            listPath[0] = 0;
        }
        if (listPath[0]) {
            trap_Cvar_Set("cg_hudFiles", listPath);
            HE_Load(listPath);
        } else {
            HE_Load(he.list);
        }
        if (keepSel < he.numItems) {
            he.selected = keepSel;
        }
        HE_Status("Saved");
        return qtrue;
    }
    Q_strncpyz(list, "{\n\tloadMenu {\n", sizeof(list));
    for (i = 0; i < he.numFiles; i++) {
        Q_strcat(list, sizeof(list), va("\t\t\"%s\"\n", paths[i]));
    }
    Q_strcat(list, sizeof(list), "\t}\n}\n");
    Com_sprintf(listPath, sizeof(listPath), "ui/%s.cfg", name);
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

static qboolean HE_Save(void) {
    return HE_SaveNamed(NULL);
}

static void HE_AfterConfirm(void);

static void HE_OpenSaveAs(void) {
    HE_DefaultName(he.saveName, sizeof(he.saveName));
    he.afterSave = he.confirm;
    he.confirm = qfalse;
    he.hudMenu = qfalse;
    he.colorKey = -1;
    he.saveAs = qtrue;
}

static void HE_DoSaveAs(void) {
    if (!HE_ValidName(he.saveName)) {
        HE_Status("Use letters, numbers, - and _ in the name.");
        return;
    }
    if (!HE_NameFree(he.saveName)) {
        HE_Status("That name is taken. Pick another one.");
        return;
    }
    he.saveAs = qfalse;
    if (HE_SaveNamed(he.saveName) && he.afterSave) {
        he.afterSave = qfalse;
        HE_AfterConfirm();
    }
}

static qboolean HE_SaveAsKey(int key) {
    int len = (int)strlen(he.saveName);

    if (key & K_CHAR_FLAG) {
        int ch = key & ~K_CHAR_FLAG;

        if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
             ch == '-') &&
            len < (int)sizeof(he.saveName) - 1) {
            he.saveName[len] = (char)ch;
            he.saveName[len + 1] = 0;
        } else if (ch == ' ' && len < (int)sizeof(he.saveName) - 1) {
            he.saveName[len] = '_';
            he.saveName[len + 1] = 0;
        }
        return qtrue;
    }
    if (key == K_BACKSPACE) {
        if (len > 0) {
            he.saveName[len - 1] = 0;
        }
        return qtrue;
    }
    if (key == K_ENTER || key == K_KP_ENTER) {
        HE_DoSaveAs();
        return qtrue;
    }
    if (key == K_ESCAPE) {
        he.saveAs = qfalse;
        return qtrue;
    }
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
    if (he.items[i].removed) {
        return qfalse;
    }
    if (!(he.items[i].item->window.flags & WINDOW_VISIBLE) && i != he.selected) {
        return qfalse;
    }
    if (HV_ItemInvisible(he.items[i].item) && i != he.selected) {
        return qfalse;
    }
    return HV_ItemShown(he.items[i].item) || hvScene.showAll || i == he.selected;
}

static qboolean HE_InCanvas(float px, float py) {
    return px >= HE_PxX(he.cv[0]) && px < HE_PxX(he.cv[0] + he.cv[2]) && py >= HE_PxY(he.cv[1]) &&
           py < HE_PxY(he.cv[1] + he.cv[3]);
}

static void HE_NewInit(itemDef_t* it, editFieldDef_t* field, const hePal_t* p, menuDef_t* menu, const char* name) {
    Item_Init(it);
    it->window.name = name;
    it->parent = menu;
    it->window.flags = WINDOW_VISIBLE | WINDOW_DECORATION;
    it->window.rectClient.w = p->w;
    it->window.rectClient.h = p->h;
    if (p->scale > 0.0f) {
        it->textscale = p->scale;
    }
    it->textStyle = p->textStyle;
    it->alignment = p->align;
    it->special = p->special;
    it->window.style = p->winStyle;
    if (p->background) {
        it->window.background = trap_R_RegisterShaderNoMip(p->background);
    }
    if (p->ownerDraw) {
        it->type = ITEM_TYPE_OWNERDRAW;
        it->window.ownerDraw = p->ownerDraw;
        Vector4Copy(p->color, it->window.foreColor);
        if (!p->winStyle) {
            Vector4Copy(p->color, it->window.backColor);
        }
    } else {
        memset(field, 0, sizeof(*field));
        it->type = ITEM_TYPE_TEXT;
        it->typeData = field;
        Vector4Copy(p->color, it->window.backColor);
    }
    Item_UpdatePosition(it);
}

static menuDef_t* HE_CustomMenu(void) {
    menuDef_t* m = HV_FindMenu(HE_CUSTOM);

    return m ? m : HV_AddMenu(HE_CUSTOM);
}

static int HE_DropAnchor(float px) {
    float f = (px - HE_PxX(he.cv[0])) / (he.cv[2] * smScale);

    return f < 0.34f ? WIDESCREEN_LEFT : f > 0.66f ? WIDESCREEN_RIGHT : WIDESCREEN_CENTER;
}

static int HE_NewAnchor(int pal, float px) {
    return hePal[pal].ownerDraw == CG_AREA_NEW_CHAT ? WIDESCREEN_LEFT : HE_DropAnchor(px);
}

static void HE_PlaceNew(itemDef_t* it, float px, float py, int anchor, const float* box) {
    menuDef_t* menu = (menuDef_t*)it->parent;
    float x = px, y = py;

    HV_FromScreen(anchor, &x, &y);
    it->widescreen = anchor;
    it->widescreenFlag = 1;
    it->window.rectClient.x = (float)floor(x - box[0] - box[2] * 0.5f - menu->window.rect.x + 0.5f);
    it->window.rectClient.y = (float)floor(y - box[1] - box[3] * 0.5f - menu->window.rect.y + 0.5f);
    Item_UpdatePosition(it);
}

static void HE_PalBox(int pal, float* box);
static void HE_RestoreCanvas(void);

static int HE_AddItem(int pal, float px, float py) {
    static const float alive = 1.0f;
    const hePal_t* p = &hePal[pal];
    menuDef_t* menu;
    heNew_t* n;
    heItem_t* e;
    float box[4];
    int i, count = 0;

    if (he.numDocs <= 0) {
        HE_Status("This HUD can be viewed but not edited.");
        return -1;
    }
    menu = HE_CustomMenu();
    if (!menu || heNumNew >= HE_MAX_NEW || he.numItems >= HE_MAX_ITEMS || menu->itemCount >= MAX_MENUITEMS) {
        HE_Status("No room for more elements.");
        return -1;
    }
    for (i = 0; i < heNumNew; i++) {
        count += heNew[i].pal == pal;
    }
    n = &heNew[heNumNew];
    n->pal = pal;
    Com_sprintf(n->name, sizeof(n->name), "%s%d", p->name, count + 1);
    HE_PalBox(pal, box);
    HE_RestoreCanvas();
    HE_NewInit(&n->item, &n->field, p, menu, n->name);
    HE_PlaceNew(&n->item, px, py, HE_NewAnchor(pal, px), box);
    menu->items[menu->itemCount] = &n->item;

    i = he.numItems++;
    e = &he.items[i];
    e->doc = -1;
    e->docItem = -1;
    e->menu = menu;
    e->item = &n->item;
    e->locked = qfalse;
    e->edited = 0;
    e->removed = qtrue;
    e->fresh = heNumNew++;
    e->slot = menu->itemCount++;
    HE_BeginUndo();
    HE_PushUndo(i, HEK_ALIVE);
    HE_Set(i, HEK_ALIVE, &alive);
    return i;
}

static qboolean HE_Removable(int i) {
    const heItem_t* e;

    if (!HE_Editable(i)) {
        return qfalse;
    }
    e = &he.items[i];
    return e->fresh >= 0 || (e->doc >= 0 && e->docItem >= 0 && heDocs[e->doc].items[e->docItem].srcFrom >= 0);
}

static void HE_Remove(int i) {
    static const float gone = 0.0f;

    if (!HE_Removable(i)) {
        HE_Status("This element can't be deleted.");
        return;
    }
    HE_BeginUndo();
    HE_PushUndo(i, HEK_ALIVE);
    HE_Set(i, HEK_ALIVE, &gone);
    HE_Status("Deleted. Ctrl+Z brings it back.");
}


static qboolean HE_TextLike(int i) {
    const itemDef_t* it = he.items[i].item;

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
        case CG_TEAM_COLOR:
        case CG_SELECTED_PLYR_TEAM_COLOR:
        case CG_PLAYER_HEALTH_BAR_100:
        case CG_PLAYER_HEALTH_BAR_200:
        case CG_PLAYER_ARMOR_BAR_100:
        case CG_PLAYER_ARMOR_BAR_200:
            return qfalse;
        default:
            break;
    }
    if (it->window.ownerDraw && !it->window.background) {
        return qtrue;
    }
    return it->text && it->text[0] ? qtrue : qfalse;
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
    return HE_LEFT_W + HE_M;
}

static float HE_ColX1(void) {
    return smVW - HE_RIGHT_W - HE_M;
}

static float HE_PanelH(void) {
    return smBottom - HE_BAR;
}

static void HE_Layout(void) {
    float x0 = HE_ColX0(), x1 = HE_ColX1();
    float y0 = HE_BAR + HE_M, y1 = smBottom - HE_STATUS_H - HE_M;
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
    if (r > HE_R_MAX) {
        r = HE_R_MAX;
    }
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

static void HE_Floating(float x, float y, float w, float h, float r, const float* color) {
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

static float HE_CapsW(const char* text, float size) {
    return HE_TextW(text, size + 2.0f, qtrue);
}

static void HE_Caps(float x, float top, float height, float size, const float* color, const char* text) {
    HE_Text(x, top, height, size + 2.0f, color, text, qtrue);
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
            HE_Round(sx, y + 4.0f, sw, h - 8.0f, 3.0f, heRowHover);
            if (enabled) {
                SM_Fill(sx, y + h - 6.0f, sw, 2.0f, heGold, NULL);
            }
        } else if (over) {
            HE_Round(sx, y + 4.0f, sw, h - 8.0f, 3.0f, heRow);
        }
        HE_CapsCenter(sx, y + 4.0f, sw, h - 8.0f, 12.0f, on ? heHi : over ? heHi : enabled ? heMd : heGhost,
                      labels[i]);
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
    SM_Fill(0.0f, 0.0f, smVW, HE_BAR, hePanel, NULL);
    SM_Fill(0.0f, HE_BAR - 1.0f, smVW, 1.0f, heEdge, NULL);
    x = HE_M;
    he.hudMenuX = x;
    {
        const char* name = HE_HudName(he.list);
        float pw = HE_TextW(name, 15.0f, qtrue) + 54.0f;

        HE_Round(x, y, pw, HE_CTRL_H, HE_R, SM_Over(HE_HIT_HUD, 0) || he.hudMenu ? heRowHover : heCard);
        HE_Text(x + 16.0f, y, HE_CTRL_H, 15.0f, heHi, name, qtrue);
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
    he.mapMenuX = x;
    HE_Pill(x, y, HE_CTRL_H, heMapNames[he.map], HE_FA_MAP, NULL, HE_PILL_SECONDARY, HE_HIT_MAP, 0,
            "Change the background");

    right = smVW - HE_M;
    right -= HE_CTRL_H;
    HE_IconButton(right, y, HE_CTRL_H, HE_FA_CLOSE, HE_HIT_CLOSE, 0, qfalse, qfalse, "Close (Esc)");
    right -= 8.0f + HE_PillW("Save", HE_FA_SAVE);
    HE_Pill(right, y, HE_CTRL_H, "Save", HE_FA_SAVE, NULL, he.dirty ? HE_PILL_PRIMARY : HE_PILL_SECONDARY,
            HE_HIT_SAVE, 0, "Save (Ctrl+S)");
    right -= 12.0f + 4.0f * 36.0f + 8.0f + 13.0f;
    HE_Round(right, y, 4.0f * 36.0f + 8.0f + 13.0f, HE_CTRL_H, HE_R, heCard);
    HE_IconButton(right + 4.0f, y + 2.0f, 36.0f, HE_FA_UNDO, HE_HIT_UNDO, 0, qfalse, he.numUndo == 0,
                  "Undo (Ctrl+Z)");
    HE_IconButton(right + 40.0f, y + 2.0f, 36.0f, HE_FA_REDO, HE_HIT_REDO, 0, qfalse, he.numRedo == 0,
                  "Redo (Ctrl+Y)");
    SM_Fill(right + 82.0f, y + 11.0f, 1.0f, HE_CTRL_H - 22.0f, heEdge, NULL);
    HE_IconButton(right + 89.0f, y + 2.0f, 36.0f, HE_FA_GRID, HE_HIT_GRID, 0, he.grid, qfalse, "Snap to grid (G)");
    HE_IconButton(right + 125.0f, y + 2.0f, 36.0f, hvScene.showAll ? HE_FA_EYE : HE_FA_EYE_OFF, HE_HIT_ALL, 0,
                  hvScene.showAll, qfalse, "Show hidden elements");
}

static float HE_ListTop(void) {
    return HE_BAR + HE_HEAD_H;
}

static float HE_ListH(void) {
    return HE_PanelH() - HE_HEAD_H - 8.0f;
}

static void HE_RestoreCanvas(void) {
    HV_SetCanvas(HE_PxX(he.cv[0]), HE_PxY(he.cv[1]), he.cv[2] * smScale, he.cv[3] * smScale);
}

static itemDef_t* HE_Scratch(int pal) {
    if (!heScratchMenu.window.rect.w) {
        Menu_Init(&heScratchMenu);
        heScratchMenu.window.rect.w = heScratchMenu.window.rectClient.w = 640.0f;
        heScratchMenu.window.rect.h = heScratchMenu.window.rectClient.h = 480.0f;
        heScratchMenu.window.flags |= WINDOW_VISIBLE;
    }
    HE_NewInit(&heScratch, &heScratchField, &hePal[pal], &heScratchMenu, hePal[pal].name);
    return &heScratch;
}

static void HE_PalBox(int pal, float* box) {
    itemDef_t* it = HE_Scratch(pal);
    float far = -8192.0f, b[4];

    HV_SetCanvas(far, far, 640.0f, 480.0f);
    if (HV_PaintLoose(it, b) && b[2] > 0.5f && b[3] > 0.5f) {
        box[0] = b[0] - far - it->window.rect.x;
        box[1] = b[1] - far - it->window.rect.y;
        box[2] = b[2];
        box[3] = b[3];
    } else {
        box[0] = 0.0f;
        box[1] = 0.0f;
        box[2] = it->window.rect.w;
        box[3] = it->window.rect.h;
    }
}

static void HE_PalPreview(int pal, float x, float y, float w, float h) {
    float box[4], s, cx = HE_PxX(x + w * 0.5f), cy = HE_PxY(y + h * 0.5f);

    HE_PalBox(pal, box);
    s = (w * 0.8f / box[2] < h * 0.7f / box[3] ? w * 0.8f / box[2] : h * 0.7f / box[3]) * smScale;
    if (s > 3.0f * smScale) {
        s = 3.0f * smScale;
    }
    HV_SetCanvas(cx - (box[0] + box[2] * 0.5f) * s, cy - (box[1] + box[3] * 0.5f) * s, 640.0f * s, 480.0f * s);
    if (HV_PaintLoose(&heScratch, box) && !hePal[pal].ownerDraw) {
        HE_PxFill(box[0], box[1], box[2], 1.0f, heLo);
        HE_PxFill(box[0], box[1] + box[3] - 1.0f, box[2], 1.0f, heLo);
        HE_PxFill(box[0], box[1], 1.0f, box[3], heLo);
        HE_PxFill(box[0] + box[2] - 1.0f, box[1], 1.0f, box[3], heLo);
    }
}

static void HE_AddStop(float at) {
    if (he.addNumStops < (int)ARRAY_LEN(he.addStops)) {
        he.addStops[he.addNumStops++] = at;
    }
}

static void HE_ScrollAdd(int dir) {
    int k;

    if (dir > 0) {
        for (k = 0; k < he.addNumStops; k++) {
            if (he.addStops[k] > he.addScroll + 0.5f) {
                he.addScroll = he.addStops[k];
                break;
            }
        }
    } else {
        float to = 0.0f;

        for (k = 0; k < he.addNumStops; k++) {
            if (he.addStops[k] < he.addScroll - 0.5f) {
                to = he.addStops[k];
            }
        }
        he.addScroll = to;
    }
    if (he.addScroll > he.addScrollMax) {
        he.addScroll = he.addScrollMax;
    }
    if (he.addScroll < 0.0f) {
        he.addScroll = 0.0f;
    }
}

static void HE_PaintAdd(float px, float pw, float top, float bottom) {
    float gap = 10.0f, x0 = px + 12.0f, tw = (pw - 24.0f - gap) * 0.5f, th = 104.0f, gh = 34.0f, y;
    const char* last = NULL;
    int i, col = 0;

    y = top - he.addScroll;
    he.addNumStops = 0;
    if (y >= top) {
        HE_Text(x0 + 4.0f, y, 26.0f, 15.0f, heMd, "Drag one onto your HUD.", qfalse);
    }
    y += 26.0f;
    for (i = 0; i < (int)ARRAY_LEN(hePal); i++) {
        float tx, prev = 70.0f;

        if (!last || Q_stricmp(last, hePal[i].group)) {
            if (col) {
                y += th + gap;
                col = 0;
            }
            last = hePal[i].group;
            HE_AddStop(y + he.addScroll - top);
            if (y >= top && y + gh <= bottom) {
                HE_Caps(x0 + 4.0f, y + 6.0f, gh - 6.0f, 12.0f, heLo, last);
            }
            y += gh;
        }
        tx = x0 + col * (tw + gap);
        if (!col && i && !Q_stricmp(hePal[i - 1].group, hePal[i].group)) {
            HE_AddStop(y + he.addScroll - top);
        }
        if (y >= top && y + th <= bottom) {
            qboolean over = SM_Over(HE_HIT_ADD, i) || (he.drag == HE_DRAG_NEW && he.dragPal == i);

            HE_Round(tx, y, tw, th, 12.0f, over ? heRowHover : heCard);
            HE_Round(tx + 6.0f, y + 6.0f, tw - 12.0f, prev, 8.0f, heBg);
            HE_PalPreview(i, tx + 6.0f, y + 6.0f, tw - 12.0f, prev);
            HE_TextCenter(tx, y + prev + 6.0f, tw, th - prev - 6.0f, 15.0f, over ? heHi : heMd, hePal[i].label,
                          qfalse);
            SM_Hit(tx, y, tw, th, HE_HIT_ADD, i, NULL);
            if (SM_Over(HE_HIT_ADD, i) && he.drag == HE_DRAG_NONE) {
                HE_Tip(tx, y, tw, th, hePalDesc[i]);
            }
        }
        if (++col == 2) {
            col = 0;
            y += th + gap;
        }
    }
    if (col) {
        y += th + gap;
    }
    HE_RestoreCanvas();
    he.addScrollMax = y + he.addScroll - top - (bottom - top) + 8.0f;
    if (he.addScrollMax < 0.0f) {
        he.addScrollMax = 0.0f;
    } else {
        for (i = 0; i < he.addNumStops; i++) {
            if (he.addStops[i] >= he.addScrollMax - 0.5f) {
                he.addScrollMax = he.addStops[i];
                break;
            }
        }
    }
    if (he.addScroll > he.addScrollMax) {
        he.addScroll = he.addScrollMax;
    }
}

static void HE_PaintDragNew(void) {
    float mx, my;

    if (he.drag != HE_DRAG_NEW) {
        return;
    }
    HE_MousePx(&mx, &my);
    if (HE_InCanvas(mx, my)) {
        float box[4], r[4];

        HE_PalBox(he.dragPal, box);
        HE_RestoreCanvas();
        HE_PlaceNew(&heScratch, mx, my, HE_NewAnchor(he.dragPal, mx), box);
        if (!HV_PaintLoose(&heScratch, r)) {
            HV_ItemScreenRect(&heScratch, r);
        }
        r[0] -= 2.0f;
        r[1] -= 2.0f;
        r[2] += 4.0f;
        r[3] += 4.0f;
        HE_PxFill(r[0], r[1], r[2], 1.0f, heGold);
        HE_PxFill(r[0], r[1] + r[3] - 1.0f, r[2], 1.0f, heGold);
        HE_PxFill(r[0], r[1], 1.0f, r[3], heGold);
        HE_PxFill(r[0] + r[2] - 1.0f, r[1], 1.0f, r[3], heGold);
    } else {
        const char* label = hePal[he.dragPal].label;
        float w = HE_TextW(label, 14.0f, qtrue) + 28.0f;

        HE_Round(smMX + 14.0f, smMY + 10.0f, w, 34.0f, 17.0f, heGold);
        HE_TextCenter(smMX + 14.0f, smMY + 10.0f, w, 34.0f, 14.0f, heGoldText, label, qtrue);
    }
}

static void HE_Select(int i);

static void HE_DropNew(void) {
    float mx, my;
    int i;

    HE_MousePx(&mx, &my);
    if (!HE_InCanvas(mx, my)) {
        if (fabs(mx - he.pressX) > 6.0f * smScale || fabs(my - he.pressY) > 6.0f * smScale) {
            return;
        }
        mx = HE_PxX(he.cv[0] + he.cv[2] * 0.5f);
        my = HE_PxY(he.cv[1] + he.cv[3] * 0.5f);
    }
    i = HE_AddItem(he.dragPal, mx, my);
    if (i >= 0) {
        HE_Select(i);
        HE_Status(va("Added %s. Drag it into place.", hePal[he.dragPal].label));
    }
}

enum { HE_LINE_HEAD, HE_LINE_ROW, HE_LINE_HIDDEN };

typedef struct {
    int kind;
    int value;
} heLine_t;

static heLine_t heLines[HE_MAX_ITEMS + HV_CAT_COUNT * 2 + 1];
static int heNumLines;

static void HE_AddLine(int kind, int value) {
    heLines[heNumLines].kind = kind;
    heLines[heNumLines].value = value;
    heNumLines++;
}

static float HE_LineH(int kind) {
    return kind == HE_LINE_ROW ? HE_ROW_H : kind == HE_LINE_HIDDEN ? 42.0f : HE_GROUP_H;
}

static void HE_BuildLines(void) {
    static qboolean shown[HE_MAX_ITEMS];
    int i, pass, cat, hidden = 0;

    heNumLines = 0;
    for (i = 0; i < he.numItems; i++) {
        shown[i] = HV_ItemShown(he.items[i].item) ? qtrue : qfalse;
        if (!he.items[i].removed && !shown[i]) {
            hidden++;
        }
    }
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            if (!hidden) {
                return;
            }
            HE_AddLine(HE_LINE_HIDDEN, hidden);
            if (!he.hiddenOpen) {
                return;
            }
        }
        for (cat = 0; cat < HV_CAT_COUNT; cat++) {
            qboolean head = qfalse;

            for (i = 0; i < he.numItems; i++) {
                if (he.items[i].removed || shown[i] != (pass == 0 ? qtrue : qfalse) ||
                    HV_ItemCategory(he.items[i].item) != cat) {
                    continue;
                }
                if (!head) {
                    HE_AddLine(HE_LINE_HEAD, cat);
                    head = qtrue;
                }
                HE_AddLine(HE_LINE_ROW, i);
            }
        }
    }
}

static void HE_PaintRow(int i, float rx, float rw, float y, const smRect_t* clip, float top, float bottom) {
    heItem_t* e = &he.items[i];
    qboolean selected = i == he.selected, shown = HV_ItemShown(e->item);
    float base = HE_Base(16.0f, y, HE_ROW_H), right = rx + rw - 10.0f;
    const char* when = HV_ItemInvisible(e->item) ? "Invisible" : HV_ItemWhenShort(e->item);
    const char* name = HV_ItemLabel(e->item);
    int fit;

    if (SM_Over(HE_HIT_LAYER, i)) {
        he.layerHover = i;
    }
    if (selected) {
        HE_RoundIn(rx, y, rw, HE_ROW_H, HE_R, clip, heRowHover);
    } else if (SM_Over(HE_HIT_LAYER, i)) {
        HE_RoundIn(rx, y, rw, HE_ROW_H, HE_R, clip, heRow);
    }
    if (y >= top && y + HE_ROW_H <= bottom) {
        if (e->locked) {
            right -= HE_IconW(13.0f, HE_FA_LOCK);
            HE_Icon(right, y, HE_ROW_H, 13.0f, heLo, HE_FA_LOCK);
            right -= 8.0f;
        }
        if (when[0]) {
            float limit = rw * 0.55f, tw;

            fit = SM_Fit(HE_FONT, 13.0f, when, limit);
            tw = SM_AdvanceBold(HE_FONT, 13.0f, when, fit, qfalse);
            right -= tw;
            SM_Text(right, HE_Base(13.0f, y, HE_ROW_H), HE_FONT, 13.0f, heLo, when, fit, qfalse);
            right -= 10.0f;
        }
        fit = SM_Fit(HE_FONT, 16.0f, name, right - (rx + 10.0f));
        SM_Text(rx + 10.0f, base, HE_FONT, 16.0f, selected ? heHi : shown ? heMd : heLo, name, fit, selected);
    }
    SM_Hit(rx, y, rw, HE_ROW_H, HE_HIT_LAYER, i, clip);
}

static void HE_PaintLayers(void) {
    float px = HE_G, py = HE_BAR, pw = HE_LEFT_W, ph = HE_PanelH();
    float top = HE_ListTop(), bottom = top + HE_ListH(), y;
    smRect_t clip = SM_R(px + 8.0f, top, pw - 16.0f, bottom - top);
    const char* tabs[2] = {"Elements", "Add"};
    int k;

    he.layerHover = -1;
    HE_Round(px, py, pw, ph, HE_R_PANEL, hePanel);
    SM_Fill(px + pw - 1.0f, py, 1.0f, ph, heEdge, NULL);
    SM_Hit(px, py, pw, ph, HE_HIT_PANEL, 0, NULL);
    HE_Segment(px + 12.0f, py + 10.0f, pw - 24.0f, 36.0f, tabs, 2, he.leftTab, HE_HIT_LTAB, qtrue, qtrue);
    if (he.leftTab == 1) {
        HE_PaintAdd(px, pw, top, bottom);
        return;
    }

    HE_BuildLines();
    y = top - he.scroll;
    for (k = 0; k < heNumLines; k++) {
        const heLine_t* l = &heLines[k];
        float lh = HE_LineH(l->kind);

        if (y + lh > top && y < bottom) {
            if (l->kind == HE_LINE_ROW) {
                HE_PaintRow(l->value, px + 8.0f, pw - 16.0f, y, &clip, top, bottom);
            } else if (y >= top && y + lh <= bottom) {
                if (l->kind == HE_LINE_HEAD) {
                    HE_Text(px + 18.0f, y + 8.0f, lh - 8.0f, 14.0f, heLo, HV_CategoryName(l->value), qtrue);
                } else {
                    const char* text = va("Not in this view (%d)", l->value);

                    if (SM_Over(HE_HIT_HIDDEN, 0)) {
                        HE_Round(px + 8.0f, y + 4.0f, pw - 16.0f, lh - 8.0f, HE_R, heRow);
                    } else {
                        HE_Round(px + 8.0f, y + 4.0f, pw - 16.0f, lh - 8.0f, HE_R, heCard);
                    }
                    HE_Text(px + 18.0f, y, lh, 15.0f, heMd, text, qfalse);
                    HE_Icon(px + pw - 34.0f, y, lh, 11.0f, heMd, he.hiddenOpen ? HE_FA_MINUS : HE_FA_PLUS);
                    SM_Hit(px + 8.0f, y + 4.0f, pw - 16.0f, lh - 8.0f, HE_HIT_HIDDEN, 0, NULL);
                }
            }
        }
        y += lh;
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
    float y = 0.0f, view = HE_ListH();
    int k;

    if (!HV_ItemShown(he.items[i].item)) {
        he.hiddenOpen = qtrue;
    }
    HE_BuildLines();
    for (k = 0; k < heNumLines; k++) {
        if (heLines[k].kind == HE_LINE_ROW && heLines[k].value == i) {
            break;
        }
        y += HE_LineH(heLines[k].kind);
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
    float h = 38.0f, step = 26.0f, lw = HE_CapsW(label, 12.0f);
    qboolean focused = he.field == field;
    qboolean over = editable && SM_Over(HE_HIT_FIELD, field);

    if (focused) {
        HE_Round(x, y, w, h, HE_R, heGold);
        HE_Round(x + 1.5f, y + 1.5f, w - 3.0f, h - 3.0f, HE_R - 1.5f, heRowHover);
    } else {
        HE_Round(x, y, w, h, HE_R, over ? heRowHover : heRow);
    }
    HE_Caps(x + 12.0f, y, h, 12.0f, heLo, label);
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
    HE_TextCenter(x, y + 156.0f, w, 22.0f, 14.0f, heMd, "from the list.", qfalse);
}

enum { HE_OG_WEAPON, HE_OG_HUD, HE_OG_CROSSHAIR, HE_OG_MESSAGES, HE_OG_COUNT };

static const char* heOptGroups[HE_OG_COUNT] = {"Weapon", "HUD", "Crosshair", "Messages"};
static int heOpts[64];
static int heOptGroup[64];
static int heNumOpts = -1;

static int HE_OptGroupOf(const stCvar_t* cv) {
    if (!Q_stricmp(cv->cvar, "cg_drawgun") || !Q_stricmp(cv->cvar, "cg_weaponbar") ||
        !Q_stricmp(cv->cvar, "cg_drawfullweaponbar")) {
        return HE_OG_WEAPON;
    }
    if (Q_stristr(cv->cvar, "crosshair")) {
        return HE_OG_CROSSHAIR;
    }
    return cv->column ? HE_OG_MESSAGES : HE_OG_HUD;
}

static void HE_BuildOpts(void) {
    int g, i, n = UI_SettingsCount();

    heNumOpts = 0;
    for (g = 0; g < HE_OG_COUNT; g++) {
        for (i = 0; i < n && heNumOpts < (int)ARRAY_LEN(heOpts); i++) {
            const stCvar_t* cv = UI_SettingsAt(i);

            if (!cv->cvar || !(cv->widget == ST_BOOL || cv->widget == ST_SELECT || cv->widget == ST_RANGE ||
                               cv->widget == ST_PALETTE || cv->widget == ST_CROSSHAIR)) {
                continue;
            }
            if ((cv->tab != ST_TAB_HUD && Q_stricmp(cv->cvar, "cg_drawgun")) || !Q_stricmp(cv->cvar, "cg_hudfiles")) {
                continue;
            }
            if (HE_OptGroupOf(cv) != g) {
                continue;
            }
            heOpts[heNumOpts] = i;
            heOptGroup[heNumOpts] = g;
            heNumOpts++;
        }
    }
}

static const char* HE_OptValue(const stCvar_t* cv) {
    static char buf[MAX_CVAR_VALUE_STRING];

    trap_Cvar_VariableStringBuffer(cv->cvar, buf, sizeof(buf));
    return buf;
}

static int HE_OptIndex(const stCvar_t* cv, const char* v) {
    int k;

    for (k = 0; k < cv->numOptions; k++) {
        if (!Q_stricmp(cv->options[k].value, v)) {
            return k;
        }
    }
    for (k = 0; k < cv->numOptions; k++) {
        if (atof(cv->options[k].value) == atof(v)) {
            return k;
        }
    }
    return -1;
}

static int HE_StepDecimals(float step) {
    double scaled = step;
    int d;

    if (step <= 0.0f) {
        return 0;
    }
    for (d = 0; d < 4; d++) {
        if (fabs(scaled - floor(scaled + 0.5)) < 0.0001) {
            break;
        }
        scaled *= 10.0;
    }
    return d;
}

static void HE_OptChange(int idx, int dir) {
    const stCvar_t* cv = UI_SettingsAt(idx);
    const char* v;

    if (!cv) {
        return;
    }
    v = HE_OptValue(cv);
    switch (cv->widget) {
        case ST_BOOL:
            trap_Cvar_Set(cv->cvar, atoi(v) ? "0" : "1");
            break;
        case ST_SELECT:
            if (cv->numOptions > 0) {
                int k = HE_OptIndex(cv, v);

                k = k < 0 ? (dir > 0 ? 0 : cv->numOptions - 1) : (k + dir + cv->numOptions) % cv->numOptions;
                trap_Cvar_Set(cv->cvar, cv->options[k].value);
            }
            break;
        case ST_RANGE: {
            float f = (float)atof(v) + (float)dir * (cv->step > 0.0f ? cv->step : 1.0f);

            if (f < cv->min) {
                f = cv->min;
            }
            if (cv->max > cv->min && f > cv->max) {
                f = cv->max;
            }
            trap_Cvar_Set(cv->cvar, va("%.*f", HE_StepDecimals(cv->step), f));
            break;
        }
        case ST_PALETTE: {
            int c = atoi(v) + dir;

            trap_Cvar_Set(cv->cvar, va("%d", c < 1 ? 26 : c > 26 ? 1 : c));
            break;
        }
        case ST_CROSSHAIR: {
            int c = atoi(v) + dir;

            trap_Cvar_Set(cv->cvar, va("%d", c < 0 ? NUM_CROSSHAIRS - 1 : c >= NUM_CROSSHAIRS ? 0 : c));
            break;
        }
        default:
            break;
    }
}

static void HE_OptStepper(float x, float y, int idx, const char* text, qhandle_t pic, const float* swatch) {
    float bw = 28.0f, mid = 52.0f, h = 30.0f;

    HE_Round(x, y, bw * 2.0f + mid, h, h * 0.5f, heCard);
    if (SM_Over(HE_HIT_OPTSTEP, idx * 2)) {
        HE_Round(x + 2.0f, y + 2.0f, bw - 2.0f, h - 4.0f, (h - 4.0f) * 0.5f, heRowHover);
    }
    if (SM_Over(HE_HIT_OPTSTEP, idx * 2 + 1)) {
        HE_Round(x + bw + mid, y + 2.0f, bw - 2.0f, h - 4.0f, (h - 4.0f) * 0.5f, heRowHover);
    }
    HE_IconCenter(x, y, bw, h, 10.0f, heMd, HE_FA_MINUS);
    HE_IconCenter(x + bw + mid, y, bw, h, 10.0f, heMd, HE_FA_PLUS);
    SM_Hit(x, y, bw, h, HE_HIT_OPTSTEP, idx * 2, NULL);
    SM_Hit(x + bw + mid, y, bw, h, HE_HIT_OPTSTEP, idx * 2 + 1, NULL);
    if (swatch) {
        HE_Round(x + bw + 6.0f, y + 6.0f, mid - 12.0f, h - 12.0f, 4.0f, swatch);
    } else if (pic) {
        trap_R_SetColor(NULL);
        trap_R_DrawStretchPic(HE_PxX(x + bw + mid * 0.5f - 11.0f), HE_PxY(y + 4.0f), 22.0f * smScale, 22.0f * smScale,
                              0, 0, 1, 1, pic);
    } else {
        HE_TextCenter(x + bw, y, mid, h, 13.0f, heHi, text, qtrue);
    }
}

static void HE_PaintOptRow(int idx, float x, float y, float w, float h) {
    const stCvar_t* cv = UI_SettingsAt(idx);
    const char* v = HE_OptValue(cv);
    float right = x + w - 8.0f, cy = y + (h - 30.0f) * 0.5f, cw = 0.0f;
    qboolean over = SM_Over(HE_HIT_OPTROW, idx) || SM_Over(HE_HIT_OPT, idx) || SM_Over(HE_HIT_OPTSTEP, idx * 2) ||
                    SM_Over(HE_HIT_OPTSTEP, idx * 2 + 1);
    int fit;

    if (over) {
        HE_Round(x, y + 2.0f, w, h - 4.0f, HE_R, heRow);
        if (cv->tip) {
            HE_Tip(x, y, w, h - 4.0f, cv->tip);
        }
    }
    SM_Hit(x, y + 2.0f, w, h - 4.0f, HE_HIT_OPTROW, idx, NULL);
    switch (cv->widget) {
        case ST_BOOL: {
            qboolean on = atoi(v) != 0;
            float tw = 40.0f, th = 22.0f, ty = y + (h - th) * 0.5f;

            cw = tw;
            HE_Round(right - tw, ty, tw, th, th * 0.5f, on ? heGold : heEdge);
            HE_Round(on ? right - th + 3.0f : right - tw + 3.0f, ty + 3.0f, th - 6.0f, th - 6.0f, (th - 6.0f) * 0.5f,
                     on ? heGoldText : heMd);
            SM_Hit(right - tw, ty, tw, th, HE_HIT_OPT, idx, NULL);
            break;
        }
        case ST_SELECT: {
            int k = HE_OptIndex(cv, v);
            const char* label = k >= 0 ? cv->options[k].label : v;
            float pw = HE_TextW(label, 13.0f, qtrue) + 50.0f;

            if (pw > w * 0.55f) {
                pw = w * 0.55f;
            }
            cw = pw;
            HE_Round(right - pw, cy, pw, 30.0f, 15.0f, SM_Over(HE_HIT_OPT, idx) ? heRowHover : heCard);
            fit = SM_Fit(HE_FONT, 13.0f, label, pw - 50.0f);
            SM_Text(right - pw + 16.0f, HE_Base(13.0f, cy, 30.0f), HE_FONT, 13.0f, heHi, label, fit, qtrue);
            HE_Icon(right - 26.0f, cy, 30.0f, 9.0f, heMd, HE_FA_CHEVRON);
            SM_Hit(right - pw, cy, pw, 30.0f, HE_HIT_OPT, idx, NULL);
            break;
        }
        case ST_RANGE:
            cw = 108.0f;
            HE_OptStepper(right - cw, cy, idx, va("%.*f", HE_StepDecimals(cv->step), atof(v)), 0, NULL);
            break;
        case ST_PALETTE: {
            int c = atoi(v);

            cw = 108.0f;
            HE_OptStepper(right - cw, cy, idx, "", 0, c >= 1 && c <= 26 ? UI_CrosshairPalette()[c - 1] : heGhost);
            break;
        }
        case ST_CROSSHAIR: {
            int c = atoi(v);

            cw = 108.0f;
            HE_OptStepper(right - cw, cy, idx, "None", c > 0 && c < NUM_CROSSHAIRS ? uiInfo.uiDC.Assets.crosshairShader[c] : 0,
                          NULL);
            break;
        }
        default:
            break;
    }
    fit = SM_Fit(HE_FONT, 13.0f, cv->title, right - cw - 12.0f - (x + 12.0f));
    SM_Text(x + 12.0f, HE_Base(13.0f, y, h), HE_FONT, 13.0f, heHi, cv->title, fit, qfalse);
}

static void HE_PaintOptions(float px, float py, float ph) {
    float x = px + 12.0f, w = HE_RIGHT_W - 24.0f, top = py + HE_HEAD_H + 4.0f, bottom = py + ph - 12.0f, y;
    float rh = 44.0f, gh = 34.0f;
    int i, last = -1;

    if (heNumOpts < 0) {
        HE_BuildOpts();
    }
    y = top - he.optScroll;
    for (i = 0; i < heNumOpts; i++) {
        if (heOptGroup[i] != last) {
            last = heOptGroup[i];
            if (y >= top && y + gh <= bottom) {
                HE_Caps(x + 12.0f, y + 8.0f, gh - 8.0f, 12.0f, heLo, heOptGroups[last]);
            }
            y += gh;
        }
        if (y >= top && y + rh <= bottom) {
            HE_PaintOptRow(heOpts[i], x, y, w, rh);
        }
        y += rh;
    }
    he.optScrollMax = y + he.optScroll - top - (bottom - top) + 8.0f;
    if (he.optScrollMax < 0.0f) {
        he.optScrollMax = 0.0f;
    }
    if (he.optScroll > he.optScrollMax) {
        he.optScroll = he.optScrollMax;
    }
}

static qboolean HE_PaintsBack(const itemDef_t* it) {
    return it->window.style == WINDOW_STYLE_FILLED || it->window.style == WINDOW_STYLE_GRADIENT;
}

static void HE_PaintProps(void) {
    static const char* anchors[5] = {"Auto", "Stretch", "Left", "Center", "Right"};
    static const char* tabs[2] = {"Properties", "Options"};
    float px = smVW - HE_G - HE_RIGHT_W, py = HE_BAR, ph = HE_PanelH();
    float x = px + 20.0f, w = HE_RIGHT_W - 40.0f, y = py + HE_HEAD_H, half = (w - 8.0f) * 0.5f;
    heItem_t* e;
    itemDef_t* it;
    qboolean editable;
    int anchor;

    HE_Round(px, py, HE_RIGHT_W, ph, HE_R_PANEL, hePanel);
    SM_Fill(px, py, 1.0f, ph, heEdge, NULL);
    SM_Hit(px, py, HE_RIGHT_W, ph, HE_HIT_PANEL, 0, NULL);
    HE_Segment(px + 12.0f, py + 10.0f, HE_RIGHT_W - 24.0f, 36.0f, tabs, 2, he.rightTab, HE_HIT_RTAB, qtrue, qtrue);
    if (he.rightTab == 1) {
        HE_PaintOptions(px, py, ph);
        return;
    }

    if (he.selected < 0) {
        HE_PaintEmpty(x, y, w);
        return;
    }
    e = &he.items[he.selected];
    it = e->item;
    editable = !e->locked;
    {
        const char* when = HV_ItemInvisible(it) ? "Invisible" : HV_ItemWhen(it);
        const char* sub = when[0] ? when : "Always visible";
        float tx = x, tw = w;
        int fit, fit2;

        if (HE_Removable(he.selected)) {
            tw -= 44.0f;
            HE_IconButton(x + w - 36.0f, y + 1.0f, 36.0f, HE_FA_TRASH, HE_HIT_DELETE, 0, qfalse, qfalse, "Delete");
        }
        fit = SM_Fit(HE_FONT, 18.0f, HV_ItemLabel(it), tw);
        fit2 = SM_Fit(HE_FONT, 14.0f, sub, tw);
        SM_Text(tx, HE_Base(18.0f, y - 6.0f, 30.0f), HE_FONT, 18.0f, heHi, HV_ItemLabel(it), fit, qtrue);
        SM_Text(tx, HE_Base(14.0f, y + 20.0f, 22.0f), HE_FONT, 14.0f, heLo, sub, fit2, qfalse);
    }
    y += 58.0f;
    if (!editable) {
        HE_Round(x, y, w, 60.0f, 12.0f, heCard);
        HE_Icon(x + 14.0f, y, 60.0f, 15.0f, heWarn, HE_FA_LOCK);
        HE_Text(x + 40.0f, y + 9.0f, 20.0f, 13.0f, heHi, "Read only", qtrue);
        HE_Text(x + 40.0f, y + 29.0f, 20.0f, 13.0f, heMd, "It comes from another HUD file.", qfalse);
        y += 72.0f;
    }

    HE_Caps(x, y, 18.0f, 12.0f, heLo, "Position and size");
    y += 26.0f;
    HE_NumberField(x, y, half, "X", HF_X, editable);
    HE_NumberField(x + half + 8.0f, y, half, "Y", HF_Y, editable);
    y += 46.0f;
    HE_NumberField(x, y, half, "W", HF_W, editable);
    HE_NumberField(x + half + 8.0f, y, half, "H", HF_H, editable);
    y += 58.0f;

    if (!(it->type == ITEM_TYPE_OWNERDRAW && it->window.ownerDraw == CG_AREA_NEW_CHAT)) {
        HE_Caps(x, y, 18.0f, 12.0f, heLo, "Stick to screen side");
        y += 26.0f;
        anchor = it->widescreenFlag ? it->widescreen + 1 : 0;
        HE_Segment(x, y, w, 40.0f, anchors, 5, anchor, HE_HIT_ANCHOR, editable, qtrue);
        y += 58.0f;
    }

    if (HE_HasText(it)) {
        HE_Caps(x, y, 18.0f, 12.0f, heLo, "Text");
        y += 26.0f;
        HE_NumberField(x, y, w, "Size", HF_SCALE, editable);
        y += 58.0f;
    }

    HE_Caps(x, y, 18.0f, 12.0f, heLo, "Colors");
    y += 26.0f;
    HE_Swatch(x, y, w, "Color", HEK_FORECOLOR, it->window.foreColor, editable);
    y += 44.0f;
    if (HE_PaintsBack(it)) {
        HE_Swatch(x, y, w, "Background", HEK_BACKCOLOR, it->window.backColor, editable);
        y += 44.0f;
    }
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
    HE_Caps(x + pad, ty, 18.0f, 12.0f, heLo, "Opacity");
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
    float w = 360.0f, row = 36.0f, head = 30.0f, pad = 8.0f, x = he.hudMenuX, y = HE_BAR + 2.0f, h, ry;
    int retail = he.numPresets, i;

    if (!he.hudMenu) {
        return;
    }
    h = pad * 2.0f + head * 2.0f + row * (float)retail +
        (row + 14.0f) * (float)(he.numHuds > retail ? he.numHuds - retail : 1) + 13.0f + row * 2.0f;
    if (y + h > smBottom - 8.0f) {
        h = smBottom - 8.0f - y;
    }
    SM_Hit(0, 0, smVW, smBottom, HE_HIT_POPUP, 1, NULL);
    HE_Floating(x, y, w, h, 14.0f, heCard);
    SM_Hit(x, y, w, h, HE_HIT_POPUP, 0, NULL);
    ry = y + pad;
    for (i = 0; i < he.numHuds; i++) {
        qboolean current = !Q_stricmp(he.huds[i], he.list);

        float rh = i < retail ? row : row + 14.0f;

        if (i == 0 || i == retail) {
            HE_Caps(x + 16.0f, ry, head, 12.0f, heLo, i < retail ? "Quake Live" : "Your HUDs");
            ry += head;
        }
        if (ry + rh > y + h - pad - 13.0f - row * 2.0f) {
            break;
        }
        if (SM_Over(HE_HIT_HUDITEM, i)) {
            HE_Round(x + 6.0f, ry, w - 12.0f, rh, 9.0f, heRowHover);
        }
        HE_Icon(x + 18.0f, ry, row, 13.0f, current ? heGold : heLo, i < retail ? HE_FA_FILE : HE_FA_FOLDER);
        HE_Text(x + 44.0f, ry, row, 14.0f, current ? heHi : heMd, HE_HudName(he.huds[i]), current);
        if (i >= retail) {
            HE_Text(x + 44.0f, ry + 22.0f, 20.0f, 12.0f, heLo, he.hudNote[i], qfalse);
        }
        if (current) {
            HE_Icon(x + w - 34.0f, ry, row, 12.0f, heGold, HE_FA_CHECK);
        }
        SM_Hit(x + 6.0f, ry, w - 12.0f, rh, HE_HIT_HUDITEM, i, NULL);
        ry += rh;
    }
    if (he.numHuds <= retail) {
        if (retail == he.numHuds) {
            HE_Caps(x + 16.0f, ry, head, 12.0f, heLo, "Your HUDs");
            ry += head;
        }
        HE_Text(x + 18.0f, ry, row, 13.0f, heGhost, "HUDs in your ui folder appear here.", qfalse);
    }
    ry = y + h - pad - row * 2.0f;
    SM_Fill(x + 16.0f, ry - 7.0f, w - 32.0f, 1.0f, heEdge, NULL);
    for (i = 0; i < 2; i++) {
        if (SM_Over(HE_HIT_HUDACT, i)) {
            HE_Round(x + 6.0f, ry, w - 12.0f, row, 9.0f, heRowHover);
        }
        HE_Icon(x + 18.0f, ry, row, 13.0f, heMd, i ? HE_FA_FOLDER : HE_FA_COPY);
        HE_Text(x + 44.0f, ry, row, 14.0f, heMd, i ? "Open HUD folder" : "Save as...", qfalse);
        SM_Hit(x + 6.0f, ry, w - 12.0f, row, HE_HIT_HUDACT, i, NULL);
        ry += row;
    }
}

static void HE_PaintMapMenu(void) {
    float w = 280.0f, row = 52.0f, pad = 8.0f, x = he.mapMenuX, y = HE_BAR + 2.0f, h, ry;
    int i;

    if (!he.mapMenu) {
        return;
    }
    h = pad * 2.0f + row * (float)ARRAY_LEN(heMaps);
    SM_Hit(0, 0, smVW, smBottom, HE_HIT_POPUP, 1, NULL);
    HE_Floating(x, y, w, h, HE_R, heCard);
    SM_Hit(x, y, w, h, HE_HIT_POPUP, 0, NULL);
    ry = y + pad;
    for (i = 0; i < (int)ARRAY_LEN(heMaps); i++) {
        qboolean current = i == he.map;

        if (SM_Over(HE_HIT_MAPITEM, i)) {
            HE_Round(x + 6.0f, ry, w - 12.0f, row, HE_R, heRowHover);
        }
        trap_R_SetColor(NULL);
        trap_R_DrawStretchPic(HE_PxX(x + 12.0f), HE_PxY(ry + 5.0f), 75.0f * smScale, 42.0f * smScale, 0, 0, 1, 1,
                              trap_R_RegisterShaderNoMip(va("gfx/hudeditor/%s", heMaps[i])));
        HE_Text(x + 98.0f, ry, row, 15.0f, current ? heHi : heMd, heMapNames[i], current);
        if (current) {
            HE_Icon(x + w - 30.0f, ry, row, 12.0f, heGold, HE_FA_CHECK);
        }
        SM_Hit(x + 6.0f, ry, w - 12.0f, row, HE_HIT_MAPITEM, i, NULL);
        ry += row;
    }
}

static void HE_PaintSaveAs(void) {
    float w = 500.0f, h = 230.0f, x = (smVW - w) * 0.5f, y = (smBottom - h) * 0.5f, bx, by, fx, fy, fw, fh, tw;

    if (!he.saveAs) {
        return;
    }
    SM_Fill(0, 0, smVW, smBottom, heShade, NULL);
    SM_Hit(0, 0, smVW, smBottom, HE_HIT_POPUP, 0, NULL);
    HE_Floating(x, y, w, h, HE_R_PANEL, hePanel);
    SM_Hit(x, y, w, h, HE_HIT_POPUP, 0, NULL);
    HE_Text(x + 28.0f, y + 24.0f, 30.0f, 20.0f, heHi, "Save as", qtrue);
    HE_Text(x + 28.0f, y + 62.0f, 22.0f, 15.0f, heMd,
            HE_IsMine() ? "Name your copy. It goes to your ui folder."
                        : "Quake Live HUDs stay as they are. Name your own copy.",
            qfalse);
    fx = x + 28.0f;
    fy = y + 98.0f;
    fw = w - 56.0f;
    fh = 46.0f;
    HE_Round(fx, fy, fw, fh, 10.0f, heGold);
    HE_Round(fx + 2.0f, fy + 2.0f, fw - 4.0f, fh - 4.0f, 8.0f, heBg);
    HE_Text(fx + 16.0f, fy, fh, 17.0f, heHi, he.saveName, qfalse);
    tw = HE_TextW(he.saveName, 17.0f, qfalse);
    if ((uiInfo.uiDC.realTime / 500) & 1) {
        SM_Fill(fx + 17.0f + tw, fy + 12.0f, 2.0f, fh - 24.0f, heGold, NULL);
    }
    by = y + h - 28.0f - HE_CTRL_H;
    bx = x + w - 28.0f - HE_PillW("Save", HE_FA_SAVE);
    HE_Pill(bx, by, HE_CTRL_H, "Save", HE_FA_SAVE, NULL, HE_PILL_PRIMARY, HE_HIT_SAVEAS, 0, NULL);
    bx -= 8.0f + HE_PillW("Cancel", 0);
    HE_Pill(bx, by, HE_CTRL_H, "Cancel", 0, NULL, HE_PILL_GHOST, HE_HIT_SAVEAS, 1, NULL);
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
    float size = 14.0f, h = 24.0f, w = HE_TextW(text, size, qtrue) + 16.0f;
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
    if (he.layerHover >= 0 && he.layerHover < he.numItems && he.layerHover != he.selected &&
        !he.items[he.layerHover].removed && !HV_ItemShown(he.items[he.layerHover].item)) {
        HV_PaintItem(he.items[he.layerHover].item);
    }
    if (hvScene.showAll) {
        for (i = 0; i < he.numItems; i++) {
            if (!he.items[i].removed && !HV_ItemShown(he.items[i].item) &&
                (he.items[i].item->window.flags & WINDOW_VISIBLE) &&
                i != he.selected) {
                HV_PaintItem(he.items[i].item);
            }
        }
    }
    if (he.grid) {
        HE_PaintGrid();
    }

    HE_MousePx(&mx, &my);
    if (!he.confirm && !he.hudMenu && !he.mapMenu && he.colorKey < 0 && he.drag == HE_DRAG_NONE &&
        HE_InCanvas(mx, my) && !smHasUnder) {
        hover = HE_PickAt(mx, my);
    }
    if (he.layerHover >= 0 && he.layerHover < he.numItems && !he.items[he.layerHover].removed) {
        hover = he.layerHover;
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
    float kw = HE_CapsW(key, 11.0f) + 14.0f, ww = HE_TextW(what, 13.0f, qfalse);

    if (x + kw + 8.0f + ww > limit) {
        return -1.0f;
    }
    HE_Round(x, y + (h - 24.0f) * 0.5f, kw, 24.0f, 6.0f, heRow);
    HE_CapsCenter(x, y + (h - 24.0f) * 0.5f, kw, 24.0f, 11.0f, heMd, key);
    HE_Text(x + kw + 8.0f, y, h, 13.0f, heLo, what, qfalse);
    return x + kw + 8.0f + ww + 18.0f;
}

static void HE_PaintStatus(void) {
    static const char* hints[][2] = {
        {"Drag", "Move"}, {"Corners", "Resize"}, {"Arrows", "Nudge"}, {"Shift", "x10"}, {"Ctrl", "Resize"},
        {"Alt", "No snapping"}, {"G", "Grid"},
    };
    float x0 = HE_LEFT_W, x1 = smVW - HE_RIGHT_W, y = smBottom - HE_STATUS_H, x = x0 + 18.0f, limit = x1 - 18.0f;
    const char* msg = NULL;
    const float* msgColor = heHi;
    int icon = HE_FA_CHECK;
    const float* iconColor = heGold;
    int i;

    SM_Fill(x0, y, x1 - x0, HE_STATUS_H, hePanel, NULL);
    SM_Fill(x0, y, x1 - x0, 1.0f, heEdge, NULL);
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
    int i;

    trap_Cvar_VariableStringBuffer("cg_hudFiles", list, sizeof(list));
    if (!list[0]) {
        Q_strncpyz(list, "ui/hud.txt", sizeof(list));
    }
    memset(&he.undo, 0, sizeof(he.undo));
    he.guideX = he.guideY = -1.0f;
    he.open = qtrue;
    he.round = trap_R_RegisterShaderNoMip("gfx/hudeditor/round");
    HE_ScanHuds();
    i = HE_FindHud(list);
    if (i >= 0) {
        Q_strncpyz(list, he.huds[i], sizeof(list));
    }
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

static float HE_ViewH(void) {
    float w = (float)uiInfo.uiDC.glconfig.vidWidth, h = (float)uiInfo.uiDC.glconfig.vidHeight, viewH = 864.0f;

    if (h > 0.0f && w / h * viewH < 1500.0f) {
        viewH = 1500.0f * h / w;
    }
    return viewH;
}

void UI_HudEditor_Paint(void) {
    SM_BeginFrameView(HE_ViewH());
    if (!he.open) {
        UI_HudEditor_Open();
    }
    he.tip = NULL;
    HE_Layout();
    if (he.drag != HE_DRAG_NONE && !trap_Key_IsDown(K_MOUSE1)) {
        if (he.drag == HE_DRAG_NEW) {
            HE_DropNew();
        }
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
    HE_PaintMapMenu();
    HE_PaintDragNew();
    HE_PaintTip();
    HE_PaintConfirm();
    HE_PaintSaveAs();
    SM_EndFrame();
}

static void HE_Select(int i) {
    if (he.field >= 0) {
        HE_CommitField();
    }
    he.colorKey = -1;
    he.selected = i;
    if (i >= 0) {
        he.rightTab = 0;
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
    if (he.saveAs) {
        if (hit && hit->type == HE_HIT_SAVEAS) {
            if (hit->index == 0) {
                HE_DoSaveAs();
            } else {
                he.saveAs = qfalse;
            }
        }
        return qtrue;
    }
    if (he.hudMenu) {
        if (hit && hit->type == HE_HIT_HUDITEM && hit->index < he.numHuds) {
            HE_SwitchHud(he.huds[hit->index]);
        } else if (hit && hit->type == HE_HIT_HUDACT && hit->index == 0) {
            HE_OpenSaveAs();
        } else if (hit && hit->type == HE_HIT_HUDACT && hit->index == 1) {
            he.hudMenu = qfalse;
            trap_Cmd_ExecuteText(EXEC_APPEND, "openhudfolder\n");
            HE_Status("Your HUD folder is open. Files you drop there show up in this list.");
        } else if (!(hit && hit->type == HE_HIT_POPUP && hit->index == 0)) {
            he.hudMenu = qfalse;
        }
        return qtrue;
    }
    if (he.mapMenu) {
        if (hit && hit->type == HE_HIT_MAPITEM) {
            he.map = hit->index;
            he.mapMenu = qfalse;
        } else if (!(hit && hit->type == HE_HIT_POPUP && hit->index == 0)) {
            he.mapMenu = qfalse;
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
                he.colorKey = -1;
                he.mapMenu = qtrue;
                return qtrue;
            case HE_HIT_HIDDEN:
                he.hiddenOpen = !he.hiddenOpen;
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
            case HE_HIT_RTAB:
                he.rightTab = hit->index;
                return qtrue;
            case HE_HIT_LTAB:
                he.leftTab = hit->index;
                return qtrue;
            case HE_HIT_ADD:
                he.drag = HE_DRAG_NEW;
                he.dragPal = hit->index;
                HE_MousePx(&he.pressX, &he.pressY);
                return qtrue;
            case HE_HIT_DELETE:
                HE_Remove(he.selected);
                return qtrue;
            case HE_HIT_OPT:
                HE_OptChange(hit->index, 1);
                return qtrue;
            case HE_HIT_OPTSTEP:
                HE_OptChange(hit->index / 2, hit->index & 1 ? 1 : -1);
                return qtrue;
            case HE_HIT_OPTROW: {
                const stCvar_t* cv = UI_SettingsAt(hit->index);

                if (cv && (cv->widget == ST_BOOL || cv->widget == ST_SELECT)) {
                    HE_OptChange(hit->index, 1);
                }
                return qtrue;
            }
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

    if (he.saveAs && key != K_MOUSE1) {
        return HE_SaveAsKey(key);
    }
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
            if (smMX >= HE_G && smMX < HE_G + HE_LEFT_W && he.leftTab == 1) {
                HE_ScrollAdd(key == K_MWHEELUP ? -1 : 1);
            } else if (smMX >= HE_G && smMX < HE_G + HE_LEFT_W) {
                he.scroll += key == K_MWHEELUP ? -HE_ROW_H * 3 : HE_ROW_H * 3;
                if (he.scroll < 0) {
                    he.scroll = 0;
                }
            } else if (he.rightTab == 1 && smMX >= smVW - HE_G - HE_RIGHT_W) {
                he.optScroll += key == K_MWHEELUP ? -88.0f : 88.0f;
                if (he.optScroll > he.optScrollMax) {
                    he.optScroll = he.optScrollMax;
                }
                if (he.optScroll < 0.0f) {
                    he.optScroll = 0.0f;
                }
            }
            return qtrue;
        case K_DEL:
        case K_KP_DEL:
        case K_BACKSPACE:
            if (!he.confirm && he.selected >= 0) {
                HE_Remove(he.selected);
            }
            return qtrue;
        case K_ESCAPE:
            if (he.confirm) {
                he.confirm = qfalse;
                he.pending[0] = 0;
            } else if (he.hudMenu) {
                he.hudMenu = qfalse;
            } else if (he.mapMenu) {
                he.mapMenu = qfalse;
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
                    if (!he.items[i].removed && (HV_ItemShown(he.items[i].item) || hvScene.showAll)) {
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
