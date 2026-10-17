/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

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
#include "server.h"

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <dlfcn.h>
#endif

#ifdef _MSC_VER
#define MQ_THREAD __declspec(thread)
#else
#define MQ_THREAD __thread
#endif

#define MQ_RET_NONE 0
#define MQ_RET_STOP 1
#define MQ_RET_STOP_EVENT 2
#define MQ_RET_STOP_ALL 3
#define MQ_RET_USAGE 4

#define MQ_PRIV_BANNED -1
#define MQ_PRIV_NONE 0
#define MQ_PRIV_MOD 1
#define MQ_PRIV_ADMIN 2
#define MQ_PRIV_ROOT 3

#define MQ_METH_VARARGS 0x0001
#define MQ_METH_NOARGS 0x0004
#define MQ_FILE_INPUT 257
#define MQ_ABI_VERSION 3
#define MQ_BUFFER 4096

typedef struct mqObject_s mqObject;
typedef intptr_t mqSize;
typedef mqObject* (*mqFunction)(mqObject* self, mqObject* args);

typedef struct {
    const char* name;
    mqFunction function;
    int flags;
    const char* doc;
} mqMethod_t;

typedef struct {
    mqSize refcount;
    void* type;
    mqObject* (*init)(void);
    mqSize index;
    mqObject* copy;
    const char* name;
    const char* doc;
    mqSize size;
    mqMethod_t* methods;
    void* slots;
    void* traverse;
    void* clear;
    void* free;
} mqModuleDef_t;

typedef struct {
    const char* name;
    const char* doc;
} mqField_t;

typedef struct {
    const char* name;
    const char* doc;
    mqField_t* fields;
    int count;
} mqStructDesc_t;

static struct {
    void* lib;

    const char* (*Py_GetVersion)(void);
    int (*Py_IsInitialized)(void);
    void (*Py_InitializeEx)(int signals);
    void (*PyEval_InitThreads)(void);
    void* (*PyEval_SaveThread)(void);
    int (*PyGILState_Ensure)(void);
    void (*PyGILState_Release)(int state);
    void (*Py_SetProgramName)(const wchar_t* name);
    void (*Py_SetPythonHome)(const wchar_t* home);
    mqObject* (*Py_CompileString)(const char* code, const char* filename, int start);
    mqObject* (*PyEval_EvalCode)(mqObject* code, mqObject* globals, mqObject* locals);
    mqObject* (*PyImport_AddModule)(const char* name);
    mqObject* (*PyImport_GetModuleDict)(void);
    mqObject* (*PyModule_GetDict)(mqObject* module);
    mqObject* (*PyModule_Create2)(mqModuleDef_t* def, int apiVersion);
    int (*PyModule_AddObject)(mqObject* module, const char* name, mqObject* value);
    int (*PyModule_AddIntConstant)(mqObject* module, const char* name, long value);
    int (*PyModule_AddStringConstant)(mqObject* module, const char* name, const char* value);
    mqObject* (*PyDict_GetItemString)(mqObject* dict, const char* key);
    int (*PyDict_SetItemString)(mqObject* dict, const char* key, mqObject* value);
    mqObject* (*PyStructSequence_NewType)(mqStructDesc_t* desc);
    mqObject* (*PyStructSequence_New)(mqObject* type);
    void (*PyStructSequence_SetItem)(mqObject* sequence, mqSize index, mqObject* value);
    mqObject* (*PyStructSequence_GetItem)(mqObject* sequence, mqSize index);
    int (*PyArg_ParseTuple)(mqObject* args, const char* format, ...);
    mqObject* (*PyLong_FromLongLong)(long long value);
    long (*PyLong_AsLong)(mqObject* object);
    mqObject* (*PyFloat_FromDouble)(double value);
    double (*PyFloat_AsDouble)(mqObject* object);
    mqObject* (*PyBool_FromLong)(long value);
    mqObject* (*PyUnicode_FromString)(const char* text);
    mqObject* (*PyUnicode_DecodeUTF8)(const char* text, mqSize size, const char* errors);
    mqObject* (*PyUnicode_AsUTF8String)(mqObject* object);
    char* (*PyBytes_AsString)(mqObject* object);
    mqObject* (*PyList_New)(mqSize size);
    int (*PyList_SetItem)(mqObject* list, mqSize index, mqObject* item);
    mqObject* (*PyObject_CallFunction)(mqObject* callable, const char* format, ...);
    mqObject* (*PyObject_CallObject)(mqObject* callable, mqObject* args);
    int (*PyCallable_Check)(mqObject* object);
    int (*PyObject_IsInstance)(mqObject* object, mqObject* type);
    void (*Py_IncRef)(mqObject* object);
    void (*Py_DecRef)(mqObject* object);
    mqObject* (*PyErr_Occurred)(void);
    void (*PyErr_Print)(void);
    void (*PyErr_SetString)(mqObject* type, const char* message);
    mqObject* (*PyErr_Format)(mqObject* type, const char* format, ...);

    mqObject* None;
    mqObject* True;
    mqObject* False;
    mqObject* LongType;
    mqObject* UnicodeType;
    mqObject** ValueError;
    mqObject** TypeError;
} py;

typedef struct {
    const char* name;
    void** address;
    qboolean optional;
} mqSymbol_t;

#define MQ_SYM(x) {#x, (void**)&py.x, qfalse}
#define MQ_OPT(x) {#x, (void**)&py.x, qtrue}

static const mqSymbol_t mqSymbols[] = {
    MQ_SYM(Py_GetVersion),
    MQ_SYM(Py_IsInitialized),
    MQ_SYM(Py_InitializeEx),
    MQ_OPT(PyEval_InitThreads),
    MQ_SYM(PyEval_SaveThread),
    MQ_SYM(PyGILState_Ensure),
    MQ_SYM(PyGILState_Release),
    MQ_OPT(Py_SetProgramName),
    MQ_OPT(Py_SetPythonHome),
    MQ_SYM(Py_CompileString),
    MQ_SYM(PyEval_EvalCode),
    MQ_SYM(PyImport_AddModule),
    MQ_SYM(PyImport_GetModuleDict),
    MQ_SYM(PyModule_GetDict),
    MQ_SYM(PyModule_Create2),
    MQ_SYM(PyModule_AddObject),
    MQ_SYM(PyModule_AddIntConstant),
    MQ_SYM(PyModule_AddStringConstant),
    MQ_SYM(PyDict_GetItemString),
    MQ_SYM(PyDict_SetItemString),
    MQ_SYM(PyStructSequence_NewType),
    MQ_SYM(PyStructSequence_New),
    MQ_SYM(PyStructSequence_SetItem),
    MQ_SYM(PyStructSequence_GetItem),
    MQ_SYM(PyArg_ParseTuple),
    MQ_SYM(PyLong_FromLongLong),
    MQ_SYM(PyLong_AsLong),
    MQ_SYM(PyFloat_FromDouble),
    MQ_SYM(PyFloat_AsDouble),
    MQ_SYM(PyBool_FromLong),
    MQ_SYM(PyUnicode_FromString),
    MQ_SYM(PyUnicode_DecodeUTF8),
    MQ_SYM(PyUnicode_AsUTF8String),
    MQ_SYM(PyBytes_AsString),
    MQ_SYM(PyList_New),
    MQ_SYM(PyList_SetItem),
    MQ_SYM(PyObject_CallFunction),
    MQ_SYM(PyObject_CallObject),
    MQ_SYM(PyCallable_Check),
    MQ_SYM(PyObject_IsInstance),
    MQ_SYM(Py_IncRef),
    MQ_SYM(Py_DecRef),
    MQ_SYM(PyErr_Occurred),
    MQ_SYM(PyErr_Print),
    MQ_SYM(PyErr_SetString),
    MQ_SYM(PyErr_Format),
    {"_Py_NoneStruct", (void**)&py.None, qfalse},
    {"_Py_TrueStruct", (void**)&py.True, qfalse},
    {"_Py_FalseStruct", (void**)&py.False, qfalse},
    {"PyLong_Type", (void**)&py.LongType, qfalse},
    {"PyUnicode_Type", (void**)&py.UnicodeType, qfalse},
    {"PyExc_ValueError", (void**)&py.ValueError, qfalse},
    {"PyExc_TypeError", (void**)&py.TypeError, qfalse},
};

typedef enum {
    MQ_CLIENT_COMMAND,
    MQ_SERVER_COMMAND,
    MQ_FRAME,
    MQ_PLAYER_CONNECT,
    MQ_PLAYER_LOADED,
    MQ_PLAYER_DISCONNECT,
    MQ_CUSTOM_COMMAND,
    MQ_NEW_GAME,
    MQ_SET_CONFIGSTRING,
    MQ_RCON,
    MQ_CONSOLE_PRINT,
    MQ_PLAYER_SPAWN,
    MQ_KAMIKAZE_USE,
    MQ_KAMIKAZE_EXPLODE,
    MQ_NUM_HANDLERS
} mqHandler_t;

static const char* const mqHandlerNames[MQ_NUM_HANDLERS] = {
    "client_command", "server_command", "frame", "player_connect", "player_loaded", "player_disconnect",
    "custom_command", "new_game", "set_configstring", "rcon", "console_print", "player_spawn",
    "kamikaze_use", "kamikaze_explode",
};

