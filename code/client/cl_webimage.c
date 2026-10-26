/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

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
#include "client.h"
#include "cl_gif.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if !defined(_WIN32) && defined(USE_CURL)
#include "cl_curl.h"
#endif

#define WEB_IMAGE_DIR "webimages"
#define MAX_WEB_IMAGES 24
#define MAX_WEB_FETCHING 2
#define MAX_GIF_FRAMES 48
#define GIF_MAX_SIDE 200
#define REGISTER_PER_FRAME 4
#define WEB_IMAGE_BUDGET 256
#define WEB_IMAGE_MAX_SIDE 4096
#define WEB_FETCH_WINDOW_MS 60000
#define WEB_FETCH_PER_WINDOW 6

typedef enum {
    WI_FREE,
    WI_FETCHING,
    WI_READY,
    WI_FAILED
} webImageState_t;

typedef struct {
    char url[MAX_OSPATH];
    char local[MAX_QPATH];
    char ospath[MAX_OSPATH];
    char tmppath[MAX_OSPATH];
    char base[MAX_QPATH];
    qboolean gif;
    webImageState_t state;
    int lastUsed;
    int maxBytes;
    int maxSide;
    volatile long result;

    int numFrames;
    int delays[MAX_GIF_FRAMES];
    qhandle_t frames[MAX_GIF_FRAMES];
    int registered;
    int duration;
} webImage_t;

static webImage_t webImages[MAX_WEB_IMAGES];
static int numWebImages;
static int webImagesFetching;
static int webImagesRegistered;
static unsigned webImageSerial;
static int webFetchTimes[WEB_FETCH_PER_WINDOW];
static int webFetchNext;

cvar_t* cl_webImages;
cvar_t* cl_webImageMaxKB;

static void CL_WebImage_SetResult(webImage_t* img, long value) {
#ifdef _MSC_VER
    InterlockedExchange(&img->result, value);
#else
    __atomic_store_n(&img->result, value, __ATOMIC_RELEASE);
#endif
}

static long CL_WebImage_GetResult(webImage_t* img) {
#ifdef _MSC_VER
    return InterlockedCompareExchange(&img->result, 0, 0);
#else
    return __atomic_load_n(&img->result, __ATOMIC_ACQUIRE);
#endif
}

static const char* CL_WebImage_Extension(const char* url) {
    static const char* known[] = {".png", ".jpg", ".jpeg", ".gif"};
    const char* end;
    const char* p;
    size_t i, len;

    end = url + strlen(url);
    for (p = url; *p; p++) {
        if (*p == '?' || *p == '#') {
            end = p;
            break;
        }
    }

    for (i = 0; i < ARRAY_LEN(known); i++) {
        len = strlen(known[i]);
        if ((size_t)(end - url) > len && !Q_stricmpn(end - len, known[i], (int)len)) {
            return known[i];
        }
    }

    return NULL;
}

static qboolean CL_WebImage_UrlOk(const char* url) {
    int len;
    int i;

    if (!url || !url[0]) {
        return qfalse;
    }

    len = (int)strlen(url);
    if (len < 12 || len >= (int)sizeof(webImages[0].url)) {
        return qfalse;
    }

    if (Q_stricmpn(url, "http://", 7) && Q_stricmpn(url, "https://", 8)) {
        return qfalse;
    }

    for (i = 0; i < len; i++) {
        if (url[i] <= ' ' || url[i] == '"' || url[i] == '\\' || (unsigned char)url[i] >= 127) {
            return qfalse;
        }
    }

    return CL_WebImage_Extension(url) != NULL;
}

