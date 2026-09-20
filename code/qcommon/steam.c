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
static void Steam_DownloadUGC_f(void);
static void Steam_P2PListen(qboolean asServer);

/*
Una devolucion de llamada de Steam vista desde C. En el SDK es una clase con
tres funciones virtuales, y el binario de retail deja ver como las ordena el
compilador: en 0x00532f1c estan Run(dato, fallo, llamada) primero, Run(dato)
segunda y GetCallbackSizeBytes() tercera -- al reves de como se declaran, que
es lo que hace MSVC con las sobrecargas.

Los campos son los de CCallbackBase: la tabla, las banderas (0x02 dice que la
devolucion es del servidor y no del cliente) y el numero de la devolucion.
*/
typedef struct {
    void** vtable;
    unsigned char flags;
    unsigned char pad[3];
    int id;
} steamCallback_t;

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

    void*(QDECL* UGC)(void);
    unsigned int(QDECL* GetItemState)(void* self, uint64_t item);
    steamBool_t(QDECL* DownloadItem)(void* self, uint64_t item, steamBool_t highPriority);
    steamBool_t(QDECL* GetItemInstallInfo)(void* self, uint64_t item, uint64_t* sizeOnDisk, char* folder,
                                           unsigned int folderSize, unsigned int* timestamp);
    steamBool_t(QDECL* GetItemDownloadInfo)(void* self, uint64_t item, uint64_t* downloaded, uint64_t* total);
    unsigned int(QDECL* GetNumSubscribedItems)(void* self);
    unsigned int(QDECL* GetSubscribedItems)(void* self, uint64_t* items, unsigned int max);
    uint64_t(QDECL* GetSteamID)(void* self);

    // la voz y la red entre pares
    void*(QDECL* Networking)(void);
    void*(QDECL* GameServerNetworking)(void);
    void*(QDECL* User)(void);
    void*(QDECL* Friends)(void);
    steamBool_t(QDECL* SendP2PPacket)(void* self, uint64_t steamId, const void* data, unsigned int size,
                                      int sendType, int channel);
    steamBool_t(QDECL* IsP2PPacketAvailable)(void* self, unsigned int* size, int channel);
    steamBool_t(QDECL* ReadP2PPacket)(void* self, void* dest, unsigned int destSize, unsigned int* size,
                                      uint64_t* from, int channel);
    steamBool_t(QDECL* AcceptP2PSessionWithUser)(void* self, uint64_t steamId);
    void(QDECL* StartVoiceRecording)(void* self);
    void(QDECL* StopVoiceRecording)(void* self);
    int(QDECL* GetVoice)(void* self, steamBool_t wantCompressed, void* dest, unsigned int destSize,
                         unsigned int* written, steamBool_t wantUncompressed, void* uncompressedDest,
                         unsigned int uncompressedSize, unsigned int* uncompressedWritten,
                         unsigned int sampleRate);
    int(QDECL* DecompressVoice)(void* self, const void* compressed, unsigned int compressedSize, void* dest,
                                unsigned int destSize, unsigned int* written, unsigned int sampleRate);
    unsigned int(QDECL* GetVoiceOptimalSampleRate)(void* self);
    uint64_t(QDECL* GetUserSteamID)(void* self);
    void(QDECL* SetInGameVoiceSpeaking)(void* self, uint64_t steamId, steamBool_t speaking);
    void(QDECL* SetRichPresence)(void* self, const char* key, const char* value);
    void(QDECL* ActivateGameOverlayToUser)(void* self, const char* dialog, uint64_t steamId);
    void(QDECL* ActivateGameOverlay)(void* self, const char* dialog);
    int(QDECL* GetFriendCount)(void* self, int flags);
    uint64_t(QDECL* GetFriendByIndex)(void* self, int index, int flags);
    const char*(QDECL* GetFriendPersonaName)(void* self, uint64_t steamId);
    int(QDECL* GetFriendPersonaState)(void* self, uint64_t steamId);
    steamBool_t(QDECL* GetFriendGamePlayed)(void* self, uint64_t steamId, void* info);
    void(QDECL* RegisterCallback)(void* callback, int id);
    void(QDECL* UnregisterCallback)(void* callback);

    void* networking;            // ISteamNetworking * (cliente)
    void* gameServerNetworking;  // ISteamNetworking * (servidor)
    void* user;                  // ISteamUser *
    void* friends;               // ISteamFriends *
    qboolean voiceRecording;
    steamCallback_t p2pClient;   // la sesion que pide el servidor
    steamCallback_t p2pServer;   // la sesion que pide cada jugador

    void* ugc;           // ISteamUGC *
    uint64_t downloading;  // el item que se esta esperando, o 0

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
    STEAM_BIND_OPT(UGC, "SteamUGC");
    STEAM_BIND_OPT(Networking, "SteamNetworking");
    STEAM_BIND_OPT(GameServerNetworking, "SteamGameServerNetworking");
    STEAM_BIND_OPT(User, "SteamUser");
    STEAM_BIND_OPT(Friends, "SteamFriends");
    STEAM_BIND_OPT(SendP2PPacket, "SteamAPI_ISteamNetworking_SendP2PPacket");
    STEAM_BIND_OPT(IsP2PPacketAvailable, "SteamAPI_ISteamNetworking_IsP2PPacketAvailable");
    STEAM_BIND_OPT(ReadP2PPacket, "SteamAPI_ISteamNetworking_ReadP2PPacket");
    STEAM_BIND_OPT(AcceptP2PSessionWithUser, "SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser");
    STEAM_BIND_OPT(StartVoiceRecording, "SteamAPI_ISteamUser_StartVoiceRecording");
    STEAM_BIND_OPT(StopVoiceRecording, "SteamAPI_ISteamUser_StopVoiceRecording");
    STEAM_BIND_OPT(GetVoice, "SteamAPI_ISteamUser_GetVoice");
    STEAM_BIND_OPT(DecompressVoice, "SteamAPI_ISteamUser_DecompressVoice");
    STEAM_BIND_OPT(GetVoiceOptimalSampleRate, "SteamAPI_ISteamUser_GetVoiceOptimalSampleRate");
    STEAM_BIND_OPT(GetUserSteamID, "SteamAPI_ISteamUser_GetSteamID");
    STEAM_BIND_OPT(SetInGameVoiceSpeaking, "SteamAPI_ISteamFriends_SetInGameVoiceSpeaking");
    STEAM_BIND_OPT(SetRichPresence, "SteamAPI_ISteamFriends_SetRichPresence");
    STEAM_BIND_OPT(ActivateGameOverlayToUser,
                   "SteamAPI_ISteamFriends_ActivateGameOverlayToUser");
    STEAM_BIND_OPT(ActivateGameOverlay, "SteamAPI_ISteamFriends_ActivateGameOverlay");
    STEAM_BIND_OPT(GetFriendCount, "SteamAPI_ISteamFriends_GetFriendCount");
    STEAM_BIND_OPT(GetFriendByIndex, "SteamAPI_ISteamFriends_GetFriendByIndex");
    STEAM_BIND_OPT(GetFriendPersonaName, "SteamAPI_ISteamFriends_GetFriendPersonaName");
    STEAM_BIND_OPT(GetFriendPersonaState, "SteamAPI_ISteamFriends_GetFriendPersonaState");
    STEAM_BIND_OPT(GetFriendGamePlayed, "SteamAPI_ISteamFriends_GetFriendGamePlayed");
    STEAM_BIND_OPT(RegisterCallback, "SteamAPI_RegisterCallback");
    STEAM_BIND_OPT(UnregisterCallback, "SteamAPI_UnregisterCallback");
    STEAM_BIND_OPT(GetItemState, "SteamAPI_ISteamUGC_GetItemState");
    STEAM_BIND_OPT(DownloadItem, "SteamAPI_ISteamUGC_DownloadItem");
    STEAM_BIND_OPT(GetItemInstallInfo, "SteamAPI_ISteamUGC_GetItemInstallInfo");
    STEAM_BIND_OPT(GetItemDownloadInfo, "SteamAPI_ISteamUGC_GetItemDownloadInfo");
    STEAM_BIND_OPT(GetNumSubscribedItems, "SteamAPI_ISteamUGC_GetNumSubscribedItems");
    STEAM_BIND_OPT(GetSubscribedItems, "SteamAPI_ISteamUGC_GetSubscribedItems");
    STEAM_BIND_OPT(GetSteamID, "SteamAPI_ISteamGameServer_GetSteamID");
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
    if (st.UGC) {
        st.ugc = st.UGC();  // la del Workshop, de donde salen los mapas
    }
    if (st.Networking) {
        st.networking = st.Networking();  // la red entre pares, por donde va la voz
    }
    if (st.User) {
        st.user = st.User();
    }
    if (st.Friends) {
        st.friends = st.Friends();
    }
    Steam_P2PListen(qfalse);
    // [QL] 0x00461565: "+voice" y "-voice" se registran del lado del cliente, que es el que
    // ademas tiene que avisarle al cgame que uno esta hablando
    Steam_SetRichPresence("At the main menu");    // [QL] 0x00461556
    Cmd_AddCommand("steam_downloadugc", Steam_DownloadUGC_f);
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

    sv_vac = Cvar_Get("sv_vac", "1", CVAR_INIT);  // [QL] asi lo registra retail
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
    if (st.GameServerNetworking) {
        st.gameServerNetworking = st.GameServerNetworking();  // por donde reparte la voz
        Steam_P2PListen(qtrue);
    }
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
    if (st.UnregisterCallback) {
        if (st.p2pServer.vtable) {
            st.UnregisterCallback(&st.p2pServer);
            st.p2pServer.vtable = NULL;
        }
        if (st.p2pClient.vtable) {
            st.UnregisterCallback(&st.p2pClient);
            st.p2pClient.vtable = NULL;
        }
    }
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

