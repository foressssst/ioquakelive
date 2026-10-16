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

static float s_quadFactor;
static vec3_t forward, right, up;
static vec3_t muzzle;

void G_BounceProjectile(vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout) {
    vec3_t v, newv;
    float dot;

    VectorSubtract(impact, start, v);
    dot = DotProduct(v, dir);
    VectorMA(v, -2 * dot, dir, newv);

    VectorNormalize(newv);
    VectorMA(impact, 8192, newv, endout);
}

void Weapon_Gauntlet(gentity_t* ent) {
    gentity_t* traceEnt = ent->target_ent;
    gentity_t* tent;
    vec3_t dir;

    if (!traceEnt) {
        return;
    }

    VectorSubtract(ent->s.pos.trBase, traceEnt->s.pos.trBase, dir);

    if (traceEnt->takedamage && traceEnt->client) {
        tent = G_TempEntity(traceEnt->s.pos.trBase, EV_MISSILE_HIT);
        tent->s.otherEntityNum = traceEnt->s.number;
        tent->s.eventParm = DirToByte(dir);
        tent->s.weapon = ent->s.weapon;
    }

    G_Damage(traceEnt, ent, ent, forward, dir,
             (int)(s_quadFactor * g_damage_g.value), 0, MOD_GAUNTLET);
}

qboolean CheckGauntletAttack(gentity_t* ent) {
    trace_t tr;
    vec3_t end;
    gentity_t* traceEnt;

    AngleVectors(ent->client->ps.viewangles, forward, right, up);

    CalcMuzzlePoint(ent, forward, right, up, muzzle);

    VectorMA(muzzle, 43, forward, end);

    trap_Trace(&tr, muzzle, NULL, NULL, end, ent->s.number,
               pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT);
    if (tr.surfaceFlags & SURF_NOIMPACT) {
        return qfalse;
    }

    traceEnt = &g_entities[tr.entityNum];
    if (!traceEnt->takedamage) {
        ent->target_ent = NULL;
        return qfalse;
    }

    ent->target_ent = traceEnt;
    return qtrue;
}

void SnapVectorTowards(vec3_t v, vec3_t to) {
    int i;

    for (i = 0; i < 3; i++) {
        if (to[i] <= v[i]) {
            v[i] = floor(v[i]);
        } else {
            v[i] = ceil(v[i]);
        }
    }
}

#define CHAINGUN_SPREAD 700
#define CHAINGUN_MAX_SPREAD 1400
#define CHAINGUN_DAMAGE 7

#define HMG_SPREAD  350
#define HMG_DAMAGE  8

#define MACHINEGUN_SPREAD 200
#define MACHINEGUN_DAMAGE 7
#define MACHINEGUN_TEAM_DAMAGE 5

void Bullet_Fire(gentity_t* ent, float spread, int damage, int mod) {
    trace_t tr;
    vec3_t end;
    vec3_t impactpoint, bouncedir;
    float r;
    float u;
    gentity_t* tent;
    gentity_t* traceEnt;
    int i, passent;

    damage *= s_quadFactor;

    r = random() * M_PI * 2.0f;
    u = sin(r) * crandom() * spread * 16;
    r = cos(r) * crandom() * spread * 16;
    VectorMA(muzzle, 8192 * 16, forward, end);
    VectorMA(end, r, right, end);
    VectorMA(end, u, up, end);

    passent = ent->s.number;
    for (i = 0; i < 2; i++) {
        {
            int mask = pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT;
            G_TracePlayerHit(&tr, muzzle, NULL, NULL, end, passent, mask);
        }
        if (tr.surfaceFlags & SURF_NOIMPACT) {
            return;
        }

        traceEnt = &g_entities[tr.entityNum];

        SnapVectorTowards(tr.endpos, muzzle);

        if (traceEnt->takedamage && traceEnt->client) {
            tent = G_TempEntity(tr.endpos, EV_BULLET_HIT_FLESH);
            tent->s.eventParm = traceEnt->s.number;
        } else {
            tent = G_TempEntity(tr.endpos, EV_BULLET_HIT_WALL);
            tent->s.eventParm = DirToByte(tr.plane.normal);
        }
        tent->s.otherEntityNum = ent->s.number;

        if (traceEnt->takedamage) {
            if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
                if (G_InvulnerabilityEffect(traceEnt, forward, tr.endpos, impactpoint, bouncedir)) {
                    G_BounceProjectile(muzzle, impactpoint, bouncedir, end);
                    VectorCopy(impactpoint, muzzle);
                    passent = ENTITYNUM_NONE;
                } else {
                    VectorCopy(tr.endpos, muzzle);
                    passent = traceEnt->s.number;
                }
                continue;
            } else {
                if (LogAccuracyHit(traceEnt, ent)) {
                    ent->client->accuracy_hits++;
                    if (ent->s.weapon < 16) {
                        ent->client->expandedStats.shotsHit[ent->s.weapon]++;
                    }
                    if (g_gametype.integer != GT_RACE) {
                        STAT_AddHits(ent, ent->s.weapon, 1);
                    }
                }
                G_Damage(traceEnt, ent, ent, forward, tr.endpos,
                         damage, 0, mod);
            }
        }
        break;
    }
}

