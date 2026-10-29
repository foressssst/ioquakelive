#include "client.h"
#include "../qcommon/json.h"
#include "../qcommon/unzip.h"
#include "tweetnacl.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#define CL_UPDATE_URL "https://gist.githubusercontent.com/foressssst/65861cb796e4d984cfd73b384e8c2098/raw/ioquakelive-version.txt"
#define CL_UPDATE_API "https://api.github.com/repos/foressssst/ioquakelive/releases?per_page=1"
#define CL_UPDATE_RELEASE_TAG "https://github.com/foressssst/ioquakelive/releases/tag/"
#define CL_UPDATE_DOWNLOAD "https://github.com/foressssst/ioquakelive/releases/download/"
#define CL_UPDATE_TIMEOUT_MS 5000
#define CL_UPDATE_DOWNLOAD_TIMEOUT_MS 30000
#define CL_VERIFIED_URL "https://gist.githubusercontent.com/foressssst/65861cb796e4d984cfd73b384e8c2098/raw/ioquakelive-verified.txt"
#define CL_VERIFIED_MAX 64
#define CL_VERIFIED_TEXT 8192
#define CL_UPDATE_ZIP "ioquakelive-update.zip"
#define CL_UPDATE_STAGE "ioquakelive-update"
#define CL_UPDATE_MAX_FILES 64

#if defined(_WIN32) && defined(_M_ARM64)
#define CL_UPDATE_ASSET "ioquakelive-windows-arm64.zip"
#elif defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
#define CL_UPDATE_ASSET "ioquakelive-windows-x64.zip"
#elif defined(_WIN32)
#define CL_UPDATE_ASSET "ioquakelive-windows-x86.zip"
#elif defined(__linux__) && defined(__aarch64__)
#define CL_UPDATE_ASSET "ioquakelive-linux-arm64.zip"
#elif defined(__linux__) && defined(__x86_64__)
#define CL_UPDATE_ASSET "ioquakelive-linux-x86_64.zip"
#endif

enum { CLU_IDLE, CLU_RUNNING, CLU_DONE, CLU_FAILED, CLU_HANDLED };
enum { CLD_IDLE, CLD_RUNNING, CLD_DOWNLOADED, CLD_EXTRACT, CLD_READY, CLD_FAILED };

#define CL_UPDATE_SITE "https://github.com/"

static char clUpdateText[0x40000];
static char clUpdateURL[512];
static volatile long clUpdateState;
static cvar_t* cl_checkUpdates;

static char clUpdateAsset[512];
static char clUpdateDigest[72];
static long clUpdateAssetSize;
static volatile long clDownloadState;
static volatile long clDownloadBytes;
static char clUpdateDir[MAX_OSPATH];
static char clUpdateZipPath[MAX_OSPATH];
static char clUpdateExe[MAX_OSPATH];
static char clUpdateFiles[CL_UPDATE_MAX_FILES][MAX_QPATH];
static int clUpdateNumFiles;
static int clUpdateShownPercent = -1;
static qboolean clUpdateRestart;
static qboolean clUpdateAutoRestart;
static volatile long clVerifiedState;
static char clVerifiedText[CL_VERIFIED_TEXT];
static unsigned char clVerifiedSigned[CL_VERIFIED_TEXT + crypto_sign_BYTES];
static unsigned char clVerifiedOpened[CL_VERIFIED_TEXT + crypto_sign_BYTES];
static byte clVerifiedIP[CL_VERIFIED_MAX][4];
static unsigned short clVerifiedPort[CL_VERIFIED_MAX];
static int clVerifiedCount;
static const unsigned char clVerifiedKey[crypto_sign_PUBLICKEYBYTES] = {
    0x54, 0x31, 0x1a, 0xf6, 0xdd, 0x5a, 0x2e, 0xe0, 0xc5, 0x43, 0xd5, 0xd5, 0xa8, 0x8b, 0x11, 0x25,
    0xf1, 0xb7, 0x96, 0xdc, 0xbf, 0xd9, 0xe8, 0xaf, 0x59, 0xe8, 0x44, 0xd2, 0x92, 0xba, 0x05, 0x89,
};

