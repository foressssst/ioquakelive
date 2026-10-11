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

static qboolean	R_CullTriSurf( srfTriangles_t *cv ) {
	int 	boxCull;

	boxCull = R_CullLocalBox( cv->bounds );

	if ( boxCull == CULL_OUT ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean	R_CullGrid( srfGridMesh_t *cv ) {
	int 	boxCull;
	int 	sphereCull;

	if ( r_nocurves->integer ) {
		return qtrue;
	}

	if ( tr.currentEntityNum != REFENTITYNUM_WORLD ) {
		sphereCull = R_CullLocalPointAndRadius( cv->localOrigin, cv->meshRadius );
	} else {
		sphereCull = R_CullPointAndRadius( cv->localOrigin, cv->meshRadius );
	}

	if ( sphereCull == CULL_OUT )
	{
		tr.pc.c_sphere_cull_patch_out++;
		return qtrue;
	}
	else if ( sphereCull == CULL_CLIP )
	{
		tr.pc.c_sphere_cull_patch_clip++;

		boxCull = R_CullLocalBox( cv->meshBounds );

		if ( boxCull == CULL_OUT ) 
		{
			tr.pc.c_box_cull_patch_out++;
			return qtrue;
		}
		else if ( boxCull == CULL_IN )
		{
			tr.pc.c_box_cull_patch_in++;
		}
		else
		{
			tr.pc.c_box_cull_patch_clip++;
		}
	}
	else
	{
		tr.pc.c_sphere_cull_patch_in++;
	}

	return qfalse;
}

static qboolean	R_CullSurface( const surfaceType_t *surface, shader_t *shader ) {
	srfSurfaceFace_t *sface;
	float			d;

	if ( r_nocull->integer ) {
		return qfalse;
	}

	if ( *surface == SF_GRID ) {
		return R_CullGrid( (srfGridMesh_t *)surface );
	}

	if ( *surface == SF_TRIANGLES ) {
		return R_CullTriSurf( (srfTriangles_t *)surface );
	}

	if ( *surface != SF_FACE ) {
		return qfalse;
	}

	if ( shader->cullType == CT_TWO_SIDED ) {
		return qfalse;
	}

	if ( !r_facePlaneCull->integer ) {
		return qfalse;
	}

	sface = ( srfSurfaceFace_t * ) surface;
	d = DotProduct (tr.or.viewOrigin, sface->plane.normal);

	if ( shader->cullType == CT_FRONT_SIDED ) {
		if ( d < sface->plane.dist - 8 ) {
			return qtrue;
		}
	} else {
		if ( d > sface->plane.dist + 8 ) {
			return qtrue;
		}
	}

	return qfalse;
}

#ifdef USE_PMLIGHT
qboolean R_LightCullBounds( const dlight_t* dl, const vec3_t mins, const vec3_t maxs )
{
	if ( dl->linear ) {
		if (dl->transformed[0] - dl->radius > maxs[0] && dl->transformed2[0] - dl->radius > maxs[0] )
			return qtrue;
		if (dl->transformed[0] + dl->radius < mins[0] && dl->transformed2[0] + dl->radius < mins[0] )
			return qtrue;

		if (dl->transformed[1] - dl->radius > maxs[1] && dl->transformed2[1] - dl->radius > maxs[1] )
			return qtrue;
		if (dl->transformed[1] + dl->radius < mins[1] && dl->transformed2[1] + dl->radius < mins[1] )
			return qtrue;

		if (dl->transformed[2] - dl->radius > maxs[2] && dl->transformed2[2] - dl->radius > maxs[2] )
			return qtrue;
		if (dl->transformed[2] + dl->radius < mins[2] && dl->transformed2[2] + dl->radius < mins[2] )
			return qtrue;

		return qfalse;
	}

	if (dl->transformed[0] - dl->radius > maxs[0])
		return qtrue;
	if (dl->transformed[0] + dl->radius < mins[0])
		return qtrue;

	if (dl->transformed[1] - dl->radius > maxs[1])
		return qtrue;
	if (dl->transformed[1] + dl->radius < mins[1])
		return qtrue;

	if (dl->transformed[2] - dl->radius > maxs[2])
		return qtrue;
	if (dl->transformed[2] + dl->radius < mins[2])
		return qtrue;

	return qfalse;
}

static qboolean R_LightCullFace( const srfSurfaceFace_t* face, const dlight_t* dl )
{
	float d = DotProduct( dl->transformed, face->plane.normal ) - face->plane.dist;
	if ( dl->linear )
	{
		float d2 = DotProduct( dl->transformed2, face->plane.normal ) - face->plane.dist;
		if ( (d < -dl->radius) && (d2 < -dl->radius) )
			return qtrue;
		if ( (d > dl->radius) && (d2 > dl->radius) ) 
			return qtrue;
	} 
	else 
	{
		if ( (d < -dl->radius) || (d > dl->radius) )
			return qtrue;
	}

	return qfalse;
}

static qboolean R_LightCullSurface( const surfaceType_t* surface, const dlight_t* dl )
{
	switch (*surface) {
	case SF_FACE:
		return R_LightCullFace( (const srfSurfaceFace_t*)surface, dl );
	case SF_GRID: {
		const srfGridMesh_t* grid = (const srfGridMesh_t*)surface;
		return R_LightCullBounds( dl, grid->meshBounds[0], grid->meshBounds[1] );
		}
	case SF_TRIANGLES: {
		const srfTriangles_t* tris = (const srfTriangles_t*)surface;
		return R_LightCullBounds( dl, tris->bounds[0], tris->bounds[1] );
		}
	default:
		return qfalse;
	};
}
#endif

#ifdef USE_LEGACY_DLIGHTS
static int R_DlightFace( srfSurfaceFace_t *face, int dlightBits ) {
	float		d;
	int			i;
	const dlight_t	*dl;

	for ( i = 0; i < tr.refdef.num_dlights; i++ ) {
		if ( ! ( dlightBits & ( 1 << i ) ) ) {
			continue;
		}
		dl = &tr.refdef.dlights[i];
		d = DotProduct( dl->transformed, face->plane.normal ) - face->plane.dist;
		if ( d < -dl->radius || d > dl->radius ) {
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits ) {
		tr.pc.c_dlightSurfacesCulled++;
	}

	face->dlightBits = dlightBits;
	return dlightBits;
}

static int R_DlightGrid( srfGridMesh_t *grid, int dlightBits ) {
	int			i;
	const dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
		if ( ! ( dlightBits & ( 1 << i ) ) ) {
			continue;
		}
		dl = &tr.refdef.dlights[i];
		if ( dl->origin[0] - dl->radius > grid->meshBounds[1][0]
			|| dl->origin[0] + dl->radius < grid->meshBounds[0][0]
			|| dl->origin[1] - dl->radius > grid->meshBounds[1][1]
			|| dl->origin[1] + dl->radius < grid->meshBounds[0][1]
			|| dl->origin[2] - dl->radius > grid->meshBounds[1][2]
			|| dl->origin[2] + dl->radius < grid->meshBounds[0][2] ) {
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits ) {
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits = dlightBits;
	return dlightBits;
}

static int R_DlightTrisurf( srfTriangles_t *surf, int dlightBits ) {
	surf->dlightBits = dlightBits;
	return dlightBits;
#if 0
	int			i;
	const dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
		if ( ! ( dlightBits & ( 1 << i ) ) ) {
			continue;
		}
		dl = &tr.refdef.dlights[i];
		if ( dl->origin[0] - dl->radius > grid->meshBounds[1][0]
			|| dl->origin[0] + dl->radius < grid->meshBounds[0][0]
			|| dl->origin[1] - dl->radius > grid->meshBounds[1][1]
			|| dl->origin[1] + dl->radius < grid->meshBounds[0][1]
			|| dl->origin[2] - dl->radius > grid->meshBounds[1][2]
			|| dl->origin[2] + dl->radius < grid->meshBounds[0][2] ) {
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits ) {
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits = dlightBits;
	return dlightBits;
#endif
}

static int R_DlightSurface( msurface_t *surf, int dlightBits ) {
	if ( *surf->data == SF_FACE ) {
		dlightBits = R_DlightFace( (srfSurfaceFace_t *)surf->data, dlightBits );
	} else if ( *surf->data == SF_GRID ) {
		dlightBits = R_DlightGrid( (srfGridMesh_t *)surf->data, dlightBits );
	} else if ( *surf->data == SF_TRIANGLES ) {
		dlightBits = R_DlightTrisurf( (srfTriangles_t *)surf->data, dlightBits );
	} else {
		dlightBits = 0;
	}

	if ( dlightBits ) {
		tr.pc.c_dlightSurfaces++;
	}

	return dlightBits;
}
#endif

static void R_AddWorldSurface( msurface_t *surf, int dlightBits ) {
	if ( surf->viewCount == tr.viewCount ) {
		return;
	}

	surf->viewCount = tr.viewCount;

	if ( R_CullSurface( surf->data, surf->shader ) ) {
		return;
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer ) 
#endif
	{
		surf->vcVisible = tr.viewCount;
		R_AddDrawSurf( surf->data, surf->shader, surf->fogIndex, 0 );
		return;
	}
#endif

#ifdef USE_LEGACY_DLIGHTS
	if ( dlightBits ) {
		dlightBits = R_DlightSurface( surf, dlightBits );
		dlightBits = ( dlightBits != 0 );
	}

	R_AddDrawSurf( surf->data, surf->shader, surf->fogIndex, dlightBits );
#endif
}

#ifdef USE_PMLIGHT
static void R_AddLitSurface( msurface_t *surf, const dlight_t *light )
{

	if ( surf->vcVisible != tr.viewCount ) {
		return;
	}

	if ( surf->shader->lightingStage < 0 ) {
		return;
	}

	if ( surf->lightCount == tr.lightCount )
		return;

	surf->lightCount = tr.lightCount;

	if ( R_LightCullSurface( surf->data, light ) ) {
		tr.pc.c_lit_culls++;
		return;
	}

	R_AddLitSurf( surf->data, surf->shader, surf->fogIndex );
}

static void R_RecursiveLightNode( const mnode_t* node )
{
	qboolean children[2];
	msurface_t** mark;
	msurface_t* surf;
	float d;
	int c;
	do {
		if ( node->visframe != tr.visCount )
			return;

		if ( node->contents != CONTENTS_NODE )
			break;

		children[0] = children[1] = qfalse;

		d = DotProduct( tr.light->origin, node->plane->normal ) - node->plane->dist;
		if ( d > -tr.light->radius ) {
			children[0] = qtrue;
		}
		if ( d < tr.light->radius ) {
			children[1] = qtrue;
		}

		if ( tr.light->linear ) {
			d = DotProduct( tr.light->origin2, node->plane->normal ) - node->plane->dist;
			if ( d > -tr.light->radius ) {
				children[0] = qtrue;
			}
			if ( d < tr.light->radius ) {
				children[1] = qtrue;
			}
		}

		if ( children[0] && children[1] ) {
			R_RecursiveLightNode( node->children[0] );
			node = node->children[1];
		}
		else if ( children[0] ) {
			node = node->children[0];
		}
		else if ( children[1] ) {
			node = node->children[1];
		}
		else {
			return;
		}

	} while ( 1 );

	tr.pc.c_lit_leafs++;

	c = node->nummarksurfaces;
	mark = node->firstmarksurface;
	while ( c-- ) {
		surf = *mark;
		R_AddLitSurface( surf, tr.light );
		mark++;
	}
}
#endif

void R_AddBrushModelSurfaces ( trRefEntity_t *ent ) {
	bmodel_t	*bmodel;
	int			clip;
	const model_t		*pModel;
	int			i;

	pModel = R_GetModelByHandle( ent->e.hModel );

	bmodel = pModel->bmodel;

	clip = R_CullLocalBox( bmodel->bounds );
	if ( clip == CULL_OUT ) {
		return;
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer ) 
#endif
	{
		dlight_t *dl;
		int s;

		for ( s = 0; s < bmodel->numSurfaces; s++ ) {
			R_AddWorldSurface( bmodel->firstSurface + s, 0 );
		}

		R_SetupEntityLighting( &tr.refdef, ent );

		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.or );

		for ( i = 0; i < tr.viewParms.num_dlights; i++ ) {
			dl = &tr.viewParms.dlights[i];
			if ( !R_LightCullBounds( dl, bmodel->bounds[0], bmodel->bounds[1] ) ) {
				tr.lightCount++;
				tr.light = dl;
				for ( s = 0; s < bmodel->numSurfaces; s++ ) {
					R_AddLitSurface( bmodel->firstSurface + s, dl );
				}
			}
		}
		return;
	}
#endif

#ifdef USE_LEGACY_DLIGHTS
	R_SetupEntityLighting( &tr.refdef, ent );
	R_DlightBmodel( bmodel );

	for ( i = 0 ; i < bmodel->numSurfaces ; i++ ) {
		R_AddWorldSurface( bmodel->firstSurface + i, tr.currentEntity->needDlights );
	}
#endif
}

static void R_RecursiveWorldNode( mnode_t *node, unsigned int planeBits, unsigned int dlightBits ) {

	do {
		unsigned int newDlights[2];

		if (node->visframe != tr.visCount) {
			return;
		}

		if ( !r_nocull->integer ) {
			int		r;

			if ( planeBits & 1 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;
				}
				if ( r == 1 ) {
					planeBits &= ~1;
				}
			}

			if ( planeBits & 2 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;
				}
				if ( r == 1 ) {
					planeBits &= ~2;
				}
			}

			if ( planeBits & 4 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;
				}
				if ( r == 1 ) {
					planeBits &= ~4;
				}
			}

			if ( planeBits & 8 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;
				}
				if ( r == 1 ) {
					planeBits &= ~8;
				}
			}

		}

		if ( node->contents != CONTENTS_NODE ) {
			break;
		}

		newDlights[0] = 0;
		newDlights[1] = 0;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
		if ( !r_dlightMode->integer )
#endif
		if ( dlightBits ) {
			int	i;

			for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
				const dlight_t	*dl;
				float		dist;

				if ( dlightBits & ( 1 << i ) ) {
					dl = &tr.refdef.dlights[i];
					dist = DotProduct( dl->origin, node->plane->normal ) - node->plane->dist;

					if ( dist > -dl->radius ) {
						newDlights[0] |= ( 1 << i );
					}
					if ( dist < dl->radius ) {
						newDlights[1] |= ( 1 << i );
					}
				}
			}
		}
#endif

		R_RecursiveWorldNode( node->children[0], planeBits, newDlights[0] );

		node = node->children[1];
#ifdef USE_LEGACY_DLIGHTS
		dlightBits = newDlights[1];
#endif
	} while ( 1 );

	{
		int			c;
		msurface_t	*surf, **mark;

		tr.pc.c_leafs++;

		if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--) {
			surf = *mark;
			R_AddWorldSurface( surf, dlightBits );
			mark++;
		}
	}
}

