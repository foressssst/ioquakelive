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
#include "../qcommon/q_steam.h"

static void SV_CloseDownload(client_t* cl);

void SV_SteamAuthResponse(uint64_t steamId, int response) {
    const char* reason;
    int i;

    if (!com_sv_running->integer || !svs.clients || !sv_maxclients) {
        return;
    }
    for (i = 0; i < sv_maxclients->integer; i++) {
        client_t* cl = &svs.clients[i];

        if (cl->state == CS_FREE || cl->steam_id != steamId) {
            continue;
        }
        if (response == 0) {
            return;
        }
        reason = Steam_AuthResponseName(response);
        if (reason) {
            SV_DropClient(cl, va("Failed to authenticate with Steam: %s", reason));
        } else {
            SV_DropClient(cl, va("Failed to authenticate with Steam: (Err:%i)", response));
        }
        return;
    }
    Com_Printf("Steam OnValidateAuthTicketResponse: couldn't match auth response to a client\n");
}

void SV_GetChallenge(netadr_t from, msg_t* msg) {
    int i;
    int oldest;
    int oldestTime;
    int tokenLen;
    challenge_t* challenge;

    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SV_GetChallenge: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    if (SVC_RateLimit(&outboundLeakyBucket, 10, 100)) {
        Com_DPrintf("SV_GetChallenge: rate limit exceeded, dropping request\n");
        return;
    }

    if (sv_serverType->integer == 1 && !Sys_IsLANAddress(from)) {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for LAN clients only.\n");
        return;
    }

    oldest = 0;
    oldestTime = 0x7fffffff;

    challenge = &svs.challenges[0];
    for (i = 0; i < MAX_CHALLENGES; i++, challenge++) {
        if (!challenge->connected && NET_CompareAdr(from, challenge->adr)) {
            break;
        }
        if (challenge->time < oldestTime) {
            oldestTime = challenge->time;
            oldest = i;
        }
    }

    if (i == MAX_CHALLENGES) {
        challenge = &svs.challenges[oldest];
        challenge->challenge = (((unsigned int)rand() << 16) ^ (unsigned int)rand()) ^ svs.time;
        challenge->adr = from;
        challenge->firstTime = svs.time;
        challenge->time = svs.time;
        challenge->connected = qfalse;
        challenge->wasRefused[0] = '\0';
    }

    tokenLen = msg->cursize - 0x11;
    if (tokenLen < 11) {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nNo Steam auth token.\n");
        return;
    }
    if (tokenLen > (int)sizeof(challenge->authToken)) {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nAuth token too large.\n");
        return;
    }
    Com_Memcpy(&challenge->steamKey, msg->data + 0x11, sizeof(challenge->steamKey));
    Com_Memcpy(challenge->authToken, msg->data + 0x19, tokenLen);
    challenge->authTokenLen = tokenLen;
    challenge->pingTime = svs.time;

    NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i", challenge->challenge);
}

static qboolean SV_IsBanned(netadr_t* from, qboolean isexception) {
    int index;
    serverBan_t* curban;

    if (!isexception) {
        if (SV_IsBanned(from, qtrue))
            return qfalse;
    }

    for (index = 0; index < serverBansCount; index++) {
        curban = &serverBans[index];

        if (curban->isexception == isexception) {
            if (NET_CompareBaseAdrMask(curban->ip, *from, curban->subnet))
                return qtrue;
        }
    }

    return qfalse;
}

