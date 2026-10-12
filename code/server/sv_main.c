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

serverStatic_t svs;
server_t sv;
vm_t* gvm = NULL;

cvar_t* sv_fps = NULL;
cvar_t* sv_timeout;
cvar_t* sv_zombietime;
cvar_t* sv_rconPassword;
cvar_t* sv_privatePassword;
cvar_t* sv_allowDownload;
cvar_t* sv_maxclients;

cvar_t* sv_privateClients;
cvar_t* sv_hostname;
cvar_t* sv_reconnectlimit;
cvar_t* sv_showloss;
cvar_t* sv_padPackets;
cvar_t* sv_killserver;
cvar_t* sv_mapname;
cvar_t* sv_mapChecksum;
cvar_t* sv_serverid;
cvar_t* sv_minRate;
cvar_t* sv_maxRate;
cvar_t* sv_dlRate;
cvar_t* sv_minPing;
cvar_t* sv_maxPing;
cvar_t* sv_serverType;
cvar_t* sv_gametype;
cvar_t* sv_ammoPack;
cvar_t* sv_pure;
cvar_t* sv_floodProtect;
cvar_t* sv_master;
cvar_t* sv_idleExit;
cvar_t* sv_errorExit;
cvar_t* sv_workshopFile;
cvar_t* sv_quitOnEmpty;
cvar_t* sv_fullRedirect;
cvar_t* sv_lanForceRate;
cvar_t* sv_banFile;
cvar_t* sv_altEntDir;
cvar_t* sv_dumpEntities;

serverBan_t serverBans[SERVER_MAXBANS];
int serverBansCount = 0;

static char* SV_ExpandNewlines(char* in) {
    static char string[1024];
    int l;

    l = 0;
    while (*in && l < sizeof(string) - 3) {
        if (*in == '\n') {
            string[l++] = '\\';
            string[l++] = 'n';
        } else {
            string[l++] = *in;
        }
        in++;
    }
    string[l] = 0;

    return string;
}

void SV_AddServerCommand(client_t* client, const char* cmd) {
    int index, i;

    if (client->state < CS_PRIMED)
        return;

    client->reliableSequence++;
    if (client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1) {
        if (client->gamestateMessageNum == -1) {
            return;
        }

        Com_Printf("===== pending server commands =====\n");
        for (i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++) {
            Com_Printf("cmd %5d: %s\n", i, client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)]);
        }
        Com_Printf("cmd %5d: %s\n", i, cmd);
        SV_DropClient(client, "Server command overflow");
        return;
    }
    index = client->reliableSequence & (MAX_RELIABLE_COMMANDS - 1);
    Q_strncpyz(client->reliableCommands[index], cmd, sizeof(client->reliableCommands[index]));
}

Q_HOOKABLE void QDECL SV_SendServerCommand(client_t* cl, const char* fmt, ...) {
    va_list argptr;
    byte message[MAX_MSGLEN];
    client_t* client;
    int j;

    va_start(argptr, fmt);
    Q_vsnprintf((char*)message, sizeof(message), fmt, argptr);
    va_end(argptr);

    if (strlen((char*)message) > 1022) {
        return;
    }

    if (cl != NULL) {
        SV_AddServerCommand(cl, (char*)message);
        return;
    }

    if (com_dedicated->integer && !strncmp((char*)message, "print", 5)) {
        Com_Printf("broadcast: %s\n", SV_ExpandNewlines((char*)message));
    }

    for (j = 0, client = svs.clients; j < sv_maxclients->integer; j++, client++) {
        SV_AddServerCommand(client, (char*)message);
    }
}

#define MAX_BUCKETS 16384
#define MAX_HASHES 1024

static leakyBucket_t buckets[MAX_BUCKETS];
static leakyBucket_t* bucketHashes[MAX_HASHES];
leakyBucket_t outboundLeakyBucket;

static long SVC_HashForAddress(netadr_t address) {
    byte* ip = NULL;
    size_t size = 0;
    int i;
    long hash = 0;

    switch (address.type) {
        case NA_IP:
            ip = address.ip;
            size = 4;
            break;
        case NA_IP6:
            ip = address.ip6;
            size = 16;
            break;
        default:
            break;
    }

    for (i = 0; i < size; i++) {
        hash += (long)(ip[i]) * (i + 119);
    }

    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    hash &= (MAX_HASHES - 1);

    return hash;
}

