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

static char *s_shaderText;

static const char *s_extensionOffset;
static int s_extendedShader;

static	shaderStage_t	stages[MAX_SHADER_STAGES];
static	shader_t		shader;
static	texModInfo_t	texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS+1];

#define FILE_HASH_SIZE		1024
static	shader_t*		hashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH		2048
static const char **shaderTextHashTable[MAX_SHADERTEXT_HASH];

#ifdef __GNUCC__
  #warning TODO: check if long is ok here
#endif

#define generateHashValue Com_GenerateHashValue

void RE_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh, *sh2;
	qhandle_t	h;

	sh = R_FindShaderByName( shaderName );
	if (sh == NULL || sh == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(shaderName, 0);
		sh = R_GetShaderByHandle(h);
	}
	if (sh == NULL || sh == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RemapShader: shader %s not found\n", shaderName );
		return;
	}

	sh2 = R_FindShaderByName( newShaderName );
	if (sh2 == NULL || sh2 == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(newShaderName, 0);
		sh2 = R_GetShaderByHandle(h);
	}

	if (sh2 == NULL || sh2 == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RemapShader: new shader %s not found\n", newShaderName );
		return;
	}

	COM_StripExtension(shaderName, strippedName, sizeof(strippedName));
	hash = generateHashValue(strippedName, FILE_HASH_SIZE);
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			if (sh != sh2) {
				sh->remappedShader = sh2;
			} else {
				sh->remappedShader = NULL;
			}
		}
	}

	if ( timeOffset ) {
		sh2->timeOffset = Q_atof( timeOffset );
	}
}

static qboolean ParseVector( const char **text, int count, float *v ) {
	const char	*token;
	int		i;

	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, "(" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = Q_atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, ")" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	return qtrue;
}

static unsigned NameToAFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		return GLS_ATEST_GT_0;
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return GLS_ATEST_LT_80;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return 0;
}

static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_DSTBLEND_ONE;
}

static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return GF_NOISE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return GF_SIN;
}

static void ParseWaveForm( const char **text, waveForm_t *wave )
{
	const char *token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->base = Q_atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->amplitude = Q_atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->phase = Q_atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->frequency = Q_atof( token );
}

static void ParseTexMod( const char *_text, shaderStage_t *stage )
{
	const char *token;
	const char **text = &_text;
	texModInfo_t *tmi;

	if ( stage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
		ri.Error( ERR_DROP, "ERROR: too many tcMod stages in shader '%s'", shader.name );
		return;
	}

	tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
	stage->bundle[0].numTexMods++;

	token = COM_ParseExt( text, qfalse );

	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = Q_atof( token );

		tmi->type = TMOD_TURBULENT;
	}
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = Q_atof( token );
		tmi->type = TMOD_SCALE;
	}
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[0] = Q_atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[1] = Q_atof( token );
		tmi->type = TMOD_SCROLL;
	}
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = Q_atof( token );

		tmi->type = TMOD_STRETCH;
	}
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = Q_atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->rotateSpeed = Q_atof( token );
		tmi->type = TMOD_ROTATE;
	}
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		ri.Printf( PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name );
	}
}

static qboolean ParseStage( shaderStage_t *stage, const char **text )
{
	const char *token;
	int i, depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	qboolean depthMaskExplicit = qfalse;

	stage->active = qfalse;

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no matching '}' found\n" );
			return qfalse;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		else if ( !Q_stricmp( token, "map" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "$whiteimage" ) )
			{
				stage->bundle[0].image[0] = tr.whiteImage;
				continue;
			}
			else if ( !Q_stricmp( token, "$lightmap" ) )
			{
				stage->bundle[0].lightmap = LIGHTMAP_INDEX_SHADER;
				if ( shader.lightmapIndex < 0 || !tr.lightmaps ) {
					stage->bundle[0].image[0] = tr.whiteImage;
				} else {
					stage->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			}
			else if ( Q_stricmpn( token, "*lightmap", 9 ) == 0 && token[9] >= '0' && token[9] <= '9' )
			{
				const int lightmapIndex = atoi( token + 9 );
				if ( lightmapIndex < 0 || tr.lightmaps == NULL ) {
					stage->bundle[0].image[0] = tr.whiteImage;
				} else {
					stage->bundle[0].lightmap = LIGHTMAP_INDEX_OFFSET + lightmapIndex;
					stage->bundle[0].image[0] = tr.lightmaps[lightmapIndex % tr.lightmapMod];
				}
				continue;
			}
			else
			{
				imgFlags_t flags = IMGFLAG_NONE;

				if (!shader.noMipMaps)
					flags |= IMGFLAG_MIPMAP;

				if (!shader.noPicMip)
					flags |= IMGFLAG_PICMIP;

				if (shader.noLightScale)
					flags |= IMGFLAG_NOLIGHTSCALE;

				stage->bundle[0].image[0] = R_FindImageFile( token, flags );

				if ( !stage->bundle[0].image[0] )
				{
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return qfalse;
				}
			}
		}
		else if ( !Q_stricmp( token, "clampmap" ) || ( !Q_stricmp( token, "screenMap" ) && s_extendedShader ) )
		{
			imgFlags_t flags;

			if ( !Q_stricmp( token, "screenMap" ) ) {
				flags = IMGFLAG_NONE;
				if ( vk.fboActive ) {
					stage->bundle[0].isScreenMap = 1;
					shader.hasScreenMap = 1;
				}
			} else {
				flags = IMGFLAG_CLAMPTOEDGE;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for '%s' keyword in shader '%s'\n",
					stage->bundle[0].isScreenMap ? "screenMap" : "clampMap", shader.name );
				return qfalse;
			}

			if (!shader.noMipMaps)
				flags |= IMGFLAG_MIPMAP;

			if (!shader.noPicMip)
				flags |= IMGFLAG_PICMIP;

			if (shader.noLightScale)
				flags |= IMGFLAG_NOLIGHTSCALE;

			stage->bundle[0].image[0] = R_FindImageFile( token, flags );
			if ( !stage->bundle[0].image[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
				return qfalse;
			}
		}
		else if ( !Q_stricmp( token, "animMap" ) )
		{
			int maxAnimations = s_extendedShader ? MAX_IMAGE_ANIMATIONS : MAX_IMAGE_ANIMATIONS_QL;

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle[0].imageAnimationSpeed = Q_atof( token );

			while ( 1 ) {
				int num;

				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				num = stage->bundle[0].numImageAnimations;
				if ( num < maxAnimations ) {
					imgFlags_t flags = IMGFLAG_NONE;

					if (!shader.noMipMaps)
						flags |= IMGFLAG_MIPMAP;

					if (!shader.noPicMip)
						flags |= IMGFLAG_PICMIP;

					if (shader.noLightScale)
						flags |= IMGFLAG_NOLIGHTSCALE;

					stage->bundle[0].image[num] = R_FindImageFile( token, flags );
					if ( !stage->bundle[0].image[num] )
					{
						ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
						return qfalse;
					}
					stage->bundle[0].numImageAnimations++;
				}
			}
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
			int handle;
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			handle = ri.CIN_PlayCinematic( token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader) );
			if ( handle != -1 ) {
				if ( !tr.scratchImage[ handle ] ) {
					tr.scratchImage[ handle ] = R_CreateImage( va( "*scratch%i", handle ), NULL, NULL, 256, 256, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE );
				}
				stage->bundle[0].isVideoMap = qtrue;
				stage->bundle[0].videoMapHandle = handle;
				stage->bundle[0].image[0] = tr.scratchImage[ handle ];
			} else {
				ri.Printf( PRINT_WARNING, "WARNING: could not load '%s' for 'videoMap' keyword in shader '%s'\n", token, shader.name );
			}
		}
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			atestBits = NameToAFunc( token );
		}
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthFuncBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if ( !Q_stricmp( token, "detail" ) )
		{
			stage->isDetail = qtrue;
		}
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if ( token[0] == '\0' )
				{
					ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					blendDstBits = GLS_DSTBLEND_ONE;
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			if ( !depthMaskExplicit ) {
				depthMaskBits = 0;
			}

		}
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->bundle[0].rgbWave );
				stage->bundle[0].rgbGen = CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				vec3_t	color;

				VectorClear( color );

				ParseVector( text, 3, color );
				stage->bundle[0].constantColor.rgba[0] = 255 * color[0];
				stage->bundle[0].constantColor.rgba[1] = 255 * color[1];
				stage->bundle[0].constantColor.rgba[2] = 255 * color[2];

				stage->bundle[0].rgbGen = CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->bundle[0].rgbGen = CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				stage->bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->bundle[0].rgbGen = CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->bundle[0].rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->bundle[0].rgbGen = CGEN_VERTEX;
				if ( stage->bundle[0].alphaGen == AGEN_IDENTITY ) {
					stage->bundle[0].alphaGen = AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->bundle[0].rgbGen = CGEN_EXACT_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				stage->bundle[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->bundle[0].rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->bundle[0].alphaWave );
				stage->bundle[0].alphaGen = AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt( text, qfalse );
				stage->bundle[0].constantColor.rgba[3] = 255 * Q_atof( token );
				stage->bundle[0].alphaGen = AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->bundle[0].alphaGen = AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->bundle[0].alphaGen = AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->bundle[0].alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->bundle[0].alphaGen = AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				stage->bundle[0].alphaGen = AGEN_LIGHTING_SPECULAR;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->bundle[0].alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->bundle[0].alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					ri.Printf( PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = Q_atof( token );

					if ( shader.portalRange < 0.001f )
						shader.portalRangeR = 0.0f;
					else
						shader.portalRangeR = 1.0f / shader.portalRange;
				}
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				const char *t = *text;
				stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
				token = COM_ParseExt( text, qfalse );
				if ( Q_stricmp( token, "firstPerson" ) == 0 )
				{
					stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED_FP;
				}
				else
				{
					*text = t;
				}
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[0] );
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[1] );

				stage->bundle[0].tcGen = TCGEN_VECTOR;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
					break;
				Q_strcat( buffer, sizeof (buffer), token );
				Q_strcat( buffer, sizeof (buffer), " " );
			}

			ParseTexMod( buffer, stage );

			continue;
		}
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;

			continue;
		}
		else if ( !Q_stricmp( token, "depthFragment" ) && s_extendedShader )
		{
			stage->depthFragment = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "dlight" ) && s_extendedShader )
		{
			stage->bundle[0].dlight = 1;
		}
		else if ( !Q_stricmp( token, "depthTest" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for depthTest in shader '%s'\n", shader.name );
				continue;
			}
			if ( !Q_stricmp( token, "disable" ) )
			{
				stage->stateBits |= GLS_DEPTHTEST_DISABLE;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	if ( stage->bundle[0].rgbGen == CGEN_BAD ) {
		if ( blendSrcBits == 0 ||
			blendSrcBits == GLS_SRCBLEND_ONE ||
			blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) {
			stage->bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		} else {
			stage->bundle[0].rgbGen = CGEN_IDENTITY;
		}
	}

	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ZERO ) ) {
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	if ( stage->bundle[0].alphaGen == AGEN_IDENTITY ) {
		if ( stage->bundle[0].rgbGen == CGEN_IDENTITY || stage->bundle[0].rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			stage->bundle[0].alphaGen = AGEN_SKIP;
		}
	}

	if ( depthMaskExplicit && shader.sort == SS_BAD ) {
		if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA  ) {
			if ( stage->bundle[0].alphaGen != AGEN_SKIP ) {
				if ( atestBits == 0 ) {
					depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
				}
			} else {
			}
			shader.sort = shader.polygonOffset ? SS_DECAL : SS_OPAQUE + 0.01f;
		} else if ( blendSrcBits == GLS_SRCBLEND_ZERO && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR && stage->bundle[0].rgbGen == CGEN_EXACT_VERTEX ) {
			depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
			shader.sort = SS_SEE_THROUGH;
		}
	}

	for ( i = 0; i < NUM_TEXTURE_BUNDLES; i++ ) {
		if ( stage->bundle[i].tcGen == TCGEN_BAD ) {
			if ( stage->bundle[i].lightmap != LIGHTMAP_INDEX_NONE ) {
				stage->bundle[i].tcGen = TCGEN_LIGHTMAP;
			} else {
				stage->bundle[i].tcGen = TCGEN_TEXTURE;
			}
		}
	}

	stage->stateBits = depthMaskBits |
		blendSrcBits | blendDstBits |
		atestBits |
		depthFuncBits;

	stage->active = qtrue;

	return qtrue;
}