void Weapon_BFG_Fire(gentity_t* ent) {
    gentity_t* m;

    m = fire_bfg(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

}

#define DEFAULT_SHOTGUN_DAMAGE 10

int ShotgunPellet(vec3_t start, vec3_t end, gentity_t* ent, int ring) {
    trace_t tr;
    int damage, i, passent;
    gentity_t* traceEnt;
    vec3_t impactpoint, bouncedir;
    vec3_t tr_start, tr_end;

    passent = ent->s.number;
    VectorCopy(start, tr_start);
    VectorCopy(end, tr_end);
    for (i = 0; i < 2; i++) {
        {
            int mask = pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT;
            G_TracePlayerHit(&tr, tr_start, NULL, NULL, tr_end, passent, mask);
        }
        traceEnt = &g_entities[tr.entityNum];

        if (tr.surfaceFlags & SURF_NOIMPACT) {
            return 0;
        }

        if (!traceEnt->takedamage) {
            return 0;
        }

        if (ring == 1) {
            damage = g_damage_sg.integer;
        } else {
            damage = g_damage_sg_outer.integer;
        }

        damage = (int)(damage * s_quadFactor);

        if (g_damage_sg_falloff.integer && g_range_sg_falloff.integer > 0) {
            float dist = Distance(start, tr.endpos);
            int falloffRange = (int)dist;
            while (falloffRange > 0 && g_range_sg_falloff.integer != 0) {
                falloffRange -= g_range_sg_falloff.integer;
                damage -= g_damage_sg_falloff.integer;
            }
            if (damage < 1) damage = 1;
        }

        if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
            if (G_InvulnerabilityEffect(traceEnt, forward, tr.endpos, impactpoint, bouncedir)) {
                G_BounceProjectile(tr_start, impactpoint, bouncedir, tr_end);
                VectorCopy(impactpoint, tr_start);
                passent = ENTITYNUM_NONE;
            } else {
                VectorCopy(tr.endpos, tr_start);
                passent = traceEnt->s.number;
            }
            continue;
        }

        G_Damage(traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_SHOTGUN);
        if (LogAccuracyHit(traceEnt, ent)) {
            return damage;
        }
        return 0;
    }
    return 0;
}

