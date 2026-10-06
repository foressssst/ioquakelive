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

serverStatic_t svs;  // persistant server info
server_t sv;         // local server
vm_t* gvm = NULL;    // game virtual machine

cvar_t* sv_fps = NULL;       // time rate for running non-clients
cvar_t* sv_timeout;          // seconds without any message
cvar_t* sv_zombietime;       // seconds to sink messages after disconnect
cvar_t* sv_rconPassword;     // password for remote server commands
cvar_t* sv_privatePassword;  // password for the privateClient slots
cvar_t* sv_allowDownload;
cvar_t* sv_maxclients;

cvar_t* sv_privateClients;  // number of clients reserved for password
cvar_t* sv_hostname;
cvar_t* sv_reconnectlimit;  // minimum seconds between connect messages
cvar_t* sv_showloss;        // report when usercmds are lost
cvar_t* sv_padPackets;      // add nop bytes to messages
cvar_t* sv_killserver;      // menu system can set to 1 to shut server down
cvar_t* sv_mapname;
cvar_t* sv_mapChecksum;
cvar_t* sv_serverid;
cvar_t* sv_minRate;
cvar_t* sv_maxRate;
cvar_t* sv_dlRate;
cvar_t* sv_minPing;
cvar_t* sv_maxPing;
cvar_t* sv_serverType;  // [QL] 1 = LAN-only server
cvar_t* sv_gametype;
cvar_t* sv_ammoPack;  // [QL] g_ammoPack
cvar_t* sv_pure;
cvar_t* sv_floodProtect;
cvar_t* sv_master;       // [QL] answer queries and show on the list
cvar_t* sv_idleExit;     // [QL] shut down after so long with no map
cvar_t* sv_errorExit;    // [QL] shut down on an error
cvar_t* sv_workshopFile; // [QL] the file listing the Workshop items, the dedicated server's
cvar_t* sv_quitOnEmpty;  // [QL] shut down once the server is left with nobody
cvar_t* sv_fullRedirect; // [ioquakelive] where to send the ones who find the server full
cvar_t* sv_lanForceRate;  // dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
cvar_t* sv_banFile;
// [QL] 0x004e3e7a and 0x004e3e90: the level's entities come from a separate file, or are
// dumped to one
cvar_t* sv_altEntDir;
cvar_t* sv_dumpEntities;

serverBan_t serverBans[SERVER_MAXBANS];
int serverBansCount = 0;

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
===============
SV_ExpandNewlines

Converts newlines to "\n" so a line prints nicer
===============
*/
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

/*
======================
SV_AddServerCommand

The given command will be transmitted to the client, and is guaranteed to
not have future snapshot_t executed before it is executed
======================
*/
void SV_AddServerCommand(client_t* client, const char* cmd) {
    int index, i;

    // It would be wasteful to queue multiple configstring updates for the same index in one snapshot.

    // do not send commands until the gamestate has been sent
    if (client->state < CS_PRIMED)
        return;

    client->reliableSequence++;
    // if we would be losing an old command that hasn't been acknowledged,
    // we must drop the connection
    // we check == instead of >= so a broadcast print added by SV_DropClient()
    // doesn't cause a recursive drop client
    if (client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1) {
        if (client->gamestateMessageNum == -1) {
            // invalid game state message
            // this can occur in SV_DropClient() to avoid calling it more than once
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

/*
=================
SV_SendServerCommand

Sends a reliable command string to be interpreted by
the client game module: "cp", "print", "chat", etc
A NULL client will broadcast to all clients
=================
*/
void QDECL SV_SendServerCommand(client_t* cl, const char* fmt, ...) {
    va_list argptr;
    byte message[MAX_MSGLEN];
    client_t* client;
    int j;

    va_start(argptr, fmt);
    Q_vsnprintf((char*)message, sizeof(message), fmt, argptr);
    va_end(argptr);

    // Fix to http://aluigi.altervista.org/adv/q3msgboom-adv.txt
    // The actual cause of the bug is probably further downstream
    // and should maybe be addressed later, but this certainly
    // fixes the problem for now
    if (strlen((char*)message) > 1022) {
        return;
    }

    if (cl != NULL) {
        SV_AddServerCommand(cl, (char*)message);
        return;
    }

    // hack to echo broadcast prints to console
    if (com_dedicated->integer && !strncmp((char*)message, "print", 5)) {
        Com_Printf("broadcast: %s\n", SV_ExpandNewlines((char*)message));
    }

    // send the data to all relevant clients
    for (j = 0, client = svs.clients; j < sv_maxclients->integer; j++, client++) {
        SV_AddServerCommand(client, (char*)message);
    }
}

/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

// This is deliberately quite large to make it more of an effort to DoS
#define MAX_BUCKETS 16384
#define MAX_HASHES 1024

static leakyBucket_t buckets[MAX_BUCKETS];
static leakyBucket_t* bucketHashes[MAX_HASHES];
leakyBucket_t outboundLeakyBucket;

/*
================
SVC_HashForAddress
================
*/
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

/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
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

        // Reclaim expired buckets
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

            // Add to the head of the relevant hash chain
            bucket->next = bucketHashes[hash];
            if (bucketHashes[hash] != NULL) {
                bucketHashes[hash]->prev = bucket;
            }

            bucket->prev = NULL;
            bucketHashes[hash] = bucket;

            return bucket;
        }
    }

    // Couldn't allocate a bucket for this address
    return NULL;
}

