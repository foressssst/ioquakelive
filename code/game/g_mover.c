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

typedef struct {
    gentity_t* ent;
    vec3_t origin;
    vec3_t angles;
    float deltayaw;
} pushed_t;
pushed_t pushed[MAX_GENTITIES], *pushed_p;

gentity_t* G_TestEntityPosition(gentity_t* ent) {
    trace_t tr;
    int mask;

    if (ent->clipmask) {
        mask = ent->clipmask;
    } else {
        mask = MASK_SOLID;
    }
    if (ent->client) {
        trap_Trace(&tr, ent->client->ps.origin, ent->r.mins, ent->r.maxs, ent->client->ps.origin, ent->s.number, mask);
    } else {
        trap_Trace(&tr, ent->s.pos.trBase, ent->r.mins, ent->r.maxs, ent->s.pos.trBase, ent->s.number, mask);
    }

    if (tr.startsolid)
        return &g_entities[tr.entityNum];

    return NULL;
}

void G_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3]) {
    AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
    VectorInverse(matrix[1]);
}

void G_TransposeMatrix(vec3_t matrix[3], vec3_t transpose[3]) {
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            transpose[i][j] = matrix[j][i];
        }
    }
}

void G_RotatePoint(vec3_t point, vec3_t matrix[3]) {
    vec3_t tvec;

    VectorCopy(point, tvec);
    point[0] = DotProduct(matrix[0], tvec);
    point[1] = DotProduct(matrix[1], tvec);
    point[2] = DotProduct(matrix[2], tvec);
}

qboolean G_TryPushingEntity(gentity_t* check, gentity_t* pusher, vec3_t move, vec3_t amove) {
    vec3_t matrix[3], transpose[3];
    vec3_t org, org2, move2;
    gentity_t* block;

    if ((pusher->s.eFlags & EF_MOVER_STOP) &&
        check->s.groundEntityNum != pusher->s.number) {
        return qfalse;
    }

    if (pushed_p > &pushed[MAX_GENTITIES]) {
        G_Error("pushed_p > &pushed[MAX_GENTITIES]");
    }
    pushed_p->ent = check;
    VectorCopy(check->s.pos.trBase, pushed_p->origin);
    VectorCopy(check->s.apos.trBase, pushed_p->angles);
    if (check->client) {
        pushed_p->deltayaw = check->client->ps.delta_angles[YAW];
        VectorCopy(check->client->ps.origin, pushed_p->origin);
    }
    pushed_p++;

    G_CreateRotationMatrix(amove, transpose);
    G_TransposeMatrix(transpose, matrix);
    if (check->client) {
        VectorSubtract(check->client->ps.origin, pusher->r.currentOrigin, org);
    } else {
        VectorSubtract(check->s.pos.trBase, pusher->r.currentOrigin, org);
    }
    VectorCopy(org, org2);
    G_RotatePoint(org2, matrix);
    VectorSubtract(org2, org, move2);
    VectorAdd(check->s.pos.trBase, move, check->s.pos.trBase);
    VectorAdd(check->s.pos.trBase, move2, check->s.pos.trBase);
    if (check->client) {
        VectorAdd(check->client->ps.origin, move, check->client->ps.origin);
        VectorAdd(check->client->ps.origin, move2, check->client->ps.origin);
        check->client->ps.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
    }

    if (check->s.groundEntityNum != pusher->s.number) {
        check->s.groundEntityNum = ENTITYNUM_NONE;
    }

    block = G_TestEntityPosition(check);
    if (!block) {
        if (check->client) {
            VectorCopy(check->client->ps.origin, check->r.currentOrigin);
        } else {
            VectorCopy(check->s.pos.trBase, check->r.currentOrigin);
        }
        trap_LinkEntity(check);
        return qtrue;
    }

    VectorCopy((pushed_p - 1)->origin, check->s.pos.trBase);
    if (check->client) {
        VectorCopy((pushed_p - 1)->origin, check->client->ps.origin);
    }
    VectorCopy((pushed_p - 1)->angles, check->s.apos.trBase);
    block = G_TestEntityPosition(check);
    if (!block) {
        check->s.groundEntityNum = ENTITYNUM_NONE;
        pushed_p--;
        return qtrue;
    }

    return qfalse;
}

qboolean G_CheckProxMinePosition(gentity_t* check) {
    vec3_t start, end;
    trace_t tr;

    VectorMA(check->s.pos.trBase, 0.125, check->movedir, start);
    VectorMA(check->s.pos.trBase, 2, check->movedir, end);
    trap_Trace(&tr, start, NULL, NULL, end, check->s.number, MASK_SOLID);

    if (tr.startsolid || tr.fraction < 1)
        return qfalse;

    return qtrue;
}

