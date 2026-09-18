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
// cg_info.c -- display information while data is being loading

#include "cg_local.h"
#include "../../ui/menudef.h"

#define MAX_LOADING_PLAYER_ICONS 16
#define MAX_LOADING_ITEM_ICONS 26

// [QL] loading screen progress has 4 stages
#define NUM_LOADING_STAGES 4

static int loadingPlayerIconCount;
static int loadingItemIconCount;
static qhandle_t loadingPlayerIcons[MAX_LOADING_PLAYER_ICONS];
static qhandle_t loadingItemIcons[MAX_LOADING_ITEM_ICONS];

// [QL] use shared gametypeDisplayNames[] from bg_misc.c

/*
======================
CG_LoadingString

======================
*/
void CG_LoadingString(const char* s) {
    Q_strncpyz(cg.infoScreenText, s, sizeof(cg.infoScreenText));

    trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem(int itemNum) {
    gitem_t* item;

    item = &bg_itemlist[itemNum];

    if (item->icon && loadingItemIconCount < MAX_LOADING_ITEM_ICONS) {
        loadingItemIcons[loadingItemIconCount++] = trap_R_RegisterShaderNoMip(item->icon);
    }

    CG_LoadingString(item->pickup_name);
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient(int clientNum) {
    const char* info;
    char* skin;
    char personality[MAX_QPATH];
    char model[MAX_QPATH];
    char iconName[MAX_QPATH];

    info = CG_ConfigString(CS_PLAYERS + clientNum);

    if (loadingPlayerIconCount < MAX_LOADING_PLAYER_ICONS) {
        Q_strncpyz(model, Info_ValueForKey(info, "model"), sizeof(model));
        skin = strrchr(model, '/');
        if (skin) {
            *skin++ = '\0';
        } else {
            skin = "default";
        }

        Com_sprintf(iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", model, skin);

        loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip(iconName);
        if (!loadingPlayerIcons[loadingPlayerIconCount]) {
            Com_sprintf(iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", DEFAULT_MODEL, "default");
            loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip(iconName);
        }
        if (loadingPlayerIcons[loadingPlayerIconCount]) {
            loadingPlayerIconCount++;
        }
    }

    Q_strncpyz(personality, Info_ValueForKey(info, "n"), sizeof(personality));

    CG_LoadingString(personality);
}

/*
====================
CG_DrawInformation

[ioquakelive] La pantalla de carga, con el diseño del front end. Es un rediseño pedido por el
jugador, no el de retail: lo que dibujaba el binario esta en el historial de git, y sus
direcciones quedan anotadas aqui por si hay que volver.

  0x1001ce10  la version sin nada que cargar, solo el humo y "Awaiting gamestate..."
  0x1001d22d  el nombre del mapa en x 10 con el tope en y 400, y los autores en 447 y 462
  0x1001d48d  el modo de juego alineado a la derecha en 620
  0x1001d524  el texto de carga centrado en y 370

Se conserva todo lo que informaba: el mapa, el modo, los autores, que se esta cargando y
cuanto falta. La composicion es centrada: la imagen del mapa grande al medio, el modo en dorado
encima, el nombre y los autores debajo, y una barra fina del ancho de la imagen. El levelshot
tambien va de fondo, pero muy oscurecido, como ambiente. El logo va donde lo pone la fila de
arriba del front end, y la letra es la del front end, Barlow.
====================
*/

// Barlow, de pak01, por la via de las fuentes del motor; -1 si no esta, y entonces Noto Sans
static int ldFonts[2] = {-2, -2};

static int LD_Font(qboolean strong) {
    if (ldFonts[0] == -2) {
        ldFonts[0] = trap_R_Font_Register("fonts/Barlow-Regular.ttf");
        ldFonts[1] = trap_R_Font_Register("fonts/Barlow-SemiBold.ttf");
    }
    return ldFonts[strong ? 1 : 0];
}

// text centred on cx with its baseline on y
static void LD_Centered(float cx, float y, float scale, const vec4_t color, const char* text, qboolean strong) {
    int font = LD_Font(strong);
    float w;

    if (!text || !text[0]) {
        return;
    }
    if (font >= 0) {
        CG_MeasureTextInFont(text, font, scale, &w, NULL);
        CG_DrawTextInFont(cx - w * 0.5f, y, font, scale, color, text, ITEM_TEXTSTYLE_NORMAL);
    } else {
        w = (float)CG_DrawTextWidth(text, scale, 0, 1);
        CG_DrawText(cx - w * 0.5f, y, 1, scale, (float*)color, text, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    }
}

#define LD_PIC_W 384.0f  // la imagen del mapa, en 16:9
#define LD_PIC_H 216.0f
#define LD_PIC_Y 118.0f

void CG_DrawInformation(void) {
    // la paleta del front end: su texto, su gris secundario y el amarillo del logo, que es el acento
    static vec4_t colorVeil = {0.0f, 0.0f, 0.0f, 0.80f};
    static vec4_t colorText = {0.87f, 0.87f, 0.87f, 1.0f};
    static vec4_t colorMuted = {0.6f, 0.6f, 0.6f, 1.0f};
    static vec4_t colorGold = {0.969f, 0.820f, 0.0f, 1.0f};
    static vec4_t colorTrack = {1.0f, 1.0f, 1.0f, 0.14f};
    const char* s;
    const char* info;
    const char* sAuthor;
    const char* sAuthor2;
    char gametype[64];
    qhandle_t levelshot;
    float s1, s2, picX;

    // [QL] compute aspect-correct texture coords for levelshot (1920x1080 source)
    s1 = ((1920.0f - (1080.0f / (float)cgs.glconfig.vidHeight) * (float)cgs.glconfig.vidWidth) / 1920.0f) * 0.5f;
    s2 = 1.0f - s1;

    info = CG_ConfigString(CS_SERVERINFO);

    // [QL] retail reads the custom settings again here, on every call
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));

    s = Info_ValueForKey(info, "mapname");
    levelshot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", s));
    if (!levelshot) {
        levelshot = trap_R_RegisterShaderNoMip("menu/art/unknownmap");
    }
    trap_R_SetColor(NULL);

    // [QL] 0x1001ce10: with a snapshot and nothing left to load, only the smoke backdrop and a line
    if (!cg.infoScreenText[0] && cg.snap) {
        trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                              cgs.media.backscreenSmokeShader);
        CG_SetWidescreen(WIDESCREEN_CENTER);
        LD_Centered(320, 240, 0.16f, colorMuted, "Awaiting gamestate", qfalse);
        CG_SetWidescreen(WIDESCREEN_STRETCH);
        return;
    }

    // the map, darkened right down, as the room the screen sits in
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                          levelshot);
    CG_SetWidescreen(WIDESCREEN_LEFT);
    CG_FillRect(-400, 0, 1440, 480, colorVeil);

    CG_SetWidescreen(WIDESCREEN_CENTER);

    // the logo where the front end's top row has it (x 405, y 26, 196 by 49 on a 1080 page)
    if (cgs.media.qlLogoShader) {
        CG_DrawPic(73, 11.6f, 87, 21.8f, cgs.media.qlLogoShader);
    }

    // the game type over the picture, in gold
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        Q_strncpyz(gametype, gametypeDisplayNames[cgs.gametype], sizeof(gametype));
    } else {
        Q_strncpyz(gametype, "Unknown Gametype", sizeof(gametype));
    }
    Q_strupr(gametype);
    LD_Centered(320, LD_PIC_Y - 12, 0.15f, colorGold, gametype, qtrue);

    // the picture itself, in the middle
    picX = 320 - LD_PIC_W * 0.5f;
    CG_DrawPic(picX, LD_PIC_Y, LD_PIC_W, LD_PIC_H, levelshot);

    // the map's name and its authors under it
    LD_Centered(320, LD_PIC_Y + LD_PIC_H + 30, 0.30f, colorText, CG_ConfigString(CS_MESSAGE), qtrue);
    sAuthor = CG_ConfigString(CS_AUTHOR);
    if (sAuthor[0]) {
        sAuthor2 = CG_ConfigString(CS_AUTHOR2);
        s = sAuthor2[0] ? va("%s  /  %s", sAuthor, sAuthor2) : sAuthor;
        LD_Centered(320, LD_PIC_Y + LD_PIC_H + 48, 0.13f, colorMuted, s, qfalse);
    }

    // how far the load has come: a fine bar the picture's width, with its track always there so
    // what is left shows as well as what is done, and under it what is loading
    CG_FillRect(picX, LD_PIC_Y + LD_PIC_H + 68, LD_PIC_W, 1.2f, colorTrack);
    CG_FillRect(picX, LD_PIC_Y + LD_PIC_H + 68, LD_PIC_W * ((float)cg.loadingStage / NUM_LOADING_STAGES), 1.2f,
                colorGold);
    s = cg.infoScreenText[0] ? va("Loading %s", cg.infoScreenText) : "Awaiting snapshot";
    LD_Centered(320, LD_PIC_Y + LD_PIC_H + 88, 0.13f, colorMuted, s, qfalse);

    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
