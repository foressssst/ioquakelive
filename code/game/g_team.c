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

#include "g_local.h"

typedef struct teamgame_s {
    float last_flag_capture;
    int last_capture_team;
    flagStatus_t redStatus;   // CTF
    flagStatus_t blueStatus;  // CTF
    flagStatus_t flagStatus;  // One Flag CTF
    int redTakenTime;
    int blueTakenTime;
    int redObeliskAttackedTime;
    int blueObeliskAttackedTime;
} teamgame_t;

teamgame_t teamgame;

gentity_t* neutralObelisk;

extern qboolean itemRegistered[];

void Team_SetFlagStatus(int team, flagStatus_t status);

/*
==============
Team_InitGame

Reset flag status at game start and warn about missing team entities.
Called from G_InitGame. Handles CTF, 1FCTF, AD, Obelisk and Harvester.
Address: 0x1004fda0
==============
*/
void Team_InitGame(void) {
    gitem_t* item;

    memset(&teamgame, 0, sizeof teamgame);

    // binary also clears two level shuffle flags here (see manifest)

    switch (g_gametype.integer) {
        case GT_CTF:
        case GT_AD:
            teamgame.redStatus = -1;  // Invalid to force update
            teamgame.blueStatus = -1;
            Team_SetFlagStatus(TEAM_RED, FLAG_ATBASE);
            Team_SetFlagStatus(TEAM_BLUE, FLAG_ATBASE);
            break;
        case GT_1FCTF:
            teamgame.flagStatus = -1;  // Invalid to force update
            Team_SetFlagStatus(TEAM_FREE, FLAG_ATBASE);
            break;
        default:
            break;
    }

    if (g_gametype.integer == GT_CTF || g_gametype.integer == GT_AD) {
        item = BG_FindItemForPowerup(PW_REDFLAG);
        if (!item || !itemRegistered[item - bg_itemlist]) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map");
        }
        item = BG_FindItemForPowerup(PW_BLUEFLAG);
        if (!item || !itemRegistered[item - bg_itemlist]) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map");
        }
    }

    if (g_gametype.integer == GT_1FCTF) {
        item = BG_FindItemForPowerup(PW_REDFLAG);
        if (!item || !itemRegistered[item - bg_itemlist]) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map");
        }
        item = BG_FindItemForPowerup(PW_BLUEFLAG);
        if (!item || !itemRegistered[item - bg_itemlist]) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map");
        }
        item = BG_FindItemForPowerup(PW_NEUTRALFLAG);
        if (!item || !itemRegistered[item - bg_itemlist]) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_neutralflag in map");
        }
    }

    if (g_gametype.integer == GT_OBELISK) {
        if (!G_Find(NULL, FOFS(classname), "team_redobelisk")) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_redobelisk in map");
        }
        if (!G_Find(NULL, FOFS(classname), "team_blueobelisk")) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_blueobelisk in map");
        }
    }

    if (g_gametype.integer == GT_HARVESTER) {
        if (!G_Find(NULL, FOFS(classname), "team_redobelisk")) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_redobelisk in map");
        }
        if (!G_Find(NULL, FOFS(classname), "team_blueobelisk")) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_blueobelisk in map");
        }
        if (!G_Find(NULL, FOFS(classname), "team_neutralobelisk")) {
            G_Printf(S_COLOR_YELLOW "WARNING: No team_neutralobelisk in map");
        }
    }
}

int OtherTeam(int team) {
    if (team == TEAM_RED)
        return TEAM_BLUE;
    else if (team == TEAM_BLUE)
        return TEAM_RED;
    return team;
}

const char* TeamName(int team) {
    if (team == TEAM_RED)
        return "RED";
    else if (team == TEAM_BLUE)
        return "BLUE";
    else if (team == TEAM_SPECTATOR)
        return "SPECTATOR";
    return "FREE";
}

// [QL] 0x1002cad0: spectators get white too
const char* TeamColorString(int team) {
    if (team == TEAM_RED)
        return S_COLOR_RED;
    else if (team == TEAM_BLUE)
        return S_COLOR_BLUE;
    return S_COLOR_WHITE;
}

// NULL for everyone
static __attribute__((format(printf, 2, 3))) void QDECL PrintMsg(gentity_t* ent, const char* fmt, ...) {
    char msg[1024];
    va_list argptr;
    char* p;

    va_start(argptr, fmt);
    if (Q_vsnprintf(msg, sizeof(msg), fmt, argptr) >= sizeof(msg)) {
        G_Error("PrintMsg overrun");
    }
    va_end(argptr);

    // double quotes are bad
    while ((p = strchr(msg, '"')) != NULL)
        *p = '\'';

    trap_SendServerCommand(((ent == NULL) ? -1 : ent - g_entities), va("print \"%s\"", msg));
}

/*
==============
FreezeTagInGame / FreezeTagInWarmup

[QL] freeze-tag live/warmup predicates (binary 0x10067e00 / 0x10067e30).
==============
*/
qboolean FreezeTagInGame(void) {
    return (level.intermissionTime == 0 && level.intermissionQueued == 0 &&
            level.warmupTime == 0 && g_gametype.integer == GT_FREEZE);
}

qboolean FreezeTagInWarmup(void) {
    return (level.intermissionTime == 0 && level.intermissionQueued == 0 &&
            level.warmupTime != 0 && g_gametype.integer == GT_FREEZE);
}

/*
==============
AddTeamScore

[QL] byte-faithful port of binary 0x10068190. Updates level.teamScores[team]
and broadcasts a team announcer sound (EV_GLOBAL_TEAM_SOUND) only on a tie,
lead change, or generic team-scored event - ordinary frags emit nothing (the
event parm stays -1). The sound is suppressed while approaching the capture
limit (handled elsewhere), except during warmup. The temp-entity
origin is irrelevant here (the event is broadcast to everyone).
==============
*/
void AddTeamScore(vec3_t origin, int team, int score) {
    int otherTeam;
    int ev = -1;
    int myScore, otherScore, newScore;
    int gt = g_gametype.integer;

    if (team == TEAM_RED) {
        otherTeam = TEAM_BLUE;
    } else {
        otherTeam = TEAM_RED;
        if (team != TEAM_BLUE) {
            otherTeam = team;
        }
    }

    if (level.intermissionQueued) {
        return;
    }
    if (level.intermissionTime) {
        return;
    }

    myScore    = level.teamScores[team];
    newScore   = myScore + score;
    otherScore = level.teamScores[otherTeam];

    if (newScore == otherScore) {
        ev = GTS_TEAMS_ARE_TIED;
    } else if (gt == GT_CTF || gt == GT_1FCTF || FreezeTagInGame() || gt == GT_AD) {
        // generic team-scored announce
        ev = (team != TEAM_RED) + GTS_REDTEAM_SCORED;      // RED->8, BLUE->9
    } else if (myScore == otherScore) {
        // teams were level before this score - it is a lead change
        if (newScore > otherScore) {
            ev = (team != TEAM_RED) + GTS_REDTEAM_TOOK_LEAD;   // RED->10, BLUE->11
        } else if (newScore < otherScore) {
            // fell behind, so the other team took the lead
            ev = (team == TEAM_RED) ? GTS_BLUETEAM_TOOK_LEAD : GTS_REDTEAM_TOOK_LEAD;
        }
    }

    // suppress the announce when one score away from the capture limit in
    // capture-based modes (unless in warmup); TDM and freeze always announce.
    if (gt == GT_TEAM || FreezeTagInGame() || g_capturelimit.integer == 0 ||
        myScore + 1 < g_capturelimit.integer || level.warmupTime != 0) {
        if (ev >= 0) {
            // [QL] 0x10068283: the event goes out from vec3_origin
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = ev;
        }
    }

    level.teamScores[team] += score;
    CalculateRanks();
}

