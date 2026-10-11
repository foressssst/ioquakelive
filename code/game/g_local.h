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
#include "bg_public.h"
#include "g_public.h"

#define GAMEVERSION BASEGAME

#define BODY_QUEUE_SIZE 8

#define INFINITE 1000000

#define FRAMETIME 100
#define CARNAGE_REWARD_TIME 3000
#define REWARD_SPRITE_TIME 2000

#define INTERMISSION_DELAY_TIME 200
#define SP_INTERMISSION_DELAY_TIME 5000

#define G_TracePlayerHit(results, start, mins, maxs, end, passEnt, mask) \
    do { \
        if (g_playerCylinders.integer) \
            trap_TraceCapsule(results, start, mins, maxs, end, passEnt, mask); \
        else \
            trap_Trace(results, start, mins, maxs, end, passEnt, mask); \
    } while (0)

#define FL_GODMODE 0x00000010
#define FL_NOTARGET 0x00000020
#define FL_TEAMSLAVE 0x00000400
#define FL_NO_KNOCKBACK 0x00000800
#define FL_DROPPED_ITEM 0x00001000
#define FL_NO_BOTS 0x00002000
#define FL_NO_HUMANS 0x00004000
#define FL_FORCE_GESTURE 0x00008000
#define FL_NOPICKUP 0x00010000
#define FL_TRAINER 0x00020000
#define FL_NOPOWERUP 0x00040000
#define FL_DROPPED_ITEM_TIMED 0x00080000
#define FL_ITEM_TIMER 0x00100000

typedef enum {
    MOVER_POS1,
    MOVER_POS2,
    MOVER_1TO2,
    MOVER_2TO1
} moverState_t;

#define SP_PODIUM_MODEL "models/mapobjects/podium/podium4.md3"

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

struct gentity_s {
    entityState_t s;
    entityShared_t r;

    struct gclient_s* client;

    qboolean inuse;

    char* classname;
    int spawnflags;

    qboolean neverFree;

    int flags;

    char* model;
    char* model2;
    int freetime;

    int eventTime;
    qboolean freeAfterEvent;
    qboolean unlinkAfterEvent;

    qboolean physicsObject;
    float physicsBounce;
    int clipmask;

    moverState_t moverState;
    int soundPos1;
    int sound1to2;
    int sound2to1;
    int soundPos2;
    int soundLoop;
    gentity_t* parent;
    gentity_t* nextTrain;
    gentity_t* prevTrain;
    vec3_t pos1, pos2;

    char* message;
    char* cvar;
    char* tourPointTarget;
    char* tourPointTargetName;
    char* noise;

    gentity_t* tourNext;
    float tourRadius;
    float tourChatTime;
    float tourCvarValue;
    int tourIgnorePlayer;

    int timestamp;
    float angle;

    char* target;
    char* target2;
    char* targetname;
    char* targetShaderName;
    char* targetShaderNewName;
    gentity_t* target_ent;

    float speed;
    vec3_t movedir;

    int nextthink;
    void (*think)(gentity_t* self);
    void (*framethink)(gentity_t* self);
    void (*reached)(gentity_t* self);
    void (*blocked)(gentity_t* self, gentity_t* other);
    void (*touch)(gentity_t* self, gentity_t* other, trace_t* trace);
    void (*use)(gentity_t* self, gentity_t* other, gentity_t* activator);
    void (*pain)(gentity_t* self, gentity_t* attacker, int damage);
    void (*die)(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod);

    int pain_debounce_time;
    int fly_sound_debounce_time;

    int health;

    qboolean takedamage;

    int damage;
    int damageFactor;
    int splashDamage;
    int splashRadius;
    int methodOfDeath;
    int splashMethodOfDeath;

    int count;

    gentity_t* chain;
    gentity_t* enemy;
    gentity_t* activator;
    char* team;
    gentity_t* teammaster;
    gentity_t* teamchain;

    gentity_t   *domCapturer;

    int kamikazeTime;
    int kamikazeShockTime;

    int watertype;
    int waterlevel;

    int noise_index;
    int bouncecount;

    float wait;
    float random;

    int spawnTime;

    gitem_t* item;
    int pickupCount;
};

typedef enum {
    CON_DISCONNECTED,
    CON_CONNECTING,
    CON_CONNECTED
} clientConnected_t;

typedef enum {
    SPECTATOR_NOT,
    SPECTATOR_FREE,
    SPECTATOR_FOLLOW,
    SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
    TEAM_BEGIN,
    TEAM_ACTIVE
} playerTeamStateState_t;

typedef struct {
    playerTeamStateState_t state;

    int location;

    int captures;
    int basedefense;
    int carrierdefense;
    int flagrecovery;
    int fragcarrier;
    int assists;

    int flagsince;
    int flagDroppedPickups;
    int lasthurtcarrier;
    int lastreturnedflag;
    int lastfraggedcarrier;

    int dmgAccumulator;
} playerTeamState_t;

typedef struct {
    team_t sessionTeam;
    int spectatorTime;
    spectatorState_t spectatorState;
    int spectatorClient;
    int weaponPrimary;
    int wins, losses;
    qboolean teamLeader;
    int privileges;
    int specOnly;
    int playQueue;
    qboolean updatePlayQueue;
    int muted;
    int prevScore;
} clientSession_t;

#define MAX_NETNAME 36
#define MAX_VOTE_COUNT 3

typedef struct {
    clientConnected_t connected;
    usercmd_t cmd;
    qboolean localClient;
    qboolean initialSpawn;
    qboolean predictItemPickup;
    qboolean ioqlClient;
    char netname[40];
    char country[24];
    int _pad_steamId;
    uint64_t steamId;
    int maxHealth;
    int voteCount;
    int voteState;
    int complaints;
    int complaintClient;
    int complaintEndTime;
    int damageFromTeammates;
    int damageToTeammates;
    qboolean ready;
    int autoaction;
    int timeouts;
    int enterTime;
    playerTeamState_t teamState;
    int inactivityTime;
    int inactivityWarning;
    int lastUserinfoUpdate;
    int userInfoFloodInfractions;
    int lastMapVoteTime;
    int lastMapVoteIndex;
    qboolean teamInfo;
} clientPersistant_t;

typedef struct {
    uint32_t    statId;
    int         lastThinkTime;
    int         teamJoinTime;
    int         totalPlayTime;
    int         serverRank;
    qboolean    serverRankIsTied;
    int         teamRank;
    qboolean    teamRankIsTied;
    int         numKills;
    int         numDeaths;
    int         numSuicides;
    int         numTeamKills;
    int         numTeamKilled;
    int         numWeaponKills[16];
    int         numWeaponDeaths[16];
    int         shotsFired[16];
    int         shotsHit[16];
    int         damageDealt[16];
    int         damageTaken[16];
    int         powerups[16];
    int         holdablePickups[7];
    int         weaponPickups[16];
    int         weaponUsageTime[16];
    int         numCaptures;
    int         numAssists;
    int         numDefends;
    int         numHolyShits;
    int         totalDamageDealt;
    int         totalDamageTaken;
    int         previousHealth;
    int         previousArmor;
    int         numAmmoPickups;
    int         numFirstMegaHealthPickups;
    int         numMegaHealthPickups;
    int         megaHealthPickupTime;
    int         numHealthPickups;
    int         numFirstRedArmorPickups;
    int         numRedArmorPickups;
    int         redArmorPickupTime;
    int         numFirstYellowArmorPickups;
    int         numYellowArmorPickups;
    int         yellowArmorPickupTime;
    int         numFirstGreenArmorPickups;
    int         numGreenArmorPickups;
    int         greenArmorPickupTime;
    int         numQuadDamagePickups;
    int         numQuadDamageKills;
    int         numBattleSuitPickups;
    int         numRegenerationPickups;
    int         numHastePickups;
    int         numInvisibilityPickups;
    int         numRedFlagPickups;
    int         numBlueFlagPickups;
    int         numNeutralFlagPickups;
    int         numMedkitPickups;
    int         numArmorPickups;
    int         numDenials;
    int         killStreak;
    int         maxKillStreak;
    int         xp;
    int         domThreeFlagsTime;
    int         numMidairShotgunKills;
} expandedStatObj_t;

