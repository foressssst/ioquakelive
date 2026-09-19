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
int Steam_ServerListCount(void);
qboolean Steam_ServerListGet(int index, unsigned int* ip, unsigned short* port, int* ping, char* info,
                             int infoSize);

// the server shares its game socket with Steam, so the query packets Steam sends arrive here
qboolean Steam_GS_HandlePacket(netadr_t from, const void* data, int len);

void Steam_Shutdown(void);

qboolean Steam_GS_Running(void);

#endif