/*
==============
OnSameTeam
==============
*/
/*
==============
OnSameTeam

[QL] 0x10067f30: outside the team gametypes, only two spectators share a team.
==============
*/
qboolean OnSameTeam(gentity_t* ent1, gentity_t* ent2) {
    if (!ent1->client || !ent2->client) {
        return qfalse;
    }

    if (g_gametype.integer < GT_TEAM && ent1->client->sess.sessionTeam != TEAM_SPECTATOR) {
        return qfalse;
    }

    return ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam;
}

/*
==============
G_VoiceFilter

Quien oye a quien. El servidor la consulta por cada par al repartir la voz, y
solo la manda cuando devuelve 0.

[QL] 0x10052e90, export +0x3c. El orden de las condiciones es el del binario:

  - a los bots no se les manda nada;
  - a quien esta silenciado no se lo reparte;
  - nadie se oye a si mismo;
  - en el intermedio y mientras se espera jugadores (warmupTime < 0) se oyen
    todos con todos;
  - en los modos por equipos, y siempre que el que habla sea espectador, se
    oyen solo los del mismo equipo;
  - en el resto de los modos se oyen todos.

Retail tiene ademas una variable de modo que nada escribe nunca -- las dos
unicas referencias en el binario la leen. Con su valor fijo en 0 las dos ramas
que dependen de ella son las de arriba, asi que aca no existe.
==============
*/
int G_VoiceFilter(int speaker, int listener) {
    gentity_t* from = &g_entities[speaker];
    gentity_t* to = &g_entities[listener];

    if (to->r.svFlags & SVF_BOT) {
        return 1;
    }
    if (!from->client || from->client->sess.muted) {
        return 1;
    }
    if (speaker == listener) {
        return 1;
    }
    if (level.intermissionTime || level.warmupTime < 0) {
        return 0;
    }
    if (g_gametype.integer >= GT_TEAM || from->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return !OnSameTeam(from, to);
    }
    return 0;
}

/*
==============
Team_SetFlagStatus

[QL] 0x10068770. CTF and Attack & Defend send both flags, '0' at base, '1' taken and '2' for
anything else; the other modes send the neutral flag's status as a digit.
==============
*/
void Team_SetFlagStatus(int team, flagStatus_t status) {
    char st[4];

    switch (team) {
        case TEAM_RED:
            if (teamgame.redStatus == status) {
                return;
            }
            teamgame.redStatus = status;
            break;

        case TEAM_BLUE:
            if (teamgame.blueStatus == status) {
                return;
            }
            teamgame.blueStatus = status;
            break;

        case TEAM_FREE:
            if (teamgame.flagStatus == status) {
                return;
            }
            teamgame.flagStatus = status;
            break;

        default:
            return;
    }

    if (g_gametype.integer == GT_CTF || g_gametype.integer == GT_AD) {
        st[0] = teamgame.redStatus < 2 ? '0' + teamgame.redStatus : '2';
        st[1] = teamgame.blueStatus < 2 ? '0' + teamgame.blueStatus : '2';
        st[2] = 0;
    } else {
        st[0] = '0' + teamgame.flagStatus;
        st[1] = 0;
    }

    trap_SetConfigstring(CS_FLAGSTATUS, st);
}

void Team_CheckDroppedItem(gentity_t* dropped) {
    if (dropped->item->giTag == PW_REDFLAG) {
        Team_SetFlagStatus(TEAM_RED, FLAG_DROPPED);
    } else if (dropped->item->giTag == PW_BLUEFLAG) {
        Team_SetFlagStatus(TEAM_BLUE, FLAG_DROPPED);
    } else if (dropped->item->giTag == PW_NEUTRALFLAG) {
        Team_SetFlagStatus(TEAM_FREE, FLAG_DROPPED);
    }
}

/*
================
Team_ForceGesture
================
*/
void Team_ForceGesture(int team) {
    int i;
    gentity_t* ent;

    for (i = 0; i < MAX_CLIENTS; i++) {
        ent = &g_entities[i];
        if (!ent->inuse)
            continue;
        if (!ent->client)
            continue;
        if (ent->client->sess.sessionTeam != team)
            continue;
        //
        ent->flags |= FL_FORCE_GESTURE;
    }
}

