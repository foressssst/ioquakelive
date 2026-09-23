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

// Use EnumProcesses() with Windows XP compatibility
#define PSAPI_VERSION 1

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <psapi.h>
#include <float.h>

#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY 0x0200
#endif

// Used to determine where to store user-specific files
static char homePath[MAX_OSPATH] = {0};

// Used to store the Steam Quake Live installation path
static char steamPath[MAX_OSPATH] = {0};

#ifndef DEDICATED
static UINT timerResolution = 0;
#endif

/*
================
Sys_SetFPUCW
Set FPU control word to default value
================
*/

#ifndef _RC_CHOP
// mingw doesn't seem to have these defined :(

#define _MCW_EM 0x0008001fU
#define _MCW_RC 0x00000300U
#define _MCW_PC 0x00030000U
#define _RC_NEAR 0x00000000U
#define _PC_53 0x00010000U

unsigned int _controlfp(unsigned int new, unsigned int mask);
#endif

#define FPUCWMASK1 (_MCW_RC | _MCW_EM)
#define FPUCW (_RC_NEAR | _MCW_EM | _PC_53)

// 64-bit Windows, x86-64 or ARM64, has no x87 precision control to set
#ifdef _WIN64
#define FPUCWMASK (FPUCWMASK1)
#else
#define FPUCWMASK (FPUCWMASK1 | _MCW_PC)
#endif

void Sys_SetFloatEnv(void) {
    _controlfp(FPUCW, FPUCWMASK);
}

/*
================
Sys_MoveUpstreamHomePath

[ioquakelive] The player's data lives in %APPDATA%\ioquakelive. It used to share upstream's
%APPDATA%\quakelive: the first start that finds that folder and not the new one renames it,
so configs, screenshots, demos and fonts all come along. The launcher does the same before
it writes anything there.
================
*/
#define HOMEPATH_UPSTREAM_NAME_WIN "quakelive"

static void Sys_MoveUpstreamHomePath(const char* appData) {
    char oldPath[MAX_OSPATH];
    char newPath[MAX_OSPATH];
    DWORD attributes;

    Com_sprintf(oldPath, sizeof(oldPath), "%s%c%s", appData, PATH_SEP, HOMEPATH_UPSTREAM_NAME_WIN);
    Com_sprintf(newPath, sizeof(newPath), "%s%c%s", appData, PATH_SEP, HOMEPATH_NAME_WIN);

    if (GetFileAttributesA(newPath) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    attributes = GetFileAttributesA(oldPath);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }
    if (MoveFileA(oldPath, newPath)) {
        Com_Printf("Moved your data from %s to %s\n", oldPath, newPath);
    } else {
        Com_Printf("WARNING: couldn't move %s to %s (error %lu)\n", oldPath, newPath, GetLastError());
    }
}

/*
================
Sys_DefaultHomePath
================
*/
char* Sys_DefaultHomePath(void) {
    TCHAR szPath[MAX_PATH];
    FARPROC qSHGetFolderPath;
    HMODULE shfolder = LoadLibrary("shfolder.dll");

    if (shfolder == NULL) {
        Com_Printf("Unable to load SHFolder.dll\n");
        return NULL;
    }

    if (!*homePath && com_homepath) {
        qSHGetFolderPath = GetProcAddress(shfolder, "SHGetFolderPathA");
        if (qSHGetFolderPath == NULL) {
            Com_Printf("Unable to find SHGetFolderPath in SHFolder.dll\n");
            FreeLibrary(shfolder);
            return NULL;
        }

        if (!SUCCEEDED(qSHGetFolderPath(NULL, CSIDL_APPDATA,
                                        NULL, 0, szPath))) {
            Com_Printf("Unable to detect CSIDL_APPDATA\n");
            FreeLibrary(shfolder);
            return NULL;
        }

        Com_sprintf(homePath, sizeof(homePath), "%s%c", szPath, PATH_SEP);

        if (com_homepath->string[0]) {
            Q_strcat(homePath, sizeof(homePath), com_homepath->string);
        } else {
            Sys_MoveUpstreamHomePath(szPath);
            Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_WIN);
        }
    }

    FreeLibrary(shfolder);
    return homePath;
}

