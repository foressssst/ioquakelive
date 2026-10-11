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
#ifndef __Q_SHARED_H
#define __Q_SHARED_H

#define PRODUCT_NAME            "ioquakelive"
#define BASEGAME                "baseqz"
#define BASEGAME_DIR            "baseq3"
#define CLIENT_WINDOW_TITLE     "ioquakelive"
#define HOMEPATH_NAME_UNIX      ".ioquakelive"
#define HOMEPATH_NAME_WIN       "ioquakelive"
#define HOMEPATH_NAME_MACOSX    HOMEPATH_NAME_WIN
#define PROTOCOL_HANDLER        "quakelive"
#define STEAMPATH_NAME			"Quake Live"
#define STEAMPATH_APPID         "282440"

#ifndef PRODUCT_VERSION
#define PRODUCT_VERSION "0.3.13"
#endif

#ifndef PRODUCT_DATE
#define PRODUCT_DATE __DATE__
#endif

#ifndef PRODUCT_TIME
#define PRODUCT_TIME __TIME__
#endif

#if defined(__has_include)
#  if __has_include("git_version.h")
#    include "git_version.h"
#  endif
#endif
#ifndef PRODUCT_GIT_HASH
#define PRODUCT_GIT_HASH "unknown"
#endif

#define FULL_PRODUCT_VERSION PRODUCT_NAME " v" PRODUCT_VERSION " (" PRODUCT_GIT_HASH ") - " PRODUCT_DATE " @ " PRODUCT_TIME

#define MAX_TEAMNAME 32

#define DEMOEXT "dm_"

#ifdef _MSC_VER

#pragma warning(disable : 4018)
#pragma warning(disable : 4032)
#pragma warning(disable : 4051)
#pragma warning(disable : 4057)
#pragma warning(disable : 4100)
#pragma warning(disable : 4115)
#pragma warning(disable : 4125)
#pragma warning(disable : 4127)
#pragma warning(disable : 4136)
#pragma warning(disable : 4152)
#pragma warning(disable : 4244)
#pragma warning(disable : 4142)
#pragma warning(disable : 4514)
#pragma warning(disable : 4702)
#pragma warning(disable : 4711)
#pragma warning(disable : 4220)
#endif

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#ifdef __GNUC__
#define UNUSED_VAR __attribute__((unused))
#else
#define UNUSED_VAR
#endif

#if (defined _MSC_VER)
#define Q_EXPORT __declspec(dllexport)
#elif (defined __SUNPRO_C)
#define Q_EXPORT __global
#elif ((__GNUC__ >= 3) && (!__EMX__) && (!sun))
#define Q_EXPORT __attribute__((visibility("default")))
#else
#define Q_EXPORT
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#ifdef _MSC_VER
#include <io.h>

typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
#else
#include <stdint.h>
#endif

#ifdef _WIN32
int Q_vsnprintf(char* str, size_t size, const char* format, va_list ap);
#else
#define Q_vsnprintf vsnprintf
#endif

#include "q_platform.h"

typedef unsigned char byte;

typedef enum { qfalse,
               qtrue } qboolean;

typedef union {
    float f;
    int i;
    unsigned int ui;
} floatint_t;

typedef int qhandle_t;
typedef int sfxHandle_t;
typedef int fileHandle_t;
typedef int clipHandle_t;

#define PAD(base, alignment) (((base) + (alignment) - 1) & ~((alignment) - 1))
#define PADLEN(base, alignment) (PAD((base), (alignment)) - (base))

#define PADP(base, alignment) ((void*)PAD((intptr_t)(base), (alignment)))

#ifdef __GNUC__
#define QALIGN(x) __attribute__((aligned(x)))
#else
#define QALIGN(x)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define STRING(s) #s
#define XSTRING(s) STRING(s)

#define MAX_QINT 0x7fffffff
#define MIN_QINT (-MAX_QINT - 1)

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))
#define STRARRAY_LEN(x) (ARRAY_LEN(x) - 1)

#define PITCH 0
#define YAW 1
#define ROLL 2

#define MAX_STRING_CHARS 1024
#define MAX_STRING_TOKENS 1024
#define MAX_TOKEN_CHARS 1024

#define MAX_INFO_STRING 1024
#define MAX_INFO_KEY 1024
#define MAX_INFO_VALUE 1024