/*
================
Team_FragBonuses

[QL] qagamex86.dll 0x10068970. Only CTF, One Flag, Overload, Harvester, Domination and
Attack & Defend give frag bonuses, and Domination has its own. In importance order, and never
more than one: fragging the flag or skull carrier, fragging someone who hurt your carrier in the
last 8 seconds, a kill near your team's flag (or obelisk), a kill near your own carrier. Every
defence gives the defend medal.
================
*/
void Team_FragBonuses(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker) {
    int i;
    int team, otherteam;
    int flag_pw, enemy_flag_pw;
    int tokens;
    int bonus;
    const char* powerup = NULL;
    const char* c;
    gentity_t *ent, *flag, *carrier = NULL;
    vec3_t v;
    float dist;

    switch (g_gametype.integer) {
        case GT_CTF:
        case GT_1FCTF:
        case GT_OBELISK:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            break;
        default:
            return;
    }

    // no bonus for fragging yourself or team mates
    if (!targ->client || !attacker->client || targ == attacker || OnSameTeam(targ, attacker)) {
        return;
    }

    team = targ->client->sess.sessionTeam;
    otherteam = OtherTeam(team);
    if (otherteam < 0) {
        return;  // whoever died isn't on a team
    }

    if (g_gametype.integer == GT_DOMINATION) {
        DOM_FragBonuses(attacker, targ);
        return;
    }

    if (team == TEAM_RED) {
        flag_pw = PW_REDFLAG;
        enemy_flag_pw = PW_BLUEFLAG;
    } else {
        flag_pw = PW_BLUEFLAG;
        enemy_flag_pw = PW_REDFLAG;
    }
    // [QL] only the carried flag turns neutral; the carrier search below keeps the team's flag
    if (g_gametype.integer == GT_1FCTF) {
        enemy_flag_pw = PW_NEUTRALFLAG;
    }

    tokens = 0;
    if (g_gametype.integer == GT_HARVESTER) {
        tokens = targ->client->ps.generic1 & 0x3f;
    }

    // the powerup the victim held, for the message
    if (targ->client->ps.powerups[PW_QUAD]) {
        powerup = "Quad";
    } else if (targ->client->ps.powerups[PW_BATTLESUIT]) {
        powerup = "BS";
    } else if (targ->client->ps.powerups[PW_REGEN]) {
        powerup = "Regen";
    } else if (targ->client->ps.powerups[PW_HASTE]) {
        powerup = "Haste";
    } else if (targ->client->ps.powerups[PW_INVIS]) {
        powerup = "Invis";
    }

    // did the attacker frag the flag or skull carrier?
    if (targ->client->ps.powerups[enemy_flag_pw] || tokens) {
        attacker->client->pers.teamState.lastfraggedcarrier = level.time;
        attacker->client->pers.teamState.fragcarrier++;
        if (targ->client->ps.powerups[enemy_flag_pw]) {
            AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS);
            PrintMsg(NULL, "%s^3 fragged %s's %sflag carrier!\n", attacker->client->pers.netname,
                     TeamName(team), powerup ? va("%s ", powerup) : "");
        } else {
            AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS * tokens * tokens);
            PrintMsg(NULL, "%s^3 fragged %s's %sskull carrier!\n", attacker->client->pers.netname,
                     TeamName(team), powerup ? va("%s ", powerup) : "");
        }

        // the target had the flag, clear the hurt carrier field on the other team
        for (i = 0; i < g_maxclients.integer; i++) {
            ent = g_entities + i;
            if (ent->inuse && ent->client->sess.sessionTeam == otherteam) {
                ent->client->pers.teamState.lasthurtcarrier = 0;
            }
        }
        return;
    }

    // did the attacker frag someone who hurt his carrier in the last 8 seconds?
    // [QL] 0x10068c66: retail also asks whether the attacker holds his own flag, but both
    // answers give the same bonus
    if (targ->client->pers.teamState.lasthurtcarrier &&
        level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT) {
        attacker->client->pers.teamState.carrierdefense++;
        attacker->client->expandedStats.numDefends++;
        AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);
        targ->client->pers.teamState.lasthurtcarrier = 0;
        PlayerAwardEF(attacker, PERS_DEFEND_COUNT, EF_AWARD_DEFEND);
        STAT_AddPlayerMedalStat(attacker, "DEFENSE");
        return;
    }

    // flag and flag carrier area defense bonuses: find the attacker's flag (or obelisk) and carrier
    if (g_gametype.integer == GT_OBELISK) {
        switch (attacker->client->sess.sessionTeam) {
            case TEAM_RED:
                c = "team_redobelisk";
                break;
            case TEAM_BLUE:
                c = "team_blueobelisk";
                break;
            default:
                return;
        }
    } else if (g_gametype.integer == GT_HARVESTER) {
        c = "team_neutralobelisk";
    } else {
        switch (attacker->client->sess.sessionTeam) {
            case TEAM_RED:
                c = "team_CTF_redflag";
                break;
            case TEAM_BLUE:
                c = "team_CTF_blueflag";
                break;
            default:
                return;
        }
        for (i = 0; i < g_maxclients.integer; i++) {
            carrier = g_entities + i;
            if (carrier->inuse && carrier->client->ps.powerups[flag_pw]) {
                break;
            }
            carrier = NULL;
        }
    }
    flag = NULL;
    while ((flag = G_Find(flag, FOFS(classname), c)) != NULL) {
        if (!(flag->flags & FL_DROPPED_ITEM)) {
            break;
        }
    }
    if (!flag) {
        return;  // can't find attacker's flag
    }

    // [QL] 0x10068e26: the base counts only the victim's distance to the flag
    VectorSubtract(targ->r.currentOrigin, flag->r.currentOrigin, v);
    if (VectorLength(v) < CTF_TARGET_PROTECT_RADIUS &&
        trap_InPVS(flag->r.currentOrigin, targ->r.currentOrigin) &&
        attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {
        // in Attack & Defend only the defending team defends its base
        if (g_gametype.integer == GT_AD && attacker->client->sess.sessionTeam != AD_DefendingTeam()) {
            return;
        }
        attacker->client->pers.teamState.basedefense++;
        bonus = CTF_FLAG_DEFENSE_BONUS;
    } else {
        // [QL] 0x10068f00: the carrier counts only the attacker's distance to him
        if (!carrier || carrier == attacker) {
            return;
        }
        VectorSubtract(attacker->r.currentOrigin, carrier->r.currentOrigin, v);
        dist = VectorLength(v);
        if (!(dist < CTF_ATTACKER_PROTECT_RADIUS &&
              (trap_InPVS(carrier->r.currentOrigin, targ->r.currentOrigin) ||
               trap_InPVS(carrier->r.currentOrigin, attacker->r.currentOrigin)))) {
            return;
        }
        if (attacker->client->sess.sessionTeam == targ->client->sess.sessionTeam) {
            return;
        }
        attacker->client->pers.teamState.carrierdefense++;
        bonus = CTF_CARRIER_PROTECT_BONUS;
    }

    attacker->client->expandedStats.numDefends++;
    AddScore(attacker, targ->r.currentOrigin, bonus);
    PlayerAwardEF(attacker, PERS_DEFEND_COUNT, EF_AWARD_DEFEND);
    STAT_AddPlayerMedalStat(attacker, "DEFENSE");
}

/*
================
Team_CheckHurtCarrier

Check to see if attacker hurt the flag carrier.  Needed when handing out bonuses for assistance to flag
carrier defense.
[QL] 0x10069080: the red or blue flag (the neutral flag does not count), then the skulls.
================
*/
void Team_CheckHurtCarrier(gentity_t* targ, gentity_t* attacker) {
    int flag_pw;

    if (!targ->client || !attacker->client)
        return;

    if (targ->client->sess.sessionTeam == TEAM_RED)
        flag_pw = PW_BLUEFLAG;
    else
        flag_pw = PW_REDFLAG;

    // flags
    if (targ->client->ps.powerups[flag_pw] &&
        targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
        attacker->client->pers.teamState.lasthurtcarrier = level.time;

    // skulls
    if ((targ->client->ps.generic1 & 0x3f) &&
        targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
        attacker->client->pers.teamState.lasthurtcarrier = level.time;
}

gentity_t* Team_ResetFlag(int team) {
    char* c;
    gentity_t *ent, *rent = NULL;

    switch (team) {
        case TEAM_RED:
            c = "team_CTF_redflag";
            break;
        case TEAM_BLUE:
            c = "team_CTF_blueflag";
            break;
        case TEAM_FREE:
            c = "team_CTF_neutralflag";
            break;
        default:
            return NULL;
    }

    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), c)) != NULL) {
        if (ent->flags & FL_DROPPED_ITEM)
            G_FreeEntity(ent);
        else {
            rent = ent;
            RespawnItem(ent);
        }
    }

    Team_SetFlagStatus(team, FLAG_ATBASE);

    return rent;
}

void Team_ResetFlags(void) {
    if (g_gametype.integer == GT_CTF) {
        Team_ResetFlag(TEAM_RED);
        Team_ResetFlag(TEAM_BLUE);
    } else if (g_gametype.integer == GT_1FCTF) {
        Team_ResetFlag(TEAM_FREE);
    }
}