void ShotgunPattern(vec3_t origin, vec3_t origin2, int seed, gentity_t* ent) {
    int i;
    float r, u, angle, ringRadius;
    int ring;
    int totalDamage = 0;
    int quality;
    vec3_t end;
    vec3_t forward, right, up;
    qboolean hitClient = qfalse;

    VectorSubtract(origin2, origin, forward);
    VectorNormalize(forward);
    PerpendicularVector(right, forward);
    CrossProduct(forward, right, up);

    if (ent->client) {
        memset(ent->client->damagePlum, 0, sizeof(ent->client->damagePlum));
    }

    for (i = 0; i < DEFAULT_SHOTGUN_COUNT; i++) {
        if (i < 6) {
            ringRadius = 4000;
            angle = (float)(i - 20) * (M_PI / 3.0f);
            ring = 1;
        } else if (i < 12) {
            ringRadius = 8000;
            angle = (float)i * (M_PI / 3.0f) + 30.0f;
            ring = 0;
        } else {
            ringRadius = 12000;
            angle = (float)i * (M_PI / 4.0f);
            ring = 0;
        }

        r = cos(angle) * ringRadius;
        u = sin(angle) * ringRadius;

        VectorMA(origin, 8192 * 16, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        {
            int pelletDamage = ShotgunPellet(origin, end, ent, ring);
            totalDamage += pelletDamage;
            if (pelletDamage) {
                if (!hitClient) {
                    hitClient = qtrue;
                    ent->client->accuracy_hits++;
                }
                ent->client->expandedStats.shotsHit[WP_SHOTGUN]++;
                if (g_gametype.integer != GT_RACE) {
                    STAT_AddHits(ent, WP_SHOTGUN, 1);
                }
            }
        }
    }

    if (totalDamage >= 75) quality = 3;
    else if (totalDamage >= 50) quality = 2;
    else if (totalDamage > 24) quality = 1;
    else quality = 0;
    if (ent->client) {
        ent->client->ps.generic1 = (ent->client->ps.generic1 & 0x3f) | (quality << 6);
    }

    if (g_damagePlums.integer && ent->client) {
        int j;
        for (j = 0; j < level.maxclients; j++) {
            if (ent->client->damagePlum[j] != 0 && g_entities[j].inuse && g_entities[j].client) {
                DamagePlum(ent, &g_entities[j], ent->client->damagePlum[j], MOD_SHOTGUN);
            }
            ent->client->damagePlum[j] = 0;
        }
    }
}

void weapon_supershotgun_fire(gentity_t* ent) {
    gentity_t* tent;

    tent = G_TempEntity(muzzle, EV_SHOTGUN);
    VectorMA(muzzle, 4096, forward, tent->s.origin2);
    SnapVector(tent->s.origin2);
    tent->s.eventParm = rand() & 255;
    tent->s.otherEntityNum = ent->s.number;

    ShotgunPattern(tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent);
}

void Weapon_GrenadeLauncher_Fire(gentity_t* ent) {
    gentity_t* m;

    forward[2] += 0.2f;
    VectorNormalize(forward);

    m = fire_grenade(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

}

void Weapon_RocketLauncher_Fire(gentity_t* ent) {
    gentity_t* m;

    m = fire_rocket(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;
    m->damageFactor = (int)s_quadFactor;

}

void Weapon_Plasmagun_Fire(gentity_t* ent) {
    gentity_t* m;

    m = fire_plasma(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

}

#define MAX_RAIL_HITS 4
void weapon_railgun_fire(gentity_t* ent) {
    vec3_t end;
    vec3_t impactpoint, bouncedir;
    trace_t trace;
    gentity_t* tent;
    gentity_t* traceEnt;
    int damage;
    int i;
    int hits;
    int unlinked;
    int passent;
    gentity_t* unlinkedEntities[MAX_RAIL_HITS];

    damage = g_damage_rg.integer * s_quadFactor;

    VectorMA(muzzle, 8192, forward, end);

    if (g_railJump.integer) {
        trace_t jumpTr;
        vec3_t jumpEnd, kickDir;

        VectorMA(muzzle, 120, forward, jumpEnd);
        trap_Trace(&jumpTr, muzzle, NULL, NULL, jumpEnd, ent->s.number, CONTENTS_SOLID);
        if (jumpTr.fraction != 1.0f && ent->client) {
            VectorCopy(end, kickDir);
            VectorNormalize(kickDir);
            ent->client->ps.velocity[0] -= (float)g_railJump.integer * kickDir[0];
            ent->client->ps.velocity[1] -= (float)g_railJump.integer * kickDir[1];
            ent->client->ps.velocity[2] -= (float)g_railJump.integer * kickDir[2];
            ent->client->ps.velocity[2] += 20.0f;
        }
    }

    unlinked = 0;
    hits = 0;
    passent = ent->s.number;
    do {
        {
            int mask = pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT;
            G_TracePlayerHit(&trace, muzzle, NULL, NULL, end, passent, mask);
        }
        if (trace.entityNum >= ENTITYNUM_MAX_NORMAL) {
            break;
        }
        traceEnt = &g_entities[trace.entityNum];
        if (traceEnt->takedamage) {
            if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
                if (G_InvulnerabilityEffect(traceEnt, forward, trace.endpos, impactpoint, bouncedir)) {
                    G_BounceProjectile(muzzle, impactpoint, bouncedir, end);
                    SnapVectorTowards(trace.endpos, muzzle);
                    tent = G_TempEntity(trace.endpos, EV_RAILTRAIL);
                    tent->s.clientNum = ent->s.clientNum;
                    VectorCopy(muzzle, tent->s.origin2);
                    VectorMA(tent->s.origin2, 4, right, tent->s.origin2);
                    VectorMA(tent->s.origin2, -1, up, tent->s.origin2);
                    tent->s.eventParm = 255;
                    VectorCopy(impactpoint, muzzle);
                    passent = ENTITYNUM_NONE;
                }
            } else {
                if (LogAccuracyHit(traceEnt, ent)) {
                    hits++;
                }
                tent = G_TempEntity(trace.endpos, EV_MISSILE_HIT);
                tent->s.otherEntityNum = traceEnt->s.number;
                tent->s.weapon = ent->s.weapon;
                {
                    int rgDamage = damage;
                    int rgMod = MOD_RAILGUN;
                    if (trace.surfaceFlags & 0x400) {
                        rgDamage += g_headShotDamage_rg.integer;
                        rgMod = MOD_RAILGUN_HEADSHOT;
                    }
                    G_Damage(traceEnt, ent, ent, forward, trace.endpos, rgDamage, 0, rgMod);
                }
            }
        }
        if (trace.contents & CONTENTS_SOLID) {
            break;
        }
        trap_UnlinkEntity(traceEnt);
        unlinkedEntities[unlinked] = traceEnt;
        unlinked++;
    } while (unlinked < MAX_RAIL_HITS);

    for (i = 0; i < unlinked; i++) {
        trap_LinkEntity(unlinkedEntities[i]);
    }

    SnapVectorTowards(trace.endpos, muzzle);

    tent = G_TempEntity(trace.endpos, EV_RAILTRAIL);

    tent->s.clientNum = ent->s.clientNum;

    VectorCopy(muzzle, tent->s.origin2);
    VectorMA(tent->s.origin2, 4, right, tent->s.origin2);
    VectorMA(tent->s.origin2, -1, up, tent->s.origin2);

    if (trace.surfaceFlags & SURF_NOIMPACT) {
        tent->s.eventParm = 255;
    } else {
        tent->s.eventParm = DirToByte(trace.plane.normal);
    }
    tent->s.clientNum = ent->s.clientNum;

    if (hits == 0) {
        ent->client->accurateCount = 0;
    } else {
        ent->client->accurateCount += hits;
        if (ent->client->accurateCount >= 2) {
            ent->client->accurateCount -= 2;
            ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
            ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP);
            ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
            ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
            STAT_AddPlayerMedalStat(ent, "IMPRESSIVE");
        }
        ent->client->accuracy_hits++;
        ent->client->expandedStats.shotsHit[WP_RAILGUN]++;
        if (g_gametype.integer != GT_RACE) {
            STAT_AddHits(ent, WP_RAILGUN, 1);
        }
    }
}

void Weapon_GrapplingHook_Fire(gentity_t* ent) {
    if (!ent->client->fireHeld && !ent->client->hook) {
        gentity_t* hook = fire_grapple(ent, muzzle, forward);
        hook->damage *= s_quadFactor;
    }

    ent->client->fireHeld = qtrue;
}

void Weapon_HookFree(gentity_t* ent) {
    if (!ent->parent || !ent->parent->client) {
        return;
    }
    ent->parent->client->hook = NULL;
    VectorClear(ent->parent->client->ps.grapplePoint);
    ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
    ent->parent->client->ps.pm_flags |= PMF_TIME_GRAPPLE;
    ent->parent->client->ps.eFlags &= ~EF_FIRING;
    if (ent->enemy && ent->enemy->client) {
        ent->enemy->client->ps.pm_flags &= ~PMF_HOOKED;
    }
    G_FreeEntity(ent);
}

void Weapon_HookThink(gentity_t* ent) {
    gentity_t* owner = ent->parent;
    gentity_t* enemy;

    if (!owner || !owner->client) {
        return;
    }

    if (!ent->enemy || ent->enemy->s.eType != ET_MOVER) {
        vec3_t start, dir, end;
        trace_t tr;

        VectorCopy(owner->client->ps.origin, start);
        start[2] += (owner->client->ps.pm_flags & PMF_DUCKED) ? 12 : 26;
        VectorSubtract(ent->r.currentOrigin, start, dir);
        VectorNormalize(dir);
        VectorMA(ent->r.currentOrigin, g_latchedHookOffset.value, dir, end);
        trap_Trace(&tr, start, vec3_origin, vec3_origin, end, owner->s.number, CONTENTS_SOLID);
        if (tr.fraction != 1.0f) {
            ent->bouncecount++;
        }
        if (ent->bouncecount >= 3) {
            gentity_t* tent = G_TempEntity(tr.endpos, EV_GENERAL_SOUND);
            tent->r.svFlags |= SVF_BROADCAST;
            tent->s.eventParm = G_SoundIndex("sound/weapons/grapple/grreset.wav");
            Weapon_HookFree(ent);
            return;
        }
    }

    if (owner->client->ps.weaponstate == WEAPON_DROPPING || owner->client->ps.weapon != WP_GRAPPLING_HOOK) {
        Weapon_HookFree(ent);
        return;
    }

    enemy = ent->enemy;
    if (enemy) {
        vec3_t v, oldorigin;

        if (enemy->s.eType == ET_PLAYER && enemy->health <= 0 && enemy->client->ps.pm_type != PM_FREEZE) {
            Weapon_HookFree(ent);
            return;
        }
        if (enemy->s.eType == ET_PLAYER && OnSameTeam(enemy, owner) && enemy->health > 0) {
            Weapon_HookFree(ent);
            return;
        }
        if (enemy->s.eType == ET_MOVER && ent->count) {
            Weapon_HookFree(ent);
            return;
        }

        VectorCopy(ent->r.currentOrigin, oldorigin);
        v[0] = enemy->r.currentOrigin[0] + (enemy->r.mins[0] + enemy->r.maxs[0]) * 0.5;
        v[1] = enemy->r.currentOrigin[1] + (enemy->r.mins[1] + enemy->r.maxs[1]) * 0.5;
        v[2] = enemy->r.currentOrigin[2] + (enemy->r.mins[2] + enemy->r.maxs[2]) * 0.5;
        SnapVectorTowards(v, oldorigin);
        G_SetOrigin(ent, v);

        if (enemy->client && enemy->client->hook) {
            Weapon_HookFree(enemy->client->hook);
        }

        if (level.time > ent->wait) {
            ent->wait = level.time + weapon_reload_hook.integer;
            G_Damage(enemy, ent, owner, forward, NULL, (int)(g_damage_gh.integer * s_quadFactor), 0, MOD_GRAPPLE);
        }
    }

    VectorCopy(ent->r.currentOrigin, owner->client->ps.grapplePoint);
}

void Weapon_LightningFire(gentity_t* ent) {
    trace_t tr;
    vec3_t end;
    vec3_t impactpoint, bouncedir;
    gentity_t *traceEnt, *tent;
    int damage, i, passent;

    damage = g_damage_lg.integer;

    passent = ent->s.number;
    for (i = 0; i < 2; i++) {
        if (g_lightningDischarge.integer && (trap_PointContents(muzzle, -1) & MASK_WATER)) {
            int cells = ent->client->ps.ammo[WP_LIGHTNING];
            if (cells != 0) {
                int points;

                if (!g_infiniteAmmo.integer && cells != -1) {
                    cells++;
                } else {
                    cells = 150;
                }
                SnapVectorTowards(muzzle, ent->s.origin);
                tent = G_TempEntity(muzzle, EV_LIGHTNING_DISCHARGE);
                tent->s.eventParm = cells;
                ent->client->ps.ammo[WP_LIGHTNING] = 0;

                points = cells * g_damage_lg.integer;
                if (G_WaterRadiusDamage(muzzle, ent, (float)points, (float)(points + 16))) {
                    ent->client->accuracy_hits++;
                    ent->client->expandedStats.shotsHit[ent->s.weapon]++;
                    if (g_gametype.integer != GT_RACE) {
                        STAT_AddHits(ent, ent->s.weapon, 1);
                    }
                    if (BG_IsRoundBasedGameType(g_gametype.integer)) {
                        ent->client->round_hits++;
                    }
                }
            }
            break;
        }

        VectorMA(muzzle, LIGHTNING_RANGE, forward, end);

        {
            int mask = pmove_NoPlayerClip.integer ? CONTENTS_SOLID : MASK_SHOT;
            G_TracePlayerHit(&tr, muzzle, NULL, NULL, end, passent, mask);
        }

        if (i) {
            tent = G_TempEntity(muzzle, EV_LIGHTNINGBOLT);
            VectorCopy(tr.endpos, end);
            SnapVector(end);
            VectorCopy(end, tent->s.origin2);
        }

        if (tr.entityNum == ENTITYNUM_NONE) {
            return;
        }

        traceEnt = &g_entities[tr.entityNum];

        if (traceEnt->takedamage) {
            if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
                if (G_InvulnerabilityEffect(traceEnt, forward, tr.endpos, impactpoint, bouncedir)) {
                    G_BounceProjectile(muzzle, impactpoint, bouncedir, end);
                    VectorCopy(impactpoint, muzzle);
                    VectorSubtract(end, impactpoint, forward);
                    VectorNormalize(forward);
                    passent = ENTITYNUM_NONE;
                } else {
                    VectorCopy(tr.endpos, muzzle);
                    passent = traceEnt->s.number;
                }
                continue;
            }
            if (LogAccuracyHit(traceEnt, ent)) {
                ent->client->accuracy_hits++;
                ent->client->expandedStats.shotsHit[WP_LIGHTNING]++;
                if (g_gametype.integer != GT_RACE) {
                    STAT_AddHits(ent, WP_LIGHTNING, 1);
                }
            }
            {
                int actualDamage = damage;
                if (g_damage_lg_falloff.integer && g_range_lg_falloff.integer > 0) {
                    int falloffRange = (int)Distance(muzzle, tr.endpos);
                    while (falloffRange > 0 && g_range_lg_falloff.integer != 0) {
                        falloffRange -= g_range_lg_falloff.integer;
                        actualDamage -= g_damage_lg_falloff.integer;
                    }
                    if (actualDamage < 1) actualDamage = 1;
                }
                G_Damage(traceEnt, ent, ent, forward, tr.endpos,
                         (int)(actualDamage * s_quadFactor), 0, MOD_LIGHTNING);
            }
        }

        if (traceEnt->takedamage && traceEnt->client) {
            tent = G_TempEntity(tr.endpos, EV_MISSILE_HIT);
            tent->s.otherEntityNum = traceEnt->s.number;
            tent->s.eventParm = DirToByte(tr.plane.normal);
            tent->s.weapon = ent->s.weapon;
        } else if (!(tr.surfaceFlags & SURF_NOIMPACT)) {
            tent = G_TempEntity(tr.endpos, EV_MISSILE_MISS);
            tent->s.eventParm = DirToByte(tr.plane.normal);
        }

        break;
    }
}

void Weapon_Nailgun_Fire(gentity_t* ent) {
    gentity_t* m;
    int count;

    for (count = 0; count < g_nailcount.integer; count++) {
        m = fire_nail(ent, muzzle, forward, right, up);
        m->damage *= s_quadFactor;
        m->splashDamage *= s_quadFactor;
    }

}

void weapon_proxlauncher_fire(gentity_t* ent) {
    gentity_t* m;

    forward[2] += 0.2f;
    VectorNormalize(forward);

    m = fire_prox(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

}

qboolean LogAccuracyHit(gentity_t* target, gentity_t* attacker) {
    if (!target->takedamage) {
        return qfalse;
    }

    if (target == attacker) {
        return qfalse;
    }

    if (!target->client) {
        return qfalse;
    }

    if (!attacker->client) {
        return qfalse;
    }

    if (OnSameTeam(target, attacker)) {
        return qfalse;
    }

    if (FreezeTagInGame() && target->client->ps.powerups[PW_FREEZE]) {
        return qfalse;
    }

    return qtrue;
}

void CalcMuzzlePoint(gentity_t* ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint) {
    float offset = (ent->client->ps.pm_flags & PMF_DUCKED) ? 3.0f : 5.0f;
    VectorCopy(ent->s.pos.trBase, muzzlePoint);
    muzzlePoint[2] += ent->client->ps.viewheight;
    VectorMA(muzzlePoint, offset, forward, muzzlePoint);
}

void CalcMuzzlePointOrigin(gentity_t* ent, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint) {
    float offset = (ent->client->ps.pm_flags & PMF_DUCKED) ? 3.0f : 5.0f;
    VectorCopy(ent->s.pos.trBase, muzzlePoint);
    muzzlePoint[2] += ent->client->ps.viewheight;
    VectorMA(muzzlePoint, offset, forward, muzzlePoint);
}

void FireWeapon(gentity_t* ent) {
    if (ent->client->ps.powerups[PW_QUAD]) {
        s_quadFactor = g_quadDamageFactor.value;
    } else if (ent->client->ps.stats[STAT_RUNE] == 3) {
        s_quadFactor = 1.5f;
    } else {
        s_quadFactor = 1.0f;
    }

    if (ent->s.weapon != WP_GRAPPLING_HOOK && ent->s.weapon != WP_GAUNTLET
        && ent->s.weapon != WP_NUM_WEAPONS) {
        int shotsFired;
        if (ent->s.weapon == WP_SHOTGUN) {
            shotsFired = DEFAULT_SHOTGUN_COUNT;
            ent->client->accuracy_shots++;
        } else if (ent->s.weapon == WP_NAILGUN) {
            shotsFired = g_nailcount.integer;
            ent->client->accuracy_shots += g_nailcount.integer;
        } else {
            shotsFired = 1;
            ent->client->accuracy_shots++;
        }
        if (ent->s.weapon < 16) {
            ent->client->expandedStats.shotsFired[ent->s.weapon] += shotsFired;
        }
        if (g_gametype.integer != GT_RACE) {
            STAT_AddShots(ent, ent->s.weapon, shotsFired);
        }
        if (BG_IsRoundBasedGameType(g_gametype.integer)) {
            ent->client->round_shots += shotsFired;
        }
    }

    AngleVectors(ent->client->ps.viewangles, forward, right, up);

    CalcMuzzlePointOrigin(ent, ent->client->ps.origin, forward, right, up, muzzle);

    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        if ((0x60cc >> (ent->s.weapon & 0x1f)) & 1) {
            HAX_Begin(ent, ent->client->ps.commandTime);
        }
    }

    switch (ent->s.weapon) {
        case WP_GAUNTLET:
            Weapon_Gauntlet(ent);
            break;
        case WP_LIGHTNING:
            Weapon_LightningFire(ent);
            break;
        case WP_SHOTGUN:
            weapon_supershotgun_fire(ent);
            break;
        case WP_MACHINEGUN: {
            float scale = (ent->client->ps.pm_flags & PMF_DUCKED) ? g_ironsights_mg.value : 1.0f;

            Bullet_Fire(ent, (float)(int)(scale * 150.0), g_damage_mg.integer, MOD_MACHINEGUN);
            break;
        }
        case WP_GRENADE_LAUNCHER:
            Weapon_GrenadeLauncher_Fire(ent);
            break;
        case WP_ROCKET_LAUNCHER:
            Weapon_RocketLauncher_Fire(ent);
            break;
        case WP_PLASMAGUN:
            Weapon_Plasmagun_Fire(ent);
            break;
        case WP_RAILGUN:
            weapon_railgun_fire(ent);
            break;
        case WP_BFG:
            Weapon_BFG_Fire(ent);
            break;
        case WP_GRAPPLING_HOOK:
            Weapon_GrapplingHook_Fire(ent);
            break;
        case WP_NAILGUN:
            Weapon_Nailgun_Fire(ent);
            break;
        case WP_PROX_LAUNCHER:
            weapon_proxlauncher_fire(ent);
            break;
        case WP_CHAINGUN: {
            float spin = 1.0 - (1000.0 - ent->client->ps.stats[STAT_SPINUP]) / 1000.0;
            int spread = (int)(spin * CHAINGUN_SPREAD + CHAINGUN_SPREAD);

            if (spread < 0) {
                spread = 0;
            } else if (spread > CHAINGUN_MAX_SPREAD) {
                spread = CHAINGUN_MAX_SPREAD;
            }
            Bullet_Fire(ent, spread, g_damage_cg.integer, MOD_CHAINGUN);
            break;
        }
        case WP_HMG:
            Bullet_Fire(ent, HMG_SPREAD, g_damage_hmg.integer, MOD_HMG);
            break;
        default:
            break;
    }

    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        if (((0x60cc >> (ent->s.weapon & 0x1f)) & 1) && !(ent->r.svFlags & SVF_BOT)) {
            HAX_End(ent);
        }
    }
}

