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
#include "tr_local.h"

#include "tr_dsa.h"

static byte s_intensitytable[256];
static unsigned char s_gammatable[256];

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE 1024
static image_t* hashTable[FILE_HASH_SIZE];

void R_GammaCorrect(byte* buffer, int bufSize) {
    int i;

    for (i = 0; i < bufSize; i++) {
        buffer[i] = s_gammatable[buffer[i]];
    }
}

typedef struct {
    char* name;
    int minimize, maximize;
} textureMode_t;

textureMode_t modes[] = {
    {"GL_NEAREST", GL_NEAREST, GL_NEAREST},
    {"GL_LINEAR", GL_LINEAR, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}};

static long generateHashValue(const char* fname) {
    int i;
    long hash;
    char letter;

    hash = 0;
    i = 0;
    while (fname[i] != '\0') {
        letter = tolower(fname[i]);
        if (letter == '.')
            break;
        if (letter == '\\')
            letter = '/';
        hash += (long)(letter) * (i + 119);
        i++;
    }
    hash &= (FILE_HASH_SIZE - 1);
    return hash;
}

void GL_TextureMode(const char* string) {
    int i;
    image_t* glt;

    for (i = 0; i < 6; i++) {
        if (!Q_stricmp(modes[i].name, string)) {
            break;
        }
    }

    if (i == 5 && glConfig.hardwareType == GLHW_3DFX_2D3D) {
        ri.Printf(PRINT_ALL, "Refusing to set trilinear on a voodoo.\n");
        i = 3;
    }

    if (i == 6) {
        ri.Printf(PRINT_ALL, "bad filter name\n");
        return;
    }

    gl_filter_min = modes[i].minimize;
    gl_filter_max = modes[i].maximize;

    for (i = 0; i < tr.numImages; i++) {
        glt = tr.images[i];
        if (glt->flags & IMGFLAG_MIPMAP && !(glt->flags & IMGFLAG_CUBEMAP)) {
            qglTextureParameterfEXT(glt->texnum, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
            qglTextureParameterfEXT(glt->texnum, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
        }
    }
}

int R_SumOfUsedImages(void) {
    int total;
    int i;

    total = 0;
    for (i = 0; i < tr.numImages; i++) {
        if (tr.images[i]->frameUsed == tr.frameCount) {
            total += tr.images[i]->uploadWidth * tr.images[i]->uploadHeight;
        }
    }

    return total;
}

void R_ImageList_f(void) {
    int i;
    int estTotalSize = 0;

    ri.Printf(PRINT_ALL, "\n      -w-- -h-- -type-- -size- --name-------\n");

    for (i = 0; i < tr.numImages; i++) {
        image_t* image = tr.images[i];
        char* format = "????   ";
        char* sizeSuffix;
        int estSize;
        int displaySize;

        estSize = image->uploadHeight * image->uploadWidth;

        switch (image->internalFormat) {
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                format = "sDXT1  ";
                estSize /= 2;
                break;
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                format = "sDXT5  ";
                break;
            case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
                format = "sBPTC  ";
                break;
            case GL_COMPRESSED_RG_RGTC2:
                format = "RGTC2  ";
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                format = "DXT1   ";
                estSize /= 2;
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                format = "DXT1a  ";
                estSize /= 2;
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                format = "DXT5   ";
                break;
            case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
                format = "BPTC   ";
                break;
            case GL_RGB4_S3TC:
                format = "S3TC   ";
                estSize /= 2;
                break;
            case GL_RGBA16F:
                format = "RGBA16F";
                estSize *= 8;
                break;
            case GL_RGBA16:
                format = "RGBA16 ";
                estSize *= 8;
                break;
            case GL_RGBA4:
            case GL_RGBA8:
            case GL_RGBA:
                format = "RGBA   ";
                estSize *= 4;
                break;
            case GL_LUMINANCE8:
            case GL_LUMINANCE:
                format = "L      ";
                break;
            case GL_RGB5:
            case GL_RGB8:
            case GL_RGB:
                format = "RGB    ";
                estSize *= 3;
                break;
            case GL_LUMINANCE8_ALPHA8:
            case GL_LUMINANCE_ALPHA:
                format = "LA     ";
                estSize *= 2;
                break;
            case GL_SRGB_EXT:
            case GL_SRGB8_EXT:
                format = "sRGB   ";
                estSize *= 3;
                break;
            case GL_SRGB_ALPHA_EXT:
            case GL_SRGB8_ALPHA8_EXT:
                format = "sRGBA  ";
                estSize *= 4;
                break;
            case GL_SLUMINANCE_EXT:
            case GL_SLUMINANCE8_EXT:
                format = "sL     ";
                break;
            case GL_SLUMINANCE_ALPHA_EXT:
            case GL_SLUMINANCE8_ALPHA8_EXT:
                format = "sLA    ";
                estSize *= 2;
                break;
            case GL_DEPTH_COMPONENT16:
                format = "Depth16";
                estSize *= 2;
                break;
            case GL_DEPTH_COMPONENT24:
                format = "Depth24";
                estSize *= 3;
                break;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_COMPONENT32:
                format = "Depth32";
                estSize *= 4;
                break;
        }

        if (image->flags & IMGFLAG_MIPMAP)
            estSize += estSize / 2;

        sizeSuffix = "b ";
        displaySize = estSize;

        if (displaySize > 1024) {
            displaySize /= 1024;
            sizeSuffix = "kb";
        }

        if (displaySize > 1024) {
            displaySize /= 1024;
            sizeSuffix = "Mb";
        }

        if (displaySize > 1024) {
            displaySize /= 1024;
            sizeSuffix = "Gb";
        }

        ri.Printf(PRINT_ALL, "%4i: %4ix%4i %s %4i%s %s\n", i, image->uploadWidth, image->uploadHeight, format, displaySize, sizeSuffix, image->imgName);
        estTotalSize += estSize;
    }

    ri.Printf(PRINT_ALL, " ---------\n");
    ri.Printf(PRINT_ALL, " approx %i bytes\n", estTotalSize);
    ri.Printf(PRINT_ALL, " %i total images\n\n", tr.numImages);
}

static void ResampleTexture(byte* in, int inwidth, int inheight, byte* out, int outwidth, int outheight) {
    int i, j;
    byte *inrow, *inrow2;
    int frac, fracstep;
    int p1[2048], p2[2048];
    byte *pix1, *pix2, *pix3, *pix4;

    if (outwidth > 2048)
        ri.Error(ERR_DROP, "ResampleTexture: max width");

    fracstep = inwidth * 0x10000 / outwidth;

    frac = fracstep >> 2;
    for (i = 0; i < outwidth; i++) {
        p1[i] = 4 * (frac >> 16);
        frac += fracstep;
    }
    frac = 3 * (fracstep >> 2);
    for (i = 0; i < outwidth; i++) {
        p2[i] = 4 * (frac >> 16);
        frac += fracstep;
    }

    for (i = 0; i < outheight; i++) {
        inrow = in + 4 * inwidth * (int)((i + 0.25) * inheight / outheight);
        inrow2 = in + 4 * inwidth * (int)((i + 0.75) * inheight / outheight);
        for (j = 0; j < outwidth; j++) {
            pix1 = inrow + p1[j];
            pix2 = inrow + p2[j];
            pix3 = inrow2 + p1[j];
            pix4 = inrow2 + p2[j];
            *out++ = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
            *out++ = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
            *out++ = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
            *out++ = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
        }
    }
}

static void RGBAtoYCoCgA(const byte* in, byte* out, int width, int height) {
    int x, y;

    for (y = 0; y < height; y++) {
        const byte* inbyte = in + y * width * 4;
        byte* outbyte = out + y * width * 4;

        for (x = 0; x < width; x++) {
            byte r, g, b, a, rb2;

            r = *inbyte++;
            g = *inbyte++;
            b = *inbyte++;
            a = *inbyte++;
            rb2 = (r + b) >> 1;

            *outbyte++ = (g + rb2) >> 1;
            *outbyte++ = (r - b + 256) >> 1;
            *outbyte++ = (g - rb2 + 256) >> 1;
            *outbyte++ = a;
        }
    }
}

static void YCoCgAtoRGBA(const byte* in, byte* out, int width, int height) {
    int x, y;

    for (y = 0; y < height; y++) {
        const byte* inbyte = in + y * width * 4;
        byte* outbyte = out + y * width * 4;

        for (x = 0; x < width; x++) {
            byte _Y, Co, Cg, a;

            _Y = *inbyte++;
            Co = *inbyte++;
            Cg = *inbyte++;
            a = *inbyte++;

            *outbyte++ = CLAMP(_Y + Co - Cg, 0, 255);
            *outbyte++ = CLAMP(_Y + Cg - 128, 0, 255);
            *outbyte++ = CLAMP(_Y - Co - Cg + 256, 0, 255);
            *outbyte++ = a;
        }
    }
}

static void RGBAtoNormal(const byte* in, byte* out, int width, int height, qboolean clampToEdge) {
    int x, y, max;

    max = 1;
    for (y = 0; y < height; y++) {
        const byte* inbyte = in + y * width * 4;
        byte* outbyte = out + y * width * 4 + 3;

        for (x = 0; x < width; x++) {
            byte result = (inbyte[0] >> 2) + (inbyte[1] >> 1) + (inbyte[2] >> 2);
            result = result * result / 255;
            *outbyte = result;
            max = MAX(max, *outbyte);
            outbyte += 4;
            inbyte += 4;
        }
    }

    if (max < 255) {
        for (y = 0; y < height; y++) {
            byte* outbyte = out + y * width * 4 + 3;

            for (x = 0; x < width; x++) {
                *outbyte = *outbyte + (255 - max);
                outbyte += 4;
            }
        }
    }

    for (y = 0; y < height; y++) {
        byte* outbyte = out + y * width * 4;

        for (x = 0; x < width; x++) {

            byte s[9];
            int x2, y2, i;
            vec3_t normal;

            i = 0;
            for (y2 = -1; y2 <= 1; y2++) {
                int src_y = y + y2;

                if (clampToEdge) {
                    src_y = CLAMP(src_y, 0, height - 1);
                } else {
                    src_y = (src_y + height) % height;
                }

                for (x2 = -1; x2 <= 1; x2++) {
                    int src_x = x + x2;

                    if (clampToEdge) {
                        src_x = CLAMP(src_x, 0, width - 1);
                    } else {
                        src_x = (src_x + width) % width;
                    }

                    s[i++] = *(out + (src_y * width + src_x) * 4 + 3);
                }
            }

            normal[0] = s[0] - s[2] + 2 * s[3] - 2 * s[5] + s[6] - s[8];

            normal[1] = s[0] + 2 * s[1] + s[2]

                        - s[6] - 2 * s[7] - s[8];

            normal[2] = s[4] * 4;

            if (!VectorNormalize2(normal, normal)) {
                VectorSet(normal, 0, 0, 1);
            }

            *outbyte++ = FloatToOffsetByte(normal[0]);
            *outbyte++ = FloatToOffsetByte(normal[1]);
            *outbyte++ = FloatToOffsetByte(normal[2]);
            outbyte++;
        }
    }
}

#define COPYSAMPLE(a, b) *(unsigned int*)(a) = *(unsigned int*)(b)

static void DoFCBI(byte* in, byte* out, int width, int height, int component) {
    int x, y;
    byte *outbyte, *inbyte;

    for (y = 2; y < height - 2; y += 2) {
        inbyte = in + (y * width + 2) * 4 + component;
        outbyte = out + (y * width + 2) * 4 + component;

        for (x = 2; x < width - 2; x += 2) {
            *outbyte = *inbyte;
            outbyte += 8;
            inbyte += 8;
        }
    }

    for (y = 3; y < height - 3; y += 2) {

        byte sa, sb, sc, sd, se, sf, sg, sh, si, sj, sk, sl;
        byte *line1, *line2, *line3, *line4;

        x = 3;

        line1 = in + ((y - 3) * width + (x - 1)) * 4 + component;
        line2 = in + ((y - 1) * width + (x - 3)) * 4 + component;
        line3 = in + ((y + 1) * width + (x - 3)) * 4 + component;
        line4 = in + ((y + 3) * width + (x - 1)) * 4 + component;

        sa = *line1;
        line1 += 8;
        sc = *line2;
        line2 += 8;
        sd = *line2;
        line2 += 8;
        se = *line2;
        line2 += 8;
        sg = *line3;
        line3 += 8;
        sh = *line3;
        line3 += 8;
        si = *line3;
        line3 += 8;
        sk = *line4;
        line4 += 8;

        outbyte = out + (y * width + x) * 4 + component;

        for (; x < width - 3; x += 2) {
            int NWd, NEd, NWp, NEp;

            sb = *line1;
            line1 += 8;
            sf = *line2;
            line2 += 8;
            sj = *line3;
            line3 += 8;
            sl = *line4;
            line4 += 8;

            NWp = sd + si;
            NEp = se + sh;
            NWd = abs(sd - si);
            NEd = abs(se - sh);

            if (NWd > 100 || NEd > 100 || abs(NWp - NEp) > 200) {
                if (NWd < NEd)
                    *outbyte = NWp >> 1;
                else
                    *outbyte = NEp >> 1;
            } else {
                int NWdd, NEdd;

                NEdd = abs(sg + sb - 3 * NEp + sk + sf + NWp);
                NWdd = abs(sa + sj - 3 * NWp + sc + sl + NEp);

                if (NWdd > NEdd)
                    *outbyte = NWp >> 1;
                else
                    *outbyte = NEp >> 1;
            }

            outbyte += 8;

            sa = sb;
            sc = sd;
            sd = se;
            se = sf;
            sg = sh;
            sh = si;
            si = sj;
            sk = sl;
        }
    }

    for (y = 3; y < height - 3; y += 2) {
        inbyte = out + (y * width + 3) * 4 + component;
        outbyte = in + (y * width + 3) * 4 + component;

        for (x = 3; x < width - 3; x += 2) {
            *outbyte = *inbyte;
            outbyte += 8;
            inbyte += 8;
        }
    }

    for (y = 2; y < height - 3; y++) {

        byte sa, sb, sc, sd, se, sf, sg, sh, si, sj, sk, sl;
        byte *line1, *line2, *line3, *line4, *line5;

        x = (y + 1) % 2 + 2;

        line1 = in + ((y - 2) * width + (x - 1)) * 4 + component;
        line2 = in + ((y - 1) * width + (x - 2)) * 4 + component;
        line3 = in + ((y)*width + (x - 1)) * 4 + component;
        line4 = in + ((y + 1) * width + (x - 2)) * 4 + component;
        line5 = in + ((y + 2) * width + (x - 1)) * 4 + component;

        sa = *line1;
        line1 += 8;
        sc = *line2;
        line2 += 8;
        sd = *line2;
        line2 += 8;
        sf = *line3;
        line3 += 8;
        sh = *line4;
        line4 += 8;
        si = *line4;
        line4 += 8;
        sk = *line5;
        line5 += 8;

        outbyte = out + (y * width + x) * 4 + component;

        for (; x < width - 3; x += 2) {
            int hd, vd, hp, vp;

            sb = *line1;
            line1 += 8;
            se = *line2;
            line2 += 8;
            sg = *line3;
            line3 += 8;
            sj = *line4;
            line4 += 8;
            sl = *line5;
            line5 += 8;

            hp = sf + sg;
            vp = sd + si;
            hd = abs(sf - sg);
            vd = abs(sd - si);

            if (hd > 100 || vd > 100 || abs(hp - vp) > 200) {
                if (hd < vd)
                    *outbyte = hp >> 1;
                else
                    *outbyte = vp >> 1;
            } else {
                int hdd, vdd;

                hdd = abs(sc + se - 3 * hp + sh + sj + vp);
                vdd = abs(sa + sk - 3 * vp + sb + sl + hp);

                if (hdd > vdd)
                    *outbyte = hp >> 1;
                else
                    *outbyte = vp >> 1;
            }

            outbyte += 8;

            sa = sb;
            sc = sd;
            sd = se;
            sf = sg;
            sh = si;
            si = sj;
            sk = sl;
        }
    }
}

static void DoFCBIQuick(byte* in, byte* out, int width, int height, int component) {
    int x, y;
    byte *outbyte, *inbyte;

    for (y = 2; y < height - 2; y += 2) {
        inbyte = in + (y * width + 2) * 4 + component;
        outbyte = out + (y * width + 2) * 4 + component;

        for (x = 2; x < width - 2; x += 2) {
            *outbyte = *inbyte;
            outbyte += 8;
            inbyte += 8;
        }
    }

    for (y = 3; y < height - 4; y += 2) {
        byte sd, se, sh, si;
        byte *line2, *line3;

        x = 3;

        line2 = in + ((y - 1) * width + (x - 1)) * 4 + component;
        line3 = in + ((y + 1) * width + (x - 1)) * 4 + component;

        sd = *line2;
        line2 += 8;
        sh = *line3;
        line3 += 8;

        outbyte = out + (y * width + x) * 4 + component;

        for (; x < width - 4; x += 2) {
            int NWd, NEd, NWp, NEp;

            se = *line2;
            line2 += 8;
            si = *line3;
            line3 += 8;

            NWp = sd + si;
            NEp = se + sh;
            NWd = abs(sd - si);
            NEd = abs(se - sh);

            if (NWd < NEd)
                *outbyte = NWp >> 1;
            else
                *outbyte = NEp >> 1;

            outbyte += 8;

            sd = se;
            sh = si;
        }
    }

    for (y = 3; y < height - 3; y += 2) {
        inbyte = out + (y * width + 3) * 4 + component;
        outbyte = in + (y * width + 3) * 4 + component;

        for (x = 3; x < width - 3; x += 2) {
            *outbyte = *inbyte;
            outbyte += 8;
            inbyte += 8;
        }
    }

    for (y = 2; y < height - 3; y++) {
        byte sd, sf, sg, si;
        byte *line2, *line3, *line4;

        x = (y + 1) % 2 + 2;

        line2 = in + ((y - 1) * width + (x)) * 4 + component;
        line3 = in + ((y)*width + (x - 1)) * 4 + component;
        line4 = in + ((y + 1) * width + (x)) * 4 + component;

        outbyte = out + (y * width + x) * 4 + component;

        sf = *line3;
        line3 += 8;

        for (; x < width - 3; x += 2) {
            int hd, vd, hp, vp;

            sd = *line2;
            line2 += 8;
            sg = *line3;
            line3 += 8;
            si = *line4;
            line4 += 8;

            hp = sf + sg;
            vp = sd + si;
            hd = abs(sf - sg);
            vd = abs(sd - si);

            if (hd < vd)
                *outbyte = hp >> 1;
            else
                *outbyte = vp >> 1;

            outbyte += 8;

            sf = sg;
        }
    }
}

static void DoLinear(byte* in, byte* out, int width, int height) {
    int x, y, i;
    byte *outbyte, *inbyte;

    for (y = 2; y < height - 2; y += 2) {
        x = 2;

        inbyte = in + (y * width + x) * 4;
        outbyte = out + (y * width + x) * 4;

        for (; x < width - 2; x += 2) {
            COPYSAMPLE(outbyte, inbyte);
            outbyte += 8;
            inbyte += 8;
        }
    }

    for (y = 1; y < height - 1; y += 2) {
        byte sd[4] = {0}, se[4] = {0}, sh[4] = {0}, si[4] = {0};
        byte *line2, *line3;

        x = 1;

        line2 = in + ((y - 1) * width + (x - 1)) * 4;
        line3 = in + ((y + 1) * width + (x - 1)) * 4;

        COPYSAMPLE(sd, line2);
        line2 += 8;
        COPYSAMPLE(sh, line3);
        line3 += 8;

        outbyte = out + (y * width + x) * 4;

        for (; x < width - 1; x += 2) {
            COPYSAMPLE(se, line2);
            line2 += 8;
            COPYSAMPLE(si, line3);
            line3 += 8;

            for (i = 0; i < 4; i++) {
                *outbyte++ = (sd[i] + si[i] + se[i] + sh[i]) >> 2;
            }

            outbyte += 4;

            COPYSAMPLE(sd, se);
            COPYSAMPLE(sh, si);
        }
    }

    for (y = 1; y < height - 1; y += 2) {
        x = 1;

        inbyte = out + (y * width + x) * 4;
        outbyte = in + (y * width + x) * 4;

        for (; x < width - 1; x += 2) {
            COPYSAMPLE(outbyte, inbyte);
            outbyte += 8;
            inbyte += 8;
        }
    }

    for (y = 1; y < height - 1; y++) {
        byte sd[4], sf[4], sg[4], si[4];
        byte *line2, *line3, *line4;

        x = y % 2 + 1;

        line2 = in + ((y - 1) * width + (x)) * 4;
        line3 = in + ((y)*width + (x - 1)) * 4;
        line4 = in + ((y + 1) * width + (x)) * 4;

        COPYSAMPLE(sf, line3);
        line3 += 8;

        outbyte = out + (y * width + x) * 4;

        for (; x < width - 1; x += 2) {
            COPYSAMPLE(sd, line2);
            line2 += 8;
            COPYSAMPLE(sg, line3);
            line3 += 8;
            COPYSAMPLE(si, line4);
            line4 += 8;

            for (i = 0; i < 4; i++) {
                *outbyte++ = (sf[i] + sg[i] + sd[i] + si[i]) >> 2;
            }

            outbyte += 4;

            COPYSAMPLE(sf, sg);
        }
    }
}

static void ExpandHalfTextureToGrid(byte* data, int width, int height) {
    int x, y;

    for (y = height / 2; y > 0; y--) {
        byte* outbyte = data + ((y * 2 - 1) * (width)-2) * 4;
        byte* inbyte = data + (y * (width / 2) - 1) * 4;

        for (x = width / 2; x > 0; x--) {
            COPYSAMPLE(outbyte, inbyte);

            outbyte -= 8;
            inbyte -= 4;
        }
    }
}

static void FillInNormalizedZ(const byte* in, byte* out, int width, int height) {
    int x, y;

    for (y = 0; y < height; y++) {
        const byte* inbyte = in + y * width * 4;
        byte* outbyte = out + y * width * 4;

        for (x = 0; x < width; x++) {
            byte nx, ny, nz, h;
            float fnx, fny, fll, fnz;

            nx = *inbyte++;
            ny = *inbyte++;
            inbyte++;
            h = *inbyte++;

            fnx = OffsetByteToFloat(nx);
            fny = OffsetByteToFloat(ny);
            fll = 1.0f - fnx * fnx - fny * fny;
            if (fll >= 0.0f)
                fnz = (float)sqrt(fll);
            else
                fnz = 0.0f;

            nz = FloatToOffsetByte(fnz);

            *outbyte++ = nx;
            *outbyte++ = ny;
            *outbyte++ = nz;
            *outbyte++ = h;
        }
    }
}

#define WORKBLOCK_SIZE 128
#define WORKBLOCK_BORDER 4
#define WORKBLOCK_REALSIZE (WORKBLOCK_SIZE + WORKBLOCK_BORDER * 2)

static void FCBIByBlock(byte* data, int width, int height, qboolean clampToEdge, qboolean normalized) {
    byte workdata[WORKBLOCK_REALSIZE * WORKBLOCK_REALSIZE * 4];
    byte outdata[WORKBLOCK_REALSIZE * WORKBLOCK_REALSIZE * 4];
    byte *inbyte, *outbyte;
    int x, y;
    int srcx, srcy;

    ExpandHalfTextureToGrid(data, width, height);

    for (y = 0; y < height; y += WORKBLOCK_SIZE) {
        for (x = 0; x < width; x += WORKBLOCK_SIZE) {
            int x2, y2;
            int workwidth, workheight, fullworkwidth, fullworkheight;

            workwidth = MIN(WORKBLOCK_SIZE, width - x);
            workheight = MIN(WORKBLOCK_SIZE, height - y);

            fullworkwidth = workwidth + WORKBLOCK_BORDER * 2;
            fullworkheight = workheight + WORKBLOCK_BORDER * 2;

            for (y2 = 0; y2 < fullworkheight; y2 += 2) {
                srcy = y + y2 - WORKBLOCK_BORDER;

                if (clampToEdge) {
                    srcy = CLAMP(srcy, 0, height - 2);
                } else {
                    srcy = (srcy + height) % height;
                }

                outbyte = workdata + y2 * fullworkwidth * 4;
                inbyte = data + srcy * width * 4;

                for (x2 = 0; x2 < fullworkwidth; x2 += 2) {
                    srcx = x + x2 - WORKBLOCK_BORDER;

                    if (clampToEdge) {
                        srcx = CLAMP(srcx, 0, width - 2);
                    } else {
                        srcx = (srcx + width) % width;
                    }

                    COPYSAMPLE(outbyte, inbyte + srcx * 4);
                    outbyte += 8;
                }
            }

            DoLinear(workdata, outdata, fullworkwidth, fullworkheight);

            if (!normalized) {
                switch (r_imageUpsampleType->integer) {
                    case 0:
                        break;
                    case 1:
                        DoFCBIQuick(workdata, outdata, fullworkwidth, fullworkheight, 0);
                        break;
                    case 2:
                    default:
                        DoFCBI(workdata, outdata, fullworkwidth, fullworkheight, 0);
                        break;
                }
            } else {
                switch (r_imageUpsampleType->integer) {
                    case 0:
                        break;
                    case 1:
                        DoFCBIQuick(workdata, outdata, fullworkwidth, fullworkheight, 0);
                        DoFCBIQuick(workdata, outdata, fullworkwidth, fullworkheight, 1);
                        break;
                    case 2:
                    default:
                        DoFCBI(workdata, outdata, fullworkwidth, fullworkheight, 0);
                        DoFCBI(workdata, outdata, fullworkwidth, fullworkheight, 1);
                        break;
                }
            }

            for (y2 = 0; y2 < workheight; y2++) {
                inbyte = outdata + ((y2 + WORKBLOCK_BORDER) * fullworkwidth + WORKBLOCK_BORDER) * 4;
                outbyte = data + ((y + y2) * width + x) * 4;
                for (x2 = 0; x2 < workwidth; x2++) {
                    COPYSAMPLE(outbyte, inbyte);
                    outbyte += 4;
                    inbyte += 4;
                }
            }
        }
    }
}
#undef COPYSAMPLE

void R_LightScaleTexture(byte* in, int inwidth, int inheight, qboolean only_gamma) {
    if (r_colorCorrectActive->integer) {
        return;
    }

    if (only_gamma) {
        if (!glConfig.deviceSupportsGamma) {
            int i, c;
            byte* p;

            p = in;

            c = inwidth * inheight;
            for (i = 0; i < c; i++, p += 4) {
                p[0] = s_gammatable[p[0]];
                p[1] = s_gammatable[p[1]];
                p[2] = s_gammatable[p[2]];
            }
        }
    } else {
        int i, c;
        byte* p;

        p = in;

        c = inwidth * inheight;

        if (glConfig.deviceSupportsGamma) {
            for (i = 0; i < c; i++, p += 4) {
                p[0] = s_intensitytable[p[0]];
                p[1] = s_intensitytable[p[1]];
                p[2] = s_intensitytable[p[2]];
            }
        } else {
            for (i = 0; i < c; i++, p += 4) {
                p[0] = s_gammatable[s_intensitytable[p[0]]];
                p[1] = s_gammatable[s_intensitytable[p[1]]];
                p[2] = s_gammatable[s_intensitytable[p[2]]];
            }
        }
    }
}

static void R_MipMap2(byte* in, int inWidth, int inHeight) {
    int i, j, k, total;
    int outWidth, outHeight;
    int widthMask, heightMask;
    byte* temp;
    byte* out;

    outWidth = inWidth >> 1;
    outHeight = inHeight >> 1;
    temp = ri.Hunk_AllocateTempMemory(MAX(1, outWidth * outHeight * 4));

    widthMask = inWidth - 1;
    heightMask = inHeight - 1;

#define TAP(y, x) in[(((i * 2 + (y)) & heightMask) * inWidth + ((j * 2 + (x)) & widthMask)) * 4 + k]
    for (i = 0; i < outHeight; i++) {
        for (j = 0; j < outWidth; j++) {
            out = temp + (i * outWidth + j) * 4;
            for (k = 0; k < 4; k++) {
                total = TAP(-1, -1) + 2 * TAP(-1, 0) + 2 * TAP(-1, 1) + TAP(-1, 2) +
                        2 * TAP(0, -1) + 4 * TAP(0, 0) + 4 * TAP(0, 1) + 2 * TAP(0, 2) +
                        2 * TAP(1, -1) + 4 * TAP(1, 0) + 4 * TAP(1, 1) + 2 * TAP(1, 2) +
                        TAP(2, -1) + 2 * TAP(2, 0) + 2 * TAP(2, 1) + TAP(2, 2);
                out[k] = total / 36;
            }
        }
    }
#undef TAP

    Com_Memcpy(in, temp, outWidth * outHeight * 4);
    ri.Hunk_FreeTempMemory(temp);
}

static void R_MipMap(byte* in, int width, int height) {
    int i, j, row;
    byte* out;

    if (!r_simpleMipMaps->integer) {
        R_MipMap2(in, width, height);
        return;
    }

    if (width == 1 && height == 1) {
        return;
    }

    row = width * 4;
    out = in;
    width >>= 1;
    height >>= 1;

    if (width == 0 || height == 0) {
        width += height;
        for (i = 0; i < width; i++, out += 4, in += 8) {
            out[0] = (in[0] + in[4]) >> 1;
            out[1] = (in[1] + in[5]) >> 1;
            out[2] = (in[2] + in[6]) >> 1;
            out[3] = (in[3] + in[7]) >> 1;
        }
        return;
    }

    for (i = 0; i < height; i++, in += row) {
        for (j = 0; j < width; j++, out += 4, in += 8) {
            out[0] = (in[0] + in[4] + in[row + 0] + in[row + 4]) >> 2;
            out[1] = (in[1] + in[5] + in[row + 1] + in[row + 5]) >> 2;
            out[2] = (in[2] + in[6] + in[row + 2] + in[row + 6]) >> 2;
            out[3] = (in[3] + in[7] + in[row + 3] + in[row + 7]) >> 2;
        }
    }
}

static void R_MipMapNormalHeight(const byte* in, byte* out, int width, int height, qboolean swizzle) {
    int i, j;
    int row;
    int sx = swizzle ? 3 : 0;
    int sa = swizzle ? 0 : 3;

    if (width == 1 && height == 1) {
        return;
    }

    row = width * 4;
    width >>= 1;
    height >>= 1;

    for (i = 0; i < height; i++, in += row) {
        for (j = 0; j < width; j++, out += 4, in += 8) {
            vec3_t v;

            v[0] = OffsetByteToFloat(in[sx]);
            v[1] = OffsetByteToFloat(in[1]);
            v[2] = OffsetByteToFloat(in[2]);

            v[0] += OffsetByteToFloat(in[sx + 4]);
            v[1] += OffsetByteToFloat(in[5]);
            v[2] += OffsetByteToFloat(in[6]);

            v[0] += OffsetByteToFloat(in[sx + row]);
            v[1] += OffsetByteToFloat(in[row + 1]);
            v[2] += OffsetByteToFloat(in[row + 2]);

            v[0] += OffsetByteToFloat(in[sx + row + 4]);
            v[1] += OffsetByteToFloat(in[row + 5]);
            v[2] += OffsetByteToFloat(in[row + 6]);

            VectorNormalizeFast(v);

            out[sx] = FloatToOffsetByte(v[0]);
            out[1] = FloatToOffsetByte(v[1]);
            out[2] = FloatToOffsetByte(v[2]);
            out[sa] = MAX(MAX(in[sa], in[sa + 4]), MAX(in[sa + row], in[sa + row + 4]));
        }
    }
}

static void R_BlendOverTexture(byte* data, int pixelCount, byte blend[4]) {
    int i;
    int inverseAlpha;
    int premult[3];

    inverseAlpha = 255 - blend[3];
    premult[0] = blend[0] * blend[3];
    premult[1] = blend[1] * blend[3];
    premult[2] = blend[2] * blend[3];

    for (i = 0; i < pixelCount; i++, data += 4) {
        data[0] = (data[0] * inverseAlpha + premult[0]) >> 9;
        data[1] = (data[1] * inverseAlpha + premult[1]) >> 9;
        data[2] = (data[2] * inverseAlpha + premult[2]) >> 9;
    }
}

byte mipBlendColors[16][4] = {
    {0, 0, 0, 0},
    {255, 0, 0, 128},
    {0, 255, 0, 128},
    {0, 0, 255, 128},
    {255, 0, 0, 128},
    {0, 255, 0, 128},
    {0, 0, 255, 128},
    {255, 0, 0, 128},
    {0, 255, 0, 128},
    {0, 0, 255, 128},
    {255, 0, 0, 128},
    {0, 255, 0, 128},
    {0, 0, 255, 128},
    {255, 0, 0, 128},
    {0, 255, 0, 128},
    {0, 0, 255, 128},
};

#define ROW_PADDING(width, bpp, alignment) PAD((width) * (bpp), (alignment)) - (width) * (bpp)
void R_ConvertTextureFormat(const byte* in, int width, int height, GLenum format, GLenum type, byte* out) {
    int x, y, rowPadding;
    int unpackAlign = 4;

    if (format == GL_RGB && type == GL_UNSIGNED_BYTE) {
        rowPadding = ROW_PADDING(width, 3, unpackAlign);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                *out++ = *in++;
                *out++ = *in++;
                *out++ = *in++;
                in++;
            }

            out += rowPadding;
        }
    } else if (format == GL_LUMINANCE && type == GL_UNSIGNED_BYTE) {
        rowPadding = ROW_PADDING(width, 1, unpackAlign);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                *out++ = *in++;
                in += 3;
            }

            out += rowPadding;
        }
    } else if (format == GL_LUMINANCE_ALPHA && type == GL_UNSIGNED_BYTE) {
        rowPadding = ROW_PADDING(width, 2, unpackAlign);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                *out++ = *in++;
                in += 2;
                *out++ = *in++;
            }

            out += rowPadding;
        }
    } else if (format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5) {
        rowPadding = ROW_PADDING(width, 2, unpackAlign);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++, in += 4, out += 2) {
                *((unsigned short*)out) = ((unsigned short)(in[0] >> 3) << 11) | ((unsigned short)(in[1] >> 2) << 5) | ((unsigned short)(in[2] >> 3) << 0);
            }

            out += rowPadding;
        }
    } else if (format == GL_RGBA && type == GL_UNSIGNED_SHORT_4_4_4_4) {
        rowPadding = ROW_PADDING(width, 2, unpackAlign);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++, in += 4, out += 2) {
                *((unsigned short*)out) = ((unsigned short)(in[0] >> 4) << 12) | ((unsigned short)(in[1] >> 4) << 8) | ((unsigned short)(in[2] >> 4) << 4) | ((unsigned short)(in[3] >> 4) << 0);
            }

            out += rowPadding;
        }
    } else {
        ri.Error(ERR_DROP, "Unable to convert RGBA image to OpenGL format 0x%X and type 0x%X", format, type);
    }
}

