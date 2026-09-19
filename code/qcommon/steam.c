/*
===========================================================================
ioquakelive - Steam integration

Quake Live's own master server is Steam's. Checked in the retail client
(quakelive_steam.exe): it imports steam_api.dll and uses SteamGameServer_Init to
register the server, and SteamMatchmakingServers for the browser. There is no
master host name anywhere in the binary because there does not need to be -- the
address comes from Steam.

What retail does, and what this file replicates:

  0x00466ed0  Steam GS init, called from Com_Init (0x004cc2b3) after the config
              files are executed. It only runs when com_build is 0. It reads
              net_ip ("localhost") and net_port (0x6d38 = 27960), takes sv_vac
              for the server mode ((sv_vac != 0) + 2, so 2 or 3), passes 0 as the
              Steam port and 0xffff as the query port -- which means "share the
              game socket" -- and the version string "1069". Then it sets the
              dedicated flag, logs on (LogOn with sv_setSteamAccount if it is
              set, LogOnAnonymous otherwise), and sets the product name
              "Quake Live" and the mod dir "baseq3".
  0x00465a40  SetMaxPlayerCount, called from SV_ChangeMaxClients (0x004e339e)
              after sv_maxclients is clamped to 64.
  0x00461500  The client's SteamAPI_Init, called from Com_Init (0x004cc16e),
              also only when com_build is 0. It registers +voice and -voice and
              sets the Steam rich presence "status" to "At the main menu".

One call in retail's init is not identified: a vtable slot at +0x9c called with
0. It is not replicated here rather than guessed at.

The library is loaded at run time from the player's own Quake Live installation
(fs_steampath), so this project redistributes no Valve or id file. Without it,
everything here turns into a no-op and the game runs exactly as before.
===========================================================================
*/

#include "q_steam.h"

#ifdef _WIN32
#include <windows.h>
#define STEAM_LIB "steam_api.dll"
#else
#include <dlfcn.h>
#define STEAM_LIB "libsteam_api.so"
#endif

// what retail passes to SteamGameServer_Init
#define STEAM_VERSION_STRING "1069"
#define STEAM_PRODUCT "Quake Live"
#define STEAM_MOD_DIR "baseq3"
#define STEAM_QUERY_PORT_SHARED 0xffff  // MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE
#define STEAM_APP_ID "282440"  // el App ID de Quake Live en Steam

typedef unsigned char steamBool_t;

// Los callbacks de la lista de servidores son metodos virtuales de C++: en Windows la
// convencion es __thiscall (el objeto en ecx), que se imita con __fastcall y un parametro de
// relleno para edx; en el ABI de Linux el objeto es el primer argumento normal.
#ifdef _WIN32
#define STEAM_CB __fastcall
#define STEAM_CB_SELF void* self, void* unusedEdx
#else
#define STEAM_CB
#define STEAM_CB_SELF void* self
#endif

// MatchMakingKeyValuePair_t
typedef struct {
    char key[256];
    char value[256];
} steamKeyValue_t;

// gameserveritem_t, con los offsets que usa el binario de retail (0x00462a50)
#pragma pack(push, 1)
typedef struct {
    unsigned short connectionPort;   //   0
    unsigned short queryPort;        //   2
    unsigned int ip;                 //   4
    int ping;                        //   8
    unsigned char hadResponse;       //  12
    unsigned char doNotRefresh;      //  13
    char gameDir[32];                //  14
    char map[32];                    //  46
    char gameDescription[64];        //  78
    unsigned char pad0[2];           // 142
    unsigned int appId;              // 144
    int players;                     // 148
    int maxPlayers;                  // 152
    int botPlayers;                  // 156
    unsigned char password;          // 160
    unsigned char secure;            // 161
    unsigned char pad1[2];           // 162
    unsigned int lastPlayed;         // 164
    int serverVersion;               // 168
    char serverName[64];             // 172
    char gameTags[128];              // 236
    unsigned long long steamId;      // 364
} steamServerItem_t;                 // 372
#pragma pack(pop)

#define STEAM_MAX_SERVERS 2048

// lo que guardamos de cada servidor que contesta
typedef struct {
    unsigned int ip;
    unsigned short port;
    int ping;
    int players;
    int maxPlayers;
    int bots;
    qboolean password;
    qboolean secure;
    char name[64];
    char map[32];
    char gameDir[32];
    char description[64];
    char tags[128];
} steamServer_t;

