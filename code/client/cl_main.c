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
#include <limits.h>

#include "../qcommon/q_steam.h"
#include "../sys/sys_local.h"
#include "../sys/sys_loadlib.h"

#ifdef USE_RENDERER_DLOPEN
cvar_t* cl_renderer;
#endif

cvar_t* cl_nodelta;
cvar_t* cl_debugMove;

cvar_t* cl_noprint;
#ifdef UPDATE_SERVER_NAME
cvar_t* cl_motd;
#endif

cvar_t* rcon_client_password;
cvar_t* rconAddress;

cvar_t* cl_timeout;
cvar_t* cl_demoRecordMessage;
cvar_t* cl_maxpackets;
cvar_t* cl_packetdup;
cvar_t* cl_timeNudge;
cvar_t* cl_autoTimeNudge;
cvar_t* cl_showTimeDelta;
cvar_t* cl_freezeDemo;

cvar_t* cl_shownet;
cvar_t* cl_showSend;
cvar_t* cl_timedemo;
cvar_t* cl_timedemoLog;
cvar_t* cl_autoRecordDemo;
cvar_t* cl_aviFrameRate;
cvar_t* cl_aviMotionJpeg;
cvar_t* cl_forceavidemo;
static cvar_t* cl_avidemo;
static cvar_t* cl_avidemoLatch;
static cvar_t* cl_avidemoMinTime;
static cvar_t* cl_avidemoMaxTime;

cvar_t* cl_freelook;
cvar_t* cl_sensitivity;

cvar_t* cl_mouseAccel;
cvar_t* cl_mouseAccelOffset;
cvar_t* cl_mouseAccelPower;
cvar_t* cl_mouseAccelDebug;
cvar_t* cl_mouseSensCap;
cvar_t* cg_ignoreMouseInput;

cvar_t* m_pitch;
cvar_t* m_yaw;
cvar_t* m_forward;
cvar_t* m_side;
cvar_t* m_filter;
cvar_t* m_cpi;

cvar_t* j_pitch;
cvar_t* j_yaw;
cvar_t* j_forward;
cvar_t* j_side;
cvar_t* j_up;
cvar_t* j_pitch_axis;
cvar_t* j_yaw_axis;
cvar_t* j_forward_axis;
cvar_t* j_side_axis;
cvar_t* j_up_axis;

cvar_t* cl_activeAction;

cvar_t* cl_motdString;

cvar_t* cl_allowDownload;
cvar_t* cl_conXOffset;
cvar_t* cl_inGameVideo;

cvar_t* cl_serverStatusResendTime;

cvar_t* cl_lanForcePackets;

cvar_t* cl_consoleKeys;

cvar_t* cl_rate;

cvar_t* cl_steamId;

cvar_t* cl_followRedirect;
static int clRedirectHops;
static qboolean clRedirecting;

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
vm_t* cgvm;

char cl_reconnectArgs[MAX_OSPATH];
char cl_oldGame[MAX_QPATH];
qboolean cl_oldGameSet;

refexport_t re;
#ifdef USE_RENDERER_DLOPEN
static void* rendererLib = NULL;
#endif

ping_t cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s {
    char string[BIG_INFO_STRING];
    netadr_t address;
    int time, startTime;
    qboolean pending;
    qboolean print;
    qboolean retrieved;
} serverStatus_t;

serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];

#if defined __USEA3D && defined __A3D_GEOM
void hA3Dg_ExportRenderGeom(refexport_t* incoming_re);
#endif

static int noGameRestart = qfalse;

static qboolean cl_workshopActive;
static qboolean cl_workshopRestart;

static int cl_steamServersTaken;
static qboolean cl_steamServersPending;
static int cl_steamServerSlot[MAX_GLOBAL_SERVERS];

extern void SV_BotFrame(int time);
void CL_CheckForResend(void);
void CL_ShowIP_f(void);
void CL_PostProcessRestart_f(void);
void CL_Userinfo_f(void);
void CL_ServerStatusResponse(netadr_t from, msg_t* msg);

void CL_AddReliableCommand(const char* cmd, qboolean isDisconnectCmd) {
    int unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

    if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) ||
        (!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS)) {
        if (com_errorEntered)
            return;
        else
            Com_Error(ERR_DROP, "Client command overflow");
    }

    Q_strncpyz(clc.reliableCommands[++clc.reliableSequence & (MAX_RELIABLE_COMMANDS - 1)],
               cmd, sizeof(*clc.reliableCommands));
}

void CL_WriteDemoMessage(msg_t* msg, int headerBytes) {
    int len, swlen;

    len = clc.serverMessageSequence;
    swlen = LittleLong(len);
    FS_Write(&swlen, 4, clc.demofile);
    len = msg->cursize - headerBytes;
    swlen = LittleLong(len);
    FS_Write(&swlen, 4, clc.demofile);
    FS_Write(msg->data + headerBytes, len, clc.demofile);
}

void CL_StopRecord_f(void) {
    int len;

    if (!clc.demorecording) {
        Com_Printf("Not recording a demo.\n");
        return;
    }

    len = -1;
    FS_Write(&len, 4, clc.demofile);
    FS_Write(&len, 4, clc.demofile);
    FS_FCloseFile(clc.demofile);
    clc.demofile = 0;
    clc.demorecording = qfalse;
    Cvar_Set("cl_demorecording", "0");
    Cvar_Set("cl_demoname", "");
    Com_Printf("Stopped demo.\n");
}

void CL_DemoFilename(int number, char* fileName, int fileNameSize) {
    int a, b, c, d;

    if (number < 0 || number > 9999)
        number = 9999;

    a = number / 1000;
    number -= a * 1000;
    b = number / 100;
    number -= b * 100;
    c = number / 10;
    number -= c * 10;
    d = number;

    Com_sprintf(fileName, fileNameSize, "demo%i%i%i%i", a, b, c, d);
}

static char demoName[MAX_QPATH];
void CL_Record_f(void) {
    char name[MAX_OSPATH];
    byte bufData[MAX_MSGLEN];
    msg_t buf;
    int i;
    int len;
    entityState_t* ent;
    entityState_t nullstate;
    char* s;

    if (clc.demorecording) {
        Com_Printf("Already recording.\n");
        CL_StopRecord_f();
        if (clc.demorecording) {
            Com_Error(ERR_DROP, "stoprecord failed");
        }
    }

    if (clc.state != CA_ACTIVE) {
        Com_Printf("You must be in a level to record.\n");
        return;
    }

    if (Cmd_Argc() == 2) {
        s = Cmd_Argv(1);
        Q_strncpyz(demoName, s, sizeof(demoName));
        Com_sprintf(name, sizeof(name), "demos/%s.%s%d", demoName, DEMOEXT, com_protocol->integer);
    } else {
        int number;

        if (Cmd_Argc() > 2) {
            Com_Printf("Correct usage: record <demoname>\n");
        }

        for (number = 0; number <= 9999; number++) {
            CL_DemoFilename(number, demoName, sizeof(demoName));
            Com_sprintf(name, sizeof(name), "demos/%s.%s%d", demoName, DEMOEXT, com_protocol->integer);

            if (!FS_FileExists(name))
                break;
        }
    }

    Com_Printf("recording to %s.\n", name);
    clc.demofile = FS_FOpenFileWrite(name);
    if (clc.demofile) {
        CL_MatchMedia(name);
    }
    if (!clc.demofile) {
        Com_Printf("ERROR: couldn't open.\n");
        return;
    }
    clc.demorecording = qtrue;

    Q_strncpyz(clc.demoName, demoName, sizeof(clc.demoName));
    Cvar_Set("cl_demorecording", "1");
    Cvar_Set("cl_demoname", clc.demoName);

    clc.demowaiting = qtrue;

    MSG_Init(&buf, bufData, sizeof(bufData));
    MSG_Bitstream(&buf);

    MSG_WriteLong(&buf, clc.reliableSequence);

    MSG_WriteByte(&buf, svc_gamestate);
    MSG_WriteLong(&buf, clc.serverCommandSequence);

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (!cl.gameState.stringOffsets[i]) {
            continue;
        }
        s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
        MSG_WriteByte(&buf, svc_configstring);
        MSG_WriteShort(&buf, i);
        MSG_WriteBigString(&buf, s);
    }

    Com_Memset(&nullstate, 0, sizeof(nullstate));
    for (i = 0; i < MAX_GENTITIES; i++) {
        ent = &cl.entityBaselines[i];
        if (!ent->number) {
            continue;
        }
        MSG_WriteByte(&buf, svc_baseline);
        MSG_WriteDeltaEntity(&buf, &nullstate, ent, qtrue);
    }

    MSG_WriteByte(&buf, svc_EOF);

    MSG_WriteLong(&buf, clc.clientNum);
    MSG_WriteLong(&buf, clc.checksumFeed);

    MSG_WriteByte(&buf, svc_EOF);

    len = LittleLong(clc.serverMessageSequence - 1);
    FS_Write(&len, 4, clc.demofile);

    len = LittleLong(buf.cursize);
    FS_Write(&len, 4, clc.demofile);
    FS_Write(buf.data, buf.cursize, clc.demofile);

}

static float CL_DemoFrameDurationSDev(void) {
    int i;
    int numFrames;
    float mean = 0.0f;
    float variance = 0.0f;

    if ((clc.timeDemoFrames - 1) > MAX_TIMEDEMO_DURATIONS)
        numFrames = MAX_TIMEDEMO_DURATIONS;
    else
        numFrames = clc.timeDemoFrames - 1;

    for (i = 0; i < numFrames; i++)
        mean += clc.timeDemoDurations[i];
    mean /= numFrames;

    for (i = 0; i < numFrames; i++) {
        float x = clc.timeDemoDurations[i];

        variance += ((x - mean) * (x - mean));
    }
    variance /= numFrames;

    return sqrt(variance);
}

void CL_DemoCompleted(void) {
    char buffer[MAX_STRING_CHARS];

    if (Cvar_VariableIntegerValue("cl_quitOnDemoCompleted")) {
        Com_Quit_f();
        return;
    }

    if (cl_timedemo && cl_timedemo->integer) {
        int time;

        time = Sys_Milliseconds() - clc.timeDemoStart;
        if (time > 0) {
            Com_sprintf(buffer, sizeof(buffer),
                        "%i frames %3.1f seconds %3.1f fps %d.0/%.1f/%d.0/%.1f ms\n",
                        clc.timeDemoFrames,
                        time / 1000.0,
                        clc.timeDemoFrames * 1000.0 / time,
                        clc.timeDemoMinDuration,
                        time / (float)clc.timeDemoFrames,
                        clc.timeDemoMaxDuration,
                        CL_DemoFrameDurationSDev());
            Com_Printf("%s", buffer);

            if (cl_timedemoLog && strlen(cl_timedemoLog->string) > 0) {
                int i;
                int numFrames;
                fileHandle_t f;

                if ((clc.timeDemoFrames - 1) > MAX_TIMEDEMO_DURATIONS)
                    numFrames = MAX_TIMEDEMO_DURATIONS;
                else
                    numFrames = clc.timeDemoFrames - 1;

                f = FS_FOpenFileWrite(cl_timedemoLog->string);
                if (f) {
                    FS_Printf(f, "# %s", buffer);

                    for (i = 0; i < numFrames; i++)
                        FS_Printf(f, "%d\n", clc.timeDemoDurations[i]);

                    FS_FCloseFile(f);
                    Com_Printf("%s written\n", cl_timedemoLog->string);
                } else {
                    Com_Printf("Couldn't open %s for writing\n",
                               cl_timedemoLog->string);
                }
            }
        }
    }

    CL_Disconnect(qtrue);
    CL_NextDemo();
}