static void RawImage_SwizzleRA(byte* data, int width, int height) {
    int i;
    byte *ptr = data, swap;

    for (i = 0; i < width * height; i++, ptr += 4) {
        swap = ptr[0];
        ptr[0] = ptr[3];
        ptr[3] = swap;
    }
}

static qboolean RawImage_ScaleToPower2(byte** data, int* inout_width, int* inout_height, imgType_t type, imgFlags_t flags, byte** resampledBuffer) {
    int width = *inout_width;
    int height = *inout_height;
    int scaled_width;
    int scaled_height;
    qboolean picmip = flags & IMGFLAG_PICMIP;
    qboolean mipmap = flags & IMGFLAG_MIPMAP;
    qboolean clampToEdge = flags & IMGFLAG_CLAMPTOEDGE;
    qboolean scaled;

    if (!mipmap || glRefConfig.textureNonPowerOfTwo) {
        scaled_width = width;
        scaled_height = height;
    } else {
        scaled_width = NextPowerOfTwo(width);
        scaled_height = NextPowerOfTwo(height);
    }

    if (r_roundImagesDown->integer && scaled_width > width)
        scaled_width >>= 1;
    if (r_roundImagesDown->integer && scaled_height > height)
        scaled_height >>= 1;

    if (picmip && data && resampledBuffer && r_imageUpsample->integer &&
        scaled_width < r_imageUpsampleMaxSize->integer && scaled_height < r_imageUpsampleMaxSize->integer) {
        int finalwidth, finalheight;

        finalwidth = scaled_width << r_imageUpsample->integer;
        finalheight = scaled_height << r_imageUpsample->integer;

        while (finalwidth > r_imageUpsampleMaxSize->integer || finalheight > r_imageUpsampleMaxSize->integer) {
            finalwidth >>= 1;
            finalheight >>= 1;
        }

        while (finalwidth > glConfig.maxTextureSize || finalheight > glConfig.maxTextureSize) {
            finalwidth >>= 1;
            finalheight >>= 1;
        }

        *resampledBuffer = ri.Hunk_AllocateTempMemory(finalwidth * finalheight * 4);

        if (scaled_width != width || scaled_height != height)
            ResampleTexture(*data, width, height, *resampledBuffer, scaled_width, scaled_height);
        else
            Com_Memcpy(*resampledBuffer, *data, width * height * 4);

        if (type == IMGTYPE_COLORALPHA)
            RGBAtoYCoCgA(*resampledBuffer, *resampledBuffer, scaled_width, scaled_height);

        while (scaled_width < finalwidth || scaled_height < finalheight) {
            scaled_width <<= 1;
            scaled_height <<= 1;

            FCBIByBlock(*resampledBuffer, scaled_width, scaled_height, clampToEdge, (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT));
        }

        if (type == IMGTYPE_COLORALPHA)
            YCoCgAtoRGBA(*resampledBuffer, *resampledBuffer, scaled_width, scaled_height);
        else if (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT)
            FillInNormalizedZ(*resampledBuffer, *resampledBuffer, scaled_width, scaled_height);

        *data = *resampledBuffer;
    } else if (scaled_width != width || scaled_height != height) {
        if (data && resampledBuffer) {
            *resampledBuffer = ri.Hunk_AllocateTempMemory(scaled_width * scaled_height * 4);
            ResampleTexture(*data, width, height, *resampledBuffer, scaled_width, scaled_height);
            *data = *resampledBuffer;
        }
    }

    width = scaled_width;
    height = scaled_height;

    if (picmip) {
        scaled_width >>= r_picmip->integer & 31;
        scaled_height >>= r_picmip->integer & 31;
    }

    while (scaled_width > glConfig.maxTextureSize || scaled_height > glConfig.maxTextureSize) {
        scaled_width >>= 1;
        scaled_height >>= 1;
    }

    scaled_width = MAX(1, scaled_width);
    scaled_height = MAX(1, scaled_height);

    scaled = (width != scaled_width) || (height != scaled_height);

    if (data) {
        while (width > scaled_width || height > scaled_height) {
            if (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT)
                R_MipMapNormalHeight(*data, *data, width, height, qfalse);
            else
                R_MipMap(*data, width, height);

            width = MAX(1, width >> 1);
            height = MAX(1, height >> 1);
        }
    }

    *inout_width = width;
    *inout_height = height;

    return scaled;
}

