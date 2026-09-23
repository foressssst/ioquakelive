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
// cl_webimage.c -- [ioquakelive] NOT FROM RETAIL: the picture behind a link somebody wrote.
//
// A link that ends in a picture is fetched once on a thread of its own, kept in the player's own
// folder and handed to the renderer as a shader; the console draws it under the link the pointer
// is on. Nothing is fetched unless cl_webImages says so, because asking for a picture tells
// whoever hosts it that this machine is here, and the link comes from whoever typed it.

#include "client.h"

#if !defined(_WIN32) && defined(USE_CURL)
#include "cl_curl.h"
#endif

#define WEB_IMAGE_DIR "webimages"
#define MAX_WEB_IMAGES 24
#define MAX_WEB_FETCHING 2  // at a time, so a console full of links is not a flood

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
    qhandle_t handle;
    webImageState_t state;
    int lastUsed;
    int maxBytes;
    volatile int result;  // 0 while the thread runs, 1 done, -1 failed
} webImage_t;

static webImage_t webImages[MAX_WEB_IMAGES];
static int numWebImages;
static int webImagesFetching;

cvar_t* cl_webImages;
cvar_t* cl_webImageMaxKB;

/*
==================
CL_WebImage_Extension

The picture formats the renderer can read. A link that does not end in one of them is not a
picture as far as this is concerned -- an animated gif needs a decoder this engine has not got
==================
*/
static const char* CL_WebImage_Extension(const char* url) {
    static const char* known[] = {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".pcx"};
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
CL_WebImage_LocalName

Where the picture lands: the link's checksum and its own extension, so two links never write
over each other and nothing the link says becomes part of the path
==================
*/
static void CL_WebImage_LocalName(const char* url, char* out, int size) {
    unsigned sum = Com_BlockChecksum(url, (int)strlen(url));

    Com_sprintf(out, size, "%s/%08x%s", WEB_IMAGE_DIR, sum, CL_WebImage_Extension(url));
}

/*
==================
CL_WebImage_Register

The file is there: the renderer takes it. One it cannot read leaves the slot failed, so it is
not asked for again
==================
*/
static void CL_WebImage_Register(webImage_t* img) {
    img->handle = re.RegisterShaderNoMip(img->local);
    img->state = img->handle ? WI_READY : WI_FAILED;

    if (img->state == WI_FAILED) {
        Com_DPrintf("CL_WebImage: the renderer could not read %s\n", img->local);
    }
}

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

All the waiting happens here, away from the frame
==================
*/
static void CL_WebImage_Thread(void* argument) {
    webImage_t* img = (webImage_t*)argument;

    img->result = CL_WebImage_Fetch(img->url, img->ospath, img->maxBytes) ? 1 : -1;
}

/*
==================
CL_WebImage_Frame

Picks up whatever the threads finished, once per client frame
==================
*/
static void CL_ImageUpload_Frame(void);

void CL_WebImage_Frame(void) {
    int i;

    CL_ImageUpload_Frame();

    if (!webImagesFetching) {
        return;
    }

    for (i = 0; i < numWebImages; i++) {
        webImage_t* img = &webImages[i];

        if (img->state != WI_FETCHING || img->result == 0) {
            continue;
        }

        webImagesFetching--;

        if (img->result > 0) {
            CL_WebImage_Register(img);
        } else {
            img->state = WI_FAILED;
        }
    }
}

/*
==================
CL_WebImage_Get

The shader for a link, or zero while there is none. The first time a link is asked for the
picture starts coming; the frames after that find it here
==================
*/
qhandle_t CL_WebImage_Get(const char* url) {
    webImage_t* img;
    fileHandle_t f;
    int i, oldest;

    if (!cl_webImages || !cl_webImages->integer || !cls.rendererStarted) {
        return 0;
    }
    if (!CL_WebImage_UrlOk(url)) {
        return 0;
    }

    for (i = 0; i < numWebImages; i++) {
        if (!Q_stricmp(webImages[i].url, url)) {
            webImages[i].lastUsed = cls.realtime;
            // after a video restart the shader is gone, but the file is still here
            if (webImages[i].state == WI_READY && !webImages[i].handle) {
                CL_WebImage_Register(&webImages[i]);
            }
            return webImages[i].handle;
        }
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
    CL_WebImage_LocalName(url, img->local, sizeof(img->local));
    img->lastUsed = cls.realtime;
    img->maxBytes = cl_webImageMaxKB->integer * 1024;

    // it may be here from another day
    if (FS_FileExists(img->local)) {
        CL_WebImage_Register(img);
        return img->handle;
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

[ioquakelive] What is on the clipboard goes up to the endpoint as a PNG and the link comes back
into the line being typed. The picture is squeezed with the zlib the engine already carries, and
both the wait and the sending happen on a thread

==============================================================================
*/

cvar_t* cl_imageHost;
cvar_t* cl_imageKey;
cvar_t* cl_imageMaxKB;

typedef struct {
    byte* png;
    int length;
    volatile int result;  // 0 going, 1 done, -1 failed
    char reply[512];
} webUpload_t;

static webUpload_t webUpload;
static qboolean webUploading;

/*
==================
CL_ImageUpload_Thread
==================
*/
static void CL_ImageUpload_Thread(void* argument) {
    webUpload_t* up = (webUpload_t*)argument;
    char header[256];

    Com_sprintf(header, sizeof(header), "X-IOQL-Key: %s\r\n", cl_imageKey->string);

    if (Sys_WebPost(cl_imageHost->string, "image/jpeg", header, up->png, up->length, up->reply, sizeof(up->reply))) {
        up->result = 1;
    } else {
        up->result = -1;
    }
}

/*
==================
CL_ImageUpload_Frame

Picks up the answer, once per client frame: the link goes to the line being typed when there is
one, and to the console either way
==================
*/
static void CL_ImageUpload_Frame(void) {
    char* p;

    if (!webUploading || webUpload.result == 0) {
        return;
    }

    webUploading = qfalse;
    free(webUpload.png);
    webUpload.png = NULL;

    if (webUpload.result < 0) {
        Com_Printf("^3La imagen no se pudo subir.\n");
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

    if (Key_GetCatcher() & KEYCATCH_MESSAGE) {
        if (chatField.buffer[0] && chatField.buffer[strlen(chatField.buffer) - 1] != ' ') {
            Q_strcat(chatField.buffer, sizeof(chatField.buffer), " ");
        }
        Q_strcat(chatField.buffer, sizeof(chatField.buffer), webUpload.reply);
        chatField.cursor = (int)strlen(chatField.buffer);
    }
}

/*
==================
CL_ImagePaste_f

The picture on the clipboard, up to the endpoint. What a screen clip is for
==================
*/
static void CL_ImagePaste_f(void) {
    byte* rgb;
    byte* flipped;
    byte* jpeg;
    size_t size;
    int width = 0, height = 0, length = 0, y;

    if (webUploading) {
        Com_Printf("Ya hay una imagen subiendo.\n");
        return;
    }
    if (!cl_imageHost->string[0]) {
        Com_Printf("^3cl_imageHost esta vacia: no hay donde subir la imagen.\n");
        return;
    }

    rgb = Sys_ClipboardImage(&width, &height);
    if (!rgb) {
        Com_Printf("^3No hay ninguna imagen en el portapapeles.\n");
        return;
    }

    // the renderer's encoder takes the rows from the bottom up, as a screenshot comes off the
    // card, and the clipboard hands them over the other way round
    flipped = (byte*)malloc((size_t)width * height * 3);
    if (!flipped) {
        free(rgb);
        return;
    }
    for (y = 0; y < height; y++) {
        Com_Memcpy(flipped + (size_t)y * width * 3, rgb + (size_t)(height - 1 - y) * width * 3, width * 3);
    }
    free(rgb);

    size = (size_t)width * height * 3 + 8192;
    jpeg = (byte*)malloc(size);
    if (!jpeg) {
        free(flipped);
        return;
    }

    length = (int)re.SaveJPGToBuffer(jpeg, size, 92, width, height, flipped, 0);
    free(flipped);

    if (length <= 0) {
        Com_Printf("^3No se pudo armar la imagen.\n");
        free(jpeg);
        return;
    }
    if (length > cl_imageMaxKB->integer * 1024) {
        Com_Printf("^3La imagen pesa %d KB y el tope es %d KB.\n", length / 1024, cl_imageMaxKB->integer);
        free(jpeg);
        return;
    }

    Com_Memset(&webUpload, 0, sizeof(webUpload));
    webUpload.png = jpeg;
    webUpload.length = length;
    webUploading = qtrue;

    Com_Printf("Subiendo la imagen (%d x %d, %d KB)...\n", width, height, length / 1024);

    if (!Sys_ThreadCreate(CL_ImageUpload_Thread, &webUpload)) {
        Com_Printf("^3No se pudo empezar la subida.\n");
        free(jpeg);
        webUpload.png = NULL;
        webUploading = qfalse;
    }
}

/*
==================
CL_WebImage_Init
==================
*/
void CL_WebImage_Init(void) {
    cl_webImages = Cvar_Get("cl_webImages", "0", CVAR_ARCHIVE);
    cl_webImageMaxKB = Cvar_Get("cl_webImageMaxKB", "2048", CVAR_ARCHIVE);

    // [ioquakelive] where a picture of your own goes. QuakeClub's endpoint takes PNG and JPEG,
    // keeps them for a month and answers with the link
    cl_imageHost = Cvar_Get("cl_imageHost", "https://quakeclub.com/ioql/upload", CVAR_ARCHIVE);
    cl_imageKey = Cvar_Get("cl_imageKey", "IdtyFF3bEOWEhiT69HV6wsBNFwuOgTfC", CVAR_ARCHIVE);
    cl_imageMaxKB = Cvar_Get("cl_imageMaxKB", "4096", CVAR_ARCHIVE);

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
        webImages[i].handle = 0;
    }
}
