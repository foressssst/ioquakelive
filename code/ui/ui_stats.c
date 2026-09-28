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
// ui_stats.c -- [QL] Career Stats, and the wins and KDR of the profile card
//
// Retail's page is web.pak's stats.js with stats_basic.js and stats_medals.js, reached
// from Statistics > Career Stats and, for a friend, from the friend list: the player's
// avatar and Steam name, a row with Statistics and Medals, and under it one or the other.
// Everything comes from Steam. The page asks for the player's stats each time it opens
// or changes tab (RequestUserStats), shows what it had meanwhile, and adds them up as
// utils.js's calculateSteamStats does: every kill_ is a kill, every mod_ a death, and
// every hits_ and shots_ goes to the accuracy.
//
// The profile card, in the bar of every page and on the main menu, prints the player's
// own wins and KDR from the same stats once Steam has answered.
//

#include "ui_web.h"
#include "../qcommon/q_steamstats.h"

typedef enum {
    ST_HIT_NONE,
    ST_HIT_TAB
} stHitType_t;

// stats_basic.js's weaponInfo: the row's name, and the stats and picture it goes by
typedef struct {
    const char* id;
    const char* name;
} stWeapon_t;

static const stWeapon_t stWeapons[] = {
    {"gauntlet", "Gauntlet"},       {"machinegun", "Machinegun"}, {"shotgun", "Shotgun"},
    {"grenade", "Grenade Launcher"}, {"rocket", "Rocket Launcher"}, {"lightning", "Lightning Gun"},
    {"railgun", "Railgun"},         {"plasma", "Plasma Gun"},     {"hmg", "HMG"},
    {"bfg", "BFG"},                 {"nailgun", "Nailgun"},       {"proxmine", "Proximity Mines"},
    {"chaingun", "Chaingun"},
};
#define ST_NUM_WEAPONS ((int)ARRAY_LEN(stWeapons))

// its modInfo: the deaths that are no weapon's; an optional one only shows when it happened
typedef struct {
    const char* id;
    qboolean optional;
    const char* name;
} stMod_t;

static const stMod_t stMods[] = {
    {"mod_water", qfalse, "Drowning"},   {"mod_slime", qfalse, "Slime"},     {"mod_lava", qfalse, "Lava"},
    {"mod_crush", qfalse, "Crushed"},    {"mod_telefrag", qfalse, "Telefrag"}, {"mod_hurt", qfalse, "Environment"},
    {"mod_kamikaze", qfalse, "Kamikaze"}, {"mod_falling", qfalse, "Falling"}, {"mod_suicide", qfalse, "Suicide"},
    {"mod_juiced", qtrue, "Juiced"},     {"mod_other", qtrue, "Other"},
};
#define ST_NUM_MODS ((int)ARRAY_LEN(stMods))

// stats_medals.js's medalInfo
typedef struct {
    const char* id;
    const char* name;
    const char* desc;
} stMedal_t;

static const stMedal_t stMedals[] = {
    {"medal_firstfrag", "First Frag", "Score the first frag in a match."},
    {"medal_gauntlet", "Humiliation", "Frag an enemy with the Gauntlet."},
    {"medal_excellent", "Excellent", "Score two frags in two seconds."},
    {"medal_revenge", "Revenge", "Frag an enemy that has fragged you three times."},
    {"medal_combokill", "Combo Kill", "Frag an enemy by damaging them with a weapon and finish them with the Railgun."},
    {"medal_midair", "Midair", "Frag an enemy with a projectile weapon while they are airborne."},
    {"medal_perforated", "Perforated", "Telefrag an enemy by invading their space through a teleporter."},
    {"medal_rampage", "Rampage", "Gib three enemies within three seconds of each frag."},
    {"medal_impressive", "Impressive", "Hit two shots in a row with the Railgun."},
    {"medal_capture", "Capture", "Help your team out by capturing a flag, Domination points, or Harvester skulls."},
    {"medal_assist", "Assist", "Be the key teammate in another player's capture play."},
    {"medal_defense", "Defense", "Frag an enemy while defending your base."},
    {"medal_headshot", "Headshot", "Frag an enemy with a Railgun shot to the head."},
    {"medal_quadgod", "Quad God", "Frag 10 enemies on a single Quad Damage spree."},
    {"medal_perfect", "Perfect", "Win a round without taking any damage."},
    {"medal_accuracy", "Accuracy", "Finish a round with over 50% accuracy."},
};
#define ST_NUM_MEDALS ((int)ARRAY_LEN(stMedals))

