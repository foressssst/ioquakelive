/*
===========================================================================
ioquakelive - Steam integration

Quake Live's master server is Steam's: the retail client imports steam_api.dll,
registers the server with SteamGameServer_Init and browses with
SteamMatchmakingServers, so there is no master host name in the binary at all.

  0x00466ed0  Steam GS init, from Com_Init (0x004cc2b3), only when com_build is
              0: net_ip and net_port, (sv_vac != 0) + 2 as the server mode, 0 as
              the Steam port and 0xffff as the query port, version "1069", then
              LogOn with sv_setSteamAccount or LogOnAnonymous, product name
              "Quake Live", mod dir "baseq3".
  0x00465a40  SetMaxPlayerCount, from SV_ChangeMaxClients (0x004e339e).
  0x00461500  the client's SteamAPI_Init, from Com_Init (0x004cc16e): +voice,
              -voice and the rich presence "status".

A vtable slot at +0x9c called with 0 is not identified and is left out rather
than guessed at. The library is loaded at run time from the player's own Quake
Live installation, so nothing of Valve's or id's is redistributed; without it
everything here turns into a no-op.
===========================================================================
*/

#include "q_steam.h"
#include "q_steamstats.h"

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

// The server list callbacks are C++ virtual methods: on 32-bit Windows the convention is
// __thiscall (the object in ecx), imitated here with __fastcall and a filler parameter for edx;
// on 64-bit Windows and in the Linux ABI the object is just the first argument.
#if defined(_WIN32) && !defined(_WIN64)
#define STEAM_CB __fastcall
#define STEAM_CB_SELF void* self, void* unusedEdx
#define STEAM_CB_PASS self, unusedEdx
#else
#define STEAM_CB
#define STEAM_CB_SELF void* self
#define STEAM_CB_PASS self
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
#define STEAM_MAX_TRACKED 256

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
    qboolean queued;  // answered again, waiting for the client to take it
} steamServer_t;

// the object Steam sees as ISteamMatchmakingServerListResponse, and the same shape for the
// rules and the players of a single server (ISteamMatchmakingRulesResponse and
// ISteamMatchmakingPlayersResponse)
typedef struct {
    void** vtable;
} steamListResponse_t;

#define STEAM_DETAILS_RULES 8192
#define STEAM_DETAILS_PLAYERS 4096

static void Steam_WorkshopListen(qboolean asServer);
static void Steam_P2PListen(qboolean asServer);
static void Steam_AuthListen(void);
static void Steam_ServersListen(void);
static void Steam_StatsListen(void);
static void Steam_UserStatsListen(void);

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
    void(QDECL* SetKeyValue)(void* self, const char* key, const char* value);
    steamBool_t(QDECL* BUpdateUserData)(void* self, uint64_t steamId, const char* name, unsigned int score);

    // the server list
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
    void(QDECL* RefreshQuery)(void* self, void* request);
    int(QDECL* ServerRules)(void* self, unsigned int ip, unsigned short port, void* response);
    int(QDECL* PlayerDetails)(void* self, unsigned int ip, unsigned short port, void* response);
    void(QDECL* CancelServerQuery)(void* self, int query);
    void*(QDECL* Lobbies)(void);
    int(QDECL* AddFavoriteGame)(void* self, unsigned int appId, unsigned int ip, unsigned short connPort,
                                unsigned short queryPort, unsigned int flags, unsigned int lastPlayed);
    steamBool_t(QDECL* RemoveFavoriteGame)(void* self, unsigned int appId, unsigned int ip,
                                           unsigned short connPort, unsigned short queryPort,
                                           unsigned int flags);

    void*(QDECL* UGC)(void);
    void*(QDECL* GameServerUGC)(void);
    uint64_t(QDECL* SubscribeItem)(void* self, uint64_t item);
    uint64_t(QDECL* UnsubscribeItem)(void* self, uint64_t item);
    void*(QDECL* Utils)(void);
    int(QDECL* GetMediumFriendAvatar)(void* self, uint64_t steamId);
    steamBool_t(QDECL* GetImageSize)(void* self, int image, unsigned int* width, unsigned int* height);
    steamBool_t(QDECL* GetImageRGBA)(void* self, int image, unsigned char* dest, int size);
    void* utils;  // ISteamUtils *
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
    steamBool_t(QDECL* InviteUserToGame)(void* self, uint64_t steamId, const char* connect);
    int(QDECL* GetFriendCount)(void* self, int flags);
    uint64_t(QDECL* GetFriendByIndex)(void* self, int index, int flags);
    const char*(QDECL* GetFriendPersonaName)(void* self, uint64_t steamId);
    int(QDECL* GetFriendPersonaState)(void* self, uint64_t steamId);
    steamBool_t(QDECL* GetFriendGamePlayed)(void* self, uint64_t steamId, void* info);
    const char*(QDECL* GetPlayerNickname)(void* self, uint64_t steamId);
    const char*(QDECL* GetFriendRichPresence)(void* self, uint64_t steamId, const char* key);
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
    steamCallback_t serversConnected;     // the server logged on to Steam
    steamCallback_t serversFailed;        // it could not
    steamCallback_t serversDisconnected;  // it lost the connection
    qboolean loggedOn;                    // [QL] 0x008e0644, between the first and the other two
    steamCallback_t itemInstalled;        // ItemInstalled_t, the Workshop's
    steamCallback_t downloadResult;       // DownloadItemResult_t

    // the players' stats and achievements: ISteamGameServerStats001 on the server and
    // ISteamUserStats011 for a game of one's own
    void*(QDECL* GameServerStats)(void);
    void*(QDECL* UserStats)(void);
    uint64_t(QDECL* RequestUserStats)(void* self, uint64_t steamId);
    steamBool_t(QDECL* GetUserStat)(void* self, uint64_t steamId, const char* name, int* value);
    steamBool_t(QDECL* SetUserStat)(void* self, uint64_t steamId, const char* name, int value);
    steamBool_t(QDECL* SetUserAchievement)(void* self, uint64_t steamId, const char* name);
    uint64_t(QDECL* StoreUserStats)(void* self, uint64_t steamId);
    steamBool_t(QDECL* RequestCurrentStats)(void* self);
    steamBool_t(QDECL* SetAchievement)(void* self, const char* name);
    steamBool_t(QDECL* StoreStats)(void* self);
    void* gameServerStats;          // ISteamGameServerStats *
    void* userStats;                // ISteamUserStats *
    steamCallback_t statsReceived;  // GSStatsReceived_t
    steamCallback_t statsStored;    // GSStatsStored_t

    // any player's stats as the client's ISteamUserStats hands them over, for the front end
    uint64_t(QDECL* UserRequestUserStats)(void* self, uint64_t steamId);
    steamBool_t(QDECL* UserGetUserStat)(void* self, uint64_t steamId, const char* name, int* value);
    steamCallback_t userStatsReceived;  // UserStatsReceived_t

    // the Steam IDs with an auth session open (retail's set at 0x00e3035c)
    uint64_t tracked[STEAM_MAX_TRACKED];
    int numTracked;

    void* ugc;  // ISteamUGC *, the server's on a dedicated server and the player's otherwise

    void* matchmaking;   // ISteamMatchmakingServers *
    void* listRequest;   // HServerListRequest
    qboolean listRefreshing;
    steamListResponse_t listResponse;
    int numServers;
    steamServer_t servers[STEAM_MAX_SERVERS];
    int serverIndex[STEAM_MAX_SERVERS];   // Steam's index + 1 -> ours + 1, 0 until it answers
    int queued[STEAM_MAX_SERVERS];        // the ones that answered again
    int numQueued;

    void* lobbies;  // ISteamMatchmaking *, where the favourites are kept

    // the rules and the players of the server the browser has selected
    steamListResponse_t rulesResponse;
    steamListResponse_t playersResponse;
    int rulesQuery;    // HServerQuery, 0 when none is going on
    int playersQuery;
    int detailsState;  // STEAM_DETAILS_*
    qboolean playersDone;
    int rulesLen;
    int playersLen;
    char rules[STEAM_DETAILS_RULES];
    char players[STEAM_DETAILS_PLAYERS];
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
    STEAM_BIND_OPT(SetKeyValue, "SteamAPI_ISteamGameServer_SetKeyValue");
    STEAM_BIND_OPT(BUpdateUserData, "SteamAPI_ISteamGameServer_BUpdateUserData");
    STEAM_BIND_OPT(Matchmaking, "SteamMatchmakingServers");
    STEAM_BIND_OPT(UGC, "SteamUGC");
    STEAM_BIND_OPT(GameServerUGC, "SteamGameServerUGC");
    STEAM_BIND_OPT(SubscribeItem, "SteamAPI_ISteamUGC_SubscribeItem");
    STEAM_BIND_OPT(UnsubscribeItem, "SteamAPI_ISteamUGC_UnsubscribeItem");
    STEAM_BIND_OPT(Utils, "SteamUtils");
    STEAM_BIND_OPT(GetMediumFriendAvatar, "SteamAPI_ISteamFriends_GetMediumFriendAvatar");
    STEAM_BIND_OPT(GetImageSize, "SteamAPI_ISteamUtils_GetImageSize");
    STEAM_BIND_OPT(GetImageRGBA, "SteamAPI_ISteamUtils_GetImageRGBA");
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
    STEAM_BIND_OPT(InviteUserToGame, "SteamAPI_ISteamFriends_InviteUserToGame");
    STEAM_BIND_OPT(GetFriendCount, "SteamAPI_ISteamFriends_GetFriendCount");
    STEAM_BIND_OPT(GetFriendByIndex, "SteamAPI_ISteamFriends_GetFriendByIndex");
    STEAM_BIND_OPT(GetFriendPersonaName, "SteamAPI_ISteamFriends_GetFriendPersonaName");
    STEAM_BIND_OPT(GetFriendPersonaState, "SteamAPI_ISteamFriends_GetFriendPersonaState");
    STEAM_BIND_OPT(GetFriendGamePlayed, "SteamAPI_ISteamFriends_GetFriendGamePlayed");
    STEAM_BIND_OPT(GetPlayerNickname, "SteamAPI_ISteamFriends_GetPlayerNickname");
    STEAM_BIND_OPT(GetFriendRichPresence, "SteamAPI_ISteamFriends_GetFriendRichPresence");
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
    STEAM_BIND_OPT(RefreshQuery, "SteamAPI_ISteamMatchmakingServers_RefreshQuery");
    STEAM_BIND_OPT(ServerRules, "SteamAPI_ISteamMatchmakingServers_ServerRules");
    STEAM_BIND_OPT(PlayerDetails, "SteamAPI_ISteamMatchmakingServers_PlayerDetails");
    STEAM_BIND_OPT(CancelServerQuery, "SteamAPI_ISteamMatchmakingServers_CancelServerQuery");
    STEAM_BIND_OPT(Lobbies, "SteamMatchmaking");
    STEAM_BIND_OPT(AddFavoriteGame, "SteamAPI_ISteamMatchmaking_AddFavoriteGame");
    STEAM_BIND_OPT(RemoveFavoriteGame, "SteamAPI_ISteamMatchmaking_RemoveFavoriteGame");
    STEAM_BIND_OPT(GameServerStats, "SteamGameServerStats");
    STEAM_BIND_OPT(UserStats, "SteamUserStats");
    STEAM_BIND_OPT(RequestUserStats, "SteamAPI_ISteamGameServerStats_RequestUserStats");
    STEAM_BIND_OPT(GetUserStat, "SteamAPI_ISteamGameServerStats_GetUserStat");  // the int32 one
    STEAM_BIND_OPT(SetUserStat, "SteamAPI_ISteamGameServerStats_SetUserStat");  // the int32 one
    STEAM_BIND_OPT(SetUserAchievement, "SteamAPI_ISteamGameServerStats_SetUserAchievement");
    STEAM_BIND_OPT(StoreUserStats, "SteamAPI_ISteamGameServerStats_StoreUserStats");
    STEAM_BIND_OPT(RequestCurrentStats, "SteamAPI_ISteamUserStats_RequestCurrentStats");
    STEAM_BIND_OPT(SetAchievement, "SteamAPI_ISteamUserStats_SetAchievement");
    STEAM_BIND_OPT(StoreStats, "SteamAPI_ISteamUserStats_StoreStats");
    STEAM_BIND_OPT(UserRequestUserStats, "SteamAPI_ISteamUserStats_RequestUserStats");
    STEAM_BIND_OPT(UserGetUserStat, "SteamAPI_ISteamUserStats_GetUserStat");  // the int32 one
    if (!st.UserGetUserStat) {
        // later flat APIs give each overload its own name
        STEAM_BIND_OPT(UserGetUserStat, "SteamAPI_ISteamUserStats_GetUserStatInt32");
    }
    return qtrue;
}

#if defined(_WIN32) && defined(_WIN64)
/*
===============================================================================

STEAM ON 64-BIT WINDOWS

[ioquakelive] Quake Live only ships a 32-bit steam_api.dll, and nothing of Valve's goes in our
package, so the 64-bit build does what steam_api does inside, against the steamclient64.dll of
the player's own Steam: CreateInterface for SteamClient017, a pipe and a user, and every
interface at the version Quake Live's steam_api.dll asks for. Each entry point below stands for
one of that dll's SteamAPI_ISteam*_* wrappers, which only call a slot of the interface's table;
the slots were read out of it, and a table keeps its order whatever the pointer size.

steamclient64 no longer runs callbacks for a library that does not register itself with it, so
they are fetched and handed out here (Steam_BGetCallback), on the thread that runs the frame.
===============================================================================
*/

