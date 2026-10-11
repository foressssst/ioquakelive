/*
===========================================================================
Quake Live font subsystem - TrueType rasteriser (tr_stbtt)

Faithful reimplementation of the TrueType glyph rasteriser Quake Live's engine
ships.  In the QL binary these functions are renamed with an "R_" prefix
(R_LoadFontTables, R_GetFontGlyphIndex, R_RasterizeGlyphOutline, ...), a
renamed copy of Sean Barrett's public-domain stb_truetype.  This port keeps the
QL names as public symbols and reproduces the algorithms (glyf outline
extraction, the version-2 antialiased sorted-edge scanline rasteriser,
hmtx/hhea metrics, kern format 0), matching observed QL behaviour (flatness
0.35, 8-bit coverage output).

Only the TrueType ('glyf') outline path is implemented.  Every font QL ships
(handelgothic, notosans-regular, droidsansmono, droidsansfallbackfull) is a
TrueType file.  CFF/Type2 ('CFF ') outlines are not handled.

C89.  No GL, no engine dependencies beyond the common headers.
===========================================================================
*/
#ifndef __TR_STBTT_H__
#define __TR_STBTT_H__

#define STBTT_vmove 1
#define STBTT_vline 2
#define STBTT_vcurve 3
#define STBTT_vcubic 4

typedef struct {
    void *userdata;
    unsigned char *data;
    int fontstart;

    int numGlyphs;

    int loca, head, glyf, hhea, hmtx, kern, gpos;
    int index_map;
    int indexToLocFormat;
} stbtt_fontinfo;

typedef struct {
    short x, y, cx, cy, cx1, cy1;
    unsigned char type, padding;
} stbtt_vertex;

int R_LoadFontTables(stbtt_fontinfo *info, const unsigned char *data, int offset);

int R_GetFontGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);

int R_GetFontUnitsPerEm(const stbtt_fontinfo *info);

void R_GetFontLineMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);

void R_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);

int R_GetGlyphKernAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2);

float R_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);

float R_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);

int R_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

int R_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);
void R_FreeGlyphShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);

void R_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);
void R_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);

void R_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph);
void R_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);

#endif