/*
================
Sys_DefaultCachePath

[ioquakelive] what is kept only to load faster goes to %LOCALAPPDATA%, out of the game folder
================
*/
char* Sys_DefaultCachePath(void) {
    static char cachePath[MAX_OSPATH];
    TCHAR szPath[MAX_PATH];
    FARPROC qSHGetFolderPath;
    HMODULE shfolder;

    if (cachePath[0]) {
        return cachePath;
    }

    shfolder = LoadLibrary("shfolder.dll");
    if (shfolder == NULL) {
        return NULL;
    }
    qSHGetFolderPath = GetProcAddress(shfolder, "SHGetFolderPathA");
    if (qSHGetFolderPath && SUCCEEDED(qSHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath))) {
        Com_sprintf(cachePath, sizeof(cachePath), "%s%c%s", szPath, PATH_SEP,
                    (com_homepath && com_homepath->string[0]) ? com_homepath->string : HOMEPATH_NAME_WIN);
    }
    FreeLibrary(shfolder);

    return cachePath[0] ? cachePath : NULL;
}

/*
================
Sys_RemoveDir
================
*/
qboolean Sys_RemoveDir(const char* path) {
    return RemoveDirectoryA(path) ? qtrue : qfalse;
}

/*
================
Sys_SteamPath
================
*/
char* Sys_SteamPath(void) {
#if defined(STEAMPATH_NAME) || defined(STEAMPATH_APPID)
    HKEY steamRegKey;
    DWORD pathLen = MAX_OSPATH;
    qboolean finishPath = qfalse;

#ifdef STEAMPATH_APPID
    // Assuming Steam is a 32-bit app
    if (!steamPath[0] && !RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App " STEAMPATH_APPID, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &steamRegKey)) {
        pathLen = MAX_OSPATH;
        if (RegQueryValueEx(steamRegKey, "InstallLocation", NULL, NULL, (LPBYTE)steamPath, &pathLen))
            steamPath[0] = '\0';

        RegCloseKey(steamRegKey);
    }
#endif

#ifdef STEAMPATH_NAME
    if (!steamPath[0] && !RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &steamRegKey)) {
        pathLen = MAX_OSPATH;
        if (RegQueryValueEx(steamRegKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &pathLen))
            if (RegQueryValueEx(steamRegKey, "InstallPath", NULL, NULL, (LPBYTE)steamPath, &pathLen))
                steamPath[0] = '\0';

        if (steamPath[0])
            finishPath = qtrue;

        RegCloseKey(steamRegKey);
    }
#endif

    if (steamPath[0]) {
        if (pathLen == MAX_OSPATH)
            pathLen--;

        steamPath[pathLen] = '\0';

        // Steam stores SteamPath with forward slashes - normalize to backslashes
        {
            char *s;
            for (s = steamPath; *s; s++) {
                if (*s == '/') *s = '\\';
            }
        }

        if (finishPath)
            Q_strcat(steamPath, MAX_OSPATH, "\\SteamApps\\common\\" STEAMPATH_NAME);
    }
#endif

    return steamPath;
}

/*
================
Sys_Milliseconds
================
*/
int sys_timeBase;
int Sys_Milliseconds(void) {
    int sys_curtime;
    static qboolean initialized = qfalse;

    if (!initialized) {
        sys_timeBase = timeGetTime();
        initialized = qtrue;
    }
    sys_curtime = timeGetTime() - sys_timeBase;

    return sys_curtime;
}


/*
================
Sys_Microseconds

[ioquakelive] Monotonic microsecond clock, for pacing frames and for measuring the pacing.
timeGetTime, which is what Sys_Milliseconds gives, moves a millisecond at a time: at 125 fps a
frame lasts 8 ms and with that clock there is no way to aim at anything but 8 or 9.
================
*/
int64_t Sys_Microseconds(void) {
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER base;
    LARGE_INTEGER now;

    if (!frequency.QuadPart) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&base);
        if (!frequency.QuadPart) {
            return (int64_t)Sys_Milliseconds() * 1000;
        }
    }
    QueryPerformanceCounter(&now);
    return ((now.QuadPart - base.QuadPart) * 1000000LL) / frequency.QuadPart;
}


