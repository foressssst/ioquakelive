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
// cg_hudtune.c -- the medals, the frag message and the centre print, opened up to cvars
//
// [wolfcam] wolfcamql lets these be placed, sized, lettered and, for the frag message,
// written with tokens. Here that is a tuning aid: the cvars are not archived, their
// defaults draw exactly as retail, and the values picked in game become the fixed layout.
#include "cg_local.h"
#include "../ui/ui_shared.h"

static vmCvar_t cg_drawRewardsX;
static vmCvar_t cg_drawRewardsY;
static vmCvar_t cg_drawRewardsAlign;
static vmCvar_t cg_drawRewardsImageScale;
static vmCvar_t cg_drawRewardsFont;
static vmCvar_t cg_drawRewardsScale;
static vmCvar_t cg_drawRewardsStyle;
static vmCvar_t cg_drawRewardsColor;
static vmCvar_t cg_drawRewardsAlpha;
static vmCvar_t cg_drawRewardsWideScreen;

static vmCvar_t cg_drawFragMessageX;
static vmCvar_t cg_drawFragMessageY;
static vmCvar_t cg_drawFragMessageAlign;
static vmCvar_t cg_drawFragMessageScale;
static vmCvar_t cg_drawFragMessageFont;
static vmCvar_t cg_drawFragMessageStyle;
static vmCvar_t cg_drawFragMessageLineSpacing;
static vmCvar_t cg_drawFragMessageColor;
static vmCvar_t cg_drawFragMessageAlpha;
static vmCvar_t cg_drawFragMessageTime;
static vmCvar_t cg_drawFragMessageFadeTime;
static vmCvar_t cg_drawFragMessageIconScale;
static vmCvar_t cg_drawFragMessageWideScreen;
static vmCvar_t cg_drawFragMessageTokens;
static vmCvar_t cg_drawFragMessageTeamTokens;
static vmCvar_t cg_drawFragMessageThawTokens;
static vmCvar_t cg_drawFragMessageFreezeTokens;
static vmCvar_t cg_drawFragMessageFreezeTeamTokens;

static vmCvar_t cg_drawCenterPrintX;
static vmCvar_t cg_drawCenterPrintY;
static vmCvar_t cg_drawCenterPrintAlign;
static vmCvar_t cg_drawCenterPrintScale;
static vmCvar_t cg_drawCenterPrintFont;
static vmCvar_t cg_drawCenterPrintStyle;
static vmCvar_t cg_drawCenterPrintLineSpacing;
static vmCvar_t cg_drawCenterPrintColor;
static vmCvar_t cg_drawCenterPrintAlpha;
static vmCvar_t cg_drawCenterPrintTime;
static vmCvar_t cg_drawCenterPrintFadeTime;
static vmCvar_t cg_drawCenterPrintWideScreen;