static void CL_UpdateSet(volatile long* v, long state) {
#ifdef _MSC_VER
    InterlockedExchange(v, state);
#else
    __atomic_store_n(v, state, __ATOMIC_RELEASE);
#endif
}

static long CL_UpdateGet(volatile long* v) {
#ifdef _MSC_VER
    return InterlockedCompareExchange(v, 0, 0);
#else
    return __atomic_load_n(v, __ATOMIC_ACQUIRE);
#endif
}

typedef struct {
    unsigned int h[8];
    unsigned long long bits;
    byte block[64];
    int used;
} clSha256_t;

static const unsigned int clSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define CL_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void CL_Sha256Init(clSha256_t* s) {
    static const unsigned int init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    Com_Memcpy(s->h, init, sizeof(init));
    s->bits = 0;
    s->used = 0;
}

static void CL_Sha256Block(clSha256_t* s, const byte* p) {
    unsigned int w[64], v[8], t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned int)p[i * 4] << 24) | ((unsigned int)p[i * 4 + 1] << 16) | ((unsigned int)p[i * 4 + 2] << 8) |
               (unsigned int)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        w[i] = w[i - 16] + (CL_ROR(w[i - 15], 7) ^ CL_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
               (CL_ROR(w[i - 2], 17) ^ CL_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10));
    }
    Com_Memcpy(v, s->h, sizeof(v));
    for (i = 0; i < 64; i++) {
        t1 = v[7] + (CL_ROR(v[4], 6) ^ CL_ROR(v[4], 11) ^ CL_ROR(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) +
             clSha256K[i] + w[i];
        t2 = (CL_ROR(v[0], 2) ^ CL_ROR(v[0], 13) ^ CL_ROR(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) {
        s->h[i] += v[i];
    }
}

static void CL_Sha256Update(clSha256_t* s, const void* data, int len) {
    const byte* p = (const byte*)data;

    s->bits += (unsigned long long)len * 8;
    while (len > 0) {
        int n = 64 - s->used < len ? 64 - s->used : len;

        Com_Memcpy(s->block + s->used, p, n);
        s->used += n;
        p += n;
        len -= n;
        if (s->used == 64) {
            CL_Sha256Block(s, s->block);
            s->used = 0;
        }
    }
}

static void CL_Sha256Hex(clSha256_t* s, char* out) {
    unsigned long long bits = s->bits;
    byte pad = 0x80;
    byte len[8];
    int i;

    CL_Sha256Update(s, &pad, 1);
    pad = 0;
    while (s->used != 56) {
        CL_Sha256Update(s, &pad, 1);
    }
    for (i = 0; i < 8; i++) {
        len[i] = (byte)(bits >> (56 - i * 8));
    }
    CL_Sha256Update(s, len, 8);
    for (i = 0; i < 8; i++) {
        Com_sprintf(out + i * 8, 9, "%08x", s->h[i]);
    }
}

typedef struct {
    char* buf;
    int size;
    int used;
    FILE* file;
    clSha256_t* sha;
    qboolean failed;
} clUpdateSink_t;

static void CL_UpdateSinkWrite(clUpdateSink_t* sink, const char* data, int n) {
    int room;

    if (sink->file) {
        if (fwrite(data, 1, (size_t)n, sink->file) != (size_t)n) {
            sink->failed = qtrue;
        }
        CL_Sha256Update(sink->sha, data, n);
        CL_UpdateSet(&clDownloadBytes, CL_UpdateGet(&clDownloadBytes) + n);
        return;
    }
    room = sink->size - 1 - sink->used;
    if (n > room) {
        n = room;
    }
    Com_Memcpy(sink->buf + sink->used, data, n);
    sink->used += n;
}

#ifdef _WIN32
static qboolean CL_UpdateHttpGet(const char* address, clUpdateSink_t* sink, int timeout) {
    URL_COMPONENTS uc;
    wchar_t url[512], host[256], path[512];
    HINTERNET session = NULL, connect = NULL, request = NULL;
    DWORD status = 0, statusLen = sizeof(status), got = 0;
    char chunk[16384];
    qboolean ok = qfalse;

    if (!MultiByteToWideChar(CP_UTF8, 0, address, -1, url, ARRAY_LEN(url))) {
        return qfalse;
    }
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = ARRAY_LEN(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = ARRAY_LEN(path);
    if (!WinHttpCrackUrl(url, 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) {
        return qfalse;
    }
    session = WinHttpOpen(L"ioquakelive", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
        connect = WinHttpConnect(session, host, uc.nPort, 0);
    }
    if (connect) {
        request = WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     WINHTTP_FLAG_SECURE);
    }
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, NULL) &&
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &statusLen, WINHTTP_NO_HEADER_INDEX) &&
        status == 200) {
        ok = qtrue;
        while (WinHttpReadData(request, chunk, sizeof(chunk), &got) && got > 0) {
            CL_UpdateSinkWrite(sink, chunk, (int)got);
        }
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
#define CLU_CURLOPT_LOW_SPEED_LIMIT 19
#define CLU_CURLOPT_LOW_SPEED_TIME 20
#define CLU_CURLINFO_RESPONSE_CODE 0x200002

static size_t CL_UpdateWrite(char* data, size_t size, size_t count, void* user) {
    CL_UpdateSinkWrite((clUpdateSink_t*)user, data, (int)(size * count));
    return size * count;
}

static qboolean CL_UpdateHttpGet(const char* address, clUpdateSink_t* sink, int timeout) {
    static const char* const names[] = {"libcurl.so.4", "libcurl.so", "libcurl.4.dylib", "/usr/lib/libcurl.4.dylib",
                                        "libcurl.dylib"};
    void* (*easyInit)(void);
    int (*easySetopt)(void*, int, ...);
    int (*easyPerform)(void*);
    int (*easyGetinfo)(void*, int, ...);
    void (*easyCleanup)(void*);
    void* lib = NULL;
    void* curl;
    long code = 0;
    qboolean ok = qfalse;
    int i;

    if (Q_strncmp(address, "https://", 8)) {
        return qfalse;
    }
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
        easySetopt(curl, CLU_CURLOPT_URL, address);
        easySetopt(curl, CLU_CURLOPT_WRITEFUNCTION, CL_UpdateWrite);
        easySetopt(curl, CLU_CURLOPT_WRITEDATA, sink);
        if (sink->file) {
            easySetopt(curl, CLU_CURLOPT_LOW_SPEED_LIMIT, 1L);
            easySetopt(curl, CLU_CURLOPT_LOW_SPEED_TIME, (long)(timeout / 1000));
        } else {
            easySetopt(curl, CLU_CURLOPT_TIMEOUT, (long)(timeout / 1000));
        }
        easySetopt(curl, CLU_CURLOPT_CONNECTTIMEOUT, (long)(CL_UPDATE_TIMEOUT_MS / 1000));
        easySetopt(curl, CLU_CURLOPT_FOLLOWLOCATION, 1L);
        easySetopt(curl, CLU_CURLOPT_NOSIGNAL, 1L);
        easySetopt(curl, CLU_CURLOPT_USERAGENT, "ioquakelive");
        if (easyPerform(curl) == 0 && easyGetinfo(curl, CLU_CURLINFO_RESPONSE_CODE, &code) == 0 && code == 200) {
            ok = qtrue;
        }
        easyCleanup(curl);
    }
    dlclose(lib);
    return ok;
}
#endif

static qboolean CL_UpdateFetch(const char* address, char* out, int size) {
    clUpdateSink_t sink;

    Com_Memset(&sink, 0, sizeof(sink));
    sink.buf = out;
    sink.size = size;
    out[0] = '\0';
    if (!CL_UpdateHttpGet(address, &sink, CL_UPDATE_TIMEOUT_MS) || sink.used <= 0) {
        return qfalse;
    }
    out[sink.used] = '\0';
    return qtrue;
}

static const char* CL_UpdateJsonValue(const char* text, const char* key) {
    const char* p = strstr(text, key);

    if (!p) {
        return NULL;
    }
    p += strlen(key);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':') {
        p++;
    }
    return p;
}

