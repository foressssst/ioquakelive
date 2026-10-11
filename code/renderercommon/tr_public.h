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
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"

#define REF_API_VERSION 12

typedef struct {
    void (*Shutdown)(qboolean destroyWindow);

    void (*BeginRegistration)(glconfig_t* config);
    qhandle_t (*RegisterModel)(const char* name);
    qhandle_t (*RegisterSkin)(const char* name);
    qhandle_t (*RegisterShader)(const char* name);
    qhandle_t (*RegisterShaderNoMip)(const char* name);
    void (*LoadWorld)(const char* name);

    void (*SetWorldVisData)(const byte* vis);

    void (*EndRegistration)(void);

    void (*ClearScene)(void);
    void (*AddRefEntityToScene)(const refEntity_t* re);
    void (*AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t* verts, int num);
    int (*LightForPoint)(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
    void (*AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
    void (*AddAdditiveLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
    void (*RenderScene)(const refdef_t* fd);

    void (*SetColor)(const float* rgba);
    void (*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);

    void (*DrawStretchRaw)(int x, int y, int w, int h, int cols, int rows, const byte* data, int client, qboolean dirty);
    void (*UploadCinematic)(int w, int h, int cols, int rows, const byte* data, int client, qboolean dirty);

    qboolean (*WindowResized)(int width, int height);

    qboolean (*ApplyVideoMode)(glconfig_t* config);

    void (*BeginFrame)(stereoFrame_t stereoFrame);

    void (*EndFrame)(int* frontEndMsec, int* backEndMsec);

    int (*MarkFragments)(int numPoints, const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t* fragmentBuffer);

    int (*LerpTag)(orientation_t* tag, qhandle_t model, int startFrame, int endFrame, float frac, const char* tagName);
    void (*ModelBounds)(qhandle_t model, vec3_t mins, vec3_t maxs);

#ifdef __USEA3D
    void (*A3D_RenderGeometry)(void* pVoidA3D, void* pVoidGeom, void* pVoidMat, void* pVoidGeomStatus);
#endif
    void (*RegisterFont)(const char* fontName, int pointSize, fontInfo_t* font);

    void (*Font_DrawString)(int x, int y, const char* text, int fontIndex, float scale, int limit, float* maxX, int flags);
    void (*TextBounds)(const char* text, int start, int fontIndex, float scale, int limit, float* extents);
    void (*GetGlyphInfo)(int fontIndex, int charValue, glyphInfo_t* glyph);
    void (*SetCompositionFont)(int fontIndex, float scale);
    int (*Font_Register)(const char* name);

    void (*RemapShader)(const char* oldShader, const char* newShader, const char* offsetTime);
    qboolean (*GetEntityToken)(char* buffer, int size);
    qboolean (*inPVS)(const vec3_t p1, const vec3_t p2);

    void (*TakeVideoFrame)(int h, int w, byte* captureBuffer, byte* encodeBuffer, qboolean motionJpeg);

    void (*BeginBloomScene)(void);
    void (*ApplyBloom)(void);

    void (*Get_Advertisements)(int* num, float* verts, char shaders[][MAX_QPATH]);

    void (*LowLatencyWait)(void);

    void (*LowLatencyFlash)(void);
} refexport_t;

typedef struct {
    void(QDECL* Printf)(int printLevel, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    void(QDECL* Error)(int errorLevel, const char* fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

    int (*Milliseconds)(void);
    int64_t (*Microseconds)(void);
    void (*JobsSubmit)(void (*function)(void*), void* data);
    int (*JobWorkers)(void);
    void (*Sleep)(int milliseconds);
    void (*ThreadYield)(void);

#ifdef HUNK_DEBUG
    void* (*Hunk_AllocDebug)(int size, ha_pref pref, char* label, char* file, int line);
#else
    void* (*Hunk_Alloc)(int size, ha_pref pref);
#endif
    void* (*Hunk_AllocateTempMemory)(int size);
    void (*Hunk_FreeTempMemory)(void* block);

    void* (*Malloc)(int bytes);
    void (*Free)(void* buf);

    cvar_t* (*Cvar_Get)(const char* name, const char* value, int flags);
    void (*Cvar_Set)(const char* name, const char* value);
    void (*Cvar_SetValue)(const char* name, float value);
    void (*Cvar_CheckRange)(cvar_t* cv, float minVal, float maxVal, qboolean shouldBeIntegral);
    cvar_t* (*Cvar_GetRange)(const char* name, const char* value, const char* minValue, const char* maxValue, int flags);
    void (*Cvar_SetDescription)(cvar_t* cv, const char* description);

    int (*Cvar_VariableIntegerValue)(const char* var_name);

    void (*Cmd_AddCommand)(const char* name, void (*cmd)(void));
    void (*Cmd_RemoveCommand)(const char* name);

    int (*Cmd_Argc)(void);
    char* (*Cmd_Argv)(int i);

    void (*Cmd_ExecuteText)(int exec_when, const char* text);

    byte* (*CM_ClusterPVS)(int cluster);

    void (*CM_DrawDebugSurface)(void (*drawPoly)(int color, int numPoints, float* points));

    int (*FS_FileIsInPAK)(const char* name, int* pCheckSum);
    long (*FS_ReadFile)(const char* name, void** buf);
    void (*FS_FreeFile)(void* buf);
    char** (*FS_ListFiles)(const char* name, const char* extension, int* numfilesfound);
    void (*FS_FreeFileList)(char** filelist);
    void (*FS_WriteFile)(const char* qpath, const void* buffer, int size);
    qboolean (*FS_FileExists)(const char* file);
    long (*FS_ReadCacheFile)(const char* name, void** buf);
    void (*FS_WriteCacheFile)(const char* name, const void* buffer, int size);

    void (*CIN_UploadCinematic)(int handle);
    int (*CIN_PlayCinematic)(const char* arg0, int xpos, int ypos, int width, int height, int bits);
    e_status (*CIN_RunCinematic)(int handle);

    void (*CL_WriteAVIVideoFrame)(const byte* buffer, int size);

    void (*IN_Init)(void* windowData);
    void (*IN_Shutdown)(void);
    void (*IN_Restart)(void);

    long (*ftol)(float f);

    void (*Sys_SetEnv)(const char* name, const char* value);
    void (*Sys_GLimpSafeInit)(void);
    void (*Sys_GLimpInit)(void);
    qboolean (*Sys_LowPhysicalMemory)(void);
} refimport_t;

#ifdef USE_RENDERER_DLOPEN
typedef refexport_t*(QDECL* GetRefAPI_t)(int apiVersion, refimport_t* rimp);
#else
refexport_t* GetRefAPI(int apiVersion, refimport_t* rimp);
#endif

#endif
