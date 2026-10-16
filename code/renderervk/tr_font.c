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
#include "../renderercommon/tr_fontstash.h"
#include "../renderercommon/tr_stbtt.h"

vec4_t g_fontColor2D = { 1.0f, 1.0f, 1.0f, 1.0f };

#define FONT_ATLAS_INIT 512
#define FONT_ATLAS_MAX_W 2048
#define FONT_ATLAS_MAX_H 1024

static FONScontext *fontContext;
static image_t *fontImage;
static qhandle_t fontShader;
static int fontAtlasWidth;
static int fontAtlasHeight;

#define MAX_FONT_INDICES 32
static int fontIndexToId[MAX_FONT_INDICES];
static char fontIndexPath[MAX_FONT_INDICES][MAX_QPATH];
static int numFontIndices;
static const char *fallbackFontPaths[] = {
	"fonts/notosans-regular.ttf",
	"fonts/notosans-latinext.ttf",
	"fonts/notosanssymbols-regular.ttf",
	"fonts/notosanssymbols2-regular.ttf",
	"fonts/notosansmath-regular.ttf",
	"fonts/notosanshebrew-regular.ttf",
	"fonts/notosanssyriac-regular.ttf",
	"fonts/notosansmongolian-regular.ttf",
	"fonts/notosanstaiviet-regular.ttf",
	"fonts/notosanstaitham-regular.ttf",
	"fonts/notosansnewtailue-regular.ttf",
	"fonts/notosanslimbu-regular.ttf",
	"fonts/notosansmeeteimayek-regular.ttf",
	"fonts/notosansvai-regular.ttf",
	"fonts/notosansbatak-regular.ttf",
	"fonts/notosansegyptianhieroglyphs-regular.ttf",
	"fonts/notosanscanadianaboriginal-regular.ttf",
	"fonts/notosanscherokee-regular.ttf",
	"fonts/notosansrunic-regular.ttf",
	"fonts/notosanstifinagh-regular.ttf",
	"fonts/notosansarmenian-regular.ttf",
	"fonts/notosansgeorgian-regular.ttf",
	"fonts/notosanscoptic-regular.ttf",
	"fonts/notosansglagolitic-regular.ttf",
	"fonts/notosansthaana-regular.ttf",
	"fonts/notosansnko-regular.ttf",
	"fonts/notosansadlam-regular.ttf",
	"fonts/notosansolchiki-regular.ttf",
	"fonts/notosansosage-regular.ttf",
	"fonts/notosanslisu-regular.ttf",
	"fonts/notosansjavanese-regular.ttf",
	"fonts/notosansbalinese-regular.ttf",
	"fonts/notosanssundanese-regular.ttf",
	"fonts/notosansbuginese-regular.ttf",
	"fonts/notosanstagalog-regular.ttf",
	"fonts/notosansbamum-regular.ttf",
	"fonts/notoemoji-regular.ttf",
	"fonts/droidsansfallbackfull.ttf",
};
static int fallbackFontIds[ARRAY_LEN( fallbackFontPaths )];

static void R_Font_AddFallbacks( int id ) {
	int i;

	if ( id == FONS_INVALID ) {
		return;
	}
	for ( i = 0; i < (int)ARRAY_LEN( fallbackFontIds ); i++ ) {
		if ( fallbackFontIds[i] != FONS_INVALID && fallbackFontIds[i] != id ) {
			fonsAddFallbackFont( fontContext, id, fallbackFontIds[i] );
		}
	}
}

static int imeCompositionFont;
static float imeCompositionScale;

static unsigned int R_Font_PackColor( const vec4_t c ) {
	int r = (int)( c[0] * 255.0f + 0.5f );
	int g = (int)( c[1] * 255.0f + 0.5f );
	int b = (int)( c[2] * 255.0f + 0.5f );
	int a = (int)( c[3] * 255.0f + 0.5f );

	r = r < 0 ? 0 : r > 255 ? 255 : r;
	g = g < 0 ? 0 : g > 255 ? 255 : g;
	b = b < 0 ? 0 : b > 255 ? 255 : b;
	a = a < 0 ? 0 : a > 255 ? 255 : a;
	return (unsigned int)r | ( (unsigned int)g << 8 ) | ( (unsigned int)b << 16 ) | ( (unsigned int)a << 24 );
}

