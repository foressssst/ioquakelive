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
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"

#define POWERUP_BLINKS 5

#define POWERUP_BLINK_TIME 1000
#define FADE_TIME 200
#define PULSE_TIME 200
#define DAMAGE_DEFLECT_TIME 100
#define DAMAGE_RETURN_TIME 400
#define DAMAGE_TIME 500
#define LAND_DEFLECT_TIME 150
#define LAND_RETURN_TIME 300
#define STEP_TIME 100
#define DUCK_TIME 100
#define PAIN_TWITCH_TIME 200
#define WEAPON_SELECT_TIME 1400
#define ITEM_SCALEUP_TIME 1000
#define ITEM_BLOB_TIME 200
#define MUZZLE_FLASH_TIME 20
#define SINK_TIME 1000
#define ATTACKER_HEAD_TIME 10000
#define REWARD_TIME 2000

#define PULSE_SCALE 1.5

#define MAX_STEP_CHANGE 32

#define MAX_VERTS_ON_POLY 10
#define MAX_MARK_POLYS 256

#define STAT_MINUS 10

#define ICON_SIZE 48
#define CHARACTER_WIDTH 32
#define CHAR_HEIGHT 48
#define TEXT_ICON_SPACE 4

#define TEAMCHAT_WIDTH 80
#define TEAMCHAT_HEIGHT 8

#define GIANT_WIDTH 32
#define GIANT_HEIGHT 48

#define NUM_CROSSHAIRS 30

#define TEAM_OVERLAY_MAXNAME_WIDTH 12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH 16

#define DEFAULT_MODEL "sarge"

typedef enum {
    FOOTSTEP_NORMAL,
    FOOTSTEP_BOOT,
    FOOTSTEP_FLESH,
    FOOTSTEP_MECH,
    FOOTSTEP_ENERGY,
    FOOTSTEP_METAL,
    FOOTSTEP_SPLASH,
    FOOTSTEP_SNOW,
    FOOTSTEP_WOOD,

    FOOTSTEP_TOTAL
} footstep_t;

typedef enum {
    IMPACTSOUND_DEFAULT,
    IMPACTSOUND_METAL,
    IMPACTSOUND_FLESH
} impactSound_t;

typedef struct {
    int oldFrame;
    int oldFrameTime;

    int frame;
    int frameTime;

    float backlerp;

    float yawAngle;
    qboolean yawing;
    float pitchAngle;
    qboolean pitching;

    int animationNumber;
    animation_t* animation;
    int animationTime;
} lerpFrame_t;

typedef struct {
    lerpFrame_t legs, torso, flag;
    int lastPainTime;
    int painTime;
    int painDirection;
    int lightningFiring;
    qboolean continuousFiring;
    vec3_t railgunImpact;
    qboolean railgunFlash;

    float barrelAngle;
    int barrelTime;
    qboolean barrelSpinning;
} playerEntity_t;

typedef struct centity_s {
    entityState_t currentState;
    entityState_t nextState;
    qboolean interpolate;
    qboolean currentValid;

    int muzzleFlashTime;
    int previousEvent;
    int teleportFlag;

    int trailTime;
    int leadTrTime;
    int dustTrailTime;
    int miscTime;

    int snapShotTime;

    playerEntity_t pe;

    int errorTime;
    vec3_t errorOrigin;
    vec3_t errorAngles;

    qboolean extrapolated;
    vec3_t rawOrigin;
    vec3_t rawAngles;

    vec3_t beamEnd;

    vec3_t lerpOrigin;
    vec3_t lerpAngles;
} centity_t;

typedef struct markPoly_s {
    struct markPoly_s *prevMark, *nextMark;
    int time;
    qhandle_t markShader;
    qboolean alphaFade;
    float color[4];
    poly_t poly;
    polyVert_t verts[MAX_VERTS_ON_POLY];
} markPoly_t;

typedef enum {
    LE_MARK,
    LE_EXPLOSION,
    LE_SPRITE_EXPLOSION,
    LE_FRAGMENT,
    LE_MOVE_SCALE_FADE,
    LE_FALL_SCALE_FADE,
    LE_FADE_RGB,
    LE_SCALE_FADE,
    LE_SCOREPLUM,
    LE_KAMIKAZE,
    LE_INVULIMPACT,
    LE_INVULJUICED,
    LE_SHOWREFENTITY
} leType_t;

typedef enum {
    LEF_PUFF_DONT_SCALE = 0x0001,
    LEF_TUMBLE = 0x0002,
    LEF_SOUND1 = 0x0004,
    LEF_SOUND2 = 0x0008,
    LEF_PUFF_DONT_SCALE_NOT_KIDDING_MAN = 0x0010
} leFlag_t;

typedef enum {
    LEMT_NONE,
    LEMT_BURN,
    LEMT_BLOOD,
    LEMT_ELECTROGIB,
    LEMT_ICE
} leMarkType_t;

typedef enum {
    LEBS_NONE,
    LEBS_BLOOD,
    LEBS_BRASS,
    LEBS_ELECTROGIB
} leBounceSoundType_t;

typedef struct localEntity_s {
    struct localEntity_s *prev, *next;
    leType_t leType;
    int leFlags;

    int startTime;
    int endTime;
    int fadeInTime;

    float lifeRate;

    trajectory_t pos;
    trajectory_t angles;

    float bounceFactor;

    float color[4];

    float radius;

    float light;
    vec3_t lightColor;

    leMarkType_t leMarkType;
    leBounceSoundType_t leBounceSoundType;

    refEntity_t refEntity;
} localEntity_t;

#define MAX_FLOATING_EFFECTS 32

typedef enum {
    FE_DAMAGE_NUMBER = 0,
    FE_ENTITY_POI    = 1,
    FE_POI           = 2,
    FE_SPRITE        = 3,
    FE_FLOAT_SPRITE  = 4
} floatingEffectType_t;

typedef struct {
    int       type;
    void*     owner;
    vec3_t    origin;
    vec4_t    color;
    qhandle_t shader;
    int       clientNum;
    int       followNum;
    int       spawnTime;
    float     lifeRate;
    int       pad_38;
    float     worldSize;
    int       pad_40;
    float     minPixels;
    int       pad_48;
    float     zOffset;
    float     fadeStart;
    char      text[16];
    float     velX;
    float     velY;
    float     accX;
    float     accY;
    int       doFade;
    int       pad_78;
    int       active;
} floatingEffect_t;

#define MAX_SPEC_ITEM_TIMERS 10

typedef struct {
    int    entityNum;
    int    side;
    int    itemIndex;
    int    time;
    int    distance;
    int    team;
    vec3_t origin;
} specItemTimer_t;

typedef struct {
    int client;
    int score;
    int roundScore;
    int ping;
    int time;
    int scoreFlags;
    int powerUps;
    int accuracy;
    int impressiveCount;
    int excellentCount;
    int guantletCount;
    int defendCount;
    int assistCount;
    int captures;
    qboolean perfect;
    int team;

    qboolean alive;
    int frags;
    int deaths;
    int bestWeapon;
    int bestWeaponAccuracy;
    int damageDone;
    int net;
    int tks;
    int tkd;
    int thaws;

    int selfKills;
    int damageReceived;
    int pickups_ra;
    int pickups_ya;
    int pickups_ga;
    int pickups_mh;
    int pickups_quad;
    int pickups_bs;
    int pickups_regen;
    int pickups_haste;
    int pickups_invis;
    int weaponKills[MAX_WEAPONS];
    int weaponAccuracy[MAX_WEAPONS];
} score_t;

typedef struct {
    int hits;
    int atts;
    int accuracy;
    int damage;
    int kills;
} duelWeaponStats_t;

typedef struct {
    int clientNum;
    int score;
    int ping;
    int time;
    int kills;
    int deaths;
    int accuracy;
    int bestWeapon;
    int damage;
    int awardExcellent;
    int awardImpressive;
    int awardHumiliation;
    qboolean perfect;
    int redArmorPickups;    float redArmorTime;
    int yellowArmorPickups; float yellowArmorTime;
    int greenArmorPickups;  float greenArmorTime;
    int megaHealthPickups;  float megaHealthTime;
    duelWeaponStats_t weaponStats[MAX_WEAPONS];
} duelScore_t;

typedef struct {
    int ra, ya, ga, mh, quad, bs, regen, haste, invis, flag, medkit;
    int quadTime, bsTime, regenTime, hasteTime, invisTime, flagTime;
} teamPickups_t;

typedef struct {
    int accuracy[MAX_WEAPONS];
    int time;
    int clientNum;
    qboolean valid;
} accuracyStats_t;

#define MAX_CUSTOM_SOUNDS 32

typedef struct {
    qboolean infoValid;

    char name[MAX_QPATH];
    char clanTag[MAX_QPATH];
    char clanName[MAX_QPATH];
    char country[MAX_QPATH];
    team_t team;
    uint64_t steamId;

    qboolean talking;
    int talkingTime;

    int botSkill;
    float botSkillValue;
    int privileges;
    int specOnly;
    int playQueue;

    vec3_t color1;
    vec3_t color2;

    byte c1RGBA[4];
    byte c2RGBA[4];

    int score;
    int location;
    int health;
    int armor;
    int curWeapon;

    int handicap;
    int wins, losses;

    int teamTask;
    qboolean teamLeader;
    qboolean ready;

    int powerups;

    int medkitUsageTime;
    int invulnerabilityStartTime;
    int invulnerabilityStopTime;

    char modelName[MAX_QPATH];
    char skinName[MAX_QPATH];
    char headModelName[MAX_QPATH];
    char headSkinName[MAX_QPATH];
    char forcedHeadModel[MAX_QPATH];
    char forcedHeadSkin[MAX_QPATH];
    qboolean deferred;

    qboolean newAnims;
    qboolean fixedlegs;
    qboolean fixedtorso;

    vec3_t headOffset;
    footstep_t footsteps;
    gender_t gender;

    qhandle_t legsModel;
    qhandle_t legsSkin;

    qhandle_t torsoModel;
    qhandle_t torsoSkin;

    qhandle_t headModel;
    qhandle_t headSkin;

    qhandle_t modelIcon;

    float modelScale;

    animation_t animations[MAX_TOTALANIMATIONS];

    sfxHandle_t sounds[MAX_CUSTOM_SOUNDS];
} clientInfo_t;

