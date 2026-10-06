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
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"

// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#define POWERUP_BLINKS 5

#define POWERUP_BLINK_TIME 1000
#define FADE_TIME 200
#define PULSE_TIME 200
#define DAMAGE_DEFLECT_TIME 100
#define DAMAGE_RETURN_TIME 400
#define DAMAGE_TIME 500
#define LAND_DEFLECT_TIME 150
#define LAND_RETURN_TIME 300
#define STEP_TIME 100  // [QL] 0x10073bd0, half of Quake 3's
#define DUCK_TIME 100
#define PAIN_TWITCH_TIME 200
#define WEAPON_SELECT_TIME 1400
#define ITEM_SCALEUP_TIME 1000
#define ITEM_BLOB_TIME 200
#define MUZZLE_FLASH_TIME 20
#define SINK_TIME 1000  // time for fragments to sink into ground before going away
#define ATTACKER_HEAD_TIME 10000
#define REWARD_TIME 2000  // [QL] 0x1000ac48

#define PULSE_SCALE 1.5  // amount to scale up the icons when activating

#define MAX_STEP_CHANGE 32

#define MAX_VERTS_ON_POLY 10
#define MAX_MARK_POLYS 256

#define STAT_MINUS 10  // num frame for '-' stats digit

#define ICON_SIZE 48
#define CHARACTER_WIDTH 32
#define CHAR_HEIGHT 48
#define TEXT_ICON_SPACE 4

#define TEAMCHAT_WIDTH 80
#define TEAMCHAT_HEIGHT 8

// very large characters
#define GIANT_WIDTH 32
#define GIANT_HEIGHT 48

#define NUM_CROSSHAIRS 30  // [QL] cgamex86.dll CG_RegisterGraphics @0x10022f40 registers 30; keep in sync with ui_shared.h

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
    FOOTSTEP_SNOW,   // [QL]
    FOOTSTEP_WOOD,   // [QL]

    FOOTSTEP_TOTAL
} footstep_t;

typedef enum {
    IMPACTSOUND_DEFAULT,
    IMPACTSOUND_METAL,
    IMPACTSOUND_FLESH
} impactSound_t;

//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
typedef struct {
    int oldFrame;
    int oldFrameTime;  // time when ->oldFrame was exactly on

    int frame;
    int frameTime;  // time when ->frame will be exactly on

    float backlerp;

    float yawAngle;
    qboolean yawing;
    float pitchAngle;
    qboolean pitching;

    int animationNumber;  // may include ANIM_TOGGLEBIT
    animation_t* animation;
    int animationTime;  // time when the first frame of the animation will be exact
} lerpFrame_t;

typedef struct {
    lerpFrame_t legs, torso, flag;
    int lastPainTime;  // [QL] cent+0x288: every pain event, unthrottled (0x10019ca6)
    int painTime;
    int painDirection;  // flip from 0 to 1
    int lightningFiring;
    qboolean continuousFiring;  // [QL] cent+0x298: firing while the watched weapon is MG, PG, CG or HMG
    vec3_t railgunImpact;   // [QL] cent+0x29c
    qboolean railgunFlash;  // [QL] cent+0x2a8; nothing sets it, as in Q3 (see CG_SpawnRailTrail)

    // machinegun spinning
    float barrelAngle;
    int barrelTime;
    qboolean barrelSpinning;
} playerEntity_t;

//=================================================

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
typedef struct centity_s {
    entityState_t currentState;  // from cg.frame
    entityState_t nextState;     // from cg.nextFrame, if available
    qboolean interpolate;        // true if next is valid to interpolate to
    qboolean currentValid;       // true if cg.frame holds this entity

    int muzzleFlashTime;  // move to playerEntity?
    int previousEvent;
    int teleportFlag;

    int trailTime;  // so missile trails can handle dropped initial packets
    int leadTrTime;  // [ioquakelive] pos.trTime when the entity appeared (CG_ProjectileLeadOffset)
    int dustTrailTime;
    int miscTime;

    int snapShotTime;  // last time this entity was found in a snapshot

    playerEntity_t pe;

    int errorTime;  // decay the error from this time
    vec3_t errorOrigin;
    vec3_t errorAngles;

    qboolean extrapolated;  // false if origin / angles is an interpolation
    vec3_t rawOrigin;
    vec3_t rawAngles;

    vec3_t beamEnd;

    // exact interpolated position of entity on this frame
    vec3_t lerpOrigin;
    vec3_t lerpAngles;
} centity_t;

//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independently from all server transmitted entities

typedef struct markPoly_s {
    struct markPoly_s *prevMark, *nextMark;
    int time;
    qhandle_t markShader;
    qboolean alphaFade;  // fade alpha instead of rgb
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
    LEF_PUFF_DONT_SCALE = 0x0001,  // do not scale size over time
    LEF_TUMBLE = 0x0002,           // tumble over time, used for ejecting shells
    LEF_SOUND1 = 0x0004,           // sound 1 for kamikaze
    LEF_SOUND2 = 0x0008,           // sound 2 for kamikaze
    // [wolfcam] LEF_PUFF_DONT_SCALE only stops the growth in
    // CG_AddMoveScaleFade; CG_AddFallScaleFade has its own copy of that
    // code and ignores it, so wolfcam added a second flag that the fall
    // path honours. The electrogib sparks set both, because nothing in
    // either function stops a later change of leType from routing them
    // to the other handler.
    LEF_PUFF_DONT_SCALE_NOT_KIDDING_MAN = 0x0010
} leFlag_t;

typedef enum {
    LEMT_NONE,
    LEMT_BURN,
    LEMT_BLOOD,
    // [wolfcam] The electrogib sphere. It scorches rather than bleeds, and
    // CG_AddFragment also keys the spark trail off this value, so clearing
    // the mark on impact ends the trail too.
    LEMT_ELECTROGIB,
    // [QL] 0x1001efd6: the shards of a thawed player. They leave an ice trail as
    // they fly.
    LEMT_ICE
} leMarkType_t;  // fragment local entities can leave marks on walls

typedef enum {
    LEBS_NONE,
    LEBS_BLOOD,
    LEBS_BRASS,
    // [wolfcam] The electrogib sphere: one of the four electrogib_bounce
    // samples, half the time. Quake Live has no flesh impacts to fall
    // through from - the gore that would play them is DLC.
    LEBS_ELECTROGIB
} leBounceSoundType_t;  // fragment local entities can make sounds on impacts

typedef struct localEntity_s {
    struct localEntity_s *prev, *next;
    leType_t leType;
    int leFlags;

    int startTime;
    int endTime;
    int fadeInTime;

    float lifeRate;  // 1.0 / (endTime - startTime)

    trajectory_t pos;
    trajectory_t angles;

    float bounceFactor;  // 0.0 = no bounce, 1.0 = perfect

    float color[4];

    float radius;

    float light;
    vec3_t lightColor;

    leMarkType_t leMarkType;  // mark to leave on fragment impact
    leBounceSoundType_t leBounceSoundType;

    refEntity_t refEntity;
} localEntity_t;

//======================================================================

// [QL] shared floating-effect pool (cgamex86.dll base DAT_10ab9978, 32 records of 0x80
// bytes each). One pool of screen-projected billboards feeds all of QL's floating
// player/world effects: damage numbers, player outlines, freeze/flag glows and head
// float-sprites. Allocated by CG_AllocFloatingEffect (binary "CG_AllocMark"
// @ 0x1002a0d0 - a mislabelled Ghidra name, not the Q3 wall-mark
// allocator in cg_marks.c), aged and expired each frame by CG_UpdateFloatingEffects
// (binary "CG_UpdateConfigStrings" @ 0x1002a190 - also mislabelled; it ages this pool,
// it is not a config-string updater) and rendered by
// CG_DrawFloatingEffects (binary "CG_DrawDamagePlums" @ 0x10011680 - draws the whole
// pool, not only plums). The struct mirrors the binary's 0x80-byte record field order.
#define MAX_FLOATING_EFFECTS 32

typedef enum {
    FE_DAMAGE_NUMBER = 0,  // floating damage plum (drawn as text)
    FE_ENTITY_POI    = 1,  // a marker over an entity (the domination points)
    FE_POI           = 2,  // a marker at a point: flags, capture points
    FE_SPRITE        = 3,  // generic world sprite (CG_General/Item/Obituary/EntityEvent)
    FE_FLOAT_SPRITE  = 4   // player head float-sprite (medals, talk, connection icons)
} floatingEffectType_t;

typedef struct {
    int       type;        // 0x00  floatingEffectType_t
    void*     owner;       // 0x04  originating centity_t* (0 if none)
    vec3_t    origin;      // 0x08  world origin
    vec4_t    color;       // 0x14  RGBA; alpha aged by CG_UpdateFloatingEffects
    qhandle_t shader;      // 0x24  billboard pic (0 = none; damage numbers draw text)
    int       clientNum;   // 0x28  FE_SPRITE viewer-cull key
    int       followNum;   // 0x2c  FE_SPRITE: the team it belongs to (0x100116c7 hides it from that team)
    int       spawnTime;   // 0x30  cg.time at allocation
    float     lifeRate;    // 0x34  lifetime in ms (default 1.0; damage number 1000.0)
    int       pad_38;      // 0x38  unused
    float     worldSize;   // 0x3c  world height used for perspective pixel scaling
    int       pad_40;      // 0x40  unused
    float     minPixels;   // 0x44  minimum on-screen pixel size (floor)
    int       pad_48;      // 0x48  unused
    float     zOffset;     // 0x4c  world Z lift above origin (outline 80, freeze 64, sprite 48)
    float     fadeStart;   // 0x50  ms after spawn when the alpha fade begins
    char      text[16];    // 0x54  damage-number string
    float     velX;        // 0x64  screen X drift, linear in life fraction
    float     velY;        // 0x68  screen Y drift, linear in life fraction
    float     accX;        // 0x6c  screen X drift, * life^2
    float     accY;        // 0x70  screen Y drift, * life^2
    int       doFade;      // 0x74  1 = run the fadeStart alpha fade
    int       pad_78;      // 0x78  unused
    int       active;      // 0x7c  slot in use flag
} floatingEffect_t;

