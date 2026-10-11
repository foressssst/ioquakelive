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
#ifndef __UI_PUBLIC_H__
#define __UI_PUBLIC_H__

#define UI_API_VERSION 8

typedef struct {
    connstate_t connState;
    int connectPacketCount;
    int clientNum;
    char servername[MAX_STRING_CHARS];
    char updateInfoString[MAX_STRING_CHARS];
    char messageString[MAX_STRING_CHARS];
} uiClientState_t;

typedef struct {
    char name[64];
    int state;
    qboolean playing;
    qboolean sameGame;
    qboolean server;
    char steamId[24];
    char address[48];
    qboolean lobby;
    char nickname[64];
    char status[128];
} uiFriend_t;

#define UI_FRIEND_JOIN 0
#define UI_FRIEND_INVITE 1
#define UI_FRIEND_PROFILE 2
#define UI_FRIEND_MESSAGE 3
#define UI_FRIEND_ACHIEVEMENTS 4

typedef enum {
    UI_ERROR,
    UI_PRINT,
    UI_MILLISECONDS,
    UI_CVAR_SET,
    UI_CVAR_VARIABLEVALUE,
    UI_CVAR_VARIABLESTRINGBUFFER,
    UI_CVAR_SETVALUE,
    UI_CVAR_RESET,
    UI_CVAR_CREATE,
    UI_CVAR_INFOSTRINGBUFFER,
    UI_ARGC,
    UI_ARGV,
    UI_CMD_EXECUTETEXT,
    UI_FS_FOPENFILE,
    UI_FS_READ,
    UI_FS_WRITE,
    UI_FS_FCLOSEFILE,
    UI_FS_GETFILELIST,
    UI_R_REGISTERMODEL,
    UI_R_REGISTERSKIN,
    UI_R_REGISTERSHADERNOMIP,
    UI_R_CLEARSCENE,
    UI_R_ADDREFENTITYTOSCENE,
    UI_R_ADDPOLYTOSCENE,
    UI_R_ADDLIGHTTOSCENE,
    UI_R_RENDERSCENE,
    UI_R_SETCOLOR,
    UI_R_DRAWSTRETCHPIC,
    UI_UPDATESCREEN,
    UI_CM_LERPTAG,
    UI_CM_LOADMODEL,
    UI_S_REGISTERSOUND,
    UI_S_STARTLOCALSOUND,
    UI_KEY_KEYNUMTOSTRINGBUF,
    UI_KEY_GETBINDINGBUF,
    UI_KEY_SETBINDING,
    UI_KEY_ISDOWN,
    UI_KEY_GETOVERSTRIKEMODE,
    UI_KEY_SETOVERSTRIKEMODE,
    UI_KEY_CLEARSTATES,
    UI_KEY_GETCATCHER,
    UI_KEY_SETCATCHER,
    UI_GETCLIPBOARDDATA,
    UI_GETGLCONFIG,
    UI_GETCLIENTSTATE,
    UI_GETCONFIGSTRING,
    UI_LAN_GETPINGQUEUECOUNT,
    UI_LAN_CLEARPING,
    UI_LAN_GETPING,
    UI_LAN_GETPINGINFO,
    UI_CVAR_REGISTER,
    UI_CVAR_UPDATE,
    UI_MEMORY_REMAINING,
    UI_GET_CDKEY,
    UI_SET_CDKEY,
    UI_R_REGISTERFONT,
    UI_R_MODELBOUNDS,
    UI_PC_ADD_GLOBAL_DEFINE,
    UI_PC_LOAD_SOURCE,
    UI_PC_FREE_SOURCE,
    UI_PC_READ_TOKEN,
    UI_PC_SOURCE_FILE_AND_LINE,
    UI_S_STOPBACKGROUNDTRACK,
    UI_S_STARTBACKGROUNDTRACK,
    UI_REAL_TIME,
    UI_LAN_GETSERVERCOUNT,
    UI_LAN_GETSERVERADDRESSSTRING,
    UI_LAN_GETSERVERINFO,
    UI_LAN_MARKSERVERVISIBLE,
    UI_LAN_UPDATEVISIBLEPINGS,
    UI_LAN_RESETPINGS,
    UI_LAN_ADDSERVER,
    UI_LAN_REMOVESERVER,
    UI_CIN_PLAYCINEMATIC,
    UI_CIN_STOPCINEMATIC,
    UI_CIN_RUNCINEMATIC,
    UI_CIN_DRAWCINEMATIC,
    UI_CIN_SETEXTENTS,
    UI_R_REMAP_SHADER,
    UI_VERIFY_CDKEY,
    UI_LAN_SERVERSTATUS,
    UI_LAN_GETSERVERPING,
    UI_LAN_SERVERISVISIBLE,
    UI_LAN_COMPARESERVERS,
    UI_FS_SEEK,
    UI_SET_PBCLSTATUS,

    UI_MEMSET = 100,
    UI_MEMCPY,
    UI_STRNCPY,
    UI_SIN,
    UI_COS,
    UI_ATAN2,
    UI_SQRT,
    UI_FLOOR,
    UI_CEIL,

    UI_R_FONT_DRAWSTRING,
    UI_R_FONT_TEXTEXTENTS,
    UI_R_GETGLYPHINFO,
    UI_IME_SETCOMPOSITIONFONT,

    UI_FACTORY_COUNT,
    UI_FACTORY_VALUE,
    UI_FACTORY_CVAR,
    UI_ARENA_COUNT,
    UI_ARENA_VALUE,
    UI_CVAR_LATCHEDSTRINGBUFFER,
    UI_R_FONT_REGISTER,
    UI_SYS_OPENURL,
    UI_STEAM_FRIEND_COUNT,
    UI_STEAM_FRIEND_GET,
    UI_STEAM_FRIEND_ACTION,
    UI_LAN_STEAM_REFRESH,
    UI_LAN_STEAM_DETAILS,
    UI_LAN_STEAM_DETAILS_GET,
    UI_LAN_STEAM_FAVORITE,
    UI_LAN_SERVER_NAME,
    UI_STEAM_USER_STATS,
    UI_STEAM_SHOW_PROFILE,
    UI_STEAM_AVATAR,
    UI_WEBPAK_EXCERPT,
    UI_WORKSHOP_DOWNLOAD_INFO
} uiImport_t;

