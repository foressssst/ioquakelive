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

#include <string.h>

trGlobals_t		tr;

static const float s_flipMatrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

refimport_t	ri;

static surfaceType_t entitySurface = SF_ENTITY;

int R_CullLocalBox( const vec3_t bounds[2] ) {
	int		i, j;
	vec3_t	transformed[8];
	float	dists[8];
	vec3_t	v;
	cplane_t	*frust;
	int			anyBack;
	int			front, back;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	for (i = 0 ; i < 8 ; i++) {
		v[0] = bounds[i&1][0];
		v[1] = bounds[(i>>1)&1][1];
		v[2] = bounds[(i>>2)&1][2];

		VectorCopy( tr.or.origin, transformed[i] );
		VectorMA( transformed[i], v[0], tr.or.axis[0], transformed[i] );
		VectorMA( transformed[i], v[1], tr.or.axis[1], transformed[i] );
		VectorMA( transformed[i], v[2], tr.or.axis[2], transformed[i] );
	}

	anyBack = 0;
	for (i = 0 ; i < 4 ; i++) {
		frust = &tr.viewParms.frustum[i];

		front = back = 0;
		for (j = 0 ; j < 8 ; j++) {
			dists[j] = DotProduct(transformed[j], frust->normal);
			if ( dists[j] > frust->dist ) {
				front = 1;
				if ( back ) {
					break;
				}
			} else {
				back = 1;
			}
		}
		if ( !front ) {
			return CULL_OUT;
		}
		anyBack |= back;
	}

	if ( !anyBack ) {
		return CULL_IN;
	}

	return CULL_CLIP;
}

int R_CullLocalPointAndRadius( const vec3_t pt, float radius )
{
	vec3_t transformed;

	R_LocalPointToWorld( pt, transformed );

	return R_CullPointAndRadius( transformed, radius );
}

int R_CullPointAndRadius( const vec3_t pt, float radius )
{
	int		i;
	float	dist;
	const cplane_t	*frust;
	qboolean mightBeClipped = qfalse;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	for (i = 0 ; i < 4 ; i++) 
	{
		frust = &tr.viewParms.frustum[i];

		dist = DotProduct( pt, frust->normal) - frust->dist;
		if ( dist < -radius )
		{
			return CULL_OUT;
		}
		else if ( dist <= radius ) 
		{
			mightBeClipped = qtrue;
		}
	}

	if ( mightBeClipped )
	{
		return CULL_CLIP;
	}

	return CULL_IN;
}

int R_CullDlight( const dlight_t* dl )
{
	int		i;
	float	dist, dist2;
	cplane_t	*frust;
	qboolean mightBeClipped = qfalse;

	if ( r_nocull->integer )
		return CULL_CLIP;

	if ( dl->linear ) {
		for ( i = 0 ; i < 4 ; i++ ) {
			frust = &tr.viewParms.frustum[i];
			dist = DotProduct( dl->transformed, frust->normal) - frust->dist;
			dist2 = DotProduct( dl->transformed2, frust->normal) - frust->dist;
			if ( dist < -dl->radius && dist2 < -dl->radius )
				return CULL_OUT;
			else if ( dist <= dl->radius || dist2 <= dl->radius ) 
				mightBeClipped = qtrue;
		}
	} 
	else
	for ( i = 0 ; i < 4 ; i++ ) {
		frust = &tr.viewParms.frustum[i];
		dist = DotProduct( dl->transformed, frust->normal) - frust->dist;
		if ( dist < -dl->radius )
			return CULL_OUT;
		else if ( dist <= dl->radius ) 
			mightBeClipped = qtrue;
	}

	if ( mightBeClipped )
		return CULL_CLIP;

	return CULL_IN;
}

static void R_LocalNormalToWorld( const vec3_t local, vec3_t world ) {
	world[0] = local[0] * tr.or.axis[0][0] + local[1] * tr.or.axis[1][0] + local[2] * tr.or.axis[2][0];
	world[1] = local[0] * tr.or.axis[0][1] + local[1] * tr.or.axis[1][1] + local[2] * tr.or.axis[2][1];
	world[2] = local[0] * tr.or.axis[0][2] + local[1] * tr.or.axis[1][2] + local[2] * tr.or.axis[2][2];
}

void R_LocalPointToWorld( const vec3_t local, vec3_t world ) {
	world[0] = local[0] * tr.or.axis[0][0] + local[1] * tr.or.axis[1][0] + local[2] * tr.or.axis[2][0] + tr.or.origin[0];
	world[1] = local[0] * tr.or.axis[0][1] + local[1] * tr.or.axis[1][1] + local[2] * tr.or.axis[2][1] + tr.or.origin[1];
	world[2] = local[0] * tr.or.axis[0][2] + local[1] * tr.or.axis[1][2] + local[2] * tr.or.axis[2][2] + tr.or.origin[2];
}

void R_WorldToLocal( const vec3_t world, vec3_t local ) {
	local[0] = DotProduct( world, tr.or.axis[0] );
	local[1] = DotProduct( world, tr.or.axis[1] );
	local[2] = DotProduct( world, tr.or.axis[2] );
}

