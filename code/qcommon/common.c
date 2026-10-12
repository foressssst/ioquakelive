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
#include "q_shared.h"
#include "qcommon.h"
#include "q_steam.h"
#include "q_zmq.h"
#include <setjmp.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/stat.h>
#else
#include <winsock.h>
#endif

int demo_protocols[] = {0};

#define MAX_NUM_ARGVS 50

#define MIN_DEDICATED_COMHUNKMEGS 1
#define MIN_COMHUNKMEGS 64
#define DEF_COMHUNKMEGS 128
#define DEF_COMZONEMEGS 64
#define DEF_COMHUNKMEGS_S XSTRING(DEF_COMHUNKMEGS)
#define DEF_COMZONEMEGS_S XSTRING(DEF_COMZONEMEGS)

int com_argc;
char* com_argv[MAX_NUM_ARGVS + 1];

jmp_buf abortframe;

FILE* debuglogfile;
static fileHandle_t pipefile;
static fileHandle_t logfile;
fileHandle_t com_journalFile;
fileHandle_t com_journalDataFile;

cvar_t* com_speeds;
cvar_t* com_developer;
cvar_t* com_dedicated;
cvar_t* com_timescale;
cvar_t* com_fixedtime;
cvar_t* com_journal;
cvar_t* com_maxfps;
cvar_t* com_timedemo;
cvar_t* com_sv_running;
cvar_t* com_cl_running;
cvar_t* com_logfile;
cvar_t* com_pipefile;
cvar_t* com_showtrace;
cvar_t* com_version;
cvar_t* com_blood;
cvar_t* cl_paused;
cvar_t* sv_paused;
cvar_t* cl_packetdelay;
cvar_t* sv_packetdelay;
cvar_t* com_cameraMode;
cvar_t* com_ansiColor;
cvar_t* com_unfocused;
cvar_t* com_maxfpsUnfocused;
cvar_t* com_minimized;
cvar_t* com_maxfpsMinimized;
cvar_t* com_gamename;
cvar_t* com_protocol;
cvar_t* com_basegame;
cvar_t* com_homepath;
cvar_t* com_busyWait;
cvar_t* com_maxfpsVRR;
cvar_t* com_workers;
static void Com_JobTest_f(void);
cvar_t* com_preciseFrameTiming;
static int64_t com_frameTargetUs;
static void Com_FrameTimes_f(void);
#ifndef DEDICATED
cvar_t* cl_allowConsoleChat;
cvar_t* com_allowConsole;
#endif

int time_game;
int time_frontend;
int time_backend;

int com_frameTime;
int com_frameNumber;

qboolean com_errorEntered = qfalse;
qboolean com_fullyInitialized = qfalse;
qboolean com_gameRestarting = qfalse;
qboolean com_gameClientRestarting = qfalse;

char com_errorMessage[MAXPRINTMSG];

void Com_WriteConfig_f(void);
void Com_WriteClientConfig_f(void);
void CIN_CloseAllVideos(void);

static char* rd_buffer;
static int rd_buffersize;
static void (*rd_flush)(char* buffer);

void Com_BeginRedirect(char* buffer, int buffersize, void (*flush)(char*)) {
    if (!buffer || !buffersize || !flush)
        return;
    rd_buffer = buffer;
    rd_buffersize = buffersize;
    rd_flush = flush;

    *rd_buffer = 0;
}

qboolean Com_Redirected(void) {
    return (qboolean)(rd_buffer != NULL);
}

void Com_EndRedirect(void) {
    if (rd_flush) {
        rd_flush(rd_buffer);
    }

    rd_buffer = NULL;
    rd_buffersize = 0;
    rd_flush = NULL;
}

#ifndef DEDICATED
static cvar_t* com_ignoreCrash;
static cvar_t* com_crashed;
#endif
static cvar_t* con_completionStyle;

static const char* Com_StripConsoleColors(const char* in, char* out, int outSize) {
    const char* s;
    int n = 0;

    for (s = in; *s; s++) {
        if (Q_IsConsoleColorString(s)) {
            break;
        }
    }
    if (!*s) {
        return in;
    }

    for (s = in; *s && n < outSize - 1; s++) {
        if (Q_IsConsoleColorString(s)) {
            s++;
            continue;
        }
        out[n++] = *s;
    }
    out[n] = '\0';

    return out;
}

Q_HOOKABLE void QDECL Com_Printf(const char* fmt, ...) {
    va_list argptr;
    char msg[MAXPRINTMSG];
    char plainBuffer[MAXPRINTMSG];
    const char* plain;
    static qboolean opening_qconsole = qfalse;

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    plain = Com_StripConsoleColors(msg, plainBuffer, sizeof(plainBuffer));

    if (rd_buffer) {
        if ((strlen(plain) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
            rd_flush(rd_buffer);
            *rd_buffer = 0;
        }
        Q_strcat(rd_buffer, rd_buffersize, plain);
        return;
    }

#ifndef DEDICATED
    CL_ConsolePrint(msg);
#endif

    Sys_Print((char*)plain);

    if (com_logfile && com_logfile->integer) {
        if (!logfile && FS_Initialized() && !opening_qconsole) {
            struct tm* newtime;
            time_t aclock;

            opening_qconsole = qtrue;

            time(&aclock);
            newtime = localtime(&aclock);

            logfile = FS_FOpenFileWrite("qconsole.log");

            if (logfile) {
                Com_Printf("logfile opened on %s\n", asctime(newtime));

                if (com_logfile->integer > 1) {
                    FS_ForceFlush(logfile);
                }
            } else {
                Com_Printf("Opening qconsole.log failed!\n");
                Cvar_SetValue("logfile", 0);
            }

            opening_qconsole = qfalse;
        }
        if (logfile && FS_Initialized()) {
            FS_Write(plain, strlen(plain), logfile);
        }
    }
}

void QDECL Com_DPrintf(const char* fmt, ...) {
    va_list argptr;
    char msg[MAXPRINTMSG];

    if (!com_developer || !com_developer->integer) {
        return;
    }

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Com_Printf("%s", msg);
}

void QDECL Com_Error(int code, const char* fmt, ...) {
    va_list argptr;
    static int lastErrorTime;
    static int errorCount;
    int currentTime;
    qboolean restartClient;

    if (com_errorEntered)
        Sys_Error("recursive error after: %s", com_errorMessage);

    com_errorEntered = qtrue;

    Cvar_Set("com_errorCode", va("%i", code));

    currentTime = Sys_Milliseconds();
    if (currentTime - lastErrorTime < 100) {
        if (++errorCount > 3) {
            code = ERR_FATAL;
        }
    } else {
        errorCount = 0;
    }
    lastErrorTime = currentTime;

    va_start(argptr, fmt);
    Q_vsnprintf(com_errorMessage, sizeof(com_errorMessage), fmt, argptr);
    va_end(argptr);

    if (code != ERR_DISCONNECT && code != ERR_NEED_CD)
        Cvar_Set("com_errorMessage", com_errorMessage);

    restartClient = com_gameClientRestarting && !(com_cl_running && com_cl_running->integer);

    com_gameRestarting = qfalse;
    com_gameClientRestarting = qfalse;

    if (code == ERR_DISCONNECT || code == ERR_SERVERDISCONNECT) {
        VM_Forced_Unload_Start();
        SV_Shutdown("Server disconnected");
        if (restartClient) {
            CL_Init();
        }
        CL_Disconnect(qtrue);
        CL_FlushMemory();
        VM_Forced_Unload_Done();
        FS_PureServerSetLoadedPaks("", "");
        com_errorEntered = qfalse;
        longjmp(abortframe, -1);
    } else if (code == ERR_DROP) {
        Com_Printf("********************\nERROR: %s\n********************\n", com_errorMessage);
        VM_Forced_Unload_Start();
        SV_Shutdown(va("Server crashed: %s", com_errorMessage));
        if (restartClient) {
            CL_Init();
        }
        CL_Disconnect(qtrue);
        CL_FlushMemory();
        VM_Forced_Unload_Done();
        FS_PureServerSetLoadedPaks("", "");
        com_errorEntered = qfalse;
        longjmp(abortframe, -1);
    } else if (code == ERR_NEED_CD) {
        VM_Forced_Unload_Start();
        SV_Shutdown("Server didn't have CD");
        if (restartClient) {
            CL_Init();
        }
        if (com_cl_running && com_cl_running->integer) {
            CL_Disconnect(qtrue);
            CL_FlushMemory();
            VM_Forced_Unload_Done();
        } else {
            Com_Printf("Server didn't have CD\n");
            VM_Forced_Unload_Done();
        }

        FS_PureServerSetLoadedPaks("", "");

        com_errorEntered = qfalse;
        longjmp(abortframe, -1);
    } else {
        VM_Forced_Unload_Start();
        CL_Shutdown(va("Client fatal crashed: %s", com_errorMessage), qtrue, qtrue);
        SV_Shutdown(va("Server fatal crashed: %s", com_errorMessage));
        VM_Forced_Unload_Done();
    }

    Com_Shutdown();

    Sys_Error("%s", com_errorMessage);
}

void Com_Quit_f(void) {
    char* p = Cmd_Args();
    if (!com_errorEntered) {
        VM_Forced_Unload_Start();
        SV_Shutdown(p[0] ? p : "Server quit");
        CL_Shutdown(p[0] ? p : "Client quit", qtrue, qtrue);
        VM_Forced_Unload_Done();
        Com_Shutdown();
        FS_Shutdown(qtrue);
    }
    Sys_Quit();
}

int com_numConsoleLines;
char** com_consoleLines;

void Com_ParseCommandLine(char* commandLine) {
    int inq = 0;
    int lines = 1;
    const char* p;

    for (p = commandLine; *p; p++) {
        if (*p == '"') {
            inq = !inq;
        }
        if ((*p == '+' && !inq) || *p == '\n' || *p == '\r') {
            lines++;
        }
    }
    com_consoleLines = malloc(lines * sizeof(*com_consoleLines));
    if (!com_consoleLines) {
        Sys_Error("Out of memory for the command line");
    }

    inq = 0;
    com_consoleLines[0] = commandLine;
    com_numConsoleLines = 1;

    while (*commandLine) {
        if (*commandLine == '"') {
            inq = !inq;
        }
        if ((*commandLine == '+' && !inq) || *commandLine == '\n' || *commandLine == '\r') {
            com_consoleLines[com_numConsoleLines] = commandLine + 1;
            com_numConsoleLines++;
            *commandLine = 0;
        }
        commandLine++;
    }
}

qboolean Com_SafeMode(void) {
    int i;

#ifndef DEDICATED
    if (com_crashed && com_crashed->integer && !com_ignoreCrash->integer) {
        return qtrue;
    }
#endif
    for (i = 0; i < com_numConsoleLines; i++) {
        Cmd_TokenizeString(com_consoleLines[i]);
        if (!Q_stricmp(Cmd_Argv(0), "safe") || !Q_stricmp(Cmd_Argv(0), "cvar_restart")) {
            com_consoleLines[i][0] = 0;
            return qtrue;
        }
    }
    return qfalse;
}

void Com_StartupVariable(const char* match) {
    int i;
    char* s;

    for (i = 0; i < com_numConsoleLines; i++) {
        Cmd_TokenizeString(com_consoleLines[i]);
        if (strcmp(Cmd_Argv(0), "set")) {
            continue;
        }

        s = Cmd_Argv(1);

        if (!match || !strcmp(s, match)) {
            if (Cvar_Flags(s) == CVAR_NONEXISTENT)
                Cvar_Get(s, Cmd_ArgsFrom(2), CVAR_USER_CREATED);
            else
                Cvar_Set2(s, Cmd_ArgsFrom(2), qfalse);
        }
    }
}

qboolean Com_AddStartupCommands(void) {
    int i;
    qboolean added;

    added = qfalse;
    for (i = 0; i < com_numConsoleLines; i++) {
        if (!com_consoleLines[i] || !com_consoleLines[i][0]) {
            continue;
        }

        if (!Q_stricmpn(com_consoleLines[i], "set ", 4)) {
            continue;
        }

        added = qtrue;
        Cbuf_AddText(com_consoleLines[i]);
        Cbuf_AddText("\n");
    }

    return added;
}

void Info_Print(const char* s) {
    char key[BIG_INFO_KEY];
    char value[BIG_INFO_VALUE];
    char* o;
    int l;

    if (*s == '\\')
        s++;
    while (*s) {
        o = key;
        while (*s && *s != '\\')
            *o++ = *s++;

        l = o - key;
        if (l < 20) {
            Com_Memset(o, ' ', 20 - l);
            key[20] = 0;
        } else
            *o = 0;
        Com_Printf("%s ", key);

        if (!*s) {
            Com_Printf("MISSING VALUE\n");
            return;
        }

        o = value;
        s++;
        while (*s && *s != '\\')
            *o++ = *s++;
        *o = 0;

        if (*s)
            s++;
        Com_Printf("%s\n", value);
    }
}

char* Com_StringContains(char* str1, char* str2, int casesensitive) {
    int len, i, j;

    len = strlen(str1) - strlen(str2);
    for (i = 0; i <= len; i++, str1++) {
        for (j = 0; str2[j]; j++) {
            if (casesensitive) {
                if (str1[j] != str2[j]) {
                    break;
                }
            } else {
                if (toupper(str1[j]) != toupper(str2[j])) {
                    break;
                }
            }
        }
        if (!str2[j]) {
            return str1;
        }
    }
    return NULL;
}

int Com_Filter(char* filter, char* name, int casesensitive) {
    char buf[MAX_TOKEN_CHARS];
    char* ptr;
    int i, found;

    while (*filter) {
        if (*filter == '*') {
            filter++;
            for (i = 0; *filter; i++) {
                if (*filter == '*' || *filter == '?')
                    break;
                buf[i] = *filter;
                filter++;
            }
            buf[i] = '\0';
            if (strlen(buf)) {
                ptr = Com_StringContains(name, buf, casesensitive);
                if (!ptr)
                    return qfalse;
                name = ptr + strlen(buf);
            }
        } else if (*filter == '?') {
            filter++;
            name++;
        } else if (*filter == '[' && *(filter + 1) == '[') {
            filter++;
        } else if (*filter == '[') {
            filter++;
            found = qfalse;
            while (*filter && !found) {
                if (*filter == ']' && *(filter + 1) != ']')
                    break;
                if (*(filter + 1) == '-' && *(filter + 2) && (*(filter + 2) != ']' || *(filter + 3) == ']')) {
                    if (casesensitive) {
                        if (*name >= *filter && *name <= *(filter + 2))
                            found = qtrue;
                    } else {
                        if (toupper(*name) >= toupper(*filter) &&
                            toupper(*name) <= toupper(*(filter + 2)))
                            found = qtrue;
                    }
                    filter += 3;
                } else {
                    if (casesensitive) {
                        if (*filter == *name)
                            found = qtrue;
                    } else {
                        if (toupper(*filter) == toupper(*name))
                            found = qtrue;
                    }
                    filter++;
                }
            }
            if (!found)
                return qfalse;
            while (*filter) {
                if (*filter == ']' && *(filter + 1) != ']')
                    break;
                filter++;
            }
            filter++;
            name++;
        } else {
            if (casesensitive) {
                if (*filter != *name)
                    return qfalse;
            } else {
                if (toupper(*filter) != toupper(*name))
                    return qfalse;
            }
            filter++;
            name++;
        }
    }
    return qtrue;
}

int Com_FilterPath(char* filter, char* name, int casesensitive) {
    int i;
    char new_filter[MAX_QPATH];
    char new_name[MAX_QPATH];

    for (i = 0; i < MAX_QPATH - 1 && filter[i]; i++) {
        if (filter[i] == '\\' || filter[i] == ':') {
            new_filter[i] = '/';
        } else {
            new_filter[i] = filter[i];
        }
    }
    new_filter[i] = '\0';
    for (i = 0; i < MAX_QPATH - 1 && name[i]; i++) {
        if (name[i] == '\\' || name[i] == ':') {
            new_name[i] = '/';
        } else {
            new_name[i] = name[i];
        }
    }
    new_name[i] = '\0';
    return Com_Filter(new_filter, new_name, casesensitive);
}

int Com_RealTime(qtime_t* qtime) {
    time_t t;
    struct tm* tms;

    t = time(NULL);
    if (!qtime)
        return t;
    tms = localtime(&t);
    if (tms) {
        qtime->tm_sec = tms->tm_sec;
        qtime->tm_min = tms->tm_min;
        qtime->tm_hour = tms->tm_hour;
        qtime->tm_mday = tms->tm_mday;
        qtime->tm_mon = tms->tm_mon;
        qtime->tm_year = tms->tm_year;
        qtime->tm_wday = tms->tm_wday;
        qtime->tm_yday = tms->tm_yday;
        qtime->tm_isdst = tms->tm_isdst;
    }
    return t;
}

#define ZONEID 0x1d4a11
#define MINFRAGMENT 64

typedef struct zonedebug_s {
    char* label;
    char* file;
    int line;
    int allocSize;
} zonedebug_t;

typedef struct memblock_s {
    int size;
    int tag;
    struct memblock_s *next, *prev;
    int id;
#ifdef ZONE_DEBUG
    zonedebug_t d;
#endif
} memblock_t;

typedef struct {
    int size;
    int used;
    memblock_t blocklist;
    memblock_t* rover;
} memzone_t;

static memzone_t* mainzone;
static memzone_t* smallzone;

static void Z_CheckHeap(void);

static void Z_ClearZone(memzone_t* zone, int size) {
    memblock_t* block;

    zone->blocklist.next = zone->blocklist.prev = block =
        (memblock_t*)((byte*)zone + sizeof(memzone_t));
    zone->blocklist.tag = 1;
    zone->blocklist.id = 0;
    zone->blocklist.size = 0;
    zone->rover = block;
    zone->size = size;
    zone->used = 0;

    block->prev = block->next = &zone->blocklist;
    block->tag = 0;
    block->id = ZONEID;
    block->size = size - sizeof(memzone_t);
}

static int Z_AvailableZoneMemory(memzone_t* zone) {
    return zone->size - zone->used;
}

int Z_AvailableMemory(void) {
    return Z_AvailableZoneMemory(mainzone);
}

void Z_Free(void* ptr) {
    memblock_t *block, *other;
    memzone_t* zone;

    if (!ptr) {
        Com_Error(ERR_DROP, "Z_Free: NULL pointer");
    }

    block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
    if (block->id != ZONEID) {
        Com_Error(ERR_FATAL, "Z_Free: freed a pointer without ZONEID");
    }
    if (block->tag == 0) {
        Com_Error(ERR_FATAL, "Z_Free: freed a freed pointer");
    }
    if (block->tag == TAG_STATIC) {
        return;
    }

    if (*(int*)((byte*)block + block->size - 4) != ZONEID) {
        Com_Error(ERR_FATAL, "Z_Free: memory block wrote past end");
    }

    if (block->tag == TAG_SMALL) {
        zone = smallzone;
    } else {
        zone = mainzone;
    }

    zone->used -= block->size;
    Com_Memset(ptr, 0xaa, block->size - sizeof(*block));

    block->tag = 0;

    other = block->prev;
    if (!other->tag) {
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;
        if (block == zone->rover) {
            zone->rover = other;
        }
        block = other;
    }

    zone->rover = block;

    other = block->next;
    if (!other->tag) {
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;
    }
}

void Z_FreeTags(int tag) {
    memzone_t* zone;

    if (tag == TAG_SMALL) {
        zone = smallzone;
    } else {
        zone = mainzone;
    }
    zone->rover = zone->blocklist.next;
    do {
        if (zone->rover->tag == tag) {
            Z_Free((void*)(zone->rover + 1));
            continue;
        }
        zone->rover = zone->rover->next;
    } while (zone->rover != &zone->blocklist);
}

#ifdef ZONE_DEBUG
void* Z_TagMallocDebug(int size, int tag, char* label, char* file, int line) {
    int allocSize;
#else
void* Z_TagMalloc(int size, int tag) {
#endif
    int extra;
    memblock_t *start, *rover, *new, *base;
    memzone_t* zone;

    if (!tag) {
        Com_Error(ERR_FATAL, "Z_TagMalloc: tried to use a 0 tag");
    }

    if (tag == TAG_SMALL) {
        zone = smallzone;
    } else {
        zone = mainzone;
    }

#ifdef ZONE_DEBUG
    allocSize = size;
#endif
    size += sizeof(memblock_t);
    size += 4;
    size = PAD(size, sizeof(intptr_t));

    base = rover = zone->rover;
    start = base->prev;

    do {
        if (rover == start) {
#ifdef ZONE_DEBUG
            Z_LogHeap();

            Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone: %s, line: %d (%s)",
                      size, zone == smallzone ? "small" : "main", file, line, label);
#else
            Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
                      size, zone == smallzone ? "small" : "main");
#endif
            return NULL;
        }
        if (rover->tag) {
            base = rover = rover->next;
        } else {
            rover = rover->next;
        }
    } while (base->tag || base->size < size);

    extra = base->size - size;
    if (extra > MINFRAGMENT) {
        new = (memblock_t*)((byte*)base + size);
        new->size = extra;
        new->tag = 0;
        new->prev = base;
        new->id = ZONEID;
        new->next = base->next;
        new->next->prev = new;
        base->next = new;
        base->size = size;
    }

    base->tag = tag;

    zone->rover = base->next;
    zone->used += base->size;

    base->id = ZONEID;

#ifdef ZONE_DEBUG
    base->d.label = label;
    base->d.file = file;
    base->d.line = line;
    base->d.allocSize = allocSize;
#endif

    *(int*)((byte*)base + base->size - 4) = ZONEID;

    return (void*)((byte*)base + sizeof(memblock_t));
}

