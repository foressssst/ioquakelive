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
#include "snd_local.h"
#include "snd_codec.h"
#include "client.h"

void S_Update_(void);
void S_Base_StopAllSounds(void);
void S_Base_StopBackgroundTrack(void);

snd_stream_t* s_backgroundStream = NULL;
static char s_backgroundLoop[MAX_QPATH];

#define SOUND_ATTENUATE 0.0008f
#define SOUND_MAX_DISTANCE 1250.0f

channel_t s_channels[MAX_CHANNELS];
channel_t loop_channels[MAX_CHANNELS];
int numLoopChannels;

static int s_soundStarted;
static qboolean s_soundMuted;

dma_t dma;

static int listener_number;
static vec3_t listener_origin;
static vec3_t listener_axis[3];

int s_soundtime;
int s_paintedtime;

#define MAX_SFX 4096
sfx_t s_knownSfx[MAX_SFX];
int s_numSfx = 0;

#define LOOP_HASH 128
static sfx_t* sfxHash[LOOP_HASH];

cvar_t* s_testsound;
cvar_t* s_show;
cvar_t* s_mixahead;
cvar_t* s_mixPreStep;
cvar_t* s_pvs;

static loopSound_t loopSounds[MAX_GENTITIES];
static channel_t* freelist = NULL;

int s_rawend[MAX_RAW_STREAMS];
portable_samplepair_t s_rawsamples[MAX_RAW_STREAMS][MAX_RAW_SAMPLES];

void S_Base_SoundInfo(void) {
    Com_Printf("----- Sound Info -----\n");
    if (!s_soundStarted) {
        Com_Printf("sound system not started\n");
        Com_Printf("----------------------\n");
        return;
    }
    if (s_soundMuted) {
        Com_Printf("sound system is muted\n");
    }
    Com_Printf("%5d stereo\n", dma.channels - 1);
    Com_Printf("%5d samples\n", dma.samples);
    Com_Printf("%5d samplebits\n", dma.samplebits);
    Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
    Com_Printf("%5d speed\n", dma.speed);
    Com_Printf("0x%x dma buffer\n", (unsigned int)(intptr_t)dma.buffer);
    if (s_backgroundStream) {
        Com_Printf("Background file: %s\n", s_backgroundLoop);
    } else {
        Com_Printf("No background file.\n");
    }
    Com_Printf("----------------------\n");
}

void S_Base_SoundList(void) {
    int i;
    sfx_t* sfx;

    for (sfx = s_knownSfx, i = 0; i < s_numSfx; i++, sfx++) {
        Com_Printf("%6i [%s] : %s\n", sfx->soundLength, sfx->inMemory ? "MEM" : "PGD", sfx->soundName);
    }
    Com_Printf("%i sounds loaded\n", s_numSfx);
    S_DisplayFreeMemory();
}

void S_ChannelFree(channel_t* v) {
    v->thesfx = NULL;
    *(channel_t**)v = freelist;
    freelist = (channel_t*)v;
}

channel_t* S_ChannelMalloc(void) {
    channel_t* v;
    if (freelist == NULL) {
        return NULL;
    }
    v = freelist;
    freelist = *(channel_t**)freelist;
    v->allocTime = Com_Milliseconds();
    return v;
}

void S_ChannelSetup(void) {
    channel_t *p, *q;

    Com_Memset(s_channels, 0, sizeof(s_channels));

    p = s_channels;
    ;
    q = p + MAX_CHANNELS;
    while (--q > p) {
        *(channel_t**)q = q - 1;
    }

    *(channel_t**)q = NULL;
    freelist = p + MAX_CHANNELS - 1;
    Com_DPrintf("Channel memory manager started\n");
}

static long S_HashSFXName(const char* name) {
    int i;
    long hash;
    char letter;

    hash = 0;
    i = 0;
    while (name[i] != '\0') {
        letter = tolower(name[i]);
        if (letter == '.')
            break;
        if (letter == '\\')
            letter = '/';
        hash += (long)(letter) * (i + 119);
        i++;
    }
    hash &= (LOOP_HASH - 1);
    return hash;
}

