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

// The server list callbacks are C++ virtual methods: on Windows the convention is __thiscall
// (the object in ecx), imitated here with __fastcall and a filler parameter for edx; in the
// Linux ABI the object is just the first argument.
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

// gameserveritem_t, with the offsets the retail binary uses (0x00462a50)
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

// what is kept from each server that answers
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

// the object Steam sees as ISteamMatchmakingServerListResponse
typedef struct {
    void** vtable;
} steamListResponse_t;

static void Steam_GS_SetGameInfo(void);
static void Steam_DownloadUGC_f(void);
static void Steam_P2PListen(qboolean asServer);
static void Steam_AuthListen(void);

/*
A Steam callback seen from C. In the SDK it is a class with three virtual functions,
and the retail binary shows how the compiler ordered them: at 0x00532f1c comes
Run(data, failed, call) first, Run(data) second and GetCallbackSizeBytes() third --
the reverse of how they are declared, which is what MSVC does with overloads.

The fields are CCallbackBase's: the table, the flags (0x02 says the callback is the
server's and not the client's) and the callback number.
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

    // voice and the peer to peer network
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

    // who each player is: the client asks for a signed ticket and the server validates it
    unsigned int(QDECL* GetAuthSessionTicket)(void* self, void* ticket, int maxTicket, unsigned int* written);
    void(QDECL* CancelAuthTicket)(void* self, unsigned int ticket);
    int(QDECL* BeginAuthSession)(void* self, const void* ticket, int len, uint64_t steamId);
    void(QDECL* EndAuthSession)(void* self, uint64_t steamId);
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
    steamCallback_t p2pClient;   // the session the server asks for
    steamCallback_t p2pServer;   // the session each player asks for
    unsigned int authTicket;       // the ticket this client asked for, so it can be cancelled
    steamCallback_t authResponse;  // what Steam says about somebody else's ticket

    void* ugc;           // ISteamUGC *
    uint64_t downloading;  // the item being waited on, or 0

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

    // The server browser part is optional on purpose: if this library does not carry it, the
    // rest -- the master, the stats, the voice -- keeps working.
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
    STEAM_BIND_OPT(GetAuthSessionTicket, "SteamAPI_ISteamUser_GetAuthSessionTicket");
    STEAM_BIND_OPT(CancelAuthTicket, "SteamAPI_ISteamUser_CancelAuthTicket");
    STEAM_BIND_OPT(BeginAuthSession, "SteamAPI_ISteamGameServer_BeginAuthSession");
    STEAM_BIND_OPT(EndAuthSession, "SteamAPI_ISteamGameServer_EndAuthSession");
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

    // Retail is launched by Steam, which passes the App ID through the environment; this is
    // launched on its own, so it says so itself. Without it the API does not know which game
    // this is and both SteamAPI_Init and SteamGameServer_Init fail.
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

[ioquakelive] A diagnostic of our own, not retail's: it says whether the library loaded,
whether the client and the server came up, whether the Steam session is logged on, whether VAC
is active and which public IP the server registered with.
===============
*/
static void Steam_Status_f(void) {
    unsigned int ip;

    Com_Printf("library:     %s\n", st.lib ? "loaded" : "no");
    Com_Printf("client:      %s\n", st.clientUp ? "up" : "no");
    Com_Printf("server:      %s\n", st.serverUp ? "up" : "no");

    // identity: the ticket this client would send in its getchallenge and, if this process
    // also runs a server, what Steam says about that ticket. It is the only way to see the
    // whole chain without two machines.
    if (st.clientUp) {
        static byte ticket[512];
        int len = Steam_AuthTicket(ticket, sizeof(ticket));

        if (len <= 0) {
            Com_Printf("ticket:      Steam gave none\n");
        } else {
            Com_Printf("ticket:      %i bytes\n", len);
            if (st.serverUp) {
                uint64_t me = Steam_ClientSteamID();
                int r = Steam_GS_BeginAuth(me, ticket, len);

                Com_Printf("BeginAuth:   %i (%s)\n", r, r == STEAM_AUTH_BEGIN_OK ? "accepted" : "refused");
                Steam_GS_EndAuth(me);
            }
        }
    }

    if (!st.serverUp) {
        return;
    }
    Com_Printf("session:     %s\n", st.BLoggedOn(st.gameServer) ? "logged on to Steam" : "not logged on");
    Com_Printf("VAC:         %s\n", st.BSecure(st.gameServer) ? "yes" : "no");
    ip = st.GetPublicIP(st.gameServer);
    Com_Printf("public IP:   %u.%u.%u.%u\n", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff,
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
        st.matchmaking = st.Matchmaking();  // the server list interface
    }
    if (st.UGC) {
        st.ugc = st.UGC();  // the Workshop's, where the maps come from
    }
    if (st.Networking) {
        st.networking = st.Networking();  // the peer to peer network, which voice travels over
    }
    if (st.User) {
        st.user = st.User();
    }
    if (st.Friends) {
        st.friends = st.Friends();
    }
    Steam_P2PListen(qfalse);
    // [QL] 0x00461565: "+voice" and "-voice" are registered on the client side, which is also
    // the side that has to tell the cgame somebody is talking
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

    sv_vac = Cvar_Get("sv_vac", "1", CVAR_INIT);  // [QL] retail registers it like this
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
        st.gameServerNetworking = st.GameServerNetworking();  // what it hands the voice out over
        Steam_P2PListen(qtrue);
    }
    Steam_AuthListen();  // whatever Steam says about each ticket
    if (!st.clientUp) {
        Cmd_AddCommand("steamstatus", Steam_Status_f);  // the client added it already if it came up
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
    // [QL] sv_master decides whether the server answers queries and shows up in the list.
    // Retail's server.cfg spells it out: turning it off takes the server out of the browser,
    // the LAN one included.
    st.EnableHeartbeats(st.gameServer,
                        (steamBool_t)(Cvar_VariableIntegerValue("sv_master") != 0));
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
        if (st.authResponse.vtable) {
            st.UnregisterCallback(&st.authResponse);
            st.authResponse.vtable = NULL;
        }
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

Quake Live has no master of its own and no browser protocol: the client asks Steam
for the list. Both halves are in the retail binary.

The server publishes its side (0x00466260): the game description is the gametype's
long name ("Free For All", "Clan Arena", ...) and the tags are the short name plus one
word per rule that is not the usual one:

    <short>,[cheats,][instagib,][lowgrav,|highgrav,][vampiric,][infected,]
    [quadhog,][sv_tags]

The client asks for the list (0x00462eb0) with a single filter, "gamedir" = "baseq3",
according to where it wants it from: internet, LAN, friends, favourites or history.
Each server that answers is built (0x00462a50) from what Steam returns in
gameserveritem_t: name, map, players, bots, ping, password, VAC, the tags and the
description.

===============================================================================
*/

// [QL] 0x00560e50: the long name goes in the game description
static const char* steamGametypeNames[] = {
    "Free For All", "Duel", "Race", "Team Deathmatch", "Clan Arena", "Capture the Flag",
    "One Flag CTF", "Overload", "Harvester", "Freeze Tag", "Domination", "Attack and Defend",
    "Red Rover"
};

// [QL] 0x00560e84: the short name opens the tags
static const char* steamGametypeTags[] = {
    "ffa", "duel", "race", "tdm", "clanarena", "ctf", "oneflag", "overload", "harvester",
    "freezetag", "domination", "a&d", "redrover"
};

#define STEAM_NUM_GAMETYPES ((int)ARRAY_LEN(steamGametypeNames))

/*
===============
Steam_GS_SetGameInfo

[QL] 0x00466260. What the browser shows about this server beyond the name and the
map: what is being played and under which rules.
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
    // infected only counts in Red Rover, and quad hog only in free for all
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

    // the trailing comma is not wanted
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

// what Steam answers about a server, turned into what the browser needs
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
    // retail does not show a server that fails to answer either
    (void)request;
    (void)index;
}

static void STEAM_CB Steam_CB_RefreshComplete(STEAM_CB_SELF, void* request, int response) {
    (void)request;
    st.listRefreshing = qfalse;
    Com_DPrintf("Steam: the list ended with %i servers (response %i)\n", st.numServers, response);
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

    // retail's only filter: that it is Quake Live and not another game under the same App ID
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
    int count = -1;
    int refreshing = -1;

    if (st.matchmaking && st.listRequest) {
        if (st.GetServerCount) {
            count = st.GetServerCount(st.matchmaking, st.listRequest);
        }
        if (st.IsRefreshing) {
            refreshing = st.IsRefreshing(st.matchmaking, st.listRequest) ? 1 : 0;
        }
    }
    Com_Printf("Steam: request %p, ours %i, Steam's %i, refreshing %i (flag %i)\n",
               st.listRequest, st.numServers, count, refreshing, st.listRefreshing);
    for (count = 0; count < st.numServers && count < 5; count++) {
        const steamServer_t* sv = &st.servers[count];

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

Returns one server from the list in the shape the browser expects: the address, the
ping Steam measured and an info string like the one the server itself would answer
with. The gametype comes from the first tag, which is how it is published
(0x00466260); an unrecognised one leaves it at zero.
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

Quake Live does not download maps over HTTP: it downloads them from the Steam
Workshop. The server publishes the items the match uses (0x00465b00) in the
CS_STEAM_WORKSHOP_IDS configstring and in the sv_referencedSteamworks cvar, and the
client, before entering, downloads the ones it lacks (0x004bba30), showing progress in
cl_downloadName / cl_downloadItem / cl_downloadTime.

Retail learns that a download finished through the OnDownloadItemResult callback
(0x00469630). Here the item's state is polled every frame, which looks the same from
the outside and avoids registering C++ callbacks by hand.

The state flags are Steam's: 1 subscribed, 4 installed, 8 needs an update, 16
downloading, 32 queued.

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

[QL] 0x004699c0. Returns qtrue when there is something to wait for: an item already
installed is not asked for again.
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
    Com_DPrintf("Workshop item %llu: state %u before asking for it.\n", (unsigned long long)item,
                Steam_WorkshopState(item));
    if (!st.DownloadItem(st.ugc, item, 1)) {
        Com_Printf("Workshop item %llu: Steam did not take the request.\n", (unsigned long long)item);
        return qfalse;
    }
    st.downloading = item;
    return qtrue;
}

// how much Steam has downloaded of the item being waited on
qboolean Steam_WorkshopProgress(uint64_t* done, uint64_t* total) {
    *done = 0;
    *total = 0;
    if (!Steam_WorkshopReady() || !st.downloading || !st.GetItemDownloadInfo) {
        return qfalse;
    }
    return st.GetItemDownloadInfo(st.ugc, st.downloading, done, total) ? qtrue : qfalse;
}

// qtrue while the item asked for is still missing
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

// where an item ended up installed, so it can be mounted
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
    item = (uint64_t)strtoull(Cmd_Argv(1), NULL, 10);
    if (!Steam_WorkshopReady()) {
        Com_Printf("Steam is not available.\n");
        return;
    }
    Steam_WorkshopDownload(item);
}

/*
===============
Steam_WorkshopList

The subscribed items, which is where retail gets what it mounts at startup
(0x004697a0). The Workshop folder on disk is already walked here, so this is for
knowing which ones Steam knows about, not for mounting them.
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

[QL] 0x00465b00: the server publishes its own Steam ID in a configstring, which is
how the client knows who it is playing against.
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

Quake Live does not send voice over its own network: it sends it over Steam's peer to
peer network, and compresses and decompresses it with Steam's voice codec.

  0x004603f0  "+voice": tells Steam this player is speaking
              (SetInGameVoiceSpeaking) and starts recording.
  0x00460d10  every frame while recording: GetVoice compressed into a 16 KB
              buffer and, when in a match, SendP2PPacket to the server's Steam
              ID -- which comes from configstring 714 -- over channel 1, with no
              delivery guarantee.
  0x00461a60  every frame on receive: ReadP2PPacket from channel 1, where the
              first byte is the speaker's number and the rest is compressed
              voice, which goes to DecompressVoice at 22050 Hz and from there to
              the mixer.

The server acts as the bridge: it takes each player's voice and hands it out to the
others with the speaker's client number in front.

===============================================================================
*/

#define STEAM_VOICE_CHANNEL 1
#define STEAM_VOICE_RATE 22050
#define STEAM_P2P_UNRELIABLE 1
#define STEAM_P2P_RELIABLE 2

// the channel the server greets each player on every ten seconds, which is what keeps the
// session open for voice. Nobody reads what travels over it.
#define STEAM_P2P_HELLO_CHANNEL 16
#define STEAM_P2P_HELLO_INTERVAL 10000

// P2PSessionRequest_t: k_iSteamNetworkingCallbacks (1200) + 2, carrying a single CSteamID
#define STEAM_CB_P2P_SESSION_REQUEST 1202
#define STEAM_CB_FLAG_GAMESERVER 0x02

/*
===============
La sesion entre pares

Steam does not deliver packets from somebody there is no session with: it raises a
callback and waits for it to be accepted. Retail registers none -- its only two calls
to SteamAPI_RegisterCallback are for something else -- so this is an addition, not a
port: without accepting the session the server's voice would never arrive.
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

// retail's compiler orders the two Run overloads the reverse of how they are declared: see
// 0x00532f1c
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

/*
===============================================================================

QUIEN ES CADA UNO

[QL] The retail server does not take the client's word for who it is. Alongside the
Steam ID, the client sends a ticket Steam signed for it in the getchallenge, and the
server hands that to Steam to say whether it belongs to who it claims. It is all
written in the engine binary:

    0x531b38  "Called BeginAuthSession on steam id %llu, got response %i"
    0x531b74  "Refusing to re-auth a client that is already being tracked"
    0x531bb0  "Called EndAuthSession on steam id %llu"
    0x531a80  "Steam OnValidateAuthTicketResponse: couldn't match auth response to a client"
    0x531d48  "Found an authed client with steam id %llu but isn't connected to the server."
    0x55f5c0  the RTTI of a CCallback<..., ValidateAuthTicketResponse_t>
    0x531778..0x531848  the nine rejection reasons, one after another

The verdict does not arrive at once: BeginAuthSession only says whether the ticket has
the shape of a ticket, and Steam answers later through a callback. That is why a
player gets in and, if the ticket is no good, is dropped a moment afterwards.

===============================================================================
*/

// ValidateAuthTicketResponse_t: k_iSteamUserCallbacks (100) + 43
#define STEAM_CB_VALIDATE_AUTH 143

/*
===============
Steam_AuthResponseName

The nine reasons Steam rejects a ticket, in retail's own words (0x531778 onwards,
which sit in the binary in the reverse order of Steam's enumeration). 0 is "fine", and
has no text.
===============
*/
const char* Steam_AuthResponseName(int response) {
    static const char* const reasons[] = {
        "",
        "User not connected to Steam",   // 1
        "License expired",               // 2
        "VAC ban on record",             // 3
        "User is logged in elsewhere",   // 4
        "VAC check timed out",           // 5
        "Issuer canceled auth ticket",   // 6
        "Auth ticket already used",      // 7
        "Auth ticket invalid",           // 8
        "Game ban on record"             // 9
    };

    if (response <= 0 || response >= (int)ARRAY_LEN(reasons)) {
        return NULL;  // the caller writes "(Err:%i)", as retail does at 0x531a18
    }
    return reasons[response];
}

/*
===============
Steam_AuthTicket

The ticket the client sends the server. Steam signs it for this session and it is
good once only, so one is asked for per connection and the previous one is cancelled.
Returns how many bytes it takes, or zero when there is no Steam.
===============
*/
int Steam_AuthTicket(void* buffer, int size) {
    unsigned int written = 0;

    if (!st.clientUp || !st.user || !st.GetAuthSessionTicket) {
        return 0;
    }
    if (st.authTicket && st.CancelAuthTicket) {
        st.CancelAuthTicket(st.user, st.authTicket);
        st.authTicket = 0;
    }
    st.authTicket = st.GetAuthSessionTicket(st.user, buffer, size, &written);
    if (!st.authTicket) {
        return 0;
    }
    return (int)written;
}

/*
===============
Steam_CB_AuthResponse

ValidateAuthTicketResponse_t: the Steam ID being checked, the reason, and the Steam ID
of the game's owner -- which differs when somebody has it on family sharing. Only the
first two are needed here.
===============
*/
// Steam packs its callback structs by operating system, not by word size: four bytes on
// Linux and macOS, eight on Windows (VALVE_CALLBACK_PACK_SMALL / _LARGE). If the size is
// not the one it expects, the callback does not match and never arrives. That matters here
// for the dedicated server, which runs on Linux.
#if defined(__linux__) || defined(__APPLE__)
#pragma pack(push, 4)
#else
#pragma pack(push, 8)
#endif
typedef struct {
    uint64_t steamId;
    int response;
    uint64_t owner;
} steamAuthResponse_t;
#pragma pack(pop)

static void STEAM_CB Steam_CB_AuthIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)param;
    (void)failed;
    (void)call;
}