void R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		eye[i] = 
			src[0] * modelMatrix[ i + 0 * 4 ] +
			src[1] * modelMatrix[ i + 1 * 4 ] +
			src[2] * modelMatrix[ i + 2 * 4 ] +
			1 * modelMatrix[ i + 3 * 4 ];
	}

	for ( i = 0 ; i < 4 ; i++ ) {
		dst[i] = 
			eye[0] * projectionMatrix[ i + 0 * 4 ] +
			eye[1] * projectionMatrix[ i + 1 * 4 ] +
			eye[2] * projectionMatrix[ i + 2 * 4 ] +
			eye[3] * projectionMatrix[ i + 3 * 4 ];
	}
}

static void R_TransformModelToClipMVP( const vec3_t src, const float *mvp, vec4_t clip ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		clip[i] = 
			src[0] * mvp[ i + 0 * 4 ] +
			src[1] * mvp[ i + 1 * 4 ] +
			src[2] * mvp[ i + 2 * 4 ] +
			1 * mvp[ i + 3 * 4 ];
	}
}

void R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window ) {
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = ( clip[2] + clip[3] ) / ( 2 * clip[3] );

	window[0] = 0.5f * ( 1.0f + normalized[0] ) * view->viewportWidth;
	window[1] = 0.5f * ( 1.0f + normalized[1] ) * view->viewportHeight;
	window[2] = normalized[2];

	window[0] = (int) ( window[0] + 0.5 );
	window[1] = (int) ( window[1] + 0.5 );
}

void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}

void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *or ) {
	float	glMatrix[16];
	vec3_t	delta;
	float	axisLength;

	if ( ent->e.reType != RT_MODEL ) {
		*or = viewParms->world;
		return;
	}

	VectorCopy( ent->e.origin, or->origin );

	VectorCopy( ent->e.axis[0], or->axis[0] );
	VectorCopy( ent->e.axis[1], or->axis[1] );
	VectorCopy( ent->e.axis[2], or->axis[2] );

	glMatrix[0] = or->axis[0][0];
	glMatrix[4] = or->axis[1][0];
	glMatrix[8] = or->axis[2][0];
	glMatrix[12] = or->origin[0];

	glMatrix[1] = or->axis[0][1];
	glMatrix[5] = or->axis[1][1];
	glMatrix[9] = or->axis[2][1];
	glMatrix[13] = or->origin[1];

	glMatrix[2] = or->axis[0][2];
	glMatrix[6] = or->axis[1][2];
	glMatrix[10] = or->axis[2][2];
	glMatrix[14] = or->origin[2];

	glMatrix[3] = 0;
	glMatrix[7] = 0;
	glMatrix[11] = 0;
	glMatrix[15] = 1;

	myGlMultMatrix( glMatrix, viewParms->world.modelMatrix, or->modelMatrix );

	VectorSubtract( viewParms->or.origin, or->origin, delta );

	if ( ent->e.nonNormalizedAxes ) {
		axisLength = VectorLength( ent->e.axis[0] );
		if ( !axisLength ) {
			axisLength = 0;
		} else {
			axisLength = 1.0f / axisLength;
		}
	} else {
		axisLength = 1.0f;
	}

	or->viewOrigin[0] = DotProduct( delta, or->axis[0] ) * axisLength;
	or->viewOrigin[1] = DotProduct( delta, or->axis[1] ) * axisLength;
	or->viewOrigin[2] = DotProduct( delta, or->axis[2] ) * axisLength;
}

static void R_RotateForViewer( void )
{
	float	viewerMatrix[16];
	vec3_t	origin;

	Com_Memset (&tr.or, 0, sizeof(tr.or));
	tr.or.axis[0][0] = 1;
	tr.or.axis[1][1] = 1;
	tr.or.axis[2][2] = 1;
	VectorCopy (tr.viewParms.or.origin, tr.or.viewOrigin);

	VectorCopy( tr.viewParms.or.origin, origin );

	viewerMatrix[0] = tr.viewParms.or.axis[0][0];
	viewerMatrix[4] = tr.viewParms.or.axis[0][1];
	viewerMatrix[8] = tr.viewParms.or.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.or.axis[1][0];
	viewerMatrix[5] = tr.viewParms.or.axis[1][1];
	viewerMatrix[9] = tr.viewParms.or.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.or.axis[2][0];
	viewerMatrix[6] = tr.viewParms.or.axis[2][1];
	viewerMatrix[10] = tr.viewParms.or.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	myGlMultMatrix( viewerMatrix, s_flipMatrix, tr.or.modelMatrix );

	tr.viewParms.world = tr.or;
}

