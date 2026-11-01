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
#include "../qcommon/qcommon.h"

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <codecapi.h>

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#endif

#define VIDEO_TICKS 10000000LL
#define VIDEO_QUALITY 80
#define VIDEO_PIXELS_PER_BIT 12
#define VIDEO_AAC_BYTES 24000

#define LUMA_R 11966
#define LUMA_G 40254
#define LUMA_B 4064
#define CB_R -6596
#define CB_G -22188
#define CB_B 28784
#define CR_R 28784
#define CR_G -26145
#define CR_B -2639

typedef HRESULT(STDAPICALLTYPE* mfStartup_t)(ULONG version, DWORD flags);
typedef HRESULT(STDAPICALLTYPE* mfShutdown_t)(void);
typedef HRESULT(STDAPICALLTYPE* mfCreateAttributes_t)(IMFAttributes** attributes, UINT32 size);
typedef HRESULT(STDAPICALLTYPE* mfCreateMediaType_t)(IMFMediaType** type);
typedef HRESULT(STDAPICALLTYPE* mfCreateMemoryBuffer_t)(DWORD size, IMFMediaBuffer** buffer);
typedef HRESULT(STDAPICALLTYPE* mfCreateSample_t)(IMFSample** sample);
typedef HRESULT(STDAPICALLTYPE* mfCreateSinkWriterFromURL_t)(LPCWSTR url, IMFByteStream* stream,
                                                            IMFAttributes* attributes, IMFSinkWriter** writer);

typedef struct {
    IMFSinkWriter* writer;
    DWORD videoStream;
    DWORD audioStream;
    int width;
    int height;
    int fps;
    int rate;
    LONGLONG frames;
    LONGLONG samples;
    HRESULT error;
    qboolean com;
    WCHAR path[MAX_OSPATH];
} sysVideo_t;

static const GUID videoRateControl = {STATIC_CODECAPI_AVEncCommonRateControlMode};
static const GUID videoQuality = {STATIC_CODECAPI_AVEncCommonQuality};

static HMODULE mfPlat;
static HMODULE mfReadWrite;
static mfStartup_t pMFStartup;
static mfShutdown_t pMFShutdown;
static mfCreateAttributes_t pMFCreateAttributes;
static mfCreateMediaType_t pMFCreateMediaType;
static mfCreateMemoryBuffer_t pMFCreateMemoryBuffer;
static mfCreateSample_t pMFCreateSample;
static mfCreateSinkWriterFromURL_t pMFCreateSinkWriterFromURL;

static qboolean Sys_VideoLoad(void) {
    if (pMFCreateSinkWriterFromURL) {
        return qtrue;
    }
    if (!mfPlat) {
        mfPlat = LoadLibraryExW(L"mfplat.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (!mfReadWrite) {
        mfReadWrite = LoadLibraryExW(L"mfreadwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (!mfPlat || !mfReadWrite) {
        return qfalse;
    }
    pMFStartup = (mfStartup_t)GetProcAddress(mfPlat, "MFStartup");
    pMFShutdown = (mfShutdown_t)GetProcAddress(mfPlat, "MFShutdown");
    pMFCreateAttributes = (mfCreateAttributes_t)GetProcAddress(mfPlat, "MFCreateAttributes");
    pMFCreateMediaType = (mfCreateMediaType_t)GetProcAddress(mfPlat, "MFCreateMediaType");
    pMFCreateMemoryBuffer = (mfCreateMemoryBuffer_t)GetProcAddress(mfPlat, "MFCreateMemoryBuffer");
    pMFCreateSample = (mfCreateSample_t)GetProcAddress(mfPlat, "MFCreateSample");
    if (!pMFStartup || !pMFShutdown || !pMFCreateAttributes || !pMFCreateMediaType || !pMFCreateMemoryBuffer ||
        !pMFCreateSample) {
        return qfalse;
    }
    pMFCreateSinkWriterFromURL =
        (mfCreateSinkWriterFromURL_t)GetProcAddress(mfReadWrite, "MFCreateSinkWriterFromURL");
    return (qboolean)(pMFCreateSinkWriterFromURL != NULL);
}

static LONGLONG Sys_VideoTicks(LONGLONG count, int rate) {
    return (count * VIDEO_TICKS + rate / 2) / rate;
}

static HRESULT Sys_VideoMediaType(IMFMediaType** type, REFGUID major, REFGUID subtype) {
    HRESULT hr = pMFCreateMediaType(type);

    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetGUID(*type, &MF_MT_MAJOR_TYPE, major);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, subtype);
    }
    return hr;
}

