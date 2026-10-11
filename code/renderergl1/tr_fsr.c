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

cvar_t *r_fsr;
cvar_t *r_fsrSharpness;

typedef enum {
	FSR_EASU,
	FSR_RCAS,
	FSR_COUNT
} fsrProgram_t;

typedef struct {
	GLuint program;
	GLint texture;
	GLint con0;
	GLint con1;
} fsrProgramState_t;

static fsrProgramState_t fsrPrograms[FSR_COUNT];
static GLuint fsrFbo;
static GLuint fsrTex;
static int fsrWidth;
static int fsrHeight;
static qboolean fsrAvailable;

static const char *fsrVertex =
	"varying vec2 var_TexCoords;\n"
	"void main() {\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
	"	var_TexCoords = gl_MultiTexCoord0.st;\n"
	"}\n";

static const char *fsrEasuFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform vec4 u_Con0;\n"
	"uniform vec4 u_Con1;\n"
	"varying vec2 var_TexCoords;\n"
	"\n"
	"float FsrRcp(float x) { return 1.0 / max(x, 1e-30); }\n"
	"float FsrLuma(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }\n"
	"vec3 FsrTap(vec2 fp, vec2 off) {\n"
	"	return texture2D(u_TextureMap, (fp + off + 0.5) * u_Con1.zw).rgb;\n"
	"}\n"
	"\n"
	"void FsrSet(inout vec2 dir, inout float len, float w,\n"
	"		float lA, float lB, float lC, float lD, float lE) {\n"
	"	float dc = lD - lC;\n"
	"	float cb = lC - lB;\n"
	"	float lenX = FsrRcp(max(abs(dc), abs(cb)));\n"
	"	float dirX = lD - lB;\n"
	"	dir.x += dirX * w;\n"
	"	lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);\n"
	"	lenX *= lenX;\n"
	"	len += lenX * w;\n"
	"	float ec = lE - lC;\n"
	"	float ca = lC - lA;\n"
	"	float lenY = FsrRcp(max(abs(ec), abs(ca)));\n"
	"	float dirY = lE - lA;\n"
	"	dir.y += dirY * w;\n"
	"	lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);\n"
	"	lenY *= lenY;\n"
	"	len += lenY * w;\n"
	"}\n"
	"\n"
	"void FsrAccum(inout vec3 aC, inout float aW, vec2 off, vec2 dir, vec2 len2,\n"
	"		float lob, float clp, vec3 c) {\n"
	"	vec2 v;\n"
	"	v.x = (off.x * dir.x) + (off.y * dir.y);\n"
	"	v.y = (off.x * -dir.y) + (off.y * dir.x);\n"
	"	v *= len2;\n"
	"	float d2 = min(v.x * v.x + v.y * v.y, clp);\n"
	"	float wB = (2.0 / 5.0) * d2 - 1.0;\n"
	"	float wA = lob * d2 - 1.0;\n"
	"	wB *= wB;\n"
	"	wA *= wA;\n"
	"	wB = (25.0 / 16.0) * wB - (25.0 / 16.0 - 1.0);\n"
	"	float w = wB * wA;\n"
	"	aC += c * w;\n"
	"	aW += w;\n"
	"}\n"
	"\n"
	"void main() {\n"
	"	vec2 ip = var_TexCoords * u_Con1.xy - 0.5;\n"
	"	vec2 pp = ip * u_Con0.xy + u_Con0.zw;\n"
	"	vec2 fp = floor(pp);\n"
	"	pp -= fp;\n"
	"\n"
	"	vec3 b = FsrTap(fp, vec2( 0.0, -1.0));\n"
	"	vec3 c = FsrTap(fp, vec2( 1.0, -1.0));\n"
	"	vec3 e = FsrTap(fp, vec2(-1.0,  0.0));\n"
	"	vec3 f = FsrTap(fp, vec2( 0.0,  0.0));\n"
	"	vec3 g = FsrTap(fp, vec2( 1.0,  0.0));\n"
	"	vec3 h = FsrTap(fp, vec2( 2.0,  0.0));\n"
	"	vec3 i = FsrTap(fp, vec2(-1.0,  1.0));\n"
	"	vec3 j = FsrTap(fp, vec2( 0.0,  1.0));\n"
	"	vec3 k = FsrTap(fp, vec2( 1.0,  1.0));\n"
	"	vec3 l = FsrTap(fp, vec2( 2.0,  1.0));\n"
	"	vec3 n = FsrTap(fp, vec2( 0.0,  2.0));\n"
	"	vec3 o = FsrTap(fp, vec2( 1.0,  2.0));\n"
	"\n"
	"	float bL = FsrLuma(b), cL = FsrLuma(c), eL = FsrLuma(e), fL = FsrLuma(f);\n"
	"	float gL = FsrLuma(g), hL = FsrLuma(h), iL = FsrLuma(i), jL = FsrLuma(j);\n"
	"	float kL = FsrLuma(k), lL = FsrLuma(l), nL = FsrLuma(n), oL = FsrLuma(o);\n"
	"\n"
	"	vec2 dir = vec2(0.0);\n"
	"	float len = 0.0;\n"
	"	FsrSet(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);\n"
	"	FsrSet(dir, len,        pp.x  * (1.0 - pp.y), cL, fL, gL, hL, kL);\n"
	"	FsrSet(dir, len, (1.0 - pp.x) *        pp.y , fL, iL, jL, kL, nL);\n"
	"	FsrSet(dir, len,        pp.x  *        pp.y , gL, jL, kL, lL, oL);\n"
	"\n"
	"	vec2 dir2 = dir * dir;\n"
	"	float dirR = dir2.x + dir2.y;\n"
	"	bool zro = dirR < (1.0 / 32768.0);\n"
	"	dirR = inversesqrt(max(dirR, 1e-30));\n"
	"	dirR = zro ? 1.0 : dirR;\n"
	"	dir.x = zro ? 1.0 : dir.x;\n"
	"	dir *= dirR;\n"
	"	len = len * 0.5;\n"
	"	len *= len;\n"
	"	float stretch = (dir.x * dir.x + dir.y * dir.y) * FsrRcp(max(abs(dir.x), abs(dir.y)));\n"
	"	vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);\n"
	"	float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;\n"
	"	float clp = FsrRcp(lob);\n"
	"\n"
	"	vec3 min4 = min(min(min(f, g), j), k);\n"
	"	vec3 max4 = max(max(max(f, g), j), k);\n"
	"\n"
	"	vec3 aC = vec3(0.0);\n"
	"	float aW = 0.0;\n"
	"	FsrAccum(aC, aW, vec2( 0.0, -1.0) - pp, dir, len2, lob, clp, b);\n"
	"	FsrAccum(aC, aW, vec2( 1.0, -1.0) - pp, dir, len2, lob, clp, c);\n"
	"	FsrAccum(aC, aW, vec2(-1.0,  1.0) - pp, dir, len2, lob, clp, i);\n"
	"	FsrAccum(aC, aW, vec2( 0.0,  1.0) - pp, dir, len2, lob, clp, j);\n"
	"	FsrAccum(aC, aW, vec2( 0.0,  0.0) - pp, dir, len2, lob, clp, f);\n"
	"	FsrAccum(aC, aW, vec2(-1.0,  0.0) - pp, dir, len2, lob, clp, e);\n"
	"	FsrAccum(aC, aW, vec2( 1.0,  1.0) - pp, dir, len2, lob, clp, k);\n"
	"	FsrAccum(aC, aW, vec2( 2.0,  1.0) - pp, dir, len2, lob, clp, l);\n"
	"	FsrAccum(aC, aW, vec2( 2.0,  0.0) - pp, dir, len2, lob, clp, h);\n"
	"	FsrAccum(aC, aW, vec2( 1.0,  0.0) - pp, dir, len2, lob, clp, g);\n"
	"	FsrAccum(aC, aW, vec2( 1.0,  2.0) - pp, dir, len2, lob, clp, o);\n"
	"	FsrAccum(aC, aW, vec2( 0.0,  2.0) - pp, dir, len2, lob, clp, n);\n"
	"\n"
	"	gl_FragColor = vec4(min(max4, max(min4, aC * FsrRcp(aW))), 1.0);\n"
	"}\n";

