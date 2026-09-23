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
// cg_main.c -- initialization and primary entry point for cgame
#include "cg_local.h"

#include "../ui/ui_shared.h"
// display context for new ui stuff
displayContextDef_t cgDC;

// [QL] the modificationCount of the six model and skin forcing cvars as CG_UpdateCvars last saw
// them (0x100769fc, 0x100785a8..0x100785b8, all -1 at start)
static int cg_forceModelModCounts[6] = { -1, -1, -1, -1, -1, -1 };

void CG_Init(int serverMessageNum, int serverCommandSequence, int clientNum);
void CG_RegisterCvars(void);
void CG_Shutdown(void);
static float CG_ChatBarY(void);
static float CG_ChatBarWidth(void);
static int CG_ChatFieldChars(void);
static void CG_ClientSteamID(int clientNum, uint64_t* out);
static void CG_SetClientTalking(int clientNum, int talking);

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain(int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11) {
    floatint_t fi;

    switch (command) {
        case CG_INIT:
            CG_Init(arg0, arg1, arg2);
            return 0;
        case CG_REGISTER_CVARS:
            CG_RegisterCvars();
            return 0;
        case CG_SHUTDOWN:
            CG_Shutdown();
            return 0;
        case CG_CONSOLE_COMMAND:
            return CG_ConsoleCommand();
        case CG_DRAW_ACTIVE_FRAME:
            CG_DrawActiveFrame(arg0, arg1, arg2);
            return 0;
        case CG_CROSSHAIR_PLAYER:
            return CG_CrosshairPlayer();
        case CG_LAST_ATTACKER:
            return CG_LastAttacker();
        case CG_KEY_EVENT:
            CG_KeyEvent(arg0, arg1);
            return 0;
        case CG_MOUSE_EVENT:
            cgDC.cursorx = cgs.cursorX;
            cgDC.cursory = cgs.cursorY;
            CG_MouseEvent(arg0, arg1);
            return 0;
        case CG_EVENT_HANDLING:
            CG_EventHandling(arg0);
            return 0;
        // [QL] cgamex86.dll exports 21 entries (table at 0x100769a8); slots 10-14 and 17-19 below.
        case CG_LAST_CHAT_COMMAND:
            CG_LastChatCommand();
            return 0;
        case CG_LAST_CHAT_COMMAND2:
            CG_LastChatCommand2();
            return 0;
        case CG_SET_KEY_CATCHER:
            CG_SetKeyCatcher();
            return 0;
        case CG_CLEAR_KEY_CATCHER:
            CG_ClearKeyCatcher();
            return 0;
        case CG_GET_ACTIVE_FRAME:
            return CG_GetActiveFrame();
        // the chat bar's two floats travel in the return value's bits
        case CG_CHAT_BAR_Y:
            fi.f = CG_ChatBarY();
            return fi.i;
        case CG_CHAT_BAR_WIDTH:
            fi.f = CG_ChatBarWidth();
            return fi.i;
        case CG_CHAT_FIELD_CHARS:
            return CG_ChatFieldChars();
        case CG_CLIENT_STEAM_ID:
            CG_ClientSteamID(arg0, (uint64_t*)arg1);
            return 0;
        case CG_SET_CLIENT_TALKING:
            CG_SetClientTalking(arg0, arg1);
            return 0;
        case CG_DEMO_REWIND:
            CG_DemoRewind(arg0, arg1);
            return 0;
        default:
            CG_Error("vmMain: unknown command %i", command);
            break;
    }
    return -1;
}

/*
================
CG_ChatBarY, CG_ChatBarWidth, CG_ChatFieldChars

[QL] cgamex86.dll 0x100209e0, 0x10020a00 and 0x10020a20, exports 17-19: where the engine's
Con_DrawNotify puts the chat bar (quakelive_steam.exe 0x4b416b). It sits at y 413 across the 640
width with a 73-character field, and during the intermission at y 455, 300 wide, with 30.
================
*/
static float CG_ChatBarY(void) {
    qboolean intermission = (cg.predictedPlayerState.pm_type == PM_INTERMISSION);

    return intermission ? 455.0f : 413.0f;
}

static float CG_ChatBarWidth(void) {
    qboolean intermission = (cg.predictedPlayerState.pm_type == PM_INTERMISSION);

    return intermission ? 300.0f : 640.0f;
}

static int CG_ChatFieldChars(void) {
    qboolean intermission = (cg.predictedPlayerState.pm_type == PM_INTERMISSION);

    return intermission ? 30 : 73;
}

/*
================
CG_ClientSteamID

The Steam ID of whoever is speaking, the only thing the engine asks the cgame for out
of everything retail copies here. It decides with it whether that voice is muted.

[QL] 0x10020910, entry 15 of the table.
================
*/
static void CG_ClientSteamID(int clientNum, uint64_t* out) {
    *out = 0;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    if (!cgs.clientinfo[clientNum].infoValid) {
        return;
    }
    *out = cgs.clientinfo[clientNum].steamId;
}

/*
================
CG_SetClientTalking

The engine calls in every time voice arrives from somebody, and the talking indicator
comes from that.

[QL] 0x10020a40, entry 20 of the table.
================
*/
static void CG_SetClientTalking(int clientNum, int talking) {
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    cgs.clientinfo[clientNum].talking = (qboolean)talking;
    cgs.clientinfo[clientNum].talkingTime = cg.time;
}

cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
weaponInfo_t cg_weapons[MAX_WEAPONS];
itemInfo_t cg_items[MAX_ITEMS];

vmCvar_t cg_railTrailTime;
vmCvar_t cg_centertime;
vmCvar_t cg_swingSpeed;
vmCvar_t cg_shadows;
vmCvar_t cg_drawFPS;
vmCvar_t cg_drawSnapshot;
vmCvar_t cg_draw3dIcons;
vmCvar_t cg_drawIcons;
vmCvar_t cg_drawAmmoWarning;
vmCvar_t cg_drawCrosshair;
vmCvar_t cg_drawRewards;
vmCvar_t cg_drawRewardsRowSize;
vmCvar_t cg_crosshairSize;
vmCvar_t cg_crosshairX;
vmCvar_t cg_crosshairY;
vmCvar_t cg_crosshairHealth;
vmCvar_t cg_draw2D;
vmCvar_t cg_drawStatus;
vmCvar_t cg_animSpeed;
vmCvar_t cg_debugAnim;
vmCvar_t cg_debugEvents;
vmCvar_t cg_errorDecay;
vmCvar_t cg_nopredict;
vmCvar_t cg_noPlayerAnims;
vmCvar_t cg_showmiss;
vmCvar_t cg_footsteps;
vmCvar_t cg_addMarks;
vmCvar_t cg_brassTime;
vmCvar_t cg_viewsize;
vmCvar_t cg_drawGun;
vmCvar_t r_dynamicLight;  // [ioquakelive] 2 turns the effect lights off
vmCvar_t cg_gun_frame;
vmCvar_t cg_gun_x;
vmCvar_t cg_gun_y;
vmCvar_t cg_gun_z;
vmCvar_t cg_tracerChance;
vmCvar_t cg_tracerWidth;
vmCvar_t cg_tracerLength;
vmCvar_t cg_autoswitch;
vmCvar_t cg_ignore;
vmCvar_t cg_simpleItems;
vmCvar_t cg_skipItems;  // [QL] 0x10016026: at 1, the items are not drawn
vmCvar_t cg_fov;
vmCvar_t cg_zoomFov;
vmCvar_t cg_thirdPerson;
vmCvar_t cg_thirdPersonRange;
vmCvar_t cg_thirdPersonAngle;
vmCvar_t cg_lagometer;
vmCvar_t cg_drawAttacker;
vmCvar_t cg_teamChatTime;
vmCvar_t cg_teamChatHeight;
vmCvar_t cg_stats;
vmCvar_t cg_paused;
vmCvar_t cg_blood;

// [wolfcam] electrogib tuning - see cg_local.h
vmCvar_t cg_gibCount;
vmCvar_t cg_gibColor;
vmCvar_t cg_gibJump;
vmCvar_t cg_gibVelocity;
vmCvar_t cg_gibVelocityRandomness;
vmCvar_t cg_gibDirScale;
vmCvar_t cg_gibOriginOffset;
vmCvar_t cg_gibOriginOffsetZ;
vmCvar_t cg_gibRandomness;
vmCvar_t cg_gibRandomnessZ;
vmCvar_t cg_gibTime;
vmCvar_t cg_gibStepTime;
vmCvar_t cg_gibBounceFactor;
vmCvar_t cg_gibGravity;
vmCvar_t cg_gibSparksSize;
vmCvar_t cg_gibSparksColor;
vmCvar_t cg_gibSparksHighlight;
vmCvar_t cg_gibFloatingVelocity;
vmCvar_t cg_gibFloatingRandomness;
vmCvar_t cg_gibFloatingOriginOffset;
vmCvar_t cg_gibFloatingOriginOffsetZ;

vmCvar_t cg_predictItems;
vmCvar_t cg_deferPlayers;
vmCvar_t cg_drawTeamOverlay;
vmCvar_t cg_teamChatsOnly;

vmCvar_t cg_hudFiles;
vmCvar_t cg_scorePlum;
vmCvar_t cg_smoothClients;
vmCvar_t cg_pmove_msec;
vmCvar_t cg_cameraMode;
vmCvar_t cg_cameraOrbit;
vmCvar_t cg_cameraOrbitDelay;
vmCvar_t cg_timescaleFadeEnd;
vmCvar_t cg_timescaleFadeSpeed;
vmCvar_t cg_timescale;

vmCvar_t cg_smallFont;
vmCvar_t cg_bigFont;
vmCvar_t cg_noTaunt;

vmCvar_t cg_noProjectileTrail;
vmCvar_t cg_oldRocket;
vmCvar_t cg_oldPlasma;
vmCvar_t cg_trueLightning;

vmCvar_t cg_currentSelectedPlayer;
vmCvar_t cg_currentSelectedPlayerName;
vmCvar_t cg_enableDust;
vmCvar_t cg_recordSPDemo;
vmCvar_t cg_recordSPDemoName;
vmCvar_t ui_singlePlayerActive;  // [QL] missing from binary parity audit
vmCvar_t cg_obeliskRespawnDelay;
vmCvar_t cg_lightningStyle;
vmCvar_t cg_screenDamage;
vmCvar_t cg_kickScale;

// [QL] additional cvars
vmCvar_t cg_armorTiered;
vmCvar_t cg_allowTaunt;
vmCvar_t cg_announcer;
vmCvar_t cg_autoHop;
vmCvar_t cg_predictLocalRailshots;  // [QL] predicted railgun autofire
vmCvar_t cg_bob;
vmCvar_t cg_bubbleTrail;
vmCvar_t cg_buzzerSound;
vmCvar_t cg_crosshairBrightness;
vmCvar_t cg_crosshairColor;
vmCvar_t cg_crosshairHitColor;
vmCvar_t cg_crosshairHitStyle;
vmCvar_t cg_crosshairHitTime;
vmCvar_t cg_crosshairPulse;
vmCvar_t cg_damagePlum;
vmCvar_t cg_damagePlumColorStyle;
vmCvar_t cg_deadBodyColor;
vmCvar_t cg_deadBodyDarken;
vmCvar_t cg_drawCrosshairTeamHealth;
vmCvar_t cg_drawFragMessages;
vmCvar_t cg_drawFullWeaponBar;
vmCvar_t cg_drawItemPickups;
vmCvar_t cg_drawCheckpointRemaining;
vmCvar_t cg_drawSpecMessages;
vmCvar_t cg_drawPregameMessages;
vmCvar_t cg_drawTieredArmorAvailability;
vmCvar_t cg_enemyHeadColor;
vmCvar_t cg_enemyLowerColor;
vmCvar_t cg_enemyUpperColor;
vmCvar_t cg_flagStyle;
vmCvar_t cg_forceEnemyModel;
vmCvar_t cg_forceTeamModel;
vmCvar_t cg_hitBeep;
vmCvar_t cg_impactSparks;
vmCvar_t cg_impactSparksLifetime;
vmCvar_t cg_impactSparksSize;
vmCvar_t cg_impactSparksVelocity;
vmCvar_t cg_itemFx;
vmCvar_t cg_itemTimers;
vmCvar_t cg_killBeep;
vmCvar_t cg_levelTimerDirection;
vmCvar_t cg_lightningImpact;
vmCvar_t cg_railReloadTime;
vmCvar_t cg_lowAmmoWarningPercentile;
vmCvar_t cg_lowAmmoWarningSound;
vmCvar_t cg_muzzleFlash;
vmCvar_t cg_obituaryRowSize;
vmCvar_t cg_playerLean;
vmCvar_t cg_plasmaStyle;
vmCvar_t cg_railStyle;
vmCvar_t ch_animateRewardIcons;
vmCvar_t cg_animateChatBalloon;
vmCvar_t ch_animateScroll;
vmCvar_t cg_zoomAnimationTime;
vmCvar_t cg_drawBrightWeapons;
vmCvar_t ch_chatLines;
vmCvar_t ch_drawLocalTime;
vmCvar_t ch_drawTargetStatus;
vmCvar_t ch_drawKeys;
vmCvar_t ch_autoStats;
vmCvar_t cg_demoKeys;
vmCvar_t ch_followKiller;
vmCvar_t ch_obituaryBand;
vmCvar_t ch_obituaryY;
vmCvar_t ch_obituaryScale;
vmCvar_t cg_ambient;
vmCvar_t ch_predictHitSounds;
vmCvar_t ch_gameEvents;
vmCvar_t ch_recordMessage;
vmCvar_t cg_demoRecording;
vmCvar_t cg_demoName;
vmCvar_t cg_rocketStyle;
vmCvar_t cg_raceBeep;
vmCvar_t cg_screenDamage_Self;
vmCvar_t cg_screenDamage_Team;
vmCvar_t cg_simpleItemsBob;
vmCvar_t cg_simpleItemsHeightOffset;
vmCvar_t cg_simpleItemsRadius;
vmCvar_t cg_speedometer;
vmCvar_t cg_switchOnEmpty;
vmCvar_t cg_switchToEmpty;
vmCvar_t cg_teamHeadColor;
vmCvar_t cg_teamLowerColor;
vmCvar_t cg_teamUpperColor;
vmCvar_t cg_thirdPersonPitch;
vmCvar_t cg_trueShotgun;
vmCvar_t cg_vignette;
vmCvar_t cg_waterWarp;
vmCvar_t cg_weaponBar;
vmCvar_t cg_zoomOutOnDeath;
vmCvar_t cg_zoomScaling;
vmCvar_t cg_zoomSensitivity;
vmCvar_t cg_zoomToggle;
vmCvar_t cg_drawInputCmds;
vmCvar_t cg_drawTeamOverlayOpacity;
vmCvar_t cg_drawTeamOverlaySize;
vmCvar_t cg_drawTeamOverlayX;
vmCvar_t cg_drawTeamOverlayY;
vmCvar_t cg_impactMarkTime;
vmCvar_t cg_chatbeep;
vmCvar_t cg_teamChatBeep;
vmCvar_t cg_gametype;
vmCvar_t cg_svFps;
vmCvar_t cg_comMaxfps;

// [QL] announcer cvars
vmCvar_t cg_announcerLastStandingVO;
vmCvar_t cg_announcerLeadsVO;
vmCvar_t cg_announcerRewardsVO;
vmCvar_t cg_announcerTiesVO;

// [QL] sound cvars
vmCvar_t s_ambient;
vmCvar_t s_announcerVolume;
vmCvar_t s_killBeepVolume;

// [QL] weapon loadout/config cvars
vmCvar_t cg_weaponConfig;
vmCvar_t cg_weaponConfig_g;
vmCvar_t cg_weaponConfig_mg;
vmCvar_t cg_weaponConfig_sg;
vmCvar_t cg_weaponConfig_gl;
vmCvar_t cg_weaponConfig_rl;
vmCvar_t cg_weaponConfig_lg;
vmCvar_t cg_weaponConfig_rg;
vmCvar_t cg_weaponConfig_pg;
vmCvar_t cg_weaponConfig_bfg;
vmCvar_t cg_weaponConfig_gh;
vmCvar_t cg_weaponConfig_ng;
vmCvar_t cg_weaponConfig_pl;
vmCvar_t cg_weaponConfig_cg;
vmCvar_t cg_weaponConfig_hmg;
vmCvar_t cg_weaponColor_grenade;
vmCvar_t cg_weaponPrimary;

// [QL] spectator/demo cvars
vmCvar_t cg_drawDemoHUD;
vmCvar_t cg_xray;
vmCvar_t cg_freecam;
vmCvar_t cg_followKiller;
vmCvar_t cg_followPowerup;
vmCvar_t cg_specDuelHealthArmor;
vmCvar_t cg_specDuelHealthColor;
vmCvar_t cg_specFov;
vmCvar_t cg_specItemTimers;
vmCvar_t cg_specItemTimersSize;
vmCvar_t cg_specItemTimersX;
vmCvar_t cg_specItemTimersY;
vmCvar_t cg_specNames;
vmCvar_t cg_specTeamVitals;
vmCvar_t cg_specTeamVitalsHealthColor;
vmCvar_t cg_specTeamVitalsWidth;
vmCvar_t cg_specTeamVitalsY;

// [QL] HUD/UI cvars
vmCvar_t cg_chatHistoryLength;
vmCvar_t cg_complaintWarning;
vmCvar_t cg_drawCrosshairTeamHealthSize;
vmCvar_t cg_drawInputCmdsSize;
vmCvar_t cg_drawInputCmdsX;
vmCvar_t cg_drawInputCmdsY;
vmCvar_t cg_drawSprites;
vmCvar_t cg_drawSpriteSelf;
vmCvar_t cg_enemyCrosshairNames;
vmCvar_t cg_enemyCrosshairNamesOpacity;
vmCvar_t cg_lowAmmoWeaponBarWarning;
vmCvar_t cg_selfOnTeamOverlay;
vmCvar_t cg_teammateCrosshairNames;
vmCvar_t cg_teammateCrosshairNamesOpacity;
vmCvar_t cg_teammateNames;
vmCvar_t cg_useItemMessage;
vmCvar_t cg_useItemWarning;
vmCvar_t cg_voiceChatIndicator;

// [QL] POI cvars
vmCvar_t cg_flagPOIs;
vmCvar_t cg_poiMaxWidth;
vmCvar_t cg_poiMinWidth;
vmCvar_t cg_powerupPOIs;
vmCvar_t cg_teammatePOIs;
vmCvar_t cg_teammatePOIsMaxWidth;
vmCvar_t cg_teammatePOIsMinWidth;

// [QL] smoke/particle cvars
vmCvar_t cg_smoke_SG;
vmCvar_t cg_smokeRadius_dust;
vmCvar_t cg_smokeRadius_flight;
vmCvar_t cg_smokeRadius_GL;
vmCvar_t cg_smokeRadius_haste;
vmCvar_t cg_smokeRadius_NG;
vmCvar_t cg_smokeRadius_RL;

// [QL] visual/rendering cvars
vmCvar_t cg_drawDeadFriendTime;
vmCvar_t cg_drawHitFriendTime;
vmCvar_t cg_forceBlueTeamModel;
vmCvar_t cg_forceDrawCrosshair;
vmCvar_t cg_forceEnemySkin;
vmCvar_t cg_forceEnemyWeaponColor;
vmCvar_t cg_forceRedTeamModel;
vmCvar_t cg_forceTeamSkin;
vmCvar_t cg_forceTeamWeaponColor;
vmCvar_t cg_lightningImpactCap;
vmCvar_t cg_screenDamageAlpha;
vmCvar_t cg_screenDamageAlpha_Team;
vmCvar_t cg_scalePlayerModelsToBB;

// [QL] gameplay cvars
vmCvar_t cg_autoAction;
vmCvar_t cg_autoProjectileNudge;
vmCvar_t cg_predictLocalRailshots;

// [QL] gameplay cvars (continued)
vmCvar_t cg_latchedHookOffset;
vmCvar_t cg_overheadNamesWidth;
vmCvar_t cg_preferredStartingWeapons;
vmCvar_t cg_projectileNudge;
vmCvar_t cg_weaponPrimaryQueued;

// [QL] camera/view cvars
vmCvar_t cg_cameraSmartMode;
vmCvar_t cg_cameraThirdPersonSmartMode;
vmCvar_t cg_filter_angles;

// [QL] loadout disable cvars
vmCvar_t cg_disableLoadout_bfg;
vmCvar_t cg_disableLoadout_cg;
vmCvar_t cg_disableLoadout_gl;
vmCvar_t cg_disableLoadout_hmg;
vmCvar_t cg_disableLoadout_lg;
vmCvar_t cg_disableLoadout_mg;
vmCvar_t cg_disableLoadout_ng;
vmCvar_t cg_disableLoadout_pg;
vmCvar_t cg_disableLoadout_pm;
vmCvar_t cg_disableLoadout_rg;
vmCvar_t cg_disableLoadout_rl;
vmCvar_t cg_disableLoadout_sg;
vmCvar_t cg_disableLoadout_gh;
vmCvar_t cg_disableLoadout_gt;

// [QL] game info cvars (server-to-client state)
vmCvar_t cg_gameInfo1;
vmCvar_t cg_gameInfo2;
vmCvar_t cg_gameInfo3;
vmCvar_t cg_gameInfo4;
vmCvar_t cg_gameInfo5;
vmCvar_t cg_gameInfo6;

// [QL] team name cvars
vmCvar_t cg_blueTeamName;
vmCvar_t cg_redTeamName;
vmCvar_t cg_playerTeam;

// [QL] voice/chat cvars
vmCvar_t cg_playVoiceChats;
vmCvar_t cg_showVoiceText;

// [QL] misc cvars
vmCvar_t cg_debugFlags;
vmCvar_t cg_drawProfileImages;
vmCvar_t cg_ignoreMouseInput;
vmCvar_t cg_lastmsg;
vmCvar_t cg_stereoSeparation;
vmCvar_t cg_trackPlayer;

// [QL] UI state cvars (shared with UI module)
vmCvar_t ui_endMapVotingDisabled;
vmCvar_t ui_gameTypeVotingDisabled;
vmCvar_t ui_intermission;
vmCvar_t ui_mainmenu;
vmCvar_t ui_mapVotingDisabled;
vmCvar_t ui_voteactive;
vmCvar_t ui_votestring;
vmCvar_t ui_warmup;

