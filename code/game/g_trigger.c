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

void InitTrigger(gentity_t* self) {
    if (!VectorCompare(self->s.angles, vec3_origin))
        G_SetMovedir(self->s.angles, self->movedir);

    trap_SetBrushModel(self, self->model);
    self->r.contents = CONTENTS_TRIGGER;
    self->r.svFlags = SVF_NOCLIENT;
}

void multi_wait(gentity_t* ent) {
    ent->nextthink = 0;
}

void multi_trigger(gentity_t* ent, gentity_t* activator) {
    ent->activator = activator;
    if (ent->nextthink) {
        return;
    }

    if (activator->client) {
        if ((ent->spawnflags & 1) &&
            activator->client->sess.sessionTeam != TEAM_RED) {
            return;
        }
        if ((ent->spawnflags & 2) &&
            activator->client->sess.sessionTeam != TEAM_BLUE) {
            return;
        }
    }

    G_UseTargets(ent, ent->activator);

    if (ent->wait > 0) {
        ent->think = multi_wait;
        ent->nextthink = level.time + (ent->wait + ent->random * crandom()) * 1000;
    } else {
        ent->touch = 0;
        ent->nextthink = level.time + (1000 / sv_fps.integer);
        ent->think = G_FreeEntity;
    }
}

void Use_Multi(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    multi_trigger(ent, activator);
}

void Touch_Multi(gentity_t* self, gentity_t* other, trace_t* trace) {
    if (!other->client) {
        return;
    }
    multi_trigger(self, other);
}

void SP_trigger_multiple(gentity_t* ent) {
    G_SpawnFloat("wait", "0.5", &ent->wait);
    G_SpawnFloat("random", "0", &ent->random);

    if (ent->random >= ent->wait && ent->wait >= 0) {
        ent->random = ent->wait - (1000 / sv_fps.integer);
        G_Printf("trigger_multiple has random >= wait\n");
    }

    ent->touch = Touch_Multi;
    ent->use = Use_Multi;

    InitTrigger(ent);
    trap_LinkEntity(ent);
}

void trigger_always_think(gentity_t* ent) {
    G_UseTargets(ent, ent);
    G_FreeEntity(ent);
}

void SP_trigger_always(gentity_t* ent) {
    ent->nextthink = level.time + 300;
    ent->think = trigger_always_think;
}

void trigger_push_touch(gentity_t* self, gentity_t* other, trace_t* trace) {
    if (!other->client) {
        return;
    }
    if (other->client->hook) {
        return;
    }

    BG_TouchJumpPad(&other->client->ps, &self->s);
}

void AimAtTarget(gentity_t* self) {
    gentity_t* ent;
    vec3_t origin;
    float height, gravity, time, forward;
    float dist;

    VectorAdd(self->r.absmin, self->r.absmax, origin);
    VectorScale(origin, 0.5, origin);

    ent = G_PickTarget(self->target);
    if (!ent) {
        G_FreeEntity(self);
        return;
    }

    height = ent->s.origin[2] - origin[2];
    gravity = g_gravity.value;
    time = sqrt(height / (.5 * gravity));
    if (!time) {
        G_FreeEntity(self);
        return;
    }

    VectorSubtract(ent->s.origin, origin, self->s.origin2);
    self->s.origin2[2] = 0;
    dist = VectorNormalize(self->s.origin2);

    forward = dist / time;
    VectorScale(self->s.origin2, forward, self->s.origin2);

    self->s.origin2[2] = time * gravity;

    self->s.origin2[0] = (int)self->s.origin2[0];
    self->s.origin2[1] = (int)self->s.origin2[1];
    self->s.origin2[2] = (int)self->s.origin2[2];
}

void SP_trigger_push(gentity_t* self) {
    InitTrigger(self);

    self->r.svFlags &= ~SVF_NOCLIENT;

    G_SoundIndex("sound/world/jumppad.wav");

    self->s.eType = ET_PUSH_TRIGGER;
    self->touch = trigger_push_touch;
    self->framethink = AimAtTarget;
    trap_LinkEntity(self);
}

void Use_target_push(gentity_t* self, gentity_t* other, gentity_t* activator) {
    if (!activator->client) {
        return;
    }

    if (activator->client->ps.pm_type != PM_NORMAL) {
        return;
    }
    if (activator->client->ps.powerups[PW_FLIGHT]) {
        return;
    }
    if (activator->client->hook) {
        return;
    }

    VectorCopy(self->s.origin2, activator->client->ps.velocity);

    if (activator->fly_sound_debounce_time < level.time) {
        activator->fly_sound_debounce_time = level.time + 1500;
        G_GlobalSound(self->noise_index);
    }
}

void SP_target_push(gentity_t* self) {
    if (!self->speed) {
        self->speed = 1000;
    }
    G_SetMovedir(self->s.angles, self->s.origin2);
    VectorScale(self->s.origin2, self->speed, self->s.origin2);

    if (self->spawnflags & 1) {
        self->noise_index = G_SoundIndex("sound/world/jumppad.wav");
    }
    if (self->target) {
        VectorCopy(self->s.origin, self->r.absmin);
        VectorCopy(self->s.origin, self->r.absmax);
        self->think = AimAtTarget;
        self->nextthink = level.time + (1000 / sv_fps.integer);
    }
    self->use = Use_target_push;
}

