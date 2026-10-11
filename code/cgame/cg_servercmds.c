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
#include "cg_local.h"
#include "../../ui/menudef.h"
#include "../ui/ui_shared.h"

typedef struct {
    const char* order;
    int taskNum;
} orderTask_t;

static const orderTask_t validOrders[] = {
    {VOICECHAT_GETFLAG, TEAMTASK_OFFENSE},
    {VOICECHAT_OFFENSE, TEAMTASK_OFFENSE},
    {VOICECHAT_DEFEND, TEAMTASK_DEFENSE},
    {VOICECHAT_DEFENDFLAG, TEAMTASK_DEFENSE},
    {VOICECHAT_PATROL, TEAMTASK_PATROL},
    {VOICECHAT_CAMP, TEAMTASK_CAMP},
    {VOICECHAT_FOLLOWME, TEAMTASK_FOLLOW},
    {VOICECHAT_RETURNFLAG, TEAMTASK_RETRIEVE},
    {VOICECHAT_FOLLOWFLAGCARRIER, TEAMTASK_ESCORT}};

static const int numValidOrders = ARRAY_LEN(validOrders);

static const char* cg_disableLoadoutCvarNames[WP_NUM_WEAPONS] = {
    NULL,
    "cg_disableLoadout_g",
    "cg_disableLoadout_mg",
    "cg_disableLoadout_sg",
    "cg_disableLoadout_gl",
    "cg_disableLoadout_rl",
    "cg_disableLoadout_lg",
    "cg_disableLoadout_rg",
    "cg_disableLoadout_pg",
    "cg_disableLoadout_bfg",
    "cg_disableLoadout_gh",
    "cg_disableLoadout_ng",
    "cg_disableLoadout_pl",
    "cg_disableLoadout_cg",
    "cg_disableLoadout_hmg",
};

static void CG_UpdateDisableLoadout(int loadout) {
    int disabled = atoi(CG_ConfigString(CS_DISABLE_LOADOUT));
    int starting = atoi(CG_ConfigString(CS_STARTING_WEAPONS));
    int i;

    for (i = WP_GAUNTLET; i < WP_NUM_WEAPONS; i++) {
        const char* name = cg_disableLoadoutCvarNames[i];

        if (!name) {
            continue;
        }
        if (!loadout || (disabled & (1 << i)) || (starting & (1 << (i - 1)))) {
            trap_Cvar_Set(name, "1");
        } else if (trap_Cvar_VariableValue(name) != 0.0f) {
            trap_Cvar_Set(name, "0");
        }
    }
}

static int CG_ValidOrder(const char* p) {
    int i;
    for (i = 0; i < numValidOrders; i++) {
        if (Q_stricmp(p, validOrders[i].order) == 0) {
            return validOrders[i].taskNum;
        }
    }
    return -1;
}