// An empty value keeps retail's, which for some of these depends on the call.
static const struct {
    vmCvar_t* vmCvar;
    const char* name;
    const char* defaultString;
} tuneCvars[] = {
    {&cg_drawRewardsX, "cg_drawRewardsX", "320"},
    {&cg_drawRewardsY, "cg_drawRewardsY", "42"},
    {&cg_drawRewardsAlign, "cg_drawRewardsAlign", "1"},
    {&cg_drawRewardsImageScale, "cg_drawRewardsImageScale", "1"},
    {&cg_drawRewardsFont, "cg_drawRewardsFont", ""},
    {&cg_drawRewardsScale, "cg_drawRewardsScale", "0.2"},
    {&cg_drawRewardsStyle, "cg_drawRewardsStyle", "0"},
    {&cg_drawRewardsColor, "cg_drawRewardsColor", ""},
    {&cg_drawRewardsAlpha, "cg_drawRewardsAlpha", ""},
    {&cg_drawRewardsWideScreen, "cg_drawRewardsWideScreen", ""},

    {&cg_drawFragMessageX, "cg_drawFragMessageX", "320"},
    {&cg_drawFragMessageY, "cg_drawFragMessageY", ""},
    {&cg_drawFragMessageAlign, "cg_drawFragMessageAlign", "1"},
    {&cg_drawFragMessageScale, "cg_drawFragMessageScale", "0.3"},
    {&cg_drawFragMessageFont, "cg_drawFragMessageFont", ""},
    {&cg_drawFragMessageStyle, "cg_drawFragMessageStyle", "6"},
    {&cg_drawFragMessageLineSpacing, "cg_drawFragMessageLineSpacing", "6"},
    {&cg_drawFragMessageColor, "cg_drawFragMessageColor", ""},
    {&cg_drawFragMessageAlpha, "cg_drawFragMessageAlpha", ""},
    {&cg_drawFragMessageTime, "cg_drawFragMessageTime", ""},
    {&cg_drawFragMessageFadeTime, "cg_drawFragMessageFadeTime", "200"},
    {&cg_drawFragMessageIconScale, "cg_drawFragMessageIconScale", "1"},
    {&cg_drawFragMessageWideScreen, "cg_drawFragMessageWideScreen", ""},
    {&cg_drawFragMessageTokens, "cg_drawFragMessageTokens", ""},
    {&cg_drawFragMessageTeamTokens, "cg_drawFragMessageTeamTokens", ""},
    {&cg_drawFragMessageThawTokens, "cg_drawFragMessageThawTokens", ""},
    {&cg_drawFragMessageFreezeTokens, "cg_drawFragMessageFreezeTokens", ""},
    {&cg_drawFragMessageFreezeTeamTokens, "cg_drawFragMessageFreezeTeamTokens", ""},

    {&cg_drawCenterPrintX, "cg_drawCenterPrintX", "320"},
    {&cg_drawCenterPrintY, "cg_drawCenterPrintY", ""},
    {&cg_drawCenterPrintAlign, "cg_drawCenterPrintAlign", "1"},
    {&cg_drawCenterPrintScale, "cg_drawCenterPrintScale", ""},
    {&cg_drawCenterPrintFont, "cg_drawCenterPrintFont", ""},
    {&cg_drawCenterPrintStyle, "cg_drawCenterPrintStyle", "6"},
    {&cg_drawCenterPrintLineSpacing, "cg_drawCenterPrintLineSpacing", "6"},
    {&cg_drawCenterPrintColor, "cg_drawCenterPrintColor", ""},
    {&cg_drawCenterPrintAlpha, "cg_drawCenterPrintAlpha", ""},
    {&cg_drawCenterPrintTime, "cg_drawCenterPrintTime", ""},
    {&cg_drawCenterPrintFadeTime, "cg_drawCenterPrintFadeTime", "200"},
    {&cg_drawCenterPrintWideScreen, "cg_drawCenterPrintWideScreen", ""},
};

void CG_RegisterTuneCvars(void) {
    int i;

    for (i = 0; i < ARRAY_LEN(tuneCvars); i++) {
        trap_Cvar_Register(tuneCvars[i].vmCvar, tuneCvars[i].name, tuneCvars[i].defaultString, 0);
    }
}

void CG_UpdateTuneCvars(void) {
    int i;

    for (i = 0; i < ARRAY_LEN(tuneCvars); i++) {
        trap_Cvar_Update(tuneCvars[i].vmCvar);
    }
}

// the cvar's number, or def when it is empty
static float CG_TuneValue(const vmCvar_t* cv, float def) {
    return cv->string[0] ? cv->value : def;
}

static int CG_TuneInt(const vmCvar_t* cv, int def) {
    return cv->string[0] ? cv->integer : def;
}

// "0xRRGGBB" into rgb, and a 0-255 alpha into a; each is left alone when its cvar is empty
static void CG_TuneColor(const vmCvar_t* rgb, const vmCvar_t* alpha, vec4_t color) {
    int value = rgb->string[0] ? Com_HexStrToInt(rgb->string) : -1;

    if (value >= 0) {
        color[0] = ((value >> 16) & 0xff) / 255.0f;
        color[1] = ((value >> 8) & 0xff) / 255.0f;
        color[2] = (value & 0xff) / 255.0f;
    }
    if (alpha->string[0]) {
        color[3] *= Com_Clamp(0.0f, 255.0f, alpha->value) / 255.0f;
    }
}

typedef struct {
    int modificationCount;
    int font;
} tuneFont_t;

