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
#include "g_local.h"

// Binary uses 1000/sv_fps.integer instead of hardcoded 50ms
#define MISSILE_PRESTEP_TIME (1000 / sv_fps.integer)

// [QL] s.eFlags bit set by weapon_nailgun_fire on a bouncing nail. G_MissileImpact
// tests the bounce mask as 0x100030 (EF_BOUNCE|EF_BOUNCE_HALF|EF_QL_NAILBOUNCE) and
// caps nail bounces at g_nailbounce. Not yet named in bg_public.h (see report).
#define EF_QL_NAILBOUNCE 0x100000

// [QL] surface flag G_MissileImpact tests (qagamex86.dll 0x1005c3ab) before letting a rocket
// explode through the surface
#define SURF_QL_DMGTHROUGH 0x200000

/*
================
G_BounceMissile

[QL] qagamex86.dll 0x1005b350. The velocity is taken at the end of the frame minus the trace
fraction of the frame, and a grenade that comes to rest is set down one unit higher, so it
lies on the floor instead of in it.
================
*/
void G_BounceMissile(gentity_t* ent, trace_t* trace) {
    vec3_t velocity;
    float dot;
    int hitTime;

    // reflect the velocity on the trace plane
    hitTime = (int)((double)level.time - trace->fraction * (double)level.frametime);
    BG_EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity);
    dot = DotProduct(velocity, trace->plane.normal);
    VectorMA(velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta);

    if (ent->s.eFlags & EF_BOUNCE_HALF) {
        VectorScale(ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta);
        // check for stop
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

/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile(gentity_t* ent) {
    vec3_t dir;
    vec3_t origin;

    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
    SnapVector(origin);
    G_SetOrigin(ent, origin);

    // we don't have a valid direction, so just point straight up
    dir[0] = dir[1] = 0;
    dir[2] = 1;

    ent->s.eType = ET_GENERAL;
    G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(dir));

    ent->freeAfterEvent = qtrue;

    // splash damage
    // [QL] 0x1005b624: the missile is its own inflictor; with no impact there is no trace or direction
    if (ent->splashDamage) {
        if (G_RadiusDamage(ent->r.currentOrigin, NULL, NULL, ent, ent)) {
            g_entities[ent->r.ownerNum].client->accuracy_hits++;
            g_entities[ent->r.ownerNum].client->expandedStats.shotsHit[ent->s.weapon]++;
            // [QL] binary inlines STAT_AddScore (engine syscall table +0x324); it
            //      internally gates on !warmupTime && !scoringDisabled.
            if (g_gametype.integer != GT_RACE) {
                STAT_AddScore(&g_entities[ent->r.ownerNum], ent->s.weapon, 1);
            }
            if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                g_entities[ent->r.ownerNum].client->round_hits++;
            }
        }
    }

    trap_LinkEntity(ent);
}

/*
================
ProximityMine_Explode
================
*/
static void ProximityMine_Explode(gentity_t* mine) {
    G_ExplodeMissile(mine);
    // if the prox mine has a trigger free it
    if (mine->activator) {
        G_FreeEntity(mine->activator);
        mine->activator = NULL;
    }
}

/*
================
ProximityMine_Die
================
*/
static void ProximityMine_Die(gentity_t* ent, gentity_t* inflictor, gentity_t* attacker, int damage, int mod) {
    ent->think = ProximityMine_Explode;
    ent->nextthink = level.time + 1;
}

/*
================
ProximityMine_Trigger
================
*/
void ProximityMine_Trigger(gentity_t* trigger, gentity_t* other, trace_t* trace) {
    vec3_t v;
    gentity_t* mine;

    if (!other->client) {
        return;
    }

    // trigger is a cube, do a distance test now to act as if it's a sphere
    VectorSubtract(trigger->s.pos.trBase, other->s.pos.trBase, v);
    if (VectorLength(v) > trigger->parent->splashRadius) {
        return;
    }

    if (g_gametype.integer >= GT_TEAM) {
        // don't trigger same team mines
        if (trigger->parent->s.generic1 == other->client->sess.sessionTeam) {
            return;
        }
    }

    // ok, now check for ability to damage so we don't get triggered through walls, closed doors, etc...
    if (!CanDamage(other, trigger->s.pos.trBase)) {
        return;
    }

    // trigger the mine!
    mine = trigger->parent;
    mine->s.loopSound = 0;
    G_AddEvent(mine, EV_PROXIMITY_MINE_TRIGGER, 0);
    mine->nextthink = level.time + 500;

    G_FreeEntity(trigger);
}

