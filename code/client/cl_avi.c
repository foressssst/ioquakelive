/*
===========================================================================
Copyright (C) 2005-2006 Tim Angus

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
#include "snd_local.h"

#define INDEX_FILE_EXTENSION ".index.dat"

#define MAX_RIFF_CHUNKS 16

typedef struct audioFormat_s {
    int rate;
    int format;
    int channels;
    int bits;

    int sampleSize;
    int totalBytes;
} audioFormat_t;

typedef struct aviFileData_s {
    qboolean fileOpen;
    fileHandle_t f;
    char fileName[MAX_QPATH];
    int fileSize;
    int moviOffset;
    int moviSize;

    fileHandle_t idxF;
    int numIndices;

    void* pipe;
    qboolean pipeFailed;

    int frameRate;
    int framePeriod;
    int width, height;
    int numVideoFrames;
    int maxRecordSize;
    qboolean motionJpeg;

    qboolean audio;
    audioFormat_t a;
    int numAudioFrames;

    int chunkStack[MAX_RIFF_CHUNKS];
    int chunkStackTop;

    byte *cBuffer, *eBuffer;
} aviFileData_t;

static aviFileData_t afd;

#define MAX_AVI_BUFFER 2048

static byte buffer[MAX_AVI_BUFFER];
static int bufIndex;

static ID_INLINE void SafeFS_Write(const void* buffer, int len, fileHandle_t f) {
    if (FS_Write(buffer, len, f) < len)
        Com_Error(ERR_DROP, "Failed to write avi file");
}

static void CL_AviOut(const void* data, int len) {
    if (afd.pipe) {
        if (!afd.pipeFailed && !Sys_PipeWrite(afd.pipe, data, len)) {
            afd.pipeFailed = qtrue;
            Cbuf_AddText("stopvideo\n");
        }
        return;
    }
    SafeFS_Write(data, len, afd.f);
}

static ID_INLINE void WRITE_STRING(const char* s) {
    Com_Memcpy(&buffer[bufIndex], s, strlen(s));
    bufIndex += strlen(s);
}

static ID_INLINE void WRITE_4BYTES(int x) {
    buffer[bufIndex + 0] = (byte)((x >> 0) & 0xFF);
    buffer[bufIndex + 1] = (byte)((x >> 8) & 0xFF);
    buffer[bufIndex + 2] = (byte)((x >> 16) & 0xFF);
    buffer[bufIndex + 3] = (byte)((x >> 24) & 0xFF);
    bufIndex += 4;
}

static ID_INLINE void WRITE_2BYTES(int x) {
    buffer[bufIndex + 0] = (byte)((x >> 0) & 0xFF);
    buffer[bufIndex + 1] = (byte)((x >> 8) & 0xFF);
    bufIndex += 2;
}

static ID_INLINE void START_CHUNK(const char* s) {
    if (afd.chunkStackTop == MAX_RIFF_CHUNKS) {
        Com_Error(ERR_DROP, "ERROR: Top of chunkstack breached");
    }

    afd.chunkStack[afd.chunkStackTop] = bufIndex;
    afd.chunkStackTop++;
    WRITE_STRING(s);
    WRITE_4BYTES(0);
}

static ID_INLINE void END_CHUNK(void) {
    int endIndex = bufIndex;

    if (afd.chunkStackTop <= 0) {
        Com_Error(ERR_DROP, "ERROR: Bottom of chunkstack breached");
    }

    afd.chunkStackTop--;
    bufIndex = afd.chunkStack[afd.chunkStackTop];
    bufIndex += 4;
    WRITE_4BYTES(endIndex - bufIndex - 4);
    bufIndex = endIndex;
    bufIndex = PAD(bufIndex, 2);
}

void CL_WriteAVIHeader(void) {
    bufIndex = 0;
    afd.chunkStackTop = 0;

    START_CHUNK("RIFF");
    {
        WRITE_STRING("AVI ");
        {
            START_CHUNK("LIST");
            {
                WRITE_STRING("hdrl");
                WRITE_STRING("avih");
                WRITE_4BYTES(56);
                WRITE_4BYTES(afd.framePeriod);
                WRITE_4BYTES(afd.maxRecordSize *
                             afd.frameRate);
                WRITE_4BYTES(0);
                WRITE_4BYTES(afd.pipe ? 0x100 : 0x110);
                WRITE_4BYTES(afd.numVideoFrames);
                WRITE_4BYTES(0);

                if (afd.audio)
                    WRITE_4BYTES(2);
                else
                    WRITE_4BYTES(1);

                WRITE_4BYTES(afd.maxRecordSize);
                WRITE_4BYTES(afd.width);
                WRITE_4BYTES(afd.height);
                WRITE_4BYTES(0);
                WRITE_4BYTES(0);
                WRITE_4BYTES(0);
                WRITE_4BYTES(0);

                START_CHUNK("LIST");
                {
                    WRITE_STRING("strl");
                    WRITE_STRING("strh");
                    WRITE_4BYTES(56);
                    WRITE_STRING("vids");

                    if (afd.motionJpeg)
                        WRITE_STRING("MJPG");
                    else
                        WRITE_4BYTES(0);

                    WRITE_4BYTES(0);
                    WRITE_4BYTES(0);
                    WRITE_4BYTES(0);

                    WRITE_4BYTES(1);
                    WRITE_4BYTES(afd.frameRate);
                    WRITE_4BYTES(0);
                    WRITE_4BYTES(afd.numVideoFrames);

                    WRITE_4BYTES(afd.maxRecordSize);
                    WRITE_4BYTES(-1);
                    WRITE_4BYTES(0);
                    WRITE_2BYTES(0);
                    WRITE_2BYTES(0);
                    WRITE_2BYTES(afd.width);
                    WRITE_2BYTES(afd.height);

                    WRITE_STRING("strf");
                    WRITE_4BYTES(40);
                    WRITE_4BYTES(40);
                    WRITE_4BYTES(afd.width);
                    WRITE_4BYTES(afd.height);
                    WRITE_2BYTES(1);
                    WRITE_2BYTES(24);

                    if (afd.motionJpeg)
                    {
                        WRITE_STRING("MJPG");
                        WRITE_4BYTES(afd.width *
                                     afd.height);
                    } else {
                        WRITE_4BYTES(0);
                        WRITE_4BYTES(afd.width *
                                     afd.height * 3);
                    }

                    WRITE_4BYTES(0);
                    WRITE_4BYTES(0);
                    WRITE_4BYTES(0);
                    WRITE_4BYTES(0);
                }
                END_CHUNK();

                if (afd.audio) {
                    START_CHUNK("LIST");
                    {
                        WRITE_STRING("strl");
                        WRITE_STRING("strh");
                        WRITE_4BYTES(56);
                        WRITE_STRING("auds");
                        WRITE_4BYTES(0);
                        WRITE_4BYTES(0);
                        WRITE_4BYTES(0);
                        WRITE_4BYTES(0);

                        WRITE_4BYTES(afd.a.sampleSize);
                        WRITE_4BYTES(afd.a.sampleSize *
                                     afd.a.rate);
                        WRITE_4BYTES(0);
                        WRITE_4BYTES(afd.a.totalBytes /
                                     afd.a.sampleSize);

                        WRITE_4BYTES(0);
                        WRITE_4BYTES(-1);
                        WRITE_4BYTES(afd.a.sampleSize);
                        WRITE_2BYTES(0);
                        WRITE_2BYTES(0);
                        WRITE_2BYTES(0);
                        WRITE_2BYTES(0);

                        WRITE_STRING("strf");
                        WRITE_4BYTES(18);
                        WRITE_2BYTES(afd.a.format);
                        WRITE_2BYTES(afd.a.channels);
                        WRITE_4BYTES(afd.a.rate);
                        WRITE_4BYTES(afd.a.sampleSize *
                                     afd.a.rate);
                        WRITE_2BYTES(afd.a.sampleSize);
                        WRITE_2BYTES(afd.a.bits);
                        WRITE_2BYTES(0);
                    }
                    END_CHUNK();
                }
            }
            END_CHUNK();

            afd.moviOffset = bufIndex;

            START_CHUNK("LIST");
            {
                WRITE_STRING("movi");
            }
        }
    }
}

static qboolean CL_OpenAVIForWriting(const char* fileName, void* pipe) {
    if (afd.fileOpen)
        return qfalse;

    Com_Memset(&afd, 0, sizeof(aviFileData_t));

    if (cl_videoFrameRate->integer <= 0) {
        Com_Printf(S_COLOR_RED "cl_videoFrameRate must be >= 1\n");
        return qfalse;
    }

    afd.pipe = pipe;
    if (!pipe) {
        if ((afd.f = FS_FOpenFileWrite(fileName)) <= 0)
            return qfalse;

        if ((afd.idxF = FS_FOpenFileWrite(
                 va("%s" INDEX_FILE_EXTENSION, fileName))) <= 0) {
            FS_FCloseFile(afd.f);
            return qfalse;
        }
    }

    Q_strncpyz(afd.fileName, fileName, MAX_QPATH);

    afd.frameRate = cl_videoFrameRate->integer;
    afd.framePeriod = (int)(1000000.0f / afd.frameRate);
    afd.width = cls.glconfig.vidWidth;
    afd.height = cls.glconfig.vidHeight;

    if (cl_aviMotionJpeg->integer && !pipe)
        afd.motionJpeg = qtrue;
    else
        afd.motionJpeg = qfalse;

#define MAX_PACK_LEN 16
    afd.cBuffer = Z_Malloc((afd.width * 4 + MAX_PACK_LEN - 1) * afd.height + MAX_PACK_LEN - 1);
    afd.eBuffer = Z_Malloc(PAD(afd.width * 3, AVI_LINE_PADDING) * afd.height);

    afd.a.rate = dma.speed;
    afd.a.format = WAV_FORMAT_PCM;
    afd.a.channels = dma.channels;
    afd.a.bits = dma.samplebits;
    afd.a.sampleSize = (afd.a.bits / 8) * afd.a.channels;


    if (!Cvar_VariableIntegerValue("s_initsound")) {
        afd.audio = qfalse;
    } else if (afd.a.bits != 16 || afd.a.channels != 2) {
        Com_Printf(S_COLOR_YELLOW "WARNING: Audio format of %d bit/%d channels not supported",
                   afd.a.bits, afd.a.channels);
        afd.audio = qfalse;
    } else {
        afd.audio = qtrue;
    }

    CL_WriteAVIHeader();

    CL_AviOut(buffer, bufIndex);
    afd.fileSize = bufIndex;

    if (!pipe) {
        bufIndex = 0;
        START_CHUNK("idx1");
        SafeFS_Write(buffer, bufIndex, afd.idxF);
    }

    afd.moviSize = 4;
    afd.fileOpen = qtrue;
    com_uncappedFrames = qtrue;
    Cvar_Set("cl_videoRecording", "1");

    return qtrue;
}

static qboolean CL_CheckFileSize(int bytesToAdd) {
    unsigned int newFileSize;

    if (afd.pipe)
        return qfalse;

    newFileSize =
        afd.fileSize +
        bytesToAdd +
        (afd.numIndices * 16) +
        4;

    if (newFileSize > INT_MAX) {
        CL_CloseAVI();

        CL_OpenAVIForWriting(va("%s_", afd.fileName), NULL);

        return qtrue;
    }

    return qfalse;
}

void CL_WriteAVIVideoFrame(const byte* imageBuffer, int size) {
    int chunkOffset = afd.fileSize - afd.moviOffset - 8;
    int chunkSize = 8 + size;
    int paddingSize = PADLEN(size, 2);
    byte padding[4] = {0};

    if (!afd.fileOpen)
        return;

    if (CL_CheckFileSize(8 + size + 2))
        return;

    bufIndex = 0;
    WRITE_STRING("00dc");
    WRITE_4BYTES(size);

    CL_AviOut(buffer, 8);
    CL_AviOut(imageBuffer, size);
    CL_AviOut(padding, paddingSize);
    afd.fileSize += (chunkSize + paddingSize);

    afd.numVideoFrames++;
    afd.moviSize += (chunkSize + paddingSize);

    if (afd.pipe) {
        afd.fileSize = afd.moviSize = 0;
        return;
    }

    if (size > afd.maxRecordSize)
        afd.maxRecordSize = size;

    bufIndex = 0;
    WRITE_STRING("00dc");
    WRITE_4BYTES(0x00000010);
    WRITE_4BYTES(chunkOffset);
    WRITE_4BYTES(size);
    SafeFS_Write(buffer, 16, afd.idxF);

    afd.numIndices++;
}

#define PCM_BUFFER_SIZE 44100

void CL_WriteAVIAudioFrame(const byte* pcmBuffer, int size) {
    static byte pcmCaptureBuffer[PCM_BUFFER_SIZE] = {0};
    static int bytesInBuffer = 0;

    if (!afd.audio)
        return;

    if (!afd.fileOpen)
        return;

    if (CL_CheckFileSize(8 + bytesInBuffer + size + 2))
        return;

    if (bytesInBuffer + size > PCM_BUFFER_SIZE) {
        Com_Printf(S_COLOR_YELLOW
                   "WARNING: Audio capture buffer overflow -- truncating\n");
        size = PCM_BUFFER_SIZE - bytesInBuffer;
    }

    Com_Memcpy(&pcmCaptureBuffer[bytesInBuffer], pcmBuffer, size);
    bytesInBuffer += size;

    if (bytesInBuffer >= (int)ceil((float)afd.a.rate / (float)afd.frameRate) *
                             afd.a.sampleSize) {
        int chunkOffset = afd.fileSize - afd.moviOffset - 8;
        int chunkSize = 8 + bytesInBuffer;
        int paddingSize = PADLEN(bytesInBuffer, 2);
        byte padding[4] = {0};

        bufIndex = 0;
        WRITE_STRING("01wb");
        WRITE_4BYTES(bytesInBuffer);

        CL_AviOut(buffer, 8);
        CL_AviOut(pcmCaptureBuffer, bytesInBuffer);
        CL_AviOut(padding, paddingSize);
        afd.fileSize += (chunkSize + paddingSize);

        afd.numAudioFrames++;
        afd.moviSize += (chunkSize + paddingSize);
        afd.a.totalBytes += bytesInBuffer;

        if (afd.pipe) {
            afd.fileSize = afd.moviSize = 0;
            bytesInBuffer = 0;
            return;
        }

        bufIndex = 0;
        WRITE_STRING("01wb");
        WRITE_4BYTES(0);
        WRITE_4BYTES(chunkOffset);
        WRITE_4BYTES(bytesInBuffer);
        SafeFS_Write(buffer, 16, afd.idxF);

        afd.numIndices++;

        bytesInBuffer = 0;
    }
}

void CL_TakeVideoFrame(void) {
    if (!afd.fileOpen)
        return;

    re.TakeVideoFrame(afd.width, afd.height,
                      afd.cBuffer, afd.eBuffer, afd.motionJpeg);
}

qboolean CL_CloseAVI(void) {
    int indexRemainder;
    int indexSize = afd.numIndices * 16;
    const char* idxFileName = va("%s" INDEX_FILE_EXTENSION, afd.fileName);

    if (!afd.fileOpen)
        return qfalse;

    afd.fileOpen = qfalse;
    com_uncappedFrames = qfalse;
    Cvar_Set("cl_videoRecording", "0");

    if (afd.pipe) {
        char log[MAX_OSPATH];
        int code;

        Z_Free(afd.cBuffer);
        Z_Free(afd.eBuffer);
        code = Sys_PipeClose(afd.pipe);
        afd.pipe = NULL;
        Com_sprintf(log, sizeof(log), "%s", FS_BuildOSPath(Cvar_VariableString("fs_homepath"), FS_GetCurrentGameDir(),
                                                           va("%s.log", afd.fileName)));
        if (code) {
            Com_Printf(S_COLOR_RED "ffmpeg could not finish %s (code %d), see %s\n", afd.fileName, code, log);
            return qfalse;
        }
        remove(log);
        Com_Printf("Wrote %d:%d frames to %s\n", afd.numVideoFrames, afd.numAudioFrames, afd.fileName);
        return qtrue;
    }

    FS_Seek(afd.idxF, 4, FS_SEEK_SET);
    bufIndex = 0;
    WRITE_4BYTES(indexSize);
    SafeFS_Write(buffer, bufIndex, afd.idxF);
    FS_FCloseFile(afd.idxF);

    if ((indexSize = FS_FOpenFileRead(idxFileName,
                                      &afd.idxF, qtrue)) <= 0) {
        FS_FCloseFile(afd.f);
        return qfalse;
    }

    indexRemainder = indexSize;

    while (indexRemainder > MAX_AVI_BUFFER) {
        FS_Read(buffer, MAX_AVI_BUFFER, afd.idxF);
        SafeFS_Write(buffer, MAX_AVI_BUFFER, afd.f);
        afd.fileSize += MAX_AVI_BUFFER;
        indexRemainder -= MAX_AVI_BUFFER;
    }
    FS_Read(buffer, indexRemainder, afd.idxF);
    SafeFS_Write(buffer, indexRemainder, afd.f);
    afd.fileSize += indexRemainder;
    FS_FCloseFile(afd.idxF);

    FS_HomeRemove(idxFileName);

    FS_Seek(afd.f, 0, FS_SEEK_SET);
    CL_WriteAVIHeader();

    bufIndex = 4;
    WRITE_4BYTES(afd.fileSize - 8);

    bufIndex = afd.moviOffset + 4;
    WRITE_4BYTES(afd.moviSize);

    SafeFS_Write(buffer, bufIndex, afd.f);

    Z_Free(afd.cBuffer);
    Z_Free(afd.eBuffer);
    FS_FCloseFile(afd.f);

    Com_Printf("Wrote %d:%d frames to %s\n", afd.numVideoFrames, afd.numAudioFrames, afd.fileName);

    return qtrue;
}

qboolean CL_VideoRecording(void) {
    return afd.fileOpen;
}

#define CL_VIDEO_ARGS                                                                                                 \
    "-hide_banner -loglevel warning -f avi -i - -vf scale=out_color_matrix=bt709:out_range=tv,format=yuv420p "        \
    "-c:v libx264 -preset medium -crf %i -colorspace bt709 -color_primaries bt709 -color_trc bt709 "                  \
    "-c:a aac -b:a 320k -movflags +faststart -y \"%s\""

static void CL_VideoFileName(char* out, int size, const char* arg, const char* ext) {
    char name[64];
    int i = 0, n = 0;

    while (arg[i] && n < (int)sizeof(name) - 1) {
        char c = arg[i++];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            name[n++] = c;
        }
    }
    name[n] = '\0';
    if (!n) {
        qtime_t t;

        Com_RealTime(&t);
        Com_sprintf(name, sizeof(name), "%04d-%02d-%02d_%02d-%02d-%02d", 1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec);
    }
    Com_sprintf(out, size, "videos/%s.%s", name, ext);
    for (i = 2; i < 1000 && FS_FileExists(out); i++) {
        Com_sprintf(out, size, "videos/%s_%i.%s", name, i, ext);
    }
}

static void* CL_VideoOpenFfmpeg(const char* file) {
    char out[MAX_OSPATH];
    char log[MAX_OSPATH];
    char exe[MAX_OSPATH];
    char args[MAX_OSPATH + 400];
    FILE* f;

    Q_strncpyz(out, FS_BuildOSPath(Cvar_VariableString("fs_homepath"), FS_GetCurrentGameDir(), file), sizeof(out));
    Com_sprintf(log, sizeof(log), "%s.log", out);
    FS_CreatePath(out);
    Com_sprintf(args, sizeof(args), CL_VIDEO_ARGS, (int)Com_Clamp(0, 51, cl_videoCrf->value), out);
#ifdef _WIN32
    Com_sprintf(exe, sizeof(exe), "%s%cffmpeg.exe", Cvar_VariableString("fs_basepath"), PATH_SEP);
#else
    Com_sprintf(exe, sizeof(exe), "%s%cffmpeg", Cvar_VariableString("fs_basepath"), PATH_SEP);
#endif
    f = Sys_FOpen(exe, "rb");
    if (f) {
        fclose(f);
        return Sys_PipeOpen(exe, args, log);
    }
    return Sys_PipeOpen("ffmpeg", args, log);
}

void CL_Video_f(void) {
    char file[MAX_QPATH];
    void* pipe;

    if (!clc.demoplaying) {
        Com_Printf("video only works while playing a demo\n");
        return;
    }
    if (afd.fileOpen) {
        Com_Printf("Already recording %s\n", afd.fileName);
        return;
    }
    CL_VideoFileName(file, sizeof(file), Cmd_Argc() > 1 ? Cmd_Argv(1) : "", "mp4");
    pipe = CL_VideoOpenFfmpeg(file);
    if (!pipe) {
        Com_Printf(S_COLOR_YELLOW "ffmpeg not found: recording a large .avi instead. Put ffmpeg next to the game for .mp4\n");
        CL_VideoFileName(file, sizeof(file), Cmd_Argc() > 1 ? Cmd_Argv(1) : "", "avi");
    }
    if (!CL_OpenAVIForWriting(file, pipe)) {
        if (pipe) {
            Sys_PipeClose(pipe);
        }
        Com_Printf(S_COLOR_RED "Could not start the video\n");
        return;
    }
    Com_Printf("Recording %s at %i fps. stopvideo ends it, and so does the end of the demo.\n", file, afd.frameRate);
}

void CL_StopVideo_f(void) {
    if (afd.fileOpen) {
        CL_CloseAVI();
    }
}