static mnode_t *R_PointInLeaf( const vec3_t p ) {
	mnode_t		*node;
	float		d;
	const cplane_t	*plane;

	if ( !tr.world ) {
		ri.Error (ERR_DROP, "R_PointInLeaf: bad model");
	}

	node = tr.world->nodes;
	while( 1 ) {
		if (node->contents != CONTENTS_NODE ) {
			break;
		}
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) {
			node = node->children[0];
		} else {
			node = node->children[1];
		}
	}

	return node;
}

static const byte *R_ClusterPVS (int cluster) {
	if ( !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters ) {
		return tr.world->novis;
	}

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

qboolean R_inPVS( const vec3_t p1, const vec3_t p2 ) {
	const mnode_t *leaf;
	const byte	*vis;

	leaf = R_PointInLeaf( p1 );
	if ( leaf->cluster < 0 )
		return qfalse;
	vis = ri.CM_ClusterPVS( leaf->cluster );
	leaf = R_PointInLeaf( p2 );
	if ( leaf->cluster < 0 )
		return qfalse;

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) ) {
		return qfalse;
	}
	return qtrue;
}

static void R_MarkLeaves (void) {
	const byte	*vis;
	mnode_t	*leaf, *parent;
	int		i;
	int		cluster;

	if ( r_lockpvs->integer ) {
		return;
	}

	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;

	if ( tr.viewCluster == cluster && !tr.refdef.areamaskModified 
		&& !r_showcluster->modified ) {
		return;
	}

	if ( r_showcluster->modified || r_showcluster->integer ) {
		r_showcluster->modified = qfalse;
		if ( r_showcluster->integer ) {
			ri.Printf( PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area );
		}
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if ( r_novis->integer || tr.viewCluster == -1 ) {
		for (i=0 ; i<tr.world->numnodes ; i++) {
			if (tr.world->nodes[i].contents != CONTENTS_SOLID) {
				tr.world->nodes[i].visframe = tr.visCount;
			}
		}
		return;
	}

	vis = R_ClusterPVS (tr.viewCluster);

	for (i=0,leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++) {
		cluster = leaf->cluster;
		if ( cluster < 0 || cluster >= tr.world->numClusters ) {
			continue;
		}

		if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
			continue;
		}

		if ( leaf->area >= 0 && (tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7))) ) {
			continue;
		}

		parent = leaf;
		do {
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}

