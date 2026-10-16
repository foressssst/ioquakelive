
#include "tr_local.h"
#include "../renderercommon/tr_fontstash.h"
#include "../renderercommon/tr_stbtt.h"

vec4_t g_fontColor2D = {1.0f, 1.0f, 1.0f, 1.0f};

#define FONT_ATLAS_INIT 512
#define FONT_ATLAS_MAX_W 2048
#define FONT_ATLAS_MAX_H 1024

static FONScontext *fontContext = NULL;
static image_t *fontImage = NULL;
static qhandle_t fontShader = 0;
static int fontAtlasWidth = 0;
static int fontAtlasHeight = 0;

#define MAX_FONT_INDICES 32
static int fontIndexToId[MAX_FONT_INDICES];
static char fontIndexPath[MAX_FONT_INDICES][MAX_QPATH];
static int numFontIndices = 0;
static int fallbackFontId = FONS_INVALID;
static int textFallbackFontId = FONS_INVALID;

static void R_Font_AddFallbacks(int id) {
    if (id == FONS_INVALID)
        return;
    if (textFallbackFontId != FONS_INVALID && textFallbackFontId != id)
        fonsAddFallbackFont(fontContext, id, textFallbackFontId);
    if (fallbackFontId != FONS_INVALID && fallbackFontId != id)
        fonsAddFallbackFont(fontContext, id, fallbackFontId);
}

static int imeCompositionFont = 0;
static float imeCompositionScale = 0.0f;