static qboolean RawImage_HasAlpha(const byte* scan, int numPixels) {
    int i;

    if (!scan)
        return qtrue;

    for (i = 0; i < numPixels; i++) {
        if (scan[i * 4 + 3] != 255) {
            return qtrue;
        }
    }

    return qfalse;
}

static GLenum RawImage_GetFormat(const byte* data, int numPixels, GLenum picFormat, qboolean lightMap, imgType_t type, imgFlags_t flags) {
    int samples = 3;
    GLenum internalFormat = GL_RGB;
    qboolean forceNoCompression = (flags & IMGFLAG_NO_COMPRESSION);
    qboolean normalmap = (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT);

    if (picFormat != GL_RGBA8)
        return picFormat;

    if (normalmap) {
        if ((type == IMGTYPE_NORMALHEIGHT) && RawImage_HasAlpha(data, numPixels) && r_parallaxMapping->integer) {
            if (!forceNoCompression && glRefConfig.textureCompression & TCR_BPTC) {
                internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
            } else if (!forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB) {
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            } else if (r_texturebits->integer == 16) {
                internalFormat = GL_RGBA4;
            } else if (r_texturebits->integer == 32) {
                internalFormat = GL_RGBA8;
            } else {
                internalFormat = GL_RGBA;
            }
        } else {
            if (!forceNoCompression && glRefConfig.textureCompression & TCR_RGTC) {
                internalFormat = GL_COMPRESSED_RG_RGTC2;
            } else if (!forceNoCompression && glRefConfig.textureCompression & TCR_BPTC) {
                internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
            } else if (!forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB) {
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            } else if (r_texturebits->integer == 16) {
                internalFormat = GL_RGB5;
            } else if (r_texturebits->integer == 32) {
                internalFormat = GL_RGB8;
            } else {
                internalFormat = GL_RGB;
            }
        }
    } else if (lightMap) {
        if (0 && r_greyscale->integer)
            internalFormat = GL_LUMINANCE;
        else
            internalFormat = GL_RGBA;
    } else {
        if (RawImage_HasAlpha(data, numPixels)) {
            samples = 4;
        }

        if (samples == 3) {
            if (0 && r_greyscale->integer) {
                if (r_texturebits->integer == 16 || r_texturebits->integer == 32)
                    internalFormat = GL_LUMINANCE8;
                else
                    internalFormat = GL_LUMINANCE;
            } else {
                if (!forceNoCompression && (glRefConfig.textureCompression & TCR_BPTC)) {
                    internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
                } else if (!forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB) {
                    internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                } else if (!forceNoCompression && glConfig.textureCompression == TC_S3TC) {
                    internalFormat = GL_RGB4_S3TC;
                } else if (r_texturebits->integer == 16 && !forceNoCompression) {
                    internalFormat = GL_RGB5;
                } else if (r_texturebits->integer == 32 || forceNoCompression) {
                    internalFormat = GL_RGB8;
                } else {
                    internalFormat = GL_RGB;
                }
            }
        } else if (samples == 4) {
            if (0 && r_greyscale->integer) {
                if (r_texturebits->integer == 16 || r_texturebits->integer == 32)
                    internalFormat = GL_LUMINANCE8_ALPHA8;
                else
                    internalFormat = GL_LUMINANCE_ALPHA;
            } else {
                if (!forceNoCompression && (glRefConfig.textureCompression & TCR_BPTC)) {
                    internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
                } else if (!forceNoCompression && glConfig.textureCompression == TC_S3TC_ARB) {
                    internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                } else if (r_texturebits->integer == 16 && !forceNoCompression) {
                    internalFormat = GL_RGBA4;
                } else if (r_texturebits->integer == 32 || forceNoCompression) {
                    internalFormat = GL_RGBA8;
                } else {
                    internalFormat = GL_RGBA;
                }
            }
        }
    }

    return internalFormat;
}