#define UI_STEAM_NUM_STATS 88
typedef struct {
    int state;
    char name[64];
    int stats[UI_STEAM_NUM_STATS];
} uiUserStats_t;

#define UI_DETAILS_NONE 0
#define UI_DETAILS_LOADING 1
#define UI_DETAILS_DONE 2
#define UI_DETAILS_FAILED 3

typedef enum {
    UIMENU_NONE,
    UIMENU_MAIN,
    UIMENU_INGAME,
    UIMENU_MAIN_OPTIONS,
    UIMENU_BAD_CD_KEY,
    UIMENU_TEAM,
    UIMENU_POSTGAME,
    UIMENU_HOME
} uiMenuCommand_t;

#define SORT_HOST 0
#define SORT_MAP 1
#define SORT_CLIENTS 2
#define SORT_GAME 3
#define SORT_PING 4
typedef enum {
    UI_GETAPIVERSION = 0,

    UI_INIT,

    UI_SHUTDOWN,

    UI_KEY_EVENT,

    UI_MOUSE_EVENT,

    UI_REFRESH,

    UI_IS_FULLSCREEN,

    UI_SET_ACTIVE_MENU,

    UI_CONSOLE_COMMAND,

    UI_DRAW_CONNECT_SCREEN,

    UI_HASUNIQUECDKEY,

    UI_REGISTER_CVARS,

    UI_CHECK_ACTIVE_MENU,

    UI_WALK_MENUS,

    UI_DRAW_ADVERTISEMENT,

    UI_MOUSE_POSITION,

} uiExport_t;

#endif