// [ioquakelive] the entry point CreateThread wants, which does not have the engine's signature
typedef struct {
    void (*function)(void*);
    void* argument;
} sysThreadStart_t;

static DWORD WINAPI Sys_ThreadEntry(LPVOID parameter) {
    sysThreadStart_t* start = (sysThreadStart_t*)parameter;

    start->function(start->argument);
    return 0;
}

/*
==============================================================================

ONE FILE OFF THE WEB   (Win32)

[ioquakelive] NOT FROM RETAIL: what the pictures behind a link are fetched with. WinHTTP comes
with Windows, so this asks for nothing new to build or to ship. It blocks, and the caller runs
it on a thread of its own

==============================================================================
*/

qboolean Sys_WebFetch(const char* url, const char* ospath, int maxBytes) {
    wchar_t wideUrl[2048];
    wchar_t host[256];
    wchar_t path[1536];
    URL_COMPONENTS parts;
    HINTERNET session = NULL;
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    FILE* out = NULL;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    DWORD available, read;
    char buffer[16384];
    int total = 0;
    qboolean ok = qfalse;

    if (!url || !ospath) {
        return qfalse;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wideUrl, (int)ARRAY_LEN(wideUrl)) == 0) {
        return qfalse;
    }

    memset(&parts, 0, sizeof(parts));
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = (DWORD)ARRAY_LEN(host);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = (DWORD)ARRAY_LEN(path);

    if (!WinHttpCrackUrl(wideUrl, 0, 0, &parts)) {
        return qfalse;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) {
        return qfalse;
    }

    session = WinHttpOpen(L"ioquakelive", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return qfalse;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);

    connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection) {
        goto done;
    }

    request = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        goto done;
    }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, NULL)) {
        goto done;
    }

    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) ||
        status != 200) {
        goto done;
    }

    out = Sys_FOpen(ospath, "wb");
    if (!out) {
        goto done;
    }

    for (;;) {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            goto done;
        }
        if (!available) {
            break;
        }
        while (available > 0) {
            DWORD want = available > sizeof(buffer) ? (DWORD)sizeof(buffer) : available;

            if (!WinHttpReadData(request, buffer, want, &read) || read == 0) {
                goto done;
            }
            if (maxBytes > 0 && total + (int)read > maxBytes) {
                goto done;  // bigger than the caller takes
            }
            if (fwrite(buffer, 1, read, out) != read) {
                goto done;
            }
            total += (int)read;
            available -= read;
        }
    }

    ok = total > 0 ? qtrue : qfalse;

done:
    if (out) {
        fclose(out);
    }
    if (request) {
        WinHttpCloseHandle(request);
    }
    if (connection) {
        WinHttpCloseHandle(connection);
    }
    if (session) {
        WinHttpCloseHandle(session);
    }
    if (!ok) {
        remove(ospath);
    }

    return ok;
}


/*
==============================================================================

THREADS   (Win32)

[ioquakelive] The minimum needed to spread work across cores: create a thread, a mutex and a
semaphore. The queue built on this lives in common.c, which is portable.

==============================================================================
*/

void* Sys_ThreadCreate(void (*function)(void*), void* argument) {
    static sysThreadStart_t starts[16];
    static int next;
    HANDLE handle;
    DWORD id;
    sysThreadStart_t* start;

    if (next >= (int)ARRAY_LEN(starts)) {
        return NULL;
    }
    start = &starts[next++];
    start->function = function;
    start->argument = argument;

    handle = CreateThread(NULL, 0, Sys_ThreadEntry, start, 0, &id);
    return (void*)handle;
}

void* Sys_MutexCreate(void) {
    CRITICAL_SECTION* section = (CRITICAL_SECTION*)calloc(1, sizeof(CRITICAL_SECTION));

    if (section) {
        InitializeCriticalSection(section);
    }
    return section;
}

void Sys_MutexLock(void* mutex) {
    EnterCriticalSection((CRITICAL_SECTION*)mutex);
}

void Sys_MutexUnlock(void* mutex) {
    LeaveCriticalSection((CRITICAL_SECTION*)mutex);
}

void* Sys_SemaphoreCreate(void) {
    return (void*)CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
}