static void CompressMonoBlock(byte outdata[8], const byte indata[16]) {
    int hi, lo, diff, bias, outbyte, shift, i;
    byte* p = outdata;

    hi = lo = indata[0];
    for (i = 1; i < 16; i++) {
        hi = MAX(indata[i], hi);
        lo = MIN(indata[i], lo);
    }

    *p++ = hi;
    *p++ = lo;

    diff = hi - lo;

    if (diff == 0) {
        outbyte = (hi == 255) ? 255 : 0;

        for (i = 0; i < 6; i++)
            *p++ = outbyte;

        return;
    }

    bias = diff / 2 - lo * 7;
    outbyte = shift = 0;
    for (i = 0; i < 16; i++) {
        const byte fixIndex[8] = {1, 7, 6, 5, 4, 3, 2, 0};
        byte index = fixIndex[(indata[i] * 7 + bias) / diff];

        outbyte |= index << shift;
        shift += 3;
        if (shift >= 8) {
            *p++ = outbyte & 0xff;
            shift -= 8;
            outbyte >>= 8;
        }
    }
}

static void RawImage_UploadToRgtc2Texture(GLuint texture, int miplevel, int x, int y, int width, int height, byte* data) {
    int wBlocks, hBlocks, iy, ix, size;
    byte *compressedData, *p;

    wBlocks = (width + 3) / 4;
    hBlocks = (height + 3) / 4;
    size = wBlocks * hBlocks * 16;

    p = compressedData = ri.Hunk_AllocateTempMemory(size);
    for (iy = 0; iy < height; iy += 4) {
        int oh = MIN(4, height - iy);

        for (ix = 0; ix < width; ix += 4) {
            byte workingData[16];
            int component;

            int ow = MIN(4, width - ix);

            for (component = 0; component < 2; component++) {
                int ox, oy;

                for (oy = 0; oy < oh; oy++)
                    for (ox = 0; ox < ow; ox++)
                        workingData[oy * 4 + ox] = data[((iy + oy) * width + ix + ox) * 4 + component];

                for (oy = 0; oy < 4; oy++)
                    for (ox = (oy < oh) ? ow : 0; ox < 4; ox++)
                        workingData[oy * 4 + ox] = workingData[(oy % oh) * 4 + ox % ow];

                CompressMonoBlock(p, workingData);
                p += 8;
            }
        }
    }

    qglCompressedTextureSubImage2DEXT(texture, GL_TEXTURE_2D, miplevel, x, y, width, height, GL_COMPRESSED_RG_RGTC2, size, compressedData);

    ri.Hunk_FreeTempMemory(compressedData);
}

static int CalculateMipSize(int width, int height, GLenum picFormat) {
    int numBlocks = ((width + 3) / 4) * ((height + 3) / 4);
    int numPixels = width * height;

    switch (picFormat) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
            return numBlocks * 8;

        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
        case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
            return numBlocks * 16;

        case GL_RGBA8:
        case GL_SRGB8_ALPHA8_EXT:
            return numPixels * 4;

        case GL_RGBA16:
            return numPixels * 8;

        default:
            ri.Printf(PRINT_ALL, "Unsupported texture format %08x\n", picFormat);
            return 0;
    }

    return 0;
}

static GLenum PixelDataFormatFromInternalFormat(GLenum internalFormat) {
    switch (internalFormat) {
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_COMPONENT16_ARB:
        case GL_DEPTH_COMPONENT24_ARB:
        case GL_DEPTH_COMPONENT32_ARB:
            return GL_DEPTH_COMPONENT;
        case GL_R8:
        case GL_RED:
            return GL_RED;
        default:
            return GL_RGBA;
            break;
    }
}