void Team_ReturnFlagSound(gentity_t* ent, int team) {
    gentity_t* te;

    if (ent == NULL) {
        G_Printf("Warning:  NULL passed to Team_ReturnFlagSound\n");
        return;
    }

    te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
    if (team == TEAM_BLUE) {
        te->s.eventParm = GTS_RED_RETURN;
    } else {
        te->s.eventParm = GTS_BLUE_RETURN;
    }
    te->r.svFlags |= SVF_BROADCAST;
}

/*
==============
Team_TakeFlagSound

[QL] 0x100694a0: the player who took the flag travels in otherEntityNum, which the client uses to
auto-follow the carrier
==============
*/
void Team_TakeFlagSound(gentity_t* ent, int team, gentity_t* player) {
    gentity_t* te;

    if (ent == NULL) {
        G_Printf("Warning:  NULL passed to Team_TakeFlagSound\n");
        return;
    }

    // only play sound when the flag was at the base
    // or not picked up the last 2 seconds
    switch (team) {
        case TEAM_RED:
            if (teamgame.blueStatus != FLAG_ATBASE) {
                if (teamgame.blueTakenTime > level.time - 2000)
                    return;
            }
            teamgame.blueTakenTime = level.time;
            break;

        case TEAM_BLUE:  // CTF
            if (teamgame.redStatus != FLAG_ATBASE) {
                if (teamgame.redTakenTime > level.time - 2000)
                    return;
            }
            teamgame.redTakenTime = level.time;
            break;
    }

    te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
    te->s.otherEntityNum = player->s.number;
    te->r.svFlags |= SVF_BROADCAST;
    if (team == TEAM_BLUE) {
        te->s.eventParm = GTS_RED_TAKEN;
    } else {
        te->s.eventParm = GTS_BLUE_TAKEN;
    }
}

/*
==============
Team_CaptureFlagSound

[QL] 0x10069550: everyone hears it but the player who captured
==============
*/
void Team_CaptureFlagSound(gentity_t* ent, int team, gentity_t* capturer) {
    gentity_t* te;

    if (ent == NULL) {
        G_Printf("Warning:  NULL passed to Team_CaptureFlagSound\n");
        return;
    }

    te = G_TempEntity(ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
    te->r.svFlags |= SVF_BROADCAST | SVF_NOTSINGLECLIENT;
    if (team == TEAM_BLUE) {
        te->s.eventParm = GTS_BLUE_CAPTURE;
    } else {
        te->s.eventParm = GTS_RED_CAPTURE;
    }
    te->r.singleClient = capturer->client->ps.clientNum;
}

// [QL] 0x100695b0
void Team_ReturnFlag(int team) {
    Team_ReturnFlagSound(Team_ResetFlag(team), team);
    if (team == TEAM_FREE) {
        PrintMsg(NULL, "^3The flag has returned!\n");
    } else {
        PrintMsg(NULL, "^3The %s flag^3 has returned!\n", TeamName(team));
    }
}

void Team_FreeEntity(gentity_t* ent) {
    if (ent->item->giTag == PW_REDFLAG) {
        Team_ReturnFlag(TEAM_RED);
    } else if (ent->item->giTag == PW_BLUEFLAG) {
        Team_ReturnFlag(TEAM_BLUE);
    } else if (ent->item->giTag == PW_NEUTRALFLAG) {
        Team_ReturnFlag(TEAM_FREE);
    }
}

/*
==============
Team_CheckLostFlag

[QL] 0x100692b0, run every frame in CTF for each team. A flag that is off its stand, with no dropped
copy lying around and nobody on the other team carrying a flag, is returned with its announcement:
it covers a carrier who is gone without dropping it.
==============
*/
void Team_CheckLostFlag(int team) {
    char* classname;
    int enemyTeam;
    gentity_t* ent;
    gclient_t* cl;
    int i;

    if (team == TEAM_RED) {
        classname = "team_CTF_redflag";
        enemyTeam = TEAM_BLUE;
    } else if (team == TEAM_BLUE) {
        classname = "team_CTF_blueflag";
        enemyTeam = TEAM_RED;
    } else {
        return;
    }

    for (ent = G_Find(NULL, FOFS(classname), classname); ent; ent = G_Find(ent, FOFS(classname), classname)) {
        if ((ent->flags & FL_DROPPED_ITEM) || !(ent->s.eFlags & EF_NODRAW)) {
            return;
        }
    }

    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != enemyTeam) {
            continue;
        }
        if (cl->ps.powerups[PW_REDFLAG] || cl->ps.powerups[PW_BLUEFLAG] || cl->ps.powerups[PW_NEUTRALFLAG]) {
            return;
        }
    }

    Team_ReturnFlag(team);
}

/*
==============
Team_DroppedFlagThink

Automatically set in Launch_Item if the item is one of the flags

Flags are unique in that if they are dropped, the base flag must be respawned when they time out
[QL] 0x10069780: in Attack & Defend a dropped flag stays where it fell
==============
*/
void Team_DroppedFlagThink(gentity_t* ent) {
    int team = TEAM_FREE;

    if (g_gametype.integer == GT_AD) {
        return;
    }

    if (ent->item->giTag == PW_REDFLAG) {
        team = TEAM_RED;
    } else if (ent->item->giTag == PW_BLUEFLAG) {
        team = TEAM_BLUE;
    } else if (ent->item->giTag == PW_NEUTRALFLAG) {
        team = TEAM_FREE;
    }

    Team_ReturnFlagSound(Team_ResetFlag(team), team);
    // Reset Flag will delete this entity
}