/*
================
SVC_RateLimit
================
*/
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

/*
================
SVC_RateLimitAddress

Rate limit for a particular address
================
*/
qboolean SVC_RateLimitAddress(netadr_t from, int burst, int period) {
    leakyBucket_t* bucket = SVC_BucketForAddress(from, burst, period);

    return SVC_RateLimit(bucket, burst, period);
}

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/
static void SVC_Status(netadr_t from) {
    char player[1024];
    char status[MAX_MSGLEN];
    int i;
    client_t* cl;
    playerState_t* ps;
    int statusLength;
    int playerLength;
    char infostring[MAX_INFO_STRING];

    // Prevent using getstatus as an amplifier
    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SVC_Status: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    // Allow getstatus to be DoSed relatively easily, but prevent
    // excess outbound bandwidth usage when being flooded inbound
    if (SVC_RateLimit(&outboundLeakyBucket, 10, 100)) {
        Com_DPrintf("SVC_Status: rate limit exceeded, dropping request\n");
        return;
    }

    // A maximum challenge length of 128 should be more than plenty.
    if (strlen(Cmd_Argv(1)) > 128)
        return;

    strcpy(infostring, Cvar_InfoString(CVAR_SERVERINFO));

    // echo back the parameter to status. so master servers can use it as a challenge
    // to prevent timed spoofed reply packets that add ghost servers
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
                break;  // can't hold any more
            }
            strcpy(status + statusLength, player);
            statusLength += playerLength;
        }
    }

    NET_OutOfBandPrint(NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status);
}

