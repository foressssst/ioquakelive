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

backEndData_t	*backEndData;
backEndState_t	backEnd;

const float *GL_Ortho( const float left, const float right, const float bottom, const float top, const float znear, const float zfar )
{
	static float m[ 16 ] = { 0 };

	m[0] = 2.0f / (right - left);
	m[5] = 2.0f / (top - bottom);
	m[10] = - 2.0f / (zfar - znear);
	m[12] = - (right + left)/(right - left);
	m[13] = - (top + bottom) / (top - bottom);
	m[14] = - (zfar + znear) / (zfar - znear);
	m[15] = 1.0f;

	return m;
}

void GL_Bind( image_t *image ) {
	GLuint texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if ( image ) {
			image->frameUsed = tr.frameCount;
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

void GL_BindTexNum( GLuint texnum ) {

	if ( r_nobind->integer && tr.dlightImage ) {
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[ glState.currenttmu ] != texnum ) {
		glState.currenttextures[ glState.currenttmu ] = texnum;
		qglBindTexture( GL_TEXTURE_2D, texnum );
	}
}

void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currenttmu = unit;
}

static void GL_SelectClientTexture( int unit )
{
	if ( glState.currentArray == unit )
	{
		return;
	}

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Error( ERR_DROP, "GL_SelectClientTexture: unit = %i", unit );
	}

	qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currentArray = unit;
}

void GL_BindTexture( int unit, GLuint texnum )
{
	if ( glState.currenttextures[ unit ] != texnum )
	{
		GL_SelectTexture( unit );
		glState.currenttextures[ unit ] = texnum;
		qglBindTexture( GL_TEXTURE_2D, texnum );
	}
}

void GL_Cull( cullType_t cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED )
	{
		qglDisable( GL_CULL_FACE );
	}
	else
	{
		qboolean cullFront;
		qglEnable( GL_CULL_FACE );

		cullFront = (cullType == CT_FRONT_SIDED);
		if ( backEnd.viewParms.portalView == PV_MIRROR )
		{
			cullFront = !cullFront;
		}

		qglCullFace( cullFront ? GL_FRONT : GL_BACK );
	}
}

void GL_TexEnv( GLint env )
{
	if ( env == glState.texEnv[ glState.currenttmu ] )
		return;

	glState.texEnv[ glState.currenttmu ] = env;

	switch ( env )
	{
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed", env );
		break;
	}
}

void GL_State( unsigned stateBits )
{
	unsigned diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	if ( diff & GLS_BLEND_BITS )
	{
		GLenum srcFactor = GL_ONE, dstFactor = GL_ONE;

		if ( stateBits & GLS_BLEND_BITS )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			ri.Error( ERR_DROP, "GL_State: invalid alpha test bits" );
			break;
		}
	}

	glState.glStateBits = stateBits;
}