static void R_AddAdvertisements( unsigned int dlightBits ) {
	int i, j, k;

	for ( i = 0; i < tr.world->numAdSurfaces; i++ ) {
		const adSurface_t *ad = &tr.world->adSurfaces[i];
		msurface_t *surf = ad->bmodel->firstSurface;
		vec3_t delta;

		if ( !R_inPVS( tr.viewParms.or.origin, ad->center ) ) {
			continue;
		}
		if ( surf->viewCount == tr.viewCount ) {
			continue;
		}

		VectorSubtract( tr.viewParms.or.origin, ad->center, delta );
		if ( DotProduct( ad->normal, delta ) < 0.0f ) {
			continue;
		}

		if ( !r_nocull->integer ) {
			for ( j = 0; j < 4; j++ ) {
				const cplane_t *frust = &tr.viewParms.frustum[j];

				for ( k = 0; k < 4; k++ ) {
					if ( DotProduct( ad->rect[k], frust->normal ) > frust->dist ) {
						break;
					}
				}
				if ( k == 4 ) {
					break;
				}
			}
			if ( j < 4 ) {
				surf->viewCount = tr.viewCount;
				continue;
			}
		}

		R_AddWorldSurface( surf, dlightBits );
	}
}

#ifdef USE_PMLIGHT
static void R_LightAdvertisements( const dlight_t *dl ) {
	int i;

	for ( i = 0; i < tr.world->numAdSurfaces; i++ ) {
		const bmodel_t *bmodel = tr.world->adSurfaces[i].bmodel;

		if ( !R_LightCullBounds( dl, bmodel->bounds[0], bmodel->bounds[1] ) ) {
			R_AddLitSurface( bmodel->firstSurface, dl );
		}
	}
}
#endif

