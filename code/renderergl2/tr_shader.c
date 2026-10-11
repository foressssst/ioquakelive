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

static char* s_shaderText;

static shaderStage_t stages[MAX_SHADER_STAGES];
static shader_t shader;
static texModInfo_t texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];
static int shader_realLightmapIndex;
static qboolean shader_noVLCollapse;

#define FILE_HASH_SIZE 1024
static shader_t* hashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH 2048
static char** shaderTextHashTable[MAX_SHADERTEXT_HASH];

#ifdef __GNUCC__
#warning TODO : check if long is ok here
#endif
static long generateHashValue(const char* fname, const int size) {
    int i;
    long hash;
    char letter;

    hash = 0;
    i = 0;
    while (fname[i] != '\0') {
        letter = tolower(fname[i]);
        if (letter == '.')
            break;
        if (letter == '\\')
            letter = '/';
        if (letter == PATH_SEP)
            letter = '/';
        hash += (long)(letter) * (i + 119);
        i++;
    }
    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    hash &= (size - 1);
    return hash;
}

void R_RemapShader(const char* shaderName, const char* newShaderName, const char* timeOffset) {
    char strippedName[MAX_QPATH];
    int hash;
    shader_t *sh, *sh2;
    qhandle_t h;

    sh = R_FindShaderByName(shaderName);
    if (sh == NULL || sh == tr.defaultShader) {
        h = RE_RegisterShaderLightMap(shaderName, 0);
        sh = R_GetShaderByHandle(h);
    }
    if (sh == NULL || sh == tr.defaultShader) {
        ri.Printf(PRINT_WARNING, "WARNING: R_RemapShader: shader %s not found\n", shaderName);
        return;
    }

    sh2 = R_FindShaderByName(newShaderName);
    if (sh2 == NULL || sh2 == tr.defaultShader) {
        h = RE_RegisterShaderLightMap(newShaderName, 0);
        sh2 = R_GetShaderByHandle(h);
    }

    if (sh2 == NULL || sh2 == tr.defaultShader) {
        ri.Printf(PRINT_WARNING, "WARNING: R_RemapShader: new shader %s not found\n", newShaderName);
        return;
    }

    COM_StripExtension(shaderName, strippedName, sizeof(strippedName));
    hash = generateHashValue(strippedName, FILE_HASH_SIZE);
    for (sh = hashTable[hash]; sh; sh = sh->next) {
        if (Q_stricmp(sh->name, strippedName) == 0) {
            if (sh != sh2) {
                sh->remappedShader = sh2;
            } else {
                sh->remappedShader = NULL;
            }
        }
    }
    if (timeOffset) {
        sh2->timeOffset = atof(timeOffset);
    }
}

static qboolean ParseVector(char** text, int count, float* v) {
    char* token;
    int i;

    token = COM_ParseExt(text, qfalse);
    if (strcmp(token, "(")) {
        ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name);
        return qfalse;
    }

    for (i = 0; i < count; i++) {
        token = COM_ParseExt(text, qfalse);
        if (!token[0]) {
            ri.Printf(PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name);
            return qfalse;
        }
        v[i] = atof(token);
    }

    token = COM_ParseExt(text, qfalse);
    if (strcmp(token, ")")) {
        ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name);
        return qfalse;
    }

    return qtrue;
}

static unsigned NameToAFunc(const char* funcname) {
    if (!Q_stricmp(funcname, "GT0")) {
        return GLS_ATEST_GT_0;
    } else if (!Q_stricmp(funcname, "LT128")) {
        return GLS_ATEST_LT_80;
    } else if (!Q_stricmp(funcname, "GE128")) {
        return GLS_ATEST_GE_80;
    }

    ri.Printf(PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name);
    return 0;
}

static int NameToSrcBlendMode(const char* name) {
    if (!Q_stricmp(name, "GL_ONE")) {
        return GLS_SRCBLEND_ONE;
    } else if (!Q_stricmp(name, "GL_ZERO")) {
        return GLS_SRCBLEND_ZERO;
    } else if (!Q_stricmp(name, "GL_DST_COLOR")) {
        return GLS_SRCBLEND_DST_COLOR;
    } else if (!Q_stricmp(name, "GL_ONE_MINUS_DST_COLOR")) {
        return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
    } else if (!Q_stricmp(name, "GL_SRC_ALPHA")) {
        return GLS_SRCBLEND_SRC_ALPHA;
    } else if (!Q_stricmp(name, "GL_ONE_MINUS_SRC_ALPHA")) {
        return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
    } else if (!Q_stricmp(name, "GL_DST_ALPHA")) {
        if (r_ignoreDstAlpha->integer)
            return GLS_SRCBLEND_ONE;

        return GLS_SRCBLEND_DST_ALPHA;
    } else if (!Q_stricmp(name, "GL_ONE_MINUS_DST_ALPHA")) {
        if (r_ignoreDstAlpha->integer)
            return GLS_SRCBLEND_ZERO;

        return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
    } else if (!Q_stricmp(name, "GL_SRC_ALPHA_SATURATE")) {
        return GLS_SRCBLEND_ALPHA_SATURATE;
    }

    ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name);
    return GLS_SRCBLEND_ONE;
}

static int NameToDstBlendMode(const char* name) {
    if (!Q_stricmp(name, "GL_ONE")) {
        return GLS_DSTBLEND_ONE;
    } else if (!Q_stricmp(name, "GL_ZERO")) {
        return GLS_DSTBLEND_ZERO;
    } else if (!Q_stricmp(name, "GL_SRC_ALPHA")) {
        return GLS_DSTBLEND_SRC_ALPHA;
    } else if (!Q_stricmp(name, "GL_ONE_MINUS_SRC_ALPHA")) {
        return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    } else if (!Q_stricmp(name, "GL_DST_ALPHA")) {
        if (r_ignoreDstAlpha->integer)
            return GLS_DSTBLEND_ONE;

        return GLS_DSTBLEND_DST_ALPHA;
    } else if (!Q_stricmp(name, "GL_ONE_MINUS_DST_ALPHA")) {
        if (r_ignoreDstAlpha->integer)
            return GLS_DSTBLEND_ZERO;

        return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
    } else if (!Q_stricmp(name, "GL_SRC_COLOR")) {
        return GLS_DSTBLEND_SRC_COLOR;
    } else if (!Q_stricmp(name, "GL_ONE_MINUS_SRC_COLOR")) {
        return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
    }

    ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name);
    return GLS_DSTBLEND_ONE;
}

static genFunc_t NameToGenFunc(const char* funcname) {
    if (!Q_stricmp(funcname, "sin")) {
        return GF_SIN;
    } else if (!Q_stricmp(funcname, "square")) {
        return GF_SQUARE;
    } else if (!Q_stricmp(funcname, "triangle")) {
        return GF_TRIANGLE;
    } else if (!Q_stricmp(funcname, "sawtooth")) {
        return GF_SAWTOOTH;
    } else if (!Q_stricmp(funcname, "inversesawtooth")) {
        return GF_INVERSE_SAWTOOTH;
    } else if (!Q_stricmp(funcname, "noise")) {
        return GF_NOISE;
    }

    ri.Printf(PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name);
    return GF_SIN;
}

static void ParseWaveForm(char** text, waveForm_t* wave) {
    char* token;

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
        return;
    }
    wave->func = NameToGenFunc(token);

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
        return;
    }
    wave->base = atof(token);

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
        return;
    }
    wave->amplitude = atof(token);

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
        return;
    }
    wave->phase = atof(token);

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
        return;
    }
    wave->frequency = atof(token);
}

static void ParseTexMod(char* _text, shaderStage_t* stage) {
    const char* token;
    char** text = &_text;
    texModInfo_t* tmi;

    if (stage->bundle[0].numTexMods == TR_MAX_TEXMODS) {
        ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'", shader.name);
        return;
    }

    tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
    stage->bundle[0].numTexMods++;

    token = COM_ParseExt(text, qfalse);

    if (!Q_stricmp(token, "turb")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.base = atof(token);
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.amplitude = atof(token);
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.phase = atof(token);
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.frequency = atof(token);

        tmi->type = TMOD_TURBULENT;
    }
    else if (!Q_stricmp(token, "scale")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->scale[0] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->scale[1] = atof(token);
        tmi->type = TMOD_SCALE;
    }
    else if (!Q_stricmp(token, "scroll")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->scroll[0] = atof(token);
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->scroll[1] = atof(token);
        tmi->type = TMOD_SCROLL;
    }
    else if (!Q_stricmp(token, "stretch")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.func = NameToGenFunc(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.base = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.amplitude = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.phase = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->wave.frequency = atof(token);

        tmi->type = TMOD_STRETCH;
    }
    else if (!Q_stricmp(token, "transform")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->matrix[0][0] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->matrix[0][1] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->matrix[1][0] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->matrix[1][1] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->translate[0] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->translate[1] = atof(token);

        tmi->type = TMOD_TRANSFORM;
    }
    else if (!Q_stricmp(token, "rotate")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name);
            return;
        }
        tmi->rotateSpeed = atof(token);
        tmi->type = TMOD_ROTATE;
    }
    else if (!Q_stricmp(token, "entityTranslate")) {
        tmi->type = TMOD_ENTITY_TRANSLATE;
    } else {
        ri.Printf(PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name);
    }
}