// the engine font a font cvar names ("rajdhani", "fonts/rajdhani.otf"...), def when it is
// empty or the font would not load
static int CG_TuneFont(const vmCvar_t* cv, tuneFont_t* cache, int def) {
    if (!cv->string[0]) {
        return def;
    }
    if (cache->modificationCount != cv->modificationCount) {
        cache->modificationCount = cv->modificationCount;
        cache->font = trap_R_Font_Register(cv->string);
    }
    return (cache->font >= 0) ? cache->font : def;
}

static tuneFont_t rewardsFont = {-1, -1};
static tuneFont_t fragMessageFont = {-1, -1};
static tuneFont_t centerPrintFont = {-1, -1};

/*
===================
CG_DrawRewardMedals

The medals of CG_DrawReward. Retail (cgamex86.dll 0x1000ac30) draws 32x32 icons with their
top at y 42: one per medal, 48 apart and centred on 320, or past cg_drawRewardsRowSize a
single one at x 296 with the count at (336, 66) in font 0, scale 0.2, solid white.
===================
*/
void CG_DrawRewardMedals(int count, qhandle_t shader, const vec4_t fade) {
    int widescreen = cg_currentWidescreen;
    float imageScale = CG_TuneValue(&cg_drawRewardsImageScale, 1.0f);
    float size = 32.0f * imageScale;
    float step = 48.0f * imageScale;
    float x = CG_TuneValue(&cg_drawRewardsX, 320.0f);
    float y = CG_TuneValue(&cg_drawRewardsY, 42.0f);
    int align = CG_TuneInt(&cg_drawRewardsAlign, 1);
    float width = (count > cg_drawRewardsRowSize.integer) ? step : count * step;
    vec4_t textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    int i;

    if (align == 1) {
        x -= width / 2.0f;
    } else if (align == 2) {
        x -= width;
    }

    CG_SetWidescreen(CG_TuneInt(&cg_drawRewardsWideScreen, widescreen));
    trap_R_SetColor(fade);
    if (count > cg_drawRewardsRowSize.integer) {
        CG_DrawRewardIcon(x, y, size, shader);
        CG_TuneColor(&cg_drawRewardsColor, &cg_drawRewardsAlpha, textColor);
        CG_DrawTextInFont(x + size + 8.0f * imageScale, y + 24.0f * imageScale,
                          CG_TuneFont(&cg_drawRewardsFont, &rewardsFont, 0), CG_TuneValue(&cg_drawRewardsScale, 0.2f),
                          textColor, va("%d", count), CG_TuneInt(&cg_drawRewardsStyle, 0));
    } else {
        for (i = 0; i < count; i++) {
            CG_DrawRewardIcon(x + step * i, y, size, shader);
        }
    }
    trap_R_SetColor(NULL);
    CG_SetWidescreen(widescreen);
}

/*
===================
CG_CenterPrintLayout

How CG_DrawCenterString lays out the print showing, the frag message included. Retail
(0x1000beb0): lines centred on 320 from the print's y, in font 0 at the print's scale with
ITEM_TEXTSTYLE_SHADOWEDMORE, 6 units apart, shown cg_centertime seconds and faded over the
last 200 ms. The frag message is CG_CenterPrint(s, 90 or 144 in a warmup, 0.3).
===================
*/
void CG_CenterPrintLayout(centerPrintLayout_t* layout) {
    qboolean frag = cg.centerPrintFrag;

    layout->x = CG_TuneValue(frag ? &cg_drawFragMessageX : &cg_drawCenterPrintX, 320.0f);
    layout->align = CG_TuneInt(frag ? &cg_drawFragMessageAlign : &cg_drawCenterPrintAlign, 1);
    layout->y = CG_TuneInt(frag ? &cg_drawFragMessageY : &cg_drawCenterPrintY, cg.centerPrintY);
    layout->scale = CG_TuneValue(frag ? &cg_drawFragMessageScale : &cg_drawCenterPrintScale, cg.centerPrintScale);
    layout->font = frag ? CG_TuneFont(&cg_drawFragMessageFont, &fragMessageFont, 0)
                        : CG_TuneFont(&cg_drawCenterPrintFont, &centerPrintFont, 0);
    layout->style = CG_TuneInt(frag ? &cg_drawFragMessageStyle : &cg_drawCenterPrintStyle, ITEM_TEXTSTYLE_SHADOWEDMORE);
    layout->lineSpacing = CG_TuneValue(frag ? &cg_drawFragMessageLineSpacing : &cg_drawCenterPrintLineSpacing, 6.0f);
    layout->color[0] = layout->color[1] = layout->color[2] = layout->color[3] = 1.0f;
    if (frag) {
        CG_TuneColor(&cg_drawFragMessageColor, &cg_drawFragMessageAlpha, layout->color);
    } else {
        CG_TuneColor(&cg_drawCenterPrintColor, &cg_drawCenterPrintAlpha, layout->color);
    }
    // [CPMA] with ch_animateScroll the frag message is CPMA's FragMessage: 150 + 1200 + 200
    layout->time = CG_TuneInt(frag ? &cg_drawFragMessageTime : &cg_drawCenterPrintTime,
                              (frag && ch_animateScroll.integer) ? 1550 : (int)(cg_centertime.value * 1000.0f));
    layout->fadeTime = CG_TuneInt(frag ? &cg_drawFragMessageFadeTime : &cg_drawCenterPrintFadeTime, 200);
    layout->widescreen = CG_TuneInt(frag ? &cg_drawFragMessageWideScreen : &cg_drawCenterPrintWideScreen, cg_currentWidescreen);
}

