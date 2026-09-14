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
// sv_factories.c -- [QL] arenas, factories and the map pool
//
// Quake Live's engine keeps three tables the game modes are built from
// (quakelive_steam.exe 0x45e570..0x45fc26):
// - arenas: every map from scripts/arenas.txt and scripts/*.arena, with the
//   gametypes its "type" key names;
// - factories: gametype presets from scripts/factories.txt and
//   scripts/*.factories, a JSON array of objects with an id, a base gametype,
//   a title and the cvars they set;
// - the map pool: "map|factory" lines from sv_mapPoolFile, which feed nextmap,
//   startRandomMap and the maps offered at the end of a match.
// "map <map> <factory>" (sv_ccmds.c) applies a factory: it restores the cvars
// the previous one changed, saves the current value of each cvar the new one
// sets, sets them and then g_gametype, g_factory and g_factoryTitle.

#include "server.h"

cvar_t* sv_mapPoolFile;
cvar_t* sv_includeCurrentMapInVote;

// the base gametypes a factory names, in g_gametype order (0x55ca54)
static const char* sv_factoryGametypes[NUM_FACTORY_GAMETYPES] = {
    "ffa", "duel", "race", "tdm", "ca", "ctf", "oneflag", "overload", "har", "ft", "dom", "ad", "rr"};

static arenaInfo_t sv_arenas[MAX_ARENA_INFOS];      // 0xc1e1f8
static int sv_numArenas;                             // 0xe2b200

static factory_t sv_factories[MAX_FACTORIES];       // 0xa111f8
static int sv_numFactories;                          // 0xe2b1fc

static mapPoolEntry_t sv_mapPool[MAX_MAPPOOL];       // 0x6111f8
static int sv_numMapPool;                            // 0xe2b1f8

/*
===============================================================================

JSON READER

The engine parsed these files with jsoncpp 0.x (Json::Reader, comments
allowed, root of any type). This reader follows its rules, so the same files
parse or fail: comments, strict separators, "true"/"false"/"null", integers
that fit in 64 bits kept as integers, and objects kept sorted by member name
with the last duplicate winning, as std::map does. isArray and isObject also
accept null, as in that version (0x42a700, 0x42a720).

===============================================================================
*/

typedef enum {
    JV_NULL,    // same order as Json::ValueType
    JV_INT,
    JV_UINT,
    JV_REAL,
    JV_STRING,
    JV_BOOL,
    JV_ARRAY,
    JV_OBJECT
} jsonValueType_t;

typedef struct jsonValue_s {
    jsonValueType_t type;
    int64_t i;
    uint64_t u;
    double d;
    qboolean b;
    char* s;
    char* key;                    // member name, when this value is an object member
    struct jsonValue_s* first;    // array elements in order, object members sorted by key
    struct jsonValue_s* next;
} jsonValue_t;

typedef enum {
    JT_OBJECT_BEGIN,
    JT_OBJECT_END,
    JT_ARRAY_BEGIN,
    JT_ARRAY_END,
    JT_STRING,
    JT_NUMBER,
    JT_TRUE,
    JT_FALSE,
    JT_NULL,
    JT_ARRAY_SEPARATOR,
    JT_MEMBER_SEPARATOR,
    JT_COMMENT,
    JT_END_OF_STREAM,
    JT_ERROR
} jsonTokenType_t;

typedef struct {
    jsonTokenType_t type;
    const char* start;
    const char* end;
} jsonToken_t;

typedef struct {
    const char* current;
    const char* end;
} jsonReader_t;

static jsonValue_t* JSON_NewValue(void) {
    jsonValue_t* v = Z_Malloc(sizeof(*v));
    return v;
}

static void JSON_ClearValue(jsonValue_t* v) {
    jsonValue_t* child;
    jsonValue_t* next;

    for (child = v->first; child; child = next) {
        next = child->next;
        JSON_ClearValue(child);
        if (child->key) {
            Z_Free(child->key);
        }
        Z_Free(child);
    }
    if (v->s) {
        Z_Free(v->s);
    }
    v->first = NULL;
    v->s = NULL;
    v->type = JV_NULL;
}

static void JSON_FreeValue(jsonValue_t* v) {
    if (!v) {
        return;
    }
    JSON_ClearValue(v);
    Z_Free(v);
}

static char JSON_GetNextChar(jsonReader_t* r) {
    if (r->current == r->end) {
        return 0;
    }
    return *r->current++;
}

static void JSON_SkipSpaces(jsonReader_t* r) {
    while (r->current != r->end) {
        char c = *r->current;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++r->current;
        } else {
            break;
        }
    }
}

static qboolean JSON_Match(jsonReader_t* r, const char* pattern, int patternLength) {
    int index;

    if (r->end - r->current < patternLength) {
        return qfalse;
    }
    for (index = patternLength; index--;) {
        if (r->current[index] != pattern[index]) {
            return qfalse;
        }
    }
    r->current += patternLength;
    return qtrue;
}

static qboolean JSON_ReadCStyleComment(jsonReader_t* r) {
    while (r->current != r->end) {
        char c = JSON_GetNextChar(r);
        if (c == '*' && *r->current == '/') {
            break;
        }
    }
    return JSON_GetNextChar(r) == '/';
}

static qboolean JSON_ReadCppStyleComment(jsonReader_t* r) {
    while (r->current != r->end) {
        char c = JSON_GetNextChar(r);
        if (c == '\r' || c == '\n') {
            break;
        }
    }
    return qtrue;
}

