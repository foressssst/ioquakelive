#include "client.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#else
#include <dlfcn.h>
#endif

#define CL_UPDATE_URL "https://gist.githubusercontent.com/foressssst/65861cb796e4d984cfd73b384e8c2098/raw/ioquakelive-version.txt"
#define CL_UPDATE_TIMEOUT_MS 5000

enum { CLU_IDLE, CLU_RUNNING, CLU_DONE, CLU_FAILED, CLU_HANDLED };

static char clUpdateText[1024];
static volatile long clUpdateState;
static cvar_t* cl_checkUpdates;

static void CL_UpdateSetState(long state) {
#ifdef _MSC_VER
    InterlockedExchange(&clUpdateState, state);
#else
    __atomic_store_n(&clUpdateState, state, __ATOMIC_RELEASE);
#endif
}

static long CL_UpdateGetState(void) {
#ifdef _MSC_VER
    return InterlockedCompareExchange(&clUpdateState, 0, 0);
#else
    return __atomic_load_n(&clUpdateState, __ATOMIC_ACQUIRE);
#endif
}

#ifdef _WIN32
static qboolean CL_UpdateFetch(char* out, int size) {
    URL_COMPONENTS uc;
    wchar_t url[512], host[256], path[512];
    HINTERNET session = NULL, connect = NULL, request = NULL;
    DWORD status = 0, statusLen = sizeof(status), got = 0;
    int used = 0;
    qboolean ok = qfalse;

    out[0] = '\0';
    if (!MultiByteToWideChar(CP_UTF8, 0, CL_UPDATE_URL, -1, url, ARRAY_LEN(url))) {
        return qfalse;
    }
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = ARRAY_LEN(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = ARRAY_LEN(path);
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) {
        return qfalse;
    }
    session = WinHttpOpen(L"ioquakelive", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, CL_UPDATE_TIMEOUT_MS, CL_UPDATE_TIMEOUT_MS, CL_UPDATE_TIMEOUT_MS,
                           CL_UPDATE_TIMEOUT_MS);
        connect = WinHttpConnect(session, host, uc.nPort, 0);
    }
    if (connect) {
        request = WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, NULL) &&
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &statusLen, WINHTTP_NO_HEADER_INDEX) &&
        status == 200) {
        while (used < size - 1 && WinHttpReadData(request, out + used, (DWORD)(size - 1 - used), &got) && got > 0) {
            used += (int)got;
        }
        out[used] = '\0';
        ok = (qboolean)(used > 0);
    }
    if (request) {
        WinHttpCloseHandle(request);
    }
    if (connect) {
        WinHttpCloseHandle(connect);
    }
    if (session) {
        WinHttpCloseHandle(session);
    }
    return ok;
}
#else
#define CLU_CURLOPT_WRITEDATA 10001
#define CLU_CURLOPT_URL 10002
#define CLU_CURLOPT_TIMEOUT 13
#define CLU_CURLOPT_USERAGENT 10018
#define CLU_CURLOPT_WRITEFUNCTION 20011
#define CLU_CURLOPT_FOLLOWLOCATION 52
#define CLU_CURLOPT_CONNECTTIMEOUT 78
#define CLU_CURLOPT_NOSIGNAL 99
#define CLU_CURLINFO_RESPONSE_CODE 0x200002

typedef struct {
    char* buf;
    int size;
    int used;
} clUpdateSink_t;

static size_t CL_UpdateWrite(char* data, size_t size, size_t count, void* user) {
    clUpdateSink_t* sink = (clUpdateSink_t*)user;
    size_t n = size * count;
    size_t room = (size_t)(sink->size - 1 - sink->used);

    if (n > room) {
        n = room;
    }
    memcpy(sink->buf + sink->used, data, n);
    sink->used += (int)n;
    return size * count;
}