/*
===============================================================================

FRAG MESSAGE TOKENS

[wolfcam] cg_drawFragMessageTokens and its team, thaw and freeze variants write the frag
message. Empty, the message is retail's. The tokens this game can fill:
  %v %V  victim name, and without colours       %k %K  your name, and without colours
  %w     weapon name                            %i     weapon icon
  %p     your place ("2nd"), %s your score      %c0xRRGGBB  colour
  %t %T  victim's and your team colour          %%     a percent sign
  %{newline [gap]} %{x n} %{y n} %{font [size] name} %{scale n} %{iconscale n} %{style n}
Font, scale, icon scale, style and colour go back to the message's at every new line.
===============================================================================
*/

typedef enum {
    FRAG_TEXT,       // i: offset of a NUL-terminated run in fragMessage.text
    FRAG_ICON,       // i: shader
    FRAG_NEWLINE,    // f: gap below the line, < 0 for the layout's
    FRAG_XOFFSET,    // f
    FRAG_YOFFSET,    // f
    FRAG_FONT,       // i: engine font
    FRAG_SCALE,      // f: times the message's scale
    FRAG_ICONSCALE,  // f
    FRAG_STYLE,      // i
    FRAG_COLOR       // i: 0xRRGGBB
} fragItemType_t;

typedef struct {
    fragItemType_t type;
    int i;
    float f;
} fragItem_t;

#define MAX_FRAG_ITEMS 128
#define MAX_FRAG_TEXT 1024

static struct {
    int numItems;  // 0: the message is retail's text
    fragItem_t items[MAX_FRAG_ITEMS];
    int textLength;
    char text[MAX_FRAG_TEXT];
} fragMessage;

static fragItem_t* CG_FragItem(fragItemType_t type) {
    fragItem_t* item;

    if (fragMessage.numItems >= MAX_FRAG_ITEMS) {
        return NULL;
    }
    item = &fragMessage.items[fragMessage.numItems++];
    item->type = type;
    item->i = 0;
    item->f = 0.0f;
    return item;
}

static void CG_FragText(const char* s, int length) {
    fragItem_t* item;

    if (length <= 0 || fragMessage.textLength + length + 1 > MAX_FRAG_TEXT) {
        return;
    }
    item = CG_FragItem(FRAG_TEXT);
    if (!item) {
        return;
    }
    item->i = fragMessage.textLength;
    memcpy(fragMessage.text + fragMessage.textLength, s, length);
    fragMessage.textLength += length;
    fragMessage.text[fragMessage.textLength++] = '\0';
}

static void CG_FragTeamColor(int team) {
    fragItem_t* item = CG_FragItem(FRAG_COLOR);

    if (item) {
        item->i = (team == TEAM_RED) ? 0xff8080 : (team == TEAM_BLUE) ? 0x80bfff : 0xffffff;
    }
}