static void KamikazeRadiusDamage(vec3_t origin, gentity_t* attacker, float damage, float radius) {
    float dist;
    gentity_t* ent;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t v;
    vec3_t dir;
    int i, e;

    if (radius < 1) {
        radius = 1;
    }

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        if (!ent->takedamage) {
            continue;
        }

        if (ent->kamikazeTime > level.time) {
            continue;
        }

        for (i = 0; i < 3; i++) {
            if (origin[i] < ent->r.absmin[i]) {
                v[i] = ent->r.absmin[i] - origin[i];
            } else if (origin[i] > ent->r.absmax[i]) {
                v[i] = origin[i] - ent->r.absmax[i];
            } else {
                v[i] = 0;
            }
        }

        dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }

        VectorSubtract(ent->r.currentOrigin, origin, dir);
        dir[2] += 24;
        G_Damage(ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS | DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE);
        ent->kamikazeTime = level.time + 3000;
    }
}

static void KamikazeShockWave(vec3_t origin, gentity_t* attacker, float damage, float push, float radius) {
    float dist;
    gentity_t* ent;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t v;
    vec3_t dir;
    int i, e;

    if (radius < 1)
        radius = 1;

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        if (ent->kamikazeShockTime > level.time) {
            continue;
        }

        for (i = 0; i < 3; i++) {
            if (origin[i] < ent->r.absmin[i]) {
                v[i] = ent->r.absmin[i] - origin[i];
            } else if (origin[i] > ent->r.absmax[i]) {
                v[i] = origin[i] - ent->r.absmax[i];
            } else {
                v[i] = 0;
            }
        }

        dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }

        VectorSubtract(ent->r.currentOrigin, origin, dir);
        dir[2] += 24;
        G_Damage(ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS | DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE);
        dir[2] = 0;
        VectorNormalize(dir);
        if (ent->client) {
            ent->client->ps.velocity[0] = dir[0] * push;
            ent->client->ps.velocity[1] = dir[1] * push;
            ent->client->ps.velocity[2] = 100;
        }
        ent->kamikazeShockTime = level.time + 3000;
    }
}

