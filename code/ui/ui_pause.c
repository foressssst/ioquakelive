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
// ui_pause.c -- [QL] the in-game menu
//
// This one retail does draw itself: ui/ingame.menu is the bar with Current Match,
// Call Vote, Admin and Add Bot on the left and Main Menu, Settings and Leave Match
// on the right, and ui/ingame_{about,callvote,admin,addbot}.menu are the four
// panels under it. What each button does is taken from those files, command for
// command; what it looks like follows web.pak's stylesheet, so the menu you get
// mid-match belongs to the same front end as everything else.
//
// The two buttons that lead nowhere here are gone: Main Menu and Settings both
// handed the screen to the browser, and Settings already has a page of its own.
//

#include "ui_web.h"

#define PS_PAGE_MAX_W 1360.0f
#define PS_PAGE_MARGIN 60.0f
#define PS_BAR_H 64.0f
#define PS_BAR_TOP 74.0f
#define PS_BODY_TOP (PS_BAR_TOP + PS_BAR_H + 26.0f)
#define PS_BOTTOM 46.0f
#define PS_ROW_H 40.0f

typedef enum {
    PS_TAB_MATCH,
    PS_TAB_VOTE,
    PS_TAB_ADMIN,
    PS_TAB_BOT,
    PS_NUM_TABS
} psTab_t;

typedef enum {
    PS_HIT_NONE,
    PS_HIT_TAB,
    PS_HIT_LEAVE,
    PS_HIT_SETTINGS,
    PS_HIT_JOIN,
    PS_HIT_SPECTATE,
    PS_HIT_RETURN,
    PS_HIT_VOTE,
    PS_HIT_VOTEARG,
    PS_HIT_PLAYER,
    PS_HIT_ADMIN,
    PS_HIT_BOT,
    PS_HIT_SKILL,
    PS_HIT_TEAM,
    PS_HIT_ADDBOT,
    PS_HIT_TRACK,
    PS_HIT_THUMB
} psHitType_t;

/*
===============================================================================

WHAT THE PANELS OFFER

Every command here is one ui/ingame_callvote.menu or ui/ingame_admin.menu sends.

===============================================================================
*/

typedef struct {
    const char* title;
    const char* command;  // NULL when it needs something picked first
    int arg;              // 1 a map, 2 a player
} psAction_t;

static const psAction_t psVotes[] = {
    {"Restart Map", "cmd callvote map_restart", 0},
    {"Next Map", "cmd callvote nextmap", 0},
    {"Shuffle Teams", "cmd callvote shuffle", 0},
    {"Cointoss", "cmd callvote cointoss", 0},
    {"Team Size 2", "cmd callvote teamsize 2", 0},
    {"Team Size 4", "cmd callvote teamsize 4", 0},
    {"No Team Size", "cmd callvote teamsize 0", 0},
    {"Timers On", "cmd callvote timers on", 0},
    {"Timers Off", "cmd callvote timers off", 0},
    {"Loadouts On", "cmd callvote loadouts on", 0},
    {"Loadouts Off", "cmd callvote loadouts off", 0},
    {"Ammo Per Weapon", "cmd callvote ammo weap", 0},
    {"Ammo Global", "cmd callvote ammo global", 0},
    {"Random 20", "cmd callvote random 20", 0},
    {"Change Map...", NULL, 1},
    {"Kick Player...", NULL, 2},
};

#define PS_NUM_VOTES ((int)ARRAY_LEN(psVotes))

static const psAction_t psServerActions[] = {
    {"Pause", "cmd pause", 0},
    {"Resume", "cmd timein", 0},
    {"Lock Teams", "cmd lock", 0},
    {"Unlock Teams", "cmd unlock", 0},
    {"Lock Red", "cmd lock red", 0},
    {"Unlock Red", "cmd unlock red", 0},
    {"Lock Blue", "cmd lock blue", 0},
    {"Unlock Blue", "cmd unlock blue", 0},
};

#define PS_NUM_SERVER_ACTIONS ((int)ARRAY_LEN(psServerActions))