// the number after a long token's keyword
static float CG_FragLongValue(const char* s, float def) {
    while (*s == ' ') {
        s++;
    }
    return (*s && *s != '}') ? (float)atof(s) : def;
}

static void CG_FragLongToken(const char* s) {
    fragItem_t* item;
    char name[MAX_QPATH];
    int n;

    if (!Q_stricmpn(s, "newline", 7)) {
        if ((item = CG_FragItem(FRAG_NEWLINE)) != NULL) {
            item->f = CG_FragLongValue(s + 7, -1.0f);
        }
    } else if (!Q_stricmpn(s, "x ", 2)) {
        if ((item = CG_FragItem(FRAG_XOFFSET)) != NULL) {
            item->f = CG_FragLongValue(s + 2, 0.0f);
        }
    } else if (!Q_stricmpn(s, "y ", 2)) {
        if ((item = CG_FragItem(FRAG_YOFFSET)) != NULL) {
            item->f = CG_FragLongValue(s + 2, 0.0f);
        }
    } else if (!Q_stricmpn(s, "scale ", 6)) {
        if ((item = CG_FragItem(FRAG_SCALE)) != NULL) {
            item->f = CG_FragLongValue(s + 6, 1.0f);
        }
    } else if (!Q_stricmpn(s, "iconscale ", 10)) {
        if ((item = CG_FragItem(FRAG_ICONSCALE)) != NULL) {
            item->f = CG_FragLongValue(s + 10, 1.0f);
        }
    } else if (!Q_stricmpn(s, "style ", 6)) {
        if ((item = CG_FragItem(FRAG_STYLE)) != NULL) {
            item->i = (int)CG_FragLongValue(s + 6, 0.0f);
        }
    } else if (!Q_stricmpn(s, "font ", 5)) {
        // wolfcamql's form carries a point size first; the glyph atlas has no use for it
        s += 5;
        while (*s == ' ' || (*s >= '0' && *s <= '9')) {
            s++;
        }
        for (n = 0; s[n] && s[n] != '}' && s[n] != ' ' && n < sizeof(name) - 1; n++) {
            name[n] = s[n];
        }
        name[n] = '\0';
        if ((item = CG_FragItem(FRAG_FONT)) != NULL) {
            item->i = trap_R_Font_Register(name);
            if (item->i < 0) {
                fragMessage.numItems--;
            }
        }
    } else {
        CG_Printf("frag message: unknown token %%{%s\n", s);
    }
}

// the weapon a means of death was dealt with, WP_NONE for the rest
static int CG_ModWeapon(int mod) {
    switch (mod) {
        case MOD_SHOTGUN:               return WP_SHOTGUN;
        case MOD_GAUNTLET:              return WP_GAUNTLET;
        case MOD_MACHINEGUN:            return WP_MACHINEGUN;
        case MOD_GRENADE:
        case MOD_GRENADE_SPLASH:        return WP_GRENADE_LAUNCHER;
        case MOD_ROCKET:
        case MOD_ROCKET_SPLASH:         return WP_ROCKET_LAUNCHER;
        case MOD_PLASMA:
        case MOD_PLASMA_SPLASH:         return WP_PLASMAGUN;
        case MOD_RAILGUN:
        case MOD_RAILGUN_HEADSHOT:      return WP_RAILGUN;
        case MOD_LIGHTNING:
        case MOD_LIGHTNING_DISCHARGE:   return WP_LIGHTNING;
        case MOD_BFG:
        case MOD_BFG_SPLASH:            return WP_BFG;
        case MOD_NAIL:                  return WP_NAILGUN;
        case MOD_CHAINGUN:              return WP_CHAINGUN;
        case MOD_PROXIMITY_MINE:        return WP_PROX_LAUNCHER;
        case MOD_HMG:                   return WP_HMG;
        default:                        return WP_NONE;
    }
}