static void ParseDeform( const char **text ) {
	const char	*token;
	deformStage_t	*ds;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS ) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "projectionShadow" ) ) {
		ds->deformation = DEFORM_PROJECTION_SHADOW;
		return;
	}

	if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE2;
		return;
	}

	if ( !Q_stricmpn( token, "text", 4 ) ) {
		int		n;

		n = token[4] - '0';
		if ( n < 0 || n > 7 ) {
			n = 0;
		}
		ds->deformation = DEFORM_TEXT0 + n;
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = Q_atof( token );

		ds->deformation = DEFORM_BULGE;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) )
	{
		float f;
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		f = Q_atof( token );
		if ( f != 0.0f )
		{
			ds->deformationSpread = 1.0f / f;
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_WAVE;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = Q_atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = Q_atof( token );

		ds->deformation = DEFORM_NORMALS;
		return;
	}

	if ( !Q_stricmp( token, "move" ) ) {
		int		i;

		for ( i = 0 ; i < 3 ; i++ ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
				return;
			}
			ds->moveVector[i] = Q_atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
		return;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}

static void ParseSkyParms( const char **text ) {
	const char		*token;
	static const char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
	char		pathname[MAX_QPATH];
	int			i;
	imgFlags_t imgFlags = IMGFLAG_MIPMAP | IMGFLAG_PICMIP;

	if ( r_neatsky->integer ) {
		imgFlags = IMGFLAG_NONE;
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga"
				, token, suf[i] );
			shader.sky.outerbox[i] = R_FindImageFile( pathname, imgFlags | IMGFLAG_CLAMPTOEDGE );

			if ( !shader.sky.outerbox[i] ) {
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	shader.sky.cloudHeight = Q_atof( token );
	if ( shader.sky.cloudHeight == 0.0 ) {
		shader.sky.cloudHeight = 512.0;
	}
	R_InitSkyTexCoords( shader.sky.cloudHeight );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga"
				, token, suf[i] );
			shader.sky.innerbox[i] = R_FindImageFile( pathname, imgFlags );
			if ( !shader.sky.innerbox[i] ) {
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.isSky = qtrue;
}

static void ParseSort( const char **text ) {
	const char	*token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) ) {
		shader.sort = SS_PORTAL;
	} else if ( !Q_stricmp( token, "sky" ) ) {
		shader.sort = SS_ENVIRONMENT;
	} else if ( !Q_stricmp( token, "opaque" ) ) {
		shader.sort = SS_OPAQUE;
	} else if ( !Q_stricmp( token, "decal" ) ) {
		shader.sort = SS_DECAL;
	} else if ( !Q_stricmp( token, "seeThrough" ) ) {
		shader.sort = SS_SEE_THROUGH;
	} else if ( !Q_stricmp( token, "banner" ) ) {
		shader.sort = SS_BANNER;
	} else if ( !Q_stricmp( token, "additive" ) ) {
		shader.sort = SS_BLEND1;
	} else if ( !Q_stricmp( token, "nearest" ) ) {
		shader.sort = SS_NEAREST;
	} else if ( !Q_stricmp( token, "underwater" ) ) {
		shader.sort = SS_UNDERWATER;
	} else {
		shader.sort = Q_atof( token );
	}
}

typedef struct {
	const char *name;
	int clearSolid, surfaceFlags, contents;
} infoParm_t;

static const infoParm_t infoParms[] = {
	{"water",		1,	0,	CONTENTS_WATER },
	{"slime",		1,	0,	CONTENTS_SLIME },
	{"lava",		1,	0,	CONTENTS_LAVA },
	{"playerclip",	1,	0,	CONTENTS_PLAYERCLIP },
	{"monsterclip",	1,	0,	CONTENTS_MONSTERCLIP },
	{"nodrop",		1,	0,	CONTENTS_NODROP },
	{"nonsolid",	1,	SURF_NONSOLID,	0},
	{"nobotclip",	1,	0,	CONTENTS_BOTCLIP },

	{"origin",		1,	0,	CONTENTS_ORIGIN },
	{"trans",		0,	0,	CONTENTS_TRANSLUCENT },
	{"detail",		0,	0,	CONTENTS_DETAIL },
	{"structural",	0,	0,	CONTENTS_STRUCTURAL },
	{"areaportal",	1,	0,	CONTENTS_AREAPORTAL },
	{"clusterportal", 1,0,  CONTENTS_CLUSTERPORTAL },
	{"donotenter",  1,  0,  CONTENTS_DONOTENTER },

	{"fog",			1,	0,	CONTENTS_FOG},
	{"sky",			0,	SURF_SKY,		0 },
	{"lightfilter",	0,	SURF_LIGHTFILTER, 0 },
	{"alphashadow",	0,	SURF_ALPHASHADOW, 0 },
	{"hint",		0,	SURF_HINT,		0 },

	{"slick",		0,	SURF_SLICK,		0 },
	{"noimpact",	0,	SURF_NOIMPACT,	0 },
	{"nomarks",		0,	SURF_NOMARKS,	0 },
	{"ladder",		0,	SURF_LADDER,	0 },
	{"nodamage",	0,	SURF_NODAMAGE,	0 },
	{"metalsteps",	0,	SURF_METALSTEPS,0 },
	{"flesh",		0,	SURF_FLESH,		0 },
	{"nosteps",		0,	SURF_NOSTEPS,	0 },

	{"nodraw",		0,	SURF_NODRAW,	0 },
	{"pointlight",	0,	SURF_POINTLIGHT, 0 },
	{"nolightmap",	0,	SURF_NOLIGHTMAP,0 },
	{"nodlight",	0,	SURF_NODLIGHT, 0 },
	{"dust",		0,	SURF_DUST, 0},
	{"snowsteps",	0,	SURF_SNOWSTEPS, 0},
	{"woodsteps",	0,	SURF_WOODSTEPS, 0}
};

static void ParseSurfaceParm( const char **text ) {
	const char	*token;
	int		numInfoParms = ARRAY_LEN( infoParms );
	int		i;

	token = COM_ParseExt( text, qfalse );
	for ( i = 0 ; i < numInfoParms ; i++ ) {
		if ( !Q_stricmp( token, infoParms[i].name ) ) {
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if ( infoParms[i].clearSolid ) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			break;
		}
	}
}

static void FinishStage( shaderStage_t *stage )
{
	int i, n;

	if ( !tr.mergeLightmaps ) {
		return;
	}

	for ( i = 0; i < ARRAY_LEN( stage->bundle ); i++ ) {
		textureBundle_t *bundle = &stage->bundle[i];
		if ( bundle->lightmap >= LIGHTMAP_INDEX_OFFSET ) {
			if ( bundle->tcGen == TCGEN_LIGHTMAP ) {
				texModInfo_t *tmi = &bundle->texMods[bundle->numTexMods];
				float x, y;
				const int lightmapIndex = R_GetLightmapCoords( bundle->lightmap - LIGHTMAP_INDEX_OFFSET, &x, &y );
				for ( n = 0; n < bundle->numTexMods; n++ ) {
					tmi = &bundle->texMods[n];
					if ( tmi->type == TMOD_TRANSFORM ) {
						tmi->translate[0] *= tr.lightmapScale[0];
						tmi->translate[1] *= tr.lightmapScale[1];
					}
				}
				bundle->image[0] = tr.lightmaps[lightmapIndex];
				tmi->type = TMOD_OFFSET;
				tmi->offset[0] = x - tr.lightmapOffset[0];
				tmi->offset[1] = y - tr.lightmapOffset[1];
				bundle->numTexMods++;
			}
			continue;
		}
		if ( bundle->lightmap == LIGHTMAP_INDEX_SHADER ) {
			if ( bundle->tcGen != TCGEN_LIGHTMAP ) {
				texModInfo_t *tmi = &bundle->texMods[bundle->numTexMods];
				tmi->type = TMOD_SCALE_OFFSET;
				tmi->scale[0] = tr.lightmapScale[0];
				tmi->scale[1] = tr.lightmapScale[1];
				tmi->offset[0] = tr.lightmapOffset[0];
				tmi->offset[1] = tr.lightmapOffset[1];
				bundle->numTexMods++;
			} else {
				for ( n = 0; n < bundle->numTexMods; n++ ) {
					texModInfo_t *tmi = &bundle->texMods[n];
					if ( tmi->type == TMOD_TRANSFORM ) {
						tmi->translate[0] *= tr.lightmapScale[0];
						tmi->translate[1] *= tr.lightmapScale[1];
					} else {
					}
				}
			}
			continue;
		}
		if ( bundle->lightmap == LIGHTMAP_INDEX_NONE ) {
			if ( bundle->tcGen == TCGEN_LIGHTMAP && shader.lightmapIndex >= 0 ) {
				texModInfo_t *tmi;
				for ( n = bundle->numTexMods; n > 0; --n ) {
					bundle->texMods[n] = bundle->texMods[n - 1];
				}
				tmi = &bundle->texMods[0];
				tmi->type = TMOD_OFFSET_SCALE;
				tmi->offset[0] = -tr.lightmapOffset[0];
				tmi->offset[1] = -tr.lightmapOffset[1];
				tmi->scale[0] = 1.0f / tr.lightmapScale[0];
				tmi->scale[1] = 1.0f / tr.lightmapScale[1];
				bundle->numTexMods++;
			}
		}
	}
}