void SV_DirectConnect(netadr_t from) {
    char userinfo[MAX_INFO_STRING];
    int i;
    client_t *cl, *newcl;
    client_t temp;
    sharedEntity_t* ent;
    int clientNum;
    int version;
    int qport;
    int challenge;
    int challengeNum = -1;
    char* password;
    int startIndex;
    char* denied;
    int count;
    char* ip;
    Com_DPrintf("SVC_DirectConnect ()\n");

    if (SV_IsBanned(&from, qfalse)) {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nYou are banned from this server.\n");
        return;
    }

    Q_strncpyz(userinfo, Cmd_Argv(1), sizeof(userinfo));

    version = atoi(Info_ValueForKey(userinfo, "protocol"));

    if (version != com_protocol->integer) {
        NET_OutOfBandPrint(NS_SERVER, from,
                           "print\nServer uses protocol version %i "
                           "(yours is %i).\n",
                           com_protocol->integer, version);
        Com_DPrintf("    rejected connect from version %i\n", version);
        return;
    }

    challenge = atoi(Info_ValueForKey(userinfo, "challenge"));
    qport = atoi(Info_ValueForKey(userinfo, "qport"));

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state == CS_FREE) {
            continue;
        }
        if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress) && (cl->netchan.qport == qport || from.port == cl->netchan.remoteAddress.port)) {
            if ((svs.time - cl->lastConnectTime) < (sv_reconnectlimit->integer * 1000)) {
                Com_DPrintf("%s:reconnect rejected : too soon\n", NET_AdrToString(from));
                return;
            }
            break;
        }
    }

    if (NET_IsLocalAddress(from))
        ip = "localhost";
    else
        ip = (char*)NET_AdrToString(from);
    if ((strlen(ip) + strlen(userinfo) + 4) >= MAX_INFO_STRING) {
        NET_OutOfBandPrint(NS_SERVER, from,
                           "print\nUserinfo string length exceeded.  "
                           "Try removing setu cvars from your config.\n");
        return;
    }
    Info_SetValueForKey(userinfo, "ip", ip);

    if (!NET_IsLocalAddress(from)) {
        int ping;
        challenge_t* challengeptr;

        for (i = 0; i < MAX_CHALLENGES; i++) {
            if (NET_CompareAdr(from, svs.challenges[i].adr)) {
                if (challenge == svs.challenges[i].challenge)
                    break;
            }
        }

        if (i == MAX_CHALLENGES) {
            NET_OutOfBandPrint(NS_SERVER, from, "print\nNo or bad challenge for your address.\n");
            return;
        }

        challengeptr = &svs.challenges[i];
        challengeNum = i;

        if (challengeptr->wasRefused[0]) {
            return;
        }

        ping = svs.time - challengeptr->pingTime;

        if (!Sys_IsLANAddress(from)) {
            if (sv_minPing->value && ping < sv_minPing->value) {
                NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for high pings only\n");
                Com_DPrintf("Client %i rejected on a too low ping\n", i);
                Q_strncpyz(challengeptr->wasRefused, "ping too low", sizeof(challengeptr->wasRefused));
                return;
            }
            if (sv_maxPing->value && ping > sv_maxPing->value) {
                NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for low pings only\n");
                Com_DPrintf("Client %i rejected on a too high ping\n", i);
                Q_strncpyz(challengeptr->wasRefused, "ping too high", sizeof(challengeptr->wasRefused));
                return;
            }
        }

        Com_Printf("Client %i connecting with %i challenge ping\n", i, ping);
        challengeptr->connected = qtrue;
    }

    newcl = &temp;
    Com_Memset(newcl, 0, sizeof(client_t));

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state == CS_FREE) {
            continue;
        }
        if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress) && (cl->netchan.qport == qport || from.port == cl->netchan.remoteAddress.port)) {
            Com_Printf("%s:reconnect\n", NET_AdrToString(from));
            newcl = cl;

            goto gotnewcl;
        }
    }

    password = Info_ValueForKey(userinfo, "password");
    if (*password && !strcmp(password, sv_privatePassword->string)) {
        startIndex = 0;
    } else {
        startIndex = sv_privateClients->integer;
    }

    newcl = NULL;
    for (i = startIndex; i < sv_maxclients->integer; i++) {
        cl = &svs.clients[i];
        if (cl->state == CS_FREE) {
            newcl = cl;
            break;
        }
    }

    if (!newcl) {
        if (NET_IsLocalAddress(from)) {
            count = 0;
            for (i = startIndex; i < sv_maxclients->integer; i++) {
                cl = &svs.clients[i];
                if (cl->netchan.remoteAddress.type == NA_BOT) {
                    count++;
                }
            }
            if (count >= sv_maxclients->integer - startIndex) {
                SV_DropClient(&svs.clients[sv_maxclients->integer - 1], "only bots on server");
                newcl = &svs.clients[sv_maxclients->integer - 1];
            } else {
                Com_Error(ERR_FATAL, "server is full on local connect");
                return;
            }
        } else {
            SV_FullRedirect(from, challenge);
            NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is full.\n");
            Com_DPrintf("Rejected a connection.\n");
            return;
        }
    }

    cl->reliableAcknowledge = 0;
    cl->reliableSequence = 0;

gotnewcl:
    *newcl = temp;
    clientNum = newcl - svs.clients;
    ent = SV_GentityNum(clientNum);
    newcl->gentity = ent;

    newcl->challenge = challenge;

    Netchan_Setup(NS_SERVER, &newcl->netchan, from, qport, challenge);
    newcl->netchan_end_queue = &newcl->netchan_start_queue;

    {
        uint64_t steamId = (challengeNum >= 0) ? svs.challenges[challengeNum].steamKey : 0;

        if (from.type == NA_LOOPBACK && !Cvar_VariableIntegerValue("com_build")) {
            static byte ticket[0x1000];

            steamId = Steam_ClientSteamID();
            if (sv_serverType->integer > 0) {
                Steam_GS_BeginAuth(steamId, ticket, Steam_AuthTicket(ticket, sizeof(ticket)));
            }
            SV_SteamStatsAddUser(steamId);
        }
        newcl->steam_id = steamId;
        Info_SetValueForKey(userinfo, "steam", va("%llu", (unsigned long long)steamId));
    }

    Q_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));

    denied = (char*)SV_GameClientConnect(clientNum, qtrue, qfalse);
    if (denied) {
        NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", denied);
        Com_DPrintf("Game rejected a connection: %s.\n", denied);
        return;
    }

    SV_UserinfoChanged(newcl);

    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");

    Com_DPrintf("SV_DirectConnect: client %d (%s) CS_FREE -> CS_CONNECTED (svs.time=%d)\n",
                clientNum, newcl->name, svs.time);

    newcl->state = CS_CONNECTED;
    newcl->nextSnapshotTime = 0;
    newcl->lastPacketTime = svs.time;
    newcl->lastConnectTime = svs.time;

    newcl->gamestateMessageNum = -1;

    count = 0;
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            count++;
        }
    }
}

