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

static qboolean R_CullSurface(msurface_t* surf) {
    if (r_nocull->integer || surf->cullinfo.type == CULLINFO_NONE) {
        return qfalse;
    }

    if (r_nocurves->integer && *surf->data == SF_GRID) {
        return qtrue;
    }

    if (surf->cullinfo.type & CULLINFO_PLANE) {
        float d;
        cullType_t ct;

        if (!r_facePlaneCull->integer) {
            return qfalse;
        }

        ct = surf->shader->cullType;

        if (ct == CT_TWO_SIDED) {
            return qfalse;
        }

        if (tr.viewParms.flags & (VPF_SHADOWMAP | VPF_DEPTHSHADOW)) {
            if (ct == CT_FRONT_SIDED) {
                ct = CT_BACK_SIDED;
            } else {
                ct = CT_FRONT_SIDED;
            }
        }

        if (tr.viewParms.flags & VPF_ORTHOGRAPHIC) {
            d = DotProduct(tr.viewParms.or.axis[0], surf->cullinfo.plane.normal);
            if (ct == CT_FRONT_SIDED) {
                if (d > 0)
                    return qtrue;
            } else {
                if (d < 0)
                    return qtrue;
            }
            return qfalse;
        }

        d = DotProduct(tr.or.viewOrigin, surf->cullinfo.plane.normal);

        if (ct == CT_FRONT_SIDED) {
            if (d < surf->cullinfo.plane.dist - 8) {
                return qtrue;
            }
        } else {
            if (d > surf->cullinfo.plane.dist + 8) {
                return qtrue;
            }
        }

        return qfalse;
    }

    if (surf->cullinfo.type & CULLINFO_SPHERE) {
        int sphereCull;

        if (tr.currentEntityNum != REFENTITYNUM_WORLD) {
            sphereCull = R_CullLocalPointAndRadius(surf->cullinfo.localOrigin, surf->cullinfo.radius);
        } else {
            sphereCull = R_CullPointAndRadius(surf->cullinfo.localOrigin, surf->cullinfo.radius);
        }

        if (sphereCull == CULL_OUT) {
            return qtrue;
        }
    }

    if (surf->cullinfo.type & CULLINFO_BOX) {
        int boxCull;

        if (tr.currentEntityNum != REFENTITYNUM_WORLD) {
            boxCull = R_CullLocalBox(surf->cullinfo.bounds);
        } else {
            boxCull = R_CullBox(surf->cullinfo.bounds);
        }

        if (boxCull == CULL_OUT) {
            return qtrue;
        }
    }

    return qfalse;
}

static int R_DlightSurface(msurface_t* surf, int dlightBits) {
    float d;
    int i;
    dlight_t* dl;

    if (surf->cullinfo.type & CULLINFO_PLANE) {
        for (i = 0; i < tr.refdef.num_dlights; i++) {
            if (!(dlightBits & (1 << i))) {
                continue;
            }
            dl = &tr.refdef.dlights[i];
            d = DotProduct(dl->origin, surf->cullinfo.plane.normal) - surf->cullinfo.plane.dist;
            if (d < -dl->radius || d > dl->radius) {
                dlightBits &= ~(1 << i);
            }
        }
    }

    if (surf->cullinfo.type & CULLINFO_BOX) {
        for (i = 0; i < tr.refdef.num_dlights; i++) {
            if (!(dlightBits & (1 << i))) {
                continue;
            }
            dl = &tr.refdef.dlights[i];
            if (dl->origin[0] - dl->radius > surf->cullinfo.bounds[1][0] || dl->origin[0] + dl->radius < surf->cullinfo.bounds[0][0] || dl->origin[1] - dl->radius > surf->cullinfo.bounds[1][1] || dl->origin[1] + dl->radius < surf->cullinfo.bounds[0][1] || dl->origin[2] - dl->radius > surf->cullinfo.bounds[1][2] || dl->origin[2] + dl->radius < surf->cullinfo.bounds[0][2]) {
                dlightBits &= ~(1 << i);
            }
        }
    }

    if (surf->cullinfo.type & CULLINFO_SPHERE) {
        for (i = 0; i < tr.refdef.num_dlights; i++) {
            if (!(dlightBits & (1 << i))) {
                continue;
            }
            dl = &tr.refdef.dlights[i];
            if (!SpheresIntersect(dl->origin, dl->radius, surf->cullinfo.localOrigin, surf->cullinfo.radius)) {
                dlightBits &= ~(1 << i);
            }
        }
    }

    switch (*surf->data) {
        case SF_FACE:
        case SF_GRID:
        case SF_TRIANGLES:
            ((srfBspSurface_t*)surf->data)->dlightBits = dlightBits;
            break;

        default:
            dlightBits = 0;
            break;
    }

    if (dlightBits) {
        tr.pc.c_dlightSurfaces++;
    } else {
        tr.pc.c_dlightSurfacesCulled++;
    }

    return dlightBits;
}