qboolean G_TryPushingProxMine(gentity_t* check, gentity_t* pusher, vec3_t move, vec3_t amove) {
    vec3_t forward, right, up;
    vec3_t org, org2, move2;
    int ret;

    VectorSubtract(vec3_origin, amove, org);
    AngleVectors(org, forward, right, up);

    VectorAdd(check->s.pos.trBase, move, check->s.pos.trBase);

    VectorSubtract(check->s.pos.trBase, pusher->r.currentOrigin, org);
    org2[0] = DotProduct(org, forward);
    org2[1] = -DotProduct(org, right);
    org2[2] = DotProduct(org, up);
    VectorSubtract(org2, org, move2);
    VectorAdd(check->s.pos.trBase, move2, check->s.pos.trBase);

    ret = G_CheckProxMinePosition(check);
    if (ret) {
        VectorCopy(check->s.pos.trBase, check->r.currentOrigin);
        trap_LinkEntity(check);
    }
    return ret;
}

void G_ExplodeMissile(gentity_t* ent);

qboolean G_MoverPush(gentity_t* pusher, vec3_t move, vec3_t amove, gentity_t** obstacle) {
    int i, e;
    gentity_t* check;
    vec3_t mins, maxs;
    pushed_t* p;
    int entityList[MAX_GENTITIES];
    int listedEntities;
    vec3_t totalMins, totalMaxs;

    *obstacle = NULL;

    if (pusher->r.currentAngles[0] || pusher->r.currentAngles[1] || pusher->r.currentAngles[2] || amove[0] || amove[1] || amove[2]) {
        float radius;

        radius = RadiusFromBounds(pusher->r.mins, pusher->r.maxs);
        for (i = 0; i < 3; i++) {
            mins[i] = pusher->r.currentOrigin[i] + move[i] - radius;
            maxs[i] = pusher->r.currentOrigin[i] + move[i] + radius;
            totalMins[i] = mins[i] - move[i];
            totalMaxs[i] = maxs[i] - move[i];
        }
    } else {
        for (i = 0; i < 3; i++) {
            mins[i] = pusher->r.absmin[i] + move[i];
            maxs[i] = pusher->r.absmax[i] + move[i];
        }

        VectorCopy(pusher->r.absmin, totalMins);
        VectorCopy(pusher->r.absmax, totalMaxs);
        for (i = 0; i < 3; i++) {
            if (move[i] > 0) {
                totalMaxs[i] += move[i];
            } else {
                totalMins[i] += move[i];
            }
        }
    }

    trap_UnlinkEntity(pusher);

    listedEntities = trap_EntitiesInBox(totalMins, totalMaxs, entityList, MAX_GENTITIES);

    VectorAdd(pusher->r.currentOrigin, move, pusher->r.currentOrigin);
    VectorAdd(pusher->r.currentAngles, amove, pusher->r.currentAngles);
    trap_LinkEntity(pusher);

    for (e = 0; e < listedEntities; e++) {
        check = &g_entities[entityList[e]];

        if (check->s.eType == ET_MISSILE) {
            if (!strcmp(check->classname, "prox mine")) {
                if (check->enemy == pusher) {
                    if (!G_TryPushingProxMine(check, pusher, move, amove)) {
                        check->s.loopSound = 0;
                        G_AddEvent(check, EV_PROXIMITY_MINE_TRIGGER, 0);
                        G_ExplodeMissile(check);
                        if (check->activator) {
                            G_FreeEntity(check->activator);
                            check->activator = NULL;
                        }
                    }
                } else {
                    if (!G_CheckProxMinePosition(check)) {
                        check->s.loopSound = 0;
                        G_AddEvent(check, EV_PROXIMITY_MINE_TRIGGER, 0);
                        G_ExplodeMissile(check);
                        if (check->activator) {
                            G_FreeEntity(check->activator);
                            check->activator = NULL;
                        }
                    }
                }
                continue;
            }
        }

        if (check->s.eType != ET_ITEM && check->s.eType != ET_PLAYER &&
            check->s.eType != ET_GRAPPLE && !check->physicsObject) {
            continue;
        }

        if (check->s.groundEntityNum != pusher->s.number) {
            if (check->r.absmin[0] >= maxs[0] || check->r.absmin[1] >= maxs[1] || check->r.absmin[2] >= maxs[2] || check->r.absmax[0] <= mins[0] || check->r.absmax[1] <= mins[1] || check->r.absmax[2] <= mins[2]) {
                continue;
            }
            if (!G_TestEntityPosition(check)) {
                continue;
            }
        }

        if (G_TryPushingEntity(check, pusher, move, amove)) {
            continue;
        }

        if (pusher->s.pos.trType == TR_SINE || pusher->s.apos.trType == TR_SINE) {
            G_Damage(check, pusher, pusher, NULL, NULL, 99999, 0, MOD_CRUSH);
            continue;
        }

        *obstacle = check;

        for (p = pushed_p - 1; p >= pushed; p--) {
            VectorCopy(p->origin, p->ent->s.pos.trBase);
            VectorCopy(p->angles, p->ent->s.apos.trBase);
            if (p->ent->client) {
                p->ent->client->ps.delta_angles[YAW] = p->deltayaw;
                VectorCopy(p->origin, p->ent->client->ps.origin);
            }
            trap_LinkEntity(p->ent);
        }
        return qfalse;
    }

    return qtrue;
}

