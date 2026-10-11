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

#define MISSILE_PRESTEP_TIME (1000 / sv_fps.integer)

#define SURF_QL_DMGTHROUGH 0x200000

void G_BounceMissile(gentity_t* ent, trace_t* trace) {
    vec3_t velocity;
    float dot;
    int hitTime;

    hitTime = (int)((double)level.time - trace->fraction * (double)level.frametime);
    BG_EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity);
    dot = DotProduct(velocity, trace->plane.normal);
    VectorMA(velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta);

    if (ent->s.eFlags & EF_BOUNCE_HALF) {
        VectorScale(ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta);
        if (trace->plane.normal[2] > 0.2 && VectorLength(ent->s.pos.trDelta) < 40) {
            trace->endpos[0] = (int)trace->endpos[0];
            trace->endpos[1] = (int)trace->endpos[1];
            trace->endpos[2] = (int)(trace->endpos[2] + 1.0);
            G_SetOrigin(ent, trace->endpos);
            return;
        }
    }

    VectorAdd(ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
    VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
    ent->s.pos.trTime = level.time;
}

void G_ExplodeMissile(gentity_t* ent) {
    vec3_t dir;
    vec3_t origin;

    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
    SnapVector(origin);
    G_SetOrigin(ent, origin);

    dir[0] = dir[1] = 0;
    dir[2] = 1;

    ent->s.eType = ET_GENERAL;
    G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(dir));

    ent->freeAfterEvent = qtrue;

    if (ent->splashDamage) {
        if (G_RadiusDamage(ent->r.currentOrigin, NULL, NULL, ent, ent)) {
            g_entities[ent->r.ownerNum].client->accuracy_hits++;
            g_entities[ent->r.ownerNum].client->expandedStats.shotsHit[ent->s.weapon]++;
            if (g_gametype.integer != GT_RACE) {
                STAT_AddHits(&g_entities[ent->r.ownerNum], ent->s.weapon, 1);
            }
            if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                g_entities[ent->r.ownerNum].client->round_hits++;
            }
        }
    }

    trap_LinkEntity(ent);
}

static void ProximityMine_Explode(gentity_t* mine) {
    G_ExplodeMissile(mine);
    if (mine->activator) {
        G_FreeEntity(mine->activator);
        mine->activator = NULL;
    }
}

static void ProximityMine_Die(gentity_t* ent, gentity_t* inflictor, gentity_t* attacker, int damage, int mod) {
    ent->think = ProximityMine_Explode;
    ent->nextthink = level.time + 1;
}

void ProximityMine_Trigger(gentity_t* trigger, gentity_t* other, trace_t* trace) {
    vec3_t v;
    gentity_t* mine;

    if (!other->client) {
        return;
    }

    VectorSubtract(trigger->s.pos.trBase, other->s.pos.trBase, v);
    if (VectorLength(v) > trigger->parent->splashRadius) {
        return;
    }

    if (g_gametype.integer >= GT_TEAM) {
        if (trigger->parent->s.generic1 == other->client->sess.sessionTeam) {
            return;
        }
    }

    if (!CanDamage(other, trigger->s.pos.trBase)) {
        return;
    }

    mine = trigger->parent;
    mine->s.loopSound = 0;
    G_AddEvent(mine, EV_PROXIMITY_MINE_TRIGGER, 0);
    mine->nextthink = level.time + 500;

    G_FreeEntity(trigger);
}

static void ProximityMine_Activate(gentity_t* ent) {
    gentity_t* trigger;
    float r;

    ent->think = ProximityMine_Explode;
    ent->nextthink = level.time + g_proxMineTimeout.integer * 1000;

    ent->takedamage = qtrue;
    ent->health = 1;
    ent->die = ProximityMine_Die;

    ent->s.loopSound = G_SoundIndex("sound/weapons/proxmine/wstbtick.wav");

    trigger = G_Spawn();

    trigger->classname = "proxmine_trigger";

    r = ent->splashRadius;
    VectorSet(trigger->r.mins, -r, -r, -r);
    VectorSet(trigger->r.maxs, r, r, r);

    G_SetOrigin(trigger, ent->s.pos.trBase);

    trigger->parent = ent;
    trigger->r.contents = CONTENTS_TRIGGER;
    trigger->touch = ProximityMine_Trigger;

    trap_LinkEntity(trigger);

    ent->activator = trigger;
}

