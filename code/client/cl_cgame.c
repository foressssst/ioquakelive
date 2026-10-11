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

#include "../botlib/botlib.h"

extern botlib_export_t* botlib_export;

extern qboolean loadCamera(const char* name);
extern void startCamera(int time);
extern qboolean getCameraInfo(int time, vec3_t* origin, vec3_t* angles);

void CL_GetGameState(gameState_t* gs) {
    *gs = cl.gameState;
}

void CL_GetGlconfig(glconfig_t* glconfig) {
    *glconfig = cls.glconfig;
}

qboolean CL_GetUserCmd(int cmdNumber, usercmd_t* ucmd) {

    if (cmdNumber > cl.cmdNumber) {
        Com_Error(ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber);
    }

    if (cmdNumber <= cl.cmdNumber - CMD_BACKUP) {
        return qfalse;
    }

    *ucmd = cl.cmds[cmdNumber & CMD_MASK];

    return qtrue;
}

int CL_GetCurrentCmdNumber(void) {
    return cl.cmdNumber;
}

qboolean CL_GetParseEntityState(int parseEntityNumber, entityState_t* state) {
    if (parseEntityNumber >= cl.parseEntitiesNum) {
        Com_Error(ERR_DROP, "CL_GetParseEntityState: %i >= %i",
                  parseEntityNumber, cl.parseEntitiesNum);
    }

    if (parseEntityNumber <= cl.parseEntitiesNum - MAX_PARSE_ENTITIES) {
        return qfalse;
    }

    *state = cl.parseEntities[parseEntityNumber & (MAX_PARSE_ENTITIES - 1)];
    return qtrue;
}

void CL_GetCurrentSnapshotNumber(int* snapshotNumber, int* serverTime) {
    *snapshotNumber = cl.snap.messageNum;
    *serverTime = cl.snap.serverTime;
}