void GL_ClientState( int unit, unsigned stateBits )
{
	unsigned diff = stateBits ^ glState.glClientStateBits[ unit ];

	if ( diff == 0 )
	{
		if ( stateBits )
		{
			GL_SelectClientTexture( unit );
		}
		return;
	}

	GL_SelectClientTexture( unit );

	if ( diff & CLS_COLOR_ARRAY )
	{
		if ( stateBits & CLS_COLOR_ARRAY )
			qglEnableClientState( GL_COLOR_ARRAY );
		else
			qglDisableClientState( GL_COLOR_ARRAY );
	}

	if ( diff & CLS_NORMAL_ARRAY )
	{
		if ( stateBits & CLS_NORMAL_ARRAY )
			qglEnableClientState( GL_NORMAL_ARRAY );
		else
			qglDisableClientState( GL_NORMAL_ARRAY );
	}

	if ( diff & CLS_TEXCOORD_ARRAY )
	{
		if ( stateBits & CLS_TEXCOORD_ARRAY )
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	glState.glClientStateBits[ unit ] = stateBits;
}

void RB_SetGL2D( void );

static void RB_Hyperspace( void ) {
	color4ub_t c;

	if ( !backEnd.isHyperspace ) {
	}

	if ( tess.shader != tr.whiteShader ) {
		RB_EndSurface();
		RB_SetGL2D();
		RB_BeginSurface( tr.whiteShader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

	if ( r_teleporterFlash->integer == 0 ) {
		c.rgba[0] = c.rgba[1] = c.rgba[2] = 0;
	} else {
		c.rgba[0] = c.rgba[1] = c.rgba[2] = (backEnd.refdef.time & 255);
	}
	c.rgba[3] = 255;

	RB_AddQuadStamp2( backEnd.refdef.x, backEnd.refdef.y, backEnd.refdef.width, backEnd.refdef.height,
		0.0, 0.0, 0.0, 0.0, c );

	RB_EndSurface();

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	backEnd.isHyperspace = qtrue;
}

static void SetViewportAndScissor( void ) {
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode( GL_MODELVIEW );

	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.scissorX, backEnd.viewParms.scissorY,
		backEnd.viewParms.scissorWidth, backEnd.viewParms.scissorHeight );
}

static void RB_BeginDrawingView( void ) {
	int clearBits = 0;

	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish();
		glState.finishCalled = qtrue;
	} else if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	backEnd.projection2D = qfalse;

	SetViewportAndScissor();

	GL_State( GLS_DEFAULT );

	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if ( 0 && r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;
#ifdef _DEBUG
		qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );
#else
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
#endif
	}
	qglClear( clearBits );

	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		RB_Hyperspace();
		backEnd.projection2D = qfalse;
		SetViewportAndScissor();
	} else {
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;

	backEnd.skyRenderedThisView = qfalse;
}

#ifdef USE_PMLIGHT
static void RB_LightingPass( void );
#endif

static void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	int				dlighted;
	qboolean		depthRange, oldDepthRange, isCrosshair, wasCrosshair;
	int				i;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
#ifdef USE_PMLIGHT
	float			oldShaderSort;
#endif
	double			originalTime;

	originalTime = backEnd.refdef.floatTime;

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldSort = MAX_UINT;
#ifdef USE_PMLIGHT
	oldShaderSort = -1;
#endif
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}

		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		if ( ( (oldSort ^ drawSurf->sort ) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL ) {
				RB_EndSurface();
			}
#ifdef USE_PMLIGHT
			#define INSERT_POINT SS_FOG
			if ( backEnd.refdef.numLitSurfs && oldShaderSort < INSERT_POINT && shader->sort >= INSERT_POINT ) {
				if ( depthRange ) {
					qglDepthRange( 0, 1 );
					RB_LightingPass();
					qglDepthRange( 0, 0.3 );
				} else {
					RB_LightingPass();
				}
				oldEntityNum = -1;
			}
			oldShaderSort = shader->sort;
#endif
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = drawSurf->sort;

		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime;

				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
				if ( !R_GetDlightMode() )
#endif
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}
#endif
				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
				if ( !R_GetDlightMode() )
#endif
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
#endif
			}

			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			qglLoadMatrixf( backEnd.or.modelMatrix );

			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange(0, 1);
	}
}

#ifdef USE_PMLIGHT
static void RB_BeginDrawingLitSurfs( void )
{
	backEnd.projection2D = qfalse;

	backEnd.skyRenderedThisView = qfalse;

	SetViewportAndScissor();

	glState.faceCulling = -1;
}

static void RB_RenderLitSurfList( dlight_t* dl ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	qboolean		depthRange, oldDepthRange, isCrosshair, wasCrosshair;
	const litSurf_t	*litSurf;
	unsigned int	oldSort;
	double			originalTime;

	originalTime = backEnd.refdef.floatTime;

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldSort = MAX_UINT;
	depthRange = qfalse;

	tess.dlightUpdateParams = qtrue;

	for ( litSurf = dl->head; litSurf; litSurf = litSurf->next ) {
		if ( litSurf->sort == oldSort ) {
			rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
			continue;
		}

		R_DecomposeLitSort( litSurf->sort, &entityNum, &shader, &fogNum );

		if ( ( (oldSort ^ litSurf->sort) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL ) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = litSurf->sort;

		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime;

				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
			}

			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			R_TransformDlights( 1, dl, &backEnd.or );
			tess.dlightUpdateParams = qtrue;

			qglLoadMatrixf( backEnd.or.modelMatrix );

			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
	}

	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}
}
#endif

