
#include "q_zmq.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET zsocket_t;
#define ZINVALID INVALID_SOCKET
#define zclose closesocket
#define ZWOULDBLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int zsocket_t;
#define ZINVALID (-1)
#define zclose close
#define ZWOULDBLOCK (errno == EWOULDBLOCK || errno == EAGAIN)
#endif

#define ZMQ_MAX_CONNS 16
#define ZMQ_IN_SIZE 65536
#define ZMQ_OUT_SIZE (256 * 1024)

typedef enum {
    ZS_GREETING,
    ZS_HELLO,
    ZS_INITIATE,
    ZS_READY,
    ZS_OPEN,
} zstate_t;

typedef enum { ZT_PUB, ZT_ROUTER } ztype_t;

typedef struct {
    zsocket_t s;
    zstate_t state;
    char address[64];
    byte in[ZMQ_IN_SIZE];
    int inLen;
    byte out[ZMQ_OUT_SIZE];
    int outLen;
    char topics[8][64];
    int numTopics;
    char message[4096];
    int messageLen;
} zconn_t;

typedef struct {
    zsocket_t listen;
    ztype_t type;
    const char* domain;
    cvar_t* password;
    zconn_t conns[ZMQ_MAX_CONNS];
} zsock_t;

static zsock_t zStats = {ZINVALID};
static zsock_t zRcon = {ZINVALID};
static qboolean zNetUp;

static cvar_t* zmq_rcon_enable;
static cvar_t* zmq_stats_enable;
static cvar_t* zmq_rcon_ip;
static cvar_t* zmq_rcon_port;
static cvar_t* zmq_stats_ip;
static cvar_t* zmq_stats_port;
static cvar_t* zmq_stats_password;
static cvar_t* zmq_rcon_password;