// calculateSteamStats
typedef struct {
    int kills, deaths, shots, hits;
    float kdr;
} stSummary_t;

// in the page's pixels, from its css (.stats and skeleton's h1 and .button)
#define ST_TITLE_H 88.4f     // h1.title: 0.25em over and under a line the 64px avatar makes
#define ST_AVATAR 64.0f
#define ST_ROW_H 30.0f       // nav.button-row
#define ST_ROW_GAP 15.0f     // its margin-bottom, 1em
#define ST_PAD 5.0f          // .stats-content
#define ST_CARD_W 169.0f     // .stats-card
#define ST_CARD_H 125.0f
#define ST_CARD_GAP 7.5f     // margin-right: 0.5em
#define ST_TH_H 24.0f
#define ST_TD_H 38.0f        // a 32px icon on the line and the line's descent
#define ST_CELL_PAD 15.0f
#define ST_MOD_W 200.0f
#define ST_MOD_H 45.0f
#define ST_MOD_GAP 8.0f
#define ST_MEDAL_W 250.0f
#define ST_MEDAL_H 125.0f
#define ST_MEDAL_GAP 20.0f
#define ST_BOTTOM 30.0f      // .body stops 30px over the page's bottom

static const vec4_t stBlack80 = {0.0f, 0.0f, 0.0f, 0.8f};
static const vec4_t stCard = SM_RGB(44, 41, 41);
static const vec4_t stYellow = SM_RGB(247, 209, 0);
static const vec4_t stBlack = {0.0f, 0.0f, 0.0f, 1.0f};

static struct {
    char steamId[24];  // whose page it is, "" for one's own
    int tab;           // 0 Statistics, 1 Medals
    uiUserStats_t data;
    qboolean loaded;
    qhandle_t weapon[ST_NUM_WEAPONS], mod[ST_NUM_MODS], medal[ST_NUM_MEDALS];
    float scroll;
    float content;  // how tall the page is, to know how far it scrolls
} st;

// whose page opens next: "" is one's own
static char stPending[24];

// one's own, for the card
static uiUserStats_t stOwn;
static int stCardPage = -2;

/*
===============
What the page reads of the stats
===============
*/
static qboolean ST_Answered(const uiUserStats_t* d) {
    return (qboolean)(d->state == UI_DETAILS_DONE || d->state == UI_DETAILS_FAILED);
}

// a stat's place in the table, -1 when there is none of that name
static int ST_Index(const char* name) {
    int i;

    for (i = 0; i < STEAM_NUM_STATS && i < UI_STEAM_NUM_STATS; i++) {
        if (!strcmp(steamStatNames[i], name)) {
            return i;
        }
    }
    return -1;
}

// whether the page has the stat: not until Steam answers, and never one the table lacks
static qboolean ST_Has(const uiUserStats_t* d, const char* name, int* value) {
    int i = ST_Index(name);

    if (i < 0 || !ST_Answered(d)) {
        return qfalse;
    }
    *value = d->stats[i];
    return qtrue;
}

// a stat as the page prints it, nothing when it lacks it
static void ST_Print(const uiUserStats_t* d, const char* name, char* out, int size) {
    int v;

    if (ST_Has(d, name, &v)) {
        Com_sprintf(out, size, "%i", v);
    } else {
        out[0] = '\0';
    }
}

static void ST_Summarize(const uiUserStats_t* d, stSummary_t* s) {
    int i;

    Com_Memset(s, 0, sizeof(*s));
    if (!ST_Answered(d)) {
        return;
    }
    for (i = 0; i < STEAM_NUM_STATS && i < UI_STEAM_NUM_STATS; i++) {
        const char* name = steamStatNames[i];

        if (!Q_strncmp(name, "hits_", 5)) {
            s->hits += d->stats[i];
        }
        if (!Q_strncmp(name, "shots_", 6)) {
            s->shots += d->stats[i];
        }
        if (!Q_strncmp(name, "kill_", 5)) {
            s->kills += d->stats[i];
        }
        if (!Q_strncmp(name, "mod_", 4)) {
            s->deaths += d->stats[i];
        }
    }
    s->kdr = s->deaths ? (float)s->kills / s->deaths : 0;
}