/*
================
ProximityMine_Activate
================
*/
static void ProximityMine_Activate(gentity_t* ent) {
    gentity_t* trigger;
    float r;

    ent->think = ProximityMine_Explode;
    // [QL] g_proxMineTimeout is in SECONDS in QL (default 20s), binary does *1000 here
    // (verified from QLDS qagamex86.dll: IMUL EAX, EAX, 0x3e8 at 0x1005b8b1)
    ent->nextthink = level.time + g_proxMineTimeout.integer * 1000;

    ent->takedamage = qtrue;
    ent->health = 1;
    ent->die = ProximityMine_Die;

    ent->s.loopSound = G_SoundIndex("sound/weapons/proxmine/wstbtick.wav");

    // build the proximity trigger
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

    // set pointer to trigger so the entity can be freed when the mine explodes
    ent->activator = trigger;
}

/*
================
ProximityMine_ExplodeOnPlayer
================
*/
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
        // make sure the explosion gets to the client
        mine->r.svFlags &= ~SVF_NOCLIENT;
        mine->splashMethodOfDeath = MOD_PROXIMITY_MINE;
        G_ExplodeMissile(mine);
    }
}

/*
================
ProximityMine_Player
================
*/
static void ProximityMine_Player(gentity_t* mine, gentity_t* player) {
    if (mine->s.eFlags & EF_NODRAW) {
        return;
    }

    G_AddEvent(mine, EV_PROXIMITY_MINE_STICK, 0);

    if (player->s.eFlags & EF_TICKING) {
        // [QL] binary (0x1005bb20) only accumulates splashDamage; the Q3
        //      splashRadius *= 1.5 line is NOT present.
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

/*
================
G_MissileImpact

[QL] qagamex86.dll 0x1005bbe0. A prox mine sticks and a hook latches before any damage, and
neither deals direct damage here. Other missiles send their impact event first. A rocket on a
surface marked for it, or with g_forceDmgThroughSurface, checks whether it can explode through:
facing down onto the surface at least g_dmgThroughSurfaceAngularThreshold, with solid again
within g_dmgThroughSurfaceDistance behind it. Then come the direct damage and the splash.
================
*/
void G_MissileImpact(gentity_t* ent, trace_t* trace) {
    gentity_t* other;
    qboolean hitClient = qfalse;
    qboolean damageThrough = qfalse;
    vec3_t forward, impactpoint, bouncedir, throughOrigin;
    int eFlags;

    other = &g_entities[trace->entityNum];

    // check for bounce
    // [QL] the bounce mask is 0x100030 (EF_BOUNCE|EF_BOUNCE_HALF|EF_QL_NAILBOUNCE). Only
    //      EF_QL_NAILBOUNCE missiles are capped (g_nailbounce); every bounce counts.
    if (!other->takedamage &&
        (ent->s.eFlags & (EF_BOUNCE | EF_BOUNCE_HALF | EF_QL_NAILBOUNCE)) &&
        (!(ent->s.eFlags & EF_QL_NAILBOUNCE) || ent->bouncecount < g_nailbounce.integer)) {
        G_BounceMissile(ent, trace);
        G_AddEvent(ent, EV_GRENADE_BOUNCE, 0);
        ent->bouncecount++;
        return;
    }

    if (other->takedamage && ent->s.weapon != WP_PROX_LAUNCHER) {
        if (other->client && other->client->invulnerabilityTime > level.time) {
            //
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

        // if it's a player, stick it on to them (flag them and remove this entity)
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

        // link the prox mine to the other entity
        ent->enemy = other;
        ent->die = ProximityMine_Die;
        VectorCopy(trace->plane.normal, ent->movedir);
        // [QL] 0x1005bea6: the box is pushed out along the surface normal
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
            // [QL] 0x1005bf5e: the hit event rides on the hook itself, and the player it holds is
            //      slowed (PMF_HOOKED)
            G_AddEvent(ent, EV_MISSILE_HIT, DirToByte(trace->plane.normal));
            nent->s.otherEntityNum = other->s.number;

            ent->enemy = other;
            ent->eventTime = level.time + weapon_reload_hook.integer;

            v[0] = other->r.currentOrigin[0] + (other->r.mins[0] + other->r.maxs[0]) * 0.5;
            v[1] = other->r.currentOrigin[1] + (other->r.mins[1] + other->r.maxs[1]) * 0.5;
            v[2] = other->r.currentOrigin[2] + (other->r.mins[2] + other->r.maxs[2]) * 0.5;

            other->client->ps.pm_flags |= PMF_HOOKED;
            SnapVectorTowards(v, ent->s.pos.trBase);  // save net bandwidth
            // [QL] 0x1005c012 then runs Pmove (0x100323a0) for the owner with an otherwise empty
            //      pmove_t: its serverTime 0 is behind commandTime, so it returns without doing anything
        } else {
            VectorCopy(trace->endpos, v);
            G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
            // [QL] 0x1005c097: it holds on to a train, and lets go of a door on its next frame
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

        SnapVectorTowards(v, ent->s.pos.trBase);  // save net bandwidth

        nent->freeAfterEvent = qtrue;
        // change over to a normal entity right at the point of impact
        nent->s.eType = ET_GENERAL;
        ent->s.eType = ET_GRAPPLE;

        // [QL] 0x1005c194: the hook faces into the surface and carries its normal
        VectorNegate(trace->plane.normal, into);
        vectoangles(into, ent->s.angles);
        ent->s.generic1 = DirToByte(trace->plane.normal);

        G_SetOrigin(ent, v);
        G_SetOrigin(nent, v);

        // [QL] 0x1005c2ab: a frame in the past, so it thinks straight away
        ent->think = Weapon_HookThink;
        ent->nextthink = level.time - (int)(1000.0 / sv_fps.integer);

        ent->parent->client->ps.pm_flags |= PMF_GRAPPLE_PULL;
        VectorCopy(ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);

        trap_LinkEntity(ent);
        trap_LinkEntity(nent);

        return;
    }

    // [QL] 0x1005c32a: the impact event comes before any damage
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

    // impact damage
    if (ent->damage && other->takedamage) {
        vec3_t velocity;

        if (LogAccuracyHit(other, &g_entities[ent->r.ownerNum])) {
            g_entities[ent->r.ownerNum].client->accuracy_hits++;
            g_entities[ent->r.ownerNum].client->expandedStats.shotsHit[ent->s.weapon]++;
            if (g_gametype.integer != GT_RACE) {
                STAT_AddScore(&g_entities[ent->r.ownerNum], ent->s.weapon, 1);
            }
            hitClient = qtrue;
            if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                g_entities[ent->r.ownerNum].client->round_hits++;
            }
        }
        BG_EvaluateTrajectoryDelta(&ent->s.pos, level.time, velocity);
        if (VectorLength(velocity) == 0) {
            velocity[2] = 1;  // stepped on a grenade
        }
        G_Damage(other, ent, &g_entities[ent->r.ownerNum], velocity,
                 ent->s.origin, ent->damage,
                 0, ent->methodOfDeath);
    }

    ent->freeAfterEvent = qtrue;

    // change over to a normal entity right at the point of impact
    ent->s.eType = ET_GENERAL;

    SnapVectorTowards(trace->endpos, ent->s.pos.trBase);  // save net bandwidth

    G_SetOrigin(ent, trace->endpos);

    // splash damage (doesn't apply to person directly hit)
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
                STAT_AddScore(&g_entities[ent->r.ownerNum], ent->s.weapon, 1);
            }
            if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                g_entities[ent->r.ownerNum].client->round_hits++;
            }
        }
    }

    trap_LinkEntity(ent);
}