static void CG_ParseScores_Ffa(void) {
    int i, idx;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (i = 0; i < cg.numScores; i++) {
        score_t *sp = &cg.scores[i];
        idx = i * 18 + 4;
        sp->client = atoi(CG_Argv(idx++));
        sp->score = atoi(CG_Argv(idx++));
        sp->ping = atoi(CG_Argv(idx++));
        sp->time = atoi(CG_Argv(idx++));
        sp->accuracy = atoi(CG_Argv(idx++));
        sp->impressiveCount = atoi(CG_Argv(idx++));
        sp->excellentCount = atoi(CG_Argv(idx++));
        sp->guantletCount = atoi(CG_Argv(idx++));
        sp->defendCount = atoi(CG_Argv(idx++));
        sp->assistCount = atoi(CG_Argv(idx++));
        sp->perfect = atoi(CG_Argv(idx++));
        sp->captures = atoi(CG_Argv(idx++));
        sp->alive = atoi(CG_Argv(idx++));
        sp->frags = atoi(CG_Argv(idx++));
        sp->deaths = atoi(CG_Argv(idx++));
        sp->bestWeapon = atoi(CG_Argv(idx++));
        sp->bestWeaponAccuracy = atoi(CG_Argv(idx++));
        sp->damageDone = atoi(CG_Argv(idx++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].powerups = sp->powerUps;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    CG_SetScoreboardSelection();
}

static int CG_ParseTeamPickups(int i, qboolean ctf) {
    teamPickups_t* p;
    int t;

    for (t = 0; t < 2; t++) {
        p = &cg.teamPickups[t];
        p->ra = atoi(CG_Argv(i++));
        p->ya = atoi(CG_Argv(i++));
        p->ga = atoi(CG_Argv(i++));
        p->mh = atoi(CG_Argv(i++));
        p->quad = atoi(CG_Argv(i++));
        p->bs = atoi(CG_Argv(i++));
        p->regen = atoi(CG_Argv(i++));
        p->haste = atoi(CG_Argv(i++));
        p->invis = atoi(CG_Argv(i++));
        if (ctf) {
            p->flag = atoi(CG_Argv(i++));
            p->medkit = atoi(CG_Argv(i++));
        }
        p->quadTime = atoi(CG_Argv(i++));
        p->bsTime = atoi(CG_Argv(i++));
        p->regenTime = atoi(CG_Argv(i++));
        p->hasteTime = atoi(CG_Argv(i++));
        p->invisTime = atoi(CG_Argv(i++));
        if (ctf) {
            p->flagTime = atoi(CG_Argv(i++));
        }
    }
    return i;
}

static void CG_ParseScores_Tdm(void) {
    int i, n;

    i = CG_ParseTeamPickups(1, qfalse);

    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->tks = atoi(CG_Argv(i++));
        sp->tkd = atoi(CG_Argv(i++));
        sp->damageDone = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseScores_Ca(void) {
    int i, n;

    i = 1;
    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->bestWeaponAccuracy = atoi(CG_Argv(i++));
        sp->damageDone = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->perfect = atoi(CG_Argv(i++));
        sp->alive = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseScores_Ctf(void) {
    int i, n;

    i = CG_ParseTeamPickups(1, qtrue);

    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->defendCount = atoi(CG_Argv(i++));
        sp->assistCount = atoi(CG_Argv(i++));
        sp->captures = atoi(CG_Argv(i++));
        sp->perfect = atoi(CG_Argv(i++));
        sp->alive = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseScores_Ft(void) {
    int i, n;

    i = CG_ParseTeamPickups(1, qfalse);

    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->thaws = atoi(CG_Argv(i++));
        sp->tks = atoi(CG_Argv(i++));
        sp->tkd = atoi(CG_Argv(i++));
        sp->damageDone = atoi(CG_Argv(i++));
        sp->alive = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseScores_Rr(void) {
    int n, idx;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    idx = 4;
    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores; n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(idx++));
        sp->score = atoi(CG_Argv(idx++));
        sp->roundScore = atoi(CG_Argv(idx++));
        sp->ping = atoi(CG_Argv(idx++));
        sp->time = atoi(CG_Argv(idx++));
        sp->frags = atoi(CG_Argv(idx++));
        sp->deaths = atoi(CG_Argv(idx++));
        sp->accuracy = atoi(CG_Argv(idx++));
        sp->bestWeapon = atoi(CG_Argv(idx++));
        sp->bestWeaponAccuracy = atoi(CG_Argv(idx++));
        sp->damageDone = atoi(CG_Argv(idx++));
        sp->impressiveCount = atoi(CG_Argv(idx++));
        sp->excellentCount = atoi(CG_Argv(idx++));
        sp->guantletCount = atoi(CG_Argv(idx++));
        sp->defendCount = atoi(CG_Argv(idx++));
        sp->assistCount = atoi(CG_Argv(idx++));
        sp->perfect = atoi(CG_Argv(idx++));
        sp->captures = atoi(CG_Argv(idx++));
        sp->alive = atoi(CG_Argv(idx++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseScores_Race(void) {
    int i, n;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }

    i = 2;
    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseSmScores(void) {
    int i, n;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    i = 4;
    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        i++;
        i++;
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    cg.numScores = n;
    CG_SetScoreboardSelection();
}

static void CG_ParseScores(void) {
    int i, idx;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (i = 0; i < cg.numScores; i++) {
        score_t *sp = &cg.scores[i];
        idx = i * 18 + 4;
        sp->client = atoi(CG_Argv(idx++));
        sp->score = atoi(CG_Argv(idx++));
        sp->ping = atoi(CG_Argv(idx++));
        sp->time = atoi(CG_Argv(idx++));
        idx += 2;
        sp->accuracy = atoi(CG_Argv(idx++));
        sp->impressiveCount = atoi(CG_Argv(idx++));
        sp->excellentCount = atoi(CG_Argv(idx++));
        sp->guantletCount = atoi(CG_Argv(idx++));
        sp->defendCount = atoi(CG_Argv(idx++));
        sp->assistCount = atoi(CG_Argv(idx++));
        sp->perfect = atoi(CG_Argv(idx++));
        sp->captures = atoi(CG_Argv(idx++));
        sp->alive = atoi(CG_Argv(idx++));
        sp->frags = atoi(CG_Argv(idx++));
        sp->deaths = atoi(CG_Argv(idx++));
        sp->bestWeapon = atoi(CG_Argv(idx++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    CG_SetScoreboardSelection();
}

static void CG_ParseTeamInfo(void) {
    int i;
    int client;

    numSortedTeamPlayers = atoi(CG_Argv(1));
    if (numSortedTeamPlayers < 0 || numSortedTeamPlayers > TEAM_MAXOVERLAY) {
        CG_Error("CG_ParseTeamInfo: numSortedTeamPlayers out of range (%d)",
                 numSortedTeamPlayers);
        return;
    }

    for (i = 0; i < numSortedTeamPlayers; i++) {
        client = atoi(CG_Argv(i + 2));
        if (client < 0 || client >= MAX_CLIENTS) {
            CG_Error("CG_ParseTeamInfo: bad client number: %d", client);
            return;
        }
        sortedTeamPlayers[i] = client;
    }
}

void CG_ParseServerinfo(void) {
    const char* info;
    const char* mapname;

    info = CG_ConfigString(CS_SERVERINFO);

    cgs.gametype = atoi(Info_ValueForKey(info, "g_gametype"));
    cgs.ioqlServer = !Q_stricmp(Info_ValueForKey(info, "com_gamename"), "ioquakelive");
    trap_Cvar_Set("cg_gametype", va("%i", cgs.gametype));
    cgs.teamsize = atoi(Info_ValueForKey(info, "teamsize"));
    cgs.teamSizeMin = atoi(Info_ValueForKey(info, "g_teamSizeMin"));
    cgs.teamForceBalance = atoi(Info_ValueForKey(info, "g_teamForceBalance"));
    cgs.dmflags = atoi(Info_ValueForKey(info, "dmflags"));
    cgs.fraglimit = atoi(Info_ValueForKey(info, "fraglimit"));
    cgs.capturelimit = atoi(Info_ValueForKey(info, "capturelimit"));
    cgs.scorelimit = atoi(Info_ValueForKey(info, "scorelimit"));
    cgs.mercylimit = atoi(Info_ValueForKey(info, "mercylimit"));
    cgs.timelimit = atoi(Info_ValueForKey(info, "timelimit"));
    cgs.roundlimit = atoi(Info_ValueForKey(info, "roundlimit"));
    cgs.roundtimelimit = atoi(Info_ValueForKey(info, "roundtimelimit"));
    cgs.roundWarmupDelay = atoi(Info_ValueForKey(info, "g_roundWarmupDelay"));
    cgs.startingHealth = atoi(Info_ValueForKey(info, "g_startingHealth"));
    cgs.adCaptureScoreBonus = atoi(Info_ValueForKey(info, "g_adCaptureScoreBonus"));
    cgs.adElimScoreBonus = atoi(Info_ValueForKey(info, "g_adElimScoreBonus"));
    cgs.adtouchScoreBonus = atoi(Info_ValueForKey(info, "g_adtouchScoreBonus"));
    cgs.freezeRoundDelay = atoi(Info_ValueForKey(info, "g_freezeRoundDelay"));
    cgs.maxclients = atoi(Info_ValueForKey(info, "sv_maxclients"));
    cgs.timeoutCount = atoi(Info_ValueForKey(info, "g_timeoutCount"));
    cgs.timelimit_overtime = atoi(Info_ValueForKey(info, "g_overtime"));
    cgs.itemHeight = atoi(Info_ValueForKey(info, "g_itemHeight"));
    cgs.gravity = atoi(Info_ValueForKey(info, "g_gravity"));
    cgs.weaponRespawn = atoi(Info_ValueForKey(info, "g_weaponRespawn"));
    cgs.itemTimers = atoi(Info_ValueForKey(info, "g_itemTimers"));
    cgs.quadDamageFactor = atoi(Info_ValueForKey(info, "g_quadDamageFactor"));

    trap_Cvar_Set("cg_loadout", Info_ValueForKey(info, "g_loadout"));
    CG_UpdateDisableLoadout(atoi(Info_ValueForKey(info, "g_loadout")));

    mapname = Info_ValueForKey(info, "mapname");
    Com_sprintf(cgs.mapname, sizeof(cgs.mapname), "maps/%s.bsp", mapname);

    cgs.voteFlags = atoi(Info_ValueForKey(info, "g_voteFlags"));
    if (cgs.voteFlags & (VF_MAP | VF_NEXTMAP)) {
        trap_Cvar_Set("ui_mapVotingDisabled", "1");
    } else {
        trap_Cvar_Set("ui_mapVotingDisabled", "0");
    }
    if ((cgs.voteFlags & VF_ENDMAP_VOTING) || cgs.localServer) {
        trap_Cvar_Set("ui_endMapVotingDisabled", "1");
    } else {
        trap_Cvar_Set("ui_endMapVotingDisabled", "0");
    }
    if (cgs.voteFlags & VF_GAMETYPE) {
        trap_Cvar_Set("ui_gameTypeVotingDisabled", "1");
    } else {
        trap_Cvar_Set("ui_gameTypeVotingDisabled", "0");
    }
}

static void CG_ParseWarmup(void) {
    const char* info;
    const char* s;
    int warmup;

    info = CG_ConfigString(CS_WARMUP);

    warmup = atoi(Info_ValueForKey(info, "time"));

    s = Info_ValueForKey(info, "g_gametype");
    cg.warmupRound = 0;
    if (*s) {
        cg.warmupGametype = atoi(s);
    } else {
        cg.warmupGametype = -1;
    }

    if (warmup > 0 && cg.warmup < 1) {
        sfxHandle_t sfx = cgs.media.countPrepareTeamSound;

        if (cg.warmupGametype < GT_TEAM &&
            (cgs.gametype < GT_TEAM || cg.warmupGametype == GT_DUEL || cgs.gametype == GT_RR)) {
            sfx = cgs.media.countPrepareSound;
        }
        trap_S_StartLocalSoundVolume(sfx, CHAN_ANNOUNCER, s_announcerVolume.value);
    }

    cg.warmup = warmup;

    trap_Cvar_Set("ui_warmup", va("%i", warmup));
}

static void CG_ParseWarmupInfo(void) {
    const char* info;
    int warmup;

    info = CG_ConfigString(CS_ROUND_WARMUP);

    warmup = atoi(Info_ValueForKey(info, "time"));
    cg.warmupRound = atoi(Info_ValueForKey(info, "round"));
    cg.timelimitWarnings &= ~8;

    if (cgs.gametype == GT_AD) {
        cg.warmupTurn = atoi(Info_ValueForKey(info, "turn"));
        cg.warmupState = atoi(Info_ValueForKey(info, "state"));
        cg.warmupRound++;
    }

    cg.warmup = warmup;
}

static void CG_ParseArmorTiered(void) {
    const char* info;

    info = CG_ConfigString(CS_ARMORINFO);
    cgs.armorTiered = atoi(Info_ValueForKey(info, "armor_tiered"));
    trap_Cvar_Set("cg_armorTiered", va("%d", cgs.armorTiered));
}

void CG_ParseConfigParams(void) {
    const char* info;
    const char* val;

    info = CG_ConfigString(CS_PLAYERINFO);

    Q_strncpyz(cgs.playermodelOverride, Info_ValueForKey(info, "g_playermodelOverride"),
               sizeof(cgs.playermodelOverride));
    Q_strncpyz(cgs.playerheadmodelOverride, Info_ValueForKey(info, "g_playerheadmodelOverride"),
               sizeof(cgs.playerheadmodelOverride));
    cgs.allowCustomHeadmodels = atoi(Info_ValueForKey(info, "g_allowCustomHeadmodels"));

    val = Info_ValueForKey(info, "g_playerheadScale");       cgs.playerheadScale = *val ? atof(val) : 1.0f;
    val = Info_ValueForKey(info, "g_playerheadScaleOffset");  cgs.playerheadScaleOffset = *val ? atof(val) : 1.0f;
    val = Info_ValueForKey(info, "g_playerModelScale");       cgs.playerModelScale = *val ? atof(val) : 1.1f;

    CG_UpdateAllModelScales();
}

void CG_ParseWeaponInfo(void) {
    static const struct {
        int weapon;
        const char* key;
    } reloads[] = {
        {WP_GAUNTLET, "weapon_reload_gauntlet"},
        {WP_MACHINEGUN, "weapon_reload_mg"},
        {WP_SHOTGUN, "weapon_reload_sg"},
        {WP_GRENADE_LAUNCHER, "weapon_reload_gl"},
        {WP_ROCKET_LAUNCHER, "weapon_reload_rl"},
        {WP_LIGHTNING, "weapon_reload_lg"},
        {WP_RAILGUN, "weapon_reload_rg"},
        {WP_PLASMAGUN, "weapon_reload_pg"},
        {WP_BFG, "weapon_reload_bfg"},
        {WP_GRAPPLING_HOOK, "weapon_reload_hook"},
        {WP_NAILGUN, "weapon_reload_ng"},
        {WP_PROX_LAUNCHER, "weapon_reload_prox"},
        {WP_CHAINGUN, "weapon_reload_cg"},
        {WP_HMG, "weapon_reload_hmg"},
    };
    const char* info = CG_ConfigString(CS_WEAPONINFO);
    const char* val;
    int i;

    for (i = 0; i < ARRAY_LEN(reloads); i++) {
        val = Info_ValueForKey(info, reloads[i].key);
        if (val[0]) {
            bg_weaponReloadTime[reloads[i].weapon] = atoi(val);
        }
    }
}

void CG_ParsePmoveParams(void) {
    const char *info;
    const char *val;

    info = CG_ConfigString(CS_PMOVEINFO);
    if (!info[0]) {
        return;
    }

    val = Info_ValueForKey(info, "pmove_AirAccel");
    if (*val) PM_SetAirAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_AirStepFriction");
    if (*val) PM_SetAirStepFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_AirSteps");
    if (*val) PM_SetAirSteps(atoi(val));

    val = Info_ValueForKey(info, "pmove_AirStopAccel");
    if (*val) PM_SetAirStopAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_AirControl");
    if (*val) PM_SetAirControl(atof(val));

    val = Info_ValueForKey(info, "pmove_AutoHop");
    if (*val) PM_SetAutoHop(atoi(val));

    val = Info_ValueForKey(info, "pmove_BunnyHop");
    if (*val) PM_SetBunnyHop(atoi(val));

    val = Info_ValueForKey(info, "pmove_ChainJump");
    if (*val) PM_SetChainJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_ChainJumpVelocity");
    if (*val) PM_SetChainJumpVelocity(atof(val));

    val = Info_ValueForKey(info, "pmove_CircleStrafeFriction");
    if (*val) PM_SetCircleStrafeFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_CrouchSlideFriction");
    if (*val) PM_SetCrouchSlideFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_CrouchSlideTime");
    if (*val) PM_SetCrouchSlideTime(atoi(val));

    val = Info_ValueForKey(info, "pmove_CrouchSlide");
    if (*val) PM_SetCrouchSlide(atoi(val));

    val = Info_ValueForKey(info, "pmove_CrouchStepJump");
    if (*val) PM_SetCrouchStepJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_DoubleJump");
    if (*val) PM_SetDoubleJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_JumpTimeDeltaMin");
    if (*val) PM_SetJumpTimeDeltaMin(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocity");
    if (*val) PM_SetJumpVelocity(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityMax");
    if (*val) PM_SetJumpVelocityMax(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityScaleAdd");
    if (*val) PM_SetJumpVelocityScaleAdd(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityTimeThreshold");
    if (*val) PM_SetJumpVelocityTimeThreshold(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityTimeThresholdOffset");
    if (*val) PM_SetJumpVelocityTimeThresholdOffset(atof(val));

    val = Info_ValueForKey(info, "pmove_noPlayerClip");
    if (*val) PM_SetNoPlayerClip(atoi(val));

    val = Info_ValueForKey(info, "pmove_RampJump");
    if (*val) PM_SetRampJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_RampJumpScale");
    if (*val) PM_SetRampJumpScale(atof(val));

    val = Info_ValueForKey(info, "pmove_StepHeight");
    if (*val) PM_SetStepHeight(atof(val));

    val = Info_ValueForKey(info, "pmove_StepJump");
    if (*val) PM_SetStepJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_StepJumpVelocity");
    if (*val) PM_SetStepJumpVelocity(atof(val));

    val = Info_ValueForKey(info, "pmove_StrafeAccel");
    if (*val) PM_SetStrafeAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_velocity_gh");
    if (*val) PM_SetVelocityGH(atof(val));

    val = Info_ValueForKey(info, "pmove_WalkAccel");
    if (*val) PM_SetWalkAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_WalkFriction");
    if (*val) PM_SetWalkFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_WaterSwimScale");
    if (*val) PM_SetWaterSwimScale(atof(val));

    val = Info_ValueForKey(info, "pmove_WaterWadeScale");
    if (*val) PM_SetWaterWadeScale(atof(val));

    val = Info_ValueForKey(info, "pmove_WeaponRaiseTime");
    if (*val) PM_SetWeaponRaiseTime(atoi(val));

    val = Info_ValueForKey(info, "pmove_WeaponDropTime");
    if (*val) PM_SetWeaponDropTime(atoi(val));

    val = Info_ValueForKey(info, "pmove_WishSpeed");
    if (*val) PM_SetWishSpeed(atof(val));
}

void CG_SetConfigValues(void) {
    const char* s;

    cgs.scores1 = atoi(CG_ConfigString(CS_SCORES1));
    cgs.scores2 = atoi(CG_ConfigString(CS_SCORES2));
    Q_strncpyz(cgs.firstPlaceName, CG_ConfigString(CS_SCORES1PLAYER), sizeof(cgs.firstPlaceName));
    Q_strncpyz(cgs.secondPlaceName, CG_ConfigString(CS_SCORES2PLAYER), sizeof(cgs.secondPlaceName));
    if (cgs.gametype == GT_DUEL) {
        cgs.clientNum1stPlayer = atoi(CG_ConfigString(CS_CLIENTNUM1STPLAYER));
        cgs.clientNum2ndPlayer = atoi(CG_ConfigString(CS_CLIENTNUM2NDPLAYER));
        cgs.score1stPlayer = atoi(CG_ConfigString(CS_SCORE1STPLAYER));
        cgs.score2ndPlayer = atoi(CG_ConfigString(CS_SCORE2NDPLAYER));
        Q_strncpyz(cgs.name1stPlayer, CG_ConfigString(CS_NAME1STPLAYER), sizeof(cgs.name1stPlayer));
        Q_strncpyz(cgs.name2ndPlayer, CG_ConfigString(CS_NAME2NDPLAYER), sizeof(cgs.name2ndPlayer));
    }
    cgs.levelStartTime = atoi(CG_ConfigString(CS_LEVEL_START_TIME));
    if (cgs.gametype >= GT_TEAM) {
        cgs.teamCountRed = atoi(CG_ConfigString(CS_TEAMCOUNT_RED));
        cgs.teamCountBlue = atoi(CG_ConfigString(CS_TEAMCOUNT_BLUE));
    }
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));
    cgs.dmgThroughDepth = atof(CG_ConfigString(CS_DMGTHROUGHDEPTH));
    cgs.playerCylinders = atoi(CG_ConfigString(CS_PLAYER_CYLINDERS));
    cgs.freezeEnd = atoi(CG_ConfigString(CS_PAUSE_START_TIME));
    cgs.pauseEnd = atoi(CG_ConfigString(CS_PAUSE_END_TIME));
    cgs.practice = atoi(CG_ConfigString(CS_PRACTICE));
    cgs.freecam = atoi(CG_ConfigString(CS_FREECAM));

    CG_ParsePmoveParams();
    CG_ParseConfigParams();
    CG_ParseWeaponInfo();
    if (cgs.gametype == GT_CTF) {
        s = CG_ConfigString(CS_FLAGSTATUS);
        cgs.redflag = s[0] - '0';
        cgs.blueflag = s[1] - '0';
    } else if (cgs.gametype == GT_1FCTF) {
        s = CG_ConfigString(CS_FLAGSTATUS);
        cgs.flagStatus = s[0] - '0';
    }
    cg.warmup = atoi(Info_ValueForKey(CG_ConfigString(CS_WARMUP), "time"));

    {
        const char* gt = Info_ValueForKey(CG_ConfigString(CS_WARMUP), "g_gametype");

        cg.warmupGametype = *gt ? atoi(gt) : -1;
    }
    CG_UpdateDisableLoadout(atoi(Info_ValueForKey(CG_ConfigString(CS_SERVERINFO), "g_loadout")));
}

void CG_ShaderStateChanged(void) {
    char originalShader[MAX_QPATH];
    char newShader[MAX_QPATH];
    char timeOffset[16];
    const char* o;
    char *n, *t;

    o = CG_ConfigString(CS_SHADERSTATE);
    while (o && *o) {
        n = strstr(o, "=");
        if (n && *n) {
            strncpy(originalShader, o, n - o);
            originalShader[n - o] = 0;
            n++;
            t = strstr(n, ":");
            if (t && *t) {
                strncpy(newShader, n, t - n);
                newShader[t - n] = 0;
            } else {
                break;
            }
            t++;
            o = strstr(t, "@");
            if (o) {
                strncpy(timeOffset, t, o - t);
                timeOffset[o - t] = 0;
                o++;
                trap_R_RemapShader(originalShader, newShader, timeOffset);
            }
        } else {
            break;
        }
    }
}

static void CG_ConfigStringModified(void) {
    const char* str;
    int num;

    num = atoi(CG_Argv(1));

    trap_GetGameState(&cgs.gameState);

    str = CG_ConfigString(num);

    if (num == CS_MUSIC) {
        CG_StartMusic();
    } else if (num == CS_SERVERINFO) {
        CG_ParseServerinfo();
    } else if (num == CS_WARMUP) {
        CG_ParseWarmup();
    } else if (num == CS_SCORES1) {
        cgs.scores1 = atoi(str);
    } else if (num == CS_SCORES2) {
        cgs.scores2 = atoi(str);
    } else if (num == CS_SCORES1PLAYER) {
        Q_strncpyz(cgs.firstPlaceName, str, sizeof(cgs.firstPlaceName));
    } else if (num == CS_SCORES2PLAYER) {
        Q_strncpyz(cgs.secondPlaceName, str, sizeof(cgs.secondPlaceName));
    } else if (num == CS_LEVEL_START_TIME) {
        int levelStartTime = atoi(str);
        if (cgs.gametype == GT_RACE && cg.race.active == 1) {
            cg.race.startTime += levelStartTime - cgs.levelStartTime;
        }
        cgs.levelStartTime = levelStartTime;
    } else if (num == CS_VOTE_TIME) {
        cgs.voteTime = atoi(str);
        cgs.voteModified = qtrue;
        if (cgs.voteTime && !cgs.localServer) {
            trap_Cvar_Set("ui_voteactive", "1");
        } else {
            trap_Cvar_Set("ui_voteactive", "0");
        }
    } else if (num == CS_VOTE_YES) {
        cgs.voteYes = atoi(str);
        cgs.voteModified = qtrue;
    } else if (num == CS_VOTE_NO) {
        cgs.voteNo = atoi(str);
        cgs.voteModified = qtrue;
    } else if (num == CS_VOTE_STRING) {
        Q_strncpyz(cgs.voteString, str, sizeof(cgs.voteString));
        trap_Cvar_Set("ui_votestring", cgs.voteString);
        if (cgs.voteString[0]) {
            CG_AddBufferedSound(cgs.media.voteNow);
        }
    } else if (num == CS_INTERMISSION) {
        cg.intermissionStarted = atoi(str);
        if (cg.intermissionStarted == 1) {
            trap_Cvar_Set("ui_intermission", "1");
        }
    } else if (num >= CS_MODELS && num < CS_MODELS + MAX_MODELS) {
        cgs.gameModels[num - CS_MODELS] = trap_R_RegisterModel(str);
    } else if (num >= CS_SOUNDS && num < CS_SOUNDS + MAX_SOUNDS) {
        if (str[0] != '*') {
            cgs.gameSounds[num - CS_SOUNDS] = trap_S_RegisterSound(str, qfalse);
        }
    } else if (num >= CS_PLAYERS && num < CS_PLAYERS + MAX_CLIENTS) {
        if (num - CS_PLAYERS == cg.clientNum) {
            cg.reloadOtherClients = qtrue;
        }
        CG_NewClientInfo(num - CS_PLAYERS);
        CG_BuildSpectatorString();
    } else if (num == CS_FLAGSTATUS) {
        if (cgs.gametype == GT_CTF) {
            cgs.redflag = str[0] - '0';
            cgs.blueflag = str[1] - '0';
        } else if (cgs.gametype == GT_1FCTF) {
            cgs.flagStatus = str[0] - '0';
        }
    } else if (num == CS_CLIENTNUM1STPLAYER) {
        cgs.clientNum1stPlayer = atoi(str);
        cg.duelPlayer1 = cgs.clientNum1stPlayer;
    } else if (num == CS_CLIENTNUM2NDPLAYER) {
        cgs.clientNum2ndPlayer = atoi(str);
        cg.duelPlayer2 = cgs.clientNum2ndPlayer;
    } else if (num == CS_SCORE1STPLAYER) {
        cgs.score1stPlayer = atoi(str);
    } else if (num == CS_SCORE2NDPLAYER) {
        cgs.score2ndPlayer = atoi(str);
    } else if (num == CS_NAME1STPLAYER) {
        Q_strncpyz(cgs.name1stPlayer, str, sizeof(cgs.name1stPlayer));
    } else if (num == CS_NAME2NDPLAYER) {
        Q_strncpyz(cgs.name2ndPlayer, str, sizeof(cgs.name2ndPlayer));
    } else if (num == CS_ROUND_START_TIME) {
        cgs.roundStartTime = atoi(str);
        if (cgs.roundStartTime) {
            cgs.roundStarted = qtrue;
        }
    } else if (num == CS_ROUND_WARMUP) {
        CG_ParseWarmupInfo();
    } else if (num == CS_TEAMCOUNT_RED) {
        cgs.teamCountRed = atoi(str);
    } else if (num == CS_TEAMCOUNT_BLUE) {
        cgs.teamCountBlue = atoi(str);
    } else if (num == CS_ARMORINFO) {
        CG_ParseArmorTiered();
    } else if (num == CS_PLAYERINFO) {
        CG_ParseConfigParams();
    } else if (num == CS_ROTATIONMAPS) {
        cg.mapVoteActive = (str[0] != '\0');
    } else if (num == CS_SHADERSTATE) {
        CG_ShaderStateChanged();
    } else if (num == CS_PMOVEINFO) {
        CG_ParsePmoveParams();
    } else if (num == CS_WEAPONINFO) {
        CG_ParseWeaponInfo();
    } else if (num == CS_CUSTOM_SETTINGS) {
        cgs.customSettings = atoi(str);
    } else if (num == CS_DMGTHROUGHDEPTH) {
        cgs.dmgThroughDepth = atof(str);
    } else if (num == CS_PLAYER_CYLINDERS) {
        cgs.playerCylinders = atoi(str);
    } else if (num == CS_PAUSE_START_TIME) {
        cgs.freezeEnd = atoi(str);
    } else if (num == CS_PAUSE_END_TIME) {
        cgs.pauseEnd = atoi(str);
    } else if (num == CS_PRACTICE) {
        cgs.practice = atoi(str);
    } else if (num == CS_FREECAM) {
        cgs.freecam = atoi(str);
    } else if (num == CS_ALLREADY_TIME) {
        cgs.allReadyTime = atoi(str);
    } else if (num == CS_DISABLE_LOADOUT || num == CS_STARTING_WEAPONS) {
        CG_UpdateDisableLoadout(atoi(Info_ValueForKey(CG_ConfigString(CS_SERVERINFO), "g_loadout")));
    }
}

static void CG_AddToTeamChat(const char* str) {
    int len;
    char *p, *ls;
    int lastcolor;
    int chatHeight;

    if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT) {
        chatHeight = cg_teamChatHeight.integer;
    } else {
        chatHeight = TEAMCHAT_HEIGHT;
    }

    if (chatHeight <= 0 || cg_teamChatTime.integer <= 0) {
        cgs.teamChatPos = cgs.teamLastChatPos = 0;
        return;
    }

    len = 0;

    p = cgs.teamChatMsgs[cgs.teamChatPos % chatHeight];
    *p = 0;

    lastcolor = '7';

    ls = NULL;
    while (*str) {
        if (len > TEAMCHAT_WIDTH - 1) {
            if (ls) {
                str -= (p - ls);
                str++;
                p -= (p - ls);
            }
            *p = 0;

            cgs.teamChatMsgTimes[cgs.teamChatPos % chatHeight] = cg.time;

            cgs.teamChatPos++;
            p = cgs.teamChatMsgs[cgs.teamChatPos % chatHeight];
            *p = 0;
            *p++ = Q_COLOR_ESCAPE;
            *p++ = lastcolor;
            len = 0;
            ls = NULL;
        }

        if (Q_IsColorString(str)) {
            *p++ = *str++;
            lastcolor = *str;
            *p++ = *str++;
            continue;
        }
        if (*str == ' ') {
            ls = p;
        }
        *p++ = *str++;
        len++;
    }
    *p = 0;

    cgs.teamChatMsgTimes[cgs.teamChatPos % chatHeight] = cg.time;
    cgs.teamChatPos++;

    if (cgs.teamChatPos - cgs.teamLastChatPos > chatHeight)
        cgs.teamLastChatPos = cgs.teamChatPos - chatHeight;
}

#define MAX_VOICEFILESIZE 16384
#define MAX_VOICECHATS    64
#define MAX_VOICESOUNDS   64
#define MAX_CHATSIZE      64

typedef struct {
    char        id[64];
    int         numSounds;
    sfxHandle_t sounds[MAX_VOICESOUNDS];
    char        chats[MAX_VOICESOUNDS][MAX_CHATSIZE];
} qlVoiceChat_t;

typedef struct {
    char          name[64];
    int           gender;
    int           numVoiceChats;
    qlVoiceChat_t voiceChats[MAX_VOICECHATS];
} qlVoiceFile_t;

typedef struct {
    char name[64];
    int  index;
} qlVoiceNameCache_t;

static qlVoiceFile_t       voiceFiles[MAX_VOICEFILES];
static qlVoiceNameCache_t  voiceChatLists[MAX_VOICECHATS];
static bufferedVoiceChat_t voiceChatBuffer[MAX_VOICECHATBUFFER];
static qlVoiceFile_t*      cg_currentVoiceChatList;

extern vmCvar_t cg_playVoiceChats;
extern vmCvar_t cg_showVoiceText;

void CG_LoadVoiceChats(void) {
    memset(voiceChatLists, 0, sizeof(voiceChatLists));
    memset(voiceFiles, 0, sizeof(voiceFiles));
    cg_currentVoiceChatList = NULL;
}

int CG_ParseVoiceChats(const char* filename) {
    fileHandle_t f;
    int len;
    char buf[MAX_VOICEFILESIZE];
    char* p;
    const char* token;
    int i;

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        return -1;
    }
    if (len >= (int)sizeof(buf)) {
        CG_Printf("^1voice chat file too large: %s is %i, max allowed is %i\n",
                  filename, len, MAX_VOICEFILESIZE);
        trap_FS_FCloseFile(f);
        return -1;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);

    p = buf;
    token = COM_ParseExt(&p, qtrue);
    if (!token || !token[0]) {
        return -1;
    }

    for (i = 0; i < MAX_VOICEFILES; i++) {
        if (!Q_stricmpn(token, voiceFiles[i].name, sizeof(voiceFiles[i].name))) {
            return i;
        }
    }
    return -1;
}

qboolean CG_GetVoiceChat(const char* cmd, sfxHandle_t* snd, const char** chat) {
    qlVoiceFile_t* list = cg_currentVoiceChatList;
    int i, rnd;

    if (!list) {
        return qfalse;
    }

    for (i = 0; i < list->numVoiceChats; i++) {
        if (cmd == NULL) {
            continue;
        }
        if (!Q_stricmp(cmd, list->voiceChats[i].id)) {
            rnd = (int)(((float)(rand() & 0x7fff) / 32768.0f) * list->voiceChats[i].numSounds);
            *snd = list->voiceChats[i].sounds[rnd];
            *chat = list->voiceChats[i].chats[rnd];
            return qtrue;
        }
    }
    return qfalse;
}

void CG_VoiceChatListForClient(int clientNum) {
    clientInfo_t* ci;
    char vchatName[64];
    char vchatFile[64];
    int pass;
    int i, j;
    int vchatIndex;
    const char* name;
    int searchGender;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = 0;
    }
    ci = &cgs.clientinfo[clientNum];

    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            if (ci->forcedHeadModel[0] == '*') {
                Com_sprintf(vchatName, sizeof(vchatName), "%s/%s",
                            ci->forcedHeadModel + 1, ci->forcedHeadSkin);
            } else {
                Com_sprintf(vchatName, sizeof(vchatName), "%s/%s",
                            ci->forcedHeadModel, ci->forcedHeadSkin);
            }
        } else {
            if (ci->forcedHeadModel[0] == '*') {
                name = ci->forcedHeadModel + 1;
            } else {
                name = ci->forcedHeadModel;
            }
            Com_sprintf(vchatName, sizeof(vchatName), "%s", name);
        }

        for (i = 0; i < MAX_VOICECHATS; i++) {
            if (voiceChatLists[i].name[0] &&
                !Q_stricmp(vchatName, voiceChatLists[i].name)) {
                cg_currentVoiceChatList = &voiceFiles[voiceChatLists[i].index];
                return;
            }
        }

        for (i = 0; i < MAX_VOICECHATS; i++) {
            if (!voiceChatLists[i].name[0]) {
                Com_sprintf(vchatFile, sizeof(vchatFile), "scripts/%s.vc", vchatName);
                vchatIndex = CG_ParseVoiceChats(vchatFile);
                if (vchatIndex != -1) {
                    Q_strncpyz(voiceChatLists[i].name, vchatName,
                               sizeof(voiceChatLists[i].name));
                    voiceChatLists[i].index = vchatIndex;
                    cg_currentVoiceChatList = &voiceFiles[vchatIndex];
                    return;
                }
                break;
            }
        }
    }

    searchGender = ci->gender;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < MAX_VOICEFILES; i++) {
            if (voiceFiles[i].name[0] && voiceFiles[i].gender == searchGender) {
                for (j = 0; j < MAX_VOICECHATS; j++) {
                    if (!voiceChatLists[j].name[0]) {
                        Q_strncpyz(voiceChatLists[j].name, vchatName,
                                   sizeof(voiceChatLists[j].name));
                        voiceChatLists[j].index = i;
                        break;
                    }
                }
                cg_currentVoiceChatList = &voiceFiles[i];
                return;
            }
        }
        if (searchGender == 0) {
            break;
        }
        searchGender = 0;
    }

    for (i = 0; i < MAX_VOICECHATS; i++) {
        if (!voiceChatLists[i].name[0]) {
            Q_strncpyz(voiceChatLists[i].name, vchatName,
                       sizeof(voiceChatLists[i].name));
            voiceChatLists[i].index = 0;
            break;
        }
    }
    cg_currentVoiceChatList = &voiceFiles[0];
}

void CG_PlayVoiceChat(bufferedVoiceChat_t* vchat) {
    int orderTask;

    if (cg.intermissionStarted == 1) {
        return;
    }

    if (cg_playVoiceChats.integer) {
        trap_S_StartLocalSound(vchat->snd, CHAN_VOICE);

        if (vchat->clientNum != cg.snap->ps.clientNum) {
            orderTask = CG_ValidOrder(vchat->cmd);
            if (orderTask > 0) {
                cgs.acceptOrderTime = cg.time + 5000;
                Q_strncpyz(cgs.acceptVoice, vchat->cmd, sizeof(cgs.acceptVoice));
                cgs.acceptTask = orderTask;
                cgs.currentOrder = vchat->clientNum;
            }
            Menus_ActivateByName("voiceMenu");
            cg.voiceTime = cg.time;
        }
    }

    if (vchat->voiceOnly == qfalse && cg_showVoiceText.integer) {
        CG_AddToTeamChat(vchat->message);
        CG_Printf("%s\n", vchat->message);
    }

    voiceChatBuffer[cg.voiceChatBufferOut].snd = 0;
}

void CG_AddBufferedVoiceChat(bufferedVoiceChat_t* vchat) {
    if (cg.intermissionStarted == 1) {
        return;
    }

    memcpy(&voiceChatBuffer[cg.voiceChatBufferIn], vchat, sizeof(bufferedVoiceChat_t));
    cg.voiceChatBufferIn = (cg.voiceChatBufferIn + 1) % MAX_VOICECHATBUFFER;

    if (cg.voiceChatBufferIn == cg.voiceChatBufferOut) {
        CG_PlayVoiceChat(&voiceChatBuffer[cg.voiceChatBufferOut]);
        cg.voiceChatBufferOut++;
    }
}

void CG_PlayBufferedVoiceChats(void) {
    if (cg.voiceChatTime < cg.time &&
        cg.voiceChatBufferOut != cg.voiceChatBufferIn &&
        voiceChatBuffer[cg.voiceChatBufferOut].snd != 0) {
        CG_PlayVoiceChat(&voiceChatBuffer[cg.voiceChatBufferOut]);
        cg.voiceChatBufferOut = (cg.voiceChatBufferOut + 1) % MAX_VOICECHATBUFFER;
        cg.voiceChatTime = cg.time + 1000;
    }
}

void CG_VoiceChatLocal(int clientNum, const char* cmd) {
    bufferedVoiceChat_t vchat;
    sfxHandle_t snd;
    const char* chatStr;

    if (cg.intermissionStarted == 1) {
        return;
    }

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = 0;
    }
    cgs.currentVoiceClient = clientNum;

    CG_VoiceChatListForClient(clientNum);

    if (!CG_GetVoiceChat(cmd, &snd, &chatStr)) {
        return;
    }

    vchat.clientNum = clientNum;
    vchat.snd = snd;
    vchat.voiceOnly = qfalse;
    Q_strncpyz(vchat.cmd, cmd, sizeof(vchat.cmd));
    Com_sprintf(vchat.message, sizeof(vchat.message), "(%s): ^5%s",
                cgs.clientinfo[clientNum].name, chatStr);

    CG_AddBufferedVoiceChat(&vchat);
}

static void CG_MapRestart(void) {
    if (cg_showmiss.integer) {
        CG_Printf("CG_MapRestart\n");
    }

    CG_InitLocalEntities();
    CG_InitMarkPolys();
    CG_ClearParticles();

    cg.damageTime = 0;
    cg.intermissionStarted = qfalse;
    cg.fraglimitWarnings = 0;
    cg.timelimitWarnings = 0;
    cg.mapRestart = qtrue;
    cg.soundBufferLocked = qfalse;
    cg.itemPickup = 0;
    cg.itemPickupTime = 0;
    cg.itemPickupLast = 0;
    cg.itemPickupBlendTime = 0;
    cg.itemPickupCount = 1;
    cg.levelShot = qfalse;
    cgs.voteTime = 0;
    cg.complaintClient = -1;
    cg.complaintEndTime = 0;
    cgs.allReadyTime = 0;

    CG_ResetAnnouncements();
    CG_StartMusic();
    trap_S_ClearLoopingSounds(qtrue);
    memset(cg.obituaries, 0, sizeof(cg.obituaries));
    memset(&cg.race, 0, sizeof(cg.race));
    cg.obitSlideStart = 0;
    cg.obitSlideKey = 0;

    if (!cg.warmup && !BG_IsRoundBasedGameType(cgs.gametype)) {
        CG_PlayFightSound();
    }
    trap_Cvar_Set("cg_thirdPerson", "0");
    trap_Cvar_Set("ui_voteactive", "0");
    CG_ResetSpecItemTimers();
}

static void CG_RemoveChatEscapeChar(char* text) {
    int i, l;

    l = 0;
    for (i = 0; text[i]; i++) {
        if (text[i] == '\x19')
            continue;
        text[l++] = text[i];
    }
    text[l] = '\0';
}

void CG_ParseAccuracy(void) {
    int i;
    for (i = 0; i < MAX_WEAPONS && i < trap_Argc() - 1; i++) {
        cg.accuracyStats.accuracy[i] = atoi(CG_Argv(i + 1));
    }
    cg.accuracyStats.time = cg.time;
    cg.accuracyStats.clientNum = cg.snap ? cg.snap->ps.clientNum : 0;
    cg.accuracyStats.valid = qtrue;
}

static void CG_DuelScoreToScore(score_t *sp, const duelScore_t *ds) {
    sp->client = ds->clientNum;
    if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
        sp->client = 0;
    }
    sp->score = ds->score;
    sp->ping = ds->ping;
    sp->time = ds->time;
    sp->frags = ds->kills;
    sp->deaths = ds->deaths;
    sp->accuracy = ds->accuracy;
    sp->bestWeapon = ds->bestWeapon;
    sp->damageDone = ds->damage;
    sp->perfect = ds->perfect;
    sp->impressiveCount = ds->awardImpressive;
    sp->excellentCount = ds->awardExcellent;
    sp->guantletCount = ds->awardHumiliation;
    sp->net = ds->kills - ds->deaths;
    sp->team = cgs.clientinfo[sp->client].team;
}

void CG_ParseDuelScores(void) {
    int j, idx;
    int player;
    int slot;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }

    memset(cg.scores, 0, sizeof(cg.scores));

    if (cg.warmup) {
        memset(cg.duelScores, 0, sizeof(cg.duelScores));
        cg.duelScores[0].clientNum = -1;
        cg.duelScores[1].clientNum = -1;
    }

    idx = 2;
    slot = 0;
    for (player = 0; player < cg.numScores; player++) {
        duelScore_t *ds;

        if (cg.intermissionStarted == 1 && cg.numScores < 2 &&
            atoi(CG_Argv(2)) != cg.duelScores[0].clientNum) {
            if (slot < MAX_CLIENTS) {
                CG_DuelScoreToScore(&cg.scores[slot], &cg.duelScores[0]);
            }
            slot++;
        }

        ds = &cg.duelScores[slot < 2 ? slot : 1];
        ds->clientNum = atoi(CG_Argv(idx++));
        ds->score = atoi(CG_Argv(idx++));
        ds->ping = atoi(CG_Argv(idx++));
        ds->time = atoi(CG_Argv(idx++));
        ds->kills = atoi(CG_Argv(idx++));
        ds->deaths = atoi(CG_Argv(idx++));
        ds->accuracy = atoi(CG_Argv(idx++));
        ds->bestWeapon = atoi(CG_Argv(idx++));
        ds->damage = atoi(CG_Argv(idx++));
        ds->awardImpressive = atoi(CG_Argv(idx++));
        ds->awardExcellent = atoi(CG_Argv(idx++));
        ds->awardHumiliation = atoi(CG_Argv(idx++));
        ds->perfect = atoi(CG_Argv(idx++));
        ds->redArmorPickups = atoi(CG_Argv(idx++));
        ds->redArmorTime = atof(CG_Argv(idx++));
        ds->yellowArmorPickups = atoi(CG_Argv(idx++));
        ds->yellowArmorTime = atof(CG_Argv(idx++));
        ds->greenArmorPickups = atoi(CG_Argv(idx++));
        ds->greenArmorTime = atof(CG_Argv(idx++));
        ds->megaHealthPickups = atoi(CG_Argv(idx++));
        ds->megaHealthTime = atof(CG_Argv(idx++));
        for (j = 0; j < 14; j++) {
            if (idx >= trap_Argc()) break;
            ds->weaponStats[j].hits = atoi(CG_Argv(idx++));
            ds->weaponStats[j].atts = atoi(CG_Argv(idx++));
            ds->weaponStats[j].accuracy = atoi(CG_Argv(idx++));
            ds->weaponStats[j].damage = atoi(CG_Argv(idx++));
            ds->weaponStats[j].kills = atoi(CG_Argv(idx++));
        }

        if (slot < MAX_CLIENTS) {
            CG_DuelScoreToScore(&cg.scores[slot], ds);
        }
        slot++;
    }

    cg.duelScoresValid = qtrue;
    CG_SetScoreboardSelection();
}

void CG_InitScores(void) {
    int i;

    memset(cg.adScores, 0, sizeof(cg.adScores));
    cg.teamScores[0] = 0;
    cg.teamScores[1] = 0;

    for (i = 0; i < 20; i++) {
        cg.adScores[i] = atoi(CG_Argv(i + 1));
    }
    cg.teamScores[0] = atoi(CG_Argv(21));
    cg.teamScores[1] = atoi(CG_Argv(22));
}

static void CG_ServerCommand(void) {
    const char* cmd;
    char text[MAX_SAY_TEXT];

    cmd = CG_Argv(0);

    if (!cmd[0]) {
        return;
    }

    if (!strcmp(cmd, "cs")) {
        CG_ConfigStringModified();
        return;
    }

    if (!strcmp(cmd, "screenshot")) {
        if (!cg.demoPlayback) {
            trap_SendConsoleCommand(va("screenshotJPEG \"%s\"\n", CG_Argv(1)));
        }
        return;
    }
    if (!strcmp(cmd, "record")) {
        if (!cg.demoPlayback) {
            trap_SendConsoleCommand(va("record \"%s\"\n", CG_Argv(1)));
        }
        return;
    }
    if (!strcmp(cmd, "stoprecord")) {
        if (!cg.demoPlayback) {
            trap_SendConsoleCommand("stoprecord; wait\n");
        }
        return;
    }

    if (!strcmp(cmd, "cp")) {
        CG_CenterPrint(CG_Argv(1), cg.warmup ? 144 : 90, 0.4f);
        return;
    }

    if (!strcmp(cmd, "pcp")) {
        CG_CenterPrint(CG_Argv(1), 90, 0.4f);
        CG_Printf("%s\n", CG_Argv(1));
        return;
    }

    if (!strcmp(cmd, "print")) {
        CG_Printf("%s", CG_Argv(1));
        cmd = CG_Argv(1);
        if (!Q_stricmpn(cmd, "vote failed", 11) || !Q_stricmpn(cmd, "team vote failed", 16)) {
            CG_AddBufferedSound(cgs.media.voteFailed);
        } else if (!Q_stricmpn(cmd, "vote passed", 11) || !Q_stricmpn(cmd, "team vote passed", 16)) {
            CG_AddBufferedSound(cgs.media.votePassed);
        }
        CG_AddChat(CG_Argv(1), 0, 0);
        return;
    }

    if (!strcmp(cmd, "chat")) {
        if (cgs.gametype >= GT_TEAM && cg_teamChatsOnly.integer) {
            return;
        }
        if (cg_chatbeep.integer) {
            trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        }
        {
            const char* p = CG_Argv(1);
            while (*p >= '0' && *p <= '9') { p++; }
            if (*p == ' ') { p++; }
            Q_strncpyz(text, p, MAX_SAY_TEXT);
        }
        CG_RemoveChatEscapeChar(text);
        CG_Printf("%s\n", text);
        CG_AddChat(text, 0, 0);
        return;
    }

    if (!strcmp(cmd, "bchat")) {
        trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        Q_strncpyz(text, CG_Argv(1), MAX_SAY_TEXT);
        CG_RemoveChatEscapeChar(text);
        CG_Printf("%s\n", text);
        CG_AddChat(text, 0, atoi(CG_Argv(2)) * 1000);
        return;
    }

    if (!strcmp(cmd, "clearChat")) {
        memset(cgs.teamChatMsgs, 0, sizeof(cgs.teamChatMsgs));
        cgs.teamChatPos = 0;
        CG_ClearChat();
        return;
    }

    if (!strcmp(cmd, "playSound")) {
        sfxHandle_t sfx = trap_S_RegisterSound(CG_Argv(1), qfalse);
        float volume = cgs.localServer ? 1.0f : s_playSoundVolume.value;

        if (sfx && volume > 0.0f) {
            trap_S_StartLocalSoundVolume(sfx, CHAN_LOCAL_SOUND, volume);
        }
        return;
    }

    if (!strcmp(cmd, "playMusic")) {
        trap_S_StartBackgroundTrack(CG_Argv(1), CG_Argv(2));
        return;
    }
    if (!strcmp(cmd, "stopMusic")) {
        trap_S_StopBackgroundTrack();
        return;
    }

    if (!strcmp(cmd, "clearSounds")) {
        trap_S_ClearSoundBuffer();
        return;
    }

    if (!strcmp(cmd, "tchat")) {
        if (cg_teamChatBeep.integer) {
            trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        }
        {
            const char* p = CG_Argv(1);
            while (*p >= '0' && *p <= '9') { p++; }
            if (*p == ' ') { p++; }
            Q_strncpyz(text, p, MAX_SAY_TEXT);
        }
        CG_RemoveChatEscapeChar(text);
        CG_Printf("%s\n", text);
        CG_AddToTeamChat(text);
        CG_AddChat(text, 1, 0);
        return;
    }

    if (!strcmp(cmd, "priv")) {
        trap_Cvar_Set("ui_priv", CG_Argv(1));
        return;
    }

    if (!strcmp(cmd, "scores_ffa")) {
        CG_ParseScores_Ffa();
        return;
    }
    if (!strcmp(cmd, "scores_tdm")) {
        CG_ParseScores_Tdm();
        return;
    }
    if (!strcmp(cmd, "scores_ca")) {
        CG_ParseScores_Ca();
        return;
    }
    if (!strcmp(cmd, "scores_ctf")) {
        CG_ParseScores_Ctf();
        return;
    }
    if (!strcmp(cmd, "scores_ft")) {
        CG_ParseScores_Ft();
        return;
    }
    if (!strcmp(cmd, "scores_rr")) {
        CG_ParseScores_Rr();
        return;
    }
    if (!strcmp(cmd, "scores_race")) {
        CG_ParseScores_Race();
        return;
    }
    if (!strcmp(cmd, "smscores")) {
        CG_ParseSmScores();
        return;
    }
    if (!strcmp(cmd, "scores")) {
        CG_ParseScores();
        return;
    }

    if (!strcmp(cmd, "tdmstats")) {
        CG_ParseTeamStats_TDM();
        return;
    }
    if (!strcmp(cmd, "castats")) {
        CG_ParseTeamStats_CA();
        return;
    }
    if (!strcmp(cmd, "ctfstats")) {
        CG_ParseTeamStats_CTF();
        return;
    }
    if (!strcmp(cmd, "acc")) {
        CG_ParseAccuracy();
        return;
    }
    if (!strcmp(cmd, "scores_duel")) {
        CG_ParseDuelScores();
        return;
    }
    if (!strcmp(cmd, "scores_ad") || !strcmp(cmd, "adscores")) {
        CG_InitScores();
        return;
    }
    if (!strcmp(cmd, "pstats")) {
        return;
    }

    if (!strcmp(cmd, "tinfo")) {
        CG_ParseTeamInfo();
        return;
    }

    if (!strcmp(cmd, "race_info")) {
        cg.race.active = (atoi(CG_Argv(1)) != 0);
        cg.race.startTime = atoi(CG_Argv(2));
        cg.race.lastTime = atoi(CG_Argv(3));
        cg.race.currentCheckPoint = atoi(CG_Argv(4));
        cg.race.nextCheckpointEnt = atoi(CG_Argv(5));
        cg.race.nextNextCheckpointEnt = atoi(CG_Argv(6));
        return;
    }
    if (!strcmp(cmd, "race_init")) {
        CG_RaceInit(1);
        return;
    }

    if (!Q_stricmpn(cmd, "complaint", 9)) {
        if (!cg.warmup) {
            cg.complaintClient = atoi(CG_Argv(1));
            cg.complaintEndTime = cg.time + (cg.complaintClient < 0 ? 10000 : 15000);
        }
        return;
    }

    if (!Q_stricmpn(cmd, "enable_vote_ui", 14)) {
        trap_Cvar_Set("ui_voteactive", "1");
        return;
    }
    if (!Q_stricmpn(cmd, "disable_vote_ui", 15)) {
        trap_Cvar_Set("ui_voteactive", "0");
        return;
    }

    if (!strcmp(cmd, "map_restart")) {
        CG_MapRestart();
        return;
    }

    if (Q_stricmp(cmd, "remapShader") == 0) {
        if (trap_Argc() == 4) {
            char shader1[MAX_QPATH];
            char shader2[MAX_QPATH];
            char shader3[MAX_QPATH];

            Q_strncpyz(shader1, CG_Argv(1), sizeof(shader1));
            Q_strncpyz(shader2, CG_Argv(2), sizeof(shader2));
            Q_strncpyz(shader3, CG_Argv(3), sizeof(shader3));

            trap_R_RemapShader(shader1, shader2, shader3);
        }
        return;
    }

    if (!strcmp(cmd, "loaddefered") || !strcmp(cmd, "loaddeferred")) {
        CG_LoadDeferredPlayers();
        return;
    }

    if (!strcmp(cmd, "clientLevelShot")) {
        cg.levelShot = qtrue;
        return;
    }

    CG_Printf("Unknown client game command: %s\n", cmd);
}

void CG_ExecuteNewServerCommands(int latestSequence) {
    while (cgs.serverCommandSequence < latestSequence) {
        if (trap_GetServerCommand(++cgs.serverCommandSequence)) {
            CG_ServerCommand();
        }
    }
}
