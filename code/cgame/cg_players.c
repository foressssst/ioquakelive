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
#include "cg_local.h"

char* cg_customSoundNames[MAX_CUSTOM_SOUNDS] = {
    "*death1.wav",
    "*death2.wav",
    "*death3.wav",
    "*jump1.wav",
    "*pain25_1.wav",
    "*pain50_1.wav",
    "*pain75_1.wav",
    "*pain100_1.wav",
    "*falling1.wav",
    "*gasp.wav",
    "*drown.wav",
    "*fall1.wav",
    "*taunt.wav"};

sfxHandle_t CG_CustomSound(int clientNum, const char* soundName) {
    clientInfo_t* ci;
    int i;

    if (soundName[0] != '*') {
        return trap_S_RegisterSound(soundName, qfalse);
    }

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = 0;
    }
    ci = &cgs.clientinfo[clientNum];

    for (i = 0; i < MAX_CUSTOM_SOUNDS && cg_customSoundNames[i]; i++) {
        if (!strcmp(soundName, cg_customSoundNames[i])) {
            return ci->sounds[i];
        }
    }

    CG_Error("Unknown custom sound: %s", soundName);
    return 0;
}

static qboolean CG_ParseAnimationFile(const char* filename, clientInfo_t* ci) {
    char *text_p, *prev;
    int len;
    int i;
    char* token;
    float fps;
    int skip;
    char text[20000];
    fileHandle_t f;
    animation_t* animations;

    animations = ci->animations;

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (len <= 0) {
        return qfalse;
    }
    if (len >= sizeof(text) - 1) {
        CG_Printf("File %s too long\n", filename);
        trap_FS_FCloseFile(f);
        return qfalse;
    }
    trap_FS_Read(text, len, f);
    text[len] = 0;
    trap_FS_FCloseFile(f);

    text_p = text;
    skip = 0;

    ci->footsteps = FOOTSTEP_NORMAL;
    VectorClear(ci->headOffset);
    ci->gender = GENDER_MALE;
    ci->fixedlegs = qfalse;
    ci->fixedtorso = qfalse;

    while (1) {
        prev = text_p;
        token = COM_Parse(&text_p);
        if (!token[0]) {
            break;
        }
        if (!Q_stricmp(token, "footsteps")) {
            token = COM_Parse(&text_p);
            if (!token[0]) {
                break;
            }
            if (!Q_stricmp(token, "default") || !Q_stricmp(token, "normal")) {
                ci->footsteps = FOOTSTEP_NORMAL;
            } else if (!Q_stricmp(token, "boot")) {
                ci->footsteps = FOOTSTEP_BOOT;
            } else if (!Q_stricmp(token, "flesh")) {
                ci->footsteps = FOOTSTEP_FLESH;
            } else if (!Q_stricmp(token, "mech")) {
                ci->footsteps = FOOTSTEP_MECH;
            } else if (!Q_stricmp(token, "energy")) {
                ci->footsteps = FOOTSTEP_ENERGY;
            } else {
                CG_Printf("Bad footsteps parm in %s: %s\n", filename, token);
            }
            continue;
        } else if (!Q_stricmp(token, "headoffset")) {
            for (i = 0; i < 3; i++) {
                token = COM_Parse(&text_p);
                if (!token[0]) {
                    break;
                }
                ci->headOffset[i] = atof(token);
            }
            continue;
        } else if (!Q_stricmp(token, "sex")) {
            token = COM_Parse(&text_p);
            if (!token[0]) {
                break;
            }
            if (token[0] == 'f' || token[0] == 'F') {
                ci->gender = GENDER_FEMALE;
            } else if (token[0] == 'n' || token[0] == 'N') {
                ci->gender = GENDER_NEUTER;
            } else {
                ci->gender = GENDER_MALE;
            }
            continue;
        } else if (!Q_stricmp(token, "fixedlegs")) {
            ci->fixedlegs = qtrue;
            continue;
        } else if (!Q_stricmp(token, "fixedtorso")) {
            ci->fixedtorso = qtrue;
            continue;
        }

        if (token[0] >= '0' && token[0] <= '9') {
            text_p = prev;
            break;
        }
        Com_Printf("unknown token '%s' in %s\n", token, filename);
    }

    for (i = 0; i < MAX_ANIMATIONS; i++) {
        token = COM_Parse(&text_p);
        if (!token[0]) {
            if (i >= TORSO_GETFLAG && i <= TORSO_NEGATIVE) {
                animations[i].firstFrame = animations[TORSO_GESTURE].firstFrame;
                animations[i].frameLerp = animations[TORSO_GESTURE].frameLerp;
                animations[i].initialLerp = animations[TORSO_GESTURE].initialLerp;
                animations[i].loopFrames = animations[TORSO_GESTURE].loopFrames;
                animations[i].numFrames = animations[TORSO_GESTURE].numFrames;
                animations[i].reversed = qfalse;
                animations[i].flipflop = qfalse;
                continue;
            }
            break;
        }
        animations[i].firstFrame = atoi(token);
        if (i == LEGS_WALKCR) {
            skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame;
        }
        if (i >= LEGS_WALKCR && i < TORSO_GETFLAG) {
            animations[i].firstFrame -= skip;
        }

        token = COM_Parse(&text_p);
        if (!token[0]) {
            break;
        }
        animations[i].numFrames = atoi(token);

        animations[i].reversed = qfalse;
        animations[i].flipflop = qfalse;
        if (animations[i].numFrames < 0) {
            animations[i].numFrames = -animations[i].numFrames;
            animations[i].reversed = qtrue;
        }

        token = COM_Parse(&text_p);
        if (!token[0]) {
            break;
        }
        animations[i].loopFrames = atoi(token);

        token = COM_Parse(&text_p);
        if (!token[0]) {
            break;
        }
        fps = atof(token);
        if (fps == 0) {
            fps = 1;
        }
        animations[i].frameLerp = 1000 / fps;
        animations[i].initialLerp = 1000 / fps;
    }

    if (i != MAX_ANIMATIONS) {
        CG_Printf("Error parsing animation file: %s\n", filename);
        return qfalse;
    }

    memcpy(&animations[LEGS_BACKCR], &animations[LEGS_WALKCR], sizeof(animation_t));
    animations[LEGS_BACKCR].reversed = qtrue;
    memcpy(&animations[LEGS_BACKWALK], &animations[LEGS_WALK], sizeof(animation_t));
    animations[LEGS_BACKWALK].reversed = qtrue;
    animations[FLAG_RUN].firstFrame = 0;
    animations[FLAG_RUN].numFrames = 16;
    animations[FLAG_RUN].loopFrames = 16;
    animations[FLAG_RUN].frameLerp = 1000 / 15;
    animations[FLAG_RUN].initialLerp = 1000 / 15;
    animations[FLAG_RUN].reversed = qfalse;
    animations[FLAG_STAND].firstFrame = 16;
    animations[FLAG_STAND].numFrames = 5;
    animations[FLAG_STAND].loopFrames = 0;
    animations[FLAG_STAND].frameLerp = 1000 / 20;
    animations[FLAG_STAND].initialLerp = 1000 / 20;
    animations[FLAG_STAND].reversed = qfalse;
    animations[FLAG_STAND2RUN].firstFrame = 16;
    animations[FLAG_STAND2RUN].numFrames = 5;
    animations[FLAG_STAND2RUN].loopFrames = 1;
    animations[FLAG_STAND2RUN].frameLerp = 1000 / 15;
    animations[FLAG_STAND2RUN].initialLerp = 1000 / 15;
    animations[FLAG_STAND2RUN].reversed = qtrue;
    return qtrue;
}

