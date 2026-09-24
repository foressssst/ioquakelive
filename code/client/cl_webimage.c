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
// cl_webimage.c -- [ioquakelive] NOT FROM RETAIL: pictures in the chat and the console.
//
// Seeing one: a link that ends in a picture is fetched once on a thread of its own and kept in the
// player's own folder. A still picture goes to the renderer as it is; a GIF is decoded into its
// frames, each one written as a TGA the renderer can read, and they take turns at their own pace.
//
// Sending one: Ctrl+V in the chat or the console, or "imagepaste", takes what is on the clipboard --
// a picture, an image file copied on the desktop, or the link of one -- up to the image host, and
// the link that comes back goes into the line being typed.
//
// The host's own pictures always show. Links anywhere else only with cl_webImages, because asking
// for a picture tells whoever hosts it that this machine is here, and the link is somebody else's.

#include "client.h"
#include "cl_gif.h"

#if !defined(_WIN32) && defined(USE_CURL)
#include "cl_curl.h"
#endif

#define WEB_IMAGE_DIR "webimages"
#define MAX_WEB_IMAGES 24
#define MAX_WEB_FETCHING 2    // at a time, so a console full of links is not a flood
#define MAX_GIF_FRAMES 80     // more than this and every n-th frame is kept, so it lasts as long
#define GIF_MAX_SIDE 200      // a sticker, not a poster: the frames are brought down to this
#define REGISTER_PER_FRAME 4  // frames handed to the renderer per client frame, so nothing stutters

typedef enum {
    WI_FREE,
    WI_FETCHING,
    WI_READY,
    WI_FAILED
} webImageState_t;

typedef struct {
    char url[MAX_OSPATH];
    char local[MAX_QPATH];    // as the filesystem names it, for the renderer
    char ospath[MAX_OSPATH];  // as the thread writes it
    char base[MAX_QPATH];     // what the frames of a GIF are named after
    qboolean gif;
    webImageState_t state;
    int lastUsed;
    int maxBytes;
    volatile int result;  // 0 while the thread runs, 1 done, -1 failed

    // a still picture has one frame
    int numFrames;
    int delays[MAX_GIF_FRAMES];
    qhandle_t frames[MAX_GIF_FRAMES];
    int registered;
    int duration;
} webImage_t;

static webImage_t webImages[MAX_WEB_IMAGES];
static int numWebImages;
static int webImagesFetching;

cvar_t* cl_webImages;
cvar_t* cl_webImageMaxKB;
cvar_t* cl_imageHost;
cvar_t* cl_imageKey;
cvar_t* cl_imageMaxKB;