typedef struct {
    qboolean    racingActive;
    int         startTime;
    int         lastTime;
    int         best_race[64];
    int         current_race[64];
    int         currentCheckPoint;
    qboolean    weaponUsed;
    gentity_t   *nextRacePoint;
    gentity_t   *nextRacePoint2;
} raceInfo_t;

struct gclient_s {
    playerState_t ps;

    clientPersistant_t pers;
    clientSession_t sess;

    qboolean noclip;

    int lastCmdTime;
    int buttons;
    int oldbuttons;

    int damage_armor;
    int damage_blood;
    vec3_t damage_from;
    qboolean damage_fromWorld;

    int impressiveCount;
    int accurateCount;

    int accuracy_shots;
    int accuracy_hits;

    int lastClientKilled;
    int lastkilled_client;
    int lasthurt_client[2];
    int lasthurt_mod[2];
    int lasthurt_time[2];

    int lastKillTime;
    int lastGibTime;
    int rampageCounter;
    int revengeCounter[64];

    int respawnTime;
    int trainerSayTime;
    int spawnRetryTime;
    int failedSpawns;
    int rewardTime;
    int airOutTime;

    qboolean fireHeld;
    gentity_t* hook;

    int switchTeamTime;

    int timeResidual;
    int timeResidualScout;
    int timeResidualArmor;
    int timeResidualHealth;
    int timeResidualPingPOI;
    int timeResidualSpecInfo;
    qboolean healthRegenActive;
    qboolean armorRegenActive;

    gentity_t* persistantPowerup;
    int portalID;
    int invulnerabilityTime;

    expandedStatObj_t expandedStats;
    qboolean pendingFollow;
    int pendingFollowClient;

    int ignoreChatsTime;
    int lastUserCmdTime;
    qboolean freezePlayer;
    int deferredSpawnTime;
    int deferredSpawnCount;
    raceInfo_t race;
    int damagePlum[MAX_CLIENTS];
    int round_shots;
    int round_hits;
    int round_damage;
    qboolean queuedSpectatorFollow;
    int queuedSpectatorClient;
};

#define MAX_SPAWN_VARS 64
#define MAX_SPAWN_VARS_CHARS 4096

typedef struct {
    roundStateState_t   eCurrent;
    roundStateState_t   eNext;
    int                 tNext;
    int                 startTime;
    int                 turn;
    int                 round;
    team_t              prevRoundWinningTeam;
    qboolean            touch;
    qboolean            capture;
} roundState_t;

typedef struct {
    gclient_t       *clients;
    gentity_t       *gentities;
    int             gentitySize;
    int             num_entities;
    int             warmupTime;
    fileHandle_t    logFile;
    int             maxclients;
    int             time;
    int             frametime;
    int             startTime;
    int             teamScores[4];
    int             nextTeamInfoTime;
    qboolean        newSession;
    qboolean        restarted;
    qboolean        shufflePending;
    int             shuffleReadyTime;
    qboolean        autoShuffleArmed;
    int             autoShuffleStart;
    int             numConnectedClients;
    int             numNonSpectatorClients;
    int             numPlayingClients;
    int             numReadyClients;
    int             numReadyHumans;
    int             numStandardClients;
    int             sortedClients[64];
    int             follow1;
    int             follow2;
    int             snd_fry;
    int             warmupModificationCount;
    char            voteString[1024];
    char            voteDisplayString[1024];
    int             voteExecuteTime;
    int             voteTime;
    int             voteYes;
    int             voteNo;
    int             pendingVoteCaller;
    qboolean        spawning;
    int             numSpawnVars;
    char            *spawnVars[64][2];
    int             numSpawnVarChars;
    char            spawnVarChars[4096];
    int             intermissionQueued;
    int             intermissionTime;
    qboolean        readyToExit;
    qboolean        votingEnded;
    int             exitTime;
    vec3_t          intermission_origin;
    vec3_t          intermission_angle;
    qboolean        locationLinked;
    gentity_t       *locationHead;
    int             timePauseBegin;
    int             mp_unpauseTime;
    int             mp_pauseClient;
    int             teamTimeoutsUsed[4];
    int             timeOvertime;
    int             timeInitialPowerupSpawn;
    int             timeInitialKeySpawn;
    int             bodyQueIndex;
    gentity_t       *bodyQue[BODY_QUEUE_SIZE];
    int             portalSequence;
    qboolean        gameStatsReported;
    qboolean        scoringDisabled;
    qboolean        mapIsTrainingMap;
    int             clientNum1stPlayer;
    int             clientNum2ndPlayer;
    char            scoreboardArchive1[1024];
    char            scoreboardArchive2[1024];
    char            firstScorer[40];
    char            lastScorer[40];
    char            lastTeamScorer[40];
    char            firstFrag[40];
    vec3_t          red_flag_origin;
    vec3_t          blue_flag_origin;
    int             spawnCount[4];
    int             runeSpawns[5];
    int             itemCount[60];
    int             suddenDeathRespawnDelay;
    int             suddenDeathRespawnDelayLastAnnounced;
    int             forceRespawnDelay;
    int             numRedArmorPickups[4];
    int             numYellowArmorPickups[4];
    int             numGreenArmorPickups[4];
    int             numMegaHealthPickups[4];
    int             numQuadDamagePickups[4];
    int             numBattleSuitPickups[4];
    int             numRegenerationPickups[4];
    int             numHastePickups[4];
    int             numInvisibilityPickups[4];
    int             quadDamagePossessionTime[4];
    int             battleSuitPossessionTime[4];
    int             regenerationPossessionTime[4];
    int             hastePossessionTime[4];
    int             invisibilityPossessionTime[4];
    int             numFlagPickups[4];
    int             numMedkitPickups[4];
    int             flagPossessionTime[4];
    gentity_t       *dominationPoints[5];
    int             dominationPointCount;
    int             dominationPointsTallied;
    int             racePointCount;
    qboolean        disableDropWeapon;
    qboolean        teamShuffleActive;
    int             lastTeamScores[4];
    int             lastTeamRoundScores[4];
    team_t          attackingTeam;
    roundState_t    roundState;
    int             lastTeamCountSent;
    int             infectedConscript;
    int             lastZombieSurvivor;
    int             zombieScoreTime;
    int             lastInfectionTime;
    char            intermissionMapNames[3][1024];
    char            intermissionMapTitles[3][1024];
    char            intermissionMapConfigs[3][1024];
    int             intermissionMapVotes[3];
    qboolean        matchForfeited;
    int             allReadyTime;
    qboolean        notifyCvarChange;
    int             notifyCvarChangeTime;
    int             lastLeadChangeTime;
    int             lastLeadChangeClient;
    int             lastLeadChangeElapsedTime;
    int             rrInfectedCounters[4];
    int             rrInfectedSpreadStart;
    int             rrSurvivorScoreTimer;
    int             itemTimerCounts[MAX_ITEMS];
    vec3_t          itemTimerRedBase;
    vec3_t          itemTimerBlueBase;
    int             numSpawnSpotsFFA;
    int             numSpawnSpotsRed;
    int             numSpawnSpotsBlue;
    int             runeSpawnSpot[5];
    qboolean        ftDropCmdsSuspended;
    int             numRacePoints;
} level_locals_t;

qboolean G_SpawnString(const char* key, const char* defaultString, char** out);
qboolean G_SpawnFloat(const char* key, const char* defaultString, float* out);
qboolean G_SpawnInt(const char* key, const char* defaultString, int* out);
qboolean G_SpawnVector(const char* key, const char* defaultString, float* out);
void G_SpawnEntitiesFromString(void);
char* G_NewString(const char* string);

void Cmd_Score_f(gentity_t* ent);
void StopFollowing(gentity_t* ent);
void BroadcastTeamChange(gclient_t* client, int oldTeam);
void SetTeam(gentity_t* ent, const char* s);
void SetTeam_Execute(gentity_t* ent, int team, spectatorState_t specState, int specClient);
void G_ShuffleTeams(void);
qboolean G_TeamIsLocked(gentity_t* ent, team_t team);
void G_ClearTeamLocks(void);
void Cmd_FollowCycle_f(gentity_t* ent, int dir);
void FollowCycle(gentity_t* ent, int dir);
void ClearVote(void);