qboolean CL_GetSnapshot(int snapshotNumber, snapshot_t* snapshot) {
    clSnapshot_t* clSnap;
    int i, count;

    if (snapshotNumber > cl.snap.messageNum) {
        Com_Error(ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.snapshot.messageNum");
    }

    if (cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP) {
        return qfalse;
    }

    clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
    if (!clSnap->valid) {
        return qfalse;
    }

    if (cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES) {
        return qfalse;
    }

    snapshot->snapFlags = clSnap->snapFlags;
    snapshot->serverCommandSequence = clSnap->serverCommandNum;
    snapshot->ping = clSnap->ping;
    snapshot->serverTime = clSnap->serverTime;
    Com_Memcpy(snapshot->areamask, clSnap->areamask, sizeof(snapshot->areamask));
    snapshot->ps = clSnap->ps;
    count = clSnap->numEntities;
    if (count > MAX_ENTITIES_IN_SNAPSHOT) {
        Com_DPrintf("CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT);
        count = MAX_ENTITIES_IN_SNAPSHOT;
    }
    snapshot->numEntities = count;
    for (i = 0; i < count; i++) {
        snapshot->entities[i] =
            cl.parseEntities[(clSnap->parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
    }

    return qtrue;
}

void CL_SetUserCmdValue(int userCmdValue, int weaponPrimary, float sensitivityScale, int fov) {
    cl.cgameUserCmdValue = userCmdValue;
    cl.cgameUserCmdWeaponPrimary = weaponPrimary;
    cl.cgameSensitivity = sensitivityScale;
    cl.cgameUserCmdFov = fov;
}

void CL_AddCgameCommand(const char* cmdName) {
    Cmd_AddCommand(cmdName, NULL);
    CL_SetCgameCommandCompletion(cmdName);
}

void CL_ConfigstringModified(void) {
    char *old, *s;
    int i, index;
    char* dup;
    gameState_t oldGs;
    int len;

    index = atoi(Cmd_Argv(1));
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP, "CL_ConfigstringModified: bad index %i", index);
    }
    s = Cmd_ArgsFrom(2);

    old = cl.gameState.stringData + cl.gameState.stringOffsets[index];
    if (!strcmp(old, s)) {
        return;
    }

    oldGs = cl.gameState;

    Com_Memset(&cl.gameState, 0, sizeof(cl.gameState));

    cl.gameState.dataCount = 1;

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (i == index) {
            dup = s;
        } else {
            dup = oldGs.stringData + oldGs.stringOffsets[i];
        }
        if (!dup[0]) {
            continue;
        }

        len = strlen(dup);

        if (len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS) {
            Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded");
        }

        cl.gameState.stringOffsets[i] = cl.gameState.dataCount;
        Com_Memcpy(cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1);
        cl.gameState.dataCount += len + 1;
    }

    if (index == CS_SYSTEMINFO) {
        CL_SystemInfoChanged();
    }
    if (index >= CS_PLAYERS && index < CS_PLAYERS + MAX_CLIENTS) {
        CL_WarmPlayersChanged();
    }
}

qboolean CL_GetServerCommand(int serverCommandNumber) {
    char* s;
    char* cmd;
    static char bigConfigString[BIG_INFO_STRING];
    int argc;

    if (serverCommandNumber <= clc.serverCommandSequence - MAX_RELIABLE_COMMANDS) {
        if (clc.demoplaying)
            return qfalse;
        Com_Error(ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out");
        return qfalse;
    }

    if (serverCommandNumber > clc.serverCommandSequence) {
        Com_Error(ERR_DROP, "CL_GetServerCommand: requested a command not received");
        return qfalse;
    }

    s = clc.serverCommands[serverCommandNumber & (MAX_RELIABLE_COMMANDS - 1)];
    clc.lastExecutedServerCommand = serverCommandNumber;

    Com_DPrintf("serverCommand: %i : %s\n", serverCommandNumber, s);

rescan:
    Cmd_TokenizeString(s);
    cmd = Cmd_Argv(0);
    argc = Cmd_Argc();

    if (!strcmp(cmd, "disconnect")) {
        if (argc >= 2)
            Com_Error(ERR_SERVERDISCONNECT, "Server disconnected - %s", Cmd_Argv(1));
        else
            Com_Error(ERR_SERVERDISCONNECT, "Server disconnected");
    }

    if (!strcmp(cmd, "bcs0")) {
        Com_sprintf(bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2));
        return qfalse;
    }

    if (!strcmp(cmd, "bcs1")) {
        s = Cmd_Argv(2);
        if (strlen(bigConfigString) + strlen(s) >= BIG_INFO_STRING) {
            Com_Error(ERR_DROP, "bcs exceeded BIG_INFO_STRING");
        }
        strcat(bigConfigString, s);
        return qfalse;
    }

    if (!strcmp(cmd, "bcs2")) {
        s = Cmd_Argv(2);
        if (strlen(bigConfigString) + strlen(s) + 1 >= BIG_INFO_STRING) {
            Com_Error(ERR_DROP, "bcs exceeded BIG_INFO_STRING");
        }
        strcat(bigConfigString, s);
        strcat(bigConfigString, "\"");
        s = bigConfigString;
        goto rescan;
    }

    if (!strcmp(cmd, "cs")) {
        CL_ConfigstringModified();
        Cmd_TokenizeString(s);
        return qtrue;
    }

    if (!strcmp(cmd, "map_restart")) {
        Con_ClearNotify();
        Cmd_TokenizeString(s);
        Com_Memset(cl.cmds, 0, sizeof(cl.cmds));
        return qtrue;
    }

    if (!strcmp(cmd, "clientLevelShot")) {
        if (!com_sv_running->integer) {
            return qfalse;
        }
        Con_Close();
        Cbuf_AddText("wait ; wait ; wait ; wait ; screenshot levelshot\n");
        return qtrue;
    }

    return qtrue;
}

void CL_CM_LoadMap(const char* mapname) {
    int checksum;

    CM_LoadMap(mapname, qtrue, &checksum);
}

void CL_ShutdownCGame(void) {
    Key_SetCatcher(Key_GetCatcher() & ~KEYCATCH_CGAME);
    cls.cgameStarted = qfalse;
    if (!cgvm) {
        return;
    }
    VM_Call(cgvm, CG_SHUTDOWN);
    VM_Free(cgvm);
    cgvm = NULL;
}

static int FloatAsInt(float f) {
    floatint_t fi;
    fi.f = f;
    return fi.i;
}

typedef enum {
    CL_LOAD_SHADERS,
    CL_LOAD_MODELS,
    CL_LOAD_SOUNDS,
    CL_LOAD_WORLD,
    CL_LOAD_FILES,
    CL_LOAD_OTHER,
    CL_LOAD_CATEGORIES
} clLoadCategory_t;

static const char* const cl_loadNames[CL_LOAD_CATEGORIES] = {"textures/shaders", "models/skins",
                                                             "sounds", "world", "files", "other"};
static int64_t cl_loadUs[CL_LOAD_CATEGORIES];
static int cl_loadCalls[CL_LOAD_CATEGORIES];
static qboolean cl_loadProfiling;

static clLoadCategory_t CL_LoadCategory(intptr_t trap) {
    switch (trap) {
        case CG_R_REGISTERSHADER:
        case CG_R_REGISTERSHADERNOMIP:
            return CL_LOAD_SHADERS;
        case CG_R_REGISTERMODEL:
        case CG_R_REGISTERSKIN:
            return CL_LOAD_MODELS;
        case CG_S_REGISTERSOUND:
            return CL_LOAD_SOUNDS;
        case CG_R_LOADWORLDMAP:
            return CL_LOAD_WORLD;
        case CG_FS_FOPENFILE:
        case CG_FS_READ:
        case CG_FS_FCLOSEFILE:
        case CG_FS_GETFILELIST:
            return CL_LOAD_FILES;
        default:
            return CL_LOAD_OTHER;
    }
}

static void CL_LoadProfileReport(float total) {
    int64_t sum = 0;
    int i;

    for (i = 0; i < CL_LOAD_CATEGORIES; i++) {
        sum += cl_loadUs[i];
    }
    Com_DPrintf("load: %.2f s, of which the cgame asked the engine for %.2f s\n", total,
               sum / 1000000.0f);
    for (i = 0; i < CL_LOAD_CATEGORIES; i++) {
        if (cl_loadUs[i] < 1000) {
            continue;
        }
        Com_DPrintf("  %-17s %6.2f s   %5i calls\n", cl_loadNames[i], cl_loadUs[i] / 1000000.0f,
                   cl_loadCalls[i]);
    }
}

static intptr_t CL_CgameSystemCallsInner(intptr_t* args);

intptr_t CL_CgameSystemCalls(intptr_t* args) {
    if (cl_loadProfiling) {
        clLoadCategory_t category = CL_LoadCategory(args[0]);
        int64_t start = Sys_Microseconds();
        intptr_t result = CL_CgameSystemCallsInner(args);

        cl_loadUs[category] += Sys_Microseconds() - start;
        cl_loadCalls[category]++;
        return result;
    }
    return CL_CgameSystemCallsInner(args);
}

static intptr_t CL_CgameSystemCallsInner(intptr_t* args) {
    switch (args[0]) {
        case CG_PRINT:
            Com_Printf("%s", (const char*)VMA(1));
            return 0;
        case CG_ERROR:
            Com_Error(ERR_DROP, "%s", (const char*)VMA(1));
            return 0;
        case CG_MILLISECONDS:
            return Sys_Milliseconds();
        case CG_CVAR_REGISTER:
            Cvar_Register(VMA(1), VMA(2), VMA(3), args[4]);
            return 0;
        case CG_CVAR_REGISTER_RANGE:
            Cvar_RegisterRange(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6]);
            return 0;
        case CG_CVAR_UPDATE:
            Cvar_Update(VMA(1));
            return 0;
        case CG_CVAR_SET:
            Cvar_SetSafe(VMA(1), VMA(2));
            return 0;
        case CG_CVAR_VARIABLESTRINGBUFFER:
            Cvar_VariableStringBuffer(VMA(1), VMA(2), args[3]);
            return 0;
        case CG_ARGC:
            return Cmd_Argc();
        case CG_ARGV:
            Cmd_ArgvBuffer(args[1], VMA(2), args[3]);
            return 0;
        case CG_ARGS:
            Cmd_ArgsBuffer(VMA(1), args[2]);
            return 0;
        case CG_FS_FOPENFILE:
            return FS_FOpenFileByMode(VMA(1), VMA(2), args[3]);
        case CG_FS_READ:
            FS_Read(VMA(1), args[2], args[3]);
            return 0;
        case CG_FS_WRITE:
            FS_Write(VMA(1), args[2], args[3]);
            return 0;
        case CG_FS_FCLOSEFILE:
            FS_FCloseFile(args[1]);
            return 0;
        case CG_FS_SEEK:
            return FS_Seek(args[1], args[2], args[3]);
        case CG_SENDCONSOLECOMMAND:
            Cbuf_AddText(VMA(1));
            return 0;
        case CG_ADDCOMMAND:
            CL_AddCgameCommand(VMA(1));
            return 0;
        case CG_REMOVECOMMAND:
            Cmd_RemoveCommandSafe(VMA(1));
            return 0;
        case CG_SENDCLIENTCOMMAND:
            CL_AddReliableCommand(VMA(1), qfalse);
            return 0;
        case CG_UPDATESCREEN:
            SCR_UpdateScreen();
            return 0;
        case CG_CM_LOADMAP:
            CL_CM_LoadMap(VMA(1));
            return 0;
        case CG_CM_NUMINLINEMODELS:
            return CM_NumInlineModels();
        case CG_CM_INLINEMODEL:
            return CM_InlineModel(args[1]);
        case CG_CM_TEMPBOXMODEL:
            return CM_TempBoxModel(VMA(1), VMA(2),  qfalse);
        case CG_CM_TEMPCAPSULEMODEL:
            return CM_TempBoxModel(VMA(1), VMA(2),  qtrue);
        case CG_CM_POINTCONTENTS:
            return CM_PointContents(VMA(1), args[2]);
        case CG_CM_TRANSFORMEDPOINTCONTENTS:
            return CM_TransformedPointContents(VMA(1), args[2], VMA(3), VMA(4));
        case CG_CM_BOXTRACE:
            CM_BoxTrace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7],  qfalse);
            return 0;
        case CG_CM_CAPSULETRACE:
            CM_BoxTrace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7],  qtrue);
            return 0;
        case CG_CM_TRANSFORMEDBOXTRACE:
            CM_TransformedBoxTrace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9),  qfalse);
            return 0;
        case CG_CM_TRANSFORMEDCAPSULETRACE:
            CM_TransformedBoxTrace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9),  qtrue);
            return 0;
        case CG_CM_MARKFRAGMENTS:
            return re.MarkFragments(args[1], VMA(2), VMA(3), args[4], VMA(5), args[6], VMA(7));
        case CG_S_STARTSOUND:
            S_StartSound(VMA(1), args[2], args[3], args[4]);
            return 0;
        case CG_S_STARTLOCALSOUND:
            S_StartLocalSound(args[1], args[2]);
            return 0;
        case CG_S_CLEARLOOPINGSOUNDS:
            S_ClearLoopingSounds(args[1]);
            return 0;
        case CG_S_ADDLOOPINGSOUND:
            S_AddLoopingSound(args[1], VMA(2), VMA(3), args[4]);
            return 0;
        case CG_S_ADDREALLOOPINGSOUND:
            S_AddRealLoopingSound(args[1], VMA(2), VMA(3), args[4]);
            return 0;
        case CG_S_STOPLOOPINGSOUND:
            S_StopLoopingSound(args[1]);
            return 0;
        case CG_S_UPDATEENTITYPOSITION:
            S_UpdateEntityPosition(args[1], VMA(2));
            return 0;
        case CG_S_RESPATIALIZE:
            S_Respatialize(args[1], VMA(2), VMA(3), args[4]);
            return 0;
        case CG_S_REGISTERSOUND:
            return S_RegisterSound(VMA(1), args[2]);
        case CG_S_STARTBACKGROUNDTRACK:
            S_StartBackgroundTrack(VMA(1), VMA(2));
            return 0;
        case CG_R_LOADWORLDMAP:
            re.LoadWorld(VMA(1));
            return 0;
        case CG_R_REGISTERMODEL:
            return re.RegisterModel(VMA(1));
        case CG_R_REGISTERSKIN:
            return re.RegisterSkin(VMA(1));
        case CG_R_REGISTERSHADER:
            return re.RegisterShader(VMA(1));
        case CG_R_REGISTERSHADERNOMIP:
            return re.RegisterShaderNoMip(VMA(1));
        case CG_R_REGISTERFONT:
            re.RegisterFont(VMA(1), args[2], VMA(3));
            return 0;
        case CG_R_FONT_DRAWSTRING:
            re.Font_DrawString(args[1], args[2], VMA(3), args[4], VMF(5), args[6], VMA(7), args[8]);
            return 0;
        case CG_R_FONT_TEXTEXTENTS:
            re.TextBounds(VMA(1), args[2], args[3], VMF(4), args[5], VMA(6));
            return 0;
        case CG_R_GETGLYPHINFO:
            re.GetGlyphInfo(args[1], args[2], VMA(3));
            return 0;
        case CG_IME_SETCOMPOSITIONFONT:
            re.SetCompositionFont(args[1], VMF(2));
            return 0;
        case CG_R_FONT_REGISTER:
            return re.Font_Register(VMA(1));
        case CG_S_STARTLOCALSOUNDVOLUME:
            S_StartLocalSoundVolume(args[1], args[2], VMF(3));
            return 0;
        case CG_FS_GETFILELIST:
            return FS_GetFileList(VMA(1), VMA(2), VMA(3), args[4]);
        case CG_S_CLEARSOUNDBUFFER:
            S_ClearSoundBuffer();
            return 0;
        case CG_KEY_GETBINDINGBUF: {
            const char* binding = Key_GetBinding(args[1]);
            Q_strncpyz(VMA(2), binding ? binding : "", args[3]);
            return 0;
        }
        case CG_R_CLEARSCENE:
            re.ClearScene();
            return 0;
        case CG_R_ADDREFENTITYTOSCENE:
            re.AddRefEntityToScene(VMA(1));
            return 0;
        case CG_R_ADDPOLYTOSCENE:
            re.AddPolyToScene(args[1], args[2], VMA(3), 1);
            return 0;
        case CG_R_ADDPOLYSTOSCENE:
            re.AddPolyToScene(args[1], args[2], VMA(3), args[4]);
            return 0;
        case CG_R_LIGHTFORPOINT:
            return re.LightForPoint(VMA(1), VMA(2), VMA(3), VMA(4));
        case CG_R_ADDLIGHTTOSCENE:
            re.AddLightToScene(VMA(1), VMF(2), VMF(3), VMF(4), VMF(5));
            return 0;
        case CG_R_ADDADDITIVELIGHTTOSCENE:
            re.AddAdditiveLightToScene(VMA(1), VMF(2), VMF(3), VMF(4), VMF(5));
            return 0;
        case CG_R_RENDERSCENE:
            re.RenderScene(VMA(1));
            return 0;
        case CG_R_BEGINBLOOMSCENE:
            re.BeginBloomScene();
            return 0;
        case CG_R_APPLYBLOOM:
            re.ApplyBloom();
            return 0;
        case CG_GET_ADVERTISEMENTS:
            re.Get_Advertisements(VMA(1), VMA(2), VMA(3));
            return 0;
        case CG_R_SETCOLOR:
            re.SetColor(VMA(1));
            return 0;
        case CG_R_DRAWSTRETCHPIC:
            re.DrawStretchPic(VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9]);
            return 0;
        case CG_R_MODELBOUNDS:
            re.ModelBounds(args[1], VMA(2), VMA(3));
            return 0;
        case CG_R_LERPTAG:
            return re.LerpTag(VMA(1), args[2], args[3], args[4], VMF(5), VMA(6));
        case CG_GETGLCONFIG:
            CL_GetGlconfig(VMA(1));
            return 0;
        case CG_GETGAMESTATE:
            CL_GetGameState(VMA(1));
            return 0;
        case CG_GETCURRENTSNAPSHOTNUMBER:
            CL_GetCurrentSnapshotNumber(VMA(1), VMA(2));
            return 0;
        case CG_GETSNAPSHOT:
            return CL_GetSnapshot(args[1], VMA(2));
        case CG_GETSERVERCOMMAND:
            return CL_GetServerCommand(args[1]);
        case CG_GETCURRENTCMDNUMBER:
            return CL_GetCurrentCmdNumber();
        case CG_GETUSERCMD:
            return CL_GetUserCmd(args[1], VMA(2));
        case CG_SETUSERCMDVALUE:
            CL_SetUserCmdValue(args[1], args[2], VMF(3), args[4]);
            return 0;
        case CG_MEMORY_REMAINING:
            return Hunk_MemoryRemaining();
        case CG_KEY_ISDOWN:
            return Key_IsDown(args[1]);
        case CG_KEY_GETCATCHER:
            return Key_GetCatcher();
        case CG_KEY_SETCATCHER:
            Key_SetCatcher(args[1]);
            return 0;
        case CG_KEY_GETKEY:
            return Key_GetKey(VMA(1));

        case CG_KEY_KEYNUMTOSTRINGBUF:
            Q_strncpyz(VMA(2), Key_KeynumToString(args[1]), args[3]);
            return 0;

        case CG_VOICE_IS_IGNORED:
            return CL_VoiceIsIgnored(((uint64_t)(unsigned int)args[2] << 32) | (unsigned int)args[1]);
        case CG_VOICE_TOGGLE_IGNORE:
            return CL_VoiceToggleIgnore(((uint64_t)(unsigned int)args[2] << 32) | (unsigned int)args[1]);

        case CG_MEMSET:
            Com_Memset(VMA(1), args[2], args[3]);
            return 0;
        case CG_MEMCPY:
            Com_Memcpy(VMA(1), VMA(2), args[3]);
            return 0;
        case CG_STRNCPY:
            strncpy(VMA(1), VMA(2), args[3]);
            return args[1];
        case CG_SIN:
            return FloatAsInt(sin(VMF(1)));
        case CG_COS:
            return FloatAsInt(cos(VMF(1)));
        case CG_ATAN2:
            return FloatAsInt(atan2(VMF(1), VMF(2)));
        case CG_SQRT:
            return FloatAsInt(sqrt(VMF(1)));
        case CG_FLOOR:
            return FloatAsInt(floor(VMF(1)));
        case CG_CEIL:
            return FloatAsInt(ceil(VMF(1)));
        case CG_ACOS:
            return FloatAsInt(Q_acos(VMF(1)));

        case CG_PC_ADD_GLOBAL_DEFINE:
            return botlib_export->PC_AddGlobalDefine(VMA(1));
        case CG_PC_LOAD_SOURCE:
            return botlib_export->PC_LoadSourceHandle(VMA(1));
        case CG_PC_FREE_SOURCE:
            return botlib_export->PC_FreeSourceHandle(args[1]);
        case CG_PC_READ_TOKEN:
            return botlib_export->PC_ReadTokenHandle(args[1], VMA(2));
        case CG_PC_SOURCE_FILE_AND_LINE:
            return botlib_export->PC_SourceFileAndLine(args[1], VMA(2), VMA(3));

        case CG_S_STOPBACKGROUNDTRACK:
            S_StopBackgroundTrack();
            return 0;

        case CG_REAL_TIME:
            return Com_RealTime(VMA(1));
        case CG_SNAPVECTOR:
            Q_SnapVector(VMA(1));
            return 0;

        case CG_CIN_PLAYCINEMATIC:
            return CIN_PlayCinematic(VMA(1), args[2], args[3], args[4], args[5], args[6]);

        case CG_CIN_STOPCINEMATIC:
            return CIN_StopCinematic(args[1]);

        case CG_CIN_RUNCINEMATIC:
            return CIN_RunCinematic(args[1]);

        case CG_CIN_DRAWCINEMATIC:
            CIN_DrawCinematic(args[1]);
            return 0;

        case CG_CIN_SETEXTENTS:
            CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
            return 0;

        case CG_R_REMAP_SHADER:
            re.RemapShader(VMA(1), VMA(2), VMA(3));
            return 0;

        case CG_GET_ENTITY_TOKEN:
            return re.GetEntityToken(VMA(1), args[2]);
        case CG_R_INPVS:
            return re.inPVS(VMA(1), VMA(2));

        default:
            assert(0);
            Com_Error(ERR_DROP, "Bad cgame system trap: %ld", (long int)args[0]);
    }
    return 0;
}

