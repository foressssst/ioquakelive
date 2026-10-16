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
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/q_steam.h"
#include "../renderercommon/tr_public.h"
#include "../ui/ui_public.h"
#include "keys.h"
#include "snd_public.h"
#include "../cgame/cg_public.h"
#include "../game/bg_public.h"

#ifdef USE_CURL
#include "cl_curl.h"
#endif

#define RETRANSMIT_TIMEOUT 3000

typedef struct {
    qboolean valid;
    int snapFlags;

    int serverTime;

    int messageNum;
    int deltaNum;
    int ping;
    byte areamask[MAX_MAP_AREA_BYTES];

    int cmdNum;
    playerState_t ps;

    int numEntities;
    int parseEntitiesNum;

    int serverCommandNum;
} clSnapshot_t;

typedef struct {
    int p_cmdNumber;
    int p_serverTime;
    int p_realtime;
} outPacket_t;

#define MAX_PARSE_ENTITIES (PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES)

extern int g_console_field_width;

typedef struct {
    int timeoutcount;
    clSnapshot_t snap;

    int serverTime;
    int oldServerTime;
    int oldFrameServerTime;
    int serverTimeDelta;
    qboolean extrapolatedSnapshot;
    qboolean newSnapshots;

    gameState_t gameState;
    char mapname[MAX_QPATH];

    int parseEntitiesNum;

    int mouseDx[2], mouseDy[2];
    int joystickAxis[MAX_JOYSTICK_AXIS];

    int cgameUserCmdValue;
    int cgameUserCmdWeaponPrimary;
    int cgameUserCmdFov;
    float cgameSensitivity;

    usercmd_t cmds[CMD_BACKUP];
    int cmdNumber;

    outPacket_t outPackets[PACKET_BACKUP];

    vec3_t viewangles;

    int serverId;
    clSnapshot_t snapshots[PACKET_BACKUP];

    entityState_t entityBaselines[MAX_GENTITIES];

    entityState_t parseEntities[MAX_PARSE_ENTITIES];
} clientActive_t;

extern clientActive_t cl;

#define MAX_TIMEDEMO_DURATIONS 4096

typedef struct {
    connstate_t state;

    int clientNum;
    int lastPacketSentTime;
    int lastPacketTime;

    char servername[MAX_OSPATH];
    netadr_t serverAddress;
    int connectTime;
    int connectPacketCount;
    char serverMessage[MAX_STRING_TOKENS];

    int challenge;
    int checksumFeed;

    int reliableSequence;
    int reliableAcknowledge;
    char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

    int serverMessageSequence;

    int serverCommandSequence;
    int lastExecutedServerCommand;
    char serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

    fileHandle_t download;
    char downloadTempName[MAX_OSPATH];
    char downloadName[MAX_OSPATH];
#ifdef USE_CURL
    qboolean cURLEnabled;
    qboolean cURLUsed;
    qboolean cURLDisconnected;
    char downloadURL[MAX_OSPATH];
    CURL* downloadCURL;
    CURLM* downloadCURLM;
#endif
    int sv_allowDownload;
    char sv_dlURL[MAX_CVAR_VALUE_STRING];
    int downloadNumber;
    int downloadBlock;
    int downloadCount;
    int downloadSize;
    char downloadList[MAX_INFO_STRING];
    qboolean downloadRestart;

    char demoName[MAX_QPATH];
    qboolean demorecording;
    qboolean demoplaying;
    qboolean demowaiting;
    qboolean firstDemoFrameSkipped;
    fileHandle_t demofile;

    int timeDemoFrames;
    int timeDemoStart;
    int timeDemoBaseTime;
    int timeDemoLastFrame;
    int timeDemoMinDuration;
    int timeDemoMaxDuration;
    unsigned char timeDemoDurations[MAX_TIMEDEMO_DURATIONS];

    float aviVideoFrameRemainder;
    float aviSoundFrameRemainder;

    netchan_t netchan;
} clientConnection_t;

extern clientConnection_t clc;

typedef struct {
    netadr_t adr;
    int start;
    int time;
    char info[MAX_INFO_STRING];
} ping_t;

typedef struct {
    netadr_t adr;
    char hostName[64];
    char mapName[MAX_NAME_LENGTH];
    char game[MAX_NAME_LENGTH];
    int netType;
    int gameType;
    int clients;
    int maxClients;
    int minPing;
    int maxPing;
    int ping;
    qboolean visible;
    int g_humanplayers;
    int g_needpass;
    int vac;
    char description[64];
    char tags[128];
} serverInfo_t;