void trigger_teleporter_touch(gentity_t* self, gentity_t* other, trace_t* trace) {
    gentity_t* dest;

    if (!other->client) {
        return;
    }
    if (other->client->ps.pm_type == PM_DEAD) {
        return;
    }
    if ((self->spawnflags & 1) &&
        other->client->sess.sessionTeam != TEAM_SPECTATOR) {
        return;
    }

    dest = G_PickTarget(self->target);
    if (!dest) {
        G_Printf("Couldn't find teleporter destination\n");
        return;
    }

    TeleportPlayer(other, dest->s.origin, dest->s.angles);
}

void SP_trigger_teleport(gentity_t* self) {
    InitTrigger(self);

    if (self->spawnflags & 1) {
        self->r.svFlags |= SVF_NOCLIENT;
    } else {
        self->r.svFlags &= ~SVF_NOCLIENT;
    }

    G_SoundIndex("sound/world/jumppad.wav");

    self->s.eType = ET_TELEPORT_TRIGGER;
    self->touch = trigger_teleporter_touch;

    trap_LinkEntity(self);
}

void hurt_use(gentity_t* self, gentity_t* other, gentity_t* activator) {
    if (self->r.linked) {
        trap_UnlinkEntity(self);
    } else {
        trap_LinkEntity(self);
    }
}

void hurt_touch(gentity_t* self, gentity_t* other, trace_t* trace) {
    int dflags;

    if (!other->takedamage) {
        return;
    }

    if (self->timestamp > level.time) {
        return;
    }

    if (self->spawnflags & 16) {
        self->timestamp = level.time + 1000;
    } else {
        self->timestamp = level.time + FRAMETIME;
    }

    if (!(self->spawnflags & 4)) {
        G_Sound(other, CHAN_AUTO, self->noise_index);
    }

    if (self->spawnflags & 8)
        dflags = DAMAGE_NO_PROTECTION;
    else
        dflags = 0;
    G_Damage(other, self, self, NULL, NULL, self->damage, dflags, MOD_TRIGGER_HURT);
}

void SP_trigger_hurt(gentity_t* self) {
    InitTrigger(self);

    self->noise_index = G_SoundIndex("sound/world/electro.wav");
    self->touch = hurt_touch;

    if (!self->damage) {
        self->damage = 5;
    }

    self->r.contents = CONTENTS_TRIGGER;

    self->use = hurt_use;

    if (self->spawnflags & 1) {
        trap_UnlinkEntity(self);
    } else {
        trap_LinkEntity(self);
    }
}

void func_timer_think(gentity_t* self) {
    G_UseTargets(self, self->activator);
    self->nextthink = level.time + 1000 * (self->wait + crandom() * self->random);
}

void func_timer_use(gentity_t* self, gentity_t* other, gentity_t* activator) {
    self->activator = activator;

    if (self->nextthink) {
        self->nextthink = 0;
        return;
    }

    func_timer_think(self);
}

void SP_func_timer(gentity_t* self) {
    G_SpawnFloat("random", "1", &self->random);
    G_SpawnFloat("wait", "1", &self->wait);

    self->use = func_timer_use;
    self->think = func_timer_think;

    if (self->random >= self->wait) {
        self->random = self->wait - (1000 / sv_fps.integer);
        G_Printf("func_timer at %s has random >= wait\n", vtos(self->s.origin));
    }

    if (self->spawnflags & 1) {
        self->nextthink = level.time + (1000 / sv_fps.integer);
        self->activator = self;
    }

    self->r.svFlags = SVF_NOCLIENT;
}

void CaptureZone_Touch(gentity_t* self, gentity_t* other) {
    gentity_t* thrown = NULL;
    gentity_t* flag;
    int team;

    if (!other->client && other->item &&
        (g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF) && other->item->giType == IT_TEAM) {
        thrown = other;
        other = other->parent;
    }
    if (!other || !other->client || !self->team) {
        return;
    }

    team = strcmp("red", self->team);
    team = (team == 0) ? TEAM_RED : (team > 0 ? TEAM_BLUE : TEAM_FREE);
    if (other->client->sess.sessionTeam == team) {
        return;
    }

    flag = G_Find(NULL, FOFS(classname), team == TEAM_RED ? "team_CTF_redflag" : "team_CTF_blueflag");
    if (flag) {
        Team_TouchOurFlag(flag, other, other->client->sess.sessionTeam, thrown, self->count);
    }
}

static void CaptureZone_TriggerTouch(gentity_t* self, gentity_t* other, trace_t* trace) {
    CaptureZone_Touch(self, other);
}

void SP_trigger_capturezone(gentity_t* self) {
    InitTrigger(self);
    self->touch = CaptureZone_TriggerTouch;
    self->r.contents = CONTENTS_TRIGGER;
}