/*
================
G_RunMissile
================
*/
void G_RunMissile(gentity_t* ent) {
    vec3_t origin;
    trace_t tr;
    int passent;

    // get current position
    BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);

    // if this missile bounced off an invulnerability sphere
    if (ent->target_ent) {
        passent = ent->target_ent->s.number;
    }

    // prox mines that left the owner bbox will attach to anything, even the owner
    else if (ent->s.weapon == WP_PROX_LAUNCHER && ent->count) {
        passent = ENTITYNUM_NONE;
    }

    else {
        // ignore interactions with the missile owner
        passent = ent->r.ownerNum;
    }
    // trace a line from the previous position to the current position
    // [QL] 0x1005c95a: with g_playerCylinders the missile is traced as a capsule
    if (g_playerCylinders.integer) {
        trap_TraceCapsule(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask);
    } else {
        trap_Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask);
    }

    if (tr.startsolid || tr.allsolid) {
        // make sure the tr.entityNum is set to the entity we're stuck in
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
        // never explode or bounce on sky
        if (tr.surfaceFlags & SURF_NOIMPACT) {
            // [QL] 0x1005ca64: a grappling hook is let go properly first
            if (ent->parent && ent->parent->client && ent->parent->client->hook == ent) {
                Weapon_HookFree(ent);
            }
            G_FreeEntity(ent);
            return;
        }
        G_MissileImpact(ent, &tr);
        if (ent->s.eType != ET_MISSILE) {
            return;  // exploded
        }
    }

    // if the prox mine wasn't yet outside the player body
    if (ent->s.weapon == WP_PROX_LAUNCHER && !ent->count) {
        // check if the prox mine is outside the owner bbox
        trap_Trace(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, ENTITYNUM_NONE, ent->clipmask);
        if (!tr.startsolid || tr.entityNum != ent->r.ownerNum) {
            ent->count = 1;
        }
    }

    // check think function after bouncing
    G_RunThink(ent);
}

