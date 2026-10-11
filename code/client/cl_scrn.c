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

qboolean scr_initialized;

cvar_t* cl_timegraph;
cvar_t* cl_debuggraph;
cvar_t* cl_graphheight;
cvar_t* cl_graphscale;
cvar_t* cl_graphshift;

void SCR_DrawNamedPic(float x, float y, float width, float height, const char* picname) {
    qhandle_t hShader;

    assert(width != 0);

    hShader = re.RegisterShader(picname);
    SCR_AdjustFrom640(&x, &y, &width, &height);
    re.DrawStretchPic(x, y, width, height, 0, 0, 1, 1, hShader);
}

void SCR_AdjustFrom640(float* x, float* y, float* w, float* h) {
    float xscale;
    float yscale;

    xscale = cls.glconfig.vidWidth / (float)SCREEN_WIDTH;
    yscale = cls.glconfig.vidHeight / (float)SCREEN_HEIGHT;
    if (x) {
        *x *= xscale;
    }
    if (y) {
        *y *= yscale;
    }
    if (w) {
        *w *= xscale;
    }
    if (h) {
        *h *= yscale;
    }
}

void SCR_FillRect(float x, float y, float width, float height, const float* color) {
    re.SetColor(color);

    SCR_AdjustFrom640(&x, &y, &width, &height);
    re.DrawStretchPic(x, y, width, height, 0, 0, 0, 0, cls.whiteShader);

    re.SetColor(NULL);
}

void SCR_DrawPic(float x, float y, float width, float height, qhandle_t hShader) {
    SCR_AdjustFrom640(&x, &y, &width, &height);
    re.DrawStretchPic(x, y, width, height, 0, 0, 1, 1, hShader);
}

static void SCR_DrawChar(int x, int y, float size, int ch) {
    int row, col;
    float frow, fcol;
    float ax, ay, aw, ah;

    ch &= 255;

    if (ch == ' ') {
        return;
    }

    if (y < -size) {
        return;
    }

    ax = x;
    ay = y;
    aw = size;
    ah = size;
    SCR_AdjustFrom640(&ax, &ay, &aw, &ah);

    row = ch >> 4;
    col = ch & 15;

    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;

    re.DrawStretchPic(ax, ay, aw, ah,
                      fcol, frow,
                      fcol + size, frow + size,
                      cls.charSetShader);
}

void SCR_DrawSmallChar(int x, int y, int ch) {
    int row, col;
    float frow, fcol;
    float size;

    ch &= 255;

    if (ch == ' ') {
        return;
    }

    if (y < -SMALLCHAR_HEIGHT) {
        return;
    }

    row = ch >> 4;
    col = ch & 15;

    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;

    re.DrawStretchPic(x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT,
                      fcol, frow,
                      fcol + size, frow + size,
                      cls.charSetShader);
}

void SCR_DrawStringExt(int x, int y, float size, const char* string, float* setColor, qboolean forceColor, qboolean noColorEscape) {
    vec4_t color;
    const char* s;
    int xx;

    color[0] = color[1] = color[2] = 0;
    color[3] = setColor[3];
    re.SetColor(color);
    s = string;
    xx = x;
    while (*s) {
        if (!noColorEscape && Q_IsColorString(s)) {
            s += 2;
            continue;
        }
        SCR_DrawChar(xx + 2, y + 2, size, *s);
        xx += size;
        s++;
    }

    s = string;
    xx = x;
    re.SetColor(setColor);
    while (*s) {
        if (Q_IsColorString(s)) {
            if (!forceColor) {
                Com_Memcpy(color, Q_ColorFromCodeIndex(Q_ColorCodeIndex(*(s + 1), qtrue)), sizeof(color));
                color[3] = setColor[3];
                re.SetColor(color);
            }
            if (!noColorEscape) {
                s += 2;
                continue;
            }
        }
        SCR_DrawChar(xx, y, size, *s);
        xx += size;
        s++;
    }
    re.SetColor(NULL);
}

void SCR_DrawBigString(int x, int y, const char* s, float alpha, qboolean noColorEscape) {
    float color[4];

    color[0] = color[1] = color[2] = 1.0;
    color[3] = alpha;
    SCR_DrawStringExt(x, y, BIGCHAR_WIDTH, s, color, qfalse, noColorEscape);
}

void SCR_DrawBigStringColor(int x, int y, const char* s, vec4_t color, qboolean noColorEscape) {
    SCR_DrawStringExt(x, y, BIGCHAR_WIDTH, s, color, qtrue, noColorEscape);
}

void SCR_DrawSmallStringExt(int x, int y, const char* string, float* setColor, qboolean forceColor, qboolean noColorEscape) {
    vec4_t color;
    const char* s;
    int xx;

    s = string;
    xx = x;
    re.SetColor(setColor);
    while (*s) {
        if (Q_IsColorString(s)) {
            if (!forceColor) {
                Com_Memcpy(color, Q_ColorFromCodeIndex(Q_ColorCodeIndex(*(s + 1), qtrue)), sizeof(color));
                color[3] = setColor[3];
                re.SetColor(color);
            }
            if (!noColorEscape) {
                s += 2;
                continue;
            }
        }
        SCR_DrawSmallChar(xx, y, *s);
        xx += SMALLCHAR_WIDTH;
        s++;
    }
    re.SetColor(NULL);
}

static int SCR_Strlen(const char* str) {
    const char* s = str;
    int count = 0;

    while (*s) {
        if (Q_IsColorString(s)) {
            s += 2;
        } else {
            count++;
            s++;
        }
    }

    return count;
}

int SCR_GetBigStringWidth(const char* str) {
    return SCR_Strlen(str) * BIGCHAR_WIDTH;
}

