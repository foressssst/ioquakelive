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

/*****************************************************************************
 * name:		snd_mem.c
 *
 * desc:		sound caching
 *
 *****************************************************************************/

#include "snd_local.h"
#include "snd_codec.h"

// [QL] quakelive_steam.exe 0x4dbb30 registers com_soundMegs with "16"
#define DEF_COMSOUNDMEGS "16"

/*
===============================================================================

memory management

===============================================================================
*/

static sndBuffer* buffer = NULL;
static sndBuffer* freelist = NULL;
static int inUse = 0;
static int totalInUse = 0;

void SND_free(sndBuffer* v) {
    *(sndBuffer**)v = freelist;
    freelist = (sndBuffer*)v;
    inUse += sizeof(sndBuffer);
}

sndBuffer* SND_malloc(void) {
    sndBuffer* v;
redo:
    if (freelist == NULL) {
        S_FreeOldestSound();
        goto redo;
    }

    inUse -= sizeof(sndBuffer);
    totalInUse += sizeof(sndBuffer);

    v = freelist;
    freelist = *(sndBuffer**)freelist;
    v->next = NULL;
    return v;
}

void SND_setup(void) {
    sndBuffer *p, *q;
    cvar_t* cv;
    int scs;

    cv = Cvar_Get("com_soundMegs", DEF_COMSOUNDMEGS, CVAR_LATCH | CVAR_ARCHIVE);

    scs = (cv->integer * 1536);

    buffer = malloc(scs * sizeof(sndBuffer));

    inUse = scs * sizeof(sndBuffer);
    p = buffer;
    ;
    q = p + scs;
    while (--q > p)
        *(sndBuffer**)q = q - 1;

    *(sndBuffer**)q = NULL;
    freelist = p + scs - 1;

    Com_Printf("Sound memory manager started\n");
}

void SND_shutdown(void) {
    free(buffer);
}

/*
================
ResampleSfx

resample / decimate to the current source rate

[QL] quakelive_steam.exe 0x4dbc00: nearest sample, no filtering
================
*/
static int ResampleSfx(sfx_t* sfx, int channels, int inrate, int inwidth, int samples, byte* data) {
    int outcount;
    int srcsample;
    float stepscale;
    int i, j;
    int sample, samplefrac, fracstep;
    int part;
    sndBuffer* chunk;

    stepscale = (float)inrate / dma.speed;  // this is usually 0.5, 1, or 2

    outcount = samples / stepscale;

    srcsample = 0;
    samplefrac = 0;
    fracstep = stepscale * 256 * channels;
    chunk = sfx->soundData;

    for (i = 0; i < outcount; i++) {
        srcsample += samplefrac >> 8;
        samplefrac &= 255;
        samplefrac += fracstep;
        for (j = 0; j < channels; j++) {
            if (inwidth == 2) {
                sample = (((short*)data)[srcsample + j]);
            } else {
                sample = (unsigned int)((unsigned char)(data[srcsample + j]) - 128) << 8;
            }
            part = (i * channels + j) & (SND_CHUNK_SIZE - 1);
            if (part == 0) {
                sndBuffer* newchunk;
                newchunk = SND_malloc();
                if (chunk == NULL) {
                    sfx->soundData = newchunk;
                } else {
                    chunk->next = newchunk;
                }
                chunk = newchunk;
            }

            chunk->sndChunk[part] = sample;
        }
    }

    return outcount;
}

//=============================================================================

/*
==============
S_LoadSound

The filename may be different than sfx->name in the case
of a forced fallback of a player specific sound
==============
*/
/*
===============================================================================

SOUNDS DECODED IN THE BACKGROUND

[ioquakelive] Decoding an OGG is 90% of what loading a map's sounds costs. And a sound is not
needed until it plays, so while a map loads the main thread only reads the file and hands the
decoding to a worker; when the load ends they are all waited on together and converted to the
mixer's format here, on the main thread, because the sound allocator is not thread safe.

This only happens while a map loads. Outside that, and with s_asyncLoad 0, loading works as it
always did.

===============================================================================
*/

#define MAX_SOUND_JOBS 1024

typedef struct {
    sfx_t* sfx;
    byte* file;  // the file's bytes, copied: the engine's temporary memory is a stack
    int fileLen;
    byte* pcm;  // what the worker leaves behind
    snd_info_t info;
    volatile int state;  // 1 trabajando, 2 listo, 3 fallo
} soundJob_t;

static soundJob_t s_soundJobs[MAX_SOUND_JOBS];
static int s_soundJobCount;
static qboolean s_asyncActive;
static cvar_t* s_asyncLoad;

#ifdef USE_CODEC_VORBIS
static void S_DecodeJob(void* data) {
    soundJob_t* job = (soundJob_t*)data;

    job->pcm = S_OGG_DecodeMemory(job->file, job->fileLen, &job->info);
    job->state = job->pcm ? 2 : 3;
}
#endif

// opens the background loading window, when there are workers
void S_AsyncLoadBegin(void) {
    if (!s_asyncLoad) {
        s_asyncLoad = Cvar_Get("s_asyncLoad", "1", CVAR_ARCHIVE);
    }
    S_AsyncLoadFinish();
    s_asyncActive = (qboolean)(Com_JobWorkers() > 0 && s_asyncLoad->integer);
}