void G_MoverTeam(gentity_t* ent) {
    vec3_t move, amove;
    gentity_t *part, *obstacle;
    vec3_t origin, angles;

    obstacle = NULL;

    pushed_p = pushed;
    for (part = ent; part; part = part->teamchain) {
        BG_EvaluateTrajectory(&part->s.pos, level.time, origin);
        BG_EvaluateTrajectory(&part->s.apos, level.time, angles);
        VectorSubtract(origin, part->r.currentOrigin, move);
        VectorSubtract(angles, part->r.currentAngles, amove);
        if (!G_MoverPush(part, move, amove, &obstacle)) {
            break;
        }
    }

    if (part) {
        for (part = ent; part; part = part->teamchain) {
            part->s.pos.trTime += level.time - ( level.time - level.frametime );
            part->s.apos.trTime += level.time - ( level.time - level.frametime );
            BG_EvaluateTrajectory(&part->s.pos, level.time, part->r.currentOrigin);
            BG_EvaluateTrajectory(&part->s.apos, level.time, part->r.currentAngles);
            trap_LinkEntity(part);
        }

        if (ent->blocked) {
            ent->blocked(ent, obstacle);
        }
        return;
    }

    for (part = ent; part; part = part->teamchain) {
        if (part->s.pos.trType == TR_LINEAR_STOP) {
            if (level.time >= part->s.pos.trTime + part->s.pos.trDuration) {
                if (part->reached) {
                    part->reached(part);
                }
            }
        }
    }
}

void G_RunMover(gentity_t* ent) {
    if (ent->flags & FL_TEAMSLAVE) {
        return;
    }

    if (ent->s.pos.trType != TR_STATIONARY || ent->s.apos.trType != TR_STATIONARY) {
        G_MoverTeam(ent);
    }

    G_RunThink(ent);
}

void SetMoverState(gentity_t* ent, moverState_t moverState, int time) {
    vec3_t delta;
    float f;

    for (; ent; ent = ent->teamchain) {
        ent->moverState = moverState;

        ent->s.pos.trTime = time;
        switch (moverState) {
            case MOVER_POS1:
                VectorCopy(ent->pos1, ent->s.pos.trBase);
                ent->s.pos.trType = TR_STATIONARY;
                break;
            case MOVER_POS2:
                VectorCopy(ent->pos2, ent->s.pos.trBase);
                ent->s.pos.trType = TR_STATIONARY;
                break;
            case MOVER_1TO2:
                VectorCopy(ent->pos1, ent->s.pos.trBase);
                VectorSubtract(ent->pos2, ent->pos1, delta);
                f = 1000.0 / ent->s.pos.trDuration;
                VectorScale(delta, f, ent->s.pos.trDelta);
                ent->s.pos.trType = TR_LINEAR_STOP;
                break;
            case MOVER_2TO1:
                VectorCopy(ent->pos2, ent->s.pos.trBase);
                VectorSubtract(ent->pos1, ent->pos2, delta);
                f = 1000.0 / ent->s.pos.trDuration;
                VectorScale(delta, f, ent->s.pos.trDelta);
                ent->s.pos.trType = TR_LINEAR_STOP;
                break;
        }
        BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->r.currentOrigin);
        trap_LinkEntity(ent);
    }
}

void MatchTeam(gentity_t* teamLeader, int moverState, int time) {
    SetMoverState(teamLeader, moverState, time);
}

void ReturnToPos1(gentity_t* ent) {
    MatchTeam(ent, MOVER_2TO1, level.time);

    ent->s.loopSound = ent->soundLoop;

    if (ent->sound2to1) {
        G_AddEvent(ent, EV_GENERAL_SOUND, ent->sound2to1);
    }
}

