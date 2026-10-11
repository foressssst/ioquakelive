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
#ifdef _WIN32

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <initguid.h>
#include <dsound.h>

#include <SDL3/SDL.h>

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#endif

#define SECONDARY_BUFFER_SIZE 0x10000

static qboolean dsound_init;
static int sample16;
static DWORD gSndBufSize;
static DWORD locksize;
static LPDIRECTSOUND pDS;
static LPDIRECTSOUNDBUFFER pDSBuf;
static HWND dsWindow;
static qboolean dsMuted;

static byte dsScratch[SECONDARY_BUFFER_SIZE];

static const char* DSoundError(int error) {
    switch (error) {
        case DSERR_BUFFERLOST:
            return "DSERR_BUFFERLOST";
        case DSERR_INVALIDCALL:
            return "DSERR_INVALIDCALLS";
        case DSERR_INVALIDPARAM:
            return "DSERR_INVALIDPARAM";
        case DSERR_PRIOLEVELNEEDED:
            return "DSERR_PRIOLEVELNEEDED";
    }

    return "unknown";
}

static HWND SNDDMA_Window(void) {
    SDL_Window** windows;
    HWND hWnd = NULL;
    int count = 0;

    windows = SDL_GetWindows(&count);
    if (windows) {
        if (count > 0) {
            hWnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(windows[0]), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        }
        SDL_free(windows);
    }

    return hWnd ? hWnd : GetDesktopWindow();
}

void SNDDMA_Shutdown(void) {
    Com_DPrintf("Shutting down sound system\n");

    if (pDS) {
        Com_DPrintf("Destroying DS buffers\n");
        Com_DPrintf("...setting NORMAL coop level\n");
        IDirectSound_SetCooperativeLevel(pDS, dsWindow, DSSCL_PRIORITY);

        if (pDSBuf) {
            Com_DPrintf("...stopping and releasing sound buffer\n");
            IDirectSoundBuffer_Stop(pDSBuf);
            IDirectSoundBuffer_Release(pDSBuf);
        }
        pDSBuf = NULL;

        dma.buffer = NULL;

        Com_DPrintf("...releasing DS object\n");
        IDirectSound_Release(pDS);
    }

    pDS = NULL;
    pDSBuf = NULL;
    dsWindow = NULL;
    dsound_init = qfalse;
    memset((void*)&dma, 0, sizeof(dma));
    CoUninitialize();
}

static qboolean SNDDMA_InitDS(void) {
    DSBUFFERDESC dsbuf;
    DSBCAPS dsbcaps;
    WAVEFORMATEX format;
    int use8;

    Com_Printf("Initializing DirectSound\n");

    use8 = 1;
    if (FAILED(CoCreateInstance(&CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound8, (void**)&pDS))) {
        use8 = 0;
        if (FAILED(CoCreateInstance(&CLSID_DirectSound, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound, (void**)&pDS))) {
            Com_Printf("failed\n");
            SNDDMA_Shutdown();
            return qfalse;
        }
    }

    IDirectSound_Initialize(pDS, NULL);

    Com_DPrintf("ok\n");

    Com_DPrintf("...setting DSSCL_PRIORITY coop level: ");

    dsWindow = SNDDMA_Window();
    if (DS_OK != IDirectSound_SetCooperativeLevel(pDS, dsWindow, DSSCL_PRIORITY)) {
        Com_Printf("failed\n");
        SNDDMA_Shutdown();
        return qfalse;
    }
    Com_DPrintf("ok\n");

    dma.channels = 2;
    dma.samplebits = 16;
    dma.speed = 22050;

    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = dma.channels;
    format.wBitsPerSample = dma.samplebits;
    format.nSamplesPerSec = dma.speed;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    memset(&dsbuf, 0, sizeof(dsbuf));
    dsbuf.dwSize = sizeof(DSBUFFERDESC);

    dsbuf.dwFlags = DSBCAPS_LOCHARDWARE | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
    if (use8) {
        dsbuf.dwFlags |= DSBCAPS_GETCURRENTPOSITION2;
    }
    dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
    dsbuf.lpwfxFormat = &format;

    memset(&dsbcaps, 0, sizeof(dsbcaps));
    dsbcaps.dwSize = sizeof(dsbcaps);

    Com_DPrintf("...creating secondary buffer: ");
    if (DS_OK == IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL)) {
        Com_Printf("locked hardware.  ok\n");
    } else {
        dsbuf.dwFlags = DSBCAPS_LOCSOFTWARE | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
        if (use8) {
            dsbuf.dwFlags |= DSBCAPS_GETCURRENTPOSITION2;
        }
        if (DS_OK != IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL)) {
            Com_Printf("failed\n");
            SNDDMA_Shutdown();
            return qfalse;
        }
        Com_DPrintf("forced to software.  ok\n");
    }

    dsMuted = qfalse;

    if (DS_OK != IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING)) {
        Com_Printf("*** Looped sound play failed ***\n");
        SNDDMA_Shutdown();
        return qfalse;
    }

    if (DS_OK != IDirectSoundBuffer_GetCaps(pDSBuf, &dsbcaps)) {
        Com_Printf("*** GetCaps failed ***\n");
        SNDDMA_Shutdown();
        return qfalse;
    }

    gSndBufSize = dsbcaps.dwBufferBytes;

    dma.channels = format.nChannels;
    dma.samplebits = format.wBitsPerSample;
    dma.isfloat = 0;
    dma.speed = format.nSamplesPerSec;
    dma.samples = gSndBufSize / (dma.samplebits / 8);
    dma.fullsamples = dma.samples / dma.channels;
    dma.submission_chunk = 1;
    dma.buffer = NULL;

    sample16 = (dma.samplebits / 8) - 1;

    SNDDMA_BeginPainting();
    if (dma.buffer) {
        memset(dma.buffer, 0, dma.samples * dma.samplebits / 8);
    }
    SNDDMA_Submit();
    return qtrue;
}