static qboolean ParseShader( const char **text )
{
	const char *token;
	int numStages;
	qboolean noDefault = qfalse;

	numStages = 0;

	s_extendedShader = (*text >= s_extensionOffset);

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' )
	{
		ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return qfalse;
	}

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name );
			return qfalse;
		}
		if ( token[0] == '}' )
		{
			break;
		}
		else if ( token[0] == '{' )
		{
			if ( numStages >= MAX_SHADER_STAGES ) {
				ri.Printf( PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", shader.name, MAX_SHADER_STAGES );
				return qfalse;
			}

			if ( !ParseStage( &stages[numStages], text ) )
			{
				return qfalse;
			}

			FinishStage( &stages[numStages] );
			numStages++;
			continue;
		}
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "q3map_sun" ) || !Q_stricmp( token, "q3map_sunExt" ) ) {
			float	a, b;

			token = COM_ParseExt( text, qfalse );
			tr.sunLight[0] = Q_atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[1] = Q_atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[2] = Q_atof( token );

			VectorNormalize( tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = Q_atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = Q_atof( token );
			a = a / 180 * M_PI;

			token = COM_ParseExt( text, qfalse );
			b = Q_atof( token );
			b = b / 180 * M_PI;

			tr.sunDirection[0] = cos( a ) * cos( b );
			tr.sunDirection[1] = sin( a ) * cos( b );
			tr.sunDirection[2] = sin( b );

			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] ) {
				shader.clampTime = Q_atof( token );
			}
		}
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "surfaceParm" ) ) {
			ParseSurfaceParm( text );
			continue;
		}
		else if ( !Q_stricmp( token, "nomipmaps" ) )
		{
			shader.noMipMaps = 1;
			shader.noPicMip = 1;
			continue;
		}
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			if ( !r_picmipExplosions->integer || ( Q_stricmp( shader.name, "rocketExplosion" ) &&
				Q_stricmp( shader.name, "grenadeExplosion" ) ) )
			{
				shader.noPicMip = 1;
			}
			continue;
		}
		else if ( !Q_stricmp( token, "novlcollapse" ) )
		{
			shader.noVLcollapse = 1;
			continue;
		}
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			shader.polygonOffset = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "fogParms" ) )
		{
			if ( !ParseVector( text, 3, shader.fogParms.color ) ) {
				return qfalse;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
				continue;
			}
			shader.fogParms.depthForOpaque = Q_atof( token );

			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "portal" ) )
		{
			shader.sort = SS_PORTAL;
			continue;
		}
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			ParseSkyParms( text );
			if ( r_neatsky->integer ) {
				shader.noPicMip = 1;
				shader.noMipMaps = 1;
			}
			continue;
		}
		else if ( !Q_stricmp(token, "light") )
		{
			COM_ParseExt( text, qfalse );
			continue;
		}
		else if ( !Q_stricmp( token, "cull") )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		else if ( !Q_stricmp( token, "nodefault" ) )
		{
			noDefault = qtrue;
			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	if ( numStages == 0 && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG ) && !noDefault ) {
		return qfalse;
	}

	shader.explicitlyDefined = qtrue;

	return qtrue;
}

static void ComputeStageIteratorFunc( void )
{
	if ( shader.isSky )
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
	}
	else
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;
	}
}

#define TEST
#define TEST_A

typedef struct {
	int		blendA;
	int		blendB;

	int		multitextureEnv;
	int		multitextureBlend;
} collapse_t;

static const collapse_t collapse[] = {
	{ 0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		GL_MODULATE, 0 },

	{ 0, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		GL_MODULATE, 0 },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ 0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
		GL_ADD, 0 },

	{ GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
		GL_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE },

	{ GLS_DSTBLEND_ONE | GLS_SRCBLEND_SRC_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_SRC_ALPHA,
		GL_BLEND_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},

	{ GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA,
		GL_BLEND_ONE_MINUS_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},

	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
		GL_BLEND_MIX_ALPHA, 0},

	{ 0, GLS_DSTBLEND_SRC_ALPHA | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA,
		GL_BLEND_MIX_ONE_MINUS_ALPHA, 0},

	{ 0, GLS_DSTBLEND_SRC_ALPHA | GLS_SRCBLEND_DST_COLOR,
		GL_BLEND_DST_COLOR_SRC_ALPHA, 0},

#if 0
	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
		GL_DECAL, 0 },
#endif
	{ -1 }
};

static int CollapseMultitexture( unsigned int st0bits, shaderStage_t *st0, shaderStage_t *st1, int num_stages ) {
	int abits, bbits;
	int i, mtEnv;
	textureBundle_t tmpBundle;
	qboolean nonIdenticalColors;
	qboolean swapLightmap;

#ifndef USE_VULKAN
	if ( !qglActiveTextureARB ) {
		return 0;
	}
#endif

	if ( !st0->active || !st1->active ) {
		return 0;
	}

	if ( st0->depthFragment || (st0->stateBits & GLS_ATEST_BITS) ) {
		return 0;
	}

#ifndef USE_VULKAN
	if ( glConfig.driverType == GLDRV_VOODOO ) {
		if ( st0->bundle[0].image[0]->TMU ==
			 st1->bundle[0].image[0]->TMU ) {
			return 0;
		}
	}
#endif

	abits = st0bits;
	bbits = st1->stateBits;

	if ( ( abits & ~( GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE ) ) !=
		( bbits & ~( GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE ) ) ) {
		return 0;
	}

	abits &= GLS_BLEND_BITS;
	bbits &= GLS_BLEND_BITS;

	for ( i = 0; collapse[i].blendA != -1 ; i++ ) {
		if ( abits == collapse[i].blendA && bbits == collapse[i].blendB ) {
			break;
		}
	}

	if ( collapse[i].blendA == -1 ) {
		return 0;
	}

	mtEnv = collapse[i].multitextureEnv;

	if ( mtEnv == GL_ADD && !glConfig.textureEnvAddAvailable ) {
		return 0;
	}

#ifdef USE_VULKAN
	if ( mtEnv == GL_ADD && st0->bundle[0].rgbGen != CGEN_IDENTITY ) {
		mtEnv = GL_ADD_NONIDENTITY;
	}

	if ( st0->mtEnv && st0->mtEnv != mtEnv ) {
		return 0;
	}
#else
	if ( mtEnv == GL_ADD && st0->rgbGen != CGEN_IDENTITY ) {
		return 0;
	}
#endif

	nonIdenticalColors = qfalse;

	if ( ( st0->bundle[0].rgbGen != st1->bundle[0].rgbGen ) || ( st0->bundle[0].alphaGen != st1->bundle[0].alphaGen ) )
	{
		nonIdenticalColors = qtrue;
	}

	if ( st0->bundle[0].rgbGen == CGEN_WAVEFORM )
	{
		if ( memcmp( &st0->bundle[0].rgbWave, &st1->bundle[0].rgbWave, sizeof( stages[0].bundle[0].rgbWave ) ) )
		{
			nonIdenticalColors = qtrue;
		}
	}

	if ( st0->bundle[0].alphaGen == AGEN_WAVEFORM )
	{
		if ( memcmp( &st0->bundle[0].alphaWave, &st1->bundle[0].alphaWave, sizeof( stages[0].bundle[0].alphaWave ) ) )
		{
			nonIdenticalColors = qtrue;
		}
	}

	if ( nonIdenticalColors )
	{
#ifdef USE_VULKAN
		switch ( mtEnv )
		{
			case GL_ADD:
			case GL_ADD_NONIDENTITY: mtEnv = GL_BLEND_ADD; break;
			case GL_MODULATE: mtEnv = GL_BLEND_MODULATE; break;
		}
#else
		return 0;
#endif
	}

	switch ( mtEnv ) {
		case GL_MODULATE:
		case GL_ADD:
			swapLightmap = qtrue;
			break;
		default:
			swapLightmap = qfalse;
			break;
	}

	if ( swapLightmap && st0->bundle[0].lightmap != LIGHTMAP_INDEX_NONE && !st0->mtEnv )
	{
		tmpBundle = st0->bundle[0];
		st0->bundle[0] = st1->bundle[0];
		st0->bundle[1] = tmpBundle;
	}
	else
	{
#ifdef USE_VULKAN
		if ( st0->mtEnv )
			st0->bundle[2] = st1->bundle[0];
		else
#endif
			st0->bundle[1] = st1->bundle[0];
	}

#ifdef USE_VULKAN
	if ( st0->mtEnv )
	{
		st0->mtEnv3 = mtEnv;
	}
	else
#endif
	{
		st0->stateBits &= ~GLS_BLEND_BITS;
		st0->stateBits |= collapse[i].multitextureBlend;

		st0->mtEnv = mtEnv;
		shader.multitextureEnv = qtrue;
	}

	st0->numTexBundles++;

	if ( num_stages > 2 )
	{
		memmove( st1, st1+1, sizeof( stages[0] ) * ( num_stages - 2 ) );
	}

	Com_Memset( st0 + num_stages - 1, 0, sizeof( stages[0] ) );

#ifdef USE_VULKAN
	if ( vk.maxBoundDescriptorSets >= 8 && num_stages >= 3 && !st0->mtEnv3 )
	{
		if ( mtEnv == GL_BLEND_ONE_MINUS_ALPHA || mtEnv == GL_BLEND_ALPHA || mtEnv == GL_BLEND_MIX_ALPHA || mtEnv == GL_BLEND_MIX_ONE_MINUS_ALPHA || mtEnv == GL_BLEND_DST_COLOR_SRC_ALPHA )
		{
			return 1 + CollapseMultitexture( st0bits, st0, st1, num_stages - 1 );
		}
		if ( abits == 0 )
		{
			return 1 + CollapseMultitexture( st0->stateBits, st0, st1, num_stages - 1 );
		}
	}
#endif

	return 1;
}