/*
===============================================================================

EL WORKSHOP

Quake Live no baja mapas por HTTP: los baja del Workshop de Steam. El servidor
publica los items que usa la partida (0x00465b00) en el configstring
CS_STEAM_WORKSHOP_IDS y en la cvar sv_referencedSteamworks, y el cliente, antes
de entrar, se baja los que no tiene (0x004bba30) mostrando el avance en
cl_downloadName / cl_downloadItem / cl_downloadTime.

Retail se entera de que termino por el callback OnDownloadItemResult
(0x00469630). Aca se pregunta por el estado del item cada cuadro, que da lo
mismo de afuera y evita registrar callbacks de C++ a mano.

Las banderas de estado son las de Steam: 1 suscrito, 4 instalado, 8 necesita
actualizarse, 16 bajando, 32 en cola.

===============================================================================
*/

#define STEAM_ITEM_INSTALLED 4
#define STEAM_ITEM_NEEDS_UPDATE 8
#define STEAM_ITEM_DOWNLOADING 16
#define STEAM_ITEM_PENDING 32

qboolean Steam_WorkshopReady(void) {
    return (qboolean)(st.ugc != NULL && st.GetItemState != NULL && st.DownloadItem != NULL);
}

unsigned int Steam_WorkshopState(uint64_t item) {
    if (!Steam_WorkshopReady()) {
        return 0;
    }
    return st.GetItemState(st.ugc, item);
}