// the per-player ones take the client number, as the .menu's uiScripts do
static const psAction_t psPlayerActions[] = {
    {"To Red", "put %i r", 0},
    {"To Blue", "put %i b", 0},
    {"To Spec", "put %i s", 0},
    {"Mute", "mute %i", 0},
    {"Unmute", "unmute %i", 0},
    {"Kick", "clientkick %i", 0},
    {"Ban", "ban %i", 0},
};

#define PS_NUM_PLAYER_ACTIONS ((int)ARRAY_LEN(psPlayerActions))

static const char* const psSkills[] = {"I Can Win", "Bring it On", "Hurt Me Plenty", "Hardcore", "Nightmare"};

/*
===============================================================================

STATE

===============================================================================
*/

static struct {
    int tab;
    float page, pageW;
    float scroll[PS_NUM_TABS];
    float listTop, view, content;
    qboolean scrollDrag;
    float scrollGrab;

    int voteArg;   // 0 none, 1 a map, 2 a player
    int player;    // the one picked in Admin
    int bot, skill, team;
} ps;

// the browser tinted the game rather than hiding it
static const vec4_t psTint = {19 / 255.0f, 68 / 255.0f, 92 / 255.0f, 0.7f};
static const vec4_t psBarBg = {0.0f, 0.0f, 0.0f, 0.55f};
static const vec4_t psBlack = {0.0f, 0.0f, 0.0f, 1.0f};

/*
===============================================================================

DRAWING

===============================================================================
*/

static void PS_Frame(void) {
    ps.pageW = smVW - 2 * PS_PAGE_MARGIN;
    if (ps.pageW > PS_PAGE_MAX_W) {
        ps.pageW = PS_PAGE_MAX_W;
    }
    ps.page = (smVW - ps.pageW) * 0.5f;
    ps.listTop = PS_BODY_TOP;
    ps.view = (float)SM_VIEW_H - PS_BOTTOM - ps.listTop;
}

static float PS_ClampScroll(float scroll) {
    if (scroll > ps.content - ps.view) {
        scroll = ps.content - ps.view;
    }
    return scroll < 0 ? 0 : scroll;
}

static qboolean PS_Button(float x, float y, float w, float h, const char* text, int type, int index,
                          qboolean strong) {
    qboolean over = SM_Over(type, index);

    SM_Fill(x, y, w, h, strong ? (over ? smYellow : smButtonHover) : (over ? smButtonHover : smButton), NULL);
    SM_Fill(x, y, w, 2, strong ? smYellow : smGrey, NULL);
    SM_Centered(x + w * 0.5f, SM_Baseline(FONT_SANS, 16, y + (h - 24) * 0.5f, 24), 16,
                (strong && over) ? psBlack : over ? smYellow : smText, text, qtrue);
    SM_Hit(x, y, w, h, type, index, NULL);
    return over;
}

