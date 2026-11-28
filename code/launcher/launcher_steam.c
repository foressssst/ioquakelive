#include <SDL3/SDL.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#include "launcher_steam.h"

#define VT(self, slot) ((*(void***)(self))[slot])
#define QL_APP 282440u
#define AVATAR_QUEUE 64
#define AVATAR_SEEN 600
#define JOIN_QUEUE 4
#define INVITE_QUEUE 16
#define ADDR_SIZE 64
#define CB_PERSONA 304
#define CB_SERVER 332
#define CB_AVATAR 334
#define CB_PRESENCE 336
#define CB_JOIN 337
#define WORLD_WAIT 30000
#define WORLD_POLL 400

typedef struct {
    int user;
    int id;
    unsigned char* param;
    int size;
} callbackMsg_t;

typedef struct {
    Uint64 game;
    Uint32 ip;
    Uint16 port;
    Uint16 query;
    Uint64 lobby;
} gameInfo_t;

typedef struct {
    Uint64 to;
    char address[ADDR_SIZE];
    bool ok;
} invite_t;

typedef struct {
    Uint64 id;
    int handle;
} seen_t;

typedef struct {
    char key[256];
    char value[256];
} keyValue_t;

#pragma pack(push, 1)
typedef struct {
    Uint16 port;
    Uint16 query;
    Uint32 ip;
    int ping;
    Uint8 responded;
    Uint8 still;
    char dir[32];
    char map[32];
    char desc[64];
    Uint8 pad0[2];
    Uint32 app;
    int players;
    int max;
    int bots;
    Uint8 password;
    Uint8 secure;
    Uint8 pad1[2];
    Uint32 played;
    int version;
    char name[64];
    char tags[128];
    Uint64 id;
} serverItem_t;
#pragma pack(pop)

typedef struct {
    void** vtable;
} response_t;

static SDL_Thread* steamThread;
static SDL_Mutex* steamLock;
static SDL_AtomicInt steamQuit, steamWant, steamState, steamVersion;

static steamFriend_t shared[STEAMF_MAX];
static int sharedCount, sharedPersona;
static char sharedMe[96];
static Uint64 sharedMeId;
static char joinQueue[JOIN_QUEUE][ADDR_SIZE];
static int joinHead, joinTail;
static invite_t inviteOut[INVITE_QUEUE], inviteBack[INVITE_QUEUE];
static int outHead, outTail, backHead, backTail;
static steamAvatar_t avatarQueue[AVATAR_QUEUE];
static int avHead, avTail;

static void* steamLib;
static void* steamClient;
static void* steamFriends;
static void* steamUtils;
static void* steamServers;
static SDL_AtomicInt worldAsk, worldState;
static steamWorld_t worldShared[STEAMW_MAX], worldWork[STEAMW_MAX];
static int worldSharedCount, worldWorkCount;
static Sint16 worldSlot[STEAMW_MAX];
static void* worldRequest;
static void* worldTable[3];
static response_t worldResponse;
static keyValue_t worldFilter;
static keyValue_t* worldFilters[1];
static bool worldDone;
static Uint64 worldUntil, worldNext;
static int steamPipe, steamUser;
static void* (*CreateInterface)(const char*, int*);
static bool (*BGetCallback)(int, callbackMsg_t*, int*);
static void (*FreeLastCallback)(int);

static steamFriend_t work[STEAMF_MAX];
static int avatarHandle[STEAMF_MAX];
static seen_t seen[AVATAR_SEEN];
static int seenCount, workCount;
static Uint64 presenceAsked[STEAMF_MAX];
static int presenceAskedCount;

static void Steam_Bump(void) {
    SDL_Event wake;

    SDL_AddAtomicInt(&steamVersion, 1);
    SDL_zero(wake);
    wake.type = SDL_EVENT_USER;
    SDL_PushEvent(&wake);
}