static const char *fsrRcasFragment =
	"uniform sampler2D u_TextureMap;\n"
	"uniform vec4 u_Con1;\n"
	"varying vec2 var_TexCoords;\n"
	"\n"
	"float FsrRcp(float x) { return 1.0 / max(x, 1e-30); }\n"
	"float FsrLuma(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }\n"
	"\n"
	"void main() {\n"
	"	vec2 t = u_Con1.zw;\n"
	"	vec3 b = texture2D(u_TextureMap, var_TexCoords + vec2( 0.0, -t.y)).rgb;\n"
	"	vec3 d = texture2D(u_TextureMap, var_TexCoords + vec2(-t.x,  0.0)).rgb;\n"
	"	vec3 e = texture2D(u_TextureMap, var_TexCoords).rgb;\n"
	"	vec3 f = texture2D(u_TextureMap, var_TexCoords + vec2( t.x,  0.0)).rgb;\n"
	"	vec3 h = texture2D(u_TextureMap, var_TexCoords + vec2( 0.0,  t.y)).rgb;\n"
	"\n"
	"	vec3 mn4 = min(min(min(b, d), f), h);\n"
	"	vec3 mx4 = max(max(max(b, d), f), h);\n"
	"	vec3 hitMin = min(mn4, e) / (4.0 * mx4);\n"
	"	vec3 hitMax = (1.0 - max(mx4, e)) / (4.0 * mn4 - 4.0);\n"
	"	vec3 lobeRGB = max(-hitMin, hitMax);\n"
	"	float lobe = max(-(0.25 - 1.0 / 16.0),\n"
	"		min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0)) * u_Con1.x;\n"
	"\n"
	"	float rcpL = FsrRcp(4.0 * lobe + 1.0);\n"
	"	gl_FragColor = vec4((lobe * b + lobe * d + lobe * h + lobe * f + e) * rcpL, 1.0);\n"
	"}\n";