/*
===============
PS_DrawBar

ui/ingame.menu's nav, in the front end's language: the sections of the menu on the
left and what leaves it on the right.
===============
*/
static void PS_DrawBar(void) {
    static const char* const names[PS_NUM_TABS] = {"Current Match", "Call Vote", "Admin", "Add Bot"};
    float x = ps.page;
    float w;
    int i;

    SM_Fill(0, PS_BAR_TOP - 14, smVW, PS_BAR_H + 14, psBarBg, NULL);
    SM_Fill(0, PS_BAR_TOP + PS_BAR_H, smVW, 1, smGrey, NULL);

    for (i = 0; i < PS_NUM_TABS; i++) {
        qboolean on = i == ps.tab;
        qboolean over = SM_Over(PS_HIT_TAB, i);

        w = SM_SpacedWidth(FONT_DEFAULT, 16, names[i], 1.5f) + 40;
        SM_SpacedText(x + 20, SM_Baseline(FONT_DEFAULT, 16, PS_BAR_TOP + 10, 24), FONT_DEFAULT, 16,
                      on ? smYellow : over ? smText : smSelectText, names[i], 1.5f, on);
        if (on) {
            SM_Fill(x, PS_BAR_TOP + PS_BAR_H - 3, w, 3, smYellow, NULL);
        }
        SM_Hit(x, PS_BAR_TOP, w, PS_BAR_H, PS_HIT_TAB, i, NULL);
        x += w;
    }

    w = SM_SpacedWidth(FONT_DEFAULT, 16, "Leave Match", 1.5f) + 40;
    x = ps.page + ps.pageW - w;
    SM_SpacedText(x + 20, SM_Baseline(FONT_DEFAULT, 16, PS_BAR_TOP + 10, 24), FONT_DEFAULT, 16,
                  SM_Over(PS_HIT_LEAVE, 0) ? smYellow : smSelectText, "Leave Match", 1.5f, qfalse);
    SM_Hit(x, PS_BAR_TOP, w, PS_BAR_H, PS_HIT_LEAVE, 0, NULL);

    w = SM_SpacedWidth(FONT_DEFAULT, 16, "Settings", 1.5f) + 40;
    x -= w;
    SM_SpacedText(x + 20, SM_Baseline(FONT_DEFAULT, 16, PS_BAR_TOP + 10, 24), FONT_DEFAULT, 16,
                  SM_Over(PS_HIT_SETTINGS, 0) ? smYellow : smSelectText, "Settings", 1.5f, qfalse);
    SM_Hit(x, PS_BAR_TOP, w, PS_BAR_H, PS_HIT_SETTINGS, 0, NULL);
}

// the gametype's own name, as UI_DrawServerSettings reads it
static const char* PS_GametypeName(int gametype) {
    static const char* const names[] = {
        "Free For All", "Duel", "Race", "Team Deathmatch", "Clan Arena", "Capture the Flag", "One Flag CTF",
        "Overload", "Harvester", "Freeze Tag", "Domination", "Attack and Defend", "Red Rover"};

    return ((unsigned int)gametype <= 12u) ? names[gametype] : "Unknown Gametype";
}

static void PS_DrawMatch(void) {
    char serverinfo[MAX_INFO_STRING];
    char line[128];
    float x = ps.page;
    float y = ps.listTop;
    float cardW = ps.pageW * 0.54f;
    qboolean spectating;
    int gametype, value;

    trap_GetConfigString(CS_SERVERINFO, serverinfo, sizeof(serverinfo));
    gametype = atoi(Info_ValueForKey(serverinfo, "g_gametype"));
    spectating = trap_Cvar_VariableValue("cg_spectating") != 0;

    SM_Fill(x, y, cardW, 210, smRowOdd, NULL);
    SM_SpacedText(x + 22, SM_Baseline(FONT_DEFAULT, 24, y + 18, 32), FONT_DEFAULT, 24, smText,
                  PS_GametypeName(gametype), -1, qfalse);
    Q_strncpyz(line, Info_ValueForKey(serverinfo, "mapname"), sizeof(line));
    SM_Text(x + 22, SM_Baseline(FONT_SANS, 15, y + 62, 22), FONT_SANS, 15, smYellow, line, -1, qtrue);

    y += 96;
    Com_sprintf(line, sizeof(line), "Time Limit: %i", atoi(Info_ValueForKey(serverinfo, "timelimit")));
    SM_Text(x + 22, SM_Baseline(FONT_SANS, 15, y, 24), FONT_SANS, 15, smSelectText, line, -1, qfalse);
    y += 26;
    if (gametype < 4) {
        Com_sprintf(line, sizeof(line), "Frag Limit: %i", atoi(Info_ValueForKey(serverinfo, "fraglimit")));
        SM_Text(x + 22, SM_Baseline(FONT_SANS, 15, y, 24), FONT_SANS, 15, smSelectText, line, -1, qfalse);
        y += 26;
    }
    value = atoi(Info_ValueForKey(serverinfo, "capturelimit"));
    if (value && gametype >= 5) {
        Com_sprintf(line, sizeof(line), "Capture Limit: %i", value);
        SM_Text(x + 22, SM_Baseline(FONT_SANS, 15, y, 24), FONT_SANS, 15, smSelectText, line, -1, qfalse);
        y += 26;
    }
    Com_sprintf(line, sizeof(line), "%i/%s Players", uiInfo.playerCount,
                Info_ValueForKey(serverinfo, "sv_maxclients"));
    SM_Text(x + 22, SM_Baseline(FONT_SANS, 15, y, 24), FONT_SANS, 15, smSelectText, line, -1, qfalse);

    // ingame_about.menu shows JOIN MATCH while cg_spectating is 1 and RETURN while it is 0,
    // and SPECTATE only in the gametypes you can watch from inside (0, 1, 2 and 12)
    x = ps.page + cardW + 26;
    y = ps.listTop;
    if (spectating) {
        PS_Button(x, y, ps.pageW - cardW - 26, 64, "JOIN MATCH", PS_HIT_JOIN, 0, qtrue);
    } else {
        PS_Button(x, y, ps.pageW - cardW - 26, 64, "RETURN TO MATCH", PS_HIT_RETURN, 0, qtrue);
    }
    y += 76;
    if (!spectating && (gametype == 0 || gametype == 1 || gametype == 2 || gametype == 12)) {
        PS_Button(x, y, ps.pageW - cardW - 26, 48, "SPECTATE", PS_HIT_SPECTATE, 0, qfalse);
    }
}