/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
void SVC_Info(netadr_t from) {
    int i, count, humans;
    char* gamedir;
    char infostring[MAX_INFO_STRING];

    // Prevent using getinfo as an amplifier
    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SVC_Info: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    // Allow getinfo to be DoSed relatively easily, but prevent
    // excess outbound bandwidth usage when being flooded inbound
    if (SVC_RateLimit(&outboundLeakyBucket, 10, 100)) {
        Com_DPrintf("SVC_Info: rate limit exceeded, dropping request\n");
        return;
    }

    /*
     * Check whether Cmd_Argv(1) has a sane length. This was not done in the original Quake3 version which led
     * to the Infostring bug discovered by Luigi Auriemma. See http://aluigi.altervista.org/ for the advisory.
     */

    // A maximum challenge length of 128 should be more than plenty.
    if (strlen(Cmd_Argv(1)) > 128)
        return;

    // don't count privateclients
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

    // echo back the parameter to status. so servers can use it as a challenge
    // to prevent timed spoofed reply packets that add ghost servers
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

/*
================
SVC_FlushRedirect

================
*/
static void SV_FlushRedirect(char* outputbuf) {
    NET_OutOfBandPrint(NS_SERVER, svs.redirectAddress, "print\n%s", outputbuf);
}

/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
static void SVC_RemoteCommand(netadr_t from, msg_t* msg) {
    qboolean valid;
    char remaining[1024];
    // TTimo - scaled down to accumulate, but not overflow anything network wise, print wise etc.
    // (OOB messages are the bottleneck here)
#define SV_OUTPUTBUF_LENGTH (1024 - 16)
    char sv_outputbuf[SV_OUTPUTBUF_LENGTH];
    char* cmd_aux;

    // Prevent using rcon as an amplifier and make dictionary attacks impractical
    if (SVC_RateLimitAddress(from, 10, 1000)) {
        Com_DPrintf("SVC_RemoteCommand: rate limit from %s exceeded, dropping request\n",
                    NET_AdrToString(from));
        return;
    }

    if (!strlen(sv_rconPassword->string) ||
        strcmp(Cmd_Argv(1), sv_rconPassword->string)) {
        static leakyBucket_t bucket;

        // Make DoS via rcon impractical
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

    // start redirecting all print outputs to the packet
    svs.redirectAddress = from;
    Com_BeginRedirect(sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

    if (!strlen(sv_rconPassword->string)) {
        Com_Printf("No rconpassword set on the server.\n");
    } else if (!valid) {
        Com_Printf("Bad rconpassword.\n");
    } else {
        remaining[0] = 0;

        // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
        // get the command directly, "rcon <pass> <command>" to avoid quoting issues
        // extract the command by walking
        // since the cmd formatting can fuckup (amount of spaces), using a dumb step by step parsing
        cmd_aux = Cmd_Cmd();
        cmd_aux += 4;
        while (cmd_aux[0] == ' ')
            cmd_aux++;
        while (cmd_aux[0] && cmd_aux[0] != ' ')  // password
            cmd_aux++;
        while (cmd_aux[0] == ' ')
            cmd_aux++;

        Q_strcat(remaining, sizeof(remaining), cmd_aux);

        Cmd_ExecuteString(remaining);
    }

    Com_EndRedirect();
}

/*
===============
SV_RedirectedCommand

[QL] quakelive_steam.exe 0x4e001d: a server command whose output goes to one address, as an
admin's in-game rcon does
===============
*/
void SV_RedirectedCommand(netadr_t to, const char* text) {
    char outputbuf[SV_OUTPUTBUF_LENGTH];

    svs.redirectAddress = to;
    Com_BeginRedirect(outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
    Cmd_ExecuteString(text);
    Com_EndRedirect();
}

/*
==============================================================================

[ioquakelive] THE ONES WHO FIND THE SERVER FULL GO TO ANOTHER ONE

sv_fullRedirect lists other servers, an address each (127.0.0.1:<port> for one on this
machine). The server asks each of them how full it is every few seconds, and someone who
comes when there is no slot left is told the one with room that has the most players, so
the servers fill one after another instead of everyone waiting at the door of the full one.
They are asked both ways: getinfo, which ioqlds answers, and Steam's A2S_INFO, the server
browser's query, which is the only one Quake Live's servers answer.
Only an ioquakelive client follows it (cl_followRedirect); Quake Live's client does not know
the packet and gets "Server is full." as always. Nobody already in is ever moved.

==============================================================================
*/

#define SV_MAX_REDIRECTS 8
#define SV_REDIRECT_ASK 5000     // how often the others are asked
#define SV_REDIRECT_FRESH 15000  // how old an answer may be and still count

typedef struct {
    netadr_t adr;
    int answered;  // Sys_Milliseconds of the last answer, 0 for none
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

// Steam's A2S_INFO, with the challenge it asked for when there is one
static void SV_RedirectAskSteam(netadr_t adr, const byte* challenge) {
    static const char query[] = "\xff\xff\xff\xffTSource Engine Query";
    byte packet[sizeof(query) + 4];

    Com_Memcpy(packet, query, sizeof(query));  // the terminating zero goes too
    if (challenge) {
        Com_Memcpy(packet + sizeof(query), challenge, 4);
    }
    NET_SendPacket(NS_SERVER, sizeof(query) + (challenge ? 4 : 0), packet, adr);
}

// every frame: the list when it changes, and the question to the others
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

// skips a string of an A2S answer; NULL when the packet ends first
static const byte* SV_SkipSteamString(const byte* p, const byte* end) {
    while (p < end && *p) {
        p++;
    }
    return (p < end) ? p + 1 : NULL;
}

/*
=================
SV_RedirectSteamAnswer

The A2S answer of one of the servers of the list: its challenge ('A'), to ask again with it,
or its info ('I'): protocol, name, map, folder and game, the app id, players, the most
players, bots, kind, system and whether it has a password.
=================
*/
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
    p++;  // protocol
    for (i = 0; i < 4 && p; i++) {
        p = SV_SkipSteamString(p, end);
    }
    if (!p || end - p < 9) {
        return qtrue;
    }
    p += 2;  // app id
    r->clients = p[0];
    r->maxClients = p[1];
    r->needPass = p[5] ? qtrue : qfalse;
    r->answered = Sys_Milliseconds();
    return qtrue;
}

// the answer of one of them, as any Quake 3 server gives it
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

/*
=================
SV_FullRedirect

Someone found no slot left: the server with room and the most players, if any answered lately.
One on this machine goes as ":<port>", for the client to put this server's address in front.
It carries the challenge the client was just checked with, which only the two of them know,
so a packet forged with this server's address cannot send a player anywhere.
=================
*/
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

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static void SV_ConnectionlessPacket(netadr_t from, msg_t* msg) {
    char* s;
    char* c;

    // [ioquakelive] Steam's answer of a server of sv_fullRedirect
    if (svNumRedirects && SV_RedirectSteamAnswer(from, msg)) {
        return;
    }

    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg);  // skip the -1 marker

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
        // if a client starts up a local server, we may see some spurious
        // server disconnect messages when their new server sees our final
        // sequenced messages to the old client
    } else if (!Q_stricmp(c, "infoResponse") && svNumRedirects) {
        SV_RedirectInfoResponse(from, msg);  // [ioquakelive] sv_fullRedirect
    } else if (Steam_GS_HandlePacket(from, msg->data, msg->cursize)) {
        // [QL] qzeroded.x64 0x43e268: what the server does not know goes to Steam, whose
        // queries arrive on the game's socket (query port 0xffff). Only here: a client's packets
        // never go to Steam, which would keep the server's answers for itself
    } else {
        Com_DPrintf("bad connectionless packet from %s:\n%s\n",
                    NET_AdrToString(from), s);
    }
}

//============================================================================

/*
=================
SV_PacketEvent
=================
*/
void SV_PacketEvent(netadr_t from, msg_t* msg) {
    int i;
    client_t* cl;
    int qport;

    // check for connectionless packet (0xffffffff) first
    if (msg->cursize >= 4 && *(int*)msg->data == -1) {
        SV_ConnectionlessPacket(from, msg);
        return;
    }

    // read the qport out of the message so we can fix up
    // stupid address translating routers
    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg);  // sequence number
    qport = MSG_ReadShort(msg) & 0xffff;

    // find which client the message is from
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state == CS_FREE) {
            continue;
        }
        if (!NET_CompareBaseAdr(from, cl->netchan.remoteAddress)) {
            continue;
        }
        // it is possible to have multiple clients from a single IP
        // address, so they are differentiated by the qport variable
        if (cl->netchan.qport != qport) {
            continue;
        }

        // the IP port can't be used to differentiate them, because
        // some address translating routers periodically change UDP
        // port assignments
        if (cl->netchan.remoteAddress.port != from.port) {
            Com_Printf("SV_PacketEvent: fixing up a translated port\n");
            cl->netchan.remoteAddress.port = from.port;
        }

        // make sure it is a valid, in sequence packet
        if (SV_Netchan_Process(cl, msg)) {
            // zombie clients still need to do the Netchan_Process
            // to make sure they don't need to retransmit the final
            // reliable message, but they don't do any other processing
            if (cl->state != CS_ZOMBIE) {
                cl->lastPacketTime = svs.time;  // don't timeout
                SV_ExecuteClientMessage(cl, msg);
            }
        }
        return;
    }
}

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
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

        // let the game dll know about the ping
        ps = SV_GameClientNum(i);
        ps->ping = cl->ping;
    }
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer
seconds, drop the conneciton.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void SV_CheckTimeouts(void) {
    int i;
    client_t* cl;
    int droppoint;
    int zombiepoint;

    droppoint = svs.time - 1000 * sv_timeout->integer;
    zombiepoint = svs.time - 1000 * sv_zombietime->integer;

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        // message times may be wrong across a changelevel
        if (cl->lastPacketTime > svs.time) {
            cl->lastPacketTime = svs.time;
        }

        if (cl->state == CS_ZOMBIE && cl->lastPacketTime < zombiepoint) {
            // using the client id cause the cl->name is empty at this point
            Com_DPrintf("Going from CS_ZOMBIE to CS_FREE for client %d\n", i);
            cl->state = CS_FREE;  // can now be reused
            continue;
        }
        if (cl->state >= CS_CONNECTED && cl->lastPacketTime < droppoint) {
            // wait several frames so a debugger session doesn't
            // cause a timeout
            if (++cl->timeoutCount > 5) {
                SV_DropClient(cl, "timed out");
                cl->state = CS_FREE;  // don't bother with zombie state
            }
        } else {
            cl->timeoutCount = 0;
        }
    }
}