static void R_SetFarClip( void )
{
	float	farthestCornerDistance;
	int		i;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048;
		return;
	}

	farthestCornerDistance = 0;
	for ( i = 0; i < 8; i++ )
	{
		vec3_t v;
		vec3_t vecTo;
		float distance;

		v[0] = tr.viewParms.visBounds[(i>>0)&1][0];
		v[1] = tr.viewParms.visBounds[(i>>1)&1][1];
		v[2] = tr.viewParms.visBounds[(i>>2)&1][2];

		VectorSubtract( v, tr.viewParms.or.origin, vecTo );

		distance = DotProduct( vecTo, vecTo );

		if ( distance > farthestCornerDistance )
		{
			farthestCornerDistance = distance;
		}
	}

	tr.viewParms.zFar = sqrt( farthestCornerDistance );
}

static void R_SetupFrustum( viewParms_t *dest, float xmin, float xmax, float ymax, float zProj, float stereoSep )
{
	vec3_t ofsorigin;
	float oppleg, adjleg, length;
	int i;

	if(stereoSep == 0 && xmin == -xmax)
	{
		VectorCopy(dest->or.origin, ofsorigin);

		length = sqrt(xmax * xmax + zProj * zProj);
		oppleg = xmax / length;
		adjleg = zProj / length;

		VectorScale(dest->or.axis[0], oppleg, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, adjleg, dest->or.axis[1], dest->frustum[0].normal);

		VectorScale(dest->or.axis[0], oppleg, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -adjleg, dest->or.axis[1], dest->frustum[1].normal);
	}
	else
	{
		VectorMA(dest->or.origin, stereoSep, dest->or.axis[1], ofsorigin);

		oppleg = xmax + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->or.axis[0], oppleg / length, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, zProj / length, dest->or.axis[1], dest->frustum[0].normal);

		oppleg = xmin + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->or.axis[0], -oppleg / length, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -zProj / length, dest->or.axis[1], dest->frustum[1].normal);
	}

	length = sqrt(ymax * ymax + zProj * zProj);
	oppleg = ymax / length;
	adjleg = zProj / length;

	VectorScale(dest->or.axis[0], oppleg, dest->frustum[2].normal);
	VectorMA(dest->frustum[2].normal, adjleg, dest->or.axis[2], dest->frustum[2].normal);

	VectorScale(dest->or.axis[0], oppleg, dest->frustum[3].normal);
	VectorMA(dest->frustum[3].normal, -adjleg, dest->or.axis[2], dest->frustum[3].normal);

	for (i=0 ; i<4 ; i++) {
		dest->frustum[i].type = PLANE_NON_AXIAL;
		dest->frustum[i].dist = DotProduct (ofsorigin, dest->frustum[i].normal);
		SetPlaneSignbits( &dest->frustum[i] );
	}

	VectorCopy( dest->or.axis[0], dest->frustum[4].normal );
	dest->frustum[4].type = PLANE_NON_AXIAL;
	dest->frustum[4].dist = DotProduct( ofsorigin, dest->frustum[4].normal ) + r_znear->value;
	SetPlaneSignbits( &dest->frustum[4] );
}

void R_SetupProjection( viewParms_t *dest, float zProj, qboolean computeFrustum )
{
	float	xmin, xmax, ymin, ymax;
	float	width, height, stereoSep = r_stereoSeparation->value;

	if ( stereoSep != 0 )
	{
		if ( dest->stereoFrame == STEREO_LEFT )
			stereoSep = zProj / stereoSep;
		else if ( dest->stereoFrame == STEREO_RIGHT )
			stereoSep = zProj / -stereoSep;
		else
			stereoSep = 0;
	}

	ymax = zProj * tan(dest->fovY * M_PI / 360.0f);
	ymin = -ymax;

	xmax = zProj * tan(dest->fovX * M_PI / 360.0f);
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;

	dest->projectionMatrix[0] = 2 * zProj / width;
	dest->projectionMatrix[4] = 0;
	dest->projectionMatrix[8] = (xmax + xmin + 2 * stereoSep) / width;
	dest->projectionMatrix[12] = 2 * zProj * stereoSep / width;

	dest->projectionMatrix[1] = 0;
	dest->projectionMatrix[5] = 2 * zProj / height;
	dest->projectionMatrix[9] = ( ymax + ymin ) / height;
	dest->projectionMatrix[13] = 0;

	dest->projectionMatrix[3] = 0;
	dest->projectionMatrix[7] = 0;
	dest->projectionMatrix[11] = -1;
	dest->projectionMatrix[15] = 0;

	if ( computeFrustum )
		R_SetupFrustum( dest, xmin, xmax, ymax, zProj, stereoSep );
}

