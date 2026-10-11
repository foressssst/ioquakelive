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
#include "cg_local.h"

void CG_CheckAmmo(void) {
    int weapon = cg.snap->ps.weapon;
    int ammo = cg.snap->ps.ammo[weapon];
    int status;

    if (weapon < 0 || weapon >= WP_NUM_WEAPONS || ammo == -1 ||
        (float)maxAmmoStandard[weapon] * cg_lowAmmoWarningPercentile.value <= (float)ammo) {
        status = 0;
    } else {
        status = ammo ? 1 : 2;
    }

    if (status != cg.lowAmmoWarning) {
        if (status == 2) {
            trap_S_StartLocalSound(cgs.media.noAmmoSound, CHAN_LOCAL_SOUND);
        } else if (status == 1 && cg_lowAmmoWarningSound.value == 1.0f) {
            trap_S_StartLocalSound(cgs.media.lowAmmoSound, CHAN_LOCAL_SOUND);
        } else if (status == 1 && cg_lowAmmoWarningSound.value == 2.0f) {
            trap_S_StartLocalSound(cgs.media.noAmmoSound, CHAN_LOCAL_SOUND);
        }
    }
    cg.lowAmmoWarning = status;
}

void CG_DamageFeedback(int yawByte, int pitchByte, int damage) {
    float left, front, up;
    float kick;
    int health;
    float scale;
    vec3_t dir;
    vec3_t angles;
    float dist;
    float yaw, pitch;

    cg.attackerTime = cg.time;

    health = cg.snap->ps.stats[STAT_HEALTH];
    if (health < 40) {
        scale = 1;
    } else {
        scale = 40.0 / health;
    }

    kick = damage * scale;

    if (kick < 5)
        kick = 5;
    if (kick > 10)
        kick = 10;

    kick = cg_kickScale.value * kick;

    if (yawByte == 255 && pitchByte == 255) {
        cg.damageX = 0;
        cg.damageY = 0;
        cg.v_dmg_roll = 0;
        cg.v_dmg_pitch = -kick;
    } else {
        pitch = pitchByte / 255.0 * 360;
        yaw = yawByte / 255.0 * 360;

        angles[PITCH] = pitch;
        angles[YAW] = yaw;
        angles[ROLL] = 0;

        AngleVectors(angles, dir, NULL, NULL);
        VectorSubtract(vec3_origin, dir, dir);

        front = DotProduct(dir, cg.refdef.viewaxis[0]);
        left = DotProduct(dir, cg.refdef.viewaxis[1]);
        up = DotProduct(dir, cg.refdef.viewaxis[2]);

        dir[0] = front;
        dir[1] = left;
        dir[2] = 0;
        dist = VectorLength(dir);
        if (dist < 0.1) {
            dist = 0.1f;
        }

        cg.v_dmg_roll = kick * left;

        cg.v_dmg_pitch = -kick * front;

        if (front <= 0.1) {
            front = 0.1f;
        }
        cg.damageX = -left / front;
        cg.damageY = up / dist;
    }

    if (cg.damageX > 1.0) {
        cg.damageX = 1.0;
    }
    if (cg.damageX < -1.0) {
        cg.damageX = -1.0;
    }

    if (cg.damageY > 1.0) {
        cg.damageY = 1.0;
    }
    if (cg.damageY < -1.0) {
        cg.damageY = -1.0;
    }

    if (kick > 10) {
        kick = 10;
    }
    cg.damageValue = kick;
    cg.v_dmg_time = cg.time + DAMAGE_TIME;
}

static void CG_SelectPreferredWeapon(void) {
    char* p = cg_preferredStartingWeapons.string;
    int i;

    for (i = 1; i < 15; i++) {
        const char* token = COM_ParseExt(&p, qtrue);
        int weapon;

        if (!token[0]) {
            break;
        }
        weapon = CG_WeaponForShortName(token);
        if (!weapon) {
            weapon = atoi(token);
        }
        if (weapon >= WP_GAUNTLET && weapon <= WP_HMG && (cg.snap->ps.stats[STAT_WEAPONS] & (1 << weapon))) {
            cg.weaponSelect = weapon;
            return;
        }
    }
    cg.weaponSelect = cg.snap->ps.weapon;
}

