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
// ui_startmatch.c -- [QL] the Start Match page
//
// Retail has no native menu to start a local match: its web UI does it, with
// web.pak's Start Match page, which reads the engine's factories and arenas
// through GetFactoryList and GetMapList (quakelive_steam.exe 0x432edc and
// 0x432c2c) and launches with "map <map> <factory>". This draws that page with
// the renderer and runs it the way the page's code does, quirks included.
//
// Sizes are the page's CSS pixels at the 1920x1080 it was laid out for, and the
// colours are its stylesheet's. Its own art is in web.pak, which is not shipped,
// so pak00's gametype icons and fonts stand in.
//

// The frame, the palette and the drawing are shared with the other pages.
#include "ui_web.h"

#define SM_MAX_FACTORIES 1024  // MAX_FACTORIES in the engine
#define SM_MAX_ARENAS 1024     // MAX_ARENA_INFOS
#define SM_MAX_INPUT 200       // the Cvar widget's maxLength


/*
===============================================================================

WHAT THE PAGE OFFERS

===============================================================================
*/

typedef struct {
    int gametype;
    const char* title;
    const char* factory;  // the one the page picks with the gametype
    const char* map;      // the one it falls back on
    const char* name;     // module 237's, which names its icons in web.pak's img/gametype
} smGametype_t;

// module 237's gametypes, in the order the page lists them (it has no Overload)
static const smGametype_t smGametypes[] = {
    {GT_FFA, "Free For All", "ffa", "campgrounds", "ffa"},
    {GT_DUEL, "Duel", "duel", "bloodrun", "duel"},
    {GT_RACE, "Race", "race", "foolishlegacy", "race"},
    {GT_TEAM, "Team Deathmatch", "tdm", "campgrounds", "tdm"},
    {GT_CA, "Clan Arena", "ca", "repent", "ca"},
    {GT_CTF, "Capture The Flag", "ctf", "spidercrossings", "ctf"},
    {GT_1FCTF, "1-Flag CTF", "oneflag", "spidercrossings", "1fctf"},
    {GT_HARVESTER, "Harvester", "har", "spidercrossings", "harvester"},
    {GT_FREEZE, "Freeze Tag", "ft", "coldwar", "ft"},
    {GT_DOMINATION, "Domination", "dom", "chemicalreaction", "dom"},
    {GT_AD, "Attack & Defend", "ad", "stonekeep", "ad"},
    {GT_RR, "Red Rover", "rr", "devilish", "rr"},
};

#define SM_NUM_GAMETYPES ((int)ARRAY_LEN(smGametypes))

// getById for anything else: "Unknown Gametype", with no factory or map of its own
static const smGametype_t smUnknownGametype = {-1, "Unknown Gametype", NULL, NULL, "unknown"};

typedef struct {
    const char* label;
    const char* value;
} smOption_t;

// module 230
static const smOption_t smServerTypes[] = {{"Offline", "0"}, {"LAN", "1"}, {"Internet", "2"}};
static const smOption_t smSkills[] = {
    {"I Can Win", "0"}, {"Bring it On", "1"}, {"Hurt Me Plenty", "2"}, {"Hardcore", "3"}, {"Nightmare", "4"}};
static const smOption_t smMapPools[] = {
    {"Mixed", "mappool.txt"}, {"Clan Arena", "mappool_ca.txt"}, {"Capture the Flag", "mappool_ctf.txt"},
    {"Duel", "mappool_duel.txt"}, {"Free For All", "mappool_ffa.txt"}, {"Race", "mappool_race.txt"},
    {"Team Deathmatch", "mappool_tdm.txt"}};

typedef enum { SM_INPUT, SM_SELECT, SM_RANGE } smWidget_t;

typedef struct {
    const char* cvar;   // as the page names it, in lower case
    const char* title;  // module 229
    smWidget_t widget;
    float min, max, step;
    int decimals;       // the step's
    const smOption_t* options;
    int numOptions;
} smCvar_t;

enum {
    SM_HOSTNAME,
    SM_SERVERTYPE,
    SM_MAXCLIENTS,
    SM_MINPLAYERS,
    SM_SKILL,
    SM_TEAMSIZE,
    SM_PASSWORD,
    SM_READY,
    SM_MAPPOOL,
    SM_NUM_CVARS
};

// module 263's list, in its order
static const smCvar_t smCvars[SM_NUM_CVARS] = {
    {"sv_hostname", "Match Name", SM_INPUT, 0, 0, 0, 0, NULL, 0},
    {"sv_servertype", "Match Type", SM_SELECT, 0, 0, 0, 0, smServerTypes, (int)ARRAY_LEN(smServerTypes)},
    {"sv_maxclients", "Maximum Players", SM_RANGE, 1, 24, 1, 0, NULL, 0},
    {"bot_minplayers", "Minimum Players", SM_RANGE, 0, 8, 1, 0, NULL, 0},
    {"g_spskill", "Bot Skill", SM_SELECT, 0, 0, 0, 0, smSkills, (int)ARRAY_LEN(smSkills)},
    {"teamsize", "Team Size", SM_RANGE, 0, 20, 1, 0, NULL, 0},
    {"g_password", "Match Password", SM_INPUT, 0, 0, 0, 0, NULL, 0},
    {"sv_warmupreadypercentage", "Warmup Ready Percentage", SM_RANGE, 0, 1, 0.01f, 2, NULL, 0},
    {"sv_mappoolfile", "Map Pool", SM_SELECT, 0, 0, 0, 0, smMapPools, (int)ARRAY_LEN(smMapPools)},
};

/*
===============================================================================

STATE

===============================================================================
*/

typedef struct {
    char id[64];
    char title[64];
    char author[64];
    char description[256];
    int basegt;
    int slot;  // the engine's, to read its cvars
} smFactory_t;

typedef struct {
    char sysname[64];
    char name[64];
    int gametypes;  // a bit per base gametype
    qhandle_t preview;
    qboolean previewTried;
    float nameWidth;  // in bold, at nameScale
    float nameScale;
} smArena_t;

static smFactory_t smFactories[SM_MAX_FACTORIES];  // by id, as the page's factories object
static int smNumFactories;
static smArena_t smArenas[SM_MAX_ARENAS];          // by sysname, as its mapdb object
static int smNumArenas;
static int smMapList[SM_MAX_ARENAS];               // the maps dialog's, by name
static int smNumMapList;
static qhandle_t smIcons[SM_NUM_GAMETYPES + 1][2];  // small and large, the unknown one last
static qhandle_t smDefaultPreview;

typedef enum { SM_DIALOG_NONE, SM_DIALOG_FACTORIES, SM_DIALOG_MAPS } smDialog_t;

static struct {
    qboolean loaded;

    // module 262's state, which it keeps in sessionStorage between visits
    int selectedGametype;
    char selectedFactory[64];
    char selectedMap[64];
    char tempFactory[64];
    char tempMap[64];
    char filter;  // mapStartsWithFilter: 0 for "all"

    // module 263's: the cvars as it last heard of them
    char values[SM_NUM_CVARS][MAX_CVAR_VALUE_STRING];
    char heard[SM_NUM_CVARS][MAX_CVAR_VALUE_STRING];
    char netPort[16];
    qboolean locked[SM_NUM_CVARS];
    char lockedValue[SM_NUM_CVARS][MAX_CVAR_VALUE_STRING];
    char locksFor[64];
    qboolean locksValid;

    // on screen
    smDialog_t dialog;
    float factoryScroll, mapScroll, optionScroll;
    int edit, editCursor, editStart, editTime;
    int select, selectFocus;
    smRect_t selectControl, selectMenu;
    int drag;
    float dragValue;
    smRect_t dragRect;
    int held;  // a slider let go off itself keeps showing where it was left
    char heldValue[32];
    qboolean scrollDrag;
    float scrollGrab;
    smRect_t article;
    float articleContent;
    int mapListGametype;
    int mapListFilter;
} sm;

/*
===============================================================================

WHAT THE CURSOR IS OVER

The page's own list of things the mouse can land on; the drawing and the hit
list itself are in ui_web.c.

===============================================================================
*/