qboolean Steam_WorkshopInstalled(uint64_t item) {
    unsigned int state = Steam_WorkshopState(item);

    return (qboolean)((state & STEAM_ITEM_INSTALLED) && !(state & STEAM_ITEM_NEEDS_UPDATE));
}

/*
===============
Steam_WorkshopDownload

[QL] 0x004699c0. Devuelve qtrue si hay que esperar: el que ya esta instalado no
se pide de nuevo.
===============
*/
qboolean Steam_WorkshopDownload(uint64_t item) {
    if (!Steam_WorkshopReady() || !item) {
        return qfalse;
    }
    if (Steam_WorkshopInstalled(item)) {
        Com_Printf("Workshop item %llu: in cache.\n", (unsigned long long)item);
        return qfalse;
    }
    Com_Printf("Workshop item %llu: requesting download.\n", (unsigned long long)item);
    Com_DPrintf("Workshop item %llu: estado %u antes de pedirlo.\n", (unsigned long long)item,
                Steam_WorkshopState(item));
    if (!st.DownloadItem(st.ugc, item, 1)) {
        Com_Printf("Workshop item %llu: Steam did not take the request.\n", (unsigned long long)item);
        return qfalse;
    }
    st.downloading = item;
    return qtrue;
}

// lo que Steam lleva bajado del item que se esta esperando
qboolean Steam_WorkshopProgress(uint64_t* done, uint64_t* total) {
    *done = 0;
    *total = 0;
    if (!Steam_WorkshopReady() || !st.downloading || !st.GetItemDownloadInfo) {
        return qfalse;
    }
    return st.GetItemDownloadInfo(st.ugc, st.downloading, done, total) ? qtrue : qfalse;
}