// [QL] a spectator item timer (cgamex86.dll 0x10ab97e4, records of 0x24 bytes), filled by
// EV_ITEM_PICKUP_SPEC (0x10019d90)
#define MAX_SPEC_ITEM_TIMERS 10

typedef struct {
    int    entityNum;  // the item entity; 0 is a free record
    int    side;       // CTF: 1 or 2 for the base it is nearer to, 3 for an item with no duplicate
    int    itemIndex;
    int    time;       // ms until it is back, counted down each frame
    int    distance;   // CTF: squared distance to that base
    int    team;       // in a random-respawn team: 1, or 2 once it has respawned
    vec3_t origin;
} specItemTimer_t;

//======================================================================

typedef struct {
    int client;
    int score;
    int roundScore;     // [QL] for RR gametype
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

    // [QL] extended fields
    qboolean alive;
    int frags;
    int deaths;
    int bestWeapon;
    int bestWeaponAccuracy;
    int damageDone;
    int net;            // frags - deaths
    int tks;            // team kills
    int tkd;            // team kill deaths
    int thaws;          // FT: thaw count

    // [QL] extended team-stats fields written by tdmstats/castats/ctfstats verbs
    // (binary score_t byte offsets in comments; cgame-internal layout so exact offsets need not match)
    int selfKills;               // 0xd0
    int damageReceived;          // 0xf0 (damageDone == 0xec already above)
    int pickups_ra;              // 0xf4  red armour
    int pickups_ya;              // 0xfc  yellow armour
    int pickups_ga;              // 0x104 green armour
    int pickups_mh;              // 0x10c mega health
    int pickups_quad;            // 0x114 quad
    int pickups_bs;              // 0x118 battlesuit
    int pickups_regen;           // 0x11c regen  (CTF verb only)
    int pickups_haste;           // 0x120 haste  (CTF verb only)
    int pickups_invis;           // 0x124 invis  (CTF verb only)
    int weaponKills[MAX_WEAPONS];    // 0x12c 15 ints (index 0=gauntlet..14). CA verb. MAX_WEAPONS must be >= 15.
    int weaponAccuracy[MAX_WEAPONS]; // 0x16c 15 ints. CA verb. NOTE quirk: CaStats reads HMG kills from [13] but HMG accuracy from [14].
} score_t;

// [QL] per-weapon stats for duel scoreboard
typedef struct {
    int hits;
    int atts;       // attempts (shots)
    int accuracy;
    int damage;
    int kills;
} duelWeaponStats_t;

// [QL] per-player duel score (populated by "scores" command in duel mode)
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

// [QL] a team's pickups, the header scores_tdm, scores_ft and scores_ctf send ahead of the
// players. Retail keeps both teams in one block (cgamex86.dll 0x10aa6864 red, 0x10aa68a8
// blue); the flag and medkit fields only come in CTF. Times held are in seconds.
typedef struct {
    int ra, ya, ga, mh, quad, bs, regen, haste, invis, flag, medkit;
    int quadTime, bsTime, regenTime, hasteTime, invisTime, flagTime;
} teamPickups_t;

// [QL] per-player weapon accuracy (from "acc" command)
typedef struct {
    int accuracy[MAX_WEAPONS];
    int time;       // when received
    int clientNum;  // which client
    qboolean valid;
} accuracyStats_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define MAX_CUSTOM_SOUNDS 32

typedef struct {
    qboolean infoValid;

    // [QL] CG_NewClientInfo (0x1003e640) fills these from the player's configstring
    char name[MAX_QPATH];      // the clan tag "cn", "^7 " and the name "n"
    char clanTag[MAX_QPATH];   // "^7", the clan tag "cn" and "^7"
    char clanName[MAX_QPATH];  // "xcn", the clan's full name
    char country[MAX_QPATH];   // "c"
    team_t team;
    uint64_t steamId;  // steam id for this client ("st")

    // [QL] 0x10020a40: the engine sets these when voice arrives from this player
    qboolean talking;
    int talkingTime;

    int botSkill;         // 0 = not bot, 1-5 = bot
    float botSkillValue;  // [QL] the same "skill" key read as a float
    int privileges;       // [QL] "p"
    int specOnly;         // [QL] "so"
    int playQueue;        // [QL] "pq"

    vec3_t color1;
    vec3_t color2;

    byte c1RGBA[4];
    byte c2RGBA[4];

    int score;     // updated by score servercmds
    int location;  // location index for team mode
    int health;    // you only get this info about your teammates
    int armor;
    int curWeapon;

    int handicap;
    int wins, losses;  // in tourney mode

    int teamTask;         // task in teamplay (offence/defence)
    qboolean teamLeader;  // true when this is a team leader
    qboolean ready;       // [QL] "rp" in the player's configstring: readied up for the match

    int powerups;  // so can display quad/flag status

    int medkitUsageTime;
    int invulnerabilityStartTime;
    int invulnerabilityStopTime;

    // when clientinfo is changed, the loading of models/skins/sounds
    // can be deferred until you are dead, to prevent hitches in
    // gameplay
    char modelName[MAX_QPATH];
    char skinName[MAX_QPATH];
    char headModelName[MAX_QPATH];
    char headSkinName[MAX_QPATH];
    // [QL] forced head/secondary model+skin written by CG_ResolveModelForClient and
    // resolved by CG_ResolveHeadSkinName (binary offsets 0x254 / 0x294). Despite the
    // ql-decompiled header naming them redTeam/blueTeam, these are not team-name strings.
    char forcedHeadModel[MAX_QPATH];  // 0x254
    char forcedHeadSkin[MAX_QPATH];   // 0x294
    qboolean deferred;

    qboolean newAnims;    // true if using the new mission pack animations
    qboolean fixedlegs;   // true if legs yaw is always the same as torso yaw
    qboolean fixedtorso;  // true if torso never changes yaw

    vec3_t headOffset;  // move head in icon views
    footstep_t footsteps;
    gender_t gender;  // from model

    qhandle_t legsModel;
    qhandle_t legsSkin;

    qhandle_t torsoModel;
    qhandle_t torsoSkin;

    qhandle_t headModel;
    qhandle_t headSkin;

    qhandle_t modelIcon;

    float modelScale;  // QL: bounding box scale factor (cg_scalePlayerModelsToBB)

    animation_t animations[MAX_TOTALANIMATIONS];

    sfxHandle_t sounds[MAX_CUSTOM_SOUNDS];
} clientInfo_t;

// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s {
    qboolean registered;
    gitem_t* item;

    qhandle_t handsModel;  // the hands don't actually draw, they just position the weapon
    qhandle_t weaponModel;
    qhandle_t barrelModel;
    qhandle_t flashModel;

    vec3_t weaponMidpoint;  // so it will rotate centered instead of by tag

    float flashDlight;
    vec3_t flashDlightColor;
    sfxHandle_t flashSound[4];  // fast firing weapons randomly choose

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
    // [QL] the weapon's third sound, +0x1c0 in the binary's weaponInfo_t: the grapple's is
    // grpull. It loops instead of firingSound while the hook holds (0x100524ad)
    sfxHandle_t pullSound;
} weaponInfo_t;

// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
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

#define MAX_REWARDSTACK 16  // [QL] 0x10043570 pushes while rewardStack < 15
#define MAX_SOUNDBUFFER 32  // [QL] 0x1004e110 wraps the announcer queue at 32
#define SOUNDBUFFER_DELAY 1500  // [QL] 0x1004e130: what each announcement holds back the next one

// [QL] a chat line; the newest one and the ring of older ones share this layout
#define CHAT_LINE_TEXT 256
typedef struct {
    int startTime;
    int endTime;
    int teamOnly;
    int fadeTime;  // [CPMA] start of its fade envelope: startTime, less a skipped fade-in
    char text[CHAT_LINE_TEXT];
} chatLine_t;

// [QL] voice chat
#define MAX_VOICEFILES 8         // parsed voice-file slots searched by CG_ParseVoiceChats
#define MAX_VOICECHATBUFFER 32   // voiceChatBuffer ring size (in/out masked modulo 32)

// [QL] maps a client model/skin string -> loaded voice-file slot index.
// Binary voiceChatLists[64] at 0x107d87c0, entry stride 0x44 (68 bytes).
// NOTE: QL's layout differs from ioquake3's voiceChatList_t - verify sizing before reuse.
typedef struct {
    char name[64];
    int  index;
} voiceChatList_t;

// [QL] one buffered incoming voice chat. 0x138 = 312 bytes; matches ioquake3's layout.
typedef struct bufferedVoiceChat_s {
    int         clientNum;
    sfxHandle_t snd;
    int         voiceOnly;
    char        cmd[150];
    char        message[150];
} bufferedVoiceChat_t;

// [QL] per-client extended team-stats row. Binary cgame stride 0x268 bytes (154 ints);
// base globals DAT_10a9cea0. Indexed by scoreboard slot (arg1 of tdmstats/castats/ctfstats).
// Field byte offsets written by the parsers:
//   TDM 0x00,0x04,0x08,0x1c,0x20,0x24,0x2c,0x34,0x3c,0x44,0x48
//   CTF adds 0x4c,0x50,0x54
//   CA  uses 0x1c,0x20 then pairs at 0x5c+i*4 and 0x9c+i*4 for i=0..14
typedef struct teamStatsRow_s {
    int fields[154];
} teamStatsRow_t;

//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS 16