void G_CheckTeamItems(void);
void G_RunItem(gentity_t* ent);
void RespawnItem(gentity_t* ent);
void Item_NotifySpectator(gentity_t *ent, int clientNum, qboolean isRespawn);
void Item_NotifySpectators(gentity_t *ent, qboolean isRespawn);
void G_SendItemTimersToSpectator(int clientNum);
void G_InitItemTimerBases(void);
void G_RespawnKey(int keyTag);

void UseHoldableItem(gentity_t* ent);
void PrecacheItem(gitem_t* it);
gentity_t* Drop_Item(gentity_t* ent, gitem_t* item, float angle);
gentity_t* LaunchItem(gitem_t* item, vec3_t origin, vec3_t velocity);
void SetRespawn(gentity_t* ent, float delay);
void G_SpawnItem(gentity_t* ent, gitem_t* item, qboolean forced);
qboolean G_GametypeInList(const char* list);
void FinishSpawningItem(gentity_t* ent);
void Think_Weapon(gentity_t* ent);
int ArmorIndex(gentity_t* ent);
void Add_Ammo(gentity_t* ent, int weapon, int count);
void Touch_Item(gentity_t* ent, gentity_t* other, trace_t* trace);

void ClearRegisteredItems(void);
void RegisterItem(gitem_t* item);
void SaveRegisteredItems(void);

int G_ModelIndex(char* name);
int G_SoundIndex(char* name);
void G_TeamCommand(team_t team, char* cmd);
void G_KillBox(gentity_t* ent);
gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match);
gentity_t* G_PickTarget(char* targetname);
void G_UseTargets(gentity_t* ent, gentity_t* activator);
void G_SetMovedir(vec3_t angles, vec3_t movedir);

void G_InitGentity(gentity_t* e);
gentity_t* G_Spawn(void);
gentity_t* G_TempEntity(vec3_t origin, int event);
void G_Sound(gentity_t* ent, int channel, int soundIndex);
void G_GlobalSound(int soundIndex);
void G_FreeEntity(gentity_t* e);
qboolean G_EntitiesFree(void);

void G_TouchTriggers(gentity_t* ent);

float* tv(float x, float y, float z);
char* vtos(const vec3_t v);

float vectoyaw(const vec3_t vec);

void G_AddPredictableEvent(gentity_t* ent, int event, int eventParm);
void G_AddEvent(gentity_t* ent, int event, int eventParm);
void G_SetOrigin(gentity_t* ent, vec3_t origin);
void AddRemap(const char* oldShader, const char* newShader, float timeOffset);
const char* BuildShaderStateConfig(void);

qboolean CanDamage(gentity_t* targ, vec3_t origin);
void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod);
qboolean G_RadiusDamage(vec3_t origin, const vec3_t dir, const trace_t* trace, gentity_t* missile, gentity_t* ignore);
qboolean G_RadiusDamageThrough(vec3_t origin, const vec3_t dir, const trace_t* trace, vec3_t throughOrigin, gentity_t* missile, gentity_t* ignore);
qboolean G_WaterRadiusDamage(vec3_t origin, gentity_t *attacker, float damage, float radius);
int G_InvulnerabilityEffect(gentity_t* targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir);
void body_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int meansOfDeath);
void Kamikaze_DeathActivate(gentity_t* self);
void GibEntity(gentity_t* self);
void TossClientItems(gentity_t* self);
void TossClientCubes(gentity_t* self);

#define DAMAGE_RADIUS 0x00000001
#define DAMAGE_NO_ARMOR 0x00000002
#define DAMAGE_NO_KNOCKBACK 0x00000004
#define DAMAGE_NO_PROTECTION 0x00000008
#define DAMAGE_NO_TEAM_PROTECTION 0x00000010

void G_RunMissile(gentity_t* ent);

gentity_t* fire_plasma(gentity_t* self, vec3_t start, vec3_t aimdir);
gentity_t* fire_grenade(gentity_t* self, vec3_t start, vec3_t aimdir);
gentity_t* fire_rocket(gentity_t* self, vec3_t start, vec3_t dir);
gentity_t* fire_bfg(gentity_t* self, vec3_t start, vec3_t dir);
gentity_t* fire_grapple(gentity_t* self, vec3_t start, vec3_t dir);
gentity_t* fire_nail(gentity_t* self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up);
gentity_t* fire_prox(gentity_t* self, vec3_t start, vec3_t aimdir);

void G_RunMover(gentity_t* ent);
void Touch_DoorTrigger(gentity_t* ent, gentity_t* other, trace_t* trace);

void trigger_teleporter_touch(gentity_t* self, gentity_t* other, trace_t* trace);

void TeleportPlayer(gentity_t* player, vec3_t origin, vec3_t angles);
void DropPortalSource(gentity_t* ent);
void DropPortalDestination(gentity_t* ent);

qboolean LogAccuracyHit(gentity_t* target, gentity_t* attacker);
void CalcMuzzlePoint(gentity_t* ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint);
void SnapVectorTowards(vec3_t v, vec3_t to);
qboolean CheckGauntletAttack(gentity_t* ent);
void Weapon_HookFree(gentity_t* ent);
void Weapon_HookThink(gentity_t* ent);

int TeamCount(int ignoreClientNum, team_t team);
int TeamLeader(int team);
team_t PickTeam(int ignoreClientNum);
void SetClientViewAngle(gentity_t* ent, vec3_t angle);
gentity_t* SelectSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot);
void CopyToBodyQue(gentity_t* ent);
void ClientRespawn(gentity_t* ent);
void BeginIntermission(void);
void InitBodyQue(void);
void ClientSpawn(gentity_t* ent);
void player_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod);
void AddScore(gentity_t* ent, vec3_t origin, int score);
qboolean FreezeTagInGame(void);
qboolean FreezeTagInWarmup(void);
void CalculateRanks(void);
void CalculateAwards(void);
qboolean SpotWouldTelefrag(gentity_t* spot);

qboolean ConsoleCommand(void);
void G_ProcessIPBans(void);
qboolean G_FilterPacket(char* from);
void G_InitAccessList(void);
void G_SaveAccessList(void);
void G_SetAccess(uint64_t steamId, int level, qboolean temporary);
int G_AccessCount(void);
qboolean G_AccessAt(int index, uint64_t* steamId, int* level, qboolean* temporary);
int G_GetAccessBySteamId(uint64_t steamId);
int  G_GetAccess(int clientNum);
void Svcmd_ForceShuffle_f(void);

void FireWeapon(gentity_t* ent);
void G_StartKamikaze(gentity_t* ent);

#define SCORES_LIST_ROOM 990