// qtrue mientras el item que se pidio siga sin estar
qboolean Steam_WorkshopBusy(void) {
    unsigned int state;

    if (!st.downloading) {
        return qfalse;
    }
    state = Steam_WorkshopState(st.downloading);
    if (state & (STEAM_ITEM_DOWNLOADING | STEAM_ITEM_PENDING)) {
        return qtrue;
    }
    if (Steam_WorkshopInstalled(st.downloading)) {
        Com_Printf("Steamworks download complete: %llu\n", (unsigned long long)st.downloading);
        st.downloading = 0;
        return qfalse;
    }
    // ni bajando ni instalado: Steam lo descarto
    Com_Printf("Download item %llu failed\n", (unsigned long long)st.downloading);
    st.downloading = 0;
    return qfalse;
}

// donde quedo instalado un item, para montarlo
qboolean Steam_WorkshopPath(uint64_t item, char* buf, int size) {
    uint64_t sizeOnDisk = 0;
    unsigned int stamp = 0;

    buf[0] = '\0';
    if (!Steam_WorkshopReady() || !st.GetItemInstallInfo) {
        return qfalse;
    }
    return st.GetItemInstallInfo(st.ugc, item, &sizeOnDisk, buf, size, &stamp) ? qtrue : qfalse;
}

/*
===============
Steam_DownloadUGC_f

[QL] "steam_downloadugc <itemid>", el comando que tiene retail (0x004df010).
===============
*/
static void Steam_DownloadUGC_f(void) {
    uint64_t item;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: steam_downloadugc <itemid>\n");
        return;
    }
    item = (uint64_t)_strtoui64(Cmd_Argv(1), NULL, 10);
    if (!Steam_WorkshopReady()) {
        Com_Printf("Steam is not available.\n");
        return;
    }
    Steam_WorkshopDownload(item);
}

/*
===============
Steam_WorkshopList

Los items suscritos, que es de donde retail saca lo que monta al arrancar
(0x004697a0). El fork ya recorre la carpeta del Workshop en disco, asi que esto
sirve para saber cuales conoce Steam, no para montarlos.
===============
*/
int Steam_WorkshopSubscribed(uint64_t* items, int max) {
    if (!Steam_WorkshopReady() || !st.GetSubscribedItems || !st.GetNumSubscribedItems) {
        return 0;
    }
    return (int)st.GetSubscribedItems(st.ugc, items, (unsigned int)max);
}

/*
===============
Steam_GS_SteamID

[QL] 0x00465b00: el servidor publica su propio Steam ID en un configstring, que
es como el cliente sabe contra quien esta jugando.
===============
*/
uint64_t Steam_GS_SteamID(void) {
    if (!st.serverUp || !st.GetSteamID) {
        return 0;
    }
    return st.GetSteamID(st.gameServer);
}

/*
===============================================================================

LA VOZ

Quake Live no manda la voz por su propia red: la manda por la red entre pares
(P2P) de Steam, y la comprime y descomprime con la voz de Steam.

  0x004603f0  "+voice": le avisa a Steam que este jugador esta hablando
              (SetInGameVoiceSpeaking) y arranca la grabacion.
  0x00460d10  cada cuadro, mientras graba: GetVoice comprimido en un buffer de
              16 KB y, si esta en partida, SendP2PPacket al Steam ID del
              servidor -- que sale del configstring 714 -- por el canal 1, sin
              garantia de entrega.
  0x00461a60  cada cuadro, al recibir: ReadP2PPacket del canal 1, el primer byte
              es el numero del que habla y el resto es la voz comprimida, que va
              a DecompressVoice a 22050 Hz y de ahi al mezclador.

El servidor hace de puente: recibe la voz de cada uno y se la reparte a los
demas poniendole delante el numero de cliente del que habla.

===============================================================================
*/

#define STEAM_VOICE_CHANNEL 1
#define STEAM_VOICE_RATE 22050
#define STEAM_P2P_UNRELIABLE 1
#define STEAM_P2P_RELIABLE 2