static void ProximityMine_ExplodeOnPlayer(gentity_t* mine) {
    gentity_t* player;

    player = mine->enemy;
    player->client->ps.eFlags &= ~EF_TICKING;

    if (player->client->invulnerabilityTime > level.time) {
        G_Damage(player, mine->parent, mine->parent, vec3_origin, mine->s.origin, 1000, DAMAGE_NO_KNOCKBACK, MOD_JUICED);
        player->client->invulnerabilityTime = 0;
        G_TempEntity(player->client->ps.origin, EV_JUICED);
    } else {
        G_SetOrigin(mine, player->s.pos.trBase);
        mine->r.svFlags &= ~SVF_NOCLIENT;
        mine->splashMethodOfDeath = MOD_PROXIMITY_MINE;
        G_ExplodeMissile(mine);
    }
}

static void ProximityMine_Player(gentity_t* mine, gentity_t* player) {
    if (mine->s.eFlags & EF_NODRAW) {
        return;
    }

    G_AddEvent(mine, EV_PROXIMITY_MINE_STICK, 0);

    if (player->s.eFlags & EF_TICKING) {
        player->activator->splashDamage += mine->splashDamage;
        mine->think = G_FreeEntity;
        mine->nextthink = level.time;
        return;
    }

    player->client->ps.eFlags |= EF_TICKING;
    player->activator = mine;

    mine->s.eFlags |= EF_NODRAW;
    mine->r.svFlags |= SVF_NOCLIENT;
    mine->s.pos.trType = TR_LINEAR;
    VectorClear(mine->s.pos.trDelta);

    mine->enemy = player;
    mine->think = ProximityMine_ExplodeOnPlayer;
    if (player->client->invulnerabilityTime > level.time) {
        mine->nextthink = level.time + 2 * 1000;
    } else {
        mine->nextthink = level.time + 10 * 1000;
    }
}