static void ZMQ_NonBlocking(zsocket_t s) {
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static qboolean ZMQ_Bind(zsock_t* zs, const char* ip, int port) {
    struct sockaddr_in addr;
    int yes = 1;

#ifdef _WIN32
    if (!zNetUp) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        zNetUp = qtrue;
    }
#endif
    zs->listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (zs->listen == ZINVALID) {
        return qfalse;
    }
    setsockopt(zs->listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    Com_Memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (!ip[0] || inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (bind(zs->listen, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(zs->listen, 8) != 0) {
        zclose(zs->listen);
        zs->listen = ZINVALID;
        return qfalse;
    }
    ZMQ_NonBlocking(zs->listen);
    return qtrue;
}

static void ZMQ_Drop(zsock_t* zs, zconn_t* c) {
    if (c->s == ZINVALID) {
        return;
    }
    if (zs == &zRcon && c->state == ZS_OPEN) {
        Com_Printf("zmq RCON client disconnected: %s\n", c->address);
    }
    zclose(c->s);
    c->s = ZINVALID;
}

static qboolean ZMQ_Queue(zconn_t* c, const void* data, int len) {
    if (c->outLen + len > ZMQ_OUT_SIZE) {
        return qfalse;
    }
    Com_Memcpy(c->out + c->outLen, data, len);
    c->outLen += len;
    return qtrue;
}

static void ZMQ_Flush(zsock_t* zs, zconn_t* c) {
    while (c->outLen > 0) {
        int n = send(c->s, (const char*)c->out, c->outLen, 0);

        if (n <= 0) {
            if (n < 0 && ZWOULDBLOCK) {
                return;
            }
            ZMQ_Drop(zs, c);
            return;
        }
        memmove(c->out, c->out + n, c->outLen - n);
        c->outLen -= n;
    }
}

#define ZF_MORE 0x01
#define ZF_LONG 0x02
#define ZF_COMMAND 0x04

static void ZMQ_SendFrame(zconn_t* c, int flags, const void* body, int len) {
    byte head[9];
    int headLen;

    if (len > 255) {
        int i;

        head[0] = (byte)(flags | ZF_LONG);
        for (i = 0; i < 8; i++) {
            head[1 + i] = (byte)(((unsigned long long)len >> (56 - 8 * i)) & 0xff);
        }
        headLen = 9;
    } else {
        head[0] = (byte)flags;
        head[1] = (byte)len;
        headLen = 2;
    }
    if (!ZMQ_Queue(c, head, headLen) || !ZMQ_Queue(c, body, len)) {
        c->outLen = 0;
    }
}

static void ZMQ_SendCommand(zconn_t* c, const char* name, const void* data, int dataLen) {
    byte body[512];
    int nameLen = (int)strlen(name);

    body[0] = (byte)nameLen;
    Com_Memcpy(body + 1, name, nameLen);
    if (dataLen > 0) {
        Com_Memcpy(body + 1 + nameLen, data, dataLen);
    }
    ZMQ_SendFrame(c, ZF_COMMAND, body, 1 + nameLen + dataLen);
}

static void ZMQ_SendReady(zsock_t* zs, zconn_t* c) {
    const char* type = zs->type == ZT_PUB ? "PUB" : "ROUTER";
    byte meta[64];
    int typeLen = (int)strlen(type);
    int n = 0;

    meta[n++] = 11;
    Com_Memcpy(meta + n, "Socket-Type", 11);
    n += 11;
    meta[n++] = 0;
    meta[n++] = 0;
    meta[n++] = 0;
    meta[n++] = (byte)typeLen;
    Com_Memcpy(meta + n, type, typeLen);
    n += typeLen;
    ZMQ_SendCommand(c, "READY", meta, n);
}

static qboolean ZMQ_Plain(const zsock_t* zs) {
    return zs->password->string[0] != '\0';
}

static void ZMQ_SendGreeting(zsock_t* zs, zconn_t* c) {
    byte g[64];

    Com_Memset(g, 0, sizeof(g));
    g[0] = 0xff;
    g[9] = 0x7f;
    g[10] = 3;
    g[11] = 0;
    Q_strncpyz((char*)g + 12, ZMQ_Plain(zs) ? "PLAIN" : "NULL", 20);
    g[32] = ZMQ_Plain(zs) ? 1 : 0;
    ZMQ_Queue(c, g, sizeof(g));
}

static qboolean ZMQ_CheckPlain(const zsock_t* zs, const byte* data, int len) {
    char user[256];
    char pass[256];
    char key[300];
    char expected[300];
    int userLen, passLen;

    if (len < 1) {
        return qfalse;
    }
    userLen = data[0];
    if (1 + userLen + 1 > len) {
        return qfalse;
    }
    passLen = data[1 + userLen];
    if (1 + userLen + 1 + passLen > len) {
        return qfalse;
    }
    Com_Memcpy(user, data + 1, userLen);
    user[userLen] = '\0';
    Com_Memcpy(pass, data + 2 + userLen, passLen);
    pass[passLen] = '\0';

    Com_sprintf(key, sizeof(key), "%s_%s", zs->domain, user);
    Com_sprintf(expected, sizeof(expected), "%s_%s", zs->domain, zs->domain);
    return (qboolean)(!strcmp(key, expected) && !strcmp(pass, zs->password->string));
}

static zconn_t* zRedirect;

static void ZMQ_RconFlush(char* text) {
    if (zRedirect && text && text[0]) {
        ZMQ_SendFrame(zRedirect, 0, text, (int)strlen(text));
    }
}

static void ZMQ_Message(zsock_t* zs, zconn_t* c, const byte* body, int len, qboolean more) {
    if (zs->type == ZT_PUB) {
        if (len >= 1 && body[0] == 1 && c->numTopics < (int)ARRAY_LEN(c->topics)) {
            int tl = len - 1 < 63 ? len - 1 : 63;

            Com_Memcpy(c->topics[c->numTopics], body + 1, tl);
            c->topics[c->numTopics][tl] = '\0';
            c->numTopics++;
        }
        return;
    }

    if (c->messageLen + len < (int)sizeof(c->message)) {
        Com_Memcpy(c->message + c->messageLen, body, len);
        c->messageLen += len;
    }
    if (more) {
        return;
    }
    c->message[c->messageLen] = '\0';
    c->messageLen = 0;
    if (!c->message[0]) {
        return;
    }
    {
        static char output[16384];
        char cmd[4096];

        Q_strncpyz(cmd, c->message, sizeof(cmd));
        Com_Printf("zmq RCON command from %s: %s\n", c->address, cmd);
        zRedirect = c;
        Com_BeginRedirect(output, sizeof(output), ZMQ_RconFlush);
        Cmd_ExecuteString(cmd);
        Com_EndRedirect();
        zRedirect = NULL;
    }
}

static void ZMQ_Process(zsock_t* zs, zconn_t* c) {
    int pos = 0;

    if (c->state == ZS_GREETING) {
        if (c->inLen < 64) {
            return;
        }
        if (c->in[0] != 0xff || c->in[9] != 0x7f) {
            ZMQ_Drop(zs, c);
            return;
        }
        pos = 64;
        if (ZMQ_Plain(zs)) {
            c->state = ZS_HELLO;
        } else {
            ZMQ_SendReady(zs, c);
            c->state = ZS_READY;
        }
    }

    for (;;) {
        byte flags;
        unsigned long long size;
        int head;
        const byte* body;

        if (c->inLen - pos < 2) {
            break;
        }
        flags = c->in[pos];
        if (flags & ZF_LONG) {
            int i;

            if (c->inLen - pos < 9) {
                break;
            }
            size = 0;
            for (i = 0; i < 8; i++) {
                size = (size << 8) | c->in[pos + 1 + i];
            }
            head = 9;
        } else {
            size = c->in[pos + 1];
            head = 2;
        }
        if (size > ZMQ_IN_SIZE - 16) {
            ZMQ_Drop(zs, c);
            return;
        }
        if ((unsigned long long)(c->inLen - pos - head) < size) {
            break;
        }
        body = c->in + pos + head;
        pos += head + (int)size;

        if (flags & ZF_COMMAND) {
            int nameLen = size ? body[0] : 0;
            const byte* data = body + 1 + nameLen;
            int dataLen = (int)size - 1 - nameLen;

            if (dataLen < 0) {
                ZMQ_Drop(zs, c);
                return;
            }

            if (c->state == ZS_HELLO && nameLen == 5 && !memcmp(body + 1, "HELLO", 5)) {
                if (!ZMQ_CheckPlain(zs, data, dataLen)) {
                    static const char text[] = "invalid user/password";
                    byte reason[64];
                    int textLen = (int)strlen(text);

                    reason[0] = (byte)textLen;
                    Com_Memcpy(reason + 1, text, textLen);
                    ZMQ_SendCommand(c, "ERROR", reason, textLen + 1);
                    ZMQ_Flush(zs, c);
                    ZMQ_Drop(zs, c);
                    return;
                }
                ZMQ_SendCommand(c, "WELCOME", NULL, 0);
                c->state = ZS_INITIATE;
            } else if (c->state == ZS_INITIATE && nameLen == 8 && !memcmp(body + 1, "INITIATE", 8)) {
                ZMQ_SendReady(zs, c);
                c->state = ZS_OPEN;
            } else if (c->state == ZS_READY && nameLen == 5 && !memcmp(body + 1, "READY", 5)) {
                c->state = ZS_OPEN;
            }
            if (c->state == ZS_OPEN && zs == &zRcon) {
                Com_Printf("zmq RCON client connected: %s\n", c->address);
            }
            continue;
        }
        if (c->state == ZS_OPEN) {
            ZMQ_Message(zs, c, body, (int)size, (qboolean)((flags & ZF_MORE) != 0));
        }
    }

    if (pos > 0) {
        memmove(c->in, c->in + pos, c->inLen - pos);
        c->inLen -= pos;
    }
}

static void ZMQ_Poll(zsock_t* zs) {
    int i;

    if (zs->listen == ZINVALID) {
        return;
    }

    for (;;) {
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        zsocket_t s = accept(zs->listen, (struct sockaddr*)&from, &fromLen);
        zconn_t* c = NULL;

        if (s == ZINVALID) {
            break;
        }
        for (i = 0; i < ZMQ_MAX_CONNS; i++) {
            if (zs->conns[i].s == ZINVALID) {
                c = &zs->conns[i];
                break;
            }
        }
        if (!c) {
            zclose(s);
            continue;
        }
        if (zs == &zRcon && !ZMQ_Plain(zs) && ntohl(from.sin_addr.s_addr) != INADDR_LOOPBACK) {
            zclose(s);
            continue;
        }
        Com_Memset(c, 0, sizeof(*c));
        c->s = s;
        c->state = ZS_GREETING;
        {
            char host[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &from.sin_addr, host, sizeof(host));
            Com_sprintf(c->address, sizeof(c->address), "%s:%i", host, ntohs(from.sin_port));
        }
        ZMQ_NonBlocking(s);
        ZMQ_SendGreeting(zs, c);
    }

    for (i = 0; i < ZMQ_MAX_CONNS; i++) {
        zconn_t* c = &zs->conns[i];

        if (c->s == ZINVALID) {
            continue;
        }
        for (;;) {
            int n = recv(c->s, (char*)c->in + c->inLen, ZMQ_IN_SIZE - c->inLen, 0);

            if (n > 0) {
                c->inLen += n;
                ZMQ_Process(zs, c);
                if (c->s == ZINVALID) {
                    break;
                }
                continue;
            }
            if (n == 0 || !ZWOULDBLOCK) {
                ZMQ_Drop(zs, c);
            }
            break;
        }
        if (c->s != ZINVALID) {
            ZMQ_Flush(zs, c);
        }
    }
}

static void ZMQ_InitSocket(zsock_t* zs, ztype_t type, const char* domain, cvar_t* password) {
    int i;

    zs->type = type;
    zs->domain = domain;
    zs->password = password;
    zs->listen = ZINVALID;
    for (i = 0; i < ZMQ_MAX_CONNS; i++) {
        zs->conns[i].s = ZINVALID;
    }
}

void ZMQ_Init(void) {
    char endpoint[128];

    zmq_rcon_enable = Cvar_Get("zmq_rcon_enable", "0", CVAR_INIT);
    zmq_stats_enable = Cvar_Get("zmq_stats_enable", "0", CVAR_INIT);
    zmq_rcon_ip = Cvar_Get("zmq_rcon_ip", "0.0.0.0", CVAR_INIT);
    zmq_rcon_port = Cvar_Get("zmq_rcon_port", "28960", CVAR_INIT);
    zmq_stats_ip = Cvar_Get("zmq_stats_ip", "", CVAR_INIT);
    zmq_stats_port = Cvar_Get("zmq_stats_port", "", CVAR_INIT);
    zmq_stats_password = Cvar_Get("zmq_stats_password", "", CVAR_ARCHIVE);
    zmq_rcon_password = Cvar_Get("zmq_rcon_password", "", CVAR_ARCHIVE);

    ZMQ_InitSocket(&zStats, ZT_PUB, "stats", zmq_stats_password);
    ZMQ_InitSocket(&zRcon, ZT_ROUTER, "rcon", zmq_rcon_password);

    if (zmq_rcon_enable->integer <= 0) {
        return;
    }
    Com_sprintf(endpoint, sizeof(endpoint), "tcp://%s:%i", zmq_rcon_ip->string, zmq_rcon_port->integer);
    if (ZMQ_Bind(&zRcon, zmq_rcon_ip->string, zmq_rcon_port->integer)) {
        Com_Printf("zmq RCON socket: %s\n", endpoint);
        if (!zmq_rcon_password->string[0]) {
            Com_Printf(S_COLOR_YELLOW "zmq RCON has no password: only connections from this machine are "
                       "accepted. Set zmq_rcon_password to use it remotely.\n");
        }
    } else {
        Com_Printf("zmq RCON socket error, bind failed: %s\n", endpoint);
    }
}

void ZMQ_StatsInit(void) {
    char endpoint[128];
    char ip[64];
    int port;

    if (!zmq_stats_enable || zmq_stats_enable->integer <= 0 || zStats.listen != ZINVALID) {
        return;
    }
    Q_strncpyz(ip, zmq_stats_ip->string, sizeof(ip));
    if (!ip[0]) {
        netadr_t adr;

        if (NET_StringToAdr(Cvar_VariableString("net_ip"), &adr, NA_IP)) {
            Com_sprintf(ip, sizeof(ip), "%i.%i.%i.%i", adr.ip[0], adr.ip[1], adr.ip[2], adr.ip[3]);
        } else {
            Q_strncpyz(ip, "0.0.0.0", sizeof(ip));
        }
    }
    port = zmq_stats_port->integer;
    if (!port) {
        port = Cvar_VariableIntegerValue("net_port");
        if (!port) {
            port = PORT_SERVER;
        }
    }
    Com_sprintf(endpoint, sizeof(endpoint), "tcp://%s:%i", ip, port);
    if (ZMQ_Bind(&zStats, ip, port)) {
        Com_Printf("zmq PUB socket: %s\n", endpoint);
    } else {
        Com_Printf("zmq PUB socket error, bind failed: %s\n", endpoint);
    }
}

void ZMQ_Frame(void) {
    ZMQ_Poll(&zRcon);
    ZMQ_Poll(&zStats);
}

void ZMQ_PublishStats(const char* json) {
    int len;
    int i;

    if (zStats.listen == ZINVALID || !json) {
        return;
    }
    len = (int)strlen(json);
    for (i = 0; i < ZMQ_MAX_CONNS; i++) {
        zconn_t* c = &zStats.conns[i];
        int t;
        qboolean wanted = qfalse;

        if (c->s == ZINVALID || c->state != ZS_OPEN) {
            continue;
        }
        for (t = 0; t < c->numTopics; t++) {
            if (!strncmp(json, c->topics[t], strlen(c->topics[t]))) {
                wanted = qtrue;
                break;
            }
        }
        if (!wanted) {
            continue;
        }
        ZMQ_SendFrame(c, 0, json, len);
        ZMQ_Flush(&zStats, c);
    }
}

static void ZMQ_CloseSocket(zsock_t* zs) {
    int i;

    for (i = 0; i < ZMQ_MAX_CONNS; i++) {
        ZMQ_Drop(zs, &zs->conns[i]);
    }
    if (zs->listen != ZINVALID) {
        zclose(zs->listen);
        zs->listen = ZINVALID;
    }
}

void ZMQ_Shutdown(void) {
    ZMQ_CloseSocket(&zStats);
    ZMQ_CloseSocket(&zRcon);
}
