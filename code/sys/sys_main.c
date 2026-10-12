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
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifndef DEDICATED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#endif

#include "sys_local.h"
#include "sys_loadlib.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

static char binaryPath[MAX_OSPATH] = {0};
static char binaryName[MAX_OSPATH] = {0};
static char installPath[MAX_OSPATH] = {0};

void Sys_SetBinaryPath(const char* path) {
    Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

char* Sys_BinaryPath(void) {
    return binaryPath;
}

const char* Sys_BinaryName(void) {
    return binaryName;
}

void Sys_SetDefaultInstallPath(const char* path) {
    Q_strncpyz(installPath, path, sizeof(installPath));
}

char* Sys_DefaultInstallPath(void) {
    if (*installPath)
        return installPath;
    else
        return Sys_Cwd();
}

#ifndef __APPLE__
char* Sys_DefaultAppPath(void) {
    return Sys_BinaryPath();
}
#endif

void Sys_In_Restart_f(void) {
#ifndef DEDICATED
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        Com_Printf("in_restart: Cannot restart input while video is shutdown\n");
        return;
    }
#endif

    IN_Restart();
}

char* Sys_ConsoleInput(void) {
    return CON_Input();
}

void Sys_SetClipboardData(const char* text) {
#ifndef DEDICATED
    SDL_SetClipboardText(text);
#else
    (void)text;
#endif
}

void Sys_OpenURL(const char* url) {
#ifndef DEDICATED
    if (!SDL_OpenURL(url)) {
        Com_Printf("^3could not open %s: %s\n", url, SDL_GetError());
    }
#else
    (void)url;
#endif
}

char* Sys_GetClipboardData(void) {
#ifdef DEDICATED
    return NULL;
#else
    char* data = NULL;
    char* cliptext;

    if ((cliptext = SDL_GetClipboardText()) != NULL) {
        if (cliptext[0] != '\0') {
            size_t bufsize = strlen(cliptext) + 1;

            data = Z_Malloc(bufsize);
            Q_strncpyz(data, cliptext, bufsize);

            strtok(data, "\n\r\b");
        }
        SDL_free(cliptext);
    }
    return data;
#endif
}

#ifdef DEDICATED
#define PID_FILENAME PRODUCT_NAME "_server.pid"
#else
#define PID_FILENAME "profile.pid"
#endif

static char* Sys_PIDFileName(const char* gamedir) {
    const char* homePath = Cvar_VariableString("fs_homepath");

    if (*homePath != '\0')
        return va("%s/%s/%s", homePath, gamedir, PID_FILENAME);

    return NULL;
}

void Sys_RemovePIDFile(const char* gamedir) {
    char* pidFile = Sys_PIDFileName(gamedir);

    if (pidFile != NULL)
        remove(pidFile);
}

qboolean Sys_WritePIDFile(const char* gamedir) {
    char* pidFile = Sys_PIDFileName(gamedir);
    FILE* f;
    qboolean stale = qfalse;

    if (pidFile == NULL)
        return qfalse;

    if ((f = fopen(pidFile, "r")) != NULL) {
        char pidBuffer[64] = {0};
        int pid;

        pid = fread(pidBuffer, sizeof(char), sizeof(pidBuffer) - 1, f);
        fclose(f);

        if (pid > 0) {
            pid = atoi(pidBuffer);
            if (!Sys_PIDIsRunning(pid))
                stale = qtrue;
        } else
            stale = qtrue;
    }

    if (FS_CreatePath(pidFile)) {
        return 0;
    }

    if ((f = fopen(pidFile, "w")) != NULL) {
        fprintf(f, "%d", Sys_PID());
        fclose(f);
    } else
        Com_Printf(S_COLOR_YELLOW "Couldn't write %s.\n", pidFile);

    return stale;
}

static __attribute__((noreturn)) void Sys_Exit(int exitCode) {
    CON_Shutdown();

#ifndef DEDICATED
    SDL_Quit();
#endif

    if (exitCode < 2 && com_fullyInitialized) {
#ifdef DEDICATED
        Sys_RemovePIDFile(FS_GetCurrentGameDir());
#else
        Sys_RemovePIDFile(BASEGAME_DIR);
#endif
    }

    NET_Shutdown();

    Sys_PlatformExit();

    exit(exitCode);
}

void Sys_Quit(void) {
    Sys_Exit(0);
}

