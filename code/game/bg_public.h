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
#ifndef BG_PUBLIC_H
#define BG_PUBLIC_H

#define GAME_VERSION BASEGAME

#define DEFAULT_GRAVITY 800
#define GIB_HEALTH -40
#define ARMOR_PROTECTION 0.66

#define MAX_ITEMS 256

#define RANK_TIED_FLAG 0x4000

#define DEFAULT_SHOTGUN_SPREAD 700
#define DEFAULT_SHOTGUN_COUNT 20

#define ITEM_RADIUS 15

#define LIGHTNING_RANGE 768

#define SCORE_NOT_PRESENT -9999

#define VOTE_TIME 30000

typedef enum {
    VOTE_NONE,
    VOTE_PENDING,
    VOTE_YES,
    VOTE_NO,
    VOTE_PASSED,
    VOTE_VETOED
} voteState_t;

#define VF_MAP              0x0001
#define VF_MAP_RESTART      0x0002
#define VF_NEXTMAP          0x0004
#define VF_GAMETYPE         0x0008
#define VF_KICK             0x0010
#define VF_TIMELIMIT        0x0020
#define VF_FRAGLIMIT        0x0040
#define VF_SHUFFLE          0x0080
#define VF_TEAMSIZE         0x0100
#define VF_COINTOSS         0x0200
#define VF_LOADOUTS         0x0400
#define VF_ENDMAP_VOTING    0x0800
#define VF_AMMO             0x1000
#define VF_TIMERS           0x2000
#define VF_WEAPRESPAWN      0x4000

#define MINS_Z -24
#define DEFAULT_VIEWHEIGHT 26
#define CROUCH_VIEWHEIGHT 12
#define DEAD_VIEWHEIGHT -16

#define CS_MUSIC 2
#define CS_MESSAGE 3
#define CS_MOTD 4
#define CS_WARMUP 5
#define CS_SCORES1 6
#define CS_SCORES2 7
#define CS_VOTE_TIME 8
#define CS_VOTE_STRING 9
#define CS_VOTE_YES 10
#define CS_VOTE_NO 11
#define CS_GAME_VERSION 12
#define CS_LEVEL_START_TIME 13
#define CS_INTERMISSION 14
#define CS_ITEMS 15
#define CS_BOTINFO 16
#define CS_MODELS 17
#define CS_SOUNDS (CS_MODELS + MAX_MODELS)
#define CS_PLAYERS (CS_SOUNDS + MAX_SOUNDS)
#define CS_LOCATIONS (CS_PLAYERS + MAX_CLIENTS)
#define CS_LAST_GENERIC (CS_LOCATIONS + MAX_LOCATIONS)
#define CS_FLAGSTATUS 658
#define CS_SCORES1PLAYER 659
#define CS_SCORES2PLAYER 660
#define CS_ROUND_WARMUP 661
#define CS_ROUND_STATUS CS_ROUND_WARMUP
#define CS_ROUND_START_TIME 662
#define CS_ROUND_TIME CS_ROUND_START_TIME
#define CS_TEAMCOUNT_RED 663
#define CS_TEAMCOUNT_BLUE 664
#define CS_SHADERSTATE 665
#define CS_NEXTMAP 666
#define CS_PRACTICE 667
#define CS_FREECAM 668
#define CS_PAUSE_START_TIME 669
#define CS_PAUSE_END_TIME 670
#define CS_TIMEOUTS_RED 671
#define CS_TIMEOUTS_BLUE 672
#define CS_MODEL_OVERRIDE 673
#define CS_PLAYER_CYLINDERS 674
#define CS_DEBUGFLAGS 675
#define CS_ENABLEBREATH 676
#define CS_DMGTHROUGHDEPTH 677
#define CS_AUTHOR 678
#define CS_AUTHOR2 679
#define CS_ADVERT_DELAY 680
#define CS_PMOVEINFO 681
#define CS_ARMORINFO 682
#define CS_WEAPONINFO 683
#define CS_PLAYERINFO 684
#define CS_SCORE1STPLAYER 685
#define CS_SCORE2NDPLAYER 686
#define CS_CLIENTNUM1STPLAYER 687
#define CS_CLIENTNUM2NDPLAYER 688
#define CS_NAME1STPLAYER 689
#define CS_NAME2NDPLAYER 690
#define CS_ATMOSEFFECT 691
#define CS_MOST_DAMAGEDEALT_PLYR 692
#define CS_MOST_ACCURATE_PLYR 693
#define CS_REDTEAMBASE 694
#define CS_BLUETEAMBASE 695
#define CS_BEST_ITEMCONTROL_PLYR 696
#define CS_MOST_VALUABLE_OFFENSIVE_PLYR 697
#define CS_MOST_VALUABLE_DEFENSIVE_PLYR 698
#define CS_MOST_VALUABLE_PLYR 699
#define CS_GENERIC_COUNT_RED 700
#define CS_GENERIC_COUNT_BLUE 701
#define CS_AD_SCORES 702
#define CS_ROUND_WINNER 703
#define CS_CUSTOM_SETTINGS 704
#define CS_ROTATIONMAPS 705
#define CS_ROTATIONVOTES 706
#define CS_DISABLE_VOTE_UI 707
#define CS_ALLREADY_TIME 708
#define CS_INFECTED_SURVIVOR_MINSPEED 709
#define CS_RACE_POINTS 710
#define CS_DISABLE_LOADOUT 711
#define CS_MATCH_GUID 712
#define CS_STARTING_WEAPONS 713
#define CS_STEAM_ID 714
#define CS_STEAM_WORKSHOP_IDS 715
#define CS_MAX 716