static void R_Font_DefineAtlas( int width, int height ) {
	fontAtlasWidth = width;
	fontAtlasHeight = height;
}

static int RB_Font_renderCreate( void *uptr, int width, int height ) {
	byte *staging;
	int i, n = FONT_ATLAS_MAX_W * FONT_ATLAS_MAX_H;

	(void)uptr;

	staging = ri.Hunk_AllocateTempMemory( n * 4 );
	for ( i = 0; i < n; i++ ) {
		staging[i * 4 + 0] = staging[i * 4 + 1] = staging[i * 4 + 2] = 255;
		staging[i * 4 + 3] = 0;
	}
	fontImage = R_CreateImage( "*fontstash", NULL, staging, FONT_ATLAS_MAX_W, FONT_ATLAS_MAX_H,
		IMGFLAG_CLAMPTOEDGE | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NOSCALE );
	ri.Hunk_FreeTempMemory( staging );
	if ( !fontImage ) {
		return 0;
	}

	R_Font_DefineAtlas( width, height );
	fontShader = RE_RegisterShaderFromImage( "*fontstash", LIGHTMAP_2D, fontImage, qfalse );
	return 1;
}

static int RB_Font_renderResize( void *uptr, int width, int height ) {
	if ( !fontImage ) {
		return RB_Font_renderCreate( uptr, width, height );
	}
	R_Font_DefineAtlas( width, height );
	return 1;
}

static void RB_Font_renderUpdate( void *uptr, int *rect, const unsigned char *data ) {
	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];
	byte *rgba, *dst;
	int x, y;

	(void)uptr;

	if ( !fontImage || w <= 0 || h <= 0 ) {
		return;
	}

	rgba = ri.Hunk_AllocateTempMemory( w * h * 4 );
	dst = rgba;
	for ( y = 0; y < h; y++ ) {
		const unsigned char *src = data + ( rect[1] + y ) * fontAtlasWidth + rect[0];

		for ( x = 0; x < w; x++, dst += 4 ) {
			dst[0] = dst[1] = dst[2] = 255;
			dst[3] = src[x];
		}
	}
	vk_upload_image_data( fontImage, rect[0], rect[1], w, h, 1, rgba, w * h * 4, qtrue );
	ri.Hunk_FreeTempMemory( rgba );
}

static void RB_Font_renderDraw( void *uptr, const float *verts, const float *tcoords, const unsigned int *colors, int nverts ) {
	unsigned int lastColor = 0;
	qboolean haveColor = qfalse;
	int i;

	(void)uptr;

	if ( !fontShader ) {
		return;
	}

	for ( i = 0; i + 6 <= nverts; i += 6 ) {
		const float su = (float)fontAtlasWidth / FONT_ATLAS_MAX_W;
		const float sv = (float)fontAtlasHeight / FONT_ATLAS_MAX_H;
		unsigned int c = colors[i];
		float x0 = verts[i * 2 + 0];
		float y0 = verts[i * 2 + 1];
		float x1 = verts[( i + 1 ) * 2 + 0];
		float y1 = verts[( i + 1 ) * 2 + 1];

		if ( !haveColor || c != lastColor ) {
			vec4_t rgba;

			rgba[0] = (float)( c & 0xff ) / 255.0f;
			rgba[1] = (float)( ( c >> 8 ) & 0xff ) / 255.0f;
			rgba[2] = (float)( ( c >> 16 ) & 0xff ) / 255.0f;
			rgba[3] = (float)( ( c >> 24 ) & 0xff ) / 255.0f;
			RE_SetColor( rgba );
			lastColor = c;
			haveColor = qtrue;
		}

		RE_StretchPic( x0, y0, x1 - x0, y1 - y0, tcoords[i * 2 + 0] * su, tcoords[i * 2 + 1] * sv,
			tcoords[( i + 1 ) * 2 + 0] * su, tcoords[( i + 1 ) * 2 + 1] * sv, fontShader );
	}

	if ( haveColor ) {
		RE_SetColor( NULL );
	}
}