// el objeto que Steam ve como ISteamMatchmakingServerListResponse
typedef struct {
    void** vtable;
} steamListResponse_t;

static void Steam_GS_SetGameInfo(void);

typedef struct {
    void* lib;
    void* gameServer;  // ISteamGameServer *
    qboolean clientUp;
    qboolean serverUp;

    steamBool_t(QDECL* GameServer_Init)(unsigned int ip, unsigned short steamPort, unsigned short gamePort,
                                        unsigned short queryPort, int serverMode, const char* version);
    void*(QDECL* GameServer)(void);
    void(QDECL* GameServer_RunCallbacks)(void);
    void(QDECL* GameServer_Shutdown)(void);
    steamBool_t(QDECL* API_Init)(void);
    void(QDECL* API_RunCallbacks)(void);
    void(QDECL* API_Shutdown)(void);

    void(QDECL* SetProduct)(void* self, const char* product);
    void(QDECL* SetModDir)(void* self, const char* modDir);
    void(QDECL* SetGameDescription)(void* self, const char* description);
    void(QDECL* SetDedicatedServer)(void* self, steamBool_t dedicated);
    void(QDECL* LogOn)(void* self, const char* token);
    void(QDECL* LogOnAnonymous)(void* self);
    void(QDECL* EnableHeartbeats)(void* self, steamBool_t active);
    void(QDECL* SetMaxPlayerCount)(void* self, int count);
    void(QDECL* SetBotPlayerCount)(void* self, int count);
    void(QDECL* SetServerName)(void* self, const char* name);
    void(QDECL* SetMapName)(void* self, const char* name);
    void(QDECL* SetPasswordProtected)(void* self, steamBool_t needsPassword);
    steamBool_t(QDECL* BLoggedOn)(void* self);
    steamBool_t(QDECL* BSecure)(void* self);
    unsigned int(QDECL* GetPublicIP)(void* self);
    int(QDECL* HandleIncomingPacket)(void* self, const void* data, int len, unsigned int srcIP,
                                     unsigned short srcPort);
    int(QDECL* GetNextOutgoingPacket)(void* self, void* out, int maxOut, unsigned int* ip, unsigned short* port);
    void(QDECL* SetGameTags)(void* self, const char* tags);

    // la lista de servidores
    void*(QDECL* Matchmaking)(void);
    void*(QDECL* RequestInternetServerList)(void* self, unsigned int appId, steamKeyValue_t** filters,
                                            unsigned int numFilters, void* response);
    void*(QDECL* RequestLANServerList)(void* self, unsigned int appId, void* response);
    void*(QDECL* RequestFriendsServerList)(void* self, unsigned int appId, steamKeyValue_t** filters,
                                           unsigned int numFilters, void* response);
    void*(QDECL* RequestFavoritesServerList)(void* self, unsigned int appId, steamKeyValue_t** filters,
                                             unsigned int numFilters, void* response);
    void*(QDECL* RequestHistoryServerList)(void* self, unsigned int appId, steamKeyValue_t** filters,
                                           unsigned int numFilters, void* response);
    void*(QDECL* GetServerDetails)(void* self, void* request, int server);
    int(QDECL* GetServerCount)(void* self, void* request);
    void(QDECL* ReleaseRequest)(void* self, void* request);
    steamBool_t(QDECL* IsRefreshing)(void* self, void* request);
    void(QDECL* CancelQuery)(void* self, void* request);

    void* matchmaking;   // ISteamMatchmakingServers *
    void* listRequest;   // HServerListRequest
    qboolean listRefreshing;
    steamListResponse_t listResponse;
    int numServers;
    steamServer_t servers[STEAM_MAX_SERVERS];
} steamState_t;

static steamState_t st;

static cvar_t* sv_vac;
static cvar_t* sv_setSteamAccount;

/*
===============
Steam_Bind

Every entry point this file uses, resolved by name. A missing one means the
library is older or newer than the one Quake Live ships, so nothing is used at
all rather than half of it.
===============
*/
static void* Steam_Symbol(const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)st.lib, name);
#else
    return dlsym(st.lib, name);
#endif
}

#define STEAM_BIND_OPT(field, name)                            do {                                                           *(void**)&st.field = Steam_Symbol(name);               } while (0)