#if (CS_MAX) > MAX_CONFIGSTRINGS
#error overflow: (CS_MAX) > MAX_CONFIGSTRINGS
#endif

typedef enum {
    GT_FFA,
    GT_DUEL,
    GT_RACE,
    GT_TEAM,
    GT_CA,
    GT_CTF,
    GT_1FCTF,
    GT_OBELISK,
    GT_HARVESTER,
    GT_FREEZE,
    GT_DOMINATION,
    GT_AD,
    GT_RR,
    GT_MAX_GAME_TYPE
} gametype_t;

extern const char *gametypeShortNames[];
extern const char *gametypeDisplayNames[];

typedef enum {
    RS_WARMUP,
    RS_COUNTDOWN,
    RS_SHUFFLE,
    RS_PLAYING,
    RS_ROUND_OVER,
    RS_EXIT
} roundStateState_t;

typedef enum { GENDER_MALE,
               GENDER_FEMALE,
               GENDER_NEUTER } gender_t;

typedef enum {
    PM_NORMAL,
    PM_NOCLIP,
    PM_SPECTATOR,
    PM_DEAD,
    PM_FREEZE,
    PM_INTERMISSION,
    PM_TUTORIAL
} pmtype_t;

typedef enum {
    WEAPON_READY,
    WEAPON_RAISING,
    WEAPON_DROPPING,
    WEAPON_FIRING
} weaponstate_t;

#define PMF_DUCKED          0x0001
#define PMF_JUMP_HELD       0x0002
#define PMF_FROZEN          0x0004
#define PMF_BACKWARDS_JUMP  0x0008
#define PMF_BACKWARDS_RUN   0x0010
#define PMF_TIME_LAND       0x0020
#define PMF_TIME_KNOCKBACK  0x0040
#define PMF_TIME_WATERJUMP  0x0080
#define PMF_PAUSED          0x0100
#define PMF_ZOOM            0x0200
#define PMF_RESPAWNED       0x0200
#define PMF_USE_ITEM_HELD   0x0400
#define PMF_GRAPPLE_PULL    0x0800
#define PMF_FOLLOW          0x1000
#define PMF_HOOKED          0x2000
#define PMF_INVULEXPAND     0x4000
#define PMF_TIME_GRAPPLE    0x8000
#define PMF_PROMODE         0x10000
#define PMF_DOUBLE_JUMPED   0x20000
#define PMF_NO_AUTOHOP      0x40000
#define PMF_LOADOUT_FORCED  0x80000
#define PMF_CROUCH_SLIDE    0x100000

#define PMF_ALL_TIMES (PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_KNOCKBACK)

