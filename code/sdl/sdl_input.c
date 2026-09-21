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

#ifdef USE_LOCAL_HEADERS
#include "SDL.h"
#else
#include <SDL.h>
#endif

#ifdef _WIN32
// [ioquakelive] reading the mouse in batches needs the window's HWND
#ifdef USE_LOCAL_HEADERS
#include "SDL_syswm.h"
#else
#include <SDL_syswm.h>
#endif
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../client/client.h"
#include "../sys/sys_local.h"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#define KMOD_SCROLL KMOD_RESERVED
#endif

static cvar_t* in_keyboardDebug = NULL;

static SDL_GameController* gamepad = NULL;
static SDL_Joystick* stick = NULL;

static qboolean mouseAvailable = qfalse;

// [ioquakelive] contadores de /inputrate
static int64_t in_rateUsTotal, in_rateUsMax, in_rateStartUs;
static int in_rateWanted, in_rateFrames, in_rateEvents, in_rateMouse, in_rateMaxFrame;

static qboolean mouseActive = qfalse;

static cvar_t* in_mouse = NULL;
// [ioquakelive] the batched mouse read; off until it is verified on real hardware
static cvar_t* in_rawBatch = NULL;
static cvar_t* in_mouseMode = NULL;
static cvar_t* in_nograb;

static cvar_t* in_joystick = NULL;
static cvar_t* in_joystickThreshold = NULL;
static cvar_t* in_joystickNo = NULL;
static cvar_t* in_joystickUseAnalog = NULL;

static int vidRestartTime = 0;

static int in_eventTime = 0;

static SDL_Window* SDL_window = NULL;

#define CTRL(a) ((a) - 'a' + 1)

/*
===============
IN_PrintKey
===============
*/
static void IN_PrintKey(const SDL_Keysym* keysym, keyNum_t key, qboolean down) {
    if (down)
        Com_Printf("+ ");
    else
        Com_Printf("  ");

    Com_Printf("Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
               keysym->scancode, SDL_GetScancodeName(keysym->scancode),
               keysym->sym, SDL_GetKeyName(keysym->sym));

    if (keysym->mod & KMOD_LSHIFT)
        Com_Printf(" KMOD_LSHIFT");
    if (keysym->mod & KMOD_RSHIFT)
        Com_Printf(" KMOD_RSHIFT");
    if (keysym->mod & KMOD_LCTRL)
        Com_Printf(" KMOD_LCTRL");
    if (keysym->mod & KMOD_RCTRL)
        Com_Printf(" KMOD_RCTRL");
    if (keysym->mod & KMOD_LALT)
        Com_Printf(" KMOD_LALT");
    if (keysym->mod & KMOD_RALT)
        Com_Printf(" KMOD_RALT");
    if (keysym->mod & KMOD_LGUI)
        Com_Printf(" KMOD_LGUI");
    if (keysym->mod & KMOD_RGUI)
        Com_Printf(" KMOD_RGUI");
    if (keysym->mod & KMOD_NUM)
        Com_Printf(" KMOD_NUM");
    if (keysym->mod & KMOD_CAPS)
        Com_Printf(" KMOD_CAPS");
    if (keysym->mod & KMOD_MODE)
        Com_Printf(" KMOD_MODE");
    if (keysym->mod & KMOD_SCROLL)
        Com_Printf(" KMOD_SCROLL");

    Com_Printf(" Q:0x%02x(%s)\n", key, Key_KeynumToString(key));
}

#define MAX_CONSOLE_KEYS 16

/*
===============
IN_IsConsoleKey

TODO: If the SDL_Scancode situation improves, use it instead of
      both of these methods
===============
*/
static qboolean IN_IsConsoleKey(keyNum_t key, int character) {
    typedef struct consoleKey_s {
        enum {
            QUAKE_KEY,
            CHARACTER
        } type;

        union {
            keyNum_t key;
            int character;
        } u;
    } consoleKey_t;

    static consoleKey_t consoleKeys[MAX_CONSOLE_KEYS];
    static int numConsoleKeys = 0;
    int i;

    // Only parse the variable when it changes
    if (cl_consoleKeys->modified) {
        char *text_p, *token;

        cl_consoleKeys->modified = qfalse;
        text_p = cl_consoleKeys->string;
        numConsoleKeys = 0;

        while (numConsoleKeys < MAX_CONSOLE_KEYS) {
            consoleKey_t* c = &consoleKeys[numConsoleKeys];
            int charCode = 0;

            token = COM_Parse(&text_p);
            if (!token[0])
                break;

            charCode = Com_HexStrToInt(token);

            if (charCode > 0) {
                c->type = CHARACTER;
                c->u.character = charCode;
            } else {
                c->type = QUAKE_KEY;
                c->u.key = Key_StringToKeynum(token);

                // 0 isn't a key
                if (c->u.key <= 0)
                    continue;
            }

            numConsoleKeys++;
        }
    }

    // If the character is the same as the key, prefer the character
    if (key == character)
        key = 0;

    for (i = 0; i < numConsoleKeys; i++) {
        consoleKey_t* c = &consoleKeys[i];

        switch (c->type) {
            case QUAKE_KEY:
                if (key && c->u.key == key)
                    return qtrue;
                break;

            case CHARACTER:
                if (c->u.character == character)
                    return qtrue;
                break;
        }
    }

    return qfalse;
}

