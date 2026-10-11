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

static qboolean qlPostReady;

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
	r_colorCorrectActive = ri.Cvar_Get( "r_colorCorrectActive", "0", CVAR_ROM | CVAR_TEMP );
	r_contrast = ri.Cvar_Get( "r_contrast", "1.0", CVAR_ARCHIVE | CVAR_USERSAVE );
	r_enableBloom = ri.Cvar_Get( "r_enableBloom", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_enableColorCorrect = ri.Cvar_Get( "r_enableColorCorrect", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_enablePostProcess = ri.Cvar_Get( "r_enablePostProcess", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE );
	r_floatingPointFBOs = ri.Cvar_Get( "r_floatingPointFBOs", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_postProcessActive = ri.Cvar_Get( "r_postProcessActive", "0", CVAR_ROM | CVAR_TEMP );
}

static int R_BloomPasses( void ) {
	return r_bloomPasses->value > 1.0f ? 2 : 1;
}

static const char *qlPostVertex =
	"varying vec2 var_TexCoords;\n"
	"void main() {\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
	"	var_TexCoords = gl_MultiTexCoord0.st;\n"
	"}\n";

static const char *qlColorCorrectFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform vec4 u_Color;\n"
	"varying vec2 var_TexCoords;\n"
	"void main() {\n"
	"	vec3 color = texture2D(u_TextureMap, var_TexCoords).rgb;\n"
	"	color = pow(color, vec3(u_Color.x));\n"
	"	color *= u_Color.y;\n"
	"	color = mix(vec3(0.5), color, u_Color.z);\n"
	"	gl_FragColor = vec4(color, 1.0);\n"
	"}\n";

static const char *qlBrightPassFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform vec4 u_Color;\n"
	"varying vec2 var_TexCoords;\n"
	"void main() {\n"
	"	vec3 color = texture2D(u_TextureMap, var_TexCoords).rgb;\n"
	"	gl_FragColor = vec4(clamp((color - u_Color.x) / (1.0 - u_Color.x), 0.0, 1.0), 1.0);\n"
	"}\n";

static const char *qlBlurFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform vec2 u_InvTexRes;\n"
	"varying vec2 var_TexCoords;\n"
	"void main() {\n"
	"	vec2 texel = u_InvTexRes;\n"
	"	vec4 result = 0.179335 * texture2D(u_TextureMap, var_TexCoords);\n"
	"	result += 0.235377 * texture2D(u_TextureMap, var_TexCoords - (10.0 / 7.0) * texel);\n"
	"	result += 0.235377 * texture2D(u_TextureMap, var_TexCoords + (10.0 / 7.0) * texel);\n"
	"	result += 0.132400 * texture2D(u_TextureMap, var_TexCoords - (24.0 / 7.0) * texel);\n"
	"	result += 0.132400 * texture2D(u_TextureMap, var_TexCoords + (24.0 / 7.0) * texel);\n"
	"	result += 0.042557 * texture2D(u_TextureMap, var_TexCoords - 5.0 * texel);\n"
	"	result += 0.042557 * texture2D(u_TextureMap, var_TexCoords + 5.0 * texel);\n"
	"	result.a = 1.0;\n"
	"	gl_FragColor = result;\n"
	"}\n";

static const char *qlCombineFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform sampler2D u_LevelsMap;\n"
	"uniform vec4 u_Color;\n"
	"varying vec2 var_TexCoords;\n"
	"const vec3 GREYSCALE = vec3(0.3, 0.59, 0.11);\n"
	"void main() {\n"
	"	vec4 scene = texture2D(u_TextureMap, var_TexCoords);\n"
	"	vec4 bloom = texture2D(u_LevelsMap, var_TexCoords);\n"
	"	bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, u_Color.x);\n"
	"	bloom *= u_Color.z;\n"
	"	scene *= 1.0 - clamp(bloom, 0.0, 1.0);\n"
	"	scene = mix(vec4(dot(scene.rgb, GREYSCALE)), scene, u_Color.y);\n"
	"	gl_FragColor = scene * u_Color.w + bloom;\n"
	"}\n";

static const char *qlCombineBloomOnlyFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform vec4 u_Color;\n"
	"varying vec2 var_TexCoords;\n"
	"const vec3 GREYSCALE = vec3(0.3, 0.59, 0.11);\n"
	"void main() {\n"
	"	vec4 bloom = texture2D(u_TextureMap, var_TexCoords);\n"
	"	bloom = mix(vec4(dot(bloom.rgb, GREYSCALE)), bloom, u_Color.x);\n"
	"	bloom *= u_Color.z;\n"
	"	gl_FragColor = vec4(bloom.rgb, 0.0);\n"
	"}\n";

static GLuint R_QLPostCompile( GLenum type, const char *source, const char *name ) {
	GLuint shader;
	GLint status;

	shader = qglCreateShader( type );
	if ( !shader ) {
		return 0;
	}
	qglShaderSource( shader, 1, &source, NULL );
	qglCompileShader( shader );
	qglGetShaderiv( shader, GL_COMPILE_STATUS, &status );
	if ( !status ) {
		char log[1024];

		qglGetShaderInfoLog( shader, sizeof( log ), NULL, log );
		ri.Printf( PRINT_WARNING, "WARNING: %s did not compile: %s\n", name, log );
		qglDeleteShader( shader );
		return 0;
	}
	return shader;
}

static qboolean R_QLPostLink( qlPostProgram_t *p, const char *fragment, const char *name ) {
	GLuint vs, fs;
	GLint status;

	vs = R_QLPostCompile( GL_VERTEX_SHADER, qlPostVertex, name );
	fs = R_QLPostCompile( GL_FRAGMENT_SHADER, fragment, name );
	if ( !vs || !fs ) {
		if ( vs ) {
			qglDeleteShader( vs );
		}
		if ( fs ) {
			qglDeleteShader( fs );
		}
		return qfalse;
	}

	p->program = qglCreateProgram();
	qglAttachShader( p->program, vs );
	qglAttachShader( p->program, fs );
	qglLinkProgram( p->program );
	qglDeleteShader( vs );
	qglDeleteShader( fs );

	qglGetProgramiv( p->program, GL_LINK_STATUS, &status );
	if ( !status ) {
		char log[1024];

		qglGetProgramInfoLog( p->program, sizeof( log ), NULL, log );
		ri.Printf( PRINT_WARNING, "WARNING: %s did not link: %s\n", name, log );
		qglDeleteProgram( p->program );
		p->program = 0;
		return qfalse;
	}

	p->texture = qglGetUniformLocation( p->program, "u_TextureMap" );
	p->levels = qglGetUniformLocation( p->program, "u_LevelsMap" );
	p->color = qglGetUniformLocation( p->program, "u_Color" );
	p->invTexRes = qglGetUniformLocation( p->program, "u_InvTexRes" );
	return qtrue;
}

static qboolean R_QLPostCreatePrograms( void ) {
	static const struct {
		int index;
		const char *fragment;
		const char *name;
	} wanted[] = {
		{ QLP_COLORCORRECT, NULL, "colorcorrect" },
		{ QLP_BRIGHTPASS, NULL, "brightpass" },
		{ QLP_BLUR, NULL, "blur" },
		{ QLP_COMBINE, NULL, "combine" },
		{ QLP_COMBINE_BLOOM_ONLY, NULL, "combine bloom" },
	};
	const char *sources[QLP_COUNT];
	int i;

	sources[QLP_COLORCORRECT] = qlColorCorrectFragment;
	sources[QLP_BRIGHTPASS] = qlBrightPassFragment;
	sources[QLP_BLUR] = qlBlurFragment;
	sources[QLP_COMBINE] = qlCombineFragment;
	sources[QLP_COMBINE_BLOOM_ONLY] = qlCombineBloomOnlyFragment;

	for ( i = 0; i < QLP_COUNT; i++ ) {
		if ( !R_QLPostLink( &qlPrograms[i], sources[i], wanted[i].name ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static void R_QLPostDeletePrograms( void ) {
	int i;

	for ( i = 0; i < QLP_COUNT; i++ ) {
		if ( qlPrograms[i].program ) {
			qglDeleteProgram( qlPrograms[i].program );
		}
	}
	Com_Memset( qlPrograms, 0, sizeof( qlPrograms ) );
}

static qboolean R_QLPostCreateBuffer( int index, int width, int height ) {
	qglGenTextures( 1, &qlBloomTex[index] );
	GL_BindTexture( 0, qlBloomTex[index] );
	qglTexImage2D( GL_TEXTURE_2D, 0, fboInternalFormat, width, height, 0, fboTextureFormat, fboTextureType, NULL );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	qglGenFramebuffers( 1, &qlBloomFbo[index] );
	FBO_BindTarget( GL_FRAMEBUFFER, qlBloomFbo[index] );
	qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, qlBloomTex[index], 0 );

	qlBloomWidth[index] = width;
	qlBloomHeight[index] = height;

	return qglCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ? qtrue : qfalse;
}

static void R_QLPostDeleteBuffers( void ) {
	int i;

	for ( i = 0; i < QL_BLOOM_BUFFERS; i++ ) {
		if ( qlBloomFbo[i] ) {
			qglDeleteFramebuffers( 1, &qlBloomFbo[i] );
		}
		if ( qlBloomTex[i] ) {
			qglDeleteTextures( 1, &qlBloomTex[i] );
		}
	}
	Com_Memset( qlBloomFbo, 0, sizeof( qlBloomFbo ) );
	Com_Memset( qlBloomTex, 0, sizeof( qlBloomTex ) );
}

void R_QLPostInit( void ) {
	int i, w, h;

	R_QLPostShutdown();

	ri.Cvar_Set( "r_postProcessActive", "0" );
	ri.Cvar_Set( "r_colorCorrectActive", "0" );
	ri.Cvar_Set( "r_bloomActive", "0" );

	if ( !r_enablePostProcess->integer || !fboEnabled || !qglCreateProgram ) {
		return;
	}

	if ( !R_QLPostCreatePrograms() ) {
		R_QLPostDeletePrograms();
		return;
	}

	ri.Cvar_Set( "r_postProcessActive", "1" );
	qlPostReady = qtrue;

	if ( r_enableColorCorrect->integer ) {
		ri.Cvar_Set( "r_colorCorrectActive", "1" );
	}

	if ( !r_enableBloom->integer ) {
		return;
	}

	for ( i = 1; i < QL_BLOOM_BUFFERS; i++ ) {
		if ( i < 5 ) {
			w = glConfig.vidWidth / 2;
			h = glConfig.vidHeight / 2;
		} else if ( R_BloomPasses() == 2 ) {
			w = glConfig.vidWidth / 4;
			h = glConfig.vidHeight / 4;
		} else {
			break;
		}

		if ( !R_QLPostCreateBuffer( i, w > 1 ? w : 1, h > 1 ? h : 1 ) ) {
			ri.Printf( PRINT_ALL, "Bloom Failure - unable to create FBO. Bloom effect disabled\n" );
			R_QLPostDeleteBuffers();
			FBO_BindMain();
			return;
		}
	}

	ri.Cvar_Set( "r_bloomActive", "1" );
	FBO_BindMain();
}

void R_QLPostShutdown( void ) {
	if ( qglCreateProgram ) {
		R_QLPostDeletePrograms();
	}
	if ( qglGenFramebuffers ) {
		R_QLPostDeleteBuffers();
	}
	qlPostReady = qfalse;
}

static void R_QLPostBlit( GLuint srcFbo, int srcW, int srcH, GLuint dstFbo, int dstW, int dstH, GLenum filter ) {
	FBO_BindTarget( GL_READ_FRAMEBUFFER, srcFbo );
	FBO_BindTarget( GL_DRAW_FRAMEBUFFER, dstFbo );
	qglBlitFramebuffer( 0, 0, srcW, srcH, 0, 0, dstW, dstH, GL_COLOR_BUFFER_BIT, filter );
	FBO_BindTarget( GL_FRAMEBUFFER, 0 );
}

static void R_QLPostQuad( int width, int height ) {
	static const vec2_t t[4] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
	static vec3_t v[4];

	v[1][0] = width;
	v[2][1] = height;
	v[3][0] = width;
	v[3][1] = height;

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY );
	qglVertexPointer( 3, GL_FLOAT, 0, v );
	qglTexCoordPointer( 2, GL_FLOAT, 0, t );
	qglDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
}

static void R_QLPostPass( qlProgram_t which, GLuint srcTexture, GLuint dstFbo, int dstWidth, int dstHeight,
	const vec4_t color, const vec2_t invTexRes, GLuint levelsTexture ) {
	const qlPostProgram_t *p = &qlPrograms[which];

	FBO_BindTarget( GL_FRAMEBUFFER, dstFbo );
	qglViewport( 0, 0, dstWidth, dstHeight );
	qglScissor( 0, 0, dstWidth, dstHeight );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0.0f, (float)dstWidth, (float)dstHeight, 0.0f, 0.0f, 1.0f ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_BindTexture( 0, srcTexture );
	if ( levelsTexture ) {
		GL_BindTexture( 1, levelsTexture );
	}

	qglUseProgram( p->program );
	if ( p->texture >= 0 ) {
		qglUniform1i( p->texture, 0 );
	}
	if ( p->levels >= 0 ) {
		qglUniform1i( p->levels, 1 );
	}
	if ( p->color >= 0 && color ) {
		qglUniform4f( p->color, color[0], color[1], color[2], color[3] );
	}
	if ( p->invTexRes >= 0 && invTexRes ) {
		qglUniform2f( p->invTexRes, invTexRes[0], invTexRes[1] );
	}

	R_QLPostQuad( dstWidth, dstHeight );

	qglUseProgram( 0 );
	if ( levelsTexture ) {
		GL_BindTexture( 1, 0 );
		GL_SelectTexture( 0 );
	}
}

void RE_BeginBloomScene( void ) {
	bloomCommand_t *cmd;

	if ( !tr.registered ) {
		return;
	}
	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_BLOOM_SCENE;
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
	const bloomCommand_t *cmd = data;

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	if ( r_bloomActive->integer && fboEnabled ) {
		FBO_BindMain();
	}

	return (const void *)( cmd + 1 );
}

const void *RB_Bloom( const void *data ) {
	const bloomCommand_t *cmd = data;
	GLuint sceneTexture, bloomTexture;
	int bloomIndex;
	vec4_t color;
	vec2_t texel;

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	if ( !qlPostReady || !r_bloomActive->integer || !qlBloomFbo[4] ) {
		return (const void *)( cmd + 1 );
	}

	FBO_ResolveMultisample();
	sceneTexture = FBO_MainTexture();
	if ( !sceneTexture ) {
		return (const void *)( cmd + 1 );
	}

	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	GL_Cull( CT_TWO_SIDED );

	R_QLPostBlit( FBO_MainBuffer(), glConfig.vidWidth, glConfig.vidHeight, qlBloomFbo[1], qlBloomWidth[1], qlBloomHeight[1], GL_NEAREST );
	VectorSet4( color, MAX( r_bloomBrightThreshold->value, 0.0f ), 0.0f, 0.0f, 0.0f );
	R_QLPostPass( QLP_BRIGHTPASS, qlBloomTex[1], qlBloomFbo[2], qlBloomWidth[2], qlBloomHeight[2], color, NULL, 0 );

	texel[0] = 0.0f;
	texel[1] = 1.0f / (float)qlBloomHeight[2];
	R_QLPostPass( QLP_BLUR, qlBloomTex[2], qlBloomFbo[3], qlBloomWidth[3], qlBloomHeight[3], NULL, texel, 0 );
	texel[0] = 1.0f / (float)qlBloomWidth[3];
	texel[1] = 0.0f;
	R_QLPostPass( QLP_BLUR, qlBloomTex[3], qlBloomFbo[4], qlBloomWidth[4], qlBloomHeight[4], NULL, texel, 0 );
	bloomIndex = 4;

	if ( qlBloomFbo[7] ) {
		R_QLPostBlit( qlBloomFbo[4], qlBloomWidth[4], qlBloomHeight[4], qlBloomFbo[5], qlBloomWidth[5], qlBloomHeight[5], GL_LINEAR );
		texel[0] = 0.0f;
		texel[1] = 1.0f / (float)qlBloomHeight[5];
		R_QLPostPass( QLP_BLUR, qlBloomTex[5], qlBloomFbo[6], qlBloomWidth[6], qlBloomHeight[6], NULL, texel, 0 );
		texel[0] = 1.0f / (float)qlBloomWidth[6];
		texel[1] = 0.0f;
		R_QLPostPass( QLP_BLUR, qlBloomTex[6], qlBloomFbo[7], qlBloomWidth[7], qlBloomHeight[7], NULL, texel, 0 );
		bloomIndex = 7;
	}

	bloomTexture = qlBloomTex[bloomIndex];

	color[0] = MAX( r_bloomSaturation->value, 0.0f );
	color[1] = MAX( r_bloomSceneSaturation->value, 0.0f );
	color[2] = MAX( r_bloomIntensity->value, 0.0f );
	color[3] = MAX( r_bloomSceneIntensity->value, 0.0f );

	FBO_BindMain();
	if ( color[1] == 1.0f && color[3] == 1.0f ) {
		GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR );
		R_QLPostPass( QLP_COMBINE_BLOOM_ONLY, bloomTexture, FBO_MainBuffer(), glConfig.vidWidth, glConfig.vidHeight, color, NULL, 0 );
		GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	} else {
		R_QLPostPass( QLP_COMBINE, sceneTexture, FBO_ScratchBuffer(), glConfig.vidWidth, glConfig.vidHeight, color, NULL, bloomTexture );
		FBO_CopyScratchToMain();
	}

	backEnd.projection2D = qfalse;
	backEnd.doneBloom = qtrue;

	return (const void *)( cmd + 1 );
}

qboolean RB_QLColorCorrect( GLuint dstFbo, int width, int height ) {
	GLuint sceneTexture;
	vec4_t color;

	if ( !qlPostReady || !r_colorCorrectActive->integer ) {
		return qfalse;
	}

	FBO_ResolveMultisample();
	sceneTexture = FBO_MainTexture();
	if ( !sceneTexture ) {
		return qfalse;
	}

	if ( tr.frameHasWorld ) {
		color[0] = r_gamma->value > 0.0f ? 1.0f / r_gamma->value : 1.0f;
		color[1] = MAX( 2.0f * r_overBrightBits->integer, 1.0f );
		color[2] = r_contrast->value;
	} else {
		color[0] = 1.0f;
		color[1] = 1.0f / tr.identityLight;
		color[2] = 1.0f;
	}
	color[3] = 1.0f;

	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
	GL_Cull( CT_TWO_SIDED );
	R_QLPostPass( QLP_COLORCORRECT, sceneTexture, dstFbo, width, height, color, NULL, 0 );

	backEnd.projection2D = qfalse;
	return qtrue;
}

qboolean R_CaptureGammaCorrect( void ) {
	return !r_colorCorrectActive->integer && tr.overbrightBits > 0 && glConfig.deviceSupportsGamma;
}