qboolean SNDDMA_Init(void) {
    memset((void*)&dma, 0, sizeof(dma));
    dsound_init = qfalse;

    CoInitialize(NULL);

    if (!SNDDMA_InitDS()) {
        return qfalse;
    }

    dsound_init = qtrue;

    Com_DPrintf("Completed successfully\n");

    return qtrue;
}

int SNDDMA_GetDMAPos(void) {
    DWORD play;
    DWORD write;
    int s;

    if (!dsound_init) {
        return 0;
    }

    IDirectSoundBuffer_GetCurrentPosition(pDSBuf, &play, &write);

    s = play;
    s >>= sample16;
    s &= (dma.samples - 1);

    return s;
}

void SNDDMA_BeginPainting(void) {
    int reps;
    DWORD dwSize2;
    void* pbuf;
    void* pbuf2;
    HRESULT hresult;
    DWORD dwStatus;

    if (!pDSBuf) {
        return;
    }

    if (!IsWindow(dsWindow)) {
        dsWindow = SNDDMA_Window();
        if (DS_OK != IDirectSound_SetCooperativeLevel(pDS, dsWindow, DSSCL_PRIORITY)) {
            Com_Printf("sound SetCooperativeLevel failed\n");
        }
    }

    dwStatus = 0;
    if (IDirectSoundBuffer_GetStatus(pDSBuf, &dwStatus) != DS_OK) {
        Com_Printf("Couldn't get sound buffer status\n");
    }

    if (dwStatus & DSBSTATUS_BUFFERLOST) {
        IDirectSoundBuffer_Restore(pDSBuf);
    }

    if (!(dwStatus & DSBSTATUS_PLAYING)) {
        IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
    }

    reps = 0;
    dma.buffer = NULL;
    locksize = 0;

    while ((hresult = IDirectSoundBuffer_Lock(pDSBuf, 0, gSndBufSize, &pbuf, &locksize, &pbuf2, &dwSize2, 0)) != DS_OK) {
        if (hresult != DSERR_BUFFERLOST) {
            Com_Printf("SNDDMA_BeginPainting: Lock failed with error '%s'\n", DSoundError(hresult));
            break;
        }
        IDirectSoundBuffer_Restore(pDSBuf);

        if (++reps > 2) {
            break;
        }
    }

    if (hresult != DS_OK) {
        dma.buffer = dsScratch;
        locksize = 0;
        return;
    }

    dma.buffer = (unsigned char*)pbuf;
}

void SNDDMA_Submit(void) {
    if (pDSBuf && dma.buffer && dma.buffer != dsScratch) {
        IDirectSoundBuffer_Unlock(pDSBuf, dma.buffer, locksize, NULL, 0);
    }
}

void SNDDMA_Mute(qboolean mute) {
    if (!pDSBuf || mute == dsMuted) {
        return;
    }
    if (SUCCEEDED(IDirectSoundBuffer_SetVolume(pDSBuf, mute ? DSBVOLUME_MIN : DSBVOLUME_MAX))) {
        dsMuted = mute;
    }
}

#endif
