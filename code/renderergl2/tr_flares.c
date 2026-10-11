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
    struct flare_s* next;

    int addedFrame;

    qboolean inPortal;
    int frameSceneNum;
    void* surface;
    int fogNum;

    int fadeTime;

    qboolean visible;
    float drawIntensity;

    int windowX, windowY;
    float eyeZ;

    vec3_t origin;
    vec3_t color;
} flare_t;

#define MAX_FLARES 256

flare_t r_flareStructs[MAX_FLARES];
flare_t *r_activeFlares, *r_inactiveFlares;

int flareCoeff;

static void R_SetFlareCoeff(void) {
    if (r_flareCoeff->value == 0.0f)
        flareCoeff = atof(FLARE_STDCOEFF);
    else
        flareCoeff = r_flareCoeff->value;
}

void R_ClearFlares(void) {
    int i;

    Com_Memset(r_flareStructs, 0, sizeof(r_flareStructs));
    r_activeFlares = NULL;
    r_inactiveFlares = NULL;

    for (i = 0; i < MAX_FLARES; i++) {
        r_flareStructs[i].next = r_inactiveFlares;
        r_inactiveFlares = &r_flareStructs[i];
    }

    R_SetFlareCoeff();
}

void RB_AddFlare(void* surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal) {
    int i;
    flare_t* f;
    vec3_t local;
    float d = 1;
    vec4_t eye, clip, normalized, window;

    backEnd.pc.c_flareAdds++;

    if (normal && (normal[0] || normal[1] || normal[2])) {
        VectorSubtract(backEnd.viewParms.or.origin, point, local);
        VectorNormalizeFast(local);
        d = DotProduct(local, normal);

        if (d < 0)
            return;
    }

    R_TransformModelToClip(point, backEnd.or.modelMatrix,
                           backEnd.viewParms.projectionMatrix, eye, clip);

    for (i = 0; i < 3; i++) {
        if (clip[i] >= clip[3] || clip[i] <= -clip[3]) {
            return;
        }
    }

    R_TransformClipToWindow(clip, &backEnd.viewParms, normalized, window);

    if (window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth || window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight) {
        return;
    }

    for (f = r_activeFlares; f; f = f->next) {
        if (f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->inPortal == backEnd.viewParms.isPortal) {
            break;
        }
    }

    if (!f) {
        if (!r_inactiveFlares) {
            return;
        }
        f = r_inactiveFlares;
        r_inactiveFlares = r_inactiveFlares->next;
        f->next = r_activeFlares;
        r_activeFlares = f;

        f->surface = surface;
        f->frameSceneNum = backEnd.viewParms.frameSceneNum;
        f->inPortal = backEnd.viewParms.isPortal;
        f->addedFrame = -1;
    }

    if (f->addedFrame != backEnd.viewParms.frameCount - 1) {
        f->visible = qfalse;
        f->fadeTime = backEnd.refdef.time - 2000;
    }

    f->addedFrame = backEnd.viewParms.frameCount;
    f->fogNum = fogNum;

    VectorCopy(point, f->origin);
    VectorCopy(color, f->color);

    VectorScale(f->color, d, f->color);

    f->windowX = backEnd.viewParms.viewportX + window[0];
    f->windowY = backEnd.viewParms.viewportY + window[1];

    f->eyeZ = eye[2];
}

void RB_AddDlightFlares(void) {
    dlight_t* l;
    int i, j, k;
    fog_t* fog = NULL;

    if (!r_flares->integer) {
        return;
    }

    l = backEnd.refdef.dlights;

    if (tr.world)
        fog = tr.world->fogs;

    for (i = 0; i < backEnd.refdef.num_dlights; i++, l++) {
        if (fog) {
            for (j = 1; j < tr.world->numfogs; j++) {
                fog = &tr.world->fogs[j];
                for (k = 0; k < 3; k++) {
                    if (l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k]) {
                        break;
                    }
                }
                if (k == 3) {
                    break;
                }
            }
            if (j == tr.world->numfogs) {
                j = 0;
            }
        } else
            j = 0;

        RB_AddFlare((void*)l, j, l->origin, l->color, NULL);
    }
}