static mqObject* mqHandlers[MQ_NUM_HANDLERS];
static qboolean mqActive;
static qboolean mqTagged;
static int mqAllowFreeClient = -1;
static const minqlxGame_t* mqGame;
static MQ_THREAD int mqPrinting;

static mqObject* mqPlayerInfoType;
static mqObject* mqPlayerStateType;
static mqObject* mqPlayerStatsType;
static mqObject* mqVector3Type;
static mqObject* mqWeaponsType;
static mqObject* mqPowerupsType;
static mqObject* mqFlightType;

qboolean sv_minqlxSkipFrame;

static mqField_t mqPlayerInfoFields[] = {
    {"client_id", "The player's client ID."},
    {"name", "The player's name."},
    {"connection_state", "The player's connection state."},
    {"userinfo", "The player's userinfo."},
    {"steam_id", "The player's 64-bit representation of the Steam ID."},
    {"team", "The player's team."},
    {"privileges", "The player's privileges."},
    {NULL, NULL},
};

static mqField_t mqPlayerStateFields[] = {
    {"is_alive", "Whether the player's alive or not."},
    {"position", "The player's position."},
    {"velocity", "The player's velocity."},
    {"health", "The player's health."},
    {"armor", "The player's armor."},
    {"noclip", "Whether the player has noclip or not."},
    {"weapon", "The weapon the player is currently using."},
    {"weapons", "The player's weapons."},
    {"ammo", "The player's weapon ammo."},
    {"powerups", "The player's powerups."},
    {"holdable", "The player's holdable item."},
    {"flight", "A struct sequence with flight parameters."},
    {"is_frozen", "Whether the player is frozen(freezetag)."},
    {NULL, NULL},
};

static mqField_t mqPlayerStatsFields[] = {
    {"score", "The player's primary score."},
    {"kills", "The player's number of kills."},
    {"deaths", "The player's number of deaths."},
    {"damage_dealt", "The player's total damage dealt."},
    {"damage_taken", "The player's total damage taken."},
    {"time", "The time in milliseconds the player has on a team since the game started."},
    {"ping", "The player's ping."},
    {NULL, NULL},
};

static mqField_t mqVector3Fields[] = {
    {"x", NULL}, {"y", NULL}, {"z", NULL}, {NULL, NULL},
};

static mqField_t mqWeaponsFields[] = {
    {"g", NULL}, {"mg", NULL}, {"sg", NULL}, {"gl", NULL}, {"rl", NULL}, {"lg", NULL}, {"rg", NULL}, {"pg", NULL},
    {"bfg", NULL}, {"gh", NULL}, {"ng", NULL}, {"pl", NULL}, {"cg", NULL}, {"hmg", NULL}, {"hands", NULL},
    {NULL, NULL},
};

static mqField_t mqPowerupsFields[] = {
    {"quad", NULL}, {"battlesuit", NULL}, {"haste", NULL}, {"invisibility", NULL}, {"regeneration", NULL},
    {"invulnerability", NULL}, {NULL, NULL},
};

static mqField_t mqFlightFields[] = {
    {"fuel", NULL}, {"max_fuel", NULL}, {"thrust", NULL}, {"refuel", NULL}, {NULL, NULL},
};

static mqStructDesc_t mqPlayerInfoDesc = {"PlayerInfo",
    "Information about a player, such as Steam ID, name, client ID, and whatnot.", mqPlayerInfoFields, 7};
static mqStructDesc_t mqPlayerStateDesc = {"PlayerState", "Information about a player's state in the game.",
    mqPlayerStateFields, 13};
static mqStructDesc_t mqPlayerStatsDesc = {"PlayerStats", "A player's score and some basic stats.",
    mqPlayerStatsFields, 7};
static mqStructDesc_t mqVector3Desc = {"Vector3", "A three-dimensional vector.", mqVector3Fields, 3};
static mqStructDesc_t mqWeaponsDesc = {"Weapons", "A struct sequence containing all the weapons in the game.",
    mqWeaponsFields, MINQLX_WEAPONS};
static mqStructDesc_t mqPowerupsDesc = {"Powerups", "A struct sequence containing all the powerups in the game.",
    mqPowerupsFields, MINQLX_POWERUPS};
static mqStructDesc_t mqFlightDesc = {"Flight",
    "A struct sequence containing parameters for the flight holdable item.", mqFlightFields, 4};

static const char mqLoader[] =
    "import traceback\n"
    "try:\n"
    "  import os\n"
    "  import sys\n"
    "  os.environ.setdefault('LD_PRELOAD', '')\n"
    "  sys.path.append('minqlx.zip')\n"
    "  sys.path.append('.')\n"
    "  import minqlx\n"
    "  minqlx.initialize()\n"
    "  ret = True\n"
    "except Exception as e:\n"
    "  e = traceback.format_exc().rstrip('\\n')\n"
    "  for line in e.split('\\n'): print(line)\n"
    "  ret = False\n";

static const char mqUnloader[] =
    "import os\n"
    "import sys\n"
    "import traceback\n"
    "import _minqlx\n"
    "try:\n"
    "  import minqlx\n"
    "  for name in list(minqlx.Plugin._loaded_plugins):\n"
    "    minqlx.unload_plugin(name)\n"
    "except Exception as e:\n"
    "  e = traceback.format_exc().rstrip('\\n')\n"
    "  for line in e.split('\\n'): print(line)\n"
    "import logging\n"
    "for handler in list(logging.getLogger('minqlx').handlers):\n"
    "  logging.getLogger('minqlx').removeHandler(handler)\n"
    "  handler.close()\n"
    "base = os.path.basename(_minqlx.get_cvar('qlx_pluginsPath') or 'minqlx-plugins')\n"
    "for name in list(sys.modules):\n"
    "  if name in ('minqlx', base) or name.startswith('minqlx.') or name.startswith(base + '.'):\n"
    "    del sys.modules[name]\n"
    "ret = True\n";

static mqObject* MQ_Ref(mqObject* object) {
    py.Py_IncRef(object);
    return object;
}

static mqObject* MQ_None(void) {
    return MQ_Ref(py.None);
}

static mqObject* MQ_Bool(qboolean value) {
    return MQ_Ref(value ? py.True : py.False);
}

static mqObject* MQ_Int(long long value) {
    return py.PyLong_FromLongLong(value);
}

static mqObject* MQ_Text(const char* text) {
    return py.PyUnicode_DecodeUTF8(text, (mqSize)strlen(text), "ignore");
}

static qboolean MQ_IsInstance(mqObject* object, mqObject* type) {
    return py.PyObject_IsInstance(object, type) == 1;
}

static qboolean MQ_CopyText(mqObject* object, char* out, int size) {
    mqObject* bytes;
    const char* text;

    if (!MQ_IsInstance(object, py.UnicodeType)) {
        return qfalse;
    }
    bytes = py.PyUnicode_AsUTF8String(object);
    if (!bytes) {
        py.PyErr_Print();
        return qfalse;
    }
    text = py.PyBytes_AsString(bytes);
    Q_strncpyz(out, text ? text : "", size);
    py.Py_DecRef(bytes);
    return qtrue;
}

static mqObject* MQ_BadClient(void) {
    return py.PyErr_Format(*py.ValueError, "client_id needs to be a number from 0 to %d.", sv_maxclients->integer);
}

static qboolean MQ_ValidClient(int clientNum) {
    return clientNum >= 0 && clientNum < sv_maxclients->integer;
}

static mqObject* MQ_Vector(const float* v) {
    mqObject* vector = py.PyStructSequence_New(mqVector3Type);
    int i;

    for (i = 0; i < 3; i++) {
        py.PyStructSequence_SetItem(vector, i, py.PyFloat_FromDouble(v[i]));
    }
    return vector;
}

static mqObject* MQ_MakePlayerInfo(int clientNum) {
    client_t* cl = &svs.clients[clientNum];
    char name[MAX_NAME_LENGTH * 4];
    int team = TEAM_SPECTATOR, privileges = -1;
    mqObject* info;

    name[0] = '\0';
    if (mqGame) {
        mqGame->PlayerInfo(clientNum, name, sizeof(name), &team, &privileges);
    }
    info = py.PyStructSequence_New(mqPlayerInfoType);
    py.PyStructSequence_SetItem(info, 0, MQ_Int(clientNum));
    py.PyStructSequence_SetItem(info, 1, MQ_Text(name));
    py.PyStructSequence_SetItem(info, 2, MQ_Int(cl->state));
    py.PyStructSequence_SetItem(info, 3, MQ_Text(cl->userinfo));
    py.PyStructSequence_SetItem(info, 4, MQ_Int((long long)cl->steam_id));
    py.PyStructSequence_SetItem(info, 5, MQ_Int(team));
    py.PyStructSequence_SetItem(info, 6, MQ_Int(privileges));
    return info;
}