static qboolean CG_RegisterClientSkin(clientInfo_t* ci, const char* modelName, const char* skinName, const char* headModelName, const char* headSkinName) {
    char filename[MAX_QPATH];

    Com_sprintf(filename, sizeof(filename), "models/players/%s/lower_%s.skin", modelName, skinName);
    ci->legsSkin = trap_R_RegisterSkin(filename);
    if (!ci->legsSkin) {
        Com_Printf("Leg skin load failure: %s\n", filename);
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/upper_%s.skin", modelName, skinName);
    ci->torsoSkin = trap_R_RegisterSkin(filename);
    if (!ci->torsoSkin) {
        Com_Printf("Torso skin load failure: %s\n", filename);
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/head_%s.skin", headModelName, headSkinName);
    ci->headSkin = trap_R_RegisterSkin(filename);
    if (!ci->headSkin) {
        Com_Printf("Head skin load failure: %s\n", filename);
    }

    if (!ci->legsSkin || !ci->torsoSkin || !ci->headSkin) {
        return qfalse;
    }
    return qtrue;
}

static void CG_CalcModelScale(clientInfo_t* ci) {
    vec3_t legsMins, legsMaxs;
    vec3_t torsoMins, torsoMaxs;
    vec3_t headMins, headMaxs;
    orientation_t tag;
    float totalHeight;

    trap_R_ModelBounds(ci->legsModel, legsMins, legsMaxs);
    trap_R_ModelBounds(ci->torsoModel, torsoMins, torsoMaxs);
    trap_R_ModelBounds(ci->headModel, headMins, headMaxs);

    trap_R_LerpTag(&tag, ci->legsModel, ci->animations[LEGS_IDLE].firstFrame,
                   ci->animations[LEGS_IDLE].firstFrame, 1.0f, "tag_torso");
    totalHeight = tag.origin[2] + 24.0f;

    trap_R_LerpTag(&tag, ci->torsoModel, ci->animations[TORSO_STAND].firstFrame,
                   ci->animations[TORSO_STAND].firstFrame, 1.0f, "tag_head");
    totalHeight += headMaxs[2] + tag.origin[2];

    ci->modelScale = (56.0f / totalHeight) * cgs.playerModelScale;
}

static qboolean CG_RegisterClientModelname(clientInfo_t* ci, const char* modelName, const char* skinName, const char* headModelName, const char* headSkinName) {
    char filename[MAX_QPATH];
    const char* headName;

    if (headModelName[0] == '\0') {
        headName = modelName;
    } else {
        headName = headModelName;
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/lower.md3", modelName);
    ci->legsModel = trap_R_RegisterModel(filename);
    if (!ci->legsModel) {
        Com_Printf("Failed to load (legs) model file %s\n", filename);
        return qfalse;
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/upper.md3", modelName);
    ci->torsoModel = trap_R_RegisterModel(filename);
    if (!ci->torsoModel) {
        Com_Printf("Failed to load (torso) model file %s\n", filename);
        return qfalse;
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/head.md3", headName);
    ci->headModel = trap_R_RegisterModel(filename);
    if (!ci->headModel) {
        Com_Printf("Failed to load (head) model file %s\n", filename);
        return qfalse;
    }

    if (!CG_RegisterClientSkin(ci, modelName, skinName, headName, headSkinName)) {
        Com_Printf("Failed to load skin %s/%s, head %s/%s\n", modelName, skinName, headName, headSkinName);
        return qfalse;
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/animation.cfg", modelName);
    if (!CG_ParseAnimationFile(filename, ci)) {
        Com_Printf("Failed to load animation file %s\n", filename);
        return qfalse;
    }

    Com_sprintf(filename, sizeof(filename), "models/players/%s/icon_%s.tga", ci->headModelName, ci->headSkinName);
    ci->modelIcon = trap_R_RegisterShaderNoMip(filename);
    if (!ci->modelIcon) {
        Com_Printf("Failed to load icon file: %s\n", filename);
        return qfalse;
    }

    if (!Q_stricmpn(modelName, "orbb", 4)) {
        ci->modelScale = 1.0f;
    } else {
        CG_CalcModelScale(ci);
    }

    return qtrue;
}

static const vec3_t cg_playerColors[26] = {
    {1.0f, 0.0f, 0.0f},       {1.0f, 0.2734375f, 0.0f}, {1.0f, 0.5f, 0.0f},       {1.0f, 0.734375f, 0.0f},
    {1.0f, 1.0f, 0.0f},       {0.734375f, 1.0f, 0.0f},  {0.5f, 1.0f, 0.0f},       {0.2734375f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},       {0.0f, 1.0f, 0.2734375f}, {0.0f, 1.0f, 0.5f},       {0.0f, 1.0f, 0.734375f},
    {0.0f, 1.0f, 1.0f},       {0.0f, 0.734375f, 1.0f},  {0.0f, 0.5f, 1.0f},       {0.0f, 0.2734375f, 1.0f},
    {0.0f, 0.0f, 1.0f},       {0.2734375f, 0.0f, 1.0f}, {0.5f, 0.0f, 1.0f},       {0.734375f, 0.0f, 1.0f},
    {1.0f, 0.0f, 1.0f},       {1.0f, 0.0f, 0.734375f},  {1.0f, 0.0f, 0.5f},       {1.0f, 0.0f, 0.2734375f},
    {1.0f, 1.0f, 1.0f},       {0.5f, 0.5f, 0.5f}};

static void CG_ColorFromString(const char* v, vec3_t color) {
    int index = atoi(v) - 1;

    if (index < 0 || index > 25) {
        index = 0;
    }
    VectorCopy(cg_playerColors[index], color);
}

static void CG_ResolveModelForClient(clientInfo_t* ci, int clientNum);

static void CG_LoadClientInfo(int clientNum, clientInfo_t* ci) {
    const char* teamSkin;
    int i, modelloaded;
    const char* s;
    orientation_t tag;

    modelloaded = qtrue;
    if (!CG_RegisterClientModelname(ci, ci->modelName, ci->skinName, ci->forcedHeadModel, ci->forcedHeadSkin)) {
        CG_ResolveModelForClient(ci, clientNum);
        if (trap_Cvar_VariableValue("com_build") != 0) {
            CG_Error("CG_RegisterClientModel( %s, %s, %s, %s ) failed", ci->modelName, ci->skinName,
                     ci->forcedHeadModel, ci->forcedHeadSkin);
        }

        if (ci->team == TEAM_RED) {
            teamSkin = "red";
        } else if (ci->team == TEAM_BLUE) {
            teamSkin = "blue";
        } else {
            teamSkin = "default";
        }

        if (!CG_RegisterClientModelname(ci, ci->modelName, teamSkin, ci->forcedHeadModel, teamSkin)) {
            if (trap_Cvar_VariableValue("com_build") != 0) {
                if (ci->team == TEAM_RED) {
                    CG_Error("CG_RegisterClientModel( %s, red, %s, red ) failed to register", ci->modelName,
                             ci->forcedHeadModel);
                } else if (ci->team == TEAM_BLUE) {
                    CG_Error("CG_RegisterClientModel( %s, blue, %s, blue ) failed to register", ci->modelName,
                             ci->forcedHeadModel);
                } else {
                    CG_Error("CG_RegisterClientModel (%s/%s) failed to register", ci->modelName, "default");
                }
            }
            Q_strncpyz(ci->headModelName, "sarge", sizeof(ci->headModelName));
            Q_strncpyz(ci->headSkinName, teamSkin, sizeof(ci->headSkinName));
            if (!CG_RegisterClientModelname(ci, "sarge", teamSkin, "sarge", teamSkin)) {
                if (trap_Cvar_VariableValue("com_build") != 0) {
                    if (ci->team == TEAM_RED) {
                        CG_Error("CG_RegisterClientModel (%s/red) failed to register", "sarge");
                    } else if (ci->team == TEAM_BLUE) {
                        CG_Error("CG_RegisterClientModel (%s/blue) failed to register", "sarge");
                    } else {
                        CG_Error("CG_RegisterClientModel (%s/%s) failed to register", "sarge", "default");
                    }
                }
                modelloaded = qfalse;
            } else if (clientNum == cg.clientNum) {
                trap_Cvar_Set("model", "sarge");
            }
        }
    }

    ci->newAnims = qfalse;
    if (ci->torsoModel && trap_R_LerpTag(&tag, ci->torsoModel, 0, 0, 1, "tag_flag")) {
        ci->newAnims = qtrue;
    }

    for (i = 0; i < MAX_CUSTOM_SOUNDS; i++) {
        s = cg_customSoundNames[i];
        if (!s) {
            break;
        }
        ci->sounds[i] = 0;
        if (modelloaded) {
            ci->sounds[i] = trap_S_RegisterSound(va("sound/player/%s/%s", ci->modelName, s + 1), qfalse);
        }
        if (!ci->sounds[i]) {
            ci->sounds[i] = trap_S_RegisterSound(va("sound/player/%s/%s", "sarge", s + 1), qfalse);
        }
    }

    ci->deferred = qfalse;

    for (i = 0; i < MAX_GENTITIES; i++) {
        if (cg_entities[i].currentState.clientNum == clientNum && cg_entities[i].currentState.eType == ET_PLAYER) {
            CG_ResetPlayerEntity(&cg_entities[i]);
        }
    }
}

static void CG_CopyClientInfoModel(clientInfo_t* from, clientInfo_t* to) {
    VectorCopy(from->headOffset, to->headOffset);
    to->footsteps = from->footsteps;
    to->gender = from->gender;

    to->legsModel = from->legsModel;
    to->legsSkin = from->legsSkin;
    to->torsoModel = from->torsoModel;
    to->torsoSkin = from->torsoSkin;
    to->headModel = from->headModel;
    to->headSkin = from->headSkin;
    to->modelIcon = from->modelIcon;

    to->modelScale = from->modelScale;

    to->newAnims = from->newAnims;

    memcpy(to->animations, from->animations, sizeof(to->animations));
    memcpy(to->sounds, from->sounds, sizeof(to->sounds));
}

static qboolean CG_ScanForExistingClientInfo(clientInfo_t* ci) {
    int i;
    clientInfo_t* match;
    char filename[MAX_QPATH];

    for (i = 0; i < cgs.maxclients; i++) {
        match = &cgs.clientinfo[i];
        if (!match->infoValid || match->deferred) {
            continue;
        }
        if (Q_stricmp(ci->modelName, match->modelName) || Q_stricmp(ci->skinName, match->skinName) ||
            Q_stricmp(ci->forcedHeadModel, match->forcedHeadModel) ||
            Q_stricmp(ci->forcedHeadSkin, match->forcedHeadSkin)) {
            continue;
        }
        if (cgs.gametype >= GT_TEAM && ci->team != match->team) {
            continue;
        }

        ci->deferred = qfalse;
        CG_CopyClientInfoModel(match, ci);
        Com_sprintf(filename, sizeof(filename), "models/players/%s/icon_%s.tga", ci->headModelName, ci->headSkinName);
        ci->modelIcon = trap_R_RegisterShaderNoMip(filename);
        return qtrue;
    }

    return qfalse;
}

static void CG_SetDeferredClientInfo(int clientNum, clientInfo_t* ci) {
    int i;
    clientInfo_t* match;

    if (cgs.gametype >= GT_TEAM) {
        for (i = 0; i < cgs.maxclients; i++) {
            match = &cgs.clientinfo[i];
            if (!match->infoValid || match->deferred) {
                continue;
            }
            if (Q_stricmp(ci->skinName, match->skinName) || ci->team != match->team) {
                continue;
            }
            ci->deferred = qtrue;
            CG_CopyClientInfoModel(match, ci);
            return;
        }
        CG_LoadClientInfo(clientNum, ci);
        return;
    }

    for (i = 0; i < cgs.maxclients; i++) {
        match = &cgs.clientinfo[i];
        if (!match->infoValid) {
            continue;
        }
        ci->deferred = qtrue;
        CG_CopyClientInfoModel(match, ci);
        return;
    }
}

qboolean CG_ShouldForceTeamSkin(int viewerTeam, int playerTeam) {
    qboolean teammate = qtrue;

    if (cgs.gametype >= GT_TEAM && (cg.predictedPlayerState.pm_type & 2) &&
        (cg.predictedPlayerState.pm_flags & PMF_FOLLOW) && viewerTeam != TEAM_RED && viewerTeam != TEAM_BLUE) {
        if (playerTeam == TEAM_BLUE) {
            teammate = qfalse;
        }
    } else if (viewerTeam != playerTeam || viewerTeam == TEAM_FREE) {
        teammate = qfalse;
    }

    if (playerTeam == TEAM_RED) {
        if (cg_forceRedTeamModel.string[0]) {
            return qtrue;
        }
    } else if (playerTeam == TEAM_BLUE) {
        if (cg_forceBlueTeamModel.string[0]) {
            return qfalse;
        }
    }
    return teammate;
}

void CG_ResolveBodySkinName(clientInfo_t* ci, qboolean modelForced, qboolean skinForced) {
    char* skin;

    if (!skinForced) {
        skin = strchr(ci->modelName, '/');
        if (skin) {
            *skin = '\0';
            skin++;
        } else if (cgs.gametype < GT_TEAM) {
            skin = "default";
        }
    } else {
        skin = ci->skinName;
    }

    if ((!modelForced || !skin) && !skinForced) {
        if (cgs.gametype >= GT_TEAM) {
            if (ci->team == TEAM_RED) {
                skin = "red";
            } else if (ci->team == TEAM_BLUE) {
                skin = "blue";
            } else {
                skin = "default";
            }
        }
    } else if (skinForced) {
        if (skin && !Q_stricmp(skin, "sport") && cgs.gametype >= GT_TEAM) {
            if (ci->team == TEAM_RED) {
                skin = "sport_red";
            } else if (ci->team == TEAM_BLUE) {
                skin = "sport_blue";
            } else {
                skin = "sport";
            }
        }
    }

    if (!skin) {
        skin = "default";
    }
    if (skin != ci->skinName) {
        Q_strncpyz(ci->skinName, skin, sizeof(ci->skinName));
    }
}

void CG_ResolveHeadSkinName(clientInfo_t* ci, qboolean modelForced, qboolean skinForced) {
    char* skin;

    if (!skinForced) {
        skin = strchr(ci->forcedHeadModel, '/');
        if (skin) {
            *skin = '\0';
            skin++;
        }
    } else {
        skin = ci->forcedHeadSkin;
    }

    if (!modelForced && !skinForced && cgs.gametype < GT_TEAM) {
        return;
    }
    if (!skin && cgs.gametype < GT_TEAM) {
        skin = "default";
    }

    if ((!modelForced || !skin) && !skinForced) {
        if (ci->team == TEAM_RED) {
            skin = "red";
        } else if (ci->team == TEAM_BLUE) {
            skin = "blue";
        } else {
            skin = "default";
        }
    } else if (skinForced) {
        if (skin && !Q_stricmp(skin, "sport") && cgs.gametype >= GT_TEAM) {
            if (ci->team == TEAM_RED) {
                skin = "sport_red";
            } else if (ci->team == TEAM_BLUE) {
                skin = "sport_blue";
            } else {
                skin = "sport";
            }
        }
    }

    if (!skin) {
        skin = "default";
    }
    if (skin != ci->forcedHeadSkin) {
        Q_strncpyz(ci->forcedHeadSkin, skin, sizeof(ci->forcedHeadSkin));
    }
}

static void CG_ResolveModelForClient(clientInfo_t* ci, int clientNum) {
    const char* forcedModel = "";
    const char* forcedSkin = "";
    qboolean modelForced = qfalse;
    qboolean skinForced;
    int viewerClient;
    int viewerTeam;
    char tmp[128];
    char* p;

    viewerClient = cg.clientNum;
    viewerTeam = cgs.clientinfo[cg.clientNum].team;

    if (!g_training.integer) {
        if (cg.predictedPlayerState.pm_flags & PMF_FOLLOW) {
            viewerClient = cg.predictedPlayerState.clientNum;
            viewerTeam = cgs.clientinfo[viewerClient].team;
        }

        if (!cgs.playermodelOverride[0]) {
            if (cgs.gametype < GT_TEAM || !(cg.predictedPlayerState.pm_type & 2) ||
                (cg.predictedPlayerState.pm_flags & PMF_FOLLOW) || viewerTeam == TEAM_RED || viewerTeam == TEAM_BLUE) {
                if (cgs.gametype == GT_DUEL) {
                    if (viewerClient != clientNum) {
                        forcedModel = cg_forceEnemyModel.string;
                        forcedSkin = cg_forceEnemySkin.string;
                    }
                } else if (ci->team != viewerTeam || viewerTeam == TEAM_FREE) {
                    forcedModel = cg_forceEnemyModel.string;
                    forcedSkin = cg_forceEnemySkin.string;
                } else {
                    forcedModel = cg_forceTeamModel.string;
                    forcedSkin = cg_forceTeamSkin.string;
                }
            }

            if (ci->team == TEAM_RED) {
                if (cg_forceRedTeamModel.string[0]) {
                    forcedModel = cg_forceRedTeamModel.string;
                }
            } else if (ci->team == TEAM_BLUE && cg_forceBlueTeamModel.string[0]) {
                forcedModel = cg_forceBlueTeamModel.string;
            }
        }

        if (cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000) && ci->team == TEAM_RED) {
            forcedModel = "bones";
            forcedSkin = "bones";
        }
    }

    if (forcedModel[0]) {
        Q_strncpyz(ci->modelName, forcedModel, sizeof(ci->modelName));
        modelForced = qtrue;
    } else if (cgs.gametype >= GT_TEAM) {
        p = strchr(ci->modelName, '/');
        if (p) {
            *p = '\0';
        }
    }

    skinForced = qfalse;
    if (forcedSkin[0]) {
        Q_strncpyz(tmp, forcedModel[0] ? forcedModel : ci->modelName, sizeof(tmp));
        p = strtok(tmp, "/");
        Q_strncpyz(ci->modelName, p, sizeof(ci->modelName));
        Q_strncpyz(ci->skinName, forcedSkin, sizeof(ci->skinName));
        skinForced = qtrue;
    }

    CG_ResolveBodySkinName(ci, modelForced, skinForced);

    if (!cgs.playerheadmodelOverride[0]) {
        if (forcedModel[0]) {
            Q_strncpyz(ci->forcedHeadModel, forcedModel, sizeof(ci->forcedHeadModel));
        }
        if (forcedSkin[0]) {
            if (forcedModel[0]) {
                Q_strncpyz(tmp, forcedModel, sizeof(tmp));
                p = strtok(tmp, "/");
                Q_strncpyz(ci->forcedHeadModel, p, sizeof(ci->forcedHeadModel));
            }
            Q_strncpyz(ci->forcedHeadSkin, forcedSkin, sizeof(ci->forcedHeadSkin));
        }
        CG_ResolveHeadSkinName(ci, modelForced, skinForced);
    }
}

void CG_UpdateAllModelScales(void) {
    int i;

    for (i = 0; i < cgs.maxclients; i++) {
        clientInfo_t* ci = &cgs.clientinfo[i];
        if (!ci->infoValid) {
            continue;
        }
        if (!Q_stricmpn(ci->modelName, "orbb", 4)) {
            ci->modelScale = 1.0f;
        } else {
            CG_CalcModelScale(ci);
        }
    }
}

void CG_NewClientInfo(int clientNum) {
    clientInfo_t* ci;
    clientInfo_t newInfo;
    const char* configstring;
    const char* v;
    char* p;
    qboolean lowMemory;

    ci = &cgs.clientinfo[clientNum];

    configstring = CG_ConfigString(clientNum + CS_PLAYERS);
    if (!configstring[0]) {
        memset(ci, 0, sizeof(*ci));
        return;
    }

    memset(&newInfo, 0, sizeof(newInfo));

    v = Info_ValueForKey(configstring, "cn");
    Q_strncpyz(newInfo.clanTag, "^7", sizeof(newInfo.clanTag));
    Q_strcat(newInfo.clanTag, sizeof(newInfo.clanTag), v);
    Q_strcat(newInfo.clanTag, sizeof(newInfo.clanTag), "^7");
    if (v[0]) {
        Q_strncpyz(newInfo.name, v, sizeof(newInfo.name));
        Q_strcat(newInfo.name, sizeof(newInfo.name), "^7 ");
    }
    Q_strcat(newInfo.name, sizeof(newInfo.name), Info_ValueForKey(configstring, "n"));

    Q_strncpyz(newInfo.clanName, Info_ValueForKey(configstring, "xcn"), sizeof(newInfo.clanName));
    Q_strncpyz(newInfo.country, Info_ValueForKey(configstring, "c"), sizeof(newInfo.country));

    CG_ColorFromString(Info_ValueForKey(configstring, "c1"), newInfo.color1);
    CG_ColorFromString(Info_ValueForKey(configstring, "c2"), newInfo.color2);

    newInfo.c1RGBA[0] = 255 * newInfo.color1[0];
    newInfo.c1RGBA[1] = 255 * newInfo.color1[1];
    newInfo.c1RGBA[2] = 255 * newInfo.color1[2];
    newInfo.c1RGBA[3] = 255;
    newInfo.c2RGBA[0] = 255 * newInfo.color2[0];
    newInfo.c2RGBA[1] = 255 * newInfo.color2[1];
    newInfo.c2RGBA[2] = 255 * newInfo.color2[2];
    newInfo.c2RGBA[3] = 255;

    v = Info_ValueForKey(configstring, "skill");
    newInfo.botSkill = atoi(v);
    newInfo.botSkillValue = atof(v);
    newInfo.handicap = atoi(Info_ValueForKey(configstring, "hc"));
    newInfo.wins = atoi(Info_ValueForKey(configstring, "w"));
    newInfo.losses = atoi(Info_ValueForKey(configstring, "l"));
    newInfo.team = atoi(Info_ValueForKey(configstring, "t"));
    newInfo.teamTask = atoi(Info_ValueForKey(configstring, "tt"));
    newInfo.teamLeader = atoi(Info_ValueForKey(configstring, "tl"));
    newInfo.ready = (Info_ValueForKey(configstring, "rp")[0] != '0') ? qtrue : qfalse;
    newInfo.privileges = atoi(Info_ValueForKey(configstring, "p"));
    newInfo.specOnly = atoi(Info_ValueForKey(configstring, "so"));
    newInfo.playQueue = atoi(Info_ValueForKey(configstring, "pq"));

    Q_strncpyz(newInfo.modelName, Info_ValueForKey(configstring, "model"), sizeof(newInfo.modelName));

    Q_strncpyz(newInfo.headModelName, newInfo.modelName, sizeof(newInfo.headModelName));
    p = strchr(newInfo.headModelName, '/');

    for (v = Info_ValueForKey(configstring, "st"); *v >= '0' && *v <= '9'; v++) {
        newInfo.steamId = newInfo.steamId * 10 + (uint64_t)(*v - '0');
    }

    if (cgs.gametype >= GT_TEAM) {
        if (p) {
            *p = '\0';
        }
        v = (newInfo.team == TEAM_RED) ? "red" : "blue";
    } else if (p) {
        *p = '\0';
        v = p + 1;
    } else {
        v = "default";
    }
    Q_strncpyz(newInfo.headSkinName, v, sizeof(newInfo.headSkinName));

    if (cgs.allowCustomHeadmodels) {
        Q_strncpyz(newInfo.forcedHeadModel, Info_ValueForKey(configstring, "hmodel"), sizeof(newInfo.forcedHeadModel));
    } else {
        Q_strncpyz(newInfo.forcedHeadModel, newInfo.modelName, sizeof(newInfo.forcedHeadModel));
    }
    p = strchr(newInfo.forcedHeadModel, '/');
    if (p) {
        *p = '\0';
        Q_strncpyz(newInfo.forcedHeadSkin, p + 1, sizeof(newInfo.forcedHeadSkin));
    } else if (cgs.gametype >= GT_TEAM) {
        Q_strncpyz(newInfo.forcedHeadSkin, (newInfo.team == TEAM_RED) ? "red" : "blue", sizeof(newInfo.forcedHeadSkin));
    } else {
        Q_strncpyz(newInfo.forcedHeadSkin, "default", sizeof(newInfo.forcedHeadSkin));
    }

    if (clientNum != cg.clientNum ||
        (cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000) && newInfo.team == TEAM_RED)) {
        CG_ResolveModelForClient(&newInfo, clientNum);
    } else {
        p = strchr(newInfo.modelName, '/');
        if (p) {
            *p = '\0';
            v = p + 1;
        } else {
            v = "default";
        }
        if (cgs.gametype >= GT_TEAM) {
            if (newInfo.team == TEAM_RED) {
                v = "red";
            } else if (newInfo.team == TEAM_BLUE) {
                v = "blue";
            } else {
                v = "default";
            }
            Q_strncpyz(newInfo.forcedHeadSkin, v, sizeof(newInfo.forcedHeadSkin));
        }
        Q_strncpyz(newInfo.skinName, v, sizeof(newInfo.skinName));

        CG_SetEndScoreboardMenu();
    }

    if (!CG_ScanForExistingClientInfo(&newInfo)) {
        lowMemory = trap_MemoryRemaining() < 4000000;

        if (lowMemory || (cg_deferPlayers.integer && !cg.loading)) {
            CG_SetDeferredClientInfo(clientNum, &newInfo);
            if (lowMemory) {
                CG_Printf("Memory is low.  Using deferred model.\n");
                newInfo.deferred = qfalse;
            }
        } else {
            CG_LoadClientInfo(clientNum, &newInfo);
        }
    }

    newInfo.infoValid = qtrue;
    *ci = newInfo;

    if (clientNum == cg.clientNum) {
        CG_ReloadOtherClients();
        CG_LoadDeferredPlayers();
    }
}

void CG_ReloadOtherClients(void) {
    int i;
    clientInfo_t* ci;

    for (i = 0; i < cgs.maxclients; i++) {
        if (i == cg.clientNum) {
            continue;
        }
        CG_NewClientInfo(i);
        ci = &cgs.clientinfo[i];
        if (!ci->infoValid) {
            continue;
        }
        if (!Q_stricmpn(ci->modelName, "orbb", 4)) {
            ci->modelScale = 1.0f;
        } else {
            CG_CalcModelScale(ci);
        }
    }
}

void CG_LoadDeferredPlayers(void) {
    int i;
    clientInfo_t* ci;

    for (i = 0, ci = cgs.clientinfo; i < cgs.maxclients; i++, ci++) {
        if (ci->infoValid && ci->deferred) {
            if (trap_MemoryRemaining() < 4000000) {
                CG_Printf("Memory is low.  Using deferred model.\n");
                ci->deferred = qfalse;
                continue;
            }
            CG_LoadClientInfo(i, ci);
        }
    }
}

static void CG_SetLerpFrameAnimation(clientInfo_t* ci, lerpFrame_t* lf, int newAnimation) {
    animation_t* anim;

    lf->animationNumber = newAnimation;
    newAnimation &= ~ANIM_TOGGLEBIT;

    if (newAnimation < 0 || newAnimation >= MAX_TOTALANIMATIONS) {
        CG_Error("Bad animation number: %i", newAnimation);
    }

    anim = &ci->animations[newAnimation];

    lf->animation = anim;
    lf->animationTime = lf->frameTime + anim->initialLerp;

    if (cg_debugAnim.integer) {
        CG_Printf("Anim: %i\n", newAnimation);
    }
}

static void CG_RunLerpFrame(clientInfo_t* ci, lerpFrame_t* lf, int newAnimation, float speedScale) {
    int f, numFrames;
    animation_t* anim;

    if (cg_animSpeed.integer == 0) {
        lf->oldFrame = lf->frame = lf->backlerp = 0;
        return;
    }

    if (newAnimation != lf->animationNumber || !lf->animation) {
        CG_SetLerpFrameAnimation(ci, lf, newAnimation);
    }

    if (cg.time >= lf->frameTime) {
        lf->oldFrame = lf->frame;
        lf->oldFrameTime = lf->frameTime;

        anim = lf->animation;
        if (!anim->frameLerp) {
            return;
        }
        if (cg.time < lf->animationTime) {
            lf->frameTime = lf->animationTime;
        } else {
            lf->frameTime = lf->oldFrameTime + anim->frameLerp;
        }
        f = (lf->frameTime - lf->animationTime) / anim->frameLerp;
        f *= speedScale;

        numFrames = anim->numFrames;
        if (anim->flipflop) {
            numFrames *= 2;
        }
        if (f >= numFrames) {
            f -= numFrames;
            if (anim->loopFrames) {
                f %= anim->loopFrames;
                f += anim->numFrames - anim->loopFrames;
            } else {
                f = numFrames - 1;
                lf->frameTime = cg.time;
            }
        }
        if (anim->reversed) {
            lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
        } else if (anim->flipflop && f >= anim->numFrames) {
            lf->frame = anim->firstFrame + anim->numFrames - 1 - (f % anim->numFrames);
        } else {
            lf->frame = anim->firstFrame + f;
        }
        if (cg.time > lf->frameTime) {
            lf->frameTime = cg.time;
            if (cg_debugAnim.integer) {
                CG_Printf("Clamp lf->frameTime\n");
            }
        }
    }

    if (lf->frameTime > cg.time + 200) {
        lf->frameTime = cg.time;
    }

    if (lf->oldFrameTime > cg.time) {
        lf->oldFrameTime = cg.time;
    }
    if (lf->frameTime == lf->oldFrameTime) {
        lf->backlerp = 0;
    } else {
        lf->backlerp = 1.0 - (float)(cg.time - lf->oldFrameTime) / (lf->frameTime - lf->oldFrameTime);
    }
}

static void CG_ClearLerpFrame(clientInfo_t* ci, lerpFrame_t* lf, int animationNumber) {
    lf->frameTime = lf->oldFrameTime = cg.time;
    CG_SetLerpFrameAnimation(ci, lf, animationNumber);
    lf->oldFrame = lf->frame = lf->animation->firstFrame;
}

static void CG_PlayerAnimation(centity_t* cent, int* legsOld, int* legs, float* legsBackLerp, int* torsoOld, int* torso, float* torsoBackLerp) {
    clientInfo_t* ci;
    int clientNum;
    float speedScale;

    clientNum = cent->currentState.clientNum;

    if (cg_noPlayerAnims.integer) {
        *legsOld = *legs = *torsoOld = *torso = 0;
        return;
    }

    if (cent->currentState.powerups & (1 << PW_HASTE)) {
        speedScale = 1.5;
    } else {
        speedScale = 1;
    }

    ci = &cgs.clientinfo[clientNum];

    if (cent->pe.legs.yawing && (cent->currentState.legsAnim & ~ANIM_TOGGLEBIT) == LEGS_IDLE) {
        CG_RunLerpFrame(ci, &cent->pe.legs, LEGS_TURN, speedScale);
    } else {
        CG_RunLerpFrame(ci, &cent->pe.legs, cent->currentState.legsAnim, speedScale);
    }

    *legsOld = cent->pe.legs.oldFrame;
    *legs = cent->pe.legs.frame;
    *legsBackLerp = cent->pe.legs.backlerp;

    CG_RunLerpFrame(ci, &cent->pe.torso, cent->currentState.torsoAnim, speedScale);

    *torsoOld = cent->pe.torso.oldFrame;
    *torso = cent->pe.torso.frame;
    *torsoBackLerp = cent->pe.torso.backlerp;
}

static void CG_SwingAngles(float destination, float swingTolerance, float clampTolerance, float speed, float* angle, qboolean* swinging) {
    float swing;
    float move;
    float scale;

    if (!*swinging) {
        swing = AngleSubtract(*angle, destination);
        if (swing > swingTolerance || swing < -swingTolerance) {
            *swinging = qtrue;
        }
    }

    if (!*swinging) {
        return;
    }

    swing = AngleSubtract(destination, *angle);
    scale = fabs(swing);
    if (scale < swingTolerance * 0.5) {
        scale = 0.5;
    } else if (scale < swingTolerance) {
        scale = 1.0;
    } else {
        scale = 2.0;
    }

    if (swing >= 0) {
        move = cg.frametime * scale * speed;
        if (move >= swing) {
            move = swing;
            *swinging = qfalse;
        }
        *angle = AngleMod(*angle + move);
    } else if (swing < 0) {
        move = cg.frametime * scale * -speed;
        if (move <= swing) {
            move = swing;
            *swinging = qfalse;
        }
        *angle = AngleMod(*angle + move);
    }

    swing = AngleSubtract(destination, *angle);
    if (swing > clampTolerance) {
        *angle = AngleMod(destination - (clampTolerance - 1));
    } else if (swing < -clampTolerance) {
        *angle = AngleMod(destination + (clampTolerance - 1));
    }
}

static void CG_AddPainTwitch(centity_t* cent, vec3_t torsoAngles) {
    int t;
    float f;

    t = cg.time - cent->pe.painTime;
    if (t >= PAIN_TWITCH_TIME) {
        return;
    }

    f = 1.0 - (float)t / PAIN_TWITCH_TIME;

    if (cent->pe.painDirection) {
        torsoAngles[ROLL] += 20 * f;
    } else {
        torsoAngles[ROLL] -= 20 * f;
    }
}

static void CG_PlayerAngles(centity_t* cent, vec3_t legs[3], vec3_t torso[3], vec3_t head[3]) {
    vec3_t legsAngles, torsoAngles, headAngles;
    float dest;
    static int movementOffsets[8] = {0, 22, 45, -22, 0, 22, -45, -22};
    vec3_t velocity;
    float speed;
    int dir, clientNum;
    clientInfo_t* ci;

    VectorCopy(cent->lerpAngles, headAngles);
    headAngles[YAW] = AngleMod(headAngles[YAW]);
    VectorClear(legsAngles);
    VectorClear(torsoAngles);

    if ((cent->currentState.legsAnim & ~ANIM_TOGGLEBIT) != LEGS_IDLE || ((cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT) != TORSO_STAND && (cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT) != TORSO_STAND2)) {
        cent->pe.torso.yawing = qtrue;
        cent->pe.torso.pitching = qtrue;
        cent->pe.legs.yawing = qtrue;
    }

    if (cent->currentState.eFlags & EF_DEAD) {
        dir = 0;
    } else {
        dir = cent->currentState.angles2[YAW];
        if (dir < 0 || dir > 7) {
            CG_Error("Bad player movement angle");
        }
    }
    legsAngles[YAW] = headAngles[YAW] + movementOffsets[dir];
    torsoAngles[YAW] = headAngles[YAW] + 0.25 * movementOffsets[dir];

    CG_SwingAngles(torsoAngles[YAW], 25, 90, cg_swingSpeed.value, &cent->pe.torso.yawAngle, &cent->pe.torso.yawing);
    CG_SwingAngles(legsAngles[YAW], 40, 90, cg_swingSpeed.value, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing);

    torsoAngles[YAW] = cent->pe.torso.yawAngle;
    legsAngles[YAW] = cent->pe.legs.yawAngle;

    if (headAngles[PITCH] > 180) {
        dest = (-360 + headAngles[PITCH]) * 0.75f;
    } else {
        dest = headAngles[PITCH] * 0.75f;
    }
    CG_SwingAngles(dest, 15, 30, 0.1f, &cent->pe.torso.pitchAngle, &cent->pe.torso.pitching);
    torsoAngles[PITCH] = cent->pe.torso.pitchAngle;

    clientNum = cent->currentState.clientNum;
    if (clientNum >= 0 && clientNum < MAX_CLIENTS) {
        ci = &cgs.clientinfo[clientNum];
        if (ci->fixedtorso) {
            torsoAngles[PITCH] = 0.0f;
        }
    }

    VectorCopy(cent->currentState.pos.trDelta, velocity);
    speed = VectorNormalize(velocity);
    if (speed) {
        vec3_t axis[3];
        float side;

        speed *= 0.05f;

        AnglesToAxis(legsAngles, axis);
        side = speed * DotProduct(velocity, axis[1]);
        legsAngles[ROLL] -= side;

        side = speed * DotProduct(velocity, axis[0]);
        legsAngles[PITCH] += side;
    }

    clientNum = cent->currentState.clientNum;
    if (clientNum >= 0 && clientNum < MAX_CLIENTS) {
        ci = &cgs.clientinfo[clientNum];
        if (ci->fixedlegs) {
            legsAngles[YAW] = torsoAngles[YAW];
            legsAngles[PITCH] = 0.0f;
            legsAngles[ROLL] = 0.0f;
        }
    }

    CG_AddPainTwitch(cent, torsoAngles);

    AnglesSubtract(headAngles, torsoAngles, headAngles);
    AnglesSubtract(torsoAngles, legsAngles, torsoAngles);
    AnglesToAxis(legsAngles, legs);
    AnglesToAxis(torsoAngles, torso);
    AnglesToAxis(headAngles, head);
}

static void CG_HasteTrail(centity_t* cent) {
    localEntity_t* smoke;
    vec3_t origin;
    int anim;

    if (cent->trailTime > cg.time) {
        return;
    }
    anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;
    if (anim != LEGS_RUN && anim != LEGS_BACK) {
        return;
    }

    cent->trailTime += 100;
    if (cent->trailTime < cg.time) {
        cent->trailTime = cg.time;
    }

    if (cg_smokeRadius_haste.value == 0.0f) {
        return;
    }

    VectorCopy(cent->lerpOrigin, origin);
    origin[2] -= 16;

    smoke = CG_SmokePuff(origin, vec3_origin,
                         cg_smokeRadius_haste.value,
                         1, 1, 1, 1,
                         500,
                         cg.time,
                         0,
                         0,
                         cgs.media.hastePuffShader);

    smoke->leType = LE_SCALE_FADE;
}

static void CG_FlightTrail(centity_t* cent) {
    localEntity_t* smoke;
    vec3_t origin;

    if (cent->trailTime > cg.time) {
        return;
    }

    cent->trailTime += 100;
    if (cent->trailTime < cg.time) {
        cent->trailTime = cg.time;
    }

    if (cg_smokeRadius_flight.value == 0.0f) {
        return;
    }

    VectorCopy(cent->lerpOrigin, origin);
    if (cg.snap && cg.snap->ps.clientNum == cent->currentState.number && !cg.renderingThirdPerson) {
        origin[2] -= 16;
    } else {
        origin[2] += 24;
    }

    smoke = CG_SmokePuff(origin, vec3_origin,
                         cg_smokeRadius_flight.value,
                         1, 1, 1, 1,
                         1000,
                         cg.time,
                         0,
                         0,
                         cgs.media.hastePuffShader);

    smoke->leType = LE_SCALE_FADE;
}

static void CG_DustTrail(centity_t* cent) {
    int anim;
    vec3_t end, vel;
    trace_t tr;

    if (!cg_enableDust.integer)
        return;

    if (cent->dustTrailTime > cg.time) {
        return;
    }

    anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;
    if (anim != LEGS_LANDB && anim != LEGS_LAND) {
        return;
    }

    cent->dustTrailTime += 40;
    if (cent->dustTrailTime < cg.time) {
        cent->dustTrailTime = cg.time;
    }

    VectorCopy(cent->currentState.pos.trBase, end);
    end[2] -= 64;
    CG_Trace(&tr, cent->currentState.pos.trBase, NULL, NULL, end, cent->currentState.number, MASK_PLAYERSOLID);

    if (!(tr.surfaceFlags & SURF_DUST))
        return;

    if (cg_smokeRadius_dust.value == 0.0f) {
        return;
    }

    VectorCopy(cent->currentState.pos.trBase, end);
    end[2] -= 16;

    VectorSet(vel, 0, 0, -30);
    CG_SmokePuff(end, vel,
                 cg_smokeRadius_dust.value,
                 .8f, .8f, 0.7f, 0.33f,
                 500,
                 cg.time,
                 0,
                 0,
                 cgs.media.dustPuffShader);
}

static void CG_TrailItem(centity_t* cent, qhandle_t hModel) {
    refEntity_t ent;
    vec3_t angles;
    vec3_t axis[3];

    VectorCopy(cent->lerpAngles, angles);
    angles[PITCH] = 0;
    angles[ROLL] = 0;
    AnglesToAxis(angles, axis);

    memset(&ent, 0, sizeof(ent));
    VectorMA(cent->lerpOrigin, -16, axis[0], ent.origin);
    ent.origin[2] += 16;
    angles[YAW] += 90;
    AnglesToAxis(angles, ent.axis);

    ent.hModel = hModel;
    trap_R_AddRefEntityToScene(&ent);
}

static void CG_PlayerFlag(centity_t* cent, qhandle_t hSkin, refEntity_t* torso) {
    clientInfo_t* ci;
    refEntity_t pole;
    refEntity_t flag;
    vec3_t angles, dir;
    int legsAnim, flagAnim, updateangles;
    float angle, d;

    memset(&pole, 0, sizeof(pole));
    pole.hModel = cgs.media.flagPoleModel;
    VectorCopy(torso->lightingOrigin, pole.lightingOrigin);
    pole.shadowPlane = torso->shadowPlane;
    pole.renderfx = torso->renderfx;
    CG_PositionEntityOnTag(&pole, torso, torso->hModel, "tag_flag");
    trap_R_AddRefEntityToScene(&pole);

    memset(&flag, 0, sizeof(flag));
    flag.hModel = cgs.media.flagFlapModel;
    flag.customSkin = hSkin;
    VectorCopy(torso->lightingOrigin, flag.lightingOrigin);
    flag.shadowPlane = torso->shadowPlane;
    flag.renderfx = torso->renderfx;

    VectorClear(angles);

    updateangles = qfalse;
    legsAnim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
    if (legsAnim == LEGS_IDLE || legsAnim == LEGS_IDLECR) {
        flagAnim = FLAG_STAND;
    } else if (legsAnim == LEGS_WALK || legsAnim == LEGS_WALKCR) {
        flagAnim = FLAG_STAND;
        updateangles = qtrue;
    } else {
        flagAnim = FLAG_RUN;
        updateangles = qtrue;
    }

    if (updateangles) {
        VectorCopy(cent->currentState.pos.trDelta, dir);
        dir[2] += 100;
        VectorNormalize(dir);
        d = DotProduct(pole.axis[2], dir);
        if (fabs(d) < 0.9) {
            d = DotProduct(pole.axis[0], dir);
            if (d > 1.0f) {
                d = 1.0f;
            } else if (d < -1.0f) {
                d = -1.0f;
            }
            angle = acos(d);

            d = DotProduct(pole.axis[1], dir);
            if (d < 0) {
                angles[YAW] = 360 - angle * 180 / M_PI;
            } else {
                angles[YAW] = angle * 180 / M_PI;
            }
            if (angles[YAW] < 0)
                angles[YAW] += 360;
            if (angles[YAW] > 360)
                angles[YAW] -= 360;

            CG_SwingAngles(angles[YAW], 25, 90, 0.15f, &cent->pe.flag.yawAngle, &cent->pe.flag.yawing);
        }

    }

    angles[YAW] = cent->pe.flag.yawAngle;
    ci = &cgs.clientinfo[cent->currentState.clientNum];
    CG_RunLerpFrame(ci, &cent->pe.flag, flagAnim, 1);
    flag.oldframe = cent->pe.flag.oldFrame;
    flag.frame = cent->pe.flag.frame;
    flag.backlerp = cent->pe.flag.backlerp;

    AnglesToAxis(angles, flag.axis);
    CG_PositionRotatedEntityOnTag(&flag, &pole, pole.hModel, "tag_flag");

    trap_R_AddRefEntityToScene(&flag);
}

static void CG_PlayerTokens(centity_t* cent, int renderfx) {
    int tokens, i, j;
    float angle;
    refEntity_t ent;
    vec3_t dir, origin;
    skulltrail_t* trail;
    if (cent->currentState.number >= MAX_CLIENTS) {
        return;
    }
    trail = &cg.skulltrails[cent->currentState.number];
    tokens = cent->currentState.generic1;
    if (!tokens) {
        trail->numpositions = 0;
        return;
    }

    if (tokens > MAX_SKULLTRAIL) {
        tokens = MAX_SKULLTRAIL;
    }

    for (i = 0; i < tokens - trail->numpositions; i++) {
        for (j = trail->numpositions; j > 0; j--) {
            VectorCopy(trail->positions[j - 1], trail->positions[j]);
        }
        VectorCopy(cent->lerpOrigin, trail->positions[0]);
    }
    trail->numpositions = tokens;

    VectorCopy(cent->lerpOrigin, origin);
    for (i = 0; i < trail->numpositions; i++) {
        VectorSubtract(trail->positions[i], origin, dir);
        if (VectorNormalize(dir) > 30) {
            VectorMA(origin, 30, dir, trail->positions[i]);
        }
        VectorCopy(trail->positions[i], origin);
    }

    memset(&ent, 0, sizeof(ent));
    if (cgs.clientinfo[cent->currentState.clientNum].team == TEAM_BLUE) {
        ent.hModel = cgs.media.redCubeModel;
    } else {
        ent.hModel = cgs.media.blueCubeModel;
    }
    ent.renderfx = renderfx;

    VectorCopy(cent->lerpOrigin, origin);
    for (i = 0; i < trail->numpositions; i++) {
        VectorSubtract(origin, trail->positions[i], ent.axis[0]);
        ent.axis[0][2] = 0;
        VectorNormalize(ent.axis[0]);
        VectorSet(ent.axis[2], 0, 0, 1);
        CrossProduct(ent.axis[0], ent.axis[2], ent.axis[1]);

        VectorCopy(trail->positions[i], ent.origin);
        angle = (((cg.time + 500 * MAX_SKULLTRAIL - 500 * i) / 16) & 255) * (M_PI * 2) / 255;
        ent.origin[2] += sin(angle) * 10;
        trap_R_AddRefEntityToScene(&ent);
        VectorCopy(trail->positions[i], origin);
    }
}

static void CG_CarrierLight(const centity_t* cent, float r, float g, float b) {
    if (r_dynamicLight.integer == 2 && cent->currentState.clientNum == cg.snap->ps.clientNum) {
        return;
    }
    trap_R_AddLightToScene(cent->lerpOrigin, 200 + (rand() & 31), r, g, b);
}

static void CG_PlayerPowerups(centity_t* cent, refEntity_t* torso) {
    int powerups;
    clientInfo_t* ci;

    powerups = cent->currentState.powerups;
    if (!powerups) {
        return;
    }

    if (powerups & (1 << PW_QUAD)) {
        CG_CarrierLight(cent, 0.2f, 0.2f, 1);
    }

    if (powerups & (1 << PW_FLIGHT)) {
        trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin, cgs.media.flightSound);
        CG_FlightTrail(cent);
    }

    ci = &cgs.clientinfo[cent->currentState.clientNum];
    if (powerups & (1 << PW_REDFLAG)) {
        if (cg_flagStyle.integer != 2 && ci->newAnims) {
            CG_PlayerFlag(cent, cgs.media.redFlagFlapSkin, torso);
        } else {
            CG_TrailItem(cent, cgs.media.redFlagModel);
        }
        CG_CarrierLight(cent, 1.0, 0.2f, 0.2f);
    }

    if (powerups & (1 << PW_BLUEFLAG)) {
        if (cg_flagStyle.integer != 2 && ci->newAnims) {
            CG_PlayerFlag(cent, cgs.media.blueFlagFlapSkin, torso);
        } else {
            CG_TrailItem(cent, cgs.media.blueFlagModel);
        }
        CG_CarrierLight(cent, 0.2f, 0.2f, 1.0);
    }

    if (powerups & (1 << PW_NEUTRALFLAG)) {
        if (cg_flagStyle.integer != 2 && ci->newAnims) {
            CG_PlayerFlag(cent, cgs.media.neutralFlagFlapSkin, torso);
        } else {
            CG_TrailItem(cent, cgs.media.neutralFlagModel);
        }
        CG_CarrierLight(cent, 1.0, 1.0, 1.0);
    }

    if (powerups & (1 << PW_HASTE)) {
        CG_HasteTrail(cent);
    }
}

qhandle_t CG_ChatBalloonShader(void) {
    if (cg_animateChatBalloon.integer && cgs.media.chatBalloonFrames[0]) {
        return cgs.media.chatBalloonFrames[(cg.time / 40) % 16];
    }
    return cgs.media.balloonShader;
}

static void CG_PlayerFloatSprite(centity_t* cent, qhandle_t shader, qboolean sameTeam) {
    floatingEffect_t* fx;
    refEntity_t ent;
    qboolean ownHead = (cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson);
    float alpha = 1.0f;
    float minSpeed, speed;
    int health, maxHealth;

    if (!sameTeam && !(cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000))) {
        memset(&ent, 0, sizeof(ent));
        VectorCopy(cent->lerpOrigin, ent.origin);
        ent.origin[2] += 48;
        ent.reType = RT_SPRITE;
        ent.customShader = shader;
        ent.radius = 10;
        ent.renderfx = ownHead ? RF_THIRD_PERSON : 0;
        ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = ent.shaderRGBA[3] = 255;
        trap_R_AddRefEntityToScene(&ent);
        return;
    }

    if (!sameTeam) {
        minSpeed = atof(CG_ConfigString(CS_INFECTED_SURVIVOR_MINSPEED));
        if (cgs.clientinfo[cg.clientNum].team != TEAM_RED || ownHead) {
            return;
        }
        speed = VectorLength(cent->currentState.pos.trDelta);
        if (minSpeed > 0.0f) {
            alpha = (minSpeed - speed) / minSpeed;
            if (alpha <= 0.0f) {
                alpha = 0.0f;
            }
        }
    } else if (ownHead) {
        return;
    }

    fx = CG_AllocFloatingEffect();
    if (!fx) {
        return;
    }

    fx->color[0] = fx->color[1] = fx->color[2] = 1.0f;
    fx->color[3] = alpha;
    if (sameTeam) {
        if (cg.predictedPlayerState.pm_type & 2) {
            fx->color[2] = 0.0f;
        } else {
            health = cent->currentState.health;
            maxHealth = cg.snap->ps.stats[STAT_MAX_HEALTH];
            if (health < maxHealth) {
                fx->color[1] = (float)health / maxHealth;
                if (fx->color[1] <= 0.0f) {
                    fx->color[1] = 0.0f;
                }
                fx->color[2] = 0.0f;
            } else {
                fx->color[2] = (float)health / (maxHealth + maxHealth);
                if (fx->color[2] > 1.0f) {
                    fx->color[2] = 1.0f;
                }
            }
        }
    }

    fx->type = FE_FLOAT_SPRITE;
    fx->owner = cent;
    fx->shader = shader;
    fx->zOffset = 48.0f;
    fx->worldSize = cg_teammatePOIsMaxWidth.value;
    fx->minPixels = cg_teammatePOIsMinWidth.value;
    VectorCopy(cent->lerpOrigin, fx->origin);
}

static void CG_PlayerSprites(centity_t* cent) {
    int team = cgs.clientinfo[cent->currentState.clientNum].team;
    int flags = cent->currentState.eFlags;
    qboolean sameTeam = qfalse;
    qboolean hiddenSurvivor;

    if (!cg_drawSprites.integer) {
        return;
    }
    if (cgs.gametype >= GT_TEAM) {
        sameTeam = cg.snap->ps.persistant[PERS_TEAM] == team;
    }
    if (!cg_drawSpriteSelf.integer && cent->currentState.clientNum == cg.clientNum) {
        return;
    }

    if (flags & EF_CONNECTION) {
        CG_PlayerFloatSprite(cent, cgs.media.connectionShader, sameTeam);
        return;
    }

    if (cgs.gametype == GT_1FCTF && (cent->currentState.powerups & (1 << PW_NEUTRALFLAG)) && sameTeam) {
        CG_PlayerFloatSprite(cent, cgs.media.poiShader, sameTeam);
        return;
    }
    if (cgs.gametype >= GT_TEAM && cg_teammatePOIs.integer && !cg_teammateNames.integer && sameTeam &&
        (cent->currentState.powerups & ((1 << PW_REDFLAG) | (1 << PW_BLUEFLAG) | (1 << PW_NEUTRALFLAG)))) {
        CG_PlayerFloatSprite(cent, cg.time - cent->pe.lastPainTime < 1500 ? cgs.media.flagCarrierHitShader
                                                                        : cgs.media.flagCarrierShader, sameTeam);
        return;
    }
    if (cgs.gametype == GT_FREEZE && sameTeam && (cent->currentState.powerups & (1 << PW_FREEZE))) {
        CG_PlayerFloatSprite(cent, cgs.media.frozenShader, sameTeam);
        return;
    }

    hiddenSurvivor = cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000) && team == TEAM_BLUE && !sameTeam;
    if ((flags & EF_TALK) && !hiddenSurvivor && !(flags & EF_DEAD)) {
        CG_PlayerFloatSprite(cent, CG_ChatBalloonShader(), sameTeam);
        return;
    }
    if ((flags & EF_AWARD_IMPRESSIVE) && !hiddenSurvivor) {
        CG_PlayerFloatSprite(cent, cgs.media.medalImpressive, sameTeam);
        return;
    }
    if ((flags & EF_AWARD_EXCELLENT) && !hiddenSurvivor) {
        CG_PlayerFloatSprite(cent, cgs.media.medalExcellent, sameTeam);
        return;
    }
    if ((flags & EF_AWARD_GAUNTLET) && !hiddenSurvivor) {
        CG_PlayerFloatSprite(cent, cgs.media.medalGauntlet, sameTeam);
        return;
    }
    if (flags & EF_AWARD_DEFEND) {
        CG_PlayerFloatSprite(cent, cgs.media.medalDefend, sameTeam);
        return;
    }
    if (flags & EF_AWARD_ASSIST) {
        CG_PlayerFloatSprite(cent, cgs.media.medalAssist, sameTeam);
        return;
    }
    if (flags & EF_AWARD_CAP) {
        CG_PlayerFloatSprite(cent, cgs.media.medalCapture, sameTeam);
        return;
    }

    if (cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000) && team == TEAM_BLUE &&
        cgs.clientinfo[cg.clientNum].team == TEAM_RED) {
        CG_PlayerFloatSprite(cent, cgs.media.infectedShader, sameTeam);
        return;
    }
    if (cgs.gametype < GT_TEAM) {
        return;
    }
    if (cg_teammatePOIs.integer && !cg_teammateNames.integer && sameTeam && !(flags & EF_DEAD)) {
        CG_PlayerFloatSprite(cent, cg.time - cent->pe.lastPainTime < cg_drawHitFriendTime.integer
                                       ? cgs.media.friendHitShader : cgs.media.friendShader, sameTeam);
    }
}