#define STEAM_BIND(field, name)                            \
    do {                                                   \
        *(void**)&st.field = Steam_Symbol(name);           \
        if (!st.field) {                                   \
            Com_Printf("Steam: %s is missing\n", name);    \
            return qfalse;                                 \
        }                                                  \
    } while (0)

static qboolean Steam_Bind(void) {
    STEAM_BIND(GameServer_Init, "SteamGameServer_Init");
    STEAM_BIND(GameServer, "SteamGameServer");
    STEAM_BIND(GameServer_RunCallbacks, "SteamGameServer_RunCallbacks");
    STEAM_BIND(GameServer_Shutdown, "SteamGameServer_Shutdown");
    STEAM_BIND(API_Init, "SteamAPI_Init");
    STEAM_BIND(API_RunCallbacks, "SteamAPI_RunCallbacks");
    STEAM_BIND(API_Shutdown, "SteamAPI_Shutdown");
    STEAM_BIND(SetProduct, "SteamAPI_ISteamGameServer_SetProduct");
    STEAM_BIND(SetModDir, "SteamAPI_ISteamGameServer_SetModDir");
    STEAM_BIND(SetGameDescription, "SteamAPI_ISteamGameServer_SetGameDescription");
    STEAM_BIND(SetDedicatedServer, "SteamAPI_ISteamGameServer_SetDedicatedServer");
    STEAM_BIND(LogOn, "SteamAPI_ISteamGameServer_LogOn");
    STEAM_BIND(LogOnAnonymous, "SteamAPI_ISteamGameServer_LogOnAnonymous");
    STEAM_BIND(EnableHeartbeats, "SteamAPI_ISteamGameServer_EnableHeartbeats");
    STEAM_BIND(SetMaxPlayerCount, "SteamAPI_ISteamGameServer_SetMaxPlayerCount");
    STEAM_BIND(SetBotPlayerCount, "SteamAPI_ISteamGameServer_SetBotPlayerCount");
    STEAM_BIND(SetServerName, "SteamAPI_ISteamGameServer_SetServerName");
    STEAM_BIND(SetMapName, "SteamAPI_ISteamGameServer_SetMapName");
    STEAM_BIND(SetPasswordProtected, "SteamAPI_ISteamGameServer_SetPasswordProtected");
    STEAM_BIND(BLoggedOn, "SteamAPI_ISteamGameServer_BLoggedOn");
    STEAM_BIND(BSecure, "SteamAPI_ISteamGameServer_BSecure");
    STEAM_BIND(GetPublicIP, "SteamAPI_ISteamGameServer_GetPublicIP");
    STEAM_BIND(HandleIncomingPacket, "SteamAPI_ISteamGameServer_HandleIncomingPacket");
    STEAM_BIND(GetNextOutgoingPacket, "SteamAPI_ISteamGameServer_GetNextOutgoingPacket");

    // Lo del navegador de servidores es opcional a proposito: si esta biblioteca no lo trae,
    // el resto (el master, las estadisticas, la voz) sigue funcionando igual.
    STEAM_BIND_OPT(SetGameTags, "SteamAPI_ISteamGameServer_SetGameTags");
    STEAM_BIND_OPT(Matchmaking, "SteamMatchmakingServers");
    STEAM_BIND_OPT(RequestInternetServerList, "SteamAPI_ISteamMatchmakingServers_RequestInternetServerList");
    STEAM_BIND_OPT(RequestLANServerList, "SteamAPI_ISteamMatchmakingServers_RequestLANServerList");
    STEAM_BIND_OPT(RequestFriendsServerList, "SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList");
    STEAM_BIND_OPT(RequestFavoritesServerList, "SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList");
    STEAM_BIND_OPT(RequestHistoryServerList, "SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList");
    STEAM_BIND_OPT(GetServerDetails, "SteamAPI_ISteamMatchmakingServers_GetServerDetails");
    STEAM_BIND_OPT(GetServerCount, "SteamAPI_ISteamMatchmakingServers_GetServerCount");
    STEAM_BIND_OPT(ReleaseRequest, "SteamAPI_ISteamMatchmakingServers_ReleaseRequest");
    STEAM_BIND_OPT(IsRefreshing, "SteamAPI_ISteamMatchmakingServers_IsRefreshing");
    STEAM_BIND_OPT(CancelQuery, "SteamAPI_ISteamMatchmakingServers_CancelQuery");
    return qtrue;
}

