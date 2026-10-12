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
#include "server.h"

static void SV_EmitPacketEntities(clientSnapshot_t* from, clientSnapshot_t* to, msg_t* msg) {
    entityState_t *oldent, *newent;
    int oldindex, newindex;
    int oldnum, newnum;
    int from_num_entities;

    if (!from) {
        from_num_entities = 0;
    } else {
        from_num_entities = from->num_entities;
    }

    newent = NULL;
    oldent = NULL;
    newindex = 0;
    oldindex = 0;
    while (newindex < to->num_entities || oldindex < from_num_entities) {
        if (newindex >= to->num_entities) {
            newnum = 9999;
        } else {
            newent = &svs.snapshotEntities[(to->first_entity + newindex) % svs.numSnapshotEntities];
            newnum = newent->number;
        }

        if (oldindex >= from_num_entities) {
            oldnum = 9999;
        } else {
            oldent = &svs.snapshotEntities[(from->first_entity + oldindex) % svs.numSnapshotEntities];
            oldnum = oldent->number;
        }

        if (newnum == oldnum) {
            MSG_WriteDeltaEntity(msg, oldent, newent, qfalse);
            oldindex++;
            newindex++;
            continue;
        }

        if (newnum < oldnum) {
            MSG_WriteDeltaEntity(msg, &sv.svEntities[newnum].baseline, newent, qtrue);
            newindex++;
            continue;
        }

        if (newnum > oldnum) {
            MSG_WriteDeltaEntity(msg, oldent, NULL, qtrue);
            oldindex++;
            continue;
        }
    }

    MSG_WriteBits(msg, (MAX_GENTITIES - 1), GENTITYNUM_BITS);
}

static void SV_WriteSnapshotToClient(client_t* client, msg_t* msg) {
    clientSnapshot_t *frame, *oldframe;
    int lastframe;
    int i;
    int snapFlags;

    frame = &client->frames[client->netchan.outgoingSequence & PACKET_MASK];

    if (client->deltaMessage <= 0 || client->state != CS_ACTIVE) {
        oldframe = NULL;
        lastframe = 0;
    } else if (client->netchan.outgoingSequence - client->deltaMessage >= (PACKET_BACKUP - 3)) {
        Com_DPrintf("%s: Delta request from out of date packet.\n", client->name);
        oldframe = NULL;
        lastframe = 0;
    } else {
        oldframe = &client->frames[client->deltaMessage & PACKET_MASK];
        lastframe = client->netchan.outgoingSequence - client->deltaMessage;

        if (oldframe->first_entity <= svs.nextSnapshotEntities - svs.numSnapshotEntities) {
            Com_DPrintf("%s: Delta request from out of date entities.\n", client->name);
            oldframe = NULL;
            lastframe = 0;
        }
    }

    MSG_WriteByte(msg, svc_snapshot);

    if (client->oldServerTime) {
        MSG_WriteLong(msg, sv.time + client->oldServerTime);
    } else {
        MSG_WriteLong(msg, sv.time);
    }

    MSG_WriteByte(msg, lastframe);

    snapFlags = svs.snapFlagServerBit;
    if (client->rateDelayed) {
        snapFlags |= SNAPFLAG_RATE_DELAYED;
    }
    if (client->state != CS_ACTIVE) {
        snapFlags |= SNAPFLAG_NOT_ACTIVE;
    }

    MSG_WriteByte(msg, snapFlags);

    MSG_WriteByte(msg, frame->areabytes);
    MSG_WriteData(msg, frame->areabits, frame->areabytes);

    if (oldframe) {
        MSG_WriteDeltaPlayerstate(msg, &oldframe->ps, &frame->ps);
    } else {
        MSG_WriteDeltaPlayerstate(msg, NULL, &frame->ps);
    }

    SV_EmitPacketEntities(oldframe, frame, msg);

    if (sv_padPackets->integer) {
        for (i = 0; i < sv_padPackets->integer; i++) {
            MSG_WriteByte(msg, svc_nop);
        }
    }
}

void SV_UpdateServerCommandsToClient(client_t* client, msg_t* msg) {
    int i;

    for (i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++) {
        MSG_WriteByte(msg, svc_serverCommand);
        MSG_WriteLong(msg, i);
        MSG_WriteString(msg, client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)]);
    }
    client->reliableSent = client->reliableSequence;
}

typedef struct {
    int numSnapshotEntities;
    int snapshotEntities[MAX_SNAPSHOT_ENTITIES];
} snapshotEntityNumbers_t;

static int QDECL SV_QsortEntityNumbers(const void* a, const void* b) {
    int *ea, *eb;

    ea = (int*)a;
    eb = (int*)b;

    if (*ea == *eb) {
        Com_Error(ERR_DROP, "SV_QsortEntityStates: duplicated entity");
    }

    if (*ea < *eb) {
        return -1;
    }

    return 1;
}

