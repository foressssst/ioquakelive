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

static void CG_ResetEntity(centity_t* cent) {
    if (cent->snapShotTime < cg.time - EVENT_VALID_MSEC) {
        cent->previousEvent = 0;
    }

    cent->trailTime = cg.snap->serverTime;
    cent->leadTrTime = cent->currentState.pos.trTime;

    VectorCopy(cent->currentState.origin, cent->lerpOrigin);
    VectorCopy(cent->currentState.angles, cent->lerpAngles);
    if (cent->currentState.eType == ET_PLAYER) {
        CG_ResetPlayerEntity(cent);
    }
}

static void CG_TransitionEntity(centity_t* cent) {
    cent->currentState = cent->nextState;
    cent->currentValid = qtrue;

    if (!cent->interpolate) {
        CG_ResetEntity(cent);
    }

    cent->interpolate = qfalse;

    CG_CheckEvents(cent);
}

void CG_DemoRewind(int snapshotNum, int serverCommandSequence) {
    int clientNum = cg.clientNum;
    qboolean showJoinMenu = cg.showJoinMenu;
    int i;

    memset(&cg, 0, sizeof(cg));
    memset(cg_entities, 0, sizeof(cg_entities));
    cg.clientNum = clientNum;
    cg.race.nextCheckpointEnt = -1;
    cg.race.nextNextCheckpointEnt = -1;
    cg.autoFollowClient = -1;
    CG_ClearChat();
    cg.speedBarColor1[0] = 0.0f; cg.speedBarColor1[1] = 1.0f;
    cg.speedBarColor1[2] = 0.0f; cg.speedBarColor1[3] = 1.0f;
    cg.speedBarColor2[0] = 1.0f; cg.speedBarColor2[1] = 1.0f;
    cg.speedBarColor2[2] = 0.0f; cg.speedBarColor2[3] = 1.0f;
    cg.weaponSelect = WP_MACHINEGUN;
    cg.showJoinMenu = showJoinMenu;

    CG_InitLocalEntities();
    CG_InitMarkPolys();
    CG_ClearParticles();
    CG_InitTeamChat();
    trap_S_ClearLoopingSounds(qtrue);

    trap_GetGameState(&cgs.gameState);
    CG_ParseServerinfo();
    CG_SetConfigValues();
    for (i = 0; i < MAX_CLIENTS; i++) {
        CG_NewClientInfo(i);
    }
    CG_BuildSpectatorString();
    CG_ShaderStateChanged();
    CG_StartMusic();

    cg.latestSnapshotNum = snapshotNum;
    cgs.processedSnapshotNum = snapshotNum - 1;
    cgs.serverCommandSequence = serverCommandSequence;
}

void CG_SetInitialSnapshot(snapshot_t* snap) {
    int i;
    centity_t* cent;
    entityState_t* state;

    cg.snap = snap;

    BG_PlayerStateToEntityState(&snap->ps, &cg_entities[snap->ps.clientNum].currentState, qfalse);

    CG_BuildSolidList();

    CG_ExecuteNewServerCommands(snap->serverCommandSequence);

    CG_Respawn();

    for (i = 0; i < cg.snap->numEntities; i++) {
        state = &cg.snap->entities[i];
        cent = &cg_entities[state->number];

        memcpy(&cent->currentState, state, sizeof(entityState_t));
        cent->interpolate = qfalse;
        cent->currentValid = qtrue;

        CG_ResetEntity(cent);

        CG_CheckEvents(cent);
    }
}

static void CG_TransitionSnapshot(void) {
    centity_t* cent;
    snapshot_t* oldFrame;
    int i;

    if (!cg.snap) {
        CG_Error("CG_TransitionSnapshot: NULL cg.snap");
    }
    if (!cg.nextSnap) {
        CG_Error("CG_TransitionSnapshot: NULL cg.nextSnap");
    }

    CG_ExecuteNewServerCommands(cg.nextSnap->serverCommandSequence);

    if (cg.mapRestart) {
    }

    for (i = 0; i < cg.snap->numEntities; i++) {
        cent = &cg_entities[cg.snap->entities[i].number];
        cent->currentValid = qfalse;
    }

    oldFrame = cg.snap;
    cg.snap = cg.nextSnap;

    BG_PlayerStateToEntityState(&cg.snap->ps, &cg_entities[cg.snap->ps.clientNum].currentState, qfalse);
    cg_entities[cg.snap->ps.clientNum].interpolate = qfalse;

    for (i = 0; i < cg.snap->numEntities; i++) {
        cent = &cg_entities[cg.snap->entities[i].number];
        CG_TransitionEntity(cent);

        if ((cent->currentState.eType == ET_MISSILE || cent->currentState.eType == ET_GRAPPLE) &&
            cent->currentState.weapon == WP_GRAPPLING_HOOK &&
            cent->currentState.otherEntityNum >= 0 && cent->currentState.otherEntityNum < MAX_CLIENTS) {
            cg.hookEntities[cent->currentState.otherEntityNum] = cent;
        }

        cent->snapShotTime = cg.snap->serverTime;
    }

    cg.nextSnap = NULL;

    if (oldFrame) {
        playerState_t *ops, *ps;

        ops = &oldFrame->ps;
        ps = &cg.snap->ps;
        if ((ps->eFlags ^ ops->eFlags) & EF_TELEPORT_BIT) {
            cg.thisFrameTeleport = qtrue;
        }

        if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) || cg_nopredict.integer) {
            CG_TransitionPlayerState(ps, ops);
        }
    }

    if (cg.reloadOtherClients == qtrue) {
        cg.reloadOtherClients = qfalse;
        CG_ReloadOtherClients();
        CG_LoadDeferredPlayers();
    }
}

