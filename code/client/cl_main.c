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
// cl_main.c  -- client main loop

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

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
vm_t* cgvm;

char cl_reconnectArgs[MAX_OSPATH];
char cl_oldGame[MAX_QPATH];
qboolean cl_oldGameSet;

// Structure containing functions exported from refresh DLL
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

// [QL] the Workshop items the match asks for and does not have yet
#define CL_MAX_WORKSHOP 32
static uint64_t cl_workshopItems[CL_MAX_WORKSHOP];
static int cl_workshopCount;
static int cl_workshopIndex;
static void CL_WorkshopNext(void);

// how many servers from Steam's list have been copied into the interface's
static int cl_steamServersTaken;
static qboolean cl_steamServersPending;

extern void SV_BotFrame(int time);
void CL_CheckForResend(void);
void CL_ShowIP_f(void);
void CL_PostProcessRestart_f(void);
void CL_Userinfo_f(void);
void CL_ServerStatusResponse(netadr_t from, msg_t* msg);

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand(const char* cmd, qboolean isDisconnectCmd) {
    int unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

    // if we would be losing an old command that hasn't been acknowledged,
    // we must drop the connection
    // also leave one slot open for the disconnect command in this case.

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

/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/

void CL_WriteDemoMessage(msg_t* msg, int headerBytes) {
    int len, swlen;

    // write the packet sequence
    len = clc.serverMessageSequence;
    swlen = LittleLong(len);
    FS_Write(&swlen, 4, clc.demofile);
    // skip the packet sequencing information
    len = msg->cursize - headerBytes;
    swlen = LittleLong(len);
    FS_Write(&swlen, 4, clc.demofile);
    FS_Write(msg->data + headerBytes, len, clc.demofile);
}

/*
====================
CL_StopRecording_f

stop recording a demo
====================
*/
void CL_StopRecord_f(void) {
    int len;

    if (!clc.demorecording) {
        Com_Printf("Not recording a demo.\n");
        return;
    }

    // finish up
    len = -1;
    FS_Write(&len, 4, clc.demofile);
    FS_Write(&len, 4, clc.demofile);
    FS_FCloseFile(clc.demofile);
    clc.demofile = 0;
    clc.demorecording = qfalse;
    Com_Printf("Stopped demo.\n");
}

/*
==================
CL_DemoFilename
==================
*/
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

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
static char demoName[MAX_QPATH];  // compiler bug workaround
void CL_Record_f(void) {
    char name[MAX_OSPATH];
    byte bufData[MAX_MSGLEN];
    msg_t buf;
    int i;
    int len;
    entityState_t* ent;
    entityState_t nullstate;
    char* s;

    if (Cmd_Argc() > 2) {
        Com_Printf("record <demoname>\n");
        return;
    }

    if (clc.demorecording) {
        Com_Printf("Already recording.\n");
        return;
    }

    if (clc.state != CA_ACTIVE) {
        Com_Printf("You must be in a level to record.\n");
        return;
    }

    // sync 0 doesn't prevent recording, so not forcing it off .. everyone does g_sync 1 ; record ; g_sync 0 ..
    if (NET_IsLocalAddress(clc.serverAddress) && !Cvar_VariableValue("g_synchronousClients")) {
        Com_Printf(S_COLOR_YELLOW "WARNING: You should set 'g_synchronousClients 1' for smoother demo recording\n");
    }

    if (Cmd_Argc() == 2) {
        s = Cmd_Argv(1);
        Q_strncpyz(demoName, s, sizeof(demoName));
        Com_sprintf(name, sizeof(name), "demos/%s.%s%d", demoName, DEMOEXT, com_protocol->integer);
    } else {
        int number;

        // scan for a free demo name
        for (number = 0; number <= 9999; number++) {
            CL_DemoFilename(number, demoName, sizeof(demoName));
            Com_sprintf(name, sizeof(name), "demos/%s.%s%d", demoName, DEMOEXT, com_protocol->integer);

            if (!FS_FileExists(name))
                break;  // file doesn't exist
        }
    }

    // open the demo file

    Com_Printf("recording to %s.\n", name);
    clc.demofile = FS_FOpenFileWrite(name);
    if (!clc.demofile) {
        Com_Printf("ERROR: couldn't open.\n");
        return;
    }
    clc.demorecording = qtrue;

    Q_strncpyz(clc.demoName, demoName, sizeof(clc.demoName));

    // don't start saving messages until a non-delta compressed message is received
    clc.demowaiting = qtrue;

    // write out the gamestate message
    MSG_Init(&buf, bufData, sizeof(bufData));
    MSG_Bitstream(&buf);

    // NOTE, MRE: all server->client messages now acknowledge
    MSG_WriteLong(&buf, clc.reliableSequence);

    MSG_WriteByte(&buf, svc_gamestate);
    MSG_WriteLong(&buf, clc.serverCommandSequence);

    // configstrings
    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (!cl.gameState.stringOffsets[i]) {
            continue;
        }
        s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
        MSG_WriteByte(&buf, svc_configstring);
        MSG_WriteShort(&buf, i);
        MSG_WriteBigString(&buf, s);
    }

    // baselines
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

    // finished writing the gamestate stuff

    // write the client num
    MSG_WriteLong(&buf, clc.clientNum);
    // write the checksum feed
    MSG_WriteLong(&buf, clc.checksumFeed);

    // finished writing the client packet
    MSG_WriteByte(&buf, svc_EOF);

    // write it to the demo file
    len = LittleLong(clc.serverMessageSequence - 1);
    FS_Write(&len, 4, clc.demofile);

    len = LittleLong(buf.cursize);
    FS_Write(&len, 4, clc.demofile);
    FS_Write(buf.data, buf.cursize, clc.demofile);

    // the rest of the demo file will be copied from net messages
}

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

/*
=================
CL_DemoFrameDurationSDev
=================
*/
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

