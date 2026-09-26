/*
===========================================================================
ioquakelive - Steam integration

Quake Live talks to Steam for two things this file covers: the server announces
itself to Steam's master servers, and the client logs in to the Steam API. Both
are what retail does; the addresses of the functions read to write this are in
steam.c next to each call.

Nothing here links against the Steamworks SDK. The library is loaded at run time
from the player's own Quake Live installation, so no Valve or id file is
redistributed with this project.
===========================================================================
*/

#ifndef Q_STEAM_H
#define Q_STEAM_H

#include "q_shared.h"
#include "../qcommon/qcommon.h"

// both called from Com_Init, where retail calls them
void Steam_Init(void);
void Steam_GS_Init(void);

// per frame, from Com_Frame
void Steam_Frame(void);

// from SV_ChangeMaxClients, where retail updates it
void Steam_GS_SetMaxPlayers(int maxPlayers);

// the server's entry in the list, kept up to date by SV_SteamFrame
void Steam_GS_SetPassword(qboolean needPassword);
void Steam_GS_SetName(const char* name);
void Steam_GS_SetMap(const char* map);
void Steam_GS_SetBotCount(int bots);
void Steam_GS_SetKeyValue(const char* key, const char* value);
void Steam_GS_SetKeyValues(const char* info);
void Steam_GS_UpdateUser(uint64_t steamId, const char* name, int score);
void Steam_GS_UpdateGameInfo(qboolean force, char* const* factoryTags, int numFactoryTags);

// the server list, which in Quake Live comes from Steam and not from a master of its own
qboolean Steam_RequestServerList(int source);  // 0 internet, 1 LAN, 2 friends, 3 favourites, 4 history
void Steam_ServerList_f(void);
qboolean Steam_ServerListRefreshing(void);
qboolean Steam_RefreshServerList(void);  // Quick Refresh: the same servers, asked again
int Steam_ServerListNextUpdated(void);   // one that answered again, or -1
const char* Steam_ServerListName(int index);  // the name as it is, which an info string cannot carry

// what the browser shows about the server it has selected
#define STEAM_DETAILS_NONE 0
#define STEAM_DETAILS_LOADING 1
#define STEAM_DETAILS_DONE 2
#define STEAM_DETAILS_FAILED 3
qboolean Steam_RequestServerDetails(unsigned int ip, unsigned short port);
int Steam_ServerDetails(char* rules, int rulesSize, char* players, int playersSize);
qboolean Steam_SetFavorite(unsigned int ip, unsigned short port, qboolean favorite);

// the Workshop, which is where Quake Live downloads its maps from
qboolean Steam_WorkshopReady(void);
qboolean Steam_WorkshopInstalled(uint64_t item);
qboolean Steam_WorkshopDownload(uint64_t item);
qboolean Steam_WorkshopBusy(void);
qboolean Steam_WorkshopProgress(uint64_t* done, uint64_t* total);
qboolean Steam_WorkshopPath(uint64_t item, char* buf, int size);
int Steam_WorkshopSubscribed(uint64_t* items, int max);
unsigned int Steam_WorkshopState(uint64_t item);
uint64_t Steam_GS_SteamID(void);
int Steam_ServerListCount(void);
qboolean Steam_ServerListGet(int index, unsigned int* ip, unsigned short* port, int* ping, char* info,
                             int infoSize);

// the server shares its game socket with Steam, so the query packets Steam sends arrive here
qboolean Steam_GS_HandlePacket(netadr_t from, const void* data, int len);

void Steam_Shutdown(void);

qboolean Steam_GS_Running(void);

// who each player is: the client asks for a ticket signed by Steam and the server hands
// it to Steam to say whether it is good. [QL] 0x531b38 onwards.
#define STEAM_AUTH_BEGIN_OK 0     // EBeginAuthSessionResult
#define STEAM_AUTH_BEGIN_DUPLICATE 2
int Steam_AuthTicket(void* buffer, int size);
int Steam_GS_BeginAuth(uint64_t steamId, const void* ticket, int len);
void Steam_GS_EndAuth(uint64_t steamId);
const char* Steam_AuthResponseName(int response);

// voice, which in Quake Live travels over Steam's peer to peer network and not the game's
qboolean Steam_VoiceReady(void);
void Steam_VoiceStart(void);
void Steam_VoiceStop(void);
qboolean Steam_VoiceRecording(void);
int Steam_VoiceCapture(void* buffer, int maxSize);
int Steam_VoiceDecompress(const void* compressed, int size, void* out, int outSize);
int Steam_VoiceRate(void);

qboolean Steam_P2PSend(uint64_t to, const void* data, int size, qboolean asServer);
int Steam_P2PReceive(void* buffer, int maxSize, uint64_t* from, qboolean asServer);
void Steam_P2PAccept(uint64_t who, qboolean asServer);
void Steam_P2PHello(uint64_t to, qboolean asServer);
uint64_t Steam_ClientSteamID(void);
void Steam_ShowFriendAdd(uint64_t steamId);
void Steam_ShowProfile(uint64_t steamId);
void Steam_ShowFriends(void);

// a Steam friend as the list sees one
typedef struct {
    uint64_t steamId;
    char name[64];
    int state;           // 0 desconectado, 1 conectado, 2 ocupado, 3 ausente, ...
    qboolean playing;    // esta jugando a algo
    qboolean sameGame;   // and that something is this game
    unsigned int ip;     // the server they are on, 0 for none
    unsigned short port;
} steamFriend_t;

int Steam_FriendCount(void);
qboolean Steam_FriendGet(int index, steamFriend_t* out);
const char* Steam_FriendStateName(int state);
void Steam_SetRichPresence(const char* status);

#endif