static qboolean JSON_ReadComment(jsonReader_t* r) {
    char c = JSON_GetNextChar(r);

    if (c == '*') {
        return JSON_ReadCStyleComment(r);
    }
    if (c == '/') {
        return JSON_ReadCppStyleComment(r);
    }
    return qfalse;
}

static qboolean JSON_ReadStringToken(jsonReader_t* r) {
    char c = 0;

    while (r->current != r->end) {
        c = JSON_GetNextChar(r);
        if (c == '\\') {
            JSON_GetNextChar(r);
        } else if (c == '"') {
            break;
        }
    }
    return c == '"';
}

static void JSON_ReadNumberToken(jsonReader_t* r) {
    while (r->current != r->end) {
        char c = *r->current;
        if (!(c >= '0' && c <= '9') && c != '.' && c != 'e' && c != 'E' && c != '+' && c != '-') {
            break;
        }
        ++r->current;
    }
}

static qboolean JSON_ReadToken(jsonReader_t* r, jsonToken_t* token) {
    qboolean ok = qtrue;
    char c;

    JSON_SkipSpaces(r);
    token->start = r->current;
    c = JSON_GetNextChar(r);
    switch (c) {
    case '{':
        token->type = JT_OBJECT_BEGIN;
        break;
    case '}':
        token->type = JT_OBJECT_END;
        break;
    case '[':
        token->type = JT_ARRAY_BEGIN;
        break;
    case ']':
        token->type = JT_ARRAY_END;
        break;
    case '"':
        token->type = JT_STRING;
        ok = JSON_ReadStringToken(r);
        break;
    case '/':
        token->type = JT_COMMENT;
        ok = JSON_ReadComment(r);
        break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '-':
        token->type = JT_NUMBER;
        JSON_ReadNumberToken(r);
        break;
    case 't':
        token->type = JT_TRUE;
        ok = JSON_Match(r, "rue", 3);
        break;
    case 'f':
        token->type = JT_FALSE;
        ok = JSON_Match(r, "alse", 4);
        break;
    case 'n':
        token->type = JT_NULL;
        ok = JSON_Match(r, "ull", 3);
        break;
    case ',':
        token->type = JT_ARRAY_SEPARATOR;
        break;
    case ':':
        token->type = JT_MEMBER_SEPARATOR;
        break;
    case 0:
        token->type = JT_END_OF_STREAM;
        break;
    default:
        ok = qfalse;
        break;
    }
    if (!ok) {
        token->type = JT_ERROR;
    }
    token->end = r->current;
    return qtrue;
}

static void JSON_SkipCommentTokens(jsonReader_t* r, jsonToken_t* token) {
    do {
        JSON_ReadToken(r, token);
    } while (token->type == JT_COMMENT);
}

static unsigned int JSON_DecodeHex4(const char** current, const char* end, qboolean* ok) {
    unsigned int unicode = 0;
    int index;

    if (end - *current < 4) {
        *ok = qfalse;
        return 0;
    }
    for (index = 0; index < 4; ++index) {
        char c = *(*current)++;
        unicode *= 16;
        if (c >= '0' && c <= '9') {
            unicode += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            unicode += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            unicode += c - 'A' + 10;
        } else {
            *ok = qfalse;
            return 0;
        }
    }
    return unicode;
}

// appends the UTF-8 form of a code point; returns the new length
static int JSON_AppendUTF8(char* out, int len, unsigned int cp) {
    if (cp <= 0x7f) {
        out[len++] = (char)cp;
    } else if (cp <= 0x7ff) {
        out[len++] = (char)(0xc0 | (0x1f & (cp >> 6)));
        out[len++] = (char)(0x80 | (0x3f & cp));
    } else if (cp <= 0xffff) {
        out[len++] = (char)(0xe0 | (0x0f & (cp >> 12)));
        out[len++] = (char)(0x80 | (0x3f & (cp >> 6)));
        out[len++] = (char)(0x80 | (0x3f & cp));
    } else if (cp <= 0x10ffff) {
        out[len++] = (char)(0xf0 | (0x07 & (cp >> 18)));
        out[len++] = (char)(0x80 | (0x3f & (cp >> 12)));
        out[len++] = (char)(0x80 | (0x3f & (cp >> 6)));
        out[len++] = (char)(0x80 | (0x3f & cp));
    }
    return len;
}

// decodes a string token without its quotes into a new Z_Malloc'd string
static char* JSON_DecodeString(const jsonToken_t* token) {
    const char* current = token->start + 1;   // skip '"'
    const char* end = token->end - 1;         // do not include '"'
    char* out = Z_Malloc((end - current) * 2 + 1);
    int len = 0;

    while (current != end) {
        char c = *current++;
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            char escape;

            if (current == end) {
                Z_Free(out);
                return NULL;
            }
            escape = *current++;
            switch (escape) {
            case '"':
                out[len++] = '"';
                break;
            case '/':
                out[len++] = '/';
                break;
            case '\\':
                out[len++] = '\\';
                break;
            case 'b':
                out[len++] = '\b';
                break;
            case 'f':
                out[len++] = '\f';
                break;
            case 'n':
                out[len++] = '\n';
                break;
            case 'r':
                out[len++] = '\r';
                break;
            case 't':
                out[len++] = '\t';
                break;
            case 'u': {
                qboolean ok = qtrue;
                unsigned int unicode = JSON_DecodeHex4(&current, end, &ok);

                if (ok && unicode >= 0xd800 && unicode <= 0xdbff) {
                    // surrogate pair
                    if (end - current < 6 || current[0] != '\\' || current[1] != 'u') {
                        ok = qfalse;
                    } else {
                        unsigned int low;
                        current += 2;
                        low = JSON_DecodeHex4(&current, end, &ok);
                        unicode = 0x10000 + ((unicode & 0x3ff) << 10) + (low & 0x3ff);
                    }
                }
                if (!ok) {
                    Z_Free(out);
                    return NULL;
                }
                len = JSON_AppendUTF8(out, len, unicode);
                break;
            }
            default:
                Z_Free(out);
                return NULL;
            }
        } else {
            out[len++] = c;
        }
    }
    out[len] = '\0';
    return out;
}