typedef struct weaponInfo_s {
    qboolean registered;
    gitem_t* item;

    qhandle_t handsModel;
    qhandle_t weaponModel;
    qhandle_t barrelModel;
    qhandle_t flashModel;

    vec3_t weaponMidpoint;

    float flashDlight;
    vec3_t flashDlightColor;
    sfxHandle_t flashSound[4];

    qhandle_t weaponIcon;
    qhandle_t ammoIcon;

    qhandle_t ammoModel;

    qhandle_t missileModel;
    sfxHandle_t missileSound;
    void (*missileTrailFunc)(centity_t*, const struct weaponInfo_s* wi);
    float missileDlight;
    vec3_t missileDlightColor;
    int missileRenderfx;

    void (*ejectBrassFunc)(centity_t*);

    float trailRadius;
    float wiTrailTime;

    sfxHandle_t readySound;
    sfxHandle_t firingSound;
    sfxHandle_t pullSound;
} weaponInfo_t;

typedef struct {
    qboolean registered;
    qhandle_t models[MAX_ITEM_MODELS];
    qhandle_t icon;
} itemInfo_t;

typedef struct {
    int itemNum;
} powerupInfo_t;

#define MAX_SKULLTRAIL 10

typedef struct {
    vec3_t positions[MAX_SKULLTRAIL];
    int numpositions;
} skulltrail_t;

#define MAX_REWARDSTACK 16
#define MAX_SOUNDBUFFER 32
#define SOUNDBUFFER_DELAY 1500

#define CHAT_LINE_TEXT 256
typedef struct {
    int startTime;
    int endTime;
    int teamOnly;
    int fadeTime;
    char text[CHAT_LINE_TEXT];
} chatLine_t;

#define MAX_VOICEFILES 8
#define MAX_VOICECHATBUFFER 32

typedef struct {
    char name[64];
    int  index;
} voiceChatList_t;

typedef struct bufferedVoiceChat_s {
    int         clientNum;
    sfxHandle_t snd;
    int         voiceOnly;
    char        cmd[150];
    char        message[150];
} bufferedVoiceChat_t;

typedef struct teamStatsRow_s {
    int fields[154];
} teamStatsRow_t;

#define MAX_PREDICTED_EVENTS 16

typedef struct {
    int clientFrame;

    int clientNum;

    qboolean demoPlayback;
    qboolean levelShot;
    int deferredPlayerLoading;
    qboolean loading;
    qboolean intermissionStarted;

    int latestSnapshotNum;
    int latestSnapshotTime;

    snapshot_t* snap;
    snapshot_t* nextSnap;
    snapshot_t activeSnapshots[2];

    float frameInterpolation;

    qboolean thisFrameTeleport;
    qboolean nextFrameTeleport;

    int frametime;

    int time;
    int oldTime;

    int physicsTime;

    int timelimitWarnings;
    int fraglimitWarnings;

    qboolean mapRestart;

    qboolean renderingThirdPerson;

    qboolean hyperspace;
    playerState_t predictedPlayerState;
    centity_t predictedPlayerEntity;
    qboolean validPPS;
    int predictedErrorTime;
    vec3_t predictedError;

    int eventSequence;
    int predictableEvents[MAX_PREDICTED_EVENTS];

    float stepChange;
    int stepTime;

    float duckChange;
    int duckTime;

    float landChange;
    int landTime;

    int weaponSelect;
    int weaponPrimaryQueued;

    vec3_t autoAngles;
    vec3_t autoAxis[3];
    vec3_t autoAnglesFast;
    vec3_t autoAxisFast[3];

    refdef_t refdef;
    vec3_t refdefViewAngles;

    qboolean zoomed;
    int zoomTime;
    float zoomSensitivity;
    int fovTarget;

    char infoScreenText[MAX_STRING_CHARS];
    int loadingStage;

    int scoresRequestTime;
    int numScores;
    int selectedScore;
    int teamScores[2];
    score_t scores[MAX_CLIENTS];
    qboolean showScores;
    qboolean scoreBoardShowing;

    qboolean showJoinMenu;

    int complaintClient;
    int complaintEndTime;

    qboolean statsShowing;
    qboolean accShowing;
    qboolean pstatsShowing;
    int      accRequestTime;
    int      pstatsRequestTime;
    int scoreFadeTime;
    char killerName[MAX_NAME_LENGTH];
    qhandle_t killerIcon;
    int spectatorPaintLen;

    skulltrail_t skulltrails[MAX_CLIENTS];

    int centerPrintTime;
    float centerPrintScale;
    int centerPrintY;
    char centerPrint[1024];
    int centerPrintLines;
    qboolean centerPrintFrag;

    int lowAmmoWarning;

    int crosshairClientNum;
    int crosshairClientTime;

    int powerupActive;
    int powerupTime;

    int attackerTime;
    int voiceTime;

    qboolean reloadOtherClients;
    int reloadFollowing;
    int reloadFollowedClient;

    int rewardStack;
    int rewardTime;
    int rewardCount[MAX_REWARDSTACK];
    qhandle_t rewardShader[MAX_REWARDSTACK];
    qhandle_t rewardSound[MAX_REWARDSTACK];

    int soundBufferIn;
    int soundBufferOut;
    int soundBufferTime[MAX_SOUNDBUFFER];
    sfxHandle_t soundBuffer[MAX_SOUNDBUFFER];
    int soundBufferDelay[MAX_SOUNDBUFFER];
    qboolean soundBufferLocked;
    int joinMenuCloseTime;

    int voiceChatTime;
    int voiceChatBufferIn;
    int voiceChatBufferOut;

    int autoFollowClient;
    int autoFollowTime;

    int warmup;
    int warmupCount;

    qboolean autoActionLive;
    qboolean autoActionEnded;
    centity_t* hookEntities[MAX_CLIENTS];

    int followKillerTime;

    int warmupGametype;
    int warmupRound;
    int warmupTurn;
    int warmupState;
    int lastAutoFireTime;

    int itemPickup;
    int itemPickupLast;
    int itemPickupCount;
    int itemPickupTime;
    int itemPickupBlendTime;
    int crosshairHitTime;
    int crosshairHitType;

    int obitSlideStart;
    int obitSlideKey;
    int chatSlideStart;
    int chatSlideKey;
    qboolean chatSlideIn;
    qboolean chatHistoryWasShown;
    int chatHistoryToggle;
    float chatHistoryFrom;
    int chatHistoryRows;

    int weaponSelectTime;
    int weaponConfigSelect;
    int weaponAnimation;
    int weaponAnimationTime;

    float damageTime;
    float damageX, damageY, damageValue;
    int damageType;

    float headYaw;
    float headEndPitch;
    float headEndYaw;
    int headEndTime;
    float headStartPitch;
    float headStartYaw;
    int headStartTime;

    float v_dmg_time;
    float v_dmg_pitch;
    float v_dmg_roll;

    float bobfracsin;
    int bobcycle;
    float xyspeed;
    int nextOrbitTime;

    refEntity_t testModelEntity;
    char testModelName[MAX_QPATH];
    qboolean testGun;

    struct {
        qboolean active;
        int      startTime;
        int      lastTime;
        int      bestTime;
        int      currentCheckPoint;
        int      nextCheckpointEnt;
        int      nextNextCheckpointEnt;
        int      checkpointDiff;
        qboolean hasDiff;
        int      checkpointsPassed;
    } race;

    specItemTimer_t specItemTimers[MAX_SPEC_ITEM_TIMERS];
    int numSpecItemTimers;

    duelScore_t duelScores[2];
    int duelPlayer1;
    int duelPlayer2;
    qboolean duelScoresValid;

    teamPickups_t teamPickups[2];

    accuracyStats_t accuracyStats;

    char voteMapNames[3][MAX_QPATH];
    char voteMapTitles[3][64];
    char voteGameTypes[3][32];
    int voteCounts[3];
    qboolean mapVoteActive;

    #define MAX_OBITUARIES 16
    #define OBIT_FADEIN 150
    #define OBIT_FADEDELAY 3000
    #define OBIT_FADEOUT 200
    struct {
        int         active;
        int         time;
        char        victimName[40];
        int         victimTeam;
        char        attackerName[40];
        int         attackerTeam;
        int         hasAttacker;
        qhandle_t   weaponIcon;
    } obituaries[MAX_OBITUARIES];

    #define SPEED_HISTORY_SIZE 128
    float speedHistory[SPEED_HISTORY_SIZE];
    int speedHistoryCount;
    int speedHistoryIndex;
    vec4_t speedBarColor1;
    vec4_t speedBarColor2;

    qboolean chatHistoryShowing;
    qboolean killRequested;
    int      disconnectRequest;

    #define MAX_CHAT_LINES  24
    #define CHAT_FADEIN 150
    #define CHAT_FADEDELAY 3000
    #define CHAT_FADEOUT 100
    chatLine_t chatLines[MAX_CHAT_LINES];
    chatLine_t currentChatLine;
    int chatIndex;

    char notifyMessages[MAX_CLIENTS][64];
    int  notifyCount;
    int  notifyScrollStart;
    int  notifyScrollTime;

    int adScores[20];

    int lastChatTime;
    int lastChatTime2;

    int keyCatcherActive;

    teamStatsRow_t teamStats[MAX_CLIENTS];

    vec3_t flagOrigin[3];

} cg_t;

typedef enum {
    TIMER_ICON_ARMOR,
    TIMER_ICON_BS,
    TIMER_ICON_HASTE,
    TIMER_ICON_HEALTH,
    TIMER_ICON_INVIS,
    TIMER_ICON_MEDKIT,
    TIMER_ICON_QUAD,
    TIMER_ICON_REGEN,
    TIMER_ICON_UNKNOWN,
    TIMER_NUM_ICONS
} timerIcon_t;

