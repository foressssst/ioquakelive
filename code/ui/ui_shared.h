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
#ifndef __UI_SHARED_H
#define __UI_SHARED_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "../client/keycodes.h"

#include "../../ui/menudef.h"

#define MAX_MENUNAME 64
#define MAX_ITEMTEXT 128
#define MAX_ITEMACTION 256
#define MAX_MENUDEFFILE 4096
#define MAX_MENUFILE 65536
#define MAX_MENUS 512
#define MAX_MENUITEMS 2048
#define MAX_COLOR_RANGES 10
#define MAX_OPEN_MENUS 16

#define WINDOW_MOUSEOVER 0x00000001
#define WINDOW_HASFOCUS 0x00000002
#define WINDOW_VISIBLE 0x00000004
#define WINDOW_GREY 0x00000008
#define WINDOW_DECORATION 0x00000010
#define WINDOW_FADINGOUT 0x00000020
#define WINDOW_FADINGIN 0x00000040
#define WINDOW_MOUSEOVERTEXT 0x00000080
#define WINDOW_INTRANSITION 0x00000100
#define WINDOW_FORECOLORSET 0x00000200
#define WINDOW_HORIZONTAL 0x00000400
#define WINDOW_LB_LEFTARROW 0x00000800
#define WINDOW_LB_RIGHTARROW 0x00001000
#define WINDOW_LB_THUMB 0x00002000
#define WINDOW_LB_PGUP 0x00004000
#define WINDOW_LB_PGDN 0x00008000
#define WINDOW_ORBITING 0x00010000
#define WINDOW_OOB_CLICK 0x00020000
#define WINDOW_WRAPPED 0x00040000
#define WINDOW_AUTOWRAPPED 0x00080000
#define WINDOW_FORCED 0x00100000
#define WINDOW_POPUP 0x00200000
#define WINDOW_BACKCOLORSET 0x00400000
#define WINDOW_TIMEDVISIBLE 0x00800000

#define CURSOR_NONE 0x00000001
#define CURSOR_ARROW 0x00000002
#define CURSOR_SIZER 0x00000004

#ifdef CGAME
#define STRING_POOL_SIZE  128 * 1024
#else
#define STRING_POOL_SIZE 1024 * 1024
#endif
#define MAX_STRING_HANDLES 8192

#define MAX_SCRIPT_ARGS 12
#define MAX_EDITFIELD 256

#define ART_FX_BASE "menu/art/fx_base"
#define ART_FX_BLUE "menu/art/fx_blue"
#define ART_FX_CYAN "menu/art/fx_cyan"
#define ART_FX_GREEN "menu/art/fx_grn"
#define ART_FX_RED "menu/art/fx_red"
#define ART_FX_TEAL "menu/art/fx_teal"
#define ART_FX_WHITE "menu/art/fx_white"
#define ART_FX_YELLOW "menu/art/fx_yel"

#define ASSET_GRADIENTBAR "ui/assets/gradientbar2.tga"
#define ASSET_SCROLLBAR "ui/assets/scrollbar.tga"
#define ASSET_SCROLLBAR_ARROWDOWN "ui/assets/scrollbar_arrow_dwn_a.tga"
#define ASSET_SCROLLBAR_ARROWUP "ui/assets/scrollbar_arrow_up_a.tga"
#define ASSET_SCROLLBAR_ARROWLEFT "ui/assets/scrollbar_arrow_left.tga"
#define ASSET_SCROLLBAR_ARROWRIGHT "ui/assets/scrollbar_arrow_right.tga"
#define ASSET_SCROLL_THUMB "ui/assets/scrollbar_thumb.tga"
#define ASSET_SLIDER_BAR "ui/assets/slider2.tga"
#define ASSET_SLIDER_THUMB "ui/assets/sliderbutt_1.tga"
#define SCROLLBAR_SIZE 16.0
#define SLIDER_WIDTH 96.0
#define SLIDER_HEIGHT 16.0
#define SLIDER_THUMB_WIDTH 12.0
#define SLIDER_THUMB_HEIGHT 20.0
#define NUM_CROSSHAIRS 30

