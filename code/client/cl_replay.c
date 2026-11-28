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

#define RP_MAX_SECONDS 60
#define RP_SNAPS_PER_SECOND 45
#define RP_ENTS_PER_SNAP 64
#define RP_MAX_CMDS 2048
#define RP_FIRST_MESSAGE 1000
#define RP_RENDER_ENV "IOQL_REPLAYVIDEO"
#define RP_RENDER_QUEUE 8
#define RP_RENDER_MAX_W 1920
#define RP_RENDER_MAX_H 1080
#define RP_RENDER_PACE 8
#define RP_RENDER_IDLE 250
#define RP_RENDER_TIMEOUT 300000
#define RP_LATEST_WAIT 60000
#define RP_RENDERING "videos/replays/rendering"
#define RP_LATEST "videos/replay.mp4"
#define RP_LATEST_PART "videos/replay.part"

enum { RV_START, RV_LOAD, RV_RECORD, RV_LATEST, RV_DONE };

typedef struct {
    int serverTime;
    int snapFlags;
    int serverCommandNum;
    byte areamask[MAX_MAP_AREA_BYTES];
    playerState_t ps;
    int firstEntity;
    int numEntities;
} rpSnap_t;

typedef struct {
    int seq;
    char text[MAX_STRING_CHARS];
} rpCmd_t;

static cvar_t* cl_replaySeconds;

static qboolean rpRecording;
static int rpSeconds;
static rpSnap_t* rpSnaps;
static int rpSnapCap;
static int rpSnapFirst;
static int rpSnapNext;
static entityState_t* rpEnts;
static int rpEntCap;
static int rpEntNext;
static rpCmd_t* rpCmds;
static int rpCmdLast;
static gameState_t rpBase;
static int rpBaseSeq;
static char rpBaseBig[BIG_INFO_STRING];
static char rpWriteBig[BIG_INFO_STRING];

static void* rpRender;
static char rpRenderName[MAX_QPATH];
static char rpQueue[RP_RENDER_QUEUE][MAX_QPATH];
static int rpQueued;

static qboolean rvActive;
static char rvName[MAX_QPATH];
static int rvStage;
static int rvBegin;
static int rvIssued;
static int rvLatestTime;
static int rvFrameTime;

static void CL_ReplayFree(void) {
    free(rpSnaps);
    free(rpEnts);
    free(rpCmds);
    rpSnaps = NULL;
    rpEnts = NULL;
    rpCmds = NULL;
    rpSnapCap = rpEntCap = 0;
    rpRecording = qfalse;
}

static void CL_ReplaySetConfigstring(gameState_t* gs, int index, const char* s) {
    static gameState_t old;
    const char* dup;
    int i, len;

    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        return;
    }
    if (!strcmp(gs->stringData + gs->stringOffsets[index], s)) {
        return;
    }
    old = *gs;
    Com_Memset(gs, 0, sizeof(*gs));
    gs->dataCount = 1;
    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        dup = (i == index) ? s : old.stringData + old.stringOffsets[i];
        if (!dup[0]) {
            continue;
        }
        len = (int)strlen(dup);
        if (len + 1 + gs->dataCount > MAX_GAMESTATE_CHARS) {
            *gs = old;
            return;
        }
        gs->stringOffsets[i] = gs->dataCount;
        Com_Memcpy(gs->stringData + gs->dataCount, dup, len + 1);
        gs->dataCount += len + 1;
    }
}