// el canal por el que el servidor saluda a cada jugador cada diez segundos, que es lo que deja
// la sesion abierta para la voz. Nadie lee lo que viaja por aca.
#define STEAM_P2P_HELLO_CHANNEL 16
#define STEAM_P2P_HELLO_INTERVAL 10000

// P2PSessionRequest_t: k_iSteamNetworkingCallbacks (1200) + 2, y adentro viene un solo CSteamID
#define STEAM_CB_P2P_SESSION_REQUEST 1202
#define STEAM_CB_FLAG_GAMESERVER 0x02

/*
===============
La sesion entre pares

Steam no entrega los paquetes de alguien con quien no hay sesion: avisa con una
devolucion de llamada y espera que uno la acepte. Retail no registra ninguna
-- sus dos unicas llamadas a SteamAPI_RegisterCallback son por otra cosa -- asi
que esto es un agregado nuestro, no algo portado: sin aceptar la sesion la voz
del servidor no llegaria nunca.
===============
*/
static void STEAM_CB Steam_CB_P2PRequestIO(STEAM_CB_SELF, void* param, unsigned char failed,
                                           uint64_t call) {
    (void)param;
    (void)failed;
    (void)call;
}

static void STEAM_CB Steam_CB_P2PRequest(STEAM_CB_SELF, void* param) {
    const steamCallback_t* me = (const steamCallback_t*)self;
    uint64_t who = *(const uint64_t*)param;
    qboolean asServer = (qboolean)((me->flags & STEAM_CB_FLAG_GAMESERVER) != 0);

    Steam_P2PAccept(who, asServer);
}

static int STEAM_CB Steam_CB_P2PRequestSize(STEAM_CB_SELF) {
    return (int)sizeof(uint64_t);
}

// el compilador de retail ordena las dos Run al reves de como se declaran: ver 0x00532f1c
static void* steamP2PVTable[3];

static void Steam_P2PListen(qboolean asServer) {
    steamCallback_t* cb = asServer ? &st.p2pServer : &st.p2pClient;

    if (!st.RegisterCallback || !st.AcceptP2PSessionWithUser || cb->vtable) {
        return;
    }
    steamP2PVTable[0] = (void*)Steam_CB_P2PRequestIO;
    steamP2PVTable[1] = (void*)Steam_CB_P2PRequest;
    steamP2PVTable[2] = (void*)Steam_CB_P2PRequestSize;

    cb->vtable = steamP2PVTable;
    cb->flags = asServer ? STEAM_CB_FLAG_GAMESERVER : 0;
    cb->id = STEAM_CB_P2P_SESSION_REQUEST;
    st.RegisterCallback(cb, STEAM_CB_P2P_SESSION_REQUEST);
}

qboolean Steam_VoiceReady(void) {
    return (qboolean)(st.clientUp && st.user && st.StartVoiceRecording != NULL);
}

/*
===============
Steam_VoiceStart / Steam_VoiceStop

[QL] 0x004603f0 y 0x00460490.
===============
*/
void Steam_VoiceStart(void) {
    if (!Steam_VoiceReady() || st.voiceRecording) {
        return;
    }
    st.voiceRecording = qtrue;
    if (st.friends && st.SetInGameVoiceSpeaking) {
        st.SetInGameVoiceSpeaking(st.friends, st.GetUserSteamID(st.user), 1);
    }
    st.StartVoiceRecording(st.user);
    Com_DPrintf("Started recording - optimal sample rate %u\n", st.GetVoiceOptimalSampleRate
                                                                   ? st.GetVoiceOptimalSampleRate(st.user)
                                                                   : 0);
}

void Steam_VoiceStop(void) {
    if (!Steam_VoiceReady() || !st.voiceRecording) {
        return;
    }
    st.voiceRecording = qfalse;
    st.StopVoiceRecording(st.user);
    if (st.friends && st.SetInGameVoiceSpeaking) {
        st.SetInGameVoiceSpeaking(st.friends, st.GetUserSteamID(st.user), 0);
    }
}

qboolean Steam_VoiceRecording(void) {
    return st.voiceRecording;
}