static sfx_t* S_FindName(const char* name) {
    int i;
    int hash;

    sfx_t* sfx;

    if (!name) {
        Com_Error(ERR_FATAL, "Sound name is NULL");
    }

    if (!name[0]) {
        Com_Printf(S_COLOR_YELLOW "WARNING: Sound name is empty\n");
        return NULL;
    }

    if (strlen(name) >= MAX_QPATH) {
        Com_Printf(S_COLOR_YELLOW "WARNING: Sound name is too long: %s\n", name);
        return NULL;
    }

    if (name[0] == '*') {
        Com_Printf(S_COLOR_YELLOW "WARNING: Tried to load player sound directly: %s\n", name);
        return NULL;
    }

    hash = S_HashSFXName(name);

    sfx = sfxHash[hash];
    while (sfx) {
        if (!Q_stricmp(sfx->soundName, name)) {
            return sfx;
        }
        sfx = sfx->next;
    }

    for (i = 0; i < s_numSfx; i++) {
        if (!s_knownSfx[i].soundName[0]) {
            break;
        }
    }

    if (i == s_numSfx) {
        if (s_numSfx == MAX_SFX) {
            Com_Error(ERR_FATAL, "S_FindName: out of sfx_t");
        }
        s_numSfx++;
    }

    sfx = &s_knownSfx[i];
    Com_Memset(sfx, 0, sizeof(*sfx));
    strcpy(sfx->soundName, name);

    sfx->next = sfxHash[hash];
    sfxHash[hash] = sfx;

    return sfx;
}

void S_Base_DisableSounds(void) {
    S_Base_StopAllSounds();
    s_soundMuted = qtrue;
}

sfxHandle_t S_Base_RegisterSound(const char* name, qboolean compressed) {
    sfx_t* sfx;

    if (!s_soundStarted) {
        return 0;
    }

    if (strlen(name) >= MAX_QPATH) {
        Com_Printf("Sound name exceeds MAX_QPATH\n");
        return 0;
    }

    sfx = S_FindName(name);
    if (!sfx) {
        return 0;
    }

    if (!sfx->soundData) {
        sfx->inMemory = qfalse;
        S_memoryLoad(sfx);
        if (sfx->defaultSound) {
            return 0;
        }
    } else if (sfx->defaultSound) {
        return 0;
    }

    return sfx - s_knownSfx;
}

void S_Base_BeginRegistration(void) {
    s_soundMuted = qfalse;

    if (s_numSfx == 0) {
        SND_setup();

        Com_Memset(s_knownSfx, '\0', sizeof(s_knownSfx));
        Com_Memset(sfxHash, '\0', sizeof(sfx_t*) * LOOP_HASH);

        S_Base_RegisterSound("sound/feedback/hit.wav", qfalse);
    }
}

void S_memoryLoad(sfx_t* sfx) {
    if (!S_LoadSound(sfx)) {
        Com_Printf(S_COLOR_YELLOW "WARNING: couldn't load sound: %s\n", sfx->soundName);
        sfx->defaultSound = qtrue;
    }
    sfx->inMemory = qtrue;
}

void S_SpatializeOrigin(vec3_t origin, int master_vol, int* left_vol, int* right_vol) {
    vec_t dot;
    vec_t dist;
    vec_t lscale, rscale, scale;
    vec3_t source_vec;
    vec3_t vec;

    VectorSubtract(origin, listener_origin, source_vec);

    dist = VectorNormalize(source_vec);
    if (dist >= SOUND_MAX_DISTANCE) {
        *left_vol = 0;
        *right_vol = 0;
        return;
    }
    dist *= SOUND_ATTENUATE;

    VectorRotate(source_vec, listener_axis, vec);

    dot = -vec[1];

    if (dma.channels == 1) {
        rscale = 1.0;
        lscale = 1.0;
    } else {
        rscale = 0.5 * (1.0 + dot);
        lscale = 0.5 * (1.0 - dot);
        if (rscale < 0) {
            rscale = 0;
        }
        if (lscale < 0) {
            lscale = 0;
        }
    }

    scale = (1.0 - dist) * rscale;
    *right_vol = (master_vol * scale);
    if (*right_vol < 0)
        *right_vol = 0;

    scale = (1.0 - dist) * lscale;
    *left_vol = (master_vol * scale);
    if (*left_vol < 0)
        *left_vol = 0;
}