/*
===============
Steam_Load

The library comes from the player's own Quake Live installation, which the file
system already located for pak00.pk3 (fs_steampath). Next to our own executable
first, so a player who drops one there wins.
===============
*/
static qboolean Steam_Load(void) {
    const char* steamPath;
    char path[MAX_OSPATH];

    if (st.lib) {
        return qtrue;
    }

    // Retail lo lanza Steam, que le pasa el App ID por el entorno; a nosotros nos lanza el
    // launcher, asi que se lo decimos nosotros. Sin esto la API no sabe de que juego se trata y
    // tanto SteamAPI_Init como SteamGameServer_Init fallan.
#ifdef _WIN32
    SetEnvironmentVariableA("SteamAppId", STEAM_APP_ID);
    SetEnvironmentVariableA("SteamGameId", STEAM_APP_ID);
#else
    setenv("SteamAppId", STEAM_APP_ID, 1);
    setenv("SteamGameId", STEAM_APP_ID, 1);
#endif
#ifdef _WIN32
    st.lib = (void*)LoadLibraryA(STEAM_LIB);
#else
    st.lib = dlopen("./" STEAM_LIB, RTLD_NOW);
#endif
    if (!st.lib) {
        steamPath = Cvar_VariableString("fs_steampath");
        if (steamPath[0]) {
            Com_sprintf(path, sizeof(path), "%s%c%s", steamPath, PATH_SEP, STEAM_LIB);
#ifdef _WIN32
            st.lib = (void*)LoadLibraryA(path);
#else
            st.lib = dlopen(path, RTLD_NOW);
#endif
        }
    }
    if (!st.lib) {
        Com_Printf("Steam: %s not found; running without Steam\n", STEAM_LIB);
        return qfalse;
    }
    if (!Steam_Bind()) {
        st.lib = NULL;
        return qfalse;
    }
    return qtrue;
}

/*
===============
Steam_Init

[QL] 0x00461500, called from Com_Init at 0x004cc16e. Retail skips all of this
when com_build is set, which is how a build with no Steam behind it runs.
===============
*/
/*
===============
Steam_Status_f

[ioquakelive] Diagnostico propio, no de retail: dice si la biblioteca cargo, si el cliente y el
servidor arrancaron, si la sesion con Steam esta iniciada, si VAC esta activo y con que IP
publica quedo registrado el servidor.
===============
*/
static void Steam_Status_f(void) {
    unsigned int ip;

    Com_Printf("biblioteca:  %s\n", st.lib ? "cargada" : "no");
    Com_Printf("cliente:     %s\n", st.clientUp ? "iniciado" : "no");
    Com_Printf("servidor:    %s\n", st.serverUp ? "iniciado" : "no");
    if (!st.serverUp) {
        return;
    }
    Com_Printf("sesion:      %s\n", st.BLoggedOn(st.gameServer) ? "iniciada en Steam" : "sin iniciar");
    Com_Printf("VAC:         %s\n", st.BSecure(st.gameServer) ? "si" : "no");
    ip = st.GetPublicIP(st.gameServer);
    Com_Printf("IP publica:  %u.%u.%u.%u\n", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff,
               ip & 0xff);
}

void Steam_Init(void) {
    if (Cvar_VariableIntegerValue("com_build")) {
        return;
    }
    if (!Steam_Load()) {
        return;
    }
    if (!st.API_Init()) {
        Com_Printf("Steam API not present.\n");
        return;
    }
    st.clientUp = qtrue;
    if (st.Matchmaking) {
        st.matchmaking = st.Matchmaking();  // la interfaz de la lista de servidores
    }
    Cmd_AddCommand("steamservers", Steam_ServerList_f);
    Cmd_AddCommand("steamstatus", Steam_Status_f);
    Com_Printf("Steam API initialized.\n");
}