// [QL] g_ cvars read by cgame
vmCvar_t g_skipTrainingEnable;
vmCvar_t g_training;

// [QL] internal/ROM cvars
vmCvar_t cg_loadout;
vmCvar_t cg_spectating;

// [QL] Last-standing announcer gate (DAT_10b716ec). Registered below so the announcer
// defaults on; distinct from the cg_announcerLastStandingVO voice-set cvar.
vmCvar_t cg_announcerLastStanding;

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
    // [QL] cgamex86.dll's entries (0x10076a00, 24 bytes) end with the range as two strings; the
    // ones whose flags carry CVAR_CG_RANGE are registered with it (0x10020bb0)
    char* minString;
    char* maxString;
} cvarTable_t;

static cvarTable_t cvarTable[] = {
    {&cg_ignore, "cg_ignore", "0", 0},  // used for debugging
    {&cg_autoswitch, "cg_autoswitch", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},  // [QL] default 0 (was Q3 "1")
    {&cg_drawGun, "cg_drawGun", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "3"},
    // [ioquakelive] a renderer cvar; the cgame reads it to know which lights to draw. The
    // 2 is ours: it leaves only the carriers' lights, and not your own. See CG_AddEffectLight.
    {&r_dynamicLight, "r_dynamicLight", "1", CVAR_ARCHIVE | CVAR_USERSAVE, "0", "2"},
    {&cg_zoomFov, "cg_zoomfov", "30", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "10", "130"},
    {&cg_fov, "cg_fov", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "10", "130"},
    {&cg_viewsize, "cg_viewsize", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "30", "100"},
    {&cg_shadows, "cg_shadows", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    // [wolfcam] The electrogib tuning set, with wolfcamql's defaults. None of
    // these names appear anywhere in cgamex86.dll - retail hardcodes every one
    // of them - so the whole set is an addition, and leaving it untouched
    // reproduces retail exactly.
    //
    // wolfcam spells the count `cg_gibs`, which is also what Quake 3 called
    // its on/off switch for body-part gibs. Keeping that name would have made
    // every inherited config silently wrong - a stored 0 turns the effect off
    // and a stored 1 throws a single sphere - so the count is spelled
    // cg_gibCount here. It is the only name that differs from wolfcam's.
    {&cg_gibCount, "cg_gibCount", "15", CVAR_ARCHIVE},
    {&cg_gibColor, "cg_gibColor", "", CVAR_ARCHIVE},
    {&cg_gibJump, "cg_gibJump", "0", CVAR_ARCHIVE},
    {&cg_gibVelocity, "cg_gibVelocity", "600", CVAR_ARCHIVE},
    {&cg_gibVelocityRandomness, "cg_gibVelocityRandomness", "250", CVAR_ARCHIVE},
    {&cg_gibDirScale, "cg_gibDirScale", "1.0", CVAR_ARCHIVE},
    {&cg_gibOriginOffset, "cg_gibOriginOffset", "0", CVAR_ARCHIVE},
    {&cg_gibOriginOffsetZ, "cg_gibOriginOffsetZ", "0", CVAR_ARCHIVE},
    {&cg_gibRandomness, "cg_gibRandomness", "250", CVAR_ARCHIVE},
    {&cg_gibRandomnessZ, "cg_gibRandomnessZ", "0", CVAR_ARCHIVE},
    {&cg_gibTime, "cg_gibTime", "1000", CVAR_ARCHIVE},
    {&cg_gibStepTime, "cg_gibStepTime", "50", CVAR_ARCHIVE},
    {&cg_gibBounceFactor, "cg_gibBounceFactor", "0.6", CVAR_ARCHIVE},
    {&cg_gibGravity, "cg_gibGravity", "800", CVAR_ARCHIVE},
    {&cg_gibSparksSize, "cg_gibSparksSize", "3.5", CVAR_ARCHIVE},
    {&cg_gibSparksColor, "cg_gibSparksColor", "", CVAR_ARCHIVE},
    {&cg_gibSparksHighlight, "cg_gibSparksHighlight", "", CVAR_ARCHIVE},
    {&cg_gibFloatingVelocity, "cg_gibFloatingVelocity", "125", CVAR_ARCHIVE},
    {&cg_gibFloatingRandomness, "cg_gibFloatingRandomness", "75", CVAR_ARCHIVE},
    {&cg_gibFloatingOriginOffset, "cg_gibFloatingOriginOffset", "30", CVAR_ARCHIVE},
    {&cg_gibFloatingOriginOffsetZ, "cg_gibFloatingOriginOffsetZ", "0", CVAR_ARCHIVE},
    {&cg_draw2D, "cg_draw2D", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawStatus, "cg_drawStatus", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawFPS, "cg_drawFPS", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawSnapshot, "cg_drawSnapshot", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_draw3dIcons, "cg_draw3dIcons", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawIcons, "cg_drawIcons", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawAmmoWarning, "cg_drawAmmoWarning", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},  // [QL] default 2 (was Q3 "1")
    {&cg_drawAttacker, "cg_drawAttacker", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawCrosshair, "cg_drawCrosshair", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "29"},
    {&cg_drawRewards, "cg_drawRewards", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawRewardsRowSize, "cg_drawRewardsRowSize", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "9"},
    {&cg_crosshairSize, "cg_crosshairSize", "32", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "320"},
    {&cg_crosshairHealth, "cg_crosshairHealth", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},  // [QL] default 0 (was Q3 "1")
    {&cg_crosshairX, "cg_crosshairX", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-320", "320"},
    {&cg_crosshairY, "cg_crosshairY", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-240", "240"},
    {&cg_brassTime, "cg_brassTime", "2500", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "10000"},
    {&cg_simpleItems, "cg_simpleItems", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_skipItems, "cg_skipItems", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_addMarks, "cg_marks", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_lagometer, "cg_lagometer", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},  // [QL] default 0 (was Q3 "1")
    {&cg_railTrailTime, "cg_railTrailTime", "400", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2000"},
    {&cg_gun_x, "cg_gunX", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-10", "10"},  // [QL] binary: CVAR_ARCHIVE (player viewmodel offset, not a cheat)
    {&cg_gun_y, "cg_gunY", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-10", "20"},
    {&cg_gun_z, "cg_gunZ", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-8", "0"},
    {&cg_centertime, "cg_centertime", "3", CVAR_CHEAT},
    {&cg_swingSpeed, "cg_swingSpeed", "0.3", CVAR_CHEAT},
    {&cg_animSpeed, "cg_animspeed", "1", CVAR_CHEAT},
    {&cg_debugAnim, "cg_debuganim", "0", CVAR_CHEAT},
    {&cg_debugEvents, "cg_debugevents", "0", CVAR_CHEAT},
    {&cg_errorDecay, "cg_errordecay", "100", CVAR_USERSAVE | CVAR_VM_CREATED, "0", "100"},
    {&cg_nopredict, "cg_nopredict", "0", CVAR_VM_CREATED | CVAR_REPLICATE, "0", "1"},
    {&cg_noPlayerAnims, "cg_noplayeranims", "0", CVAR_CHEAT},
    {&cg_showmiss, "cg_showmiss", "0", 0},
    {&cg_footsteps, "cg_footsteps", "1", CVAR_CHEAT},
    {&cg_tracerChance, "cg_tracerchance", "0.4", CVAR_CHEAT},
    {&cg_tracerWidth, "cg_tracerwidth", "1", CVAR_CHEAT},
    {&cg_tracerLength, "cg_tracerlength", "100", CVAR_CHEAT},
    {&cg_thirdPersonRange, "cg_thirdPersonRange", "80", CVAR_CHEAT},  // [QL] default 80 (was Q3 "40")
    {&cg_thirdPersonAngle, "cg_thirdPersonAngle", "0", CVAR_CHEAT},
    {&cg_thirdPerson, "cg_thirdPerson", "0", CVAR_CHEAT},  // [QL] cheat-protected (binary: CVAR_CHEAT)
    {&cg_teamChatTime, "cg_teamChatTime", "3000", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "5000"},
    {&cg_teamChatHeight, "cg_teamChatHeight", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "8"},
    {&cg_predictItems, "cg_predictItems", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_USERINFO | CVAR_ARCHIVE},  // [QL] binary: CVAR_ARCHIVE | CVAR_USERINFO
    {&cg_deferPlayers, "cg_deferPlayers", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 1 (was Q3 "0")
    {&cg_drawTeamOverlay, "cg_drawTeamOverlay", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},  // [QL] default 1 (was Q3 "0")
    {&cg_stats, "cg_stats", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_teamChatsOnly, "cg_teamChatsOnly", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    // the following variables are created in other parts of the system,
    // but we also reference them here
    {&cg_paused, "cl_paused", "0", CVAR_ROM},
    {&cg_blood, "com_blood", "1", CVAR_ARCHIVE},

    {&cg_currentSelectedPlayer, "cg_currentSelectedPlayer", "0", 0},
    {&cg_currentSelectedPlayerName, "cg_currentSelectedPlayerName", "", 0},
    {&cg_enableDust, "g_enableDust", "0", CVAR_SERVERINFO},
    {&cg_recordSPDemo, "ui_recordSPDemo", "0", 0},
    {&cg_recordSPDemoName, "ui_recordSPDemoName", "", 0},
    {&ui_singlePlayerActive, "ui_singlePlayerActive", "0", CVAR_USERINFO},  // [QL] binary: CVAR_USERINFO
    {&cg_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", 0},
    {&cg_hudFiles, "cg_hudFiles", "ui/hud.txt", CVAR_USERSAVE | CVAR_REPLICATE},

    {&cg_cameraOrbit, "cg_cameraOrbit", "0", CVAR_CHEAT},
    {&cg_cameraOrbitDelay, "cg_cameraOrbitDelay", "50", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_timescaleFadeEnd, "cg_timescaleFadeEnd", "1", 0},
    {&cg_timescaleFadeSpeed, "cg_timescaleFadeSpeed", "0", 0},
    {&cg_timescale, "timescale", "1", CVAR_VM_CREATED, "0.1", "10"},
    {&cg_scorePlum, "cg_scorePlums", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},  // [QL] default 0 (was Q3 "1")
    {&cg_smoothClients, "cg_smoothClients", "0", CVAR_USERINFO | CVAR_ARCHIVE},
    {&cg_cameraMode, "com_cameraMode", "0", CVAR_CHEAT},

    {&cg_smallFont, "ui_smallFont", "0.25", CVAR_ROM},
    {&cg_bigFont, "ui_bigFont", "0.4", CVAR_ROM},
    {&cg_noTaunt, "cg_noTaunt", "0", CVAR_ARCHIVE},

    {&cg_noProjectileTrail, "cg_noProjectileTrail", "0", CVAR_ARCHIVE},
    {&cg_oldRocket, "cg_oldRocket", "1", CVAR_ARCHIVE},
    {&cg_oldPlasma, "cg_oldPlasma", "1", CVAR_ARCHIVE},
    {&cg_trueLightning, "cg_trueLightning", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},  // [QL] default 1 (was Q3 "0.0")
    {&cg_lightningStyle, "cg_lightningStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "5"},
    {&cg_screenDamage, "cg_screenDamage", "0x700000C8", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_kickScale, "cg_kickScale", "0.25", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},

    // [QL] additional cvars from cgamex86.dll binary (build 1069)
    {&cg_armorTiered, "cg_armorTiered", "0", CVAR_ROM},
    {&cg_allowTaunt, "cg_allowTaunt", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_announcer, "cg_announcer", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "3"},
    {&cg_autoHop, "cg_autoHop", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_USERINFO | CVAR_ARCHIVE},  // [QL] binary: CVAR_ARCHIVE | CVAR_USERINFO (server reads via userinfo)
    {&cg_predictLocalRailshots, "cg_predictLocalRailshots", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] predicted railgun autofire trail
    {&cg_bob, "cg_bob", "0.25", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.0", "1.0"},
    {&cg_bubbleTrail, "cg_bubbleTrail", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_buzzerSound, "cg_buzzerSound", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_crosshairBrightness, "cg_crosshairBrightness", "1.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.0", "1.0"},
    {&cg_crosshairColor, "cg_crosshairColor", "25", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "26"},
    {&cg_crosshairHitColor, "cg_crosshairHitColor", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "26"},
    {&cg_crosshairHitStyle, "cg_crosshairHitStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "8"},
    {&cg_crosshairHitTime, "cg_crosshairHitTime", "200.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.0", "1000.0"},
    {&cg_crosshairPulse, "cg_crosshairPulse", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_damagePlum, "cg_damagePlum", "g mg sg gl rl lg rg pg bfg gh cg ng pl hmg", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_damagePlumColorStyle, "cg_damagePlumColorStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "3"},
    {&cg_deadBodyColor, "cg_deadBodyColor", "0x101010FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_deadBodyDarken, "cg_deadBodyDarken", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawCrosshairTeamHealth, "cg_drawCrosshairTeamHealth", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_drawFragMessages, "cg_drawFragMessages", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawFullWeaponBar, "cg_drawFullWeaponBar", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawItemPickups, "cg_drawItemPickups", "3", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "7"},
    {&cg_drawCheckpointRemaining, "cg_drawCheckpointRemaining", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawSpecMessages, "cg_drawSpecMessages", "1", CVAR_USERSAVE | CVAR_VM_CREATED, "0", "1"},
    {&cg_drawPregameMessages, "cg_drawPregameMessages", "1", CVAR_USERSAVE | CVAR_VM_CREATED, "0", "1"},
    {&cg_drawTieredArmorAvailability, "cg_drawTieredArmorAvailability", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_enemyHeadColor, "cg_enemyHeadColor", "0x2a8000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyLowerColor, "cg_enemyLowerColor", "0x2a8000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyUpperColor, "cg_enemyUpperColor", "0x2a8000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_flagStyle, "cg_flagStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "2"},
    {&cg_forceEnemyModel, "cg_forceEnemyModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceTeamModel, "cg_forceTeamModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_hitBeep, "cg_hitBeep", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "3"},
    {&cg_impactSparks, "cg_impactSparks", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_impactSparksLifetime, "cg_impactSparksLifetime", "250", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1000"},
    {&cg_impactSparksSize, "cg_impactSparksSize", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "2", "16"},
    {&cg_impactSparksVelocity, "cg_impactSparksVelocity", "128", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-128", "128"},
    {&cg_itemFx, "cg_itemFx", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "7"},
    {&cg_itemTimers, "cg_itemTimers", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_killBeep, "cg_killBeep", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "8"},
    {&cg_levelTimerDirection, "cg_levelTimerDirection", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_lightningImpact, "cg_lightningImpact", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_railReloadTime, "cg_railReloadTime", "1500", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lowAmmoWarningPercentile, "cg_lowAmmoWarningPercentile", "0.20", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.01", "1.00"},
    {&cg_lowAmmoWarningSound, "cg_lowAmmoWarningSound", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_muzzleFlash, "cg_muzzleFlash", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_obituaryRowSize, "cg_obituaryRowSize", "5", CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "5"},
    {&cg_playerLean, "cg_playerLean", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_ARCHIVE, "0", "1"},
    {&cg_plasmaStyle, "cg_plasmaStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "2"},
    {&cg_railStyle, "cg_railStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "2"},
    // [CPMA] 1.53 animations with CPMA's defaults, except ch_animateScroll, on by default here
    {&ch_animateRewardIcons, "ch_animateRewardIcons", "1", CVAR_ARCHIVE},
    {&cg_animateChatBalloon, "cg_animateChatBalloon", "1", CVAR_ARCHIVE},
    {&ch_animateScroll, "ch_animateScroll", "1", CVAR_ARCHIVE},
    {&cg_zoomAnimationTime, "cg_zoomAnimationTime", "150", CVAR_ARCHIVE},
    // [CPMA] weapons drawn as flat silhouettes: 1 your own gun, 2 teammates', 4 enemies', 8 the ones
    // lying on the map, 16 anybody's while you spectate with the free camera
    {&cg_drawBrightWeapons, "cg_drawBrightWeapons", "0", CVAR_ARCHIVE},
    {&ch_chatLines, "ch_chatLines", "3", CVAR_ARCHIVE},
    // [CPMA] the LocalTime and RecordingDemo elements of CPMA 1.53, in Quake Live's own type
    {&ch_drawLocalTime, "ch_drawLocalTime", "0", CVAR_ARCHIVE},
    {&ch_drawTargetStatus, "ch_drawTargetStatus", "1", CVAR_ARCHIVE},
    {&ch_drawKeys, "ch_drawKeys", "0", CVAR_ARCHIVE},
    {&ch_autoStats, "ch_autoStats", "0", CVAR_ARCHIVE},
    {&cg_demoKeys, "cg_demoKeys", "0", CVAR_ARCHIVE},
    {&ch_followKiller, "ch_followKiller", "0", CVAR_ARCHIVE},
    {&ch_obituaryBand, "ch_obituaryBand", "1", CVAR_ARCHIVE},
    {&ch_obituaryY, "ch_obituaryY", "14", CVAR_ARCHIVE},
    {&ch_obituaryScale, "ch_obituaryScale", "0.85", CVAR_ARCHIVE},
    // [QL] the engine owns this one; the cgame only reads it to leave the map's sounds out
    {&cg_ambient, "s_ambient", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE | CVAR_LATCH},
    {&ch_predictHitSounds, "ch_predictHitSounds", "0", CVAR_ARCHIVE},
    {&ch_gameEvents, "ch_gameEvents", "1", CVAR_ARCHIVE},
    {&ch_recordMessage, "ch_recordMessage", "1", CVAR_ARCHIVE},
    {&cg_demoRecording, "cl_demorecording", "0", CVAR_ROM},
    {&cg_demoName, "cl_demoname", "", CVAR_ROM},
    {&cg_rocketStyle, "cg_rocketStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "1", "2"},
    {&cg_raceBeep, "cg_raceBeep", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "8"},
    {&cg_screenDamage_Self, "cg_screenDamage_Self", "0x00000000", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamage_Team, "cg_screenDamage_Team", "0x700000C8", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_simpleItemsBob, "cg_simpleItemsBob", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_simpleItemsHeightOffset, "cg_simpleItemsHeightOffset", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "18"},
    {&cg_simpleItemsRadius, "cg_simpleItemsRadius", "15", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "8", "24"},
    {&cg_speedometer, "cg_speedometer", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "4"},
    {&cg_switchOnEmpty, "cg_switchOnEmpty", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_switchToEmpty, "cg_switchToEmpty", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_teamHeadColor, "cg_teamHeadColor", "0x808080FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamLowerColor, "cg_teamLowerColor", "0x808080FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamUpperColor, "cg_teamUpperColor", "0x808080FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_thirdPersonPitch, "cg_thirdPersonPitch", "-1", CVAR_CHEAT},
    {&cg_trueShotgun, "cg_trueShotgun", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_vignette, "cg_vignette", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_waterWarp, "cg_waterWarp", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_weaponBar, "cg_weaponBar", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "4"},
    {&cg_zoomOutOnDeath, "cg_zoomOutOnDeath", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_zoomScaling, "cg_zoomScaling", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_zoomSensitivity, "cg_zoomSensitivity", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "10"},
    {&cg_zoomToggle, "cg_zoomToggle", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawInputCmds, "cg_drawInputCmds", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_drawTeamOverlayOpacity, "cg_drawTeamOverlayOpacity", "0.75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawTeamOverlaySize, "cg_drawTeamOverlaySize", "0.16", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.12", "0.22"},
    {&cg_drawTeamOverlayX, "cg_drawTeamOverlayX", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-640", "640"},
    {&cg_drawTeamOverlayY, "cg_drawTeamOverlayY", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "-480", "480"},
    {&cg_impactMarkTime, "cg_impactMarkTime", "10000", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "10000"},
    {&cg_chatbeep, "cg_chatbeep", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_teamChatBeep, "cg_teamChatBeep", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_gametype, "cg_gametype", "0", CVAR_ROM},
    {&cg_svFps, "sv_fps", "40", CVAR_ROM},
    // [QL] the cgame registers com_maxfps again with a range of its own, so the ceiling is
    // locked in two places: raising it in the engine alone drops it back to 250 on map load
    {&cg_comMaxfps, "com_maxfps", "125", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1000"},

    // [QL] announcer cvars
    {&cg_announcerLastStandingVO, "cg_announcerLastStandingVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_announcerLeadsVO, "cg_announcerLeadsVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_announcerRewardsVO, "cg_announcerRewardsVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_announcerTiesVO, "cg_announcerTiesVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},

    // [QL] sound cvars
    {&s_ambient, "s_ambient", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_LATCH | CVAR_ARCHIVE},
    {&s_announcerVolume, "s_announcerVolume", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&s_killBeepVolume, "s_killBeepVolume", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "4"},

    // [QL] weapon config cvars
    {&cg_weaponConfig, "cg_weaponConfig", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_g, "cg_weaponConfig_g", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_mg, "cg_weaponConfig_mg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_sg, "cg_weaponConfig_sg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_gl, "cg_weaponConfig_gl", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_rl, "cg_weaponConfig_rl", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_lg, "cg_weaponConfig_lg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_rg, "cg_weaponConfig_rg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_pg, "cg_weaponConfig_pg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_bfg, "cg_weaponConfig_bfg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_gh, "cg_weaponConfig_gh", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_ng, "cg_weaponConfig_ng", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_pl, "cg_weaponConfig_pl", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_cg, "cg_weaponConfig_cg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_hmg, "cg_weaponConfig_hmg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponColor_grenade, "cg_weaponColor_grenade", "0x007000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponPrimary, "cg_weaponPrimary", "", CVAR_ROM},

    // [QL] spectator/demo cvars
    {&cg_drawDemoHUD, "cg_drawDemoHUD", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    // [ioquakelive] for demos and spectators only: players through walls, and a free camera
    {&cg_xray, "cg_xray", "0", CVAR_ARCHIVE, "0", "1"},
    {&cg_freecam, "cg_freecam", "0", 0, "0", "1"},
    {&cg_followKiller, "cg_followKiller", "0", CVAR_VM_CREATED, "0", "1"},
    {&cg_followPowerup, "cg_followPowerup", "0", CVAR_VM_CREATED, "0", "2"},
    {&cg_specDuelHealthArmor, "cg_specDuelHealthArmor", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_specDuelHealthColor, "cg_specDuelHealthColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_specFov, "cg_specFov", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_specItemTimers, "cg_specItemTimers", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "15"},
    {&cg_specItemTimersSize, "cg_specItemTimersSize", "0.24", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.12", "0.35"},
    {&cg_specItemTimersX, "cg_specItemTimersX", "10", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "640"},
    {&cg_specItemTimersY, "cg_specItemTimersY", "200", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "480"},
    {&cg_specNames, "cg_specNames", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_specTeamVitals, "cg_specTeamVitals", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_specTeamVitalsHealthColor, "cg_specTeamVitalsHealthColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_specTeamVitalsWidth, "cg_specTeamVitalsWidth", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "60", "120"},
    {&cg_specTeamVitalsY, "cg_specTeamVitalsY", "85", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "480"},

    // [QL] HUD/UI cvars
    {&cg_chatHistoryLength, "cg_chatHistoryLength", "6", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "24"},
    {&cg_complaintWarning, "cg_complaintWarning", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawCrosshairTeamHealthSize, "cg_drawCrosshairTeamHealthSize", "0.12", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.10", "0.26"},
    {&cg_drawInputCmdsSize, "cg_drawInputCmdsSize", "24", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "16", "200"},
    {&cg_drawInputCmdsX, "cg_drawInputCmdsX", "320", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "640"},
    {&cg_drawInputCmdsY, "cg_drawInputCmdsY", "240", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "480"},
    {&cg_drawSprites, "cg_drawSprites", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawSpriteSelf, "cg_drawSpriteSelf", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_enemyCrosshairNames, "cg_enemyCrosshairNames", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_enemyCrosshairNamesOpacity, "cg_enemyCrosshairNamesOpacity", "0.75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_lowAmmoWeaponBarWarning, "cg_lowAmmoWeaponBarWarning", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_selfOnTeamOverlay, "cg_selfOnTeamOverlay", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_teammateCrosshairNames, "cg_teammateCrosshairNames", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_teammateCrosshairNamesOpacity, "cg_teammateCrosshairNamesOpacity", "0.75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_teammateNames, "cg_teammateNames", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_useItemMessage, "cg_useItemMessage", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_useItemWarning, "cg_useItemWarning", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_voiceChatIndicator, "cg_voiceChatIndicator", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},

    // [QL] POI cvars
    {&cg_flagPOIs, "cg_flagPOIs", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_poiMaxWidth, "cg_poiMaxWidth", "32.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "16.0", "32.0"},
    {&cg_poiMinWidth, "cg_poiMinWidth", "16.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "2.0", "16.0"},
    {&cg_powerupPOIs, "cg_powerupPOIs", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "2"},
    {&cg_teammatePOIs, "cg_teammatePOIs", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_teammatePOIsMaxWidth, "cg_teammatePOIsMaxWidth", "24.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "16.0", "32.0"},
    {&cg_teammatePOIsMinWidth, "cg_teammatePOIsMinWidth", "4.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "2.0", "16.0"},

    // [QL] smoke/particle cvars
    {&cg_smoke_SG, "cg_smoke_SG", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_smokeRadius_dust, "cg_smokeRadius_dust", "24", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "32"},
    {&cg_smokeRadius_flight, "cg_smokeRadius_flight", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "16"},
    {&cg_smokeRadius_GL, "cg_smokeRadius_GL", "64", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "64"},
    {&cg_smokeRadius_haste, "cg_smokeRadius_haste", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "16"},
    {&cg_smokeRadius_NG, "cg_smokeRadius_NG", "16", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "16"},
    {&cg_smokeRadius_RL, "cg_smokeRadius_RL", "32", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "32"},

    // [QL] visual/rendering cvars
    {&cg_blood, "cg_blood", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_drawDeadFriendTime, "cg_drawDeadFriendTime", "3000.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0.0", "5000.0"},
    {&cg_drawHitFriendTime, "cg_drawHitFriendTime", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "5000"},
    {&cg_forceBlueTeamModel, "cg_forceBlueTeamModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceDrawCrosshair, "cg_forceDrawCrosshair", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_forceEnemySkin, "cg_forceEnemySkin", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceEnemyWeaponColor, "cg_forceEnemyWeaponColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_forceRedTeamModel, "cg_forceRedTeamModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceTeamSkin, "cg_forceTeamSkin", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceTeamWeaponColor, "cg_forceTeamWeaponColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_lightningImpactCap, "cg_lightningImpactCap", "192", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "768"},
    {&cg_screenDamageAlpha, "cg_screenDamageAlpha", "200", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamageAlpha_Team, "cg_screenDamageAlpha_Team", "200", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_scalePlayerModelsToBB, "cg_scalePlayerModelsToBB", "1", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT

    // [QL] gameplay cvars
    {&cg_autoAction, "cg_autoAction", "3", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_USERINFO | CVAR_ARCHIVE, "0", "3"},  // [QL] binary: CVAR_ARCHIVE | CVAR_USERINFO
    {&cg_autoProjectileNudge, "cg_autoProjectileNudge", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_latchedHookOffset, "cg_latchedHookOffset", "1.0", CVAR_ROM},
    {&cg_overheadNamesWidth, "cg_overheadNamesWidth", "75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "50", "100"},
    {&cg_preferredStartingWeapons, "cg_preferredStartingWeapons", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_predictLocalRailshots, "cg_predictLocalRailshots", "1", 0},
    {&cg_projectileNudge, "cg_projectileNudge", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT

    // [QL] camera/view cvars
    {&cg_cameraSmartMode, "cg_cameraSmartMode", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_cameraThirdPersonSmartMode, "cg_cameraThirdPersonSmartMode", "1", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_filter_angles, "cg_filter_angles", "0", 0},
    {&cg_stereoSeparation, "cg_stereoSeparation", "0.4", CVAR_ARCHIVE},

    // [QL] loadout disable cvars - CVAR_ROM (set by server via CS_DISABLE_LOADOUT, must not persist)
    {&cg_disableLoadout_bfg, "cg_disableLoadout_bfg", "0", CVAR_ROM},
    {&cg_disableLoadout_cg, "cg_disableLoadout_cg", "0", CVAR_ROM},
    {&cg_disableLoadout_gh, "cg_disableLoadout_gh", "0", CVAR_ROM},
    {&cg_disableLoadout_gl, "cg_disableLoadout_gl", "0", CVAR_ROM},
    {&cg_disableLoadout_gt, "cg_disableLoadout_g",  "0", CVAR_ROM},  // [QL] cvar name matches cgamex86.dll string table
    {&cg_disableLoadout_hmg, "cg_disableLoadout_hmg", "0", CVAR_ROM},
    {&cg_disableLoadout_lg, "cg_disableLoadout_lg", "0", CVAR_ROM},
    {&cg_disableLoadout_mg, "cg_disableLoadout_mg", "0", CVAR_ROM},
    {&cg_disableLoadout_ng, "cg_disableLoadout_ng", "0", CVAR_ROM},
    {&cg_disableLoadout_pg, "cg_disableLoadout_pg", "0", CVAR_ROM},
    {&cg_disableLoadout_pm, "cg_disableLoadout_pl", "0", CVAR_ROM},  // [QL] cvar name matches cgamex86.dll string table
    {&cg_disableLoadout_rg, "cg_disableLoadout_rg", "0", CVAR_ROM},
    {&cg_disableLoadout_rl, "cg_disableLoadout_rl", "0", CVAR_ROM},
    {&cg_disableLoadout_sg, "cg_disableLoadout_sg", "0", CVAR_ROM},

    // [QL] game info cvars (server-to-client state)
    {&cg_gameInfo1, "cg_gameInfo1", "0", CVAR_ROM},
    {&cg_gameInfo2, "cg_gameInfo2", "0", CVAR_ROM},
    {&cg_gameInfo3, "cg_gameInfo3", "0", CVAR_ROM},
    {&cg_gameInfo4, "cg_gameInfo4", "0", CVAR_ROM},
    {&cg_gameInfo5, "cg_gameInfo5", "0", CVAR_ROM},
    {&cg_gameInfo6, "cg_gameInfo6", "0", CVAR_ROM},

    // [QL] team name cvars
    {&cg_blueTeamName, "cg_blueTeamName", "", 0},
    {&cg_redTeamName, "cg_redTeamName", "", 0},
    {&cg_playerTeam, "cg_playerTeam", "", CVAR_ROM},

    // [QL] voice/chat cvars
    {&cg_playVoiceChats, "cg_playVoiceChats", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_showVoiceText, "cg_showVoiceText", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT

    // [QL] misc cvars
    {&cg_debugFlags, "cg_debugFlags", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_drawProfileImages, "cg_drawProfileImages", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE, "0", "1"},
    {&cg_ignoreMouseInput, "cg_ignoreMouseInput", "0", CVAR_ROM},
    {&cg_lastmsg, "cg_lastmsg", "", 0},
    {&cg_trackPlayer, "cg_trackPlayer", "-1", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_weaponPrimaryQueued, "cg_weaponPrimaryQueued", "", CVAR_TEMP},  // [QL] menu writes via exec; binary uses CVAR_TEMP

    // [QL] UI state cvars (shared with UI module)
    {&ui_endMapVotingDisabled, "ui_endMapVotingDisabled", "0", CVAR_ROM},
    {&ui_gameTypeVotingDisabled, "ui_gameTypeVotingDisabled", "0", CVAR_ROM},
    {&ui_intermission, "ui_intermission", "0", CVAR_ROM},
    {&ui_mainmenu, "ui_mainmenu", "1", CVAR_ROM},
    {&ui_mapVotingDisabled, "ui_mapVotingDisabled", "0", CVAR_ROM},
    {&ui_voteactive, "ui_voteactive", "0", CVAR_ROM},
    {&ui_votestring, "ui_votestring", "", CVAR_CHEAT},
    {&ui_warmup, "ui_warmup", "0", CVAR_ROM},

    // [QL] g_ cvars read by cgame
    {&g_skipTrainingEnable, "g_skipTrainingEnable", "0", 0},
    {&g_training, "g_training", "0", 0},

    // [QL] internal/ROM cvars
    {&cg_loadout, "cg_loadout", "0", CVAR_ROM},
    {&cg_spectating, "cg_spectating", "0", CVAR_ROM},

    // [QL] last-standing announcer gate (defaults on)
    {&cg_announcerLastStanding, "cg_announcerLastStanding", "1", CVAR_ARCHIVE},
};

static int cvarTableSize = ARRAY_LEN(cvarTable);

/*
=================
CG_RegisterCvars
=================
*/
void CG_RegisterCvars(void) {
    int i;
    cvarTable_t* cv;
    char var[MAX_TOKEN_CHARS];

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        // [QL] 0x10020bb0: an entry flagged CVAR_CG_RANGE goes through the range import (+0x14),
        // which keeps the value between the entry's two strings
        if (cv->cvarFlags & CVAR_CG_RANGE) {
            trap_Cvar_RegisterRange(cv->vmCvar, cv->cvarName, cv->defaultString,
                                    cv->minString, cv->maxString, cv->cvarFlags);
        } else {
            trap_Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
        }
    }

    // [QL] ROM build-version cvar reported by cgamex86.dll build 1069.
    trap_Cvar_Register(NULL, "cg_version", "1069 win-x86 May 25 2016 15:18:10", CVAR_ROM);

    // [QL] clear any stale vote-active flag on (re)load.
    trap_Cvar_Set("ui_voteactive", "0");

    // see if we are also running the server on this machine
    trap_Cvar_VariableStringBuffer("sv_running", var, sizeof(var));
    cgs.localServer = atoi(var);

    trap_Cvar_Register(NULL, "model", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE);
    trap_Cvar_Register(NULL, "headmodel", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE);
}

/*
=================
CG_UpdateCvars
=================
*/
void CG_UpdateCvars(void) {
    int i;
    cvarTable_t* cv;

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        trap_Cvar_Update(cv->vmCvar);
    }

    // check for modications here
    // [QL] no "teamoverlay" userinfo: the name is in none of retail's modules, and its server
    // sends the team list (tinfo) whatever the client draws

    // [QL] once loading is over, a change to any of the six forcing cvars reloads the local
    // client, whose CG_NewClientInfo then reloads everybody else (0x10020cc7)
    if (!cg.loading) {
        if (cg_forceModelModCounts[0] != cg_forceTeamModel.modificationCount ||
            cg_forceModelModCounts[1] != cg_forceTeamSkin.modificationCount ||
            cg_forceModelModCounts[2] != cg_forceEnemyModel.modificationCount ||
            cg_forceModelModCounts[3] != cg_forceEnemySkin.modificationCount ||
            cg_forceModelModCounts[4] != cg_forceRedTeamModel.modificationCount ||
            cg_forceModelModCounts[5] != cg_forceBlueTeamModel.modificationCount) {
            cg_forceModelModCounts[0] = cg_forceTeamModel.modificationCount;
            cg_forceModelModCounts[1] = cg_forceTeamSkin.modificationCount;
            cg_forceModelModCounts[2] = cg_forceEnemyModel.modificationCount;
            cg_forceModelModCounts[3] = cg_forceEnemySkin.modificationCount;
            cg_forceModelModCounts[4] = cg_forceRedTeamModel.modificationCount;
            cg_forceModelModCounts[5] = cg_forceBlueTeamModel.modificationCount;
            CG_NewClientInfo(cg.clientNum);
        }
    }
}

int CG_CrosshairPlayer(void) {
    if (cg.time > (cg.crosshairClientTime + 1000)) {
        return -1;
    }
    return cg.crosshairClientNum;
}

int CG_LastAttacker(void) {
    if (!cg.attackerTime) {
        return -1;
    }
    return cg.snap->ps.persistant[PERS_ATTACKER];
}

// ============================================================================
// [QL] vmMain export slots 10-14 (cgamex86.dll table at 0x100769a8).
// ============================================================================

// Address: 0x10029ff0
// Re-adds the last outgoing chat line to the chat display and arms a 3s recall
// timer. Matches: ioquakelive (QL-only export).
void CG_LastChatCommand(void) {
    char msg[256];

    cg.lastChatTime = cg.time + 3000;
    trap_Cvar_VariableStringBuffer("cg_lastmsg", msg, sizeof(msg));
    CG_AddChat(msg, 0, 0);
}

// Address: 0x1002a060
// Same as CG_LastChatCommand but arms the second recall timer.
void CG_LastChatCommand2(void) {
    char msg[256];

    cg.lastChatTime2 = cg.time + 3000;
    trap_Cvar_VariableStringBuffer("cg_lastmsg", msg, sizeof(msg));
    CG_AddChat(msg, 0, 0);
}

// Address: 0x10007cd0
// Sets the cgame key-catcher flag (global 0x10a9c9b4) if not already set.
void CG_SetKeyCatcher(void) {
    if (cg.keyCatcherActive == 0) {
        cg.keyCatcherActive = 1;
    }
}

// Address: 0x10007cf0
// Clears the cgame key-catcher flag if set.
void CG_ClearKeyCatcher(void) {
    if (cg.keyCatcherActive != 0) {
        cg.keyCatcherActive = 0;
    }
}

// Address: 0x1004e4d0
// Returns the client frame counter (dword at 0x10a9c1f4, cg.clientFrame),
// bumped once per rendered scene in CG_DrawActiveFrame.
int CG_GetActiveFrame(void) {
    return cg.clientFrame;
}

// ============================================================================
// [QL] match / race / map-name helpers
// ============================================================================

// Address: 0x10001320
// Formats a millisecond race time as "[-]M:0S.mmm".
const char* CG_FormatRaceTime(int msec) {
    int absMsec = (msec < 0) ? -msec : msec;
    float seconds = (float)absMsec / 1000.0f;
    int minutes = (int)seconds / 60;
    float secs = seconds - (float)minutes * 60.0f;
    const char* pad = (secs < 10.0f) ? "0" : "";
    const char* sign = (msec < 0) ? "-" : "";

    return va("%s%d:%s%.03f", sign, minutes, pad, (double)secs);
}

// Address: 0x100253f0
// Reads ui/country.txt and pre-caches every country flag shader.
void CG_CacheCountryFlags(void) {
    fileHandle_t f;
    int len;
    char buf[4096];
    char code[32 + 1024];  // code[0..31] name + code[32..] path scratch
    char* p;
    int i;

    code[0] = '\0';

    trap_R_RegisterShaderNoMip("ui/assets/flags/none.tga");

    len = trap_FS_FOpenFile("ui/country.txt", &f, FS_READ);
    if (len <= 0) {
        Com_Printf("ERROR: CG_CacheCountryFlags: %s too small\n", "ui/country.txt");
        return;
    }
    if (len >= (int)sizeof(buf)) {
        Com_Printf("ERROR: CG_CacheCountryFlags: %s too large. Size is %d, limit is %d\n",
                   "ui/country.txt", len, (int)sizeof(buf));
        return;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);

    p = code;
    for (i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\r' || c == '\t' || c == ' ') {
            continue;
        }
        if (c == '\n') {
            *p = '\0';
            if (code[0] != '\0') {
                Com_sprintf(code + 32, 1024, "ui/assets/flags/%s.tga", code);
                trap_R_RegisterShaderNoMip(code + 32);
            }
            code[0] = '\0';
            p = code;
        } else {
            *p++ = c;
        }
    }
}

/*
=================
CG_CountryFlagShader

[QL] 0x1002df70: the flag of a country code, ui/assets/flags/<code>, or the blank one when
there is none. With no country at all (a bot, or nobody set one) retail looks up an empty
name first, and the renderer warns about it; that lookup is skipped.
=================
*/
qhandle_t CG_CountryFlagShader(const char* country) {
    qhandle_t h = 0;

    if (country && country[0]) {
        h = trap_R_RegisterShaderNoMip(va("ui/assets/flags/%s.tga", country));
    }
    if (!h) {
        h = trap_R_RegisterShaderNoMip("ui/assets/flags/none.tga");
    }
    return h;
}

// Address: 0x1002a4a0
// Resets race-mode tracking state; on a full reset also clears the personal
// best. Only acts in GT_RACE.
// NOTE: the QL race globals (0x10abaab0..0x10abaad4) map onto ioquakelive's
// cg.race sub-struct + cg.raceActive; the exact field pairing is a Phase-3
// reconciliation item (see cg_local.h race notes).
void CG_RaceInit(int fullReset) {
    if (cgs.gametype != GT_RACE) {
        return;
    }

    cg.raceActive = 0;                    // 0x10abaab0
    cg.race.startTime = 0;                // 0x10abaab4
    cg.race.finishTime = 0;               // 0x10abaac0
    cg.race.bestSplit = -1;               // 0x10abaac4
    cg.race.checkpointDiff = -1;          // 0x10abaac8
    cg.race.hasDiff = qfalse;             // 0x10abaacc
    cg.race.checkpointCount = 0;          // 0x10abaad0
    cg.race.currentCheckpointEnt = 0;     // 0x10abaad4

    if (fullReset) {
        cg.race.bestTime = 0;             // 0x10abaab8
        cg.race.totalCheckpoints = 0;     // 0x10abaabc
    }

    trap_SendConsoleCommand("race_reset");
}

// Address: 0x1002a500
// Whether the local player holds first place. Never as a spectator. In FFA,
// duel, race and red rover: rank 0 and not tied. In team games: the player's
// team is ahead, red also on a tie (retail compares red with >=, blue with >).
// Retail also copies the player's name into a local buffer it never reads.
qboolean CG_LocalPlayerLeads(void) {
    int rank;
    int team;

    if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return qfalse;
    }

    if (cgs.gametype >= GT_FFA && (cgs.gametype <= GT_RACE || cgs.gametype == GT_RR)) {
        rank = cg.snap->ps.persistant[PERS_RANK];
        if (rank & RANK_TIED_FLAG) {
            return qfalse;
        }
        return rank == 0;
    }

    team = cgs.clientinfo[cg.snap->ps.clientNum].team;
    if (team == TEAM_RED) {
        return cgs.scores1 >= cgs.scores2;
    }
    if (team == TEAM_BLUE) {
        return cgs.scores2 > cgs.scores1;
    }
    return qfalse;
}

// Address: 0x1002a5f0
// Returns the number of overtime periods elapsed past the time limit.
int CG_GetOvertimeCount(void) {
    int timeUsed;
    int elapsed;
    int overtimeCount;
    int i;

    // [QL] 0x10a403e0: freeze/timeout end time, else current time.
    timeUsed = cgs.freezeEnd;
    if (timeUsed == 0) {
        timeUsed = cg.time;
    }

    // [QL] seconds past the time limit: retail divides by -1000 (the magic -0x10624dd3), and
    // without the minus this counted overtimes while there was still time left
    elapsed = (timeUsed - cgs.levelStartTime - cgs.timelimit * 60000) / 1000;

    if (elapsed <= 0) {
        return 0;
    }

    overtimeCount = 1;
    for (i = 1; i <= elapsed; i++) {
        // [QL] 0x10a3ffa8 = overtime period length (g_overtime == cgs.timelimit_overtime).
        if (cgs.timelimit_overtime != 0 && (i % cgs.timelimit_overtime) == 0) {
            overtimeCount++;
        }
    }
    return overtimeCount;
}

// Address: 0x10056f90
// Translates an internal map filename to its display name via a fixed table
// (49 entries, extracted from cgamex86.dll 0x10075df0). Falls back to the
// input name when no prefix match is found.
typedef struct {
    const char* filename;
    const char* displayName;
} mapTranslation_t;

static const mapTranslation_t mapTranslations[] = {
    { "qzca1", "asylum" },
    { "qzca2", "trinity" },
    { "qzca3", "quarantine" },
    { "qzctf1", "duelingkeeps" },
    { "qzctf2", "troubledwaters" },
    { "qzctf3", "stronghold" },
    { "qzctf4", "spacectf" },
    { "qzctf5", "falloutbunker" },
    { "qzctf6", "beyondreality" },
    { "qzctf7", "ironworks" },
    { "qzctf8", "siberia" },
    { "qzctf9", "bloodlust" },
    { "qzctf10", "courtyard" },
    { "qzdm1", "arenagate" },
    { "qzdm2", "spillway" },
    { "qzdm3", "hearth" },
    { "qzdm4", "eviscerated" },
    { "qzdm5", "forgotten" },
    { "qzdm6", "campgrounds" },
    { "qzdm7", "retribution" },
    { "qzdm8", "brimstoneabbey" },
    { "qzdm9", "heroskeep" },
    { "qzdm10", "namelessplace" },
    { "qzdm11", "chemicalreaction" },
    { "qzdm12", "dredwerkz" },
    { "qzdm13", "lostworld" },
    { "qzdm14", "grimdungeons" },
    { "qzdm15", "demonkeep" },
    { "qzdm16", "cobaltstation" },
    { "qzdm17", "longestyard" },
    { "qzdm18", "spacechamber" },
    { "qzdm19", "terminalheights" },
    { "qzdm20", "hiddenfortress" },
    { "qzteam1", "basesiege" },
    { "qzteam2", "falloutbunker" },
    { "qzteam3", "innersanctums" },
    { "qzteam4", "scornforge" },
    { "qzteam6", "vortexportal" },
    { "qzteam7", "rebound" },
    { "qztourney1", "powerstation" },
    { "qztourney2", "provinggrounds" },
    { "qztourney3", "hellsgate" },
    { "qztourney4", "verticalvengeance" },
    { "qztourney5", "hellsgateredux" },
    { "qztourney6", "almostlost" },
    { "qztourney7", "furiousheights" },
    { "qztourney8", "sacellum" },
    { "qztourney9", "houseofdecay" },
    { "ztntourney1", "bloodrun" },
};

const char* CG_TranslateMapName(const char* mapname) {
    int i;

    if (!mapname) {
        return NULL;
    }

    for (i = 0; i < (int)ARRAY_LEN(mapTranslations); i++) {
        if (mapTranslations[i].filename &&
            !Q_stricmpn(mapname, mapTranslations[i].filename, strlen(mapname))) {
            return mapTranslations[i].displayName;
        }
    }
    return mapname;
}

void QDECL CG_Printf(const char* msg, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    trap_Print(text);
}

void QDECL CG_Error(const char* msg, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    trap_Error(text);
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

/*
================
CG_Argv
================
*/
const char* CG_Argv(int arg) {
    static char buffer[MAX_STRING_CHARS];

    trap_Argv(arg, buffer, sizeof(buffer));

    return buffer;
}

//========================================================================

/*
=================
CG_AnnouncerSound

[QL] cgamex86.dll 0x10020dd0. The announcer's folder follows cg_announcer: 1 vo, 2 vo_evil,
3 vo_female. Any other value uses vo and puts the cvar back to 1.
=================
*/
static const char* CG_AnnouncerSound(const char* name) {
    const char* voice;

    switch (cg_announcer.integer) {
        case 2:
            voice = "vo_evil";
            break;
        case 3:
            voice = "vo_female";
            break;
        default:
            voice = "vo";
            if (cg_announcer.integer != 1) {
                trap_Cvar_Set("cg_announcer", "1");
                trap_Cvar_Update(&cg_announcer);
            }
            break;
    }
    return va("sound/%s/%s", voice, name);
}

/*
=================
CG_RegisterSounds

called during a precache command

[QL] cgamex86.dll 0x10020e70. Each mode loads only the announcements it can play, the gore
pack's sounds load only when its dlc_gibs folder exists, and every item's pickup sound loads
whether or not the map has the item.
=================
*/
static void CG_RegisterSounds(void) {
    int i;
    int gorePack;
    char name[MAX_QPATH];
    char fileList[1024];
    const char* soundName;

    gorePack = trap_FS_GetFileList("dlc_gibs", "", fileList, sizeof(fileList));

    cgs.media.voDefaultSound = trap_S_RegisterSound("sound/misc/vo_default.ogg", qfalse);
    cgs.media.voEvilSound = trap_S_RegisterSound("sound/misc/vo_evil.ogg", qfalse);
    cgs.media.voFemaleSound = trap_S_RegisterSound("sound/misc/vo_female.ogg", qfalse);
    cgs.media.oneMinuteSound = trap_S_RegisterSound(CG_AnnouncerSound("1_minute.ogg"), qfalse);
    cgs.media.fiveMinuteSound = trap_S_RegisterSound(CG_AnnouncerSound("5_minute.ogg"), qfalse);
    cgs.media.suddenDeathSound = trap_S_RegisterSound(CG_AnnouncerSound("sudden_death.ogg"), qfalse);
    cgs.media.overtimeVoSound = trap_S_RegisterSound(CG_AnnouncerSound("overtime.ogg"), qfalse);
    cgs.media.oneFragSound = trap_S_RegisterSound(CG_AnnouncerSound("1_frag.ogg"), qfalse);
    cgs.media.twoFragSound = trap_S_RegisterSound(CG_AnnouncerSound("2_frags.ogg"), qfalse);
    cgs.media.threeFragSound = trap_S_RegisterSound(CG_AnnouncerSound("3_frags.ogg"), qfalse);
    cgs.media.count3Sound = trap_S_RegisterSound(CG_AnnouncerSound("three.ogg"), qfalse);
    cgs.media.count2Sound = trap_S_RegisterSound(CG_AnnouncerSound("two.ogg"), qfalse);
    cgs.media.count1Sound = trap_S_RegisterSound(CG_AnnouncerSound("one.ogg"), qfalse);
    cgs.media.countFightSound = trap_S_RegisterSound(CG_AnnouncerSound("fight.ogg"), qfalse);
    cgs.media.countPrepareSound = trap_S_RegisterSound(CG_AnnouncerSound("prepare_to_fight.ogg"), qfalse);
    cgs.media.klaxon1Sound = trap_S_RegisterSound("sound/world/klaxon1.ogg", qfalse);
    cgs.media.klaxon2Sound = trap_S_RegisterSound("sound/world/klaxon2.ogg", qfalse);
    cgs.media.buzzerSound = trap_S_RegisterSound("sound/world/buzzer.ogg", qfalse);
    cgs.media.doorGoldSound = trap_S_RegisterSound("sound/movers/doors/door_gold.wav", qfalse);
    cgs.media.doorSilverSound = trap_S_RegisterSound("sound/movers/doors/door_silver.wav", qfalse);

    if (cgs.gametype == GT_RACE) {
        cgs.media.goSound = trap_S_RegisterSound(CG_AnnouncerSound("go.ogg"), qfalse);
        cgs.media.newHighScoreSound = trap_S_RegisterSound(CG_AnnouncerSound("new_high_score.ogg"), qfalse);
    }

    if (cgs.gametype > GT_RACE) {
        cgs.media.hitTeamSound = trap_S_RegisterSound("sound/feedback/hit_teammate.ogg", qfalse);
        cgs.media.returnYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagreturn_yourteam.ogg", qfalse);
        cgs.media.returnOpponentSound = trap_S_RegisterSound("sound/teamplay/flagreturn_opponent.ogg", qfalse);
        cgs.media.countPrepareTeamSound = trap_S_RegisterSound(CG_AnnouncerSound("prepare_your_team.ogg"), qfalse);
        cgs.media.redLeadsSound = trap_S_RegisterSound(CG_AnnouncerSound("red_leads.ogg"), qfalse);
        cgs.media.blueLeadsSound = trap_S_RegisterSound(CG_AnnouncerSound("blue_leads.ogg"), qfalse);
        cgs.media.teamsTiedSound = trap_S_RegisterSound(CG_AnnouncerSound("teams_tied.ogg"), qfalse);
        cgs.media.redScoredSound = trap_S_RegisterSound(CG_AnnouncerSound("red_scores.ogg"), qfalse);
        cgs.media.blueScoredSound = trap_S_RegisterSound(CG_AnnouncerSound("blue_scores.ogg"), qfalse);
        cgs.media.yourTeamScoredSound = trap_S_RegisterSound(CG_AnnouncerSound("your_team_scores.ogg"), qfalse);
        cgs.media.enemyScoredSound = trap_S_RegisterSound(CG_AnnouncerSound("enemy_scores.ogg"), qfalse);
        cgs.media.blueWinsSound = trap_S_RegisterSound(CG_AnnouncerSound("blue_wins.ogg"), qfalse);
        cgs.media.redWinsSound = trap_S_RegisterSound(CG_AnnouncerSound("red_wins.ogg"), qfalse);
    }

    if (BG_IsRoundBasedGameType(cgs.gametype)) {
        cgs.media.thirtySecondWarningSound = trap_S_RegisterSound(CG_AnnouncerSound("30_second_warning.ogg"), qfalse);
        cgs.media.roundBeginsInSound = trap_S_RegisterSound(CG_AnnouncerSound("round_begins_in.ogg"), qfalse);
        cgs.media.redWinsRoundSound = trap_S_RegisterSound(CG_AnnouncerSound("red_wins_round.ogg"), qfalse);
        cgs.media.blueWinsRoundSound = trap_S_RegisterSound(CG_AnnouncerSound("blue_wins_round.ogg"), qfalse);
        cgs.media.roundDrawSound = trap_S_RegisterSound(CG_AnnouncerSound("round_draw.ogg"), qfalse);
        cgs.media.roundOverSound = trap_S_RegisterSound(CG_AnnouncerSound("round_over.ogg"), qfalse);
        cgs.media.survivorSound = trap_S_RegisterSound("sound/feedback/survivor_01.ogg", qfalse);
        cgs.media.lastStandingSound = trap_S_RegisterSound(CG_AnnouncerSound("last_standing.ogg"), qfalse);
    }

    switch (cgs.gametype) {
        case GT_CTF:
        case GT_1FCTF:
        case GT_OBELISK:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            cgs.media.captureAwardSound = trap_S_RegisterSound("sound/teamplay/flagcapture_yourteam.ogg", qfalse);
            cgs.media.captureYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagcapture_yourteam.ogg", qfalse);
            cgs.media.captureOpponentSound = trap_S_RegisterSound("sound/teamplay/flagcapture_opponent.ogg", qfalse);
            cgs.media.returnYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagreturn_yourteam.ogg", qfalse);
            cgs.media.returnOpponentSound = trap_S_RegisterSound("sound/teamplay/flagreturn_opponent.ogg", qfalse);
            cgs.media.takenYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagtaken_yourteam.ogg", qfalse);
            cgs.media.takenOpponentSound = trap_S_RegisterSound("sound/teamplay/flagtaken_opponent.ogg", qfalse);

            if (cgs.gametype == GT_DOMINATION) {
                cgs.media.pointNeutralizedSound = trap_S_RegisterSound("sound/teamplay/flagreturn_opponent.ogg", qfalse);
                cgs.media.pointHumSound = trap_S_RegisterSound("sound/ambient/loop_elec_hum_01_short.ogg", qfalse);
                cgs.media.pointCapturedSounds[0] = trap_S_RegisterSound("sound/teamplay/flagcapture_yourteam.ogg", qfalse);
                cgs.media.pointCapturedSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("a_captured.ogg"), qfalse);
                cgs.media.pointCapturedSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("b_captured.ogg"), qfalse);
                cgs.media.pointCapturedSounds[3] = trap_S_RegisterSound(CG_AnnouncerSound("c_captured.ogg"), qfalse);
                cgs.media.pointCapturedSounds[4] = trap_S_RegisterSound(CG_AnnouncerSound("d_captured.ogg"), qfalse);
                cgs.media.pointCapturedSounds[5] = trap_S_RegisterSound(CG_AnnouncerSound("e_captured.ogg"), qfalse);
                cgs.media.pointLostSounds[0] = trap_S_RegisterSound("sound/teamplay/flagcapture_opponent.ogg", qfalse);
                cgs.media.pointLostSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("a_lost.ogg"), qfalse);
                cgs.media.pointLostSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("b_lost.ogg"), qfalse);
                cgs.media.pointLostSounds[3] = trap_S_RegisterSound(CG_AnnouncerSound("c_lost.ogg"), qfalse);
                cgs.media.pointLostSounds[4] = trap_S_RegisterSound(CG_AnnouncerSound("d_lost.ogg"), qfalse);
                cgs.media.pointLostSounds[5] = trap_S_RegisterSound(CG_AnnouncerSound("e_lost.ogg"), qfalse);
            }

            if (cgs.gametype == GT_CTF || cgs.gametype == GT_AD) {
                cgs.media.blueFlagTakenSound = trap_S_RegisterSound(CG_AnnouncerSound("blue_flag_taken.ogg"), qfalse);
                cgs.media.blueFlagReturnedSound = trap_S_RegisterSound(CG_AnnouncerSound("blue_flag_returned.ogg"), qfalse);
                cgs.media.redFlagTakenSound = trap_S_RegisterSound(CG_AnnouncerSound("red_flag_taken.ogg"), qfalse);
                cgs.media.redFlagReturnedSound = trap_S_RegisterSound(CG_AnnouncerSound("red_flag_returned.ogg"), qfalse);
                cgs.media.yourFlagReturnedSound = trap_S_RegisterSound(CG_AnnouncerSound("your_flag_returned.ogg"), qfalse);
                cgs.media.enemyFlagReturnedSound = trap_S_RegisterSound(CG_AnnouncerSound("enemy_flag_returned.ogg"), qfalse);
                cgs.media.enemyTookYourFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("your_flag_taken.ogg"), qfalse);
                cgs.media.yourTeamTookEnemyFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("enemy_flag_taken.ogg"), qfalse);
                cgs.media.enemyTookTheFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("the_enemy_has_flag.ogg"), qfalse);
                cgs.media.yourTeamTookTheFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("your_team_has_flag.ogg"), qfalse);
            }

            if (cgs.gametype == GT_1FCTF || cgs.gametype == GT_CTF || cgs.gametype == GT_AD) {
                cgs.media.youHaveFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("you_have_flag.ogg"), qfalse);
                cgs.media.holyShitSound = trap_S_RegisterSound(CG_AnnouncerSound("holy_shit.ogg"), qfalse);
            }

            if (cgs.gametype == GT_OBELISK) {
                cgs.media.obeliskHitSound1 = trap_S_RegisterSound("sound/items/obelisk_hit_01.ogg", qfalse);
                cgs.media.obeliskHitSound2 = trap_S_RegisterSound("sound/items/obelisk_hit_02.ogg", qfalse);
                cgs.media.obeliskHitSound3 = trap_S_RegisterSound("sound/items/obelisk_hit_03.ogg", qfalse);
                cgs.media.obeliskRespawnSound = trap_S_RegisterSound("sound/items/obelisk_respawn.ogg", qfalse);
                cgs.media.yourBaseIsUnderAttackSound = trap_S_RegisterSound(CG_AnnouncerSound("base_attack.ogg"), qfalse);
            }

            if (cgs.gametype == GT_AD) {
                cgs.media.attackTheFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("attack_the_flag.ogg"), qfalse);
                cgs.media.defendTheFlagSound = trap_S_RegisterSound(CG_AnnouncerSound("defend_the_flag.ogg"), qfalse);
            }
            break;

        default:
            break;
    }

    if (cgs.gametype == GT_RR) {
        cgs.media.biteSound = trap_S_RegisterSound(CG_AnnouncerSound("bite.ogg"), qfalse);
        cgs.media.infectedSound = trap_S_RegisterSound(CG_AnnouncerSound("infected.ogg"), qfalse);
        cgs.media.nightmareSound = trap_S_RegisterSound("sound/misc/nightmare.wav", qfalse);
        cgs.media.kamikazeRespawnSound = trap_S_RegisterSound("sound/items/kamikazerespawn.wav", qfalse);
    }

    cgs.media.tracerSound = trap_S_RegisterSound("sound/weapons/machinegun/buletby1.ogg", qfalse);
    cgs.media.selectSound = trap_S_RegisterSound("sound/weapons/change.ogg", qfalse);
    cgs.media.wearOffSound = trap_S_RegisterSound("sound/items/wearoff.ogg", qfalse);
    cgs.media.useNothingSound = trap_S_RegisterSound("sound/items/use_nothing.ogg", qfalse);
    cgs.media.electroGibSound[0] = trap_S_RegisterSound("sound/misc/electrogib_01.ogg", qfalse);
    cgs.media.electroGibSound[1] = trap_S_RegisterSound("sound/misc/electrogib_02.ogg", qfalse);
    cgs.media.electroGibSound[2] = trap_S_RegisterSound("sound/misc/electrogib_03.ogg", qfalse);
    cgs.media.electroGibSound[3] = trap_S_RegisterSound("sound/misc/electrogib_04.ogg", qfalse);
    cgs.media.electroGibBounceSound[0] = trap_S_RegisterSound("sound/misc/electrogib_bounce_01.ogg", qfalse);
    cgs.media.electroGibBounceSound[1] = trap_S_RegisterSound("sound/misc/electrogib_bounce_02.ogg", qfalse);
    cgs.media.electroGibBounceSound[2] = trap_S_RegisterSound("sound/misc/electrogib_bounce_03.ogg", qfalse);
    cgs.media.electroGibBounceSound[3] = trap_S_RegisterSound("sound/misc/electrogib_bounce_04.ogg", qfalse);
    cgs.media.electroGibBounceSound[4] = trap_S_RegisterSound("sound/misc/electrogib_bounce_05.ogg", qfalse);

    // [QL] See content/optional/gore/README.md
    if (gorePack > 0) {
        cgs.media.gibSound = trap_S_RegisterSound("dlc_gibs/gibsplt1.ogg", qfalse);
        cgs.media.gibBounce1Sound = trap_S_RegisterSound("dlc_gibs/gibimp1.ogg", qfalse);
        cgs.media.gibBounce2Sound = trap_S_RegisterSound("dlc_gibs/gibimp2.ogg", qfalse);
        cgs.media.gibBounce3Sound = trap_S_RegisterSound("dlc_gibs/gibimp3.ogg", qfalse);
        cgs.media.invulnerabilityJuicedSound = trap_S_RegisterSound("dlc_gibs/invul_juiced.ogg", qfalse);
    }

    cgs.media.useInvulnerabilitySound = trap_S_RegisterSound("sound/items/invul_activate.ogg", qfalse);
    cgs.media.invulnerabilityImpactSound1 = trap_S_RegisterSound("sound/items/invul_impact_01.ogg", qfalse);
    cgs.media.invulnerabilityImpactSound2 = trap_S_RegisterSound("sound/items/invul_impact_02.ogg", qfalse);
    cgs.media.invulnerabilityImpactSound3 = trap_S_RegisterSound("sound/items/invul_impact_03.ogg", qfalse);
    cgs.media.teleInSound = trap_S_RegisterSound("sound/world/telein.ogg", qfalse);
    cgs.media.teleOutSound = trap_S_RegisterSound("sound/world/teleout.ogg", qfalse);
    cgs.media.respawnSound = trap_S_RegisterSound("sound/items/respawn1.ogg", qfalse);
    cgs.media.lowAmmoSound = trap_S_RegisterSound("sound/weapons/lowammo.ogg", qfalse);
    cgs.media.noAmmoSound = trap_S_RegisterSound("sound/weapons/noammo.ogg", qfalse);
    cgs.media.talkSound = trap_S_RegisterSound("sound/player/talk.ogg", qfalse);
    cgs.media.landSound = trap_S_RegisterSound("sound/player/land1.ogg", qfalse);

    cgs.media.hitSounds[0] = trap_S_RegisterSound("sound/feedback/hit0.ogg", qfalse);
    cgs.media.hitSounds[1] = trap_S_RegisterSound("sound/feedback/hit1.ogg", qfalse);
    cgs.media.hitSounds[2] = trap_S_RegisterSound("sound/feedback/hit2.ogg", qfalse);
    cgs.media.hitSounds[3] = trap_S_RegisterSound("sound/feedback/hit3.ogg", qfalse);
    cgs.media.killBeepSounds[0] = trap_S_RegisterSound("sound/feedback/impact1.ogg", qfalse);
    cgs.media.killBeepSounds[1] = trap_S_RegisterSound("sound/feedback/impact2.ogg", qfalse);
    cgs.media.killBeepSounds[2] = trap_S_RegisterSound("sound/feedback/impact3.ogg", qfalse);
    cgs.media.killBeepSounds[3] = trap_S_RegisterSound("sound/feedback/impact4.ogg", qfalse);
    cgs.media.killBeepSounds[4] = trap_S_RegisterSound("sound/feedback/impact5.ogg", qfalse);
    cgs.media.killBeepSounds[5] = trap_S_RegisterSound("sound/feedback/impact6.ogg", qfalse);
    cgs.media.killBeepSounds[6] = trap_S_RegisterSound("sound/world/bell_01.ogg", qfalse);
    cgs.media.killBeepSounds[7] = trap_S_RegisterSound("sound/misc/chaching.ogg", qfalse);

    cgs.media.watrInSound = trap_S_RegisterSound("sound/player/watr_in.ogg", qfalse);
    cgs.media.watrOutSound = trap_S_RegisterSound("sound/player/watr_out.ogg", qfalse);
    cgs.media.watrUnSound = trap_S_RegisterSound("sound/player/watr_un.ogg", qfalse);
    cgs.media.jumpPadSound = trap_S_RegisterSound("sound/world/jumppad.ogg", qfalse);

    cgs.media.impressiveSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("impressive1.ogg"), qfalse);
    cgs.media.impressiveSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("impressive2.ogg"), qfalse);
    cgs.media.impressiveSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("impressive3.ogg"), qfalse);
    cgs.media.excellentSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("excellent1.ogg"), qfalse);
    cgs.media.excellentSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("excellent2.ogg"), qfalse);
    cgs.media.excellentSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("excellent3.ogg"), qfalse);
    cgs.media.deniedSound = trap_S_RegisterSound(CG_AnnouncerSound("denied.ogg"), qfalse);
    cgs.media.perfectSound = trap_S_RegisterSound(CG_AnnouncerSound("perfect.ogg"), qfalse);
    cgs.media.humiliationSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("humiliation1.ogg"), qfalse);
    cgs.media.humiliationSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("humiliation2.ogg"), qfalse);
    cgs.media.humiliationSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("humiliation3.ogg"), qfalse);
    cgs.media.assistSound = trap_S_RegisterSound(CG_AnnouncerSound("assist.ogg"), qfalse);
    cgs.media.defendSound = trap_S_RegisterSound(CG_AnnouncerSound("defense.ogg"), qfalse);
    cgs.media.comboKillSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("combokill1.ogg"), qfalse);
    cgs.media.comboKillSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("combokill2.ogg"), qfalse);
    cgs.media.comboKillSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("combokill3.ogg"), qfalse);
    cgs.media.rampageSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("rampage1.ogg"), qfalse);
    cgs.media.rampageSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("rampage2.ogg"), qfalse);
    cgs.media.rampageSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("rampage3.ogg"), qfalse);
    cgs.media.midAirSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("midair1.ogg"), qfalse);
    cgs.media.midAirSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("midair2.ogg"), qfalse);
    cgs.media.midAirSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("midair3.ogg"), qfalse);
    cgs.media.revengeSounds[0] = trap_S_RegisterSound(CG_AnnouncerSound("revenge1.ogg"), qfalse);
    cgs.media.revengeSounds[1] = trap_S_RegisterSound(CG_AnnouncerSound("revenge2.ogg"), qfalse);
    cgs.media.revengeSounds[2] = trap_S_RegisterSound(CG_AnnouncerSound("revenge3.ogg"), qfalse);
    cgs.media.firstFragSound = trap_S_RegisterSound(CG_AnnouncerSound("first_frag.ogg"), qfalse);
    cgs.media.perforatedSound = trap_S_RegisterSound(CG_AnnouncerSound("perforated.ogg"), qfalse);
    cgs.media.firstImpressiveSound = trap_S_RegisterSound(CG_AnnouncerSound("first_impressive.ogg"), qfalse);
    cgs.media.firstExcellentSound = trap_S_RegisterSound(CG_AnnouncerSound("first_excellent.ogg"), qfalse);
    cgs.media.firstHumiliationSound = trap_S_RegisterSound(CG_AnnouncerSound("first_gauntlet.ogg"), qfalse);
    cgs.media.headshotSound = trap_S_RegisterSound(CG_AnnouncerSound("headshot.ogg"), qfalse);
    cgs.media.accuracySound = trap_S_RegisterSound(CG_AnnouncerSound("accuracy.ogg"), qfalse);
    cgs.media.quadGodSound = trap_S_RegisterSound(CG_AnnouncerSound("quad_god.ogg"), qfalse);
    cgs.media.takenLeadSound = trap_S_RegisterSound(CG_AnnouncerSound("lead_taken.ogg"), qfalse);
    cgs.media.tiedLeadSound = trap_S_RegisterSound(CG_AnnouncerSound("lead_tied.ogg"), qfalse);
    cgs.media.lostLeadSound = trap_S_RegisterSound(CG_AnnouncerSound("lead_lost.ogg"), qfalse);
    cgs.media.voteNow = trap_S_RegisterSound(CG_AnnouncerSound("vote_now.ogg"), qfalse);
    cgs.media.votePassed = trap_S_RegisterSound(CG_AnnouncerSound("vote_passed.ogg"), qfalse);
    cgs.media.voteFailed = trap_S_RegisterSound(CG_AnnouncerSound("vote_failed.ogg"), qfalse);
    cgs.media.winnerSound = trap_S_RegisterSound(CG_AnnouncerSound("you_win.ogg"), qfalse);
    cgs.media.loserSound = trap_S_RegisterSound(CG_AnnouncerSound("you_lose.ogg"), qfalse);

    for (i = 0; i < 4; i++) {
        Com_sprintf(name, sizeof(name), "sound/player/footsteps/step%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_NORMAL][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/boot%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_BOOT][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/flesh%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_FLESH][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/mech%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_MECH][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/energy%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_ENERGY][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/splash%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_SPLASH][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/clank%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_METAL][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/snow%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_SNOW][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/wood%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_WOOD][i] = trap_S_RegisterSound(name, qfalse);
    }

    for (i = 1; i < bg_numItems; i++) {
        if (bg_itemlist[i].pickup_sound) {
            trap_S_RegisterSound(bg_itemlist[i].pickup_sound, qfalse);
        }
    }

    for (i = 1; i < MAX_SOUNDS; i++) {
        soundName = CG_ConfigString(CS_SOUNDS + i);
        if (!soundName[0]) {
            break;
        }
        if (soundName[0] == '*') {
            continue;  // custom sound
        }
        cgs.gameSounds[i] = trap_S_RegisterSound(soundName, qfalse);
    }

    cgs.media.battleSuitVoSound = trap_S_RegisterSound(CG_AnnouncerSound("battlesuit.ogg"), qfalse);
    cgs.media.hasteVoSound = trap_S_RegisterSound(CG_AnnouncerSound("haste.ogg"), qfalse);
    cgs.media.invisibilityVoSound = trap_S_RegisterSound(CG_AnnouncerSound("invisibility.ogg"), qfalse);
    cgs.media.quadDamageVoSound = trap_S_RegisterSound(CG_AnnouncerSound("quad_damage.ogg"), qfalse);
    cgs.media.regenerationVoSound = trap_S_RegisterSound(CG_AnnouncerSound("regeneration.ogg"), qfalse);
    cgs.media.armorRegenVoSound = trap_S_RegisterSound(CG_AnnouncerSound("armor_regen.ogg"), qfalse);
    cgs.media.damageVoSound = trap_S_RegisterSound(CG_AnnouncerSound("damage.ogg"), qfalse);
    cgs.media.guardVoSound = trap_S_RegisterSound(CG_AnnouncerSound("guard.ogg"), qfalse);
    cgs.media.scoutVoSound = trap_S_RegisterSound(CG_AnnouncerSound("scout.ogg"), qfalse);
    cgs.media.flightSound = trap_S_RegisterSound("sound/items/flight.ogg", qfalse);
    cgs.media.medkitSound = trap_S_RegisterSound("sound/items/use_medkit.ogg", qfalse);
    cgs.media.quadSound = trap_S_RegisterSound("sound/items/damage3.ogg", qfalse);
    cgs.media.sfx_ric1 = trap_S_RegisterSound("sound/weapons/machinegun/ric1.ogg", qfalse);
    cgs.media.sfx_ric2 = trap_S_RegisterSound("sound/weapons/machinegun/ric2.ogg", qfalse);
    cgs.media.sfx_ric3 = trap_S_RegisterSound("sound/weapons/machinegun/ric3.ogg", qfalse);
    cgs.media.sfx_rockexp = trap_S_RegisterSound("sound/weapons/rocket/rocklx1a.ogg", qfalse);
    cgs.media.sfx_plasmaexp = trap_S_RegisterSound("sound/weapons/plasma/plasmx1a.ogg", qfalse);
    cgs.media.sfx_proxexp = trap_S_RegisterSound("sound/weapons/proxmine/wstbexpl.ogg", qfalse);
    cgs.media.sfx_nghit = trap_S_RegisterSound("sound/weapons/nailgun/wnalimpd.ogg", qfalse);
    cgs.media.sfx_nghitflesh = trap_S_RegisterSound("sound/weapons/nailgun/wnalimpl.ogg", qfalse);
    cgs.media.sfx_nghitmetal = trap_S_RegisterSound("sound/weapons/nailgun/wnalimpm.ogg", qfalse);
    cgs.media.sfx_chghit = trap_S_RegisterSound("sound/weapons/vulcan/wvulimpd.ogg", qfalse);
    cgs.media.sfx_chghitflesh = trap_S_RegisterSound("sound/weapons/vulcan/wvulimpl.ogg", qfalse);
    cgs.media.sfx_chghitmetal = trap_S_RegisterSound("sound/weapons/vulcan/wvulimpm.ogg", qfalse);
    cgs.media.chaingunWindSound = trap_S_RegisterSound("sound/weapons/vulcan/wvulwind.ogg", qfalse);
    cgs.media.weaponHoverSound = trap_S_RegisterSound("sound/weapons/weapon_hover.ogg", qfalse);
    cgs.media.grappleHitSound = trap_S_RegisterSound("sound/weapons/grapple/grhit.ogg", qfalse);
    cgs.media.kamikazeExplodeSound = trap_S_RegisterSound("sound/items/kam_explode.ogg", qfalse);
    cgs.media.kamikazeImplodeSound = trap_S_RegisterSound("sound/items/kam_implode.ogg", qfalse);
    cgs.media.kamikazeFarSound = trap_S_RegisterSound("sound/items/kam_explode_far.ogg", qfalse);
    cgs.media.youSuckSound = trap_S_RegisterSound("sound/misc/yousuck.ogg", qfalse);
    cgs.media.wstbimplSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpl.ogg", qfalse);
    cgs.media.wstbimpmSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpm.ogg", qfalse);
    cgs.media.wstbimpdSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpd.ogg", qfalse);
    cgs.media.wstbactvSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbactv.ogg", qfalse);
    cgs.media.regenSound = trap_S_RegisterSound("sound/items/regen.ogg", qfalse);
    cgs.media.armorPickupSound = trap_S_RegisterSound("sound/misc/ar1_pkup.ogg", qfalse);
    cgs.media.protectSound = trap_S_RegisterSound("sound/items/protect3.ogg", qfalse);
    cgs.media.n_healthSound = trap_S_RegisterSound("sound/items/n_health.ogg", qfalse);
    cgs.media.hgrenb1aSound = trap_S_RegisterSound("sound/weapons/grenade/hgrenb1a.ogg", qfalse);
    cgs.media.hgrenb2aSound = trap_S_RegisterSound("sound/weapons/grenade/hgrenb2a.ogg", qfalse);
    cgs.media.timPumpSound = trap_S_RegisterSound("sound/misc/tim_pump.ogg", qfalse);
}

//===================================================================================

/*
=================
CG_RegisterGraphics

This function may execute for a couple of minutes with a slow disk.
=================
*/
static void CG_RegisterGraphics(void) {
    int i;
    char items[MAX_ITEMS + 1];
    static char* sb_nums[11] = {
        "gfx/2d/numbers/zero_32b",
        "gfx/2d/numbers/one_32b",
        "gfx/2d/numbers/two_32b",
        "gfx/2d/numbers/three_32b",
        "gfx/2d/numbers/four_32b",
        "gfx/2d/numbers/five_32b",
        "gfx/2d/numbers/six_32b",
        "gfx/2d/numbers/seven_32b",
        "gfx/2d/numbers/eight_32b",
        "gfx/2d/numbers/nine_32b",
        "gfx/2d/numbers/minus_32b",
    };
    // [QL] Race checkpoint assets (binary-verified from cgamex86.dll CG_RegisterGraphics)
    cgs.media.raceFlagB = trap_R_RegisterModel("models/flag3/b_flag3.md3");
    cgs.media.raceFlagR = trap_R_RegisterModel("models/flag3/r_flag3.md3");
    cgs.media.raceFlagN = trap_R_RegisterModel("models/flag3/n_flag3.md3");
    cgs.media.raceFlagF = trap_R_RegisterModel("models/flag3/f_flag3.md3");
    cgs.media.raceFlagG = trap_R_RegisterModel("models/flag3/g_flag3.md3");
    cgs.media.raceFlagD = trap_R_RegisterModel("models/flag3/d_flag3.md3");
    cgs.media.raceMarkerStart = trap_R_RegisterShader("gfx/2d/race/start");
    cgs.media.raceMarkerCheckpoint = trap_R_RegisterShader("gfx/2d/race/checkpoint");
    cgs.media.raceMarkerFinish = trap_R_RegisterShader("gfx/2d/race/finish");
    cgs.media.domPointModel = trap_R_RegisterModel("models/powerups/domination/dompoint.md3");
    cgs.media.domSkinRed = trap_R_RegisterSkin("models/powerups/domination/domred.skin");
    cgs.media.domSkinBlue = trap_R_RegisterSkin("models/powerups/domination/domblue.skin");
    cgs.media.domSkinNeutral = trap_R_RegisterSkin("models/powerups/domination/domntrl.skin");

    // clear any references to old media
    memset(&cg.refdef, 0, sizeof(cg.refdef));
    trap_R_ClearScene();

    CG_LoadingString(cgs.mapname);

    trap_R_LoadWorldMap(cgs.mapname);

    // precache status bar pics
    CG_LoadingString("game media");

    for (i = 0; i < 11; i++) {
        cgs.media.numberShaders[i] = trap_R_RegisterShader(sb_nums[i]);
    }

    cgs.media.botSkillShaders[0] = trap_R_RegisterShader("menu/art/skill1.png");
    cgs.media.botSkillShaders[1] = trap_R_RegisterShader("menu/art/skill2.png");
    cgs.media.botSkillShaders[2] = trap_R_RegisterShader("menu/art/skill3.png");
    cgs.media.botSkillShaders[3] = trap_R_RegisterShader("menu/art/skill4.png");
    cgs.media.botSkillShaders[4] = trap_R_RegisterShader("menu/art/skill5.png");

    cgs.media.viewBloodShader = trap_R_RegisterShader("viewBloodBlend");
    // [QL] cgamex86.dll 0x100230e7 registers "viewDamageBlend" into 0x10a5f4f4, right
    // alongside viewBloodBlend (0x100242da -> 0x10a5f4f0). They are the two halves of one
    // choice, not alternatives that happen to coexist: pak00's scripts/gfx.shader points
    // viewDamageBlend at gfx/damage/damage_screen, which ships with the base game, and
    // viewBloodBlend at dlc_gibs/blood_screen, which only the Steam Workshop gore pack
    // brings. Registering only the blood one left a stock install drawing the default
    // (missing-texture) shader across the screen on every hit.
    cgs.media.viewDamageShader = trap_R_RegisterShader("viewDamageBlend");

    cgs.media.deferShader = trap_R_RegisterShaderNoMip("gfx/2d/defer.png");

    cgs.media.scoreboardName = trap_R_RegisterShaderNoMip("menu/tab/name.tga");
    cgs.media.scoreboardPing = trap_R_RegisterShaderNoMip("menu/tab/ping.tga");
    cgs.media.scoreboardScore = trap_R_RegisterShaderNoMip("menu/tab/score.tga");
    cgs.media.scoreboardTime = trap_R_RegisterShaderNoMip("menu/tab/time.tga");

    // [QL] the duel board's status plates, in the binary's order (0x10023804..0x100238e3)
    cgs.media.duelPlateReady[0] = trap_R_RegisterShader("ui/assets/score/1st_plyr_ready.tga");
    cgs.media.duelPlateNotReady[0] = trap_R_RegisterShader("ui/assets/score/1st_plyr_notready.tga");
    cgs.media.duelPlateReady[1] = trap_R_RegisterShader("ui/assets/score/2nd_plyr_ready.tga");
    cgs.media.duelPlateNotReady[1] = trap_R_RegisterShader("ui/assets/score/2nd_plyr_notready.tga");
    cgs.media.duelPlateLeads[0] = trap_R_RegisterShader("ui/assets/score/1st_plyr_leads.tga");
    cgs.media.duelPlateTied[0] = trap_R_RegisterShader("ui/assets/score/1st_plyr_tied.tga");
    cgs.media.duelPlateTrails[0] = trap_R_RegisterShader("ui/assets/score/1st_plyr_trails.tga");
    cgs.media.duelPlateLeads[1] = trap_R_RegisterShader("ui/assets/score/2nd_plyr_leads.tga");
    cgs.media.duelPlateTied[1] = trap_R_RegisterShader("ui/assets/score/2nd_plyr_tied.tga");
    cgs.media.duelPlateTrails[1] = trap_R_RegisterShader("ui/assets/score/2nd_plyr_trails.tga");
    if (cgs.gametype >= GT_FREEZE) {
        cgs.media.thawIconShader = trap_R_RegisterShader("icons/thaw");
        // [QL] the base gibs, registered right here by the binary and distinct from the gore
        // pack's (dlc_gibs): it keeps the two sets in separate globals
        cgs.media.gibBrainBase = trap_R_RegisterModel("models/gibs/brain.md3");
        cgs.media.gibAbdomenBase = trap_R_RegisterModel("models/gibs/abdomen.md3");
    }

    cgs.media.smokePuffShader = trap_R_RegisterShader("smokePuff");
    cgs.media.smokePuffRageProShader = trap_R_RegisterShader("smokePuffRagePro");
    cgs.media.shotgunSmokePuffShader = trap_R_RegisterShader("shotgunSmokePuff");

    cgs.media.nailPuffShader = trap_R_RegisterShader("nailtrail");
    cgs.media.blueProxMine = trap_R_RegisterModel("models/weaphits/proxmineb.md3");

    cgs.media.plasmaBallShader = trap_R_RegisterShader("sprites/plasma1");
    // [QL] blood spray shaders (translucent sprites with proper alpha blend)
    cgs.media.bloodSprayShaders[0] = trap_R_RegisterShader("bloodSpray1");
    cgs.media.bloodSprayShaders[1] = trap_R_RegisterShader("bloodSpray2");
    cgs.media.bloodSprayShaders[2] = trap_R_RegisterShader("bloodSpray3");
    cgs.media.bloodSprayShaders[3] = trap_R_RegisterShader("bloodSpray4");
    cgs.media.bloodTrailShader = cgs.media.bloodSprayShaders[0];
    cgs.media.lagometerShader = trap_R_RegisterShader("lagometer");
    cgs.media.connectionShader = trap_R_RegisterShader("disconnected");

    cgs.media.waterBubbleShader = trap_R_RegisterShader("waterBubble");

    // [QL] 0x10024d9d: the item respawn timer's faces and wedges
    cgs.media.timerIcons[TIMER_ICON_ARMOR] = trap_R_RegisterShader("gfx/2d/timer/armor");
    cgs.media.timerIcons[TIMER_ICON_BS] = trap_R_RegisterShader("gfx/2d/timer/bs");
    cgs.media.timerIcons[TIMER_ICON_HASTE] = trap_R_RegisterShader("gfx/2d/timer/haste");
    cgs.media.timerIcons[TIMER_ICON_HEALTH] = trap_R_RegisterShader("gfx/2d/timer/health");
    cgs.media.timerIcons[TIMER_ICON_INVIS] = trap_R_RegisterShader("gfx/2d/timer/invis");
    cgs.media.timerIcons[TIMER_ICON_MEDKIT] = trap_R_RegisterShader("gfx/2d/timer/medkit");
    cgs.media.timerIcons[TIMER_ICON_QUAD] = trap_R_RegisterShader("gfx/2d/timer/quad");
    cgs.media.timerIcons[TIMER_ICON_REGEN] = trap_R_RegisterShader("gfx/2d/timer/regen");
    cgs.media.timerIcons[TIMER_ICON_UNKNOWN] = trap_R_RegisterShader("gfx/2d/timer/unknown");
    cgs.media.timerSlices[0][0] = trap_R_RegisterShader("gfx/2d/timer/slice5");
    cgs.media.timerSlices[0][1] = trap_R_RegisterShader("gfx/2d/timer/slice5_current");
    cgs.media.timerSlices[1][0] = trap_R_RegisterShader("gfx/2d/timer/slice7");
    cgs.media.timerSlices[1][1] = trap_R_RegisterShader("gfx/2d/timer/slice7_current");
    cgs.media.timerSlices[2][0] = trap_R_RegisterShader("gfx/2d/timer/slice12");
    cgs.media.timerSlices[2][1] = trap_R_RegisterShader("gfx/2d/timer/slice12_current");
    cgs.media.timerSlices[3][0] = trap_R_RegisterShader("gfx/2d/timer/slice24");
    cgs.media.timerSlices[3][1] = trap_R_RegisterShader("gfx/2d/timer/slice24_current");

    cgs.media.tracerShader = trap_R_RegisterShader("gfx/misc/tracer");
    // [QL] 0x1002321e: the impact sparks carry their own handle on the same shader
    cgs.media.sparkParticleShader = trap_R_RegisterShader("gfx/misc/tracer");
    cgs.media.selectShader = trap_R_RegisterShader("gfx/2d/select");
    cgs.media.weaponBarHighlightShader = trap_R_RegisterShader("ui/assets/hud/weaplit2.tga");
    cgs.media.weaponBarIdleShader[0] = trap_R_RegisterShader("ui/assets/hud/weapidle1.tga");
    cgs.media.weaponBarIdleShader[1] = trap_R_RegisterShader("ui/assets/hud/weapidle2.tga");
    cgs.media.infiniteAmmoShader = trap_R_RegisterShader("icons/infinite.tga");

    for (i = 1; i < NUM_CROSSHAIRS; i++) {
        cgs.media.crosshairShader[i] = trap_R_RegisterShader(va("gfx/2d/crosshair%i", i));
    }

    cgs.media.backTileShader = trap_R_RegisterShader("gfx/2d/backtile");
    cgs.media.noammoShader = trap_R_RegisterShader("icons/noammo");
    cgs.media.fragIconShader = trap_R_RegisterShader("icons/icon_frag");

    // powerup shaders
    cgs.media.quadShader = trap_R_RegisterShader("powerups/quad");
    cgs.media.quadWeaponShader = trap_R_RegisterShader("powerups/quadWeapon");
    cgs.media.brightWeaponShader = trap_R_RegisterShader("ioqlBrightWeapon");
    cgs.media.battleSuitShader = trap_R_RegisterShader("powerups/battleSuit");
    cgs.media.battleWeaponShader = trap_R_RegisterShader("powerups/battleWeapon");
    cgs.media.invisShader = trap_R_RegisterShader("powerups/invisibility");
    cgs.media.ghostWeaponShader = trap_R_RegisterShader("ghostWeaponShader");
    cgs.media.regenShader = trap_R_RegisterShader("powerups/regen");
    cgs.media.spawnArmorShader = trap_R_RegisterShader("powerups/spawnArmor");
    cgs.media.spawnArmor2Shader = trap_R_RegisterShader("powerups/spawnArmor2");
    cgs.media.noPlayerClipShader = trap_R_RegisterShader("noPlayerClipShader");
    cgs.media.gooShader = trap_R_RegisterShader("powerups/goo");
    cgs.media.xrayMaskShader = trap_R_RegisterShader("ioqlXrayMask");
    cgs.media.xrayShader = trap_R_RegisterShader("ioqlXray");
    // [QL] 0x100236e4: the markers drawn over the powerups
    cgs.media.powerupPoi[0] = trap_R_RegisterShader("gfx/2d/powerup/quad");
    cgs.media.powerupPoi[1] = trap_R_RegisterShader("gfx/2d/powerup/bs");
    cgs.media.powerupPoi[2] = trap_R_RegisterShader("gfx/2d/powerup/haste");
    cgs.media.powerupPoi[3] = trap_R_RegisterShader("gfx/2d/powerup/invis");
    cgs.media.powerupPoi[4] = trap_R_RegisterShader("gfx/2d/powerup/regen");
    cgs.media.powerupIncomingShader = trap_R_RegisterShader("gfx/2d/powerup/incoming");
    cgs.media.hastePuffShader = trap_R_RegisterShader("hasteSmokePuff");

    // [QL] gametype icons for scoreboard/HUD (from pak00: ui/assets/hud/*.png)
    cgs.media.gametypeIcon[GT_FFA]        = trap_R_RegisterShaderNoMip("ui/assets/hud/ffa");
    cgs.media.gametypeIcon[GT_DUEL]       = trap_R_RegisterShaderNoMip("ui/assets/hud/duel");
    cgs.media.gametypeIcon[GT_RACE]       = trap_R_RegisterShaderNoMip("ui/assets/hud/race");
    cgs.media.gametypeIcon[GT_TEAM]       = trap_R_RegisterShaderNoMip("ui/assets/hud/tdm");
    cgs.media.gametypeIcon[GT_CA]         = trap_R_RegisterShaderNoMip("ui/assets/hud/ca");
    cgs.media.gametypeIcon[GT_CTF]        = trap_R_RegisterShaderNoMip("ui/assets/hud/ctf");
    cgs.media.gametypeIcon[GT_1FCTF]      = trap_R_RegisterShaderNoMip("ui/assets/hud/1f");
    cgs.media.gametypeIcon[GT_OBELISK]    = trap_R_RegisterShaderNoMip("ui/assets/hud/ob");
    cgs.media.gametypeIcon[GT_HARVESTER]  = trap_R_RegisterShaderNoMip("ui/assets/hud/har");
    cgs.media.gametypeIcon[GT_FREEZE]     = trap_R_RegisterShaderNoMip("ui/assets/hud/ft");
    cgs.media.gametypeIcon[GT_DOMINATION] = trap_R_RegisterShaderNoMip("ui/assets/hud/dom");
    cgs.media.gametypeIcon[GT_AD]         = trap_R_RegisterShaderNoMip("ui/assets/hud/ad");
    cgs.media.gametypeIcon[GT_RR]         = trap_R_RegisterShaderNoMip("ui/assets/hud/rr");
    cgs.media.raceNavShader = trap_R_RegisterShader("gfx/misc/racenav.jpg");

    if (cgs.gametype == GT_HARVESTER) {
        cgs.media.redCubeModel = trap_R_RegisterModel("models/powerups/orb/r_orb.md3");
        cgs.media.blueCubeModel = trap_R_RegisterModel("models/powerups/orb/b_orb.md3");
        cgs.media.redCubeIcon = trap_R_RegisterShader("icons/skull_red");
        cgs.media.blueCubeIcon = trap_R_RegisterShader("icons/skull_blue");
    }

    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER) {
        cgs.media.redFlagModel = trap_R_RegisterModel("models/flags/r_flag.md3");
        cgs.media.blueFlagModel = trap_R_RegisterModel("models/flags/b_flag.md3");
        cgs.media.redFlagShader[0] = trap_R_RegisterShaderNoMip("icons/iconf_red1");
        cgs.media.redFlagShader[1] = trap_R_RegisterShaderNoMip("icons/iconf_red2");
        cgs.media.redFlagShader[2] = trap_R_RegisterShaderNoMip("icons/iconf_red3");
        cgs.media.blueFlagShader[0] = trap_R_RegisterShaderNoMip("icons/iconf_blu1");
        cgs.media.blueFlagShader[1] = trap_R_RegisterShaderNoMip("icons/iconf_blu2");
        cgs.media.blueFlagShader[2] = trap_R_RegisterShaderNoMip("icons/iconf_blu3");

        cgs.media.flagPoleModel = trap_R_RegisterModel("models/flag2/flagpole.md3");
        cgs.media.flagFlapModel = trap_R_RegisterModel("models/flag2/flagflap3.md3");

        cgs.media.redFlagFlapSkin = trap_R_RegisterSkin("models/flag2/red.skin");
        cgs.media.blueFlagFlapSkin = trap_R_RegisterSkin("models/flag2/blue.skin");
        cgs.media.neutralFlagFlapSkin = trap_R_RegisterSkin("models/flag2/white.skin");

        cgs.media.redFlagBaseModel = trap_R_RegisterModel("models/mapobjects/flagbase/red_base.md3");
        cgs.media.blueFlagBaseModel = trap_R_RegisterModel("models/mapobjects/flagbase/blue_base.md3");
        cgs.media.neutralFlagBaseModel = trap_R_RegisterModel("models/mapobjects/flagbase/ntrl_base.md3");
    }

    // [QL] the binary always registers the gold key, outside any conditional
    cgs.media.keyGoldModel = trap_R_RegisterModel("models/powerups/keys/key_gold.md3");
    cgs.media.droppedFlagModel = trap_R_RegisterModel("models/flags/d_flag.md3");

    if (cgs.gametype == GT_1FCTF) {
        cgs.media.neutralFlagModel = trap_R_RegisterModel("models/flags/n_flag.md3");
        cgs.media.flagShader[0] = trap_R_RegisterShaderNoMip("icons/iconf_neutral1");
        cgs.media.flagShader[1] = trap_R_RegisterShaderNoMip("icons/iconf_red2");
        cgs.media.flagShader[2] = trap_R_RegisterShaderNoMip("icons/iconf_blu2");
        cgs.media.flagShader[3] = trap_R_RegisterShaderNoMip("icons/iconf_neutral3");
    }

    if (cgs.gametype == GT_OBELISK) {
        cgs.media.rocketExplosionShader = trap_R_RegisterShader("rocketExplosion");
        cgs.media.overloadBaseModel = trap_R_RegisterModel("models/powerups/overload_base.md3");
        cgs.media.overloadTargetModel = trap_R_RegisterModel("models/powerups/overload_target.md3");
        cgs.media.overloadLightsModel = trap_R_RegisterModel("models/powerups/overload_lights.md3");
        cgs.media.overloadEnergyModel = trap_R_RegisterModel("models/powerups/overload_energy.md3");
    }

    if (cgs.gametype == GT_HARVESTER) {
        cgs.media.harvesterModel = trap_R_RegisterModel("models/powerups/harvester/harvester.md3");
        cgs.media.harvesterRedSkin = trap_R_RegisterSkin("models/powerups/harvester/red.skin");
        cgs.media.harvesterBlueSkin = trap_R_RegisterSkin("models/powerups/harvester/blue.skin");
        cgs.media.harvesterNeutralModel = trap_R_RegisterModel("models/powerups/obelisk/obelisk.md3");
    }

    // [QL] 0x1002303c: the segments of the health bars over teammates
    cgs.media.healthSegmentShader = trap_R_RegisterShader("gfx/2d/health_segment.tga");
    // [QL] 0x100233f4: the arrows of cg_drawInputCmds
    cgs.media.cmdUpShader = trap_R_RegisterShaderNoMip("gfx/2d/race/cmd_up");
    cgs.media.cmdDownShader = trap_R_RegisterShaderNoMip("gfx/2d/race/cmd_down");
    cgs.media.cmdRightShader = trap_R_RegisterShaderNoMip("gfx/2d/race/cmd_right");
    cgs.media.cmdLeftShader = trap_R_RegisterShaderNoMip("gfx/2d/race/cmd_left");

    // [QL] 0x100240d9: the bite over Red Rover's survivors
    if (cgs.gametype == GT_RR) {
        cgs.media.infectedShader = trap_R_RegisterShaderNoMip("gfx/2d/infected/bite");
    }

    cgs.media.redKamikazeShader = trap_R_RegisterShader("models/weaphits/kamikred");
    cgs.media.dustPuffShader = trap_R_RegisterShader("hasteSmokePuff");

    if (cgs.gametype >= GT_TEAM) {
        cgs.media.redQuadShader = trap_R_RegisterShader("powerups/blueflag");
        cgs.media.teamStatusBar = trap_R_RegisterShader("gfx/2d/colorbar.tga");
        cgs.media.blueKamikazeShader = trap_R_RegisterShader("models/weaphits/kamikblu");
        cgs.media.poiShader = trap_R_RegisterShader("sprites/neutralflagcarrier");
        // [QL] 0x100237e7 and 0x100240f4: the same marker wears a different face when
        // the match is running Quad Hog or Red Rover
        cgs.media.quadHogPoiShader = trap_R_RegisterShader("gfx/2d/quad_hog/quadhog");
        cgs.media.infectedPoiShader = trap_R_RegisterShader("gfx/2d/infected/bite");
        // [QL] 0x1002415d: the teammate markers
        cgs.media.friendShader = trap_R_RegisterShader("sprites/friend");
        cgs.media.friendHitShader = trap_R_RegisterShader("sprites/friend_hit");
        cgs.media.friendDeadShader = trap_R_RegisterShader("sprites/friend_dead");
    }
    // [QL] 0x100241e7: the flag carriers' markers, in the modes with flags
    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_AD) {
        cgs.media.flagCarrierShader = trap_R_RegisterShader("sprites/flagcarrier");
        cgs.media.flagCarrierHitShader = trap_R_RegisterShader("sprites/flagcarrier_hit");
    }

    cgs.media.armorModel = trap_R_RegisterModel("models/powerups/armor/armor_yel.md3");
    cgs.media.armorIcon = trap_R_RegisterShaderNoMip("icons/iconr_yellow");

    cgs.media.machinegunBrassModel = trap_R_RegisterModel("models/weapons2/shells/m_shell.md3");
    cgs.media.shotgunBrassModel = trap_R_RegisterModel("models/weapons2/shells/s_shell.md3");

    cgs.media.smoke2 = trap_R_RegisterModel("models/weapons2/shells/s_shell.md3");

    cgs.media.balloonShader = trap_R_RegisterShader("sprites/balloon3");
    // [QL] 0x100244f5: the followed player's frame in the team vitals
    cgs.media.voiceWindowShader = trap_R_RegisterShader("ui/assets/voiceWindow");
    // [QL] 0x10023696: the two icons in the scoreboard's first column
    cgs.media.scoreMutedShader = trap_R_RegisterShader("ui/assets/score/muted");
    cgs.media.scoreSpeakingShader = trap_R_RegisterShader("ui/assets/score/speaking");

    cgs.media.bloodExplosionShader = cgs.media.bloodSprayShaders[0];

    cgs.media.bulletFlashModel = trap_R_RegisterModel("models/weaphits/bullet.md3");
    cgs.media.ringFlashModel = trap_R_RegisterModel("models/weaphits/ring02.md3");
    cgs.media.dishFlashModel = trap_R_RegisterModel("models/weaphits/boom01.md3");
    cgs.media.teleportEffectModel = trap_R_RegisterModel("models/powerups/pop.md3");

    cgs.media.kamikazeEffectModel = trap_R_RegisterModel("models/weaphits/kamboom2.md3");
    cgs.media.kamikazeShockWave = trap_R_RegisterModel("models/weaphits/kamwave.md3");
    cgs.media.kamikazeHeadModel = trap_R_RegisterModel("models/powerups/kamikazi.md3");
    cgs.media.kamikazeHeadTrail = trap_R_RegisterModel("models/powerups/trailtest.md3");
    cgs.media.guardPowerupModel = trap_R_RegisterModel("models/powerups/guard_player.md3");
    cgs.media.scoutPowerupModel = trap_R_RegisterModel("models/powerups/scout_player.md3");
    cgs.media.doublerPowerupModel = trap_R_RegisterModel("models/powerups/doubler_player.md3");
    cgs.media.ammoRegenPowerupModel = trap_R_RegisterModel("models/powerups/ammo_player.md3");
    cgs.media.invulnerabilityImpactModel = trap_R_RegisterModel("models/powerups/shield/impact.md3");
    cgs.media.invulnerabilityJuicedModel = trap_R_RegisterModel("dlc_gibs/juicer.md3");  // [QL] moved from models/powerups/shield/
    cgs.media.medkitUsageModel = trap_R_RegisterModel("models/powerups/regen.md3");
    cgs.media.heartShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/selectedhealth.tga");
    cgs.media.invulnerabilityPowerupModel = trap_R_RegisterModel("models/powerups/shield/shield.md3");

    // [QL] 16 medal types (verified from binary CG_RegisterGraphics)
    cgs.media.medalAccuracy = trap_R_RegisterShaderNoMip("medal_accuracy");
    cgs.media.medalAssist = trap_R_RegisterShaderNoMip("medal_assist");
    cgs.media.medalCapture = trap_R_RegisterShaderNoMip("medal_capture");
    cgs.media.medalComboKill = trap_R_RegisterShaderNoMip("medal_combokill");
    cgs.media.medalDefend = trap_R_RegisterShaderNoMip("medal_defense");
    cgs.media.medalExcellent = trap_R_RegisterShaderNoMip("medal_excellent");
    cgs.media.medalFirstFrag = trap_R_RegisterShaderNoMip("medal_firstfrag");
    cgs.media.medalGauntlet = trap_R_RegisterShaderNoMip("medal_gauntlet");
    cgs.media.medalHeadshot = trap_R_RegisterShaderNoMip("medal_headshot");
    cgs.media.medalImpressive = trap_R_RegisterShaderNoMip("medal_impressive");

    // [CPMA] animation frames, CPMA 1.53's own, shipped in pak01; the checks keep a pak01
    // built without them working
    {
        static const char* const medalNames[6] = {"assist", "capture", "defend", "excellent", "gauntlet", "impressive"};
        fileHandle_t f;
        int m, n;

        if (trap_FS_FOpenFile("menu/medals/impressive/1.tga", &f, FS_READ) > 0) {
            trap_FS_FCloseFile(f);
            for (m = 0; m < 6; m++) {
                for (n = 0; n < 24; n++) {
                    cgs.media.medalFrames[m][n] = trap_R_RegisterShaderNoMip(va("menu/medals/%s/%d", medalNames[m], n + 1));
                }
            }
        }
        if (trap_FS_FOpenFile("sprites/chat01.tga", &f, FS_READ) > 0) {
            trap_FS_FCloseFile(f);
            for (n = 0; n < 16; n++) {
                cgs.media.chatBalloonFrames[n] = trap_R_RegisterShader(va("sprites/ioql_chat%02d", n + 1));
            }
        }

        // [ioquakelive] Handel Gothic Ex for the frag message (CG_DrawCenterString), shipped in
        // pak01. Without it the frag message falls back to Quake Live's Handel Gothic.
        cgs.media.fragMessageFont = 0;
        if (trap_FS_FOpenFile("fonts/handelgothicex.ttf", &f, FS_READ) > 0) {
            trap_FS_FCloseFile(f);
            cgs.media.fragMessageFont = trap_R_Font_Register("fonts/handelgothicex.ttf");
            if (cgs.media.fragMessageFont < 0) {
                cgs.media.fragMessageFont = 0;
            }
        }
    }
    cgs.media.medalMidair = trap_R_RegisterShaderNoMip("medal_midair");
    cgs.media.medalPerfect = trap_R_RegisterShaderNoMip("medal_perfect");
    cgs.media.medalPerforated = trap_R_RegisterShaderNoMip("medal_perforated");
    cgs.media.medalQuadGod = trap_R_RegisterShaderNoMip("medal_quadgod");
    cgs.media.medalRampage = trap_R_RegisterShaderNoMip("medal_rampage");
    cgs.media.medalRevenge = trap_R_RegisterShaderNoMip("medal_revenge");

    // [QL] 0x10024808: the scoreboards' pickup icons, in retail's order
    cgs.media.pickupRA = trap_R_RegisterShaderNoMip("pickup_RA");
    cgs.media.pickupYA = trap_R_RegisterShaderNoMip("pickup_YA");
    cgs.media.pickupGA = trap_R_RegisterShaderNoMip("pickup_GA");
    cgs.media.pickupMH = trap_R_RegisterShaderNoMip("pickup_MH");
    cgs.media.pickupMK = trap_R_RegisterShaderNoMip("pickup_MK");
    cgs.media.pickupBlueFlag = trap_R_RegisterShaderNoMip("pickup_BLUEFLAG");
    cgs.media.pickupNeutralFlag = trap_R_RegisterShaderNoMip("pickup_NTRLFLAG");
    cgs.media.pickupRedFlag = trap_R_RegisterShaderNoMip("pickup_REDFLAG");
    cgs.media.pickupQuad = trap_R_RegisterShaderNoMip("pickup_QUAD");
    cgs.media.pickupBS = trap_R_RegisterShaderNoMip("pickup_BS");
    cgs.media.pickupRegen = trap_R_RegisterShaderNoMip("pickup_REGEN");
    cgs.media.pickupHaste = trap_R_RegisterShaderNoMip("pickup_HASTE");
    cgs.media.pickupInvis = trap_R_RegisterShaderNoMip("pickup_INVIS");

    // [QL] 0x10024343. The Steam Workshop gore pack - the body-part gibs a
    // player can install if they want Quake 3's look back. Retail's pak00 ships
    // none of these files, so RE_RegisterModel hands back 0 for every one and
    // CG_GibPlayer takes the electrogib branch. That handle being zero *is*
    // retail's on/off switch; there is no cvar behind it. The assets sit
    // unshipped in content/optional/gore/.
    //
    // The binary keeps sixteen slots because it holds left and right variants
    // separately, but the duplicated pairs all point at the same file.
    cgs.media.gibSkull = trap_R_RegisterModel("dlc_gibs/skull.md3");
    cgs.media.gibBrain = trap_R_RegisterModel("dlc_gibs/brain.md3");
    cgs.media.gibChest = trap_R_RegisterModel("dlc_gibs/chest.md3");
    cgs.media.gibAbdomen = trap_R_RegisterModel("dlc_gibs/abdomen.md3");
    cgs.media.gibIntestine = trap_R_RegisterModel("dlc_gibs/intestine.md3");
    cgs.media.gibArm = trap_R_RegisterModel("dlc_gibs/arm.md3");
    cgs.media.gibForearm = trap_R_RegisterModel("dlc_gibs/forearm.md3");
    cgs.media.gibFist = trap_R_RegisterModel("dlc_gibs/fist.md3");
    cgs.media.gibLeg = trap_R_RegisterModel("dlc_gibs/leg.md3");
    cgs.media.gibFoot = trap_R_RegisterModel("dlc_gibs/foot.md3");

    // [QL] freeze tag
    cgs.media.iceShardModel = trap_R_RegisterModel("models/gibs/sphere.md3");
    // [QL] 0x100242b4 registers the same model separately as the no-gore
    // gib. Retail pak00 ships models/gibs/sphere.md3; it does not ship a
    // single dlc_gibs entry.
    cgs.media.gibSphere = cgs.media.iceShardModel;
    // [QL] 0x10023501. An animated additive sprite (deathSpike6..11.jpg,
    // sawtooth rgbGen) defined in retail scripts/gfx.shader - the electric
    // discharge that goes off where the body was.
    cgs.media.deathEffectShader = trap_R_RegisterShader("deathEffect");
    // [QL] 0x1002321e. The spark sprite that rises out of a gib.
    cgs.media.gibSparkShader = trap_R_RegisterShader("gfx/misc/tracer");
    // [QL] 0x100231a5: the trail left by the ice shards
    cgs.media.iceTrailShader = trap_R_RegisterShader("iceTrail");
    cgs.media.iceShardShader1 = trap_R_RegisterShader("powerups/ice1");
    cgs.media.iceShardShader2 = trap_R_RegisterShader("powerups/ice2");
    cgs.media.iceShardShader3 = trap_R_RegisterShader("powerups/ice3");
    cgs.media.frozenShader = trap_R_RegisterShader("sprites/frozen");
    cgs.media.iceMarkShader = trap_R_RegisterShader("iceMark");

    memset(cg_items, 0, sizeof(cg_items));
    memset(cg_weapons, 0, sizeof(cg_weapons));

    // only register the items that the server says we need
    Q_strncpyz(items, CG_ConfigString(CS_ITEMS), sizeof(items));

    for (i = 1; i < bg_numItems; i++) {
        if (items[i] == '1') {
            CG_LoadingItem(i);
            CG_RegisterItemVisuals(i);
        }
    }

    // wall marks
    cgs.media.bulletMarkShader = trap_R_RegisterShader("gfx/damage/bullet_mrk");
    // [QL] 0x10024a99: the mark the grapple leaves
    cgs.media.crackedMarkShader = trap_R_RegisterShader("gfx/damage/cracked_mrk");
    cgs.media.burnMarkShader = trap_R_RegisterShader("gfx/damage/burn_med_mrk");
    cgs.media.holeMarkShader = trap_R_RegisterShader("gfx/damage/hole_lg_mrk");
    cgs.media.energyMarkShader = trap_R_RegisterShader("gfx/damage/plasma_mrk");
    cgs.media.shadowMarkShader = trap_R_RegisterShader("markShadow");
    cgs.media.wakeMarkShader = trap_R_RegisterShader("wake");
    cgs.media.bloodMarkShader = cgs.media.bloodSprayShaders[0];

    // register the inline models
    cgs.numInlineModels = trap_CM_NumInlineModels();
    for (i = 1; i < cgs.numInlineModels; i++) {
        char name[10];
        vec3_t mins, maxs;
        int j;

        Com_sprintf(name, sizeof(name), "*%i", i);
        cgs.inlineDrawModel[i] = trap_R_RegisterModel(name);
        trap_R_ModelBounds(cgs.inlineDrawModel[i], mins, maxs);
        for (j = 0; j < 3; j++) {
            cgs.inlineModelMidpoints[i][j] = mins[j] + 0.5 * (maxs[j] - mins[j]);
        }
    }

    // register all the server specified models
    for (i = 1; i < MAX_MODELS; i++) {
        const char* modelName;

        modelName = CG_ConfigString(CS_MODELS + i);
        if (!modelName[0]) {
            break;
        }
        cgs.gameModels[i] = trap_R_RegisterModel(modelName);
    }

    // new stuff
    cgs.media.patrolShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/patrol.png");
    cgs.media.assaultShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/assault.png");
    cgs.media.campShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/camp.png");
    cgs.media.followShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/follow.png");
    cgs.media.defendShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/defend.png");
    cgs.media.teamLeaderShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/team_leader.png");
    cgs.media.retrieveShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/retrieve.png");
    cgs.media.escortShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/escort.png");
    cgs.media.cursor = trap_R_RegisterShaderNoMip("menu/art/3_cursor2");
    cgs.media.sizeCursor = trap_R_RegisterShaderNoMip("ui/assets/sizecursor.png");
    // [QL] cgamex86.dll registers "ui/assets/3_cursor3.tga" at 0x10024d0c.
    // "ui/assets/selectcursor.png" is in neither pak00 nor pak01, so this
    // handle used to resolve to the default shader - invisible in practice.
    // pak00 ships the file as .png; the renderer resolves the extension.
    cgs.media.selectCursor = trap_R_RegisterShaderNoMip("ui/assets/3_cursor3");
    cgs.media.flagShaders[0] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_in_base.tga");
    cgs.media.flagShaders[1] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_capture.tga");
    cgs.media.flagShaders[2] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_missing.tga");

    // [QL] flag-status HUD icon set for CG_DrawFlagStatus / CG_DrawOneFlagStatus.
    // Indexed [icon + team*4]: team 0 neutral, 1 red, 2 blue; icon 0 at-base, 1 taken,
    // 2 dropped, 3 stolen. (binary CG_RegisterGraphics, store base DAT_10a5fc2c @0x10023a4f.)
    {
        static const char* flagStatusShaders[12] = {
            "gfx/2d/flag_status/flag_at_base.tga",
            "gfx/2d/flag_status/flag_taken.tga",
            "gfx/2d/flag_status/flag_dropped.tga",
            "gfx/2d/flag_status/flag_stolen.tga",
            "gfx/2d/flag_status/red_flag_at_base.tga",
            "gfx/2d/flag_status/red_flag_taken.tga",
            "gfx/2d/flag_status/red_flag_dropped.tga",
            "gfx/2d/flag_status/red_flag_stolen.tga",
            "gfx/2d/flag_status/blue_flag_at_base.tga",
            "gfx/2d/flag_status/blue_flag_taken.tga",
            "gfx/2d/flag_status/blue_flag_dropped.tga",
            "gfx/2d/flag_status/blue_flag_stolen.tga",
        };
        int fi;
        for (fi = 0; fi < 12; fi++) {
            cgs.media.flagStatusHandles[fi] = trap_R_RegisterShaderNoMip(flagStatusShaders[fi]);
        }
    }

    // [QL] the follow pointer is kept apart from the set of twelve icons
    cgs.media.flagTrackPointerShader = trap_R_RegisterShaderNoMip("gfx/2d/flag_status/track_pointer.tga");

    // [QL] head models are in the player directory, not a separate heads/ directory
    trap_R_RegisterModel("models/players/james/lower.md3");
    trap_R_RegisterModel("models/players/james/upper.md3");
    trap_R_RegisterModel("models/players/james/head.md3");

    trap_R_RegisterModel("models/players/janet/lower.md3");
    trap_R_RegisterModel("models/players/janet/upper.md3");
    trap_R_RegisterModel("models/players/janet/head.md3");

    CG_ClearParticles();
    /*
        for (i=1; i<MAX_PARTICLES_AREAS; i++)
        {
            {
                int rval;

                rval = CG_NewParticleArea ( CS_PARTICLES + i);
                if (!rval)
                    break;
            }
        }
    */
}

/*
=======================
CG_CompareSpectators

[QL] 0x10024f90: the spectators who only watch last, the rest by their place in the play
queue, so the ones waiting to play read in turn
=======================
*/
static int QDECL CG_CompareSpectators(const void* a, const void* b) {
    const clientInfo_t* ca = &cgs.clientinfo[*(const int*)a];
    const clientInfo_t* cb = &cgs.clientinfo[*(const int*)b];

    if (ca->specOnly != cb->specOnly) {
        return (ca->specOnly > cb->specOnly) ? 1 : -1;
    }
    if (ca->playQueue > cb->playQueue) {
        return 1;
    }
    return (ca->playQueue == cb->playQueue) ? 0 : -1;
}

/*
=======================
CG_BuildSpectatorString

[QL] 0x10024ff0, the spectator list the scoreboards scroll along their bottom: one entry per
spectator, in the order above. In duel the entry opens with the spectator's place in the
play queue, renumbered so the list counts from one, and with "(s)" for a spectator who only
ever watches. Retail strips the colours from the stored name, so a spectator reads plain
everywhere until their next configstring.
=======================
*/
void CG_BuildSpectatorString(void) {
    int list[MAX_CLIENTS];
    int count = 0;
    int queued = 0;
    int i, place;
    clientInfo_t* ci;
    const char* prefix;
    char* in;
    char* out;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_SPECTATOR) {
            list[count++] = i;
        }
    }
    qsort(list, count, sizeof(list[0]), CG_CompareSpectators);

    memset(cg.notifyMessages, 0, sizeof(cg.notifyMessages));
    for (i = 0; i < count; i++) {
        ci = &cgs.clientinfo[list[i]];

        prefix = "";
        if (cgs.gametype == GT_DUEL) {
            if (ci->specOnly) {
                prefix = "^7(^5s^7)";
            } else if (ci->playQueue) {
                place = ci->playQueue;
                if (queued != place - 1) {
                    place = queued + 1;
                }
                prefix = va("(%d)", place);
                queued++;
            }
        }

        // the colours and the control characters go, in the name itself
        out = ci->name;
        for (in = ci->name; *in; in++) {
            if (*in == Q_COLOR_ESCAPE && in[1] && in[1] >= '0' && in[1] <= '7') {
                in++;
            } else if ((unsigned char)*in >= ' ') {
                *out++ = *in;
            }
        }
        *out = '\0';

        Q_strncpyz(cg.notifyMessages[i], va("%s%s", prefix, ci->name), sizeof(cg.notifyMessages[i]));
    }

    if (count != cg.notifyCount) {
        cg.notifyCount = count;
        if (count < cg.notifyScrollStart) {
            cg.notifyScrollStart = 0;
        }
    }
}

/*
===================
CG_RegisterClients
===================
*/
static void CG_RegisterClients(void) {
    int i;

    CG_LoadingClient(cg.clientNum);
    CG_NewClientInfo(cg.clientNum);

    for (i = 0; i < MAX_CLIENTS; i++) {
        const char* clientInfo;

        if (cg.clientNum == i) {
            continue;
        }

        clientInfo = CG_ConfigString(CS_PLAYERS + i);
        if (!clientInfo[0]) {
            continue;
        }
        CG_LoadingClient(i);
        CG_NewClientInfo(i);
    }
    CG_BuildSpectatorString();
}

//===========================================================================

/*
=================
CG_ConfigString
=================
*/
const char* CG_ConfigString(int index) {
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        CG_Error("CG_ConfigString: bad index: %i", index);
    }
    return cgs.gameState.stringData + cgs.gameState.stringOffsets[index];
}

//==================================================================

/*
======================
CG_StartMusic

======================
*/
void CG_StartMusic(void) {
    char* s;
    char parm1[MAX_QPATH], parm2[MAX_QPATH];

    // start the background music
    s = (char*)CG_ConfigString(CS_MUSIC);
    Q_strncpyz(parm1, COM_Parse(&s), sizeof(parm1));
    Q_strncpyz(parm2, COM_Parse(&s), sizeof(parm2));

    trap_S_StartBackgroundTrack(parm1, parm2);
}

char* CG_GetMenuBuffer(const char* filename) {
    int len;
    fileHandle_t f;
    static char buf[MAX_MENUFILE];

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        trap_Print(va(S_COLOR_RED "menu file not found: %s, using default\n", filename));
        return NULL;
    }
    if (len >= MAX_MENUFILE) {
        trap_Print(va(S_COLOR_RED "menu file too large: %s is %i, max allowed is %i\n", filename, len, MAX_MENUFILE));
        trap_FS_FCloseFile(f);
        return NULL;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);

    return buf;
}

//
// ==============================
// new hud stuff ( mission pack )
// ==============================
//
qboolean CG_Asset_Parse(int handle) {
    pc_token_t token;
    const char* tempStr;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;
    if (Q_stricmp(token.string, "{") != 0) {
        return qfalse;
    }

    while (1) {
        if (!trap_PC_ReadToken(handle, &token))
            return qfalse;

        if (Q_stricmp(token.string, "}") == 0) {
            return qtrue;
        }

        // font - parse but skip registerFont if already registered by CG_Init
        if (Q_stricmp(token.string, "font") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            if (!cgDC.Assets.fontRegistered) {
                cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.textFont);
            }
            continue;
        }

        // smallFont
        if (Q_stricmp(token.string, "smallFont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            if (!cgDC.Assets.fontRegistered) {
                cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.smallFont);
            }
            continue;
        }

        // bigFont
        if (Q_stricmp(token.string, "bigfont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            if (!cgDC.Assets.fontRegistered) {
                cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.bigFont);
            }
            continue;
        }

        // gradientbar
        if (Q_stricmp(token.string, "gradientbar") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
            continue;
        }

        // enterMenuSound
        if (Q_stricmp(token.string, "menuEnterSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.menuEnterSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // exitMenuSound
        if (Q_stricmp(token.string, "menuExitSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.menuExitSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // itemFocusSound
        if (Q_stricmp(token.string, "itemFocusSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.itemFocusSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // menuBuzzSound
        if (Q_stricmp(token.string, "menuBuzzSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.menuBuzzSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "cursor") == 0) {
            if (!PC_String_Parse(handle, &cgDC.Assets.cursorStr)) {
                return qfalse;
            }
            cgDC.Assets.cursor = trap_R_RegisterShaderNoMip(cgDC.Assets.cursorStr);
            continue;
        }

        if (Q_stricmp(token.string, "fadeClamp") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.fadeClamp)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeCycle") == 0) {
            if (!PC_Int_Parse(handle, &cgDC.Assets.fadeCycle)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeAmount") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.fadeAmount)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowX") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.shadowX)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowY") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.shadowY)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowColor") == 0) {
            if (!PC_Color_Parse(handle, &cgDC.Assets.shadowColor)) {
                return qfalse;
            }
            cgDC.Assets.shadowFadeClamp = cgDC.Assets.shadowColor[3];
            continue;
        }
    }
    return qfalse;
}

void CG_ParseMenu(const char* menuFile) {
    pc_token_t token;
    int handle;

    handle = trap_PC_LoadSource(menuFile);
    if (!handle)
        handle = trap_PC_LoadSource("ui/testhud.menu");
    if (!handle)
        return;

    while (1) {
        if (!trap_PC_ReadToken(handle, &token)) {
            break;
        }

        // if ( Q_stricmp( token, "{" ) ) {
        //	Com_Printf( "Missing { in menu file\n" );
        //	break;
        // }

        // if ( menuCount == MAX_MENUS ) {
        //	Com_Printf( "Too many menus!\n" );
        //	break;
        // }

        if (token.string[0] == '}') {
            break;
        }

        if (Q_stricmp(token.string, "assetGlobalDef") == 0) {
            if (CG_Asset_Parse(handle)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token.string, "menudef") == 0) {
            // start a new menu
            Menu_New(handle);
        }
    }
    trap_PC_FreeSource(handle);
}

qboolean CG_Load_Menu(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        CG_ParseMenu(token);
    }
    return qfalse;
}

void CG_LoadMenus(const char* menuFile) {
    char* token;
    char* p;
    int len, start;
    fileHandle_t f;
    static char buf[MAX_MENUDEFFILE];

    start = trap_Milliseconds();

    len = trap_FS_FOpenFile(menuFile, &f, FS_READ);
    if (!f) {
        Com_Printf(S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile);
        len = trap_FS_FOpenFile("ui/hud.txt", &f, FS_READ);
        if (!f) {
            CG_Error(S_COLOR_RED "default menu file not found: ui/hud.txt, unable to continue!");
        }
    }

    if (len >= MAX_MENUDEFFILE) {
        trap_FS_FCloseFile(f);
        CG_Error(S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", menuFile, len, MAX_MENUDEFFILE);
        return;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);

    COM_Compress(buf);

    Menu_Reset();

    p = buf;

    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0]) {
            break;
        }

        // if ( Q_stricmp( token, "{" ) ) {
        //	Com_Printf( "Missing { in menu file\n" );
        //	break;
        // }

        // if ( menuCount == MAX_MENUS ) {
        //	Com_Printf( "Too many menus!\n" );
        //	break;
        // }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "loadmenu") == 0) {
            if (CG_Load_Menu(&p)) {
                continue;
            } else {
                break;
            }
        }
    }

    Com_Printf("UI menu load time = %d milliseconds\n", trap_Milliseconds() - start);
}

