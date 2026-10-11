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

#define JSON_IMPLEMENTATION
#include "../qcommon/json.h"
#undef JSON_IMPLEMENTATION

#include <time.h>

#define MT_MAX_MATCHES 100
#define MT_MAX_ROWS 64
#define MT_MAX_COLS 32
#define MT_MAX_EVENTS 1000
#define MT_MAX_MEDIA 16

typedef enum {
    MT_HIT_NONE,
    MT_HIT_MATCH,
    MT_HIT_BACK,
    MT_HIT_SECTION,
    MT_HIT_SORT,
    MT_HIT_PROFILE,
    MT_HIT_MEDIA,
    MT_HIT_SHOT
} mtHitType_t;

typedef enum {
    MT_SEC_PLAYERS,
    MT_SEC_MEDALS,
    MT_SEC_ITEMS,
    MT_SEC_KILLS,
    MT_SEC_DEATHS,
    MT_SEC_ACCURACY,
    MT_SEC_DAMAGE,
    MT_SEC_EVENTS,
    MT_NUM_SECTIONS
} mtSection_t;

static const char* const mtSectionNames[MT_NUM_SECTIONS] = {"Basic",    "Medals", "Items",  "Kills",
                                                            "Deaths",   "Accuracy", "Damage", "Events"};

typedef struct {
    const char* id;
    const char* img;
} mtMedal_t;

static const mtMedal_t mtMedals[] = {
    {"FIRSTFRAG", "medal_firstfrag"},   {"HUMILIATION", "medal_gauntlet"}, {"EXCELLENT", "medal_excellent"},
    {"REVENGE", "medal_revenge"},       {"COMBOKILL", "medal_combokill"},  {"MIDAIR", "medal_midair"},
    {"PERFORATED", "medal_perforated"}, {"RAMPAGE", "medal_rampage"},      {"IMPRESSIVE", "medal_impressive"},
    {"CAPTURES", "medal_capture"},      {"ASSISTS", "medal_assist"},       {"DEFENDS", "medal_defense"},
    {"HEADSHOT", "medal_headshot"},     {"QUADGOD", "medal_quadgod"},      {"PERFECT", "medal_perfect"},
    {"ACCURACY", "medal_accuracy"},
};
#define MT_NUM_MEDALS ((int)ARRAY_LEN(mtMedals))

typedef struct {
    const char* name;
    const char* title;
} mtGametype_t;

static const mtGametype_t mtGametypes[] = {
    {"ffa", "Free For All"}, {"duel", "Duel"},          {"race", "Race"},          {"tdm", "Team Deathmatch"},
    {"ca", "Clan Arena"},    {"ctf", "Capture The Flag"}, {"1fctf", "1-Flag CTF"}, {"harvester", "Harvester"},
    {"ft", "Freeze Tag"},    {"dom", "Domination"},     {"ad", "Attack & Defend"}, {"rr", "Red Rover"},
};

typedef enum {
    MT_READ_PATH,
    MT_READ_WEAPON,
    MT_READ_ACCURACY,
    MT_READ_RACE
} mtRead_t;

typedef struct {
    const char* label;
    mtRead_t read;
    const char* a;
    const char* b;
} mtColumn_t;

static const mtColumn_t mtPlayerCols[] = {
    {"Score", MT_READ_PATH, "SCORE", NULL},   {"K", MT_READ_PATH, "KILLS", NULL},
    {"D", MT_READ_PATH, "DEATHS", NULL},      {"DmgD", MT_READ_PATH, "DAMAGE", "DEALT"},
    {"DmgT", MT_READ_PATH, "DAMAGE", "TAKEN"}, {NULL},
};
static const mtColumn_t mtRaceCols[] = {{"Score", MT_READ_RACE, "SCORE", NULL}, {NULL}};
static const mtColumn_t mtMedalCols[] = {
    {"Acc", MT_READ_PATH, "MEDALS", "ACCURACY"},    {"Asst", MT_READ_PATH, "MEDALS", "ASSISTS"},
    {"Cap", MT_READ_PATH, "MEDALS", "CAPTURES"},    {"Combo", MT_READ_PATH, "MEDALS", "COMBOKILL"},
    {"Def", MT_READ_PATH, "MEDALS", "DEFENDS"},     {"Exc", MT_READ_PATH, "MEDALS", "EXCELLENT"},
    {"Headshot", MT_READ_PATH, "MEDALS", "HEADSHOT"}, {"Hum", MT_READ_PATH, "MEDALS", "HUMILIATION"},
    {"Impr", MT_READ_PATH, "MEDALS", "IMPRESSIVE"}, {"Midair", MT_READ_PATH, "MEDALS", "MIDAIR"},
    {"Prft", MT_READ_PATH, "MEDALS", "PERFECT"},    {"Perf", MT_READ_PATH, "MEDALS", "PERFORATED"},
    {"Quad God", MT_READ_PATH, "MEDALS", "QUADGOD"}, {"Rampage", MT_READ_PATH, "MEDALS", "RAMPAGE"},
    {"Revenge", MT_READ_PATH, "MEDALS", "REVENGE"}, {NULL},
};
static const mtColumn_t mtItemCols[] = {
    {"Ammo", MT_READ_PATH, "PICKUPS", "AMMO"},
    {"Armor", MT_READ_PATH, "PICKUPS", "ARMOR"},
    {"Armor Regen", MT_READ_PATH, "PICKUPS", "ARMOR_REGEN"},
    {"Battlesuit", MT_READ_PATH, "PICKUPS", "BATTLESUIT"},
    {"Blue Flag", MT_READ_PATH, "PICKUPS", "BLUE_FLAG"},
    {"Doubler", MT_READ_PATH, "PICKUPS", "DOUBLER"},
    {"Flight", MT_READ_PATH, "PICKUPS", "FLIGHT"},
    {"GA", MT_READ_PATH, "PICKUPS", "GREEN_ARMOR"},
    {"Guard", MT_READ_PATH, "PICKUPS", "GUARD"},
    {"Haste", MT_READ_PATH, "PICKUPS", "HASTE"},
    {"Health", MT_READ_PATH, "PICKUPS", "HEALTH"},
    {"Invisibility", MT_READ_PATH, "PICKUPS", "INVIS"},
    {"Invuln", MT_READ_PATH, "PICKUPS", "INVULNERABILITY"},
    {"Kamikaze", MT_READ_PATH, "PICKUPS", "KAMIKAZE"},
    {"Medkit", MT_READ_PATH, "PICKUPS", "MEDKIT"},
    {"MH", MT_READ_PATH, "PICKUPS", "MEGA_HEALTH"},
    {"Neutral Flag", MT_READ_PATH, "PICKUPS", "NEUTRAL_FLAG"},
    {"Other Holdable", MT_READ_PATH, "PICKUPS", "OTHER_HOLDABLE"},
    {"Other Powerup", MT_READ_PATH, "PICKUPS", "OTHER_POWERUP"},
    {"Portal", MT_READ_PATH, "PICKUPS", "PORTAL"},
    {"Quad", MT_READ_PATH, "PICKUPS", "QUAD"},
    {"Red Armor", MT_READ_PATH, "PICKUPS", "RED_ARMOR"},
    {"Red Flag", MT_READ_PATH, "PICKUPS", "RED_FLAG"},
    {"Regen", MT_READ_PATH, "PICKUPS", "REGEN"},
    {"Scout", MT_READ_PATH, "PICKUPS", "SCOUT"},
    {"Teleporter", MT_READ_PATH, "PICKUPS", "TELEPORTER"},
    {"YA", MT_READ_PATH, "PICKUPS", "YELLOW_ARMOR"},
    {NULL},
};