void SV_FreeClient(client_t* client) {
    SV_Netchan_FreeQueue(client);
    SV_CloseDownload(client);
}

void SV_DropClient(client_t* drop, const char* reason) {
    int i;
    challenge_t* challenge;
    const qboolean isBot = drop->netchan.remoteAddress.type == NA_BOT;

    Com_Printf("SV_DropClient: dropping client %d (%s), state=%d, reason='%s'\n",
               (int)(drop - svs.clients), drop->name, drop->state, reason);

    if (drop->state == CS_ZOMBIE) {
        return;
    }

    if (!isBot) {
        if (drop->steam_id) {
            Com_DPrintf("Ending auth session for steam id %llu\n", (unsigned long long)drop->steam_id);
            Steam_GS_EndAuth(drop->steam_id);
            SV_SteamStatsRemoveUser(drop->steam_id);
        }

        challenge = &svs.challenges[0];

        for (i = 0; i < MAX_CHALLENGES; i++, challenge++) {
            if (NET_CompareAdr(drop->netchan.remoteAddress, challenge->adr)) {
                Com_Memset(challenge, 0, sizeof(*challenge));
                break;
            }
        }
    }

    SV_FreeClient(drop);

    drop->reliableSequence = drop->reliableAcknowledge;
    drop->gamestateMessageNum = -1;

    SV_SendServerCommand(NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", drop->name, reason);

    SV_GameClientDisconnect(drop - svs.clients);

    SV_SendServerCommand(drop, "disconnect \"%s\"", reason);

    if (isBot) {
        SV_BotFreeClient(drop - svs.clients);

        drop->state = CS_FREE;
    } else {
        Com_DPrintf("Going to CS_ZOMBIE for %s\n", drop->name);
        drop->state = CS_ZOMBIE;
    }

    SV_SetUserinfo(drop - svs.clients, "");

    for (i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            break;
        }
    }
}

static void SV_SendClientGameState(client_t* client) {
    int start;
    entityState_t *base, nullstate;
    msg_t msg;
    byte msgBuffer[MAX_MSGLEN];

    Com_DPrintf("SV_SendClientGameState: client %d (%s) -> CS_PRIMED\n",
                (int)(client - svs.clients), client->name);
    client->state = CS_PRIMED;
    client->pureAuthentic = 0;
    client->gotCP = qfalse;

    Com_Memset(client->csUpdated, 0, sizeof(client->csUpdated));

    client->gamestateMessageNum = client->netchan.outgoingSequence;

    MSG_Init(&msg, msgBuffer, sizeof(msgBuffer));

    MSG_WriteLong(&msg, client->lastClientCommand);

    SV_UpdateServerCommandsToClient(client, &msg);

    MSG_WriteByte(&msg, svc_gamestate);
    MSG_WriteLong(&msg, client->reliableSequence);

    for (start = 0; start < MAX_CONFIGSTRINGS; start++) {
        if (sv.configstrings[start][0]) {
            MSG_WriteByte(&msg, svc_configstring);
            MSG_WriteShort(&msg, start);
            MSG_WriteBigString(&msg, sv.configstrings[start]);
        }
    }

    Com_Memset(&nullstate, 0, sizeof(nullstate));
    for (start = 0; start < MAX_GENTITIES; start++) {
        base = &sv.svEntities[start].baseline;
        if (!base->number) {
            continue;
        }
        MSG_WriteByte(&msg, svc_baseline);
        MSG_WriteDeltaEntity(&msg, &nullstate, base, qtrue);
    }

    MSG_WriteByte(&msg, svc_EOF);

    MSG_WriteLong(&msg, client - svs.clients);

    MSG_WriteLong(&msg, sv.checksumFeed);

    SV_SendMessageToClient(&msg, client);
}

void SV_ClientEnterWorld(client_t* client, usercmd_t* cmd) {
    int clientNum;
    sharedEntity_t* ent;

    Com_DPrintf("SV_ClientEnterWorld: client %d (%s^7) CS_PRIMED -> CS_ACTIVE\n",
                (int)(client - svs.clients), client->name);
    client->state = CS_ACTIVE;

    SV_UpdateConfigstrings(client);

    clientNum = client - svs.clients;
    ent = SV_GentityNum(clientNum);
    ent->s.number = clientNum;
    client->gentity = ent;

    client->deltaMessage = -1;
    client->nextSnapshotTime = 0;

    if (cmd)
        memcpy(&client->lastUsercmd, cmd, sizeof(client->lastUsercmd));
    else
        memset(&client->lastUsercmd, '\0', sizeof(client->lastUsercmd));

    SV_GameClientBegin(clientNum);
}