static qboolean CG_OwnerDrawHandleKey(int ownerDraw, int flags, float* special, int key) {
    return qfalse;
}

static int CG_FeederCount(float feederID) {
    int i, count;
    count = 0;
    if (feederID == FEEDER_REDTEAM_LIST) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_RED) {
                count++;
            }
        }
    } else if (feederID == FEEDER_BLUETEAM_LIST) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_BLUE) {
                count++;
            }
        }
    } else if (feederID == FEEDER_SCOREBOARD || feederID == FEEDER_ENDSCOREBOARD) {
        return cg.numScores;
    } else if (feederID == FEEDER_REDTEAM_STATS) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_RED) count++;
        }
    } else if (feederID == FEEDER_BLUETEAM_STATS) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_BLUE) count++;
        }
    }
    return count;
}

/*
=================
CG_SetScoreSelection

[QL] 0x10025f60 (the menu arrives in edi). A spectator selects no score (-1); anyone
else selects the score whose client is the snapshot's player, and keeps the previous
selection while that client is missing from the scores.

Team gametypes hand that score's index to the list of the player's team and -1 to
the other list. Other gametypes select on FEEDER_SCOREBOARD.
=================
*/
void CG_SetScoreSelection(void* p) {
    menuDef_t* menu = (menuDef_t*)p;
    playerState_t* ps = &cg.snap->ps;
    int i;

    if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        cg.selectedScore = -1;
    } else {
        for (i = 0; i < cg.numScores; i++) {
            if (ps->clientNum == cg.scores[i].client) {
                cg.selectedScore = i;
                break;
            }
        }
    }

    if (menu == NULL) {
        // just interested in setting the selected score
        return;
    }

    if (cgs.gametype >= GT_TEAM) {
        int feeder = FEEDER_REDTEAM_LIST;
        int other = FEEDER_BLUETEAM_LIST;

        // retail reads cg.scores[-1] for a spectator; here no score is blue then
        if (cg.selectedScore >= 0 && cg.scores[cg.selectedScore].team == TEAM_BLUE) {
            feeder = FEEDER_BLUETEAM_LIST;
            other = FEEDER_REDTEAM_LIST;
        }
        Menu_SetFeederSelection(menu, feeder, cg.selectedScore, NULL);
        Menu_SetFeederSelection(menu, other, -1, NULL);
    } else {
        Menu_SetFeederSelection(menu, FEEDER_SCOREBOARD, cg.selectedScore, NULL);
    }
}