#define SHADOW_DISTANCE 128
static qboolean CG_PlayerShadow(centity_t* cent, float* shadowPlane) {
    vec3_t end, mins = {-15, -15, 0}, maxs = {15, 15, 2};
    trace_t trace;
    float alpha;

    *shadowPlane = 0;

    if (cg_shadows.integer == 0) {
        return qfalse;
    }

    if (cent->currentState.powerups & (1 << PW_INVIS)) {
        return qfalse;
    }

    VectorCopy(cent->lerpOrigin, end);
    end[2] -= SHADOW_DISTANCE;

    trap_CM_BoxTrace(&trace, cent->lerpOrigin, end, mins, maxs, 0, MASK_PLAYERSOLID);

    if (trace.fraction == 1.0 || trace.startsolid || trace.allsolid) {
        return qfalse;
    }

    *shadowPlane = trace.endpos[2] + 1;

    if (cg_shadows.integer != 1) {
        return qtrue;
    }

    alpha = 1.0 - trace.fraction;

    CG_ImpactMark(cgs.media.shadowMarkShader, trace.endpos, trace.plane.normal,
                  cent->pe.legs.yawAngle, alpha, alpha, alpha, 1, qfalse, 24, qtrue);

    return qtrue;
}

static void CG_PlayerSplash(centity_t* cent) {
    vec3_t start, end;
    trace_t trace;
    int contents;
    polyVert_t verts[4];

    if (!cg_shadows.integer) {
        return;
    }

    VectorCopy(cent->lerpOrigin, end);
    end[2] -= 24;

    contents = CG_PointContents(end, 0);
    if (!(contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))) {
        return;
    }

    VectorCopy(cent->lerpOrigin, start);
    start[2] += 32;

    contents = CG_PointContents(start, 0);
    if (contents & (CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        return;
    }

    trap_CM_BoxTrace(&trace, start, end, NULL, NULL, 0, (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA));

    if (trace.fraction == 1.0) {
        return;
    }

    VectorCopy(trace.endpos, verts[0].xyz);
    verts[0].xyz[0] -= 32;
    verts[0].xyz[1] -= 32;
    verts[0].st[0] = 0;
    verts[0].st[1] = 0;
    verts[0].modulate[0] = 255;
    verts[0].modulate[1] = 255;
    verts[0].modulate[2] = 255;
    verts[0].modulate[3] = 255;

    VectorCopy(trace.endpos, verts[1].xyz);
    verts[1].xyz[0] -= 32;
    verts[1].xyz[1] += 32;
    verts[1].st[0] = 0;
    verts[1].st[1] = 1;
    verts[1].modulate[0] = 255;
    verts[1].modulate[1] = 255;
    verts[1].modulate[2] = 255;
    verts[1].modulate[3] = 255;

    VectorCopy(trace.endpos, verts[2].xyz);
    verts[2].xyz[0] += 32;
    verts[2].xyz[1] += 32;
    verts[2].st[0] = 1;
    verts[2].st[1] = 1;
    verts[2].modulate[0] = 255;
    verts[2].modulate[1] = 255;
    verts[2].modulate[2] = 255;
    verts[2].modulate[3] = 255;

    VectorCopy(trace.endpos, verts[3].xyz);
    verts[3].xyz[0] += 32;
    verts[3].xyz[1] -= 32;
    verts[3].st[0] = 1;
    verts[3].st[1] = 0;
    verts[3].modulate[0] = 255;
    verts[3].modulate[1] = 255;
    verts[3].modulate[2] = 255;
    verts[3].modulate[3] = 255;

    trap_R_AddPolyToScene(cgs.media.wakeMarkShader, 4, verts);
}