/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static keyNum_t IN_TranslateSDLToQ3Key(SDL_Keysym* keysym, qboolean down) {
    keyNum_t key = 0;

    if (keysym->scancode >= SDL_SCANCODE_1 && keysym->scancode <= SDL_SCANCODE_0) {
        // Always map the number keys as such even if they actually map
        // to other characters (eg, "1" is "&" on an AZERTY keyboard).
        // This is required for SDL before 2.0.6, except on Windows
        // which already had this behavior.
        if (keysym->scancode == SDL_SCANCODE_0)
            key = '0';
        else
            key = '1' + keysym->scancode - SDL_SCANCODE_1;
    } else if (keysym->sym >= SDLK_SPACE && keysym->sym < SDLK_DELETE) {
        // These happen to match the ASCII chars
        key = (int)keysym->sym;
    } else {
        switch (keysym->sym) {
            case SDLK_PAGEUP:
                key = K_PGUP;
                break;
            case SDLK_KP_9:
                key = K_KP_PGUP;
                break;
            case SDLK_PAGEDOWN:
                key = K_PGDN;
                break;
            case SDLK_KP_3:
                key = K_KP_PGDN;
                break;
            case SDLK_KP_7:
                key = K_KP_HOME;
                break;
            case SDLK_HOME:
                key = K_HOME;
                break;
            case SDLK_KP_1:
                key = K_KP_END;
                break;
            case SDLK_END:
                key = K_END;
                break;
            case SDLK_KP_4:
                key = K_KP_LEFTARROW;
                break;
            case SDLK_LEFT:
                key = K_LEFTARROW;
                break;
            case SDLK_KP_6:
                key = K_KP_RIGHTARROW;
                break;
            case SDLK_RIGHT:
                key = K_RIGHTARROW;
                break;
            case SDLK_KP_2:
                key = K_KP_DOWNARROW;
                break;
            case SDLK_DOWN:
                key = K_DOWNARROW;
                break;
            case SDLK_KP_8:
                key = K_KP_UPARROW;
                break;
            case SDLK_UP:
                key = K_UPARROW;
                break;
            case SDLK_ESCAPE:
                key = K_ESCAPE;
                break;
            case SDLK_KP_ENTER:
                key = K_KP_ENTER;
                break;
            case SDLK_RETURN:
                key = K_ENTER;
                break;
            case SDLK_TAB:
                key = K_TAB;
                break;
            case SDLK_F1:
                key = K_F1;
                break;
            case SDLK_F2:
                key = K_F2;
                break;
            case SDLK_F3:
                key = K_F3;
                break;
            case SDLK_F4:
                key = K_F4;
                break;
            case SDLK_F5:
                key = K_F5;
                break;
            case SDLK_F6:
                key = K_F6;
                break;
            case SDLK_F7:
                key = K_F7;
                break;
            case SDLK_F8:
                key = K_F8;
                break;
            case SDLK_F9:
                key = K_F9;
                break;
            case SDLK_F10:
                key = K_F10;
                break;
            case SDLK_F11:
                key = K_F11;
                break;
            case SDLK_F12:
                key = K_F12;
                break;
            case SDLK_F13:
                key = K_F13;
                break;
            case SDLK_F14:
                key = K_F14;
                break;
            case SDLK_F15:
                key = K_F15;
                break;

            case SDLK_BACKSPACE:
                key = K_BACKSPACE;
                break;
            case SDLK_KP_PERIOD:
                key = K_KP_DEL;
                break;
            case SDLK_DELETE:
                key = K_DEL;
                break;
            case SDLK_PAUSE:
                key = K_PAUSE;
                break;

            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
                key = K_SHIFT;
                break;

            case SDLK_LCTRL:
            case SDLK_RCTRL:
                key = K_CTRL;
                break;

#ifdef __APPLE__
            case SDLK_RGUI:
            case SDLK_LGUI:
                key = K_COMMAND;
                break;
#else
            case SDLK_RGUI:
            case SDLK_LGUI:
                key = K_SUPER;
                break;
#endif

            case SDLK_RALT:
            case SDLK_LALT:
                key = K_ALT;
                break;

            case SDLK_KP_5:
                key = K_KP_5;
                break;
            case SDLK_INSERT:
                key = K_INS;
                break;
            case SDLK_KP_0:
                key = K_KP_INS;
                break;
            case SDLK_KP_MULTIPLY:
                key = K_KP_STAR;
                break;
            case SDLK_KP_PLUS:
                key = K_KP_PLUS;
                break;
            case SDLK_KP_MINUS:
                key = K_KP_MINUS;
                break;
            case SDLK_KP_DIVIDE:
                key = K_KP_SLASH;
                break;

            case SDLK_MODE:
                key = K_MODE;
                break;
            case SDLK_HELP:
                key = K_HELP;
                break;
            case SDLK_PRINTSCREEN:
                key = K_PRINT;
                break;
            case SDLK_SYSREQ:
                key = K_SYSREQ;
                break;
            case SDLK_MENU:
                key = K_MENU;
                break;
            case SDLK_APPLICATION:
                key = K_MENU;
                break;
            case SDLK_POWER:
                key = K_POWER;
                break;
            case SDLK_UNDO:
                key = K_UNDO;
                break;
            case SDLK_SCROLLLOCK:
                key = K_SCROLLOCK;
                break;
            case SDLK_NUMLOCKCLEAR:
                key = K_KP_NUMLOCK;
                break;
            case SDLK_CAPSLOCK:
                key = K_CAPSLOCK;
                break;

            default:
                if (!(keysym->sym & SDLK_SCANCODE_MASK) && keysym->scancode <= 95) {
                    // Map Unicode characters to 95 world keys using the key's scan code.
                    // FIXME: There aren't enough world keys to cover all the scancodes.
                    // Maybe create a map of scancode to quake key at start up and on
                    // key map change; allocate world key numbers as needed similar
                    // to SDL 1.2.
                    key = K_WORLD_0 + (int)keysym->scancode;
                }
                break;
        }
    }

    if (in_keyboardDebug->integer)
        IN_PrintKey(keysym, key, down);

    // [ioquakelive] The key under Escape opens the console whatever it prints. cl_consoleKeys
    // matches by the character produced, and on a Latin American or Spanish layout that key
    // is not ` or ~, which left those keyboards with no console at all. SDL reports the same
    // scancode for that physical key on every layout, so that is what decides here; the cvar
    // still works, for anyone who wants a different key as well.
    if (keysym->scancode == SDL_SCANCODE_GRAVE || IN_IsConsoleKey(key, 0)) {
        // Console keys can't be bound or generate characters
        key = K_CONSOLE;
    }

    return key;
}

/*
===============
IN_GobbleMotionEvents
===============
*/
static void IN_GobbleMotionEvents(void) {
    SDL_Event dummy[1];
    int val = 0;

    // Gobble any mouse motion events
    SDL_PumpEvents();
    while ((val = SDL_PeepEvents(dummy, 1, SDL_GETEVENT,
                                 SDL_MOUSEMOTION, SDL_MOUSEMOTION)) > 0) {
    }

    if (val < 0)
        Com_Printf("IN_GobbleMotionEvents failed: %s\n", SDL_GetError());
}


#ifdef _WIN32
/*
===============================================================================

THE MOUSE, READ IN BATCHES   (in_mouse 2, Windows)

[ioquakelive] SDL2 services raw input one event at a time: for every movement it calls
GetRawInputData, which takes a lock, and at 8000 Hz that is eight calls per
millisecond. The batched read, GetRawInputBuffer, arrived in SDL 3, and the retail
engine does not have it either.

Here the device is registered against our own window, SDL's relative mode is turned off
- without it SDL does not even look at WM_INPUT - and once per frame the whole queue is
drained and summed into a single event. Reading before SDL pumps the messages matters:
once a WM_INPUT reaches DefWindowProc the system frees its data.

===============================================================================
*/