static void STEAM_CB Steam_CB_Auth(STEAM_CB_SELF, void* param) {
    const steamAuthResponse_t* r = (const steamAuthResponse_t*)param;

    (void)self;
    SV_SteamAuthResponse(r->steamId, r->response);
}

static int STEAM_CB Steam_CB_AuthSize(STEAM_CB_SELF) {
    return (int)sizeof(steamAuthResponse_t);
}

// retail's compiler orders the two Run overloads the reverse of how they are declared: see
// 0x00532f1c
static void* steamAuthVTable[3];

static void Steam_AuthListen(void) {
    if (!st.RegisterCallback || !st.BeginAuthSession || st.authResponse.vtable) {
        return;
    }
    steamAuthVTable[0] = (void*)Steam_CB_AuthIO;
    steamAuthVTable[1] = (void*)Steam_CB_Auth;
    steamAuthVTable[2] = (void*)Steam_CB_AuthSize;

    st.authResponse.vtable = steamAuthVTable;
    st.authResponse.flags = STEAM_CB_FLAG_GAMESERVER;
    st.authResponse.id = STEAM_CB_VALIDATE_AUTH;
    st.RegisterCallback(&st.authResponse, STEAM_CB_VALIDATE_AUTH);
}

/*
===============
Steam_GS_BeginAuth / Steam_GS_EndAuth

What the server asks Steam about each player. BeginAuthSession returns an
EBeginAuthSessionResult: 0 ok, 1 invalid ticket, 2 already tracked, 3 wrong
version, 4 different game, 5 expired ticket. The real verdict arrives later,
through the callback.
===============
*/
int Steam_GS_BeginAuth(uint64_t steamId, const void* ticket, int len) {
    int result;

    if (!st.serverUp || !st.gameServer || !st.BeginAuthSession || len <= 0) {
        return STEAM_AUTH_BEGIN_OK;  // nobody to ask without Steam
    }
    result = st.BeginAuthSession(st.gameServer, ticket, len, steamId);
    Com_DPrintf("Called BeginAuthSession on steam id %llu, got response %i\n",
                (unsigned long long)steamId, result);
    return result;
}