#define MAXTOUCH 32
typedef struct {
    playerState_t* ps;

    usercmd_t cmd;
    int tracemask;
    int debugLevel;
    int noFootsteps;
    qboolean gauntletHit;

    int numtouch;
    int touchents[MAXTOUCH];

    vec3_t mins, maxs;

    int watertype;
    int waterlevel;

    float xyspeed;

    float stepHeight;
    int stepTime;

    void (*trace)(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask);
    int (*pointcontents)(const vec3_t point, int passEntityNum);

    qboolean hookEnemy;

    qboolean retailBobCycle;
} pmove_t;

void PM_UpdateViewAngles(playerState_t* ps, const usercmd_t* cmd);
void Pmove(pmove_t* pmove);

typedef enum {
    STAT_HEALTH,
    STAT_HOLDABLE_ITEM,
    STAT_RUNE,
    STAT_WEAPONS,
    STAT_ARMOR,
    STAT_BSKILL,
    STAT_CLIENTS_READY,
    STAT_MAX_HEALTH,
    STAT_SPINUP,
    STAT_FLIGHT_THRUST,
    STAT_CUR_FLIGHT_FUEL,
    STAT_MAX_FLIGHT_FUEL,
    STAT_FLIGHT_REFUEL,
    STAT_JUMPTIME,
    STAT_ARMORTYPE,
    STAT_KEY,
} statIndex_t;

#define STAT_PERSISTANT_POWERUP STAT_RUNE
#define STAT_DEAD_YAW           STAT_BSKILL

typedef enum {
    PERS_SCORE,
    PERS_HITS,
    PERS_RANK,
    PERS_TEAM,
    PERS_SPAWN_COUNT,
    PERS_PLAYEREVENTS,
    PERS_ATTACKER,
    PERS_KILLED,
    PERS_IMPRESSIVE_COUNT,
    PERS_EXCELLENT_COUNT,
    PERS_DEFEND_COUNT,
    PERS_ASSIST_COUNT,
    PERS_GAUNTLET_FRAG_COUNT,
    PERS_CAPTURES,
    PERS_ATTACKEE_ARMOR
} persEnum_t;

#define EF_DEAD 0x00000001
#define EF_TICKING 0x00000002
#define EF_TELEPORT_BIT 0x00000004
#define EF_AWARD_EXCELLENT 0x00000008
#define EF_PLAYER_EVENT 0x00000010
#define EF_BOUNCE 0x00000010
#define EF_BOUNCE_HALF 0x00000020
#define EF_AWARD_GAUNTLET 0x00000040
#define EF_NODRAW 0x00000080
#define EF_FIRING 0x00000100
#define EF_KAMIKAZE 0x00000200
#define EF_MOVER_STOP 0x00000400
#define EF_AWARD_CAP 0x00000800
#define EF_TALK 0x00001000
#define EF_CONNECTION 0x00002000
#define EF_SPECTATOR 0x00004000
#define EF_AWARD_IMPRESSIVE 0x00008000
#define EF_AWARD_DEFEND 0x00010000
#define EF_AWARD_ASSIST 0x00020000
#define EF_AWARD_DENIED 0x00040000
#define EF_NAILBOUNCE 0x00100000

typedef enum {
    PW_NONE,

    PW_REDFLAG,
    PW_BLUEFLAG,
    PW_NEUTRALFLAG,
    PW_QUAD,
    PW_BATTLESUIT,
    PW_HASTE,
    PW_INVIS,
    PW_REGEN,
    PW_FLIGHT,
    PW_INVULNERABILITY,
    PW_SCOUT,
    PW_GUARD,
    PW_DOUBLER,
    PW_AMMOREGEN,
    PW_FREEZE,

    PW_NUM_POWERUPS

} powerup_t;

typedef enum {
    HI_NONE,

    HI_TELEPORTER,
    HI_MEDKIT,
    HI_KAMIKAZE,
    HI_PORTAL,
    HI_INVULNERABILITY,
    HI_FLIGHT,

    HI_NUM_HOLDABLE
} holdable_t;