static leakyBucket_t* SVC_BucketForAddress(netadr_t address, int burst, int period) {
    leakyBucket_t* bucket = NULL;
    int i;
    long hash = SVC_HashForAddress(address);
    int now = Sys_Milliseconds();

    for (bucket = bucketHashes[hash]; bucket; bucket = bucket->next) {
        switch (bucket->type) {
            case NA_IP:
                if (memcmp(bucket->ipv._4, address.ip, 4) == 0) {
                    return bucket;
                }
                break;

            case NA_IP6:
                if (memcmp(bucket->ipv._6, address.ip6, 16) == 0) {
                    return bucket;
                }
                break;

            default:
                break;
        }
    }

    for (i = 0; i < MAX_BUCKETS; i++) {
        int interval;

        bucket = &buckets[i];
        interval = now - bucket->lastTime;

        if (bucket->lastTime > 0 && (interval > (burst * period) ||
                                     interval < 0)) {
            if (bucket->prev != NULL) {
                bucket->prev->next = bucket->next;
            } else {
                bucketHashes[bucket->hash] = bucket->next;
            }

            if (bucket->next != NULL) {
                bucket->next->prev = bucket->prev;
            }

            Com_Memset(bucket, 0, sizeof(leakyBucket_t));
        }

        if (bucket->type == NA_BAD) {
            bucket->type = address.type;
            switch (address.type) {
                case NA_IP:
                    Com_Memcpy(bucket->ipv._4, address.ip, 4);
                    break;
                case NA_IP6:
                    Com_Memcpy(bucket->ipv._6, address.ip6, 16);
                    break;
                default:
                    break;
            }

            bucket->lastTime = now;
            bucket->burst = 0;
            bucket->hash = hash;

            bucket->next = bucketHashes[hash];
            if (bucketHashes[hash] != NULL) {
                bucketHashes[hash]->prev = bucket;
            }

            bucket->prev = NULL;
            bucketHashes[hash] = bucket;

            return bucket;
        }
    }

    return NULL;
}

qboolean SVC_RateLimit(leakyBucket_t* bucket, int burst, int period) {
    if (bucket != NULL) {
        int now = Sys_Milliseconds();
        int interval = now - bucket->lastTime;
        int expired = interval / period;
        int expiredRemainder = interval % period;

        if (expired > bucket->burst || interval < 0) {
            bucket->burst = 0;
            bucket->lastTime = now;
        } else {
            bucket->burst -= expired;
            bucket->lastTime = now - expiredRemainder;
        }

        if (bucket->burst < burst) {
            bucket->burst++;

            return qfalse;
        }
    }

    return qtrue;
}

qboolean SVC_RateLimitAddress(netadr_t from, int burst, int period) {
    leakyBucket_t* bucket = SVC_BucketForAddress(from, burst, period);

    return SVC_RateLimit(bucket, burst, period);
}

static void SVC_Status(netadr_t from) {
    char player[1024];
    char status[MAX_MSGLEN];
    int i;
    client_t* cl;
    playerState_t* ps;
    int statusLength;
    int playerLength;
    char infostring[MAX_INFO_STRING];

    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SVC_Status: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    if (SVC_RateLimit(&outboundLeakyBucket, 10, 100)) {
        Com_DPrintf("SVC_Status: rate limit exceeded, dropping request\n");
        return;
    }

    if (strlen(Cmd_Argv(1)) > 128)
        return;

    strcpy(infostring, Cvar_InfoString(CVAR_SERVERINFO));

    Info_SetValueForKey(infostring, "challenge", Cmd_Argv(1));

    status[0] = 0;
    statusLength = 0;

    for (i = 0; i < sv_maxclients->integer; i++) {
        cl = &svs.clients[i];
        if (cl->state >= CS_CONNECTED) {
            ps = SV_GameClientNum(i);
            Com_sprintf(player, sizeof(player), "%i %i \"%s\"\n",
                        ps->persistant[PERS_SCORE], cl->ping, cl->name);
            playerLength = strlen(player);
            if (statusLength + playerLength >= sizeof(status)) {
                break;
            }
            strcpy(status + statusLength, player);
            statusLength += playerLength;
        }
    }

    NET_OutOfBandPrint(NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status);
}