static HRESULT Sys_VideoPicture(IMFMediaType* type, const sysVideo_t* v) {
    HRESULT hr = IMFMediaType_SetUINT32(type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, ((UINT64)v->width << 32) | (UINT32)v->height);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT64(type, &MF_MT_FRAME_RATE, ((UINT64)v->fps << 32) | 1);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT64(type, &MF_MT_PIXEL_ASPECT_RATIO, ((UINT64)1 << 32) | 1);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
    }
    return hr;
}

static HRESULT Sys_VideoSound(IMFMediaType* type, const sysVideo_t* v) {
    HRESULT hr = IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, v->rate);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    }
    return hr;
}

static void Sys_VideoRelease(IMFMediaType** type) {
    if (*type) {
        IMFMediaType_Release(*type);
        *type = NULL;
    }
}

static HRESULT Sys_VideoInput(sysVideo_t* v, IMFMediaType* type) {
    IMFAttributes* params = NULL;
    HRESULT hr = pMFCreateAttributes(&params, 2);

    if (SUCCEEDED(hr)) {
        hr = IMFAttributes_SetUINT32(params, &videoRateControl, eAVEncCommonRateControlMode_Quality);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFAttributes_SetUINT32(params, &videoQuality, VIDEO_QUALITY);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSinkWriter_SetInputMediaType(v->writer, v->videoStream, type, params);
    }
    if (params) {
        IMFAttributes_Release(params);
    }
    if (FAILED(hr)) {
        hr = IMFSinkWriter_SetInputMediaType(v->writer, v->videoStream, type, NULL);
    }
    return hr;
}

static HRESULT Sys_VideoStreams(sysVideo_t* v) {
    IMFMediaType* type = NULL;
    UINT64 bitrate = (UINT64)v->width * v->height * v->fps / VIDEO_PIXELS_PER_BIT;
    HRESULT hr = Sys_VideoMediaType(&type, &MFMediaType_Video, &MFVideoFormat_H264);

    if (SUCCEEDED(hr)) {
        hr = Sys_VideoPicture(type, v);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_AVG_BITRATE, (UINT32)MIN(bitrate, 0xffffffffu));
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSinkWriter_AddStream(v->writer, type, &v->videoStream);
    }
    Sys_VideoRelease(&type);
    if (SUCCEEDED(hr)) {
        hr = Sys_VideoMediaType(&type, &MFMediaType_Video, &MFVideoFormat_NV12);
    }
    if (SUCCEEDED(hr)) {
        hr = Sys_VideoPicture(type, v);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFMediaType_SetUINT32(type, &MF_MT_DEFAULT_STRIDE, v->width);
    }
    if (SUCCEEDED(hr)) {
        hr = Sys_VideoInput(v, type);
    }
    Sys_VideoRelease(&type);
    if (SUCCEEDED(hr) && v->rate) {
        hr = Sys_VideoMediaType(&type, &MFMediaType_Audio, &MFAudioFormat_AAC);
        if (SUCCEEDED(hr)) {
            hr = Sys_VideoSound(type, v);
        }
        if (SUCCEEDED(hr)) {
            hr = IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, VIDEO_AAC_BYTES);
        }
        if (SUCCEEDED(hr)) {
            hr = IMFSinkWriter_AddStream(v->writer, type, &v->audioStream);
        }
        Sys_VideoRelease(&type);
        if (SUCCEEDED(hr)) {
            hr = Sys_VideoMediaType(&type, &MFMediaType_Audio, &MFAudioFormat_PCM);
        }
        if (SUCCEEDED(hr)) {
            hr = Sys_VideoSound(type, v);
        }
        if (SUCCEEDED(hr)) {
            hr = IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
        }
        if (SUCCEEDED(hr)) {
            hr = IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, v->rate * 4);
        }
        if (SUCCEEDED(hr)) {
            hr = IMFSinkWriter_SetInputMediaType(v->writer, v->audioStream, type, NULL);
        }
        Sys_VideoRelease(&type);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSinkWriter_BeginWriting(v->writer);
    }
    return hr;
}

