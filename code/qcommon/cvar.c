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
// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"

cvar_t* cvar_vars = NULL;
cvar_t* cvar_cheats;
int cvar_modifiedFlags;

#define MAX_CVARS 2048
cvar_t cvar_indexes[MAX_CVARS];
int cvar_numIndexes;

#define FILE_HASH_SIZE 256
static cvar_t* hashTable[FILE_HASH_SIZE];

/*
================
Cvar_ComputeNumeric

[QL] Fill var->value / var->integer from var->string. A leading "0x"/"0X" is
parsed as hex, so packed-colour cvars (cg_enemyLowerColor "0x2a8000FF",
cg_screenDamage, cg_deadBodyColor, ...) carry the packed value in .integer the
way the shipped engine does. Everything else stays plain decimal - the prefix
test avoids strtol's octal handling for leading-zero decimals.
================
*/
static void Cvar_ComputeNumeric(cvar_t* var) {
    const char* s = var->string;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && s[2] != '\0') {
        var->integer = (int)strtoul(s, NULL, 16);
        var->value = (float)var->integer;
    } else {
        var->value = atof(s);
        var->integer = atoi(s);
    }
}

/*
================
return a hash value for the filename
================
*/
static long generateHashValue(const char* fname) {
    int i;
    long hash;
    char letter;

    hash = 0;
    i = 0;
    while (fname[i] != '\0') {
        letter = tolower(fname[i]);
        hash += (long)(letter) * (i + 119);
        i++;
    }
    hash &= (FILE_HASH_SIZE - 1);
    return hash;
}

/*
============
Cvar_ValidateString
============
*/
static qboolean Cvar_ValidateString(const char* s) {
    if (!s) {
        return qfalse;
    }
    if (strchr(s, '\\')) {
        return qfalse;
    }
    if (strchr(s, '\"')) {
        return qfalse;
    }
    if (strchr(s, ';')) {
        return qfalse;
    }
    return qtrue;
}

/*
============
Cvar_FindVar
============
*/
static cvar_t* Cvar_FindVar(const char* var_name) {
    cvar_t* var;
    long hash;

    hash = generateHashValue(var_name);

    for (var = hashTable[hash]; var; var = var->hashNext) {
        if (!Q_stricmp(var_name, var->name)) {
            return var;
        }
    }

    return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(const char* var_name) {
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return var->value;
}

/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableIntegerValue(const char* var_name) {
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return var->integer;
}

/*
============
Cvar_VariableString
============
*/
char* Cvar_VariableString(const char* var_name) {
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return "";
    return var->string;
}

/*
============
Cvar_VariableStringBuffer
============
*/
void Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize) {
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var) {
        *buffer = 0;
    } else {
        Q_strncpyz(buffer, var->string, bufsize);
    }
}

/*
============
Cvar_LatchedVariableStringBuffer

[QL] The value a cvar is going to have: its latched string while a change
waits for a restart, else its string. Retail's GetConfig gave the web UI its
cvars this way (quakelive_steam.exe 0x433e10).
============
*/
void Cvar_LatchedVariableStringBuffer(const char* var_name, char* buffer, int bufsize) {
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var) {
        *buffer = 0;
    } else {
        Q_strncpyz(buffer, var->latchedString ? var->latchedString : var->string, bufsize);
    }
}

/*
============
Cvar_Flags
============
*/
int Cvar_Flags(const char* var_name) {
    cvar_t* var;

    if (!(var = Cvar_FindVar(var_name)))
        return CVAR_NONEXISTENT;
    else {
        if (var->modified)
            return var->flags | CVAR_MODIFIED;
        else
            return var->flags;
    }
}

/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion(void (*callback)(const char* s)) {
    cvar_t* cvar;

    for (cvar = cvar_vars; cvar; cvar = cvar->next) {
        if (cvar->name)
            callback(cvar->name);
    }
}

