#include "g_local.h"

typedef struct haxSnap_s {
    struct haxSnap_s* next;
    struct haxSnap_s* prev;
    vec3_t origin;
    vec3_t mins;
    vec3_t maxs;
    int time;
} haxSnap_t;

typedef struct {
    vec3_t origin;
    vec3_t mins;
    vec3_t maxs;
    int time;
} haxSaved_t;

static haxSnap_t* haxHistory[MAX_CLIENTS];
static haxSaved_t haxSaved[MAX_CLIENTS];

void HAX_Init(void) {
    int i, j, count;
    haxSnap_t* ring;

    count = g_lagHaxHistory.integer;
    if (count < 1) {
        return;
    }
    for (i = 0; i < level.maxclients; i++) {
        ring = G_Alloc(count * sizeof(haxSnap_t));
        haxHistory[i] = ring;
        for (j = 0; j < count; j++) {
            ring[j].next = &ring[(j + 1) % count];
            ring[j].prev = &ring[(j + count - 1) % count];
            ring[j].time = 0;
        }
    }
}

void HAX_Update(gentity_t* ent) {
    haxSnap_t* slot;

    slot = haxHistory[ent->s.number];
    if (!slot) {
        return;
    }
    slot = slot->next;
    if (ent->s.eType == ET_PLAYER) {
        slot->time = level.time;
        VectorCopy(ent->s.pos.trBase, slot->origin);
        VectorCopy(ent->r.mins, slot->mins);
        VectorCopy(ent->r.maxs, slot->maxs);
    } else {
        slot->time = 0;
    }
    haxHistory[ent->s.number] = slot;
}

void HAX_Clear(gentity_t* ent) {
    if (haxHistory[ent->s.number]) {
        haxHistory[ent->s.number]->time = 0;
    }
}

static void HAX_Replay(gentity_t* ent, int time) {
    haxSnap_t* cur;
    haxSnap_t* older;
    haxSnap_t* newer;
    float frac;
    vec3_t diff;

    cur = haxHistory[ent->s.number];
    if (!cur || time > cur->time) {
        return;
    }
    older = cur;
    newer = cur;
    while (older->time > time) {
        haxSnap_t* prev = older->prev;
        newer = older;
        if (prev->time >= older->time) {
            break;
        }
        older = prev;
    }
    if (older->time == 0) {
        return;
    }
    trap_UnlinkEntity(ent);
    if (older == newer || older->time == newer->time) {
        VectorCopy(older->origin, ent->r.currentOrigin);
    } else {
        frac = (float)(time - older->time) / (float)(older->time - newer->time);
        VectorSubtract(older->origin, newer->origin, diff);
        VectorMA(older->origin, frac, diff, ent->r.currentOrigin);
    }
    VectorCopy(newer->mins, ent->r.mins);
    VectorCopy(newer->maxs, ent->r.maxs);
    trap_LinkEntity(ent);
}

void HAX_Begin(gentity_t* shooter, int startTime) {
    int i;
    gentity_t* ent;

    if (shooter->r.svFlags & SVF_BOT) {
        return;
    }
    if (startTime < level.time - g_lagHaxMs.integer) {
        startTime = level.time - g_lagHaxMs.integer;
    }
    for (i = 0; i < level.maxclients; i++) {
        ent = &g_entities[i];
        haxSaved[i].time = 0;
        if (ent->client && ent->client->pers.connected == CON_CONNECTED && ent != shooter &&
            ent->client->ps.pm_type != PM_SPECTATOR) {
            VectorCopy(ent->r.currentOrigin, haxSaved[i].origin);
            VectorCopy(ent->r.mins, haxSaved[i].mins);
            VectorCopy(ent->r.maxs, haxSaved[i].maxs);
            haxSaved[i].time = level.time;
            HAX_Replay(ent, startTime);
        }
    }
}

void HAX_End(gentity_t* shooter) {
    int i;
    gentity_t* ent;

    (void)shooter;
    for (i = 0; i < level.maxclients; i++) {
        if (haxSaved[i].time == level.time) {
            ent = &g_entities[i];
            VectorCopy(haxSaved[i].origin, ent->r.currentOrigin);
            VectorCopy(haxSaved[i].mins, ent->r.mins);
            VectorCopy(haxSaved[i].maxs, ent->r.maxs);
            trap_LinkEntity(ent);
        }
    }
}
