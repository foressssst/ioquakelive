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

#ifdef USE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define CIN_MPEG_MAX_W 1920
#define CIN_MPEG_MAX_H 1080
#define CIN_MPEG_LEAD 0.02
#define CIN_MPEG_PACKETS 512
#define CIN_MPEG_PCM 4096
#define CIN_MPEG_STEPS 2048
#define CIN_MPEG_HIST 32768
#define CIN_MPEG_THREADS 2

typedef struct {
    AVFormatContext* fmt;
    AVCodecContext* video;
    AVCodecContext* audio;
    AVPacket* packet;
    AVPacket* ring[CIN_MPEG_PACKETS];
    AVFrame* sound;
    AVFrame* next;
    AVFrame* shown;
    int head;
    int count;
    int videoStream;
    int audioStream;
    double videoBase;
    double audioBase;
    double time;
    double ahead;
    double lead;
    double skip;
    qboolean hasNext;
    qboolean eof;
    qboolean flushed;
    qboolean done;
    qboolean loop;
    int kr, kgu, kgv, kb, ky, yo;
    byte* rgba;
    int width;
    int height;
    short pcm[CIN_MPEG_PCM * 2];
    short* hist;
    int histHead;
    int histCount;
    int histRate;
} cinMpeg_t;

static AVCodecContext* CIN_MpegCodec(const AVStream* st, int threads) {
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    AVCodecContext* c;

    if (!codec) {
        return NULL;
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        return NULL;
    }
    if (avcodec_parameters_to_context(c, st->codecpar) < 0) {
        avcodec_free_context(&c);
        return NULL;
    }
    c->thread_count = threads;
    if (threads > 1) {
        c->thread_type = FF_THREAD_FRAME;
    }
    if (avcodec_open2(c, codec, NULL) < 0) {
        avcodec_free_context(&c);
        return NULL;
    }
    return c;
}

static void CIN_MpegMatrix(cinMpeg_t* m) {
    qboolean wide = m->video->colorspace == AVCOL_SPC_BT709 ||
                    (m->video->colorspace == AVCOL_SPC_UNSPECIFIED && m->height >= 720);
    qboolean full = m->video->color_range == AVCOL_RANGE_JPEG;
    double r = wide ? 1.5748 : 1.402, gu = wide ? 0.1873 : 0.344136, gv = wide ? 0.4681 : 0.714136, b = wide ? 1.8556 : 1.772;
    double c = full ? 1.0 : 255.0 / 224.0;

    m->ky = (int)((full ? 1.0 : 255.0 / 219.0) * 65536.0 + 0.5);
    m->yo = full ? 0 : 16;
    m->kr = (int)(r * c * 65536.0 + 0.5);
    m->kgu = (int)(gu * c * 65536.0 + 0.5);
    m->kgv = (int)(gv * c * 65536.0 + 0.5);
    m->kb = (int)(b * c * 65536.0 + 0.5);
}

static byte CIN_MpegClamp(int v) {
    v >>= 16;
    return (byte)(v < 0 ? 0 : v > 255 ? 255 : v);
}

static void CIN_MpegConvert(cinMpeg_t* m, const AVFrame* f) {
    int x, y;

    if ((f->format != AV_PIX_FMT_YUV420P && f->format != AV_PIX_FMT_YUVJ420P) || f->width != m->width || f->height != m->height) {
        return;
    }
    for (y = 0; y < m->height; y++) {
        const byte* py = f->data[0] + y * f->linesize[0];
        const byte* pu = f->data[1] + (y >> 1) * f->linesize[1];
        const byte* pv = f->data[2] + (y >> 1) * f->linesize[2];
        byte* out = m->rgba + y * m->width * 4;

        for (x = 0; x < m->width; x++) {
            int l = (py[x] - m->yo) * m->ky + 32768;
            int u = pu[x >> 1] - 128;
            int v = pv[x >> 1] - 128;

            out[0] = CIN_MpegClamp(l + m->kr * v);
            out[1] = CIN_MpegClamp(l - m->kgu * u - m->kgv * v);
            out[2] = CIN_MpegClamp(l + m->kb * u);
            out += 4;
        }
    }
}