typedef struct {
    qboolean rendererStarted;
    qboolean soundStarted;
    qboolean soundRegistered;
    qboolean uiStarted;
    qboolean cgameStarted;

    int framecount;
    int frametime;

    int realtime;
    int realFrametime;

    int numlocalservers;
    serverInfo_t localServers[MAX_OTHER_SERVERS];

    int numglobalservers;
    serverInfo_t globalServers[MAX_GLOBAL_SERVERS];
    int numGlobalServerAddresses;
    netadr_t globalServerAddresses[MAX_GLOBAL_SERVERS];

    int numfavoriteservers;
    serverInfo_t favoriteServers[MAX_OTHER_SERVERS];

    int pingUpdateSource;

    netadr_t updateServer;
    char updateChallenge[MAX_TOKEN_CHARS];
    char updateInfoString[MAX_INFO_STRING];

    netadr_t rconAddress;

    glconfig_t glconfig;
    qhandle_t charSetShader;
    qhandle_t whiteShader;
    qhandle_t consoleShader;
    qhandle_t recordShader;
} clientStatic_t;

extern clientStatic_t cls;

extern char cl_oldGame[MAX_QPATH];
extern qboolean cl_oldGameSet;

extern vm_t* cgvm;
extern vm_t* uivm;
extern refexport_t re;

extern cvar_t* cl_nodelta;
extern cvar_t* cl_debugMove;
extern cvar_t* cl_noprint;
extern cvar_t* cl_timegraph;
extern cvar_t* cl_maxpackets;
extern cvar_t* cl_packetdup;
extern cvar_t* cl_shownet;
extern cvar_t* cl_showSend;
extern cvar_t* cl_timeNudge;
extern cvar_t* cl_autoTimeNudge;
extern cvar_t* cl_showTimeDelta;
extern cvar_t* cl_freezeDemo;

void CL_DemoSeekApply(void);
void CL_DemoGamestate(void);
void CL_DemoSnapshotRead(void);
int CL_DemoSuspend(void);
void CL_DemoResume(int pos);

extern cvar_t* cl_yawspeed;
extern cvar_t* cl_pitchspeed;
extern cvar_t* cl_run;
extern cvar_t* cl_anglespeedkey;

extern cvar_t* cl_sensitivity;
extern cvar_t* cl_freelook;

extern cvar_t* cl_mouseAccel;
extern cvar_t* cl_mouseAccelOffset;
extern cvar_t* cl_mouseAccelPower;
extern cvar_t* cl_mouseAccelDebug;
extern cvar_t* cl_mouseSensCap;
extern cvar_t* cg_ignoreMouseInput;
extern cvar_t* m_cpi;

extern cvar_t* m_pitch;
extern cvar_t* m_yaw;
extern cvar_t* m_forward;
extern cvar_t* m_side;
extern cvar_t* m_filter;

extern cvar_t* j_pitch;
extern cvar_t* j_yaw;
extern cvar_t* j_forward;
extern cvar_t* j_side;
extern cvar_t* j_up;
extern cvar_t* j_pitch_axis;
extern cvar_t* j_yaw_axis;
extern cvar_t* j_forward_axis;
extern cvar_t* j_side_axis;
extern cvar_t* j_up_axis;

extern cvar_t* cl_timedemo;
extern cvar_t* cl_aviFrameRate;
extern cvar_t* cl_aviMotionJpeg;

extern cvar_t* cl_activeAction;

extern cvar_t* cl_allowDownload;
extern cvar_t* cl_downloadMethod;
extern cvar_t* cl_conXOffset;
extern cvar_t* cl_inGameVideo;

extern cvar_t* cl_lanForcePackets;
extern cvar_t* cl_autoRecordDemo;


void CL_Init(void);
void CL_AddReliableCommand(const char* cmd, qboolean isDisconnectCmd);

void CL_StartHunkUsers(qboolean rendererOnly);

void CL_Disconnect_f(void);
void CL_OpenURL(const char* url);
void CL_GetChallengePacket(void);
void CL_Vid_Restart_f(void);

extern cvar_t* cl_demoRecordMessage;
void CL_Snd_Restart_f(void);
void CL_StartDemoLoop(void);
void CL_NextDemo(void);
void CL_ReadDemoMessage(void);
void CL_StopRecord_f(void);

qboolean CL_InitDownloads(void);
void CL_DownloadsComplete(void);
void CL_NextDownload(void);

void CL_GetPing(int n, char* buf, int buflen, int* pingtime);
void CL_GetPingInfo(int n, char* buf, int buflen);
void CL_ClearPing(int n);
int CL_GetPingQueueCount(void);

void CL_ShutdownRef(void);
void CL_InitRef(void);
int CL_ServerStatus(char* serverAddress, char* serverStatusString, int maxLen);

qboolean CL_CheckPaused(void);

typedef struct {
    int down[2];
    unsigned downtime;
    unsigned msec;
    qboolean active;
    qboolean wasPressed;
} kbutton_t;

void CL_InitInput(void);
void CL_ShutdownInput(void);
void CL_SendCmd(void);
void CL_ClearState(void);
void CL_ReadPackets(void);

void CL_WritePacket(void);
void IN_CenterView(void);

void CL_VerifyCode(void);

float CL_KeyState(kbutton_t* key);
int Key_StringToKeynum(char* str);
char* Key_KeynumToString(int keynum);