static qboolean CL_WebImage_HostAllowed(const char* url) {
    static const char* const hosts[] = {"imgur.com", "giphy.com", "tenor.com", "discordapp.com", "discordapp.net"};
    char host[128];
    const char* p;
    int len, i, n;

    p = strstr(url, "://");
    if (!p) {
        return qfalse;
    }
    p += 3;
    for (len = 0; p[len] != '/'; len++) {
        if (len >= (int)sizeof(host) - 1 || !(isalnum((unsigned char)p[len]) || p[len] == '.' || p[len] == '-')) {
            return qfalse;
        }
        host[len] = (char)tolower((unsigned char)p[len]);
    }
    host[len] = '\0';

    for (i = 0; i < (int)ARRAY_LEN(hosts); i++) {
        n = (int)strlen(hosts[i]);
        if (len == n && !strcmp(host, hosts[i])) {
            return qtrue;
        }
        if (len > n && host[len - n - 1] == '.' && !strcmp(host + len - n, hosts[i])) {
            return qtrue;
        }
    }
    return qfalse;
}

static qboolean CL_WebImage_CachePath(const char* name, char* path, int size) {
    const char* base = Sys_DefaultCachePath();
    char* p;

    if (!base || !base[0]) {
        return qfalse;
    }
    Com_sprintf(path, size, "%s%c%s", base, PATH_SEP, name);
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = PATH_SEP;
        }
    }
    return qtrue;
}

static void CL_WebImage_LocalName(const char* url, char* local, int localSize, char* base, int baseSize) {
    Com_sprintf(base, baseSize, "%s/%08x", WEB_IMAGE_DIR, ++webImageSerial);
    Com_sprintf(local, localSize, "%s%s", base, CL_WebImage_Extension(url));
}

typedef struct {
    webImage_t* img;
    int step;
    int kept;
    char homePath[MAX_OSPATH];
} gifWriter_t;

static qboolean CL_WebImage_WriteTGA(const char* ospath, const byte* rgba, int width, int height) {
    byte header[18];
    byte* out;
    FILE* f;
    int w = width, h = height, x, y, sx, sy;
    float scale = 1.0f;

    if (width > GIF_MAX_SIDE || height > GIF_MAX_SIDE) {
        scale = (float)GIF_MAX_SIDE / (float)(width > height ? width : height);
        w = (int)(width * scale);
        h = (int)(height * scale);
        if (w < 1) {
            w = 1;
        }
        if (h < 1) {
            h = 1;
        }
    }

    out = (byte*)malloc((size_t)w * h * 4);
    if (!out) {
        return qfalse;
    }

    for (y = 0; y < h; y++) {
        int y0 = (int)(y / scale), y1 = (int)((y + 1) / scale);

        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > height) {
            y1 = height;
        }
        for (x = 0; x < w; x++) {
            int x0 = (int)(x / scale), x1 = (int)((x + 1) / scale);
            unsigned sum[4] = {0, 0, 0, 0};
            int n = 0;
            byte* px = out + ((size_t)y * w + x) * 4;

            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (x1 > width) {
                x1 = width;
            }
            for (sy = y0; sy < y1; sy++) {
                for (sx = x0; sx < x1; sx++) {
                    const byte* in = rgba + ((size_t)sy * width + sx) * 4;

                    sum[0] += in[0];
                    sum[1] += in[1];
                    sum[2] += in[2];
                    sum[3] += in[3];
                    n++;
                }
            }
            if (!n) {
                n = 1;
            }
            px[0] = (byte)(sum[2] / n);
            px[1] = (byte)(sum[1] / n);
            px[2] = (byte)(sum[0] / n);
            px[3] = (byte)(sum[3] / n);
        }
    }

    memset(header, 0, sizeof(header));
    header[2] = 2;
    header[12] = (byte)(w & 255);
    header[13] = (byte)(w >> 8);
    header[14] = (byte)(h & 255);
    header[15] = (byte)(h >> 8);
    header[16] = 32;
    header[17] = 0x08;

    f = Sys_FOpen(ospath, "wb");
    if (!f) {
        free(out);
        return qfalse;
    }
    fwrite(header, 1, sizeof(header), f);
    for (y = h - 1; y >= 0; y--) {
        fwrite(out + (size_t)y * w * 4, 1, (size_t)w * 4, f);
    }
    fclose(f);
    free(out);

    return qtrue;
}