static const char* const mtWeaponLabels[] = {"BFG", "CG", "G",     "GL", "HMG", "LG", "MG",
                                             "NG",  "OTHER", "PG", "PL", "RG",  "RL", "SG"};
static const char* const mtWeaponKeys[] = {"BFG",     "CHAINGUN", "GAUNTLET",     "GRENADE", "HMG",
                                           "LIGHTNING", "MACHINEGUN", "NAILGUN", "OTHER_WEAPON", "PLASMA",
                                           "PROXMINE", "RAILGUN",  "ROCKET",       "SHOTGUN"};
#define MT_NUM_WEAPONS ((int)ARRAY_LEN(mtWeaponKeys))

typedef struct {
    const char* name;
    const char* img;
} mtIcon_t;

static const mtIcon_t mtPowerups[] = {
    {"QUAD", "quad"},         {"BATTLESUIT", "battlesuit"}, {"HASTE", "haste"},       {"INVIS", "invis"},
    {"REGEN", "regen"},       {"FLIGHT", "flight"},         {"RED_FLAG", "redflag"},  {"BLUE_FLAG", "blueflag"},
    {"NEUTRAL_FLAG", "nflag"}, {"SCOUT", "scout"},          {"GUARD", "guard"},       {"DOUBLER", "doubler"},
    {"ARMOR_REGEN", "armor_regen"}, {"INVULNERABILITY", "invuln"},
};

typedef struct {
    const char* mod;
    const char* img;
    const char* img2;
    const char* text;
} mtMod_t;

static const mtMod_t mtMods[] = {
    {"UNKNOWN", "frag", NULL, "(Unknown)"},
    {"SHOTGUN", "shotgun", NULL, NULL},
    {"GAUNTLET", "gauntlet", NULL, NULL},
    {"MACHINEGUN", "machinegun", NULL, NULL},
    {"GRENADE", "grenade", "headshot", NULL},
    {"GRENADE_SPLASH", "grenade", NULL, NULL},
    {"ROCKET", "rocket", "headshot", NULL},
    {"ROCKET_SPLASH", "rocket", NULL, NULL},
    {"PLASMA", "plasma", NULL, NULL},
    {"PLASMA_SPLASH", "plasma", NULL, NULL},
    {"RAILGUN", "railgun", NULL, NULL},
    {"LIGHTNING", "lightning", NULL, NULL},
    {"BFG", "bfg", "headshot", NULL},
    {"BFG_SPLASH", "bfg", NULL, NULL},
    {"WATER", "frag", NULL, "(Water)"},
    {"SLIME", "frag", NULL, "(Slime)"},
    {"LAVA", "frag", NULL, "(Lava)"},
    {"CRUSH", "frag", NULL, "(Crushed)"},
    {"TELEFRAG", "frag", NULL, "(Telefrag)"},
    {"FALLING", "frag", NULL, "(Falling)"},
    {"SUICIDE", "frag", NULL, "(Suicide)"},
    {"LASER", "frag", NULL, "(Laser)"},
    {"TRIGGER_HURT", "frag", NULL, "(Environment)"},
    {"NAILGUN", "nailgun", NULL, NULL},
    {"CHAINGUN", "chaingun", NULL, NULL},
    {"PROXMINE", "proxmine", NULL, NULL},
    {"KAMIKAZE", "kamikaze", NULL, NULL},
    {"JUICED", "frag", NULL, "(Juiced)"},
    {"GRAPPLE", "grapple", NULL, NULL},
    {"SWITCHTEAM", "frag", NULL, "(Switch Team)"},
    {"THAW", "thaw", NULL, NULL},
    {"LIGHTNING_DISCHARGE", "frag", NULL, "(Discharge)"},
    {"HMG", "hmg", NULL, NULL},
    {"RAILGUN_HEADSHOT", "railgun", "headshot", NULL},
};
#define MT_NUM_MODS ((int)ARRAY_LEN(mtMods))

typedef struct {
    char guid[64];
    char map[64];
    char gameType[32];
    char status[8];
    double timePlayed;
    int gameLength;
    char server[128];
    char exitMsg[128];
    qboolean personal;
    int rank, score, kills, deaths;
    int medals[MT_NUM_MEDALS];
    qhandle_t levelshot;
} mtMatch_t;

typedef struct {
    char name[80];
    char steamId[24];
    int team;
    qhandle_t icon;
    double v[MT_MAX_COLS];
    qboolean has[MT_MAX_COLS];
    char text[MT_MAX_COLS][16];
} mtRow_t;

typedef struct {
    int numCols;
    const char* labels[MT_MAX_COLS];
    qboolean shown[MT_MAX_COLS];
    int rows;
    mtRow_t row[MT_MAX_ROWS];
} mtTable_t;

typedef struct {
    char name[64];
    int team;
    int numPowerups;
    qhandle_t powerups[8];
} mtWho_t;

typedef struct {
    int time;
    int mod;
    qboolean killer;
    mtWho_t who[2];
} mtEvent_t;

static struct {
    qboolean loaded;
    int count;
    mtMatch_t match[MT_MAX_MATCHES];

    int open;
    int section;
    mtTable_t table[MT_SEC_EVENTS];
    int sortCol, sortDesc;
    int order[MT_MAX_ROWS];
    int numEvents;
    mtEvent_t events[MT_MAX_EVENTS];
    int numMedia;
    char media[MT_MAX_MEDIA][MAX_QPATH];
    char shot[MAX_QPATH];
    qhandle_t shotImage;

    float scroll;
    float content;

    qhandle_t medal[MT_NUM_MEDALS];
    qhandle_t modIcon[MT_NUM_MODS][2];
    qhandle_t powerupIcon[ARRAY_LEN(mtPowerups)];
} mt;

static char mtBuffer[0x100000];