static qboolean JSON_DecodeDouble(const jsonToken_t* token, jsonValue_t* value) {
    char* buffer;
    int length = token->end - token->start;
    int count;
    double d = 0.0;

    if (length < 0) {
        return qfalse;
    }
    buffer = Z_Malloc(length + 1);
    memcpy(buffer, token->start, length);
    buffer[length] = '\0';
    count = sscanf(buffer, "%lf", &d);
    Z_Free(buffer);
    if (count != 1) {
        return qfalse;
    }
    value->type = JV_REAL;
    value->d = d;
    return qtrue;
}

static qboolean JSON_DecodeNumber(const jsonToken_t* token, jsonValue_t* value) {
    const char* inspect;
    const char* current;
    qboolean isDouble = qfalse;
    qboolean isNegative;
    uint64_t maxIntegerValue, threshold, v = 0;

    for (inspect = token->start; inspect != token->end; ++inspect) {
        isDouble = isDouble || *inspect == '.' || *inspect == 'e' || *inspect == 'E' || *inspect == '+'
                   || (*inspect == '-' && inspect != token->start);
    }
    if (isDouble) {
        return JSON_DecodeDouble(token, value);
    }

    current = token->start;
    isNegative = *current == '-';
    if (isNegative) {
        ++current;
    }
    maxIntegerValue = isNegative ? 0x8000000000000000ULL : 0xffffffffffffffffULL;
    threshold = maxIntegerValue / 10;
    while (current < token->end) {
        char c = *current++;
        unsigned int digit;

        if (c < '0' || c > '9') {
            return qfalse;
        }
        digit = c - '0';
        if (v >= threshold) {
            if (v > threshold || current != token->end || digit > maxIntegerValue % 10) {
                return JSON_DecodeDouble(token, value);
            }
        }
        v = v * 10 + digit;
    }
    if (isNegative) {
        value->type = JV_INT;
        value->i = -(int64_t)v;
    } else if (v <= 0x7fffffffULL) {
        value->type = JV_INT;
        value->i = (int64_t)v;
    } else {
        value->type = JV_UINT;
        value->u = v;
    }
    return qtrue;
}

// the member named key of an object, created as null if it is not there
static jsonValue_t* JSON_ObjectMember(jsonValue_t* object, const char* key) {
    jsonValue_t** link = &object->first;
    jsonValue_t* member;

    while (*link) {
        int cmp = strcmp((*link)->key, key);
        if (cmp == 0) {
            return *link;
        }
        if (cmp > 0) {
            break;
        }
        link = &(*link)->next;
    }
    member = JSON_NewValue();
    member->key = CopyString(key);
    member->next = *link;
    *link = member;
    return member;
}

static qboolean JSON_ReadValue(jsonReader_t* r, jsonValue_t* value);

static qboolean JSON_ReadObject(jsonReader_t* r, jsonValue_t* value) {
    jsonToken_t tokenName;
    char* name = NULL;
    qboolean nameEmpty = qtrue;

    JSON_ClearValue(value);
    value->type = JV_OBJECT;
    while (JSON_ReadToken(r, &tokenName)) {
        jsonToken_t colon, comma;
        jsonValue_t* member;
        qboolean ok;

        while (tokenName.type == JT_COMMENT) {
            JSON_ReadToken(r, &tokenName);
        }
        if (tokenName.type == JT_OBJECT_END && nameEmpty) {
            return qtrue;  // empty object
        }
        if (tokenName.type != JT_STRING) {
            break;
        }
        name = JSON_DecodeString(&tokenName);
        if (!name) {
            return qfalse;
        }
        nameEmpty = name[0] == '\0';
        JSON_ReadToken(r, &colon);
        if (colon.type != JT_MEMBER_SEPARATOR) {
            Z_Free(name);
            return qfalse;
        }
        member = JSON_ObjectMember(value, name);
        Z_Free(name);
        ok = JSON_ReadValue(r, member);
        if (!ok) {
            return qfalse;
        }
        JSON_ReadToken(r, &comma);
        if (comma.type != JT_OBJECT_END && comma.type != JT_ARRAY_SEPARATOR && comma.type != JT_COMMENT) {
            return qfalse;
        }
        while (comma.type == JT_COMMENT) {
            JSON_ReadToken(r, &comma);
        }
        if (comma.type == JT_OBJECT_END) {
            return qtrue;
        }
    }
    return qfalse;  // missing '}' or object member name
}

