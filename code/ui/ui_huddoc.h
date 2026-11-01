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
#ifndef UI_HUDDOC_H
#define UI_HUDDOC_H

#include "../qcommon/q_shared.h"

#define HD_MAX_TEXT 393216
#define HD_MAX_FILES 32
#define HD_MAX_TOKENS 65536
#define HD_MAX_XTOKENS 65536
#define HD_MAX_DEFINES 2048
#define HD_MAX_PROPS 16384
#define HD_MAX_ITEMS 2048
#define HD_MAX_MENUS 256
#define HD_MAX_EDITS 2048
#define HD_MAX_EDITTEXT 65536

typedef enum { HDT_NAME, HDT_NUMBER, HDT_STRING, HDT_PUNCT } hdTokType_t;

typedef enum { HDK_ASSET, HDK_MENU, HDK_ITEM } hdKind_t;

typedef struct {
    int file;
    int from, to;
    int line;
    byte type;
} hdTok_t;

typedef struct {
    int tok;
    int origin;
    byte direct;
} hdXTok_t;

typedef struct {
    int kind;
    int owner;
    int keyword;
    int x, xValues, xEnd;
    int srcFrom, srcTo;
    int valFrom, valTo;
    qboolean editable;
    int numValues;
    float values[4];
} hdProp_t;

typedef struct {
    int menu;
    int xOpen, xClose;
    int firstProp, numProps;
    int closeFrom;
    int srcFrom, srcTo;
    qboolean local;
    float rect[4];
    qboolean hasRect;
} hdItem_t;

typedef struct {
    int xOpen, xClose;
    int firstProp, numProps;
    int firstItem, numItems;
    int closeFrom;
    qboolean local;
    float rect[4];
    qboolean hasRect;
    char name[64];
} hdMenu_t;

typedef struct {
    int from, to;
    int textOfs, textLen;
} hdEdit_t;

typedef int (*hdReadFile_t)(const char* path, char* buf, int size);

typedef struct {
    char name[MAX_QPATH];
    int ofs, len;
} hdFile_t;

typedef struct {
    int nameTok;
    int bodyFirst, bodyCount;
    qboolean params;
} hdDefine_t;

typedef struct {
    char text[HD_MAX_TEXT];
    int textUsed;
    hdFile_t files[HD_MAX_FILES];
    int numFiles;
    hdTok_t toks[HD_MAX_TOKENS];
    int numToks;
    hdXTok_t xtoks[HD_MAX_XTOKENS];
    int numX;
    hdDefine_t defines[HD_MAX_DEFINES];
    int numDefines;
    hdProp_t props[HD_MAX_PROPS];
    int numProps;
    hdItem_t items[HD_MAX_ITEMS];
    int numItems;
    hdMenu_t menus[HD_MAX_MENUS];
    int numMenus;
    hdEdit_t edits[HD_MAX_EDITS];
    int numEdits;
    char editText[HD_MAX_EDITTEXT];
    int editTextUsed;
    int unknownKeywords;
    int unsupported;
    int endFrom;
    char error[256];
    hdReadFile_t readFile;
} hdDoc_t;

qboolean HD_Load(hdDoc_t* d, const char* name, const char* text, int len, hdReadFile_t readFile);
const char* HD_Keyword(int kind, int keyword);
int HD_FindProp(const hdDoc_t* d, int kind, int owner, const char* key);
qboolean HD_SetValues(hdDoc_t* d, int kind, int owner, const char* key, const float* values, int count);
qboolean HD_SetRect(hdDoc_t* d, int item, const float rect[4]);
qboolean HD_Insert(hdDoc_t* d, int at, const char* text);
qboolean HD_RemoveItem(hdDoc_t* d, int item);
void HD_ClearEdits(hdDoc_t* d);
int HD_Write(const hdDoc_t* d, char* out, int outSize);
void HD_TokenString(const hdDoc_t* d, int tok, char* out, int size);

#endif
