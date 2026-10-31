/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

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
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "client.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define DISCORD_APP_ID "1288275925359923251"
#define DISCORD_IMAGE "https://quakeclub.com/icons/favicon-192.png"
#define DISCORD_SITE "https://quakeclub.com"
#define DISCORD_RETRY_MSEC 20000
#define DISCORD_GAP_MSEC 5000
#define DISCORD_TEXT 128

#ifdef MSG_NOSIGNAL
#define DISCORD_SEND_FLAGS MSG_NOSIGNAL
#else
#define DISCORD_SEND_FLAGS 0
#endif

enum {
    DISCORD_OP_HANDSHAKE,
    DISCORD_OP_FRAME,
    DISCORD_OP_CLOSE,
    DISCORD_OP_PING,
    DISCORD_OP_PONG
};

static cvar_t* cl_discordPresence;
#ifdef _WIN32
static HANDLE discordPipe = INVALID_HANDLE_VALUE;
#else
static int discordSock = -1;
#endif
static qboolean discordReady;
static int discordLastTry;
static int discordLastSent;
static int discordNonce;
static char discordLast[2048];
static char discordWhere[MAX_OSPATH];
static long long discordSince;
static byte discordIn[16384];
static int discordInLen;

static const char* const discordGametypes[] = {
    "Free For All", "Duel", "Race", "Team Deathmatch", "Clan Arena", "Capture The Flag", "One Flag CTF",
    "Overload", "Harvester", "Freeze Tag", "Domination", "Attack & Defend", "Red Rover"};

static qboolean Discord_IsOpen(void) {
#ifdef _WIN32
    return discordPipe != INVALID_HANDLE_VALUE;
#else
    return discordSock >= 0;
#endif
}

static void Discord_Close(void) {
#ifdef _WIN32
    if (discordPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(discordPipe);
        discordPipe = INVALID_HANDLE_VALUE;
    }
#else
    if (discordSock >= 0) {
        close(discordSock);
        discordSock = -1;
    }
#endif
    discordReady = qfalse;
    discordInLen = 0;
    discordLast[0] = '\0';
}

#ifndef _WIN32
static qboolean Discord_PeerIsUser(int s) {
#if defined(__linux__)
    struct ucred cred;
    socklen_t len = sizeof(cred);

    return (qboolean)(getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0 && cred.uid == getuid());
#else
    uid_t uid;
    gid_t gid;

    return (qboolean)(getpeereid(s, &uid, &gid) == 0 && uid == getuid());
#endif
}

static qboolean Discord_OpenPath(const char* dir, const char* sub, int n) {
    struct sockaddr_un addr;
    int s;

    Com_Memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (Com_sprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%sdiscord-ipc-%i", dir, sub, n) >=
        (int)sizeof(addr.sun_path) - 1) {
        return qfalse;
    }
    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) {
        return qfalse;
    }
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0 || !Discord_PeerIsUser(s)) {
        close(s);
        return qfalse;
    }
#ifdef SO_NOSIGPIPE
    {
        int one = 1;

        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    }
#endif
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    discordSock = s;
    return qtrue;
}
#endif