static void S_Base_StartSoundEx(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, float volume) {
    channel_t* ch;
    sfx_t* sfx;
    int i, time;

    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    if (!origin && (entityNum < 0 || entityNum >= MAX_GENTITIES)) {
        Com_Error(ERR_DROP, "S_StartSound: bad entitynum %i", entityNum);
    }

    if (sfxHandle < 0 || sfxHandle >= s_numSfx) {
        Com_Printf(S_COLOR_YELLOW "S_StartSound: handle %i out of range\n", sfxHandle);
        return;
    }

    sfx = &s_knownSfx[sfxHandle];

    if (sfx->inMemory == qfalse) {
        S_memoryLoad(sfx);
    }

    if (s_show->integer == 1) {
        Com_Printf("%i : %s\n", s_paintedtime, sfx->soundName);
    }

    time = Com_Milliseconds();
    sfx->lastTimeUsed = time;

    if (entityNum != ENTITYNUM_WORLD) {
        for (i = 0, ch = s_channels; i < MAX_CHANNELS; i++, ch++) {
            if (ch->entnum == entityNum && ch->allocTime == time && ch->thesfx == sfx) {
                Com_DPrintf("double sound start: %d %s\n", entityNum, sfx->soundName);
                return;
            }
        }
    }

    ch = S_ChannelMalloc();
    if (!ch) {
        if (entityNum == listener_number) {
            for (i = 0; i < MAX_CHANNELS; i++) {
                if (s_channels[i].entnum != listener_number && s_channels[i].entchannel != CHAN_ANNOUNCER &&
                    (!ch || s_channels[i].allocTime < ch->allocTime)) {
                    ch = &s_channels[i];
                }
            }
        }
        if (!ch) {
            for (i = 0; i < MAX_CHANNELS; i++) {
                if (s_channels[i].entnum == entityNum && s_channels[i].entchannel != CHAN_ANNOUNCER &&
                    (!ch || s_channels[i].allocTime < ch->allocTime)) {
                    ch = &s_channels[i];
                }
            }
        }
        if (!ch) {
            for (i = 0; i < MAX_CHANNELS; i++) {
                if (s_channels[i].entnum == ENTITYNUM_WORLD && (!ch || s_channels[i].allocTime < ch->allocTime)) {
                    ch = &s_channels[i];
                }
            }
            if (!ch) {
                return;
            }
        }
        ch->allocTime = time;
    }

    if (origin) {
        VectorCopy(origin, ch->origin);
        ch->fixed_origin = qtrue;
    } else {
        ch->fixed_origin = qfalse;
    }

    ch->leftvol = ch->rightvol = (int)(volume * 127.0f);
    ch->entnum = entityNum;
    ch->thesfx = sfx;
    ch->startSample = START_SAMPLE_IMMEDIATE;
    ch->entchannel = entchannel;
    ch->doppler = qfalse;
}

void S_Base_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle) {
    S_Base_StartSoundEx(origin, entityNum, entchannel, sfxHandle, 1.0f);
}

static void S_Base_StartLocalSoundVolume(sfxHandle_t sfxHandle, int channelNum, float volume) {
    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    if (sfxHandle < 0 || sfxHandle >= s_numSfx) {
        Com_Printf(S_COLOR_YELLOW "S_StartLocalSound: handle %i out of range\n", sfxHandle);
        return;
    }

    S_Base_StartSoundEx(NULL, listener_number, channelNum, sfxHandle, volume);
}