void CG_Respawn(void) {
    cg.thisFrameTeleport = qtrue;

    cg.weaponSelectTime = cg.time;

    trap_Cvar_Set("cg_weaponPrimary", va("%i", cg.weaponPrimaryQueued));

    CG_SelectPreferredWeapon();

    cg.itemPickup = 0;
    cg.itemPickupTime = 0;
    cg.itemPickupLast = 0;
    cg.itemPickupBlendTime = 0;
    cg.itemPickupCount = 1;

    if (cgs.gametype == GT_RACE) {
        cg.race.active = qfalse;
        cg.race.startTime = 0;
        cg.race.currentCheckPoint = 0;
        cg.race.checkpointDiff = 0;
        cg.race.hasDiff = qfalse;
        cg.race.checkpointsPassed = 0;
        cg.race.nextCheckpointEnt = -1;
        cg.race.nextNextCheckpointEnt = -1;
        trap_SendClientCommand("raceinit");
    }

    if (cg.snap->ps.eFlags & EF_SPECTATOR) {
        trap_SendClientCommand("specresp");
    }
}

extern char* eventnames[];

void CG_CheckPlayerstateEvents(playerState_t* ps, playerState_t* ops) {
    int i;
    int event;
    centity_t* cent;

    if (ps->externalEvent && ps->externalEvent != ops->externalEvent) {
        cent = &cg_entities[ps->clientNum];
        cent->currentState.event = ps->externalEvent;
        cent->currentState.eventParm = ps->externalEventParm;
        CG_EntityEvent(cent, cent->lerpOrigin);
    }

    cent = &cg.predictedPlayerEntity;
    for (i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++) {
        if (i >= ops->eventSequence
            || (i > ops->eventSequence - MAX_PS_EVENTS && ps->events[i & (MAX_PS_EVENTS - 1)] != ops->events[i & (MAX_PS_EVENTS - 1)])) {
            event = ps->events[i & (MAX_PS_EVENTS - 1)];
            cent->currentState.event = event;
            cent->currentState.eventParm = ps->eventParms[i & (MAX_PS_EVENTS - 1)];
            CG_EntityEvent(cent, cent->lerpOrigin);

            cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

            cg.eventSequence++;
        }
    }
}

void CG_CheckChangedPredictableEvents(playerState_t* ps) {
    int i;
    int event;
    centity_t* cent;

    cent = &cg.predictedPlayerEntity;
    for (i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++) {
        if (i >= cg.eventSequence) {
            continue;
        }
        if (i > cg.eventSequence - MAX_PREDICTED_EVENTS) {
            if (ps->events[i & (MAX_PS_EVENTS - 1)] != cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)]) {
                event = ps->events[i & (MAX_PS_EVENTS - 1)];
                cent->currentState.event = event;
                cent->currentState.eventParm = ps->eventParms[i & (MAX_PS_EVENTS - 1)];
                CG_EntityEvent(cent, cent->lerpOrigin);

                cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

                if (cg_showmiss.integer) {
                    CG_Printf("WARNING: changed predicted event\n");
                }
            }
        }
    }
}

void CG_PushReward(sfxHandle_t sfx, qhandle_t shader, int rewardCount) {
    if (cg.rewardStack < (MAX_REWARDSTACK - 1)) {
        cg.rewardStack++;
        cg.rewardSound[cg.rewardStack] = sfx;
        cg.rewardShader[cg.rewardStack] = shader;
        cg.rewardCount[cg.rewardStack] = rewardCount;
    }
}

static sfxHandle_t CG_HitBeepSound(const playerState_t* ps) {
    int tier = ps->generic1 >> 6;

    if (cg_hitBeep.integer == 1) {
        return cgs.media.hitSounds[1];
    }
    if (cg_hitBeep.integer == 3) {
        tier = 3 - tier;
    }
    switch (tier) {
        case 1:
            return cgs.media.hitSounds[1];
        case 2:
            return cgs.media.hitSounds[2];
        case 3:
            return cgs.media.hitSounds[3];
        default:
            return cgs.media.hitSounds[0];
    }
}

static void CG_CheckHitSounds(const playerState_t* ps, const playerState_t* ops) {
    if (ps->persistant[PERS_HITS] > ops->persistant[PERS_HITS]) {
        if (cg_killBeep.integer >= 1 &&
            ps->persistant[PERS_ATTACKEE_ARMOR] > ops->persistant[PERS_ATTACKEE_ARMOR]) {
            trap_S_StartLocalSoundVolume(cgs.media.killBeepSounds[cg_killBeep.integer - 1],
                                         CHAN_LOCAL_SOUND, s_killBeepVolume.value);
        } else if (cg_hitBeep.integer) {
            trap_S_StartLocalSound(CG_HitBeepSound(ps), CHAN_LOCAL_SOUND);
        }
    } else if (ps->persistant[PERS_HITS] < ops->persistant[PERS_HITS] && cg_hitBeep.integer) {
        trap_S_StartLocalSound(cgs.media.hitTeamSound, CHAN_LOCAL_SOUND);
    }
}

