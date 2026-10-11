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
#ifndef _QCOMMON_H_
#define _QCOMMON_H_

#include "../qcommon/cm_public.h"

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

typedef struct {
    qboolean allowoverflow;
    qboolean overflowed;
    qboolean oob;
    byte* data;
    int maxsize;
    int cursize;
    int readcount;
    int bit;
} msg_t;

void MSG_Init(msg_t* buf, byte* data, int length);
void MSG_InitOOB(msg_t* buf, byte* data, int length);
void MSG_Clear(msg_t* buf);
void MSG_WriteData(msg_t* buf, const void* data, int length);
void MSG_Bitstream(msg_t* buf);

void MSG_Copy(msg_t* buf, byte* data, int length, msg_t* src);

struct usercmd_s;
struct entityState_s;
struct playerState_s;

void MSG_WriteBits(msg_t* msg, int value, int bits);

void MSG_WriteChar(msg_t* sb, int c);
void MSG_WriteByte(msg_t* sb, int c);
void MSG_WriteShort(msg_t* sb, int c);
void MSG_WriteLong(msg_t* sb, int c);
void MSG_WriteFloat(msg_t* sb, float f);
void MSG_WriteString(msg_t* sb, const char* s);
void MSG_WriteBigString(msg_t* sb, const char* s);
void MSG_WriteAngle16(msg_t* sb, float f);
int MSG_HashKey(const char* string, int maxlen);

void MSG_BeginReading(msg_t* sb);
void MSG_BeginReadingOOB(msg_t* sb);

int MSG_ReadBits(msg_t* msg, int bits);

int MSG_ReadChar(msg_t* sb);
int MSG_ReadByte(msg_t* sb);
int MSG_ReadShort(msg_t* sb);
int MSG_ReadLong(msg_t* sb);
float MSG_ReadFloat(msg_t* sb);
char* MSG_ReadString(msg_t* sb);
char* MSG_ReadBigString(msg_t* sb);
char* MSG_ReadStringLine(msg_t* sb);
float MSG_ReadAngle16(msg_t* sb);
void MSG_ReadData(msg_t* sb, void* buffer, int size);
int MSG_LookaheadByte(msg_t* msg);

void MSG_WriteDeltaUsercmdKey(msg_t* msg, int key, usercmd_t* from, usercmd_t* to);
void MSG_ReadDeltaUsercmdKey(msg_t* msg, int key, usercmd_t* from, usercmd_t* to);

void MSG_WriteDeltaEntity(msg_t* msg, struct entityState_s* from, struct entityState_s* to, qboolean force);
void MSG_ReadDeltaEntity(msg_t* msg, entityState_t* from, entityState_t* to, int number);

void MSG_WriteDeltaPlayerstate(msg_t* msg, struct playerState_s* from, struct playerState_s* to);
void MSG_ReadDeltaPlayerstate(msg_t* msg, struct playerState_s* from, struct playerState_s* to);

void MSG_ReportChangeVectors_f(void);

#define NET_ENABLEV4 0x01
#define NET_ENABLEV6 0x02
#define NET_PRIOV6 0x04
#define NET_DISABLEMCAST 0x08

#define PACKET_BACKUP 32
#define PACKET_MASK (PACKET_BACKUP - 1)

#define MAX_PACKET_USERCMDS 32

#define MAX_SNAPSHOT_ENTITIES 256

#define PORT_ANY -1

#define MAX_RELIABLE_COMMANDS 64

typedef enum {
    NA_BAD = 0,
    NA_BOT,
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IP6,
    NA_MULTICAST6,
    NA_UNSPEC
} netadrtype_t;

typedef enum {
    NS_CLIENT,
    NS_SERVER
} netsrc_t;

#define NET_ADDRSTRMAXLEN 48
typedef struct {
    netadrtype_t type;

    byte ip[4];
    byte ip6[16];

    unsigned short port;
    unsigned long scope_id;
} netadr_t;