// NEXTRAWINPUTBLOCK, the system macro, uses QWORD, which is not always declared and breaks the
// x64 build. The step is the same: the next block, aligned to a pointer's size (4 on 32 bits,
// 8 on 64).
#define IOQL_RAWINPUT_ALIGN(x) (((x) + sizeof(void*) - 1) & ~(ULONG_PTR)(sizeof(void*) - 1))
#define IOQL_NEXT_RAWINPUT(p) ((PRAWINPUT)IOQL_RAWINPUT_ALIGN((ULONG_PTR)((PBYTE)(p) + (p)->header.dwSize)))

static qboolean rawMouseOn;
static BYTE* rawMouseBuffer;
static UINT rawMouseBufferSize;
static int rawMouseLastAbsX, rawMouseLastAbsY;
static qboolean rawMouseHasAbs;
static qboolean rawMouseAnnounced;
// [ioquakelive] the safety net: how many events the batch delivered and how many SDL saw
static int rawMouseSeen;
static int rawMouseSdlMotion;

static qboolean IN_RawMouseStart(void) {
    RAWINPUTDEVICE device;
    SDL_SysWMinfo info;

    if (rawMouseOn) {
        return qtrue;
    }
    SDL_VERSION(&info.version);
    if (!SDL_window || !SDL_GetWindowWMInfo(SDL_window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        return qfalse;
    }
    if (!rawMouseBuffer) {
        // room to spare for what arrives between two frames at 8000 Hz
        rawMouseBufferSize = sizeof(RAWINPUT) * 512;
        rawMouseBuffer = (BYTE*)Z_Malloc(rawMouseBufferSize);
    }

    device.usUsagePage = 0x01;  // generic desktop
    device.usUsage = 0x02;      // mouse
    device.dwFlags = 0;         // only while focused, and without taking the normal messages away
    device.hwndTarget = info.info.win.window;

    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        Com_DPrintf("RegisterRawInputDevices failed, falling back to SDL's mouse\n");
        return qfalse;
    }
    rawMouseHasAbs = qfalse;
    rawMouseSeen = 0;
    rawMouseSdlMotion = 0;
    rawMouseOn = qtrue;
    if (!rawMouseAnnounced) {
        rawMouseAnnounced = qtrue;
        Com_Printf("Mouse: raw input read in batches (GetRawInputBuffer)\n");
    }
    return qtrue;
}

static void IN_RawMouseStop(void) {
    RAWINPUTDEVICE device;

    if (!rawMouseOn) {
        return;
    }
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = NULL;
    RegisterRawInputDevices(&device, 1, sizeof(device));
    rawMouseOn = qfalse;
}

// drains the queue and posts a single movement with the sum of everything that arrived
static void IN_RawMouseRead(void) {
    int dx = 0, dy = 0;

    if (!rawMouseOn) {
        return;
    }
    for (;;) {
        UINT bytes = rawMouseBufferSize;
        UINT count = GetRawInputBuffer((PRAWINPUT)rawMouseBuffer, &bytes, sizeof(RAWINPUTHEADER));
        PRAWINPUT raw = (PRAWINPUT)rawMouseBuffer;
        UINT i;

        if (count == 0 || count == (UINT)-1) {
            break;
        }
        for (i = 0; i < count; i++) {
            if (raw->header.dwType == RIM_TYPEMOUSE) {
                const RAWMOUSE* m = &raw->data.mouse;

                if (m->usFlags & MOUSE_MOVE_ABSOLUTE) {
                    // escritorio remoto, tabletas y mouses virtuales mandan posicion, no delta
                    if (rawMouseHasAbs) {
                        dx += m->lLastX - rawMouseLastAbsX;
                        dy += m->lLastY - rawMouseLastAbsY;
                    }
                    rawMouseLastAbsX = m->lLastX;
                    rawMouseLastAbsY = m->lLastY;
                    rawMouseHasAbs = qtrue;
                } else {
                    dx += m->lLastX;
                    dy += m->lLastY;
                }
                in_rateMouse += (in_rateWanted != 0);
                rawMouseSeen++;
            }
            raw = IOQL_NEXT_RAWINPUT(raw);
        }
    }
    if (dx || dy) {
        Com_QueueEvent(in_eventTime, SE_MOUSE, dx, dy, 0, NULL);
    }

    // SDL still sees the system pointer even when we ignore it. If it sees movement and the
    // batch delivered nothing, this path does not work on this machine: it falls back to SDL's
    // and says so, so that nobody is left unable to look around.
    if (rawMouseSdlMotion > 30 && rawMouseSeen == 0) {
        Com_Printf("^3Mouse: batched raw input delivered nothing, falling back to SDL\n");
        IN_RawMouseStop();
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(SDL_window, SDL_TRUE);
    }
}
#endif

/*
===============
IN_GrabMouse

[ioquakelive] Grabbing the mouse to play: through the batched path when it works, and through
SDL's relative mode otherwise, which is what was here before.
===============
*/
static void IN_GrabMouse(void) {
#ifdef _WIN32
    // [ioquakelive] opt in: on real hardware the view did not move through this path, and the
    // safety net did not catch it (see the in_rawBatch comment in IN_Init)
    if (in_mouse->integer == 2 && in_rawBatch && in_rawBatch->integer && IN_RawMouseStart()) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_FALSE);
        SDL_SetWindowGrab(SDL_window, SDL_TRUE);
        return;
    }
#endif
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_SetWindowGrab(SDL_window, SDL_TRUE);
}

static void IN_ReleaseMouse(void) {
#ifdef _WIN32
    IN_RawMouseStop();
#endif
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowGrab(SDL_window, SDL_FALSE);
}

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse(qboolean isFullscreen) {
    if (!mouseAvailable || !SDL_WasInit(SDL_INIT_VIDEO))
        return;

    if (!mouseActive) {
        IN_GrabMouse();

        IN_GobbleMotionEvents();
    }

    // in_nograb makes no sense in fullscreen mode
    if (!isFullscreen) {
        if (in_nograb->modified || !mouseActive) {
            if (in_nograb->integer) {
                IN_ReleaseMouse();
            } else {
                IN_GrabMouse();
            }

            in_nograb->modified = qfalse;
        }
    }

    mouseActive = qtrue;
}