static const vec4_t mtBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t mtOdd = SM_RGB(36, 33, 33);
static const vec4_t mtEven = SM_RGB(28, 25, 25);
static const vec4_t mtHead = SM_RGB(34, 34, 34);
static const vec4_t mtYellow = SM_RGB(247, 209, 0);
static const vec4_t mtLight = SM_RGB(238, 238, 238);
static const vec4_t mtGrey = SM_RGB(153, 153, 153);
static const vec4_t mtMedalGrey = SM_RGB(170, 170, 170);
static const vec4_t mtLink = SM_RGB(30, 174, 219);
static const vec4_t mtRed = SM_RGB(204, 51, 51);
static const vec4_t mtBlue = SM_RGB(0, 102, 153);
static const vec4_t mtNoTeam = SM_RGB(100, 100, 100);
static const vec4_t mtGametype = {181 / 255.0f, 44 / 255.0f, 23 / 255.0f, 0.5f};
static const vec4_t mtBlack = {0.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t mtWhite = {1.0f, 1.0f, 1.0f, 1.0f};

static int MT_ReadFile(const char* name, char* buf, int size) {
    fileHandle_t f;
    int len = trap_FS_FOpenFile(name, &f, FS_READ);

    if (!f) {
        return -1;
    }
    if (len <= 0 || len >= size) {
        trap_FS_FCloseFile(f);
        return -1;
    }
    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);
    return len;
}

static qboolean MT_Exists(const char* name) {
    fileHandle_t f;
    int len = trap_FS_FOpenFile(name, &f, FS_READ);

    if (f) {
        trap_FS_FCloseFile(f);
    }
    return (qboolean)(f && len > 0);
}

static const char* MT_Member(const char* obj, const char* end, const char* name) {
    if (!obj || obj >= end || *obj != '{') {
        return NULL;
    }
    return JSON_ObjectGetNamedValue(obj, end, name);
}

static qboolean MT_Number(const char* v, const char* end, double* out) {
    if (!v || v >= end || !(*v == '-' || (*v >= '0' && *v <= '9'))) {
        return qfalse;
    }
    *out = JSON_ValueGetDouble(v, end);
    return qtrue;
}

static int MT_Int(const char* obj, const char* end, const char* name) {
    double d = 0;

    MT_Number(MT_Member(obj, end, name), end, &d);
    return (int)d;
}

static void MT_String(const char* obj, const char* end, const char* name, char* out, int size) {
    const char* v = MT_Member(obj, end, name);

    out[0] = '\0';
    if (v && JSON_ValueGetType(v, end) == JSONTYPE_STRING) {
        JSON_ValueGetString(v, end, out, (unsigned int)size);
    }
}

static void MT_StripColors(const char* in, char* out, int size) {
    int n = 0;

    for (; *in && n < size - 1; in++) {
        if (*in == '^') {
            if (in[1] >= '0' && in[1] <= '9') {
                in++;
            }
            continue;
        }
        out[n++] = *in;
    }
    out[n] = '\0';
}

static qhandle_t MT_ModelIcon(const char* model, int team) {
    char base[MAX_QPATH];
    char* skin;
    qhandle_t h;

    Q_strncpyz(base, model[0] ? model : "sarge", sizeof(base));
    skin = strchr(base, '/');
    if (skin) {
        *skin++ = '\0';
    } else {
        skin = "default";
    }
    if (team == 1) {
        skin = "red";
    } else if (team == 2) {
        skin = "blue";
    }
    h = trap_R_RegisterShaderNoMip(va("webui/img/models/icon/%s_%s.png", base, skin));
    if (!h) {
        h = trap_R_RegisterShaderNoMip(va("webui/img/models/icon/%s_default.png", base));
    }
    if (!h) {
        h = trap_R_RegisterShaderNoMip("webui/img/models/icon/sarge_default.png");
    }
    return h;
}

static const char* MT_MapName(const char* map) {
    const char* info = UI_GetArenaInfoByMap(map);
    const char* name = info ? Info_ValueForKey(info, "longname") : "";

    return name[0] ? name : map;
}

static const char* MT_GametypeTitle(const char* shortName) {
    int i;

    for (i = 0; i < (int)ARRAY_LEN(mtGametypes); i++) {
        if (!Q_stricmp(mtGametypes[i].name, shortName)) {
            return mtGametypes[i].title;
        }
    }
    return "Unknown Gametype";
}

static void MT_Ordinal(int n, char* out, int size) {
    static const char* const s[4] = {"th", "st", "nd", "rd"};
    int v = n % 100;
    const char* suffix = ((v - 20) % 10 >= 1 && (v - 20) % 10 <= 3 && v > 20) ? s[(v - 20) % 10]
                         : (v >= 1 && v <= 3) ? s[v] : s[0];

    Com_sprintf(out, size, "%i%s", n, suffix);
}

static void MT_LoadMatch(mtMatch_t* m, const char* guid) {
    const char* end;
    const char* personal;
    int len;
    int i;

    Com_Memset(m, 0, sizeof(*m));
    len = MT_ReadFile(va("matches/%s.json", guid), mtBuffer, sizeof(mtBuffer));
    if (len <= 0) {
        return;
    }
    end = mtBuffer + len;
    Q_strncpyz(m->guid, guid, sizeof(m->guid));
    MT_String(mtBuffer, end, "MAP", m->map, sizeof(m->map));
    MT_String(mtBuffer, end, "GAME_TYPE", m->gameType, sizeof(m->gameType));
    MT_String(mtBuffer, end, "STATUS", m->status, sizeof(m->status));
    MT_Number(MT_Member(mtBuffer, end, "TIME_PLAYED"), end, &m->timePlayed);
    m->gameLength = MT_Int(mtBuffer, end, "GAME_LENGTH");
    MT_String(mtBuffer, end, "SERVER_TITLE", m->server, sizeof(m->server));
    MT_String(mtBuffer, end, "EXIT_MSG", m->exitMsg, sizeof(m->exitMsg));
    personal = MT_Member(mtBuffer, end, "PERSONAL_STATS");
    if (personal && *personal == '{') {
        const char* medals = MT_Member(personal, end, "MEDALS");

        m->personal = qtrue;
        m->rank = MT_Int(personal, end, "RANK");
        m->score = MT_Int(personal, end, "SCORE");
        m->kills = MT_Int(personal, end, "KILLS");
        m->deaths = MT_Int(personal, end, "DEATHS");
        for (i = 0; i < MT_NUM_MEDALS; i++) {
            m->medals[i] = MT_Int(medals, end, mtMedals[i].id);
        }
    }
    m->levelshot = trap_R_RegisterShaderNoMip(va("webui/img/levelshots/sm/%s.jpg", m->map));
    if (!m->levelshot) {
        m->levelshot = trap_R_RegisterShaderNoMip("webui/img/levelshots/sm/default.jpg");
    }
}