void SVC_Info(netadr_t from) {
    int i, count, humans;
    char* gamedir;
    char infostring[MAX_INFO_STRING];

    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SVC_Info: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    if (SVC_RateLimit(&outboundLeakyBucket, 10, 100)) {
        Com_DPrintf("SVC_Info: rate limit exceeded, dropping request\n");
        return;
    }

    if (strlen(Cmd_Argv(1)) > 128)
        return;

    count = humans = 0;
    for (i = sv_privateClients->integer; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            count++;
            if (svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
                humans++;
            }
        }
    }

    infostring[0] = 0;

    Info_SetValueForKey(infostring, "challenge", Cmd_Argv(1));

    Info_SetValueForKey(infostring, "gamename", com_gamename->string);

    Info_SetValueForKey(infostring, "protocol", va("%i", com_protocol->integer));

    Info_SetValueForKey(infostring, "hostname", sv_hostname->string);
    Info_SetValueForKey(infostring, "mapname", sv_mapname->string);
    Info_SetValueForKey(infostring, "clients", va("%i", count));
    Info_SetValueForKey(infostring, "g_humanplayers", va("%i", humans));
    Info_SetValueForKey(infostring, "sv_maxclients",
                        va("%i", sv_maxclients->integer - sv_privateClients->integer));
    Info_SetValueForKey(infostring, "gametype", va("%i", sv_gametype->integer));
    Info_SetValueForKey(infostring, "pure", va("%i", sv_pure->integer));
    Info_SetValueForKey(infostring, "g_needpass", va("%d", Cvar_VariableIntegerValue("g_needpass")));

    if (sv_minPing->integer) {
        Info_SetValueForKey(infostring, "minPing", va("%i", sv_minPing->integer));
    }
    if (sv_maxPing->integer) {
        Info_SetValueForKey(infostring, "maxPing", va("%i", sv_maxPing->integer));
    }
    gamedir = Cvar_VariableString("fs_game");
    if (*gamedir) {
        Info_SetValueForKey(infostring, "game", gamedir);
    }

    NET_OutOfBandPrint(NS_SERVER, from, "infoResponse\n%s", infostring);
}

static void SV_FlushRedirect(char* outputbuf) {
    NET_OutOfBandPrint(NS_SERVER, svs.redirectAddress, "print\n%s", outputbuf);
}

