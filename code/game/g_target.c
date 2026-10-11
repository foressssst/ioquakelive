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

void Use_Target_Give(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    gentity_t* t;
    trace_t trace;

    if (!activator->client) {
        return;
    }

    if (!ent->target) {
        return;
    }

    memset(&trace, 0, sizeof(trace));
    t = NULL;
    while ((t = G_Find(t, FOFS(targetname), ent->target)) != NULL) {
        if (!t->item) {
            continue;
        }
        Touch_Item(t, activator, &trace);

        t->nextthink = 0;
        trap_UnlinkEntity(t);
    }
}

void SP_target_give(gentity_t* ent) {
    ent->use = Use_Target_Give;
}

static void Use_Target_Remove_Keys(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    if (!activator->client || !activator->client->ps.stats[STAT_KEY]) {
        return;
    }
    if (activator->client->ps.stats[STAT_KEY] & KEY_SILVER) {
        G_RespawnKey(KEY_SILVER);
    }
    if (activator->client->ps.stats[STAT_KEY] & KEY_GOLD) {
        G_RespawnKey(KEY_GOLD);
    }
    if (activator->client->ps.stats[STAT_KEY] & KEY_MASTER) {
        G_RespawnKey(KEY_MASTER);
    }
    activator->client->ps.stats[STAT_KEY] = 0;
}

void SP_target_remove_keys(gentity_t* ent) {
    ent->use = Use_Target_Remove_Keys;
}

void Use_target_remove_powerups(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    if (!activator->client) {
        return;
    }

    if (activator->client->ps.powerups[PW_REDFLAG]) {
        Team_ReturnFlag(TEAM_RED);
    } else if (activator->client->ps.powerups[PW_BLUEFLAG]) {
        Team_ReturnFlag(TEAM_BLUE);
    } else if (activator->client->ps.powerups[PW_NEUTRALFLAG]) {
        Team_ReturnFlag(TEAM_FREE);
    }

    memset(activator->client->ps.powerups, 0, sizeof(activator->client->ps.powerups));
}

void SP_target_remove_powerups(gentity_t* ent) {
    ent->use = Use_target_remove_powerups;
}

void Think_Target_Delay(gentity_t* ent) {
    G_UseTargets(ent, ent->activator);
}

void Use_Target_Delay(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    ent->nextthink = level.time + (ent->wait + ent->random * crandom()) * 1000;
    ent->think = Think_Target_Delay;
    ent->activator = activator;
}

void SP_target_delay(gentity_t* ent) {
    if (!G_SpawnFloat("delay", "0", &ent->wait)) {
        G_SpawnFloat("wait", "1", &ent->wait);
    }

    if (!ent->wait) {
        ent->wait = 1;
    }
    ent->use = Use_Target_Delay;
}

void Use_Target_Score(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    AddScore(activator, ent->r.currentOrigin, ent->count);
}

void SP_target_score(gentity_t* ent) {
    if (!ent->count) {
        ent->count = 1;
    }
    ent->use = Use_Target_Score;
}

void Use_Target_Print(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    if (activator->client && (ent->spawnflags & 4)) {
        trap_SendServerCommand(activator - g_entities, va("cp \"%s\"", ent->message));
        return;
    }

    if (ent->spawnflags & 3) {
        if (ent->spawnflags & 1) {
            G_TeamCommand(TEAM_RED, va("cp \"%s\"", ent->message));
        }
        if (ent->spawnflags & 2) {
            G_TeamCommand(TEAM_BLUE, va("cp \"%s\"", ent->message));
        }
        return;
    }

    trap_SendServerCommand(-1, va("cp \"%s\"", ent->message));
}

void SP_target_print(gentity_t* ent) {
    ent->use = Use_Target_Print;
}

void Use_Target_Speaker(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    if (activator->client && activator->client->hook) {
        return;
    }

    if (ent->spawnflags & 3) {
        if (ent->s.loopSound)
            ent->s.loopSound = 0;
        else
            ent->s.loopSound = ent->noise_index;
    } else {
        if (ent->spawnflags & 8) {
            G_AddEvent(activator, EV_GENERAL_SOUND, ent->noise_index);
        } else if (ent->spawnflags & 4) {
            G_AddEvent(ent, EV_GLOBAL_SOUND, ent->noise_index);
        } else {
            G_AddEvent(ent, EV_GENERAL_SOUND, ent->noise_index);
        }
    }
}

void SP_target_speaker(gentity_t* ent) {
    char buffer[MAX_QPATH];
    char* s;

    G_SpawnFloat("wait", "0", &ent->wait);
    G_SpawnFloat("random", "0", &ent->random);

    if (!G_SpawnString("noise", "NOSOUND", &s)) {
        G_Error("target_speaker without a noise key at %s", vtos(ent->s.origin));
    }

    if (s[0] == '*') {
        ent->spawnflags |= 8;
    }

    if (!strstr(s, ".wav")) {
        Com_sprintf(buffer, sizeof(buffer), "%s.wav", s);
    } else {
        Q_strncpyz(buffer, s, sizeof(buffer));
    }
    ent->noise_index = G_SoundIndex(buffer);

    ent->s.eType = ET_SPEAKER;
    ent->s.eventParm = ent->noise_index;
    ent->s.frame = ent->wait * 10;
    ent->s.clientNum = ent->random * 10;

    if (ent->spawnflags & 1) {
        ent->s.loopSound = ent->noise_index;
    }

    ent->use = Use_Target_Speaker;

    if (ent->spawnflags & 4) {
        ent->r.svFlags |= SVF_BROADCAST;
    }

    VectorCopy(ent->s.origin, ent->s.pos.trBase);

    trap_LinkEntity(ent);
}