static qboolean CL_WebImage_GifFrame(int frame, const byte* rgba, int width, int height, int delayMs, void* user) {
    gifWriter_t* gw = (gifWriter_t*)user;
    webImage_t* img = gw->img;
    char name[MAX_QPATH];
    char ospath[MAX_OSPATH];

    if (frame % gw->step) {
        if (gw->kept > 0) {
            img->delays[gw->kept - 1] += delayMs;
        }
        return qtrue;
    }

    if (gw->kept >= MAX_GIF_FRAMES) {
        return qfalse;
    }

    Com_sprintf(name, sizeof(name), "%s_f%02d.tga", img->base, gw->kept);
    Com_sprintf(ospath, sizeof(ospath), "%s%c%s", gw->homePath, PATH_SEP, name);
    if (!CL_WebImage_WriteTGA(ospath, rgba, width, height)) {
        return qfalse;
    }

    img->delays[gw->kept] = delayMs;
    gw->kept++;

    return qtrue;
}

static int CL_WebImage_GifCount(const byte* data, int length) {
    int pos, count = 0, size;

    if (length < 13) {
        return 0;
    }
    pos = 13;
    if (data[10] & 0x80) {
        pos += 3 * (2 << (data[10] & 7));
    }

    while (pos < length) {
        int block = data[pos++];

        if (block == 0x3B) {
            break;
        }
        if (block == 0x21) {
            pos++;
        } else if (block == 0x2C) {
            int packed;

            if (pos + 9 > length) {
                break;
            }
            packed = data[pos + 8];
            pos += 9;
            if (packed & 0x80) {
                pos += 3 * (2 << (packed & 7));
            }
            pos++;
            count++;
        } else {
            break;
        }
        while (pos < length && (size = data[pos++]) > 0) {
            pos += size;
        }
    }

    return count;
}

static qboolean CL_WebImage_DecodeGif(webImage_t* img) {
    gifWriter_t gw;
    FILE* f;
    byte* data;
    long length;
    int total, i;
    char* slash;

    f = Sys_FOpen(img->tmppath, "rb");
    if (!f) {
        return qfalse;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length <= 0 || length > img->maxBytes) {
        fclose(f);
        return qfalse;
    }
    data = (byte*)malloc(length);
    if (!data || fread(data, 1, length, f) != (size_t)length) {
        fclose(f);
        free(data);
        return qfalse;
    }
    fclose(f);

    Q_strncpyz(gw.homePath, img->tmppath, sizeof(gw.homePath));
    slash = strrchr(gw.homePath, PATH_SEP);
    if (!slash) {
        free(data);
        return qfalse;
    }
    *slash = '\0';
    slash = strrchr(gw.homePath, PATH_SEP);
    if (!slash) {
        free(data);
        return qfalse;
    }
    *slash = '\0';

    total = CL_WebImage_GifCount(data, (int)length);
    gw.img = img;
    gw.step = total > MAX_GIF_FRAMES ? (total + MAX_GIF_FRAMES - 1) / MAX_GIF_FRAMES : 1;
    gw.kept = 0;

    if (!GIF_Decode(data, (int)length, 0, CL_WebImage_GifFrame, &gw) || !gw.kept) {
        free(data);
        return qfalse;
    }
    free(data);

    img->numFrames = gw.kept;
    img->duration = 0;
    for (i = 0; i < img->numFrames; i++) {
        img->duration += img->delays[i];
    }

    return qtrue;
}