void CG_AddRefEntityWithPowerups(refEntity_t* ent, entityState_t* state, int team) {
    if ((cgs.customSettings & 0x2000000) && state->number != cg.snap->ps.clientNum) {
        ent->customShader = cgs.media.noPlayerClipShader;
        trap_R_AddRefEntityToScene(ent);
        return;
    }

    if (state->powerups & (1 << PW_INVIS)) {
        ent->customShader = cgs.media.invisShader;
        trap_R_AddRefEntityToScene(ent);
        return;
    }

    trap_R_AddRefEntityToScene(ent);

    if (state->powerups & (1 << PW_NONE)) {
        ent->customShader = ((state->event & ~EV_EVENT_BITS) == EV_PAIN)
                                ? cgs.media.spawnArmorShader
                                : cgs.media.spawnArmor2Shader;
        trap_R_AddRefEntityToScene(ent);
    }

    if (state->powerups & (1 << PW_QUAD)) {
        ent->customShader = cgs.media.quadShader;
        trap_R_AddRefEntityToScene(ent);
    }

    if ((state->powerups & (1 << PW_REGEN)) && ((cg.time / 100) % 10) == 1) {
        ent->customShader = cgs.media.regenShader;
        trap_R_AddRefEntityToScene(ent);
    }

    if (state->powerups & (1 << PW_BATTLESUIT)) {
        ent->customShader = cgs.media.battleSuitShader;
        trap_R_AddRefEntityToScene(ent);
    }

    if (state->powerups & (1 << PW_FREEZE)) {
        if (state->generic1 & 2) {
            ent->customShader = cgs.media.iceShardShader1;
        } else if (state->generic1 & 1) {
            ent->customShader = cgs.media.iceShardShader2;
        } else {
            ent->customShader = cgs.media.iceShardShader3;
        }
        trap_R_AddRefEntityToScene(ent);
    }
}