void Reached_BinaryMover(gentity_t* ent) {
    ent->s.loopSound = ent->soundLoop;

    if (ent->moverState == MOVER_1TO2) {
        SetMoverState(ent, MOVER_POS2, level.time);

        if (ent->soundPos2) {
            G_AddEvent(ent, EV_GENERAL_SOUND, ent->soundPos2);
        }

        ent->think = ReturnToPos1;
        ent->nextthink = level.time + ent->wait;

        if (!ent->activator) {
            ent->activator = ent;
        }
        G_UseTargets(ent, ent->activator);
    } else if (ent->moverState == MOVER_2TO1) {
        SetMoverState(ent, MOVER_POS1, level.time);

        if (ent->soundPos1) {
            G_AddEvent(ent, EV_GENERAL_SOUND, ent->soundPos1);
        }

        if (ent->teammaster == ent || !ent->teammaster) {
            trap_AdjustAreaPortalState(ent, qfalse);
        }
    } else {
        G_Error("Reached_BinaryMover: bad moverState");
    }
}

void Use_BinaryMover(gentity_t* ent, gentity_t* other, gentity_t* activator) {
    int total;
    int partial;

    if (ent->flags & FL_TEAMSLAVE) {
        Use_BinaryMover(ent->teammaster, other, activator);
        return;
    }

    ent->activator = activator;

    if (ent->moverState == MOVER_POS1) {
        MatchTeam(ent, MOVER_1TO2, level.time + 50);

        if (ent->sound1to2) {
            if (ent->spawnflags & 0x10) {
                G_AddEvent(ent, EV_GENERAL_SOUND, G_SoundIndex("sound/movers/doors/door_silver.wav"));
            } else if (ent->spawnflags & 0x20) {
                G_AddEvent(ent, EV_GENERAL_SOUND, G_SoundIndex("sound/movers/doors/door_gold.wav"));
            } else {
                G_AddEvent(ent, EV_GENERAL_SOUND, ent->sound1to2);
            }
        }

        ent->s.loopSound = ent->soundLoop;

        if (ent->teammaster == ent || !ent->teammaster) {
            trap_AdjustAreaPortalState(ent, qtrue);
        }
        return;
    }

    if (ent->moverState == MOVER_POS2) {
        ent->nextthink = level.time + ent->wait;
        return;
    }

    if (ent->moverState == MOVER_2TO1) {
        total = ent->s.pos.trDuration;
        partial = level.time - ent->s.pos.trTime;
        if (partial > total) {
            partial = total;
        }

        MatchTeam(ent, MOVER_1TO2, level.time - (total - partial));

        if (ent->sound1to2) {
            G_AddEvent(ent, EV_GENERAL_SOUND, ent->sound1to2);
        }
        return;
    }

    if (ent->moverState == MOVER_1TO2) {
        total = ent->s.pos.trDuration;
        partial = level.time - ent->s.pos.trTime;
        if (partial > total) {
            partial = total;
        }

        MatchTeam(ent, MOVER_2TO1, level.time - (total - partial));

        if (ent->sound2to1) {
            G_AddEvent(ent, EV_GENERAL_SOUND, ent->sound2to1);
        }
        return;
    }
}

void InitMover(gentity_t* ent) {
    vec3_t move;
    float distance;
    float light;
    vec3_t color;
    qboolean lightSet, colorSet;
    char* sound;

    if (ent->model2) {
        ent->s.modelindex2 = G_ModelIndex(ent->model2);
    }

    if (G_SpawnString("noise", "100", &sound)) {
        ent->s.loopSound = G_SoundIndex(sound);
    }

    lightSet = G_SpawnFloat("light", "100", &light);
    colorSet = G_SpawnVector("color", "1 1 1", color);
    if (lightSet || colorSet) {
        int r, g, b, i;

        r = color[0] * 255;
        if (r > 255) {
            r = 255;
        }
        g = color[1] * 255;
        if (g > 255) {
            g = 255;
        }
        b = color[2] * 255;
        if (b > 255) {
            b = 255;
        }
        i = light / 4;
        if (i > 255) {
            i = 255;
        }
        ent->s.constantLight = r | (g << 8) | (b << 16) | (i << 24);
    }

    ent->use = Use_BinaryMover;
    ent->reached = Reached_BinaryMover;

    ent->moverState = MOVER_POS1;
    ent->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    ent->s.eType = ET_MOVER;
    VectorCopy(ent->pos1, ent->r.currentOrigin);
    trap_LinkEntity(ent);

    ent->s.pos.trType = TR_STATIONARY;
    VectorCopy(ent->pos1, ent->s.pos.trBase);

    VectorSubtract(ent->pos2, ent->pos1, move);
    distance = VectorLength(move);
    if (!ent->speed) {
        ent->speed = 100;
    }
    VectorScale(move, ent->speed, ent->s.pos.trDelta);
    ent->s.pos.trDuration = distance * 1000 / ent->speed;
    if (ent->s.pos.trDuration <= 0) {
        ent->s.pos.trDuration = 1;
    }
}

