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

#include "client.h"

#define DW_ENV "IOQL_DEMOWINDOW"
#define DW_MAX_W 1920
#define DW_MAX_H 1200
#define DW_RING 1024
#define DW_HANDLE 15
#define DW_SCREEN 0.90f
#define DW_BAR 40.0f
#define DW_PARENT_SILENCE 20000
#define DW_CLOSE_WAIT 3000
#define DW_CHILD_WAIT 100
#define DW_FRAME_WAIT 50
#define DW_LOAD_PACE 25
#define DW_TEXT_LITERAL 0x40000000
#define DW_ICON_CLOSE "\xef\x80\x8d"

enum { DW_EV_KEY, DW_EV_CHAR, DW_EV_MOUSE };

typedef struct {
    int type;
    int a;
    int b;
} dwEvent_t;

typedef struct {
    volatile int heartbeat;
    volatile int quit;
    volatile int width;
    volatile int height;
    volatile int ready;
    volatile int seq;
    volatile int head;
    volatile int tail;
    volatile int load;
    volatile int playing;
    char demo[MAX_QPATH * 2];
    dwEvent_t ring[DW_RING];
    byte frames[2][DW_MAX_W * DW_MAX_H * 4];
} dwShared_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int bar;
} dwRect_t;

static const vec4_t dwRaised = {15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f};
static const vec4_t dwBorder = {51 / 255.0f, 51 / 255.0f, 51 / 255.0f, 1.0f};
static const vec4_t dwKey = {34 / 255.0f, 34 / 255.0f, 34 / 255.0f, 1.0f};
static const vec4_t dwText = {221 / 255.0f, 221 / 255.0f, 221 / 255.0f, 1.0f};
static const vec4_t dwSoft = {204 / 255.0f, 204 / 255.0f, 204 / 255.0f, 1.0f};
static const vec4_t dwMuted = {153 / 255.0f, 153 / 255.0f, 153 / 255.0f, 1.0f};
static const vec4_t dwDanger = {204 / 255.0f, 51 / 255.0f, 51 / 255.0f, 1.0f};

static dwShared_t* dw;
static void* dwTick;
static void* dwFrameSignal;

static void* dwProcess;
static qboolean dwOpen;
static int dwCloseTime;
static int dwLastSeq;
static int dwMouseX;
static int dwMouseY;
static qboolean dwOverClose;
static qboolean dwHandleReady;
static qboolean dwAssets;
static qhandle_t dwBackdrop;
static int dwFontSans;
static int dwFontBold;
static int dwFontIcons;
static char dwTitle[MAX_QPATH * 2];
static char dwHome[MAX_OSPATH];
static char dwBase[MAX_OSPATH];

static byte* dwCapture;
static byte* dwEncode;
static int dwCaptureW;
static int dwCaptureH;
static int dwChildBeat;
static int dwChildBeatTime;
static int dwChildLoad;
static int dwChildIssued;

qboolean CL_DemoWindowOpen(void) {
    return dwOpen;
}

static void CL_DemoWindowPush(int type, int a, int b) {
    int next = (dw->head + 1) % DW_RING;

    if (next == dw->tail) {
        return;
    }
    dw->ring[dw->head].type = type;
    dw->ring[dw->head].a = a;
    dw->ring[dw->head].b = b;
    dw->head = next;
}

static void CL_DemoWindowRect(dwRect_t* r) {
    int sw = cls.glconfig.vidWidth;
    int sh = cls.glconfig.vidHeight;

    r->bar = (int)(DW_BAR * sh / 1080.0f);
    r->w = (int)(sw * DW_SCREEN);
    r->h = (int)(sh * DW_SCREEN) - r->bar;
    r->x = (sw - r->w) / 2;
    r->y = (sh - r->h + r->bar) / 2;
}

static void CL_DemoWindowFinish(void) {
    Sys_PipeClose(dwProcess);
    dwProcess = NULL;
    Sys_SignalClose(dwTick);
    Sys_SignalClose(dwFrameSignal);
    dwTick = dwFrameSignal = NULL;
    Sys_SharedClose(dw);
    dw = NULL;
    dwOpen = qfalse;
    dwCloseTime = 0;
}