void G_MissileImpact(gentity_t* ent, trace_t* trace) {
    gentity_t* other;
    qboolean hitClient = qfalse;
    qboolean damageThrough = qfalse;
    vec3_t forward, impactpoint, bouncedir, throughOrigin;
    int eFlags;

    other = &g_entities[trace->entityNum];

    if (!other->takedamage &&
        (ent->s.eFlags & (EF_BOUNCE | EF_BOUNCE_HALF | EF_NAILBOUNCE)) &&
        (!(ent->s.eFlags & EF_NAILBOUNCE) || ent->bouncecount < g_nailbounce.integer)) {
        G_BounceMissile(ent, trace);
        G_AddEvent(ent, EV_GRENADE_BOUNCE, 0);
        ent->bouncecount++;
        return;
    }

    if (other->takedamage && ent->s.weapon != WP_PROX_LAUNCHER) {
        if (other->client && other->client->invulnerabilityTime > level.time) {
            VectorCopy(ent->s.pos.trDelta, forward);
            VectorNormalize(forward);
            if (G_InvulnerabilityEffect(other, forward, ent->s.pos.trBase, impactpoint, bouncedir)) {
                VectorCopy(bouncedir, trace->plane.normal);
                eFlags = ent->s.eFlags & EF_BOUNCE_HALF;
                ent->s.eFlags &= ~EF_BOUNCE_HALF;
                G_BounceMissile(ent, trace);
                ent->s.eFlags |= eFlags;
            }
            ent->target_ent = other;
            return;
        }
    }

    if (ent->s.weapon == WP_PROX_LAUNCHER) {
        if (ent->s.pos.trType != TR_GRAVITY) {
            return;
        }

        if (other->s.eType == ET_PLAYER && other->health > 0) {
            ProximityMine_Player(ent, other);
            return;
        }

        SnapVectorTowards(trace->endpos, ent->s.pos.trBase);
        G_SetOrigin(ent, trace->endpos);
        ent->s.pos.trType = TR_STATIONARY;
        VectorClear(ent->s.pos.trDelta);

        G_AddEvent(ent, EV_PROXIMITY_MINE_STICK, trace->surfaceFlags);

        ent->think = ProximityMine_Activate;
        ent->nextthink = level.time + 2000;

        vectoangles(trace->plane.normal, ent->s.angles);
        ent->s.angles[0] += 90;

        ent->enemy = other;
        ent->die = ProximityMine_Die;
        VectorCopy(trace->plane.normal, ent->movedir);
        VectorSet(ent->r.mins, -4, -4, -4);
        VectorSet(ent->r.maxs, 4, 4, 4);
        VectorMA(ent->r.mins, 4, trace->plane.normal, ent->r.mins);
        VectorMA(ent->r.maxs, 4, trace->plane.normal, ent->r.maxs);
        trap_LinkEntity(ent);

        return;
    }

    if (!strcmp(ent->classname, "hook")) {
        gentity_t* nent;
        vec3_t v, into;

        nent = G_Spawn();
        if (other->takedamage && other->client) {
            G_AddEvent(ent, EV_MISSILE_HIT, DirToByte(trace->plane.normal));
            nent->s.otherEntityNum = other->s.number;

            ent->enemy = other;
            ent->eventTime = level.time + weapon_reload_hook.integer;

            v[0] = other->r.currentOrigin[0] + (other->r.mins[0] + other->r.maxs[0]) * 0.5;
            v[1] = other->r.currentOrigin[1] + (other->r.mins[1] + other->r.maxs[1]) * 0.5;
            v[2] = other->r.currentOrigin[2] + (other->r.mins[2] + other->r.maxs[2]) * 0.5;

            other->client->ps.pm_flags |= PMF_HOOKED;
            SnapVectorTowards(v, ent->s.pos.trBase);
        } else {
            VectorCopy(trace->endpos, v);
            G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
            if (other->s.eType == ET_MOVER) {
                if (!strcmp(other->classname, "func_train")) {
                    ent->enemy = other;
                    ent->count = 0;
                } else if (!strcmp(other->classname, "func_door") || !strcmp(other->classname, "func_rotating")) {
                    ent->enemy = other;
                    ent->count = 1;
                }
            } else {
                ent->enemy = NULL;
            }
        }

        SnapVectorTowards(v, ent->s.pos.trBase);

        nent->freeAfterEvent = qtrue;
        nent->s.eType = ET_GENERAL;
        ent->s.eType = ET_GRAPPLE;

        VectorNegate(trace->plane.normal, into);
        vectoangles(into, ent->s.angles);
        ent->s.generic1 = DirToByte(trace->plane.normal);

        G_SetOrigin(ent, v);
        G_SetOrigin(nent, v);

        ent->think = Weapon_HookThink;
        ent->nextthink = level.time - (int)(1000.0 / sv_fps.integer);

        ent->parent->client->ps.pm_flags |= PMF_GRAPPLE_PULL;
        VectorCopy(ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);

        trap_LinkEntity(ent);
        trap_LinkEntity(nent);

        return;
    }

    VectorCopy(ent->s.pos.trDelta, forward);
    VectorNormalize(forward);

    if (other->takedamage && other->client) {
        G_AddEvent(ent, EV_MISSILE_HIT, DirToByte(trace->plane.normal));
        ent->s.otherEntityNum = other->s.number;
    } else if (ent->s.weapon == WP_ROCKET_LAUNCHER &&
               (g_forceDmgThroughSurface.integer || (trace->surfaceFlags & SURF_QL_DMGTHROUGH))) {
        if (-trace->plane.normal[2] >= g_dmgThroughSurfaceAngularThreshold.value) {
            vec3_t start;
            trace_t tr;

            VectorMA(trace->endpos, g_dmgThroughSurfaceDistance.value, trace->plane.normal, start);
            trap_Trace(&tr, start, vec3_origin, vec3_origin, trace->endpos, ENTITYNUM_NONE, CONTENTS_SOLID);
            if (tr.fraction < 1.0f && !tr.startsolid && !tr.allsolid) {
                G_AddEvent(ent, EV_MISSILE_MISS_DMGTHROUGH, DirToByte(trace->plane.normal));
                VectorCopy(tr.endpos, throughOrigin);
                damageThrough = qtrue;
            } else {
                G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
            }
        } else {
            G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
        }
    } else if (trace->surfaceFlags & SURF_METALSTEPS) {
        G_AddEvent(ent, EV_MISSILE_MISS_METAL, DirToByte(trace->plane.normal));
    } else {
        G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
    }

    if (ent->damage && other->takedamage) {
        vec3_t velocity;

        if (LogAccuracyHit(other, &g_entities[ent->r.ownerNum])) {
            g_entities[ent->r.ownerNum].client->accuracy_hits++;
            g_entities[ent->r.ownerNum].client->expandedStats.shotsHit[ent->s.weapon]++;
            if (g_gametype.integer != GT_RACE) {
                STAT_AddHits(&g_entities[ent->r.ownerNum], ent->s.weapon, 1);
            }
            hitClient = qtrue;
            if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                g_entities[ent->r.ownerNum].client->round_hits++;
            }
        }
        BG_EvaluateTrajectoryDelta(&ent->s.pos, level.time, velocity);
        if (VectorLength(velocity) == 0) {
            velocity[2] = 1;
        }
        G_Damage(other, ent, &g_entities[ent->r.ownerNum], velocity,
                 ent->s.origin, ent->damage,
                 0, ent->methodOfDeath);
    }

    ent->freeAfterEvent = qtrue;

    ent->s.eType = ET_GENERAL;

    SnapVectorTowards(trace->endpos, ent->s.pos.trBase);

    G_SetOrigin(ent, trace->endpos);

    if (ent->splashDamage) {
        qboolean hit;

        if (damageThrough) {
            hit = G_RadiusDamageThrough(trace->endpos, forward, trace, throughOrigin, ent, other);
        } else {
            hit = G_RadiusDamage(trace->endpos, forward, trace, ent, other);
        }
        if (hit && !hitClient) {
            g_entities[ent->r.ownerNum].client->accuracy_hits++;
            g_entities[ent->r.ownerNum].client->expandedStats.shotsHit[ent->s.weapon]++;
            if (g_gametype.integer != GT_RACE) {
                STAT_AddHits(&g_entities[ent->r.ownerNum], ent->s.weapon, 1);
            }
            if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                g_entities[ent->r.ownerNum].client->round_hits++;
            }
        }
    }

    trap_LinkEntity(ent);
}