cpuFeatures_t Sys_GetProcessorFeatures(void) {
    cpuFeatures_t features = 0;

#ifndef DEDICATED
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    features |= CF_RDTSC;
#endif
    if (SDL_HasMMX())
        features |= CF_MMX;
    if (SDL_HasSSE())
        features |= CF_SSE;
    if (SDL_HasSSE2())
        features |= CF_SSE2;
    if (SDL_HasAltiVec())
        features |= CF_ALTIVEC;
#endif

    return features;
}

void Sys_Init(void) {
    Cmd_AddCommand("in_restart", Sys_In_Restart_f);
    Cvar_Set("arch", OS_STRING " " ARCH_STRING);
    Cvar_Set("username", Sys_GetCurrentUser());
}

void Sys_AnsiColorPrint(const char* msg) {
    static char buffer[MAXPRINTMSG];
    int length = 0;
    static int q3ToAnsi[8] =
        {
            30,
            31,
            32,
            33,
            34,
            36,
            35,
            0
        };

    while (*msg) {
        if (Q_IsColorString(msg) || *msg == '\n') {
            if (length > 0) {
                buffer[length] = '\0';
                fputs(buffer, stderr);
                length = 0;
            }

            if (*msg == '\n') {
                fputs("\033[0m\n", stderr);
                msg++;
            } else {
                int code = Q_ColorCodeIndex(*(msg + 1), qtrue);
                int ansi;

                if (code >= 0 && code < 8) {
                    ansi = q3ToAnsi[code];
                } else {
                    const float* rgb = Q_ColorFromCodeIndex(code);
                    int bits = (rgb[0] >= 0.5f) + 2 * (rgb[1] >= 0.5f) + 4 * (rgb[2] >= 0.5f);

                    ansi = (bits == 7) ? 0 : 30 + bits;
                }
                Com_sprintf(buffer, sizeof(buffer), "\033[%dm", ansi);
                fputs(buffer, stderr);
                msg += 2;
            }
        } else {
            if (length >= MAXPRINTMSG - 1)
                break;

            buffer[length] = *msg;
            length++;
            msg++;
        }
    }

    if (length > 0) {
        buffer[length] = '\0';
        fputs(buffer, stderr);
    }
}

void Sys_Print(const char* msg) {
    CON_LogWrite(msg);
    CON_Print(msg);
}

void Sys_Error(const char* error, ...) {
    va_list argptr;
    char string[1024];

    va_start(argptr, error);
    Q_vsnprintf(string, sizeof(string), error, argptr);
    va_end(argptr);

    Sys_ErrorDialog(string);

    Sys_Exit(3);
}

#if 0
static __attribute__((format(printf, 1, 2))) void Sys_Warn(char* warning, ...)
{
	va_list argptr;
	char    string[1024];

	va_start(argptr, warning);
	Q_vsnprintf(string, sizeof(string), warning, argptr);
	va_end(argptr);

	CON_Print(va("Warning: %s", string));
}
#endif

int Sys_FileTime(char* path) {
    struct stat buf;

    if (stat(path, &buf) == -1)
        return -1;

    return buf.st_mtime;
}

void Sys_UnloadDll(void* dllHandle) {
    if (!dllHandle) {
        Com_Printf("Sys_UnloadDll(NULL)\n");
        return;
    }

    Sys_UnloadLibrary(dllHandle);
}

#ifdef DEDICATED
Q_HOOKABLE void Sys_SetModuleOffset(char* moduleName, void* offset) {
    Com_Printf("Sys_SetModuleOffset: %s 0x%llX\n", moduleName, (unsigned long long)(intptr_t)offset);
    if (strcmp(moduleName, "qagame") && strcmp(moduleName, "cgame") && strcmp(moduleName, "ui")) {
        Com_Printf("Unknown module!\n");
    }
}
#endif

