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
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../game/g_public.h"
#include "../game/bg_public.h"

#define PERS_SCORE 0

#define MAX_ENT_CLUSTERS 16

typedef struct svEntity_s {
    struct worldSector_s* worldSector;
    struct svEntity_s* nextEntityInWorldSector;

    entityState_t baseline;
    int numClusters;
    int clusternums[MAX_ENT_CLUSTERS];
    int lastCluster;
    int areanum, areanum2;
    int snapshotCounter;
} svEntity_t;

typedef enum {
    SS_DEAD,
    SS_LOADING,
    SS_GAME
} serverState_t;

typedef struct {
    serverState_t state;
    qboolean restarting;
    int serverId;
    int restartedServerId;
    int checksumFeed;
    int checksumFeedServerId;
    int snapshotCounter;
    int timeResidual;
    int nextFrameTime;
    char* configstrings[MAX_CONFIGSTRINGS];
    svEntity_t svEntities[MAX_GENTITIES];

    char* entityParsePoint;

    sharedEntity_t* gentities;
    int gentitySize;
    int num_entities;

    playerState_t* gameClients;
    int gameClientSize;

    int restartTime;
    int time;
} server_t;

typedef struct {
    int areabytes;
    byte areabits[MAX_MAP_AREA_BYTES];
    playerState_t ps;
    int num_entities;
    int first_entity;
    int messageSent;
    int messageAcked;
    int messageSize;
} clientSnapshot_t;

typedef enum {
    CS_FREE,
    CS_ZOMBIE,
    CS_CONNECTED,
    CS_PRIMED,
    CS_ACTIVE
} clientState_t;

typedef struct netchan_buffer_s {
    msg_t msg;
    byte msgBuffer[MAX_MSGLEN];
    char clientCommandString[MAX_STRING_CHARS];
    struct netchan_buffer_s* next;
} netchan_buffer_t;

typedef struct client_s {
    clientState_t state;
    char userinfo[MAX_INFO_STRING];

    char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
    int reliableSequence;
    int reliableAcknowledge;
    int reliableSent;
    int messageAcknowledge;

    int gamestateMessageNum;
    int challenge;

    usercmd_t lastUsercmd;
    int lastMessageNum;
    int lastClientCommand;
    char lastClientCommandString[MAX_STRING_CHARS];
    sharedEntity_t* gentity;
    char name[MAX_NAME_LENGTH];

    char downloadName[MAX_QPATH];
    fileHandle_t download;
    int downloadSize;
    int downloadCount;
    int downloadClientBlock;
    int downloadCurrentBlock;
    int downloadXmitBlock;
    unsigned char* downloadBlocks[MAX_DOWNLOAD_WINDOW];
    int downloadBlockSize[MAX_DOWNLOAD_WINDOW];
    qboolean downloadEOF;
    int downloadSendTime;

    uint8_t _dead_space[3972];

    int deltaMessage;
    int nextReliableTime;
    float floodScore;
    int floodTime;
    float floodCount;
    int lastPacketTime;
    int lastConnectTime;
    int nextSnapshotTime;
    qboolean rateDelayed;
    int timeoutCount;
    clientSnapshot_t frames[PACKET_BACKUP];
    int ping;
    int rate;
    int snapshotMsec;
    int pureAuthentic;
    qboolean gotCP;
    qboolean csUpdated[MAX_CONFIGSTRINGS];
    netchan_t netchan;
    netchan_buffer_t* netchan_start_queue;
    netchan_buffer_t** netchan_end_queue;

    uint8_t _reserved[40];
    int rageQuit;
    int serverIdSequence;
    int oldServerTime;
    int _padding;
    uint64_t steam_id;
} client_t;

#define MAX_CHALLENGES 1024

#define AUTHORIZE_TIMEOUT 5000

typedef struct {
    netadr_t adr;
    int challenge;
    int time;
    int pingTime;
    int firstTime;
    qboolean connected;
    uint64_t steamKey;
    byte authToken[512];
    int authTokenLen;
    char wasRefused[132];
} challenge_t;

#define NUM_FACTORY_GAMETYPES 13
#define MAX_ARENA_INFOS 1024
#define MAX_FACTORIES 1024
#define MAX_FACTORY_TAGS 8
#define MAX_FACTORY_CVARS 256
#define MAX_MAPPOOL 1024

typedef struct {
    char map[1024];
    char longname[1024];
    int gametypes[NUM_FACTORY_GAMETYPES];
} arenaInfo_t;

typedef struct factory_s {
    char* id;
    char* title;
    char* author;
    char* description;
    int basegt;
    char* tags[MAX_FACTORY_TAGS];
    struct {
        char* name;
        char* value;
    } cvars[MAX_FACTORY_CVARS];
} factory_t;