static void Steam_Env(const char* value) {
#ifdef _WIN32
    SetEnvironmentVariableA("SteamAppId", value);
    SetEnvironmentVariableA("SteamGameId", value);
#else
    if (value) {
        setenv("SteamAppId", value, 1);
        setenv("SteamGameId", value, 1);
    } else {
        unsetenv("SteamAppId");
        unsetenv("SteamGameId");
    }
#endif
}

static void* Lib_Function(const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)steamLib, name);
#else
    return (void*)SDL_LoadFunction((SDL_SharedObject*)steamLib, name);
#endif
}

static void* Lib_Open(void) {
#ifdef _WIN32
    wchar_t wide[1024];
    DWORD bytes = sizeof(wide);
    HMODULE lib = NULL;
    int i;

    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll64", RRF_RT_REG_SZ, NULL, wide, &bytes) == ERROR_SUCCESS) {
        lib = LoadLibraryExW(wide, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    bytes = sizeof(wide) - 64;
    if (!lib && RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, NULL, wide, &bytes) == ERROR_SUCCESS) {
        for (i = 0; wide[i]; i++) {
            if (wide[i] == L'/') {
                wide[i] = L'\\';
            }
        }
        if (i > 0 && wide[i - 1] != L'\\') {
            wide[i++] = L'\\';
        }
        SDL_memcpy(wide + i, L"steamclient64.dll", sizeof(L"steamclient64.dll"));
        lib = LoadLibraryExW(wide, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    return (void*)lib;
#else
    static const char* const places[] = {
#ifdef __APPLE__
        "Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steamclient.dylib",
#else
        ".steam/sdk64/steamclient.so",
        ".steam/steam/linux64/steamclient.so",
        ".local/share/Steam/linux64/steamclient.so",
        ".var/app/com.valvesoftware.Steam/.local/share/Steam/linux64/steamclient.so",
#endif
    };
    const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    char path[1200];
    SDL_SharedObject* lib = NULL;
    int i;

    for (i = 0; !lib && home && i < (int)SDL_arraysize(places); i++) {
        SDL_snprintf(path, sizeof(path), "%s%s", home, places[i]);
        lib = SDL_LoadObject(path);
    }
#ifdef __APPLE__
    if (!lib) {
        lib = SDL_LoadObject("/Applications/Steam.app/Contents/MacOS/steamclient.dylib");
    }
#endif
    return (void*)lib;
#endif
}

static bool Steam_Load(void) {
    if (steamClient) {
        return true;
    }
    if (!steamLib) {
        steamLib = Lib_Open();
    }
    if (!steamLib) {
        return false;
    }
    *(void**)&CreateInterface = Lib_Function("CreateInterface");
    *(void**)&BGetCallback = Lib_Function("Steam_BGetCallback");
    *(void**)&FreeLastCallback = Lib_Function("Steam_FreeLastCallback");
    if (!CreateInterface || !BGetCallback || !FreeLastCallback) {
        return false;
    }
    steamClient = CreateInterface("SteamClient017", NULL);
    return steamClient != NULL;
}

static Uint64 Steam_Self(void* user) {
    Uint64 id = 0;

    if (!user) {
        return 0;
    }
#ifdef _WIN32
    ((void* (*)(void*, Uint64*))VT(user, 2))(user, &id);
#else
    id = ((Uint64(*)(void*))VT(user, 2))(user);
#endif
    return id;
}

static Uint64 Friend_ByIndex(int index) {
    Uint64 id = 0;

#ifdef _WIN32
    ((void* (*)(void*, Uint64*, int, int))VT(steamFriends, 4))(steamFriends, &id, index, 4);
#else
    id = ((Uint64(*)(void*, int, int))VT(steamFriends, 4))(steamFriends, index, 4);
#endif
    return id;
}

static const char* Friend_Text(int slot, Uint64 id) {
    const char* s = ((const char* (*)(void*, Uint64))VT(steamFriends, slot))(steamFriends, id);

    return s ? s : "";
}

static const char* Friend_Presence(Uint64 id, const char* key) {
    const char* s = ((const char* (*)(void*, Uint64, const char*))VT(steamFriends, 45))(steamFriends, id, key);

    return s ? s : "";
}

static void World_Responded(void* self, void* request, int index) {
    (void)self;
    (void)request;
    (void)index;
}

static void World_Failed(void* self, void* request, int index) {
    (void)self;
    (void)request;
    (void)index;
}

static void World_Complete(void* self, void* request, int response) {
    (void)self;
    (void)request;
    (void)response;
    worldDone = true;
}

static void World_Release(void) {
    if (worldRequest && steamServers) {
        ((void (*)(void*, void*))VT(steamServers, 6))(steamServers, worldRequest);
    }
    worldRequest = NULL;
}

static int World_Order(const void* a, const void* b) {
    const steamWorld_t* x = (const steamWorld_t*)a;
    const steamWorld_t* y = (const steamWorld_t*)b;
    int hx = x->players - x->bots, hy = y->players - y->bots;

    if (hx != hy) {
        return hy - hx;
    }
    return x->ping - y->ping;
}

static void World_Publish(void) {
    SDL_LockMutex(steamLock);
    SDL_memcpy(worldShared, worldWork, sizeof(steamWorld_t) * (size_t)worldWorkCount);
    worldSharedCount = worldWorkCount;
    SDL_qsort(worldShared, (size_t)worldSharedCount, sizeof(steamWorld_t), World_Order);
    SDL_UnlockMutex(steamLock);
    Steam_Bump();
}

static void World_Poll(void) {
    int count = ((int (*)(void*, void*))VT(steamServers, 11))(steamServers, worldRequest), i;
    bool changed = false;

    for (i = 0; i < count && i < STEAMW_MAX; i++) {
        const serverItem_t* it = ((const serverItem_t* (*)(void*, void*, int))VT(steamServers, 7))(steamServers, worldRequest, i);
        steamWorld_t* w;

        if (!it || !it->responded || it->app != QL_APP) {
            continue;
        }
        if (worldSlot[i]) {
            w = &worldWork[worldSlot[i] - 1];
        } else {
            if (worldWorkCount >= STEAMW_MAX) {
                continue;
            }
            worldSlot[i] = (Sint16)(worldWorkCount + 1);
            w = &worldWork[worldWorkCount++];
        }
        SDL_memcpy(w->name, it->name, sizeof(w->name));
        w->name[sizeof(w->name) - 1] = '\0';
        SDL_memcpy(w->map, it->map, sizeof(w->map));
        w->map[sizeof(w->map) - 1] = '\0';
        SDL_memcpy(w->tags, it->tags, sizeof(w->tags));
        w->tags[sizeof(w->tags) - 1] = '\0';
        SDL_snprintf(w->addr, sizeof(w->addr), "%u.%u.%u.%u:%u", (it->ip >> 24) & 255u, (it->ip >> 16) & 255u, (it->ip >> 8) & 255u, it->ip & 255u,
                     (unsigned)it->port);
        w->players = it->players;
        w->max = it->max;
        w->bots = it->bots;
        w->ping = it->ping;
        w->password = it->password != 0;
        changed = true;
    }
    if (changed && worldWorkCount >= worldSharedCount) {
        World_Publish();
    }
}

static void World_Start(Uint64 now) {
    World_Release();
    worldWorkCount = 0;
    SDL_memset(worldSlot, 0, sizeof(worldSlot));
    worldDone = false;
    worldTable[0] = (void*)World_Responded;
    worldTable[1] = (void*)World_Failed;
    worldTable[2] = (void*)World_Complete;
    worldResponse.vtable = worldTable;
    SDL_zero(worldFilter);
    SDL_strlcpy(worldFilter.key, "gamedir", sizeof(worldFilter.key));
    SDL_strlcpy(worldFilter.value, "baseq3", sizeof(worldFilter.value));
    worldFilters[0] = &worldFilter;
    if (steamServers) {
        worldRequest = ((void* (*)(void*, Uint32, keyValue_t**, Uint32, void*))VT(steamServers, 0))(steamServers, QL_APP, worldFilters, 1, &worldResponse);
    }
    SDL_SetAtomicInt(&worldState, worldRequest ? STEAMW_LOADING : STEAMW_FAILED);
    worldUntil = now + WORLD_WAIT;
    worldNext = now + WORLD_POLL;
    Steam_Bump();
}

static void World_Run(Uint64 now) {
    if (SDL_GetAtomicInt(&worldAsk)) {
        SDL_SetAtomicInt(&worldAsk, 0);
        World_Start(now);
    }
    if (!worldRequest) {
        return;
    }
    ((void (*)(void*))VT(steamClient, 19))(steamClient);
    if (now < worldNext && !worldDone) {
        return;
    }
    worldNext = now + WORLD_POLL;
    World_Poll();
    if (worldDone || now >= worldUntil) {
        World_Release();
        World_Publish();
        SDL_SetAtomicInt(&worldState, STEAMW_DONE);
        Steam_Bump();
    }
}

static void Steam_Disconnect(void) {
    World_Release();
    if (SDL_GetAtomicInt(&worldState) == STEAMW_LOADING) {
        SDL_SetAtomicInt(&worldState, STEAMW_FAILED);
    }
    if (steamUser) {
        ((void (*)(void*, int, int))VT(steamClient, 4))(steamClient, steamPipe, steamUser);
    }
    if (steamPipe) {
        ((bool (*)(void*, int))VT(steamClient, 1))(steamClient, steamPipe);
        ((bool (*)(void*))VT(steamClient, 22))(steamClient);
    }
    steamPipe = steamUser = 0;
    steamFriends = steamUtils = steamServers = NULL;
    seenCount = presenceAskedCount = workCount = 0;
    SDL_LockMutex(steamLock);
    sharedCount = 0;
    sharedMe[0] = '\0';
    sharedMeId = 0;
    SDL_UnlockMutex(steamLock);
    Steam_Bump();
}

static int Steam_Connect(void) {
    void* user = NULL;
    Uint64 me;

    if (!Steam_Load()) {
        return STEAMF_MISSING;
    }
    Steam_Env("282440");
    steamPipe = ((int (*)(void*))VT(steamClient, 0))(steamClient);
    steamUser = steamPipe ? ((int (*)(void*, int))VT(steamClient, 2))(steamClient, steamPipe) : 0;
    Steam_Env(NULL);
    if (!steamPipe) {
        return STEAMF_DOWN;
    }
    if (steamUser) {
        steamUtils = ((void* (*)(void*, int, const char*))VT(steamClient, 9))(steamClient, steamPipe, "SteamUtils007");
        steamFriends = ((void* (*)(void*, int, int, const char*))VT(steamClient, 8))(steamClient, steamUser, steamPipe, "SteamFriends015");
        user = ((void* (*)(void*, int, int, const char*))VT(steamClient, 5))(steamClient, steamUser, steamPipe, "SteamUser018");
        steamServers = ((void* (*)(void*, int, int, const char*))VT(steamClient, 11))(steamClient, steamUser, steamPipe, "SteamMatchMakingServers002");
    }
    if (!steamUser || !steamUtils || !steamFriends || ((Uint32(*)(void*))VT(steamUtils, 9))(steamUtils) != QL_APP) {
        bool refused = steamUser != 0;

        Steam_Disconnect();
        return refused ? STEAMF_REFUSED : STEAMF_DOWN;
    }
    me = Steam_Self(user);
    SDL_LockMutex(steamLock);
    sharedMeId = me;
    SDL_UnlockMutex(steamLock);
    if (me && !Friend_Presence(me, "ioql_state")[0]) {
        ((bool (*)(void*, const char*, const char*))VT(steamFriends, 43))(steamFriends, "ioql_state", "launcher");
    }
    return STEAMF_UP;
}

static void Join_Push(const char* address) {
    SDL_LockMutex(steamLock);
    if ((joinHead + 1) % JOIN_QUEUE != joinTail) {
        SDL_strlcpy(joinQueue[joinHead], address, ADDR_SIZE);
        joinHead = (joinHead + 1) % JOIN_QUEUE;
    }
    SDL_UnlockMutex(steamLock);
    Steam_Bump();
}

static void Join_Address(const char* text) {
    char host[ADDR_SIZE], out[ADDR_SIZE];
    const char* colon;
    size_t n = 0, digits = 0;

    while (*text && *text != ' ' && *text != '\t' && *text != '\r' && *text != '\n' && n < sizeof(host) - 1) {
        host[n++] = *text++;
    }
    host[n] = '\0';
    colon = SDL_strchr(host, ':');
    n = colon ? (size_t)(colon - host) : n;
    while (digits < n && SDL_isdigit((unsigned char)host[digits])) {
        digits++;
    }
    if (n > 0 && digits == n && n <= 10) {
        Uint64 v = SDL_strtoull(host, NULL, 10);

        SDL_snprintf(out, sizeof(out), "%u.%u.%u.%u%s", (unsigned)(v >> 24) & 255u, (unsigned)(v >> 16) & 255u, (unsigned)(v >> 8) & 255u,
                     (unsigned)v & 255u, colon ? colon : "");
        Join_Push(out);
    } else if (host[0]) {
        Join_Push(host);
    }
}

static void Join_Connect(const char* text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == '+') {
        text++;
    }
    if (SDL_strncasecmp(text, "connect", 7) || (text[7] != ' ' && text[7] != '\t')) {
        return;
    }
    text += 7;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    Join_Address(text);
}

static bool Steam_Pump(void) {
    callbackMsg_t msg;
    int call;
    bool dirty = false;

    ((void (*)(void*))VT(steamUtils, 14))(steamUtils);
    while (BGetCallback(steamPipe, &msg, &call)) {
        if (msg.id == CB_PERSONA || msg.id == CB_PRESENCE || msg.id == CB_AVATAR) {
            dirty = true;
        } else if (msg.id == CB_JOIN && msg.param && msg.size > 8) {
            char text[260];
            int n = SDL_min(msg.size - 8, (int)sizeof(text) - 1);

            SDL_memcpy(text, msg.param + 8, (size_t)n);
            text[n] = '\0';
            Join_Connect(text);
        } else if (msg.id == CB_SERVER && msg.param && msg.size >= ADDR_SIZE) {
            char server[ADDR_SIZE + 1];

            SDL_memcpy(server, msg.param, ADDR_SIZE);
            server[ADDR_SIZE] = '\0';
            Join_Address(server);
        }
        FreeLastCallback(steamPipe);
    }
    return dirty;
}

static void Steam_Invites(void) {
    static invite_t item;
    char text[ADDR_SIZE + 16];

    for (;;) {
        SDL_LockMutex(steamLock);
        if (outTail == outHead) {
            SDL_UnlockMutex(steamLock);
            return;
        }
        item = inviteOut[outTail];
        outTail = (outTail + 1) % INVITE_QUEUE;
        SDL_UnlockMutex(steamLock);
        SDL_snprintf(text, sizeof(text), "+connect %s", item.address);
        item.ok = ((bool (*)(void*, Uint64, const char*))VT(steamFriends, 49))(steamFriends, item.to, text);
        SDL_LockMutex(steamLock);
        if ((backHead + 1) % INVITE_QUEUE != backTail) {
            inviteBack[backHead] = item;
            backHead = (backHead + 1) % INVITE_QUEUE;
        }
        SDL_UnlockMutex(steamLock);
        Steam_Bump();
    }
}

static bool Presence_Asked(Uint64 id) {
    int i;

    for (i = 0; i < presenceAskedCount; i++) {
        if (presenceAsked[i] == id) {
            return true;
        }
    }
    if (presenceAskedCount < STEAMF_MAX) {
        presenceAsked[presenceAskedCount++] = id;
    }
    return false;
}

static void Steam_Rebuild(void) {
    int n = ((int (*)(void*, int))VT(steamFriends, 3))(steamFriends, 4), count = 0, persona, i;
    const char* me = ((const char* (*)(void*))VT(steamFriends, 0))(steamFriends);

    persona = ((int (*)(void*))VT(steamFriends, 2))(steamFriends);
    for (i = 0; i < n && count < STEAMF_MAX; i++) {
        steamFriend_t* f = &work[count];
        gameInfo_t game;
        const char* nick;
        Uint64 id = Friend_ByIndex(i);

        if (!id) {
            continue;
        }
        SDL_zerop(f);
        SDL_zero(game);
        f->id = id;
        nick = Friend_Text(11, id);
        SDL_strlcpy(f->name, nick[0] ? nick : Friend_Text(7, id), sizeof(f->name));
        f->persona = ((int (*)(void*, Uint64))VT(steamFriends, 6))(steamFriends, id);
        if (((bool (*)(void*, Uint64, gameInfo_t*))VT(steamFriends, 8))(steamFriends, id, &game) && game.game) {
            f->game = (Uint32)(game.game & 0xFFFFFF) == QL_APP ? STEAMF_GAME_QL : STEAMF_GAME_OTHER;
        }
        if (f->game == STEAMF_GAME_QL) {
            if (!Presence_Asked(id)) {
                ((void (*)(void*, Uint64))VT(steamFriends, 48))(steamFriends, id);
            }
            SDL_strlcpy(f->state, Friend_Presence(id, "ioql_state"), sizeof(f->state));
            SDL_strlcpy(f->mode, Friend_Presence(id, "ioql_mode"), sizeof(f->mode));
            SDL_strlcpy(f->map, Friend_Presence(id, "ioql_map"), sizeof(f->map));
            SDL_strlcpy(f->host, Friend_Presence(id, "ioql_host"), sizeof(f->host));
            SDL_strlcpy(f->players, Friend_Presence(id, "ioql_players"), sizeof(f->players));
            SDL_strlcpy(f->connect, Friend_Presence(id, "ioql_connect"), sizeof(f->connect));
            f->since = (Sint64)SDL_strtoll(Friend_Presence(id, "ioql_since"), NULL, 10);
            if (!f->state[0]) {
                const char* status = Friend_Presence(id, "status");

                if (!SDL_strcmp(status, "Playing a match")) {
                    SDL_strlcpy(f->state, "match", sizeof(f->state));
                } else if (!SDL_strcmp(status, "At the main menu")) {
                    SDL_strlcpy(f->state, "menu", sizeof(f->state));
                }
            }
            if (!f->connect[0] && game.ip && game.port) {
                SDL_snprintf(f->connect, sizeof(f->connect), "%u.%u.%u.%u:%u", (game.ip >> 24) & 0xff, (game.ip >> 16) & 0xff,
                             (game.ip >> 8) & 0xff, game.ip & 0xff, (unsigned)game.port);
                if (!f->state[0]) {
                    SDL_strlcpy(f->state, "match", sizeof(f->state));
                }
            }
        }
        avatarHandle[count] = ((int (*)(void*, Uint64))VT(steamFriends, 35))(steamFriends, id);
        count++;
    }
    workCount = count;
    SDL_LockMutex(steamLock);
    SDL_memcpy(shared, work, sizeof(steamFriend_t) * (size_t)count);
    sharedCount = count;
    sharedPersona = persona;
    SDL_strlcpy(sharedMe, me ? me : "", sizeof(sharedMe));
    SDL_UnlockMutex(steamLock);
    Steam_Bump();
}

static int Avatar_Seen(Uint64 id) {
    int i;

    for (i = 0; i < seenCount; i++) {
        if (seen[i].id == id) {
            return i;
        }
    }
    return -1;
}

static void Steam_Avatars(void) {
    int i, fetched = 0;

    for (i = 0; i < workCount && fetched < 8; i++) {
        Uint32 w = 0, h = 0;
        int handle = avatarHandle[i], slot = Avatar_Seen(work[i].id);
        unsigned char* rgba;
        steamAvatar_t* a;

        if (handle <= 0 || (slot >= 0 && seen[slot].handle == handle)) {
            continue;
        }
        if (!((bool (*)(void*, int, Uint32*, Uint32*))VT(steamUtils, 5))(steamUtils, handle, &w, &h) || !w || !h || w > 512 || h > 512) {
            continue;
        }
        rgba = (unsigned char*)SDL_malloc((size_t)w * h * 4);
        if (!rgba) {
            continue;
        }
        if (!((bool (*)(void*, int, unsigned char*, int))VT(steamUtils, 6))(steamUtils, handle, rgba, (int)(w * h * 4))) {
            SDL_free(rgba);
            continue;
        }
        fetched++;
        if (slot < 0 && seenCount < AVATAR_SEEN) {
            slot = seenCount++;
            seen[slot].id = work[i].id;
        }
        if (slot >= 0) {
            seen[slot].handle = handle;
        }
        SDL_LockMutex(steamLock);
        if ((avHead + 1) % AVATAR_QUEUE != avTail) {
            a = &avatarQueue[avHead];
            a->id = work[i].id;
            a->w = (int)w;
            a->h = (int)h;
            a->rgba = rgba;
            avHead = (avHead + 1) % AVATAR_QUEUE;
            rgba = NULL;
        }
        SDL_UnlockMutex(steamLock);
        if (rgba) {
            SDL_free(rgba);
            if (slot >= 0) {
                seen[slot].handle = 0;
            }
            break;
        }
        Steam_Bump();
    }
}

static int SDLCALL Steam_Thread(void* data) {
    Uint64 nextTry = 0, nextBuild = 0, nextFull = 0;
    bool dirty = false;

    (void)data;
    while (!SDL_GetAtomicInt(&steamQuit)) {
        Uint64 now = SDL_GetTicks();

        if (!SDL_GetAtomicInt(&steamWant)) {
            if (steamPipe) {
                Steam_Disconnect();
            }
            if (SDL_GetAtomicInt(&steamState) != STEAMF_IDLE) {
                SDL_SetAtomicInt(&steamState, STEAMF_IDLE);
                Steam_Bump();
            }
            nextTry = 0;
            SDL_Delay(100);
            continue;
        }
        if (!steamPipe) {
            int result;

            if (now < nextTry) {
                SDL_Delay(100);
                continue;
            }
            SDL_SetAtomicInt(&steamState, STEAMF_TRYING);
            result = Steam_Connect();
            if (result == STEAMF_UP) {
                Steam_Rebuild();
                now = SDL_GetTicks();
                nextBuild = now + 500;
                nextFull = now + 30000;
                dirty = false;
            } else {
                nextTry = now + (result == STEAMF_MISSING ? 30000 : 5000);
            }
            SDL_SetAtomicInt(&steamState, result);
            Steam_Bump();
            continue;
        }
        if (Steam_Pump()) {
            dirty = true;
        }
        Steam_Invites();
        World_Run(now);
        if ((dirty && now >= nextBuild) || now >= nextFull) {
            Steam_Rebuild();
            dirty = false;
            nextBuild = now + 500;
            nextFull = now + 30000;
        }
        Steam_Avatars();
        SDL_Delay(50);
    }
    if (steamPipe) {
        Steam_Disconnect();
    }
    return 0;
}

void SteamF_Init(void) {
    if (steamThread) {
        return;
    }
    steamLock = SDL_CreateMutex();
    if (!steamLock) {
        return;
    }
    SDL_SetAtomicInt(&steamQuit, 0);
    steamThread = SDL_CreateThread(Steam_Thread, "steam", NULL);
}

void SteamF_Shutdown(void) {
    steamAvatar_t a;

    if (!steamThread) {
        return;
    }
    SDL_SetAtomicInt(&steamQuit, 1);
    SDL_WaitThread(steamThread, NULL);
    steamThread = NULL;
    while (SteamF_PopAvatar(&a)) {
        SDL_free(a.rgba);
    }
}

void SteamF_Want(bool want) {
    SDL_SetAtomicInt(&steamWant, want ? 1 : 0);
}

int SteamF_State(void) {
    return SDL_GetAtomicInt(&steamState);
}

int SteamF_Version(void) {
    return SDL_GetAtomicInt(&steamVersion);
}

Uint64 SteamF_Me(void) {
    Uint64 id;

    if (!steamLock) {
        return 0;
    }
    SDL_LockMutex(steamLock);
    id = sharedMeId;
    SDL_UnlockMutex(steamLock);
    return id;
}

int SteamF_Copy(steamFriend_t* out, int max, char* me, int meSize, int* myPersona) {
    int n;

    if (!steamLock) {
        return 0;
    }
    SDL_LockMutex(steamLock);
    n = SDL_min(max, sharedCount);
    SDL_memcpy(out, shared, sizeof(steamFriend_t) * (size_t)n);
    if (me) {
        SDL_strlcpy(me, sharedMe, (size_t)meSize);
    }
    if (myPersona) {
        *myPersona = sharedPersona;
    }
    SDL_UnlockMutex(steamLock);
    return n;
}

bool SteamF_PopAvatar(steamAvatar_t* out) {
    bool have = false;

    if (!steamLock) {
        return false;
    }
    SDL_LockMutex(steamLock);
    if (avTail != avHead) {
        *out = avatarQueue[avTail];
        avTail = (avTail + 1) % AVATAR_QUEUE;
        have = true;
    }
    SDL_UnlockMutex(steamLock);
    return have;
}

bool SteamF_PopJoin(char* out, int size) {
    bool have = false;

    if (!steamLock) {
        return false;
    }
    SDL_LockMutex(steamLock);
    if (joinTail != joinHead) {
        SDL_strlcpy(out, joinQueue[joinTail], (size_t)size);
        joinTail = (joinTail + 1) % JOIN_QUEUE;
        have = true;
    }
    SDL_UnlockMutex(steamLock);
    return have;
}

void SteamF_Invite(Uint64 to, const char* address) {
    if (!steamLock || !to || !address || !address[0]) {
        return;
    }
    SDL_LockMutex(steamLock);
    if ((outHead + 1) % INVITE_QUEUE != outTail) {
        inviteOut[outHead].to = to;
        inviteOut[outHead].ok = false;
        SDL_strlcpy(inviteOut[outHead].address, address, sizeof(inviteOut[outHead].address));
        outHead = (outHead + 1) % INVITE_QUEUE;
    }
    SDL_UnlockMutex(steamLock);
}

bool SteamF_PopInvite(Uint64* to, bool* ok) {
    bool have = false;

    if (!steamLock) {
        return false;
    }
    SDL_LockMutex(steamLock);
    if (backTail != backHead) {
        *to = inviteBack[backTail].to;
        *ok = inviteBack[backTail].ok;
        backTail = (backTail + 1) % INVITE_QUEUE;
        have = true;
    }
    SDL_UnlockMutex(steamLock);
    return have;
}

void SteamW_Ask(void) {
    SDL_SetAtomicInt(&worldAsk, 1);
}

int SteamW_State(void) {
    return SDL_GetAtomicInt(&worldState);
}

int SteamW_Copy(steamWorld_t* out, int max) {
    int n;

    if (!steamLock) {
        return 0;
    }
    SDL_LockMutex(steamLock);
    n = SDL_min(max, worldSharedCount);
    SDL_memcpy(out, worldShared, sizeof(steamWorld_t) * (size_t)n);
    SDL_UnlockMutex(steamLock);
    return n;
}