typedef struct {
    const char* command;
    const char* args[MAX_SCRIPT_ARGS];
} scriptDef_t;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} rectDef_t;

typedef rectDef_t Rectangle;

typedef struct {
    Rectangle rect;
    Rectangle rectClient;
    const char* name;
    const char* group;
    const char* cinematicName;
    int cinematic;
    int style;
    int border;
    int ownerDraw;
    int ownerDrawFlags;
    int ownerDrawFlags2;
    float borderSize;
    int flags;
    Rectangle rectEffects;
    Rectangle rectEffects2;
    int offsetTime;
    int nextTime;
    int pad_74;
    vec4_t foreColor;
    vec4_t backColor;
    vec4_t borderColor;
    vec4_t outlineColor;
    vec4_t altRowColor;
    vec4_t elementColor;
    vec4_t selectedColor;
    qhandle_t background;
    qhandle_t outlineImage;
    Rectangle backgroundSize;
} windowDef_t;

typedef windowDef_t Window;

typedef struct {
    vec4_t color;
    float low;
    float high;
} colorRangeDef_t;

#define MAX_LB_COLUMNS 32

typedef struct columnInfo_s {
    int pos;
    int width;
    int maxChars;
} columnInfo_t;

typedef struct listBoxDef_s {
    int startPos;
    int endPos;
    int drawPadding;
    int cursorPos;
    float elementWidth;
    float elementHeight;
    int elementStyle;
    int numColumns;
    columnInfo_t columnInfo[MAX_LB_COLUMNS];
    const char* doubleClick;
    qboolean notselectable;
} listBoxDef_t;

typedef struct editFieldDef_s {
    float minVal;
    float maxVal;
    float defVal;
    float range;
    int maxChars;
    int maxPaintChars;
    int paintOffset;
} editFieldDef_t;

#define MAX_MULTI_CVARS 32

typedef struct multiDef_s {
    const char* cvarList[MAX_MULTI_CVARS];
    const char* cvarStr[MAX_MULTI_CVARS];
    float cvarValue[MAX_MULTI_CVARS];
    int count;
    qboolean strDef;
    qboolean videoMode;
} multiDef_t;

typedef struct modelDef_s {
    int angle;
    vec3_t origin;
    float fov_x;
    float fov_y;
    int rotationSpeed;
} modelDef_t;

#define CVAR_ENABLE 0x00000001
#define CVAR_DISABLE 0x00000002
#define CVAR_SHOW 0x00000004
#define CVAR_HIDE 0x00000008

typedef struct itemDef_s {
    Window window;
    Rectangle textRect;
    int type;
    int alignment;
    int textalignment;
    float textalignx;
    float textaligny;
    int fontIndex;
    float textscale;
    int textStyle;
    const char* text;
    void* parent;
    qhandle_t asset;
    const char* mouseEnterText;
    const char* mouseExitText;
    const char* mouseEnter;
    const char* mouseExit;
    const char* action;
    const char* onFocus;
    const char* leaveFocus;
    const char* cvar;
    const char* cvarTest;
    const char* cvarTest2;
    const char* cvarTest3;
    const char* cvarTest4;
    const char* enableCvar;
    const char* enableCvar2;
    const char* enableCvar3;
    const char* enableCvar4;
    int cvarFlags;
    int precision;
    int cvarInt;
    sfxHandle_t focusSound;
    qhandle_t elementImage;
    int numColors;
    colorRangeDef_t colorRanges[MAX_COLOR_RANGES];
    float special;
    int cursorPos;
    void* typeData;
    int cellId;
    const char* defaultContent;
    int widescreen;
    int widescreenFlag;
} itemDef_t;

typedef struct {
    Window window;
    const char* font;
    qboolean fullScreen;
    int itemCount;
    int fontIndex;
    int cursorItem;
    int fadeCycle;
    float fadeClamp;
    float fadeAmount;
    const char* onOpen;
    const char* onClose;
    const char* onESC;
    const char* soundName;

    vec4_t focusColor;
    vec4_t disableColor;
    itemDef_t* items[MAX_MENUITEMS];
    int widescreen;
} menuDef_t;