static const char* CL_ReplayQuoted(const char* s, char* out, int size) {
    const char* end;
    int len;

    while (*s == ' ') {
        s++;
    }
    if (*s == '"') {
        s++;
        end = strrchr(s, '"');
        len = end ? (int)(end - s) : (int)strlen(s);
    } else {
        len = (int)strlen(s);
        while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ')) {
            len--;
        }
    }
    if (len >= size) {
        len = size - 1;
    }
    Com_Memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static void CL_ReplayApply(gameState_t* gs, char* big, const char* cmd) {
    static char value[BIG_INFO_STRING];
    const char* p;
    int index;

    if (!Q_strncmp(cmd, "cs ", 3)) {
        p = cmd + 3;
        index = atoi(p);
        while (*p && *p != ' ') {
            p++;
        }
        CL_ReplaySetConfigstring(gs, index, CL_ReplayQuoted(p, value, sizeof(value)));
        return;
    }
    if (!Q_strncmp(cmd, "bcs0 ", 5) || !Q_strncmp(cmd, "bcs1 ", 5) || !Q_strncmp(cmd, "bcs2 ", 5)) {
        int part = cmd[3] - '0';
        p = cmd + 5;
        index = atoi(p);
        while (*p && *p != ' ') {
            p++;
        }
        CL_ReplayQuoted(p, value, sizeof(value));
        if (part == 0) {
            Q_strncpyz(big, value, BIG_INFO_STRING);
        } else if (strlen(big) + strlen(value) < BIG_INFO_STRING) {
            Q_strcat(big, BIG_INFO_STRING, value);
        }
        if (part == 2) {
            CL_ReplaySetConfigstring(gs, index, big);
            big[0] = '\0';
        }
    }
}

static rpCmd_t* CL_ReplayCmd(int seq) {
    rpCmd_t* c = &rpCmds[seq & (RP_MAX_CMDS - 1)];

    return c->seq == seq ? c : NULL;
}

static void CL_ReplayAdvanceBase(int seq) {
    rpCmd_t* c;

    while (rpBaseSeq < seq) {
        rpBaseSeq++;
        c = CL_ReplayCmd(rpBaseSeq);
        if (c) {
            CL_ReplayApply(&rpBase, rpBaseBig, c->text);
        }
    }
}

static void CL_ReplayDropOldest(void) {
    rpSnap_t* s;

    if (rpSnapFirst >= rpSnapNext) {
        return;
    }
    s = &rpSnaps[rpSnapFirst % rpSnapCap];
    if (s->serverCommandNum > rpBaseSeq) {
        CL_ReplayAdvanceBase(s->serverCommandNum);
    }
    rpSnapFirst++;
}

static void CL_ReplayStoreCommand(int seq, const char* text) {
    rpCmd_t* c;

    if (seq <= rpCmdLast) {
        return;
    }
    while (seq - rpBaseSeq >= RP_MAX_CMDS) {
        if (rpSnapFirst < rpSnapNext) {
            CL_ReplayDropOldest();
        } else {
            CL_ReplayAdvanceBase(seq - RP_MAX_CMDS + 1);
        }
    }
    c = &rpCmds[seq & (RP_MAX_CMDS - 1)];
    c->seq = seq;
    Q_strncpyz(c->text, text, sizeof(c->text));
    rpCmdLast = seq;
}

static qboolean CL_ReplayWanted(void) {
    return Cvar_VariableIntegerValue("cl_streamerMode") && cl_replaySeconds->integer > 0 && !com_demoWindowParent &&
           !clc.demoplaying && clc.state >= CA_PRIMED;
}

static void CL_ReplayStart(void) {
    int seconds, seq;

    seconds = cl_replaySeconds->integer;
    if (seconds > RP_MAX_SECONDS) {
        seconds = RP_MAX_SECONDS;
    }
    if (!rpSnaps || seconds != rpSeconds) {
        CL_ReplayFree();
        rpSeconds = seconds;
        rpSnapCap = seconds * RP_SNAPS_PER_SECOND + 64;
        rpEntCap = rpSnapCap * RP_ENTS_PER_SNAP;
        rpSnaps = (rpSnap_t*)malloc(sizeof(rpSnap_t) * rpSnapCap);
        rpEnts = (entityState_t*)malloc(sizeof(entityState_t) * rpEntCap);
        rpCmds = (rpCmd_t*)malloc(sizeof(rpCmd_t) * RP_MAX_CMDS);
        if (!rpSnaps || !rpEnts || !rpCmds) {
            CL_ReplayFree();
            return;
        }
    }
    Com_Memset(rpCmds, 0, sizeof(rpCmd_t) * RP_MAX_CMDS);
    rpSnapFirst = rpSnapNext = 0;
    rpEntNext = 0;
    rpBase = cl.gameState;
    rpBaseSeq = clc.lastExecutedServerCommand;
    rpCmdLast = rpBaseSeq;
    rpBaseBig[0] = '\0';
    rpRecording = qtrue;
    for (seq = rpBaseSeq + 1; seq <= clc.serverCommandSequence; seq++) {
        if (seq > clc.serverCommandSequence - MAX_RELIABLE_COMMANDS) {
            CL_ReplayStoreCommand(seq, clc.serverCommands[seq & (MAX_RELIABLE_COMMANDS - 1)]);
        }
    }
}