static void CL_UpdateFindAsset(const char* text) {
#ifdef CL_UPDATE_ASSET
    const char* end = text + strlen(text);
    const char* release = JSON_ArrayGetFirstValue(text, end);
    const char* assets = release ? JSON_ObjectGetNamedValue(release, end, "assets") : NULL;
    const char* asset;
    const char* v;
    char name[64];

    for (asset = assets ? JSON_ArrayGetFirstValue(assets, end) : NULL; asset;
         asset = JSON_ArrayGetNextValue(asset, end)) {
        v = JSON_ObjectGetNamedValue(asset, end, "name");
        if (!v || !JSON_ValueGetString(v, end, name, sizeof(name)) || Q_stricmp(name, CL_UPDATE_ASSET)) {
            continue;
        }
        v = JSON_ObjectGetNamedValue(asset, end, "browser_download_url");
        if (v) {
            JSON_ValueGetString(v, end, clUpdateAsset, sizeof(clUpdateAsset));
        }
        v = JSON_ObjectGetNamedValue(asset, end, "digest");
        if (v) {
            JSON_ValueGetString(v, end, clUpdateDigest, sizeof(clUpdateDigest));
        }
        v = JSON_ObjectGetNamedValue(asset, end, "size");
        clUpdateAssetSize = v ? (long)JSON_ValueGetDouble(v, end) : 0;
        if (Q_strncmp(clUpdateAsset, CL_UPDATE_DOWNLOAD, (int)sizeof(CL_UPDATE_DOWNLOAD) - 1) ||
            Q_strncmp(clUpdateDigest, "sha256:", 7) || strlen(clUpdateDigest) != 71 || clUpdateAssetSize <= 0) {
            clUpdateAsset[0] = '\0';
        }
        return;
    }
#else
    (void)text;
#endif
}