extern int cl_connectedToPureServer;
extern int cl_connectedToCheatServer;

void CL_SystemInfoChanged(void);
void CL_ParseServerMessage(msg_t* msg);

void CL_ServerInfoPacket(netadr_t from, msg_t* msg);
void CL_LocalServers_f(void);
void CL_GlobalServers_f(void);
void CL_SteamServersFrame(void);
void CL_WorkshopFrame(void);
void CL_WarmPlayersChanged(void);
void CL_UpdateInit(void);
void CL_UpdateFrame(void);
void CL_VoiceFrame(void);
void CL_MatchReportFrame(void);
void CL_MatchMedia(const char* path);
void CL_SetCgameCommandCompletion(const char* cmdName);
qboolean CL_VoiceIsIgnored(uint64_t steamId);
qboolean CL_VoiceToggleIgnore(uint64_t steamId);
void CL_FriendDo(const steamFriend_t* fr, int action);
extern cvar_t* s_voiceVolume;
void CL_FavoriteServers_f(void);
qboolean CL_UpdateVisiblePings_f(int source);

void Con_DrawCharacter(int cx, int line, int num);

void Con_CheckResize(void);
void Con_Init(void);
void Con_Shutdown(void);
void Con_Clear_f(void);
void Con_ToggleConsole_f(void);
void Con_ToggleConsole_Forced(void);
void Con_DrawNotify(void);
void Con_ClearNotify(void);
void Con_RunConsole(void);
void Con_DrawConsole(void);

void Con_MousePosition(int x, int y);
qboolean Con_MouseClick(void);
qboolean Con_LinkPromptActive(void);
qboolean Con_LinkPromptKey(int key);
void Con_PageUp(void);
void Con_PageDown(void);
void Con_Top(void);
void Con_Bottom(void);
void Con_Close(void);
int Con_GetStyle(void);
qboolean Con_HandleMarkMode(qboolean ctrlDown, qboolean shiftDown, int key);
qboolean Con_IsInMarkMode(void);
qboolean Con_SwallowCharEvent(int ch);
void Con_ContinueSearch(qboolean forward);
void Con_ScrollLines(int lines);
void Con_ScrollPages(int pages);
extern cvar_t* con_history;

void CL_LoadConsoleHistory(void);
void CL_SaveConsoleHistory(void);

void SCR_Init(void);
void SCR_UpdateScreen(void);

void SCR_DebugGraph(float value);

int SCR_GetBigStringWidth(const char* str);

void SCR_AdjustFrom640(float* x, float* y, float* w, float* h);
void SCR_FillRect(float x, float y, float width, float height, const float* color);
void SCR_DrawPic(float x, float y, float width, float height, qhandle_t hShader);
void SCR_DrawNamedPic(float x, float y, float width, float height, const char* picname);

void SCR_DrawBigString(int x, int y, const char* s, float alpha, qboolean noColorEscape);
void SCR_DrawBigStringColor(int x, int y, const char* s, vec4_t color, qboolean noColorEscape);
void SCR_DrawSmallStringExt(int x, int y, const char* string, float* setColor, qboolean forceColor, qboolean noColorEscape);
void SCR_DrawSmallChar(int x, int y, int ch);

void CL_PlayCinematic_f(void);
void SCR_DrawCinematic(void);
void SCR_RunCinematic(void);
void SCR_StopCinematic(void);
int CIN_PlayCinematic(const char* arg0, int xpos, int ypos, int width, int height, int bits);
e_status CIN_StopCinematic(int handle);
e_status CIN_RunCinematic(int handle);
void CIN_DrawCinematic(int handle);
void CIN_SetExtents(int handle, int x, int y, int w, int h);
void CIN_SetLooping(int handle, qboolean loop);
void CIN_UploadCinematic(int handle);
void CIN_CloseAllVideos(void);

void CL_InitCGame(void);
void CL_ShutdownCGame(void);
void CL_ResolutionChanged(void);
qboolean CL_GameCommand(void);
void CL_CGameRendering(stereoFrame_t stereo);
void CL_SetCGameTime(void);
void CL_FirstSnapshot(void);
void CL_ShaderStateChanged(void);

void CL_InitUI(void);
void CL_ShutdownUI(void);
int Key_GetCatcher(void);
void Key_SetCatcher(int catcher);

void CL_Netchan_Transmit(netchan_t* chan, msg_t* msg);
qboolean CL_Netchan_Process(netchan_t* chan, msg_t* msg);

qboolean CL_OpenAVIForWriting(const char* filename);
void CL_TakeVideoFrame(void);
void CL_WriteAVIVideoFrame(const byte* imageBuffer, int size);
void CL_WriteAVIAudioFrame(const byte* pcmBuffer, int size);
qboolean CL_CloseAVI(void);
qboolean CL_VideoRecording(void);

void CL_WriteDemoMessage(msg_t* msg, int headerBytes);