#ifdef USE_PMLIGHT

static int tcmodWeight2( const shaderStage_t* st )
{
	int i;

	for ( i = 0; i < st->bundle[0].numTexMods; i++ ) {
		switch ( st->bundle[0].texMods[i].type ) {
		case TMOD_NONE:
		case TMOD_SCALE:
		case TMOD_TRANSFORM:
		case TMOD_OFFSET:
		case TMOD_SCALE_OFFSET:
		case TMOD_OFFSET_SCALE:
			break;
		default:
			return 0;
		}
	}
	return 1;
}

static void FindLightingStage( const int stage ) {
	int i, selected, lightmap, whiteImage;

	for ( i = 0; i < stage; i++ ) {
		if ( stages[i].bundle[0].image[0] == NULL ) {
			continue;
		}
		if ( stages[i].bundle[0].dlight ) {
			shader.lightingStage = i;
			return;
		}
	}

	if ( shader.isSky || (shader.surfaceFlags & (SURF_NODLIGHT | SURF_SKY))  ) {
		return;
	}

	selected = -2;
	lightmap = -2;
	whiteImage = -1;
	for ( i = 0; i < stage; i++ ) {
		const shaderStage_t *st = &stages[i];
		const textureBundle_t *b = &st->bundle[0];
		if ( !st->active ) {
			break;
		}
		if ( b->lightmap != LIGHTMAP_INDEX_NONE ) {
			if ( selected == i - 1 ) {
				break;
			}
			lightmap = i;
			continue;
		}
		if ( b->tcGen != TCGEN_TEXTURE ) {
			continue;
		}
		if ( b->image[0] == tr.whiteImage ) {
			if ( whiteImage < 0 ) {
				whiteImage = i;
			}
			continue;
		}
		if ( selected >= 0 ) {
			if ( st->isDetail ) {
				continue;
			}
			if ( stages[selected].bundle[0].numImageAnimations < b->numImageAnimations ) {
				continue;
			}
			if ( tcmodWeight2( &stages[selected] ) > tcmodWeight2( st ) ) {
				continue;
			}
			if ( ( st->stateBits & GLS_BLEND_BITS ) == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_SRC_COLOR ) ) {
				if ( ( stages[selected].stateBits & GLS_BLEND_BITS ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_SRC_ALPHA ) ) {
					continue;
				}
			}
			if ( ( st->stateBits == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) ) ) {
				if ( stages[selected].stateBits == ( GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_80 ) ) {
					break;
				}
			}
		}
		selected = i;
		if ( i == lightmap + 1 ) {
			break;
		}
	}

	if ( selected < 0  && lightmap >= 0 ) {
		selected = lightmap;
	}

	if ( selected >= 0 ) {
		shader.lightingStage = selected;
		stages[selected].bundle[0].dlight = 1;
	}
}

static void FindLightingBundle( void )
{
	int i, n;

	if ( shader.lightingStage < 0 ) {
		return;
	}

	shader.lightingStage = -1;

	if (  shader.sort == SS_ENVIRONMENT || shader.sort >= SS_FOG ) {
		return;
	}

	for ( i = 0; i < shader.numUnfoggedPasses; i++ ) {
		const shaderStage_t* st = &stages[i];
		if ( !st->active ) {
			break;
		}
		for ( n = 0; n < st->numTexBundles; n++ ) {
			if ( st->bundle[n].dlight ) {
				shader.lightingStage = i;
				shader.lightingBundle = n;
			}
		}
	}
}
#endif

static void FixRenderCommandList( int newShader ) {
	renderCommandList_t	*cmdList = &backEndData->commands;

	if ( cmdList ) {
		const void *curCmd = cmdList->cmds;

		*( (int *)( cmdList->cmds + cmdList->used ) ) = RC_END_OF_LIST;

		while ( 1 ) {
			curCmd = PADP(curCmd, sizeof(void *));

			switch ( *(const int *)curCmd ) {
			case RC_SET_COLOR:
				{
				const setColorCommand_t *sc_cmd = (const setColorCommand_t *)curCmd;
				curCmd = (const void *)(sc_cmd + 1);
				break;
				}
			case RC_STRETCH_PIC:
				{
				const stretchPicCommand_t *sp_cmd = (const stretchPicCommand_t *)curCmd;
				curCmd = (const void *)(sp_cmd + 1);
				break;
				}
			case RC_DRAW_SURFS:
				{
				int i;
				drawSurf_t	*drawSurf;
				shader_t	*sh;
				int			fogNum;
				int			entityNum;
				int			dlightMap;
				int			sortedIndex;
				const drawSurfsCommand_t *ds_cmd =  (const drawSurfsCommand_t *)curCmd;

				for ( i = 0, drawSurf = ds_cmd->drawSurfs; i < ds_cmd->numDrawSurfs; i++, drawSurf++ ) {
					R_DecomposeSort( drawSurf->sort, &entityNum, &sh, &fogNum, &dlightMap );
					sortedIndex = (( drawSurf->sort >> QSORT_SHADERNUM_SHIFT ) & SHADERNUM_MASK);
					if ( sortedIndex >= newShader ) {
						sortedIndex = sh->sortedIndex;
						drawSurf->sort = (sortedIndex << QSORT_SHADERNUM_SHIFT) | (entityNum << QSORT_REFENTITYNUM_SHIFT) | ( fogNum << QSORT_FOGNUM_SHIFT ) | (int)dlightMap;
					}
				}
				curCmd = (const void *)(ds_cmd + 1);
				break;
				}
			case RC_DRAW_BUFFER:
				{
				const drawBufferCommand_t *db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
				}
			case RC_SWAP_BUFFERS:
				{
				const swapBuffersCommand_t *sb_cmd = (const swapBuffersCommand_t *)curCmd;
				curCmd = (const void *)(sb_cmd + 1);
				break;
				}
			case RC_FINISHBLOOM:
				{
				const finishBloomCommand_t *fb_cmd = (const finishBloomCommand_t *)curCmd;
				curCmd = (const void *)(fb_cmd + 1);
				break;
				}
			case RC_COLORMASK:
				{
				const colorMaskCommand_t *cm_cmd = (const colorMaskCommand_t *)curCmd;
				curCmd = (const void *)(cm_cmd + 1);
				break;
				}
			case RC_CLEARDEPTH:
				{
				const clearDepthCommand_t *cd_cmd = (const clearDepthCommand_t *)curCmd;
				curCmd = (const void *)(cd_cmd + 1);
				break;
				}
			case RC_CLEARCOLOR:
				{
				const clearColorCommand_t *cc_cmd = (const clearColorCommand_t *)curCmd;
				curCmd = (const void *)(cc_cmd + 1);
				break;
				}
			case RC_END_OF_LIST:
			default:
				return;
			}
		}
	}
}

static qboolean EqualACgen( const shaderStage_t *st1, const shaderStage_t *st2 )
{
	if ( st1 == NULL || st2 == NULL ) {
		return qfalse;
	}

	if ( st1->bundle[0].adjustColorsForFog != st2->bundle[0].adjustColorsForFog ) {
		return qfalse;
	}

	return qtrue;
}

static qboolean EqualRGBgen( const shaderStage_t *st1, const shaderStage_t *st2 )
{
	if ( st1 == NULL || st2 == NULL ) {
		return qfalse;
	}

	if ( st1->bundle[0].rgbGen != st2->bundle[0].rgbGen || st1->active != st2->active ) {
		return qfalse;
	}

	if ( st1->bundle[0].rgbGen == CGEN_CONST ) {
		if ( st1->bundle[0].constantColor.u32 != st2->bundle[0].constantColor.u32 ) {
			return qfalse;
		}
	}

	if ( st1->bundle[0].rgbGen == CGEN_WAVEFORM ) {
		if ( memcmp( &st1->bundle[0].rgbWave, &st2->bundle[0].rgbWave, sizeof( st1->bundle[0].rgbWave ) ) != 0 ) {
			return qfalse;
		}
	}

	if ( st1->bundle[0].alphaGen != st2->bundle[0].alphaGen ) {
		return qfalse;
	}

	if ( st1->bundle[0].alphaGen == AGEN_CONST ) {
		if ( st1->bundle[0].rgbGen != CGEN_CONST ) {
			if ( st1->bundle[0].constantColor.rgba[3] != st2->bundle[0].constantColor.rgba[3] ) {
				return qfalse;
			}
		}
	}

	if ( st1->bundle[0].alphaGen == AGEN_WAVEFORM ) {
		if ( memcmp( &st1->bundle[0].alphaWave, &st2->bundle[0].alphaWave, sizeof( st1->bundle[0].alphaWave ) ) != 0 ) {
			return qfalse;
		}
	}

	return qtrue;
}

static qboolean EqualTCgen( int bundle, const shaderStage_t *st1, const shaderStage_t *st2 )
{
	const textureBundle_t *b1, *b2;
	int tm;

	if ( st1 == NULL || st2 == NULL )
		return qfalse;

	if ( st1->active != st2->active )
		return qfalse;

	b1 = &st1->bundle[ bundle ];
	b2 = &st2->bundle[ bundle ];

	if ( b1->tcGen != b2->tcGen ) {
		return qfalse;
	}

	if ( b1->tcGen == TCGEN_VECTOR ) {
		if ( memcmp( b1->tcGenVectors, b2->tcGenVectors, sizeof( b1->tcGenVectors ) ) != 0 ) {
			return qfalse;
		}
	}

	if ( b1->numTexMods != b2->numTexMods ) {
		return qfalse;
	}

	for ( tm = 0; tm < b1->numTexMods; tm++ ) {
		const texModInfo_t *tm1 = &b1->texMods[ tm ];
		const texModInfo_t *tm2 = &b2->texMods[ tm ];

		if ( tm1->type != tm2->type ) {
			return qfalse;
		}

		if ( tm1->type == TMOD_TURBULENT || tm1->type == TMOD_STRETCH ) {
			if ( memcmp( &tm1->wave, &tm2->wave, sizeof( tm1->wave ) ) != 0 ) {
				return qfalse;
			}
			continue;
		}

		if ( tm1->type == TMOD_SCROLL ) {
			if ( memcmp( tm1->scroll, tm2->scroll, sizeof( tm1->scroll ) ) != 0 ) {
				return qfalse;
			}
			continue;
		}

		if ( tm1->type == TMOD_SCALE ) {
			if ( memcmp( tm1->scale, tm2->scale, sizeof( tm1->scale ) ) != 0 ) {
				return qfalse;
			}
			continue;
		}

		if ( tm1->type == TMOD_OFFSET ) {
			if ( memcmp( tm1->offset, tm2->offset, sizeof( tm1->offset ) ) != 0 ) {
				return qfalse;
			}
			continue;
		}

		if ( tm1->type == TMOD_SCALE_OFFSET ) {
			if ( memcmp( tm1->scale, tm2->scale, sizeof( tm1->scale ) ) != 0 ) {
				return qfalse;
			}
			if ( memcmp( tm1->offset, tm2->offset, sizeof( tm1->offset ) ) != 0 ) {
				return qfalse;
			}
			continue;
		}

		if ( tm1->type == TMOD_TRANSFORM ) {
			if ( memcmp( tm1->matrix, tm2->matrix, sizeof( tm1->matrix ) ) != 0 ) {
				return qfalse;
			}
			if ( memcmp( tm1->translate, tm2->translate, sizeof( tm1->translate ) ) != 0 ) {
				return qfalse;
			}
			continue;
		}

		if ( tm1->type == TMOD_ROTATE && tm1->rotateSpeed != tm2->rotateSpeed ) {
			return qfalse;
		}
	}

	return qtrue;
}