void CL_ReadDemoMessage(void) {
    int r;
    msg_t buf;
    byte bufData[MAX_MSGLEN];
    int s;

    if (!clc.demofile) {
        CL_DemoCompleted();
        return;
    }

    r = FS_Read(&s, 4, clc.demofile);
    if (r != 4) {
        CL_DemoCompleted();
        return;
    }
    clc.serverMessageSequence = LittleLong(s);

    MSG_Init(&buf, bufData, sizeof(bufData));

    r = FS_Read(&buf.cursize, 4, clc.demofile);
    if (r != 4) {
        CL_DemoCompleted();
        return;
    }
    buf.cursize = LittleLong(buf.cursize);
    if (buf.cursize == -1) {
        CL_DemoCompleted();
        return;
    }
    if (buf.cursize > buf.maxsize) {
        Com_Error(ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
    }
    r = FS_Read(buf.data, buf.cursize, clc.demofile);
    if (r != buf.cursize) {
        Com_Printf("Demo file was truncated.\n");
        CL_DemoCompleted();
        return;
    }

    clc.lastPacketTime = cls.realtime;
    buf.readcount = 0;
    CL_ParseServerMessage(&buf);
    CL_DemoSnapshotRead();
}

static int CL_WalkDemoExt(char* arg, char* name, int* demofile) {
    int i = 0;
    *demofile = 0;

    {
        Com_sprintf(name, MAX_OSPATH, "demos/%s.%s%d", arg, DEMOEXT, com_protocol->integer);
        FS_FOpenFileRead(name, demofile, qtrue);

        if (*demofile) {
            Com_Printf("Demo file: %s\n", name);
            return com_protocol->integer;
        }
    }

    Com_Printf("Not found: %s\n", name);

    while (demo_protocols[i]) {
        if (demo_protocols[i] == com_protocol->integer)
            continue;

        Com_sprintf(name, MAX_OSPATH, "demos/%s.%s%d", arg, DEMOEXT, demo_protocols[i]);
        FS_FOpenFileRead(name, demofile, qtrue);
        if (*demofile) {
            Com_Printf("Demo file: %s\n", name);

            return demo_protocols[i];
        } else
            Com_Printf("Not found: %s\n", name);
        i++;
    }

    return -1;
}

static void CL_CompleteDemoName(char* args, int argNum) {
    if (argNum == 2) {
        char demoExt[16];

        Com_sprintf(demoExt, sizeof(demoExt), ".%s%d", DEMOEXT, com_protocol->integer);
        Field_CompleteFilename("demos", demoExt, qtrue, qtrue);
    }
}

static int CL_PlayerNames(const char** namesPtr, char (*names)[MAX_NAME_LENGTH], qboolean numbers);

static void CL_CompleteCallvote(char* args, int argNum) {
    static const char* voteNames[] = {
        "ammo", "clientkick", "cointoss", "fraglimit", "g_gametype", "kick", "loadouts",
        "map", "map_restart", "nextmap", "random", "shuffle", "teamsize", "timelimit",
        "timers", "weaprespawn"
    };
    static const char* onOff[] = {"ON", "OFF"};
    static const char* ammo[] = {"GLOBAL", "WEAP"};
    static const char* names[MAX_COMPLETION_NAMES];
    static char players[MAX_CLIENTS][MAX_NAME_LENGTH];
    const char* vote = Cmd_Argv(1);
    int count;

    if (argNum == 2) {
        Field_CompleteList(voteNames, ARRAY_LEN(voteNames));
        return;
    }

    if (argNum == 3) {
        if (!Q_stricmp(vote, "kick")) {
            count = CL_PlayerNames(names, players, qfalse);
            Field_CompletePlayerName(names, count);
            return;
        }
        if (!Q_stricmp(vote, "clientkick")) {
            count = CL_PlayerNames(names, players, qtrue);
            Field_CompleteList(names, count);
            return;
        }
        if (!Q_stricmp(vote, "loadouts") || !Q_stricmp(vote, "timers")) {
            Field_CompleteList(onOff, ARRAY_LEN(onOff));
            return;
        }
        if (!Q_stricmp(vote, "ammo")) {
            Field_CompleteList(ammo, ARRAY_LEN(ammo));
            return;
        }
    }

    if (Q_stricmp(vote, "map")) {
        return;
    }

    if (argNum == 3) {
        count = SV_ArenaNameList(names, ARRAY_LEN(names));
    } else if (argNum == 4) {
        count = SV_FactoryNameList(names, ARRAY_LEN(names));
    } else {
        return;
    }

    Field_CompleteList(names, count);
}

static void CL_CompleteGameCommand(char* args, int argNum) {
    static const char* teamNames[] = {"red", "blue", "spectator", "free", "follow1", "follow2"};
    static const char* sideNames[] = {"red", "blue", "spectator", "free"};
    static const char* lockNames[] = {"red", "blue"};
    static const char* voteNames[] = {"yes", "no"};
    static const char* names[MAX_CLIENTS];
    static char players[MAX_CLIENTS][MAX_NAME_LENGTH];
    const char* cmd = Cmd_Argv(0);
    int count;

    if (!Q_stricmp(cmd, "team")) {
        if (argNum == 2) {
            Field_CompleteList(teamNames, ARRAY_LEN(teamNames));
        }
    } else if (!Q_stricmp(cmd, "vote")) {
        if (argNum == 2) {
            Field_CompleteList(voteNames, ARRAY_LEN(voteNames));
        }
    } else if (!Q_stricmp(cmd, "lock") || !Q_stricmp(cmd, "unlock")) {
        if (argNum == 2) {
            Field_CompleteList(lockNames, ARRAY_LEN(lockNames));
        }
    } else if (!Q_stricmp(cmd, "put")) {
        if (argNum == 2) {
            count = CL_PlayerNames(names, players, qfalse);
            Field_CompletePlayerName(names, count);
        } else if (argNum == 3) {
            Field_CompleteList(sideNames, ARRAY_LEN(sideNames));
        }
    }
}

static void CL_CompleteAddbot(char* args, int argNum) {
    static const char* skills[] = {"1", "2", "3", "4", "5"};
    static const char* teams[] = {"red", "blue"};
    static char botNames[MAX_COMPLETION_NAMES][MAX_NAME_LENGTH];
    static const char* names[MAX_COMPLETION_NAMES];
    static int count = -1;

    if (argNum == 2) {
        if (count < 0) {
            char* buf;
            char* p;
            char* token;

            count = 0;
            if (FS_ReadFile("scripts/bots.txt", (void**)&buf) > 0 && buf) {
                p = buf;
                while ((token = COM_Parse(&p)) != NULL && token[0] && count < MAX_COMPLETION_NAMES) {
                    if (!Q_stricmp(token, "name")) {
                        token = COM_ParseExt(&p, qfalse);
                        if (token[0]) {
                            Q_strncpyz(botNames[count], token, sizeof(botNames[count]));
                            names[count] = botNames[count];
                            count++;
                        }
                    }
                }
                FS_FreeFile(buf);
            }
        }
        Field_CompleteList(names, count);
    } else if (argNum == 3) {
        Field_CompleteList(skills, ARRAY_LEN(skills));
    } else if (argNum == 4) {
        Field_CompleteList(teams, ARRAY_LEN(teams));
    }
}

static void CL_CompletePlayerName(char* args, int argNum);

void CL_SetCgameCommandCompletion(const char* cmdName) {
    static const char* byPlayer[] = {
        "addadmin", "addmod", "addscore", "ban", "demote", "follow", "mute", "tell", "tempban", "unmute"
    };
    static const char* byGame[] = {"lock", "put", "team", "unlock", "vote"};
    int i;

    if (!Q_stricmp(cmdName, "callvote")) {
        Cmd_SetCommandCompletionFunc(cmdName, CL_CompleteCallvote);
        return;
    }
    if (!Q_stricmp(cmdName, "addbot")) {
        Cmd_SetCommandCompletionFunc(cmdName, CL_CompleteAddbot);
        return;
    }
    for (i = 0; i < ARRAY_LEN(byGame); i++) {
        if (!Q_stricmp(cmdName, byGame[i])) {
            Cmd_SetCommandCompletionFunc(cmdName, CL_CompleteGameCommand);
            return;
        }
    }
    for (i = 0; i < ARRAY_LEN(byPlayer); i++) {
        if (!Q_stricmp(cmdName, byPlayer[i])) {
            Cmd_SetCommandCompletionFunc(cmdName, CL_CompletePlayerName);
            return;
        }
    }
}

#define MAX_DEMO_STRETCHES	32

static char		demoPath[MAX_OSPATH];
static int		demoStretchStart[MAX_DEMO_STRETCHES];
static int		demoStretchLength[MAX_DEMO_STRETCHES];
static int		demoStretchAt[MAX_DEMO_STRETCHES];
static int		demoStretches;
static int		demoStretch;
static int		demoLastSnap = -1;
static int		demoSeekTarget = -1;
static int		demoSeekStretch;
static qboolean	demoSeekRestart;

typedef struct {
	qboolean		valid;
	int				messageNum;
	playerState_t	ps;
	int				numEntities;
	int				parseEntitiesNum;
} demoScanFrame_t;

typedef struct {
	entityState_t	baselines[MAX_GENTITIES];
	entityState_t	parseEntities[MAX_PARSE_ENTITIES];
	int				parseEntitiesNum;
	demoScanFrame_t	frames[PACKET_BACKUP];
	int				obituarySeen[MAX_GENTITIES];
} demoScan_t;

static void CL_DemoScanGamestate(msg_t *msg, demoScan_t *scan) {
	entityState_t	nullstate;
	int				cmd, num;

	Com_Memset(&nullstate, 0, sizeof(nullstate));
	Com_Memset(scan->baselines, 0, sizeof(scan->baselines));
	Com_Memset(scan->frames, 0, sizeof(scan->frames));

	MSG_ReadLong(msg);
	while (msg->readcount <= msg->cursize) {
		cmd = MSG_ReadByte(msg);
		if (cmd == svc_configstring) {
			MSG_ReadShort(msg);
			MSG_ReadBigString(msg);
		} else if (cmd == svc_baseline) {
			num = MSG_ReadBits(msg, GENTITYNUM_BITS);
			if (num < 0 || num >= MAX_GENTITIES) {
				return;
			}
			MSG_ReadDeltaEntity(msg, &nullstate, &scan->baselines[num], num);
		} else {
			break;
		}
	}
	MSG_ReadLong(msg);
	MSG_ReadLong(msg);
}

static void CL_DemoScanDelta(msg_t *msg, demoScan_t *scan, demoScanFrame_t *frame, int num,
	entityState_t *from, qboolean unchanged) {
	entityState_t *state = &scan->parseEntities[scan->parseEntitiesNum & (MAX_PARSE_ENTITIES - 1)];

	if (unchanged) {
		*state = *from;
	} else {
		MSG_ReadDeltaEntity(msg, from, state, num);
	}
	if (state->number == MAX_GENTITIES - 1) {
		return;
	}
	scan->parseEntitiesNum++;
	frame->numEntities++;
}

static int CL_DemoScanNextOld(demoScan_t *scan, demoScanFrame_t *old, int *index, entityState_t **state) {
	if (!old || ++*index >= old->numEntities) {
		return 99999;
	}
	*state = &scan->parseEntities[(old->parseEntitiesNum + *index) & (MAX_PARSE_ENTITIES - 1)];
	return (*state)->number;
}

static qboolean CL_DemoScanEntities(msg_t *msg, demoScan_t *scan, demoScanFrame_t *old, demoScanFrame_t *frame) {
	entityState_t	*oldstate = NULL;
	int				oldindex = -1, oldnum, newnum;

	frame->parseEntitiesNum = scan->parseEntitiesNum;
	frame->numEntities = 0;
	oldnum = CL_DemoScanNextOld(scan, old, &oldindex, &oldstate);

	while (1) {
		newnum = MSG_ReadBits(msg, GENTITYNUM_BITS);
		if (newnum == MAX_GENTITIES - 1) {
			break;
		}
		if (msg->readcount > msg->cursize) {
			return qfalse;
		}
		while (oldnum < newnum) {
			CL_DemoScanDelta(msg, scan, frame, oldnum, oldstate, qtrue);
			oldnum = CL_DemoScanNextOld(scan, old, &oldindex, &oldstate);
		}
		if (oldnum == newnum) {
			CL_DemoScanDelta(msg, scan, frame, newnum, oldstate, qfalse);
			oldnum = CL_DemoScanNextOld(scan, old, &oldindex, &oldstate);
		} else {
			CL_DemoScanDelta(msg, scan, frame, newnum, &scan->baselines[newnum], qfalse);
		}
	}
	while (oldnum != 99999) {
		CL_DemoScanDelta(msg, scan, frame, oldnum, oldstate, qtrue);
		oldnum = CL_DemoScanNextOld(scan, old, &oldindex, &oldstate);
	}
	return qtrue;
}

static demoScanFrame_t *CL_DemoScanSnapshot(msg_t *msg, demoScan_t *scan, int seq, int *serverTime) {
	static demoScanFrame_t	frame;
	demoScanFrame_t			*old = NULL;
	byte					areamask[MAX_MAP_AREA_BYTES];
	int						delta, len;

	*serverTime = MSG_ReadLong(msg);
	delta = MSG_ReadByte(msg);
	MSG_ReadByte(msg);

	Com_Memset(&frame, 0, sizeof(frame));
	frame.messageNum = seq;
	frame.valid = qtrue;
	if (delta) {
		old = &scan->frames[(seq - delta) & PACKET_MASK];
		if (!old->valid || old->messageNum != seq - delta
			|| scan->parseEntitiesNum - old->parseEntitiesNum > MAX_PARSE_ENTITIES - MAX_SNAPSHOT_ENTITIES) {
			frame.valid = qfalse;
		}
	}

	len = MSG_ReadByte(msg);
	if (len > (int)sizeof(areamask)) {
		return NULL;
	}
	MSG_ReadData(msg, areamask, len);
	MSG_ReadDeltaPlayerstate(msg, old ? &old->ps : NULL, &frame.ps);
	if (!CL_DemoScanEntities(msg, scan, old, &frame)) {
		return NULL;
	}

	scan->frames[seq & PACKET_MASK] = frame;
	return frame.valid ? &scan->frames[seq & PACKET_MASK] : NULL;
}

static void CL_DemoScan(const char *name) {
	static byte		data[MAX_MSGLEN];
	char			marks[MAX_STRING_CHARS], frags[MAX_STRING_CHARS];
	demoScan_t		*scan;
	demoScanFrame_t	*frame;
	entityState_t	*es;
	fileHandle_t	f;
	msg_t			buf;
	int				seq, len, cmd, t, n, i, last = 0, lastSeq = -1, start = Sys_Milliseconds();
	qboolean		mark = qfalse, gamestate = qfalse, fresh;

	demoStretches = 0;
	marks[0] = '\0';
	frags[0] = '\0';

	scan = Z_Malloc(sizeof(*scan));
	FS_FOpenFileRead(name, &f, qtrue);
	if (f) {
		while (FS_Read(&seq, 4, f) == 4 && FS_Read(&len, 4, f) == 4) {
			seq = LittleLong(seq);
			len = LittleLong(len);
			if (len < 0 || len > MAX_MSGLEN || FS_Read(data, len, f) != len) {
				break;
			}
			MSG_Init(&buf, data, sizeof(data));
			buf.cursize = len;
			MSG_Bitstream(&buf);
			MSG_ReadLong(&buf);

			while (buf.readcount <= buf.cursize) {
				cmd = MSG_ReadByte(&buf);
				if (cmd == svc_nop) {
					continue;
				}
				if (cmd == svc_serverCommand) {
					MSG_ReadLong(&buf);
					if (!Q_stricmpn(MSG_ReadString(&buf), "map_restart", 11)) {
						mark = qtrue;
					}
					continue;
				}
				if (cmd == svc_gamestate) {
					CL_DemoScanGamestate(&buf, scan);
					gamestate = qtrue;
					continue;
				}
				if (cmd != svc_snapshot) {
					break;
				}
				frame = CL_DemoScanSnapshot(&buf, scan, seq, &t);

				if (!demoStretches || gamestate || t < last - 1000) {
					if (demoStretches == MAX_DEMO_STRETCHES) {
						break;
					}
					n = demoStretches++;
					demoStretchStart[n] = t;
					demoStretchLength[n] = 0;
					demoStretchAt[n] = n ? demoStretchAt[n - 1] + demoStretchLength[n - 1] : 0;
					mark = n ? qtrue : qfalse;
				}
				gamestate = qfalse;
				n = demoStretches - 1;
				demoStretchLength[n] = t - demoStretchStart[n];
				if (mark && strlen(marks) < sizeof(marks) - 16) {
					Q_strcat(marks, sizeof(marks), va("%s%d", marks[0] ? " " : "",
						demoStretchAt[n] + demoStretchLength[n]));
				}
				mark = qfalse;
				last = t;

				for (i = 0; frame && i < frame->numEntities; i++) {
					es = &scan->parseEntities[(frame->parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
					if (es->eType != ET_EVENTS + EV_OBITUARY) {
						continue;
					}
					fresh = scan->obituarySeen[es->number] != lastSeq ? qtrue : qfalse;
					scan->obituarySeen[es->number] = seq;
					if (fresh && es->otherEntityNum2 == frame->ps.clientNum
						&& es->otherEntityNum != es->otherEntityNum2 && strlen(frags) < sizeof(frags) - 16) {
						Q_strcat(frags, sizeof(frags), va("%s%d", frags[0] ? " " : "",
							demoStretchAt[n] + demoStretchLength[n]));
					}
				}
				lastSeq = seq;
				break;
			}
		}
		FS_FCloseFile(f);
	}
	Z_Free(scan);

	n = demoStretches - 1;
	Cvar_Set("cl_demoLength", va("%d", demoStretches ? demoStretchAt[n] + demoStretchLength[n] : 0));
	Cvar_Set("cl_demoMarks", marks);
	Cvar_Set("cl_demoFrags", frags);
	Cvar_Set("cl_demoPos", "0");
	Com_DPrintf("demo scan: %d ms\n", Sys_Milliseconds() - start);
}

static int CL_DemoTimeAt(int t) {
	int n = demoStretch < demoStretches ? demoStretch : demoStretches - 1;

	if (n < 0) {
		return 0;
	}
	t -= demoStretchStart[n];
	if (t < 0) {
		t = 0;
	} else if (t > demoStretchLength[n]) {
		t = demoStretchLength[n];
	}
	return demoStretchAt[n] + t;
}

static int CL_DemoPos(void) {
	return CL_DemoTimeAt(cl.serverTime);
}

#define DEMO_SPREAD_BACKUPS		8
#define DEMO_RECENT_BACKUPS		8
#define DEMO_BACKUPS			(DEMO_SPREAD_BACKUPS + DEMO_RECENT_BACKUPS)
#define DEMO_RECENT_SPACING		10000

typedef struct {
	qboolean			valid;
	int					pos;
	int					filePos;
	int					stretch;
	int					lastSnap;
	clientActive_t		cl;
	clientConnection_t	clc;
} demoBackup_t;

static demoBackup_t*	demoBackups;
static int				demoRecentNext;
static int				demoRecentSlot;

static void CL_DemoForgetBackups(void) {
	int i;

	if (demoBackups) {
		for (i = 0; i < DEMO_BACKUPS; i++) {
			demoBackups[i].valid = qfalse;
		}
	}
	demoRecentNext = 0;
	demoRecentSlot = 0;
}

static void CL_DemoFreeBackups(void) {
	free(demoBackups);
	demoBackups = NULL;
	CL_DemoForgetBackups();
}

static void CL_DemoTakeBackup(demoBackup_t* b, int pos) {
	b->valid = qtrue;
	b->pos = pos;
	b->filePos = FS_FTell(clc.demofile);
	b->stretch = demoStretch;
	b->lastSnap = demoLastSnap;
	memcpy(&b->cl, &cl, sizeof(cl));
	memcpy(&b->clc, &clc, sizeof(clc));
}

static void CL_DemoBackup(void) {
	int pos, length, slot;

	if (clc.state != CA_ACTIVE || !demoStretches || !clc.demofile || cl_timedemo->integer) {
		return;
	}
	if (!demoBackups) {
		demoBackups = calloc(DEMO_BACKUPS, sizeof(demoBackup_t));
		if (!demoBackups) {
			return;
		}
	}

	pos = CL_DemoTimeAt(cl.snap.serverTime);
	length = demoStretchAt[demoStretches - 1] + demoStretchLength[demoStretches - 1];
	slot = length > 0 ? (int)((long long)pos * DEMO_SPREAD_BACKUPS / length) : 0;
	if (slot >= DEMO_SPREAD_BACKUPS) {
		slot = DEMO_SPREAD_BACKUPS - 1;
	}
	if (!demoBackups[slot].valid) {
		CL_DemoTakeBackup(&demoBackups[slot], pos);
	}

	if (pos >= demoRecentNext) {
		CL_DemoTakeBackup(&demoBackups[DEMO_SPREAD_BACKUPS + demoRecentSlot], pos);
		demoRecentSlot = (demoRecentSlot + 1) % DEMO_RECENT_BACKUPS;
		demoRecentNext = pos + DEMO_RECENT_SPACING;
	}
}

void CL_DemoGamestate(void) {
	if (clc.demoplaying && clc.state == CA_ACTIVE) {
		demoStretch++;
		demoLastSnap = -1;
		CL_DemoForgetBackups();
	}
}

void CL_DemoSnapshotRead(void) {
	if (!cl.snap.valid) {
		return;
	}
	if (demoLastSnap >= 0 && cl.snap.serverTime < demoLastSnap - 1000 && demoStretch + 1 < demoStretches) {
		demoStretch++;
	}
	demoLastSnap = cl.snap.serverTime;
	CL_DemoBackup();
}

int CL_DemoSuspend(void) {
	int pos;

	if (!clc.demoplaying || !clc.demofile) {
		return -1;
	}
	pos = FS_FTell(clc.demofile);
	FS_FCloseFile(clc.demofile);
	clc.demofile = 0;
	return pos;
}

void CL_DemoResume(int pos) {
	if (pos < 0) {
		return;
	}
	FS_FOpenFileRead(demoPath, &clc.demofile, qtrue);
	if (clc.demofile) {
		FS_Seek(clc.demofile, pos, FS_SEEK_SET);
	}
}

static qboolean CL_DemoRewind(int pos) {
	demoBackup_t* b = NULL;
	fileHandle_t demofile;
	int i, lead;

	if (!demoBackups || !cgvm) {
		return qfalse;
	}
	for (lead = 1000; lead >= 0 && !b; lead -= 1000) {
		for (i = 0; i < DEMO_BACKUPS; i++) {
			if (demoBackups[i].valid && demoBackups[i].pos <= pos - lead && (!b || demoBackups[i].pos > b->pos)) {
				b = &demoBackups[i];
			}
		}
	}
	if (!b) {
		return qfalse;
	}

	S_StopAllSounds();

	demofile = clc.demofile;
	memcpy(&cl, &b->cl, sizeof(cl));
	memcpy(&clc, &b->clc, sizeof(clc));
	clc.demofile = demofile;
	FS_Seek(clc.demofile, b->filePos, FS_SEEK_SET);
	demoStretch = b->stretch;
	demoLastSnap = b->lastSnap;
	demoRecentNext = b->pos + DEMO_RECENT_SPACING;

	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.serverTime = cl.oldServerTime = cl.oldFrameServerTime = cl.snap.serverTime;

	VM_Call(cgvm, CG_DEMO_REWIND, cl.snap.messageNum, clc.lastExecutedServerCommand);
	return qtrue;
}

static void CL_DemoSeek(int pos) {
	int n;

	if (!clc.demoplaying || !demoStretches) {
		Com_Printf("no demo is playing\n");
		return;
	}
	n = demoStretches - 1;
	if (pos < 0) {
		pos = 0;
	} else if (pos > demoStretchAt[n] + demoStretchLength[n]) {
		pos = demoStretchAt[n] + demoStretchLength[n];
	}
	while (n > 0 && demoStretchAt[n] > pos) {
		n--;
	}

	demoSeekStretch = n;
	demoSeekTarget = demoStretchStart[n] + pos - demoStretchAt[n];
	if (clc.state != CA_ACTIVE) {
		return;
	}
	if (n != demoStretch || demoSeekTarget < cl.serverTime) {
		if (pos < CL_DemoPos() && CL_DemoRewind(pos)) {
			return;
		}
		demoSeekRestart = qtrue;
		Cbuf_ExecuteText(EXEC_INSERT, va("demo \"%s\"\n", clc.demoName));
	}
}

void CL_DemoSeekApply(void) {
	int target, ahead;

	if (!clc.demoplaying || clc.state != CA_ACTIVE || cl_timedemo->integer) {
		return;
	}
	if (demoSeekTarget >= 0) {
		if (demoStretch < demoSeekStretch && demoStretch < demoStretches) {
			target = demoStretchStart[demoStretch] + demoStretchLength[demoStretch] + 1000;
		} else {
			target = demoStretch == demoSeekStretch ? demoSeekTarget : cl.serverTime;
			demoSeekTarget = -1;
		}
		ahead = target - cl.serverTime;
		if (ahead > 0) {
			cl.serverTimeDelta += ahead;
			cl.serverTime += ahead;
			cl.oldServerTime = cl.serverTime;
		}
	}
	Cvar_Set("cl_demoPos", va("%d", CL_DemoPos()));
}

static float CL_SeekSeconds(float fallback) {
	return Cmd_Argc() > 1 ? atof(Cmd_Argv(1)) : fallback;
}

static void CL_Rewind_f(void) {
	CL_DemoSeek(CL_DemoPos() - (int)(CL_SeekSeconds(10) * 1000));
}

static void CL_FastForward_f(void) {
	CL_DemoSeek(CL_DemoPos() + (int)(CL_SeekSeconds(10) * 1000));
}

static void CL_FragForward_f(void) {
	char		frags[MAX_STRING_CHARS];
	const char	*p;
	int			before = (int)(CL_SeekSeconds(3) * 1000);
	int			now = CL_DemoPos();
	int			t;

	Cvar_VariableStringBuffer("cl_demoFrags", frags, sizeof(frags));
	for (p = frags; *p; ) {
		t = atoi(p);
		if (t - before > now + 500) {
			CL_DemoSeek(t - before);
			return;
		}
		while (*p && *p != ' ') {
			p++;
		}
		while (*p == ' ') {
			p++;
		}
	}
	Com_Printf("no more frags\n");
}

static void CL_SeekDemo_f(void) {
	const char *s = Cmd_Argv(1);
	const char *colon = strchr(s, ':');
	float seconds;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: seekdemo <seconds> or <m:ss>\n");
		return;
	}
	seconds = colon ? atoi(s) * 60 + atof(colon + 1) : atof(s);
	CL_DemoSeek((int)(seconds * 1000));
}

void CL_PlayDemo_f(void) {
    char name[MAX_OSPATH];
    char arg[MAX_OSPATH];
    char* ext_test;
    int protocol, i;
    char retry[MAX_OSPATH];
    qboolean seekRestart;

    if (Cmd_Argc() != 2) {
        Com_Printf("demo <demoname>\n");
        return;
    }

    Cvar_Set("sv_killserver", "2");

    Q_strncpyz(arg, Cmd_Argv(1), sizeof(arg));

    CL_Disconnect(qtrue);

    ext_test = strrchr(arg, '.');

    if (ext_test && !Q_stricmpn(ext_test + 1, DEMOEXT, ARRAY_LEN(DEMOEXT) - 1)) {
        protocol = atoi(ext_test + ARRAY_LEN(DEMOEXT));

        for (i = 0; demo_protocols[i]; i++) {
            if (demo_protocols[i] == protocol)
                break;
        }

        if (demo_protocols[i] || protocol == com_protocol->integer) {
            Com_sprintf(name, sizeof(name), "demos/%s", arg);
            FS_FOpenFileRead(name, &clc.demofile, qtrue);
        } else {
            int len;

            Com_Printf("Protocol %d not supported for demos\n", protocol);
            len = ext_test - arg;

            if (len >= ARRAY_LEN(retry))
                len = ARRAY_LEN(retry) - 1;

            Q_strncpyz(retry, arg, len + 1);
            retry[len] = '\0';
            protocol = CL_WalkDemoExt(retry, name, &clc.demofile);
        }
    } else
        protocol = CL_WalkDemoExt(arg, name, &clc.demofile);

    if (!clc.demofile) {
        Com_Error(ERR_DROP, "couldn't open %s", name);
        return;
    }
    Q_strncpyz(clc.demoName, arg, sizeof(clc.demoName));

    seekRestart = demoSeekRestart;
    if (!demoSeekRestart) {
        demoSeekTarget = -1;
    }
    demoSeekRestart = qfalse;
    demoStretch = 0;
    demoLastSnap = -1;
    CL_DemoForgetBackups();
    if (!seekRestart || Q_stricmp(demoPath, name)) {
        Q_strncpyz(demoPath, name, sizeof(demoPath));
        CL_DemoScan(name);
    }

    Con_Close();

    clc.state = CA_CONNECTED;
    clc.demoplaying = qtrue;
    Q_strncpyz(clc.servername, arg, sizeof(clc.servername));

    while (clc.state >= CA_CONNECTED && clc.state < CA_PRIMED) {
        CL_ReadDemoMessage();
    }
    clc.firstDemoFrameSkipped = qfalse;
}

void CL_StartDemoLoop(void) {
    Cbuf_AddText("d1\n");
    Key_SetCatcher(0);
}

void CL_NextDemo(void) {
    char v[MAX_STRING_CHARS];

    Q_strncpyz(v, Cvar_VariableString("nextdemo"), sizeof(v));
    v[MAX_STRING_CHARS - 1] = 0;
    Com_DPrintf("CL_NextDemo: %s\n", v);
    if (!v[0]) {
        return;
    }

    Cvar_Set("nextdemo", "");
    Cbuf_AddText(v);
    Cbuf_AddText("\n");
    Cbuf_Execute();
}

void CL_ShutdownAll(qboolean shutdownRef) {
    if (CL_VideoRecording())
        CL_CloseAVI();

    if (clc.demorecording)
        CL_StopRecord_f();

#ifdef USE_CURL
    CL_cURL_Shutdown();
#endif
    S_DisableSounds();
    CL_ShutdownCGame();
    CL_ShutdownUI();

    if (shutdownRef)
        CL_ShutdownRef();
    else if (re.Shutdown)
        re.Shutdown(qfalse);

    cls.uiStarted = qfalse;
    cls.cgameStarted = qfalse;
    cls.rendererStarted = qfalse;
    cls.soundRegistered = qfalse;
}

void CL_ClearMemory(qboolean shutdownRef) {
    CL_ShutdownAll(shutdownRef);

    if (!com_sv_running || !com_sv_running->integer) {
        Hunk_Clear();
        CM_ClearMap();
    } else {
        Hunk_ClearToMark();
    }
}

void CL_FlushMemory(void) {
    CL_ClearMemory(qfalse);
    CL_StartHunkUsers(qfalse);
}

void CL_MapLoading(void) {
    if (com_dedicated->integer) {
        clc.state = CA_DISCONNECTED;
        Key_SetCatcher(KEYCATCH_CONSOLE);
        return;
    }

    if (!com_cl_running->integer) {
        return;
    }

    Con_Close();
    Key_SetCatcher(0);

    if (clc.state >= CA_CONNECTED && !Q_stricmp(clc.servername, "localhost")) {
        clc.state = CA_CONNECTED;
        Com_Memset(cls.updateInfoString, 0, sizeof(cls.updateInfoString));
        Com_Memset(clc.serverMessage, 0, sizeof(clc.serverMessage));
        Com_Memset(&cl.gameState, 0, sizeof(cl.gameState));
        clc.lastPacketSentTime = -9999;
        SCR_UpdateScreen();
    } else {
        Cvar_Set("nextmap", "");
        CL_Disconnect(qtrue);
        Q_strncpyz(clc.servername, "localhost", sizeof(clc.servername));
        clc.state = CA_CHALLENGING;
        Key_SetCatcher(0);
        SCR_UpdateScreen();
        clc.connectTime = -RETRANSMIT_TIMEOUT;
        NET_StringToAdr(clc.servername, &clc.serverAddress, NA_UNSPEC);

        CL_CheckForResend();
    }
}

void CL_ClearState(void) {

    Com_Memset(&cl, 0, sizeof(cl));
}

static void CL_OldGame(void) {
    if (cl_oldGameSet) {
        cl_oldGameSet = qfalse;
        Cvar_Set2("fs_game", cl_oldGame, qtrue);
        FS_ConditionalRestart(clc.checksumFeed, qfalse);
    }
}

void CL_Disconnect(qboolean showMainMenu) {
    if (!com_cl_running || !com_cl_running->integer) {
        return;
    }

    Cvar_Set("r_uiFullScreen", "1");

    if (clc.demorecording) {
        CL_StopRecord_f();
    }

    if (clc.download) {
        FS_FCloseFile(clc.download);
        clc.download = 0;
    }
    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");

    if (clc.demofile) {
        FS_FCloseFile(clc.demofile);
        clc.demofile = 0;
    }
    CL_DemoFreeBackups();

    if (uivm && showMainMenu) {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE);
    }

    SCR_StopCinematic();
    S_ClearSoundBuffer();

    if (clc.state >= CA_CONNECTED) {
        CL_AddReliableCommand("disconnect", qtrue);
        CL_WritePacket();
        CL_WritePacket();
        CL_WritePacket();
    }

    FS_PureServerSetLoadedPaks("", "");
    FS_PureServerSetReferencedPaks("", "");

    if (com_sv_running && com_sv_running->integer) {
        SV_SteamStatsRemoveUser(Steam_ClientSteamID());
    }

    CL_ClearState();

    Com_Memset(&clc, 0, sizeof(clc));

    clc.state = CA_DISCONNECTED;

    Cvar_Set("sv_cheats", "1");

    cl_connectedToPureServer = qfalse;

    if (CL_VideoRecording()) {
        SCR_UpdateScreen();
        CL_CloseAVI();
    }

    if (!noGameRestart)
        CL_OldGame();
    else
        noGameRestart = qfalse;
}

void CL_ForwardCommandToServer(const char* string) {
    char* cmd;

    cmd = Cmd_Argv(0);

    if (cmd[0] == '-') {
        return;
    }

    if (clc.demoplaying || clc.state < CA_CONNECTED || cmd[0] == '+') {
        Com_Printf("Unknown command \"%s" S_COLOR_WHITE "\"\n", cmd);
        return;
    }

    if (Cmd_Argc() > 1) {
        CL_AddReliableCommand(string, qfalse);
    } else {
        CL_AddReliableCommand(cmd, qfalse);
    }
}

void CL_RequestMotd(void) {
#ifdef UPDATE_SERVER_NAME
    char info[MAX_INFO_STRING];

    if (!cl_motd->integer) {
        return;
    }
    Com_Printf("Resolving %s\n", UPDATE_SERVER_NAME);
    if (!NET_StringToAdr(UPDATE_SERVER_NAME, &cls.updateServer, NA_IP)) {
        Com_Printf("Couldn't resolve address\n");
        return;
    }
    cls.updateServer.port = BigShort(PORT_UPDATE);
    Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", UPDATE_SERVER_NAME,
               cls.updateServer.ip[0], cls.updateServer.ip[1],
               cls.updateServer.ip[2], cls.updateServer.ip[3],
               BigShort(cls.updateServer.port));

    info[0] = 0;

    Com_sprintf(cls.updateChallenge, sizeof(cls.updateChallenge), "%i", (int)((((unsigned int)rand() << 16) ^ (unsigned int)rand()) ^ Com_Milliseconds()));

    Info_SetValueForKey(info, "challenge", cls.updateChallenge);
    Info_SetValueForKey(info, "renderer", cls.glconfig.renderer_string);
    Info_SetValueForKey(info, "version", com_version->string);

    NET_OutOfBandPrint(NS_CLIENT, cls.updateServer, "getmotd \"%s\"\n", info);
#endif
}