/*
=================
CL_DemoCompleted
=================
*/
void CL_DemoCompleted(void) {
    char buffer[MAX_STRING_CHARS];

    // [QL] cl_quitOnDemoCompleted: quit when the demo ends, so a demo can be played back
    // unattended and the process closes itself
    if (Cvar_VariableIntegerValue("cl_quitOnDemoCompleted")) {
        Com_Quit_f();
        return;
    }

    if (cl_timedemo && cl_timedemo->integer) {
        int time;

        time = Sys_Milliseconds() - clc.timeDemoStart;
        if (time > 0) {
            // Millisecond times are frame durations:
            // minimum/average/maximum/std deviation
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

            // Write a log of all the frame durations
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

/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage(void) {
    int r;
    msg_t buf;
    byte bufData[MAX_MSGLEN];
    int s;

    if (!clc.demofile) {
        CL_DemoCompleted();
        return;
    }

    // get the sequence number
    r = FS_Read(&s, 4, clc.demofile);
    if (r != 4) {
        CL_DemoCompleted();
        return;
    }
    clc.serverMessageSequence = LittleLong(s);

    // init the message
    MSG_Init(&buf, bufData, sizeof(bufData));

    // get the length
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

/*
====================
CL_WalkDemoExt
====================
*/
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

/*
====================
CL_CompleteDemoName
====================
*/
static void CL_CompleteDemoName(char* args, int argNum) {
    if (argNum == 2) {
        char demoExt[16];

        Com_sprintf(demoExt, sizeof(demoExt), ".%s%d", DEMOEXT, com_protocol->integer);
        Field_CompleteFilename("demos", demoExt, qtrue, qtrue);
    }
}

/*
==================
CL_CompleteCallvote

"callvote <what> [...]". The names are the ones Cmd_CallVote_f accepts
(0x10061e40); on "map" the map and the factory complete as they do for the "map"
command. Retail hands tab completion the whole map list and leaves it there
(0x004b6a60); here it goes argument by argument, like everything else in this
engine.
==================
*/
static void CL_CompleteCallvote(char* args, int argNum) {
    static const char* voteNames[] = {
        "ammo", "clientkick", "cointoss", "fraglimit", "g_gametype", "kick", "loadouts",
        "map", "map_restart", "nextmap", "random", "shuffle", "teamsize", "timelimit",
        "timers", "weaprespawn"
    };
    static const char* names[MAX_COMPLETION_NAMES];
    int count;

    if (argNum == 2) {
        Field_CompleteList(voteNames, ARRAY_LEN(voteNames));
        return;
    }

    if (Q_stricmp(Cmd_Argv(1), "map")) {
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

static void CL_CompletePlayerName(char* args, int argNum);

/*
==================
CL_SetCgameCommandCompletion

The cgame registers its commands when it starts (trap_AddCommand), so that is where tab
completion has to be hung on them. The ones that take a player's name take it from the
configstrings, which is all the client has.
==================
*/
void CL_SetCgameCommandCompletion(const char* cmdName) {
    static const char* byPlayer[] = {
        "addadmin", "addmod", "addscore", "ban", "demote", "follow", "mute", "put", "tell", "unmute"
    };
    int i;

    if (!Q_stricmp(cmdName, "callvote")) {
        Cmd_SetCommandCompletionFunc(cmdName, CL_CompleteCallvote);
        return;
    }
    for (i = 0; i < ARRAY_LEN(byPlayer); i++) {
        if (!Q_stricmp(cmdName, byPlayer[i])) {
            Cmd_SetCommandCompletionFunc(cmdName, CL_CompletePlayerName);
            return;
        }
    }
}

/*
===============================================================================

[ioquakelive] DEMO SEEKING

A demo is a chain of deltas, so going back means playing it again from the start and
running through it undrawn up to the time asked for, the way WolfcamQL does. Going
forward only moves the clock, and CL_SetCGameTime reads up to it.

A server that reloads the map halfway, as QuakeClub's do when the readyup starts the match,
sends a new gamestate and starts its clock over, so a demo can hold several stretches of
server time. Demo time runs on across them: each stretch starts where the last one ended.

===============================================================================
*/

#define MAX_DEMO_STRETCHES	32

static char		demoPath[MAX_OSPATH];		// the file being played, for reopening it
static int		demoStretchStart[MAX_DEMO_STRETCHES];	// server time of its first snapshot
static int		demoStretchLength[MAX_DEMO_STRETCHES];
static int		demoStretchAt[MAX_DEMO_STRETCHES];		// demo time it begins at
static int		demoStretches;
static int		demoStretch;				// the one being played
static int		demoLastSnap = -1;			// server time of the last snapshot read
static int		demoSeekTarget = -1;		// server time to reach in demoSeekStretch, -1 for none
static int		demoSeekStretch;
static qboolean	demoSeekRestart;			// the next demo command is a seek going back

/*
====================
CL_DemoScan

one pass over the file before it plays: where each stretch of server time begins and how long
it runs, the times the match restarts, and the frags of the player being watched, which the
timeline marks and fragforward jumps to. The snapshots are read whole, as CL_ParseSnapshot reads
them, into a parse state of the scan's own that is thrown away at the end
====================
*/
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
	int				obituarySeen[MAX_GENTITIES];	// the last message whose snapshot carried it
} demoScan_t;

static void CL_DemoScanGamestate(msg_t *msg, demoScan_t *scan) {
	entityState_t	nullstate;
	int				cmd, num;

	Com_Memset(&nullstate, 0, sizeof(nullstate));
	Com_Memset(scan->baselines, 0, sizeof(scan->baselines));
	// the frames before a gamestate are no base for a delta after it
	Com_Memset(scan->frames, 0, sizeof(scan->frames));

	MSG_ReadLong(msg);	// server command sequence
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
			break;	// svc_EOF
		}
	}
	MSG_ReadLong(msg);	// client number
	MSG_ReadLong(msg);	// checksum feed
}

// one entity into the scan's ring, as CL_DeltaEntity puts it into cl.parseEntities
static void CL_DemoScanDelta(msg_t *msg, demoScan_t *scan, demoScanFrame_t *frame, int num,
	entityState_t *from, qboolean unchanged) {
	entityState_t *state = &scan->parseEntities[scan->parseEntitiesNum & (MAX_PARSE_ENTITIES - 1)];

	if (unchanged) {
		*state = *from;
	} else {
		MSG_ReadDeltaEntity(msg, from, state, num);
	}
	if (state->number == MAX_GENTITIES - 1) {
		return;	// removed
	}
	scan->parseEntitiesNum++;
	frame->numEntities++;
}

// the next entity of the frame being deltaed from, 99999 past its last
static int CL_DemoScanNextOld(demoScan_t *scan, demoScanFrame_t *old, int *index, entityState_t **state) {
	if (!old || ++*index >= old->numEntities) {
		return 99999;
	}
	*state = &scan->parseEntities[(old->parseEntitiesNum + *index) & (MAX_PARSE_ENTITIES - 1)];
	return (*state)->number;
}

// as CL_ParsePacketEntities; qfalse when the message runs out
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

// as CL_ParseSnapshot: the frame it read, or NULL when it is no good
static demoScanFrame_t *CL_DemoScanSnapshot(msg_t *msg, demoScan_t *scan, int seq, int *serverTime) {
	static demoScanFrame_t	frame;
	demoScanFrame_t			*old = NULL;
	byte					areamask[MAX_MAP_AREA_BYTES];
	int						delta, len;

	*serverTime = MSG_ReadLong(msg);
	delta = MSG_ReadByte(msg);
	MSG_ReadByte(msg);	// snapFlags

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
				break;	// -1 ends the demo; anything else is a damaged tail
			}
			MSG_Init(&buf, data, sizeof(data));
			buf.cursize = len;
			MSG_Bitstream(&buf);
			MSG_ReadLong(&buf);	// reliable acknowledge

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
					break;	// svc_EOF, or nothing the timeline needs
				}
				frame = CL_DemoScanSnapshot(&buf, scan, seq, &t);

				// a new stretch after a gamestate, or if the clock went back without one
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

				// a frag: an obituary new in this snapshot, with the watched player the killer.
				// The event entity lives on for a few snapshots and counts in the first
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

// demo time of server time t in the stretch being played
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

// demo time of what is on screen
static int CL_DemoPos(void) {
	return CL_DemoTimeAt(cl.serverTime);
}

/*
===============================================================================

REWIND

[ioquakelive] NOT FROM RETAIL, the way WolfcamQL does it. While a demo plays, the client is
copied every so often with its place in the file: once in each eighth of the demo, so any
point can be reached, and every ten seconds for the last eighty, so a short step back has
little to read. Going back restores the newest copy a second before the target and the demo
is read on from there, undrawn, in the same frame, while the cgame starts over from the
snapshot restored with everything it loaded. The demo is played again, with its loading
screen, only for a point before every copy or behind a gamestate.

Reading a whole demo takes some 40 milliseconds; what going back used to cost was the cgame
loading everything again.

===============================================================================
*/

#define DEMO_SPREAD_BACKUPS		8
#define DEMO_RECENT_BACKUPS		8
#define DEMO_BACKUPS			(DEMO_SPREAD_BACKUPS + DEMO_RECENT_BACKUPS)
#define DEMO_RECENT_SPACING		10000

typedef struct {
	qboolean			valid;
	int					pos;			// demo time of its snapshot
	int					filePos;		// the demo file right after it
	int					stretch;
	int					lastSnap;
	clientActive_t		cl;
	clientConnection_t	clc;
} demoBackup_t;

static demoBackup_t*	demoBackups;		// DEMO_BACKUPS of them, while a demo plays
static int				demoRecentNext;		// demo time the next recent copy is due
static int				demoRecentSlot;

/*
====================
CL_DemoForgetBackups

a new demo, this one again, or a gamestate halfway: none of the copies holds any more
====================
*/
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

/*
====================
CL_DemoFreeBackups

the demo is over; the copies are some 40 megabytes
====================
*/
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

/*
====================
CL_DemoBackup

after each snapshot of a demo being watched: the first in each eighth of the demo, and one
every DEMO_RECENT_SPACING into the ring of recent ones
====================
*/
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

/*
====================
CL_DemoGamestate

called by CL_ParseGamestate: one arriving while the demo already runs begins its next stretch
====================
*/
void CL_DemoGamestate(void) {
	if (clc.demoplaying && clc.state == CA_ACTIVE) {
		demoStretch++;
		demoLastSnap = -1;
		// [ioquakelive] the cgame starts over on it: going back past it plays the demo again
		CL_DemoForgetBackups();
	}
}

/*
====================
CL_DemoSnapshotRead

called after each demo message: the server's clock starting over without a gamestate begins
the next stretch too, as CL_DemoScan counts them
====================
*/
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

/*
====================
CL_DemoSuspend / CL_DemoResume

around a filesystem restart, which closes every open file: the demo is put down with its
place kept, and taken up again there
====================
*/
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

/*
====================
CL_DemoRewind

back to demo time pos from the newest copy of the client a second before it, which leaves
snapshots behind the target to draw it from, or failing that the newest at it; qfalse when
there is none
====================
*/
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

	// the file handle is the current one: a filesystem restart reopens the demo
	demofile = clc.demofile;
	memcpy(&cl, &b->cl, sizeof(cl));
	memcpy(&clc, &b->clc, sizeof(clc));
	clc.demofile = demofile;
	FS_Seek(clc.demofile, b->filePos, FS_SEEK_SET);
	demoStretch = b->stretch;
	demoLastSnap = b->lastSnap;
	demoRecentNext = b->pos + DEMO_RECENT_SPACING;

	// the clock goes back with it, and CL_DemoSeekApply takes it on to the target
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.serverTime = cl.oldServerTime = cl.oldFrameServerTime = cl.snap.serverTime;

	VM_Call(cgvm, CG_DEMO_REWIND, cl.snap.messageNum, clc.lastExecutedServerCommand);
	return qtrue;
}

/*
====================
CL_DemoSeek

jumps to demo time pos: ahead in the same stretch right away, anywhere else by playing the
demo again and running through it
====================
*/
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
	// still starting, or starting over: at the top of the file, where every target is ahead
	if (clc.state != CA_ACTIVE) {
		return;
	}
	if (n != demoStretch || demoSeekTarget < cl.serverTime) {
		// [ioquakelive] going back starts from a copy of the client when there is one
		if (pos < CL_DemoPos() && CL_DemoRewind(pos)) {
			return;
		}
		// inserted, not appended: whatever else is queued runs after the demo is back
		demoSeekRestart = qtrue;
		Cbuf_ExecuteText(EXEC_INSERT, va("demo \"%s\"\n", clc.demoName));
	}
}

/*
====================
CL_DemoSeekApply

called by CL_SetCGameTime before it reads the demo: moves the clock toward a pending seek,
and the demo is then read up to it in one go, undrawn. A seek into a later stretch runs off
the end of each one before it, into its gamestate, until it gets there.
====================
*/
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

// fragforward [seconds]: that long before the next frag of the player being watched, as WolfcamQL
static void CL_FragForward_f(void) {
	char		frags[MAX_STRING_CHARS];
	const char	*p;
	int			before = (int)(CL_SeekSeconds(3) * 1000);
	int			now = CL_DemoPos();
	int			t;

	Cvar_VariableStringBuffer("cl_demoFrags", frags, sizeof(frags));
	for (p = frags; *p; ) {
		t = atoi(p);
		// past the one it last jumped to
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

// seekdemo <seconds> or <m:ss>, from the start of the demo
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

/*
====================
CL_PlayDemo_f

demo <demoname>

====================
*/
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

    // make sure a local server is killed
    // 2 means don't force disconnect of local client
    Cvar_Set("sv_killserver", "2");

    // open the demo file
    Q_strncpyz(arg, Cmd_Argv(1), sizeof(arg));

    CL_Disconnect(qtrue);

    // check for an extension .DEMOEXT_?? (?? is protocol)
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

    // [ioquakelive] a seek going back keeps its target through the restart; any other demo
    // drops it
    seekRestart = demoSeekRestart;
    if (!demoSeekRestart) {
        demoSeekTarget = -1;
    }
    demoSeekRestart = qfalse;
    demoStretch = 0;
    demoLastSnap = -1;
    CL_DemoForgetBackups();
    // a seek going back plays the same file again: what the scan found still holds
    if (!seekRestart || Q_stricmp(demoPath, name)) {
        Q_strncpyz(demoPath, name, sizeof(demoPath));
        CL_DemoScan(name);
    }

    Con_Close();

    clc.state = CA_CONNECTED;
    clc.demoplaying = qtrue;
    Q_strncpyz(clc.servername, arg, sizeof(clc.servername));

    // read demo messages until connected
    while (clc.state >= CA_CONNECTED && clc.state < CA_PRIMED) {
        CL_ReadDemoMessage();
    }
    // don't get the first snapshot this frame, to prevent the long
    // time from the gamestate load from messing causing a time skip
    clc.firstDemoFrameSkipped = qfalse;
}

/*
====================
CL_StartDemoLoop

Closing the main menu will restart the demo loop
====================
*/
void CL_StartDemoLoop(void) {
    // start the demo loop again
    Cbuf_AddText("d1\n");
    Key_SetCatcher(0);
}

/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
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

//======================================================================

/*
=====================
CL_ShutdownAll
=====================
*/
void CL_ShutdownAll(qboolean shutdownRef) {
    if (CL_VideoRecording())
        CL_CloseAVI();

    if (clc.demorecording)
        CL_StopRecord_f();

#ifdef USE_CURL
    CL_cURL_Shutdown();
#endif
    // clear sounds
    S_DisableSounds();
    // shutdown CGame
    CL_ShutdownCGame();
    // shutdown UI
    CL_ShutdownUI();

    // shutdown the renderer
    if (shutdownRef)
        CL_ShutdownRef();
    else if (re.Shutdown)
        re.Shutdown(qfalse);  // don't destroy window or context

    cls.uiStarted = qfalse;
    cls.cgameStarted = qfalse;
    cls.rendererStarted = qfalse;
    cls.soundRegistered = qfalse;
}

/*
=================
CL_ClearMemory

Called by Com_GameRestart
=================
*/
void CL_ClearMemory(qboolean shutdownRef) {
    // shutdown all the client stuff
    CL_ShutdownAll(shutdownRef);

    // if not running a server clear the whole hunk
    if (!com_sv_running || !com_sv_running->integer) {
        // clear the whole hunk
        Hunk_Clear();
        // clear collision map data
        CM_ClearMap();
    } else {
        // clear all the client data on the hunk
        Hunk_ClearToMark();
    }
}

/*
=================
CL_FlushMemory

Called by CL_MapLoading, CL_Connect_f, CL_PlayDemo_f, and CL_ParseGamestate the only
ways a client gets into a game
Also called by Com_Error
=================
*/
void CL_FlushMemory(void) {
    CL_ClearMemory(qfalse);
    CL_StartHunkUsers(qfalse);
}

/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
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

    // if we are already connected to the local host, stay connected
    if (clc.state >= CA_CONNECTED && !Q_stricmp(clc.servername, "localhost")) {
        clc.state = CA_CONNECTED;  // so the connect screen is drawn
        Com_Memset(cls.updateInfoString, 0, sizeof(cls.updateInfoString));
        Com_Memset(clc.serverMessage, 0, sizeof(clc.serverMessage));
        Com_Memset(&cl.gameState, 0, sizeof(cl.gameState));
        clc.lastPacketSentTime = -9999;
        SCR_UpdateScreen();
    } else {
        // clear nextmap so the cinematic shutdown doesn't execute it
        Cvar_Set("nextmap", "");
        CL_Disconnect(qtrue);
        Q_strncpyz(clc.servername, "localhost", sizeof(clc.servername));
        clc.state = CA_CHALLENGING;  // so the connect screen is drawn
        Key_SetCatcher(0);
        SCR_UpdateScreen();
        clc.connectTime = -RETRANSMIT_TIMEOUT;
        NET_StringToAdr(clc.servername, &clc.serverAddress, NA_UNSPEC);
        // we don't need a challenge on the localhost

        CL_CheckForResend();
    }
}