typedef struct {
    int clientFrame;  // incremented each frame

    int clientNum;

    qboolean demoPlayback;
    qboolean levelShot;  // taking a level menu screenshot
    int deferredPlayerLoading;
    qboolean loading;              // don't defer players at initial startup
    qboolean intermissionStarted;  // don't play voice rewards, because game will end shortly

    // there are only one or two snapshot_t that are relevant at a time
    int latestSnapshotNum;   // the number of snapshots the client system has received
    int latestSnapshotTime;  // the time from latestSnapshotNum, so we don't need to read the snapshot yet

    snapshot_t* snap;      // cg.snap->serverTime <= cg.time
    snapshot_t* nextSnap;  // cg.nextSnap->serverTime > cg.time, or NULL
    snapshot_t activeSnapshots[2];

    float frameInterpolation;  // (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

    qboolean thisFrameTeleport;
    qboolean nextFrameTeleport;

    int frametime;  // cg.time - cg.oldTime

    int time;  // this is the time value that the client
    // is rendering at.
    int oldTime;  // time at last frame, used for missile trails and prediction checking

    int physicsTime;  // either cg.snap->time or cg.nextSnap->time

    int timelimitWarnings;  // 5 min, 1 min, overtime
    int fraglimitWarnings;

    qboolean mapRestart;  // set on a map restart to set back the weapon

    qboolean renderingThirdPerson;  // during deaths, chasecams, etc

    // prediction state
    qboolean hyperspace;  // true if prediction has hit a trigger_teleport
    playerState_t predictedPlayerState;
    centity_t predictedPlayerEntity;
    qboolean validPPS;  // clear until the first call to CG_PredictPlayerState
    int predictedErrorTime;
    vec3_t predictedError;

    int eventSequence;
    int predictableEvents[MAX_PREDICTED_EVENTS];

    float stepChange;  // for stair up smoothing
    int stepTime;

    float duckChange;  // for duck viewheight smoothing
    int duckTime;

    float landChange;  // for landing hard
    int landTime;

    // input state sent to server
    int weaponSelect;
    int weaponPrimaryQueued;  // [QL] 0x10a9c7a4: cg_weaponPrimaryQueued as a weapon number

    // auto rotating items
    vec3_t autoAngles;
    vec3_t autoAxis[3];
    vec3_t autoAnglesFast;
    vec3_t autoAxisFast[3];

    // view rendering
    refdef_t refdef;
    vec3_t refdefViewAngles;  // will be converted to refdef.viewaxis

    // zoom key
    qboolean zoomed;
    int zoomTime;
    float zoomSensitivity;
    int fovTarget;  // [QL] the fov sent to the server: the one being zoomed to, not the interpolated one

    // information screen text during loading
    char infoScreenText[MAX_STRING_CHARS];
    int loadingStage;  // [QL] loading progress counter (0-4)

    // scoreboard
    int scoresRequestTime;
    int numScores;
    int selectedScore;
    int teamScores[2];
    score_t scores[MAX_CLIENTS];
    qboolean showScores;
    qboolean scoreBoardShowing;

    // [QL] cg+0x2d10c. Set by CG_Init and cleared by CG_CloseMenus; while it is
    // up CG_Draw2D hands the frame to CG_UpdateJoinMenu instead of the HUD.
    // No Q3 counterpart - Q3 had no join menu, it dropped you into spectator.
    qboolean showJoinMenu;

    // [QL] team-kill complaint prompt (server "complaint" verb). complaintClient is the
    // offender clientNum (or a negative status code); complaintEndTime is when the prompt
    // expires. Consumed by CG_DrawVote (binary DAT_10a5fdc8 / DAT_10a5fdcc).
    int complaintClient;
    int complaintEndTime;

    qboolean statsShowing;     // [QL] +stats overlay active
    qboolean accShowing;       // [QL] +acc overlay active
    qboolean pstatsShowing;    // [QL] +pstats overlay active
    int      accRequestTime;   // [QL] throttle for acc server requests
    int      pstatsRequestTime;// [QL] throttle for pstats server requests
    int scoreFadeTime;
    char killerName[MAX_NAME_LENGTH];
    qhandle_t killerIcon;  // [QL] the killing weapon's icon, drawn after "Fragged by" (0x10aa69b0)
    int spectatorPaintLen;                 // current offset from start

    // skull trails
    skulltrail_t skulltrails[MAX_CLIENTS];

    // centerprinting
    int centerPrintTime;
    float centerPrintScale;  // [QL] text scale (0x10ab88c8), not a character width
    int centerPrintY;
    char centerPrint[1024];
    int centerPrintLines;
    qboolean centerPrintFrag;  // [CPMA] a frag message, faded as CPMA's FragMessage

    // low ammo warning state
    int lowAmmoWarning;  // 1 = low, 2 = empty

    // crosshair client ID
    int crosshairClientNum;
    int crosshairClientTime;

    // powerup active flashing
    int powerupActive;
    int powerupTime;

    // attacking player
    int attackerTime;
    int voiceTime;

    // [QL] CG_PredictPlayerState (0x10044748) flags a reload of the other players' forced
    // models when following starts or stops, the followed player changes or the team changes;
    // CG_TransitionSnapshot (0x1004c2ba) carries it out
    qboolean reloadOtherClients;
    int reloadFollowing;
    int reloadFollowedClient;

    // reward medals
    int rewardStack;
    int rewardTime;
    int rewardCount[MAX_REWARDSTACK];
    qhandle_t rewardShader[MAX_REWARDSTACK];
    qhandle_t rewardSound[MAX_REWARDSTACK];

    // [QL] announcer queue, 0x10ab8db4-0x10ab8f3c. An entry plays at soundBufferTime and sets
    // the time of the entry after it to its own delay from then.
    int soundBufferIn;
    int soundBufferOut;
    int soundBufferTime[MAX_SOUNDBUFFER];
    sfxHandle_t soundBuffer[MAX_SOUNDBUFFER];
    int soundBufferDelay[MAX_SOUNDBUFFER];
    qboolean soundBufferLocked;  // [QL] 0x10ab8f3c: the match result was announced, nothing more queues
    int joinMenuCloseTime;       // [QL] 0x10a9c9b0: when CG_CloseMenus last ran

    // for voice chat buffer
    int voiceChatTime;
    int voiceChatBufferIn;
    int voiceChatBufferOut;

    // [QL] deferred spectator auto-follow (CG_CheckAutoFollow, DAT_10ab97c0/4).
    // autoFollowClient == -1 means no pending follow. Distinct from cg_predict.c's
    // CG_SpecAutoFollow.
    int autoFollowClient;
    int autoFollowTime;

    // warmup countdown
    int warmup;
    int warmupCount;

    // [CPMA] cg_autoAction: whether the match is running, whether its end was dealt with
    // and whether the demo being recorded was started here
    qboolean autoActionLive;
    qboolean autoActionEnded;
    centity_t* hookEntities[MAX_CLIENTS];  // [QL] 0x10aba9a8: each client's hook, as the snapshots bring it

    // [CPMA] when the spectator camera last jumped to whoever got the frag
    int followKillerTime;

    // [ioquakelive] hits the client sounded on its own, and when the last one was
    int warmupGametype;     // [QL] gametype override from CS_WARMUP (-1 = use current)
    int warmupRound;             // [QL] CS_ROUND_WARMUP "round" (0x10ab8f58), +1 in GT_AD
    int warmupTurn;              // [QL] CS_ROUND_WARMUP "turn", GT_AD only (0x10ab8f5c)
    int warmupState;             // [QL] CS_ROUND_WARMUP "state", GT_AD only (0x10ab8f60)
    int lastAutoFireTime;        // [QL] last predicted railgun autofire (cg.time)

    //==========================

    int itemPickup;
    int itemPickupLast;       // [QL] 0x10ab8f74, the item the pickup display drew last frame
    int itemPickupCount;      // [QL] 0x10ab8f78, the same item picked up in a row ("x%i")
    int itemPickupTime;
    int itemPickupBlendTime;  // the pulse around the crosshair is timed separately
    int crosshairHitTime;     // [QL] 0x10ab8fb0, set on a hit (0x10043c2e)
    int crosshairHitType;     // [QL] 0x10ab8fb4, (generic1 >> 6) + 1

    // [CPMA] ch_animateScroll: when a list started sliding a row (0 while still) and the
    // start time of the item that set it off (see CG_CpmaSlide)
    int obitSlideStart;
    int obitSlideKey;
    int chatSlideStart;
    int chatSlideKey;
    qboolean chatSlideIn;  // the rows rise into place (a line came in) rather than sink
    // [ioquakelive] with ch_animateScroll the chat history, and the chat behind the scoreboard,
    // ease in and out: whether they were up, when that changed, how far in they were then and
    // how many lines they had
    qboolean chatHistoryWasShown;
    int chatHistoryToggle;
    float chatHistoryFrom;
    int chatHistoryRows;

    int weaponSelectTime;
    int weaponConfigSelect;  // [QL] 0x10a9c7b0: the weapon whose config ran last
    int weaponAnimation;
    int weaponAnimationTime;

    // blend blobs
    float damageTime;
    float damageX, damageY, damageValue;
    int damageType;  // [QL] hit-effect colour selector for CG_DrawDamageBlend (2=green,3=orange,4=blue,else light-blue)

    // status bar head
    float headYaw;
    float headEndPitch;
    float headEndYaw;
    int headEndTime;
    float headStartPitch;
    float headStartYaw;
    int headStartTime;

    // view movement
    float v_dmg_time;
    float v_dmg_pitch;
    float v_dmg_roll;

    // temp working variables for player view
    float bobfracsin;
    int bobcycle;
    float xyspeed;
    int nextOrbitTime;

    // qboolean cameraMode;		// if rendering from a loaded camera

    // development tool
    refEntity_t testModelEntity;
    char testModelName[MAX_QPATH];
    qboolean testGun;

    // [QL] race state, 0x10abaab0..0x10abaad4
    struct {
        qboolean active;                // 0x10abaab0: a run under way
        int      startTime;             // 0x10abaab4: server time of the start
        int      lastTime;              // 0x10abaab8: the last finish
        int      bestTime;              // 0x10abaabc: the best finish on this map
        int      currentCheckPoint;     // 0x10abaac0: only race_info fills it
        int      nextCheckpointEnt;     // 0x10abaac4: the next point (-1 none)
        int      nextNextCheckpointEnt; // 0x10abaac8: the one after it (-1 none)
        int      checkpointDiff;        // 0x10abaacc: split against the best run
        qboolean hasDiff;               // 0x10abaad0
        int      checkpointsPassed;     // 0x10abaad4: the start counts as one
    } race;

    // [QL] the spectator item timers (count at 0x10ab994c)
    specItemTimer_t specItemTimers[MAX_SPEC_ITEM_TIMERS];
    int numSpecItemTimers;

    // [QL] duel scoreboard data
    duelScore_t duelScores[2];
    int duelPlayer1;        // clientNum of 1st duel player (from CS_CLIENTNUM1STPLAYER)
    int duelPlayer2;        // clientNum of 2nd duel player (from CS_CLIENTNUM2NDPLAYER)
    qboolean duelScoresValid;

    // [QL] each team's pickups (from scores_tdm/scores_ft/scores_ctf), red then blue
    teamPickups_t teamPickups[2];

    // [QL] per-player accuracy (from "acc" command)
    accuracyStats_t accuracyStats;

    // [QL] map vote data (from CS_ROTATIONMAPS/CS_ROTATIONVOTES)
    char voteMapNames[3][MAX_QPATH];
    char voteMapTitles[3][64];
    char voteGameTypes[3][32];
    int voteCounts[3];
    qboolean mapVoteActive;

    // [QL] obituary / kill feed ring buffer (binary: 16 entries at 10ab8fec)
    #define MAX_OBITUARIES 16
    // [CPMA] GameEvents timing in CPMA 1.53's hud.cfg, used with ch_animateScroll
    #define OBIT_FADEIN 150
    #define OBIT_FADEDELAY 3000
    #define OBIT_FADEOUT 200
    struct {
        int         active;
        int         time;           // cg.time when it happened
        char        victimName[40]; // resolved + color-stripped for team games
        int         victimTeam;     // 0=free, 1=red, 2=blue, 3=spec
        char        attackerName[40];
        int         attackerTeam;
        int         hasAttacker;    // 0 for world/suicide
        qhandle_t   weaponIcon;     // shader handle for weapon/skull icon
    } obituaries[MAX_OBITUARIES];

    // [QL] speedometer history ring buffer
    #define SPEED_HISTORY_SIZE 128
    float speedHistory[SPEED_HISTORY_SIZE];
    int speedHistoryCount;
    int speedHistoryIndex;
    vec4_t speedBarColor1;   // lower bar color (green)
    vec4_t speedBarColor2;   // upper/overflow bar color (yellow)

    // [QL] chat history and misc client-side flags
    qboolean chatHistoryShowing;
    qboolean killRequested;  // [QL] 0x10abaad8: the kill command was used; race stops hinting it
    int      disconnectRequest;

    // [QL] chat ring buffer (binary: 24 entries at DAT_10079950)
    #define MAX_CHAT_LINES  24
    // [CPMA] Chat timing in CPMA 1.53's hud.cfg, used with ch_animateScroll
    #define CHAT_FADEIN 150
    #define CHAT_FADEDELAY 3000
    #define CHAT_FADEOUT 100
    chatLine_t chatLines[MAX_CHAT_LINES];
    chatLine_t currentChatLine;
    int chatIndex;

    // [QL] the spectator list the scoreboards scroll, one entry per spectator, built by
    // CG_BuildSpectatorString (0x10024ff0) and drawn by CG_DrawScrollingNotify (0x100351a0).
    // Binary: 0x40-byte stride (DAT_10aa69b4), count (DAT_10ab69b4), rolling start index
    // (DAT_10ab69bc), 4000ms advance timer (DAT_10ab69b8).
    char notifyMessages[MAX_CLIENTS][64];
    int  notifyCount;
    int  notifyScrollStart;
    int  notifyScrollTime;


    // [QL] A&D round-overlay scoreboard, written by CG_InitScores (argv 1..20), read by CG_DrawRoundOverlay.
    // Dedicated array at global 0x10aa67d0 (memset 0x50 = 20 ints); -1 means "no score".
    // not cg.scores[].client - do not overload the score_t path. teamScores[2] above holds argv(21)/argv(22).
    int adScores[20];

    // [QL] chat-recall timers. CG_LastChatCommand sets lastChatTime = cg.time + 3000;
    // CG_LastChatCommand2 sets lastChatTime2 = cg.time + 3000; both re-add cg_lastmsg via CG_AddChat.
    int lastChatTime;
    int lastChatTime2;

    // [QL] cgame key-catcher flag (global 0x10a9c9b4).
    // CG_SetKeyCatcher sets it to 1 if 0; CG_ClearKeyCatcher sets it to 0 if nonzero.
    int keyCatcherActive;

    // [QL] per-client extended team-stats array (tdmstats/castats/ctfstats), indexed by scoreboard slot.
    teamStatsRow_t teamStats[MAX_CLIENTS];

    // [QL] 0x10aba978: where each flag was last seen standing, by team (TEAM_FREE the neutral
    // one); CG_Item keeps it for the flags and CG_TeamBase for the harvester's receptacles
    vec3_t flagOrigin[3];

} cg_t;

// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
// [QL] the faces a respawn timer can wear, in the order the binary registers them
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
    qhandle_t modifiedShader;  // [QL] the mark on a modified weapon's icon

    // [QL] loading screen assets
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
    qhandle_t droppedFlagModel;   // [QL] "models/flags/d_flag.md3"
    qhandle_t flagTrackPointerShader;  // [QL] "gfx/2d/flag_status/track_pointer.tga"
    qhandle_t keyGoldModel;       // [QL] "models/powerups/keys/key_gold.md3"

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

    // gib explosions
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
    qhandle_t gibBrainBase;       // [QL] "models/gibs/brain.md3", aparte de los de dlc_gibs
    qhandle_t gibAbdomenBase;     // [QL] "models/gibs/abdomen.md3"

    qhandle_t smoke2;

    qhandle_t machinegunBrassModel;
    qhandle_t shotgunBrassModel;

    qhandle_t railRingsShader;
    qhandle_t railCoreShader;

    qhandle_t lightningShader[5];

    qhandle_t grapplingChainShader;

    qhandle_t balloonShader;
    // [CPMA] animation frames from CPMA 1.53's package, shipped in pak01; 0 without them
    qhandle_t medalFrames[6][24];
    int fragMessageFont;  // [ioquakelive] engine font of the frag message: Handel Gothic Ex, or 0
    qhandle_t chatBalloonFrames[16];
    qhandle_t connectionShader;

    qhandle_t selectShader;
    qhandle_t weaponBarHighlightShader;  // [QL] "ui/assets/hud/weaplit2.tga"
    qhandle_t weaponBarIdleShader[2];    // [QL] "ui/assets/hud/weapidle1.tga" y weapidle2
    qhandle_t infiniteAmmoShader;        // [QL] "icons/infinite.tga"
    qhandle_t viewBloodShader;
    qhandle_t viewDamageShader;  // [QL] viewDamageBlend, the screen-damage tint of a stock install
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
    qhandle_t bloodSprayShaders[4];  // [QL] bloodSpray1-4

    qhandle_t sparkParticleShader;  // QL: impact spark particles
    // [QL] the item respawn timer (gfx/2d/timer): the item's face and the ring of wedges
    qhandle_t timerIcons[TIMER_NUM_ICONS];
    qhandle_t timerSlices[4][2];  // 5, 7, 12 and 24 wedges, plain and current
    qhandle_t vignetteShader;      // QL: fullscreen vignette overlay

    qhandle_t nailPuffShader;
    qhandle_t blueProxMine;

    // [QL] obituary icons
    qhandle_t fragIconShader;   // "icons/icon_frag" - skull for world/suicide deaths
    qhandle_t thawIconShader;   // [QL] "icons/thaw", registered from Freeze Tag on (0x100238e8)

    qhandle_t numberShaders[11];

    qhandle_t shadowMarkShader;

    qhandle_t botSkillShaders[5];

    // wall mark shaders
    qhandle_t wakeMarkShader;
    qhandle_t bloodMarkShader;
    qhandle_t bulletMarkShader;
    qhandle_t crackedMarkShader;  // [QL] gfx/damage/cracked_mrk, the grapple's mark
    qhandle_t burnMarkShader;
    qhandle_t holeMarkShader;
    qhandle_t energyMarkShader;

    // powerup shaders
    qhandle_t quadShader;
    qhandle_t spawnArmorShader;     // [QL] powerups/spawnArmor, spawn protection
    qhandle_t spawnArmor2Shader;    // [QL] powerups/spawnArmor2, mientras esta recibiendo dano
    qhandle_t noPlayerClipShader;   // [QL] noPlayerClipShader, others when they do not collide
    qhandle_t gooShader;            // [QL] powerups/goo, el infectado de Red Rover
    qhandle_t xrayMaskShader;       // [ioquakelive] cg_xray, pak01's ioqlXrayMask and ioqlXray
    qhandle_t xrayShader;
    qhandle_t quadWeaponShader;
    qhandle_t brightWeaponShader;  // [CPMA] cg_drawBrightWeapons
    qhandle_t invisShader;
    qhandle_t ghostWeaponShader;  // [QL] cg_drawGun 3, el arma fantasma
    qhandle_t regenShader;
    qhandle_t battleSuitShader;
    qhandle_t battleWeaponShader;
    qhandle_t hastePuffShader;

    qhandle_t redKamikazeShader;
    qhandle_t blueKamikazeShader;

    // weapon effect models
    qhandle_t bulletFlashModel;
    qhandle_t ringFlashModel;
    qhandle_t dishFlashModel;
    qhandle_t lightningExplosionModel;

    // weapon effect shaders
    qhandle_t railExplosionShader;
    qhandle_t plasmaExplosionShader;
    qhandle_t bulletExplosionShader;
    qhandle_t rocketExplosionShader;
    qhandle_t grenadeExplosionShader;
    qhandle_t bfgExplosionShader;
    qhandle_t bloodExplosionShader;

    // special effects models
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

    // scoreboard headers
    qhandle_t scoreboardName;
    qhandle_t scoreboardPing;
    qhandle_t scoreboardScore;
    qhandle_t scoreboardTime;

    // [QL] medals/awards shown during gameplay (16 types)
    qhandle_t medalAccuracy;
    qhandle_t medalAssist;
    qhandle_t medalCapture;
    qhandle_t medalComboKill;
    qhandle_t medalDefend;       // was medalDefend
    qhandle_t medalExcellent;    // was medalExcellent
    qhandle_t medalFirstFrag;
    qhandle_t medalGauntlet;     // was medalGauntlet
    qhandle_t medalHeadshot;
    qhandle_t medalImpressive;   // was medalImpressive
    qhandle_t medalMidair;
    qhandle_t medalPerfect;
    qhandle_t medalPerforated;
    qhandle_t medalQuadGod;
    qhandle_t medalRampage;
    qhandle_t medalRevenge;

    // [QL] the scoreboards' pickup icons, registered after the medals (0x10024808)
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

    // [QL] sounds, in the order cgamex86.dll's CG_RegisterSounds (0x10020e70) loads them.
    // "vo" marks the announcer voices under sound/vo, vo_evil or vo_female.
    sfxHandle_t voDefaultSound;  // the three announcer samples
    sfxHandle_t voEvilSound;
    sfxHandle_t voFemaleSound;
    sfxHandle_t oneMinuteSound;  // vo
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

    sfxHandle_t goSound;  // race: vo
    sfxHandle_t newHighScoreSound;

    sfxHandle_t hitTeamSound;  // team modes
    sfxHandle_t returnYourTeamSound;
    sfxHandle_t returnOpponentSound;
    sfxHandle_t countPrepareTeamSound;  // vo
    sfxHandle_t redLeadsSound;
    sfxHandle_t blueLeadsSound;
    sfxHandle_t teamsTiedSound;
    sfxHandle_t redScoredSound;
    sfxHandle_t blueScoredSound;
    sfxHandle_t yourTeamScoredSound;
    sfxHandle_t enemyScoredSound;
    sfxHandle_t blueWinsSound;
    sfxHandle_t redWinsSound;

    sfxHandle_t thirtySecondWarningSound;  // round modes: vo
    sfxHandle_t roundBeginsInSound;
    sfxHandle_t redWinsRoundSound;
    sfxHandle_t blueWinsRoundSound;
    sfxHandle_t roundDrawSound;
    sfxHandle_t roundOverSound;
    sfxHandle_t survivorSound;
    sfxHandle_t lastStandingSound;  // vo

    sfxHandle_t captureAwardSound;  // flag and point modes
    sfxHandle_t captureYourTeamSound;
    sfxHandle_t captureOpponentSound;
    sfxHandle_t takenYourTeamSound;
    sfxHandle_t takenOpponentSound;
    sfxHandle_t pointNeutralizedSound;  // domination
    sfxHandle_t pointHumSound;
    sfxHandle_t pointCapturedSounds[6];  // by point, 1-5 are vo a-e
    sfxHandle_t pointLostSounds[6];
    sfxHandle_t blueFlagTakenSound;  // CTF and A&D: vo
    sfxHandle_t blueFlagReturnedSound;
    sfxHandle_t redFlagTakenSound;
    sfxHandle_t redFlagReturnedSound;
    sfxHandle_t yourFlagReturnedSound;
    sfxHandle_t enemyFlagReturnedSound;
    sfxHandle_t enemyTookYourFlagSound;
    sfxHandle_t yourTeamTookEnemyFlagSound;
    sfxHandle_t enemyTookTheFlagSound;
    sfxHandle_t yourTeamTookTheFlagSound;
    sfxHandle_t youHaveFlagSound;  // one flag, CTF and A&D: vo
    sfxHandle_t holyShitSound;
    sfxHandle_t obeliskHitSound1;  // overload
    sfxHandle_t obeliskHitSound2;
    sfxHandle_t obeliskHitSound3;
    sfxHandle_t obeliskRespawnSound;
    sfxHandle_t yourBaseIsUnderAttackSound;  // vo
    sfxHandle_t attackTheFlagSound;  // A&D: vo
    sfxHandle_t defendTheFlagSound;

    sfxHandle_t biteSound;  // red rover: vo
    sfxHandle_t infectedSound;
    sfxHandle_t nightmareSound;
    sfxHandle_t kamikazeRespawnSound;

    sfxHandle_t tracerSound;
    sfxHandle_t selectSound;
    sfxHandle_t wearOffSound;
    sfxHandle_t useNothingSound;
    // [QL] The no-gore gib set, and the gore pack's flesh sounds, which load only when the
    // pack's dlc_gibs folder exists
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
    sfxHandle_t hitSounds[4];       // [QL] sound/feedback/hit0-3, by damage tier
    sfxHandle_t killBeepSounds[8];  // [QL] cg_killBeep 1-8: impact1-6, bell_01, chaching
    sfxHandle_t watrInSound;
    sfxHandle_t watrOutSound;
    sfxHandle_t watrUnSound;
    sfxHandle_t jumpPadSound;

    sfxHandle_t impressiveSounds[3];  // vo
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

    sfxHandle_t battleSuitVoSound;  // powerup voices: vo
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

    // [QL] registered by CG_RegisterWeapon for the lightning gun (0x10050fb1)
    sfxHandle_t sfx_lghit1;
    sfxHandle_t sfx_lghit2;
    sfxHandle_t sfx_lghit3;

    // new stuff
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

    // [QL] Race checkpoint models and shaders (binary-verified)
    qhandle_t raceFlagB;          // models/flag3/b_flag3.md3 (blue)
    qhandle_t raceFlagR;          // [QL] models/flag3/r_flag3.md3
    qhandle_t raceFlagN;          // [QL] models/flag3/n_flag3.md3
    qhandle_t raceNavShader;      // [QL] "gfx/misc/racenav.jpg"
    qhandle_t raceFlagF;          // models/flag3/f_flag3.md3 (finish)
    qhandle_t raceFlagG;          // models/flag3/g_flag3.md3 (green/start)
    qhandle_t raceFlagD;          // models/flag3/d_flag3.md3 (default/checkpoint)
    qhandle_t raceMarkerStart;    // gfx/2d/race/start
    qhandle_t raceMarkerCheckpoint; // gfx/2d/race/checkpoint
    qhandle_t raceMarkerFinish;   // gfx/2d/race/finish
    qhandle_t domPointModel;      // models/powerups/domination/dompoint.md3 (pedestal)
    qhandle_t domSkinRed;         // models/powerups/domination/domred.skin
    qhandle_t domSkinBlue;        // models/powerups/domination/domblue.skin
    qhandle_t domSkinNeutral;     // models/powerups/domination/domntrl.skin
    qhandle_t domShadowShader;    // [QL] "domShadow", the pulsing ring under a point

    // [QL] POI markers
    qhandle_t poiShader;            // sprites/neutralflagcarrier, the EV_POI marker
    qhandle_t quadHogPoiShader;     // gfx/2d/quad_hog/quadhog, the same marker in Quad Hog
    qhandle_t powerupPoi[5];        // [QL] gfx/2d/powerup/{quad,bs,haste,invis,regen}
    qhandle_t powerupIncomingShader;// [QL] gfx/2d/powerup/incoming, mientras vuelve
    qhandle_t friendShader;         // [QL] sprites/friend, a teammate's marker
    qhandle_t friendHitShader;      // [QL] sprites/friend_hit, one hurt within cg_drawHitFriendTime
    qhandle_t friendDeadShader;     // [QL] sprites/friend_dead, where a teammate died
    qhandle_t flagCarrierShader;    // [QL] sprites/flagcarrier
    qhandle_t flagCarrierHitShader; // [QL] sprites/flagcarrier_hit
    qhandle_t infectedShader;       // [QL] gfx/2d/infected/bite, over Red Rover's survivors
    qhandle_t adAttackShader;       // [QL] gfx/2d/ad/poi_attack, the enemy flag to take
    qhandle_t adCaptureShader;      // [QL] gfx/2d/ad/poi_capture, where to bring it
    qhandle_t adDefendShader;       // [QL] gfx/2d/ad/poi_defend, your own flag
    qhandle_t harvesterCaptureShader;  // [QL] gfx/2d/har/poi_capture, where to bring the skulls
    qhandle_t domCaptureShader[5];     // [QL] gfx/2d/dom_point/dom_cap_a..e
    qhandle_t domDefendShader[5];      // [QL] gfx/2d/dom_point/dom_def_a..e
    qhandle_t domCaptureDistShader[5]; // [QL] the same, _dist: a point being fought over
    qhandle_t domDefendDistShader[5];
    qhandle_t healthSegmentShader;  // [QL] gfx/2d/health_segment.tga, the bars over teammates
    qhandle_t cmdUpShader;          // [QL] gfx/2d/race/cmd_up, the arrows of cg_drawInputCmds
    qhandle_t cmdDownShader;        // [QL] gfx/2d/race/cmd_down
    qhandle_t cmdRightShader;       // [QL] gfx/2d/race/cmd_right
    qhandle_t cmdLeftShader;        // [QL] gfx/2d/race/cmd_left
    qhandle_t voiceWindowShader;    // [QL] ui/assets/voiceWindow, the followed player's frame in the team vitals
    qhandle_t scoreMutedShader;     // [QL] ui/assets/score/muted, on the scoreboard, for a muted player
    qhandle_t scoreSpeakingShader;  // [QL] ui/assets/score/speaking, for whoever is talking

    // [QL] freeze tag
    qhandle_t iceShardModel;     // models/gibs/sphere.md3
    qhandle_t gibSphere;         // [QL] same model, the no-gore gib
    qhandle_t deathEffectShader; // [QL] "deathEffect", the electrogib flash
    qhandle_t gibSparkShader;    // [QL] "gfx/misc/tracer", the rising sparks
    qhandle_t iceShardShader1;   // powerups/ice1
    qhandle_t iceShardShader2;   // powerups/ice2
    qhandle_t iceShardShader3;   // powerups/ice3
    qhandle_t frozenShader;      // sprites/frozen
    qhandle_t iceMarkShader;     // iceMark
    qhandle_t iceTrailShader;    // [QL] iceTrail, the shards' trail

    // [QL] gametype icons for scoreboard/HUD
    qhandle_t gametypeIcon[GT_MAX_GAME_TYPE];

    // [QL] freeze-tag effect media (CG_ThawPlayer).
    // (frozenShader already declared above.)
    qhandle_t iceWhiteModel;      // ice shard model A, global 0x10a5f428 (first/odd shards)
    qhandle_t iceBlueModel;        // ice shard model B, global 0x10a5f42c (alternate shards)

    // [QL] friend marker
    qhandle_t friendMarkModel;

    // [QL] wallbang / shotgun-kill debris
    qhandle_t debrisPuffShader;    // [QL] surfacePuff, CG_MissileHitWall_DmgThrough debris
    qhandle_t bloodPuffShader;     // CG_ShotgunKillEffect / CG_ShotgunPellet blood puff

    // [QL] the flag status icons (CG_DrawFlagStatus / CG_DrawFlagStatusBar), 0x10a5fc2c, indexed
    // [icon + team*4]: icon 0 at base, 1 taken, 2 dropped, 3 stolen; team 0 neutral, 1 red, 2 blue
    qhandle_t flagStatusHandles[12];

    // [QL] duel player-status plate shaders (CG_DrawPlayerStatusLeft/Right).
    // Globals DAT_10a5f360..0x10a5f384.
    // [QL] the duel board's status plates, [0] for the first player (left), [1] the second
    qhandle_t duelPlateReady[2], duelPlateNotReady[2], duelPlateLeads[2], duelPlateTied[2], duelPlateTrails[2];

} cgMedia_t;

// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct {
    gameState_t gameState;  // gamestate from server
    glconfig_t glconfig;    // rendering configuration
    float screenXScale;     // derived from glconfig
    float screenYScale;
    float screenXBias;
    float widescreenBias;   // [QL] half extra width on widescreen (0 if 4:3 or narrower)

    int serverCommandSequence;  // reliable command stream counter
    int processedSnapshotNum;   // the number of snapshots cgame has requested

    qboolean localServer;  // detected on startup by checking sv_running
    qboolean ioqlServer;   // [ioquakelive] the server is ours, not retail's: its com_gamename

    // parsed from serverinfo (order matches QL binary's CG_ParseServerinfo)
    gametype_t gametype;
    int teamsize;           // [QL]
    int teamSizeMin;        // [QL] g_teamSizeMin
    int teamForceBalance;   // [QL] g_teamForceBalance
    int dmflags;
    int fraglimit;
    int capturelimit;
    int scorelimit;         // [QL]
    int mercylimit;         // [QL]
    int timelimit;
    int roundlimit;
    int roundtimelimit;
    int roundWarmupDelay;   // [QL] g_roundWarmupDelay
    int freezeRoundDelay;   // [QL] g_freezeRoundDelay
    int maxclients;
    int timeoutCount;       // [QL] g_timeoutCount
    int timelimit_overtime;  // [QL] g_overtime
    int itemHeight;         // [QL] g_itemHeight
    int gravity;            // [QL] g_gravity
    int weaponRespawn;      // [QL] g_weaponRespawn
    int itemTimers;         // [QL] g_itemTimers
    int quadDamageFactor;   // [QL] g_quadDamageFactor
    int voteFlags;          // [QL] g_voteFlags
    int startingHealth;     // [QL] g_startingHealth
    int adCaptureScoreBonus;  // [QL] g_adCaptureScoreBonus
    int adElimScoreBonus;     // [QL] g_adElimScoreBonus
    int adtouchScoreBonus;    // [QL] g_adtouchScoreBonus
    char mapname[MAX_QPATH];

    // [QL] parsed from CS_PLAYERINFO (CG_ParseConfigParams, 0x10048e40). A non-empty
    // override string disables client model/head forcing for that part.
    char playermodelOverride[MAX_QPATH];      // g_playermodelOverride
    char playerheadmodelOverride[MAX_QPATH];  // g_playerheadmodelOverride
    int allowCustomHeadmodels;                // g_allowCustomHeadmodels
    float playerheadScale;                    // g_playerheadScale
    float playerheadScaleOffset;              // g_playerheadScaleOffset
    float playerModelScale;                   // g_playerModelScale

    int voteTime;
    int voteYes;
    int voteNo;
    qboolean voteModified;  // beep whenever changed
    char voteString[MAX_STRING_TOKENS];

    int teamVoteTime[2];
    int teamVoteYes[2];
    int teamVoteNo[2];
    qboolean teamVoteModified[2];  // beep whenever changed
    char teamVoteString[2][MAX_STRING_TOKENS];

    int levelStartTime;
    qboolean svCheats;  // [QL] 0x10abaaa8: sv_cheats of the systeminfo when the map loaded

    int scores1, scores2;   // from configstrings
    char firstPlaceName[40];   // [QL] CS_SCORES1PLAYER (0x10a40400)
    char secondPlaceName[40];  // [QL] CS_SCORES2PLAYER (0x10a40428)
    int redflag, blueflag;  // flag status from configstrings
    int flagStatus;

    qboolean newHud;

    //
    // locally derived information from gamestate
    //
    qhandle_t gameModels[MAX_MODELS];
    sfxHandle_t gameSounds[MAX_SOUNDS];

    int numInlineModels;
    qhandle_t inlineDrawModel[MAX_MODELS];
    vec3_t inlineModelMidpoints[MAX_MODELS];

    clientInfo_t clientinfo[MAX_CLIENTS];

    // teamchat width is *3 because of embedded color codes
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

    // orders
    int currentOrder;
    qboolean orderPending;
    int orderTime;
    int currentVoiceClient;
    int acceptOrderTime;
    int acceptTask;
    int acceptLeader;
    char acceptVoice[MAX_NAME_LENGTH];

    // media
    cgMedia_t media;

    // [QL] round-based mode tracking
    qboolean roundStarted;
    int roundNum;

    // [QL] duel player configstrings (0x10a40450-0x10a404af)
    int clientNum1stPlayer;
    int clientNum2ndPlayer;
    int score1stPlayer;       // CS_SCORE1STPLAYER
    int score2ndPlayer;       // CS_SCORE2NDPLAYER
    char name1stPlayer[40];   // CS_NAME1STPLAYER
    char name2ndPlayer[40];   // CS_NAME2NDPLAYER

    // [QL] HUD configstrings
    int teamCountRed;    // CS_TEAMCOUNT_RED  (alive red count for round modes)
    int teamCountBlue;   // CS_TEAMCOUNT_BLUE (alive blue count for round modes)
    int armorTiered;     // CS_ARMORINFO tiered-armor flag (mirrors cg_armorTiered)
    int customSettings;  // CS_CUSTOM_SETTINGS bitmask (DAT_10a3ff28); bit 0x4000000 = g_rrInfected
    int playerCylinders; // CS_PLAYER_CYLINDERS (DAT_10a5fd9c); capsule hulls for hit prediction

    // [QL] wallbang surface depth (_DAT_10a5fda8). Parsed from a server configstring,
    // used only by CG_MissileHitWall_DmgThrough. Not a client cvar.
    float dmgThroughDepth;

    // [QL] per-team alive count for CG_DrawTeamAliveCount (CA/FT). Binary indexes int array base DAT_10a404c8.
    int teamAliveCount[TEAM_NUM_TEAMS];

    // [QL] freeze/timeout end time (DAT_10a403e0). Used by CG_GetOvertimeCount /
    // CG_GetLevelTimerMsec; 0 means "use cg.time".
    int freezeEnd;
    // [QL] pause auto-unpause / timein-countdown target (DAT_10a403e4, CS_PAUSE_END_TIME).
    // 0 = indefinite pause; non-zero = level.time the match resumes. Used by CG_DrawTimeout.
    int pauseEnd;
    // [QL] DAT_10a403e8: the pause start the klaxon played for, then when the countdown
    // voice last played
    int pauseSoundTime;

    // [QL] practice-mode flag (CS_PRACTICE 667, DAT_10a3ff30); warmup / weapon-select HUD.
    int practice;
    // [QL] free-spectator camera flag (CS_FREECAM 668, DAT_10a5fd0c); CG_DrawSpectator hint.
    int freecam;
    // [QL] "all ready" countdown target time (CS_ALLREADY_TIME 708, DAT_10a403d8);
    // read by CG_DrawWarmupMessages.
    int allReadyTime;
    // [QL] round start time (CS_ROUND_START_TIME 662, DAT_10a403dc); round timer + sounds.
    int roundStartTime;
} cgs_t;