/*
===============
Steam_GS_Init

[QL] 0x00466ed0, called from Com_Init at 0x004cc2b3.
===============
*/
void Steam_GS_Init(void) {
    const char* account;
    netadr_t adr;
    unsigned int ip = 0;
    int mode;
    int port;

    if (Cvar_VariableIntegerValue("com_build")) {
        return;
    }
    if (st.serverUp || !Steam_Load()) {
        return;
    }

    sv_vac = Cvar_Get("sv_vac", "0", CVAR_ARCHIVE);
    sv_setSteamAccount = Cvar_Get("sv_setSteamAccount", "", CVAR_ARCHIVE);

    // retail reads net_ip and net_port here and hands Steam the address it binds
    if (NET_StringToAdr(Cvar_VariableString("net_ip"), &adr, NA_IP)) {
        ip = (adr.ip[0] << 24) | (adr.ip[1] << 16) | (adr.ip[2] << 8) | adr.ip[3];
    }
    port = Cvar_VariableIntegerValue("net_port");
    if (!port) {
        port = PORT_SERVER;
    }

    // (sv_vac != 0) + 2: eServerModeAuthentication or eServerModeAuthenticationAndSecure
    mode = (sv_vac->integer != 0) + 2;

    if (!st.GameServer_Init((unsigned int)ip, 0, (unsigned short)port, STEAM_QUERY_PORT_SHARED, mode,
                            STEAM_VERSION_STRING)) {
        Com_Printf("Failed to initialize Steam GS API.\n");
        return;
    }
    st.gameServer = st.GameServer();
    if (!st.gameServer) {
        Com_Printf("Failed to initialize Steam GS API.\n");
        return;
    }
    st.serverUp = qtrue;
    if (!st.clientUp) {
        Cmd_AddCommand("steamstatus", Steam_Status_f);  // el cliente ya lo agrego si arranco
    }

    st.SetDedicatedServer(st.gameServer, (steamBool_t)(Cvar_VariableIntegerValue("dedicated") > 0));

    account = Cvar_VariableString("sv_setSteamAccount");
    if (account[0]) {
        st.LogOn(st.gameServer, account);
    } else {
        st.LogOnAnonymous(st.gameServer);
    }

    st.SetProduct(st.gameServer, STEAM_PRODUCT);
    st.SetModDir(st.gameServer, STEAM_MOD_DIR);
    st.SetGameDescription(st.gameServer, STEAM_PRODUCT);
    st.EnableHeartbeats(st.gameServer, 1);
    Com_Printf("Steam Gameserver initialized.\n");
}

/*
===============
Steam_GS_SetMaxPlayers

[QL] 0x00465a40, called from SV_ChangeMaxClients at 0x004e339e.
===============
*/
void Steam_GS_SetMaxPlayers(int maxPlayers) {
    if (!st.serverUp) {
        return;
    }
    st.SetMaxPlayerCount(st.gameServer, maxPlayers);
}

void Steam_GS_SetServerInfo(const char* hostname, const char* mapName, qboolean needPassword, int botCount) {
    if (!st.serverUp) {
        return;
    }
    if (hostname && hostname[0]) {
        st.SetServerName(st.gameServer, hostname);
    }
    if (mapName && mapName[0]) {
        st.SetMapName(st.gameServer, mapName);
    }
    st.SetPasswordProtected(st.gameServer, (steamBool_t)(needPassword != qfalse));
    st.SetBotPlayerCount(st.gameServer, botCount);
    Steam_GS_SetGameInfo();
}

/*
===============
Steam_GS_HandlePacket

The query port is 0xffff, which tells Steam to share the game's own socket: the
queries from the master arrive on our port and have to be handed over. Steam
answers them through GetNextOutgoingPacket, which Steam_Frame sends.

Returns qtrue when the packet belonged to Steam, so the caller drops it.
===============
*/
qboolean Steam_GS_HandlePacket(netadr_t from, const void* data, int len) {
    unsigned int ip;

    if (!st.serverUp) {
        return qfalse;
    }
    if (from.type != NA_IP) {
        return qfalse;
    }
    ip = (from.ip[0] << 24) | (from.ip[1] << 16) | (from.ip[2] << 8) | from.ip[3];
    return st.HandleIncomingPacket(st.gameServer, data, len, ip, BigShort(from.port)) ? qtrue : qfalse;
}