static void RB_Font_renderDelete( void *uptr ) {
	(void)uptr;
	fontImage = NULL;
	fontShader = 0;
	fontAtlasWidth = 0;
	fontAtlasHeight = 0;
}

static void R_Font_HandleError( void *uptr, int error, int val ) {
	FONScontext *ctx = (FONScontext *)uptr;
	int w, h;

	(void)val;

	if ( error != FONS_ATLAS_FULL ) {
		return;
	}

	fonsGetAtlasSize( ctx, &w, &h );
	if ( w <= h && w < FONT_ATLAS_MAX_W ) {
		w *= 2;
	} else if ( h < FONT_ATLAS_MAX_H ) {
		h *= 2;
	} else if ( w < FONT_ATLAS_MAX_W ) {
		w *= 2;
	} else {
		fonsResetAtlas( ctx, FONT_ATLAS_MAX_W, FONT_ATLAS_MAX_H );
		return;
	}
	fonsExpandAtlas( ctx, w, h );
}

static int R_Font_LoadTTF( const char *path, const char *name ) {
	void *buffer = NULL;
	unsigned char *copy;
	int len, id;

	len = ri.FS_ReadFile( path, &buffer );
	if ( len <= 0 || !buffer ) {
		if ( buffer ) {
			ri.FS_FreeFile( buffer );
		}
		return FONS_INVALID;
	}

	if ( len >= 4 && !memcmp( buffer, "OTTO", 4 ) ) {
		ri.FS_FreeFile( buffer );
		ri.Printf( PRINT_WARNING, "font '%s' has CFF outlines, which are not read; a TrueType one is needed\n", path );
		return FONS_INVALID;
	}

	copy = (unsigned char *)malloc( len );
	if ( !copy ) {
		ri.FS_FreeFile( buffer );
		return FONS_INVALID;
	}
	Com_Memcpy( copy, buffer, len );
	ri.FS_FreeFile( buffer );

	id = fonsAddFont( fontContext, name, copy, len, 1 );
	if ( id == FONS_INVALID ) {
		free( copy );
		ri.Printf( PRINT_WARNING, "R_InitFonts: failed to parse font '%s'\n", path );
	}
	return id;
}

#define MAX_EMOJI_NAME 80
static char (*emojiNames)[MAX_EMOJI_NAME];
static qhandle_t *emojiShaders;
static int numEmoji;

static void R_Font_LoadEmoji( void ) {
	char **list;
	const char **names;
	int n, i;

	list = ri.FS_ListFiles( "emoji", ".png", &n );
	if ( !list || n <= 0 ) {
		if ( list ) {
			ri.FS_FreeFileList( list );
		}
		return;
	}

	emojiNames = ri.Malloc( n * sizeof( *emojiNames ) );
	emojiShaders = ri.Malloc( n * sizeof( *emojiShaders ) );
	names = ri.Malloc( n * sizeof( *names ) );
	for ( i = 0; i < n; i++ ) {
		Q_strncpyz( emojiNames[i], list[i], sizeof( emojiNames[i] ) );
		COM_StripExtension( emojiNames[i], emojiNames[i], sizeof( emojiNames[i] ) );
		emojiShaders[i] = 0;
		names[i] = emojiNames[i];
	}
	numEmoji = n;
	ri.FS_FreeFileList( list );

	fonsSetEmoji( names, n );
	ri.Free( (void *)names );
}

static void R_Font_FreeEmoji( void ) {
	fonsSetEmoji( NULL, 0 );
	if ( emojiNames ) {
		ri.Free( emojiNames );
	}
	if ( emojiShaders ) {
		ri.Free( emojiShaders );
	}
	emojiNames = NULL;
	emojiShaders = NULL;
	numEmoji = 0;
}