static void RawImage_UploadTexture(GLuint texture, byte* data, int x, int y, int width, int height, GLenum target, GLenum picFormat, GLenum dataFormat, GLenum dataType, int numMips, GLenum internalFormat, imgType_t type, imgFlags_t flags, qboolean subtexture) {
    qboolean rgtc = internalFormat == GL_COMPRESSED_RG_RGTC2;
    qboolean rgba8 = picFormat == GL_RGBA8 || picFormat == GL_SRGB8_ALPHA8_EXT;
    qboolean rgba = rgba8 || picFormat == GL_RGBA16;
    qboolean mipmap = !!(flags & IMGFLAG_MIPMAP);
    int size, miplevel;
    qboolean lastMip = qfalse;
    byte* formatBuffer = NULL;

    if (qglesMajorVersion && rgba8 && (dataFormat != GL_RGBA || dataType != GL_UNSIGNED_BYTE)) {
        formatBuffer = ri.Hunk_AllocateTempMemory(4 * width * height);
    }

    miplevel = 0;
    do {
        lastMip = (width == 1 && height == 1) || !mipmap;
        size = CalculateMipSize(width, height, picFormat);

        if (!rgba) {
            qglCompressedTextureSubImage2DEXT(texture, target, miplevel, x, y, width, height, picFormat, size, data);
        } else {
            if (rgba8 && miplevel != 0 && r_colorMipLevels->integer)
                R_BlendOverTexture((byte*)data, width * height, mipBlendColors[miplevel]);

            if (rgba8 && rgtc)
                RawImage_UploadToRgtc2Texture(texture, miplevel, x, y, width, height, data);
            else if (formatBuffer) {
                R_ConvertTextureFormat(data, width, height, dataFormat, dataType, formatBuffer);
                qglTextureSubImage2DEXT(texture, target, miplevel, x, y, width, height, dataFormat, dataType, formatBuffer);
            } else
                qglTextureSubImage2DEXT(texture, target, miplevel, x, y, width, height, dataFormat, dataType, data);
        }

        if (!lastMip && numMips < 2) {
            if (rgba8 && type != IMGTYPE_NORMAL && type != IMGTYPE_NORMALHEIGHT) {
                R_MipMap(data, width, height);
            } else if (glRefConfig.framebufferObject) {
                qglGenerateTextureMipmapEXT(texture, target);
                break;
            } else if (rgba8) {
                R_MipMapNormalHeight(data, data, width, height, glRefConfig.swizzleNormalmap);
            }
        }

        x >>= 1;
        y >>= 1;
        width = MAX(1, width >> 1);
        height = MAX(1, height >> 1);
        miplevel++;

        if (numMips > 1) {
            data += size;
            numMips--;
        }
    } while (!lastMip);

    if (formatBuffer != NULL)
        ri.Hunk_FreeTempMemory(formatBuffer);
}

static void Upload32(byte* data, int x, int y, int width, int height, GLenum picFormat, GLenum dataFormat, GLenum dataType, int numMips, image_t* image, qboolean scaled) {
    int i, c;
    byte* scan;

    imgType_t type = image->type;
    imgFlags_t flags = image->flags;
    GLenum internalFormat = image->internalFormat;
    qboolean rgba8 = picFormat == GL_RGBA8 || picFormat == GL_SRGB8_ALPHA8_EXT;
    qboolean mipmap = !!(flags & IMGFLAG_MIPMAP) && (rgba8 || numMips > 1);
    qboolean cubemap = !!(flags & IMGFLAG_CUBEMAP);

    if (rgba8 && !cubemap) {
        c = width * height;
        scan = data;

        if (type == IMGTYPE_COLORALPHA) {
            if (r_greyscale->integer) {
                for (i = 0; i < c; i++) {
                    byte luma = LUMA(scan[i * 4], scan[i * 4 + 1], scan[i * 4 + 2]);
                    scan[i * 4] = luma;
                    scan[i * 4 + 1] = luma;
                    scan[i * 4 + 2] = luma;
                }
            } else if (r_greyscale->value) {
                for (i = 0; i < c; i++) {
                    float luma = LUMA(scan[i * 4], scan[i * 4 + 1], scan[i * 4 + 2]);
                    scan[i * 4] = LERP(scan[i * 4], luma, r_greyscale->value);
                    scan[i * 4 + 1] = LERP(scan[i * 4 + 1], luma, r_greyscale->value);
                    scan[i * 4 + 2] = LERP(scan[i * 4 + 2], luma, r_greyscale->value);
                }
            }

            if (!(flags & IMGFLAG_NOLIGHTSCALE) && (scaled || mipmap))
                R_LightScaleTexture(data, width, height, !mipmap);
        }

        if (glRefConfig.swizzleNormalmap && (type == IMGTYPE_NORMAL || type == IMGTYPE_NORMALHEIGHT))
            RawImage_SwizzleRA(data, width, height);
    }

    if (cubemap) {
        for (i = 0; i < 6; i++) {
            int w2 = width, h2 = height;
            RawImage_UploadTexture(image->texnum, data, x, y, width, height, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, picFormat, dataFormat, dataType, numMips, internalFormat, type, flags, qfalse);
            for (c = numMips; c; c--) {
                data += CalculateMipSize(w2, h2, picFormat);
                w2 = MAX(1, w2 >> 1);
                h2 = MAX(1, h2 >> 1);
            }
        }
    } else {
        RawImage_UploadTexture(image->texnum, data, x, y, width, height, GL_TEXTURE_2D, picFormat, dataFormat, dataType, numMips, internalFormat, type, flags, qfalse);
    }

    GL_CheckErrors();
}