void G_RunMissile(gentity_t* ent) {
    vec3_t origin;
    trace_t tr;
    int passent;

    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);

    if (ent->target_ent) {
        passent = ent->target_ent->s.number;
    }

    else if (ent->s.weapon == WP_PROX_LAUNCHER && ent->count) {
        passent = ENTITYNUM_NONE;
    }

    else {
        passent = ent->r.ownerNum;
    }
    if (g_playerCylinders.integer) {
        trap_TraceCapsule(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask);
    } else {
        trap_Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask);
    }

    if (tr.startsolid || tr.allsolid) {
        if (g_playerCylinders.integer) {
            trap_TraceCapsule(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask);
        } else {
            trap_Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask);
        }
        tr.fraction = 0;
    } else {
        VectorCopy(tr.endpos, ent->r.currentOrigin);
    }

    trap_LinkEntity(ent);

    if (tr.fraction != 1) {
        if (tr.surfaceFlags & SURF_NOIMPACT) {
            if (ent->parent && ent->parent->client && ent->parent->client->hook == ent) {
                Weapon_HookFree(ent);
            }
            G_FreeEntity(ent);
            return;
        }
        G_MissileImpact(ent, &tr);
        if (ent->s.eType != ET_MISSILE) {
            return;
        }
    }

    if (ent->s.weapon == WP_PROX_LAUNCHER && !ent->count) {
        trap_Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, ENTITYNUM_NONE, ent->clipmask);
        if (!tr.startsolid || tr.entityNum != ent->r.ownerNum) {
            ent->count = 1;
        }
    }

    G_RunThink(ent);
}

