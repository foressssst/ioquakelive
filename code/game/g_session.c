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
#include "g_local.h"

void G_WriteClientSessionData(gclient_t* client) {
    const char* s;
    const char* var;

    s = va("%i %ld %i %i %i %i %i %i %i %i %i %i %i",
           client->sess.sessionTeam,
           client->sess.spectatorTime,
           client->sess.spectatorState,
           client->sess.spectatorClient,
           client->sess.weaponPrimary,
           client->sess.wins,
           client->sess.losses,
           client->sess.teamLeader,
           client->sess.privileges,
           client->sess.specOnly,
           client->sess.playQueue,
           client->sess.muted,
           client->sess.prevScore);

    var = va("session%i", (int)(client - level.clients));

    trap_Cvar_Set(var, s);
}

void G_ReadSessionData(gclient_t* client) {
    char s[MAX_STRING_CHARS];
    const char* var;
    int teamLeader;
    int spectatorState;
    int sessionTeam;

    var = va("session%i", (int)(client - level.clients));
    trap_Cvar_VariableStringBuffer(var, s, sizeof(s));

    sscanf(s, "%i %ld %i %i %i %i %i %i %i %i %i %i %i",
           &sessionTeam,
           &client->sess.spectatorTime,
           &spectatorState,
           &client->sess.spectatorClient,
           &client->sess.weaponPrimary,
           &client->sess.wins,
           &client->sess.losses,
           &teamLeader,
           &client->sess.privileges,
           &client->sess.specOnly,
           &client->sess.playQueue,
           &client->sess.muted,
           &client->sess.prevScore);

    if (g_teamSpawnAsSpec.integer && g_gametype.integer >= GT_TEAM && level.warmupTime) {
        sessionTeam = TEAM_SPECTATOR;
    }

    client->sess.sessionTeam = (team_t)sessionTeam;
    client->sess.spectatorState = (spectatorState_t)spectatorState;
    client->sess.teamLeader = (qboolean)teamLeader;
}

void G_InitSessionData(gclient_t* client, char* userinfo) {
    clientSession_t* sess;

    sess = &client->sess;

    if (g_gametype.integer >= GT_TEAM) {
        if (g_teamAutoJoin.integer) {
            sess->sessionTeam = PickTeam(-1);
            BroadcastTeamChange(client, -1);
        } else {
            sess->sessionTeam = TEAM_SPECTATOR;
        }
    } else {
        sess->sessionTeam = TEAM_SPECTATOR;
    }

    sess->spectatorState = SPECTATOR_FREE;
    sess->spectatorTime = (int)time(NULL);
    sess->privileges = G_GetAccess((int)(client - level.clients));
    sess->playQueue = 0;
    sess->specOnly = (g_gametype.integer == GT_DUEL) ? 1 : 0;

    G_WriteClientSessionData(client);
}

void G_InitWorldSession(void) {
    char s[MAX_STRING_CHARS];
    int gt;

    trap_Cvar_VariableStringBuffer("session", s, sizeof(s));
    gt = atoi(s);

    if (g_gametype.integer != gt) {
        level.newSession = qtrue;
        G_Printf("Gametype changed, clearing session data.\n");
    }
}

void G_WriteSessionData(void) {
    int i;

    trap_Cvar_Set("session", va("%i", g_gametype.integer));

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            G_WriteClientSessionData(&level.clients[i]);
        }
    }
}