void CL_ForwardToServer_f(void) {
    if (clc.state != CA_ACTIVE || clc.demoplaying) {
        Com_Printf("Not connected to a server.\n");
        return;
    }

    if (Cmd_Argc() > 1) {
        CL_AddReliableCommand(Cmd_Args(), qfalse);
    }
}

void CL_Disconnect_f(void) {
    SCR_StopCinematic();
    if (clc.state != CA_DISCONNECTED && clc.state != CA_CINEMATIC) {
        Com_Error(ERR_DISCONNECT, "Disconnected from server");
    }
}

void CL_Reconnect_f(void) {
    if (!strlen(cl_reconnectArgs))
        return;
    Cbuf_AddText(va("connect %s\n", cl_reconnectArgs));
}

void CL_Connect_f(void) {
    char server[MAX_OSPATH];
    const char* serverString;
    int argc = Cmd_Argc();
    netadrtype_t family = NA_UNSPEC;

    if (argc != 2 && argc != 3) {
        Com_Printf("usage: connect [-4|-6] server\n");
        return;
    }

    if (argc == 2)
        Q_strncpyz(server, Cmd_Argv(1), sizeof(server));
    else {
        if (!strcmp(Cmd_Argv(1), "-4"))
            family = NA_IP;
        else if (!strcmp(Cmd_Argv(1), "-6"))
            family = NA_IP6;
        else
            Com_Printf("warning: only -4 or -6 as address type understood.\n");

        Q_strncpyz(server, Cmd_Argv(2), sizeof(server));
    }

    if (clRedirecting) {
        clRedirecting = qfalse;
    } else {
        clRedirectHops = 0;
    }

    Q_strncpyz(cl_reconnectArgs, Cmd_Args(), sizeof(cl_reconnectArgs));

    CL_RequestMotd();

    clc.serverMessage[0] = 0;

    if (com_sv_running->integer && !strcmp(server, "localhost")) {
        SV_Shutdown("Server quit");
    }

    Cvar_Set("sv_killserver", "1");
    SV_Frame(0);

    noGameRestart = qtrue;
    CL_Disconnect(qtrue);
    Con_Close();

    Q_strncpyz(clc.servername, server, sizeof(clc.servername));

    if (!NET_StringToAdr(clc.servername, &clc.serverAddress, family)) {
        Com_Printf("Bad server address\n");
        clc.state = CA_DISCONNECTED;
        return;
    }
    if (clc.serverAddress.port == 0) {
        clc.serverAddress.port = BigShort(PORT_SERVER);
    }

    serverString = NET_AdrToStringwPort(clc.serverAddress);

    Com_Printf("%s resolved to %s\n", clc.servername, serverString);

    if (NET_IsLocalAddress(clc.serverAddress))
        clc.state = CA_CHALLENGING;
    else {
        clc.state = CA_CONNECTING;

        clc.challenge = (((unsigned int)rand() << 16) ^ (unsigned int)rand()) ^ Com_Milliseconds();
    }

    Key_SetCatcher(0);
    clc.connectTime = -99999;
    clc.connectPacketCount = 0;

    Cvar_Set("cl_currentServerAddress", server);
}

#define MAX_RCON_MESSAGE 1024

static void CL_CompleteRcon(char* args, int argNum) {
    if (argNum == 2) {
        char* p = Com_SkipTokens(args, 1, " ");

        if (p > args)
            Field_CompleteCommand(p, qtrue, qtrue);
    }
}

static int CL_PlayerNames(const char** namesPtr, char (*names)[MAX_NAME_LENGTH], qboolean numbers) {
    const char* info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
    const int clientCount = MIN(atoi(Info_ValueForKey(info, "sv_maxclients")), MAX_CLIENTS);
    int i, nameCount = 0;

    for (i = 0; i < clientCount; i++) {
        const char* name;

        if (i == clc.clientNum) {
            continue;
        }
        info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + i];
        name = Info_ValueForKey(info, "n");
        if (name[0] == '\0') {
            continue;
        }
        if (numbers) {
            Com_sprintf(names[nameCount], MAX_NAME_LENGTH, "%i", i);
        } else {
            Q_strncpyz(names[nameCount], name, MAX_NAME_LENGTH);
            Q_CleanStr(names[nameCount]);
        }
        namesPtr[nameCount] = names[nameCount];
        nameCount++;
    }
    if (!numbers) {
        qsort((void*)namesPtr, nameCount, sizeof(namesPtr[0]), Com_strCompare);
    }
    return nameCount;
}