static qboolean CL_WebImage_Sane(const webImage_t* img) {
    const char* ext = CL_WebImage_Extension(img->url);
    FILE* f;
    byte* data;
    long length;
    int w = 0, h = 0, pos, marker, seglen;

    f = Sys_FOpen(img->tmppath, "rb");
    if (!f) {
        return qfalse;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length < 24 || length > img->maxBytes) {
        fclose(f);
        return qfalse;
    }
    data = (byte*)malloc(length);
    if (!data || fread(data, 1, length, f) != (size_t)length) {
        fclose(f);
        free(data);
        return qfalse;
    }
    fclose(f);

    if (!Q_stricmp(ext, ".png")) {
        if (!memcmp(data, "\x89PNG\r\n\x1a\n", 8) && !memcmp(data + 12, "IHDR", 4)) {
            w = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
            h = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
        }
    } else if (!Q_stricmp(ext, ".gif")) {
        if (!memcmp(data, "GIF87a", 6) || !memcmp(data, "GIF89a", 6)) {
            w = data[6] | (data[7] << 8);
            h = data[8] | (data[9] << 8);
        }
    } else if (data[0] == 0xFF && data[1] == 0xD8) {
        pos = 2;
        while (pos + 9 < length && data[pos] == 0xFF) {
            marker = data[pos + 1];
            if (marker == 0xFF) {
                pos++;
                continue;
            }
            if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) {
                pos += 2;
                continue;
            }
            if (marker == 0xD9 || marker == 0xDA) {
                break;
            }
            seglen = (data[pos + 2] << 8) | data[pos + 3];
            if (seglen < 2) {
                break;
            }
            if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                h = (data[pos + 5] << 8) | data[pos + 6];
                w = (data[pos + 7] << 8) | data[pos + 8];
                break;
            }
            pos += 2 + seglen;
        }
    }
    free(data);

    return w > 0 && h > 0 && w <= img->maxSide && h <= img->maxSide ? qtrue : qfalse;
}

#if !defined(_WIN32) && defined(USE_CURL)
typedef struct {
    FILE* f;
    int written;
    int max;
} webSink_t;

static size_t CL_WebImage_CurlWrite(void* buffer, size_t size, size_t nmemb, void* stream) {
    webSink_t* sink = (webSink_t*)stream;
    size_t n = size * nmemb;

    if (sink->max > 0 && (size_t)sink->written + n > (size_t)sink->max) {
        return 0;
    }
    sink->written += (int)n;
    return fwrite(buffer, 1, n, sink->f);
}

static qboolean CL_WebImage_Fetch(const char* url, const char* ospath, int maxBytes) {
    CURL* easy;
    FILE* out;
    CURLcode res;
    webSink_t sink;

    easy = qcurl_easy_init();
    if (!easy) {
        return qfalse;
    }

    out = Sys_FOpen(ospath, "wb");
    if (!out) {
        qcurl_easy_cleanup(easy);
        return qfalse;
    }

    qcurl_easy_setopt_warn(easy, CURLOPT_URL, url);
    qcurl_easy_setopt_warn(easy, CURLOPT_USERAGENT, FULL_PRODUCT_VERSION);
    qcurl_easy_setopt_warn(easy, CURLOPT_WRITEFUNCTION, CL_WebImage_CurlWrite);
    sink.f = out;
    sink.written = 0;
    sink.max = maxBytes;
    qcurl_easy_setopt_warn(easy, CURLOPT_WRITEDATA, &sink);
    qcurl_easy_setopt_warn(easy, CURLOPT_FAILONERROR, 1);
    qcurl_easy_setopt_warn(easy, CURLOPT_FOLLOWLOCATION, 0);
    qcurl_easy_setopt_warn(easy, CURLOPT_NOPROGRESS, 1);
    qcurl_easy_setopt_warn(easy, CURLOPT_NOSIGNAL, 1);
    qcurl_easy_setopt_warn(easy, CURLOPT_CONNECTTIMEOUT, 10);
    qcurl_easy_setopt_warn(easy, CURLOPT_TIMEOUT, 30);
    qcurl_easy_setopt_warn(easy, CURLOPT_MAXFILESIZE, (long)maxBytes);
    qcurl_easy_setopt_warn(easy, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);

    res = qcurl_easy_perform(easy);
    fclose(out);
    qcurl_easy_cleanup(easy);

    if (res != CURLE_OK) {
        remove(ospath);
        return qfalse;
    }

    return qtrue;
}
#elif defined(_WIN32)
static qboolean CL_WebImage_Fetch(const char* url, const char* ospath, int maxBytes) {
    return Sys_WebFetch(url, ospath, maxBytes);
}
#else
static qboolean CL_WebImage_Fetch(const char* url, const char* ospath, int maxBytes) {
    return qfalse;
}
#endif