typedef enum {
    WP_NONE,

    WP_GAUNTLET,
    WP_MACHINEGUN,
    WP_SHOTGUN,
    WP_GRENADE_LAUNCHER,
    WP_ROCKET_LAUNCHER,
    WP_LIGHTNING,
    WP_RAILGUN,
    WP_PLASMAGUN,
    WP_BFG,
    WP_GRAPPLING_HOOK,
    WP_NAILGUN,
    WP_PROX_LAUNCHER,
    WP_CHAINGUN,
    WP_HMG,

    WP_NUM_WEAPONS
} weapon_t;

#define NUM_LIGHTNING_STYLES 5

#define PLAYEREVENT_DENIEDREWARD 0x0001
#define PLAYEREVENT_GAUNTLETREWARD 0x0002
#define PLAYEREVENT_HOLYSHIT 0x0004
#define PLAYEREVENT_FIRSTFRAG 0x0008

#define EV_EVENT_BIT1 0x00000100
#define EV_EVENT_BIT2 0x00000200
#define EV_EVENT_BITS (EV_EVENT_BIT1 | EV_EVENT_BIT2)

#define EVENT_VALID_MSEC 300

typedef enum {
    EV_NONE,

    EV_FOOTSTEP,
    EV_FOOTSTEP_METAL,
    EV_FOOTSPLASH,
    EV_FOOTWADE,
    EV_SWIM,

    EV_FALL_SHORT,
    EV_FALL_MEDIUM,
    EV_FALL_FAR,

    EV_JUMP_PAD,
    EV_JUMP,

    EV_WATER_TOUCH,
    EV_WATER_LEAVE,
    EV_WATER_UNDER,
    EV_WATER_CLEAR,

    EV_ITEM_PICKUP,
    EV_GLOBAL_ITEM_PICKUP,

    EV_NOAMMO,
    EV_CHANGE_WEAPON,
    EV_DROP_WEAPON,
    EV_FIRE_WEAPON,

    EV_USE_ITEM0,
    EV_USE_ITEM1,
    EV_USE_ITEM2,
    EV_USE_ITEM3,
    EV_USE_ITEM4,
    EV_USE_ITEM5,
    EV_USE_ITEM6,
    EV_USE_ITEM7,
    EV_USE_ITEM8,
    EV_USE_ITEM9,
    EV_USE_ITEM10,
    EV_USE_ITEM11,
    EV_USE_ITEM12,
    EV_USE_ITEM13,
    EV_USE_ITEM14,
    EV_USE_ITEM15,

    EV_ITEM_RESPAWN,
    EV_ITEM_POP,
    EV_PLAYER_TELEPORT_IN,
    EV_PLAYER_TELEPORT_OUT,

    EV_GRENADE_BOUNCE,

    EV_GENERAL_SOUND,
    EV_GLOBAL_SOUND,
    EV_GLOBAL_TEAM_SOUND,

    EV_BULLET_HIT_FLESH,
    EV_BULLET_HIT_WALL,

    EV_MISSILE_HIT,
    EV_MISSILE_MISS,
    EV_MISSILE_MISS_METAL,
    EV_RAILTRAIL,
    EV_SHOTGUN,
    EV_BULLET,

    EV_PAIN,
    EV_DEATH1,
    EV_DEATH2,
    EV_DEATH3,
    EV_DROWN,
    EV_OBITUARY,

    EV_POWERUP_QUAD,
    EV_POWERUP_BATTLESUIT,
    EV_POWERUP_REGEN,
    EV_POWERUP_ARMORREGEN,

    EV_GIB_PLAYER,
    EV_SCOREPLUM,

    EV_PROXIMITY_MINE_STICK,
    EV_PROXIMITY_MINE_TRIGGER,
    EV_KAMIKAZE,
    EV_OBELISKEXPLODE,
    EV_OBELISKPAIN,
    EV_INVUL_IMPACT,
    EV_JUICED,
    EV_LIGHTNINGBOLT,

    EV_DEBUG_LINE,
    EV_TAUNT,
    EV_TAUNT_YES,
    EV_TAUNT_NO,
    EV_TAUNT_FOLLOWME,
    EV_TAUNT_GETFLAG,
    EV_TAUNT_GUARDBASE,
    EV_TAUNT_PATROL,

    EV_FOOTSTEP_SNOW,
    EV_FOOTSTEP_WOOD,
    EV_ITEM_PICKUP_SPEC,
    EV_OVERTIME,
    EV_GAMEOVER,
    EV_MISSILE_MISS_DMGTHROUGH,
    EV_THAW_PLAYER,
    EV_THAW_TICK,
    EV_SHOTGUN_KILL,
    EV_POI,
    EV_DEBUG_HITBOX,
    EV_LIGHTNING_DISCHARGE,
    EV_RACE_START,
    EV_RACE_CHECKPOINT,
    EV_RACE_FINISH,
    EV_DAMAGEPLUM,
    EV_AWARD,
    EV_INFECTED,
    EV_NEW_HIGH_SCORE,

    EV_NUM_ETYPES
} entity_event_t;