static void SCR_DrawRecordText(float x, float y, float size, const char* text) {
    re.SetColor(colorWhite);
    re.Font_DrawString((int)(x * cls.glconfig.vidWidth / 640.0f), (int)(y * cls.glconfig.vidHeight / 480.0f), text,
                       2, cls.glconfig.vidHeight / 480.0f * size, -1, NULL, 1);
    re.SetColor(NULL);
}

void SCR_DrawDemoRecording(void) {
    char string[1024];
    float size;
    int pos;

    if (!clc.demorecording || !cl_demoRecordMessage->integer) {
        return;
    }

    if (cl_demoRecordMessage->integer == 1) {
        pos = FS_FTell(clc.demofile);
        Com_sprintf(string, sizeof(string), "RECORDING %s: %ik", clc.demoName, pos / 1024);
        SCR_DrawRecordText((float)((80 - (int)strlen(string)) * 4), 420.0f, 8.0f, string);
    } else if (cl_demoRecordMessage->integer == 2) {
        size = (float)((cls.glconfig.vidHeight / 480) * 11);
        re.SetColor(NULL);
        re.DrawStretchPic(cls.glconfig.vidWidth * 0.0015625f, cls.glconfig.vidHeight * 0.979167f, size, size, 0, 0, 1,
                          1, cls.recordShader);
        SCR_DrawRecordText(9.0f, 477.0f, 8.0f, "REC");
    }
}

static int current;
static float values[1024];

void SCR_DebugGraph(float value) {
    values[current] = value;
    current = (current + 1) % ARRAY_LEN(values);
}

void SCR_DrawDebugGraph(void) {
    int a, x, y, w, i, h;
    float v;

    w = cls.glconfig.vidWidth;
    x = 0;
    y = cls.glconfig.vidHeight;
    re.SetColor(g_color_table[0]);
    re.DrawStretchPic(x, y - cl_graphheight->integer,
                      w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader);
    re.SetColor(NULL);

    for (a = 0; a < w; a++) {
        i = (ARRAY_LEN(values) + current - 1 - (a % ARRAY_LEN(values))) % ARRAY_LEN(values);
        v = values[i];
        v = v * cl_graphscale->integer + cl_graphshift->integer;

        if (v < 0)
            v += cl_graphheight->integer * (1 + (int)(-v / cl_graphheight->integer));
        h = (int)v % cl_graphheight->integer;
        re.DrawStretchPic(x + w - 1 - a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader);
    }
}

void SCR_Init(void) {
    cl_timegraph = Cvar_Get("timegraph", "0", CVAR_CHEAT);
    cl_debuggraph = Cvar_Get("debuggraph", "0", CVAR_CHEAT);
    cl_graphheight = Cvar_Get("graphheight", "32", CVAR_CHEAT);
    cl_graphscale = Cvar_Get("graphscale", "1", CVAR_CHEAT);
    cl_graphshift = Cvar_Get("graphshift", "0", CVAR_CHEAT);

    scr_initialized = qtrue;
}

void SCR_DrawScreenField(stereoFrame_t stereoFrame) {
    qboolean uiFullscreen;

    re.BeginFrame(stereoFrame);

    uiFullscreen = (uivm && VM_Call(uivm, UI_IS_FULLSCREEN));

    if (uiFullscreen || clc.state < CA_LOADING) {
        if (cls.glconfig.vidWidth * SCREEN_HEIGHT > cls.glconfig.vidHeight * SCREEN_WIDTH) {
            re.SetColor(g_color_table[0]);
            re.DrawStretchPic(0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader);
            re.SetColor(NULL);
        }
    }

    if (uivm && !uiFullscreen) {
        switch (clc.state) {
            default:
                Com_Error(ERR_FATAL, "SCR_DrawScreenField: bad clc.state");
                break;
            case CA_CINEMATIC:
                SCR_DrawCinematic();
                break;
            case CA_DISCONNECTED:
                S_StopAllSounds();
                VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN);
                break;
            case CA_CONNECTING:
            case CA_CHALLENGING:
            case CA_CONNECTED:
                VM_Call(uivm, UI_REFRESH, cls.realtime);
                VM_Call(uivm, UI_DRAW_CONNECT_SCREEN, qfalse);
                break;
            case CA_LOADING:
            case CA_PRIMED:
                CL_CGameRendering(stereoFrame);

                VM_Call(uivm, UI_REFRESH, cls.realtime);
                VM_Call(uivm, UI_DRAW_CONNECT_SCREEN, qtrue);
                break;
            case CA_ACTIVE:
                CL_CGameRendering(stereoFrame);
                SCR_DrawDemoRecording();
                break;
        }
    }

    if (Key_GetCatcher() & KEYCATCH_UI && uivm) {
        VM_Call(uivm, UI_REFRESH, cls.realtime);
    }

    Con_DrawConsole();

    if (cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer) {
        SCR_DrawDebugGraph();
    }
}

void SCR_UpdateScreen(void) {
    static int recursive;

    if (!scr_initialized) {
        return;
    }

    if (++recursive > 2) {
        Com_Error(ERR_FATAL, "SCR_UpdateScreen: recursively called");
    }
    recursive = 1;

    if (uivm || com_dedicated->integer) {
        int in_anaglyphMode = Cvar_VariableIntegerValue("r_anaglyphMode");
        if (cls.glconfig.stereoEnabled || in_anaglyphMode) {
            SCR_DrawScreenField(STEREO_LEFT);
            SCR_DrawScreenField(STEREO_RIGHT);
        } else {
            SCR_DrawScreenField(STEREO_CENTER);
        }

        if (com_speeds->integer) {
            re.EndFrame(&time_frontend, &time_backend);
        } else {
            re.EndFrame(NULL, NULL);
        }
    }

    recursive = 0;
}
