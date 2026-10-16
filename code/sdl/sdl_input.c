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
#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../client/client.h"
#include "../sys/sys_local.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static cvar_t* in_keyboardDebug = NULL;

static SDL_Gamepad* gamepad = NULL;
static SDL_Joystick* stick = NULL;

static qboolean mouseAvailable = qfalse;

static int64_t in_rateUsTotal, in_rateUsMax, in_rateStartUs;
static int in_rateWanted, in_rateFrames, in_rateEvents, in_rateMouse, in_rateMaxFrame;

static qboolean mouseActive = qfalse;

static cvar_t* in_mouse = NULL;
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

static void IN_PrintKey(const SDL_KeyboardEvent* keysym, keyNum_t key, qboolean down) {
    if (down)
        Com_Printf("+ ");
    else
        Com_Printf("  ");

    Com_Printf("Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
               keysym->scancode, SDL_GetScancodeName(keysym->scancode),
               keysym->key, SDL_GetKeyName(keysym->key));

    if (keysym->mod & SDL_KMOD_LSHIFT)
        Com_Printf(" KMOD_LSHIFT");
    if (keysym->mod & SDL_KMOD_RSHIFT)
        Com_Printf(" KMOD_RSHIFT");
    if (keysym->mod & SDL_KMOD_LCTRL)
        Com_Printf(" KMOD_LCTRL");
    if (keysym->mod & SDL_KMOD_RCTRL)
        Com_Printf(" KMOD_RCTRL");
    if (keysym->mod & SDL_KMOD_LALT)
        Com_Printf(" KMOD_LALT");
    if (keysym->mod & SDL_KMOD_RALT)
        Com_Printf(" KMOD_RALT");
    if (keysym->mod & SDL_KMOD_LGUI)
        Com_Printf(" KMOD_LGUI");
    if (keysym->mod & SDL_KMOD_RGUI)
        Com_Printf(" KMOD_RGUI");
    if (keysym->mod & SDL_KMOD_NUM)
        Com_Printf(" KMOD_NUM");
    if (keysym->mod & SDL_KMOD_CAPS)
        Com_Printf(" KMOD_CAPS");
    if (keysym->mod & SDL_KMOD_MODE)
        Com_Printf(" KMOD_MODE");
    if (keysym->mod & SDL_KMOD_SCROLL)
        Com_Printf(" KMOD_SCROLL");

    Com_Printf(" Q:0x%02x(%s)\n", key, Key_KeynumToString(key));
}

static keyNum_t IN_TranslateSDLToQ3Key(const SDL_KeyboardEvent* keysym, qboolean down) {
    keyNum_t key = 0;

    if (keysym->scancode >= SDL_SCANCODE_1 && keysym->scancode <= SDL_SCANCODE_0) {
        if (keysym->scancode == SDL_SCANCODE_0)
            key = '0';
        else
            key = '1' + keysym->scancode - SDL_SCANCODE_1;
    } else if (keysym->key >= SDLK_SPACE && keysym->key < SDLK_DELETE) {
        key = (int)keysym->key;
    } else {
        switch (keysym->key) {
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
                if (!(keysym->key & SDLK_SCANCODE_MASK) && keysym->scancode <= 95) {
                    key = K_WORLD_0 + (int)keysym->scancode;
                }
                break;
        }
    }

    if (in_keyboardDebug->integer)
        IN_PrintKey(keysym, key, down);

    if (keysym->scancode == SDL_SCANCODE_GRAVE) {
        key = K_CONSOLE;
    }

    return key;
}

static void IN_GobbleMotionEvents(void) {
    SDL_Event dummy[1];
    int val = 0;

    SDL_PumpEvents();
    while ((val = SDL_PeepEvents(dummy, 1, SDL_GETEVENT,
                                 SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_MOTION)) > 0) {
    }

    if (val < 0)
        Com_Printf("IN_GobbleMotionEvents failed: %s\n", SDL_GetError());
}