static gentity_t* CreateMissile(vec3_t start, vec3_t dir, int speed, gentity_t* owner) {
    gentity_t* bolt;

    bolt = G_Spawn();
    bolt->think = G_ExplodeMissile;
    bolt->s.eType = ET_MISSILE;
    bolt->s.eFlags = 0;
    bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    bolt->r.ownerNum = owner->s.number;
    bolt->parent = owner;
    bolt->clipmask = MASK_SHOT;
    bolt->target_ent = NULL;

    if (pmove_NoPlayerClip.integer) {
        bolt->clipmask &= ~CONTENTS_BODY;
    }

    VectorCopy(start, bolt->s.pos.trBase);
    SnapVector(bolt->s.pos.trBase);
    VectorCopy(bolt->s.pos.trBase, bolt->r.currentOrigin);

    if (weapon_gravity_rl.integer == 0 || g_guidedRocket.integer != 0) {
        bolt->s.pos.trType = TR_LINEAR;
    } else {
        bolt->s.pos.trType = TR_GRAVITY;
    }
    bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;

    VectorNormalize(dir);
    VectorScale(dir, speed, bolt->s.pos.trDelta);
    SnapVector(bolt->s.pos.trDelta);

    VectorSet(bolt->r.mins, -0.01f, -0.01f, -0.01f);
    VectorSet(bolt->r.maxs, 0.01f, 0.01f, 0.01f);

    return bolt;
}

static void G_PlasmaAccelThink(gentity_t* ent) {
    VectorScale(ent->s.pos.trDelta, g_accelFactor_pg.value, ent->s.pos.trDelta);
    ent->nextthink = (int)((double)level.time + g_accelRate_pg.value);
}

gentity_t* fire_plasma(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* bolt;

    bolt = CreateMissile(start, dir, g_velocity_pg.integer, self);
    bolt->classname = "plasma";
    bolt->nextthink = level.time + 10000;
    bolt->s.weapon = WP_PLASMAGUN;
    bolt->r.ownerNum = self->s.number;
    bolt->s.otherEntityNum = self->s.number;
    bolt->damage = g_damage_pg.integer;
    bolt->splashDamage = g_splashdamage_pg.integer;
    bolt->splashRadius = g_splashradius_pg.integer;
    bolt->methodOfDeath = MOD_PLASMA;
    bolt->splashMethodOfDeath = MOD_PLASMA_SPLASH;
    bolt->s.pos.trType = weapon_gravity_pg.integer ? TR_GRAVITY : TR_LINEAR;

    if (g_accelFactor_pg.value != 1.0f) {
        bolt->nextthink = (int)((double)level.time + g_accelRate_pg.value);
        bolt->think = G_PlasmaAccelThink;
    }

    return bolt;
}

gentity_t* fire_grenade(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* bolt;

    bolt = CreateMissile(start, dir, g_velocity_gl.integer, self);
    bolt->classname = "grenade";
    bolt->nextthink = level.time + 2500;
    bolt->s.weapon = WP_GRENADE_LAUNCHER;
    bolt->r.ownerNum = self->s.number;
    bolt->s.otherEntityNum = self->s.number;
    bolt->damage = g_damage_gl.integer;
    bolt->splashDamage = g_splashdamage_gl.integer;
    bolt->splashRadius = g_splashradius_gl.integer;
    bolt->methodOfDeath = MOD_GRENADE;
    bolt->splashMethodOfDeath = MOD_GRENADE_SPLASH;
    bolt->s.eFlags = EF_BOUNCE_HALF;
    bolt->s.pos.trType = TR_GRAVITY;
    bolt->s.clientNum = self->s.clientNum;

    return bolt;
}