static void CL_CompletePlayerName(char* args, int argNum) {
    if (argNum == 2) {
        static char names[MAX_CLIENTS][MAX_NAME_LENGTH];
        static const char* namesPtr[MAX_CLIENTS];
        const int nameCount = CL_PlayerNames(namesPtr, names, qfalse);

        Field_CompletePlayerName(namesPtr, nameCount);
    }
}

void CL_Rcon_f(void) {
    char message[MAX_RCON_MESSAGE];
    netadr_t to;

    if (!rcon_client_password->string[0]) {
        if (clc.state >= CA_CONNECTED) {
            CL_AddReliableCommand(Cmd_Cmd(), qfalse);
            return;
        }
        Com_Printf(
            "You must set 'rconpassword' before\n"
            "issuing an rcon command.\n");
        return;
    }

    message[0] = -1;
    message[1] = -1;
    message[2] = -1;
    message[3] = -1;
    message[4] = 0;

    Q_strcat(message, MAX_RCON_MESSAGE, "rcon ");

    Q_strcat(message, MAX_RCON_MESSAGE, rcon_client_password->string);
    Q_strcat(message, MAX_RCON_MESSAGE, " ");

    Q_strcat(message, MAX_RCON_MESSAGE, Cmd_Cmd() + 5);

    if (clc.state >= CA_CONNECTED) {
        to = clc.netchan.remoteAddress;
    } else {
        if (!strlen(rconAddress->string)) {
            Com_Printf(
                "You must either be connected,\n"
                "or set the 'rconAddress' cvar\n"
                "to issue rcon commands\n");

            return;
        }
        NET_StringToAdr(rconAddress->string, &to, NA_UNSPEC);
        if (to.port == 0) {
            to.port = BigShort(PORT_SERVER);
        }
    }

    NET_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
    cls.rconAddress = to;
}

void CL_SendPureChecksums(void) {
    char cMsg[MAX_INFO_VALUE];

    Com_sprintf(cMsg, sizeof(cMsg), "cp %d %s", cl.serverId, FS_ReferencedPakPureChecksums());

    CL_AddReliableCommand(cMsg, qfalse);
}

void CL_ResetPureClientAtServer(void) {
    CL_AddReliableCommand("vdr", qfalse);
}

void CL_ResolutionChanged(void) {
    Cvar_SetValue("vid_resizeCount", Cvar_VariableIntegerValue("vid_resizeCount") + 1);
    if (cls.cgameStarted && cgvm) {
        VM_Call(cgvm, CG_EVENT_HANDLING, CGAME_EVENT_RESOLUTION);
    }
}

void CL_Vid_Restart_f(void) {
    if (Cmd_Argc() > 1 && !Q_stricmp(Cmd_Argv(1), "fast") && cls.rendererStarted &&
        !Cvar_VariableIntegerValue("r_noFastRestart") && re.ApplyVideoMode &&
        re.ApplyVideoMode(&cls.glconfig)) {
        CL_ResolutionChanged();
        IN_Restart();
        return;
    }

    if (CL_VideoRecording()) {
        CL_CloseAVI();
    }

    if (clc.demorecording)
        CL_StopRecord_f();

    S_StopAllSounds();

    if (!FS_ConditionalRestart(clc.checksumFeed, qtrue)) {
        if (com_sv_running->integer) {
            Hunk_ClearToMark();
        } else {
            Hunk_Clear();
        }

        CL_ShutdownUI();
        CL_ShutdownCGame();
        CL_ShutdownRef();
        CL_ResetPureClientAtServer();
        FS_ClearPakReferences(FS_UI_REF | FS_CGAME_REF);

        cls.rendererStarted = qfalse;
        cls.uiStarted = qfalse;
        cls.cgameStarted = qfalse;
        cls.soundRegistered = qfalse;

        Cvar_Set("cl_paused", "0");

        CL_InitRef();

        CL_StartHunkUsers(qfalse);

        if (clc.state > CA_CONNECTED && clc.state != CA_CINEMATIC) {
            cls.cgameStarted = qtrue;
            CL_InitCGame();
            CL_SendPureChecksums();
        }
    }
}

void CL_Snd_Shutdown(void) {
    S_Shutdown();
    cls.soundStarted = qfalse;
}

void CL_Snd_Restart_f(void) {
    CL_Snd_Shutdown();
    CL_Vid_Restart_f();
}

void CL_OpenedPK3List_f(void) {
    Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames());
}

void CL_ReferencedPK3List_f(void) {
    Com_Printf("Referenced PK3 Names: %s\n", FS_ReferencedPakNames());
}

void CL_Configstrings_f(void) {
    int i;
    int ofs;

    if (clc.state != CA_ACTIVE) {
        Com_Printf("Not connected to a server.\n");
        return;
    }

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        ofs = cl.gameState.stringOffsets[i];
        if (!ofs) {
            continue;
        }
        Com_Printf("%4i: %s\n", i, cl.gameState.stringData + ofs);
    }
}

void CL_Clientinfo_f(void) {
    Com_Printf("--------- Client Information ---------\n");
    Com_Printf("state: %i\n", clc.state);
    Com_Printf("Server: %s\n", clc.servername);
    Com_Printf("User info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_USERINFO));
    Com_Printf("--------------------------------------\n");
}

void CL_PostProcessRestart_f(void) {
}

void CL_Userinfo_f(void) {
    Com_Printf("User info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_USERINFO));
}

void CL_DownloadsComplete(void) {
#ifdef USE_CURL
    if (clc.cURLUsed) {
        clc.cURLUsed = qfalse;
        CL_cURL_Shutdown();
        if (clc.cURLDisconnected) {
            if (clc.downloadRestart) {
                FS_Restart(clc.checksumFeed);
                clc.downloadRestart = qfalse;
            }
            clc.cURLDisconnected = qfalse;
            CL_Reconnect_f();
            return;
        }
    }
#endif

    if (clc.downloadRestart) {
        clc.downloadRestart = qfalse;

        FS_Restart(clc.checksumFeed);

        CL_AddReliableCommand("donedl", qfalse);

        return;
    }

    clc.state = CA_LOADING;

    Com_EventLoop();

    if (clc.state != CA_LOADING) {
        return;
    }

    Cvar_Set("r_uiFullScreen", "0");

    CL_FlushMemory();

    cls.cgameStarted = qtrue;
    CL_InitCGame();

    CL_SendPureChecksums();

    CL_WritePacket();
    CL_WritePacket();
    CL_WritePacket();
}

void CL_BeginDownload(const char* localName, const char* remoteName) {
    Com_DPrintf(
        "***** CL_BeginDownload *****\n"
        "Localname: %s\n"
        "Remotename: %s\n"
        "****************************\n",
        localName, remoteName);

    Q_strncpyz(clc.downloadName, localName, sizeof(clc.downloadName));
    Com_sprintf(clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName);

    Cvar_Set("cl_downloadName", remoteName);
    Cvar_Set("cl_downloadSize", "0");
    Cvar_Set("cl_downloadCount", "0");
    Cvar_SetValue("cl_downloadTime", cls.realtime);

    clc.downloadBlock = 0;
    clc.downloadCount = 0;

    CL_AddReliableCommand(va("download %s", remoteName), qfalse);
}

void CL_NextDownload(void) {
    char* s;
    char *remoteName, *localName;
    qboolean useCURL = qfalse;

    if (*clc.downloadName) {
        char* zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clc.downloadName, "");
        zippath[strlen(zippath) - 1] = '\0';

        if (!FS_CompareZipChecksum(zippath))
            Com_Error(ERR_DROP, "Incorrect checksum for file: %s", clc.downloadName);
    }

    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");

    if (*clc.downloadList) {
        s = clc.downloadList;

        if (*s == '@')
            s++;
        remoteName = s;

        if ((s = strchr(s, '@')) == NULL) {
            CL_DownloadsComplete();
            return;
        }

        *s++ = 0;
        localName = s;
        if ((s = strchr(s, '@')) != NULL)
            *s++ = 0;
        else
            s = localName + strlen(localName);
#ifdef USE_CURL
        if (!(cl_allowDownload->integer & DLF_NO_REDIRECT)) {
            if (clc.sv_allowDownload & DLF_NO_REDIRECT) {
                Com_Printf(
                    "WARNING: server does not "
                    "allow download redirection "
                    "(sv_allowDownload is %d)\n",
                    clc.sv_allowDownload);
            } else if (!*clc.sv_dlURL) {
                Com_Printf(
                    "WARNING: server allows "
                    "download redirection, but does not "
                    "have sv_dlURL set\n");
            } else if (!CL_cURL_Init()) {
                Com_Printf(
                    "WARNING: could not load "
                    "cURL library\n");
            } else {
                CL_cURL_BeginDownload(localName, va("%s/%s",
                                                    clc.sv_dlURL, remoteName));
                useCURL = qtrue;
            }
        } else if (!(clc.sv_allowDownload & DLF_NO_REDIRECT)) {
            Com_Printf(
                "WARNING: server allows download "
                "redirection, but it disabled by client "
                "configuration (cl_allowDownload is %d)\n",
                cl_allowDownload->integer);
        }
#endif
        if (!useCURL) {
            if ((cl_allowDownload->integer & DLF_NO_UDP)) {
                Com_Error(ERR_DROP,
                          "UDP Downloads are "
                          "disabled on your client. "
                          "(cl_allowDownload is %d)",
                          cl_allowDownload->integer);
                return;
            } else {
                CL_BeginDownload(localName, remoteName);
            }
        }
        clc.downloadRestart = qtrue;

        memmove(clc.downloadList, s, strlen(s) + 1);

        return;
    }

    CL_DownloadsComplete();
}

qboolean CL_Connected(void) {
    return (qboolean)(clc.state >= CA_CONNECTED);
}

qboolean CL_InitDownloads(void) {
    char copy[BIG_INFO_STRING];
    const char* list;
    char* token;
    unsigned long long item;
    int total = 0;
    int index = 0;

    list = cl.gameState.stringData + cl.gameState.stringOffsets[CS_STEAM_WORKSHOP_IDS];
    if (clc.demoplaying || !list[0]) {
        return qfalse;
    }
    Com_Printf("Server requires the following workshop items: %s\n", list);
    Q_strncpyz(copy, list, sizeof(copy));
    for (token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
        total++;
    }
    Q_strncpyz(copy, list, sizeof(copy));
    for (token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
        item = 0;
        sscanf(token, "%llu", &item);
        if (Steam_WorkshopQueue((uint64_t)item, qtrue)) {
            cl_workshopRestart = qtrue;
            index++;
            Cvar_Set("cl_downloadItem", token);
            Cvar_Set("cl_downloadName", va("Workshop item %i of %i", index, total));
            Cvar_SetValue("cl_downloadTime", cls.realtime);
        }
    }
    clc.state = CA_CONNECTED;
    cl_workshopActive = qtrue;
    return qtrue;
}

void CL_WorkshopFrame(void) {
    char missing[2048];

    if (!cl_workshopActive || !Steam_WorkshopDone()) {
        return;
    }
    if (cl_workshopRestart) {
        Com_Printf("Steamworks downloads complete - FS restart is required\n");
        FS_Restart(clc.checksumFeed);
        cl_workshopRestart = qfalse;
        return;
    }
    Com_DPrintf("Steamworks downloads complete\n");
    if (FS_ComparePaks(missing, sizeof(missing), qfalse)) {
        Com_Printf("WARNING: Missing pk3s referenced by the server:\n%s\nThe server will most likely refuse the "
                   "connection.\n",
                   missing);
    }
    cl_workshopActive = qfalse;
    if (clc.state == CA_CONNECTED) {
        CL_DownloadsComplete();
    }
}

static qboolean cl_warmPending;
static char cl_warmedModels[64][MAX_QPATH];
static int cl_numWarmedModels;

void CL_WarmPlayersChanged(void) {
    cl_warmPending = qtrue;
}

static void CL_WarmModel(const char* model) {
    char name[MAX_QPATH];
    char* slash;
    int i;

    Q_strncpyz(name, model, sizeof(name));
    slash = strchr(name, '/');
    if (slash) {
        *slash = '\0';
    }
    if (!name[0] || strchr(name, '.') || strchr(name, '\\')) {
        return;
    }
    Q_strlwr(name);
    for (i = 0; i < cl_numWarmedModels; i++) {
        if (!strcmp(cl_warmedModels[i], name)) {
            return;
        }
    }
    if (cl_numWarmedModels == ARRAY_LEN(cl_warmedModels)) {
        return;
    }
    Q_strncpyz(cl_warmedModels[cl_numWarmedModels++], name, MAX_QPATH);
    FS_WarmPrefix(va("models/players/%s/", name));
    FS_WarmPrefix(va("sound/player/%s/", name));
}

static void CL_WarmPlayers(void) {
    const char* info;
    int i;

    if (!cl_warmPending || clc.state != CA_ACTIVE) {
        return;
    }
    cl_warmPending = qfalse;
    CL_WarmModel(Cvar_VariableString("cg_forceEnemyModel"));
    CL_WarmModel(Cvar_VariableString("cg_forceTeamModel"));
    for (i = 0; i < MAX_CLIENTS; i++) {
        info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + i];
        if (!info[0]) {
            continue;
        }
        CL_WarmModel(Info_ValueForKey(info, "model"));
        CL_WarmModel(Info_ValueForKey(info, "hmodel"));
    }
}

void CL_CheckForResend(void) {
    int port;
    char info[MAX_INFO_STRING];
    char data[MAX_INFO_STRING + 10];

    if (clc.demoplaying) {
        return;
    }

    if (clc.state != CA_CONNECTING && clc.state != CA_CHALLENGING) {
        return;
    }

    if (cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT) {
        return;
    }

    clc.connectTime = cls.realtime;
    clc.connectPacketCount++;

    switch (clc.state) {
        case CA_CONNECTING:
        {
            byte payload[13 + 8 + 504];
            unsigned long long steamId = Steam_ClientSteamID();
            int ticketLen;

            if (!steamId) {
#ifdef _MSC_VER
                steamId = strtoull(cl_steamId->string, NULL, 10);
#else
                steamId = strtoull(cl_steamId->string, NULL, 10);
#endif
            }
            Com_Memcpy(payload,      "getchallenge ", 13);
            Com_Memcpy(payload + 13, &steamId, 8);

            ticketLen = Steam_AuthTicket(payload + 21, sizeof(payload) - 21);
            if (ticketLen <= 0) {
                Com_Memset(payload + 21, 0, 16);
                ticketLen = 16;
            }
            NET_OutOfBandRaw(NS_CLIENT, clc.serverAddress, payload, 21 + ticketLen);
            break;
        }

        case CA_CHALLENGING:
            port = Cvar_VariableValue("net_qport");

            Q_strncpyz(info, Cvar_InfoString(CVAR_USERINFO), sizeof(info));

            Info_SetValueForKey(info, "protocol", va("%i", com_protocol->integer));
            Info_SetValueForKey(info, "qport", va("%i", port));
            Info_SetValueForKey(info, "challenge", va("%i", clc.challenge));

            Com_sprintf(data, sizeof(data), "connect \"%s\"", info);
            NET_OutOfBandData(NS_CLIENT, clc.serverAddress, (byte*)data, strlen(data));
            cvar_modifiedFlags &= ~CVAR_USERINFO;
            break;

        default:
            Com_Error(ERR_FATAL, "CL_CheckForResend: bad clc.state");
    }
}

void CL_MotdPacket(netadr_t from) {
#ifdef UPDATE_SERVER_NAME
    char* challenge;
    char* info;

    if (!NET_CompareAdr(from, cls.updateServer)) {
        return;
    }

    info = Cmd_Argv(1);

    challenge = Info_ValueForKey(info, "challenge");
    if (strcmp(challenge, cls.updateChallenge)) {
        return;
    }

    challenge = Info_ValueForKey(info, "motd");

    Q_strncpyz(cls.updateInfoString, info, sizeof(cls.updateInfoString));
    Cvar_Set("cl_motdString", challenge);
#endif
}

void CL_InitServerInfo(serverInfo_t* server, netadr_t* address) {
    server->adr = *address;
    server->clients = 0;
    server->hostName[0] = '\0';
    server->mapName[0] = '\0';
    server->maxClients = 0;
    server->maxPing = 0;
    server->minPing = 0;
    server->ping = -1;
    server->game[0] = '\0';
    server->gameType = 0;
    server->netType = 0;
    server->g_humanplayers = 0;
    server->g_needpass = 0;
    server->vac = 0;
    server->description[0] = '\0';
    server->tags[0] = '\0';
}

#define MAX_SERVERSPERPACKET 256