static void PS_DrawVote(void) {
    float x = ps.page;
    float y = ps.listTop - ps.scroll[PS_TAB_VOTE];
    float w = (ps.pageW - 3 * 16) / 4.0f;
    smRect_t clip = SM_R(ps.page - 4, ps.listTop, ps.pageW + 8, ps.view);
    int i;

    if (ps.voteArg == 2) {
        ps.content = uiInfo.playerCount * PS_ROW_H;
        for (i = 0; i < uiInfo.playerCount; i++) {
            float ry = y + i * PS_ROW_H;
            qboolean over = SM_Over(PS_HIT_VOTEARG, i);

            if (ry + PS_ROW_H < ps.listTop || ry > ps.listTop + ps.view) {
                continue;
            }
            SM_Fill(x, ry, ps.pageW, PS_ROW_H, over ? smCardHover : (i & 1) ? smRowOdd : smRowEven, &clip);
            SM_Text(x + 16, SM_Baseline(FONT_SANS, 15, ry + 9, 22), FONT_SANS, 15, over ? smText : smSelectText,
                    uiInfo.playerNames[i], -1, qfalse);
            SM_Hit(x, ry, ps.pageW, PS_ROW_H, PS_HIT_VOTEARG, i, &clip);
        }
        return;
    }
    if (ps.voteArg == 1) {
        ps.content = uiInfo.mapCount * PS_ROW_H;
        for (i = 0; i < uiInfo.mapCount; i++) {
            float ry = y + i * PS_ROW_H;
            qboolean over = SM_Over(PS_HIT_VOTEARG, i);

            if (ry + PS_ROW_H < ps.listTop || ry > ps.listTop + ps.view) {
                continue;
            }
            SM_Fill(x, ry, ps.pageW, PS_ROW_H, over ? smCardHover : (i & 1) ? smRowOdd : smRowEven, &clip);
            SM_Text(x + 16, SM_Baseline(FONT_SANS, 15, ry + 9, 22), FONT_SANS, 15, over ? smText : smSelectText,
                    uiInfo.mapList[i].mapName, -1, qfalse);
            SM_Hit(x, ry, ps.pageW, PS_ROW_H, PS_HIT_VOTEARG, i, &clip);
        }
        return;
    }

    ps.content = 0;
    for (i = 0; i < PS_NUM_VOTES; i++) {
        float bx = x + (i % 4) * (w + 16);
        float by = ps.listTop + (i / 4) * 62.0f;

        PS_Button(bx, by, w, 48, psVotes[i].title, PS_HIT_VOTE, i, qfalse);
    }
}

