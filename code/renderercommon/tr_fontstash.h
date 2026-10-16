/*
===========================================================================
Quake Live font subsystem - fontstash driver (tr_fontstash)

Faithful reimplementation of the fontstash glyph-cache / text-layout library
Quake Live's engine ships (the fons* symbols: fonsCreateInternal, fonsAddFont,
fons__getGlyph, fons__getGlyphQuad, fonsDrawText, ...).  Renamed copy of Mikko
Mononen's public-domain fontstash, backed by the QL-faithful stb_truetype port
in tr_stbtt.c.

The renderer/GPU side sits behind FONSparams (the ~5-function backend QL
implements as its NanoVG-GL shim): renderCreate/Resize/Update/Draw/Delete.
fontstash rasterises glyphs on demand into a single-channel coverage atlas,
packs them with a skyline packer, and emits two triangles per glyph.  The
backend uploads dirty atlas rects and draws the emitted quads.

QL customisation reproduced here: fonsDrawText reads Quake3-style '^' colour
codes and sets the per-vertex colour.

C89.  No GL, no engine dependencies beyond tr_stbtt.
===========================================================================
*/
#ifndef __TR_FONTSTASH_H__
#define __TR_FONTSTASH_H__

#define FONS_INVALID -1

enum FONSflags {
    FONS_ZERO_TOPLEFT = 1,
    FONS_ZERO_BOTTOMLEFT = 2
};

enum FONSalign {
    FONS_ALIGN_LEFT = 1 << 0,
    FONS_ALIGN_CENTER = 1 << 1,
    FONS_ALIGN_RIGHT = 1 << 2,
    FONS_ALIGN_TOP = 1 << 3,
    FONS_ALIGN_MIDDLE = 1 << 4,
    FONS_ALIGN_BOTTOM = 1 << 5,
    FONS_ALIGN_BASELINE = 1 << 6
};

enum FONSglyphBitmap {
    FONS_GLYPH_BITMAP_OPTIONAL = 1,
    FONS_GLYPH_BITMAP_REQUIRED = 2
};

enum FONSerrorCode {
    FONS_ATLAS_FULL = 1,
    FONS_SCRATCH_FULL = 2,
    FONS_STATES_OVERFLOW = 3,
    FONS_STATES_UNDERFLOW = 4
};

#define FONS_TEXT_LITERAL 0x40000000
#define FONS_TEXT_NORECOLOR 0x20000000

typedef struct FONSparams {
    int width, height;
    unsigned char flags;
    void *userPtr;
    int (*renderCreate)(void *uptr, int width, int height);
    int (*renderResize)(void *uptr, int width, int height);
    void (*renderUpdate)(void *uptr, int *rect, const unsigned char *data);
    void (*renderDraw)(void *uptr, const float *verts, const float *tcoords, const unsigned int *colors, int nverts);
    void (*renderDelete)(void *uptr);
    void (*renderEmoji)(void *uptr, float x0, float y0, float x1, float y1, unsigned int color, int emoji);
} FONSparams;

typedef struct FONSquad {
    float x0, y0, s0, t0;
    float x1, y1, s1, t1;
} FONSquad;

typedef struct FONStextIter {
    float x, y, nextx, nexty, scale, spacing;
    unsigned int codepoint;
    short isize, iblur;
    struct FONSfont *font;
    int prevGlyphIndex;
    const char *str;
    const char *next;
    const char *end;
    unsigned int utf8state;
    int bitmapOption;
} FONStextIter;

typedef struct FONScontext FONScontext;

FONScontext *fonsCreateInternal(FONSparams *params);
void fonsDeleteInternal(FONScontext *s);

void fonsSetErrorCallback(FONScontext *s, void (*callback)(void *uptr, int error, int val), void *uptr);
void fonsGetAtlasSize(FONScontext *s, int *width, int *height);
int fonsExpandAtlas(FONScontext *s, int width, int height);
int fonsResetAtlas(FONScontext *stash, int width, int height);

int fonsAddFont(FONScontext *s, const char *name, unsigned char *data, int ndata, int freeData);
int fonsGetFontByName(FONScontext *s, const char *name);
int fonsAddFallbackFont(FONScontext *stash, int base, int fallback);
void fonsSetFontContrast(FONScontext *stash, int font, float gamma, float gain);

void fonsPushState(FONScontext *s);
void fonsPopState(FONScontext *s);
void fonsClearState(FONScontext *s);

void fonsSetSize(FONScontext *s, float size);
void fonsSetColor(FONScontext *s, unsigned int color);
void fonsSetSpacing(FONScontext *s, float spacing);
void fonsSetBlur(FONScontext *s, float blur);
void fonsSetAlign(FONScontext *s, int align);
void fonsSetFont(FONScontext *s, int font);
void fonsSetExtendedColors(int extended);
void fonsSetEmoji(const char *const *names, int count);

float fonsDrawText(FONScontext *s, float x, float y, const char *string, const char *end, int flags, float *maxX);

float fonsTextBounds(FONScontext *s, float x, float y, const char *string, const char *end, float *bounds);
void fonsLineBounds(FONScontext *s, float y, float *miny, float *maxy);
void fonsVertMetrics(FONScontext *s, float *ascender, float *descender, float *lineh);

int fonsTextIterInit(FONScontext *stash, FONStextIter *iter, float x, float y, const char *str, const char *end, int bitmapOption);
int fonsTextIterNext(FONScontext *stash, FONStextIter *iter, FONSquad *quad);

const unsigned char *fonsGetTextureData(FONScontext *stash, int *width, int *height);
int fonsValidateTexture(FONScontext *s, int *dirty);

#endif