void CL_ServersResponsePacket(const netadr_t* from, msg_t* msg, qboolean extended) {
    int i, j, count, total;
    netadr_t addresses[MAX_SERVERSPERPACKET];
    int numservers;
    byte* buffptr;
    byte* buffend;

    Com_Printf("CL_ServersResponsePacket from %s\n", NET_AdrToStringwPort(*from));

    if (cls.numglobalservers == -1) {
        cls.numglobalservers = 0;
        cls.numGlobalServerAddresses = 0;
    }

    numservers = 0;
    buffptr = msg->data;
    buffend = buffptr + msg->cursize;

    do {
        if (*buffptr == '\\' || (extended && *buffptr == '/'))
            break;

        buffptr++;
    } while (buffptr < buffend);

    while (buffptr + 1 < buffend) {
        if (*buffptr == '\\') {
            buffptr++;

            if (buffend - buffptr < sizeof(addresses[numservers].ip) + sizeof(addresses[numservers].port) + 1)
                break;

            for (i = 0; i < sizeof(addresses[numservers].ip); i++)
                addresses[numservers].ip[i] = *buffptr++;

            addresses[numservers].type = NA_IP;
        }
        else if (extended && *buffptr == '/') {
            buffptr++;

            if (buffend - buffptr < sizeof(addresses[numservers].ip6) + sizeof(addresses[numservers].port) + 1)
                break;

            for (i = 0; i < sizeof(addresses[numservers].ip6); i++)
                addresses[numservers].ip6[i] = *buffptr++;

            addresses[numservers].type = NA_IP6;
            addresses[numservers].scope_id = from->scope_id;
        } else
            break;

        addresses[numservers].port = (*buffptr++) << 8;
        addresses[numservers].port += *buffptr++;
        addresses[numservers].port = BigShort(addresses[numservers].port);

        if (*buffptr != '\\' && *buffptr != '/')
            break;

        numservers++;
        if (numservers >= MAX_SERVERSPERPACKET)
            break;
    }

    count = cls.numglobalservers;

    for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {
        serverInfo_t* server = &cls.globalServers[count];

        for (j = 0; j < count; j++) {
            if (NET_CompareAdr(cls.globalServers[j].adr, addresses[i]))
                break;
        }

        if (j < count)
            continue;

        CL_InitServerInfo(server, &addresses[i]);
        count++;
    }

    if (count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS) {
        for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++) {
            cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
        }
    }

    cls.numglobalservers = count;
    total = count + cls.numGlobalServerAddresses;

    Com_Printf("%d servers parsed (total %d)\n", numservers, total);
}

#define CL_MAX_REDIRECTS 3

static void CL_RedirectPacket(netadr_t from) {
    char server[MAX_OSPATH];
    const char* to = Cmd_Argv(2);
    netadr_t adr;

    if (!cl_followRedirect->integer || clc.state != CA_CHALLENGING || !NET_CompareAdr(from, clc.serverAddress) ||
        Cmd_Argc() != 3 || atoi(Cmd_Argv(1)) != clc.challenge || !to[0] || clRedirectHops >= CL_MAX_REDIRECTS) {
        return;
    }
    if (to[0] == ':') {
        if (from.type == NA_IP6) {
            Com_sprintf(server, sizeof(server), "[%s]%s", NET_AdrToString(from), to);
        } else {
            Com_sprintf(server, sizeof(server), "%s%s", NET_AdrToString(from), to);
        }
    } else {
        Q_strncpyz(server, to, sizeof(server));
    }
    if (strspn(server, "0123456789abcdefABCDEF.:[]") != strlen(server) ||
        !NET_StringToAdr(server, &adr, NA_UNSPEC) || NET_CompareAdr(adr, clc.serverAddress)) {
        return;
    }
    clRedirectHops++;
    clRedirecting = qtrue;
    Com_Printf("Server is full: it sends you to %s\n", server);
    Cbuf_ExecuteText(EXEC_INSERT, va("connect %s\n", server));
}

void CL_ConnectionlessPacket(netadr_t from, msg_t* msg) {
    char* s;
    char* c;

    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg);

    s = MSG_ReadStringLine(msg);

    Cmd_TokenizeString(s);

    c = Cmd_Argv(0);

    Com_DPrintf("CL packet %s: %s\n", NET_AdrToStringwPort(from), c);

    if (!Q_stricmp(c, "challengeResponse")) {
        if (clc.state != CA_CONNECTING) {
            Com_DPrintf("Unwanted challenge response received. Ignored.\n");
            return;
        }

        clc.challenge = atoi(Cmd_Argv(1));
        clc.state = CA_CHALLENGING;
        clc.connectPacketCount = 0;
        clc.connectTime = -99999;

        clc.serverAddress = from;
        Com_DPrintf("challengeResponse: %d\n", clc.challenge);
        return;
    }

    if (!Q_stricmp(c, "connectResponse")) {
        if (clc.state >= CA_CONNECTED) {
            Com_Printf("Dup connect received. Ignored.\n");
            return;
        }
        if (clc.state != CA_CHALLENGING) {
            Com_Printf("connectResponse packet while not connecting. Ignored.\n");
            return;
        }
        if (!NET_CompareAdr(from, clc.serverAddress)) {
            Com_Printf("connectResponse from wrong address. Ignored.\n");
            return;
        }

        Netchan_Setup(NS_CLIENT, &clc.netchan, from, Cvar_VariableValue("net_qport"),
                      clc.challenge);

        clc.state = CA_CONNECTED;
        clc.lastPacketSentTime = -9999;
        return;
    }

    if (!Q_stricmp(c, "infoResponse")) {
        CL_ServerInfoPacket(from, msg);
        return;
    }

    if (!Q_stricmp(c, "statusResponse")) {
        CL_ServerStatusResponse(from, msg);
        return;
    }

    if (!Q_stricmp(c, "echo")) {
        if (NET_CompareAdr(from, clc.serverAddress) || NET_CompareAdr(from, cls.rconAddress)) {
            NET_OutOfBandPrint(NS_CLIENT, from, "%s", Cmd_Argv(1));
        }
        return;
    }

    if (!Q_stricmp(c, "keyAuthorize")) {
        return;
    }

    if (!Q_stricmp(c, "motd")) {
        CL_MotdPacket(from);
        return;
    }

    if (!Q_stricmp(c, "redirect")) {
        CL_RedirectPacket(from);
        return;
    }

    if (!Q_stricmp(c, "print")) {
        if (NET_CompareAdr(from, clc.serverAddress) || NET_CompareAdr(from, cls.rconAddress)) {
            s = MSG_ReadString(msg);

            Q_strncpyz(clc.serverMessage, s, sizeof(clc.serverMessage));
            Com_Printf("%s", s);
        }
        return;
    }

    if (!Q_strncmp(c, "getserversResponse", 18)) {
        CL_ServersResponsePacket(&from, msg, qfalse);
        return;
    }

    if (!Q_strncmp(c, "getserversExtResponse", 21)) {
        CL_ServersResponsePacket(&from, msg, qtrue);
        return;
    }

    Com_DPrintf("Unknown connectionless packet command.\n");
}

void CL_PacketEvent(netadr_t from, msg_t* msg) {
    int headerBytes;

    clc.lastPacketTime = cls.realtime;

    if (msg->cursize >= 4 && *(int*)msg->data == -1) {
        CL_ConnectionlessPacket(from, msg);
        return;
    }

    if (clc.state < CA_CONNECTED) {
        return;
    }

    if (msg->cursize < 4) {
        Com_Printf("%s: Runt packet\n", NET_AdrToStringwPort(from));
        return;
    }

    if (!NET_CompareAdr(from, clc.netchan.remoteAddress)) {
        Com_DPrintf("%s:sequenced packet without connection\n", NET_AdrToStringwPort(from));
        return;
    }

    if (!CL_Netchan_Process(&clc.netchan, msg)) {
        return;
    }

    headerBytes = msg->readcount;

    clc.serverMessageSequence = LittleLong(*(int*)msg->data);

    clc.lastPacketTime = cls.realtime;
    CL_ParseServerMessage(msg);

    if (clc.demorecording && !clc.demowaiting) {
        CL_WriteDemoMessage(msg, headerBytes);
    }
}

void CL_CheckTimeout(void) {
    if (clc.demoplaying) {
        cl.timeoutcount = 0;
        return;
    }

    if ((!CL_CheckPaused() || !sv_paused->integer) && clc.state >= CA_CONNECTED && clc.state != CA_CINEMATIC && cls.realtime - clc.lastPacketTime > cl_timeout->value * 1000) {
        if (++cl.timeoutcount > 5) {
            Com_Printf("\nServer connection timed out.\n");
            CL_Disconnect(qtrue);
            return;
        }
    } else {
        cl.timeoutcount = 0;
    }
}

qboolean CL_CheckPaused(void) {
    if (cl_paused->integer || cl_paused->modified)
        return qtrue;

    return qfalse;
}

void CL_CheckUserinfo(void) {
    if (clc.state < CA_CONNECTED)
        return;

    if (CL_CheckPaused())
        return;

    if (cvar_modifiedFlags & CVAR_USERINFO) {
        cvar_modifiedFlags &= ~CVAR_USERINFO;
        CL_AddReliableCommand(va("userinfo \"%s\"", Cvar_InfoString(CVAR_USERINFO)), qfalse);
    }
}

void CL_LowLatencyWait(void) {
    if (cls.rendererStarted && re.LowLatencyWait) {
        re.LowLatencyWait();
    }
}

int CL_DisplayFrequency(void) {
    return cls.rendererStarted ? cls.glconfig.displayFrequency : 0;
}

void CL_Frame(int msec) {
    if (!com_cl_running->integer) {
        return;
    }

#ifdef USE_CURL
    if (clc.downloadCURLM) {
        CL_cURL_PerformDownload();
        if (clc.cURLDisconnected) {
            cls.realFrametime = msec;
            cls.frametime = msec;
            cls.realtime += cls.frametime;
            SCR_UpdateScreen();
            S_Update();
            Con_RunConsole();
            cls.framecount++;
            return;
        }
    }
#endif

    if (clc.state == CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_UI) && !com_sv_running->integer && uivm) {
        S_StopAllSounds();
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN);
    }

    if (cl_avidemoLatch->integer &&
        (!cl_avidemoMinTime->integer || cl.serverTime > cl_avidemoMinTime->integer)) {
        Cvar_Set("cl_avidemo", va("%d", cl_avidemoLatch->integer));
        Cvar_Set("cl_avidemo_latch", "");
    }

    if (cl_avidemo->integer && msec) {
        if (clc.state == CA_ACTIVE || cl_forceavidemo->integer) {
            qboolean early = (qboolean)(cl_avidemoMinTime->integer && cl.serverTime < cl_avidemoMinTime->integer);

            if (cl_avidemoMaxTime->integer && cl.serverTime > cl_avidemoMaxTime->integer) {
                CL_Disconnect_f();
                return;
            }
            if (!early) {
                Cbuf_ExecuteText(EXEC_NOW, "screenshot silent\n");
            }
        }
        msec = (1000 / cl_avidemo->integer) * com_timescale->value;
        if (msec == 0) {
            msec = 1;
        }
    }

    if (CL_VideoRecording() && cl_aviFrameRate->integer && msec) {
        if (clc.state == CA_ACTIVE || cl_forceavidemo->integer) {
            float fps = MIN(cl_aviFrameRate->value * com_timescale->value, 1000.0f);
            float frameDuration = MAX(1000.0f / fps, 1.0f) + clc.aviVideoFrameRemainder;

            CL_TakeVideoFrame();

            msec = (int)frameDuration;
            clc.aviVideoFrameRemainder = frameDuration - msec;
        }
    }

    if (cl_autoRecordDemo->integer) {
        if (clc.state == CA_ACTIVE && !clc.demorecording && !clc.demoplaying) {
            qtime_t now;
            char* nowString;
            char* p;
            char mapName[MAX_QPATH];
            char serverName[MAX_OSPATH];

            Com_RealTime(&now);
            nowString = va("%04d%02d%02d%02d%02d%02d",
                           1900 + now.tm_year,
                           1 + now.tm_mon,
                           now.tm_mday,
                           now.tm_hour,
                           now.tm_min,
                           now.tm_sec);

            Q_strncpyz(serverName, clc.servername, MAX_OSPATH);
            p = strstr(serverName, ":");
            if (p) {
                *p = '.';
            }

            Q_strncpyz(mapName, COM_SkipPath(cl.mapname), sizeof(cl.mapname));
            COM_StripExtension(mapName, mapName, sizeof(mapName));

            Cbuf_ExecuteText(EXEC_NOW,
                             va("record %s-%s-%s", nowString, serverName, mapName));
        } else if (clc.state != CA_ACTIVE && clc.demorecording) {
            CL_StopRecord_f();
        }
    }

    cls.realFrametime = msec;

    cls.frametime = msec;

    cls.realtime += cls.frametime;

    if (cl_timegraph->integer) {
        SCR_DebugGraph(cls.realFrametime * 0.25);
    }

    CL_CheckUserinfo();

    CL_CheckTimeout();

    CL_SendCmd();

    CL_CheckForResend();

    CL_SteamServersFrame();

    CL_WorkshopFrame();
    CL_WarmPlayers();
    CL_UpdateFrame();

    CL_MatchReportFrame();
    CL_VoiceFrame();

    CL_SetCGameTime();

    SCR_UpdateScreen();

    S_Update();

    SCR_RunCinematic();

    Con_RunConsole();

    cls.framecount++;
}

static __attribute__((format(printf, 2, 3))) void QDECL CL_RefPrintf(int print_level, const char* fmt, ...) {
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (print_level == PRINT_ALL) {
        Com_Printf("%s", msg);
    } else if (print_level == PRINT_WARNING) {
        Com_Printf(S_COLOR_YELLOW "%s", msg);
    } else if (print_level == PRINT_ERROR) {
        Com_Printf(S_COLOR_RED "%s", msg);
    } else if (print_level == PRINT_DEVELOPER) {
#if DEBUG_RENDERER
        Com_DPrintf(S_COLOR_RED "%s", msg);
#endif
    }
}

void CL_ShutdownRef(void) {
    if (re.Shutdown) {
        re.Shutdown(qtrue);
    }

    Com_Memset(&re, 0, sizeof(re));

#ifdef USE_RENDERER_DLOPEN
    if (rendererLib) {
        Sys_UnloadLibrary(rendererLib);
        rendererLib = NULL;
    }
#endif
}

void CL_InitRenderer(void) {
    re.BeginRegistration(&cls.glconfig);

    cls.charSetShader = re.RegisterShader("gfx/2d/bigchars");
    cls.whiteShader = re.RegisterShader("white");
    cls.consoleShader = re.RegisterShader("console");
    cls.recordShader = re.RegisterShaderNoMip("icons/record");
    g_console_field_width = cls.glconfig.vidWidth / SMALLCHAR_WIDTH - 2;
    g_consoleField.widthInChars = g_console_field_width;
}

void CL_StartHunkUsers(qboolean rendererOnly) {
    if (!com_cl_running) {
        return;
    }

    if (!com_cl_running->integer) {
        return;
    }

    if (!cls.rendererStarted) {
        cls.rendererStarted = qtrue;
        CL_InitRenderer();
    }

    if (rendererOnly) {
        return;
    }

    if (!cls.soundStarted) {
        cls.soundStarted = qtrue;
        S_Init();
    }

    if (!cls.soundRegistered) {
        cls.soundRegistered = qtrue;
        S_BeginRegistration();
    }

    if (com_dedicated->integer) {
        return;
    }

    if (!cls.uiStarted) {
        cls.uiStarted = qtrue;
        CL_InitUI();
    }
}

void* CL_RefMalloc(int size) {
    return Z_TagMalloc(size, TAG_RENDERER);
}

int CL_ScaledMilliseconds(void) {
    return Sys_Milliseconds() * com_timescale->value;
}