static void CL_DemoWindowClose(void) {
    if (!dwProcess || dwCloseTime) {
        return;
    }
    dw->quit = 1;
    dwOpen = qfalse;
    dwCloseTime = Sys_Milliseconds() + DW_CLOSE_WAIT;
}

static fileHandle_t CL_DemoWindowOpenDemo(const char* name) {
    int i;

    for (i = 0; i < 2; i++) {
        fileHandle_t f = 0;
        const char* path = i ? va("demos/%s.%s%d", name, DEMOEXT, PROTOCOL_VERSION) : va("demos/%s", name);

        if (FS_FOpenFileRead(path, &f, qtrue) >= 0 && f) {
            return f;
        }
    }
    return 0;
}

static qboolean CL_DemoWindowLaunch(const char* name) {
    char exe[MAX_OSPATH];
    char log[MAX_OSPATH];
    char sw[16];
    char sh[16];
    int w, h;
    dwRect_t r;
    const char* argv[] = {exe,
                          "+set", "fs_basepath", dwBase,
                          "+set", "fs_homepath", dwHome,
                          "+set", "r_fullscreen", "0",
                          "+set", "r_windowedMode", "-1",
                          "+set", "r_windowedWidth", sw,
                          "+set", "r_windowedHeight", sh,
                          "+set", "r_swapInterval", "0",
                          "+set", "com_maxfps", "0",
                          "+set", "com_maxfpsUnfocused", "0",
                          "+set", "com_maxfpsMinimized", "0",
                          "+set", "com_preciseFrameTiming", "0",
                          "+set", "com_introPlayed", "1",
                          "+set", "com_ignoreCrash", "1",
                          "+set", "s_muteBackground", "0",
                          "+set", "cg_drawFPS", "0",
                          "+set", "net_enabled", "0",
                          "+set", "logfile", "0",
                          NULL};

    CL_DemoWindowRect(&r);
    w = MIN(r.w, DW_MAX_W) & ~1;
    h = MIN((int)((float)w * r.h / r.w), DW_MAX_H) & ~1;
    Com_sprintf(sw, sizeof(sw), "%i", w);
    Com_sprintf(sh, sizeof(sh), "%i", h);
    Com_sprintf(exe, sizeof(exe), "%s%c%s", Sys_BinaryPath(), PATH_SEP, Sys_BinaryName());
    Com_sprintf(log, sizeof(log), "%s", FS_BuildOSPath(dwHome, BASEGAME_DIR, "demowindow.log"));

    dw = (dwShared_t*)Sys_SharedOpen(va("dw%d", Sys_PID()), sizeof(dwShared_t), qtrue);
    if (!dw) {
        return qfalse;
    }
    dwTick = Sys_SignalOpen(va("dwt%d", Sys_PID()), qtrue);
    dwFrameSignal = Sys_SignalOpen(va("dwf%d", Sys_PID()), qtrue);
    dw->heartbeat = dw->quit = dw->ready = dw->seq = dw->head = dw->tail = dw->playing = 0;
    dw->load = 1;
    Q_strncpyz(dw->demo, name, sizeof(dw->demo));

    Sys_SetEnv(DW_ENV, va("%d", Sys_PID()));
    dwProcess = Sys_PipeOpen(argv, log);
    Sys_SetEnv(DW_ENV, "");
    if (!dwProcess) {
        Sys_SignalClose(dwTick);
        Sys_SignalClose(dwFrameSignal);
        dwTick = dwFrameSignal = NULL;
        Sys_SharedClose(dw);
        dw = NULL;
        return qfalse;
    }
    dwLastSeq = 0;
    dwCloseTime = 0;
    return qtrue;
}

static void CL_DemoWindowNames(const char* name) {
    char* ext;

    Q_strncpyz(dwTitle, name, sizeof(dwTitle));
    ext = strrchr(dwTitle, '.');
    if (ext && !Q_stricmpn(ext + 1, DEMOEXT, strlen(DEMOEXT))) {
        *ext = '\0';
    }
}