void Sys_SemaphorePost(void* semaphore) {
    ReleaseSemaphore((HANDLE)semaphore, 1, NULL);
}

void Sys_SemaphoreWait(void* semaphore) {
    WaitForSingleObject((HANDLE)semaphore, INFINITE);
}

// [ioquakelive] gives up the rest of the slice; that is what to do while waiting on another
// thread, rather than spinning and taking the core from it
void Sys_ThreadYield(void) {
    SwitchToThread();
}

int Sys_ProcessorCount(void) {
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
}

/*
================
Sys_RandomBytes
================
*/
qboolean Sys_RandomBytes(byte* string, int len) {
    HCRYPTPROV prov;

    if (!CryptAcquireContext(&prov, NULL, NULL,
                             PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return qfalse;
    }

    if (!CryptGenRandom(prov, len, (BYTE*)string)) {
        CryptReleaseContext(prov, 0);
        return qfalse;
    }
    CryptReleaseContext(prov, 0);
    return qtrue;
}

/*
================
Sys_GetCurrentUser
================
*/
char* Sys_GetCurrentUser(void) {
    static char s_userName[1024];
    unsigned long size = sizeof(s_userName);

    if (!GetUserName(s_userName, &size))
        strcpy(s_userName, "player");

    if (!s_userName[0]) {
        strcpy(s_userName, "player");
    }

    return s_userName;
}

#define MEM_THRESHOLD 96 * 1024 * 1024

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory(void) {
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
}

/*
==============
Sys_Basename
==============
*/
const char* Sys_Basename(char* path) {
    static char base[MAX_OSPATH] = {0};
    int length;

    length = strlen(path) - 1;

    // Skip trailing slashes
    while (length > 0 && path[length] == '\\')
        length--;

    while (length > 0 && path[length - 1] != '\\')
        length--;

    Q_strncpyz(base, &path[length], sizeof(base));

    length = strlen(base) - 1;

    // Strip trailing slashes
    while (length > 0 && base[length] == '\\')
        base[length--] = '\0';

    return base;
}

/*
==============
Sys_Dirname
==============
*/
const char* Sys_Dirname(char* path) {
    static char dir[MAX_OSPATH] = {0};
    int length;

    Q_strncpyz(dir, path, sizeof(dir));
    length = strlen(dir) - 1;

    while (length > 0 && dir[length] != '\\')
        length--;

    dir[length] = '\0';

    return dir;
}

/*
==============
Sys_FOpen
==============
*/
FILE* Sys_FOpen(const char* ospath, const char* mode) {
    size_t length;

    // Windows API ignores all trailing spaces and periods which can get around Quake 3 file system restrictions.
    length = strlen(ospath);
    if (length == 0 || ospath[length - 1] == ' ' || ospath[length - 1] == '.') {
        return NULL;
    }

    return fopen(ospath, mode);
}

/*
==============
Sys_Mkdir
==============
*/
qboolean Sys_Mkdir(const char* path) {
    if (!CreateDirectory(path, NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            return qfalse;
    }

    return qtrue;
}

/*
==================
Sys_Mkfifo
Noop on windows because named pipes do not function the same way
==================
*/
FILE* Sys_Mkfifo(const char* ospath) {
    return NULL;
}

/*
==============
Sys_Cwd
==============
*/
char* Sys_Cwd(void) {
    static char cwd[MAX_OSPATH];

    _getcwd(cwd, sizeof(cwd) - 1);
    cwd[MAX_OSPATH - 1] = 0;

    return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==============
Sys_ListFilteredFiles
==============
*/
void Sys_ListFilteredFiles(const char* basedir, char* subdirs, char* filter, char** list, int* numfiles) {
    char search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    char filename[MAX_OSPATH];
    intptr_t findhandle;
    struct _finddata_t findinfo;

    if (*numfiles >= MAX_FOUND_FILES - 1) {
        return;
    }

    if (basedir[0] == '\0') {
        return;
    }

    if (strlen(subdirs)) {
        Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
    } else {
        Com_sprintf(search, sizeof(search), "%s\\*", basedir);
    }

    findhandle = _findfirst(search, &findinfo);
    if (findhandle == -1) {
        return;
    }

    do {
        if (findinfo.attrib & _A_SUBDIR) {
            if (Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, "..")) {
                if (strlen(subdirs)) {
                    Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name);
                } else {
                    Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
                }
                Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
            }
        }
        if (*numfiles >= MAX_FOUND_FILES - 1) {
            break;
        }
        Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name);
        if (!Com_FilterPath(filter, filename, qfalse))
            continue;
        list[*numfiles] = CopyString(filename);
        (*numfiles)++;
    } while (_findnext(findhandle, &findinfo) != -1);

    _findclose(findhandle);
}