static void IN_GrabMouse(void) {
    SDL_SetWindowRelativeMouseMode(SDL_window, true);
    SDL_SetWindowMouseGrab(SDL_window, true);
}

static void IN_ReleaseMouse(void) {
    SDL_SetWindowRelativeMouseMode(SDL_window, false);
    SDL_SetWindowMouseGrab(SDL_window, false);
}

static void IN_ActivateMouse(qboolean isFullscreen) {
    if (!mouseAvailable || !SDL_WasInit(SDL_INIT_VIDEO))
        return;

    if (!mouseActive) {
        IN_GrabMouse();

        IN_GobbleMotionEvents();
    }

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

static void IN_DeactivateMouse(qboolean isFullscreen) {
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return;

    if (!isFullscreen)
        SDL_ShowCursor();

    if (!mouseAvailable)
        return;

    if (mouseActive) {
        IN_GobbleMotionEvents();

        IN_ReleaseMouse();

        if (SDL_GetWindowFlags(SDL_window) & SDL_WINDOW_MOUSE_FOCUS)
            SDL_WarpMouseInWindow(SDL_window, cls.glconfig.vidWidth / 2.0f, cls.glconfig.vidHeight / 2.0f);

        mouseActive = qfalse;
    }
}

static int joy_keys[16] = {
    K_LEFTARROW, K_RIGHTARROW,
    K_UPARROW, K_DOWNARROW,
    K_JOY17, K_JOY18,
    K_JOY19, K_JOY20,
    K_JOY21, K_JOY22,
    K_JOY23, K_JOY24,
    K_JOY25, K_JOY26,
    K_JOY27, K_JOY28};

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
    qboolean buttons[SDL_GAMEPAD_BUTTON_COUNT];
    unsigned int oldaxes;
    int oldaaxes[MAX_JOYSTICK_AXIS];
    unsigned int oldhats;
} stick_state;

static void IN_InitJoystick(void) {
    int i = 0;
    int total = 0;
    char buf[16384] = "";
    SDL_JoystickID* joysticks;
    SDL_JoystickID id;

    if (gamepad)
        SDL_CloseGamepad(gamepad);

    if (stick != NULL)
        SDL_CloseJoystick(stick);

    stick = NULL;
    gamepad = NULL;
    memset(&stick_state, '\0', sizeof(stick_state));

    Cvar_Set("ui_joyavail", "0");

    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
        if (!SDL_Init(SDL_INIT_JOYSTICK)) {
            Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
            return;
        }
        Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
    }

    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
        Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMEPAD)...\n");
        if (!SDL_Init(SDL_INIT_GAMEPAD)) {
            Com_DPrintf("SDL_Init(SDL_INIT_GAMEPAD) failed: %s\n", SDL_GetError());
            return;
        }
        Com_DPrintf("SDL_Init(SDL_INIT_GAMEPAD) passed.\n");
    }

    joysticks = SDL_GetJoysticks(&total);
    Com_DPrintf("%d possible joysticks\n", total);

    for (i = 0; i < total; i++) {
        const char* name = SDL_GetJoystickNameForID(joysticks[i]);

        Q_strcat(buf, sizeof(buf), name ? name : "");
        Q_strcat(buf, sizeof(buf), "\n");
    }

    Cvar_Get("in_availableJoysticks", "", CVAR_ROM);

    Cvar_Set("in_availableJoysticks", buf);

    if (!in_joystick->integer || !joysticks || total <= 0) {
        Com_DPrintf("Joystick is not active.\n");
        SDL_free(joysticks);
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        return;
    }

    in_joystickNo = Cvar_Get("in_joystickNo", "0", CVAR_ARCHIVE);
    if (in_joystickNo->integer < 0 || in_joystickNo->integer >= total)
        Cvar_Set("in_joystickNo", "0");

    in_joystickUseAnalog = Cvar_Get("in_joystickUseAnalog", "0", CVAR_ARCHIVE);

    id = joysticks[in_joystickNo->integer];
    SDL_free(joysticks);

    stick = SDL_OpenJoystick(id);

    if (stick == NULL) {
        Com_DPrintf("No joystick opened: %s\n", SDL_GetError());
        return;
    }

    if (SDL_IsGamepad(id))
        gamepad = SDL_OpenGamepad(id);

    Cvar_Set("ui_joyavail", "1");
    Com_DPrintf("Joystick %d opened\n", in_joystickNo->integer);
    Com_DPrintf("Name:       %s\n", SDL_GetJoystickName(stick));
    Com_DPrintf("Axes:       %d\n", SDL_GetNumJoystickAxes(stick));
    Com_DPrintf("Hats:       %d\n", SDL_GetNumJoystickHats(stick));
    Com_DPrintf("Buttons:    %d\n", SDL_GetNumJoystickButtons(stick));
    Com_DPrintf("Balls:      %d\n", SDL_GetNumJoystickBalls(stick));
    Com_DPrintf("Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No");
    Com_DPrintf("Is gamepad: %s\n", gamepad ? "Yes" : "No");
}