void RB_SetGL2D( void ) {

	if ( backEnd.projection2D ) {
		return;
	}

	backEnd.projection2D = qtrue;

	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_State( GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_CLIP_PLANE0 );

	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = (double)backEnd.refdef.time * 0.001;
}

void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty ) {
	int			i, j;
	int			start, end;

	if ( !tr.registered ) {
		return;
	}

	start = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows) {
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	RE_UploadCinematic( w, h, cols, rows, data, client, dirty );

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	tr.cinematicShader->stages[0]->bundle[0].image[0] = tr.scratchImage[client];
	RE_StretchPic( x, y, w, h, 0.5f / cols, 0.5f / rows, 1.0f - 0.5f / cols, 1.0f - 0.5 / rows, tr.cinematicShader->index );
}

void RE_UploadCinematic( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty ) {

	image_t *image;

	if ( !tr.scratchImage[ client ] ) {
		tr.scratchImage[ client ] = R_CreateImage( va( "*scratch%i", client ), NULL, (byte *)data, cols, rows, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE );
		return;
	}

	image = tr.scratchImage[ client ];

	GL_Bind( image );

	if ( cols != image->width || rows != image->height ) {
		image->width = image->uploadWidth = cols;
		image->height = image->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, image->internalFormat, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );
	} else if ( dirty ) {
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
}

static const void *RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D.rgba[0] = cmd->color[0] * 255;
	backEnd.color2D.rgba[1] = cmd->color[1] * 255;
	backEnd.color2D.rgba[2] = cmd->color[2] * 255;
	backEnd.color2D.rgba[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

static const void *RB_StretchPic( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		RB_SetGL2D();
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_FBO
	R_BloomScreen();
#endif

	RB_AddQuadStamp2( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2, backEnd.color2D );

	return (const void *)(cmd + 1);
}

#ifdef USE_PMLIGHT
static void RB_LightingPass( void )
{
	dlight_t	*dl;
	int	i;

#ifdef USE_VBO
	VBO_Flush();

	tess.allowVBO = qfalse;
#endif

	tess.dlightPass = qtrue;

	for ( i = 0; i < backEnd.viewParms.num_dlights; i++ )
	{
		dl = &backEnd.viewParms.dlights[i];
		if ( dl->head )
		{
			tess.light = dl;
			RB_RenderLitSurfList( dl );
		}
	}

	tess.dlightPass = qfalse;

	backEnd.viewParms.num_dlights = 0;
	GL_ProgramDisable();
}
#endif

static void transform_to_eye_space( const vec3_t v, vec3_t v_eye )
{
	const float *m = backEnd.viewParms.world.modelMatrix;
	v_eye[0] = m[0]*v[0] + m[4]*v[1] + m[8 ]*v[2] + m[12];
	v_eye[1] = m[1]*v[0] + m[5]*v[1] + m[9 ]*v[2] + m[13];
	v_eye[2] = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14];
};

static void RB_DebugPolygon( int color, int numPoints, float *points ) {
	vec3_t pa;
	vec3_t pb;
	vec3_t p;
	vec3_t q;
	vec3_t n;
	int i;

	if ( numPoints < 3 ) {
		return;
	}

	transform_to_eye_space( &points[0], pa );
	transform_to_eye_space( &points[3], pb );
	VectorSubtract( pb, pa, p );

	for ( i = 2; i < numPoints; i++ ) {
		transform_to_eye_space( &points[3*i], pb );
		VectorSubtract( pb, pa, q );
		CrossProduct( q, p, n );
		if ( VectorLength( n ) > 1e-5 ) {
			break;
		}
	}

	if ( DotProduct( n, pa ) >= 0 ) {
		return;
	}

	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_2D );

	GL_ClientState( 0, CLS_NONE );
	qglVertexPointer( 3, GL_FLOAT, 0, points );

	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
	qglColor4f( color&1, (color>>1)&1, (color>>2)&1, 1 );
	qglDrawArrays( GL_TRIANGLE_FAN, 0, numPoints );

	qglDepthRange( 0, 0 );
	qglColor4f( 1, 1, 1, 1 );
	qglDrawArrays( GL_LINE_LOOP, 0, numPoints );
	qglDepthRange( 0, 1 );

	qglEnable( GL_TEXTURE_2D );
}