static void MT_LoadList(void) {
    static char index[0x10000];
    char* lines[MT_MAX_MATCHES * 50];
    int numLines = 0;
    char* p;
    int len, i;

    mt.count = 0;
    len = MT_ReadFile("matches/index.txt", index, sizeof(index));
    if (len <= 0) {
        return;
    }
    for (p = index; *p && numLines < (int)ARRAY_LEN(lines);) {
        char* eol = strchr(p, '\n');

        if (eol) {
            *eol = '\0';
        }
        if (*p && *p != '\r') {
            char* cr = strchr(p, '\r');

            if (cr) {
                *cr = '\0';
            }
            lines[numLines++] = p;
        }
        if (!eol) {
            break;
        }
        p = eol + 1;
    }
    for (i = numLines - 1; i >= 0 && mt.count < MT_MAX_MATCHES; i--) {
        MT_LoadMatch(&mt.match[mt.count], lines[i]);
        if (mt.match[mt.count].guid[0]) {
            mt.count++;
        }
    }
}

static qboolean MT_Cell(const mtColumn_t* c, const char* p, const char* end, double* v, char* text, int size,
                        qboolean keepZero) {
    double h, s;
    const char* w;

    text[0] = '\0';
    switch (c->read) {
        case MT_READ_PATH: {
            const char* o = c->b ? MT_Member(p, end, c->a) : p;

            if (!MT_Number(MT_Member(o, end, c->b ? c->b : c->a), end, v)) {
                return qfalse;
            }
            break;
        }
        case MT_READ_WEAPON:
            w = MT_Member(MT_Member(p, end, "WEAPONS"), end, c->a);
            if (!MT_Number(MT_Member(w, end, c->b), end, v)) {
                return qfalse;
            }
            break;
        case MT_READ_ACCURACY:
            w = MT_Member(MT_Member(p, end, "WEAPONS"), end, c->a);
            if (!MT_Number(MT_Member(w, end, "H"), end, &h) || !MT_Number(MT_Member(w, end, "S"), end, &s) || s == 0) {
                return qfalse;
            }
            *v = (double)(int)(h / s * 100);
            break;
        case MT_READ_RACE: {
            int ms;

            if (!MT_Number(MT_Member(p, end, c->a), end, v)) {
                return qfalse;
            }
            ms = (int)*v;
            if (ms == -1) {
                Q_strncpyz(text, "DNF", size);
            } else if (ms / 60000) {
                Com_sprintf(text, size, "%i:%02i.%02i", ms / 60000, (ms / 1000) % 60, (ms % 1000) / 10);
            } else {
                Com_sprintf(text, size, "%02i.%02i", (ms / 1000) % 60, (ms % 1000) / 10);
            }
            return qtrue;
        }
    }
    if (!keepZero && *v == 0) {
        return qfalse;
    }
    Com_sprintf(text, size, "%i", (int)*v);
    return qtrue;
}

static void MT_BuildTable(mtTable_t* t, const mtColumn_t* cols, int numCols, const char* players, const char* end,
                          qboolean keepZero) {
    const char* p;
    int c, r;

    Com_Memset(t, 0, sizeof(*t));
    t->numCols = numCols;
    for (c = 0; c < numCols; c++) {
        t->labels[c] = cols[c].label;
    }
    for (p = JSON_ArrayGetFirstValue(players, end); p && t->rows < MT_MAX_ROWS; p = JSON_ArrayGetNextValue(p, end)) {
        mtRow_t* row = &t->row[t->rows++];
        char name[64], model[MAX_QPATH];

        MT_String(p, end, "NAME", name, sizeof(name));
        MT_StripColors(name, row->name, sizeof(row->name));
        if (MT_Int(p, end, "QUIT")) {
            Q_strcat(row->name, sizeof(row->name), " (Quit)");
        }
        MT_String(p, end, "STEAM_ID", row->steamId, sizeof(row->steamId));
        row->team = MT_Int(p, end, "TEAM");
        MT_String(p, end, "MODEL", model, sizeof(model));
        row->icon = MT_ModelIcon(model, row->team);
        for (c = 0; c < numCols; c++) {
            row->has[c] = MT_Cell(&cols[c], p, end, &row->v[c], row->text[c], sizeof(row->text[c]), keepZero);
        }
    }
    for (c = 0; c < numCols; c++) {
        double sum = 0;
        qboolean text = qfalse;

        for (r = 0; r < t->rows; r++) {
            if (t->row[r].has[c]) {
                sum += t->row[r].v[c];
                if (cols[c].read == MT_READ_RACE && t->row[r].text[c][0]) {
                    text = qtrue;
                }
            }
        }
        t->shown[c] = (qboolean)(sum > 0 || text);
    }
}

static void MT_WeaponColumns(mtColumn_t* cols, mtRead_t read, const char* field) {
    int i;

    for (i = 0; i < MT_NUM_WEAPONS; i++) {
        cols[i].label = mtWeaponLabels[i];
        cols[i].read = read;
        cols[i].a = mtWeaponKeys[i];
        cols[i].b = field;
    }
}

static int MT_Count(const mtColumn_t* cols) {
    int n = 0;

    while (cols[n].label) {
        n++;
    }
    return n;
}

static void MT_LoadWho(mtWho_t* w, const char* who, const char* end) {
    const char* p;
    int i;

    Com_Memset(w, 0, sizeof(*w));
    MT_String(who, end, "NAME", w->name, sizeof(w->name));
    w->team = MT_Int(who, end, "TEAM");
    for (p = JSON_ArrayGetFirstValue(MT_Member(who, end, "POWERUPS"), end); p && w->numPowerups < 8;
         p = JSON_ArrayGetNextValue(p, end)) {
        char name[32];

        JSON_ValueGetString(p, end, name, sizeof(name));
        for (i = 0; i < (int)ARRAY_LEN(mtPowerups); i++) {
            if (!strcmp(mtPowerups[i].name, name)) {
                w->powerups[w->numPowerups++] = mt.powerupIcon[i];
                break;
            }
        }
    }
}