//==============================================================================

extern cgs_t cgs;
extern cg_t cg;
extern centity_t cg_entities[MAX_GENTITIES];

// [QL] Color wheel arrays (runtime-initialized, 26 entries for 'a'-'z')
extern unsigned int g_colorWheel[26];
extern vec3_t g_colorWheelNormalized[26];
extern weaponInfo_t cg_weapons[MAX_WEAPONS];
extern itemInfo_t cg_items[MAX_ITEMS];
extern markPoly_t cg_markPolys[MAX_MARK_POLYS];

// [QL] the cvar table's mark for an entry registered with its range (0x10020bb0). It is the bit
// the engine calls CVAR_VM_CREATED, which the engine adds to every cvar a module registers.
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
extern vmCvar_t cg_drawRewardsRowSize;  // [QL] medals at once before it becomes icon + count
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
extern vmCvar_t cg_drawItemPickups;       // [QL] 0x10a65a00: 1 icon, 2 name, 4 game clock
extern vmCvar_t cg_levelTimerDirection;   // [QL] 0x10a6e6a0
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
extern vmCvar_t cg_bob;  // [QL] how much the view bobs and leans; retail's only bob cvar
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
extern vmCvar_t r_dynamicLight;  // [ioquakelive] ver CG_AddEffectLight
extern vmCvar_t cg_viewsize;
extern vmCvar_t cg_tracerChance;
extern vmCvar_t cg_tracerWidth;
extern vmCvar_t cg_tracerLength;
extern vmCvar_t cg_autoswitch;
extern vmCvar_t cg_itemFx;
extern vmCvar_t cg_itemTimers;  // [QL] 0 turns off the timer left where the item was
extern vmCvar_t cg_simpleItems;
extern vmCvar_t cg_skipItems;
extern vmCvar_t cg_simpleItemsBob;           // [QL] 0 none, 1 every item, 2 only the timed ones
extern vmCvar_t cg_simpleItemsHeightOffset;  // [QL] how far the icon sits over the item
extern vmCvar_t cg_simpleItemsRadius;        // [QL] the icon's size
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