void Blocked_Door(gentity_t* ent, gentity_t* other) {
    if (!other->client) {
        if (other->s.eType == ET_ITEM && other->item->giType == IT_TEAM) {
            Team_FreeEntity(other);
            return;
        }
        if (other->s.eType == ET_GRAPPLE && other->count) {
            G_ReleaseGrapple(other);
            return;
        }
        G_TempEntity(other->s.origin, EV_ITEM_POP);
        G_FreeEntity(other);
        return;
    }

    if (ent->damage) {
        G_Damage(other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH);
    }
    if (ent->spawnflags & 4) {
        return;
    }

    Use_BinaryMover(ent, ent, other);
}

void Touch_PlatCenterTrigger(gentity_t* ent, gentity_t* other, trace_t* trace);

static void Touch_DoorTriggerSpectator(gentity_t* ent, gentity_t* other, trace_t* trace) {
    int axis;
    vec3_t origin, dir, angles;

    axis = ent->count;
    VectorClear(dir);

    if (fabs(other->s.origin[axis] - ent->r.absmin[axis]) <=
        fabs(other->s.origin[axis] - ent->r.absmax[axis])) {
        dir[axis] = 1.0;
        origin[axis] = ent->r.absmax[axis] + 10;
    } else {
        dir[axis] = -1.0;
        origin[axis] = ent->r.absmin[axis] - 10;
    }

    if (axis != 0) {
        origin[0] = (ent->r.absmax[0] + ent->r.absmin[0]) * 0.5;
    }
    if (axis != 1) {
        origin[1] = (ent->r.absmax[1] + ent->r.absmin[1]) * 0.5;
    }
    if (axis != 2) {
        origin[2] = (ent->r.absmax[2] + ent->r.absmin[2]) * 0.5;
    }

    vectoangles(dir, angles);
    TeleportPlayer(other, origin, angles);
}

void Touch_DoorTrigger(gentity_t* ent, gentity_t* other, trace_t* trace) {
    gentity_t* door;
    gclient_t* client;

    client = other->client;
    if (!client) {
        return;
    }

    door = ent->parent;

    if (door->moverState == MOVER_POS2) {
        return;
    }

    if (client->sess.sessionTeam == TEAM_SPECTATOR) {
        if (door->moverState != MOVER_1TO2 &&
            other->pain_debounce_time < level.time) {
            other->pain_debounce_time = level.time + 1000;
            Touch_DoorTriggerSpectator(ent, other, trace);
        }
    } else if (door->moverState != MOVER_1TO2 &&
               (((door->spawnflags & 0x10) && (client->ps.stats[STAT_KEY] & (KEY_SILVER | KEY_MASTER))) ||
                ((door->spawnflags & 0x20) && (client->ps.stats[STAT_KEY] & (KEY_GOLD | KEY_MASTER))))) {
        Use_BinaryMover(door, ent, other);
    }
}

static void Think_KeyDoor(gentity_t* ent) {
    gentity_t* other;
    vec3_t mins, maxs;
    int i, best;

    for (other = ent; other; other = other->teamchain) {
        other->takedamage = qfalse;
    }

    VectorCopy(ent->r.absmin, mins);
    VectorCopy(ent->r.absmax, maxs);

    for (other = ent->teamchain; other; other = other->teamchain) {
        AddPointToBounds(other->r.absmin, mins, maxs);
        AddPointToBounds(other->r.absmax, mins, maxs);
    }

    best = 0;
    for (i = 1; i < 3; i++) {
        if (maxs[i] - mins[i] < maxs[best] - mins[best]) {
            best = i;
        }
    }
    maxs[best] += 120;
    mins[best] -= 120;

    other = G_Spawn();
    other->classname = "door_trigger";
    VectorCopy(mins, other->r.mins);
    VectorCopy(maxs, other->r.maxs);
    other->parent = ent;
    other->r.contents = CONTENTS_TRIGGER;
    other->touch = Touch_DoorTrigger;
    other->count = best;
    trap_LinkEntity(other);

    MatchTeam(ent, ent->moverState, level.time);
}

void Think_SpawnNewDoorTrigger(gentity_t* ent) {
    gentity_t* other;
    vec3_t mins, maxs;
    int i, best;

    for (other = ent; other; other = other->teamchain) {
        other->takedamage = qtrue;
    }

    VectorCopy(ent->r.absmin, mins);
    VectorCopy(ent->r.absmax, maxs);

    for (other = ent->teamchain; other; other = other->teamchain) {
        AddPointToBounds(other->r.absmin, mins, maxs);
        AddPointToBounds(other->r.absmax, mins, maxs);
    }

    best = 0;
    for (i = 1; i < 3; i++) {
        if (maxs[i] - mins[i] < maxs[best] - mins[best]) {
            best = i;
        }
    }
    maxs[best] += 120;
    mins[best] -= 120;

    other = G_Spawn();
    other->classname = "door_trigger";
    VectorCopy(mins, other->r.mins);
    VectorCopy(maxs, other->r.maxs);
    other->parent = ent;
    other->r.contents = CONTENTS_TRIGGER;
    other->touch = Touch_PlatCenterTrigger;
    other->count = best;
    trap_LinkEntity(other);

    MatchTeam(ent, ent->moverState, level.time);
}