/*
===============
IN_DeactivateMouse
===============
*/
static void IN_DeactivateMouse(qboolean isFullscreen) {
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return;

    // Always show the cursor when the mouse is disabled,
    // but not when fullscreen
    if (!isFullscreen)
        SDL_ShowCursor(SDL_TRUE);

    if (!mouseAvailable)
        return;

    if (mouseActive) {
        IN_GobbleMotionEvents();

        IN_ReleaseMouse();

        // Don't warp the mouse unless the cursor is within the window
        if (SDL_GetWindowFlags(SDL_window) & SDL_WINDOW_MOUSE_FOCUS)
            SDL_WarpMouseInWindow(SDL_window, cls.glconfig.vidWidth / 2, cls.glconfig.vidHeight / 2);

        mouseActive = qfalse;
    }
}

// We translate axes movement into keypresses
static int joy_keys[16] = {
    K_LEFTARROW, K_RIGHTARROW,
    K_UPARROW, K_DOWNARROW,
    K_JOY17, K_JOY18,
    K_JOY19, K_JOY20,
    K_JOY21, K_JOY22,
    K_JOY23, K_JOY24,
    K_JOY25, K_JOY26,
    K_JOY27, K_JOY28};

// translate hat events into keypresses
// the 4 highest buttons are used for the first hat ...
static int hat_keys[16] = {
    K_JOY29, K_JOY30,
    K_JOY31, K_JOY32,
    K_JOY25, K_JOY26,
    K_JOY27, K_JOY28,
    K_JOY21, K_JOY22,
    K_JOY23, K_JOY24,
    K_JOY17, K_JOY18,
    K_JOY19, K_JOY20};

struct
{
    qboolean buttons[SDL_CONTROLLER_BUTTON_MAX + 1];  // +1 because old max was 16, current SDL_CONTROLLER_BUTTON_MAX is 15
    unsigned int oldaxes;
    int oldaaxes[MAX_JOYSTICK_AXIS];
    unsigned int oldhats;
} stick_state;

/*
===============
IN_InitJoystick
===============
*/
static void IN_InitJoystick(void) {
    int i = 0;
    int total = 0;
    char buf[16384] = "";

    if (gamepad)
        SDL_GameControllerClose(gamepad);

    if (stick != NULL)
        SDL_JoystickClose(stick);

    stick = NULL;
    gamepad = NULL;
    memset(&stick_state, '\0', sizeof(stick_state));

    // [QL] quakelive_steam.exe 0x4eacd0 clears ui_joyavail before looking for a
    // device and sets it once one is open (0x4eae66); the settings page shows its
    // Gamepad section only while it is above 0
    Cvar_Set("ui_joyavail", "0");

    // SDL 2.0.4 requires SDL_INIT_JOYSTICK to be initialized separately from
    // SDL_INIT_GAMECONTROLLER for SDL_JoystickOpen() to work correctly,
    // despite https://wiki.libsdl.org/SDL_Init (retrieved 2016-08-16)
    // indicating SDL_INIT_JOYSTICK should be initialized automatically.
    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
        if (SDL_Init(SDL_INIT_JOYSTICK) != 0) {
            Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
            return;
        }
        Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
    }

    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
        Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMECONTROLLER)...\n");
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
            Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
            return;
        }
        Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) passed.\n");
    }

    total = SDL_NumJoysticks();
    Com_DPrintf("%d possible joysticks\n", total);

    // Print list and build cvar to allow ui to select joystick.
    for (i = 0; i < total; i++) {
        Q_strcat(buf, sizeof(buf), SDL_JoystickNameForIndex(i));
        Q_strcat(buf, sizeof(buf), "\n");
    }

    Cvar_Get("in_availableJoysticks", "", CVAR_ROM);

    // Update cvar on in_restart or controller add/remove.
    Cvar_Set("in_availableJoysticks", buf);

    if (!in_joystick->integer) {
        Com_DPrintf("Joystick is not active.\n");
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        return;
    }

    in_joystickNo = Cvar_Get("in_joystickNo", "0", CVAR_ARCHIVE);
    if (in_joystickNo->integer < 0 || in_joystickNo->integer >= total)
        Cvar_Set("in_joystickNo", "0");

    in_joystickUseAnalog = Cvar_Get("in_joystickUseAnalog", "0", CVAR_ARCHIVE);

    stick = SDL_JoystickOpen(in_joystickNo->integer);

    if (stick == NULL) {
        Com_DPrintf("No joystick opened: %s\n", SDL_GetError());
        return;
    }

    if (SDL_IsGameController(in_joystickNo->integer))
        gamepad = SDL_GameControllerOpen(in_joystickNo->integer);

    Cvar_Set("ui_joyavail", "1");
    Com_DPrintf("Joystick %d opened\n", in_joystickNo->integer);
    Com_DPrintf("Name:       %s\n", SDL_JoystickNameForIndex(in_joystickNo->integer));
    Com_DPrintf("Axes:       %d\n", SDL_JoystickNumAxes(stick));
    Com_DPrintf("Hats:       %d\n", SDL_JoystickNumHats(stick));
    Com_DPrintf("Buttons:    %d\n", SDL_JoystickNumButtons(stick));
    Com_DPrintf("Balls:      %d\n", SDL_JoystickNumBalls(stick));
    Com_DPrintf("Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No");
    Com_DPrintf("Is gamepad: %s\n", gamepad ? "Yes" : "No");

    SDL_JoystickEventState(SDL_QUERY);
    SDL_GameControllerEventState(SDL_QUERY);
}