typedef struct {
    qhandle_t charsetShader;
    qhandle_t charsetProp;
    qhandle_t charsetPropGlow;
    qhandle_t charsetPropB;
    qhandle_t whiteShader;
    qhandle_t modifiedShader;

    qhandle_t loadingbackShader;
    qhandle_t gtBackgroundShader;
    qhandle_t qlLogoShader;
    qhandle_t logoBackgroundShader;
    qhandle_t backscreenSmokeShader;

    qhandle_t redCubeModel;
    qhandle_t blueCubeModel;
    qhandle_t redCubeIcon;
    qhandle_t blueCubeIcon;

    qhandle_t redFlagModel;
    qhandle_t blueFlagModel;
    qhandle_t neutralFlagModel;
    qhandle_t droppedFlagModel;
    qhandle_t flagTrackPointerShader;
    qhandle_t keyGoldModel;

    qhandle_t flagPoleModel;
    qhandle_t flagFlapModel;

    qhandle_t redFlagFlapSkin;
    qhandle_t blueFlagFlapSkin;
    qhandle_t neutralFlagFlapSkin;

    qhandle_t redFlagBaseModel;
    qhandle_t blueFlagBaseModel;
    qhandle_t neutralFlagBaseModel;

    qhandle_t overloadBaseModel;
    qhandle_t overloadTargetModel;
    qhandle_t overloadLightsModel;
    qhandle_t overloadEnergyModel;

    qhandle_t harvesterModel;
    qhandle_t harvesterRedSkin;
    qhandle_t harvesterBlueSkin;
    qhandle_t harvesterNeutralModel;

    qhandle_t armorModel;
    qhandle_t armorIcon;

    qhandle_t teamStatusBar;

    qhandle_t deferShader;

    qhandle_t gibAbdomen;
    qhandle_t gibArm;
    qhandle_t gibChest;
    qhandle_t gibFist;
    qhandle_t gibFoot;
    qhandle_t gibForearm;
    qhandle_t gibIntestine;
    qhandle_t gibLeg;
    qhandle_t gibSkull;
    qhandle_t gibBrain;
    qhandle_t gibBrainBase;
    qhandle_t gibAbdomenBase;

    qhandle_t smoke2;

    qhandle_t machinegunBrassModel;
    qhandle_t shotgunBrassModel;

    qhandle_t railRingsShader;
    qhandle_t railCoreShader;

    qhandle_t lightningShader[5];

    qhandle_t grapplingChainShader;

    qhandle_t balloonShader;
    qhandle_t medalFrames[6][24];
    int fragMessageFont;
    qhandle_t chatBalloonFrames[16];
    qhandle_t connectionShader;

    qhandle_t selectShader;
    qhandle_t weaponBarHighlightShader;
    qhandle_t weaponBarIdleShader[2];
    qhandle_t infiniteAmmoShader;
    qhandle_t viewBloodShader;
    qhandle_t viewDamageShader;
    qhandle_t tracerShader;
    qhandle_t crosshairShader[NUM_CROSSHAIRS];
    qhandle_t lagometerShader;
    qhandle_t backTileShader;
    qhandle_t noammoShader;

    qhandle_t smokePuffShader;
    qhandle_t smokePuffRageProShader;
    qhandle_t shotgunSmokePuffShader;
    qhandle_t plasmaBallShader;
    qhandle_t waterBubbleShader;
    qhandle_t bloodTrailShader;
    qhandle_t bloodSprayShaders[4];

    qhandle_t sparkParticleShader;
    qhandle_t timerIcons[TIMER_NUM_ICONS];
    qhandle_t timerSlices[4][2];
    qhandle_t vignetteShader;

    qhandle_t nailPuffShader;
    qhandle_t blueProxMine;

    qhandle_t fragIconShader;
    qhandle_t thawIconShader;

    qhandle_t numberShaders[11];

    qhandle_t shadowMarkShader;

    qhandle_t botSkillShaders[5];

    qhandle_t wakeMarkShader;
    qhandle_t bloodMarkShader;
    qhandle_t bulletMarkShader;
    qhandle_t crackedMarkShader;
    qhandle_t burnMarkShader;
    qhandle_t holeMarkShader;
    qhandle_t energyMarkShader;

    qhandle_t quadShader;
    qhandle_t spawnArmorShader;
    qhandle_t spawnArmor2Shader;
    qhandle_t noPlayerClipShader;
    qhandle_t gooShader;
    qhandle_t xrayMaskShader;
    qhandle_t xrayShader;
    qhandle_t quadWeaponShader;
    qhandle_t brightWeaponShader;
    qhandle_t invisShader;
    qhandle_t ghostWeaponShader;
    qhandle_t regenShader;
    qhandle_t battleSuitShader;
    qhandle_t battleWeaponShader;
    qhandle_t hastePuffShader;

    qhandle_t redKamikazeShader;
    qhandle_t blueKamikazeShader;

    qhandle_t bulletFlashModel;
    qhandle_t ringFlashModel;
    qhandle_t dishFlashModel;
    qhandle_t lightningExplosionModel;

    qhandle_t railExplosionShader;
    qhandle_t plasmaExplosionShader;
    qhandle_t bulletExplosionShader;
    qhandle_t rocketExplosionShader;
    qhandle_t grenadeExplosionShader;
    qhandle_t bfgExplosionShader;
    qhandle_t bloodExplosionShader;

    qhandle_t teleportEffectModel;
    qhandle_t teleportEffectShader;

    qhandle_t kamikazeEffectModel;
    qhandle_t kamikazeShockWave;
    qhandle_t kamikazeHeadModel;
    qhandle_t kamikazeHeadTrail;
    qhandle_t guardPowerupModel;
    qhandle_t scoutPowerupModel;
    qhandle_t doublerPowerupModel;
    qhandle_t ammoRegenPowerupModel;
    qhandle_t invulnerabilityImpactModel;
    qhandle_t invulnerabilityJuicedModel;
    qhandle_t medkitUsageModel;
    qhandle_t dustPuffShader;
    qhandle_t heartShader;
    qhandle_t invulnerabilityPowerupModel;

    qhandle_t scoreboardName;
    qhandle_t scoreboardPing;
    qhandle_t scoreboardScore;
    qhandle_t scoreboardTime;

    qhandle_t medalAccuracy;
    qhandle_t medalAssist;
    qhandle_t medalCapture;
    qhandle_t medalComboKill;
    qhandle_t medalDefend;
    qhandle_t medalExcellent;
    qhandle_t medalFirstFrag;
    qhandle_t medalGauntlet;
    qhandle_t medalHeadshot;
    qhandle_t medalImpressive;
    qhandle_t medalMidair;
    qhandle_t medalPerfect;
    qhandle_t medalPerforated;
    qhandle_t medalQuadGod;
    qhandle_t medalRampage;
    qhandle_t medalRevenge;

    qhandle_t pickupRA;
    qhandle_t pickupYA;
    qhandle_t pickupGA;
    qhandle_t pickupMH;
    qhandle_t pickupMK;
    qhandle_t pickupBlueFlag;
    qhandle_t pickupNeutralFlag;
    qhandle_t pickupRedFlag;
    qhandle_t pickupQuad;
    qhandle_t pickupBS;
    qhandle_t pickupRegen;
    qhandle_t pickupHaste;
    qhandle_t pickupInvis;

    sfxHandle_t voDefaultSound;
    sfxHandle_t voEvilSound;
    sfxHandle_t voFemaleSound;
    sfxHandle_t oneMinuteSound;
    sfxHandle_t fiveMinuteSound;
    sfxHandle_t suddenDeathSound;
    sfxHandle_t overtimeVoSound;
    sfxHandle_t oneFragSound;
    sfxHandle_t twoFragSound;
    sfxHandle_t threeFragSound;
    sfxHandle_t count3Sound;
    sfxHandle_t count2Sound;
    sfxHandle_t count1Sound;
    sfxHandle_t countFightSound;
    sfxHandle_t countPrepareSound;
    sfxHandle_t klaxon1Sound;
    sfxHandle_t klaxon2Sound;
    sfxHandle_t buzzerSound;
    sfxHandle_t doorGoldSound;
    sfxHandle_t doorSilverSound;

    sfxHandle_t goSound;
    sfxHandle_t newHighScoreSound;

    sfxHandle_t hitTeamSound;
    sfxHandle_t returnYourTeamSound;
    sfxHandle_t returnOpponentSound;
    sfxHandle_t countPrepareTeamSound;
    sfxHandle_t redLeadsSound;
    sfxHandle_t blueLeadsSound;
    sfxHandle_t teamsTiedSound;
    sfxHandle_t redScoredSound;
    sfxHandle_t blueScoredSound;
    sfxHandle_t yourTeamScoredSound;
    sfxHandle_t enemyScoredSound;
    sfxHandle_t blueWinsSound;
    sfxHandle_t redWinsSound;

    sfxHandle_t thirtySecondWarningSound;
    sfxHandle_t roundBeginsInSound;
    sfxHandle_t redWinsRoundSound;
    sfxHandle_t blueWinsRoundSound;
    sfxHandle_t roundDrawSound;
    sfxHandle_t roundOverSound;
    sfxHandle_t survivorSound;
    sfxHandle_t lastStandingSound;

    sfxHandle_t captureAwardSound;
    sfxHandle_t captureYourTeamSound;
    sfxHandle_t captureOpponentSound;
    sfxHandle_t takenYourTeamSound;
    sfxHandle_t takenOpponentSound;
    sfxHandle_t pointNeutralizedSound;
    sfxHandle_t pointHumSound;
    sfxHandle_t pointCapturedSounds[6];
    sfxHandle_t pointLostSounds[6];
    sfxHandle_t blueFlagTakenSound;
    sfxHandle_t blueFlagReturnedSound;
    sfxHandle_t redFlagTakenSound;
    sfxHandle_t redFlagReturnedSound;
    sfxHandle_t yourFlagReturnedSound;
    sfxHandle_t enemyFlagReturnedSound;
    sfxHandle_t enemyTookYourFlagSound;
    sfxHandle_t yourTeamTookEnemyFlagSound;
    sfxHandle_t enemyTookTheFlagSound;
    sfxHandle_t yourTeamTookTheFlagSound;
    sfxHandle_t youHaveFlagSound;
    sfxHandle_t holyShitSound;
    sfxHandle_t obeliskHitSound1;
    sfxHandle_t obeliskHitSound2;
    sfxHandle_t obeliskHitSound3;
    sfxHandle_t obeliskRespawnSound;
    sfxHandle_t yourBaseIsUnderAttackSound;
    sfxHandle_t attackTheFlagSound;
    sfxHandle_t defendTheFlagSound;

    sfxHandle_t biteSound;
    sfxHandle_t infectedSound;
    sfxHandle_t nightmareSound;
    sfxHandle_t kamikazeRespawnSound;

    sfxHandle_t tracerSound;
    sfxHandle_t selectSound;
    sfxHandle_t wearOffSound;
    sfxHandle_t useNothingSound;
    sfxHandle_t electroGibSound[4];
    sfxHandle_t electroGibBounceSound[5];
    sfxHandle_t gibSound;
    sfxHandle_t gibBounce1Sound;
    sfxHandle_t gibBounce2Sound;
    sfxHandle_t gibBounce3Sound;
    sfxHandle_t invulnerabilityJuicedSound;
    sfxHandle_t useInvulnerabilitySound;
    sfxHandle_t invulnerabilityImpactSound1;
    sfxHandle_t invulnerabilityImpactSound2;
    sfxHandle_t invulnerabilityImpactSound3;
    sfxHandle_t teleInSound;
    sfxHandle_t teleOutSound;
    sfxHandle_t respawnSound;
    sfxHandle_t lowAmmoSound;
    sfxHandle_t noAmmoSound;
    sfxHandle_t talkSound;
    sfxHandle_t landSound;
    sfxHandle_t hitSounds[4];
    sfxHandle_t killBeepSounds[8];
    sfxHandle_t watrInSound;
    sfxHandle_t watrOutSound;
    sfxHandle_t watrUnSound;
    sfxHandle_t jumpPadSound;

    sfxHandle_t impressiveSounds[3];
    sfxHandle_t excellentSounds[3];
    sfxHandle_t deniedSound;
    sfxHandle_t perfectSound;
    sfxHandle_t humiliationSounds[3];
    sfxHandle_t assistSound;
    sfxHandle_t defendSound;
    sfxHandle_t comboKillSounds[3];
    sfxHandle_t rampageSounds[3];
    sfxHandle_t midAirSounds[3];
    sfxHandle_t revengeSounds[3];
    sfxHandle_t firstFragSound;
    sfxHandle_t perforatedSound;
    sfxHandle_t firstImpressiveSound;
    sfxHandle_t firstExcellentSound;
    sfxHandle_t firstHumiliationSound;
    sfxHandle_t headshotSound;
    sfxHandle_t accuracySound;
    sfxHandle_t quadGodSound;
    sfxHandle_t takenLeadSound;
    sfxHandle_t tiedLeadSound;
    sfxHandle_t lostLeadSound;
    sfxHandle_t voteNow;
    sfxHandle_t votePassed;
    sfxHandle_t voteFailed;
    sfxHandle_t winnerSound;
    sfxHandle_t loserSound;

    sfxHandle_t footsteps[FOOTSTEP_TOTAL][4];

    sfxHandle_t battleSuitVoSound;
    sfxHandle_t hasteVoSound;
    sfxHandle_t invisibilityVoSound;
    sfxHandle_t quadDamageVoSound;
    sfxHandle_t regenerationVoSound;
    sfxHandle_t armorRegenVoSound;
    sfxHandle_t damageVoSound;
    sfxHandle_t guardVoSound;
    sfxHandle_t scoutVoSound;
    sfxHandle_t flightSound;
    sfxHandle_t medkitSound;
    sfxHandle_t quadSound;
    sfxHandle_t sfx_ric1;
    sfxHandle_t sfx_ric2;
    sfxHandle_t sfx_ric3;
    sfxHandle_t sfx_rockexp;
    sfxHandle_t sfx_plasmaexp;
    sfxHandle_t sfx_proxexp;
    sfxHandle_t sfx_nghit;
    sfxHandle_t sfx_nghitflesh;
    sfxHandle_t sfx_nghitmetal;
    sfxHandle_t sfx_chghit;
    sfxHandle_t sfx_chghitflesh;
    sfxHandle_t sfx_chghitmetal;
    sfxHandle_t chaingunWindSound;
    sfxHandle_t weaponHoverSound;
    sfxHandle_t grappleHitSound;
    sfxHandle_t kamikazeExplodeSound;
    sfxHandle_t kamikazeImplodeSound;
    sfxHandle_t kamikazeFarSound;
    sfxHandle_t youSuckSound;
    sfxHandle_t wstbimplSound;
    sfxHandle_t wstbimpmSound;
    sfxHandle_t wstbimpdSound;
    sfxHandle_t wstbactvSound;
    sfxHandle_t regenSound;
    sfxHandle_t armorPickupSound;
    sfxHandle_t protectSound;
    sfxHandle_t n_healthSound;
    sfxHandle_t hgrenb1aSound;
    sfxHandle_t hgrenb2aSound;
    sfxHandle_t timPumpSound;

    sfxHandle_t sfx_lghit1;
    sfxHandle_t sfx_lghit2;
    sfxHandle_t sfx_lghit3;

    qhandle_t patrolShader;
    qhandle_t assaultShader;
    qhandle_t campShader;
    qhandle_t followShader;
    qhandle_t defendShader;
    qhandle_t teamLeaderShader;
    qhandle_t retrieveShader;
    qhandle_t escortShader;

    qhandle_t cursor;
    qhandle_t selectCursor;
    qhandle_t sizeCursor;

    qhandle_t raceFlagB;
    qhandle_t raceFlagR;
    qhandle_t raceFlagN;
    qhandle_t raceNavShader;
    qhandle_t raceFlagF;
    qhandle_t raceFlagG;
    qhandle_t raceFlagD;
    qhandle_t raceMarkerStart;
    qhandle_t raceMarkerCheckpoint;
    qhandle_t raceMarkerFinish;
    qhandle_t domPointModel;
    qhandle_t domSkinRed;
    qhandle_t domSkinBlue;
    qhandle_t domSkinNeutral;
    qhandle_t domShadowShader;

    qhandle_t poiShader;
    qhandle_t quadHogPoiShader;
    qhandle_t powerupPoi[5];
    qhandle_t powerupIncomingShader;
    qhandle_t friendShader;
    qhandle_t friendHitShader;
    qhandle_t friendDeadShader;
    qhandle_t flagCarrierShader;
    qhandle_t flagCarrierHitShader;
    qhandle_t infectedShader;
    qhandle_t adAttackShader;
    qhandle_t adCaptureShader;
    qhandle_t adDefendShader;
    qhandle_t harvesterCaptureShader;
    qhandle_t domCaptureShader[5];
    qhandle_t domDefendShader[5];
    qhandle_t domCaptureDistShader[5];
    qhandle_t domDefendDistShader[5];
    qhandle_t healthSegmentShader;
    qhandle_t cmdUpShader;
    qhandle_t cmdDownShader;
    qhandle_t cmdRightShader;
    qhandle_t cmdLeftShader;
    qhandle_t voiceWindowShader;
    qhandle_t scoreMutedShader;
    qhandle_t scoreSpeakingShader;

    qhandle_t iceShardModel;
    qhandle_t gibSphere;
    qhandle_t deathEffectShader;
    qhandle_t gibSparkShader;
    qhandle_t iceShardShader1;
    qhandle_t iceShardShader2;
    qhandle_t iceShardShader3;
    qhandle_t frozenShader;
    qhandle_t iceMarkShader;
    qhandle_t iceTrailShader;

    qhandle_t gametypeIcon[GT_MAX_GAME_TYPE];

    qhandle_t iceWhiteModel;
    qhandle_t iceBlueModel;

    qhandle_t friendMarkModel;

    qhandle_t debrisPuffShader;
    qhandle_t bloodPuffShader;

    qhandle_t flagStatusHandles[12];

    qhandle_t duelPlateReady[2], duelPlateNotReady[2], duelPlateLeads[2], duelPlateTied[2], duelPlateTrails[2];

} cgMedia_t;