/*
==================
CL_WebImage_Extension

The picture formats this reads: the renderer's own, and GIF, which is decoded here. A link that ends
in none of them is not a picture as far as this is concerned
==================
*/
static const char* CL_WebImage_Extension(const char* url) {
    static const char* known[] = {".png", ".jpg", ".jpeg", ".gif", ".tga", ".bmp", ".pcx"};
    const char* end;
    const char* p;
    size_t i, len;

    // the path ends where the query or the fragment starts
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

/*
==================
CL_WebImage_UrlOk

Only http and https, only what fits in the buffer, and nothing that could steer the fetch
somewhere else or end up outside the cache folder
==================
*/
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

/*
==================
CL_WebImage_FromHost

Whether a link is one of the image host's own pictures: the upload address with "upload" turned
into "i/". Those show whatever cl_webImages says -- they are what the game itself sends
==================
*/
static qboolean CL_WebImage_FromHost(const char* url) {
    char prefix[MAX_OSPATH];
    char* slash;

    if (!cl_imageHost || !cl_imageHost->string[0]) {
        return qfalse;
    }

    Q_strncpyz(prefix, cl_imageHost->string, sizeof(prefix));
    slash = strrchr(prefix, '/');
    if (!slash) {
        return qfalse;
    }
    Q_strncpyz(slash + 1, "i/", (int)(sizeof(prefix) - (slash + 1 - prefix)));

    return !Q_stricmpn(url, prefix, (int)strlen(prefix)) ? qtrue : qfalse;
}

/*
==================
CL_WebImage_LocalName

Where the picture lands: the link's checksum and its own extension, so two links never write
over each other and nothing the link says becomes part of the path
==================
*/
static void CL_WebImage_LocalName(const char* url, char* local, int localSize, char* base, int baseSize) {
    unsigned sum = Com_BlockChecksum(url, (int)strlen(url));

    Com_sprintf(base, baseSize, "%s/%08x", WEB_IMAGE_DIR, sum);
    Com_sprintf(local, localSize, "%s%s", base, CL_WebImage_Extension(url));
}

/*
==============================================================================

THE FRAMES OF A GIF

==============================================================================
*/

typedef struct {
    webImage_t* img;
    int step;      // every step-th frame is kept
    int kept;
    char homePath[MAX_OSPATH];
} gifWriter_t;

/*
==================
CL_WebImage_WriteTGA

One frame, brought down to GIF_MAX_SIDE with a box filter and written as an uncompressed TGA with
its alpha. The rows go from the bottom up: the renderer's loader takes every TGA that way and only
warns about the header's top-down bit, so a picture written top row first comes out upside down
==================
*/
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
            // the TGA wants blue, green, red, alpha
            px[0] = (byte)(sum[2] / n);
            px[1] = (byte)(sum[1] / n);
            px[2] = (byte)(sum[0] / n);
            px[3] = (byte)(sum[3] / n);
        }
    }

    memset(header, 0, sizeof(header));
    header[2] = 2;  // uncompressed true colour
    header[12] = (byte)(w & 255);
    header[13] = (byte)(w >> 8);
    header[14] = (byte)(h & 255);
    header[15] = (byte)(h >> 8);
    header[16] = 32;
    header[17] = 0x08;  // eight bits of alpha, the first row the bottom one

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

    // a frame between two kept ones only adds its time to the one before
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

// how many images a GIF has, counted without decoding a single one
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
            pos++;  // the label
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
            pos++;  // the LZW minimum code size
            count++;
        } else {
            break;
        }
        // the sub-blocks
        while (pos < length && (size = data[pos++]) > 0) {
            pos += size;
        }
    }

    return count;
}

/*
==================
CL_WebImage_DecodeGif

In the fetching thread: the GIF on disk becomes its frames on disk, and a small index beside them
says how many there are and how long each lasts, so the next time nothing is decoded again
==================
*/
static qboolean CL_WebImage_DecodeGif(webImage_t* img) {
    gifWriter_t gw;
    FILE* f;
    byte* data;
    long length;
    int total, i;
    char ospath[MAX_OSPATH];
    char* slash;

    f = Sys_FOpen(img->ospath, "rb");
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

    // the frames go beside the GIF, in the same folder
    Q_strncpyz(gw.homePath, img->ospath, sizeof(gw.homePath));
    slash = strrchr(gw.homePath, PATH_SEP);
    if (!slash) {
        free(data);
        return qfalse;
    }
    *slash = '\0';
    slash = strrchr(gw.homePath, PATH_SEP);  // and above webimages, where the names start
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

    // the index: the count and every delay
    Com_sprintf(ospath, sizeof(ospath), "%s%c%s.frames", gw.homePath, PATH_SEP, img->base);
    f = Sys_FOpen(ospath, "wb");
    if (f) {
        fprintf(f, "%d", img->numFrames);
        for (i = 0; i < img->numFrames; i++) {
            fprintf(f, " %d", img->delays[i]);
        }
        fprintf(f, "\n");
        fclose(f);
    }

    return qtrue;
}