typedef enum {
    GTS_RED_CAPTURE,
    GTS_BLUE_CAPTURE,
    GTS_RED_RETURN,
    GTS_BLUE_RETURN,
    GTS_RED_TAKEN,
    GTS_BLUE_TAKEN,
    GTS_REDOBELISK_ATTACKED,
    GTS_BLUEOBELISK_ATTACKED,
    GTS_REDTEAM_SCORED,
    GTS_BLUETEAM_SCORED,
    GTS_REDTEAM_TOOK_LEAD,
    GTS_BLUETEAM_TOOK_LEAD,
    GTS_TEAMS_ARE_TIED,
    GTS_KAMIKAZE,
    GTS_REDTEAM_WON,
    GTS_BLUETEAM_WON,
    GTS_RED_WINS_ROUND,
    GTS_BLUE_WINS_ROUND,
    GTS_DRAW_ROUND,
    GTS_LAST_STANDING,
    GTS_ROUND_OVER,
    GTS_DOMINATION_POINT_CAPTURE = 23,
    GTS_SURVIVOR = 24
} global_team_sound_t;

typedef enum {
    AWARD_COMBOKILL,
    AWARD_RAMPAGE,
    AWARD_MIDAIR,
    AWARD_REVENGE,
    AWARD_PERFORATED,
    AWARD_HEADSHOT,
    AWARD_ACCURACY,
    AWARD_QUADGOD,
    AWARD_FIRSTFRAG,
    AWARD_PERFECT,
    AWARD_NUM_AWARDS
} award_t;

typedef enum {
    BOTH_DEATH1,
    BOTH_DEAD1,
    BOTH_DEATH2,
    BOTH_DEAD2,
    BOTH_DEATH3,
    BOTH_DEAD3,

    TORSO_GESTURE,

    TORSO_ATTACK,
    TORSO_ATTACK2,

    TORSO_DROP,
    TORSO_RAISE,

    TORSO_STAND,
    TORSO_STAND2,

    LEGS_WALKCR,
    LEGS_WALK,
    LEGS_RUN,
    LEGS_BACK,
    LEGS_SWIM,

    LEGS_JUMP,
    LEGS_LAND,

    LEGS_JUMPB,
    LEGS_LANDB,

    LEGS_IDLE,
    LEGS_IDLECR,

    LEGS_TURN,

    TORSO_GETFLAG,
    TORSO_GUARDBASE,
    TORSO_PATROL,
    TORSO_FOLLOWME,
    TORSO_AFFIRMATIVE,
    TORSO_NEGATIVE,

    MAX_ANIMATIONS,

    LEGS_BACKCR,
    LEGS_BACKWALK,
    FLAG_RUN,
    FLAG_STAND,
    FLAG_STAND2RUN,

    MAX_TOTALANIMATIONS
} animNumber_t;

typedef struct animation_s {
    int firstFrame;
    int numFrames;
    int loopFrames;
    int frameLerp;
    int initialLerp;
    int reversed;
    int flipflop;
} animation_t;

#define ANIM_TOGGLEBIT 128

typedef enum {
    TEAM_FREE,
    TEAM_RED,
    TEAM_BLUE,
    TEAM_SPECTATOR,

    TEAM_NUM_TEAMS
} team_t;

#define TEAM_LOCATION_UPDATE_TIME 1000

#define TEAM_MAXOVERLAY 32