void FreeForAllScoreboardMessage(gentity_t* ent);
int STAT_GetBestWeapon(gclient_t *cl);
void FFAScoreboardMessage_impl(void);
void DuelScoreboardMessage_impl(void);
void DuelScoreboardMessage(gentity_t *ent);
void RaceScoreboardMessage(gentity_t *ent);
void QDECL PrintMsg(gentity_t *ent, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void TeamDeathmatchScoreboardMessage(gentity_t *ent);
void TeamDeathmatchStatisticsMessage(gentity_t *ent);
void ClanArenaScoreboardMessage(gentity_t *ent);
void ClanArenaStatisticsMessage(gentity_t *ent);
void CaptureTheFlagScoreboardMessage(gentity_t *ent);
void CaptureTheFlagStatisticsMessage(gentity_t *ent);
void FreezeTagScoreboardMessage(gentity_t *ent);
void RedRoverScoreboardMessage(gentity_t *ent);
void ADScoreboardMessage(gentity_t *ent);
void SendScoreboardMessageToTeam(int team);
void AddDuelScore(void);

void MoveClientToIntermission(gentity_t* ent);
void FindIntermissionPoint(void);
void SetLeader(int team, int client);
void CheckTeamLeader(int team);
void G_RunThink(gentity_t* ent);
void SetWarmupState(int warmupTime);
qboolean TeamsPresent(void);
qboolean CheckForfeitConditions(gentity_t *ent, int mode);
void ForfeitMatch(void);
qboolean G_CheckTeamBalance(void);
qboolean BG_IsScoreBasedGameType(void);
qboolean ScoreIsTied(void);
void QDECL G_LogPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void LogExit(int isShutdown, int restart, const char* string);
void SendScoreboardMessageToAllClients(void);
void QDECL G_Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void QDECL G_Error(const char* fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void G_ShutdownGame(int restart);
void G_InitGame(int levelTime, int randomSeed, int restart);
void G_RunFrame(int levelTime);
void MP_Pause(gentity_t* ent, int duration);
void MP_PauseThink(void);

int STAT_GetBestWeapon(gclient_t *cl);
void STAT_AddPlayerMedalStat(gentity_t *ent, const char *medal);
void STAT_AddRoundOverStat(int round, int winTeam, int isDraw);
void STAT_AddShots(gentity_t *ent, int weapon, int amount);
void STAT_AddHits(gentity_t *ent, int weapon, int amount);
void STAT_AddPlayerDeathStat(gentity_t *self, gentity_t *attacker, int mod);
void STAT_AddDamageStat(gentity_t *targ, gentity_t *attacker, int mod, int damage);
void STAT_UpdatePlaytime(gentity_t *ent);
void PlayerAwardEV(gentity_t *ent, int award);
void PlayerAwardEF(gentity_t *ent, int persIdx, int efBit);
void LastManStanding(int team);
qboolean G_ObfuscateEnemyInfoInSnapshotCheck(int clientNum1, int clientNum2);
qboolean G_ClientAlwaysInSnapshot(int viewerNum, int clientNum);
qboolean G_SnapshotSkipEntity(int viewerNum, int entityNum);
qboolean G_ClientIsAdmin(int clientNum);
qboolean G_EntityAlwaysInSnapshot(int entityNum);

void CheckTournament(void);
void AddDuelPlayer(void);
void AddDuelScore(void);
gclient_t *G_NextDuelPlayer(void);

void CTF_CheckTeamItems(void);

void OneFCTF_CheckTeamItems(void);

void Obelisk_CheckTeamItems(void);

void Harvester_CheckTeamItems(void);
void Harvester_RegisterItems(void);

void SP_race_point(gentity_t *ent);
void Touch_RaceCheckpoint(gentity_t *self, gentity_t *other, trace_t *trace);
void Cmd_RaceInit_f(gentity_t *ent);
void Cmd_RacePoint_f(gentity_t *ent);

void DOM_FragBonuses(gentity_t *attacker, gentity_t *victim);

void AD_RoundStateTransition(void);
qboolean AD_CheckExitRules(int doExit);
void AD_PlayerKilled(int team);
int AD_IsInPlayState(void);
int AD_CanScore(void);

extern vmCvar_t g_rrInfected;
extern vmCvar_t g_rrInfectedSpreadTime;
extern vmCvar_t g_rrInfectedSpreadWarningTime;
extern vmCvar_t g_rrInfectedSurvivorScoreMethod;
extern vmCvar_t g_rrInfectedSurvivorScoreRate;
extern vmCvar_t g_rrInfectedSurvivorScoreBonus;
extern vmCvar_t g_rrInfectedSurvivorMinSpeed;
extern vmCvar_t g_rrInfectedSurvivorPingRate;
extern vmCvar_t g_rrInfectedZombieSpeed;
extern vmCvar_t g_rrInfectedZombieHealthBonus;
extern vmCvar_t g_rrInfectedZombieFragBonus;
extern vmCvar_t g_rrDamageScoreBonus;
extern vmCvar_t g_rrDeathScorePenalty;

void RR_InitRoundState(void);
qboolean RR_CheckExitRules(int doExit);
void RR_RoundStateTransition(void);
void RR_Think(void);
void RR_OnPlayerDeath(gentity_t *victim);
void RR_SpreadInfection(void);
void ClientSpawn_RedRover(gentity_t *ent);
team_t PickTeam_RoundAware(int clientNum);
void RR_SurvivorBonuses(int mode);
extern int rr_lastRedClient;
extern int rr_lastBlueClient;

int RR_GetRoundState(void);
int CheckRoundTimeout(int redCount, int blueCount);
void ClientBegin_RoundBased_impl(gentity_t *ent);
void Team_InitGame(void);
void DOM_CheckPlayers(void);
int AD_GetRoundState(void);
int AD_AttackingTeam(void);
int AD_DefendingTeam(void);
qboolean AD_CheckRoundEnd(int redAlive, int blueAlive);

void Freeze_RoundStateTransition(void);
int Freeze_GetRoundState(void);
void Freeze_InstaKill(gentity_t *self, int mode);
void Freeze_Think(void);
void Freeze_ClientThawCheck(gentity_t *ent, int msec);
void Freeze_AutoThaw(int team);
void G_ThrowFlag(gentity_t *self, vec3_t velocity, vec3_t point);
void CaptureZone_Touch(gentity_t *self, gentity_t *other);
int Team_TouchOurFlag(gentity_t *ent, gentity_t *other, int team, gentity_t *thrown, int score);
const char* G_GametypeDisplayName(void);
void Drop_DamagedHealth(gentity_t *attacker, gentity_t *targ, int amount);
qboolean Freeze_TeamFrozen(int team);
qboolean Freeze_GameIsOver(int *aliveCounts, int *healthTotals);

int CA_GetRoundState(void);
void CA_RoundStateTransition(void);
qboolean CA_CheckExitRules(int doExit);
qboolean CA_AdjustDamage(gentity_t *target, gentity_t *attacker, int *take, int *asave);
qboolean AD_AdjustDamage(gentity_t *target, gentity_t *attacker, int *take, int *asave);
qboolean RR_AdjustDamage(gentity_t *target, gentity_t *attacker, int *take, int *asave);
void CA_PlayerKilled(int team);
void G_RegisterCvars(void);
void G_UpdateCustomSettings(void);

char* ClientConnect(int clientNum, qboolean firstTime, qboolean isBot);
void ClientUserinfoChanged(int clientNum);
void G_UpdatePlayQueue(void);
void G_CheckPlayQueueChanged(void);
void ClientDisconnect(int clientNum);
void ClientBegin(int clientNum);
void ClientCommand(int clientNum);
void RACE_ClientBegin(gentity_t* ent);
void ClientBegin_RoundBased(gentity_t* ent);
void DamagePlum(gentity_t* attacker, gentity_t* victim, int damage, int mod);
void G_AutoAction(void);
void G_AutoActionClientConnect(gentity_t* ent);
void Freeze_ClientBegin(gentity_t* ent);
void RR_ClientBegin(gentity_t* ent);
void SelectSpawnWeapon(gentity_t* ent);
void G_GiveStartingWeapons(gentity_t* ent);
void G_SetInfiniteAmmo(gentity_t* ent);
void G_SetStartingHealth(gentity_t* ent);
extern const char* weaponShortName[WP_NUM_WEAPONS];
void Team_LivingTeamCounts(int *outRed, int *outBlue);
void Team_ConnectedTeamCounts(int counts[TEAM_NUM_TEAMS]);
int G_GetAccess(int clientNum);
qboolean G_FloodCheck(gclient_t* client);
void G_ReleaseGrapple(gentity_t* ent);
void STAT_InitClient(gentity_t* ent);
void STAT_PublishClientDisconnect(gclient_t* client, int reason);
void STAT_SubscribeClient(gentity_t* ent);
void STAT_LogDisconnect(int clientNum);
void STAT_MatchEnd(int isShutdown, int restart, const char* exitMsg);

void ClientThink(int clientNum);
void ClientEndFrame(gentity_t* ent);
void G_RunClient(gentity_t* ent);

qboolean OnSameTeam(gentity_t* ent1, gentity_t* ent2);
int G_VoiceFilter(int speaker, int listener);
void Team_CheckDroppedItem(gentity_t* dropped);
qboolean CheckObeliskAttack(gentity_t* obelisk, gentity_t* attacker);

void* G_Alloc(int size);
void G_InitMemory(void);
void Svcmd_GameMem_f(void);

void G_ReadSessionData(gclient_t* client);
void G_InitSessionData(gclient_t* client, char* userinfo);

void G_InitWorldSession(void);
void G_WriteSessionData(void);

void UpdateTournamentInfo(void);
void SpawnModelsOnVictoryPads(void);
void Svcmd_AbortPodium_f(void);

void G_InitBots(qboolean restart);
char* G_GetBotInfoByNumber(int num);
char* G_GetBotInfoByName(const char* name);
void G_CheckBotSpawn(void);
void G_RemoveQueuedBotBegin(int clientNum);
qboolean G_BotConnect(int clientNum, qboolean restart);
void Svcmd_AddBot_f(void);
void G_AddBot(const char* name, float skill, const char* team, int delay, char* altname);

void SP_info_tour_point(gentity_t* ent);
int G_TrainingHumanClient(void);
int G_TrainingBotClient(void);
void Cmd_SkipTraining(int clientNum);
void G_TrainingFrame(void);
const char* G_TrainingText(const char* key);
const char* G_TrainingMessage(const gentity_t* point);
const char* G_TrainingSay(const char* command);

int BotNumBots(void);
float BotGetSkill(int client);
void BotSetSkill(int client, float skill);
void Svcmd_BotList_f(void);
void BotInterbreedEndMatch(void);

#define MAX_FILEPATH 144

typedef struct bot_settings_s {
    char characterfile[MAX_FILEPATH];
    float skill;
    char team[MAX_FILEPATH];
} bot_settings_t;

int BotAISetup(int restart);
int BotAIShutdown(int restart);
int BotAILoadMap(int restart);
int BotAISetupClient(int client, struct bot_settings_s* settings, qboolean restart);
int BotAIShutdownClient(int client, qboolean restart);
int BotAIStartFrame(int time);
void BotTestAAS(vec3_t origin);

#include "g_team.h"

extern level_locals_t level;
extern gentity_t g_entities[MAX_GENTITIES];
extern gclient_t g_clients[MAX_CLIENTS];
extern gameImport_t *imports;

#define FOFS(x) ((size_t)&(((gentity_t*)0)->x))

extern vmCvar_t g_version;
extern vmCvar_t g_gametype;
extern vmCvar_t g_dedicated;
extern vmCvar_t g_cheats;
extern vmCvar_t g_maxclients;
extern vmCvar_t g_restarted;

extern vmCvar_t g_dmflags;
extern vmCvar_t g_fraglimit;
extern vmCvar_t g_timelimit;
extern vmCvar_t g_capturelimit;
extern vmCvar_t g_friendlyFire;
extern vmCvar_t g_password;
extern vmCvar_t g_needpass;
extern vmCvar_t g_gravity;
extern vmCvar_t g_speed;
extern vmCvar_t g_knockback;
extern vmCvar_t g_inactivity;
extern vmCvar_t g_inactivityWarning;
extern vmCvar_t g_dropInactive;
extern vmCvar_t g_debugInactivity;
extern vmCvar_t g_debugMove;
extern vmCvar_t g_debugAlloc;
extern vmCvar_t g_debugDamage;
extern vmCvar_t g_weaponRespawn;
extern vmCvar_t g_motd;
extern vmCvar_t g_warmup;
extern vmCvar_t g_doWarmup;
extern vmCvar_t g_gameState;
extern vmCvar_t sv_warmupReadyPercentage;
extern vmCvar_t g_warmupDelay;
extern vmCvar_t g_warmupReadyDelay;
extern vmCvar_t g_warmupReadyDelayAction;
extern vmCvar_t g_lastManStandingMessage;
extern vmCvar_t bot_autoReady;
extern vmCvar_t g_teamForcePresent;
extern vmCvar_t g_allowVote;
extern vmCvar_t g_allowVoteMidGame;
extern vmCvar_t g_allowSpecVote;
extern vmCvar_t g_voteFlags;
extern vmCvar_t g_voteDelay;
extern vmCvar_t g_voteLimit;
extern vmCvar_t g_teamAutoJoin;
extern vmCvar_t g_teamForceBalance;
extern vmCvar_t g_banIPs;
extern vmCvar_t g_filterBan;
extern vmCvar_t g_obeliskHealth;
extern vmCvar_t g_obeliskRegenPeriod;
extern vmCvar_t g_obeliskRegenAmount;
extern vmCvar_t g_obeliskRespawnDelay;
extern vmCvar_t g_cubeTimeout;
extern vmCvar_t g_knockback_z;
extern vmCvar_t g_knockback_z_self;
extern vmCvar_t g_knockback_cripple;
extern vmCvar_t g_enableDust;
extern vmCvar_t g_proxMineTimeout;
extern vmCvar_t g_infiniteAmmo;
extern vmCvar_t g_runes;

extern vmCvar_t g_damage_g;
extern vmCvar_t g_damage_mg;
extern vmCvar_t g_damage_sg;
extern vmCvar_t g_damage_gl;
extern vmCvar_t g_damage_rl;
extern vmCvar_t g_damage_lg;
extern vmCvar_t g_damage_rg;
extern vmCvar_t g_damage_pg;
extern vmCvar_t g_damage_bfg;
extern vmCvar_t g_damage_gh;
extern vmCvar_t g_damage_ng;
extern vmCvar_t g_damage_cg;
extern vmCvar_t g_damage_hmg;
extern vmCvar_t g_damage_pl;

extern vmCvar_t g_damage_sg_outer;
extern vmCvar_t g_damage_sg_falloff;
extern vmCvar_t g_range_sg_falloff;
extern vmCvar_t g_damage_lg_falloff;
extern vmCvar_t g_range_lg_falloff;
extern vmCvar_t g_headShotDamage_rg;
extern vmCvar_t g_railJump;

extern vmCvar_t g_knockback_g;
extern vmCvar_t g_knockback_mg;
extern vmCvar_t g_knockback_sg;
extern vmCvar_t g_knockback_gl;
extern vmCvar_t g_knockback_rl;
extern vmCvar_t g_knockback_rl_self;
extern vmCvar_t g_knockback_lg;
extern vmCvar_t g_knockback_rg;
extern vmCvar_t g_knockback_pg;
extern vmCvar_t g_knockback_pg_self;
extern vmCvar_t g_knockback_bfg;
extern vmCvar_t g_knockback_gh;
extern vmCvar_t g_knockback_ng;
extern vmCvar_t g_knockback_pl;
extern vmCvar_t g_knockback_cg;
extern vmCvar_t g_knockback_hmg;

extern vmCvar_t weapon_reload_mg;
extern vmCvar_t weapon_reload_sg;
extern vmCvar_t weapon_reload_gl;
extern vmCvar_t weapon_reload_rl;
extern vmCvar_t weapon_reload_lg;
extern vmCvar_t weapon_reload_rg;
extern vmCvar_t weapon_reload_pg;
extern vmCvar_t weapon_reload_bfg;
extern vmCvar_t weapon_reload_gh;
extern vmCvar_t weapon_reload_ng;
extern vmCvar_t weapon_reload_prox;
extern vmCvar_t weapon_reload_cg;
extern vmCvar_t weapon_reload_hmg;

extern vmCvar_t g_splashdamage_gl;
extern vmCvar_t g_splashdamage_rl;
extern vmCvar_t g_splashdamage_pg;
extern vmCvar_t g_splashdamage_bfg;
extern vmCvar_t g_splashdamage_pl;
extern vmCvar_t g_splashdamageOffset;
extern vmCvar_t g_rocketsplashOffset;

extern vmCvar_t g_max_knockback;

extern vmCvar_t g_splashradius_gl;
extern vmCvar_t g_splashradius_rl;
extern vmCvar_t g_splashradius_pg;
extern vmCvar_t g_splashradius_bfg;
extern vmCvar_t g_splashradius_pl;

extern vmCvar_t g_velocity_gl;
extern vmCvar_t g_velocity_rl;
extern vmCvar_t g_velocity_pg;
extern vmCvar_t g_velocity_bfg;
extern vmCvar_t g_velocity_gh;

extern vmCvar_t g_accelFactor_rl;
extern vmCvar_t g_accelFactor_pg;
extern vmCvar_t g_accelFactor_bfg;
extern vmCvar_t g_accelRate_rl;
extern vmCvar_t g_accelRate_pg;
extern vmCvar_t g_accelRate_bfg;

extern vmCvar_t weapon_gravity_rl;
extern vmCvar_t weapon_gravity_pg;
extern vmCvar_t weapon_gravity_bfg;
extern vmCvar_t weapon_gravity_ng;

extern vmCvar_t g_nailspeed;
extern vmCvar_t g_nailcount;
extern vmCvar_t g_nailspread;
extern vmCvar_t g_nailbounce;
extern vmCvar_t g_nailbouncepercentage;

extern vmCvar_t g_guidedRocket;

extern vmCvar_t pmove_AirControl;
extern vmCvar_t pmove_AutoHop;
extern vmCvar_t pmove_CrouchSlide;
extern vmCvar_t pmove_DoubleJump;
extern vmCvar_t pmove_NoPlayerClip;

extern vmCvar_t g_loadout;
extern vmCvar_t g_disableLoadout;
int BG_ParseGametypeStringtoFlag(const char *str);

extern vmCvar_t g_ammoPack;
extern vmCvar_t g_ammoRespawn;
extern vmCvar_t g_spawnItemAmmo;

extern vmCvar_t g_teamsize;
extern vmCvar_t g_teamSizeMin;
extern vmCvar_t g_overtime;
extern vmCvar_t g_scorelimit;
extern vmCvar_t g_mercylimit;
extern vmCvar_t g_mercytime;
extern vmCvar_t g_rrAllowNegativeScores;
extern vmCvar_t sv_quitOnExitLevel;
extern vmCvar_t g_training;
extern vmCvar_t g_itemHeight;
extern vmCvar_t g_itemTimers;
extern vmCvar_t g_specItemTimers;
extern vmCvar_t g_quadDamageFactor;
extern vmCvar_t g_freezeRoundDelay;
extern vmCvar_t g_timeoutCount;

extern vmCvar_t g_forceDmgThroughSurface;
extern vmCvar_t g_dmgThroughSurfaceDistance;
extern vmCvar_t g_dmgThroughSurfaceDampening;
extern vmCvar_t g_dmgThroughSurfaceAngularThreshold;

extern vmCvar_t g_playerCylinders;

extern vmCvar_t sv_fps;

extern vmCvar_t roundtimelimit;
extern vmCvar_t g_roundWarmupDelay;
extern vmCvar_t roundlimit;
extern vmCvar_t g_lastManStandingWarning;
extern vmCvar_t g_roundDrawLivingCount;
extern vmCvar_t g_roundDrawHealthCount;
extern vmCvar_t g_spawnArmor;
extern vmCvar_t g_adElimScoreBonus;
extern vmCvar_t g_adCaptureScoreBonus;

extern vmCvar_t g_freezeThawTick;
extern vmCvar_t g_freezeProtectedSpawnTime;
extern vmCvar_t g_freezeThawTime;
extern vmCvar_t g_freezeAutoThawTime;
extern vmCvar_t g_freezeThawRadius;
extern vmCvar_t g_freezeThawThroughSurface;
extern vmCvar_t g_freezeThawWinningTeam;
extern vmCvar_t g_freezeRemovePowerupsOnRound;
extern vmCvar_t g_freezeResetHealthOnRound;
extern vmCvar_t g_freezeResetArmorOnRound;
extern vmCvar_t g_freezeResetWeaponsOnRound;

extern vmCvar_t g_lagHaxHistory;
extern vmCvar_t g_lagHaxMs;

void HAX_Init(void);
void HAX_Update(gentity_t *ent);
void HAX_Clear(gentity_t *ent);
void HAX_Begin(gentity_t *shooter, int startTime);
void HAX_End(gentity_t *shooter);

extern vmCvar_t g_ironsights_mg;

extern vmCvar_t g_quadHog;
extern vmCvar_t g_quadHogTime;
extern vmCvar_t g_damagePlums;
void QuadHog_Spawn(void);
void QuadHog_Check(void);
void G_CountSpawnSpots(void);
void G_SpawnRunes(void);

extern vmCvar_t g_startingWeapons;
extern vmCvar_t g_startingHealth;
extern vmCvar_t g_startingHealthBonus;
extern vmCvar_t g_startingArmor;
extern vmCvar_t armor_tiered;
extern vmCvar_t g_startingAmmo_g;
extern vmCvar_t g_startingAmmo_mg;
extern vmCvar_t g_startingAmmo_sg;
extern vmCvar_t g_startingAmmo_gl;
extern vmCvar_t g_startingAmmo_rl;
extern vmCvar_t g_startingAmmo_lg;
extern vmCvar_t g_startingAmmo_rg;
extern vmCvar_t g_startingAmmo_pg;
extern vmCvar_t g_startingAmmo_bfg;
extern vmCvar_t g_startingAmmo_gh;
extern vmCvar_t g_startingAmmo_ng;
extern vmCvar_t g_startingAmmo_pl;
extern vmCvar_t g_startingAmmo_cg;
extern vmCvar_t g_startingAmmo_hmg;

extern vmCvar_t bot_breakPoint;
extern vmCvar_t bot_debugVar;
extern vmCvar_t bot_dynamicSkill;
extern vmCvar_t bot_followDist;
extern vmCvar_t bot_followMe;
extern vmCvar_t bot_gauntlet;
extern vmCvar_t bot_gauntletOnly;
extern vmCvar_t bot_hud;
extern vmCvar_t bot_instaGibAimSkill;
extern vmCvar_t bot_itemDelayTime;
extern vmCvar_t bot_showAreaNumber;
extern vmCvar_t bot_showAreas;
extern vmCvar_t bot_showAvoidSpots;
extern vmCvar_t bot_showPath;
extern vmCvar_t bot_showTourPoints;
extern vmCvar_t bot_startingSkill;
extern vmCvar_t bot_teamkill;
extern vmCvar_t bot_training;
extern vmCvar_t g_accessFile;
extern vmCvar_t g_adTouchScoreBonus;
extern vmCvar_t g_allTalk;
extern vmCvar_t g_allowCustomHeadmodels;
extern vmCvar_t g_allowForfeit;
extern vmCvar_t g_allowKill;
extern vmCvar_t g_ammoPackHack;
extern vmCvar_t g_autoAction;
extern vmCvar_t g_battleSuitDampen;
extern vmCvar_t g_bestStartingWeapons;
extern vmCvar_t g_botSpawnList;
extern vmCvar_t g_complaintDamageThreshold;
extern vmCvar_t g_complaintLimit;
extern vmCvar_t g_customSettings;
extern vmCvar_t g_debugFlags;
extern vmCvar_t g_debugThawTime;
extern vmCvar_t g_debugVampiricDamage;
extern vmCvar_t g_domCapTime;
extern vmCvar_t g_domDistressThreshold;
extern vmCvar_t g_domEnableContention;
extern vmCvar_t g_domNeutralFlag;
extern vmCvar_t g_domScoreRate;
extern vmCvar_t g_domTeammateCapScale;
extern vmCvar_t g_dropCmds;
extern vmCvar_t g_dropDamagedHealth;
extern vmCvar_t g_dropPowerups;
extern vmCvar_t g_dropSkulls;
extern vmCvar_t g_droppedFlagBonus;
extern vmCvar_t g_droppedPowerupsDecay;
extern vmCvar_t g_enableDebugTrace;
extern vmCvar_t g_enemyTeamRespawnRatio;
extern vmCvar_t g_factory;
extern vmCvar_t g_factoryTitle;
extern vmCvar_t g_flagBounce;
extern vmCvar_t g_flagPhysics;
extern vmCvar_t g_flightRefuelRate;
extern vmCvar_t g_flightThrust;
extern vmCvar_t g_floodprot_decay;
extern vmCvar_t g_floodprot_maxcount;
extern vmCvar_t g_forceAtmosphericEffects;
extern vmCvar_t g_forceSendConfigstring;
extern vmCvar_t g_forceSmallScoreboardMessage;
extern vmCvar_t g_freezeEnvironmentalRespawnDelay;
extern vmCvar_t g_friendlyFireDampen;
extern vmCvar_t g_gauntletSpeedFactor;
extern vmCvar_t g_grantItemOnSpawn;
extern vmCvar_t g_instaGib;
extern vmCvar_t g_kamiAttenuate;
extern vmCvar_t g_kamiMinRatio;
extern vmCvar_t g_kickBadUserinfo;
extern vmCvar_t g_latchedHookOffset;
extern vmCvar_t g_levelStartTime;
extern vmCvar_t g_lightningDischarge;
extern vmCvar_t g_maxDeferredSpawns;
extern vmCvar_t g_maxFlightFuel;
extern vmCvar_t g_midAirMinHeight;
extern vmCvar_t g_neutralFlagPingRate;
void G_PointOfInterest(gentity_t* ent, int duration, int payload);
extern vmCvar_t g_playerModelScale;
extern vmCvar_t g_playerheadScale;
extern vmCvar_t g_playerheadScaleOffset;
extern vmCvar_t g_playerheadmodelOverride;
extern vmCvar_t g_playermodelOverride;
extern vmCvar_t g_powerupRespawn;
extern vmCvar_t g_quadHogIdle;
extern vmCvar_t g_quadHogPingRate;
extern vmCvar_t g_regenArmor;
extern vmCvar_t g_regenArmorAfterHealth;
extern vmCvar_t g_regenArmorRate;
extern vmCvar_t g_regenHealth;
extern vmCvar_t g_regenHealthRate;
extern vmCvar_t g_respawn_delay_max;
extern vmCvar_t g_respawn_delay_min;
extern vmCvar_t g_returnFlagOnSuicide;
extern vmCvar_t g_rrRoundScoreBonus;
extern vmCvar_t g_shuffle_automatic;
extern vmCvar_t g_shuffle_automatic_minplayers;
extern vmCvar_t g_shuffle_minplayers;
extern vmCvar_t g_shuffle_timedelay;
extern vmCvar_t g_skipTrainingEnable;
extern vmCvar_t g_trainingSpanish;
extern vmCvar_t g_spawnArmorDmgScale;
extern vmCvar_t g_spawnDelayRandom_key;
extern vmCvar_t g_spawnDelayRandom_powerup;
extern vmCvar_t g_spawnDelay_key;
extern vmCvar_t g_spawnDelay_powerup;
extern vmCvar_t g_spawnItemArmor;
extern vmCvar_t g_spawnItemHealth;
extern vmCvar_t g_spawnItemHoldable;
extern vmCvar_t g_spawnItemPowerup;
extern vmCvar_t g_spawnItemWeapons;
extern vmCvar_t g_spawnMinDistance;
extern vmCvar_t g_spawnRandomRatio;
extern vmCvar_t g_suddenDeathRespawn;
extern vmCvar_t g_suddenDeathRespawnIncrement;
extern vmCvar_t g_suddenDeathRespawnMax;
extern vmCvar_t g_suddenDeathRespawnPrint;
extern vmCvar_t g_suddenDeathRespawnStart;
extern vmCvar_t g_suddenDeathRespawnTick;
extern vmCvar_t g_switchTeamDelay;
extern vmCvar_t g_tackleFlag;
extern vmCvar_t g_teamSpawnAsSpec;
extern vmCvar_t g_teamSpecFreeCam;
extern vmCvar_t g_teamSpecSayEnable;
extern vmCvar_t g_throwFlagForwardMult;
extern vmCvar_t g_throwFlagVelocity;
extern vmCvar_t g_timeoutLen;
extern vmCvar_t g_vampiricDamage;
extern vmCvar_t gamedate;
extern vmCvar_t practiceflags;
extern vmCvar_t g_advancedTraining;
extern vmCvar_t g_trainingSpawn;
extern vmCvar_t g_trainingTarget;
extern vmCvar_t sv_mapname;
extern vmCvar_t ui_singlePlayerActive;
extern vmCvar_t weapon_reload_gauntlet;
extern vmCvar_t weapon_reload_hook;
void trap_Print(const char* text);
void trap_Error(const char* fmt, ...) __attribute__((noreturn));
int trap_Milliseconds(void);
int trap_RealTime(qtime_t* qtime);
int trap_Argc(void);
void trap_Argv(int n, char* buffer, int bufferLength);
void trap_Args(char* buffer, int bufferLength);
int trap_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode);
void trap_FS_Read(void* buffer, int len, fileHandle_t f);
void trap_FS_Write(const void* buffer, int len, fileHandle_t f);
void trap_FS_FCloseFile(fileHandle_t f);
int trap_FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
int trap_FS_Seek(fileHandle_t f, long offset, int origin);
void trap_SendConsoleCommand(int exec_when, const char* text);
void trap_Cvar_Register(vmCvar_t* cvar, const char* var_name, const char* value, int flags);
void trap_Cvar_Update(vmCvar_t* cvar);
void trap_Cvar_Set(const char* var_name, const char* value);
int trap_Cvar_VariableIntegerValue(const char* var_name);
float trap_Cvar_VariableValue(const char* var_name);
void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);
void trap_LocateGameData(gentity_t* gEnts, int numGEntities, int sizeofGEntity_t, playerState_t* gameClients, int sizeofGameClient);
void trap_DropClient(int clientNum, const char* reason);
void trap_SendServerCommand(int clientNum, const char* text);
void trap_SetConfigstring(int num, const char* string);
void trap_GetConfigstring(int num, char* buffer, int bufferSize);
void trap_GetUserinfo(int num, char* buffer, int bufferSize);
qboolean trap_HasAchievement(int clientNum, int achievementId);
void trap_SetAchievement(int clientNum, int achievementId);
void trap_SetUserinfo(int num, const char* buffer);
void trap_GetServerinfo(char* buffer, int bufferSize);
void trap_SetBrushModel(gentity_t* ent, const char* name);
void trap_Trace(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask);
void trap_TraceCapsule(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask);
int trap_PointContents(const vec3_t point, int passEntityNum);
qboolean trap_InPVS(const vec3_t p1, const vec3_t p2);
qboolean trap_InPVSIgnorePortals(const vec3_t p1, const vec3_t p2);
void trap_AdjustAreaPortalState(gentity_t* ent, qboolean open);
qboolean trap_AreasConnected(int area1, int area2);
void trap_LinkEntity(gentity_t* ent);
void trap_UnlinkEntity(gentity_t* ent);
int trap_EntitiesInBox(const vec3_t mins, const vec3_t maxs, int* entityList, int maxcount);
qboolean trap_EntityContact(const vec3_t mins, const vec3_t maxs, const gentity_t* ent);
int trap_BotAllocateClient(void);
void trap_BotFreeClient(int clientNum);
void trap_GetUsercmd(int clientNum, usercmd_t* cmd);
qboolean trap_GetEntityToken(char* buffer, int bufferSize);

