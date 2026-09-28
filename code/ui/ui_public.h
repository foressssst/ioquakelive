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
//
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

// [ioquakelive] a Steam friend as the interface sees one. It is steamFriend_t without the
// Steam ID: actions travel by index, so the module never has to push 64 bits through the
// engine call.
typedef struct {
    char name[64];
    int state;          // 0 desconectado, 1 conectado, 2 ocupado, 3 ausente, 4 dormido
    qboolean playing;   // esta jugando a algo
    qboolean sameGame;  // and that something is Quake Live
    qboolean server;    // and they can be reached on a server
    char steamId[24];   // for the page that shows their stats
    char address[48];   // the server they are on, "" for none
} uiFriend_t;

// what trap_SteamFriendAction knows how to do with one
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
    UI_GET_CDKEY,   // [Q3 remnant] unused - do not remove (would shift enum values)
    UI_SET_CDKEY,   // [Q3 remnant] unused - do not remove (would shift enum values)
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
    UI_VERIFY_CDKEY,  // [Q3 remnant] unused - do not remove (would shift enum values)
    UI_LAN_SERVERSTATUS,
    UI_LAN_GETSERVERPING,
    UI_LAN_SERVERISVISIBLE,
    UI_LAN_COMPARESERVERS,
    // 1.32
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

    UI_R_FONT_DRAWSTRING,  // [QL] fontstash/stb_truetype text rendering
    UI_R_FONT_TEXTEXTENTS,
    UI_R_GETGLYPHINFO,
    UI_IME_SETCOMPOSITIONFONT,

    // [QL] what retail's web UI read through GetFactoryList, GetMapList and
    // GetConfig, for Start Match
    UI_FACTORY_COUNT,
    UI_FACTORY_VALUE,
    UI_FACTORY_CVAR,
    UI_ARENA_COUNT,
    UI_ARENA_VALUE,
    UI_CVAR_LATCHEDSTRINGBUFFER,
    // [ioquakelive] what cgame already has as CG_R_FONT_REGISTER: loads a TrueType
    // font by name for the front end's own typeface, returns its index or -1
    UI_R_FONT_REGISTER,
    // [ioquakelive] opens a link in the player's browser, which is what retail's
    // web UI did through the Steam overlay (OpenSteamOverlayURL). Only the UI can
    // ask for it: a console command would let a server send one by stufftext.
    UI_SYS_OPENURL,
    // [ioquakelive] the Steam friends for the menu's social strip. In retail this list came
    // from the web page (GetFriendList of the JavaScript bridge), which does not exist here;
    // the data is the same the console's "friends" command prints.
    UI_STEAM_FRIEND_COUNT,
    UI_STEAM_FRIEND_GET,
    UI_STEAM_FRIEND_ACTION,
    // [ioquakelive] what retail's browser asked of its JavaScript bridge besides the list:
    // RefreshList, RequestServerDetails and SetFavoriteServer
    UI_LAN_STEAM_REFRESH,
    UI_LAN_STEAM_DETAILS,
    UI_LAN_STEAM_DETAILS_GET,
    UI_LAN_STEAM_FAVORITE,
    // [ioquakelive] a server's name as it is: the info string loses one with a backslash in it
    UI_LAN_SERVER_NAME,
    // [ioquakelive] a player's Steam stats, for Career Stats and the profile card: what retail's
    // pages asked of the JavaScript bridge with RequestUserStats and got on
    // users.stats.<id>.received
    UI_STEAM_USER_STATS,
    // [ioquakelive] a player's Steam profile in the overlay, which retail's player tables
    // opened with ActivateGameOverlayToUser("steamid", ...)
    UI_STEAM_SHOW_PROFILE
} uiImport_t;

// [ioquakelive] what trap_SteamUserStats hands over: the state (UI_DETAILS_*), the player's
// Steam name and every stat of qcommon/q_steamstats.h's table, in its order
#define UI_STEAM_NUM_STATS 88
typedef struct {
    int state;
    char name[64];
    int stats[UI_STEAM_NUM_STATS];
} uiUserStats_t;

// what trap_LAN_SteamDetailsGet says about the request
#define UI_DETAILS_NONE 0
#define UI_DETAILS_LOADING 1
#define UI_DETAILS_DONE 2
#define UI_DETAILS_FAILED 3

typedef enum {
    UIMENU_NONE,
    UIMENU_MAIN,
    UIMENU_INGAME,
    UIMENU_MAIN_OPTIONS,
    UIMENU_BAD_CD_KEY,  // [Q3 remnant] unused - do not remove (would shift enum values)
    UIMENU_TEAM,
    UIMENU_POSTGAME,
    // [ioquakelive] the front end over a match: what retail's "Main Menu" button did by
    // showing the browser at its home page. Last, so nothing above moves.
    UIMENU_HOME
} uiMenuCommand_t;

#define SORT_HOST 0
#define SORT_MAP 1
#define SORT_CLIENTS 2
#define SORT_GAME 3
#define SORT_PING 4
typedef enum {
    UI_GETAPIVERSION = 0,  // system reserved

    UI_INIT,
    //	void	UI_Init( void );

    UI_SHUTDOWN,
    //	void	UI_Shutdown( void );

    UI_KEY_EVENT,
    //	void	UI_KeyEvent( int key );

    UI_MOUSE_EVENT,
    //	void	UI_MouseEvent( int dx, int dy );

    UI_REFRESH,
    //	void	UI_Refresh( int time );

    UI_IS_FULLSCREEN,
    //	qboolean UI_IsFullscreen( void );

    UI_SET_ACTIVE_MENU,
    //	void	UI_SetActiveMenu( uiMenuCommand_t menu );

    UI_CONSOLE_COMMAND,
    //	qboolean UI_ConsoleCommand( int realTime );

    UI_DRAW_CONNECT_SCREEN,
    //	void	UI_DrawConnectScreen( qboolean overlay );
    // if !overlay, the background will be drawn, otherwise it will be
    // overlayed over whatever the cgame has drawn.
    // a GetClientState syscall will be made to get the current strings

    UI_HASUNIQUECDKEY,  // [Q3 remnant] always returns qtrue - do not remove (would shift enum values)

    // [QL additions]
    UI_REGISTER_CVARS,
    //	void	UI_RegisterCvars( void );

    UI_CHECK_ACTIVE_MENU,
    //	qboolean UI_CheckActiveMenu( void );

    UI_WALK_MENUS,
    //	void	UI_WalkMenus( void (*callback)(void *menu) );

    UI_DRAW_ADVERTISEMENT,
    //	void	UI_DrawAdvertisement( void );

    UI_MOUSE_POSITION,
    //	void	UI_MousePosition( int x, int y );
    // [ioquakelive] the system pointer's window pixels, while the mouse is let go

} uiExport_t;

#endif