typedef enum {
    TEAMTASK_NONE,
    TEAMTASK_OFFENSE,
    TEAMTASK_DEFENSE,
    TEAMTASK_PATROL,
    TEAMTASK_FOLLOW,
    TEAMTASK_RETRIEVE,
    TEAMTASK_ESCORT,
    TEAMTASK_CAMP
} teamtask_t;

typedef enum {
    MOD_UNKNOWN,
    MOD_SHOTGUN,
    MOD_GAUNTLET,
    MOD_MACHINEGUN,
    MOD_GRENADE,
    MOD_GRENADE_SPLASH,
    MOD_ROCKET,
    MOD_ROCKET_SPLASH,
    MOD_PLASMA,
    MOD_PLASMA_SPLASH,
    MOD_RAILGUN,
    MOD_LIGHTNING,
    MOD_BFG,
    MOD_BFG_SPLASH,
    MOD_WATER,
    MOD_SLIME,
    MOD_LAVA,
    MOD_CRUSH,
    MOD_TELEFRAG,
    MOD_FALLING,
    MOD_SUICIDE,
    MOD_TARGET_LASER,
    MOD_TRIGGER_HURT,
    MOD_NAIL,
    MOD_CHAINGUN,
    MOD_PROXIMITY_MINE,
    MOD_KAMIKAZE,
    MOD_JUICED,
    MOD_GRAPPLE,
    MOD_SWITCH_TEAMS,
    MOD_THAW,
    MOD_LIGHTNING_DISCHARGE,
    MOD_HMG,
    MOD_RAILGUN_HEADSHOT
} meansOfDeath_t;

typedef enum {
    IT_BAD,
    IT_WEAPON,
    IT_AMMO,
    IT_ARMOR,
    IT_HEALTH,
    IT_POWERUP,
    IT_HOLDABLE,
    IT_PERSISTANT_POWERUP,
    IT_TEAM,
    IT_KEY
} itemType_t;

#define KEY_SILVER  1
#define KEY_GOLD    2
#define KEY_MASTER  4

#define MAX_ITEM_MODELS 4

typedef struct gitem_s {
    char* classname;
    char* pickup_sound;
    char* world_model[MAX_ITEM_MODELS];

    char* icon;
    char* pickup_name;

    int quantity;
    itemType_t giType;

    int giTag;

    char* precaches;
    char* sounds;

    int itemTimer;

    int validGametypes;
} gitem_t;

extern gitem_t bg_itemlist[];
extern int bg_numItems;

extern int bg_weaponReloadTime[WP_NUM_WEAPONS];
void BG_SetWeaponReload(int weapon, int reloadTime);

void PM_SetAirAccel(float value);
void PM_SetAirControl(float value);
void PM_SetAirStepFriction(float value);
void PM_SetAirSteps(int value);
void PM_SetAirStopAccel(float value);
void PM_SetAutoHop(int value);
void PM_SetBunnyHop(int value);
void PM_SetChainJump(int value);
void PM_SetChainJumpVelocity(float value);
void PM_SetCircleStrafeFriction(float value);
void PM_SetCrouchSlideFriction(float value);
void PM_SetCrouchSlideTime(int value);
void PM_SetCrouchStepJump(int value);
void PM_SetJumpTimeDeltaMin(float value);
void PM_SetJumpVelocity(float value);
void PM_SetJumpVelocityMax(float value);
void PM_SetJumpVelocityScaleAdd(float value);
void PM_SetJumpVelocityTimeThreshold(float value);
void PM_SetJumpVelocityTimeThresholdOffset(float value);
void PM_SetNoPlayerClip(int value);
void PM_SetRampJump(int value);
void PM_SetRampJumpScale(float value);
void PM_SetStepHeight(float value);
void PM_SetStepJump(int value);
void PM_SetStepJumpVelocity(float value);
void PM_SetStrafeAccel(float value);
void PM_SetWalkAccel(float value);
void PM_SetWalkFriction(float value);
void PM_SetWaterSwimScale(float value);
void PM_SetWaterWadeScale(float value);
void PM_SetWeaponDropTime(int value);
void PM_SetWeaponRaiseTime(int value);
void PM_SetWishSpeed(float value);
void PM_SetDoubleJump(int value);
void PM_SetCrouchSlide(int value);
void PM_SetVelocityGH(float value);
void PM_SetMovementConfig(void);