// CallbackMsg_t
typedef struct {
    int user;
    int id;
    unsigned char* param;
    int size;
} steamCallbackMsg_t;

#define SD_MAX_CALLBACKS 32
#define SD_ACCOUNT_GAMESERVER 3  // k_EAccountTypeGameServer
#define SD_FLAG_SECURE 0x02      // k_unServerFlagSecure
#define SD_FLAG_PRIVATE 0x20     // k_unServerFlagPrivate
#define SD_CB_GAMESERVER 0x02    // CCallbackBase's flag for a server's callback

static struct {
    HMODULE lib;
    void* client;  // ISteamClient017 *
    void*(QDECL* CreateInterface)(const char* version, int* returnCode);
    steamBool_t(QDECL* BGetCallback)(int pipe, steamCallbackMsg_t* msg, int* call);
    void(QDECL* FreeLastCallback)(int pipe);

    int clientPipe;
    int clientUser;
    int serverPipe;
    int serverUser;
    void* user;         // ISteamUser018 *
    void* friends;      // ISteamFriends015 *
    void* lobbies;      // ISteamMatchmaking009 *
    void* matchmaking;  // ISteamMatchmakingServers002 *
    void* networking;   // ISteamNetworking005 *
    void* ugc;          // ISteamUGC007 *
    void* utils;        // ISteamUtils007 *, the client's
    void* gameServer;   // ISteamGameServer012 *
    void* gameServerNetworking;
    void* gameServerStats;  // ISteamGameServerStats001 *
    void* gameServerUGC;    // ISteamUGC007 *, the server's
    void* userStats;        // ISteamUserStats011 *

    steamCallback_t* callbacks[SD_MAX_CALLBACKS];
    int numCallbacks;
} sd;

// a slot of an interface's table
#define SD_VT(self, slot) ((*(void***)(self))[slot])

// ISteamClient017, at the slots Quake Live's steam_api.dll calls (0x3b406565, 0x3b407730)
static int SD_CreateSteamPipe(void) {
    return ((int (*)(void*))SD_VT(sd.client, 0))(sd.client);
}

static void SD_ReleaseSteamPipe(int pipe) {
    ((steamBool_t(*)(void*, int))SD_VT(sd.client, 1))(sd.client, pipe);
}

static int SD_ConnectToGlobalUser(int pipe) {
    return ((int (*)(void*, int))SD_VT(sd.client, 2))(sd.client, pipe);
}

static int SD_CreateLocalUser(int* pipe, int accountType) {
    return ((int (*)(void*, int*, int))SD_VT(sd.client, 3))(sd.client, pipe, accountType);
}

static void SD_ReleaseUser(int pipe, int user) {
    ((void (*)(void*, int, int))SD_VT(sd.client, 4))(sd.client, pipe, user);
}

static void SD_SetLocalIPBinding(unsigned int ip, unsigned short port) {
    ((void (*)(void*, unsigned int, unsigned short))SD_VT(sd.client, 7))(sd.client, ip, port);
}

// GetISteamUser (5), GetISteamGameServer (6), GetISteamFriends (8), GetISteamMatchmaking (10),
// GetISteamMatchmakingServers (11), GetISteamUserStats (13), GetISteamGameServerStats (14),
// GetISteamNetworking (16), GetISteamUGC (26)
static void* SD_Interface(int slot, int user, int pipe, const char* version) {
    return ((void* (*)(void*, int, int, const char*))SD_VT(sd.client, slot))(sd.client, user, pipe, version);
}

// GetISteamUtils (9), and its GetAppID (9): the App ID this process says it is
static unsigned int SD_AppID(int pipe) {
    void* utils = ((void* (*)(void*, int, const char*))SD_VT(sd.client, 9))(sd.client, pipe, "SteamUtils007");

    if (!utils) {
        return 0;
    }
    return ((unsigned int (*)(void*))SD_VT(utils, 9))(utils);
}

static void SD_ShutdownIfAllPipesClosed(void) {
    ((steamBool_t(*)(void*))SD_VT(sd.client, 22))(sd.client);
}

/*
The wrappers. On 64-bit Windows a method gets its object as the first argument, as a plain
function would, so each one only fetches its slot.
*/
#define SD_VOID(name, slot, params, args) \
    static void QDECL SD_##name params { ((void (*) params)SD_VT(self, slot)) args; }
#define SD_RET(type, name, slot, params, args) \
    static type QDECL SD_##name params { return ((type (*) params)SD_VT(self, slot)) args; }

// ISteamGameServer012
SD_VOID(SetProduct, 1, (void* self, const char* a), (self, a))
SD_VOID(SetGameDescription, 2, (void* self, const char* a), (self, a))
SD_VOID(SetModDir, 3, (void* self, const char* a), (self, a))
SD_VOID(SetDedicatedServer, 4, (void* self, steamBool_t a), (self, a))
SD_VOID(LogOn, 5, (void* self, const char* a), (self, a))
SD_VOID(LogOnAnonymous, 6, (void* self), (self))
SD_RET(steamBool_t, BLoggedOn, 8, (void* self), (self))
SD_RET(steamBool_t, BSecure, 9, (void* self), (self))
SD_VOID(SetMaxPlayerCount, 12, (void* self, int a), (self, a))
SD_VOID(SetBotPlayerCount, 13, (void* self, int a), (self, a))
SD_VOID(SetServerName, 14, (void* self, const char* a), (self, a))
SD_VOID(SetMapName, 15, (void* self, const char* a), (self, a))
SD_VOID(SetPasswordProtected, 16, (void* self, steamBool_t a), (self, a))
SD_VOID(SetKeyValue, 20, (void* self, const char* a, const char* b), (self, a, b))
SD_VOID(SetGameTags, 21, (void* self, const char* a), (self, a))
SD_RET(steamBool_t, BUpdateUserData, 27, (void* self, uint64_t a, const char* b, unsigned int c), (self, a, b, c))
SD_RET(int, BeginAuthSession, 29, (void* self, const void* a, int b, uint64_t c), (self, a, b, c))
SD_VOID(EndAuthSession, 30, (void* self, uint64_t a), (self, a))
SD_RET(unsigned int, GetPublicIP, 36, (void* self), (self))
SD_RET(int, GetNextOutgoingPacket, 38, (void* self, void* a, int b, unsigned int* c, unsigned short* d),
       (self, a, b, c, d))
SD_VOID(EnableHeartbeats, 39, (void* self, steamBool_t a), (self, a))

// the method answers a bool: only its low byte means anything
static int QDECL SD_HandleIncomingPacket(void* self, const void* data, int len, unsigned int ip,
                                         unsigned short port) {
    return ((steamBool_t(*)(void*, const void*, int, unsigned int, unsigned short))SD_VT(self, 37))(
               self, data, len, ip, port)
               ? 1
               : 0;
}

/*
A method that answers a CSteamID: MSVC hands a class back through a pointer that goes right
after the object, not in a register, so these three are spelled out.
*/
static uint64_t QDECL SD_GetSteamID(void* self) {
    uint64_t id = 0;

    ((void* (*)(void*, uint64_t*))SD_VT(self, 10))(self, &id);  // ISteamGameServer012
    return id;
}

static uint64_t QDECL SD_GetUserSteamID(void* self) {
    uint64_t id = 0;

    ((void* (*)(void*, uint64_t*))SD_VT(self, 2))(self, &id);  // ISteamUser018
    return id;
}

static uint64_t QDECL SD_GetFriendByIndex(void* self, int index, int flags) {
    uint64_t id = 0;

    ((void* (*)(void*, uint64_t*, int, int))SD_VT(self, 4))(self, &id, index, flags);  // ISteamFriends015
    return id;
}

// ISteamMatchmakingServers002
SD_RET(void*, RequestInternetServerList, 0,
       (void* self, unsigned int a, steamKeyValue_t** b, unsigned int c, void* d), (self, a, b, c, d))
SD_RET(void*, RequestLANServerList, 1, (void* self, unsigned int a, void* b), (self, a, b))
SD_RET(void*, RequestFriendsServerList, 2,
       (void* self, unsigned int a, steamKeyValue_t** b, unsigned int c, void* d), (self, a, b, c, d))
SD_RET(void*, RequestFavoritesServerList, 3,
       (void* self, unsigned int a, steamKeyValue_t** b, unsigned int c, void* d), (self, a, b, c, d))
SD_RET(void*, RequestHistoryServerList, 4,
       (void* self, unsigned int a, steamKeyValue_t** b, unsigned int c, void* d), (self, a, b, c, d))
SD_VOID(ReleaseRequest, 6, (void* self, void* a), (self, a))
SD_RET(void*, GetServerDetails, 7, (void* self, void* a, int b), (self, a, b))
SD_VOID(CancelQuery, 8, (void* self, void* a), (self, a))
SD_VOID(RefreshQuery, 9, (void* self, void* a), (self, a))
SD_RET(steamBool_t, IsRefreshing, 10, (void* self, void* a), (self, a))
SD_RET(int, GetServerCount, 11, (void* self, void* a), (self, a))
SD_RET(int, PlayerDetails, 14, (void* self, unsigned int a, unsigned short b, void* c), (self, a, b, c))
SD_RET(int, ServerRules, 15, (void* self, unsigned int a, unsigned short b, void* c), (self, a, b, c))
SD_VOID(CancelServerQuery, 16, (void* self, int a), (self, a))

// ISteamMatchmaking009
SD_RET(int, AddFavoriteGame, 2,
       (void* self, unsigned int a, unsigned int b, unsigned short c, unsigned short d, unsigned int e,
        unsigned int f),
       (self, a, b, c, d, e, f))
SD_RET(steamBool_t, RemoveFavoriteGame, 3,
       (void* self, unsigned int a, unsigned int b, unsigned short c, unsigned short d, unsigned int e),
       (self, a, b, c, d, e))

// ISteamUGC007
SD_RET(uint64_t, SubscribeItem, 48, (void* self, uint64_t a), (self, a))
SD_RET(uint64_t, UnsubscribeItem, 49, (void* self, uint64_t a), (self, a))
SD_RET(unsigned int, GetNumSubscribedItems, 50, (void* self), (self))
SD_RET(unsigned int, GetSubscribedItems, 51, (void* self, uint64_t* a, unsigned int b), (self, a, b))
SD_RET(unsigned int, GetItemState, 52, (void* self, uint64_t a), (self, a))
SD_RET(steamBool_t, GetItemInstallInfo, 53,
       (void* self, uint64_t a, uint64_t* b, char* c, unsigned int d, unsigned int* e), (self, a, b, c, d, e))
SD_RET(steamBool_t, GetItemDownloadInfo, 54, (void* self, uint64_t a, uint64_t* b, uint64_t* c), (self, a, b, c))
SD_RET(steamBool_t, DownloadItem, 55, (void* self, uint64_t a, steamBool_t b), (self, a, b))

// ISteamNetworking005, the client's and the server's
SD_RET(steamBool_t, SendP2PPacket, 0,
       (void* self, uint64_t a, const void* b, unsigned int c, int d, int e), (self, a, b, c, d, e))
SD_RET(steamBool_t, IsP2PPacketAvailable, 1, (void* self, unsigned int* a, int b), (self, a, b))
SD_RET(steamBool_t, ReadP2PPacket, 2,
       (void* self, void* a, unsigned int b, unsigned int* c, uint64_t* d, int e), (self, a, b, c, d, e))
SD_RET(steamBool_t, AcceptP2PSessionWithUser, 3, (void* self, uint64_t a), (self, a))

// ISteamUser018
SD_VOID(StartVoiceRecording, 7, (void* self), (self))
SD_VOID(StopVoiceRecording, 8, (void* self), (self))
SD_RET(int, GetVoice, 10,
       (void* self, steamBool_t a, void* b, unsigned int c, unsigned int* d, steamBool_t e, void* f,
        unsigned int g, unsigned int* h, unsigned int i),
       (self, a, b, c, d, e, f, g, h, i))
SD_RET(int, DecompressVoice, 11,
       (void* self, const void* a, unsigned int b, void* c, unsigned int d, unsigned int* e, unsigned int f),
       (self, a, b, c, d, e, f))
SD_RET(unsigned int, GetVoiceOptimalSampleRate, 12, (void* self), (self))
SD_RET(unsigned int, GetAuthSessionTicket, 13, (void* self, void* a, int b, unsigned int* c), (self, a, b, c))
SD_VOID(CancelAuthTicket, 16, (void* self, unsigned int a), (self, a))

// ISteamFriends015
SD_RET(int, GetFriendCount, 3, (void* self, int a), (self, a))
SD_RET(int, GetFriendPersonaState, 6, (void* self, uint64_t a), (self, a))
SD_RET(const char*, GetFriendPersonaName, 7, (void* self, uint64_t a), (self, a))
SD_RET(int, GetMediumFriendAvatar, 35, (void* self, uint64_t a), (self, a))