// waits on everything handed out and leaves it ready to play
void S_AsyncLoadFinish(void) {
    int i;

    for (i = 0; i < s_soundJobCount; i++) {
        soundJob_t* job = &s_soundJobs[i];

        while (job->state == 1) {
            Sys_ThreadYield();
        }
        if (job->state == 2) {
            job->sfx->soundData = NULL;
            job->sfx->soundLength = ResampleSfx(job->sfx, job->info.channels, job->info.rate, job->info.width,
                                                job->info.samples, job->pcm + job->info.dataofs);
            job->sfx->soundChannels = job->info.channels;
        } else {
            Com_Printf(S_COLOR_YELLOW "WARNING: couldn't load sound: %s\n", job->sfx->soundName);
            job->sfx->defaultSound = qtrue;
        }
        if (job->pcm) {
            free(job->pcm);
            job->pcm = NULL;
        }
        if (job->file) {
            free(job->file);
            job->file = NULL;
        }
    }
    s_soundJobCount = 0;
    s_asyncActive = qfalse;
}

// If the load was cut short and sound is shut down, what was handed out is thrown away: the
// workers are waited on, so their memory is not freed under them, and nothing is converted.
void S_AsyncLoadDiscard(void) {
    int i;

    for (i = 0; i < s_soundJobCount; i++) {
        soundJob_t* job = &s_soundJobs[i];

        while (job->state == 1) {
            Sys_ThreadYield();
        }
        if (job->pcm) {
            free(job->pcm);
            job->pcm = NULL;
        }
        if (job->file) {
            free(job->file);
            job->file = NULL;
        }
    }
    s_soundJobCount = 0;
    s_asyncActive = qfalse;
}

// when the sound is an OGG and a map is loading, hands it out and returns qtrue
static qboolean S_LoadSoundAsync(sfx_t* sfx, const char* name) {
#ifndef USE_CODEC_VORBIS
    // with no Vorbis there is no OGG decoder, so everything takes the usual path
    (void)sfx;
    (void)name;
    return qfalse;
#else
    soundJob_t* job;
    void* fileData = NULL;
    int len;

    if (!s_asyncActive || s_soundJobCount >= MAX_SOUND_JOBS || Q_stricmp(COM_GetExtension(name), "ogg")) {
        return qfalse;
    }
    len = FS_ReadFile(name, &fileData);
    if (len <= 0 || !fileData) {
        if (fileData) {
            FS_FreeFile(fileData);
        }
        return qfalse;
    }

    job = &s_soundJobs[s_soundJobCount];
    job->file = (byte*)malloc(len);
    if (!job->file) {
        FS_FreeFile(fileData);
        return qfalse;
    }
    Com_Memcpy(job->file, fileData, len);
    FS_FreeFile(fileData);

    job->sfx = sfx;
    job->fileLen = len;
    job->pcm = NULL;
    job->state = 1;
    s_soundJobCount++;

    // there is no data until it completes; nothing plays while the map loads
    sfx->soundData = NULL;
    sfx->soundLength = 0;
    sfx->lastTimeUsed = Com_Milliseconds();

    Com_JobsSubmit(S_DecodeJob, job);
    return qtrue;
#endif
}

// [ioquakelive] where the sound loading time goes, to know what can be spread across threads;
// /soundload prints it
static int64_t s_loadLookupUs, s_loadCodecUs, s_loadResampleUs;
static int s_loadWav, s_loadOgg;

void S_SoundLoadReport_f(void) {
    Com_Printf("sounds: %i wav, %i ogg | %.2f s finding, %.2f s reading and decoding, %.2f s resampling\n",
               s_loadWav, s_loadOgg, s_loadLookupUs / 1000000.0f, s_loadCodecUs / 1000000.0f,
               s_loadResampleUs / 1000000.0f);
    s_loadLookupUs = s_loadCodecUs = s_loadResampleUs = 0;
    s_loadWav = s_loadOgg = 0;
}

qboolean S_LoadSound(sfx_t* sfx) {
    byte* data;
    snd_info_t info;
    char name[MAX_QPATH];
    int64_t start;

    // [QL] quakelive_steam.exe 0x4dbd10: when the file is missing its name loses the extension and
    // defaults to .ogg, so "x.wav" loads "x.ogg" and a map's "x.ogg.wav" loads "x.ogg"
    start = Sys_Microseconds();
    Q_strncpyz(name, sfx->soundName, sizeof(name));
    if (FS_ReadFile(name, NULL) < 0) {
        COM_StripExtension(sfx->soundName, name, sizeof(name));
        COM_DefaultExtension(name, sizeof(name), ".ogg");
    }
    s_loadLookupUs += Sys_Microseconds() - start;

    // [ioquakelive] while a map loads, an OGG is decoded in the background
    start = Sys_Microseconds();
    if (S_LoadSoundAsync(sfx, name)) {
        s_loadCodecUs += Sys_Microseconds() - start;
        s_loadOgg++;
        return qtrue;
    }

    // load it in
    start = Sys_Microseconds();
    data = S_CodecLoad(name, &info);
    s_loadCodecUs += Sys_Microseconds() - start;
    if (!data)
        return qfalse;
    if (!Q_stricmp(COM_GetExtension(name), "ogg")) {
        s_loadOgg++;
    } else {
        s_loadWav++;
    }

    if (info.width == 1) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: %s is an 8-bit audio file\n", sfx->soundName);
    }

    if (info.rate != 22050) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: %s is not a 22kHz audio file\n", sfx->soundName);
    }

    sfx->soundData = NULL;
    start = Sys_Microseconds();
    sfx->soundLength = ResampleSfx(sfx, info.channels, info.rate, info.width, info.samples, data + info.dataofs);
    s_loadResampleUs += Sys_Microseconds() - start;
    sfx->soundChannels = info.channels;

    Hunk_FreeTempMemory(data);

    sfx->lastTimeUsed = Com_Milliseconds();

    return qtrue;
}

// [QL] quakelive_steam.exe 0x4dbbe0
void S_DisplayFreeMemory(void) {
    Com_Printf("%d bytes sound buffer memory in use, %d free \n", totalInUse, inUse);
}