extern menuDef_t* menuScoreboard;
extern menuDef_t* menuEndScoreboard;

/*
=================
CG_SetScoreboardSelection

[QL] What every score parser in the binary ends with (0x1004541d, 0x10045d7a, ...): the
selection goes to the end-of-match board at intermission and to the in-game board otherwise.
=================
*/
void CG_SetScoreboardSelection(void) {
    if (!cg.snap) {
        return;
    }
    CG_SetScoreSelection(cg.snap->ps.pm_type == PM_INTERMISSION ? menuEndScoreboard : menuScoreboard);
}

// FIXME: might need to cache this info
static clientInfo_t* CG_InfoFromScoreIndex(int index, int team, int* scoreIndex) {
    int i, count;
    if (cgs.gametype >= GT_TEAM) {
        count = 0;
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == team) {
                if (count == index) {
                    *scoreIndex = i;
                    return &cgs.clientinfo[cg.scores[i].client];
                }
                count++;
            }
        }
    }
    *scoreIndex = index;
    return &cgs.clientinfo[cg.scores[index].client];
}

// [QL] Format damage with "k" suffix for scoreboard display (binary-verified)
static const char* CG_FormatDamage(int dmg) {
    if (dmg >= 10000) return va("%.0fk", (float)dmg / 1000.0f);
    if (dmg >= 1000) return va("%.1fk", (float)dmg / 1000.0f);
    return va("%d", dmg);
}