static void IN_ShutdownJoystick(void) {
    if (!SDL_WasInit(SDL_INIT_GAMEPAD))
        return;

    if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        return;

    if (gamepad) {
        SDL_CloseGamepad(gamepad);
        gamepad = NULL;
    }

    if (stick) {
        SDL_CloseJoystick(stick);
        stick = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
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

static void IN_GamepadMove(void) {
    int i;
    int translatedAxes[MAX_JOYSTICK_AXIS];
    qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

    SDL_UpdateGamepads();

    for (i = 0; i <= SDL_GAMEPAD_BUTTON_TOUCHPAD; i++) {
        qboolean pressed = SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_SOUTH + i)) ? qtrue : qfalse;
        if (pressed != stick_state.buttons[i]) {
            if (i >= SDL_GAMEPAD_BUTTON_MISC1) {
                Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_GAMEPAD_BUTTON_MISC1, pressed, 0, NULL);
            } else {
                Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
            }
            stick_state.buttons[i] = pressed;
        }
    }

    if (in_joystickUseAnalog->integer) {
        for (i = 0; i < MAX_JOYSTICK_AXIS; i++) {
            translatedAxes[i] = 0;
            translatedAxesSet[i] = qfalse;
        }
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++) {
        int axis = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)(SDL_GAMEPAD_AXIS_LEFTX + i));
        int oldAxis = stick_state.oldaaxes[i];

        float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

        if (f < 0.0f)
            f = 0.0f;

        axis = (int)(32767 * ((axis < 0) ? -f : f));

        if (axis != oldAxis) {
            const int negMap[SDL_GAMEPAD_AXIS_COUNT] = {K_PAD0_LEFTSTICK_LEFT, K_PAD0_LEFTSTICK_UP, K_PAD0_RIGHTSTICK_LEFT, K_PAD0_RIGHTSTICK_UP, 0, 0};
            const int posMap[SDL_GAMEPAD_AXIS_COUNT] = {K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER};

            qboolean posAnalog = qfalse, negAnalog = qfalse;
            int negKey = negMap[i];
            int posKey = posMap[i];

            if (in_joystickUseAnalog->integer) {
                int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

                posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
                negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

                if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0) {
                    translatedAxes[posAxis] = 0;
                    translatedAxesSet[posAxis] = qtrue;
                }

                if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0) {
                    translatedAxes[negAxis] = 0;
                    translatedAxesSet[negAxis] = qtrue;
                }

                if (posAnalog && axis > 0) {
                    translatedAxes[posAxis] = axis * posSign;
                    translatedAxesSet[posAxis] = qtrue;
                }

                if (negAnalog && axis < 0) {
                    translatedAxes[negAxis] = -axis * negSign;
                    translatedAxesSet[negAxis] = qtrue;
                }
            }

            if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
                Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

            if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
                Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

            if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
                Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

            if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
                Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

            stick_state.oldaaxes[i] = axis;
        }
    }

    if (in_joystickUseAnalog->integer) {
        for (i = 0; i < MAX_JOYSTICK_AXIS; i++) {
            if (translatedAxesSet[i])
                Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
        }
    }
}

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

    SDL_UpdateJoysticks();

    total = SDL_GetNumJoystickBalls(stick);
    if (total > 0) {
        int balldx = 0;
        int balldy = 0;
        for (i = 0; i < total; i++) {
            int dx = 0;
            int dy = 0;
            SDL_GetJoystickBall(stick, i, &dx, &dy);
            balldx += dx;
            balldy += dy;
        }
        if (balldx || balldy) {
            if (abs(balldx) > 1)
                balldx *= 2;
            if (abs(balldy) > 1)
                balldy *= 2;
            Com_QueueEvent(in_eventTime, SE_MOUSE, balldx, balldy, 0, NULL);
        }
    }

    total = SDL_GetNumJoystickButtons(stick);
    if (total > 0) {
        if (total > ARRAY_LEN(stick_state.buttons))
            total = ARRAY_LEN(stick_state.buttons);
        for (i = 0; i < total; i++) {
            qboolean pressed = (SDL_GetJoystickButton(stick, i) != 0);
            if (pressed != stick_state.buttons[i]) {
                Com_QueueEvent(in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL);
                stick_state.buttons[i] = pressed;
            }
        }
    }

    total = SDL_GetNumJoystickHats(stick);
    if (total > 0) {
        if (total > 4)
            total = 4;
        for (i = 0; i < total; i++) {
            ((Uint8*)&hats)[i] = SDL_GetJoystickHat(stick, i);
        }
    }

    if (hats != stick_state.oldhats) {
        for (i = 0; i < 4; i++) {
            if (((Uint8*)&hats)[i] != ((Uint8*)&stick_state.oldhats)[i]) {
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

    stick_state.oldhats = hats;

    total = SDL_GetNumJoystickAxes(stick);
    if (total > 0) {
        if (in_joystickUseAnalog->integer) {
            if (total > MAX_JOYSTICK_AXIS)
                total = MAX_JOYSTICK_AXIS;
            for (i = 0; i < total; i++) {
                Sint16 axis = SDL_GetJoystickAxis(stick, i);
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
                Sint16 axis = SDL_GetJoystickAxis(stick, i);
                float f = ((float)axis) / 32767.0f;
                if (f < -in_joystickThreshold->value) {
                    axes |= (1 << (i * 2));
                } else if (f > in_joystickThreshold->value) {
                    axes |= (1 << ((i * 2) + 1));
                }
            }
        }
    }

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

    stick_state.oldaxes = axes;
}

static void IN_PointerToRender(float* mx, float* my) {
    int pw, ph, w, h;

    if (!SDL_window || !SDL_GetWindowSizeInPixels(SDL_window, &pw, &ph) || !SDL_GetWindowSize(SDL_window, &w, &h)) {
        return;
    }
    if (w <= 0 || h <= 0 || (pw == cls.glconfig.vidWidth && ph == cls.glconfig.vidHeight)) {
        return;
    }
    *mx *= (float)cls.glconfig.vidWidth / w;
    *my *= (float)cls.glconfig.vidHeight / h;
}

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
        if (in_rateWanted) {
            in_rateEvents++;
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                in_rateMouse++;
            }
        }
        switch (e.type) {
            case SDL_EVENT_KEY_DOWN:
                if (e.key.repeat && Key_GetCatcher() == 0)
                    break;

                if ((key = IN_TranslateSDLToQ3Key(&e.key, qtrue)))
                    Com_QueueEvent(in_eventTime, SE_KEY, key, qtrue, 0, NULL);

                if (key == K_BACKSPACE)
                    Com_QueueEvent(in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL);
                else if (keys[K_CTRL].down && key >= 'a' && key <= 'z')
                    Com_QueueEvent(in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL);

                lastKeyDown = key;
                break;

            case SDL_EVENT_KEY_UP:
                if ((key = IN_TranslateSDLToQ3Key(&e.key, qfalse)))
                    Com_QueueEvent(in_eventTime, SE_KEY, key, qfalse, 0, NULL);

                lastKeyDown = 0;
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (lastKeyDown != K_CONSOLE) {
                    const char* c = e.text.text;

                    while (*c) {
                        int utf32 = 0;

                        if ((*c & 0x80) == 0)
                            utf32 = *c++;
                        else if ((*c & 0xE0) == 0xC0)
                        {
                            utf32 |= (*c++ & 0x1F) << 6;
                            utf32 |= (*c++ & 0x3F);
                        } else if ((*c & 0xF0) == 0xE0)
                        {
                            utf32 |= (*c++ & 0x0F) << 12;
                            utf32 |= (*c++ & 0x3F) << 6;
                            utf32 |= (*c++ & 0x3F);
                        } else if ((*c & 0xF8) == 0xF0)
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
                            Com_QueueEvent(in_eventTime, SE_CHAR, utf32, 0, 0, NULL);
                        }
                    }
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (mouseActive) {
                    static float restX, restY;
                    float x = e.motion.xrel + restX;
                    float y = e.motion.yrel + restY;
                    int dx = (int)x;
                    int dy = (int)y;

                    restX = x - dx;
                    restY = y - dy;

                    if (!dx && !dy)
                        break;
                    Com_QueueEvent(in_eventTime, SE_MOUSE, dx, dy, 0, NULL);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
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
                               (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? qtrue : qfalse), 0, NULL);
            } break;

            case SDL_EVENT_MOUSE_WHEEL:
                if (e.wheel.y > 0) {
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL);
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL);
                } else if (e.wheel.y < 0) {
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL);
                    Com_QueueEvent(in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL);
                }
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (in_joystick->integer)
                    IN_InitJoystick();
                break;

            case SDL_EVENT_AUDIO_DEVICE_REMOVED:
                if (!e.adevice.recording) {
                    S_DeviceLost();
                }
                break;

            case SDL_EVENT_QUIT:
                Cbuf_ExecuteText(EXEC_NOW, "quit Closed window\n");
                break;

            case SDL_EVENT_WINDOW_RESIZED: {
                    int width, height;

                    width = e.window.data1;
                    height = e.window.data2;

                    {
                        SDL_WindowFlags wflags = SDL_GetWindowFlags(SDL_window);

                        if ((wflags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) ||
                            !(wflags & SDL_WINDOW_INPUT_FOCUS)) {
                            break;
                        }
                    }
                    if (width < 32 || height < 32 || Cvar_VariableIntegerValue("com_minimized")) {
                        break;
                    }

                    if (cls.glconfig.vidWidth == width && cls.glconfig.vidHeight == height) {
                        break;
                    }

                    if (!Cvar_VariableIntegerValue("r_fullscreen")) {
                        Cvar_SetValue("r_windowedWidth", width);
                        Cvar_SetValue("r_windowedHeight", height);
                        Cvar_Set("r_windowedMode", "-1");
                    }

                    if (re.WindowResized && re.WindowResized(width, height)) {
                        cls.glconfig.vidWidth = width;
                        cls.glconfig.vidHeight = height;
                        CL_ResolutionChanged();
                        break;
                    }

                    vidRestartTime = Sys_Milliseconds() + 1000;
                } break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                Cvar_SetValue("com_minimized", 1);
                break;
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
                Cvar_SetValue("com_minimized", 0);
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                Cvar_SetValue("com_unfocused", 1);
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                Cvar_SetValue("com_unfocused", 0);
                break;