static qboolean Discord_Open(void) {
    int n;
#ifdef _WIN32
    for (n = 0; n < 10; n++) {
        HANDLE h = CreateFileA(va("\\\\.\\pipe\\discord-ipc-%i", n), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);

        if (h != INVALID_HANDLE_VALUE) {
            discordPipe = h;
            return qtrue;
        }
    }
#else
    static const char* const vars[] = {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"};
    static const char* const subs[] = {"", "app/com.discordapp.Discord/", "snap.discord/"};
    const char* dirs[5];
    int numDirs = 0;
    int d, s;

    for (d = 0; d < 4; d++) {
        const char* v = getenv(vars[d]);

        if (v && v[0]) {
            dirs[numDirs++] = v;
        }
    }
    dirs[numDirs++] = "/tmp";
    for (d = 0; d < numDirs; d++) {
        for (s = 0; s < 3; s++) {
            for (n = 0; n < 10; n++) {
                if (Discord_OpenPath(dirs[d], subs[s], n)) {
                    return qtrue;
                }
            }
        }
    }
#endif
    return qfalse;
}

static qboolean Discord_Write(int op, const char* json, int len) {
    byte frame[4096];
    int total = 8 + len;
    int i;

    if (!Discord_IsOpen() || total > (int)sizeof(frame)) {
        return qfalse;
    }
    for (i = 0; i < 4; i++) {
        frame[i] = (byte)(op >> (8 * i));
        frame[4 + i] = (byte)(len >> (8 * i));
    }
    Com_Memcpy(frame + 8, json, len);
#ifdef _WIN32
    {
        DWORD written = 0;

        if (!WriteFile(discordPipe, frame, total, &written, NULL) || (int)written != total) {
            Discord_Close();
            return qfalse;
        }
    }
#else
    if (send(discordSock, frame, total, DISCORD_SEND_FLAGS) != total) {
        Discord_Close();
        return qfalse;
    }
#endif
    return qtrue;
}

static int Discord_ReadInt(const byte* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void Discord_Drain(void) {
    int room;
    int got = 0;

    while (Discord_IsOpen()) {
        room = (int)sizeof(discordIn) - discordInLen;
#ifdef _WIN32
        {
            DWORD avail = 0;
            DWORD read = 0;

            if (!PeekNamedPipe(discordPipe, NULL, 0, NULL, &avail, NULL)) {
                Discord_Close();
                return;
            }
            if (!avail) {
                break;
            }
            if ((int)avail > room) {
                avail = room;
            }
            if (!ReadFile(discordPipe, discordIn + discordInLen, avail, &read, NULL)) {
                Discord_Close();
                return;
            }
            got = (int)read;
        }
#else
        got = (int)recv(discordSock, discordIn + discordInLen, room, 0);
        if (got == 0 || (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            Discord_Close();
            return;
        }
        if (got < 0) {
            break;
        }
#endif
        discordInLen += got;
        while (discordInLen >= 8) {
            int op = Discord_ReadInt(discordIn);
            int len = Discord_ReadInt(discordIn + 4);
            char text[sizeof(discordIn)];

            if (len < 0 || len > (int)sizeof(discordIn) - 8) {
                Discord_Close();
                return;
            }
            if (discordInLen < 8 + len) {
                break;
            }
            Com_Memcpy(text, discordIn + 8, len);
            text[len] = '\0';
            discordInLen -= 8 + len;
            memmove(discordIn, discordIn + 8 + len, discordInLen);
            if (op == DISCORD_OP_PING) {
                Discord_Write(DISCORD_OP_PONG, text, len);
            } else if (op == DISCORD_OP_CLOSE) {
                Com_DPrintf("Discord closed the presence: %s\n", text);
                Discord_Close();
                return;
            } else if (op == DISCORD_OP_FRAME && strstr(text, "\"evt\"") && strstr(text, "\"READY\"")) {
                discordReady = qtrue;
            } else if (op == DISCORD_OP_FRAME && strstr(text, "\"evt\"") && strstr(text, "\"ERROR\"")) {
                Com_DPrintf("Discord presence: %s\n", text);
            }
        }
        if (discordInLen == (int)sizeof(discordIn)) {
            Discord_Close();
            return;
        }
    }
}

static void Discord_Escape(char* out, int size, const char* in) {
    int l = 0;
    int chars = 0;

    for (; *in && l < size - 3 && chars < DISCORD_TEXT - 1; in++) {
        if (Q_IsColorString(in)) {
            in++;
            continue;
        }
        if (*in < ' ' || *in > '~') {
            continue;
        }
        if (*in == '"' || *in == '\\') {
            out[l++] = '\\';
        }
        out[l++] = *in;
        chars++;
    }
    out[l] = '\0';
}

static void Discord_Field(char* json, int size, const char* key, const char* value) {
    char text[DISCORD_TEXT * 2 + 1];

    Discord_Escape(text, sizeof(text), value);
    if (strlen(text) >= 2) {
        Q_strcat(json, size, va(",\"%s\":\"%s\"", key, text));
    }
}

static unsigned Discord_Hash(const char* s) {
    unsigned h = 2166136261u;

    for (; *s; s++) {
        h = (h ^ (byte)*s) * 16777619u;
    }
    return h;
}

static const char* Discord_ConfigString(int index) {
    return cl.gameState.stringData + cl.gameState.stringOffsets[index];
}

static void Discord_Activity(char* json, int size) {
    char details[DISCORD_TEXT];
    char where[MAX_OSPATH];
    char factory[64];
    char map[MAX_QPATH];
    char host[DISCORD_TEXT];
    const char* state = "";
    const char* info;
    int players = 0;
    int maxPlayers = 0;
    int i;

    where[0] = '\0';
    if (clc.state == CA_ACTIVE) {
        int gametype;

        info = Discord_ConfigString(CS_SERVERINFO);
        Q_strncpyz(factory, Info_ValueForKey(info, "g_factoryTitle"), sizeof(factory));
        Q_strncpyz(map, Info_ValueForKey(info, "mapname"), sizeof(map));
        Q_strncpyz(host, Info_ValueForKey(info, "sv_hostname"), sizeof(host));
        gametype = atoi(Info_ValueForKey(info, "g_gametype"));
        if (!factory[0] && gametype >= 0 && gametype < (int)ARRAY_LEN(discordGametypes)) {
            Q_strncpyz(factory, discordGametypes[gametype], sizeof(factory));
        }
        if (clc.demoplaying) {
            Com_sprintf(details, sizeof(details), "Watching a demo");
            state = map;
        } else {
            Com_sprintf(details, sizeof(details), "%s on %s", factory[0] ? factory : "Playing", map);
            if (!Q_stricmp(clc.servername, "localhost")) {
                state = "Local match";
            } else {
                state = host;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (Discord_ConfigString(CS_PLAYERS + i)[0]) {
                        players++;
                    }
                }
                maxPlayers = atoi(Info_ValueForKey(info, "sv_maxclients"));
            }
            Q_strncpyz(where, clc.servername, sizeof(where));
        }
    } else if (clc.state > CA_DISCONNECTED && clc.state != CA_CINEMATIC) {
        Com_sprintf(details, sizeof(details), "Connecting");
    } else {
        Com_sprintf(details, sizeof(details), "In the menus");
    }

    if (where[0] && Q_stricmp(where, discordWhere)) {
        discordSince = (long long)time(NULL);
    }
    Q_strncpyz(discordWhere, where, sizeof(discordWhere));

    json[0] = '\0';
    Q_strcat(json, size, "\"assets\":{\"large_image\":\"" DISCORD_IMAGE "\",\"large_text\":\"ioquakelive " PRODUCT_VERSION "\"}");
    Discord_Field(json, size, "details", details);
    Discord_Field(json, size, "state", state);
    if (where[0]) {
        Q_strcat(json, size, va(",\"timestamps\":{\"start\":%lld}", discordSince));
    }
    if (players > 0 && maxPlayers >= players) {
        Q_strcat(json, size, va(",\"party\":{\"id\":\"ioql-%08x\",\"size\":[%i,%i]}",
                                Discord_Hash(where), players, maxPlayers));
    }
    Q_strcat(json, size, ",\"buttons\":[{\"label\":\"quakeclub.com\",\"url\":\"" DISCORD_SITE "\"}]");
}

static int Discord_Pid(void) {
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

static void Discord_Send(const char* activity) {
    char payload[3072];

    if (activity) {
        Com_sprintf(payload, sizeof(payload),
                    "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%i,\"activity\":{%s}},\"nonce\":\"%i\"}", Discord_Pid(),
                    activity, ++discordNonce);
    } else {
        Com_sprintf(payload, sizeof(payload), "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%i},\"nonce\":\"%i\"}",
                    Discord_Pid(), ++discordNonce);
    }
    Discord_Write(DISCORD_OP_FRAME, payload, (int)strlen(payload));
}

void CL_Discord_Init(void) {
    cl_discordPresence = Cvar_Get("cl_discordPresence", "1", CVAR_ARCHIVE);
    Cvar_CheckRange(cl_discordPresence, 0, 1, qtrue);
}

void CL_Discord_Shutdown(void) {
    if (Discord_IsOpen() && discordReady) {
        Discord_Send(NULL);
    }
    Discord_Close();
}

void CL_Discord_Frame(void) {
    static const char handshake[] = "{\"v\":1,\"client_id\":\"" DISCORD_APP_ID "\"}";
    char activity[2048];
    int now = Sys_Milliseconds();

    if (!cl_discordPresence || com_dedicated->integer) {
        return;
    }
    if (!cl_discordPresence->integer) {
        if (Discord_IsOpen()) {
            CL_Discord_Shutdown();
        }
        return;
    }
    if (!Discord_IsOpen()) {
        if (discordLastTry && now - discordLastTry < DISCORD_RETRY_MSEC) {
            return;
        }
        discordLastTry = now;
        if (!Discord_Open()) {
            return;
        }
        Discord_Write(DISCORD_OP_HANDSHAKE, handshake, (int)strlen(handshake));
    }
    Discord_Drain();
    if (!discordReady || (discordLastSent && now - discordLastSent < DISCORD_GAP_MSEC)) {
        return;
    }
    Discord_Activity(activity, sizeof(activity));
    if (!strcmp(activity, discordLast)) {
        return;
    }
    Discord_Send(activity);
    Q_strncpyz(discordLast, activity, sizeof(discordLast));
    discordLastSent = now;
}