static GLuint R_FSRCompile( GLenum type, const char *source, const char *name ) {
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

static qboolean R_FSRLink( fsrProgramState_t *p, const char *fragment, const char *name ) {
	GLuint vs, fs;
	GLint status;

	vs = R_FSRCompile( GL_VERTEX_SHADER, fsrVertex, name );
	fs = R_FSRCompile( GL_FRAGMENT_SHADER, fragment, name );
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
	p->con0 = qglGetUniformLocation( p->program, "u_Con0" );
	p->con1 = qglGetUniformLocation( p->program, "u_Con1" );
	return qtrue;
}

static void R_FSRDeleteBuffer( void ) {
	if ( fsrFbo ) {
		qglDeleteFramebuffers( 1, &fsrFbo );
		fsrFbo = 0;
	}
	if ( fsrTex ) {
		qglDeleteTextures( 1, &fsrTex );
		fsrTex = 0;
	}
	fsrWidth = fsrHeight = 0;
}

static qboolean R_FSRCreateBuffer( int width, int height ) {
	R_FSRDeleteBuffer();

	qglGenTextures( 1, &fsrTex );
	GL_BindTexture( 0, fsrTex );
	qglTexImage2D( GL_TEXTURE_2D, 0, fboInternalFormat, width, height, 0, fboTextureFormat,
		fboTextureType, NULL );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	qglGenFramebuffers( 1, &fsrFbo );
	FBO_BindTarget( GL_FRAMEBUFFER, fsrFbo );
	qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fsrTex, 0 );

	if ( qglCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		R_FSRDeleteBuffer();
		return qfalse;
	}

	fsrWidth = width;
	fsrHeight = height;
	return qtrue;
}

void R_FSRRegister( void ) {
	r_fsr = ri.Cvar_Get( "r_fsr", "0", CVAR_ARCHIVE_ND );
	RI_Cvar_CheckRange( r_fsr, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_fsr, "FidelityFX Super Resolution 1.0: the game renders smaller than "
		"the window and FSR brings it up to it\n"
		" 0: off\n"
		" 1: Ultra Quality, 77% of the window\n"
		" 2: Quality, 67%\n"
		" 3: Balanced, 59%\n"
		" 4: Performance, 50%\n"
		"With \\r_renderScale, the size of \\r_renderWidth / \\r_renderHeight instead." );
	RI_Cvar_SetGroup( r_fsr, CVG_RENDERER );

	r_fsrSharpness = ri.Cvar_Get( "r_fsrSharpness", "0.2", CVAR_ARCHIVE_ND );
	RI_Cvar_CheckRange( r_fsrSharpness, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_fsrSharpness, "Sharpening of \\r_fsr, 0 the sharpest." );
	RI_Cvar_SetGroup( r_fsrSharpness, CVG_RENDERER );
}