static mqObject* MQ_PlayerInfo(mqObject* self, mqObject* args) {
    int clientNum;

    if (!py.PyArg_ParseTuple(args, "i:player", &clientNum)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (mqAllowFreeClient != clientNum && svs.clients[clientNum].state == CS_FREE) {
        return MQ_None();
    }
    return MQ_MakePlayerInfo(clientNum);
}

static mqObject* MQ_PlayersInfo(mqObject* self, mqObject* args) {
    mqObject* list = py.PyList_New(sv_maxclients->integer);
    int i;

    for (i = 0; i < sv_maxclients->integer; i++) {
        mqObject* item = svs.clients[i].state == CS_FREE ? MQ_None() : MQ_MakePlayerInfo(i);

        if (py.PyList_SetItem(list, i, item) == -1) {
            py.Py_DecRef(list);
            return NULL;
        }
    }
    return list;
}

static mqObject* MQ_GetUserinfo(mqObject* self, mqObject* args) {
    int clientNum;

    if (!py.PyArg_ParseTuple(args, "i:get_userinfo", &clientNum)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (mqAllowFreeClient != clientNum && svs.clients[clientNum].state == CS_FREE) {
        return MQ_None();
    }
    return MQ_Text(svs.clients[clientNum].userinfo);
}

static mqObject* MQ_SendServerCommand(mqObject* self, mqObject* args) {
    mqObject* target;
    const char* cmd;
    int clientNum;

    if (!py.PyArg_ParseTuple(args, "Os:send_server_command", &target, &cmd)) {
        return NULL;
    }
    if (target == py.None) {
        SV_SendServerCommand(NULL, "%s\n", cmd);
        return MQ_Bool(qtrue);
    }
    if (MQ_IsInstance(target, py.LongType)) {
        clientNum = py.PyLong_AsLong(target);
        if (MQ_ValidClient(clientNum)) {
            if (svs.clients[clientNum].state != CS_ACTIVE) {
                return MQ_Bool(qfalse);
            }
            SV_SendServerCommand(&svs.clients[clientNum], "%s\n", cmd);
            return MQ_Bool(qtrue);
        }
    }
    return py.PyErr_Format(*py.ValueError, "client_id needs to be a number from 0 to %d, or None.",
                           sv_maxclients->integer);
}

static mqObject* MQ_ClientCommand(mqObject* self, mqObject* args) {
    const char* cmd;
    int clientNum;

    if (!py.PyArg_ParseTuple(args, "is:client_command", &clientNum, &cmd)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return py.PyErr_Format(*py.ValueError, "client_id needs to be a number from 0 to %d, or None.",
                               sv_maxclients->integer);
    }
    if (svs.clients[clientNum].state == CS_FREE || svs.clients[clientNum].state == CS_ZOMBIE) {
        return MQ_Bool(qfalse);
    }
    SV_ExecuteClientCommand(&svs.clients[clientNum], cmd, qtrue);
    return MQ_Bool(qtrue);
}

static mqObject* MQ_ConsoleCommand(mqObject* self, mqObject* args) {
    const char* cmd;

    if (!py.PyArg_ParseTuple(args, "s:console_command", &cmd)) {
        return NULL;
    }
    Cmd_ExecuteString(cmd);
    return MQ_None();
}

static mqObject* MQ_GetCvar(mqObject* self, mqObject* args) {
    const char* name;

    if (!py.PyArg_ParseTuple(args, "s:get_cvar", &name)) {
        return NULL;
    }
    if (Cvar_Flags(name) != CVAR_NONEXISTENT) {
        return py.PyUnicode_FromString(Cvar_VariableString(name));
    }
    return MQ_None();
}

static mqObject* MQ_SetCvar(mqObject* self, mqObject* args) {
    const char *name, *value;
    int flags = 0;

    if (!py.PyArg_ParseTuple(args, "ss|i:set_cvar", &name, &value, &flags)) {
        return NULL;
    }
    if (Cvar_Flags(name) == CVAR_NONEXISTENT) {
        Cvar_Get(name, value, flags);
        return MQ_Bool(qtrue);
    }
    Cvar_Set2(name, value, flags == -1 ? qtrue : qfalse);
    return MQ_Bool(qfalse);
}

static mqObject* MQ_SetCvarLimit(mqObject* self, mqObject* args) {
    const char *name, *value, *min, *max;
    int flags = 0;

    if (!py.PyArg_ParseTuple(args, "ssss|i:set_cvar_limit", &name, &value, &min, &max, &flags)) {
        return NULL;
    }
    Cvar_GetRange(name, value, min, max, flags);
    return MQ_None();
}

static mqObject* MQ_Kick(mqObject* self, mqObject* args) {
    char reason[MAX_STRING_CHARS];
    mqObject* why;
    int clientNum;

    if (!py.PyArg_ParseTuple(args, "iO:kick", &clientNum, &why)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return py.PyErr_Format(*py.ValueError, "client_id needs to be a number from 0 to %d, or None.",
                               sv_maxclients->integer);
    }
    if (svs.clients[clientNum].state != CS_ACTIVE) {
        return py.PyErr_Format(*py.ValueError, "client_id must be None or the ID of an active player.");
    }
    if (why == py.None || (MQ_CopyText(why, reason, sizeof(reason)) && !reason[0])) {
        SV_DropClient(&svs.clients[clientNum], "was kicked.");
    } else if (MQ_CopyText(why, reason, sizeof(reason))) {
        SV_DropClient(&svs.clients[clientNum], reason);
    }
    return MQ_None();
}

static mqObject* MQ_ConsolePrint(mqObject* self, mqObject* args) {
    const char* text;

    if (!py.PyArg_ParseTuple(args, "s:console_print", &text)) {
        return NULL;
    }
    Com_Printf("%s\n", text);
    return MQ_None();
}

static mqObject* MQ_GetConfigstring(mqObject* self, mqObject* args) {
    char buffer[MQ_BUFFER];
    int index;

    if (!py.PyArg_ParseTuple(args, "i:get_configstring", &index)) {
        return NULL;
    }
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        return py.PyErr_Format(*py.ValueError, "index needs to be a number from 0 to %d.", MAX_CONFIGSTRINGS);
    }
    SV_GetConfigstring(index, buffer, sizeof(buffer));
    return MQ_Text(buffer);
}

static mqObject* MQ_SetConfigstring(mqObject* self, mqObject* args) {
    const char* value;
    int index;

    if (!py.PyArg_ParseTuple(args, "is:set_configstring", &index, &value)) {
        return NULL;
    }
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        return py.PyErr_Format(*py.ValueError, "index needs to be a number from 0 to %d.", MAX_CONFIGSTRINGS);
    }
    SV_SetConfigstring(index, value);
    return MQ_None();
}

static mqObject* MQ_ForceVote(mqObject* self, mqObject* args) {
    int pass;

    if (!py.PyArg_ParseTuple(args, "p:force_vote", &pass)) {
        return NULL;
    }
    return MQ_Bool(mqGame && mqGame->ForceVote(pass ? qtrue : qfalse));
}

static void MQ_CustomCommand_f(void);

static mqObject* MQ_AddConsoleCommand(mqObject* self, mqObject* args) {
    const char* cmd;

    if (!py.PyArg_ParseTuple(args, "s:add_console_command", &cmd)) {
        return NULL;
    }
    Cmd_AddCommand(cmd, MQ_CustomCommand_f);
    return MQ_None();
}

static mqObject* MQ_RegisterHandler(mqObject* self, mqObject* args) {
    const char* event;
    mqObject* handler;
    int i;

    if (!py.PyArg_ParseTuple(args, "sO:register_handler", &event, &handler)) {
        return NULL;
    }
    if (handler != py.None && !py.PyCallable_Check(handler)) {
        py.PyErr_SetString(*py.TypeError, "The handler must be callable.");
        return NULL;
    }
    for (i = 0; i < MQ_NUM_HANDLERS; i++) {
        if (!strcmp(mqHandlerNames[i], event)) {
            if (mqHandlers[i]) {
                py.Py_DecRef(mqHandlers[i]);
            }
            mqHandlers[i] = handler == py.None ? NULL : MQ_Ref(handler);
            return MQ_None();
        }
    }
    py.PyErr_SetString(*py.ValueError, "Invalid event.");
    return NULL;
}

static mqObject* MQ_Holdable(int holdable) {
    switch (holdable) {
        case 0:
            return MQ_None();
        case 27:
            return py.PyUnicode_FromString("teleporter");
        case 28:
            return py.PyUnicode_FromString("medkit");
        case 34:
            return py.PyUnicode_FromString("flight");
        case 37:
            return py.PyUnicode_FromString("kamikaze");
        case 38:
            return py.PyUnicode_FromString("portal");
        case 39:
            return py.PyUnicode_FromString("invulnerability");
        default:
            return py.PyUnicode_FromString("unknown");
    }
}