#ifdef ZONE_DEBUG
void* Z_MallocDebug(int size, char* label, char* file, int line) {
#else
void* Z_Malloc(int size) {
#endif
    void* buf;

#ifdef ZONE_DEBUG
    buf = Z_TagMallocDebug(size, TAG_GENERAL, label, file, line);
#else
    buf = Z_TagMalloc(size, TAG_GENERAL);
#endif
    Com_Memset(buf, 0, size);

    return buf;
}

#ifdef ZONE_DEBUG
void* S_MallocDebug(int size, char* label, char* file, int line) {
    return Z_TagMallocDebug(size, TAG_SMALL, label, file, line);
}
#else
void* S_Malloc(int size) {
    return Z_TagMalloc(size, TAG_SMALL);
}
#endif

static void Z_CheckHeap(void) {
    memblock_t* block;

    for (block = mainzone->blocklist.next;; block = block->next) {
        if (block->next == &mainzone->blocklist) {
            break;
        }
        if ((byte*)block + block->size != (byte*)block->next)
            Com_Error(ERR_FATAL, "Z_CheckHeap: block size does not touch the next block");
        if (block->next->prev != block) {
            Com_Error(ERR_FATAL, "Z_CheckHeap: next block doesn't have proper back link");
        }
        if (!block->tag && !block->next->tag) {
            Com_Error(ERR_FATAL, "Z_CheckHeap: two consecutive free blocks");
        }
    }
}

void Z_LogZoneHeap(memzone_t* zone, char* name) {
#ifdef ZONE_DEBUG
    char dump[32], *ptr;
    int i, j;
#endif
    memblock_t* block;
    char buf[4096];
    int size, allocSize, numBlocks;

    if (!logfile || !FS_Initialized())
        return;
    size = numBlocks = 0;
#ifdef ZONE_DEBUG
    allocSize = 0;
#endif
    Com_sprintf(buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name);
    FS_Write(buf, strlen(buf), logfile);
    for (block = zone->blocklist.next; block->next != &zone->blocklist; block = block->next) {
        if (block->tag) {
#ifdef ZONE_DEBUG
            ptr = ((char*)block) + sizeof(memblock_t);
            j = 0;
            for (i = 0; i < 20 && i < block->d.allocSize; i++) {
                if (ptr[i] >= 32 && ptr[i] < 127) {
                    dump[j++] = ptr[i];
                } else {
                    dump[j++] = '_';
                }
            }
            dump[j] = '\0';
            Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
            FS_Write(buf, strlen(buf), logfile);
            allocSize += block->d.allocSize;
#endif
            size += block->size;
            numBlocks++;
        }
    }
#ifdef ZONE_DEBUG
    size -= numBlocks * sizeof(zonedebug_t);
#else
    allocSize = numBlocks * sizeof(memblock_t);
#endif
    Com_sprintf(buf, sizeof(buf), "%d %s memory in %d blocks\r\n", size, name, numBlocks);
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "%d %s memory overhead\r\n", size - allocSize, name);
    FS_Write(buf, strlen(buf), logfile);
}

void Z_LogHeap(void) {
    Z_LogZoneHeap(mainzone, "MAIN");
    Z_LogZoneHeap(smallzone, "SMALL");
}

typedef struct memstatic_s {
    memblock_t b;
    byte mem[2];
} memstatic_t;

memstatic_t emptystring =
    {{(sizeof(memblock_t) + 2 + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'\0', '\0'}};
memstatic_t numberstring[] = {
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'0', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'1', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'2', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'3', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'4', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'5', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'6', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'7', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'8', '\0'}},
    {{(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'9', '\0'}}};

char* CopyString(const char* in) {
    char* out;

    if (!in[0]) {
        return ((char*)&emptystring) + sizeof(memblock_t);
    } else if (!in[1]) {
        if (in[0] >= '0' && in[0] <= '9') {
            return ((char*)&numberstring[in[0] - '0']) + sizeof(memblock_t);
        }
    }
    out = S_Malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}

#define HUNK_MAGIC 0x89537892
#define HUNK_FREE_MAGIC 0x89537893

typedef struct {
    int magic;
    int size;
} hunkHeader_t;

typedef struct {
    int mark;
    int permanent;
    int temp;
    int tempHighwater;
} hunkUsed_t;

typedef struct hunkblock_s {
    int size;
    byte printed;
    struct hunkblock_s* next;
    char* label;
    char* file;
    int line;
} hunkblock_t;

static hunkblock_t* hunkblocks;

static hunkUsed_t hunk_low, hunk_high;
static hunkUsed_t *hunk_permanent, *hunk_temp;

static byte* s_hunkData = NULL;
static int s_hunkTotal;

static int s_zoneTotal;
static int s_smallZoneTotal;

