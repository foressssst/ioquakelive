/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

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
// cl_gif.c -- [ioquakelive] NOT FROM RETAIL: the frames of a GIF, for the pictures people post.
//
// The renderer reads PNG, JPEG and TGA, not GIF, and a GIF is what a sticker or a reaction is. This
// decodes one frame after the other onto a canvas the size of the picture, the way a browser shows
// it -- each frame's disposal, its transparent colour, interlaced rows -- and hands every finished
// canvas to the caller with how long it stays up. Only one canvas and the one before it are ever
// in memory, whatever the number of frames.

#include "client.h"
#include "cl_gif.h"

#define GIF_MAX_CODES 4096
#define GIF_MAX_CANVAS 2048        // a side of the canvas or of a frame, and nothing bigger is a sticker
#define GIF_MAX_PIXELS (1 << 28)  // decoded over the whole file, so one built to hold the thread cannot

typedef struct {
    const byte* data;
    int length;
    int pos;
} gifReader_t;

// the strings of one LZW stream. Each decode has its own: two fetching threads can be at a GIF each
typedef struct {
    unsigned short prefix[GIF_MAX_CODES];
    byte suffix[GIF_MAX_CODES];
    byte stack[GIF_MAX_CODES + 1];
} gifTable_t;

static int GIF_Byte(gifReader_t* r) {
    return r->pos < r->length ? r->data[r->pos++] : -1;
}

static int GIF_Word(gifReader_t* r) {
    int lo = GIF_Byte(r);
    int hi = GIF_Byte(r);

    return (lo < 0 || hi < 0) ? -1 : lo | (hi << 8);
}

// sub-blocks: a length byte, that many bytes, and a zero to end them
static qboolean GIF_SkipBlocks(gifReader_t* r) {
    int size;

    while ((size = GIF_Byte(r)) > 0) {
        if (r->pos + size > r->length) {
            return qfalse;
        }
        r->pos += size;
    }
    return size == 0 ? qtrue : qfalse;
}

/*
==================
GIF_DecodeImage

One image's LZW stream into colour indices, width * height of them, in the order the stream gives
them. The codes grow from minSize + 1 bits up to 12, a clear code starts the table over, and an end
code finishes it; a stream that runs short leaves the rest of the image at index 0
==================
*/
static qboolean GIF_DecodeImage(gifReader_t* r, gifTable_t* t, int minSize, byte* out, int count) {
    int clearCode, endCode, nextCode, codeSize, codeMask;
    int bits = 0, bitCount = 0, blockLeft = 0;
    int oldCode = -1, first = 0, written = 0;
    int code, in, sp, i;

    if (minSize < 2 || minSize > 11) {
        return qfalse;
    }

    clearCode = 1 << minSize;
    endCode = clearCode + 1;
    nextCode = clearCode + 2;
    codeSize = minSize + 1;
    codeMask = (1 << codeSize) - 1;

    for (i = 0; i < clearCode; i++) {
        t->prefix[i] = 0;
        t->suffix[i] = (byte)i;
    }

    while (written < count) {
        // enough bits for one code
        while (bitCount < codeSize) {
            if (blockLeft == 0) {
                blockLeft = GIF_Byte(r);
                if (blockLeft <= 0) {
                    goto done;  // the data ended before the picture did
                }
            }
            in = GIF_Byte(r);
            if (in < 0) {
                goto done;
            }
            blockLeft--;
            bits |= in << bitCount;
            bitCount += 8;
        }

        code = bits & codeMask;
        bits >>= codeSize;
        bitCount -= codeSize;

        if (code == clearCode) {
            codeSize = minSize + 1;
            codeMask = (1 << codeSize) - 1;
            nextCode = clearCode + 2;
            oldCode = -1;
            continue;
        }
        if (code == endCode) {
            break;
        }

        if (oldCode < 0) {
            if (code >= clearCode) {
                return qfalse;
            }
            out[written++] = t->suffix[code];
            oldCode = first = code;
            continue;
        }

        in = code;
        sp = 0;

        if (code >= nextCode) {
            // the one code that is not in the table yet: the last string and its own first byte
            if (code > nextCode) {
                return qfalse;
            }
            t->stack[sp++] = (byte)first;
            code = oldCode;
        }

        while (code >= clearCode) {
            if (sp >= GIF_MAX_CODES) {
                return qfalse;
            }
            t->stack[sp++] = t->suffix[code];
            code = t->prefix[code];
        }
        first = t->suffix[code];
        t->stack[sp++] = (byte)first;

        while (sp > 0 && written < count) {
            out[written++] = t->stack[--sp];
        }

        if (nextCode < GIF_MAX_CODES) {
            t->prefix[nextCode] = (unsigned short)oldCode;
            t->suffix[nextCode] = (byte)first;
            nextCode++;
            if (nextCode > codeMask && codeSize < 12) {
                codeSize++;
                codeMask = (1 << codeSize) - 1;
            }
        }
        oldCode = in;
    }

done:
    // whatever is left of the image data is skipped, so the next block starts where it should
    if (blockLeft > 0) {
        r->pos += blockLeft;
        if (r->pos > r->length) {
            r->pos = r->length;
        }
    }
    GIF_SkipBlocks(r);

    if (written < count) {
        memset(out + written, 0, count - written);
    }
    return qtrue;
}