static void R_SetupProjectionZ( viewParms_t *dest )
{
	const float zNear = r_znear->value;
	const float zFar = dest->zFar;
	const float depth = zFar - zNear;

	dest->projectionMatrix[2] = 0;
	dest->projectionMatrix[6] = 0;
#ifdef USE_VULKAN
#ifdef USE_REVERSED_DEPTH
	dest->projectionMatrix[10] = zNear / depth;
	dest->projectionMatrix[14] = zFar * zNear / depth;
#else
	dest->projectionMatrix[10] = - zFar / depth;
	dest->projectionMatrix[14] = - zFar * zNear / depth;
#endif
#else
	dest->projectionMatrix[10] = -( zFar + zNear ) / depth;
	dest->projectionMatrix[14] = -2 * zFar * zNear / depth;
#endif

	if ( dest->portalView != PV_NONE )
	{
		float	plane[4];
		float	plane2[4];
		vec4_t q, c;

#ifdef USE_VULKAN
#ifdef USE_REVERSED_DEPTH
		dest->projectionMatrix[10] = - zFar / depth;
		dest->projectionMatrix[14] = - zFar * zNear / depth;
#endif
#endif
		plane[0] = dest->portalPlane.normal[0];
		plane[1] = dest->portalPlane.normal[1];
		plane[2] = dest->portalPlane.normal[2];
		plane[3] = dest->portalPlane.dist;

		plane2[0] = -DotProduct( dest->or.axis[1], plane );
		plane2[1] =  DotProduct( dest->or.axis[2], plane );
		plane2[2] = -DotProduct( dest->or.axis[0], plane );
		plane2[3] =  DotProduct( plane, dest->or.origin) - plane[3];

		q[0] = (SGN(plane2[0]) + dest->projectionMatrix[8]) / dest->projectionMatrix[0];
		q[1] = (SGN(plane2[1]) + dest->projectionMatrix[9]) / dest->projectionMatrix[5];
		q[2] = -1.0f;
#ifdef USE_VULKAN
		q[3] = - dest->projectionMatrix[10] / dest->projectionMatrix[14];
#else
		q[3] = (1.0f + dest->projectionMatrix[10]) / dest->projectionMatrix[14];
#endif
		VectorScale4( plane2, 2.0f / DotProduct4(plane2, q), c );

		dest->projectionMatrix[2]  = c[0];
		dest->projectionMatrix[6]  = c[1];
#ifdef USE_VULKAN
		dest->projectionMatrix[10] = c[2];
#else
		dest->projectionMatrix[10] = c[2] + 1.0f;
#endif
		dest->projectionMatrix[14] = c[3];

#ifdef USE_REVERSED_DEPTH
		dest->projectionMatrix[2] = -dest->projectionMatrix[2];
		dest->projectionMatrix[6] = -dest->projectionMatrix[6];
		dest->projectionMatrix[10] = -(dest->projectionMatrix[10] + 1.0);
		dest->projectionMatrix[14] = -dest->projectionMatrix[14];
#endif
	}
}

static void R_MirrorPoint( const vec3_t in, const orientation_t *surface, const orientation_t *camera, vec3_t out ) {
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	VectorSubtract( in, surface->origin, local );

	VectorClear( transformed );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct( local, surface->axis[i] );
		VectorMA( transformed, d, camera->axis[i], transformed );
	}

	VectorAdd( transformed, camera->origin, out );
}

static void R_MirrorVector( const vec3_t in, const orientation_t *surface, const orientation_t *camera, vec3_t out ) {
	int		i;
	float	d;

	VectorClear( out );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(in, surface->axis[i]);
		VectorMA( out, d, camera->axis[i], out );
	}
}

static void R_PlaneForSurface( const surfaceType_t *surfType, cplane_t *plane ) {
	srfTriangles_t	*tri;
	srfPoly_t		*poly;
	drawVert_t		*v1, *v2, *v3;
	vec4_t			plane4;

	if (!surfType) {
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
	switch (*surfType) {
	case SF_FACE:
		*plane = ((srfSurfaceFace_t *)surfType)->plane;
		return;
	case SF_TRIANGLES:
		tri = (srfTriangles_t *)surfType;
		v1 = tri->verts + tri->indexes[0];
		v2 = tri->verts + tri->indexes[1];
		v3 = tri->verts + tri->indexes[2];
		PlaneFromPoints( plane4, v1->xyz, v2->xyz, v3->xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	case SF_POLY:
		poly = (srfPoly_t *)surfType;
		PlaneFromPoints( plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	default:
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
}

static qboolean R_GetPortalOrientations( const drawSurf_t *drawSurf, int entityNum,
							 orientation_t *surface, orientation_t *camera,
							 vec3_t pvsOrigin, portalView_t *portalView ) {
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;
	vec3_t		transformed;

	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	if ( entityNum != REFENTITYNUM_WORLD ) {
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.or );

		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.or.origin );

		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.or.origin );
	} else {
		plane = originalPlane;
	}

	VectorCopy( plane.normal, surface->axis[0] );
	PerpendicularVector( surface->axis[1], surface->axis[0] );
	CrossProduct( surface->axis[0], surface->axis[1], surface->axis[2] );

	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) {
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		VectorCopy( e->e.oldorigin, pvsOrigin );

		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) {
			VectorScale( plane.normal, plane.dist, surface->origin );
			VectorCopy( surface->origin, camera->origin );
			VectorSubtract( vec3_origin, surface->axis[0], camera->axis[0] );
			VectorCopy( surface->axis[1], camera->axis[1] );
			VectorCopy( surface->axis[2], camera->axis[2] );

			*portalView = PV_MIRROR;
			return qtrue;
		}

		d = DotProduct( e->e.origin, plane.normal ) - plane.dist;
		VectorMA( e->e.origin, -d, surface->axis[0], surface->origin );

		VectorCopy( e->e.oldorigin, camera->origin );
		AxisCopy( e->e.axis, camera->axis );
		VectorSubtract( vec3_origin, camera->axis[0], camera->axis[0] );
		VectorSubtract( vec3_origin, camera->axis[1], camera->axis[1] );

		if ( e->e.oldframe ) {
			if ( e->e.frame ) {
				d = (tr.refdef.time/1000.0f) * e->e.frame;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			} else {
				d = sin( tr.refdef.time * 0.003f );
				d = e->e.skinNum + d * 4;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			}
		}
		else if ( e->e.skinNum ) {
			d = e->e.skinNum;
			VectorCopy( camera->axis[1], transformed );
			RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
			CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
		}

		*portalView = PV_PORTAL;
		return qtrue;
	}

	return qfalse;
}