/*
==================
SV_CheckPaused
==================
*/
static qboolean SV_CheckPaused(void) {
    int count;
    client_t* cl;
    int i;

    if (!cl_paused->integer) {
        return qfalse;
    }

    // only pause if there is just a single client connected
    count = 0;
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state >= CS_CONNECTED && cl->netchan.remoteAddress.type != NA_BOT) {
            count++;
        }
    }

    if (count > 1) {
        // don't pause
        if (sv_paused->integer)
            Cvar_Set("sv_paused", "0");
        return qfalse;
    }

    if (!sv_paused->integer)
        Cvar_Set("sv_paused", "1");
    return qtrue;
}

/*
==================
SV_VoiceFrame

The server bridges the voice: each player sends theirs to it over Steam's peer to peer
network, and it hands them out to the others with the speaker's client number in front.

[QL] 0x00466850, in the same frame that services the server's Steam. Every ten seconds
it sends each player a greeting, which is the only thing keeping the peer session alive,
and while there are packets on the voice channel it finds whose they are by Steam ID and
passes them to everyone the game module lets hear that player.

One deliberate difference: when the sender is none of the players retail hands the packet
out anyway, passing it off as client 0. Here it is dropped.
==================
*/
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

/*
==================
SV_SteamIdentity

[QL] 0x00465b00 (qzeroded.x64 0x447120): who this server is as far as Steam is concerned
and which Workshop items are needed to play here. The client downloads the ones it lacks
before entering.
==================
*/
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