/*
===============
Steam_VoiceCapture

Lo que el microfono junto desde la ultima vez, ya comprimido. Devuelve cuantos
bytes son, o 0 si no hay nada.
===============
*/
int Steam_VoiceCapture(void* buffer, int maxSize) {
    unsigned int written = 0;
    int result;

    if (!Steam_VoiceReady() || !st.voiceRecording || !st.GetVoice) {
        return 0;
    }
    result = st.GetVoice(st.user, 1, buffer, (unsigned int)maxSize, &written, 0, NULL, 0, NULL, 0);
    if (result != 0) {  // k_EVoiceResultOK
        return 0;
    }
    return (int)written;
}

// la voz de otro, de vuelta a sonido: devuelve cuantos bytes de PCM salieron
int Steam_VoiceDecompress(const void* compressed, int size, void* out, int outSize) {
    unsigned int written = 0;

    if (!Steam_VoiceReady() || !st.DecompressVoice || size <= 0) {
        return 0;
    }
    if (st.DecompressVoice(st.user, compressed, (unsigned int)size, out, (unsigned int)outSize, &written,
                           STEAM_VOICE_RATE) != 0) {
        return 0;
    }
    return (int)written;
}

int Steam_VoiceRate(void) {
    return STEAM_VOICE_RATE;
}

/*
===============
Steam_P2PSend / Steam_P2PReceive

El puente de Steam entre dos jugadores. El servidor usa su propia interfaz
(SteamGameServerNetworking) y el cliente la suya.
===============
*/
static void* Steam_P2P(qboolean asServer) {
    return asServer ? st.gameServerNetworking : st.networking;
}

qboolean Steam_P2PSend(uint64_t to, const void* data, int size, qboolean asServer) {
    void* self = Steam_P2P(asServer);

    if (!self || !st.SendP2PPacket || !to) {
        return qfalse;
    }
    return st.SendP2PPacket(self, to, data, (unsigned int)size, STEAM_P2P_UNRELIABLE,
                            STEAM_VOICE_CHANNEL)
               ? qtrue
               : qfalse;
}

int Steam_P2PReceive(void* buffer, int maxSize, uint64_t* from, qboolean asServer) {
    void* self = Steam_P2P(asServer);
    unsigned int size = 0;
    unsigned int read = 0;

    if (!self || !st.IsP2PPacketAvailable || !st.ReadP2PPacket) {
        return 0;
    }
    if (!st.IsP2PPacketAvailable(self, &size, STEAM_VOICE_CHANNEL)) {
        return 0;
    }
    if (!st.ReadP2PPacket(self, buffer, (unsigned int)maxSize, &read, from, STEAM_VOICE_CHANNEL)) {
        return 0;
    }
    return (int)read;
}

// hay que aceptar al otro antes de que Steam entregue sus paquetes
void Steam_P2PAccept(uint64_t who, qboolean asServer) {
    void* self = Steam_P2P(asServer);

    if (self && st.AcceptP2PSessionWithUser && who) {
        st.AcceptP2PSessionWithUser(self, who);
    }
}

/*
===============
Steam_P2PHello

[QL] 0x004668ca. Cada diez segundos el servidor le manda esto a cada jugador,
por un canal que nadie lee, solo para que Steam mantenga viva la sesion. El
contenido da igual: el de retail es una broma del equipo de id.
===============
*/
void Steam_P2PHello(uint64_t to, qboolean asServer) {
    void* self = Steam_P2P(asServer);
    static const char hello[] = "ioquakelive voice keepalive";

    if (self && st.SendP2PPacket && to) {
        st.SendP2PPacket(self, to, hello, (unsigned int)sizeof(hello), STEAM_P2P_RELIABLE,
                         STEAM_P2P_HELLO_CHANNEL);
    }
}

/*
===============
Steam_ShowFriendAdd / Steam_ShowProfile

Las dos paginas de la superposicion de Steam que QL abre sobre otro jugador.

[QL] los nombres de los dialogos estan en 0x005327f0, pegados a las ordenes que
los usan: "friendadd" para agregarlo y "steamid" para su perfil.
===============
*/
static void Steam_ShowOverlayUser(const char* dialog, uint64_t steamId) {
    if (st.clientUp && st.friends && st.ActivateGameOverlayToUser && steamId) {
        st.ActivateGameOverlayToUser(st.friends, dialog, steamId);
    }
}