static void KamikazeDamage(gentity_t* self) {
    int i;
    float t;
    gentity_t* ent;
    vec3_t newangles;

    self->count += 100;

    if (self->count >= KAMI_SHOCKWAVE_STARTTIME) {
        t = self->count - KAMI_SHOCKWAVE_STARTTIME;
        KamikazeShockWave(self->s.pos.trBase, self->activator, 25, 400, (int)(float)t * KAMI_SHOCKWAVE_MAXRADIUS / (KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME));
    }
    if (self->count >= KAMI_EXPLODE_STARTTIME) {
        t = self->count - KAMI_EXPLODE_STARTTIME;
        KamikazeRadiusDamage(self->s.pos.trBase, self->activator, 400, (int)(float)t * KAMI_BOOMSPHERE_MAXRADIUS / (KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME));
    }

    if (self->count >= KAMI_SHOCKWAVE_ENDTIME) {
        G_FreeEntity(self);
        return;
    }
    self->nextthink = level.time + 100;

    newangles[0] = crandom() * 2;
    newangles[1] = crandom() * 2;
    newangles[2] = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        ent = &g_entities[i];
        if (!ent->inuse)
            continue;
        if (!ent->client)
            continue;

        if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
            ent->client->ps.velocity[0] += crandom() * 120;
            ent->client->ps.velocity[1] += crandom() * 120;
            ent->client->ps.velocity[2] = 30 + random() * 25;
        }

        ent->client->ps.delta_angles[0] += ANGLE2SHORT(newangles[0] - self->movedir[0]);
        ent->client->ps.delta_angles[1] += ANGLE2SHORT(newangles[1] - self->movedir[1]);
        ent->client->ps.delta_angles[2] += ANGLE2SHORT(newangles[2] - self->movedir[2]);
    }
    VectorCopy(newangles, self->movedir);
}