typedef enum {
    SM_HIT_BLOCK,  // takes the mouse off what is under it
    SM_HIT_GAMETYPE_CARD,
    SM_HIT_ARENA_CARD,
    SM_HIT_RESET,
    SM_HIT_INPUT,
    SM_HIT_SELECT,
    SM_HIT_MENU,
    SM_HIT_OPTION,
    SM_HIT_RANGE,
    SM_HIT_LAUNCH,
    SM_HIT_GAMETYPE,
    SM_HIT_FACTORY,
    SM_HIT_FILTER,
    SM_HIT_MAP,
    SM_HIT_CANCEL,
    SM_HIT_OK,
    SM_HIT_TRACK,
    SM_HIT_THUMB
} smHitType_t;


/*
===============================================================================

THE ENGINE'S FACTORIES AND ARENAS

===============================================================================
*/

static void SM_LoadFactories(void) {
    char id[64], basegt[16];
    int count = trap_Factory_Count();
    int i, pos;
    smFactory_t* f;

    smNumFactories = 0;
    for (i = 0; i < count; i++) {
        // GetFactoryList leaves out the hidden ones (0x432f32)
        if (!trap_Factory_Value(i, "id", id, sizeof(id)) || id[0] == '_') {
            continue;
        }
        // and keys the rest by id: sorted, a repeated id keeping the later one
        for (pos = 0; pos < smNumFactories && strcmp(smFactories[pos].id, id) < 0; pos++) {
        }
        if (pos == smNumFactories || strcmp(smFactories[pos].id, id)) {
            if (smNumFactories == SM_MAX_FACTORIES) {
                continue;
            }
            memmove(&smFactories[pos + 1], &smFactories[pos], (smNumFactories - pos) * sizeof(smFactories[0]));
            smNumFactories++;
        }
        f = &smFactories[pos];
        memset(f, 0, sizeof(*f));
        Q_strncpyz(f->id, id, sizeof(f->id));
        trap_Factory_Value(i, "title", f->title, sizeof(f->title));
        trap_Factory_Value(i, "author", f->author, sizeof(f->author));
        trap_Factory_Value(i, "description", f->description, sizeof(f->description));
        trap_Factory_Value(i, "basegt", basegt, sizeof(basegt));
        f->basegt = atoi(basegt);
        f->slot = i;
    }
}

static void SM_LoadArenas(void) {
    char sysname[64], bits[16];
    int count = trap_Arena_Count();
    int i, pos;
    smArena_t* a;

    smNumArenas = 0;
    for (i = 0; i < count; i++) {
        if (!trap_Arena_Value(i, "sysname", sysname, sizeof(sysname)) || !sysname[0]) {
            continue;
        }
        // GetMapList keys them by map: sorted, a repeated map keeping the later one
        for (pos = 0; pos < smNumArenas && strcmp(smArenas[pos].sysname, sysname) < 0; pos++) {
        }
        if (pos == smNumArenas || strcmp(smArenas[pos].sysname, sysname)) {
            if (smNumArenas == SM_MAX_ARENAS) {
                continue;
            }
            memmove(&smArenas[pos + 1], &smArenas[pos], (smNumArenas - pos) * sizeof(smArenas[0]));
            smNumArenas++;
        }
        a = &smArenas[pos];
        memset(a, 0, sizeof(*a));
        Q_strncpyz(a->sysname, sysname, sizeof(a->sysname));
        trap_Arena_Value(i, "name", a->name, sizeof(a->name));
        trap_Arena_Value(i, "gametypes", bits, sizeof(bits));
        a->gametypes = atoi(bits);
    }
}

