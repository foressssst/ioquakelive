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

cvar_t* r_bloomActive;
cvar_t* r_bloomBlurFalloff;
cvar_t* r_bloomBlurRadius;
cvar_t* r_bloomBlurScale;
cvar_t* r_bloomBrightThreshold;
cvar_t* r_bloomIntensity;
cvar_t* r_bloomPasses;
cvar_t* r_bloomSaturation;
cvar_t* r_bloomSceneIntensity;
cvar_t* r_bloomSceneSaturation;
cvar_t* r_colorCorrectActive;
cvar_t* r_contrast;
cvar_t* r_enableBloom;
cvar_t* r_enableColorCorrect;
cvar_t* r_enablePostProcess;
cvar_t* r_floatingPointFBOs;
cvar_t* r_postProcessActive;

void R_QLPostRegister(void) {
    r_bloomActive = ri.Cvar_Get("r_bloomActive", "0", CVAR_ROM | CVAR_TEMP | CVAR_USERSAVE);
    r_bloomBlurFalloff = ri.Cvar_GetRange("r_bloomBlurFalloff", "0.0", "0.75", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomBlurRadius = ri.Cvar_GetRange("r_bloomBlurRadius", "5", "1", "12", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomBlurScale = ri.Cvar_GetRange("r_bloomBlurScale", "0.0", "1.0", "2.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomBrightThreshold = ri.Cvar_GetRange("r_bloomBrightThreshold", "0.25", "0.0", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomIntensity = ri.Cvar_GetRange("r_bloomIntensity", "0.5", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomPasses = ri.Cvar_GetRange("r_bloomPasses", "1", "1", "2", CVAR_ARCHIVE | CVAR_LATCH | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomSaturation = ri.Cvar_GetRange("r_bloomSaturation", "0.8", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomSceneIntensity = ri.Cvar_GetRange("r_bloomSceneIntensity", "1.0", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomSceneSaturation = ri.Cvar_GetRange("r_bloomSceneSaturation", "1.0", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_colorCorrectActive = ri.Cvar_Get("r_colorCorrectActive", "0", CVAR_ROM | CVAR_TEMP);
    r_contrast = ri.Cvar_Get("r_contrast", "1.0", CVAR_ARCHIVE | CVAR_USERSAVE);
    r_enableBloom = ri.Cvar_Get("r_enableBloom", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE);
    r_enableColorCorrect = ri.Cvar_Get("r_enableColorCorrect", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE);
    r_enablePostProcess = ri.Cvar_Get("r_enablePostProcess", "1", CVAR_ARCHIVE | CVAR_LATCH | CVAR_USERSAVE);
    r_floatingPointFBOs = ri.Cvar_Get("r_floatingPointFBOs", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_postProcessActive = ri.Cvar_Get("r_postProcessActive", "0", CVAR_ROM | CVAR_TEMP);
}

static int R_BloomPasses(void) {
    return r_bloomPasses->value > 1.0f ? 2 : 1;
}

void R_QLPostInit(void) {
    ri.Cvar_Set("r_postProcessActive", "0");
    ri.Cvar_Set("r_colorCorrectActive", "0");
    ri.Cvar_Set("r_bloomActive", "0");

    if (!r_enablePostProcess->integer || !glRefConfig.framebufferObject || !glRefConfig.framebufferBlit) {
        return;
    }

    ri.Cvar_Set("r_postProcessActive", "1");

    if (r_enableColorCorrect->integer) {
        ri.Cvar_Set("r_colorCorrectActive", "1");
    }

    if (r_enableBloom->integer) {
        ri.Cvar_Set("r_bloomActive", "1");
    }
}

void R_QLPostCreateImages(int width, int height, int format) {
    int i, w, h;

    if (!r_bloomActive->integer) {
        return;
    }

    for (i = 1; i < 8; i++) {
        if (i < 5) {
            w = width / 2;
            h = height / 2;
        } else if (R_BloomPasses() == 2) {
            w = width / 4;
            h = height / 4;
        } else {
            break;
        }

        tr.qlBloomImage[i] = R_CreateImage(va("*qlBloom%d", i), NULL, w, h, IMGTYPE_COLORALPHA, IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE, format);
    }
}

void R_QLPostCreateFBOs(void) {
    int i;

    if (!r_postProcessActive->integer) {
        return;
    }

    if (!tr.renderFbo || !tr.screenScratchFbo) {
        if (r_colorCorrectActive->integer) {
            ri.Printf(PRINT_ALL, "Color Correct Failure - unable to create backbuffer texture. Color Correct effect disabled\n");
            ri.Cvar_Set("r_colorCorrectActive", "0");
        }

        if (r_bloomActive->integer) {
            ri.Printf(PRINT_ALL, "Bloom Failure - unable to create backbuffer texture. Bloom effect disabled\n");
            ri.Cvar_Set("r_bloomActive", "0");
        }

        R_SetColorMappings();
        return;
    }

    if (!r_bloomActive->integer) {
        return;
    }

    for (i = 1; i < 8 && tr.qlBloomImage[i]; i++) {
        tr.qlBloomFbo[i] = FBO_Create(va("_qlBloom%d", i), tr.qlBloomImage[i]->width, tr.qlBloomImage[i]->height);
        FBO_AttachImage(tr.qlBloomFbo[i], tr.qlBloomImage[i], GL_COLOR_ATTACHMENT0, 0);

        if (!R_CheckFBO(tr.qlBloomFbo[i])) {
            ri.Printf(PRINT_ALL, "Bloom Failure - unable to create FBO. Bloom effect disabled\n");
            ri.Cvar_Set("r_bloomActive", "0");
            return;
        }
    }
}

void RE_BeginBloomScene(void) {
    bloomCommand_t* cmd;

    if (!tr.registered) {
        return;
    }

    cmd = R_GetCommandBuffer(sizeof(*cmd));
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_BLOOM_SCENE;
}

void RE_ApplyBloom(void) {
    bloomCommand_t* cmd;

    if (!tr.registered || !r_bloomActive->integer) {
        return;
    }

    cmd = R_GetCommandBuffer(sizeof(*cmd));
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_BLOOM;
}

const void* RB_BloomScene(const void* data) {
    const bloomCommand_t* cmd = data;

    if (tess.numIndexes) {
        RB_EndSurface();
    }

    if (r_bloomActive->integer && tr.renderFbo) {
        FBO_Bind(tr.renderFbo);
    }

    return (const void*)(cmd + 1);
}

const void* RB_Bloom(const void* data) {
    const bloomCommand_t* cmd = data;
    FBO_t *sceneFbo, *bloomFbo;
    vec4_t color;

    if (tess.numIndexes) {
        RB_EndSurface();
    }

    if (!r_bloomActive->integer || !tr.renderFbo || !tr.qlBloomFbo[4]) {
        return (const void*)(cmd + 1);
    }

    sceneFbo = tr.renderFbo;
    if (tr.msaaResolveFbo) {
        FBO_FastBlit(tr.renderFbo, NULL, tr.msaaResolveFbo, NULL, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        sceneFbo = tr.msaaResolveFbo;
    }

    FBO_FastBlit(sceneFbo, NULL, tr.qlBloomFbo[1], NULL, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    VectorSet4(color, MAX(r_bloomBrightThreshold->value, 0.0f), 0.0f, 0.0f, 0.0f);
    FBO_Blit(tr.qlBloomFbo[1], NULL, NULL, tr.qlBloomFbo[2], NULL, &tr.qlBrightPassShader, color, 0);
    FBO_Blit(tr.qlBloomFbo[2], NULL, NULL, tr.qlBloomFbo[3], NULL, &tr.qlBlurShader[1], NULL, 0);
    FBO_Blit(tr.qlBloomFbo[3], NULL, NULL, tr.qlBloomFbo[4], NULL, &tr.qlBlurShader[0], NULL, 0);
    bloomFbo = tr.qlBloomFbo[4];

    if (tr.qlBloomFbo[7]) {
        FBO_FastBlit(tr.qlBloomFbo[4], NULL, tr.qlBloomFbo[5], NULL, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        FBO_Blit(tr.qlBloomFbo[5], NULL, NULL, tr.qlBloomFbo[6], NULL, &tr.qlBlurShader[1], NULL, 0);
        FBO_Blit(tr.qlBloomFbo[6], NULL, NULL, tr.qlBloomFbo[7], NULL, &tr.qlBlurShader[0], NULL, 0);
        bloomFbo = tr.qlBloomFbo[7];
    }

    color[0] = MAX(r_bloomSaturation->value, 0.0f);
    color[1] = MAX(r_bloomSceneSaturation->value, 0.0f);
    color[2] = MAX(r_bloomIntensity->value, 0.0f);
    color[3] = MAX(r_bloomSceneIntensity->value, 0.0f);

    if (color[1] == 1.0f && color[3] == 1.0f) {
        FBO_Blit(bloomFbo, NULL, NULL, tr.renderFbo, NULL, &tr.qlCombineShader[1], color, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR);
    } else {
        GL_BindToTMU(bloomFbo->colorImage[0], TB_LEVELSMAP);
        FBO_Blit(sceneFbo, NULL, NULL, tr.screenScratchFbo, NULL, &tr.qlCombineShader[0], color, 0);
        FBO_FastBlit(tr.screenScratchFbo, NULL, tr.renderFbo, NULL, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    backEnd.projection2D = qfalse;

    return (const void*)(cmd + 1);
}

qboolean RB_ColorCorrect(FBO_t* dst) {
    FBO_t* srcFbo;
    vec4_t color;

    if (!r_colorCorrectActive->integer || !tr.renderFbo) {
        return qfalse;
    }

    srcFbo = tr.renderFbo;
    if (tr.msaaResolveFbo) {
        FBO_FastBlit(tr.renderFbo, NULL, tr.msaaResolveFbo, NULL, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        srcFbo = tr.msaaResolveFbo;
    }

    color[0] = r_gamma->value > 0.0f ? 1.0f / r_gamma->value : 1.0f;
    color[1] = MAX(2.0f * r_overBrightBits->integer, 1.0f);
    color[2] = r_contrast->value;
    color[3] = 1.0f;

    FBO_Blit(srcFbo, NULL, NULL, dst, NULL, &tr.qlColorCorrectShader[(color[0] != 1.0f) | ((color[2] != 1.0f) << 1)], color, 0);

    backEnd.projection2D = qfalse;

    return qtrue;
}

FBO_t* RB_BindCaptureFbo(qboolean* bound) {
    FBO_t* oldFbo = glState.currentFBO;

    *bound = qfalse;

    if (!tr.screenScratchFbo || !RB_ColorCorrect(tr.screenScratchFbo)) {
        return oldFbo;
    }

    FBO_Bind(tr.screenScratchFbo);
    *bound = qtrue;

    return oldFbo;
}

qboolean R_CaptureGammaCorrect(void) {
    return !r_colorCorrectActive->integer && tr.overbrightBits > 0 && glConfig.deviceSupportsGamma;
}