static void SV_CloseDownload(client_t* cl) {
    int i;

    if (cl->download) {
        FS_FCloseFile(cl->download);
    }
    cl->download = 0;
    *cl->downloadName = 0;

    for (i = 0; i < MAX_DOWNLOAD_WINDOW; i++) {
        if (cl->downloadBlocks[i]) {
            Z_Free(cl->downloadBlocks[i]);
            cl->downloadBlocks[i] = NULL;
        }
    }
}

static void SV_StopDownload_f(client_t* cl) {
    if (*cl->downloadName)
        Com_DPrintf("clientDownload: %d : file \"%s\" aborted\n", (int)(cl - svs.clients), cl->downloadName);

    SV_CloseDownload(cl);
}

static void SV_DoneDownload_f(client_t* cl) {
    if (cl->state == CS_ACTIVE)
        return;

    Com_DPrintf("clientDownload: %s Done\n", cl->name);
    SV_SendClientGameState(cl);
}

static void SV_NextDownload_f(client_t* cl) {
    int block = atoi(Cmd_Argv(1));

    if (block == cl->downloadClientBlock) {
        Com_DPrintf("clientDownload: %d : client acknowledge of block %d\n", (int)(cl - svs.clients), block);

        if (cl->downloadBlockSize[cl->downloadClientBlock % MAX_DOWNLOAD_WINDOW] == 0) {
            Com_Printf("clientDownload: %d : file \"%s\" completed\n", (int)(cl - svs.clients), cl->downloadName);
            SV_CloseDownload(cl);
            return;
        }

        cl->downloadSendTime = svs.time;
        cl->downloadClientBlock++;
        return;
    }
    SV_DropClient(cl, "broken download");
}

static void SV_BeginDownload_f(client_t* cl) {
    SV_CloseDownload(cl);

    Q_strncpyz(cl->downloadName, Cmd_Argv(1), sizeof(cl->downloadName));
}