typedef struct {
    gameState_t gameState;
    glconfig_t glconfig;
    float screenXScale;
    float screenYScale;
    float screenXBias;
    float widescreenBias;

    int serverCommandSequence;
    int processedSnapshotNum;

    qboolean localServer;
    qboolean ioqlServer;

    gametype_t gametype;
    int teamsize;
    int teamSizeMin;
    int teamForceBalance;
    int dmflags;
    int fraglimit;
    int capturelimit;
    int scorelimit;
    int mercylimit;
    int timelimit;
    int roundlimit;
    int roundtimelimit;
    int roundWarmupDelay;
    int freezeRoundDelay;
    int maxclients;
    int timeoutCount;
    int timelimit_overtime;
    int itemHeight;
    int gravity;
    int weaponRespawn;
    int itemTimers;
    int quadDamageFactor;
    int voteFlags;
    int startingHealth;
    int adCaptureScoreBonus;
    int adElimScoreBonus;
    int adtouchScoreBonus;
    char mapname[MAX_QPATH];

    char playermodelOverride[MAX_QPATH];
    char playerheadmodelOverride[MAX_QPATH];
    int allowCustomHeadmodels;
    float playerheadScale;
    float playerheadScaleOffset;
    float playerModelScale;

    int voteTime;
    int voteYes;
    int voteNo;
    qboolean voteModified;
    char voteString[MAX_STRING_TOKENS];

    int teamVoteTime[2];
    int teamVoteYes[2];
    int teamVoteNo[2];
    qboolean teamVoteModified[2];
    char teamVoteString[2][MAX_STRING_TOKENS];

    int levelStartTime;
    qboolean svCheats;

    int scores1, scores2;
    char firstPlaceName[40];
    char secondPlaceName[40];
    int redflag, blueflag;
    int flagStatus;

    qboolean newHud;

    qhandle_t gameModels[MAX_MODELS];
    sfxHandle_t gameSounds[MAX_SOUNDS];

    int numInlineModels;
    qhandle_t inlineDrawModel[MAX_MODELS];
    vec3_t inlineModelMidpoints[MAX_MODELS];

    clientInfo_t clientinfo[MAX_CLIENTS];

    char teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH * 3 + 1];
    int teamChatMsgTimes[TEAMCHAT_HEIGHT];
    int teamChatPos;
    int teamLastChatPos;

    int cursorX;
    int cursorY;
    qboolean eventHandling;
    qboolean mouseCaptured;
    qboolean sizingHud;
    void* capturedItem;
    qhandle_t activeCursor;

    int currentOrder;
    qboolean orderPending;
    int orderTime;
    int currentVoiceClient;
    int acceptOrderTime;
    int acceptTask;
    int acceptLeader;
    char acceptVoice[MAX_NAME_LENGTH];

    cgMedia_t media;

    qboolean roundStarted;
    int roundNum;

    int clientNum1stPlayer;
    int clientNum2ndPlayer;
    int score1stPlayer;
    int score2ndPlayer;
    char name1stPlayer[40];
    char name2ndPlayer[40];

    int teamCountRed;
    int teamCountBlue;
    int armorTiered;
    int customSettings;
    int playerCylinders;

    float dmgThroughDepth;

    int teamAliveCount[TEAM_NUM_TEAMS];

    int freezeEnd;
    int pauseEnd;
    int pauseSoundTime;

    int practice;
    int freecam;
    int allReadyTime;
    int roundStartTime;
} cgs_t;