void Com_Meminfo_f(void) {
    memblock_t* block;
    int zoneBytes, zoneBlocks;
    int smallZoneBytes;
    int botlibBytes, rendererBytes;
    int unused;

    zoneBytes = 0;
    botlibBytes = 0;
    rendererBytes = 0;
    zoneBlocks = 0;
    for (block = mainzone->blocklist.next;; block = block->next) {
        if (Cmd_Argc() != 1) {
            Com_Printf("block:%p    size:%7i    tag:%3i\n",
                       (void*)block, block->size, block->tag);
        }
        if (block->tag) {
            zoneBytes += block->size;
            zoneBlocks++;
            if (block->tag == TAG_BOTLIB) {
                botlibBytes += block->size;
            } else if (block->tag == TAG_RENDERER) {
                rendererBytes += block->size;
            }
        }

        if (block->next == &mainzone->blocklist) {
            break;
        }
        if ((byte*)block + block->size != (byte*)block->next) {
            Com_Printf("ERROR: block size does not touch the next block\n");
        }
        if (block->next->prev != block) {
            Com_Printf("ERROR: next block doesn't have proper back link\n");
        }
        if (!block->tag && !block->next->tag) {
            Com_Printf("ERROR: two consecutive free blocks\n");
        }
    }

    smallZoneBytes = 0;
    for (block = smallzone->blocklist.next;; block = block->next) {
        if (block->tag) {
            smallZoneBytes += block->size;
        }

        if (block->next == &smallzone->blocklist) {
            break;
        }
    }

    Com_Printf("%8i bytes total hunk\n", s_hunkTotal);
    Com_Printf("%8i bytes total zone\n", s_zoneTotal);
    Com_Printf("\n");
    Com_Printf("%8i low mark\n", hunk_low.mark);
    Com_Printf("%8i low permanent\n", hunk_low.permanent);
    if (hunk_low.temp != hunk_low.permanent) {
        Com_Printf("%8i low temp\n", hunk_low.temp);
    }
    Com_Printf("%8i low tempHighwater\n", hunk_low.tempHighwater);
    Com_Printf("\n");
    Com_Printf("%8i high mark\n", hunk_high.mark);
    Com_Printf("%8i high permanent\n", hunk_high.permanent);
    if (hunk_high.temp != hunk_high.permanent) {
        Com_Printf("%8i high temp\n", hunk_high.temp);
    }
    Com_Printf("%8i high tempHighwater\n", hunk_high.tempHighwater);
    Com_Printf("\n");
    Com_Printf("%8i total hunk in use\n", hunk_low.permanent + hunk_high.permanent);
    unused = 0;
    if (hunk_low.tempHighwater > hunk_low.permanent) {
        unused += hunk_low.tempHighwater - hunk_low.permanent;
    }
    if (hunk_high.tempHighwater > hunk_high.permanent) {
        unused += hunk_high.tempHighwater - hunk_high.permanent;
    }
    Com_Printf("%8i unused highwater\n", unused);
    Com_Printf("\n");
    Com_Printf("%8i bytes in %i zone blocks\n", zoneBytes, zoneBlocks);
    Com_Printf("        %8i bytes in dynamic botlib\n", botlibBytes);
    Com_Printf("        %8i bytes in dynamic renderer\n", rendererBytes);
    Com_Printf("        %8i bytes in dynamic other\n", zoneBytes - (botlibBytes + rendererBytes));
    Com_Printf("        %8i bytes in small Zone memory\n", smallZoneBytes);
}

void Com_TouchMemory(void) {
    int start, end;
    int i, j;
    unsigned sum;
    memblock_t* block;

    Z_CheckHeap();

    start = Sys_Milliseconds();

    sum = 0;

    j = hunk_low.permanent >> 2;
    for (i = 0; i < j; i += 64) {
        sum += ((int*)s_hunkData)[i];
    }

    i = (s_hunkTotal - hunk_high.permanent) >> 2;
    j = hunk_high.permanent >> 2;
    for (; i < j; i += 64) {
        sum += ((int*)s_hunkData)[i];
    }

    for (block = mainzone->blocklist.next;; block = block->next) {
        if (block->tag) {
            j = block->size >> 2;
            for (i = 0; i < j; i += 64) {
                sum += ((int*)block)[i];
            }
        }
        if (block->next == &mainzone->blocklist) {
            break;
        }
    }

    end = Sys_Milliseconds();

    Com_DPrintf("Com_TouchMemory: %i msec\n", end - start);
}

void Com_InitSmallZoneMemory(void) {
    s_smallZoneTotal = 512 * 1024;
    smallzone = calloc(s_smallZoneTotal, 1);
    if (!smallzone) {
        Com_Error(ERR_FATAL, "Small zone data failed to allocate %1.1f megs", (float)s_smallZoneTotal / (1024 * 1024));
    }
    Z_ClearZone(smallzone, s_smallZoneTotal);
}

void Com_InitZoneMemory(void) {
    cvar_t* cv;

    cv = Cvar_Get("com_zoneMegs", DEF_COMZONEMEGS_S, CVAR_LATCH | CVAR_ARCHIVE);

    if (cv->integer < DEF_COMZONEMEGS) {
        s_zoneTotal = 1024 * 1024 * DEF_COMZONEMEGS;
    } else {
        s_zoneTotal = cv->integer * 1024 * 1024;
    }

    mainzone = calloc(s_zoneTotal, 1);
    if (!mainzone) {
        Com_Error(ERR_FATAL, "Zone data failed to allocate %i megs", s_zoneTotal / (1024 * 1024));
    }
    Z_ClearZone(mainzone, s_zoneTotal);
}

void Hunk_Log(void) {
    hunkblock_t* block;
    char buf[4096];
    int size, numBlocks;

    if (!logfile || !FS_Initialized())
        return;
    size = 0;
    numBlocks = 0;
    Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
    FS_Write(buf, strlen(buf), logfile);
    for (block = hunkblocks; block; block = block->next) {
#ifdef HUNK_DEBUG
        Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", block->size, block->file, block->line, block->label);
        FS_Write(buf, strlen(buf), logfile);
#endif
        size += block->size;
        numBlocks++;
    }
    Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
    FS_Write(buf, strlen(buf), logfile);
}

void Hunk_SmallLog(void) {
    hunkblock_t *block, *block2;
    char buf[4096];
    int size, locsize, numBlocks;

    if (!logfile || !FS_Initialized())
        return;
    for (block = hunkblocks; block; block = block->next) {
        block->printed = qfalse;
    }
    size = 0;
    numBlocks = 0;
    Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
    FS_Write(buf, strlen(buf), logfile);
    for (block = hunkblocks; block; block = block->next) {
        if (block->printed) {
            continue;
        }
        locsize = block->size;
        for (block2 = block->next; block2; block2 = block2->next) {
            if (block->line != block2->line) {
                continue;
            }
            if (Q_stricmp(block->file, block2->file)) {
                continue;
            }
            size += block2->size;
            locsize += block2->size;
            block2->printed = qtrue;
        }
#ifdef HUNK_DEBUG
        Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", locsize, block->file, block->line, block->label);
        FS_Write(buf, strlen(buf), logfile);
#endif
        size += block->size;
        numBlocks++;
    }
    Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
    FS_Write(buf, strlen(buf), logfile);
}

void Com_InitHunkMemory(void) {
    cvar_t* cv;
    int nMinAlloc;
    char* pMsg = NULL;

    if (FS_LoadStack() != 0) {
        Com_Error(ERR_FATAL, "Hunk initialization failed. File system load stack not zero");
    }

    cv = Cvar_Get("com_hunkMegs", DEF_COMHUNKMEGS_S, CVAR_LATCH | CVAR_ARCHIVE);
    Cvar_SetDescription(cv, "The size of the hunk memory segment");

    if (com_dedicated && com_dedicated->integer) {
        nMinAlloc = MIN_DEDICATED_COMHUNKMEGS;
        pMsg = "Minimum com_hunkMegs for a dedicated server is %i, allocating %i megs.\n";
    } else {
        nMinAlloc = MIN_COMHUNKMEGS;
        pMsg = "Minimum com_hunkMegs is %i, allocating %i megs.\n";
    }

    if (cv->integer < nMinAlloc) {
        s_hunkTotal = 1024 * 1024 * nMinAlloc;
        Com_Printf(pMsg, nMinAlloc, s_hunkTotal / (1024 * 1024));
    } else {
        s_hunkTotal = cv->integer * 1024 * 1024;
    }

    s_hunkData = calloc(s_hunkTotal + 31, 1);
    if (!s_hunkData) {
        Com_Error(ERR_FATAL, "Hunk data failed to allocate %i megs", s_hunkTotal / (1024 * 1024));
    }
    s_hunkData = (byte*)(((intptr_t)s_hunkData + 31) & ~31);
    Hunk_Clear();

    Cmd_AddCommand("meminfo", Com_Meminfo_f);
#ifdef ZONE_DEBUG
    Cmd_AddCommand("zonelog", Z_LogHeap);
#endif
#ifdef HUNK_DEBUG
    Cmd_AddCommand("hunklog", Hunk_Log);
    Cmd_AddCommand("hunksmalllog", Hunk_SmallLog);
#endif
}

int Hunk_MemoryRemaining(void) {
    int low, high;

    low = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
    high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

    return s_hunkTotal - (low + high);
}

void Hunk_SetMark(void) {
    hunk_low.mark = hunk_low.permanent;
    hunk_high.mark = hunk_high.permanent;
}

void Hunk_ClearToMark(void) {
    hunk_low.permanent = hunk_low.temp = hunk_low.mark;
    hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}

qboolean Hunk_CheckMark(void) {
    if (hunk_low.mark || hunk_high.mark) {
        return qtrue;
    }
    return qfalse;
}

void CL_ShutdownCGame(void);
void CL_ShutdownUI(void);
void SV_ShutdownGameProgs(void);

void Hunk_Clear(void) {
#ifndef DEDICATED
    CL_ShutdownCGame();
    CL_ShutdownUI();
#endif
    SV_ShutdownGameProgs();
#ifndef DEDICATED
    CIN_CloseAllVideos();
#endif
    hunk_low.mark = 0;
    hunk_low.permanent = 0;
    hunk_low.temp = 0;
    hunk_low.tempHighwater = 0;

    hunk_high.mark = 0;
    hunk_high.permanent = 0;
    hunk_high.temp = 0;
    hunk_high.tempHighwater = 0;

    hunk_permanent = &hunk_low;
    hunk_temp = &hunk_high;

    Com_Printf("Hunk_Clear: reset the hunk ok\n");
    VM_Clear();
#ifdef HUNK_DEBUG
    hunkblocks = NULL;
#endif
}

static void Hunk_SwapBanks(void) {
    hunkUsed_t* swap;

    if (hunk_temp->temp != hunk_temp->permanent) {
        return;
    }

    if (hunk_temp->tempHighwater - hunk_temp->permanent >
        hunk_permanent->tempHighwater - hunk_permanent->permanent) {
        swap = hunk_temp;
        hunk_temp = hunk_permanent;
        hunk_permanent = swap;
    }
}