int CG_LightVerts(vec3_t normal, int numVerts, polyVert_t* verts) {
    int i, j;
    float incoming;
    vec3_t ambientLight;
    vec3_t lightDir;
    vec3_t directedLight;

    trap_R_LightForPoint(verts[0].xyz, ambientLight, directedLight, lightDir);

    for (i = 0; i < numVerts; i++) {
        incoming = DotProduct(normal, lightDir);
        if (incoming <= 0) {
            verts[i].modulate[0] = ambientLight[0];
            verts[i].modulate[1] = ambientLight[1];
            verts[i].modulate[2] = ambientLight[2];
            verts[i].modulate[3] = 255;
            continue;
        }
        j = (ambientLight[0] + incoming * directedLight[0]);
        if (j > 255) {
            j = 255;
        }
        verts[i].modulate[0] = j;

        j = (ambientLight[1] + incoming * directedLight[1]);
        if (j > 255) {
            j = 255;
        }
        verts[i].modulate[1] = j;

        j = (ambientLight[2] + incoming * directedLight[2]);
        if (j > 255) {
            j = 255;
        }
        verts[i].modulate[2] = j;

        verts[i].modulate[3] = 255;
    }
    return qtrue;
}

static void Color_UnpackScale(refEntity_t* ent, int packed, int scale) {
    int c;

    c = ((packed >> 24) & 0xff) * scale;
    ent->shaderRGBA[0] = (c > 255) ? 255 : c;
    c = ((packed >> 16) & 0xff) * scale;
    ent->shaderRGBA[1] = (c > 255) ? 255 : c;
    c = ((packed >> 8) & 0xff) * scale;
    ent->shaderRGBA[2] = (c > 255) ? 255 : c;
    ent->shaderRGBA[3] = packed & 0xff;
}