void RB_TestFlare(flare_t* f) {
    float depth;
    qboolean visible;
    float fade;
    float screenZ;
    FBO_t* oldFbo;

    backEnd.pc.c_flareTests++;

    glState.finishCalled = qfalse;

    oldFbo = glState.currentFBO;
    if (tr.msaaResolveFbo) {
        FBO_Bind(tr.msaaResolveFbo);
    }

    qglReadPixels(f->windowX, f->windowY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

    if (tr.msaaResolveFbo) {
        FBO_Bind(oldFbo);
    }

    screenZ = backEnd.viewParms.projectionMatrix[14] /
              ((2 * depth - 1) * backEnd.viewParms.projectionMatrix[11] - backEnd.viewParms.projectionMatrix[10]);

    visible = (-f->eyeZ - -screenZ) < 24;

    if (visible) {
        if (!f->visible) {
            f->visible = qtrue;
            f->fadeTime = backEnd.refdef.time - 1;
        }
        fade = ((backEnd.refdef.time - f->fadeTime) / 1000.0f) * r_flareFade->value;
    } else {
        if (f->visible) {
            f->visible = qfalse;
            f->fadeTime = backEnd.refdef.time - 1;
        }
        fade = 1.0f - ((backEnd.refdef.time - f->fadeTime) / 1000.0f) * r_flareFade->value;
    }

    if (fade < 0) {
        fade = 0;
    }
    if (fade > 1) {
        fade = 1;
    }

    f->drawIntensity = fade;
}

void RB_RenderFlare(flare_t* f) {
    float size;
    vec3_t color;
    int iColor[3];
    float distance, intensity, factor;
    byte fogFactors[3] = {255, 255, 255};

    backEnd.pc.c_flareRenders++;

    if (f->eyeZ > -1.0f)
        distance = 1.0f;
    else
        distance = -f->eyeZ;

    size = backEnd.viewParms.viewportWidth * (r_flareSize->value / 640.0f + 8 / distance);

    factor = distance + size * sqrt(flareCoeff);

    intensity = flareCoeff * size * size / (factor * factor);

    VectorScale(f->color, f->drawIntensity * intensity, color);

    if (tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs) {
        tess.numVertexes = 1;
        VectorCopy(f->origin, tess.xyz[0]);
        tess.fogNum = f->fogNum;

        RB_CalcModulateColorsByFog(fogFactors);

        if (!(fogFactors[0] || fogFactors[1] || fogFactors[2]))
            return;
    }

    iColor[0] = color[0] * fogFactors[0] * 257;
    iColor[1] = color[1] * fogFactors[1] * 257;
    iColor[2] = color[2] * fogFactors[2] * 257;

    RB_BeginSurface(tr.flareShader, f->fogNum, 0);

    tess.xyz[tess.numVertexes][0] = f->windowX - size;
    tess.xyz[tess.numVertexes][1] = f->windowY - size;
    tess.texCoords[tess.numVertexes][0] = 0;
    tess.texCoords[tess.numVertexes][1] = 0;
    tess.color[tess.numVertexes][0] = iColor[0];
    tess.color[tess.numVertexes][1] = iColor[1];
    tess.color[tess.numVertexes][2] = iColor[2];
    tess.color[tess.numVertexes][3] = 65535;
    tess.numVertexes++;

    tess.xyz[tess.numVertexes][0] = f->windowX - size;
    tess.xyz[tess.numVertexes][1] = f->windowY + size;
    tess.texCoords[tess.numVertexes][0] = 0;
    tess.texCoords[tess.numVertexes][1] = 1;
    tess.color[tess.numVertexes][0] = iColor[0];
    tess.color[tess.numVertexes][1] = iColor[1];
    tess.color[tess.numVertexes][2] = iColor[2];
    tess.color[tess.numVertexes][3] = 65535;
    tess.numVertexes++;

    tess.xyz[tess.numVertexes][0] = f->windowX + size;
    tess.xyz[tess.numVertexes][1] = f->windowY + size;
    tess.texCoords[tess.numVertexes][0] = 1;
    tess.texCoords[tess.numVertexes][1] = 1;
    tess.color[tess.numVertexes][0] = iColor[0];
    tess.color[tess.numVertexes][1] = iColor[1];
    tess.color[tess.numVertexes][2] = iColor[2];
    tess.color[tess.numVertexes][3] = 65535;
    tess.numVertexes++;

    tess.xyz[tess.numVertexes][0] = f->windowX + size;
    tess.xyz[tess.numVertexes][1] = f->windowY - size;
    tess.texCoords[tess.numVertexes][0] = 1;
    tess.texCoords[tess.numVertexes][1] = 0;
    tess.color[tess.numVertexes][0] = iColor[0];
    tess.color[tess.numVertexes][1] = iColor[1];
    tess.color[tess.numVertexes][2] = iColor[2];
    tess.color[tess.numVertexes][3] = 65535;
    tess.numVertexes++;

    tess.indexes[tess.numIndexes++] = 0;
    tess.indexes[tess.numIndexes++] = 1;
    tess.indexes[tess.numIndexes++] = 2;
    tess.indexes[tess.numIndexes++] = 0;
    tess.indexes[tess.numIndexes++] = 2;
    tess.indexes[tess.numIndexes++] = 3;

    RB_EndSurface();
}

void RB_RenderFlares(void) {
    flare_t* f;
    flare_t** prev;
    qboolean draw;
    mat4_t oldmodelview, oldprojection, matrix;

    if (!r_flares->integer) {
        return;
    }

    if (r_flares->modified) {
        if (qglesMajorVersion >= 1 && !glRefConfig.readDepth) {
            ri.Printf(PRINT_WARNING, "OpenGL ES needs GL_NV_read_depth to read depth to determine if flares are visible\n");
            ri.Cvar_Set("r_flares", "0");
        }
        r_flares->modified = qfalse;
    }

    if (r_flareCoeff->modified) {
        R_SetFlareCoeff();
        r_flareCoeff->modified = qfalse;
    }

    backEnd.currentEntity = &tr.worldEntity;
    backEnd.or = backEnd.viewParms.world;

    draw = qfalse;
    prev = &r_activeFlares;
    while ((f = *prev) != NULL) {
        if (f->addedFrame < backEnd.viewParms.frameCount - 1) {
            *prev = f->next;
            f->next = r_inactiveFlares;
            r_inactiveFlares = f;
            continue;
        }

        f->drawIntensity = 0;
        if (f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->inPortal == backEnd.viewParms.isPortal) {
            RB_TestFlare(f);
            if (f->drawIntensity) {
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

    if (!draw) {
        return;
    }

    Mat4Copy(glState.projection, oldprojection);
    Mat4Copy(glState.modelview, oldmodelview);
    Mat4Identity(matrix);
    GL_SetModelviewMatrix(matrix);
    Mat4Ortho(backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
              backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
              -99999, 99999, matrix);
    GL_SetProjectionMatrix(matrix);

    for (f = r_activeFlares; f; f = f->next) {
        if (f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->inPortal == backEnd.viewParms.isPortal && f->drawIntensity) {
            RB_RenderFlare(f);
        }
    }

    GL_SetProjectionMatrix(oldprojection);
    GL_SetModelviewMatrix(oldmodelview);
}