static qboolean IsMirror( const drawSurf_t *drawSurf, int entityNum )
{
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;

	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	if ( entityNum != REFENTITYNUM_WORLD )
	{
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.or );

		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.or.origin );

		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.or.origin );
	}
	else
	{
		plane = originalPlane;
	}

	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) 
	{
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) 
		{
			return qtrue;
		}

		return qfalse;
	}
	return qfalse;
}

static qboolean SurfIsOffscreen( const drawSurf_t *drawSurf, qboolean *isMirror ) {
	float shortest = 100000000;
	int entityNum;
	int numTriangles;
	shader_t *shader;
	int		fogNum;
	int dlighted;
	vec4_t clip, eye;
	int i;
	unsigned int pointAnd = (unsigned int)~0;

	*isMirror = qfalse;

	R_RotateForViewer();

	R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
	RB_BeginSurface( shader, fogNum );
#ifdef USE_VBO
	tess.allowVBO = qfalse;
#endif
#ifdef USE_TESS_NEEDS_NORMAL
	tess.needsNormal = qtrue;
#endif
	rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );

	for ( i = 0; i < tess.numVertexes; i++ )
	{
		int j;
		unsigned int pointFlags = 0;

		R_TransformModelToClip( tess.xyz[i], tr.or.modelMatrix, tr.viewParms.projectionMatrix, eye, clip );

		for ( j = 0; j < 3; j++ )
		{
			if ( clip[j] >= clip[3] )
			{
				pointFlags |= (1 << (j*2));
			}
			else if ( clip[j] <= -clip[3] )
			{
				pointFlags |= ( 1 << (j*2+1));
			}
		}
		pointAnd &= pointFlags;
	}

	if ( pointAnd )
	{
		tess.numIndexes = 0;
		return qtrue;
	}

	numTriangles = tess.numIndexes / 3;

	for ( i = 0; i < tess.numIndexes; i += 3 )
	{
		vec3_t normal;
		float len;

		VectorSubtract( tess.xyz[tess.indexes[i]], tr.viewParms.or.origin, normal );

		len = VectorLengthSquared( normal );
		if ( len < shortest )
		{
			shortest = len;
		}

		if ( DotProduct( normal, tess.normal[tess.indexes[i]] ) >= 0 )
		{
			numTriangles--;
		}
	}
	tess.numIndexes = 0;
	if ( !numTriangles )
	{
		return qtrue;
	}

	if ( IsMirror( drawSurf, entityNum ) )
	{
		*isMirror = qtrue;
		return qfalse;
	}

	if ( shortest > (tess.shader->portalRange*tess.shader->portalRange) )
	{
		return qtrue;
	}

	return qfalse;
}