// [QL] FFA/Duel/RR scoreboard columns 6-13 (14 total, binary FUN_10026160)
static const char* CG_FeederColumnsFfa(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            return "";
        case 7:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 8:
            return va("%d/%d", sp->frags, sp->deaths);
        case 9:
            return CG_FormatDamage(sp->damageDone);
        case 10:
            if (sp->damageDone > 0 && sp->bestWeapon > 0 && sp->bestWeapon < WP_NUM_WEAPONS)
                *handle = cg_weapons[sp->bestWeapon].weaponIcon;
            return "";
        case 11:
            if (sp->damageDone > 0 && sp->bestWeapon > 0)
                return va("%d%%", sp->bestWeaponAccuracy);
            return "";
        case 12:
            return va("%d", sp->time);
        case 13:
            if (sp->ping == -1) return "connecting";
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] TDM columns 6-9 (10 total), FT columns 6-9 (10 total) - binary FUN_10027e30
static const char* CG_FeederColumnsTdm(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 7:
            if (cgs.gametype == GT_FREEZE)
                return va("%d/%d", sp->frags, sp->deaths);
            return va("%d", sp->net);
        case 8:
            if (cgs.gametype == GT_FREEZE)
                return va("%d", sp->thaws);
            return CG_FormatDamage(sp->damageDone);
        case 9:
            if (sp->ping == -1) return "connecting";
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] CA columns 6-11 (12 total) - binary FUN_10027240
static const char* CG_FeederColumnsCa(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 7:
            return va("%d/%d", sp->frags, sp->deaths);
        case 8:
            return CG_FormatDamage(sp->damageDone);
        case 9:
            if (sp->damageDone > 0 && sp->bestWeapon > 0 && sp->bestWeapon < WP_NUM_WEAPONS)
                *handle = cg_weapons[sp->bestWeapon].weaponIcon;
            return "";
        case 10:
            if (sp->damageDone > 0 && sp->bestWeapon > 0)
                return va("%d%%", sp->bestWeaponAccuracy);
            return "";
        case 11:
            if (sp->ping == -1) return "connecting";
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] CTF/1FCTF/HAR/DOM/AD columns 6-11 (12 total) - binary FUN_100268b0
static const char* CG_FeederColumnsCtf(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 7:
            return va("%d/%d", sp->frags, sp->deaths);
        case 8:
            return va("%d", sp->captures);
        case 9:
            return va("%d", sp->assistCount);
        case 10:
            return va("%d", sp->defendCount);
        case 11:
            if (sp->ping == -1) return "connecting";
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] Race scoreboard columns 0-6 (7 total, different layout) - binary FUN_10027c40
static const char* CG_FeederItemTextRace(clientInfo_t* info, score_t* sp, int column, qhandle_t* handle) {
    switch (column) {
        case 0:
            *handle = info->modelIcon;
            return "";
        case 1:
            return "";
        case 2:
            if (cg.warmup) {
                *handle = (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << sp->client))
                    ? trap_R_RegisterShader("ui/assets/score/arrowg")
                    : trap_R_RegisterShader("ui/assets/score/arrowr");
            }
            return "";
        case 3:
            return info->name;
        case 4:
            if (sp->score > 0 && sp->score != 0x7FFFFFFF)
                return va("%d:%02d.%03d", sp->score / 60000, (sp->score / 1000) % 60, sp->score % 1000);
            return "";
        case 5:
            return va("%d", sp->time);
        case 6:
            if (sp->ping == -1) return "connecting";
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] Stats feeder - returns available data, stubs for unavailable per-weapon/item stats
// Binary has separate functions per gametype: FUN_10028300 (TDM/FT), CG_FeederItemText (CA), FUN_10026cf0 (CTF)
static const char* CG_FeederItemTextStats(float feederID, int index, int column, qhandle_t* handle) {
    int scoreIndex = 0;
    int team = (feederID == FEEDER_REDTEAM_STATS) ? TEAM_RED : TEAM_BLUE;
    clientInfo_t* info = CG_InfoFromScoreIndex(index, team, &scoreIndex);
    score_t* sp = &cg.scores[scoreIndex];

    if (!info || !info->infoValid) return "";

    switch (cgs.gametype) {
        case GT_CA:
            // CA stats: 17 columns
            switch (column) {
                case 0: return info->name;
                case 1: return va("%d", sp->score);
                case 2: return va("%d", sp->frags);
                case 3: return va("%d", sp->deaths);
                case 4: return CG_FormatDamage(sp->damageDone);
                case 5: return "";
                case 6: return va("%d%%", sp->accuracy);
                case 16: return va("%d", sp->time);
            }
            return "";
        case GT_TEAM:
        case GT_FREEZE:
            // TDM/FT stats: 18 columns
            switch (column) {
                case 0: return info->name;
                case 1: return va("%d", sp->score);
                case 2: return va("%d", sp->frags);
                case 3: return va("%d", sp->deaths);
                case 4: return "";
                case 5: return va("%d", sp->tks);
                case 6: return va("%d", sp->tkd);
                case 7: return va("%d", sp->net);
                case 8: return CG_FormatDamage(sp->damageDone);
                case 9: return "";
                case 10: return va("%d%%", sp->accuracy);
                case 17: return va("%d", sp->time);
            }
            return "";
        default:
            // CTF/etc stats: 19 columns
            switch (column) {
                case 0: return info->name;
                case 1: return va("%d", sp->score);
                case 2: return va("%d", sp->frags);
                case 3: return va("%d", sp->deaths);
                case 4: return "";
                case 5: return va("%d", sp->net);
                case 6: return CG_FormatDamage(sp->damageDone);
                case 7: return "";
                case 8: return va("%d%%", sp->accuracy);
                case 18: return va("%d", sp->time);
            }
            return "";
    }
}