static qboolean JSON_ReadArray(jsonReader_t* r, jsonValue_t* value) {
    jsonValue_t** link;

    JSON_ClearValue(value);
    value->type = JV_ARRAY;
    JSON_SkipSpaces(r);
    if (r->current != r->end && *r->current == ']') {
        jsonToken_t endArray;
        JSON_ReadToken(r, &endArray);
        return qtrue;
    }
    link = &value->first;
    for (;;) {
        jsonValue_t* element = JSON_NewValue();
        jsonToken_t token;

        *link = element;
        link = &element->next;
        if (!JSON_ReadValue(r, element)) {
            return qfalse;
        }
        JSON_ReadToken(r, &token);
        while (token.type == JT_COMMENT) {
            JSON_ReadToken(r, &token);
        }
        if (token.type != JT_ARRAY_SEPARATOR && token.type != JT_ARRAY_END) {
            return qfalse;  // missing ',' or ']' in array declaration
        }
        if (token.type == JT_ARRAY_END) {
            break;
        }
    }
    return qtrue;
}

static qboolean JSON_ReadValue(jsonReader_t* r, jsonValue_t* value) {
    jsonToken_t token;

    JSON_SkipCommentTokens(r, &token);
    switch (token.type) {
    case JT_OBJECT_BEGIN:
        return JSON_ReadObject(r, value);
    case JT_ARRAY_BEGIN:
        return JSON_ReadArray(r, value);
    case JT_NUMBER:
        JSON_ClearValue(value);
        return JSON_DecodeNumber(&token, value);
    case JT_STRING: {
        char* s = JSON_DecodeString(&token);
        if (!s) {
            return qfalse;
        }
        JSON_ClearValue(value);
        value->type = JV_STRING;
        value->s = s;
        return qtrue;
    }
    case JT_TRUE:
    case JT_FALSE:
        JSON_ClearValue(value);
        value->type = JV_BOOL;
        value->b = (token.type == JT_TRUE);
        return qtrue;
    case JT_NULL:
        JSON_ClearValue(value);
        return qtrue;
    default:
        return qfalse;  // syntax error: value, object or array expected
    }
}

// parses a whole document; anything after the root value is ignored, as the
// engine's reader did
static jsonValue_t* JSON_Parse(const char* text, int length) {
    jsonReader_t r;
    jsonValue_t* root = JSON_NewValue();

    r.current = text;
    r.end = text + length;
    if (!JSON_ReadValue(&r, root)) {
        JSON_FreeValue(root);
        return NULL;
    }
    return root;
}

static qboolean JSON_IsString(const jsonValue_t* v) {
    return v && v->type == JV_STRING;
}

static qboolean JSON_IsArray(const jsonValue_t* v) {
    return v && (v->type == JV_NULL || v->type == JV_ARRAY);
}

static qboolean JSON_IsObject(const jsonValue_t* v) {
    return v && (v->type == JV_NULL || v->type == JV_OBJECT);
}

// Json::Value::isIntegral of that version: int, uint and bool (0x42a6b0)
static qboolean JSON_IsIntegral(const jsonValue_t* v) {
    return v && (v->type == JV_INT || v->type == JV_UINT || v->type == JV_BOOL);
}

// isNumeric: isIntegral or real (0x42a6d0)
static qboolean JSON_IsNumeric(const jsonValue_t* v) {
    return JSON_IsIntegral(v) || (v && v->type == JV_REAL);
}

// the member of an object, or NULL (a null Value for the engine) if there is none
static const jsonValue_t* JSON_Get(const jsonValue_t* object, const char* key) {
    const jsonValue_t* member;

    if (!object || object->type != JV_OBJECT) {
        return NULL;
    }
    for (member = object->first; member; member = member->next) {
        if (!strcmp(member->key, key)) {
            return member;
        }
    }
    return NULL;
}

static int JSON_AsInt(const jsonValue_t* v) {
    switch (v->type) {
    case JV_INT:
        return (int)v->i;
    case JV_UINT:
        return (int)v->u;
    case JV_REAL:
        return (int)v->d;
    case JV_BOOL:
        return v->b ? 1 : 0;
    default:
        return 0;
    }
}

static double JSON_AsDouble(const jsonValue_t* v) {
    switch (v->type) {
    case JV_INT:
        return (double)v->i;
    case JV_UINT:
        return (double)v->u;
    case JV_REAL:
        return v->d;
    case JV_BOOL:
        return v->b ? 1.0 : 0.0;
    default:
        return 0.0;
    }
}

/*
===============================================================================

ARENAS

===============================================================================
*/

/*
===============
SV_ParseInfos

Reads "{ key value ... }" blocks into info strings, at most max of them.
quakelive_steam.exe 0x45e570.
===============
*/
static int SV_ParseInfos(char* buf, int max, char* infos[]) {
    char* token;
    int count = 0;
    char key[MAX_TOKEN_CHARS];
    char info[MAX_INFO_STRING];

    while (1) {
        token = COM_Parse(&buf);
        if (!token[0]) {
            break;
        }
        if (strcmp(token, "{")) {
            Com_Printf("Missing { in info file\n");
            break;
        }
        if (count == max) {
            Com_Printf("Max infos exceeded\n");
            break;
        }

        info[0] = '\0';
        while (1) {
            token = COM_ParseExt(&buf, qtrue);
            if (!token[0]) {
                Com_Printf("Unexpected end of info file\n");
                break;
            }
            if (!strcmp(token, "}")) {
                break;
            }
            Q_strncpyz(key, token, sizeof(key));

            token = COM_ParseExt(&buf, qfalse);
            if (!token[0]) {
                strcpy(token, "<NULL>");
            }
            Info_SetValueForKey(info, key, token);
        }
        infos[count] = CopyString(info);
        count++;
    }
    return count;
}