int trap_DebugPolygonCreate(int color, int numPoints, vec3_t* points);
void trap_DebugPolygonDelete(int id);

int trap_BotLibSetup(void);
int trap_BotLibShutdown(void);
int trap_BotLibVarSet(char* var_name, char* value);
int trap_BotLibVarGet(char* var_name, char* value, int size);
int trap_BotLibDefine(char* string);
int trap_BotLibStartFrame(float time);
int trap_BotLibLoadMap(const char* mapname);
int trap_BotLibUpdateEntity(int ent, void * bue);
int trap_BotLibTest(int parm0, char* parm1, vec3_t parm2, vec3_t parm3);

int trap_BotGetSnapshotEntity(int clientNum, int sequence);
int trap_BotGetServerCommand(int clientNum, char* message, int size);
void trap_BotUserCommand(int client, usercmd_t* ucmd);

int trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int* areas, int maxareas);
int trap_AAS_AreaInfo(int areanum, void * info);
void trap_AAS_EntityInfo(int entnum, void * info);

int trap_AAS_Initialized(void);
void trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs);
float trap_AAS_Time(void);

int trap_AAS_PointAreaNum(vec3_t point);
int trap_AAS_PointReachabilityAreaIndex(vec3_t point);
int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int* areas, vec3_t* points, int maxareas);