void S_Base_StartLocalSound(sfxHandle_t sfxHandle, int channelNum) {
    S_Base_StartLocalSoundVolume(sfxHandle, channelNum, 1.0f);
}

void S_Base_ClearSoundBuffer(void) {
    int clear;

    if (!s_soundStarted)
        return;

    Com_Memset(loopSounds, 0, MAX_GENTITIES * sizeof(loopSound_t));
    Com_Memset(loop_channels, 0, MAX_CHANNELS * sizeof(channel_t));
    numLoopChannels = 0;

    S_ChannelSetup();

    Com_Memset(s_rawend, '\0', sizeof(s_rawend));

    if (dma.samplebits == 8)
        clear = 0x80;
    else
        clear = 0;

    SNDDMA_BeginPainting();
    if (dma.buffer)
        Com_Memset(dma.buffer, clear, dma.samples * dma.samplebits / 8);
    SNDDMA_Submit();
}

void S_Base_StopAllSounds(void) {
    if (!s_soundStarted) {
        return;
    }

    S_Base_StopBackgroundTrack();

    S_Base_ClearSoundBuffer();
}

void S_Base_StopLoopingSound(int entityNum) {
    loopSounds[entityNum].active = qfalse;
}

void S_Base_ClearLoopingSounds(qboolean killall) {
    int i;

    for (i = 0; i < MAX_GENTITIES; i++) {
        loopSounds[i].active = qfalse;
    }
    numLoopChannels = 0;
}

void S_Base_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle) {
    sfx_t* sfx;
    loopSound_t* loop;

    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    if (sfxHandle < 0 || sfxHandle >= s_numSfx) {
        Com_Printf(S_COLOR_YELLOW "S_AddLoopingSound: handle %i out of range\n", sfxHandle);
        return;
    }

    sfx = &s_knownSfx[sfxHandle];

    if (sfx->inMemory == qfalse) {
        S_memoryLoad(sfx);
    }

    if (!sfx->soundLength) {
        Com_Error(ERR_DROP, "%s has length 0", sfx->soundName);
    }

    loop = &loopSounds[entityNum];
    VectorCopy(origin, loop->origin);
    VectorCopy(velocity, loop->velocity);
    loop->active = qtrue;
    loop->doppler = qfalse;
    loop->sfx = sfx;
    loop->oldDopplerScale = 1.0f;
    loop->dopplerScale = 1.0f;

    if (s_doppler->integer && VectorLengthSquared(velocity) > 0.0f) {
        vec3_t out;
        float lena, lenb;

        loop->doppler = qtrue;
        lena = DistanceSquared(loopSounds[listener_number].origin, loop->origin);
        VectorAdd(loop->origin, loop->velocity, out);
        lenb = DistanceSquared(loopSounds[listener_number].origin, out);
        loop->oldDopplerScale = 1.0f;
        loop->dopplerScale = lenb / (lena * 100);
        if (loop->dopplerScale <= 1.0f) {
            loop->doppler = qfalse;
        }
    }

    loop->framenum = cls.framecount;
}

void S_Base_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle) {
    S_Base_AddLoopingSound(entityNum, origin, velocity, sfxHandle);
}