void CL_RegisterCGameCvars(void) {
    cgvm = VM_Create("cgame", CL_CgameSystemCalls);
    if (!cgvm) {
        Com_Printf("Couldn't load cgame VM during cvar registration.\n");
        return;
    }
    VM_Call(cgvm, CG_REGISTER_CVARS);
    VM_Free(cgvm);
    cgvm = NULL;
}

void CL_InitCGame(void) {
    const char* info;
    const char* mapname;
    int t1, t2;

    t1 = Sys_Milliseconds();

    Com_Memset(cl_loadUs, 0, sizeof(cl_loadUs));
    Com_Memset(cl_loadCalls, 0, sizeof(cl_loadCalls));
    cl_loadProfiling = qtrue;

    S_AsyncLoadBegin();

    Con_Close();

    info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
    mapname = Info_ValueForKey(info, "mapname");
    Com_sprintf(cl.mapname, sizeof(cl.mapname), "maps/%s.bsp", mapname);

    cgvm = VM_Create("cgame", CL_CgameSystemCalls);
    if (!cgvm) {
        Com_Error(ERR_DROP, "VM_Create on cgame failed");
    }
    clc.state = CA_LOADING;

    VM_Call(cgvm, CG_REGISTER_CVARS);

    VM_Call(cgvm, CG_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum);

    if (!clc.demoplaying && !cl_connectedToCheatServer)
        Cvar_SetCheatState();

    clc.state = CA_PRIMED;

    t2 = Sys_Milliseconds();

    S_AsyncLoadFinish();
    t2 = Sys_Milliseconds();

    cl_loadProfiling = qfalse;
    Com_Printf("CL_InitCGame: %5.2f seconds\n", (t2 - t1) / 1000.0);
    CL_LoadProfileReport((t2 - t1) / 1000.0f);

    re.EndRegistration();

    if (!Sys_LowPhysicalMemory()) {
        Com_TouchMemory();
    }

    Con_ClearNotify();
}