void CL_InitRef(void) {
    refimport_t ri;
    refexport_t* ret;
#ifdef USE_RENDERER_DLOPEN
    GetRefAPI_t GetRefAPI;
    char dllName[MAX_OSPATH];
#endif

    Com_Printf("----- Initializing Renderer ----\n");

#ifdef USE_RENDERER_DLOPEN
    cl_renderer = Cvar_Get("cl_renderer", "opengl1", CVAR_ARCHIVE | CVAR_LATCH);

    {
        cvar_t* seen = Cvar_Get("cl_rendererDefault", "opengl2", CVAR_ARCHIVE);

        if (Q_stricmp(seen->string, cl_renderer->resetString)) {
            if (!Q_stricmp(cl_renderer->string, seen->string)) {
                Cvar_Set("cl_renderer", cl_renderer->resetString);
            }
            Cvar_Set("cl_rendererDefault", cl_renderer->resetString);
        }
    }

    rendererLib = NULL;

    if (Q_stristr(Sys_BinaryName(), "-vulkan")) {
        Com_sprintf(dllName, sizeof(dllName), "vulkan" ARCH_STRING DLL_EXT);
        rendererLib = Sys_LoadDll(dllName, qfalse);
        if (!rendererLib)
            Com_Printf("failed:\n\"%s\"\n", Sys_LibraryError());
    }

    if (!rendererLib) {
        Com_sprintf(dllName, sizeof(dllName), "%s" ARCH_STRING DLL_EXT, cl_renderer->string);
        rendererLib = Sys_LoadDll(dllName, qfalse);
    }

    if (!rendererLib && Q_stricmp(cl_renderer->string, cl_renderer->resetString)) {
        Com_Printf("failed:\n\"%s\"\n", Sys_LibraryError());
        Cvar_ForceReset("cl_renderer");
        Com_sprintf(dllName, sizeof(dllName), "%s" ARCH_STRING DLL_EXT, cl_renderer->string);
        rendererLib = Sys_LoadDll(dllName, qfalse);
    }
    if (!rendererLib && Q_stricmp(cl_renderer->string, "opengl2")) {
        Com_Printf("failed:\n\"%s\"\n", Sys_LibraryError());
        Com_sprintf(dllName, sizeof(dllName), "opengl2" ARCH_STRING DLL_EXT);
        rendererLib = Sys_LoadDll(dllName, qfalse);
    }
    if (!rendererLib) {
        Com_Printf("failed:\n\"%s\"\n", Sys_LibraryError());
        Com_Error(ERR_FATAL, "Failed to load renderer");
    }

    GetRefAPI = Sys_LoadFunction(rendererLib, "GetRefAPI");
    if (!GetRefAPI) {
        Com_Error(ERR_FATAL, "Can't load symbol GetRefAPI: '%s'", Sys_LibraryError());
    }
#endif

    ri.Cmd_AddCommand = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cmd_Argc = Cmd_Argc;
    ri.Cmd_Argv = Cmd_Argv;
    ri.Cmd_ExecuteText = Cbuf_ExecuteText;
    ri.Printf = CL_RefPrintf;
    ri.Error = Com_Error;
    ri.Milliseconds = CL_ScaledMilliseconds;
    ri.Microseconds = Sys_Microseconds;
    ri.JobsSubmit = Com_JobsSubmit;
    ri.JobWorkers = Com_JobWorkers;
    ri.Sleep = Sys_Sleep;
    ri.ThreadYield = Sys_ThreadYield;
    ri.Malloc = CL_RefMalloc;
    ri.Free = Z_Free;
#ifdef HUNK_DEBUG
    ri.Hunk_AllocDebug = Hunk_AllocDebug;
#else
    ri.Hunk_Alloc = Hunk_Alloc;
#endif
    ri.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
    ri.Hunk_FreeTempMemory = Hunk_FreeTempMemory;

    ri.CM_ClusterPVS = CM_ClusterPVS;
    ri.CM_DrawDebugSurface = CM_DrawDebugSurface;

    ri.FS_ReadFile = FS_ReadFile;
    ri.FS_FreeFile = FS_FreeFile;
    ri.FS_WriteFile = FS_WriteFile;
    ri.FS_FreeFileList = FS_FreeFileList;
    ri.FS_ListFiles = FS_ListFiles;
    ri.FS_FileIsInPAK = FS_FileIsInPAK;
    ri.FS_FileExists = FS_FileExists;
    ri.FS_ReadCacheFile = FS_ReadCacheFile;
    ri.FS_WriteCacheFile = FS_WriteCacheFile;
    ri.Cvar_Get = Cvar_Get;
    ri.Cvar_Set = Cvar_Set;
    ri.Cvar_SetValue = Cvar_SetValue;
    ri.Cvar_CheckRange = Cvar_CheckRange;
    ri.Cvar_GetRange = Cvar_GetRange;
    ri.Cvar_SetDescription = Cvar_SetDescription;
    ri.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

    ri.CIN_UploadCinematic = CIN_UploadCinematic;
    ri.CIN_PlayCinematic = CIN_PlayCinematic;
    ri.CIN_RunCinematic = CIN_RunCinematic;

    ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;

    ri.IN_Init = IN_Init;
    ri.IN_Shutdown = IN_Shutdown;
    ri.IN_Restart = IN_Restart;

    ri.ftol = Q_ftol;

    ri.Sys_SetEnv = Sys_SetEnv;
    ri.Sys_GLimpSafeInit = Sys_GLimpSafeInit;
    ri.Sys_GLimpInit = Sys_GLimpInit;
    ri.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;

    ret = GetRefAPI(REF_API_VERSION, &ri);

#if defined __USEA3D && defined __A3D_GEOM
    hA3Dg_ExportRenderGeom(ret);
#endif

#ifdef USE_RENDERER_DLOPEN
    if (!ret) {
        char fallback[MAX_OSPATH];

        Com_sprintf(fallback, sizeof(fallback), "%s" ARCH_STRING DLL_EXT, cl_renderer->resetString);
        if (Q_stricmp(dllName, fallback)) {
            Com_Printf("...going on with %s\n", fallback);
            Sys_UnloadLibrary(rendererLib);
            rendererLib = Sys_LoadDll(fallback, qfalse);
            GetRefAPI = rendererLib ? Sys_LoadFunction(rendererLib, "GetRefAPI") : NULL;
            ret = GetRefAPI ? GetRefAPI(REF_API_VERSION, &ri) : NULL;
        }
    }
#endif

    Com_Printf("-------------------------------\n");

    if (!ret) {
        Com_Error(ERR_FATAL, "Couldn't initialize refresh");
    }

    re = *ret;

    Cvar_Set("cl_paused", "0");
}

void CL_SetModel_f(void) {
    char* arg;
    char name[256];

    arg = Cmd_Argv(1);
    if (arg[0]) {
        Cvar_Set("model", arg);
        Cvar_Set("headmodel", arg);
    } else {
        Cvar_VariableStringBuffer("model", name, sizeof(name));
        Com_Printf("model is set to %s\n", name);
    }
}

void CL_Sayto_f(void) {
    char* rawname;
    char name[MAX_NAME_LENGTH];
    char cleanName[MAX_NAME_LENGTH];
    const char* info;
    int count;
    int i;
    int clientNum;
    char* p;

    if (Cmd_Argc() < 3) {
        Com_Printf("sayto <player name> <text>\n");
        return;
    }

    rawname = Cmd_Argv(1);

    Com_FieldStringToPlayerName(name, MAX_NAME_LENGTH, rawname);

    info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
    count = atoi(Info_ValueForKey(info, "sv_maxclients"));

    clientNum = -1;
    for (i = 0; i < count; i++) {
        info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + i];
        Q_strncpyz(cleanName, Info_ValueForKey(info, "n"), sizeof(cleanName));
        Q_CleanStr(cleanName);

        if (!Q_stricmp(cleanName, name)) {
            clientNum = i;
            break;
        }
    }
    if (clientNum <= -1) {
        Com_Printf("No such player name: %s.\n", name);
        return;
    }

    p = Cmd_ArgsFrom(2);

    if (*p == '"') {
        p++;
        p[strlen(p) - 1] = 0;
    }

    CL_AddReliableCommand(va("tell %i \"%s\"", clientNum, p), qfalse);
}

void CL_Web_ChangeHash_f(void) {
    char* web_path;

    if (clc.state < CA_CONNECTED || !uivm) {
        Com_Printf("You must be in-game to use this command.");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Not enough arguments.\n");
        return;
    }

    web_path = Cmd_Argv(1);

	if (web_path[0] == '/') {
		web_path++;
	}

    if (Q_stristr(web_path, "settings")) {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN_OPTIONS);
    } else {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_HOME);
    }
}

static void CL_Web_HideBrowser_f(void) {
    if (!uivm || clc.state != CA_ACTIVE) {
        return;
    }
    VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE);
}

void CL_OpenURL(const char* url) {
    const char* p;

    if (!url || Q_strncmp(url, "https://", 8)) {
        Com_Printf("openURL: refused, not an https link\n");
        return;
    }
    for (p = url; *p; p++) {
        if (*p <= ' ' || *p >= 127 || *p == '"' || *p == '\'' || *p == '\\') {
            Com_Printf("openURL: refused, bad character in the link\n");
            return;
        }
    }
    if (p - url > 512) {
        Com_Printf("openURL: refused, link too long\n");
        return;
    }
    Com_Printf("opening %s\n", url);
    Sys_OpenURL(url);
}

#define MAX_ALIASES         100
#define ALIAS_NAME_SIZE     32
#define ALIAS_CMD_SIZE      256

typedef struct {
    char    name[ALIAS_NAME_SIZE];
    char    cmd[ALIAS_CMD_SIZE];
    int     active;
} alias_t;

static alias_t aliases[MAX_ALIASES];

void *CL_FindAlias(const char *name) {
    int i;
    for (i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].active && !Q_stricmp(name, aliases[i].name)) {
            return &aliases[i];
        }
    }
    return NULL;
}

void CL_RunAlias(void) {
    const char *name;
    alias_t *a;

    name = Cmd_Argv(0);
    a = (alias_t *)CL_FindAlias(name);
    if (a) {
        Cbuf_ExecuteText(EXEC_INSERT, va("%s\n", a->cmd));
        return;
    }
    Com_Printf("^1ALIAS: %s not found^7\n", Cmd_Argv(0));
}

static void CL_ListAliases(void) {
    int i;
    Com_Printf("Alias List:\n");
    for (i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].active) {
            Com_Printf("  %s \"%s\"\n", aliases[i].name, aliases[i].cmd);
        }
    }
}

static void CL_Alias_f(void) {
    alias_t *a;
    int i;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: alias \"command\"\n");
        CL_ListAliases();
        return;
    }

    if (Cmd_Argc() < 3) {
        a = (alias_t *)CL_FindAlias(Cmd_Argv(0));
        if (a) {
            Com_Printf("Alias: %s \"%s\"\n", a->name, a->cmd);
        }
        return;
    }

    a = (alias_t *)CL_FindAlias(Cmd_Argv(1));
    if (a) {
        Q_strncpyz(a->cmd, Cmd_Argv(2), ALIAS_CMD_SIZE);
        return;
    }

    for (i = 0; i < MAX_ALIASES; i++) {
        if (!aliases[i].active) {
            Q_strncpyz(aliases[i].name, Cmd_Argv(1), ALIAS_NAME_SIZE);
            Q_strncpyz(aliases[i].cmd, Cmd_Argv(2), ALIAS_CMD_SIZE);
            aliases[i].active = 1;
            return;
        }
    }

    Com_Printf("^1No free alias slots. Use UnAlias to remove an alias before adding this one.^7\n");
}

static void CL_Unalias_f(void) {
    alias_t *a;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: unalias \"command\"\n");
        CL_ListAliases();
        return;
    }

    a = (alias_t *)CL_FindAlias(Cmd_Argv(1));
    if (a) {
        a->active = 0;
        Cmd_RemoveCommand(a->name);
    }
}

static void CL_UnaliasAll_f(void) {
    int i;
    for (i = 0; i < MAX_ALIASES; i++) {
        aliases[i].active = 0;
    }
}

void Cmd_WriteAliases(fileHandle_t f) {
    int i;
    FS_Printf(f, "unaliasall\n");
    for (i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].active) {
            FS_Printf(f, "alias %s \"%s\"\n", aliases[i].name, aliases[i].cmd);
        }
    }
}

void CL_InitAliases(void) {
    memset(aliases, 0, sizeof(aliases));
    Cmd_AddCommand("alias", CL_Alias_f);
    Cmd_AddCommand("unalias", CL_Unalias_f);
    Cmd_AddCommand("unaliasall", CL_UnaliasAll_f);
}

static void CL_VoiceOn_f(void);
static void CL_Friends_f(void);
static void CL_ClientFriendInvite_f(void);
static void CL_ClientViewProfile_f(void);
static void CL_VoiceOff_f(void);