/*
=====================
CL_ClearState

Called before parsing a gamestate
=====================
*/
void CL_ClearState(void) {
    //	S_StopAllSounds();

    Com_Memset(&cl, 0, sizeof(cl));
}

static void CL_OldGame(void) {
    if (cl_oldGameSet) {
        // change back to previous fs_game
        cl_oldGameSet = qfalse;
        Cvar_Set2("fs_game", cl_oldGame, qtrue);
        FS_ConditionalRestart(clc.checksumFeed, qfalse);
    }
}

/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect(qboolean showMainMenu) {
    if (!com_cl_running || !com_cl_running->integer) {
        return;
    }

    // shutting down the client so enter full screen ui mode
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
    // [ioquakelive] and the copies of the client kept for going back in it
    CL_DemoFreeBackups();

    if (uivm && showMainMenu) {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE);
    }

    SCR_StopCinematic();
    S_ClearSoundBuffer();

    // send a disconnect message to the server
    // send it a few times in case one is dropped
    if (clc.state >= CA_CONNECTED) {
        CL_AddReliableCommand("disconnect", qtrue);
        CL_WritePacket();
        CL_WritePacket();
        CL_WritePacket();
    }

    // Remove pure paks
    FS_PureServerSetLoadedPaks("", "");
    FS_PureServerSetReferencedPaks("", "");

    CL_ClearState();

    // wipe the client connection
    Com_Memset(&clc, 0, sizeof(clc));

    clc.state = CA_DISCONNECTED;

    // allow cheats locally
    Cvar_Set("sv_cheats", "1");

    // not connected to a pure server anymore
    cl_connectedToPureServer = qfalse;

    // Stop recording any video
    if (CL_VideoRecording()) {
        // Finish rendering current frame
        SCR_UpdateScreen();
        CL_CloseAVI();
    }

    if (!noGameRestart)
        CL_OldGame();
    else
        noGameRestart = qfalse;
}

/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer(const char* string) {
    char* cmd;

    cmd = Cmd_Argv(0);

    // ignore key up commands
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

/*
===================
CL_RequestMotd

===================
*/
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

/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f(void) {
    if (clc.state != CA_ACTIVE || clc.demoplaying) {
        Com_Printf("Not connected to a server.\n");
        return;
    }

    // don't forward the first argument
    if (Cmd_Argc() > 1) {
        CL_AddReliableCommand(Cmd_Args(), qfalse);
    }
}

/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f(void) {
    SCR_StopCinematic();
    if (clc.state != CA_DISCONNECTED && clc.state != CA_CINEMATIC) {
        Com_Error(ERR_DISCONNECT, "Disconnected from server");
    }
}

/*
================
CL_Reconnect_f

================
*/
void CL_Reconnect_f(void) {
    if (!strlen(cl_reconnectArgs))
        return;
    Cbuf_AddText(va("connect %s\n", cl_reconnectArgs));
}

/*
================
CL_Connect_f

================
*/
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

    // save arguments for reconnect
    Q_strncpyz(cl_reconnectArgs, Cmd_Args(), sizeof(cl_reconnectArgs));

    // fire a message off to the motd server
    CL_RequestMotd();

    // clear any previous "server full" type messages
    clc.serverMessage[0] = 0;

    if (com_sv_running->integer && !strcmp(server, "localhost")) {
        // if running a local server, kill it
        SV_Shutdown("Server quit");
    }

    // make sure a local server is killed
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

    // if we aren't playing on a lan, we need to request a challenge
    if (NET_IsLocalAddress(clc.serverAddress))
        clc.state = CA_CHALLENGING;
    else {
        clc.state = CA_CONNECTING;

        // Set a client challenge number that ideally is mirrored back by the server.
        clc.challenge = (((unsigned int)rand() << 16) ^ (unsigned int)rand()) ^ Com_Milliseconds();
    }

    Key_SetCatcher(0);
    clc.connectTime = -99999;  // CL_CheckForResend() will fire immediately
    clc.connectPacketCount = 0;

    // server connection string
    Cvar_Set("cl_currentServerAddress", server);
}

#define MAX_RCON_MESSAGE 1024

/*
==================
CL_CompleteRcon
==================
*/
static void CL_CompleteRcon(char* args, int argNum) {
    if (argNum == 2) {
        // Skip "rcon "
        char* p = Com_SkipTokens(args, 1, " ");

        if (p > args)
            Field_CompleteCommand(p, qtrue, qtrue);
    }
}

