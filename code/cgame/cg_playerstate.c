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
// cg_playerstate.c -- this file acts on changes in a new playerState_t
// With normal play, this will be done after local prediction, but when
// following another player or playing back a demo, it will be checked
// when the snapshot transitions like all the other entities

#include "cg_local.h"

/*
==============
CG_CheckAmmo

If the ammo has gone low enough to generate the warning, play a sound

[QL] cgamex86.dll 0x10042e50. Only the weapon in hand counts: it is low under
cg_lowAmmoWarningPercentile of its ammo cap and empty at 0. Running out sounds noammo;
running low sounds lowammo with cg_lowAmmoWarningSound 1, noammo with 2, nothing otherwise.
==============
*/
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

/*
==============
CG_DamageFeedback
==============
*/
void CG_DamageFeedback(int yawByte, int pitchByte, int damage) {
    float left, front, up;
    float kick;
    int health;
    float scale;
    vec3_t dir;
    vec3_t angles;
    float dist;
    float yaw, pitch;

    // show the attacking player's head and name in corner
    cg.attackerTime = cg.time;

    // the lower on health you are, the greater the view kick will be
    health = cg.snap->ps.stats[STAT_HEALTH];
    if (health < 40) {
        scale = 1;
    } else {
        scale = 40.0 / health;
    }

    kick = damage * scale;  // scale the kick by the damage taken

    if (kick < 5)
        kick = 5;
    if (kick > 10)
        kick = 10;

    kick = cg_kickScale.value * kick;  // allow user to vary the kick scale

    // if yaw and pitch are both 255, make the damage always centered (falling, etc)
    if (yawByte == 255 && pitchByte == 255) {
        cg.damageX = 0;
        cg.damageY = 0;
        cg.v_dmg_roll = 0;
        cg.v_dmg_pitch = -kick;
    } else {
        // positional
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

    // clamp the position
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

    // don't let the screen flashes vary as much
    if (kick > 10) {
        kick = 10;
    }
    cg.damageValue = kick;
    cg.v_dmg_time = cg.time + DAMAGE_TIME;
}

/*
================
CG_Respawn

A respawn happened this snapshot
================
*/
void CG_Respawn(void) {
    // no error decay on player movement
    cg.thisFrameTeleport = qtrue;

    // display weapons available
    cg.weaponSelectTime = cg.time;

    // select the weapon the server says we are using
    cg.weaponSelect = cg.snap->ps.weapon;

    // [QL] 0x1004340f: the pickup display starts over
    cg.itemPickup = 0;
    cg.itemPickupTime = 0;
    cg.itemPickupLast = 0;
    cg.itemPickupBlendTime = 0;
    cg.itemPickupCount = 1;

    // [QL] mirror loadout primary into ROM cvar so HUD/UI elements can read
    // the actual server-side selection. Matches qagame cgamex86.dll CG_Respawn.
    trap_Cvar_Set("cg_weaponPrimary", va("%i", cg.snap->ps.weaponPrimary));

    // [QL] Race: reset checkpoints on respawn
    if (cgs.gametype == GT_RACE) {
        memset(&cg.race, 0, sizeof(cg.race));
        cg.race.nextCheckpointEnt = -1;
        cg.race.nextNextCheckpointEnt = -1;
        trap_SendConsoleCommand("raceinit\n");
    }

    // [QL] Spectator item timers: request current item respawn state
    if (cg.snap->ps.eFlags & EF_VOTED) {
        trap_SendConsoleCommand("specresp\n");
    }
}

extern char* eventnames[];

/*
==============
CG_CheckPlayerstateEvents
==============
*/
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

    cent = &cg.predictedPlayerEntity;  // cg_entities[ ps->clientNum ];
    // go through the predictable events buffer
    for (i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++) {
        // if we have a new predictable event
        if (i >= ops->eventSequence
            // or the server told us to play another event instead of a predicted event we already issued
            // or something the server told us changed our prediction causing a different event
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

/*
==================
CG_CheckChangedPredictableEvents
==================
*/
void CG_CheckChangedPredictableEvents(playerState_t* ps) {
    int i;
    int event;
    centity_t* cent;

    cent = &cg.predictedPlayerEntity;
    for (i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++) {
        //
        if (i >= cg.eventSequence) {
            continue;
        }
        // if this event is not further back in than the maximum predictable events we remember
        if (i > cg.eventSequence - MAX_PREDICTED_EVENTS) {
            // if the new playerstate event is different from a previously predicted one
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

/*
==================
pushReward
==================
*/
// [QL] 0x10043570; EV_AWARD pushes through here too
void CG_PushReward(sfxHandle_t sfx, qhandle_t shader, int rewardCount) {
    if (cg.rewardStack < (MAX_REWARDSTACK - 1)) {
        cg.rewardStack++;
        cg.rewardSound[cg.rewardStack] = sfx;
        cg.rewardShader[cg.rewardStack] = shader;
        cg.rewardCount[cg.rewardStack] = rewardCount;
    }
}

/*
==================
CG_CheckHitSounds

[ioquakelive] Not retail's: the hit sounds follow WolfcamQL, by the user's choice. Retail
(cgamex86.dll 0x100435b0) plays them from CG_CheckLocalSounds, which live play reaches through
the prediction; the predicted state takes each new snapshot the moment its packet lands, so the
sounds come at the pace the packets arrive and a lightning gun's hits bunch up and spread out.
WolfcamQL plays them as the snapshots are transitioned, on the game's own clock, and that is how
they are played here, from CG_TransitionSnapshot.

What plays is WolfcamQL's too (cg_playerstate.c, CG_CheckLocalSounds): nothing on the snapshot
the team changes; a hit plays cg_hitBeep's sound, 1 hit.wav, 2 hit0-3 by the damage tier the
server keeps in generic1 (generic1 / 64), 3 the same tiers reversed; a teammate hit plays
hit_teammate unless cg_hitBeep is 0. The kill beep is not here: WolfcamQL plays it from the
obituary, on top of the hit (CG_Obituary).
==================
*/
void CG_CheckHitSounds(const playerState_t* ps, const playerState_t* ops) {
    sfxHandle_t sfx;

    // don't play the sounds if the player just changed teams
    if (ps->persistant[PERS_TEAM] != ops->persistant[PERS_TEAM]) {
        return;
    }

    if (ps->persistant[PERS_HITS] > ops->persistant[PERS_HITS]) {
        if (cg_hitBeep.integer == 1) {
            trap_S_StartLocalSound(cgs.media.hitSound, CHAN_LOCAL_SOUND);
        } else if (cg_hitBeep.integer == 2) {
            switch (ps->generic1 / 64) {
            case 1:
                sfx = cgs.media.hitSounds[1];
                break;
            case 2:
                sfx = cgs.media.hitSounds[2];
                break;
            case 3:
                sfx = cgs.media.hitSounds[3];
                break;
            default:
                sfx = cgs.media.hitSounds[0];
                break;
            }
            trap_S_StartLocalSound(sfx, CHAN_LOCAL_SOUND);
        } else if (cg_hitBeep.integer == 3) {
            switch (ps->generic1 / 64) {
            case 0:
                sfx = cgs.media.hitSounds[3];
                break;
            case 1:
                sfx = cgs.media.hitSounds[2];
                break;
            case 2:
                sfx = cgs.media.hitSounds[1];
                break;
            default:
                sfx = cgs.media.hitSounds[0];
                break;
            }
            trap_S_StartLocalSound(sfx, CHAN_LOCAL_SOUND);
        }
    } else if (ps->persistant[PERS_HITS] < ops->persistant[PERS_HITS] && cg_hitBeep.integer) {
        trap_S_StartLocalSound(cgs.media.hitTeamSound, CHAN_LOCAL_SOUND);
    }
}

/*
==================
CG_CheckLocalSounds

[QL] cgamex86.dll 0x100435b0. Nothing plays on the snapshot the spawn count changes, which a
team change also does. The hit sounds that open retail's version are played apart, as WolfcamQL
plays them (CG_CheckHitSounds).

Then come the medals, the player event voices and the flag pickup, which stop once the
intermission has started, then the lead changes and the time and frag limit warnings, which
stop during warmup. The player's own pain sound comes from EV_PAIN.
==================
*/
void CG_CheckLocalSounds(playerState_t* ps, playerState_t* ops) {
    int count, bits;
    sfxHandle_t sfx;

    // don't play the sounds if the player just respawned or changed teams
    if (ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT]) {
        return;
    }

    // if we are going into the intermission, don't start any voices
    if (cg.intermissionStarted == 1) {
        return;
    }

    // medals, each only when its count moved to something other than 0; the second and
    // later impressive, excellent and gauntlet medals pick one of three voices
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

    // if any of the player event bits changed
    bits = ps->persistant[PERS_PLAYEREVENTS] ^ ops->persistant[PERS_PLAYEREVENTS];
    if (bits & PLAYEREVENT_DENIEDREWARD) {
        CG_AddBufferedSound(cgs.media.deniedSound);
    } else if (bits & PLAYEREVENT_GAUNTLETREWARD) {
        CG_AddBufferedSound(cgs.media.humiliationSounds[rand() % 3]);
    } else if (bits & PLAYEREVENT_HOLYSHIT) {
        CG_AddBufferedSound(cgs.media.holyShitSound);
    }

    // check for flag pickup
    if ((cgs.gametype >= GT_CTF && cgs.gametype <= GT_HARVESTER) || cgs.gametype == GT_AD) {
        if ((ps->powerups[PW_REDFLAG] != ops->powerups[PW_REDFLAG] && ps->powerups[PW_REDFLAG]) ||
            (ps->powerups[PW_BLUEFLAG] != ops->powerups[PW_BLUEFLAG] && ps->powerups[PW_BLUEFLAG]) ||
            (ps->powerups[PW_NEUTRALFLAG] != ops->powerups[PW_NEUTRALFLAG] && ps->powerups[PW_NEUTRALFLAG])) {
            CG_AddBufferedSound(cgs.media.youHaveFlagSound);
        }
    }

    // never play lead changes or limit warnings during warmup
    if (cg.warmup) {
        return;
    }

    // lead changes, skipped on the snapshot that brings the first frag
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

    // timelimit warnings, not while the game is paused and only for a limit the player had not
    // already passed when they closed the join menu
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
            // [QL] thirty seconds left in the round
            roundLeft = cgs.roundtimelimit * 1000 - cg.time + cgs.roundStartTime;
            if ((unsigned)(roundLeft - 1) <= 29998) {
                cg.timelimitWarnings |= 8;
                CG_AddBufferedSound(cgs.media.thirtySecondWarningSound);
            }
        }
    }

    // fraglimit warnings, from the first score only: in team deathmatch that is red's
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

/*
===============
CG_TransitionPlayerState

[QL] cgamex86.dll 0x10043b60. The damage time is the client time of the snapshot that brought
the damage, and a map restart respawns the player once whatever the spawn count says.
===============
*/
void CG_TransitionPlayerState(playerState_t* ps, playerState_t* ops) {
    // check for changing follow mode
    if (ps->clientNum != ops->clientNum) {
        cg.thisFrameTeleport = qtrue;
        // make sure we don't get any unwanted transition effects
        *ops = *ps;
    }

    // damage events (player is getting wounded)
    if (ps->damageEvent != ops->damageEvent && ps->damageCount) {
        cg.damageTime = (float)cg.time;
        CG_DamageFeedback(ps->damageYaw, ps->damagePitch, ps->damageCount);
    }

    // respawning
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
        // [QL] 0x10043c1d, right after the call: the crosshair hit effect starts on any hit,
        // even on the snapshot the local sounds skip
        if (ps->persistant[PERS_HITS] > ops->persistant[PERS_HITS]) {
            cg.crosshairHitType = (ps->generic1 >> 6) + 1;
            cg.crosshairHitTime = cg.time;
        }
    }

    // check for going low on ammo
    CG_CheckAmmo();

    // run events
    CG_CheckPlayerstateEvents(ps, ops);

    // smooth the ducking viewheight change
    if (ps->viewheight != ops->viewheight) {
        cg.duckChange = ps->viewheight - ops->viewheight;
        cg.duckTime = cg.time;
    }
}