static void CL_WebImage_Thread(void* argument) {
    webImage_t* img = (webImage_t*)argument;

    if (!CL_WebImage_Fetch(img->url, img->tmppath, img->maxBytes) || !CL_WebImage_Sane(img)) {
        remove(img->tmppath);
        CL_WebImage_SetResult(img, -1);
        return;
    }
    if (img->gif) {
        qboolean ok = CL_WebImage_DecodeGif(img);

        remove(img->tmppath);
        if (!ok) {
            CL_WebImage_SetResult(img, -1);
            return;
        }
    } else {
        remove(img->ospath);
        if (rename(img->tmppath, img->ospath)) {
            remove(img->tmppath);
            CL_WebImage_SetResult(img, -1);
            return;
        }
    }
    CL_WebImage_SetResult(img, 1);
}

static void CL_WebImage_Register(webImage_t* img) {
    char name[MAX_QPATH];
    int n = 0;

    if (!img->gif) {
        if (webImagesRegistered >= WEB_IMAGE_BUDGET) {
            img->state = WI_FAILED;
            return;
        }
        webImagesRegistered++;
        img->frames[0] = re.RegisterShaderNoMip(img->local);
        img->numFrames = 1;
        img->registered = 1;
        img->state = img->frames[0] ? WI_READY : WI_FAILED;
        return;
    }

    img->state = WI_READY;
    while (img->registered < img->numFrames && n < REGISTER_PER_FRAME) {
        if (webImagesRegistered >= WEB_IMAGE_BUDGET) {
            img->numFrames = img->registered;
            break;
        }
        webImagesRegistered++;
        Com_sprintf(name, sizeof(name), "%s_f%02d.tga", img->base, img->registered);
        img->frames[img->registered] = re.RegisterShaderNoMip(name);
        img->registered++;
        n++;
    }
    if (!img->frames[0]) {
        img->state = WI_FAILED;
    }
    if (img->numFrames < 1) {
        img->numFrames = 1;
    }
}

void CL_WebImage_Frame(void) {
    int i;

    for (i = 0; i < numWebImages; i++) {
        webImage_t* img = &webImages[i];

        if (img->state == WI_FETCHING && CL_WebImage_GetResult(img) != 0) {
            webImagesFetching--;
            if (CL_WebImage_GetResult(img) > 0) {
                img->registered = 0;
                CL_WebImage_Register(img);
            } else {
                img->state = WI_FAILED;
            }
        } else if (img->state == WI_READY && img->gif && img->registered < img->numFrames) {
            CL_WebImage_Register(img);
        }
    }
}