/*
==================
CL_CompletePlayerName
==================
*/
static void CL_CompletePlayerName(char* args, int argNum) {
    if (argNum == 2) {
        char names[MAX_CLIENTS][MAX_NAME_LENGTH];
        const char* namesPtr[MAX_CLIENTS];
        int i;
        int clientCount;
        int nameCount;
        const char* info;
        const char* name;

        // configstring
        info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
        clientCount = atoi(Info_ValueForKey(info, "sv_maxclients"));

        nameCount = 0;

        for (i = 0; i < clientCount; i++) {
            if (i == clc.clientNum)
                continue;

            info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + i];

            name = Info_ValueForKey(info, "n");
            if (name[0] == '\0')
                continue;
            Q_strncpyz(names[nameCount], name, sizeof(names[nameCount]));
            Q_CleanStr(names[nameCount]);

            namesPtr[nameCount] = names[nameCount];
            nameCount++;
        }
        qsort((void*)namesPtr, nameCount, sizeof(namesPtr[0]), Com_strCompare);

        Field_CompletePlayerName(namesPtr, nameCount);
    }
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f(void) {
    char message[MAX_RCON_MESSAGE];
    netadr_t to;

    if (!rcon_client_password->string[0]) {
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

    // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
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

/*
=================
CL_SendPureChecksums
=================
*/
void CL_SendPureChecksums(void) {
    char cMsg[MAX_INFO_VALUE];

    // if we are pure we need to send back a command with our referenced pk3 checksums
    Com_sprintf(cMsg, sizeof(cMsg), "cp %d %s", cl.serverId, FS_ReferencedPakPureChecksums());

    CL_AddReliableCommand(cMsg, qfalse);
}

/*
=================
CL_ResetPureClientAtServer
=================
*/
void CL_ResetPureClientAtServer(void) {
    CL_AddReliableCommand("vdr", qfalse);
}

/*
=================
CL_Vid_Restart_f

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
void CL_Vid_Restart_f(void) {
    // Settings may have changed so stop recording now
    if (CL_VideoRecording()) {
        CL_CloseAVI();
    }

    if (clc.demorecording)
        CL_StopRecord_f();

    // don't let them loop during the restart
    S_StopAllSounds();

    if (!FS_ConditionalRestart(clc.checksumFeed, qtrue)) {
        // if not running a server clear the whole hunk
        if (com_sv_running->integer) {
            // clear all the client data on the hunk
            Hunk_ClearToMark();
        } else {
            // clear the whole hunk
            Hunk_Clear();
        }

        // shutdown the UI
        CL_ShutdownUI();
        // shutdown the CGame
        CL_ShutdownCGame();
        // shutdown the renderer and clear the renderer interface
        CL_ShutdownRef();
        // client is no longer pure until new checksums are sent
        CL_ResetPureClientAtServer();
        // clear pak references
        FS_ClearPakReferences(FS_UI_REF | FS_CGAME_REF);
        // reinitialize the filesystem if the game directory or checksum has changed

        cls.rendererStarted = qfalse;
        cls.uiStarted = qfalse;
        cls.cgameStarted = qfalse;
        cls.soundRegistered = qfalse;

        // unpause so the cgame definitely gets a snapshot and renders a frame
        Cvar_Set("cl_paused", "0");

        // initialize the renderer interface
        CL_InitRef();

        // startup all the client stuff
        CL_StartHunkUsers(qfalse);

        // start the cgame if connected
        if (clc.state > CA_CONNECTED && clc.state != CA_CINEMATIC) {
            cls.cgameStarted = qtrue;
            CL_InitCGame();
            // send pure checksums
            CL_SendPureChecksums();
        }
    }
}

/*
=================
CL_Snd_Restart

Restart the sound subsystem
=================
*/
void CL_Snd_Shutdown(void) {
    S_Shutdown();
    cls.soundStarted = qfalse;
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
void CL_Snd_Restart_f(void) {
    CL_Snd_Shutdown();
    // sound will be reinitialized by vid_restart
    CL_Vid_Restart_f();
}

/*
==================
CL_PK3List_f
==================
*/
void CL_OpenedPK3List_f(void) {
    Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames());
}

/*
==================
CL_PureList_f
==================
*/
void CL_ReferencedPK3List_f(void) {
    Com_Printf("Referenced PK3 Names: %s\n", FS_ReferencedPakNames());
}

/*
==================
CL_Configstrings_f
==================
*/
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

/*
==============
CL_Clientinfo_f
==============
*/
void CL_Clientinfo_f(void) {
    Com_Printf("--------- Client Information ---------\n");
    Com_Printf("state: %i\n", clc.state);
    Com_Printf("Server: %s\n", clc.servername);
    Com_Printf("User info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_USERINFO));
    Com_Printf("--------------------------------------\n");
}

/*
==============
CL_PostProcessRestart_f

[QL] Restart post-processing pipeline.
In the binary this calls through a renderer function pointer.
No-op in ioquakelive since we don't have a separate post-process pipeline.
==============
*/
void CL_PostProcessRestart_f(void) {
}

/*
==============
CL_Userinfo_f

[QL] Dump current userinfo string
==============
*/
void CL_Userinfo_f(void) {
    Com_Printf("User info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_USERINFO));
}

//====================================================================

/*
=================
CL_DownloadsComplete

Called when all downloading has been completed
=================
*/
void CL_DownloadsComplete(void) {
#ifdef USE_CURL
    // if we downloaded with cURL
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

    // if we downloaded files we need to restart the file system
    if (clc.downloadRestart) {
        clc.downloadRestart = qfalse;

        FS_Restart(clc.checksumFeed);  // We possibly downloaded a pak, restart the file system to load it

        // inform the server so we get new gamestate info
        CL_AddReliableCommand("donedl", qfalse);

        // by sending the donedl command we request a new gamestate
        // so we don't want to load stuff yet
        return;
    }

    // let the client game init and load data
    clc.state = CA_LOADING;

    // Pump the loop, this may change gamestate!
    Com_EventLoop();

    // if the gamestate was changed by calling Com_EventLoop
    // then we loaded everything already and we don't want to do it again.
    if (clc.state != CA_LOADING) {
        return;
    }

    // starting to load a map so we get out of full screen ui mode
    Cvar_Set("r_uiFullScreen", "0");

    // flush client memory and start loading stuff
    // this will also (re)load the UI
    // if this is a local client then only the client part of the hunk
    // will be cleared, note that this is done after the hunk mark has been set
    CL_FlushMemory();

    // initialize the CGame
    cls.cgameStarted = qtrue;
    CL_InitCGame();

    // set pure checksums
    CL_SendPureChecksums();

    CL_WritePacket();
    CL_WritePacket();
    CL_WritePacket();
}

/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/
void CL_BeginDownload(const char* localName, const char* remoteName) {
    Com_DPrintf(
        "***** CL_BeginDownload *****\n"
        "Localname: %s\n"
        "Remotename: %s\n"
        "****************************\n",
        localName, remoteName);

    Q_strncpyz(clc.downloadName, localName, sizeof(clc.downloadName));
    Com_sprintf(clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName);

    // Set so UI gets access to it
    Cvar_Set("cl_downloadName", remoteName);
    Cvar_Set("cl_downloadSize", "0");
    Cvar_Set("cl_downloadCount", "0");
    Cvar_SetValue("cl_downloadTime", cls.realtime);

    clc.downloadBlock = 0;  // Starting new file
    clc.downloadCount = 0;

    CL_AddReliableCommand(va("download %s", remoteName), qfalse);
}

/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload(void) {
    char* s;
    char *remoteName, *localName;
    qboolean useCURL = qfalse;

    // A download has finished, check whether this matches a referenced checksum
    if (*clc.downloadName) {
        char* zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clc.downloadName, "");
        zippath[strlen(zippath) - 1] = '\0';

        if (!FS_CompareZipChecksum(zippath))
            Com_Error(ERR_DROP, "Incorrect checksum for file: %s", clc.downloadName);
    }

    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");

    // We are looking to start a download here
    if (*clc.downloadList) {
        s = clc.downloadList;

        // format is:
        //  @remotename@localname@remotename@localname, etc.

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
            s = localName + strlen(localName);  // point at the nul byte
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
#endif /* USE_CURL */
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

        // move over the rest
        memmove(clc.downloadList, s, strlen(s) + 1);

        return;
    }

    CL_DownloadsComplete();
}

/*
==================
CL_WorkshopDownloads

[QL] 0x004bba30. Before entering, the client downloads through Steam whatever
Workshop items of CS_STEAM_WORKSHOP_IDS it lacks, one at a time, with progress in
cl_downloadName, cl_downloadItem and cl_downloadTime. Returns qtrue when there is
a download to wait for.
==================
*/
static qboolean CL_WorkshopDownloads(void) {
    const char* list;
    char copy[BIG_INFO_STRING];
    char* token;
    int total = 0;

    cl_workshopCount = 0;
    cl_workshopIndex = 0;

    if (!Steam_WorkshopReady()) {
        return qfalse;
    }
    list = cl.gameState.stringData + cl.gameState.stringOffsets[CS_STEAM_WORKSHOP_IDS];
    if (!list[0]) {
        return qfalse;
    }

    Com_Printf("Server requires the following workshop items: %s\n", list);
    Q_strncpyz(copy, list, sizeof(copy));
    for (token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
        uint64_t item = (uint64_t)strtoull(token, NULL, 10);

        if (!item || total >= CL_MAX_WORKSHOP) {
            continue;
        }
        if (Steam_WorkshopInstalled(item)) {
            continue;
        }
        cl_workshopItems[total++] = item;
    }
    cl_workshopCount = total;
    if (!total) {
        return qfalse;
    }

    // from here it is like any other download: the game waits connected and restarts the
    // filesystem at the end to see the new pk3s
    clc.state = CA_CONNECTED;
    clc.downloadRestart = qtrue;
    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");
    CL_WorkshopNext();
    return qtrue;
}

/*
==================
CL_WorkshopNext

Asks for the next item on the list. With none left it takes the usual path:
restarts the filesystem and enters.
==================
*/
void CL_WorkshopNext(void) {
    while (cl_workshopIndex < cl_workshopCount) {
        uint64_t item = cl_workshopItems[cl_workshopIndex++];

        Cvar_Set("cl_downloadItem", va("%llu", (unsigned long long)item));
        Cvar_Set("cl_downloadName", va("Workshop item %i of %i", cl_workshopIndex, cl_workshopCount));
        Cvar_SetValue("cl_downloadTime", cls.realtime);
        Cvar_SetValue("cl_downloadSize", 0);
        Cvar_SetValue("cl_downloadCount", 0);
        if (Steam_WorkshopDownload(item)) {
            return;  // wait for Steam to bring it down
        }
    }

    // no quedo ninguno pendiente
    cl_workshopCount = 0;
    Cvar_Set("cl_downloadName", "");
    Cvar_Set("cl_downloadItem", "");
    CL_DownloadsComplete();
}

/*
==================
CL_WorkshopFrame

While Steam fetches an item, progress goes into the same cvars the connection screen
watches. When it finishes, it moves on to whichever is still missing.
==================
*/
void CL_WorkshopFrame(void) {
    uint64_t done = 0;
    uint64_t total = 0;

    if (!cl_workshopCount) {
        return;
    }
    if (Steam_WorkshopBusy()) {
        if (Steam_WorkshopProgress(&done, &total) && total) {
            Cvar_SetValue("cl_downloadSize", (float)total);
            Cvar_SetValue("cl_downloadCount", (float)done);
        }
        return;
    }
    CL_WorkshopNext();
}

/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads(void) {
    char missingfiles[1024];

    // [QL] 0x004bba30: Quake Live's maps come from the Workshop, not over HTTP
    if (CL_WorkshopDownloads()) {
        return;
    }

    if (!(cl_allowDownload->integer & DLF_ENABLE)) {
        // autodownload is disabled on the client
        // but it's possible that some referenced files on the server are missing
        if (FS_ComparePaks(missingfiles, sizeof(missingfiles), qfalse)) {
            // NOTE TTimo I would rather have that printed as a modal message box
            //   but at this point while joining the game we don't know wether we will successfully join or not
            Com_Printf(
                "\nWARNING: You are missing some files referenced by the server:\n%s"
                "You might not be able to join the game\n"
                "Go to the setting menu to turn on autodownload, or get the file elsewhere\n\n",
                missingfiles);
        }
    } else if (FS_ComparePaks(clc.downloadList, sizeof(clc.downloadList), qtrue)) {
        Com_Printf("Need paks: %s\n", clc.downloadList);

        if (*clc.downloadList) {
            // if autodownloading is not enabled on the server
            clc.state = CA_CONNECTED;

            *clc.downloadTempName = *clc.downloadName = 0;
            Cvar_Set("cl_downloadName", "");

            CL_NextDownload();
            return;
        }
    }

    CL_DownloadsComplete();
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend(void) {
    int port;
    char info[MAX_INFO_STRING];
    char data[MAX_INFO_STRING + 10];

    // don't send anything if playing back a demo
    if (clc.demoplaying) {
        return;
    }

    // resend if we haven't gotten a reply yet
    if (clc.state != CA_CONNECTING && clc.state != CA_CHALLENGING) {
        return;
    }

    if (cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT) {
        return;
    }

    clc.connectTime = cls.realtime;  // for retransmit requests
    clc.connectPacketCount++;

    switch (clc.state) {
        case CA_CONNECTING:
        {
            // QL getchallenge payload: "getchallenge " (13) + steam key (8) + auth token (11-512).
            // Can't use NET_OutOfBandPrint (stops at nulls) or NET_OutOfBandData (adds Huffman).
            //
            // [QL] Steam signs the ticket for this session and the server hands it to Steam
            // to learn whether the Steam ID belongs to who it claims. With no Steam the usual
            // filler and the fallback identifier go out instead, so a local network still
            // plays, where there is nobody to validate anything either.
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
            Com_Memcpy(payload,      "getchallenge ", 13);  // command
            Com_Memcpy(payload + 13, &steamId, 8);          // steam key

            ticketLen = Steam_AuthTicket(payload + 21, sizeof(payload) - 21);
            if (ticketLen <= 0) {
                Com_Memset(payload + 21, 0, 16);  // the server demands eleven bytes at least
                ticketLen = 16;
            }
            NET_OutOfBandRaw(NS_CLIENT, clc.serverAddress, payload, 21 + ticketLen);
            break;
        }

        case CA_CHALLENGING:
            // sending back the challenge
            port = Cvar_VariableValue("net_qport");

            Q_strncpyz(info, Cvar_InfoString(CVAR_USERINFO), sizeof(info));

            Info_SetValueForKey(info, "protocol", va("%i", com_protocol->integer));
            Info_SetValueForKey(info, "qport", va("%i", port));
            Info_SetValueForKey(info, "challenge", va("%i", clc.challenge));

            Com_sprintf(data, sizeof(data), "connect \"%s\"", info);
            NET_OutOfBandData(NS_CLIENT, clc.serverAddress, (byte*)data, strlen(data));
            // the most current userinfo has been sent, so watch for any
            // newer changes to userinfo variables
            cvar_modifiedFlags &= ~CVAR_USERINFO;
            break;

        default:
            Com_Error(ERR_FATAL, "CL_CheckForResend: bad clc.state");
    }
}

/*
===================
CL_MotdPacket

===================
*/
void CL_MotdPacket(netadr_t from) {
#ifdef UPDATE_SERVER_NAME
    char* challenge;
    char* info;

    // if not from our server, ignore it
    if (!NET_CompareAdr(from, cls.updateServer)) {
        return;
    }

    info = Cmd_Argv(1);

    // check challenge
    challenge = Info_ValueForKey(info, "challenge");
    if (strcmp(challenge, cls.updateChallenge)) {
        return;
    }

    challenge = Info_ValueForKey(info, "motd");

    Q_strncpyz(cls.updateInfoString, info, sizeof(cls.updateInfoString));
    Cvar_Set("cl_motdString", challenge);
#endif
}

/*
===================
CL_InitServerInfo
===================
*/
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
}

#define MAX_SERVERSPERPACKET 256

/*
===================
CL_ServersResponsePacket
===================
*/
void CL_ServersResponsePacket(const netadr_t* from, msg_t* msg, qboolean extended) {
    int i, j, count, total;
    netadr_t addresses[MAX_SERVERSPERPACKET];
    int numservers;
    byte* buffptr;
    byte* buffend;

    Com_Printf("CL_ServersResponsePacket from %s\n", NET_AdrToStringwPort(*from));

    if (cls.numglobalservers == -1) {
        // state to detect lack of servers or lack of response
        cls.numglobalservers = 0;
        cls.numGlobalServerAddresses = 0;
    }

    // parse through server response string
    numservers = 0;
    buffptr = msg->data;
    buffend = buffptr + msg->cursize;

    // advance to initial token
    do {
        if (*buffptr == '\\' || (extended && *buffptr == '/'))
            break;

        buffptr++;
    } while (buffptr < buffend);

    while (buffptr + 1 < buffend) {
        // IPv4 address
        if (*buffptr == '\\') {
            buffptr++;

            if (buffend - buffptr < sizeof(addresses[numservers].ip) + sizeof(addresses[numservers].port) + 1)
                break;

            for (i = 0; i < sizeof(addresses[numservers].ip); i++)
                addresses[numservers].ip[i] = *buffptr++;

            addresses[numservers].type = NA_IP;
        }
        // IPv6 address, if it's an extended response
        else if (extended && *buffptr == '/') {
            buffptr++;

            if (buffend - buffptr < sizeof(addresses[numservers].ip6) + sizeof(addresses[numservers].port) + 1)
                break;

            for (i = 0; i < sizeof(addresses[numservers].ip6); i++)
                addresses[numservers].ip6[i] = *buffptr++;

            addresses[numservers].type = NA_IP6;
            addresses[numservers].scope_id = from->scope_id;
        } else
            // syntax error!
            break;

        // parse out port
        addresses[numservers].port = (*buffptr++) << 8;
        addresses[numservers].port += *buffptr++;
        addresses[numservers].port = BigShort(addresses[numservers].port);

        // syntax check
        if (*buffptr != '\\' && *buffptr != '/')
            break;

        numservers++;
        if (numservers >= MAX_SERVERSPERPACKET)
            break;
    }

    count = cls.numglobalservers;

    for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {
        // build net address
        serverInfo_t* server = &cls.globalServers[count];

        // Tequila: It's possible to have sent many master server requests. Then
        // we may receive many times the same addresses from the master server.
        // We just avoid to add a server if it is still in the global servers list.
        for (j = 0; j < count; j++) {
            if (NET_CompareAdr(cls.globalServers[j].adr, addresses[i]))
                break;
        }

        if (j < count)
            continue;

        CL_InitServerInfo(server, &addresses[i]);
        // advance to next slot
        count++;
    }

    // if getting the global list
    if (count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS) {
        // if we couldn't store the servers in the main list anymore
        for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++) {
            // just store the addresses in an additional list
            cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
        }
    }

    cls.numglobalservers = count;
    total = count + cls.numGlobalServerAddresses;

    Com_Printf("%d servers parsed (total %d)\n", numservers, total);
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket(netadr_t from, msg_t* msg) {
    char* s;
    char* c;

    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg);  // skip the -1

    s = MSG_ReadStringLine(msg);

    Cmd_TokenizeString(s);

    c = Cmd_Argv(0);

    Com_DPrintf("CL packet %s: %s\n", NET_AdrToStringwPort(from), c);

    if (!Q_stricmp(c, "challengeResponse")) {
        if (clc.state != CA_CONNECTING) {
            Com_DPrintf("Unwanted challenge response received. Ignored.\n");
            return;
        }

        // QL server sends: "challengeResponse <challenge>"
        // No client challenge echo or protocol version in Argv(2)/Argv(3).
        clc.challenge = atoi(Cmd_Argv(1));
        clc.state = CA_CHALLENGING;
        clc.connectPacketCount = 0;
        clc.connectTime = -99999;

        // take this address as the new server address.  This allows
        // a server proxy to hand off connections to multiple servers
        clc.serverAddress = from;
        Com_DPrintf("challengeResponse: %d\n", clc.challenge);
        return;
    }

    // server connection
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

        // QL server sends just "connectResponse" with no challenge echo.
        // Address check above is sufficient anti-hijack protection.
        Netchan_Setup(NS_CLIENT, &clc.netchan, from, Cvar_VariableValue("net_qport"),
                      clc.challenge);

        clc.state = CA_CONNECTED;
        clc.lastPacketSentTime = -9999;  // send first packet immediately
        return;
    }

    // server responding to an info broadcast
    if (!Q_stricmp(c, "infoResponse")) {
        CL_ServerInfoPacket(from, msg);
        return;
    }

    // server responding to a get playerlist
    if (!Q_stricmp(c, "statusResponse")) {
        CL_ServerStatusResponse(from, msg);
        return;
    }

    // echo request from server
    if (!Q_stricmp(c, "echo")) {
        // NOTE: we may have to add exceptions for auth and update servers
        if (NET_CompareAdr(from, clc.serverAddress) || NET_CompareAdr(from, cls.rconAddress)) {
            NET_OutOfBandPrint(NS_CLIENT, from, "%s", Cmd_Argv(1));
        }
        return;
    }

    // cd check
    if (!Q_stricmp(c, "keyAuthorize")) {
        // we don't use these now, so dump them on the floor
        return;
    }

    // global MOTD from id
    if (!Q_stricmp(c, "motd")) {
        CL_MotdPacket(from);
        return;
    }

    // echo request from server
    if (!Q_stricmp(c, "print")) {
        // NOTE: we may have to add exceptions for auth and update servers
        if (NET_CompareAdr(from, clc.serverAddress) || NET_CompareAdr(from, cls.rconAddress)) {
            s = MSG_ReadString(msg);

            Q_strncpyz(clc.serverMessage, s, sizeof(clc.serverMessage));
            Com_Printf("%s", s);
        }
        return;
    }

    // list of servers sent back by a master server (classic)
    if (!Q_strncmp(c, "getserversResponse", 18)) {
        CL_ServersResponsePacket(&from, msg, qfalse);
        return;
    }

    // list of servers sent back by a master server (extended)
    if (!Q_strncmp(c, "getserversExtResponse", 21)) {
        CL_ServersResponsePacket(&from, msg, qtrue);
        return;
    }

    Com_DPrintf("Unknown connectionless packet command.\n");
}

/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent(netadr_t from, msg_t* msg) {
    int headerBytes;

    clc.lastPacketTime = cls.realtime;

    if (msg->cursize >= 4 && *(int*)msg->data == -1) {
        CL_ConnectionlessPacket(from, msg);
        return;
    }

    if (clc.state < CA_CONNECTED) {
        return;  // can't be a valid sequenced packet
    }

    if (msg->cursize < 4) {
        Com_Printf("%s: Runt packet\n", NET_AdrToStringwPort(from));
        return;
    }

    //
    // packet from server
    //
    if (!NET_CompareAdr(from, clc.netchan.remoteAddress)) {
        Com_DPrintf("%s:sequenced packet without connection\n", NET_AdrToStringwPort(from));
        // FIXME: send a client disconnect?
        return;
    }

    if (!CL_Netchan_Process(&clc.netchan, msg)) {
        return;  // out of order, duplicated, etc
    }

    // the header is different lengths for reliable and unreliable messages
    headerBytes = msg->readcount;

    // track the last message received so it can be returned in
    // client messages, allowing the server to detect a dropped
    // gamestate
    clc.serverMessageSequence = LittleLong(*(int*)msg->data);

    clc.lastPacketTime = cls.realtime;
    CL_ParseServerMessage(msg);

    //
    // we don't know if it is ok to save a demo message until
    // after we have parsed the frame
    //
    if (clc.demorecording && !clc.demowaiting) {
        CL_WriteDemoMessage(msg, headerBytes);
    }
}

/*
==================
CL_CheckTimeout

==================
*/
void CL_CheckTimeout(void) {
    // [ioquakelive] a demo is a file, not a connection: it has nothing to time out. The clock
    // only moved while the demo was being read, so a demo held on pause ran out cl_timeout
    // (40 seconds) and dropped to the menu by itself
    if (clc.demoplaying) {
        cl.timeoutcount = 0;
        return;
    }

    //
    // check timeout
    //
    if ((!CL_CheckPaused() || !sv_paused->integer) && clc.state >= CA_CONNECTED && clc.state != CA_CINEMATIC && cls.realtime - clc.lastPacketTime > cl_timeout->value * 1000) {
        if (++cl.timeoutcount > 5) {  // timeoutcount saves debugger
            Com_Printf("\nServer connection timed out.\n");
            CL_Disconnect(qtrue);
            return;
        }
    } else {
        cl.timeoutcount = 0;
    }
}

/*
==================
CL_CheckPaused
Check whether client has been paused.
==================
*/
qboolean CL_CheckPaused(void) {
    // if cl_paused->modified is set, the cvar has only been changed in
    // this frame. Keep paused in this frame to ensure the server doesn't
    // lag behind.
    if (cl_paused->integer || cl_paused->modified)
        return qtrue;

    return qfalse;
}

//============================================================================

/*
==================
CL_CheckUserinfo

==================
*/
void CL_CheckUserinfo(void) {
    // don't add reliable commands when not yet connected
    if (clc.state < CA_CONNECTED)
        return;

    // don't overflow the reliable command buffer when paused
    if (CL_CheckPaused())
        return;

    // send a reliable userinfo update if needed
    if (cvar_modifiedFlags & CVAR_USERINFO) {
        cvar_modifiedFlags &= ~CVAR_USERINFO;
        CL_AddReliableCommand(va("userinfo \"%s\"", Cvar_InfoString(CVAR_USERINFO)), qfalse);
    }
}

/*
==================
CL_LowLatencyWait

[ioquakelive] the renderer decides: without a low latency path this costs nothing
==================
*/
void CL_LowLatencyWait(void) {
    if (cls.rendererStarted && re.LowLatencyWait) {
        re.LowLatencyWait();
    }
}

/*
==================
CL_Frame

==================
*/
void CL_Frame(int msec) {
    if (!com_cl_running->integer) {
        return;
    }

#ifdef USE_CURL
    if (clc.downloadCURLM) {
        CL_cURL_PerformDownload();
        // we can't process frames normally when in disconnected
        // download mode since the ui vm expects clc.state to be
        // CA_CONNECTED
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
        // if disconnected, bring up the menu
        S_StopAllSounds();
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN);
    }

    // if recording an avi, lock to a fixed fps
    if (CL_VideoRecording() && cl_aviFrameRate->integer && msec) {
        // save the current screen
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
            // If not recording a demo, and we should be, start one
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
            // Replace the ":" in the address as it is not a valid
            // file name character
            p = strstr(serverName, ":");
            if (p) {
                *p = '.';
            }

            Q_strncpyz(mapName, COM_SkipPath(cl.mapname), sizeof(cl.mapname));
            COM_StripExtension(mapName, mapName, sizeof(mapName));

            Cbuf_ExecuteText(EXEC_NOW,
                             va("record %s-%s-%s", nowString, serverName, mapName));
        } else if (clc.state != CA_ACTIVE && clc.demorecording) {
            // Recording, but not CA_ACTIVE, so stop recording
            CL_StopRecord_f();
        }
    }

    // save the msec before checking pause
    cls.realFrametime = msec;

    // decide the simulation time
    cls.frametime = msec;

    cls.realtime += cls.frametime;

    if (cl_timegraph->integer) {
        SCR_DebugGraph(cls.realFrametime * 0.25);
    }

    // see if we need to update any userinfo
    CL_CheckUserinfo();

    // if we haven't gotten a packet in a long time,
    // drop the connection
    CL_CheckTimeout();

    // send intentions now
    CL_SendCmd();

    // resend a connection request if necessary
    CL_CheckForResend();

    // [QL] whatever Steam keeps answering about the server list
    CL_SteamServersFrame();

    // [QL] and whatever is coming down from the Workshop while joining a server
    CL_WorkshopFrame();

    // [QL] voice, which comes and goes over Steam's peer to peer network
    CL_VoiceFrame();

    // decide on the serverTime to render
    CL_SetCGameTime();

    // update the screen
    SCR_UpdateScreen();

    // update audio
    S_Update();

    // advance local effects for next frame
    SCR_RunCinematic();

    Con_RunConsole();

    cls.framecount++;
}

