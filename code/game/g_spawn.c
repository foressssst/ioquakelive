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
#include "g_local.h"

qboolean G_SpawnString(const char* key, const char* defaultString, char** out) {
    int i;

    if (!level.spawning) {
        *out = (char*)defaultString;
    }

    for (i = 0; i < level.numSpawnVars; i++) {
        if (!Q_stricmp(key, level.spawnVars[i][0])) {
            *out = level.spawnVars[i][1];
            return qtrue;
        }
    }

    *out = (char*)defaultString;
    return qfalse;
}

qboolean G_SpawnFloat(const char* key, const char* defaultString, float* out) {
    char* s;
    qboolean present;

    present = G_SpawnString(key, defaultString, &s);
    *out = atof(s);
    return present;
}

qboolean G_SpawnInt(const char* key, const char* defaultString, int* out) {
    char* s;
    qboolean present;

    present = G_SpawnString(key, defaultString, &s);
    *out = atoi(s);
    return present;
}

qboolean G_SpawnVector(const char* key, const char* defaultString, float* out) {
    char* s;
    qboolean present;

    present = G_SpawnString(key, defaultString, &s);
    sscanf(s, "%f %f %f", &out[0], &out[1], &out[2]);
    return present;
}

typedef enum {
    F_INT,
    F_FLOAT,
    F_STRING,
    F_LSTRING,
    F_VECTOR,
    F_ANGLEHACK,
    F_ENTITY,
    F_ITEM,
    F_CLIENT,
    F_IGNORE
} fieldtype_t;

typedef struct
{
    char* name;
    size_t ofs;
    fieldtype_t type;
} field_t;

field_t fields[] = {
    {"classname", FOFS(classname), F_STRING},
    {"origin", FOFS(s.origin), F_VECTOR},
    {"model", FOFS(model), F_STRING},
    {"model2", FOFS(model2), F_STRING},
    {"spawnflags", FOFS(spawnflags), F_INT},
    {"speed", FOFS(speed), F_FLOAT},
    {"target", FOFS(target), F_STRING},
    {"targetname", FOFS(targetname), F_STRING},
    {"message", FOFS(message), F_STRING},
    {"cvar", FOFS(cvar), F_STRING},
    {"tourPointTarget", FOFS(tourPointTarget), F_STRING},
    {"tourPointTargetName", FOFS(tourPointTargetName), F_STRING},
    {"noise", FOFS(noise), F_STRING},
    {"team", FOFS(team), F_STRING},
    {"wait", FOFS(wait), F_FLOAT},
    {"random", FOFS(random), F_FLOAT},
    {"count", FOFS(count), F_INT},
    {"health", FOFS(health), F_INT},
    {"light", 0, F_IGNORE},
    {"dmg", FOFS(damage), F_INT},
    {"angles", FOFS(s.angles), F_VECTOR},
    {"angle", FOFS(s.angles), F_ANGLEHACK},
    {"targetShaderName", FOFS(targetShaderName), F_STRING},
    {"targetShaderNewName", FOFS(targetShaderNewName), F_STRING},

    {NULL}};

typedef struct {
    char* name;
    void (*spawn)(gentity_t* ent);
} spawn_t;

void SP_info_player_start(gentity_t* ent);
void SP_info_player_deathmatch(gentity_t* ent);
void SP_info_player_intermission(gentity_t* ent);

void SP_func_plat(gentity_t* ent);
void SP_func_static(gentity_t* ent);
void SP_func_rotating(gentity_t* ent);
void SP_func_bobbing(gentity_t* ent);
void SP_func_pendulum(gentity_t* ent);
void SP_func_button(gentity_t* ent);
void SP_func_door(gentity_t* ent);
void SP_func_train(gentity_t* ent);
void SP_func_timer(gentity_t* self);

void SP_trigger_always(gentity_t* ent);
void SP_trigger_multiple(gentity_t* ent);
void SP_trigger_push(gentity_t* ent);
void SP_trigger_teleport(gentity_t* ent);
void SP_trigger_hurt(gentity_t* ent);
void SP_trigger_capturezone(gentity_t* ent);

void SP_target_remove_powerups(gentity_t* ent);
void SP_target_give(gentity_t* ent);
void SP_target_remove_keys(gentity_t* ent);
void SP_target_delay(gentity_t* ent);
void SP_target_speaker(gentity_t* ent);
void SP_target_print(gentity_t* ent);
void SP_target_laser(gentity_t* self);
void SP_target_score(gentity_t* ent);
void SP_target_teleporter(gentity_t* ent);
void SP_target_relay(gentity_t* ent);
void SP_target_cvar(gentity_t* ent);
void SP_target_achievement(gentity_t* ent);
void SP_target_kill(gentity_t* ent);
void SP_target_position(gentity_t* ent);
void SP_target_location(gentity_t* ent);
void SP_target_push(gentity_t* ent);