static void CL_DemoWindowPlay(const char* name) {
    fileHandle_t f;

    if (strpbrk(name, "\";\r\n") || strstr(name, "..") || !(f = CL_DemoWindowOpenDemo(name))) {
        Com_Printf("Demo not found: %s\n", name);
        return;
    }
    FS_FCloseFile(f);

    if (dwProcess && dwCloseTime) {
        Sys_PipeKill(dwProcess);
        CL_DemoWindowFinish();
    }
    if (dwProcess) {
        Q_strncpyz(dw->demo, name, sizeof(dw->demo));
        dw->load++;
    } else if (!CL_DemoWindowLaunch(name)) {
        Com_Printf(S_COLOR_RED "Could not open the demo window\n");
        return;
    }

    CL_DemoWindowNames(name);
    dwAssets = qfalse;
    if (uivm && (Key_GetCatcher() & KEYCATCH_UI)) {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE);
    }
    Con_Close();
    Key_ClearStates();
    dwOpen = qtrue;
}

qboolean CL_DemoWindowRoute(const char* name) {
    if (com_demoWindowParent || clc.state != CA_ACTIVE || clc.demoplaying || cl_timedemo->integer) {
        return qfalse;
    }
    CL_DemoWindowPlay(name);
    return qtrue;
}

void CL_DemoWindow_f(void) {
    const char* name = Cmd_Argv(1);

    if (com_demoWindowParent) {
        return;
    }
    if (Cmd_Argc() < 2) {
        if (dwOpen) {
            CL_DemoWindowClose();
        } else {
            Cbuf_ExecuteText(EXEC_INSERT, "ui_replays\n");
        }
        return;
    }
    if (strpbrk(name, "\";\r\n")) {
        Com_Printf("Demo not found: %s\n", name);
        return;
    }
    Cbuf_ExecuteText(EXEC_INSERT, va("demo \"%s\"\n", name));
}

void CL_DemoWindowInit(void) {
    Q_strncpyz(dwHome, Cvar_VariableString("fs_homepath"), sizeof(dwHome));
    Q_strncpyz(dwBase, Cvar_VariableString("fs_basepath"), sizeof(dwBase));
}

void CL_DemoWindowRendererRestart(void) {
    dwHandleReady = qfalse;
    dwAssets = qfalse;
}

void CL_DemoWindowShutdown(void) {
    int i;

    if (!dwProcess) {
        return;
    }
    dw->quit = 1;
    for (i = 0; i < 20 && Sys_PipeAlive(dwProcess); i++) {
        Sys_Sleep(50);
    }
    if (Sys_PipeAlive(dwProcess)) {
        Sys_PipeKill(dwProcess);
    }
    CL_DemoWindowFinish();
}

qboolean CL_DemoWindowKey(int key, qboolean down) {
    dwRect_t r;

    if (!dwOpen || (Key_GetCatcher() & KEYCATCH_CONSOLE) || key == K_CONSOLE || key == K_ALT ||
        (key == K_ENTER && keys[K_ALT].down)) {
        return qfalse;
    }
    if (key == K_ESCAPE || (Key_GetBinding(key) && Q_stristr(Key_GetBinding(key), "demowindow"))) {
        if (down) {
            CL_DemoWindowClose();
        }
        return qtrue;
    }
    if (key == K_MOUSE1 && dwOverClose) {
        if (down) {
            CL_DemoWindowClose();
        }
        return qtrue;
    }
    CL_DemoWindowRect(&r);
    if (key >= K_MOUSE1 && key <= K_MOUSE5 &&
        (dwMouseX < r.x || dwMouseY < r.y || dwMouseX >= r.x + r.w || dwMouseY >= r.y + r.h)) {
        return qtrue;
    }
    CL_DemoWindowPush(DW_EV_KEY, key, down);
    return qtrue;
}