int SV_WriteDownloadToClient(client_t* cl, msg_t* msg) {
    int curindex;
    int unreferenced = 1;
    char errorMessage[1024];
    char pakbuf[MAX_QPATH], *pakptr;
    int numRefPaks;

    if (!*cl->downloadName)
        return 0;

    if (!cl->download) {
        Com_sprintf(pakbuf, sizeof(pakbuf), "%s", cl->downloadName);
        pakptr = strrchr(pakbuf, '.');

        if (pakptr) {
            *pakptr = '\0';

            if (!Q_stricmp(pakptr + 1, "pk3")) {
                const char* referencedPaks = FS_ReferencedPakNames();

                Cmd_TokenizeStringIgnoreQuotes(referencedPaks);
                numRefPaks = Cmd_Argc();

                for (curindex = 0; curindex < numRefPaks; curindex++) {
                    if (!FS_FilenameCompare(Cmd_Argv(curindex), pakbuf)) {
                        unreferenced = 0;

                        break;
                    }
                }
            }
        }

        cl->download = 0;

        if (!(sv_allowDownload->integer & DLF_ENABLE) ||
            (sv_allowDownload->integer & DLF_NO_UDP) ||
            unreferenced ||
            (cl->downloadSize = FS_SV_FOpenFileRead(cl->downloadName, &cl->download)) < 0) {
            if (unreferenced) {
                Com_Printf("clientDownload: %d : \"%s\" is not referenced and cannot be downloaded.\n", (int)(cl - svs.clients), cl->downloadName);
                Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" is not referenced and cannot be downloaded.", cl->downloadName);
            } else if (!(sv_allowDownload->integer & DLF_ENABLE) ||
                       (sv_allowDownload->integer & DLF_NO_UDP)) {
                Com_Printf("clientDownload: %d : \"%s\" download disabled\n", (int)(cl - svs.clients), cl->downloadName);
                if (sv_pure->integer) {
                    Com_sprintf(errorMessage, sizeof(errorMessage),
                                "Could not download \"%s\" because autodownloading is disabled on the server.\n\n"
                                "You will need to get this file elsewhere before you "
                                "can connect to this pure server.\n",
                                cl->downloadName);
                } else {
                    Com_sprintf(errorMessage, sizeof(errorMessage),
                                "Could not download \"%s\" because autodownloading is disabled on the server.\n\n"
                                "The server you are connecting to is not a pure server, "
                                "set autodownload to No in your settings and you might be "
                                "able to join the game anyway.\n",
                                cl->downloadName);
                }
            } else {
                Com_Printf("clientDownload: %d : \"%s\" file not found on server\n", (int)(cl - svs.clients), cl->downloadName);
                Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" not found on server for autodownloading.\n", cl->downloadName);
            }
            MSG_WriteByte(msg, svc_download);
            MSG_WriteShort(msg, 0);
            MSG_WriteLong(msg, -1);
            MSG_WriteString(msg, errorMessage);

            *cl->downloadName = 0;

            if (cl->download)
                FS_FCloseFile(cl->download);

            return 1;
        }

        Com_Printf("clientDownload: %d : beginning \"%s\"\n", (int)(cl - svs.clients), cl->downloadName);

        cl->downloadCurrentBlock = cl->downloadClientBlock = cl->downloadXmitBlock = 0;
        cl->downloadCount = 0;
        cl->downloadEOF = qfalse;
    }

    while (cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW &&
           cl->downloadSize != cl->downloadCount) {
        curindex = (cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW);

        if (!cl->downloadBlocks[curindex])
            cl->downloadBlocks[curindex] = Z_Malloc(MAX_DOWNLOAD_BLKSIZE);

        cl->downloadBlockSize[curindex] = FS_Read(cl->downloadBlocks[curindex], MAX_DOWNLOAD_BLKSIZE, cl->download);

        if (cl->downloadBlockSize[curindex] < 0) {
            cl->downloadCount = cl->downloadSize;
            break;
        }

        cl->downloadCount += cl->downloadBlockSize[curindex];

        cl->downloadCurrentBlock++;
    }

    if (cl->downloadCount == cl->downloadSize &&
        !cl->downloadEOF &&
        cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW) {
        cl->downloadBlockSize[cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW] = 0;
        cl->downloadCurrentBlock++;

        cl->downloadEOF = qtrue;
    }

    if (cl->downloadClientBlock == cl->downloadCurrentBlock)
        return 0;

    if (cl->downloadXmitBlock == cl->downloadCurrentBlock) {
        if (svs.time - cl->downloadSendTime > 1000)
            cl->downloadXmitBlock = cl->downloadClientBlock;
        else
            return 0;
    }

    curindex = (cl->downloadXmitBlock % MAX_DOWNLOAD_WINDOW);

    MSG_WriteByte(msg, svc_download);
    MSG_WriteShort(msg, cl->downloadXmitBlock);

    if (cl->downloadXmitBlock == 0)
        MSG_WriteLong(msg, cl->downloadSize);

    MSG_WriteShort(msg, cl->downloadBlockSize[curindex]);

    if (cl->downloadBlockSize[curindex])
        MSG_WriteData(msg, cl->downloadBlocks[curindex], cl->downloadBlockSize[curindex]);

    Com_DPrintf("clientDownload: %d : writing block %d\n", (int)(cl - svs.clients), cl->downloadXmitBlock);

    cl->downloadXmitBlock++;
    cl->downloadSendTime = svs.time;

    return 1;
}

int SV_SendQueuedMessages(void) {
    int i, retval = -1, nextFragT;
    client_t* cl;

    for (i = 0; i < sv_maxclients->integer; i++) {
        cl = &svs.clients[i];

        if (cl->state) {
            nextFragT = SV_RateMsec(cl);

            if (!nextFragT)
                nextFragT = SV_Netchan_TransmitNextFragment(cl);

            if (nextFragT >= 0 && (retval == -1 || retval > nextFragT))
                retval = nextFragT;
        }
    }

    return retval;
}

int SV_SendDownloadMessages(void) {
    int i, numDLs = 0, retval;
    client_t* cl;
    msg_t msg;
    byte msgBuffer[MAX_MSGLEN];

    for (i = 0; i < sv_maxclients->integer; i++) {
        cl = &svs.clients[i];

        if (cl->state && *cl->downloadName) {
            MSG_Init(&msg, msgBuffer, sizeof(msgBuffer));
            MSG_WriteLong(&msg, cl->lastClientCommand);

            retval = SV_WriteDownloadToClient(cl, &msg);

            if (retval) {
                MSG_WriteByte(&msg, svc_EOF);
                SV_Netchan_Transmit(cl, &msg);
                numDLs += retval;
            }
        }
    }

    return numDLs;
}

static void SV_Disconnect_f(client_t* cl) {
    SV_DropClient(cl, "disconnected");
}