#if defined(PROTOCOL_HANDLER) && defined(__APPLE__)
            case SDL_EVENT_DROP_FILE: {
                const char* filename = e.drop.data;

                if (!Q_strncmp(filename, PROTOCOL_HANDLER ":", strlen(PROTOCOL_HANDLER ":"))) {
                    char* protocolCommand = Sys_ParseProtocolUri(filename);

                    if (protocolCommand) {
                        Cbuf_ExecuteText(EXEC_APPEND, va("%s\n", protocolCommand));
                        free(protocolCommand);
                    }
                }

            } break;
#endif

            default:
                break;
        }
    }
}

void IN_Frame(void) {
    static qboolean menuPointerSeen;
    qboolean loading;

    IN_JoyMove();

    loading = (clc.state != CA_DISCONNECTED && clc.state != CA_ACTIVE);

    cls.glconfig.isFullscreen = Cvar_VariableIntegerValue("r_fullscreen") != 0;

    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        IN_DeactivateMouse(qfalse);

        if (SDL_GetMouseFocus() == SDL_window) {
            float mx, my;

            SDL_GetMouseState(&mx, &my);
            IN_PointerToRender(&mx, &my);
            Com_QueueEvent(in_eventTime, SE_MOUSE_ABS, (int)mx, (int)my, 0, NULL);
        }
    } else if (Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME)) {
        IN_DeactivateMouse(qfalse);

        if (SDL_GetMouseFocus() == SDL_window) {
            static int lastX, lastY, lastCatcher = -1;
            int catcher = Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME);
            float mx, my;

            SDL_GetMouseState(&mx, &my);
            IN_PointerToRender(&mx, &my);
            if ((int)mx != lastX || (int)my != lastY || catcher != lastCatcher || !menuPointerSeen) {
                lastX = (int)mx;
                lastY = (int)my;
                lastCatcher = catcher;
                menuPointerSeen = qtrue;
                Com_QueueEvent(in_eventTime, SE_MOUSE_ABS, (int)mx, (int)my, 0, NULL);
            }
        } else {
            menuPointerSeen = qfalse;
        }
    } else if (!cls.glconfig.isFullscreen && loading) {
        IN_DeactivateMouse(cls.glconfig.isFullscreen);
    } else if (!(SDL_GetWindowFlags(SDL_window) & SDL_WINDOW_INPUT_FOCUS)) {
        IN_DeactivateMouse(cls.glconfig.isFullscreen);
    } else
        IN_ActivateMouse(cls.glconfig.isFullscreen);

    if (in_rateWanted) {
        int64_t startUs = Sys_Microseconds();
        int before = in_rateEvents;
        int64_t costUs;

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
        IN_ProcessEvents();
    }

    in_eventTime = Sys_Milliseconds();

    if ((vidRestartTime != 0) && (vidRestartTime < Sys_Milliseconds())) {
        vidRestartTime = 0;
        Cbuf_AddText("vid_restart\n");
    }
}