static smFactory_t* SM_Factory(const char* id) {
    int lo = 0, hi = smNumFactories - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(smFactories[mid].id, id);

        if (!c) {
            return &smFactories[mid];
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

static smArena_t* SM_Arena(const char* sysname) {
    int lo = 0, hi = smNumArenas - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(smArenas[mid].sysname, sysname);

        if (!c) {
            return &smArenas[mid];
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

static const smGametype_t* SM_Gametype(int gametype) {
    int i;

    for (i = 0; i < SM_NUM_GAMETYPES; i++) {
        if (smGametypes[i].gametype == gametype) {
            return &smGametypes[i];
        }
    }
    return &smUnknownGametype;
}

// mapdb[map].gametypes[gametype]
static qboolean SM_Supports(const char* map, int gametype) {
    const smArena_t* arena = SM_Arena(map);

    return arena && gametype >= 0 && gametype < 31 && (arena->gametypes & (1 << gametype));
}

// img/gametype/sm (28 square) for the dialog's list, xl (75 square) for the card
static qhandle_t SM_GametypeIcon(const smGametype_t* gt, qboolean large) {
    int i = (int)(gt - smGametypes);

    if (i < 0 || i >= SM_NUM_GAMETYPES) {
        i = SM_NUM_GAMETYPES;
    }
    if (!smIcons[i][large]) {
        smIcons[i][large] = trap_R_RegisterShaderNoMip(va("webui/img/gametype/%s/%s.png", large ? "xl" : "sm", gt->name));
    }
    return smIcons[i][large];
}

// url(sm/<map>.jpg) over url(sm/default.jpg): the default shows where a map has none
static qhandle_t SM_Preview(smArena_t* arena, const char* map) {
    qhandle_t shot;

    if (!smDefaultPreview) {
        smDefaultPreview = trap_R_RegisterShaderNoMip("webui/img/levelshots/sm/default.jpg");
    }
    if (arena && arena->previewTried) {
        return arena->preview ? arena->preview : smDefaultPreview;
    }
    shot = trap_R_RegisterShaderNoMip(va("webui/img/levelshots/sm/%s.jpg", map));
    if (arena) {
        arena->preview = shot;
        arena->previewTried = qtrue;
    }
    return shot ? shot : smDefaultPreview;
}

// url(lg/<map>.jpg), with no fallback
static qhandle_t SM_Levelshot(const char* map) {
    static char last[64];
    static qhandle_t shot;
    static qboolean tried;

    if (!tried || strcmp(last, map)) {
        Q_strncpyz(last, map, sizeof(last));
        shot = trap_R_RegisterShaderNoMip(va("webui/img/levelshots/lg/%s.jpg", map));
        tried = qtrue;
    }
    return shot;
}

static float SM_NameWidth(smArena_t* arena) {
    if (arena->nameScale != smScale) {
        arena->nameWidth = SM_AdvanceBold(FONT_SANS, 24, arena->name, -1, qtrue);
        arena->nameScale = smScale;
    }
    return arena->nameWidth;
}

/*
===============================================================================

THE PAGE'S STATE (module 262)

===============================================================================
*/

static void SM_Defaults(void) {
    // getInitialState
    sm.selectedGametype = GT_FFA;
    Q_strncpyz(sm.selectedFactory, "ffa", sizeof(sm.selectedFactory));
    Q_strncpyz(sm.selectedMap, "campgrounds", sizeof(sm.selectedMap));
    Q_strncpyz(sm.tempFactory, "ffa", sizeof(sm.tempFactory));
    Q_strncpyz(sm.tempMap, "campgrounds", sizeof(sm.tempMap));
    sm.filter = 0;
    sm.mapListGametype = -1;
}

// selectGametype: the gametype takes effect at once; the dialog's factory and map follow it
static void SM_SelectGametype(int gametype) {
    const smGametype_t* gt = SM_Gametype(gametype);

    if (!SM_Supports(sm.tempMap, gametype) && gt->map) {
        Q_strncpyz(sm.tempMap, gt->map, sizeof(sm.tempMap));
    }
    if (gt->factory) {
        Q_strncpyz(sm.tempFactory, gt->factory, sizeof(sm.tempFactory));
    }
    sm.selectedGametype = gametype;
}

static void SM_SelectFactory(const char* id) {
    const smFactory_t* f = SM_Factory(id);

    Q_strncpyz(sm.tempFactory, id, sizeof(sm.tempFactory));
    if (f) {
        const smGametype_t* gt = SM_Gametype(f->basegt);

        if (!SM_Supports(sm.tempMap, f->basegt) && gt->map) {
            Q_strncpyz(sm.tempMap, gt->map, sizeof(sm.tempMap));
        }
    }
}

// confirmFactoryDialog: the map is checked against the selected one, not the dialog's
static void SM_ConfirmFactories(void) {
    const smFactory_t* f;

    Q_strncpyz(sm.selectedFactory, sm.tempFactory, sizeof(sm.selectedFactory));
    f = SM_Factory(sm.selectedFactory);
    if (f) {
        const smGametype_t* gt = SM_Gametype(f->basegt);

        if (!SM_Supports(sm.selectedMap, f->basegt) && gt->map) {
            Q_strncpyz(sm.selectedMap, gt->map, sizeof(sm.selectedMap));
        }
    }
    sm.dialog = SM_DIALOG_NONE;
}

static void SM_CancelFactories(void) {
    Q_strncpyz(sm.tempFactory, sm.selectedFactory, sizeof(sm.tempFactory));
    Q_strncpyz(sm.tempMap, sm.selectedMap, sizeof(sm.tempMap));
    sm.dialog = SM_DIALOG_NONE;
}

static void SM_ConfirmMaps(void) {
    Q_strncpyz(sm.selectedMap, sm.tempMap, sizeof(sm.selectedMap));
    sm.dialog = SM_DIALOG_NONE;
}

static void SM_CancelMaps(void) {
    Q_strncpyz(sm.tempMap, sm.selectedMap, sizeof(sm.tempMap));
    sm.dialog = SM_DIALOG_NONE;
}

// launchMatch: SendGameCommand("map <map> <factory>")
static void SM_Launch(void) {
    trap_Cmd_ExecuteText(EXEC_APPEND, va("map %s %s\n", sm.selectedMap, sm.selectedFactory));
}

// the letters menu: ALL, A to Z and #
static char SM_FilterChar(int k) {
    if (k <= 0) {
        return 0;
    }
    return k == 27 ? '#' : (char)('A' + k - 1);
}

static char SM_Upper(char c) {
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

/*
===============
SM_BuildMapList

The maps of the selected gametype whose name starts with the letter, sorted by
name (lodash's sortBy keeps the mapdb order among equals). "#" means to take
names that start with anything but a letter, but the page's test for it can
never be true: it matches only a name that starts with "#".
===============
*/
static void SM_BuildMapList(void) {
    int i, j;

    if (sm.mapListGametype == sm.selectedGametype && sm.mapListFilter == sm.filter) {
        return;
    }
    sm.mapListGametype = sm.selectedGametype;
    sm.mapListFilter = sm.filter;
    smNumMapList = 0;
    for (i = 0; i < smNumArenas; i++) {
        const smArena_t* a = &smArenas[i];

        if (sm.selectedGametype < 0 || sm.selectedGametype >= 31 || !(a->gametypes & (1 << sm.selectedGametype))) {
            continue;
        }
        if (sm.filter && SM_Upper(a->name[0]) != sm.filter) {
            continue;
        }
        // insertion keeps equal names in mapdb order
        for (j = smNumMapList; j > 0 && strcmp(smArenas[smMapList[j - 1]].name, a->name) > 0; j--) {
            smMapList[j] = smMapList[j - 1];
        }
        smMapList[j] = i;
        smNumMapList++;
    }
}

// the factories dialog's list: the gametype's in id order, its default one first
static int SM_FactoryList(int* list) {
    const smGametype_t* gt = SM_Gametype(sm.selectedGametype);
    const smFactory_t* def = gt->factory ? SM_Factory(gt->factory) : NULL;
    int n = 0, i;

    if (def) {
        list[n++] = (int)(def - smFactories);
    }
    for (i = 0; i < smNumFactories; i++) {
        if (smFactories[i].basegt != sm.selectedGametype) {
            continue;
        }
        if (gt->factory && !strcmp(smFactories[i].id, gt->factory)) {
            continue;
        }
        list[n++] = i;
    }
    return n;
}

/*
===============================================================================

THE OPTIONS (module 263)

===============================================================================
*/

// JavaScript's value == n for a string: its number, "" being 0
static qboolean SM_Equals(const char* value, float n) {
    char* end;
    double d;

    while (*value == ' ' || *value == '\t') {
        value++;
    }
    if (!*value) {
        return n == 0;
    }
    d = strtod(value, &end);
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    return *end == '\0' && d == n;
}

// the value a row shows: a factory's own for a cvar it sets
static const char* SM_Value(int i) {
    return sm.locked[i] ? sm.lockedValue[i] : sm.values[i];
}

// cvarChanged: the match type moves the port, and a map pool reloads
static void SM_Heard(int i) {
    if (i == SM_SERVERTYPE) {
        trap_Cvar_Set("net_port", SM_Equals(sm.values[i], 1) ? "27015" : "27960");
        trap_Cmd_ExecuteText(EXEC_APPEND, "net_restart\n");
    } else if (i == SM_MAPPOOL) {
        trap_Cmd_ExecuteText(EXEC_APPEND, "reload_mappool\n");
    }
}

/*
===============
SM_ReadCvars

The page learns a cvar's value from GetConfig when it opens and from the
engine's event when it changes; both give the latched value while one waits.
A change is an event: it replaces what the page shows, but for a text input
being typed in.
===============
*/
static void SM_ReadCvars(qboolean events) {
    char value[MAX_CVAR_VALUE_STRING];
    int i;

    for (i = 0; i < SM_NUM_CVARS; i++) {
        trap_Cvar_LatchedVariableStringBuffer(smCvars[i].cvar, value, sizeof(value));
        if (events && !strcmp(value, sm.heard[i])) {
            continue;
        }
        Q_strncpyz(sm.heard[i], value, sizeof(sm.heard[i]));
        if (i != sm.edit) {
            Q_strncpyz(sm.values[i], value, sizeof(sm.values[i]));
        }
        if (events) {
            sm.held = -1;
            SM_Heard(i);
        }
    }
    trap_Cvar_LatchedVariableStringBuffer("net_port", sm.netPort, sizeof(sm.netPort));
}

// renderCvar: a cvar the selected factory sets is disabled
static void SM_UpdateLocks(void) {
    char name[MAX_CVAR_VALUE_STRING], value[MAX_CVAR_VALUE_STRING];
    const smFactory_t* f;
    int i, j;

    if (sm.locksValid && !strcmp(sm.locksFor, sm.selectedFactory)) {
        return;
    }
    Q_strncpyz(sm.locksFor, sm.selectedFactory, sizeof(sm.locksFor));
    sm.locksValid = qtrue;
    memset(sm.locked, 0, sizeof(sm.locked));
    f = SM_Factory(sm.selectedFactory);
    if (!f) {
        return;
    }
    for (j = 0; trap_Factory_Cvar(f->slot, j, name, sizeof(name), value, sizeof(value)); j++) {
        for (i = 0; i < SM_NUM_CVARS; i++) {
            if (!strcmp(name, smCvars[i].cvar)) {
                sm.locked[i] = qtrue;
                Q_strncpyz(sm.lockedValue[i], value, sizeof(sm.lockedValue[i]));
            }
        }
    }
}

// onBlurInput: what was typed goes to the cvar
static void SM_EndEdit(void) {
    if (sm.edit < 0) {
        return;
    }
    trap_Cvar_Set(smCvars[sm.edit].cvar, sm.values[sm.edit]);
    sm.edit = -1;
}

// the gap between characters nearest to x, in the text shown from start
static int SM_CaretAt(const char* text, int start, float x) {
    int len = (int)strlen(text);
    int n, best = start;
    float bestDist = 1e9f;

    for (n = start; n <= len; n++) {
        float dist = (float)fabs(SM_Advance(FONT_SANS, 15, text + start, n - start) - x);

        if (dist >= bestDist) {
            break;
        }
        bestDist = dist;
        best = n;
    }
    return best;
}

static void SM_OpenSelect(int i) {
    const smCvar_t* cv = &smCvars[i];
    int k;

    sm.select = i;
    sm.selectFocus = 0;
    for (k = 0; k < cv->numOptions; k++) {
        if (!strcmp(cv->options[k].value, sm.values[i])) {
            sm.selectFocus = k;
        }
    }
    // the menu shows its focused option
    sm.optionScroll = sm.selectFocus * 40.0f + 40.0f - 198.0f;
    if (sm.optionScroll < 0) {
        sm.optionScroll = 0;
    }
}

// where a slider's thumb goes for the cursor at x: the step's nearest value
static float SM_RangeAt(int i, float x) {
    const smCvar_t* cv = &smCvars[i];
    float frac = (x - (sm.dragRect.x + 1) - 8) / (sm.dragRect.w - 2 - 16);
    float value;

    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    value = cv->min + frac * (cv->max - cv->min);
    value = cv->min + (float)floor((value - cv->min) / cv->step + 0.5f) * cv->step;
    return value > cv->max ? cv->max : value;
}

static void SM_FormatRange(int i, float value, char* buf, int size) {
    Com_sprintf(buf, size, smCvars[i].decimals ? "%.2f" : "%.0f", value);
}

/*
===============================================================================

THE PAGE

css/main.css's .start-match: a panel at 80% black with the page's h1, two columns of
48% with 4% between them -- the two cards on the left, the cvar list on the right --
and the launch button under them, half the page wide. The page is overflow-y: scroll,
so its bar's gutter is always there.

===============================================================================
*/

#define SM_BAR 8.0f
#define SM_CARD_H 200.0f
#define SM_CARD_FONT 21.3333f  // 16pt
#define SM_LAN_H 39.0f         // p.lan, with its padding

static const vec4_t smrCard = SM_RGB(44, 41, 41);
static const vec4_t smrCardHover = SM_RGB(60, 57, 57);
static const vec4_t smrRowOdd = SM_RGB(36, 33, 33);
static const vec4_t smrRowEven = SM_RGB(28, 25, 25);
static const vec4_t smrYellow = SM_RGB(247, 209, 0);
static const vec4_t smrSelect = SM_RGB(51, 51, 51);
static const vec4_t smrSelectHover = SM_RGB(68, 68, 68);
static const vec4_t smrSelectValue = SM_RGB(204, 204, 204);
static const vec4_t smrPlaceholder = SM_RGB(170, 170, 170);
static const vec4_t smrOption = SM_RGB(187, 187, 187);
static const vec4_t smrOptionFocus = SM_RGB(34, 34, 34);
static const vec4_t smrInput = SM_RGB(15, 15, 15);
static const vec4_t smrInputBorder = SM_RGB(51, 51, 51);
static const vec4_t smrRange = SM_RGB(34, 34, 34);
static const vec4_t smrRangeHover = SM_RGB(68, 68, 68);
static const vec4_t smrLink = SM_RGB(30, 174, 219);
static const vec4_t smrLinkHover = SM_RGB(15, 160, 206);
static const vec4_t smrLaunch = SM_RGB(61, 61, 61);
static const vec4_t smrLaunchHover = SM_RGB(93, 93, 93);
static const vec4_t smrDialog = SM_RGB(30, 30, 30);
static const vec4_t smrBlack80 = {0, 0, 0, 0.8f};
static const vec4_t smrThumb = SM_RGB(51, 51, 51);
static const vec4_t smrThumbHover = SM_RGB(153, 153, 153);

// the layout of the page for this frame: set by SM_DrawPage, read by the rows
static struct {
    float x;         // the content's left, inside the panel's padding
    float width;     // the content's width
    float colW;      // one of the two columns
    float listX;     // the cvar list's left
    float valX;      // a row's control, from the row's left
    float valW;
} smL;

static void SM_Layout(void) {
    float page = UI_Home_Left();

    smL.x = page + 5;
    smL.width = HM_CONTAINER_W - SM_BAR - 10;
    smL.colW = smL.width * 0.48f;
    smL.listX = smL.x + smL.colW + smL.width * 0.04f;
    // children seven columns and val five columns, inside the row's padding
    smL.valX = 7.5f + (smL.colW - 15) * (0.566667f + 0.04f);
    smL.valW = (smL.colW - 15) * 0.393333f;
}

// centred in the card's content
static void SM_CardCentered(float x, float w, float lineTop, float lineHeight, const char* text) {
    SM_Text(x + (w - SM_Advance(FONT_SANS, SM_CARD_FONT, text, -1)) * 0.5f,
            SM_Baseline(FONT_SANS, SM_CARD_FONT, lineTop, lineHeight), FONT_SANS, SM_CARD_FONT, smText, text, -1,
            qfalse);
}

// button.card: #2C2929, #3C3939 under the cursor, with its yellow h2
static void SM_DrawCardBack(float x, float y, float w, const char* title, int hit) {
    SM_Fill(x, y, w, SM_CARD_H, SM_Over(hit, 0) ? smrCardHover : smrCard, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(x + 10.6667f, SM_Baseline(FONT_SANS, 24, y + 12, 30), FONT_SANS, 24, smrYellow, title, -1, qfalse);
    SM_SetSpacing(0);
    SM_Hit(x, y, w, SM_CARD_H, hit, 0, NULL);
}

static void SM_DrawGametypeCard(float x, float y, float w) {
    const smFactory_t* factory = SM_Factory(sm.selectedFactory);
    const smGametype_t* gt = SM_Gametype(factory ? factory->basegt : -1);
    float cx = x + 10.6667f, cw = w - 21.3333f;

    SM_DrawCardBack(x, y, w, "Game Type", SM_HIT_GAMETYPE_CARD);
    SM_Pic(cx + (cw - 75) * 0.5f, y + 54, 75, 75, SM_GametypeIcon(gt, qtrue), NULL);
    SM_CardCentered(cx, cw, y + 134.6f, 26.6667f, gt->title);
    // the factory's title, or "Default" when it is the gametype's
    SM_CardCentered(cx, cw, y + 161.3f, 26.6667f,
                    factory ? (strcmp(gt->title, factory->title) ? factory->title : "Default") : "");
}

static void SM_DrawArenaCard(float x, float y, float w) {
    smArena_t* arena = SM_Arena(sm.selectedMap);
    float cx = x + 10.6667f, cw = w - 21.3333f;

    SM_DrawCardBack(x, y, w, "Current Arena", SM_HIT_ARENA_CARD);
    // span.mapshot: 179 by 100 with the shot's width filling it
    {
        float sx = cx + (cw - 179) * 0.5f;
        smRect_t shot = SM_R(sx, y + 54.8f, 179, 100);

        SM_Pic(sx, y + 54.8f, 179, 179 * 270.0f / 480.0f, SM_Preview(arena, sm.selectedMap), &shot);
    }
    SM_CardCentered(cx, cw, y + 160.5f, 26.6667f, arena ? arena->name : sm.selectedMap);
}

// a text input: #0F0F0F with a #333 border, the text 9 pixels in, a caret while typing
static void SM_DrawInput(int i, float x, float y) {
    const char* value = sm.values[i];
    float w = smL.valW, h = 38;
    float width = w - 17;
    int len = (int)strlen(value);
    int start = 0;
    qboolean editing = sm.edit == i;

    SM_Fill(x, y, w, h, smrInput, NULL);
    SM_Border(x, y, w, h, 1, smrInputBorder, NULL);
    if (editing) {
        if (sm.editCursor > len) {
            sm.editCursor = len;
        }
        if (sm.editStart > sm.editCursor) {
            sm.editStart = sm.editCursor;
        }
        while (sm.editStart < sm.editCursor &&
               SM_Advance(FONT_SANS, 15, value + sm.editStart, sm.editCursor - sm.editStart) > width) {
            sm.editStart++;
        }
        start = sm.editStart;
    }
    SM_TextRaw(x + 9, SM_Baseline(FONT_SANS, 15, y, h), FONT_SANS, 15, smText, value + start,
               SM_Fit(FONT_SANS, 15, value + start, width), qfalse);
    if (editing && ((uiInfo.uiDC.realTime - sm.editTime) / 500) % 2 == 0) {
        SM_Fill(x + 9 + SM_AdvanceRaw(FONT_SANS, 15, value + start, sm.editCursor - start, qfalse), y + 10, 1, h - 20,
                smText, NULL);
    }
    SM_Hit(x, y, w, h, SM_HIT_INPUT, i, NULL);
}

// react-select's control: the option's label in #CCC, or its placeholder in #AAA
static void SM_DrawSelect(int i, float x, float y) {
    const smCvar_t* cv = &smCvars[i];
    const char* label = "Default";
    qboolean chosen = qfalse;
    qboolean open = sm.select == i;
    float w = smL.valW, h = 36;
    int k;

    for (k = 0; k < cv->numOptions; k++) {
        if (!strcmp(cv->options[k].value, sm.values[i])) {
            label = cv->options[k].label;
            chosen = qtrue;
        }
    }
    SM_Fill(x, y, w, h, SM_Over(SM_HIT_SELECT, i) ? smrSelectHover : smrSelect, NULL);
    SM_Text(x + 10, SM_Baseline(FONT_SANS, 15, y + 8, 24), FONT_SANS, 15, chosen ? smrSelectValue : smrPlaceholder,
            label, SM_Fit(FONT_SANS, 15, label, w - 52), qfalse);
    SM_Arrow(x + w - 20, y + 18, open);
    SM_Hit(x, y, w, h, SM_HIT_SELECT, i, NULL);
    if (open) {
        sm.selectControl = SM_R(x, y, w, h);
    }
}

// .range: the value in 20% of the width, then the input: #222 with a #333 border and a yellow
// square of a thumb
static void SM_DrawRange(int i, float x, float y) {
    const smCvar_t* cv = &smCvars[i];
    float bx = x + smL.valW * 0.2f, bw = smL.valW * 0.8f;
    float value, frac;
    char text[32];

    if (sm.drag == i) {
        value = sm.dragValue;
    } else if (sm.held == i) {
        value = (float)atof(sm.heldValue);
    } else {
        value = (float)atof(sm.values[i]);
    }
    SM_FormatRange(i, value, text, sizeof(text));
    SM_Text(x + 8, SM_Baseline(FONT_SANS, 15, y + 1, 16), FONT_SANS, 15, smText, text, -1, qfalse);
    frac = (value - cv->min) / (cv->max - cv->min);
    if (frac < 0) {
        frac = 0;
    } else if (frac > 1) {
        frac = 1;
    }
    SM_Fill(bx, y, bw, 18, (SM_Over(SM_HIT_RANGE, i) || sm.drag == i) ? smrRangeHover : smrRange, NULL);
    SM_Border(bx, y, bw, 18, 1, smrInputBorder, NULL);
    SM_Fill(bx + 1 + frac * (bw - 2 - 16), y + 1, 16, 16, smrYellow, NULL);
    // the hit is the input itself: SM_RangeAt reads the value off it
    SM_Hit(bx, y, bw, 18, SM_HIT_RANGE, i, NULL);
}

static float SM_RowHeight(int i) {
    if (sm.locked[i]) {
        return 50;  // div.disabled holds nothing
    }
    return smCvars[i].widget == SM_INPUT ? 53.0f : smCvars[i].widget == SM_SELECT ? 51.0f : 50.0f;
}

// the row itself and the dropdown it opened count as hovering it
static qboolean SM_RowHovered(int i, float x, float y, float h) {
    smRect_t row = SM_R(x, y, smL.colW, h);

    if (sm.dialog != SM_DIALOG_NONE) {
        return qfalse;
    }
    if (sm.select >= 0 && SM_In(&sm.selectMenu, smMX, smMY)) {
        return i == sm.select;
    }
    return SM_In(&row, smMX, smMY);
}

/*
===============
SM_DrawRow

.row.cvar: #242121 and #1C1919 by turns, its title on the left with "(Reset)" after it while
the row is under the cursor -- or "(Locked)" when the factory sets that cvar -- and the
control on the right.
===============
*/
static float SM_DrawRow(int i, float x, float y, int child) {
    const smCvar_t* cv = &smCvars[i];
    float h = SM_RowHeight(i);
    float base = SM_Baseline(FONT_SANS, 15, y + 13.5f, 24);
    float after = x + 7.5f + SM_Advance(FONT_SANS, 15, va("%s ", cv->title), -1);
    qboolean hovered = SM_RowHovered(i, x, y, h);

    SM_Fill(x, y, smL.colW, h, child % 2 ? smrRowOdd : smrRowEven, NULL);
    SM_Text(x + 7.5f, base, FONT_SANS, 15, smText, cv->title, -1, qfalse);
    if (sm.locked[i]) {
        // disabled: "(Locked)" and no value, as getWidget reads it from a variable
        // declared further down, still undefined there
        SM_Text(after, base, FONT_SANS, 15, smText, "(Locked)", -1, qfalse);
        return h;
    }
    // a.resetLink shows while the row is hovered
    if (hovered) {
        float rw = SM_Advance(FONT_SANS, 15, "(Reset)", -1);

        SM_Text(after, base, FONT_SANS, 15, SM_Over(SM_HIT_RESET, i) ? smrLinkHover : smrLink, "(Reset)", -1, qfalse);
        SM_Hit(after, y + 13.5f, rw, 24, SM_HIT_RESET, i, NULL);
    }
    if (cv->widget == SM_INPUT) {
        SM_DrawInput(i, x + smL.valX, y + 7.5f);
    } else if (cv->widget == SM_SELECT) {
        SM_DrawSelect(i, x + smL.valX, y + 7.5f);
    } else {
        SM_DrawRange(i, x + smL.valX, y + 16.5f);
    }
    return h;
}

static qboolean SM_ShowPortNote(void) {
    return SM_Equals(SM_Value(SM_SERVERTYPE), 2);
}

// p.launch: a button half the page wide, 16pt, without skeleton's capitals
static void SM_DrawLaunch(float y) {
    float w = smL.width * 0.5f, h = 51.3333f;
    float x = smL.x + (smL.width - w) * 0.5f;
    qboolean over = SM_Over(SM_HIT_LAUNCH, 0);
    float tw = SM_SpacedWidth(FONT_SANS, SM_CARD_FONT, "Launch Match", 1.0f);

    SM_Fill(x, y, w, h, over ? smrLaunchHover : smrLaunch, NULL);
    SM_SpacedText(x + (w - tw) * 0.5f, SM_Baseline(FONT_SANS, SM_CARD_FONT, y, h), FONT_SANS, SM_CARD_FONT,
                  over ? smrYellow : smText, "Launch Match", 1.0f, qtrue);
    SM_Hit(x, y, w, h, SM_HIT_LAUNCH, 0, NULL);
}

static void SM_DrawPage(void) {
    float page = UI_Home_Left();
    float top = HM_NAV_H, bottom = UI_Home_FrameBottom();
    float y, listY;
    int child = 0;
    int i;

    // the page sits in the front end's frame, under its top row
    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_PLAY);
    SM_Layout();

    SM_Fill(page, top, HM_CONTAINER_W, bottom - top, smrBlack80, NULL);
    SM_SetSpacing(SM_SPACING_H1);
    SM_Text(smL.x + 6.4f, SM_Baseline(FONT_DEFAULT, 32, top + 8, 38.4f), FONT_DEFAULT, 32, smText, "Start Match", -1,
            qfalse);
    SM_SetSpacing(0);
    y = top + 54.4f;

    // the two cards
    SM_DrawGametypeCard(smL.x, y, smL.colW);
    SM_DrawArenaCard(smL.x, y + SM_CARD_H + SM_CARD_FONT, smL.colW);

    // the cvar list: p.lan follows the match type when it is Internet, one more of the rows'
    // siblings, so the rows after it swap their shades
    listY = y;
    for (i = 0; i < SM_NUM_CVARS; i++) {
        listY += SM_DrawRow(i, smL.listX, listY, ++child);
        if (i == SM_SERVERTYPE && SM_ShowPortNote()) {
            char note[128];

            Com_sprintf(note, sizeof(note),
                        "To host public Internet servers, you will need to forward UDP port %s to your machine.",
                        sm.netPort);
            SM_Text(smL.listX + 11.25f, SM_Baseline(FONT_SANS, 15, listY + 7.5f, 24), FONT_SANS, 15, smText, note,
                    SM_Fit(FONT_SANS, 15, note, smL.colW - 11.25f), qfalse);
            listY += SM_LAN_H;
            child++;
        }
    }

    // the row is as tall as its taller column; the launch paragraph comes 1em under it
    y += 2 * (SM_CARD_H + SM_CARD_FONT) > listY - y ? 2 * (SM_CARD_H + SM_CARD_FONT) : listY - y;
    SM_DrawLaunch(y + 15 + 5.3333f);
}

// .Select-menu-outer, under the control: #333, options 40 high, the focused one #222
static void SM_DrawDropdown(void) {
    const smCvar_t* cv = &smCvars[sm.select];
    float x = sm.selectControl.x, y = sm.selectControl.y + sm.selectControl.h - 1;
    float w = sm.selectControl.w;
    float full = cv->numOptions * 40.0f;
    float view = full < 198.0f ? full : 198.0f;
    smRect_t box = SM_R(x, y, w, view);
    int k;

    if (sm.optionScroll > full - view) {
        sm.optionScroll = full - view;
    }
    if (sm.optionScroll < 0) {
        sm.optionScroll = 0;
    }
    SM_Fill(x, y, w, view, smrSelect, NULL);
    SM_Hit(x, y, w, view, SM_HIT_MENU, sm.select, NULL);
    for (k = 0; k < cv->numOptions; k++) {
        float oy = y + k * 40.0f - sm.optionScroll;
        float base = SM_Baseline(FONT_SANS, 15, oy + 8, 24);

        // the option under the mouse takes the focus
        if (SM_Over(SM_HIT_OPTION, k)) {
            sm.selectFocus = k;
        }
        if (k == sm.selectFocus) {
            SM_Fill(x, oy, w, 40, smrOptionFocus, &box);
        }
        if (SM_TextInside(base, 15, &box)) {
            SM_Text(x + 10, base, FONT_SANS, 15, k == sm.selectFocus ? smrSelectValue : smrOption,
                    cv->options[k].label, -1, qfalse);
        }
        SM_Hit(x, oy, w, 40, SM_HIT_OPTION, k, &box);
    }
    sm.selectMenu = SM_R(x, y, w, view);
}

/*
===============================================================================

THE DIALOGS

The modal-styles module: an overlay at 80% black, and a band across the whole screen --
600 high for the gametypes, over the chosen arena's large shot, 980 for the arenas -- with
the widemodal-content at 80% black in its middle 70%.

===============================================================================
*/

static smRect_t SM_DrawDialog(float height, qhandle_t background) {
    float top = (SM_VIEW_H - height) * 0.5f;
    float cx = smVW * 0.15f, cw = smVW * 0.7f;

    SM_Fill(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, smrBlack80, NULL);
    SM_Hit(-smVW, -SM_VIEW_H, 3 * smVW, 3 * SM_VIEW_H, SM_HIT_BLOCK, 0, NULL);
    SM_Fill(0, top, smVW, height, smrDialog, NULL);
    if (background) {
        // background-size: 100%, centred
        smRect_t band = SM_R(0, top, smVW, height);
        float shotH = smVW * 1080.0f / 1920.0f;

        SM_Pic(0, top + (height - shotH) * 0.5f, smVW, shotH, background, &band);
    }
    SM_Fill(cx, top, cw, height, smrBlack80, NULL);
    return SM_R(cx + 15, top + 15, cw - 30, height - 30);
}

// Cancel and OK at the right, skeleton's buttons with a space between them
static void SM_DrawButtons(float right, float y) {
    float okW = SM_WebButtonWidth("OK");
    float cancelW = SM_WebButtonWidth("Cancel");
    float okX = right - okW;
    float cancelX = okX - SM_Advance(FONT_SANS, 15, " ", -1) - cancelW;

    SM_WebButton(cancelX, y, cancelW, "Cancel", qfalse, SM_Over(SM_HIT_CANCEL, 0), NULL);
    SM_Hit(cancelX, y, cancelW, SM_WEB_BUTTON_H, SM_HIT_CANCEL, 0, NULL);
    SM_WebButton(okX, y, okW, "OK", qfalse, SM_Over(SM_HIT_OK, 0), NULL);
    SM_Hit(okX, y, okW, SM_WEB_BUTTON_H, SM_HIT_OK, 0, NULL);
}

// the webkit scrollbar: an 8 pixel #333 thumb, #999 under the cursor
static void SM_DrawScrollbar(float x, float y, float h, float content, float scroll) {
    float thumbH = h * h / content;
    float thumbY;

    if (thumbH < 16) {
        thumbH = 16;
    }
    thumbY = y + (h - thumbH) * scroll / (content - h);
    SM_Hit(x, y, 8, h, SM_HIT_TRACK, 0, NULL);
    SM_Fill(x, thumbY, 8, thumbH, (SM_Over(SM_HIT_THUMB, 0) || sm.scrollDrag) ? smrThumbHover : smrThumb, NULL);
    SM_Hit(x, thumbY, 8, thumbH, SM_HIT_THUMB, 0, NULL);
}

static float SM_ClampScroll(float scroll, float content, float view) {
    if (scroll > content - view) {
        scroll = content - view;
    }
    return scroll < 0 ? 0 : scroll;
}

// the height of a factory's entry: its title, author and description, wrapped
static float SM_FactoryHeight(const smFactory_t* f, float width) {
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    float h = 7.5f;

    h += SM_Wrap(FONT_SANS, 24, f->title, width, starts, lens) * 38.4f;
    h += SM_Wrap(FONT_SANS, 15, f->author[0] ? f->author : "No author provided", width, starts, lens) * 24.0f;
    h += SM_Wrap(FONT_SANS, 15, f->description[0] ? f->description : "No description.", width, starts, lens) * 24.0f;
    return h;
}

static float SM_DrawLines(float x, float y, float size, float lineHeight, const float* color, const char* text,
                          float width, qboolean bold, const smRect_t* clip) {
    int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
    int n = SM_Wrap(FONT_SANS, size, text, width, starts, lens);
    int k;

    for (k = 0; k < n; k++) {
        float base = SM_Baseline(FONT_SANS, size, y, lineHeight);

        if (SM_TextInside(base, size, clip)) {
            SM_Text(x, base, FONT_SANS, size, color, text + starts[k], lens[k], bold);
        }
        y += lineHeight;
    }
    return y;
}

/*
===============
SM_DrawFactories

The gametypes on the left, each its small icon and its title, the chosen one on #333;
the factories of that gametype on the right, the chosen one on #333 in yellow.
===============
*/
static void SM_DrawFactories(void) {
    smRect_t in = SM_DrawDialog(600, SM_Levelshot(sm.tempMap));
    smRect_t clip;
    float space = SM_Advance(FONT_SANS, 18.6667f, " ", 1);
    float itemW = 0, menuW, ax, aw, ah = in.h * 0.95f, ew, content, y;
    int list[SM_MAX_FACTORIES];
    int n, k;

    // menu: the gametypes, as wide as the widest with its icon and padding
    for (k = 0; k < SM_NUM_GAMETYPES; k++) {
        float w = SM_Advance(FONT_SANS, 18.6667f, smGametypes[k].title, -1);

        if (w > itemW) {
            itemW = w;
        }
    }
    itemW += 3 + 28 + 5 + space + 50;
    menuW = itemW + 18.6667f;
    for (k = 0; k < SM_NUM_GAMETYPES; k++) {
        float ry = in.y + k * 36.0f;

        if (smGametypes[k].gametype == sm.selectedGametype) {
            SM_Fill(in.x, ry, itemW, 36, smrSelect, NULL);
        }
        SM_Pic(in.x + 3, ry + 4, 28, 28, SM_GametypeIcon(&smGametypes[k], qfalse), NULL);
        SM_Text(in.x + 36 + space, SM_Baseline(FONT_SANS, 18.6667f, ry + 3, 29.8667f), FONT_SANS, 18.6667f, smText,
                smGametypes[k].title, -1, qfalse);
        SM_Hit(in.x, ry, itemW, 36, SM_HIT_GAMETYPE, k, NULL);
    }

    // article: the factories, scrolling when they do not fit
    ax = in.x + menuW;
    aw = in.w - menuW;
    clip = SM_R(ax, in.y, aw, ah);
    n = SM_FactoryList(list);
    ew = aw;
    content = 0;
    for (k = 0; k < n; k++) {
        content += SM_FactoryHeight(&smFactories[list[k]], ew - 15) + 15;
    }
    if (content > ah) {
        ew = aw - 8;
        content = 0;
        for (k = 0; k < n; k++) {
            content += SM_FactoryHeight(&smFactories[list[k]], ew - 15) + 15;
        }
    }
    sm.factoryScroll = SM_ClampScroll(sm.factoryScroll, content, ah);
    y = in.y - sm.factoryScroll;
    for (k = 0; k < n; k++) {
        const smFactory_t* f = &smFactories[list[k]];
        float eh = SM_FactoryHeight(f, ew - 15);

        if (y < clip.y + clip.h && y + eh > clip.y) {
            qboolean selected = !strcmp(f->id, sm.tempFactory);
            const float* color = selected ? smrYellow : smText;
            float ty = y + 3.75f;

            if (selected) {
                SM_Fill(ax, y, ew, eh, smrSelect, &clip);
            }
            ty = SM_DrawLines(ax + 15, ty, 24, 38.4f, color, f->title, ew - 15, qtrue, &clip);
            ty = SM_DrawLines(ax + 15, ty, 15, 24, color, f->author[0] ? f->author : "No author provided", ew - 15,
                              qfalse, &clip);
            SM_DrawLines(ax + 15, ty, 15, 24, color, f->description[0] ? f->description : "No description.", ew - 15,
                         qfalse, &clip);
            SM_Hit(ax, y, ew, eh, SM_HIT_FACTORY, list[k], &clip);
        }
        y += eh + 15;
    }
    if (content > ah) {
        SM_DrawScrollbar(ax + aw - 8, in.y, ah, content, sm.factoryScroll);
    }
    sm.article = clip;
    sm.articleContent = content;
    SM_DrawButtons(in.x + in.w, in.y + ah + 10);
}

// lays out (and draws, given a clip) the map tiles as inline blocks; returns their height
static float SM_MapTiles(float x, float y, float width, const smRect_t* clip) {
    float cx = 0, cy = 0;
    int k;

    for (k = 0; k < smNumMapList; k++) {
        smArena_t* a = &smArenas[smMapList[k]];
        // as wide as the levelshot or the name, with the padding
        float tw = SM_NameWidth(a);

        tw = (tw > 250 ? tw : 250) + 12;
        if (cx > 0 && cx + tw + 24 > width) {
            cx = 0;
            cy += 214.4f;
        }
        if (clip && y + cy < clip->y + clip->h && y + cy + 190.4f > clip->y) {
            float tx = x + cx, ty = y + cy;
            float base = SM_Baseline(FONT_SANS, 24, ty + 146, 38.4f);
            qboolean selected = !strcmp(a->sysname, sm.tempMap);
            smRect_t shot = SM_R(tx + (tw - 250) * 0.5f, ty + 6, 250, 140);

            if (selected) {
                SM_Fill(tx, ty, tw, 190.4f, smrSelect, clip);
            }
            if (SM_Clip(&shot, clip)) {
                SM_Pic(tx + (tw - 250) * 0.5f, ty + 6, 250, 250 * 270.0f / 480.0f, SM_Preview(a, a->sysname), &shot);
            }
            if (SM_TextInside(base, 24, clip)) {
                SM_Text(tx + (tw - SM_NameWidth(a)) * 0.5f, base, FONT_SANS, 24, selected ? smrYellow : smText, a->name,
                        -1, qtrue);
            }
            SM_Hit(tx, ty, tw, 190.4f, SM_HIT_MAP, smMapList[k], clip);
        }
        cx += tw + 24;
    }
    return smNumMapList ? cy + 214.4f : 0;
}

/*
===============
SM_DrawMaps

ALL, A to Z and # down the left, the chosen one on #333, and the arenas that play the
gametype in tiles, the chosen one on #333 with its name in yellow.
===============
*/
static void SM_DrawMaps(void) {
    smRect_t in = SM_DrawDialog(980, 0);
    smRect_t clip;
    float itemW = SM_Advance(FONT_SANS, 18.6667f, "ALL", -1) + 18.6667f;
    float menuW = itemW + 18.6667f;
    float ax = in.x + menuW, aw = in.w - menuW, ah = in.h * 0.95f, width, content;
    int k;

    // menu: ALL, A to Z and #, one under the other
    for (k = 0; k < 28; k++) {
        const char* label = k == 0 ? "ALL" : va("%c", SM_FilterChar(k));
        float ry = in.y + k * 33.6f;

        if (SM_FilterChar(k) == sm.filter) {
            SM_Fill(in.x, ry, itemW, 33.6f, smrSelect, NULL);
        }
        SM_Text(in.x + (itemW - SM_Advance(FONT_SANS, 18.6667f, label, -1)) * 0.5f,
                SM_Baseline(FONT_SANS, 18.6667f, ry + 1.8667f, 29.8667f), FONT_SANS, 18.6667f, smText, label, -1,
                qfalse);
        SM_Hit(in.x, ry, itemW, 33.6f, SM_HIT_FILTER, k, NULL);
    }

    // article: the maps, scrolling when they do not fit
    SM_BuildMapList();
    clip = SM_R(ax, in.y, aw, ah);
    width = aw;
    content = SM_MapTiles(ax, in.y, width, NULL);
    if (content > ah) {
        width = aw - 8;
        content = SM_MapTiles(ax, in.y, width, NULL);
    }
    sm.mapScroll = SM_ClampScroll(sm.mapScroll, content, ah);
    SM_MapTiles(ax, in.y - sm.mapScroll, width, &clip);
    if (content > ah) {
        SM_DrawScrollbar(ax + aw - 8, in.y, ah, content, sm.mapScroll);
    }
    sm.article = clip;
    sm.articleContent = content;
    SM_DrawButtons(in.x + in.w, in.y + ah + 10);
}

/*
===============================================================================

INPUT

===============================================================================
*/

static void SM_CancelDialog(void) {
    if (sm.dialog == SM_DIALOG_FACTORIES) {
        SM_CancelFactories();
    } else if (sm.dialog == SM_DIALOG_MAPS) {
        SM_CancelMaps();
    }
}

static void SM_ConfirmDialog(void) {
    if (sm.dialog == SM_DIALOG_FACTORIES) {
        SM_ConfirmFactories();
    } else if (sm.dialog == SM_DIALOG_MAPS) {
        SM_ConfirmMaps();
    }
}

static float* SM_DialogScroll(void) {
    return sm.dialog == SM_DIALOG_FACTORIES ? &sm.factoryScroll : &sm.mapScroll;
}

static void SM_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);

    // the bar above the page takes its own clicks
    if (sm.dialog == SM_DIALOG_NONE && sm.select < 0 && UI_Home_NavClick()) {
        return;
    }
    // a click redraws the page, which lets go of a slider's leftover value
    sm.held = -1;
    // the text input loses the focus to a click elsewhere
    if (sm.edit >= 0 && !(hit && hit->type == SM_HIT_INPUT && hit->index == sm.edit)) {
        SM_EndEdit();
    }
    // and an open dropdown closes, unless the click is on it
    if (sm.select >= 0 && !(hit && (hit->type == SM_HIT_MENU || hit->type == SM_HIT_OPTION ||
                                    (hit->type == SM_HIT_SELECT && hit->index == sm.select)))) {
        sm.select = -1;
    }
    if (!hit) {
        return;
    }
    switch (hit->type) {
        case SM_HIT_GAMETYPE_CARD:
            // the dialog opens scrolled to the top: a hidden element loses its scroll
            sm.dialog = SM_DIALOG_FACTORIES;
            sm.factoryScroll = 0;
            break;
        case SM_HIT_ARENA_CARD:
            sm.dialog = SM_DIALOG_MAPS;
            sm.mapScroll = 0;
            break;
        case SM_HIT_RESET:
            trap_Cmd_ExecuteText(EXEC_APPEND, va("reset %s\n", smCvars[hit->index].cvar));
            break;
        case SM_HIT_INPUT:
            if (sm.edit != hit->index) {
                sm.edit = hit->index;
                sm.editStart = 0;
            }
            sm.editCursor = SM_CaretAt(sm.values[sm.edit], sm.editStart, smMX - (hit->r.x + 9));
            sm.editTime = uiInfo.uiDC.realTime;
            break;
        case SM_HIT_SELECT:
            if (sm.select == hit->index) {
                sm.select = -1;
            } else {
                SM_OpenSelect(hit->index);
            }
            break;
        case SM_HIT_OPTION:
            // onSelectChanged: the cvar changes, and the page shows it when it hears so
            trap_Cvar_Set(smCvars[sm.select].cvar, smCvars[sm.select].options[hit->index].value);
            sm.select = -1;
            break;
        case SM_HIT_RANGE:
            sm.drag = hit->index;
            sm.dragRect = hit->r;
            sm.dragValue = SM_RangeAt(sm.drag, smMX);
            break;
        case SM_HIT_LAUNCH:
            SM_Launch();
            break;
        case SM_HIT_GAMETYPE:
            SM_SelectGametype(smGametypes[hit->index].gametype);
            break;
        case SM_HIT_FACTORY:
            SM_SelectFactory(smFactories[hit->index].id);
            break;
        case SM_HIT_FILTER:
            sm.filter = SM_FilterChar(hit->index);
            break;
        case SM_HIT_MAP:
            Q_strncpyz(sm.tempMap, smArenas[hit->index].sysname, sizeof(sm.tempMap));
            break;
        case SM_HIT_CANCEL:
            SM_CancelDialog();
            break;
        case SM_HIT_OK:
            SM_ConfirmDialog();
            break;
        case SM_HIT_THUMB:
            sm.scrollDrag = qtrue;
            sm.scrollGrab = smMY - hit->r.y;
            break;
        case SM_HIT_TRACK:
            // a click on the track pages up or down
            if (smMY < sm.article.y + sm.article.h * 0.5f) {
                *SM_DialogScroll() -= sm.article.h * 0.875f;
            } else {
                *SM_DialogScroll() += sm.article.h * 0.875f;
            }
            break;
        default:
            break;
    }
}

static void SM_Wheel(float amount) {
    if (sm.select >= 0 && SM_In(&sm.selectMenu, smMX, smMY)) {
        sm.optionScroll += amount;
    } else if (sm.dialog != SM_DIALOG_NONE && SM_In(&sm.article, smMX, smMY)) {
        *SM_DialogScroll() += amount;
    }
}

// what is held down with the mouse follows it, and is let go when the button is
static void SM_UpdateDrags(void) {
    if (sm.drag >= 0) {
        if (trap_Key_IsDown(K_MOUSE1)) {
            // onInputRange: the value shown follows the thumb
            sm.dragValue = SM_RangeAt(sm.drag, smMX);
        } else {
            // onMouseUp: only a release over the slider sets the cvar
            char value[32];

            SM_FormatRange(sm.drag, sm.dragValue, value, sizeof(value));
            if (SM_In(&sm.dragRect, smMX, smMY)) {
                trap_Cvar_Set(smCvars[sm.drag].cvar, value);
            } else {
                sm.held = sm.drag;
                Q_strncpyz(sm.heldValue, value, sizeof(sm.heldValue));
            }
            sm.drag = -1;
        }
    }
    if (sm.scrollDrag) {
        if (!trap_Key_IsDown(K_MOUSE1) || sm.dialog == SM_DIALOG_NONE) {
            sm.scrollDrag = qfalse;
        } else if (sm.articleContent > sm.article.h) {
            float thumbH = sm.article.h * sm.article.h / sm.articleContent;

            if (thumbH < 16) {
                thumbH = 16;
            }
            *SM_DialogScroll() = (smMY - sm.scrollGrab - sm.article.y) / (sm.article.h - thumbH) *
                                 (sm.articleContent - sm.article.h);
        }
    }
}

// typing in a text input; false for what is not for it
static qboolean SM_EditKey(int key) {
    char* value = sm.values[sm.edit];
    int len = (int)strlen(value);

    if (key & K_CHAR_FLAG) {
        key &= ~K_CHAR_FLAG;
        if (key == 'h' - 'a' + 1) {  // backspace comes as ctrl-h
            if (sm.editCursor > 0) {
                memmove(&value[sm.editCursor - 1], &value[sm.editCursor], len + 1 - sm.editCursor);
                sm.editCursor--;
            }
        } else if (key >= 32 && key < 256 && len < SM_MAX_INPUT) {
            memmove(&value[sm.editCursor + 1], &value[sm.editCursor], len + 1 - sm.editCursor);
            value[sm.editCursor++] = (char)key;
        }
        sm.editTime = uiInfo.uiDC.realTime;
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
        case K_MOUSE2:
        case K_MOUSE3:
        case K_MWHEELUP:
        case K_MWHEELDOWN:
            return qfalse;
        case K_DEL:
        case K_KP_DEL:
            if (sm.editCursor < len) {
                memmove(&value[sm.editCursor], &value[sm.editCursor + 1], len - sm.editCursor);
            }
            break;
        case K_LEFTARROW:
        case K_KP_LEFTARROW:
            if (sm.editCursor > 0) {
                sm.editCursor--;
            }
            break;
        case K_RIGHTARROW:
        case K_KP_RIGHTARROW:
            if (sm.editCursor < len) {
                sm.editCursor++;
            }
            break;
        case K_HOME:
        case K_KP_HOME:
            sm.editCursor = 0;
            break;
        case K_END:
        case K_KP_END:
            sm.editCursor = len;
            break;
        case K_ESCAPE:
        case K_TAB:
            // leaving the field: onBlur
            SM_EndEdit();
            break;
        default:
            // Enter does nothing in the page's inputs; typed keys come again as characters
            break;
    }
    sm.editTime = uiInfo.uiDC.realTime;
    return qtrue;
}

/*
===============================================================================

ENTRY POINTS

===============================================================================
*/

static void SM_Load(void) {
    if (sm.loaded) {
        return;
    }
    // the page's factories and mapdb modules read the engine once, when the web UI loads
    SM_LoadFactories();
    SM_LoadArenas();
    SM_Defaults();
    sm.edit = -1;
    sm.select = -1;
    sm.drag = -1;
    sm.held = -1;
    sm.loaded = qtrue;
}

/*
===============
UI_StartMatch_Open

The "startmatch" menu's onOpen: the page mounts again with the state it saved,
its dialogs closed.
===============
*/
void UI_StartMatch_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    SM_Load();
    sm.dialog = SM_DIALOG_NONE;
    sm.edit = -1;
    sm.select = -1;
    sm.drag = -1;
    sm.held = -1;
    sm.scrollDrag = qfalse;
    SM_ReadCvars(qfalse);
    // StartMatchCvars' componentWillMount: the match type picks the port, and the network restarts on it
    if (sm.values[SM_SERVERTYPE][0]) {
        trap_Cvar_Set("net_port", SM_Equals(sm.values[SM_SERVERTYPE], 1) ? "27015" : "27960");
        trap_Cmd_ExecuteText(EXEC_APPEND, "net_restart\n");
    }
    // the page takes keys before the mouse moves over it
    menu = Menus_FindByName("startmatch");
    item = menu ? Menu_FindItemByName(menu, "startmatch") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_StartMatch_Paint(void) {
    const smHit_t* under;

    SM_Frame();
    SM_SetLook(SM_LOOK_RETAIL);
    if (!sm.loaded) {
        SM_Load();
        SM_ReadCvars(qfalse);
    }
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;

    SM_ReadCvars(qtrue);
    SM_UpdateLocks();
    SM_UpdateDrags();
    SM_DrawPage();
    if (sm.select >= 0) {
        SM_DrawDropdown();
    }
    if (sm.dialog == SM_DIALOG_FACTORIES) {
        SM_DrawFactories();
    } else if (sm.dialog == SM_DIALOG_MAPS) {
        SM_DrawMaps();
    }
    UI_Home_Banner();

    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
}

qboolean UI_StartMatch_HandleKey(int key) {
    if (sm.edit >= 0 && SM_EditKey(key)) {
        return qtrue;
    }
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
            SM_Click();
            return qtrue;
        case K_MWHEELUP:
            SM_Wheel(-100);
            return qtrue;
        case K_MWHEELDOWN:
            SM_Wheel(100);
            return qtrue;
        case K_ENTER:
        case K_KP_ENTER:
            // the dialogs' keydown handlers: Enter confirms
            SM_ConfirmDialog();
            return qtrue;
        case K_ESCAPE:
            // and Escape cancels; an open dropdown closes first
            if (sm.select >= 0) {
                sm.select = -1;
                return qtrue;
            }
            if (sm.dialog != SM_DIALOG_NONE) {
                SM_CancelDialog();
                return qtrue;
            }
            // the menu's onESC goes back to the main menu
            return qfalse;
        default:
            return qfalse;
    }
}

/*
===============
UI_StartMatch_Preselect

[ioquakelive] The main menu opens Start Match with a gametype already chosen: its "featured
modes". The choice is made the same way as from the gametype dialog -- the factory and, if the
chosen map does not support that mode, the mode's default map -- so the page ends up as if the
player had picked it there.
===============
*/
void UI_StartMatch_Preselect(const char* factory) {
    const smFactory_t* f;

    SM_Load();
    Q_strncpyz(sm.tempFactory, factory, sizeof(sm.tempFactory));
    SM_ConfirmFactories();
    f = SM_Factory(sm.selectedFactory);
    if (f) {
        sm.selectedGametype = f->basegt;
    }
}