static void PS_DrawAdmin(void) {
    float x = ps.page;
    float w = (ps.pageW - 3 * 16) / 4.0f;
    float y;
    smRect_t clip;
    int i;

    for (i = 0; i < PS_NUM_SERVER_ACTIONS; i++) {
        float bx = x + (i % 4) * (w + 16);
        float by = ps.listTop + (i / 4) * 58.0f;

        PS_Button(bx, by, w, 44, psServerActions[i].title, PS_HIT_ADMIN, -1 - i, qfalse);
    }

    y = ps.listTop + 2 * 58.0f + 14;
    SM_SpacedText(x, SM_Baseline(FONT_DEFAULT, 15, y, 22), FONT_DEFAULT, 15, smYellow, "Players", 1.5f, qfalse);
    y += 34;
    clip = SM_R(ps.page - 4, y, ps.pageW + 8, ps.listTop + ps.view - y);
    for (i = 0; i < uiInfo.playerCount; i++) {
        float ry = y + i * PS_ROW_H - ps.scroll[PS_TAB_ADMIN];
        qboolean chosen = i == ps.player;
        qboolean over = SM_Over(PS_HIT_PLAYER, i);

        if (ry + PS_ROW_H < y || ry > ps.listTop + ps.view) {
            continue;
        }
        SM_Fill(x, ry, ps.pageW, PS_ROW_H, chosen ? smCard : over ? smCardHover : (i & 1) ? smRowOdd : smRowEven,
                &clip);
        if (chosen) {
            SM_Fill(x, ry, 3, PS_ROW_H, smYellow, &clip);
        }
        SM_Text(x + 16, SM_Baseline(FONT_SANS, 15, ry + 9, 22), FONT_SANS, 15, chosen ? smYellow : smSelectText,
                uiInfo.playerNames[i], -1, qfalse);
        SM_Hit(x, ry, ps.pageW, PS_ROW_H, PS_HIT_PLAYER, i, &clip);
    }
    ps.content = (y - ps.listTop) + uiInfo.playerCount * PS_ROW_H;

    if (ps.player >= 0 && ps.player < uiInfo.playerCount) {
        float ay = ps.listTop + ps.view - 52;
        float aw = (ps.pageW - (PS_NUM_PLAYER_ACTIONS - 1) * 10) / (float)PS_NUM_PLAYER_ACTIONS;

        for (i = 0; i < PS_NUM_PLAYER_ACTIONS; i++) {
            PS_Button(x + i * (aw + 10), ay, aw, 40, psPlayerActions[i].title, PS_HIT_ADMIN, i, qfalse);
        }
    }
}