#define BIG_INFO_STRING 8192
#define BIG_INFO_KEY 8192
#define BIG_INFO_VALUE 8192

#define MAX_QPATH 64
#ifdef PATH_MAX
#define MAX_OSPATH PATH_MAX
#else
#define MAX_OSPATH 256
#endif

#define MAX_NAME_LENGTH 40

#define MAX_SAY_TEXT 150

typedef enum {
    EXEC_NOW,
    EXEC_INSERT,
    EXEC_APPEND
} cbufExec_t;

#define MAX_MAP_AREA_BYTES 32

typedef enum {
    PRINT_ALL,
    PRINT_DEVELOPER,
    PRINT_WARNING,
    PRINT_ERROR
} printParm_t;

#ifdef ERR_FATAL
#undef ERR_FATAL
#endif

typedef enum {
    ERR_FATAL,
    ERR_DROP,
    ERR_SERVERDISCONNECT,
    ERR_DISCONNECT,
    ERR_NEED_CD
} errorParm_t;

#define PROP_GAP_WIDTH 3
#define PROP_SPACE_WIDTH 8
#define PROP_HEIGHT 27
#define PROP_SMALL_SIZE_SCALE 0.75

#define BLINK_DIVISOR 200
#define PULSE_DIVISOR 75

#define UI_LEFT 0x00000000
#define UI_CENTER 0x00000001
#define UI_RIGHT 0x00000002
#define UI_FORMATMASK 0x00000007
#define UI_SMALLFONT 0x00000010
#define UI_BIGFONT 0x00000020
#define UI_GIANTFONT 0x00000040
#define UI_DROPSHADOW 0x00000800
#define UI_BLINK 0x00001000
#define UI_INVERSE 0x00002000
#define UI_PULSE 0x00004000

#if !defined(NDEBUG) && !defined(BSPC)
#define HUNK_DEBUG
#endif