// renderWeapon's accuracy: hundredths cut, not rounded, and JavaScript's way of printing them
static void ST_Accuracy(const uiUserStats_t* d, const char* id, char* out, int size) {
    int hits, shots, len;

    out[0] = '\0';
    if (!ST_Has(d, va("hits_%s", id), &hits) || !ST_Has(d, va("shots_%s", id), &shots)) {
        return;  // undefined over undefined is NaN
    }
    if (!shots) {
        if (hits) {
            Q_strncpyz(out, "NaN%", size);  // parseInt of Infinity
        }
        return;
    }
    Com_sprintf(out, size, "%.2f", (int)((double)hits / shots * 10000.0) / 100.0);
    len = strlen(out);
    while (len > 0 && out[len - 1] == '0') {
        out[--len] = '\0';
    }
    if (len > 0 && out[len - 1] == '.') {
        out[--len] = '\0';
    }
    Q_strcat(out, size, "%");
}

// utils.js's stripColors: each caret and the digit after it, and then any caret left
static void ST_StripColors(const char* in, char* out, int size) {
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

/*
===============
The profile card's line
===============
*/

// the card asks again each time it comes up on a page other than the one it was on
void UI_Stats_CardFor(int page) {
    if (page != stCardPage) {
        stCardPage = page;
        trap_SteamUserStats("", &stOwn, qtrue);
    }
}

// profile_card.js's details: "Wins: <wins> KDR: <kdr>", once Steam has answered
void UI_Stats_DrawCardLine(float x, float lineTop, float width) {
    stSummary_t s;
    char line[64];
    int wins = 0;

    trap_SteamUserStats("", &stOwn, qfalse);
    if (!ST_Answered(&stOwn)) {
        return;
    }
    ST_Summarize(&stOwn, &s);
    ST_Has(&stOwn, "wins", &wins);
    Com_sprintf(line, sizeof(line), "Wins: %i KDR: %.2f", wins, s.kdr);
    SM_Text(x, SM_Baseline(FONT_SANS, 15.0f, lineTop, 24.0f), FONT_SANS, 15.0f, smText, line,
            SM_Fit(FONT_SANS, 15.0f, line, width), qfalse);
}

/*
===============
The page
===============
*/
static void ST_Load(void) {
    int i;

    if (st.loaded) {
        return;
    }
    st.loaded = qtrue;
    for (i = 0; i < ST_NUM_WEAPONS; i++) {
        st.weapon[i] = trap_R_RegisterShaderNoMip(va("webui/img/weapons/%s.png", stWeapons[i].id));
    }
    for (i = 0; i < ST_NUM_MODS; i++) {
        st.mod[i] = trap_R_RegisterShaderNoMip(va("webui/img/mod/%s.png", stMods[i].id));
    }
    for (i = 0; i < ST_NUM_MEDALS; i++) {
        st.medal[i] = trap_R_RegisterShaderNoMip(va("webui/img/medals/%s.png", stMedals[i].id));
    }
}

// text with css's 2px 2px black shadow
static void ST_Shadowed(float x, float baseline, int font, float size, const float* color, const char* text,
                        qboolean bold) {
    SM_Text(x + 2, baseline + 2, font, size, stBlack, text, -1, bold);
    SM_Text(x, baseline, font, size, color, text, -1, bold);
}

// one of the button row's: transparent and borderless, yellow when it is the one showing
static float ST_Tab(float x, float y, const char* label, int tab, const smRect_t* clip) {
    char upper[32];
    float w;
    qboolean chosen = (qboolean)(st.tab == tab);
    qboolean over = SM_Over(ST_HIT_TAB, tab);

    Q_strncpyz(upper, label, sizeof(upper));
    Q_strupr(upper);
    w = SM_SpacedWidth(FONT_SANS, 11, upper, 1.0f) + 40;
    if (chosen) {
        SM_Fill(x, y, w, ST_ROW_H, stYellow, clip);
    }
    if (SM_TextInside(SM_Baseline(FONT_SANS, 11, y, ST_ROW_H), 11, clip)) {
        SM_SpacedText(x + 20, SM_Baseline(FONT_SANS, 11, y, ST_ROW_H), FONT_SANS, 11,
                      chosen ? stBlack : over ? stYellow : smText, upper, 1.0f, qtrue);
    }
    SM_Hit(x, y, w, ST_ROW_H, ST_HIT_TAB, tab, clip);
    return w;
}

// a column's width in the weapons table: the widest of its cells and its padding
static float ST_ColumnWidth(int col, const char* head, char cells[][24], int count) {
    float w = SM_AdvanceBold(FONT_SANS, 15, head, -1, qtrue);
    int i;

    for (i = 0; i < count; i++) {
        float cw = SM_Advance(FONT_SANS, 15, cells[i], -1);

        if (cw > w) {
            w = cw;
        }
    }
    return w + (col == 0 ? 0 : ST_CELL_PAD) + (col == 6 ? 0 : ST_CELL_PAD);
}

static float ST_DrawBasic(float x0, float y0, float innerW, const smRect_t* clip) {
    static const char* const cardTitles[5] = {"Kills", "Deaths", "KDR", "Record", "Played"};
    static const char* const heads[7] = {"", "", "Kills", "Deaths", "Hits", "Shots", "Accuracy"};
    char cells[7][ST_NUM_WEAPONS][24];
    char values[5][32];
    float colW[7];
    stSummary_t s;
    float y, tableW, bx, by;
    int i, c;

    ST_Summarize(&st.data, &s);

    // the five cards: what the page has, and nothing of the record until Steam answers
    Com_sprintf(values[0], sizeof(values[0]), "%i", s.kills);
    Com_sprintf(values[1], sizeof(values[1]), "%i", s.deaths);
    Com_sprintf(values[2], sizeof(values[2]), "%.2f", s.kdr);
    {
        char wins[16], losses[16];

        ST_Print(&st.data, "wins", wins, sizeof(wins));
        ST_Print(&st.data, "losses", losses, sizeof(losses));
        Com_sprintf(values[3], sizeof(values[3]), "%s / %s", wins, losses);
    }
    ST_Print(&st.data, "played", values[4], sizeof(values[4]));

    for (i = 0; i < 5; i++) {
        float cx = x0 + i * (ST_CARD_W + ST_CARD_GAP);
        // componentDidUpdate shrinks a long number 4px a letter past ten
        int len = strlen(values[i]);
        float size = len > 10 ? 36.0f - 4.0f * (len - 10) : 36.0f;
        float tw = SM_Advance(FONT_SANS, size, values[i], -1);

        SM_Fill(cx, y0, ST_CARD_W, ST_CARD_H, stCard, clip);
        if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 21.333f, y0 + 5.3f, 25.6f), 21.333f, clip)) {
            SM_Text(cx + 4.27f, SM_Baseline(FONT_DEFAULT, 21.333f, y0 + 5.3f, 25.6f), FONT_DEFAULT, 21.333f, smText,
                    cardTitles[i], -1, qfalse);
        }
        if (SM_TextInside(SM_Baseline(FONT_SANS, size, y0 + 50.9f, 45.0f), size, clip)) {
            SM_Text(cx + (ST_CARD_W - tw) * 0.5f, SM_Baseline(FONT_SANS, size, y0 + 50.9f, 45.0f), FONT_SANS, size,
                    stYellow, values[i], SM_Fit(FONT_SANS, size, values[i], ST_CARD_W), qfalse);
        }
    }
    y = y0 + ST_CARD_H + 30.0f;  // the container's margin-bottom, 2em

    // Weapons: the table's columns as wide as what they hold
    for (i = 0; i < ST_NUM_WEAPONS; i++) {
        Q_strncpyz(cells[0][i], stWeapons[i].name, sizeof(cells[0][i]));
        cells[1][i][0] = '\0';
        ST_Print(&st.data, va("kill_%s", stWeapons[i].id), cells[2][i], sizeof(cells[2][i]));
        ST_Print(&st.data, va("mod_%s", stWeapons[i].id), cells[3][i], sizeof(cells[3][i]));
        ST_Print(&st.data, va("hits_%s", stWeapons[i].id), cells[4][i], sizeof(cells[4][i]));
        ST_Print(&st.data, va("shots_%s", stWeapons[i].id), cells[5][i], sizeof(cells[5][i]));
        if (strcmp(stWeapons[i].id, "gauntlet")) {
            ST_Accuracy(&st.data, stWeapons[i].id, cells[6][i], sizeof(cells[6][i]));
        } else {
            cells[6][i][0] = '\0';
        }
    }
    tableW = 0;
    for (c = 0; c < 7; c++) {
        colW[c] = c == 1 ? 32.0f + 2 * ST_CELL_PAD : ST_ColumnWidth(c, heads[c], cells[c], ST_NUM_WEAPONS);
        tableW += colW[c];
    }

    if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), 32, clip)) {
        SM_Text(x0 + 6.4f, SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), FONT_DEFAULT, 32, smText, "Weapons", -1,
                qfalse);
    }
    by = y + 54.4f;
    bx = x0;
    for (c = 0; c < 7; c++) {
        float right = bx + colW[c] - (c == 6 ? 0 : ST_CELL_PAD);

        if (heads[c][0] && SM_TextInside(SM_Baseline(FONT_SANS, 15, by, ST_TH_H), 15, clip)) {
            SM_Text(right - SM_AdvanceBold(FONT_SANS, 15, heads[c], -1, qtrue), SM_Baseline(FONT_SANS, 15, by, ST_TH_H),
                    FONT_SANS, 15, smText, heads[c], -1, qtrue);
        }
        bx += colW[c];
    }
    by += ST_TH_H;
    for (i = 0; i < ST_NUM_WEAPONS; i++) {
        float ry = by + i * ST_TD_H;
        float baseline = ry + 32.0f;  // the cells' text sits on the line the icon stands on

        bx = x0;
        for (c = 0; c < 7; c++) {
            float right = bx + colW[c] - (c == 6 ? 0 : ST_CELL_PAD);

            if (c == 1) {
                SM_Pic(bx + ST_CELL_PAD, ry, 32, 32, st.weapon[i], clip);
            } else if (cells[c][i][0] && SM_TextInside(baseline, 15, clip)) {
                SM_Text(right - SM_Advance(FONT_SANS, 15, cells[c][i], -1), baseline, FONT_SANS, 15, smText,
                        cells[c][i], -1, qfalse);
            }
            bx += colW[c];
        }
    }

    // Other Deaths, beside it: inline-block after the table's 100px margin
    {
        float mx = x0 + tableW + 100.0f;
        float my = y + 79.4f;
        float nameSize = 21.333f;
        float space = SM_Advance(FONT_DEFAULT, nameSize, " ", -1);
        float bottom = by + ST_NUM_WEAPONS * ST_TD_H + 30.0f;

        if (mx + ST_MOD_W > x0 + innerW) {
            // no room beside it: it goes under, as an inline-block would
            mx = x0;
            y = bottom;
            my = y + 79.4f;
        }
        if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), 32, clip)) {
            SM_Text(mx + 6.4f, SM_Baseline(FONT_DEFAULT, 32, y + 8, 38.4f), FONT_DEFAULT, 32, smText, "Other Deaths", -1,
                    qfalse);
        }
        for (i = 0; i < ST_NUM_MODS; i++) {
            char number[16];
            int v = 0;
            float nb;

            if (stMods[i].optional && (!ST_Has(&st.data, stMods[i].id, &v) || !v)) {
                continue;
            }
            ST_Print(&st.data, stMods[i].id, number, sizeof(number));
            SM_Pic(mx, my, ST_MOD_W, ST_MOD_H, st.mod[i], clip);
            nb = SM_Baseline(FONT_DEFAULT, nameSize, my + 20.0f, 24.0f);
            if (SM_TextInside(nb, nameSize, clip)) {
                float numberW = SM_AdvanceBold(FONT_DEFAULT, nameSize, number, -1, qtrue);

                ST_Shadowed(mx + ST_MOD_W - 40 - space - SM_Advance(FONT_DEFAULT, nameSize, stMods[i].name, -1), nb,
                            FONT_DEFAULT, nameSize, smText, stMods[i].name, qfalse);
                ST_Shadowed(mx + ST_MOD_W - 20 - numberW * 0.5f, nb, FONT_DEFAULT, nameSize, smText, number, qtrue);
            }
            my += ST_MOD_H + ST_MOD_GAP;
        }
        if (my > bottom) {
            bottom = my;
        }
        return bottom - y0;
    }
}