static void SVC_RemoteCommand(netadr_t from, msg_t* msg) {
    qboolean valid;
    char remaining[1024];
#define SV_OUTPUTBUF_LENGTH (1024 - 16)
    char sv_outputbuf[SV_OUTPUTBUF_LENGTH];
    char* cmd_aux;

    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SVC_RemoteCommand: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    if (!strlen(sv_rconPassword->string) ||
        strcmp(Cmd_Argv(1), sv_rconPassword->string)) {
        static leakyBucket_t bucket;

        if (SVC_RateLimit(&bucket, 10, 1000)) {
            Com_DPrintf("SVC_RemoteCommand: rate limit exceeded, dropping request\n");
            return;
        }

        valid = qfalse;
        Com_Printf("Bad rcon from %s: %s\n", NET_AdrToString(from), Cmd_ArgsFrom(2));
    } else {
        valid = qtrue;
        Com_Printf("Rcon from %s: %s\n", NET_AdrToString(from), Cmd_ArgsFrom(2));
    }

    svs.redirectAddress = from;
    Com_BeginRedirect(sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

    if (!strlen(sv_rconPassword->string)) {
        Com_Printf("No rconpassword set on the server.\n");
    } else if (!valid) {
        Com_Printf("Bad rconpassword.\n");
    } else {
        remaining[0] = 0;

        cmd_aux = Cmd_Cmd();
        cmd_aux += 4;
        while (cmd_aux[0] == ' ')
            cmd_aux++;
        while (cmd_aux[0] && cmd_aux[0] != ' ')
            cmd_aux++;
        while (cmd_aux[0] == ' ')
            cmd_aux++;

        Q_strcat(remaining, sizeof(remaining), cmd_aux);

        Cmd_ExecuteString(remaining);
    }

    Com_EndRedirect();
}

void SV_RedirectedCommand(netadr_t to, const char* text) {
    char outputbuf[SV_OUTPUTBUF_LENGTH];

    svs.redirectAddress = to;
    Com_BeginRedirect(outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
    Cmd_ExecuteString(text);
    Com_EndRedirect();
}

#define SV_MAX_REDIRECTS 8
#define SV_REDIRECT_ASK 5000
#define SV_REDIRECT_FRESH 15000

typedef struct {
    netadr_t adr;
    int answered;
    int clients;
    int maxClients;
    qboolean needPass;
} svRedirect_t;

static svRedirect_t svRedirects[SV_MAX_REDIRECTS];
static int svNumRedirects;
static int svRedirectAsked;

static void SV_ParseRedirects(void) {
    char list[MAX_CVAR_VALUE_STRING];
    char* p;

    svNumRedirects = 0;
    Q_strncpyz(list, sv_fullRedirect->string, sizeof(list));
    for (p = list; *p; p++) {
        if (*p == ',' || *p == ';' || *p == '\t') {
            *p = ' ';
        }
    }
    p = list;
    while (*p && svNumRedirects < SV_MAX_REDIRECTS) {
        svRedirect_t* r = &svRedirects[svNumRedirects];
        char* address;

        while (*p == ' ') {
            p++;
        }
        address = p;
        while (*p && *p != ' ') {
            p++;
        }
        if (*p) {
            *p++ = '\0';
        }
        if (!address[0]) {
            break;
        }
        Com_Memset(r, 0, sizeof(*r));
        if (!NET_StringToAdr(address, &r->adr, NA_UNSPEC) || r->adr.type == NA_LOOPBACK) {
            Com_Printf("sv_fullRedirect: bad address %s\n", address);
            continue;
        }
        if (!r->adr.port) {
            r->adr.port = BigShort(PORT_SERVER);
        }
        svNumRedirects++;
    }
    svRedirectAsked = 0;
}

static void SV_RedirectAskSteam(netadr_t adr, const byte* challenge) {
    static const char query[] = "\xff\xff\xff\xffTSource Engine Query";
    byte packet[sizeof(query) + 4];

    Com_Memcpy(packet, query, sizeof(query));
    if (challenge) {
        Com_Memcpy(packet + sizeof(query), challenge, 4);
    }
    NET_SendPacket(NS_SERVER, sizeof(query) + (challenge ? 4 : 0), packet, adr);
}

static void SV_RedirectFrame(void) {
    int now = Sys_Milliseconds();
    int i;

    if (sv_fullRedirect->modified) {
        sv_fullRedirect->modified = qfalse;
        SV_ParseRedirects();
    }
    if (!svNumRedirects || (svRedirectAsked && now - svRedirectAsked < SV_REDIRECT_ASK)) {
        return;
    }
    svRedirectAsked = now;
    for (i = 0; i < svNumRedirects; i++) {
        NET_OutOfBandPrint(NS_SERVER, svRedirects[i].adr, "getinfo redirect");
        SV_RedirectAskSteam(svRedirects[i].adr, NULL);
    }
}

static const byte* SV_SkipSteamString(const byte* p, const byte* end) {
    while (p < end && *p) {
        p++;
    }
    return (p < end) ? p + 1 : NULL;
}

static qboolean SV_RedirectSteamAnswer(netadr_t from, msg_t* msg) {
    const byte* p = msg->data + 5;
    const byte* end = msg->data + msg->cursize;
    svRedirect_t* r = NULL;
    int i;

    if (msg->cursize < 5 || *(int*)msg->data != -1 || (msg->data[4] != 'A' && msg->data[4] != 'I')) {
        return qfalse;
    }
    for (i = 0; i < svNumRedirects; i++) {
        if (NET_CompareAdr(from, svRedirects[i].adr)) {
            r = &svRedirects[i];
            break;
        }
    }
    if (!r) {
        return qfalse;
    }
    if (msg->data[4] == 'A') {
        if (msg->cursize >= 9) {
            SV_RedirectAskSteam(from, msg->data + 5);
        }
        return qtrue;
    }
    p++;
    for (i = 0; i < 4 && p; i++) {
        p = SV_SkipSteamString(p, end);
    }
    if (!p || end - p < 9) {
        return qtrue;
    }
    p += 2;
    r->clients = p[0];
    r->maxClients = p[1];
    r->needPass = p[5] ? qtrue : qfalse;
    r->answered = Sys_Milliseconds();
    return qtrue;
}

static void SV_RedirectInfoResponse(netadr_t from, msg_t* msg) {
    const char* info;
    int i;

    for (i = 0; i < svNumRedirects && !NET_CompareAdr(from, svRedirects[i].adr); i++) {
    }
    if (i == svNumRedirects) {
        return;
    }
    info = MSG_ReadString(msg);
    svRedirects[i].clients = atoi(Info_ValueForKey(info, "clients"));
    svRedirects[i].maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
    svRedirects[i].needPass = atoi(Info_ValueForKey(info, "g_needpass")) ? qtrue : qfalse;
    svRedirects[i].answered = Sys_Milliseconds();
}

void SV_FullRedirect(netadr_t from, int challenge) {
    svRedirect_t* best = NULL;
    int now = Sys_Milliseconds();
    int i;

    for (i = 0; i < svNumRedirects; i++) {
        svRedirect_t* r = &svRedirects[i];

        if (!r->answered || now - r->answered > SV_REDIRECT_FRESH || r->needPass || r->clients >= r->maxClients) {
            continue;
        }
        if (!best || r->clients > best->clients) {
            best = r;
        }
    }
    if (!best) {
        return;
    }
    if (best->adr.type == NA_IP && best->adr.ip[0] == 127) {
        NET_OutOfBandPrint(NS_SERVER, from, "redirect %i :%i", challenge, BigShort(best->adr.port));
    } else {
        NET_OutOfBandPrint(NS_SERVER, from, "redirect %i %s", challenge, NET_AdrToStringwPort(best->adr));
    }
    Com_Printf("Server is full: %s sent to %s (%i of %i)\n", NET_AdrToString(from),
               NET_AdrToStringwPort(best->adr), best->clients, best->maxClients);
}

static void SV_ConnectionlessPacket(netadr_t from, msg_t* msg) {
    char* s;
    char* c;

    if (svNumRedirects && SV_RedirectSteamAnswer(from, msg)) {
        return;
    }

    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg);

    if (!Q_strncmp("connect", (char*)&msg->data[4], 7)) {
        Huff_Decompress(msg, 12);
    }

    s = MSG_ReadStringLine(msg);
    Cmd_TokenizeString(s);

    c = Cmd_Argv(0);
    Com_DPrintf("SV packet %s : %s\n", NET_AdrToString(from), c);

    if (!Q_stricmp(c, "getstatus")) {
        SVC_Status(from);
    } else if (!Q_stricmp(c, "getinfo")) {
        SVC_Info(from);
    } else if (!Q_stricmp(c, "getchallenge")) {
        SV_GetChallenge(from, msg);
    } else if (!Q_stricmp(c, "connect")) {
        SV_DirectConnect(from);
    } else if (!Q_stricmp(c, "rcon")) {
        SVC_RemoteCommand(from, msg);
    } else if (!Q_stricmp(c, "disconnect")) {
    } else if (!Q_stricmp(c, "infoResponse") && svNumRedirects) {
        SV_RedirectInfoResponse(from, msg);
    } else if (Steam_GS_HandlePacket(from, msg->data, msg->cursize)) {
    } else {
        Com_DPrintf("bad connectionless packet from %s:\n%s\n",
                    NET_AdrToString(from), s);
    }
}

void SV_PacketEvent(netadr_t from, msg_t* msg) {
    int i;
    client_t* cl;
    int qport;

    if (msg->cursize >= 4 && *(int*)msg->data == -1) {
        SV_ConnectionlessPacket(from, msg);
        return;
    }

    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg);
    qport = MSG_ReadShort(msg) & 0xffff;

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state == CS_FREE) {
            continue;
        }
        if (!NET_CompareBaseAdr(from, cl->netchan.remoteAddress)) {
            continue;
        }
        if (cl->netchan.qport != qport) {
            continue;
        }

        if (cl->netchan.remoteAddress.port != from.port) {
            Com_Printf("SV_PacketEvent: fixing up a translated port\n");
            cl->netchan.remoteAddress.port = from.port;
        }

        if (SV_Netchan_Process(cl, msg)) {
            if (cl->state != CS_ZOMBIE) {
                cl->lastPacketTime = svs.time;
                SV_ExecuteClientMessage(cl, msg);
            }
        }
        return;
    }
}