static unsigned int R_Font_PackColor(const vec4_t c) {
    int r = (int)(c[0] * 255.0f + 0.5f);
    int g = (int)(c[1] * 255.0f + 0.5f);
    int b = (int)(c[2] * 255.0f + 0.5f);
    int a = (int)(c[3] * 255.0f + 0.5f);
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    if (a < 0) a = 0; else if (a > 255) a = 255;
    return ((unsigned int)r) | ((unsigned int)g << 8) | ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

static void R_Font_SetTexParams(GLuint tex) {
    qglTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    qglTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    qglTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    qglTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    qglTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static int RB_Font_renderCreate(void *uptr, int width, int height) {
    byte *staging;

    (void)uptr;

    staging = (byte *)calloc((size_t)width * height, 4);
    if (staging == NULL)
        return 0;

    fontImage = R_CreateImage("*fontstash", staging, width, height,
                              IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOLIGHTSCALE, GL_RGBA8);
    free(staging);
    if (fontImage == NULL)
        return 0;

    qglTextureImage2DEXT(fontImage->texnum, GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    R_Font_SetTexParams(fontImage->texnum);
    fontImage->internalFormat = GL_R8;
    fontImage->width = fontImage->uploadWidth = width;
    fontImage->height = fontImage->uploadHeight = height;

    fontShader = RE_RegisterShaderFromImage("*fontstash", LIGHTMAP_2D, fontImage, qfalse);

    fontAtlasWidth = width;
    fontAtlasHeight = height;

    GL_CheckErrors();
    return 1;
}

static int RB_Font_renderResize(void *uptr, int width, int height) {
    if (fontImage == NULL)
        return RB_Font_renderCreate(uptr, width, height);

    qglTextureImage2DEXT(fontImage->texnum, GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    R_Font_SetTexParams(fontImage->texnum);
    fontImage->width = fontImage->uploadWidth = width;
    fontImage->height = fontImage->uploadHeight = height;

    fontAtlasWidth = width;
    fontAtlasHeight = height;

    GL_CheckErrors();
    return 1;
}

static void RB_Font_renderUpdate(void *uptr, int *rect, const unsigned char *data) {
    int w = rect[2] - rect[0];
    int h = rect[3] - rect[1];

    (void)uptr;

    if (fontImage == NULL || w <= 0 || h <= 0)
        return;

    qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    qglPixelStorei(GL_UNPACK_ROW_LENGTH, fontAtlasWidth);
    qglPixelStorei(GL_UNPACK_SKIP_PIXELS, rect[0]);
    qglPixelStorei(GL_UNPACK_SKIP_ROWS, rect[1]);
    qglTextureSubImage2DEXT(fontImage->texnum, GL_TEXTURE_2D, 0, rect[0], rect[1], w, h, GL_RED, GL_UNSIGNED_BYTE, data);
    qglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    qglPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    qglPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    GL_CheckErrors();
}

static void RB_Font_renderDraw(void *uptr, const float *verts, const float *tcoords, const unsigned int *colors, int nverts) {
    int i;
    unsigned int lastColor = 0;
    qboolean haveColor = qfalse;

    (void)uptr;

    if (fontShader == 0)
        return;

    for (i = 0; i + 6 <= nverts; i += 6) {
        unsigned int c = colors[i];
        float x0 = verts[i * 2 + 0];
        float y0 = verts[i * 2 + 1];
        float x1 = verts[(i + 1) * 2 + 0];
        float y1 = verts[(i + 1) * 2 + 1];
        float s0 = tcoords[i * 2 + 0];
        float t0 = tcoords[i * 2 + 1];
        float s1 = tcoords[(i + 1) * 2 + 0];
        float t1 = tcoords[(i + 1) * 2 + 1];

        if (!haveColor || c != lastColor) {
            vec4_t rgba;
            rgba[0] = (float)(c & 0xff) / 255.0f;
            rgba[1] = (float)((c >> 8) & 0xff) / 255.0f;
            rgba[2] = (float)((c >> 16) & 0xff) / 255.0f;
            rgba[3] = (float)((c >> 24) & 0xff) / 255.0f;
            RE_SetColor(rgba);
            lastColor = c;
            haveColor = qtrue;
        }

        RE_StretchPic(x0, y0, x1 - x0, y1 - y0, s0, t0, s1, t1, fontShader);
    }

    if (haveColor)
        RE_SetColor(NULL);
}

static void RB_Font_renderDelete(void *uptr) {
    (void)uptr;
    fontImage = NULL;
    fontShader = 0;
    fontAtlasWidth = 0;
    fontAtlasHeight = 0;
}

static void R_Font_HandleError(void *uptr, int error, int val) {
    FONScontext *ctx = (FONScontext *)uptr;
    int w, h;

    (void)val;

    if (error != FONS_ATLAS_FULL)
        return;

    fonsGetAtlasSize(ctx, &w, &h);

    if (w <= h && w < FONT_ATLAS_MAX_W)
        w *= 2;
    else if (h < FONT_ATLAS_MAX_H)
        h *= 2;
    else if (w < FONT_ATLAS_MAX_W)
        w *= 2;
    else {
        fonsResetAtlas(ctx, FONT_ATLAS_MAX_W, FONT_ATLAS_MAX_H);
        return;
    }

    fonsExpandAtlas(ctx, w, h);
}

static int R_Font_LoadTTF(const char *path, const char *name) {
    void *buffer = NULL;
    unsigned char *copy;
    int len, id;

    len = ri.FS_ReadFile(path, &buffer);
    if (len <= 0 || buffer == NULL) {
        if (buffer)
            ri.FS_FreeFile(buffer);
        return FONS_INVALID;
    }

    copy = (unsigned char *)malloc(len);
    if (copy == NULL) {
        ri.FS_FreeFile(buffer);
        return FONS_INVALID;
    }
    Com_Memcpy(copy, buffer, len);
    ri.FS_FreeFile(buffer);

    id = fonsAddFont(fontContext, name, copy, len, 1);
    if (id == FONS_INVALID) {
        free(copy);
        ri.Printf(PRINT_WARNING, "R_InitFonts: failed to parse font '%s'\n", path);
    }
    return id;
}

void R_InitFonts(void) {
    FONSparams params;
    int normal, sans, mono, fallback;

    if (fontContext != NULL)
        return;

    Com_Memset(&params, 0, sizeof(params));
    params.width = FONT_ATLAS_INIT;
    params.height = FONT_ATLAS_INIT;
    params.flags = FONS_ZERO_TOPLEFT;
    params.userPtr = NULL;
    params.renderCreate = RB_Font_renderCreate;
    params.renderResize = RB_Font_renderResize;
    params.renderUpdate = RB_Font_renderUpdate;
    params.renderDraw = RB_Font_renderDraw;
    params.renderDelete = RB_Font_renderDelete;

    fontContext = fonsCreateInternal(&params);
    if (fontContext == NULL) {
        ri.Printf(PRINT_WARNING, "R_InitFonts: failed to create font context\n");
        return;
    }
    fonsSetErrorCallback(fontContext, R_Font_HandleError, fontContext);

    normal = R_Font_LoadTTF(DEFAULT_FONT, "normal");
    sans = R_Font_LoadTTF(DEFAULT_SANS_FONT, "sans");
    mono = R_Font_LoadTTF(DEFAULT_MONO_FONT, "mono");
    fallback = R_Font_LoadTTF("fonts/droidsansfallbackfull.ttf", "sans-fallback");
    textFallbackFontId = R_Font_LoadTTF("fonts/notosans-regular.ttf", "text-fallback");

    fontIndexToId[0] = (normal != FONS_INVALID) ? normal : 0;
    fontIndexToId[1] = (sans != FONS_INVALID) ? sans : fontIndexToId[0];
    fontIndexToId[2] = (mono != FONS_INVALID) ? mono : fontIndexToId[0];
    Q_strncpyz(fontIndexPath[0], DEFAULT_FONT, sizeof(fontIndexPath[0]));
    Q_strncpyz(fontIndexPath[1], DEFAULT_SANS_FONT, sizeof(fontIndexPath[1]));
    Q_strncpyz(fontIndexPath[2], DEFAULT_MONO_FONT, sizeof(fontIndexPath[2]));
    numFontIndices = 3;
    fallbackFontId = fallback;

    R_Font_AddFallbacks(normal);
    R_Font_AddFallbacks(sans);
    R_Font_AddFallbacks(mono);

    if (normal == FONS_INVALID)
        ri.Printf(PRINT_WARNING, "R_InitFonts: default font '%s' missing - text will not render\n", DEFAULT_FONT);
}

void R_FontShutdown(void) {
    if (fontContext == NULL)
        return;
    fonsDeleteInternal(fontContext);
    fontContext = NULL;
    numFontIndices = 0;
    fallbackFontId = FONS_INVALID;
}

static qboolean R_Font_IsWebText(const char *path) {
    return (!Q_stricmpn(path, "webui/", 6) || !Q_stricmpn(path, "fonts/Barlow-", 13) ||
            !Q_stricmp(path, "fonts/LiberationSans-Regular.ttf")) ? qtrue : qfalse;
}

int RE_Font_Register(const char *name) {
    char paths[2][MAX_QPATH];
    const char *dir;
    int numPaths, i, p, id;

    if (fontContext == NULL || name == NULL || name[0] == '\0')
        return -1;

    dir = (strchr(name, '/') || strchr(name, '\\')) ? "" : "fonts/";
    if (COM_GetExtension(name)[0] != '\0') {
        Com_sprintf(paths[0], sizeof(paths[0]), "%s%s", dir, name);
        numPaths = 1;
    } else {
        Com_sprintf(paths[0], sizeof(paths[0]), "%s%s.ttf", dir, name);
        Com_sprintf(paths[1], sizeof(paths[1]), "%s%s.otf", dir, name);
        numPaths = 2;
    }

    for (p = 0; p < numPaths; p++) {
        for (i = 0; i < numFontIndices; i++) {
            if (!Q_stricmp(fontIndexPath[i], paths[p]))
                return i;
        }
    }

    if (numFontIndices >= MAX_FONT_INDICES) {
        ri.Printf(PRINT_WARNING, "RE_Font_Register: no room for '%s', %i fonts are loaded\n", name, numFontIndices);
        return -1;
    }

    for (p = 0; p < numPaths; p++) {
        id = R_Font_LoadTTF(paths[p], paths[p]);
        if (id == FONS_INVALID)
            continue;
        if (fallbackFontId != FONS_INVALID)
            fonsAddFallbackFont(fontContext, id, fallbackFontId);
        if (R_Font_IsWebText(paths[p]))
            fonsSetFontContrast(fontContext, id, 1.7f, 1.2f);
        fontIndexToId[numFontIndices] = id;
        Q_strncpyz(fontIndexPath[numFontIndices], paths[p], sizeof(fontIndexPath[numFontIndices]));
        return numFontIndices++;
    }

    if (numPaths > 1)
        ri.Printf(PRINT_WARNING, "RE_Font_Register: couldn't load font '%s' (looked for %s and %s; /fontlist shows what there is)\n",
                  name, paths[0], paths[1]);
    else
        ri.Printf(PRINT_WARNING, "RE_Font_Register: couldn't load font '%s' (looked for %s; /fontlist shows what there is)\n",
                  name, paths[0]);
    return -1;
}

static void R_FontListFiles(const char *extension) {
    char **files;
    int numFiles, i;

    files = ri.FS_ListFiles("fonts", extension, &numFiles);
    for (i = 0; i < numFiles; i++)
        ri.Printf(PRINT_ALL, "  %s\n", files[i]);
    ri.FS_FreeFileList(files);
}

void R_FontList_f(void) {
    int i;

    ri.Printf(PRINT_ALL, "Loaded fonts:\n");
    for (i = 0; i < numFontIndices; i++)
        ri.Printf(PRINT_ALL, "  %2i: %s\n", i, fontIndexPath[i]);

    ri.Printf(PRINT_ALL, "Fonts to choose from, by name without the extension:\n");
    R_FontListFiles(".ttf");
    R_FontListFiles(".otf");
}

static int R_Font_ResolveId(int fontIndex) {
    if (fontIndex < 0 || fontIndex >= numFontIndices)
        return (numFontIndices > 0) ? fontIndexToId[0] : 0;
    return fontIndexToId[fontIndex];
}

static void R_Font_SetColorCodes(void) {
    static cvar_t *extendedColors;

    if (extendedColors == NULL)
        extendedColors = ri.Cvar_Get("cl_extendedColors", "1", CVAR_ROM);
    fonsSetExtendedColors(extendedColors->integer);
}

void RE_Font_DrawString(int x, int y, const char *text, int fontIndex, float scale, int limit, float *maxX, int flags) {
    const char *end = NULL;

    if (fontContext == NULL || text == NULL || text[0] == '\0')
        return;

    fonsSetFont(fontContext, R_Font_ResolveId(fontIndex));
    fonsSetSize(fontContext, scale);
    fonsSetAlign(fontContext, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
    fonsSetSpacing(fontContext, 0.0f);
    fonsSetBlur(fontContext, 0.0f);
    fonsSetColor(fontContext, R_Font_PackColor(g_fontColor2D));

    if (limit > 0 && (int)strlen(text) > limit)
        end = text + limit;

    R_Font_SetColorCodes();
    fonsDrawText(fontContext, (float)x, (float)y, text, end, flags, maxX);
}

void RE_TextBounds(const char *text, int start, int fontIndex, float scale, int limit, float *extents) {
    const char *str, *end;

    if (extents == NULL)
        return;
    extents[0] = extents[1] = extents[2] = extents[3] = extents[4] = 0.0f;

    if (fontContext == NULL || text == NULL)
        return;

    fonsSetFont(fontContext, R_Font_ResolveId(fontIndex));
    fonsSetSize(fontContext, scale);
    fonsSetAlign(fontContext, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
    fonsSetSpacing(fontContext, 0.0f);
    fonsSetBlur(fontContext, 0.0f);

    str = text + (start > 0 ? start : 0);
    end = (limit > 0) ? str + limit : NULL;

    R_Font_SetColorCodes();
    fonsTextBounds(fontContext, 0.0f, 0.0f, str, end, extents);
    fonsVertMetrics(fontContext, &extents[4], NULL, NULL);
}

void RE_GetGlyphInfo(int fontIndex, int charValue, glyphInfo_t *out) {
    char buf[8];
    float bounds[4];
    float ascender, descender, lineh;
    int n = 0;
    unsigned int cp = (unsigned int)charValue;

    if (out == NULL)
        return;
    Com_Memset(out, 0, sizeof(*out));
    if (fontContext == NULL)
        return;

    if (cp < 0x80) {
        buf[n++] = (char)cp;
    } else if (cp < 0x800) {
        buf[n++] = (char)(0xC0 | (cp >> 6));
        buf[n++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        buf[n++] = (char)(0xE0 | (cp >> 12));
        buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (cp & 0x3F));
    } else {
        buf[n++] = (char)(0xF0 | (cp >> 18));
        buf[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (cp & 0x3F));
    }
    buf[n] = '\0';

    fonsSetFont(fontContext, R_Font_ResolveId(fontIndex));
    fonsSetAlign(fontContext, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);

    out->xSkip = (int)fonsTextBounds(fontContext, 0.0f, 0.0f, buf, NULL, bounds);
    out->left = (int)bounds[0];
    out->top = (int)bounds[1];
    out->imageWidth = (int)(bounds[2] - bounds[0]);
    out->imageHeight = (int)(bounds[3] - bounds[1]);
    fonsVertMetrics(fontContext, &ascender, &descender, &lineh);
    out->height = (int)(ascender - descender);
    out->glyph = fontShader;
}

void RE_SetCompositionFont(int fontIndex, float scale) {
    imeCompositionFont = fontIndex;
    imeCompositionScale = scale;
}

void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
    (void)fontName;
    (void)pointSize;
    if (font)
        Com_Memset(font, 0, sizeof(*font));
}