void CG_CheckLocalSounds(playerState_t* ps, playerState_t* ops) {
    int count, bits;
    sfxHandle_t sfx;

    if (ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT]) {
        return;
    }

    CG_CheckHitSounds(ps, ops);

    if (cg.intermissionStarted == 1) {
        return;
    }

    count = ps->persistant[PERS_CAPTURES];
    if (count && count != ops->persistant[PERS_CAPTURES]) {
        CG_PushReward(cgs.media.captureAwardSound, cgs.media.medalCapture, count);
    }
    count = ps->persistant[PERS_ASSIST_COUNT];
    if (count && count != ops->persistant[PERS_ASSIST_COUNT]) {
        CG_PushReward(cgs.media.assistSound, cgs.media.medalAssist, count);
    }
    count = ps->persistant[PERS_DEFEND_COUNT];
    if (count && count != ops->persistant[PERS_DEFEND_COUNT]) {
        CG_PushReward(cgs.media.defendSound, cgs.media.medalDefend, count);
    }
    count = ps->persistant[PERS_IMPRESSIVE_COUNT];
    if (count && count != ops->persistant[PERS_IMPRESSIVE_COUNT]) {
        sfx = (count == 1) ? cgs.media.firstImpressiveSound : cgs.media.impressiveSounds[rand() % 3];
        CG_PushReward(sfx, cgs.media.medalImpressive, count);
    }
    count = ps->persistant[PERS_EXCELLENT_COUNT];
    if (count && count != ops->persistant[PERS_EXCELLENT_COUNT]) {
        sfx = (count == 1) ? cgs.media.firstExcellentSound : cgs.media.excellentSounds[rand() % 3];
        CG_PushReward(sfx, cgs.media.medalExcellent, count);
    }
    count = ps->persistant[PERS_GAUNTLET_FRAG_COUNT];
    if (count && count != ops->persistant[PERS_GAUNTLET_FRAG_COUNT]) {
        sfx = (count == 1) ? cgs.media.firstHumiliationSound : cgs.media.humiliationSounds[rand() % 3];
        CG_PushReward(sfx, cgs.media.medalGauntlet, count);
    }

    bits = ps->persistant[PERS_PLAYEREVENTS] ^ ops->persistant[PERS_PLAYEREVENTS];
    if (bits & PLAYEREVENT_DENIEDREWARD) {
        CG_AddBufferedSound(cgs.media.deniedSound);
    } else if (bits & PLAYEREVENT_GAUNTLETREWARD) {
        CG_AddBufferedSound(cgs.media.humiliationSounds[rand() % 3]);
    } else if (bits & PLAYEREVENT_HOLYSHIT) {
        CG_AddBufferedSound(cgs.media.holyShitSound);
    }

    if ((cgs.gametype >= GT_CTF && cgs.gametype <= GT_HARVESTER) || cgs.gametype == GT_AD) {
        if ((ps->powerups[PW_REDFLAG] != ops->powerups[PW_REDFLAG] && ps->powerups[PW_REDFLAG]) ||
            (ps->powerups[PW_BLUEFLAG] != ops->powerups[PW_BLUEFLAG] && ps->powerups[PW_BLUEFLAG]) ||
            (ps->powerups[PW_NEUTRALFLAG] != ops->powerups[PW_NEUTRALFLAG] && ps->powerups[PW_NEUTRALFLAG])) {
            CG_AddBufferedSound(cgs.media.youHaveFlagSound);
        }
    }

    if (cg.warmup) {
        return;
    }

    if (!((ps->persistant[PERS_PLAYEREVENTS] ^ ops->persistant[PERS_PLAYEREVENTS]) & PLAYEREVENT_FIRSTFRAG) &&
        ps->persistant[PERS_RANK] != ops->persistant[PERS_RANK] && cgs.gametype < GT_TEAM) {
        if (ps->persistant[PERS_RANK] == 0) {
            if (cg_announcerLeadsVO.integer) {
                CG_AddBufferedSound(cgs.media.takenLeadSound);
            }
        } else if (ps->persistant[PERS_RANK] == RANK_TIED_FLAG) {
            if (cg_announcerTiesVO.integer) {
                CG_AddBufferedSound(cgs.media.tiedLeadSound);
            }
        } else if ((ops->persistant[PERS_RANK] & ~RANK_TIED_FLAG) == 0) {
            if (cg_announcerLeadsVO.integer) {
                CG_AddBufferedSound(cgs.media.lostLeadSound);
            }
        }
    }

    if (cgs.timelimit > 0 && !cgs.freezeEnd) {
        int msec = cg.time - cgs.levelStartTime;
        int oneMinute = cgs.timelimit * 60000 - 60000;
        int suddenDeath = cgs.timelimit * 60000 + 2000;
        int fiveMinutes = (cgs.timelimit - 5) * 60000;
        int roundLeft;

        if (!(cg.timelimitWarnings & 4) && msec > suddenDeath && cg.joinMenuCloseTime < cgs.levelStartTime + suddenDeath) {
            cg.timelimitWarnings |= 1 | 2 | 4;
            if (!cgs.timelimit_overtime || BG_IsRoundBasedGameType(cgs.gametype)) {
                CG_AddBufferedSound(cgs.media.suddenDeathSound);
            }
        } else if (!(cg.timelimitWarnings & 2) && msec > oneMinute && cg.joinMenuCloseTime < cgs.levelStartTime + oneMinute) {
            cg.timelimitWarnings |= 1 | 2;
            CG_AddBufferedSound(cgs.media.oneMinuteSound);
        } else if (cgs.timelimit > 5 && !(cg.timelimitWarnings & 1) && msec > fiveMinutes &&
                   cg.joinMenuCloseTime < cgs.levelStartTime + fiveMinutes) {
            cg.timelimitWarnings |= 1;
            CG_AddBufferedSound(cgs.media.fiveMinuteSound);
        } else if (!(cg.timelimitWarnings & 8) && BG_IsRoundBasedGameType(cgs.gametype) && cgs.roundStartTime > 0) {
            roundLeft = cgs.roundtimelimit * 1000 - cg.time + cgs.roundStartTime;
            if ((unsigned)(roundLeft - 1) <= 29998) {
                cg.timelimitWarnings |= 8;
                CG_AddBufferedSound(cgs.media.thirtySecondWarningSound);
            }
        }
    }

    if (cgs.fraglimit > 0 && cgs.gametype <= GT_TEAM) {
        if (!(cg.fraglimitWarnings & 4) && cgs.scores1 == cgs.fraglimit - 1) {
            cg.fraglimitWarnings |= 1 | 2 | 4;
            CG_AddBufferedSound(cgs.media.oneFragSound);
        } else if (cgs.fraglimit > 2 && !(cg.fraglimitWarnings & 2) && cgs.scores1 == cgs.fraglimit - 2) {
            cg.fraglimitWarnings |= 1 | 2;
            CG_AddBufferedSound(cgs.media.twoFragSound);
        } else if (cgs.fraglimit > 3 && !(cg.fraglimitWarnings & 1) && cgs.scores1 == cgs.fraglimit - 3) {
            cg.fraglimitWarnings |= 1;
            CG_AddBufferedSound(cgs.media.threeFragSound);
        }
    }
}