static void SV_AddEntToSnapshot(svEntity_t* svEnt, sharedEntity_t* gEnt, snapshotEntityNumbers_t* eNums) {
    if (svEnt->snapshotCounter == sv.snapshotCounter) {
        return;
    }
    svEnt->snapshotCounter = sv.snapshotCounter;

    if (eNums->numSnapshotEntities == MAX_SNAPSHOT_ENTITIES) {
        return;
    }

    eNums->snapshotEntities[eNums->numSnapshotEntities] = gEnt->s.number;
    eNums->numSnapshotEntities++;
}

static void SV_AddEntitiesVisibleFromPoint(vec3_t origin, clientSnapshot_t* frame, snapshotEntityNumbers_t* eNums, qboolean portal, int viewerNum) {
    int e, i;
    sharedEntity_t* ent;
    svEntity_t* svEnt;
    int l;
    int clientarea, clientcluster;
    int leafnum;
    byte* clientpvs;
    byte* bitvector;

    if (!sv.state) {
        return;
    }

    leafnum = CM_PointLeafnum(origin);
    clientarea = CM_LeafArea(leafnum);
    clientcluster = CM_LeafCluster(leafnum);

    frame->areabytes = CM_WriteAreaBits(frame->areabits, clientarea);

    clientpvs = CM_ClusterPVS(clientcluster);

    for (e = 0; e < sv.num_entities; e++) {
        ent = SV_GentityNum(e);

        if (!ent->r.linked) {
            continue;
        }

        if (ent->s.number != e) {
            Com_DPrintf("FIXING ENT->S.NUMBER!!!\n");
            ent->s.number = e;
        }

        if (ent->r.svFlags & SVF_NOCLIENT) {
            continue;
        }

        if (ent->r.svFlags & SVF_SINGLECLIENT) {
            if (frame->ps.eFlags & EF_SPECTATOR) {
                if (ent->r.singleClient != viewerNum) {
                    continue;
                }
            } else if (ent->r.singleClient != frame->ps.clientNum) {
                continue;
            }
        }
        if (ent->r.svFlags & SVF_NOTSINGLECLIENT) {
            if (ent->r.singleClient == frame->ps.clientNum) {
                continue;
            }
        }
        if (ent->r.svFlags & SVF_CLIENTMASK) {
            if (frame->ps.clientNum >= 32)
                Com_Error(ERR_DROP, "SVF_CLIENTMASK: clientNum >= 32");
            if (~ent->r.singleClient & (1 << frame->ps.clientNum))
                continue;
        }

        svEnt = SV_SvEntityForGentity(ent);

        if (svEnt->snapshotCounter == sv.snapshotCounter) {
            continue;
        }

        if (ent->r.svFlags & SVF_BROADCAST) {
            SV_AddEntToSnapshot(svEnt, ent, eNums);
            continue;
        }

        if (SV_GameSnapshotSkipEntity(viewerNum, e)) {
            continue;
        }

        if (e < MAX_CLIENTS && SV_GameSnapshotAlwaysSend(viewerNum, ent->s.number)) {
            SV_AddEntToSnapshot(svEnt, ent, eNums);
            continue;
        }

        if (e >= MAX_CLIENTS && SV_GameEntityAlwaysSend(e)) {
            SV_AddEntToSnapshot(svEnt, ent, eNums);
            continue;
        }

        if (!CM_AreasConnected(clientarea, svEnt->areanum)) {
            if (!CM_AreasConnected(clientarea, svEnt->areanum2)) {
                continue;
            }
        }

        bitvector = clientpvs;

        if (!svEnt->numClusters) {
            continue;
        }
        l = 0;
        for (i = 0; i < svEnt->numClusters; i++) {
            l = svEnt->clusternums[i];
            if (bitvector[l >> 3] & (1 << (l & 7))) {
                break;
            }
        }

        if (i == svEnt->numClusters) {
            if (svEnt->lastCluster) {
                for (; l <= svEnt->lastCluster; l++) {
                    if (bitvector[l >> 3] & (1 << (l & 7))) {
                        break;
                    }
                }
                if (l == svEnt->lastCluster) {
                    continue;
                }
            } else {
                continue;
            }
        }

        SV_AddEntToSnapshot(svEnt, ent, eNums);

        if (ent->r.svFlags & SVF_PORTAL) {
            if (ent->s.generic1) {
                vec3_t dir;
                VectorSubtract(ent->s.origin, origin, dir);
                if (VectorLengthSquared(dir) > (float)ent->s.generic1 * ent->s.generic1) {
                    continue;
                }
            }
            SV_AddEntitiesVisibleFromPoint(ent->s.origin2, frame, eNums, qtrue, viewerNum);
        }
    }
}