static void SV_CalcPings(void) {
    int i, j;
    client_t* cl;
    int total, count;
    int delta;
    playerState_t* ps;

    for (i = 0; i < sv_maxclients->integer; i++) {
        cl = &svs.clients[i];
        if (cl->state != CS_ACTIVE) {
            cl->ping = 999;
            continue;
        }
        if (!cl->gentity) {
            cl->ping = 999;
            continue;
        }
        if (cl->gentity->r.svFlags & SVF_BOT) {
            cl->ping = 0;
            continue;
        }

        total = 0;
        count = 0;
        for (j = 0; j < PACKET_BACKUP; j++) {
            if (cl->frames[j].messageAcked <= 0) {
                continue;
            }
            delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
            count++;
            total += delta;
        }
        if (!count) {
            cl->ping = 999;
        } else {
            cl->ping = total / count;
            if (cl->ping > 999) {
                cl->ping = 999;
            }
        }

        ps = SV_GameClientNum(i);
        ps->ping = cl->ping;
    }
}

static void SV_CheckTimeouts(void) {
    int i;
    client_t* cl;
    int droppoint;
    int zombiepoint;

    droppoint = svs.time - 1000 * sv_timeout->integer;
    zombiepoint = svs.time - 1000 * sv_zombietime->integer;

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->lastPacketTime > svs.time) {
            cl->lastPacketTime = svs.time;
        }

        if (cl->state == CS_ZOMBIE && cl->lastPacketTime < zombiepoint) {
            Com_DPrintf("Going from CS_ZOMBIE to CS_FREE for client %d\n", i);
            cl->state = CS_FREE;
            continue;
        }
        if (cl->state >= CS_CONNECTED && cl->lastPacketTime < droppoint) {
            if (++cl->timeoutCount > 5) {
                SV_DropClient(cl, "timed out");
                cl->state = CS_FREE;
            }
        } else {
            cl->timeoutCount = 0;
        }
    }
}