/*
===============
SV_LoadArenasFromFile

quakelive_steam.exe 0x45ea60.
===============
*/
static void SV_LoadArenasFromFile(const char* filename) {
    fileHandle_t f;
    int len, count, i, gt;
    char buf[0x4000];
    char* infos[MAX_ARENA_INFOS];

    len = FS_FOpenFileRead(filename, &f, qfalse);
    if (!f) {
        Com_Printf("%s", va("^1file not found: %s\n^7", filename));
        return;
    }
    if (len >= 0x4000) {
        Com_Printf("%s", va("^1file too large: %s is %i, max allowed is %i^7", filename, len, 0x4000));
        FS_FCloseFile(f);
        return;
    }

    FS_Read(buf, len, f);
    buf[len] = '\0';
    FS_FCloseFile(f);

    count = SV_ParseInfos(buf, MAX_ARENA_INFOS - sv_numArenas, infos);
    for (i = 0; i < count; i++) {
        arenaInfo_t* arena = &sv_arenas[sv_numArenas];
        const char* type;

        Q_strncpyz(arena->map, Info_ValueForKey(infos[i], "map"), sizeof(arena->map));
        if (arena->map[0]) {
            Q_strncpyz(arena->longname, Info_ValueForKey(infos[i], "longname"), sizeof(arena->longname));
            if (!arena->longname[0]) {
                Q_strncpyz(arena->longname, arena->map, sizeof(arena->longname));
            }
            // a gametype is supported when its name appears anywhere in "type"
            type = Info_ValueForKey(infos[i], "type");
            for (gt = 0; gt < NUM_FACTORY_GAMETYPES; gt++) {
                arena->gametypes[gt] = strstr(type, sv_factoryGametypes[gt]) != NULL;
            }
            sv_numArenas++;
        }
        Z_Free(infos[i]);
    }
}

/*
===============
SV_LoadArenas

scripts/arenas.txt, then every .arena file in scripts. quakelive_steam.exe
0x45ec80, called from SV_Init.
===============
*/
void SV_LoadArenas(void) {
    int numFiles, i;
    char fileList[1024];
    char filename[128];
    char* name;

    SV_LoadArenasFromFile("scripts/arenas.txt");

    numFiles = FS_GetFileList("scripts", ".arena", fileList, sizeof(fileList));
    name = fileList;
    for (i = 0; i < numFiles; i++) {
        int nameLen = strlen(name);
        Com_sprintf(filename, sizeof(filename), "scripts/%s", name);
        SV_LoadArenasFromFile(filename);
        name += nameLen + 1;
    }

    Com_Printf("%s", va("%i arenas parsed\n", sv_numArenas));
}

/*
===============
SV_FindArena

quakelive_steam.exe 0x45e9f0.
===============
*/
const arenaInfo_t* SV_FindArena(const char* map) {
    int i;

    for (i = 0; i < sv_numArenas; i++) {
        if (!strcmp(sv_arenas[i].map, map)) {
            return &sv_arenas[i];
        }
    }
    return NULL;
}

int SV_NumArenas(void) {
    return sv_numArenas;
}

const arenaInfo_t* SV_ArenaByNum(int num) {
    if (num < 0 || num >= sv_numArenas) {
        return NULL;
    }
    return &sv_arenas[num];
}

/*
===============
SV_ReloadArenas_f

quakelive_steam.exe 0x45ed90.
===============
*/
void SV_ReloadArenas_f(void) {
    Com_Printf("reloading arena definitions...\n");
    memset(sv_arenas, 0, sizeof(sv_arenas));
    sv_numArenas = 0;
    SV_LoadArenas();
}

/*
===============================================================================

FACTORIES

===============================================================================
*/

/*
===============
SV_FindFactory

Case-sensitive, over every slot. quakelive_steam.exe 0x45e760.
===============
*/
factory_t* SV_FindFactory(const char* id) {
    int i;

    for (i = 0; i < MAX_FACTORIES; i++) {
        if (sv_factories[i].id && !strcmp(sv_factories[i].id, id)) {
            return &sv_factories[i];
        }
    }
    return NULL;
}

int SV_NumFactories(void) {
    return sv_numFactories;
}

const factory_t* SV_FactoryByNum(int num) {
    if (num < 0 || num >= sv_numFactories || !sv_factories[num].id) {
        return NULL;
    }
    return &sv_factories[num];
}

/*
===============
SV_PrintValidFactories

The ids of the loaded factories, but the hidden ones ("_training", "_rj",
"_sj"). quakelive_steam.exe 0x45e7d0.
===============
*/
void SV_PrintValidFactories(void) {
    int i;

    Com_Printf("Valid factories: ");
    for (i = 0; i < sv_numFactories; i++) {
        if (sv_factories[i].id && sv_factories[i].id[0] != '_') {
            Com_Printf("%s ", sv_factories[i].id);
        }
    }
    Com_Printf("\n");
}