static void MT_Open(int index) {
    mtMatch_t* m = &mt.match[index];
    mtColumn_t cols[MT_MAX_COLS];
    const char* end;
    const char* list;
    const char* p;
    int len, i;

    mt.open = index;
    mt.section = MT_SEC_PLAYERS;
    mt.sortCol = -1;
    mt.scroll = 0;
    Com_Memset(mt.table, 0, sizeof(mt.table));
    mt.numEvents = 0;

    len = MT_ReadFile(va("matches/%s.players.json", m->guid), mtBuffer, sizeof(mtBuffer));
    if (len > 0) {
        end = mtBuffer + len;
        list = MT_Member(mtBuffer, end, "PLYR_STATS");
        if (!Q_stricmp(m->gameType, "RACE")) {
            MT_BuildTable(&mt.table[MT_SEC_PLAYERS], mtRaceCols, MT_Count(mtRaceCols), list, end, qtrue);
        } else {
            MT_BuildTable(&mt.table[MT_SEC_PLAYERS], mtPlayerCols, MT_Count(mtPlayerCols), list, end, qtrue);
        }
        MT_BuildTable(&mt.table[MT_SEC_MEDALS], mtMedalCols, MT_Count(mtMedalCols), list, end, qfalse);
        MT_BuildTable(&mt.table[MT_SEC_ITEMS], mtItemCols, MT_Count(mtItemCols), list, end, qfalse);
        MT_WeaponColumns(cols, MT_READ_WEAPON, "K");
        MT_BuildTable(&mt.table[MT_SEC_KILLS], cols, MT_NUM_WEAPONS, list, end, qfalse);
        MT_WeaponColumns(cols, MT_READ_WEAPON, "D");
        MT_BuildTable(&mt.table[MT_SEC_DEATHS], cols, MT_NUM_WEAPONS, list, end, qfalse);
        MT_WeaponColumns(cols, MT_READ_ACCURACY, NULL);
        MT_BuildTable(&mt.table[MT_SEC_ACCURACY], cols, MT_NUM_WEAPONS, list, end, qfalse);
        MT_WeaponColumns(cols, MT_READ_WEAPON, "DG");
        MT_BuildTable(&mt.table[MT_SEC_DAMAGE], cols, MT_NUM_WEAPONS, list, end, qfalse);
    }

    len = MT_ReadFile(va("matches/%s.events.json", m->guid), mtBuffer, sizeof(mtBuffer));
    if (len > 0) {
        end = mtBuffer + len;
        list = MT_Member(mtBuffer, end, "PLYR_EVENTS");
        if (!list) {
            list = MT_Member(mtBuffer, end, "PLYR_DEATHS");
        }
        for (p = JSON_ArrayGetFirstValue(list, end); p && mt.numEvents < MT_MAX_EVENTS;
             p = JSON_ArrayGetNextValue(p, end)) {
            mtEvent_t* ev = &mt.events[mt.numEvents++];
            const char* killer = MT_Member(p, end, "KILLER");
            char mod[32];

            ev->time = MT_Int(p, end, "TIME");
            MT_String(p, end, "MOD", mod, sizeof(mod));
            ev->mod = -1;
            for (i = 0; i < MT_NUM_MODS; i++) {
                if (!strcmp(mtMods[i].mod, mod)) {
                    ev->mod = i;
                    break;
                }
            }
            ev->killer = (qboolean)(killer && *killer == '{');
            if (ev->killer) {
                MT_LoadWho(&ev->who[0], killer, end);
            }
            MT_LoadWho(&ev->who[1], MT_Member(p, end, "VICTIM"), end);
        }
    }

    mt.numMedia = 0;
    len = MT_ReadFile("matches/media.txt", mtBuffer, sizeof(mtBuffer));
    for (p = len > 0 ? mtBuffer : NULL; p && *p && mt.numMedia < MT_MAX_MEDIA;) {
        char* eol = strchr((char*)p, '\n');
        char* space;

        if (eol) {
            *eol = '\0';
        }
        space = strchr((char*)p, ' ');
        if (space && !Q_stricmpn(space + 1, m->guid, (int)strlen(m->guid))) {
            *space = '\0';
            Q_strncpyz(mt.media[mt.numMedia++], p, MAX_QPATH);
        }
        p = eol ? eol + 1 : NULL;
    }
    if (mt.numMedia) {
        return;
    }
    if (MT_Exists(va("screenshots/%s.jpg", m->guid))) {
        Com_sprintf(mt.media[mt.numMedia++], MAX_QPATH, "%s.jpg", m->guid);
    }
    if (MT_Exists(va("demos/%s.dm_91", m->guid))) {
        Com_sprintf(mt.media[mt.numMedia++], MAX_QPATH, "%s.dm_91", m->guid);
    }
}

static void MT_Shadowed(float x, float baseline, int font, float size, const float* color, const char* text,
                        qboolean bold, const smRect_t* clip) {
    if (!SM_TextInside(baseline, size, clip)) {
        return;
    }
    SM_Text(x + 2, baseline + 2, font, size, mtBlack, text, -1, bold);
    SM_Text(x, baseline, font, size, color, text, -1, bold);
}

static void MT_Line(float x, float lineTop, float lineH, int font, float size, const float* color, const char* text,
                    qboolean bold, const smRect_t* clip) {
    float baseline = SM_Baseline(font, size, lineTop, lineH);

    if (SM_TextInside(baseline, size, clip)) {
        SM_Text(x, baseline, font, size, color, text, -1, bold);
    }
}

static void MT_Date(double ms, char* out, int size) {
    static const char* const months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    time_t t = (time_t)(ms / 1000);
    struct tm* lt = localtime(&t);

    if (!lt) {
        out[0] = '\0';
        return;
    }
    Com_sprintf(out, size, "%s %i %02i, %02i:%02i", months[lt->tm_mon], lt->tm_mday, lt->tm_year % 100, lt->tm_hour,
                lt->tm_min);
}

static void MT_Duration(int seconds, char* out, int size) {
    if (seconds / 60) {
        Com_sprintf(out, size, "%imin %isec", seconds / 60, seconds % 60);
    } else {
        Com_sprintf(out, size, "%isec", seconds);
    }
}