void Steam_GS_EndAuth(uint64_t steamId) {
    if (!st.serverUp || !st.gameServer || !st.EndAuthSession || !steamId) {
        return;
    }
    st.EndAuthSession(st.gameServer, steamId);
    Com_DPrintf("Called EndAuthSession on steam id %llu\n", (unsigned long long)steamId);
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

What the microphone has gathered since last time, already compressed. Returns how many
bytes that is, or 0 when there is nothing.
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

// the peer has to be accepted before Steam will deliver its packets
void Steam_P2PAccept(uint64_t who, qboolean asServer) {
    void* self = Steam_P2P(asServer);

    if (self && st.AcceptP2PSessionWithUser && who) {
        st.AcceptP2PSessionWithUser(self, who);
    }
}

/*
===============
Steam_P2PHello

[QL] 0x004668ca. Every ten seconds the server sends this to each player, over a channel
nobody reads, purely so that Steam keeps the session alive. The contents do not matter:
retail's is a joke from the id team.
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

The two pages of the Steam overlay QL opens on another player.

[QL] the dialog names are at 0x005327f0, next to the commands that use them:
"friendadd" to add them and "steamid" for their profile.
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

Steam hands it over one at a time: how many carry the "immediate friend" flag (4) and
then, by index, their Steam ID. From each one come the name, the state and, when they
are playing something, the game and the server they are on.

FriendGameInfo_t as Steam fills it: the game id (8 bytes), the server's IP, its port
and query port, and the lobby id. Only the game and the server matter here.
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

A Steam name is data, not markup: Quake players do carry colour codes in theirs
("^2Runnyr") and, left in, they recolour whatever comes after and the menu's initial
comes out as a caret.

Q_CleanStr is no good here: along with the codes it throws away everything that is not
printable ASCII, which leaves a name written in unusual symbols empty.
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
            s++;  // nor control characters: they break the row
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
    steamFriendGame_t game;
    const char* name;
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
    name = st.GetFriendPersonaName ? st.GetFriendPersonaName(st.friends, id) : NULL;
    Q_strncpyz(out->name, name ? name : "", sizeof(out->name));
    Steam_StripColors(out->name);
    out->state = st.GetFriendPersonaState ? st.GetFriendPersonaState(st.friends, id) : 0;

    Com_Memset(&game, 0, sizeof(game));
    if (st.GetFriendGamePlayed && st.GetFriendGamePlayed(st.friends, id, &game)) {
        out->playing = qtrue;
        // Steam's game id carries the App ID in its low 32 bits
        out->sameGame = (qboolean)((unsigned int)game.gameId == (unsigned int)atoi(STEAM_APP_ID));
        out->ip = game.ip;
        out->port = game.port;
    }
    return qtrue;
}

/*
===============
Steam_FriendStateName

The names Steam gives each state, in the order of its enumeration.
===============
*/
const char* Steam_FriendStateName(int state) {
    static const char* names[] = {
        "offline", "online", "busy", "away", "snooze",
        "looking to trade", "looking to play"
    };

    if (state < 0 || state >= (int)ARRAY_LEN(names)) {
        return "?";
    }
    return names[state];
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

// [QL] 0x00461556: what a friend sees in the Steam list
void Steam_SetRichPresence(const char* status) {
    if (st.clientUp && st.friends && st.SetRichPresence) {
        st.SetRichPresence(st.friends, "status", status);
    }
}