static void SV_VerifyPaks_f(client_t* cl) {
    int nBinChkSum, nClientPaks, nServerPaks, nModuleSlots, i, j, nCurArg, workChk;
    int nClientChkSum[1024];
    int nServerChkSum[1024];
    const char *pPaks, *pArg;
    qboolean bGood, engineBin;

    if (!sv_pure->integer) {
        return;
    }

    bGood = qtrue;
    nClientPaks = Cmd_Argc();

    pArg = Cmd_Argv(1);
    if (!*pArg) {
        Com_DPrintf("pure check failed: pArg is NULL\n");
        bGood = qfalse;
    } else if (atoi(pArg) < sv.checksumFeedServerId) {
        Com_DPrintf("ignoring outdated cp command from client %s\n", cl->name);
        return;
    }

    nBinChkSum = 0;
    engineBin = FS_FileIsInEnginePak("qagame" ARCH_STRING DLL_EXT);
    if (!engineBin && FS_FileIsInPAK("qagame" ARCH_STRING DLL_EXT, &nBinChkSum) != 1) {
        Com_DPrintf("pure mode failure: no pak file for binaries\n");
        bGood = qfalse;
    }

    while (bGood) {
        if (nClientPaks < 6) {
            Com_DPrintf("pure check failure: nClientPaks < 6\n");
            bGood = qfalse;
            break;
        }

        nModuleSlots = 1;
        if (*Cmd_Argv(3) != '@' && nClientPaks >= 8 && *Cmd_Argv(5) == '@') {
            nModuleSlots = 3;
        }

        nCurArg = 2;
        for (i = 0; i < nModuleSlots; i++) {
            pArg = Cmd_Argv(nCurArg++);
            if (!*pArg) {
                Com_DPrintf("pure mode failure: bin pArg is NULL\n");
                bGood = qfalse;
                break;
            }
            if (*pArg == '@') {
                Com_DPrintf("pure mode failure: unexpected delimiter\n");
                bGood = qfalse;
                break;
            }
            if (!i && !engineBin && atoi(pArg) != nBinChkSum) {
                Com_DPrintf("pure mode failure: bin %i != %i\n", atoi(pArg), nBinChkSum);
                bGood = qfalse;
                break;
            }
        }
        if (!bGood) {
            break;
        }

        pArg = Cmd_Argv(nCurArg++);
        if (*pArg != '@') {
            Com_DPrintf("pure mode failure: didn't find delimiter\n");
            bGood = qfalse;
            break;
        }

        for (i = 0; nCurArg < nClientPaks && i < 1024; i++) {
            nClientChkSum[i] = atoi(Cmd_Argv(nCurArg++));
        }

        nClientPaks = i - 1;

        for (i = 0; i < nClientPaks && bGood; i++) {
            for (j = 0; j < nClientPaks; j++) {
                if (i != j && nClientChkSum[i] == nClientChkSum[j]) {
                    Com_DPrintf("pure mode failure: got duplicate checksum %i\n", nClientChkSum[i]);
                    bGood = qfalse;
                    break;
                }
            }
        }
        if (!bGood) {
            break;
        }

        pPaks = FS_LoadedPakPureChecksums();
        Cmd_TokenizeString(pPaks);
        nServerPaks = Cmd_Argc();
        if (nServerPaks > 1024) {
            nServerPaks = 1024;
        }
        for (i = 0; i < nServerPaks; i++) {
            nServerChkSum[i] = atoi(Cmd_Argv(i));
        }

        for (i = 0; i < nClientPaks; i++) {
            for (j = 0; j < nServerPaks; j++) {
                if (nClientChkSum[i] == nServerChkSum[j]) {
                    break;
                }
            }
            if (j >= nServerPaks) {
                bGood = qfalse;
                break;
            }
        }
        if (!bGood) {
            Com_DPrintf("pure mode failure: client checksums:");
            for (i = 0; i < nClientPaks; i++) {
                Com_DPrintf("%i ", nClientChkSum[i]);
            }
            Com_DPrintf("\n");
            Com_DPrintf("pure mode failure: server checksums:");
            for (i = 0; i < nServerPaks; i++) {
                Com_DPrintf("%i ", nServerChkSum[i]);
            }
            Com_DPrintf("\n");
            Com_DPrintf("pure mode failure: client has extra pk3s\n");
            break;
        }

        workChk = sv.checksumFeed;
        for (i = 0; i < nClientPaks; i++) {
            workChk ^= nClientChkSum[i];
        }
        workChk ^= nClientPaks;
        if (workChk != nClientChkSum[nClientPaks]) {
            Com_DPrintf("pure mode failure: workChk %i != nClientChkSum[nClientPaks] %i\n", workChk,
                        nClientChkSum[nClientPaks]);
            bGood = qfalse;
            break;
        }

        break;
    }

    cl->gotCP = qtrue;

    if (bGood) {
        cl->pureAuthentic = 1;
    } else {
        cl->pureAuthentic = 0;
        cl->nextSnapshotTime = -1;
        cl->state = CS_ACTIVE;
        SV_SendClientSnapshot(cl);
        SV_DropClient(cl, "Unpure client detected. Invalid .PK3 files referenced!");
    }
}

static void SV_ResetPureClient_f(client_t* cl) {
    cl->pureAuthentic = 0;
    cl->gotCP = qfalse;
}