// [wolfcam] The electrogib tuning set, names and defaults straight from
// wolfcamql's cvar table. None of these exist in retail Quake Live - the
// retail client hardcodes every one of them - so they are a deliberate
// addition on top of the ported effect, not a reconstruction of anything.
// They are what makes the effect adjustable without a rebuild; leaving
// every one at its default reproduces wolfcam's picture exactly.
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

// Ceiling on cg_gibCount, which wolfcam leaves open. Each sphere costs one
// local entity, one floating spark, and about seven live trail sparks, so
// fifty is already ~450 of the 512 MAX_LOCAL_ENTITIES. Past the pool the
// allocator recycles the entity CG_AddLocalEntities is walking toward and
// the client dies; this is well clear of any setting worth using.
#define MAX_ELECTROGIBS 50

extern vmCvar_t cg_predictItems;
extern vmCvar_t cg_deferPlayers;
extern vmCvar_t cg_teamChatsOnly;
extern vmCvar_t cg_complaintWarning;

extern vmCvar_t cg_scorePlum;
// pmove_fixed / pmove_msec removed - not in QL binary
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

// [QL] additional cvars
extern vmCvar_t cg_damagePlum;
extern vmCvar_t cg_damagePlumColorStyle;
extern vmCvar_t cg_armorTiered;
extern vmCvar_t cg_announcer;
extern vmCvar_t cg_autoHop;
extern vmCvar_t cg_predictLocalRailshots;  // [QL] predicted railgun autofire
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
extern vmCvar_t cg_lightningImpact;         // [QL] gate the LG impact flare
// [QL] every smoke has its radius in a cvar, and 0 turns it off
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
extern vmCvar_t cg_specDuelHealthArmor;  // [QL] gates the spectator HUD health/armor bars
extern vmCvar_t cg_specDuelHealthColor;  // [QL] health-gradient tint for those bars
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

// QL cvars (binary-verified from cgamex86.dll)
extern vmCvar_t cg_chatbeep;
extern vmCvar_t cg_teamChatBeep;
extern vmCvar_t cg_deadBodyColor;
extern vmCvar_t cg_deadBodyDarken;
extern vmCvar_t g_training;  // [QL] server training flag; gates model forcing (CG_ResolveModelForClient)
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
extern vmCvar_t cg_drawBrightWeapons;  // [CPMA] bitmask of the weapons drawn as flat silhouettes
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
extern vmCvar_t cg_playerTeam;  // [QL] ROM; its .integer keeps the viewer's last team (CG_PredictPlayerState)
extern vmCvar_t cg_trueShotgun;
extern vmCvar_t cg_vignette;
extern vmCvar_t cg_zoomOutOnDeath;
extern vmCvar_t cg_zoomScaling;
extern vmCvar_t cg_zoomSensitivity;
extern vmCvar_t cg_zoomToggle;

// [QL] positive-sense taunt gate (DAT_10a68eec). EV_TAUNT plays only when non-zero.
extern vmCvar_t cg_allowTaunt;
// [QL] GTS_LAST_STANDING announcer gate (DAT_10b716ec).
extern vmCvar_t cg_announcerLastStanding;

//
// cg_main.c
//
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

// [QL] cgameExport_t slots 10-14
void CG_LastChatCommand(void);
void CG_LastChatCommand2(void);
void CG_SetKeyCatcher(void);
void CG_ClearKeyCatcher(void);
int  CG_GetActiveFrame(void);

// [QL] race / match-state helpers
void        CG_RaceInit(int fullReset);
int         CG_RaceCheckpointsLeft(void);
const char* CG_FormatRaceTime(int msec);
int         CG_GetOvertimeCount(void);
qboolean    CG_LocalPlayerLeads(void);
const char* CG_TranslateMapName(const char* mapname);
void        CG_CacheCountryFlags(void);
qhandle_t   CG_CountryFlagShader(const char* country);

//
// cg_view.c
//
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
// [QL] menu pointer cache (binary caches these in one run at 0x10006eb8) and the
// competitive spectator overlay (0x1000dad0).
void CG_CacheMenus(void);
void CG_DrawSpectatorHud(void);
void CG_DrawSpectator(float x, float y);  // [QL] reached only via the CG_SPEC_MESSAGES ownerdraw
// [QL] CG_CheckAutoFollow, CG_FilterAngles, CG_DrawDuelWeaponStats are file-static in cg_view.c.
qboolean CG_GetLevelTimerMsec(int* msec);

//
// cg_drawtools.c
//
// [QL] server command parsers
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

//
// cg_draw.c, cg_newDraw.c
//
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

//
// cg_player.c
//
void CG_Player(centity_t* cent);
void CG_ResetPlayerEntity(centity_t* cent);
void CG_AddRefEntityWithPowerups(refEntity_t* ent, entityState_t* state, int team);
void CG_NewClientInfo(int clientNum);
sfxHandle_t CG_CustomSound(int clientNum, const char* soundName);

// [QL] model/skin forcing + scaling. CG_ResolveModelForClient, CG_CalcModelScale,
// CG_PlayerTeamSkins and CG_ObjectiveMarker are file-static in cg_players.c;
// CG_DomPointMarker and CG_Draw3DPlayerModel are cross-file (see below).
qboolean CG_ShouldForceTeamSkin(int viewerTeam, int playerTeam);
// [QL] AD-objective outline decal - the binary calls this from the entity dispatch in
// cg_ents.c (not from CG_Player), so it's declared here.
void CG_DomPointMarker(centity_t* cent);
// [QL] full legs+torso+head player model for the spectator/duel model owner-draws in
// cg_newdraw.c (replaces the head-only CG_DrawHead stand-in).
void CG_Draw3DPlayerModel(int clientNum, int torsoAnim, float x, float y, float w, float h, int weapon,
                          float animFraction, const vec3_t angles);
void CG_ResolveBodySkinName(clientInfo_t* ci, qboolean modelForced, qboolean skinForced);
void CG_ResolveHeadSkinName(clientInfo_t* ci, qboolean modelForced, qboolean skinForced);
void CG_UpdateAllModelScales(void);  // cross-file: called when the model-scale cvar changes

//
// cg_predict.c
//
void CG_BuildSolidList(void);
int CG_PointContents(const vec3_t point, int passEntityNum);
void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask);
void CG_CapsuleTrace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask);
void CG_PredictPlayerState(void);
void CG_LoadDeferredPlayers(void);
void CG_ReloadOtherClients(void);
void CG_SetEndScoreboardMenu(void);  // [QL] 0x10028f30, cg_draw.c

//
// cg_events.c
//
void CG_CheckEvents(centity_t* cent);
const char* CG_PlaceString(int rank);
void CG_EntityEvent(centity_t* cent, vec3_t position);
void CG_PainEvent(centity_t* cent, int health);
// [QL] event-driven helpers
void CG_SpecItemTimerEvent(entityState_t* es);  // [QL] 0x10019d90, EV_ITEM_PICKUP_SPEC
void CG_UpdateSpecItemTimers(void);             // [QL] 0x1001ca90, once a frame
void CG_ResetSpecItemTimers(void);              // [QL] 0x1001c8d0, on map_restart
void CG_PlayIntermissionSound(void);
void CG_PlayMatchStateSound(int state);

//
// cg_ents.c
//
void CG_SetEntitySoundPosition(centity_t* cent);
void CG_AddPacketEntities(void);
void CG_Beam(centity_t* cent);
void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out);

void CG_PositionEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName);
void CG_PositionRotatedEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName);

//
// cg_weapons.c
//
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
// [CPMA] the weapon model cg_drawBrightWeapons is asked about: one lying on the map, the one in your
// own first-person view, or one a player carries
typedef enum { BRIGHTWEAPON_ON_MAP, BRIGHTWEAPON_OWN_VIEW, BRIGHTWEAPON_CARRIED } brightWeapon_t;
void CG_BrightWeapon(refEntity_t* ent, int weaponNum, int owner, brightWeapon_t kind);
void CG_GrappleTrail(centity_t* owner, const centity_t* hook, const vec3_t start);
// CG_RailTrailCore, CG_SpawnRailTrail, CG_GetRailColorFloat, CG_GetRailColorByte are file-static in cg_weapons.c.
void CG_MissileHitWall_DmgThrough(vec3_t origin, vec3_t dir, int weapon);  // wallbang (EV_MISSILE_MISS_DMGTHROUGH)
void CG_ShotgunKillEffect(centity_t* cent);  // EV_SHOTGUN_KILL blood-puff burst
void CG_AddViewWeapon(playerState_t* ps);
void CG_AddPlayerWeapon(refEntity_t* parent, playerState_t* ps, centity_t* cent, int team);
void CG_AddEffectLight(const vec3_t org, float intensity, float r, float g, float b);
void CG_DrawWeaponSelect(void);
void CG_DrawWeaponBar(void);    // [QL] weapon bar (Left/Right/Centered/Classic modes)

void CG_OutOfAmmoChange(void);  // should this be in pmove?
void CG_DropWeaponChange(int dropped);

//
// cg_marks.c
//
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

//
// cg_localents.c
//
void CG_InitLocalEntities(void);
localEntity_t* CG_AllocLocalEntity(void);
void CG_AddLocalEntities(void);

//
// cg_effects.c
//
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
void CG_DamagePlum(int damage, int weapon, vec3_t org);  // [QL]

// [QL] shared floating-effect pool (cg_effects.c)
floatingEffect_t* CG_AllocFloatingEffect(void);  // [QL] binary "CG_AllocMark" @ 0x1002a0d0
qboolean CG_ProjectFloatingEffect(const vec3_t point, float* x, float* y);  // 640x480, top-left origin
void CG_HealthGradientColor(int clientNum, int health, vec3_t out);         // [QL] 0x1000ca90
void CG_UpdateFloatingEffects(void);             // [QL] binary "CG_UpdateConfigStrings" @ 0x1002a190
void CG_DrawFloatingEffects(void);               // [QL] binary "CG_DrawDamagePlums" @ 0x10011680