image_t* R_CreateImage2(const char* name, byte* pic, int width, int height, GLenum picFormat, int numMips, imgType_t type, imgFlags_t flags, int internalFormat) {
    byte* resampledBuffer = NULL;
    image_t* image;
    qboolean isLightmap = qfalse, scaled = qfalse;
    long hash;
    int glWrapClampMode, mipWidth, mipHeight, miplevel;
    qboolean rgba8 = picFormat == GL_RGBA8 || picFormat == GL_SRGB8_ALPHA8_EXT;
    qboolean mipmap = !!(flags & IMGFLAG_MIPMAP);
    qboolean cubemap = !!(flags & IMGFLAG_CUBEMAP);
    qboolean picmip = !!(flags & IMGFLAG_PICMIP);
    qboolean lastMip;
    GLenum textureTarget = cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    GLenum dataFormat, dataType;

    if (strlen(name) >= MAX_QPATH) {
        ri.Error(ERR_DROP, "R_CreateImage: \"%s\" is too long", name);
    }
    if (!strncmp(name, "*lightmap", 9)) {
        isLightmap = qtrue;
    }
    if (!Q_stricmpn(name, "webui/", 6)) {
        flags |= IMGFLAG_NO_COMPRESSION;
    }

    if (tr.numImages == MAX_DRAWIMAGES) {
        ri.Error(ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit");
    }

    image = tr.images[tr.numImages] = ri.Hunk_Alloc(sizeof(image_t), h_low);
    qglGenTextures(1, &image->texnum);
    tr.numImages++;

    image->type = type;
    image->flags = flags;

    strcpy(image->imgName, name);

    image->width = width;
    image->height = height;
    if (flags & IMGFLAG_CLAMPTOEDGE)
        glWrapClampMode = GL_CLAMP_TO_EDGE;
    else
        glWrapClampMode = GL_REPEAT;

    if (!internalFormat)
        internalFormat = RawImage_GetFormat(pic, width * height, picFormat, isLightmap, image->type, image->flags);

    dataFormat = PixelDataFormatFromInternalFormat(internalFormat);
    dataType = picFormat == GL_RGBA16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

    if (qglesMajorVersion) {
        switch (internalFormat) {
            case GL_LUMINANCE:
            case GL_LUMINANCE8:
                internalFormat = GL_LUMINANCE;
                dataFormat = GL_LUMINANCE;
                dataType = GL_UNSIGNED_BYTE;
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_LUMINANCE8_ALPHA8:
                internalFormat = GL_LUMINANCE_ALPHA;
                dataFormat = GL_LUMINANCE_ALPHA;
                dataType = GL_UNSIGNED_BYTE;
                break;
            case GL_RGB:
            case GL_RGB8:
                internalFormat = GL_RGB;
                dataFormat = GL_RGB;
                dataType = GL_UNSIGNED_BYTE;
                break;
            case GL_RGB5:
                internalFormat = GL_RGB;
                dataFormat = GL_RGB;
                dataType = GL_UNSIGNED_SHORT_5_6_5;
                break;
            case GL_RGBA:
            case GL_RGBA8:
                internalFormat = GL_RGBA;
                dataFormat = GL_RGBA;
                dataType = GL_UNSIGNED_BYTE;
                break;
            case GL_RGBA4:
                internalFormat = GL_RGBA;
                dataFormat = GL_RGBA;
                dataType = GL_UNSIGNED_SHORT_4_4_4_4;
                break;
            case GL_R8:
            case GL_RED:
                internalFormat = GL_R8;
                dataFormat = GL_RED;
                dataType = GL_UNSIGNED_BYTE;
                break;
            default:
                ri.Error(ERR_DROP, "Missing OpenGL ES support for image '%s' with internal format 0x%X\n", name, internalFormat);
        }
    }

    image->internalFormat = internalFormat;

    if (!cubemap) {
        if (rgba8)
            scaled = RawImage_ScaleToPower2(&pic, &width, &height, type, flags, &resampledBuffer);
        else if (pic && picmip) {
            for (miplevel = r_picmip->integer; miplevel > 0 && numMips > 1; miplevel--, numMips--) {
                int size = CalculateMipSize(width, height, picFormat);
                width = MAX(1, width >> 1);
                height = MAX(1, height >> 1);
                pic += size;
            }
        }
    }

    image->uploadWidth = width;
    image->uploadHeight = height;

    mipWidth = width;
    mipHeight = height;
    miplevel = 0;
    do {
        lastMip = !mipmap || (mipWidth == 1 && mipHeight == 1);
        if (cubemap) {
            int i;

            for (i = 0; i < 6; i++)
                qglTextureImage2DEXT(image->texnum, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, miplevel, internalFormat, mipWidth, mipHeight, 0, dataFormat, dataType, NULL);
        } else {
            qglTextureImage2DEXT(image->texnum, GL_TEXTURE_2D, miplevel, internalFormat, mipWidth, mipHeight, 0, dataFormat, dataType, NULL);
        }

        mipWidth = MAX(1, mipWidth >> 1);
        mipHeight = MAX(1, mipHeight >> 1);
        miplevel++;
    } while (!lastMip);

    if (pic)
        Upload32(pic, 0, 0, width, height, picFormat, dataFormat, dataType, numMips, image, scaled);

    if (resampledBuffer != NULL)
        ri.Hunk_FreeTempMemory(resampledBuffer);

    qglTextureParameterfEXT(image->texnum, textureTarget, GL_TEXTURE_WRAP_S, glWrapClampMode);
    qglTextureParameterfEXT(image->texnum, textureTarget, GL_TEXTURE_WRAP_T, glWrapClampMode);

    if (cubemap)
        qglTextureParameteriEXT(image->texnum, textureTarget, GL_TEXTURE_WRAP_R, glWrapClampMode);

    if (textureFilterAnisotropic && !cubemap)
        qglTextureParameteriEXT(image->texnum, textureTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                mipmap ? (GLint)Com_Clamp(1, maxAnisotropy, r_ext_max_anisotropy->integer) : 1);

    switch (internalFormat) {
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_COMPONENT16_ARB:
        case GL_DEPTH_COMPONENT24_ARB:
        case GL_DEPTH_COMPONENT32_ARB:
            if (!QGL_VERSION_ATLEAST(3, 0)) {
                qglTextureParameterfEXT(image->texnum, textureTarget, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
            }
            qglTextureParameterfEXT(image->texnum, textureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            qglTextureParameterfEXT(image->texnum, textureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        default:
            qglTextureParameterfEXT(image->texnum, textureTarget, GL_TEXTURE_MIN_FILTER, mipmap ? gl_filter_min : GL_LINEAR);
            qglTextureParameterfEXT(image->texnum, textureTarget, GL_TEXTURE_MAG_FILTER, mipmap ? gl_filter_max : GL_LINEAR);
            break;
    }

    GL_CheckErrors();

    hash = generateHashValue(name);
    image->next = hashTable[hash];
    hashTable[hash] = image;

    return image;
}

image_t* R_CreateImage(const char* name, byte* pic, int width, int height, imgType_t type, imgFlags_t flags, int internalFormat) {
    int64_t uploadStart = ri.Microseconds();
    image_t* uploaded = R_CreateImage2(name, pic, width, height, GL_RGBA8, 0, type, flags, internalFormat);

    r_imageUploadUs += ri.Microseconds() - uploadStart;
    return uploaded;
}

void R_UpdateSubImage(image_t* image, byte* pic, int x, int y, int width, int height, GLenum picFormat) {
    GLenum dataFormat, dataType;

    dataFormat = PixelDataFormatFromInternalFormat(image->internalFormat);
    dataType = picFormat == GL_RGBA16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

    Upload32(pic, x, y, width, height, picFormat, dataFormat, dataType, 0, image, qfalse);
}

void R_LoadDDS(const char* filename, byte** pic, int* width, int* height, GLenum* picFormat, int* numMips);

typedef struct
{
    char* ext;
    void (*ImageLoader)(const char*, unsigned char**, int*, int*);
} imageExtToLoaderMap_t;

static imageExtToLoaderMap_t imageLoaders[] =
    {
        {"png", R_LoadPNG},
        {"tga", R_LoadTGA},
        {"jpg", R_LoadJPG},
        {"jpeg", R_LoadJPG},
        {"pcx", R_LoadPCX},
        {"bmp", R_LoadBMP}};

static int numImageLoaders = ARRAY_LEN(imageLoaders);

void R_LoadImage(const char* name, byte** pic, int* width, int* height, GLenum* picFormat, int* numMips) {
    qboolean orgNameFailed = qfalse;
    int orgLoader = -1;
    int i;
    char localName[MAX_QPATH];
    const char* ext;
    char* altName;

    *pic = NULL;
    *width = 0;
    *height = 0;
    *picFormat = GL_RGBA8;
    *numMips = 0;

    Q_strncpyz(localName, name, MAX_QPATH);

    ext = COM_GetExtension(localName);

    if (r_ext_compressed_textures->integer) {
        char ddsName[MAX_QPATH];

        COM_StripExtension(name, ddsName, MAX_QPATH);
        Q_strcat(ddsName, MAX_QPATH, ".dds");

        R_LoadDDS(ddsName, pic, width, height, picFormat, numMips);

        if (*pic)
            return;
    }

    if (*ext) {
        for (i = 0; i < numImageLoaders; i++) {
            if (!Q_stricmp(ext, imageLoaders[i].ext)) {
                imageLoaders[i].ImageLoader(localName, pic, width, height);
                break;
            }
        }

        if (i < numImageLoaders) {
            if (*pic == NULL) {
                orgNameFailed = qtrue;
                orgLoader = i;
                COM_StripExtension(name, localName, MAX_QPATH);
            } else {
                return;
            }
        }
    }

    for (i = 0; i < numImageLoaders; i++) {
        if (i == orgLoader)
            continue;

        altName = va("%s.%s", localName, imageLoaders[i].ext);

        imageLoaders[i].ImageLoader(altName, pic, width, height);

        if (*pic) {
            if (orgNameFailed) {
                ri.Printf(PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
                          name, altName);
            }

            break;
        }
    }
}

int64_t r_imageDecodeUs, r_imageUploadUs, r_imageReadUs;
int r_imageCount;

void R_ImageLoadStatsReset(void) {
    r_shaderFindUs = r_shaderCachedUs = 0;
    r_shaderFindCount = r_shaderCachedCount = 0;
    r_imageDecodeUs = 0;
    r_imageUploadUs = 0;
    r_imageReadUs = 0;
    r_imageCount = 0;
}

void R_ImageLoadStatsReport(void) {
    if (!r_imageCount) {
        return;
    }
    ri.Printf(PRINT_ALL, "shaders: %i asked, %.2f s of which %.2f s is their images\n",
              r_shaderFindCount, r_shaderFindUs / 1000000.0f,
              (r_imageDecodeUs + r_imageUploadUs) / 1000000.0f);
    ri.Printf(PRINT_ALL, "  %i were already loaded and cost %.2f s just to look up\n",
              r_shaderCachedCount, r_shaderCachedUs / 1000000.0f);
    ri.Printf(PRINT_ALL, "images: %i loaded, %.2f s reading, %.2f s decoding, %.2f s uploading\n",
              r_imageCount, r_imageReadUs / 1000000.0f,
              (r_imageDecodeUs - r_imageReadUs) / 1000000.0f, r_imageUploadUs / 1000000.0f);
}

#define R_PREFETCH_MAX 2048
#define R_PREFETCH_AHEAD 16

typedef enum { PF_EMPTY, PF_WORKING, PF_READY, PF_FAILED } prefetchState_t;

typedef struct {
    char name[MAX_QPATH];
    void* file;
    int fileLen;
    byte* pixels;
    int width, height;
    volatile int state;
} prefetchEntry_t;

static prefetchEntry_t r_prefetch[R_PREFETCH_MAX];
static int r_prefetchCount;
static int r_prefetchNext;
static int r_prefetchTaken;
static qboolean r_prefetchOn;
void R_PrefetchFinish(void);
static int r_prefetchHits, r_prefetchMisses;
static int64_t r_prefetchWaitUs;

static char r_loadedNames[R_PREFETCH_MAX][MAX_QPATH];
static int r_loadedCount;
static char r_prefetchList[MAX_QPATH];

static void R_PrefetchFile(const char* list, char* out, int size) {
    char base[MAX_QPATH];

    COM_StripExtension(list, base, sizeof(base));
    Com_sprintf(out, size, "prefetch/%s.txt", COM_SkipPath(base));
}

static qboolean R_PrefetchSameName(const char* wanted, const char* entry) {
    char a[MAX_QPATH], b[MAX_QPATH];

    COM_StripExtension(wanted, a, sizeof(a));
    COM_StripExtension(entry, b, sizeof(b));
    return (qboolean)(!Q_stricmp(a, b));
}

static void R_PrefetchDecode(void* data) {
    prefetchEntry_t* entry = (prefetchEntry_t*)data;
    byte* pixels = NULL;
    int width = 0, height = 0;

    if (!Q_stricmp(COM_GetExtension(entry->name), "png")) {
        R_DecodePNG((byte*)entry->file, entry->fileLen, entry->name, &pixels, &width, &height);
    } else if (!Q_stricmp(COM_GetExtension(entry->name), "tga")) {
        R_DecodeTGA((byte*)entry->file, entry->fileLen, entry->name, &pixels, &width, &height, malloc,
                    free, qtrue);
    } else {
        R_DecodeJPG((byte*)entry->file, entry->fileLen, &pixels, &width, &height, malloc, qtrue);
    }
    entry->pixels = pixels;
    entry->width = width;
    entry->height = height;
    entry->state = pixels ? PF_READY : PF_FAILED;
}

static void R_PrefetchPump(void) {
    while (r_prefetchNext < r_prefetchCount && r_prefetchNext - r_prefetchTaken < R_PREFETCH_AHEAD) {
        prefetchEntry_t* entry = &r_prefetch[r_prefetchNext++];
        void* buffer = NULL;
        int len = ri.FS_ReadFile(entry->name, &buffer);

        if (len <= 0 || !buffer) {
            if (buffer) {
                ri.FS_FreeFile(buffer);
            }
            entry->state = PF_FAILED;
            continue;
        }
        entry->file = malloc(len);
        if (entry->file) {
            Com_Memcpy(entry->file, buffer, len);
        }
        ri.FS_FreeFile(buffer);
        if (!entry->file) {
            entry->state = PF_FAILED;
            continue;
        }
        entry->fileLen = len;
        entry->state = PF_WORKING;
        ri.JobsSubmit(R_PrefetchDecode, entry);
    }
}

static void R_PrefetchRelease(prefetchEntry_t* entry) {
    if (entry->file) {
        free(entry->file);
        entry->file = NULL;
    }
    if (entry->pixels) {
        free(entry->pixels);
        entry->pixels = NULL;
    }
    entry->state = PF_EMPTY;
    while (r_prefetchTaken < r_prefetchNext && r_prefetch[r_prefetchTaken].state == PF_EMPTY) {
        r_prefetchTaken++;
    }
}

static qboolean R_PrefetchTake(const char* name, byte** pic, int* width, int* height) {
    int i;

    if (!r_prefetchOn) {
        return qfalse;
    }
    R_PrefetchPump();

    for (i = r_prefetchTaken; i < r_prefetchNext; i++) {
        prefetchEntry_t* entry = &r_prefetch[i];
        int bytes;

        if (entry->state == PF_EMPTY || !R_PrefetchSameName(name, entry->name)) {
            continue;
        }
        if (entry->state == PF_WORKING) {
            int64_t waitStart = ri.Microseconds();

            while (entry->state == PF_WORKING) {
                ri.ThreadYield();
            }
            r_prefetchWaitUs += ri.Microseconds() - waitStart;
        }
        if (entry->state != PF_READY) {
            R_PrefetchRelease(entry);
            return qfalse;
        }
        bytes = entry->width * entry->height * 4;
        Q_strncpyz(r_lastImageName, entry->name, sizeof(r_lastImageName));
        r_prefetchHits++;
        *pic = (byte*)ri.Malloc(bytes);
        Com_Memcpy(*pic, entry->pixels, bytes);
        *width = entry->width;
        *height = entry->height;
        R_PrefetchRelease(entry);
        R_PrefetchPump();
        return qtrue;
    }
    r_prefetchMisses++;
    return qfalse;
}

void R_PrefetchStart(const char* listName) {
    char fileName[MAX_QPATH];
    char* text;
    char* line;
    int len;

    R_PrefetchFinish();

    r_prefetchCount = r_prefetchNext = r_prefetchTaken = 0;
    r_prefetchHits = r_prefetchMisses = 0;
    r_prefetchWaitUs = 0;
    r_loadedCount = 0;
    r_prefetchOn = qfalse;
    Q_strncpyz(r_prefetchList, listName, sizeof(r_prefetchList));

    if (!ri.JobWorkers() || !r_prefetchTextures || !r_prefetchTextures->integer) {
        return;
    }
    R_PrefetchFile(listName, fileName, sizeof(fileName));
    len = ri.FS_ReadFile(fileName, (void**)&text);
    if (len <= 0 || !text) {
        return;
    }
    for (line = strtok(text, "\r\n"); line && r_prefetchCount < R_PREFETCH_MAX;
         line = strtok(NULL, "\r\n")) {
        if (*line) {
            Q_strncpyz(r_prefetch[r_prefetchCount].name, line, MAX_QPATH);
            r_prefetch[r_prefetchCount].state = PF_EMPTY;
            r_prefetchCount++;
        }
    }
    ri.FS_FreeFile(text);

    if (r_prefetchCount) {
        r_prefetchOn = qtrue;
        R_PrefetchPump();
    }
}

static void R_PrefetchRecord(void) {
    if (r_loadedCount < R_PREFETCH_MAX && r_lastImageName[0]) {
        Q_strncpyz(r_loadedNames[r_loadedCount++], r_lastImageName, MAX_QPATH);
    }
    r_lastImageName[0] = '\0';
}

void R_PrefetchFinish(void) {
    char fileName[MAX_QPATH];
    char* text;
    int size = 0;
    int i;

    if (r_prefetchCount || r_prefetchHits) {
        ri.Printf(PRINT_ALL, "prefetch: %i ready, %i not listed, %i listed, %.2f s waiting\n",
                  r_prefetchHits, r_prefetchMisses, r_prefetchCount, r_prefetchWaitUs / 1000000.0f);
    }

    while (r_prefetchTaken < r_prefetchNext) {
        prefetchEntry_t* entry = &r_prefetch[r_prefetchTaken];

        while (entry->state == PF_WORKING) {
            ri.ThreadYield();
        }
        R_PrefetchRelease(entry);
        if (r_prefetch[r_prefetchTaken].state != PF_EMPTY) {
            r_prefetchTaken++;
        }
    }
    r_prefetchOn = qfalse;

    if (!r_loadedCount || !r_prefetchList[0] || !r_prefetchTextures || !r_prefetchTextures->integer) {
        return;
    }
    for (i = 0; i < r_loadedCount; i++) {
        size += (int)strlen(r_loadedNames[i]) + 1;
    }
    text = (char*)ri.Malloc(size + 1);
    text[0] = '\0';
    for (i = 0; i < r_loadedCount; i++) {
        Q_strcat(text, size + 1, r_loadedNames[i]);
        Q_strcat(text, size + 1, "\n");
    }
    R_PrefetchFile(r_prefetchList, fileName, sizeof(fileName));
    ri.FS_WriteFile(fileName, text, (int)strlen(text));
    ri.Free(text);
    r_loadedCount = 0;
    r_prefetchList[0] = '\0';
}

image_t* R_FindImageFile(const char* name, imgType_t type, imgFlags_t flags) {
    image_t* image;
    int width, height;
    byte* pic;
    GLenum picFormat;
    int picNumMips;
    long hash;
    imgFlags_t checkFlagsTrue, checkFlagsFalse;

    if (!name) {
        return NULL;
    }

    hash = generateHashValue(name);

    for (image = hashTable[hash]; image; image = image->next) {
        if (!strcmp(name, image->imgName)) {
            if (strcmp(name, "*white")) {
                if (image->flags != flags) {
                    ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name, image->flags, flags);
                }
            }
            return image;
        }
    }

    {
        int64_t start = ri.Microseconds();

        if (R_PrefetchTake(name, &pic, &width, &height)) {
            picFormat = GL_RGBA8;
            picNumMips = 0;
        } else {
            R_LoadImage(name, &pic, &width, &height, &picFormat, &picNumMips);
        }
        r_imageDecodeUs += ri.Microseconds() - start;
    }
    if (pic == NULL) {
        return NULL;
    }
    R_PrefetchRecord();
    r_imageCount++;

    checkFlagsTrue = IMGFLAG_PICMIP | IMGFLAG_MIPMAP | IMGFLAG_GENNORMALMAP;
    checkFlagsFalse = IMGFLAG_CUBEMAP;
    if (r_normalMapping->integer && (picFormat == GL_RGBA8) && (type == IMGTYPE_COLORALPHA) &&
        ((flags & checkFlagsTrue) == checkFlagsTrue) && !(flags & checkFlagsFalse)) {
        char normalName[MAX_QPATH];
        image_t* normalImage;
        int normalWidth, normalHeight;
        imgFlags_t normalFlags;

        normalFlags = (flags & ~IMGFLAG_GENNORMALMAP) | IMGFLAG_NOLIGHTSCALE;

        COM_StripExtension(name, normalName, MAX_QPATH);
        Q_strcat(normalName, MAX_QPATH, "_n");

        normalImage = R_FindImageFile(normalName, IMGTYPE_NORMAL, normalFlags);

        if (normalImage == NULL) {
            byte* normalPic;
            int x, y;

            normalWidth = width;
            normalHeight = height;
            normalPic = ri.Malloc(width * height * 4);
            RGBAtoNormal(pic, normalPic, width, height, flags & IMGFLAG_CLAMPTOEDGE);

#if 1
            RGBAtoYCoCgA(pic, pic, width, height);
            for (y = 0; y < height; y++) {
                byte* picbyte = pic + y * width * 4;
                byte* normbyte = normalPic + y * width * 4;
                for (x = 0; x < width; x++) {
                    int div = MAX(normbyte[2] - 127, 16);
                    picbyte[0] = CLAMP(picbyte[0] * 128 / div, 0, 255);
                    picbyte += 4;
                    normbyte += 4;
                }
            }
            YCoCgAtoRGBA(pic, pic, width, height);
#else
            {
                byte* blurPic;

                RGBAtoYCoCgA(pic, pic, width, height);
                blurPic = ri.Malloc(width * height);

                for (y = 1; y < height - 1; y++) {
                    byte* picbyte = pic + y * width * 4;
                    byte* blurbyte = blurPic + y * width;

                    picbyte += 4;
                    blurbyte += 1;

                    for (x = 1; x < width - 1; x++) {
                        int result;

                        result = *(picbyte - (width + 1) * 4) + *(picbyte - width * 4) + *(picbyte - (width - 1) * 4) +
                                 *(picbyte - 1 * 4) + *(picbyte) + *(picbyte + 1 * 4) +
                                 *(picbyte + (width - 1) * 4) + *(picbyte + width * 4) + *(picbyte + (width + 1) * 4);

                        result /= 9;

                        *blurbyte = result;
                        picbyte += 4;
                        blurbyte += 1;
                    }
                }

                for (y = 1; y < height - 1; y++) {
                    byte* picbyte = pic + y * width * 4;
                    byte* blurbyte = blurPic + y * width;

                    picbyte += 4;
                    blurbyte += 1;

                    for (x = 1; x < width - 1; x++) {
                        picbyte[0] = *blurbyte;
                        picbyte += 4;
                        blurbyte += 1;
                    }
                }

                ri.Free(blurPic);

                YCoCgAtoRGBA(pic, pic, width, height);
            }
#endif

            R_CreateImage(normalName, normalPic, normalWidth, normalHeight, IMGTYPE_NORMAL, normalFlags, 0);
            ri.Free(normalPic);
        }
    }

    if ((flags & IMGFLAG_MIPMAP) && picFormat != GL_RGBA8 && picFormat != GL_SRGB8_ALPHA8_EXT) {
        int wh = MAX(width, height);
        int neededMips = 0;
        while (wh) {
            neededMips++;
            wh >>= 1;
        }
        if (neededMips > picNumMips)
            flags &= ~IMGFLAG_MIPMAP;
    }

    image = R_CreateImage2((char*)name, pic, width, height, picFormat, picNumMips, type, flags, 0);
    ri.Free(pic);
    return image;
}

#define DLIGHT_SIZE 16
static void R_CreateDlightImage(void) {
    int x, y;
    byte data[DLIGHT_SIZE][DLIGHT_SIZE][4];
    int b;

    for (x = 0; x < DLIGHT_SIZE; x++) {
        for (y = 0; y < DLIGHT_SIZE; y++) {
            float d;

            d = (DLIGHT_SIZE / 2 - 0.5f - x) * (DLIGHT_SIZE / 2 - 0.5f - x) +
                (DLIGHT_SIZE / 2 - 0.5f - y) * (DLIGHT_SIZE / 2 - 0.5f - y);
            b = 4000 / d;
            if (b > 255) {
                b = 255;
            } else if (b < 75) {
                b = 0;
            }
            data[y][x][0] =
                data[y][x][1] =
                    data[y][x][2] = b;
            data[y][x][3] = 255;
        }
    }
    tr.dlightImage = R_CreateImage("*dlight", (byte*)data, DLIGHT_SIZE, DLIGHT_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0);
}

void R_InitFogTable(void) {
    int i;
    float d;
    float exp;

    exp = 0.5;

    for (i = 0; i < FOG_TABLE_SIZE; i++) {
        d = pow((float)i / (FOG_TABLE_SIZE - 1), exp);

        tr.fogTable[i] = d;
    }
}

float R_FogFactor(float s, float t) {
    float d;

    s -= 1.0 / 512;
    if (s < 0) {
        return 0;
    }
    if (t < 1.0 / 32) {
        return 0;
    }
    if (t < 31.0 / 32) {
        s *= (t - 1.0f / 32.0f) / (30.0f / 32.0f);
    }

    s *= 8;

    if (s > 1.0) {
        s = 1.0;
    }

    d = tr.fogTable[(int)(s * (FOG_TABLE_SIZE - 1))];

    return d;
}

#define FOG_S 256
#define FOG_T 32
static void R_CreateFogImage(void) {
    int x, y;
    byte* data;
    float d;

    data = ri.Hunk_AllocateTempMemory(FOG_S * FOG_T * 4);

    for (x = 0; x < FOG_S; x++) {
        for (y = 0; y < FOG_T; y++) {
            d = R_FogFactor((x + 0.5f) / FOG_S, (y + 0.5f) / FOG_T);

            data[(y * FOG_S + x) * 4 + 0] =
                data[(y * FOG_S + x) * 4 + 1] =
                    data[(y * FOG_S + x) * 4 + 2] = 255;
            data[(y * FOG_S + x) * 4 + 3] = 255 * d;
        }
    }
    tr.fogImage = R_CreateImage("*fog", (byte*)data, FOG_S, FOG_T, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0);
    ri.Hunk_FreeTempMemory(data);
}

#define DEFAULT_SIZE 16
static void R_CreateDefaultImage(void) {
    int x;
    byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

    Com_Memset(data, 32, sizeof(data));
    for (x = 0; x < DEFAULT_SIZE; x++) {
        data[0][x][0] =
            data[0][x][1] =
                data[0][x][2] =
                    data[0][x][3] = 255;

        data[x][0][0] =
            data[x][0][1] =
                data[x][0][2] =
                    data[x][0][3] = 255;

        data[DEFAULT_SIZE - 1][x][0] =
            data[DEFAULT_SIZE - 1][x][1] =
                data[DEFAULT_SIZE - 1][x][2] =
                    data[DEFAULT_SIZE - 1][x][3] = 255;

        data[x][DEFAULT_SIZE - 1][0] =
            data[x][DEFAULT_SIZE - 1][1] =
                data[x][DEFAULT_SIZE - 1][2] =
                    data[x][DEFAULT_SIZE - 1][3] = 255;
    }
    tr.defaultImage = R_CreateImage("*default", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_MIPMAP, 0);
}

void R_CreateBuiltinImages(void) {
    int x, y;
    byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

    R_CreateDefaultImage();

    Com_Memset(data, 255, sizeof(data));
    tr.whiteImage = R_CreateImage("*white", (byte*)data, 8, 8, IMGTYPE_COLORALPHA, IMGFLAG_NONE, 0);

    if (r_dlightMode->integer >= 2) {
        for (x = 0; x < MAX_DLIGHTS; x++) {
            tr.shadowCubemaps[x] = R_CreateImage(va("*shadowcubemap%i", x), NULL, PSHADOW_MAP_SIZE, PSHADOW_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE | IMGFLAG_CUBEMAP, 0);
        }
    }

    for (x = 0; x < DEFAULT_SIZE; x++) {
        for (y = 0; y < DEFAULT_SIZE; y++) {
            data[y][x][0] =
                data[y][x][1] =
                    data[y][x][2] = tr.identityLightByte;
            data[y][x][3] = 255;
        }
    }

    tr.identityLightImage = R_CreateImage("*identityLight", (byte*)data, 8, 8, IMGTYPE_COLORALPHA, IMGFLAG_NONE, 0);

    for (x = 0; x < 32; x++) {
        tr.scratchImage[x] = R_CreateImage("*scratch", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_PICMIP | IMGFLAG_CLAMPTOEDGE, 0);
    }

    R_CreateDlightImage();
    R_CreateFogImage();

    if (glRefConfig.framebufferObject) {
        int width, height, hdrFormat, rgbFormat;

        width = glConfig.vidWidth;
        height = glConfig.vidHeight;

        hdrFormat = GL_RGBA8;
        if (r_hdr->integer && glRefConfig.textureFloat)
            hdrFormat = GL_RGBA16F_ARB;
        else if (r_postProcessActive->integer && r_floatingPointFBOs->integer)
            hdrFormat = GL_RGBA16;

        rgbFormat = GL_RGBA8;

        tr.renderImage = R_CreateImage("_render", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);

        if (r_shadowBlur->integer || r_hdr->integer || r_postProcessActive->integer)
            tr.screenScratchImage = R_CreateImage("screenScratch", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, rgbFormat);

        R_QLPostCreateImages(width, height, r_hdr->integer ? GL_RGBA8 : hdrFormat);

        if (r_shadowBlur->integer || r_ssao->integer)
            tr.hdrDepthImage = R_CreateImage("*hdrDepth", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_R32F);

        if (r_drawSunRays->integer)
            tr.sunRaysImage = R_CreateImage("*sunRays", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, rgbFormat);

        tr.renderDepthImage = R_CreateImage("*renderdepth", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);
        tr.textureDepthImage = R_CreateImage("*texturedepth", NULL, PSHADOW_MAP_SIZE, PSHADOW_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);

        {
            void* p;

            data[0][0][0] = 0;
            data[0][0][1] = 0.45f * 255;
            data[0][0][2] = 255;
            data[0][0][3] = 255;
            p = data;

            tr.calcLevelsImage = R_CreateImage("*calcLevels", p, 1, 1, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
            tr.targetLevelsImage = R_CreateImage("*targetLevels", p, 1, 1, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
            tr.fixedLevelsImage = R_CreateImage("*fixedLevels", p, 1, 1, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, hdrFormat);
        }

        for (x = 0; x < 2; x++) {
            tr.textureScratchImage[x] = R_CreateImage(va("*textureScratch%d", x), NULL, 256, 256, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
        }
        for (x = 0; x < 2; x++) {
            tr.quarterImage[x] = R_CreateImage(va("*quarter%d", x), NULL, width / 2, height / 2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
        }

        if (r_ssao->integer) {
            tr.screenSsaoImage = R_CreateImage("*screenSsao", NULL, width / 2, height / 2, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
        }

        for (x = 0; x < MAX_DRAWN_PSHADOWS; x++) {
            tr.pshadowMaps[x] = R_CreateImage(va("*shadowmap%i", x), NULL, PSHADOW_MAP_SIZE, PSHADOW_MAP_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);
        }

        if (r_sunlightMode->integer) {
            for (x = 0; x < 4; x++) {
                tr.sunShadowDepthImage[x] = R_CreateImage(va("*sunshadowdepth%i", x), NULL, r_shadowMapSize->integer, r_shadowMapSize->integer, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_DEPTH_COMPONENT24);
                qglTextureParameterfEXT(tr.sunShadowDepthImage[x]->texnum, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
                qglTextureParameterfEXT(tr.sunShadowDepthImage[x]->texnum, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            }

            tr.screenShadowImage = R_CreateImage("*screenShadow", NULL, width, height, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, GL_RGBA8);
        }

        if (r_cubeMapping->integer) {
            tr.renderCubeImage = R_CreateImage("*renderCube", NULL, r_cubemapSize->integer, r_cubemapSize->integer, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE | IMGFLAG_MIPMAP | IMGFLAG_CUBEMAP, rgbFormat);
        }
    }
}

void R_SetColorMappings(void) {
    int i, j;
    float g;
    int inf;

    tr.overbrightBits = r_overBrightBits->integer;

    if (!r_colorCorrectActive->integer && (!glConfig.deviceSupportsGamma || !glConfig.isFullscreen)) {
        tr.overbrightBits = 0;
    }

    if (glConfig.colorBits > 16) {
        if (tr.overbrightBits > 2) {
            tr.overbrightBits = 2;
        }
    } else {
        if (tr.overbrightBits > 1) {
            tr.overbrightBits = 1;
        }
    }

    if (tr.overbrightBits < 0) {
        tr.overbrightBits = 0;
    }

    tr.identityLight = 1.0f / (1 << tr.overbrightBits);
    tr.identityLightByte = 255 * tr.identityLight;

    if (r_intensity->value <= 1) {
        ri.Cvar_Set("r_intensity", "1");
    }

    if (r_gamma->value < 0.5f) {
        ri.Cvar_Set("r_gamma", "0.5");
    } else if (r_gamma->value > 3.0f) {
        ri.Cvar_Set("r_gamma", "3.0");
    }

    g = r_gamma->value;

    for (i = 0; i < 256; i++) {
        if (g == 1) {
            inf = i;
        } else {
            inf = 255 * pow(i / 255.0f, 1.0f / g) + 0.5f;
        }

        inf <<= tr.overbrightBits;

        if (inf < 0) {
            inf = 0;
        }
        if (inf > 255) {
            inf = 255;
        }
        s_gammatable[i] = inf;
    }

    for (i = 0; i < 256; i++) {
        j = i * r_intensity->value;
        if (j > 255) {
            j = 255;
        }
        s_intensitytable[i] = j;
    }

    if (!r_colorCorrectActive->integer && glConfig.deviceSupportsGamma) {
        GLimp_SetGamma(s_gammatable, s_gammatable, s_gammatable);
    }
}

void R_InitImages(void) {
    Com_Memset(hashTable, 0, sizeof(hashTable));
    R_SetColorMappings();

    R_CreateBuiltinImages();
}

void R_DeleteTextures(void) {
    int i;

    for (i = 0; i < tr.numImages; i++) {
        qglDeleteTextures(1, &tr.images[i]->texnum);
    }
    Com_Memset(tr.images, 0, sizeof(tr.images));

    tr.numImages = 0;

    GL_BindNullTextures();
}

static char* CommaParse(char** data_p) {
    int c = 0, len;
    char* data;
    static char com_token[MAX_TOKEN_CHARS];

    data = *data_p;
    len = 0;
    com_token[0] = 0;

    if (!data) {
        *data_p = NULL;
        return com_token;
    }

    while (1) {
        while ((c = *data) <= ' ') {
            if (!c) {
                break;
            }
            data++;
        }

        c = *data;

        if (c == '/' && data[1] == '/') {
            data += 2;
            while (*data && *data != '\n') {
                data++;
            }
        }
        else if (c == '/' && data[1] == '*') {
            data += 2;
            while (*data && (*data != '*' || data[1] != '/')) {
                data++;
            }
            if (*data) {
                data += 2;
            }
        } else {
            break;
        }
    }

    if (c == 0) {
        return "";
    }

    if (c == '\"') {
        data++;
        while (1) {
            c = *data++;
            if (c == '\"' || !c) {
                com_token[len] = 0;
                *data_p = (char*)data;
                return com_token;
            }
            if (len < MAX_TOKEN_CHARS - 1) {
                com_token[len] = c;
                len++;
            }
        }
    }

    do {
        if (len < MAX_TOKEN_CHARS - 1) {
            com_token[len] = c;
            len++;
        }
        data++;
        c = *data;
    } while (c > 32 && c != ',');

    com_token[len] = 0;

    *data_p = (char*)data;
    return com_token;
}

qhandle_t RE_RegisterSkin(const char* name) {
    skinSurface_t parseSurfaces[MAX_SKIN_SURFACES];
    qhandle_t hSkin;
    skin_t* skin;
    skinSurface_t* surf;
    union {
        char* c;
        void* v;
    } text;
    char* text_p;
    char* token;
    char surfName[MAX_QPATH];
    int totalSurfaces;

    if (!name || !name[0]) {
        ri.Printf(PRINT_DEVELOPER, "Empty name passed to RE_RegisterSkin\n");
        return 0;
    }

    if (strlen(name) >= MAX_QPATH) {
        ri.Printf(PRINT_DEVELOPER, "Skin name exceeds MAX_QPATH\n");
        return 0;
    }

    for (hSkin = 1; hSkin < tr.numSkins; hSkin++) {
        skin = tr.skins[hSkin];
        if (!Q_stricmp(skin->name, name)) {
            if (skin->numSurfaces == 0) {
                return 0;
            }
            return hSkin;
        }
    }

    if (tr.numSkins == MAX_SKINS) {
        ri.Printf(PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name);
        return 0;
    }
    tr.numSkins++;
    skin = ri.Hunk_Alloc(sizeof(skin_t), h_low);
    tr.skins[hSkin] = skin;
    Q_strncpyz(skin->name, name, sizeof(skin->name));
    skin->numSurfaces = 0;

    R_IssuePendingRenderCommands();

    if (strcmp(name + strlen(name) - 5, ".skin")) {
        skin->numSurfaces = 1;
        skin->surfaces = ri.Hunk_Alloc(sizeof(skinSurface_t), h_low);
        skin->surfaces[0].shader = R_FindShader(name, LIGHTMAP_NONE, qtrue);
        return hSkin;
    }

    ri.FS_ReadFile(name, &text.v);
    if (!text.c) {
        return 0;
    }

    totalSurfaces = 0;
    text_p = text.c;
    while (text_p && *text_p) {
        token = CommaParse(&text_p);
        Q_strncpyz(surfName, token, sizeof(surfName));

        if (!token[0]) {
            break;
        }
        Q_strlwr(surfName);

        if (*text_p == ',') {
            text_p++;
        }

        if (strstr(token, "tag_")) {
            continue;
        }

        token = CommaParse(&text_p);

        if (skin->numSurfaces < MAX_SKIN_SURFACES) {
            surf = &parseSurfaces[skin->numSurfaces];
            Q_strncpyz(surf->name, surfName, sizeof(surf->name));
            surf->shader = R_FindShader(token, LIGHTMAP_NONE, qtrue);
            skin->numSurfaces++;
        }

        totalSurfaces++;
    }

    ri.FS_FreeFile(text.v);

    if (totalSurfaces > MAX_SKIN_SURFACES) {
        ri.Printf(PRINT_WARNING, "WARNING: Ignoring excess surfaces (found %d, max is %d) in skin '%s'!\n",
                  totalSurfaces, MAX_SKIN_SURFACES, name);
    }

    if (skin->numSurfaces == 0) {
        return 0;
    }

    skin->surfaces = ri.Hunk_Alloc(skin->numSurfaces * sizeof(skinSurface_t), h_low);
    memcpy(skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof(skinSurface_t));

    return hSkin;
}

void R_InitSkins(void) {
    skin_t* skin;

    tr.numSkins = 1;

    skin = tr.skins[0] = ri.Hunk_Alloc(sizeof(skin_t), h_low);
    Q_strncpyz(skin->name, "<default skin>", sizeof(skin->name));
    skin->numSurfaces = 1;
    skin->surfaces = ri.Hunk_Alloc(sizeof(skinSurface_t), h_low);
    skin->surfaces[0].shader = tr.defaultShader;
}

skin_t* R_GetSkinByHandle(qhandle_t hSkin) {
    if (hSkin < 1 || hSkin >= tr.numSkins) {
        return tr.skins[0];
    }
    return tr.skins[hSkin];
}

void R_SkinList_f(void) {
    int i, j;
    skin_t* skin;

    ri.Printf(PRINT_ALL, "------------------\n");

    for (i = 0; i < tr.numSkins; i++) {
        skin = tr.skins[i];

        ri.Printf(PRINT_ALL, "%3i:%s (%d surfaces)\n", i, skin->name, skin->numSurfaces);
        for (j = 0; j < skin->numSurfaces; j++) {
            ri.Printf(PRINT_ALL, "       %s = %s\n",
                      skin->surfaces[j].name, skin->surfaces[j].shader->name);
        }
    }
    ri.Printf(PRINT_ALL, "------------------\n");
}
