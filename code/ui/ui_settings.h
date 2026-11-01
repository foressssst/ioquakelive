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
#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

typedef enum {
    ST_TAB_INPUT,
    ST_TAB_GAMEPAD,
    ST_TAB_BASIC,
    ST_TAB_GAME,
    ST_TAB_HUD,
    ST_TAB_TEAM,
    ST_TAB_WEAPONS,
    ST_TAB_VIDEO,
    ST_TAB_SOUND,
    ST_TAB_SPECTATING,
    ST_NUM_TABS
} stTab_t;

typedef enum {
    ST_HEADING,
    ST_TEXT,
    ST_BREAK,
    ST_BOOL,
    ST_SELECT,
    ST_RANGE,
    ST_INPUT,
    ST_PALETTE,
    ST_RGB,
    ST_MODEL,
    ST_MODELSKIN,
    ST_CROSSHAIR,
    ST_BIND
} stWidget_t;

typedef struct {
    const char* label;
    const char* value;
} stOption_t;

typedef struct {
    int tab;
    const char* title;
    const char* cvar;
    int widget;
    float min, max, step;
    const stOption_t* options;
    int numOptions;
    const char* tip;
    int column;
} stCvar_t;

int UI_SettingsCount(void);
const stCvar_t* UI_SettingsAt(int i);
const stCvar_t* UI_SettingsCvar(const char* cvar);
const vec4_t* UI_CrosshairPalette(void);

#endif