static qboolean CL_UpdateFromGitHub(void) {
    char tag[32];
    const char* p;
    qboolean pre;
    int n;

    if (!CL_UpdateFetch(CL_UPDATE_API, clUpdateText, sizeof(clUpdateText))) {
        return qfalse;
    }
    p = CL_UpdateJsonValue(clUpdateText, "\"tag_name\"");
    if (!p || *p++ != '"') {
        return qfalse;
    }
    for (n = 0; n < (int)sizeof(tag) - 1 && p[n] && p[n] != '"'; n++) {
        if ((p[n] < '0' || p[n] > '9') && p[n] != '.' && p[n] != 'v') {
            return qfalse;
        }
        tag[n] = p[n];
    }
    if (p[n] != '"' || !n) {
        return qfalse;
    }
    tag[n] = '\0';
    p = CL_UpdateJsonValue(clUpdateText, "\"prerelease\"");
    if (!p) {
        return qfalse;
    }
    pre = !Q_strncmp(p, "true", 4) ? qtrue : qfalse;
    CL_UpdateFindAsset(clUpdateText);
    Com_sprintf(clUpdateText, sizeof(clUpdateText), "%s %s%s%s", tag[0] == 'v' ? tag + 1 : tag,
                CL_UPDATE_RELEASE_TAG, tag, pre ? " pre" : "");
    return qtrue;
}

void randombytes(unsigned char* out, unsigned long long len) {
    while (len > 0) {
        int n = len > 1024 ? 1024 : (int)len;

        Com_RandomBytes(out, n);
        out += n;
        len -= (unsigned long long)n;
    }
}