void NET_Init(void);
void NET_Shutdown(void);
void NET_Restart_f(void);
void NET_Config(qboolean enableNetworking);
void NET_FlushPacketQueue(void);
void NET_SendPacket(netsrc_t sock, int length, const void* data, netadr_t to);
void QDECL NET_OutOfBandPrint(netsrc_t net_socket, netadr_t adr, const char* format, ...) __attribute__((format(printf, 3, 4)));
void NET_OutOfBandRaw(netsrc_t sock, netadr_t adr, const byte* data, int len);
void QDECL NET_OutOfBandData(netsrc_t sock, netadr_t adr, byte* format, int len);

qboolean NET_CompareAdr(netadr_t a, netadr_t b);
qboolean NET_CompareBaseAdrMask(netadr_t a, netadr_t b, int netmask);
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
qboolean NET_IsLocalAddress(netadr_t adr);
const char* NET_AdrToString(netadr_t a);
const char* NET_AdrToStringwPort(netadr_t a);
int NET_StringToAdr(const char* s, netadr_t* a, netadrtype_t family);
qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t* net_from, msg_t* net_message);
void NET_JoinMulticast6(void);
void NET_LeaveMulticast6(void);
void NET_Sleep(int msec);

#define MAX_MSGLEN 32768

#define MAX_DOWNLOAD_WINDOW 48
#define MAX_DOWNLOAD_BLKSIZE 1024

typedef struct {
    netsrc_t sock;

    int dropped;

    netadr_t remoteAddress;
    int qport;

    int incomingSequence;
    int outgoingSequence;

    int fragmentSequence;
    int fragmentLength;
    byte fragmentBuffer[MAX_MSGLEN];

    qboolean unsentFragments;
    int unsentFragmentStart;
    int unsentLength;
    byte unsentBuffer[MAX_MSGLEN];

    int challenge;
    int lastSentTime;
    int lastSentSize;

} netchan_t;

void Netchan_Init(int qport);
void Netchan_Setup(netsrc_t sock, netchan_t* chan, netadr_t adr, int qport, int challenge);

void Netchan_Transmit(netchan_t* chan, int length, const byte* data);
void Netchan_TransmitNextFragment(netchan_t* chan);

qboolean Netchan_Process(netchan_t* chan, msg_t* msg);

#define PROTOCOL_VERSION 91

extern int demo_protocols[];

#define PORT_MASTER 27950
#define PORT_UPDATE 27951
#define PORT_SERVER 27960
#define NUM_SERVER_PORTS 4

enum svc_ops_e {
    svc_bad,
    svc_nop,
    svc_gamestate,
    svc_configstring,
    svc_baseline,
    svc_serverCommand,
    svc_download,
    svc_snapshot,
    svc_EOF,
};

enum clc_ops_e {
    clc_bad,
    clc_nop,
    clc_move,
    clc_moveNoDelta,
    clc_clientCommand,
    clc_EOF,
};

typedef struct vm_s vm_t;

typedef enum {
    TRAP_MEMSET = 100,
    TRAP_MEMCPY,
    TRAP_STRNCPY,
    TRAP_SIN,
    TRAP_COS,
    TRAP_ATAN2,
    TRAP_SQRT,
    TRAP_MATRIXMULTIPLY,
    TRAP_ANGLEVECTORS,
    TRAP_PERPENDICULARVECTOR,
    TRAP_FLOOR,
    TRAP_CEIL,

    TRAP_TESTPRINTINT,
    TRAP_TESTPRINTFLOAT
} sharedTraps_t;

typedef intptr_t(QDECL* vmMainProc)(int callNum, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11);

void VM_Init(void);
vm_t* VM_Create(const char* module, intptr_t (*systemCalls)(intptr_t*));

void VM_Free(vm_t* vm);
void VM_Clear(void);
void VM_Forced_Unload_Start(void);
void VM_Forced_Unload_Done(void);
vm_t* VM_Restart(vm_t* vm, qboolean unpure);

intptr_t QDECL VM_Call(vm_t* vm, int callNum, ...);

void VM_Debug(int level);

void* VM_ArgPtr(intptr_t intValue);
void* VM_ExplicitArgPtr(vm_t* vm, intptr_t intValue);

