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
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "server.h"

static void SV_Netchan_Encode(client_t* client, msg_t* msg, const char* clientCommandString) {
    long i, index;
    byte key, *string;
    int srdc, sbit;
    qboolean soob;

    if (msg->cursize < SV_ENCODE_START) {
        return;
    }

    srdc = msg->readcount;
    sbit = msg->bit;
    soob = msg->oob;

    msg->bit = 0;
    msg->readcount = 0;
    msg->oob = qfalse;

     MSG_ReadLong(msg);

    msg->oob = soob;
    msg->bit = sbit;
    msg->readcount = srdc;

    string = (byte*)clientCommandString;
    index = 0;
    key = client->challenge ^ client->netchan.outgoingSequence;
    for (i = SV_ENCODE_START; i < msg->cursize; i++) {
        if (!string[index])
            index = 0;
        if (string[index] > 127 || string[index] == '%') {
            key ^= '.' << (i & 1);
        } else {
            key ^= string[index] << (i & 1);
        }
        index++;
        *(msg->data + i) = *(msg->data + i) ^ key;
    }
}

static void SV_Netchan_Decode(client_t* client, msg_t* msg) {
    int serverId, messageAcknowledge, reliableAcknowledge;
    int i, index, srdc, sbit;
    qboolean soob;
    byte key, *string;

    srdc = msg->readcount;
    sbit = msg->bit;
    soob = msg->oob;

    msg->oob = qfalse;

    serverId = MSG_ReadLong(msg);
    messageAcknowledge = MSG_ReadLong(msg);
    reliableAcknowledge = MSG_ReadLong(msg);

    msg->oob = soob;
    msg->bit = sbit;
    msg->readcount = srdc;

    string = (byte*)client->reliableCommands[reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1)];
    index = 0;
    key = client->challenge ^ serverId ^ messageAcknowledge;
    for (i = msg->readcount + SV_DECODE_START; i < msg->cursize; i++) {
        if (!string[index])
            index = 0;
        if (string[index] > 127 || string[index] == '%') {
            key ^= '.' << (i & 1);
        } else {
            key ^= string[index] << (i & 1);
        }
        index++;
        *(msg->data + i) = *(msg->data + i) ^ key;
    }
}

void SV_Netchan_FreeQueue(client_t* client) {
    netchan_buffer_t *netbuf, *next;

    for (netbuf = client->netchan_start_queue; netbuf; netbuf = next) {
        next = netbuf->next;
        Z_Free(netbuf);
    }

    client->netchan_start_queue = NULL;
    client->netchan_end_queue = &client->netchan_start_queue;
}

void SV_Netchan_TransmitNextInQueue(client_t* client) {
    netchan_buffer_t* netbuf;

    Com_DPrintf("#462 Netchan_TransmitNextFragment: popping a queued message for transmit\n");
    netbuf = client->netchan_start_queue;

    SV_Netchan_Encode(client, &netbuf->msg, netbuf->clientCommandString);

    Netchan_Transmit(&client->netchan, netbuf->msg.cursize, netbuf->msg.data);

    client->netchan_start_queue = netbuf->next;
    if (!client->netchan_start_queue) {
        Com_DPrintf("#462 Netchan_TransmitNextFragment: emptied queue\n");
        client->netchan_end_queue = &client->netchan_start_queue;
    } else
        Com_DPrintf("#462 Netchan_TransmitNextFragment: remaining queued message\n");

    Z_Free(netbuf);
}

int SV_Netchan_TransmitNextFragment(client_t* client) {
    if (client->netchan.unsentFragments) {
        Netchan_TransmitNextFragment(&client->netchan);
        return SV_RateMsec(client);
    } else if (client->netchan_start_queue) {
        SV_Netchan_TransmitNextInQueue(client);
        return SV_RateMsec(client);
    }

    return -1;
}

void SV_Netchan_Transmit(client_t* client, msg_t* msg) {
    MSG_WriteByte(msg, svc_EOF);

    if (client->netchan.unsentFragments || client->netchan_start_queue) {
        netchan_buffer_t* netbuf;
        Com_DPrintf("#462 SV_Netchan_Transmit: unsent fragments, stacked\n");
        netbuf = (netchan_buffer_t*)Z_Malloc(sizeof(netchan_buffer_t));
        MSG_Copy(&netbuf->msg, netbuf->msgBuffer, sizeof(netbuf->msgBuffer), msg);
        Q_strncpyz(netbuf->clientCommandString, client->lastClientCommandString,
                   sizeof(netbuf->clientCommandString));
        netbuf->next = NULL;
        *client->netchan_end_queue = netbuf;
        client->netchan_end_queue = &(*client->netchan_end_queue)->next;
    } else {
        SV_Netchan_Encode(client, msg, client->lastClientCommandString);
        Netchan_Transmit(&client->netchan, msg->cursize, msg->data);
    }
}

qboolean SV_Netchan_Process(client_t* client, msg_t* msg) {
    int ret;
    ret = Netchan_Process(&client->netchan, msg);
    if (!ret)
        return qfalse;

    SV_Netchan_Decode(client, msg);

    return qtrue;
}