void SP_light(gentity_t* self);
void SP_info_null(gentity_t* self);
void SP_info_notnull(gentity_t* self);
void SP_info_camp(gentity_t* self);
void SP_path_corner(gentity_t* self);

void SP_misc_teleporter_dest(gentity_t* self);
void SP_misc_model(gentity_t* ent);
void SP_misc_portal_camera(gentity_t* ent);
void SP_misc_portal_surface(gentity_t* ent);

void SP_shooter_rocket(gentity_t* ent);
void SP_shooter_plasma(gentity_t* ent);
void SP_shooter_grenade(gentity_t* ent);

void SP_team_CTF_redplayer(gentity_t* ent);
void SP_team_CTF_blueplayer(gentity_t* ent);

void SP_team_CTF_redspawn(gentity_t* ent);
void SP_team_CTF_bluespawn(gentity_t* ent);

void SP_team_blueobelisk(gentity_t* ent);
void SP_team_redobelisk(gentity_t* ent);
void SP_team_neutralobelisk(gentity_t* ent);

void SP_item_botroam(gentity_t* ent) {}

void SP_advertisement(gentity_t *ent) {
    char *s;

    if (trap_Cvar_VariableValue("com_build")) {
        G_SpawnString("cellId", "0", &s);
        if (!atoi(s)) {
            G_Error("advertisement entity with no cellId at %s", vtos(ent->s.origin));
        }
    }
    G_FreeEntity(ent);
}

void SP_team_dom_point(gentity_t *ent);
void SP_race_point(gentity_t *ent);

spawn_t spawns[] = {
    {"info_player_start", SP_info_player_start},
    {"info_player_deathmatch", SP_info_player_deathmatch},
    {"info_player_intermission", SP_info_player_intermission},
    {"info_null", SP_info_null},
    {"info_notnull", SP_info_notnull},
    {"info_camp", SP_info_camp},

    {"func_plat", SP_func_plat},
    {"func_button", SP_func_button},
    {"func_door", SP_func_door},
    {"func_static", SP_func_static},
    {"func_rotating", SP_func_rotating},
    {"func_bobbing", SP_func_bobbing},
    {"func_pendulum", SP_func_pendulum},
    {"func_train", SP_func_train},
    {"func_group", SP_info_null},
    {"func_timer", SP_func_timer},

    {"trigger_always", SP_trigger_always},
    {"trigger_multiple", SP_trigger_multiple},
    {"trigger_push", SP_trigger_push},
    {"trigger_teleport", SP_trigger_teleport},
    {"trigger_hurt", SP_trigger_hurt},
    {"trigger_capturezone", SP_trigger_capturezone},

    {"target_give", SP_target_give},
    {"target_remove_keys", SP_target_remove_keys},
    {"target_remove_powerups", SP_target_remove_powerups},
    {"target_delay", SP_target_delay},
    {"target_speaker", SP_target_speaker},
    {"target_print", SP_target_print},
    {"target_laser", SP_target_laser},
    {"target_score", SP_target_score},
    {"target_teleporter", SP_target_teleporter},
    {"target_relay", SP_target_relay},
    {"target_cvar", SP_target_cvar},
    {"target_achievement", SP_target_achievement},
    {"target_kill", SP_target_kill},
    {"target_position", SP_target_position},
    {"target_location", SP_target_location},
    {"target_push", SP_target_push},

    {"light", SP_light},
    {"path_corner", SP_path_corner},

    {"misc_teleporter_dest", SP_misc_teleporter_dest},
    {"misc_model", SP_misc_model},
    {"misc_portal_surface", SP_misc_portal_surface},
    {"misc_portal_camera", SP_misc_portal_camera},

    {"shooter_rocket", SP_shooter_rocket},
    {"shooter_grenade", SP_shooter_grenade},
    {"shooter_plasma", SP_shooter_plasma},

    {"team_CTF_redplayer", SP_team_CTF_redplayer},
    {"team_CTF_blueplayer", SP_team_CTF_blueplayer},

    {"team_CTF_redspawn", SP_team_CTF_redspawn},
    {"team_CTF_bluespawn", SP_team_CTF_bluespawn},

    {"team_redobelisk", SP_team_redobelisk},
    {"team_blueobelisk", SP_team_blueobelisk},
    {"team_neutralobelisk", SP_team_neutralobelisk},

    {"item_botroam", SP_item_botroam},

    {"advertisement", SP_advertisement},
    {"team_dom_point", SP_team_dom_point},
    {"race_point", SP_race_point},

    {"info_tour_point", SP_info_tour_point},

    {NULL, 0}};