static float ST_DrawMedals(float x0, float y0, float innerW, const smRect_t* clip) {
    int cols = (int)((innerW + ST_MEDAL_GAP) / (ST_MEDAL_W + ST_MEDAL_GAP));
    int i;

    if (cols < 1) {
        cols = 1;
    }
    for (i = 0; i < ST_NUM_MEDALS; i++) {
        float mx = x0 + (i % cols) * (ST_MEDAL_W + ST_MEDAL_GAP);
        float my = y0 + (i / cols) * (ST_MEDAL_H + ST_MEDAL_GAP);
        char number[16];
        int starts[SM_MAX_LINES], lens[SM_MAX_LINES];
        int lines, k;

        SM_Fill(mx, my, ST_MEDAL_W, ST_MEDAL_H, stBlack80, clip);

        // .data, floated right: the medal and the count under it, which shrinks past three
        // digits as componentDidUpdate has it
        SM_Pic(mx + ST_MEDAL_W - 10 - 64, my + 10, 64, 64, st.medal[i], clip);
        ST_Print(&st.data, stMedals[i].id, number, sizeof(number));
        if (number[0]) {
            int len = strlen(number);
            float size = len > 3 ? 24.0f - 4.0f * (len - 3) : 36.0f;
            float nb = SM_Baseline(FONT_DEFAULT, size, my + 77, 30);

            if (SM_TextInside(nb, size, clip)) {
                ST_Shadowed(mx + ST_MEDAL_W - 10 - 32 - SM_AdvanceBold(FONT_DEFAULT, size, number, -1, qtrue) * 0.5f, nb,
                            FONT_DEFAULT, size, smText, number, qtrue);
            }
        }

        // the article: the name in yellow, 18pt, and the description wrapped beside the float
        if (SM_TextInside(SM_Baseline(FONT_DEFAULT, 24, my + 16, 28.8f), 24, clip)) {
            SM_Text(mx + 10 + 4.8f, SM_Baseline(FONT_DEFAULT, 24, my + 16, 28.8f), FONT_DEFAULT, 24, stYellow,
                    stMedals[i].name, -1, qfalse);
        }
        lines = SM_Wrap(FONT_SANS, 12, stMedals[i].desc, ST_MEDAL_W - 20 - 64 - 10, starts, lens);
        for (k = 0; k < lines; k++) {
            float lb = SM_Baseline(FONT_SANS, 12, my + 50.8f + k * 19.2f, 19.2f);

            if (SM_TextInside(lb, 12, clip)) {
                SM_Text(mx + 10, lb, FONT_SANS, 12, smText, stMedals[i].desc + starts[k], lens[k], qfalse);
            }
        }
    }
    return ((ST_NUM_MEDALS + cols - 1) / cols) * (ST_MEDAL_H + ST_MEDAL_GAP);
}

