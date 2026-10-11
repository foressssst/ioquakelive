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

level_locals_t level;

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
    int modCount;
    void (*onChanged)(void);
} cvarTable_t;

gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];

gameImport_t *imports;

vmCvar_t g_gametype;
vmCvar_t g_dmflags;
vmCvar_t g_fraglimit;
vmCvar_t g_timelimit;
vmCvar_t g_capturelimit;
vmCvar_t g_friendlyFire;
vmCvar_t g_password;
vmCvar_t g_needpass;
vmCvar_t g_maxclients;
vmCvar_t g_dedicated;
vmCvar_t g_speed;
vmCvar_t g_gravity;
vmCvar_t g_cheats;
vmCvar_t g_knockback;
vmCvar_t g_inactivity;
vmCvar_t g_inactivityWarning;
vmCvar_t g_dropInactive;
vmCvar_t g_debugInactivity;
vmCvar_t g_debugMove;
vmCvar_t g_debugDamage;
vmCvar_t g_debugAlloc;
vmCvar_t g_weaponRespawn;
vmCvar_t g_motd;
vmCvar_t g_warmup;
vmCvar_t g_doWarmup;
vmCvar_t g_restarted;
vmCvar_t g_logfile;
vmCvar_t g_logfileSync;
vmCvar_t g_podiumDist;
vmCvar_t g_podiumDrop;
vmCvar_t g_allowVote;
vmCvar_t g_allowVoteMidGame;
vmCvar_t g_allowSpecVote;
vmCvar_t g_voteFlags;
vmCvar_t g_voteDelay;
vmCvar_t g_voteLimit;
vmCvar_t g_teamAutoJoin;
vmCvar_t g_teamForceBalance;
vmCvar_t g_banIPs;
vmCvar_t g_filterBan;
vmCvar_t g_knockback_z;
vmCvar_t g_knockback_z_self;
vmCvar_t g_knockback_cripple;
vmCvar_t g_obeliskHealth;
vmCvar_t g_obeliskRegenPeriod;
vmCvar_t g_obeliskRegenAmount;
vmCvar_t g_obeliskRespawnDelay;
vmCvar_t g_cubeTimeout;
vmCvar_t g_enableDust;
vmCvar_t g_proxMineTimeout;
vmCvar_t g_infiniteAmmo;
vmCvar_t g_runes;

vmCvar_t bot_breakPoint;
vmCvar_t bot_debugVar;
vmCvar_t bot_dynamicSkill;
vmCvar_t bot_followDist;
vmCvar_t bot_followMe;
vmCvar_t bot_gauntlet;
vmCvar_t bot_gauntletOnly;
vmCvar_t bot_hud;
vmCvar_t bot_instaGibAimSkill;
vmCvar_t bot_itemDelayTime;
vmCvar_t bot_showAreaNumber;
vmCvar_t bot_showAreas;
vmCvar_t bot_showAvoidSpots;
vmCvar_t bot_showPath;
vmCvar_t bot_showTourPoints;
vmCvar_t bot_startingSkill;
vmCvar_t bot_teamkill;
vmCvar_t bot_training;
vmCvar_t g_accessFile;
vmCvar_t g_adTouchScoreBonus;
vmCvar_t g_allTalk;
vmCvar_t g_allowCustomHeadmodels;
vmCvar_t g_allowForfeit;
vmCvar_t g_allowKill;
vmCvar_t g_ammoPackHack;
vmCvar_t g_autoAction;
vmCvar_t g_battleSuitDampen;
vmCvar_t g_bestStartingWeapons;
vmCvar_t g_botSpawnList;
vmCvar_t g_complaintDamageThreshold;
vmCvar_t g_complaintLimit;
vmCvar_t g_customSettings;
vmCvar_t g_debugFlags;
vmCvar_t g_debugThawTime;
vmCvar_t g_debugVampiricDamage;
vmCvar_t g_domCapTime;
vmCvar_t g_domDistressThreshold;
vmCvar_t g_domEnableContention;
vmCvar_t g_domNeutralFlag;
vmCvar_t g_domScoreRate;
vmCvar_t g_domTeammateCapScale;
vmCvar_t g_dropCmds;
vmCvar_t g_dropDamagedHealth;
vmCvar_t g_dropPowerups;
vmCvar_t g_dropSkulls;
vmCvar_t g_droppedFlagBonus;
vmCvar_t g_droppedPowerupsDecay;
vmCvar_t g_enableDebugTrace;
vmCvar_t g_enemyTeamRespawnRatio;
vmCvar_t g_factory;
vmCvar_t g_factoryTitle;
vmCvar_t g_flagBounce;
vmCvar_t g_flagPhysics;
vmCvar_t g_flightRefuelRate;
vmCvar_t g_flightThrust;
vmCvar_t g_floodprot_decay;
vmCvar_t g_floodprot_maxcount;
vmCvar_t g_forceAtmosphericEffects;
vmCvar_t g_forceSendConfigstring;
vmCvar_t g_forceSmallScoreboardMessage;
vmCvar_t g_freezeEnvironmentalRespawnDelay;
vmCvar_t g_friendlyFireDampen;
vmCvar_t g_gauntletSpeedFactor;
vmCvar_t g_grantItemOnSpawn;
vmCvar_t g_instaGib;
vmCvar_t g_kamiAttenuate;
vmCvar_t g_kamiMinRatio;
vmCvar_t g_kickBadUserinfo;
vmCvar_t g_latchedHookOffset;
vmCvar_t g_levelStartTime;
vmCvar_t g_lightningDischarge;
vmCvar_t g_maxDeferredSpawns;
vmCvar_t g_maxFlightFuel;
vmCvar_t g_midAirMinHeight;
vmCvar_t g_neutralFlagPingRate;
vmCvar_t g_playerModelScale;
vmCvar_t g_playerheadScale;
vmCvar_t g_playerheadScaleOffset;
vmCvar_t g_playerheadmodelOverride;
vmCvar_t g_playermodelOverride;
vmCvar_t g_powerupRespawn;
vmCvar_t g_quadHogIdle;
vmCvar_t g_quadHogPingRate;
vmCvar_t g_regenArmor;
vmCvar_t g_regenArmorAfterHealth;
vmCvar_t g_regenArmorRate;
vmCvar_t g_regenHealth;
vmCvar_t g_regenHealthRate;
vmCvar_t g_respawn_delay_max;
vmCvar_t g_respawn_delay_min;
vmCvar_t g_returnFlagOnSuicide;
vmCvar_t g_rrRoundScoreBonus;
vmCvar_t g_shuffle_automatic;
vmCvar_t g_shuffle_automatic_minplayers;
vmCvar_t g_shuffle_minplayers;
vmCvar_t g_shuffle_timedelay;
vmCvar_t g_skipTrainingEnable;
vmCvar_t g_trainingSpanish;
vmCvar_t g_spawnArmorDmgScale;
vmCvar_t g_spawnDelayRandom_key;
vmCvar_t g_spawnDelayRandom_powerup;
vmCvar_t g_spawnDelay_key;
vmCvar_t g_spawnDelay_powerup;
vmCvar_t g_spawnItemArmor;
vmCvar_t g_spawnItemHealth;
vmCvar_t g_spawnItemHoldable;
vmCvar_t g_spawnItemPowerup;
vmCvar_t g_spawnItemWeapons;
vmCvar_t g_spawnMinDistance;
vmCvar_t g_spawnRandomRatio;
vmCvar_t g_suddenDeathRespawn;
vmCvar_t g_suddenDeathRespawnIncrement;
vmCvar_t g_suddenDeathRespawnMax;
vmCvar_t g_suddenDeathRespawnPrint;
vmCvar_t g_suddenDeathRespawnStart;
vmCvar_t g_suddenDeathRespawnTick;
vmCvar_t g_switchTeamDelay;
vmCvar_t g_tackleFlag;
vmCvar_t g_teamSpawnAsSpec;
vmCvar_t g_teamSpecFreeCam;
vmCvar_t g_teamSpecSayEnable;
vmCvar_t g_throwFlagForwardMult;
vmCvar_t g_throwFlagVelocity;
vmCvar_t g_timeoutLen;
vmCvar_t g_vampiricDamage;
vmCvar_t gamedate;
vmCvar_t g_version;
vmCvar_t practiceflags;
vmCvar_t g_advancedTraining;
vmCvar_t g_trainingSpawn;
vmCvar_t g_trainingTarget;
vmCvar_t sv_mapname;
vmCvar_t ui_singlePlayerActive;
vmCvar_t weapon_reload_gauntlet;
vmCvar_t weapon_reload_hook;
vmCvar_t g_damage_g;
vmCvar_t g_damage_mg;
vmCvar_t g_damage_sg;
vmCvar_t g_damage_gl;
vmCvar_t g_damage_rl;
vmCvar_t g_damage_lg;
vmCvar_t g_damage_rg;
vmCvar_t g_damage_pg;
vmCvar_t g_damage_bfg;
vmCvar_t g_damage_gh;
vmCvar_t g_damage_ng;
vmCvar_t g_damage_cg;
vmCvar_t g_damage_hmg;
vmCvar_t g_damage_pl;

vmCvar_t g_knockback_g;
vmCvar_t g_knockback_mg;
vmCvar_t g_knockback_sg;
vmCvar_t g_knockback_gl;
vmCvar_t g_knockback_rl;
vmCvar_t g_knockback_rl_self;
vmCvar_t g_knockback_lg;
vmCvar_t g_knockback_rg;
vmCvar_t g_knockback_pg;
vmCvar_t g_knockback_pg_self;
vmCvar_t g_knockback_bfg;
vmCvar_t g_knockback_gh;
vmCvar_t g_knockback_ng;
vmCvar_t g_knockback_pl;
vmCvar_t g_knockback_cg;
vmCvar_t g_knockback_hmg;

vmCvar_t weapon_reload_mg;
vmCvar_t weapon_reload_sg;
vmCvar_t weapon_reload_gl;
vmCvar_t weapon_reload_rl;
vmCvar_t weapon_reload_lg;
vmCvar_t weapon_reload_rg;
vmCvar_t weapon_reload_pg;
vmCvar_t weapon_reload_bfg;
vmCvar_t weapon_reload_gh;
vmCvar_t weapon_reload_ng;
vmCvar_t weapon_reload_prox;
vmCvar_t weapon_reload_cg;
vmCvar_t weapon_reload_hmg;

vmCvar_t g_splashdamage_gl;
vmCvar_t g_splashdamage_rl;
vmCvar_t g_splashdamage_pg;
vmCvar_t g_splashdamage_bfg;
vmCvar_t g_splashdamage_pl;
vmCvar_t g_splashdamageOffset;
vmCvar_t g_rocketsplashOffset;

vmCvar_t g_max_knockback;

vmCvar_t g_splashradius_gl;
vmCvar_t g_splashradius_rl;
vmCvar_t g_splashradius_pg;
vmCvar_t g_splashradius_bfg;
vmCvar_t g_splashradius_pl;

vmCvar_t g_velocity_gl;
vmCvar_t g_velocity_rl;
vmCvar_t g_velocity_pg;
vmCvar_t g_velocity_bfg;
vmCvar_t g_velocity_gh;

vmCvar_t g_accelFactor_rl;
vmCvar_t g_accelFactor_pg;
vmCvar_t g_accelFactor_bfg;
vmCvar_t g_accelRate_rl;
vmCvar_t g_accelRate_pg;
vmCvar_t g_accelRate_bfg;

vmCvar_t weapon_gravity_rl;
vmCvar_t weapon_gravity_pg;
vmCvar_t weapon_gravity_bfg;
vmCvar_t weapon_gravity_ng;

vmCvar_t g_nailspeed;
vmCvar_t g_nailcount;
vmCvar_t g_nailspread;
vmCvar_t g_nailbounce;
vmCvar_t g_nailbouncepercentage;

vmCvar_t g_guidedRocket;

vmCvar_t g_loadout;
vmCvar_t g_disableLoadout;

vmCvar_t g_ammoPack;
vmCvar_t g_ammoRespawn;
vmCvar_t g_spawnItemAmmo;

vmCvar_t g_gameState;
vmCvar_t sv_warmupReadyPercentage;
vmCvar_t g_warmupDelay;
vmCvar_t g_warmupReadyDelay;
vmCvar_t g_warmupReadyDelayAction;
vmCvar_t g_lastManStandingMessage;
vmCvar_t bot_autoReady;
vmCvar_t g_teamForcePresent;

vmCvar_t g_teamsize;
vmCvar_t g_teamSizeMin;
vmCvar_t g_overtime;
vmCvar_t g_scorelimit;
vmCvar_t g_mercylimit;
vmCvar_t g_mercytime;
vmCvar_t g_rrAllowNegativeScores;
vmCvar_t sv_quitOnExitLevel;
vmCvar_t g_training;
vmCvar_t g_itemHeight;
vmCvar_t g_itemTimers;
vmCvar_t g_specItemTimers;
vmCvar_t g_quadDamageFactor;
vmCvar_t g_freezeRoundDelay;
vmCvar_t g_timeoutCount;

vmCvar_t g_forceDmgThroughSurface;
vmCvar_t g_dmgThroughSurfaceDistance;
vmCvar_t g_dmgThroughSurfaceDampening;
vmCvar_t g_dmgThroughSurfaceAngularThreshold;

vmCvar_t g_playerCylinders;

vmCvar_t sv_fps;

vmCvar_t roundtimelimit;
vmCvar_t g_roundWarmupDelay;
vmCvar_t roundlimit;
vmCvar_t g_lastManStandingWarning;
vmCvar_t g_roundDrawLivingCount;
vmCvar_t g_roundDrawHealthCount;
vmCvar_t g_spawnArmor;
vmCvar_t g_adElimScoreBonus;
vmCvar_t g_adCaptureScoreBonus;

vmCvar_t g_freezeThawTick;
vmCvar_t g_freezeProtectedSpawnTime;
vmCvar_t g_freezeThawTime;
vmCvar_t g_freezeAutoThawTime;
vmCvar_t g_freezeThawRadius;
vmCvar_t g_freezeThawThroughSurface;
vmCvar_t g_freezeThawWinningTeam;
vmCvar_t g_freezeRemovePowerupsOnRound;
vmCvar_t g_freezeResetHealthOnRound;
vmCvar_t g_freezeResetArmorOnRound;
vmCvar_t g_freezeResetWeaponsOnRound;

vmCvar_t g_lagHaxHistory;
vmCvar_t g_lagHaxMs;

vmCvar_t g_ironsights_mg;

vmCvar_t g_quadHog;
vmCvar_t g_quadHogTime;
vmCvar_t g_damagePlums;

vmCvar_t g_damage_sg_outer;
vmCvar_t g_damage_sg_falloff;
vmCvar_t g_range_sg_falloff;
vmCvar_t g_damage_lg_falloff;
vmCvar_t g_range_lg_falloff;
vmCvar_t g_headShotDamage_rg;
vmCvar_t g_railJump;

vmCvar_t g_rrInfected;
vmCvar_t g_rrInfectedSpreadTime;
vmCvar_t g_rrInfectedSpreadWarningTime;
vmCvar_t g_rrInfectedSurvivorScoreMethod;
vmCvar_t g_rrInfectedSurvivorScoreRate;
vmCvar_t g_rrInfectedSurvivorScoreBonus;
vmCvar_t g_rrInfectedSurvivorMinSpeed;
vmCvar_t g_rrInfectedSurvivorPingRate;
vmCvar_t g_rrInfectedZombieSpeed;
vmCvar_t g_rrInfectedZombieHealthBonus;
vmCvar_t g_rrInfectedZombieFragBonus;
vmCvar_t g_rrDamageScoreBonus;
vmCvar_t g_rrDeathScorePenalty;

vmCvar_t armor_tiered;

vmCvar_t g_startingWeapons;
vmCvar_t g_startingHealth;
vmCvar_t g_startingHealthBonus;
vmCvar_t g_startingArmor;
vmCvar_t g_startingAmmo_g;
vmCvar_t g_startingAmmo_mg;
vmCvar_t g_startingAmmo_sg;
vmCvar_t g_startingAmmo_gl;
vmCvar_t g_startingAmmo_rl;
vmCvar_t g_startingAmmo_lg;
vmCvar_t g_startingAmmo_rg;
vmCvar_t g_startingAmmo_pg;
vmCvar_t g_startingAmmo_bfg;
vmCvar_t g_startingAmmo_gh;
vmCvar_t g_startingAmmo_ng;
vmCvar_t g_startingAmmo_pl;
vmCvar_t g_startingAmmo_cg;
vmCvar_t g_startingAmmo_hmg;

vmCvar_t pmove_JumpVelocity;
vmCvar_t pmove_JumpVelocityMax;
vmCvar_t pmove_JumpVelocityScaleAdd;
vmCvar_t pmove_JumpVelocityTimeThreshold;
vmCvar_t pmove_JumpVelocityTimeThresholdOffset;
vmCvar_t pmove_ChainJump;
vmCvar_t pmove_ChainJumpVelocity;
vmCvar_t pmove_StepJumpVelocity;
vmCvar_t pmove_JumpTimeDeltaMin;
vmCvar_t pmove_RampJump;
vmCvar_t pmove_RampJumpScale;
vmCvar_t pmove_StepJump;
vmCvar_t pmove_CrouchStepJump;
vmCvar_t pmove_AutoHop;
vmCvar_t pmove_BunnyHop;
vmCvar_t pmove_AirAccel;
vmCvar_t pmove_AirStopAccel;
vmCvar_t pmove_AirControl;
vmCvar_t pmove_StrafeAccel;
vmCvar_t pmove_WalkAccel;
vmCvar_t pmove_WalkFriction;
vmCvar_t pmove_CircleStrafeFriction;
vmCvar_t pmove_CrouchSlideFriction;
vmCvar_t pmove_CrouchSlideTime;
vmCvar_t pmove_AirSteps;
vmCvar_t pmove_AirStepFriction;
vmCvar_t pmove_StepHeight;
vmCvar_t pmove_WaterSwimScale;
vmCvar_t pmove_WaterWadeScale;
vmCvar_t pmove_WishSpeed;
vmCvar_t pmove_WeaponDropTime;
vmCvar_t pmove_WeaponRaiseTime;
vmCvar_t pmove_NoPlayerClip;
vmCvar_t pmove_DoubleJump;
vmCvar_t pmove_CrouchSlide;
vmCvar_t pmove_VelocityGH;

static qboolean pmoveInfoDirty = qtrue;
static qboolean armorInfoDirty = qtrue;
static qboolean weaponInfoDirty = qtrue;
static qboolean playerInfoDirty = qtrue;

static void OnChangedStartingWeapons(void) {
    trap_SetConfigstring(CS_STARTING_WEAPONS, va("%i", g_startingWeapons.integer));
}

static void OnChangedModelOverride(void) {
    int i;
    for (i = 0; i < level.maxclients; i++) {
        if (g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED) {
            ClientUserinfoChanged(i);
        }
    }
}

static void OnChangedInfiniteAmmo(void) {
    int i, w;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t* client = g_entities[i].client;
        if (!client || client->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (g_infiniteAmmo.integer) {
            for (w = 1; w < WP_NUM_WEAPONS; w++) {
                client->ps.ammo[w] = -1;
            }
        } else {
            for (w = 1; w < WP_NUM_WEAPONS; w++) {
                if (w != WP_GAUNTLET && w != WP_GRAPPLING_HOOK && client->ps.ammo[w] == -1) {
                    gitem_t* item = BG_FindItemForWeapon(w);
                    client->ps.ammo[w] = item ? item->quantity : 0;
                }
            }
        }
    }
}