static void RB_Font_renderEmoji( void *uptr, float x0, float y0, float x1, float y1, unsigned int color, int emoji ) {
	vec4_t rgba;

	(void)uptr;

	if ( emoji < 0 || emoji >= numEmoji ) {
		return;
	}
	if ( !emojiShaders[emoji] ) {
		char name[MAX_QPATH];

		Com_sprintf( name, sizeof( name ), "emoji/%s.png", emojiNames[emoji] );
		emojiShaders[emoji] = RE_RegisterShaderNoMip( name );
		if ( !emojiShaders[emoji] ) {
			emojiShaders[emoji] = -1;
		}
	}
	if ( emojiShaders[emoji] < 0 ) {
		return;
	}

	rgba[0] = rgba[1] = rgba[2] = 1.0f;
	rgba[3] = (float)( ( color >> 24 ) & 0xff ) / 255.0f;
	RE_SetColor( rgba );
	RE_StretchPic( x0, y0, x1 - x0, y1 - y0, 0.0f, 0.0f, 1.0f, 1.0f, emojiShaders[emoji] );
	RE_SetColor( NULL );
}

void R_InitFonts( void ) {
	FONSparams params;
	int normal, sans, mono, i;

	if ( fontContext ) {
		return;
	}

	Com_Memset( &params, 0, sizeof( params ) );
	params.width = FONT_ATLAS_INIT;
	params.height = FONT_ATLAS_INIT;
	params.flags = FONS_ZERO_TOPLEFT;
	params.renderCreate = RB_Font_renderCreate;
	params.renderResize = RB_Font_renderResize;
	params.renderUpdate = RB_Font_renderUpdate;
	params.renderDraw = RB_Font_renderDraw;
	params.renderDelete = RB_Font_renderDelete;
	params.renderEmoji = RB_Font_renderEmoji;

	fontContext = fonsCreateInternal( &params );
	if ( !fontContext ) {
		ri.Printf( PRINT_WARNING, "R_InitFonts: failed to create font context\n" );
		return;
	}
	fonsSetErrorCallback( fontContext, R_Font_HandleError, fontContext );

	normal = R_Font_LoadTTF( DEFAULT_FONT, "normal" );
	sans = R_Font_LoadTTF( DEFAULT_SANS_FONT, "sans" );
	mono = R_Font_LoadTTF( DEFAULT_MONO_FONT, "mono" );

	for ( i = 0; i < (int)ARRAY_LEN( fallbackFontPaths ); i++ ) {
		fallbackFontIds[i] = R_Font_LoadTTF( fallbackFontPaths[i], fallbackFontPaths[i] );
	}

	fontIndexToId[0] = normal != FONS_INVALID ? normal : 0;
	fontIndexToId[1] = sans != FONS_INVALID ? sans : fontIndexToId[0];
	fontIndexToId[2] = mono != FONS_INVALID ? mono : fontIndexToId[0];
	Q_strncpyz( fontIndexPath[0], DEFAULT_FONT, sizeof( fontIndexPath[0] ) );
	Q_strncpyz( fontIndexPath[1], DEFAULT_SANS_FONT, sizeof( fontIndexPath[1] ) );
	Q_strncpyz( fontIndexPath[2], DEFAULT_MONO_FONT, sizeof( fontIndexPath[2] ) );
	numFontIndices = 3;

	R_Font_AddFallbacks( normal );
	R_Font_AddFallbacks( sans );
	R_Font_AddFallbacks( mono );

	if ( normal == FONS_INVALID ) {
		ri.Printf( PRINT_WARNING, "R_InitFonts: default font '%s' missing - text will not render\n", DEFAULT_FONT );
	}

	R_Font_LoadEmoji();
}

void R_FontShutdown( void ) {
	int i;

	if ( !fontContext ) {
		return;
	}
	fonsDeleteInternal( fontContext );
	fontContext = NULL;
	numFontIndices = 0;
	R_Font_FreeEmoji();
	for ( i = 0; i < (int)ARRAY_LEN( fallbackFontIds ); i++ ) {
		fallbackFontIds[i] = FONS_INVALID;
	}
}