/*
===============
IN_ShutdownJoystick
===============
*/
static void IN_ShutdownJoystick(void) {
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
        return;

    if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        return;

    if (gamepad) {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
    }

    if (stick) {
        SDL_JoystickClose(stick);
        stick = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static qboolean KeyToAxisAndSign(int keynum, int* outAxis, int* outSign) {
    char* bind;

    if (!keynum)
        return qfalse;

    bind = Key_GetBinding(keynum);

    if (!bind || *bind != '+')
        return qfalse;

    *outSign = 0;

    if (Q_stricmp(bind, "+forward") == 0) {
        *outAxis = j_forward_axis->integer;
        *outSign = j_forward->value > 0.0f ? 1 : -1;
    } else if (Q_stricmp(bind, "+back") == 0) {
        *outAxis = j_forward_axis->integer;
        *outSign = j_forward->value > 0.0f ? -1 : 1;
    } else if (Q_stricmp(bind, "+moveleft") == 0) {
        *outAxis = j_side_axis->integer;
        *outSign = j_side->value > 0.0f ? -1 : 1;
    } else if (Q_stricmp(bind, "+moveright") == 0) {
        *outAxis = j_side_axis->integer;
        *outSign = j_side->value > 0.0f ? 1 : -1;
    } else if (Q_stricmp(bind, "+lookup") == 0) {
        *outAxis = j_pitch_axis->integer;
        *outSign = j_pitch->value > 0.0f ? -1 : 1;
    } else if (Q_stricmp(bind, "+lookdown") == 0) {
        *outAxis = j_pitch_axis->integer;
        *outSign = j_pitch->value > 0.0f ? 1 : -1;
    } else if (Q_stricmp(bind, "+left") == 0) {
        *outAxis = j_yaw_axis->integer;
        *outSign = j_yaw->value > 0.0f ? 1 : -1;
    } else if (Q_stricmp(bind, "+right") == 0) {
        *outAxis = j_yaw_axis->integer;
        *outSign = j_yaw->value > 0.0f ? -1 : 1;
    } else if (Q_stricmp(bind, "+moveup") == 0) {
        *outAxis = j_up_axis->integer;
        *outSign = j_up->value > 0.0f ? 1 : -1;
    } else if (Q_stricmp(bind, "+movedown") == 0) {
        *outAxis = j_up_axis->integer;
        *outSign = j_up->value > 0.0f ? -1 : 1;
    }

    return *outSign != 0;
}

/*
===============
IN_GamepadMove
===============
*/
static void IN_GamepadMove(void) {
    int i;
    int translatedAxes[MAX_JOYSTICK_AXIS];
    qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

    SDL_GameControllerUpdate();

    // check buttons
    for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
        qboolean pressed = SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_A + i);
        if (pressed != stick_state.buttons[i]) {
#if SDL_VERSION_ATLEAST(2, 0, 14)
            if (i >= SDL_CONTROLLER_BUTTON_MISC1) {
                Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_CONTROLLER_BUTTON_MISC1, pressed, 0, NULL);
            } else
#endif
            {
                Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
            }
            stick_state.buttons[i] = pressed;
        }
    }

    // must defer translated axes until all real axes are processed
    // must be done this way to prevent a later mapped axis from zeroing out a previous one
    if (in_joystickUseAnalog->integer) {
        for (i = 0; i < MAX_JOYSTICK_AXIS; i++) {
            translatedAxes[i] = 0;
            translatedAxesSet[i] = qfalse;
        }
    }

    // check axes
    for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        int axis = SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX + i);
        int oldAxis = stick_state.oldaaxes[i];

        // Smoothly ramp from dead zone to maximum value
        float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

        if (f < 0.0f)
            f = 0.0f;

        axis = (int)(32767 * ((axis < 0) ? -f : f));

        if (axis != oldAxis) {
            const int negMap[SDL_CONTROLLER_AXIS_MAX] = {K_PAD0_LEFTSTICK_LEFT, K_PAD0_LEFTSTICK_UP, K_PAD0_RIGHTSTICK_LEFT, K_PAD0_RIGHTSTICK_UP, 0, 0};
            const int posMap[SDL_CONTROLLER_AXIS_MAX] = {K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER};

            qboolean posAnalog = qfalse, negAnalog = qfalse;
            int negKey = negMap[i];
            int posKey = posMap[i];

            if (in_joystickUseAnalog->integer) {
                int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

                // get axes and axes signs for keys if available
                posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
                negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

                // positive to negative/neutral -> keyup if axis hasn't yet been set
                if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0) {
                    translatedAxes[posAxis] = 0;
                    translatedAxesSet[posAxis] = qtrue;
                }

                // negative to positive/neutral -> keyup if axis hasn't yet been set
                if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0) {
                    translatedAxes[negAxis] = 0;
                    translatedAxesSet[negAxis] = qtrue;
                }

                // negative/neutral to positive -> keydown
                if (posAnalog && axis > 0) {
                    translatedAxes[posAxis] = axis * posSign;
                    translatedAxesSet[posAxis] = qtrue;
                }

                // positive/neutral to negative -> keydown
                if (negAnalog && axis < 0) {
                    translatedAxes[negAxis] = -axis * negSign;
                    translatedAxesSet[negAxis] = qtrue;
                }
            }

            // keyups first so they get overridden by keydowns later

            // positive to negative/neutral -> keyup
            if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
                Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

            // negative to positive/neutral -> keyup
            if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
                Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

            // negative/neutral to positive -> keydown
            if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
                Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

            // positive/neutral to negative -> keydown
            if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
                Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

            stick_state.oldaaxes[i] = axis;
        }
    }

    // set translated axes
    if (in_joystickUseAnalog->integer) {
        for (i = 0; i < MAX_JOYSTICK_AXIS; i++) {
            if (translatedAxesSet[i])
                Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
        }
    }
}