/*
===============
SV_ParseFactory

One factory object. quakelive_steam.exe 0x45f380.
===============
*/
static void SV_ParseFactory(const jsonValue_t* object) {
    const jsonValue_t* v;
    const jsonValue_t* cvars;
    const jsonValue_t* member;
    const char* id;
    const char* basegt;
    factory_t* factory;
    int gt, i, count;

    if (!JSON_IsObject(object)) {
        Com_Printf("^1A specified factory is not an object. All factories must be JSON objects.\n^7");
        return;
    }
    v = JSON_Get(object, "id");
    if (!JSON_IsString(v)) {
        Com_Printf("^1A specified factory is missing required key \"id\", or is not a string.\n^7");
        return;
    }
    id = v->s;
    if (!JSON_IsString(JSON_Get(object, "basegt"))) {
        Com_Printf("^1Factory with id %s is missing key \"basegt\", or is not a string.\n^7", id);
        return;
    }
    if (!JSON_IsString(JSON_Get(object, "title"))) {
        Com_Printf("^1Factory with id %s is missing key \"title\", or is not a string.\n^7", id);
        return;
    }
    cvars = JSON_Get(object, "cvars");
    if (!JSON_IsObject(cvars)) {
        Com_Printf("^1Factory with id %s is missing key \"cvars\", or is not an object.\n^7", id);
        return;
    }

    basegt = JSON_Get(object, "basegt")->s;
    for (gt = 0; gt < NUM_FACTORY_GAMETYPES; gt++) {
        if (!strcmp(sv_factoryGametypes[gt], basegt)) {
            break;
        }
    }
    if (gt == NUM_FACTORY_GAMETYPES) {
        Com_Printf("^1Factory with id %s specifies an invalid basegt.\n^7", id);
        return;
    }

    // a repeated id is reported and still added; lookups keep finding the first
    if (SV_FindFactory(id)) {
        Com_Printf("^1Factory with id %s already exists.\n^7", id);
    }

    // the first free slot from the count on; the count ends one past it
    factory = NULL;
    while (sv_numFactories < MAX_FACTORIES) {
        factory_t* slot = &sv_factories[sv_numFactories++];
        if (!slot->id) {
            factory = slot;
            break;
        }
    }
    if (!factory) {
        return;
    }

    factory->id = CopyString(id);
    factory->title = CopyString(JSON_Get(object, "title")->s);
    v = JSON_Get(object, "author");
    if (JSON_IsString(v)) {
        factory->author = CopyString(v->s);
    }
    v = JSON_Get(object, "description");
    if (JSON_IsString(v)) {
        factory->description = CopyString(v->s);
    }
    factory->basegt = gt;

    // up to eight tags, looking at the first eight elements only
    v = JSON_Get(object, "tags");
    if (v && v->type == JV_ARRAY) {
        count = 0;
        for (member = v->first, i = 0; member && i < MAX_FACTORY_TAGS; member = member->next, i++) {
            if (JSON_IsString(member)) {
                factory->tags[count++] = CopyString(member->s);
            }
        }
    }

    // the cvars in member name order; a value that is not a number, a boolean
    // or a string is left unset, which ends the list when it is applied
    if (cvars && cvars->type == JV_OBJECT) {
        for (member = cvars->first, i = 0; member && i < MAX_FACTORY_CVARS; member = member->next, i++) {
            factory->cvars[i].name = CopyString(member->key);
            if (JSON_IsIntegral(member)) {
                factory->cvars[i].value = CopyString(va("%i", JSON_AsInt(member)));
            } else if (JSON_IsString(member)) {
                factory->cvars[i].value = CopyString(member->s);
            } else if (JSON_IsNumeric(member)) {
                factory->cvars[i].value = CopyString(va("%f", JSON_AsDouble(member)));
            }
        }
    }
}

/*
===============
SV_LoadFactoriesFromFile

quakelive_steam.exe 0x45f800.
===============
*/
static void SV_LoadFactoriesFromFile(const char* filename) {
    fileHandle_t f;
    int len;
    char buf[0x8000];
    jsonValue_t* root;
    jsonValue_t* element;

    len = FS_FOpenFileRead(filename, &f, qfalse);
    if (!f) {
        Com_Printf("^1file not found: %s\n^7", filename);
        return;
    }
    if (len >= 0x8000) {
        // the limit is 32768; the message says 16384, as the engine's did
        Com_Printf("^1file too large: %s is %i, max allowed is %i^7", filename, len, 0x4000);
        FS_FCloseFile(f);
        return;
    }

    FS_Read(buf, len, f);
    buf[len] = '\0';
    FS_FCloseFile(f);

    // the engine parsed the buffer as a C string
    root = JSON_Parse(buf, strlen(buf));
    if (!root) {
        Com_Printf("^1factories file %s was not a valid JSON document.^7", filename);
        return;
    }

    if (JSON_IsArray(root)) {
        for (element = root->first; element; element = element->next) {
            SV_ParseFactory(element);
        }
    } else if (JSON_IsObject(root)) {
        SV_ParseFactory(root);
    }
    JSON_FreeValue(root);

    Com_Printf("loaded factories from %s, total %i\n", filename, sv_numFactories);
}