static HRESULT Sys_VideoCreate(sysVideo_t* v, BOOL hardware) {
    IMFAttributes* attributes = NULL;
    HRESULT hr = pMFCreateAttributes(&attributes, 1);

    if (SUCCEEDED(hr)) {
        hr = IMFAttributes_SetUINT32(attributes, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, hardware);
    }
    if (SUCCEEDED(hr)) {
        hr = pMFCreateSinkWriterFromURL(v->path, NULL, attributes, &v->writer);
    }
    if (attributes) {
        IMFAttributes_Release(attributes);
    }
    if (SUCCEEDED(hr)) {
        hr = Sys_VideoStreams(v);
    }
    if (FAILED(hr)) {
        if (v->writer) {
            IMFSinkWriter_Release(v->writer);
            v->writer = NULL;
        }
        DeleteFileW(v->path);
    }
    return hr;
}

void* Sys_VideoOpen(const char* path, int width, int height, int fps, int rate) {
    sysVideo_t* v;
    HRESULT hr;

    if (width < 2 || height < 2 || fps <= 0 || !Sys_VideoLoad()) {
        return NULL;
    }
    v = Z_Malloc(sizeof(*v));
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, v->path, ARRAY_LEN(v->path))) {
        Z_Free(v);
        return NULL;
    }
    v->width = width & ~1;
    v->height = height & ~1;
    v->fps = fps;
    v->rate = rate;
    v->com = (qboolean)SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    hr = pMFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (SUCCEEDED(hr)) {
        if (FAILED(Sys_VideoCreate(v, TRUE))) {
            hr = Sys_VideoCreate(v, FALSE);
        }
        if (SUCCEEDED(hr)) {
            return v;
        }
        pMFShutdown();
    }
    if (v->com) {
        CoUninitialize();
    }
    Z_Free(v);
    return NULL;
}

static void Sys_VideoNv12(byte* out, const byte* bgr, int stride, int rows, int width, int height) {
    byte* chroma = out + width * height;
    int x, y;

    for (y = 0; y < height; y += 2) {
        const byte* a = bgr + (rows - 1 - y) * stride;
        const byte* b = a - stride;
        byte* ya = out + y * width;
        byte* yb = ya + width;
        byte* c = chroma + (y >> 1) * width;

        for (x = 0; x < width; x += 2, a += 6, b += 6, ya += 2, yb += 2, c += 2) {
            int sb = a[0] + a[3] + b[0] + b[3];
            int sg = a[1] + a[4] + b[1] + b[4];
            int sr = a[2] + a[5] + b[2] + b[5];

            ya[0] = (byte)((LUMA_R * a[2] + LUMA_G * a[1] + LUMA_B * a[0] + (16 << 16) + (1 << 15)) >> 16);
            ya[1] = (byte)((LUMA_R * a[5] + LUMA_G * a[4] + LUMA_B * a[3] + (16 << 16) + (1 << 15)) >> 16);
            yb[0] = (byte)((LUMA_R * b[2] + LUMA_G * b[1] + LUMA_B * b[0] + (16 << 16) + (1 << 15)) >> 16);
            yb[1] = (byte)((LUMA_R * b[5] + LUMA_G * b[4] + LUMA_B * b[3] + (16 << 16) + (1 << 15)) >> 16);
            c[0] = (byte)((CB_R * sr + CB_G * sg + CB_B * sb + (128 << 18) + (1 << 17)) >> 18);
            c[1] = (byte)((CR_R * sr + CR_G * sg + CR_B * sb + (128 << 18) + (1 << 17)) >> 18);
        }
    }
}

static HRESULT Sys_VideoSend(sysVideo_t* v, DWORD stream, IMFMediaBuffer* buffer, LONGLONG start, LONGLONG end) {
    IMFSample* sample = NULL;
    HRESULT hr = pMFCreateSample(&sample);

    if (SUCCEEDED(hr)) {
        hr = IMFSample_AddBuffer(sample, buffer);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSample_SetSampleTime(sample, start);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSample_SetSampleDuration(sample, end - start);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSinkWriter_WriteSample(v->writer, stream, sample);
    }
    if (sample) {
        IMFSample_Release(sample);
    }
    return hr;
}