static qboolean SV_CheckPaused(void) {
    int count;
    client_t* cl;
    int i;

    if (!cl_paused->integer) {
        return qfalse;
    }

    count = 0;
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state >= CS_CONNECTED && cl->netchan.remoteAddress.type != NA_BOT) {
            count++;
        }
    }

    if (count > 1) {
        if (sv_paused->integer)
            Cvar_Set("sv_paused", "0");
        return qfalse;
    }

    if (!sv_paused->integer)
        Cvar_Set("sv_paused", "1");
    return qtrue;
}

void SV_VoiceFrame(void) {
    static int lastHello;
    byte packet[1 + 16384];
    uint64_t from;
    int bytes;
    int i;

    if (!Steam_GS_Running() || sv.state == SS_DEAD || !svs.clients) {
        return;
    }

    if (svs.time - lastHello > 10000 || svs.time < lastHello) {
        lastHello = svs.time;
        for (i = 0; i < sv_maxclients->integer; i++) {
            if (svs.clients[i].state == CS_ACTIVE) {
                Steam_P2PHello(svs.clients[i].steam_id, qtrue);
            }
        }
    }

    while ((bytes = Steam_P2PReceive(packet + 1, sizeof(packet) - 1, &from, qtrue)) > 0) {
        int speaker = -1;

        for (i = 0; i < sv_maxclients->integer; i++) {
            if (svs.clients[i].state == CS_ACTIVE && svs.clients[i].steam_id == from) {
                speaker = i;
                break;
            }
        }
        if (speaker < 0) {
            continue;
        }
        packet[0] = (byte)speaker;

        for (i = 0; i < sv_maxclients->integer; i++) {
            if (svs.clients[i].state != CS_ACTIVE) {
                continue;
            }
            if (SV_GameVoiceFilter(speaker, i) != 0) {
                continue;
            }
            Steam_P2PSend(svs.clients[i].steam_id, packet, bytes + 1, qtrue);
        }
    }
}

