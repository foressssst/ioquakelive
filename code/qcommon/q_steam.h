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

// from SV_SpawnServer: the name and the map the list shows
void Steam_GS_SetServerInfo(const char* hostname, const char* mapName, qboolean needPassword, int botCount);

// la lista de servidores, que en Quake Live la da Steam y no un master propio
qboolean Steam_RequestServerList(int source);  // 0 internet, 1 LAN, 2 amigos, 3 favoritos, 4 historial
void Steam_ServerList_f(void);
qboolean Steam_ServerListRefreshing(void);

// el Workshop, que es de donde Quake Live baja los mapas
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

// la voz, que en Quake Live va por la red entre pares de Steam y no por la del juego
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

// un amigo de Steam, tal como lo ve la lista
typedef struct {
    uint64_t steamId;
    char name[64];
    int state;           // 0 desconectado, 1 conectado, 2 ocupado, 3 ausente, ...
    qboolean playing;    // esta jugando a algo
    qboolean sameGame;   // y ese algo es este juego
    unsigned int ip;     // el servidor donde esta, 0 si ninguno
    unsigned short port;
} steamFriend_t;

int Steam_FriendCount(void);
qboolean Steam_FriendGet(int index, steamFriend_t* out);
const char* Steam_FriendStateName(int state);
void Steam_SetRichPresence(const char* status);

#endif