qboolean CL_GameCommand(void) {
    if (!cgvm) {
        return qfalse;
    }

    return VM_Call(cgvm, CG_CONSOLE_COMMAND);
}

void CL_CGameRendering(stereoFrame_t stereo) {
    VM_Call(cgvm, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereo, clc.demoplaying);
    VM_Debug(0);
}

#define RESET_TIME 500

void CL_AdjustTimeDelta(void) {
    int newDelta;
    int deltaDelta;

    cl.newSnapshots = qfalse;

    if (clc.demoplaying) {
        return;
    }

    newDelta = cl.snap.serverTime - cls.realtime;
    deltaDelta = abs(newDelta - cl.serverTimeDelta);

    if (deltaDelta > RESET_TIME) {
        cl.serverTimeDelta = newDelta;
        cl.oldServerTime = cl.snap.serverTime;
        cl.serverTime = cl.snap.serverTime;
        if (cl_showTimeDelta->integer) {
            Com_Printf("<RESET> ");
        }
    } else if (deltaDelta > 100) {
        if (cl_showTimeDelta->integer) {
            Com_Printf("<FAST> ");
        }
        cl.serverTimeDelta = (cl.serverTimeDelta + newDelta) >> 1;
    } else {

        if (com_timescale->value == 0 || com_timescale->value == 1) {
            if (cl.extrapolatedSnapshot) {
                cl.extrapolatedSnapshot = qfalse;
                cl.serverTimeDelta -= 2;
            } else {
                cl.serverTimeDelta++;
            }
        }
    }

    if (cl_showTimeDelta->integer) {
        Com_Printf("%i ", cl.serverTimeDelta);
    }
}