//============================================================================

/*
================
CL_RefPrintf

DLL glue
================
*/
static __attribute__((format(printf, 2, 3))) void QDECL CL_RefPrintf(int print_level, const char* fmt, ...) {
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (print_level == PRINT_ALL) {
        Com_Printf("%s", msg);
    } else if (print_level == PRINT_WARNING) {
        Com_Printf(S_COLOR_YELLOW "%s", msg);  // yellow
    } else if (print_level == PRINT_ERROR) {
        Com_Printf(S_COLOR_RED "%s", msg);  // red
    } else if (print_level == PRINT_DEVELOPER) {
#if DEBUG_RENDERER
        Com_DPrintf(S_COLOR_RED "%s", msg);  // red - developer only
#endif
    }
}

/*
============
CL_ShutdownRef
============
*/
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

/*
============
CL_InitRenderer
============
*/
void CL_InitRenderer(void) {
    // this sets up the renderer and calls R_Init
    re.BeginRegistration(&cls.glconfig);

    // load character sets
    cls.charSetShader = re.RegisterShader("gfx/2d/bigchars");
    cls.whiteShader = re.RegisterShader("white");
    cls.consoleShader = re.RegisterShader("console");
    g_console_field_width = cls.glconfig.vidWidth / SMALLCHAR_WIDTH - 2;
    g_consoleField.widthInChars = g_console_field_width;
}