static int R_PshadowSurface(msurface_t* surf, int pshadowBits) {
    float d;
    int i;
    pshadow_t* ps;

    if (surf->cullinfo.type & CULLINFO_PLANE) {
        for (i = 0; i < tr.refdef.num_pshadows; i++) {
            if (!(pshadowBits & (1 << i))) {
                continue;
            }
            ps = &tr.refdef.pshadows[i];
            d = DotProduct(ps->lightOrigin, surf->cullinfo.plane.normal) - surf->cullinfo.plane.dist;
            if (d < -ps->lightRadius || d > ps->lightRadius) {
                pshadowBits &= ~(1 << i);
            }
        }
    }

    if (surf->cullinfo.type & CULLINFO_BOX) {
        for (i = 0; i < tr.refdef.num_pshadows; i++) {
            if (!(pshadowBits & (1 << i))) {
                continue;
            }
            ps = &tr.refdef.pshadows[i];
            if (ps->lightOrigin[0] - ps->lightRadius > surf->cullinfo.bounds[1][0] || ps->lightOrigin[0] + ps->lightRadius < surf->cullinfo.bounds[0][0] || ps->lightOrigin[1] - ps->lightRadius > surf->cullinfo.bounds[1][1] || ps->lightOrigin[1] + ps->lightRadius < surf->cullinfo.bounds[0][1] || ps->lightOrigin[2] - ps->lightRadius > surf->cullinfo.bounds[1][2] || ps->lightOrigin[2] + ps->lightRadius < surf->cullinfo.bounds[0][2] || BoxOnPlaneSide(surf->cullinfo.bounds[0], surf->cullinfo.bounds[1], &ps->cullPlane) == 2) {
                pshadowBits &= ~(1 << i);
            }
        }
    }

    if (surf->cullinfo.type & CULLINFO_SPHERE) {
        for (i = 0; i < tr.refdef.num_pshadows; i++) {
            if (!(pshadowBits & (1 << i))) {
                continue;
            }
            ps = &tr.refdef.pshadows[i];
            if (!SpheresIntersect(ps->viewOrigin, ps->viewRadius, surf->cullinfo.localOrigin, surf->cullinfo.radius) || DotProduct(surf->cullinfo.localOrigin, ps->cullPlane.normal) - ps->cullPlane.dist < -surf->cullinfo.radius) {
                pshadowBits &= ~(1 << i);
            }
        }
    }

    switch (*surf->data) {
        case SF_FACE:
        case SF_GRID:
        case SF_TRIANGLES:
            ((srfBspSurface_t*)surf->data)->pshadowBits = pshadowBits;
            break;

        default:
            pshadowBits = 0;
            break;
    }

    if (pshadowBits) {
    }

    return pshadowBits;
}

static void R_AddWorldSurface(msurface_t* surf, int dlightBits, int pshadowBits) {

    if (R_CullSurface(surf)) {
        return;
    }

     {
        dlightBits = R_DlightSurface(surf, dlightBits);
        dlightBits = (dlightBits != 0);
    }

     {
        pshadowBits = R_PshadowSurface(surf, pshadowBits);
        pshadowBits = (pshadowBits != 0);
    }

    R_AddDrawSurf(surf->data, surf->shader, surf->fogIndex, dlightBits, pshadowBits, surf->cubemapIndex);
}

