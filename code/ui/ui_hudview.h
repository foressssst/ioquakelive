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
#ifndef UI_HUDVIEW_H
#define UI_HUDVIEW_H

#include "ui_local.h"

#define HV_MAX_MENUS 128
#define HV_MAX_FILES 16

typedef struct {
    int gametype;
    int team;
    qboolean warmup;
    qboolean hasFlag;
    qboolean showAll;
} hvScene_t;

extern hvScene_t hvScene;

int HV_ReadList(const char* listFile, char (*files)[MAX_QPATH], int max);
qboolean HV_Load(char (*files)[MAX_QPATH], int numFiles, int* menusPerFile);
void HV_Unload(void);
qboolean HV_Loaded(void);
int HV_MenuCount(void);
menuDef_t* HV_Menu(int index);
menuDef_t* HV_FindMenu(const char* name);
menuDef_t* HV_AddMenu(const char* name);

void HV_SetCanvas(float x, float y, float w, float h);
void HV_Paint(void);
void HV_PaintItem(itemDef_t* item);
qboolean HV_PaintLoose(itemDef_t* item, float* out);

int HV_ItemAnchor(const itemDef_t* item);
void HV_ToScreen(int anchor, float* x, float* y, float* w, float* h);
void HV_FromScreen(int anchor, float* x, float* y);
float HV_UnitsPerPixel(int anchor);
void HV_ItemScreenRect(itemDef_t* item, float* out);
void HV_ItemHitRect(itemDef_t* item, float* out);
qboolean HV_ItemPaintRect(const itemDef_t* item, float* out);
qboolean HV_ItemShown(const itemDef_t* item);
const char* HV_OwnerDrawName(int ownerDraw);
const char* HV_ItemLabel(const itemDef_t* item);
const char* HV_ItemKind(const itemDef_t* item);
qhandle_t HV_ItemThumb(const itemDef_t* item, const char** text);
const char* HV_MenuLabel(const menuDef_t* menu);

#endif