void S_AddLoopSounds(void) {
    int i, j, time;
    int left_total, right_total, left, right;
    channel_t* ch;
    loopSound_t *loop, *loop2;
    static int loopFrame;

    numLoopChannels = 0;

    time = Com_Milliseconds();

    loopFrame++;
    for (i = 0; i < MAX_GENTITIES; i++) {
        loop = &loopSounds[i];
        if (!loop->active || loop->mergeFrame == loopFrame) {
            continue;
        }

        S_SpatializeOrigin(loop->origin, 127, &left_total, &right_total);
        loop->sfx->lastTimeUsed = time;

        for (j = (i + 1); j < MAX_GENTITIES; j++) {
            loop2 = &loopSounds[j];
            if (!loop2->active || loop2->doppler || loop2->sfx != loop->sfx) {
                continue;
            }
            loop2->mergeFrame = loopFrame;

            S_SpatializeOrigin(loop2->origin, 127, &left, &right);
            loop2->sfx->lastTimeUsed = time;
            left_total += left;
            right_total += right;
        }
        if (left_total == 0 && right_total == 0) {
            continue;
        }

        ch = &loop_channels[numLoopChannels];

        if (left_total > 255) {
            left_total = 255;
        }
        if (right_total > 255) {
            right_total = 255;
        }

        ch->leftvol = left_total;
        ch->rightvol = right_total;
        ch->thesfx = loop->sfx;
        ch->doppler = loop->doppler;
        ch->dopplerScale = loop->dopplerScale;
        ch->oldDopplerScale = loop->oldDopplerScale;
        numLoopChannels++;
        if (numLoopChannels == MAX_CHANNELS) {
            return;
        }
    }
}

void S_Base_RawSamples(int stream, int samples, int rate, int width, int s_channels, const byte* data, float volume, int entityNum) {
    int i;
    int src, dst;
    float scale;
    int intVolumeLeft, intVolumeRight;
    portable_samplepair_t* rawsamples;

    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    if ((stream < 0) || (stream >= MAX_RAW_STREAMS)) {
        return;
    }

    rawsamples = s_rawsamples[stream];

    {
        int leftvol, rightvol;

        if (entityNum >= 0 && entityNum < MAX_GENTITIES) {
            S_SpatializeOrigin(loopSounds[entityNum].origin, 256, &leftvol, &rightvol);
        } else {
            leftvol = rightvol = 256;
        }

        intVolumeLeft = leftvol * volume * s_volume->value;
        intVolumeRight = rightvol * volume * s_volume->value;
    }

    if (s_rawend[stream] < s_soundtime) {
        Com_DPrintf("S_Base_RawSamples: resetting minimum: %i < %i\n", s_rawend[stream], s_soundtime);
        s_rawend[stream] = s_soundtime;
    }

    scale = (float)rate / dma.speed;

    if (s_channels == 2 && width == 2) {
        if (scale == 1.0) {
            for (i = 0; i < samples; i++) {
                dst = s_rawend[stream] & (MAX_RAW_SAMPLES - 1);
                s_rawend[stream]++;
                rawsamples[dst].left = ((short*)data)[i * 2] * intVolumeLeft;
                rawsamples[dst].right = ((short*)data)[i * 2 + 1] * intVolumeRight;
            }
        } else {
            for (i = 0;; i++) {
                src = i * scale;
                if (src >= samples)
                    break;
                dst = s_rawend[stream] & (MAX_RAW_SAMPLES - 1);
                s_rawend[stream]++;
                rawsamples[dst].left = ((short*)data)[src * 2] * intVolumeLeft;
                rawsamples[dst].right = ((short*)data)[src * 2 + 1] * intVolumeRight;
            }
        }
    } else if (s_channels == 1 && width == 2) {
        for (i = 0;; i++) {
            src = i * scale;
            if (src >= samples)
                break;
            dst = s_rawend[stream] & (MAX_RAW_SAMPLES - 1);
            s_rawend[stream]++;
            rawsamples[dst].left = ((short*)data)[src] * intVolumeLeft;
            rawsamples[dst].right = ((short*)data)[src] * intVolumeRight;
        }
    } else if (s_channels == 2 && width == 1) {
        intVolumeLeft *= 256;
        intVolumeRight *= 256;

        for (i = 0;; i++) {
            src = i * scale;
            if (src >= samples)
                break;
            dst = s_rawend[stream] & (MAX_RAW_SAMPLES - 1);
            s_rawend[stream]++;
            rawsamples[dst].left = ((char*)data)[src * 2] * intVolumeLeft;
            rawsamples[dst].right = ((char*)data)[src * 2 + 1] * intVolumeRight;
        }
    } else if (s_channels == 1 && width == 1) {
        intVolumeLeft *= 256;
        intVolumeRight *= 256;

        for (i = 0;; i++) {
            src = i * scale;
            if (src >= samples)
                break;
            dst = s_rawend[stream] & (MAX_RAW_SAMPLES - 1);
            s_rawend[stream]++;
            rawsamples[dst].left = (((byte*)data)[src] - 128) * intVolumeLeft;
            rawsamples[dst].right = (((byte*)data)[src] - 128) * intVolumeRight;
        }
    }

    if (s_rawend[stream] > s_soundtime + MAX_RAW_SAMPLES) {
        Com_DPrintf("S_Base_RawSamples: overflowed %i > %i\n", s_rawend[stream], s_soundtime);
    }
}