static mqObject* MQ_PlayerState(mqObject* self, mqObject* args) {
    minqlxPlayerState_t ps;
    mqObject *state, *weapons, *ammo, *powerups, *flight;
    int clientNum, i;

    if (!py.PyArg_ParseTuple(args, "i:player_state", &clientNum)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame || !mqGame->PlayerState(clientNum, &ps)) {
        return MQ_None();
    }
    state = py.PyStructSequence_New(mqPlayerStateType);
    py.PyStructSequence_SetItem(state, 0, py.PyBool_FromLong(ps.alive));
    py.PyStructSequence_SetItem(state, 1, MQ_Vector(ps.origin));
    py.PyStructSequence_SetItem(state, 2, MQ_Vector(ps.velocity));
    py.PyStructSequence_SetItem(state, 3, MQ_Int(ps.health));
    py.PyStructSequence_SetItem(state, 4, MQ_Int(ps.armor));
    py.PyStructSequence_SetItem(state, 5, py.PyBool_FromLong(ps.noclip));
    py.PyStructSequence_SetItem(state, 6, MQ_Int(ps.weapon));
    weapons = py.PyStructSequence_New(mqWeaponsType);
    ammo = py.PyStructSequence_New(mqWeaponsType);
    for (i = 0; i < MINQLX_WEAPONS; i++) {
        py.PyStructSequence_SetItem(weapons, i, py.PyBool_FromLong(ps.weapons & (1 << (i + 1))));
        py.PyStructSequence_SetItem(ammo, i, MQ_Int(ps.ammo[i]));
    }
    py.PyStructSequence_SetItem(state, 7, weapons);
    py.PyStructSequence_SetItem(state, 8, ammo);
    powerups = py.PyStructSequence_New(mqPowerupsType);
    for (i = 0; i < MINQLX_POWERUPS; i++) {
        py.PyStructSequence_SetItem(powerups, i, MQ_Int(ps.powerups[i]));
    }
    py.PyStructSequence_SetItem(state, 9, powerups);
    py.PyStructSequence_SetItem(state, 10, MQ_Holdable(ps.holdable));
    flight = py.PyStructSequence_New(mqFlightType);
    for (i = 0; i < 4; i++) {
        py.PyStructSequence_SetItem(flight, i, MQ_Int(ps.flight[i]));
    }
    py.PyStructSequence_SetItem(state, 11, flight);
    py.PyStructSequence_SetItem(state, 12, py.PyBool_FromLong(ps.frozen));
    return state;
}

static mqObject* MQ_PlayerStats(mqObject* self, mqObject* args) {
    minqlxPlayerStats_t st;
    mqObject* stats;
    int clientNum;

    if (!py.PyArg_ParseTuple(args, "i:player_stats", &clientNum)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame || !mqGame->PlayerStats(clientNum, &st)) {
        return MQ_None();
    }
    stats = py.PyStructSequence_New(mqPlayerStatsType);
    py.PyStructSequence_SetItem(stats, 0, MQ_Int(st.score));
    py.PyStructSequence_SetItem(stats, 1, MQ_Int(st.kills));
    py.PyStructSequence_SetItem(stats, 2, MQ_Int(st.deaths));
    py.PyStructSequence_SetItem(stats, 3, MQ_Int(st.damageDealt));
    py.PyStructSequence_SetItem(stats, 4, MQ_Int(st.damageTaken));
    py.PyStructSequence_SetItem(stats, 5, MQ_Int(st.time));
    py.PyStructSequence_SetItem(stats, 6, MQ_Int(st.ping));
    return stats;
}

static mqObject* MQ_SetVector(mqObject* args, const char* format,
                              qboolean (*set)(int clientNum, const float* v)) {
    mqObject* vector;
    float v[3];
    int clientNum, i;

    if (!py.PyArg_ParseTuple(args, format, &clientNum, &vector)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame) {
        return MQ_Bool(qfalse);
    }
    if (!MQ_IsInstance(vector, mqVector3Type)) {
        return py.PyErr_Format(*py.ValueError, "Argument must be of type minqlx.Vector3.");
    }
    for (i = 0; i < 3; i++) {
        v[i] = (float)py.PyFloat_AsDouble(py.PyStructSequence_GetItem(vector, i));
    }
    return MQ_Bool(set(clientNum, v));
}

static mqObject* MQ_SetPosition(mqObject* self, mqObject* args) {
    return MQ_SetVector(args, "iO:set_position", mqGame ? mqGame->SetPosition : NULL);
}

static mqObject* MQ_SetVelocity(mqObject* self, mqObject* args) {
    return MQ_SetVector(args, "iO:set_velocity", mqGame ? mqGame->SetVelocity : NULL);
}