gitem_t* BG_FindItem(const char* pickupName);
gitem_t* BG_FindItemForWeapon(weapon_t weapon);
qboolean BG_WeaponInLoadout(int weapon);
gitem_t* BG_FindItemForPowerup(powerup_t pw);
gitem_t* BG_FindItemForHoldable(holdable_t pw);
#define ITEM_INDEX(x) ((x) - bg_itemlist)

qboolean BG_CanItemBeGrabbed(int atTime, int warmupTime, int armorTiered, int weaponRespawn,
                             int gametype, const entityState_t* ent, const playerState_t* ps);
qboolean BG_IsRoundBasedGameType(int gametype);
int BG_GetWeaponFromMeansOfDeath(int mod);

int BG_ParseWeaponStringtoFlag(char *string);

extern const int maxAmmoStandard[WP_NUM_WEAPONS];

#define DF_NO_TEAM_DAMAGE           1
#define DF_NO_TEAM_ARMOR_DAMAGE     2
#define DF_NO_SELF_DAMAGE           4
#define DF_NO_SELF_ARMOR_DAMAGE     8
#define DF_NO_FALLING_DAMAGE        16
#define DF_NO_FOOTSTEPS             32
#define DF_INSTAGIB                 0x20000

#define MASK_ALL (-1)
#define MASK_SOLID (CONTENTS_SOLID)
#define MASK_PLAYERSOLID (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY)
#define MASK_DEADSOLID (CONTENTS_SOLID | CONTENTS_PLAYERCLIP)
#define MASK_WATER (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
#define MASK_OPAQUE (CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA)
#define MASK_SHOT (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE)

typedef enum {
    ET_GENERAL,
    ET_PLAYER,
    ET_ITEM,
    ET_MISSILE,
    ET_MOVER,
    ET_BEAM,
    ET_PORTAL,
    ET_SPEAKER,
    ET_PUSH_TRIGGER,
    ET_TELEPORT_TRIGGER,
    ET_INVISIBLE,
    ET_GRAPPLE,
    ET_TEAM,

    ET_EVENTS
} entityType_t;

void BG_EvaluateTrajectory(const trajectory_t* tr, int atTime, vec3_t result);
void BG_EvaluateTrajectoryDelta(const trajectory_t* tr, int atTime, vec3_t result);

void BG_AddPredictableEventToPlayerstate(int newEvent, int eventParm, playerState_t* ps);

void BG_TouchJumpPad(playerState_t* ps, entityState_t* jumppad);

void BG_PlayerStateToEntityState(playerState_t* ps, entityState_t* s, qboolean snap);
void BG_PlayerStateToEntityStateExtraPolate(playerState_t* ps, entityState_t* s, int time, qboolean snap);

qboolean BG_PlayerTouchesItem(playerState_t* ps, entityState_t* item, int atTime, int itemHeight);

#define MAX_ARENAS 1024
#define MAX_ARENAS_TEXT 16384

#define MAX_BOTS 1024
#define MAX_BOTS_TEXT 8192

#define KAMI_SHOCKWAVE_STARTTIME 0
#define KAMI_SHOCKWAVEFADE_STARTTIME 1500
#define KAMI_SHOCKWAVE_ENDTIME 2000
#define KAMI_EXPLODE_STARTTIME 250
#define KAMI_IMPLODE_STARTTIME 2000
#define KAMI_IMPLODE_ENDTIME 2250
#define KAMI_SHOCKWAVE2_STARTTIME 2000
#define KAMI_SHOCKWAVE2FADE_STARTTIME 2500
#define KAMI_SHOCKWAVE2_ENDTIME 3000
#define KAMI_SHOCKWAVEMODEL_RADIUS 88
#define KAMI_BOOMSPHEREMODEL_RADIUS 72
#define KAMI_SHOCKWAVE_MAXRADIUS 1320
#define KAMI_BOOMSPHERE_MAXRADIUS 720
#define KAMI_SHOCKWAVE2_MAXRADIUS 704

#endif