static qboolean ParseStage(shaderStage_t* stage, char** text) {
    char* token;
    int depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
    qboolean depthMaskExplicit = qfalse;

    stage->active = qtrue;

    while (1) {
        token = COM_ParseExt(text, qtrue);
        if (!token[0]) {
            ri.Printf(PRINT_WARNING, "WARNING: no matching '}' found\n");
            return qfalse;
        }

        if (token[0] == '}') {
            break;
        }
        else if (!Q_stricmp(token, "map")) {
            token = COM_ParseExt(text, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }

            if (!Q_stricmp(token, "$whiteimage")) {
                stage->bundle[0].image[0] = tr.whiteImage;
                continue;
            } else if (!Q_stricmp(token, "$lightmap")) {
                stage->bundle[0].isLightmap = qtrue;
                if (shader.lightmapIndex < 0 || !tr.lightmaps) {
                    stage->bundle[0].image[0] = tr.whiteImage;
                } else {
                    stage->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
                }
                continue;
            } else if (!Q_stricmp(token, "$deluxemap")) {
                if (!tr.worldDeluxeMapping) {
                    ri.Printf(PRINT_WARNING, "WARNING: shader '%s' wants a deluxe map in a map compiled without them\n", shader.name);
                    return qfalse;
                }

                stage->bundle[0].isLightmap = qtrue;
                if (shader.lightmapIndex < 0) {
                    stage->bundle[0].image[0] = tr.whiteImage;
                } else {
                    stage->bundle[0].image[0] = tr.deluxemaps[shader.lightmapIndex];
                }
                continue;
            } else {
                imgType_t type = IMGTYPE_COLORALPHA;
                imgFlags_t flags = IMGFLAG_NONE;

                if (!shader.noMipMaps)
                    flags |= IMGFLAG_MIPMAP;

                if (!shader.noPicMip)
                    flags |= IMGFLAG_PICMIP;

                if (stage->type == ST_NORMALMAP || stage->type == ST_NORMALPARALLAXMAP) {
                    type = IMGTYPE_NORMAL;
                    flags |= IMGFLAG_NOLIGHTSCALE;

                    if (stage->type == ST_NORMALPARALLAXMAP)
                        type = IMGTYPE_NORMALHEIGHT;
                } else {
                    if (r_genNormalMaps->integer)
                        flags |= IMGFLAG_GENNORMALMAP;
                }

                stage->bundle[0].image[0] = R_FindImageFile(token, type, flags);

                if (!stage->bundle[0].image[0]) {
                    ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
                    return qfalse;
                }
            }
        }
        else if (!Q_stricmp(token, "clampmap")) {
            imgType_t type = IMGTYPE_COLORALPHA;
            imgFlags_t flags = IMGFLAG_CLAMPTOEDGE;

            token = COM_ParseExt(text, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }

            if (!shader.noMipMaps)
                flags |= IMGFLAG_MIPMAP;

            if (!shader.noPicMip)
                flags |= IMGFLAG_PICMIP;

            if (stage->type == ST_NORMALMAP || stage->type == ST_NORMALPARALLAXMAP) {
                type = IMGTYPE_NORMAL;
                flags |= IMGFLAG_NOLIGHTSCALE;

                if (stage->type == ST_NORMALPARALLAXMAP)
                    type = IMGTYPE_NORMALHEIGHT;
            } else {
                if (r_genNormalMaps->integer)
                    flags |= IMGFLAG_GENNORMALMAP;
            }

            stage->bundle[0].image[0] = R_FindImageFile(token, type, flags);
            if (!stage->bundle[0].image[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
                return qfalse;
            }
        }
        else if (!Q_stricmp(token, "animMap")) {
            token = COM_ParseExt(text, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }
            stage->bundle[0].imageAnimationSpeed = atof(token);

            while (1) {
                int num;

                token = COM_ParseExt(text, qfalse);
                if (!token[0]) {
                    break;
                }
                num = stage->bundle[0].numImageAnimations;
                if (num < MAX_IMAGE_ANIMATIONS) {
                    imgFlags_t flags = IMGFLAG_NONE;

                    if (!shader.noMipMaps)
                        flags |= IMGFLAG_MIPMAP;

                    if (!shader.noPicMip)
                        flags |= IMGFLAG_PICMIP;

                    stage->bundle[0].image[num] = R_FindImageFile(token, IMGTYPE_COLORALPHA, flags);
                    if (!stage->bundle[0].image[num]) {
                        ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
                        return qfalse;
                    }
                    stage->bundle[0].numImageAnimations++;
                }
            }
        } else if (!Q_stricmp(token, "videoMap")) {
            token = COM_ParseExt(text, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }
            stage->bundle[0].videoMapHandle = ri.CIN_PlayCinematic(token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader));
            if (stage->bundle[0].videoMapHandle != -1) {
                stage->bundle[0].isVideoMap = qtrue;
                stage->bundle[0].image[0] = tr.scratchImage[stage->bundle[0].videoMapHandle];
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: could not load '%s' for 'videoMap' keyword in shader '%s'\n", token, shader.name);
            }
        }
        else if (!Q_stricmp(token, "alphaFunc")) {
            token = COM_ParseExt(text, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }

            atestBits = NameToAFunc(token);
        }
        else if (!Q_stricmp(token, "depthfunc")) {
            token = COM_ParseExt(text, qfalse);

            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }

            if (!Q_stricmp(token, "lequal")) {
                depthFuncBits = 0;
            } else if (!Q_stricmp(token, "equal")) {
                depthFuncBits = GLS_DEPTHFUNC_EQUAL;
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name);
                continue;
            }
        }
        else if (!Q_stricmp(token, "detail")) {
            stage->isDetail = qtrue;
        }
        else if (!Q_stricmp(token, "blendfunc")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name);
                continue;
            }
            if (!Q_stricmp(token, "add")) {
                blendSrcBits = GLS_SRCBLEND_ONE;
                blendDstBits = GLS_DSTBLEND_ONE;
            } else if (!Q_stricmp(token, "filter")) {
                blendSrcBits = GLS_SRCBLEND_DST_COLOR;
                blendDstBits = GLS_DSTBLEND_ZERO;
            } else if (!Q_stricmp(token, "blend")) {
                blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
                blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
            } else {
                blendSrcBits = NameToSrcBlendMode(token);

                token = COM_ParseExt(text, qfalse);
                if (token[0] == 0) {
                    ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name);
                    continue;
                }
                blendDstBits = NameToDstBlendMode(token);
            }

            if (!depthMaskExplicit) {
                depthMaskBits = 0;
            }
        }
        else if (!Q_stricmp(token, "stage")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameters for stage in shader '%s'\n", shader.name);
                continue;
            }

            if (!Q_stricmp(token, "diffuseMap")) {
                stage->type = ST_DIFFUSEMAP;
            } else if (!Q_stricmp(token, "normalMap") || !Q_stricmp(token, "bumpMap")) {
                stage->type = ST_NORMALMAP;
                VectorSet4(stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
            } else if (!Q_stricmp(token, "normalParallaxMap") || !Q_stricmp(token, "bumpParallaxMap")) {
                if (r_parallaxMapping->integer)
                    stage->type = ST_NORMALPARALLAXMAP;
                else
                    stage->type = ST_NORMALMAP;
                VectorSet4(stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
            } else if (!Q_stricmp(token, "specularMap")) {
                stage->type = ST_SPECULARMAP;
                VectorSet4(stage->specularScale, 1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: unknown stage parameter '%s' in shader '%s'\n", token, shader.name);
                continue;
            }
        }
        else if (!Q_stricmp(token, "specularreflectance")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specular reflectance in shader '%s'\n", shader.name);
                continue;
            }

            if (r_pbr->integer) {
                stage->specularScale[1] = (atof(token) < 0.5f) ? 0.0f : 1.0f;
            } else {
                stage->specularScale[0] =
                    stage->specularScale[1] =
                        stage->specularScale[2] = atof(token);
            }
        }
        else if (!Q_stricmp(token, "specularexponent")) {
            float exponent;

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specular exponent in shader '%s'\n", shader.name);
                continue;
            }

            exponent = atof(token);

            if (r_pbr->integer)
                stage->specularScale[0] = 1.0f - powf(2.0f / (exponent + 2.0), 0.25);
            else {
                exponent = CLAMP(exponent, 0.0f, 8190.0f);
                stage->specularScale[3] = (log2f(exponent + 2.0f) - 1.0f) / 12.0f;
            }
        }
        else if (!Q_stricmp(token, "gloss")) {
            float gloss;

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for gloss in shader '%s'\n", shader.name);
                continue;
            }

            gloss = atof(token);

            if (r_pbr->integer)
                stage->specularScale[0] = 1.0f - exp2f(-3.0f * gloss);
            else
                stage->specularScale[3] = gloss;
        }
        else if (!Q_stricmp(token, "roughness")) {
            float roughness;

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for roughness in shader '%s'\n", shader.name);
                continue;
            }

            roughness = atof(token);

            if (r_pbr->integer)
                stage->specularScale[0] = 1.0 - roughness;
            else {
                if (roughness >= 0.125)
                    stage->specularScale[3] = log2f(1.0f / roughness) / 3.0f;
                else
                    stage->specularScale[3] = 1.0f;
            }
        }
        else if (!Q_stricmp(token, "parallaxdepth")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for parallaxDepth in shader '%s'\n", shader.name);
                continue;
            }

            stage->normalScale[3] = atof(token);
        }
        else if (!Q_stricmp(token, "normalscale")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for normalScale in shader '%s'\n", shader.name);
                continue;
            }

            stage->normalScale[0] = atof(token);

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                stage->normalScale[1] = stage->normalScale[0];
                continue;
            }

            stage->normalScale[1] = atof(token);

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                continue;
            }

            stage->normalScale[3] = atof(token);
        }
        else if (!Q_stricmp(token, "specularscale")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specularScale in shader '%s'\n", shader.name);
                continue;
            }

            stage->specularScale[0] = atof(token);

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for specularScale in shader '%s'\n", shader.name);
                continue;
            }

            stage->specularScale[1] = atof(token);

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                if (r_pbr->integer) {
                    float smoothness = stage->specularScale[1];
                    stage->specularScale[1] = (stage->specularScale[0] < 0.5f) ? 0.0f : 1.0f;
                    stage->specularScale[0] = smoothness;
                } else {
                    stage->specularScale[3] = stage->specularScale[1];
                    stage->specularScale[1] =
                        stage->specularScale[2] = stage->specularScale[0];
                }
                continue;
            }

            stage->specularScale[2] = atof(token);

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                continue;
            }

            stage->specularScale[3] = atof(token);

        }
        else if (!Q_stricmp(token, "rgbGen")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name);
                continue;
            }

            if (!Q_stricmp(token, "wave")) {
                ParseWaveForm(text, &stage->rgbWave);
                stage->rgbGen = CGEN_WAVEFORM;
            } else if (!Q_stricmp(token, "const")) {
                vec3_t color;

                VectorClear(color);

                ParseVector(text, 3, color);
                stage->constantColor[0] = 255 * color[0];
                stage->constantColor[1] = 255 * color[1];
                stage->constantColor[2] = 255 * color[2];

                stage->rgbGen = CGEN_CONST;
            } else if (!Q_stricmp(token, "identity")) {
                stage->rgbGen = CGEN_IDENTITY;
            } else if (!Q_stricmp(token, "identityLighting")) {
                stage->rgbGen = CGEN_IDENTITY_LIGHTING;
            } else if (!Q_stricmp(token, "entity")) {
                stage->rgbGen = CGEN_ENTITY;
            } else if (!Q_stricmp(token, "oneMinusEntity")) {
                stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
            } else if (!Q_stricmp(token, "vertex")) {
                stage->rgbGen = CGEN_VERTEX;
                if (stage->alphaGen == 0) {
                    stage->alphaGen = AGEN_VERTEX;
                }
            } else if (!Q_stricmp(token, "exactVertex")) {
                stage->rgbGen = CGEN_EXACT_VERTEX;
            } else if (!Q_stricmp(token, "vertexLit")) {
                stage->rgbGen = CGEN_VERTEX_LIT;
                if (stage->alphaGen == 0) {
                    stage->alphaGen = AGEN_VERTEX;
                }
            } else if (!Q_stricmp(token, "exactVertexLit")) {
                stage->rgbGen = CGEN_EXACT_VERTEX_LIT;
            } else if (!Q_stricmp(token, "lightingDiffuse")) {
                stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
            } else if (!Q_stricmp(token, "oneMinusVertex")) {
                stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name);
                continue;
            }
        }
        else if (!Q_stricmp(token, "alphaGen")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name);
                continue;
            }

            if (!Q_stricmp(token, "wave")) {
                ParseWaveForm(text, &stage->alphaWave);
                stage->alphaGen = AGEN_WAVEFORM;
            } else if (!Q_stricmp(token, "const")) {
                token = COM_ParseExt(text, qfalse);
                stage->constantColor[3] = 255 * atof(token);
                stage->alphaGen = AGEN_CONST;
            } else if (!Q_stricmp(token, "identity")) {
                stage->alphaGen = AGEN_IDENTITY;
            } else if (!Q_stricmp(token, "entity")) {
                stage->alphaGen = AGEN_ENTITY;
            } else if (!Q_stricmp(token, "oneMinusEntity")) {
                stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
            } else if (!Q_stricmp(token, "vertex")) {
                stage->alphaGen = AGEN_VERTEX;
            } else if (!Q_stricmp(token, "lightingSpecular")) {
                stage->alphaGen = AGEN_LIGHTING_SPECULAR;
            } else if (!Q_stricmp(token, "oneMinusVertex")) {
                stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
            } else if (!Q_stricmp(token, "portal")) {
                stage->alphaGen = AGEN_PORTAL;
                token = COM_ParseExt(text, qfalse);
                if (token[0] == 0) {
                    shader.portalRange = 256;
                    ri.Printf(PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name);
                } else {
                    shader.portalRange = atof(token);
                }
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name);
                continue;
            }
        }
        else if (!Q_stricmp(token, "texgen") || !Q_stricmp(token, "tcGen")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name);
                continue;
            }

            if (!Q_stricmp(token, "environment")) {
                stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
            } else if (!Q_stricmp(token, "lightmap")) {
                stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
            } else if (!Q_stricmp(token, "texture") || !Q_stricmp(token, "base")) {
                stage->bundle[0].tcGen = TCGEN_TEXTURE;
            } else if (!Q_stricmp(token, "vector")) {
                ParseVector(text, 3, stage->bundle[0].tcGenVectors[0]);
                ParseVector(text, 3, stage->bundle[0].tcGenVectors[1]);

                stage->bundle[0].tcGen = TCGEN_VECTOR;
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name);
            }
        }
        else if (!Q_stricmp(token, "tcMod")) {
            char buffer[1024] = "";

            while (1) {
                token = COM_ParseExt(text, qfalse);
                if (token[0] == 0)
                    break;
                Q_strcat(buffer, sizeof(buffer), token);
                Q_strcat(buffer, sizeof(buffer), " ");
            }

            ParseTexMod(buffer, stage);

            continue;
        }
        else if (!Q_stricmp(token, "depthwrite")) {
            depthMaskBits = GLS_DEPTHMASK_TRUE;
            depthMaskExplicit = qtrue;

            continue;
        }
        else if (!Q_stricmp(token, "depthTest")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parm for depthTest in shader '%s'\n",
                          shader.name);
                continue;
            }
            if (!Q_stricmp(token, "disable")) {
                stage->stateBits |= GLS_DEPTHTEST_DISABLE;
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token,
                          shader.name);
            }
            continue;
        } else {
            ri.Printf(PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name);
            return qfalse;
        }
    }

    if (stage->rgbGen == CGEN_BAD) {
        if (blendSrcBits == 0 ||
            blendSrcBits == GLS_SRCBLEND_ONE ||
            blendSrcBits == GLS_SRCBLEND_SRC_ALPHA) {
            stage->rgbGen = CGEN_IDENTITY_LIGHTING;
        } else {
            stage->rgbGen = CGEN_IDENTITY;
        }
    }

    if ((blendSrcBits == GLS_SRCBLEND_ONE) &&
        (blendDstBits == GLS_DSTBLEND_ZERO)) {
        blendDstBits = blendSrcBits = 0;
        depthMaskBits = GLS_DEPTHMASK_TRUE;
    }

    if (stage->alphaGen == AGEN_IDENTITY) {
        if (stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_LIGHTING_DIFFUSE) {
            stage->alphaGen = AGEN_SKIP;
        }
    }

    stage->stateBits = depthMaskBits |
                       blendSrcBits | blendDstBits |
                       atestBits |
                       depthFuncBits;

    return qtrue;
}