#define VMA(x) VM_ArgPtr(args[x])
static ID_INLINE float _vmf(intptr_t x) {
    floatint_t fi;
    fi.i = (int)x;
    return fi.f;
}
#define VMF(x) _vmf(args[x])

void Cbuf_Init(void);

void Cbuf_AddText(const char* text);

void Cbuf_ExecuteText(int exec_when, const char* text);

void Cbuf_Execute(void);

typedef void (*xcommand_t)(void);

void Cmd_Init(void);

void Cmd_AddCommand(const char* cmd_name, xcommand_t function);

void Cmd_RemoveCommand(const char* cmd_name);

typedef void (*completionFunc_t)(char* args, int argNum);

void Cmd_RemoveCommandSafe(const char* cmd_name);

void Cmd_CommandCompletion(void (*callback)(const char* s));
void Cmd_SetCommandCompletionFunc(const char* command,
                                  completionFunc_t complete);
void Cmd_CompleteArgument(const char* command, char* args, int argNum);
void Cmd_CompleteCfgName(char* args, int argNum);

int Cmd_Argc(void);
char* Cmd_Argv(int arg);
void Cmd_ArgvBuffer(int arg, char* buffer, int bufferLength);
char* Cmd_Args(void);
char* Cmd_ArgsFrom(int arg);
void Cmd_ArgsBuffer(char* buffer, int bufferLength);
char* Cmd_Cmd(void);
void Cmd_Args_Sanitize(void);

void Cmd_TokenizeString(const char* text);
void Cmd_TokenizeStringIgnoreQuotes(const char* text_in);
int Cmd_ArgOffset(int arg);
int Cmd_ArgIndexFromOffset(int offset);
qboolean Cmd_ArgQuoted(int arg);

typedef void (QDECL* printf_t)(const char* fmt, ...);
typedef void (*helpSearchCallback_t)(const char* name, const char* help, const char* pattern, qboolean isCvar);
void Cmd_SetHelp(const char* cmd_name, const char* help);
qboolean Cmd_GetHelp(const char* cmd_name, const char** name, const char** help, qboolean* fromModule);
void Cmd_EnumHelp(helpSearchCallback_t callback, const char* pattern);

void Cmd_ExecuteString(const char* text);

cvar_t* Cvar_Get(const char* var_name, const char* value, int flags);
cvar_t* Cvar_GetRange(const char* var_name, const char* value, const char* minValue, const char* maxValue, int flags);

void Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags);
void Cvar_RegisterRange(vmCvar_t* vmCvar, const char* varName, const char* defaultValue,
                        const char* minValue, const char* maxValue, int flags);

void Cvar_Update(vmCvar_t* vmCvar);

void Cvar_Set(const char* var_name, const char* value);

cvar_t* Cvar_Set2(const char* var_name, const char* value, qboolean force);

void Cvar_SetSafe(const char* var_name, const char* value);

void Cvar_SetLatched(const char* var_name, const char* value);

void Cvar_SetValue(const char* var_name, float value);
void Cvar_SetValueSafe(const char* var_name, float value);

float Cvar_VariableValue(const char* var_name);
int Cvar_VariableIntegerValue(const char* var_name);

char* Cvar_VariableString(const char* var_name);
void Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);

void Cvar_LatchedVariableStringBuffer(const char* var_name, char* buffer, int bufsize);

int Cvar_Flags(const char* var_name);

void Cvar_CommandCompletion(void (*callback)(const char* s));

void Cvar_Reset(const char* var_name);
void Cvar_ForceReset(const char* var_name);

qboolean Cvar_SaveFactoryValue(const char* var_name);
qboolean Cvar_RestoreFactoryValue(const char* var_name);

void Cvar_SetCheatState(void);

qboolean Cvar_Command(void);

void Cvar_WriteVariables(fileHandle_t f_hw, fileHandle_t f_rep, int clientOnly);

void Cvar_Init(void);