static void R_GetModelViewBounds( int *mins, int *maxs )
{
	float			minn[2];
	float			maxn[2];
	float			norm[2];
	float			mvp[16];
	float			dist[4];
	vec4_t			clip;
	int				i,j;

	minn[0] = minn[1] =  1.0;
	maxn[0] = maxn[1] = -1.0;

	myGlMultMatrix( tr.or.modelMatrix, tr.viewParms.projectionMatrix, mvp );

	for ( i = 0; i < tess.numVertexes; i++ ) {
		R_TransformModelToClipMVP( tess.xyz[i], mvp, clip );
		if ( clip[3] <= 0.0 ) {
			dist[0] = DotProduct( tess.xyz[i], tr.viewParms.frustum[0].normal ) - tr.viewParms.frustum[0].dist;
			dist[1] = DotProduct( tess.xyz[i], tr.viewParms.frustum[1].normal ) - tr.viewParms.frustum[1].dist;
			dist[2] = DotProduct( tess.xyz[i], tr.viewParms.frustum[2].normal ) - tr.viewParms.frustum[2].dist;
			dist[3] = DotProduct( tess.xyz[i], tr.viewParms.frustum[3].normal ) - tr.viewParms.frustum[3].dist;
			if ( dist[0] <= 0 && dist[1] <= 0 ) {
				if ( dist[0] < dist[1] ) {
					maxn[0] =  1.0f;
				} else {
					minn[0] = -1.0f;
				}
			} else {
				if ( dist[0] <= 0 ) maxn[0] =  1.0f;
				if ( dist[1] <= 0 ) minn[0] = -1.0f;
			}
			if ( dist[2] <= 0 && dist[3] <= 0 ) {
				if ( dist[2] < dist[3] )
					minn[1] = -1.0f;
				else
					maxn[1] =  1.0f;
			} else {
				if ( dist[2] <= 0 ) minn[1] = -1.0f;
				if ( dist[3] <= 0 ) maxn[1] =  1.0f;
			}
		} else {
			for ( j = 0; j < 2; j++ ) {
				if ( clip[j] >  clip[3] ) clip[j] =  clip[3]; else
				if ( clip[j] < -clip[3] ) clip[j] = -clip[3];
			}
			norm[0] = clip[0] / clip[3];
			norm[1] = clip[1] / clip[3];
			for ( j = 0; j < 2; j++ ) {
				if ( norm[j] < minn[j] ) minn[j] = norm[j];
				if ( norm[j] > maxn[j] ) maxn[j] = norm[j];
			}
		}
	}

	mins[0] = (int)(-0.5 + 0.5 * ( 1.0 + minn[0] ) * tr.viewParms.viewportWidth);
	mins[1] = (int)(-0.5 + 0.5 * ( 1.0 + minn[1] ) * tr.viewParms.viewportHeight);
	maxs[0] = (int)(0.5 + 0.5 * ( 1.0 + maxn[0] ) * tr.viewParms.viewportWidth);
	maxs[1] = (int)(0.5 + 0.5 * ( 1.0 + maxn[1] ) * tr.viewParms.viewportHeight);
}