typedef struct {
    const char* fontStr;
    const char* cursorStr;
    const char* gradientStr;
    fontInfo_t textFont;
    fontInfo_t smallFont;
    fontInfo_t bigFont;
    fontInfo_t extraFonts[3];
    qhandle_t cursor;
    qhandle_t gradientBar;
    qhandle_t scrollBarArrowUp;
    qhandle_t scrollBarArrowDown;
    qhandle_t scrollBarArrowLeft;
    qhandle_t scrollBarArrowRight;
    qhandle_t scrollBar;
    qhandle_t scrollBarThumb;
    qhandle_t buttonMiddle;
    qhandle_t buttonInside;
    qhandle_t solidBox;
    qhandle_t sliderBar;
    qhandle_t sliderThumb;
    sfxHandle_t menuEnterSound;
    sfxHandle_t menuExitSound;
    sfxHandle_t menuBuzzSound;
    sfxHandle_t itemFocusSound;
    float fadeClamp;
    int fadeCycle;
    float fadeAmount;
    float shadowX;
    float shadowY;
    vec4_t shadowColor;
    float shadowFadeClamp;
    qboolean fontRegistered;

    qhandle_t fxBasePic;
    qhandle_t fxPic[7];
    qhandle_t crosshairShader[NUM_CROSSHAIRS];

} cachedAssets_t;

typedef struct {
    const char* name;
    void (*handler)(itemDef_t* item, char** args);
} commandDef_t;