// a GIF decoded another day: its index, if the frames are still there
static qboolean CL_WebImage_ReadIndex(webImage_t* img) {
    char name[MAX_QPATH];
    char* text = NULL;
    const char* p;
    int i, n;

    Com_sprintf(name, sizeof(name), "%s.frames", img->base);
    if (FS_ReadFile(name, (void**)&text) <= 0 || !text) {
        return qfalse;
    }

    p = text;
    n = atoi(COM_Parse((char**)&p));
    if (n <= 0 || n > MAX_GIF_FRAMES) {
        FS_FreeFile(text);
        return qfalse;
    }
    img->numFrames = n;
    img->duration = 0;
    for (i = 0; i < n; i++) {
        img->delays[i] = atoi(COM_Parse((char**)&p));
        if (img->delays[i] <= 0) {
            img->delays[i] = 100;
        }
        img->duration += img->delays[i];
    }
    FS_FreeFile(text);

    Com_sprintf(name, sizeof(name), "%s_f%02d.tga", img->base, n - 1);
    return FS_FileExists(name);
}

/*
==============================================================================

FETCHING

==============================================================================
*/

#if !defined(_WIN32) && defined(USE_CURL)
static size_t CL_WebImage_CurlWrite(void* buffer, size_t size, size_t nmemb, void* stream) {
    return fwrite(buffer, size, nmemb, (FILE*)stream);
}

