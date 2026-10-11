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

#define		MAX_FLARES		256

flare_t		r_flareStructs[MAX_FLARES];
flare_t		*r_activeFlares, *r_inactiveFlares;

void R_ClearFlares( void ) {
	int		i;

	Com_Memset( r_flareStructs, 0, sizeof( r_flareStructs ) );
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for ( i = 0 ; i < MAX_FLARES ; i++ ) {
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}

void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {
	int				i;
	flare_t			*f;
	vec3_t			local;
	float			d = 1;
	vec4_t			eye, clip, normalized, window;

	backEnd.pc.c_flareAdds++;

	if ( normal && (normal[0] || normal[1] || normal[2]) )
	{
		VectorSubtract( backEnd.viewParms.or.origin, point, local );
		VectorNormalizeFast( local );
		d = DotProduct( local, normal );

		if ( d < 0 )
			return;
	}

	R_TransformModelToClip( point, backEnd.or.modelMatrix, 
		backEnd.viewParms.projectionMatrix, eye, clip );

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return;
		}
	}

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth
		|| window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return;
	}

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum
			&& f->portalView == backEnd.viewParms.portalView ) {
			break;
		}
	}

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
		f->addedFrame = -1;
	}

	if ( f->addedFrame != backEnd.viewParms.frameCount - 1 ) {
		f->visible = qfalse;
		f->fadeTime = backEnd.refdef.time - 2000;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy( point, f->origin );
	VectorCopy( color, f->color );

	VectorScale( f->color, d, f->color ); 

	f->windowX = backEnd.viewParms.viewportX + window[0];
	f->windowY = backEnd.viewParms.viewportY + window[1];

	f->eyeZ = eye[2];

	if ( backEnd.viewParms.portalView )
		f->drawZ = (clip[2] + clip[3] - 1.5 ) / ( 2 * clip[3] );
	else
		f->drawZ = (clip[2] + clip[3] - 0.5 ) / ( 2 * clip[3] );
}

void RB_AddDlightFlares( void ) {
	dlight_t		*l;
	int				i, j, k;
	fog_t			*fog = NULL;

	if ( !r_flares->integer ) {
		return;
	}

	l = backEnd.refdef.dlights;

	if(tr.world)
		fog = tr.world->fogs;

	for (i=0 ; i<backEnd.refdef.num_dlights ; i++, l++) {

		if(fog)
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

static void RB_TestFlare( flare_t *f ) {
	float			depth;
	qboolean		visible;
	float			fade;

	backEnd.pc.c_flareTests++;

	glState.finishCalled = qfalse;

	qglReadPixels( f->windowX, f->windowY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	visible = (depth > f->drawZ);
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
	}
	if ( fade > 1 ) {
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

	intensity = r_flareCoeff->value * size * size / (factor * factor);

	VectorScale(f->color, f->drawIntensity * intensity, color);

	if ( tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs )
	{
		tess.numVertexes = 1;
		VectorCopy(f->origin, tess.xyz[0]);
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog(fogFactors);

		if (!(fogFactors[0] || fogFactors[1] || fogFactors[2]))
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

void RB_RenderFlares (void) {
	flare_t		*f;
	flare_t		**prev;
	qboolean	draw;

	if ( !r_flares->integer ) {
		return;
	}

	if ( backEnd.isHyperspace ) {
		return;
	}

	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

#ifdef USE_FBO
	if ( blitMSfbo ) {
		FBO_BlitMS( qtrue );
	}
#endif

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
			if ( f->drawIntensity ) {
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

#ifdef USE_FBO
	if ( blitMSfbo ) {
		FBO_BindMain();
	}
#endif

	if ( !draw ) {
		return;
	}

	if ( backEnd.viewParms.portalView != PV_NONE ) {
		qglDisable( GL_CLIP_PLANE0 );
	}

	qglPushMatrix();
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadMatrixf( GL_Ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, -99999, 99999 ) );

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView && f->drawIntensity ) {
			RB_RenderFlare( f );
		}
	}

	qglPopMatrix();
	qglMatrixMode( GL_MODELVIEW );
	qglPopMatrix();
}