void SV_SteamIdentity(void) {
    const char* items;

    if (!Steam_GS_Running()) {
        return;
    }
    SV_SetConfigstring(CS_STEAM_ID, va("%llu", (unsigned long long)Steam_GS_SteamID()));
    items = FS_ReferencedSteamworksItems();
    Cvar_Set("sv_referencedSteamworks", items);
    SV_SetConfigstring(CS_STEAM_WORKSHOP_IDS, items);
}

void SV_SteamFrame(qboolean force) {
    static int lastPlayers;
    const cvar_t* needPass;
    client_t* cl;
    int bots;
    int i;

    if (!Steam_GS_Running() || !svs.clients) {
        return;
    }

    if (force) {
        Steam_GS_SetMaxPlayers(sv_maxclients->integer);
    }
    needPass = Cvar_Find("g_needpass");
    if (needPass && (force || needPass->modified)) {
        Steam_GS_SetPassword((qboolean)(needPass->integer != 0));
        ((cvar_t*)needPass)->modified = qfalse;
    }
    if (force || sv_hostname->modified) {
        Steam_GS_SetName(sv_hostname->string);
        sv_hostname->modified = qfalse;
    }
    if (force || sv_mapname->modified) {
        Steam_GS_SetMap(sv_mapname->string);
        sv_mapname->modified = qfalse;
    }

    if (force || Sys_Milliseconds() - lastPlayers > 3000) {
        Steam_GS_SetKeyValue("g_redScore", sv.configstrings[CS_SCORES1]);
        Steam_GS_SetKeyValue("g_blueScore", sv.configstrings[CS_SCORES2]);

        bots = 0;
        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
            qboolean bot;

            if (cl->state == CS_FREE) {
                continue;
            }
            bot = (qboolean)(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT));
            if (bot) {
                bots++;
            }
            if (!cl->steam_id) {
                continue;
            }
            Steam_GS_UpdateUser(cl->steam_id, va("%s%s", bot ? "(Bot) " : "", Info_ValueForKey(cl->userinfo, "name")),
                                SV_GameClientScore(i));
        }
        lastPlayers = Sys_Milliseconds();
        Steam_GS_SetBotCount(bots);
    }

    Steam_GS_UpdateGameInfo(force, svs.factory ? svs.factory->tags : NULL, svs.factory ? MAX_FACTORY_TAGS : 0);
}

void SV_SteamConnected(void) {
    SV_SteamStatsConnected();
    if (!com_sv_running || !com_sv_running->integer) {
        return;
    }
    SV_SteamIdentity();
    SV_SteamFrame(qtrue);
    Steam_GS_SetKeyValues(Cvar_InfoString(CVAR_SERVERINFO));
}

int SV_FrameMsec() {
    if (sv_fps) {
        int frameMsec;

        frameMsec = 1000.0f / sv_fps->value;

        if (frameMsec < sv.timeResidual)
            return 0;
        else
            return frameMsec - sv.timeResidual;
    } else
        return 1;
}