qboolean Sys_VideoFrame(void* video, const byte* bgr, int stride, int rows) {
    sysVideo_t* v = video;
    IMFMediaBuffer* buffer = NULL;
    DWORD size = (DWORD)(v->width * v->height * 3 / 2);
    BYTE* data = NULL;
    HRESULT hr;

    if (FAILED(v->error)) {
        return qfalse;
    }
    if (rows < v->height || stride < v->width * 3) {
        v->error = E_INVALIDARG;
        return qfalse;
    }
    hr = pMFCreateMemoryBuffer(size, &buffer);
    if (SUCCEEDED(hr)) {
        hr = IMFMediaBuffer_Lock(buffer, &data, NULL, NULL);
    }
    if (SUCCEEDED(hr)) {
        Sys_VideoNv12(data, bgr, stride, rows, v->width, v->height);
        IMFMediaBuffer_Unlock(buffer);
        hr = IMFMediaBuffer_SetCurrentLength(buffer, size);
    }
    if (SUCCEEDED(hr)) {
        hr = Sys_VideoSend(v, v->videoStream, buffer, Sys_VideoTicks(v->frames, v->fps),
                           Sys_VideoTicks(v->frames + 1, v->fps));
    }
    if (buffer) {
        IMFMediaBuffer_Release(buffer);
    }
    if (FAILED(hr)) {
        v->error = hr;
        return qfalse;
    }
    v->frames++;
    return qtrue;
}

qboolean Sys_VideoAudio(void* video, const short* pcm, int count) {
    sysVideo_t* v = video;
    IMFMediaBuffer* buffer = NULL;
    DWORD size = (DWORD)(count * 4);
    BYTE* data = NULL;
    HRESULT hr;

    if (FAILED(v->error)) {
        return qfalse;
    }
    if (!v->rate || count <= 0) {
        return qtrue;
    }
    hr = pMFCreateMemoryBuffer(size, &buffer);
    if (SUCCEEDED(hr)) {
        hr = IMFMediaBuffer_Lock(buffer, &data, NULL, NULL);
    }
    if (SUCCEEDED(hr)) {
        Com_Memcpy(data, pcm, size);
        IMFMediaBuffer_Unlock(buffer);
        hr = IMFMediaBuffer_SetCurrentLength(buffer, size);
    }
    if (SUCCEEDED(hr)) {
        hr = Sys_VideoSend(v, v->audioStream, buffer, Sys_VideoTicks(v->samples, v->rate),
                           Sys_VideoTicks(v->samples + count, v->rate));
    }
    if (buffer) {
        IMFMediaBuffer_Release(buffer);
    }
    if (FAILED(hr)) {
        v->error = hr;
        return qfalse;
    }
    v->samples += count;
    return qtrue;
}

static unsigned int Sys_VideoGet32(const byte* p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
}

static void Sys_VideoPut32(byte* p, unsigned int x) {
    p[0] = (byte)(x >> 24);
    p[1] = (byte)(x >> 16);
    p[2] = (byte)(x >> 8);
    p[3] = (byte)x;
}

static qboolean Sys_VideoShiftChunks(byte* box, unsigned int size, unsigned int shift) {
    unsigned int at = 8;

    while (at + 8 <= size) {
        byte* child = box + at;
        unsigned int len = Sys_VideoGet32(child);
        unsigned int n, i;

        if (len < 8 || len > size - at) {
            return qfalse;
        }
        if (!memcmp(child + 4, "trak", 4) || !memcmp(child + 4, "mdia", 4) || !memcmp(child + 4, "minf", 4) ||
            !memcmp(child + 4, "stbl", 4)) {
            if (!Sys_VideoShiftChunks(child, len, shift)) {
                return qfalse;
            }
        } else if (!memcmp(child + 4, "stco", 4)) {
            n = len >= 16 ? Sys_VideoGet32(child + 12) : 0;
            if (len < 16 || n > (len - 16) / 4) {
                return qfalse;
            }
            for (i = 0; i < n; i++) {
                UINT64 offset = (UINT64)Sys_VideoGet32(child + 16 + i * 4) + shift;

                if (offset > 0xffffffffu) {
                    return qfalse;
                }
                Sys_VideoPut32(child + 16 + i * 4, (unsigned int)offset);
            }
        } else if (!memcmp(child + 4, "co64", 4)) {
            n = len >= 16 ? Sys_VideoGet32(child + 12) : 0;
            if (len < 16 || n > (len - 16) / 8) {
                return qfalse;
            }
            for (i = 0; i < n; i++) {
                byte* e = child + 16 + i * 8;
                UINT64 offset = (((UINT64)Sys_VideoGet32(e) << 32) | Sys_VideoGet32(e + 4)) + shift;

                Sys_VideoPut32(e, (unsigned int)(offset >> 32));
                Sys_VideoPut32(e + 4, (unsigned int)offset);
            }
        }
        at += len;
    }
    return qtrue;
}