int trap_AAS_PointContents(vec3_t point);
int trap_AAS_NextBSPEntity(int ent);
int trap_AAS_ValueForBSPEpairKey(int ent, char* key, char* value, int size);
int trap_AAS_VectorForBSPEpairKey(int ent, char* key, vec3_t v);
int trap_AAS_FloatForBSPEpairKey(int ent, char* key, float* value);
int trap_AAS_IntForBSPEpairKey(int ent, char* key, int* value);

int trap_AAS_AreaReachability(int areanum);

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int trap_AAS_EnableRoutingArea(int areanum, int enable);
int trap_AAS_PredictRoute(void * route, int areanum, vec3_t origin, int goalareanum, int travelflags, int maxareas, int maxtime, int stopevent, int stopcontents, int stoptfl, int stopareanum);

int trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags, void * altroutegoals, int maxaltroutegoals, int type);
int trap_AAS_Swimming(vec3_t origin);
int trap_AAS_PredictClientMovement(void * move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);

void trap_EA_Say(int client, char* str);
void trap_EA_SayTeam(int client, char* str);
void trap_EA_Command(int client, char* command);

void trap_EA_Action(int client, int action);
void trap_EA_Gesture(int client);
void trap_EA_Talk(int client);
void trap_EA_Attack(int client);
void trap_EA_Use(int client);
void trap_EA_Respawn(int client);
void trap_EA_Crouch(int client);
void trap_EA_MoveUp(int client);
void trap_EA_MoveDown(int client);
void trap_EA_MoveForward(int client);
void trap_EA_MoveBack(int client);
void trap_EA_MoveLeft(int client);
void trap_EA_MoveRight(int client);
void trap_EA_SelectWeapon(int client, int weapon);
void trap_EA_Jump(int client);
void trap_EA_DelayedJump(int client);
void trap_EA_Move(int client, vec3_t dir, float speed);
void trap_EA_View(int client, vec3_t viewangles);