/*
==================
GIF_Decode

Walks the whole file and calls back with every frame, already laid on the canvas. The canvas is
width * height * 4 bytes of red, green, blue and alpha with the top row first; the callback must
copy it, not keep it. A file broken halfway keeps the frames before the break, as a browser shows
it, and qfalse means not one frame came out. A canvas or a frame over GIF_MAX_CANVAS is refused,
and it stops after GIF_MAX_PIXELS pixels, or after maxFrames frames if that is above zero
==================
*/
qboolean GIF_Decode(const byte* data, int length, int maxFrames, gifFrameFunc_t func, void* user) {
    gifReader_t r;
    byte globalPalette[256 * 3];
    byte localPalette[256 * 3];
    int width, height, flags, globalSize = 0, frames = 0, pixels = 0;
    int delay = 100, transparent = -1, disposal = 0;
    gifTable_t* table = NULL;
    byte* canvas = NULL;
    byte* previous = NULL;
    byte* indices = NULL;

    if (!data || length < 13 || (memcmp(data, "GIF87a", 6) && memcmp(data, "GIF89a", 6))) {
        return qfalse;
    }

    r.data = data;
    r.length = length;
    r.pos = 6;

    width = GIF_Word(&r);
    height = GIF_Word(&r);
    flags = GIF_Byte(&r);
    GIF_Byte(&r);  // background colour: the canvas starts transparent, as browsers show it
    GIF_Byte(&r);  // aspect

    if (width <= 0 || height <= 0 || width > GIF_MAX_CANVAS || height > GIF_MAX_CANVAS || flags < 0) {
        return qfalse;
    }

    if (flags & 0x80) {
        globalSize = 2 << (flags & 7);
        if (r.pos + globalSize * 3 > r.length) {
            return qfalse;
        }
        memcpy(globalPalette, r.data + r.pos, globalSize * 3);
        r.pos += globalSize * 3;
    }

    table = (gifTable_t*)malloc(sizeof(*table));
    canvas = (byte*)calloc((size_t)width * height, 4);
    if (!table || !canvas) {
        goto out;
    }

    for (;;) {
        int block = GIF_Byte(&r);

        if (block < 0 || block == 0x3B) {  // the trailer, or the end of the data
            break;
        }

        if (block == 0x21) {  // an extension
            int label = GIF_Byte(&r);

            if (label == 0xF9) {  // graphic control: this frame's delay, disposal and transparency
                int size = GIF_Byte(&r);
                int packed = GIF_Byte(&r);
                int hundredths = GIF_Word(&r);
                int index = GIF_Byte(&r);

                if (size != 4 || packed < 0 || hundredths < 0 || index < 0) {
                    break;
                }
                disposal = (packed >> 2) & 7;
                transparent = (packed & 1) ? index : -1;
                // browsers show 0 and 10 ms as 100 ms, and so does this
                delay = hundredths <= 1 ? 100 : hundredths * 10;
                GIF_SkipBlocks(&r);
            } else if (!GIF_SkipBlocks(&r)) {
                break;
            }
            continue;
        }

        if (block == 0x2C) {  // an image
            int left = GIF_Word(&r);
            int top = GIF_Word(&r);
            int w = GIF_Word(&r);
            int h = GIF_Word(&r);
            int packed = GIF_Byte(&r);
            const byte* palette = globalPalette;
            int paletteSize = globalSize;
            int minSize, cost, x, y, row, pass, step;
            static const int passStart[4] = {0, 4, 2, 1};
            static const int passStep[4] = {8, 8, 4, 2};

            if (left < 0 || top < 0 || w <= 0 || h <= 0 || w > GIF_MAX_CANVAS || h > GIF_MAX_CANVAS ||
                packed < 0) {
                break;
            }

            // what the frame costs: its pixels, a table's worth at least, so thousands of tiny ones
            // stop as well, and the canvas's size too when it goes back to the one before, since that
            // copies the canvas whole
            cost = w * h + GIF_MAX_CODES + (disposal == 3 ? width * height : 0);
            if (pixels > GIF_MAX_PIXELS - cost) {
                break;
            }
            pixels += cost;

            if (packed & 0x80) {
                paletteSize = 2 << (packed & 7);
                if (r.pos + paletteSize * 3 > r.length) {
                    break;
                }
                memcpy(localPalette, r.data + r.pos, paletteSize * 3);
                r.pos += paletteSize * 3;
                palette = localPalette;
            }
            if (!paletteSize) {
                break;
            }

            minSize = GIF_Byte(&r);
            indices = (byte*)malloc((size_t)w * h);
            if (!indices || !GIF_DecodeImage(&r, table, minSize, indices, w * h)) {
                break;
            }

            // what a "restore to previous" frame goes back to is the canvas before it was drawn
            if (disposal == 3) {
                if (!previous) {
                    previous = (byte*)malloc((size_t)width * height * 4);
                    if (!previous) {
                        break;
                    }
                }
                memcpy(previous, canvas, (size_t)width * height * 4);
            }

            // the rows as they come: in order, or in the four passes of an interlaced image
            row = 0;
            for (pass = 0; pass < ((packed & 0x40) ? 4 : 1); pass++) {
                step = (packed & 0x40) ? passStep[pass] : 1;
                for (y = (packed & 0x40) ? passStart[pass] : 0; y < h; y += step, row++) {
                    int cy = top + y;

                    if (cy >= height) {
                        continue;
                    }
                    for (x = 0; x < w; x++) {
                        int cx = left + x;
                        int index = indices[row * w + x];
                        byte* px;

                        if (cx >= width || index == transparent || index >= paletteSize) {
                            continue;
                        }
                        px = canvas + ((size_t)cy * width + cx) * 4;
                        px[0] = palette[index * 3 + 0];
                        px[1] = palette[index * 3 + 1];
                        px[2] = palette[index * 3 + 2];
                        px[3] = 255;
                    }
                }
            }
            free(indices);
            indices = NULL;

            if (!func(frames, canvas, width, height, delay, user)) {
                break;
            }
            frames++;
            if (maxFrames > 0 && frames >= maxFrames) {
                break;
            }

            // what this frame leaves behind for the next one
            if (disposal == 2 && left < width) {
                for (y = top; y < top + h && y < height; y++) {
                    memset(canvas + ((size_t)y * width + left) * 4, 0,
                           (size_t)(left + w > width ? width - left : w) * 4);
                }
            } else if (disposal == 3) {
                memcpy(canvas, previous, (size_t)width * height * 4);
            }

            // a graphic control extension lasts for one image
            delay = 100;
            transparent = -1;
            disposal = 0;
            continue;
        }

        break;  // anything else is not a GIF this reads
    }

out:
    free(indices);
    free(table);
    free(canvas);
    free(previous);

    return frames > 0 ? qtrue : qfalse;
}
