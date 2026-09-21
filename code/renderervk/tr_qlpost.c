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
//
// tr_qlpost.c -- [QL] the colour correction and the bloom, retail's render commands 9 to 11
//

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
cvar_t *r_colorCorrectActive;
cvar_t *r_contrast;
cvar_t *r_enableBloom;
cvar_t *r_enableColorCorrect;
cvar_t *r_enablePostProcess;
cvar_t *r_floatingPointFBOs;
cvar_t *r_postProcessActive;

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
	GLint texture;   // u_TextureMap
	GLint levels;    // u_LevelsMap
	GLint color;     // u_Color
	GLint invTexRes; // u_InvTexRes
} qlPostProgram_t;

static qlPostProgram_t qlPrograms[QLP_COUNT];

// 1 to 4 at half the scene's size, 5 to 7 at a quarter with r_bloomPasses 2
#define QL_BLOOM_BUFFERS 8

static GLuint qlBloomFbo[QL_BLOOM_BUFFERS];
static GLuint qlBloomTex[QL_BLOOM_BUFFERS];
static int qlBloomWidth[QL_BLOOM_BUFFERS];
static int qlBloomHeight[QL_BLOOM_BUFFERS];

/*
===============
R_QLPostRegister

[QL] 0x449000; nothing reads r_bloomBlurFalloff, r_bloomBlurRadius or r_bloomBlurScale
===============
*/
void R_QLPostRegister( void ) {
	r_bloomActive = ri.Cvar_Get( "r_bloomActive", "0", CVAR_ROM | CVAR_TEMP | CVAR_USERSAVE );
	r_bloomBlurFalloff = ri.Cvar_GetRange( "r_bloomBlurFalloff", "0.0", "0.75", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomBlurRadius = ri.Cvar_GetRange( "r_bloomBlurRadius", "5", "1", "12", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomBlurScale = ri.Cvar_GetRange( "r_bloomBlurScale", "0.0", "1.0", "2.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomBrightThreshold = ri.Cvar_GetRange( "r_bloomBrightThreshold", "0.25", "0.0", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomIntensity = ri.Cvar_GetRange( "r_bloomIntensity", "0.5", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	// retail's flags also carry 0x2000, a binary range: see R_BloomPasses
	r_bloomPasses = ri.Cvar_GetRange( "r_bloomPasses", "1", "1", "2", CVAR_ARCHIVE | CVAR_LATCH | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomSaturation = ri.Cvar_GetRange( "r_bloomSaturation", "0.8", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomSceneIntensity = ri.Cvar_GetRange( "r_bloomSceneIntensity", "1.0", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_bloomSceneSaturation = ri.Cvar_GetRange( "r_bloomSceneSaturation", "1.0", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE );
	r_colorCorrectActive = ri.Cvar_Get( "r_colorCorrectActive", "0", CVAR_ROM | CVAR_TEMP );
	r_contrast = ri.Cvar_Get( "r_contrast", "1.0", CVAR_ARCHIVE | CVAR_USERSAVE );
	r_enableBloom = ri.Cvar_Get( "r_enableBloom", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_enableColorCorrect = ri.Cvar_Get( "r_enableColorCorrect", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_enablePostProcess = ri.Cvar_Get( "r_enablePostProcess", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_floatingPointFBOs = ri.Cvar_Get( "r_floatingPointFBOs", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_postProcessActive = ri.Cvar_Get( "r_postProcessActive", "0", CVAR_ROM | CVAR_TEMP );

	// the pipelines bake these, so a change has to rebuild them
	RI_Cvar_SetGroup( r_contrast, CVG_RENDERER );
	RI_Cvar_SetGroup( r_bloomBrightThreshold, CVG_RENDERER );
	RI_Cvar_SetGroup( r_bloomIntensity, CVG_RENDERER );
	RI_Cvar_SetGroup( r_bloomSaturation, CVG_RENDERER );

}

/*
===============
R_QLPostInit

[QL] 0x450710: the colour correction rides the gamma pass, which needs the frame buffer
===============
*/
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

	// the pipelines arrive later, when R_InitImages builds them out of r_bloom
	if ( r_enableBloom->integer ) {
		ri.Cvar_Set( "r_bloomActive", "1" );
	}
}

// the pipelines and the bloom images belong to vk.c, which destroys them
void R_QLPostShutdown( void ) {
}

/*
=============
RE_BeginBloomScene

[QL] refexport 30 (0x43cba0), before the 3D view: queues render command 11, which here has
nothing to do because the scene always lands in our own attachment
=============
*/
void RE_BeginBloomScene( void ) {
}

/*
=============
RE_ApplyBloom

[QL] refexport 31 (0x4384d0), after the 3D view: queues render command 10
=============
*/
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

/*
=============
RB_BloomScene

[QL] render command 11 (0x437a50)
=============
*/
const void *RB_BloomScene( const void *data ) {
	return (const void *)( (const bloomCommand_t *)data + 1 );
}

const void *RB_Bloom( const void *data ) {
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	if ( r_bloomActive->integer ) {
		vk_ql_bloom();
	}

	return (const void *)( (const bloomCommand_t *)data + 1 );
}

/*
=============
R_CaptureGammaCorrect

[QL] 0x4481c0: only when the ramp gave the overbright
=============
*/
qboolean R_CaptureGammaCorrect( void ) {
	return !r_colorCorrectActive->integer && tr.overbrightBits > 0 && glConfig.deviceSupportsGamma;
}