#ifdef HUNK_DEBUG
void* Hunk_AllocDebug(int size, ha_pref preference, char* label, char* file, int line) {
#else
void* Hunk_Alloc(int size, ha_pref preference) {
#endif
    void* buf;

    if (s_hunkData == NULL) {
        Com_Error(ERR_FATAL, "Hunk_Alloc: Hunk memory system not initialized");
    }

    if (preference == h_dontcare || hunk_temp->temp != hunk_temp->permanent) {
        Hunk_SwapBanks();
    } else {
        if (preference == h_low && hunk_permanent != &hunk_low) {
            Hunk_SwapBanks();
        } else if (preference == h_high && hunk_permanent != &hunk_high) {
            Hunk_SwapBanks();
        }
    }

#ifdef HUNK_DEBUG
    size += sizeof(hunkblock_t);
#endif

    size = (size + 31) & ~31;

    if (hunk_low.temp + hunk_high.temp + size > s_hunkTotal) {
#ifdef HUNK_DEBUG
        Hunk_Log();
        Hunk_SmallLog();

        Com_Error(ERR_DROP, "Hunk_Alloc failed on %i: %s, line: %d (%s)", size, file, line, label);
#else
        Com_Error(ERR_DROP, "Hunk_Alloc failed on %i", size);
#endif
    }

    if (hunk_permanent == &hunk_low) {
        buf = (void*)(s_hunkData + hunk_permanent->permanent);
        hunk_permanent->permanent += size;
    } else {
        hunk_permanent->permanent += size;
        buf = (void*)(s_hunkData + s_hunkTotal - hunk_permanent->permanent);
    }

    hunk_permanent->temp = hunk_permanent->permanent;

    Com_Memset(buf, 0, size);

#ifdef HUNK_DEBUG
    {
        hunkblock_t* block;

        block = (hunkblock_t*)buf;
        block->size = size - sizeof(hunkblock_t);
        block->file = file;
        block->label = label;
        block->line = line;
        block->next = hunkblocks;
        hunkblocks = block;
        buf = ((byte*)buf) + sizeof(hunkblock_t);
    }
#endif
    return buf;
}

void* Hunk_AllocateTempMemory(int size) {
    void* buf;
    hunkHeader_t* hdr;

    if (s_hunkData == NULL) {
        return Z_Malloc(size);
    }

    Hunk_SwapBanks();

    size = PAD(size, sizeof(intptr_t)) + sizeof(hunkHeader_t);

    if (hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal) {
        Com_Error(ERR_DROP, "Hunk_AllocateTempMemory: failed on %i", size);
    }

    if (hunk_temp == &hunk_low) {
        buf = (void*)(s_hunkData + hunk_temp->temp);
        hunk_temp->temp += size;
    } else {
        hunk_temp->temp += size;
        buf = (void*)(s_hunkData + s_hunkTotal - hunk_temp->temp);
    }

    if (hunk_temp->temp > hunk_temp->tempHighwater) {
        hunk_temp->tempHighwater = hunk_temp->temp;
    }

    hdr = (hunkHeader_t*)buf;
    buf = (void*)(hdr + 1);

    hdr->magic = HUNK_MAGIC;
    hdr->size = size;

    return buf;
}

void Hunk_FreeTempMemory(void* buf) {
    hunkHeader_t* hdr;

    if (s_hunkData == NULL) {
        Z_Free(buf);
        return;
    }

    hdr = ((hunkHeader_t*)buf) - 1;
    if (hdr->magic != HUNK_MAGIC) {
        Com_Error(ERR_FATAL, "Hunk_FreeTempMemory: bad magic");
    }

    hdr->magic = HUNK_FREE_MAGIC;

    if (hunk_temp == &hunk_low) {
        if (hdr == (void*)(s_hunkData + hunk_temp->temp - hdr->size)) {
            hunk_temp->temp -= hdr->size;
        } else {
            Com_Printf("Hunk_FreeTempMemory: not the final block\n");
        }
    } else {
        if (hdr == (void*)(s_hunkData + s_hunkTotal - hunk_temp->temp)) {
            hunk_temp->temp -= hdr->size;
        } else {
            Com_Printf("Hunk_FreeTempMemory: not the final block\n");
        }
    }
}

void Hunk_ClearTempMemory(void) {
    if (s_hunkData != NULL) {
        hunk_temp->temp = hunk_temp->permanent;
    }
}

#define MAX_PUSHED_EVENTS 1024
static int com_pushedEventsHead = 0;
static int com_pushedEventsTail = 0;
static sysEvent_t com_pushedEvents[MAX_PUSHED_EVENTS];

void Com_InitJournaling(void) {
    Com_StartupVariable("journal");
    com_journal = Cvar_Get("journal", "0", CVAR_INIT);
    if (!com_journal->integer) {
        return;
    }

    if (com_journal->integer == 1) {
        Com_Printf("Journaling events\n");
        com_journalFile = FS_FOpenFileWrite("journal.dat");
        com_journalDataFile = FS_FOpenFileWrite("journaldata.dat");
    } else if (com_journal->integer == 2) {
        Com_Printf("Replaying journaled events\n");
        FS_FOpenFileRead("journal.dat", &com_journalFile, qtrue);
        FS_FOpenFileRead("journaldata.dat", &com_journalDataFile, qtrue);
    }

    if (!com_journalFile || !com_journalDataFile) {
        Cvar_Set("com_journal", "0");
        com_journalFile = 0;
        com_journalDataFile = 0;
        Com_Printf("Couldn't open journal files\n");
    }
}

#define MAX_QUEUED_EVENTS 256
#define MASK_QUEUED_EVENTS (MAX_QUEUED_EVENTS - 1)

static sysEvent_t eventQueue[MAX_QUEUED_EVENTS];
static int eventHead = 0;
static int eventTail = 0;

void Com_QueueEvent(int time, sysEventType_t type, int value, int value2, int ptrLength, void* ptr) {
    sysEvent_t* ev;

    if (type == SE_MOUSE && eventHead != eventTail) {
        ev = &eventQueue[(eventHead + MAX_QUEUED_EVENTS - 1) & MASK_QUEUED_EVENTS];

        if (ev->evType == SE_MOUSE) {
            ev->evValue += value;
            ev->evValue2 += value2;
            return;
        }
    }

    if (type == SE_MOUSE_ABS && eventHead != eventTail) {
        ev = &eventQueue[(eventHead + MAX_QUEUED_EVENTS - 1) & MASK_QUEUED_EVENTS];

        if (ev->evType == SE_MOUSE_ABS) {
            ev->evValue = value;
            ev->evValue2 = value2;
            return;
        }
    }

    ev = &eventQueue[eventHead & MASK_QUEUED_EVENTS];

    if (eventHead - eventTail >= MAX_QUEUED_EVENTS) {
        Com_Printf("Com_QueueEvent: overflow\n");
        if (ev->evPtr) {
            Z_Free(ev->evPtr);
        }
        eventTail++;
    }

    eventHead++;

    if (time == 0) {
        time = Sys_Milliseconds();
    }

    ev->evTime = time;
    ev->evType = type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
}

sysEvent_t Com_GetSystemEvent(void) {
    sysEvent_t ev;
    char* s;

    if (eventHead > eventTail) {
        eventTail++;
        return eventQueue[(eventTail - 1) & MASK_QUEUED_EVENTS];
    }

    s = Sys_ConsoleInput();
    if (s) {
        char* b;
        int len;

        len = strlen(s) + 1;
        b = Z_Malloc(len);
        strcpy(b, s);
        Com_QueueEvent(0, SE_CONSOLE, 0, 0, len, b);
    }

    if (eventHead > eventTail) {
        eventTail++;
        return eventQueue[(eventTail - 1) & MASK_QUEUED_EVENTS];
    }

    memset(&ev, 0, sizeof(ev));
    ev.evTime = Sys_Milliseconds();

    return ev;
}

sysEvent_t Com_GetRealEvent(void) {
    int r;
    sysEvent_t ev;

    if (com_journal->integer == 2) {
        r = FS_Read(&ev, sizeof(ev), com_journalFile);
        if (r != sizeof(ev)) {
            Com_Error(ERR_FATAL, "Error reading from journal file");
        }
        if (ev.evPtrLength) {
            ev.evPtr = Z_Malloc(ev.evPtrLength);
            r = FS_Read(ev.evPtr, ev.evPtrLength, com_journalFile);
            if (r != ev.evPtrLength) {
                Com_Error(ERR_FATAL, "Error reading from journal file");
            }
        }
    } else {
        ev = Com_GetSystemEvent();

        if (com_journal->integer == 1) {
            r = FS_Write(&ev, sizeof(ev), com_journalFile);
            if (r != sizeof(ev)) {
                Com_Error(ERR_FATAL, "Error writing to journal file");
            }
            if (ev.evPtrLength) {
                r = FS_Write(ev.evPtr, ev.evPtrLength, com_journalFile);
                if (r != ev.evPtrLength) {
                    Com_Error(ERR_FATAL, "Error writing to journal file");
                }
            }
        }
    }

    return ev;
}

void Com_InitPushEvent(void) {
    memset(com_pushedEvents, 0, sizeof(com_pushedEvents));
    com_pushedEventsHead = 0;
    com_pushedEventsTail = 0;
}

void Com_PushEvent(sysEvent_t* event) {
    sysEvent_t* ev;
    static int printedWarning = 0;

    ev = &com_pushedEvents[com_pushedEventsHead & (MAX_PUSHED_EVENTS - 1)];

    if (com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS) {
        if (!printedWarning) {
            printedWarning = qtrue;
            Com_Printf("WARNING: Com_PushEvent overflow\n");
        }

        if (ev->evPtr) {
            Z_Free(ev->evPtr);
        }
        com_pushedEventsTail++;
    } else {
        printedWarning = qfalse;
    }

    *ev = *event;
    com_pushedEventsHead++;
}

sysEvent_t Com_GetEvent(void) {
    if (com_pushedEventsHead > com_pushedEventsTail) {
        com_pushedEventsTail++;
        return com_pushedEvents[(com_pushedEventsTail - 1) & (MAX_PUSHED_EVENTS - 1)];
    }
    return Com_GetRealEvent();
}

void Com_RunAndTimeServerPacket(netadr_t* evFrom, msg_t* buf) {
    int t1, t2, msec;

    t1 = 0;

    if (com_speeds->integer) {
        t1 = Sys_Milliseconds();
    }

    SV_PacketEvent(*evFrom, buf);

    if (com_speeds->integer) {
        t2 = Sys_Milliseconds();
        msec = t2 - t1;
        if (com_speeds->integer == 3) {
            Com_Printf("SV_PacketEvent time: %i\n", msec);
        }
    }
}

int Com_EventLoop(void) {
    sysEvent_t ev;
    netadr_t evFrom;
    byte bufData[MAX_MSGLEN];
    msg_t buf;

    MSG_Init(&buf, bufData, sizeof(bufData));

    while (1) {
        ev = Com_GetEvent();

        if (ev.evType == SE_NONE) {
            while (NET_GetLoopPacket(NS_CLIENT, &evFrom, &buf)) {
                CL_PacketEvent(evFrom, &buf);
            }

            while (NET_GetLoopPacket(NS_SERVER, &evFrom, &buf)) {
                if (com_sv_running->integer) {
                    Com_RunAndTimeServerPacket(&evFrom, &buf);
                }
            }

            return ev.evTime;
        }

        switch (ev.evType) {
            case SE_KEY:
                CL_KeyEvent(ev.evValue, ev.evValue2, ev.evTime);
                break;
            case SE_CHAR:
                CL_CharEvent(ev.evValue);
                break;
            case SE_MOUSE:
                CL_MouseEvent(ev.evValue, ev.evValue2, ev.evTime);
                break;
            case SE_MOUSE_ABS:
                CL_MouseAbsEvent(ev.evValue, ev.evValue2);
                break;
            case SE_JOYSTICK_AXIS:
                CL_JoystickEvent(ev.evValue, ev.evValue2, ev.evTime);
                break;
            case SE_CONSOLE:
                Cbuf_AddText((char*)ev.evPtr);
                Cbuf_AddText("\n");
                break;
            default:
                Com_Error(ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType);
                break;
        }

        if (ev.evPtr) {
            Z_Free(ev.evPtr);
        }
    }

    return 0;
}

int Com_Milliseconds(void) {
    sysEvent_t ev;

    do {
        ev = Com_GetRealEvent();
        if (ev.evType != SE_NONE) {
            Com_PushEvent(&ev);
        }
    } while (ev.evType != SE_NONE);

    return ev.evTime;
}

static void __attribute__((__noreturn__)) Com_Error_f(void) {
    if (Cmd_Argc() > 1) {
        Com_Error(ERR_DROP, "Testing drop error");
    } else {
        Com_Error(ERR_FATAL, "Testing fatal error");
    }
}

static void Com_Freeze_f(void) {
    float s;
    int start, now;

    if (Cmd_Argc() != 2) {
        Com_Printf("freeze <seconds>\n");
        return;
    }
    s = atof(Cmd_Argv(1));

    start = Com_Milliseconds();

    while (1) {
        now = Com_Milliseconds();
        if ((now - start) * 0.001 > s) {
            break;
        }
    }
}

static void Com_Crash_f(void) {
    *(volatile int*)0 = 0x12345678;
}

void Com_Setenv_f(void) {
    int argc = Cmd_Argc();
    char* arg1 = Cmd_Argv(1);

    if (argc > 2) {
        char* arg2 = Cmd_ArgsFrom(2);

        Sys_SetEnv(arg1, arg2);
    } else if (argc == 2) {
        char* env = getenv(arg1);

        if (env)
            Com_Printf("%s=%s\n", arg1, env);
        else
            Com_Printf("%s undefined\n", arg1);
    }
}

#define COM_CONFIG_VERSION 4

static void Com_MigrateConfig(void) {
    cvar_t* version = Cvar_Get("com_configVersion", "0", CVAR_ARCHIVE);

    if (version->integer < 1) {
        if (Cvar_Flags("con_style") != CVAR_NONEXISTENT)
            Cvar_Set("con_style", "1");
        if (Cvar_Flags("r_autoExposure") != CVAR_NONEXISTENT)
            Cvar_Set("r_autoExposure", "0");
    }

    if (version->integer < 2) {
        static const struct {
            const char* name;
            const char* oldValue;
            const char* newValue;
        } defaults[] = {
            {"r_picmip", "1", "0"},
            {"r_lodbias", "0", "-2"},
            {"r_texturebits", "0", "32"},
            {"r_colorbits", "0", "32"},
            {"r_stencilbits", "8", "0"},
            {"r_hdr", "1", "0"},
            {"r_toneMap", "1", "0"},
            {"r_depthPrepass", "1", "0"},
            {"r_normalMapping", "1", "0"},
            {"r_specularMapping", "1", "0"},
            {"r_deluxeMapping", "1", "0"},
            {"r_sunlightMode", "1", "0"},
            {"r_sunShadows", "1", "0"},
        };
        int i;

        for (i = 0; i < ARRAY_LEN(defaults); i++) {
            if (Cvar_Flags(defaults[i].name) != CVAR_NONEXISTENT &&
                !strcmp(Cvar_VariableString(defaults[i].name), defaults[i].oldValue))
                Cvar_Set(defaults[i].name, defaults[i].newValue);
        }
    }

    if (version->integer < 3) {
        static const struct {
            const char* name;
            const char* oldValue;
            const char* newValue;
        } defaults[] = {
            {"sensitivity", "5", "4"},
            {"cl_mouseAccelOffset", "5", "0"},
            {"in_mouse", "1", "2"},
        };
        int i;

        for (i = 0; i < ARRAY_LEN(defaults); i++) {
            if (Cvar_Flags(defaults[i].name) != CVAR_NONEXISTENT &&
                !strcmp(Cvar_VariableString(defaults[i].name), defaults[i].oldValue))
                Cvar_Set(defaults[i].name, defaults[i].newValue);
        }
    }

    if (version->integer < 4) {
        static const struct {
            const char* name;
            const char* oldValue;
            const char* newValue;
        } defaults[] = {
            {"com_maxfpsUnfocused", "0", "60"},
            {"com_maxfpsMinimized", "0", "10"},
        };
        int i;

        for (i = 0; i < ARRAY_LEN(defaults); i++) {
            if (Cvar_Flags(defaults[i].name) != CVAR_NONEXISTENT &&
                !strcmp(Cvar_VariableString(defaults[i].name), defaults[i].oldValue))
                Cvar_Set(defaults[i].name, defaults[i].newValue);
        }
    }

    if (version->integer != COM_CONFIG_VERSION)
        Cvar_Set("com_configVersion", va("%i", COM_CONFIG_VERSION));
}

#ifndef DEDICATED
static void Com_CheckCrash(void) {
    com_ignoreCrash = Cvar_Get("com_ignoreCrash", "0", 0);
    com_crashed = Cvar_Get("com_crashed", "0", CVAR_ROM);

    if (Sys_WritePIDFile(BASEGAME_DIR) && !com_ignoreCrash->integer) {
        Cvar_Set("com_crashed", "1");
    }
    if (!com_crashed->integer) {
        return;
    }
    if (FS_ReadFile(QZCONFIG_CFG, NULL) <= 0) {
        Cvar_Set("com_ignoreCrash", "1");
        return;
    }
    if (Sys_Dialog(DT_YES_NO, "Quake Live didn't quit properly the last time it ran.\n"
                              "Would you like to launch the game in safe mode?\n"
                              "It will change your system settings, but not your controls.",
                   "Enable Safe Mode?") != DR_YES) {
        Cvar_Set("com_ignoreCrash", "1");
    }
}
#endif

void Com_ExecuteCfg(void) {
    const qboolean safe = Com_SafeMode();
    qboolean readConfig = qfalse;

    Cbuf_ExecuteText(EXEC_NOW, "exec default.cfg\n");
    Cbuf_Execute();

    {
#ifdef DEDICATED
        const qboolean dedicated = qtrue;
#else
        const qboolean dedicated = (qboolean)(Cvar_VariableIntegerValue("dedicated") != 0);
#endif

        if (dedicated) {
            Cbuf_ExecuteText(EXEC_NOW, "exec server.cfg\n");
            Cbuf_Execute();
        } else {
            if (!safe) {
                readConfig = (qboolean)(FS_ReadFile(QZCONFIG_CFG, NULL) > 0);
                Cbuf_ExecuteText(EXEC_NOW, "exec " QZCONFIG_CFG "\n");
                Cbuf_Execute();
            }
#ifdef REPCONFIG_CFG
            Cbuf_ExecuteText(EXEC_NOW, "exec " REPCONFIG_CFG "\n");
            Cbuf_Execute();
#endif
        }

        Cbuf_ExecuteText(EXEC_NOW, "exec autoexec.cfg\n");
        Cbuf_Execute();

        if (readConfig) {
            Com_MigrateConfig();
        } else {
            Cvar_Get("com_configVersion", va("%i", COM_CONFIG_VERSION), CVAR_ARCHIVE);
        }
    }
}

void Com_GameRestart(int checksumFeed, qboolean disconnect) {
    if (!com_gameRestarting && com_fullyInitialized) {
        com_gameRestarting = qtrue;
        com_gameClientRestarting = com_cl_running->integer;

        if (com_sv_running->integer)
            SV_Shutdown("Game directory changed");

        if (com_gameClientRestarting) {
            if (disconnect)
                CL_Disconnect(qfalse);

            CL_Shutdown("Game directory changed", disconnect, qfalse);
        }

        FS_Restart(checksumFeed);

        Cvar_Restart(qtrue);
        Com_ExecuteCfg();

        if (disconnect) {
            NET_Restart_f();
        }

        if (com_gameClientRestarting) {
            CL_Init();
            CL_StartHunkUsers(qfalse);
        }

        com_gameRestarting = qfalse;
        com_gameClientRestarting = qfalse;
    }
}

static void Com_InitRand(void) {
    unsigned int seed;

    if (Sys_RandomBytes((byte*)&seed, sizeof(seed)))
        srand(seed);
    else
        srand(time(NULL));
}

void Com_Init(char* commandLine) {
    char* s;
    int qport;

    Com_Printf("%s %s\n", FULL_PRODUCT_VERSION, PLATFORM_STRING);

    if (setjmp(abortframe)) {
        Sys_Error("Error during initialization");
    }

    Com_Memset(&eventQueue[0], 0, MAX_QUEUED_EVENTS * sizeof(sysEvent_t));

    Com_InitRand();

    Com_InitPushEvent();

    Com_InitSmallZoneMemory();
    Cvar_Init();

    Com_ParseCommandLine(commandLine);

    Cbuf_Init();

    Com_StartupVariable(NULL);

    Com_InitZoneMemory();
    Cmd_Init();

    com_developer = Cvar_Get("developer", "0", CVAR_TEMP);

    CL_InitKeyCommands();

    com_basegame = Cvar_Get("com_basegame", BASEGAME_DIR, CVAR_INIT);
    com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT | CVAR_PROTECTED);

#ifndef DEDICATED
    if (!Cvar_VariableIntegerValue("dedicated")) {
        Steam_Init();
    }
#endif
    FS_InitFilesystem();

    Com_InitJournaling();

    Cmd_AddCommand("setenv", Com_Setenv_f);
    if (com_developer && com_developer->integer) {
        Cmd_AddCommand("error", Com_Error_f);
        Cmd_AddCommand("crash", Com_Crash_f);
        Cmd_AddCommand("freeze", Com_Freeze_f);
    }
    Cmd_AddCommand("quit", Com_Quit_f);
    Cmd_AddCommand("frametimes", Com_FrameTimes_f);
    Cmd_AddCommand("jobtest", Com_JobTest_f);
    Cmd_AddCommand("changeVectors", MSG_ReportChangeVectors_f);
    Cmd_AddCommand("writeconfig", Com_WriteConfig_f);
    Cmd_SetCommandCompletionFunc("writeconfig", Cmd_CompleteCfgName);
    Cmd_AddCommand("writeClientConfig", Com_WriteClientConfig_f);

#ifndef DEDICATED
    Com_CheckCrash();
#endif

    SV_RegisterGameCvars();
    CL_RegisterCGameCvars();

    Com_ExecuteCfg();

    Com_StartupVariable(NULL);

#ifdef DEDICATED
    com_dedicated = Cvar_Get("dedicated", "1", CVAR_INIT);
    Cvar_CheckRange(com_dedicated, 1, 2, qtrue);
#else
    com_dedicated = Cvar_Get("dedicated", "0", CVAR_LATCH);
    Cvar_CheckRange(com_dedicated, 0, 2, qtrue);
#endif
    Com_InitHunkMemory();

    cvar_modifiedFlags &= ~CVAR_ARCHIVE;

#ifdef __EMSCRIPTEN__
    com_maxfps = Cvar_Get("com_maxfps", "0", CVAR_ARCHIVE);
#else
    com_maxfps = Cvar_GetRange("com_maxfps", "125", "0", "1000", CVAR_ARCHIVE | CVAR_USERSAVE);
#endif
    com_blood = Cvar_Get("com_blood", "1", CVAR_ARCHIVE);

    com_logfile = Cvar_Get("logfile", "0", CVAR_TEMP);

    com_timescale = Cvar_Get("timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO);
    com_fixedtime = Cvar_Get("fixedtime", "0", CVAR_CHEAT);
    com_showtrace = Cvar_Get("com_showtrace", "0", CVAR_CHEAT);
    com_speeds = Cvar_Get("com_speeds", "0", 0);
    com_timedemo = Cvar_Get("timedemo", "0", CVAR_CHEAT);
    com_cameraMode = Cvar_Get("com_cameraMode", "0", CVAR_CHEAT);

    cl_paused = Cvar_Get("cl_paused", "0", CVAR_ROM);
    sv_paused = Cvar_Get("sv_paused", "0", CVAR_ROM);
    cl_packetdelay = Cvar_Get("cl_packetdelay", "0", CVAR_CHEAT);
    sv_packetdelay = Cvar_Get("sv_packetdelay", "0", CVAR_CHEAT);
    com_sv_running = Cvar_Get("sv_running", "0", CVAR_ROM);
    com_cl_running = Cvar_Get("cl_running", "0", CVAR_ROM);
    com_ansiColor = Cvar_Get("com_ansiColor", "0", CVAR_ARCHIVE);

    com_unfocused = Cvar_Get("com_unfocused", "0", CVAR_ROM);
    com_maxfpsUnfocused = Cvar_Get("com_maxfpsUnfocused", "60", CVAR_ARCHIVE);
    com_minimized = Cvar_Get("com_minimized", "0", CVAR_ROM);
    com_maxfpsMinimized = Cvar_Get("com_maxfpsMinimized", "10", CVAR_ARCHIVE);
    com_busyWait = Cvar_Get("com_busyWait", "0", CVAR_ARCHIVE);
    com_maxfpsVRR = Cvar_Get("com_maxfpsVRR", "0", CVAR_ARCHIVE);

    Cvar_Get("com_pid", va("%d", Sys_PID()), CVAR_ROM);
    Cvar_Get("com_idleSleep", "1", CVAR_ARCHIVE | CVAR_USERSAVE);
    Cvar_Get("sv_idleRestart", "1", 0);
    Cvar_Get("fs_copyfiles", "0", CVAR_INIT);
    Cvar_Get("fs_restrict", "", CVAR_INIT);
#ifndef DEDICATED
    Cvar_Get("cl_platform", "1", CVAR_ROM);
    Cvar_Get("in_joyBallScale", "1.0", CVAR_ARCHIVE);
    Cvar_Get("in_mididevice", "0", CVAR_ARCHIVE);
    Cvar_Get("in_midiport", "1", CVAR_ARCHIVE);
    Cvar_Get("r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE);
    Cvar_Get("r_maskMinidriver", "0", CVAR_LATCH);
#endif
    com_workers = Cvar_Get("com_workers", "-1", CVAR_ARCHIVE | CVAR_LATCH);
    com_preciseFrameTiming = Cvar_Get("com_preciseFrameTiming", "1", CVAR_ARCHIVE);

    Com_JobsInit();
    Cvar_Get("com_errorMessage", "", CVAR_ROM | CVAR_NORESTART);

    s = va("%s %s %s", FULL_PRODUCT_VERSION, PLATFORM_STRING, PRODUCT_DATE);
    com_version = Cvar_Get("version", s, CVAR_ROM | CVAR_SERVERINFO);
    com_gamename = Cvar_Get("com_gamename", PRODUCT_NAME, CVAR_SERVERINFO | CVAR_INIT);
    com_protocol = Cvar_Get("com_protocol", va("%i", PROTOCOL_VERSION), CVAR_INIT);
    Cvar_Get("protocol", com_protocol->string, CVAR_SERVERINFO | CVAR_ROM);

#ifndef DEDICATED
    cl_allowConsoleChat = Cvar_Get("cl_allowConsoleChat", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    com_allowConsole = Cvar_Get("com_allowConsole", "1", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
#endif

    con_completionStyle = Cvar_Get("con_completionStyle", "0", CVAR_ARCHIVE);
    Cvar_CheckRange(con_completionStyle, 0, 1, qtrue);
    Cvar_SetHelp("con_completionStyle", "auto-completion style\n"
                 S_COLOR_VAL "    0 " S_COLOR_HELP "= Legacy, always print all the matches\n"
                 S_COLOR_VAL "    1 " S_COLOR_HELP "= ET-style, print them once, then tab cycles through them");

    Sys_Init();

#ifdef DEDICATED
    Sys_WritePIDFile(FS_GetCurrentGameDir());
#endif

    Com_RandomBytes((byte*)&qport, sizeof(int));
    Netchan_Init(qport & 0xffff);

    VM_Init();
    SV_Init();

    Steam_GS_Init();
#ifdef DEDICATED
    SV_WorkshopInit();
#endif

    ZMQ_Init();

    com_dedicated->modified = qfalse;
#ifndef DEDICATED
    CL_Init();
#endif

    com_frameTime = Com_Milliseconds();

    Com_AddStartupCommands();

    CL_StartHunkUsers(qfalse);

    com_fullyInitialized = qtrue;

    com_pipefile = Cvar_Get("com_pipefile", "", CVAR_ARCHIVE | CVAR_LATCH);
    if (com_pipefile->string[0]) {
        pipefile = FS_FCreateOpenPipeFile(com_pipefile->string);
    }

    Com_Printf("--- Common Initialization Complete ---\n");

    if (com_dedicated->integer) {
        Cbuf_AddText("vstr serverstartup\n");
    }
}

void Com_ReadFromPipe(void) {
    static char buf[MAX_STRING_CHARS];
    static int accu = 0;
    int read;

    if (!pipefile)
        return;

    while ((read = FS_Read(buf + accu, sizeof(buf) - accu - 1, pipefile)) > 0) {
        char* brk = NULL;
        int i;

        for (i = accu; i < accu + read; ++i) {
            if (buf[i] == '\0')
                buf[i] = '\n';
            if (buf[i] == '\n' || buf[i] == '\r')
                brk = &buf[i + 1];
        }
        buf[accu + read] = '\0';

        accu += read;

        if (brk) {
            char tmp = *brk;
            *brk = '\0';
            Cbuf_ExecuteText(EXEC_APPEND, buf);
            *brk = tmp;

            accu -= brk - buf;
            memmove(buf, brk, accu + 1);
        } else if (accu >= sizeof(buf) - 1)
        {
            Cbuf_ExecuteText(EXEC_APPEND, buf);
            accu = 0;
        }
    }
}

void Com_WriteConfigToFile(const char* filename) {
    fileHandle_t f;

    f = FS_FOpenFileWrite(filename);
    if (!f) {
        Com_Printf("Couldn't write %s.\n", filename);
        return;
    }

    FS_Printf(f, "// Hardware cfg - Generated by QUAKE LIVE. Do not modify\n");
    Key_WriteBindings(f);
    Cvar_WriteVariables(f, f, 0);
    Cmd_WriteAliases(f);
    FS_FCloseFile(f);
}

void Com_WriteSplitConfig(const char* hwName, const char* repName) {
    fileHandle_t f_hw, f_rep;

    f_hw = FS_FOpenFileWrite(hwName);
    if (!f_hw) {
        Com_Printf("Couldn't write %s.\n", hwName);
        return;
    }

    f_rep = FS_FOpenFileWrite(repName);
    if (!f_rep) {
        Com_Printf("Couldn't write %s.\n", repName);
        FS_FCloseFile(f_hw);
        return;
    }

    FS_Printf(f_hw, "// Hardware cfg - Generated by QUAKE LIVE. Do not modify\n");
    FS_Printf(f_rep, "// Replicate cfg - Generated by QUAKE LIVE. Do not modify\n");

    Key_WriteBindings(f_rep);

    Cvar_WriteVariables(f_hw, f_rep, 0);

    Cmd_WriteAliases(f_hw);

    FS_FCloseFile(f_rep);
    FS_FCloseFile(f_hw);
}

void Com_WriteConfiguration(void) {
    if (!com_fullyInitialized) {
        return;
    }

    if (!(cvar_modifiedFlags & CVAR_ARCHIVE)) {
        return;
    }
    cvar_modifiedFlags &= ~CVAR_ARCHIVE;

    if (com_dedicated->integer) {
        return;
    }

#ifndef DEDICATED
    Com_WriteSplitConfig(QZCONFIG_CFG, REPCONFIG_CFG);
#endif
}

void Com_WriteConfig_f(void) {
    char filename[MAX_QPATH];
    char repfilename[MAX_QPATH];
    int argc = Cmd_Argc();

    if (argc != 2 && argc != 3) {
        Com_Printf("Usage: writeconfig <filename> [<rep filename>]\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
    COM_DefaultExtension(filename, sizeof(filename), ".cfg");

    if (!COM_CompareExtension(filename, ".cfg")) {
        Com_Printf("Com_WriteConfig_f: Only the \".cfg\" extension is supported by this command!\n");
        return;
    }

    if (argc == 3) {
        Q_strncpyz(repfilename, Cmd_Argv(2), sizeof(repfilename));
        COM_DefaultExtension(repfilename, sizeof(repfilename), ".cfg");
        Com_Printf("Writing hardware cfg %s. Replicated cfg %s\n", filename, repfilename);
        Com_WriteSplitConfig(filename, repfilename);
    } else {
        Com_Printf("Writing %s.\n", filename);
        Com_WriteConfigToFile(filename);
    }
}

void Com_WriteClientConfig_f(void) {
    char filename[MAX_QPATH];
    fileHandle_t f;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: writeClientConfig <filename>\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
    COM_DefaultExtension(filename, sizeof(filename), ".cfg");

    Com_Printf("Writing %s.\n", filename);
    f = FS_FOpenFileWrite(filename);
    if (!f) {
        Com_Printf("Couldn't write %s.\n", filename);
        return;
    }

    FS_Printf(f, "// Hardware cfg - Generated by QUAKE LIVE. Do not modify\n");
    Key_WriteBindings(f);
    Cvar_WriteVariables(f, f, 1);
    FS_FCloseFile(f);
}

int Com_ModifyMsec(int msec) {
    int clampTime;

    if (com_fixedtime->integer) {
        msec = com_fixedtime->integer;
    } else if (com_timescale->value) {
        msec *= com_timescale->value;
    } else if (com_cameraMode->integer) {
        msec *= com_timescale->value;
    }

    if (msec < 1 && com_timescale->value) {
        msec = 1;
    }

    if (com_dedicated->integer) {
        if (com_sv_running->integer && msec > 500)
            Com_Printf("Hitch warning: %i msec frame time\n", msec);

        clampTime = 5000;
    } else if (!com_sv_running->integer) {
        clampTime = 5000;
    } else {
        clampTime = 200;
    }

    if (msec > clampTime) {
        msec = clampTime;
    }

    return msec;
}

int Com_TimeVal(int minMsec) {
    int timeVal;

    timeVal = Sys_Milliseconds() - com_frameTime;

    if (timeVal >= minMsec)
        timeVal = 0;
    else
        timeVal = minMsec - timeVal;

    return timeVal;
}

#define COM_FT_MAX 8192

static int64_t com_ftSamples[COM_FT_MAX];
static int64_t com_ftLast;
static int com_ftWanted;
static int com_ftCount;

static int Com_FtCompare(const void* a, const void* b) {
    int64_t x = *(const int64_t*)a;
    int64_t y = *(const int64_t*)b;

    return (x > y) - (x < y);
}

static void Com_FrameTimesReport(void) {
    static int64_t sorted[COM_FT_MAX];
    int64_t total = 0, deviation = 0, worstAt = 0;
    float average;
    int i, worst = 0;

    for (i = 0; i < com_ftCount; i++) {
        if (com_ftSamples[i] > com_ftSamples[worst]) {
            worst = i;
            worstAt = total;
        }
        total += com_ftSamples[i];
    }
    average = (float)total / com_ftCount;
    for (i = 0; i < com_ftCount; i++) {
        deviation += com_ftSamples[i] > average ? com_ftSamples[i] - (int64_t)average
                                                : (int64_t)average - com_ftSamples[i];
    }
    Com_Memcpy(sorted, com_ftSamples, com_ftCount * sizeof(sorted[0]));
    qsort(sorted, com_ftCount, sizeof(sorted[0]), Com_FtCompare);

    Com_Printf("frametimes: %i frames in %.3f s\n", com_ftCount, total / 1000000.0f);
    Com_Printf("  average %.2f ms (%.1f fps)   min %.2f   max %.2f\n", average / 1000.0f,
               1000000.0f / average, sorted[0] / 1000.0f, sorted[com_ftCount - 1] / 1000.0f);
    Com_Printf("  median %.2f   1%% worst %.2f   jitter %.2f ms\n",
               sorted[com_ftCount / 2] / 1000.0f, sorted[com_ftCount - 1 - com_ftCount / 100] / 1000.0f,
               (float)deviation / com_ftCount / 1000.0f);
    Com_Printf("  worst: frame %i, %.3f s in\n", worst, worstAt / 1000000.0f);
}

static void Com_FrameTimes_f(void) {
    int wanted = 600;

    if (Cmd_Argc() > 1) {
        wanted = atoi(Cmd_Argv(1));
    }
    if (wanted < 10) {
        wanted = 10;
    }
    if (wanted > COM_FT_MAX) {
        wanted = COM_FT_MAX;
    }
    com_ftWanted = wanted;
    com_ftCount = 0;
    com_ftLast = 0;
    Com_Printf("frametimes: measuring %i frames...\n", wanted);
}

static void Com_FrameTimeSample(void) {
    int64_t now;

    if (!com_ftWanted) {
        return;
    }
    now = Sys_Microseconds();
    if (com_ftLast) {
        com_ftSamples[com_ftCount++] = now - com_ftLast;
    }
    com_ftLast = now;
    if (com_ftCount >= com_ftWanted) {
        Com_FrameTimesReport();
        com_ftWanted = 0;
        com_ftCount = 0;
        com_ftLast = 0;
    }
}

#define COM_MAX_JOBS 4096
#define COM_MAX_WORKERS 8

typedef struct {
    void (*function)(void*);
    void* data;
} comJob_t;

static comJob_t com_jobQueue[COM_MAX_JOBS];
static int com_jobHead, com_jobTail;
static void* com_jobMutex;
static void* com_jobSemaphore;
static int com_jobWorkers;

static void Com_JobWorker(void* unused) {
    (void)unused;

    for (;;) {
        comJob_t job;

        Sys_SemaphoreWait(com_jobSemaphore);

        Sys_MutexLock(com_jobMutex);
        if (com_jobTail == com_jobHead) {
            Sys_MutexUnlock(com_jobMutex);
            continue;
        }
        job = com_jobQueue[com_jobTail & (COM_MAX_JOBS - 1)];
        com_jobTail++;
        Sys_MutexUnlock(com_jobMutex);

        job.function(job.data);
    }
}

void Com_JobsInit(void) {
    int wanted = com_workers->integer;
    int i;

    if (com_jobWorkers || wanted == 0) {
        return;
    }
    if (wanted < 0) {
        wanted = Sys_ProcessorCount() - 1;
    }
    if (wanted > COM_MAX_WORKERS) {
        wanted = COM_MAX_WORKERS;
    }
    if (wanted < 1) {
        return;
    }

    com_jobMutex = Sys_MutexCreate();
    com_jobSemaphore = Sys_SemaphoreCreate();
    if (!com_jobMutex || !com_jobSemaphore) {
        return;
    }
    for (i = 0; i < wanted; i++) {
        if (!Sys_ThreadCreate(Com_JobWorker, NULL)) {
            break;
        }
        com_jobWorkers++;
    }
    Com_DPrintf("Workers: %i\n", com_jobWorkers);
}

int Com_JobWorkers(void) {
    return com_jobWorkers;
}

void Com_JobsSubmit(void (*function)(void*), void* data) {
    qboolean queued = qfalse;

    if (com_jobWorkers) {
        Sys_MutexLock(com_jobMutex);
        if (com_jobHead - com_jobTail < COM_MAX_JOBS) {
            com_jobQueue[com_jobHead & (COM_MAX_JOBS - 1)].function = function;
            com_jobQueue[com_jobHead & (COM_MAX_JOBS - 1)].data = data;
            com_jobHead++;
            queued = qtrue;
        }
        Sys_MutexUnlock(com_jobMutex);
    }
    if (queued) {
        Sys_SemaphorePost(com_jobSemaphore);
        return;
    }
    function(data);
}

typedef struct {
    int from, to;
    double total;
    volatile int done;
} comJobTest_t;

static void Com_JobTestWork(void* data) {
    comJobTest_t* test = (comJobTest_t*)data;
    double total = 0;
    int i;

    for (i = test->from; i < test->to; i++) {
        total += sqrt((double)i);
    }
    test->total = total;
    test->done = 1;
}

static void Com_JobTest_f(void) {
    static comJobTest_t tests[COM_MAX_WORKERS + 1];
    const int slice = 2000000;
    int parts = Com_JobWorkers() + 1;
    double parallel = 0, single = 0;
    int64_t startUs;
    float parallelMs, singleMs;
    int i;

    for (i = 0; i < parts; i++) {
        tests[i].from = i * slice;
        tests[i].to = (i + 1) * slice;
        tests[i].done = 0;
    }
    startUs = Sys_Microseconds();
    for (i = 0; i < parts; i++) {
        Com_JobsSubmit(Com_JobTestWork, &tests[i]);
    }
    for (i = 0; i < parts; i++) {
        while (!tests[i].done) {
            Sys_Sleep(0);
        }
        parallel += tests[i].total;
    }
    parallelMs = (Sys_Microseconds() - startUs) / 1000.0f;

    startUs = Sys_Microseconds();
    for (i = 0; i < parts; i++) {
        Com_JobTestWork(&tests[i]);
        single += tests[i].total;
    }
    singleMs = (Sys_Microseconds() - startUs) / 1000.0f;

    Com_Printf("jobs: %i workers, %i parts\n", Com_JobWorkers(), parts);
    Com_Printf("  spread out %.1f ms, single thread %.1f ms  (%.2fx)\n", parallelMs, singleMs,
               singleMs / (parallelMs > 0 ? parallelMs : 1));
    Com_Printf("  results %s\n", parallel == single ? "match" : "DO NOT MATCH");
}

static int Com_FrameCap(void) {
    int cap = com_maxfps->integer;

#ifndef DEDICATED
    if (com_maxfpsVRR->integer) {
        int hz = CL_DisplayFrequency();

        if (hz > 3 && (cap <= 0 || cap > hz - 3)) {
            cap = hz - 3;
        }
    }
#endif
    return cap;
}

static void Com_IdleExit(void) {
    static int idleSince = 0;
    int limit = Cvar_VariableIntegerValue("sv_idleExit");

    if (!com_dedicated || !com_dedicated->integer || limit <= 0) {
        idleSince = 0;
        return;
    }
    if (com_sv_running->integer) {
        idleSince = 0;
        return;
    }
    if (!idleSince) {
        idleSince = Sys_Milliseconds();
        return;
    }
    if (Sys_Milliseconds() - idleSince > limit * 1000) {
        Com_Printf("Server has been idle for %i seconds, exiting.\n", limit);
        Com_Quit_f();
    }
}

void Com_Frame(void) {
    Steam_Frame();
    Com_IdleExit();
    SV_VoiceFrame();
    ZMQ_Frame();
    int msec, minMsec;
    int timeVal, timeValSV;
    int capUs;
    static int lastTime = 0, bias = 0;

    int timeBeforeFirstEvents;
    int timeBeforeServer;
    int timeBeforeEvents;
    int timeBeforeClient;
    int timeAfter;

    if (setjmp(abortframe)) {
        return;
    }

    timeBeforeFirstEvents = 0;
    timeBeforeServer = 0;
    timeBeforeEvents = 0;
    timeBeforeClient = 0;
    timeAfter = 0;

    Com_WriteConfiguration();

    if (com_speeds->integer) {
        timeBeforeFirstEvents = Sys_Milliseconds();
    }

    capUs = 0;
    if (!com_timedemo->integer) {
        if (com_dedicated->integer)
            minMsec = SV_FrameMsec();
        else {
            if (com_minimized->integer && com_maxfpsMinimized->integer > 0) {
                minMsec = 1000 / com_maxfpsMinimized->integer;
                capUs = 1000000 / com_maxfpsMinimized->integer;
            } else if (com_unfocused->integer && com_maxfpsUnfocused->integer > 0) {
                minMsec = 1000 / com_maxfpsUnfocused->integer;
                capUs = 1000000 / com_maxfpsUnfocused->integer;
            } else if (Com_FrameCap() > 0) {
                minMsec = 1000 / Com_FrameCap();
                capUs = 1000000 / Com_FrameCap();
            } else
                minMsec = 1;

            timeVal = com_frameTime - lastTime;
            bias += timeVal - minMsec;

            if (bias > minMsec)
                bias = minMsec;

            minMsec -= bias;
        }
    } else
        minMsec = 1;

    if (com_preciseFrameTiming->integer && capUs > 1000) {
        int64_t nowUs = Sys_Microseconds();

        if (!com_frameTargetUs || nowUs - com_frameTargetUs > 250000 ||
            com_frameTargetUs - nowUs > 250000) {
            com_frameTargetUs = nowUs;
        }
        com_frameTargetUs += capUs;

        for (;;) {
            int64_t remainUs;
            int sleepMs;

            timeValSV = com_sv_running->integer ? SV_SendQueuedPackets() : 0;

            remainUs = com_frameTargetUs - Sys_Microseconds();
            if (remainUs <= 0) {
                NET_Sleep(0);
                break;
            }

            sleepMs = (int)((remainUs - 1500) / 1000);
            if (com_sv_running->integer && timeValSV < sleepMs)
                sleepMs = timeValSV;

            if (com_busyWait->integer || sleepMs < 1)
                NET_Sleep(0);
            else
                NET_Sleep(sleepMs);
        }
    } else {
        com_frameTargetUs = 0;

        do {
            if (com_sv_running->integer) {
                timeValSV = SV_SendQueuedPackets();

                timeVal = Com_TimeVal(minMsec);

                if (timeValSV < timeVal)
                    timeVal = timeValSV;
            } else
                timeVal = Com_TimeVal(minMsec);

            if (com_busyWait->integer || timeVal < 1)
                NET_Sleep(0);
            else
                NET_Sleep(timeVal - 1);
        } while (Com_TimeVal(minMsec));
    }

    Com_FrameTimeSample();

#ifndef DEDICATED
    if (!com_dedicated->integer) {
        CL_LowLatencyWait();
    }
#endif

    IN_Frame();

    lastTime = com_frameTime;
    com_frameTime = Com_EventLoop();

    msec = com_frameTime - lastTime;

    Cbuf_Execute();

    msec = Com_ModifyMsec(msec);

    if (com_speeds->integer) {
        timeBeforeServer = Sys_Milliseconds();
    }

    SV_Frame(msec);

    if (com_dedicated->modified) {
        Cvar_Get("dedicated", "0", 0);
        com_dedicated->modified = qfalse;
        if (!com_dedicated->integer) {
            SV_Shutdown("dedicated set to 0");
            CL_FlushMemory();
        }
    }

#ifndef DEDICATED
    if (com_speeds->integer) {
        timeBeforeEvents = Sys_Milliseconds();
    }
    Com_EventLoop();
    Cbuf_Execute();

    if (com_speeds->integer) {
        timeBeforeClient = Sys_Milliseconds();
    }

    CL_Frame(msec);

    if (com_speeds->integer) {
        timeAfter = Sys_Milliseconds();
    }
#else
    if (com_speeds->integer) {
        timeAfter = Sys_Milliseconds();
        timeBeforeEvents = timeAfter;
        timeBeforeClient = timeAfter;
    }
#endif

    NET_FlushPacketQueue();

    if (com_speeds->integer) {
        int all, sv, ev, cl;

        all = timeAfter - timeBeforeServer;
        sv = timeBeforeEvents - timeBeforeServer;
        ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
        cl = timeAfter - timeBeforeClient;
        sv -= time_game;
        cl -= time_frontend + time_backend;

        Com_Printf("frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i bk:%3i\n",
                   com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend);
    }

    if (com_showtrace->integer) {
        extern int c_traces, c_brush_traces, c_patch_traces;
        extern int c_pointcontents;

        Com_Printf("%4i traces  (%ib %ip) %4i points\n", c_traces,
                   c_brush_traces, c_patch_traces, c_pointcontents);
        c_traces = 0;
        c_brush_traces = 0;
        c_patch_traces = 0;
        c_pointcontents = 0;
    }

    Com_ReadFromPipe();

    com_frameNumber++;
}

void Com_Shutdown(void) {
    Steam_Shutdown();
    ZMQ_Shutdown();
    if (logfile) {
        FS_FCloseFile(logfile);
        logfile = 0;
    }

    if (com_journalFile) {
        FS_FCloseFile(com_journalFile);
        com_journalFile = 0;
    }

    if (pipefile) {
        FS_FCloseFile(pipefile);
        FS_HomeRemove(com_pipefile->string);
    }
}

void Field_Clear(field_t* edit) {
    memset(edit->buffer, 0, MAX_EDIT_LINE);
    edit->cursor = 0;
    edit->scroll = 0;
    edit->acOffset = 0;
    edit->acLength = 0;
}

static const char* completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int matchCount;
static field_t* completionField;

#define MAX_CYCLE_MATCHES 1024
#define CYCLE_POOL_SIZE 32768
static qboolean cycleCollect;
static char cyclePool[CYCLE_POOL_SIZE];
static int cyclePoolUsed;
static const char* cycleMatches[MAX_CYCLE_MATCHES];
static int cycleCount;
static field_t* cycleField;
static int cycleIndex;
static int cycleOffset;
static char cycleBuffer[MAX_EDIT_LINE];

static void Field_BeginMatches(void) {
    matchCount = 0;
    shortestMatch[0] = 0;
    cycleCount = 0;
    cyclePoolUsed = 0;
}

static void Field_AddCycleMatch(const char* s) {
    const int len = (int)strlen(s) + 1;

    if (cycleCount >= MAX_CYCLE_MATCHES || cyclePoolUsed + len > CYCLE_POOL_SIZE) {
        return;
    }

    Com_Memcpy(cyclePool + cyclePoolUsed, s, len);
    cycleMatches[cycleCount++] = cyclePool + cyclePoolUsed;
    cyclePoolUsed += len;
}

static void FindMatches(const char* s) {
    int i;

    if (Q_stricmpn(s, completionString, strlen(completionString))) {
        return;
    }
    matchCount++;
    if (cycleCollect) {
        Field_AddCycleMatch(s);
    }
    if (matchCount == 1) {
        Q_strncpyz(shortestMatch, s, sizeof(shortestMatch));
        return;
    }

    for (i = 0; shortestMatch[i]; i++) {
        if (i >= strlen(s)) {
            shortestMatch[i] = 0;
            break;
        }

        if (tolower(shortestMatch[i]) != tolower(s[i])) {
            shortestMatch[i] = 0;
        }
    }
}

static void PrintMatches(const char* s) {
    if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch))) {
        Com_Printf("    %s\n", s);
    }
}

static void PrintCvarMatches(const char* s) {
    char value[TRUNCATE_LENGTH];

    if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch))) {
        Com_TruncateLongString(value, Cvar_VariableString(s));
        Com_Printf("    %s = \"%s\"\n", s, value);
    }
}

