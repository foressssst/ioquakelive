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
#include <stdlib.h>
#include <stdio.h>

#include <SDL3/SDL.h>

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"
#include "../client/client.h"

qboolean snd_inited = qfalse;

cvar_t* s_sdlBits;
cvar_t* s_sdlSpeed;
cvar_t* s_sdlChannels;
cvar_t* s_sdlDevSamps;
cvar_t* s_sdlMixSamps;

static int dmapos = 0;
static int dmasize = 0;

static SDL_AudioStream* sdlPlaybackStream;

static SDL_AtomicInt sdlMuted;

static void SDLCALL SNDDMA_AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total) {
    int bytes = dma.samplebits / 8;
    int pos;

    (void)userdata;
    (void)total;

    if (additional <= 0 || !snd_inited || !dma.buffer)
        return;

    pos = dmapos * bytes;
    if (pos >= dmasize)
        pos = 0;

    while (additional > 0) {
        int chunk = dmasize - pos;

        if (chunk > additional)
            chunk = additional;

        if (SDL_GetAtomicInt(&sdlMuted)) {
            byte silence[1024];
            int left = chunk;

            memset(silence, (dma.samplebits == 8) ? 0x80 : 0, sizeof(silence));
            while (left > 0) {
                const int n = left > (int)sizeof(silence) ? (int)sizeof(silence) : left;
                SDL_PutAudioStreamData(stream, silence, n);
                left -= n;
            }
        } else {
            SDL_PutAudioStreamData(stream, dma.buffer + pos, chunk);
        }
        additional -= chunk;
        pos += chunk;

        if (pos >= dmasize)
            pos = 0;
    }

    dmapos = pos / bytes;
}

static void SNDDMA_PrintAudiospec(const char* str, const SDL_AudioSpec* spec, int samples) {
    Com_Printf("%s:\n", str);
    Com_Printf("  Format:   %s\n", SDL_GetAudioFormatName(spec->format));
    Com_Printf("  Freq:     %d\n", spec->freq);
    Com_Printf("  Samples:  %d\n", samples);
    Com_Printf("  Channels: %d\n", spec->channels);
}

qboolean SNDDMA_Init(void) {
    SDL_AudioSpec spec;
    SDL_AudioSpec device;
    int samples;
    int tmp;

    if (snd_inited)
        return qtrue;

    if (!s_sdlBits) {
        s_sdlBits = Cvar_Get("s_sdlBits", "16", CVAR_ARCHIVE);
        s_sdlSpeed = Cvar_Get("s_sdlSpeed", "0", CVAR_ARCHIVE);
        s_sdlChannels = Cvar_Get("s_sdlChannels", "2", CVAR_ARCHIVE);
        s_sdlDevSamps = Cvar_Get("s_sdlDevSamps", "0", CVAR_ARCHIVE);
        s_sdlMixSamps = Cvar_Get("s_sdlMixSamps", "0", CVAR_ARCHIVE);
    }

    Com_Printf("SDL_InitSubSystem( SDL_INIT_AUDIO )... ");

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        Com_Printf("FAILED (%s)\n", SDL_GetError());
        return qfalse;
    }

    Com_Printf("OK\n");

    Com_Printf("SDL audio driver is \"%s\".\n", SDL_GetCurrentAudioDriver());

    tmp = ((int)s_sdlBits->value);
    if ((tmp != 16) && (tmp != 8))
        tmp = 16;

    SDL_zero(spec);
    spec.freq = (int)s_sdlSpeed->value;
    if (!spec.freq)
        spec.freq = 22050;
    spec.format = ((tmp == 16) ? SDL_AUDIO_S16 : SDL_AUDIO_U8);
    spec.channels = (int)s_sdlChannels->value;

    if (s_sdlDevSamps->value)
        samples = s_sdlDevSamps->value;
    else if (spec.freq <= 11025)
        samples = 256;
    else if (spec.freq <= 22050)
        samples = 512;
    else if (spec.freq <= 44100)
        samples = 1024;
    else
        samples = 2048;

    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, va("%d", samples));

    sdlPlaybackStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, SNDDMA_AudioCallback, NULL);
    if (!sdlPlaybackStream) {
        Com_Printf("SDL_OpenAudioDeviceStream() failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return qfalse;
    }

    SNDDMA_PrintAudiospec("SDL_AudioSpec", &spec, samples);

    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(sdlPlaybackStream), &device, &tmp))
        Com_Printf("  Device:   %s, %d Hz, %d channels, %d samples\n", SDL_GetAudioFormatName(device.format), device.freq, device.channels, tmp);

    tmp = s_sdlMixSamps->value;
    if (!tmp)
        tmp = 0x10000 / (SDL_AUDIO_BITSIZE(spec.format) / 8);

    tmp -= tmp % spec.channels;

    dmapos = 0;
    dma.samplebits = SDL_AUDIO_BITSIZE(spec.format);
    dma.isfloat = SDL_AUDIO_ISFLOAT(spec.format);
    dma.channels = spec.channels;
    dma.samples = tmp;
    dma.fullsamples = dma.samples / dma.channels;
    dma.submission_chunk = 1;
    dma.speed = spec.freq;
    dmasize = (dma.samples * (dma.samplebits / 8));
    dma.buffer = calloc(1, dmasize);

    Com_Printf("Starting SDL audio callback...\n");
    SDL_ResumeAudioStreamDevice(sdlPlaybackStream);

    Com_Printf("SDL audio initialized.\n");
    snd_inited = qtrue;
    return qtrue;
}

void SNDDMA_Mute(qboolean mute) {
    SDL_SetAtomicInt(&sdlMuted, mute ? 1 : 0);
}

int SNDDMA_GetDMAPos(void) {
    return dmapos;
}

void SNDDMA_Shutdown(void) {
    if (sdlPlaybackStream) {
        Com_Printf("Closing SDL audio playback device...\n");
        SDL_DestroyAudioStream(sdlPlaybackStream);
        Com_Printf("SDL audio playback device closed.\n");
        sdlPlaybackStream = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(dma.buffer);
    dma.buffer = NULL;
    dmapos = dmasize = 0;
    snd_inited = qfalse;
    Com_Printf("SDL audio shut down.\n");
}

void SNDDMA_Submit(void) {
    if (sdlPlaybackStream)
        SDL_UnlockAudioStream(sdlPlaybackStream);
}

void SNDDMA_BeginPainting(void) {
    if (sdlPlaybackStream)
        SDL_LockAudioStream(sdlPlaybackStream);
}