static void ST_DrawTitle(float left, float top, float width, const smRect_t* clip) {
    char name[64];
    float nx = left + 6.4f + ST_AVATAR;
    float baseline = top + 8 + ST_AVATAR;

    SM_Fill(left, top, width, ST_TITLE_H, stBlack80, clip);
    // the avatar, once Steam has it; meanwhile one's own model stands in for one's own, as on
    // the card, and anybody else's is the initial of the name, as in the friend list
    if (!st.steamId[0]) {
        qhandle_t face = UI_Home_Face();

        if (face) {
            SM_Pic(left + 6.4f, top + 8, ST_AVATAR, ST_AVATAR, face, clip);
        }
    } else if (SM_Avatar(st.steamId)) {
        SM_Pic(left + 6.4f, top + 8, ST_AVATAR, ST_AVATAR, SM_Avatar(st.steamId), clip);
    } else {
        char initial[8];

        SM_Fill(left + 6.4f, top + 8, ST_AVATAR, ST_AVATAR, smPage, clip);
        SM_Fill(left + 8.4f, top + 10, ST_AVATAR - 4, ST_AVATAR - 4, smField, clip);
        ST_StripColors(st.data.name, initial, 2);
        if (initial[0] && SM_TextInside(SM_Baseline(FONT_DEFAULT, 32, top + 8, ST_AVATAR), 32, clip)) {
            SM_Text(left + 6.4f + (ST_AVATAR - SM_Advance(FONT_DEFAULT, 32, initial, -1)) * 0.5f,
                    SM_Baseline(FONT_DEFAULT, 32, top + 8, ST_AVATAR), FONT_DEFAULT, 32, smText, initial, -1, qfalse);
        }
    }
    ST_StripColors(st.data.name, name, sizeof(name));
    nx += SM_Advance(FONT_DEFAULT, 32, " ", -1);
    if (name[0] && SM_TextInside(baseline, 32, clip)) {
        SM_Text(nx, baseline, FONT_DEFAULT, 32, smText, name, SM_Fit(FONT_DEFAULT, 32, name, left + width - nx), qfalse);
    }
}