void* Sys_LoadDll(const char* name, qboolean useSystemLib) {
    void* dllhandle = NULL;

    if (!Sys_DllExtension(name)) {
        Com_Printf("Refusing to attempt to load library \"%s\": Extension not allowed.\n", name);
        return NULL;
    }

    if (useSystemLib) {
        Com_Printf("Trying to load \"%s\"...\n", name);
        dllhandle = Sys_LoadLibrary(name);
    }

    if (!dllhandle) {
        const char* topDir;
        char libPath[MAX_OSPATH];
        int len;

        topDir = Sys_BinaryPath();

        if (!*topDir)
            topDir = ".";

        len = Com_sprintf(libPath, sizeof(libPath), "%s%c%s", topDir, PATH_SEP, name);
        if (len < sizeof(libPath)) {
            Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, topDir);
            dllhandle = Sys_LoadLibrary(libPath);
        } else {
            Com_Printf("Skipping trying to load \"%s\" from \"%s\", file name is too long.\n", name, topDir);
        }

        if (!dllhandle) {
            const char* basePath = Cvar_VariableString("fs_basepath");

            if (!basePath || !*basePath)
                basePath = ".";

            if (FS_FilenameCompare(topDir, basePath)) {
                len = Com_sprintf(libPath, sizeof(libPath), "%s%c%s", basePath, PATH_SEP, name);
                if (len < sizeof(libPath)) {
                    Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, basePath);
                    dllhandle = Sys_LoadLibrary(libPath);
                } else {
                    Com_Printf("Skipping trying to load \"%s\" from \"%s\", file name is too long.\n", name, basePath);
                }
            }

            if (!dllhandle)
                Com_Printf("Loading \"%s\" failed\n", name);
        }
    }

    return dllhandle;
}

void* Sys_LoadGameDll(const char* name,
                      vmMainProc* entryPoint,
                      intptr_t (*systemcalls)(intptr_t, ...)) {
    void* libHandle;
    void (*dllEntry)(intptr_t (*syscallptr)(intptr_t, ...));

    assert(name);

    if (!Sys_DllExtension(name)) {
        Com_Printf("Refusing to attempt to load library \"%s\": Extension not allowed.\n", name);
        return NULL;
    }

    Com_Printf("Loading dll file %s\n", name);
    libHandle = Sys_LoadLibrary(name);

    if (!libHandle) {
        Com_Printf("Sys_LoadGameDll(%s) failed:\n\"%s\"\n", name, Sys_LibraryError());
        return NULL;
    }

    dllEntry = Sys_LoadFunction(libHandle, "dllEntry");
    *entryPoint = Sys_LoadFunction(libHandle, "vmMain");

    if (!*entryPoint || !dllEntry) {
        Com_Printf("Sys_LoadGameDll(%s) failed to find vmMain function:\n\"%s\" !\n", name, Sys_LibraryError());
        Sys_UnloadLibrary(libHandle);

        return NULL;
    }

    Com_DPrintf("Sys_LoadGameDll(%s) found vmMain function at %p\n", name, *entryPoint);
    dllEntry(systemcalls);

    return libHandle;
}

void Sys_ParseArgs(int argc, char** argv) {
    if (argc == 2) {
        if (!strcmp(argv[1], "--version") ||
            !strcmp(argv[1], "-v")) {
#ifdef DEDICATED
            fprintf(stdout, FULL_PRODUCT_VERSION " dedicated server\n");
#else
            fprintf(stdout, FULL_PRODUCT_VERSION " client\n");
#endif
            Sys_Exit(0);
        }
    }
}

char* Sys_ParseProtocolUri(const char* uri) {
    if (Q_strncmp(uri, PROTOCOL_HANDLER ":", strlen(PROTOCOL_HANDLER ":"))) {
        Com_Printf("Sys_ParseProtocolUri: unsupported protocol.\n");
        return NULL;
    }
    uri += strlen(PROTOCOL_HANDLER ":");
    if (!Q_strncmp(uri, "//", strlen("//"))) {
        uri += strlen("//");
    }
    Com_Printf("Sys_ParseProtocolUri: %s\n", uri);

    if (!Q_strncmp(uri, "connect/", strlen("connect/"))) {
        int i, bufsize;
        char* out;

        uri += strlen("connect/");
        if (*uri == '\0' || *uri == '?') {
            Com_Printf("Sys_ParseProtocolUri: missing argument.\n");
            return NULL;
        }

        for (i = 0; uri[i] != '\0'; i++) {
            if (uri[i] == '?') {
                break;
            }

            if (isalpha(uri[i]) == 0 && isdigit(uri[i]) == 0 && uri[i] != '.' && uri[i] != ':' && uri[i] != '-' && uri[i] != '[' && uri[i] != ']') {
                Com_Printf("Sys_ParseProtocolUri: hostname contains unsupported character.\n");
                return NULL;
            }
        }

        bufsize = strlen("connect ") + i + 1;
        out = malloc(bufsize);
        strcpy(out, "connect ");
        strncat(out, uri, i);
        return out;
    } else {
        Com_Printf("Sys_ParseProtocolUri: unsupported command.\n");
        return NULL;
    }
}

#ifndef DEFAULT_BASEDIR
#ifdef __APPLE__
#define DEFAULT_BASEDIR Sys_StripAppBundle(Sys_BinaryPath())
#else
#define DEFAULT_BASEDIR Sys_BinaryPath()
#endif
#endif