static void ParseDeform(char** text) {
    char* token;
    deformStage_t* ds;

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name);
        return;
    }

    if (shader.numDeforms == MAX_SHADER_DEFORMS) {
        ri.Printf(PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name);
        return;
    }

    ds = &shader.deforms[shader.numDeforms];
    shader.numDeforms++;

    if (!Q_stricmp(token, "projectionShadow")) {
        ds->deformation = DEFORM_PROJECTION_SHADOW;
        return;
    }

    if (!Q_stricmp(token, "autosprite")) {
        ds->deformation = DEFORM_AUTOSPRITE;
        return;
    }

    if (!Q_stricmp(token, "autosprite2")) {
        ds->deformation = DEFORM_AUTOSPRITE2;
        return;
    }

    if (!Q_stricmpn(token, "text", 4)) {
        int n;

        n = token[4] - '0';
        if (n < 0 || n > 7) {
            n = 0;
        }
        ds->deformation = DEFORM_TEXT0 + n;
        return;
    }

    if (!Q_stricmp(token, "bulge")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
            return;
        }
        ds->bulgeWidth = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
            return;
        }
        ds->bulgeHeight = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
            return;
        }
        ds->bulgeSpeed = atof(token);

        ds->deformation = DEFORM_BULGE;
        return;
    }

    if (!Q_stricmp(token, "wave")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
            return;
        }

        if (atof(token) != 0) {
            ds->deformationSpread = 1.0f / atof(token);
        } else {
            ds->deformationSpread = 100.0f;
            ri.Printf(PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name);
        }

        ParseWaveForm(text, &ds->deformationWave);
        ds->deformation = DEFORM_WAVE;
        return;
    }

    if (!Q_stricmp(token, "normal")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
            return;
        }
        ds->deformationWave.amplitude = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
            return;
        }
        ds->deformationWave.frequency = atof(token);

        ds->deformation = DEFORM_NORMALS;
        return;
    }

    if (!Q_stricmp(token, "move")) {
        int i;

        for (i = 0; i < 3; i++) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
                return;
            }
            ds->moveVector[i] = atof(token);
        }

        ParseWaveForm(text, &ds->deformationWave);
        ds->deformation = DEFORM_MOVE;
        return;
    }

    ri.Printf(PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name);
}

static void ParseSkyParms(char** text) {
    char* token;
    static char* suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
    char pathname[MAX_QPATH];
    int i;
    imgFlags_t imgFlags = IMGFLAG_MIPMAP | IMGFLAG_PICMIP;

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
        return;
    }
    if (strcmp(token, "-")) {
        for (i = 0; i < 6; i++) {
            Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
            shader.sky.outerbox[i] = R_FindImageFile((char*)pathname, IMGTYPE_COLORALPHA, imgFlags | IMGFLAG_CLAMPTOEDGE);

            if (!shader.sky.outerbox[i]) {
                shader.sky.outerbox[i] = tr.defaultImage;
            }
        }
    }

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
        return;
    }
    shader.sky.cloudHeight = atof(token);
    if (!shader.sky.cloudHeight) {
        shader.sky.cloudHeight = 512;
    }
    R_InitSkyTexCoords(shader.sky.cloudHeight);

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
        return;
    }
    if (strcmp(token, "-")) {
        for (i = 0; i < 6; i++) {
            Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
            shader.sky.innerbox[i] = R_FindImageFile((char*)pathname, IMGTYPE_COLORALPHA, imgFlags);
            if (!shader.sky.innerbox[i]) {
                shader.sky.innerbox[i] = tr.defaultImage;
            }
        }
    }

    shader.isSky = qtrue;
}

void ParseSort(char** text) {
    char* token;

    token = COM_ParseExt(text, qfalse);
    if (token[0] == 0) {
        ri.Printf(PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name);
        return;
    }

    if (!Q_stricmp(token, "portal")) {
        shader.sort = SS_PORTAL;
    } else if (!Q_stricmp(token, "sky")) {
        shader.sort = SS_ENVIRONMENT;
    } else if (!Q_stricmp(token, "opaque")) {
        shader.sort = SS_OPAQUE;
    } else if (!Q_stricmp(token, "decal")) {
        shader.sort = SS_DECAL;
    } else if (!Q_stricmp(token, "seeThrough")) {
        shader.sort = SS_SEE_THROUGH;
    } else if (!Q_stricmp(token, "banner")) {
        shader.sort = SS_BANNER;
    } else if (!Q_stricmp(token, "additive")) {
        shader.sort = SS_BLEND1;
    } else if (!Q_stricmp(token, "nearest")) {
        shader.sort = SS_NEAREST;
    } else if (!Q_stricmp(token, "underwater")) {
        shader.sort = SS_UNDERWATER;
    } else {
        shader.sort = atof(token);
    }
}

typedef struct {
    char* name;
    int clearSolid, surfaceFlags, contents;
} infoParm_t;

infoParm_t infoParms[] = {
    {"water", 1, 0, CONTENTS_WATER},
    {"slime", 1, 0, CONTENTS_SLIME},
    {"lava", 1, 0, CONTENTS_LAVA},
    {"playerclip", 1, 0, CONTENTS_PLAYERCLIP},
    {"monsterclip", 1, 0, CONTENTS_MONSTERCLIP},
    {"nodrop", 1, 0, CONTENTS_NODROP},
    {"nonsolid", 1, SURF_NONSOLID, 0},
    {"nobotclip", 1, 0, CONTENTS_BOTCLIP},

    {"origin", 1, 0, CONTENTS_ORIGIN},
    {"trans", 0, 0, CONTENTS_TRANSLUCENT},
    {"detail", 0, 0, CONTENTS_DETAIL},
    {"structural", 0, 0, CONTENTS_STRUCTURAL},
    {"areaportal", 1, 0, CONTENTS_AREAPORTAL},
    {"clusterportal", 1, 0, CONTENTS_CLUSTERPORTAL},
    {"donotenter", 1, 0, CONTENTS_DONOTENTER},

    {"fog", 1, 0, CONTENTS_FOG},
    {"sky", 0, SURF_SKY, 0},
    {"lightfilter", 0, SURF_LIGHTFILTER, 0},
    {"alphashadow", 0, SURF_ALPHASHADOW, 0},
    {"hint", 0, SURF_HINT, 0},

    {"slick", 0, SURF_SLICK, 0},
    {"noimpact", 0, SURF_NOIMPACT, 0},
    {"nomarks", 0, SURF_NOMARKS, 0},
    {"ladder", 0, SURF_LADDER, 0},
    {"nodamage", 0, SURF_NODAMAGE, 0},
    {"metalsteps", 0, SURF_METALSTEPS, 0},
    {"flesh", 0, SURF_FLESH, 0},
    {"nosteps", 0, SURF_NOSTEPS, 0},

    {"nodraw", 0, SURF_NODRAW, 0},
    {"pointlight", 0, SURF_POINTLIGHT, 0},
    {"nolightmap", 0, SURF_NOLIGHTMAP, 0},
    {"nodlight", 0, SURF_NODLIGHT, 0},
    {"dust", 0, SURF_DUST, 0},
    {"snowsteps", 0, SURF_SNOWSTEPS, 0},
    {"woodsteps", 0, SURF_WOODSTEPS, 0}
};