void R_AddWorldSurfaces( void ) {
#ifdef USE_PMLIGHT
	dlight_t* dl;
	int i;
#endif

	if ( !r_drawworld->integer ) {
		return;
	}

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	R_MarkLeaves ();

	ClearBounds( tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );

	if ( tr.refdef.num_dlights > MAX_DLIGHTS ) {
		tr.refdef.num_dlights = MAX_DLIGHTS;
	}

	R_RecursiveWorldNode( tr.world->nodes, 15, ( 1ULL << tr.refdef.num_dlights ) - 1 );

	if ( !( tr.refdef.rdflags & RDF_HYPERSPACE ) ) {
		R_AddAdvertisements( ( 1ULL << tr.refdef.num_dlights ) - 1 );
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( !r_dlightMode->integer )
		return;
#endif

	R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.viewParms.world );
	for ( i = 0; i < tr.viewParms.num_dlights; i++ ) 
	{
		dl = &tr.viewParms.dlights[i];	
		dl->head = dl->tail = NULL;
		if ( R_CullDlight( dl ) == CULL_OUT ) {
			tr.pc.c_light_cull_out++;
			continue;
		}
		tr.pc.c_light_cull_in++;
		tr.lightCount++;
		tr.light = dl;
		R_RecursiveLightNode( tr.world->nodes );
		R_LightAdvertisements( dl );
	}
#endif
}