static int CL_VerifiedHex(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static qboolean CL_VerifiedLoad(void) {
    const char* body;
    const char* line;
    unsigned long long opened;
    unsigned int a, b, c, d, port;
    int i, hi, lo, len;

    if (!CL_UpdateFetch(CL_VERIFIED_URL, clVerifiedText, sizeof(clVerifiedText))) {
        return qfalse;
    }
    body = strchr(clVerifiedText, '\n');
    if (!body || body - clVerifiedText != crypto_sign_BYTES * 2) {
        return qfalse;
    }
    for (i = 0; i < crypto_sign_BYTES; i++) {
        hi = CL_VerifiedHex(clVerifiedText[i * 2]);
        lo = CL_VerifiedHex(clVerifiedText[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return qfalse;
        }
        clVerifiedSigned[i] = (unsigned char)(hi << 4 | lo);
    }
    body++;
    len = (int)strlen(body);
    Com_Memcpy(clVerifiedSigned + crypto_sign_BYTES, body, len);
    if (crypto_sign_open(clVerifiedOpened, &opened, clVerifiedSigned, (unsigned long long)(crypto_sign_BYTES + len),
                         clVerifiedKey) != 0) {
        return qfalse;
    }
    for (line = body; line && *line && clVerifiedCount < CL_VERIFIED_MAX; line = strchr(line, '\n')) {
        if (*line == '\n') {
            line++;
        }
        if (sscanf(line, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &port) == 5 && a < 256 && b < 256 && c < 256 && d < 256 &&
            port > 0 && port < 65536) {
            clVerifiedIP[clVerifiedCount][0] = (byte)a;
            clVerifiedIP[clVerifiedCount][1] = (byte)b;
            clVerifiedIP[clVerifiedCount][2] = (byte)c;
            clVerifiedIP[clVerifiedCount][3] = (byte)d;
            clVerifiedPort[clVerifiedCount] = (unsigned short)port;
            clVerifiedCount++;
        }
    }
    return qtrue;
}

qboolean CL_ServerVerified(const netadr_t* adr) {
    int i;

    if (adr->type != NA_IP || CL_UpdateGet(&clVerifiedState) != CLU_DONE) {
        return qfalse;
    }
    for (i = 0; i < clVerifiedCount; i++) {
        if (!memcmp(adr->ip, clVerifiedIP[i], 4) && (unsigned short)BigShort(adr->port) == clVerifiedPort[i]) {
            return qtrue;
        }
    }
    return qfalse;
}

static void CL_UpdateThread(void* unused) {
    (void)unused;
    if (CL_UpdateGet(&clUpdateState) == CLU_RUNNING) {
        if (CL_UpdateFromGitHub()) {
            CL_UpdateSet(&clUpdateState, CLU_DONE);
        } else {
            CL_UpdateSet(&clUpdateState,
                         CL_UpdateFetch(CL_UPDATE_URL, clUpdateText, sizeof(clUpdateText)) ? CLU_DONE : CLU_FAILED);
        }
    }
    CL_UpdateSet(&clVerifiedState, CL_VerifiedLoad() ? CLU_DONE : CLU_FAILED);
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

static const char* CL_UpdatePath(const char* name) {
    return va("%s%c%s", clUpdateDir, PATH_SEP, name);
}

static void CL_UpdateDownloadThread(void* unused) {
    clUpdateSink_t sink;
    clSha256_t sha;
    char hex[65];
    qboolean ok;

    (void)unused;
    Com_Memset(&sink, 0, sizeof(sink));
    CL_Sha256Init(&sha);
    sink.sha = &sha;
    sink.file = Sys_FOpen(clUpdateZipPath, "wb");
    if (!sink.file) {
        CL_UpdateSet(&clDownloadState, CLD_FAILED);
        return;
    }
    ok = CL_UpdateHttpGet(clUpdateAsset, &sink, CL_UPDATE_DOWNLOAD_TIMEOUT_MS);
    fclose(sink.file);
    CL_Sha256Hex(&sha, hex);
    if (!ok || sink.failed || CL_UpdateGet(&clDownloadBytes) != clUpdateAssetSize || Q_stricmp(hex, clUpdateDigest + 7)) {
        remove(clUpdateZipPath);
        CL_UpdateSet(&clDownloadState, CLD_FAILED);
        return;
    }
    CL_UpdateSet(&clDownloadState, CLD_DOWNLOADED);
}

static qboolean CL_UpdateSafeName(const char* name) {
    const char* p;

    if (!name[0] || name[0] == '/' || name[0] == '\\' || strchr(name, ':') || strstr(name, "..") ||
        strlen(name) >= MAX_QPATH - 8) {
        return qfalse;
    }
    for (p = name; *p; p++) {
        if ((unsigned char)*p < ' ' || *p == '\\') {
            return qfalse;
        }
    }
    return qtrue;
}

static void CL_UpdateMakeDirs(const char* path) {
    char dir[MAX_OSPATH];
    char* p;

    Q_strncpyz(dir, path, sizeof(dir));
    for (p = dir + strlen(clUpdateDir) + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            Sys_Mkdir(dir);
            *p = PATH_SEP;
        }
    }
}

static qboolean CL_UpdateExtract(void) {
    static byte buf[65536];
    unz_file_info info;
    char name[MAX_QPATH];
    unzFile zip;
    FILE* out;
    int ret, n;
    qboolean ok = qtrue;

    clUpdateNumFiles = 0;
    zip = unzOpen(clUpdateZipPath);
    if (!zip) {
        return qfalse;
    }
    Sys_Mkdir(CL_UpdatePath(CL_UPDATE_STAGE));
    for (ret = unzGoToFirstFile(zip); ok && ret == UNZ_OK; ret = unzGoToNextFile(zip)) {
        if (unzGetCurrentFileInfo(zip, &info, name, sizeof(name), NULL, 0, NULL, 0) != UNZ_OK) {
            ok = qfalse;
            break;
        }
        if (name[0] && name[strlen(name) - 1] == '/') {
            continue;
        }
        if (!CL_UpdateSafeName(name) || clUpdateNumFiles >= CL_UPDATE_MAX_FILES) {
            ok = qfalse;
            break;
        }
        CL_UpdateMakeDirs(CL_UpdatePath(va("%s/%s", CL_UPDATE_STAGE, name)));
        out = Sys_FOpen(CL_UpdatePath(va("%s%c%s", CL_UPDATE_STAGE, PATH_SEP, name)), "wb");
        if (!out || unzOpenCurrentFile(zip) != UNZ_OK) {
            if (out) {
                fclose(out);
            }
            ok = qfalse;
            break;
        }
        while ((n = unzReadCurrentFile(zip, buf, sizeof(buf))) > 0) {
            if (fwrite(buf, 1, (size_t)n, out) != (size_t)n) {
                ok = qfalse;
                break;
            }
        }
        if (n < 0 || unzCloseCurrentFile(zip) != UNZ_OK) {
            ok = qfalse;
        }
        fclose(out);
        Q_strncpyz(clUpdateFiles[clUpdateNumFiles++], name, MAX_QPATH);
    }
    unzClose(zip);
    remove(clUpdateZipPath);
    return (qboolean)(ok && clUpdateNumFiles > 0);
}

static qboolean CL_UpdateMove(const char* from, const char* to) {
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) ? qtrue : qfalse;
#else
    return rename(from, to) == 0 ? qtrue : qfalse;
#endif
}

static void CL_UpdateCleanup(void) {
    static const char* const dirs[] = {"", "baseq3"};
    char dir[MAX_OSPATH];
    char** list;
    int i, j, count;

    for (i = 0; i < (int)ARRAY_LEN(dirs); i++) {
        if (dirs[i][0]) {
            Com_sprintf(dir, sizeof(dir), "%s%c%s", clUpdateDir, PATH_SEP, dirs[i]);
        } else {
            Q_strncpyz(dir, clUpdateDir, sizeof(dir));
        }
        list = Sys_ListFiles(dir, ".old", NULL, &count, qfalse);
        for (j = 0; list && j < count; j++) {
            remove(va("%s%c%s", dir, PATH_SEP, list[j]));
        }
        Sys_FreeFileList(list);
    }
    remove(clUpdateZipPath);
}

static void CL_UpdateExeDir(void) {
    char* slash;
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, clUpdateDir, sizeof(clUpdateDir));

    if (!n || n >= sizeof(clUpdateDir)) {
        clUpdateDir[0] = '\0';
    }
#else
    ssize_t n = readlink("/proc/self/exe", clUpdateDir, sizeof(clUpdateDir) - 1);

    clUpdateDir[n > 0 ? n : 0] = '\0';
#endif
    Q_strncpyz(clUpdateExe, clUpdateDir, sizeof(clUpdateExe));
    slash = strrchr(clUpdateDir, PATH_SEP);
    if (slash) {
        *slash = '\0';
    } else {
        clUpdateDir[0] = '\0';
    }
}