static void CG_SetNextSnap(snapshot_t* snap) {
    int num;
    entityState_t* es;
    centity_t* cent;

    cg.nextSnap = snap;

    BG_PlayerStateToEntityState(&snap->ps, &cg_entities[snap->ps.clientNum].nextState, qfalse);
    cg_entities[cg.snap->ps.clientNum].interpolate = qtrue;

    for (num = 0; num < snap->numEntities; num++) {
        es = &snap->entities[num];
        cent = &cg_entities[es->number];

        memcpy(&cent->nextState, es, sizeof(entityState_t));

        if (!cent->currentValid || ((cent->currentState.eFlags ^ es->eFlags) & EF_TELEPORT_BIT)) {
            cent->interpolate = qfalse;
        } else {
            cent->interpolate = qtrue;
        }
    }

    if (cg.snap && ((snap->ps.eFlags ^ cg.snap->ps.eFlags) & EF_TELEPORT_BIT)) {
        cg.nextFrameTeleport = qtrue;
    } else {
        cg.nextFrameTeleport = qfalse;
    }

    if (cg.nextSnap->ps.clientNum != cg.snap->ps.clientNum) {
        cg.nextFrameTeleport = qtrue;
    }

    if ((cg.nextSnap->snapFlags ^ cg.snap->snapFlags) & SNAPFLAG_SERVERCOUNT) {
        cg.nextFrameTeleport = qtrue;
    }

    CG_BuildSolidList();
}

static snapshot_t* CG_ReadNextSnapshot(void) {
    qboolean r;
    snapshot_t* dest;

    if (cg.latestSnapshotNum > cgs.processedSnapshotNum + 1000 && !cg.demoPlayback) {
        CG_Printf("WARNING: CG_ReadNextSnapshot: way out of range, %i > %i\n",
                  cg.latestSnapshotNum, cgs.processedSnapshotNum);
    }

    while (cgs.processedSnapshotNum < cg.latestSnapshotNum) {
        if (cg.snap == &cg.activeSnapshots[0]) {
            dest = &cg.activeSnapshots[1];
        } else {
            dest = &cg.activeSnapshots[0];
        }

        cgs.processedSnapshotNum++;
        r = trap_GetSnapshot(cgs.processedSnapshotNum, dest);

        if (cg.snap && r && dest->serverTime == cg.snap->serverTime) {
        }

        if (r) {
            CG_AddLagometerSnapshotInfo(dest);
            return dest;
        }

        CG_AddLagometerSnapshotInfo(NULL);

    }

    return NULL;
}

void CG_ProcessSnapshots(void) {
    snapshot_t* snap;
    int n;

    trap_GetCurrentSnapshotNumber(&n, &cg.latestSnapshotTime);
    if (n != cg.latestSnapshotNum) {
        if (n < cg.latestSnapshotNum) {
            CG_Error("CG_ProcessSnapshots: n < cg.latestSnapshotNum");
        }
        cg.latestSnapshotNum = n;
    }

    while (!cg.snap) {
        snap = CG_ReadNextSnapshot();
        if (!snap) {
            return;
        }

        if (!(snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
            CG_SetInitialSnapshot(snap);
        }
    }

    do {
        if (!cg.nextSnap) {
            snap = CG_ReadNextSnapshot();

            if (!snap) {
                break;
            }

            CG_SetNextSnap(snap);

            if (cg.nextSnap->serverTime < cg.snap->serverTime) {
                CG_Error("CG_ProcessSnapshots: Server time went backwards");
            }
        }

        if (cg.time >= cg.snap->serverTime && cg.time < cg.nextSnap->serverTime) {
            break;
        }

        CG_TransitionSnapshot();
    } while (1);

    if (cg.snap == NULL) {
        CG_Error("CG_ProcessSnapshots: cg.snap == NULL");
    }
    if (cg.time < cg.snap->serverTime) {
        cg.time = cg.snap->serverTime;
    }
    if (cg.nextSnap != NULL && cg.nextSnap->serverTime <= cg.time) {
        CG_Error("CG_ProcessSnapshots: cg.nextSnap->serverTime <= cg.time");
    }
}
