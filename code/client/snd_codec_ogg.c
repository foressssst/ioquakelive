/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
Copyright (C) 2005-2006 Joerg Dietrich <dietrich_joerg@gmx.de>

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
#ifdef USE_CODEC_VORBIS

#include "client.h"
#include "snd_codec.h"

#include <errno.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#define OGG_SAMPLEWIDTH 2

snd_codec_t ogg_codec =
    {
        "ogg",
        S_OGG_CodecLoad,
        S_OGG_CodecOpenStream,
        S_OGG_CodecReadStream,
        S_OGG_CodecCloseStream,
        NULL};

size_t S_OGG_Callback_read(void* ptr, size_t size, size_t nmemb, void* datasource) {
    snd_stream_t* stream;
    int byteSize = 0;
    int bytesRead = 0;
    size_t nMembRead = 0;

    if (!ptr) {
        errno = EFAULT;
        return 0;
    }

    if (!(size && nmemb)) {
        errno = 0;
        return 0;
    }

    if (!datasource) {
        errno = EBADF;
        return 0;
    }

    stream = (snd_stream_t*)datasource;

    byteSize = nmemb * size;

    bytesRead = FS_Read(ptr, byteSize, stream->file);

    stream->pos += bytesRead;

    nMembRead = bytesRead / size;

    if (bytesRead % size) {
        nMembRead++;
    }

    return nMembRead;
}

int S_OGG_Callback_seek(void* datasource, ogg_int64_t offset, int whence) {
    snd_stream_t* stream;
    int retVal = 0;

    if (!datasource) {
        errno = EBADF;
        return -1;
    }

    stream = (snd_stream_t*)datasource;

    switch (whence) {
        case SEEK_SET: {
            retVal = FS_Seek(stream->file, (long)offset, FS_SEEK_SET);

            if (retVal < 0) {
                return retVal;
            }

            stream->pos = (int)offset;
            break;
        }

        case SEEK_CUR: {
            retVal = FS_Seek(stream->file, (long)offset, FS_SEEK_CUR);

            if (retVal < 0) {
                return retVal;
            }

            stream->pos += (int)offset;
            break;
        }

        case SEEK_END: {
            retVal = FS_Seek(stream->file, (long)offset, FS_SEEK_END);

            if (retVal < 0) {
                return retVal;
            }

            stream->pos = stream->length + (int)offset;
            break;
        }

        default: {
            errno = EINVAL;
            return -1;
        }
    }

    stream->pos = (stream->pos < 0) ? 0 : stream->pos;
    stream->pos = (stream->pos > stream->length) ? stream->length : stream->pos;

    return 0;
}

int S_OGG_Callback_close(void* datasource) {
    return 0;
}

long S_OGG_Callback_tell(void* datasource) {
    snd_stream_t* stream;

    if (!datasource) {
        errno = EBADF;
        return -1;
    }

    stream = (snd_stream_t*)datasource;

    return (long)FS_FTell(stream->file);
}

const ov_callbacks S_OGG_Callbacks =
    {
        &S_OGG_Callback_read,
        &S_OGG_Callback_seek,
        &S_OGG_Callback_close,
        &S_OGG_Callback_tell};

snd_stream_t* S_OGG_CodecOpenStream(const char* filename) {
    snd_stream_t* stream;

    OggVorbis_File* vf;

    vorbis_info* OGGInfo;
    ogg_int64_t numSamples;

    if (!filename) {
        return NULL;
    }

    stream = S_CodecUtilOpen(filename, &ogg_codec);
    if (!stream) {
        return NULL;
    }

    vf = Z_Malloc(sizeof(OggVorbis_File));
    if (!vf) {
        S_CodecUtilClose(&stream);

        return NULL;
    }

    if (ov_open_callbacks(stream, vf, NULL, 0, S_OGG_Callbacks) != 0) {
        Z_Free(vf);

        S_CodecUtilClose(&stream);

        return NULL;
    }

    if (!ov_seekable(vf)) {
        ov_clear(vf);

        Z_Free(vf);

        S_CodecUtilClose(&stream);

        return NULL;
    }

    if (ov_streams(vf) != 1) {
        ov_clear(vf);

        Z_Free(vf);

        S_CodecUtilClose(&stream);

        return NULL;
    }

    OGGInfo = ov_info(vf, 0);
    if (!OGGInfo) {
        ov_clear(vf);

        Z_Free(vf);

        S_CodecUtilClose(&stream);

        return NULL;
    }

    numSamples = ov_pcm_total(vf, 0);

    stream->info.rate = OGGInfo->rate;
    stream->info.width = OGG_SAMPLEWIDTH;
    stream->info.channels = OGGInfo->channels;
    stream->info.samples = numSamples;
    stream->info.size = stream->info.samples * stream->info.channels * stream->info.width;
    stream->info.dataofs = 0;

    stream->pos = 0;

    stream->ptr = vf;

    return stream;
}

void S_OGG_CodecCloseStream(snd_stream_t* stream) {
    if (!stream) {
        return;
    }

    ov_clear((OggVorbis_File*)stream->ptr);

    Z_Free(stream->ptr);

    S_CodecUtilClose(&stream);
}