static void ParseSurfaceParm(char** text) {
    char* token;
    int numInfoParms = ARRAY_LEN(infoParms);
    int i;

    token = COM_ParseExt(text, qfalse);
    for (i = 0; i < numInfoParms; i++) {
        if (!Q_stricmp(token, infoParms[i].name)) {
            shader.surfaceFlags |= infoParms[i].surfaceFlags;
            shader.contentFlags |= infoParms[i].contents;
#if 0
			if (infoParms[i].clearSolid) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
            break;
        }
    }
}

static qboolean ParseShader(char** text) {
    qboolean noDefault = qfalse;
    char* token;
    int s;

    s = 0;

    token = COM_ParseExt(text, qtrue);
    if (token[0] != '{') {
        ri.Printf(PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name);
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(text, qtrue);
        if (!token[0]) {
            ri.Printf(PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name);
            return qfalse;
        }

        if (token[0] == '}') {
            break;
        }
        else if (token[0] == '{') {
            if (s >= MAX_SHADER_STAGES) {
                ri.Printf(PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", shader.name, MAX_SHADER_STAGES);
                return qfalse;
            }

            if (!ParseStage(&stages[s], text)) {
                return qfalse;
            }
            stages[s].active = qtrue;
            s++;

            continue;
        }
        else if (!Q_stricmpn(token, "qer", 3)) {
            SkipRestOfLine(text);
            continue;
        }
        else if (!Q_stricmp(token, "q3map_sun") || !Q_stricmp(token, "q3map_sunExt") || !Q_stricmp(token, "q3gl2_sun")) {
            float a, b;
            qboolean isGL2Sun = qfalse;

            if (!Q_stricmp(token, "q3gl2_sun") && r_sunShadows->integer) {
                isGL2Sun = qtrue;
                tr.sunShadows = qtrue;
            }

            token = COM_ParseExt(text, qfalse);
            tr.sunLight[0] = atof(token);
            token = COM_ParseExt(text, qfalse);
            tr.sunLight[1] = atof(token);
            token = COM_ParseExt(text, qfalse);
            tr.sunLight[2] = atof(token);

            VectorNormalize(tr.sunLight);

            token = COM_ParseExt(text, qfalse);
            a = atof(token);
            VectorScale(tr.sunLight, a, tr.sunLight);

            token = COM_ParseExt(text, qfalse);
            a = atof(token);
            a = a / 180 * M_PI;

            token = COM_ParseExt(text, qfalse);
            b = atof(token);
            b = b / 180 * M_PI;

            tr.sunDirection[0] = cos(a) * cos(b);
            tr.sunDirection[1] = sin(a) * cos(b);
            tr.sunDirection[2] = sin(b);

            if (isGL2Sun) {
                token = COM_ParseExt(text, qfalse);
                tr.sunShadowScale = atof(token);

                if (token[0]) {
                    token = COM_ParseExt(text, qfalse);
                    if (token[0]) {
                        tr.sunShadowScale = atof(token);
                    }
                }
            }

            if (token[0]) {
                SkipRestOfLine(text);
            }
            continue;
        }
        else if (!Q_stricmp(token, "q3gl2_tonemap")) {
            token = COM_ParseExt(text, qfalse);
            tr.toneMinAvgMaxLevel[0] = atof(token);
            token = COM_ParseExt(text, qfalse);
            tr.toneMinAvgMaxLevel[1] = atof(token);
            token = COM_ParseExt(text, qfalse);
            tr.toneMinAvgMaxLevel[2] = atof(token);

            token = COM_ParseExt(text, qfalse);
            tr.autoExposureMinMax[0] = atof(token);
            token = COM_ParseExt(text, qfalse);
            tr.autoExposureMinMax[1] = atof(token);

            SkipRestOfLine(text);
            continue;
        } else if (!Q_stricmp(token, "deformVertexes")) {
            ParseDeform(text);
            continue;
        } else if (!Q_stricmp(token, "tesssize")) {
            SkipRestOfLine(text);
            continue;
        } else if (!Q_stricmp(token, "clampTime")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0]) {
                shader.clampTime = atof(token);
            }
        }
        else if (!Q_stricmpn(token, "q3map", 5)) {
            SkipRestOfLine(text);
            continue;
        }
        else if (!Q_stricmp(token, "surfaceParm")) {
            ParseSurfaceParm(text);
            continue;
        }
        else if (!Q_stricmp(token, "nomipmaps")) {
            shader.noMipMaps = qtrue;
            shader.noPicMip = qtrue;
            continue;
        }
        else if (!Q_stricmp(token, "nopicmip")) {
            if (!r_picmipExplosions->integer ||
                (Q_stricmp(shader.name, "rocketExplosion") && Q_stricmp(shader.name, "grenadeExplosion"))) {
                shader.noPicMip = qtrue;
            }
            continue;
        }
        else if (!Q_stricmp(token, "polygonOffset")) {
            shader.polygonOffset = qtrue;
            continue;
        }
        else if (!Q_stricmp(token, "entityMergable")) {
            shader.entityMergable = qtrue;
            continue;
        }

        else if (!Q_stricmp(token, "fogParms")) {
            if (!ParseVector(text, 3, shader.fogParms.color)) {
                return qfalse;
            }

            if (r_greyscale->integer) {
                float luminance;

                luminance = LUMA(shader.fogParms.color[0], shader.fogParms.color[1], shader.fogParms.color[2]);
                VectorSet(shader.fogParms.color, luminance, luminance, luminance);
            } else if (r_greyscale->value) {
                float luminance;

                luminance = LUMA(shader.fogParms.color[0], shader.fogParms.color[1], shader.fogParms.color[2]);
                shader.fogParms.color[0] = LERP(shader.fogParms.color[0], luminance, r_greyscale->value);
                shader.fogParms.color[1] = LERP(shader.fogParms.color[1], luminance, r_greyscale->value);
                shader.fogParms.color[2] = LERP(shader.fogParms.color[2], luminance, r_greyscale->value);
            }

            token = COM_ParseExt(text, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name);
                continue;
            }
            shader.fogParms.depthForOpaque = atof(token);

            SkipRestOfLine(text);
            continue;
        } else if (!Q_stricmp(token, "portal")) {
            shader.sort = SS_PORTAL;
            shader.isPortal = qtrue;
            continue;
        } else if (!Q_stricmp(token, "skyparms")) {
            ParseSkyParms(text);
            continue;
        } else if (!Q_stricmp(token, "light")) {
            COM_ParseExt(text, qfalse);
            continue;
        } else if (!Q_stricmp(token, "cull")) {
            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0) {
                ri.Printf(PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name);
                continue;
            }

            if (!Q_stricmp(token, "none") || !Q_stricmp(token, "twosided") || !Q_stricmp(token, "disable")) {
                shader.cullType = CT_TWO_SIDED;
            } else if (!Q_stricmp(token, "back") || !Q_stricmp(token, "backside") || !Q_stricmp(token, "backsided")) {
                shader.cullType = CT_BACK_SIDED;
            } else {
                ri.Printf(PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name);
            }
            continue;
        } else if (!Q_stricmp(token, "sort")) {
            ParseSort(text);
            continue;
        } else if (!Q_stricmp(token, "nodefault")) {
            noDefault = qtrue;
            continue;
        } else if (!Q_stricmp(token, "novlcollapse")) {
            shader_noVLCollapse = qtrue;
            continue;
        } else {
            ri.Printf(PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name);
            return qfalse;
        }
    }

    if (s == 0 && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG) && !noDefault) {
        return qfalse;
    }

    shader.explicitlyDefined = qtrue;

    return qtrue;
}

static void ComputeStageIteratorFunc(void) {
    shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;

    if (shader.isSky) {
        shader.optimalStageIteratorFunc = RB_StageIteratorSky;
        return;
    }
}

static void ComputeVertexAttribs(void) {
    int i, stage;

    shader.vertexAttribs = ATTR_POSITION | ATTR_NORMAL;

    if (shader.isPortal) {
        shader.vertexAttribs |= ATTR_NORMAL;
    }

    if (shader.defaultShader) {
        shader.vertexAttribs |= ATTR_TEXCOORD;
        return;
    }

    if (shader.numDeforms) {
        for (i = 0; i < shader.numDeforms; i++) {
            deformStage_t* ds = &shader.deforms[i];

            switch (ds->deformation) {
                case DEFORM_BULGE:
                    shader.vertexAttribs |= ATTR_NORMAL | ATTR_TEXCOORD;
                    break;

                case DEFORM_AUTOSPRITE:
                    shader.vertexAttribs |= ATTR_NORMAL | ATTR_COLOR;
                    break;

                case DEFORM_WAVE:
                case DEFORM_NORMALS:
                case DEFORM_TEXT0:
                case DEFORM_TEXT1:
                case DEFORM_TEXT2:
                case DEFORM_TEXT3:
                case DEFORM_TEXT4:
                case DEFORM_TEXT5:
                case DEFORM_TEXT6:
                case DEFORM_TEXT7:
                    shader.vertexAttribs |= ATTR_NORMAL;
                    break;

                default:
                case DEFORM_NONE:
                case DEFORM_MOVE:
                case DEFORM_PROJECTION_SHADOW:
                case DEFORM_AUTOSPRITE2:
                    break;
            }
        }
    }

    for (stage = 0; stage < MAX_SHADER_STAGES; stage++) {
        shaderStage_t* pStage = &stages[stage];

        if (!pStage->active) {
            break;
        }

        if (pStage->glslShaderGroup == tr.lightallShader) {
            shader.vertexAttribs |= ATTR_NORMAL;

            if ((pStage->glslShaderIndex & LIGHTDEF_LIGHTTYPE_MASK) && !(r_normalMapping->integer == 0 && r_specularMapping->integer == 0)) {
                shader.vertexAttribs |= ATTR_TANGENT;
            }

            switch (pStage->glslShaderIndex & LIGHTDEF_LIGHTTYPE_MASK) {
                case LIGHTDEF_USE_LIGHTMAP:
                case LIGHTDEF_USE_LIGHT_VERTEX:
                    shader.vertexAttribs |= ATTR_LIGHTDIRECTION;
                    break;
                default:
                    break;
            }
        }

        for (i = 0; i < NUM_TEXTURE_BUNDLES; i++) {
            if (pStage->bundle[i].image[0] == 0) {
                continue;
            }

            switch (pStage->bundle[i].tcGen) {
                case TCGEN_TEXTURE:
                    shader.vertexAttribs |= ATTR_TEXCOORD;
                    break;
                case TCGEN_LIGHTMAP:
                    shader.vertexAttribs |= ATTR_LIGHTCOORD;
                    break;
                case TCGEN_ENVIRONMENT_MAPPED:
                    shader.vertexAttribs |= ATTR_NORMAL;
                    break;

                default:
                    break;
            }
        }

        switch (pStage->rgbGen) {
            case CGEN_EXACT_VERTEX:
            case CGEN_VERTEX:
            case CGEN_EXACT_VERTEX_LIT:
            case CGEN_VERTEX_LIT:
            case CGEN_ONE_MINUS_VERTEX:
                shader.vertexAttribs |= ATTR_COLOR;
                break;

            case CGEN_LIGHTING_DIFFUSE:
                shader.vertexAttribs |= ATTR_NORMAL;
                break;

            default:
                break;
        }

        switch (pStage->alphaGen) {
            case AGEN_LIGHTING_SPECULAR:
                shader.vertexAttribs |= ATTR_NORMAL;
                break;

            case AGEN_VERTEX:
            case AGEN_ONE_MINUS_VERTEX:
                shader.vertexAttribs |= ATTR_COLOR;
                break;

            default:
                break;
        }
    }
}