//=============================================================================

/*
=================
CreateMissile
// Address: 0x1005cba0
[QL] Shared missile factory used by fire_grenade / fire_rocket / fire_plasma /
fire_bfg. Spawns the entity, sets ET_MISSILE + SVF_USE_CURRENT_ORIGIN, the
MASK_SHOT clip (minus CONTENTS_BODY under pmove_NoPlayerClip: 0x6000001 ->
0x4000001), snaps origin and velocity, and gives the bolt a tiny +/-0.01 bbox.
trType is TR_GRAVITY unless weapon_gravity_rl is 0 or g_guidedRocket is set.
NOTE: VectorNormalize(dir) happens INSIDE here, so callers must not pre-normalize.
=================
*/
static gentity_t* CreateMissile(vec3_t start, vec3_t dir, int speed, gentity_t* owner) {
    gentity_t* bolt;

    bolt = G_Spawn();
    bolt->think = G_ExplodeMissile;
    bolt->s.eType = ET_MISSILE;
    bolt->s.eFlags = 0;
    bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    bolt->r.ownerNum = owner->s.number;
    bolt->parent = owner;
    bolt->clipmask = MASK_SHOT;  // [QL] SOLID|BODY|CORPSE = 0x6000001
    bolt->target_ent = NULL;

    // [QL] pmove_NoPlayerClip strips CONTENTS_BODY so missiles pass through players
    if (pmove_NoPlayerClip.integer) {
        bolt->clipmask &= ~CONTENTS_BODY;  // 0x6000001 -> 0x4000001
    }

    VectorCopy(start, bolt->s.pos.trBase);
    SnapVector(bolt->s.pos.trBase);
    VectorCopy(bolt->s.pos.trBase, bolt->r.currentOrigin);

    // [QL] weapon_gravity_rl selects gravity; g_guidedRocket forces linear
    if (weapon_gravity_rl.integer == 0 || g_guidedRocket.integer != 0) {
        bolt->s.pos.trType = TR_LINEAR;
    } else {
        bolt->s.pos.trType = TR_GRAVITY;
    }
    bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;  // move a bit on the very first frame

    VectorNormalize(dir);
    VectorScale(dir, speed, bolt->s.pos.trDelta);
    SnapVector(bolt->s.pos.trDelta);  // save net bandwidth

    // small bbox for precise collision
    VectorSet(bolt->r.mins, -0.01f, -0.01f, -0.01f);
    VectorSet(bolt->r.maxs, 0.01f, 0.01f, 0.01f);

    return bolt;
}

//=============================================================================

/*
=================
G_PlasmaAccelThink

[QL] qagamex86.dll 0x1005d020. Every g_accelRate_pg ms the bolt's velocity is multiplied by
g_accelFactor_pg; the trajectory is not rebased, and the bolt no longer times out.
=================
*/
static void G_PlasmaAccelThink(gentity_t* ent) {
    VectorScale(ent->s.pos.trDelta, g_accelFactor_pg.value, ent->s.pos.trDelta);
    ent->nextthink = (int)((double)level.time + g_accelRate_pg.value);
}

/*
=================
fire_plasma
// Address: 0x1005d070
=================
*/
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
    bolt->s.pos.trType = weapon_gravity_pg.integer ? TR_GRAVITY : TR_LINEAR;  // [QL]

    // [QL] 0x1005d10d
    if (g_accelFactor_pg.value != 1.0f) {
        bolt->nextthink = (int)((double)level.time + g_accelRate_pg.value);
        bolt->think = G_PlasmaAccelThink;
    }

    return bolt;
}

//=============================================================================