static qboolean CG_PlayerTeamSkins(centity_t* cent, refEntity_t* legs, refEntity_t* torso, refEntity_t* head) {
    int viewerTeam;
    int playerTeam;
    qboolean useEnemy;
    int scale;
    vmCvar_t *legsColor, *torsoColor, *headColor;

    viewerTeam = cgs.clientinfo[cg.predictedPlayerState.clientNum].team;
    playerTeam = cgs.clientinfo[cent->currentState.clientNum].team;

    if (cgs.gametype < GT_TEAM || !(cg.predictedPlayerState.pm_type & 2) ||
        (cg.predictedPlayerState.pm_flags & PMF_FOLLOW) || viewerTeam == TEAM_RED || viewerTeam == TEAM_BLUE) {
        if (viewerTeam == playerTeam) {
            useEnemy = (viewerTeam == TEAM_FREE);
        } else {
            useEnemy = qtrue;
        }
    } else {
        useEnemy = (playerTeam == TEAM_BLUE);
    }

    if (playerTeam == TEAM_RED) {
        if (cg_forceRedTeamModel.string[0]) {
            useEnemy = qfalse;
        }
    } else if (playerTeam == TEAM_BLUE && cg_forceBlueTeamModel.string[0]) {
        useEnemy = qtrue;
    }

    scale = 1;
    if (trap_Cvar_VariableValue("r_colorCorrectActive") == 0) {
        scale = 2;
    }

    if (cg_deadBodyDarken.integer && (cent->currentState.eFlags & EF_DEAD)) {
        Color_UnpackScale(legs, cg_deadBodyColor.integer, scale);
        Color_UnpackScale(torso, cg_deadBodyColor.integer, scale);
        Color_UnpackScale(head, cg_deadBodyColor.integer, scale);
        return useEnemy;
    }

    if (useEnemy) {
        legsColor = &cg_enemyLowerColor;
        torsoColor = &cg_enemyUpperColor;
        headColor = &cg_enemyHeadColor;
    } else {
        legsColor = &cg_teamLowerColor;
        torsoColor = &cg_teamUpperColor;
        headColor = &cg_teamHeadColor;
    }

    if (legsColor->integer == 0) {
        legs->shaderRGBA[0] = legs->shaderRGBA[1] = legs->shaderRGBA[2] = legs->shaderRGBA[3] = 255;
    } else {
        Color_UnpackScale(legs, legsColor->integer, scale);
    }
    if (torsoColor->integer == 0) {
        torso->shaderRGBA[0] = torso->shaderRGBA[1] = torso->shaderRGBA[2] = torso->shaderRGBA[3] = 255;
    } else {
        Color_UnpackScale(torso, torsoColor->integer, scale);
    }
    if (headColor->integer == 0) {
        head->shaderRGBA[0] = head->shaderRGBA[1] = head->shaderRGBA[2] = head->shaderRGBA[3] = 255;
    } else {
        Color_UnpackScale(head, headColor->integer, scale);
    }
    return useEnemy;
}

void CG_DomPointMarker(centity_t* cent) {
    int myTeam;
    int owner;
    unsigned idx;
    qhandle_t shader;
    float r, g, b;
    floatingEffect_t* fx;

    myTeam = cgs.clientinfo[cg.clientNum].team;
    idx = (unsigned)(cent->currentState.powerups - 1);
    owner = cent->currentState.modelindex2;

    if (idx >= 5) {
        idx = 0;
    }

    if (cent->currentState.weapon != 0) {
        shader = (myTeam == owner) ? cgs.media.domDefendDistShader[idx] : cgs.media.domCaptureDistShader[idx];
    } else {
        shader = (myTeam == owner) ? cgs.media.domDefendShader[idx] : cgs.media.domCaptureShader[idx];
    }

    r = (owner == TEAM_FREE || owner == TEAM_RED) ? 1.0f : 0.0f;
    if (owner == TEAM_RED) {
        g = 0.0f;
        b = 0.0f;
    } else if (owner == TEAM_BLUE) {
        g = 0.5f;
        b = 1.0f;
    } else if (owner == TEAM_FREE) {
        g = 1.0f;
        b = 1.0f;
    } else {
        g = 1.0f;
        b = 0.0f;
    }

    fx = CG_AllocFloatingEffect();
    if (!fx) {
        return;
    }

    fx->type = FE_ENTITY_POI;
    fx->owner = cent;
    VectorCopy(cent->lerpOrigin, fx->origin);
    fx->color[0] = r;
    fx->color[1] = g;
    fx->color[2] = b;
    fx->color[3] = 1.0f;
    fx->shader = shader;
    fx->zOffset = 80.0f;
    fx->worldSize = cg_poiMaxWidth.value;
    fx->minPixels = cg_poiMinWidth.value;
}