extern int r_numdlights;
static qboolean R_MirrorViewBySurface( const drawSurf_t *drawSurf, int entityNum ) {
	viewParms_t		newParms;
	viewParms_t		oldParms;
	orientation_t	surface, camera;
	qboolean		isMirror;

	if ( tr.viewParms.portalView != PV_NONE ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n" );
		return qfalse;
	}

	if ( r_noportals->integer > 1  ) {
		return qfalse;
	}

	if ( SurfIsOffscreen( drawSurf, &isMirror ) ) {
		return qfalse;
	}

	if ( !isMirror && r_noportals->integer ) {
		return qfalse;
	}

	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.portalView = PV_NONE;

	if ( !R_GetPortalOrientations( drawSurf, entityNum, &surface, &camera, 
		newParms.pvsOrigin, &newParms.portalView ) ) {
		return qfalse;
	}

#ifdef USE_PMLIGHT
	if ( r_numdlights + oldParms.num_dlights <= ARRAY_LEN( backEndData->dlights ) ) {
		int i;
		newParms.dlights = oldParms.dlights + oldParms.num_dlights;
		newParms.num_dlights = oldParms.num_dlights;
		r_numdlights += oldParms.num_dlights;
		for ( i = 0; i < oldParms.num_dlights; i++ )
			newParms.dlights[i] = oldParms.dlights[i];
	}
#endif

#if defined (USE_VULKAN) && !defined (USE_BUFFER_CLEAR)
	if ( tess.numVertexes > 2 && r_fastsky->integer && vk.clearAttachment ) {
#else
	if ( tess.numVertexes > 2 && r_fastsky->integer ) {
#endif
		int mins[2], maxs[2];
		R_GetModelViewBounds( mins, maxs );
		newParms.scissorX = newParms.viewportX + mins[0];
		newParms.scissorY = newParms.viewportY + mins[1];
		newParms.scissorWidth = maxs[0] - mins[0];
		newParms.scissorHeight = maxs[1] - mins[1];
	}

	R_MirrorPoint( oldParms.or.origin, &surface, &camera, newParms.or.origin );

	VectorSubtract( vec3_origin, camera.axis[0], newParms.portalPlane.normal );
	newParms.portalPlane.dist = DotProduct( camera.origin, newParms.portalPlane.normal );

	R_MirrorVector (oldParms.or.axis[0], &surface, &camera, newParms.or.axis[0]);
	R_MirrorVector (oldParms.or.axis[1], &surface, &camera, newParms.or.axis[1]);
	R_MirrorVector (oldParms.or.axis[2], &surface, &camera, newParms.or.axis[2]);

	R_RenderView( &newParms );

	tr.viewParms = oldParms;

	return qtrue;
}

static int R_SpriteFogNum( const trRefEntity_t *ent ) {
	int				i, j;
	const fog_t		*fog;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	if ( ent->e.renderfx & RF_CROSSHAIR ) {
		return 0;
	}

	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( ent->e.origin[j] - ent->e.radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( ent->e.origin[j] + ent->e.radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}

static ID_INLINE void R_Radix( int byte, int size, const drawSurf_t *source, drawSurf_t *dest )
{
  int           count[ 256 ] = { 0 };
  int           index[ 256 ];
  int           i;
  unsigned char *sortKey;
  unsigned char *end;

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + byte;
  end = sortKey + ( size * sizeof( drawSurf_t ) );
  for( ; sortKey < end; sortKey += sizeof( drawSurf_t ) )
    ++count[ *sortKey ];

  index[ 0 ] = 0;

  for( i = 1; i < 256; ++i )
    index[ i ] = index[ i - 1 ] + count[ i - 1 ];

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + byte;
  for( i = 0; i < size; ++i, sortKey += sizeof( drawSurf_t ) )
    dest[ index[ *sortKey ]++ ] = source[ i ];
}

static void R_RadixSort( drawSurf_t *source, int size )
{
  static drawSurf_t scratch[ MAX_DRAWSURFS ];
#ifdef Q3_LITTLE_ENDIAN
  R_Radix( 0, size, source, scratch );
  R_Radix( 1, size, scratch, source );
  R_Radix( 2, size, source, scratch );
  R_Radix( 3, size, scratch, source );
#else
  R_Radix( 3, size, source, scratch );
  R_Radix( 2, size, scratch, source );
  R_Radix( 1, size, source, scratch );
  R_Radix( 0, size, scratch, source );
#endif
}

#ifdef USE_PMLIGHT

typedef struct litSurf_tape_s {
	struct litSurf_s *first;
	struct litSurf_s *last;
	unsigned count;
} litSurf_tape_t;

static void R_SortLitsurfs( dlight_t* dl )
{
	litSurf_tape_t tape[ 4 ];
	int				base;
	litSurf_t		*p;
	litSurf_t		*next;
	unsigned		block_size;
	litSurf_tape_t	*tape0;
	litSurf_tape_t	*tape1;
	int				dest;
	litSurf_tape_t	*output_tape;
	litSurf_tape_t	*chosen_tape;
	unsigned		n0, n1;

	tape[0].count = tape[1].count = 0;
	tape[0].first = tape[1].first = NULL;

	base = 0;
	p = dl->head;

	while ( p ) {
		next = p->next;
		p->next = tape[base].first;
		tape[base].first = p;
		tape[base].count++;
		p = next;
		base ^= 1;
	}

	block_size = 1;
	for ( base = 0; tape[base+1].count; base ^= 2, block_size <<= 1 )
	{
		tape0 = tape + base;
		tape1 = tape + base + 1;
		dest = base ^ 2;

		tape[dest].count = tape[dest+1].count = 0;
		for (; tape0->count; dest ^= 1)
		{
			output_tape = tape + dest;
			n0 = n1 = block_size;

			while (1)
			{
				if (n0 == 0 || tape0->count == 0)
				{
					if (n1 == 0 || tape1->count == 0)
						break;
					chosen_tape = tape1;
					n1--;
				}
				else if (n1 == 0 || tape1->count == 0)
				{
					chosen_tape = tape0;
					n0--;
				}
				else if (tape0->first->sort > tape1->first->sort)
				{
					chosen_tape = tape1;
					n1--;
				}
				else
				{
					chosen_tape = tape0;
					n0--;
				}
				chosen_tape->count--;
				p = chosen_tape->first;
				chosen_tape->first = p->next;
				if (output_tape->count == 0)
					output_tape->first = p;
				else
					output_tape->last->next = p;
				output_tape->last = p;
				output_tape->count++;
			}
		}
	}

	if (tape[base].count > 1)
		tape[base].last->next = NULL;

	dl->head = tape[base].first;
}

void R_AddLitSurf( surfaceType_t *surface, shader_t *shader, int fogIndex )
{
	struct litSurf_s *litsurf;

	if ( tr.refdef.numLitSurfs >= ARRAY_LEN( backEndData->litSurfs ) )
		return;

	tr.pc.c_lit_surfs++;

	litsurf = &tr.refdef.litSurfs[ tr.refdef.numLitSurfs++ ];

	litsurf->sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT) 
		| tr.shiftedEntityNum | ( fogIndex << QSORT_FOGNUM_SHIFT );
	litsurf->surface = surface;

	if ( !tr.light->head )
		tr.light->head = litsurf;
	if ( tr.light->tail )
		tr.light->tail->next = litsurf;

	tr.light->tail = litsurf;
	tr.light->tail->next = NULL;
}

void R_DecomposeLitSort( unsigned sort, int *entityNum, shader_t **shader, int *fogNum ) {
	*fogNum = ( sort >> QSORT_FOGNUM_SHIFT ) & FOGNUM_MASK;
	*shader = tr.sortedShaders[ ( sort >> QSORT_SHADERNUM_SHIFT ) & SHADERNUM_MASK ];
	*entityNum = ( sort >> QSORT_REFENTITYNUM_SHIFT ) & REFENTITYNUM_MASK;
}

#endif

void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, 
				   int fogIndex, int dlightMap ) {
	int			index;

	index = tr.refdef.numDrawSurfs & DRAWSURF_MASK;
	tr.refdef.drawSurfs[index].sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT) 
		| tr.shiftedEntityNum | ( fogIndex << QSORT_FOGNUM_SHIFT ) | (int)dlightMap;
	tr.refdef.drawSurfs[index].surface = surface;
	tr.refdef.numDrawSurfs++;
}