void SV_UserinfoChanged(client_t* cl) {
    char* val;
    char* ip;
    int i;
    int len;

    Q_strncpyz(cl->name, Info_ValueForKey(cl->userinfo, "name"), sizeof(cl->name));

    val = Info_ValueForKey(cl->userinfo, "rate");
    if (strlen(val)) {
        i = atoi(val);
        if (i < 4000)
            i = 4000;
        else if (i > 50000)
            i = 50000;
        cl->rate = i;
    } else {
        cl->rate = 8000;
    }
    if (Sys_IsLANAddress(cl->netchan.remoteAddress) && sv_lanForceRate->integer == 1) {
        cl->rate = 99999;
    }
    i = 1000 / sv_fps->integer;
    if (i != cl->snapshotMsec) {
        cl->nextSnapshotTime = 0;
        cl->snapshotMsec = i;
    }

    if (NET_IsLocalAddress(cl->netchan.remoteAddress))
        ip = "localhost";
    else
        ip = (char*)NET_AdrToString(cl->netchan.remoteAddress);

    val = Info_ValueForKey(cl->userinfo, "ip");
    if (val[0])
        len = strlen(ip) - strlen(val) + strlen(cl->userinfo);
    else
        len = strlen(ip) + 4 + strlen(cl->userinfo);

    if (len >= MAX_INFO_STRING)
        SV_DropClient(cl, "userinfo string length exceeded");
    else
        Info_SetValueForKey(cl->userinfo, "ip", ip);
}

static void SV_UpdateUserinfo_f(client_t* cl) {
    Q_strncpyz(cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo));

    SV_UserinfoChanged(cl);
    SV_GameClientUserinfoChanged(cl - svs.clients);
}

typedef struct {
    char* name;
    void (*func)(client_t* cl);
} ucmd_t;

static void SV_Rcon_f(client_t* cl) {
    char text[1024];

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }
    if (Cmd_Argc() < 2 || !SV_GameClientIsAdmin(cl - svs.clients)) {
        return;
    }
    Q_strncpyz(text, Cmd_Cmd() + 5, sizeof(text));
    SV_RedirectedCommand(cl->netchan.remoteAddress, text);
}

static ucmd_t ucmds[] = {
    {"userinfo", SV_UpdateUserinfo_f},
    {"disconnect", SV_Disconnect_f},
    {"cp", SV_VerifyPaks_f},
    {"vdr", SV_ResetPureClient_f},
    {"download", SV_BeginDownload_f},
    {"nextdl", SV_NextDownload_f},
    {"stopdl", SV_StopDownload_f},
    {"donedl", SV_DoneDownload_f},
    {"rcon", SV_Rcon_f},

    {NULL, NULL}};

void SV_ExecuteClientCommand(client_t* cl, const char* s, qboolean clientOK) {
    ucmd_t* u;
    qboolean bProcessed = qfalse;

    Cmd_TokenizeString(s);

    for (u = ucmds; u->name; u++) {
        if (!strcmp(Cmd_Argv(0), u->name)) {
            u->func(cl);
            bProcessed = qtrue;
            break;
        }
    }

    if (clientOK) {
        if (!u->name && sv.state == SS_GAME && (cl->state == CS_ACTIVE || cl->state == CS_PRIMED)) {
            Cmd_Args_Sanitize();
            SV_GameClientCommand(cl - svs.clients);
        }
    } else if (!bProcessed)
        Com_DPrintf("client text ignored for %s: %s\n", cl->name, Cmd_Argv(0));
}

static qboolean SV_ClientCommand(client_t* cl, msg_t* msg) {
    int seq;
    const char* s;
    qboolean clientOk = qtrue;

    seq = MSG_ReadLong(msg);
    s = MSG_ReadString(msg);

    if (cl->lastClientCommand >= seq) {
        return qtrue;
    }

    Com_DPrintf("clientCommand: %s : %i : %s\n", cl->name, seq, s);

    if (seq > cl->lastClientCommand + 1) {
        Com_Printf("Client %s lost %i clientCommands\n", cl->name,
                   seq - cl->lastClientCommand + 1);
        SV_DropClient(cl, "Lost reliable commands");
        return qfalse;
    }

    if (!com_cl_running->integer && cl->state >= CS_ACTIVE && sv_floodProtect->value > 0.0f) {
        float score = cl->floodScore + 1.0f - (svs.time - cl->floodTime) / 1000.0f;

        if (score < 0.0f) {
            score = 0.0f;
        }
        cl->floodScore = score;
        cl->floodTime = svs.time;

        if (score > sv_floodProtect->value) {
            SV_DropClient(cl, "Flooding the server");
            return qfalse;
        }
    }

    SV_ExecuteClientCommand(cl, s, clientOk);

    cl->lastClientCommand = seq;
    Com_sprintf(cl->lastClientCommandString, sizeof(cl->lastClientCommandString), "%s", s);

    return qtrue;
}

void SV_ClientThink(client_t* cl, usercmd_t* cmd) {
    cl->lastUsercmd = *cmd;

    if (cl->state != CS_ACTIVE) {
        return;
    }

    SV_GameClientThink(cl - svs.clients);
}