/*
===================
CG_BuildFragMessage

Called right after the frag message went to CG_CenterPrint. victimName is the name the retail
message uses; target is -1 for a message with no victim behind it (testfragmessage).
===================
*/
void CG_BuildFragMessage(int target, int mod, const char* victimName, qboolean sameTeam) {
    const char* tokens;
    const char* p;
    const char* end;
    char plain[MAX_QPATH];
    int weapon = CG_ModWeapon(mod);
    const clientInfo_t* you = &cgs.clientinfo[cg.snap->ps.clientNum];
    fragItem_t* item;
    int value, length;

    // a token cvar set in this very frame has not reached its vmCvar yet
    CG_UpdateTuneCvars();

    if (cgs.gametype == GT_FREEZE) {
        tokens = (mod == MOD_THAW) ? cg_drawFragMessageThawTokens.string
                 : sameTeam        ? cg_drawFragMessageFreezeTeamTokens.string
                                   : cg_drawFragMessageFreezeTokens.string;
    } else {
        tokens = (sameTeam && cgs.gametype != GT_RR && cgs.gametype != GT_RACE) ? cg_drawFragMessageTeamTokens.string
                                                                                : cg_drawFragMessageTokens.string;
    }

    fragMessage.numItems = 0;
    fragMessage.textLength = 0;

    for (p = tokens; *p;) {
        if (*p != '%') {
            for (end = p; *end && *end != '%'; end++) {
            }
            CG_FragText(p, (int)(end - p));
            p = end;
            continue;
        }
        switch (p[1]) {
            case 'v':
                CG_FragText(victimName, strlen(victimName));
                break;
            case 'V':
                Q_strncpyz(plain, victimName, sizeof(plain));
                Q_CleanStr(plain);
                CG_FragText(plain, strlen(plain));
                break;
            case 'k':
                CG_FragText(va("%s" S_COLOR_WHITE, you->name), strlen(you->name) + 2);
                break;
            case 'K':
                Q_strncpyz(plain, you->name, sizeof(plain));
                Q_CleanStr(plain);
                CG_FragText(plain, strlen(plain));
                break;
            case 'w':
                if (weapon != WP_NONE && cg_weapons[weapon].item) {
                    CG_FragText(cg_weapons[weapon].item->pickup_name, strlen(cg_weapons[weapon].item->pickup_name));
                }
                break;
            case 'i':
                if ((item = CG_FragItem(FRAG_ICON)) != NULL) {
                    item->i = (weapon != WP_NONE) ? cg_weapons[weapon].weaponIcon : cgs.media.fragIconShader;
                }
                break;
            case 'p': {
                const char* place = CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1);
                CG_FragText(place, strlen(place));
                break;
            }
            case 's': {
                const char* score = va("%i", cg.snap->ps.persistant[PERS_SCORE]);
                CG_FragText(score, strlen(score));
                break;
            }
            case 'c':
                length = 0;
                if (p[2] == '0' && p[3] == 'x') {
                    for (length = 2; length < 8 && isxdigit((unsigned char)p[2 + length]); length++) {
                    }
                    value = Com_HexStrToInt(va("%.*s", length, p + 2));
                    if (value >= 0 && (item = CG_FragItem(FRAG_COLOR)) != NULL) {
                        item->i = value;
                    }
                }
                p += length;  // the "0xRRGGBB"
                break;
            case 't':
                CG_FragTeamColor(target >= 0 ? cgs.clientinfo[target].team : TEAM_FREE);
                break;
            case 'T':
                CG_FragTeamColor(you->team);
                break;
            case '%':
                CG_FragText("%", 1);
                break;
            case '{':
                CG_FragLongToken(p + 2);
                for (end = p + 2; *end && *end != '}'; end++) {
                }
                p = *end ? end - 1 : end - 2;
                break;
            case '\0':
                p--;
                break;
            default:
                CG_Printf("frag message: unknown token %%%c\n", p[1]);
                break;
        }
        p += 2;
    }
}