#ifdef _WIN32
static HWND in_hwnd;
static WNDPROC in_sdlWndProc;

static int IN_Clamp(int lo, int hi, int v) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static LRESULT CALLBACK IN_SizingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SIZING && lParam) {
        RECT* r = (RECT*)lParam;
        RECT b = {0, 0, 0, 0};
        int bw, bh, w, h;

        AdjustWindowRectEx(&b, (DWORD)GetWindowLongW(hwnd, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE));
        bw = b.right - b.left;
        bh = b.bottom - b.top;
        w = (r->right - r->left) - bw;
        h = (r->bottom - r->top) - bh;
        if (w < 1024) {
            w = 1024;
        }
        if (h < 768) {
            h = 768;
        }
        switch (wParam) {
            case WMSZ_LEFT:
                r->left = r->right - bw - w;
                r->bottom = r->top + bh + IN_Clamp(w * 9 / 16, w * 3 / 4, h);
                break;
            case WMSZ_RIGHT:
                r->right = r->left + bw + w;
                r->bottom = r->top + bh + IN_Clamp(w * 9 / 16, w * 3 / 4, h);
                break;
            case WMSZ_TOP:
            case WMSZ_TOPRIGHT:
                r->top = r->bottom - bh - h;
                r->right = r->left + bw + IN_Clamp(h * 4 / 3, h * 16 / 9, w);
                break;
            case WMSZ_TOPLEFT:
                r->top = r->bottom - bh - h;
                r->left = r->right - bw - IN_Clamp(h * 4 / 3, h * 16 / 9, w);
                break;
            case WMSZ_BOTTOM:
            case WMSZ_BOTTOMRIGHT:
                r->bottom = r->top + bh + h;
                r->right = r->left + bw + IN_Clamp(h * 4 / 3, h * 16 / 9, w);
                break;
            case WMSZ_BOTTOMLEFT:
                r->bottom = r->top + bh + h;
                r->left = r->right - bw - IN_Clamp(h * 4 / 3, h * 16 / 9, w);
                break;
            default:
                break;
        }
    }
    return CallWindowProcW(in_sdlWndProc, hwnd, msg, wParam, lParam);
}