void Steam_Frame(void) {
    if (st.clientUp) {
        st.API_RunCallbacks();
    }
    if (!st.serverUp) {
        return;
    }
    st.GameServer_RunCallbacks();

    // whatever Steam wants to answer goes out on our own socket
    for (;;) {
        static byte packet[1400];
        unsigned int ip = 0;
        unsigned short port = 0;
        int len = st.GetNextOutgoingPacket(st.gameServer, packet, sizeof(packet), &ip, &port);
        netadr_t to;

        if (len <= 0) {
            break;
        }
        Com_Memset(&to, 0, sizeof(to));
        to.type = NA_IP;
        to.ip[0] = (byte)(ip >> 24);
        to.ip[1] = (byte)(ip >> 16);
        to.ip[2] = (byte)(ip >> 8);
        to.ip[3] = (byte)ip;
        to.port = BigShort(port);
        NET_SendPacket(NS_SERVER, len, packet, to);
    }
}

qboolean Steam_GS_Running(void) {
    return st.serverUp;
}

void Steam_Shutdown(void) {
    if (st.serverUp) {
        st.EnableHeartbeats(st.gameServer, 0);
        st.GameServer_Shutdown();
        st.serverUp = qfalse;
        st.gameServer = NULL;
    }
    if (st.clientUp) {
        st.API_Shutdown();
        st.clientUp = qfalse;
    }
}

/*
===============================================================================

LA LISTA DE SERVIDORES

Quake Live no tiene master propio ni protocolo de navegador: el cliente le pide
la lista a Steam. En el binario de retail estan las dos mitades.

El servidor publica lo suyo (0x00466260): la descripcion del juego es el nombre
largo del tipo de juego ("Free For All", "Clan Arena", ...) y las etiquetas son
el nombre corto mas una palabra por cada regla que no es la de siempre:

    <corto>,[cheats,][instagib,][lowgrav,|highgrav,][vampiric,][infected,]
    [quadhog,][sv_tags]

El cliente pide la lista (0x00462eb0) con un solo filtro, "gamedir" = "baseq3",
segun de donde la quiera: internet, LAN, amigos, favoritos o historial. Cada
servidor que contesta se arma (0x00462a50) con lo que Steam devuelve en
gameserveritem_t: nombre, mapa, jugadores, bots, ping, contraseña, VAC, las
etiquetas y la descripcion.

===============================================================================
*/

// [QL] 0x00560e50: el nombre largo va en la descripcion del juego
static const char* steamGametypeNames[] = {
    "Free For All", "Duel", "Race", "Team Deathmatch", "Clan Arena", "Capture the Flag",
    "One Flag CTF", "Overload", "Harvester", "Freeze Tag", "Domination", "Attack and Defend",
    "Red Rover"
};

// [QL] 0x00560e84: el nombre corto abre las etiquetas
static const char* steamGametypeTags[] = {
    "ffa", "duel", "race", "tdm", "clanarena", "ctf", "oneflag", "overload", "harvester",
    "freezetag", "domination", "a&d", "redrover"
};

#define STEAM_NUM_GAMETYPES ((int)ARRAY_LEN(steamGametypeNames))

/*
===============
Steam_GS_SetGameInfo

[QL] 0x00466260. Lo que el navegador muestra de este servidor, aparte del nombre
y el mapa: de que se juega y con que reglas.
===============
*/
static void Steam_GS_SetGameInfo(void) {
    int gametype;
    float gravity;
    char tags[256];
    const char* extra;

    if (!st.serverUp || !st.SetGameTags) {
        return;
    }

    gametype = Cvar_VariableIntegerValue("g_gametype");
    if (gametype < 0 || gametype >= STEAM_NUM_GAMETYPES) {
        st.SetGameDescription(st.gameServer, "Unknown Gametype");
        Q_strncpyz(tags, "", sizeof(tags));
    } else {
        st.SetGameDescription(st.gameServer, steamGametypeNames[gametype]);
        Com_sprintf(tags, sizeof(tags), "%s,", steamGametypeTags[gametype]);
    }

    if (Cvar_VariableIntegerValue("sv_cheats")) {
        Q_strcat(tags, sizeof(tags), "cheats,");
    }
    if (Cvar_VariableIntegerValue("g_instagib")) {
        Q_strcat(tags, sizeof(tags), "instagib,");
    }
    gravity = Cvar_VariableValue("g_gravity");
    if (gravity < 800.0f) {
        Q_strcat(tags, sizeof(tags), "lowgrav,");
    } else if (gravity > 800.0f) {
        Q_strcat(tags, sizeof(tags), "highgrav,");
    }
    if (Cvar_VariableValue("g_vampiricDamage") > 0.0f) {
        Q_strcat(tags, sizeof(tags), "vampiric,");
    }
    // infectados solo cuenta en red rover, y el quad hog solo en free for all
    if (Cvar_VariableIntegerValue("g_rrInfected") && gametype == 12) {
        Q_strcat(tags, sizeof(tags), "infected,");
    }
    if (Cvar_VariableIntegerValue("g_quadHog") && gametype == 0) {
        Q_strcat(tags, sizeof(tags), "quadhog,");
    }
    extra = Cvar_VariableString("sv_tags");
    if (extra[0]) {
        Q_strcat(tags, sizeof(tags), extra);
        if (tags[strlen(tags) - 1] != ',') {
            Q_strcat(tags, sizeof(tags), ",");
        }
    }

    // la coma del final sobra
    if (tags[0] && tags[strlen(tags) - 1] == ',') {
        tags[strlen(tags) - 1] = '\0';
    }
    st.SetGameTags(st.gameServer, tags);
}

