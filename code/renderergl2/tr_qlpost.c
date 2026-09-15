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
// tr_qlpost.c -- [QL] Quake Live's post effects: the colour correction and the bloom
//
// quakelive_steam.exe draws the 3D view into a frame buffer of its own (render command 11, which
// the cgame queues right before the view), puts the bloom over it and copies the result to the
// back buffer (command 10, right after the view), and when the frame ends runs the colour
// correction over the whole back buffer, HUD included (command 9, queued by RE_EndFrame). This
// renderer already draws the whole frame into tr.renderFbo, so the bloom works in place there and
// the colour correction is the frame's last copy, the one to the screen.
//
// The colour correction gives the gamma, the overbright and the contrast also in a window. The
// frame is drawn at 1 / 2^r_overBrightBits of its brightness, as for a hardware gamma ramp (see
// R_SetColorMappings and ComputeShaderColors), and the correction brings it back up.
//

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

/*
===============
R_QLPostRegister

[QL] quakelive_steam.exe 0x449000 (R_Register). The three Active cvars are read only and say which
effects the renderer could start. Nothing reads r_bloomBlurFalloff, r_bloomBlurRadius or
r_bloomBlurScale; the first and the last have a default below their range, so they start at its
minimum.
===============
*/
void R_QLPostRegister(void) {
    r_bloomActive = ri.Cvar_Get("r_bloomActive", "0", CVAR_ROM | CVAR_TEMP | CVAR_USERSAVE);
    r_bloomBlurFalloff = ri.Cvar_GetRange("r_bloomBlurFalloff", "0.0", "0.75", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomBlurRadius = ri.Cvar_GetRange("r_bloomBlurRadius", "5", "1", "12", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomBlurScale = ri.Cvar_GetRange("r_bloomBlurScale", "0.0", "1.0", "2.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomBrightThreshold = ri.Cvar_GetRange("r_bloomBrightThreshold", "0.25", "0.0", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    r_bloomIntensity = ri.Cvar_GetRange("r_bloomIntensity", "0.5", "0.0", "10.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    // retail's flags also carry 0x2000, a binary range: see R_BloomPasses
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

/*
===============
R_BloomPasses

[QL] retail registers r_bloomPasses with flag 0x2000, which makes its range binary: any value over
the minimum counts as the maximum
===============
*/
static int R_BloomPasses(void) {
    return r_bloomPasses->value > 1.0f ? 2 : 1;
}

/*
===============
R_QLPostInit

[QL] 0x450710, before the images are made: with r_enablePostProcess the renderer starts the colour
correction (0x43cfe0) and the bloom (0x4380f0) that are enabled. Both need frame buffers, and here
also the blit between them. R_QLPostCreateFBOs turns them back off when their frame buffers fail.
===============
*/
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

/*
===============
R_QLPostCreateImages

[QL] 0x437e40: the bloom works at half the scene's size, in four frame buffers, and with
r_bloomPasses 2 goes on at a quarter, in three more. They take the scene's format.
===============
*/
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

/*
===============
R_QLPostCreateFBOs

[QL] 0x437e40 and 0x43ce50: an effect whose frame buffers can't be made is turned off, with
retail's message. The scene's frame buffer is tr.renderFbo, made in FBO_Init.
===============
*/
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

        // without the colour correction the overbright goes back to the gamma ramp
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

/*
===============
RE_BeginBloomScene

[QL] refexport 30 (0x43cba0), which the cgame calls right before the 3D view: queues render command 11
===============
*/
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

/*
===============
RE_ApplyBloom

[QL] refexport 31 (0x4384d0), which the cgame calls right after the 3D view: queues render command 10
when the bloom runs
===============
*/
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

/*
=============
RB_BloomScene

[QL] render command 11 (0x437a50): the view that follows goes into the bloom's frame buffer. Here the
whole frame is already drawn into tr.renderFbo, so this only makes sure it is the one bound.
=============
*/
const void* RB_BloomScene(const void* data) {
    const bloomCommand_t* cmd = data;

    // finish any 2D drawing if needed
    if (tess.numIndexes) {
        RB_EndSurface();
    }

    if (r_bloomActive->integer && tr.renderFbo) {
        FBO_Bind(tr.renderFbo);
    }

    return (const void*)(cmd + 1);
}

/*
=============
RB_Bloom

[QL] render command 10, 0x436ec0. The scene goes to half size taking one texel of every four, with no
filtering (downsample1.fs); there, what is brighter than r_bloomBrightThreshold is kept and stretched
back to the whole range (brightpass.fs), then blurred up and down and then across (blurvertical.fs,
blurhoriz.fs: 11 texels with fixed weights). With r_bloomPasses 2 the blurred light goes on to a
quarter size, four texels averaged, and is blurred again. The last pass (combine.fs) lays the light
over the scene, each with its own saturation and intensity; retail's uniforms don't go below 0
(0x438590).
=============
*/
const void* RB_Bloom(const void* data) {
    const bloomCommand_t* cmd = data;
    FBO_t *sceneFbo, *bloomFbo;
    vec4_t color;

    // finish any 2D drawing if needed
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
        // the scene as it is: scene * (1 - bloom) + bloom is a blend, and nothing needs copying
        FBO_Blit(bloomFbo, NULL, NULL, tr.renderFbo, NULL, &tr.qlCombineShader[1], color, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR);
    } else {
        GL_BindToTMU(bloomFbo->colorImage[0], TB_LEVELSMAP);
        FBO_Blit(sceneFbo, NULL, NULL, tr.screenScratchFbo, NULL, &tr.qlCombineShader[0], color, 0);
        FBO_FastBlit(tr.screenScratchFbo, NULL, tr.renderFbo, NULL, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // the blits left their own viewport
    backEnd.projection2D = qfalse;

    return (const void*)(cmd + 1);
}

/*
=============
RB_ColorCorrect

[QL] render command 9, 0x436dc0, queued by RE_EndFrame (0x43cd10): the finished frame, HUD included,
through colorcorrect.fs into dst, the screen or a capture's frame buffer. The uniforms are
1 / r_gamma (1 when r_gamma isn't above 0), twice r_overBrightBits (at least 1) and r_contrast
(0x43cfe0, 0x43cd60). Returns qfalse when the colour correction is off.
=============
*/
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

    // the variant without the gamma or the contrast when they would change nothing
    FBO_Blit(srcFbo, NULL, NULL, dst, NULL, &tr.qlColorCorrectShader[(color[0] != 1.0f) | ((color[2] != 1.0f) << 1)], color, 0);

    backEnd.projection2D = qfalse;

    return qtrue;
}

/*
=============
RB_BindCaptureFbo

With the colour correction a frame is only finished on its way to the screen, so a screenshot, a
level shot or a video frame reads a corrected copy of it. [QL] Retail reads the back buffer, after
the correction (0x4481c0). Returns the frame buffer to put back when done, or qfalse in *bound when
nothing was changed.
=============
*/
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

/*
=============
R_CaptureGammaCorrect

[QL] 0x4481c0: a capture only goes through the gamma table when the gamma ramp was giving the
overbright; the colour correction's copy is already finished
=============
*/
qboolean R_CaptureGammaCorrect(void) {
    return !r_colorCorrectActive->integer && tr.overbrightBits > 0 && glConfig.deviceSupportsGamma;
}