static void OnChangedPmoveJumpVelocity(void)          { PM_SetJumpVelocity(pmove_JumpVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityMax(void)        { PM_SetJumpVelocityMax(pmove_JumpVelocityMax.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityScaleAdd(void)   { PM_SetJumpVelocityScaleAdd(pmove_JumpVelocityScaleAdd.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityTimeThreshold(void) { PM_SetJumpVelocityTimeThreshold(pmove_JumpVelocityTimeThreshold.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityTimeThresholdOffset(void) { PM_SetJumpVelocityTimeThresholdOffset(pmove_JumpVelocityTimeThresholdOffset.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveChainJump(void)              { PM_SetChainJump(pmove_ChainJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveChainJumpVelocity(void)      { PM_SetChainJumpVelocity(pmove_ChainJumpVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStepJumpVelocity(void)       { PM_SetStepJumpVelocity(pmove_StepJumpVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpTimeDeltaMin(void)       { PM_SetJumpTimeDeltaMin(pmove_JumpTimeDeltaMin.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveRampJump(void)               { PM_SetRampJump(pmove_RampJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveRampJumpScale(void)          { PM_SetRampJumpScale(pmove_RampJumpScale.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStepJump(void)               { PM_SetStepJump(pmove_StepJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchStepJump(void)         { PM_SetCrouchStepJump(pmove_CrouchStepJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAutoHop(void)                { PM_SetAutoHop(pmove_AutoHop.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveBunnyHop(void)               { PM_SetBunnyHop(pmove_BunnyHop.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirAccel(void)               { PM_SetAirAccel(pmove_AirAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirStopAccel(void)           { PM_SetAirStopAccel(pmove_AirStopAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirControl(void)             { PM_SetAirControl(pmove_AirControl.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStrafeAccel(void)            { PM_SetStrafeAccel(pmove_StrafeAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWalkAccel(void)              { PM_SetWalkAccel(pmove_WalkAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWalkFriction(void)           { PM_SetWalkFriction(pmove_WalkFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCircleStrafeFriction(void)   { PM_SetCircleStrafeFriction(pmove_CircleStrafeFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchSlideFriction(void)    { PM_SetCrouchSlideFriction(pmove_CrouchSlideFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchSlideTime(void)        { PM_SetCrouchSlideTime(pmove_CrouchSlideTime.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirSteps(void)               { PM_SetAirSteps(pmove_AirSteps.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirStepFriction(void)        { PM_SetAirStepFriction(pmove_AirStepFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStepHeight(void)             { PM_SetStepHeight(pmove_StepHeight.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWaterSwimScale(void)         { PM_SetWaterSwimScale(pmove_WaterSwimScale.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWaterWadeScale(void)         { PM_SetWaterWadeScale(pmove_WaterWadeScale.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWishSpeed(void)              { PM_SetWishSpeed(pmove_WishSpeed.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWeaponDropTime(void)         { PM_SetWeaponDropTime(pmove_WeaponDropTime.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWeaponRaiseTime(void)        { PM_SetWeaponRaiseTime(pmove_WeaponRaiseTime.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveNoPlayerClip(void)           { PM_SetNoPlayerClip(pmove_NoPlayerClip.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveDoubleJump(void)             { PM_SetDoubleJump(pmove_DoubleJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchSlide(void)            { PM_SetCrouchSlide(pmove_CrouchSlide.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveVelocityGH(void)             { PM_SetVelocityGH(pmove_VelocityGH.value); pmoveInfoDirty = qtrue; }

static void OnChangedWeaponReloadMG(void)   { BG_SetWeaponReload(WP_MACHINEGUN, weapon_reload_mg.integer); }
static void OnChangedWeaponReloadSG(void)   { BG_SetWeaponReload(WP_SHOTGUN, weapon_reload_sg.integer); }
static void OnChangedWeaponReloadGL(void)   { BG_SetWeaponReload(WP_GRENADE_LAUNCHER, weapon_reload_gl.integer); }
static void OnChangedWeaponReloadRL(void)   { BG_SetWeaponReload(WP_ROCKET_LAUNCHER, weapon_reload_rl.integer); }
static void OnChangedWeaponReloadLG(void)   { BG_SetWeaponReload(WP_LIGHTNING, weapon_reload_lg.integer); }
static void OnChangedWeaponReloadRG(void)   { BG_SetWeaponReload(WP_RAILGUN, weapon_reload_rg.integer); }
static void OnChangedWeaponReloadPG(void)   { BG_SetWeaponReload(WP_PLASMAGUN, weapon_reload_pg.integer); }
static void OnChangedWeaponReloadBFG(void)  { BG_SetWeaponReload(WP_BFG, weapon_reload_bfg.integer); }
static void OnChangedWeaponReloadGauntlet(void) { BG_SetWeaponReload(WP_GAUNTLET, weapon_reload_gauntlet.integer); }
static void OnChangedWeaponReloadGH(void)   { BG_SetWeaponReload(WP_GRAPPLING_HOOK, weapon_reload_gh.integer); }
static void OnChangedWeaponReloadNG(void)   { BG_SetWeaponReload(WP_NAILGUN, weapon_reload_ng.integer); }
static void OnChangedWeaponReloadProx(void) { BG_SetWeaponReload(WP_PROX_LAUNCHER, weapon_reload_prox.integer); }
static void OnChangedWeaponReloadCG(void)   { BG_SetWeaponReload(WP_CHAINGUN, weapon_reload_cg.integer); }
static void OnChangedWeaponReloadHMG(void)  { BG_SetWeaponReload(WP_HMG, weapon_reload_hmg.integer); }

static cvarTable_t gameCvarTable[] = {
    {&g_cheats, "sv_cheats", "", 0, 0, NULL},

    {&gamedate, "gamedate", "May 25 2016", CVAR_ROM, 0, NULL},
    {&g_version, "g_version", "1069 win-x86 May 25 2016 15:18:16", CVAR_ROM, 0, NULL},
    {&g_restarted, "g_restarted", "0", CVAR_ROM, 0, NULL},

    {&g_gametype, "g_gametype", "0", CVAR_ROM | CVAR_LATCH | CVAR_SERVERINFO, 0, NULL},

    {&g_maxclients, "sv_maxclients", "8", CVAR_LATCH | CVAR_SERVERINFO | CVAR_ARCHIVE, 0, NULL},

    {&g_dmflags, "dmflags", "0", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_fraglimit, "fraglimit", "50", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},
    {&g_timelimit, "timelimit", "0", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},
    {&g_capturelimit, "capturelimit", "8", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},

    {&g_friendlyFire, "g_friendlyFire", "0", CVAR_GAMERULE, 0, NULL},

    {&g_warmup, "g_warmup", "10", CVAR_ARCHIVE, 0, NULL},
    {&g_doWarmup, "g_doWarmup", "1", CVAR_ARCHIVE, 0, NULL},
    {&g_gameState, "g_gameState", "PRE_GAME", CVAR_ROM | CVAR_SERVERINFO, 0, NULL},
    {&sv_warmupReadyPercentage, "sv_warmupReadyPercentage", "0.51", CVAR_LATCH | CVAR_ARCHIVE, 0, NULL},
    {&g_warmupDelay, "g_warmupDelay", "15", 0, 0, NULL},
    {&g_warmupReadyDelay, "g_warmupReadyDelay", "0", 0, 0, NULL},
    {&g_warmupReadyDelayAction, "g_warmupReadyDelayAction", "1", 0, 0, NULL},
    {&g_lastManStandingMessage, "g_lastManStandingMessage", "You are the last standing", 0, 0, NULL},
    {&bot_autoReady, "bot_autoReady", "1", CVAR_GAMERULE, 0, NULL},
    {&g_teamForcePresent, "g_teamForcePresent", "1", 0, 0, NULL},
    {&g_logfile, "g_log", "", CVAR_ARCHIVE, 0, NULL},
    {&g_logfileSync, "g_logSync", "0", CVAR_ARCHIVE, 0, NULL},

    {&g_password, "g_password", "", CVAR_USERINFO, 0, NULL},

    {&g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, NULL},
    {&g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, NULL},

    {&g_needpass, "g_needpass", "0", CVAR_ROM | CVAR_SERVERINFO, 0, NULL},

    {&g_dedicated, "dedicated", "0", 0, 0, NULL},

    {&g_speed, "g_speed", "320", CVAR_GAMERULE, 0, NULL},
    {&g_gravity, "g_gravity", "800", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_knockback, "g_knockback", "1000", CVAR_GAMERULE, 0, NULL},
    {&g_weaponRespawn, "g_weaponRespawn", "5", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_inactivity, "g_inactivity", "0", 0, 0, NULL},
    {&g_inactivityWarning, "g_inactivityWarning", "10", 0, 0, NULL},
    {&g_dropInactive, "g_dropInactive", "1", 0, 0, NULL},
    {&g_debugInactivity, "g_debugInactivity", "0", 0, 0, NULL},
    {&g_debugMove, "g_debugMove", "0", 0, 0, NULL},
    {&g_debugDamage, "g_debugDamage", "0", 0, 0, NULL},
    {&g_debugAlloc, "g_debugAlloc", "0", 0, 0, NULL},
    {&g_motd, "g_motd", "", 0, 0, NULL},

    {&g_podiumDist, "g_podiumDist", "80", CVAR_GAMERULE, 0, NULL},
    {&g_podiumDrop, "g_podiumDrop", "70", CVAR_GAMERULE, 0, NULL},

    {&g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, NULL},
    {&g_allowVoteMidGame, "g_allowVoteMidGame", "0", 0, 0, NULL},
    {&g_allowSpecVote, "g_allowSpecVote", "0", 0, 0, NULL},
    {&g_voteFlags, "g_voteFlags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, NULL},
    {&g_voteDelay, "g_voteDelay", "0", 0, 0, NULL},
    {&g_voteLimit, "g_voteLimit", "0", 0, 0, NULL},

    {&g_obeliskHealth, "g_obeliskHealth", "2500", CVAR_GAMERULE, 0, NULL},
    {&g_obeliskRegenPeriod, "g_obeliskRegenPeriod", "1", CVAR_GAMERULE, 0, NULL},
    {&g_obeliskRegenAmount, "g_obeliskRegenAmount", "15", CVAR_GAMERULE, 0, NULL},
    {&g_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", CVAR_GAMERULE, 0, NULL},

    {&g_cubeTimeout, "g_cubeTimeout", "30", CVAR_GAMERULE, 0, NULL},

    {&g_enableDust, "g_enableDust", "0", 0, 0, NULL},
    {&g_proxMineTimeout, "g_proxMineTimeout", "20", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_infiniteAmmo, "g_infiniteAmmo", "0", CVAR_GAMERULE, 0, OnChangedInfiniteAmmo},
    {&g_runes, "g_runes", "0", CVAR_GAMERULE | CVAR_LATCH, 0, NULL},

    {&g_damage_g, "g_damage_g", "50", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_mg, "g_damage_mg", "5", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_sg, "g_damage_sg", "5", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_gl, "g_damage_gl", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_rl, "g_damage_rl", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_lg, "g_damage_lg", "6", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_rg, "g_damage_rg", "80", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_pg, "g_damage_pg", "20", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_bfg, "g_damage_bfg", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_gh, "g_damage_gh", "10", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_ng, "g_damage_ng", "12", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_cg, "g_damage_cg", "8", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_hmg, "g_damage_hmg", "8", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_pl, "g_damage_pl", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_knockback_g, "g_knockback_g", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_mg, "g_knockback_mg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_sg, "g_knockback_sg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_gl, "g_knockback_gl", "1.10", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_rl, "g_knockback_rl", "0.90", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_rl_self, "g_knockback_rl_self", "1.10", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_lg, "g_knockback_lg", "1.75", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_rg, "g_knockback_rg", "0.85", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_pg, "g_knockback_pg", "1.10", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_pg_self, "g_knockback_pg_self", "1.30", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_bfg, "g_knockback_bfg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_gh, "g_knockback_gh", "-5", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_ng, "g_knockback_ng", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_pl, "g_knockback_pl", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_cg, "g_knockback_cg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_hmg, "g_knockback_hmg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_knockback_z, "g_knockback_z", "24", CVAR_GAMERULE, 0, NULL},
    {&g_knockback_z_self, "g_knockback_z_self", "24", CVAR_GAMERULE, 0, NULL},
    {&g_knockback_cripple, "g_knockback_cripple", "0", CVAR_GAMERULE, 0, NULL},

    {&weapon_reload_mg, "weapon_reload_mg", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadMG},
    {&weapon_reload_sg, "weapon_reload_sg", "1000", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadSG},
    {&weapon_reload_gl, "weapon_reload_gl", "800", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadGL},
    {&weapon_reload_rl, "weapon_reload_rl", "800", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadRL},
    {&weapon_reload_lg, "weapon_reload_lg", "50", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadLG},
    {&weapon_reload_rg, "weapon_reload_rg", "1500", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadRG},
    {&weapon_reload_pg, "weapon_reload_pg", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadPG},
    {&weapon_reload_bfg, "weapon_reload_bfg", "300", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadBFG},
    {&weapon_reload_gh, "weapon_reload_gh", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadGH},
    {&weapon_reload_ng, "weapon_reload_ng", "1000", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadNG},
    {&weapon_reload_prox, "weapon_reload_prox", "800", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadProx},
    {&weapon_reload_cg, "weapon_reload_cg", "50", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadCG},
    {&weapon_reload_hmg, "weapon_reload_hmg", "75", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadHMG},

    {&g_splashdamage_gl, "g_splashdamage_gl", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashdamage_rl, "g_splashdamage_rl", "84", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashdamage_pg, "g_splashdamage_pg", "15", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashdamage_bfg, "g_splashdamage_bfg", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashdamage_pl, "g_splashdamage_pl", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashdamageOffset, "g_splashdamageOffset", "0.05", CVAR_GAMERULE, 0, NULL},
    {&g_rocketsplashOffset, "g_rocketsplashOffset", "-10.0", CVAR_GAMERULE, 0, NULL},

    {&g_max_knockback, "g_max_knockback", "120", CVAR_GAMERULE, 0, NULL},

    {&g_splashradius_gl, "g_splashradius_gl", "150", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashradius_rl, "g_splashradius_rl", "120", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashradius_pg, "g_splashradius_pg", "20", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashradius_bfg, "g_splashradius_bfg", "80", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_splashradius_pl, "g_splashradius_pl", "150", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_velocity_gl, "g_velocity_gl", "700", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_velocity_rl, "g_velocity_rl", "1000", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_velocity_pg, "g_velocity_pg", "2000", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_velocity_bfg, "g_velocity_bfg", "1800", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_velocity_gh, "g_velocity_gh", "1800", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_accelFactor_rl, "g_accelFactor_rl", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_accelFactor_pg, "g_accelFactor_pg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_accelFactor_bfg, "g_accelFactor_bfg", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_accelRate_rl, "g_accelRate_rl", "16", CVAR_GAMERULE, 0, NULL},
    {&g_accelRate_pg, "g_accelRate_pg", "16", CVAR_GAMERULE, 0, NULL},
    {&g_accelRate_bfg, "g_accelRate_bfg", "16", CVAR_GAMERULE, 0, NULL},

    {&weapon_gravity_rl, "weapon_gravity_rl", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, NULL},
    {&weapon_gravity_pg, "weapon_gravity_pg", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, NULL},
    {&weapon_gravity_bfg, "weapon_gravity_bfg", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, NULL},
    {&weapon_gravity_ng, "weapon_gravity_ng", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, NULL},

    {&g_nailspeed, "g_nailspeed", "1000", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_nailcount, "g_nailcount", "10", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_nailspread, "g_nailspread", "400", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_nailbounce, "g_nailbounce", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_nailbouncepercentage, "g_nailbouncepercentage", "65", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_guidedRocket, "g_guidedRocket", "0", CVAR_GAMERULE, 0, NULL},

    {&g_loadout, "g_loadout", "0", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_disableLoadout, "g_disableLoadout", "0", CVAR_GAMERULE, 0, NULL},

    {&g_ammoPack, "g_ammoPack", "0", CVAR_GAMERULE | CVAR_LATCH, 0, NULL},
    {&g_ammoRespawn, "g_ammoRespawn", "40", CVAR_GAMERULE, 0, NULL},
    {&g_spawnItemAmmo, "g_spawnItemAmmo", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_teamsize, "teamsize", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_teamSizeMin, "g_teamSizeMin", "1", CVAR_SERVERINFO, 0, NULL},
    {&g_overtime, "g_overtime", "120", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_scorelimit, "scorelimit", "150", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},
    {&g_mercylimit, "mercylimit", "0", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},
    {&g_mercytime, "g_mercytime", "0", CVAR_GAMERULE | CVAR_NORESTART, 0, NULL},
    {&g_rrAllowNegativeScores, "g_rrAllowNegativeScores", "0", CVAR_GAMERULE, 0, NULL},
    {&sv_quitOnExitLevel, "sv_quitOnExitLevel", "0", 0, 0, NULL},
    {&g_training, "g_training", "0", CVAR_GAMERULE | CVAR_SYSTEMINFO, 0, NULL},
    {&g_itemHeight, "g_itemHeight", "35", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_itemTimers, "g_itemTimers", "1", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_specItemTimers, "g_specItemTimers", "1", 0, 0, NULL},
    {&g_quadDamageFactor, "g_quadDamageFactor", "3", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_freezeRoundDelay, "g_freezeRoundDelay", "4000", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_timeoutCount, "g_timeoutCount", "0", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},

    {&g_startingWeapons, "g_startingWeapons", "3", CVAR_GAMERULE, 0, OnChangedStartingWeapons},
    {&g_startingHealth, "g_startingHealth", "100", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_startingHealthBonus, "g_startingHealthBonus", "25", CVAR_GAMERULE, 0, NULL},
    {&g_startingArmor, "g_startingArmor", "0", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_g, "g_startingAmmo_g", "-1", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_mg, "g_startingAmmo_mg", "100", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_sg, "g_startingAmmo_sg", "10", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_gl, "g_startingAmmo_gl", "10", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_rl, "g_startingAmmo_rl", "5", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_lg, "g_startingAmmo_lg", "100", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_rg, "g_startingAmmo_rg", "5", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_pg, "g_startingAmmo_pg", "50", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_bfg, "g_startingAmmo_bfg", "10", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_gh, "g_startingAmmo_gh", "-1", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_ng, "g_startingAmmo_ng", "10", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_pl, "g_startingAmmo_pl", "5", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_cg, "g_startingAmmo_cg", "100", CVAR_GAMERULE, 0, NULL},
    {&g_startingAmmo_hmg, "g_startingAmmo_hmg", "50", CVAR_GAMERULE, 0, NULL},

    {&pmove_JumpVelocity, "pmove_JumpVelocity", "275.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveJumpVelocity},
    {&pmove_JumpVelocityMax, "pmove_JumpVelocityMax", "700.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveJumpVelocityMax},
    {&pmove_JumpVelocityScaleAdd, "pmove_JumpVelocityScaleAdd", "0.4f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveJumpVelocityScaleAdd},
    {&pmove_JumpVelocityTimeThreshold, "pmove_JumpVelocityTimeThreshold", "500.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveJumpVelocityTimeThreshold},
    {&pmove_JumpVelocityTimeThresholdOffset, "pmove_JumpVelocityTimeThresholdOffset", "0.6f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveJumpVelocityTimeThresholdOffset},
    {&pmove_ChainJump, "pmove_ChainJump", "1", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveChainJump},
    {&pmove_ChainJumpVelocity, "pmove_ChainJumpVelocity", "110.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveChainJumpVelocity},
    {&pmove_StepJumpVelocity, "pmove_StepJumpVelocity", "48.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveStepJumpVelocity},
    {&pmove_JumpTimeDeltaMin, "pmove_JumpTimeDeltaMin", "100.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveJumpTimeDeltaMin},
    {&pmove_RampJump, "pmove_RampJump", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveRampJump},
    {&pmove_RampJumpScale, "pmove_RampJumpScale", "1.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveRampJumpScale},
    {&pmove_StepJump, "pmove_StepJump", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveStepJump},
    {&pmove_CrouchStepJump, "pmove_CrouchStepJump", "1", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveCrouchStepJump},
    {&pmove_AutoHop, "pmove_AutoHop", "1", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveAutoHop},
    {&pmove_BunnyHop, "pmove_BunnyHop", "1", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveBunnyHop},
    {&pmove_AirAccel, "pmove_AirAccel", "1.0f", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveAirAccel},
    {&pmove_AirStopAccel, "pmove_AirStopAccel", "1.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveAirStopAccel},
    {&pmove_AirControl, "pmove_AirControl", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, OnChangedPmoveAirControl},
    {&pmove_StrafeAccel, "pmove_StrafeAccel", "1.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveStrafeAccel},
    {&pmove_WalkAccel, "pmove_WalkAccel", "10.0f", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWalkAccel},
    {&pmove_WalkFriction, "pmove_WalkFriction", "6.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWalkFriction},
    {&pmove_CircleStrafeFriction, "pmove_CircleStrafeFriction", "6.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveCircleStrafeFriction},
    {&pmove_CrouchSlideFriction, "pmove_CrouchSlideFriction", "0.5", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveCrouchSlideFriction},
    {&pmove_CrouchSlideTime, "pmove_CrouchSlideTime", "2000", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveCrouchSlideTime},
    {&pmove_AirSteps, "pmove_AirSteps", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveAirSteps},
    {&pmove_AirStepFriction, "pmove_AirStepFriction", "0.03f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveAirStepFriction},
    {&pmove_StepHeight, "pmove_StepHeight", "22.0f", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveStepHeight},
    {&pmove_WaterSwimScale, "pmove_WaterSwimScale", "0.6f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWaterSwimScale},
    {&pmove_WaterWadeScale, "pmove_WaterWadeScale", "0.8f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWaterWadeScale},
    {&pmove_WishSpeed, "pmove_WishSpeed", "400.0f", CVAR_GAMERULE | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWishSpeed},
    {&pmove_WeaponDropTime, "pmove_WeaponDropTime", "200", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWeaponDropTime},
    {&pmove_WeaponRaiseTime, "pmove_WeaponRaiseTime", "200", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveWeaponRaiseTime},
    {&pmove_NoPlayerClip, "pmove_noPlayerClip", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveNoPlayerClip},
    {&pmove_DoubleJump, "pmove_DoubleJump", "0", CVAR_GAMERULE, 0, OnChangedPmoveDoubleJump},
    {&pmove_CrouchSlide, "pmove_CrouchSlide", "0", CVAR_GAMERULE, 0, OnChangedPmoveCrouchSlide},
    {&pmove_VelocityGH, "pmove_velocity_gh", "800", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_PMOVE, 0, OnChangedPmoveVelocityGH},

    {&g_forceDmgThroughSurface, "g_forceDmgThroughSurface", "0", CVAR_GAMERULE, 0, NULL},
    {&g_dmgThroughSurfaceDistance, "g_dmgThroughSurfaceDistance", "-33.1f", CVAR_GAMERULE, 0, NULL},
    {&g_dmgThroughSurfaceDampening, "g_dmgThroughSurfaceDampening", "0.5f", CVAR_GAMERULE, 0, NULL},
    {&g_dmgThroughSurfaceAngularThreshold, "g_dmgThroughSurfaceAngularThreshold", "0.5f", CVAR_GAMERULE, 0, NULL},

    {&g_playerCylinders, "g_playerCylinders", "1", CVAR_GAMERULE, 0, NULL},
    {&sv_fps, "sv_fps", "40", CVAR_ROM, 0, NULL},

    {&roundtimelimit, "roundtimelimit", "180", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},
    {&g_roundWarmupDelay, "g_roundWarmupDelay", "10000", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&roundlimit, "roundlimit", "10", CVAR_GAMERULE | CVAR_NORESTART | CVAR_SERVERINFO, 0, NULL},
    {&g_lastManStandingWarning, "g_lastManStandingWarning", "1", 0, 0, NULL},
    {&g_roundDrawLivingCount, "g_roundDrawLivingCount", "1", CVAR_GAMERULE, 0, NULL},
    {&g_roundDrawHealthCount, "g_roundDrawHealthCount", "1", CVAR_GAMERULE, 0, NULL},
    {&g_spawnArmor, "g_spawnArmor", "0", CVAR_GAMERULE, 0, NULL},
    {&g_adElimScoreBonus, "g_adElimScoreBonus", "2", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_adCaptureScoreBonus, "g_adCaptureScoreBonus", "3", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},

    {&g_freezeThawTick, "g_freezeThawTick", "1", CVAR_GAMERULE, 0, NULL},
    {&g_freezeProtectedSpawnTime, "g_freezeProtectedSpawnTime", "0", CVAR_GAMERULE, 0, NULL},
    {&g_freezeThawTime, "g_freezeThawTime", "2000", CVAR_GAMERULE, 0, NULL},
    {&g_freezeAutoThawTime, "g_freezeAutoThawTime", "120000", CVAR_GAMERULE, 0, NULL},
    {&g_freezeThawRadius, "g_freezeThawRadius", "96", CVAR_GAMERULE, 0, NULL},
    {&g_freezeThawThroughSurface, "g_freezeThawThroughSurface", "0", CVAR_GAMERULE, 0, NULL},
    {&g_freezeThawWinningTeam, "g_freezeThawWinningTeam", "1", CVAR_GAMERULE, 0, NULL},
    {&g_freezeRemovePowerupsOnRound, "g_freezeRemovePowerupsOnRound", "1", CVAR_GAMERULE, 0, NULL},
    {&g_freezeResetHealthOnRound, "g_freezeResetHealthOnRound", "1", CVAR_GAMERULE, 0, NULL},
    {&g_freezeResetArmorOnRound, "g_freezeResetArmorOnRound", "1", CVAR_GAMERULE, 0, NULL},
    {&g_freezeResetWeaponsOnRound, "g_freezeResetWeaponsOnRound", "1", CVAR_GAMERULE, 0, NULL},

    {&g_lagHaxHistory, "g_lagHaxHistory", "4", CVAR_LATCH, 0, NULL},
    {&g_lagHaxMs, "g_lagHaxMs", "80", CVAR_LATCH, 0, NULL},
    {&g_ironsights_mg, "g_ironsights_mg", "1.0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_TEMP, 0, NULL},

    {&g_quadHog, "g_quadHog", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_LATCH, 0, NULL},
    {&g_quadHogTime, "g_quadHogTime", "60", CVAR_GAMERULE, 0, NULL},
    {&g_damagePlums, "g_damagePlums", "2", CVAR_GAMERULE, 0, NULL},

    {&g_damage_sg_outer, "g_damage_sg_outer", "5", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_damage_sg_falloff, "g_damage_sg_falloff", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_range_sg_falloff, "g_range_sg_falloff", "768", CVAR_GAMERULE, 0, NULL},
    {&g_damage_lg_falloff, "g_damage_lg_falloff", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_range_lg_falloff, "g_range_lg_falloff", "768", CVAR_GAMERULE, 0, NULL},
    {&g_headShotDamage_rg, "g_headShotDamage_rg", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_railJump, "g_railJump", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},

    {&g_rrInfected, "g_rrInfected", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_LATCH, 0, NULL},
    {&g_rrInfectedSpreadTime, "g_rrInfectedSpreadTime", "40", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedSpreadWarningTime, "g_rrInfectedSpreadWarningTime", "10", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedSurvivorScoreMethod, "g_rrInfectedSurvivorScoreMethod", "2", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedSurvivorScoreRate, "g_rrInfectedSurvivorScoreRate", "30", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedSurvivorScoreBonus, "g_rrInfectedSurvivorScoreBonus", "1", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedSurvivorMinSpeed, "g_rrInfectedSurvivorMinSpeed", "500.0f", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedSurvivorPingRate, "g_rrInfectedSurvivorPingRate", "2000", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedZombieSpeed, "g_rrInfectedZombieSpeed", "1.15", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedZombieHealthBonus, "g_rrInfectedZombieHealthBonus", "50", CVAR_GAMERULE, 0, NULL},
    {&g_rrInfectedZombieFragBonus, "g_rrInfectedZombieFragBonus", "2", CVAR_GAMERULE, 0, NULL},
    {&g_rrDamageScoreBonus, "g_rrDamageScoreBonus", "0", CVAR_GAMERULE, 0, NULL},
    {&g_rrDeathScorePenalty, "g_rrDeathScorePenalty", "-1", CVAR_GAMERULE, 0, NULL},

    {&armor_tiered, "armor_tiered", "0", CVAR_GAMERULE | CVAR_GAMERULE_ARMOR, 0, NULL},
    {&bot_breakPoint, "bot_breakPoint", "0", 0, 0, NULL},
    {&bot_debugVar, "bot_debugVar", "0", 0, 0, NULL},
    {&bot_dynamicSkill, "bot_dynamicSkill", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_followDist, "bot_followDist", "250", CVAR_GAMERULE, 0, NULL},
    {&bot_followMe, "bot_followMe", "", CVAR_GAMERULE, 0, NULL},
    {&bot_gauntlet, "bot_gauntlet", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_gauntletOnly, "bot_gauntletOnly", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_hud, "bot_hud", "-1", CVAR_CHEAT, 0, NULL},
    {&bot_instaGibAimSkill, "bot_instaGibAimSkill", "0.4", CVAR_GAMERULE, 0, NULL},
    {&bot_itemDelayTime, "bot_itemDelayTime", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_showAreaNumber, "bot_showAreaNumber", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_showAreas, "bot_showAreas", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_showAvoidSpots, "bot_showAvoidSpots", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_showPath, "bot_showPath", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_showTourPoints, "bot_showTourPoints", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_startingSkill, "bot_startingSkill", "1", CVAR_GAMERULE, 0, NULL},
    {&bot_teamkill, "bot_teamkill", "0", CVAR_GAMERULE, 0, NULL},
    {&bot_training, "bot_training", "0", CVAR_GAMERULE, 0, NULL},
    {&g_accessFile, "g_accessFile", "access.txt", 0, 0, NULL},
    {&g_adTouchScoreBonus, "g_adTouchScoreBonus", "1", CVAR_GAMERULE | CVAR_SERVERINFO, 0, NULL},
    {&g_allTalk, "g_allTalk", "0", 0, 0, NULL},
    {&g_allowCustomHeadmodels, "g_allowCustomHeadmodels", "0", CVAR_GAMERULE | CVAR_GAMERULE_MODEL, 0, NULL},
    {&g_allowForfeit, "g_allowForfeit", "1", CVAR_GAMERULE, 0, NULL},
    {&g_allowKill, "g_allowKill", "1000", CVAR_GAMERULE, 0, NULL},
    {&g_ammoPackHack, "g_ammoPackHack", "0", CVAR_GAMERULE | CVAR_LATCH, 0, NULL},
    {&g_autoAction, "g_autoAction", "0", 0, 0, NULL},
    {&g_battleSuitDampen, "g_battleSuitDampen", "0.25", CVAR_GAMERULE, 0, NULL},
    {&g_bestStartingWeapons, "g_bestStartingWeapons", "gh bfg rl lg rg hmg cg sg pg mg ng gl g pl", CVAR_GAMERULE, 0, NULL},
    {&g_botSpawnList, "g_botSpawnList", "", 0, 0, NULL},
    {&g_complaintDamageThreshold, "g_complaintDamageThreshold", "400", CVAR_ARCHIVE, 0, NULL},
    {&g_complaintLimit, "g_complaintLimit", "5", CVAR_ARCHIVE, 0, NULL},
    {&g_customSettings, "g_customSettings", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_debugFlags, "g_debugFlags", "0", 0, 0, NULL},
    {&g_debugThawTime, "g_debugThawTime", "0", 0, 0, NULL},
    {&g_debugVampiricDamage, "g_debugVampiricDamage", "0", 0, 0, NULL},
    {&g_domCapTime, "g_domCapTime", "5", CVAR_GAMERULE, 0, NULL},
    {&g_domDistressThreshold, "g_domDistressThreshold", "75", CVAR_GAMERULE, 0, NULL},
    {&g_domEnableContention, "g_domEnableContention", "1", CVAR_GAMERULE, 0, NULL},
    {&g_domNeutralFlag, "g_domNeutralFlag", "0", CVAR_GAMERULE, 0, NULL},
    {&g_domScoreRate, "g_domScoreRate", "5", CVAR_GAMERULE, 0, NULL},
    {&g_domTeammateCapScale, "g_domTeammateCapScale", "0.5", CVAR_GAMERULE, 0, NULL},
    {&g_dropCmds, "g_dropCmds", "7", CVAR_GAMERULE, 0, NULL},
    {&g_dropDamagedHealth, "g_dropDamagedHealth", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_TEMP, 0, NULL},
    {&g_dropPowerups, "g_dropPowerups", "1", CVAR_GAMERULE, 0, NULL},
    {&g_dropSkulls, "g_dropSkulls", "1", CVAR_GAMERULE, 0, NULL},
    {&g_droppedFlagBonus, "g_droppedFlagBonus", "1", CVAR_TEMP, 0, NULL},
    {&g_droppedPowerupsDecay, "g_droppedPowerupsDecay", "1", CVAR_GAMERULE, 0, NULL},
    {&g_enableDebugTrace, "g_enableDebugTrace", "0", 0, 0, NULL},
    {&g_enemyTeamRespawnRatio, "g_enemyTeamRespawnRatio", "1.5", CVAR_GAMERULE, 0, NULL},
    {&g_factory, "g_factory", "", CVAR_ROM | CVAR_SERVERINFO, 0, NULL},
    {&g_factoryTitle, "g_factoryTitle", "", CVAR_ROM | CVAR_SERVERINFO, 0, NULL},
    {&g_flagBounce, "g_flagBounce", "0.25", CVAR_GAMERULE, 0, NULL},
    {&g_flagPhysics, "g_flagPhysics", "0", CVAR_GAMERULE, 0, NULL},
    {&g_flightRefuelRate, "g_flightRefuelRate", "0", CVAR_GAMERULE, 0, NULL},
    {&g_flightThrust, "g_flightThrust", "1200", CVAR_GAMERULE, 0, NULL},
    {&g_floodprot_decay, "g_floodprot_decay", "1000", 0, 0, NULL},
    {&g_floodprot_maxcount, "g_floodprot_maxcount", "10", 0, 0, NULL},
    {&g_forceAtmosphericEffects, "g_forceAtmosphericEffects", "", CVAR_GAMERULE, 0, NULL},
    {&g_forceSendConfigstring, "g_forceSendConfigstring", "0", 0, 0, NULL},
    {&g_forceSmallScoreboardMessage, "g_forceSmallScoreboardMessage", "0", 0, 0, NULL},
    {&g_freezeEnvironmentalRespawnDelay, "g_freezeEnvironmentalRespawnDelay", "5000", CVAR_GAMERULE, 0, NULL},
    {&g_friendlyFireDampen, "g_friendlyFireDampen", "1.00", CVAR_GAMERULE, 0, NULL},
    {&g_gauntletSpeedFactor, "g_gauntletSpeedFactor", "1.0", CVAR_GAMERULE, 0, NULL},
    {&g_grantItemOnSpawn, "g_grantItemOnSpawn", "", CVAR_GAMERULE, 0, NULL},
    {&g_instaGib, "g_instaGib", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_SERVERINFO, 0, NULL},
    {&g_kamiAttenuate, "g_kamiAttenuate", "2048", CVAR_GAMERULE, 0, NULL},
    {&g_kamiMinRatio, "g_kamiMinRatio", "0.1", CVAR_GAMERULE, 0, NULL},
    {&g_kickBadUserinfo, "g_kickBadUserinfo", "1", 0, 0, NULL},
    {&g_latchedHookOffset, "g_latchedHookOffset", "-2.0f", CVAR_GAMERULE, 0, NULL},
    {&g_levelStartTime, "g_levelStartTime", "0", CVAR_ROM | CVAR_SERVERINFO, 0, NULL},
    {&g_lightningDischarge, "g_lightningDischarge", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_maxDeferredSpawns, "g_maxDeferredSpawns", "4", 0, 0, NULL},
    {&g_maxFlightFuel, "g_maxFlightFuel", "16000", CVAR_GAMERULE, 0, NULL},
    {&g_midAirMinHeight, "g_midAirMinHeight", "96", CVAR_GAMERULE, 0, NULL},
    {&g_neutralFlagPingRate, "g_neutralFlagPingRate", "2400", CVAR_GAMERULE, 0, NULL},
    {&g_playerModelScale, "g_playerModelScale", "1.1", CVAR_GAMERULE | CVAR_GAMERULE_MODEL, 0, NULL},
    {&g_playerheadScale, "g_playerheadScale", "1.0", CVAR_GAMERULE | CVAR_GAMERULE_MODEL, 0, NULL},
    {&g_playerheadScaleOffset, "g_playerheadScaleOffset", "1.0", CVAR_GAMERULE | CVAR_GAMERULE_MODEL, 0, NULL},
    {&g_playerheadmodelOverride, "g_playerheadmodelOverride", "", CVAR_GAMERULE | CVAR_GAMERULE_MODEL, 0, OnChangedModelOverride},
    {&g_playermodelOverride, "g_playermodelOverride", "", CVAR_GAMERULE | CVAR_GAMERULE_MODEL, 0, OnChangedModelOverride},
    {&g_powerupRespawn, "g_powerupRespawn", "120", CVAR_GAMERULE, 0, NULL},
    {&g_quadHogIdle, "g_quadHogIdle", "20", CVAR_GAMERULE, 0, NULL},
    {&g_quadHogPingRate, "g_quadHogPingRate", "1500", CVAR_GAMERULE, 0, NULL},
    {&g_regenArmor, "g_regenArmor", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_regenArmorAfterHealth, "g_regenArmorAfterHealth", "0", CVAR_GAMERULE, 0, NULL},
    {&g_regenArmorRate, "g_regenArmorRate", "100", CVAR_GAMERULE, 0, NULL},
    {&g_regenHealth, "g_regenHealth", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_regenHealthRate, "g_regenHealthRate", "100", CVAR_GAMERULE, 0, NULL},
    {&g_respawn_delay_max, "g_respawn_delay_max", "2400", CVAR_GAMERULE, 0, NULL},
    {&g_respawn_delay_min, "g_respawn_delay_min", "2100", CVAR_GAMERULE, 0, NULL},
    {&g_returnFlagOnSuicide, "g_returnFlagOnSuicide", "0", CVAR_GAMERULE, 0, NULL},
    {&g_rrRoundScoreBonus, "g_rrRoundScoreBonus", "0", CVAR_GAMERULE, 0, NULL},
    {&g_shuffle_automatic, "g_shuffle_automatic", "0", 0, 0, NULL},
    {&g_shuffle_automatic_minplayers, "g_shuffle_automatic_minplayers", "6", 0, 0, NULL},
    {&g_shuffle_minplayers, "g_shuffle_minplayers", "3", 0, 0, NULL},
    {&g_shuffle_timedelay, "g_shuffle_timedelay", "5000", 0, 0, NULL},
    {&g_skipTrainingEnable, "g_skipTrainingEnable", "0", CVAR_ROM | CVAR_SYSTEMINFO, 0, NULL},
    {&g_trainingSpanish, "g_trainingSpanish", "0", CVAR_ARCHIVE, 0, NULL},
    {&g_spawnArmorDmgScale, "g_spawnArmorDmgScale", "0.5", CVAR_GAMERULE, 0, NULL},
    {&g_spawnDelayRandom_key, "g_spawnDelayRandom_key", "15", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnDelayRandom_powerup, "g_spawnDelayRandom_powerup", "15", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnDelay_key, "g_spawnDelay_key", "30", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnDelay_powerup, "g_spawnDelay_powerup", "45", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnItemArmor, "g_spawnItemArmor", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnItemHealth, "g_spawnItemHealth", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnItemHoldable, "g_spawnItemHoldable", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnItemPowerup, "g_spawnItemPowerup", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnItemWeapons, "g_spawnItemWeapons", "1", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&g_spawnMinDistance, "g_spawnMinDistance", "64", CVAR_GAMERULE, 0, NULL},
    {&g_spawnRandomRatio, "g_spawnRandomRatio", "0.5", CVAR_GAMERULE, 0, NULL},
    {&g_suddenDeathRespawn, "g_suddenDeathRespawn", "0", CVAR_GAMERULE, 0, NULL},
    {&g_suddenDeathRespawnIncrement, "g_suddenDeathRespawnIncrement", "1", CVAR_GAMERULE, 0, NULL},
    {&g_suddenDeathRespawnMax, "g_suddenDeathRespawnMax", "10", CVAR_GAMERULE, 0, NULL},
    {&g_suddenDeathRespawnPrint, "g_suddenDeathRespawnPrint", "1", CVAR_GAMERULE, 0, NULL},
    {&g_suddenDeathRespawnStart, "g_suddenDeathRespawnStart", "3", CVAR_GAMERULE, 0, NULL},
    {&g_suddenDeathRespawnTick, "g_suddenDeathRespawnTick", "60", CVAR_GAMERULE, 0, NULL},
    {&g_switchTeamDelay, "g_switchTeamDelay", "3", CVAR_GAMERULE, 0, NULL},
    {&g_tackleFlag, "g_tackleFlag", "0", CVAR_GAMERULE, 0, NULL},
    {&g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE, 0, NULL},
    {&g_teamForceBalance, "g_teamForceBalance", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, NULL},
    {&g_teamSpawnAsSpec, "g_teamSpawnAsSpec", "0", 0, 0, NULL},
    {&g_teamSpecFreeCam, "g_teamSpecFreeCam", "0", 0, 0, NULL},
    {&g_teamSpecSayEnable, "g_teamSpecSayEnable", "1", 0, 0, NULL},
    {&g_throwFlagForwardMult, "g_throwFlagForwardMult", "2.5", 0, 0, NULL},
    {&g_throwFlagVelocity, "g_throwFlagVelocity", "0", 0, 0, NULL},
    {&g_timeoutLen, "g_timeoutLen", "60", CVAR_GAMERULE, 0, NULL},
    {&g_vampiricDamage, "g_vampiricDamage", "0", CVAR_GAMERULE | CVAR_GAMERULE_REPL, 0, NULL},
    {&practiceflags, "practiceflags", "0", CVAR_GAMERULE | CVAR_TEMP, 0, NULL},
    {&g_advancedTraining, "g_advancedTraining", "0", CVAR_GAMERULE | CVAR_TEMP, 0, NULL},
    {&g_trainingSpawn, "g_trainingSpawn", "", CVAR_GAMERULE | CVAR_TEMP, 0, NULL},
    {&g_trainingTarget, "g_trainingTarget", "", CVAR_GAMERULE | CVAR_TEMP, 0, NULL},
    {&sv_mapname, "sv_mapname", "", CVAR_ROM | CVAR_SERVERINFO, 0, NULL},
    {&ui_singlePlayerActive, "ui_singlePlayerActive", "", 0, 0, NULL},
    {&weapon_reload_gauntlet, "weapon_reload_gauntlet", "400", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, OnChangedWeaponReloadGauntlet},
    {&weapon_reload_hook, "weapon_reload_hook", "100", CVAR_GAMERULE | CVAR_GAMERULE_REPL | CVAR_GAMERULE_WEAPON, 0, NULL},

    {NULL, NULL, NULL, 0, 0, NULL}
};

static int gameCvarTableSize = ARRAY_LEN(gameCvarTable);

void G_InitGame(int levelTime, int randomSeed, int restart);
void G_RunFrame(int levelTime);
void G_ShutdownGame(int restart);
void CheckExitRules(void);
int G_ClientScore(int clientNum);

Q_EXPORT void dllEntry(void **vmMainOut, gameImport_t *syscallTable, int *apiVersion) {
    imports = syscallTable;

    vmMainOut[GAME_SHUTDOWN]                = (void *)G_ShutdownGame;
    vmMainOut[GAME_RUN_FRAME]               = (void *)G_RunFrame;
    vmMainOut[GAME_REGISTER_CVARS]          = (void *)G_RegisterCvars;
    vmMainOut[GAME_INIT]                    = (void *)G_InitGame;
    vmMainOut[GAME_CONSOLE_COMMAND]         = (void *)ConsoleCommand;
    vmMainOut[GAME_CLIENT_USERINFO_CHANGED] = (void *)ClientUserinfoChanged;
    vmMainOut[GAME_CLIENT_THINK]            = (void *)ClientThink;
    vmMainOut[GAME_CLIENT_DISCONNECT]       = (void *)ClientDisconnect;
    vmMainOut[GAME_CLIENT_CONNECT]          = (void *)ClientConnect;
    vmMainOut[GAME_CLIENT_COMMAND]          = (void *)ClientCommand;
    vmMainOut[GAME_CLIENT_BEGIN]            = (void *)ClientBegin;
    vmMainOut[GAME_SNAPSHOT_VISIBILITY]     = (void *)G_ObfuscateEnemyInfoInSnapshotCheck;
    vmMainOut[GAME_SNAPSHOT_ALWAYS_SEND]    = (void *)G_ClientAlwaysInSnapshot;
    vmMainOut[GAME_SNAPSHOT_SKIP_ENTITY]    = (void *)G_SnapshotSkipEntity;
    vmMainOut[GAME_CLIENT_IS_ADMIN]         = (void *)G_ClientIsAdmin;
    vmMainOut[GAME_ENTITY_ALWAYS_SEND]      = (void *)G_EntityAlwaysInSnapshot;
    vmMainOut[GAME_BOT_AI_START_FRAME]      = (void *)BotAIStartFrame;
    vmMainOut[GAME_VOICE_FILTER]            = (void *)G_VoiceFilter;
    vmMainOut[GAME_CLIENT_SCORE]            = (void *)G_ClientScore;

    *apiVersion = GAME_API_VERSION;
}

int G_ClientScore(int clientNum) {
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !g_entities[clientNum].client) {
        return 0;
    }
    return g_entities[clientNum].client->ps.persistant[PERS_SCORE];
}

void QDECL G_Printf(const char* fmt, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    trap_Print(text);
}

void QDECL G_Error(const char* fmt, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    trap_Error(text);
}

void G_FindTeams(void) {
    gentity_t *e, *e2;
    int i, j;
    int c, c2;

    c = 0;
    c2 = 0;
    for (i = MAX_CLIENTS, e = g_entities + i; i < level.num_entities; i++, e++) {
        if (!e->inuse)
            continue;
        if (!e->team)
            continue;
        if (e->flags & FL_TEAMSLAVE)
            continue;
        e->teammaster = e;
        c++;
        c2++;
        for (j = i + 1, e2 = e + 1; j < level.num_entities; j++, e2++) {
            if (!e2->inuse)
                continue;
            if (!e2->team)
                continue;
            if (e2->flags & FL_TEAMSLAVE)
                continue;
            if (!strcmp(e->team, e2->team)) {
                c2++;
                e2->teamchain = e->teamchain;
                e->teamchain = e2;
                e2->teammaster = e;
                e2->flags |= FL_TEAMSLAVE;

                if (e2->targetname) {
                    e->targetname = e2->targetname;
                    e2->targetname = NULL;
                }
            }
        }
    }

    G_Printf("%i teams with %i entities\n", c, c2);
}

static int g_customSettingsFlags;

typedef struct {
    const char  *cvarName;
    int         bit;
} customSetting_t;

static const customSetting_t customSettingsBits[] = {
    { "weapon_reload_gauntlet", 0x00000001 },
    { "g_damage_g", 0x00000001 },
    { "g_knockback_g", 0x00000001 },
    { "weapon_reload_mg", 0x00000002 },
    { "g_damage_mg", 0x00000002 },
    { "g_knockback_mg", 0x00000002 },
    { "g_ironsights_mg", 0x00000002 },
    { "weapon_reload_sg", 0x00000004 },
    { "g_damage_sg", 0x00000004 },
    { "g_damage_sg_outer", 0x00000004 },
    { "g_knockback_sg", 0x00000004 },
    { "g_damage_sg_falloff", 0x00000004 },
    { "weapon_reload_gl", 0x00000008 },
    { "g_damage_gl", 0x00000008 },
    { "g_splashdamage_gl", 0x00000008 },
    { "g_splashradius_gl", 0x00000008 },
    { "g_velocity_gl", 0x00000008 },
    { "g_knockback_gl", 0x00000008 },
    { "weapon_reload_rl", 0x00000010 },
    { "g_damage_rl", 0x00000010 },
    { "g_splashdamage_rl", 0x00000010 },
    { "g_splashradius_rl", 0x00000010 },
    { "g_velocity_rl", 0x00000010 },
    { "g_knockback_rl", 0x00000010 },
    { "g_knockback_rl_self", 0x00000010 },
    { "g_accelFactor_rl", 0x00000010 },
    { "weapon_gravity_rl", 0x00000010 },
    { "weapon_reload_lg", 0x00000020 },
    { "g_damage_lg", 0x00000020 },
    { "g_knockback_lg", 0x00000020 },
    { "g_damage_lg_falloff", 0x00000020 },
    { "weapon_reload_rg", 0x00000040 },
    { "g_damage_rg", 0x00000040 },
    { "g_knockback_rg", 0x00000040 },
    { "weapon_reload_pg", 0x00000080 },
    { "g_damage_pg", 0x00000080 },
    { "g_splashdamage_pg", 0x00000080 },
    { "g_splashradius_pg", 0x00000080 },
    { "g_velocity_pg", 0x00000080 },
    { "g_knockback_pg", 0x00000080 },
    { "g_knockback_pg_self", 0x00000080 },
    { "g_accelFactor_pg", 0x00000080 },
    { "weapon_gravity_pg", 0x00000080 },
    { "weapon_reload_bfg", 0x00000100 },
    { "g_damage_bfg", 0x00000100 },
    { "g_splashdamage_bfg", 0x00000100 },
    { "g_splashradius_bfg", 0x00000100 },
    { "g_velocity_bfg", 0x00000100 },
    { "g_knockback_bfg", 0x00000100 },
    { "g_accelFactor_bfg", 0x00000100 },
    { "weapon_gravity_bfg", 0x00000100 },
    { "g_damage_gh", 0x00000200 },
    { "weapon_reload_gh", 0x00000200 },
    { "weapon_reload_hook", 0x00000200 },
    { "g_velocity_gh", 0x00000200 },
    { "g_knockback_gh", 0x00000200 },
    { "pmove_velocity_gh", 0x00000200 },
    { "weapon_reload_ng", 0x00000400 },
    { "g_damage_ng", 0x00000400 },
    { "g_nailspeed", 0x00000400 },
    { "g_knockback_ng", 0x00000400 },
    { "g_nailcount", 0x00000400 },
    { "g_nailspread", 0x00000400 },
    { "g_nailbounce", 0x00000400 },
    { "g_nailbouncepercentage", 0x00000400 },
    { "weapon_gravity_ng", 0x00000400 },
    { "weapon_reload_prox", 0x00000800 },
    { "g_splashdamage_pl", 0x00000800 },
    { "g_splashradius_pl", 0x00000800 },
    { "g_knockback_pl", 0x00000800 },
    { "g_proxMineTimeout", 0x00000800 },
    { "weapon_reload_cg", 0x00001000 },
    { "g_damage_cg", 0x00001000 },
    { "g_knockback_cg", 0x00001000 },
    { "pmove_AirControl", 0x00002000 },
    { "pmove_RampJump", 0x00004000 },
    { "g_speed", 0x00008000 },
    { "pmove_AirAccel", 0x00008000 },
    { "pmove_WalkAccel", 0x00008000 },
    { "pmove_WalkFriction", 0x00008000 },
    { "pmove_AirSteps", 0x00008000 },
    { "pmove_BunnyHop", 0x00008000 },
    { "pmove_StepJump", 0x00008000 },
    { "pmove_StepHeight", 0x00008000 },
    { "pmove_WeaponRaiseTime", 0x00010000 },
    { "pmove_WeaponDropTime", 0x00010000 },
    { "g_instaGib", 0x00020000 },
    { "g_quadHog", 0x00040000 },
    { "g_regenHealth", 0x00080000 },
    { "g_regenArmor", 0x00080000 },
    { "g_dropDamagedHealth", 0x00100000 },
    { "g_vampiricDamage", 0x00200000 },
    { "g_spawnItemWeapon", 0x00400000 },
    { "g_spawnItemPowerup", 0x00400000 },
    { "g_spawnItemHoldable", 0x00400000 },
    { "g_spawnItemHealth", 0x00400000 },
    { "g_spawnItemArmor", 0x00400000 },
    { "g_spawnItemAmmo", 0x00400000 },
    { "g_spawnDelay_key", 0x00400000 },
    { "g_spawnDelay_powerup", 0x00400000 },
    { "g_spawnDelayRandom_key", 0x00400000 },
    { "g_spawnDelayRandom_powerup", 0x00400000 },
    { "g_headShotDamage_rg", 0x00800000 },
    { "g_railJump", 0x01000000 },
    { "pmove_noPlayerClip", 0x02000000 },
    { "g_rrInfected", 0x04000000 },
    { "g_lightningDischarge", 0x08000000 }
};

static void G_ParseCustomSettings(cvarTable_t *cv, qboolean noClear) {
    int         i;
    int         bit;
    qboolean    custom;

    if (!cv->cvarName) {
        return;
    }

    bit = 0;
    for (i = 0; i < (int)ARRAY_LEN(customSettingsBits); i++) {
        if (!Q_stricmp(cv->cvarName, customSettingsBits[i].cvarName)) {
            bit = customSettingsBits[i].bit;
            break;
        }
    }
    if (!bit) {
        return;
    }

    custom = (cv->defaultString == NULL) ||
             (cv->vmCvar != NULL && Q_stricmp(cv->defaultString, cv->vmCvar->string) != 0);

    if (custom) {
        g_customSettingsFlags |= bit;
    } else if (!noClear) {
        g_customSettingsFlags &= ~bit;
    }
}

void G_UpdateCustomSettings(void) {
    int         i;
    gentity_t   *ent;

    for (i = 0; i < level.maxclients; i++) {
        ent = &g_entities[i];
        if (ent->client && ent->client->pers.connected == CON_CONNECTED) {
            ClientUserinfoChanged(i);
        }
    }
}

int PM_DbgStepJump(void); float PM_DbgSwim(void); float PM_DbgWade(void);
int PM_DbgAutoHop(void); int PM_DbgChainJump(void); int PM_DbgAirSteps(void);

void G_RegisterCvars(void) {
    int i;
    cvarTable_t *cv;

    g_customSettingsFlags = 0;
    trap_Cvar_Set("g_customSettings", "0");

    for (i = 0, cv = gameCvarTable; cv->cvarName != NULL; i++, cv++) {
        trap_Cvar_Register(cv->vmCvar, cv->cvarName,
                           cv->defaultString, cv->cvarFlags);
        if (cv->vmCvar)
            cv->modCount = cv->vmCvar->modificationCount;

        if (cv->onChanged) {
            cv->onChanged();
        }

        if (cv->cvarFlags & CVAR_GAMERULE_REPL) {
            G_ParseCustomSettings(cv, qtrue);
        }
    }

    if (g_gametype.integer < 0 || g_gametype.integer > 12) {
        G_Printf("g_gametype %i is out of range, defaulting to 0\n", g_gametype.integer);
        trap_Cvar_Set("g_gametype", "0");
    }

    trap_Cvar_Set("g_customSettings", va("%i", g_customSettingsFlags));
    trap_SetConfigstring(CS_CUSTOM_SETTINGS, va("%i", g_customSettingsFlags));
}

static const char* const g_announcedCvars[] = {
    "armor_tiered", "capturelimit", "dmflags", "fraglimit", "g_allTalk", "g_ammoPack",
    "g_ammoPackHack", "g_ammoRespawn", "g_bestStartingWeapons", "g_complaintDamageThreshold",
    "g_complaintLimit", "g_damage_bfg", "g_damage_cg", "g_damage_g", "g_damage_gh", "g_damage_gl",
    "g_damage_hmg", "g_damage_lg", "g_damage_lg_falloff", "g_damage_mg", "g_damage_ng",
    "g_damage_pg", "g_damage_pl", "g_damage_rg", "g_damage_rl", "g_damage_sg",
    "g_damage_sg_falloff", "g_damage_sg_outer", "g_debugFlags", "g_debugInactivity",
    "g_dmgThroughSurfaceDistance", "g_domCapTime", "g_domNeutralFlag", "g_domScoreRate",
    "g_domTeammateCapScale", "g_doWarmup", "g_dropInactive", "g_droppedPowerupsDecay",
    "g_dropPowerups", "g_dropSkulls", "g_enableDust", "g_flagBounce", "g_flagPhysics",
    "g_forceAtmosphericEffects", "g_forceDmgThroughSurface", "g_freezeAutoThawTime",
    "g_freezeEnvironmentalRespawnDelay", "g_freezeProtectedSpawnTime",
    "g_freezeRemovePowerupsOnRound", "g_freezeResetArmorOnRound", "g_freezeResetHealthOnRound",
    "g_freezeResetWeaponsOnRound", "g_freezeThawRadius", "g_freezeThawThroughSurface",
    "g_freezeThawTick", "g_freezeThawTime", "g_freezeThawWinningTeam", "g_friendlyFire",
    "g_friendlyFireDampen", "g_gauntletSpeedFactor", "g_gravity", "g_guidedRocket",
    "g_headShotDamage_rg", "g_inactivity", "g_inactivityWarning", "g_infiniteAmmo", "g_instaGib",
    "g_ironsights_mg", "g_itemHeight", "g_itemTimers", "g_knockback", "g_knockback_bfg",
    "g_knockback_cg", "g_knockback_cripple", "g_knockback_g", "g_knockback_gh", "g_knockback_gl",
    "g_knockback_hmg", "g_knockback_lg", "g_knockback_mg", "g_knockback_ng", "g_knockback_pg",
    "g_knockback_pg_self", "g_knockback_pl", "g_knockback_rg", "g_knockback_rl",
    "g_knockback_rl_self", "g_knockback_sg", "g_knockback_z", "g_knockback_z_self",
    "g_lightningDischarge", "g_loadout", "g_max_knockback", "g_mercytime", "g_nailbounce",
    "g_nailbouncepercentage", "g_nailcount", "g_nailspeed", "g_nailspread",
    "g_neutralFlagPingRate", "g_overtime", "g_playerCylinders", "g_quadDamageFactor", "g_quadHog",
    "g_quadHogIdle", "g_quadHogPingRate", "g_quadHogTime", "g_railJump", "g_range_lg_falloff",
    "g_range_sg_falloff", "g_regenArmor", "g_regenArmorAfterHealth", "g_regenArmorRate",
    "g_regenHealth", "g_regenHealthRate", "g_respawn_delay_max", "g_respawn_delay_min",
    "g_returnFlagOnSuicide", "g_roundWarmupDelay", "g_rrAllowNegativeScores",
    "g_rrDamageScoreBonus", "g_rrDeathScorePenalty", "g_rrInfected", "g_rrInfectedSpreadTime",
    "g_rrInfectedSpreadWarningTime", "g_rrInfectedSurvivorMinSpeed",
    "g_rrInfectedSurvivorPingRate", "g_rrInfectedSurvivorScoreBonus",
    "g_rrInfectedSurvivorScoreMethod", "g_rrInfectedSurvivorScoreRate",
    "g_rrInfectedZombieFragBonus", "g_rrInfectedZombieHealthBonus", "g_rrInfectedZombieSpeed",
    "g_rrRoundScoreBonus", "g_runes", "g_spawnArmor", "g_spawnItemAmmo", "g_spawnItemArmor",
    "g_spawnItemHealth", "g_spawnItemHoldable", "g_spawnItemPowerup", "g_spawnItemWeapons",
    "g_speed", "g_splashdamage_bfg", "g_splashdamage_gl", "g_splashdamage_pg", "g_splashdamage_pl",
    "g_splashdamage_rl", "g_splashradius_bfg", "g_splashradius_gl", "g_splashradius_pg",
    "g_splashradius_pl", "g_splashradius_rl", "g_startingAmmo_bfg", "g_startingAmmo_cg",
    "g_startingAmmo_g", "g_startingAmmo_gh", "g_startingAmmo_gl", "g_startingAmmo_hmg",
    "g_startingAmmo_lg", "g_startingAmmo_mg", "g_startingAmmo_ng", "g_startingAmmo_pg",
    "g_startingAmmo_pl", "g_startingAmmo_rg", "g_startingAmmo_rl", "g_startingAmmo_sg",
    "g_startingArmor", "g_startingHealth", "g_startingHealthBonus", "g_startingWeapons",
    "g_suddenDeathRespawn", "g_tackleFlag", "g_teamForcePresent", "g_teamSizeMin",
    "g_teamSpawnAsSpec", "g_teamSpecFreeCam", "g_throwFlagForwardMult", "g_throwFlagVelocity",
    "g_timeoutCount", "g_timeoutLen", "g_vampiricDamage", "g_velocity_bfg", "g_velocity_gh",
    "g_velocity_gl", "g_velocity_pg", "g_velocity_rl", "g_warmup", "g_weaponRespawn", "mercylimit",
    "pmove_AirAccel", "pmove_AirStepFriction", "pmove_AirSteps", "pmove_AirStopAccel",
    "pmove_AutoHop", "pmove_BunnyHop", "pmove_ChainJump", "pmove_ChainJumpVelocity",
    "pmove_CircleStrafeFriction", "pmove_CrouchSlideFriction", "pmove_CrouchSlideTime",
    "pmove_CrouchStepJump", "pmove_JumpTimeDeltaMin", "pmove_JumpVelocity",
    "pmove_JumpVelocityMax", "pmove_JumpVelocityScaleAdd", "pmove_JumpVelocityTimeThreshold",
    "pmove_JumpVelocityTimeThresholdOffset", "pmove_noPlayerClip", "pmove_RampJump",
    "pmove_RampJumpScale", "pmove_StepHeight", "pmove_StepJump", "pmove_StepJumpVelocity",
    "pmove_StrafeAccel", "pmove_velocity_gh", "pmove_WalkAccel", "pmove_WalkFriction",
    "pmove_WaterSwimScale", "pmove_WaterWadeScale", "pmove_WeaponDropTime",
    "pmove_WeaponRaiseTime", "pmove_WishSpeed", "roundlimit", "roundtimelimit", "scorelimit",
    "teamsize", "timelimit", "weapon_gravity_bfg", "weapon_gravity_ng", "weapon_gravity_pg",
    "weapon_gravity_rl", "weapon_reload_bfg", "weapon_reload_cg", "weapon_reload_gauntlet",
    "weapon_reload_gh", "weapon_reload_gl", "weapon_reload_hmg", "weapon_reload_hook",
    "weapon_reload_lg", "weapon_reload_mg", "weapon_reload_ng", "weapon_reload_pg",
    "weapon_reload_prox", "weapon_reload_rg", "weapon_reload_rl", "weapon_reload_sg",
};

static int G_CompareCvarName(const void* a, const void* b) {
    return Q_stricmp((const char*)a, *(const char* const*)b);
}

static qboolean G_CvarAnnounced(const char* name) {
    return bsearch(name, g_announcedCvars, ARRAY_LEN(g_announcedCvars), sizeof(g_announcedCvars[0]),
                   G_CompareCvarName) ? qtrue : qfalse;
}

void G_UpdateCvars(void) {
    int i;
    cvarTable_t *cv;
    qboolean customChanged = qfalse;

    for (i = 0, cv = gameCvarTable; cv->cvarName != NULL; i++, cv++) {
        if (cv->vmCvar) {
            trap_Cvar_Update(cv->vmCvar);

            if (cv->modCount != cv->vmCvar->modificationCount) {
                cv->modCount = cv->vmCvar->modificationCount;

                if (cv->onChanged && !g_training.integer) {
                    cv->onChanged();
                }

                if (g_cheats.integer && G_CvarAnnounced(cv->cvarName)) {
                    trap_SendServerCommand(-1, va("print \"Server: %s changed to %s\n\"",
                                                  cv->cvarName, cv->vmCvar->string));
                }

                if (cv->cvarFlags & CVAR_GAMERULE_ARMOR)  armorInfoDirty = qtrue;
                if (cv->cvarFlags & CVAR_GAMERULE_WEAPON) weaponInfoDirty = qtrue;
                if (cv->cvarFlags & CVAR_GAMERULE_MODEL)  playerInfoDirty = qtrue;

                if (cv->cvarFlags & CVAR_GAMERULE_REPL) {
                    G_ParseCustomSettings(cv, qfalse);
                    trap_Cvar_Set("g_customSettings", va("%i", g_customSettingsFlags));
                    customChanged = qtrue;
                }
            }
        }
    }

    if (customChanged) {
        trap_SetConfigstring(CS_CUSTOM_SETTINGS, va("%i", g_customSettingsFlags));
    }
}

static void G_SendPmoveInfo(void) {
    char info[MAX_INFO_STRING];

    info[0] = '\0';

    Info_SetValueForKey(info, "pmove_AirAccel", va("%g", pmove_AirAccel.value));
    Info_SetValueForKey(info, "pmove_AirStepFriction", va("%g", pmove_AirStepFriction.value));
    Info_SetValueForKey(info, "pmove_AirSteps", va("%i", pmove_AirSteps.integer));
    Info_SetValueForKey(info, "pmove_AirStopAccel", va("%g", pmove_AirStopAccel.value));
    Info_SetValueForKey(info, "pmove_AirControl", va("%g", pmove_AirControl.value));
    Info_SetValueForKey(info, "pmove_AutoHop", va("%i", pmove_AutoHop.integer));
    Info_SetValueForKey(info, "pmove_BunnyHop", va("%i", pmove_BunnyHop.integer));
    Info_SetValueForKey(info, "pmove_ChainJump", va("%i", pmove_ChainJump.integer));
    Info_SetValueForKey(info, "pmove_ChainJumpVelocity", va("%g", pmove_ChainJumpVelocity.value));
    Info_SetValueForKey(info, "pmove_CircleStrafeFriction", va("%g", pmove_CircleStrafeFriction.value));
    Info_SetValueForKey(info, "pmove_CrouchSlideFriction", va("%g", pmove_CrouchSlideFriction.value));
    Info_SetValueForKey(info, "pmove_CrouchSlideTime", va("%i", pmove_CrouchSlideTime.integer));
    Info_SetValueForKey(info, "pmove_CrouchSlide", va("%i", pmove_CrouchSlide.integer));
    Info_SetValueForKey(info, "pmove_CrouchStepJump", va("%i", pmove_CrouchStepJump.integer));
    Info_SetValueForKey(info, "pmove_DoubleJump", va("%i", pmove_DoubleJump.integer));
    Info_SetValueForKey(info, "pmove_JumpTimeDeltaMin", va("%g", pmove_JumpTimeDeltaMin.value));
    Info_SetValueForKey(info, "pmove_JumpVelocity", va("%g", pmove_JumpVelocity.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityMax", va("%g", pmove_JumpVelocityMax.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityScaleAdd", va("%g", pmove_JumpVelocityScaleAdd.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityTimeThreshold", va("%g", pmove_JumpVelocityTimeThreshold.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityTimeThresholdOffset", va("%g", pmove_JumpVelocityTimeThresholdOffset.value));
    Info_SetValueForKey(info, "pmove_noPlayerClip", va("%i", pmove_NoPlayerClip.integer));
    Info_SetValueForKey(info, "pmove_RampJump", va("%i", pmove_RampJump.integer));
    Info_SetValueForKey(info, "pmove_RampJumpScale", va("%g", pmove_RampJumpScale.value));
    Info_SetValueForKey(info, "pmove_StepHeight", va("%g", pmove_StepHeight.value));
    Info_SetValueForKey(info, "pmove_StepJump", va("%i", pmove_StepJump.integer));
    Info_SetValueForKey(info, "pmove_StepJumpVelocity", va("%g", pmove_StepJumpVelocity.value));
    Info_SetValueForKey(info, "pmove_StrafeAccel", va("%g", pmove_StrafeAccel.value));
    Info_SetValueForKey(info, "pmove_velocity_gh", va("%g", pmove_VelocityGH.value));
    Info_SetValueForKey(info, "pmove_WalkAccel", va("%g", pmove_WalkAccel.value));
    Info_SetValueForKey(info, "pmove_WalkFriction", va("%g", pmove_WalkFriction.value));
    Info_SetValueForKey(info, "pmove_WaterSwimScale", va("%g", pmove_WaterSwimScale.value));
    Info_SetValueForKey(info, "pmove_WaterWadeScale", va("%g", pmove_WaterWadeScale.value));
    Info_SetValueForKey(info, "pmove_WeaponRaiseTime", va("%i", pmove_WeaponRaiseTime.integer));
    Info_SetValueForKey(info, "pmove_WeaponDropTime", va("%i", pmove_WeaponDropTime.integer));
    Info_SetValueForKey(info, "pmove_WishSpeed", va("%g", pmove_WishSpeed.value));

    trap_SetConfigstring(CS_PMOVEINFO, info);
}

static void G_SendArmorInfo(void) {
    char info[MAX_INFO_STRING];

    info[0] = '\0';
    Info_SetValueForKey(info, "armor_tiered", armor_tiered.string);

    trap_SetConfigstring(CS_ARMORINFO, info);
}

static void G_SendWeaponInfo(void) {
    char info[MAX_INFO_STRING];

    info[0] = '\0';
    Info_SetValueForKey(info, "weapon_gravity_bfg", weapon_gravity_bfg.string);
    Info_SetValueForKey(info, "weapon_gravity_ng", weapon_gravity_ng.string);
    Info_SetValueForKey(info, "weapon_gravity_pg", weapon_gravity_pg.string);
    Info_SetValueForKey(info, "weapon_gravity_rl", weapon_gravity_rl.string);
    Info_SetValueForKey(info, "weapon_reload_bfg", weapon_reload_bfg.string);
    Info_SetValueForKey(info, "weapon_reload_cg", weapon_reload_cg.string);
    Info_SetValueForKey(info, "weapon_reload_gauntlet", weapon_reload_gauntlet.string);
    Info_SetValueForKey(info, "weapon_reload_gh", weapon_reload_gh.string);
    Info_SetValueForKey(info, "weapon_reload_gl", weapon_reload_gl.string);
    Info_SetValueForKey(info, "weapon_reload_hmg", weapon_reload_hmg.string);
    Info_SetValueForKey(info, "weapon_reload_hook", weapon_reload_hook.string);
    Info_SetValueForKey(info, "weapon_reload_lg", weapon_reload_lg.string);
    Info_SetValueForKey(info, "weapon_reload_mg", weapon_reload_mg.string);
    Info_SetValueForKey(info, "weapon_reload_ng", weapon_reload_ng.string);
    Info_SetValueForKey(info, "weapon_reload_pg", weapon_reload_pg.string);
    Info_SetValueForKey(info, "weapon_reload_prox", weapon_reload_prox.string);
    Info_SetValueForKey(info, "weapon_reload_rg", weapon_reload_rg.string);
    Info_SetValueForKey(info, "weapon_reload_rl", weapon_reload_rl.string);
    Info_SetValueForKey(info, "weapon_reload_sg", weapon_reload_sg.string);

    trap_SetConfigstring(CS_WEAPONINFO, info);
}

static void G_SendPlayerInfo(void) {
    char info[MAX_INFO_STRING];

    info[0] = '\0';
    Info_SetValueForKey(info, "g_allowCustomHeadmodels", g_allowCustomHeadmodels.string);
    Info_SetValueForKey(info, "g_playerheadScale", g_playerheadScale.string);
    Info_SetValueForKey(info, "g_playerheadScaleOffset", g_playerheadScaleOffset.string);
    Info_SetValueForKey(info, "g_playerModelScale", g_playerModelScale.string);

    trap_SetConfigstring(CS_PLAYERINFO, info);
}

void G_InitGame(int levelTime, int randomSeed, int restart) {
    int i;

    G_Printf("------- Game Initialization -------\n");
    G_Printf("gamename: %s\n", GAMEVERSION);
    G_Printf("gamedate: %s\n", PRODUCT_DATE);

    srand(randomSeed);

    G_RegisterCvars();

    G_InitAccessList();

    G_InitMemory();

    memset(&level, 0, sizeof(level));
    level.time = levelTime;
    level.startTime = levelTime;
    level.warmupTime = -1;

    level.snd_fry = G_SoundIndex("sound/player/fry.wav");
    G_SoundIndex("sound/player/gurp1.wav");
    G_SoundIndex("sound/player/gurp2.wav");

    if (g_logfile.string[0]) {
        if (g_logfileSync.integer) {
            trap_FS_FOpenFile(g_logfile.string, &level.logFile, FS_APPEND_SYNC);
        } else {
            trap_FS_FOpenFile(g_logfile.string, &level.logFile, FS_APPEND);
        }
        if (!level.logFile) {
            G_Printf("WARNING: Couldn't open logfile: %s\n", g_logfile.string);
        } else {
            char serverinfo[MAX_INFO_STRING];

            trap_GetServerinfo(serverinfo, sizeof(serverinfo));

            G_LogPrintf("------------------------------------------------------------\n");
            G_LogPrintf("InitGame: %s\n", serverinfo);
        }
    } else {
        G_Printf("Not logging to disk.\n");
    }

    G_InitWorldSession();

    memset(g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]));
    level.gentities = g_entities;

    level.maxclients = g_maxclients.integer;
    memset(g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]));
    level.clients = g_clients;

    for (i = 0; i < level.maxclients; i++) {
        g_entities[i].client = level.clients + i;
    }

    level.num_entities = MAX_CLIENTS;

    for (i = 0; i < MAX_CLIENTS; i++) {
        g_entities[i].classname = "clientslot";
    }

    trap_LocateGameData(level.gentities, level.num_entities, sizeof(gentity_t),
                        &level.clients[0].ps, sizeof(level.clients[0]));

    InitBodyQue();

    ClearRegisteredItems();

    G_SpawnEntitiesFromString();

    G_FindTeams();

    G_CountSpawnSpots();

    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Init();
    }

    if (g_gametype.integer >= GT_TEAM) {
        G_CheckTeamItems();
    }

    SaveRegisteredItems();

    G_SetMatchGUID();
    STAT_ResetMedals();
    G_ClearTeamLocks();

    G_Printf("-----------------------------------\n");

    if (trap_Cvar_VariableIntegerValue("bot_enable")) {
        BotAISetup(restart);
        BotAILoadMap(restart);
        G_InitBots(restart);
    }

    trap_SetConfigstring(CS_INTERMISSION, "");

    switch (g_gametype.integer) {
    case GT_CA:
        level.roundState.tNext = 0;
        level.roundState.eCurrent = (level.warmupTime == 0) ? RS_COUNTDOWN : RS_WARMUP;
        CA_RoundStateTransition();
        break;
    case GT_CTF:
        G_InitItemTimerBases();
        break;
    case GT_FREEZE:
        level.roundState.tNext = 0;
        level.roundState.eCurrent = (level.warmupTime == 0) ? RS_COUNTDOWN : RS_WARMUP;
        Freeze_RoundStateTransition();
        break;
    case GT_AD:
        level.roundState.tNext = 0;
        level.roundState.eCurrent = (level.warmupTime == 0) ? RS_COUNTDOWN : RS_WARMUP;
        AD_RoundStateTransition();
        level.roundState.turn = 0;
        level.roundState.round = 0;
        level.roundState.capture = qfalse;
        level.roundState.touch = qfalse;
        ADScoreboardMessage(NULL);
        break;
    case GT_RR:
        RR_InitRoundState();
        break;
    default:
        break;
    }

    if (g_gametype.integer == GT_FFA && g_quadHog.integer) {
        QuadHog_Spawn();
    }
    G_SpawnRunes();

    trap_SetConfigstring(CS_STARTING_WEAPONS, va("%i", g_startingWeapons.integer));

    trap_SetConfigstring(CS_INFECTED_SURVIVOR_MINSPEED, va("%f", g_rrInfectedSurvivorMinSpeed.value));

    trap_SetConfigstring(CS_PRACTICE, va("%d", practiceflags.integer));
    trap_SetConfigstring(CS_FREECAM, va("%d", g_teamSpecFreeCam.integer));
    trap_SetConfigstring(CS_PLAYER_CYLINDERS, va("%d", g_playerCylinders.integer));
    trap_SetConfigstring(CS_DEBUGFLAGS, va("%d", g_debugFlags.integer));
    trap_SetConfigstring(CS_DMGTHROUGHDEPTH, va("%f", g_dmgThroughSurfaceDistance.value));

    G_SendPmoveInfo();
    G_SendArmorInfo();
    G_SendWeaponInfo();
    G_SendPlayerInfo();
}

void G_ShutdownGame(int restart) {
    G_SaveAccessList();
    G_Printf("==== ShutdownGame ====\n");

    if (level.logFile) {
        G_LogPrintf("ShutdownGame:\n");
        G_LogPrintf("------------------------------------------------------------\n");
        trap_FS_FCloseFile(level.logFile);
        level.logFile = 0;
    }

    G_WriteSessionData();

    if (trap_Cvar_VariableIntegerValue("bot_enable")) {
        BotAIShutdown(restart);
    }
}

void QDECL Com_Error(int level, const char* error, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    trap_Error(text);
}

void QDECL Com_Printf(const char* msg, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    trap_Print(text);
}

void RemoveTournamentLoser(void) {
    int clientNum;

    if (level.numPlayingClients != 2) {
        return;
    }

    clientNum = level.sortedClients[1];

    if (level.clients[clientNum].pers.connected != CON_CONNECTED) {
        return;
    }

    SetTeam(&g_entities[clientNum], "s");
}

void RemoveTournamentWinner(void) {
    int clientNum;

    if (level.numPlayingClients != 2) {
        return;
    }

    clientNum = level.sortedClients[0];

    if (level.clients[clientNum].pers.connected != CON_CONNECTED) {
        return;
    }

    SetTeam(&g_entities[clientNum], "s");
}

void AdjustTournamentScores(void) {
    int clientNum;

    clientNum = level.sortedClients[0];
    if (level.clients[clientNum].pers.connected == CON_CONNECTED) {
        level.clients[clientNum].sess.wins++;
        ClientUserinfoChanged(clientNum);
    }

    clientNum = level.sortedClients[1];
    if (level.clients[clientNum].pers.connected == CON_CONNECTED) {
        level.clients[clientNum].sess.losses++;
        ClientUserinfoChanged(clientNum);
    }
}

int QDECL SortRanks(const void* a, const void* b) {
    gclient_t *ca, *cb;

    ca = &level.clients[*(int*)a];
    cb = &level.clients[*(int*)b];

    if (ca->sess.spectatorClient < 0) {
        return 1;
    }
    if (cb->sess.spectatorClient < 0) {
        return -1;
    }

    if (ca->pers.connected == CON_CONNECTING) {
        return 1;
    }
    if (cb->pers.connected == CON_CONNECTING) {
        return -1;
    }

    if (ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR) {
        if (ca->sess.spectatorTime < cb->sess.spectatorTime) {
            return -1;
        }
        if (ca->sess.spectatorTime > cb->sess.spectatorTime) {
            return 1;
        }
        return 0;
    }
    if (ca->sess.sessionTeam == TEAM_SPECTATOR) {
        return 1;
    }
    if (cb->sess.sessionTeam == TEAM_SPECTATOR) {
        return -1;
    }

    if (g_gametype.integer == GT_RACE) {
        if (ca->ps.persistant[PERS_SCORE] > cb->ps.persistant[PERS_SCORE]) {
            return 1;
        }
        if (ca->ps.persistant[PERS_SCORE] < cb->ps.persistant[PERS_SCORE]) {
            return -1;
        }
        return 0;
    }
    if (ca->ps.persistant[PERS_SCORE] > cb->ps.persistant[PERS_SCORE]) {
        return -1;
    }
    if (ca->ps.persistant[PERS_SCORE] < cb->ps.persistant[PERS_SCORE]) {
        return 1;
    }

    if (g_gametype.integer == GT_RR) {
        if (ca->ps.localPersistant[0] > cb->ps.localPersistant[0]) {
            return -1;
        }
        if (ca->ps.localPersistant[0] < cb->ps.localPersistant[0]) {
            return 1;
        }
    }
    return 0;
}

static void G_AwardConfigstring(int index, int clientNum) {
    trap_SetConfigstring(index, clientNum < 0 ? "" : va("%i", clientNum));
}

void CalculateAwards(void) {
    int mostAccurate = -1, mostDamage = -1, itemControl = -1;
    int offensive = -1, defensive = -1, valuable = -1;
    int bestAccuracy = 0, accuracyShots = 0;
    int bestDamage = 0, damageShots = 0;
    int bestItems = 0;
    int bestOffense = 0, bestDefense = 0, bestValue = 0;
    int offenseScore = -999, defenseScore = -999, valueScore = -999;
    qboolean hasItems, hasValuable;
    int players = 0;
    int i;

    hasItems = (qboolean)(g_gametype.integer != GT_RACE && g_gametype.integer != GT_CA &&
                          g_gametype.integer != GT_DOMINATION && g_gametype.integer != GT_AD &&
                          g_gametype.integer != GT_RR);
    hasValuable = (qboolean)((g_gametype.integer >= GT_CTF && g_gametype.integer <= GT_HARVESTER) ||
                             g_gametype.integer == GT_DOMINATION || g_gametype.integer == GT_AD);

    for (i = 0; i < level.maxclients; i++) {
        gclient_t* cl = &level.clients[i];
        expandedStatObj_t* st = &cl->expandedStats;
        int accuracy = 0;
        int score = cl->ps.persistant[PERS_SCORE];

        if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        players++;

        if (cl->accuracy_hits > 0 && cl->accuracy_shots > 0) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        }
        if (accuracy == bestAccuracy ? cl->accuracy_shots > accuracyShots : accuracy > bestAccuracy) {
            bestAccuracy = accuracy;
            accuracyShots = cl->accuracy_shots;
            mostAccurate = i;
        }

        if (st->totalDamageDealt == bestDamage ? cl->accuracy_shots < damageShots : st->totalDamageDealt > bestDamage) {
            bestDamage = st->totalDamageDealt;
            damageShots = cl->accuracy_shots;
            mostDamage = i;
        }

        if (hasItems) {
            int items = (st->numGreenArmorPickups +
                         (st->numYellowArmorPickups + (st->numRedArmorPickups + st->numMegaHealthPickups) * 2) * 2) *
                            25 +
                        (st->numBattleSuitPickups + st->numQuadDamagePickups) * 480;

            if (items > bestItems) {
                bestItems = items;
                itemControl = i;
            }
        }

        if (hasValuable) {
            int defense = (st->numDefends + st->numAssists) * 10;
            int offense = (st->numAssists + st->numCaptures * 10) * 10;
            int value = st->numCaptures * 25 + (st->maxKillStreak - st->numDeaths) + st->numKills + defense;

            if (offense > bestOffense || (offense == bestOffense && score > offenseScore)) {
                bestOffense = offense;
                offenseScore = score;
                offensive = i;
            }
            if (defense > bestDefense || (defense == bestDefense && score > defenseScore)) {
                bestDefense = defense;
                defenseScore = score;
                defensive = i;
            }
            if (value > bestValue || (value == bestValue && score > valueScore)) {
                bestValue = value;
                valueScore = score;
                valuable = i;
            }
        }
    }

    if (!players) {
        return;
    }
    G_AwardConfigstring(CS_MOST_ACCURATE_PLYR, mostAccurate);
    G_AwardConfigstring(CS_MOST_DAMAGEDEALT_PLYR, mostDamage);
    if (hasItems) {
        G_AwardConfigstring(CS_BEST_ITEMCONTROL_PLYR, itemControl);
    }
    if (hasValuable) {
        G_AwardConfigstring(CS_MOST_VALUABLE_OFFENSIVE_PLYR, offensive);
        G_AwardConfigstring(CS_MOST_VALUABLE_DEFENSIVE_PLYR, defensive);
        G_AwardConfigstring(CS_MOST_VALUABLE_PLYR, valuable);
    }
}

void CalculateRanks(void) {
    int i;
    int rank;
    int score;
    int newScore;
    gclient_t* cl;

    level.follow1 = -1;
    level.follow2 = -1;
    level.numConnectedClients = 0;
    level.numNonSpectatorClients = 0;
    level.numPlayingClients = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected != CON_DISCONNECTED) {
            level.sortedClients[level.numConnectedClients] = i;
            level.numConnectedClients++;

            if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR) {
                level.numNonSpectatorClients++;

                if (level.clients[i].pers.connected == CON_CONNECTED) {
                    level.numPlayingClients++;
                    if (level.follow1 == -1) {
                        level.follow1 = i;
                    } else if (level.follow2 == -1) {
                        level.follow2 = i;
                    }
                }
            }
        }
    }

    qsort(level.sortedClients, level.numConnectedClients,
          sizeof(level.sortedClients[0]), SortRanks);

    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        int teamLastIdx[4] = { -1, -1, -1, -1 };
        int teamLastScore[4] = { 0, 0, 0, 0 };
        int teamRankCounter[4] = { -1, -1, -1, -1 };
        rank = -1;
        score = 0;

        for (i = 0; i < level.numConnectedClients; i++) {
            int ci = level.sortedClients[i];
            cl = &level.clients[ci];

            if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE]) {
                cl->ps.persistant[PERS_RANK] = 2;
            } else if (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE]) {
                cl->ps.persistant[PERS_RANK] = 0;
            } else {
                cl->ps.persistant[PERS_RANK] = 1;
            }

            if (cl->sess.sessionTeam == TEAM_SPECTATOR)
                continue;

            newScore = cl->ps.persistant[PERS_SCORE];

            if (i == 0 || newScore != score) {
                rank++;
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qfalse;
            } else {
                if (i > 0) {
                    level.clients[level.sortedClients[i - 1]].expandedStats.serverRankIsTied = qtrue;
                }
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qtrue;
            }

            {
                int t = cl->sess.sessionTeam;
                if (teamLastIdx[t] == -1 || newScore != teamLastScore[t]) {
                    teamRankCounter[t]++;
                    cl->expandedStats.teamRank = teamRankCounter[t];
                    cl->expandedStats.teamRankIsTied = qfalse;
                } else {
                    level.clients[level.sortedClients[teamLastIdx[t]]].expandedStats.teamRankIsTied = qtrue;
                    cl->expandedStats.teamRank = teamRankCounter[t];
                    cl->expandedStats.teamRankIsTied = qtrue;
                }
                teamLastIdx[t] = i;
                teamLastScore[t] = newScore;
            }

            score = newScore;
        }
    } else {
        int raceSentinel = (g_gametype.integer == GT_RACE) ? 0x7FFFFFFF : 0;
        rank = -1;
        score = raceSentinel;
        for (i = 0; i < level.numPlayingClients; i++) {
            int ci = level.sortedClients[i];
            cl = &level.clients[ci];
            newScore = cl->ps.persistant[PERS_SCORE];
            if (i == 0 || newScore != score) {
                rank = i;
                cl->ps.persistant[PERS_RANK] = rank;
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qfalse;
            } else {
                if (i > 0) {
                    level.clients[level.sortedClients[i - 1]].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
                    level.clients[level.sortedClients[i - 1]].expandedStats.serverRankIsTied = qtrue;
                }
                cl->ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qtrue;
            }
            score = newScore;
        }
    }

    if (level.lastLeadChangeTime == 0) {
        level.lastLeadChangeClient = -1;
        level.lastLeadChangeTime = level.time;
    } else if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        int newLead;
        if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE]) {
            newLead = 2;
        } else if (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE]) {
            newLead = 0;
        } else {
            newLead = 1;
        }
        if (level.lastLeadChangeClient != newLead && newLead != 2) {
            level.lastLeadChangeClient = newLead;
            level.lastLeadChangeElapsedTime = level.time - level.startTime;
        }
    } else {
        if (level.numPlayingClients > 0
            && level.lastLeadChangeClient != level.sortedClients[0]
            && level.clients[level.sortedClients[0]].expandedStats.serverRankIsTied == qfalse) {
            level.lastLeadChangeClient = level.sortedClients[0];
            level.lastLeadChangeElapsedTime = level.time - level.startTime;
        }
    }

    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        trap_SetConfigstring(CS_SCORES1, va("%i", level.teamScores[TEAM_RED]));
        trap_SetConfigstring(CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE]));
        trap_SetConfigstring(CS_SCORES1PLAYER, "TEAM_RED");
        trap_SetConfigstring(CS_SCORES2PLAYER, "TEAM_BLUE");
    } else {
        if (level.numPlayingClients < 1) {
            trap_SetConfigstring(CS_SCORES1, va("%i", SCORE_NOT_PRESENT));
            trap_SetConfigstring(CS_SCORES1PLAYER, "");
        } else {
            trap_SetConfigstring(CS_SCORES1, va("%i", level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE]));
            trap_SetConfigstring(CS_SCORES1PLAYER, level.clients[level.sortedClients[0]].pers.netname);
        }
        if (level.numPlayingClients < 2) {
            trap_SetConfigstring(CS_SCORES2, va("%i", SCORE_NOT_PRESENT));
            trap_SetConfigstring(CS_SCORES2PLAYER, "");
        } else {
            trap_SetConfigstring(CS_SCORES2, va("%i", level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE]));
            trap_SetConfigstring(CS_SCORES2PLAYER, level.clients[level.sortedClients[1]].pers.netname);
        }

        if (g_gametype.integer == GT_DUEL && !level.intermissionQueued && !level.intermissionTime) {
            int p1, p2;

            if (level.numPlayingClients == 0) {
                trap_SetConfigstring(CS_SCORE1STPLAYER, va("%i", SCORE_NOT_PRESENT));
                trap_SetConfigstring(CS_CLIENTNUM1STPLAYER, "-1");
                trap_SetConfigstring(CS_NAME1STPLAYER, "");
                trap_SetConfigstring(CS_SCORE2NDPLAYER, va("%i", SCORE_NOT_PRESENT));
                trap_SetConfigstring(CS_CLIENTNUM2NDPLAYER, "-1");
                trap_SetConfigstring(CS_NAME2NDPLAYER, "");
            } else {
                if (level.numPlayingClients >= 2) {
                    if (level.sortedClients[0] < level.sortedClients[1]) {
                        p1 = level.sortedClients[0];
                        p2 = level.sortedClients[1];
                    } else {
                        p1 = level.sortedClients[1];
                        p2 = level.sortedClients[0];
                    }
                } else {
                    p1 = level.sortedClients[0];
                    p2 = -1;
                }

                trap_SetConfigstring(CS_SCORE1STPLAYER, va("%i", level.clients[p1].ps.persistant[PERS_SCORE]));
                trap_SetConfigstring(CS_CLIENTNUM1STPLAYER, va("%i", p1));
                trap_SetConfigstring(CS_NAME1STPLAYER, level.clients[p1].pers.netname);

                if (p2 >= 0) {
                    trap_SetConfigstring(CS_SCORE2NDPLAYER, va("%i", level.clients[p2].ps.persistant[PERS_SCORE]));
                    trap_SetConfigstring(CS_CLIENTNUM2NDPLAYER, va("%i", p2));
                    trap_SetConfigstring(CS_NAME2NDPLAYER, level.clients[p2].pers.netname);
                } else {
                    trap_SetConfigstring(CS_SCORE2NDPLAYER, va("%i", SCORE_NOT_PRESENT));
                    trap_SetConfigstring(CS_CLIENTNUM2NDPLAYER, "-1");
                    trap_SetConfigstring(CS_NAME2NDPLAYER, "");
                }
            }
        }
    }

    if (level.intermissionTime) {
        for (i = 0; i < level.maxclients; i++) {
            if (level.clients[i].pers.connected == CON_CONNECTED) {
                MoveClientToIntermission(g_entities + i);
            }
        }
        CalculateAwards();
        SendScoreboardMessageToAllClients();
    }
}

void SendScoreboardMessageToAllClients(void) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            FreeForAllScoreboardMessage(g_entities + i);
        }
    }
}

void MoveClientToIntermission(gentity_t* ent) {
    if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
        StopFollowing(ent);
    }

    VectorCopy(level.intermission_origin, ent->s.origin);
    VectorCopy(level.intermission_origin, ent->client->ps.origin);
    VectorCopy(level.intermission_angle, ent->client->ps.viewangles);
    ent->client->ps.pm_type = PM_INTERMISSION;

    memset(ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups));

    ent->client->ps.eFlags = 0;
    ent->s.eFlags = 0;
    ent->s.eType = ET_GENERAL;
    ent->s.modelindex = 0;
    ent->s.loopSound = 0;
    ent->s.event = 0;
    ent->r.contents = 0;
}

void FindIntermissionPoint(void) {
    gentity_t *ent, *target;
    vec3_t dir;

    ent = G_Find(NULL, FOFS(classname), "info_player_intermission");
    if (!ent) {
        if (trap_Cvar_VariableIntegerValue("developer")) {
            G_Printf("WARNING: no info_player_intermission\n");
        }
        SelectSpawnPoint(vec3_origin, level.intermission_origin, level.intermission_angle, qfalse);
    } else {
        VectorCopy(ent->s.origin, level.intermission_origin);
        VectorCopy(ent->s.angles, level.intermission_angle);
        if (ent->target) {
            target = G_PickTarget(ent->target);
            if (target) {
                VectorSubtract(target->s.origin, level.intermission_origin, dir);
                vectoangles(dir, level.intermission_angle);
            }
        }
    }
}

void BeginIntermission(void) {
    int i;
    gentity_t* ent;
    char nextmaps[MAX_STRING_CHARS];

    if (level.intermissionTime) {
        return;
    }

    trap_SetConfigstring(CS_INTERMISSION, "1");

    if (g_gametype.integer == GT_DUEL) {
        AdjustTournamentScores();
    }

    level.intermissionTime = level.time;

    FindIntermissionPoint();

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            level.clients[i].pers.voteState = VOTE_PENDING;
            level.clients[i].pers.lastMapVoteTime = 0;
            level.clients[i].pers.lastMapVoteIndex = 0;
        }
    }
    ClearVote();

    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (nextmaps[0]) {
        char mapInfo[MAX_INFO_STRING];
        char votes[MAX_INFO_STRING];

        for (i = 0; i < 3; i++) {
            char key[16];
            const char *val;

            Com_sprintf(key, sizeof(key), "map_%d", i);
            val = Info_ValueForKey(nextmaps, key);
            Q_strncpyz(level.intermissionMapNames[i], val, sizeof(level.intermissionMapNames[i]));

            Com_sprintf(key, sizeof(key), "title_%d", i);
            val = Info_ValueForKey(nextmaps, key);
            Q_strncpyz(level.intermissionMapTitles[i], val, sizeof(level.intermissionMapTitles[i]));

            Com_sprintf(key, sizeof(key), "cfg_%d", i);
            val = Info_ValueForKey(nextmaps, key);
            Q_strncpyz(level.intermissionMapConfigs[i], val, sizeof(level.intermissionMapConfigs[i]));

            level.intermissionMapVotes[i] = 0;
        }

        mapInfo[0] = '\0';
        votes[0] = '\0';
        for (i = 0; i < 3; i++) {
            Info_SetValueForKey(mapInfo, va("map_%i", i), level.intermissionMapNames[i]);
            Info_SetValueForKey(mapInfo, va("title_%i", i), level.intermissionMapTitles[i]);
            Info_SetValueForKey(mapInfo, va("gt_%i", i), Info_ValueForKey(nextmaps, va("gt_%i", i)));
            Info_SetValueForKey(votes, va("%i", i), "0");
        }

        trap_SetConfigstring(CS_ROTATIONMAPS, mapInfo);
        trap_SetConfigstring(CS_ROTATIONVOTES, votes);
        level.voteTime = level.time;
        trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
    }

    for (i = 0; i < level.maxclients; i++) {
        ent = g_entities + i;
        if (!ent->inuse || !ent->client) {
            continue;
        }
        MoveClientToIntermission(ent);
    }
    CalculateAwards();

    SendScoreboardMessageToAllClients();
}

void ExitLevel(void) {
    int i;
    gclient_t* cl;
    char mapname[MAX_STRING_CHARS];
    char serverinfo[MAX_INFO_STRING];
    char nextmaps[MAX_STRING_CHARS];
    int bestMap = 0;

    if ( g_training.integer ) {
        trap_SendConsoleCommand( EXEC_APPEND, "killserver\n" );
        return;
    }
    if ( sv_quitOnExitLevel.integer ) {
        trap_SendConsoleCommand( EXEC_APPEND, "quit\n" );
        return;
    }

    BotInterbreedEndMatch();

    trap_SetConfigstring(CS_INTERMISSION, "");

    if (g_gametype.integer == GT_DUEL) {
        if (!level.restarted) {
            RemoveTournamentLoser();
            trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
            level.restarted = qtrue;
            level.intermissionTime = 0;
        }
        return;
    }

    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (nextmaps[0] && !(g_voteFlags.integer & VF_ENDMAP_VOTING)) {
        {
            int tiedMaps[3];
            int numTied = 0;
            int bestVotes = 0;

            for ( i = 0; i < 3; i++ ) {
                if ( level.intermissionMapNames[i][0] ) {
                    if ( level.intermissionMapVotes[i] > bestVotes ) {
                        bestVotes = level.intermissionMapVotes[i];
                        numTied = 0;
                    }
                    if ( level.intermissionMapVotes[i] == bestVotes ) {
                        tiedMaps[numTied++] = i;
                    }
                }
            }
            if ( numTied > 0 ) {
                bestMap = tiedMaps[rand() % numTied];
            }
        }

        trap_GetServerinfo(serverinfo, sizeof(serverinfo));
        Q_strncpyz(mapname, Info_ValueForKey(serverinfo, "mapname"), sizeof(mapname));

        if (level.intermissionMapNames[bestMap][0]) {
            if (Q_stricmp(mapname, level.intermissionMapNames[bestMap]) == 0) {
                trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
                level.restarted = qtrue;
            } else {
                if (level.intermissionMapConfigs[bestMap][0]) {
                    trap_Cvar_Set("nextmap", va("map %s %s",
                                  level.intermissionMapNames[bestMap],
                                  level.intermissionMapConfigs[bestMap]));
                } else {
                    trap_Cvar_Set("nextmap", va("map %s",
                                  level.intermissionMapNames[bestMap]));
                }
                trap_SendConsoleCommand(EXEC_APPEND, "vstr nextmap\n");
            }
        } else {
            trap_SendConsoleCommand(EXEC_APPEND, "vstr nextmap\n");
        }
    } else {
        trap_SendConsoleCommand(EXEC_APPEND, "vstr nextmap\n");
    }

    ClearVote();

    level.intermissionTime = 0;

    level.teamScores[TEAM_RED] = 0;
    level.teamScores[TEAM_BLUE] = 0;
    level.teamScores[TEAM_FREE] = 0;
    level.lastTeamScores[TEAM_RED] = 0;
    level.lastTeamScores[TEAM_BLUE] = 0;
    level.lastTeamRoundScores[TEAM_RED] = 0;
    level.lastTeamRoundScores[TEAM_BLUE] = 0;

    for (i = 0; i < g_maxclients.integer; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        cl->ps.persistant[PERS_SCORE] = 0;
    }

    G_WriteSessionData();

    for (i = 0; i < g_maxclients.integer; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            level.clients[i].pers.connected = CON_CONNECTING;
        }
    }
}

void QDECL G_LogPrintf(const char* fmt, ...) {
    va_list argptr;
    char string[1024];
    int min, tens, sec;

    sec = (level.time - level.startTime) / 1000;

    min = sec / 60;
    sec -= min * 60;
    tens = sec / 10;
    sec -= tens * 10;

    Com_sprintf(string, sizeof(string), "%3i:%i%i ", min, tens, sec);

    va_start(argptr, fmt);
    Q_vsnprintf(string + 7, sizeof(string) - 7, fmt, argptr);
    va_end(argptr);

    if (g_dedicated.integer) {
        G_Printf("%s", string + 7);
    }

    if (!level.logFile) {
        return;
    }

    trap_FS_Write(string, strlen(string), level.logFile);
}

void LogExit(int isShutdown, int restart, const char* string) {
    int i, numSorted;
    gclient_t* cl;

    G_LogPrintf("Exit: %s\n", string);

    if (isShutdown == 0) {
        level.intermissionQueued = level.time;
    }

    numSorted = level.numConnectedClients;
    if (numSorted > 32) {
        numSorted = 32;
    }

    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        G_LogPrintf("red:%i  blue:%i\n",
                    level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]);
    }

    for (i = 0; i < numSorted; i++) {
        int ping;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        if (cl->pers.connected == CON_CONNECTING) {
            continue;
        }

        ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

        G_LogPrintf("score: %i  ping: %i  client: %i %s stats: %d \n",
                    cl->ps.persistant[PERS_SCORE], ping,
                    level.sortedClients[i], cl->pers.netname, cl->expandedStats.statId);
    }

    if (isShutdown == 0 && restart == 0) {
        gentity_t *te;
        if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
            te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->s.eventParm = (level.teamScores[TEAM_RED] <= level.teamScores[TEAM_BLUE])
                              ? GTS_BLUETEAM_WON : GTS_REDTEAM_WON;
            te->r.svFlags |= SVF_BROADCAST;
        } else {
            te = G_TempEntity(vec3_origin, EV_GAMEOVER);
            te->r.svFlags |= SVF_BROADCAST;
        }
    }

    for ( i = 0; i < level.numConnectedClients; i++ ) {
        cl = &level.clients[level.sortedClients[i]];
        if ( cl->sess.sessionTeam != TEAM_SPECTATOR ) {
            cl->sess.prevScore = cl->ps.persistant[PERS_SCORE];
        }
    }
    STAT_MatchEnd(isShutdown, restart, string);

    level.gameStatsReported = qtrue;

    G_LogPrintf("stats: %d\n", level.numPlayingClients);
}

void CheckIntermissionExit(void) {
    int i;
    gclient_t* cl;
    int readyMask;
    int timeout;
    char nextmaps[MAX_STRING_CHARS];

    G_AutoAction();

    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (nextmaps[0] && !(g_voteFlags.integer & VF_ENDMAP_VOTING)) {
        timeout = 20000;
    } else {
        timeout = 10000;
    }

    readyMask = 0;
    for (i = 0; i < g_maxclients.integer; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (g_entities[i].r.svFlags & SVF_BOT) {
            continue;
        }
        if (cl->pers.ready) {
            if (i < 16) {
                readyMask |= 1 << i;
            }
        }
    }

    for (i = 0; i < g_maxclients.integer; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
    }

    if (!level.votingEnded &&
        level.time >= level.intermissionTime + timeout) {
        level.votingEnded = qtrue;
    }

    if (level.time >= level.intermissionTime + timeout + 3000) {
        level.readyToExit = qtrue;
        level.exitTime = level.time;
        ExitLevel();
    }
}

qboolean ScoreIsTied(void) {
    int a, b;

    if (level.numPlayingClients < 2) {
        return qfalse;
    }

    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
    }

    a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
    b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

    return a == b;
}

void CheckExitRules(void) {
    int i;
    gclient_t* cl;
    if (level.intermissionTime) {
        CheckIntermissionExit();
        return;
    }

    if (level.intermissionQueued) {
        if (level.time - level.intermissionQueued < INTERMISSION_DELAY_TIME) {
            return;
        }
        level.intermissionQueued = 0;
        BeginIntermission();
        return;
    }

    if (level.warmupTime != 0) {
        return;
    }

    switch ( g_gametype.integer ) {
    case GT_CA:
    case GT_AD:
    case GT_RR:
        return;
    case GT_FREEZE:
        if ( level.roundState.eCurrent != 0 ) {
            return;
        }
        break;
    default:
        break;
    }

    if ( ScoreIsTied() ) {
        if ( !g_timelimit.integer ) {
            return;
        }
        if ( !g_overtime.integer ) {
            return;
        }
        if ( level.time - level.startTime < g_timelimit.integer * 60000 + level.timeOvertime ) {
            return;
        }
        {
            gentity_t *te = G_TempEntity( vec3_origin, EV_OVERTIME );
            te->r.svFlags |= SVF_BROADCAST;
        }
        level.timeOvertime += g_overtime.integer * 1000;
        return;
    }

    if (g_timelimit.integer && !level.warmupTime) {
        if (level.time - level.startTime >= g_timelimit.integer * 60000 + level.timeOvertime) {
            trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
            LogExit(0, 0, "Timelimit hit.");
            return;
        }
    }

    if (g_fraglimit.integer &&
        (g_gametype.integer == GT_FFA || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_TEAM)) {
        if (level.teamScores[TEAM_RED] >= g_fraglimit.integer) {
            trap_SendServerCommand(-1, "print \"Red hit the fraglimit.\n\"");
            LogExit(0, 0, "Fraglimit hit.");
            return;
        }

        if (level.teamScores[TEAM_BLUE] >= g_fraglimit.integer) {
            trap_SendServerCommand(-1, "print \"Blue hit the fraglimit.\n\"");
            LogExit(0, 0, "Fraglimit hit.");
            return;
        }

        for (i = 0; i < g_maxclients.integer; i++) {
            cl = level.clients + i;
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            if (cl->sess.sessionTeam != TEAM_FREE) {
                continue;
            }

            if (cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer) {
                LogExit(0, 0, "Fraglimit hit.");
                trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
                                              cl->pers.netname));
                return;
            }
        }
    }

    if (g_capturelimit.integer &&
        g_gametype.integer >= GT_CTF && g_gametype.integer <= GT_HARVESTER) {
        if (level.teamScores[TEAM_RED] >= g_capturelimit.integer) {
            trap_SendServerCommand(-1, "print \"Red hit the capturelimit.\n\"");
            LogExit(0, 0, "Capturelimit hit.");
            return;
        }

        if (level.teamScores[TEAM_BLUE] >= g_capturelimit.integer) {
            trap_SendServerCommand(-1, "print \"Blue hit the capturelimit.\n\"");
            LogExit(0, 0, "Capturelimit hit.");
            return;
        }
    }

    if ( g_scorelimit.integer && g_gametype.integer == GT_DOMINATION ) {
        if ( level.teamScores[TEAM_RED] >= g_scorelimit.integer ) {
            trap_SendServerCommand( -1, "print \"Red hit the scorelimit.\n\"" );
            LogExit( 0, 0, "Scorelimit hit." );
            return;
        }
        if ( level.teamScores[TEAM_BLUE] >= g_scorelimit.integer ) {
            trap_SendServerCommand( -1, "print \"Blue hit the scorelimit.\n\"" );
            LogExit( 0, 0, "Scorelimit hit." );
            return;
        }
    }

    if ( g_mercylimit.integer && g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR ) {
        int mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
        if ( level.time - level.startTime >= mercyTime ) {
            int diff = level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE];
            if ( diff >= g_mercylimit.integer ) {
                trap_SendServerCommand( -1, "print \"Red hit the mercylimit.\n\"" );
                LogExit( 0, 0, "Mercylimit hit." );
                return;
            }
            if ( -diff >= g_mercylimit.integer ) {
                trap_SendServerCommand( -1, "print \"Blue hit the mercylimit.\n\"" );
                LogExit( 0, 0, "Mercylimit hit." );
                return;
            }
        }
    }

}

void SetWarmupState(int warmupTime) {
    const char *gameState;

    level.warmupTime = warmupTime;
    trap_SetConfigstring(CS_WARMUP, va("\\time\\%i", warmupTime));

    if (warmupTime < 0) {
        gameState = "PRE_GAME";
    } else if (warmupTime == 0) {
        gameState = "IN_PROGRESS";
    } else {
        gameState = "COUNT_DOWN";
    }
    trap_Cvar_Set("g_gameState", gameState);

    G_AutoAction();
}

static void G_AutoActionClean(char* out, const char* in) {
    int len = 0;

    while (*in && len < 39) {
        if (*in == '^') {
            in += in[1] ? 2 : 1;
            continue;
        }
        if (*in > ' ' && *in < 127 && !strchr("\"'*./:<>?\\|", *in)) {
            out[len++] = *in;
        }
        in++;
    }
    out[len] = '\0';
    if (!len) {
        Q_strncpyz(out, "invalid", 40);
    }
}

static const char* G_AutoActionGameType(int gametype) {
    switch (gametype) {
        case GT_FFA: return "FFA";
        case GT_DUEL: return "DUEL";
        case GT_RACE: return "RACE";
        case GT_TEAM: return "TDM";
        case GT_CA: return "CA";
        case GT_CTF: return "CTF";
        case GT_1FCTF: return "1F";
        case GT_OBELISK: return "OB";
        case GT_HARVESTER: return "HAR";
        case GT_FREEZE: return "FT";
        case GT_DOMINATION: return "DOM";
        case GT_AD: return "AD";
        case GT_RR: return "RR";
        default: return "ERR";
    }
}

static const char* G_AutoActionName(gentity_t* ent) {
    static char name[256];
    char first[40], second[40], raw[40], info[MAX_INFO_STRING];
    int clientNum = ent - g_entities;
    qtime_t t;

    name[0] = '\0';
    if (g_gametype.integer == GT_DUEL) {
        trap_GetConfigstring(CS_SCORES1PLAYER, raw, sizeof(raw));
        G_AutoActionClean(first, raw);
        trap_GetConfigstring(CS_SCORES2PLAYER, raw, sizeof(raw));
        G_AutoActionClean(second, raw);
        if (clientNum == level.sortedClients[0]) {
            Q_strncpyz(name, va("%s(POV)-vs-%s", first, second), sizeof(name));
        } else if (clientNum == level.sortedClients[1]) {
            Q_strncpyz(name, va("%s(POV)-vs-%s", second, first), sizeof(name));
        } else {
            Q_strncpyz(name, va("%s-vs-%s", first, second), sizeof(name));
        }
    } else {
        if (g_instaGib.integer) {
            Q_strcat(name, sizeof(name), "i");
        }
        Q_strcat(name, sizeof(name), G_AutoActionGameType(g_gametype.integer));
        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
            G_AutoActionClean(first, ent->client->pers.netname);
            Q_strcat(name, sizeof(name), va("-%s", first));
        }
    }
    trap_GetServerinfo(info, sizeof(info));
    Q_strcat(name, sizeof(name), va("-%s", Info_ValueForKey(info, "mapname")));
    trap_RealTime(&t);
    Q_strcat(name, sizeof(name), va("-%d_%02d_%02d-%02d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1,
                                    t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec));
    return name;
}

static int autoActionFlags;

static void G_AutoActionStopRecording(void) {
    int i;

    if (!(autoActionFlags & 1)) {
        return;
    }
    autoActionFlags &= ~1;
    for (i = 0; i < level.maxclients; i++) {
        gentity_t* ent = &g_entities[i];
        if (ent->inuse && ent->client->pers.connected == CON_CONNECTED && (ent->client->pers.autoaction & 1)) {
            trap_SendServerCommand(i, "stoprecord");
        }
    }
}

void G_AutoActionClientConnect(gentity_t* ent) {
    if ((autoActionFlags & 1) && (ent->client->pers.autoaction & 1)) {
        trap_SendServerCommand(ent - g_entities, va("record \"%s\"\n", G_AutoActionName(ent)));
    }
}

void G_AutoAction(void) {
    int i;

    if (level.warmupTime == -1) {
        G_AutoActionStopRecording();
        autoActionFlags = 0;
        return;
    }
    if (level.warmupTime == 0) {
        if (!level.intermissionTime) {
            autoActionFlags |= 1;
            return;
        }
        if (level.time - level.intermissionTime > 4000 && !(autoActionFlags & 2)) {
            G_AutoActionStopRecording();
            autoActionFlags |= 2;
            for (i = 0; i < level.maxclients; i++) {
                gentity_t* ent = &g_entities[i];
                if (ent->inuse && ent->client->pers.connected == CON_CONNECTED && (ent->client->pers.autoaction & 2)) {
                    trap_SendServerCommand(i, va("screenshot \"%s\"\n", G_AutoActionName(ent)));
                }
            }
        }
        return;
    }
    if (!(autoActionFlags & 1)) {
        autoActionFlags |= 1;
        for (i = 0; i < level.maxclients; i++) {
            gentity_t* ent = &g_entities[i];
            if (ent->inuse && ent->client->pers.connected == CON_CONNECTED && (ent->client->pers.autoaction & 1)) {
                trap_SendServerCommand(i, va("record \"%s\"\n", G_AutoActionName(ent)));
            }
        }
    }
}

qboolean BG_IsScoreBasedGameType(void) {
    return g_gametype.integer < 0 || (g_gametype.integer > GT_RACE && g_gametype.integer != GT_RR);
}

qboolean G_CheckTeamBalance(void) {
    int redCount, blueCount;

    if (g_teamForceBalance.integer == 0)
        return qtrue;

    redCount = TeamCount(-1, TEAM_RED);
    blueCount = TeamCount(-1, TEAM_BLUE);

    if (redCount + 1 < blueCount || blueCount + 1 < redCount)
        return qfalse;

    return qtrue;
}

qboolean TeamsPresent(void) {
    int redCount, blueCount;

    if (g_teamForcePresent.integer == 0) {
        return qtrue;
    }

    blueCount = TeamCount(-1, TEAM_BLUE);
    redCount = TeamCount(-1, TEAM_RED);

    if (g_rrInfected.integer == 0 ||
        (blueCount < g_teamSizeMin.integer && level.numPlayingClients < g_teamSizeMin.integer)) {
        if (g_gametype.integer < GT_TEAM) {
            if (level.numPlayingClients < 2) {
                return qfalse;
            }
        } else {
            if (redCount < g_teamSizeMin.integer) {
                return qfalse;
            }
            if (blueCount < g_teamSizeMin.integer) {
                return qfalse;
            }
        }
    }

    return qtrue;
}

static qboolean CheckWarmupConditions(void) {
    int i;
    int numPlaying = 0;
    int numReady = 0;
    int redCount, blueCount;
    gclient_t *cl;

    if (g_doWarmup.integer == 0) {
        return qtrue;
    }

    if (g_warmupDelay.integer != 0 && g_gametype.integer != GT_DUEL &&
        level.time - level.startTime < g_warmupDelay.integer * 1000) {
        return qfalse;
    }

    if (level.warmupTime < 0) {
        int readyMask = 0;

        for (i = 0; i < level.maxclients; i++) {
            cl = level.clients + i;
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
                continue;
            }

            if (cl->pers.ready) {
                numReady++;
            } else {
                if (!(g_entities[i].r.svFlags & SVF_BOT) || bot_autoReady.integer == 0) {
                    continue;
                }
            }
            numPlaying++;
            if (i < 16) {
                readyMask |= (1 << i);
            }
        }
        level.numReadyClients = numPlaying;
        level.numReadyHumans = numReady;

        for (i = 0; i < level.maxclients; i++) {
            cl = level.clients + i;
            if (cl->pers.connected == CON_CONNECTED) {
                cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
            }
        }
    }

    if (g_teamForcePresent.integer != 0) {
        blueCount = TeamCount(-1, TEAM_BLUE);
        redCount = TeamCount(-1, TEAM_RED);

        if (g_rrInfected.integer == 0 ||
            (blueCount < g_teamSizeMin.integer && level.numPlayingClients < g_teamSizeMin.integer)) {
            if (g_gametype.integer < GT_TEAM) {
                if (level.numPlayingClients < 2) {
                    return qfalse;
                }
            } else {
                if (redCount < g_teamSizeMin.integer) {
                    return qfalse;
                }
                if (blueCount < g_teamSizeMin.integer) {
                    return qfalse;
                }
            }
        }
    }

    if (!G_CheckTeamBalance()) {
        return qfalse;
    }

    if (level.warmupTime < 0) {
        if (g_gametype.integer == GT_DUEL) {
            if (level.numPlayingClients == 2 && numPlaying == 2) {
                return qtrue;
            }
        } else {
            if ((numReady != 0 &&
                 sv_warmupReadyPercentage.value <= (float)numPlaying / (float)level.numPlayingClients) ||
                sv_warmupReadyPercentage.value == 0.0f) {
                return qtrue;
            }
        }
    } else {
        if (level.warmupTime != 0) {
            for (i = 0; i < level.maxclients; i++) {
                cl = level.clients + i;
                if (cl->pers.connected == CON_CONNECTED) {
                    cl->ps.pm_flags |= PMF_FROZEN;
                }
            }
        }
        return qtrue;
    }

    return qfalse;
}

qboolean CheckForfeitConditions(gentity_t *ent, int mode) {
    int elapsed;
    int redCount, blueCount;

    if (level.timePauseBegin != 0) {
        return qfalse;
    }

    elapsed = level.time - level.startTime;
    blueCount = TeamCount(-1, TEAM_BLUE);
    redCount = TeamCount(-1, TEAM_RED);

    if (mode != 0) {
        if (g_gametype.integer == GT_FFA || g_gametype.integer == GT_RACE ||
            g_gametype.integer == GT_RR) {
            const char *gtName;
            if (g_gametype.integer < 13) {
                gtName = gametypeDisplayNames[g_gametype.integer];
            } else {
                gtName = "Unknown Gametype";
            }
            trap_SendServerCommand(ent - g_entities,
                va("print \"Forfeit is not available in %s.\n\"", gtName));
            return qfalse;
        }

        if (g_allowForfeit.integer == 0) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeits are not enabled on this server.\n\"");
            return qfalse;
        }
        if (level.warmupTime != 0) {
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is not available in warmup.\n\"");
            return qfalse;
        }

        if (g_gametype.integer == GT_DUEL) {
            int rank = ent->client->ps.persistant[PERS_RANK];
            if (rank != 0 && (rank & ~0x4000) != 0) {
                ent->client->ps.persistant[PERS_SCORE] = -999;
                return qtrue;
            }
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is only available to the losing player.\n\"");
            return qfalse;
        }

        if (g_gametype.integer < GT_TEAM) {
            return qfalse;
        }

        if (ent->client->sess.sessionTeam == TEAM_RED &&
            level.teamScores[TEAM_RED] < level.teamScores[TEAM_BLUE]) {
            if (redCount == 1) {
                ent->client->ps.persistant[PERS_SCORE] = -999;
                return qtrue;
            }
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is only available to the last remaining player on the losing team.\n\"");
        } else if (ent->client->sess.sessionTeam == TEAM_BLUE &&
                   level.teamScores[TEAM_BLUE] < level.teamScores[TEAM_RED]) {
            if (blueCount == 1) {
                ent->client->ps.persistant[PERS_SCORE] = -999;
                return qtrue;
            }
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is only available to the last remaining player on the losing team.\n\"");
        } else {
            trap_SendServerCommand(ent - g_entities,
                "print \"Forfeit is only available to members of the losing team.\n\"");
        }
        return qfalse;
    }

    if (elapsed / 1000 > 1 && level.intermissionQueued == 0 && level.intermissionTime == 0) {
        if (BG_IsRoundBasedGameType(g_gametype.integer)) {
            if (level.roundState.eCurrent < RS_PLAYING) {
                return qfalse;
            }
        } else {
            if (level.warmupTime != 0) {
                return qfalse;
            }
        }

        if (g_training.integer == 0) {
            if (((g_gametype.integer < GT_TEAM || g_gametype.integer == GT_RR) &&
                 level.numPlayingClients < 2) ||
                (BG_IsScoreBasedGameType() && (redCount < 1 || blueCount < 1))) {
                return qtrue;
            }
        }
    }

    return qfalse;
}

void ForfeitMatch(void) {
    int redCount, blueCount;

    if (g_gametype.integer == GT_DUEL) {
        gclient_t *cl;
        cl = level.clients + level.clientNum1stPlayer;
        if (cl->pers.connected == CON_DISCONNECTED || cl->sess.sessionTeam != TEAM_FREE) {
            cl->ps.persistant[PERS_SCORE] = -999;
        }
        cl = level.clients + level.clientNum2ndPlayer;
        if (cl->pers.connected == CON_DISCONNECTED || cl->sess.sessionTeam != TEAM_FREE) {
            cl->ps.persistant[PERS_SCORE] = -999;
        }
    } else if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        blueCount = TeamCount(-1, TEAM_BLUE);
        redCount = TeamCount(-1, TEAM_RED);
        if (redCount < 1) {
            level.teamScores[TEAM_RED] = -999;
            trap_SetConfigstring(CS_SCORES1, va("%i", -999));
        }
        if (blueCount < 1) {
            level.teamScores[TEAM_BLUE] = -999;
            trap_SetConfigstring(CS_SCORES2, va("%i", -999));
        }
    }

    level.matchForfeited = qtrue;
    trap_SendServerCommand(-1, "print \"Game has been forfeited.\n\"");
    LogExit(0, 0, "Players have forfeited.");
}

static void CheckWarmupAndForfeit(void) {
    int i;
    gclient_t *cl;

    if (level.numPlayingClients == 0) {
        return;
    }

    if (level.warmupTime == 0) {
        if (CheckForfeitConditions(NULL, 0)) {
            ForfeitMatch();
        }
        return;
    }

    if (!CheckWarmupConditions()) {
        if (level.warmupTime == -1) {
            CheckTournament();
            return;
        }

        SetWarmupState(-1);
        G_LogPrintf("Warmup:\n");

        for (i = 0; i < level.maxclients; i++) {
            cl = level.clients + i;
            if (cl->pers.connected == CON_CONNECTED) {
                cl->ps.pm_flags &= ~PMF_FROZEN;
                if (cl->pers.ready == 1) {
                    cl->pers.ready = 0;
                    ClientUserinfoChanged(cl->ps.clientNum);
                }
            }
        }
        return;
    }

    if (g_warmup.modificationCount != level.warmupModificationCount) {
        level.warmupModificationCount = g_warmup.modificationCount;
        SetWarmupState(-1);
    }

    if (level.warmupTime < 0) {
        SetWarmupState(g_warmup.integer * 1000 + level.time);
        return;
    }

    if (level.time >= level.warmupTime) {
        STAT_MatchStarted();

        level.warmupTime += 10000;
        trap_Cvar_Set("g_restarted", "1");
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        level.restarted = qtrue;
        return;
    }
}

static qboolean ExecuteVoteCommand(const char *voteCmd) {
    char arg0[MAX_STRING_TOKENS];
    char arg1[MAX_STRING_TOKENS];
    char *p;

    Q_strncpyz(arg0, voteCmd, sizeof(arg0));
    p = strchr(arg0, ' ');
    if (p) {
        *p = '\0';
        Q_strncpyz(arg1, p + 1, sizeof(arg1));
        if (arg1[0] == '"') {
            int len = strlen(arg1);
            if (len > 1 && arg1[len-1] == '"') {
                arg1[len-1] = '\0';
            }
            memmove(arg1, arg1 + 1, strlen(arg1));
        }
    } else {
        arg1[0] = '\0';
    }

    if (!Q_stricmp(arg0, "cointoss")) {
        if (rand() & 1) {
            trap_SendServerCommand(-1, "print \"^3The coin is: ^5HEADS^7\n\"");
        } else {
            trap_SendServerCommand(-1, "print \"^3The coin is: ^5TAILS^7\n\"");
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "random")) {
        int val = atoi(arg1);
        if (val == 2) {
            if (rand() & 1) {
                trap_SendServerCommand(-1, "print \"^3The coin is: ^5HEADS^7\n\"");
            } else {
                trap_SendServerCommand(-1, "print \"^3The coin is: ^5TAILS^7\n\"");
            }
        } else if (val >= 3 && val <= 100) {
            trap_SendServerCommand(-1,
                va("print \"^3Random number is: ^5%d^7\n\"", (rand() % val) + 1));
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "loadouts")) {
        if (!Q_stricmp(arg1, "ON")) {
            trap_Cvar_Set("g_loadout", "1");
        } else if (!Q_stricmp(arg1, "OFF")) {
            trap_Cvar_Set("g_loadout", "0");
        } else {
            trap_SendServerCommand(-1,
                "print \"^3Valid loadout options are:    ^5ON    ^5OFF^7\n\"");
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "ammo")) {
        if (!Q_stricmp(arg1, "GLOBAL")) {
            trap_Cvar_Set("g_ammoPack", "1");
        } else if (!Q_stricmp(arg1, "WEAP")) {
            trap_Cvar_Set("g_ammoPack", "0");
        } else {
            trap_SendServerCommand(-1,
                "print \"^3Valid loadout options are:    ^5GLOBAL    ^5WEAP^7\n\"");
            return qtrue;
        }
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        return qtrue;
    }
    if (!Q_stricmp(arg0, "timers")) {
        if (!Q_stricmp(arg1, "ON")) {
            trap_Cvar_Set("g_itemTimers", "1");
        } else if (!Q_stricmp(arg1, "OFF")) {
            trap_Cvar_Set("g_itemTimers", "0");
        } else {
            trap_SendServerCommand(-1,
                "print \"^3Valid item timer options are:    ^5ON    ^5OFF^7\n\"");
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "shuffle")) {
        G_ShuffleTeams();
        return qtrue;
    }
    if (!Q_stricmp(arg0, "teamsize")) {
        trap_Cvar_Set("teamsize", arg1);
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        return qtrue;
    }
    if (!Q_stricmp(arg0, "weaprespawn")) {
        trap_Cvar_Set("g_weaponRespawn", arg1);
        return qtrue;
    }

    return qfalse;
}

static void G_CheckAutoShuffle(void) {
    int fin;

    if (!g_shuffle_automatic.integer) {
        return;
    }

    if (!level.autoShuffleStart) {
        if (level.numPlayingClients < g_shuffle_automatic_minplayers.integer) {
            level.autoShuffleArmed = qtrue;
            return;
        }
        if (!level.autoShuffleArmed || !level.time) {
            return;
        }
        level.autoShuffleStart = level.time;
    }

    if (level.warmupTime >= 0) {
        return;
    }

    fin = level.autoShuffleStart + g_shuffle_timedelay.integer;
    if (level.time < fin) {
        int quedan = (fin - level.time) / 1000;

        if (quedan != (fin - (level.time - level.frametime)) / 1000) {
            trap_SendServerCommand(-1, va("cp \"Auto-Shuffling Teams in %d seconds.\n\"", quedan));
        }
        return;
    }

    G_ShuffleTeams();
}

void CheckVote(void) {
    int i;
    int voteYes, voteNo, voteEligible;
    gentity_t *ent;

    if (level.intermissionTime) {
        return;
    }

    if (level.voteExecuteTime && level.voteExecuteTime < level.time) {
        level.voteExecuteTime = 0;
        if (!ExecuteVoteCommand(level.voteString)) {
            trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
        }
    }

    if (!level.voteTime) {
        return;
    }

    voteYes = 0;
    voteNo = 0;
    voteEligible = 0;

    for (i = 0; i < level.maxclients; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || ent->client->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }

        switch (ent->client->pers.voteState) {
            case VOTE_YES:
                voteYes++;
                voteEligible++;
                break;
            case VOTE_NO:
                voteNo++;
            case VOTE_PENDING:
                voteEligible++;
                break;
            case VOTE_PASSED:
                trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " passed the vote.\n\"",
                    ent->client->pers.netname));
                goto vote_passed;
            case VOTE_VETOED:
                trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " vetoed the vote.\n\"",
                    ent->client->pers.netname));
                goto vote_failed;
            default:
                break;
        }
    }

    if ((float)voteYes > (float)voteEligible * 0.5f) {
vote_passed:
        trap_SendServerCommand(-1, "print \"Vote passed.\n\"");
        ClearVote();
        level.voteExecuteTime = level.time + 3000;
    } else if ((float)voteNo >= (float)voteEligible * 0.5f) {
vote_failed:
        trap_SendServerCommand(-1, "print \"Vote failed.\n\"");
        ClearVote();
    } else {
        if (level.voteYes != voteYes) {
            level.voteYes = voteYes;
            trap_SetConfigstring(CS_VOTE_YES, va("%i", voteYes));
        }
        if (level.voteNo != voteNo) {
            level.voteNo = voteNo;
            trap_SetConfigstring(CS_VOTE_NO, va("%i", voteNo));
        }
        if (level.time - level.voteTime > 29999) {
            trap_SendServerCommand(-1, "print \"Vote failed.\n\"");
            ClearVote();
        }
    }
}

void PrintTeam(int team, char* message) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].sess.sessionTeam != team)
            continue;
        trap_SendServerCommand(i, message);
    }
}

void SetLeader(int team, int client) {
    int i;

    if (level.clients[client].pers.connected == CON_DISCONNECTED) {
        PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname));
        return;
    }
    if (level.clients[client].sess.sessionTeam != team) {
        PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname));
        return;
    }
    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].sess.sessionTeam != team)
            continue;
        if (level.clients[i].sess.teamLeader) {
            level.clients[i].sess.teamLeader = qfalse;
            ClientUserinfoChanged(i);
        }
    }
    level.clients[client].sess.teamLeader = qtrue;
    ClientUserinfoChanged(client);
}

void CheckTeamLeader(int team) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].sess.sessionTeam != team)
            continue;
        if (level.clients[i].sess.teamLeader)
            break;
    }
    if (i >= level.maxclients) {
        for (i = 0; i < level.maxclients; i++) {
            if (level.clients[i].sess.sessionTeam != team)
                continue;
            if (!(g_entities[i].r.svFlags & SVF_BOT)) {
                level.clients[i].sess.teamLeader = qtrue;
                break;
            }
        }

        if (i >= level.maxclients) {
            for (i = 0; i < level.maxclients; i++) {
                if (level.clients[i].sess.sessionTeam != team)
                    continue;
                level.clients[i].sess.teamLeader = qtrue;
                break;
            }
        }
    }
}

void CheckCvars(void) {
    static int lastMod = -1;

    if (g_password.modificationCount != lastMod) {
        lastMod = g_password.modificationCount;
        if (*g_password.string && Q_stricmp(g_password.string, "none")) {
            trap_Cvar_Set("g_needpass", "1");
        } else {
            trap_Cvar_Set("g_needpass", "0");
        }
    }
}

static void G_UpdateTeamCounts(void) {
    int red, blue;

    if (level.time > level.lastTeamCountSent && level.time - level.lastTeamCountSent <= 250) {
        return;
    }
    level.lastTeamCountSent = level.time;

    if (level.roundState.eCurrent == RS_WARMUP) {
        red = TeamCount(-1, TEAM_RED);
        blue = TeamCount(-1, TEAM_BLUE);
    } else {
        Team_LivingTeamCounts(&red, &blue);
    }
    trap_SetConfigstring(CS_TEAMCOUNT_RED, va("%i", red));
    trap_SetConfigstring(CS_TEAMCOUNT_BLUE, va("%i", blue));
}

void G_RunThink(gentity_t* ent) {
    int thinktime;

    if (ent->framethink) {
        ent->framethink(ent);
    }

    thinktime = ent->nextthink;
    if (thinktime <= 0) {
        return;
    }
    if (thinktime > level.time) {
        return;
    }

    ent->nextthink = 0;
    if (!ent->think) {
        G_Error("NULL ent->think");
    }
    ent->think(ent);
}

void G_RunFrame(int levelTime) {
    int i;
    gentity_t* ent;

    if (level.restarted) {
        return;
    }

    level.frametime = levelTime - level.time;
    level.time = levelTime;

    if (level.timePauseBegin) {
        level.startTime += level.frametime;
        MP_PauseThink();
    }

    G_UpdateCvars();

    if (g_suddenDeathRespawn.integer && !level.intermissionTime && !level.intermissionQueued) {
        int overtime = level.time - level.startTime - g_timelimit.integer * 60000;
        if (g_timelimit.integer && level.warmupTime == 0 && overtime > 0) {
            int tickMs = g_suddenDeathRespawnTick.integer * 1000;
            int delay = (tickMs > 0)
                            ? (overtime / tickMs) * g_suddenDeathRespawnIncrement.integer +
                                  g_suddenDeathRespawnStart.integer
                            : g_suddenDeathRespawnStart.integer;
            if (delay > g_suddenDeathRespawnMax.integer) {
                delay = g_suddenDeathRespawnMax.integer;
            }
            level.forceRespawnDelay = delay * 1000;
            if (level.forceRespawnDelay != level.suddenDeathRespawnDelayLastAnnounced) {
                trap_SendServerCommand(-1, va("cp \"%d second respawn delay\n\"",
                                              level.forceRespawnDelay / 1000));
                level.suddenDeathRespawnDelayLastAnnounced = level.forceRespawnDelay;
            }
        } else {
            level.forceRespawnDelay = g_respawn_delay_min.integer;
        }
    }

    if (pmoveInfoDirty) {
        G_SendPmoveInfo();
        pmoveInfoDirty = qfalse;
    }
    if (armorInfoDirty) {
        G_SendArmorInfo();
        armorInfoDirty = qfalse;
    }
    if (weaponInfoDirty) {
        G_SendWeaponInfo();
        weaponInfoDirty = qfalse;
    }
    if (playerInfoDirty) {
        G_SendPlayerInfo();
        playerInfoDirty = qfalse;
    }

    if (level.intermissionTime) {
        for (i = 0; i < level.maxclients; i++) {
            gclient_t* cl = &level.clients[i];
            int elapsed;
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            elapsed = level.time - cl->pers.lastMapVoteTime;
            if (elapsed > 2250 && elapsed - level.frametime <= 2250) {
                trap_SendServerCommand(i, "enable_vote_ui");
            }
        }
    }

    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }

        if (level.time - ent->eventTime > EVENT_VALID_MSEC) {
            if (ent->s.event) {
                ent->s.event = 0;
                if (ent->client) {
                    ent->client->ps.externalEvent = 0;
                }
            }
            if (ent->freeAfterEvent) {
                G_FreeEntity(ent);
                continue;
            } else if (ent->unlinkAfterEvent) {
                ent->unlinkAfterEvent = qfalse;
                trap_UnlinkEntity(ent);
            }
        }

        if (ent->freeAfterEvent) {
            continue;
        }

        if (!ent->r.linked && ent->neverFree) {
            continue;
        }

        if (ent->s.eType == ET_MISSILE) {
            G_RunMissile(ent);
            continue;
        }

        if (ent->s.eType == ET_GRAPPLE) {
            Weapon_HookThink(ent);
            continue;
        }

        if (ent->s.eType == ET_ITEM || ent->physicsObject) {
            G_RunItem(ent);
            continue;
        }

        if (ent->s.eType == ET_MOVER) {
            G_RunMover(ent);
            continue;
        }

        if (i < MAX_CLIENTS) {
            G_RunClient(ent);
            continue;
        }

        G_RunThink(ent);
    }

    ent = &g_entities[0];
    for (i = 0; i < level.maxclients; i++, ent++) {
        if (ent->inuse) {
            ClientEndFrame(ent);
        }
    }

    AddDuelPlayer();

    G_CheckPlayQueueChanged();

    if (g_gametype.integer == GT_CA || g_gametype.integer == GT_FREEZE ||
        g_gametype.integer == GT_AD || g_gametype.integer == GT_RR) {
        G_UpdateTeamCounts();
    }

    CheckWarmupAndForfeit();

    if (g_gametype.integer == GT_FFA) {
        if (g_quadHog.integer) {
            QuadHog_Check();
        }
    } else if (g_gametype.integer == GT_RR) {
        RR_Think();
    } else if (g_gametype.integer == GT_CTF) {
        Team_CheckLostFlag(TEAM_RED);
        Team_CheckLostFlag(TEAM_BLUE);
    } else if (g_gametype.integer == GT_DOMINATION) {
        DOM_CheckPlayers();
    } else if (g_gametype.integer == GT_AD) {
        if (!level.intermissionQueued && !level.intermissionTime && !level.warmupTime) {
            if (level.timePauseBegin != 0) {
                level.roundState.startTime += level.frametime;
            }
            if (AD_GetRoundState() == RS_PLAYING) {
                int redAlive, blueAlive;
                Team_LivingTeamCounts(&redAlive, &blueAlive);
                if (AD_CheckRoundEnd(redAlive, blueAlive)) {
                    level.roundState.tNext = 0;
                    level.roundState.eCurrent = RS_ROUND_OVER;
                    AD_RoundStateTransition();
                }
            }
        }
    } else if (g_gametype.integer == GT_CA) {
        if (!level.intermissionQueued && !level.intermissionTime && !level.warmupTime) {
            if (level.timePauseBegin != 0) {
                level.roundState.startTime += level.frametime;
            }
            if (CA_GetRoundState() == RS_PLAYING) {
                int redAlive, blueAlive;
                Team_LivingTeamCounts(&redAlive, &blueAlive);
                if (redAlive == 0 || blueAlive == 0 ||
                    (roundtimelimit.integer &&
                     level.time - level.roundState.startTime >= roundtimelimit.integer * 1000)) {
                    level.roundState.tNext = 0;
                    level.roundState.eCurrent = RS_ROUND_OVER;
                    CA_RoundStateTransition();
                }
            }
        }
    } else if (g_gametype.integer == GT_FREEZE) {
        Freeze_Think();
    }

    CheckExitRules();

    if (g_gametype.integer >= GT_TEAM) {
        CheckTeamStatus();
        G_CheckAutoShuffle();
    }

    CheckVote();

    CheckCvars();

}