void Think_MatchTeam(gentity_t* ent) {
    MatchTeam(ent, ent->moverState, level.time);
}

void SP_func_door(gentity_t* ent) {
    vec3_t abs_movedir;
    float distance;
    vec3_t size;
    float lip;

    ent->sound1to2 = ent->sound2to1 = G_SoundIndex("sound/movers/doors/dr1_strt.wav");
    ent->soundPos1 = ent->soundPos2 = G_SoundIndex("sound/movers/doors/dr1_end.wav");

    ent->blocked = Blocked_Door;

    if (!ent->speed)
        ent->speed = 400;

    if (!ent->wait)
        ent->wait = 2;
    ent->wait *= 1000;

    G_SpawnFloat("lip", "8", &lip);

    G_SpawnInt("dmg", "2", &ent->damage);

    VectorCopy(ent->s.origin, ent->pos1);

    trap_SetBrushModel(ent, ent->model);
    G_SetMovedir(ent->s.angles, ent->movedir);
    abs_movedir[0] = fabs(ent->movedir[0]);
    abs_movedir[1] = fabs(ent->movedir[1]);
    abs_movedir[2] = fabs(ent->movedir[2]);
    VectorSubtract(ent->r.maxs, ent->r.mins, size);
    distance = DotProduct(abs_movedir, size) - lip;
    VectorMA(ent->pos1, distance, ent->movedir, ent->pos2);

    if (ent->spawnflags & 1) {
        vec3_t temp;

        VectorCopy(ent->pos2, temp);
        VectorCopy(ent->s.origin, ent->pos2);
        VectorCopy(temp, ent->pos1);
    }

    InitMover(ent);

    ent->nextthink = level.time + (1000 / sv_fps.integer);

    if (!(ent->flags & FL_TEAMSLAVE)) {
        int health;

        G_SpawnInt("health", "0", &health);
        if (health) {
            ent->takedamage = qtrue;
        }
        if (ent->spawnflags & 0x30) {
            ent->think = Think_KeyDoor;
        } else if (ent->targetname || health) {
            ent->think = Think_MatchTeam;
        } else {
            ent->think = Think_SpawnNewDoorTrigger;
        }
    }
}

void Touch_Plat(gentity_t* ent, gentity_t* other, trace_t* trace) {
    if (!other->client || other->client->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }

    if (ent->moverState == MOVER_POS2) {
        ent->nextthink = level.time + 1000;
    }
}

void Touch_PlatCenterTrigger(gentity_t* ent, gentity_t* other, trace_t* trace) {
    if (!other->client || other->client->sess.sessionTeam != TEAM_SPECTATOR) {
        if (ent->parent->moverState != MOVER_1TO2) {
            Use_BinaryMover(ent->parent, ent, other);
        }
        return;
    }

    if (ent->parent->moverState != MOVER_POS2 &&
        ent->parent->moverState != MOVER_1TO2 &&
        other->pain_debounce_time < level.time) {
        other->pain_debounce_time = level.time + 1000;
        Touch_DoorTriggerSpectator(ent, other, trace);
    }
}

void SpawnPlatTrigger(gentity_t* ent) {
    gentity_t* trigger;
    vec3_t tmin, tmax;

    trigger = G_Spawn();
    trigger->classname = "plat_trigger";
    trigger->touch = Touch_PlatCenterTrigger;
    trigger->r.contents = CONTENTS_TRIGGER;
    trigger->parent = ent;

    tmin[0] = ent->pos1[0] + ent->r.mins[0] + 33;
    tmin[1] = ent->pos1[1] + ent->r.mins[1] + 33;
    tmin[2] = ent->pos1[2] + ent->r.mins[2];

    tmax[0] = ent->pos1[0] + ent->r.maxs[0] - 33;
    tmax[1] = ent->pos1[1] + ent->r.maxs[1] - 33;
    tmax[2] = ent->pos1[2] + ent->r.maxs[2] + 8;

    if (tmax[0] <= tmin[0]) {
        tmin[0] = ent->pos1[0] + (ent->r.mins[0] + ent->r.maxs[0]) * 0.5;
        tmax[0] = tmin[0] + 1;
    }
    if (tmax[1] <= tmin[1]) {
        tmin[1] = ent->pos1[1] + (ent->r.mins[1] + ent->r.maxs[1]) * 0.5;
        tmax[1] = tmin[1] + 1;
    }

    VectorCopy(tmin, trigger->r.mins);
    VectorCopy(tmax, trigger->r.maxs);

    trap_LinkEntity(trigger);
}