void R_AddBrushModelSurfaces(trRefEntity_t* ent) {
    bmodel_t* bmodel;
    int clip;
    model_t* pModel;
    int i;

    pModel = R_GetModelByHandle(ent->e.hModel);

    bmodel = pModel->bmodel;

    clip = R_CullLocalBox(bmodel->bounds);
    if (clip == CULL_OUT) {
        return;
    }

    R_SetupEntityLighting(&tr.refdef, ent);
    R_DlightBmodel(bmodel);

    for (i = 0; i < bmodel->numSurfaces; i++) {
        int surf = bmodel->firstSurface + i;

        if (tr.world->surfacesViewCount[surf] != tr.viewCount) {
            tr.world->surfacesViewCount[surf] = tr.viewCount;
            R_AddWorldSurface(tr.world->surfaces + surf, tr.currentEntity->needDlights, 0);
        }
    }
}

static void R_RecursiveWorldNode(mnode_t* node, uint32_t planeBits, uint32_t dlightBits, uint32_t pshadowBits) {
    do {
        uint32_t newDlights[2];
        uint32_t newPShadows[2];

        if (!(tr.viewParms.flags & VPF_DEPTHSHADOW) && node->visCounts[tr.visIndex] != tr.visCounts[tr.visIndex]) {
            return;
        }

        if (!r_nocull->integer) {
            int r;

            if (planeBits & 1) {
                r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
                if (r == 2) {
                    return;
                }
                if (r == 1) {
                    planeBits &= ~1;
                }
            }

            if (planeBits & 2) {
                r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
                if (r == 2) {
                    return;
                }
                if (r == 1) {
                    planeBits &= ~2;
                }
            }

            if (planeBits & 4) {
                r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
                if (r == 2) {
                    return;
                }
                if (r == 1) {
                    planeBits &= ~4;
                }
            }

            if (planeBits & 8) {
                r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
                if (r == 2) {
                    return;
                }
                if (r == 1) {
                    planeBits &= ~8;
                }
            }

            if (planeBits & 16) {
                r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[4]);
                if (r == 2) {
                    return;
                }
                if (r == 1) {
                    planeBits &= ~16;
                }
            }
        }

        if (node->contents != -1) {
            break;
        }

        newDlights[0] = 0;
        newDlights[1] = 0;
        if (dlightBits) {
            int i;

            for (i = 0; i < tr.refdef.num_dlights; i++) {
                dlight_t* dl;
                float dist;

                if (dlightBits & (1 << i)) {
                    dl = &tr.refdef.dlights[i];
                    dist = DotProduct(dl->origin, node->plane->normal) - node->plane->dist;

                    if (dist > -dl->radius) {
                        newDlights[0] |= (1 << i);
                    }
                    if (dist < dl->radius) {
                        newDlights[1] |= (1 << i);
                    }
                }
            }
        }

        newPShadows[0] = 0;
        newPShadows[1] = 0;
        if (pshadowBits) {
            int i;

            for (i = 0; i < tr.refdef.num_pshadows; i++) {
                pshadow_t* shadow;
                float dist;

                if (pshadowBits & (1 << i)) {
                    shadow = &tr.refdef.pshadows[i];
                    dist = DotProduct(shadow->lightOrigin, node->plane->normal) - node->plane->dist;

                    if (dist > -shadow->lightRadius) {
                        newPShadows[0] |= (1 << i);
                    }
                    if (dist < shadow->lightRadius) {
                        newPShadows[1] |= (1 << i);
                    }
                }
            }
        }

        R_RecursiveWorldNode(node->children[0], planeBits, newDlights[0], newPShadows[0]);

        node = node->children[1];
        dlightBits = newDlights[1];
        pshadowBits = newPShadows[1];
    } while (1);

    {
        int c;
        int surf, *view;

        tr.pc.c_leafs++;

        if (node->mins[0] < tr.viewParms.visBounds[0][0]) {
            tr.viewParms.visBounds[0][0] = node->mins[0];
        }
        if (node->mins[1] < tr.viewParms.visBounds[0][1]) {
            tr.viewParms.visBounds[0][1] = node->mins[1];
        }
        if (node->mins[2] < tr.viewParms.visBounds[0][2]) {
            tr.viewParms.visBounds[0][2] = node->mins[2];
        }

        if (node->maxs[0] > tr.viewParms.visBounds[1][0]) {
            tr.viewParms.visBounds[1][0] = node->maxs[0];
        }
        if (node->maxs[1] > tr.viewParms.visBounds[1][1]) {
            tr.viewParms.visBounds[1][1] = node->maxs[1];
        }
        if (node->maxs[2] > tr.viewParms.visBounds[1][2]) {
            tr.viewParms.visBounds[1][2] = node->maxs[2];
        }

        view = tr.world->marksurfaces + node->firstmarksurface;

        c = node->nummarksurfaces;
        while (c--) {
            surf = *view;
            if (tr.world->surfacesViewCount[surf] != tr.viewCount) {
                tr.world->surfacesViewCount[surf] = tr.viewCount;
                tr.world->surfacesDlightBits[surf] = dlightBits;
                tr.world->surfacesPshadowBits[surf] = pshadowBits;
            } else {
                tr.world->surfacesDlightBits[surf] |= dlightBits;
                tr.world->surfacesPshadowBits[surf] |= pshadowBits;
            }
            view++;
        }
    }
}

