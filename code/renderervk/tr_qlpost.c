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

cvar_t *r_bloomActive;
cvar_t *r_bloomBlurFalloff;
cvar_t *r_bloomBlurRadius;
cvar_t *r_bloomBlurScale;
cvar_t *r_bloomBrightThreshold;
cvar_t *r_bloomIntensity;
cvar_t *r_bloomPasses;
cvar_t *r_bloomSaturation;
cvar_t *r_bloomSceneIntensity;
cvar_t *r_bloomSceneSaturation;
cvar_t *r_bloomFusion;
cvar_t *r_colorCorrectActive;
cvar_t *r_contrast;
cvar_t *r_enableBloom;
cvar_t *r_enableColorCorrect;
cvar_t *r_enablePostProcess;
cvar_t *r_floatingPointFBOs;
cvar_t *r_postProcessActive;
cvar_t *web_browserActive;

typedef enum {
	QLP_COLORCORRECT,
	QLP_BRIGHTPASS,
	QLP_BLUR,
	QLP_COMBINE,
	QLP_COMBINE_BLOOM_ONLY,
	QLP_COUNT
} qlProgram_t;

typedef struct {
	GLuint program;
	GLint texture;
	GLint levels;
	GLint color;
	GLint invTexRes;
} qlPostProgram_t;

static qlPostProgram_t qlPrograms[QLP_COUNT];

#define QL_BLOOM_BUFFERS 8

static GLuint qlBloomFbo[QL_BLOOM_BUFFERS];
static GLuint qlBloomTex[QL_BLOOM_BUFFERS];
static int qlBloomWidth[QL_BLOOM_BUFFERS];
static int qlBloomHeight[QL_BLOOM_BUFFERS];