/*
==================
SV_SteamFrame

[QL] qzeroded.x64 0x4468a0. The server's entry in Steam's list: the password, the name
and the map when they change, and every three seconds the score, who is playing and
how many of them are bots, which is what the browser shows about a match. Forced when
the map starts and when the server logs on, and then everything goes out again.

Each player goes with the score the game keeps; a bot has "(Bot) " in front, and only
a client with a Steam ID can be listed.
==================
*/
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

/*
==================
SV_SteamConnected

[QL] qzeroded.x64 0x447190. The server logged on to Steam: everything its entry shows
goes out again, now under its own Steam ID.
==================
*/
void SV_SteamConnected(void) {
    SV_SteamStatsConnected();  // [QL] 0x00467190, each player's stats are asked for
    if (!com_sv_running || !com_sv_running->integer) {
        return;
    }
    SV_SteamIdentity();
    SV_SteamFrame(qtrue);
    Steam_GS_SetKeyValues(Cvar_InfoString(CVAR_SERVERINFO));
}

/*
==================
SV_FrameMsec
Return time in millseconds until processing of the next server frame.
==================
*/
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

/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
void SV_Frame(int msec) {
    int frameMsec;
    int startTime;

    // the menu kills the server with this cvar
    if (sv_killserver->integer) {
        SV_Shutdown("Server was killed");
        Cvar_Set("sv_killserver", "0");
        return;
    }

    // [QL] 0x004e3e3a: with sv_quitOnEmpty, a server left with nobody on it shuts down, so
    // that whatever supervises it decides whether to bring it back up
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
        // Running as a server, but no map loaded
#ifdef DEDICATED
        // Block until something interesting happens
        Sys_Sleep(-1);
#endif

        return;
    }

    // [ioquakelive] how full the servers of sv_fullRedirect are
    SV_RedirectFrame();

    // allow pause if only the local client is connected
    if (SV_CheckPaused()) {
        return;
    }

    // if it isn't time for the next frame, do nothing
    if (sv_fps->integer < 1) {
        Cvar_Set("sv_fps", "10");
    }

    frameMsec = 1000 / sv_fps->integer * com_timescale->value;
    // don't let it scale below 1ms
    if (frameMsec < 1) {
        Cvar_Set("timescale", va("%f", sv_fps->integer / 1000.0f));
        frameMsec = 1;
    }

    sv.timeResidual += msec;

    if (!com_dedicated->integer)
        SV_BotFrame(sv.time + sv.timeResidual);

    // if time is about to hit the 32nd bit, kick all clients
    // and clear sv.time, rather
    // than checking for negative time wraparound everywhere.
    // 2giga-milliseconds = 23 days, so it won't be too often
    if (svs.time > 0x70000000) {
        SV_Shutdown("Restarting server due to time wrapping");
        Cbuf_AddText(va("map %s\n", Cvar_VariableString("mapname")));
        return;
    }
    // this can happen considerably earlier when lots of clients play and the map doesn't change
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

    // update infostrings if anything has been changed
    if (cvar_modifiedFlags & CVAR_SERVERINFO) {
        // [QL] qzeroded.x64 0x43ead0: Steam's list carries the same keys, as the server's rules
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
        startTime = 0;  // quite a compiler warning
    }

    // update ping based on the all received frames
    SV_CalcPings();

    if (com_dedicated->integer)
        SV_BotFrame(sv.time);

    // run the game simulation in chunks
    while (sv.timeResidual >= frameMsec) {
        sv.timeResidual -= frameMsec;
        svs.time += frameMsec;
        sv.time += frameMsec;

        // let everything in the world think and move
        SV_GameRunFrame(sv.time);
    }

    if (com_speeds->integer) {
        time_game = Sys_Milliseconds() - startTime;
    }

    // check timeouts
    SV_CheckTimeouts();

    // send messages back to the clients
    SV_SendClientMessages();
}