/*
==============
strgtr
==============
*/
static qboolean strgtr(const char* s0, const char* s1) {
    int l0, l1, i;

    l0 = strlen(s0);
    l1 = strlen(s1);

    if (l1 < l0) {
        l0 = l1;
    }

    for (i = 0; i < l0; i++) {
        if (s1[i] > s0[i]) {
            return qtrue;
        }
        if (s1[i] < s0[i]) {
            return qfalse;
        }
    }
    return qfalse;
}

/*
==============
Sys_ListFiles
==============
*/
char** Sys_ListFiles(const char* directory, const char* extension, char* filter, int* numfiles, qboolean wantsubs) {
    char search[MAX_OSPATH];
    int nfiles;
    char** listCopy;
    char* list[MAX_FOUND_FILES];
    struct _finddata_t findinfo;
    intptr_t findhandle;
    int flag;
    int i;
    int extLen;

    if (filter) {
        nfiles = 0;
        Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);

        list[nfiles] = 0;
        *numfiles = nfiles;

        if (!nfiles)
            return NULL;

        listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
        for (i = 0; i < nfiles; i++) {
            listCopy[i] = list[i];
        }
        listCopy[i] = NULL;

        return listCopy;
    }

    if (directory[0] == '\0') {
        *numfiles = 0;
        return NULL;
    }

    if (!extension) {
        extension = "";
    }

    // passing a slash as extension will find directories
    if (extension[0] == '/' && extension[1] == 0) {
        extension = "";
        flag = 0;
    } else {
        flag = _A_SUBDIR;
    }

    extLen = strlen(extension);

    Com_sprintf(search, sizeof(search), "%s\\*%s", directory, extension);

    // search
    nfiles = 0;

    findhandle = _findfirst(search, &findinfo);
    if (findhandle == -1) {
        *numfiles = 0;
        return NULL;
    }

    do {
        if ((!wantsubs && flag ^ (findinfo.attrib & _A_SUBDIR)) || (wantsubs && findinfo.attrib & _A_SUBDIR)) {
            if (*extension) {
                if (strlen(findinfo.name) < extLen ||
                    Q_stricmp(
                        findinfo.name + strlen(findinfo.name) - extLen,
                        extension)) {
                    continue;  // didn't match
                }
            }
            if (nfiles == MAX_FOUND_FILES - 1) {
                break;
            }
            list[nfiles] = CopyString(findinfo.name);
            nfiles++;
        }
    } while (_findnext(findhandle, &findinfo) != -1);

    list[nfiles] = 0;

    _findclose(findhandle);

    // return a copy of the list
    *numfiles = nfiles;

    if (!nfiles) {
        return NULL;
    }

    listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;

    do {
        flag = 0;
        for (i = 1; i < nfiles; i++) {
            if (strgtr(listCopy[i - 1], listCopy[i])) {
                char* temp = listCopy[i];
                listCopy[i] = listCopy[i - 1];
                listCopy[i - 1] = temp;
                flag = 1;
            }
        }
    } while (flag);

    return listCopy;
}

/*
==============
Sys_FreeFileList
==============
*/
void Sys_FreeFileList(char** list) {
    int i;

    if (!list) {
        return;
    }

    for (i = 0; list[i]; i++) {
        Z_Free(list[i]);
    }

    Z_Free(list);
}

/*
==============
Sys_Sleep

Block execution for msec or until input is received.
==============
*/
void Sys_Sleep(int msec) {
    if (msec == 0)
        return;

#ifdef DEDICATED
    if (msec < 0)
        WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), INFINITE);
    else
        WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), msec);
