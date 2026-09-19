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
cuanto falta. La composicion sigue al menu: una banda gris a la izquierda con toda la
composicion es cinematica, elegida por el jugador: el mapa ocupa toda la pantalla y la
informacion va en una franja abajo que la cruza -- a la izquierda el modo, el mapa y sus
autores; a la derecha las caras de quienes van entrando y que se esta cargando --, con la barra
de progreso al pie, de lado a lado. Los grises y la letra son los del front end, sin dorados.

Las caras se recolectan en CG_LoadingClient y hasta este redisenio no se dibujaban en ningun
lado.

Las medidas usan LD_Edge y LD_ViewW porque en WIDESCREEN_LEFT la x = 0 no es el borde de la
pantalla sino el del area 4:3: sin eso, la franja y la barra no llegaban a los bordes.

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

// text with its left edge on x and its baseline on y
static void LD_Left(float x, float y, float scale, const vec4_t color, const char* text, qboolean strong) {
    int font = LD_Font(strong);

    if (!text || !text[0]) {
        return;
    }
    if (font >= 0) {
        CG_DrawTextInFont(x, y, font, scale, color, text, ITEM_TEXTSTYLE_NORMAL);
    } else {
        CG_DrawText(x, y, 1, scale, (float*)color, text, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    }
}

// text with its right edge on x and its baseline on y
static void LD_Right(float x, float y, float scale, const vec4_t color, const char* text, qboolean strong) {
    int font = LD_Font(strong);
    float w;

    if (!text || !text[0]) {
        return;
    }
    if (font >= 0) {
        CG_MeasureTextInFont(text, font, scale, &w, NULL);
        CG_DrawTextInFont(x - w, y, font, scale, color, text, ITEM_TEXTSTYLE_NORMAL);
    } else {
        w = (float)CG_DrawTextWidth(text, scale, 0, 1);
        CG_DrawText(x - w, y, 1, scale, (float*)color, text, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    }
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

#define LD_STRIP 96.0f   // la franja de abajo, de lado a lado
#define LD_MARGIN 44.0f  // el margen a los dos lados
#define LD_FACE 26.0f    // la cara de un jugador
#define LD_FACE_GAP 6.0f

// en WIDESCREEN_LEFT la x = 0 cae en el borde del area 4:3, no en el de la pantalla: en 16:9 el
// borde de verdad esta 107 mas a la izquierda. LD_Edge devuelve esa x negativa y LD_ViewW, el
// ancho visible completo (853 en 16:9)
static float LD_ViewW(void) {
    float aspect = (float)cgs.glconfig.vidWidth / (float)cgs.glconfig.vidHeight;

    return 640.0f * aspect * 0.75f;
}

static float LD_Edge(void) {
    return -(LD_ViewW() - 640.0f) * 0.5f;
}

// las caras de los que van entrando, en una fila que termina en x
static void LD_DrawPlayersRight(float x, float y) {
    int i;

    for (i = 0; i < loadingPlayerIconCount; i++) {
        float px = x - (loadingPlayerIconCount - i) * (LD_FACE + LD_FACE_GAP) + LD_FACE_GAP;

        CG_DrawPic(px, y, LD_FACE, LD_FACE, loadingPlayerIcons[i]);
    }
}

void CG_DrawInformation(void) {
    // la paleta del menu: el gris de los paneles, su texto y su gris apagado
    static vec4_t colorPanel = {0.118f, 0.122f, 0.129f, 0.92f};
    static vec4_t colorText = {0.87f, 0.87f, 0.87f, 1.0f};
    static vec4_t colorMuted = {0.6f, 0.6f, 0.6f, 1.0f};
    static vec4_t colorTrack = {1.0f, 1.0f, 1.0f, 0.14f};
    const char* s;
    const char* info;
    const char* sAuthor;
    const char* sAuthor2;
    char gametype[64];
    qhandle_t levelshot;
    float s1, s2;
    float left, right, y;

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

    // el mapa ocupa toda la pantalla: es la imagen, no un recuadro
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight, s1, 0, s2, 1.0f,
                          levelshot);

    CG_SetWidescreen(WIDESCREEN_LEFT);
    left = LD_Edge() + LD_MARGIN;
    right = LD_Edge() + LD_ViewW() - LD_MARGIN;

    // la franja de abajo, plana y de lado a lado
    CG_FillRect(LD_Edge(), 480.0f - LD_STRIP, LD_ViewW(), LD_STRIP, colorPanel);

    // el logo arriba, contra el mismo margen
    if (cgs.media.qlLogoShader) {
        CG_DrawPic(left, 34.0f, 116.0f, 29.0f, cgs.media.qlLogoShader);
    }

    // a la izquierda de la franja: el modo, el mapa y sus autores
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        Q_strncpyz(gametype, gametypeDisplayNames[cgs.gametype], sizeof(gametype));
    } else {
        Q_strncpyz(gametype, "Unknown Gametype", sizeof(gametype));
    }
    Q_strupr(gametype);
    y = 480.0f - LD_STRIP + 26.0f;
    LD_Left(left, y, 0.12f, colorMuted, gametype, qtrue);
    LD_Left(left, y + 30.0f, 0.30f, colorText, CG_ConfigString(CS_MESSAGE), qtrue);
    sAuthor = CG_ConfigString(CS_AUTHOR);
    if (sAuthor[0]) {
        sAuthor2 = CG_ConfigString(CS_AUTHOR2);
        s = sAuthor2[0] ? va("%s  /  %s", sAuthor, sAuthor2) : sAuthor;
        LD_Left(left, y + 50.0f, 0.12f, colorMuted, s, qfalse);
    }

    // a la derecha: las caras de quienes van entrando y, debajo, que se esta cargando. Las caras
    // se juntan en CG_LoadingClient y hasta este redisenio no se dibujaban en ningun lado
    if (loadingPlayerIconCount) {
        LD_DrawPlayersRight(right, y - 4.0f);
    }
    s = cg.infoScreenText[0] ? va("Loading %s", cg.infoScreenText) : "Awaiting snapshot";
    LD_Right(right, y + 50.0f, 0.12f, colorMuted, s, qfalse);

    // la barra de progreso cruza todo el ancho, pegada al borde de abajo
    CG_FillRect(LD_Edge(), 477.0f, LD_ViewW(), 3.0f, colorTrack);
    CG_FillRect(LD_Edge(), 477.0f, LD_ViewW() * ((float)cg.loadingStage / NUM_LOADING_STAGES), 3.0f, colorText);

    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