/*
===============
IN_JoyMove
===============
*/
static void IN_JoyMove(void) {
    unsigned int axes = 0;
    unsigned int hats = 0;
    int total = 0;
    int i = 0;

    if (gamepad) {
        IN_GamepadMove();
        return;
    }

    if (!stick)
        return;

    SDL_JoystickUpdate();

    // update the ball state.
    total = SDL_JoystickNumBalls(stick);
    if (total > 0) {
        int balldx = 0;
        int balldy = 0;
        for (i = 0; i < total; i++) {
            int dx = 0;
            int dy = 0;
            SDL_JoystickGetBall(stick, i, &dx, &dy);
            balldx += dx;
            balldy += dy;
        }
        if (balldx || balldy) {
            // !!! FIXME: is this good for stick balls, or just mice?
            // Scale like the mouse input...
            if (abs(balldx) > 1)
                balldx *= 2;
            if (abs(balldy) > 1)
                balldy *= 2;
            Com_QueueEvent(in_eventTime, SE_MOUSE, balldx, balldy, 0, NULL);
        }
    }

    // now query the stick buttons...
    total = SDL_JoystickNumButtons(stick);
    if (total > 0) {
        if (total > ARRAY_LEN(stick_state.buttons))
            total = ARRAY_LEN(stick_state.buttons);
        for (i = 0; i < total; i++) {
            qboolean pressed = (SDL_JoystickGetButton(stick, i) != 0);
            if (pressed != stick_state.buttons[i]) {
                Com_QueueEvent(in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL);
                stick_state.buttons[i] = pressed;
            }
        }
    }

    // look at the hats...
    total = SDL_JoystickNumHats(stick);
    if (total > 0) {
        if (total > 4)
            total = 4;
        for (i = 0; i < total; i++) {
            ((Uint8*)&hats)[i] = SDL_JoystickGetHat(stick, i);
        }
    }

    // update hat state
    if (hats != stick_state.oldhats) {
        for (i = 0; i < 4; i++) {
            if (((Uint8*)&hats)[i] != ((Uint8*)&stick_state.oldhats)[i]) {
                // release event
                switch (((Uint8*)&stick_state.oldhats)[i]) {
                    case SDL_HAT_UP:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 0], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_RIGHT:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 1], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_DOWN:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 2], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_LEFT:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 3], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_RIGHTUP:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 0], qfalse, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 1], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_RIGHTDOWN:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 2], qfalse, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 1], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_LEFTUP:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 0], qfalse, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 3], qfalse, 0, NULL);
                        break;
                    case SDL_HAT_LEFTDOWN:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 2], qfalse, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 3], qfalse, 0, NULL);
                        break;
                    default:
                        break;
                }
                // press event
                switch (((Uint8*)&hats)[i]) {
                    case SDL_HAT_UP:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 0], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_RIGHT:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 1], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_DOWN:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 2], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_LEFT:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 3], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_RIGHTUP:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 0], qtrue, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 1], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_RIGHTDOWN:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 2], qtrue, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 1], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_LEFTUP:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 0], qtrue, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 3], qtrue, 0, NULL);
                        break;
                    case SDL_HAT_LEFTDOWN:
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 2], qtrue, 0, NULL);
                        Com_QueueEvent(in_eventTime, SE_KEY, hat_keys[4 * i + 3], qtrue, 0, NULL);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // save hat state
    stick_state.oldhats = hats;

    // finally, look at the axes...
    total = SDL_JoystickNumAxes(stick);
    if (total > 0) {
        if (in_joystickUseAnalog->integer) {
            if (total > MAX_JOYSTICK_AXIS)
                total = MAX_JOYSTICK_AXIS;
            for (i = 0; i < total; i++) {
                Sint16 axis = SDL_JoystickGetAxis(stick, i);
                float f = ((float)abs(axis)) / 32767.0f;

                if (f < in_joystickThreshold->value)
                    axis = 0;

                if (axis != stick_state.oldaaxes[i]) {
                    Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, axis, 0, NULL);
                    stick_state.oldaaxes[i] = axis;
                }
            }
        } else {
            if (total > 16)
                total = 16;
            for (i = 0; i < total; i++) {
                Sint16 axis = SDL_JoystickGetAxis(stick, i);
                float f = ((float)axis) / 32767.0f;
                if (f < -in_joystickThreshold->value) {
                    axes |= (1 << (i * 2));
                } else if (f > in_joystickThreshold->value) {
                    axes |= (1 << ((i * 2) + 1));
                }
            }
        }
    }

    /* Time to update axes state based on old vs. new. */
    if (axes != stick_state.oldaxes) {
        for (i = 0; i < 16; i++) {
            if ((axes & (1 << i)) && !(stick_state.oldaxes & (1 << i))) {
                Com_QueueEvent(in_eventTime, SE_KEY, joy_keys[i], qtrue, 0, NULL);
            }

            if (!(axes & (1 << i)) && (stick_state.oldaxes & (1 << i))) {
                Com_QueueEvent(in_eventTime, SE_KEY, joy_keys[i], qfalse, 0, NULL);
            }
        }
    }

    /* Save for future generations. */
    stick_state.oldaxes = axes;
}

/*
===============
IN_ProcessEvents
===============
*/
/*
===============
IN_InputRate_f

[ioquakelive] /inputrate [n]: measures n input frames and reports the mouse's real rate, how
many events arrive per frame and what servicing them costs. It answers whether a high rate
mouse -- 4000 or 8000 Hz -- is costing this machine frames, without guessing.
===============
*/

static void IN_InputRateReport(void) {
    float seconds = (Sys_Microseconds() - in_rateStartUs) / 1000000.0f;

    if (seconds <= 0.0f) {
        seconds = 0.000001f;
    }
    Com_Printf("input: %i frames in %.2f s\n", in_rateFrames, seconds);
    Com_Printf("  mouse moves %i (%.0f per second)   all events %i\n", in_rateMouse,
               in_rateMouse / seconds, in_rateEvents);
    Com_Printf("  per frame: %.1f average, %i worst\n", (float)in_rateEvents / in_rateFrames,
               in_rateMaxFrame);
    Com_Printf("  cost per frame: %.3f ms average, %.3f ms worst\n",
               (float)in_rateUsTotal / in_rateFrames / 1000.0f, in_rateUsMax / 1000.0f);
}

static void IN_InputRate_f(void) {
    int wanted = 600;

    if (Cmd_Argc() > 1) {
        wanted = atoi(Cmd_Argv(1));
    }
    if (wanted < 10) {
        wanted = 10;
    }
    in_rateWanted = wanted;
    in_rateFrames = 0;
    in_rateEvents = 0;
    in_rateMouse = 0;
    in_rateMaxFrame = 0;
    in_rateUsTotal = 0;
    in_rateUsMax = 0;
    in_rateStartUs = Sys_Microseconds();
    Com_Printf("input: measuring %i frames...\n", wanted);
}