static void CG_ObjectiveMarker(centity_t* cent) {
    static const vec4_t redColor = {1.0f, 0.2f, 0.2f, 1.0f};
    static const vec4_t blueColor = {0.2f, 0.6f, 1.0f, 1.0f};
    int powerups;
    int team;
    qboolean carrying = qfalse;
    qboolean twoFlags = qtrue;
    vec3_t origin;
    vec4_t color;
    floatingEffect_t* fx;

    if (cent->currentState.clientNum != cg.snap->ps.clientNum) {
        return;
    }

    powerups = cent->currentState.powerups;
    team = cgs.clientinfo[cg.snap->ps.clientNum].team;
    VectorClear(origin);
    color[0] = color[1] = color[2] = color[3] = 0.0f;

    if (cgs.gametype == GT_HARVESTER) {
        if (team == TEAM_RED) {
            VectorCopy(cg.flagOrigin[TEAM_BLUE], origin);
            Vector4Copy(blueColor, color);
        } else if (team == TEAM_BLUE) {
            VectorCopy(cg.flagOrigin[TEAM_RED], origin);
            Vector4Copy(redColor, color);
        }
        if ((cent->currentState.generic1 & 0x3f) && (fx = CG_AllocFloatingEffect()) != NULL) {
            fx->type = FE_POI;
            fx->owner = NULL;
            VectorCopy(origin, fx->origin);
            Vector4Copy(color, fx->color);
            fx->zOffset = 64.0f;
            fx->worldSize = cg_poiMaxWidth.value;
            fx->minPixels = cg_poiMinWidth.value;
            fx->shader = cgs.media.harvesterCaptureShader;
        }
    }

    if (!powerups) {
        return;
    }

    if (cgs.gametype == GT_CTF) {
        if ((team == TEAM_RED && cgs.redflag > 0) || (team == TEAM_BLUE && cgs.blueflag > 0)) {
            return;
        }
    } else if (cgs.gametype == GT_1FCTF) {
        twoFlags = qfalse;
    }

    if (powerups & (1 << PW_REDFLAG)) {
        carrying = qtrue;
        VectorCopy(cg.flagOrigin[twoFlags ? TEAM_BLUE : TEAM_RED], origin);
        Vector4Copy(twoFlags ? blueColor : redColor, color);
    }
    if (powerups & (1 << PW_BLUEFLAG)) {
        carrying = qtrue;
        VectorCopy(cg.flagOrigin[twoFlags ? TEAM_RED : TEAM_BLUE], origin);
        Vector4Copy(twoFlags ? redColor : blueColor, color);
    }
    if (powerups & (1 << PW_NEUTRALFLAG)) {
        carrying = qtrue;
        if (twoFlags) {
            VectorCopy(cg.flagOrigin[team == TEAM_RED ? TEAM_RED : TEAM_BLUE], origin);
        } else {
            VectorCopy(cg.flagOrigin[team == TEAM_RED ? TEAM_BLUE : TEAM_RED], origin);
        }
        Vector4Copy(team == TEAM_RED ? redColor : blueColor, color);
    }
    if (!carrying) {
        return;
    }

    if (cg_flagPOIs.integer && (fx = CG_AllocFloatingEffect()) != NULL) {
        fx->type = FE_POI;
        fx->owner = NULL;
        VectorCopy(origin, fx->origin);
        Vector4Copy(color, fx->color);
        fx->zOffset = 64.0f;
        fx->worldSize = cg_poiMaxWidth.value;
        fx->minPixels = cg_poiMinWidth.value;
        fx->shader = cgs.media.adCaptureShader;
    }
}

void CG_Draw3DPlayerModel(int clientNum, int torsoAnim, float x, float y, float w, float h, int weapon,
                          float animFraction, const vec3_t angles) {
    refdef_t refdef;
    refEntity_t legs;
    refEntity_t torso;
    refEntity_t head;
    clientInfo_t* ci;
    vec3_t origin;
    float size, reach;
    double viewDist;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    ci = &cgs.clientinfo[clientNum];
    if (!ci->infoValid || !ci->legsModel || !ci->torsoModel || !ci->headModel) {
        return;
    }

    size = 32.0f;
    if (ci->modelScale != 0.0f) {
        size = 32.0f / (ci->modelScale * 0.85f);
    }

    CG_AdjustFrom640(&x, &y, &w, &h);

    memset(&refdef, 0, sizeof(refdef));
    memset(&legs, 0, sizeof(legs));
    memset(&torso, 0, sizeof(torso));
    memset(&head, 0, sizeof(head));

    legs.shaderRGBA[1] = 0x80;
    legs.shaderRGBA[3] = 0xff;
    Byte4Copy(legs.shaderRGBA, torso.shaderRGBA);
    Byte4Copy(legs.shaderRGBA, head.shaderRGBA);

    refdef.x = (int)x;
    refdef.y = (int)y;
    refdef.width = (int)w;
    refdef.height = (int)h;
    refdef.rdflags = RDF_NOWORLDMODEL;
    AxisClear(refdef.viewaxis);

    refdef.fov_x = (float)(int)(refdef.width / 640.0 * 90.0);
    viewDist = refdef.width / tan(refdef.fov_x / 360.0 * M_PI);
    refdef.fov_y = atan2((double)refdef.height, (float)viewDist) * (360.0 / M_PI);

    reach = (size + 24.0f) * 1.1f;
    origin[0] = reach / tan(refdef.fov_x * M_PI / 180.0 * 0.5);
    origin[1] = 0.0f;
    origin[2] = (size - 24.0f) * -0.5f;

    refdef.time = cg.time;

    trap_R_ClearScene();

    legs.hModel = ci->legsModel;
    legs.customSkin = ci->legsSkin;
    legs.renderfx = RF_NOSHADOW | RF_LIGHTING_ORIGIN;
    legs.frame = (int)(ci->animations[LEGS_IDLE].numFrames * animFraction + ci->animations[LEGS_IDLE].firstFrame);
    AnglesToAxis(angles, legs.axis);
    VectorCopy(origin, legs.origin);
    VectorCopy(origin, legs.lightingOrigin);
    trap_R_AddRefEntityToScene(&legs);

    torso.hModel = ci->torsoModel;
    torso.customSkin = ci->torsoSkin;
    torso.renderfx = RF_NOSHADOW | RF_LIGHTING_ORIGIN;
    torso.frame = (int)(ci->animations[torsoAnim].numFrames * animFraction + ci->animations[torsoAnim].firstFrame);
    AnglesToAxis(vec3_origin, torso.axis);
    VectorCopy(origin, torso.lightingOrigin);
    CG_PositionRotatedEntityOnTag(&torso, &legs, ci->legsModel, "tag_torso");
    trap_R_AddRefEntityToScene(&torso);

    head.hModel = ci->headModel;
    head.customSkin = ci->headSkin;
    head.renderfx = RF_NOSHADOW | RF_LIGHTING_ORIGIN;
    AnglesToAxis(vec3_origin, head.axis);
    VectorCopy(origin, head.lightingOrigin);
    CG_PositionRotatedEntityOnTag(&head, &torso, ci->torsoModel, "tag_head");
    trap_R_AddRefEntityToScene(&head);

    if (weapon > WP_NONE && weapon < MAX_WEAPONS) {
        refEntity_t gun;

        memset(&gun, 0, sizeof(gun));
        gun.hModel = cg_weapons[weapon].weaponModel;
        gun.renderfx = RF_NOSHADOW | RF_LIGHTING_ORIGIN;
        AnglesToAxis(vec3_origin, gun.axis);
        VectorCopy(origin, gun.lightingOrigin);
        CG_PositionRotatedEntityOnTag(&gun, &torso, ci->torsoModel, "tag_weapon");
        trap_R_AddRefEntityToScene(&gun);

        if (weapon == WP_MACHINEGUN || weapon == WP_GAUNTLET || weapon == WP_BFG) {
            refEntity_t barrel;

            memset(&barrel, 0, sizeof(barrel));
            barrel.hModel = cg_weapons[weapon].barrelModel;
            barrel.renderfx = RF_NOSHADOW | RF_LIGHTING_ORIGIN;
            AnglesToAxis(vec3_origin, barrel.axis);
            VectorCopy(origin, barrel.lightingOrigin);
            CG_PositionRotatedEntityOnTag(&barrel, &gun, cg_weapons[weapon].weaponModel, "tag_barrel");
            trap_R_AddRefEntityToScene(&barrel);
        }
    }

    origin[0] -= 100.0f;
    origin[1] += 100.0f;
    origin[2] += 100.0f;
    trap_R_AddLightToScene(origin, 500, 1.0f, 1.0f, 1.0f);

    origin[0] -= 100.0f;
    origin[1] -= 100.0f;
    origin[2] -= 100.0f;
    trap_R_AddLightToScene(origin, 500, 1.0f, 0.0f, 0.0f);

    trap_R_RenderScene(&refdef);
}

qboolean CG_XrayAllowed(void) {
    if (!cg_xray.integer) {
        return qfalse;
    }
    return (cg.demoPlayback || cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR) ? qtrue : qfalse;
}

static void CG_AddXray(const refEntity_t* part, const byte* color) {
    refEntity_t ent = *part;

    ent.renderfx = (ent.renderfx & ~RF_THIRD_PERSON) | RF_DEPTHHACK | RF_NOSHADOW;
    ent.customShader = cgs.media.xrayMaskShader;
    trap_R_AddRefEntityToScene(&ent);

    ent.customShader = cgs.media.xrayShader;
    ent.shaderRGBA[0] = color[0];
    ent.shaderRGBA[1] = color[1];
    ent.shaderRGBA[2] = color[2];
    ent.shaderRGBA[3] = 255;
    trap_R_AddRefEntityToScene(&ent);
}