int S_OGG_CodecReadStream(snd_stream_t* stream, int bytes, void* buffer) {
    int bytesRead, bytesLeft, c;
    char* bufPtr;

    int BS = 0;

    int IsBigEndian = 0;

#ifdef Q3_BIG_ENDIAN
    IsBigEndian = 1;
#endif

    if (!(stream && buffer)) {
        return 0;
    }

    if (bytes <= 0) {
        return 0;
    }

    bytesRead = 0;
    bytesLeft = bytes;
    bufPtr = buffer;

    while (-1) {
        c = ov_read((OggVorbis_File*)stream->ptr, bufPtr, bytesLeft, IsBigEndian, OGG_SAMPLEWIDTH, 1, &BS);

        if (c <= 0) {
            break;
        }

        bytesRead += c;
        bytesLeft -= c;
        bufPtr += c;

        if (bytesLeft <= 0) {
            break;
        }
    }

    return bytesRead;
}

void* S_OGG_CodecLoad(const char* filename, snd_info_t* info) {
    snd_stream_t* stream;
    byte* buffer;
    int bytesRead;

    if (!(filename && info)) {
        return NULL;
    }

    stream = S_OGG_CodecOpenStream(filename);
    if (!stream) {
        return NULL;
    }

    info->rate = stream->info.rate;
    info->width = stream->info.width;
    info->channels = stream->info.channels;
    info->samples = stream->info.samples;
    info->size = stream->info.size;
    info->dataofs = stream->info.dataofs;

    buffer = Hunk_AllocateTempMemory(info->size);
    if (!buffer) {
        S_OGG_CodecCloseStream(stream);

        return NULL;
    }

    bytesRead = S_OGG_CodecReadStream(stream, info->size, buffer);

    if (bytesRead <= 0) {
        Hunk_FreeTempMemory(buffer);
        S_OGG_CodecCloseStream(stream);

        return NULL;
    }

    S_OGG_CodecCloseStream(stream);

    return buffer;
}

typedef struct {
    const byte* data;
    size_t size;
    size_t pos;
} oggMemory_t;

static size_t S_OGG_MemRead(void* ptr, size_t size, size_t nmemb, void* source) {
    oggMemory_t* mem = (oggMemory_t*)source;
    size_t wanted = size * nmemb;

    if (!size || mem->pos >= mem->size) {
        return 0;
    }
    if (wanted > mem->size - mem->pos) {
        wanted = mem->size - mem->pos;
    }
    Com_Memcpy(ptr, mem->data + mem->pos, wanted);
    mem->pos += wanted;
    return wanted / size;
}

static int S_OGG_MemSeek(void* source, ogg_int64_t offset, int whence) {
    oggMemory_t* mem = (oggMemory_t*)source;
    ogg_int64_t target;

    switch (whence) {
        case SEEK_SET:
            target = offset;
            break;
        case SEEK_CUR:
            target = (ogg_int64_t)mem->pos + offset;
            break;
        case SEEK_END:
            target = (ogg_int64_t)mem->size + offset;
            break;
        default:
            return -1;
    }
    if (target < 0 || target > (ogg_int64_t)mem->size) {
        return -1;
    }
    mem->pos = (size_t)target;
    return 0;
}

static int S_OGG_MemClose(void* source) {
    (void)source;
    return 0;
}

static long S_OGG_MemTell(void* source) {
    return (long)((oggMemory_t*)source)->pos;
}

byte* S_OGG_DecodeMemory(const byte* data, int length, snd_info_t* info) {
    const ov_callbacks callbacks = {S_OGG_MemRead, S_OGG_MemSeek, S_OGG_MemClose, S_OGG_MemTell};
    oggMemory_t mem;
    OggVorbis_File vf;
    vorbis_info* vi;
    byte* pcm;
    int got = 0;
    int section;
    int bigEndian = 0;

#ifdef Q3_BIG_ENDIAN
    bigEndian = 1;
#endif

    mem.data = data;
    mem.size = (size_t)length;
    mem.pos = 0;

    if (ov_open_callbacks(&mem, &vf, NULL, 0, callbacks) != 0) {
        return NULL;
    }
    if (ov_streams(&vf) != 1 || !(vi = ov_info(&vf, 0))) {
        ov_clear(&vf);
        return NULL;
    }

    info->rate = vi->rate;
    info->width = OGG_SAMPLEWIDTH;
    info->channels = vi->channels;
    info->samples = (int)ov_pcm_total(&vf, 0);
    info->size = info->samples * info->channels * info->width;
    info->dataofs = 0;
    if (info->size <= 0) {
        ov_clear(&vf);
        return NULL;
    }

    pcm = (byte*)malloc(info->size);
    if (!pcm) {
        ov_clear(&vf);
        return NULL;
    }
    while (got < info->size) {
        long read = ov_read(&vf, (char*)pcm + got, info->size - got, bigEndian, OGG_SAMPLEWIDTH, 1, &section);

        if (read <= 0) {
            break;
        }
        got += (int)read;
    }
    ov_clear(&vf);

    if (got <= 0) {
        free(pcm);
        return NULL;
    }
    info->size = got;
    info->samples = got / (info->channels * info->width);
    return pcm;
}

#endif