void SP_func_plat(gentity_t* ent) {
    float lip, height;

    ent->sound1to2 = ent->sound2to1 = G_SoundIndex("sound/movers/plats/pt1_strt.wav");
    ent->soundPos1 = ent->soundPos2 = G_SoundIndex("sound/movers/plats/pt1_end.wav");

    VectorClear(ent->s.angles);

    G_SpawnFloat("lip", "8", &lip);
    G_SpawnFloat("speed", "200", &ent->speed);
    G_SpawnInt("dmg", "2", &ent->damage);
    G_SpawnFloat("wait", "1", &ent->wait);
    ent->wait *= 1000;

    trap_SetBrushModel(ent, ent->model);

    if (!G_SpawnFloat("height", "0", &height)) {
        height = (ent->r.maxs[2] - ent->r.mins[2]) - lip;
    }

    VectorCopy(ent->s.origin, ent->pos2);
    VectorCopy(ent->pos2, ent->pos1);
    ent->pos1[2] -= height;

    InitMover(ent);

    ent->touch = Touch_Plat;

    ent->blocked = Blocked_Door;

    ent->parent = ent;

    if (!ent->targetname) {
        SpawnPlatTrigger(ent);
    }
}

void Touch_Button(gentity_t* ent, gentity_t* other, trace_t* trace) {
    if (!other->client) {
        return;
    }

    if (ent->moverState == MOVER_POS1) {
        Use_BinaryMover(ent, other, other);
    }
}

static void Touch_KeyButton(gentity_t* ent, gentity_t* other, trace_t* trace) {
    gclient_t* client;

    client = other->client;
    if (!client) {
        return;
    }

    if (ent->moverState != MOVER_POS1) {
        return;
    }

    if (((ent->spawnflags & 0x10) && (client->ps.stats[STAT_KEY] & (KEY_SILVER | KEY_MASTER))) ||
        ((ent->spawnflags & 0x20) && (client->ps.stats[STAT_KEY] & (KEY_GOLD | KEY_MASTER)))) {
        Use_BinaryMover(ent, other, other);
    }
}

void SP_func_button(gentity_t* ent) {
    vec3_t abs_movedir;
    float distance;
    vec3_t size;
    float lip;

    ent->sound1to2 = G_SoundIndex("sound/movers/switches/butn2.wav");

    if (!ent->speed) {
        ent->speed = 40;
    }

    if (!ent->wait) {
        ent->wait = 1;
    }
    ent->wait *= 1000;

    VectorCopy(ent->s.origin, ent->pos1);

    trap_SetBrushModel(ent, ent->model);

    G_SpawnFloat("lip", "4", &lip);

    G_SetMovedir(ent->s.angles, ent->movedir);
    abs_movedir[0] = fabs(ent->movedir[0]);
    abs_movedir[1] = fabs(ent->movedir[1]);
    abs_movedir[2] = fabs(ent->movedir[2]);
    VectorSubtract(ent->r.maxs, ent->r.mins, size);
    distance = abs_movedir[0] * size[0] + abs_movedir[1] * size[1] + abs_movedir[2] * size[2] - lip;
    VectorMA(ent->pos1, distance, ent->movedir, ent->pos2);

    if (ent->spawnflags & 0x30) {
        ent->touch = Touch_KeyButton;
        InitMover(ent);
        return;
    }

    if (ent->health) {
        ent->takedamage = qtrue;
    } else {
        ent->touch = Touch_Button;
    }

    InitMover(ent);
}

#define TRAIN_START_ON 1
#define TRAIN_TOGGLE 2
#define TRAIN_BLOCK_STOPS 4

void Think_BeginMoving(gentity_t* ent) {
    ent->s.pos.trTime = level.time;
    ent->s.pos.trType = TR_LINEAR_STOP;
}

void Reached_Train(gentity_t* ent) {
    gentity_t* next;
    float speed;
    vec3_t move;
    float length;

    next = ent->nextTrain;
    if (!next || !next->nextTrain) {
        return;
    }

    G_UseTargets(next, NULL);

    ent->nextTrain = next->nextTrain;
    VectorCopy(next->s.origin, ent->pos1);
    VectorCopy(next->nextTrain->s.origin, ent->pos2);

    if (next->speed) {
        speed = next->speed;
    } else {
        speed = ent->speed;
    }
    if (speed < 1) {
        speed = 1;
    }

    VectorSubtract(ent->pos2, ent->pos1, move);
    length = VectorLength(move);

    ent->s.pos.trDuration = length * 1000 / speed;

    ent->s.loopSound = next->soundLoop;

    SetMoverState(ent, MOVER_1TO2, level.time);

    if (next->wait) {
        ent->nextthink = level.time + next->wait * 1000;
        ent->think = Think_BeginMoving;
        ent->s.pos.trType = TR_STATIONARY;
    }
}