/*
===============
La lista de amigos

Steam la entrega de a uno: cuantos hay con la bandera de "amigo inmediato" (4) y
despues, por indice, su Steam ID. De cada uno salen el nombre, el estado y, si
esta jugando a algo, el juego y el servidor donde esta.

FriendGameInfo_t, tal como lo llena Steam: el id del juego (8 bytes), la IP del
servidor, el puerto y el puerto de consulta, y el id de la sala. Aca solo
interesan el juego y el servidor.
===============
*/
#define STEAM_FRIEND_IMMEDIATE 4

typedef struct {
    uint64_t gameId;
    unsigned int ip;
    unsigned short port;
    unsigned short queryPort;
    uint64_t lobby;
} steamFriendGame_t;

/*
===============
Steam_StripColors

Un nombre de Steam es un dato, no una marca: hay jugadores de Quake que llevan
codigos de color en el suyo ("^2Runnyr") y, sin sacarlos, recolorean lo que venga
detras y la inicial del menu sale siendo un acento circunflejo.

Q_CleanStr no sirve aca: ademas de los codigos tira todo lo que no sea ASCII
imprimible, y con eso un nombre como "Gze" con simbolos raros queda vacio.
===============
*/
static void Steam_StripColors(char* s) {
    char* d = s;

    while (*s) {
        if (Q_IsColorString(s)) {
            s += 2;
            continue;
        }
        if ((unsigned char)*s < 0x20) {
            s++;  // los de control tampoco: rompen la fila
            continue;
        }
        *d++ = *s++;
    }
    *d = '\0';
}

int Steam_FriendCount(void) {
    if (!st.clientUp || !st.friends || !st.GetFriendCount) {
        return 0;
    }
    return st.GetFriendCount(st.friends, STEAM_FRIEND_IMMEDIATE);
}

qboolean Steam_FriendGet(int index, steamFriend_t* out) {
    steamFriendGame_t juego;
    const char* nombre;
    uint64_t id;

    Com_Memset(out, 0, sizeof(*out));
    if (!st.clientUp || !st.friends || !st.GetFriendByIndex) {
        return qfalse;
    }
    id = st.GetFriendByIndex(st.friends, index, STEAM_FRIEND_IMMEDIATE);
    if (!id) {
        return qfalse;
    }

    out->steamId = id;
    nombre = st.GetFriendPersonaName ? st.GetFriendPersonaName(st.friends, id) : NULL;
    Q_strncpyz(out->name, nombre ? nombre : "", sizeof(out->name));
    Steam_StripColors(out->name);
    out->state = st.GetFriendPersonaState ? st.GetFriendPersonaState(st.friends, id) : 0;

    Com_Memset(&juego, 0, sizeof(juego));
    if (st.GetFriendGamePlayed && st.GetFriendGamePlayed(st.friends, id, &juego)) {
        out->playing = qtrue;
        // el id de juego de Steam lleva el App ID en sus 32 bits bajos
        out->sameGame = (qboolean)((unsigned int)juego.gameId == (unsigned int)atoi(STEAM_APP_ID));
        out->ip = juego.ip;
        out->port = juego.port;
    }
    return qtrue;
}

/*
===============
Steam_FriendStateName

Los nombres que le pone Steam a cada estado, en el orden de su enumeracion.
===============
*/
const char* Steam_FriendStateName(int state) {
    static const char* nombres[] = {
        "desconectado", "conectado", "ocupado", "ausente", "dormido",
        "buscando cambio", "buscando partida"
    };

    if (state < 0 || state >= (int)ARRAY_LEN(nombres)) {
        return "?";
    }
    return nombres[state];
}

void Steam_ShowFriends(void) {
    if (st.clientUp && st.friends && st.ActivateGameOverlay) {
        st.ActivateGameOverlay(st.friends, "friends");
    }
}

void Steam_ShowFriendAdd(uint64_t steamId) {
    Steam_ShowOverlayUser("friendadd", steamId);
}

void Steam_ShowProfile(uint64_t steamId) {
    Steam_ShowOverlayUser("steamid", steamId);
}

uint64_t Steam_ClientSteamID(void) {
    if (!st.clientUp || !st.user || !st.GetUserSteamID) {
        return 0;
    }
    return st.GetUserSteamID(st.user);
}

// [QL] 0x00461556: lo que ve un amigo en la lista de Steam
void Steam_SetRichPresence(const char* status) {
    if (st.clientUp && st.friends && st.SetRichPresence) {
        st.SetRichPresence(st.friends, "status", status);
    }
}