/*
===============
Steam_ServerListReset
===============
*/
static void Steam_ServerListReset(void) {
    if (st.listRequest && st.ReleaseRequest) {
        st.ReleaseRequest(st.matchmaking, st.listRequest);
    }
    st.listRequest = NULL;
    st.numServers = 0;
    st.listRefreshing = qfalse;
}

// lo que Steam contesta de un servidor, pasado a lo que el navegador necesita
static void Steam_ServerListAdd(void* request, int index) {
    steamServerItem_t* item;
    steamServer_t* out;

    if (st.numServers >= STEAM_MAX_SERVERS) {
        return;
    }
    item = (steamServerItem_t*)st.GetServerDetails(st.matchmaking, request, index);
    if (!item || !item->hadResponse) {
        return;
    }

    out = &st.servers[st.numServers++];
    out->ip = item->ip;
    out->port = item->connectionPort;
    out->ping = item->ping;
    out->players = item->players;
    out->maxPlayers = item->maxPlayers;
    out->bots = item->botPlayers;
    out->password = (qboolean)(item->password != 0);
    out->secure = (qboolean)(item->secure != 0);
    Q_strncpyz(out->name, item->serverName, sizeof(out->name));
    Q_strncpyz(out->map, item->map, sizeof(out->map));
    Q_strncpyz(out->gameDir, item->gameDir, sizeof(out->gameDir));
    Q_strncpyz(out->description, item->gameDescription, sizeof(out->description));
    Q_strncpyz(out->tags, item->gameTags, sizeof(out->tags));
}

static void STEAM_CB Steam_CB_ServerResponded(STEAM_CB_SELF, void* request, int index) {
    Steam_ServerListAdd(request, index);
}

static void STEAM_CB Steam_CB_ServerFailedToRespond(STEAM_CB_SELF, void* request, int index) {
    // retail tampoco muestra al que no contesta
    (void)request;
    (void)index;
}

static void STEAM_CB Steam_CB_RefreshComplete(STEAM_CB_SELF, void* request, int response) {
    (void)request;
    st.listRefreshing = qfalse;
    Com_DPrintf("Steam: la lista termino con %i servidores (respuesta %i)\n", st.numServers, response);
}

static void* steamListVTable[3];