static void CollapseStagesToLightall(shaderStage_t* diffuse,
                                     shaderStage_t* normal,
                                     shaderStage_t* specular,
                                     shaderStage_t* lightmap,
                                     qboolean useLightVector,
                                     qboolean useLightVertex,
                                     qboolean parallax,
                                     qboolean tcgen) {
    int defs = 0;

    diffuse->type = ST_GLSL;

    if (lightmap) {
        diffuse->bundle[TB_LIGHTMAP] = lightmap->bundle[0];
        defs |= LIGHTDEF_USE_LIGHTMAP;
    } else if (useLightVector) {
        defs |= LIGHTDEF_USE_LIGHT_VECTOR;
    } else if (useLightVertex) {
        defs |= LIGHTDEF_USE_LIGHT_VERTEX;
    }

    if (r_deluxeMapping->integer && tr.worldDeluxeMapping && lightmap && shader.lightmapIndex >= 0) {
        diffuse->bundle[TB_DELUXEMAP] = lightmap->bundle[0];
        diffuse->bundle[TB_DELUXEMAP].image[0] = tr.deluxemaps[shader.lightmapIndex];
    }

    if (r_normalMapping->integer) {
        image_t* diffuseImg;
        if (normal) {
            diffuse->bundle[TB_NORMALMAP] = normal->bundle[0];
            if (parallax && r_parallaxMapping->integer)
                defs |= LIGHTDEF_USE_PARALLAXMAP;

            VectorCopy4(normal->normalScale, diffuse->normalScale);
        } else if ((lightmap || useLightVector || useLightVertex) && (diffuseImg = diffuse->bundle[TB_DIFFUSEMAP].image[0])) {
            char normalName[MAX_QPATH];
            image_t* normalImg;
            imgFlags_t normalFlags = (diffuseImg->flags & ~IMGFLAG_GENNORMALMAP) | IMGFLAG_NOLIGHTSCALE;

            COM_StripExtension(diffuseImg->imgName, normalName, MAX_QPATH);
            Q_strcat(normalName, MAX_QPATH, "_nh");

            normalImg = R_FindImageFile(normalName, IMGTYPE_NORMALHEIGHT, normalFlags);

            if (normalImg) {
                parallax = qtrue;
            } else {
                normalName[strlen(normalName) - 1] = '\0';
                normalImg = R_FindImageFile(normalName, IMGTYPE_NORMAL, normalFlags);
            }

            if (normalImg) {
                diffuse->bundle[TB_NORMALMAP] = diffuse->bundle[0];
                diffuse->bundle[TB_NORMALMAP].numImageAnimations = 0;
                diffuse->bundle[TB_NORMALMAP].image[0] = normalImg;

                if (parallax && r_parallaxMapping->integer)
                    defs |= LIGHTDEF_USE_PARALLAXMAP;

                VectorSet4(diffuse->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
            }
        }
    }

    if (r_specularMapping->integer) {
        image_t* diffuseImg;
        if (specular) {
            diffuse->bundle[TB_SPECULARMAP] = specular->bundle[0];
            VectorCopy4(specular->specularScale, diffuse->specularScale);
        } else if ((lightmap || useLightVector || useLightVertex) && (diffuseImg = diffuse->bundle[TB_DIFFUSEMAP].image[0])) {
            char specularName[MAX_QPATH];
            image_t* specularImg;
            imgFlags_t specularFlags = (diffuseImg->flags & ~IMGFLAG_GENNORMALMAP) | IMGFLAG_NOLIGHTSCALE;

            COM_StripExtension(diffuseImg->imgName, specularName, MAX_QPATH);
            Q_strcat(specularName, MAX_QPATH, "_s");

            specularImg = R_FindImageFile(specularName, IMGTYPE_COLORALPHA, specularFlags);

            if (specularImg) {
                diffuse->bundle[TB_SPECULARMAP] = diffuse->bundle[0];
                diffuse->bundle[TB_SPECULARMAP].numImageAnimations = 0;
                diffuse->bundle[TB_SPECULARMAP].image[0] = specularImg;

                VectorSet4(diffuse->specularScale, 1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    }

    if (tcgen || diffuse->bundle[0].numTexMods) {
        defs |= LIGHTDEF_USE_TCGEN_AND_TCMOD;
    }

    diffuse->glslShaderGroup = tr.lightallShader;
    diffuse->glslShaderIndex = defs;
}

static int CollapseStagesToGLSL(void) {
    int i, j, numStages;
    qboolean skip = qfalse;

    if (shader.numDeforms != 0) {
        skip = qtrue;
    }

    if (!skip) {
        if (stages[0].active && stages[0].bundle[0].tcGen == TCGEN_LIGHTMAP && stages[1].active) {
            int blendBits = stages[1].stateBits & (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

            if (blendBits == (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO) || blendBits == (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR)) {
                int stateBits0 = stages[0].stateBits;
                int stateBits1 = stages[1].stateBits;
                shaderStage_t swapStage;

                swapStage = stages[0];
                stages[0] = stages[1];
                stages[1] = swapStage;

                stages[0].stateBits = stateBits0;
                stages[1].stateBits = stateBits1;
            }
        }
    }

    if (!skip) {
        for (i = 0; i < MAX_SHADER_STAGES; i++) {
            shaderStage_t* pStage = &stages[i];

            if (!pStage->active)
                continue;

            if (pStage->adjustColorsForFog) {
                skip = qtrue;
                break;
            }

            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP) {
                int blendBits = pStage->stateBits & (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

                if (blendBits != (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO) && blendBits != (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR)) {
                    skip = qtrue;
                    break;
                }
            }

            switch (pStage->bundle[0].tcGen) {
                case TCGEN_TEXTURE:
                case TCGEN_LIGHTMAP:
                case TCGEN_ENVIRONMENT_MAPPED:
                case TCGEN_VECTOR:
                    break;
                default:
                    skip = qtrue;
                    break;
            }

            switch (pStage->alphaGen) {
                case AGEN_LIGHTING_SPECULAR:
                case AGEN_PORTAL:
                    skip = qtrue;
                    break;
                default:
                    break;
            }
        }
    }

    if (!skip) {
        qboolean usedLightmap = qfalse;

        for (i = 0; i < MAX_SHADER_STAGES; i++) {
            shaderStage_t* pStage = &stages[i];
            shaderStage_t *diffuse, *normal, *specular, *lightmap;
            qboolean parallax, tcgen, diffuselit, vertexlit;

            if (!pStage->active)
                continue;

            if (pStage->type != ST_COLORMAP)
                continue;

            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP)
                continue;

            diffuse = pStage;
            normal = NULL;
            parallax = qfalse;
            specular = NULL;
            lightmap = NULL;

            for (j = i + 1; j < MAX_SHADER_STAGES; j++) {
                shaderStage_t* pStage2 = &stages[j];

                if (!pStage2->active)
                    continue;

                switch (pStage2->type) {
                    case ST_NORMALMAP:
                        if (!normal) {
                            normal = pStage2;
                        }
                        break;

                    case ST_NORMALPARALLAXMAP:
                        if (!normal) {
                            normal = pStage2;
                            parallax = qtrue;
                        }
                        break;

                    case ST_SPECULARMAP:
                        if (!specular) {
                            specular = pStage2;
                        }
                        break;

                    case ST_COLORMAP:
                        if (pStage2->bundle[0].tcGen == TCGEN_LIGHTMAP) {
                            int blendBits = pStage->stateBits & (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

                            if (!usedLightmap || (blendBits != (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO) && blendBits != (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR))) {
                                lightmap = pStage2;
                                usedLightmap = qtrue;
                            }
                        }
                        break;

                    default:
                        break;
                }
            }

            tcgen = qfalse;
            if (diffuse->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED || diffuse->bundle[0].tcGen == TCGEN_LIGHTMAP || diffuse->bundle[0].tcGen == TCGEN_VECTOR) {
                tcgen = qtrue;
            }

            diffuselit = qfalse;
            if (diffuse->rgbGen == CGEN_LIGHTING_DIFFUSE) {
                diffuselit = qtrue;
            }

            vertexlit = qfalse;
            if (diffuse->rgbGen == CGEN_VERTEX_LIT || diffuse->rgbGen == CGEN_EXACT_VERTEX_LIT) {
                vertexlit = qtrue;
            }

            CollapseStagesToLightall(diffuse, normal, specular, lightmap, diffuselit, vertexlit, parallax, tcgen);
        }

        for (i = 0; i < MAX_SHADER_STAGES; i++) {
            shaderStage_t* pStage = &stages[i];

            if (!pStage->active)
                continue;

            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP) {
                pStage->active = qfalse;
            }
        }
    }

    for (i = 0; i < MAX_SHADER_STAGES; i++) {
        shaderStage_t* pStage = &stages[i];

        if (!pStage->active)
            continue;

        if (pStage->type == ST_NORMALMAP) {
            pStage->active = qfalse;
        }

        if (pStage->type == ST_NORMALPARALLAXMAP) {
            pStage->active = qfalse;
        }

        if (pStage->type == ST_SPECULARMAP) {
            pStage->active = qfalse;
        }
    }

    numStages = 0;
    for (i = 0; i < MAX_SHADER_STAGES; i++) {
        if (!stages[i].active)
            continue;

        if (i == numStages) {
            numStages++;
            continue;
        }

        stages[numStages] = stages[i];
        stages[i].active = qfalse;
        numStages++;
    }

    if (r_sunlightMode->integer && shader.numDeforms == 0) {
        for (i = 0; i < MAX_SHADER_STAGES; i++) {
            shaderStage_t* pStage = &stages[i];
            int blendBits;

            if (!pStage->active)
                continue;

            if (pStage->adjustColorsForFog)
                continue;

            if (pStage->bundle[TB_DIFFUSEMAP].tcGen != TCGEN_LIGHTMAP)
                continue;

            blendBits = pStage->stateBits & (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

            if (blendBits != 0 &&
                blendBits != (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO) &&
                blendBits != (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR)) {
                continue;
            }

            pStage->glslShaderGroup = tr.lightallShader;
            pStage->glslShaderIndex = LIGHTDEF_USE_LIGHTMAP;
            pStage->bundle[TB_LIGHTMAP] = pStage->bundle[TB_DIFFUSEMAP];
            pStage->bundle[TB_DIFFUSEMAP].image[0] = tr.whiteImage;
            pStage->bundle[TB_DIFFUSEMAP].isLightmap = qfalse;
            pStage->bundle[TB_DIFFUSEMAP].tcGen = TCGEN_TEXTURE;
        }
    }

    if (shader.numDeforms == 0) {
        for (i = 0; i < MAX_SHADER_STAGES; i++) {
            shaderStage_t* pStage = &stages[i];

            if (!pStage->active)
                continue;

            if (pStage->adjustColorsForFog)
                continue;

            if (pStage->rgbGen == CGEN_LIGHTING_DIFFUSE) {
                pStage->glslShaderGroup = tr.lightallShader;
                pStage->glslShaderIndex = LIGHTDEF_USE_LIGHT_VECTOR;

                if (pStage->bundle[0].tcGen != TCGEN_TEXTURE || pStage->bundle[0].numTexMods != 0)
                    pStage->glslShaderIndex |= LIGHTDEF_USE_TCGEN_AND_TCMOD;
            }
        }
    }

    return numStages;
}

static void FixRenderCommandList(int newShader) {
    renderCommandList_t* cmdList = &backEndData->commands;

    if (cmdList) {
        const void* curCmd = cmdList->cmds;

        while (1) {
            curCmd = PADP(curCmd, sizeof(void*));

            switch (*(const int*)curCmd) {
                case RC_SET_COLOR: {
                    const setColorCommand_t* sc_cmd = (const setColorCommand_t*)curCmd;
                    curCmd = (const void*)(sc_cmd + 1);
                    break;
                }
                case RC_STRETCH_PIC: {
                    const stretchPicCommand_t* sp_cmd = (const stretchPicCommand_t*)curCmd;
                    curCmd = (const void*)(sp_cmd + 1);
                    break;
                }
                case RC_DRAW_SURFS: {
                    int i;
                    drawSurf_t* drawSurf;
                    shader_t* shader;
                    int fogNum;
                    int entityNum;
                    int dlightMap;
                    int pshadowMap;
                    int sortedIndex;
                    const drawSurfsCommand_t* ds_cmd = (const drawSurfsCommand_t*)curCmd;

                    for (i = 0, drawSurf = ds_cmd->drawSurfs; i < ds_cmd->numDrawSurfs; i++, drawSurf++) {
                        R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &dlightMap, &pshadowMap);
                        sortedIndex = ((drawSurf->sort >> QSORT_SHADERNUM_SHIFT) & (MAX_SHADERS - 1));
                        if (sortedIndex >= newShader) {
                            sortedIndex++;
                            drawSurf->sort = (sortedIndex << QSORT_SHADERNUM_SHIFT) | entityNum | (fogNum << QSORT_FOGNUM_SHIFT) | ((int)pshadowMap << QSORT_PSHADOW_SHIFT) | (int)dlightMap;
                        }
                    }
                    curCmd = (const void*)(ds_cmd + 1);
                    break;
                }
                case RC_DRAW_BUFFER: {
                    const drawBufferCommand_t* db_cmd = (const drawBufferCommand_t*)curCmd;
                    curCmd = (const void*)(db_cmd + 1);
                    break;
                }
                case RC_SWAP_BUFFERS: {
                    const swapBuffersCommand_t* sb_cmd = (const swapBuffersCommand_t*)curCmd;
                    curCmd = (const void*)(sb_cmd + 1);
                    break;
                }
                case RC_END_OF_LIST:
                default:
                    return;
            }
        }
    }
}

static void SortNewShader(void) {
    int i;
    float sort;
    shader_t* newShader;

    newShader = tr.shaders[tr.numShaders - 1];
    sort = newShader->sort;

    for (i = tr.numShaders - 2; i >= 0; i--) {
        if (tr.sortedShaders[i]->sort <= sort) {
            break;
        }
        tr.sortedShaders[i + 1] = tr.sortedShaders[i];
        tr.sortedShaders[i + 1]->sortedIndex++;
    }

    FixRenderCommandList(i + 1);

    newShader->sortedIndex = i + 1;
    tr.sortedShaders[i + 1] = newShader;
}

static shader_t* GeneratePermanentShader(void) {
    shader_t* newShader;
    int i, b;
    int size, hash;

    if (tr.numShaders == MAX_SHADERS) {
        ri.Printf(PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
        return tr.defaultShader;
    }

    newShader = ri.Hunk_Alloc(sizeof(shader_t), h_low);

    *newShader = shader;

    if (shader.sort <= SS_OPAQUE) {
        newShader->fogPass = FP_EQUAL;
    } else if (shader.contentFlags & CONTENTS_FOG) {
        newShader->fogPass = FP_LE;
    }

    tr.shaders[tr.numShaders] = newShader;
    newShader->index = tr.numShaders;

    tr.sortedShaders[tr.numShaders] = newShader;
    newShader->sortedIndex = tr.numShaders;

    tr.numShaders++;

    for (i = 0; i < newShader->numUnfoggedPasses; i++) {
        if (!stages[i].active) {
            break;
        }
        newShader->stages[i] = ri.Hunk_Alloc(sizeof(stages[i]), h_low);
        *newShader->stages[i] = stages[i];

        for (b = 0; b < NUM_TEXTURE_BUNDLES; b++) {
            size = newShader->stages[i]->bundle[b].numTexMods * sizeof(texModInfo_t);
            newShader->stages[i]->bundle[b].texMods = ri.Hunk_Alloc(size, h_low);
            Com_Memcpy(newShader->stages[i]->bundle[b].texMods, stages[i].bundle[b].texMods, size);
        }
    }

    SortNewShader();

    hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
    newShader->next = hashTable[hash];
    hashTable[hash] = newShader;

    return newShader;
}

static void VertexLightingCollapse(void) {
    int stage;
    shaderStage_t* bestStage;
    int bestImageRank;
    int rank;

    if (shader.sort == SS_OPAQUE) {
        bestStage = &stages[0];
        bestImageRank = -999999;

        for (stage = 0; stage < MAX_SHADER_STAGES; stage++) {
            shaderStage_t* pStage = &stages[stage];

            if (!pStage->active) {
                break;
            }
            rank = 0;

            if (pStage->bundle[0].isLightmap) {
                rank -= 100;
            }
            if (pStage->bundle[0].tcGen != TCGEN_TEXTURE) {
                rank -= 5;
            }
            if (pStage->bundle[0].numTexMods) {
                rank -= 5;
            }
            if (pStage->rgbGen != CGEN_IDENTITY && pStage->rgbGen != CGEN_IDENTITY_LIGHTING) {
                rank -= 3;
            }

            if (rank > bestImageRank) {
                bestImageRank = rank;
                bestStage = pStage;
            }
        }

        stages[0].bundle[0] = bestStage->bundle[0];
        stages[0].stateBits &= ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);
        stages[0].stateBits |= GLS_DEPTHMASK_TRUE;
        if (shader.lightmapIndex == LIGHTMAP_NONE) {
            stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
        } else {
            stages[0].rgbGen = CGEN_EXACT_VERTEX;
        }
        stages[0].alphaGen = AGEN_SKIP;
    } else {
        if (stages[0].bundle[0].isLightmap) {
            stages[0] = stages[1];
        }

        if (stages[0].rgbGen == CGEN_ONE_MINUS_ENTITY || stages[1].rgbGen == CGEN_ONE_MINUS_ENTITY) {
            stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
        }
        if ((stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_SAWTOOTH) && (stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_INVERSE_SAWTOOTH)) {
            stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
        }
        if ((stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_INVERSE_SAWTOOTH) && (stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_SAWTOOTH)) {
            stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
        }
    }

    for (stage = 1; stage < MAX_SHADER_STAGES; stage++) {
        shaderStage_t* pStage = &stages[stage];

        if (!pStage->active) {
            break;
        }

        Com_Memset(pStage, 0, sizeof(*pStage));
    }
}

static void FixFatLightmapTexCoords(void) {
    texModInfo_t* tmi;
    int lightmapnum;
    int stage;
    int size;
    int i;

    if (!r_mergeLightmaps->integer || tr.fatLightmapCols <= 0) {
        return;
    }

    if (shader.lightmapIndex < 0) {
        return;
    }

    lightmapnum = shader_realLightmapIndex;

    if (tr.worldDeluxeMapping)
        lightmapnum >>= 1;

    lightmapnum %= (tr.fatLightmapCols * tr.fatLightmapRows);

    for (stage = 0; stage < MAX_SHADER_STAGES; stage++) {
        shaderStage_t* pStage = &stages[stage];

        if (!pStage->active) {
            break;
        }

        if (pStage->bundle[0].isLightmap) {
            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP) {
                for (i = 0; i < pStage->bundle[0].numTexMods; i++) {
                    tmi = &pStage->bundle[0].texMods[i];

                    if (tmi->type == TMOD_TRANSFORM) {
                        tmi->translate[0] /= (float)tr.fatLightmapCols;
                        tmi->translate[1] /= (float)tr.fatLightmapRows;
                    }
                }
            }

            if (pStage->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED) {
                if (pStage->bundle[0].numTexMods == TR_MAX_TEXMODS) {
                    ri.Printf(PRINT_DEVELOPER, "WARNING: too many tcmods to fix lightmap texcoords for r_mergeLightmaps in shader '%s'", shader.name);
                } else {
                    tmi = &pStage->bundle[0].texMods[pStage->bundle[0].numTexMods];
                    pStage->bundle[0].numTexMods++;

                    tmi->matrix[0][0] = 1.0f / tr.fatLightmapCols;
                    tmi->matrix[0][1] = 0;
                    tmi->matrix[1][0] = 0;
                    tmi->matrix[1][1] = 1.0f / tr.fatLightmapRows;

                    tmi->translate[0] = (lightmapnum % tr.fatLightmapCols) / (float)tr.fatLightmapCols;
                    tmi->translate[1] = (lightmapnum / tr.fatLightmapCols) / (float)tr.fatLightmapRows;

                    tmi->type = TMOD_TRANSFORM;
                }
            }
        }
        else if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP) {
            if (pStage->bundle[0].numTexMods == TR_MAX_TEXMODS) {
                ri.Printf(PRINT_DEVELOPER, "WARNING: too many tcmods to fix lightmap texcoords for r_mergeLightmaps in shader '%s'", shader.name);
            } else {
                size = pStage->bundle[0].numTexMods * sizeof(texModInfo_t);

                if (size) {
                    memmove(&pStage->bundle[0].texMods[1], &pStage->bundle[0].texMods[0], size);
                }

                tmi = &pStage->bundle[0].texMods[0];
                pStage->bundle[0].numTexMods++;

                tmi->matrix[0][0] = tr.fatLightmapCols;
                tmi->matrix[0][1] = 0;
                tmi->matrix[1][0] = 0;
                tmi->matrix[1][1] = tr.fatLightmapRows;

                tmi->translate[0] = -(lightmapnum % tr.fatLightmapCols);
                tmi->translate[1] = -(lightmapnum / tr.fatLightmapCols);

                tmi->type = TMOD_TRANSFORM;
            }
        }
    }
}

static void InitShaderEx(const char* name, int lightmapIndex, int realLightmapIndex) {
    int i;

    Com_Memset(&shader, 0, sizeof(shader));
    Com_Memset(&stages, 0, sizeof(stages));

    Q_strncpyz(shader.name, name, sizeof(shader.name));
    shader.lightmapIndex = lightmapIndex;
    shader_realLightmapIndex = realLightmapIndex;
    shader_noVLCollapse = qfalse;

    for (i = 0; i < MAX_SHADER_STAGES; i++) {
        stages[i].bundle[0].texMods = texMods[i];

        VectorSet4(stages[i].normalScale, 0.0f, 0.0f, 0.0f, 0.0f);
        if (r_pbr->integer) {
            stages[i].specularScale[0] = r_baseGloss->value;
        } else {
            stages[i].specularScale[0] =
                stages[i].specularScale[1] =
                    stages[i].specularScale[2] = r_baseSpecular->value;
            stages[i].specularScale[3] = r_baseGloss->value;
        }
    }
}

static void InitShader(const char* name, int lightmapIndex) {
    InitShaderEx(name, lightmapIndex, lightmapIndex);
}

static shader_t* FinishShader(void) {
    int stage;
    qboolean hasLightmapStage;
    qboolean vertexLightmap;

    hasLightmapStage = qfalse;
    vertexLightmap = qfalse;

    if (shader.isSky) {
        shader.sort = SS_ENVIRONMENT;
    }

    if (shader.polygonOffset && !shader.sort) {
        shader.sort = SS_DECAL;
    }

    for (stage = 0; stage < MAX_SHADER_STAGES;) {
        shaderStage_t* pStage = &stages[stage];

        if (!pStage->active) {
            break;
        }

        if (!pStage->bundle[0].image[0]) {
            ri.Printf(PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name);
            pStage->active = qfalse;
            stage++;
            continue;
        }

        if (pStage->isDetail && !r_detailTextures->integer) {
            int index;

            for (index = stage + 1; index < MAX_SHADER_STAGES; index++) {
                if (!stages[index].active)
                    break;
            }

            if (index < MAX_SHADER_STAGES)
                memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage));
            else {
                if (stage + 1 < MAX_SHADER_STAGES)
                    memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage - 1));

                Com_Memset(&stages[index - 1], 0, sizeof(*stages));
            }

            continue;
        }

        if (pStage->bundle[0].isLightmap) {
            if (pStage->bundle[0].tcGen == TCGEN_BAD) {
                pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
            }
            hasLightmapStage = qtrue;
        } else {
            if (pStage->bundle[0].tcGen == TCGEN_BAD) {
                pStage->bundle[0].tcGen = TCGEN_TEXTURE;
            }
        }

        if ((pStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) &&
            (stages[0].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))) {
            int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
            int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

            if (((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ONE)) ||
                ((blendSrcBits == GLS_SRCBLEND_ZERO) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR))) {
                pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
            }
            else if ((blendSrcBits == GLS_SRCBLEND_SRC_ALPHA) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)) {
                pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
            }
            else if ((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)) {
                pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
            } else {
            }

            if (!shader.sort) {
                if (pStage->stateBits & GLS_DEPTHMASK_TRUE) {
                    shader.sort = SS_SEE_THROUGH;
                } else {
                    shader.sort = SS_BLEND0;
                }
            }
        }

        stage++;
    }

    if (!shader.sort) {
        shader.sort = SS_OPAQUE;
    }

    if (stage > 1 && !shader_noVLCollapse &&
        ((r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2)) {
        VertexLightingCollapse();
        hasLightmapStage = qfalse;
    }

    FixFatLightmapTexCoords();

    stage = CollapseStagesToGLSL();

    if (shader.lightmapIndex >= 0 && !hasLightmapStage) {
        if (vertexLightmap) {
            ri.Printf(PRINT_DEVELOPER, "WARNING: shader '%s' has VERTEX forced lightmap!\n", shader.name);
        } else {
            ri.Printf(PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name);
        }
    }

    shader.numUnfoggedPasses = stage;

    if (stage == 0 && !shader.isSky)
        shader.sort = SS_FOG;

    ComputeStageIteratorFunc();

    ComputeVertexAttribs();

    return GeneratePermanentShader();
}