// [QL] Main feeder dispatcher - dispatches to per-gametype functions
// Binary: FUN_10028830 dispatches by feederID + gametype
static const char* CG_FeederItemText(float feederID, int index, int column, qhandle_t* handle) {
    int scoreIndex = 0;
    clientInfo_t* info = NULL;
    int team = -1;
    score_t* sp = NULL;

    *handle = -1;

    // Stats feeders have completely separate column layouts
    if (feederID == FEEDER_REDTEAM_STATS || feederID == FEEDER_BLUETEAM_STATS) {
        return CG_FeederItemTextStats(feederID, index, column, handle);
    }

    if (feederID == FEEDER_REDTEAM_LIST) {
        team = TEAM_RED;
    } else if (feederID == FEEDER_BLUETEAM_LIST) {
        team = TEAM_BLUE;
    }

    info = CG_InfoFromScoreIndex(index, team, &scoreIndex);
    sp = &cg.scores[scoreIndex];

    if (!info || !info->infoValid) return "";

    // Race scoreboard has unique column layout (7 columns, name at col 3)
    if (cgs.gametype == GT_RACE && team == -1) {
        return CG_FeederItemTextRace(info, sp, column, handle);
    }

    // Common icon columns 0-5 (all non-race gametypes)
    switch (column) {
        case 0:
            *handle = info->modelIcon;
            return "";
        // [QL] 0x100262b7: muted first, and if not muted, talking
        case 1:
            if (info->steamId && trap_VoiceIsIgnored(info->steamId)) {
                *handle = cgs.media.scoreMutedShader;
            } else if (info->talking) {
                *handle = cgs.media.scoreSpeakingShader;
            }
            return "";
        case 2:
            return "";
        case 3:
            if (cg.warmup) {
                *handle = (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << sp->client))
                    ? trap_R_RegisterShader("ui/assets/score/arrowg")
                    : trap_R_RegisterShader("ui/assets/score/arrowr");
            } else if (info->team == TEAM_RED) {
                *handle = trap_R_RegisterShader("ui/assets/score/ca_arrow_red");
            } else if (info->team == TEAM_BLUE) {
                *handle = trap_R_RegisterShader("ui/assets/score/ca_arrow_blue");
            }
            return "";
        // [QL] 0x10026381, and at column 4 of every scoreboard: the country flag
        case 4:
            *handle = CG_CountryFlagShader(info->country);
            return "";
        case 5:
            return info->name;
    }

    // Gametype-specific columns 6+ for team feeders
    if (team != -1) {
        switch (cgs.gametype) {
            case GT_TEAM:
            case GT_FREEZE:
                return CG_FeederColumnsTdm(sp, info, column, handle);
            case GT_CA:
                return CG_FeederColumnsCa(sp, info, column, handle);
            case GT_CTF:
            case GT_1FCTF:
            case GT_HARVESTER:
            case GT_DOMINATION:
            case GT_AD:
                return CG_FeederColumnsCtf(sp, info, column, handle);
            default:
                return CG_FeederColumnsFfa(sp, info, column, handle);
        }
    }

    // Non-team feeders (FEEDER_SCOREBOARD, FEEDER_ENDSCOREBOARD) use FFA layout
    return CG_FeederColumnsFfa(sp, info, column, handle);
}