static void MT_DrawWidget(const mtMatch_t* m, float x, float y, const smRect_t* clip) {
    char line[256], date[64], length[32], rank[16];
    float sx = x + 270 + 20;
    float sw, w;

    SM_Pic(x, y, 270, 150, m->levelshot, clip);
    SM_Fill(x, y, 270, 25.6f, mtGametype, clip);
    {
        const char* title = MT_GametypeTitle(m->gameType);

        MT_Shadowed(x + 270 - 5 - SM_AdvanceBold(FONT_DEFAULT, 16, title, -1, qtrue),
                    SM_Baseline(FONT_DEFAULT, 16, y, 25.6f), FONT_DEFAULT, 16, mtWhite, title, qtrue, clip);
    }
    if (m->personal) {
        MT_Ordinal(m->rank, rank, sizeof(rank));
        MT_Shadowed(x + (270 - SM_Advance(FONT_DEFAULT, 32, rank, -1)) * 0.5f, SM_Baseline(FONT_DEFAULT, 32, y + 40.6f, 51.2f),
                    FONT_DEFAULT, 32, mtYellow, rank, qfalse, clip);
    }
    {
        const char* result = !strcmp(m->status, "WIN") ? "Win" : !strcmp(m->status, "LOSE") ? "Loss" : "Quit";

        MT_Shadowed(x + (270 - SM_Advance(FONT_DEFAULT, 18.667f, result, -1)) * 0.5f,
                    SM_Baseline(FONT_DEFAULT, 18.667f, y + 91.8f, 29.9f), FONT_DEFAULT, 18.667f, mtLight, result, qfalse,
                    clip);
    }

    SM_SetSpacing(SM_SPACING_H1);
    MT_Line(sx, y, 30, FONT_DEFAULT, 24, mtYellow, MT_MapName(m->map), qfalse, clip);
    sw = SM_Advance(FONT_DEFAULT, 24, MT_MapName(m->map), -1);
    SM_SetSpacing(0);
    MT_Date(m->timePlayed, date, sizeof(date));
    MT_Duration(m->gameLength, length, sizeof(length));
    Com_sprintf(line, sizeof(line), "%s (%s)", date, length);
    MT_Line(sx, y + 30, 25.6f, FONT_SANS, 16, mtLight, line, qfalse, clip);
    w = SM_Advance(FONT_SANS, 16, line, -1);
    sw = w > sw ? w : sw;
    MT_StripColors(m->server, line, sizeof(line));
    MT_Line(sx, y + 55.6f, 25.6f, FONT_SANS, 16, mtLight, line, qfalse, clip);
    w = SM_Advance(FONT_SANS, 16, line, -1);
    sw = w > sw ? w : sw;
    if (SM_TextInside(SM_Baseline(FONT_SANS, 16, y + 81.2f, 25.6f), 16, clip)) {
        SM_TextRaw(sx, SM_Baseline(FONT_SANS, 16, y + 81.2f, 25.6f), FONT_SANS, 16, mtLight, m->exitMsg, -1, qfalse);
    }
    w = SM_AdvanceRaw(FONT_SANS, 16, m->exitMsg, -1, qfalse);
    sw = w > sw ? w : sw;

    if (m->personal) {
        float px = sx + sw + 20;
        float mx = px, my = y + 93.5f;
        int i;

        SM_SetSpacing(SM_SPACING_H1);
        MT_Line(px, y, 30, FONT_DEFAULT, 24, mtYellow, "Personal", qfalse, clip);
        SM_SetSpacing(0);
        if (!Q_stricmp(m->gameType, "RACE")) {
            char best[32];

            SM_SetSpacing(SM_SPACING_H1);
            MT_Line(px, y + 30, 20.8f, FONT_SANS, 16, mtGrey, "Best Time", qtrue, clip);
            SM_SetSpacing(0);
            if (m->score == -1) {
                Q_strncpyz(best, "DNF", sizeof(best));
            } else if (m->score / 60000) {
                Com_sprintf(best, sizeof(best), "%i:%02i.%02i", m->score / 60000, (m->score / 1000) % 60, (m->score % 1000) / 10);
            } else {
                Com_sprintf(best, sizeof(best), "%02i.%02i", (m->score / 1000) % 60, (m->score % 1000) / 10);
            }
            MT_Line(px, y + 53.8f, 36, FONT_SANS, 26.667f, mtLight, best, qtrue, clip);
            return;
        }
        SM_SetSpacing(SM_SPACING_H1);
        MT_Line(px, y + 30, 20.8f, FONT_SANS, 16, mtGrey, "Score", qtrue, clip);
        SM_SetSpacing(0);
        Com_sprintf(line, sizeof(line), "%i", m->score);
        MT_Line(px, y + 53.8f, 36, FONT_SANS, 26.667f, mtLight, line, qtrue, clip);
        SM_SetSpacing(SM_SPACING_H1);
        MT_Line(px + 75, y + 30, 20.8f, FONT_SANS, 16, mtGrey, "KDR", qtrue, clip);
        SM_SetSpacing(0);
        Com_sprintf(line, sizeof(line), "%.2f", m->deaths ? (double)m->kills / m->deaths : (double)m->kills);
        MT_Line(px + 75, y + 53.8f, 36, FONT_SANS, 26.667f, mtLight, line, qtrue, clip);

        for (i = 0; i < MT_NUM_MEDALS; i++) {
            char count[16];
            float cw;

            if (!m->medals[i]) {
                continue;
            }
            Com_sprintf(count, sizeof(count), "%i", m->medals[i]);
            cw = 40 + 11.7f + SM_Advance(FONT_SANS, 18.667f, count, -1);
            if (mx > px && mx + cw > px + 550) {
                mx = px;
                my += 47.9f;
            }
            SM_Pic(mx, my, 40, 40, mt.medal[i], clip);
            if (SM_TextInside(my + 40, 18.667f, clip)) {
                SM_Icon(mx + 40, my + 40, 14.933f, mtMedalGrey, 0xf00d);
                SM_Text(mx + 51.7f, my + 40, FONT_SANS, 18.667f, mtMedalGrey, count, -1, qfalse);
            }
            mx += cw;
        }
    }
}

static void MT_DrawList(float left, float top, float width, const smRect_t* clip) {
    float y;
    int i;

    SM_SetSpacing(SM_SPACING_H1);
    MT_Line(left + 11.4f, top + 8, 38.4f, FONT_DEFAULT, 32, smText, "My Matches", qfalse, clip);
    SM_SetSpacing(0);
    y = top + 54.4f;
    for (i = 0; i < mt.count; i++) {
        SM_Fill(left + 5, y, width - 10, 150, (i & 1) ? mtEven : mtOdd, clip);
        MT_DrawWidget(&mt.match[i], left + 5, y, clip);
        SM_Hit(left + 5, y, width - 10, 150, MT_HIT_MATCH, i, clip);
        y += 160;
    }
    mt.content = y - top;
}

static float MT_RowButton(float x, float y, const char* label, int glyph, int type, int index, qboolean chosen,
                          const smRect_t* clip) {
    char upper[32];
    float w;
    qboolean over = SM_Over(type, index);
    float baseline = SM_Baseline(FONT_SANS, 11, y, 30);

    if (glyph) {
        w = SM_IconWidth(18.667f, glyph) + 40;
    } else {
        Q_strncpyz(upper, label, sizeof(upper));
        Q_strupr(upper);
        w = SM_SpacedWidth(FONT_SANS, 11, upper, 1.0f) + 40;
    }
    if (chosen) {
        SM_Fill(x, y, w, 30, mtYellow, clip);
    }
    if (SM_TextInside(baseline, 11, clip)) {
        const float* color = chosen ? mtBlack : over ? mtYellow : smText;

        if (glyph) {
            SM_Icon(x + 20, y + 15 + 18.667f * SM_AWESOME_BASE, 18.667f, color, glyph);
        } else {
            SM_SpacedText(x + 20, baseline, FONT_SANS, 11, color, upper, 1.0f, qtrue);
        }
    }
    SM_Hit(x, y, w, 30, type, index, clip);
    return w;
}

static int MT_Compare(int a, int b) {
    const mtTable_t* t = &mt.table[mt.section];
    const mtRow_t* ra = &t->row[a];
    const mtRow_t* rb = &t->row[b];
    int c = mt.sortCol;
    int r;

    if (c == MT_MAX_COLS) {
        r = Q_stricmp(ra->name, rb->name);
    } else if (ra->has[c] != rb->has[c]) {
        return ra->has[c] ? -1 : 1;
    } else {
        r = ra->v[c] < rb->v[c] ? -1 : ra->v[c] > rb->v[c] ? 1 : 0;
    }
    return mt.sortDesc ? -r : r;
}