static void CL_OpenUpdate_f(void) {
    if (clUpdateURL[0]) {
        CL_OpenURL(clUpdateURL);
    }
}

static void CL_UpdateDownload_f(void) {
    long state = CL_UpdateGet(&clDownloadState);

    if (!clUpdateAsset[0] || !clUpdateDir[0] || state == CLD_FAILED) {
        CL_OpenUpdate_f();
        return;
    }
    if (state != CLD_IDLE) {
        return;
    }
    clUpdateAutoRestart = qtrue;
    CL_UpdateSet(&clDownloadBytes, 0);
    CL_UpdateSet(&clDownloadState, CLD_RUNNING);
    Cvar_Set("cl_updateStatus", "downloading");
    Cvar_Set("cl_updateProgress", "0");
    if (!Sys_ThreadCreate(CL_UpdateDownloadThread, NULL)) {
        CL_UpdateSet(&clDownloadState, CLD_FAILED);
    }
}

static void CL_UpdateRestart_f(void) {
    if (CL_UpdateGet(&clDownloadState) != CLD_READY) {
        return;
    }
    clUpdateRestart = qtrue;
    Cbuf_ExecuteText(EXEC_APPEND, "quit\n");
}

void CL_UpdateApplyOnExit(void) {
    char target[MAX_OSPATH], staged[MAX_OSPATH], old[MAX_OSPATH];
    int i;

    if (CL_UpdateGet(&clDownloadState) != CLD_READY) {
        return;
    }
    for (i = 0; i < clUpdateNumFiles; i++) {
        Q_strncpyz(target, CL_UpdatePath(clUpdateFiles[i]), sizeof(target));
        Q_strncpyz(staged, CL_UpdatePath(va("%s%c%s", CL_UPDATE_STAGE, PATH_SEP, clUpdateFiles[i])), sizeof(staged));
        Com_sprintf(old, sizeof(old), "%s.old", target);
        remove(old);
        CL_UpdateMakeDirs(target);
        CL_UpdateMove(target, old);
        if (!CL_UpdateMove(staged, target)) {
            CL_UpdateMove(old, target);
        }
    }
#ifdef _WIN32
    RemoveDirectoryA(CL_UpdatePath(va("%s%cbaseq3", CL_UPDATE_STAGE, PATH_SEP)));
    RemoveDirectoryA(CL_UpdatePath(CL_UPDATE_STAGE));
    if (clUpdateRestart) {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        static char line[8192];

        Q_strncpyz(line, GetCommandLineA(), sizeof(line));
        Com_Memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        if (CreateProcessA(clUpdateExe, line, NULL, NULL, FALSE, 0, NULL, clUpdateDir, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
#else
    rmdir(CL_UpdatePath(va("%s%cbaseq3", CL_UPDATE_STAGE, PATH_SEP)));
    rmdir(CL_UpdatePath(CL_UPDATE_STAGE));
    if (clUpdateRestart) {
        char* args[2];

        args[0] = clUpdateExe;
        args[1] = NULL;
        if (chdir(clUpdateDir) == 0) {
            execv(clUpdateExe, args);
        }
    }
#endif
}

void CL_UpdateInit(void) {
    cl_checkUpdates = Cvar_Get("cl_checkUpdates", "1", CVAR_ARCHIVE);
    Cvar_Get("cl_updateVersion", "", CVAR_ROM);
    Cvar_Get("cl_updateStatus", "", CVAR_ROM);
    Cvar_Get("cl_updateProgress", "0", CVAR_ROM);
    Cvar_Get("cl_updateAuto", "0", CVAR_ROM);
    Cmd_AddCommand("openupdate", CL_OpenUpdate_f);
    Cmd_AddCommand("updatedownload", CL_UpdateDownload_f);
    Cmd_AddCommand("updaterestart", CL_UpdateRestart_f);
    if (!clUpdateDir[0]) {
        CL_UpdateExeDir();
        if (clUpdateDir[0]) {
            Q_strncpyz(clUpdateZipPath, CL_UpdatePath(CL_UPDATE_ZIP), sizeof(clUpdateZipPath));
            CL_UpdateCleanup();
        }
    }
    if (CL_UpdateGet(&clVerifiedState) != CLU_IDLE) {
        return;
    }
    CL_UpdateSet(&clVerifiedState, CLU_RUNNING);
    if (cl_checkUpdates->integer && CL_UpdateGet(&clUpdateState) == CLU_IDLE) {
        CL_UpdateSet(&clUpdateState, CLU_RUNNING);
    }
    if (!Sys_ThreadCreate(CL_UpdateThread, NULL)) {
        if (CL_UpdateGet(&clUpdateState) == CLU_RUNNING) {
            CL_UpdateSet(&clUpdateState, CLU_FAILED);
        }
        CL_UpdateSet(&clVerifiedState, CLU_FAILED);
    }
}

static void CL_UpdateDownloadFrame(void) {
    long state = CL_UpdateGet(&clDownloadState);
    int percent;

    if (state == CLD_RUNNING) {
        percent = clUpdateAssetSize > 0 ? (int)(CL_UpdateGet(&clDownloadBytes) * 100.0 / clUpdateAssetSize) : 0;
        if (percent != clUpdateShownPercent) {
            clUpdateShownPercent = percent;
            Cvar_Set("cl_updateProgress", va("%i", percent > 100 ? 100 : percent));
        }
    } else if (state == CLD_DOWNLOADED) {
        Cvar_Set("cl_updateProgress", "100");
        Cvar_Set("cl_updateStatus", "installing");
        CL_UpdateSet(&clDownloadState, CLD_EXTRACT);
    } else if (state == CLD_EXTRACT) {
        if (CL_UpdateExtract()) {
            CL_UpdateSet(&clDownloadState, CLD_READY);
            Cvar_Set("cl_updateStatus", "ready");
            Com_Printf("The ioquakelive update is ready; it is installed when the game restarts.\n");
        } else {
            CL_UpdateSet(&clDownloadState, CLD_FAILED);
            Cvar_Set("cl_updateStatus", "failed");
        }
    } else if (state == CLD_READY) {
        if (clUpdateAutoRestart && !clUpdateRestart && clc.state == CA_DISCONNECTED) {
            CL_UpdateRestart_f();
        }
    } else if (state == CLD_FAILED && Q_stricmp(Cvar_VariableString("cl_updateStatus"), "failed")) {
        Cvar_Set("cl_updateStatus", "failed");
        Com_Printf("The ioquakelive update could not be downloaded.\n");
    }
}

void CL_UpdateFrame(void) {
    char version[32];
    char url[512];
    char kind[8];
    const char* p;
    long state;

    CL_UpdateDownloadFrame();
    state = CL_UpdateGet(&clUpdateState);
    if (state != CLU_DONE && state != CLU_FAILED) {
        return;
    }
    CL_UpdateSet(&clUpdateState, CLU_HANDLED);
    if (state == CLU_FAILED) {
        Com_DPrintf("update check: no answer\n");
        return;
    }
    kind[0] = '\0';
    if (sscanf(clUpdateText, "%31s %511s %7s", version, url, kind) < 2) {
        return;
    }
    for (p = version; *p; p++) {
        if ((*p < '0' || *p > '9') && *p != '.') {
            return;
        }
    }
    if (Q_strncmp(url, CL_UPDATE_SITE, (int)sizeof(CL_UPDATE_SITE) - 1)) {
        return;
    }
    if (CL_UpdateCompare(version, PRODUCT_VERSION) <= 0) {
        Cvar_Set("cl_updateVersion", PRODUCT_VERSION);
        Cvar_Set("cl_updateStatus", "latest");
        Com_Printf("ioquakelive is up to date (%s)\n", PRODUCT_VERSION);
        return;
    }
    Q_strncpyz(clUpdateURL, url, sizeof(clUpdateURL));
    Cvar_Set("cl_updateVersion", version);
    Cvar_Set("cl_updateStatus", !Q_stricmp(kind, "pre") ? "prerelease" : "available");
    Cvar_Set("cl_updateAuto", clUpdateAsset[0] && clUpdateDir[0] ? "1" : "0");
    Com_Printf("A new version of ioquakelive is available: %s\n%s\n", version, url);
}