static int Field_ListStyle(void) {
    return Cvar_VariableIntegerValue("con_style");
}

static int Field_ListWidth(void) {
#ifndef DEDICATED
    const int width = Con_GetLineWidth();
    if (width > 16) {
        return width;
    }
#endif
    return 78;
}

static int QDECL Field_CompareMatches(const void* a, const void* b) {
    return Q_stricmp(*(const char* const*)a, *(const char* const*)b);
}

static void Field_TruncatePrintString(char* buffer, int maxLength) {
    int i, printed;

    for (i = 0, printed = 0; buffer[i]; i++) {
        if (Q_IsColorString(&buffer[i]) || Q_IsConsoleColorString(&buffer[i])) {
            i++;
        } else if (buffer[i] != '\n') {
            printed++;
        }
    }
    if (printed <= maxLength || maxLength < 4) {
        return;
    }

    for (i = 0, printed = 0; buffer[i]; i++) {
        if (Q_IsColorString(&buffer[i]) || Q_IsConsoleColorString(&buffer[i])) {
            i++;
        } else if (printed++ == maxLength - 3) {
            break;
        }
    }
    strcpy(&buffer[i], "...\n");
}

static void Field_FirstHelpLine(const char* help, char* out, int outSize) {
    int len = 0;

    if (help) {
        len = (int)strcspn(help, "\n");
        if (len >= outSize) {
            len = outSize - 1;
        }
        Com_Memcpy(out, help, len);
    }
    out[len] = '\0';
}