static void MT_Sort(void) {
    const mtTable_t* t;
    int i, j;

    if (mt.section >= MT_SEC_EVENTS) {
        return;
    }
    t = &mt.table[mt.section];
    for (i = 0; i < t->rows; i++) {
        mt.order[i] = i;
    }
    if (mt.sortCol < 0) {
        return;
    }
    for (i = 1; i < t->rows; i++) {
        int v = mt.order[i];

        for (j = i - 1; j >= 0 && MT_Compare(mt.order[j], v) > 0; j--) {
            mt.order[j + 1] = mt.order[j];
        }
        mt.order[j + 1] = v;
    }
}

static float MT_DrawTable(float x, float y, const smRect_t* clip) {
    const mtTable_t* t = &mt.table[mt.section];
    float colW[MT_MAX_COLS];
    float nameW = 300;
    float cx, ry;
    int c, r;

    for (r = 0; r < t->rows; r++) {
        float w = 12 + 6 + 32 + SM_Advance(FONT_SANS, 15, " ", -1) + SM_Advance(FONT_SANS, 15, t->row[r].name, -1);

        nameW = w > nameW ? w : nameW;
    }
    for (c = 0; c < t->numCols; c++) {
        float w = SM_AdvanceBold(FONT_SANS, 15, t->labels[c], -1, qtrue) + 12;

        for (r = 0; r < t->rows; r++) {
            if (t->row[r].has[c]) {
                float cw = SM_Advance(FONT_SANS, 15, t->row[r].text[c], -1) + 12;

                w = cw > w ? cw : w;
            }
        }
        colW[c] = w < 50 ? 50 : w;
    }

    y += 4;
    cx = x;
    SM_Fill(cx, y, nameW, 24, mtHead, clip);
    MT_Line(cx + (nameW - SM_AdvanceBold(FONT_SANS, 15, "Name", -1, qtrue)) * 0.5f, y, 24, FONT_SANS, 15, mtYellow, "Name",
            qtrue, clip);
    SM_Hit(cx, y, nameW, 24, MT_HIT_SORT, MT_MAX_COLS, clip);
    cx += nameW;
    for (c = 0; c < t->numCols; c++) {
        if (!t->shown[c]) {
            continue;
        }
        SM_Fill(cx, y, colW[c], 24, mtHead, clip);
        MT_Line(cx + (colW[c] - SM_AdvanceBold(FONT_SANS, 15, t->labels[c], -1, qtrue)) * 0.5f, y, 24, FONT_SANS, 15,
                mtYellow, t->labels[c], qtrue, clip);
        SM_Hit(cx, y, colW[c], 24, MT_HIT_SORT, c, clip);
        cx += colW[c];
    }

    ry = y + 24 + 4;
    for (r = 0; r < t->rows; r++) {
        const mtRow_t* row = &t->row[mt.order[r]];
        qboolean link = (qboolean)(row->steamId[0] && strcmp(row->steamId, "0"));
        float lw;

        SM_Fill(x, ry, cx - x, 44, (r & 1) ? mtEven : mtOdd, clip);
        SM_Fill(x, ry, 6, 44, row->team == 1 ? mtRed : row->team == 2 ? mtBlue : mtNoTeam, clip);
        SM_Pic(x + 12, ry + 6, 32, 32, row->icon, clip);
        MT_Line(x + 12 + 32 + SM_Advance(FONT_SANS, 15, " ", -1), ry + 10, 20, FONT_SANS, 15, link ? mtLink : smText,
                row->name, qfalse, clip);
        if (link) {
            lw = 32 + SM_Advance(FONT_SANS, 15, " ", -1) + SM_Advance(FONT_SANS, 15, row->name, -1);
            SM_Hit(x + 12, ry + 6, lw, 32, MT_HIT_PROFILE, mt.order[r], clip);
        }
        cx = x + nameW;
        for (c = 0; c < t->numCols; c++) {
            if (!t->shown[c]) {
                continue;
            }
            if (row->has[c]) {
                MT_Line(cx + 6, ry + 10, 20, FONT_SANS, 15, smText, row->text[c], qfalse, clip);
            }
            cx += colW[c];
        }
        ry += 48;
    }
    return ry - y + 4 + 15;
}

static float MT_DrawEvents(float x, float y, float width, const smRect_t* clip) {
    float space = SM_Advance(FONT_SANS, 15, " ", -1);
    int i, k;

    for (i = 0; i < mt.numEvents; i++) {
        const mtEvent_t* ev = &mt.events[i];
        float ly = y + i * 37.9f;
        float baseline = SM_Baseline(FONT_SANS, 15, ly + 6.5f, 20);
        char time[32];
        float lx;

        if (ly + 34.9f < clip->y || ly > clip->y + clip->h) {
            continue;
        }
        SM_Fill(x, ly, width, 34.9f, mtBlack80, clip);
        Com_sprintf(time, sizeof(time), "%i:%02i:", ev->time / 60, ev->time % 60);
        if (!SM_TextInside(baseline, 15, clip)) {
            continue;
        }
        SM_Text(x + 4.5f + 50 - SM_Advance(FONT_SANS, 15, time, -1), baseline, FONT_SANS, 15, mtYellow, time, -1, qfalse);
        lx = x + 4.5f + 50 + space;
        if (ev->killer) {
            const mtWho_t* w = &ev->who[0];

            for (k = 0; k < w->numPowerups; k++) {
                SM_Pic(lx, ly + 6.4f, 24, 24, w->powerups[k], clip);
                lx += 24;
            }
            lx += space;
            SM_TextRaw(lx, baseline, FONT_SANS, 15, w->team == 1 ? mtRed : w->team == 2 ? mtBlue : smText, w->name, -1,
                       qfalse);
            lx += SM_AdvanceRaw(FONT_SANS, 15, w->name, -1, qfalse);
        } else {
            lx += space;
        }
        lx += space;
        if (ev->mod >= 0) {
            const mtMod_t* mod = &mtMods[ev->mod];

            SM_Pic(lx, ly + 6.4f, 24, 24, mt.modIcon[ev->mod][0], clip);
            lx += 24;
            if (mod->img2) {
                SM_Pic(lx, ly + 6.4f, 24, 24, mt.modIcon[ev->mod][1], clip);
                lx += 24;
            }
            if (mod->text) {
                SM_Text(lx, baseline, SM_ItalicFace(), 15, mtGrey, mod->text, -1, qfalse);
                lx += SM_Advance(SM_ItalicFace(), 15, mod->text, -1);
            }
        }
        lx += space;
        {
            const mtWho_t* w = &ev->who[1];

            for (k = 0; k < w->numPowerups; k++) {
                SM_Pic(lx, ly + 6.4f, 24, 24, w->powerups[k], clip);
                lx += 24;
            }
            lx += space;
            SM_TextRaw(lx, baseline, FONT_SANS, 15, w->team == 1 ? mtRed : w->team == 2 ? mtBlue : smText, w->name, -1,
                       qfalse);
        }
    }
    return mt.numEvents * 37.9f;
}