static void PS_DrawBot(void) {
    float x = ps.page;
    float y = ps.listTop;
    float w = (ps.pageW - 3 * 16) / 4.0f;
    int numBots = UI_GetNumBots();
    int i;

    SM_SpacedText(x, SM_Baseline(FONT_DEFAULT, 15, y, 22), FONT_DEFAULT, 15, smYellow, "Bot", 1.5f, qfalse);
    SM_Fill(x + 70, y + 2, ps.pageW - 70, 34, smInput, NULL);
    SM_Text(x + 82, SM_Baseline(FONT_SANS, 15, y + 8, 22), FONT_SANS, 15, smText,
            UI_GetBotNameByNumber(ps.bot), -1, qfalse);
    SM_Text(x + ps.pageW - 52, SM_Baseline(FONT_SANS, 17, y + 7, 22), FONT_SANS, 17,
            SM_Over(PS_HIT_BOT, -1) ? smYellow : smArrow, "<", -1, qtrue);
    SM_Text(x + ps.pageW - 26, SM_Baseline(FONT_SANS, 17, y + 7, 22), FONT_SANS, 17,
            SM_Over(PS_HIT_BOT, 1) ? smYellow : smArrow, ">", -1, qtrue);
    SM_Hit(x + ps.pageW - 58, y + 2, 26, 34, PS_HIT_BOT, -1, NULL);
    SM_Hit(x + ps.pageW - 32, y + 2, 26, 34, PS_HIT_BOT, 1, NULL);
    if (numBots <= 0) {
        SM_Text(x + 82, SM_Baseline(FONT_SANS, 15, y + 8, 22), FONT_SANS, 15, smSelectText, "No bots found", -1,
                qfalse);
    }

    y += 62;
    SM_SpacedText(x, SM_Baseline(FONT_DEFAULT, 15, y + 12, 22), FONT_DEFAULT, 15, smYellow, "Skill", 1.5f, qfalse);
    for (i = 0; i < (int)ARRAY_LEN(psSkills); i++) {
        float bx = x + 70 + i * ((ps.pageW - 70) / 5.0f);
        qboolean on = i == ps.skill;
        qboolean over = SM_Over(PS_HIT_SKILL, i);

        SM_Fill(bx, y, (ps.pageW - 70) / 5.0f - 10, 40, on ? smYellow : over ? smButtonHover : smButton, NULL);
        SM_Centered(bx + ((ps.pageW - 70) / 5.0f - 10) * 0.5f, SM_Baseline(FONT_SANS, 14, y + 9, 22), 14,
                    on ? psBlack : smText, psSkills[i], qtrue);
        SM_Hit(bx, y, (ps.pageW - 70) / 5.0f - 10, 40, PS_HIT_SKILL, i, NULL);
    }

    y += 62;
    SM_SpacedText(x, SM_Baseline(FONT_DEFAULT, 15, y + 12, 22), FONT_DEFAULT, 15, smYellow, "Team", 1.5f, qfalse);
    for (i = 0; i < 2; i++) {
        float bx = x + 70 + i * (w + 16);
        qboolean on = i == ps.team;
        qboolean over = SM_Over(PS_HIT_TEAM, i);

        SM_Fill(bx, y, w, 40, on ? smYellow : over ? smButtonHover : smButton, NULL);
        SM_Centered(bx + w * 0.5f, SM_Baseline(FONT_SANS, 14, y + 9, 22), 14, on ? psBlack : smText,
                    i ? "Blue" : "Red", qtrue);
        SM_Hit(bx, y, w, 40, PS_HIT_TEAM, i, NULL);
    }

    y += 78;
    PS_Button(x, y, w, 48, "ADD BOT", PS_HIT_ADDBOT, 0, qtrue);
    ps.content = 0;
}

static void PS_DrawPage(void) {
    PS_Frame();
    ps.scroll[ps.tab] = PS_ClampScroll(ps.scroll[ps.tab]);

    SM_FillScreen(psTint);
    PS_DrawBar();

    switch (ps.tab) {
        case PS_TAB_MATCH:
            PS_DrawMatch();
            break;
        case PS_TAB_VOTE:
            PS_DrawVote();
            break;
        case PS_TAB_ADMIN:
            PS_DrawAdmin();
            break;
        case PS_TAB_BOT:
            PS_DrawBot();
            break;
        default:
            break;
    }
}

/*
===============================================================================

WHAT IT DOES

===============================================================================
*/

static void PS_Close(void) {
    trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
    trap_Key_ClearStates();
    Menus_CloseAll();
    trap_Cvar_Set("cl_paused", "0");
}

static void PS_Do(const char* command) {
    trap_Cmd_ExecuteText(EXEC_APPEND, va("%s\n", command));
}