extern cgs_t cgs;
extern cg_t cg;
extern centity_t cg_entities[MAX_GENTITIES];

extern unsigned int g_colorWheel[26];
extern vec3_t g_colorWheelNormalized[26];
extern weaponInfo_t cg_weapons[MAX_WEAPONS];
extern itemInfo_t cg_items[MAX_ITEMS];
extern markPoly_t cg_markPolys[MAX_MARK_POLYS];

#define CVAR_CG_RANGE 0x1000

extern vmCvar_t cg_centertime;
extern vmCvar_t cg_swingSpeed;
extern vmCvar_t cg_shadows;
extern vmCvar_t cg_drawFPS;
extern vmCvar_t cg_drawSnapshot;
extern vmCvar_t cg_draw3dIcons;
extern vmCvar_t cg_drawIcons;
extern vmCvar_t cg_drawAmmoWarning;
extern vmCvar_t cg_drawCrosshair;
extern vmCvar_t cg_drawRewards;
extern vmCvar_t cg_drawRewardsRowSize;
extern vmCvar_t cg_announcerRewardsVO;
extern vmCvar_t cg_announcerLeadsVO;
extern vmCvar_t cg_announcerTiesVO;
extern vmCvar_t cg_announcerLastStandingVO;
extern vmCvar_t cg_buzzerSound;
extern vmCvar_t cg_flagStyle;
extern vmCvar_t cg_drawTeamOverlay;
extern vmCvar_t cg_drawTeamOverlayOpacity;
extern vmCvar_t cg_drawTeamOverlaySize;
extern vmCvar_t cg_drawTeamOverlayX;
extern vmCvar_t cg_drawTeamOverlayY;
extern vmCvar_t cg_selfOnTeamOverlay;
extern vmCvar_t cg_drawItemPickups;
extern vmCvar_t cg_levelTimerDirection;
extern vmCvar_t cg_crosshairX;
extern vmCvar_t cg_crosshairY;
extern vmCvar_t cg_crosshairSize;
extern vmCvar_t cg_crosshairHealth;
extern vmCvar_t cg_crosshairColor;
extern vmCvar_t cg_crosshairBrightness;
extern vmCvar_t cg_crosshairPulse;
extern vmCvar_t cg_crosshairHitStyle;
extern vmCvar_t cg_crosshairHitColor;
extern vmCvar_t cg_crosshairHitTime;
extern vmCvar_t cg_drawStatus;
extern vmCvar_t cg_draw2D;
extern vmCvar_t cg_drawDemoHUD;
extern vmCvar_t cg_xray;
extern vmCvar_t cg_freecam;
qboolean CG_XrayAllowed(void);
extern qboolean cg_demoCursor;
qboolean CG_MenuHoldsKeys(void);
void CG_DemoCursorFrame(void);
qboolean CG_DemoHUDKey(int key, qboolean down);
extern vmCvar_t cg_animSpeed;
extern vmCvar_t cg_debugAnim;
extern vmCvar_t cg_debugEvents;
extern vmCvar_t cg_railTrailTime;
extern vmCvar_t cg_errorDecay;
extern vmCvar_t cg_nopredict;
extern vmCvar_t cg_noPlayerAnims;
extern vmCvar_t cg_showmiss;
extern vmCvar_t cg_footsteps;
extern vmCvar_t cg_addMarks;
extern vmCvar_t cg_brassTime;
extern vmCvar_t cg_bob;
extern vmCvar_t cg_gun_frame;
extern vmCvar_t cg_gun_x;
extern vmCvar_t cg_gun_y;
extern vmCvar_t cg_gun_z;
extern vmCvar_t cg_drawGun;
extern vmCvar_t cg_weaponConfig;
extern vmCvar_t cg_weaponConfig_g;
extern vmCvar_t cg_weaponConfig_mg;
extern vmCvar_t cg_weaponConfig_sg;
extern vmCvar_t cg_weaponConfig_gl;
extern vmCvar_t cg_weaponConfig_rl;
extern vmCvar_t cg_weaponConfig_lg;
extern vmCvar_t cg_weaponConfig_rg;
extern vmCvar_t cg_weaponConfig_pg;
extern vmCvar_t cg_weaponConfig_bfg;
extern vmCvar_t cg_weaponConfig_gh;
extern vmCvar_t cg_weaponConfig_ng;
extern vmCvar_t cg_weaponConfig_pl;
extern vmCvar_t cg_weaponConfig_cg;
extern vmCvar_t cg_weaponConfig_hmg;
extern vmCvar_t r_dynamicLight;
extern vmCvar_t cg_viewsize;
extern vmCvar_t cg_tracerChance;
extern vmCvar_t cg_tracerWidth;
extern vmCvar_t cg_tracerLength;
extern vmCvar_t cg_autoswitch;
extern vmCvar_t cg_itemFx;
extern vmCvar_t cg_itemTimers;
extern vmCvar_t cg_simpleItems;
extern vmCvar_t cg_skipItems;
extern vmCvar_t cg_simpleItemsBob;
extern vmCvar_t cg_simpleItemsHeightOffset;
extern vmCvar_t cg_simpleItemsRadius;
extern vmCvar_t cg_fov;
extern vmCvar_t cg_zoomFov;
extern vmCvar_t cg_thirdPersonRange;
extern vmCvar_t cg_thirdPersonAngle;
extern vmCvar_t cg_thirdPersonPitch;
extern vmCvar_t cg_cameraSmartMode;
extern vmCvar_t cg_cameraThirdPersonSmartMode;
extern vmCvar_t cg_thirdPerson;
extern vmCvar_t cg_lagometer;
extern vmCvar_t cg_drawAttacker;
extern vmCvar_t cg_teamChatTime;
extern vmCvar_t cg_teamChatHeight;
extern vmCvar_t cg_stats;
extern vmCvar_t cg_paused;
extern vmCvar_t cg_blood;

extern vmCvar_t cg_gibCount;
extern vmCvar_t cg_gibColor;
extern vmCvar_t cg_gibJump;
extern vmCvar_t cg_gibVelocity;
extern vmCvar_t cg_gibVelocityRandomness;
extern vmCvar_t cg_gibDirScale;
extern vmCvar_t cg_gibOriginOffset;
extern vmCvar_t cg_gibOriginOffsetZ;
extern vmCvar_t cg_gibRandomness;
extern vmCvar_t cg_gibRandomnessZ;
extern vmCvar_t cg_gibTime;
extern vmCvar_t cg_gibStepTime;
extern vmCvar_t cg_gibBounceFactor;
extern vmCvar_t cg_gibGravity;
extern vmCvar_t cg_gibSparksSize;
extern vmCvar_t cg_gibSparksColor;
extern vmCvar_t cg_gibSparksHighlight;
extern vmCvar_t cg_gibFloatingVelocity;
extern vmCvar_t cg_gibFloatingRandomness;
extern vmCvar_t cg_gibFloatingOriginOffset;
extern vmCvar_t cg_gibFloatingOriginOffsetZ;

#define MAX_ELECTROGIBS 50

extern vmCvar_t cg_predictItems;
extern vmCvar_t cg_deferPlayers;
extern vmCvar_t cg_teamChatsOnly;
extern vmCvar_t cg_complaintWarning;

extern vmCvar_t cg_scorePlum;
extern vmCvar_t cg_cameraOrbit;
extern vmCvar_t cg_cameraOrbitDelay;
extern vmCvar_t cg_timescaleFadeEnd;
extern vmCvar_t cg_timescaleFadeSpeed;
extern vmCvar_t cg_timescale;
extern vmCvar_t cg_cameraMode;

extern vmCvar_t cg_smallFont;
extern vmCvar_t cg_bigFont;

extern vmCvar_t cg_bubbleTrail;
extern vmCvar_t cg_trueLightning;

extern vmCvar_t cg_currentSelectedPlayer;
extern vmCvar_t cg_currentSelectedPlayerName;
extern vmCvar_t cg_enableDust;
extern vmCvar_t cg_recordSPDemo;
extern vmCvar_t cg_recordSPDemoName;
extern vmCvar_t cg_obeliskRespawnDelay;
extern vmCvar_t cg_lightningStyle;
extern vmCvar_t cg_screenDamage;
extern vmCvar_t cg_kickScale;