char* Cvar_InfoString(int bit);
char* Cvar_InfoString_Big(int bit);
void Cvar_InfoStringBuffer(int bit, char* buff, int buffsize);
void Cvar_CheckRange(cvar_t* cv, float minVal, float maxVal, qboolean shouldBeIntegral);
void Cvar_SetDescription(cvar_t* var, const char* var_description);
const cvar_t* Cvar_Find(const char* var_name);
void Cvar_SetHelp(const char* var_name, const char* help);
qboolean Cvar_GetHelp(const char* var_name, const char** help);
void Cvar_EnumHelp(helpSearchCallback_t callback, const char* pattern);
const char* Cmd_BuiltInHelp(const char* cmd_name);

const char* Cvar_BuiltInHelp(const char* var_name);
void Cvar_PrintFirstHelpLine(const cvar_t* var, printf_t print);
void Cvar_PrintFlags(const cvar_t* var, printf_t print);

void Cvar_Restart(qboolean unsetVM);
void Cvar_Restart_f(void);

void Cvar_CompleteCvarName(char* args, int argNum);

extern int cvar_modifiedFlags;

#define FS_GENERAL_REF 0x01
#define FS_UI_REF 0x02
#define FS_CGAME_REF 0x04
#define FS_QAGAME_REF 0x08
#define MAX_FILE_HANDLES 64

#define QZCONFIG_CFG "qzconfig.cfg"
#ifndef DEDICATED
#define REPCONFIG_CFG "repconfig.cfg"
#endif

qboolean FS_Initialized(void);

void FS_InitFilesystem(void);
void FS_Shutdown(qboolean closemfp);

qboolean FS_ConditionalRestart(int checksumFeed, qboolean disconnect);
void FS_Restart(int checksumFeed);
void FS_WorkshopRestart(void);
void FS_WarmPrefix(const char* prefix);

void FS_AddGameDirectory(const char* path, const char* dir);

char** FS_ListFiles(const char* directory, const char* extension, int* numfiles);

void FS_FreeFileList(char** list);

qboolean FS_FileExists(const char* file);
const char* FS_FindSystemFile(const char* file);

qboolean FS_CreatePath(char* OSPath);

void FS_FindVM(void** startSearch, char* found, int foundlen, const char* name, int enableDll);

char* FS_BuildOSPath(const char* base, const char* game, const char* qpath);
qboolean FS_CompareZipChecksum(const char* zipfile);

int FS_LoadStack(void);

int FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
int FS_GetModList(char* listbuf, int bufsize);

void FS_GetModDescription(const char* modDir, char* description, int descriptionLen);

fileHandle_t FS_FOpenFileWrite(const char* qpath);
fileHandle_t FS_FOpenFileAppend(const char* filename);
fileHandle_t FS_FOpenFileAppendQuiet(const char* filename);
fileHandle_t FS_FCreateOpenPipeFile(const char* filename);

fileHandle_t FS_SV_FOpenFileWrite(const char* filename);
long FS_SV_FOpenFileRead(const char* filename, fileHandle_t* fp);
void FS_SV_Rename(const char* from, const char* to, qboolean safe);
long FS_FOpenFileRead(const char* qpath, fileHandle_t* file, qboolean uniqueFILE);

int FS_FileIsInPAK(const char* filename, int* pChecksum);
qboolean FS_FileIsInEnginePak(const char* filename);

int FS_Write(const void* buffer, int len, fileHandle_t f);

int FS_Read(void* buffer, int len, fileHandle_t f);

void FS_FCloseFile(fileHandle_t f);

long FS_ReadFileDir(const char* qpath, void* searchPath, qboolean unpure, void** buffer);
long FS_ReadFile(const char* qpath, void** buffer);

void FS_ForceFlush(fileHandle_t f);

void FS_FreeFile(void* buffer);

void FS_WriteFile(const char* qpath, const void* buffer, int size);

long FS_filelength(fileHandle_t f);

int FS_FTell(fileHandle_t f);

void FS_Flush(fileHandle_t f);

