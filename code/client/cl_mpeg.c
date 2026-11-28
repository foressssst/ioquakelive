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

#define PL_MPEG_IMPLEMENTATION
#include "../pl_mpeg/pl_mpeg.h"

#define CIN_MPEG_MAX_W 1920
#define CIN_MPEG_MAX_H 1080
#define CIN_MPEG_LEAD 0.02

typedef struct {
    plm_t* plm;
    byte* rgba;
    int width;
    int height;
    qboolean fresh;
    short pcm[PLM_AUDIO_SAMPLES_PER_FRAME * 2];
} cinMpeg_t;

static void CIN_MpegVideo(plm_t* plm, plm_frame_t* frame, void* user) {
    cinMpeg_t* m = (cinMpeg_t*)user;

    (void)plm;
    if ((int)frame->width == m->width && (int)frame->height == m->height) {
        plm_frame_to_rgba(frame, m->rgba, m->width * 4);
        m->fresh = qtrue;
    }
}

static void CIN_MpegAudio(plm_t* plm, plm_samples_t* samples, void* user) {
    cinMpeg_t* m = (cinMpeg_t*)user;
    int count = (int)samples->count;
    int i;

    if (count > PLM_AUDIO_SAMPLES_PER_FRAME) {
        count = PLM_AUDIO_SAMPLES_PER_FRAME;
    }
    for (i = 0; i < count * 2; i++) {
        float v = samples->interleaved[i] * 32767.0f;

        m->pcm[i] = (short)(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
    }
    S_RawSamples(0, count, plm_get_samplerate(plm), 2, 2, (const byte*)m->pcm, 1.0f, -1);
}

void* CIN_MpegOpen(const char* path, qboolean silent, qboolean loop, int* width, int* height) {
    FILE* f = Sys_FOpen(path, "rb");
    cinMpeg_t* m;
    plm_t* plm;
    int w, h, i;

    if (!f) {
        return NULL;
    }
    plm = plm_create_with_file(f, 1);
    if (!plm) {
        fclose(f);
        return NULL;
    }
    w = plm_get_width(plm);
    h = plm_get_height(plm);
    if (!plm_has_headers(plm) || w <= 0 || h <= 0 || w > CIN_MPEG_MAX_W || h > CIN_MPEG_MAX_H || (w & 1) || (h & 1)) {
        plm_destroy(plm);
        return NULL;
    }
    m = (cinMpeg_t*)Z_Malloc(sizeof(*m));
    m->rgba = (byte*)Z_Malloc(w * h * 4);
    for (i = 3; i < w * h * 4; i += 4) {
        m->rgba[i] = 255;
    }
    m->plm = plm;
    m->width = w;
    m->height = h;
    plm_set_video_decode_callback(plm, CIN_MpegVideo, m);
    plm_set_audio_decode_callback(plm, CIN_MpegAudio, m);
    plm_set_audio_enabled(plm, !silent);
    plm_set_audio_lead_time(plm, Cvar_VariableValue("s_mixahead") + CIN_MPEG_LEAD);
    plm_set_loop(plm, loop);
    *width = w;
    *height = h;
    return m;
}

byte* CIN_MpegPixels(void* video) {
    return ((cinMpeg_t*)video)->rgba;
}

qboolean CIN_MpegRun(void* video, int msec, qboolean* fresh) {
    cinMpeg_t* m = (cinMpeg_t*)video;

    plm_decode(m->plm, msec * 0.001);
    *fresh = m->fresh;
    m->fresh = qfalse;
    return plm_has_ended(m->plm) ? qtrue : qfalse;
}

void CIN_MpegResync(void* video) {
    cinMpeg_t* m = (cinMpeg_t*)video;

    plm_seek(m->plm, plm_get_time(m->plm), 1);
}

void CIN_MpegClose(void* video) {
    cinMpeg_t* m = (cinMpeg_t*)video;

    plm_destroy(m->plm);
    Z_Free(m->rgba);
    Z_Free(m);
}