void trap_EA_EndRegular(int client, float thinktime);
void trap_EA_GetInput(int client, float thinktime, void * input);
void trap_EA_ResetInput(int client);

int trap_BotLoadCharacter(char* charfile, float skill);
void trap_BotFreeCharacter(int character);
float trap_Characteristic_Float(int character, int index);
float trap_Characteristic_BFloat(int character, int index, float min, float max);
int trap_Characteristic_Integer(int character, int index);
int trap_Characteristic_BInteger(int character, int index, int min, int max);
void trap_Characteristic_String(int character, int index, char* buf, int size);

int trap_BotAllocChatState(void);
void trap_BotFreeChatState(int handle);
void trap_BotQueueConsoleMessage(int chatstate, int type, char* message);
void trap_BotRemoveConsoleMessage(int chatstate, int handle);
int trap_BotNextConsoleMessage(int chatstate, void * cm);
int trap_BotNumConsoleMessages(int chatstate);
void trap_BotInitialChat(int chatstate, char* type, int mcontext, char* var0, char* var1, char* var2, char* var3, char* var4, char* var5, char* var6, char* var7);
int trap_BotNumInitialChats(int chatstate, char* type);
int trap_BotReplyChat(int chatstate, char* message, int mcontext, int vcontext, char* var0, char* var1, char* var2, char* var3, char* var4, char* var5, char* var6, char* var7);
int trap_BotChatLength(int chatstate);
void trap_BotEnterChat(int chatstate, int client, int sendto);
void trap_BotGetChatMessage(int chatstate, char* buf, int size);
int trap_StringContains(char* str1, char* str2, int casesensitive);
int trap_BotFindMatch(char* str, void * match, unsigned long int context);
void trap_BotMatchVariable(void * match, int variable, char* buf, int size);
void trap_UnifyWhiteSpaces(char* string);
void trap_BotReplaceSynonyms(char* string, unsigned long int context);
int trap_BotLoadChatFile(int chatstate, char* chatfile, char* chatname);
void trap_BotSetChatGender(int chatstate, int gender);
void trap_BotSetChatName(int chatstate, char* name, int client);
void trap_BotResetGoalState(int goalstate);
void trap_BotRemoveFromAvoidGoals(int goalstate, int number);
void trap_BotResetAvoidGoals(int goalstate);
void trap_BotPushGoal(int goalstate, void * goal);
void trap_BotPopGoal(int goalstate);
void trap_BotEmptyGoalStack(int goalstate);
void trap_BotDumpAvoidGoals(int goalstate);
void trap_BotDumpGoalStack(int goalstate);
void trap_BotGoalName(int number, char* name, int size);
int trap_BotGetTopGoal(int goalstate, void * goal);
int trap_BotGetSecondGoal(int goalstate, void * goal);
int trap_BotChooseLTGItem(int goalstate, vec3_t origin, int* inventory, int travelflags);
int trap_BotChooseNBGItem(int goalstate, vec3_t origin, int* inventory, int travelflags, void * ltg, float maxtime);
int trap_BotTouchingGoal(vec3_t origin, void * goal);
int trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void * goal);
int trap_BotGetNextCampSpotGoal(int num, void * goal);
int trap_BotGetMapLocationGoal(char* name, void * goal);
int trap_BotGetLevelItemGoal(int index, char* classname, void * goal);
float trap_BotAvoidGoalTime(int goalstate, int number);
void trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime);
void trap_BotInitLevelItems(void);
void trap_BotUpdateEntityItems(void);
int trap_BotLoadItemWeights(int goalstate, char* filename);
void trap_BotFreeItemWeights(int goalstate);
void trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child);
void trap_BotSaveGoalFuzzyLogic(int goalstate, char* filename);
void trap_BotMutateGoalFuzzyLogic(int goalstate, float range);
int trap_BotAllocGoalState(int state);
void trap_BotFreeGoalState(int handle);