/*
==============
Team_TouchOurFlag

[QL] qagamex86.dll 0x10069800. A dropped flag is returned. A capture prints how long the flag
was carried ("captured in" when it came straight from the enemy base, "held for" when it was
picked up off the ground on the way), gives the capture medal, and gives the assist medal to
every teammate who returned the flag or fragged the enemy carrier in the last 10 seconds, the
capturer included. The capture that reaches the capture limit plays no capture sound, outside
warmup. 'score' is what the capture adds to the team score. Retail has a second caller
(0x1006bda0, a team trigger) that is not in ioquakelive.
==============
*/
int Team_TouchOurFlag(gentity_t* ent, gentity_t* other, int team, int score) {
    int i;
    int enemy_flag;
    int minutes;
    float held;
    gentity_t* player;
    gclient_t* cl = other->client;

    if (level.intermissionQueued || level.intermissionTime) {
        return 0;
    }

    if (g_gametype.integer == GT_1FCTF) {
        enemy_flag = PW_NEUTRALFLAG;
        if (!cl->ps.powerups[PW_NEUTRALFLAG]) {
            return 0;
        }
    } else {
        enemy_flag = (cl->sess.sessionTeam == TEAM_RED) ? PW_BLUEFLAG : PW_REDFLAG;

        if ((ent->flags & FL_DROPPED_ITEM) && !ent->freeAfterEvent) {
            // hey, it's not home.  return it by teleporting it back
            PrintMsg(NULL, "%s^3 returned the %s flag!\n", cl->pers.netname, TeamName(team));
            cl->pers.teamState.flagrecovery++;
            AddScore(other, ent->r.currentOrigin, CTF_RECOVERY_BONUS);
            cl->pers.teamState.lastreturnedflag = level.time;
            // ResetFlag will remove this entity!  We must return zero
            Team_ReturnFlagSound(Team_ResetFlag(team), team);
            return 0;
        }

        // the flag is at home base.  if the player has the enemy
        // flag, he's just won!
        if (!cl->ps.powerups[enemy_flag]) {
            return 0;  // We don't have the flag
        }
    }

    held = (float)(cl->pers.teamState.flagsince / 1000.0);
    minutes = (int)held / 60;
    held = (float)(held - minutes * 60.0);
    PrintMsg(NULL,
             cl->pers.teamState.flagDroppedPickups == 0
                 ? "%s%s TEAM^3 CAPTURED the flag!^7 (%s captured in %d:%s%.03f)\n"
                 : "%s%s TEAM^3 CAPTURED the flag!^7 (%s held for %d:%s%.03f)\n",
             TeamColorString(team), TeamName(team), cl->pers.netname, minutes, held < 10.0f ? "0" : "",
             held);
    Q_strncpyz(level.lastTeamScorer, cl->pers.netname, sizeof(level.lastTeamScorer));

    cl->ps.powerups[enemy_flag] = 0;
    cl->pers.teamState.captures++;
    // [QL] Team_TouchOurFlag 0x100697e0: capturer's numCaptures (+0x7f8)
    cl->expandedStats.numCaptures++;

    // Increase the team's score
    AddTeamScore(ent->s.pos.trBase, cl->sess.sessionTeam, score);
    Team_ForceGesture(cl->sess.sessionTeam);

    PlayerAwardEF(other, PERS_CAPTURES, EF_AWARD_CAP);
    STAT_AddPlayerMedalStat(other, "CAPTURE");

    // other gets another 10 frag bonus
    AddScore(other, ent->r.currentOrigin, CTF_CAPTURE_BONUS);

    if (!g_capturelimit.integer || level.teamScores[cl->sess.sessionTeam] < g_capturelimit.integer ||
        level.warmupTime) {
        Team_CaptureFlagSound(ent, team, other);
    }

    if (g_gametype.integer == GT_AD && !trap_HasAchievement(cl->ps.clientNum, 0x1e)) {
        // capture while every enemy is alive, against a team of three or more
        int alive[TEAM_NUM_TEAMS], total[TEAM_NUM_TEAMS];

        memset(alive, 0, sizeof(alive));
        memset(total, 0, sizeof(total));
        for (i = 0; i < level.maxclients; i++) {
            gclient_t* c = &level.clients[i];

            if (c->pers.connected != CON_CONNECTED) {
                continue;
            }
            total[c->sess.sessionTeam]++;
            if (c->ps.pm_type == PM_NORMAL) {
                alive[c->sess.sessionTeam]++;
            }
        }
        if (((cl->sess.sessionTeam == TEAM_BLUE && alive[TEAM_RED] == total[TEAM_RED]) ||
             (cl->sess.sessionTeam == TEAM_RED && alive[TEAM_BLUE] == total[TEAM_BLUE])) &&
            total[OtherTeam(cl->sess.sessionTeam)] >= 3) {
            trap_SetAchievement(cl->ps.clientNum, 0x1e);
        }
    }

    if (g_gametype.integer == GT_CTF && !trap_HasAchievement(cl->ps.clientNum, 0x36) &&
        level.teamScores[cl->sess.sessionTeam] == g_capturelimit.integer &&
        level.teamScores[cl->sess.sessionTeam] - level.teamScores[OtherTeam(cl->sess.sessionTeam)] == 1) {
        // win the game on the limit by a single capture
        trap_SetAchievement(cl->ps.clientNum, 0x36);
    }

    // Ok, let's do the player loop, hand out the bonuses
    for (i = 0; i < g_maxclients.integer; i++) {
        player = &g_entities[i];

        if (!player->inuse) {
            continue;
        }

        if (player->client->sess.sessionTeam != cl->sess.sessionTeam) {
            player->client->pers.teamState.lasthurtcarrier = -5;
            continue;
        }

        if (player != other) {
            AddScore(player, ent->r.currentOrigin, CTF_TEAM_BONUS);
        }
        // award extra points for capture assists
        if (player->client->pers.teamState.lastreturnedflag + CTF_RETURN_FLAG_ASSIST_TIMEOUT > level.time ||
            player->client->pers.teamState.lastfraggedcarrier + CTF_FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
            other->client->pers.teamState.assists++;
            // [QL] Team_TouchOurFlag 0x100697e0: assisting teammate's numAssists (+0x7fc)
            player->client->expandedStats.numAssists++;
            AddScore(player, ent->r.currentOrigin, CTF_RETURN_FLAG_ASSIST_BONUS);
            PlayerAwardEF(player, PERS_ASSIST_COUNT, EF_AWARD_ASSIST);
            STAT_AddPlayerMedalStat(player, "ASSIST");
        }
    }

    if (g_gametype.integer == GT_AD) {
        level.roundState.capture = qtrue;
        Team_ResetFlag(TEAM_RED);
        Team_ResetFlag(TEAM_BLUE);
    } else if (g_gametype.integer == GT_CTF) {
        Team_ResetFlag(TEAM_RED);
        Team_ResetFlag(TEAM_BLUE);
    } else if (g_gametype.integer == GT_1FCTF) {
        Team_ResetFlag(TEAM_FREE);
    }

    return 0;  // Do not respawn this automatically
}

/*
==============
Team_TouchEnemyFlag

[QL] qagamex86.dll 0x10069e30. Only a flag taken from its base scores and counts for the stats;
one picked up off the ground scores with g_droppedFlagBonus when someone else dropped it. In
Attack & Defend no flag is taken during warmup, and the first touch of a round adds
g_adTouchScoreBonus to the team score.
==============
*/
int Team_TouchEnemyFlag(gentity_t* ent, gentity_t* other, int team) {
    gclient_t* cl = other->client;

    if (level.intermissionQueued || level.intermissionTime) {
        return 0;
    }

    if (g_gametype.integer == GT_AD && level.warmupTime) {
        return 0;
    }

    if (g_gametype.integer == GT_1FCTF) {
        PrintMsg(NULL, "%s^3 got the flag!\n", cl->pers.netname);
        cl->ps.powerups[PW_NEUTRALFLAG] = INT_MAX;  // flags never expire
        Team_SetFlagStatus(TEAM_FREE, team == TEAM_RED ? FLAG_TAKEN_RED : FLAG_TAKEN_BLUE);
    } else {
        PrintMsg(NULL, "%s^3 got the %s flag!\n", cl->pers.netname, TeamName(team));
        if (team == TEAM_RED) {
            cl->ps.powerups[PW_REDFLAG] = INT_MAX;  // flags never expire
        } else {
            cl->ps.powerups[PW_BLUEFLAG] = INT_MAX;  // flags never expire
        }
        Team_SetFlagStatus(team, FLAG_TAKEN);
    }

    if (ent->s.modelindex2 == 0) {
        // [QL] 0x10069f7e: taken from its base
        AddScore(other, ent->r.currentOrigin, CTF_FLAG_BONUS);
        if (g_gametype.integer == GT_1FCTF) {
            cl->expandedStats.numNeutralFlagPickups++;
        } else if (team == TEAM_RED) {
            cl->expandedStats.numRedFlagPickups++;
        } else if (team == TEAM_BLUE) {
            cl->expandedStats.numBlueFlagPickups++;
        }
        level.numFlagPickups[cl->sess.sessionTeam]++;
    } else if (g_droppedFlagBonus.integer && ent->activator && ent->activator != other) {
        AddScore(other, ent->r.currentOrigin, CTF_FLAG_BONUS);
    }

    cl->pers.teamState.flagsince = 0;
    if (ent->s.modelindex2) {
        cl->pers.teamState.flagDroppedPickups++;
    } else {
        cl->pers.teamState.flagDroppedPickups = 0;
    }

    Team_TakeFlagSound(ent, team, other);

    if (g_gametype.integer == GT_AD && ent->s.modelindex2 == 0 && !level.roundState.touch) {
        AddTeamScore(ent->s.pos.trBase, cl->sess.sessionTeam, g_adTouchScoreBonus.integer);
        level.roundState.touch = qtrue;
    }

    return -1;  // Do not respawn this automatically, but do delete it if it was FL_DROPPED
}