qboolean CL_DemoWindowChar(int ch) {
    if (!dwOpen || (Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return qfalse;
    }
    CL_DemoWindowPush(DW_EV_CHAR, ch, 0);
    return qtrue;
}

qboolean CL_DemoWindowMouse(int x, int y) {
    dwRect_t r;

    if (!dwOpen || (Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return qfalse;
    }
    CL_DemoWindowRect(&r);
    dwOverClose = (x >= r.x + r.w - r.bar && x < r.x + r.w && y >= r.y - r.bar && y < r.y) ? qtrue : qfalse;
    if ((x != dwMouseX || y != dwMouseY) && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h && dw->width) {
        CL_DemoWindowPush(DW_EV_MOUSE, (x - r.x) * dw->width / r.w, (y - r.y) * dw->height / r.h);
    }
    dwMouseX = x;
    dwMouseY = y;
    return qtrue;
}

static void CL_DemoWindowFill(float x, float y, float w, float h, const vec4_t color) {
    re.SetColor(color);
    re.DrawStretchPic(x, y, w, h, 0, 0, 1, 1, cls.whiteShader);
}

static void CL_DemoWindowText(float x, float y, const char* text, int font, float size, const vec4_t color) {
    re.SetColor(color);
    re.Font_DrawString((int)x, (int)y, text, font, size, -1, NULL, DW_TEXT_LITERAL);
}

static float CL_DemoWindowTextWidth(const char* text, int font, float size) {
    float e[5];

    re.TextBounds(text, 0, font, size, -1, e);
    return e[2] - e[0];
}

static int CL_DemoWindowFont(const char* path, int fallback) {
    int f = re.Font_Register(path);

    return f >= 0 ? f : fallback;
}

static void CL_DemoWindowAssets(void) {
    if (dwAssets) {
        return;
    }
    dwAssets = qtrue;
    dwFontSans = CL_DemoWindowFont("webui/fonts/NotoSans-Regular.ttf", 1);
    dwFontBold = CL_DemoWindowFont("webui/fonts/NotoSans-Bold.ttf", dwFontSans);
    dwFontIcons = re.Font_Register("webui/fonts/fontawesome-webfont.ttf");
    dwBackdrop = re.RegisterShaderNoMip("ui/assets/backscreen_smoke");
}

static void CL_DemoWindowAwaitFrame(void) {
    int start = Sys_Milliseconds();

    while (dw->seq == dwLastSeq && Sys_PipeAlive(dwProcess)) {
        int left = DW_FRAME_WAIT - (Sys_Milliseconds() - start);

        if (left <= 0) {
            break;
        }
        Sys_SignalWait(dwFrameSignal, left);
    }
}

static void CL_DemoWindowDrawLoading(const dwRect_t* r) {
    float s0 = ((1920.0f - 1080.0f * r->w / r->h) / 1920.0f) * 0.5f;

    CL_DemoWindowFill(r->x, r->y, r->w, r->h, dwRaised);
    if (dwBackdrop) {
        re.SetColor(NULL);
        re.DrawStretchPic(r->x, r->y, r->w, r->h, s0, 0.0f, 1.0f - s0, 1.0f, dwBackdrop);
    }
}

static void CL_DemoWindowDrawBar(const dwRect_t* r, float s) {
    char title[sizeof(dwTitle) + 4];
    float top = (float)(r->y - r->bar);
    float mid = top + r->bar * 0.5f;
    float x = r->x + 16.0f * s;
    float close = (float)(r->x + r->w - r->bar);
    float hint = 13.0f * s;
    float keySize = 11.0f * s;
    float right, w, keyW;
    int len;

    CL_DemoWindowFill(r->x, top, r->w, r->bar, dwRaised);
    CL_DemoWindowFill(r->x, r->y - 1, r->w, 1, dwBorder);

    if (dwOverClose) {
        CL_DemoWindowFill(close, top, r->bar, r->bar, dwDanger);
    }
    if (dwFontIcons >= 0) {
        w = CL_DemoWindowTextWidth(DW_ICON_CLOSE, dwFontIcons, 16.0f * s);
        CL_DemoWindowText(close + (r->bar - w) * 0.5f, mid + 6.0f * s, DW_ICON_CLOSE, dwFontIcons, 16.0f * s,
                          dwOverClose ? dwText : dwMuted);
    } else {
        w = CL_DemoWindowTextWidth("X", dwFontBold, 15.0f * s);
        CL_DemoWindowText(close + (r->bar - w) * 0.5f, mid + 5.5f * s, "X", dwFontBold, 15.0f * s,
                          dwOverClose ? dwText : dwMuted);
    }

    right = close - 16.0f * s;
    w = CL_DemoWindowTextWidth("to close", dwFontSans, hint);
    right -= w;
    CL_DemoWindowText(right, mid + 4.5f * s, "to close", dwFontSans, hint, dwMuted);
    keyW = CL_DemoWindowTextWidth("ESC", dwFontBold, keySize) + 12.0f * s;
    right -= 8.0f * s + keyW;
    CL_DemoWindowFill(right, mid - 10.0f * s, keyW, 20.0f * s, dwBorder);
    CL_DemoWindowFill(right + 1, mid - 10.0f * s + 1, keyW - 2, 20.0f * s - 2, dwKey);
    CL_DemoWindowText(right + 6.0f * s, mid + 4.0f * s, "ESC", dwFontBold, keySize, dwSoft);

    Q_strncpyz(title, dwTitle, sizeof(title));
    len = (int)strlen(title);
    while (len > 0 && CL_DemoWindowTextWidth(title, dwFontBold, 15.0f * s) > right - 24.0f * s - x) {
        len--;
        Com_sprintf(title + len, sizeof(title) - len, "...");
    }
    CL_DemoWindowText(x, mid + 5.5f * s, title, dwFontBold, 15.0f * s, dwText);
}

void CL_DemoWindowDraw(void) {
    static byte blank[16 * 16 * 4];
    dwRect_t r;
    float s;
    int i;

    if (com_demoWindowParent) {
        if (dw && dwChildLoad == dw->load && clc.state >= CA_LOADING) {
            re.TakeVideoFrame(dwCaptureW, dwCaptureH, dwCapture, dwEncode, qfalse);
        }
        return;
    }
    if (!dwOpen) {
        return;
    }
    CL_DemoWindowAssets();
    CL_DemoWindowRect(&r);
    s = cls.glconfig.vidHeight / 1080.0f;

    for (i = 3; i > 0; i--) {
        float pad = (i * i + 1) * s;
        vec4_t shadow = {0.0f, 0.0f, 0.0f, 0.12f * (4 - i)};

        CL_DemoWindowFill(r.x - pad, r.y - r.bar - pad, r.w + pad * 2, r.h + r.bar + pad * 2, shadow);
    }
    CL_DemoWindowFill(r.x - 1, r.y - r.bar - 1, r.w + 2, r.h + r.bar + 2, dwBorder);
    if (dw->playing == dw->load && dw->seq) {
        int seq, slot;

        CL_DemoWindowAwaitFrame();
        seq = dw->seq;
        slot = dw->ready;
        if (!dwHandleReady) {
            re.UploadCinematic(16, 16, 16, 16, blank, DW_HANDLE, qtrue);
            dwHandleReady = qtrue;
        }
        re.SetColor(NULL);
        re.DrawStretchRaw(r.x, r.y, r.w, r.h, dw->width, dw->height, dw->frames[slot], DW_HANDLE, seq != dwLastSeq);
        dwLastSeq = seq;
    } else {
        CL_DemoWindowDrawLoading(&r);
    }
    CL_DemoWindowDrawBar(&r, s);
    re.SetColor(NULL);
}

static void CL_DemoWindowChildOpen(void) {
    int need;

    dw = (dwShared_t*)Sys_SharedOpen(va("dw%d", com_demoWindowParent), sizeof(dwShared_t), qfalse);
    if (!dw) {
        Cbuf_AddText("quit\n");
        return;
    }
    dwTick = Sys_SignalOpen(va("dwt%d", com_demoWindowParent), qfalse);
    dwFrameSignal = Sys_SignalOpen(va("dwf%d", com_demoWindowParent), qfalse);
    dwCaptureW = MIN(cls.glconfig.vidWidth, DW_MAX_W);
    dwCaptureH = MIN(cls.glconfig.vidHeight, DW_MAX_H);
    need = (dwCaptureW * 4 + 64) * dwCaptureH + 64;
    dwCapture = Z_Malloc(need);
    dwEncode = Z_Malloc(PAD(dwCaptureW * 3, 4) * dwCaptureH);
    dw->width = dwCaptureW;
    dw->height = dwCaptureH;
    dwChildBeat = dw->heartbeat;
    dwChildBeatTime = Sys_Milliseconds();
}

void CL_DemoWindowWait(void) {
    int start;

    if (!com_demoWindowParent) {
        if (dwProcess) {
            dw->heartbeat++;
            Sys_SignalRaise(dwTick);
            if (dwOpen && dw->playing != dw->load) {
                Sys_SignalWait(dwFrameSignal, DW_LOAD_PACE);
            }
        }
        return;
    }
    if (!dw) {
        return;
    }
    start = Sys_Milliseconds();
    while (dw->heartbeat == dwChildBeat && !dw->quit) {
        int left = DW_CHILD_WAIT - (Sys_Milliseconds() - start);

        if (left <= 0) {
            break;
        }
        Sys_SignalWait(dwTick, left);
    }
    if (dw->heartbeat != dwChildBeat) {
        dwChildBeat = dw->heartbeat;
        dwChildBeatTime = Sys_Milliseconds();
    }
}

static void CL_DemoWindowChildQuit(void) {
    dw->quit = 1;
    Cbuf_AddText("quit\n");
}

void CL_DemoWindowFrame(void) {
    if (!com_demoWindowParent) {
        if (dwProcess) {
            if (!Sys_PipeAlive(dwProcess)) {
                CL_DemoWindowFinish();
            } else if (dwCloseTime && Sys_Milliseconds() > dwCloseTime) {
                Sys_PipeKill(dwProcess);
                CL_DemoWindowFinish();
            }
        }
        return;
    }

    if (!dw) {
        if (!cls.rendererStarted || !cls.uiStarted) {
            return;
        }
        CL_DemoWindowChildOpen();
        if (!dw) {
            return;
        }
    }

    if (dw->quit || Sys_Milliseconds() - dwChildBeatTime > DW_PARENT_SILENCE || Sys_ProcessExited(com_demoWindowParent)) {
        CL_DemoWindowChildQuit();
        return;
    }

    while (dw->tail != dw->head) {
        dwEvent_t ev = dw->ring[dw->tail];

        dw->tail = (dw->tail + 1) % DW_RING;
        switch (ev.type) {
            case DW_EV_KEY:
                Com_QueueEvent(Sys_Milliseconds(), SE_KEY, ev.a, ev.b, 0, NULL);
                break;
            case DW_EV_CHAR:
                Com_QueueEvent(Sys_Milliseconds(), SE_CHAR, ev.a, 0, 0, NULL);
                break;
            case DW_EV_MOUSE:
                Com_QueueEvent(Sys_Milliseconds(), SE_MOUSE_ABS, ev.a, ev.b, 0, NULL);
                break;
        }
    }

    if (dw->load != dwChildLoad) {
        dwChildLoad = dw->load;
        dw->playing = 0;
        dwChildIssued = cls.framecount;
        Cbuf_AddText(va("demo \"%s\"\n", dw->demo));
        return;
    }
    if (!clc.demoplaying && cls.framecount - dwChildIssued > 1) {
        CL_DemoWindowChildQuit();
    }
}

qboolean CL_DemoWindowTakeFrame(const byte* image, int size) {
    int slot, y, line;
    byte* out;

    if (!com_demoWindowParent) {
        return qfalse;
    }
    if (!dw) {
        return qtrue;
    }
    line = PAD(dwCaptureW * 3, 4);
    if (size < line * dwCaptureH) {
        return qtrue;
    }
    slot = !dw->ready;
    out = dw->frames[slot];
    for (y = dwCaptureH - 1; y >= 0; y--) {
        const byte* in = image + y * line;
        int x;

        for (x = 0; x < dwCaptureW; x++) {
            out[0] = in[2];
            out[1] = in[1];
            out[2] = in[0];
            out[3] = 255;
            out += 4;
            in += 3;
        }
    }
    dw->ready = slot;
    dw->seq++;
    dw->playing = dwChildLoad;
    Sys_SignalRaise(dwFrameSignal);
    return qtrue;
}