static qboolean CL_UpdateFetch(char* out, int size) {
    static const char* const names[] = {"libcurl.so.4", "libcurl.so", "libcurl.4.dylib", "/usr/lib/libcurl.4.dylib",
                                        "libcurl.dylib"};
    void* (*easyInit)(void);
    int (*easySetopt)(void*, int, ...);
    int (*easyPerform)(void*);
    int (*easyGetinfo)(void*, int, ...);
    void (*easyCleanup)(void*);
    clUpdateSink_t sink;
    void* lib = NULL;
    void* curl;
    long code = 0;
    qboolean ok = qfalse;
    int i;

    out[0] = '\0';
    for (i = 0; i < (int)ARRAY_LEN(names) && !lib; i++) {
        lib = dlopen(names[i], RTLD_NOW);
    }
    if (!lib) {
        return qfalse;
    }
    *(void**)&easyInit = dlsym(lib, "curl_easy_init");
    *(void**)&easySetopt = dlsym(lib, "curl_easy_setopt");
    *(void**)&easyPerform = dlsym(lib, "curl_easy_perform");
    *(void**)&easyGetinfo = dlsym(lib, "curl_easy_getinfo");
    *(void**)&easyCleanup = dlsym(lib, "curl_easy_cleanup");
    if (easyInit && easySetopt && easyPerform && easyGetinfo && easyCleanup && (curl = easyInit()) != NULL) {
        sink.buf = out;
        sink.size = size;
        sink.used = 0;
        easySetopt(curl, CLU_CURLOPT_URL, CL_UPDATE_URL);
        easySetopt(curl, CLU_CURLOPT_WRITEFUNCTION, CL_UpdateWrite);
        easySetopt(curl, CLU_CURLOPT_WRITEDATA, &sink);
        easySetopt(curl, CLU_CURLOPT_TIMEOUT, (long)(CL_UPDATE_TIMEOUT_MS / 1000));
        easySetopt(curl, CLU_CURLOPT_CONNECTTIMEOUT, (long)(CL_UPDATE_TIMEOUT_MS / 1000));
        easySetopt(curl, CLU_CURLOPT_FOLLOWLOCATION, 1L);
        easySetopt(curl, CLU_CURLOPT_NOSIGNAL, 1L);
        easySetopt(curl, CLU_CURLOPT_USERAGENT, "ioquakelive");
        if (easyPerform(curl) == 0 && easyGetinfo(curl, CLU_CURLINFO_RESPONSE_CODE, &code) == 0 && code == 200 &&
            sink.used > 0) {
            out[sink.used] = '\0';
            ok = qtrue;
        }
        easyCleanup(curl);
    }
    dlclose(lib);
    return ok;
}
#endif

static void CL_UpdateThread(void* unused) {
    (void)unused;
    CL_UpdateSetState(CL_UpdateFetch(clUpdateText, sizeof(clUpdateText)) ? CLU_DONE : CLU_FAILED);
}

static int CL_UpdateCompare(const char* a, const char* b) {
    int x[3] = {0, 0, 0};
    int y[3] = {0, 0, 0};
    int i;

    sscanf(a, "%d.%d.%d", &x[0], &x[1], &x[2]);
    sscanf(b, "%d.%d.%d", &y[0], &y[1], &y[2]);
    for (i = 0; i < 3; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

void CL_UpdateInit(void) {
    cl_checkUpdates = Cvar_Get("cl_checkUpdates", "1", CVAR_ARCHIVE);
    Cvar_Get("cl_updateVersion", "", CVAR_ROM);
    Cvar_Get("cl_updateURL", "", CVAR_ROM);
    if (!cl_checkUpdates->integer || CL_UpdateGetState() != CLU_IDLE) {
        return;
    }
    CL_UpdateSetState(CLU_RUNNING);
    if (!Sys_ThreadCreate(CL_UpdateThread, NULL)) {
        CL_UpdateSetState(CLU_FAILED);
    }
}

void CL_UpdateFrame(void) {
    char version[32];
    char url[512];
    const char* p;
    long state = CL_UpdateGetState();

    if (state != CLU_DONE && state != CLU_FAILED) {
        return;
    }
    CL_UpdateSetState(CLU_HANDLED);
    if (state == CLU_FAILED) {
        Com_DPrintf("update check: no answer\n");
        return;
    }
    if (sscanf(clUpdateText, "%31s %511s", version, url) != 2) {
        return;
    }
    for (p = version; *p; p++) {
        if ((*p < '0' || *p > '9') && *p != '.') {
            return;
        }
    }
    if (Q_strncmp(url, "https://", 8) || CL_UpdateCompare(version, PRODUCT_VERSION) <= 0) {
        return;
    }
    Cvar_Set("cl_updateVersion", version);
    Cvar_Set("cl_updateURL", url);
    Com_Printf("A new version of ioquakelive is available: %s\n%s\n", version, url);
}