extern vmCvar_t cg_damagePlum;
extern vmCvar_t cg_damagePlumColorStyle;
extern vmCvar_t cg_armorTiered;
extern vmCvar_t cg_announcer;
extern vmCvar_t cg_autoHop;
extern vmCvar_t cg_predictLocalRailshots;
extern vmCvar_t cg_blood;
extern vmCvar_t cg_drawCheckpointRemaining;
extern vmCvar_t cg_drawSprites;
extern vmCvar_t cg_drawSpriteSelf;
extern vmCvar_t cg_drawSpecMessages;
extern vmCvar_t cg_drawCrosshairTeamHealth;
extern vmCvar_t cg_drawCrosshairTeamHealthSize;
extern vmCvar_t cg_enemyCrosshairNames;
extern vmCvar_t cg_enemyCrosshairNamesOpacity;
extern vmCvar_t cg_teammateCrosshairNames;
extern vmCvar_t cg_teammateCrosshairNamesOpacity;
extern vmCvar_t cg_teammateNames;
extern vmCvar_t cg_comMaxfps;
extern vmCvar_t cg_forceEnemyModel;
extern vmCvar_t cg_forceEnemySkin;
extern vmCvar_t cg_forceTeamModel;
extern vmCvar_t cg_forceTeamSkin;
extern vmCvar_t cg_forceBlueTeamModel;
extern vmCvar_t cg_forceRedTeamModel;
extern vmCvar_t cg_hitBeep;
extern vmCvar_t cg_voiceChatIndicator;
extern vmCvar_t cg_killBeep;
extern vmCvar_t cg_lightningImpactCap;
extern vmCvar_t cg_lightningImpact;
extern vmCvar_t cg_smokeRadius_RL;
extern vmCvar_t cg_smokeRadius_GL;
extern vmCvar_t cg_smokeRadius_NG;
extern vmCvar_t cg_smokeRadius_dust;
extern vmCvar_t cg_smokeRadius_haste;
extern vmCvar_t cg_smokeRadius_flight;
extern vmCvar_t cg_loadout;
extern vmCvar_t cg_weaponPrimaryQueued;
extern vmCvar_t cg_preferredStartingWeapons;
extern vmCvar_t cg_scalePlayerModelsToBB;
extern vmCvar_t cg_screenDamageAlpha;
extern vmCvar_t cg_screenDamageAlpha_Team;
extern vmCvar_t cg_screenDamage_Self;
extern vmCvar_t cg_screenDamage_Team;
extern vmCvar_t cg_spectating;
extern vmCvar_t cg_specItemTimers;
extern vmCvar_t cg_specItemTimersSize;
extern vmCvar_t cg_specItemTimersX;
extern vmCvar_t cg_specItemTimersY;
extern vmCvar_t cg_specTeamVitals;
extern vmCvar_t cg_specTeamVitalsHealthColor;
extern vmCvar_t cg_specTeamVitalsWidth;
extern vmCvar_t cg_specTeamVitalsY;
extern vmCvar_t cg_specDuelHealthArmor;
extern vmCvar_t cg_specDuelHealthColor;
extern vmCvar_t cg_speedometer;
extern vmCvar_t cg_flagPOIs;
extern vmCvar_t cg_gametype;
extern vmCvar_t cg_poiMaxWidth;
extern vmCvar_t cg_poiMinWidth;
extern vmCvar_t cg_powerupPOIs;
extern vmCvar_t cg_teammatePOIs;
extern vmCvar_t cg_teammatePOIsMaxWidth;
extern vmCvar_t cg_teammatePOIsMinWidth;
extern vmCvar_t cg_drawDeadFriendTime;
extern vmCvar_t cg_drawHitFriendTime;
extern vmCvar_t cg_specNames;
extern vmCvar_t cg_overheadNamesWidth;
extern vmCvar_t cg_drawInputCmds;
extern vmCvar_t cg_drawInputCmdsSize;
extern vmCvar_t cg_drawInputCmdsX;
extern vmCvar_t cg_drawInputCmdsY;
extern vmCvar_t cg_weaponBar;
extern vmCvar_t cg_weaponPrimary;
extern vmCvar_t cg_drawFullWeaponBar;
extern vmCvar_t cg_lowAmmoWeaponBarWarning;
extern vmCvar_t cg_lowAmmoWarningPercentile;
extern vmCvar_t cg_lowAmmoWarningSound;
extern vmCvar_t cg_raceBeep;
extern vmCvar_t cg_useItemMessage;
extern vmCvar_t cg_useItemWarning;
extern vmCvar_t cg_obituaryRowSize;
extern vmCvar_t s_announcerVolume;
extern vmCvar_t s_killBeepVolume;
extern vmCvar_t s_playSoundVolume;

extern vmCvar_t cg_chatbeep;
extern vmCvar_t cg_teamChatBeep;
extern vmCvar_t cg_deadBodyColor;
extern vmCvar_t cg_deadBodyDarken;
extern vmCvar_t g_training;
extern vmCvar_t cg_enemyHeadColor;
extern vmCvar_t cg_enemyLowerColor;
extern vmCvar_t cg_enemyUpperColor;
extern vmCvar_t cg_followKiller;
extern vmCvar_t cg_followPowerup;
extern vmCvar_t cg_impactMarkTime;
extern vmCvar_t cg_impactSparks;
extern vmCvar_t cg_impactSparksLifetime;
extern vmCvar_t cg_impactSparksSize;
extern vmCvar_t cg_impactSparksVelocity;
extern vmCvar_t cg_muzzleFlash;
extern vmCvar_t cg_plasmaStyle;
extern vmCvar_t cg_railStyle;
extern vmCvar_t cg_svFps;
extern vmCvar_t cg_projectileNudge;
extern vmCvar_t cg_autoProjectileNudge;
extern vmCvar_t ch_animateRewardIcons;
extern vmCvar_t ch_rewardY;
extern vmCvar_t ch_fragMessageY;
extern vmCvar_t ch_fragMessageScale;
extern vmCvar_t cg_animateChatBalloon;
extern vmCvar_t ch_animateScroll;
extern vmCvar_t cg_zoomAnimationTime;
extern vmCvar_t cg_drawBrightWeapons;
extern vmCvar_t ch_chatLines;
extern vmCvar_t ch_drawLocalTime;
extern vmCvar_t ch_drawTargetStatus;
extern vmCvar_t ch_drawKeys;
extern vmCvar_t cg_autoAction;
extern vmCvar_t ch_autoStats;
extern vmCvar_t cg_demoKeys;
extern vmCvar_t ch_followKiller;
extern vmCvar_t ch_recordingDemo;
extern vmCvar_t cg_ambient;
extern vmCvar_t ch_gameEvents;
extern vmCvar_t ch_recordMessage;
extern vmCvar_t cg_demoRecording;
extern vmCvar_t cg_demoName;
extern vmCvar_t cg_rocketStyle;

void CG_ProjectileLeadOffset(const centity_t* cent, vec3_t offset);
extern vmCvar_t cg_smoke_SG;
extern vmCvar_t cg_specFov;
extern vmCvar_t cg_switchOnEmpty;
extern vmCvar_t cg_switchToEmpty;
extern vmCvar_t cg_teamHeadColor;
extern vmCvar_t cg_teamLowerColor;
extern vmCvar_t cg_teamUpperColor;
extern vmCvar_t cg_forceTeamWeaponColor;
extern vmCvar_t cg_forceEnemyWeaponColor;
extern vmCvar_t cg_weaponColor_grenade;
extern vmCvar_t cg_playerTeam;
extern vmCvar_t cg_trueShotgun;
extern vmCvar_t cg_vignette;
extern vmCvar_t cg_zoomOutOnDeath;
extern vmCvar_t cg_zoomScaling;
extern vmCvar_t cg_zoomSensitivity;
extern vmCvar_t cg_zoomToggle;

extern vmCvar_t cg_allowTaunt;
extern vmCvar_t cg_announcerLastStanding;

const char* CG_ConfigString(int index);
const char* CG_Argv(int arg);

void QDECL CG_Printf(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void QDECL CG_Error(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));

void CG_StartMusic(void);

void CG_UpdateCvars(void);

int CG_CrosshairPlayer(void);
int CG_LastAttacker(void);
void CG_LoadMenus(const char* menuFile);
void CG_ParseMenu(const char* menuFile);
void CG_KeyEvent(int key, qboolean down);
void CG_MouseEvent(int x, int y);
void CG_EventHandling(int type);
void CG_RankRunFrame(void);
void CG_SetScoreSelection(void* menu);
void CG_SetScoreboardSelection(void);
score_t* CG_GetSelectedScore(void);
void CG_BuildSpectatorString(void);

void CG_LastChatCommand(void);
void CG_LastChatCommand2(void);
void CG_SetKeyCatcher(void);
void CG_ClearKeyCatcher(void);
int  CG_GetActiveFrame(void);

void        CG_RaceInit(int fullReset);
int         CG_RaceCheckpointsLeft(void);
const char* CG_FormatRaceTime(int msec);
int         CG_GetOvertimeCount(void);
qboolean    CG_LocalPlayerLeads(void);
const char* CG_TranslateMapName(const char* mapname);
void        CG_CacheCountryFlags(void);
qhandle_t   CG_CountryFlagShader(const char* country);

void CG_TestModel_f(void);
void CG_TestGun_f(void);
void CG_TestModelNextFrame_f(void);
void CG_TestModelPrevFrame_f(void);
void CG_TestModelNextSkin_f(void);
void CG_TestModelPrevSkin_f(void);
void CG_ZoomDown_f(void);
void CG_ZoomUp_f(void);
void CG_AddBufferedSound(sfxHandle_t sfx);
void CG_ResetAnnouncements(void);
void CG_PlayFightSound(void);
void CG_PlayCountdownSound(int count);

void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback);
void CG_CloseMenus(void);
void CG_HideJoinMenu(void);
void CG_UpdateJoinMenu(void);
void CG_CacheMenus(void);
void CG_DrawSpectatorHud(void);
void CG_DrawSpectator(float x, float y);
qboolean CG_GetLevelTimerMsec(int* msec);

void CG_ParseDuelScores(void);
void CG_ParseAccuracy(void);

void CG_AdjustFrom640(float* x, float* y, float* w, float* h);
void CG_SetWidescreen(int mode);
void CG_FillRect(float x, float y, float width, float height, const float* color);
void CG_DrawPic(float x, float y, float width, float height, qhandle_t hShader);
void CG_DrawString(float x, float y, const char* string, float charWidth, float charHeight, const float* modulate);

void CG_DrawStringExt(int x, int y, const char* string, const float* setColor, qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars);
void CG_DrawBigString(int x, int y, const char* s, float alpha);
void CG_DrawBigStringColor(int x, int y, const char* s, vec4_t color);
void CG_DrawSmallString(int x, int y, const char* s, float alpha);
void CG_DrawSmallStringColor(int x, int y, const char* s, vec4_t color);

int CG_DrawStrlen(const char* str);