void SV_Frame(int msec) {
    int frameMsec;
    int startTime;

    if (sv_killserver->integer) {
        SV_Shutdown("Server was killed");
        Cvar_Set("sv_killserver", "0");
        return;
    }

    if (sv_quitOnEmpty && sv_quitOnEmpty->integer && com_sv_running->integer) {
        int i, humanos = 0;

        for (i = 0; i < sv_maxclients->integer; i++) {
            if (svs.clients[i].state >= CS_CONNECTED &&
                svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
                humanos++;
            }
        }
        if (!humanos && svs.time > 60000) {
            Com_Printf("sv_quitOnEmpty: no players left, exiting.\n");
            Com_Quit_f();
            return;
        }
    }

    if (!com_sv_running->integer) {
#ifdef DEDICATED
        Sys_Sleep(-1);
#endif

        return;
    }

    SV_RedirectFrame();

    if (SV_CheckPaused()) {
        return;
    }

    if (sv_fps->integer < 1) {
        Cvar_Set("sv_fps", "10");
    }

    frameMsec = 1000 / sv_fps->integer * com_timescale->value;
    if (frameMsec < 1) {
        Cvar_Set("timescale", va("%f", sv_fps->integer / 1000.0f));
        frameMsec = 1;
    }

    sv.timeResidual += msec;

    if (!com_dedicated->integer)
        SV_BotFrame(sv.time + sv.timeResidual);

    if (svs.time > 0x70000000) {
        SV_Shutdown("Restarting server due to time wrapping");
        Cbuf_AddText(va("map %s\n", Cvar_VariableString("mapname")));
        return;
    }
    if (svs.nextSnapshotEntities >= 0x7FFFFFFE - svs.numSnapshotEntities) {
        SV_Shutdown("Restarting server due to numSnapshotEntities wrapping");
        Cbuf_AddText(va("map %s\n", Cvar_VariableString("mapname")));
        return;
    }

    if (sv.restartTime && sv.time >= sv.restartTime) {
        sv.restartTime = 0;
        Cbuf_AddText("map_restart 0\n");
        return;
    }

    if (cvar_modifiedFlags & CVAR_SERVERINFO) {
        const char* info = Cvar_InfoString(CVAR_SERVERINFO);

        Steam_GS_SetKeyValues(info);
        SV_SetConfigstring(CS_SERVERINFO, info);
        cvar_modifiedFlags &= ~CVAR_SERVERINFO;
    }
    if (cvar_modifiedFlags & CVAR_SYSTEMINFO) {
        SV_SetConfigstring(CS_SYSTEMINFO, Cvar_InfoString_Big(CVAR_SYSTEMINFO));
        cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
    }

    if (com_speeds->integer) {
        startTime = Sys_Milliseconds();
    } else {
        startTime = 0;
    }

    SV_CalcPings();

    if (com_dedicated->integer)
        SV_BotFrame(sv.time);

    while (sv.timeResidual >= frameMsec) {
        sv.timeResidual -= frameMsec;
        svs.time += frameMsec;
        sv.time += frameMsec;

        SV_GameRunFrame(sv.time);
    }

    if (com_speeds->integer) {
        time_game = Sys_Milliseconds() - startTime;
    }

    SV_CheckTimeouts();

    SV_SendClientMessages();
}

#define UDPIP_HEADER_SIZE 28
#define UDPIP6_HEADER_SIZE 48

int SV_RateMsec(client_t* client) {
    int rate, rateMsec;
    int messageSize;

    messageSize = client->netchan.lastSentSize;
    rate = client->rate;

    if (sv_maxRate->integer) {
        if (sv_maxRate->integer < 1000)
            Cvar_Set("sv_MaxRate", "1000");
        if (sv_maxRate->integer < rate)
            rate = sv_maxRate->integer;
    }

    if (sv_minRate->integer) {
        if (sv_minRate->integer < 1000)
            Cvar_Set("sv_minRate", "1000");
        if (sv_minRate->integer > rate)
            rate = sv_minRate->integer;
    }

    if (client->netchan.remoteAddress.type == NA_IP6)
        messageSize += UDPIP6_HEADER_SIZE;
    else
        messageSize += UDPIP_HEADER_SIZE;

    rateMsec = messageSize * 1000 / ((int)(rate * com_timescale->value));
    rate = Sys_Milliseconds() - client->netchan.lastSentTime;

    if (rate > rateMsec)
        return 0;
    else
        return rateMsec - rate;
}

int SV_SendQueuedPackets() {
    int numBlocks;
    int dlStart, deltaT, delayT;
    static int dlNextRound = 0;
    int timeVal = INT_MAX;

    delayT = SV_SendQueuedMessages();
    if (delayT >= 0)
        timeVal = delayT;

    if (sv_dlRate->integer) {
        dlStart = Sys_Milliseconds();
        deltaT = dlNextRound - dlStart;

        if (deltaT > 0) {
            if (deltaT < timeVal)
                timeVal = deltaT + 1;
        } else {
            numBlocks = SV_SendDownloadMessages();

            if (numBlocks) {
                deltaT = Sys_Milliseconds() - dlStart;

                delayT = 1000 * numBlocks * MAX_DOWNLOAD_BLKSIZE;
                delayT /= sv_dlRate->integer * 1024;

                if (delayT <= deltaT + 1) {
                    if (timeVal > 2)
                        timeVal = 2;

                    dlNextRound = dlStart + deltaT + 1;
                } else {
                    dlNextRound = dlStart + delayT;
                    delayT -= deltaT;

                    if (delayT < timeVal)
                        timeVal = delayT;
                }
            }
        }
    } else {
        if (SV_SendDownloadMessages())
            timeVal = 0;
    }

    return timeVal;
}