void CL_ReplayGamestate(void) {
    rpRecording = qfalse;
    if (CL_ReplayWanted()) {
        CL_ReplayStart();
    }
}

void CL_ReplayCommand(int seq, const char* text) {
    if (rpRecording) {
        CL_ReplayStoreCommand(seq, text);
    }
}

void CL_ReplaySnapshot(void) {
    rpSnap_t* s;
    int i, count, keep;

    if (!CL_ReplayWanted()) {
        if (rpSnaps) {
            CL_ReplayFree();
        }
        return;
    }
    if (!rpRecording || cl_replaySeconds->integer != rpSeconds) {
        CL_ReplayStart();
        if (!rpRecording) {
            return;
        }
    }
    if (!cl.snap.valid) {
        return;
    }
    count = cl.snap.numEntities;
    if (count > MAX_SNAPSHOT_ENTITIES) {
        count = MAX_SNAPSHOT_ENTITIES;
    }
    while (rpSnapNext - rpSnapFirst >= rpSnapCap) {
        CL_ReplayDropOldest();
    }
    s = &rpSnaps[rpSnapNext % rpSnapCap];
    s->serverTime = cl.snap.serverTime;
    s->snapFlags = cl.snap.snapFlags;
    s->serverCommandNum = cl.snap.serverCommandNum;
    Com_Memcpy(s->areamask, cl.snap.areamask, sizeof(s->areamask));
    s->ps = cl.snap.ps;
    s->firstEntity = rpEntNext;
    s->numEntities = count;
    for (i = 0; i < count; i++) {
        rpEnts[(rpEntNext + i) % rpEntCap] = cl.parseEntities[(cl.snap.parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
    }
    rpEntNext += count;
    rpSnapNext++;

    keep = rpSeconds * 1000 + 1000;
    while (rpSnapFirst < rpSnapNext - 1) {
        rpSnap_t* first = &rpSnaps[rpSnapFirst % rpSnapCap];
        if (cl.snap.serverTime - first->serverTime <= keep && rpEntNext - first->firstEntity <= rpEntCap) {
            break;
        }
        CL_ReplayDropOldest();
    }
}

void CL_ReplayDisconnect(void) {
    rpRecording = qfalse;
    rpSnapFirst = rpSnapNext = 0;
}

static entityState_t* CL_ReplayEntity(const rpSnap_t* s, int i) {
    return &rpEnts[(s->firstEntity + i) % rpEntCap];
}

static void CL_ReplayWriteEntities(msg_t* msg, const rpSnap_t* from, const rpSnap_t* to) {
    entityState_t* oldEnt;
    entityState_t* newEnt;
    int oldIndex = 0, newIndex = 0;
    int oldNum, newNum;
    int oldCount = from ? from->numEntities : 0;

    while (newIndex < to->numEntities || oldIndex < oldCount) {
        newEnt = newIndex < to->numEntities ? CL_ReplayEntity(to, newIndex) : NULL;
        oldEnt = oldIndex < oldCount ? CL_ReplayEntity(from, oldIndex) : NULL;
        newNum = newEnt ? newEnt->number : 9999;
        oldNum = oldEnt ? oldEnt->number : 9999;
        if (newNum == oldNum) {
            MSG_WriteDeltaEntity(msg, oldEnt, newEnt, qfalse);
            oldIndex++;
            newIndex++;
        } else if (newNum < oldNum) {
            MSG_WriteDeltaEntity(msg, &cl.entityBaselines[newNum], newEnt, qtrue);
            newIndex++;
        } else {
            MSG_WriteDeltaEntity(msg, oldEnt, NULL, qtrue);
            oldIndex++;
        }
    }
    MSG_WriteBits(msg, (MAX_GENTITIES - 1), GENTITYNUM_BITS);
}

static void CL_ReplayFlush(fileHandle_t f, msg_t* msg, int messageNum) {
    int len;

    MSG_WriteByte(msg, svc_EOF);
    len = LittleLong(messageNum);
    FS_Write(&len, 4, f);
    len = LittleLong(msg->cursize);
    FS_Write(&len, 4, f);
    FS_Write(msg->data, msg->cursize, f);
}

static qboolean CL_ReplayWrite(const char* path, int seconds) {
    static byte data[MAX_MSGLEN];
    static gameState_t gs;
    entityState_t nullstate;
    rpSnap_t* first;
    rpSnap_t* prev = NULL;
    rpSnap_t* s;
    fileHandle_t f;
    msg_t msg;
    int i, k, start, newest, seq, messageNum, prevMessage = 0, len;
    const char* text;

    if (!rpRecording || rpSnapNext - rpSnapFirst < 2) {
        return qfalse;
    }
    newest = rpSnaps[(rpSnapNext - 1) % rpSnapCap].serverTime;
    start = rpSnapFirst;
    while (start < rpSnapNext - 1 && newest - rpSnaps[start % rpSnapCap].serverTime > seconds * 1000) {
        start++;
    }
    first = &rpSnaps[start % rpSnapCap];

    gs = rpBase;
    Q_strncpyz(rpWriteBig, rpBaseBig, sizeof(rpWriteBig));
    for (seq = rpBaseSeq + 1; seq <= first->serverCommandNum; seq++) {
        rpCmd_t* c = CL_ReplayCmd(seq);
        if (c) {
            CL_ReplayApply(&gs, rpWriteBig, c->text);
        }
    }

    f = FS_FOpenFileWrite(path);
    if (!f) {
        return qfalse;
    }

    MSG_Init(&msg, data, sizeof(data));
    MSG_Bitstream(&msg);
    MSG_WriteLong(&msg, clc.reliableSequence);
    MSG_WriteByte(&msg, svc_gamestate);
    MSG_WriteLong(&msg, first->serverCommandNum);
    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (!gs.stringOffsets[i]) {
            continue;
        }
        MSG_WriteByte(&msg, svc_configstring);
        MSG_WriteShort(&msg, i);
        MSG_WriteBigString(&msg, gs.stringData + gs.stringOffsets[i]);
    }
    Com_Memset(&nullstate, 0, sizeof(nullstate));
    for (i = 0; i < MAX_GENTITIES; i++) {
        if (!cl.entityBaselines[i].number) {
            continue;
        }
        MSG_WriteByte(&msg, svc_baseline);
        MSG_WriteDeltaEntity(&msg, &nullstate, &cl.entityBaselines[i], qtrue);
    }
    MSG_WriteByte(&msg, svc_EOF);
    MSG_WriteLong(&msg, clc.clientNum);
    MSG_WriteLong(&msg, clc.checksumFeed);
    messageNum = RP_FIRST_MESSAGE;
    CL_ReplayFlush(f, &msg, messageNum - 1);

    seq = first->serverCommandNum;
    for (k = start; k < rpSnapNext; k++) {
        s = &rpSnaps[k % rpSnapCap];
        MSG_Init(&msg, data, sizeof(data));
        MSG_Bitstream(&msg);
        MSG_WriteLong(&msg, clc.reliableSequence);
        while (seq < s->serverCommandNum) {
            rpCmd_t* c = CL_ReplayCmd(seq + 1);
            text = c ? c->text : "";
            if (msg.cursize + (int)strlen(text) + 64 > MAX_MSGLEN - 8192) {
                CL_ReplayFlush(f, &msg, messageNum++);
                MSG_Init(&msg, data, sizeof(data));
                MSG_Bitstream(&msg);
                MSG_WriteLong(&msg, clc.reliableSequence);
            }
            seq++;
            MSG_WriteByte(&msg, svc_serverCommand);
            MSG_WriteLong(&msg, seq);
            MSG_WriteString(&msg, text);
        }
        if (prev && messageNum - prevMessage >= PACKET_BACKUP) {
            prev = NULL;
        }
        MSG_WriteByte(&msg, svc_snapshot);
        MSG_WriteLong(&msg, s->serverTime);
        MSG_WriteByte(&msg, prev ? messageNum - prevMessage : 0);
        MSG_WriteByte(&msg, s->snapFlags);
        MSG_WriteByte(&msg, sizeof(s->areamask));
        MSG_WriteData(&msg, s->areamask, sizeof(s->areamask));
        MSG_WriteDeltaPlayerstate(&msg, prev ? &prev->ps : NULL, &s->ps);
        CL_ReplayWriteEntities(&msg, prev, s);
        CL_ReplayFlush(f, &msg, messageNum);
        prev = s;
        prevMessage = messageNum;
        messageNum++;
    }

    len = -1;
    FS_Write(&len, 4, f);
    FS_Write(&len, 4, f);
    FS_FCloseFile(f);
    return qtrue;
}

static int CL_ReplaySave(char* name, int size) {
    qtime_t t;
    char path[MAX_QPATH];
    char stamp[MAX_QPATH];
    int seconds, n;

    if (!Cvar_VariableIntegerValue("cl_streamerMode")) {
        Com_Printf("Replays need Streamer Mode: turn on cl_streamerMode.\n");
        return 0;
    }
    if (clc.state != CA_ACTIVE || clc.demoplaying) {
        Com_Printf("Replays work while connected to a server.\n");
        return 0;
    }
    seconds = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : cl_replaySeconds->integer;
    if (seconds < 1) {
        seconds = cl_replaySeconds->integer;
    }
    if (seconds > rpSeconds) {
        seconds = rpSeconds;
    }
    Com_RealTime(&t);
    Com_sprintf(stamp, sizeof(stamp), "replay-%04d-%02d-%02d_%02d-%02d-%02d", 1900 + t.tm_year, t.tm_mon + 1,
                t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    Q_strncpyz(name, stamp, size);
    for (n = 2;; n++) {
        Com_sprintf(path, sizeof(path), "demos/%s.%s%d", name, DEMOEXT, PROTOCOL_VERSION);
        if ((!FS_FileExists(path) && !FS_FileExists(va("videos/replays/%s.mp4", name))) || n > 99) {
            break;
        }
        Com_sprintf(name, size, "%s-%d", stamp, n);
    }
    if (!CL_ReplayWrite(path, seconds)) {
        Com_Printf("Nothing to replay yet.\n");
        return 0;
    }
    return seconds;
}

static void CL_ReplayOSPath(char* out, int size, const char* qpath) {
    Q_strncpyz(out, FS_BuildOSPath(Cvar_VariableString("fs_homepath"), FS_GetCurrentGameDir(), qpath), size);
}

static void CL_ReplayJsonPath(char* out, int size, const char* qpath) {
    char path[MAX_OSPATH];
    int i, n = 0;

    CL_ReplayOSPath(path, sizeof(path), qpath);
    for (i = 0; path[i] && n < size - 3; i++) {
        if (path[i] == '\\' || path[i] == '"') {
            out[n++] = '\\';
        }
        out[n++] = path[i];
    }
    out[n] = '\0';
}

static void CL_ReplayFailed(const char* name) {
    Com_Printf("Replay: the video could not be made. The demo is in demos/%s.%s%d\n", name, DEMOEXT,
               PROTOCOL_VERSION);
}

static void CL_ReplayReady(const char* name) {
    char file[MAX_OSPATH * 2];
    char latest[MAX_OSPATH * 2];

    if (!FS_FileExists(va("videos/replays/%s.mp4", name))) {
        CL_ReplayFailed(name);
        return;
    }
    Com_Printf("Replay ready: videos/replays/%s.mp4\n", name);
    CL_ReplayJsonPath(file, sizeof(file), va("videos/replays/%s.mp4", name));
    CL_ReplayJsonPath(latest, sizeof(latest), RP_LATEST);
    CL_StreamerPublish("replay", va("{\"name\":\"%s\",\"file\":\"%s\",\"latest\":\"%s\"}", name, file, latest));
}

static qboolean CL_ReplayRenderLaunch(const char* name) {
    char exe[MAX_OSPATH];
    char log[MAX_OSPATH];
    char home[MAX_OSPATH];
    char base[MAX_OSPATH];
    char audio[64];
    char sw[16];
    char sh[16];
    const char* old;
    int w = cls.glconfig.vidWidth;
    int h = cls.glconfig.vidHeight;
    const char* argv[] = {exe,
                          "+set", "fs_basepath", base,
                          "+set", "fs_homepath", home,
                          "+set", "r_fullscreen", "0",
                          "+set", "r_windowedMode", "-1",
                          "+set", "r_windowedWidth", sw,
                          "+set", "r_windowedHeight", sh,
                          "+set", "r_swapInterval", "0",
                          "+set", "com_maxfps", "250x",
                          "+set", "com_maxfpsUnfocused", "0",
                          "+set", "com_maxfpsMinimized", "0",
                          "+set", "com_introPlayed", "1",
                          "+set", "com_ignoreCrash", "1",
                          "+set", "s_muteBackground", "0",
                          "+set", "cg_drawFPS", "0",
                          "+set", "net_enabled", "0",
                          "+set", "logfile", "0",
                          NULL};

    if (w > RP_RENDER_MAX_W || h > RP_RENDER_MAX_H) {
        float scale = MIN((float)RP_RENDER_MAX_W / w, (float)RP_RENDER_MAX_H / h);

        w = (int)(w * scale);
        h = (int)(h * scale);
    }
    Com_sprintf(sw, sizeof(sw), "%i", w & ~1);
    Com_sprintf(sh, sizeof(sh), "%i", h & ~1);
    Q_strncpyz(home, Cvar_VariableString("fs_homepath"), sizeof(home));
    Q_strncpyz(base, Cvar_VariableString("fs_basepath"), sizeof(base));
    Com_sprintf(exe, sizeof(exe), "%s%c%s", Sys_BinaryPath(), PATH_SEP, Sys_BinaryName());
    Q_strncpyz(log, FS_BuildOSPath(home, BASEGAME_DIR, "replayvideo.log"), sizeof(log));
    old = getenv("SDL_AUDIO_DRIVER");
    Q_strncpyz(audio, old ? old : "", sizeof(audio));

    Sys_SetEnv("IOQL_DEMOWINDOW", va("%d", Sys_PID()));
    Sys_SetEnv(RP_RENDER_ENV, name);
    Sys_SetEnv("SDL_AUDIO_DRIVER", "dummy");
    rpRender = Sys_PipeOpen(argv, log);
    Sys_SetEnv("IOQL_DEMOWINDOW", "");
    Sys_SetEnv(RP_RENDER_ENV, "");
    Sys_SetEnv("SDL_AUDIO_DRIVER", audio);
    if (!rpRender) {
        return qfalse;
    }
    Q_strncpyz(rpRenderName, name, sizeof(rpRenderName));
    return qtrue;
}

static void CL_Replay_f(void) {
    char name[MAX_QPATH];
    int seconds = CL_ReplaySave(name, sizeof(name));

    if (!seconds) {
        return;
    }
    if (rpQueued >= RP_RENDER_QUEUE) {
        CL_ReplayFailed(name);
        return;
    }
    Q_strncpyz(rpQueue[rpQueued++], name, MAX_QPATH);
    Com_Printf("Replay: saving the last %i seconds as a video.\n", seconds);
}

static qboolean CL_ReplayCopy(const char* from, const char* to) {
    static byte buf[1 << 16];
    FILE* in;
    FILE* out;
    size_t n;
    qboolean ok = qtrue;

    in = Sys_FOpen(from, "rb");
    if (!in) {
        return qfalse;
    }
    out = Sys_FOpen(to, "wb");
    if (!out) {
        fclose(in);
        return qfalse;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = qfalse;
            break;
        }
    }
    fclose(in);
    if (fclose(out)) {
        ok = qfalse;
    }
    if (!ok) {
        remove(to);
    }
    return ok;
}

static void CL_ReplayRenderQuit(void) {
    rvStage = RV_DONE;
    Cbuf_AddText("quit\n");
}

static void CL_ReplayRenderFinish(void) {
    char from[MAX_OSPATH];
    char to[MAX_OSPATH];
    char part[MAX_OSPATH];

    CL_ReplayOSPath(from, sizeof(from), va("%s/%s.mp4", RP_RENDERING, rvName));
    CL_ReplayOSPath(to, sizeof(to), va("videos/replays/%s.mp4", rvName));
    if (!CL_VideoLastOk() || rename(from, to)) {
        remove(from);
        CL_ReplayRenderQuit();
        return;
    }
    CL_ReplayOSPath(part, sizeof(part), RP_LATEST_PART);
    if (!CL_ReplayCopy(to, part)) {
        CL_ReplayRenderQuit();
        return;
    }
    rvStage = RV_LATEST;
    rvLatestTime = Sys_Milliseconds();
}

static void CL_ReplayRenderLatest(void) {
    char part[MAX_OSPATH];
    char latest[MAX_OSPATH];

    CL_ReplayOSPath(part, sizeof(part), RP_LATEST_PART);
    CL_ReplayOSPath(latest, sizeof(latest), RP_LATEST);
    remove(latest);
    if (!rename(part, latest) || Sys_Milliseconds() - rvLatestTime > RP_LATEST_WAIT) {
        remove(part);
        CL_ReplayRenderQuit();
    }
}

static void CL_ReplayRenderFrame(void) {
    if (rvStage == RV_DONE) {
        return;
    }
    if (Sys_Milliseconds() - rvBegin > RP_RENDER_TIMEOUT) {
        CL_ReplayRenderQuit();
        return;
    }
    switch (rvStage) {
        case RV_START:
            if (!cls.rendererStarted || !cls.uiStarted) {
                return;
            }
            rvIssued = cls.framecount;
            rvStage = RV_LOAD;
            Cbuf_AddText(va("demo \"%s\"\n", rvName));
            return;
        case RV_LOAD:
            if (clc.demoplaying && clc.state == CA_ACTIVE) {
                if (CL_VideoRecordTo(va("%s/%s.mp4", RP_RENDERING, rvName))) {
                    rvStage = RV_RECORD;
                } else {
                    CL_ReplayRenderQuit();
                }
            } else if (!clc.demoplaying && cls.framecount - rvIssued > 1) {
                CL_ReplayRenderQuit();
            }
            return;
        case RV_RECORD:
            if (!CL_VideoRecording()) {
                CL_ReplayRenderFinish();
            }
            return;
        case RV_LATEST:
            CL_ReplayRenderLatest();
            return;
    }
}

qboolean CL_ReplayRendering(void) {
    return rvActive;
}

void CL_ReplayRenderWait(void) {
    int pace = rvStage == RV_LATEST || rvStage == RV_DONE ? RP_RENDER_IDLE : RP_RENDER_PACE;
    int left = pace - (Sys_Milliseconds() - rvFrameTime);

    if (left > 0) {
        Sys_Sleep(left);
    }
    rvFrameTime = Sys_Milliseconds();
}

void CL_ReplayFrame(void) {
    if (rvActive) {
        CL_ReplayRenderFrame();
        return;
    }
    if (rpRender && !Sys_PipeAlive(rpRender)) {
        Sys_PipeClose(rpRender);
        rpRender = NULL;
        CL_ReplayReady(rpRenderName);
    }
    if (!rpRender && rpQueued) {
        char name[MAX_QPATH];

        Q_strncpyz(name, rpQueue[0], sizeof(name));
        rpQueued--;
        memmove(rpQueue[0], rpQueue[1], rpQueued * sizeof(rpQueue[0]));
        if (!CL_ReplayRenderLaunch(name)) {
            CL_ReplayFailed(name);
        }
    }
}

void CL_ReplayInit(void) {
    const char* render = getenv(RP_RENDER_ENV);

    cl_replaySeconds = Cvar_Get("cl_replaySeconds", "20", CVAR_ARCHIVE);
    Cvar_CheckRange(cl_replaySeconds, 5, RP_MAX_SECONDS, qtrue);
    Cmd_AddCommand("replay", CL_Replay_f);
    if (com_demoWindowParent && render && *render && !strpbrk(render, "\";\r\n/\\:") && !strstr(render, "..")) {
        rvActive = qtrue;
        Q_strncpyz(rvName, render, sizeof(rvName));
        rvBegin = Sys_Milliseconds();
        Sys_BackgroundPriority();
    }
}

void CL_ReplayShutdown(void) {
    Cmd_RemoveCommand("replay");
    CL_ReplayFree();
    rpQueued = 0;
}