#else
    // Client Sys_Sleep doesn't support waiting on stdin
    if (msec < 0)
        return;

    Sleep(msec);
#endif
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog(const char* error) {
    Sys_Print(va("%s\n", error));

    if (Sys_Dialog(DT_YES_NO, va("%s. Copy console log to clipboard?", error),
                   "Error") == DR_YES) {
        HGLOBAL memoryHandle;
        char* clipMemory;

        memoryHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, CON_LogSize() + 1);
        clipMemory = (char*)GlobalLock(memoryHandle);

        if (clipMemory) {
            char* p = clipMemory;
            char buffer[1024];
            unsigned int size;

            while ((size = CON_LogRead(buffer, sizeof(buffer))) > 0) {
                Com_Memcpy(p, buffer, size);
                p += size;
            }

            *p = '\0';

            if (OpenClipboard(NULL) && EmptyClipboard())
                SetClipboardData(CF_TEXT, memoryHandle);

            GlobalUnlock(clipMemory);
            CloseClipboard();
        }
    }
}

/*
==============
Sys_Dialog

Display a win32 dialog box
==============
*/
dialogResult_t Sys_Dialog(dialogType_t type, const char* message, const char* title) {
    UINT uType;

    switch (type) {
        default:
        case DT_INFO:
            uType = MB_ICONINFORMATION | MB_OK;
            break;
        case DT_WARNING:
            uType = MB_ICONWARNING | MB_OK;
            break;
        case DT_ERROR:
            uType = MB_ICONERROR | MB_OK;
            break;
        case DT_YES_NO:
            uType = MB_ICONQUESTION | MB_YESNO;
            break;
        case DT_OK_CANCEL:
            uType = MB_ICONWARNING | MB_OKCANCEL;
            break;
    }

    switch (MessageBox(NULL, message, title, uType)) {
        default:
        case IDOK:
            return DR_OK;
        case IDCANCEL:
            return DR_CANCEL;
        case IDYES:
            return DR_YES;
        case IDNO:
            return DR_NO;
    }
}

/*
==============
Sys_GLimpSafeInit

Windows specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit(void) {
}

/*
==============
Sys_GLimpInit

Windows specific GL implementation initialisation
==============
*/
void Sys_GLimpInit(void) {
}

/*
==============
Sys_PlatformInit

Windows specific initialisation
==============
*/


void Sys_PlatformInit(void) {
#ifndef DEDICATED
    TIMECAPS ptc;
#endif

    Sys_SetFloatEnv();


#ifndef DEDICATED
    if (timeGetDevCaps(&ptc, sizeof(ptc)) == MMSYSERR_NOERROR) {
        timerResolution = ptc.wPeriodMin;

        if (timerResolution > 1) {
            Com_Printf(
                "Warning: Minimum supported timer resolution is %ums "
                "on this system, recommended resolution 1ms\n",
                timerResolution);
        }

        timeBeginPeriod(timerResolution);
    } else
        timerResolution = 0;
#endif
}

/*
==============
Sys_PlatformExit

Windows specific initialisation
==============
*/
void Sys_PlatformExit(void) {
#ifndef DEDICATED
    if (timerResolution)
        timeEndPeriod(timerResolution);
#endif
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/
void Sys_SetEnv(const char* name, const char* value) {
    if (value)
        _putenv(va("%s=%s", name, value));
    else
        _putenv(va("%s=", name));
}

/*
==============
Sys_PID
==============
*/
int Sys_PID(void) {
    return GetCurrentProcessId();
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning(int pid) {
    DWORD processes[1024];
    DWORD numBytes, numProcesses;
    int i;

    if (!EnumProcesses(processes, sizeof(processes), &numBytes))
        return qfalse;  // Assume it's not running

    numProcesses = numBytes / sizeof(DWORD);

    // Search for the pid
    for (i = 0; i < numProcesses; i++) {
        if (processes[i] == pid)
            return qtrue;
    }

    return qfalse;
}

/*
=================
Sys_DllExtension

Check if filename should be allowed to be loaded as a DLL.
=================
*/
qboolean Sys_DllExtension(const char* name) {
    return COM_CompareExtension(name, DLL_EXT);
}
