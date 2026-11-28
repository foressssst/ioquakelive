#ifndef LAUNCHER_STEAM_H
#define LAUNCHER_STEAM_H

#include <SDL3/SDL.h>

#define STEAMF_MAX 500

enum { STEAMF_OFF, STEAMF_ONLINE, STEAMF_BUSY, STEAMF_AWAY, STEAMF_SNOOZE, STEAMF_TRADE, STEAMF_PLAY };
enum { STEAMF_GAME_NONE, STEAMF_GAME_QL, STEAMF_GAME_OTHER };
enum { STEAMF_IDLE, STEAMF_TRYING, STEAMF_UP, STEAMF_DOWN, STEAMF_REFUSED, STEAMF_MISSING };

typedef struct {
    Uint64 id;
    char name[96];
    int persona;
    int game;
    char state[16];
    char mode[64];
    char map[64];
    char host[96];
    char players[16];
    char connect[64];
    Sint64 since;
} steamFriend_t;

typedef struct {
    Uint64 id;
    int w, h;
    unsigned char* rgba;
} steamAvatar_t;

#define STEAMW_MAX 1024

enum { STEAMW_IDLE, STEAMW_LOADING, STEAMW_DONE, STEAMW_FAILED };

typedef struct {
    char name[64];
    char map[32];
    char tags[128];
    char addr[32];
    int players, max, bots, ping;
    bool password;
} steamWorld_t;

void SteamF_Init(void);
void SteamF_Shutdown(void);
void SteamF_Want(bool want);
int SteamF_State(void);
int SteamF_Version(void);
Uint64 SteamF_Me(void);
int SteamF_Copy(steamFriend_t* out, int max, char* me, int meSize, int* myPersona);
bool SteamF_PopAvatar(steamAvatar_t* out);
bool SteamF_PopJoin(char* out, int size);
void SteamF_Invite(Uint64 to, const char* address);
bool SteamF_PopInvite(Uint64* to, bool* ok);
void SteamW_Ask(void);
int SteamW_State(void);
int SteamW_Copy(steamWorld_t* out, int max);

#endif