static void SortNewShader( void ) {
	int		i;
	float	sort;
	shader_t	*newShader;

	newShader = tr.shaders[ tr.numShaders - 1 ];
	sort = newShader->sort;

	for ( i = tr.numShaders - 2 ; i >= 0 ; i-- ) {
		if ( tr.sortedShaders[ i ]->sort <= sort ) {
			break;
		}
		tr.sortedShaders[i+1] = tr.sortedShaders[i];
		tr.sortedShaders[i+1]->sortedIndex++;
	}

	FixRenderCommandList( i+1 );

	newShader->sortedIndex = i+1;
	tr.sortedShaders[i+1] = newShader;
}

static shader_t *GeneratePermanentShader( void ) {
	shader_t	*newShader;
	int			i, b;
	int			size, hash;

	if ( tr.numShaders >= MAX_SHADERS ) {
		ri.Printf( PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	newShader = ri.Hunk_Alloc( sizeof( shader_t ), h_low );

	*newShader = shader;

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for ( i = 0 ; i < newShader->numUnfoggedPasses ; i++ ) {
		if ( !stages[i].active ) {
			break;
		}
		newShader->stages[i] = ri.Hunk_Alloc( sizeof( stages[i] ), h_low );
		*newShader->stages[i] = stages[i];

		for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
			size = newShader->stages[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			if ( size ) {
				newShader->stages[i]->bundle[b].texMods = ri.Hunk_Alloc( size, h_low );
				Com_Memcpy( newShader->stages[i]->bundle[b].texMods, stages[i].bundle[b].texMods, size );
			}
		}
	}

	SortNewShader();

	hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = hashTable[hash];
	hashTable[hash] = newShader;

	return newShader;
}

static void VertexLightingCollapse( void ) {
	int		stage;
	shaderStage_t	*bestStage;
	int		bestImageRank;
	int		rank;

	if ( shader.sort == SS_OPAQUE ) {

		bestStage = &stages[0];
		bestImageRank = -999999;

		for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
			shaderStage_t *pStage = &stages[stage];

			if ( !pStage->active ) {
				break;
			}
			rank = 0;

			if ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE ) {
				rank -= 100;
			}
			if ( pStage->bundle[0].tcGen != TCGEN_TEXTURE ) {
				rank -= 5;
			}
			if ( pStage->bundle[0].numTexMods ) {
				rank -= 5;
			}
			if ( pStage->bundle[0].rgbGen != CGEN_IDENTITY && pStage->bundle[0].rgbGen != CGEN_IDENTITY_LIGHTING ) {
				rank -= 3;
			}

			if ( rank > bestImageRank  ) {
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		stages[0].bundle[0] = bestStage->bundle[0];
		stages[0].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		stages[0].stateBits |= GLS_DEPTHMASK_TRUE;
		if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
			stages[0].bundle[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		} else {
			stages[0].bundle[0].rgbGen = CGEN_EXACT_VERTEX;
		}
		stages[0].bundle[0].alphaGen = AGEN_SKIP;
	} else {
		if ( stages[0].bundle[0].lightmap != LIGHTMAP_INDEX_NONE ) {
			stages[0] = stages[1];
		}

		if ( stages[0].bundle[0].rgbGen == CGEN_ONE_MINUS_ENTITY || stages[1].bundle[0].rgbGen == CGEN_ONE_MINUS_ENTITY ) {
			stages[0].bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].bundle[0].rgbGen == CGEN_WAVEFORM && stages[0].bundle[0].rgbWave.func == GF_SAWTOOTH )
			&& ( stages[1].bundle[0].rgbGen == CGEN_WAVEFORM && stages[1].bundle[0].rgbWave.func == GF_INVERSE_SAWTOOTH ) ) {
			stages[0].bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].bundle[0].rgbGen == CGEN_WAVEFORM && stages[0].bundle[0].rgbWave.func == GF_INVERSE_SAWTOOTH )
			&& ( stages[1].bundle[0].rgbGen == CGEN_WAVEFORM && stages[1].bundle[0].rgbWave.func == GF_SAWTOOTH ) ) {
			stages[0].bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		Com_Memset( pStage, 0, sizeof( *pStage ) );
	}
}

static void InitShader( const char *name, int lightmapIndex ) {
	int i;

	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );

	Q_strncpyz( shader.name, name, sizeof( shader.name ) );
	shader.lightmapIndex = lightmapIndex;

	shader.lightmapSearchIndex = shader.lightmapIndex;

	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].bundle[0].texMods = texMods[i];
	}

#ifdef USE_PMLIGHT
	shader.lightingBundle = 0;
	shader.lightingStage = -1;
#endif
}

static void DetectNeeds( void )
{
	int i, n;

	for ( i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if ( !stages[i].active )
			break;

		for ( n = 0; n < NUM_TEXTURE_BUNDLES; n++ ) {
			const texCoordGen_t t = stages[i].bundle[n].tcGen;
			if ( t == TCGEN_LIGHTMAP )
			{
				shader.needsST2 = qtrue;
			} 
			if ( t == TCGEN_ENVIRONMENT_MAPPED || t == TCGEN_ENVIRONMENT_MAPPED_FP )
			{
				shader.needsNormal = qtrue;
			}
			if ( stages[i].bundle[n].alphaGen == AGEN_LIGHTING_SPECULAR || stages[i].bundle[n].rgbGen == CGEN_LIGHTING_DIFFUSE )
			{
				shader.needsNormal = qtrue;
			}
		}
#if 0
		t1 = stages[i].bundle[0].tcGen;
		t2 = stages[i].bundle[1].tcGen;

		if ( t1 == TCGEN_LIGHTMAP || t2 == TCGEN_LIGHTMAP )
		{
			shader.needsST2 = qtrue;
		}
		if ( t1 == TCGEN_ENVIRONMENT_MAPPED || t1 == TCGEN_ENVIRONMENT_MAPPED_FP )
		{
			shader.needsNormal = qtrue;
		}
		if ( t2 == TCGEN_ENVIRONMENT_MAPPED || t2 == TCGEN_ENVIRONMENT_MAPPED_FP )
		{
			shader.needsNormal = qtrue;
		}
		if ( stages[i].bundle[0].alphaGen == AGEN_LIGHTING_SPECULAR || stages[i].bundle[0].rgbGen == CGEN_LIGHTING_DIFFUSE )
		{
			shader.needsNormal = qtrue;
		}
#endif
	}
	for ( i = 0; i < shader.numDeforms; i++ )
	{
		if ( shader.deforms[i].deformation == DEFORM_WAVE || shader.deforms[i].deformation == DEFORM_NORMALS || shader.deforms[i].deformation == DEFORM_BULGE ) {
			shader.needsNormal = qtrue;
		}
		if ( shader.deforms[i].deformation >= DEFORM_TEXT0 && shader.deforms[i].deformation <= DEFORM_TEXT7 ) {
			shader.needsNormal = qtrue;
		}
	}
}