// ISteamUtils007
SD_RET(steamBool_t, GetImageSize, 5, (void* self, int a, unsigned int* b, unsigned int* c), (self, a, b, c))
SD_RET(steamBool_t, GetImageRGBA, 6, (void* self, int a, unsigned char* b, int c), (self, a, b, c))
SD_RET(steamBool_t, GetFriendGamePlayed, 8, (void* self, uint64_t a, void* b), (self, a, b))
// [QL] 0x00433837 reads the nickname through +0x2c and the rich presence through +0xb4
SD_RET(const char*, GetPlayerNickname, 11, (void* self, uint64_t a), (self, a))
SD_RET(const char*, GetFriendRichPresence, 45, (void* self, uint64_t a, const char* b), (self, a, b))
SD_VOID(SetInGameVoiceSpeaking, 27, (void* self, uint64_t a, steamBool_t b), (self, a, b))
SD_VOID(ActivateGameOverlay, 28, (void* self, const char* a), (self, a))
SD_VOID(ActivateGameOverlayToUser, 29, (void* self, const char* a, uint64_t b), (self, a, b))
// [QL] 0x004325e6 calls it through +0xc4
SD_RET(steamBool_t, InviteUserToGame, 49, (void* self, uint64_t a, const char* b), (self, a, b))
SD_VOID(SetRichPresence, 43, (void* self, const char* a, const char* b), (self, a, b))

// ISteamGameServerStats001, the slots Quake Live's steam_api.dll calls (the int32 overloads are
// 2 and 5: MSVC puts each pair the other way round)
SD_RET(uint64_t, RequestUserStats, 0, (void* self, uint64_t a), (self, a))
SD_RET(steamBool_t, GetUserStat, 2, (void* self, uint64_t a, const char* b, int* c), (self, a, b, c))
SD_RET(steamBool_t, SetUserStat, 5, (void* self, uint64_t a, const char* b, int c), (self, a, b, c))
SD_RET(steamBool_t, SetUserAchievement, 7, (void* self, uint64_t a, const char* b), (self, a, b))
SD_RET(uint64_t, StoreUserStats, 9, (void* self, uint64_t a), (self, a))

// ISteamUserStats011
SD_RET(steamBool_t, RequestCurrentStats, 0, (void* self), (self))
SD_RET(steamBool_t, SetAchievement, 7, (void* self, const char* a), (self, a))
SD_RET(steamBool_t, StoreStats, 10, (void* self), (self))
// [QL] 0x0045ffd0 reads a player's stats through slot 18, the int32 GetUserStat, for the same
// reason as above; RequestUserStats is 16
SD_RET(uint64_t, UserRequestUserStats, 16, (void* self, uint64_t a), (self, a))
SD_RET(steamBool_t, UserGetUserStat, 18, (void* self, uint64_t a, const char* b, int* c), (self, a, b, c))

// the accessors, which steam_api answers from its context
static void* QDECL SD_GameServer(void) {
    return sd.gameServer;
}
static void* QDECL SD_Matchmaking(void) {
    return sd.matchmaking;
}
static void* QDECL SD_Lobbies(void) {
    return sd.lobbies;
}
static void* QDECL SD_Utils(void) {
    return sd.utils;
}
static void* QDECL SD_UGC(void) {
    return sd.ugc;
}
static void* QDECL SD_GameServerUGC(void) {
    return sd.gameServerUGC;
}
static void* QDECL SD_Networking(void) {
    return sd.networking;
}
static void* QDECL SD_GameServerNetworking(void) {
    return sd.gameServerNetworking;
}
static void* QDECL SD_User(void) {
    return sd.user;
}
static void* QDECL SD_Friends(void) {
    return sd.friends;
}
static void* QDECL SD_GameServerStats(void) {
    return sd.gameServerStats;
}
static void* QDECL SD_UserStats(void) {
    return sd.userStats;
}

/*
===============
SD_API_Init

SteamAPI_Init: a pipe to the Steam client that is running and its logged on user. Without a
running Steam there is no user, and the client goes on without Steam, as with steam_api.
===============
*/
static steamBool_t QDECL SD_API_Init(void) {
    if (sd.clientUser) {
        return 1;
    }
    sd.clientPipe = SD_CreateSteamPipe();
    if (!sd.clientPipe) {
        return 0;
    }
    sd.clientUser = SD_ConnectToGlobalUser(sd.clientPipe);
    if (!sd.clientUser || !SD_AppID(sd.clientPipe)) {
        if (sd.clientUser) {
            SD_ReleaseUser(sd.clientPipe, sd.clientUser);
        }
        SD_ReleaseSteamPipe(sd.clientPipe);
        sd.clientPipe = sd.clientUser = 0;
        return 0;
    }
    sd.user = SD_Interface(5, sd.clientUser, sd.clientPipe, "SteamUser018");
    sd.friends = SD_Interface(8, sd.clientUser, sd.clientPipe, "SteamFriends015");
    sd.lobbies = SD_Interface(10, sd.clientUser, sd.clientPipe, "SteamMatchMaking009");
    sd.matchmaking = SD_Interface(11, sd.clientUser, sd.clientPipe, "SteamMatchMakingServers002");
    sd.networking = SD_Interface(16, sd.clientUser, sd.clientPipe, "SteamNetworking005");
    sd.ugc = SD_Interface(26, sd.clientUser, sd.clientPipe, "STEAMUGC_INTERFACE_VERSION007");
    sd.userStats = SD_Interface(13, sd.clientUser, sd.clientPipe, "STEAMUSERSTATS_INTERFACE_VERSION011");
    sd.utils = ((void* (*)(void*, int, const char*))SD_VT(sd.client, 9))(sd.client, sd.clientPipe, "SteamUtils007");
    return 1;
}

static void QDECL SD_API_Shutdown(void) {
    if (sd.clientUser) {
        SD_ReleaseUser(sd.clientPipe, sd.clientUser);
    }
    if (sd.clientPipe) {
        SD_ReleaseSteamPipe(sd.clientPipe);
    }
    sd.clientPipe = sd.clientUser = 0;
    sd.user = sd.friends = sd.lobbies = sd.matchmaking = sd.networking = sd.ugc = sd.utils = NULL;
    sd.userStats = NULL;
    SD_ShutdownIfAllPipesClosed();
}

/*
===============
SD_GameServer_Init

SteamGameServer_Init, as Quake Live's steam_api.dll does it (0x3b407730): the address to bind,
a user of its own of the game server kind, the interfaces, and InitGameServer with the flags
the server mode stands for. A game server needs no running Steam client.
===============
*/
static steamBool_t QDECL SD_GameServer_Init(unsigned int ip, unsigned short steamPort, unsigned short gamePort,
                                            unsigned short queryPort, int serverMode, const char* version) {
    unsigned int flags, appId;

    if (sd.serverUser) {
        return 1;
    }
    SD_SetLocalIPBinding(ip, steamPort);
    sd.serverPipe = 0;
    sd.serverUser = SD_CreateLocalUser(&sd.serverPipe, SD_ACCOUNT_GAMESERVER);
    if (!sd.serverUser) {
        return 0;
    }
    sd.gameServer = SD_Interface(6, sd.serverUser, sd.serverPipe, "SteamGameServer012");
    sd.gameServerNetworking = SD_Interface(16, sd.serverUser, sd.serverPipe, "SteamNetworking005");
    sd.gameServerStats = SD_Interface(14, sd.serverUser, sd.serverPipe, "SteamGameServerStats001");
    sd.gameServerUGC = SD_Interface(26, sd.serverUser, sd.serverPipe, "STEAMUGC_INTERFACE_VERSION007");

    flags = serverMode == 3 ? SD_FLAG_SECURE : 0;
    if (serverMode == 1) {
        flags |= SD_FLAG_PRIVATE;
    }
    appId = SD_AppID(sd.serverPipe);
    if (!sd.gameServer || !appId) {
        return 0;
    }
    // InitGameServer(ip, game port, query port, flags, App ID, version), slot 0
    return ((steamBool_t(*)(void*, unsigned int, unsigned short, unsigned short, unsigned int, unsigned int,
                            const char*))SD_VT(sd.gameServer, 0))(sd.gameServer, ip, gamePort, queryPort, flags,
                                                                  appId, version);
}

static void QDECL SD_GameServer_Shutdown(void) {
    if (sd.serverUser) {
        SD_ReleaseUser(sd.serverPipe, sd.serverUser);
    }
    if (sd.serverPipe) {
        SD_ReleaseSteamPipe(sd.serverPipe);
    }
    sd.serverPipe = sd.serverUser = 0;
    sd.gameServer = sd.gameServerNetworking = sd.gameServerStats = sd.gameServerUGC = NULL;
    SD_ShutdownIfAllPipesClosed();
}

/*
===============
SD_RegisterCallback / SD_Dispatch

What steam_api's callback registry does: each callback object says which callback it is and
whether it is the server's (flag 0x02), and each one that arrives on a pipe goes to its Run(data),
the second slot of the object's table as MSVC orders it.
===============
*/
static void QDECL SD_RegisterCallback(void* callback, int id) {
    int i;

    (void)id;  // the object carries it
    for (i = 0; i < sd.numCallbacks; i++) {
        if (sd.callbacks[i] == callback) {
            return;
        }
    }
    if (sd.numCallbacks < SD_MAX_CALLBACKS) {
        sd.callbacks[sd.numCallbacks++] = (steamCallback_t*)callback;
    }
}

static void QDECL SD_UnregisterCallback(void* callback) {
    int i;

    for (i = 0; i < sd.numCallbacks; i++) {
        if (sd.callbacks[i] == callback) {
            sd.callbacks[i] = sd.callbacks[--sd.numCallbacks];
            return;
        }
    }
}

static void SD_Dispatch(int pipe, qboolean server) {
    steamCallbackMsg_t msg;
    int call;
    int i;

    if (!pipe) {
        return;
    }
    while (sd.BGetCallback(pipe, &msg, &call)) {
        for (i = 0; i < sd.numCallbacks; i++) {
            steamCallback_t* cb = sd.callbacks[i];

            if (cb->id == msg.id && ((cb->flags & SD_CB_GAMESERVER) != 0) == (server != qfalse)) {
                ((void (*)(void*, void*))cb->vtable[1])(cb, msg.param);
            }
        }
        sd.FreeLastCallback(pipe);
    }
}

/*
Quake Live's SteamAPI_RunCallbacks (0x3b407260) runs ISteamUtils007::RunFrame, slot 14, before
anything else: it is what works through the results of a server list, which otherwise stays
empty and refreshing forever.
*/
static void QDECL SD_API_RunCallbacks(void) {
    if (sd.utils) {
        ((void (*)(void*))SD_VT(sd.utils, 14))(sd.utils);
    }
    SD_Dispatch(sd.clientPipe, qfalse);
}

static void QDECL SD_GameServer_RunCallbacks(void) {
    SD_Dispatch(sd.serverPipe, qtrue);
}

/*
===============
SD_Load

steamclient64.dll: next to our own executable first, then the one of the player's Steam client,
which Steam writes down in the registry while it runs, and last the Steam folder itself.
===============
*/
static qboolean SD_RegistryString(const char* subkey, const char* value, char* out, DWORD size) {
    HKEY key;
    DWORD type = 0;
    LONG r;

    out[0] = '\0';
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return qfalse;
    }
    r = RegQueryValueExA(key, value, NULL, &type, (LPBYTE)out, &size);
    RegCloseKey(key);
    return (qboolean)(r == ERROR_SUCCESS && type == REG_SZ && out[0]);
}