typedef struct {
    qhandle_t (*registerShaderNoMip)(const char* p);
    void (*setColor)(const vec4_t v);
    void (*drawHandlePic)(float x, float y, float w, float h, qhandle_t asset);
    void (*drawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
    void (*drawText)(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, int fontIndex);
    float (*textWidth)(const char* text, float scale, int limit, int fontIndex);
    float (*textHeight)(const char* text, float scale, int limit, int fontIndex);
    qhandle_t (*registerModel)(const char* p);
    void (*modelBounds)(qhandle_t model, vec3_t min, vec3_t max);
    void (*fillRect)(float x, float y, float w, float h, const vec4_t color);
    void (*drawRect)(float x, float y, float w, float h, float size, const vec4_t color);
    void (*drawSides)(float x, float y, float w, float h, float size);
    void (*drawTopBottom)(float x, float y, float w, float h, float size);
    void (*clearScene)(void);
    void (*addRefEntityToScene)(const refEntity_t* re);
    void (*renderScene)(const refdef_t* fd);
    void (*registerFont)(const char* pFontname, int pointSize, fontInfo_t* font);
    void (*ownerDrawItem)(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int fontIndex);
    float (*getValue)(int ownerDraw);
    qboolean (*ownerDrawVisible)(int flags, int flags2);
    void (*runScript)(char** p);
    void (*getTeamColor)(vec4_t* color);
    void (*getCVarString)(const char* cvar, char* buffer, int bufsize);
    float (*getCVarValue)(const char* cvar);
    void (*setCVar)(const char* cvar, const char* value);
    void (*drawTextWithCursor)(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style, int fontIndex);
    void (*setOverstrikeMode)(qboolean b);
    qboolean (*getOverstrikeMode)(void);
    void (*startLocalSound)(sfxHandle_t sfx, int channelNum);
    qboolean (*ownerDrawHandleKey)(int ownerDraw, int flags, float* special, int key);
    int (*feederCount)(float feederID);
    const char* (*feederItemText)(float feederID, int index, int column, qhandle_t* handle);
    qhandle_t (*feederItemImage)(float feederID, int index);
    void (*feederSelection)(float feederID, int index);
    void (*keynumToStringBuf)(int keynum, char* buf, int buflen);
    void (*getBindingBuf)(int keynum, char* buf, int buflen);
    void (*setBinding)(int keynum, const char* binding);
    void (*executeText)(int exec_when, const char* text);
    void (*Error)(int level, const char* error, ...) __attribute__((noreturn, format(printf, 2, 3)));
    void (*Print)(const char* msg, ...) __attribute__((format(printf, 1, 2)));
    void (*Pause)(qboolean b);
    int (*ownerDrawWidth)(int ownerDraw, float scale);
    sfxHandle_t (*registerSound)(const char* name, qboolean compressed);
    void (*startBackgroundTrack)(const char* intro, const char* loop);
    void (*stopBackgroundTrack)(void);
    int (*playCinematic)(const char* name, float x, float y, float w, float h);
    void (*stopCinematic)(int handle);
    void (*drawCinematic)(int handle, float x, float y, float w, float h);
    void (*runCinematicFrame)(int handle);
    void (*setWidescreen)(int mode);

    float yscale;
    float xscale;
    float bias;
    int realTime;
    int frameTime;
    int cursorx;
    int cursory;
    qboolean debug;

    cachedAssets_t Assets;

    glconfig_t glconfig;
    qhandle_t whiteShader;
    qhandle_t gradientImage;
    qhandle_t cursor;
    float FPS;

} displayContextDef_t;

const char* String_Alloc(const char* p);
void String_Init(void);
void String_Report(void);
void Init_Display(displayContextDef_t* dc);
void Display_ExpandMacros(char* buff);
void Menu_Init(menuDef_t* menu);
void Item_Init(itemDef_t* item);
void Menu_PostParse(menuDef_t* menu);
menuDef_t* Menu_GetFocused(void);
void Menu_HandleKey(menuDef_t* menu, int key, qboolean down);
void Menu_HandleMouseMove(menuDef_t* menu, float x, float y);
void Menu_ScrollFeeder(menuDef_t* menu, int feeder, qboolean down);
qboolean Float_Parse(char** p, float* f);
qboolean Color_Parse(char** p, vec4_t* c);
qboolean Int_Parse(char** p, int* i);
qboolean Rect_Parse(char** p, rectDef_t* r);
qboolean String_Parse(char** p, const char** out);
qboolean Script_Parse(char** p, const char** out);
qboolean PC_Float_Parse(int handle, float* f);
qboolean PC_Color_Parse(int handle, vec4_t* c);
qboolean PC_Int_Parse(int handle, int* i);
qboolean PC_Rect_Parse(int handle, rectDef_t* r);
qboolean PC_String_Parse(int handle, const char** out);
qboolean PC_Script_Parse(int handle, const char** out);
int Menu_Count(void);
void Menu_New(int handle);
void Menu_PaintAll(void);
menuDef_t* Menus_ActivateByName(const char* p);
void Menu_Reset(void);
qboolean Menus_AnyFullScreenVisible(void);
qboolean Menus_AnyVisible(void);
void Menus_Activate(menuDef_t* menu);

int UI_SelectForKey(int key);
displayContextDef_t* Display_GetContext(void);
void* Display_CaptureItem(int x, int y);
qboolean Display_MouseMove(void* p, int x, int y);
int Display_CursorType(int x, int y);
qboolean Display_KeyBindPending(void);
void Menus_OpenByName(const char* p);
menuDef_t* Menus_FindByName(const char* p);
itemDef_t* Menu_FindItemByName(menuDef_t* menu, const char* p);
void Menus_ShowByName(const char* p);
void Menus_CloseByName(const char* p);
void Display_HandleKey(int key, qboolean down, int x, int y);
void LerpColor(vec4_t a, vec4_t b, vec4_t c, float t);
void Menus_CloseAll(void);
void Menu_Paint(menuDef_t* menu, qboolean forcePaint);
void Menu_SetFeederSelection(menuDef_t* menu, int feeder, int index, const char* name);
void Display_CacheAll(void);

void* UI_Alloc(int size);
void UI_InitMemory(void);
qboolean UI_OutOfMemory(void);

void Controls_GetConfig(void);
void Controls_SetConfig(qboolean restart);
void Controls_SetDefaults(void);

int trap_PC_AddGlobalDefine(char* define);
int trap_PC_LoadSource(const char* filename);
int trap_PC_FreeSource(int handle);
int trap_PC_ReadToken(int handle, pc_token_t* pc_token);
int trap_PC_SourceFileAndLine(int handle, char* filename, int* line);

#endif
