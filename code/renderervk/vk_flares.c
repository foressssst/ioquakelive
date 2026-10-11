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

typedef struct flare_s {
	struct		flare_s	*next;

	int			addedFrame;
	uint32_t	testCount;

	portalView_t portalView;
	int			frameSceneNum;
	void		*surface;
	int			fogNum;

	int			fadeTime;

	qboolean	visible;
	float		drawIntensity;

	int			windowX, windowY;
	float		eyeZ;
	float		drawZ;

	vec3_t		origin;
	vec3_t		color;
} flare_t;

static flare_t	r_flareStructs[ MAX_FLARES ];
static flare_t	*r_activeFlares, *r_inactiveFlares;

void R_ClearFlares( void ) {
	int		i;

	if ( !vk.fragmentStores )
		return;

	Com_Memset( r_flareStructs, 0, sizeof( r_flareStructs ) );
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for ( i = 0 ; i < MAX_FLARES ; i++ ) {
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}

static flare_t *R_SearchFlare( void *surface )
{
	flare_t *f;

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			return f;
		}
	}

	return NULL;
}

void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {
	int				i;
	flare_t			*f;
	vec3_t			local;
	float			d = 1;
	vec4_t			eye, clip, normalized, window;

	backEnd.pc.c_flareAdds++;

	if ( normal && (normal[0] || normal[1] || normal[2] ) )	{
		VectorSubtract( backEnd.viewParms.or.origin, point, local );
		VectorNormalizeFast( local );
		d = DotProduct( local, normal );
		if ( d < 0 ) {
			return;
		}
	}

	R_TransformModelToClip( point, backEnd.or.modelMatrix, backEnd.viewParms.projectionMatrix, eye, clip );

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return;
		}
	}

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth || window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return;
	}

	f = R_SearchFlare( surface );

	if ( !f ) {
		if ( !r_inactiveFlares ) {
			return;
		}
		f = r_inactiveFlares;
		r_inactiveFlares = r_inactiveFlares->next;
		f->next = r_activeFlares;
		r_activeFlares = f;

		f->surface = surface;
		f->frameSceneNum = backEnd.viewParms.frameSceneNum;
		f->portalView = backEnd.viewParms.portalView;
		f->visible = qfalse;
		f->fadeTime = backEnd.refdef.time - 2000;
		f->testCount = 0;
	} else {
		++f->testCount;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy( point, f->origin );
	VectorCopy( color, f->color );

	VectorScale( f->color, d, f->color );

	f->windowX = backEnd.viewParms.viewportX + window[0];
	f->windowY = backEnd.viewParms.viewportY + window[1];

	f->eyeZ = eye[2];

#ifdef USE_REVERSED_DEPTH
	f->drawZ = (clip[2]+0.20) / clip[3];
#else
	f->drawZ = (clip[2]-0.20) / clip[3];
#endif

}

void RB_AddDlightFlares( void ) {
	dlight_t		*l;
	int				i, j, k;
	fog_t			*fog = NULL;

	if ( !r_flares->integer ) {
		return;
	}

	l = backEnd.refdef.dlights;

	if ( tr.world )
		fog = tr.world->fogs;

	for ( i = 0 ; i < backEnd.refdef.num_dlights; i++, l++ ) {

		if ( fog )
		{
			for ( j = 1 ; j < tr.world->numfogs ; j++ ) {
				fog = &tr.world->fogs[j];
				for ( k = 0 ; k < 3 ; k++ ) {
					if ( l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k] ) {
						break;
					}
				}
				if ( k == 3 ) {
					break;
				}
			}
			if ( j == tr.world->numfogs ) {
				j = 0;
			}
		}
		else
			j = 0;

		RB_AddFlare( (void *)l, j, l->origin, l->color, NULL );
	}
}

static float *vk_ortho( float x1, float x2,
						float y2, float y1,
						float z1, float z2 ) {

	static float m[16] = { 0 };

	m[0] = 2.0f / (x2 - x1);
	m[5] = 2.0f / (y2 - y1);
	m[10] = 1.0f / (z1 - z2);
	m[12] = -(x2 + x1) / (x2 - x1);
	m[13] = -(y2 + y1) / (y2 - y1);
	m[14] = z1 / (z1 - z2);
	m[15] = 1.0f;

	return m;
}