/*
============
Cvar_Validate
============
*/
static const char* Cvar_Validate(cvar_t* var,
                                 const char* value,
                                 qboolean warn) {
    static char s[MAX_CVAR_VALUE_STRING];
    float valuef;
    qboolean changed = qfalse;

    if (!var->validate)
        return value;

    if (!value)
        return value;

    if (Q_isanumber(value)) {
        valuef = atof(value);

        if (var->integral) {
            if (!Q_isintegral(valuef)) {
                if (warn)
                    Com_Printf("WARNING: cvar '%s' must be integral", var->name);

                valuef = (int)valuef;
                changed = qtrue;
            }
        }
    } else {
        if (warn)
            Com_Printf("WARNING: cvar '%s' must be numeric", var->name);

        valuef = atof(var->resetString);
        changed = qtrue;
    }

    if (valuef < var->min) {
        if (warn) {
            if (changed)
                Com_Printf(" and is");
            else
                Com_Printf("WARNING: cvar '%s'", var->name);

            if (Q_isintegral(var->min))
                Com_Printf(" out of range (min %d)", (int)var->min);
            else
                Com_Printf(" out of range (min %f)", var->min);
        }

        valuef = var->min;
        changed = qtrue;
    } else if (valuef > var->max) {
        if (warn) {
            if (changed)
                Com_Printf(" and is");
            else
                Com_Printf("WARNING: cvar '%s'", var->name);

            if (Q_isintegral(var->max))
                Com_Printf(" out of range (max %d)", (int)var->max);
            else
                Com_Printf(" out of range (max %f)", var->max);
        }

        valuef = var->max;
        changed = qtrue;
    }

    if (changed) {
        if (Q_isintegral(valuef)) {
            Com_sprintf(s, sizeof(s), "%d", (int)valuef);

            if (warn)
                Com_Printf(", setting to %d\n", (int)valuef);
        } else {
            Com_sprintf(s, sizeof(s), "%f", valuef);

            if (warn)
                Com_Printf(", setting to %f\n", valuef);
        }

        return s;
    } else
        return value;
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t* Cvar_Get(const char* var_name, const char* var_value, int flags) {
    cvar_t* var;
    long hash;
    int index;

    if (!var_name || !var_value) {
        Com_Error(ERR_FATAL, "Cvar_Get: NULL parameter");
    }

    if (!Cvar_ValidateString(var_name)) {
        Com_Printf("invalid cvar name string: %s\n", var_name);
        var_name = "BADNAME";
    }

#if 0  // FIXME: values with backslash happen
	if ( !Cvar_ValidateString( var_value ) ) {
		Com_Printf("invalid cvar value string: %s\n", var_value );
		var_value = "BADVALUE";
	}
#endif

    var = Cvar_FindVar(var_name);

    if (var) {
        var_value = Cvar_Validate(var, var_value, qfalse);

        // Make sure the game code cannot mark engine-added variables as gamecode vars
        if (var->flags & CVAR_VM_CREATED) {
            if (!(flags & CVAR_VM_CREATED))
                var->flags &= ~CVAR_VM_CREATED;
        } else if (!(var->flags & CVAR_USER_CREATED)) {
            if (flags & CVAR_VM_CREATED)
                flags &= ~CVAR_VM_CREATED;
        }

        // if the C code is now specifying a variable that the user already
        // set a value for, take the new value as the reset value
        if (var->flags & CVAR_USER_CREATED) {
            var->flags &= ~CVAR_USER_CREATED;
            Z_Free(var->resetString);
            var->resetString = CopyString(var_value);

            if (flags & CVAR_ROM) {
                // this variable was set by the user,
                // so force it to value given by the engine.

                if (var->latchedString)
                    Z_Free(var->latchedString);

                var->latchedString = CopyString(var_value);
            }
        }

        var->flags |= flags;

        // only allow one non-empty reset string without a warning
        if (!var->resetString[0]) {
            // we don't have a reset string yet
            Z_Free(var->resetString);
            var->resetString = CopyString(var_value);
        } else if (var_value[0] && strcmp(var->resetString, var_value)) {
            Com_DPrintf("Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
                        var_name, var->resetString, var_value);
        }
        // if we have a latched string, take that value now
        if (var->latchedString) {
            char* s;

            s = var->latchedString;
            var->latchedString = NULL;  // otherwise cvar_set2 would free it
            Cvar_Set2(var_name, s, qtrue);
            Z_Free(s);
        }

        // ZOID--needs to be set so that cvars the game sets as
        // SERVERINFO get sent to clients
        cvar_modifiedFlags |= flags;

        return var;
    }

    //
    // allocate a new cvar
    //

    // find a free cvar
    for (index = 0; index < MAX_CVARS; index++) {
        if (!cvar_indexes[index].name)
            break;
    }

    if (index >= MAX_CVARS) {
        if (!com_errorEntered)
            Com_Error(ERR_FATAL, "Error: Too many cvars, cannot create a new one!");

        return NULL;
    }

    var = &cvar_indexes[index];

    if (index >= cvar_numIndexes)
        cvar_numIndexes = index + 1;

    var->name = CopyString(var_name);
    var->string = CopyString(var_value);
    var->modified = qtrue;
    var->modificationCount = 1;
    Cvar_ComputeNumeric(var);
    var->resetString = CopyString(var_value);
    var->validate = qfalse;
    // [ioquakelive] a Quake Live cvar gets its help text as it is created, the modules'
    // cvars included (cvar_help.c)
    {
        const char* help = Cvar_BuiltInHelp(var_name);

        var->description = help ? CopyString(help) : NULL;
    }
    var->minString = NULL;
    var->maxString = NULL;

    // link the variable in
    var->next = cvar_vars;
    if (cvar_vars)
        cvar_vars->prev = var;

    var->prev = NULL;
    cvar_vars = var;

    var->flags = flags;
    // note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
    cvar_modifiedFlags |= var->flags;

    hash = generateHashValue(var_name);
    var->hashIndex = hash;

    var->hashNext = hashTable[hash];
    if (hashTable[hash])
        hashTable[hash]->hashPrev = var;

    var->hashPrev = NULL;
    hashTable[hash] = var;

    return var;
}

/*
============
Cvar_RangeValue

[QL] quakelive_steam.exe 0x4cd0a4: with a range, a value above it gives the maximum's string and
one below it the minimum's; any other value, or a cvar without a range, is kept as it is
============
*/
static const char* Cvar_RangeValue(const cvar_t* var, const char* value) {
    float f;

    if (!var->minString) {
        return value;
    }
    f = atof(value);
    if (f > var->max) {
        return var->maxString;
    }
    if (f < var->min) {
        return var->minString;
    }
    return value;
}

/*
============
Cvar_ClampToRange

[QL] quakelive_steam.exe 0x4cdf0d and 0x4cd1e9: a current value outside the range is replaced by
the bound's string, with no warning and no modification of its own
============
*/
static void Cvar_ClampToRange(cvar_t* var) {
    const char* bound;

    if (!var->minString) {
        return;
    }
    if (var->value > var->max) {
        bound = var->maxString;
    } else if (var->value < var->min) {
        bound = var->minString;
    } else {
        return;
    }
    Z_Free(var->string);
    var->string = CopyString(bound);
    Cvar_ComputeNumeric(var);
}

/*
============
Cvar_GetRange

[QL] quakelive_steam.exe 0x4cdd30: Cvar_Get with a range given as two strings. The range replaces
any earlier one and the value is brought inside it right away; Cvar_Set2 does the same with every
later value. Nothing is printed for a value out of range.
============
*/
cvar_t* Cvar_GetRange(const char* var_name, const char* var_value, const char* min_value, const char* max_value, int flags) {
    cvar_t* var;

    if (!min_value || !max_value) {
        Com_Error(ERR_FATAL, "Cvar_GetRange: NULL parameter");
    }

    var = Cvar_Get(var_name, var_value, flags);
    if (!var) {
        return NULL;
    }

    if (var->minString) {
        Z_Free(var->minString);
    }
    if (var->maxString) {
        Z_Free(var->maxString);
    }
    var->minString = CopyString(min_value);
    var->maxString = CopyString(max_value);
    var->min = atof(min_value);
    var->max = atof(max_value);

    Cvar_ClampToRange(var);
    return var;
}

/*
============
Cvar_Print

Prints the value, default, and latched string of the given variable
============
*/
void Cvar_Print(cvar_t* v) {
    Com_Printf("\"%s\" is:\"%s" S_COLOR_WHITE "\"",
               v->name, v->string);

    if (!(v->flags & CVAR_ROM)) {
        if (!Q_stricmp(v->string, v->resetString)) {
            Com_Printf(", the default");
        } else {
            Com_Printf(" default:\"%s" S_COLOR_WHITE "\"",
                       v->resetString);
        }
    }

    Com_Printf("\n");

    if (v->latchedString) {
        Com_Printf("latched: \"%s\"\n", v->latchedString);
    }

    if (v->description) {
        Com_Printf("%s\n", v->description);
    }
}

/*
============
Cvar_Set2
============
*/
cvar_t* Cvar_Set2(const char* var_name, const char* value, qboolean force) {
    cvar_t* var;

    //	Com_DPrintf( "Cvar_Set2: %s %s\n", var_name, value );

    if (!Cvar_ValidateString(var_name)) {
        Com_Printf("invalid cvar name string: %s\n", var_name);
        var_name = "BADNAME";
    }

#if 0  // FIXME
	if ( value && !Cvar_ValidateString( value ) ) {
		Com_Printf("invalid cvar value string: %s\n", value );
		var_value = "BADVALUE";
	}
#endif

    var = Cvar_FindVar(var_name);
    if (!var) {
        if (!value) {
            return NULL;
        }
        // create it
        if (!force) {
            return Cvar_Get(var_name, value, CVAR_USER_CREATED);
        } else {
            return Cvar_Get(var_name, value, 0);
        }
    }

    if (!value) {
        value = var->resetString;
    }

    value = Cvar_Validate(var, value, qtrue);

    if ((var->flags & CVAR_LATCH) && var->latchedString) {
        if (!strcmp(value, var->string)) {
            Z_Free(var->latchedString);
            var->latchedString = NULL;
            return var;
        }

        if (!strcmp(value, var->latchedString))
            return var;
    } else if (!strcmp(value, var->string))
        return var;

    // note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
    cvar_modifiedFlags |= var->flags;

    if (!force) {
        if (var->flags & CVAR_ROM) {
            Com_Printf("%s is read only.\n", var_name);
            return var;
        }

        if (var->flags & CVAR_INIT) {
            Com_Printf("%s is write protected.\n", var_name);
            return var;
        }

        if ((var->flags & CVAR_CHEAT) && !cvar_cheats->integer) {
            Com_Printf("%s is cheat protected.\n", var_name);
            return var;
        }

        if (var->flags & CVAR_LATCH) {
            if (var->latchedString) {
                if (strcmp(value, var->latchedString) == 0)
                    return var;
                Z_Free(var->latchedString);
            } else {
                if (strcmp(value, var->string) == 0)
                    return var;
            }

            Com_Printf("%s will be changed upon restarting.\n", var_name);
            // [QL] 0x4cd0a4: a value outside the range is latched as the bound's string
            var->latchedString = CopyString(Cvar_RangeValue(var, value));
            var->modified = qtrue;
            var->modificationCount++;
            return var;
        }
    } else {
        if (var->latchedString) {
            Z_Free(var->latchedString);
            var->latchedString = NULL;
        }
    }

    if (!strcmp(value, var->string))
        return var;  // not changed

    var->modified = qtrue;
    var->modificationCount++;

    Z_Free(var->string);  // free the old value string

    var->string = CopyString(value);
    Cvar_ComputeNumeric(var);
    // [QL] 0x4cd1e9: a value outside the range becomes the bound's string
    Cvar_ClampToRange(var);

    return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set(const char* var_name, const char* value) {
    Cvar_Set2(var_name, value, qtrue);
}

/*
============
Cvar_SetSafe
============
*/
void Cvar_SetSafe(const char* var_name, const char* value) {
    int flags = Cvar_Flags(var_name);

    if ((flags != CVAR_NONEXISTENT) && (flags & CVAR_PROTECTED)) {
        if (value)
            Com_Error(ERR_DROP,
                      "Restricted source tried to set "
                      "\"%s\" to \"%s\"",
                      var_name, value);
        else
            Com_Error(ERR_DROP,
                      "Restricted source tried to "
                      "modify \"%s\"",
                      var_name);
        return;
    }
    Cvar_Set(var_name, value);
}

/*
============
Cvar_SetLatched
============
*/
void Cvar_SetLatched(const char* var_name, const char* value) {
    Cvar_Set2(var_name, value, qfalse);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue(const char* var_name, float value) {
    char val[32];

    if (value == (int)value) {
        Com_sprintf(val, sizeof(val), "%i", (int)value);
    } else {
        Com_sprintf(val, sizeof(val), "%f", value);
    }
    Cvar_Set(var_name, val);
}

/*
============
Cvar_SetValueSafe
============
*/
void Cvar_SetValueSafe(const char* var_name, float value) {
    char val[32];

    if (Q_isintegral(value))
        Com_sprintf(val, sizeof(val), "%i", (int)value);
    else
        Com_sprintf(val, sizeof(val), "%f", value);
    Cvar_SetSafe(var_name, val);
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset(const char* var_name) {
    Cvar_Set2(var_name, NULL, qfalse);
}

/*
============
Cvar_ForceReset
============
*/
void Cvar_ForceReset(const char* var_name) {
    Cvar_Set2(var_name, NULL, qtrue);
}

/*
============
Cvar_SaveFactoryValue

[QL] Keeps the current value of a cvar a factory is about to set, replacing
whatever an earlier factory kept. quakelive_steam.exe 0x4cd370.
============
*/
qboolean Cvar_SaveFactoryValue(const char* var_name) {
    cvar_t* var = Cvar_FindVar(var_name);

    if (!var) {
        return qfalse;
    }
    if (var->factoryString) {
        Z_Free(var->factoryString);
    }
    var->factoryString = CopyString(var->string);
    return qtrue;
}

/*
============
Cvar_RestoreFactoryValue

[QL] Sets a cvar back to the value kept before a factory set it.
quakelive_steam.exe 0x4cd3b0.
============
*/
qboolean Cvar_RestoreFactoryValue(const char* var_name) {
    cvar_t* var = Cvar_FindVar(var_name);
    char* value;

    if (!var || !var->factoryString) {
        return qfalse;
    }
    value = var->factoryString;
    var->factoryString = NULL;
    Cvar_Set2(var_name, value, qtrue);
    Z_Free(value);
    return qtrue;
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState(void) {
    cvar_t* var;

    // set all default vars to the safe value
    for (var = cvar_vars; var; var = var->next) {
        if (var->flags & CVAR_CHEAT) {
            // the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here
            // because of a different var->latchedString
            if (var->latchedString) {
                Z_Free(var->latchedString);
                var->latchedString = NULL;
            }
            if (strcmp(var->resetString, var->string))
                Cvar_Set(var->name, var->resetString);
        }
    }
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command(void) {
    cvar_t* v;

    // check variables
    v = Cvar_FindVar(Cmd_Argv(0));
    if (!v) {
        return qfalse;
    }

    // perform a variable print or set
    if (Cmd_Argc() == 1) {
        Cvar_Print(v);
        return qtrue;
    }

    // set the value if forcing isn't required
    Cvar_Set2(v->name, Cmd_Args(), qfalse);
    return qtrue;
}

/*
============
Cvar_Print_f

Prints the contents of a cvar
(preferred over Cvar_Command where cvar names and commands conflict)
============
*/
void Cvar_Print_f(void) {
    char* name;
    cvar_t* cv;

    if (Cmd_Argc() != 2) {
        Com_Printf("usage: print <variable>\n");
        return;
    }

    name = Cmd_Argv(1);

    cv = Cvar_FindVar(name);

    if (cv)
        Cvar_Print(cv);
    else
        Com_Printf("Cvar %s does not exist.\n", name);
}

/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding, optionally through a list of
given values
============
*/
void Cvar_Toggle_f(void) {
    int i, c = Cmd_Argc();
    char* curval;

    if (c < 2) {
        Com_Printf("usage: toggle <variable> [value1, value2, ...]\n");
        return;
    }

    if (c == 2) {
        Cvar_Set2(Cmd_Argv(1), va("%d", !Cvar_VariableValue(Cmd_Argv(1))),
                  qfalse);
        return;
    }

    if (c == 3) {
        Com_Printf("toggle: nothing to toggle to\n");
        return;
    }

    curval = Cvar_VariableString(Cmd_Argv(1));

    // don't bother checking the last arg for a match since the desired
    // behaviour is the same as no match (set to the first argument)
    for (i = 2; i + 1 < c; i++) {
        if (strcmp(curval, Cmd_Argv(i)) == 0) {
            Cvar_Set2(Cmd_Argv(1), Cmd_Argv(i + 1), qfalse);
            return;
        }
    }

    // fallback
    Cvar_Set2(Cmd_Argv(1), Cmd_Argv(2), qfalse);
}

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_Set_f(void) {
    int c;
    char* cmd;
    cvar_t* v;

    c = Cmd_Argc();
    cmd = Cmd_Argv(0);

    if (c < 2) {
        Com_Printf("usage: %s <variable> <value>\n", cmd);
        return;
    }
    if (c == 2) {
        Cvar_Print_f();
        return;
    }

    v = Cvar_Set2(Cmd_Argv(1), Cmd_ArgsFrom(2), qfalse);
    if (!v) {
        return;
    }
    // "seta" - mark as archived
    if (cmd[3] == 'a') {
        if (!(v->flags & CVAR_ARCHIVE)) {
            v->flags |= CVAR_ARCHIVE;
            cvar_modifiedFlags |= CVAR_ARCHIVE;
        }
    }
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f(void) {
    if (Cmd_Argc() != 2) {
        Com_Printf("usage: reset <variable>\n");
        return;
    }
    Cvar_Reset(Cmd_Argv(1));
}

/*
============
Cvar_WriteVariables

[QL] Writes archived cvars to two files:
  f_hw  = qzconfig.cfg (hardware settings)
  f_rep = repconfig.cfg (replicated/user-created settings)

Routing logic (matches binary):
  - CVAR_REPLICATE (0x800) cvars -> repconfig
  - CVAR_USER_CREATED (0x80) cvars -> repconfig
  - Other CVAR_ARCHIVE (0x1) cvars -> qzconfig

Pass the same handle for both to write everything to one file.
Skips "password" cvar (matches binary).

clientOnly: if non-zero (Com_WriteClientConfig_f calls Cvar_WriteVariables(f, f, 1)) only
writes CVAR_USERSAVE cvars (flag 0x80000, the "CVAR_USERINFO_ALL" subset) whose value differs
from their resetString. Otherwise writes all CVAR_ARCHIVE or CVAR_REPLICATE cvars.
============
*/
void Cvar_WriteVariables(fileHandle_t f_hw, fileHandle_t f_rep, int clientOnly) {
    cvar_t* var;
    char buffer[2048];
    const char* value;
    fileHandle_t dest;

    for (var = cvar_vars; var; var = var->next) {
        // [QL] skip sensitive cvars
        if (!Q_stricmp(var->name, "password"))
            continue;

        // [QL] Address: 0x004cb630 (Com_WriteClientConfig_f) - client config mode writes only
        // the CVAR_USERSAVE (0x80000) subset, and only cvars that differ from their reset value.
        if (clientOnly) {
            if (!(var->flags & CVAR_USERSAVE))
                continue;
            value = var->latchedString ? var->latchedString : var->string;
            if (!Q_stricmp(value, var->resetString))
                continue;
        }

        if (!((var->flags & CVAR_ARCHIVE) || (var->flags & CVAR_REPLICATE)))
            continue;

        // write the latched value, even if it hasn't taken effect yet
        value = var->latchedString ? var->latchedString : var->string;

        if (strlen(var->name) + strlen(value) + 10 > sizeof(buffer)) {
            Com_Printf(S_COLOR_YELLOW
                       "WARNING: value of variable "
                       "\"%s\" too long to write to file\n",
                       var->name);
            continue;
        }

        Com_sprintf(buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, value);

        // [QL] route to repconfig if CVAR_REPLICATE or CVAR_USER_CREATED
        dest = f_hw;
        if ((var->flags & CVAR_REPLICATE) || (var->flags & CVAR_USER_CREATED)) {
            dest = f_rep;
        }

        FS_Write(buffer, strlen(buffer), dest);
    }
}

/*
============
Cvar_List_f
============
*/
void Cvar_List_f(void) {
    cvar_t* var;
    int i;
    char* match;

    if (Cmd_Argc() > 1) {
        match = Cmd_Argv(1);
    } else {
        match = NULL;
    }

    i = 0;
    for (var = cvar_vars; var; var = var->next, i++) {
        if (!var->name || (match && !Com_Filter(match, var->name, qfalse)))
            continue;

        if (var->flags & CVAR_SERVERINFO) {
            Com_Printf("S");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_SYSTEMINFO) {
            Com_Printf("s");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_USERINFO) {
            Com_Printf("U");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_ROM) {
            Com_Printf("R");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_INIT) {
            Com_Printf("I");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_ARCHIVE) {
            Com_Printf("A");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_LATCH) {
            Com_Printf("L");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_CHEAT) {
            Com_Printf("C");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_USER_CREATED) {
            Com_Printf("?");
        } else {
            Com_Printf(" ");
        }

        Com_Printf(" %s \"%s\"\n", var->name, var->string);
    }

    Com_Printf("\n%i total cvars\n", i);
    Com_Printf("%i cvar indexes\n", cvar_numIndexes);
}

/*
============
Cvar_ListModified_f
============
*/
void Cvar_ListModified_f(void) {
    cvar_t* var;
    int totalModified;
    char* value;
    char* match;

    if (Cmd_Argc() > 1) {
        match = Cmd_Argv(1);
    } else {
        match = NULL;
    }

    totalModified = 0;
    for (var = cvar_vars; var; var = var->next) {
        if (!var->name || !var->modificationCount)
            continue;

        value = var->latchedString ? var->latchedString : var->string;
        if (!strcmp(value, var->resetString))
            continue;

        totalModified++;

        if (match && !Com_Filter(match, var->name, qfalse))
            continue;

        if (var->flags & CVAR_SERVERINFO) {
            Com_Printf("S");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_SYSTEMINFO) {
            Com_Printf("s");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_USERINFO) {
            Com_Printf("U");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_ROM) {
            Com_Printf("R");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_INIT) {
            Com_Printf("I");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_ARCHIVE) {
            Com_Printf("A");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_LATCH) {
            Com_Printf("L");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_CHEAT) {
            Com_Printf("C");
        } else {
            Com_Printf(" ");
        }
        if (var->flags & CVAR_USER_CREATED) {
            Com_Printf("?");
        } else {
            Com_Printf(" ");
        }

        Com_Printf(" %s \"%s\", default \"%s\"\n", var->name, value, var->resetString);
    }

    Com_Printf("\n%i total modified cvars\n", totalModified);
}

/*
============
Cvar_Unset

Unsets a cvar
============
*/

cvar_t* Cvar_Unset(cvar_t* cv) {
    cvar_t* next = cv->next;

    // note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
    cvar_modifiedFlags |= cv->flags;

    if (cv->name)
        Z_Free(cv->name);
    if (cv->string)
        Z_Free(cv->string);
    if (cv->latchedString)
        Z_Free(cv->latchedString);
    if (cv->factoryString)
        Z_Free(cv->factoryString);
    if (cv->resetString)
        Z_Free(cv->resetString);
    if (cv->description)
        Z_Free(cv->description);
    if (cv->minString)
        Z_Free(cv->minString);
    if (cv->maxString)
        Z_Free(cv->maxString);

    if (cv->prev)
        cv->prev->next = cv->next;
    else
        cvar_vars = cv->next;
    if (cv->next)
        cv->next->prev = cv->prev;

    if (cv->hashPrev)
        cv->hashPrev->hashNext = cv->hashNext;
    else
        hashTable[cv->hashIndex] = cv->hashNext;
    if (cv->hashNext)
        cv->hashNext->hashPrev = cv->hashPrev;

    Com_Memset(cv, '\0', sizeof(*cv));

    return next;
}

/*
============
Cvar_Unset_f

Unsets a userdefined cvar
============
*/

void Cvar_Unset_f(void) {
    cvar_t* cv;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <varname>\n", Cmd_Argv(0));
        return;
    }

    cv = Cvar_FindVar(Cmd_Argv(1));

    if (!cv)
        return;

    if (cv->flags & CVAR_USER_CREATED)
        Cvar_Unset(cv);
    else
        Com_Printf("Error: %s: Variable %s is not user created.\n", Cmd_Argv(0), cv->name);
}

/*
============
Cvar_Restart

Resets all cvars to their hardcoded values and removes userdefined variables
and variables added via the VMs if requested.
============
*/

void Cvar_Restart(qboolean unsetVM) {
    cvar_t* curvar;

    curvar = cvar_vars;

    while (curvar) {
        if ((curvar->flags & CVAR_USER_CREATED) ||
            (unsetVM && (curvar->flags & CVAR_VM_CREATED))) {
            // throw out any variables the user/vm created
            curvar = Cvar_Unset(curvar);
            continue;
        }

        if (!(curvar->flags & (CVAR_ROM | CVAR_INIT | CVAR_NORESTART))) {
            // Just reset the rest to their default values.
            Cvar_Set2(curvar->name, curvar->resetString, qfalse);
        }

        curvar = curvar->next;
    }
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f(void) {
    Cvar_Restart(qfalse);
}

/*
=====================
Cvar_InfoString
=====================
*/
char* Cvar_InfoString(int bit) {
    static char info[MAX_INFO_STRING];
    cvar_t* var;

    info[0] = 0;

    for (var = cvar_vars; var; var = var->next) {
        if (var->name && (var->flags & bit))
            Info_SetValueForKey(info, var->name, var->string);
    }

    return info;
}

/*
=====================
Cvar_InfoString_Big

  handles large info strings ( CS_SYSTEMINFO )
=====================
*/
char* Cvar_InfoString_Big(int bit) {
    static char info[BIG_INFO_STRING];
    cvar_t* var;

    info[0] = 0;

    for (var = cvar_vars; var; var = var->next) {
        if (var->name && (var->flags & bit))
            Info_SetValueForKey_Big(info, var->name, var->string);
    }
    return info;
}

/*
=====================
Cvar_InfoStringBuffer
=====================
*/
void Cvar_InfoStringBuffer(int bit, char* buff, int buffsize) {
    Q_strncpyz(buff, Cvar_InfoString(bit), buffsize);
}

/*
=====================
Cvar_CheckRange
=====================
*/
void Cvar_CheckRange(cvar_t* var, float min, float max, qboolean integral) {
    var->validate = qtrue;
    var->min = min;
    var->max = max;
    var->integral = integral;

    // Force an initial range check
    Cvar_Set(var->name, var->string);
}

/*
=====================
Cvar_SetDescription
=====================
*/
void Cvar_SetDescription(cvar_t* var, const char* var_description) {
    if (var_description && var_description[0] != '\0') {
        if (var->description != NULL) {
            Z_Free(var->description);
        }
        var->description = CopyString(var_description);
    }
}

/*
=====================
Cvar_Find

[CNQ3] Read-only lookup for the help and console code outside this file.
=====================
*/
const cvar_t* Cvar_Find(const char* var_name) {
    return Cvar_FindVar(var_name);
}

/*
=====================
Cvar_SetHelp / Cvar_GetHelp / Cvar_EnumHelp

[CNQ3] A cvar's help text is its description: the first line is the short
description shown in completion lists, the rest are the details printed by
/help and by the console help panel.
=====================
*/
void Cvar_SetHelp(const char* var_name, const char* help) {
    cvar_t* var = Cvar_FindVar(var_name);

    if (var) {
        Cvar_SetDescription(var, help);
    }
}

qboolean Cvar_GetHelp(const char* var_name, const char** help) {
    cvar_t* var = Cvar_FindVar(var_name);

    *help = var ? var->description : NULL;
    return var != NULL;
}

void Cvar_EnumHelp(helpSearchCallback_t callback, const char* pattern) {
    cvar_t* var;

    if (!pattern || !pattern[0]) {
        return;
    }

    for (var = cvar_vars; var; var = var->next) {
        if (var->name) {
            callback(var->name, var->description, pattern, qtrue);
        }
    }
}

/*
=====================
Cvar_PrintFirstHelpLine

[CNQ3] "name <range> (default: value)". Cvars in this tree have no types, so
the range is the one set with Cvar_CheckRange or given to Cvar_GetRange, if any;
a range given as two strings counts as integral when neither has a point.
=====================
*/
void Cvar_PrintFirstHelpLine(const cvar_t* var, printf_t print) {
    const qboolean integral = var->minString ? (!strchr(var->minString, '.') && !strchr(var->maxString, '.'))
                                             : var->integral;

    print(S_COLOR_CVAR "%s" S_COLOR_WHITE " <", var->name);

    if (!var->validate && !var->minString) {
        print("value");
    } else if (integral) {
        const int minV = (int)var->min;
        const int maxV = (int)var->max;
        const int diff = maxV - minV;

        if (diff >= 0 && diff <= 3) {
            int i;

            for (i = 0; i <= diff; i++) {
                print("%s" S_COLOR_VAL "%d", i ? S_COLOR_WHITE "|" : "", minV + i);
            }
        } else {
            print(S_COLOR_VAL "%d" S_COLOR_WHITE " to " S_COLOR_VAL "%d", minV, maxV);
        }
    } else {
        print(S_COLOR_VAL "%g" S_COLOR_WHITE " to " S_COLOR_VAL "%g", var->min, var->max);
    }

    print(S_COLOR_WHITE "> (default: \"" S_COLOR_VAL "%s" S_COLOR_WHITE "\")\n", var->resetString);
}

/*
=====================
Cvar_PrintFlags

[CNQ3] "Attributes: Archived, Cheat-protected, ..." with this tree's flags,
Quake Live's included.
=====================
*/
void Cvar_PrintFlags(const cvar_t* var, printf_t print) {
    static const struct {
        int flag;
        const char* name;
    } names[] = {
        {CVAR_ARCHIVE, "Archived"},
        {CVAR_USERINFO, "User Info"},
        {CVAR_SERVERINFO, "Server Info"},
        {CVAR_SYSTEMINFO, "System Info"},
        {CVAR_INIT, "Init"},
        {CVAR_LATCH, "Latched"},
        {CVAR_ROM, "Read-only"},
        {CVAR_USER_CREATED, "User-created"},
        {CVAR_TEMP, "Temporary"},
        {CVAR_CHEAT, "Cheat-protected"},
        {CVAR_NORESTART, "No Reset"},
        {CVAR_REPLICATE, "Replicated"},
        {CVAR_VM_CREATED, "Game module"},
        {CVAR_PROTECTED, "Protected"},
        {CVAR_USERSAVE, "User-saved"},
        {CVAR_GAMERULE, "Game rule"},
    };
    int i;
    int count = 0;
    int printed = 0;

    for (i = 0; i < (int)ARRAY_LEN(names); i++) {
        if (var->flags & names[i].flag) {
            count++;
        }
    }

    print(count != 1 ? "Attributes: " : "Attribute: ");

    for (i = 0; i < (int)ARRAY_LEN(names); i++) {
        if (!(var->flags & names[i].flag)) {
            continue;
        }
        if (printed) {
            print(", ");
        }
        print("%s", names[i].name);
        printed++;
    }

    if (!printed) {
        print("none");
    }

    print("\n");
}

/*
=====================
Cvar_RegisterVM

basically a slightly modified Cvar_Get for the interpreted modules, with Cvar_GetRange when a
range is given
=====================
*/
static void Cvar_RegisterVM(vmCvar_t* vmCvar, const char* varName, const char* defaultValue,
                            const char* minValue, const char* maxValue, int flags) {
    cvar_t* cv;

    // There is code in Cvar_Get to prevent CVAR_ROM cvars being changed by the
    // user. In other words CVAR_ARCHIVE and CVAR_ROM are mutually exclusive
    // flags. Unfortunately some historical game code (including single player
    // baseq3) sets both flags. We unset CVAR_ROM for such cvars.
    if ((flags & (CVAR_ARCHIVE | CVAR_ROM)) == (CVAR_ARCHIVE | CVAR_ROM)) {
        Com_DPrintf(S_COLOR_YELLOW
                    "WARNING: Unsetting CVAR_ROM from cvar '%s', "
                    "since it is also CVAR_ARCHIVE\n",
                    varName);
        flags &= ~CVAR_ROM;
    }

    // Don't allow VM to specific a different creator or other internal flags.
    if (flags & CVAR_USER_CREATED) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: VM tried to set CVAR_USER_CREATED on cvar '%s'\n", varName);
        flags &= ~CVAR_USER_CREATED;
    }
    if (flags & CVAR_PROTECTED) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: VM tried to set CVAR_PROTECTED on cvar '%s'\n", varName);
        flags &= ~CVAR_PROTECTED;
    }
    if (flags & CVAR_MODIFIED) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: VM tried to set CVAR_MODIFIED on cvar '%s'\n", varName);
        flags &= ~CVAR_MODIFIED;
    }
    if (flags & CVAR_NONEXISTENT) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: VM tried to set CVAR_NONEXISTENT on cvar '%s'\n", varName);
        flags &= ~CVAR_NONEXISTENT;
    }

    cv = Cvar_FindVar(varName);

    // Don't modify cvar if it's protected.
    if (cv && (cv->flags & CVAR_PROTECTED)) {
        Com_DPrintf(S_COLOR_YELLOW "WARNING: VM tried to register protected cvar '%s' with value '%s'%s\n",
                    varName, defaultValue, (flags & ~cv->flags) != 0 ? " and new flags" : "");
    } else if (minValue) {
        cv = Cvar_GetRange(varName, defaultValue, minValue, maxValue, flags | CVAR_VM_CREATED);
    } else {
        cv = Cvar_Get(varName, defaultValue, flags | CVAR_VM_CREATED);
    }

    if (!vmCvar)
        return;

    vmCvar->handle = cv - cvar_indexes;
    vmCvar->modificationCount = -1;
    Cvar_Update(vmCvar);
}

/*
=====================
Cvar_Register
=====================
*/
void Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags) {
    Cvar_RegisterVM(vmCvar, varName, defaultValue, NULL, NULL, flags);
}

/*
=====================
Cvar_RegisterRange

[QL] quakelive_steam.exe 0x4ce460, cgame import +0x14: Cvar_Register with a range given as two
strings
=====================
*/
void Cvar_RegisterRange(vmCvar_t* vmCvar, const char* varName, const char* defaultValue,
                        const char* minValue, const char* maxValue, int flags) {
    Cvar_RegisterVM(vmCvar, varName, defaultValue, minValue, maxValue, flags);
}

/*
=====================
Cvar_Update

updates an interpreted modules' version of a cvar
=====================
*/
void Cvar_Update(vmCvar_t* vmCvar) {
    cvar_t* cv = NULL;
    assert(vmCvar);

    if ((unsigned)vmCvar->handle >= cvar_numIndexes) {
        Com_Error(ERR_DROP, "Cvar_Update: handle out of range");
    }

    cv = cvar_indexes + vmCvar->handle;

    if (cv->modificationCount == vmCvar->modificationCount) {
        return;
    }
    if (!cv->string) {
        return;  // variable might have been cleared by a cvar_restart
    }
    vmCvar->modificationCount = cv->modificationCount;
    if (strlen(cv->string) + 1 > MAX_CVAR_VALUE_STRING)
        Com_Error(ERR_DROP, "Cvar_Update: src %s length %u exceeds MAX_CVAR_VALUE_STRING",
                  cv->string,
                  (unsigned int)strlen(cv->string));
    Q_strncpyz(vmCvar->string, cv->string, MAX_CVAR_VALUE_STRING);

    vmCvar->value = cv->value;
    vmCvar->integer = cv->integer;
}

/*
==================
Cvar_CompleteCvarName
==================
*/
void Cvar_CompleteCvarName(char* args, int argNum) {
    if (argNum == 2) {
        // Skip "<cmd> "
        char* p = Com_SkipTokens(args, 1, " ");

        if (p > args)
            Field_CompleteCommand(p, qfalse, qtrue);
    }
}

/*
============
[QL] Cvar_Add_f - cvarAdd <cvar> <value>
============
*/
static void Cvar_Add_f(void) {
    float val;

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: cvarAdd <variable> <value>\n");
        return;
    }

    val = Cvar_VariableValue(Cmd_Argv(1));
    Cvar_SetValue(Cmd_Argv(1), val + atof(Cmd_Argv(2)));
}

/*
============
[QL] Cvar_Mult_f - cvarMult <cvar> <value>
============
*/
static void Cvar_Mult_f(void) {
    float val;

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: cvarMult <variable> <value>\n");
        return;
    }

    val = Cvar_VariableValue(Cmd_Argv(1));
    Cvar_SetValue(Cmd_Argv(1), val * atof(Cmd_Argv(2)));
}