/*
===================
CG_DrawFragMessageTokens

Paints the frag message written by tokens, laid out as CG_DrawCenterString lays out lines:
the block starts scale * 16 * lines above y and each line sits on its own tallest letters.
False when the message is retail's text.
===================
*/
qboolean CG_DrawFragMessageTokens(const centerPrintLayout_t* layout, const vec4_t fade) {
    const fragItem_t* line = fragMessage.items;
    const fragItem_t* stop = fragMessage.items + fragMessage.numItems;
    const fragItem_t* item;
    float iconScale = CG_TuneValue(&cg_drawFragMessageIconScale, 1.0f);
    float y, x, baseline, gap, height, width, w, h, scale, icons, dy;
    int lines, font, style;
    vec4_t color;

    if (!fragMessage.numItems) {
        return qfalse;
    }

    for (lines = 1, item = line; item < stop; item++) {
        if (item->type == FRAG_NEWLINE) {
            lines++;
        }
    }
    y = (float)(int)((float)layout->y - layout->scale * 16.0f * lines);

    CG_SetWidescreen(layout->widescreen);
    while (line < stop) {
        // measure the line in the message's font, scale, style and icon scale
        font = layout->font;
        scale = layout->scale;
        icons = iconScale;
        height = (float)CG_DrawTextHeightInFont(font, scale);
        width = 0.0f;
        gap = -1.0f;
        for (item = line; item < stop && item->type != FRAG_NEWLINE; item++) {
            switch (item->type) {
                case FRAG_TEXT:
                    width += CG_TextAdvanceInFont(fragMessage.text + item->i, font, scale);
                    height = MAX(height, CG_DrawTextHeightInFont(font, scale));
                    break;
                case FRAG_ICON:
                    width += CG_DrawTextHeightInFont(font, scale) * icons;
                    break;
                case FRAG_FONT:
                    font = item->i;
                    break;
                case FRAG_SCALE:
                    scale = layout->scale * item->f;
                    break;
                case FRAG_ICONSCALE:
                    icons = iconScale * item->f;
                    break;
                default:
                    break;
            }
        }
        if (item < stop) {
            gap = item->f;
        }

        x = layout->x;
        if (layout->align == 1) {
            x = (float)(((int)(layout->x * 2.0f) - (int)width) / 2);
        } else if (layout->align == 2) {
            x -= width;
        }
        baseline = y + height;
        dy = 0.0f;

        font = layout->font;
        scale = layout->scale;
        icons = iconScale;
        style = layout->style;
        Vector4Copy(fade, color);
        for (item = line; item < stop && item->type != FRAG_NEWLINE; item++) {
            switch (item->type) {
                case FRAG_TEXT:
                    CG_DrawTextInFont(x, baseline + dy, font, scale, color, fragMessage.text + item->i, style);
                    x += CG_TextAdvanceInFont(fragMessage.text + item->i, font, scale);
                    break;
                case FRAG_ICON:
                    h = CG_DrawTextHeightInFont(font, scale);
                    w = h * icons;
                    color[0] = color[1] = color[2] = 1.0f;
                    trap_R_SetColor(color);
                    CG_DrawPic(x, baseline + dy - h / 2.0f - w / 2.0f, w, w, (qhandle_t)item->i);
                    trap_R_SetColor(NULL);
                    Vector4Copy(fade, color);
                    x += w;
                    break;
                case FRAG_XOFFSET:
                    x += item->f;
                    break;
                case FRAG_YOFFSET:
                    dy += item->f;
                    break;
                case FRAG_FONT:
                    font = item->i;
                    break;
                case FRAG_SCALE:
                    scale = layout->scale * item->f;
                    break;
                case FRAG_ICONSCALE:
                    icons = iconScale * item->f;
                    break;
                case FRAG_STYLE:
                    style = item->i;
                    break;
                case FRAG_COLOR:
                    color[0] = ((item->i >> 16) & 0xff) / 255.0f;
                    color[1] = ((item->i >> 8) & 0xff) / 255.0f;
                    color[2] = (item->i & 0xff) / 255.0f;
                    break;
                default:
                    break;
            }
        }

        y += height + (gap >= 0.0f ? gap : layout->lineSpacing);
        line = (item < stop) ? item + 1 : stop;
    }
    CG_SetWidescreen(WIDESCREEN_CENTER);
    return qtrue;
}

// testfragmessage: a frag message as if you had just fragged someone with a rocket
void CG_TestFragMessage_f(void) {
    if (cg.snap) {
        CG_ShowFragMessage(-1, MOD_ROCKET, "Victim" S_COLOR_WHITE);
    }
}

// testrewards [count]: an excellent medal, count times over
void CG_TestRewards_f(void) {
    int count = (trap_Argc() > 1) ? atoi(CG_Argv(1)) : 1;

    if (cg.snap) {
        CG_PushReward(0, cgs.media.medalExcellent, (count > 0) ? count : 1);
    }
}