static void RB_TestFlare( flare_t *f ) {
	qboolean		visible;
	float			fade;
	float			*m;
	uint32_t		offset;
	int				i;

	backEnd.pc.c_flareTests++;

	offset = (f - r_flareStructs) * vk.storage_alignment;

	if ( f->testCount ) {
		uint32_t *cnt = (uint32_t*)(vk.storage.buffer_ptr + offset);
		if ( *cnt )
			visible = qtrue;
		else
			visible = qfalse;

		f->testCount = 1;
	} else {
		visible = qfalse;
	}

	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 0, 1 );
	vk_update_mvp( m );

	tess.xyz[0][0] = f->windowX;
	tess.xyz[0][1] = f->windowY;
	tess.xyz[0][2] = -f->drawZ;
	tess.numVertexes = 1;

#ifdef USE_VBO
	tess.vboIndex = 0;
#endif
	for ( i = 0; i < VK_DESC_COUNT; i++ ) {
		vk_reset_descriptor( i );
	}
	vk_bind_pipeline( vk.dot_pipeline );
	vk_bind_geometry( TESS_XYZ );
	vk_draw_dot( offset );

	if ( visible ) {
		if ( !f->visible ) {
			f->visible = qtrue;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = ( ( backEnd.refdef.time - f->fadeTime ) /1000.0f ) * r_flareFade->value;
	} else {
		if ( f->visible ) {
			f->visible = qfalse;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = 1.0f - ( ( backEnd.refdef.time - f->fadeTime ) / 1000.0f ) * r_flareFade->value;
	}

	if ( fade < 0 ) {
		fade = 0;
	} else if ( fade > 1 ) {
		fade = 1;
	}

	f->drawIntensity = fade;
}

static void RB_RenderFlare( flare_t *f ) {
	float			size;
	vec3_t			color;
	float distance, intensity, factor;
	byte fogFactors[3] = {255, 255, 255};
	color4ub_t		c;

	backEnd.pc.c_flareRenders++;

	if ( f->eyeZ > -1.0f )
		distance = 1.0f;
	else
		distance = -f->eyeZ;

	size = backEnd.viewParms.viewportWidth * ( r_flareSize->value/640.0f + 8 / distance );

	factor = distance + size * sqrt( r_flareCoeff->value );

	intensity = r_flareCoeff->value * size * size / ( factor * factor );

	VectorScale( f->color, f->drawIntensity * intensity, color );

	if ( tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs )
	{
		tess.numVertexes = 1;
		VectorCopy( f->origin, tess.xyz[0] );
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog( fogFactors );

		if ( !(fogFactors[0] || fogFactors[1] || fogFactors[2]) )
			return;
	}

	RB_BeginSurface( tr.flareShader, f->fogNum );

	c.rgba[0] = color[0] * fogFactors[0];
	c.rgba[1] = color[1] * fogFactors[1];
	c.rgba[2] = color[2] * fogFactors[2];
	c.rgba[3] = 255;

	RB_AddQuadStamp2( f->windowX - size, f->windowY - size, size * 2, size * 2, 0, 0, 1, 1, c );

	RB_EndSurface();
}

void RB_RenderFlares( void ) {
	flare_t		*f;
	flare_t		**prev;
	qboolean	draw;
	float		*m;

	if ( !r_flares->integer ) {
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
		return;
	}

	if ( backEnd.isHyperspace ) {
		return;
	}

	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	draw = qfalse;
	prev = &r_activeFlares;
	while ( ( f = *prev ) != NULL ) {
		if ( backEnd.viewParms.frameCount - f->addedFrame > 0 && f->portalView == backEnd.viewParms.portalView ) {
			*prev = f->next;
			f->next = r_inactiveFlares;
			r_inactiveFlares = f;
			continue;
		}

		f->drawIntensity = 0;
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			RB_TestFlare( f );
			if ( f->testCount == 0 ) {
			} else if ( f->drawIntensity ) {
				draw = qtrue;
			} else {
				*prev = f->next;
				f->next = r_inactiveFlares;
				r_inactiveFlares = f;
				continue;
			}
		}

		prev = &f->next;
	}

	if ( !draw ) {
		return;
	}

#ifdef USE_REVERSED_DEPTH
	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 1.0, 0.0 );
#else
	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 0.0, 1.0 );
#endif

	vk_update_mvp( m );

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView && f->drawIntensity ) {
			RB_RenderFlare( f );
		}
	}

}