void QDECL FS_Printf(fileHandle_t f, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

int FS_FOpenFileByMode(const char* qpath, fileHandle_t* f, fsMode_t mode);

int FS_Seek(fileHandle_t f, long offset, int origin);

qboolean FS_FilenameCompare(const char* s1, const char* s2);

const char* FS_LoadedPakNames(void);
const char* FS_LoadedPakChecksums(void);
const char* FS_LoadedPakPureChecksums(void);

const char* FS_ReferencedPakNames(void);
const char* FS_ReferencedSteamworksItems(void);
const char* FS_ReferencedPakChecksums(void);
const char* FS_ReferencedPakPureChecksums(void);

void FS_ClearPakReferences(int flags);

void FS_PureServerSetReferencedPaks(const char* pakSums, const char* pakNames);
void FS_PureServerSetLoadedPaks(const char* pakSums, const char* pakNames);

qboolean FS_CheckDirTraversal(const char* checkdir);
qboolean FS_InvalidGameDir(const char* gamedir);
qboolean FS_ComparePaks(char* neededpaks, int len, qboolean dlstring);

void FS_Rename(const char* from, const char* to);

void FS_Remove(const char* osPath);
void FS_HomeRemove(const char* homePath);
long FS_ReadCacheFile(const char* name, void** buffer);
void FS_WriteCacheFile(const char* name, const void* buffer, int size);

void FS_FilenameCompletion(const char* dir, const char* ext, qboolean stripExt, void (*callback)(const char* s), qboolean allowNonPureFilesOnDisk);

const char* FS_GetCurrentGameDir(void);
qboolean FS_Which(const char* filename, void* searchPath);

qboolean FS_ExtractGamecode(const char* module, char* outOSPath);

#define MAX_EDIT_LINE 256
typedef struct {
    int cursor;
    int scroll;
    int widthInChars;
    char buffer[MAX_EDIT_LINE];
    int acOffset;
    int acLength;
} field_t;

void Field_Clear(field_t* edit);
void Field_AutoComplete(field_t* edit);
void Field_CompleteKeyname(void);
void Field_CompleteList(const char* const* list, int count);
void Field_CompleteFilename(const char* dir,
                            const char* ext,
                            qboolean stripExt,
                            qboolean allowNonPureFilesOnDisk);
void Field_CompleteCommand(char* cmd,
                           qboolean doCommands,
                           qboolean doCvars);
void Field_CompletePlayerName(const char** names, int count);
void Field_ResetCompletionCycle(void);
void Field_InsertCVarValue(field_t* edit, const cvar_t* cvar, qboolean defaultValue);
void Field_InsertBindCommand(field_t* edit, const char* key, const char* command);

typedef enum {
    CF_RDTSC = 1 << 0,
    CF_MMX = 1 << 1,
    CF_MMX_EXT = 1 << 2,
    CF_3DNOW = 1 << 3,
    CF_3DNOW_EXT = 1 << 4,
    CF_SSE = 1 << 5,
    CF_SSE2 = 1 << 6,
    CF_ALTIVEC = 1 << 7
} cpuFeatures_t;

#define MAXPRINTMSG 4096

typedef enum {
    SE_NONE = 0,
    SE_KEY,
    SE_CHAR,
    SE_MOUSE,
    SE_JOYSTICK_AXIS,
    SE_CONSOLE,
    SE_MOUSE_ABS
} sysEventType_t;

typedef struct {
    int evTime;
    sysEventType_t evType;
    int evValue, evValue2;
    int evPtrLength;
    void* evPtr;
} sysEvent_t;

void Com_QueueEvent(int time, sysEventType_t type, int value, int value2, int ptrLength, void* ptr);
int Com_EventLoop(void);
sysEvent_t Com_GetSystemEvent(void);

char* CopyString(const char* in);
void Info_Print(const char* s);

void Com_BeginRedirect(char* buffer, int buffersize, void (*flush)(char*));
void Com_EndRedirect(void);
void QDECL Com_Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void QDECL Com_DPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void QDECL Com_Error(int code, const char* fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void Com_Quit_f(void) __attribute__((noreturn));
void Com_GameRestart(int checksumFeed, qboolean disconnect);

int Com_Milliseconds(void);
unsigned Com_BlockChecksum(const void* buffer, int length);
char* Com_MD5File(const char* filename, int length, const char* prefix, int prefix_len);

int Com_Filter(char* filter, char* name, int casesensitive);
int Com_FilterPath(char* filter, char* name, int casesensitive);

typedef enum {
    PHR_NOTFOUND,
    PHR_NOHELP,
    PHR_HADHELP
} printHelpResult_t;
printHelpResult_t Com_PrintHelp(const char* name, printf_t print, qboolean printNotFound, qboolean printModules, qboolean printFlags);
qboolean Com_IsValidHexColor(const char* s, qboolean hasAlpha);
void Com_ParseHexColor(float* c, const char* text, qboolean hasAlpha);
int Com_RealTime(qtime_t* qtime);
qboolean Com_SafeMode(void);
void Com_RunAndTimeServerPacket(netadr_t* evFrom, msg_t* buf);

void Com_StartupVariable(const char* match);

qboolean Com_PlayerNameToFieldString(char* str, int length, const char* name);
qboolean Com_FieldStringToPlayerName(char* name, int length, const char* rawname);
int QDECL Com_strCompare(const void* a, const void* b);

extern cvar_t* com_developer;
extern cvar_t* com_dedicated;
extern cvar_t* com_speeds;
extern cvar_t* com_timescale;
extern cvar_t* com_sv_running;
extern cvar_t* com_cl_running;
extern cvar_t* com_version;
extern cvar_t* com_blood;
extern cvar_t* com_journal;
extern cvar_t* com_cameraMode;
extern cvar_t* com_ansiColor;
extern cvar_t* com_unfocused;
extern cvar_t* com_maxfpsUnfocused;
extern cvar_t* com_minimized;
extern cvar_t* com_maxfpsMinimized;
extern cvar_t* com_basegame;
extern cvar_t* com_homepath;

extern cvar_t* cl_paused;
extern cvar_t* sv_paused;

extern cvar_t* cl_packetdelay;
extern cvar_t* sv_packetdelay;

extern cvar_t* com_gamename;
extern cvar_t* com_protocol;
#ifndef DEDICATED
extern cvar_t* cl_allowConsoleChat;
extern cvar_t* com_allowConsole;
#endif

extern int time_game;
extern int time_frontend;
extern int time_backend;

extern int com_frameTime;

extern qboolean com_errorEntered;
extern qboolean com_fullyInitialized;

extern fileHandle_t com_journalFile;
extern fileHandle_t com_journalDataFile;

typedef enum {
    TAG_FREE,
    TAG_GENERAL,
    TAG_BOTLIB,
    TAG_RENDERER,
    TAG_SMALL,
    TAG_STATIC
} memtag_t;

#if !defined(NDEBUG) && !defined(BSPC)
#define ZONE_DEBUG
#endif

#ifdef ZONE_DEBUG
#define Z_TagMalloc(size, tag) Z_TagMallocDebug(size, tag, #size, __FILE__, __LINE__)
#define Z_Malloc(size) Z_MallocDebug(size, #size, __FILE__, __LINE__)
#define S_Malloc(size) S_MallocDebug(size, #size, __FILE__, __LINE__)
void* Z_TagMallocDebug(int size, int tag, char* label, char* file, int line);
void* Z_MallocDebug(int size, char* label, char* file, int line);
void* S_MallocDebug(int size, char* label, char* file, int line);
#else
void* Z_TagMalloc(int size, int tag);
void* Z_Malloc(int size);
void* S_Malloc(int size);
#endif
void Z_Free(void* ptr);
void Z_FreeTags(int tag);
int Z_AvailableMemory(void);
void Z_LogHeap(void);

void Hunk_Clear(void);
void Hunk_ClearToMark(void);
void Hunk_SetMark(void);
qboolean Hunk_CheckMark(void);
void Hunk_ClearTempMemory(void);
void* Hunk_AllocateTempMemory(int size);
void Hunk_FreeTempMemory(void* buf);
int Hunk_MemoryRemaining(void);
void Hunk_Log(void);

void Com_TouchMemory(void);

void Com_Init(char* commandLine);
void Com_Frame(void);
void Com_Shutdown(void);

void CL_InitKeyCommands(void);

void CL_Init(void);
void CL_RegisterCGameCvars(void);
void CL_Disconnect(qboolean showMainMenu);
void CL_Shutdown(char* finalmsg, qboolean disconnect, qboolean quit);
void CL_Frame(int msec);
void CL_LowLatencyWait(void);
int CL_DisplayFrequency(void);
qboolean CL_GameCommand(void);
qboolean CL_Connected(void);
qboolean Com_Redirected(void);
void *CL_FindAlias(const char *name);
void CL_RunAlias(void);
void CL_InitAliases(void);
void Cmd_WriteAliases(fileHandle_t f);
void CL_KeyEvent(int key, qboolean down, unsigned time);

void CL_CharEvent(int key);

void CL_MouseEvent(int dx, int dy, int time);
void CL_MouseAbsEvent(int x, int y);

void CL_JoystickEvent(int axis, int value, int time);

void CL_PacketEvent(netadr_t from, msg_t* msg);

void CL_ConsolePrint(char* text);
int Con_GetLineWidth(void);

void CL_MapLoading(void);

void CL_ForwardCommandToServer(const char* string);

void CL_FlushMemory(void);

void CL_ShutdownAll(qboolean shutdownRef);

void CL_InitRef(void);

void CL_StartHunkUsers(qboolean rendererOnly);

void CL_Snd_Shutdown(void);

void Key_KeynameCompletion(void (*callback)(const char* s));

void Key_WriteBindings(fileHandle_t f);

void S_ClearSoundBuffer(void);

void SCR_DebugGraph(float value);

#define AVI_LINE_PADDING 4

void SV_Init(void);
#ifdef DEDICATED
void SV_WorkshopInit(void);
#endif
void SV_RegisterGameCvars(void);
void SV_Shutdown(char* finalmsg);
void SV_Frame(int msec);
void SV_VoiceFrame(void);
void SV_SteamAuthResponse(uint64_t steamId, int response);
void SV_SteamFrame(qboolean force);
void SV_SteamConnected(void);
void SV_SteamStatsReceived(int result);
void SV_SteamStatsStored(int result);
void SV_SteamStatsRemoveUser(uint64_t steamId);

#define MAX_COMPLETION_NAMES 1024
int SV_ArenaNameList(const char** out, int max);
int SV_FactoryNameList(const char** out, int max);
void SV_PacketEvent(netadr_t from, msg_t* msg);
int SV_FrameMsec(void);
qboolean SV_GameCommand(void);
int SV_SendQueuedPackets(void);
int SV_NumFactories(void);
qboolean SV_FactoryValue(int num, const char* key, char* buf, int size);
qboolean SV_FactoryCvar(int num, int index, char* name, int nameSize, char* value, int valueSize);
int SV_NumArenas(void);
qboolean SV_ArenaValue(int num, const char* key, char* buf, int size);

qboolean UI_GameCommand(void);
void IN_Init(void* windowData);
void IN_Frame(void);
void IN_Shutdown(void);
void IN_Restart(void);
void IN_ListInputDevices(void);

#define MAX_JOYSTICK_AXIS 16

void Sys_Init(void);

void* QDECL Sys_LoadGameDll(const char* name, vmMainProc* entryPoint, intptr_t(QDECL* systemcalls)(intptr_t, ...));
void Sys_UnloadDll(void* dllHandle);

qboolean Sys_DllExtension(const char* name);

char* Sys_GetCurrentUser(void);

void QDECL Sys_Error(const char* error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Sys_Quit(void) __attribute__((noreturn));
char* Sys_GetClipboardData(void);
void Sys_SetClipboardData(const char* text);
void Sys_OpenURL(const char* url);

void Sys_Print(const char* msg);

int Sys_Milliseconds(void);
int64_t Sys_Microseconds(void);

void* Sys_ThreadCreate(void (*function)(void*), void* argument);
void* Sys_MutexCreate(void);
void Sys_MutexLock(void* mutex);
void Sys_MutexUnlock(void* mutex);
void* Sys_SemaphoreCreate(void);
void Sys_SemaphorePost(void* semaphore);
void Sys_SemaphoreWait(void* semaphore);
void Sys_ThreadYield(void);
int Sys_ProcessorCount(void);

void Com_JobsInit(void);
void Com_JobsSubmit(void (*function)(void*), void* data);
int Com_JobWorkers(void);

qboolean Sys_RandomBytes(byte* string, int len);

cpuFeatures_t Sys_GetProcessorFeatures(void);

void Sys_SetErrorText(const char* text);

void Sys_SendPacket(int length, const void* data, netadr_t to);

qboolean Sys_StringToAdr(const char* s, netadr_t* a, netadrtype_t family);

qboolean Sys_IsLANAddress(netadr_t adr);
void Sys_ShowIP(void);

FILE* Sys_FOpen(const char* ospath, const char* mode);
qboolean Sys_Mkdir(const char* path);
FILE* Sys_Mkfifo(const char* ospath);
char* Sys_Cwd(void);
void Sys_SetDefaultInstallPath(const char* path);
char* Sys_DefaultInstallPath(void);
char* Sys_SteamPath(void);
char* Sys_GogPath(void);
char* Sys_MicrosoftStorePath(void);

#ifdef __APPLE__
char* Sys_DefaultAppPath(void);
#ifndef DEDICATED
qboolean Sys_LocatePak0(const char *destDir);
#endif
#endif

void Sys_SetDefaultHomePath(const char* path);
char* Sys_DefaultHomePath(void);
char* Sys_DefaultCachePath(void);
qboolean Sys_RemoveDir(const char* path);
const char* Sys_Dirname(char* path);
const char* Sys_BinaryName(void);
const char* Sys_Basename(char* path);
char* Sys_ConsoleInput(void);

char** Sys_ListFiles(const char* directory, const char* extension, char* filter, int* numfiles, qboolean wantsubs);
void Sys_FreeFileList(char** list);
void Sys_Sleep(int msec);

qboolean Sys_LowPhysicalMemory(void);

void Sys_SetEnv(const char* name, const char* value);

typedef enum {
    DR_YES = 0,
    DR_NO = 1,
    DR_OK = 0,
    DR_CANCEL = 1
} dialogResult_t;

typedef enum {
    DT_INFO,
    DT_WARNING,
    DT_ERROR,
    DT_YES_NO,
    DT_OK_CANCEL
} dialogType_t;

dialogResult_t Sys_Dialog(dialogType_t type, const char* message, const char* title);

void Sys_RemovePIDFile(const char* gamedir);
qboolean Sys_WritePIDFile(const char* gamedir);
int Sys_PID(void);

#define NYT HMAX
#define INTERNAL_NODE (HMAX + 1)

typedef struct nodetype {
    struct nodetype *left, *right, *parent;
    struct nodetype *next, *prev;
    struct nodetype** head;
    int weight;
    int symbol;
} node_t;

#define HMAX 256

typedef struct {
    int blocNode;
    int blocPtrs;

    node_t* tree;
    node_t* lhead;
    node_t* ltail;
    node_t* loc[HMAX + 1];
    node_t** freelist;

    node_t nodeList[768];
    node_t* nodePtrs[768];
} huff_t;

typedef struct {
    huff_t compressor;
    huff_t decompressor;
} huffman_t;

void Huff_Compress(msg_t* buf, int offset);
void Huff_Decompress(msg_t* buf, int offset);
void Huff_Init(huffman_t* huff);
void Huff_addRef(huff_t* huff, byte ch);
int Huff_Receive(node_t* node, int* ch, byte* fin);
void Huff_transmit(huff_t* huff, int ch, byte* fout, int maxoffset);
void Huff_offsetReceive(node_t* node, int* ch, byte* fin, int* offset, int maxoffset);
void Huff_offsetTransmit(huff_t* huff, int ch, byte* fout, int* offset, int maxoffset);
void Huff_putBit(int bit, byte* fout, int* offset);
int Huff_getBit(byte* fout, int* offset);

int Huff_getBloc(void);
void Huff_setBloc(int _bloc);

extern huffman_t clientHuffTables;

#define SV_ENCODE_START 4
#define SV_DECODE_START 12
#define CL_ENCODE_START 12
#define CL_DECODE_START 4

#define DLF_ENABLE 1
#define DLF_NO_REDIRECT 2
#define DLF_NO_UDP 4
#define DLF_NO_DISCONNECT 8

#endif
