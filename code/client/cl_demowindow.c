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
#define DW_MAX_W 1600
#define DW_MAX_H 1200
#define DW_RING 1024
#define DW_HANDLE 15
#define DW_SCREEN 0.85f
#define DW_BAR 24
#define DW_PARENT_SILENCE 20000
#define DW_CLOSE_WAIT 3000

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
    char demo[MAX_QPATH * 2];
    dwEvent_t ring[DW_RING];
    byte frames[2][DW_MAX_W * DW_MAX_H * 4];
} dwShared_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} dwRect_t;

static dwShared_t* dw;

static void* dwProcess;
static qboolean dwOpen;
static int dwCloseTime;
static int dwLastSeq;
static int dwWantW;
static int dwWantH;
static int dwMouseX;
static int dwMouseY;
static qboolean dwOverClose;
static qboolean dwHandleReady;
static char dwHome[MAX_OSPATH];
static char dwBase[MAX_OSPATH];

static byte* dwCapture;
static byte* dwEncode;
static int dwCaptureW;
static int dwCaptureH;
static int dwChildSeenBeat;
static int dwChildSeenTime;
static qboolean dwChildPlayed;

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
    int w = (int)(sw * DW_SCREEN);
    int h = (int)(sh * DW_SCREEN);

    r->w = w;
    r->h = h;
    r->x = (sw - w) / 2;
    r->y = (sh - h + DW_BAR) / 2;
}