typedef enum {
    h_high,
    h_low,
    h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
#define Hunk_Alloc(size, preference) Hunk_AllocDebug(size, preference, #size, __FILE__, __LINE__)
void* Hunk_AllocDebug(int size, ha_pref preference, char* label, char* file, int line);
#else
void* Hunk_Alloc(int size, ha_pref preference);
#endif

#define Com_Memset memset
#define Com_Memcpy memcpy

#define CIN_system 1
#define CIN_loop 2
#define CIN_hold 4
#define CIN_silent 8
#define CIN_shader 16

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef vec_t quat_t[4];

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define NUMVERTEXNORMALS 162
extern vec3_t bytedirs[NUMVERTEXNORMALS];

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define TINYCHAR_WIDTH (SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT (SMALLCHAR_HEIGHT / 2)

#define SMALLCHAR_WIDTH 8
#define SMALLCHAR_HEIGHT 16

#define BIGCHAR_WIDTH 16
#define BIGCHAR_HEIGHT 16

#define GIANTCHAR_WIDTH 32
#define GIANTCHAR_HEIGHT 48

extern vec4_t colorBlack;
extern vec4_t colorRed;
extern vec4_t colorGreen;
extern vec4_t colorBlue;
extern vec4_t colorYellow;
extern vec4_t colorMagenta;
extern vec4_t colorCyan;
extern vec4_t colorWhite;
extern vec4_t colorLtGrey;
extern vec4_t colorMdGrey;
extern vec4_t colorDkGrey;

#define Q_COLOR_ESCAPE '^'
qboolean Q_IsColorString(const char* p);

#define COLOR_BLACK '0'
#define COLOR_RED '1'
#define COLOR_GREEN '2'
#define COLOR_YELLOW '3'
#define COLOR_BLUE '4'
#define COLOR_CYAN '5'
#define COLOR_MAGENTA '6'
#define COLOR_WHITE '7'
#define ColorIndexForNumber(c) ((c) & 0x07)
#define ColorIndex(c) (ColorIndexForNumber((c) - '0'))

#define S_COLOR_BLACK "^0"
#define S_COLOR_RED "^1"
#define S_COLOR_GREEN "^2"
#define S_COLOR_YELLOW "^3"
#define S_COLOR_BLUE "^4"
#define S_COLOR_CYAN "^5"
#define S_COLOR_MAGENTA "^6"
#define S_COLOR_WHITE "^7"

#define COLOR_CON_CVAR '\x01'
#define COLOR_CON_CMD '\x02'
#define COLOR_CON_VAL '\x03'
#define COLOR_CON_HELP '\x04'
#define S_COLOR_CVAR "^\x01"
#define S_COLOR_CMD "^\x02"
#define S_COLOR_VAL "^\x03"
#define S_COLOR_HELP "^\x04"
#define Q_IsConsoleColorString(p) ((p) && (p)[0] == Q_COLOR_ESCAPE && (p)[1] >= COLOR_CON_CVAR && (p)[1] <= COLOR_CON_HELP)

#define Q_NUM_COLOR_CODES 36
int Q_ColorCodeIndex(int c, qboolean extended);
int Q_ColorCodeChar(int index);
const float* Q_ColorFromCodeIndex(int index);

extern vec4_t g_color_table[8];

#define MAKERGB(v, r, g, b) \
    v[0] = r;               \
    v[1] = g;               \
    v[2] = b
#define MAKERGBA(v, r, g, b, a) \
    v[0] = r;                   \
    v[1] = g;                   \
    v[2] = b;                   \
    v[3] = a

#define DEG2RAD(a) (((a) * M_PI) / 180.0F)
#define RAD2DEG(a) (((a) * 180.0f) / M_PI)

struct cplane_s;

extern vec3_t vec3_origin;
extern vec3_t axisDefault[3];

#define nanmask (255 << 23)

#define IS_NAN(x) (((*(int*)&x) & nanmask) == nanmask)

int Q_isnan(float x);

long Q_ftol(float f);
void Q_SnapVector(vec3_t vec);

#if idppc

static ID_INLINE float Q_rsqrt(float number) {
    float x = 0.5f * number;
    float y;
#ifdef __GNUC__
    asm("frsqrte %0,%1" : "=f"(y) : "f"(number));
#else
    y = __frsqrte(number);
#endif
    return y * (1.5f - (x * y * y));
}

#ifdef __GNUC__
static ID_INLINE float Q_fabs(float x) {
    float abs_x;

    asm("fabs %0,%1" : "=f"(abs_x) : "f"(x));
    return abs_x;
}
#else
#define Q_fabs __fabsf
#endif

#else
float Q_fabs(float f);
float Q_rsqrt(float f);
#endif

#define SQRTFAST(x) ((x) * Q_rsqrt(x))

signed char ClampChar(int i);
signed short ClampShort(int i);

int DirToByte(vec3_t dir);
void ByteToDir(int b, vec3_t dir);

#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorSubtract(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define VectorAdd(a, b, c) ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorCopy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorScale(v, s, o) ((o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s))
#define VectorMA(v, s, b, o) ((o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s))

#define VectorClear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, b) ((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define Vector4Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])

#define Byte4Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])

#define QuatCopy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])

#define SnapVector(v)         \
    {                         \
        v[0] = ((int)(v[0])); \
        v[1] = ((int)(v[1])); \
        v[2] = ((int)(v[2])); \
    }
vec_t _DotProduct(const vec3_t v1, const vec3_t v2);
void _VectorSubtract(const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorAdd(const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorCopy(const vec3_t in, vec3_t out);
void _VectorScale(const vec3_t in, float scale, vec3_t out);
void _VectorMA(const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc);

unsigned ColorBytes3(float r, float g, float b);
unsigned ColorBytes4(float r, float g, float b, float a);

float NormalizeColor(const vec3_t in, vec3_t out);

float RadiusFromBounds(const vec3_t mins, const vec3_t maxs);
void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);

static ID_INLINE int VectorCompare(const vec3_t v1, const vec3_t v2) {
    if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
        return 0;
    }
    return 1;
}

static ID_INLINE vec_t VectorLength(const vec3_t v) {
    return (vec_t)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static ID_INLINE vec_t VectorLengthSquared(const vec3_t v) {
    return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static ID_INLINE vec_t Distance(const vec3_t p1, const vec3_t p2) {
    vec3_t v;

    VectorSubtract(p2, p1, v);
    return VectorLength(v);
}

static ID_INLINE vec_t DistanceSquared(const vec3_t p1, const vec3_t p2) {
    vec3_t v;

    VectorSubtract(p2, p1, v);
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

static ID_INLINE void VectorNormalizeFast(vec3_t v) {
    float ilength;

    ilength = Q_rsqrt(DotProduct(v, v));

    v[0] *= ilength;
    v[1] *= ilength;
    v[2] *= ilength;
}

static ID_INLINE void VectorInverse(vec3_t v) {
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
}

static ID_INLINE void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross) {
    cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
    cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
    cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t VectorNormalize(vec3_t v);
vec_t VectorNormalize2(const vec3_t v, vec3_t out);
void Vector4Scale(const vec4_t in, vec_t scale, vec4_t out);
void VectorRotate(vec3_t in, vec3_t matrix[3], vec3_t out);
int Q_log2(int val);

float Q_acos(float c);

int Q_rand(int* seed);
float Q_random(int* seed);
float Q_crandom(int* seed);

#define random() ((rand() & 0x7fff) / ((float)0x7fff))
#define crandom() (2.0 * (random() - 0.5))

void vectoangles(const vec3_t value1, vec3_t angles);
void AnglesToAxis(const vec3_t angles, vec3_t axis[3]);

void AxisClear(vec3_t axis[3]);
void AxisCopy(vec3_t in[3], vec3_t out[3]);

void SetPlaneSignbits(struct cplane_s* out);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s* plane);

qboolean BoundsIntersect(const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsIntersectSphere(const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec_t radius);
qboolean BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs, const vec3_t origin);

float AngleMod(float a);
float LerpAngle(float from, float to, float frac);
float AngleSubtract(float a1, float a2);
void AnglesSubtract(vec3_t v1, vec3_t v2, vec3_t v3);

float AngleNormalize360(float angle);
float AngleNormalize180(float angle);
float AngleDelta(float angle1, float angle2);

qboolean PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c);
void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
void RotateAroundDirection(vec3_t axis[3], float yaw);
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);

void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void PerpendicularVector(vec3_t dst, const vec3_t src);

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

float Com_Clamp(float min, float max, float value);
char* COM_SkipPath(char* pathname);
const char* COM_GetExtension(const char* name);
void COM_StripExtension(const char* in, char* out, int destsize);
qboolean COM_CompareExtension(const char* in, const char* ext);
void COM_DefaultExtension(char* path, int maxSize, const char* extension);

void COM_BeginParseSession(const char* name);
int COM_GetCurrentParseLine(void);
char* COM_Parse(char** data_p);
char* COM_ParseExt(char** data_p, qboolean allowLineBreak);
int COM_Compress(char* data_p);
void COM_ParseError(char* format, ...) __attribute__((format(printf, 1, 2)));
void COM_ParseWarning(char* format, ...) __attribute__((format(printf, 1, 2)));

#define MAX_TOKENLENGTH 1024

#ifndef TT_STRING
#define TT_STRING 1
#define TT_LITERAL 2
#define TT_NUMBER 3
#define TT_NAME 4
#define TT_PUNCTUATION 5
#endif

typedef struct pc_token_s {
    int type;
    int subtype;
    int intvalue;
    float floatvalue;
    char string[MAX_TOKENLENGTH];
} pc_token_t;

void COM_MatchToken(char** buf_p, char* match);

qboolean SkipBracedSection(char** program, int depth);
void SkipRestOfLine(char** data);

void Parse1DMatrix(char** buf_p, int x, float* m);
void Parse2DMatrix(char** buf_p, int y, int x, float* m);
void Parse3DMatrix(char** buf_p, int z, int y, int x, float* m);
int Com_HexStrToInt(const char* str);

int QDECL Com_sprintf(char* dest, int size, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

char* Com_SkipTokens(char* s, int numTokens, char* sep);
char* Com_SkipCharset(char* s, char* sep);

void Com_RandomBytes(byte* string, int len);

typedef enum {
    FS_READ,
    FS_WRITE,
    FS_APPEND,
    FS_APPEND_SYNC
} fsMode_t;

typedef enum {
    FS_SEEK_CUR,
    FS_SEEK_END,
    FS_SEEK_SET
} fsOrigin_t;

int Q_isprint(int c);
int Q_islower(int c);
int Q_isupper(int c);
int Q_isalpha(int c);
qboolean Q_isanumber(const char* s);
qboolean Q_isintegral(float f);

int Q_stricmp(const char* s1, const char* s2);
int Q_strncmp(const char* s1, const char* s2, int n);
int Q_stricmpn(const char* s1, const char* s2, int n);
char* Q_strlwr(char* s1);
char* Q_strupr(char* s1);
const char* Q_stristr(const char* s, const char* find);

char* Q_strreplace(char* s, const char* s1, const char* s2);

void Q_strncpyz(char* dest, const char* src, int destsize);
void Q_strcat(char* dest, int size, const char* src);

int Q_PrintStrlen(const char* string);
int Q_UTF8_Encode(char* out, int codepoint);
int Q_UTF8_Start(const char* s, int pos);
int Q_UTF8_Next(const char* s, int pos);
char* Q_CleanStr(char* string);
int Q_CountChar(const char* string, char tocount);

typedef struct
{
    byte b0;
    byte b1;
    byte b2;
    byte b3;
    byte b4;
    byte b5;
    byte b6;
    byte b7;
} qint64;

char* QDECL va(char* format, ...) __attribute__((format(printf, 1, 2)));

#define TRUNCATE_LENGTH 64
void Com_TruncateLongString(char* buffer, const char* s);

char* Info_ValueForKey(const char* s, const char* key);
void Info_RemoveKey(char* s, const char* key);
void Info_RemoveKey_Big(char* s, const char* key);
void Info_SetValueForKey(char* s, const char* key, const char* value);
void Info_SetValueForKey_Big(char* s, const char* key, const char* value);
qboolean Info_Validate(const char* s);
void Info_NextPair(const char** s, char* key, char* value);

void QDECL Com_Error(int level, const char* error, ...) __attribute__((noreturn, format(printf, 2, 3)));
void QDECL Com_Printf(const char* msg, ...) __attribute__((format(printf, 1, 2)));

#define CVAR_ARCHIVE 0x0001
#define CVAR_USERINFO 0x0002
#define CVAR_SERVERINFO 0x0004
#define CVAR_SYSTEMINFO 0x0008
#define CVAR_INIT 0x0010
#define CVAR_LATCH 0x0020
#define CVAR_ROM 0x0040
#define CVAR_USER_CREATED 0x0080
#define CVAR_TEMP 0x0100
#define CVAR_CHEAT 0x0200
#define CVAR_NORESTART 0x0400

#define CVAR_REPLICATE 0x0800
#define CVAR_VM_CREATED 0x1000
#define CVAR_PROTECTED 0x2000

#define CVAR_GAMERULE_PMOVE  0x004000
#define CVAR_GAMERULE_WEAPON 0x008000
#define CVAR_GAMERULE_MODEL  0x010000
#define CVAR_GAMERULE_ARMOR  0x020000
#define CVAR_GAMERULE_REPL   0x040000
#define CVAR_GAMERULE        0x100000

#define CVAR_USERSAVE        0x080000
#define CVAR_MODIFIED 0x40000000
#define CVAR_NONEXISTENT 0x80000000

typedef struct cvar_s cvar_t;

struct cvar_s {
    char* name;
    char* string;
    char* resetString;
    char* latchedString;
    char* factoryString;
    int flags;
    qboolean modified;
    int modificationCount;
    float value;
    int integer;
    qboolean validate;
    qboolean integral;
    float min;
    float max;
    char* description;
    char* minString;
    char* maxString;

    cvar_t* next;
    cvar_t* prev;
    cvar_t* hashNext;
    cvar_t* hashPrev;
    int hashIndex;
};

#define MAX_CVAR_VALUE_STRING 256

typedef int cvarHandle_t;

typedef struct {
    cvarHandle_t handle;
    int modificationCount;
    float value;
    int integer;
    char string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

#include "surfaceflags.h"

#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2
#define PLANE_NON_AXIAL 3

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)))

typedef struct cplane_s {
    vec3_t normal;
    float dist;
    byte type;
    byte signbits;
    byte pad[2];
} cplane_t;

typedef struct {
    qboolean allsolid;
    qboolean startsolid;
    float fraction;
    vec3_t endpos;
    cplane_t plane;
    int surfaceFlags;
    int contents;
    int entityNum;
} trace_t;

typedef struct {
    int firstPoint;
    int numPoints;
} markFragment_t;

typedef struct {
    vec3_t origin;
    vec3_t axis[3];
} orientation_t;

#define KEYCATCH_CONSOLE 0x0001
#define KEYCATCH_UI 0x0002
#define KEYCATCH_MESSAGE 0x0004
#define KEYCATCH_CGAME 0x0008

typedef enum {
    CHAN_AUTO,
    CHAN_LOCAL,
    CHAN_WEAPON,
    CHAN_VOICE,
    CHAN_ITEM,
    CHAN_BODY,
    CHAN_LOCAL_SOUND,
    CHAN_ANNOUNCER
} soundChannel_t;

#define ANGLE2SHORT(x) ((int)((x) * 65536 / 360) & 65535)
#define SHORT2ANGLE(x) ((x) * (360.0 / 65536))

#define SNAPFLAG_RATE_DELAYED 1
#define SNAPFLAG_NOT_ACTIVE 2
#define SNAPFLAG_SERVERCOUNT 4

#define MAX_CLIENTS 64
#define MAX_LOCATIONS 64

#define GENTITYNUM_BITS 10
#define MAX_GENTITIES (1 << GENTITYNUM_BITS)

#define ENTITYNUM_NONE (MAX_GENTITIES - 1)
#define ENTITYNUM_WORLD (MAX_GENTITIES - 2)
#define ENTITYNUM_MAX_NORMAL (MAX_GENTITIES - 2)

#define MAX_MODELS 256
#define MAX_SOUNDS 256

#define MAX_CONFIGSTRINGS 1024

#define CS_SERVERINFO 0
#define CS_SYSTEMINFO 1

#define RESERVED_CONFIGSTRINGS 2

#define MAX_GAMESTATE_CHARS 16000
typedef struct {
    int stringOffsets[MAX_CONFIGSTRINGS];
    char stringData[MAX_GAMESTATE_CHARS];
    int dataCount;
} gameState_t;

#define MAX_STATS 16
#define MAX_PERSISTANT 16
#define MAX_POWERUPS 16
#define MAX_WEAPONS 16

#define MAX_PS_EVENTS 2

#define PS_PMOVEFRAMECOUNTBITS 6

typedef struct playerState_s {
    int commandTime;
    int pm_type;
    int bobCycle;
    int pm_flags;
    int pm_time;

    vec3_t origin;
    vec3_t velocity;
    int weaponTime;
    int gravity;
    int speed;
    int delta_angles[3];

    int groundEntityNum;

    int legsTimer;
    int legsAnim;

    int torsoTimer;
    int torsoAnim;

    int movementDir;

    vec3_t grapplePoint;

    int eFlags;

    int eventSequence;
    int events[MAX_PS_EVENTS];
    int eventParms[MAX_PS_EVENTS];

    int externalEvent;
    int externalEventParm;

    int clientNum;
    int location;
    int weapon;
    int weaponPrimary;
    int weaponstate;

    int fov;
    vec3_t viewangles;
    int viewheight;

    int damageEvent;
    int damageYaw;
    int damagePitch;
    int damageCount;

    int stats[MAX_STATS];
    int persistant[MAX_PERSISTANT];
    int powerups[MAX_POWERUPS];
    int ammo[MAX_WEAPONS];

    int generic1;
    int loopSound;
    int jumppad_ent;

    int jumpTime;
    int doubleJumped;
    int crouchTime;
    int crouchSlideTime;
    char forwardmove;
    char rightmove;
    char upmove;
    char _pad;

    int ping;
    int pmove_framecount;
    int jumppad_frame;
    int entityEventSequence;

    int freezetime;
    int thawtime;
    int thawClientNum_valid;
    int thawClientNum;

    int respawnTime;
    int localPersistant[MAX_PERSISTANT];
    int roundDamage;
    int roundShots;
    int roundHits;
} playerState_t;

#define BUTTON_ATTACK 1
#define BUTTON_TALK 2
#define BUTTON_USE_HOLDABLE 4
#define BUTTON_GESTURE 8
#define BUTTON_WALKING 16
#define BUTTON_AFFIRMATIVE 32
#define BUTTON_NEGATIVE 64

#define BUTTON_GETFLAG 128
#define BUTTON_GUARDBASE 256
#define BUTTON_PATROL 512
#define BUTTON_FOLLOWME 1024

#define BUTTON_JUMP 2048
#define BUTTON_ANY 4096

#define MOVE_RUN 120

typedef struct usercmd_s {
    int serverTime;
    int angles[3];
    int buttons;
    byte weapon;
    byte weaponPrimary;
    byte fov;
    signed char forwardmove;
    signed char rightmove;
    signed char upmove;
} usercmd_t;

#define SOLID_BMODEL 0xffffff

typedef enum {
    TR_STATIONARY,
    TR_INTERPOLATE,
    TR_LINEAR,
    TR_LINEAR_STOP,
    TR_SINE,
    TR_GRAVITY,
    TR_CUSTOM_GRAVITY
} trType_t;

typedef struct {
    trType_t trType;
    int trTime;
    int trDuration;
    vec3_t trBase;
    vec3_t trDelta;
    float gravity;
} trajectory_t;

typedef struct entityState_s {
    int number;
    int eType;
    int eFlags;

    trajectory_t pos;
    trajectory_t apos;

    int time;
    int time2;

    vec3_t origin;
    vec3_t origin2;

    vec3_t angles;
    vec3_t angles2;

    int otherEntityNum;
    int otherEntityNum2;

    int groundEntityNum;

    int constantLight;
    int loopSound;

    int modelindex;
    int modelindex2;
    int clientNum;
    int frame;

    int solid;

    int event;
    int eventParm;

    int powerups;
    int health;
    int armor;
    int weapon;
    int location;
    int legsAnim;
    int torsoAnim;

    int generic1;
    int jumpTime;
    int doubleJumped;
} entityState_t;

typedef enum {
    CA_UNINITIALIZED,
    CA_DISCONNECTED,
    CA_AUTHORIZING,
    CA_CONNECTING,
    CA_CHALLENGING,
    CA_CONNECTED,
    CA_LOADING,
    CA_PRIMED,
    CA_ACTIVE,
    CA_CINEMATIC
} connstate_t;

#define DEFAULT_FONT "fonts/handelgothic.ttf"
#define DEFAULT_SANS_FONT "fonts/notosans-regular.ttf"
#define DEFAULT_MONO_FONT "fonts/droidsansmono.ttf"
#define TEXT_LITERAL 0x40000000
#define TEXT_NORECOLOR 0x20000000
#define GLYPH_START 0
#define GLYPH_END 255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND 127
#define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1
typedef struct {
    int height;
    int top;
    int bottom;
    int pitch;
    int left;
    int xSkip;
    int imageWidth;
    int imageHeight;
    float s;
    float t;
    float s2;
    float t2;
    qhandle_t glyph;
    char shaderName[32];
} glyphInfo_t;

typedef struct extraGlyphInfo_s {
    int charValue;

    int height;
    int top;
    int bottom;
    int pitch;
    int left;
    int xSkip;
    int imageWidth;
    int imageHeight;
    float s;
    float t;
    float s2;
    float t2;
    qhandle_t glyph;
    char shaderName[32];

    struct extraGlyphInfo_s *prev, *next;
} extraGlyphInfo_t;

typedef struct {
    glyphInfo_t glyphs[GLYPHS_PER_FONT];
    float glyphScale;
    char name[MAX_QPATH];
} fontInfo_t;

#define Square(x) ((x) * (x))

typedef struct qtime_s {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
} qtime_t;

#define AS_LOCAL 0
#define AS_MPLAYER 1
#define AS_GLOBAL 2
#define AS_FAVORITES 3

typedef enum {
    FMV_IDLE,
    FMV_PLAY,
    FMV_EOF,
    FMV_ID_BLT,
    FMV_ID_IDLE,
    FMV_LOOPED,
    FMV_ID_WAIT
} e_status;

typedef enum _flag_status {
    FLAG_ATBASE = 0,
    FLAG_TAKEN,
    FLAG_DROPPED,
    FLAG_TAKEN_RED,
    FLAG_TAKEN_BLUE
} flagStatus_t;

#define MAX_GLOBAL_SERVERS 4096
#define MAX_OTHER_SERVERS 128
#define MAX_PINGREQUESTS 32
#define MAX_SERVERSTATUSREQUESTS 16

#define SAY_ALL 0
#define SAY_TEAM 1
#define SAY_TELL 2

#define MAX_MAP_ADVERTISEMENTS 30

#define LERP(a, b, w) ((a) * (1.0f - (w)) + (b) * (w))
#define LUMA(red, green, blue) (0.2126f * (red) + 0.7152f * (green) + 0.0722f * (blue))

#endif