void R_QLPostRegister( void ) {
	r_bloomActive = ri.Cvar_Get( "r_bloomActive", "0", CVAR_ROM | CVAR_TEMP | CVAR_USERSAVE );
	r_bloomBlurFalloff = ri.Cvar_GetRange( "r_bloomBlurFalloff", "0.0", "0.75", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomBlurRadius = ri.Cvar_GetRange( "r_bloomBlurRadius", "5", "1", "12", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomBlurScale = ri.Cvar_GetRange( "r_bloomBlurScale", "0.0", "1.0", "2.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomBrightThreshold = ri.Cvar_GetRange( "r_bloomBrightThreshold", "0.25", "0.0", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomIntensity = ri.Cvar_GetRange( "r_bloomIntensity", "0.5", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomPasses = ri.Cvar_GetRange( "r_bloomPasses", "1", "1", "2", CVAR_ARCHIVE | CVAR_LATCH | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomSaturation = ri.Cvar_GetRange( "r_bloomSaturation", "0.8", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomSceneIntensity = ri.Cvar_GetRange( "r_bloomSceneIntensity", "1.0", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomSceneSaturation = ri.Cvar_GetRange( "r_bloomSceneSaturation", "1.0", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomFusion = ri.Cvar_Get( "r_bloomFusion", "1", CVAR_ARCHIVE_ND );
	r_colorCorrectActive = ri.Cvar_Get( "r_colorCorrectActive", "0", CVAR_ROM | CVAR_TEMP );
	r_contrast = ri.Cvar_Get( "r_contrast", "1.0", CVAR_ARCHIVE | CVAR_USERSAVE );
	r_enableBloom = ri.Cvar_Get( "r_enableBloom", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_enableColorCorrect = ri.Cvar_Get( "r_enableColorCorrect", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_enablePostProcess = ri.Cvar_Get( "r_enablePostProcess", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_floatingPointFBOs = ri.Cvar_Get( "r_floatingPointFBOs", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_postProcessActive = ri.Cvar_Get( "r_postProcessActive", "0", CVAR_ROM | CVAR_TEMP );
	web_browserActive = ri.Cvar_Get( "web_browserActive", "0", CVAR_ROM );

	RI_Cvar_SetGroup( r_contrast, CVG_RENDERER );
	RI_Cvar_SetGroup( r_bloomBrightThreshold, CVG_RENDERER );
	RI_Cvar_SetGroup( r_bloomIntensity, CVG_RENDERER );
	RI_Cvar_SetGroup( r_bloomSaturation, CVG_RENDERER );

}

void R_QLPostInit( void ) {
	ri.Cvar_Set( "r_postProcessActive", "0" );
	ri.Cvar_Set( "r_colorCorrectActive", "0" );
	ri.Cvar_Set( "r_bloomActive", "0" );

	if ( !r_enablePostProcess->integer || !vk.fboActive ) {
		return;
	}

	ri.Cvar_Set( "r_postProcessActive", "1" );

	if ( r_enableColorCorrect->integer ) {
		ri.Cvar_Set( "r_colorCorrectActive", "1" );
	}

	if ( r_enableBloom->integer ) {
		ri.Cvar_Set( "r_bloomActive", "1" );
	}
}

void R_QLPostShutdown( void ) {
}

void RE_BeginBloomScene( void ) {
}

void RE_ApplyBloom( void ) {
	bloomCommand_t *cmd;

	if ( !tr.registered || !r_bloomActive->integer ) {
		return;
	}
	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_BLOOM;
}

const void *RB_BloomScene( const void *data ) {
	return (const void *)( (const bloomCommand_t *)data + 1 );
}

#define BLOOM_TILE_WORDS 128

static uint32_t bloomMask[BLOOM_TILE_WORDS];
static int bloomShift, bloomCols, bloomRows;

static qboolean R_BloomTile( int tx, int ty ) {
	const int i = ty * bloomCols + tx;

	return ( bloomMask[i >> 5] >> ( i & 31 ) ) & 1 ? qtrue : qfalse;
}

static void R_BloomMarkTiles( float x, float y, float w, float h ) {
	const int shift = bloomShift;
	const int cols = bloomCols;
	const int rows = bloomRows;
	int x0, y0, x1, y1, tx, ty, i;

	if ( w < 0.0f ) {
		x += w;
		w = -w;
	}
	if ( h < 0.0f ) {
		y += h;
		h = -h;
	}
	x0 = (int)floor( x ) - 1;
	y0 = (int)floor( y ) - 1;
	x1 = (int)ceil( x + w ) + 1;
	y1 = (int)ceil( y + h ) + 1;
	if ( x0 < 0 ) {
		x0 = 0;
	}
	if ( y0 < 0 ) {
		y0 = 0;
	}
	if ( x1 > glConfig.vidWidth ) {
		x1 = glConfig.vidWidth;
	}
	if ( y1 > glConfig.vidHeight ) {
		y1 = glConfig.vidHeight;
	}
	if ( x1 <= x0 || y1 <= y0 ) {
		return;
	}
	for ( ty = y0 >> shift; ty <= ( ( y1 - 1 ) >> shift ) && ty < rows; ty++ ) {
		for ( tx = x0 >> shift; tx <= ( ( x1 - 1 ) >> shift ) && tx < cols; tx++ ) {
			i = ty * cols + tx;
			bloomMask[i >> 5] |= 1u << ( i & 31 );
		}
	}
}

static qboolean R_BloomBuildRects( uint32_t list, qboolean dirty ) {
	VkRect2D *rects = vk.bloomRects[list];
	uint32_t open[2][VK_BLOOM_RECTS];
	uint32_t openCount[2] = { 0, 0 };
	uint32_t count = 0, prev, cur, j, k;
	int tx, ty, x0;

	for ( ty = 0; ty < bloomRows; ty++ ) {
		prev = ty & 1;
		cur = prev ^ 1;
		openCount[cur] = 0;
		j = 0;
		tx = 0;
		while ( tx < bloomCols ) {
			if ( R_BloomTile( tx, ty ) != dirty ) {
				tx++;
				continue;
			}
			x0 = tx;
			while ( tx < bloomCols && R_BloomTile( tx, ty ) == dirty ) {
				tx++;
			}
			while ( j < openCount[prev] && rects[open[prev][j]].offset.x < x0 ) {
				j++;
			}
			if ( j < openCount[prev] && rects[open[prev][j]].offset.x == x0 && rects[open[prev][j]].extent.width == (uint32_t)( tx - x0 ) ) {
				k = open[prev][j++];
				rects[k].extent.height++;
			} else {
				if ( count == VK_BLOOM_RECTS ) {
					return qfalse;
				}
				k = count++;
				rects[k].offset.x = x0;
				rects[k].offset.y = ty;
				rects[k].extent.width = tx - x0;
				rects[k].extent.height = 1;
			}
			open[cur][openCount[cur]++] = k;
		}
	}

	for ( k = 0; k < count; k++ ) {
		rects[k].offset.x <<= bloomShift;
		rects[k].offset.y <<= bloomShift;
		rects[k].extent.width = MIN( rects[k].extent.width << bloomShift, (uint32_t)( glConfig.vidWidth - rects[k].offset.x ) );
		rects[k].extent.height = MIN( rects[k].extent.height << bloomShift, (uint32_t)( glConfig.vidHeight - rects[k].offset.y ) );
	}

	vk.bloomRectCount[list] = count;
	return qtrue;
}

static qboolean R_BloomScanTiles( const void *data ) {
	const stretchPicCommand_t *pic;
	const drawTris2DCommand_t *tris;
	const polyVert_t *vert;
	const drawSurfsCommand_t *surfs;
	float x0, y0, x1, y1;
	const viewParms_t *vp;
	uint32_t bits;
	int shift, cols, rows, dirty, i;

	if ( !r_bloomFusion->integer || r_showImages->integer ) {
		return qfalse;
	}

	cols = rows = 0;
	for ( shift = 5; shift < 10; shift++ ) {
		cols = ( glConfig.vidWidth + ( 1 << shift ) - 1 ) >> shift;
		rows = ( glConfig.vidHeight + ( 1 << shift ) - 1 ) >> shift;
		if ( cols * rows <= BLOOM_TILE_WORDS * 32 ) {
			break;
		}
	}
	if ( shift == 10 ) {
		return qfalse;
	}

	Com_Memset( bloomMask, 0, sizeof( bloomMask ) );
	bloomShift = shift;
	bloomCols = cols;
	bloomRows = rows;

	data = (const bloomCommand_t *)data + 1;
	while ( 1 ) {
		data = PADP( data, sizeof( void * ) );
		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = (const setColorCommand_t *)data + 1;
			break;
		case RC_STRETCH_PIC:
			pic = (const stretchPicCommand_t *)data;
			if ( pic->shader->numDeforms ) {
				return qfalse;
			}
			R_BloomMarkTiles( pic->x, pic->y, pic->w, pic->h );
			data = pic + 1;
			break;
		case RC_DRAW_TRIS_2D:
			tris = (const drawTris2DCommand_t *)data;
			vert = (const polyVert_t *)( tris + 1 );
			if ( tris->shader->numDeforms ) {
				return qfalse;
			}
			x0 = x1 = vert[0].xyz[0];
			y0 = y1 = vert[0].xyz[1];
			for ( i = 1; i < tris->numVerts; i++ ) {
				if ( vert[i].xyz[0] < x0 ) x0 = vert[i].xyz[0];
				if ( vert[i].xyz[0] > x1 ) x1 = vert[i].xyz[0];
				if ( vert[i].xyz[1] < y0 ) y0 = vert[i].xyz[1];
				if ( vert[i].xyz[1] > y1 ) y1 = vert[i].xyz[1];
			}
			R_BloomMarkTiles( x0, y0, x1 - x0, y1 - y0 );
			data = vert + tris->numVerts;
			break;
		case RC_DRAW_SURFS:
			surfs = (const drawSurfsCommand_t *)data;
			vp = &surfs->viewParms;
			R_BloomMarkTiles( vp->viewportX, glConfig.vidHeight - ( vp->viewportY + vp->viewportHeight ), vp->viewportWidth, vp->viewportHeight );
			data = surfs + 1;
			break;
		case RC_COLORMASK:
			data = (const colorMaskCommand_t *)data + 1;
			break;
		case RC_CLEARDEPTH:
			data = (const clearDepthCommand_t *)data + 1;
			break;
		case RC_BLOOM_SCENE:
		case RC_BLOOM:
			data = (const bloomCommand_t *)data + 1;
			break;
		case RC_SWAP_BUFFERS:
		case RC_END_OF_LIST:
			dirty = 0;
			for ( i = 0; i < BLOOM_TILE_WORDS; i++ ) {
				for ( bits = bloomMask[i]; bits; bits &= bits - 1 ) {
					dirty++;
				}
			}
			if ( dirty * 2 > cols * rows ) {
				return qfalse;
			}
			return ( R_BloomBuildRects( 0, qtrue ) && R_BloomBuildRects( 1, qfalse ) ) ? qtrue : qfalse;
		default:
			return qfalse;
		}
	}
}

const void *RB_Bloom( const void *data ) {
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	if ( r_bloomActive->integer ) {
		vk.bloomRectsReady = R_BloomScanTiles( data );
		vk_ql_bloom();
		vk.bloomRectsReady = qfalse;
	}

	return (const void *)( (const bloomCommand_t *)data + 1 );
}

qboolean R_CaptureGammaCorrect( void ) {
	return !r_colorCorrectActive->integer && tr.overbrightBits > 0 && glConfig.deviceSupportsGamma;
}