static void SV_UserMove(client_t* cl, msg_t* msg, qboolean delta) {
    int i, key;
    int cmdCount;
    usercmd_t nullcmd;
    usercmd_t cmds[MAX_PACKET_USERCMDS];
    usercmd_t *cmd, *oldcmd;

    if (delta) {
        cl->deltaMessage = cl->messageAcknowledge;
    } else {
        cl->deltaMessage = -1;
    }

    cmdCount = MSG_ReadByte(msg);

    if (cmdCount < 1) {
        Com_Printf("cmdCount < 1\n");
        return;
    }

    if (cmdCount > MAX_PACKET_USERCMDS) {
        Com_Printf("cmdCount > MAX_PACKET_USERCMDS\n");
        return;
    }

    key = sv.checksumFeed;
    key ^= cl->messageAcknowledge;
    key ^= MSG_HashKey(cl->reliableCommands[cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1)], 32);

    Com_Memset(&nullcmd, 0, sizeof(nullcmd));
    oldcmd = &nullcmd;
    for (i = 0; i < cmdCount; i++) {
        cmd = &cmds[i];
        MSG_ReadDeltaUsercmdKey(msg, key, oldcmd, cmd);
        oldcmd = cmd;
    }

    if (cl->frames[cl->messageAcknowledge & PACKET_MASK].messageAcked <= 0) {
        cl->frames[cl->messageAcknowledge & PACKET_MASK].messageAcked = Sys_Milliseconds();
    }

    if (sv_pure->integer != 0 && cl->pureAuthentic == 0 && !cl->gotCP) {
        if (cl->state == CS_ACTIVE) {
            Com_DPrintf("%s: didn't get cp command, resending gamestate\n", cl->name);
            SV_SendClientGameState(cl);
        }
        return;
    }

    if (cl->state == CS_PRIMED) {
        SV_ClientEnterWorld(cl, &cmds[0]);
    }

    if (sv_pure->integer != 0 && cl->pureAuthentic == 0) {
        SV_DropClient(cl, "Cannot validate pure client!");
        return;
    }

    if (cl->state != CS_ACTIVE) {
        cl->deltaMessage = -1;
        return;
    }

    for (i = 0; i < cmdCount; i++) {
        if (cmds[i].serverTime > cmds[cmdCount - 1].serverTime) {
            continue;
        }
        if (cmds[i].serverTime <= cl->lastUsercmd.serverTime) {
            continue;
        }
        SV_ClientThink(cl, &cmds[i]);
    }
}

void SV_ExecuteClientMessage(client_t* cl, msg_t* msg) {
    int c;
    int serverId;

    MSG_Bitstream(msg);

    serverId = MSG_ReadLong(msg);
    cl->messageAcknowledge = MSG_ReadLong(msg);

    if (cl->messageAcknowledge < 0) {
#ifndef NDEBUG
        SV_DropClient(cl, "DEBUG: illegible client message");
#endif
        return;
    }

    cl->reliableAcknowledge = MSG_ReadLong(msg);

    MSG_ReadByte(msg);

    if ((cl->reliableSequence - cl->reliableAcknowledge >= MAX_RELIABLE_COMMANDS) || (cl->reliableSequence - cl->reliableAcknowledge < 0)) {
#ifndef NDEBUG
        SV_DropClient(cl, "DEBUG: illegible client message");
#endif
        cl->reliableAcknowledge = cl->reliableSequence;
        return;
    }
    if (serverId != sv.serverId && !*cl->downloadName && !strstr(cl->lastClientCommandString, "nextdl")) {
        if (serverId >= sv.restartedServerId && serverId < sv.serverId) {
            Com_DPrintf("%s : ignoring pre map_restart / outdated client message\n", cl->name);
            return;
        }
        if (cl->state != CS_ACTIVE && cl->messageAcknowledge > cl->gamestateMessageNum) {
            Com_DPrintf("%s : dropped gamestate, resending\n", cl->name);
            SV_SendClientGameState(cl);
        }
        return;
    }

    if (cl->oldServerTime && serverId == sv.serverId) {
        Com_DPrintf("%s acknowledged gamestate\n", cl->name);
        cl->oldServerTime = 0;
    }

    do {
        c = MSG_ReadByte(msg);

        if (c == clc_EOF) {
            break;
        }

        if (c != clc_clientCommand) {
            break;
        }
        if (!SV_ClientCommand(cl, msg)) {
            return;
        }
        if (cl->state == CS_ZOMBIE) {
            return;
        }
    } while (1);

    if (c == clc_move) {
        SV_UserMove(cl, msg, qtrue);
    } else if (c == clc_moveNoDelta) {
        SV_UserMove(cl, msg, qfalse);
    } else if (c == clc_EOF) {
    } else {
        Com_Printf("WARNING: bad command byte %d for client %i\n", c, (int)(cl - svs.clients));
    }
}