void CL_Init(void) {
    Com_Printf("----- Client Initialization -----\n");

    Con_Init();

    if (!com_fullyInitialized) {
        CL_ClearState();
        clc.state = CA_DISCONNECTED;
        cl_oldGameSet = qfalse;
    }

    cls.realtime = 0;

    CL_InitInput();

    cl_noprint = Cvar_Get("cl_noprint", "0", 0);
#ifdef UPDATE_SERVER_NAME
    cl_motd = Cvar_Get("cl_motd", "1", 0);
#endif

    cl_timeout = Cvar_Get("cl_timeout", "40", 0);

    cl_timeNudge = Cvar_GetRange("cl_timeNudge", "0", "-20", "0", CVAR_ARCHIVE | CVAR_REPLICATE);
    cl_autoTimeNudge = Cvar_GetRange("cl_autoTimeNudge", "0", "0", "1", CVAR_ARCHIVE | CVAR_REPLICATE);
    cl_shownet = Cvar_Get("cl_shownet", "0", CVAR_TEMP);
    cl_showSend = Cvar_Get("cl_showSend", "0", CVAR_TEMP);
    cl_showTimeDelta = Cvar_Get("cl_showTimeDelta", "0", CVAR_TEMP);
    cl_freezeDemo = Cvar_Get("cl_freezeDemo", "0", CVAR_TEMP);
    rcon_client_password = Cvar_Get("rconPassword", "", CVAR_TEMP);
    cl_activeAction = Cvar_Get("activeAction", "", CVAR_TEMP);

    cl_timedemo = Cvar_Get("timedemo", "0", 0);
    cl_timedemoLog = Cvar_Get("cl_timedemoLog", "", CVAR_ARCHIVE);
    cl_autoRecordDemo = Cvar_Get("cl_autoRecordDemo", "0", CVAR_ARCHIVE);
    cl_aviFrameRate = Cvar_Get("cl_aviFrameRate", "25", CVAR_ARCHIVE);
    cl_aviMotionJpeg = Cvar_Get("cl_aviMotionJpeg", "1", CVAR_ARCHIVE);
    cl_forceavidemo = Cvar_Get("cl_forceavidemo", "0", 0);
    cl_avidemo = Cvar_Get("cl_avidemo", "0", 0);
    cl_avidemoLatch = Cvar_Get("cl_avidemo_latch", "", 0);
    cl_avidemoMinTime = Cvar_Get("cl_avidemo_mintime", "", 0);
    cl_avidemoMaxTime = Cvar_Get("cl_avidemo_maxtime", "", 0);

    rconAddress = Cvar_Get("rconAddress", "", 0);

    cl_yawspeed = Cvar_Get("cl_yawspeed", "140", CVAR_CHEAT);
    cl_pitchspeed = Cvar_Get("cl_pitchspeed", "140", CVAR_CHEAT);
    cl_anglespeedkey = Cvar_Get("cl_anglespeedkey", "1.5", CVAR_CHEAT);

    cl_maxpackets = Cvar_Get("cl_maxpackets", "125", CVAR_CHEAT);
    cl_packetdup = Cvar_Get("cl_packetdup", "1", CVAR_ARCHIVE | CVAR_USERSAVE);

    cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE);

    cl_sensitivity = Cvar_Get("sensitivity", "4", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccel = Cvar_Get("cl_mouseAccel", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccelOffset = Cvar_Get("cl_mouseAccelOffset", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccelPower = Cvar_Get("cl_mouseAccelPower", "2", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccelDebug = Cvar_Get("cl_mouseAccelDebug", "0", 0);
    cl_mouseSensCap = Cvar_Get("cl_mouseSensCap", "0", CVAR_ARCHIVE | CVAR_USERSAVE);
    cl_freelook = Cvar_Get("cl_freelook", "1", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cg_ignoreMouseInput = Cvar_Get("cg_ignoreMouseInput", "0", CVAR_ROM);
    Cvar_Get("cl_viewAccel", "1.7", CVAR_ARCHIVE | CVAR_USERSAVE);

    cl_allowDownload = Cvar_Get("cl_allowDownload", "0", CVAR_ARCHIVE);
    cl_followRedirect = Cvar_Get("cl_followRedirect", "1", CVAR_ARCHIVE);
    CL_UpdateInit();
#ifdef USE_CURL_DLOPEN
    cl_cURLLib = Cvar_Get("cl_cURLLib", DEFAULT_CURL_LIB, CVAR_ARCHIVE | CVAR_PROTECTED);
#endif

    cl_conXOffset = Cvar_Get("cl_conXOffset", "0", 0);
#ifdef __APPLE__
    cl_inGameVideo = Cvar_Get("r_inGameVideo", "0", CVAR_ARCHIVE);
#else
    cl_inGameVideo = Cvar_Get("r_inGameVideo", "1", CVAR_ARCHIVE);
#endif

    cl_serverStatusResendTime = Cvar_Get("cl_serverStatusResendTime", "750", 0);

    m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    m_forward = Cvar_Get("m_forward", "0.25", CVAR_ARCHIVE | CVAR_USERSAVE);
    m_side = Cvar_Get("m_side", "0.25", CVAR_ARCHIVE | CVAR_USERSAVE);
    m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    m_cpi = Cvar_Get("m_cpi", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);

    j_pitch = Cvar_Get("j_pitch", "0.022", CVAR_ARCHIVE);
    j_yaw = Cvar_Get("j_yaw", "-0.022", CVAR_ARCHIVE);
    j_forward = Cvar_Get("j_forward", "-0.25", CVAR_ARCHIVE);
    j_side = Cvar_Get("j_side", "0.25", CVAR_ARCHIVE);
    j_up = Cvar_Get("j_up", "0", CVAR_ARCHIVE);

    j_pitch_axis = Cvar_Get("j_pitch_axis", "3", CVAR_ARCHIVE);
    j_yaw_axis = Cvar_Get("j_yaw_axis", "2", CVAR_ARCHIVE);
    j_forward_axis = Cvar_Get("j_forward_axis", "1", CVAR_ARCHIVE);
    j_side_axis = Cvar_Get("j_side_axis", "0", CVAR_ARCHIVE);
    j_up_axis = Cvar_Get("j_up_axis", "4", CVAR_ARCHIVE);

    Cvar_CheckRange(j_pitch_axis, 0, MAX_JOYSTICK_AXIS - 1, qtrue);
    Cvar_CheckRange(j_yaw_axis, 0, MAX_JOYSTICK_AXIS - 1, qtrue);
    Cvar_CheckRange(j_forward_axis, 0, MAX_JOYSTICK_AXIS - 1, qtrue);
    Cvar_CheckRange(j_side_axis, 0, MAX_JOYSTICK_AXIS - 1, qtrue);
    Cvar_CheckRange(j_up_axis, 0, MAX_JOYSTICK_AXIS - 1, qtrue);

    cl_motdString = Cvar_Get("cl_motdString", "", CVAR_ROM);

    Cvar_Get("cl_maxPing", "800", CVAR_ARCHIVE);

    cl_lanForcePackets = Cvar_Get("cl_lanForcePackets", "1", CVAR_ARCHIVE);

    cl_consoleKeys = Cvar_Get("cl_consoleKeys", "~ ` 0x7e 0x60", CVAR_ARCHIVE);

    cl_steamId = Cvar_Get("cl_steamId", "0", CVAR_ARCHIVE);
    Cvar_Get("cl_quitOnDemoCompleted", "0", 0);

    Cvar_Get("name", "UnnamedPlayer", CVAR_USERINFO | CVAR_ARCHIVE);
    cl_rate = Cvar_Get("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_CheckRange(cl_rate, 8000, 25000, qtrue);
    Cvar_Get("model", "sarge", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_Get("headmodel", "sarge", CVAR_USERINFO | CVAR_ARCHIVE);

    Cvar_Get("color1", "4", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_Get("color2", "5", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_Get("handicap", "100", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_Get("teamtask", "0", CVAR_USERINFO);
    Cvar_Get("sex", "male", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_SetDescription(Cvar_Get("country", "", CVAR_USERINFO | CVAR_ARCHIVE),
                        "Your country, as its two-letter code in lower case (cl, ar, us...), for the flag\n"
                        "next to your name on the scoreboards. Empty shows no flag.");
    Cvar_Get("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE);

    Cvar_Get("password", "", CVAR_USERINFO);
    Cvar_Get("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_Get("ioql", PRODUCT_VERSION, CVAR_USERINFO | CVAR_ROM);

    Cvar_Get("cg_viewsize", "100", CVAR_ARCHIVE);

    Cmd_AddCommand("cmd", CL_ForwardToServer_f);
    Cmd_AddCommand("configstrings", CL_Configstrings_f);
    Cmd_AddCommand("clientinfo", CL_Clientinfo_f);
    Cmd_AddCommand("snd_restart", CL_Snd_Restart_f);
    Cmd_AddCommand("vid_restart", CL_Vid_Restart_f);
    Cmd_AddCommand("disconnect", CL_Disconnect_f);
    Cmd_AddCommand("record", CL_Record_f);
    Cmd_AddCommand("demo", CL_PlayDemo_f);
    Cmd_SetCommandCompletionFunc("demo", CL_CompleteDemoName);
    Cmd_AddCommand("rewind", CL_Rewind_f);
    Cmd_AddCommand("fastforward", CL_FastForward_f);
    Cmd_AddCommand("seekdemo", CL_SeekDemo_f);
    Cmd_AddCommand("fragforward", CL_FragForward_f);
    Cvar_Get("cl_demoLength", "0", CVAR_ROM);
    Cvar_Get("cl_demoPos", "0", CVAR_ROM);
    Cvar_Get("cl_demoMarks", "", CVAR_ROM);
    Cvar_Get("cl_demoFrags", "", CVAR_ROM);
    Cvar_Get("cl_demorecording", "0", CVAR_ROM);
    Cvar_Get("vid_resizeCount", "0", CVAR_ROM);
    Cvar_Get("cl_demoname", "", CVAR_ROM);
    Cmd_AddCommand("stoprecord", CL_StopRecord_f);
    Cmd_AddCommand("connect", CL_Connect_f);
    Cmd_AddCommand("reconnect", CL_Reconnect_f);
    Cmd_AddCommand("rcon", CL_Rcon_f);
    Cmd_SetCommandCompletionFunc("rcon", CL_CompleteRcon);
    Cmd_AddCommand("localservers", CL_LocalServers_f);
    Cmd_AddCommand("globalservers", CL_GlobalServers_f);
    Cmd_AddCommand("friends", CL_Friends_f);
    Cmd_AddCommand("clientfriendinvite", CL_ClientFriendInvite_f);
    Cmd_AddCommand("clientviewprofile", CL_ClientViewProfile_f);
    Cmd_AddCommand("+voice", CL_VoiceOn_f);
    Cmd_AddCommand("-voice", CL_VoiceOff_f);
    Cmd_AddCommand("showip", CL_ShowIP_f);
    Cmd_AddCommand("fs_openedList", CL_OpenedPK3List_f);
    Cmd_AddCommand("fs_referencedList", CL_ReferencedPK3List_f);
    Cmd_AddCommand("model", CL_SetModel_f);
    Cmd_AddCommand("web_changeHash", CL_Web_ChangeHash_f);
    Cmd_AddCommand("web_hideBrowser", CL_Web_HideBrowser_f);
    Cmd_AddCommand("postprocess_restart", CL_PostProcessRestart_f);

    cl_demoRecordMessage = Cvar_Get("cl_demoRecordMessage", "2", CVAR_ARCHIVE | CVAR_USERSAVE);
    Cvar_CheckRange(cl_demoRecordMessage, 0, 2, qtrue);
    Cmd_AddCommand("userinfo", CL_Userinfo_f);
    if (!com_dedicated->integer) {
        Cmd_AddCommand("sayto", CL_Sayto_f);
        Cmd_SetCommandCompletionFunc("sayto", CL_CompletePlayerName);
    }
    CL_InitAliases();

    CL_InitRef();

    SCR_Init();

    Cvar_Set("cl_running", "1");

    Com_Printf("----- Client Initialization Complete -----\n");
}

void CL_Shutdown(char* finalmsg, qboolean disconnect, qboolean quit) {
    static qboolean recursive = qfalse;

    if (!(com_cl_running && com_cl_running->integer))
        return;

    Com_Printf("----- CL_Shutdown -----\n");

    if (recursive) {
        Com_Printf("WARNING: Recursive shutdown\n");
        return;
    }
    recursive = qtrue;

    noGameRestart = quit;

    if (disconnect)
        CL_Disconnect(qtrue);

    CL_ClearMemory(qtrue);
    CL_Snd_Shutdown();

    Cmd_RemoveCommand("cmd");
    Cmd_RemoveCommand("configstrings");
    Cmd_RemoveCommand("clientinfo");
    Cmd_RemoveCommand("snd_restart");
    Cmd_RemoveCommand("vid_restart");
    Cmd_RemoveCommand("disconnect");
    Cmd_RemoveCommand("record");
    Cmd_RemoveCommand("demo");
    Cmd_RemoveCommand("rewind");
    Cmd_RemoveCommand("fastforward");
    Cmd_RemoveCommand("seekdemo");
    Cmd_RemoveCommand("fragforward");
    Cmd_RemoveCommand("stoprecord");
    Cmd_RemoveCommand("connect");
    Cmd_RemoveCommand("reconnect");
    Cmd_RemoveCommand("rcon");
    Cmd_RemoveCommand("showip");
    Cmd_RemoveCommand("fs_openedList");
    Cmd_RemoveCommand("fs_referencedList");
    Cmd_RemoveCommand("model");
    Cmd_RemoveCommand("postprocess_restart");
    Cmd_RemoveCommand("userinfo");

    CL_ShutdownInput();
    Con_Shutdown();

    Cvar_Set("cl_running", "0");

    recursive = qfalse;

    Com_Memset(&cls, 0, sizeof(cls));
    Key_SetCatcher(0);

    Com_Printf("-----------------------\n");
}

static void CL_SetServerInfo(serverInfo_t* server, const char* info, int ping) {
    if (server) {
        if (info) {
            server->clients = atoi(Info_ValueForKey(info, "clients"));
            Q_strncpyz(server->hostName, Info_ValueForKey(info, "hostname"), sizeof(server->hostName));
            Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"), MAX_NAME_LENGTH);
            server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
            Q_strncpyz(server->game, Info_ValueForKey(info, "game"), MAX_NAME_LENGTH);
            server->gameType = atoi(Info_ValueForKey(info, "gametype"));
            server->netType = atoi(Info_ValueForKey(info, "nettype"));
            server->minPing = atoi(Info_ValueForKey(info, "minPing"));
            server->maxPing = atoi(Info_ValueForKey(info, "maxPing"));
            server->g_humanplayers = atoi(Info_ValueForKey(info, "g_humanplayers"));
            server->g_needpass = atoi(Info_ValueForKey(info, "g_needpass"));
            server->vac = atoi(Info_ValueForKey(info, "sv_vac"));
            Q_strncpyz(server->description, Info_ValueForKey(info, "gamedesc"), sizeof(server->description));
            Q_strncpyz(server->tags, Info_ValueForKey(info, "tags"), sizeof(server->tags));
        }
        server->ping = ping;
    }
}

static void CL_SetServerInfoByAddress(netadr_t from, const char* info, int ping) {
    int i;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (NET_CompareAdr(from, cls.localServers[i].adr)) {
            CL_SetServerInfo(&cls.localServers[i], info, ping);
        }
    }

    for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
        if (NET_CompareAdr(from, cls.globalServers[i].adr)) {
            CL_SetServerInfo(&cls.globalServers[i], info, ping);
        }
    }

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (NET_CompareAdr(from, cls.favoriteServers[i].adr)) {
            CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
        }
    }
}

void CL_ServerInfoPacket(netadr_t from, msg_t* msg) {
    int i, type;
    char info[MAX_INFO_STRING];
    char* infoString;
    int prot;
    char* gamename;
    qboolean gameMismatch;

    infoString = MSG_ReadString(msg);

    gamename = Info_ValueForKey(infoString, "gamename");

    gameMismatch = !*gamename || strcmp(gamename, com_gamename->string) != 0;

    if (gameMismatch) {
        Com_DPrintf("Game mismatch in info packet: %s\n", infoString);
        return;
    }

    prot = atoi(Info_ValueForKey(infoString, "protocol"));

    if (prot != com_protocol->integer) {
        Com_DPrintf("Different protocol info packet: %s\n", infoString);
        return;
    }

    for (i = 0; i < MAX_PINGREQUESTS; i++) {
        if (cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr(from, cl_pinglist[i].adr)) {
            cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
            Com_DPrintf("ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToString(from));

            Q_strncpyz(cl_pinglist[i].info, infoString, sizeof(cl_pinglist[i].info));

            switch (from.type) {
                case NA_BROADCAST:
                case NA_IP:
                    type = 1;
                    break;
                case NA_IP6:
                    type = 2;
                    break;
                default:
                    type = 0;
                    break;
            }
            Info_SetValueForKey(cl_pinglist[i].info, "nettype", va("%d", type));
            CL_SetServerInfoByAddress(from, infoString, cl_pinglist[i].time);

            return;
        }
    }

    if (cls.pingUpdateSource != AS_LOCAL) {
        return;
    }

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (cls.localServers[i].adr.port == 0) {
            break;
        }

        if (NET_CompareAdr(from, cls.localServers[i].adr)) {
            return;
        }
    }

    if (i == MAX_OTHER_SERVERS) {
        Com_DPrintf("MAX_OTHER_SERVERS hit, dropping infoResponse\n");
        return;
    }

    cls.numlocalservers = i + 1;
    CL_InitServerInfo(&cls.localServers[i], &from);

    Q_strncpyz(info, MSG_ReadString(msg), MAX_INFO_STRING);
    if (strlen(info)) {
        if (info[strlen(info) - 1] != '\n') {
            Q_strcat(info, sizeof(info), "\n");
        }
        Com_Printf("%s: %s", NET_AdrToStringwPort(from), info);
    }
}

serverStatus_t* CL_GetServerStatus(netadr_t from) {
    int i, oldest, oldestTime;

    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (NET_CompareAdr(from, cl_serverStatusList[i].address)) {
            return &cl_serverStatusList[i];
        }
    }
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (cl_serverStatusList[i].retrieved) {
            return &cl_serverStatusList[i];
        }
    }
    oldest = -1;
    oldestTime = 0;
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (oldest == -1 || cl_serverStatusList[i].startTime < oldestTime) {
            oldest = i;
            oldestTime = cl_serverStatusList[i].startTime;
        }
    }
    return &cl_serverStatusList[oldest];
}

int CL_ServerStatus(char* serverAddress, char* serverStatusString, int maxLen) {
    int i;
    netadr_t to;
    serverStatus_t* serverStatus;

    if (!serverAddress) {
        for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
            cl_serverStatusList[i].address.port = 0;
            cl_serverStatusList[i].retrieved = qtrue;
        }
        return qfalse;
    }
    if (!NET_StringToAdr(serverAddress, &to, NA_UNSPEC)) {
        return qfalse;
    }
    serverStatus = CL_GetServerStatus(to);
    if (!serverStatusString) {
        serverStatus->retrieved = qtrue;
        return qfalse;
    }

    if (NET_CompareAdr(to, serverStatus->address)) {
        if (!serverStatus->pending) {
            Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
            serverStatus->retrieved = qtrue;
            serverStatus->startTime = 0;
            return qtrue;
        }
        else if (serverStatus->startTime < Com_Milliseconds() - cl_serverStatusResendTime->integer) {
            serverStatus->print = qfalse;
            serverStatus->pending = qtrue;
            serverStatus->retrieved = qfalse;
            serverStatus->time = 0;
            serverStatus->startTime = Com_Milliseconds();
            NET_OutOfBandPrint(NS_CLIENT, to, "getstatus");
            return qfalse;
        }
    }
    else if (serverStatus->retrieved) {
        serverStatus->address = to;
        serverStatus->print = qfalse;
        serverStatus->pending = qtrue;
        serverStatus->retrieved = qfalse;
        serverStatus->startTime = Com_Milliseconds();
        serverStatus->time = 0;
        NET_OutOfBandPrint(NS_CLIENT, to, "getstatus");
        return qfalse;
    }
    return qfalse;
}

void CL_ServerStatusResponse(netadr_t from, msg_t* msg) {
    char* s;
    char info[MAX_INFO_STRING];
    int i, l, score, ping;
    int len;
    serverStatus_t* serverStatus;

    serverStatus = NULL;
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (NET_CompareAdr(from, cl_serverStatusList[i].address)) {
            serverStatus = &cl_serverStatusList[i];
            break;
        }
    }
    if (!serverStatus) {
        return;
    }

    s = MSG_ReadStringLine(msg);

    len = 0;
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string) - len, "%s", s);

    if (serverStatus->print) {
        Com_Printf("Server settings:\n");
        while (*s) {
            for (i = 0; i < 2 && *s; i++) {
                if (*s == '\\')
                    s++;
                l = 0;
                while (*s) {
                    info[l++] = *s;
                    if (l >= MAX_INFO_STRING - 1)
                        break;
                    s++;
                    if (*s == '\\') {
                        break;
                    }
                }
                info[l] = '\0';
                if (i) {
                    Com_Printf("%s\n", info);
                } else {
                    Com_Printf("%-24s", info);
                }
            }
        }
    }

    len = strlen(serverStatus->string);
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string) - len, "\\");

    if (serverStatus->print) {
        Com_Printf("\nPlayers:\n");
        Com_Printf("num: score: ping: name:\n");
    }
    for (i = 0, s = MSG_ReadStringLine(msg); *s; s = MSG_ReadStringLine(msg), i++) {
        len = strlen(serverStatus->string);
        Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string) - len, "\\%s", s);

        if (serverStatus->print) {
            score = ping = 0;
            sscanf(s, "%d %d", &score, &ping);
            s = strchr(s, ' ');
            if (s)
                s = strchr(s + 1, ' ');
            if (s)
                s++;
            else
                s = "unknown";
            Com_Printf("%-2d   %-3d    %-3d   %s\n", i, score, ping, s);
        }
    }
    len = strlen(serverStatus->string);
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string) - len, "\\");

    serverStatus->time = Com_Milliseconds();
    serverStatus->address = from;
    serverStatus->pending = qfalse;
    if (serverStatus->print) {
        serverStatus->retrieved = qtrue;
    }
}

void CL_GlobalServers_f(void) {
    int source;
    int i;

    if (Cmd_Argc() < 2) {
        Com_Printf("usage: globalservers <0-4> [protocol] [keywords]\n");
        return;
    }

    source = atoi(Cmd_Argv(1));
    if (source < 0 || source > 4) {
        source = 0;
    }

    for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
        qboolean visible = cls.globalServers[i].visible;

        Com_Memset(&cls.globalServers[i], 0, sizeof(cls.globalServers[i]));
        cls.globalServers[i].visible = visible;
    }
    cls.numglobalservers = 0;
    cls.numGlobalServerAddresses = 0;
    cls.pingUpdateSource = AS_GLOBAL;
    cl_steamServersTaken = 0;
    cl_steamServersPending = qtrue;

    if (!Steam_RequestServerList(source)) {
        Com_Printf("No server list: Steam is not available.\n");
        return;
    }
    Com_Printf("Asking Steam for the server list...\n");
}

#define CL_VOICE_MAX_COMPRESSED 0x4000
#define CL_VOICE_MAX_PCM 0x8000

#define CL_MAX_IGNORED 64

static uint64_t cl_ignored[CL_MAX_IGNORED];
static int cl_numIgnored;

qboolean CL_VoiceIsIgnored(uint64_t steamId) {
    int i;

    for (i = 0; i < cl_numIgnored; i++) {
        if (cl_ignored[i] == steamId) {
            return qtrue;
        }
    }
    return qfalse;
}

qboolean CL_VoiceToggleIgnore(uint64_t steamId) {
    int i;

    if (!steamId) {
        return qfalse;
    }
    for (i = 0; i < cl_numIgnored; i++) {
        if (cl_ignored[i] == steamId) {
            cl_ignored[i] = cl_ignored[--cl_numIgnored];
            return qfalse;
        }
    }
    if (cl_numIgnored < CL_MAX_IGNORED) {
        cl_ignored[cl_numIgnored++] = steamId;
    }
    return qtrue;
}

static qboolean CL_VoiceIgnored(int clientNum) {
    uint64_t steamId = 0;

    if (!cgvm) {
        return qfalse;
    }
    VM_Call(cgvm, CG_CLIENT_STEAM_ID, clientNum, (intptr_t)&steamId);
    return CL_VoiceIsIgnored(steamId);
}