/*
============================
CL_StartHunkUsers

After the server has cleared the hunk, these will need to be restarted
This is the only place that any of these functions are called from
============================
*/
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

/*
============
CL_RefMalloc
============
*/
void* CL_RefMalloc(int size) {
    return Z_TagMalloc(size, TAG_RENDERER);
}

int CL_ScaledMilliseconds(void) {
    return Sys_Milliseconds() * com_timescale->value;
}

/*
============
CL_InitRef
============
*/
void CL_InitRef(void) {
    refimport_t ri;
    refexport_t* ret;
#ifdef USE_RENDERER_DLOPEN
    GetRefAPI_t GetRefAPI;
    char dllName[MAX_OSPATH];
#endif

    Com_Printf("----- Initializing Renderer ----\n");

#ifdef USE_RENDERER_DLOPEN
    // [ioquakelive] the renderer of retail's lineage, on Quake3e's, is the default; renderergl2
    // stays as the backup
    cl_renderer = Cvar_Get("cl_renderer", "opengl1", CVAR_ARCHIVE | CVAR_LATCH);

    // [ioquakelive] a config on the old default moves to the new one; another choice is kept
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

    // [ioquakelive] ioquakelive-vulkan is this same executable under another name, the way Quake3e
    // ships its two: it asks for vulkan and leaves cl_renderer alone, so the OpenGL one keeps what
    // the config says. Without Vulkan it goes on to the OpenGL chain below
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

    // [ioquakelive] a renderer that doesn't load falls back to the default, and the default to
    // renderergl2, without keeping it: the next start tries the chosen one again
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

    // cinematic stuff

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

    Com_Printf("-------------------------------\n");

    if (!ret) {
        Com_Error(ERR_FATAL, "Couldn't initialize refresh");
    }

    re = *ret;

    // unpause so the cgame definitely gets a snapshot and renders a frame
    Cvar_Set("cl_paused", "0");
}

//===========================================================================================

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

//===========================================================================================

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

/*
====================
CL_Web_ChangeHash_f

[QL] This would usually change the web path Awesomium is looking at, but 
for us we're just going to hijack it and display the legacy menus.
====================
*/
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

	// [QL] our glue for the binary handler Web_SetLocation_f (0x004f31d0). Strip one
	// leading '/'. Compare against the char '/', not the string "/" (that was a
	// char-vs-pointer bug, C4047, and always false).
	if (web_path[0] == '/') {
		web_path++;
	}
    
    // [ioquakelive] the browser's two destinations this build has a page for: the
    // settings, and anything else, which in retail is its home page. Retail's in-game
    // "Main Menu" button sends "/" and used to close the menu here.
    if (Q_stristr(web_path, "settings")) {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN_OPTIONS);
    } else {
        VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_HOME);
    }
}

/*
====================
CL_Web_HideBrowser_f

[QL] quakelive_steam.exe 0x4f24d0: hides the browser and gives the screen back to
the game (web_browserActive 0, the browser's key catcher cleared). Its App sends it
from the "return to your match" banner. Here the front end is the UI module, so the
same thing is closing it -- and only over a match, since without one there is
nothing to go back to.
====================
*/
static void CL_Web_HideBrowser_f(void) {
    if (!uivm || clc.state != CA_ACTIVE) {
        return;
    }
    VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE);
}

/*
====================
CL_OpenURL

[ioquakelive] Opens a link outside the game. Retail handed its links to the Steam
overlay, which opens the system browser when the overlay is off; without Steam
that is all this build can do. Only a plain https link is opened: the address goes
to the shell, and a server that could reach this would be handing the shell
whatever it liked.
====================
*/
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


// =====================================================================
// [QL] Alias system - binary-verified from quakelive_steam.exe
// CL_FindAlias: 0x004B4F80, CL_RunAlias: 0x004B4FD0
// CL_Alias_f: 0x004B5060, CL_Unalias_f: 0x004B5170, CL_UnaliasAll_f: 0x004B51C0
// CL_ListAliases: 0x004B5020, Cmd_WriteAliases: 0x004B51F0
// CL_InitAliases: 0x004B5240
// =====================================================================

#define MAX_ALIASES         100
#define ALIAS_NAME_SIZE     32   // 0x20
#define ALIAS_CMD_SIZE      256  // 0x100

typedef struct {
    char    name[ALIAS_NAME_SIZE];  // offset 0x00
    char    cmd[ALIAS_CMD_SIZE];    // offset 0x20
    int     active;                 // offset 0x120 (288)
} alias_t;  // total: 0x124 (292) bytes

static alias_t aliases[MAX_ALIASES];

// 0x004B4F80 - Search for an alias by name (case-insensitive)
void *CL_FindAlias(const char *name) {
    int i;
    for (i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].active && !Q_stricmp(name, aliases[i].name)) {
            return &aliases[i];
        }
    }
    return NULL;
}

// 0x004B4FD0 - Execute the command text of an alias looked up by Cmd_Argv(0)
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

// 0x004B5020 - List all defined aliases
static void CL_ListAliases(void) {
    int i;
    Com_Printf("Alias List:\n");
    for (i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].active) {
            Com_Printf("  %s \"%s\"\n", aliases[i].name, aliases[i].cmd);
        }
    }
}

// 0x004B5060 - "alias [name] [command]"
static void CL_Alias_f(void) {
    alias_t *a;
    int i;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: alias \"command\"\n");
        CL_ListAliases();
        return;
    }

    if (Cmd_Argc() < 3) {
        // [QL] Binary uses Cmd_Argv(0) here - looks up "alias" itself, which
        // is never a valid alias name, so this path effectively does nothing.
        a = (alias_t *)CL_FindAlias(Cmd_Argv(0));
        if (a) {
            Com_Printf("Alias: %s \"%s\"\n", a->name, a->cmd);
        }
        return;
    }

    // argc >= 3: create or update
    a = (alias_t *)CL_FindAlias(Cmd_Argv(1));
    if (a) {
        // Update existing alias
        Q_strncpyz(a->cmd, Cmd_Argv(2), ALIAS_CMD_SIZE);
        return;
    }

    // Find a free slot
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

// 0x004B5170 - "unalias <name>"
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

// 0x004B51C0 - "unaliasall"
static void CL_UnaliasAll_f(void) {
    int i;
    for (i = 0; i < MAX_ALIASES; i++) {
        aliases[i].active = 0;
    }
}

// 0x004B51F0 - Write aliases to config file
void Cmd_WriteAliases(fileHandle_t f) {
    int i;
    FS_Printf(f, "unaliasall\n");
    for (i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].active) {
            FS_Printf(f, "alias %s \"%s\"\n", aliases[i].name, aliases[i].cmd);
        }
    }
}

// 0x004B5240 - Initialize the alias system
void CL_InitAliases(void) {
    memset(aliases, 0, sizeof(aliases));
    Cmd_AddCommand("alias", CL_Alias_f);
    Cmd_AddCommand("unalias", CL_Unalias_f);
    Cmd_AddCommand("unaliasall", CL_UnaliasAll_f);
}

// =====================================================================



/*
====================
CL_Init
====================
*/

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
        clc.state = CA_DISCONNECTED;  // no longer CA_UNINITIALIZED
        cl_oldGameSet = qfalse;
    }

    cls.realtime = 0;

    CL_InitInput();

    //
    // register our variables
    //
    cl_noprint = Cvar_Get("cl_noprint", "0", 0);
#ifdef UPDATE_SERVER_NAME
    cl_motd = Cvar_Get("cl_motd", "1", 0);
