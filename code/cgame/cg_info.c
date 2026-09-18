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
direcciones quedan anotadas aqui abajo por si hay que volver.

  0x1001ce10  la version sin nada que cargar, solo el humo y "Awaiting gamestate..."
  0x1001d22d  el nombre del mapa en x 10 con el tope en y 400, y los autores en 447 y 462
  0x1001d48d  el modo de juego alineado a la derecha en 620
  0x1001d524  el texto de carga centrado en y 370

Se conserva todo lo que informaba: el levelshot, la marca, el modo, el nombre del mapa, sus
autores, que se esta cargando y cuanto falta. Cambia como se ve: Noto Sans en vez de Handel
Gothic, un velo sobre el levelshot para que el texto se lea siempre, el modo como etiqueta
espaciada, y el progreso como una barra fina de lado a lado.
====================
*/
void CG_DrawInformation(void) {
    const char *s;
    const char *info;
    const char *sAuthor, *sAuthor2;
    const char *gametypeName;
    qhandle_t levelshot;
    int i;
    int w;
    float s1, s2;

    // [ioquakelive] la paleta del front end: el negro de sus paneles, el gris de su texto
    // secundario y el amarillo del logo de QUAKE LIVE, que es su acento
    static vec4_t colorVeil     = { 0.0f,   0.0f,   0.0f,   0.45f };
    static vec4_t colorPanel    = { 0.035f, 0.035f, 0.035f, 0.94f };
    static vec4_t colorMuted    = { 0.6f,   0.6f,   0.6f,   1.0f  };
    static vec4_t colorGold     = { 0.969f, 0.820f, 0.0f,   1.0f  };
    static vec4_t colorGoldLine = { 0.969f, 0.820f, 0.0f,   0.45f };
    static vec4_t colorTrack    = { 1.0f,   1.0f,   1.0f,   0.14f };

#define LOADING_PANEL_Y 372
#define LOADING_BAR_Y 452
#define LOADING_FADE_BANDS 10

    // [QL] compute aspect-correct texture coords for levelshot (1920x1080 source)
    s1 = ((1920.0f - (1080.0f / (float)cgs.glconfig.vidHeight) * (float)cgs.glconfig.vidWidth)
          / 1920.0f) * 0.5f;
    s2 = 1.0f - s1;

    info = CG_ConfigString(CS_SERVERINFO);

    // [QL] retail reads the custom settings again here, on every call
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));

    // register levelshot
    s = Info_ValueForKey(info, "mapname");
    levelshot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", s));
    if (!levelshot) {
        levelshot = trap_R_RegisterShaderNoMip("menu/art/unknownmap");
    }
    trap_R_SetColor(NULL);

    // [QL] cgamex86.dll 0x1001ce10 (0x1001cf80): with a snapshot and nothing left to load, only
    // the smoke backdrop over the whole screen and "Awaiting gamestate..." centred at y 178, in
    // font 0 at 0.3, shadowed
    if (!cg.infoScreenText[0] && cg.snap) {
        CG_SetWidescreen(WIDESCREEN_CENTER);
        trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight,
                              s1, 0, s2, 1.0f, cgs.media.backscreenSmokeShader);
        s = "Awaiting gamestate...";
        w = CG_DrawTextWidth(s, 0.3f, 0, 0);
        CG_DrawText(320 - w * 0.5f, 178, 0, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
        CG_SetWidescreen(WIDESCREEN_STRETCH);
        return;
    }

    // el levelshot ocupa la pantalla, con las coordenadas corregidas para que no se deforme
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight,
                          s1, 0, s2, 1.0f, levelshot);

    // --- el velo ---
    // Un levelshot cualquiera puede ser claro, oscuro o tener mucho detalle justo donde va el
    // texto. El velo lo apaga parejo, y abajo se hace mas denso para que el nombre del mapa y
    // la barra tengan siempre fondo. Asi el diseño no depende de que foto toco.
    CG_SetWidescreen(WIDESCREEN_LEFT);
    CG_FillRect(-400, 0, 1440, 480, colorVeil);
    for (i = 0; i < LOADING_FADE_BANDS; i++) {
        vec4_t band;

        Vector4Copy(colorPanel, band);
        band[3] = colorPanel[3] * ((float)i / LOADING_FADE_BANDS);
        CG_FillRect(-400, (float)(LOADING_PANEL_Y + i * 4 - LOADING_FADE_BANDS * 4), 1440, 4, band);
    }
    CG_FillRect(-400, LOADING_PANEL_Y, 1440, 480 - LOADING_PANEL_Y, colorPanel);

    // --- la marca, arriba a la izquierda ---
    if (cgs.media.qlLogoShader) {
        CG_DrawPic(28, 26, 176, 44, cgs.media.qlLogoShader);
    }

    // --- el nombre del mapa y sus autores, abajo a la izquierda ---
    s = CG_ConfigString(CS_MESSAGE);
    CG_DrawText(28, (float)(LOADING_PANEL_Y + 34 + CG_DrawTextHeight(s, 0.42f, 0, 1)), 1, 0.42f, colorWhite, s, 0, 0,
                ITEM_TEXTSTYLE_NORMAL);

    sAuthor = CG_ConfigString(CS_AUTHOR);
    if (sAuthor[0]) {
        sAuthor2 = CG_ConfigString(CS_AUTHOR2);
        s = sAuthor2[0] ? va("%s  %s", sAuthor, sAuthor2) : sAuthor;
        CG_DrawText(28, (float)(LOADING_PANEL_Y + 56 + CG_DrawTextHeight(s, 0.18f, 0, 1)), 1, 0.18f, colorMuted, s, 0,
                    0, ITEM_TEXTSTYLE_NORMAL);
    }

    // --- el modo de juego, arriba a la derecha ---
    CG_SetWidescreen(WIDESCREEN_RIGHT);
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        gametypeName = gametypeDisplayNames[cgs.gametype];
    } else {
        gametypeName = "Unknown Gametype";
    }
    w = CG_DrawTextWidth(gametypeName, 0.26f, 0, 1);
    CG_DrawText((float)(612 - w), (float)(40 + CG_DrawTextHeight(gametypeName, 0.26f, 0, 1)), 1, 0.26f, colorGold,
                gametypeName, 0, 0, ITEM_TEXTSTYLE_NORMAL);
    CG_FillRect(612 - w, 50, (float)w, 1, colorGoldLine);

    // --- que se esta cargando, y cuanto falta ---
    CG_SetWidescreen(WIDESCREEN_LEFT);
    if (cg.infoScreenText[0]) {
        s = va("Cargando %s", cg.infoScreenText);
    } else {
        s = "Esperando al servidor";
    }
    CG_DrawText(28, (float)(LOADING_BAR_Y - 8), 1, 0.16f, colorMuted, s, 0, 0, ITEM_TEXTSTYLE_NORMAL);

    // La barra va de lado a lado y avanza por etapas, que es lo unico que el motor sabe del
    // progreso. Debajo queda siempre su carril, asi se ve cuanto falta y no solo cuanto va.
    CG_FillRect(28, LOADING_BAR_Y, 584, 2, colorTrack);
    CG_FillRect(28, LOADING_BAR_Y, 584.0f * ((float)cg.loadingStage / NUM_LOADING_STAGES), 2, colorGold);

    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
