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

#include <setjmp.h>

#include "tr_common.h"

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#ifdef USE_INTERNAL_JPEG
#define JPEG_INTERNALS
#endif

#include <jpeglib.h>

#ifndef USE_INTERNAL_JPEG
#if JPEG_LIB_VERSION < 80 && !defined(MEM_SRCDST_SUPPORTED)
#error Need system libjpeg >= 80 or jpeg_mem_ support
#endif
#endif

/* Catching errors, as done in libjpeg's example.c */
typedef struct q_jpeg_error_mgr_s {
    struct jpeg_error_mgr pub; /* "public" fields */

    jmp_buf setjmp_buffer; /* for return to caller */

    // [ioquakelive] a worker cannot print: the console is not thread safe
    qboolean silent;
} q_jpeg_error_mgr_t;

static void R_JPGErrorExit(j_common_ptr cinfo) {
    char buffer[JMSG_LENGTH_MAX];

    /* cinfo->err really points to a q_jpeg_error_mgr_s struct, so coerce pointer */
    q_jpeg_error_mgr_t* jerr = (q_jpeg_error_mgr_t*)cinfo->err;

    (*cinfo->err->format_message)(cinfo, buffer);

    if (!jerr->silent) {
        ri.Printf(PRINT_ALL, "Error: %s", buffer);
    }

    /* Return control to the setjmp point */
    longjmp(jerr->setjmp_buffer, 1);
}

static void R_JPGOutputMessage(j_common_ptr cinfo) {
    q_jpeg_error_mgr_t* jerr = (q_jpeg_error_mgr_t*)cinfo->err;
    char buffer[JMSG_LENGTH_MAX];

    if (jerr->silent) {
        return;
    }

    /* Create the message */
    (*cinfo->err->format_message)(cinfo, buffer);

    /* Send it to stderr, adding a newline */
    ri.Printf(PRINT_ALL, "%s\n", buffer);
}

// [ioquakelive] the output buffer is asked for through here: the main thread uses the engine's
// and a worker uses the system's, which is thread safe
static void* R_JPGAllocEngine(size_t bytes) {
    return ri.Malloc((int)bytes);
}

// [ioquakelive] the name of the last JPEG read, extension included, for the load list
char r_lastImageName[MAX_QPATH];

/*
=============
R_DecodeJPG

[ioquakelive] Decodes a JPEG that is already in memory. It exists because decoding is the only
part of the load path that can be spread across threads: reading cannot, the filesystem is not
thread safe. An invalid format does not take the game down -- it returns empty and the caller
decides -- because erroring out from a worker would not be safe.
=============
*/
void R_DecodeJPG(byte* fileData, int fileLen, byte** pic, int* width, int* height,
                 void* (*allocator)(size_t), qboolean silent) {
    struct jpeg_decompress_struct cinfo = {NULL};
    q_jpeg_error_mgr_t jerr;
    JSAMPARRAY buffer;
    unsigned int row_stride;
    unsigned int pixelcount, memcount;
    unsigned int sindex, dindex;
    byte* out;
    byte* buf;

    *pic = NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    cinfo.err->error_exit = R_JPGErrorExit;
    cinfo.err->output_message = R_JPGOutputMessage;
    jerr.silent = silent;

    /* Establish the setjmp return context for R_JPGErrorExit to use. */
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, fileData, fileLen);
    (void)jpeg_read_header(&cinfo, TRUE);

    /*
     * Make sure it always converts images to RGB color space. This will
     * automatically convert 8-bit greyscale images to RGB as well.
     */
    cinfo.out_color_space = JCS_RGB;

    (void)jpeg_start_decompress(&cinfo);

    pixelcount = cinfo.output_width * cinfo.output_height;

    if (!cinfo.output_width || !cinfo.output_height ||
        ((pixelcount * 4) / cinfo.output_width) / 4 != cinfo.output_height ||
        pixelcount > 0x1FFFFFFF || cinfo.output_components != 3) {
        jpeg_destroy_decompress(&cinfo);
        return;
    }

    memcount = pixelcount * 4;
    row_stride = cinfo.output_width * cinfo.output_components;

    out = (byte*)allocator(memcount);
    if (!out) {
        jpeg_destroy_decompress(&cinfo);
        return;
    }

    *width = cinfo.output_width;
    *height = cinfo.output_height;

    while (cinfo.output_scanline < cinfo.output_height) {
        buf = ((out + (row_stride * cinfo.output_scanline)));
        buffer = &buf;
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);
    }

    buf = out;

    // Expand from RGB to RGBA
    sindex = pixelcount * cinfo.output_components;
    dindex = memcount;

    do {
        buf[--dindex] = 255;
        buf[--dindex] = buf[--sindex];
        buf[--dindex] = buf[--sindex];
        buf[--dindex] = buf[--sindex];
    } while (sindex);

    *pic = out;

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