static void Field_PrintCmdMatch(const char* s) {
    char msg[MAX_STRING_CHARS];
    char desc[MAX_STRING_CHARS];
    const char* name;
    const char* help;
    qboolean fromModule;
    char marker;

    Cmd_GetHelp(s, &name, &help, &fromModule);
    Field_FirstHelpLine(help, desc, sizeof(desc));
    marker = (help && strchr(help, '\n')) ? 'h' : ' ';

    if (desc[0]) {
        Com_sprintf(msg, sizeof(msg), " %c  " S_COLOR_CMD "%s" S_COLOR_WHITE " - " S_COLOR_HELP "%s\n", marker, s, desc);
    } else {
        Com_sprintf(msg, sizeof(msg), " %c  " S_COLOR_CMD "%s\n", marker, s);
    }
    Field_TruncatePrintString(msg, Field_ListWidth());
    Com_Printf("%s", msg);
}

static void Field_PrintCvarMatch(const cvar_t* var) {
    char msg[MAX_STRING_CHARS];
    char desc[MAX_STRING_CHARS];
    char value[TRUNCATE_LENGTH];
    char marker;

    Field_FirstHelpLine(var->description, desc, sizeof(desc));
    Com_TruncateLongString(value, var->string);
    if (var->flags & CVAR_USER_CREATED) {
        marker = '?';
    } else {
        marker = (var->description && strchr(var->description, '\n')) ? 'h' : ' ';
    }

    if (desc[0]) {
        Com_sprintf(msg, sizeof(msg), " %c  " S_COLOR_CVAR "%s" S_COLOR_WHITE " = \"" S_COLOR_VAL "%s" S_COLOR_WHITE "\" - " S_COLOR_HELP "%s\n",
                    marker, var->name, value, desc);
    } else {
        Com_sprintf(msg, sizeof(msg), " %c  " S_COLOR_CVAR "%s" S_COLOR_WHITE " = \"" S_COLOR_VAL "%s" S_COLOR_WHITE "\"\n",
                    marker, var->name, value);
    }
    Field_TruncatePrintString(msg, Field_ListWidth());
    Com_Printf("%s", msg);
}