static qboolean R_Font_IsWebText( const char *path ) {
	return ( !Q_stricmpn( path, "webui/", 6 ) || !Q_stricmpn( path, "fonts/Barlow-", 13 ) ||
		!Q_stricmp( path, "fonts/LiberationSans-Regular.ttf" ) ) ? qtrue : qfalse;
}

int RE_Font_Register( const char *name ) {
	char paths[2][MAX_QPATH];
	const char *dir;
	int numPaths, i, p, id;

	if ( !fontContext || !name || !name[0] ) {
		return -1;
	}

	dir = ( strchr( name, '/' ) || strchr( name, '\\' ) ) ? "" : "fonts/";
	if ( COM_GetExtension( name )[0] ) {
		Com_sprintf( paths[0], sizeof( paths[0] ), "%s%s", dir, name );
		numPaths = 1;
	} else {
		Com_sprintf( paths[0], sizeof( paths[0] ), "%s%s.ttf", dir, name );
		Com_sprintf( paths[1], sizeof( paths[1] ), "%s%s.otf", dir, name );
		numPaths = 2;
	}

	for ( p = 0; p < numPaths; p++ ) {
		for ( i = 0; i < numFontIndices; i++ ) {
			if ( !Q_stricmp( fontIndexPath[i], paths[p] ) ) {
				return i;
			}
		}
	}

	if ( numFontIndices >= MAX_FONT_INDICES ) {
		ri.Printf( PRINT_WARNING, "RE_Font_Register: no room for '%s', %i fonts are loaded\n", name, numFontIndices );
		return -1;
	}

	for ( p = 0; p < numPaths; p++ ) {
		id = R_Font_LoadTTF( paths[p], paths[p] );
		if ( id == FONS_INVALID ) {
			continue;
		}
		R_Font_AddFallbacks( id );
		if ( R_Font_IsWebText( paths[p] ) ) {
			fonsSetFontContrast( fontContext, id, 1.7f, 1.2f );
		}
		fontIndexToId[numFontIndices] = id;
		Q_strncpyz( fontIndexPath[numFontIndices], paths[p], sizeof( fontIndexPath[numFontIndices] ) );
		return numFontIndices++;
	}

	if ( numPaths > 1 ) {
		ri.Printf( PRINT_WARNING, "RE_Font_Register: couldn't load font '%s' (looked for %s and %s; /fontlist shows what there is)\n",
			name, paths[0], paths[1] );
	} else {
		ri.Printf( PRINT_WARNING, "RE_Font_Register: couldn't load font '%s' (looked for %s; /fontlist shows what there is)\n",
			name, paths[0] );
	}
	return -1;
}

static void R_FontListFiles( const char *extension ) {
	char **files;
	int numFiles, i;

	files = ri.FS_ListFiles( "fonts", extension, &numFiles );
	for ( i = 0; i < numFiles; i++ ) {
		ri.Printf( PRINT_ALL, "  %s\n", files[i] );
	}
	ri.FS_FreeFileList( files );
}

void R_FontList_f( void ) {
	int i;

	ri.Printf( PRINT_ALL, "Loaded fonts:\n" );
	for ( i = 0; i < numFontIndices; i++ ) {
		ri.Printf( PRINT_ALL, "  %2i: %s\n", i, fontIndexPath[i] );
	}

	ri.Printf( PRINT_ALL, "Fonts to choose from, by name without the extension:\n" );
	R_FontListFiles( ".ttf" );
	R_FontListFiles( ".otf" );
}

static int R_Font_ResolveId( int fontIndex ) {
	if ( fontIndex < 0 || fontIndex >= numFontIndices ) {
		return numFontIndices > 0 ? fontIndexToId[0] : 0;
	}
	return fontIndexToId[fontIndex];
}

static void R_Font_SetColorCodes( void ) {
	static cvar_t *extendedColors;

	if ( !extendedColors ) {
		extendedColors = ri.Cvar_Get( "cl_extendedColors", "1", CVAR_ROM );
	}
	fonsSetExtendedColors( extendedColors->integer );
}