void R_LoadJPG(const char* filename, unsigned char** pic, int* width, int* height) {
    union {
        byte* b;
        void* v;
    } fbuffer;
    int len;

    // [ioquakelive] reading from the pk3 and decoding are measured apart: see r_imageReadUs in
    // tr_common.h
    {
        int64_t readStart = ri.Microseconds();

        len = ri.FS_ReadFile((char*)filename, &fbuffer.v);
        r_imageReadUs += ri.Microseconds() - readStart;
    }
    if (!fbuffer.b || len < 0) {
        return;
    }

    R_DecodeJPG(fbuffer.b, len, pic, width, height, R_JPGAllocEngine, qfalse);
    if (*pic) {
        // [ioquakelive] for the map's load list: the real name, extension included
        Q_strncpyz(r_lastImageName, filename, sizeof(r_lastImageName));
    } else {
        ri.Printf(PRINT_ALL, ", loading file %s\n", filename);
    }
    ri.FS_FreeFile(fbuffer.v);
}

/* Expanded data destination object for stdio output */

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    byte* outfile; /* target stream */
    int size;
} my_destination_mgr;

typedef my_destination_mgr* my_dest_ptr;

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

static void
init_destination(j_compress_ptr cinfo) {
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;

    dest->pub.next_output_byte = dest->outfile;
    dest->pub.free_in_buffer = dest->size;
}

/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

static boolean
empty_output_buffer(j_compress_ptr cinfo) {
    my_dest_ptr dest = (my_dest_ptr)cinfo->dest;

    jpeg_destroy_compress(cinfo);

    // Make crash fatal or we would probably leak memory.
    ri.Error(ERR_FATAL, "Output buffer for encoded JPEG image has insufficient size of %d bytes",
             dest->size);

    return FALSE;
}

/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

static void term_destination(j_compress_ptr cinfo) {
}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

static void
jpegDest(j_compress_ptr cinfo, byte* outfile, int size) {
    my_dest_ptr dest;

    /* The destination object is made permanent so that multiple JPEG images
     * can be written to the same file without re-executing jpeg_stdio_dest.
     * This makes it dangerous to use this manager and a different destination
     * manager serially with the same JPEG object, because their private object
     * sizes may be different.  Caveat programmer.
     */
    if (cinfo->dest == NULL) { /* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr*)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
                                                                               sizeof(my_destination_mgr));
    }

    dest = (my_dest_ptr)cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outfile = outfile;
    dest->size = size;
}

/*
=================
SaveJPGToBuffer

Encodes JPEG from image in image_buffer and writes to buffer.
Expects RGB input data
=================
*/
size_t RE_SaveJPGToBuffer(byte* buffer, size_t bufSize, int quality, int image_width, int image_height, byte* image_buffer, int padding) {
    struct jpeg_compress_struct cinfo;
    q_jpeg_error_mgr_t jerr;
    JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
    my_dest_ptr dest;
    int row_stride; /* physical row width in image buffer */
    size_t outcount;

    /* Step 1: allocate and initialize JPEG compression object */
    cinfo.err = jpeg_std_error(&jerr.pub);
    cinfo.err->error_exit = R_JPGErrorExit;
    cinfo.err->output_message = R_JPGOutputMessage;

    /* Establish the setjmp return context for R_JPGErrorExit to use. */
    if (setjmp(jerr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object and return.
         */
        jpeg_destroy_compress(&cinfo);

        ri.Printf(PRINT_ALL, "\n");
        return 0;
    }

    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    /* Step 2: specify data destination (eg, a file) */
    /* Note: steps 2 and 3 can be done in either order. */
    jpegDest(&cinfo, buffer, bufSize);

    /* Step 3: set parameters for compression */
    cinfo.image_width = image_width; /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;     /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
    /* If quality is set high, disable chroma subsampling */
    if (quality >= 85) {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 1;
    }

    /* Step 4: Start compressor */
    jpeg_start_compress(&cinfo, TRUE);

    /* Step 5: while (scan lines remain to be written) */
    /*           jpeg_write_scanlines(...); */
    row_stride = image_width * cinfo.input_components + padding; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could pass
         * more than one scanline at a time if that's more convenient.
         */
        row_pointer[0] = &image_buffer[((cinfo.image_height - 1) * row_stride) - cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    /* Step 6: Finish compression */
    jpeg_finish_compress(&cinfo);

    dest = (my_dest_ptr)cinfo.dest;
    outcount = dest->size - dest->pub.free_in_buffer;

    /* Step 7: release JPEG compression object */
    jpeg_destroy_compress(&cinfo);

    /* And we're done! */
    return outcount;
}

void RE_SaveJPG(char* filename, int quality, int image_width, int image_height, byte* image_buffer, int padding) {
    byte* out;
    size_t bufSize;

    bufSize = image_width * image_height * 3;
    out = ri.Hunk_AllocateTempMemory(bufSize);

    bufSize = RE_SaveJPGToBuffer(out, bufSize, quality, image_width, image_height, image_buffer, padding);
    ri.FS_WriteFile(filename, out, bufSize);

    ri.Hunk_FreeTempMemory(out);
}
