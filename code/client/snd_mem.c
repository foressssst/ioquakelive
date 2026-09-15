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
qboolean S_LoadSound(sfx_t* sfx) {
    byte* data;
    snd_info_t info;
    char name[MAX_QPATH];

    // [QL] quakelive_steam.exe 0x4dbd10: when the file is missing its name loses the extension and
    // defaults to .ogg, so "x.wav" loads "x.ogg" and a map's "x.ogg.wav" loads "x.ogg"
    Q_strncpyz(name, sfx->soundName, sizeof(name));
    if (FS_ReadFile(name, NULL) < 0) {
        COM_StripExtension(sfx->soundName, name, sizeof(name));
        COM_DefaultExtension(name, sizeof(name), ".ogg");
    }

    // load it in
    data = S_CodecLoad(name, &info);
    if (!data)
        return qfalse;

    if (info.width == 1) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: %s is an 8-bit audio file\n", sfx->soundName);
    }

    if (info.rate != 22050) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: %s is not a 22kHz audio file\n", sfx->soundName);
    }

    sfx->soundData = NULL;
    sfx->soundLength = ResampleSfx(sfx, info.channels, info.rate, info.width, info.samples, data + info.dataofs);
    sfx->soundChannels = info.channels;

    Hunk_FreeTempMemory(data);

    sfx->lastTimeUsed = Com_Milliseconds();

    return qtrue;
}

// [QL] quakelive_steam.exe 0x4dbbe0
void S_DisplayFreeMemory(void) {
    Com_Printf("%d bytes sound buffer memory in use, %d free \n", totalInUse, inUse);
}