static shader_t *FinishShader( void ) {
	int			stage, i, n, m;
	qboolean	hasLightmapStage;
	qboolean	vertexLightmap;
	qboolean	colorBlend;
	qboolean	depthMask;
	qboolean	fogCollapse;
	shaderStage_t *lastStage[NUM_TEXTURE_BUNDLES];

	hasLightmapStage = qfalse;
	vertexLightmap = qfalse;
	colorBlend = qfalse;
	depthMask = qfalse;
	fogCollapse = qfalse;

	if ( shader.isSky ) {
		shader.sort = SS_ENVIRONMENT;
	}

	if ( shader.polygonOffset && shader.sort == SS_BAD ) {
		shader.sort = SS_DECAL;
	}

	for ( stage = 0; stage < MAX_SHADER_STAGES; ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		if ( pStage->bundle[0].image[0] == NULL ) {
			ri.Printf( PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name );
			pStage->active = qfalse;
			stage++;
			continue;
		}

		if ( pStage->isDetail && !r_detailTextures->integer )
		{
			int index;

			for(index = stage + 1; index < MAX_SHADER_STAGES; index++)
			{
				if(!stages[index].active)
					break;
			}

			if(index < MAX_SHADER_STAGES)
				memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage));
			else
			{
				if(stage + 1 < MAX_SHADER_STAGES)
					memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage - 1));

				Com_Memset(&stages[index - 1], 0, sizeof(*stages));
			}

			continue;
		}

		if ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE ) {
			if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
				pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			hasLightmapStage = qtrue;
		} else {
			if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
				pStage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
		}

		if ( pStage->stateBits & GLS_DEPTHMASK_TRUE ) {
			depthMask = qtrue;
		}

		if ( ( pStage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
			 ( stages[0].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ) {
			int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			if ( ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE ) ) ||
				( ( blendSrcBits == GLS_SRCBLEND_ZERO ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) ) {
				pStage->bundle[0].adjustColorsForFog = ACFF_MODULATE_RGB;
			}
			else if ( ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->bundle[0].adjustColorsForFog = ACFF_MODULATE_ALPHA;
			}
			else if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->bundle[0].adjustColorsForFog = ACFF_MODULATE_RGBA;
			} else {
			}

			colorBlend = qtrue;
		}

		stage++;
	}

	if ( shader.sort == SS_BAD ) {
		if ( colorBlend ) {
			if ( depthMask ) {
				shader.sort = SS_SEE_THROUGH;
			} else {
				shader.sort = SS_BLEND0;
			}
		} else {
			shader.sort = SS_OPAQUE;
		}
	}

	if ( stage > 1 && !shader.noVLcollapse && r_vertexLight->integer && !r_uiFullScreen->integer ) {
		VertexLightingCollapse();
		stage = 1;
		hasLightmapStage = qfalse;
	}

	DetectNeeds();

	for ( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		shaderStage_t *pStage = &stages[ i ];
		if ( !pStage->active )
			break;
		if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY && pStage->bundle[0].alphaGen == AGEN_IDENTITY )
			pStage->bundle[0].alphaGen = AGEN_SKIP;
		else if ( pStage->bundle[0].rgbGen == CGEN_CONST && pStage->bundle[0].alphaGen == AGEN_CONST )
			pStage->bundle[0].alphaGen = AGEN_SKIP;
		else if ( pStage->bundle[0].rgbGen == CGEN_VERTEX && pStage->bundle[0].alphaGen == AGEN_VERTEX )
			pStage->bundle[0].alphaGen = AGEN_SKIP;
	}

	if ( stage > 1 && stages[0].bundle[0].image[0] == tr.whiteImage && stages[0].bundle[0].numImageAnimations <= 1 && stages[0].bundle[0].rgbGen == CGEN_IDENTITY && stages[0].bundle[0].alphaGen == AGEN_SKIP ) {
		if ( stages[1].stateBits == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO ) ) {
			stages[1].stateBits = stages[0].stateBits & ( GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE | GLS_DEPTHFUNC_EQUAL );
#ifdef USE_PMLIGHT
			stages[1].bundle[0].dlight |= stages[0].bundle[0].dlight;
#endif
			memmove( &stages[0], &stages[1], sizeof( stages[0] ) * ( stage - 1 ) );
			stages[stage - 1].active = qfalse;
			stage--;
		}
	}

	if ( stage > 1 ) {
		if ( stages[0].bundle[0].rgbGen == CGEN_IDENTITY && stages[0].bundle[0].alphaGen == AGEN_SKIP && stages[1].bundle[0].alphaGen == AGEN_SKIP ) {
			if ( ( stages[1].stateBits & GLS_BLEND_BITS ) == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO ) ) {
				if ( stages[1].bundle[0].image[0] == tr.whiteImage && !stages[1].bundle[0].dlight ) {
					stages[0].bundle[0].rgbGen = stages[1].bundle[0].rgbGen;
					if ( stage > 2 ) {
						memmove( &stages[1], &stages[2], sizeof( stages[0] ) * ( stage - 2 ) );
					}
					stages[stage - 1].active = qfalse;
					stage--;
				}
			}
		}
	}

	for ( i = 0; i < stage; i++ ) {
		stages[ i ].numTexBundles = 1;
	}

#ifdef USE_PMLIGHT
	FindLightingStage( stage );
#endif

	if ( r_ext_multitexture->integer ) {
		for ( i = 0; i < stage-1; i++ ) {
			stage -= CollapseMultitexture( stages[i+0].stateBits,  &stages[i+0], &stages[i+1], stage-i );
		}
	}

	if ( shader.lightmapIndex >= 0 && !hasLightmapStage ) {
		if ( vertexLightmap ) {
			ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has VERTEX forced lightmap!\n", shader.name );
		} else {
			ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name );
			shader.lightmapIndex = LIGHTMAP_NONE;
		}
	}

	shader.numUnfoggedPasses = stage;

	if ( stage == 0 && !shader.isSky )
		shader.sort = SS_FOG;

	if ( shader.sort <= SS_OPAQUE ) {
		shader.fogPass = FP_EQUAL;
	} else if ( shader.contentFlags & CONTENTS_FOG ) {
		shader.fogPass = FP_LE;
	}

#ifdef USE_VULKAN

#ifdef USE_FOG_COLLAPSE
	if ( vk.maxBoundDescriptorSets >= 6 && !(shader.contentFlags & CONTENTS_FOG) && shader.fogPass != FP_NONE ) {
		fogCollapse = qtrue;
		if ( stage == 1 ) {
		} else {
			if ( tr.numFogs ) {
				for ( i = 0; i < stage; i++ ) {
					const uint32_t blendBits = stages[i].stateBits & GLS_BLEND_BITS;
					switch ( blendBits & GLS_SRCBLEND_BITS ) {
					case GLS_SRCBLEND_DST_COLOR:
					case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
					case GLS_SRCBLEND_DST_ALPHA:
					case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
						fogCollapse = qfalse;
						break;
					}
					switch ( blendBits & GLS_DSTBLEND_BITS ) {
						case GLS_DSTBLEND_DST_ALPHA:
						case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
						fogCollapse = qfalse;
						break;
					}
				}
				if ( fogCollapse ) {
					for ( i = 1; i < stage; i++ ) {
						const uint32_t blendBits = stages[i].stateBits & GLS_BLEND_BITS;
						if ( blendBits == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) || blendBits == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) ) {
							if ( stages[i].bundle[0].adjustColorsForFog == ACFF_NONE ) {
								fogCollapse = qfalse;
								break;
							}
						}
					}
				}
				if ( fogCollapse ) {
					for ( i = 1; i < stage; i++ ) {
						const uint32_t blendBits = stages[i].stateBits & GLS_BLEND_BITS;
						if ( blendBits == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE) ) {
							stages[i].bundle[0].adjustColorsForFog = ACFF_MODULATE_RGBA;
						}
					}
				}
			}
		}
	}
	if ( tr.numFogs == 0 ) {
		fogCollapse = qtrue;
	}
#endif

	shader.tessFlags = TESS_XYZ;

	{
		Vk_Pipeline_Def def;

		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = shader.cullType;
		def.polygon_offset = shader.polygonOffset;

		if ( (stages[0].stateBits & GLS_DEPTHMASK_TRUE) == 0 ) {
			def.allow_discard = 1;
		}

		for ( i = 0; i < stage; i++ ) {
			int env_mask;
			shaderStage_t *pStage = &stages[i];
			def.state_bits = pStage->stateBits;

			if ( pStage->mtEnv3 ) {
				switch ( pStage->mtEnv3 ) {
					case GL_MODULATE:
						pStage->tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_MULTI_TEXTURE_MUL3;
						break;
					case GL_ADD:
						pStage->tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_MULTI_TEXTURE_ADD3_1_1;
						break;
					case GL_ADD_NONIDENTITY:
						pStage->tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_MULTI_TEXTURE_ADD3;
						break;

					case GL_BLEND_MODULATE:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_MUL;
						break;
					case GL_BLEND_ADD:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_ADD;
						break;
					case GL_BLEND_ALPHA:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_ALPHA;
						break;
					case GL_BLEND_ONE_MINUS_ALPHA:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_ONE_MINUS_ALPHA;
						break;
					case GL_BLEND_MIX_ONE_MINUS_ALPHA:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA;
						break;
					case GL_BLEND_MIX_ALPHA:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_MIX_ALPHA;
						break;
					case GL_BLEND_DST_COLOR_SRC_ALPHA:
						pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2;
						def.shader_type = TYPE_BLEND3_DST_COLOR_SRC_ALPHA;
						break;

					default:
						break;
				}
			}
			else {
				switch ( pStage->mtEnv ) {
				case GL_MODULATE:
					pStage->tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_MULTI_TEXTURE_MUL2;
					if ( ( pStage->bundle[0].adjustColorsForFog == ACFF_NONE && pStage->bundle[1].adjustColorsForFog == ACFF_NONE ) || fogCollapse ) {
						if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY && pStage->bundle[1].rgbGen == CGEN_IDENTITY ) {
							if ( pStage->bundle[1].alphaGen == AGEN_SKIP && pStage->bundle[0].alphaGen == AGEN_SKIP ) {
								pStage->tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = TYPE_MULTI_TEXTURE_MUL2_IDENTITY;
							}
						}
						else if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY_LIGHTING && pStage->bundle[1].rgbGen == CGEN_IDENTITY_LIGHTING && pStage->bundle[0].alphaGen == pStage->bundle[1].alphaGen ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP || pStage->bundle[0].alphaGen == AGEN_IDENTITY ) {
								pStage->tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage->bundle[0].alphaGen == AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
					}
					break;
				case GL_ADD:
					pStage->tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_MULTI_TEXTURE_ADD2_1_1;
					if ( ( pStage->bundle[0].adjustColorsForFog == ACFF_NONE && pStage->bundle[1].adjustColorsForFog == ACFF_NONE ) || fogCollapse ) {
						if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY && pStage->bundle[1].rgbGen == CGEN_IDENTITY ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP && pStage->bundle[1].alphaGen == AGEN_SKIP ) {
								pStage->tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = TYPE_MULTI_TEXTURE_ADD2_IDENTITY;
							}
						}
						else if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY_LIGHTING && pStage->bundle[1].rgbGen == CGEN_IDENTITY_LIGHTING && pStage->bundle[0].alphaGen == pStage->bundle[1].alphaGen ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP || pStage->bundle[0].alphaGen == AGEN_IDENTITY ) {
								pStage->tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage->bundle[0].alphaGen == AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
					}
					break;
				case GL_ADD_NONIDENTITY:
					pStage->tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_MULTI_TEXTURE_ADD2;
					if ( ( pStage->bundle[0].adjustColorsForFog == ACFF_NONE && pStage->bundle[1].adjustColorsForFog == ACFF_NONE ) || fogCollapse ) {
						if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY && pStage->bundle[1].rgbGen == CGEN_IDENTITY ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP && pStage->bundle[1].alphaGen == AGEN_SKIP ) {
								pStage->tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = TYPE_MULTI_TEXTURE_ADD2_IDENTITY;
							}
						}
						else if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY_LIGHTING && pStage->bundle[1].rgbGen == CGEN_IDENTITY_LIGHTING && pStage->bundle[0].alphaGen == pStage->bundle[1].alphaGen ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP || pStage->bundle[0].alphaGen == AGEN_IDENTITY ) {
								pStage->tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage->bundle[0].alphaGen == AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
					}
					break;
				case GL_BLEND_MODULATE:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_MUL;
					break;
				case GL_BLEND_ADD:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_ADD;
					break;
				case GL_BLEND_ALPHA:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_ALPHA;
					break;
				case GL_BLEND_ONE_MINUS_ALPHA:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_ONE_MINUS_ALPHA;
					break;
				case GL_BLEND_MIX_ALPHA:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_MIX_ALPHA;
					break;
				case GL_BLEND_MIX_ONE_MINUS_ALPHA:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_MIX_ONE_MINUS_ALPHA;
					break;
				case GL_BLEND_DST_COLOR_SRC_ALPHA:
					pStage->tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = TYPE_BLEND2_DST_COLOR_SRC_ALPHA;
					break;

				default:
					pStage->tessFlags = TESS_RGBA0 | TESS_ST0;
					def.shader_type = TYPE_SIGNLE_TEXTURE;
					if ( pStage->bundle[0].adjustColorsForFog == ACFF_NONE || fogCollapse ) {
						if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP ) {
								pStage->tessFlags = TESS_ST0;
								def.shader_type = TYPE_SIGNLE_TEXTURE_IDENTITY;
							}
						}
						else if ( pStage->bundle[0].rgbGen == CGEN_IDENTITY_LIGHTING ) {
							if ( pStage->bundle[0].alphaGen == AGEN_SKIP || pStage->bundle[0].alphaGen == AGEN_IDENTITY ) {
								pStage->tessFlags = TESS_ST0;
								def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage->bundle[0].alphaGen == AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
						else if ( pStage->bundle[0].rgbGen == CGEN_ENTITY ) {
							if ( pStage->bundle[0].alphaGen == AGEN_ENTITY || pStage->bundle[0].alphaGen == AGEN_SKIP || pStage->bundle[0].alphaGen == AGEN_IDENTITY ) {
								pStage->tessFlags = TESS_ST0 | TESS_ENT0;
								def.shader_type = TYPE_SIGNLE_TEXTURE_ENT_COLOR;
							}
						}
					}
					break;
				}
			}

			for ( env_mask = 0, n = 0; n < pStage->numTexBundles; n++ ) {
				if ( pStage->bundle[n].numTexMods ) {
					continue;
				}
				if ( pStage->bundle[n].tcGen == TCGEN_ENVIRONMENT_MAPPED && ( pStage->bundle[n].lightmap == LIGHTMAP_INDEX_NONE || !tr.mergeLightmaps ) ) {
					env_mask |= (1 << n);
				}
			}

			if ( env_mask == 1 && !pStage->depthFragment ) {
				if ( def.shader_type >= TYPE_GENERIC_BEGIN && def.shader_type <= TYPE_GENERIC_END ) {
					def.shader_type++;
					shader.tessFlags |= TESS_NNN | TESS_VPOS;
					pStage->tessFlags &= ~TESS_ST0;
					pStage->tessFlags |= TESS_ENV;
					pStage->bundle[0].tcGen = TCGEN_BAD;
				}
			}

			def.mirror = qfalse;
			pStage->vk_pipeline[0] = vk_find_pipeline_ext( 0, &def, qtrue );
			def.mirror = qtrue;
			pStage->vk_mirror_pipeline[0] = vk_find_pipeline_ext( 0, &def, qfalse );

			if ( pStage->depthFragment ) {
				def.mirror = qfalse;
				def.shader_type = TYPE_SIGNLE_TEXTURE_DF;
				pStage->vk_pipeline_df = vk_find_pipeline_ext( 0, &def, qtrue );
				def.mirror = qtrue;
				def.shader_type = TYPE_SIGNLE_TEXTURE_DF;
				pStage->vk_mirror_pipeline_df = vk_find_pipeline_ext( 0, &def, qfalse );
			}