static qhandle_t CG_FeederItemImage(float feederID, int index) {
    // [QL] Return country flag or other per-player image for premium scoreboard.
    // Standard scoreboard uses column 0 icon handle from CG_FeederItemText instead.
    // FEEDER_ENDSCOREBOARD could show country flags here if we had the data.
    return 0;
}

/*
=================
CG_FeederSelection

[QL] 0x10028b10. -1 is ignored, and outside the team gametypes the index is the
score.

In a team gametype FEEDER_REDTEAM_LIST means red and every other feeder blue. At
intermission the index is a row of that team's list. Otherwise it arrives as an
index into cg.scores and becomes the player's row on the team's list, written into
playerlistRED / playerlistBLUE, while the end board's other team is cleared.
=================
*/
static void CG_FeederSelection(float feederID, int index) {
    itemDef_t* list;
    itemDef_t* other;
    qboolean intermission;
    int team, i, count;

    if (index == -1) {
        return;
    }

    if (cgs.gametype < GT_TEAM) {
        cg.selectedScore = index;
        return;
    }

    team = (feederID == FEEDER_REDTEAM_LIST) ? TEAM_RED : TEAM_BLUE;
    intermission = (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) ? qtrue : qfalse;

    count = 0;
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team != team) {
            continue;
        }
        if (intermission) {
            if (index == count) {
                cg.selectedScore = i;
            }
        } else if (index >= 0 && index < MAX_CLIENTS && cg.scores[i].client == cg.scores[index].client) {
            index = count;
        }
        count++;
    }

    list = Menu_FindItemByName(menuScoreboard, (team == TEAM_BLUE) ? "playerlistBLUE" : "playerlistRED");
    other = Menu_FindItemByName(menuEndScoreboard, (team == TEAM_BLUE) ? "playerlistRED" : "playerlistBLUE");
    if (list && !intermission) {
        list->cursorPos = index;
    }
    if (other) {
        other->cursorPos = -1;
    }
}