qboolean G_CallSpawn(gentity_t* ent) {
    spawn_t* s;
    gitem_t* item;

    if (!ent->classname) {
        G_Printf("G_CallSpawn: NULL classname\n");
        return qfalse;
    }

    for (item = bg_itemlist + 1; item->classname; item++) {
        if (!strcmp(item->classname, ent->classname)) {
            G_SpawnItem(ent, item, qfalse);
            return qtrue;
        }
    }

    for (s = spawns; s->name; s++) {
        if (!strcmp(s->name, ent->classname)) {
            s->spawn(ent);
            return qtrue;
        }
    }
    G_Printf("%s doesn't have a spawn function\n", ent->classname);
    return qfalse;
}

char* G_NewString(const char* string) {
    char *newb, *new_p;
    int i, l;

    l = strlen(string) + 1;

    newb = G_Alloc(l);

    new_p = newb;

    for (i = 0; i < l; i++) {
        if (string[i] == '\\' && i < l - 1) {
            i++;
            if (string[i] == 'n') {
                *new_p++ = '\n';
            } else {
                *new_p++ = '\\';
            }
        } else {
            *new_p++ = string[i];
        }
    }

    return newb;
}

void G_ParseField(const char* key, const char* value, gentity_t* ent) {
    field_t* f;
    byte* b;
    float v;
    vec3_t vec;

    for (f = fields; f->name; f++) {
        if (!Q_stricmp(f->name, key)) {
            b = (byte*)ent;

            switch (f->type) {
                case F_STRING:
                    *(char**)(b + f->ofs) = G_NewString(value);
                    break;
                case F_VECTOR:
                    sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
                    ((float*)(b + f->ofs))[0] = vec[0];
                    ((float*)(b + f->ofs))[1] = vec[1];
                    ((float*)(b + f->ofs))[2] = vec[2];
                    break;
                case F_INT:
                    *(int*)(b + f->ofs) = atoi(value);
                    break;
                case F_FLOAT:
                    *(float*)(b + f->ofs) = atof(value);
                    break;
                case F_ANGLEHACK:
                    v = atof(value);
                    ((float*)(b + f->ofs))[0] = 0;
                    ((float*)(b + f->ofs))[1] = v;
                    ((float*)(b + f->ofs))[2] = 0;
                    break;
                case F_IGNORE:
                    break;
                default:
                    break;
            }
            return;
        }
    }
}

static qboolean G_ItemValidForGametype(const char* classname) {
    const gitem_t* item;
    int gt;

    if (!classname || !classname[0]) {
        return qfalse;
    }

    gt = g_gametype.integer;

    for (item = bg_itemlist + 1; item->classname; item++) {
        if (!strcmp(item->classname, classname)) {
            return (item->validGametypes & (1 << (gt & 0x1f))) != 0;
        }
    }

    if (gt != GT_AD) {
        return qfalse;
    }
    if (!strcmp(classname, "team_redobelisk") || !strcmp(classname, "team_blueobelisk")) {
        return qtrue;
    }
    return qfalse;
}

qboolean G_GametypeInList(const char* list) {
    const char* name;
    const char* p;
    int len;

    if (g_gametype.integer < GT_FFA || g_gametype.integer >= GT_MAX_GAME_TYPE) {
        return qfalse;
    }
    name = gametypeShortNames[g_gametype.integer];
    len = strlen(name);

    for (p = list; *p; ) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (!Q_stricmpn(p, name, len) && (p[len] == '\0' || p[len] == ' ' || p[len] == '\t')) {
            return qtrue;
        }
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
    }
    return qfalse;
}

void G_SpawnGEntityFromSpawnVars(void) {
    int i;
    gentity_t* ent;
    char* value;
    qboolean itemValid;

    ent = G_Spawn();

    for (i = 0; i < level.numSpawnVars; i++) {
        G_ParseField(level.spawnVars[i][0], level.spawnVars[i][1], ent);
    }

    itemValid = G_ItemValidForGametype(ent->classname);

    if (g_gametype.integer >= GT_TEAM) {
        G_SpawnInt("notteam", "0", &i);
        if (i && !itemValid) {
            G_FreeEntity(ent);
            return;
        }
    } else {
        G_SpawnInt("notfree", "0", &i);
        if (i && !itemValid) {
            G_FreeEntity(ent);
            return;
        }
    }

    if (G_SpawnString("gametype", "", &value)) {
        if (!G_GametypeInList(value) && !itemValid) {
            G_FreeEntity(ent);
            return;
        }
    }

    if (G_SpawnString("not_gametype", "", &value)) {
        if (G_GametypeInList(value) && !itemValid) {
            G_FreeEntity(ent);
            return;
        }
    }

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);

    if (!G_CallSpawn(ent)) {
        G_FreeEntity(ent);
    }
}

char* G_AddSpawnVarToken(const char* string) {
    int l;
    char* dest;

    l = strlen(string);
    if (level.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS) {
        G_Error("G_AddSpawnVarToken: MAX_SPAWN_CHARS");
    }

    dest = level.spawnVarChars + level.numSpawnVarChars;
    memcpy(dest, string, l + 1);

    level.numSpawnVarChars += l + 1;

    return dest;
}