static qboolean SD_Load(void) {
    char path[MAX_OSPATH];
    char dir[MAX_OSPATH];

    if (sd.client) {
        return qtrue;
    }
    sd.lib = LoadLibraryA("steamclient64.dll");
    if (!sd.lib && SD_RegistryString("Software\\Valve\\Steam\\ActiveProcess", "SteamClientDll64", path,
                                     sizeof(path))) {
        sd.lib = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if (!sd.lib && SD_RegistryString("Software\\Valve\\Steam", "SteamPath", dir, sizeof(dir))) {
        Com_sprintf(path, sizeof(path), "%s\\steamclient64.dll", dir);
        sd.lib = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if (!sd.lib) {
        return qfalse;
    }
    *(void**)&sd.CreateInterface = (void*)GetProcAddress(sd.lib, "CreateInterface");
    *(void**)&sd.BGetCallback = (void*)GetProcAddress(sd.lib, "Steam_BGetCallback");
    *(void**)&sd.FreeLastCallback = (void*)GetProcAddress(sd.lib, "Steam_FreeLastCallback");
    if (sd.CreateInterface && sd.BGetCallback && sd.FreeLastCallback) {
        sd.client = sd.CreateInterface("SteamClient017", NULL);
    }
    if (!sd.client) {
        Com_Printf("Steam: steamclient64.dll does not offer SteamClient017\n");
        FreeLibrary(sd.lib);
        Com_Memset(&sd, 0, sizeof(sd));
        return qfalse;
    }
    return qtrue;
}

// every entry point the rest of this file uses, pointed at the wrappers above
static void SD_Bind(void) {
#define SD_SET(field) *(void**)&st.field = (void*)SD_##field
    SD_SET(GameServer_Init);
    SD_SET(GameServer);
    SD_SET(GameServer_RunCallbacks);
    SD_SET(GameServer_Shutdown);
    SD_SET(API_Init);
    SD_SET(API_RunCallbacks);
    SD_SET(API_Shutdown);
    SD_SET(SetProduct);
    SD_SET(SetModDir);
    SD_SET(SetGameDescription);
    SD_SET(SetDedicatedServer);
    SD_SET(LogOn);
    SD_SET(LogOnAnonymous);
    SD_SET(EnableHeartbeats);
    SD_SET(SetMaxPlayerCount);
    SD_SET(SetBotPlayerCount);
    SD_SET(SetServerName);
    SD_SET(SetMapName);
    SD_SET(SetPasswordProtected);
    SD_SET(BLoggedOn);
    SD_SET(BSecure);
    SD_SET(GetPublicIP);
    SD_SET(HandleIncomingPacket);
    SD_SET(GetNextOutgoingPacket);
    SD_SET(SetGameTags);
    SD_SET(SetKeyValue);
    SD_SET(BUpdateUserData);
    SD_SET(Matchmaking);
    SD_SET(RequestInternetServerList);
    SD_SET(RequestLANServerList);
    SD_SET(RequestFriendsServerList);
    SD_SET(RequestFavoritesServerList);
    SD_SET(RequestHistoryServerList);
    SD_SET(GetServerDetails);
    SD_SET(GetServerCount);
    SD_SET(ReleaseRequest);
    SD_SET(IsRefreshing);
    SD_SET(CancelQuery);
    SD_SET(RefreshQuery);
    SD_SET(ServerRules);
    SD_SET(PlayerDetails);
    SD_SET(CancelServerQuery);
    SD_SET(Lobbies);
    SD_SET(AddFavoriteGame);
    SD_SET(RemoveFavoriteGame);
    SD_SET(UGC);
    SD_SET(GameServerUGC);
    SD_SET(SubscribeItem);
    SD_SET(UnsubscribeItem);
    SD_SET(Utils);
    SD_SET(GetMediumFriendAvatar);
    SD_SET(GetImageSize);
    SD_SET(GetImageRGBA);
    SD_SET(GetItemState);
    SD_SET(DownloadItem);
    SD_SET(GetItemInstallInfo);
    SD_SET(GetItemDownloadInfo);
    SD_SET(GetNumSubscribedItems);
    SD_SET(GetSubscribedItems);
    SD_SET(GetSteamID);
    SD_SET(Networking);
    SD_SET(GameServerNetworking);
    SD_SET(User);
    SD_SET(Friends);
    SD_SET(SendP2PPacket);
    SD_SET(IsP2PPacketAvailable);
    SD_SET(ReadP2PPacket);
    SD_SET(AcceptP2PSessionWithUser);
    SD_SET(StartVoiceRecording);
    SD_SET(StopVoiceRecording);
    SD_SET(GetVoice);
    SD_SET(DecompressVoice);
    SD_SET(GetVoiceOptimalSampleRate);
    SD_SET(GetUserSteamID);
    SD_SET(GetAuthSessionTicket);
    SD_SET(CancelAuthTicket);
    SD_SET(BeginAuthSession);
    SD_SET(EndAuthSession);
    SD_SET(SetInGameVoiceSpeaking);
    SD_SET(SetRichPresence);
    SD_SET(ActivateGameOverlayToUser);
    SD_SET(ActivateGameOverlay);
    SD_SET(InviteUserToGame);
    SD_SET(GetFriendCount);
    SD_SET(GetFriendByIndex);
    SD_SET(GetFriendPersonaName);
    SD_SET(GetFriendPersonaState);
    SD_SET(GetFriendGamePlayed);
    SD_SET(GetPlayerNickname);
    SD_SET(GetFriendRichPresence);
    SD_SET(RegisterCallback);
    SD_SET(UnregisterCallback);
    SD_SET(GameServerStats);
    SD_SET(UserStats);
    SD_SET(RequestUserStats);
    SD_SET(GetUserStat);
    SD_SET(SetUserStat);
    SD_SET(SetUserAchievement);
    SD_SET(StoreUserStats);
    SD_SET(RequestCurrentStats);
    SD_SET(SetAchievement);
    SD_SET(StoreStats);
    SD_SET(UserRequestUserStats);
    SD_SET(UserGetUserStat);
#undef SD_SET
}
#endif  // 64-bit Windows

/*
===============
Steam_Load

The library comes from the player's own Quake Live installation, which the file
system already located for pak00.pk3 (fs_steampath). Next to our own executable
first, so a player who drops one there wins.
===============
*/
static qboolean Steam_Load(void) {
#if !defined(_WIN32) || !defined(_WIN64)
    const char* steamPath;
    char path[MAX_OSPATH];
#endif

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
#if defined(_WIN32) && defined(_WIN64)
    // no steam_api here: see "Steam on 64-bit Windows"
    if (!SD_Load()) {
        Com_Printf("Steam: steamclient64.dll not found; running without Steam\n");
        return qfalse;
    }
    st.lib = (void*)sd.lib;
    SD_Bind();
    return qtrue;
#else
#ifdef _WIN32
    st.lib = (void*)LoadLibraryA(STEAM_LIB);
#else
    st.lib = dlopen("./" STEAM_LIB, RTLD_NOW);
#endif
    if (!st.lib) {
        // the client's Steam comes up before the filesystem creates fs_steampath
        steamPath = Cvar_VariableString("fs_steampath");
        if (!steamPath[0]) {
            steamPath = Sys_SteamPath();
        }
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
#endif
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
    // a new ticket cancels the one this client handed a server when it connected, and Steam then
    // tells that server so: it drops the player ("Issuer canceled auth ticket")
    if (st.clientUp && st.authTicket) {
        Com_Printf("ticket:      one in use, handed to the server this client joined\n");
    } else if (st.clientUp) {
        static byte ticket[512];
        int len = Steam_AuthTicket(ticket, sizeof(ticket));

        if (len <= 0) {
            Com_Printf("ticket:      Steam gave none\n");
        } else {
            Com_Printf("ticket:      %i bytes\n", len);
            if (st.serverUp) {
                uint64_t me = Steam_ClientSteamID();
                qboolean ok = Steam_GS_BeginAuth(me, ticket, len);

                Com_Printf("BeginAuth:   %s\n", ok ? "accepted" : "refused");
                Steam_GS_EndAuth(me);
            }
        }
        // the test ticket goes, so a live one always means a connection's
        if (st.authTicket && st.CancelAuthTicket) {
            st.CancelAuthTicket(st.user, st.authTicket);
            st.authTicket = 0;
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
    if (st.Lobbies) {
        st.lobbies = st.Lobbies();  // the favourites live here
    }
    // [QL] the player's Workshop. Retail takes it with the server's Steam (0x46700e), after the
    // filesystem's first start, and then restarts the filesystem to mount it; taken here, the
    // first start already mounts it and the second is not needed
    if (st.UGC) {
        st.ugc = st.UGC();
    }
    Steam_WorkshopListen(qfalse);
    if (st.Utils) {
        st.utils = st.Utils();  // where the avatars' pixels come from
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
    // [ioquakelive] what a game of one's own gives goes to the player's own stats, which Steam
    // only takes once they are loaded
    if (st.UserStats) {
        st.userStats = st.UserStats();
    }
    if (st.userStats && st.RequestCurrentStats) {
        st.RequestCurrentStats(st.userStats);
    }
    Steam_P2PListen(qfalse);
    Steam_UserStatsListen();
    // [QL] 0x00461565: "+voice" and "-voice" are registered on the client side, which is also
    // the side that has to tell the cgame somebody is talking
    Steam_SetRichPresence("At the main menu");    // [QL] 0x00461556
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

    // retail reads net_ip and net_port here and hands Steam the address it binds. Before the
    // network is up net_ip is still empty, which is every interface: nothing to resolve
    if (Cvar_VariableString("net_ip")[0] && NET_StringToAdr(Cvar_VariableString("net_ip"), &adr, NA_IP)) {
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
    // [QL] qzeroded.x64 0x4474e6: a dedicated server downloads with its own Workshop interface
    if (Cvar_VariableIntegerValue("dedicated") > 0) {
        st.ugc = st.GameServerUGC ? st.GameServerUGC() : NULL;
        Steam_WorkshopListen(qtrue);
    }
    if (st.GameServerNetworking) {
        st.gameServerNetworking = st.GameServerNetworking();  // what it hands the voice out over
        Steam_P2PListen(qtrue);
    }
    Steam_AuthListen();  // whatever Steam says about each ticket
    Steam_ServersListen();  // whether it logs on
    if (st.GameServerStats) {
        st.gameServerStats = st.GameServerStats();
    }
    Steam_StatsListen();  // the players' stats, when they arrive and when they are stored
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

/*
===============
The server's entry in the list

[QL] qzeroded.x64 0x4468a0 keeps it up to date. The server's side of it, what it reads
from the players and the cvars, is SV_SteamFrame; these hand each piece to Steam.
===============
*/
void Steam_GS_SetPassword(qboolean needPassword) {
    if (!st.serverUp) {
        return;
    }
    st.SetPasswordProtected(st.gameServer, (steamBool_t)(needPassword != qfalse));
}

void Steam_GS_SetName(const char* name) {
    if (!st.serverUp) {
        return;
    }
    st.SetServerName(st.gameServer, name);
}

void Steam_GS_SetMap(const char* map) {
    if (!st.serverUp) {
        return;
    }
    st.SetMapName(st.gameServer, map);
}

void Steam_GS_SetBotCount(int bots) {
    if (!st.serverUp) {
        return;
    }
    st.SetBotPlayerCount(st.gameServer, bots);
}

void Steam_GS_SetKeyValue(const char* key, const char* value) {
    if (!st.serverUp || !st.SetKeyValue) {
        return;
    }
    st.SetKeyValue(st.gameServer, key, value ? value : "");
}

// a player as the browser lists it: the name, the score and, counted by Steam, the time
void Steam_GS_UpdateUser(uint64_t steamId, const char* name, int score) {
    if (!st.serverUp || !st.BUpdateUserData) {
        return;
    }
    st.BUpdateUserData(st.gameServer, steamId, name, (unsigned int)score);
}

/*
===============
Steam_GS_SetKeyValues

[QL] qzeroded.x64 0x447080. Every key of an info string as one of the server's rules,
which is what the browser shows about a match: retail hands over the serverinfo cvars
whenever they change.
===============
*/
void Steam_GS_SetKeyValues(const char* info) {
    static char key[BIG_INFO_KEY];
    static char value[BIG_INFO_VALUE];

    if (!st.serverUp || !st.SetKeyValue || !info) {
        return;
    }
    for (;;) {
        Info_NextPair(&info, key, value);
        if (!key[0]) {
            break;
        }
        st.SetKeyValue(st.gameServer, key, value);
    }
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
    if (com_sv_running && com_sv_running->integer) {
        SV_SteamFrame(qfalse);
    }

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
        if (st.serversConnected.vtable) {
            st.UnregisterCallback(&st.serversConnected);
            st.UnregisterCallback(&st.serversFailed);
            st.UnregisterCallback(&st.serversDisconnected);
            st.serversConnected.vtable = NULL;
        }
        if (st.itemInstalled.vtable) {
            st.UnregisterCallback(&st.itemInstalled);
            st.UnregisterCallback(&st.downloadResult);
            st.itemInstalled.vtable = NULL;
        }
        if (st.userStatsReceived.vtable) {
            st.UnregisterCallback(&st.userStatsReceived);
            st.userStatsReceived.vtable = NULL;
        }
        if (st.statsReceived.vtable) {
            st.UnregisterCallback(&st.statsReceived);
            st.UnregisterCallback(&st.statsStored);
            st.statsReceived.vtable = NULL;
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
        st.gameServerStats = NULL;
        st.loggedOn = qfalse;
        st.numTracked = 0;
    }
    if (st.clientUp) {
        st.API_Shutdown();
        st.clientUp = qfalse;
    }
    st.ugc = NULL;
}

/*
===============================================================================

THE SERVER LIST

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

// the cvars the tags are made of, found as retail finds them: one that does not exist is skipped
static cvar_t* Steam_Cvar(const char* name) {
    return (cvar_t*)Cvar_Find(name);
}

static void Steam_TagsAdd(char* tags, int size, const char* tag) {
    Q_strcat(tags, size, tag);
}

/*
===============
Steam_GS_UpdateGameInfo

[QL] qzeroded.x64 0x446abc, the end of the list update. What the browser shows about
this server beyond the name and the map: what is being played and under which rules,
followed by the tags the server's owner adds in sv_tags and the factory's own. It is
only rebuilt when a cvar it reads changed, or when forced.

The description waits for a gametype that only comes in with the next map.
===============
*/
void Steam_GS_UpdateGameInfo(qboolean force, char* const* factoryTags, int numFactoryTags) {
    static int gametypeCount = -1;
    cvar_t* gametypeVar;
    cvar_t* watched[7];
    cvar_t* cheats;
    cvar_t* instagib;
    cvar_t* gravity;
    cvar_t* vampiric;
    cvar_t* infected;
    cvar_t* quadHog;
    cvar_t* extra;
    char tags[1024];
    qboolean gametypeChanged;
    int gametype;
    int len;
    int i;

    if (!st.serverUp || !st.SetGameTags) {
        return;
    }

    watched[0] = cheats = Steam_Cvar("sv_cheats");
    watched[1] = instagib = Steam_Cvar("g_instagib");
    watched[2] = gravity = Steam_Cvar("g_gravity");
    watched[3] = vampiric = Steam_Cvar("g_vampiricDamage");
    watched[4] = infected = Steam_Cvar("g_rrInfected");
    watched[5] = quadHog = Steam_Cvar("g_quadhog");
    watched[6] = extra = Steam_Cvar("sv_tags");
    gametypeVar = Steam_Cvar("g_gametype");

    // retail looks at g_gametype's modified flag and leaves it set; the count says the same
    // without touching the flag
    gametypeChanged = (qboolean)(gametypeVar && gametypeVar->modificationCount != gametypeCount);
    if (gametypeVar) {
        gametypeCount = gametypeVar->modificationCount;
    }

    gametype = (int)Cvar_VariableValue("g_gametype");
    if (gametypeVar && (force || (gametypeChanged && !gametypeVar->latchedString))) {
        st.SetGameDescription(st.gameServer, (unsigned int)gametype < STEAM_NUM_GAMETYPES
                                                 ? steamGametypeNames[gametype]
                                                 : "Unknown Gametype");
    }
    if (!force && !gametypeChanged) {
        for (i = 0; i < (int)ARRAY_LEN(watched); i++) {
            if (watched[i] && watched[i]->modified) {
                break;
            }
        }
        if (i == (int)ARRAY_LEN(watched)) {
            return;
        }
    }

    Com_sprintf(tags, sizeof(tags), "%s,",
                (unsigned int)gametype < STEAM_NUM_GAMETYPES ? steamGametypeTags[gametype] : "");
    if (cheats) {
        if (cheats->integer) {
            Steam_TagsAdd(tags, sizeof(tags), "cheats,");
        }
        cheats->modified = qfalse;
    }
    if (instagib) {
        if (instagib->integer) {
            Steam_TagsAdd(tags, sizeof(tags), "instagib,");
        }
        instagib->modified = qfalse;
    }
    if (gravity) {
        if (gravity->value > 800.0f) {
            Steam_TagsAdd(tags, sizeof(tags), "highgrav,");
        } else if (gravity->value < 800.0f) {
            Steam_TagsAdd(tags, sizeof(tags), "lowgrav,");
        }
        gravity->modified = qfalse;
    }
    if (vampiric) {
        if (vampiric->value > 0.0f) {
            Steam_TagsAdd(tags, sizeof(tags), "vampiric,");
        }
        vampiric->modified = qfalse;
    }
    // infected only counts in Red Rover, and quad hog only in free for all
    if (infected) {
        if (infected->integer && gametype == 12) {
            Steam_TagsAdd(tags, sizeof(tags), "infected,");
        }
        infected->modified = qfalse;
    }
    if (quadHog) {
        if (quadHog->integer && gametype == 0) {
            Steam_TagsAdd(tags, sizeof(tags), "quadhog,");
        }
        quadHog->modified = qfalse;
    }
    if (extra) {
        if (extra->string[0]) {
            Steam_TagsAdd(tags, sizeof(tags), extra->string);
            if (tags[strlen(tags) - 1] != ',') {
                Steam_TagsAdd(tags, sizeof(tags), ",");
            }
        }
        extra->modified = qfalse;
    }
    for (i = 0; factoryTags && i < numFactoryTags; i++) {
        if (factoryTags[i]) {
            Steam_TagsAdd(tags, sizeof(tags), factoryTags[i]);
            Steam_TagsAdd(tags, sizeof(tags), ",");
        }
    }

    // the trailing comma is not wanted
    len = (int)strlen(tags);
    if (len && tags[len - 1] == ',') {
        tags[len - 1] = '\0';
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
    st.numQueued = 0;
    Com_Memset(st.serverIndex, 0, sizeof(st.serverIndex));
    st.listRefreshing = qfalse;
}

// what Steam answers about a server, turned into what the browser needs
static void Steam_ServerListAdd(void* request, int index) {
    steamServerItem_t* item;
    steamServer_t* out;
    int ours;

    item = (steamServerItem_t*)st.GetServerDetails(st.matchmaking, request, index);
    if (!item || !item->hadResponse) {
        return;
    }

    // a server that answers again after a refresh keeps its place and waits for the client
    ours = (index >= 0 && index < STEAM_MAX_SERVERS) ? st.serverIndex[index] - 1 : -1;
    if (ours >= 0) {
        out = &st.servers[ours];
        if (!out->queued && st.numQueued < STEAM_MAX_SERVERS) {
            out->queued = qtrue;
            st.queued[st.numQueued++] = ours;
        }
    } else {
        if (st.numServers >= STEAM_MAX_SERVERS) {
            return;
        }
        if (index >= 0 && index < STEAM_MAX_SERVERS) {
            st.serverIndex[index] = st.numServers + 1;
        }
        out = &st.servers[st.numServers++];
        out->queued = qfalse;
    }
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
    int shown = 0;

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
    // [ioquakelive] "steamservers <text>": every server whose name has it, not just the first five
    for (count = 0; count < st.numServers; count++) {
        const steamServer_t* sv = &st.servers[count];

        if (Cmd_Argc() > 1 ? !Q_stristr(sv->name, Cmd_Argv(1)) : shown >= 5) {
            continue;
        }
        shown++;
        Com_Printf("  %-28s %-12s %2i/%-2i bots %i ping %3i  %s | %s | %i.%i.%i.%i:%i\n",
                   sv->name, sv->map, sv->players, sv->maxPlayers, sv->bots, sv->ping,
                   sv->description, sv->tags, (sv->ip >> 24) & 0xff, (sv->ip >> 16) & 0xff,
                   (sv->ip >> 8) & 0xff, sv->ip & 0xff, sv->port);
    }
}

qboolean Steam_ServerListRefreshing(void) {
    return st.listRefreshing;
}

/*
===============
Steam_RefreshServerList

The browser's Quick Refresh (retail's RefreshList): Steam asks every server already in the
list again, without looking for new ones. What answers comes back through
Steam_ServerListNextUpdated.
===============
*/
qboolean Steam_RefreshServerList(void) {
    if (!st.listRequest || !st.RefreshQuery || st.listRefreshing) {
        return qfalse;
    }
    st.listRefreshing = qtrue;
    st.RefreshQuery(st.matchmaking, st.listRequest);
    return qtrue;
}

// a server that answered again since the last call, or -1
int Steam_ServerListNextUpdated(void) {
    int ours;

    if (!st.numQueued) {
        return -1;
    }
    ours = st.queued[--st.numQueued];
    st.servers[ours].queued = qfalse;
    return ours;
}

/*
===============
A server's rules and players

What the browser shows about the server it has selected (retail's RequestServerDetails):
Steam asks the server itself, which answers with the keys it published (SetKeyValue) and the
players it listed (BUpdateUserData). The rules arrive one by one and end in a complete or a
failure; the players, the same.
===============
*/
static void Steam_DetailsAppend(char* buf, int size, int* len, const char* text) {
    int n;

    for (n = 0; text[n] && *len + 1 < size; n++) {
        char c = text[n];

        // the separators of each format and anything unprintable go out as spaces
        buf[(*len)++] = (c == '\\' || c == '\n' || c == '\t' || (unsigned char)c < ' ') ? ' ' : c;
    }
    buf[*len] = '\0';
}

static void STEAM_CB Steam_CB_RulesResponded(STEAM_CB_SELF, const char* rule, const char* value) {
    (void)self;
    if (!rule || !rule[0]) {
        return;
    }
    if (!value) {
        value = "";
    }
    if (st.rulesLen + (int)strlen(rule) + (int)strlen(value) + 3 >= STEAM_DETAILS_RULES) {
        return;
    }
    st.rules[st.rulesLen++] = '\\';
    Steam_DetailsAppend(st.rules, sizeof(st.rules), &st.rulesLen, rule);
    st.rules[st.rulesLen++] = '\\';
    st.rules[st.rulesLen] = '\0';
    Steam_DetailsAppend(st.rules, sizeof(st.rules), &st.rulesLen, value);
}

static void STEAM_CB Steam_CB_RulesFailed(STEAM_CB_SELF) {
    (void)self;
    st.rulesQuery = 0;
    st.detailsState = STEAM_DETAILS_FAILED;
}

static void STEAM_CB Steam_CB_RulesComplete(STEAM_CB_SELF) {
    (void)self;
    st.rulesQuery = 0;
    st.detailsState = STEAM_DETAILS_DONE;
}

// one line per player: the name, the score and the seconds on the server
static void STEAM_CB Steam_CB_PlayerAdded(STEAM_CB_SELF, const char* name, int score, float seconds) {
    char line[64];

    (void)self;
    if (st.playersLen + MAX_NAME_LENGTH + (int)sizeof(line) >= STEAM_DETAILS_PLAYERS) {
        return;
    }
    Steam_DetailsAppend(st.players, sizeof(st.players), &st.playersLen, name ? name : "");
    Com_sprintf(line, sizeof(line), "\t%i\t%i\n", score, (int)seconds);
    Q_strncpyz(st.players + st.playersLen, line, sizeof(st.players) - st.playersLen);
    st.playersLen += (int)strlen(st.players + st.playersLen);
}

static void STEAM_CB Steam_CB_PlayersFailed(STEAM_CB_SELF) {
    (void)self;
    st.playersQuery = 0;
    st.playersLen = 0;
    st.players[0] = '\0';
    st.playersDone = qtrue;
}

static void STEAM_CB Steam_CB_PlayersComplete(STEAM_CB_SELF) {
    (void)self;
    st.playersQuery = 0;
    st.playersDone = qtrue;
}

static void* steamRulesVTable[3];
static void* steamPlayersVTable[3];

// whatever is still going on is dropped: Steam must not answer into a request nobody reads
static void Steam_CancelDetails(void) {
    if (st.rulesQuery && st.CancelServerQuery) {
        st.CancelServerQuery(st.matchmaking, st.rulesQuery);
    }
    if (st.playersQuery && st.CancelServerQuery) {
        st.CancelServerQuery(st.matchmaking, st.playersQuery);
    }
    st.rulesQuery = 0;
    st.playersQuery = 0;
}

qboolean Steam_RequestServerDetails(unsigned int ip, unsigned short port) {
    if (!st.clientUp || !st.matchmaking || !st.ServerRules || !st.PlayerDetails) {
        return qfalse;
    }
    Steam_CancelDetails();

    steamRulesVTable[0] = (void*)Steam_CB_RulesResponded;
    steamRulesVTable[1] = (void*)Steam_CB_RulesFailed;
    steamRulesVTable[2] = (void*)Steam_CB_RulesComplete;
    st.rulesResponse.vtable = steamRulesVTable;
    steamPlayersVTable[0] = (void*)Steam_CB_PlayerAdded;
    steamPlayersVTable[1] = (void*)Steam_CB_PlayersFailed;
    steamPlayersVTable[2] = (void*)Steam_CB_PlayersComplete;
    st.playersResponse.vtable = steamPlayersVTable;

    st.rulesLen = 0;
    st.rules[0] = '\0';
    st.playersLen = 0;
    st.players[0] = '\0';
    st.playersDone = qfalse;
    st.detailsState = STEAM_DETAILS_LOADING;

    // Quake Live's servers answer queries on the game port (STEAM_QUERY_PORT_SHARED)
    st.rulesQuery = st.ServerRules(st.matchmaking, ip, port, &st.rulesResponse);
    st.playersQuery = st.PlayerDetails(st.matchmaking, ip, port, &st.playersResponse);
    return qtrue;
}

int Steam_ServerDetails(char* rules, int rulesSize, char* players, int playersSize) {
    if (rules && rulesSize > 0) {
        Q_strncpyz(rules, st.detailsState == STEAM_DETAILS_DONE ? st.rules : "", rulesSize);
    }
    if (players && playersSize > 0) {
        Q_strncpyz(players, st.playersDone ? st.players : "", playersSize);
    }
    return st.detailsState;
}

/*
===============
Steam_SetFavorite

Retail's SetFavoriteServer (0x00432736 removes): the favourites are Steam's own, the ones
its server browser shows, and the list's type 3 reads them back.
===============
*/
qboolean Steam_SetFavorite(unsigned int ip, unsigned short port, qboolean favorite) {
    unsigned int appId = (unsigned int)atoi(STEAM_APP_ID);

    if (!st.clientUp || !st.lobbies || !st.AddFavoriteGame || !st.RemoveFavoriteGame) {
        return qfalse;
    }
    if (favorite) {
        st.AddFavoriteGame(st.lobbies, appId, ip, port, port, 1, (unsigned int)time(NULL));
    } else {
        st.RemoveFavoriteGame(st.lobbies, appId, ip, port, port, 1);
    }
    return qtrue;
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
// a value for the info string: the backslash that separates its keys cannot be in one
static const char* Steam_InfoValue(const char* in, char* out, int size) {
    int n;

    for (n = 0; in[n] && n < size - 1; n++) {
        out[n] = in[n] == '\\' ? ' ' : in[n];
    }
    out[n] = '\0';
    return out;
}

// the server's name as Steam has it, which the info string can only carry cleaned
const char* Steam_ServerListName(int index) {
    if (index < 0 || index >= st.numServers) {
        return "";
    }
    return st.servers[index].name;
}

qboolean Steam_ServerListGet(int index, unsigned int* ip, unsigned short* port, int* ping, char* info,
                             int infoSize) {
    const steamServer_t* s;
    char firstTag[32];
    char name[64], map[32], desc[64], tags[128];
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
                "\\game\\%s\\gametype\\%i\\g_needpass\\%i\\nettype\\1\\sv_vac\\%i\\tags\\%s"
                "\\gamedesc\\%s",
                Steam_InfoValue(s->name, name, sizeof(name)), Steam_InfoValue(s->map, map, sizeof(map)),
                s->players, s->players - s->bots, s->maxPlayers, s->gameDir, gametype, s->password ? 1 : 0,
                s->secure ? 1 : 0, Steam_InfoValue(s->tags, tags, sizeof(tags)),
                Steam_InfoValue(s->description, desc, sizeof(desc)));
    return qtrue;
}

/*
===============================================================================

THE WORKSHOP

Quake Live does not download maps over HTTP: it downloads them from the Steam
Workshop, and both sides do it the same way ([QL] qzeroded.x64 0x44e300-0x44ec60,
quakelive_steam.exe 0x469260-0x469ab0). A queue of items, which Steam downloads one at
a time, and a table with the folders they were installed to, which the filesystem
mounts at every restart for the rest of the session (FS_AddWorkshop). The dedicated
server fills the queue from sv_workshopFile before it starts (SV_WorkshopInit); the
client, with the items the server it joins publishes in CS_STEAM_WORKSHOP_IDS
(CL_InitDownloads).

Steam says an item is done through ItemInstalled_t and DownloadItemResult_t,
k_iClientUGCCallbacks (3400) + 5 and + 6. Of its state flags only two matter here:
1 subscribed and 4 installed.

===============================================================================
*/

#define STEAM_ITEM_SUBSCRIBED 1
#define STEAM_ITEM_INSTALLED 4

#define STEAM_CB_ITEM_INSTALLED 3405
#define STEAM_CB_DOWNLOAD_RESULT 3406
#define STEAM_CB_FLAG_GAMESERVER 0x02

#define STEAM_WORKSHOP_FOLDERS 256  // [QL] what the table holds
#define STEAM_WORKSHOP_QUEUE 256

// what the two callbacks carry, packed as the SDK packs them: by 8 on Windows, by 4 elsewhere
#ifdef _WIN32
#pragma pack(push, 8)
#else
#pragma pack(push, 4)
#endif
typedef struct {
    unsigned int appId;
    uint64_t item;
} steamItemInstalled_t;

typedef struct {
    unsigned int appId;
    uint64_t item;
    int result;  // EResult, 1 is k_EResultOK
} steamDownloadResult_t;
#pragma pack(pop)

static struct {
    qboolean active;                       // a queue is going on (0x01309860)
    uint64_t current;                      // the item Steam is downloading (0x01309840)
    uint64_t queue[STEAM_WORKSHOP_QUEUE];  // the ones still missing, the current one first
    int numQueued;
    char folders[STEAM_WORKSHOP_FOLDERS][MAX_OSPATH];  // (0x01309880)
    uint64_t folderItems[STEAM_WORKSHOP_FOLDERS];
} ws;

qboolean Steam_WorkshopUGC(void) {
    return (qboolean)(st.ugc != NULL && st.GetItemState != NULL && st.DownloadItem != NULL);
}

static unsigned int Steam_ItemState(uint64_t item) {
    return st.GetItemState(st.ugc, item);
}

/*
===============
Steam_WorkshopNext

[QL] 0x44e300 / 0x469400: the item that finished leaves the queue and Steam is asked
for the next one
===============
*/
static void Steam_WorkshopNext(void) {
    int i, j;

    for (;;) {
        for (i = j = 0; i < ws.numQueued; i++) {
            if (ws.queue[i] != ws.current) {
                ws.queue[j++] = ws.queue[i];
            }
        }
        ws.numQueued = j;
        if (!ws.numQueued) {
            ws.current = 0;
            return;
        }
        ws.current = ws.queue[0];
        Com_Printf("Workshop item %llu: was queued, requesting download.\n", (unsigned long long)ws.current);
        if (st.DownloadItem(st.ugc, ws.current, 1)) {
            return;
        }
        // [ioquakelive] an id Steam turns down never gets an answer: retail waited for it forever
        Com_Printf("Workshop item %llu: Steam refused the download.\n", (unsigned long long)ws.current);
    }
}

/*
===============
Steam_WorkshopMount

[QL] 0x44e3f0 / 0x469470: the folder the item was installed to goes into the table the
filesystem mounts. Outside a queue the filesystem restarts at once to mount it; inside
one, a download that finished brings the next one. A subscribed item stays out of the
table, because the filesystem mounts it with the rest of the subscribed ones.
===============
*/
static void Steam_WorkshopMount(uint64_t item, qboolean inCache) {
    char folder[MAX_OSPATH];
    uint64_t size = 0;
    unsigned int stamp = 0;
    int i;

    folder[0] = '\0';
    if (st.GetItemInstallInfo) {
        st.GetItemInstallInfo(st.ugc, item, &size, folder, sizeof(folder), &stamp);
    }
    if (!(Steam_ItemState(item) & STEAM_ITEM_SUBSCRIBED)) {
        // [ioquakelive] once per item: retail adds it again each time a server asks for it
        for (i = 0; i < STEAM_WORKSHOP_FOLDERS; i++) {
            if (!ws.folders[i][0] || ws.folderItems[i] == item) {
                Q_strncpyz(ws.folders[i], folder, sizeof(ws.folders[i]));
                ws.folderItems[i] = item;
                break;
            }
        }
        if (!ws.active) {
            FS_WorkshopRestart();
            return;
        }
        if (!inCache) {
            Com_DPrintf("Steamworks download complete: %llu\n", (unsigned long long)item);
            Steam_WorkshopNext();
        }
        return;
    }
    if (!ws.active) {
        FS_WorkshopRestart();
        return;
    }
    // [ioquakelive] retail leaves the queue waiting forever when the item it was downloading
    // turns out to be subscribed
    if (!inCache) {
        Steam_WorkshopNext();
    }
}

/*
===============
Steam_WorkshopQueue

[QL] 0x44e8d0 / 0x4699c0. In a queue (the server's file, the items of the server being
joined) an item Steam already has is mounted from its cache and the rest wait their
turn. Outside one (steam_downloadugc) an item is downloaded on its own, unless a queue
is going on. Returns qtrue when there is a download to wait for.
===============
*/
qboolean Steam_WorkshopQueue(uint64_t item, qboolean queued) {
    if (!Steam_WorkshopUGC()) {
        return qfalse;
    }
    if (!queued) {
        if (ws.active) {
            Com_Printf("Steamworks downloads are already active, ignoring an explicit download request for %llu\n",
                       (unsigned long long)item);
            return qfalse;
        }
        if (!(Steam_ItemState(item) & STEAM_ITEM_INSTALLED)) {
            Com_Printf("Workshop item %llu: download\n", (unsigned long long)item);
            if (!st.DownloadItem(st.ugc, item, 1)) {
                Com_Printf("Workshop item %llu: Steam refused the download.\n", (unsigned long long)item);
                return qfalse;
            }
            return qtrue;
        }
    } else {
        ws.active = qtrue;
        if (!(Steam_ItemState(item) & STEAM_ITEM_INSTALLED)) {
            if (ws.numQueued) {
                Com_Printf("Workshop item %llu: queueing download.\n", (unsigned long long)item);
            } else {
                Com_Printf("Workshop item %llu: requesting download.\n", (unsigned long long)item);
                if (!st.DownloadItem(st.ugc, item, 1)) {
                    Com_Printf("Workshop item %llu: Steam refused the download.\n", (unsigned long long)item);
                    return qfalse;
                }
                ws.current = item;
            }
            if (ws.numQueued < STEAM_WORKSHOP_QUEUE) {
                ws.queue[ws.numQueued++] = item;
            }
            return qtrue;
        }
    }
    Com_Printf("Workshop item %llu: in cache.\n", (unsigned long long)item);
    Steam_WorkshopMount(item, qtrue);
    return qfalse;
}

/*
===============
Steam_WorkshopDone

[QL] 0x44ea60 / 0x4692d0: qtrue when no queue is going on. The call that finds the queue
empty says so and still answers qfalse.
===============
*/
qboolean Steam_WorkshopDone(void) {
    if (!ws.active) {
        return qtrue;
    }
    if (!ws.numQueued) {
        Com_Printf("Download completed for all steamworks items\n");
        ws.active = qfalse;
    }
    return qfalse;
}

// the items still missing, the one being downloaded included
int Steam_WorkshopPending(void) {
    return ws.numQueued;
}

uint64_t Steam_WorkshopCurrent(void) {
    return ws.current;
}

// how much of an item Steam has, for the connection screen and the server's wait
qboolean Steam_WorkshopProgress(uint64_t item, uint64_t* done, uint64_t* total) {
    *done = 0;
    *total = 0;
    if (!Steam_WorkshopUGC() || !st.GetItemDownloadInfo || !item) {
        return qfalse;
    }
    return st.GetItemDownloadInfo(st.ugc, item, done, total) ? qtrue : qfalse;
}

// where an item ended up installed, so it can be mounted
qboolean Steam_WorkshopPath(uint64_t item, char* buf, int size) {
    uint64_t sizeOnDisk = 0;
    unsigned int stamp = 0;

    buf[0] = '\0';
    if (!Steam_WorkshopUGC() || !st.GetItemInstallInfo) {
        return qfalse;
    }
    return st.GetItemInstallInfo(st.ugc, item, &sizeOnDisk, buf, size, &stamp) ? qtrue : qfalse;
}

// [QL] 0x44e690 / 0x4697a0: the items the player is subscribed to, up to 256
int Steam_WorkshopSubscribed(uint64_t* items, int max) {
    if (!Steam_WorkshopUGC() || !st.GetSubscribedItems || !st.GetNumSubscribedItems) {
        return 0;
    }
    return (int)st.GetSubscribedItems(st.ugc, items, (unsigned int)max);
}

// a folder of the table, NULL past the last one
const char* Steam_WorkshopFolder(int index, uint64_t* item) {
    if (index < 0 || index >= STEAM_WORKSHOP_FOLDERS || !ws.folders[index][0]) {
        return NULL;
    }
    *item = ws.folderItems[index];
    return ws.folders[index];
}

/*
===============
Steam_CB_ItemInstalled / Steam_CB_DownloadResult

[QL] 0x44e510 / 0x4695c0 and 0x44e570 / 0x469630. The first mounts an item that is not
subscribed; the second, the one the queue is waiting for, or moves on when Steam could
not download it. Retail compares the App ID with ISteamUtils::GetAppID, which here is
always the one Steam_Load declares.
===============
*/
static unsigned int Steam_OwnAppID(void) {
    return (unsigned int)atoi(STEAM_APP_ID);
}

static void STEAM_CB Steam_CB_ItemInstalled(STEAM_CB_SELF, void* param) {
    const steamItemInstalled_t* p = (const steamItemInstalled_t*)param;

    if (p->appId != Steam_OwnAppID()) {
        Com_DPrintf("OnDownloadItemResult skip, invalid app id %d\n", (int)p->appId);
        return;
    }
    if (!Steam_WorkshopUGC() || (Steam_ItemState(p->item) & STEAM_ITEM_SUBSCRIBED)) {
        return;
    }
    Steam_WorkshopMount(p->item, qfalse);
}

static void STEAM_CB Steam_CB_DownloadResult(STEAM_CB_SELF, void* param) {
    const steamDownloadResult_t* p = (const steamDownloadResult_t*)param;

    if (p->appId != Steam_OwnAppID()) {
        Com_Printf("OnDownloadItemResult skip, invalid app id %d\n", (int)p->appId);
        return;
    }
    if (ws.active && p->item != ws.current) {
        Com_DPrintf("OnDownloadItemResult skip, not the active download %llu\n", (unsigned long long)p->item);
        return;
    }
    if (p->result != 1) {
        Com_Printf("Download item %llu failed with EResult code %i\n", (unsigned long long)p->item, p->result);
        Steam_WorkshopNext();
        return;
    }
    if (!Steam_WorkshopUGC()) {
        return;
    }
    Steam_WorkshopMount(p->item, qfalse);
}

static void STEAM_CB Steam_CB_ItemInstalledIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_ItemInstalled(STEAM_CB_PASS, param);
}

static void STEAM_CB Steam_CB_DownloadResultIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_DownloadResult(STEAM_CB_PASS, param);
}

static int STEAM_CB Steam_CB_ItemInstalledSize(STEAM_CB_SELF) {
    return (int)sizeof(steamItemInstalled_t);
}

static int STEAM_CB Steam_CB_DownloadResultSize(STEAM_CB_SELF) {
    return (int)sizeof(steamDownloadResult_t);
}

static void* steamItemInstalledVTable[3];
static void* steamDownloadResultVTable[3];

// [QL] SteamWorkshopCallbacks (0x44e5f0 / 0x4696d0): the server's callbacks on a dedicated
// server, the client's otherwise
static void Steam_WorkshopListen(qboolean asServer) {
    if (!st.RegisterCallback || st.itemInstalled.vtable) {
        return;
    }
    steamItemInstalledVTable[0] = (void*)Steam_CB_ItemInstalledIO;
    steamItemInstalledVTable[1] = (void*)Steam_CB_ItemInstalled;
    steamItemInstalledVTable[2] = (void*)Steam_CB_ItemInstalledSize;
    steamDownloadResultVTable[0] = (void*)Steam_CB_DownloadResultIO;
    steamDownloadResultVTable[1] = (void*)Steam_CB_DownloadResult;
    steamDownloadResultVTable[2] = (void*)Steam_CB_DownloadResultSize;

    st.itemInstalled.vtable = steamItemInstalledVTable;
    st.itemInstalled.flags = asServer ? STEAM_CB_FLAG_GAMESERVER : 0;
    st.itemInstalled.id = STEAM_CB_ITEM_INSTALLED;
    st.RegisterCallback(&st.itemInstalled, STEAM_CB_ITEM_INSTALLED);

    st.downloadResult.vtable = steamDownloadResultVTable;
    st.downloadResult.flags = asServer ? STEAM_CB_FLAG_GAMESERVER : 0;
    st.downloadResult.id = STEAM_CB_DOWNLOAD_RESULT;
    st.RegisterCallback(&st.downloadResult, STEAM_CB_DOWNLOAD_RESULT);
}

/*
===============
Steam_DownloadUGC_f

[QL] "steam_downloadugc <itemid>" (qzeroded.x64 0x4376e0, quakelive_steam.exe 0x4df010),
one of the server's operator commands: an item downloaded on its own, outside the queue
===============
*/
void Steam_DownloadUGC_f(void) {
    unsigned long long item = 0;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: steam_downloadugc <itemid>\n");
        return;
    }
    sscanf(Cmd_Argv(1), "%llu", &item);
    Steam_WorkshopQueue((uint64_t)item, qfalse);
}

#ifndef DEDICATED
/*
===============
Steam_SubscribeUGC_f / Steam_UnsubscribeUGC_f

[QL] quakelive_steam.exe 0x4df070 and 0x4df0d0: the player's subscription to an item. One
Steam already has installed is mounted at once.
===============
*/
void Steam_SubscribeUGC_f(void) {
    unsigned long long item = 0;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: steam_subscribeugc <itemid>\n");
        return;
    }
    sscanf(Cmd_Argv(1), "%llu", &item);
    // [ioquakelive] the player's account, so not through rcon
    if (Com_Redirected()) {
        Com_Printf("steam_subscribeugc only works from the local console.\n");
        return;
    }
    if (!Steam_WorkshopUGC() || !st.SubscribeItem) {
        return;
    }
    st.SubscribeItem(st.ugc, (uint64_t)item);
    if (Steam_ItemState((uint64_t)item) & STEAM_ITEM_INSTALLED) {
        FS_WorkshopRestart();
    }
}

void Steam_UnsubscribeUGC_f(void) {
    unsigned long long item = 0;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: steam_unsubscribeugc <itemid>\n");
        return;
    }
    sscanf(Cmd_Argv(1), "%llu", &item);
    if (Com_Redirected()) {
        Com_Printf("steam_unsubscribeugc only works from the local console.\n");
        return;
    }
    if (!Steam_WorkshopUGC() || !st.UnsubscribeItem) {
        return;
    }
    st.UnsubscribeItem(st.ugc, (uint64_t)item);
}
#endif

// [QL] 0x008e0644: whether the server is logged on to Steam
qboolean Steam_GS_LoggedOn(void) {
    return (qboolean)(st.serverUp && st.loggedOn);
}

// what the dedicated server runs while it waits at startup
void Steam_GS_RunCallbacks(void) {
    if (st.serverUp) {
        st.GameServer_RunCallbacks();
    }
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

VOICE

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
The peer to peer session

Steam does not deliver packets from somebody there is no session with: it raises a
callback and waits for it to be accepted. Retail registers none -- its only two calls
to SteamAPI_RegisterCallback are for something else -- so this is an addition, not a
port: without accepting the session the server's voice would never arrive.
===============
*/
static void STEAM_CB Steam_CB_P2PRequest(STEAM_CB_SELF, void* param) {
    const steamCallback_t* me = (const steamCallback_t*)self;
    uint64_t who = *(const uint64_t*)param;
    qboolean asServer = (qboolean)((me->flags & STEAM_CB_FLAG_GAMESERVER) != 0);

    Steam_P2PAccept(who, asServer);
}

static void STEAM_CB Steam_CB_P2PRequestIO(STEAM_CB_SELF, void* param, unsigned char failed,
                                           uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_P2PRequest(STEAM_CB_PASS, param);
}

static int STEAM_CB Steam_CB_P2PRequestSize(STEAM_CB_SELF) {
    return (int)sizeof(uint64_t);
}

// retail's compiler orders the two Run overloads the reverse of how they are declared (see
// 0x00532f1c); GCC and clang keep the declared order, so both do the same job
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

WHO EACH PLAYER IS

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

static void STEAM_CB Steam_CB_Auth(STEAM_CB_SELF, void* param) {
    const steamAuthResponse_t* r = (const steamAuthResponse_t*)param;

    (void)self;
    SV_SteamAuthResponse(r->steamId, r->response);
}

static void STEAM_CB Steam_CB_AuthIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_Auth(STEAM_CB_PASS, param);
}

static int STEAM_CB Steam_CB_AuthSize(STEAM_CB_SELF) {
    return (int)sizeof(steamAuthResponse_t);
}

// retail's compiler orders the two Run overloads the reverse of how they are declared (see
// 0x00532f1c); GCC and clang keep the declared order, so both do the same job
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
Whether the server is logged on

[QL] qzeroded.x64 0x4471f0 registers three: the connection (0x447190), the failure
(0x446640) and the disconnection (0x446660). When the server logs on, everything the list
shows goes out again, and the Steam ID with it: until then the one the server has is not
its own. SteamServersConnected_t, SteamServerConnectFailure_t and SteamServersDisconnected_t
are k_iSteamUserCallbacks (100) + 1, + 2 and + 3.
===============
*/
#define STEAM_CB_SERVERS_CONNECTED 101
#define STEAM_CB_SERVERS_FAILED 102
#define STEAM_CB_SERVERS_DISCONNECTED 103

static void STEAM_CB Steam_CB_Servers(STEAM_CB_SELF, void* param) {
    const steamCallback_t* me = (const steamCallback_t*)self;

    (void)param;
    switch (me->id) {
        case STEAM_CB_SERVERS_CONNECTED:
            Com_Printf("Connected to Steam servers\n");
            st.loggedOn = qtrue;
            SV_SteamConnected();
            break;
        case STEAM_CB_SERVERS_FAILED:
            st.loggedOn = qfalse;
            Com_Printf("Failed to connect to Steam servers\n");
            break;
        default:
            st.loggedOn = qfalse;
            Com_Printf("Disconnected from Steam servers\n");
            break;
    }
}

static void STEAM_CB Steam_CB_ServersIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_Servers(STEAM_CB_PASS, param);
}

// the sizes of retail's (0x510c30, 0x510c70, 0x510cb0): the first is an empty struct
static int STEAM_CB Steam_CB_ServersSize(STEAM_CB_SELF) {
    return ((const steamCallback_t*)self)->id == STEAM_CB_SERVERS_CONNECTED ? 1 : 4;
}

static void* steamServersVTable[3];

static void Steam_ServersListenOne(steamCallback_t* cb, int id) {
    cb->vtable = steamServersVTable;
    cb->flags = STEAM_CB_FLAG_GAMESERVER;
    cb->id = id;
    st.RegisterCallback(cb, id);
}

static void Steam_ServersListen(void) {
    if (!st.RegisterCallback || st.serversConnected.vtable) {
        return;
    }
    steamServersVTable[0] = (void*)Steam_CB_ServersIO;
    steamServersVTable[1] = (void*)Steam_CB_Servers;
    steamServersVTable[2] = (void*)Steam_CB_ServersSize;

    Steam_ServersListenOne(&st.serversConnected, STEAM_CB_SERVERS_CONNECTED);
    Steam_ServersListenOne(&st.serversFailed, STEAM_CB_SERVERS_FAILED);
    Steam_ServersListenOne(&st.serversDisconnected, STEAM_CB_SERVERS_DISCONNECTED);
}

/*
===============
Steam_GS_BeginAuth / Steam_GS_EndAuth

What the server asks Steam about each player. [QL] 0x00465fd0: somebody with a session
already open is let through without asking again, and a ticket Steam takes
(k_EBeginAuthSessionResultOK) opens one. The real verdict arrives later, through the
callback. 0x004661e0 closes it.

Retail turns everybody away while its Steam is not up; here the server goes on without it,
as it always has.
===============
*/
static int Steam_GS_TrackedIndex(uint64_t steamId) {
    int i;

    for (i = 0; i < st.numTracked; i++) {
        if (st.tracked[i] == steamId) {
            return i;
        }
    }
    return -1;
}

qboolean Steam_GS_BeginAuth(uint64_t steamId, const void* ticket, int len) {
    int result;

    if (!st.serverUp || !st.gameServer || !st.BeginAuthSession || len <= 0) {
        return qtrue;  // nobody to ask without Steam
    }
    if (Steam_GS_TrackedIndex(steamId) >= 0) {
        Com_DPrintf("Refusing to re-auth a client that is already being tracked\n");
        return qtrue;
    }
    result = st.BeginAuthSession(st.gameServer, ticket, len, steamId);
    Com_DPrintf("Called BeginAuthSession on steam id %llu, got response %i\n",
                (unsigned long long)steamId, result);
    if (result != STEAM_AUTH_BEGIN_OK) {
        return qfalse;
    }
    if (st.numTracked < STEAM_MAX_TRACKED) {
        st.tracked[st.numTracked++] = steamId;
    }
    return qtrue;
}

void Steam_GS_EndAuth(uint64_t steamId) {
    int i;

    if (!st.serverUp || !st.gameServer || !st.EndAuthSession) {
        Com_DPrintf("Can't end auth session on steam id %llu, isServerActive == false\n",
                    (unsigned long long)steamId);
        return;
    }
    st.EndAuthSession(st.gameServer, steamId);
    i = Steam_GS_TrackedIndex(steamId);
    if (i >= 0) {
        st.tracked[i] = st.tracked[--st.numTracked];
    }
    Com_DPrintf("Called EndAuthSession on steam id %llu\n", (unsigned long long)steamId);
}

/*
===============================================================================

THE PLAYERS' STATS AND ACHIEVEMENTS

What sv_steamstats.c keeps for each player goes to Steam through here: on a server with
ISteamGameServerStats, in a game of one's own with the player's ISteamUserStats.

[QL] 0x00467850 registers two callbacks for each player, GSStatsReceived_t (1800) and
GSStatsStored_t (1801), k_iSteamGameServerStatsCallbacks + 0 and + 1. Both carry the result
and the player; CSteamID is packed to one byte in the SDK, so the Steam ID sits right after
the result and the struct is twelve bytes on every system (retail's size, 0x00467480).
===============================================================================
*/
#define STEAM_CB_GS_STATS_RECEIVED 1800
#define STEAM_CB_GS_STATS_STORED 1801

#pragma pack(push, 4)
typedef struct {
    int result;  // EResult, 1 is k_EResultOK
    uint64_t steamId;
} steamStatsResult_t;
#pragma pack(pop)

static void STEAM_CB Steam_CB_Stats(STEAM_CB_SELF, void* param) {
    const steamCallback_t* me = (const steamCallback_t*)self;
    const steamStatsResult_t* r = (const steamStatsResult_t*)param;

    if (me->id == STEAM_CB_GS_STATS_RECEIVED) {
        SV_SteamStatsReceived(r->result);
    } else {
        SV_SteamStatsStored(r->result);
    }
}

static void STEAM_CB Steam_CB_StatsIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_Stats(STEAM_CB_PASS, param);
}

static int STEAM_CB Steam_CB_StatsSize(STEAM_CB_SELF) {
    return (int)sizeof(steamStatsResult_t);
}

static void* steamStatsVTable[3];

static void Steam_StatsListen(void) {
    if (!st.RegisterCallback || !st.gameServerStats || st.statsReceived.vtable) {
        return;
    }
    steamStatsVTable[0] = (void*)Steam_CB_StatsIO;
    steamStatsVTable[1] = (void*)Steam_CB_Stats;
    steamStatsVTable[2] = (void*)Steam_CB_StatsSize;

    st.statsReceived.vtable = steamStatsVTable;
    st.statsReceived.flags = STEAM_CB_FLAG_GAMESERVER;
    st.statsReceived.id = STEAM_CB_GS_STATS_RECEIVED;
    st.RegisterCallback(&st.statsReceived, STEAM_CB_GS_STATS_RECEIVED);

    st.statsStored.vtable = steamStatsVTable;
    st.statsStored.flags = STEAM_CB_FLAG_GAMESERVER;
    st.statsStored.id = STEAM_CB_GS_STATS_STORED;
    st.RegisterCallback(&st.statsStored, STEAM_CB_GS_STATS_STORED);
}

// [QL] 0x00467190 and the end of 0x00467850: only asked for once the server is logged on
void Steam_GS_RequestUserStats(uint64_t steamId) {
    if (!st.serverUp || !st.gameServerStats || !st.RequestUserStats || !st.BLoggedOn(st.gameServer)) {
        return;
    }
    st.RequestUserStats(st.gameServerStats, steamId);
}

qboolean Steam_GS_GetUserStat(uint64_t steamId, const char* name, int* value) {
    if (!st.gameServerStats || !st.GetUserStat) {
        return qfalse;
    }
    return st.GetUserStat(st.gameServerStats, steamId, name, value) ? qtrue : qfalse;
}

qboolean Steam_GS_SetUserStat(uint64_t steamId, const char* name, int value) {
    if (!st.gameServerStats || !st.SetUserStat) {
        return qfalse;
    }
    return st.SetUserStat(st.gameServerStats, steamId, name, value) ? qtrue : qfalse;
}

qboolean Steam_GS_SetUserAchievement(uint64_t steamId, const char* name) {
    if (!st.gameServerStats || !st.SetUserAchievement) {
        return qfalse;
    }
    return st.SetUserAchievement(st.gameServerStats, steamId, name) ? qtrue : qfalse;
}

void Steam_GS_StoreUserStats(uint64_t steamId) {
    if (st.gameServerStats && st.StoreUserStats) {
        st.StoreUserStats(st.gameServerStats, steamId);
    }
}

// [QL] 0x00467eeb: in a game of one's own the achievement goes to the player's own Steam
qboolean Steam_SetAchievement(const char* name) {
    if (!st.userStats || !st.SetAchievement) {
        return qfalse;
    }
    return st.SetAchievement(st.userStats, name) ? qtrue : qfalse;
}

void Steam_StoreStats(void) {
    if (st.userStats && st.StoreStats) {
        st.StoreStats(st.userStats);
    }
}

/*
===============================================================================

A PLAYER'S STATS FOR THE FRONT END

[QL] The web UI's Career Stats and profile card asked for them with RequestUserStats
(0x00431e50) and Steam answered with UserStatsReceived_t (1101, k_iSteamUserStatsCallbacks
+ 1), which 0x0045ffd0 turned into what the page read: the player's Steam name and every
stat of the table, 0 when the answer was not k_EResultOK. It came for any player asked
about, and for the player's own stats when the game started (RequestCurrentStats).

The struct: the game's ID, the result and the player, whose CSteamID is packed to one
byte, so the player sits at 12 on every system. Steam packs its callbacks to 8 on Windows
and to 4 elsewhere, which only changes the size it is told.

The pages ask every frame, so the answers are kept: the latest few players.
===============================================================================
*/
#define STEAM_CB_USER_STATS_RECEIVED 1101
#ifdef _WIN32
#define STEAM_USER_STATS_SIZE 24
#else
#define STEAM_USER_STATS_SIZE 20
#endif

#define STEAM_MAX_USER_STATS 16

typedef struct {
    uint64_t steamId;
    int state;  // STEAM_STATS_*
    int asked;  // when it was last asked for or answered, to know which to drop
    char name[64];
    int stats[STEAM_NUM_STATS];
} steamUserStats_t;

static steamUserStats_t steamUserStats[STEAM_MAX_USER_STATS];

static steamUserStats_t* Steam_UserStatsSlot(uint64_t steamId, qboolean make) {
    steamUserStats_t* oldest = &steamUserStats[0];
    int i;

    for (i = 0; i < STEAM_MAX_USER_STATS; i++) {
        if (steamUserStats[i].steamId == steamId) {
            return &steamUserStats[i];
        }
        if (steamUserStats[i].asked < oldest->asked) {
            oldest = &steamUserStats[i];
        }
    }
    if (!make) {
        return NULL;
    }
    Com_Memset(oldest, 0, sizeof(*oldest));
    oldest->steamId = steamId;
    return oldest;
}

static void STEAM_CB Steam_CB_UserStats(STEAM_CB_SELF, void* param) {
    const byte* data = (const byte*)param;
    steamUserStats_t* u;
    uint64_t steamId;
    const char* name;
    int result;
    int i;

    Com_Memcpy(&result, data + 8, sizeof(result));
    Com_Memcpy(&steamId, data + 12, sizeof(steamId));
    u = Steam_UserStatsSlot(steamId, qtrue);
    u->state = result == 1 ? STEAM_STATS_DONE : STEAM_STATS_FAILED;
    u->asked = Sys_Milliseconds();
    name = (st.friends && st.GetFriendPersonaName) ? st.GetFriendPersonaName(st.friends, steamId) : NULL;
    Q_strncpyz(u->name, name ? name : "", sizeof(u->name));
    for (i = 0; i < STEAM_NUM_STATS; i++) {
        u->stats[i] = 0;
        if (result == 1 && st.userStats && st.UserGetUserStat) {
            st.UserGetUserStat(st.userStats, steamId, steamStatNames[i], &u->stats[i]);
        }
    }
}

static void STEAM_CB Steam_CB_UserStatsIO(STEAM_CB_SELF, void* param, unsigned char failed, uint64_t call) {
    (void)failed;
    (void)call;
    Steam_CB_UserStats(STEAM_CB_PASS, param);
}

static int STEAM_CB Steam_CB_UserStatsSize(STEAM_CB_SELF) {
    return STEAM_USER_STATS_SIZE;
}

static void* steamUserStatsVTable[3];

static void Steam_UserStatsListen(void) {
    if (!st.RegisterCallback || !st.userStats || !st.UserGetUserStat || st.userStatsReceived.vtable) {
        return;
    }
    steamUserStatsVTable[0] = (void*)Steam_CB_UserStatsIO;
    steamUserStatsVTable[1] = (void*)Steam_CB_UserStats;
    steamUserStatsVTable[2] = (void*)Steam_CB_UserStatsSize;

    st.userStatsReceived.vtable = steamUserStatsVTable;
    st.userStatsReceived.flags = 0;
    st.userStatsReceived.id = STEAM_CB_USER_STATS_RECEIVED;
    st.RegisterCallback(&st.userStatsReceived, STEAM_CB_USER_STATS_RECEIVED);
}

/*
===============
Steam_UserStats

What the front end knows of a player's stats, 0 for oneself. With request it asks Steam
again, as each page of retail's did when it opened, and keeps showing the last answer
meanwhile.
===============
*/
int Steam_UserStats(uint64_t steamId, qboolean request, char* name, int nameSize, int* stats, int numStats) {
    steamUserStats_t* u;

    if (!steamId) {
        steamId = Steam_ClientSteamID();
    }
    if (!steamId || !st.clientUp || !st.userStats || !st.UserRequestUserStats || !st.userStatsReceived.vtable) {
        return STEAM_STATS_NONE;
    }
    u = Steam_UserStatsSlot(steamId, request);
    if (!u) {
        return STEAM_STATS_NONE;
    }
    if (request) {
        if (u->state == STEAM_STATS_NONE || u->state == STEAM_STATS_FAILED) {
            u->state = STEAM_STATS_LOADING;
        }
        u->asked = Sys_Milliseconds();
        st.UserRequestUserStats(st.userStats, steamId);
    }
    if (name && nameSize > 0) {
        Q_strncpyz(name, u->name, nameSize);
    }
    if (stats && numStats > 0) {
        Com_Memcpy(stats, u->stats, sizeof(int) * (numStats < STEAM_NUM_STATS ? numStats : STEAM_NUM_STATS));
    }
    return u->state;
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

Steam's bridge between two players. The server uses its own interface
(SteamGameServerNetworking) and the client uses its own.
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

/*
===============
Steam_GS_SendReport / Steam_P2PReadReport

[QL] The match report goes from the server to each player over channel 0, reliably
(0x00468ee0), and the client reads it whole, as big as Steam says it is (0x00461d40).
===============
*/
#define STEAM_REPORT_CHANNEL 0

qboolean Steam_GS_SendReport(uint64_t to, const void* data, int size) {
    void* self = Steam_P2P(qtrue);

    if (!self || !st.SendP2PPacket || !to) {
        return qfalse;
    }
    return st.SendP2PPacket(self, to, data, (unsigned int)size, STEAM_P2P_RELIABLE, STEAM_REPORT_CHANNEL) ? qtrue
                                                                                                    : qfalse;
}

// a report that has arrived, in memory the caller frees; NULL when none has
void* Steam_P2PReadReport(int* size) {
    void* self = Steam_P2P(qfalse);
    unsigned int avail = 0;
    unsigned int read = 0;
    uint64_t from = 0;
    void* buffer;

    if (!self || !st.IsP2PPacketAvailable || !st.ReadP2PPacket) {
        return NULL;
    }
    if (!st.IsP2PPacketAvailable(self, &avail, STEAM_REPORT_CHANNEL) || !avail) {
        return NULL;
    }
    buffer = malloc(avail);
    if (!buffer) {
        return NULL;
    }
    if (!st.ReadP2PPacket(self, buffer, avail, &read, &from, STEAM_REPORT_CHANNEL)) {
        free(buffer);
        return NULL;
    }
    *size = (int)read;
    return buffer;
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
The friends list

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
    // [QL] 0x00433837: the nickname one gave them, and what their game says they are doing
    name = st.GetPlayerNickname ? st.GetPlayerNickname(st.friends, id) : NULL;
    Q_strncpyz(out->nickname, name ? name : "", sizeof(out->nickname));
    Steam_StripColors(out->nickname);
    name = st.GetFriendRichPresence ? st.GetFriendRichPresence(st.friends, id, "status") : NULL;
    Q_strncpyz(out->status, name ? name : "", sizeof(out->status));

    Com_Memset(&game, 0, sizeof(game));
    if (st.GetFriendGamePlayed && st.GetFriendGamePlayed(st.friends, id, &game)) {
        out->playing = qtrue;
        // Steam's game id carries the App ID in its low 32 bits
        out->sameGame = (qboolean)((unsigned int)game.gameId == (unsigned int)atoi(STEAM_APP_ID));
        out->ip = game.ip;
        out->port = game.port;
        out->lobby = (qboolean)(game.lobby != 0);
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

// the friend list's Send Message and View Achievements
void Steam_ShowChat(uint64_t steamId) {
    Steam_ShowOverlayUser("chat", steamId);
}

void Steam_ShowAchievements(uint64_t steamId) {
    Steam_ShowOverlayUser("achievements", steamId);
}

// [QL] Send Invite (0x00432520): an invitation to the match one is in, which Steam hands the
// friend with its connect line
qboolean Steam_InviteToGame(uint64_t steamId, const char* connect) {
    if (!st.clientUp || !st.friends || !st.InviteUserToGame || !steamId) {
        return qfalse;
    }
    return st.InviteUserToGame(st.friends, steamId, connect) ? qtrue : qfalse;
}

/*
===============
Steam_Avatar

[QL] The web UI showed a player's Steam avatar as asset://steam/avatar/<id>. The renderer
only reads images from files, so the medium avatar (64x64) goes to avatars/<id>.tga,
once a session per player; Steam may take a moment to have it, and until then the
answer is no.
===============
*/
#define STEAM_MAX_AVATARS 128

qboolean Steam_Avatar(uint64_t steamId, char* path, int size) {
    static uint64_t written[STEAM_MAX_AVATARS];
    static int numWritten;
    unsigned int w = 0, h = 0, i;
    byte header[18];
    byte* pixels;
    fileHandle_t f;
    int image;

    if (!steamId) {
        steamId = Steam_ClientSteamID();
    }
    if (!steamId) {
        return qfalse;
    }
    Com_sprintf(path, size, "avatars/%llu.tga", (unsigned long long)steamId);
    for (i = 0; i < (unsigned int)numWritten; i++) {
        if (written[i] == steamId) {
            return qtrue;
        }
    }
    if (!st.clientUp || !st.friends || !st.utils || !st.GetMediumFriendAvatar || !st.GetImageSize ||
        !st.GetImageRGBA) {
        return qfalse;
    }
    image = st.GetMediumFriendAvatar(st.friends, steamId);
    if (image <= 0 || !st.GetImageSize(st.utils, image, &w, &h) || !w || !h || w > 512 || h > 512) {
        return qfalse;  // none, or still on its way
    }
    pixels = (byte*)Z_Malloc(w * h * 4);
    if (!st.GetImageRGBA(st.utils, image, pixels, (int)(w * h * 4))) {
        Z_Free(pixels);
        return qfalse;
    }
    // an uncompressed 32-bit TGA from the bottom left, whose pixels go blue, green, red, alpha:
    // the renderer's loader does not take the top-down flag
    for (i = 0; i < w * h; i++) {
        byte r = pixels[i * 4];

        pixels[i * 4] = pixels[i * 4 + 2];
        pixels[i * 4 + 2] = r;
    }
    Com_Memset(header, 0, sizeof(header));
    header[2] = 2;
    header[12] = (byte)(w & 0xff);
    header[13] = (byte)(w >> 8);
    header[14] = (byte)(h & 0xff);
    header[15] = (byte)(h >> 8);
    header[16] = 32;
    header[17] = 0x08;
    f = FS_FOpenFileWrite(path);
    if (f) {
        FS_Write(header, sizeof(header), f);
        for (i = h; i > 0; i--) {
            FS_Write(pixels + (i - 1) * w * 4, (int)(w * 4), f);
        }
        FS_FCloseFile(f);
    }
    Z_Free(pixels);
    if (!f) {
        return qfalse;
    }
    if (numWritten < STEAM_MAX_AVATARS) {
        written[numWritten++] = steamId;
    }
    return qtrue;
}

// the address Steam sees the server at, 0 when it is not logged on
unsigned int Steam_GS_PublicIP(void) {
    if (!st.serverUp || !st.GetPublicIP) {
        return 0;
    }
    return st.GetPublicIP(st.gameServer);
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