void target_laser_think(gentity_t* self) {
    vec3_t end;
    trace_t tr;
    vec3_t point;

    if (self->enemy) {
        VectorMA(self->enemy->s.origin, 0.5, self->enemy->r.mins, point);
        VectorMA(point, 0.5, self->enemy->r.maxs, point);
        VectorSubtract(point, self->s.origin, self->movedir);
        VectorNormalize(self->movedir);
    }

    VectorMA(self->s.origin, 2048, self->movedir, end);

    trap_Trace(&tr, self->s.origin, NULL, NULL, end, self->s.number, CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE);

    if (tr.entityNum) {
        G_Damage(&g_entities[tr.entityNum], self, self->activator, self->movedir,
                 tr.endpos, self->damage, DAMAGE_NO_KNOCKBACK, MOD_TARGET_LASER);
    }

    VectorCopy(tr.endpos, self->s.origin2);

    trap_LinkEntity(self);
    self->nextthink = level.time + (1000 / sv_fps.integer);
}

void target_laser_on(gentity_t* self) {
    if (!self->activator)
        self->activator = self;
    target_laser_think(self);
}

void target_laser_off(gentity_t* self) {
    trap_UnlinkEntity(self);
    self->nextthink = 0;
}

void target_laser_use(gentity_t* self, gentity_t* other, gentity_t* activator) {
    self->activator = activator;
    if (self->nextthink > 0)
        target_laser_off(self);
    else
        target_laser_on(self);
}

void target_laser_start(gentity_t* self) {
    gentity_t* ent;

    self->s.eType = ET_BEAM;

    if (self->target) {
        ent = G_Find(NULL, FOFS(targetname), self->target);
        if (!ent) {
            G_Printf("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
        }
        self->enemy = ent;
    } else {
        G_SetMovedir(self->s.angles, self->movedir);
    }

    self->use = target_laser_use;
    self->think = target_laser_think;

    if (!self->damage) {
        self->damage = 1;
    }

    if (self->spawnflags & 1)
        target_laser_on(self);
    else
        target_laser_off(self);
}

void SP_target_laser(gentity_t* self) {
    self->think = target_laser_start;
    self->nextthink = level.time + (1000 / sv_fps.integer);
}

void target_teleporter_use(gentity_t* self, gentity_t* other, gentity_t* activator) {
    gentity_t* dest;

    if (!activator->client)
        return;
    dest = G_PickTarget(self->target);
    if (!dest) {
        G_Printf("Couldn't find teleporter destination\n");
        return;
    }

    TeleportPlayer(activator, dest->s.origin, dest->s.angles);
}

void SP_target_teleporter(gentity_t* self) {
    if (!self->targetname)
        G_Printf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

    self->use = target_teleporter_use;
}

void target_relay_use(gentity_t* self, gentity_t* other, gentity_t* activator) {
    if ((self->spawnflags & 1) && activator->client && activator->client->sess.sessionTeam != TEAM_RED) {
        return;
    }
    if ((self->spawnflags & 2) && activator->client && activator->client->sess.sessionTeam != TEAM_BLUE) {
        return;
    }
    if (self->spawnflags & 4) {
        gentity_t* ent;

        ent = G_PickTarget(self->target);
        if (ent && ent->use) {
            ent->use(ent, self, activator);
        }
        return;
    }
    G_UseTargets(self, activator);
}

void SP_target_relay(gentity_t* self) {
    self->use = target_relay_use;
}

void target_kill_use(gentity_t* self, gentity_t* other, gentity_t* activator) {
    G_Damage(activator, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void SP_target_kill(gentity_t* self) {
    self->use = target_kill_use;
}

void Use_Target_Cvar(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    trap_Cvar_Set(va("%s", ent->cvar), va("%f", ent->random));
}

void SP_target_cvar(gentity_t* ent) {
    char* s;

    G_SpawnString("cvarValue", "-1.0f", &s);
    ent->random = atof(s);
    ent->use = Use_Target_Cvar;
}

static const int target_achievementIds[8] = { 0, 2, 3, 4, 5, 6, 7, 8 };

void Use_Target_Achievement(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    int index;

    if (!activator->client) {
        return;
    }

    index = ent->health;
    if (index == 0 || index >= 8) {
        return;
    }

    if (!trap_HasAchievement(activator->client->ps.clientNum, target_achievementIds[index])) {
        trap_SetAchievement(activator->client->ps.clientNum, target_achievementIds[index]);
    }
}

void SP_target_achievement(gentity_t* ent) {
    char* s;

    G_SpawnString("id", "0", &s);
    ent->health = atoi(s);
    ent->use = Use_Target_Achievement;
}

void SP_target_position(gentity_t* self) {
    G_SetOrigin(self, self->s.origin);
}

static void target_location_linkup(gentity_t* ent) {
    int i;
    int n;

    if (level.locationLinked)
        return;

    level.locationLinked = qtrue;

    level.locationHead = NULL;

    trap_SetConfigstring(CS_LOCATIONS, "unknown");

    for (i = 0, ent = g_entities, n = 1;
         i < level.num_entities;
         i++, ent++) {
        if (ent->classname && !Q_stricmp(ent->classname, "target_location")) {
            ent->health = n;
            trap_SetConfigstring(CS_LOCATIONS + n, ent->message);
            n++;
            ent->nextTrain = level.locationHead;
            level.locationHead = ent;
        }
    }

}

void SP_target_location(gentity_t* self) {
    self->think = target_location_linkup;
    self->nextthink = level.time + 200;

    G_SetOrigin(self, self->s.origin);
}