void CG_Player(centity_t* cent) {
    clientInfo_t* ci;
    refEntity_t legs;
    refEntity_t torso;
    refEntity_t head;
    int clientNum;
    int renderfx;
    qboolean shadow;
    float shadowPlane;
    refEntity_t skull;
    refEntity_t powerup;
    int t;
    float c;
    float angle;
    vec3_t dir, angles;
    qboolean enemy;

    clientNum = cent->currentState.clientNum;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        CG_Error("Bad clientNum on player entity");
    }
    ci = &cgs.clientinfo[clientNum];

    if (!ci->infoValid) {
        return;
    }

    renderfx = 0;
    if (cent->currentState.number == cg.snap->ps.clientNum) {
        if (!cg.renderingThirdPerson) {
            renderfx = RF_THIRD_PERSON;
        } else {
            if (cg_cameraMode.integer) {
                return;
            }
        }
    }

    memset(&legs, 0, sizeof(legs));
    memset(&torso, 0, sizeof(torso));
    memset(&head, 0, sizeof(head));

    enemy = CG_PlayerTeamSkins(cent, &legs, &torso, &head);

    CG_PlayerAngles(cent, legs.axis, torso.axis, head.axis);

    CG_PlayerAnimation(cent, &legs.oldframe, &legs.frame, &legs.backlerp,
                       &torso.oldframe, &torso.frame, &torso.backlerp);

    CG_PlayerSprites(cent);

    shadow = CG_PlayerShadow(cent, &shadowPlane);

    CG_PlayerSplash(cent);

    if (cg_shadows.integer == 3 && shadow) {
        renderfx |= RF_SHADOW_PLANE;
    }
    renderfx |= RF_LIGHTING_ORIGIN;
    if (cgs.gametype == GT_HARVESTER) {
        CG_PlayerTokens(cent, renderfx);
    }
    if (cg_scalePlayerModelsToBB.integer && ci->modelScale != 1.0f) {
        int k;
        float s = ci->modelScale;
        for (k = 0; k < 3; k++) {
            VectorScale(legs.axis[k], s, legs.axis[k]);
            VectorScale(torso.axis[k], s, torso.axis[k]);
            VectorScale(head.axis[k], s, head.axis[k]);
        }
    }

    legs.hModel = ci->legsModel;
    legs.customSkin = ci->legsSkin;

    VectorCopy(cent->lerpOrigin, legs.origin);

    VectorCopy(cent->lerpOrigin, legs.lightingOrigin);
    legs.shadowPlane = shadowPlane;
    legs.renderfx = renderfx;
    if (cg_scalePlayerModelsToBB.integer && ci->modelScale != 1.0f) {
        legs.origin[2] -= (24.0f - ci->modelScale * 24.0f);
        legs.lightingOrigin[2] = legs.origin[2];
    }
    VectorCopy(legs.origin, legs.oldorigin);

    CG_AddRefEntityWithPowerups(&legs, &cent->currentState, ci->team);

    if (!legs.hModel) {
        return;
    }

    torso.hModel = ci->torsoModel;
    if (!torso.hModel) {
        return;
    }

    torso.customSkin = ci->torsoSkin;

    VectorCopy(cent->lerpOrigin, torso.lightingOrigin);

    CG_PositionRotatedEntityOnTag(&torso, &legs, ci->legsModel, "tag_torso");

    torso.shadowPlane = shadowPlane;
    torso.renderfx = renderfx;

    CG_AddRefEntityWithPowerups(&torso, &cent->currentState, ci->team);

    if (cent->currentState.eFlags & EF_KAMIKAZE) {
        memset(&skull, 0, sizeof(skull));

        VectorCopy(cent->lerpOrigin, skull.lightingOrigin);
        skull.shadowPlane = shadowPlane;
        skull.renderfx = renderfx;

        if (cent->currentState.eFlags & EF_DEAD) {
            angle = ((cg.time / 7) & 255) * (M_PI * 2) / 255;
            if (angle > M_PI * 2)
                angle -= (float)M_PI * 2;
            dir[0] = sin(angle) * 20;
            dir[1] = cos(angle) * 20;
            angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255;
            dir[2] = 15 + sin(angle) * 8;
            VectorAdd(torso.origin, dir, skull.origin);

            dir[2] = 0;
            VectorCopy(dir, skull.axis[1]);
            VectorNormalize(skull.axis[1]);
            VectorSet(skull.axis[2], 0, 0, 1);
            CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);

            skull.hModel = cgs.media.kamikazeHeadModel;
            trap_R_AddRefEntityToScene(&skull);
            skull.hModel = cgs.media.kamikazeHeadTrail;
            trap_R_AddRefEntityToScene(&skull);
        } else {
            angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255;
            dir[0] = cos(angle) * 20;
            dir[1] = sin(angle) * 20;
            dir[2] = cos(angle) * 20;
            VectorAdd(torso.origin, dir, skull.origin);

            angles[0] = sin(angle) * 30;
            angles[1] = (angle * 180 / M_PI) + 90;
            if (angles[1] > 360)
                angles[1] -= 360;
            angles[2] = 0;
            AnglesToAxis(angles, skull.axis);

            skull.hModel = cgs.media.kamikazeHeadModel;
            trap_R_AddRefEntityToScene(&skull);
            VectorInverse(skull.axis[1]);
            skull.hModel = cgs.media.kamikazeHeadTrail;
            trap_R_AddRefEntityToScene(&skull);

            angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255 + M_PI;
            if (angle > M_PI * 2)
                angle -= (float)M_PI * 2;
            dir[0] = sin(angle) * 20;
            dir[1] = cos(angle) * 20;
            dir[2] = cos(angle) * 20;
            VectorAdd(torso.origin, dir, skull.origin);

            angles[0] = cos(angle - 0.5 * M_PI) * 30;
            angles[1] = 360 - (angle * 180 / M_PI);
            if (angles[1] > 360)
                angles[1] -= 360;
            angles[2] = 0;
            AnglesToAxis(angles, skull.axis);

            skull.hModel = cgs.media.kamikazeHeadModel;
            trap_R_AddRefEntityToScene(&skull);
            skull.hModel = cgs.media.kamikazeHeadTrail;
            trap_R_AddRefEntityToScene(&skull);

            angle = ((cg.time / 3) & 255) * (M_PI * 2) / 255 + 0.5 * M_PI;
            if (angle > M_PI * 2)
                angle -= (float)M_PI * 2;
            dir[0] = sin(angle) * 20;
            dir[1] = cos(angle) * 20;
            dir[2] = 0;
            VectorAdd(torso.origin, dir, skull.origin);

            VectorCopy(dir, skull.axis[1]);
            VectorNormalize(skull.axis[1]);
            VectorSet(skull.axis[2], 0, 0, 1);
            CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);

            skull.hModel = cgs.media.kamikazeHeadModel;
            trap_R_AddRefEntityToScene(&skull);
            skull.hModel = cgs.media.kamikazeHeadTrail;
            trap_R_AddRefEntityToScene(&skull);
        }
    }

    if (cent->currentState.powerups & (1 << PW_GUARD)) {
        memcpy(&powerup, &torso, sizeof(torso));
        powerup.hModel = cgs.media.guardPowerupModel;
        powerup.frame = 0;
        powerup.oldframe = 0;
        powerup.customSkin = 0;
        trap_R_AddRefEntityToScene(&powerup);
    }
    if (cent->currentState.powerups & (1 << PW_SCOUT)) {
        memcpy(&powerup, &torso, sizeof(torso));
        powerup.hModel = cgs.media.scoutPowerupModel;
        powerup.frame = 0;
        powerup.oldframe = 0;
        powerup.customSkin = 0;
        trap_R_AddRefEntityToScene(&powerup);
    }
    if (cent->currentState.powerups & (1 << PW_DOUBLER)) {
        memcpy(&powerup, &torso, sizeof(torso));
        powerup.hModel = cgs.media.doublerPowerupModel;
        powerup.frame = 0;
        powerup.oldframe = 0;
        powerup.customSkin = 0;
        trap_R_AddRefEntityToScene(&powerup);
    }
    if (cent->currentState.powerups & (1 << PW_AMMOREGEN)) {
        memcpy(&powerup, &torso, sizeof(torso));
        powerup.hModel = cgs.media.ammoRegenPowerupModel;
        powerup.frame = 0;
        powerup.oldframe = 0;
        powerup.customSkin = 0;
        trap_R_AddRefEntityToScene(&powerup);
    }
    if (cent->currentState.powerups & (1 << PW_INVULNERABILITY)) {
        if (!ci->invulnerabilityStartTime) {
            ci->invulnerabilityStartTime = cg.time;
        }
        ci->invulnerabilityStopTime = cg.time;
    } else {
        ci->invulnerabilityStartTime = 0;
    }
    if ((cent->currentState.powerups & (1 << PW_INVULNERABILITY)) ||
        cg.time - ci->invulnerabilityStopTime < 250) {
        memcpy(&powerup, &torso, sizeof(torso));
        powerup.hModel = cgs.media.invulnerabilityPowerupModel;
        powerup.frame = 0;
        powerup.oldframe = 0;
        powerup.customSkin = 0;
        powerup.renderfx &= ~RF_THIRD_PERSON;
        VectorCopy(cent->lerpOrigin, powerup.origin);

        if (cg.time - ci->invulnerabilityStartTime < 250) {
            c = (float)(cg.time - ci->invulnerabilityStartTime) / 250;
        } else if (cg.time - ci->invulnerabilityStopTime < 250) {
            c = (float)(250 - (cg.time - ci->invulnerabilityStopTime)) / 250;
        } else {
            c = 1;
        }
        VectorSet(powerup.axis[0], c, 0, 0);
        VectorSet(powerup.axis[1], 0, c, 0);
        VectorSet(powerup.axis[2], 0, 0, c);
        trap_R_AddRefEntityToScene(&powerup);
    }

    t = cg.time - ci->medkitUsageTime;
    if (ci->medkitUsageTime && t < 500) {
        memcpy(&powerup, &torso, sizeof(torso));
        powerup.hModel = cgs.media.medkitUsageModel;
        powerup.frame = 0;
        powerup.oldframe = 0;
        powerup.customSkin = 0;
        powerup.renderfx &= ~RF_THIRD_PERSON;
        VectorClear(angles);
        AnglesToAxis(angles, powerup.axis);
        VectorCopy(cent->lerpOrigin, powerup.origin);
        powerup.origin[2] += -24 + (float)t * 80 / 500;
        if (t > 400) {
            c = (float)(t - 1000) * 0xff / 100;
            powerup.shaderRGBA[0] = 0xff - c;
            powerup.shaderRGBA[1] = 0xff - c;
            powerup.shaderRGBA[2] = 0xff - c;
            powerup.shaderRGBA[3] = 0xff - c;
        } else {
            powerup.shaderRGBA[0] = 0xff;
            powerup.shaderRGBA[1] = 0xff;
            powerup.shaderRGBA[2] = 0xff;
            powerup.shaderRGBA[3] = 0xff;
        }
        trap_R_AddRefEntityToScene(&powerup);
    }

    head.hModel = ci->headModel;
    if (!head.hModel) {
        return;
    }
    head.customSkin = ci->headSkin;

    VectorCopy(cent->lerpOrigin, head.lightingOrigin);

    CG_PositionRotatedEntityOnTag(&head, &torso, ci->torsoModel, "tag_head");

    head.shadowPlane = shadowPlane;
    head.renderfx = renderfx;

    CG_AddRefEntityWithPowerups(&head, &cent->currentState, ci->team);

    CG_DustTrail(cent);

    if (cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000) && ci->team == TEAM_RED &&
        cent->currentState.weapon == WP_GAUNTLET) {
        CG_RegisterWeapon(WP_GAUNTLET);
    } else {
        CG_AddPlayerWeapon(&torso, NULL, cent, ci->team);
    }

    CG_PlayerPowerups(cent, &torso);

    CG_ObjectiveMarker(cent);

    if (cgs.gametype == GT_RR && (cgs.customSettings & 0x4000000) && ci->team == TEAM_RED) {
        legs.customShader = cgs.media.gooShader;
        torso.customShader = cgs.media.gooShader;
        head.customShader = cgs.media.gooShader;
        trap_R_AddRefEntityToScene(&legs);
        trap_R_AddRefEntityToScene(&torso);
        trap_R_AddRefEntityToScene(&head);
        trap_S_AddLoopingSound(cent->currentState.number, cent->lerpOrigin, vec3_origin,
                               cgs.media.nightmareSound);
    }

    if (CG_XrayAllowed() && enemy && !(cent->currentState.eFlags & EF_DEAD) &&
        cent->currentState.number != cg.snap->ps.clientNum) {
        CG_AddXray(&legs, torso.shaderRGBA);
        CG_AddXray(&torso, torso.shaderRGBA);
        CG_AddXray(&head, torso.shaderRGBA);
    }
}

void CG_ResetPlayerEntity(centity_t* cent) {
    cent->errorTime = -99999;
    cent->extrapolated = qfalse;

    CG_ClearLerpFrame(&cgs.clientinfo[cent->currentState.clientNum], &cent->pe.legs, cent->currentState.legsAnim);
    CG_ClearLerpFrame(&cgs.clientinfo[cent->currentState.clientNum], &cent->pe.torso, cent->currentState.torsoAnim);

    BG_EvaluateTrajectory(&cent->currentState.pos, cg.time, cent->lerpOrigin);
    BG_EvaluateTrajectory(&cent->currentState.apos, cg.time, cent->lerpAngles);

    VectorCopy(cent->lerpOrigin, cent->rawOrigin);
    VectorCopy(cent->lerpAngles, cent->rawAngles);

    memset(&cent->pe.legs, 0, sizeof(cent->pe.legs));
    cent->pe.legs.yawAngle = cent->rawAngles[YAW];
    cent->pe.legs.yawing = qfalse;
    cent->pe.legs.pitchAngle = 0;
    cent->pe.legs.pitching = qfalse;

    memset(&cent->pe.torso, 0, sizeof(cent->pe.torso));
    cent->pe.torso.yawAngle = cent->rawAngles[YAW];
    cent->pe.torso.yawing = qfalse;
    cent->pe.torso.pitchAngle = cent->rawAngles[PITCH];
    cent->pe.torso.pitching = qfalse;
}