/*
============
[QL] Cvar_Clear_f - clearcvar <cvar>
============
*/
static void Cvar_Clear_f(void) {
    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: clearcvar <variable>\n");
        return;
    }

    Cvar_Set(Cmd_Argv(1), "");
}

/*
============
Cvar_Op_f

[CNQ3] cvar_add / cvar_mul <cvar> <number>: like cvarAdd and cvarMult, but the
result stays within the cvar's range and integer cvars stay integers.
============
*/
static void Cvar_Op_f(void) {
    const qboolean multiply = !Q_stricmp(Cmd_Argv(0), "cvar_mul") ? qtrue : qfalse;
    cvar_t* var;
    float operand;
    float result;

    if (Cmd_Argc() != 3) {
        Com_Printf("usage: %s <cvar> <number>\n", Cmd_Argv(0));
        return;
    }

    var = Cvar_FindVar(Cmd_Argv(1));
    if (!var) {
        Com_Printf("%s: variable '%s' not found\n", Cmd_Argv(0), Cmd_Argv(1));
        return;
    }

    if (sscanf(Cmd_Argv(2), "%f", &operand) != 1 || operand != operand ||
        operand > 1.0e30f || operand < -1.0e30f) {
        Com_Printf("%s: '%s' isn't a finite number\n", Cmd_Argv(0), Cmd_Argv(2));
        return;
    }

    if (var->validate && var->integral && operand != (float)(int)operand) {
        Com_Printf("%s: " S_COLOR_CVAR "%s" S_COLOR_WHITE " only takes whole numbers\n", Cmd_Argv(0), var->name);
        return;
    }

    result = multiply ? var->value * operand : var->value + operand;
    if (var->validate) {
        if (result < var->min) {
            result = var->min;
        } else if (result > var->max) {
            result = var->max;
        }
    }

    if (var->validate && var->integral) {
        Cvar_Set2(var->name, va("%d", (int)result), qfalse);
    } else {
        Cvar_Set2(var->name, va("%g", result), qfalse);
    }
}