qhandle_t CL_WebImage_Get(const char* url) {
    webImage_t* img;
    char tmpLocal[MAX_QPATH];
    int i, oldest, t, frame;

    if (!cls.rendererStarted || !CL_WebImage_UrlOk(url)) {
        return 0;
    }
    if (!cl_webImages || !cl_webImages->integer || !CL_WebImage_HostAllowed(url)) {
        return 0;
    }

    for (i = 0; i < numWebImages; i++) {
        img = &webImages[i];
        if (Q_stricmp(img->url, url)) {
            continue;
        }
        img->lastUsed = cls.realtime;
        if (img->state != WI_READY) {
            return 0;
        }
        if (!img->frames[0]) {
            img->registered = 0;
            CL_WebImage_Register(img);
        }
        if (img->numFrames <= 1 || img->duration <= 0) {
            return img->frames[0];
        }
        t = cls.realtime % img->duration;
        for (frame = 0; frame < img->numFrames - 1 && t >= img->delays[frame]; frame++) {
            t -= img->delays[frame];
        }
        return frame < img->registered && img->frames[frame] ? img->frames[frame] : img->frames[0];
    }

    if (webImagesFetching >= MAX_WEB_FETCHING) {
        return 0;
    }
    if (webFetchTimes[webFetchNext] && cls.realtime - webFetchTimes[webFetchNext] < WEB_FETCH_WINDOW_MS) {
        return 0;
    }
#if !defined(_WIN32) && defined(USE_CURL)
    if (!CL_cURL_Init()) {
        return 0;
    }
    {
        CURL* warm = qcurl_easy_init();

        if (!warm) {
            return 0;
        }
        qcurl_easy_cleanup(warm);
    }
#endif

    if (numWebImages < MAX_WEB_IMAGES) {
        img = &webImages[numWebImages++];
    } else {
        oldest = -1;
        for (i = 0; i < numWebImages; i++) {
            if (webImages[i].state == WI_FETCHING) {
                continue;
            }
            if (oldest < 0 || webImages[i].lastUsed < webImages[oldest].lastUsed) {
                oldest = i;
            }
        }
        if (oldest < 0) {
            return 0;
        }
        img = &webImages[oldest];
    }

    Com_Memset(img, 0, sizeof(*img));
    Q_strncpyz(img->url, url, sizeof(img->url));
    CL_WebImage_LocalName(url, img->local, sizeof(img->local), img->base, sizeof(img->base));
    img->gif = !Q_stricmp(CL_WebImage_Extension(url), ".gif") ? qtrue : qfalse;
    img->lastUsed = cls.realtime;
    img->maxBytes = (int)Com_Clamp(64, 16384, cl_webImageMaxKB->integer) * 1024;
    img->maxSide = cls.glconfig.maxTextureSize > 0 ? MIN(cls.glconfig.maxTextureSize, WEB_IMAGE_MAX_SIDE)
                                                  : WEB_IMAGE_MAX_SIDE;

    Com_sprintf(tmpLocal, sizeof(tmpLocal), "%s.tmp", img->base);
    if (!CL_WebImage_CachePath(img->local, img->ospath, sizeof(img->ospath)) ||
        !CL_WebImage_CachePath(tmpLocal, img->tmppath, sizeof(img->tmppath)) || FS_CreatePath(img->tmppath)) {
        img->state = WI_FAILED;
        return 0;
    }

    img->state = WI_FETCHING;
    CL_WebImage_SetResult(img, 0);
    webFetchTimes[webFetchNext] = cls.realtime ? cls.realtime : 1;
    webFetchNext = (webFetchNext + 1) % WEB_FETCH_PER_WINDOW;

    if (!Sys_ThreadCreate(CL_WebImage_Thread, img)) {
        img->state = WI_FAILED;
        return 0;
    }

    webImagesFetching++;

    return 0;
}

qboolean CL_WebImage_Link(const char* url) {
    if (!CL_WebImage_UrlOk(url) || !cl_webImages || !cl_webImages->integer || !CL_WebImage_HostAllowed(url)) {
        return qfalse;
    }
    CL_WebImage_Get(url);
    return qtrue;
}

qboolean CL_WebImage_Busy(void) {
    return webImagesFetching > 0 ? qtrue : qfalse;
}

static void CL_WebImage_ClearCache(void) {
    char dir[MAX_OSPATH];
    char** list;
    int i, count;

    if (!CL_WebImage_CachePath(WEB_IMAGE_DIR, dir, sizeof(dir))) {
        return;
    }
    list = Sys_ListFiles(dir, NULL, NULL, &count, qfalse);
    for (i = 0; i < count; i++) {
        if (list[i][0] != '.') {
            remove(va("%s%c%s", dir, PATH_SEP, list[i]));
        }
    }
    Sys_FreeFileList(list);
}

void CL_WebImage_Init(void) {
    cl_webImages = Cvar_Get("cl_webImages", "1", CVAR_ARCHIVE);
    cl_webImageMaxKB = Cvar_Get("cl_webImageMaxKB", "8192", CVAR_ARCHIVE);
    CL_WebImage_ClearCache();
}

void CL_WebImage_Shutdown(void) {
    int i;

    for (i = 0; i < numWebImages; i++) {
        Com_Memset(webImages[i].frames, 0, sizeof(webImages[i].frames));
        webImages[i].registered = 0;
    }
    webImagesRegistered = 0;
}