/*
===============
SV_LoadFactories

scripts/factories.txt, then every .factories file in scripts.
quakelive_steam.exe 0x45fa50, called from SV_Init.
===============
*/
void SV_LoadFactories(void) {
    int numFiles, i;
    char fileList[1024];
    char filename[128];
    char* name;

    SV_LoadFactoriesFromFile("scripts/factories.txt");

    numFiles = FS_GetFileList("scripts", ".factories", fileList, sizeof(fileList));
    name = fileList;
    for (i = 0; i < numFiles; i++) {
        int nameLen = strlen(name);
        Com_sprintf(filename, sizeof(filename), "scripts/%s", name);
        SV_LoadFactoriesFromFile(filename);
        name += nameLen + 1;
    }
}

static void SV_FreeFactory(factory_t* factory) {
    int i;

    if (factory->author) {
        Z_Free(factory->author);
    }
    if (factory->description) {
        Z_Free(factory->description);
    }
    Z_Free(factory->id);
    Z_Free(factory->title);
    for (i = 0; i < MAX_FACTORY_CVARS && factory->cvars[i].name; i++) {
        Z_Free(factory->cvars[i].name);
        if (factory->cvars[i].value) {
            Z_Free(factory->cvars[i].value);
        }
    }
    for (i = 0; i < MAX_FACTORY_TAGS; i++) {
        if (factory->tags[i]) {
            Z_Free(factory->tags[i]);
        }
    }
    memset(factory, 0, sizeof(*factory));
}

/*
===============
SV_ReloadFactories_f

Frees every factory but the one in play and loads the files again.
quakelive_steam.exe 0x45fb40.
===============
*/
void SV_ReloadFactories_f(void) {
    int i;

    for (i = 0; i < sv_numFactories; i++) {
        if (svs.factory == &sv_factories[i]) {
            Com_Printf("not clearing currently loaded factory id %s, continuing\n", sv_factories[i].id);
        } else if (sv_factories[i].id) {
            SV_FreeFactory(&sv_factories[i]);
        }
    }
    sv_numFactories = 0;
    SV_LoadFactories();
}

/*
===============
SV_ApplyFactory

Restores what the factory in play changed, then saves and sets the cvars of
the new one, g_gametype, g_factory and g_factoryTitle. Part of the map command
(quakelive_steam.exe 0x4ddf00..0x4ddfa0).
===============
*/
void SV_ApplyFactory(factory_t* factory) {
    int i;

    if (svs.factory == factory) {
        return;
    }
    if (svs.factory) {
        SV_RestoreFactoryCvars(svs.factory);
    }
    for (i = 0; i < MAX_FACTORY_CVARS; i++) {
        if (!factory->cvars[i].name || !factory->cvars[i].value) {
            break;
        }
        Cvar_SaveFactoryValue(factory->cvars[i].name);
        Cvar_Set(factory->cvars[i].name, factory->cvars[i].value);
    }
    svs.factory = factory;
    Cvar_Set("g_gametype", va("%i", factory->basegt));
    Cvar_Set("g_factory", va("%s", factory->id));
    Cvar_Set("g_factoryTitle", va("%s", factory->title));
}

/*
===============
SV_RestoreFactoryCvars

quakelive_steam.exe 0x4ddf00 (map) and 0x4e3fbd (SV_Shutdown).
===============
*/
void SV_RestoreFactoryCvars(const factory_t* factory) {
    int i;

    for (i = 0; i < MAX_FACTORY_CVARS; i++) {
        if (!factory->cvars[i].name) {
            break;
        }
        Cvar_RestoreFactoryValue(factory->cvars[i].name);
    }
}

/*
===============================================================================

MAP POOL

===============================================================================
*/

/*
===============
SV_LoadMapPool

"map|factory" lines. quakelive_steam.exe 0x45edd0, called from SV_Init with
sv_mapPoolFile.
===============
*/
void SV_LoadMapPool(const char* filename) {
    fileHandle_t f;
    int len;
    char buf[0x8000];
    char path[MAX_QPATH];
    char* line;

    len = FS_FOpenFileRead(filename, &f, qfalse);
    if (!f) {
        Com_Printf("^1rotation file not found: %s\n^7", filename);
        return;
    }
    if (len > 0x7fff) {
        Com_Printf("^1rotations file too large: %s is %i, max allowed is %i^7", filename, len, 0x8000);
        FS_FCloseFile(f);
        return;
    }

    FS_Read(buf, len, f);
    buf[len] = '\0';
    FS_FCloseFile(f);

    // the engine allowed one line more than its table held; this stops at the table
    line = strtok(buf, "\n");
    while (line && sv_numMapPool < MAX_MAPPOOL) {
        const factory_t* factory;
        const arenaInfo_t* arena;
        const char* longname;
        mapPoolEntry_t* entry;
        char* p;

        if (*line == '#') {
            line = strtok(NULL, "\n");
            continue;
        }
        p = strchr(line, '\r');
        if (p) {
            *p = '\0';
        }
        p = strchr(line, '|');
        if (!p) {
            Com_Printf("^1map rotation item missing map or factory name, skipping: %s\n^7", line);
            line = strtok(NULL, "\n");
            continue;
        }
        factory = SV_FindFactory(p + 1);
        if (!factory || p[1] == '_') {
            Com_Printf("^1invalid factory found in rotation, skipping: %s\n^7", line);
            line = strtok(NULL, "\n");
            continue;
        }
        *p = '\0';

        if (!Cvar_VariableIntegerValue("com_build")) {
            Com_sprintf(path, sizeof(path), "maps/%s.bsp", line);
            if (FS_ReadFile(path, NULL) == -1) {
                Com_Printf("^1map doesn't exist, skipping: %s\n^7", line);
                line = strtok(NULL, "\n");
                continue;
            }
        }

        arena = SV_FindArena(line);
        longname = line;
        if (arena) {
            longname = arena->longname;
            if (!arena->gametypes[factory->basegt]) {
                Com_Printf("^1map isn't valid for factory gametype, skipping: %s\n^7", line);
                line = strtok(NULL, "\n");
                continue;
            }
        }

        entry = &sv_mapPool[sv_numMapPool++];
        Q_strncpyz(entry->map, line, sizeof(entry->map));
        Q_strncpyz(entry->longname, longname, sizeof(entry->longname));
        Q_strncpyz(entry->factory, factory->id, sizeof(entry->factory));
        Q_strncpyz(entry->factoryTitle, factory->title, sizeof(entry->factoryTitle));
        line = strtok(NULL, "\n");
    }

    Com_Printf("loaded %i maps into the map pool\n", sv_numMapPool);
}