static void Field_PrintSortedMatches(void) {
    int i;

    for (i = 0; i < cycleCount; i++) {
        if (!Cvar_Find(cycleMatches[i])) {
            Field_PrintCmdMatch(cycleMatches[i]);
        }
    }
    for (i = 0; i < cycleCount; i++) {
        const cvar_t* var = Cvar_Find(cycleMatches[i]);
        if (var) {
            Field_PrintCvarMatch(var);
        }
    }
}

static void Field_StartCycle(int offset) {
    cycleField = NULL;
    if (cycleCount < 2 || cycleCount != matchCount) {
        return;
    }

    qsort((void*)cycleMatches, cycleCount, sizeof(cycleMatches[0]), Field_CompareMatches);

    if (!con_completionStyle || !con_completionStyle->integer) {
        return;
    }

    cycleField = completionField;
    cycleOffset = offset;
    cycleIndex = -1;
    Q_strncpyz(cycleBuffer, completionField->buffer, sizeof(cycleBuffer));
    completionField->acOffset = offset;
    completionField->acLength = 0;
}

static char* Field_FindFirstSeparator(char* s) {
    int i;

    for (i = 0; i < strlen(s); i++) {
        if (s[i] == ';')
            return &s[i];
    }

    return NULL;
}

static qboolean Field_Complete(void) {
    int completionOffset;

    if (matchCount == 0)
        return qtrue;

    completionOffset = strlen(completionField->buffer) - strlen(completionString);

    Q_strncpyz(&completionField->buffer[completionOffset], shortestMatch,
               sizeof(completionField->buffer) - completionOffset);

    completionField->cursor = strlen(completionField->buffer);

    if (matchCount == 1) {
        Q_strcat(completionField->buffer, sizeof(completionField->buffer), " ");
        completionField->cursor++;
        return qtrue;
    }

    Field_StartCycle(completionOffset);

    Com_Printf("]%s\n", completionField->buffer);

    return qfalse;
}