static float CIN_MpegSample(const AVFrame* f, int channel, int index) {
    int channels = f->ch_layout.nb_channels;

    switch (f->format) {
        case AV_SAMPLE_FMT_FLTP:
            return ((const float*)f->extended_data[channel])[index];
        case AV_SAMPLE_FMT_FLT:
            return ((const float*)f->extended_data[0])[index * channels + channel];
        case AV_SAMPLE_FMT_S16P:
            return ((const short*)f->extended_data[channel])[index] / 32768.0f;
        case AV_SAMPLE_FMT_S16:
            return ((const short*)f->extended_data[0])[index * channels + channel] / 32768.0f;
        default:
            return 0.0f;
    }
}

static short CIN_MpegPcm(float v) {
    v *= 32767.0f;
    return (short)(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
}

static void CIN_MpegKeep(cinMpeg_t* m, int rate, int count) {
    int i;

    if (rate != m->histRate) {
        m->histRate = rate;
        m->histHead = m->histCount = 0;
    }
    for (i = 0; i < count; i++) {
        m->hist[m->histHead * 2] = m->pcm[i * 2];
        m->hist[m->histHead * 2 + 1] = m->pcm[i * 2 + 1];
        m->histHead = (m->histHead + 1) % CIN_MPEG_HIST;
    }
    m->histCount = m->histCount + count > CIN_MPEG_HIST ? CIN_MPEG_HIST : m->histCount + count;
}

static void CIN_MpegSound(cinMpeg_t* m, const AVFrame* f) {
    int rate = f->sample_rate, total = f->nb_samples, channels = f->ch_layout.nb_channels, i, k;
    double start = f->best_effort_timestamp == AV_NOPTS_VALUE ? m->ahead : f->best_effort_timestamp * m->audioBase;
    double end;

    if (rate <= 0 || channels <= 0 || total <= 0) {
        return;
    }
    end = start + (double)total / rate;
    m->ahead = end;
    if (end <= m->skip) {
        return;
    }
    i = start < m->skip ? (int)((m->skip - start) * rate) : 0;
    while (i < total) {
        int count = total - i < CIN_MPEG_PCM ? total - i : CIN_MPEG_PCM;

        for (k = 0; k < count; k++) {
            float left = CIN_MpegSample(f, 0, i + k);

            m->pcm[k * 2] = CIN_MpegPcm(left);
            m->pcm[k * 2 + 1] = CIN_MpegPcm(channels > 1 ? CIN_MpegSample(f, 1, i + k) : left);
        }
        S_RawSamples(0, count, rate, 2, 2, (const byte*)m->pcm, 1.0f, -1);
        CIN_MpegKeep(m, rate, count);
        i += count;
    }
}

static void CIN_MpegAudio(cinMpeg_t* m, const AVPacket* p) {
    if (avcodec_send_packet(m->audio, p) < 0) {
        return;
    }
    while (avcodec_receive_frame(m->audio, m->sound) >= 0) {
        CIN_MpegSound(m, m->sound);
        av_frame_unref(m->sound);
    }
}

static qboolean CIN_MpegRead(cinMpeg_t* m) {
    if (m->eof || m->count >= CIN_MPEG_PACKETS) {
        return qfalse;
    }
    if (av_read_frame(m->fmt, m->packet) < 0) {
        m->eof = qtrue;
        if (m->audio) {
            CIN_MpegAudio(m, NULL);
        }
        return qtrue;
    }
    if (m->packet->stream_index == m->videoStream) {
        av_packet_move_ref(m->ring[(m->head + m->count) % CIN_MPEG_PACKETS], m->packet);
        m->count++;
        return qtrue;
    }
    if (m->audio && m->packet->stream_index == m->audioStream) {
        CIN_MpegAudio(m, m->packet);
    }
    av_packet_unref(m->packet);
    return qtrue;
}

static double CIN_MpegFrameTime(const cinMpeg_t* m, const AVFrame* f) {
    return f->best_effort_timestamp == AV_NOPTS_VALUE ? m->time : f->best_effort_timestamp * m->videoBase;
}

static qboolean CIN_MpegVideo(cinMpeg_t* m, qboolean* popped) {
    for (;;) {
        int r;

        if (m->hasNext) {
            if (CIN_MpegFrameTime(m, m->next) > m->time) {
                return qfalse;
            }
            av_frame_unref(m->shown);
            av_frame_move_ref(m->shown, m->next);
            m->hasNext = qfalse;
            *popped = qtrue;
            continue;
        }
        if (m->done) {
            return qfalse;
        }
        r = avcodec_receive_frame(m->video, m->next);
        if (r >= 0) {
            m->hasNext = qtrue;
            continue;
        }
        if (r == AVERROR_EOF) {
            m->done = qtrue;
            return qfalse;
        }
        if (r != AVERROR(EAGAIN)) {
            m->done = qtrue;
            return qfalse;
        }
        if (m->count > 0) {
            AVPacket* p = m->ring[m->head];

            avcodec_send_packet(m->video, p);
            av_packet_unref(p);
            m->head = (m->head + 1) % CIN_MPEG_PACKETS;
            m->count--;
            continue;
        }
        if (m->eof && !m->flushed) {
            avcodec_send_packet(m->video, NULL);
            m->flushed = qtrue;
            continue;
        }
        return qtrue;
    }
}

static void CIN_MpegSeek(cinMpeg_t* m, double t) {
    int i;

    av_seek_frame(m->fmt, m->videoStream, (int64_t)(t / m->videoBase), AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(m->video);
    if (m->audio) {
        avcodec_flush_buffers(m->audio);
    }
    for (i = 0; i < CIN_MPEG_PACKETS; i++) {
        av_packet_unref(m->ring[i]);
    }
    av_frame_unref(m->next);
    m->head = m->count = 0;
    m->histHead = m->histCount = 0;
    m->hasNext = m->eof = m->flushed = m->done = qfalse;
    m->time = t;
    m->ahead = t;
    m->skip = t;
}

void* CIN_MpegOpen(const char* path, qboolean silent, qboolean loop, int* width, int* height) {
    AVFormatContext* fmt = NULL;
    cinMpeg_t* m;
    qboolean ok;
    int v, a, i;

    av_log_set_level(AV_LOG_QUIET);
    if (avformat_open_input(&fmt, path, NULL, NULL) < 0) {
        return NULL;
    }
    if (avformat_find_stream_info(fmt, NULL) < 0 || (v = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
        avformat_close_input(&fmt);
        return NULL;
    }
    a = silent ? -1 : av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, v, NULL, 0);
    m = (cinMpeg_t*)Z_Malloc(sizeof(*m));
    m->fmt = fmt;
    m->videoStream = v;
    m->audioStream = a;
    m->video = CIN_MpegCodec(fmt->streams[v], CIN_MPEG_THREADS);
    m->audio = a >= 0 ? CIN_MpegCodec(fmt->streams[a], 1) : NULL;
    m->hist = m->audio ? (short*)Z_Malloc(CIN_MPEG_HIST * 2 * sizeof(short)) : NULL;
    m->packet = av_packet_alloc();
    m->sound = av_frame_alloc();
    m->next = av_frame_alloc();
    m->shown = av_frame_alloc();
    ok = m->video && m->packet && m->sound && m->next && m->shown;
    for (i = 0; i < CIN_MPEG_PACKETS; i++) {
        m->ring[i] = av_packet_alloc();
        ok = ok && m->ring[i];
    }
    if (m->video) {
        m->width = m->video->width;
        m->height = m->video->height;
    }
    if (!ok || m->width <= 0 || m->height <= 0 || m->width > CIN_MPEG_MAX_W || m->height > CIN_MPEG_MAX_H || (m->width & 1) || (m->height & 1)) {
        CIN_MpegClose(m);
        return NULL;
    }
    m->videoBase = av_q2d(fmt->streams[v]->time_base);
    m->audioBase = m->audio ? av_q2d(fmt->streams[a]->time_base) : 0.0;
    m->lead = Cvar_VariableValue("s_mixahead") + CIN_MPEG_LEAD;
    m->loop = loop;
    CIN_MpegMatrix(m);
    m->rgba = (byte*)Z_Malloc(m->width * m->height * 4);
    for (i = 3; i < m->width * m->height * 4; i += 4) {
        m->rgba[i] = 255;
    }
    *width = m->width;
    *height = m->height;
    return m;
}

byte* CIN_MpegPixels(void* video) {
    return ((cinMpeg_t*)video)->rgba;
}

qboolean CIN_MpegRun(void* video, int msec, qboolean* fresh) {
    cinMpeg_t* m = (cinMpeg_t*)video;
    qboolean popped = qfalse;
    int steps;

    m->time += msec * 0.001;
    for (steps = 0; steps < CIN_MPEG_STEPS; steps++) {
        qboolean starved = CIN_MpegVideo(m, &popped);
        qboolean heard = !m->audio || m->eof || m->ahead >= m->time + m->lead;

        if ((!starved && heard) || !CIN_MpegRead(m)) {
            break;
        }
    }
    if (popped) {
        CIN_MpegConvert(m, m->shown);
    }
    *fresh = popped;
    if (m->done && !m->hasNext) {
        if (!m->loop) {
            return qtrue;
        }
        CIN_MpegSeek(m, 0.0);
    }
    return qfalse;
}

void CIN_MpegResync(void* video) {
    cinMpeg_t* m = (cinMpeg_t*)video;
    int count, start;

    if (!m->hist || m->histRate <= 0) {
        return;
    }
    count = (int)((m->ahead - m->time) * m->histRate);
    if (count > m->histCount) {
        count = m->histCount;
    }
    start = (m->histHead - (count > 0 ? count : 0) + CIN_MPEG_HIST) % CIN_MPEG_HIST;
    while (count > 0) {
        int n = CIN_MPEG_HIST - start < count ? CIN_MPEG_HIST - start : count;

        S_RawSamples(0, n, m->histRate, 2, 2, (const byte*)(m->hist + start * 2), 1.0f, -1);
        start = (start + n) % CIN_MPEG_HIST;
        count -= n;
    }
}

void CIN_MpegClose(void* video) {
    cinMpeg_t* m = (cinMpeg_t*)video;
    int i;

    avcodec_free_context(&m->video);
    avcodec_free_context(&m->audio);
    avformat_close_input(&m->fmt);
    av_packet_free(&m->packet);
    av_frame_free(&m->sound);
    av_frame_free(&m->next);
    av_frame_free(&m->shown);
    for (i = 0; i < CIN_MPEG_PACKETS; i++) {
        av_packet_free(&m->ring[i]);
    }
    if (m->rgba) {
        Z_Free(m->rgba);
    }
    if (m->hist) {
        Z_Free(m->hist);
    }
    Z_Free(m);
}

#else

void* CIN_MpegOpen(const char* path, qboolean silent, qboolean loop, int* width, int* height) {
    (void)path;
    (void)silent;
    (void)loop;
    (void)width;
    (void)height;
    return NULL;
}

byte* CIN_MpegPixels(void* video) {
    (void)video;
    return NULL;
}

qboolean CIN_MpegRun(void* video, int msec, qboolean* fresh) {
    (void)video;
    (void)msec;
    *fresh = qfalse;
    return qtrue;
}

void CIN_MpegResync(void* video) {
    (void)video;
}

void CIN_MpegClose(void* video) {
    (void)video;
}

#endif