static char* FindShaderInShaderText(const char* shadername) {
    char *token, *p;

    int i, hash;

    hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

    if (shaderTextHashTable[hash]) {
        for (i = 0; shaderTextHashTable[hash][i]; i++) {
            p = shaderTextHashTable[hash][i];
            token = COM_ParseExt(&p, qtrue);

            if (!Q_stricmp(token, shadername))
                return p;
        }
    }

    return NULL;
}

shader_t* R_FindShaderByName(const char* name) {
    char strippedName[MAX_QPATH];
    int hash;
    shader_t* sh;

    if ((name == NULL) || (name[0] == 0)) {
        return tr.defaultShader;
    }

    COM_StripExtension(name, strippedName, sizeof(strippedName));

    hash = generateHashValue(strippedName, FILE_HASH_SIZE);

    for (sh = hashTable[hash]; sh; sh = sh->next) {
        if (Q_stricmp(sh->name, strippedName) == 0) {
            return sh;
        }
    }

    return tr.defaultShader;
}

shader_t* R_FindShader(const char* name, int lightmapIndex, qboolean mipRawImage) {
    return R_FindShaderEx(name, lightmapIndex, mipRawImage, lightmapIndex);
}

int64_t r_shaderFindUs, r_shaderCachedUs;
int r_shaderFindCount, r_shaderCachedCount;