static void RB_DebugGraphics( void ) {

	if ( !r_debugSurface->integer ) {
		return;
	}

	GL_Bind( tr.whiteImage );
	GL_Cull( CT_FRONT_SIDED );

	ri.CM_DrawDebugSurface( RB_DebugPolygon );
}

static const void *RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t *cmd;

	RB_EndSurface();

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_BeginDrawingView();

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( r_drawSun->integer ) {
		RB_DrawSun( 0.1f, tr.sunShader );
	}

	RB_ShadowFinish();

	RB_RenderFlares();

#ifdef USE_PMLIGHT
	if ( backEnd.refdef.numLitSurfs ) {
		RB_BeginDrawingLitSurfs();
		RB_LightingPass();
	}
#endif

#ifdef USE_FBO
	if ( !backEnd.doneSurfaces && tr.needScreenMap ) {
		if ( backEnd.viewParms.frameSceneNum == 1 ) {
			FBO_CopyScreen();
		}
	}
#endif

	RB_DebugGraphics();

	backEnd.doneSurfaces = qtrue;

	return (const void *)(cmd + 1);
}

static const void *RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

#ifdef USE_FBO
	if ( fboEnabled ) {
		FBO_BindMain();
		qglDrawBuffer( GL_COLOR_ATTACHMENT0 );
	} else {
		qglDrawBuffer( cmd->buffer );
	}
#else
	qglDrawBuffer( cmd->buffer );
#endif

	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;
	const vec2_t t[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
	vec3_t v[4];

	RB_SetGL2D();

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, t );

	start = ri.Milliseconds();

	for ( i = 0; i < tr.numImages; i++ ) {
		image = tr.images[ i ];
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );

		VectorSet(v[0],x,y,0);
		VectorSet(v[1],x+w,y,0);
		VectorSet(v[2],x,y+h,0);
		VectorSet(v[3],x+w,y+h,0);

		qglVertexPointer( 3, GL_FLOAT, 0, v );
		qglDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}

static const void *RB_ColorMask( const void *data )
{
	const colorMaskCommand_t *cmd = data;

	qglColorMask( cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3] );

	return (const void *)(cmd + 1);
}

static const void *RB_ClearDepth( const void *data )
{
	const clearDepthCommand_t *cmd = data;

	RB_EndSurface();

	qglClear( GL_DEPTH_BUFFER_BIT );

	return (const void *)(cmd + 1);
}