qboolean G_ParseSpawnVars(void) {
    char keyname[MAX_TOKEN_CHARS];
    char com_token[MAX_TOKEN_CHARS];

    level.numSpawnVars = 0;
    level.numSpawnVarChars = 0;

    if (!trap_GetEntityToken(com_token, sizeof(com_token))) {
        return qfalse;
    }
    if (com_token[0] != '{') {
        G_Error("G_ParseSpawnVars: found %s when expecting {", com_token);
    }

    while (1) {
        if (!trap_GetEntityToken(keyname, sizeof(keyname))) {
            G_Error("G_ParseSpawnVars: EOF without closing brace");
        }

        if (keyname[0] == '}') {
            break;
        }

        if (!trap_GetEntityToken(com_token, sizeof(com_token))) {
            G_Error("G_ParseSpawnVars: EOF without closing brace");
        }

        if (com_token[0] == '}') {
            G_Error("G_ParseSpawnVars: closing brace without data");
        }
        if (level.numSpawnVars == MAX_SPAWN_VARS) {
            G_Error("G_ParseSpawnVars: MAX_SPAWN_VARS");
        }
        level.spawnVars[level.numSpawnVars][0] = G_AddSpawnVarToken(keyname);
        level.spawnVars[level.numSpawnVars][1] = G_AddSpawnVarToken(com_token);
        level.numSpawnVars++;
    }

    return qtrue;
}

void SP_worldspawn(void) {
    char* s;

    G_SpawnString("classname", "", &s);
    if (Q_stricmp(s, "worldspawn")) {
        G_Error("SP_worldspawn: The first entity isn't 'worldspawn'");
    }

    trap_SetConfigstring(CS_GAME_VERSION, GAME_VERSION);

    trap_SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));

    G_SpawnString("music", "", &s);
    trap_SetConfigstring(CS_MUSIC, s);

    G_SpawnString("message", "", &s);
    trap_SetConfigstring(CS_MESSAGE, s);

    G_SpawnString("author", "", &s);
    trap_SetConfigstring(CS_AUTHOR, s);

    G_SpawnString("author2", "", &s);
    trap_SetConfigstring(CS_AUTHOR2, s);

    if (g_loadout.integer) {
        int mask;
        G_SpawnString("disable_loadout", "", &s);
        mask = BG_ParseWeaponStringtoFlag(s);
        trap_SetConfigstring(CS_DISABLE_LOADOUT, va("%i", mask));
        trap_Cvar_Set("g_disableLoadout", va("%i", mask));
    }

    trap_SetConfigstring(CS_MOTD, g_motd.string);

    if (G_SpawnString("gravity", "800", &s)) {
        trap_Cvar_Set("g_gravity", s);
    }

    G_SpawnString("enableDust", "0", &s);
    trap_Cvar_Set("g_enableDust", s);

    G_SpawnString("enableBreath", "0", &s);
    trap_SetConfigstring(CS_ENABLEBREATH, s);

    G_SpawnString("trainingMap", "0", &s);
    if (atoi(s) == 1) {
        trap_Cvar_Set("g_training", "2");
    }
    level.mapIsTrainingMap = strcmp(s, "0") != 0;

    if (G_SpawnString("atmosphere", "", &s)) {
        trap_SetConfigstring(CS_ATMOSEFFECT, s);
    } else {
        trap_SetConfigstring(CS_ATMOSEFFECT, "");
    }

    g_entities[ENTITYNUM_WORLD].s.number = ENTITYNUM_WORLD;
    g_entities[ENTITYNUM_WORLD].r.ownerNum = ENTITYNUM_NONE;
    g_entities[ENTITYNUM_WORLD].classname = "worldspawn";

    g_entities[ENTITYNUM_NONE].s.number = ENTITYNUM_NONE;
    g_entities[ENTITYNUM_NONE].r.ownerNum = ENTITYNUM_NONE;
    g_entities[ENTITYNUM_NONE].classname = "nothing";

    SetWarmupState(-1);
    if (g_restarted.integer) {
        trap_Cvar_Set("g_restarted", "0");
        SetWarmupState(0);
    } else if (g_doWarmup.integer) {
        SetWarmupState(-1);
        G_LogPrintf("Warmup:\n");
    }

}

void G_SpawnEntitiesFromString(void) {
    level.spawning = qtrue;
    level.numSpawnVars = 0;

    if (!G_ParseSpawnVars()) {
        G_Error("SpawnEntities: no entities");
    }
    SP_worldspawn();

    while (G_ParseSpawnVars()) {
        G_SpawnGEntityFromSpawnVars();
    }

    level.spawning = qfalse;
}