static mnode_t* R_PointInLeaf(const vec3_t p) {
    mnode_t* node;
    float d;
    cplane_t* plane;

    if (!tr.world) {
        ri.Error(ERR_DROP, "R_PointInLeaf: bad model");
    }

    node = tr.world->nodes;
    while (1) {
        if (node->contents != -1) {
            break;
        }
        plane = node->plane;
        d = DotProduct(p, plane->normal) - plane->dist;
        if (d > 0) {
            node = node->children[0];
        } else {
            node = node->children[1];
        }
    }

    return node;
}

static const byte* R_ClusterPVS(int cluster) {
    if (!tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters) {
        return NULL;
    }

    return tr.world->vis + cluster * tr.world->clusterBytes;
}

qboolean R_inPVS(const vec3_t p1, const vec3_t p2) {
    mnode_t* leaf;
    byte* vis;

    leaf = R_PointInLeaf(p1);
    vis = ri.CM_ClusterPVS(leaf->cluster);
    leaf = R_PointInLeaf(p2);

    if (!(vis[leaf->cluster >> 3] & (1 << (leaf->cluster & 7)))) {
        return qfalse;
    }
    return qtrue;
}

static void R_MarkLeaves(void) {
    const byte* vis;
    mnode_t *leaf, *parent;
    int i;
    int cluster;

    if (r_lockpvs->integer) {
        return;
    }

    leaf = R_PointInLeaf(tr.viewParms.pvsOrigin);
    cluster = leaf->cluster;

    for (i = 0; i < MAX_VISCOUNTS; i++) {
        if (tr.refdef.areamaskModified || r_showcluster->modified) {
            tr.visClusters[i] = -2;
        } else if (tr.visClusters[i] == cluster) {
            if (tr.visClusters[i] != tr.visClusters[tr.visIndex] && r_showcluster->integer) {
                ri.Printf(PRINT_ALL, "found cluster:%i  area:%i  index:%i\n", cluster, leaf->area, i);
            }
            tr.visIndex = i;
            return;
        }
    }

    tr.visIndex = (tr.visIndex + 1) % MAX_VISCOUNTS;
    tr.visCounts[tr.visIndex]++;
    tr.visClusters[tr.visIndex] = cluster;

    if (r_showcluster->modified || r_showcluster->integer) {
        r_showcluster->modified = qfalse;
        if (r_showcluster->integer) {
            ri.Printf(PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area);
        }
    }

    vis = R_ClusterPVS(tr.visClusters[tr.visIndex]);

    for (i = 0, leaf = tr.world->nodes; i < tr.world->numnodes; i++, leaf++) {
        cluster = leaf->cluster;
        if (cluster < 0 || cluster >= tr.world->numClusters) {
            continue;
        }

        if (vis && !(vis[cluster >> 3] & (1 << (cluster & 7)))) {
            continue;
        }

        if ((tr.refdef.areamask[leaf->area >> 3] & (1 << (leaf->area & 7)))) {
            continue;
        }

        parent = leaf;
        do {
            if (parent->visCounts[tr.visIndex] == tr.visCounts[tr.visIndex])
                break;
            parent->visCounts[tr.visIndex] = tr.visCounts[tr.visIndex];
            parent = parent->parent;
        } while (parent);
    }
}