static float CG_Cvar_Get(const char* cvar) {
    char buff[128];
    memset(buff, 0, sizeof(buff));
    trap_Cvar_VariableStringBuffer(cvar, buff, sizeof(buff));
    return atof(buff);
}

void CG_Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style) {
    CG_Text_Paint(x, y, scale, color, text, 0, limit, style);
}

// [QL] cgamex86.dll 0x10028d80: the texts are measured in font 0, the font their drawers use
static int CG_OwnerDrawWidth(int ownerDraw, float scale) {
    switch (ownerDraw) {
        case CG_GAME_TYPE:
            return CG_DrawTextWidth(CG_GameTypeString(), scale, 0, 0);
        case CG_GAME_STATUS:
            return CG_DrawTextWidth(CG_GetGameStatusText(), scale, 0, 0);
        case CG_MATCH_STATUS:
            return CG_DrawTextWidth(CG_GetMatchStatusText(), scale, 0, 0);
        case CG_KILLER:
            return CG_DrawTextWidth(CG_GetKillerText(), scale, 0, 0);
    }
    return 0;
}

static int CG_PlayCinematic(const char* name, float x, float y, float w, float h) {
    return trap_CIN_PlayCinematic(name, x, y, w, h, CIN_loop);
}

static void CG_StopCinematic(int handle) {
    trap_CIN_StopCinematic(handle);
}

static void CG_DrawCinematic(int handle, float x, float y, float w, float h) {
    trap_CIN_SetExtents(handle, x, y, w, h);
    trap_CIN_DrawCinematic(handle);
}

static void CG_RunCinematicFrame(int handle) {
    trap_CIN_RunCinematic(handle);
}

/*
=================
CG_LoadHudMenu();

=================
*/
/*
=================
Display-context shims for cgame

ui_shared.c is compiled into both modules and calls these through DC->. The UI module
wires them to Key_ and Cmd_ traps that cgame's syscall table does not export, so they
used to be left out - six NULL pointers waiting for a menu script. Script_Exec found
them: `exec` is the first command in the join menu's nav bar actions, so clicking
Settings called DC->executeText(NULL).

executeText has a real equivalent here, trap_SendConsoleCommand. The rest drive key
binding and text-field editing, which no cgame-owned menu uses, so they are honest
no-ops: what matters is that the pointers are never NULL.
=================
*/
static void CG_DC_ExecuteText(int exec_when, const char* text) {
    // cgame only ever gets EXEC_APPEND from ui_shared's Script_Exec, which is
    // what trap_SendConsoleCommand does.
    (void)exec_when;
    trap_SendConsoleCommand(text);
}

static void CG_DC_SetOverstrikeMode(qboolean b) {
    (void)b;
}

static qboolean CG_DC_GetOverstrikeMode(void) {
    return qfalse;
}

static void CG_DC_SetBinding(int keynum, const char* binding) {
    (void)keynum;
    (void)binding;
}

static void CG_DC_GetBindingBuf(int keynum, char* buf, int buflen) {
    (void)keynum;
    if (buf && buflen > 0) {
        buf[0] = 0;
    }
}

static void CG_DC_Pause(qboolean b) {
    (void)b;
}

void CG_LoadHudMenu(void) {
    char buff[1024];
    const char* hudSet;

    cgDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
    cgDC.setColor = &trap_R_SetColor;
    cgDC.drawHandlePic = &CG_DrawPic;
    cgDC.drawStretchPic = &trap_R_DrawStretchPic;
    cgDC.drawText = &CG_DrawText_DC;
    cgDC.textWidth = &CG_TextWidth_DC;
    cgDC.textHeight = &CG_TextHeight_DC;
    cgDC.registerModel = &trap_R_RegisterModel;
    cgDC.modelBounds = &trap_R_ModelBounds;
    cgDC.fillRect = &CG_FillRect;
    cgDC.drawRect = &CG_DrawRect;
    cgDC.drawSides = &CG_DrawSides;
    cgDC.drawTopBottom = &CG_DrawTopBottom;
    cgDC.clearScene = &trap_R_ClearScene;
    cgDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
    cgDC.renderScene = &trap_R_RenderScene;
    cgDC.registerFont = &trap_R_RegisterFont;
    cgDC.ownerDrawItem = &CG_OwnerDraw;
    cgDC.getValue = &CG_GetValue;
    cgDC.ownerDrawVisible = &CG_OwnerDrawVisible;
    cgDC.runScript = &CG_RunMenuScript;
    cgDC.getTeamColor = &CG_GetTeamColor;
    cgDC.setCVar = trap_Cvar_Set;
    cgDC.getCVarString = trap_Cvar_VariableStringBuffer;
    cgDC.getCVarValue = CG_Cvar_Get;
    cgDC.drawTextWithCursor = &CG_DrawTextWithCursor_DC;
    cgDC.setOverstrikeMode = &CG_DC_SetOverstrikeMode;
    cgDC.getOverstrikeMode = &CG_DC_GetOverstrikeMode;
    cgDC.startLocalSound = &trap_S_StartLocalSound;
    cgDC.ownerDrawHandleKey = &CG_OwnerDrawHandleKey;
    cgDC.feederCount = &CG_FeederCount;
    cgDC.feederItemImage = &CG_FeederItemImage;
    cgDC.feederItemText = &CG_FeederItemText;
    cgDC.feederSelection = &CG_FeederSelection;
    cgDC.setBinding = &CG_DC_SetBinding;
    cgDC.getBindingBuf = &CG_DC_GetBindingBuf;
    cgDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
    cgDC.executeText = &CG_DC_ExecuteText;
    cgDC.Error = &Com_Error;
    cgDC.Print = &Com_Printf;
    cgDC.ownerDrawWidth = &CG_OwnerDrawWidth;
    cgDC.Pause = &CG_DC_Pause;
    cgDC.registerSound = &trap_S_RegisterSound;
    cgDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
    cgDC.stopBackgroundTrack = &trap_S_StopBackgroundTrack;
    cgDC.playCinematic = &CG_PlayCinematic;
    cgDC.stopCinematic = &CG_StopCinematic;
    cgDC.drawCinematic = &CG_DrawCinematic;
    cgDC.runCinematicFrame = &CG_RunCinematicFrame;
    cgDC.setWidescreen = &CG_SetWidescreen;

    Init_Display(&cgDC);

    Menu_Reset();

    trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));
    hudSet = buff;
    if (hudSet[0] == '\0') {
        hudSet = "ui/hud.txt";
    }

    // [QL] Load hud.txt first - CG_LoadMenus calls Menu_Reset() internally,
    // so all CG_ParseMenu calls must come AFTER to avoid being wiped.
    CG_LoadMenus(hudSet);

    // [QL] Static table menus (binary-verified order)
    CG_ParseMenu("ui/intro.menu");
    CG_ParseMenu("ui/ingamescoreteam.menu");
    CG_ParseMenu("ui/ingamescorenoteam.menu");
    CG_ParseMenu("ui/endscoreteam.menu");
    CG_ParseMenu("ui/endscorenoteam.menu");
    CG_ParseMenu("ui/spectator.menu");
    CG_ParseMenu("ui/spectator_follow.menu");
    CG_ParseMenu("ui/comp_spectator.menu");
    CG_ParseMenu("ui/comp_spectator_follow.menu");
    CG_ParseMenu("ui/ingamestats.menu");

    // [QL] Per-gametype in-game scoreboard menus
    CG_ParseMenu("ui/ingame_scoreboard_ffa.menu");
    CG_ParseMenu("ui/ingame_scoreboard_duel.menu");
    CG_ParseMenu("ui/ingame_scoreboard_race.menu");
    CG_ParseMenu("ui/ingame_scoreboard_tdm.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ca.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ctf.menu");
    CG_ParseMenu("ui/ingame_scoreboard_1fctf.menu");
    CG_ParseMenu("ui/ingame_scoreboard_har.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ft.menu");
    CG_ParseMenu("ui/ingame_scoreboard_dom.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ad.menu");
    CG_ParseMenu("ui/ingame_scoreboard_rr.menu");

    // [QL] Per-gametype end-of-game scoreboard menus
    CG_ParseMenu("ui/end_scoreboard_ffa.menu");
    CG_ParseMenu("ui/end_scoreboard_duel.menu");
    CG_ParseMenu("ui/end_scoreboard_race.menu");
    CG_ParseMenu("ui/end_scoreboard_tdm.menu");
    CG_ParseMenu("ui/end_scoreboard_ca.menu");
    CG_ParseMenu("ui/end_scoreboard_ctf.menu");
    CG_ParseMenu("ui/end_scoreboard_1fctf.menu");
    CG_ParseMenu("ui/end_scoreboard_har.menu");
    CG_ParseMenu("ui/end_scoreboard_ft.menu");
    CG_ParseMenu("ui/end_scoreboard_dom.menu");
    CG_ParseMenu("ui/end_scoreboard_ad.menu");
    CG_ParseMenu("ui/end_scoreboard_rr.menu");

    // [QL] resolve every menu cgame keeps a pointer to, right after loading them, the
    // way the binary does at 0x10006eb8. Doing it here rather than lazily on first use
    // means a pointer can never outlive the Menus[] slot it came from.
    CG_CacheMenus();
}

void CG_AssetCache(void) {
    cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(ASSET_GRADIENTBAR);
    cgDC.Assets.fxBasePic = trap_R_RegisterShaderNoMip(ART_FX_BASE);
    cgDC.Assets.fxPic[0] = trap_R_RegisterShaderNoMip(ART_FX_RED);
    cgDC.Assets.fxPic[1] = trap_R_RegisterShaderNoMip(ART_FX_YELLOW);
    cgDC.Assets.fxPic[2] = trap_R_RegisterShaderNoMip(ART_FX_GREEN);
    cgDC.Assets.fxPic[3] = trap_R_RegisterShaderNoMip(ART_FX_TEAL);
    cgDC.Assets.fxPic[4] = trap_R_RegisterShaderNoMip(ART_FX_BLUE);
    cgDC.Assets.fxPic[5] = trap_R_RegisterShaderNoMip(ART_FX_CYAN);
    cgDC.Assets.fxPic[6] = trap_R_RegisterShaderNoMip(ART_FX_WHITE);
    cgDC.Assets.scrollBar = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR);
    cgDC.Assets.scrollBarArrowDown = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
    cgDC.Assets.scrollBarArrowUp = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
    cgDC.Assets.scrollBarArrowLeft = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
    cgDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
    cgDC.Assets.scrollBarThumb = trap_R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);
    cgDC.Assets.sliderBar = trap_R_RegisterShaderNoMip(ASSET_SLIDER_BAR);
    cgDC.Assets.sliderThumb = trap_R_RegisterShaderNoMip(ASSET_SLIDER_THUMB);
}

/*
=================
CG_Init

Called after every level change or subsystem restart
Will perform callbacks to make the loading info screen update.
=================
*/
void CG_Init(int serverMessageNum, int serverCommandSequence, int clientNum) {
    const char* s;

    // clear everything
    memset(&cgs, 0, sizeof(cgs));
    memset(&cg, 0, sizeof(cg));
    memset(cg_entities, 0, sizeof(cg_entities));
    memset(cg_weapons, 0, sizeof(cg_weapons));
    memset(cg_items, 0, sizeof(cg_items));

    cg.clientNum = clientNum;
    cg.race.nextCheckpointEnt = -1;
    cg.race.nextNextCheckpointEnt = -1;
    cg.race.currentCheckpointEnt = -1;
    cg.autoFollowClient = -1;  // [QL] deferred auto-follow idle (CG_CheckAutoFollow)
    CG_ClearChat();

    cgs.processedSnapshotNum = serverMessageNum;
    cgs.serverCommandSequence = serverCommandSequence;

    // load a few needed things before we do any screen updates
    cgs.media.charsetShader = trap_R_RegisterShader("gfx/2d/bigchars");
    cgs.media.whiteShader = trap_R_RegisterShader("white");
    cgs.media.charsetProp = trap_R_RegisterShaderNoMip("menu/art/font1_prop.png");
    cgs.media.charsetPropGlow = trap_R_RegisterShaderNoMip("menu/art/font1_prop_glo.png");
    cgs.media.charsetPropB = trap_R_RegisterShaderNoMip("menu/art/font2_prop.png");

    // [QL] Register TTF fonts early so CG_Text_Paint works during loading screen.
    // Register at 2x point sizes for sharper rendering at high resolutions -
    // ioq3's font atlas is fixed at registration time; higher point size = more glyph detail.
    // Override glyphScale so text draws at the correct visual size.
    // CG_LoadHudMenu() later tries to re-register with abstract names like "fonts/font"
    // which don't exist in the pk3 - RE_RegisterFont returns early on file-not-found
    // WITHOUT modifying the output struct, so our early registration is preserved.
    trap_R_RegisterFont("fonts/notosans-regular.ttf", 48, &cgDC.Assets.textFont);
    trap_R_RegisterFont("fonts/droidsansmono.ttf", 32, &cgDC.Assets.smallFont);
    trap_R_RegisterFont("fonts/handelgothic.ttf", 96, &cgDC.Assets.bigFont);
    cgDC.Assets.fontRegistered = qtrue;

    // [QL] Populate extraFonts[] for fontIndex-based selection:
    // 0 = textFont (NotoSans), 1 = bigFont (handelgothic), 2 = smallFont (DroidSansMono)
    memcpy(&cgDC.Assets.extraFonts[0], &cgDC.Assets.textFont, sizeof(fontInfo_t));
    memcpy(&cgDC.Assets.extraFonts[1], &cgDC.Assets.bigFont, sizeof(fontInfo_t));
    memcpy(&cgDC.Assets.extraFonts[2], &cgDC.Assets.smallFont, sizeof(fontInfo_t));

    // [QL] loading screen assets
    cgs.media.loadingbackShader = trap_R_RegisterShaderNoMip("levelshots/loadingback.jpg");
    cgs.media.gtBackgroundShader = trap_R_RegisterShaderNoMip("ui/assets/main_menu/gt_background.png");
    cgs.media.qlLogoShader = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo.png");
    cgs.media.logoBackgroundShader = trap_R_RegisterShaderNoMip("ui/assets/main_menu/logo_background.png");
    cgs.media.backscreenSmokeShader = trap_R_RegisterShaderNoMip("ui/assets/backscreen_smoke.jpg");

    // [QL] initialize speedometer bar colors (green / yellow, alpha set per-mode)
    cg.speedBarColor1[0] = 0.0f; cg.speedBarColor1[1] = 1.0f;
    cg.speedBarColor1[2] = 0.0f; cg.speedBarColor1[3] = 1.0f;
    cg.speedBarColor2[0] = 1.0f; cg.speedBarColor2[1] = 1.0f;
    cg.speedBarColor2[2] = 0.0f; cg.speedBarColor2[3] = 1.0f;

    CG_RegisterCvars();
    CG_InitColorWheel();

    // [QL] reset state cvars on map load
    trap_Cvar_Set("ui_mainmenu", "0");
    trap_Cvar_Set("ui_intermission", "0");

    CG_InitConsoleCommands();

    cg.weaponSelect = WP_MACHINEGUN;

    cgs.redflag = cgs.blueflag = -1;  // For compatibily, default to unset for
    cgs.flagStatus = -1;              // old servers

    // get the rendering configuration from the client system
    trap_GetGlconfig(&cgs.glconfig);
    cgs.screenXScale = cgs.glconfig.vidWidth / (float)SCREEN_WIDTH;
    cgs.screenYScale = cgs.glconfig.vidHeight / (float)SCREEN_HEIGHT;

    // [QL] compute widescreen bias: half the extra width beyond 4:3
    {
        float width43 = 4.0f * cgs.glconfig.vidHeight / 3.0f;
        if ((float)cgs.glconfig.vidWidth > width43) {
            cgs.widescreenBias = ((float)cgs.glconfig.vidWidth - width43) * 0.5f;
        } else {
            cgs.widescreenBias = 0.0f;
        }
    }

    // get the gamestate from the client system
    trap_GetGameState(&cgs.gameState);

    // check version
    s = CG_ConfigString(CS_GAME_VERSION);
    if (strcmp(s, GAME_VERSION)) {
        CG_Error("Client/Server game mismatch: %s/%s", GAME_VERSION, s);
    }

    s = CG_ConfigString(CS_LEVEL_START_TIME);
    cgs.levelStartTime = atoi(s);

    // [QL] set before the first loading repaint (0x10029bc5, ahead of "collision map" at
    // 0x10029c4e): the repaint runs CG_DrawActiveFrame, whose CG_UpdateCvars would otherwise
    // build the local clientinfo with nothing loaded to copy from
    cg.loading = qtrue;  // force players to load instead of defer

    CG_ParseServerinfo();
    // [QL] parse player model/head overrides before clients are registered so the
    // g_allowCustomHeadmodels / model-override rules apply on the initial load.
    CG_ParseConfigParams();
    // [QL] custom-settings bitmask must be set before CG_RegisterClients so the RR-infected
    // bones model (customSettings bit 0x4000000) applies to clients on the initial load.
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));

    // load the new map
    CG_LoadingString("collision map");

    trap_CM_LoadMap(cgs.mapname);

    String_Init();

    cg.loadingStage++;

    CG_LoadingString("sounds");

    CG_RegisterSounds();
    cg.loadingStage++;

    CG_LoadingString("graphics");

    CG_RegisterGraphics();
    cg.loadingStage++;

    CG_LoadingString("clients");

    CG_RegisterClients();  // if low on memory, some clients will be deferred
    cg.loadingStage++;

    // [QL] 0x10029da7: with com_build the client stops deferring anything -- it registers all
    // fourteen weapons and Crash's three tutorial lines, which otherwise load only when played
    if (trap_Cvar_VariableValue("com_build") != 0) {
        int weapon;

        for (weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++) {
            CG_RegisterWeapon(weapon);
        }
        trap_S_RegisterSound("sound/vo/crash/39a.wav", qfalse);
        trap_S_RegisterSound("sound/vo/crash/41a.wav", qfalse);
        trap_S_RegisterSound("sound/vo/crash/40b.wav", qfalse);
    }

    CG_AssetCache();
    CG_LoadHudMenu();  // load new hud stuff

    cg.loading = qfalse;  // future players will be deferred

    CG_InitLocalEntities();

    CG_InitMarkPolys();

    // remove the last loading update
    cg.infoScreenText[0] = 0;

    // Make sure we have update values (scores)
    CG_SetConfigValues();

    // [QL] pre-cache country flag shaders for the scoreboard/profile UI.
    CG_CacheCountryFlags();

    CG_StartMusic();

    CG_LoadingString("");

    CG_InitTeamChat();

    CG_ShaderStateChanged();

    trap_S_ClearLoopingSounds(qtrue);

    // [QL] cgamex86.dll CG_Init tail @0x10029fb2 stores 1 here as its very last
    // action, so a fresh connection always comes up on the join menu instead of
    // dropping straight into spectator. CG_UpdateJoinMenu takes it from here.
    cg.showJoinMenu = qtrue;
}

/*
=================
CG_Shutdown

Called before every level change or subsystem restart
=================
*/
void CG_Shutdown(void) {
    // some mods may need to do cleanup work here,
    // like closing files or archiving session data
}