static void MT_DrawDetails(float left, float top, float width, const smRect_t* clip) {
    float y = top;
    float bx;
    int i;

    SM_Fill(left, y, width, 160, mtBlack80, clip);
    MT_DrawWidget(&mt.match[mt.open], left, y, clip);
    y += 160;

    SM_Fill(left, y, width, 30, mtBlack80, clip);
    bx = left + MT_RowButton(left, y, NULL, 0xf100, MT_HIT_BACK, 0, qfalse, clip);
    if (mt.numMedia < 4) {
        for (i = 0; i < mt.numMedia; i++) {
            bx += MT_RowButton(bx, y, NULL, strstr(mt.media[i], ".dm_91") ? 0xf03d : 0xf03e, MT_HIT_MEDIA, i, qfalse, clip);
        }
    }
    for (i = 0; i < MT_NUM_SECTIONS; i++) {
        bx += MT_RowButton(bx, y, mtSectionNames[i], 0, MT_HIT_SECTION, i, (qboolean)(mt.section == i), clip);
    }
    y += 30 + 15;

    {
        float h = mt.section == MT_SEC_EVENTS ? MT_DrawEvents(left + 5, y + 5, width - 10, clip) + 15
                                              : MT_DrawTable(left + 5, y + 5, clip);
        SM_Fill(left, y, width, h + 10, mtBlack80, clip);
        if (mt.section == MT_SEC_EVENTS) {
            MT_DrawEvents(left + 5, y + 5, width - 10, clip);
        } else {
            MT_DrawTable(left + 5, y + 5, clip);
        }
        y += h + 10;
    }
    mt.content = y - top;
}

static void MT_Load(void) {
    int i;

    if (mt.loaded) {
        return;
    }
    mt.loaded = qtrue;
    for (i = 0; i < MT_NUM_MEDALS; i++) {
        mt.medal[i] = trap_R_RegisterShaderNoMip(va("webui/img/medals/%s.png", mtMedals[i].img));
    }
    for (i = 0; i < MT_NUM_MODS; i++) {
        mt.modIcon[i][0] = trap_R_RegisterShaderNoMip(va("webui/img/stat_icons/%s.png", mtMods[i].img));
        if (mtMods[i].img2) {
            mt.modIcon[i][1] = trap_R_RegisterShaderNoMip(va("webui/img/stat_icons/%s.png", mtMods[i].img2));
        }
    }
    for (i = 0; i < (int)ARRAY_LEN(mtPowerups); i++) {
        mt.powerupIcon[i] = trap_R_RegisterShaderNoMip(va("webui/img/stat_icons/%s.png", mtPowerups[i].img));
    }
}

static char mtPending[64];

void UI_Matches_ShowMatch(const char* guid) {
    Q_strncpyz(mtPending, guid, sizeof(mtPending));
    Menus_CloseAll();
    Menus_ActivateByName("matches");
}

void UI_Matches_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;
    int i;

    MT_Load();
    MT_LoadList();
    mt.open = -1;
    mt.scroll = 0;
    mt.shot[0] = '\0';
    if (mtPending[0]) {
        for (i = 0; i < mt.count && Q_stricmp(mt.match[i].guid, mtPending); i++) {
        }
        if (i == mt.count && mt.count < MT_MAX_MATCHES) {
            MT_LoadMatch(&mt.match[i], mtPending);
            if (mt.match[i].guid[0]) {
                mt.count++;
            }
        }
        if (i < mt.count) {
            MT_Open(i);
        }
        mtPending[0] = '\0';
    }

    menu = Menus_FindByName("matches");
    item = menu ? Menu_FindItemByName(menu, "matches") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Matches_Paint(void) {
    float left = UI_Home_Left();
    float width = HM_CONTAINER_W;
    float view = smBottom - 30 - HM_NAV_H;
    float top;
    smRect_t clip;

    SM_BeginFrame();
    MT_Load();
    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_MATCHES);

    if (mt.scroll > mt.content - view) {
        mt.scroll = mt.content - view;
    }
    if (mt.scroll < 0) {
        mt.scroll = 0;
    }
    clip = SM_R(left, HM_NAV_H - 8, width, view + 8);
    top = HM_NAV_H - mt.scroll;
    if (mt.open < 0) {
        SM_Fill(left, HM_NAV_H, width, view, mtBlack80, NULL);
        MT_DrawList(left, top, width, &clip);
    } else {
        MT_Sort();
        MT_DrawDetails(left, top, width, &clip);
    }

    if (mt.shot[0]) {
        float sw = smVW * 0.9f * 0.95f;
        float sh = sw * 9.0f / 16.0f;
        float ty = -smTop / smScale;

        SM_Fill(-1, ty, smVW + 2, smBottom - ty + 1, mtBlack80, NULL);
        if (mt.shotImage) {
            SM_Pic((smVW - sw) * 0.5f, ty + (smBottom - ty - sh) * 0.5f, sw, sh, mt.shotImage, NULL);
        }
        SM_Hit(0, ty, smVW, smBottom - ty, MT_HIT_SHOT, 0, NULL);
    }

    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Matches_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (mt.shot[0]) {
        if (key == K_ESCAPE || key == K_MOUSE1) {
            mt.shot[0] = '\0';
            return qtrue;
        }
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        mt.scroll += key == K_MWHEELUP ? -60.0f : 60.0f;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (!hit) {
        return qtrue;
    }
    switch (hit->type) {
        case MT_HIT_MATCH:
            MT_Open(hit->index);
            break;
        case MT_HIT_BACK:
            mt.open = -1;
            mt.scroll = 0;
            break;
        case MT_HIT_SECTION:
            mt.section = hit->index;
            mt.sortCol = -1;
            break;
        case MT_HIT_SORT:
            if (mt.sortCol == hit->index) {
                mt.sortDesc = !mt.sortDesc;
            } else {
                mt.sortCol = hit->index;
                mt.sortDesc = 0;
            }
            break;
        case MT_HIT_PROFILE:
            trap_SteamShowProfile(mt.table[mt.section].row[hit->index].steamId);
            break;
        case MT_HIT_MEDIA: {
            char name[MAX_QPATH];
            char* dot;

            Q_strncpyz(name, mt.media[hit->index], sizeof(name));
            dot = strrchr(name, '.');
            if (dot && !Q_stricmp(dot, ".dm_91")) {
                *dot = '\0';
                trap_Cmd_ExecuteText(EXEC_APPEND, va("demo \"%s\"\n", name));
            } else {
                Q_strncpyz(mt.shot, name, sizeof(mt.shot));
                mt.shotImage = trap_R_RegisterShaderNoMip(va("screenshots/%s", name));
            }
            break;
        }
        default:
            break;
    }
    return qtrue;
}
