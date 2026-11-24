#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#define JPEG_INTERNALS
#include "../libjpeg-turbo/src/jpeglib.h"

typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
} launcherJpegError_t;

static void Launcher_JpegError(j_common_ptr cinfo) {
    longjmp(((launcherJpegError_t*)cinfo->err)->jump, 1);
}

static void Launcher_JpegMessage(j_common_ptr cinfo) {
    (void)cinfo;
}

unsigned char* Launcher_DecodeJpeg(const unsigned char* data, unsigned long size, int* width, int* height, int denom) {
    struct jpeg_decompress_struct cinfo;
    launcherJpegError_t err;
    unsigned char* volatile out = NULL;

    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = Launcher_JpegError;
    err.pub.output_message = Launcher_JpegMessage;
    if (setjmp(err.jump)) {
        jpeg_destroy_decompress(&cinfo);
        free(out);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char*)data, size);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    cinfo.scale_num = 1;
    cinfo.scale_denom = (unsigned int)(denom > 0 ? denom : 1);
    jpeg_start_decompress(&cinfo);
    out = malloc((size_t)cinfo.output_width * cinfo.output_height * 3);
    if (!out) {
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = out + (size_t)cinfo.output_scanline * cinfo.output_width * 3;

        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    *width = (int)cinfo.output_width;
    *height = (int)cinfo.output_height;
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return out;
}