/*
====================
SV_RateMsec

Return the number of msec until another message can be sent to
a client based on its rate settings
====================
*/

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

/*
====================
SV_SendQueuedPackets

Send download messages and queued packets in the time that we're idle, i.e.
not computing a server frame or sending client snapshots.
Return the time in msec until we expect to be called next
====================
*/

int SV_SendQueuedPackets() {
    int numBlocks;
    int dlStart, deltaT, delayT;
    static int dlNextRound = 0;
    int timeVal = INT_MAX;

    // Send out fragmented packets now that we're idle
    delayT = SV_SendQueuedMessages();
    if (delayT >= 0)
        timeVal = delayT;

    if (sv_dlRate->integer) {
        // Rate limiting. This is very imprecise for high
        // download rates due to millisecond timedelta resolution
        dlStart = Sys_Milliseconds();
        deltaT = dlNextRound - dlStart;

        if (deltaT > 0) {
            if (deltaT < timeVal)
                timeVal = deltaT + 1;
        } else {
            numBlocks = SV_SendDownloadMessages();

            if (numBlocks) {
                // There are active downloads
                deltaT = Sys_Milliseconds() - dlStart;

                delayT = 1000 * numBlocks * MAX_DOWNLOAD_BLKSIZE;
                delayT /= sv_dlRate->integer * 1024;

                if (delayT <= deltaT + 1) {
                    // Sending the last round of download messages
                    // took too long for given rate, don't wait for
                    // next round, but always enforce a 1ms delay
                    // between DL message rounds so we don't hog
                    // all of the bandwidth. This will result in an
                    // effective maximum rate of 1MB/s per user, but the
                    // low download window size limits this anyways.
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