#ifdef USE_FOG_COLLAPSE
			if ( fogCollapse && tr.numFogs > 0 ) {
				Vk_Pipeline_Def def;
				Vk_Pipeline_Def def_mirror;

				vk_get_pipeline_def( pStage->vk_pipeline[0], &def );
				vk_get_pipeline_def( pStage->vk_mirror_pipeline[0], &def_mirror );

				def.fog_stage = 1;
				def_mirror.fog_stage = 1;
				def.acff = pStage->bundle[0].adjustColorsForFog;
				def_mirror.acff = pStage->bundle[0].adjustColorsForFog;

				pStage->vk_pipeline[1] = vk_find_pipeline_ext( 0, &def, qfalse );
				pStage->vk_mirror_pipeline[1] = vk_find_pipeline_ext( 0, &def_mirror, qfalse );

				pStage->bundle[0].adjustColorsForFog = ACFF_NONE;

				shader.fogCollapse = qtrue;
			}
#endif
		}
	}
#endif

#ifdef USE_PMLIGHT
	FindLightingBundle();
#endif

#if 1
	Com_Memset( lastStage, 0, sizeof( lastStage ) );
	for ( i = 0; i < shader.numUnfoggedPasses - 1; i++ ) {
		if ( !stages[ i+1 ].active )
			break;
		for ( n = 0; n < NUM_TEXTURE_BUNDLES; n++ ) {
			if ( stages[ i ].bundle[ n ].image[ 0 ] != NULL ) {
				lastStage[ n ] = &stages[ i ];
			}
			if ( EqualTCgen( n, lastStage[ n ], &stages[ i+1 ] ) && (lastStage[n]->tessFlags & (TESS_ST0 << n) ) ) {
				stages[ i+1 ].tessFlags &= ~(TESS_ST0 << n);
			}
			if ( EqualRGBgen( lastStage[n], &stages[ i+1 ] ) && EqualACgen( lastStage[n], &stages[ i+1 ] ) && (lastStage[n]->tessFlags & (TESS_RGBA0 << n) ) ) {
				stages[ i+1 ].tessFlags &= ~(TESS_RGBA0 << n);
			}
		}
	}
#endif

	for ( i = 0; i < shader.numUnfoggedPasses; i++ ) {
		if ( !stages[i].active ) {
			continue;
		}
		for ( n = 0; n < stages[i].numTexBundles; n++ ) {
			for ( m = 0; m < stages[i].bundle[n].numTexMods; m++ ) {
				if ( stages[i].bundle[n].texMods[m].type == TMOD_STRETCH ) {
					if ( fabsf( stages[i].bundle[n].texMods[m].wave.amplitude ) < 1e-6f ) {
						if ( stages[i].bundle[n].texMods[m].wave.amplitude >= 0.0f ) {
							stages[i].bundle[n].texMods[m].wave.amplitude = 1e-6f;
						} else {
							stages[i].bundle[n].texMods[m].wave.amplitude = -1e-6f;
						}
					}
				}
			}
		}
	}

	ComputeStageIteratorFunc();

	return GeneratePermanentShader();
}

static const char *FindShaderInShaderText( const char *shadername ) {

	const char *token, *p;

	int i, hash;

	hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

	if(shaderTextHashTable[hash])
	{
		for (i = 0; shaderTextHashTable[hash][i]; i++)
		{
			p = shaderTextHashTable[hash][i];
			token = COM_ParseExt(&p, qtrue);
			if(!Q_stricmp(token, shadername))
				return p;
		}
	}

	return NULL;
}

shader_t *R_FindShaderByName( const char *name ) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ( (name==NULL) || (name[0] == 0) ) {
		return tr.defaultShader;
	}

	COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	for (sh=hashTable[hash]; sh; sh=sh->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			return sh;
		}
	}

	return tr.defaultShader;
}

static void R_CreateDefaultShading( image_t *image ) {
	if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_BY_VERTEX ) {
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = CGEN_EXACT_VERTEX;
		stages[0].bundle[0].alphaGen = AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_2D ) {
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = CGEN_VERTEX;
		stages[0].bundle[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			GLS_SRCBLEND_SRC_ALPHA |
			GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if ( shader.lightmapIndex == LIGHTMAP_WHITEIMAGE ) {
		stages[0].active = qtrue;
		stages[0].bundle[0].image[0] = image;
		stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = GLS_DEFAULT;
	} else {
		stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[0].bundle[0].lightmap = LIGHTMAP_INDEX_SHADER;
		stages[0].active = qtrue;
		stages[0].bundle[0].tcGen = TCGEN_LIGHTMAP;
		stages[0].bundle[0].rgbGen = CGEN_IDENTITY;
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].bundle[0].tcGen = TCGEN_TEXTURE;
		stages[1].bundle[0].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}
}

int64_t	r_shaderFindUs, r_shaderCachedUs;
int		r_shaderFindCount, r_shaderCachedCount;

static shader_t *R_FindShaderInner( const char *name, int lightmapIndex, qboolean mipRawImage );

shader_t *R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage ) {
	int64_t		start = ri.Microseconds();
	int			before = tr.numShaders;
	shader_t	*found = R_FindShaderInner( name, lightmapIndex, mipRawImage );
	int64_t		cost = ri.Microseconds() - start;

	r_shaderFindUs += cost;
	r_shaderFindCount++;
	if ( tr.numShaders == before ) {
		r_shaderCachedUs += cost;
		r_shaderCachedCount++;
	}
	return found;
}

static shader_t *R_FindShaderInner( const char *name, int lightmapIndex, qboolean mipRawImage ) {
	char		strippedName[MAX_QPATH];
	unsigned long hash;
	const char	*shaderText;
	image_t		*image;
	shader_t	*sh;

	if ( name[0] == '\0' ) {
		return tr.defaultShader;
	}

	if ( lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps ) {
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	} else if ( lightmapIndex < LIGHTMAP_2D ) {
		ri.Printf( PRINT_WARNING, "WARNING: shader '%s' has invalid lightmap index of %d\n", name, lightmapIndex  );
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}

	COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	for (sh = hashTable[hash]; sh; sh = sh->next) {
		if ( (sh->lightmapSearchIndex == lightmapIndex || sh->defaultShader) &&	!Q_stricmp(sh->name, strippedName)) {
			return sh;
		}
	}

	InitShader( strippedName, lightmapIndex );

	shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText ) {
		if ( r_printShaders->integer ) {
			ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
		}

		if ( !ParseShader( &shaderText ) ) {
			shader.defaultShader = qtrue;
		}

		return FinishShader();
	}

	{
		imgFlags_t flags;

		flags = IMGFLAG_NONE;

		if (mipRawImage)
		{
			flags |= IMGFLAG_MIPMAP | IMGFLAG_PICMIP;
		}
		else
		{
			flags |= IMGFLAG_CLAMPTOEDGE;
		}

		image = R_FindImageFile( name, flags );
		if ( !image ) {
			ri.Printf( PRINT_DEVELOPER, "Couldn't find image file for shader %s\n", name );
			shader.defaultShader = qtrue;
			return FinishShader();
		}
	}

	R_CreateDefaultShading( image );

	return FinishShader();
}

qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage) {
	unsigned long hash;
	shader_t	*sh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	if ( lightmapIndex >= tr.numLightmaps ) {
		lightmapIndex = LIGHTMAP_WHITEIMAGE;
	}

	for (sh=hashTable[hash]; sh; sh=sh->next) {
		if ( (sh->lightmapSearchIndex == lightmapIndex || sh->defaultShader) && !Q_stricmp(sh->name, name)) {
			return sh->index;
		}
	}

	InitShader( name, lightmapIndex );

	R_CreateDefaultShading( image );

	sh = FinishShader();

	return sh->index;
}

qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmapIndex, qtrue );

	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

qhandle_t RE_RegisterShader( const char *name ) {
	shader_t	*sh;

	if ( !name ) {
		ri.Printf( PRINT_ALL, "NULL shader\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qtrue );

	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

qhandle_t RE_RegisterShaderNoMip( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qfalse );

	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	if ( hShader < 0 ) {
	  ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	if ( hShader >= tr.numShaders ) {
		ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

void	R_ShaderList_f (void) {
	int			i;
	int			count;
	const shader_t *sh;

	ri.Printf (PRINT_ALL, "-----------------------\n");

	count = 0;
	for ( i = 0 ; i < tr.numShaders ; i++ ) {
		if ( ri.Cmd_Argc() > 1 ) {
			sh = tr.sortedShaders[i];
		} else {
			sh = tr.shaders[i];
		}

		ri.Printf( PRINT_ALL, "%i ", sh->numUnfoggedPasses );

		if ( sh->lightmapIndex >= 0 ) {
			ri.Printf (PRINT_ALL, "L ");
		} else {
			ri.Printf (PRINT_ALL, "  ");
		}
		if ( sh->multitextureEnv ) {
			ri.Printf( PRINT_ALL, "MT(x) " );
		} else {
			ri.Printf( PRINT_ALL, "      " );
		}
		if ( sh->explicitlyDefined ) {
			ri.Printf( PRINT_ALL, "E " );
		} else {
			ri.Printf( PRINT_ALL, "  " );
		}

		if ( sh->optimalStageIteratorFunc == RB_StageIteratorGeneric ) {
			ri.Printf( PRINT_ALL, "gen " );
		} else if ( sh->optimalStageIteratorFunc == RB_StageIteratorSky ) {
			ri.Printf( PRINT_ALL, "sky " );
		} else {
			ri.Printf( PRINT_ALL, "    " );
		}

		if ( sh->defaultShader ) {
			ri.Printf( PRINT_ALL, ": %s (DEFAULTED)\n", sh->name );
		} else {
			ri.Printf( PRINT_ALL, ": %s\n", sh->name );
		}
		count++;
	}
	ri.Printf (PRINT_ALL, "%i total shaders\n", count);
	ri.Printf (PRINT_ALL, "------------------\n");
}

#define	MAX_SHADER_FILES 16384

static int loadShaderBuffers( char **shaderFiles, const int numShaderFiles, char **buffers )
{
	char filename[MAX_QPATH+8];
	char shaderName[MAX_QPATH];
	const char *p, *token;
	long summand, sum = 0;
	int shaderLine;
	int i;
	const char *shaderStart;
	qboolean denyErrors;

	for ( i = 0; i < numShaderFiles; i++ )
	{
		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
		summand = ri.FS_ReadFile( filename, (void **)&buffers[i] );

		if ( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );

		if ( summand == 35910 && strcmp( shaderFiles[i], "sky.shader" ) == 0 )
		{
			if ( memcmp( buffers[i] + 0x3D3E, "\tcloudparms ", 12 ) == 0 )
			{
				memcpy( buffers[i] + 0x27D7, "/*", 2 );
				memcpy( buffers[i] + 0x2A93, "*/", 2 );

				memcpy( buffers[i] + 0x3CA9, "/*", 2 );
				memcpy( buffers[i] + 0x3FC2, "*/", 2 );
			}
		}
		else if ( summand == 116073 && strcmp( shaderFiles[i], "sfx.shader" ) == 0 )
		{
			if ( memcmp( buffers[i] + 93457, "textures/sfx/xfinalfog\r\n", 24 ) == 0 )
			{
				memcpy( buffers[i] + 93457, "/*", 2 );
				memcpy( buffers[i] + 93663, "*/", 2 );
			}
		}

		p = buffers[i];
		COM_BeginParseSession( filename );

		shaderStart = NULL;
		denyErrors = qfalse;

		while ( 1 )
		{
			token = COM_ParseExt( &p, qtrue );

			if ( !*token )
				break;

			Q_strncpyz( shaderName, token, sizeof( shaderName ) );
			shaderLine = COM_GetCurrentParseLine();

			token = COM_ParseExt( &p, qtrue );
			if ( token[0] != '{' || token[1] != '\0' )
			{
				ri.Printf( PRINT_DEVELOPER, "File %s: shader \"%s\" " \
					"on line %d missing opening brace", filename, shaderName, shaderLine );
				if ( token[0] )
					ri.Printf( PRINT_DEVELOPER, " (found \"%s\" on line %d)\n", token, COM_GetCurrentParseLine() );
				else
					ri.Printf( PRINT_DEVELOPER, "\n" );

				if ( denyErrors || !p )
				{
					ri.Printf( PRINT_WARNING, "Ignoring entire file '%s' due to error.\n", filename );
					ri.FS_FreeFile( buffers[i] );
					buffers[i] = NULL;
					break;
				}

				SkipRestOfLine( &p );
				shaderStart = p;
				continue;
			}

			if ( !SkipBracedSection( &p, 1 ) )
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" " \
					"on line %d missing closing brace.\n", filename, shaderName, shaderLine );
				ri.FS_FreeFile( buffers[i] );
				buffers[i] = NULL;
				break;
			}

			denyErrors = qtrue;
		}

		if ( buffers[ i ] ) {
			if ( shaderStart ) {
				summand -= (shaderStart - buffers[i]);
				if ( summand >= 0 ) {
					memmove( buffers[i], shaderStart, summand + 1 );
				}
			}
			sum += COM_Compress( buffers[ i ] );
		}
	}

	return sum;
}

static void ScanAndLoadShaderFiles( void )
{
	char **shaderFiles, **shaderxFiles;
	char *buffers[MAX_SHADER_FILES];
	char *xbuffers[MAX_SHADER_FILES];
	int numShaderFiles, numShaderxFiles;
	int i;
	const char *token, *hashMem;
	char *textEnd;
	const char *p, *oldp;
	int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size, pass;

	long sum = 0;

	shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );

	if ( 1 ) {
		shaderxFiles = ri.FS_ListFiles( "scripts", ".shaderx", &numShaderxFiles );
	} else {
		shaderxFiles = NULL;
		numShaderxFiles = 0;
	}

	if ( (!shaderFiles || !numShaderFiles) && (!shaderxFiles || !numShaderxFiles) ) {
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES ) {
		numShaderFiles = MAX_SHADER_FILES;
	}
	if ( numShaderxFiles > MAX_SHADER_FILES ) {
		numShaderxFiles = MAX_SHADER_FILES;
	}

	sum = 0;
	sum += loadShaderBuffers( shaderxFiles, numShaderxFiles, xbuffers );
	sum += loadShaderBuffers( shaderFiles, numShaderFiles, buffers );

	s_shaderText = ri.Hunk_Alloc( sum + numShaderxFiles*2 + numShaderFiles*2 + 1, h_low );
	s_shaderText[ 0 ] = s_shaderText[ sum + numShaderxFiles*2 + numShaderFiles*2 ] = '\0';

	textEnd = s_shaderText;

	for ( i = numShaderFiles - 1; i >= 0 ; i-- ) {
		if ( buffers[ i ] ) {
			textEnd = Q_stradd( textEnd, buffers[ i ] );
			textEnd = Q_stradd( textEnd, "\n" );
			ri.FS_FreeFile( buffers[ i ] );
		}
	}

	s_extensionOffset = textEnd;

	for ( i = numShaderxFiles - 1; i >= 0 ; i-- ) {
		if ( xbuffers[ i ] ) {
			textEnd = Q_stradd( textEnd, xbuffers[ i ] );
			textEnd = Q_stradd( textEnd, "\n" );
			ri.FS_FreeFile( xbuffers[ i ] );
		}
	}

	if ( shaderxFiles )
		ri.FS_FreeFileList( shaderxFiles );
	if ( shaderFiles )
		ri.FS_FreeFileList( shaderFiles );

	Com_Memset( shaderTextHashTableSizes, 0, sizeof( shaderTextHashTableSizes ) );
	size = 0;

	p = s_shaderText;
	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}
		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection(&p, 0);
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = ri.Hunk_Alloc( size * sizeof(char *), h_low );

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++) {
		shaderTextHashTable[i] = (const char **) hashMem;
		hashMem = ((char *) hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	Com_Memset( shaderTextHashTableSizes, 0, sizeof( shaderTextHashTableSizes ) );
	for ( pass = 0; pass < 2; pass++ ) {
		p = s_shaderText;
		while ( 1 ) {
			oldp = p;
			token = COM_ParseExt( &p, qtrue );
			if ( token[0] == 0 ) {
				break;
			}
			if ( ( oldp >= s_extensionOffset ) == ( pass == 0 ) ) {
				hash = generateHashValue( token, MAX_SHADERTEXT_HASH );
				shaderTextHashTable[hash][shaderTextHashTableSizes[hash]++] = (char*)oldp;
			}
			SkipBracedSection( &p, 0 );
		}
	}
}

static void CreateInternalShaders( void ) {
	tr.numShaders = 0;

	InitShader( "<default>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();

	InitShader( "<white>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.whiteImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].bundle[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.whiteShader = FinishShader();

	InitShader( "<stencil shadow>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	shader.sort = SS_STENCIL_SHADOW;
	tr.shadowShader = FinishShader();

	InitShader( "<cinematic>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE;
	tr.cinematicShader = FinishShader();
}

static void CreateExternalShaders( void ) {
	tr.projectionShadowShader = R_FindShader( "projectionShadow", LIGHTMAP_NONE, qtrue );
	tr.flareShader = R_FindShader( "flareShader", LIGHTMAP_NONE, qtrue );

	if(!tr.flareShader->defaultShader)
	{
		int index;

		for(index = 0; index < tr.flareShader->numUnfoggedPasses; index++)
		{
			tr.flareShader->stages[index]->bundle[0].adjustColorsForFog = ACFF_NONE;
			tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
		}
	}

	tr.sunShader = R_FindShader( "sun", LIGHTMAP_NONE, qtrue );
}

void R_InitShaders( void ) {
	ri.Printf( PRINT_ALL, "Initializing Shaders\n" );

	Com_Memset(hashTable, 0, sizeof(hashTable));

	CreateInternalShaders();

	ScanAndLoadShaderFiles();

	CreateExternalShaders();
}