void S_Base_UpdateEntityPosition(int entityNum, const vec3_t origin) {
    if (entityNum < 0 || entityNum >= MAX_GENTITIES) {
        Com_Error(ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum);
    }
    VectorCopy(origin, loopSounds[entityNum].origin);
}

void S_Base_Respatialize(int entityNum, const vec3_t head, vec3_t axis[3], int inwater) {
    int i;
    channel_t* ch;
    vec3_t origin;

    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    listener_number = entityNum;
    VectorCopy(head, listener_origin);
    VectorCopy(axis[0], listener_axis[0]);
    VectorCopy(axis[1], listener_axis[1]);
    VectorCopy(axis[2], listener_axis[2]);

    ch = s_channels;
    for (i = 0; i < MAX_CHANNELS; i++, ch++) {
        if (!ch->thesfx) {
            continue;
        }
        if (ch->entnum == listener_number && !inwater) {
            continue;
        }

        if (ch->fixed_origin) {
            VectorCopy(ch->origin, origin);
        } else {
            VectorCopy(loopSounds[ch->entnum].origin, origin);
        }

        if (s_pvs->integer && !re.inPVS(listener_origin, origin)) {
            ch->leftvol = 0;
            ch->rightvol = 0;
            continue;
        }

        S_SpatializeOrigin(origin, 127, &ch->leftvol, &ch->rightvol);
    }

    S_AddLoopSounds();
}

qboolean S_ScanChannelStarts(void) {
    channel_t* ch;
    int i;
    qboolean newSamples;

    newSamples = qfalse;
    ch = s_channels;

    for (i = 0; i < MAX_CHANNELS; i++, ch++) {
        if (!ch->thesfx) {
            continue;
        }
        if (ch->startSample == START_SAMPLE_IMMEDIATE) {
            ch->startSample = s_paintedtime;
            newSamples = qtrue;
            continue;
        }

        if (ch->startSample + (ch->thesfx->soundLength) <= s_paintedtime) {
            S_ChannelFree(ch);
        }
    }

    return newSamples;
}