/*
===============
SV_ReloadMapPool_f

quakelive_steam.exe 0x45f340.
===============
*/
void SV_ReloadMapPool_f(void) {
    Com_Printf("reloading map pool...\n");
    memset(sv_mapPool, 0, sizeof(sv_mapPool));
    sv_numMapPool = 0;
    SV_LoadMapPool(sv_mapPoolFile->string);
}

/*
===============
SV_RandomMapPoolEntry

Campgrounds as Free For All when the pool is empty. quakelive_steam.exe
0x45e830.
===============
*/
static const mapPoolEntry_t* SV_RandomMapPoolEntry(void) {
    static const mapPoolEntry_t defaultEntry = {"campgrounds", "Campgrounds", "ffa", "Free For All"};
    int r;

    if (!sv_numMapPool) {
        return &defaultEntry;
    }
    r = rand() << 16;
    r ^= rand();
    r ^= Sys_Milliseconds();
    r = abs(r);
    return &sv_mapPool[(unsigned int)(r % sv_numMapPool) % (unsigned int)sv_numMapPool];
}

/*
===============
SV_StartRandomMap_f

quakelive_steam.exe 0x45e8b0.
===============
*/
void SV_StartRandomMap_f(void) {
    mapPoolEntry_t entry = *SV_RandomMapPoolEntry();

    Cbuf_ExecuteText(EXEC_NOW, va("map %s %s", entry.map, entry.factory));
}

/*
===============
SV_SetNextMap

A random pool entry, or a restart of this map without a pool.
quakelive_steam.exe 0x45e940, called at the end of SV_SpawnServer and
map_restart.
===============
*/
void SV_SetNextMap(void) {
    mapPoolEntry_t entry;

    if (!sv_numMapPool) {
        Cvar_Set("nextmap", "map_restart 0");
        return;
    }
    entry = *SV_RandomMapPoolEntry();
    Cvar_Set("nextmap", va("map %s %s", entry.map, entry.factory));
}

/*
===============
SV_SetNextMaps

The maps offered at the end of a match, as map_N, title_N, cfg_N and gt_N
keys: this map first when sv_includeCurrentMapInVote is set, then the whole
pool when it has fewer than four maps, or three random maps from it.
quakelive_steam.exe 0x45f050, called after SV_SetNextMap.
===============
*/
void SV_SetNextMaps(void) {
    char info[1024];
    const mapPoolEntry_t* entries[MAX_MAPPOOL];
    int count = 0;
    int i;

    info[0] = '\0';

    if (sv_includeCurrentMapInVote->integer && svs.factory) {
        const char* mapname = Cvar_VariableString("mapname");
        const arenaInfo_t* arena = SV_FindArena(mapname);

        if (arena) {
            count = 1;
            Info_SetValueForKey(info, va("map_%i", 0), mapname);
            Info_SetValueForKey(info, va("title_%i", 0), arena->longname);
            Info_SetValueForKey(info, va("cfg_%i", 0), svs.factory->id);
            Info_SetValueForKey(info, va("gt_%i", 0), svs.factory->title);
        }
    }

    if (sv_numMapPool < 4) {
        // the pool entry has the same index as its keys, so with this map
        // included the first pool entry is left out
        for (i = count; i < sv_numMapPool; i++) {
            Info_SetValueForKey(info, va("map_%i", i), sv_mapPool[i].map);
            Info_SetValueForKey(info, va("title_%i", i), sv_mapPool[i].longname);
            Info_SetValueForKey(info, va("cfg_%i", i), sv_mapPool[i].factory);
            Info_SetValueForKey(info, va("gt_%i", i), sv_mapPool[i].factoryTitle);
        }
    } else {
        for (i = 0; i < sv_numMapPool; i++) {
            entries[i] = &sv_mapPool[i];
        }
        while (count < 3) {
            int r = rand() << 16;
            int index;

            r ^= rand();
            r ^= Sys_Milliseconds();
            index = abs(r) % sv_numMapPool;
            if (entries[index]) {
                Info_SetValueForKey(info, va("map_%i", count), entries[index]->map);
                Info_SetValueForKey(info, va("title_%i", count), entries[index]->longname);
                Info_SetValueForKey(info, va("cfg_%i", count), entries[index]->factory);
                Info_SetValueForKey(info, va("gt_%i", count), entries[index]->factoryTitle);
                entries[index] = NULL;
                count++;
            }
        }
    }

    Cvar_Set("nextmaps", info);
}