void trap_BotResetMoveState(int movestate);
void trap_BotMoveToGoal(void * result, int movestate, void * goal, int travelflags);
int trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type);
void trap_BotResetAvoidReach(int movestate);
void trap_BotResetLastAvoidReach(int movestate);
int trap_BotReachabilityArea(vec3_t origin, int testground);
int trap_BotMovementViewTarget(int movestate, void * goal, int travelflags, float lookahead, vec3_t target);
int trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void * goal, int travelflags, vec3_t target);
int trap_BotAllocMoveState(void);
void trap_BotFreeMoveState(int handle);
void trap_BotInitMoveState(int handle, void * initmove);
void trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);

int trap_BotChooseBestFightWeapon(int weaponstate, int* inventory);
void trap_BotGetWeaponInfo(int weaponstate, int weapon, void * weaponinfo);
int trap_BotLoadWeaponWeights(int weaponstate, char* filename);
int trap_BotAllocWeaponState(void);
void trap_BotFreeWeaponState(int weaponstate);
void trap_BotResetWeaponState(int weaponstate);

int trap_GeneticParentsAndChildSelection(int numranks, float* ranks, int* parent1, int* parent2, int* child);

void trap_SnapVector(float* v);

void trap_IncrementSteamStat(int clientNum, int stat, int amount);
qboolean trap_VerifySteamAuth(int clientNum);
qboolean trap_FactoryExists(const char* id);
void trap_GenerateMatchGUID(char* buf, int size);
void trap_StatsEvent(uint64_t steamId, const char* type, const char* document);

void STAT_PlayerConnect(gentity_t* ent);
void STAT_PlayerDisconnect(gentity_t* ent);
void STAT_PlayerSwitchTeam(gentity_t* ent, int oldTeam, int newTeam);
void STAT_PlayerMedal(gentity_t* ent, const char* medal, int total);
void STAT_RoundOver(int round, int winningTeam, qboolean draw);
void STAT_PlayerRaceComplete(gentity_t* ent, int raceTime, qboolean weaponsUsed);
void STAT_PlayerKill(gentity_t* victim, gentity_t* attacker, int mod, int victimStreak);
void STAT_PlayerStats(gclient_t* cl, qboolean aborted, qboolean quit);
void STAT_UpdatePlayTime(gentity_t* ent);
void STAT_MatchStarted(void);
void STAT_MatchReport(qboolean aborted, int restarted, const char* exitMsg);
void G_SetMatchGUID(void);
void STAT_ResetMedals(void);
int STAT_MedalTotal(gentity_t* ent, const char* medal);
uint64_t trap_GetSteamID(int clientNum);
