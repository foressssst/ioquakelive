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
// win_snd.c -- DirectSound, as quakelive_steam.exe drives it (0x4efd70)

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
static qboolean dsMuted;  // [ioquakelive] the buffer's volume is at its minimum

// where the mixer paints when the buffer cannot be locked, so a lost lock is silence
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

/*
==================
SNDDMA_Window

The game window, which belongs to SDL and is made again on a vid_restart
==================
*/
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

/*
==================
SNDDMA_Shutdown
==================
*/
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

/*
==================
SNDDMA_InitDS
==================
*/
static qboolean SNDDMA_InitDS(void) {
    DSBUFFERDESC dsbuf;
    DSBCAPS dsbcaps;
    WAVEFORMATEX format;
    int use8;

    Com_Printf("Initializing DirectSound\n");

    use8 = 1;
    // Create IDirectSound using the primary sound device
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

    // create the secondary buffer we'll actually work with
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

    // take advantage of 2D hardware if available. [QL] 0x4efd70: retail only asks for global
    // focus when s_muteBackground is 0. With the mute on, the buffer belongs to the window and
    // Windows silences it the moment it loses focus, which is instant; muting it by hand while
    // the game runs slowed down behind another window left the device repeating what it had
    // [ioquakelive] global focus always, and the mute is done with the buffer's volume
    // (SNDDMA_Mute): Windows 11 does not silence a buffer without it when the window loses focus
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
        // Couldn't get hardware, fallback to software.
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

    // Make sure mixer is active
    if (DS_OK != IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING)) {
        Com_Printf("*** Looped sound play failed ***\n");
        SNDDMA_Shutdown();
        return qfalse;
    }

    // get the returned buffer size
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
    dma.buffer = NULL;  // must be locked first

    sample16 = (dma.samplebits / 8) - 1;

    SNDDMA_BeginPainting();
    if (dma.buffer) {
        memset(dma.buffer, 0, dma.samples * dma.samplebits / 8);
    }
    SNDDMA_Submit();
    return qtrue;
}

/*
==================
SNDDMA_Init

Initialize direct sound
Returns false if failed
==================
*/
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

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
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

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
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

    // a vid_restart made the window again: the coop level follows it, as retail's does on activation
    if (!IsWindow(dsWindow)) {
        dsWindow = SNDDMA_Window();
        if (DS_OK != IDirectSound_SetCooperativeLevel(pDS, dsWindow, DSSCL_PRIORITY)) {
            Com_Printf("sound SetCooperativeLevel failed\n");
        }
    }

    // if the buffer was lost or stopped, restore it and/or restart it
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

    // lock the dsound buffer
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

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the dsound buffer
===============
*/
void SNDDMA_Submit(void) {
    // unlock the dsound buffer
    if (pDSBuf && dma.buffer && dma.buffer != dsScratch) {
        IDirectSoundBuffer_Unlock(pDSBuf, dma.buffer, locksize, NULL, 0);
    }
}

/*
==============
SNDDMA_Mute

[ioquakelive] The buffer's volume, at its minimum or back to full: DirectSound applies it to
what is playing now, not to what comes next
===============
*/
void SNDDMA_Mute(qboolean mute) {
    if (!pDSBuf || mute == dsMuted) {
        return;
    }
    if (SUCCEEDED(IDirectSoundBuffer_SetVolume(pDSBuf, mute ? DSBVOLUME_MIN : DSBVOLUME_MAX))) {
        dsMuted = mute;
    }
}

#endif  // _WIN32