static qboolean Sys_VideoCopy(FILE* in, FILE* out, __int64 from, __int64 to, byte* buffer, int size) {
    if (_fseeki64(in, from, SEEK_SET)) {
        return qfalse;
    }
    while (from < to) {
        int n = (int)MIN(to - from, (__int64)size);

        if ((int)fread(buffer, 1, n, in) != n || (int)fwrite(buffer, 1, n, out) != n) {
            return qfalse;
        }
        from += n;
    }
    return qtrue;
}

static void Sys_VideoFastStart(const WCHAR* path) {
    WCHAR temp[MAX_OSPATH + 8];
    __int64 total, at = 0, moovAt = -1, mdatAt = -1;
    unsigned int moovSize = 0;
    byte head[16];
    byte* moov = NULL;
    byte* buffer = NULL;
    FILE* in;
    FILE* out = NULL;
    qboolean ok = qfalse;

    in = _wfopen(path, L"rb");
    if (!in) {
        return;
    }
    _fseeki64(in, 0, SEEK_END);
    total = _ftelli64(in);
    while (at + 8 <= total) {
        __int64 len;

        if (_fseeki64(in, at, SEEK_SET) || fread(head, 1, 8, in) != 8) {
            break;
        }
        len = Sys_VideoGet32(head);
        if (len == 1) {
            if (fread(head + 8, 1, 8, in) != 8) {
                break;
            }
            len = ((__int64)Sys_VideoGet32(head + 8) << 32) | Sys_VideoGet32(head + 12);
        } else if (len == 0) {
            len = total - at;
        }
        if (len < 8 || len > total - at) {
            break;
        }
        if (!memcmp(head + 4, "moov", 4) && Sys_VideoGet32(head) > 1) {
            moovAt = at;
            moovSize = Sys_VideoGet32(head);
        } else if (!memcmp(head + 4, "mdat", 4) && mdatAt < 0) {
            mdatAt = at;
        }
        at += len;
    }
    if (at != total || moovAt < 0 || mdatAt < 0 || moovAt < mdatAt || moovSize > 0x4000000) {
        fclose(in);
        return;
    }
    moov = malloc(moovSize);
    buffer = malloc(0x100000);
    _snwprintf(temp, ARRAY_LEN(temp), L"%ls.tmp", path);
    temp[ARRAY_LEN(temp) - 1] = 0;
    if (moov && buffer && !_fseeki64(in, moovAt, SEEK_SET) && fread(moov, 1, moovSize, in) == moovSize &&
        Sys_VideoShiftChunks(moov, moovSize, moovSize) && (out = _wfopen(temp, L"wb")) != NULL) {
        ok = (qboolean)(Sys_VideoCopy(in, out, 0, mdatAt, buffer, 0x100000) &&
                        fwrite(moov, 1, moovSize, out) == moovSize &&
                        Sys_VideoCopy(in, out, mdatAt, moovAt, buffer, 0x100000) &&
                        Sys_VideoCopy(in, out, moovAt + moovSize, total, buffer, 0x100000));
        ok = (qboolean)(!fclose(out) && ok);
    }
    fclose(in);
    free(buffer);
    free(moov);
    if (out && (!ok || !MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING))) {
        DeleteFileW(temp);
    }
}

int Sys_VideoClose(void* video) {
    sysVideo_t* v = video;
    HRESULT hr = v->frames ? IMFSinkWriter_Finalize(v->writer) : E_ABORT;

    IMFSinkWriter_Release(v->writer);
    if (SUCCEEDED(hr) && SUCCEEDED(v->error)) {
        Sys_VideoFastStart(v->path);
    }
    pMFShutdown();
    if (v->com) {
        CoUninitialize();
    }
    if (FAILED(v->error)) {
        hr = v->error;
    }
    if (!v->frames) {
        DeleteFileW(v->path);
    }
    Z_Free(v);
    return FAILED(hr) ? (int)hr : 0;
}

#endif