// [QL] freeze-tag / discharge / blood client-side effects (triggered from CG_EntityEvent).
void CG_ThawPlayer(const vec3_t origin);
void CG_IceTrail(localEntity_t* le);  // [QL] 0x1001d790
void CG_SpawnIceShard(const vec3_t origin, const vec3_t velocity, qhandle_t hModel);
void CG_LightningDischargeEffect(int intensity);
void CG_BloodSplat(vec3_t origin);      // EV_INVUL_IMPACT (0x46) handler
void CG_PlayBloodSound(vec3_t origin);  // EV_OBELISKPAIN (0x45) handler

void CG_GibPlayer(vec3_t playerOrigin, vec3_t dir);
qhandle_t CG_ElectroGibSparkShader(vec4_t color);
void CG_BigExplode(vec3_t playerOrigin);

void CG_Bleed(vec3_t origin, int entityNum);
void CG_BloodSplatEffect(vec3_t origin, int entityNum);  // QL: blood splat (replaces CG_Bleed for hit effects)
void CG_SpawnParticleEffect(vec3_t origin, vec3_t vel, float size, float r, float g, float b, float a, float lifetime, int startTime, int flags, qhandle_t shader);  // [QL] 0x10012f30: one impact spark
void CG_SpecAutoFollow(int clientNum, int mode);  // QL: auto-follow spectator
void CG_AddGameEvent(int clientNum, qhandle_t icon, const char* what);
void CG_ReadResolution(void);
void CG_SpecFollowKiller(int clientNum);

localEntity_t* CG_MakeExplosion(vec3_t origin, vec3_t dir, qhandle_t hModel, qhandle_t shader, int msec, qboolean isSprite);

//
// cg_snapshot.c
//
void CG_ProcessSnapshots(void);
void CG_DemoRewind(int snapshotNum, int serverCommandSequence);

//
// cg_info.c
//
void CG_LoadingString(const char* s);
void CG_LoadingItem(int itemNum);
void CG_LoadingClient(int clientNum);
void CG_DrawInformation(void);

//
// cg_scoreboard.c
//
qboolean CG_DrawOldScoreboard(void);
void CG_DrawTourneyScoreboard(void);

//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand(void);
void CG_InitConsoleCommands(void);
void CG_ScoresDown_f(void);
void CG_ClearChat(void);
void CG_AddChat(const char *text, int teamOnly, int extraTime);
void CG_DrawChat(float x, float y, float w, float h, float scale);
void CG_InitColorWheel(void);

//
// cg_servercmds.c
//
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

// [QL] extended team-stats parsers (per-client verbs; arg1 = scoreboard slot into cg.teamStats[])
void CG_ParseTeamStats_TDM(void);
void CG_ParseTeamStats_CA(void);
void CG_ParseTeamStats_CTF(void);
void CG_InitScores(void);  // scores_ad / adscores parser (22 ints -> cg.adScores + cg.teamScores)

// [QL] voice-chat receive path (entity-event driven via CG_VoiceChatLocal)
void CG_PlayVoiceChat(bufferedVoiceChat_t* vchat);
void CG_AddBufferedVoiceChat(bufferedVoiceChat_t* vchat);
qboolean CG_GetVoiceChat(const char* cmd, sfxHandle_t* snd, const char** chat);
int CG_ParseVoiceChats(const char* filename);
void CG_VoiceChatListForClient(int clientNum);

//
// cg_playerstate.c
//
void CG_Respawn(void);
void CG_TransitionPlayerState(playerState_t* ps, playerState_t* ops);
void CG_PushReward(sfxHandle_t sfx, qhandle_t shader, int rewardCount);

//
// cg_drawtools.c, [CPMA] SuperHUD animations
//
float CG_CpmaEaseOut(float x);
float CG_CpmaFade(int start, int fadein, int fadedelay, int time);
qboolean CG_CpmaFadingOut(int start, int fadein, int fadedelay, int time);
float CG_CpmaSlide(int start, int time);

//
// cg_players.c, [CPMA] cg_animateChatBalloon
//
qhandle_t CG_ChatBalloonShader(void);
void CG_CheckChangedPredictableEvents(playerState_t* ps);

//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void trap_Print(const char* fmt);

// abort the game
void trap_Error(const char* fmt) __attribute__((noreturn));

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int trap_Milliseconds(void);

// console variable interaction
void trap_Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags);
// [QL] cgame import +0x14, used by CG_RegisterCvars for the CVAR_CG_RANGE entries
void trap_Cvar_RegisterRange(vmCvar_t* vmCvar, const char* varName, const char* defaultValue,
                             const char* minValue, const char* maxValue, int flags);
void trap_Cvar_Update(vmCvar_t* vmCvar);
void trap_Cvar_Set(const char* var_name, const char* value);
void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);
float trap_Cvar_VariableValue(const char* var_name);

// ServerCommand and ConsoleCommand parameter access
int trap_Argc(void);
void trap_Argv(int n, char* buffer, int bufferLength);
void trap_Args(char* buffer, int bufferLength);

// filesystem access
// returns length of file
int trap_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode);
int trap_FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
void trap_FS_Read(void* buffer, int len, fileHandle_t f);
void trap_FS_Write(const void* buffer, int len, fileHandle_t f);
void trap_FS_FCloseFile(fileHandle_t f);
int trap_FS_Seek(fileHandle_t f, long offset, int origin);  // fsOrigin_t

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void trap_SendConsoleCommand(const char* text);

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void trap_AddCommand(const char* cmdName);
void trap_RemoveCommand(const char* cmdName);

// send a string to the server over the network
void trap_SendClientCommand(const char* s);

// force a screen update, only used during gamestate load
void trap_UpdateScreen(void);

// model collision
void trap_CM_LoadMap(const char* mapname);
int trap_CM_NumInlineModels(void);
clipHandle_t trap_CM_InlineModel(int index);  // 0 = world, 1+ = bmodels
clipHandle_t trap_CM_TempBoxModel(const vec3_t mins, const vec3_t maxs);
clipHandle_t trap_CM_TempCapsuleModel(const vec3_t mins, const vec3_t maxs);
int trap_CM_PointContents(const vec3_t p, clipHandle_t model);
int trap_CM_TransformedPointContents(const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles);
void trap_CM_BoxTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void trap_CM_CapsuleTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void trap_CM_TransformedBoxTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles);
void trap_CM_TransformedCapsuleTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles);

// Returns the projection of a polygon onto the solid brushes in the world
int trap_CM_MarkFragments(int numPoints, const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t* fragmentBuffer);

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void trap_S_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
void trap_S_StopLoopingSound(int entnum);

// a local sound is always played full volume
void trap_S_StartLocalSound(sfxHandle_t sfx, int channelNum);
void trap_S_ClearLoopingSounds(qboolean killall);
void trap_S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void trap_S_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void trap_S_UpdateEntityPosition(int entityNum, const vec3_t origin);

// respatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void trap_S_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater);
sfxHandle_t trap_S_RegisterSound(const char* sample, qboolean compressed);  // returns buzz if not found
void trap_S_StartBackgroundTrack(const char* intro, const char* loop);      // empty name stops music
void trap_S_StopBackgroundTrack(void);
void trap_S_ClearSoundBuffer(void);

void trap_R_LoadWorldMap(const char* mapname);

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t trap_R_RegisterModel(const char* name);        // returns rgb axis if not found
qhandle_t trap_R_RegisterSkin(const char* name);         // returns all white if not found
qhandle_t trap_R_RegisterShader(const char* name);       // returns all white if not found
qhandle_t trap_R_RegisterShaderNoMip(const char* name);  // returns all white if not found

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void trap_R_ClearScene(void);
void trap_R_AddRefEntityToScene(const refEntity_t* re);

// polys are intended for simple wall marks, not really for doing
// significant construction
void trap_R_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts);
void trap_R_AddPolysToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int numPolys);
void trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void trap_R_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
int trap_R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
void trap_R_RenderScene(const refdef_t* fd);
void trap_R_BeginBloomScene(void);
void trap_R_ApplyBloom(void);
void trap_R_SetColor(const float* rgba);  // NULL = 1,1,1,1
void trap_R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs);
int trap_R_LerpTag(orientation_t* tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char* tagName);
void trap_R_RemapShader(const char* oldShader, const char* newShader, const char* timeOffset);
qboolean trap_R_inPVS(const vec3_t p1, const vec3_t p2);

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void trap_GetGlconfig(glconfig_t* glconfig);

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void trap_GetGameState(gameState_t* gamestate);

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned separately so that
// snapshot latency can be calculated.
void trap_GetCurrentSnapshotNumber(int* snapshotNumber, int* serverTime);

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean trap_GetSnapshot(int snapshotNumber, snapshot_t* snapshot);

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean trap_GetServerCommand(int serverCommandNumber);

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int trap_GetCurrentCmdNumber(void);

qboolean trap_GetUserCmd(int cmdNumber, usercmd_t* ucmd);

// used for the weapon select and zoom
// [QL] 4-arg variant matches cgamex86.dll CG_DrawActiveFrame syscall:
//   stateValue       -> cmd.weapon         (usercmd_t offset 0x14)
//   weaponPrimary    -> cmd.weaponPrimary  (usercmd_t offset 0x15) - loadout
//   sensitivityScale -> mouse zoom sens (engine cl.cgameSensitivity)
//   fov              -> cmd.fov            (usercmd_t offset 0x16)
void trap_SetUserCmdValue(int stateValue, int weaponPrimary, float sensitivityScale, int fov);

// aids for VM testing
void testPrintInt(char* string, int i);
void testPrintFloat(char* string, float f);

int trap_MemoryRemaining(void);
void trap_R_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font);
void trap_R_Font_DrawString(int x, int y, const char* text, int fontIndex, float scale, int limit, float* maxX, int flags);
void trap_R_Font_TextExtents(const char* text, int start, int fontIndex, float scale, int limit, float* extents);  // extents[5]
void trap_R_GetGlyphInfo(int fontIndex, int charValue, glyphInfo_t* glyph);
void trap_IME_SetCompositionFont(int fontIndex, float scale);
int trap_R_Font_Register(const char* name);
// [ioquakelive] text in an engine font (trap_R_Font_Register), outside cg_draw.c
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