void G_StartKamikaze(gentity_t* ent) {
    gentity_t* explosion;
    gentity_t* te;
    vec3_t snapped;

    explosion = G_Spawn();

    explosion->s.eType = ET_EVENTS + EV_KAMIKAZE;
    explosion->eventTime = level.time;

    if (ent->activator && ent->activator->client &&
        ent->activator->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }

    if (ent->client) {
        VectorCopy(ent->s.pos.trBase, snapped);
    } else {
        VectorCopy(ent->activator->s.pos.trBase, snapped);
    }
    SnapVector(snapped);
    G_SetOrigin(explosion, snapped);

    explosion->classname = "kamikaze";
    explosion->s.pos.trType = TR_STATIONARY;

    explosion->kamikazeTime = level.time;

    explosion->think = KamikazeDamage;
    explosion->nextthink = level.time + 100;
    explosion->count = 0;
    VectorClear(explosion->movedir);

    trap_LinkEntity(explosion);

    if (ent->client) {
        explosion->activator = ent;
        ent->s.eFlags &= ~EF_KAMIKAZE;
        G_Damage(ent, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_KAMIKAZE);
    } else {
        if (!strcmp(ent->activator->classname, "bodyque")) {
            explosion->activator = &g_entities[ent->activator->r.ownerNum];
        } else {
            explosion->activator = ent->activator;
        }
    }

    te = G_TempEntity(snapped, EV_GLOBAL_TEAM_SOUND);
    te->r.svFlags |= SVF_BROADCAST;
    te->s.eventParm = GTS_KAMIKAZE;
}