void CG_TransitionPlayerState(playerState_t* ps, playerState_t* ops) {
    if (ps->clientNum != ops->clientNum) {
        cg.thisFrameTeleport = qtrue;
        *ops = *ps;
    }

    if (ps->damageEvent != ops->damageEvent && ps->damageCount) {
        cg.damageTime = (float)cg.time;
        CG_DamageFeedback(ps->damageYaw, ps->damagePitch, ps->damageCount);
    }

    if (!cg.mapRestart) {
        if (ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT]) {
            CG_Respawn();
        }
    } else {
        CG_Respawn();
        cg.mapRestart = qfalse;
    }

    if (cg.snap->ps.pm_type != PM_INTERMISSION && ps->persistant[PERS_TEAM] != TEAM_SPECTATOR) {
        CG_CheckLocalSounds(ps, ops);
        if (ps->persistant[PERS_HITS] > ops->persistant[PERS_HITS]) {
            cg.crosshairHitType = (ps->generic1 >> 6) + 1;
            cg.crosshairHitTime = cg.time;
        }
    }

    CG_CheckAmmo();

    CG_CheckPlayerstateEvents(ps, ops);

    if (ps->viewheight != ops->viewheight) {
        cg.duckChange = ps->viewheight - ops->viewheight;
        cg.duckTime = cg.time;
    }
}