static void IN_ProcessEvents(void) {
    SDL_Event e;
    keyNum_t key = 0;
    static keyNum_t lastKeyDown = 0;

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return;

    while (SDL_PollEvent(&e)) {
        // [ioquakelive] for /inputrate: how many events come in and how many are the mouse's
        if (in_rateWanted) {
            in_rateEvents++;
#ifdef _WIN32
            if (e.type == SDL_MOUSEMOTION && !rawMouseOn) {
#else
            if (e.type == SDL_MOUSEMOTION) {
#endif
                in_rateMouse++;
            }
        }
        switch (e.type) {
            case SDL_KEYDOWN:
                if (e.key.repeat && Key_GetCatcher() == 0)
                    break;

                if ((key = IN_TranslateSDLToQ3Key(&e.key.keysym, qtrue)))
                    Com_QueueEvent(in_eventTime, SE_KEY, key, qtrue, 0, NULL);

                if (key == K_BACKSPACE)
                    Com_QueueEvent(in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL);
                else if (keys[K_CTRL].down && key >= 'a' && key <= 'z')
                    Com_QueueEvent(in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL);

                lastKeyDown = key;
                break;

            case SDL_KEYUP:
                if ((key = IN_TranslateSDLToQ3Key(&e.key.keysym, qfalse)))
                    Com_QueueEvent(in_eventTime, SE_KEY, key, qfalse, 0, NULL);

                lastKeyDown = 0;
                break;

            case SDL_TEXTINPUT:
                if (lastKeyDown != K_CONSOLE) {
                    char* c = e.text.text;

                    // Quick and dirty UTF-8 to UTF-32 conversion
                    while (*c) {
                        int utf32 = 0;

                        if ((*c & 0x80) == 0)
                            utf32 = *c++;
                        else if ((*c & 0xE0) == 0xC0)  // 110x xxxx
                        {
                            utf32 |= (*c++ & 0x1F) << 6;
                            utf32 |= (*c++ & 0x3F);
                        } else if ((*c & 0xF0) == 0xE0)  // 1110 xxxx
                        {
                            utf32 |= (*c++ & 0x0F) << 12;
                            utf32 |= (*c++ & 0x3F) << 6;
                            utf32 |= (*c++ & 0x3F);
                        } else if ((*c & 0xF8) == 0xF0)  // 1111 0xxx
                        {
                            utf32 |= (*c++ & 0x07) << 18;
                            utf32 |= (*c++ & 0x3F) << 12;
                            utf32 |= (*c++ & 0x3F) << 6;
                            utf32 |= (*c++ & 0x3F);
                        } else {
                            Com_DPrintf("Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c);
                            c++;
                        }

                        if (utf32 != 0) {
                            if (IN_IsConsoleKey(0, utf32)) {
                                Com_QueueEvent(in_eventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL);
                                Com_QueueEvent(in_eventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL);
                            } else
                                Com_QueueEvent(in_eventTime, SE_CHAR, utf32, 0, 0, NULL);
                        }
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                // [ioquakelive] on the batched path the movement has already been read from raw
                // input; SDL's here is the system pointer's, and adding it would count twice
#ifdef _WIN32
                if (rawMouseOn) {
                    rawMouseSdlMotion++;
                    break;
                }
#endif
                if (mouseActive) {
                    if (!e.motion.xrel && !e.motion.yrel)
                        break;
                    Com_QueueEvent(in_eventTime, SE_MOUSE, e.motion.xrel, e.motion.yrel, 0, NULL);
                }
                // [ioquakelive] while the mouse is let go the pointer is not followed by its
                // moves but read where it is, once a frame, in IN_Frame
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int b;
                switch (e.button.button) {
                    case SDL_BUTTON_LEFT:
                        b = K_MOUSE1;
                        break;
                    case SDL_BUTTON_MIDDLE:
                        b = K_MOUSE3;
                        break;
                    case SDL_BUTTON_RIGHT:
                        b = K_MOUSE2;
                        break;
                    case SDL_BUTTON_X1:
                        b = K_MOUSE4;
                        break;
                    case SDL_BUTTON_X2:
                        b = K_MOUSE5;
                        break;
                    default:
                        b = K_AUX1 + (e.button.button - SDL_BUTTON_X2 + 1) % 16;
                        break;
                }
                Com_QueueEvent(in_eventTime, SE_KEY, b,
                               (e.type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse), 0, NULL);
            } break;

            case SDL_MOUSEWHEEL:
                if (e.wheel.y > 0) {
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL);
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL);
                } else if (e.wheel.y < 0) {
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL);
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL);
                }
                break;

            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
                if (in_joystick->integer)
                    IN_InitJoystick();
                break;

            // [ioquakelive] Windows can take the audio device away from the game -- on switching
            // windows, on an output change, or when another program grabs it exclusively. SDL
            // closes it and reports it through this event; the engine did not listen, so the
            // sound went away and did not come back until a restart. It is reopened here. The
            // capture device does not count: voice handles that one itself.
            case SDL_AUDIODEVICEREMOVED:
                if (!e.adevice.iscapture) {
                    S_DeviceLost();
                }
                break;

            case SDL_QUIT:
                Cbuf_ExecuteText(EXEC_NOW, "quit Closed window\n");
                break;

            case SDL_WINDOWEVENT:
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_RESIZED: {
                        int width, height;

                        width = e.window.data1;
                        height = e.window.data2;

                        // ignore this event on fullscreen
                        if (cls.glconfig.isFullscreen) {
                            break;
                        }

                        // check if size actually changed
                        if (cls.glconfig.vidWidth == width && cls.glconfig.vidHeight == height) {
                            break;
                        }

                        // [QL] a window keeps its size in r_windowedWidth and r_windowedHeight
                        Cvar_SetValue("r_windowedWidth", width);
                        Cvar_SetValue("r_windowedHeight", height);
                        Cvar_Set("r_windowedMode", "-1");

                        // Wait until user stops dragging for 1 second, so
                        // we aren't constantly recreating the GL context while
                        // he tries to drag...
                        vidRestartTime = Sys_Milliseconds() + 1000;
                    } break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        Cvar_SetValue("com_minimized", 1);
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        Cvar_SetValue("com_minimized", 0);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        Cvar_SetValue("com_unfocused", 1);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        Cvar_SetValue("com_unfocused", 0);
                        break;
                }
                break;

#if defined(PROTOCOL_HANDLER) && defined(__APPLE__)
            case SDL_DROPFILE: {
                char* filename = e.drop.file;

                // Handle macOS open URL event. URL protocol scheme must be set in Info.plist.
                if (!Q_strncmp(filename, PROTOCOL_HANDLER ":", strlen(PROTOCOL_HANDLER ":"))) {
                    char* protocolCommand = Sys_ParseProtocolUri(filename);

                    if (protocolCommand) {
                        Cbuf_ExecuteText(EXEC_APPEND, va("%s\n", protocolCommand));
                        free(protocolCommand);
                    }
                }

                SDL_free(filename);
            } break;
#endif

            default:
                break;
        }
    }
}