static const void *RB_ClearColor( const void *data )
{
	const clearColorCommand_t *cmd = data;

	if ( cmd->fullscreen )
	{
		qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}

	if ( cmd->colorMask )
	{
		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	}

	if ( r_fastsky->integer ) {
		qglClearColor( ( ( r_fastSkyColor->integer >> 16 ) & 0xff ) / 255.0f,
			( ( r_fastSkyColor->integer >> 8 ) & 0xff ) / 255.0f,
			( r_fastSkyColor->integer & 0xff ) / 255.0f, 1.0f );
	} else {
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}

	if ( cmd->frontAndBack )
	{
		qglDrawBuffer( GL_FRONT );
		qglClear( GL_COLOR_BUFFER_BIT );
		qglDrawBuffer( GL_BACK );
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	return (const void *)(cmd + 1);
}

#ifdef USE_FBO
static const void *RB_FinishBloom( const void *data )
{
	const finishBloomCommand_t *cmd = data;

	RB_EndSurface();

	if ( fboEnabled )
	{
		if ( blitMSfbo && tr.frameSceneNum == 1 )
		{
			FBO_BlitMS( qfalse );
			blitMSfbo = qfalse;
		}

		if ( r_bloom->integer && !r_bloomActive->integer && qglActiveTextureARB )
		{
			if ( !backEnd.doneBloom && backEnd.doneSurfaces )
			{
				RB_SetGL2D();
				qglColor4f( 1, 1, 1, 1 );
				FBO_Bloom( 0, 0, qfalse );
			}
		}
	}

	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	backEnd.drawConsole = qtrue;

	return (const void *)(cmd + 1);
}
#endif

static const void *RB_SwapBuffers( const void *data ) {

	const swapBuffersCommand_t	*cmd;

	RB_EndSurface();

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( r_showImages->integer && !backEnd.drawConsole ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		qglFinish();
	}

#ifdef USE_FBO
	if ( fboEnabled ) {
		FBO_PostProcess();
	}
#endif

	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	if ( backEnd.screenshotMask && tr.frameCount > 1 ) {
#ifdef USE_FBO
		if ( superSampled ) {
			qglScissor( 0, 0, gls.captureWidth, gls.captureHeight );
			qglViewport( 0, 0, gls.captureWidth, gls.captureHeight );
			FBO_BlitSS();
		}
#endif
		if ( backEnd.screenshotMask & SCREENSHOT_TGA && backEnd.screenshotTGA[0] ) {
			RB_TakeScreenshot( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotTGA );
			if ( !backEnd.screenShotTGAsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotTGA );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_JPG && backEnd.screenshotJPG[0] ) {
			RB_TakeScreenshotJPEG( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotJPG );
			if ( !backEnd.screenShotJPGsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotJPG );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_BMP && ( backEnd.screenshotBMP[0] || ( backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD ) ) ) {
			RB_TakeScreenshotBMP( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotBMP, backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD );
			if ( !backEnd.screenShotBMPsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotBMP );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_AVI ) {
			RB_TakeVideoFrameCmd( &backEnd.vcmd );
		}
		if ( backEnd.screenshotMask & SCREENSHOT_LEVELSHOT ) {
			RB_TakeLevelShot();
		}

		backEnd.screenshotJPG[0] = '\0';
		backEnd.screenshotTGA[0] = '\0';
		backEnd.screenshotBMP[0] = '\0';
		backEnd.screenshotMask = 0;
	}

	if ( r_maxQueuedFrames->integer > 0 && qglFenceSync ) {
		int depth = r_maxQueuedFrames->integer;
		int slot;

		if ( depth > MAX_FRAME_FENCES ) {
			depth = MAX_FRAME_FENCES;
		}
		slot = backEnd.fenceIndex % depth;
		if ( backEnd.frameFences[slot] ) {
			qglClientWaitSync( backEnd.frameFences[slot], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000 );
			qglDeleteSync( backEnd.frameFences[slot] );
		}
		backEnd.frameFences[slot] = qglFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		backEnd.fenceIndex++;
	}

	GLimp_EndFrame();

#ifdef USE_FBO
	FBO_BindMain();
#endif

	backEnd.projection2D = qfalse;
	backEnd.doneBloom = qfalse;
	backEnd.doneSurfaces = qfalse;
	backEnd.drawConsole = qfalse;

	r_anaglyphMode->modified = qfalse;

	return (const void *)(cmd + 1);
}

void RB_ExecuteRenderCommands( const void *data ) {

	backEnd.pc.msec = ri.Milliseconds();

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
#ifdef USE_FBO
		case RC_FINISHBLOOM:
			data = RB_FinishBloom(data);
			break;
#endif
		case RC_BLOOM_SCENE:
			data = RB_BloomScene(data);
			break;
		case RC_BLOOM:
			data = RB_Bloom(data);
			break;
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
		case RC_CLEARCOLOR:
			data = RB_ClearColor(data);
			break;
		case RC_END_OF_LIST:
		default:
			return;
		}
	}
}

void RB_ClearFrameFences( void ) {
	int i;

	for ( i = 0; i < MAX_FRAME_FENCES; i++ ) {
		if ( backEnd.frameFences[i] && qglDeleteSync ) {
			qglDeleteSync( backEnd.frameFences[i] );
		}
		backEnd.frameFences[i] = NULL;
	}
	backEnd.fenceIndex = 0;
}