/*
============
Cvar_SetEmpty_f

[CNQ3] setempty <cvar>
============
*/
static void Cvar_SetEmpty_f(void) {
    if (Cmd_Argc() != 2) {
        Com_Printf("usage: %s <cvar>\n", Cmd_Argv(0));
        return;
    }

    if (Cvar_FindVar(Cmd_Argv(1))) {
        Cvar_Set2(Cmd_Argv(1), "", qfalse);
    }
}

/*
============
Cvar_Trim_f

[CNQ3] cvar_trim [-f]: removes every user-created cvar. Unless forced it only
runs with a local server, when all the modules have registered their cvars.
============
*/
static void Cvar_Trim_f(void) {
    cvar_t* var;
    int count = 0;

    if (!(Cmd_Argc() == 2 && !Q_stricmp(Cmd_Argv(1), "-f")) &&
        !(com_cl_running && com_cl_running->integer && com_sv_running && com_sv_running->integer)) {
        Com_Printf("You're not running a listen server, so not all subsystems/VMs are loaded.\n");
        Com_Printf("This means you'd remove cvars that are probably best kept around.\n");
        Com_Printf("If you don't care, you can force the call by running '/%s -f'.\n", Cmd_Argv(0));
        return;
    }

    var = cvar_vars;
    while (var) {
        if (var->flags & CVAR_USER_CREATED) {
            var = Cvar_Unset(var);
            count++;
            continue;
        }
        var = var->next;
    }

    Com_Printf("%d user-created cvar%s removed\n", count, count == 1 ? "" : "s");
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init(void) {
    Com_Memset(cvar_indexes, '\0', sizeof(cvar_indexes));
    Com_Memset(hashTable, '\0', sizeof(hashTable));

    cvar_cheats = Cvar_Get("sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO);

    Cmd_AddCommand("toggle", Cvar_Toggle_f);
    Cmd_SetCommandCompletionFunc("toggle", Cvar_CompleteCvarName);
    Cmd_AddCommand("set", Cvar_Set_f);
    Cmd_SetCommandCompletionFunc("set", Cvar_CompleteCvarName);
    Cmd_AddCommand("seta", Cvar_Set_f);
    Cmd_SetCommandCompletionFunc("seta", Cvar_CompleteCvarName);
    Cmd_AddCommand("reset", Cvar_Reset_f);
    Cmd_SetCommandCompletionFunc("reset", Cvar_CompleteCvarName);

    Cmd_AddCommand("cvarlist", Cvar_List_f);
    Cmd_AddCommand("listcvars", Cvar_List_f);
    Cmd_AddCommand("cvar_restart", Cvar_Restart_f);

    // [QL] Additional cvar commands
    Cmd_AddCommand("cvarAdd", Cvar_Add_f);
    Cmd_AddCommand("cvarMult", Cvar_Mult_f);
    Cmd_AddCommand("clearcvar", Cvar_Clear_f);

    // [CNQ3] cvar commands
    Cmd_AddCommand("unset", Cvar_Unset_f);
    Cmd_SetCommandCompletionFunc("unset", Cvar_CompleteCvarName);
    Cmd_AddCommand("setempty", Cvar_SetEmpty_f);
    Cmd_SetCommandCompletionFunc("setempty", Cvar_CompleteCvarName);
    Cmd_AddCommand("cvar_add", Cvar_Op_f);
    Cmd_SetCommandCompletionFunc("cvar_add", Cvar_CompleteCvarName);
    Cmd_AddCommand("cvar_mul", Cvar_Op_f);
    Cmd_SetCommandCompletionFunc("cvar_mul", Cvar_CompleteCvarName);
    Cmd_AddCommand("cvar_trim", Cvar_Trim_f);

    Cmd_SetHelp("toggle", "toggles a cvar between 0 and 1, or cycles through a list of values\n"
                          "Usage: " S_COLOR_CMD "toggle " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "[value1 value2 ...]");
    Cmd_SetHelp("set", "sets a cvar, creating it when it doesn't exist\n"
                       "Usage: " S_COLOR_CMD "set " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "<value>");
    Cmd_SetHelp("seta", "sets a cvar and saves it in the config\n"
                        "Usage: " S_COLOR_CMD "seta " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "<value>");
    Cmd_SetHelp("reset", "resets a cvar to its default value\n"
                         "Usage: " S_COLOR_CMD "reset " S_COLOR_CVAR "<cvar>");
    Cmd_SetHelp("cvarlist", "lists and filters all cvars\n"
                            "The optional argument filters the list, '*' matches 0 or more characters.\n"
                            "Example: " S_COLOR_CMD "cvarlist " S_COLOR_VAL "*fov" S_COLOR_HELP " lists the cvars containing 'fov'");
    Cmd_SetHelp("listcvars", "lists and filters all cvars\n"
                             "Same as " S_COLOR_CMD "cvarlist" S_COLOR_HELP ".");
    Cmd_SetHelp("cvar_restart", "resets all cvars to their default values\n"
                                "User-created cvars are removed.");
    Cmd_SetHelp("cvarAdd", "adds a number to a cvar\n"
                           "Usage: " S_COLOR_CMD "cvarAdd " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "<number>");
    Cmd_SetHelp("cvarMult", "multiplies a cvar by a number\n"
                            "Usage: " S_COLOR_CMD "cvarMult " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "<number>");
    Cmd_SetHelp("clearcvar", "sets a cvar to an empty string\n"
                             "Usage: " S_COLOR_CMD "clearcvar " S_COLOR_CVAR "<cvar>");
    Cmd_SetHelp("unset", "removes a user-created cvar\n"
                         "Usage: " S_COLOR_CMD "unset " S_COLOR_CVAR "<cvar>");
    Cmd_SetHelp("setempty", "sets a cvar to an empty string\n"
                            "Usage: " S_COLOR_CMD "setempty " S_COLOR_CVAR "<cvar>");
    Cmd_SetHelp("cvar_add", "adds a number to a cvar, staying within its range\n"
                            "Usage: " S_COLOR_CMD "cvar_add " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "<number>");
    Cmd_SetHelp("cvar_mul", "multiplies a cvar by a number, staying within its range\n"
                            "Usage: " S_COLOR_CMD "cvar_mul " S_COLOR_CVAR "<cvar> " S_COLOR_VAL "<number>");
    Cmd_SetHelp("cvar_trim", "removes all user-created cvars\n"
                             "Usage: " S_COLOR_CMD "cvar_trim " S_COLOR_VAL "[-f]\n" S_COLOR_HELP
                             "Without -f it only runs with a local server, when every module has registered its cvars.");
}