/*
==============
Pickup_Team

[QL] qagamex86.dll 0x1006a040
==============
*/
int Pickup_Team(gentity_t* ent, gentity_t* other) {
    int team;
    gclient_t* cl = other->client;

    if (g_gametype.integer == GT_OBELISK) {
        // there are no team items that can be picked up in obelisk
        G_FreeEntity(ent);
        return 0;
    }

    if (g_gametype.integer == GT_HARVESTER) {
        // the only team items that can be picked up in harvester are the skulls
        if (ent->spawnflags != cl->sess.sessionTeam && (cl->ps.generic1 & 0x3f) < 62) {
            cl->ps.generic1++;
        }
        // [QL] 0x1006a0df: returning 0 skips Touch_Item's pickup sound, so the skull plays its own
        if (cl->pers.predictItemPickup) {
            G_AddPredictableEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
        } else {
            G_AddEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);
        }
        G_FreeEntity(ent);
        return 0;
    }

    // figure out what team this flag is
    if (strcmp(ent->classname, "team_CTF_redflag") == 0) {
        team = TEAM_RED;
    } else if (strcmp(ent->classname, "team_CTF_blueflag") == 0) {
        team = TEAM_BLUE;
    } else if (strcmp(ent->classname, "team_CTF_neutralflag") == 0) {
        team = TEAM_FREE;
    } else {
        PrintMsg(other, "Don't know what team the flag is on.\n");
        return 0;
    }

    if (g_gametype.integer == GT_1FCTF) {
        if (team == TEAM_FREE) {
            return Team_TouchEnemyFlag(ent, other, cl->sess.sessionTeam);
        }
        if (team != cl->sess.sessionTeam) {
            return Team_TouchOurFlag(ent, other, cl->sess.sessionTeam, 1);
        }
        return 0;
    }

    // in Attack & Defend only the attacking team touches the flags
    if (g_gametype.integer == GT_AD && AD_AttackingTeam() != cl->sess.sessionTeam) {
        return 0;
    }

    if (team == cl->sess.sessionTeam) {
        return Team_TouchOurFlag(ent, other, team, g_gametype.integer == GT_AD ? g_adCaptureScoreBonus.integer : 1);
    }
    return Team_TouchEnemyFlag(ent, other, team);
}

/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
gentity_t* Team_GetLocation(gentity_t* ent) {
    gentity_t *eloc, *best;
    float bestlen, len;
    vec3_t origin;

    best = NULL;
    bestlen = 3 * 8192.0 * 8192.0;

    VectorCopy(ent->r.currentOrigin, origin);

    for (eloc = level.locationHead; eloc; eloc = eloc->nextTrain) {
        len = (origin[0] - eloc->r.currentOrigin[0]) * (origin[0] - eloc->r.currentOrigin[0]) + (origin[1] - eloc->r.currentOrigin[1]) * (origin[1] - eloc->r.currentOrigin[1]) + (origin[2] - eloc->r.currentOrigin[2]) * (origin[2] - eloc->r.currentOrigin[2]);

        if (len > bestlen) {
            continue;
        }

        if (!trap_InPVS(origin, eloc->r.currentOrigin)) {
            continue;
        }

        bestlen = len;
        best = eloc;
    }

    return best;
}

/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
qboolean Team_GetLocationMsg(gentity_t* ent, char* loc, int loclen) {
    gentity_t* best;

    best = Team_GetLocation(ent);

    if (!best)
        return qfalse;

    if (best->count) {
        if (best->count < 0)
            best->count = 0;
        if (best->count > 7)
            best->count = 7;
        Com_sprintf(loc, loclen, "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, best->count + '0', best->message);
    } else
        Com_sprintf(loc, loclen, "%s", best->message);

    return qtrue;
}

/*---------------------------------------------------------------------------*/

/*
================
SelectRandomTeamSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define MAX_TEAM_SPAWN_POINTS 32
gentity_t* SelectRandomTeamSpawnPoint(int teamstate, team_t team) {
    gentity_t* spot;
    int count;
    int selection;
    gentity_t* spots[MAX_TEAM_SPAWN_POINTS];
    char* classname;

    if (teamstate == TEAM_BEGIN) {
        if (team == TEAM_RED)
            classname = "team_CTF_redplayer";
        else if (team == TEAM_BLUE)
            classname = "team_CTF_blueplayer";
        else
            return NULL;
    } else {
        if (team == TEAM_RED)
            classname = "team_CTF_redspawn";
        else if (team == TEAM_BLUE)
            classname = "team_CTF_bluespawn";
        else
            return NULL;
    }
    count = 0;

    spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), classname)) != NULL) {
        if (SpotWouldTelefrag(spot)) {
            continue;
        }
        spots[count] = spot;
        if (++count == MAX_TEAM_SPAWN_POINTS)
            break;
    }

    if (!count) {  // no spots that won't telefrag
        return G_Find(NULL, FOFS(classname), classname);
    }

    selection = rand() % count;
    return spots[selection];
}

/*
===========
SelectCTFSpawnPoint

============
*/
gentity_t* SelectCTFSpawnPoint(team_t team, int teamstate, vec3_t origin, vec3_t angles, qboolean isbot) {
    gentity_t* spot;

    spot = SelectRandomTeamSpawnPoint(teamstate, team);

    if (!spot) {
        return SelectSpawnPoint(vec3_origin, origin, angles, isbot);
    }

    VectorCopy(spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy(spot->s.angles, angles);

    return spot;
}

/*---------------------------------------------------------------------------*/

static int QDECL SortClients(const void* a, const void* b) {
    return *(int*)a - *(int*)b;
}

/*
==================
TeamOverlayMessage

[QL] qagamex86.dll 0x10068490 (Q3's TeamplayInfoMessage). "tinfo <count> <client>..." lists
up to eight teammates by client number, leaving out anyone in PM_SPECTATOR; health, armor,
weapon and location reach the client through the snapshots.
==================
*/
void TeamOverlayMessage(gentity_t* ent) {
    char entry[1024];
    char string[8192];
    int stringlength;
    int i, j;
    gentity_t* player;
    int cnt;
    int clients[TEAM_MAXOVERLAY];

    // figure out what client should be on the display
    // we are limited to 8, but we want to use the top eight players
    // but in client order (so they don't keep changing position on the overlay)
    for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < 8; i++) {
        player = g_entities + level.sortedClients[i];
        if (OnSameTeam(player, ent) && player->client->ps.pm_type != PM_SPECTATOR) {
            clients[cnt++] = level.sortedClients[i];
        }
    }

    // We have the top eight players, sort them by clientNum
    qsort(clients, cnt, sizeof(clients[0]), SortClients);

    // send the latest information on all clients
    string[0] = 0;
    stringlength = 0;

    for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < 8; i++) {
        player = g_entities + i;
        if (OnSameTeam(player, ent) && player->client->ps.pm_type != PM_SPECTATOR) {
            Com_sprintf(entry, sizeof(entry), " %i", i);
            j = strlen(entry);
            if (stringlength + j > sizeof(string))
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
            cnt++;
        }
    }

    trap_SendServerCommand(ent - g_entities, va("tinfo %i %s", cnt, string));
}