float* CG_FadeColor(int startMsec, int totalMsec);
float* CG_TeamColor(int team);
void CG_TileClear(void);
void CG_ColorForHealth(vec4_t hcolor);
void CG_GetColorForHealth(int health, int armor, vec4_t hcolor);

void UI_DrawProportionalString(int x, int y, const char* str, int style, vec4_t color);
void CG_DrawRect(float x, float y, float width, float height, float size, const float* color);
void CG_DrawSides(float x, float y, float w, float h, float size);
void CG_DrawTopBottom(float x, float y, float w, float h, float size);

extern int sortedTeamPlayers[TEAM_MAXOVERLAY];
extern int numSortedTeamPlayers;
extern char systemChat[256];
extern char teamChat1[256];
extern char teamChat2[256];

void CG_AddLagometerFrameInfo(void);
void CG_AddLagometerSnapshotInfo(snapshot_t* snap);
void CG_CenterPrint(const char* str, int y, float scale);
void CG_DrawHead(float x, float y, float w, float h, int clientNum, vec3_t headAngles, const byte* color);
void CG_DrawActive(stereoFrame_t stereoView);
void CG_DrawFlagModel(float x, float y, float w, float h, int team, qboolean force2D);
void CG_DrawTeamBackground(int x, int y, int w, int h, float alpha, int team);
void CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int fontIndex);
void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style);
int CG_Text_Width(const char* text, float scale, int limit);
int CG_Text_Height(const char* text, float scale, int limit);
void CG_Text_Paint_Font(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, fontInfo_t* font);
float CG_Text_Width_Font(const char* text, float scale, int limit, fontInfo_t* font);
float CG_Text_Height_Font(const char* text, float scale, int limit, fontInfo_t* font);
void CG_DrawText_DC(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, int fontIndex);
void CG_DrawText(float x, float y, int fontIndex, float scale, vec4_t color, const char *text, float adjust, int maxChars, int textStyle);
int CG_DrawTextWidth(const char *text, float scale, int limit, int fontIndex);
int CG_DrawTextHeight(const char *text, float scale, int limit, int fontIndex);
int CG_DrawTextLineHeight(float scale, int fontIndex);
float CG_DrawTextCapHeight(float scale, int fontIndex);
void CG_DrawTextClipped(float *maxX, float x, float y, float scale, const vec4_t color, const char *text);
extern vmCvar_t cg_redTeamName;
extern vmCvar_t cg_blueTeamName;
extern vmCvar_t cg_chatHistoryLength;
extern vmCvar_t cg_drawFragMessages;
extern vmCvar_t cg_cameraOrbit;
extern vmCvar_t g_training;
extern vmCvar_t g_skipTrainingEnable;
extern vmCvar_t cg_drawPregameMessages;
extern vmCvar_t cg_ignoreMouseInput;
extern int cg_currentWidescreen;
extern qboolean cg_letterboxDrawn;
float CG_TextWidth_DC(const char* text, float scale, int limit, int fontIndex);
float CG_TextHeight_DC(const char* text, float scale, int limit, int fontIndex);
void CG_DrawTextWithCursor_DC(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style, int fontIndex);
void CG_SelectPrevPlayer(void);
void CG_SelectNextPlayer(void);
float CG_GetValue(int ownerDraw);
qboolean CG_OwnerDrawVisible(int flags, int flags2);
void CG_RunMenuScript(char** args);
void CG_ShowResponseHead(void);
void CG_SetPrintString(int type, const char* p);
void CG_InitTeamChat(void);
void CG_GetTeamColor(vec4_t* color);
const char* CG_GetGameStatusText(void);
const char* CG_GetMatchStatusText(void);
const char* CG_GetKillerText(void);
void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles, const byte* color);
void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader);
void CG_CheckOrderPending(void);
const char* CG_GameTypeString(void);
qboolean CG_YourTeamHasFlag(void);
qboolean CG_OtherTeamHasFlag(void);
qhandle_t CG_StatusHandle(int task);

void CG_Player(centity_t* cent);
void CG_ResetPlayerEntity(centity_t* cent);
void CG_AddRefEntityWithPowerups(refEntity_t* ent, entityState_t* state, int team);
void CG_NewClientInfo(int clientNum);
sfxHandle_t CG_CustomSound(int clientNum, const char* soundName);

qboolean CG_ShouldForceTeamSkin(int viewerTeam, int playerTeam);
void CG_DomPointMarker(centity_t* cent);
void CG_Draw3DPlayerModel(int clientNum, int torsoAnim, float x, float y, float w, float h, int weapon,
                          float animFraction, const vec3_t angles);
void CG_ResolveBodySkinName(clientInfo_t* ci, qboolean modelForced, qboolean skinForced);
void CG_ResolveHeadSkinName(clientInfo_t* ci, qboolean modelForced, qboolean skinForced);
void CG_UpdateAllModelScales(void);

void CG_BuildSolidList(void);
int CG_PointContents(const vec3_t point, int passEntityNum);
void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask);
void CG_CapsuleTrace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask);
void CG_PredictPlayerState(void);
void CG_LoadDeferredPlayers(void);
void CG_ReloadOtherClients(void);
void CG_SetEndScoreboardMenu(void);

void CG_CheckEvents(centity_t* cent);
const char* CG_PlaceString(int rank);
void CG_EntityEvent(centity_t* cent, vec3_t position);
void CG_PainEvent(centity_t* cent, int health);
void CG_SpecItemTimerEvent(entityState_t* es);
void CG_UpdateSpecItemTimers(void);
void CG_ResetSpecItemTimers(void);
void CG_PlayIntermissionSound(void);
void CG_PlayMatchStateSound(int state);

void CG_SetEntitySoundPosition(centity_t* cent);
void CG_AddPacketEntities(void);
void CG_Beam(centity_t* cent);
void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out);

void CG_PositionEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName);
void CG_PositionRotatedEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName);

void CG_NextWeapon_f(void);
void CG_PrevWeapon_f(void);
void CG_Weapon_f(void);

void CG_RegisterWeapon(int weaponNum);
int CG_WeaponForShortName(const char* name);
void CG_RegisterItemVisuals(int itemNum);

void CG_FireWeapon(centity_t* cent);
void CG_MissileHitWall(int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType);
void CG_MissileHitPlayer(int weapon, vec3_t origin, vec3_t dir, int entityNum);
void CG_ShotgunFire(entityState_t* es);
void CG_Bullet(vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum);

void CG_RailTrail(clientInfo_t* ci, vec3_t start, vec3_t end);
typedef enum { BRIGHTWEAPON_ON_MAP, BRIGHTWEAPON_OWN_VIEW, BRIGHTWEAPON_CARRIED } brightWeapon_t;
void CG_BrightWeapon(refEntity_t* ent, int weaponNum, int owner, brightWeapon_t kind);
void CG_GrappleTrail(centity_t* owner, const centity_t* hook, const vec3_t start);
void CG_MissileHitWall_DmgThrough(vec3_t origin, vec3_t dir, int weapon);
void CG_ShotgunKillEffect(centity_t* cent);
void CG_AddViewWeapon(playerState_t* ps);
void CG_AddPlayerWeapon(refEntity_t* parent, playerState_t* ps, centity_t* cent, int team);
void CG_AddEffectLight(const vec3_t org, float intensity, float r, float g, float b);
void CG_DrawWeaponSelect(void);
void CG_DrawWeaponBar(void);

void CG_OutOfAmmoChange(void);
void CG_DropWeaponChange(int dropped);

void CG_InitMarkPolys(void);
void CG_AddMarks(void);
void CG_ImpactMark(qhandle_t markShader,
                   const vec3_t origin,
                   const vec3_t dir,
                   float orientation,
                   float r,
                   float g,
                   float b,
                   float a,
                   qboolean alphaFade,
                   float radius,
                   qboolean temporary);

void CG_InitLocalEntities(void);
localEntity_t* CG_AllocLocalEntity(void);
void CG_AddLocalEntities(void);

localEntity_t* CG_SmokePuff(const vec3_t p,
                            const vec3_t vel,
                            float radius,
                            float r,
                            float g,
                            float b,
                            float a,
                            float duration,
                            int startTime,
                            int fadeInTime,
                            int leFlags,
                            qhandle_t hShader);
localEntity_t* CG_SmokePuff_ql(const vec3_t p,
                               const vec3_t vel,
                               float radius,
                               float r,
                               float g,
                               float b,
                               float a,
                               float duration,
                               int startTime,
                               int fadeInTime,
                               int leFlags,
                               qhandle_t hShader);
void CG_BubbleTrail(vec3_t start, vec3_t end, float spacing);
void CG_SpawnEffect(vec3_t org);
void CG_KamikazeEffect(vec3_t org);
void CG_ObeliskExplode(vec3_t org, int entityNum);
void CG_ObeliskPain(vec3_t org);
void CG_InvulnerabilityImpact(vec3_t org, vec3_t angles);
void CG_InvulnerabilityJuiced(vec3_t org);
void CG_LightningBoltBeam(vec3_t start, vec3_t end);

void CG_ScorePlum(int client, vec3_t org, int score);
void CG_DamagePlum(int damage, int weapon, vec3_t org);

floatingEffect_t* CG_AllocFloatingEffect(void);
qboolean CG_ProjectFloatingEffect(const vec3_t point, float* x, float* y);
void CG_HealthGradientColor(int clientNum, int health, vec3_t out);
void CG_UpdateFloatingEffects(void);
void CG_DrawFloatingEffects(void);

void CG_ThawPlayer(const vec3_t origin);
void CG_IceTrail(localEntity_t* le);
void CG_SpawnIceShard(const vec3_t origin, const vec3_t velocity, qhandle_t hModel);
void CG_LightningDischargeEffect(int intensity);
void CG_BloodSplat(vec3_t origin);
void CG_PlayBloodSound(vec3_t origin);

void CG_GibPlayer(vec3_t playerOrigin, vec3_t dir);
qhandle_t CG_ElectroGibSparkShader(vec4_t color);
void CG_BigExplode(vec3_t playerOrigin);