void RE_Font_DrawString( int x, int y, const char *text, int fontIndex, float scale, int limit, float *maxX, int flags ) {
	const char *end = NULL;

	if ( !fontContext || !text || !text[0] ) {
		return;
	}

	fonsSetFont( fontContext, R_Font_ResolveId( fontIndex ) );
	fonsSetSize( fontContext, scale );
	fonsSetAlign( fontContext, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE );
	fonsSetSpacing( fontContext, 0.0f );
	fonsSetBlur( fontContext, 0.0f );
	fonsSetColor( fontContext, R_Font_PackColor( g_fontColor2D ) );

	if ( limit > 0 && (int)strlen( text ) > limit ) {
		end = text + limit;
	}

	R_Font_SetColorCodes();
	fonsDrawText( fontContext, (float)x, (float)y, text, end, flags, maxX );
}

void RE_TextBounds( const char *text, int start, int fontIndex, float scale, int limit, float *extents ) {
	const char *str, *end;

	if ( !extents ) {
		return;
	}
	extents[0] = extents[1] = extents[2] = extents[3] = extents[4] = 0.0f;

	if ( !fontContext || !text ) {
		return;
	}

	fonsSetFont( fontContext, R_Font_ResolveId( fontIndex ) );
	fonsSetSize( fontContext, scale );
	fonsSetAlign( fontContext, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE );
	fonsSetSpacing( fontContext, 0.0f );
	fonsSetBlur( fontContext, 0.0f );

	str = text + ( start > 0 ? start : 0 );
	end = limit > 0 ? str + limit : NULL;

	R_Font_SetColorCodes();
	fonsTextBounds( fontContext, 0.0f, 0.0f, str, end, extents );
	fonsVertMetrics( fontContext, &extents[4], NULL, NULL );
}

void RE_GetGlyphInfo( int fontIndex, int charValue, glyphInfo_t *out ) {
	char buf[8];
	float bounds[4];
	float ascender, descender, lineh;
	unsigned int cp = (unsigned int)charValue;
	int n = 0;

	if ( !out ) {
		return;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	if ( !fontContext ) {
		return;
	}

	if ( cp < 0x80 ) {
		buf[n++] = (char)cp;
	} else if ( cp < 0x800 ) {
		buf[n++] = (char)( 0xC0 | ( cp >> 6 ) );
		buf[n++] = (char)( 0x80 | ( cp & 0x3F ) );
	} else if ( cp < 0x10000 ) {
		buf[n++] = (char)( 0xE0 | ( cp >> 12 ) );
		buf[n++] = (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
		buf[n++] = (char)( 0x80 | ( cp & 0x3F ) );
	} else {
		buf[n++] = (char)( 0xF0 | ( cp >> 18 ) );
		buf[n++] = (char)( 0x80 | ( ( cp >> 12 ) & 0x3F ) );
		buf[n++] = (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
		buf[n++] = (char)( 0x80 | ( cp & 0x3F ) );
	}
	buf[n] = '\0';

	fonsSetFont( fontContext, R_Font_ResolveId( fontIndex ) );
	fonsSetAlign( fontContext, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE );

	out->xSkip = (int)fonsTextBounds( fontContext, 0.0f, 0.0f, buf, NULL, bounds );
	out->left = (int)bounds[0];
	out->top = (int)bounds[1];
	out->imageWidth = (int)( bounds[2] - bounds[0] );
	out->imageHeight = (int)( bounds[3] - bounds[1] );
	fonsVertMetrics( fontContext, &ascender, &descender, &lineh );
	out->height = (int)( ascender - descender );
	out->glyph = fontShader;
}

void RE_SetCompositionFont( int fontIndex, float scale ) {
	imeCompositionFont = fontIndex;
	imeCompositionScale = scale;
}

void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	(void)fontName;
	(void)pointSize;
	if ( font ) {
		Com_Memset( font, 0, sizeof( *font ) );
	}
}