static void CL_DemoWindowFinish(void) {
    Sys_PipeClose(dwProcess);
    dwProcess = NULL;
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

static qboolean CL_DemoWindowDemoExists(const char* name) {
    int i;

    for (i = 0; i < 2; i++) {
        fileHandle_t f = 0;
        const char* path = i ? va("demos/%s.%s%d", name, DEMOEXT, PROTOCOL_VERSION) : va("demos/%s", name);

        if (FS_FOpenFileRead(path, &f, qtrue) >= 0 && f) {
            FS_FCloseFile(f);
            return qtrue;
        }
    }
    return qfalse;
}

void CL_DemoWindow_f(void) {
    char exe[MAX_OSPATH];
    char log[MAX_OSPATH];
    char sw[16];
    char sh[16];
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    dwRect_t r;
    const char* argv[] = {exe,
                          "+set", "fs_basepath", dwBase,
                          "+set", "fs_homepath", dwHome,
                          "+set", "r_fullscreen", "0",
                          "+set", "r_windowedMode", "-1",
                          "+set", "r_windowedWidth", sw,
                          "+set", "r_windowedHeight", sh,
                          "+set", "r_swapInterval", "0",
                          "+set", "com_maxfps", "60",
                          "+set", "com_maxfpsUnfocused", "0",
                          "+set", "com_maxfpsMinimized", "0",
                          "+set", "com_preciseFrameTiming", "0",
                          "+set", "com_introPlayed", "1",
                          "+set", "com_ignoreCrash", "1",
                          "+set", "s_muteWhenUnfocused", "0",
                          "+set", "s_muteWhenMinimized", "0",
                          "+set", "net_enabled", "0",
                          "+set", "logfile", "0",
                          NULL};

    if (com_demoWindowParent) {
        return;
    }
    if (dwOpen) {
        CL_DemoWindowClose();
        return;
    }
    if (dwProcess) {
        return;
    }
    if (!cls.rendererStarted) {
        return;
    }
    if (strpbrk(name, "\";\r\n") || strstr(name, "..")) {
        Com_Printf("Demo not found: %s\n", name);
        return;
    }
    if (*name && !CL_DemoWindowDemoExists(name)) {
        Com_Printf("Demo not found: %s\n", name);
        return;
    }

    CL_DemoWindowRect(&r);
    dwWantW = MIN(r.w, DW_MAX_W) & ~1;
    dwWantH = MIN((int)((float)dwWantW * r.h / r.w), DW_MAX_H) & ~1;
    Com_sprintf(sw, sizeof(sw), "%i", dwWantW);
    Com_sprintf(sh, sizeof(sh), "%i", dwWantH);
    Com_sprintf(exe, sizeof(exe), "%s%c%s", Sys_BinaryPath(), PATH_SEP, Sys_BinaryName());
    Com_sprintf(log, sizeof(log), "%s", FS_BuildOSPath(dwHome, BASEGAME_DIR, "demowindow.log"));

    dw = (dwShared_t*)Sys_SharedOpen(va("dw%d", Sys_PID()), sizeof(dwShared_t), qtrue);
    if (!dw) {
        Com_Printf(S_COLOR_RED "Could not open the demo window\n");
        return;
    }
    dw->heartbeat = dw->quit = dw->ready = dw->seq = dw->head = dw->tail = 0;
    Q_strncpyz(dw->demo, name, sizeof(dw->demo));

    Sys_SetEnv(DW_ENV, va("%d", Sys_PID()));
    dwProcess = Sys_PipeOpen(argv, log);
    Sys_SetEnv(DW_ENV, "");
    if (!dwProcess) {
        Sys_SharedClose(dw);
        dw = NULL;
        Com_Printf(S_COLOR_RED "Could not open the demo window\n");
        return;
    }

    Key_ClearStates();
    dwOpen = qtrue;
    dwLastSeq = 0;
    dwCloseTime = 0;
}

void CL_DemoWindowInit(void) {
    Q_strncpyz(dwHome, Cvar_VariableString("fs_homepath"), sizeof(dwHome));
    Q_strncpyz(dwBase, Cvar_VariableString("fs_basepath"), sizeof(dwBase));
}

void CL_DemoWindowRendererRestart(void) {
    dwHandleReady = qfalse;
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
    dwOverClose = (x >= r.x + r.w - DW_BAR && x < r.x + r.w && y >= r.y - DW_BAR && y < r.y) ? qtrue : qfalse;
    if ((x != dwMouseX || y != dwMouseY) && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h && dw->width) {
        CL_DemoWindowPush(DW_EV_MOUSE, (x - r.x) * dw->width / r.w, (y - r.y) * dw->height / r.h);
    }
    dwMouseX = x;
    dwMouseY = y;
    return qtrue;
}

void CL_DemoWindowDraw(void) {
    static vec4_t back = {0.0f, 0.0f, 0.0f, 0.9f};
    static vec4_t closeHover = {0.7f, 0.1f, 0.1f, 1.0f};
    static vec4_t text = {1.0f, 1.0f, 1.0f, 1.0f};
    static byte blank[16 * 16 * 4];
    dwRect_t r;

    if (!dwOpen) {
        return;
    }
    CL_DemoWindowRect(&r);
    re.SetColor(back);
    re.DrawStretchPic(r.x - 2, r.y - DW_BAR, r.w + 4, r.h + DW_BAR + 2, 0, 0, 1, 1, cls.whiteShader);
    if (dwOverClose) {
        re.SetColor(closeHover);
        re.DrawStretchPic(r.x + r.w - DW_BAR, r.y - DW_BAR, DW_BAR, DW_BAR, 0, 0, 1, 1, cls.whiteShader);
    }
    re.SetColor(NULL);
    if (dw->seq) {
        int seq = dw->seq;
        int slot = dw->ready;

        if (!dwHandleReady) {
            re.UploadCinematic(16, 16, 16, 16, blank, DW_HANDLE, qtrue);
            dwHandleReady = qtrue;
        }
        re.DrawStretchRaw(r.x, r.y, r.w, r.h, dw->width, dw->height, dw->frames[slot], DW_HANDLE, seq != dwLastSeq);
        dwLastSeq = seq;
    } else {
        SCR_DrawSmallStringExt(r.x + r.w / 2 - 40, r.y + r.h / 2, "Loading...", text, qtrue, qtrue);
    }
    SCR_DrawSmallStringExt(r.x + 6, r.y - DW_BAR + 4, "Demo - ESC to close", text, qtrue, qtrue);
    SCR_DrawSmallStringExt(r.x + r.w - DW_BAR + 8, r.y - DW_BAR + 4, "X", text, qtrue, qtrue);
    re.SetColor(NULL);
}

static void CL_DemoWindowChildOpen(void) {
    int need;

    dw = (dwShared_t*)Sys_SharedOpen(va("dw%d", com_demoWindowParent), sizeof(dwShared_t), qfalse);
    if (!dw) {
        Cbuf_AddText("quit\n");
        return;
    }
    Sys_LowerPriority();
    dwCaptureW = MIN(cls.glconfig.vidWidth, DW_MAX_W);
    dwCaptureH = MIN(cls.glconfig.vidHeight, DW_MAX_H);
    need = (dwCaptureW * 4 + 64) * dwCaptureH + 64;
    dwCapture = Z_Malloc(need);
    dwEncode = Z_Malloc(PAD(dwCaptureW * 3, 4) * dwCaptureH);
    dw->width = dwCaptureW;
    dw->height = dwCaptureH;
    dwChildSeenTime = Sys_Milliseconds();
    if (dw->demo[0]) {
        Cbuf_AddText(va("demo \"%s\"\n", dw->demo));
    } else {
        Cbuf_AddText("ui_replays\n");
    }
}

void CL_DemoWindowFrame(void) {
    if (!com_demoWindowParent) {
        if (dwProcess) {
            dw->heartbeat++;
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

    if (dw->heartbeat != dwChildSeenBeat) {
        dwChildSeenBeat = dw->heartbeat;
        dwChildSeenTime = Sys_Milliseconds();
    }
    if (dw->quit || Sys_Milliseconds() - dwChildSeenTime > DW_PARENT_SILENCE) {
        dw->quit = 1;
        Cbuf_AddText("quit\n");
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

    if (clc.demoplaying) {
        dwChildPlayed = qtrue;
    } else if (dwChildPlayed && clc.state == CA_DISCONNECTED) {
        dwChildPlayed = qfalse;
        Cbuf_AddText("ui_replays\n");
    }

    re.TakeVideoFrame(dwCaptureW, dwCaptureH, dwCapture, dwEncode, qfalse);
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
    return qtrue;
}