float R_FSRScale( void ) {
	static const float scales[5] = { 1.0f, 1.0f / 1.3f, 1.0f / 1.5f, 1.0f / 1.7f, 1.0f / 2.0f };
	const int level = r_fsr ? r_fsr->integer : 0;

	if ( level < 1 || level > 4 || r_renderScale->integer || !r_fbo->integer ) {
		return 1.0f;
	}
	return scales[level];
}

void R_FSRInit( void ) {
	fsrAvailable = qfalse;

	if ( !qglCreateProgram || !qglUseProgram ) {
		return;
	}

	if ( !R_FSRLink( &fsrPrograms[FSR_EASU], fsrEasuFragment, "FSR upscale" ) ) {
		return;
	}
	if ( !R_FSRLink( &fsrPrograms[FSR_RCAS], fsrRcasFragment, "FSR sharpening" ) ) {
		return;
	}

	fsrAvailable = qtrue;
}

void R_FSRShutdown( void ) {
	int i;

	R_FSRDeleteBuffer();

	for ( i = 0; i < FSR_COUNT; i++ ) {
		if ( fsrPrograms[i].program ) {
			qglDeleteProgram( fsrPrograms[i].program );
		}
	}
	Com_Memset( fsrPrograms, 0, sizeof( fsrPrograms ) );
	fsrAvailable = qfalse;
}

static void R_FSRQuad( int width, int height ) {
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

static void R_FSRSetup( int dstWidth, int dstHeight ) {
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0.0f, (float)dstWidth, (float)dstHeight, 0.0f, 0.0f, 1.0f ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();
}

qboolean RB_FSRBlit( GLuint srcTexture, int srcWidth, int srcHeight,
	int x0, int y0, int x1, int y1 ) {
	const int dstWidth = x1 - x0;
	const int dstHeight = y1 - y0;
	const fsrProgramState_t *p;

	if ( !fsrAvailable || !r_fsr->integer ) {
		return qfalse;
	}
	if ( dstWidth <= srcWidth || dstHeight <= srcHeight ) {
		return qfalse;
	}
	if ( dstWidth != fsrWidth || dstHeight != fsrHeight ) {
		if ( !R_FSRCreateBuffer( dstWidth, dstHeight ) ) {
			return qfalse;
		}
		ri.Printf( PRINT_ALL, "...FSR upscale %ix%i to %ix%i\n", srcWidth, srcHeight,
			dstWidth, dstHeight );
	}

	p = &fsrPrograms[FSR_EASU];
	FBO_BindTarget( GL_FRAMEBUFFER, fsrFbo );
	qglViewport( 0, 0, dstWidth, dstHeight );
	qglScissor( 0, 0, dstWidth, dstHeight );
	R_FSRSetup( dstWidth, dstHeight );
	GL_BindTexture( 0, srcTexture );
	qglUseProgram( p->program );
	if ( p->texture >= 0 ) {
		qglUniform1i( p->texture, 0 );
	}
	if ( p->con0 >= 0 ) {
		qglUniform4f( p->con0, (float)srcWidth / (float)dstWidth, (float)srcHeight / (float)dstHeight,
			0.5f * (float)srcWidth / (float)dstWidth - 0.5f,
			0.5f * (float)srcHeight / (float)dstHeight - 0.5f );
	}
	if ( p->con1 >= 0 ) {
		qglUniform4f( p->con1, (float)dstWidth, (float)dstHeight,
			1.0f / (float)srcWidth, 1.0f / (float)srcHeight );
	}
	R_FSRQuad( dstWidth, dstHeight );

	p = &fsrPrograms[FSR_RCAS];
	FBO_BindTarget( GL_FRAMEBUFFER, 0 );
	qglDrawBuffer( GL_BACK );
	qglViewport( x0, y0, dstWidth, dstHeight );
	qglScissor( x0, y0, dstWidth, dstHeight );
	R_FSRSetup( dstWidth, dstHeight );
	GL_BindTexture( 0, fsrTex );
	qglUseProgram( p->program );
	if ( p->texture >= 0 ) {
		qglUniform1i( p->texture, 0 );
	}
	if ( p->con1 >= 0 ) {
		qglUniform4f( p->con1, powf( 2.0f, -r_fsrSharpness->value ), 0.0f,
			1.0f / (float)dstWidth, 1.0f / (float)dstHeight );
	}
	R_FSRQuad( dstWidth, dstHeight );

	qglUseProgram( 0 );
	return qtrue;
}