/*
=================
fire_grenade
// Address: 0x1005cd40
=================
*/
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
    bolt->s.clientNum = self->s.clientNum;  // [QL] binary copies self->s.clientNum

    return bolt;
}

//=============================================================================

/*
=================
G_BFGAccelThink

[QL] qagamex86.dll 0x1005d150, the same as G_PlasmaAccelThink with g_accelFactor_bfg and
g_accelRate_bfg.
=================
*/
static void G_BFGAccelThink(gentity_t* ent) {
    VectorScale(ent->s.pos.trDelta, g_accelFactor_bfg.value, ent->s.pos.trDelta);
    ent->nextthink = (int)((double)level.time + g_accelRate_bfg.value);
}

/*
=================
fire_bfg
// Address: 0x1005d1a0
=================
*/
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
    // [QL] binary does NOT override trType here; BFG keeps CreateMissile's
    //      weapon_gravity_rl-based trajectory (weapon_gravity_bfg is unused).

    // [QL] 0x1005d234
    if (g_accelFactor_bfg.value != 1.0f) {
        bolt->nextthink = (int)((double)level.time + g_accelRate_bfg.value);
        bolt->think = G_BFGAccelThink;
    }

    return bolt;
}

/*
=================
G_GuidedRocketThink
// Address: 0x1005ce30
// .so name: G_GuidedRocketThink (Ghidra mislabels it "Weapon_RocketHomingThink")
[QL] Steers a guided rocket toward the shooter's view direction each frame.
Speed is HARDCODED to 20; trTime is NOT updated; think interval is 25ms.
=================
*/
static void G_GuidedRocketThink(gentity_t *ent) {
    if (ent->parent->client) {
        vec3_t forward;
        AngleVectors(ent->parent->client->ps.viewangles, forward, NULL, NULL);
        VectorCopy(forward, ent->movedir);
        VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
        VectorScale(forward, 20, ent->s.pos.trDelta);
        // [QL] binary does NOT set ent->s.pos.trTime here
    }
    ent->nextthink = level.time + 25;
}

//=============================================================================

/*
=================
G_RocketAccelThink

[QL] qagamex86.dll 0x1005cd70, the same as G_PlasmaAccelThink with g_accelFactor_rl and
g_accelRate_rl.
=================
*/
static void G_RocketAccelThink(gentity_t* ent) {
    VectorScale(ent->s.pos.trDelta, g_accelFactor_rl.value, ent->s.pos.trDelta);
    ent->nextthink = (int)((double)level.time + g_accelRate_rl.value);
}

/*
=================
fire_rocket
// Address: 0x1005cf20
=================
*/
gentity_t* fire_rocket(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* bolt;

    bolt = CreateMissile(start, dir, g_velocity_rl.integer, self);
    bolt->classname = "rocket";
    bolt->nextthink = level.time + 15000;
    bolt->s.weapon = WP_ROCKET_LAUNCHER;
    bolt->r.ownerNum = self->s.number;
    bolt->s.otherEntityNum = self->s.number;
    bolt->damage = g_damage_rl.integer;
    bolt->splashDamage = 100;  // [QL] binary hardcodes 100 (NOT g_splashdamage_rl)
    bolt->splashRadius = g_splashradius_rl.integer;
    bolt->methodOfDeath = MOD_ROCKET;
    bolt->splashMethodOfDeath = MOD_ROCKET_SPLASH;

    // [QL] guided rocket mode: steers from the first frame, nextthink a frame in the past (0x1005cfca)
    if (g_guidedRocket.integer) {
        bolt->nextthink = level.time - (int)(1000.0 / sv_fps.integer);
        bolt->think = G_GuidedRocketThink;
    }

    // [QL] 0x1005cfdc: acceleration replaces the guided steering
    if (g_accelFactor_rl.value != 1.0f) {
        bolt->nextthink = (int)((double)level.time + g_accelRate_rl.value);
        bolt->think = G_RocketAccelThink;
    }

    return bolt;
}