#endif

    // [QL] quakelive_steam.exe 0x4bca53: 40 seconds
    cl_timeout = Cvar_Get("cl_timeout", "40", 0);

    // [QL] 0x4bca3d and 0x4bc731: cl_timeNudge is -20 to 0, cl_autoTimeNudge a switch
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

    rconAddress = Cvar_Get("rconAddress", "", 0);

    // [QL] quakelive_steam.exe 0x4bc70e, 0x4bc942, 0x4bca85: turning with keys is cheat protected
    cl_yawspeed = Cvar_Get("cl_yawspeed", "140", CVAR_CHEAT);
    cl_pitchspeed = Cvar_Get("cl_pitchspeed", "140", CVAR_CHEAT);
    cl_anglespeedkey = Cvar_Get("cl_anglespeedkey", "1.5", CVAR_CHEAT);

    // [QL] 0x4bc835: 125 packets a second, and cheat protected
    cl_maxpackets = Cvar_Get("cl_maxpackets", "125", CVAR_CHEAT);
    cl_packetdup = Cvar_Get("cl_packetdup", "1", CVAR_ARCHIVE | CVAR_USERSAVE);

    cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE);

    // [QL] the mouse (0x4bc7be-0x4bcb1e): sensitivity 4, and retail's acceleration, which adds
    // (cl_mouseAccel * (rate - cl_mouseAccelOffset)) ^ (cl_mouseAccelPower - 1) to the sensitivity, up
    // to cl_mouseSensCap. With m_cpi (the mouse's counts per inch) the rate is in cm/s and the
    // sensitivity is in degrees per cm. cl_mouseAccelDebug writes mouse.log. See CL_MouseMove.
    cl_sensitivity = Cvar_Get("sensitivity", "4", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccel = Cvar_Get("cl_mouseAccel", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccelOffset = Cvar_Get("cl_mouseAccelOffset", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccelPower = Cvar_Get("cl_mouseAccelPower", "2", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cl_mouseAccelDebug = Cvar_Get("cl_mouseAccelDebug", "0", 0);
    cl_mouseSensCap = Cvar_Get("cl_mouseSensCap", "0", CVAR_ARCHIVE | CVAR_USERSAVE);
    cl_freelook = Cvar_Get("cl_freelook", "1", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cg_ignoreMouseInput = Cvar_Get("cg_ignoreMouseInput", "0", CVAR_ROM);
    // [QL] view acceleration of retail's joystick code, which this engine's SDL joystick doesn't use
    Cvar_Get("cl_viewAccel", "1.7", CVAR_ARCHIVE | CVAR_USERSAVE);

    cl_allowDownload = Cvar_Get("cl_allowDownload", "0", CVAR_ARCHIVE);
#ifdef USE_CURL_DLOPEN
    cl_cURLLib = Cvar_Get("cl_cURLLib", DEFAULT_CURL_LIB, CVAR_ARCHIVE | CVAR_PROTECTED);
#endif

    cl_conXOffset = Cvar_Get("cl_conXOffset", "0", 0);
#ifdef __APPLE__
    // In game video is REALLY slow in Mac OS X right now due to driver slowness
    cl_inGameVideo = Cvar_Get("r_inGameVideo", "0", CVAR_ARCHIVE);
#else
    cl_inGameVideo = Cvar_Get("r_inGameVideo", "1", CVAR_ARCHIVE);
#endif

    cl_serverStatusResendTime = Cvar_Get("cl_serverStatusResendTime", "750", 0);

    // init autoswitch so the ui will have it correctly even
    // if the cgame hasn't been started
    Cvar_Get("cg_autoswitch", "1", CVAR_ARCHIVE);

    m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    m_forward = Cvar_Get("m_forward", "0.25", CVAR_ARCHIVE | CVAR_USERSAVE);
    m_side = Cvar_Get("m_side", "0.25", CVAR_ARCHIVE | CVAR_USERSAVE);
    // [QL] the number of frames whose view angles are averaged, up to 31 (0x4b5640)
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

    // ~ and `, as keys and characters
    cl_consoleKeys = Cvar_Get("cl_consoleKeys", "~ ` 0x7e 0x60", CVAR_ARCHIVE);

    cl_steamId = Cvar_Get("cl_steamId", "0", CVAR_ARCHIVE);
    // [QL] 0x004e3f1a: quit when a demo finishes playing back
    Cvar_Get("cl_quitOnDemoCompleted", "0", 0);

    // userinfo
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
    // [QL] the country shown next to the name. It travels in the userinfo and the server
    // publishes it under the "c" key of the player's configstring; the cgame already caches
    // the flags from ui/country.txt. Empty means no flag.
    Cvar_Get("country", "", CVAR_USERINFO | CVAR_ARCHIVE);
    Cvar_Get("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE);

    Cvar_Get("password", "", CVAR_USERINFO);
    Cvar_Get("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE);

    // cgame might not be initialized before menu is used
    Cvar_Get("cg_viewsize", "100", CVAR_ARCHIVE);
    // Make sure cg_stereoSeparation is zero as that variable is deprecated and should not be used anymore.
    Cvar_Get("cg_stereoSeparation", "0", CVAR_ROM);

    //
    // register our commands
    //
    Cmd_AddCommand("cmd", CL_ForwardToServer_f);
    Cmd_AddCommand("configstrings", CL_Configstrings_f);
    Cmd_AddCommand("clientinfo", CL_Clientinfo_f);
    Cmd_AddCommand("snd_restart", CL_Snd_Restart_f);
    Cmd_AddCommand("vid_restart", CL_Vid_Restart_f);
    Cmd_AddCommand("disconnect", CL_Disconnect_f);
    Cmd_AddCommand("record", CL_Record_f);
    Cmd_AddCommand("demo", CL_PlayDemo_f);
    Cmd_SetCommandCompletionFunc("demo", CL_CompleteDemoName);
    // [ioquakelive] moving through a demo, and what its timeline is drawn from
    Cmd_AddCommand("rewind", CL_Rewind_f);
    Cmd_AddCommand("fastforward", CL_FastForward_f);
    Cmd_AddCommand("seekdemo", CL_SeekDemo_f);
    Cmd_AddCommand("fragforward", CL_FragForward_f);
    Cvar_Get("cl_demoLength", "0", CVAR_ROM);
    Cvar_Get("cl_demoPos", "0", CVAR_ROM);
    Cvar_Get("cl_demoMarks", "", CVAR_ROM);
    Cvar_Get("cl_demoFrags", "", CVAR_ROM);
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
    Cmd_AddCommand("+voice", CL_VoiceOn_f);   // [QL] 0x00461565
    Cmd_AddCommand("-voice", CL_VoiceOff_f);
    Cmd_AddCommand("showip", CL_ShowIP_f);
    Cmd_AddCommand("fs_openedList", CL_OpenedPK3List_f);
    Cmd_AddCommand("fs_referencedList", CL_ReferencedPK3List_f);
    Cmd_AddCommand("model", CL_SetModel_f);
    Cmd_AddCommand("web_changeHash", CL_Web_ChangeHash_f);
    Cmd_AddCommand("web_hideBrowser", CL_Web_HideBrowser_f);
    // [QL] additional client commands
    Cmd_AddCommand("postprocess_restart", CL_PostProcessRestart_f);
    Cmd_AddCommand("userinfo", CL_Userinfo_f);
    if (!com_dedicated->integer) {
        Cmd_AddCommand("sayto", CL_Sayto_f);
        Cmd_SetCommandCompletionFunc("sayto", CL_CompletePlayerName);
    }
    CL_InitAliases();

    CL_InitRef();

    SCR_Init();

    //	Cbuf_Execute ();

    Cvar_Set("cl_running", "1");

    Com_Printf("----- Client Initialization Complete -----\n");
}

/*
===============
CL_Shutdown

===============
*/
void CL_Shutdown(char* finalmsg, qboolean disconnect, qboolean quit) {
    static qboolean recursive = qfalse;

    // check whether the client is running at all.
    if (!(com_cl_running && com_cl_running->integer))
        return;

    Com_Printf("----- Client Shutdown (%s) -----\n", finalmsg);

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
            Q_strncpyz(server->hostName, Info_ValueForKey(info, "hostname"), MAX_NAME_LENGTH);
            Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"), MAX_NAME_LENGTH);
            server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
            Q_strncpyz(server->game, Info_ValueForKey(info, "game"), MAX_NAME_LENGTH);
            server->gameType = atoi(Info_ValueForKey(info, "gametype"));
            server->netType = atoi(Info_ValueForKey(info, "nettype"));
            server->minPing = atoi(Info_ValueForKey(info, "minPing"));  // [QL] binary uses camelCase
            server->maxPing = atoi(Info_ValueForKey(info, "maxPing"));  // [QL] binary uses camelCase
            server->g_humanplayers = atoi(Info_ValueForKey(info, "g_humanplayers"));
            server->g_needpass = atoi(Info_ValueForKey(info, "g_needpass"));
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

/*
===================
CL_ServerInfoPacket
===================
*/
void CL_ServerInfoPacket(netadr_t from, msg_t* msg) {
    int i, type;
    char info[MAX_INFO_STRING];
    char* infoString;
    int prot;
    char* gamename;
    qboolean gameMismatch;

    infoString = MSG_ReadString(msg);

    // if this isn't the correct gamename, ignore it
    gamename = Info_ValueForKey(infoString, "gamename");

    gameMismatch = !*gamename || strcmp(gamename, com_gamename->string) != 0;

    if (gameMismatch) {
        Com_DPrintf("Game mismatch in info packet: %s\n", infoString);
        return;
    }

    // if this isn't the correct protocol version, ignore it
    prot = atoi(Info_ValueForKey(infoString, "protocol"));

    if (prot != com_protocol->integer) {
        Com_DPrintf("Different protocol info packet: %s\n", infoString);
        return;
    }

    // iterate servers waiting for ping response
    for (i = 0; i < MAX_PINGREQUESTS; i++) {
        if (cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr(from, cl_pinglist[i].adr)) {
            // calc ping time
            cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
            Com_DPrintf("ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToString(from));

            // save of info
            Q_strncpyz(cl_pinglist[i].info, infoString, sizeof(cl_pinglist[i].info));

            // tack on the net type
            // NOTE: make sure these types are in sync with the netnames strings in the UI
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

    // if not just sent a local broadcast or pinging local servers
    if (cls.pingUpdateSource != AS_LOCAL) {
        return;
    }

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        // empty slot
        if (cls.localServers[i].adr.port == 0) {
            break;
        }

        // avoid duplicate
        if (NET_CompareAdr(from, cls.localServers[i].adr)) {
            return;
        }
    }

    if (i == MAX_OTHER_SERVERS) {
        Com_DPrintf("MAX_OTHER_SERVERS hit, dropping infoResponse\n");
        return;
    }

    // add this to the list
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

/*
===================
CL_GetServerStatus
===================
*/
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

/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus(char* serverAddress, char* serverStatusString, int maxLen) {
    int i;
    netadr_t to;
    serverStatus_t* serverStatus;

    // if no server address then reset all server status requests
    if (!serverAddress) {
        for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
            cl_serverStatusList[i].address.port = 0;
            cl_serverStatusList[i].retrieved = qtrue;
        }
        return qfalse;
    }
    // get the address
    if (!NET_StringToAdr(serverAddress, &to, NA_UNSPEC)) {
        return qfalse;
    }
    serverStatus = CL_GetServerStatus(to);
    // if no server status string then reset the server status request for this address
    if (!serverStatusString) {
        serverStatus->retrieved = qtrue;
        return qfalse;
    }

    // if this server status request has the same address
    if (NET_CompareAdr(to, serverStatus->address)) {
        // if we received a response for this server status request
        if (!serverStatus->pending) {
            Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
            serverStatus->retrieved = qtrue;
            serverStatus->startTime = 0;
            return qtrue;
        }
        // resend the request regularly
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
    // if retrieved
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

/*
===================
CL_ServerStatusResponse
===================
*/
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
    // if we didn't request this server status
    if (!serverStatus) {
        return;
    }

    s = MSG_ReadStringLine(msg);

    len = 0;
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string) - len, "%s", s);

    if (serverStatus->print) {
        Com_Printf("Server settings:\n");
        // print cvars
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

/*
==================
CL_GlobalServers_f

[QL] 0x00462eb0. Quake Live has no master of its own and sends "getservers" nowhere:
it asks Steam for the list, with a single filter ("gamedir" = "baseq3"). The first
argument is where it wants the list from, as in retail: 0 internet, 1 LAN, 2 friends,
3 favourites, 4 history. The remaining arguments -- protocol and keywords -- are
accepted so as not to break anyone who sends them, but Steam does not use them.
==================
*/
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

    // the list is rebuilt: the visible flag belongs to the interface and is left alone
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

/*
===============================================================================

VOICE

Quake Live does not send voice over its own network: it sends it over Steam's peer to
peer network and compresses and decompresses it with Steam's voice codec. The client
only talks to the server, which does the handing out.

  0x004603f0  "+voice" starts recording (it lives in steam.c, which is where the
              Steam interface lives).
  0x00460d10  every frame while recording: gathers what is compressed and sends it
              to the server's Steam ID, which comes from configstring 714.
  0x00461a60  every frame on receive: the first byte is the speaker's number and
              the rest is voice, which goes to the mixer at 22050 Hz.

===============================================================================
*/

// the most retail gathers in one go, and the most that comes out of decompressing it
#define CL_VOICE_MAX_COMPRESSED 0x4000
#define CL_VOICE_MAX_PCM 0x8000

/*
===============
The mute list

It goes by Steam ID and not by client number, so muting somebody mutes them on the
next map and the next server too. Retail keeps it in a sorted set (0x00461990 queries
it, 0x00461c00 changes it) and offers it to the cgame as two calls: ask and toggle.
===============
*/
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

// [QL] 0x00461c00: devuelve si quedo silenciado
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

/*
===============
CL_VoiceIgnored / CL_VoiceTalking

The two bridges to the cgame, which is what knows each player's Steam ID and what
draws who is talking.

[QL] 0x00461b6a and 0x00461b80: entries 15 and 20 of the cgame table.
===============
*/
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

/*
===============
CL_ServerSteamID

The Steam ID of the server we are connected to, which the server publishes in
configstring 714. Without it there is nobody to send voice to.
===============
*/
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

/*
===============
CL_Friends_f

"friends" prints the Steam friends list: who is online, what they are doing and,
when they are playing this, which server they are on. In retail the list was a web
page, so there is nothing to mirror: the data comes from Steam and the shape is
ours.
===============
*/
/*
===============
CL_FriendByNumber

The friend in that position of the online list, which is the numbering the player has
in front of them.
===============
*/
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
            continue;  // offline friends are not listed
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

/*
===============
CL_FriendDo

What can be done with a friend: join their server, open the Steam window to add them,
or view their profile. The console command and the menu's social strip both call it,
being two ways of asking for the same thing.
===============
*/
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
        case UI_FRIEND_INVITE:
            Steam_ShowFriendAdd(fr->steamId);
            return;
        case UI_FRIEND_PROFILE:
            Steam_ShowProfile(fr->steamId);
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

/*
===============
CL_ClientFriendInvite_f / CL_ClientViewProfile_f

The Steam overlay over another player in the match, to add them or to view their
profile. The client number is turned into a Steam ID by asking the cgame. [QL]
both commands are in the retail engine, next to their dialogs' names (0x005327f0).
===============
*/
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

/*
===============
CL_VoiceOn_f / CL_VoiceOff_f

[QL] 0x004603f0 and 0x00460490. Besides starting and stopping the recording, they tell
the cgame that you yourself are talking, which is what lights your own indicator without
waiting for the voice to come back from the server.
===============
*/
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

/*
===============
CL_VoiceFrame

[QL] 0x00460d10 y 0x00461a60.
===============
*/
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

    // what the microphone gathered goes to the server as it is
    if (Steam_VoiceRecording()) {
        bytes = Steam_VoiceCapture(compressed, sizeof(compressed));
        if (bytes > 0 && server) {
            Steam_P2PSend(server, compressed, bytes, qfalse);
        }
    }

    // the greeting that keeps the session open in both directions. Retail only sends it from
    // the server side (0x004668ca); without the client's, Steam drops incoming voice until you
    // speak for the first time.
    if (server && (cls.realtime - lastHello > 10000 || cls.realtime < lastHello)) {
        lastHello = cls.realtime;
        Steam_P2PHello(server, qfalse);
    }

    // and what arrives is everyone else's voice, with the speaker's number in front
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

/*
==================
CL_SteamServersFrame

Steam answers server by server as the search goes on. What arrives is copied into the
list the interface watches, with the ping Steam measured, so there is no need to ask each
server anything: ioquake3's "getinfo" does not exist in Quake Live.
==================
*/
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

        // the same server can arrive twice if the search is retried
        for (j = 0; j < cls.numglobalservers; j++) {
            if (NET_CompareAdr(cls.globalServers[j].adr, adr)) {
                break;
            }
        }
        if (j < cls.numglobalservers) {
            // already there: it takes Steam's latest word and keeps its place
            CL_SetServerInfo(&cls.globalServers[j], info, ping);
            continue;
        }

        server = &cls.globalServers[cls.numglobalservers++];
        CL_InitServerInfo(server, &adr);
        CL_SetServerInfo(server, info, ping);
    }

    // the closing message, once only, like the one ioquake3's master printed
    if (cl_steamServersPending && !Steam_ServerListRefreshing()) {
        cl_steamServersPending = qfalse;
        Com_Printf("%d servers parsed\n", cls.numglobalservers);
    }
}

/*
==================
CL_LocalServers_f
==================
*/
void CL_LocalServers_f(void) {
    char* message;
    int i, j;
    netadr_t to;

    Com_Printf("Scanning for servers on the local network...\n");

    // reset the list, waiting for response
    cls.numlocalservers = 0;
    cls.pingUpdateSource = AS_LOCAL;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        qboolean b = cls.localServers[i].visible;
        Com_Memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
        cls.localServers[i].visible = b;
    }
    Com_Memset(&to, 0, sizeof(to));

    // The 'xxx' in the message is a challenge that will be echoed back
    // by the server.  We don't care about that here, but master servers
    // can use that to prevent spoofed server responses from invalid ip
    message = "\377\377\377\377getinfo xxx";

    // send each message twice in case one is dropped
    for (i = 0; i < 2; i++) {
        // send a broadcast packet on each server port
        // we support multiple server ports so a single machine
        // can nicely run multiple servers
        for (j = 0; j < NUM_SERVER_PORTS; j++) {
            to.port = BigShort((short)(PORT_SERVER + j));

            to.type = NA_BROADCAST;
            NET_SendPacket(NS_CLIENT, strlen(message), message, to);
            to.type = NA_MULTICAST6;
            NET_SendPacket(NS_CLIENT, strlen(message), message, to);
        }
    }
}

/*
==================
CL_GetPing
==================
*/
void CL_GetPing(int n, char* buf, int buflen, int* pingtime) {
    const char* str;
    int time;
    int maxPing;

    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
        // empty or invalid slot
        buf[0] = '\0';
        *pingtime = 0;
        return;
    }

    str = NET_AdrToStringwPort(cl_pinglist[n].adr);
    Q_strncpyz(buf, str, buflen);

    time = cl_pinglist[n].time;
    if (!time) {
        // check for timeout
        time = Sys_Milliseconds() - cl_pinglist[n].start;
        maxPing = Cvar_VariableIntegerValue("cl_maxPing");
        if (maxPing < 100) {
            maxPing = 100;
        }
        if (time < maxPing) {
            // not timed out yet
            time = 0;
        }
    }

    CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

    *pingtime = time;
}

/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo(int n, char* buf, int buflen) {
    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
        // empty or invalid slot
        if (buflen)
            buf[0] = '\0';
        return;
    }

    Q_strncpyz(buf, cl_pinglist[n].info, buflen);
}

/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing(int n) {
    if (n < 0 || n >= MAX_PINGREQUESTS)
        return;

    cl_pinglist[n].adr.port = 0;
}

/*
==================
CL_GetPingQueueCount
==================
*/
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

/*
==================
CL_GetFreePing
==================
*/
ping_t* CL_GetFreePing(void) {
    ping_t* pingptr;
    ping_t* best;
    int oldest;
    int i;
    int time;

    pingptr = cl_pinglist;
    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        // find free ping slot
        if (pingptr->adr.port) {
            if (!pingptr->time) {
                if (Sys_Milliseconds() - pingptr->start < 500) {
                    // still waiting for response
                    continue;
                }
            } else if (pingptr->time < 500) {
                // results have not been queried
                continue;
            }
        }

        // clear it
        pingptr->adr.port = 0;
        return (pingptr);
    }

    // use oldest entry
    pingptr = cl_pinglist;
    best = cl_pinglist;
    oldest = INT_MIN;
    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        // scan for oldest
        time = Sys_Milliseconds() - pingptr->start;
        if (time > oldest) {
            oldest = time;
            best = pingptr;
        }
    }

    return (best);
}

/*
==================
CL_UpdateVisiblePings_f
==================
*/
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
                            // already on the list
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
                // if the server has a ping higher than cl_maxPing or
                // the ping packet got lost
                else if (server[i].ping == 0) {
                    // if we are updating global servers
                    if (source == AS_GLOBAL) {
                        //
                        if (cls.numGlobalServerAddresses > 0) {
                            // overwrite this server with one from the additional global servers
                            cls.numGlobalServerAddresses--;
                            CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
                            // NOTE: the server[i].visible flag stays untouched
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

/*
==================
CL_ServerStatus_f
==================
*/
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

/*
==================
CL_ShowIP_f
==================
*/
void CL_ShowIP_f(void) {
    Sys_ShowIP();
}