/*
==================
CheckTeamStatus

[QL] qagamex86.dll 0x10068670, from G_RunFrame in team gametypes. The overlay is sent once, a
second after someone spawns or begins (level.nextTeamInfoTime), to every red and blue player.
==================
*/
void CheckTeamStatus(void) {
    int i;
    gentity_t *loc, *ent;

    if (level.intermissionTime) {
        return;
    }
    if (level.nextTeamInfoTime <= 0 || level.time < level.nextTeamInfoTime) {
        return;
    }
    level.nextTeamInfoTime = -1;

    for (i = 0; i < g_maxclients.integer; i++) {
        ent = g_entities + i;

        if (ent->client->pers.connected != CON_CONNECTED || !ent->inuse) {
            continue;
        }

        if (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE) {
            loc = Team_GetLocation(ent);
            ent->client->ps.location = loc ? loc->health : 0;
        }
    }

    for (i = 0; i < g_maxclients.integer; i++) {
        ent = g_entities + i;

        if (ent->client->pers.connected != CON_CONNECTED || !ent->inuse) {
            continue;
        }

        if (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE) {
            TeamOverlayMessage(ent);
        }
    }
}

/*-----------------------------------------------------------------*/

/*QUAKED team_CTF_redplayer (1 0 0) (-16 -16 -16) (16 16 32)
Only in CTF games.  Red players spawn here at game start.
*/
void SP_team_CTF_redplayer(gentity_t* ent) {
}

/*QUAKED team_CTF_blueplayer (0 0 1) (-16 -16 -16) (16 16 32)
Only in CTF games.  Blue players spawn here at game start.
*/
void SP_team_CTF_blueplayer(gentity_t* ent) {
}

/*QUAKED team_CTF_redspawn (1 0 0) (-16 -16 -24) (16 16 32)
potential spawning position for red team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_redspawn(gentity_t* ent) {
}

/*QUAKED team_CTF_bluespawn (0 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for blue team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_bluespawn(gentity_t* ent) {
}

/*
================
Obelisks
================
*/

static void ObeliskRegen(gentity_t* self) {
    self->nextthink = level.time + g_obeliskRegenPeriod.integer * 1000;
    if (self->health >= g_obeliskHealth.integer) {
        return;
    }

    G_AddEvent(self, EV_POWERUP_REGEN, 0);
    self->health += g_obeliskRegenAmount.integer;
    if (self->health > g_obeliskHealth.integer) {
        self->health = g_obeliskHealth.integer;
    }

    self->activator->s.modelindex2 = self->health * 0xff / g_obeliskHealth.integer;
    self->activator->s.frame = 0;
}

static void ObeliskRespawn(gentity_t* self) {
    self->takedamage = qtrue;
    self->health = g_obeliskHealth.integer;

    self->think = ObeliskRegen;
    self->nextthink = level.time + g_obeliskRegenPeriod.integer * 1000;

    self->activator->s.frame = 0;
}

// [QL] 0x1006a6c0
static void ObeliskDie(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod) {
    int otherTeam;

    otherTeam = OtherTeam(self->spawnflags);
    AddTeamScore(self->s.pos.trBase, otherTeam, 1);
    Team_ForceGesture(otherTeam);

    self->takedamage = qfalse;
    self->think = ObeliskRespawn;
    self->nextthink = level.time + g_obeliskRespawnDelay.integer * 1000;

    self->activator->s.modelindex2 = 0xff;
    self->activator->s.frame = 2;

    G_AddEvent(self->activator, EV_OBELISKEXPLODE, 0);

    attacker->client->ps.persistant[PERS_CAPTURES]++;
    attacker->client->expandedStats.numCaptures++;
    AddScore(attacker, self->r.currentOrigin, CTF_CAPTURE_BONUS);

    // add the sprite over the player's head
    attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP);
    attacker->client->ps.eFlags |= EF_AWARD_CAP;
    attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

    teamgame.redObeliskAttackedTime = 0;
    teamgame.blueObeliskAttackedTime = 0;
}

// [QL] 0x1006a830: a Harvester base taking a carrier's skulls
static void ObeliskTouch(gentity_t* self, gentity_t* other, trace_t* trace) {
    int team;
    int tokens;

    if (!other->client) {
        return;
    }

    team = other->client->sess.sessionTeam;
    if (OtherTeam(team) != self->spawnflags) {
        return;
    }

    tokens = other->client->ps.generic1 & 0x3f;
    if (tokens <= 0) {
        return;
    }

    PrintMsg(NULL, "%s%s TEAM^3 CAPTURED %i skull%s!^7 (brought by %s)\n", TeamColorString(team), TeamName(team),
             tokens, (tokens == 1) ? "" : "s", other->client->pers.netname);

    AddTeamScore(self->s.pos.trBase, team, tokens);
    Team_ForceGesture(team);

    other->client->ps.persistant[PERS_CAPTURES] += tokens;
    other->client->expandedStats.numCaptures += tokens;
    AddScore(other, self->r.currentOrigin, CTF_CAPTURE_BONUS * tokens);

    // add the sprite over the player's head
    other->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP);
    other->client->ps.eFlags |= EF_AWARD_CAP;
    other->client->rewardTime = level.time + REWARD_SPRITE_TIME;

    other->client->ps.generic1 &= 0xc0;

    Team_CaptureFlagSound(self, self->spawnflags, other);

    if (!trap_HasAchievement(other->client->ps.clientNum, 0x20) && tokens >= 3) {
        trap_SetAchievement(other->client->ps.clientNum, 0x20);
    }
}