void S_Base_Update(void) {
    int i;
    int total;
    channel_t* ch;

    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    if (s_show->integer == 2) {
        total = 0;
        ch = s_channels;
        for (i = 0; i < MAX_CHANNELS; i++, ch++) {
            if (ch->thesfx && (ch->leftvol || ch->rightvol)) {
                Com_Printf("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName);
                total++;
            }
        }

        Com_Printf("----(%i)---- painted: %i\n", total, s_paintedtime);
    }

    S_UpdateBackgroundTrack();

    S_Update_();
}

void S_GetSoundtime(void) {
    int samplepos;
    static int buffers;
    static int oldsamplepos;

    if (CL_VideoRecording()) {
        float fps = MIN(cl_aviFrameRate->value, 1000.0f);
        float frameDuration = MAX(dma.speed / fps, 1.0f) + clc.aviSoundFrameRemainder;

        int msec = (int)frameDuration;
        s_soundtime += msec;
        clc.aviSoundFrameRemainder = frameDuration - msec;

        return;
    }

    samplepos = SNDDMA_GetDMAPos();
    if (samplepos < oldsamplepos) {
        buffers++;

        if (s_paintedtime > 0x40000000) {
            buffers = 0;
            s_paintedtime = dma.fullsamples;
            S_Base_StopAllSounds();
        }
    }
    oldsamplepos = samplepos;

    s_soundtime = buffers * dma.fullsamples + samplepos / dma.channels;

#if 0
	if (s_paintedtime < s_soundtime)
	{
		Com_DPrintf ("S_Update_ : overflow\n");
		s_paintedtime = s_soundtime;
	}
#endif

    if (dma.submission_chunk < 256) {
        s_paintedtime = s_soundtime + s_mixPreStep->value * dma.speed;
    } else {
        s_paintedtime = s_soundtime + dma.submission_chunk;
    }
}

void S_Update_(void) {
    unsigned endtime;
    static int ot = -1;

    if (!s_soundStarted || s_soundMuted) {
        return;
    }

    S_GetSoundtime();

    endtime = (int)floor(s_mixahead->value * (double)dma.speed + (double)s_soundtime + 0.5);

    if (s_soundtime == ot) {
        return;
    }
    ot = s_soundtime;

    S_ScanChannelStarts();

    endtime = (endtime + dma.submission_chunk - 1) & ~(dma.submission_chunk - 1);

    if (endtime - s_soundtime > dma.fullsamples)
        endtime = s_soundtime + dma.fullsamples;

    SNDDMA_BeginPainting();

    S_PaintChannels(endtime);

    SNDDMA_Submit();
}

void S_Base_StopBackgroundTrack(void) {
    if (!s_backgroundStream)
        return;
    S_CodecCloseStream(s_backgroundStream);
    s_backgroundStream = NULL;
    s_rawend[0] = 0;
}

static void S_OpenBackgroundStream(const char* filename) {
    if (s_backgroundStream) {
        S_CodecCloseStream(s_backgroundStream);
        s_backgroundStream = NULL;
    }

    s_backgroundStream = S_CodecOpenStream(filename);
    if (!s_backgroundStream) {
        Com_Printf(S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", filename);
        return;
    }
}

void S_Base_StartBackgroundTrack(const char* intro, const char* loop) {
    if (!intro) {
        intro = "";
    }
    if (!loop || !loop[0]) {
        loop = intro;
    }
    Com_DPrintf("S_StartBackgroundTrack( %s, %s )\n", intro, loop);

    if (!*intro) {
        S_Base_StopBackgroundTrack();
        return;
    }

    Q_strncpyz(s_backgroundLoop, loop, sizeof(s_backgroundLoop));

    S_OpenBackgroundStream(intro);
}

void S_Base_ClearBackgroundLoop(void) {
    s_backgroundLoop[0] = '\0';
}

void S_UpdateBackgroundTrack(void) {
    static float musicVolume;
    int bufferSamples;
    int fileSamples;
    byte raw[30000];
    int fileBytes;
    int r;

    if (!s_backgroundStream) {
        return;
    }

    musicVolume = (s_musicVolume->value + s_musicVolume->value + musicVolume) * 0.25f;

    if (musicVolume <= 0) {
        return;
    }

    if (s_rawend[0] < s_soundtime) {
        s_rawend[0] = s_soundtime;
    }

    while (s_rawend[0] < s_soundtime + MAX_RAW_SAMPLES) {
        bufferSamples = MAX_RAW_SAMPLES - (s_rawend[0] - s_soundtime);

        fileSamples = bufferSamples * s_backgroundStream->info.rate / dma.speed;

        if (!fileSamples)
            return;

        fileBytes = fileSamples * (s_backgroundStream->info.width * s_backgroundStream->info.channels);
        if (fileBytes > sizeof(raw)) {
            fileBytes = sizeof(raw);
            fileSamples = fileBytes / (s_backgroundStream->info.width * s_backgroundStream->info.channels);
        }

        r = S_CodecReadStream(s_backgroundStream, fileBytes, raw);
        if (r < fileBytes) {
            fileSamples = r / (s_backgroundStream->info.width * s_backgroundStream->info.channels);
        }

        if (r > 0) {
            S_Base_RawSamples(0, fileSamples, s_backgroundStream->info.rate,
                              s_backgroundStream->info.width, s_backgroundStream->info.channels, raw, musicVolume, -1);
        } else {
            if (s_backgroundLoop[0]) {
                S_OpenBackgroundStream(s_backgroundLoop);
                if (!s_backgroundStream)
                    return;
            } else {
                S_Base_StopBackgroundTrack();
                return;
            }
        }
    }
}

void S_FreeOldestSound(void) {
    int i, oldest, used;
    sfx_t* sfx;
    sndBuffer *buffer, *nbuffer;

    oldest = Com_Milliseconds();
    used = 0;

    for (i = 1; i < s_numSfx; i++) {
        sfx = &s_knownSfx[i];
        if (sfx->inMemory && sfx->lastTimeUsed < oldest) {
            used = i;
            oldest = sfx->lastTimeUsed;
        }
    }

    sfx = &s_knownSfx[used];

    Com_DPrintf("S_FreeOldestSound: freeing sound %s\n", sfx->soundName);

    buffer = sfx->soundData;
    while (buffer != NULL) {
        nbuffer = buffer->next;
        SND_free(buffer);
        buffer = nbuffer;
    }
    sfx->inMemory = qfalse;
    sfx->soundData = NULL;
}

qboolean S_Base_ReopenDevice(void) {
    int speed = dma.speed;
    int channels = dma.channels;
    int samplebits = dma.samplebits;

    if (!s_soundStarted) {
        return qfalse;
    }

    SNDDMA_Shutdown();
    if (!SNDDMA_Init()) {
        s_soundStarted = 0;
        return qfalse;
    }
    if (dma.speed != speed || dma.channels != channels || dma.samplebits != samplebits) {
        return qfalse;
    }

    S_Base_StopAllSounds();
    return qtrue;
}

void S_Base_Shutdown(void) {
    if (!s_soundStarted) {
        return;
    }

    SNDDMA_Shutdown();
    SND_shutdown();

    s_soundStarted = 0;
    s_numSfx = 0;

    Cmd_RemoveCommand("s_info");
}

qboolean S_Base_Init(soundInterface_t* si) {
    qboolean r;

    if (!si) {
        return qfalse;
    }

    r = SNDDMA_Init();

    if (r) {
        s_soundStarted = 1;
        s_soundMuted = 1;

        Com_Memset(sfxHash, 0, sizeof(sfx_t*) * LOOP_HASH);

        s_soundtime = 0;
        s_paintedtime = 0;

        S_Base_StopAllSounds();
    } else {
        return qfalse;
    }

    si->Shutdown = S_Base_Shutdown;
    si->StartSound = S_Base_StartSound;
    si->StartLocalSound = S_Base_StartLocalSound;
    si->StartLocalSoundVolume = S_Base_StartLocalSoundVolume;
    si->StartBackgroundTrack = S_Base_StartBackgroundTrack;
    si->StopBackgroundTrack = S_Base_StopBackgroundTrack;
    si->RawSamples = S_Base_RawSamples;
    si->StopAllSounds = S_Base_StopAllSounds;
    si->ClearLoopingSounds = S_Base_ClearLoopingSounds;
    si->AddLoopingSound = S_Base_AddLoopingSound;
    si->AddRealLoopingSound = S_Base_AddRealLoopingSound;
    si->StopLoopingSound = S_Base_StopLoopingSound;
    si->Respatialize = S_Base_Respatialize;
    si->UpdateEntityPosition = S_Base_UpdateEntityPosition;
    si->Update = S_Base_Update;
    si->DisableSounds = S_Base_DisableSounds;
    si->BeginRegistration = S_Base_BeginRegistration;
    si->RegisterSound = S_Base_RegisterSound;
    si->ClearSoundBuffer = S_Base_ClearSoundBuffer;
    si->SoundInfo = S_Base_SoundInfo;
    si->SoundList = S_Base_SoundList;
    si->SetMute = SNDDMA_Mute;

    return qtrue;
}