void R_DecomposeSort( unsigned sort, int *entityNum, shader_t **shader, 
					 int *fogNum, int *dlightMap ) {
	*fogNum = ( sort >> QSORT_FOGNUM_SHIFT ) & FOGNUM_MASK;
	*shader = tr.sortedShaders[ ( sort >> QSORT_SHADERNUM_SHIFT ) & SHADERNUM_MASK ];
	*entityNum = ( sort >> QSORT_REFENTITYNUM_SHIFT ) & REFENTITYNUM_MASK;
	*dlightMap = sort & DLIGHT_MASK;
}

static void R_SortDrawSurfs( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader;
	int				fogNum;
	int				entityNum;
	int				dlighted;
	int				i;

	if ( numDrawSurfs < 1 ) {
		R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
		return;
	}

	R_RadixSort( drawSurfs, numDrawSurfs );

	for ( i = 0 ; i < numDrawSurfs ; i++ ) {
		R_DecomposeSort( (drawSurfs+i)->sort, &entityNum, &shader, &fogNum, &dlighted );

		if ( shader->sort > SS_PORTAL ) {
			break;
		}

		if ( shader->sort == SS_BAD ) {
			ri.Error (ERR_DROP, "Shader '%s'with sort == SS_BAD", shader->name );
		}

		if ( R_MirrorViewBySurface( (drawSurfs+i), entityNum) ) {
			if ( r_portalOnly->integer ) {
				return;
			}
#if defined (USE_VULKAN) && !defined (USE_BUFFER_CLEAR)
			if ( r_fastsky->integer == 0 || !vk.clearAttachment ) {
#else
			if ( r_fastsky->integer == 0 ) {
#endif
				break;
			}
		}
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer ) 
#endif
	{
		dlight_t *dl;
		for ( i = 0; i < tr.refdef.num_dlights; ++i ) { 
			dl = &tr.refdef.dlights[ i ];
			if ( dl->head ) {
				R_SortLitsurfs( dl );
			}
		}
	}
#endif

	R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
}

static void R_AddEntitySurfaces( void ) {
	trRefEntity_t	*ent;
	shader_t		*shader;

	if ( !r_drawentities->integer ) {
		return;
	}

	for ( tr.currentEntityNum = 0;
			tr.currentEntityNum < tr.refdef.num_entities;
			tr.currentEntityNum++ ) {
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];
#ifdef USE_LEGACY_DLIGHTS
		ent->needDlights = 0;
#endif
		tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

		if ( (ent->e.renderfx & RF_FIRST_PERSON) && (tr.viewParms.portalView != PV_NONE) ) {
			continue;
		}

		switch ( ent->e.reType ) {
		case RT_PORTALSURFACE:
			break;
		case RT_SPRITE:
		case RT_BEAM:
		case RT_LIGHTNING:
		case RT_RAIL_CORE:
		case RT_RAIL_RINGS:
			if ( (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE) ) {
				continue;
			}
			shader = R_GetShaderByHandle( ent->e.customShader );
			R_AddDrawSurf( &entitySurface, shader, R_SpriteFogNum( ent ), 0 );
			break;

		case RT_MODEL:
			R_RotateForEntity( ent, &tr.viewParms, &tr.or );

			tr.currentModel = R_GetModelByHandle( ent->e.hModel );
			if (!tr.currentModel) {
				R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
			} else {
				switch ( tr.currentModel->type ) {
				case MOD_MESH:
					R_AddMD3Surfaces( ent );
					break;
				case MOD_MDR:
					R_MDRAddAnimSurfaces( ent );
					break;
				case MOD_IQM:
					R_AddIQMSurfaces( ent );
					break;
				case MOD_BRUSH:
					R_AddBrushModelSurfaces( ent );
					break;
				case MOD_BAD:
					if ( (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE) ) {
						break;
					}
					R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
					break;
				default:
					ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
					break;
				}
			}
			break;
		default:
			ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		}
	}

}

static void R_GenerateDrawSurfs( void ) {
	R_AddWorldSurfaces ();

	R_AddPolygonSurfaces();

	R_SetFarClip();

	R_SetupProjectionZ( &tr.viewParms );

	R_AddEntitySurfaces();
}

void R_RenderView( const viewParms_t *parms ) {
	int		firstDrawSurf;
	int		numDrawSurfs;

	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 ) {
		return;
	}

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;

	R_RotateForViewer();

	R_SetupProjection( &tr.viewParms, r_zproj->value, qtrue );

	R_GenerateDrawSurfs();

	numDrawSurfs = tr.refdef.numDrawSurfs;
	if ( numDrawSurfs > MAX_DRAWSURFS ) {
		numDrawSurfs = MAX_DRAWSURFS;
	}

	R_SortDrawSurfs( tr.refdef.drawSurfs + firstDrawSurf, numDrawSurfs - firstDrawSurf );
}
