
#ifndef Q_STEAM_H
#define Q_STEAM_H

#include "q_shared.h"
#include "../qcommon/qcommon.h"

void Steam_Init(void);
void Steam_GS_Init(void);

void Steam_Frame(void);

void Steam_GS_SetMaxPlayers(int maxPlayers);

void Steam_GS_SetPassword(qboolean needPassword);
void Steam_GS_SetName(const char* name);
void Steam_GS_SetMap(const char* map);
void Steam_GS_SetBotCount(int bots);
void Steam_GS_SetKeyValue(const char* key, const char* value);
void Steam_GS_SetKeyValues(const char* info);
void Steam_GS_UpdateUser(uint64_t steamId, const char* name, int score);
void Steam_GS_UpdateGameInfo(qboolean force, char* const* factoryTags, int numFactoryTags);

qboolean Steam_RequestServerList(int source);
void Steam_ServerList_f(void);
qboolean Steam_ServerListRefreshing(void);
qboolean Steam_RefreshServerList(void);
int Steam_ServerListNextUpdated(void);
const char* Steam_ServerListName(int index);

#define STEAM_DETAILS_NONE 0
#define STEAM_DETAILS_LOADING 1
#define STEAM_DETAILS_DONE 2
#define STEAM_DETAILS_FAILED 3
qboolean Steam_RequestServerDetails(unsigned int ip, unsigned short port);
int Steam_ServerDetails(char* rules, int rulesSize, char* players, int playersSize);
qboolean Steam_SetFavorite(unsigned int ip, unsigned short port, qboolean favorite);

qboolean Steam_WorkshopUGC(void);
qboolean Steam_WorkshopQueue(uint64_t item, qboolean queued);
qboolean Steam_WorkshopDone(void);
int Steam_WorkshopPending(void);
uint64_t Steam_WorkshopCurrent(void);
qboolean Steam_WorkshopProgress(uint64_t item, uint64_t* done, uint64_t* total);
qboolean Steam_WorkshopPath(uint64_t item, char* buf, int size);
int Steam_WorkshopSubscribed(uint64_t* items, int max);
const char* Steam_WorkshopFolder(int index, uint64_t* item);
void Steam_DownloadUGC_f(void);
#ifndef DEDICATED
void Steam_SubscribeUGC_f(void);
void Steam_UnsubscribeUGC_f(void);
#endif
qboolean Steam_GS_LoggedOn(void);
void Steam_GS_RunCallbacks(void);
uint64_t Steam_GS_SteamID(void);
int Steam_ServerListCount(void);
qboolean Steam_ServerListGet(int index, unsigned int* ip, unsigned short* port, int* ping, char* info,
                             int infoSize);

qboolean Steam_GS_HandlePacket(netadr_t from, const void* data, int len);

void Steam_Shutdown(void);

qboolean Steam_GS_Running(void);

#define STEAM_AUTH_BEGIN_OK 0
#define STEAM_AUTH_BEGIN_DUPLICATE 2
int Steam_AuthTicket(void* buffer, int size);
qboolean Steam_GS_BeginAuth(uint64_t steamId, const void* ticket, int len);
void Steam_GS_EndAuth(uint64_t steamId);
const char* Steam_AuthResponseName(int response);

void Steam_GS_RequestUserStats(uint64_t steamId);
qboolean Steam_GS_GetUserStat(uint64_t steamId, const char* name, int* value);
qboolean Steam_GS_SetUserStat(uint64_t steamId, const char* name, int value);
qboolean Steam_GS_SetUserAchievement(uint64_t steamId, const char* name);
void Steam_GS_StoreUserStats(uint64_t steamId);
qboolean Steam_SetAchievement(const char* name);
void Steam_StoreStats(void);

#define STEAM_STATS_NONE 0
#define STEAM_STATS_LOADING 1
#define STEAM_STATS_DONE 2
#define STEAM_STATS_FAILED 3
int Steam_UserStats(uint64_t steamId, qboolean request, char* name, int nameSize, int* stats, int numStats);

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
qboolean Steam_GS_SendReport(uint64_t to, const void* data, int size);
void* Steam_P2PReadReport(int* size);
uint64_t Steam_ClientSteamID(void);
void Steam_ShowFriendAdd(uint64_t steamId);
void Steam_ShowProfile(uint64_t steamId);
void Steam_ShowChat(uint64_t steamId);
void Steam_ShowAchievements(uint64_t steamId);
qboolean Steam_InviteToGame(uint64_t steamId, const char* connect);
unsigned int Steam_GS_PublicIP(void);
qboolean Steam_Avatar(uint64_t steamId, char* path, int size);
void Steam_ShowFriends(void);

typedef struct {
    uint64_t steamId;
    char name[64];
    int state;
    qboolean playing;
    qboolean sameGame;
    unsigned int ip;
    unsigned short port;
    qboolean lobby;
    char nickname[64];
    char status[128];
} steamFriend_t;

int Steam_FriendCount(void);
qboolean Steam_FriendGet(int index, steamFriend_t* out);
const char* Steam_FriendStateName(int state);
void Steam_SetRichPresence(const char* status);

#endif