/*
=================
fire_grapple
// Address: 0x1005d2e0
=================
*/
gentity_t* fire_grapple(gentity_t* self, vec3_t start, vec3_t dir) {
    gentity_t* hook;

    VectorNormalize(dir);

    hook = G_Spawn();
    hook->classname = "hook";
    hook->nextthink = level.time + 10000;
    hook->think = Weapon_HookFree;  // [QL] Ghidra label at 0x1006e320 is a guess ("G_ReleaseGrapple"); .so symbol is Weapon_HookFree
    hook->s.eType = ET_MISSILE;
    hook->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    hook->s.weapon = WP_GRAPPLING_HOOK;
    hook->r.ownerNum = self->s.number;
    hook->methodOfDeath = MOD_GRAPPLE;
    hook->clipmask = MASK_SHOT;  // [QL] 0x6000001, minus CONTENTS_BODY under pmove
    if (pmove_NoPlayerClip.integer) {
        hook->clipmask &= ~CONTENTS_BODY;
    }
    hook->parent = self;
    hook->target_ent = NULL;
    hook->bouncecount = 0;
    hook->count = 0;
    hook->s.pos.trType = TR_LINEAR;
    hook->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;  // move a bit on the very first frame
    hook->s.otherEntityNum = self->s.number;                 // use to match beam in client
    hook->damage = g_damage_gh.integer;                      // [QL] binary sets damage from g_damage_gh

    VectorCopy(start, hook->s.pos.trBase);
    VectorScale(dir, g_velocity_gh.integer, hook->s.pos.trDelta);
    SnapVector(hook->s.pos.trDelta);  // save net bandwidth
    VectorCopy(start, hook->r.currentOrigin);

    self->client->hook = hook;

    return hook;
}

/*
=================
fire_nail
[QL] NOTE: qagamex86.dll has NO standalone fire_nail; the nail spawn is inlined
into weapon_nailgun_fire (0x1006da90, g_weapon.c). This Q3-style helper is kept
only because g_weapon.c still calls it. The binary sets the bouncing-nail flag
as EF_QL_NAILBOUNCE (0x100000), not EF_BOUNCE. See report.
=================
*/
gentity_t* fire_nail(gentity_t* self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up) {
    gentity_t* bolt;
    vec3_t dir;
    vec3_t end;
    float r, u, scale;

    bolt = G_Spawn();
    bolt->classname = "nail";
    bolt->nextthink = level.time + 4500;  // [QL] 0x1194 = 4500
    bolt->think = G_ExplodeMissile;
    bolt->s.eType = ET_MISSILE;
    bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    bolt->s.weapon = WP_NAILGUN;
    bolt->r.ownerNum = self->s.number;
    bolt->parent = self;
    bolt->damage = g_damage_ng.integer;
    bolt->methodOfDeath = MOD_NAIL;
    bolt->clipmask = pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT;  //
    bolt->target_ent = NULL;
    bolt->s.otherEntityNum = self->s.number;  // [QL]

    bolt->s.pos.trType = weapon_gravity_ng.integer ? TR_GRAVITY : TR_LINEAR;  // [QL]
    bolt->s.pos.trTime = level.time;
    VectorCopy(start, bolt->s.pos.trBase);

    // [QL] nail bounce
    if (g_nailbounce.integer) {
        if ((rand() % 100) >= (100 - g_nailbouncepercentage.integer)) {
            bolt->s.eFlags = EF_BOUNCE;
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

/*
=================
fire_prox
// Address: 0x1005d480
=================
*/
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
    bolt->s.otherEntityNum = self->s.number;  // [QL] binary sets s.otherEntityNum (0x94)
    bolt->parent = self;
    bolt->damage = g_damage_pl.integer;
    bolt->splashDamage = g_splashdamage_pl.integer;
    bolt->splashRadius = g_splashradius_pl.integer;
    bolt->methodOfDeath = MOD_PROXIMITY_MINE;
    bolt->splashMethodOfDeath = MOD_PROXIMITY_MINE;
    bolt->clipmask = MASK_SHOT;  // [QL] 0x6000001, minus CONTENTS_BODY under pmove
    if (pmove_NoPlayerClip.integer) {
        bolt->clipmask &= ~CONTENTS_BODY;
    }
    bolt->target_ent = NULL;
    // count is used to check if the prox mine left the player bbox
    // if count == 1 then the prox mine left the player bbox and can attack to it
    bolt->count = 0;

    // [QL] team stored in s.generic1 (0xE0); ProximityMine_Trigger reads it back
    bolt->s.generic1 = self->client->sess.sessionTeam;

    bolt->s.pos.trType = TR_GRAVITY;
    bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;  // move a bit on the very first frame
    VectorCopy(start, bolt->s.pos.trBase);
    VectorScale(dir, 700, bolt->s.pos.trDelta);
    SnapVector(bolt->s.pos.trDelta);  // save net bandwidth

    VectorCopy(start, bolt->r.currentOrigin);

    return bolt;
}