static void R_AddAdvertisements(uint32_t dlightBits) {
    int i, j, k;

    for (i = 0; i < tr.world->numAdSurfaces; i++) {
        const adSurface_t* ad = &tr.world->adSurfaces[i];
        int surf = ad->bmodel->firstSurface;
        vec3_t delta;

        if (!R_inPVS(tr.viewParms.or.origin, ad->center)) {
            continue;
        }
        if (tr.world->surfacesViewCount[surf] == tr.viewCount) {
            continue;
        }

        VectorSubtract(tr.viewParms.or.origin, ad->center, delta);
        if (DotProduct(ad->normal, delta) < 0.0f) {
            continue;
        }
        tr.world->surfacesViewCount[surf] = tr.viewCount;

        if (!r_nocull->integer) {
            for (j = 0; j < 4; j++) {
                const cplane_t* frust = &tr.viewParms.frustum[j];

                for (k = 0; k < 4; k++) {
                    if (DotProduct(ad->rect[k], frust->normal) > frust->dist) {
                        break;
                    }
                }
                if (k == 4) {
                    break;
                }
            }
            if (j < 4) {
                continue;
            }
        }

        R_AddWorldSurface(tr.world->surfaces + surf, dlightBits, 0);
    }
}

void R_AddWorldSurfaces(void) {
    uint32_t planeBits, dlightBits, pshadowBits;

    if (!r_drawworld->integer) {
        return;
    }

    if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }

    tr.currentEntityNum = REFENTITYNUM_WORLD;
    tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

    if (!(tr.viewParms.flags & VPF_DEPTHSHADOW))
        R_MarkLeaves();

    ClearBounds(tr.viewParms.visBounds[0], tr.viewParms.visBounds[1]);

    if (tr.refdef.num_dlights > MAX_DLIGHTS) {
        tr.refdef.num_dlights = MAX_DLIGHTS;
    }

    if (tr.refdef.num_pshadows > MAX_DRAWN_PSHADOWS) {
        tr.refdef.num_pshadows = MAX_DRAWN_PSHADOWS;
    }

    planeBits = (tr.viewParms.flags & VPF_FARPLANEFRUSTUM) ? 31 : 15;

    if (tr.viewParms.flags & VPF_DEPTHSHADOW) {
        dlightBits = 0;
        pshadowBits = 0;
    } else if (!(tr.viewParms.flags & VPF_SHADOWMAP)) {
        dlightBits = (1ULL << tr.refdef.num_dlights) - 1;
        pshadowBits = (1ULL << tr.refdef.num_pshadows) - 1;
    } else {
        dlightBits = (1ULL << tr.refdef.num_dlights) - 1;
        pshadowBits = 0;
    }

    R_RecursiveWorldNode(tr.world->nodes, planeBits, dlightBits, pshadowBits);

    {
        int i;

        tr.refdef.dlightMask = 0;

        for (i = 0; i < tr.world->numWorldSurfaces; i++) {
            if (tr.world->surfacesViewCount[i] != tr.viewCount)
                continue;

            R_AddWorldSurface(tr.world->surfaces + i, tr.world->surfacesDlightBits[i], tr.world->surfacesPshadowBits[i]);
            tr.refdef.dlightMask |= tr.world->surfacesDlightBits[i];
        }

        tr.refdef.dlightMask = ~tr.refdef.dlightMask;
    }

    if (!(tr.refdef.rdflags & RDF_HYPERSPACE)) {
        R_AddAdvertisements(dlightBits);
    }
}