static void IN_HookWindow(qboolean hook) {
    if (hook) {
        HWND hwnd = SDL_window ? (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(SDL_window),
                                                              SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)
                               : NULL;

        if (hwnd && !in_hwnd) {
            in_sdlWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)IN_SizingWndProc);
            if (in_sdlWndProc) {
                in_hwnd = hwnd;
            }
        }
    } else if (in_hwnd) {
        if ((WNDPROC)GetWindowLongPtrW(in_hwnd, GWLP_WNDPROC) == IN_SizingWndProc) {
            SetWindowLongPtrW(in_hwnd, GWLP_WNDPROC, (LONG_PTR)in_sdlWndProc);
        }
        in_hwnd = NULL;
        in_sdlWndProc = NULL;
    }
}
#endif

static bool SDLCALL IN_FocusWatch(void* userdata, SDL_Event* e) {
    (void)userdata;
    if (e->type == SDL_EVENT_WINDOW_FOCUS_LOST || e->type == SDL_EVENT_WINDOW_MINIMIZED) {
        S_WindowFocusLost();
    }
    return true;
}

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

    in_mouse = Cvar_Get("in_mouse", "2", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE);
    in_mouseMode = Cvar_Get("in_mouseMode", "undefined", CVAR_ROM | CVAR_TEMP | CVAR_USERSAVE);
    in_nograb = Cvar_Get("in_nograb", "0", CVAR_TEMP);
    Cvar_Get("in_raw_useWindowHandle", "0", CVAR_ARCHIVE);

    if (in_mouse->integer == 0) {
        Com_Printf("Mouse control not active.\n");
    } else if (in_mouse->integer == 2) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
        Cvar_Set("in_mouseMode", "win32(Raw)");
    } else {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, in_mouse->integer == -1 ? "1" : "0");
        Cvar_Set("in_mouseMode", "win32");
    }

    in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH);
    in_joystickThreshold = Cvar_Get("joy_threshold", "0.15", CVAR_ARCHIVE);