static void G_BFGAccelThink(gentity_t* ent) {
    VectorScale(ent->s.pos.trDelta, g_accelFactor_bfg.value, ent->s.pos.trDelta);
    ent->nextthink = (int)((double)level.time + g_accelRate_bfg.value);
}

gentity_t* fire_bfg(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* bolt;

    bolt = CreateMissile(start, dir, g_velocity_bfg.integer, self);
    bolt->classname = "bfg";
    bolt->nextthink = level.time + 10000;
    bolt->s.weapon = WP_BFG;
    bolt->r.ownerNum = self->s.number;
    bolt->s.otherEntityNum = self->s.number;
    bolt->damage = g_damage_bfg.integer;
    bolt->splashDamage = g_splashdamage_bfg.integer;
    bolt->splashRadius = g_splashradius_bfg.integer;
    bolt->methodOfDeath = MOD_BFG;
    bolt->splashMethodOfDeath = MOD_BFG_SPLASH;

    if (g_accelFactor_bfg.value != 1.0f) {
        bolt->nextthink = (int)((double)level.time + g_accelRate_bfg.value);
        bolt->think = G_BFGAccelThink;
    }

    return bolt;
}

static void G_GuidedRocketThink(gentity_t *ent) {
    if (ent->parent->client) {
        vec3_t forward;
        AngleVectors(ent->parent->client->ps.viewangles, forward, NULL, NULL);
        VectorCopy(forward, ent->movedir);
        VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
        VectorScale(forward, 20, ent->s.pos.trDelta);
    }
    ent->nextthink = level.time + 25;
}

static void G_RocketAccelThink(gentity_t* ent) {
    VectorScale(ent->s.pos.trDelta, g_accelFactor_rl.value, ent->s.pos.trDelta);
    ent->nextthink = (int)((double)level.time + g_accelRate_rl.value);
}

gentity_t* fire_rocket(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* bolt;

    bolt = CreateMissile(start, dir, g_velocity_rl.integer, self);
    bolt->classname = "rocket";
    bolt->nextthink = level.time + 15000;
    bolt->s.weapon = WP_ROCKET_LAUNCHER;
    bolt->r.ownerNum = self->s.number;
    bolt->s.otherEntityNum = self->s.number;
    bolt->damage = g_damage_rl.integer;
    bolt->splashDamage = 100;
    bolt->splashRadius = g_splashradius_rl.integer;
    bolt->methodOfDeath = MOD_ROCKET;
    bolt->splashMethodOfDeath = MOD_ROCKET_SPLASH;

    if (g_guidedRocket.integer) {
        bolt->nextthink = level.time - (int)(1000.0 / sv_fps.integer);
        bolt->think = G_GuidedRocketThink;
    }

    if (g_accelFactor_rl.value != 1.0f) {
        bolt->nextthink = (int)((double)level.time + g_accelRate_rl.value);
        bolt->think = G_RocketAccelThink;
    }

    return bolt;
}

gentity_t* fire_grapple(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* hook;

    VectorNormalize(dir);

    hook = G_Spawn();
    hook->classname = "hook";
    hook->nextthink = level.time + 10000;
    hook->think = Weapon_HookFree;
    hook->s.eType = ET_MISSILE;
    hook->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    hook->s.weapon = WP_GRAPPLING_HOOK;
    hook->r.ownerNum = self->s.number;
    hook->methodOfDeath = MOD_GRAPPLE;
    hook->clipmask = MASK_SHOT;
    if (pmove_NoPlayerClip.integer) {
        hook->clipmask &= ~CONTENTS_BODY;
    }
    hook->parent = self;
    hook->target_ent = NULL;
    hook->bouncecount = 0;
    hook->count = 0;
    hook->s.pos.trType = TR_LINEAR;
    hook->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;
    hook->s.otherEntityNum = self->s.number;
    hook->damage = g_damage_gh.integer;

    VectorCopy(start, hook->s.pos.trBase);
    VectorScale(dir, g_velocity_gh.integer, hook->s.pos.trDelta);
    SnapVector(hook->s.pos.trDelta);
    VectorCopy(start, hook->r.currentOrigin);

    self->client->hook = hook;

    return hook;
}