void CG_Bleed(vec3_t origin, int entityNum);
void CG_BloodSplatEffect(vec3_t origin, int entityNum);
void CG_SpawnParticleEffect(vec3_t origin, vec3_t vel, float size, float r, float g, float b, float a, float lifetime, int startTime, int flags, qhandle_t shader);
void CG_SpecAutoFollow(int clientNum, int mode);
void CG_AddGameEvent(int clientNum, qhandle_t icon, const char* what);
void CG_ReadResolution(void);
void CG_SpecFollowKiller(int clientNum);

localEntity_t* CG_MakeExplosion(vec3_t origin, vec3_t dir, qhandle_t hModel, qhandle_t shader, int msec, qboolean isSprite);

void CG_ProcessSnapshots(void);
void CG_DemoRewind(int snapshotNum, int serverCommandSequence);

void CG_LoadingString(const char* s);
void CG_LoadingItem(int itemNum);
void CG_LoadingClient(int clientNum);
void CG_DrawInformation(void);

qboolean CG_DrawOldScoreboard(void);
void CG_DrawTourneyScoreboard(void);

qboolean CG_ConsoleCommand(void);
void CG_InitConsoleCommands(void);
void CG_ScoresDown_f(void);
void CG_ClearChat(void);
void CG_AddChat(const char *text, int teamOnly, int extraTime);
void CG_DrawChat(float x, float y, float w, float h, float scale);
void CG_InitColorWheel(void);

void CG_ExecuteNewServerCommands(int latestSequence);
void CG_ParseServerinfo(void);
void CG_SetConfigValues(void);
void CG_ParsePmoveParams(void);
void CG_ParseConfigParams(void);
void CG_ParseWeaponInfo(void);
void CG_ShaderStateChanged(void);
void CG_LoadVoiceChats(void);
void CG_VoiceChatLocal(int clientNum, const char* cmd);
void CG_PlayBufferedVoiceChats(void);

void CG_ParseTeamStats_TDM(void);
void CG_ParseTeamStats_CA(void);
void CG_ParseTeamStats_CTF(void);
void CG_InitScores(void);

void CG_PlayVoiceChat(bufferedVoiceChat_t* vchat);
void CG_AddBufferedVoiceChat(bufferedVoiceChat_t* vchat);
qboolean CG_GetVoiceChat(const char* cmd, sfxHandle_t* snd, const char** chat);
int CG_ParseVoiceChats(const char* filename);
void CG_VoiceChatListForClient(int clientNum);

void CG_Respawn(void);
void CG_TransitionPlayerState(playerState_t* ps, playerState_t* ops);
void CG_PushReward(sfxHandle_t sfx, qhandle_t shader, int rewardCount);

float CG_CpmaEaseOut(float x);
float CG_CpmaFade(int start, int fadein, int fadedelay, int time);
qboolean CG_CpmaFadingOut(int start, int fadein, int fadedelay, int time);
float CG_CpmaSlide(int start, int time);

qhandle_t CG_ChatBalloonShader(void);
void CG_CheckChangedPredictableEvents(playerState_t* ps);

void trap_Print(const char* fmt);

void trap_Error(const char* fmt) __attribute__((noreturn));

int trap_Milliseconds(void);

void trap_Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags);
void trap_Cvar_RegisterRange(vmCvar_t* vmCvar, const char* varName, const char* defaultValue,
                             const char* minValue, const char* maxValue, int flags);
void trap_Cvar_Update(vmCvar_t* vmCvar);
void trap_Cvar_Set(const char* var_name, const char* value);
void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);
float trap_Cvar_VariableValue(const char* var_name);

int trap_Argc(void);
void trap_Argv(int n, char* buffer, int bufferLength);
void trap_Args(char* buffer, int bufferLength);

int trap_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode);
int trap_FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
void trap_FS_Read(void* buffer, int len, fileHandle_t f);
void trap_FS_Write(const void* buffer, int len, fileHandle_t f);
void trap_FS_FCloseFile(fileHandle_t f);
int trap_FS_Seek(fileHandle_t f, long offset, int origin);

void trap_SendConsoleCommand(const char* text);

void trap_AddCommand(const char* cmdName);
void trap_RemoveCommand(const char* cmdName);

void trap_SendClientCommand(const char* s);

void trap_UpdateScreen(void);

void trap_CM_LoadMap(const char* mapname);
int trap_CM_NumInlineModels(void);
clipHandle_t trap_CM_InlineModel(int index);
clipHandle_t trap_CM_TempBoxModel(const vec3_t mins, const vec3_t maxs);
clipHandle_t trap_CM_TempCapsuleModel(const vec3_t mins, const vec3_t maxs);
int trap_CM_PointContents(const vec3_t p, clipHandle_t model);
int trap_CM_TransformedPointContents(const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles);
void trap_CM_BoxTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void trap_CM_CapsuleTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void trap_CM_TransformedBoxTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles);
void trap_CM_TransformedCapsuleTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles);

int trap_CM_MarkFragments(int numPoints, const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t* fragmentBuffer);

void trap_S_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
void trap_S_StopLoopingSound(int entnum);

void trap_S_StartLocalSound(sfxHandle_t sfx, int channelNum);
void trap_S_ClearLoopingSounds(qboolean killall);
void trap_S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void trap_S_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void trap_S_UpdateEntityPosition(int entityNum, const vec3_t origin);

void trap_S_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater);
sfxHandle_t trap_S_RegisterSound(const char* sample, qboolean compressed);
void trap_S_StartBackgroundTrack(const char* intro, const char* loop);
void trap_S_StopBackgroundTrack(void);
void trap_S_ClearSoundBuffer(void);

void trap_R_LoadWorldMap(const char* mapname);

qhandle_t trap_R_RegisterModel(const char* name);
qhandle_t trap_R_RegisterSkin(const char* name);
qhandle_t trap_R_RegisterShader(const char* name);
qhandle_t trap_R_RegisterShaderNoMip(const char* name);

void trap_R_ClearScene(void);
void trap_R_AddRefEntityToScene(const refEntity_t* re);

void trap_R_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts);
void trap_R_AddPolysToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int numPolys);
void trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void trap_R_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
int trap_R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
void trap_R_RenderScene(const refdef_t* fd);
void trap_R_BeginBloomScene(void);
void trap_R_ApplyBloom(void);
void trap_R_SetColor(const float* rgba);
void trap_R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs);
int trap_R_LerpTag(orientation_t* tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char* tagName);
void trap_R_RemapShader(const char* oldShader, const char* newShader, const char* timeOffset);
qboolean trap_R_inPVS(const vec3_t p1, const vec3_t p2);

void trap_GetGlconfig(glconfig_t* glconfig);

void trap_GetGameState(gameState_t* gamestate);

void trap_GetCurrentSnapshotNumber(int* snapshotNumber, int* serverTime);

qboolean trap_GetSnapshot(int snapshotNumber, snapshot_t* snapshot);

qboolean trap_GetServerCommand(int serverCommandNumber);

int trap_GetCurrentCmdNumber(void);

qboolean trap_GetUserCmd(int cmdNumber, usercmd_t* ucmd);

void trap_SetUserCmdValue(int stateValue, int weaponPrimary, float sensitivityScale, int fov);

void testPrintInt(char* string, int i);
void testPrintFloat(char* string, float f);

int trap_MemoryRemaining(void);
void trap_R_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font);
void trap_R_Font_DrawString(int x, int y, const char* text, int fontIndex, float scale, int limit, float* maxX, int flags);
void trap_R_Font_TextExtents(const char* text, int start, int fontIndex, float scale, int limit, float* extents);
void trap_R_GetGlyphInfo(int fontIndex, int charValue, glyphInfo_t* glyph);
void trap_IME_SetCompositionFont(int fontIndex, float scale);
int trap_R_Font_Register(const char* name);
void CG_DrawTextInFont(float x, float y, int engineFont, float scale, const vec4_t color, const char* text, int style);
void CG_MeasureTextInFont(const char* text, int engineFont, float scale, float* w, float* h);
void trap_S_StartLocalSoundVolume(sfxHandle_t sfx, int channelNum, float volume);
qboolean trap_Key_IsDown(int keynum);
int trap_Key_GetCatcher(void);
void trap_Key_SetCatcher(int catcher);
int trap_Key_GetKey(const char* binding);
void trap_Key_KeynumToStringBuf(int keynum, char* buf, int buflen);
void trap_Key_GetBindingBuf(int keynum, char* buf, int buflen);
qboolean trap_VoiceIsIgnored(uint64_t steamId);
qboolean trap_VoiceToggleIgnore(uint64_t steamId);

void CG_KeyNameForCommand(const char* command, char* buf, int buflen);

void trap_Get_Advertisements(int* num, float* verts, char shaders[][MAX_QPATH]);

typedef enum {
    SYSTEM_PRINT,
    CHAT_PRINT,
    TEAMCHAT_PRINT
} q3print_t;

int trap_CIN_PlayCinematic(const char* arg0, int xpos, int ypos, int width, int height, int bits);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic(int handle);
void trap_CIN_DrawCinematic(int handle);
void trap_CIN_SetExtents(int handle, int x, int y, int w, int h);

int trap_RealTime(qtime_t* qtime);
void trap_SnapVector(float* v);

qboolean trap_loadCamera(const char* name);
void trap_startCamera(int time);
qboolean trap_getCameraInfo(int time, vec3_t* origin, vec3_t* angles);

qboolean trap_GetEntityToken(char* buffer, int bufferSize);

void CG_ClearParticles(void);
void CG_AddParticles(void);
void CG_ParticleSnow(qhandle_t pshader, vec3_t origin, vec3_t origin2, int turb, float range, int snum);
void CG_ParticleSmoke(qhandle_t pshader, centity_t* cent);
void CG_AddParticleShrapnel(localEntity_t* le);
void CG_ParticleSnowFlurry(qhandle_t pshader, centity_t* cent);
void CG_ParticleBulletDebris(vec3_t org, vec3_t vel, int duration);
void CG_ParticleSparks(vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void CG_ParticleDust(centity_t* cent, vec3_t origin, vec3_t dir);
void CG_ParticleMisc(qhandle_t pshader, vec3_t origin, int size, int duration, float alpha);
void CG_ParticleExplosion(char* animStr, vec3_t origin, vec3_t vel, int duration, int sizeStart, int sizeEnd);
extern qboolean initparticles;
int CG_NewParticleArea(int num);