static void CL_VoiceTalking(int clientNum, qboolean talking) {
    if (cgvm) {
        VM_Call(cgvm, CG_SET_CLIENT_TALKING, clientNum, talking);
    }
}

static uint64_t CL_ServerSteamID(void) {
    const char* s;

    if (!cl.gameState.stringOffsets[CS_STEAM_ID]) {
        return 0;
    }
    s = cl.gameState.stringData + cl.gameState.stringOffsets[CS_STEAM_ID];
#ifdef _MSC_VER
    return (uint64_t)strtoull(s, NULL, 10);
#else
    return (uint64_t)strtoull(s, NULL, 10);
#endif
}

static qboolean CL_FriendByNumber(int number, steamFriend_t* out) {
    int total = Steam_FriendCount();
    int seen = 0;
    int i;

    for (i = 0; i < total; i++) {
        if (!Steam_FriendGet(i, out) || out->state == 0) {
            continue;
        }
        if (++seen == number) {
            return qtrue;
        }
    }
    return qfalse;
}

static void CL_FriendsList(void) {
    steamFriend_t fr;
    int total, shown = 0;
    int i;

    total = Steam_FriendCount();
    if (total <= 0) {
        Com_Printf("Steam returned no friends list.\n");
        return;
    }

    Com_Printf("^7%3s %-28s %-16s %s\n", "", "friend", "state", "where");
    for (i = 0; i < total; i++) {
        char where[64];

        if (!Steam_FriendGet(i, &fr) || fr.state == 0) {
            continue;
        }

        where[0] = '\0';
        if (fr.sameGame && fr.ip) {
            Com_sprintf(where, sizeof(where), "^2%u.%u.%u.%u:%u",
                        (fr.ip >> 24) & 0xff, (fr.ip >> 16) & 0xff,
                        (fr.ip >> 8) & 0xff, fr.ip & 0xff, fr.port);
        } else if (fr.sameGame) {
            Q_strncpyz(where, "^2in Quake Live", sizeof(where));
        } else if (fr.playing) {
            Q_strncpyz(where, "^3in another game", sizeof(where));
        }

        shown++;
        Com_Printf("%3i %-28s %-16s %s^7\n", shown, fr.name,
                   Steam_FriendStateName(fr.state), where);
    }
    Com_Printf("%i of %i online. ^7friends join|invite|profile <number>\n", shown, total);
}

void CL_FriendDo(const steamFriend_t* fr, int action) {
    switch (action) {
        case UI_FRIEND_JOIN:
            if (!fr->sameGame || !fr->ip) {
                Com_Printf("%s is not on a Quake Live server.\n", fr->name);
                return;
            }
            Com_Printf("Joining %s's server...\n", fr->name);
            Cbuf_AddText(va("connect %u.%u.%u.%u:%u\n",
                            (fr->ip >> 24) & 0xff, (fr->ip >> 16) & 0xff,
                            (fr->ip >> 8) & 0xff, fr->ip & 0xff, fr->port));
            return;
        case UI_FRIEND_INVITE: {
            char connect[128];

            if (clc.state < CA_CONNECTED) {
                Com_Printf("You are not in a match to invite %s to.\n", fr->name);
                return;
            }
            if (clc.serverAddress.type == NA_LOOPBACK) {
                unsigned int ip = Steam_GS_PublicIP();

                Com_sprintf(connect, sizeof(connect), "+connect %u.%u.%u.%u:%s", (ip >> 24) & 0xff,
                            (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, Cvar_VariableString("net_port"));
            } else {
                Com_sprintf(connect, sizeof(connect), "+connect %s", NET_AdrToString(clc.serverAddress));
            }
            Steam_InviteToGame(fr->steamId, connect);
            return;
        }
        case UI_FRIEND_PROFILE:
            Steam_ShowProfile(fr->steamId);
            return;
        case UI_FRIEND_MESSAGE:
            Steam_ShowChat(fr->steamId);
            return;
        case UI_FRIEND_ACHIEVEMENTS:
            Steam_ShowAchievements(fr->steamId);
            return;
        default:
            return;
    }
}

static void CL_Friends_f(void) {
    steamFriend_t fr;
    const char* what;

    if (Cmd_Argc() < 2) {
        CL_FriendsList();
        return;
    }

    what = Cmd_Argv(1);
    if (Cmd_Argc() != 3) {
        Com_Printf("usage: friends %s <number from the list>\n", what);
        return;
    }
    if (!CL_FriendByNumber(atoi(Cmd_Argv(2)), &fr)) {
        Com_Printf("No friend with that number.\n");
        return;
    }

    if (!Q_stricmp(what, "join")) {
        CL_FriendDo(&fr, UI_FRIEND_JOIN);
        return;
    }
    if (!Q_stricmp(what, "invite")) {
        CL_FriendDo(&fr, UI_FRIEND_INVITE);
        return;
    }
    if (!Q_stricmp(what, "profile")) {
        CL_FriendDo(&fr, UI_FRIEND_PROFILE);
        return;
    }
    Com_Printf("usage: friends [join|invite|profile <number>]\n");
}

static void CL_OverlayOnPlayer_f(const char* command, void (*action)(uint64_t)) {
    uint64_t steamId = 0;
    int clientNum;

    if (Cmd_Argc() != 2) {
        Com_Printf("usage: %s <client number>\n", command);
        return;
    }
    if (clc.state != CA_ACTIVE || !cgvm) {
        Com_Printf("Not connected to a server.\n");
        return;
    }
    clientNum = atoi(Cmd_Argv(1));
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    VM_Call(cgvm, CG_CLIENT_STEAM_ID, clientNum, (intptr_t)&steamId);
    if (!steamId) {
        return;
    }
    action(steamId);
}

static void CL_ClientFriendInvite_f(void) {
    CL_OverlayOnPlayer_f("clientfriendinvite", Steam_ShowFriendAdd);
}

static void CL_ClientViewProfile_f(void) {
    CL_OverlayOnPlayer_f("clientviewprofile", Steam_ShowProfile);
}

static void CL_VoiceOn_f(void) {
    Steam_VoiceStart();
    if (Steam_VoiceRecording()) {
        CL_VoiceTalking(clc.clientNum, qtrue);
    }
}

static void CL_VoiceOff_f(void) {
    qboolean wasOn = Steam_VoiceRecording();

    Steam_VoiceStop();
    if (wasOn) {
        CL_VoiceTalking(clc.clientNum, qfalse);
    }
}

void CL_VoiceFrame(void) {
    static byte compressed[CL_VOICE_MAX_COMPRESSED];
    static byte pcm[CL_VOICE_MAX_PCM];
    static int lastHello;
    uint64_t server = 0;
    uint64_t from;
    int bytes;

    if (!Steam_VoiceReady()) {
        return;
    }

    if (clc.state == CA_ACTIVE) {
        server = CL_ServerSteamID();
    }

    if (Steam_VoiceRecording()) {
        bytes = Steam_VoiceCapture(compressed, sizeof(compressed));
        if (bytes > 0 && server) {
            Steam_P2PSend(server, compressed, bytes, qfalse);
        }
    }

    if (server && (cls.realtime - lastHello > 10000 || cls.realtime < lastHello)) {
        lastHello = cls.realtime;
        Steam_P2PHello(server, qfalse);
    }

    while ((bytes = Steam_P2PReceive(compressed, sizeof(compressed), &from, qfalse)) > 1) {
        int who = compressed[0];
        int out;

        out = Steam_VoiceDecompress(compressed + 1, bytes - 1, pcm, sizeof(pcm));
        if (out <= 0) {
            Com_DPrintf("%d compressed voice bytes, decompressed to 0\n", bytes - 1);
            continue;
        }
        if (who < 0 || who >= MAX_CLIENTS) {
            continue;
        }
        if (CL_VoiceIgnored(who)) {
            continue;
        }
        CL_VoiceTalking(who, qtrue);
        S_RawSamples(who + 1, out / 2, Steam_VoiceRate(), 2, 1, pcm, s_voiceVolume->value, -1);
    }
}

void CL_SteamServersFrame(void) {
    int count;
    int i;
    int j;

    count = Steam_ServerListCount();
    while (cl_steamServersTaken < count && cls.numglobalservers < MAX_GLOBAL_SERVERS) {
        unsigned int ip = 0;
        unsigned short port = 0;
        int ping = 0;
        char info[MAX_INFO_STRING];
        netadr_t adr;
        serverInfo_t* server;

        i = cl_steamServersTaken++;
        if (!Steam_ServerListGet(i, &ip, &port, &ping, info, sizeof(info))) {
            continue;
        }

        Com_Memset(&adr, 0, sizeof(adr));
        adr.type = NA_IP;
        adr.ip[0] = (byte)(ip >> 24);
        adr.ip[1] = (byte)(ip >> 16);
        adr.ip[2] = (byte)(ip >> 8);
        adr.ip[3] = (byte)ip;
        adr.port = BigShort(port);

        for (j = 0; j < cls.numglobalservers; j++) {
            if (NET_CompareAdr(cls.globalServers[j].adr, adr)) {
                break;
            }
        }
        if (j < cls.numglobalservers) {
            CL_SetServerInfo(&cls.globalServers[j], info, ping);
            Q_strncpyz(cls.globalServers[j].hostName, Steam_ServerListName(i), sizeof(cls.globalServers[j].hostName));
            cl_steamServerSlot[i] = j;
            continue;
        }

        cl_steamServerSlot[i] = cls.numglobalservers;
        server = &cls.globalServers[cls.numglobalservers++];
        CL_InitServerInfo(server, &adr);
        CL_SetServerInfo(server, info, ping);
        Q_strncpyz(server->hostName, Steam_ServerListName(i), sizeof(server->hostName));
    }

    while ((i = Steam_ServerListNextUpdated()) >= 0) {
        unsigned int ip = 0;
        unsigned short port = 0;
        int ping = 0;
        char info[MAX_INFO_STRING];

        if (i >= cl_steamServersTaken || i >= MAX_GLOBAL_SERVERS) {
            continue;
        }
        if (Steam_ServerListGet(i, &ip, &port, &ping, info, sizeof(info))) {
            CL_SetServerInfo(&cls.globalServers[cl_steamServerSlot[i]], info, ping);
            Q_strncpyz(cls.globalServers[cl_steamServerSlot[i]].hostName, Steam_ServerListName(i),
                       sizeof(cls.globalServers[0].hostName));
        }
    }

    if (cl_steamServersPending && !Steam_ServerListRefreshing()) {
        cl_steamServersPending = qfalse;
        Com_Printf("%d servers parsed\n", cls.numglobalservers);
    }
}

void CL_LocalServers_f(void) {
    char* message;
    int i, j;
    netadr_t to;

    Com_Printf("Scanning for servers on the local network...\n");

    cls.numlocalservers = 0;
    cls.pingUpdateSource = AS_LOCAL;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        qboolean b = cls.localServers[i].visible;
        Com_Memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
        cls.localServers[i].visible = b;
    }
    Com_Memset(&to, 0, sizeof(to));

    message = "\377\377\377\377getinfo xxx";

    for (i = 0; i < 2; i++) {
        for (j = 0; j < NUM_SERVER_PORTS; j++) {
            to.port = BigShort((short)(PORT_SERVER + j));

            to.type = NA_BROADCAST;
            NET_SendPacket(NS_CLIENT, strlen(message), message, to);
            to.type = NA_MULTICAST6;
            NET_SendPacket(NS_CLIENT, strlen(message), message, to);
        }
    }
}

void CL_GetPing(int n, char* buf, int buflen, int* pingtime) {
    const char* str;
    int time;
    int maxPing;

    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
        buf[0] = '\0';
        *pingtime = 0;
        return;
    }

    str = NET_AdrToStringwPort(cl_pinglist[n].adr);
    Q_strncpyz(buf, str, buflen);

    time = cl_pinglist[n].time;
    if (!time) {
        time = Sys_Milliseconds() - cl_pinglist[n].start;
        maxPing = Cvar_VariableIntegerValue("cl_maxPing");
        if (maxPing < 100) {
            maxPing = 100;
        }
        if (time < maxPing) {
            time = 0;
        }
    }

    CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

    *pingtime = time;
}

void CL_GetPingInfo(int n, char* buf, int buflen) {
    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
        if (buflen)
            buf[0] = '\0';
        return;
    }

    Q_strncpyz(buf, cl_pinglist[n].info, buflen);
}

void CL_ClearPing(int n) {
    if (n < 0 || n >= MAX_PINGREQUESTS)
        return;

    cl_pinglist[n].adr.port = 0;
}

int CL_GetPingQueueCount(void) {
    int i;
    int count;
    ping_t* pingptr;

    count = 0;
    pingptr = cl_pinglist;

    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        if (pingptr->adr.port) {
            count++;
        }
    }

    return (count);
}

ping_t* CL_GetFreePing(void) {
    ping_t* pingptr;
    ping_t* best;
    int oldest;
    int i;
    int time;

    pingptr = cl_pinglist;
    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        if (pingptr->adr.port) {
            if (!pingptr->time) {
                if (Sys_Milliseconds() - pingptr->start < 500) {
                    continue;
                }
            } else if (pingptr->time < 500) {
                continue;
            }
        }

        pingptr->adr.port = 0;
        return (pingptr);
    }

    pingptr = cl_pinglist;
    best = cl_pinglist;
    oldest = INT_MIN;
    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        time = Sys_Milliseconds() - pingptr->start;
        if (time > oldest) {
            oldest = time;
            best = pingptr;
        }
    }

    return (best);
}

qboolean CL_UpdateVisiblePings_f(int source) {
    int slots, i;
    char buff[MAX_STRING_CHARS];
    int pingTime;
    int max;
    qboolean status = qfalse;

    if (source < 0 || source > AS_FAVORITES) {
        return qfalse;
    }

    cls.pingUpdateSource = source;

    slots = CL_GetPingQueueCount();
    if (slots < MAX_PINGREQUESTS) {
        serverInfo_t* server = NULL;

        switch (source) {
            case AS_LOCAL:
                server = &cls.localServers[0];
                max = cls.numlocalservers;
                break;
            case AS_GLOBAL:
                server = &cls.globalServers[0];
                max = cls.numglobalservers;
                break;
            case AS_FAVORITES:
                server = &cls.favoriteServers[0];
                max = cls.numfavoriteservers;
                break;
            default:
                return qfalse;
        }
        for (i = 0; i < max; i++) {
            if (server[i].visible) {
                if (server[i].ping == -1) {
                    int j;

                    if (slots >= MAX_PINGREQUESTS) {
                        break;
                    }
                    for (j = 0; j < MAX_PINGREQUESTS; j++) {
                        if (!cl_pinglist[j].adr.port) {
                            continue;
                        }
                        if (NET_CompareAdr(cl_pinglist[j].adr, server[i].adr)) {
                            break;
                        }
                    }
                    if (j >= MAX_PINGREQUESTS) {
                        status = qtrue;
                        for (j = 0; j < MAX_PINGREQUESTS; j++) {
                            if (!cl_pinglist[j].adr.port) {
                                break;
                            }
                        }
                        memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
                        cl_pinglist[j].start = Sys_Milliseconds();
                        cl_pinglist[j].time = 0;
                        NET_OutOfBandPrint(NS_CLIENT, cl_pinglist[j].adr, "getinfo xxx");
                        slots++;
                    }
                }
                else if (server[i].ping == 0) {
                    if (source == AS_GLOBAL) {
                        if (cls.numGlobalServerAddresses > 0) {
                            cls.numGlobalServerAddresses--;
                            CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
                        }
                    }
                }
            }
        }
    }

    if (slots) {
        status = qtrue;
    }
    for (i = 0; i < MAX_PINGREQUESTS; i++) {
        if (!cl_pinglist[i].adr.port) {
            continue;
        }
        CL_GetPing(i, buff, MAX_STRING_CHARS, &pingTime);
        if (pingTime != 0) {
            CL_ClearPing(i);
            status = qtrue;
        }
    }

    return status;
}

void CL_ServerStatus_f(void) {
    netadr_t to, *toptr = NULL;
    char* server;
    serverStatus_t* serverStatus;
    int argc;
    netadrtype_t family = NA_UNSPEC;

    argc = Cmd_Argc();

    if (argc != 2 && argc != 3) {
        if (clc.state != CA_ACTIVE || clc.demoplaying) {
            Com_Printf("Not connected to a server.\n");
            Com_Printf("usage: serverstatus [-4|-6] server\n");
            return;
        }

        toptr = &clc.serverAddress;
    }

    if (!toptr) {
        Com_Memset(&to, 0, sizeof(netadr_t));

        if (argc == 2)
            server = Cmd_Argv(1);
        else {
            if (!strcmp(Cmd_Argv(1), "-4"))
                family = NA_IP;
            else if (!strcmp(Cmd_Argv(1), "-6"))
                family = NA_IP6;
            else
                Com_Printf("warning: only -4 or -6 as address type understood.\n");

            server = Cmd_Argv(2);
        }

        toptr = &to;
        if (!NET_StringToAdr(server, toptr, family))
            return;
    }

    NET_OutOfBandPrint(NS_CLIENT, *toptr, "getstatus");

    serverStatus = CL_GetServerStatus(*toptr);
    serverStatus->address = *toptr;
    serverStatus->print = qtrue;
    serverStatus->pending = qtrue;
}

void CL_ShowIP_f(void) {
    Sys_ShowIP();
}