/*
===============
IN_Frame
===============
*/
void IN_Frame(void) {
    qboolean loading;

    IN_JoyMove();

    // If not DISCONNECTED (main menu) or ACTIVE (in game), we're loading
    loading = (clc.state != CA_DISCONNECTED && clc.state != CA_ACTIVE);

    // update isFullscreen since it might of changed since the last vid_restart
    cls.glconfig.isFullscreen = Cvar_VariableIntegerValue("r_fullscreen") != 0;

    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        // [ioquakelive] The console lets go of the mouse, fullscreen included, because the
        // arrow the system draws is the one that clicks its links, and it is told where that
        // arrow is for the same reason the menus are.
        IN_DeactivateMouse(qfalse);

        if (SDL_GetMouseFocus() == SDL_window) {
            int mx, my;

            SDL_GetMouseState(&mx, &my);
            Com_QueueEvent(in_eventTime, SE_MOUSE_ABS, mx, my, 0, NULL);
        }
    } else if (Key_GetCatcher() & KEYCATCH_UI) {
        // [ioquakelive] A menu is up. Quake Live has no cursor of its own out there: it
        // lets go of the mouse and you move the one the system draws, fullscreen included,
        // so the pointer is passed as qfalse rather than the real mode.
        IN_DeactivateMouse(qfalse);

        // The menu is told where the pointer is, every frame. Walking its cursor by relative
        // moves left it a little away from the arrow the system draws -- clicks landed where
        // the arrow was not -- and reading the position instead cannot drift: whatever the
        // menu thought, the next frame puts it back under the arrow.
        if (SDL_GetMouseFocus() == SDL_window) {
            int mx, my;

            SDL_GetMouseState(&mx, &my);
            Com_QueueEvent(in_eventTime, SE_MOUSE_ABS, mx, my, 0, NULL);
        }
    } else if (!cls.glconfig.isFullscreen && loading) {
        // Loading in windowed mode
        IN_DeactivateMouse(cls.glconfig.isFullscreen);
    } else if (!(SDL_GetWindowFlags(SDL_window) & SDL_WINDOW_INPUT_FOCUS)) {
        // Window not got focus
        IN_DeactivateMouse(cls.glconfig.isFullscreen);
    } else
        IN_ActivateMouse(cls.glconfig.isFullscreen);

    // [ioquakelive] /inputrate times what servicing a frame's input costs; with no measurement
    // asked for this does nothing
    if (in_rateWanted) {
        int64_t startUs = Sys_Microseconds();
        int before = in_rateEvents;
        int64_t costUs;

#ifdef _WIN32
        IN_RawMouseRead();
#endif
        IN_ProcessEvents();

        costUs = Sys_Microseconds() - startUs;
        in_rateUsTotal += costUs;
        if (costUs > in_rateUsMax) {
            in_rateUsMax = costUs;
        }
        if (in_rateEvents - before > in_rateMaxFrame) {
            in_rateMaxFrame = in_rateEvents - before;
        }
        if (++in_rateFrames >= in_rateWanted) {
            IN_InputRateReport();
            in_rateWanted = 0;
        }
    } else {
#ifdef _WIN32
        IN_RawMouseRead();
#endif
        IN_ProcessEvents();
    }

    // Set event time for next frame to earliest possible time an event could happen
    in_eventTime = Sys_Milliseconds();

    // In case we had to delay actual restart of video system
    if ((vidRestartTime != 0) && (vidRestartTime < Sys_Milliseconds())) {
        vidRestartTime = 0;
        Cbuf_AddText("vid_restart\n");
    }
}

/*
===============
IN_Init
===============
*/
void IN_Init(void* windowData) {
    int appState;

    Cmd_AddCommand("inputrate", IN_InputRate_f);

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        Com_Error(ERR_FATAL, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )");
        return;
    }

    SDL_window = (SDL_Window*)windowData;

    Com_DPrintf("\n------- Input Initialization -------\n");

    in_keyboardDebug = Cvar_Get("in_keyboardDebug", "0", CVAR_ARCHIVE);

    // mouse variables
    // [QL] quakelive_steam.exe 0x4ec160-0x4ec316: in_mouse 2 is raw input, 1 DirectInput, -1 the
    // Windows cursor and 0 no mouse; in_mouseMode says which one runs. Here 1 and 2 are SDL's relative
    // mode, which reads raw input, and -1 warps the cursor to the centre like the cursor mode did.
    in_mouse = Cvar_Get("in_mouse", "2", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE);
    // [ioquakelive] The batched mouse read (GetRawInputBuffer), meant for 8000 Hz mice. On real
    // hardware the view did not move with it, and the safety net did not catch that: it compared
    // against SDL's movement, which stops arriving once the clipped cursor reaches the window's
    // edge. It stays off until it is verified on real hardware; in_mouse 2 is SDL's relative
    // mode again, which is what worked.
    in_rawBatch = Cvar_Get("in_rawBatch", "0", CVAR_ARCHIVE | CVAR_LATCH);
    in_mouseMode = Cvar_Get("in_mouseMode", "undefined", CVAR_ROM | CVAR_TEMP | CVAR_USERSAVE);
    in_nograb = Cvar_Get("in_nograb", "0", CVAR_TEMP);
    // [QL] which window raw input is registered to; SDL registers it itself
    Cvar_Get("in_raw_useWindowHandle", "0", CVAR_ARCHIVE);

    if (in_mouse->integer == 0) {
        Com_Printf("Mouse control not active.\n");
    } else if (in_mouse->integer == 2) {
        // [ioquakelive] raw input is read in batches right here on Windows; on the other
        // plataformas sigue siendo el modo relativo de SDL
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
#ifdef _WIN32
        Cvar_Set("in_mouseMode", in_rawBatch->integer ? "win32(Raw, batched)" : "win32(Raw)");
#else
        Cvar_Set("in_mouseMode", "win32(Raw)");
#endif
    } else {
        // 1 and -1 both fall back on the system's mouse; -1 warps it to the centre, as retail's
        // cursor mode did
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, in_mouse->integer == -1 ? "1" : "0");
        Cvar_Set("in_mouseMode", "win32");
    }

    in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH);
    in_joystickThreshold = Cvar_Get("joy_threshold", "0.15", CVAR_ARCHIVE);

#if defined(PROTOCOL_HANDLER) && defined(__APPLE__)
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
#endif

    SDL_StartTextInput();

    mouseAvailable = (in_mouse->value != 0);
    IN_DeactivateMouse(Cvar_VariableIntegerValue("r_fullscreen") != 0);

    appState = SDL_GetWindowFlags(SDL_window);
    Cvar_SetValue("com_unfocused", !(appState & SDL_WINDOW_INPUT_FOCUS));
    Cvar_SetValue("com_minimized", appState & SDL_WINDOW_MINIMIZED);

    if (in_joystick->integer) {
        IN_InitJoystick();
    }

    Com_DPrintf("------------------------------------\n");
}

/*
===============
IN_ListInputDevices

[QL] quakelive_steam.exe 0x4eab90: retail lists the raw input mice with their button count and sample
rate. SDL hides the devices behind its own mouse, so this prints what it knows about it.
===============
*/
void IN_ListInputDevices(void) {
    if (in_mouse->integer != 2) {
        Com_Printf("ListInputDevices is only supported for Raw Input (in_mouse 2).\n");
        return;
    }

    Com_Printf("Raw Input Mouse Devices: \n");
    Com_Printf("  Mouse Id: 0, Mode: %s, Relative: %s\n", in_mouseMode->string,
               SDL_GetRelativeMouseMode() ? "yes" : "no");
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown(void) {
    SDL_StopTextInput();

    IN_DeactivateMouse(Cvar_VariableIntegerValue("r_fullscreen") != 0);
    mouseAvailable = qfalse;

    IN_ShutdownJoystick();

    SDL_window = NULL;
}

/*
===============
IN_Restart
===============
*/
void IN_Restart(void) {
    IN_ShutdownJoystick();
    IN_Init(SDL_window);
}