gentity_t* fire_nail(gentity_t* self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up) {
    gentity_t* bolt;
    vec3_t dir;
    vec3_t end;
    float r, u, scale;

    bolt = G_Spawn();
    bolt->classname = "nail";
    bolt->nextthink = level.time + 4500;
    bolt->think = G_ExplodeMissile;
    bolt->s.eType = ET_MISSILE;
    bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    bolt->s.weapon = WP_NAILGUN;
    bolt->r.ownerNum = self->s.number;
    bolt->parent = self;
    bolt->damage = g_damage_ng.integer;
    bolt->methodOfDeath = MOD_NAIL;
    bolt->clipmask = pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT;
    bolt->target_ent = NULL;
    bolt->s.otherEntityNum = self->s.number;

    bolt->s.pos.trType = weapon_gravity_ng.integer ? TR_GRAVITY : TR_LINEAR;
    bolt->s.pos.trTime = level.time;
    VectorCopy(start, bolt->s.pos.trBase);

    if (g_nailbounce.integer) {
        if ((rand() % 100) >= (100 - g_nailbouncepercentage.integer)) {
            bolt->s.eFlags = EF_NAILBOUNCE;
        }
    }

    r = random() * M_PI * 2.0f;
    u = sin(r) * crandom() * g_nailspread.integer * 16;
    r = cos(r) * crandom() * g_nailspread.integer * 16;
    VectorMA(start, 8192 * 16, forward, end);
    VectorMA(end, r, right, end);
    VectorMA(end, u, up, end);
    VectorSubtract(end, start, dir);
    VectorNormalize(dir);

    scale = g_nailspeed.integer + random() * 1800;
    VectorScale(dir, scale, bolt->s.pos.trDelta);
    SnapVector(bolt->s.pos.trDelta);

    VectorCopy(start, bolt->r.currentOrigin);

    return bolt;
}

gentity_t* fire_prox(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* bolt;

    VectorNormalize(dir);

    bolt = G_Spawn();
    bolt->classname = "prox mine";
    bolt->nextthink = level.time + 3000;
    bolt->think = G_ExplodeMissile;
    bolt->s.eType = ET_MISSILE;
    bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    bolt->s.weapon = WP_PROX_LAUNCHER;
    bolt->s.eFlags = 0;
    bolt->r.ownerNum = self->s.number;
    bolt->s.otherEntityNum = self->s.number;
    bolt->parent = self;
    bolt->damage = g_damage_pl.integer;
    bolt->splashDamage = g_splashdamage_pl.integer;
    bolt->splashRadius = g_splashradius_pl.integer;
    bolt->methodOfDeath = MOD_PROXIMITY_MINE;
    bolt->splashMethodOfDeath = MOD_PROXIMITY_MINE;
    bolt->clipmask = MASK_SHOT;
    if (pmove_NoPlayerClip.integer) {
        bolt->clipmask &= ~CONTENTS_BODY;
    }
    bolt->target_ent = NULL;
    bolt->count = 0;

    bolt->s.generic1 = self->client->sess.sessionTeam;

    bolt->s.pos.trType = TR_GRAVITY;
    bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;
    VectorCopy(start, bolt->s.pos.trBase);
    VectorScale(dir, 700, bolt->s.pos.trDelta);
    SnapVector(bolt->s.pos.trDelta);

    VectorCopy(start, bolt->r.currentOrigin);

    return bolt;
}