static void SV_BuildClientSnapshot(client_t* client) {
    vec3_t org;
    clientSnapshot_t* frame;
    snapshotEntityNumbers_t entityNumbers;
    int i;
    sharedEntity_t* ent;
    entityState_t* state;
    svEntity_t* svEnt;
    sharedEntity_t* clent;
    int clientNum;
    playerState_t* ps;

    sv.snapshotCounter++;

    frame = &client->frames[client->netchan.outgoingSequence & PACKET_MASK];

    entityNumbers.numSnapshotEntities = 0;
    Com_Memset(frame->areabits, 0, sizeof(frame->areabits));

    frame->num_entities = 0;

    clent = client->gentity;
    if (!clent || client->state == CS_ZOMBIE) {
        return;
    }

    ps = SV_GameClientNum(client - svs.clients);
    frame->ps = *ps;

    clientNum = frame->ps.clientNum;
    if (clientNum < 0 || clientNum >= MAX_GENTITIES) {
        Com_Error(ERR_DROP, "SV_SvEntityForGentity: bad gEnt");
    }
    svEnt = &sv.svEntities[clientNum];

    svEnt->snapshotCounter = sv.snapshotCounter;

    VectorCopy(ps->origin, org);
    org[2] += ps->viewheight;

    SV_AddEntitiesVisibleFromPoint(org, frame, &entityNumbers, qfalse, clent->s.number);

    qsort(entityNumbers.snapshotEntities, entityNumbers.numSnapshotEntities,
          sizeof(entityNumbers.snapshotEntities[0]), SV_QsortEntityNumbers);

    for (i = 0; i < MAX_MAP_AREA_BYTES / 4; i++) {
        ((int*)frame->areabits)[i] = ((int*)frame->areabits)[i] ^ -1;
    }

    frame->num_entities = 0;
    frame->first_entity = svs.nextSnapshotEntities;
    for (i = 0; i < entityNumbers.numSnapshotEntities; i++) {
        ent = SV_GentityNum(entityNumbers.snapshotEntities[i]);
        state = &svs.snapshotEntities[svs.nextSnapshotEntities % svs.numSnapshotEntities];
        *state = ent->s;

        if (state->number < MAX_CLIENTS && SV_GameSnapshotVisibility(clent->s.clientNum, state->number)) {
            state->health = 0;
            state->armor = 0;
            state->location = 0;
        }

        svs.nextSnapshotEntities++;
        if (svs.nextSnapshotEntities >= 0x7FFFFFFE) {
            Com_Error(ERR_FATAL, "svs.nextSnapshotEntities wrapped");
        }
        frame->num_entities++;
    }
}

Q_HOOKABLE void SV_SendMessageToClient(msg_t* msg, client_t* client) {
    client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
    client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = Sys_Milliseconds();
    client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = -1;

    SV_Netchan_Transmit(client, msg);
}

void SV_SendClientSnapshot(client_t* client) {
    byte msg_buf[MAX_MSGLEN];
    msg_t msg;

    SV_BuildClientSnapshot(client);

    if (client->gentity && client->gentity->r.svFlags & SVF_BOT) {
        return;
    }

    MSG_Init(&msg, msg_buf, sizeof(msg_buf));
    msg.allowoverflow = qtrue;

    MSG_WriteLong(&msg, client->lastClientCommand);

    SV_UpdateServerCommandsToClient(client, &msg);

    SV_WriteSnapshotToClient(client, &msg);

    if (msg.overflowed) {
        Com_Printf("WARNING: msg overflowed for %s\n", client->name);
        MSG_Clear(&msg);
    }

    SV_SendMessageToClient(&msg, client);
}

void SV_SendClientMessages(void) {
    int i;
    client_t* c;

    for (i = 0; i < sv_maxclients->integer; i++) {
        c = &svs.clients[i];

        if (!c->state)
            continue;

        if (svs.time - c->nextSnapshotTime < c->snapshotMsec * com_timescale->value)
            continue;

        if (*c->downloadName)
            continue;

        if (c->netchan.unsentFragments || c->netchan_start_queue) {
            c->rateDelayed = qtrue;
            continue;
        }

        if (!(c->netchan.remoteAddress.type == NA_LOOPBACK ||
              (sv_lanForceRate->integer && Sys_IsLANAddress(c->netchan.remoteAddress)))) {
            if (SV_RateMsec(c) > 0) {
                c->rateDelayed = qtrue;
                continue;
            }
        }

        SV_SendClientSnapshot(c);
        c->nextSnapshotTime = svs.time;
        c->rateDelayed = qfalse;
    }
}