static void PS_Click(void) {
    const smHit_t* hit = SM_HitAt(smMX, smMY);
    int type = hit ? hit->type : PS_HIT_NONE;
    int index = hit ? hit->index : 0;

    switch (type) {
        case PS_HIT_TAB:
            ps.tab = index;
            ps.voteArg = 0;
            break;
        case PS_HIT_LEAVE:
            // ingame.menu navbtn7: uiScript leave
            PS_Do("disconnect");
            PS_Close();
            break;
        case PS_HIT_SETTINGS:
            Menus_ActivateByName("settings");
            break;
        case PS_HIT_JOIN:
            PS_Do("team a");
            PS_Close();
            break;
        case PS_HIT_SPECTATE:
            PS_Do("team s");
            PS_Close();
            break;
        case PS_HIT_RETURN:
            PS_Close();
            break;
        case PS_HIT_VOTE:
            if (psVotes[index].command) {
                PS_Do(psVotes[index].command);
                PS_Close();
            } else {
                ps.voteArg = psVotes[index].arg;
                ps.scroll[PS_TAB_VOTE] = 0;
                if (ps.voteArg == 1) {
                    UI_LoadArenasIntoMapList();
                } else {
                    UI_BuildPlayerList();
                }
            }
            break;
        case PS_HIT_VOTEARG:
            if (ps.voteArg == 1 && index >= 0 && index < uiInfo.mapCount) {
                PS_Do(va("cmd callvote map %s", uiInfo.mapList[index].mapLoadName));
            } else if (ps.voteArg == 2 && index >= 0 && index < uiInfo.playerCount) {
                PS_Do(va("cmd callvote clientkick %i", uiInfo.teamClientNums[index]));
            }
            ps.voteArg = 0;
            PS_Close();
            break;
        case PS_HIT_PLAYER:
            ps.player = index;
            break;
        case PS_HIT_ADMIN:
            if (index < 0) {
                PS_Do(psServerActions[-1 - index].command);
            } else if (ps.player >= 0 && ps.player < uiInfo.playerCount) {
                char command[64];

                Com_sprintf(command, sizeof(command), psPlayerActions[index].command,
                            uiInfo.teamClientNums[ps.player]);
                PS_Do(command);
            }
            break;
        case PS_HIT_BOT: {
            int numBots = UI_GetNumBots();

            if (numBots > 0) {
                ps.bot = (ps.bot + index + numBots) % numBots;
            }
            break;
        }
        case PS_HIT_SKILL:
            ps.skill = index;
            break;
        case PS_HIT_TEAM:
            ps.team = index;
            break;
        case PS_HIT_ADDBOT:
            // ingame_addbot.menu's addBot: name, skill 1..5, and a team
            PS_Do(va("addbot %s %i %s", UI_GetBotNameByNumber(ps.bot), ps.skill + 1, ps.team ? "Blue" : "Red"));
            PS_Close();
            break;
        case PS_HIT_THUMB:
            ps.scrollDrag = qtrue;
            ps.scrollGrab = smMY;
            break;
        default:
            break;
    }
}

/*
===============================================================================

THE PAGE

===============================================================================
*/

void UI_Pause_Open(void) {
    menuDef_t* menu;
    itemDef_t* item;

    ps.voteArg = 0;
    ps.player = -1;
    ps.scrollDrag = qfalse;
    UI_BuildPlayerList();

    menu = Menus_FindByName("ingame");
    item = menu ? Menu_FindItemByName(menu, "pause") : NULL;
    if (item) {
        item->window.flags |= WINDOW_HASFOCUS;
    }
}

void UI_Pause_Paint(void) {
    const smHit_t* under;

    SM_Frame();
    under = SM_HitAt(smMX, smMY);
    smHasUnder = under != NULL;
    if (under) {
        smUnder = *under;
    }
    smNumHits = 0;

    if (ps.scrollDrag && ps.content > ps.view) {
        float thumbH = ps.view * ps.view / ps.content;

        if (thumbH < 24) {
            thumbH = 24;
        }
        ps.scroll[ps.tab] =
            PS_ClampScroll(ps.scroll[ps.tab] + (smMY - ps.scrollGrab) * (ps.content - ps.view) / (ps.view - thumbH));
        ps.scrollGrab = smMY;
    }

    PS_DrawPage();

    memcpy(smLastHits, smHits, smNumHits * sizeof(smHits[0]));
    smNumLastHits = smNumHits;
}

qboolean UI_Pause_HandleKey(int key) {
    if (key & K_CHAR_FLAG) {
        return qtrue;
    }
    switch (key) {
        case K_MOUSE1:
            PS_Click();
            return qtrue;
        case K_MWHEELUP:
            ps.scroll[ps.tab] = PS_ClampScroll(ps.scroll[ps.tab] - 90);
            return qtrue;
        case K_MWHEELDOWN:
            ps.scroll[ps.tab] = PS_ClampScroll(ps.scroll[ps.tab] + 90);
            return qtrue;
        case K_ESCAPE:
            if (ps.voteArg) {
                ps.voteArg = 0;
                return qtrue;
            }
            PS_Close();
            return qtrue;
        default:
            return qfalse;
    }
}

void UI_Pause_MouseUp(void) {
    ps.scrollDrag = qfalse;
}