static void ObeliskPain(gentity_t* self, gentity_t* attacker, int damage) {
    int actualDamage = damage / 10;
    if (actualDamage <= 0) {
        actualDamage = 1;
    }
    self->activator->s.modelindex2 = self->health * 0xff / g_obeliskHealth.integer;
    if (!self->activator->s.frame) {
        G_AddEvent(self, EV_OBELISKPAIN, 0);
    }
    self->activator->s.frame = 1;
    AddScore(attacker, self->r.currentOrigin, actualDamage);
}

// spawn invisible damagable obelisk entity / harvester base trigger.
gentity_t* SpawnObelisk(vec3_t origin, vec3_t mins, vec3_t maxs, int team) {
    gentity_t* ent;

    ent = G_Spawn();

    VectorCopy(origin, ent->s.origin);
    VectorCopy(origin, ent->s.pos.trBase);
    VectorCopy(origin, ent->r.currentOrigin);

    VectorCopy(mins, ent->r.mins);
    VectorCopy(maxs, ent->r.maxs);

    ent->s.eType = ET_GENERAL;
    ent->flags = FL_NO_KNOCKBACK;

    if (g_gametype.integer == GT_OBELISK) {
        ent->r.contents = CONTENTS_SOLID;
        ent->takedamage = qtrue;
        ent->health = g_obeliskHealth.integer;
        ent->die = ObeliskDie;
        ent->pain = ObeliskPain;
        ent->think = ObeliskRegen;
        ent->nextthink = level.time + g_obeliskRegenPeriod.integer * 1000;
    }
    if (g_gametype.integer == GT_HARVESTER) {
        ent->r.contents = CONTENTS_TRIGGER;
        ent->touch = ObeliskTouch;
    }

    G_SetOrigin(ent, ent->s.origin);

    ent->spawnflags = team;

    trap_LinkEntity(ent);

    return ent;
}

// setup entity for team base model / obelisk model.
void ObeliskInit(gentity_t* ent) {
    trace_t tr;
    vec3_t dest;

    ent->s.eType = ET_TEAM;

    VectorSet(ent->r.mins, -15, -15, 0);
    VectorSet(ent->r.maxs, 15, 15, 87);

    if (ent->spawnflags & 1) {
        // suspended
        G_SetOrigin(ent, ent->s.origin);
    } else {
        // mappers like to put them exactly on the floor, but being coplanar
        // will sometimes show up as starting in solid, so lif it up one pixel
        ent->s.origin[2] += 1;

        // drop to floor
        VectorSet(dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096);
        trap_Trace(&tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID);
        if (tr.startsolid) {
            ent->s.origin[2] -= 1;
            G_Printf("SpawnObelisk: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));

            ent->s.groundEntityNum = ENTITYNUM_NONE;
            G_SetOrigin(ent, ent->s.origin);
        } else {
            // allow to ride movers
            ent->s.groundEntityNum = tr.entityNum;
            G_SetOrigin(ent, tr.endpos);
        }
    }
}

/*QUAKED team_redobelisk (1 0 0) (-16 -16 0) (16 16 8)
 */
void SP_team_redobelisk(gentity_t* ent) {
    gentity_t* obelisk;

    if (g_gametype.integer <= GT_TEAM) {
        G_FreeEntity(ent);
        return;
    }
    ObeliskInit(ent);
    if (g_gametype.integer == GT_OBELISK) {
        obelisk = SpawnObelisk(ent->s.origin, ent->r.mins, ent->r.maxs, TEAM_RED);
        obelisk->activator = ent;
        // initial obelisk health value
        ent->s.modelindex2 = 0xff;
        ent->s.frame = 0;
    }
    if (g_gametype.integer == GT_HARVESTER) {
        obelisk = SpawnObelisk(ent->s.origin, ent->r.mins, ent->r.maxs, TEAM_RED);
        obelisk->activator = ent;
    }
    ent->s.modelindex = TEAM_RED;
    trap_LinkEntity(ent);
}

/*QUAKED team_blueobelisk (0 0 1) (-16 -16 0) (16 16 88)
 */
void SP_team_blueobelisk(gentity_t* ent) {
    gentity_t* obelisk;

    if (g_gametype.integer <= GT_TEAM) {
        G_FreeEntity(ent);
        return;
    }
    ObeliskInit(ent);
    if (g_gametype.integer == GT_OBELISK) {
        obelisk = SpawnObelisk(ent->s.origin, ent->r.mins, ent->r.maxs, TEAM_BLUE);
        obelisk->activator = ent;
        // initial obelisk health value
        ent->s.modelindex2 = 0xff;
        ent->s.frame = 0;
    }
    if (g_gametype.integer == GT_HARVESTER) {
        obelisk = SpawnObelisk(ent->s.origin, ent->r.mins, ent->r.maxs, TEAM_BLUE);
        obelisk->activator = ent;
    }
    ent->s.modelindex = TEAM_BLUE;
    trap_LinkEntity(ent);
}

/*QUAKED team_neutralobelisk (0 0 1) (-16 -16 0) (16 16 88)
 */
void SP_team_neutralobelisk(gentity_t* ent) {
    if (g_gametype.integer != GT_1FCTF && g_gametype.integer != GT_HARVESTER) {
        G_FreeEntity(ent);
        return;
    }
    ObeliskInit(ent);
    if (g_gametype.integer == GT_HARVESTER) {
        neutralObelisk = SpawnObelisk(ent->s.origin, ent->r.mins, ent->r.maxs, TEAM_FREE);
        neutralObelisk->activator = ent;
    }
    ent->s.modelindex = TEAM_FREE;
    trap_LinkEntity(ent);
}

/*
================
CheckObeliskAttack
================
*/
qboolean CheckObeliskAttack(gentity_t* obelisk, gentity_t* attacker) {
    gentity_t* te;

    // if this really is an obelisk
    if (obelisk->die != ObeliskDie) {
        return qfalse;
    }

    // if the attacker is a client
    if (!attacker->client) {
        return qfalse;
    }

    // if the obelisk is on the same team as the attacker then don't hurt it
    if (obelisk->spawnflags == attacker->client->sess.sessionTeam) {
        return qtrue;
    }

    // obelisk may be hurt

    // if not played any sounds recently
    if ((obelisk->spawnflags == TEAM_RED &&
         teamgame.redObeliskAttackedTime < level.time - OVERLOAD_ATTACK_BASE_SOUND_TIME) ||
        (obelisk->spawnflags == TEAM_BLUE &&
         teamgame.blueObeliskAttackedTime < level.time - OVERLOAD_ATTACK_BASE_SOUND_TIME)) {
        // tell which obelisk is under attack
        te = G_TempEntity(obelisk->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
        if (obelisk->spawnflags == TEAM_RED) {
            te->s.eventParm = GTS_REDOBELISK_ATTACKED;
            teamgame.redObeliskAttackedTime = level.time;
        } else {
            te->s.eventParm = GTS_BLUEOBELISK_ATTACKED;
            teamgame.blueObeliskAttackedTime = level.time;
        }
        te->r.svFlags |= SVF_BROADCAST;
    }

    return qfalse;
}