void Think_SetupTrainTargets(gentity_t* ent) {
    gentity_t *path, *next, *start;

    ent->nextTrain = G_Find(NULL, FOFS(targetname), ent->target);
    if (!ent->nextTrain) {
        G_Printf("func_train at %s with an unfound target\n",
                 vtos(ent->r.absmin));
        return;
    }

    start = NULL;
    for (path = ent->nextTrain; path != start; path = next) {
        if (!start) {
            start = path;
        }

        if (!path->target) {
            G_Printf("Train corner at %s without a target\n",
                     vtos(path->s.origin));
            return;
        }

        next = NULL;
        do {
            next = G_Find(next, FOFS(targetname), path->target);
            if (!next) {
                G_Printf("Train corner at %s without a target path_corner\n",
                         vtos(path->s.origin));
                return;
            }
        } while (strcmp(next->classname, "path_corner"));

        path->nextTrain = next;
    }

    Reached_Train(ent);
}

void SP_path_corner(gentity_t* self) {
    if (!self->targetname) {
        G_Printf("path_corner with no targetname at %s\n", vtos(self->s.origin));
        G_FreeEntity(self);
        return;
    }
}

void SP_func_train(gentity_t* self) {
    VectorClear(self->s.angles);

    if (!self->speed) {
        self->speed = 100;
    }

    if (!self->target) {
        G_Printf("func_train without a target at %s\n", vtos(self->r.absmin));
        G_FreeEntity(self);
        return;
    }

    G_SpawnInt("dmg", "0", &self->damage);
    if (self->damage > 0) {
        self->spawnflags |= 4;
    }

    trap_SetBrushModel(self, self->model);
    InitMover(self);

    self->blocked = Blocked_Door;
    self->reached = Reached_Train;

    self->nextthink = level.time + (1000 / sv_fps.integer);
    self->think = Think_SetupTrainTargets;
}

void SP_func_static(gentity_t* ent) {
    trap_SetBrushModel(ent, ent->model);
    InitMover(ent);
    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);
}

void SP_func_rotating(gentity_t* ent) {
    if (!ent->speed) {
        ent->speed = 100;
    }

    ent->s.apos.trType = TR_LINEAR;
    if (ent->spawnflags & 4) {
        ent->s.apos.trDelta[2] = ent->speed;
    } else if (ent->spawnflags & 8) {
        ent->s.apos.trDelta[0] = ent->speed;
    } else {
        ent->s.apos.trDelta[1] = ent->speed;
    }

    G_SpawnInt("dmg", "2", &ent->damage);

    trap_SetBrushModel(ent, ent->model);
    InitMover(ent);

    VectorCopy(ent->s.apos.trBase, ent->r.currentAngles);
    ent->blocked = Blocked_Door;
    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);

    trap_LinkEntity(ent);
}

void SP_func_bobbing(gentity_t* ent) {
    float height;
    float phase;

    G_SpawnFloat("speed", "4", &ent->speed);
    G_SpawnFloat("height", "32", &height);
    G_SpawnInt("dmg", "2", &ent->damage);
    G_SpawnFloat("phase", "0", &phase);

    trap_SetBrushModel(ent, ent->model);
    InitMover(ent);

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);

    ent->s.pos.trDuration = ent->speed * 1000;
    ent->s.pos.trTime = ent->s.pos.trDuration * phase;
    ent->s.pos.trType = TR_SINE;

    if (ent->spawnflags & 1) {
        ent->s.pos.trDelta[0] = height;
    } else if (ent->spawnflags & 2) {
        ent->s.pos.trDelta[1] = height;
    } else {
        ent->s.pos.trDelta[2] = height;
    }
}

void SP_func_pendulum(gentity_t* ent) {
    float freq;
    float length;
    float phase;
    float speed;

    G_SpawnFloat("speed", "30", &speed);
    G_SpawnInt("dmg", "2", &ent->damage);
    G_SpawnFloat("phase", "0", &phase);

    trap_SetBrushModel(ent, ent->model);

    length = fabs(ent->r.mins[2]);
    if (length < 8) {
        length = 8;
    }

    freq = 1 / (M_PI * 2) * sqrt(g_gravity.value / (3 * length));

    ent->s.pos.trDuration = (1000 / freq);

    InitMover(ent);

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);

    VectorCopy(ent->s.angles, ent->s.apos.trBase);

    ent->s.apos.trDuration = 1000 / freq;
    ent->s.apos.trTime = ent->s.apos.trDuration * phase;
    ent->s.apos.trType = TR_SINE;
    ent->s.apos.trDelta[2] = speed;
}