typedef struct {
    char map[1024];
    char longname[1024];
    char factory[1024];
    char factoryTitle[1024];
} mapPoolEntry_t;

typedef struct {
    qboolean initialized;
    int time;
    int snapFlagServerBit;
    client_t* clients;
    int numSnapshotEntities;
    int nextSnapshotEntities;
    entityState_t* snapshotEntities;
    challenge_t challenges[MAX_CHALLENGES];
    netadr_t redirectAddress;
    netadr_t authorizeAddress;
    factory_t* factory;
} serverStatic_t;

#define SERVER_MAXBANS 1024
typedef struct
{
    netadr_t ip;
    int subnet;

    qboolean isexception;
} serverBan_t;

extern serverStatic_t svs;
extern server_t sv;

extern cvar_t* sv_fps;
extern cvar_t* sv_timeout;
extern cvar_t* sv_zombietime;
extern cvar_t* sv_rconPassword;
extern cvar_t* sv_privatePassword;
extern cvar_t* sv_allowDownload;
extern cvar_t* sv_maxclients;

extern cvar_t* sv_privateClients;
extern cvar_t* sv_hostname;
extern cvar_t* sv_reconnectlimit;
extern cvar_t* sv_showloss;
extern cvar_t* sv_padPackets;
extern cvar_t* sv_killserver;
extern cvar_t* sv_mapname;
extern cvar_t* sv_mapChecksum;
extern cvar_t* sv_serverid;
extern cvar_t* sv_minRate;
extern cvar_t* sv_maxRate;
extern cvar_t* sv_dlRate;
extern cvar_t* sv_minPing;
extern cvar_t* sv_maxPing;
extern cvar_t* sv_serverType;
extern cvar_t* sv_gametype;
extern cvar_t* sv_ammoPack;
extern cvar_t* sv_mapPoolFile;
extern cvar_t* sv_includeCurrentMapInVote;
extern cvar_t* sv_pure;
extern cvar_t* sv_floodProtect;
extern cvar_t* sv_master;
extern cvar_t* sv_idleExit;
extern cvar_t* sv_errorExit;
extern cvar_t* sv_workshopFile;
extern cvar_t* sv_quitOnEmpty;
extern cvar_t* sv_fullRedirect;
extern cvar_t* sv_lanForceRate;

void SV_FullRedirect(netadr_t from, int challenge);
extern cvar_t* sv_altEntDir;
extern cvar_t* sv_dumpEntities;
extern cvar_t* sv_banFile;

extern serverBan_t serverBans[SERVER_MAXBANS];
extern int serverBansCount;

typedef struct leakyBucket_s leakyBucket_t;
struct leakyBucket_s {
    netadrtype_t type;

    union {
        byte _4[4];
        byte _6[16];
    } ipv;

    int lastTime;
    signed char burst;

    long hash;

    leakyBucket_t *prev, *next;
};

extern leakyBucket_t outboundLeakyBucket;

qboolean SVC_RateLimit(leakyBucket_t* bucket, int burst, int period);
qboolean SVC_RateLimitAddress(netadr_t from, int burst, int period);