/*
==================
CL_WebImage_Fetch

Off Windows the engine's own libcurl does it, the one it opens to download maps. It blocks,
which is what the thread is for
==================
*/
static qboolean CL_WebImage_Fetch(const char* url, const char* ospath, int maxBytes) {
    CURL* easy;
    FILE* out;
    CURLcode res;

    if (!CL_cURL_Init()) {
        return qfalse;
    }

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
    qcurl_easy_setopt_warn(easy, CURLOPT_WRITEDATA, out);
    qcurl_easy_setopt_warn(easy, CURLOPT_FAILONERROR, 1);
    qcurl_easy_setopt_warn(easy, CURLOPT_FOLLOWLOCATION, 1);
    qcurl_easy_setopt_warn(easy, CURLOPT_MAXREDIRS, 5);
    qcurl_easy_setopt_warn(easy, CURLOPT_NOPROGRESS, 1);
    qcurl_easy_setopt_warn(easy, CURLOPT_CONNECTTIMEOUT, 10);
    qcurl_easy_setopt_warn(easy, CURLOPT_TIMEOUT, 30);
    qcurl_easy_setopt_warn(easy, CURLOPT_MAXFILESIZE, (long)maxBytes);
    // a link in a chat line goes nowhere but the web
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

/*
==================
CL_WebImage_Thread

All the waiting happens here, away from the frame: the fetch, and for a GIF its frames too
==================
*/
static void CL_WebImage_Thread(void* argument) {
    webImage_t* img = (webImage_t*)argument;

    if (!CL_WebImage_Fetch(img->url, img->ospath, img->maxBytes)) {
        img->result = -1;
        return;
    }
    if (img->gif && !CL_WebImage_DecodeGif(img)) {
        img->result = -1;
        return;
    }
    img->result = 1;
}

/*
==================
CL_WebImage_Register

The renderer takes what is on disk. A still picture is one shader; a GIF's frames are handed over
a few per client frame, so a long one does not stall the game while it loads
==================
*/
static void CL_WebImage_Register(webImage_t* img) {
    char name[MAX_QPATH];
    int n = 0;

    if (!img->gif) {
        img->frames[0] = re.RegisterShaderNoMip(img->local);
        img->numFrames = 1;
        img->registered = 1;
        img->state = img->frames[0] ? WI_READY : WI_FAILED;
        return;
    }

    img->state = WI_READY;
    while (img->registered < img->numFrames && n < REGISTER_PER_FRAME) {
        Com_sprintf(name, sizeof(name), "%s_f%02d.tga", img->base, img->registered);
        img->frames[img->registered] = re.RegisterShaderNoMip(name);
        img->registered++;
        n++;
    }
    if (!img->frames[0]) {
        img->state = WI_FAILED;
    }
}

/*
==================
CL_WebImage_Frame

Picks up whatever the threads finished and keeps handing GIF frames to the renderer, once per
client frame
==================
*/
static void CL_ImageUpload_Frame(void);

void CL_WebImage_Frame(void) {
    int i;

    CL_ImageUpload_Frame();

    for (i = 0; i < numWebImages; i++) {
        webImage_t* img = &webImages[i];

        if (img->state == WI_FETCHING && img->result != 0) {
            webImagesFetching--;
            if (img->result > 0) {
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

/*
==================
CL_WebImage_Get

The shader for a link at this moment -- for a GIF, the frame its clock is on -- or zero while there
is none. The first time a link is asked for, the picture starts coming
==================
*/
qhandle_t CL_WebImage_Get(const char* url) {
    webImage_t* img;
    fileHandle_t f;
    int i, oldest, t, frame;

    if (!cls.rendererStarted || !CL_WebImage_UrlOk(url)) {
        return 0;
    }
    if ((!cl_webImages || !cl_webImages->integer) && !CL_WebImage_FromHost(url)) {
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
        // after a video restart the shaders are gone, but the files are still here
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
        // a frame the renderer has not taken yet waits on the first one
        return frame < img->registered && img->frames[frame] ? img->frames[frame] : img->frames[0];
    }

    if (webImagesFetching >= MAX_WEB_FETCHING) {
        return 0;  // in a moment, when one of the others is done
    }

    if (numWebImages < MAX_WEB_IMAGES) {
        img = &webImages[numWebImages++];
    } else {
        // the one nobody has looked at for longest gives up its slot, unless it is still coming
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
    img->maxBytes = cl_webImageMaxKB->integer * 1024;

    // it may be here from another day
    if (!img->gif && FS_FileExists(img->local)) {
        CL_WebImage_Register(img);
        return img->frames[0];
    }
    if (img->gif && CL_WebImage_ReadIndex(img)) {
        CL_WebImage_Register(img);
        return img->frames[0];
    }

    // the filesystem makes the folder; the thread writes the file itself, by its own path
    f = FS_FOpenFileWrite(img->local);
    if (!f) {
        img->state = WI_FAILED;
        return 0;
    }
    FS_FCloseFile(f);
    Q_strncpyz(img->ospath, FS_BuildOSPath(Cvar_VariableString("fs_homepath"), FS_GetCurrentGameDir(), img->local),
               sizeof(img->ospath));

    img->state = WI_FETCHING;
    img->result = 0;

    if (!Sys_ThreadCreate(CL_WebImage_Thread, img)) {
        img->state = WI_FAILED;
        return 0;
    }

    webImagesFetching++;

    return 0;
}

/*
==============================================================================

SENDING ONE

[ioquakelive] What is on the clipboard goes up to the host and the link comes back into the line
being typed. A picture is made a JPEG with the renderer's own encoder; an image file copied on the
desktop goes as it is, so a GIF keeps moving; and the link of a picture somewhere else is fetched
and sent up again, so everybody sees it from the host. The waiting happens on a thread

==============================================================================
*/

typedef enum {
    UP_BYTES,  // bytes already in hand
    UP_LINK    // a link to fetch first
} webUploadKind_t;

typedef struct {
    webUploadKind_t kind;
    byte* data;
    int length;
    char contentType[32];
    char link[MAX_OSPATH];
    char tempPath[MAX_OSPATH];
    volatile int result;  // 0 going, 1 done, -1 failed
    char reply[512];
} webUpload_t;

static webUpload_t webUpload;
static qboolean webUploading;
static field_t* webUploadField;  // the line the link goes back to

static const char* CL_ImageContentType(const char* name) {
    const char* ext = CL_WebImage_Extension(name);

    if (!ext) {
        return NULL;
    }
    if (!Q_stricmp(ext, ".png")) {
        return "image/png";
    }
    if (!Q_stricmp(ext, ".jpg") || !Q_stricmp(ext, ".jpeg")) {
        return "image/jpeg";
    }
    if (!Q_stricmp(ext, ".gif")) {
        return "image/gif";
    }
    return NULL;  // the host keeps these three and nothing else
}

/*
==================
CL_ImageUpload_Thread
==================
*/
static void CL_ImageUpload_Thread(void* argument) {
    webUpload_t* up = (webUpload_t*)argument;
    char header[256];

    // a link elsewhere: fetched first, then sent up like a file
    if (up->kind == UP_LINK) {
        FILE* f;
        long length;

        if (!CL_WebImage_Fetch(up->link, up->tempPath, cl_imageMaxKB->integer * 1024)) {
            up->result = -1;
            return;
        }
        f = Sys_FOpen(up->tempPath, "rb");
        if (!f) {
            up->result = -1;
            return;
        }
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        up->data = length > 0 ? (byte*)malloc(length) : NULL;
        if (!up->data || fread(up->data, 1, length, f) != (size_t)length) {
            fclose(f);
            up->result = -1;
            return;
        }
        fclose(f);
        remove(up->tempPath);
        up->length = (int)length;
    }

    Com_sprintf(header, sizeof(header), "X-IOQL-Key: %s\r\n", cl_imageKey->string);

    if (Sys_WebPost(cl_imageHost->string, up->contentType, header, up->data, up->length, up->reply,
                    sizeof(up->reply))) {
        up->result = 1;
    } else {
        up->result = -1;
    }
}

// the link into the line being typed, as if it had been typed there
static void CL_ImageUpload_Insert(const char* text) {
    field_t* field = webUploadField;

    if (!field) {
        return;
    }
    // only while that line is still being typed
    if (field == &chatField && !(Key_GetCatcher() & KEYCATCH_MESSAGE)) {
        return;
    }
    if (field == &g_consoleField && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    if (field->buffer[0] && field->buffer[strlen(field->buffer) - 1] != ' ') {
        Q_strcat(field->buffer, sizeof(field->buffer), " ");
    }
    Q_strcat(field->buffer, sizeof(field->buffer), text);
    field->cursor = (int)strlen(field->buffer);
}

/*
==================
CL_ImageUpload_Frame

Picks up the answer, once per client frame: the link goes to the line being typed and to the
console. When a link from elsewhere could not be sent up, it goes as it was
==================
*/
static void CL_ImageUpload_Frame(void) {
    char* p;

    if (!webUploading || webUpload.result == 0) {
        return;
    }

    webUploading = qfalse;
    free(webUpload.data);
    webUpload.data = NULL;

    if (webUpload.result < 0) {
        if (webUpload.kind == UP_LINK) {
            Com_Printf("^3No se pudo subir la imagen del link; va el link tal cual.\n");
            CL_ImageUpload_Insert(webUpload.link);
        } else {
            Com_Printf("^3La imagen no se pudo subir.\n");
        }
        return;
    }

    // the answer is the link and nothing else
    for (p = webUpload.reply; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }

    if (!webUpload.reply[0]) {
        Com_Printf("^3La imagen se subio pero no volvio ningun link.\n");
        return;
    }

    Com_Printf("Imagen: %s\n", webUpload.reply);
    CL_ImageUpload_Insert(webUpload.reply);
}

static qboolean CL_ImageUpload_Start(void) {
    webUploading = qtrue;

    if (!Sys_ThreadCreate(CL_ImageUpload_Thread, &webUpload)) {
        Com_Printf("^3No se pudo empezar la subida.\n");
        free(webUpload.data);
        webUpload.data = NULL;
        webUploading = qfalse;
        return qfalse;
    }
    return qtrue;
}

// a picture off the clipboard, made a JPEG
static qboolean CL_ImagePaste_Bitmap(void) {
    byte* rgb;
    byte* flipped;
    byte* jpeg;
    size_t size;
    int width = 0, height = 0, length, y;

    rgb = Sys_ClipboardImage(&width, &height);
    if (!rgb) {
        return qfalse;
    }

    // the renderer's encoder takes the rows from the bottom up, as a screenshot comes off the
    // card, and the clipboard hands them over the other way round
    flipped = (byte*)malloc((size_t)width * height * 3);
    if (!flipped) {
        free(rgb);
        return qtrue;
    }
    for (y = 0; y < height; y++) {
        Com_Memcpy(flipped + (size_t)y * width * 3, rgb + (size_t)(height - 1 - y) * width * 3, width * 3);
    }
    free(rgb);

    size = (size_t)width * height * 3 + 8192;
    jpeg = (byte*)malloc(size);
    if (!jpeg) {
        free(flipped);
        return qtrue;
    }

    length = (int)re.SaveJPGToBuffer(jpeg, size, 92, width, height, flipped, 0);
    free(flipped);

    if (length <= 0) {
        Com_Printf("^3No se pudo armar la imagen.\n");
        free(jpeg);
        return qtrue;
    }
    if (length > cl_imageMaxKB->integer * 1024) {
        Com_Printf("^3La imagen pesa %d KB y el tope es %d KB.\n", length / 1024, cl_imageMaxKB->integer);
        free(jpeg);
        return qtrue;
    }

    Com_Memset(&webUpload, 0, sizeof(webUpload));
    webUpload.kind = UP_BYTES;
    webUpload.data = jpeg;
    webUpload.length = length;
    Q_strncpyz(webUpload.contentType, "image/jpeg", sizeof(webUpload.contentType));

    Com_Printf("Subiendo la imagen (%d x %d, %d KB)...\n", width, height, length / 1024);
    CL_ImageUpload_Start();
    return qtrue;
}

// an image file copied on the desktop, as it is
static qboolean CL_ImagePaste_File(void) {
    char path[MAX_OSPATH];
    const char* type;
    FILE* f;
    long length;
    byte* data;

    if (!Sys_ClipboardImageFile(path, sizeof(path))) {
        return qfalse;
    }

    type = CL_ImageContentType(path);
    if (!type) {
        Com_Printf("^3Ese archivo no es PNG, JPEG ni GIF.\n");
        return qtrue;
    }

    f = Sys_FOpen(path, "rb");
    if (!f) {
        Com_Printf("^3No se pudo abrir %s.\n", path);
        return qtrue;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length <= 0 || length > cl_imageMaxKB->integer * 1024) {
        fclose(f);
        Com_Printf("^3La imagen pesa %ld KB y el tope es %d KB.\n", length / 1024, cl_imageMaxKB->integer);
        return qtrue;
    }
    data = (byte*)malloc(length);
    if (!data || fread(data, 1, length, f) != (size_t)length) {
        fclose(f);
        free(data);
        return qtrue;
    }
    fclose(f);

    Com_Memset(&webUpload, 0, sizeof(webUpload));
    webUpload.kind = UP_BYTES;
    webUpload.data = data;
    webUpload.length = (int)length;
    Q_strncpyz(webUpload.contentType, type, sizeof(webUpload.contentType));

    {
        // the file's own name, whichever way its folders are separated
        const char* name = path;
        const char* p;

        for (p = path; *p; p++) {
            if (*p == '/' || *p == '\\') {
                name = p + 1;
            }
        }
        Com_Printf("Subiendo %s (%ld KB)...\n", name, length / 1024);
    }
    CL_ImageUpload_Start();
    return qtrue;
}

// the link of a picture somewhere else, sent up again from the host
static qboolean CL_ImagePaste_Link(void) {
    char* text = Sys_GetClipboardData();
    const char* type;
    fileHandle_t f;
    char local[MAX_QPATH];

    if (!text) {
        return qfalse;
    }
    if (!CL_WebImage_UrlOk(text) || CL_WebImage_FromHost(text) || !(type = CL_ImageContentType(text))) {
        Z_Free(text);
        return qfalse;  // plain text, or already the host's: it pastes as it is
    }

    Com_Memset(&webUpload, 0, sizeof(webUpload));
    webUpload.kind = UP_LINK;
    Q_strncpyz(webUpload.link, text, sizeof(webUpload.link));
    Q_strncpyz(webUpload.contentType, type, sizeof(webUpload.contentType));
    Z_Free(text);

    Com_sprintf(local, sizeof(local), "%s/subiendo%s", WEB_IMAGE_DIR, CL_WebImage_Extension(webUpload.link));
    f = FS_FOpenFileWrite(local);
    if (!f) {
        return qfalse;
    }
    FS_FCloseFile(f);
    Q_strncpyz(webUpload.tempPath, FS_BuildOSPath(Cvar_VariableString("fs_homepath"), FS_GetCurrentGameDir(), local),
               sizeof(webUpload.tempPath));

    Com_Printf("Subiendo la imagen del link...\n");
    CL_ImageUpload_Start();
    return qtrue;
}

/*
==================
CL_ImagePasteInto

What Ctrl+V does in the chat and the console before anything else: a picture, an image file or the
link of one on the clipboard goes up, and the answer comes back into that line. qfalse means the
clipboard holds plain text, which pastes the usual way
==================
*/
qboolean CL_ImagePasteInto(field_t* field) {
    if (!cl_imageHost || !cl_imageHost->string[0]) {
        return qfalse;
    }
    if (webUploading) {
        Com_Printf("Ya hay una imagen subiendo.\n");
        return qtrue;
    }

    webUploadField = field;

    if (CL_ImagePaste_File() || CL_ImagePaste_Bitmap() || CL_ImagePaste_Link()) {
        return qtrue;
    }

    webUploadField = NULL;
    return qfalse;
}

/*
==================
CL_ImagePaste_f

The command, for a bind: into whichever line is open, the chat's or the console's
==================
*/
static void CL_ImagePaste_f(void) {
    field_t* field = NULL;

    if (Key_GetCatcher() & KEYCATCH_MESSAGE) {
        field = &chatField;
    } else if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        field = &g_consoleField;
    }

    if (!CL_ImagePasteInto(field)) {
        Com_Printf("^3No hay ninguna imagen en el portapapeles.\n");
    }
}

/*
==================
CL_WebImage_Init
==================
*/
void CL_WebImage_Init(void) {
    cl_webImages = Cvar_Get("cl_webImages", "0", CVAR_ARCHIVE);
    cl_webImageMaxKB = Cvar_Get("cl_webImageMaxKB", "8192", CVAR_ARCHIVE);

    // [ioquakelive] where a picture of your own goes. QuakeClub's endpoint takes PNG, JPEG and GIF,
    // keeps them for a month and answers with the link
    cl_imageHost = Cvar_Get("cl_imageHost", "https://quakeclub.com/ioql/upload", CVAR_ARCHIVE);

    // The endpoint asks for this in a header, and it rides in every copy of the game: it is no
    // secret and it guards nothing. It only keeps whatever crawls the web from posting pictures
    // by accident. What holds the door is on the other side -- nginx's rate limit, the size cap,
    // the three formats it will take and the month it keeps them
    cl_imageKey = Cvar_Get("cl_imageKey", "IdtyFF3bEOWEhiT69HV6wsBNFwuOgTfC", CVAR_ARCHIVE);
    cl_imageMaxKB = Cvar_Get("cl_imageMaxKB", "8192", CVAR_ARCHIVE);

    Cmd_AddCommand("imagepaste", CL_ImagePaste_f);
}

/*
==================
CL_WebImage_Shutdown

Whatever is still coming is left alone: the thread writes to its own file and to its own slot,
and the table is not read again until it is asked for
==================
*/
void CL_WebImage_Shutdown(void) {
    int i;

    // the shaders go with the renderer; what was fetched stays on disk and is asked for again
    for (i = 0; i < numWebImages; i++) {
        Com_Memset(webImages[i].frames, 0, sizeof(webImages[i].frames));
        webImages[i].registered = 0;
    }
}