static shader_t* R_FindShaderInner(const char* name, int lightmapIndex, qboolean mipRawImage, int realLightmapIndex);

shader_t* R_FindShaderEx(const char* name, int lightmapIndex, qboolean mipRawImage, int realLightmapIndex) {
    int64_t start = ri.Microseconds();
    int before = tr.numShaders;
    shader_t* found = R_FindShaderInner(name, lightmapIndex, mipRawImage, realLightmapIndex);
    int64_t cost = ri.Microseconds() - start;

    r_shaderFindUs += cost;
    r_shaderFindCount++;
    if (tr.numShaders == before) {
        r_shaderCachedUs += cost;
        r_shaderCachedCount++;
    }
    return found;
}

static shader_t* R_FindShaderInner(const char* name, int lightmapIndex, qboolean mipRawImage, int realLightmapIndex) {
    char strippedName[MAX_QPATH];
    int hash;
    char* shaderText;
    image_t* image;
    shader_t* sh;

    if (name[0] == 0) {
        return tr.defaultShader;
    }

    if (lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps) {
        lightmapIndex = LIGHTMAP_BY_VERTEX;
    } else if (lightmapIndex < LIGHTMAP_2D) {
        ri.Printf(PRINT_WARNING, "WARNING: shader '%s' has invalid lightmap index of %d\n", name, lightmapIndex);
        lightmapIndex = LIGHTMAP_BY_VERTEX;
    }

    COM_StripExtension(name, strippedName, sizeof(strippedName));

    hash = generateHashValue(strippedName, FILE_HASH_SIZE);

    for (sh = hashTable[hash]; sh; sh = sh->next) {
        if ((sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
            !Q_stricmp(sh->name, strippedName)) {
            return sh;
        }
    }

    InitShaderEx(strippedName, lightmapIndex, realLightmapIndex);

    shaderText = FindShaderInShaderText(strippedName);
    if (shaderText) {
        if (r_printShaders->integer) {
            ri.Printf(PRINT_ALL, "*SHADER* %s\n", name);
        }

        if (!ParseShader(&shaderText)) {
            shader.defaultShader = qtrue;
        }
        sh = FinishShader();
        return sh;
    }

    {
        imgFlags_t flags;

        flags = IMGFLAG_NONE;

        if (mipRawImage) {
            flags |= IMGFLAG_MIPMAP | IMGFLAG_PICMIP;

            if (r_genNormalMaps->integer)
                flags |= IMGFLAG_GENNORMALMAP;
        } else {
            flags |= IMGFLAG_CLAMPTOEDGE;
        }

        image = R_FindImageFile(name, IMGTYPE_COLORALPHA, flags);
        if (!image) {
            ri.Printf(PRINT_DEVELOPER, "Couldn't find image file for shader %s\n", name);
            shader.defaultShader = qtrue;
            return FinishShader();
        }
    }

    if (shader.lightmapIndex == LIGHTMAP_NONE) {
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
        stages[0].stateBits = GLS_DEFAULT;
    } else if (shader.lightmapIndex == LIGHTMAP_BY_VERTEX) {
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_EXACT_VERTEX;
        stages[0].alphaGen = AGEN_SKIP;
        stages[0].stateBits = GLS_DEFAULT;
    } else if (shader.lightmapIndex == LIGHTMAP_2D) {
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_VERTEX;
        stages[0].alphaGen = AGEN_VERTEX;
        stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
                              GLS_SRCBLEND_SRC_ALPHA |
                              GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    } else if (shader.lightmapIndex == LIGHTMAP_WHITEIMAGE) {
        stages[0].bundle[0].image[0] = tr.whiteImage;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
        stages[0].stateBits = GLS_DEFAULT;

        stages[1].bundle[0].image[0] = image;
        stages[1].active = qtrue;
        stages[1].rgbGen = CGEN_IDENTITY;
        stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    } else {
        stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
        stages[0].bundle[0].isLightmap = qtrue;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_IDENTITY;
        stages[0].stateBits = GLS_DEFAULT;

        stages[1].bundle[0].image[0] = image;
        stages[1].active = qtrue;
        stages[1].rgbGen = CGEN_IDENTITY;
        stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    }

    return FinishShader();
}

qhandle_t RE_RegisterShaderFromImage(const char* name, int lightmapIndex, image_t* image, qboolean mipRawImage) {
    int hash;
    shader_t* sh;

    hash = generateHashValue(name, FILE_HASH_SIZE);

    if (lightmapIndex >= tr.numLightmaps) {
        lightmapIndex = LIGHTMAP_WHITEIMAGE;
    }

    for (sh = hashTable[hash]; sh; sh = sh->next) {
        if ((sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
            !Q_stricmp(sh->name, name)) {
            return sh->index;
        }
    }

    InitShader(name, lightmapIndex);

    if (shader.lightmapIndex == LIGHTMAP_NONE) {
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
        stages[0].stateBits = GLS_DEFAULT;
    } else if (shader.lightmapIndex == LIGHTMAP_BY_VERTEX) {
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_EXACT_VERTEX;
        stages[0].alphaGen = AGEN_SKIP;
        stages[0].stateBits = GLS_DEFAULT;
    } else if (shader.lightmapIndex == LIGHTMAP_2D) {
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_VERTEX;
        stages[0].alphaGen = AGEN_VERTEX;
        stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
                              GLS_SRCBLEND_SRC_ALPHA |
                              GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    } else if (shader.lightmapIndex == LIGHTMAP_WHITEIMAGE) {
        stages[0].bundle[0].image[0] = tr.whiteImage;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
        stages[0].stateBits = GLS_DEFAULT;

        stages[1].bundle[0].image[0] = image;
        stages[1].active = qtrue;
        stages[1].rgbGen = CGEN_IDENTITY;
        stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    } else {
        stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
        stages[0].bundle[0].isLightmap = qtrue;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_IDENTITY;
        stages[0].stateBits = GLS_DEFAULT;

        stages[1].bundle[0].image[0] = image;
        stages[1].active = qtrue;
        stages[1].rgbGen = CGEN_IDENTITY;
        stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    }

    sh = FinishShader();
    return sh->index;
}

qhandle_t RE_RegisterShaderLightMap(const char* name, int lightmapIndex) {
    shader_t* sh;

    if (strlen(name) >= MAX_QPATH) {
        ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n");
        return 0;
    }

    sh = R_FindShader(name, lightmapIndex, qtrue);

    if (sh->defaultShader) {
        return 0;
    }

    return sh->index;
}

qhandle_t RE_RegisterShader(const char* name) {
    shader_t* sh;

    if (strlen(name) >= MAX_QPATH) {
        ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n");
        return 0;
    }

    sh = R_FindShader(name, LIGHTMAP_2D, qtrue);

    if (sh->defaultShader) {
        return 0;
    }

    return sh->index;
}

qhandle_t RE_RegisterShaderNoMip(const char* name) {
    shader_t* sh;

    if (strlen(name) >= MAX_QPATH) {
        ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n");
        return 0;
    }

    sh = R_FindShader(name, LIGHTMAP_2D, qfalse);

    if (sh->defaultShader) {
        return 0;
    }

    return sh->index;
}

shader_t* R_GetShaderByHandle(qhandle_t hShader) {
    if (hShader < 0) {
        ri.Printf(PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader);
        return tr.defaultShader;
    }
    if (hShader >= tr.numShaders) {
        ri.Printf(PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader);
        return tr.defaultShader;
    }
    return tr.shaders[hShader];
}

void R_ShaderList_f(void) {
    int i;
    int count;
    shader_t* shader;

    ri.Printf(PRINT_ALL, "-----------------------\n");

    count = 0;
    for (i = 0; i < tr.numShaders; i++) {
        if (ri.Cmd_Argc() > 1) {
            shader = tr.sortedShaders[i];
        } else {
            shader = tr.shaders[i];
        }

        ri.Printf(PRINT_ALL, "%i ", shader->numUnfoggedPasses);

        if (shader->lightmapIndex >= 0) {
            ri.Printf(PRINT_ALL, "L ");
        } else {
            ri.Printf(PRINT_ALL, "  ");
        }
        if (shader->explicitlyDefined) {
            ri.Printf(PRINT_ALL, "E ");
        } else {
            ri.Printf(PRINT_ALL, "  ");
        }

        if (shader->optimalStageIteratorFunc == RB_StageIteratorGeneric) {
            ri.Printf(PRINT_ALL, "gen ");
        } else if (shader->optimalStageIteratorFunc == RB_StageIteratorSky) {
            ri.Printf(PRINT_ALL, "sky ");
        } else {
            ri.Printf(PRINT_ALL, "    ");
        }

        if (shader->defaultShader) {
            ri.Printf(PRINT_ALL, ": %s (DEFAULTED)\n", shader->name);
        } else {
            ri.Printf(PRINT_ALL, ": %s\n", shader->name);
        }
        count++;
    }
    ri.Printf(PRINT_ALL, "%i total shaders\n", count);
    ri.Printf(PRINT_ALL, "------------------\n");
}

#define MAX_SHADER_FILES 4096
static void ScanAndLoadShaderFiles(void) {
    char** shaderFiles;
    char* buffers[MAX_SHADER_FILES] = {NULL};
    char* p;
    int numShaderFiles;
    int i;
    char *oldp, *token, *hashMem, *textEnd;
    int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size;
    char shaderName[MAX_QPATH];
    int shaderLine;

    long sum = 0, summand;
    shaderFiles = ri.FS_ListFiles("scripts", ".shader", &numShaderFiles);

    if (!shaderFiles || !numShaderFiles) {
        ri.Printf(PRINT_WARNING, "WARNING: no shader files found\n");
        return;
    }

    if (numShaderFiles > MAX_SHADER_FILES) {
        numShaderFiles = MAX_SHADER_FILES;
    }

    for (i = 0; i < numShaderFiles; i++) {
        char filename[MAX_QPATH];

        {
            char* ext;
            Com_sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);
            if ((ext = strrchr(filename, '.'))) {
                strcpy(ext, ".mtr");
            }

            if (ri.FS_ReadFile(filename, NULL) <= 0) {
                Com_sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);
            }
        }

        ri.Printf(PRINT_DEVELOPER, "...loading '%s'\n", filename);
        summand = ri.FS_ReadFile(filename, (void**)&buffers[i]);

        if (!buffers[i])
            ri.Error(ERR_DROP, "Couldn't load %s", filename);

        p = buffers[i];
        COM_BeginParseSession(filename);
        while (1) {
            token = COM_ParseExt(&p, qtrue);

            if (!*token)
                break;

            Q_strncpyz(shaderName, token, sizeof(shaderName));
            shaderLine = COM_GetCurrentParseLine();

            token = COM_ParseExt(&p, qtrue);
            if (token[0] != '{' || token[1] != '\0') {
                ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing opening brace",
                          filename, shaderName, shaderLine);
                if (token[0]) {
                    ri.Printf(PRINT_WARNING, " (found \"%s\" on line %d)", token, COM_GetCurrentParseLine());
                }
                ri.Printf(PRINT_WARNING, ".\n");
                ri.FS_FreeFile(buffers[i]);
                buffers[i] = NULL;
                break;
            }

            if (!SkipBracedSection(&p, 1)) {
                ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing closing brace.\n",
                          filename, shaderName, shaderLine);
                ri.FS_FreeFile(buffers[i]);
                buffers[i] = NULL;
                break;
            }
        }

        if (buffers[i])
            sum += summand;
    }

    s_shaderText = ri.Hunk_Alloc(sum + numShaderFiles * 2, h_low);
    s_shaderText[0] = '\0';
    textEnd = s_shaderText;

    for (i = numShaderFiles - 1; i >= 0; i--) {
        if (!buffers[i])
            continue;

        strcat(textEnd, buffers[i]);
        strcat(textEnd, "\n");
        textEnd += strlen(textEnd);
        ri.FS_FreeFile(buffers[i]);
    }

    COM_Compress(s_shaderText);

    ri.FS_FreeFileList(shaderFiles);

    Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));
    size = 0;

    p = s_shaderText;
    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (token[0] == 0) {
            break;
        }

        hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
        shaderTextHashTableSizes[hash]++;
        size++;
        SkipBracedSection(&p, 0);
    }

    size += MAX_SHADERTEXT_HASH;

    hashMem = ri.Hunk_Alloc(size * sizeof(char*), h_low);

    for (i = 0; i < MAX_SHADERTEXT_HASH; i++) {
        shaderTextHashTable[i] = (char**)hashMem;
        hashMem = ((char*)hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char*));
    }

    Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));

    p = s_shaderText;
    while (1) {
        oldp = p;
        token = COM_ParseExt(&p, qtrue);
        if (token[0] == 0) {
            break;
        }

        hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
        shaderTextHashTable[hash][shaderTextHashTableSizes[hash]++] = oldp;

        SkipBracedSection(&p, 0);
    }

    ri.Printf(PRINT_ALL, "ScanAndLoadShaderFiles: %d shader files, %d shaders total\n", numShaderFiles, size);

    return;
}