/*
===============
Steam_RequestServerList

[QL] 0x00462eb0. source: 0 internet, 1 LAN, 2 amigos, 3 favoritos, 4 historial.
===============
*/
qboolean Steam_RequestServerList(int source) {
    steamKeyValue_t filter;
    steamKeyValue_t* filters[1];
    unsigned int appId = (unsigned int)atoi(STEAM_APP_ID);

    if (!st.clientUp || !st.matchmaking || !st.RequestInternetServerList) {
        return qfalse;
    }

    Steam_ServerListReset();

    steamListVTable[0] = (void*)Steam_CB_ServerResponded;
    steamListVTable[1] = (void*)Steam_CB_ServerFailedToRespond;
    steamListVTable[2] = (void*)Steam_CB_RefreshComplete;
    st.listResponse.vtable = steamListVTable;

    // el unico filtro de retail: que sea Quake Live y no otro juego del mismo App ID
    Com_Memset(&filter, 0, sizeof(filter));
    Q_strncpyz(filter.key, "gamedir", sizeof(filter.key));
    Q_strncpyz(filter.value, STEAM_MOD_DIR, sizeof(filter.value));
    filters[0] = &filter;

    st.listRefreshing = qtrue;
    switch (source) {
        case 1:
            st.listRequest = st.RequestLANServerList(st.matchmaking, appId, &st.listResponse);
            break;
        case 2:
            st.listRequest = st.RequestFriendsServerList(st.matchmaking, appId, filters, 1, &st.listResponse);
            break;
        case 3:
            st.listRequest = st.RequestFavoritesServerList(st.matchmaking, appId, filters, 1, &st.listResponse);
            break;
        case 4:
            st.listRequest = st.RequestHistoryServerList(st.matchmaking, appId, filters, 1, &st.listResponse);
            break;
        default:
            st.listRequest = st.RequestInternetServerList(st.matchmaking, appId, filters, 1, &st.listResponse);
            break;
    }
    if (!st.listRequest) {
        st.listRefreshing = qfalse;
        return qfalse;
    }
    return qtrue;
}

void Steam_ServerList_f(void) {
    int cuenta = -1;
    int refrescando = -1;

    if (st.matchmaking && st.listRequest) {
        if (st.GetServerCount) {
            cuenta = st.GetServerCount(st.matchmaking, st.listRequest);
        }
        if (st.IsRefreshing) {
            refrescando = st.IsRefreshing(st.matchmaking, st.listRequest) ? 1 : 0;
        }
    }
    Com_Printf("Steam: pedido %p, nuestros %i, de Steam %i, refrescando %i (bandera %i)\n",
               st.listRequest, st.numServers, cuenta, refrescando, st.listRefreshing);
    for (cuenta = 0; cuenta < st.numServers && cuenta < 5; cuenta++) {
        const steamServer_t* sv = &st.servers[cuenta];

        Com_Printf("  %-28s %-12s %2i/%-2i bots %i ping %3i  %s | %s | %i.%i.%i.%i:%i\n",
                   sv->name, sv->map, sv->players, sv->maxPlayers, sv->bots, sv->ping,
                   sv->description, sv->tags, (sv->ip >> 24) & 0xff, (sv->ip >> 16) & 0xff,
                   (sv->ip >> 8) & 0xff, sv->ip & 0xff, sv->port);
    }
}

qboolean Steam_ServerListRefreshing(void) {
    return st.listRefreshing;
}

int Steam_ServerListCount(void) {
    return st.numServers;
}

/*
===============
Steam_ServerListGet

Devuelve un servidor de la lista como lo espera el navegador: la direccion, el
ping que midio Steam y una cadena de info igual a la que contestaria el propio
servidor. El tipo de juego sale de la primera etiqueta, que es como lo publica
(0x00466260); si no se reconoce, queda en cero.
===============
*/
qboolean Steam_ServerListGet(int index, unsigned int* ip, unsigned short* port, int* ping, char* info,
                             int infoSize) {
    const steamServer_t* s;
    char firstTag[32];
    const char* comma;
    int gametype;
    int i;

    if (index < 0 || index >= st.numServers) {
        return qfalse;
    }
    s = &st.servers[index];
    *ip = s->ip;
    *port = s->port;
    *ping = s->ping;

    Q_strncpyz(firstTag, s->tags, sizeof(firstTag));
    comma = strchr(firstTag, ',');
    if (comma) {
        firstTag[comma - firstTag] = '\0';
    }
    gametype = 0;
    for (i = 0; i < STEAM_NUM_GAMETYPES; i++) {
        if (!Q_stricmp(firstTag, steamGametypeTags[i])) {
            gametype = i;
            break;
        }
    }

    Com_sprintf(info, infoSize,
                "\\hostname\\%s\\mapname\\%s\\clients\\%i\\g_humanplayers\\%i\\sv_maxclients\\%i"
                "\\game\\%s\\gametype\\%i\\g_needpass\\%i\\nettype\\1\\sv_vac\\%i\\tags\\%s",
                s->name, s->map, s->players, s->players - s->bots, s->maxPlayers, s->gameDir,
                gametype, s->password ? 1 : 0, s->secure ? 1 : 0, s->tags);
    return qtrue;
}