static void ST_Request(void) {
    trap_SteamUserStats(st.steamId, &st.data, qtrue);
}

// Career Stats of a player, "" for one's own
void UI_Stats_Show(const char* steamId) {
    Q_strncpyz(stPending, steamId ? steamId : "", sizeof(stPending));
    Menus_CloseAll();
    Menus_ActivateByName("stats");
}

void UI_Stats_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    ST_Load();
    Q_strncpyz(st.steamId, stPending, sizeof(st.steamId));
    st.tab = 0;
    st.scroll = 0;
    ST_Request();

    menu = Menus_FindByName("stats");
    item = menu ? Menu_FindItemByName(menu, "stats") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Stats_Paint(void) {
    float left = UI_Home_Left();
    float width = HM_CONTAINER_W;
    float view = smBottom - ST_BOTTOM - HM_NAV_H;
    float top, y, inner;
    smRect_t clip;

    SM_BeginFrame();
    ST_Load();
    trap_SteamUserStats(st.steamId, &st.data, qfalse);
    UI_Home_Backdrop();
    UI_Home_PageTop(HM_NAV_STATS);

    // .body.scroll-y: it scrolls when the page is taller than the screen
    if (st.scroll > st.content - view) {
        st.scroll = st.content - view;
    }
    if (st.scroll < 0) {
        st.scroll = 0;
    }
    clip = SM_R(left, HM_NAV_H, width, view);
    top = HM_NAV_H - st.scroll;

    ST_DrawTitle(left, top, width, &clip);

    y = top + ST_TITLE_H;
    SM_Fill(left, y, width, ST_ROW_H, stBlack80, &clip);
    {
        float bx = left;

        bx += ST_Tab(bx, y, "Statistics", 0, &clip);
        ST_Tab(bx, y, "Medals", 1, &clip);
    }
    y += ST_ROW_H + ST_ROW_GAP;

    // .stats-content: black at 0.8 around what the tab holds, 5px in; measured on the frame
    // before, since the box goes under what it holds
    SM_Fill(left, y, width, st.content - (y - top) > 0 ? st.content - (y - top) : 0, stBlack80, &clip);
    inner = st.tab == 0 ? ST_DrawBasic(left + ST_PAD, y + ST_PAD, width - 2 * ST_PAD, &clip)
                        : ST_DrawMedals(left + ST_PAD, y + ST_PAD, width - 2 * ST_PAD, &clip);
    st.content = (y - top) + inner + 2 * ST_PAD;

    UI_Home_Banner();
    SM_EndFrame();
}

qboolean UI_Stats_HandleKey(int key) {
    const smHit_t* hit;

    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    if (key == K_MWHEELUP || key == K_MWHEELDOWN) {
        st.scroll += key == K_MWHEELUP ? -60.0f : 60.0f;
        return qtrue;
    }
    if (key != K_MOUSE1) {
        return qfalse;  // Escape goes back through the menu's onESC
    }
    if (UI_Home_NavClick()) {
        return qtrue;
    }
    hit = SM_HitAt(smMX, smMY);
    if (hit && hit->type == ST_HIT_TAB) {
        // each tab is a route of its own, and the page asks Steam again on the way
        st.tab = hit->index;
        st.scroll = 0;
        ST_Request();
    }
    return qtrue;
}