void CL_FirstSnapshot(void) {
    if (cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE) {
        Com_DPrintf("CL_FirstSnapshot: REJECTED (SNAPFLAG_NOT_ACTIVE), state=%d\n", clc.state);
        return;
    }
    Com_DPrintf("CL_FirstSnapshot: SUCCESS, transitioning to CA_ACTIVE\n");
    clc.state = CA_ACTIVE;

    cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
    cl.oldServerTime = cl.snap.serverTime;

    clc.timeDemoBaseTime = cl.snap.serverTime;

    if (cl_activeAction->string[0]) {
        Cbuf_AddText(cl_activeAction->string);
        Cvar_Set("activeAction", "");
    }
}

void CL_SetCGameTime(void) {
    if (clc.state != CA_ACTIVE) {
        if (clc.state != CA_PRIMED) {
            return;
        }
        if (clc.demoplaying) {
            int i;

            if (!clc.firstDemoFrameSkipped) {
                clc.firstDemoFrameSkipped = qtrue;
                return;
            }
            for (i = 0; i < 4096 && clc.state == CA_PRIMED && clc.demoplaying; i++) {
                CL_ReadDemoMessage();
                if (cl.newSnapshots) {
                    cl.newSnapshots = qfalse;
                    CL_FirstSnapshot();
                }
            }
        }
        if (cl.newSnapshots) {
            cl.newSnapshots = qfalse;
            CL_FirstSnapshot();
        }
        if (clc.state != CA_ACTIVE) {
            return;
        }
    }

    if (!cl.snap.valid) {
        Com_Error(ERR_DROP, "CL_SetCGameTime: !cl.snap.valid");
    }

    if (sv_paused->integer && CL_CheckPaused() && com_sv_running->integer) {
        return;
    }

    if (cl.snap.serverTime < cl.oldFrameServerTime) {
        if (clc.demoplaying) {
            cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
            cl.oldServerTime = cl.snap.serverTime;
        } else if (clc.state == CA_ACTIVE) {
            Com_Error(ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime");
        } else {
            Com_Printf("^3CL_SetCGameTime: cl.snap.serverTime < cl.oldFrameServerTime\n");
        }
    }
    cl.oldFrameServerTime = cl.snap.serverTime;

    if (clc.demoplaying && cl_freezeDemo->integer) {
        cl.serverTimeDelta = cl.serverTime - cls.realtime;
    } else {
        static int prevTimeNudge;
        int tn;

        if (clc.demoplaying || Cvar_VariableIntegerValue("cg_spectating") ||
            Sys_IsLANAddress(clc.netchan.remoteAddress)) {
            tn = 0;
        } else if (cl_autoTimeNudge->integer == 0) {
            tn = cl_timeNudge->integer;
        } else {
            tn = (int)(cl.snap.ping * -0.5);

            if (prevTimeNudge != 0 && prevTimeNudge != tn) {
                tn = prevTimeNudge;
            }

            if (tn < -20) {
                tn = -20;
            } else if (tn > 0) {
                tn = 0;
            }
        }

        cl.serverTime = cls.realtime + cl.serverTimeDelta - tn;
        prevTimeNudge = tn;

        if (cl.serverTime < cl.oldServerTime) {
            cl.serverTime = cl.oldServerTime;
        }
        cl.oldServerTime = cl.serverTime;

        if (cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5) {
            cl.extrapolatedSnapshot = qtrue;
        }
    }

    if (cl.newSnapshots) {
        CL_AdjustTimeDelta();
    }

    if (!clc.demoplaying) {
        return;
    }

    if (cl_timedemo->integer) {
        int now = Sys_Milliseconds();
        int frameDuration;

        if (!clc.timeDemoStart) {
            clc.timeDemoStart = clc.timeDemoLastFrame = now;
            clc.timeDemoMinDuration = INT_MAX;
            clc.timeDemoMaxDuration = 0;
        }

        frameDuration = now - clc.timeDemoLastFrame;
        clc.timeDemoLastFrame = now;

        if (clc.timeDemoFrames > 0) {
            if (frameDuration > clc.timeDemoMaxDuration)
                clc.timeDemoMaxDuration = frameDuration;

            if (frameDuration < clc.timeDemoMinDuration)
                clc.timeDemoMinDuration = frameDuration;

            if (frameDuration > UCHAR_MAX)
                frameDuration = UCHAR_MAX;

            clc.timeDemoDurations[(clc.timeDemoFrames - 1) %
                                  MAX_TIMEDEMO_DURATIONS] = frameDuration;
        }

        clc.timeDemoFrames++;
        cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * 50;
    }

    CL_DemoSeekApply();

    while (cl.serverTime >= cl.snap.serverTime) {
        int before = cl.snap.serverTime;

        CL_ReadDemoMessage();
        if (clc.state != CA_ACTIVE) {
            return;
        }
        if (cl.snap.serverTime < before - 1000) {
            cl.serverTimeDelta += cl.snap.serverTime - cl.serverTime;
            cl.serverTime = cl.oldServerTime = cl.snap.serverTime;
            break;
        }
    }
}