void SV_FinalMessage(char* message);
void QDECL SV_SendServerCommand(client_t* cl, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

void SV_AddOperatorCommands(void);
void SV_RemoveOperatorCommands(void);

int SV_RateMsec(client_t* client);

void SV_SetConfigstring(int index, const char* val);
void SV_GetConfigstring(int index, char* buffer, int bufferSize);
void SV_UpdateConfigstrings(client_t* client);

void SV_SetUserinfo(int index, const char* val);
void SV_GetUserinfo(int index, char* buffer, int bufferSize);

void SV_ChangeMaxClients(void);
void SV_SpawnServer(char* server, qboolean killBots);

void SV_LoadArenas(void);
const arenaInfo_t* SV_FindArena(const char* map);
int SV_NumArenas(void);
const arenaInfo_t* SV_ArenaByNum(int num);
void SV_ReloadArenas_f(void);

void SV_LoadFactories(void);
factory_t* SV_FindFactory(const char* id);
int SV_NumFactories(void);
const factory_t* SV_FactoryByNum(int num);
void SV_PrintValidFactories(void);
void SV_ReloadFactories_f(void);
void SV_ApplyFactory(factory_t* factory);
void SV_RestoreFactoryCvars(const factory_t* factory);

void SV_LoadMapPool(const char* filename);
void SV_ReloadMapPool_f(void);
void SV_StartRandomMap_f(void);
void SV_SetNextMap(void);
void SV_SetNextMaps(void);

void SV_GetChallenge(netadr_t from, msg_t* msg);

void SV_DirectConnect(netadr_t from);

void SV_ExecuteClientMessage(client_t* cl, msg_t* msg);
void SV_UserinfoChanged(client_t* cl);

void SV_ClientEnterWorld(client_t* client, usercmd_t* cmd);
void SV_FreeClient(client_t* client);
void SV_DropClient(client_t* drop, const char* reason);

void SV_ExecuteClientCommand(client_t* cl, const char* s, qboolean clientOK);
void SV_ClientThink(client_t* cl, usercmd_t* cmd);

int SV_WriteDownloadToClient(client_t* cl, msg_t* msg);
int SV_SendDownloadMessages(void);
int SV_SendQueuedMessages(void);

void SV_AddServerCommand(client_t* client, const char* cmd);
void SV_UpdateServerCommandsToClient(client_t* client, msg_t* msg);
void SV_WriteFrameToClient(client_t* client, msg_t* msg);
void SV_SendMessageToClient(msg_t* msg, client_t* client);
void SV_SendClientMessages(void);
void SV_SendClientSnapshot(client_t* client);

int SV_NumForGentity(sharedEntity_t* ent);
sharedEntity_t* SV_GentityNum(int num);
playerState_t* SV_GameClientNum(int num);
svEntity_t* SV_SvEntityForGentity(sharedEntity_t* gEnt);
sharedEntity_t* SV_GEntityForSvEntity(svEntity_t* svEnt);
void SV_InitGameProgs(void);
void SV_ShutdownGameProgs(void);
void SV_RestartGameProgs(void);
qboolean SV_inPVS(const vec3_t p1, const vec3_t p2);

void SV_GameShutdown(int restart);
void SV_GameRunFrame(int serverTime);
void SV_GameInit(int levelTime, int randomSeed, int restart);
qboolean SV_GameConsoleCommand(void);
const char *SV_GameClientConnect(int clientNum, qboolean firstTime, qboolean isBot);
void SV_GameClientUserinfoChanged(int clientNum);
void SV_GameClientThink(int clientNum);
void SV_GameClientDisconnect(int clientNum);
void SV_GameClientCommand(int clientNum);
qboolean SV_GameSnapshotVisibility(int viewerClientNum, int entityNum);
qboolean SV_GameSnapshotAlwaysSend(int viewerNum, int clientNum);
void SV_GameBotAIStartFrame(int time);
int SV_GameVoiceFilter(int speaker, int listener);
int SV_GameClientScore(int clientNum);
void SV_GameClientBegin(int clientNum);
qboolean SV_GameSnapshotSkipEntity(int viewerNum, int entityNum);
qboolean SV_GameClientIsAdmin(int clientNum);
qboolean SV_GameEntityAlwaysSend(int entityNum);
void SV_RedirectedCommand(netadr_t to, const char* text);

void SV_SteamStatsAddUser(uint64_t steamId);
void SV_SteamStatsConnected(void);
void SV_SteamStatsIncrement(uint64_t steamId, int stat, int amount);
void SV_SteamStatsSetAchievement(uint64_t steamId, int achievement);
qboolean SV_SteamStatsHasAchievement(uint64_t steamId, int achievement);
void SV_SteamStatsEvent(uint64_t steamId, const char* type, const char* document);
void SV_VoiceFrame(void);
void SV_SteamIdentity(void);
void SV_RegisterGameCvars(void);

void SV_BotFrame(int time);
int SV_BotAllocateClient(void);
void SV_BotFreeClient(int clientNum);

void SV_BotInitCvars(void);
int SV_BotLibSetup(void);
int SV_BotLibShutdown(void);
int SV_BotGetSnapshotEntity(int client, int ent);
int SV_BotGetConsoleMessage(int client, char* buf, int size);

int BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t* points);
void BotImport_DebugPolygonDelete(int id);

void SV_BotInitBotLib(void);

void SV_ClearWorld(void);

void SV_UnlinkEntity(sharedEntity_t* ent);

void SV_LinkEntity(sharedEntity_t* ent);

clipHandle_t SV_ClipHandleForEntity(const sharedEntity_t* ent);

void SV_SectorList_f(void);

int SV_AreaEntities(const vec3_t mins, const vec3_t maxs, int* entityList, int maxcount);

int SV_PointContents(const vec3_t p, int passEntityNum);

void SV_Trace(trace_t* results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, int capsule);

void SV_ClipToEntity(trace_t* trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask, int capsule);

void SV_Netchan_Transmit(client_t* client, msg_t* msg);
int SV_Netchan_TransmitNextFragment(client_t* client);
qboolean SV_Netchan_Process(client_t* client, msg_t* msg);
void SV_Netchan_FreeQueue(client_t* client);