#if defined(PROTOCOL_HANDLER) && defined(__APPLE__)
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
#endif

    SDL_StartTextInput(SDL_window);

    SDL_AddEventWatch(IN_FocusWatch, NULL);
#ifdef _WIN32
    IN_HookWindow(qtrue);
#endif

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

void IN_ListInputDevices(void) {
    if (in_mouse->integer != 2) {
        Com_Printf("ListInputDevices is only supported for Raw Input (in_mouse 2).\n");
        return;
    }

    Com_Printf("Raw Input Mouse Devices: \n");
    Com_Printf("  Mouse Id: 0, Mode: %s, Relative: %s\n", in_mouseMode->string,
               SDL_GetWindowRelativeMouseMode(SDL_window) ? "yes" : "no");
}

void IN_Shutdown(void) {
    Cmd_RemoveCommand("inputrate");

    SDL_RemoveEventWatch(IN_FocusWatch, NULL);
#ifdef _WIN32
    IN_HookWindow(qfalse);
#endif

    SDL_StopTextInput(SDL_window);

    IN_DeactivateMouse(Cvar_VariableIntegerValue("r_fullscreen") != 0);
    mouseAvailable = qfalse;

    IN_ShutdownJoystick();

    SDL_window = NULL;
}

void IN_Restart(void) {
    Cmd_RemoveCommand("inputrate");

    IN_ShutdownJoystick();
    IN_Init(SDL_window);
}