static mqObject* MQ_NoClip(mqObject* self, mqObject* args) {
    int clientNum, activate;

    if (!py.PyArg_ParseTuple(args, "ip:noclip", &clientNum, &activate)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    return MQ_Bool(mqGame && mqGame->NoClip(clientNum, activate ? qtrue : qfalse));
}

static mqObject* MQ_SetInt(mqObject* args, const char* format, qboolean (*set)(int clientNum, int value)) {
    int clientNum, value;

    if (!py.PyArg_ParseTuple(args, format, &clientNum, &value)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    return MQ_Bool(set && set(clientNum, value));
}

static mqObject* MQ_SetHealth(mqObject* self, mqObject* args) {
    return MQ_SetInt(args, "ii:set_health", mqGame ? mqGame->SetHealth : NULL);
}

static mqObject* MQ_SetArmor(mqObject* self, mqObject* args) {
    return MQ_SetInt(args, "ii:set_armor", mqGame ? mqGame->SetArmor : NULL);
}

static mqObject* MQ_SetScore(mqObject* self, mqObject* args) {
    return MQ_SetInt(args, "ii:set_score", mqGame ? mqGame->SetScore : NULL);
}

static mqObject* MQ_SetPrivileges(mqObject* self, mqObject* args) {
    return MQ_SetInt(args, "ii:set_privileges", mqGame ? mqGame->SetPrivileges : NULL);
}

static mqObject* MQ_SetHoldable(mqObject* self, mqObject* args) {
    int clientNum, holdable;

    if (!py.PyArg_ParseTuple(args, "ii:set_holdable", &clientNum, &holdable)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame) {
        return MQ_Bool(qfalse);
    }
    if (holdable < 0 || holdable >= mqGame->NumItems()) {
        return py.PyErr_Format(*py.ValueError, "holdable needs to be a number from 0 to %d.", mqGame->NumItems() - 1);
    }
    return MQ_Bool(mqGame->SetHoldable(clientNum, holdable));
}

static mqObject* MQ_SetWeapon(mqObject* self, mqObject* args) {
    int clientNum, weapon;

    if (!py.PyArg_ParseTuple(args, "ii:set_weapon", &clientNum, &weapon)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame) {
        return MQ_Bool(qfalse);
    }
    if (weapon < 0 || weapon >= MAX_WEAPONS) {
        return py.PyErr_Format(*py.ValueError, "Weapon must be a number from 0 to 15.");
    }
    return MQ_Bool(mqGame->SetWeapon(clientNum, weapon));
}

static mqObject* MQ_SetInvulnerability(mqObject* self, mqObject* args) {
    int clientNum, time;

    if (!py.PyArg_ParseTuple(args, "ii:set_invulnerability", &clientNum, &time)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame) {
        return MQ_Bool(qfalse);
    }
    if (time <= 0) {
        return py.PyErr_Format(*py.ValueError, "time needs to be positive integer.");
    }
    return MQ_Bool(mqGame->SetInvulnerability(clientNum, time));
}

static qboolean MQ_ReadInts(mqObject* sequence, int count, int* out) {
    int i;

    for (i = 0; i < count; i++) {
        mqObject* item = py.PyStructSequence_GetItem(sequence, i);

        if (!MQ_IsInstance(item, py.LongType)) {
            py.PyErr_Format(*py.ValueError, "Tuple argument %d is not an integer.", i);
            return qfalse;
        }
        out[i] = py.PyLong_AsLong(item);
    }
    return qtrue;
}

static mqObject* MQ_SetWeapons(mqObject* self, mqObject* args) {
    mqObject* weapons;
    int clientNum, flags = 0, i;

    if (!py.PyArg_ParseTuple(args, "iO:set_weapons", &clientNum, &weapons)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!mqGame) {
        return MQ_Bool(qfalse);
    }
    if (!MQ_IsInstance(weapons, mqWeaponsType)) {
        return py.PyErr_Format(*py.ValueError, "Argument must be of type minqlx.Weapons.");
    }
    for (i = 0; i < MINQLX_WEAPONS; i++) {
        mqObject* w = py.PyStructSequence_GetItem(weapons, i);

        if (w != py.True && w != py.False) {
            return py.PyErr_Format(*py.ValueError, "Tuple argument %d is not a boolean.", i);
        }
        if (w == py.True) {
            flags |= 1 << (i + 1);
        }
    }
    return MQ_Bool(mqGame->SetWeapons(clientNum, flags));
}

static mqObject* MQ_SetIntSequence(mqObject* args, const char* format, mqObject* type, const char* typeName,
                                   int count, qboolean (*set)(int clientNum, const int* values)) {
    mqObject* sequence;
    int values[MINQLX_WEAPONS];
    int clientNum;

    if (!py.PyArg_ParseTuple(args, format, &clientNum, &sequence)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    if (!set) {
        return MQ_Bool(qfalse);
    }
    if (!MQ_IsInstance(sequence, type)) {
        return py.PyErr_Format(*py.ValueError, "Argument must be of type minqlx.%s.", typeName);
    }
    if (!MQ_ReadInts(sequence, count, values)) {
        return NULL;
    }
    return MQ_Bool(set(clientNum, values));
}

static mqObject* MQ_SetAmmo(mqObject* self, mqObject* args) {
    return MQ_SetIntSequence(args, "iO:set_ammo", mqWeaponsType, "Weapons", MINQLX_WEAPONS,
                             mqGame ? mqGame->SetAmmo : NULL);
}

static mqObject* MQ_SetPowerups(mqObject* self, mqObject* args) {
    return MQ_SetIntSequence(args, "iO:set_powerups", mqPowerupsType, "Powerups", MINQLX_POWERUPS,
                             mqGame ? mqGame->SetPowerups : NULL);
}

static mqObject* MQ_SetFlight(mqObject* self, mqObject* args) {
    return MQ_SetIntSequence(args, "iO:set_flight", mqFlightType, "Flight", 4, mqGame ? mqGame->SetFlight : NULL);
}

static mqObject* MQ_ClientOnly(mqObject* args, const char* format, qboolean (*action)(int clientNum)) {
    int clientNum;

    if (!py.PyArg_ParseTuple(args, format, &clientNum)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    return MQ_Bool(action && action(clientNum));
}

static mqObject* MQ_DropHoldable(mqObject* self, mqObject* args) {
    return MQ_ClientOnly(args, "i:drop_holdable", mqGame ? mqGame->DropHoldable : NULL);
}

static mqObject* MQ_PlayerSpawn(mqObject* self, mqObject* args) {
    return MQ_ClientOnly(args, "i:player_spawn", mqGame ? mqGame->PlayerSpawn : NULL);
}

static mqObject* MQ_Callvote(mqObject* self, mqObject* args) {
    const char *vote, *display;
    int seconds = 30;

    if (!py.PyArg_ParseTuple(args, "ss|i:callvote", &vote, &display, &seconds)) {
        return NULL;
    }
    if (mqGame) {
        mqGame->Callvote(vote, display, seconds);
    }
    return MQ_None();
}

static mqObject* MQ_AllowSinglePlayer(mqObject* self, mqObject* args) {
    int allow;

    if (!py.PyArg_ParseTuple(args, "p:allow_single_player", &allow)) {
        return NULL;
    }
    if (mqGame) {
        mqGame->AllowSinglePlayer(allow ? qtrue : qfalse);
    }
    return MQ_None();
}

static mqObject* MQ_DestroyKamikazeTimers(mqObject* self, mqObject* args) {
    if (mqGame) {
        mqGame->DestroyKamikazeTimers();
    }
    return MQ_Bool(qtrue);
}

static mqObject* MQ_SpawnItem(mqObject* self, mqObject* args) {
    int item, x, y, z, numItems;

    if (!py.PyArg_ParseTuple(args, "iiii:spawn_item", &item, &x, &y, &z)) {
        return NULL;
    }
    numItems = mqGame ? mqGame->NumItems() : 0;
    if (item < 1 || item >= numItems) {
        return py.PyErr_Format(*py.ValueError, "item_id needs to be a number from 1 to %d.", numItems);
    }
    mqGame->SpawnItem(item, x, y, z);
    return MQ_Bool(qtrue);
}

static mqObject* MQ_RemoveDroppedItems(mqObject* self, mqObject* args) {
    if (mqGame) {
        mqGame->RemoveDroppedItems();
    }
    return MQ_Bool(qtrue);
}

static mqObject* MQ_SlayWithMod(mqObject* self, mqObject* args) {
    int clientNum, mod;

    if (!py.PyArg_ParseTuple(args, "ii:slay_with_mod", &clientNum, &mod)) {
        return NULL;
    }
    if (!MQ_ValidClient(clientNum)) {
        return MQ_BadClient();
    }
    return MQ_Bool(mqGame && mqGame->SlayWithMod(clientNum, mod));
}

static mqObject* MQ_ReplaceItems(mqObject* self, mqObject* args) {
    char entityClassname[MAX_STRING_CHARS], itemClassname[MAX_STRING_CHARS], error[256];
    mqObject *entity, *item;
    int entityNum = 0, itemNum = 0, result;
    qboolean entityByName = qfalse, itemByName = qfalse;

    if (!py.PyArg_ParseTuple(args, "OO:replace_items", &entity, &item)) {
        return NULL;
    }
    if (MQ_IsInstance(entity, py.LongType)) {
        entityNum = py.PyLong_AsLong(entity);
    } else if (MQ_CopyText(entity, entityClassname, sizeof(entityClassname))) {
        entityByName = qtrue;
    } else {
        return py.PyErr_Format(*py.ValueError, "entity needs to be type of int or string.");
    }
    if (MQ_IsInstance(item, py.LongType)) {
        itemNum = py.PyLong_AsLong(item);
    } else if (MQ_CopyText(item, itemClassname, sizeof(itemClassname))) {
        itemByName = qtrue;
    } else {
        return py.PyErr_Format(*py.ValueError, "item needs to be type of int or string.");
    }
    if (!mqGame) {
        return MQ_Bool(qfalse);
    }
    error[0] = '\0';
    result = mqGame->ReplaceItems(entityNum, entityByName ? entityClassname : NULL, itemNum,
                                  itemByName ? itemClassname : NULL, error, sizeof(error));
    if (result < 0) {
        py.PyErr_SetString(*py.ValueError, error);
        return NULL;
    }
    return MQ_Bool(result > 0);
}

static mqObject* MQ_DevPrintItems(mqObject* self, mqObject* args) {
    if (mqGame) {
        mqGame->DevPrintItems();
    }
    return MQ_None();
}

static mqObject* MQ_ForceWeaponRespawnTime(mqObject* self, mqObject* args) {
    int respawnTime;

    if (!py.PyArg_ParseTuple(args, "i:force_weapon_respawn_time", &respawnTime)) {
        return NULL;
    }
    if (respawnTime < 0) {
        return py.PyErr_Format(*py.ValueError, "respawn time needs to be an integer 0 or greater");
    }
    if (mqGame) {
        mqGame->ForceWeaponRespawnTime(respawnTime);
    }
    return MQ_Bool(qtrue);
}

static mqMethod_t mqMethods[] = {
    {"player_info", MQ_PlayerInfo, MQ_METH_VARARGS, "Returns a dictionary with information about a player by ID."},
    {"players_info", MQ_PlayersInfo, MQ_METH_NOARGS, "Returns a list with dictionaries with information about all the players on the server."},
    {"get_userinfo", MQ_GetUserinfo, MQ_METH_VARARGS, "Returns a string with a player's userinfo."},
    {"send_server_command", MQ_SendServerCommand, MQ_METH_VARARGS, "Sends a server command to either one specific client or all the clients."},
    {"client_command", MQ_ClientCommand, MQ_METH_VARARGS, "Tells the server to process a command from a specific client."},
    {"console_command", MQ_ConsoleCommand, MQ_METH_VARARGS, "Executes a command as if it was executed from the server console."},
    {"get_cvar", MQ_GetCvar, MQ_METH_VARARGS, "Gets a cvar."},
    {"set_cvar", MQ_SetCvar, MQ_METH_VARARGS, "Sets a cvar."},
    {"set_cvar_limit", MQ_SetCvarLimit, MQ_METH_VARARGS, "Sets a non-string cvar with a minimum and maximum value."},
    {"kick", MQ_Kick, MQ_METH_VARARGS, "Kick a player and allowing the admin to supply a reason for it."},
    {"console_print", MQ_ConsolePrint, MQ_METH_VARARGS, "Prints text on the console. If used during an RCON command, it will be printed in the player's console."},
    {"get_configstring", MQ_GetConfigstring, MQ_METH_VARARGS, "Get a configstring."},
    {"set_configstring", MQ_SetConfigstring, MQ_METH_VARARGS, "Sets a configstring and sends it to all the players on the server."},
    {"force_vote", MQ_ForceVote, MQ_METH_VARARGS, "Forces the current vote to either fail or pass."},
    {"add_console_command", MQ_AddConsoleCommand, MQ_METH_VARARGS, "Adds a console command that will be handled by Python code."},
    {"register_handler", MQ_RegisterHandler, MQ_METH_VARARGS, "Register an event handler. Can be called more than once per event, but only the last one will work."},
    {"player_state", MQ_PlayerState, MQ_METH_VARARGS, "Get information about the player's state in the game."},
    {"player_stats", MQ_PlayerStats, MQ_METH_VARARGS, "Get some player stats."},
    {"set_position", MQ_SetPosition, MQ_METH_VARARGS, "Sets a player's position vector."},
    {"set_velocity", MQ_SetVelocity, MQ_METH_VARARGS, "Sets a player's velocity vector."},
    {"noclip", MQ_NoClip, MQ_METH_VARARGS, "Sets noclip for a player."},
    {"set_health", MQ_SetHealth, MQ_METH_VARARGS, "Sets a player's health."},
    {"set_armor", MQ_SetArmor, MQ_METH_VARARGS, "Sets a player's armor."},
    {"set_weapons", MQ_SetWeapons, MQ_METH_VARARGS, "Sets a player's weapons."},
    {"set_weapon", MQ_SetWeapon, MQ_METH_VARARGS, "Sets a player's current weapon."},
    {"set_ammo", MQ_SetAmmo, MQ_METH_VARARGS, "Sets a player's ammo."},
    {"set_powerups", MQ_SetPowerups, MQ_METH_VARARGS, "Sets a player's powerups."},
    {"set_holdable", MQ_SetHoldable, MQ_METH_VARARGS, "Sets a player's holdable item."},
    {"drop_holdable", MQ_DropHoldable, MQ_METH_VARARGS, "Drops player's holdable item."},
    {"set_flight", MQ_SetFlight, MQ_METH_VARARGS, "Sets a player's flight parameters, such as current fuel, max fuel and, so on."},
    {"set_invulnerability", MQ_SetInvulnerability, MQ_METH_VARARGS, "Makes player invulnerable for limited time."},
    {"set_score", MQ_SetScore, MQ_METH_VARARGS, "Sets a player's score."},
    {"callvote", MQ_Callvote, MQ_METH_VARARGS, "Calls a vote as if started by the server and not a player."},
    {"allow_single_player", MQ_AllowSinglePlayer, MQ_METH_VARARGS, "Allows or disallows a game with only a single player in it to go on without forfeiting. Useful for race."},
    {"player_spawn", MQ_PlayerSpawn, MQ_METH_VARARGS, "Allows or disallows a game with only a single player in it to go on without forfeiting. Useful for race."},
    {"set_privileges", MQ_SetPrivileges, MQ_METH_VARARGS, "Sets a player's privileges. Does not persist."},
    {"destroy_kamikaze_timers", MQ_DestroyKamikazeTimers, MQ_METH_NOARGS, "Removes all current kamikaze timers."},
    {"spawn_item", MQ_SpawnItem, MQ_METH_VARARGS, "Spawns item with specified coordinates."},
    {"remove_dropped_items", MQ_RemoveDroppedItems, MQ_METH_NOARGS, "Removes all dropped items."},
    {"slay_with_mod", MQ_SlayWithMod, MQ_METH_VARARGS, "Slay player with mean of death."},
    {"replace_items", MQ_ReplaceItems, MQ_METH_VARARGS, "Replaces target entity's item with specified one."},
    {"dev_print_items", MQ_DevPrintItems, MQ_METH_NOARGS, "Prints all items and entity numbers to server console."},
    {"force_weapon_respawn_time", MQ_ForceWeaponRespawnTime, MQ_METH_VARARGS, "Force all weapons to have a specified respawn time, overriding custom map respawn times set for them."},
    {NULL, NULL, 0, NULL},
};

static mqModuleDef_t mqModuleDef = {
    1, NULL, NULL, 0, NULL, "minqlx", NULL, -1, mqMethods, NULL, NULL, NULL, NULL,
};

static const struct {
    const char* name;
    long value;
} mqConstants[] = {
    {"RET_NONE", MQ_RET_NONE}, {"RET_STOP", MQ_RET_STOP}, {"RET_STOP_EVENT", MQ_RET_STOP_EVENT},
    {"RET_STOP_ALL", MQ_RET_STOP_ALL}, {"RET_USAGE", MQ_RET_USAGE},
    {"PRI_HIGHEST", 0}, {"PRI_HIGH", 1}, {"PRI_NORMAL", 2}, {"PRI_LOW", 3}, {"PRI_LOWEST", 4},
    {"CVAR_ARCHIVE", CVAR_ARCHIVE}, {"CVAR_USERINFO", CVAR_USERINFO}, {"CVAR_SERVERINFO", CVAR_SERVERINFO},
    {"CVAR_SYSTEMINFO", CVAR_SYSTEMINFO}, {"CVAR_INIT", CVAR_INIT}, {"CVAR_LATCH", CVAR_LATCH},
    {"CVAR_ROM", CVAR_ROM}, {"CVAR_USER_CREATED", CVAR_USER_CREATED}, {"CVAR_TEMP", CVAR_TEMP},
    {"CVAR_CHEAT", CVAR_CHEAT}, {"CVAR_NORESTART", CVAR_NORESTART},
    {"PRIV_NONE", MQ_PRIV_NONE}, {"PRIV_MOD", MQ_PRIV_MOD}, {"PRIV_ADMIN", MQ_PRIV_ADMIN},
    {"PRIV_ROOT", MQ_PRIV_ROOT}, {"PRIV_BANNED", MQ_PRIV_BANNED},
    {"CS_FREE", CS_FREE}, {"CS_ZOMBIE", CS_ZOMBIE}, {"CS_CONNECTED", CS_CONNECTED}, {"CS_PRIMED", CS_PRIMED},
    {"CS_ACTIVE", CS_ACTIVE},
    {"TEAM_FREE", TEAM_FREE}, {"TEAM_RED", TEAM_RED}, {"TEAM_BLUE", TEAM_BLUE}, {"TEAM_SPECTATOR", TEAM_SPECTATOR},
    {"MOD_UNKNOWN", MOD_UNKNOWN}, {"MOD_SHOTGUN", MOD_SHOTGUN}, {"MOD_GAUNTLET", MOD_GAUNTLET},
    {"MOD_MACHINEGUN", MOD_MACHINEGUN}, {"MOD_GRENADE", MOD_GRENADE}, {"MOD_GRENADE_SPLASH", MOD_GRENADE_SPLASH},
    {"MOD_ROCKET", MOD_ROCKET}, {"MOD_ROCKET_SPLASH", MOD_ROCKET_SPLASH}, {"MOD_PLASMA", MOD_PLASMA},
    {"MOD_PLASMA_SPLASH", MOD_PLASMA_SPLASH}, {"MOD_RAILGUN", MOD_RAILGUN}, {"MOD_LIGHTNING", MOD_LIGHTNING},
    {"MOD_BFG", MOD_BFG}, {"MOD_BFG_SPLASH", MOD_BFG_SPLASH}, {"MOD_WATER", MOD_WATER}, {"MOD_SLIME", MOD_SLIME},
    {"MOD_LAVA", MOD_LAVA}, {"MOD_CRUSH", MOD_CRUSH}, {"MOD_TELEFRAG", MOD_TELEFRAG},
    {"MOD_FALLING", MOD_FALLING}, {"MOD_SUICIDE", MOD_SUICIDE}, {"MOD_TARGET_LASER", MOD_TARGET_LASER},
    {"MOD_TRIGGER_HURT", MOD_TRIGGER_HURT}, {"MOD_NAIL", MOD_NAIL}, {"MOD_CHAINGUN", MOD_CHAINGUN},
    {"MOD_PROXIMITY_MINE", MOD_PROXIMITY_MINE}, {"MOD_KAMIKAZE", MOD_KAMIKAZE}, {"MOD_JUICED", MOD_JUICED},
    {"MOD_GRAPPLE", MOD_GRAPPLE}, {"MOD_SWITCH_TEAMS", MOD_SWITCH_TEAMS}, {"MOD_THAW", MOD_THAW},
    {"MOD_LIGHTNING_DISCHARGE", MOD_LIGHTNING_DISCHARGE}, {"MOD_HMG", MOD_HMG},
    {"MOD_RAILGUN_HEADSHOT", MOD_RAILGUN_HEADSHOT},
};

static mqObject* MQ_InitModule(void) {
    mqObject* module = py.PyModule_Create2(&mqModuleDef, MQ_ABI_VERSION);
    int i;

    if (!module) {
        return NULL;
    }
    py.PyModule_AddStringConstant(module, "__version__", "ioqlds-" PRODUCT_VERSION);
    py.PyModule_AddObject(module, "DEBUG", MQ_Bool(qfalse));
    for (i = 0; i < (int)ARRAY_LEN(mqConstants); i++) {
        py.PyModule_AddIntConstant(module, mqConstants[i].name, mqConstants[i].value);
    }

    mqPlayerInfoType = py.PyStructSequence_NewType(&mqPlayerInfoDesc);
    mqPlayerStateType = py.PyStructSequence_NewType(&mqPlayerStateDesc);
    mqPlayerStatsType = py.PyStructSequence_NewType(&mqPlayerStatsDesc);
    mqVector3Type = py.PyStructSequence_NewType(&mqVector3Desc);
    mqWeaponsType = py.PyStructSequence_NewType(&mqWeaponsDesc);
    mqPowerupsType = py.PyStructSequence_NewType(&mqPowerupsDesc);
    mqFlightType = py.PyStructSequence_NewType(&mqFlightDesc);
    if (!mqPlayerInfoType || !mqPlayerStateType || !mqPlayerStatsType || !mqVector3Type || !mqWeaponsType ||
        !mqPowerupsType || !mqFlightType) {
        return NULL;
    }
    py.PyModule_AddObject(module, "PlayerInfo", MQ_Ref(mqPlayerInfoType));
    py.PyModule_AddObject(module, "PlayerState", MQ_Ref(mqPlayerStateType));
    py.PyModule_AddObject(module, "PlayerStats", MQ_Ref(mqPlayerStatsType));
    py.PyModule_AddObject(module, "Vector3", MQ_Ref(mqVector3Type));
    py.PyModule_AddObject(module, "Weapons", MQ_Ref(mqWeaponsType));
    py.PyModule_AddObject(module, "Powerups", MQ_Ref(mqPowerupsType));
    py.PyModule_AddObject(module, "Flight", MQ_Ref(mqFlightType));
    return module;
}

static int MQ_Lock(void) {
    return py.PyGILState_Ensure();
}

static void MQ_Unlock(int state) {
    py.PyGILState_Release(state);
}

static mqObject* MQ_Call(mqHandler_t handler, mqObject* result) {
    if (!result) {
        py.PyErr_Print();
        Com_Printf("minqlx: the %s handler raised an exception\n", mqHandlerNames[handler]);
    }
    return result;
}

static const char* MQ_StringResult(mqObject* result, const char* input, char* buffer, int size) {
    if (!result) {
        return input;
    }
    if (result == py.False) {
        return NULL;
    }
    if (MQ_CopyText(result, buffer, size)) {
        return buffer;
    }
    return input;
}

static const char* MQ_TextEvent(mqHandler_t handler, qboolean withId, int id, const char* text, char* buffer, int size) {
    const char* out;
    mqObject *value, *result;
    int gil;

    if (!mqHandlers[handler]) {
        return text;
    }
    gil = MQ_Lock();
    if (py.PyErr_Occurred()) {
        MQ_Unlock(gil);
        return text;
    }
    value = MQ_Text(text);
    if (withId) {
        result = MQ_Call(handler, py.PyObject_CallFunction(mqHandlers[handler], "iO", id, value));
    } else {
        result = MQ_Call(handler, py.PyObject_CallFunction(mqHandlers[handler], "O", value));
    }
    out = MQ_StringResult(result, text, buffer, size);
    if (value) {
        py.Py_DecRef(value);
    }
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
    return out;
}

const char* SV_MinqlxClientCommand(int clientNum, const char* cmd) {
    static char buffer[MQ_BUFFER];

    if (!mqActive) {
        return cmd;
    }
    return MQ_TextEvent(MQ_CLIENT_COMMAND, qtrue, clientNum, cmd, buffer, sizeof(buffer));
}

const char* SV_MinqlxServerCommand(int clientNum, const char* cmd) {
    static char buffer[MQ_BUFFER];

    if (!mqActive) {
        return cmd;
    }
    return MQ_TextEvent(MQ_SERVER_COMMAND, qtrue, clientNum, cmd, buffer, sizeof(buffer));
}

const char* SV_MinqlxSetConfigstring(int index, const char* value) {
    static char buffer[MQ_BUFFER];

    if (!mqActive || index == 16 || (index >= 662 && index < 670)) {
        return value;
    }
    return MQ_TextEvent(MQ_SET_CONFIGSTRING, qtrue, index, value, buffer, sizeof(buffer));
}

static qboolean MQ_ConsolePrintFilter(const char* text) {
    static MQ_THREAD char buffer[MQ_BUFFER];
    const char* out;

    if (!mqActive || !mqHandlers[MQ_CONSOLE_PRINT] || mqPrinting) {
        return qtrue;
    }
    mqPrinting++;
    out = MQ_TextEvent(MQ_CONSOLE_PRINT, qfalse, 0, text, buffer, sizeof(buffer));
    mqPrinting--;
    return out != NULL;
}

static void MQ_SimpleEvent(mqHandler_t handler, const char* format, int a, int b) {
    mqObject* result;
    int gil;

    if (!mqActive || !mqHandlers[handler]) {
        return;
    }
    gil = MQ_Lock();
    if (!format) {
        result = MQ_Call(handler, py.PyObject_CallObject(mqHandlers[handler], NULL));
    } else {
        result = MQ_Call(handler, py.PyObject_CallFunction(mqHandlers[handler], format, a, b));
    }
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
}

void SV_MinqlxFrame(void) {
    if (!sv_minqlxSkipFrame) {
        MQ_SimpleEvent(MQ_FRAME, NULL, 0, 0);
    }
}

void SV_MinqlxNewGame(qboolean restart) {
    mqObject* result;
    int gil;

    if (!mqActive || !mqHandlers[MQ_NEW_GAME]) {
        return;
    }
    gil = MQ_Lock();
    result = MQ_Call(MQ_NEW_GAME, py.PyObject_CallFunction(mqHandlers[MQ_NEW_GAME], "O", restart ? py.True : py.False));
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
}

void SV_MinqlxGameInit(qboolean restart) {
    const char* tags;

    if (!mqActive) {
        return;
    }
    if (!mqTagged) {
        mqTagged = qtrue;
        tags = Cvar_VariableString("sv_tags");
        if (strlen(tags) > 2) {
            Cbuf_ExecuteText(EXEC_INSERT, va("sv_tags \"minqlx,%s\"", tags));
        } else {
            Cbuf_ExecuteText(EXEC_INSERT, "sv_tags \"minqlx\"");
        }
    }
    if (restart) {
        SV_MinqlxNewGame(qtrue);
    }
}

void SV_MinqlxClientLoaded(int clientNum) {
    MQ_SimpleEvent(MQ_PLAYER_LOADED, "i", clientNum, 0);
}

void SV_MinqlxClientDisconnect(int clientNum, const char* reason) {
    mqObject* result;
    int gil;

    if (!mqActive || !mqHandlers[MQ_PLAYER_DISCONNECT]) {
        return;
    }
    gil = MQ_Lock();
    mqAllowFreeClient = clientNum;
    result = MQ_Call(MQ_PLAYER_DISCONNECT, py.PyObject_CallFunction(mqHandlers[MQ_PLAYER_DISCONNECT], "is",
                                                                   clientNum, reason ? reason : ""));
    mqAllowFreeClient = -1;
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
}

static const char* MQ_HookClientConnect(int clientNum, qboolean isBot) {
    static char buffer[MQ_BUFFER];
    const char* denied = NULL;
    mqObject* result;
    int gil;

    if (!mqActive || !mqHandlers[MQ_PLAYER_CONNECT]) {
        return NULL;
    }
    gil = MQ_Lock();
    mqAllowFreeClient = clientNum;
    result = MQ_Call(MQ_PLAYER_CONNECT, py.PyObject_CallFunction(mqHandlers[MQ_PLAYER_CONNECT], "iO", clientNum,
                                                                isBot ? py.True : py.False));
    mqAllowFreeClient = -1;
    if (result == py.False) {
        denied = "You are banned from this server.";
    } else if (result && MQ_CopyText(result, buffer, sizeof(buffer))) {
        denied = buffer;
    }
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
    return denied;
}

static void MQ_HookClientSpawn(int clientNum) {
    MQ_SimpleEvent(MQ_PLAYER_SPAWN, "i", clientNum, 0);
}

static void MQ_HookKamikazeUse(int clientNum) {
    MQ_SimpleEvent(MQ_KAMIKAZE_USE, "i", clientNum, 0);
}

static void MQ_HookKamikazeExplode(int clientNum, qboolean usedOnDemand) {
    MQ_SimpleEvent(MQ_KAMIKAZE_EXPLODE, "ii", clientNum, usedOnDemand);
}

static const minqlxHooks_t mqHooks = {
    MQ_HookClientConnect,
    MQ_HookClientSpawn,
    MQ_HookKamikazeUse,
    MQ_HookKamikazeExplode,
};

int SV_MinqlxLock(void) {
    return mqActive ? MQ_Lock() : -1;
}

void SV_MinqlxUnlock(int state) {
    if (mqActive && state != -1) {
        MQ_Unlock(state);
    }
}

void SV_MinqlxSetGame(void* proc) {
    int gil = SV_MinqlxLock();

    mqGame = (proc && mqActive) ? ((minqlxGameProc)proc)(&mqHooks) : NULL;
    SV_MinqlxUnlock(gil);
}

static void MQ_Rcon(const char* cmd) {
    mqObject* result;
    int gil;

    if (!mqActive || !mqHandlers[MQ_RCON]) {
        return;
    }
    gil = MQ_Lock();
    result = MQ_Call(MQ_RCON, py.PyObject_CallFunction(mqHandlers[MQ_RCON], "s", cmd));
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
}

static void MQ_CustomCommand_f(void) {
    mqObject* result;
    int gil;

    if (!mqActive || !mqHandlers[MQ_CUSTOM_COMMAND]) {
        return;
    }
    gil = MQ_Lock();
    result = MQ_Call(MQ_CUSTOM_COMMAND, py.PyObject_CallFunction(mqHandlers[MQ_CUSTOM_COMMAND], "s", Cmd_Args()));
    if (result == py.False) {
        Com_Printf("The command failed to be executed. pyminqlx found no handler.\n");
    }
    if (result) {
        py.Py_DecRef(result);
    }
    MQ_Unlock(gil);
}

static void MQ_Cmd_f(void) {
    SV_SendServerCommandRaw(NULL, "%s\n", Cmd_Args());
}

static void MQ_CenterPrint_f(void) {
    SV_SendServerCommandRaw(NULL, "cp \"%s\"\n", Cmd_Args());
}

static void MQ_Print_f(void) {
    SV_SendServerCommandRaw(NULL, "print \"%s\n\"\n", Cmd_Args());
}

static int MQ_TargetClient(int args) {
    int clientNum;

    if (Cmd_Argc() < 2) {
        Com_Printf(args > 1 ? "Usage: %s <client_id> [damage]\n" : "Usage: %s <client_id>\n", Cmd_Argv(0));
        return -1;
    }
    clientNum = atoi(Cmd_Argv(1));
    if (clientNum < 0 || clientNum > sv_maxclients->integer) {
        Com_Printf("client_id must be a number between 0 and %d\n.", sv_maxclients->integer);
        return -1;
    }
    return clientNum;
}

static void MQ_Slap_f(void) {
    int clientNum = MQ_TargetClient(2);
    int damage;

    if (clientNum < 0) {
        return;
    }
    damage = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 0;
    if (!mqGame || clientNum >= sv_maxclients->integer || !mqGame->IsAlive(clientNum)) {
        Com_Printf("The player is currently not active.\n");
        return;
    }
    Com_Printf("Slapping...\n");
    if (damage) {
        SV_SendServerCommandRaw(NULL, "print \"%s^7 was slapped for %d damage!\n\"\n", svs.clients[clientNum].name,
                                damage);
    } else {
        SV_SendServerCommandRaw(NULL, "print \"%s^7 was slapped!\n\"\n", svs.clients[clientNum].name);
    }
    mqGame->Slap(clientNum, damage);
}

static void MQ_Slay_f(void) {
    int clientNum = MQ_TargetClient(1);

    if (clientNum < 0) {
        return;
    }
    if (!mqGame || clientNum >= sv_maxclients->integer || !mqGame->IsAlive(clientNum)) {
        Com_Printf("The player is currently not active.\n");
        return;
    }
    Com_Printf("Slaying player...\n");
    SV_SendServerCommandRaw(NULL, "print \"%s^7 was slain!\n\"\n", svs.clients[clientNum].name);
    mqGame->Slay(clientNum);
}

static void MQ_Qlx_f(void) {
    MQ_Rcon(Cmd_Args());
}

static qboolean MQ_RunCode(const char* source) {
    mqObject *mainModule, *globals, *code, *result;

    mainModule = py.PyImport_AddModule("__main__");
    globals = py.PyModule_GetDict(mainModule);
    code = py.Py_CompileString(source, "<minqlx>", MQ_FILE_INPUT);
    if (!code) {
        py.PyErr_Print();
        return qfalse;
    }
    result = py.PyEval_EvalCode(code, globals, globals);
    py.Py_DecRef(code);
    if (!result) {
        py.PyErr_Print();
        return qfalse;
    }
    py.Py_DecRef(result);
    return py.PyDict_GetItemString(globals, "ret") == py.True;
}

static void MQ_PyRestart_f(void) {
    qboolean loaded;
    int gil, i;

    if (!mqActive) {
        return;
    }
    Com_Printf("Restarting Python...\n");
    gil = MQ_Lock();
    for (i = 0; i < MQ_NUM_HANDLERS; i++) {
        if (mqHandlers[i]) {
            py.Py_DecRef(mqHandlers[i]);
            mqHandlers[i] = NULL;
        }
    }
    MQ_RunCode(mqUnloader);
    loaded = MQ_RunCode(mqLoader);
    MQ_Unlock(gil);
    if (!loaded) {
        Com_Printf("minqlx: minqlx could not be loaded again\n");
        return;
    }
    SV_MinqlxNewGame(qfalse);
}

static void* MQ_OpenLibrary(const char* name) {
#ifdef _WIN32
    return (void*)LoadLibraryA(name);
#else
    return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
#endif
}

static void* MQ_Symbol(const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)py.lib, name);
#else
    return dlsym(py.lib, name);
#endif
}

static qboolean MQ_OtherPythonRunning(void) {
#ifdef _WIN32
    static const char* const names[] = {"python3.dll", "python313.dll", "python312.dll", "python311.dll",
                                        "python310.dll", "python39.dll", "python38.dll"};
    int i;

    for (i = 0; i < (int)ARRAY_LEN(names); i++) {
        HMODULE lib = GetModuleHandleA(names[i]);
        int (*initialized)(void) = lib ? (int (*)(void))GetProcAddress(lib, "Py_IsInitialized") : NULL;

        if (initialized && initialized()) {
            return qtrue;
        }
    }
    return qfalse;
#else
    void* self = dlopen(NULL, RTLD_NOW);
    int (*initialized)(void) = self ? (int (*)(void))dlsym(self, "Py_IsInitialized") : NULL;

    return initialized && initialized();
#endif
}

static qboolean MQ_LoadPython(void) {
    char name[MAX_OSPATH];
    cvar_t* library = Cvar_Get("qlx_pythonLibrary", "", CVAR_INIT);
    int minor, i;

    if (library->string[0]) {
        py.lib = MQ_OpenLibrary(library->string);
    }
#ifdef _WIN32
    if (!py.lib) {
        py.lib = MQ_OpenLibrary("python3.dll");
    }
    for (minor = 14; !py.lib && minor >= 5; minor--) {
        Com_sprintf(name, sizeof(name), "python3%d.dll", minor);
        py.lib = MQ_OpenLibrary(name);
    }
#else
    if (!py.lib) {
        py.lib = MQ_OpenLibrary("libpython3.so");
    }
    for (minor = 14; !py.lib && minor >= 5; minor--) {
        Com_sprintf(name, sizeof(name), "libpython3.%d.so.1.0", minor);
        py.lib = MQ_OpenLibrary(name);
        if (!py.lib) {
            Com_sprintf(name, sizeof(name), "libpython3.%dm.so.1.0", minor);
            py.lib = MQ_OpenLibrary(name);
        }
    }
#endif
    if (!py.lib) {
        Com_Printf("minqlx: no Python 3 library was found (set qlx_pythonLibrary to its path)\n");
        return qfalse;
    }
    for (i = 0; i < (int)ARRAY_LEN(mqSymbols); i++) {
        *mqSymbols[i].address = MQ_Symbol(mqSymbols[i].name);
        if (!*mqSymbols[i].address && !mqSymbols[i].optional) {
            Com_Printf("minqlx: the Python library has no %s\n", mqSymbols[i].name);
            return qfalse;
        }
    }
    if (!py.Py_GetVersion() || py.Py_GetVersion()[0] != '3' || atoi(py.Py_GetVersion() + 2) < 5) {
        Com_Printf("minqlx: Python %s is too old, 3.5 or newer is needed\n", py.Py_GetVersion());
        return qfalse;
    }
    return qtrue;
}

#ifdef _WIN32
static void MQ_SetHome(void) {
    static wchar_t home[MAX_OSPATH];
    wchar_t landmark[MAX_OSPATH];
    wchar_t* slash;

    if (getenv("PYTHONHOME") || !py.Py_SetPythonHome) {
        return;
    }
    if (!GetModuleFileNameW(GetModuleHandleW(L"python3.dll") ? GetModuleHandleW(L"python3.dll") : (HMODULE)py.lib,
                            home, MAX_OSPATH)) {
        return;
    }
    slash = wcsrchr(home, L'\\');
    if (!slash) {
        return;
    }
    *slash = L'\0';
    _snwprintf(landmark, MAX_OSPATH, L"%ls\\Lib\\os.py", home);
    landmark[MAX_OSPATH - 1] = L'\0';
    if (GetFileAttributesW(landmark) != INVALID_FILE_ATTRIBUTES) {
        py.Py_SetPythonHome(home);
    }
}
#endif

static qboolean MQ_Start(void) {
    mqObject* module;

#ifdef _WIN32
    MQ_SetHome();
#else
    if (py.Py_SetProgramName) {
        py.Py_SetProgramName(L"python3");
    }
#endif
    py.Py_InitializeEx(0);
    if (!py.Py_IsInitialized()) {
        Com_Printf("minqlx: Python did not initialize\n");
        return qfalse;
    }
    if (py.PyEval_InitThreads && atoi(py.Py_GetVersion() + 2) < 7) {
        py.PyEval_InitThreads();
    }

    module = MQ_InitModule();
    if (!module) {
        py.PyErr_Print();
        return qfalse;
    }
    py.PyDict_SetItemString(py.PyImport_GetModuleDict(), "_minqlx", module);
    py.Py_DecRef(module);

    if (!MQ_RunCode(mqLoader)) {
        return qfalse;
    }

    py.PyEval_SaveThread();
    mqActive = qtrue;
    com_printFilter = MQ_ConsolePrintFilter;
    return qtrue;
}

static qboolean MQ_Wanted(void) {
    FILE* f = fopen("minqlx.zip", "rb");

    if (!f) {
        f = fopen("minqlx/__init__.py", "rb");
    }
    if (!f) {
        return qfalse;
    }
    fclose(f);
    return qtrue;
}

void SV_MinqlxInit(void) {
    if (!com_dedicated->integer || !MQ_Wanted()) {
        return;
    }
    if (MQ_OtherPythonRunning()) {
        Com_Printf("minqlx: another Python core is already running in this process, the built-in one stays off\n");
        return;
    }
    Com_Printf("minqlx: loading Python...\n");
    if (!MQ_LoadPython()) {
        Com_Error(ERR_FATAL, "minqlx: minqlx.zip is here but Python 3 could not be loaded");
    }
    Cmd_AddCommand("cmd", MQ_Cmd_f);
    Cmd_AddCommand("cp", MQ_CenterPrint_f);
    Cmd_AddCommand("print", MQ_Print_f);
    Cmd_AddCommand("slap", MQ_Slap_f);
    Cmd_AddCommand("slay", MQ_Slay_f);
    Cmd_AddCommand("qlx", MQ_Qlx_f);
    Cmd_AddCommand("pycmd", MQ_CustomCommand_f);
    Cmd_AddCommand("pyrestart", MQ_PyRestart_f);
    if (!MQ_Start()) {
        Com_Error(ERR_FATAL, "minqlx: Python initialization failed");
    }
    Com_Printf("minqlx: Python %.6s ready\n", py.Py_GetVersion());
}