#ifndef DEDICATED
void Field_CompleteKeyname(void) {
    Field_BeginMatches();

    Key_KeynameCompletion(FindMatches);

    if (!Field_Complete())
        Key_KeynameCompletion(PrintMatches);
}
#endif

void Field_CompleteList(const char* const* list, int count) {
    int i;

    Field_BeginMatches();

    for (i = 0; i < count; i++) {
        if (list[i]) {
            FindMatches(list[i]);
        }
    }

    if (!Field_Complete()) {
        for (i = 0; i < count; i++) {
            if (list[i]) {
                PrintMatches(list[i]);
            }
        }
    }
}

void Field_CompleteFilename(const char* dir,
                            const char* ext,
                            qboolean stripExt,
                            qboolean allowNonPureFilesOnDisk) {
    Field_BeginMatches();

    FS_FilenameCompletion(dir, ext, stripExt, FindMatches, allowNonPureFilesOnDisk);

    if (!Field_Complete())
        FS_FilenameCompletion(dir, ext, stripExt, PrintMatches, allowNonPureFilesOnDisk);
}

void Field_CompleteCommand(char* cmd,
                           qboolean doCommands,
                           qboolean doCvars) {
    int completionArgument = 0;

    cmd = Com_SkipCharset(cmd, " \"");

    Cmd_TokenizeStringIgnoreQuotes(cmd);
    completionArgument = Cmd_Argc();

    if (*(cmd + strlen(cmd) - 1) == ' ') {
        completionString = "";
        completionArgument++;
    } else
        completionString = Cmd_Argv(completionArgument - 1);

#ifndef DEDICATED
    if (cl_allowConsoleChat->integer && completionField->buffer[0] &&
        completionField->buffer[0] != '\\') {
        if (completionField->buffer[0] != '/') {
            if (strlen(completionField->buffer) + 1 >=
                sizeof(completionField->buffer))
                return;

            memmove(&completionField->buffer[1],
                    &completionField->buffer[0],
                    strlen(completionField->buffer) + 1);
            completionField->cursor++;
        }

        completionField->buffer[0] = '\\';
    }
#endif

    if (completionArgument > 1) {
        const char* baseCmd = Cmd_Argv(0);
        char* p;

#ifndef DEDICATED
        if (baseCmd[0] == '\\' || baseCmd[0] == '/')
            baseCmd++;
#endif

        if ((p = Field_FindFirstSeparator(cmd)))
            Field_CompleteCommand(p + 1, qtrue, qtrue);
        else
            Cmd_CompleteArgument(baseCmd, cmd, completionArgument);
    } else {
        if (completionString[0] == '\\' || completionString[0] == '/')
            completionString++;

        Field_BeginMatches();

        if (strlen(completionString) == 0)
            return;

        if (doCommands)
            Cmd_CommandCompletion(FindMatches);

        if (doCvars)
            Cvar_CommandCompletion(FindMatches);

        if (!Field_Complete()) {
            if (Field_ListStyle() == 1 && cycleCount == matchCount) {
                Field_PrintSortedMatches();
            } else {
                if (doCommands)
                    Cmd_CommandCompletion(PrintMatches);

                if (doCvars)
                    Cvar_CommandCompletion(PrintCvarMatches);
            }
        }
    }
}

void Field_AutoComplete(field_t* field) {
    if (cycleField == field && !strcmp(field->buffer, cycleBuffer)) {
        cycleIndex = (cycleIndex + 1) % cycleCount;
        Q_strncpyz(field->buffer + cycleOffset, cycleMatches[cycleIndex], sizeof(field->buffer) - cycleOffset);
        field->cursor = strlen(field->buffer);
        field->acOffset = cycleOffset;
        field->acLength = field->cursor - cycleOffset;
        Q_strncpyz(cycleBuffer, field->buffer, sizeof(cycleBuffer));
        return;
    }
    Field_ResetCompletionCycle();

    completionField = field;
    cycleCollect = ((con_completionStyle && con_completionStyle->integer) || Field_ListStyle() == 1) ? qtrue : qfalse;

    Field_CompleteCommand(completionField->buffer, qtrue, qtrue);

    cycleCollect = qfalse;
}

void Field_ResetCompletionCycle(void) {
    if (cycleField) {
        cycleField->acOffset = 0;
        cycleField->acLength = 0;
        cycleField = NULL;
    }
}

void Field_InsertCVarValue(field_t* edit, const cvar_t* cvar, qboolean defaultValue) {
    const char* const value = defaultValue ? cvar->resetString : cvar->string;
    const char* const quote = (value[0] == '\0' || strchr(value, ' ')) ? "\"" : "";

    Com_sprintf(edit->buffer, sizeof(edit->buffer), "\\%s %s", cvar->name, quote);
    edit->cursor = strlen(edit->buffer);
    Q_strcat(edit->buffer, sizeof(edit->buffer), va("%s%s", value, quote));
}

void Field_InsertBindCommand(field_t* edit, const char* key, const char* command) {
    const char* quote;

    if (!command) {
        command = "";
    }
    quote = (command[0] == '\0' || strchr(command, ' ')) ? "\"" : "";

    Com_sprintf(edit->buffer, sizeof(edit->buffer), "\\bind %s %s", key, quote);
    edit->cursor = strlen(edit->buffer);
    Q_strcat(edit->buffer, sizeof(edit->buffer), va("%s%s", command, quote));
}

printHelpResult_t Com_PrintHelp(const char* name, printf_t print, qboolean printNotFound, qboolean printModules, qboolean printFlags) {
    const cvar_t* var = Cvar_Find(name);
    const char* registeredName = name;
    const char* help = NULL;
    const char* newLine;
    qboolean fromModule = qfalse;
    char first[MAX_STRING_CHARS];
    int firstLen;

    if (var) {
        registeredName = var->name;
        help = var->description;
        Cvar_PrintFirstHelpLine(var, print);
        if (var->latchedString) {
            print("Current: \"" S_COLOR_VAL "%s" S_COLOR_WHITE "\" (Latched: \"" S_COLOR_VAL "%s" S_COLOR_WHITE "\")\n",
                  var->string, var->latchedString);
        } else {
            print("Current: \"" S_COLOR_VAL "%s" S_COLOR_WHITE "\"\n", var->string);
        }
        if (printModules) {
            if (var->flags & CVAR_USER_CREATED) {
                print("Module: Unknown\n");
            } else {
                print("Module: %s\n", (var->flags & CVAR_VM_CREATED) ? "Game module" : "Engine");
            }
        }
        if (printFlags) {
            Cvar_PrintFlags(var, print);
        }
    } else if (Cmd_GetHelp(name, &registeredName, &help, &fromModule)) {
        print(S_COLOR_CMD "%s\n", registeredName);
        if (printModules) {
            print("Module: %s\n", fromModule ? "Game module" : "Engine");
        }
    } else {
        if (printNotFound) {
            print("found no cvar/command with the name '%s'\n", name);
        }
        return PHR_NOTFOUND;
    }

    if (!help || !help[0]) {
        if (printNotFound) {
            print("no help text found for %s %s%s\n", var ? "cvar" : "command", var ? S_COLOR_CVAR : S_COLOR_CMD, registeredName);
        }
        return PHR_NOHELP;
    }

    newLine = strchr(help, '\n');
    firstLen = newLine ? (int)(newLine - help) : (int)strlen(help);
    if (firstLen >= (int)sizeof(first)) {
        firstLen = sizeof(first) - 1;
    }
    Com_Memcpy(first, help, firstLen);
    first[firstLen] = '\0';
    first[0] = toupper((unsigned char)first[0]);
    print(S_COLOR_HELP "%s%s\n", first, (firstLen > 0 && first[firstLen - 1] != '.') ? "." : "");

    if (newLine && newLine[1]) {
        print(S_COLOR_HELP "%s\n", newLine + 1);
    }

    return PHR_HADHELP;
}

qboolean Com_IsValidHexColor(const char* s, qboolean hasAlpha) {
    const int chars = hasAlpha ? 8 : 6;
    int i;

    for (i = 0; i < chars; i++) {
        const char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return qfalse;
        }
    }

    return s[chars] == '\0' ? qtrue : qfalse;
}

void Com_ParseHexColor(float* c, const char* text, qboolean hasAlpha) {
    unsigned int uc[4];

    c[0] = c[1] = c[2] = c[3] = 1.0f;

    if (hasAlpha) {
        if (sscanf(text, "%02X%02X%02X%02X", &uc[0], &uc[1], &uc[2], &uc[3]) != 4) {
            return;
        }
        c[3] = uc[3] / 255.0f;
    } else if (sscanf(text, "%02X%02X%02X", &uc[0], &uc[1], &uc[2]) != 3) {
        return;
    }

    c[0] = uc[0] / 255.0f;
    c[1] = uc[1] / 255.0f;
    c[2] = uc[2] / 255.0f;
}

void Com_RandomBytes(byte* string, int len) {
    int i;

    if (Sys_RandomBytes(string, len))
        return;

    Com_Printf("Com_RandomBytes: using weak randomization\n");
    for (i = 0; i < len; i++)
        string[i] = (unsigned char)(rand() % 256);
}

qboolean Com_IsVoipTarget(uint8_t* voipTargets, int voipTargetsSize, int clientNum) {
    int index;
    if (clientNum < 0) {
        for (index = 0; index < voipTargetsSize; index++) {
            if (voipTargets[index])
                return qtrue;
        }

        return qfalse;
    }

    index = clientNum >> 3;

    if (index < voipTargetsSize)
        return (voipTargets[index] & (1 << (clientNum & 0x07)));

    return qfalse;
}

static qboolean Field_CompletePlayerNameFinal(qboolean whitespace) {
    int completionOffset;

    if (matchCount == 0)
        return qtrue;

    completionOffset = strlen(completionField->buffer) - strlen(completionString);

    Q_strncpyz(&completionField->buffer[completionOffset], shortestMatch,
               sizeof(completionField->buffer) - completionOffset);

    completionField->cursor = strlen(completionField->buffer);

    if (matchCount == 1 && whitespace) {
        Q_strcat(completionField->buffer, sizeof(completionField->buffer), " ");
        completionField->cursor++;
        return qtrue;
    }

    return qfalse;
}

static void Name_PlayerNameCompletion(const char** names, int nameCount, void (*callback)(const char* s)) {
    int i;

    for (i = 0; i < nameCount; i++) {
        callback(names[i]);
    }
}

qboolean Com_FieldStringToPlayerName(char* name, int length, const char* rawname) {
    char hex[5];
    int i;
    int ch;

    if (name == NULL || rawname == NULL)
        return qfalse;

    if (length <= 0)
        return qtrue;

    for (i = 0; *rawname && i + 1 <= length; rawname++, i++) {
        if (*rawname == '\\') {
            Q_strncpyz(hex, rawname + 1, sizeof(hex));
            ch = Com_HexStrToInt(hex);
            if (ch > -1) {
                name[i] = ch;
                rawname += 4;
            } else {
                name[i] = *rawname;
            }
        } else {
            name[i] = *rawname;
        }
    }
    name[i] = '\0';

    return qtrue;
}

qboolean Com_PlayerNameToFieldString(char* str, int length, const char* name) {
    const char* p;
    int i;
    int x1, x2;

    if (str == NULL || name == NULL)
        return qfalse;

    if (length <= 0)
        return qtrue;

    *str = '\0';
    p = name;

    for (i = 0; *p != '\0'; i++, p++) {
        if (i + 1 >= length)
            break;

        if (*p <= ' ') {
            if (i + 5 + 1 >= length)
                break;

            x1 = *p >> 4;
            x2 = *p & 15;

            str[i + 0] = '\\';
            str[i + 1] = '0';
            str[i + 2] = 'x';
            str[i + 3] = x1 > 9 ? x1 - 10 + 'a' : x1 + '0';
            str[i + 4] = x2 > 9 ? x2 - 10 + 'a' : x2 + '0';

            i += 4;
        } else {
            str[i] = *p;
        }
    }
    str[i] = '\0';

    return qtrue;
}

void Field_CompletePlayerName(const char** names, int nameCount) {
    qboolean whitespace;

    Field_BeginMatches();

    if (nameCount <= 0)
        return;

    Name_PlayerNameCompletion(names, nameCount, FindMatches);

    if (completionString[0] == '\0') {
        Com_PlayerNameToFieldString(shortestMatch, sizeof(shortestMatch), names[0]);
    }

    if (completionString[0] != '\0' && Q_stricmp(shortestMatch, completionString) == 0 && nameCount > 1) {
        int i;

        for (i = 0; i < nameCount; i++) {
            if (Q_stricmp(names[i], completionString) == 0) {
                i++;
                if (i >= nameCount) {
                    i = 0;
                }

                Com_PlayerNameToFieldString(shortestMatch, sizeof(shortestMatch), names[i]);
                break;
            }
        }
    }

    if (matchCount > 1) {
        Com_Printf("]%s\n", completionField->buffer);

        Name_PlayerNameCompletion(names, nameCount, PrintMatches);
    }

    whitespace = nameCount == 1 ? qtrue : qfalse;
    if (!Field_CompletePlayerNameFinal(whitespace)) {
    }
}

int QDECL Com_strCompare(const void* a, const void* b) {
    const char** pa = (const char**)a;
    const char** pb = (const char**)b;
    return strcmp(*pa, *pb);
}