static void CreateInternalShaders(void) {
    tr.numShaders = 0;

    InitShader("<default>", LIGHTMAP_NONE);
    stages[0].bundle[0].image[0] = tr.defaultImage;
    stages[0].active = qtrue;
    stages[0].stateBits = GLS_DEFAULT;
    tr.defaultShader = FinishShader();

    Q_strncpyz(shader.name, "<stencil shadow>", sizeof(shader.name));
    shader.sort = SS_STENCIL_SHADOW;
    tr.shadowShader = FinishShader();
}

static void CreateExternalShaders(void) {
    tr.projectionShadowShader = R_FindShader("projectionShadow", LIGHTMAP_NONE, qtrue);
    tr.flareShader = R_FindShader("flareShader", LIGHTMAP_NONE, qtrue);

    if (!tr.flareShader->defaultShader) {
        int index;

        for (index = 0; index < tr.flareShader->numUnfoggedPasses; index++) {
            tr.flareShader->stages[index]->adjustColorsForFog = ACFF_NONE;
            tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
        }
    }

    tr.sunShader = R_FindShader("sun", LIGHTMAP_NONE, qtrue);

    tr.sunFlareShader = R_FindShader("gfx/2d/sunflare", LIGHTMAP_NONE, qtrue);

    if (tr.sunFlareShader->defaultShader) {
        image_t* image;

        if (!tr.flareShader->defaultShader && tr.flareShader->stages[0] && tr.flareShader->stages[0]->bundle[0].image[0])
            image = tr.flareShader->stages[0]->bundle[0].image[0];
        else
            image = tr.dlightImage;

        InitShader("gfx/2d/sunflare", LIGHTMAP_NONE);
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].stateBits = GLS_DEFAULT;
        tr.sunFlareShader = FinishShader();
    }
}

void R_InitShaders(void) {
    ri.Printf(PRINT_ALL, "Initializing Shaders\n");

    Com_Memset(hashTable, 0, sizeof(hashTable));

    CreateInternalShaders();

    ScanAndLoadShaderFiles();

    CreateExternalShaders();
}