void Sys_SigHandler(int signal) {
    static qboolean signalcaught = qfalse;

    if (signalcaught) {
        fprintf(stderr, "DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n",
                signal);
    } else {
        signalcaught = qtrue;
        VM_Forced_Unload_Start();
#ifndef DEDICATED
        CL_Shutdown(va("Received signal %d", signal), qtrue, qtrue);
#endif
        SV_Shutdown(va("Received signal %d", signal));
        VM_Forced_Unload_Done();
    }

    if (signal == SIGTERM || signal == SIGINT)
        Sys_Exit(1);
    else
        Sys_Exit(2);
}

int main(int argc, char** argv) {
    int i;
    char* commandLine;
    int commandLineSize = MAX_STRING_CHARS;
#ifdef PROTOCOL_HANDLER
    char* protocolCommand = NULL;
#endif

    extern void Sys_LaunchAutoupdater(int argc, char** argv);
    Sys_LaunchAutoupdater(argc, argv);

#ifndef DEDICATED

#if !SDL_VERSION_ATLEAST(MINSDL_MAJOR, MINSDL_MINOR, MINSDL_PATCH)
#error A more recent version of SDL is required
#endif

    int ver = SDL_GetVersion();

#define MINSDL_VERSION    \
    XSTRING(MINSDL_MAJOR) \
    "." XSTRING(MINSDL_MINOR) "." XSTRING(MINSDL_PATCH)

    if (ver < SDL_VERSIONNUM(MINSDL_MAJOR, MINSDL_MINOR, MINSDL_PATCH)) {
        Sys_Dialog(DT_ERROR, va("SDL version " MINSDL_VERSION " or greater is required, "
                                "but only version %d.%d.%d was found. You may be able to obtain a more recent copy "
                                "from http://www.libsdl.org/.",
                                SDL_VERSIONNUM_MAJOR(ver), SDL_VERSIONNUM_MINOR(ver), SDL_VERSIONNUM_MICRO(ver)),
                   "SDL Library Too Old");

        Sys_Exit(1);
    }
#endif

    Sys_PlatformInit();

    Sys_Milliseconds();

#ifdef __APPLE__
    if (argc >= 2 && Q_strncmp(argv[1], "-psn", 4) == 0)
        argc = 1;

    Sys_DisableStateRestoration();
#endif

    Sys_ParseArgs(argc, argv);
    Sys_SetBinaryPath(Sys_Dirname(argv[0]));

    {
        const char* name = argv[0];
        const char* p;

        for (p = argv[0]; *p; p++) {
            if (*p == '/' || *p == '\\')
                name = p + 1;
        }
        Q_strncpyz(binaryName, name, sizeof(binaryName));
    }
    Sys_SetDefaultInstallPath(DEFAULT_BASEDIR);

    for (i = 1; i < argc; i++) {
        commandLineSize += (int)strlen(argv[i]) + 3;
    }
    commandLine = calloc((size_t)commandLineSize, 1);
    if (!commandLine) {
        Sys_Error("Out of memory for the command line");
    }

    for (i = 1; i < argc; i++) {
        qboolean containsSpaces;

        if (!strcmp(argv[i], "--uri")) {
#ifdef PROTOCOL_HANDLER
            if (argc > i + 1) {
                protocolCommand = Sys_ParseProtocolUri(argv[i + 1]);
            }
#endif
            break;
        }

        containsSpaces = strchr(argv[i], ' ') != NULL;
        if (containsSpaces)
            Q_strcat(commandLine, commandLineSize, "\"");

        Q_strcat(commandLine, commandLineSize, argv[i]);

        if (containsSpaces)
            Q_strcat(commandLine, commandLineSize, "\"");

        Q_strcat(commandLine, commandLineSize, " ");
    }

#ifdef PROTOCOL_HANDLER
    if (protocolCommand != NULL) {
        Q_strcat(commandLine, commandLineSize, "+");
        Q_strcat(commandLine, commandLineSize, protocolCommand);
        free(protocolCommand);
    }
#endif

    CON_Init();
    Com_Init(commandLine);
    NET_Init();

    signal(SIGILL, Sys_SigHandler);
    signal(SIGFPE, Sys_SigHandler);
    signal(SIGSEGV, Sys_SigHandler);
    signal(SIGTERM, Sys_SigHandler);
    signal(SIGINT, Sys_SigHandler);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Com_Frame, 0, 1);
#else
    while (1) {
        Com_Frame();
    }
#endif

    return 0;
}
