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
#define GAME_API_VERSION 10

#define SVF_NOCLIENT 0x00000001

#define SVF_CLIENTMASK 0x00000002

#define SVF_BOT 0x00000008
#define SVF_BROADCAST 0x00000020
#define SVF_PORTAL 0x00000040
#define SVF_USE_CURRENT_ORIGIN 0x00000080
#define SVF_SINGLECLIENT 0x00000100
#define SVF_NOSERVERINFO 0x00000200
#define SVF_CAPSULE 0x00000400
#define SVF_NOTSINGLECLIENT 0x00000800

typedef struct {
    entityState_t unused;

    qboolean linked;
    int linkcount;

    int svFlags;

    int singleClient;

    qboolean bmodel;
    vec3_t mins, maxs;
    int contents;

    vec3_t absmin, absmax;

    vec3_t currentOrigin;
    vec3_t currentAngles;

    int ownerNum;
} entityShared_t;

typedef struct {
    entityState_t s;
    entityShared_t r;
} sharedEntity_t;

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

typedef struct {
     void    (*trap_SendConsoleCommand)(const char *text);
     int     (*trap_RealTime)(qtime_t *qtime);
     void    (*trap_Printf)(const char *fmt, ...);
     void    *_unused_03;
     int     (*trap_FS_Write)(const void *buf, int len, fileHandle_t f);
     void    *_unused_05;
     int     (*trap_FS_Read)(void *buf, int len, fileHandle_t f);
     void    *_unused_07;
     int     (*trap_FS_FOpenFile)(const char *name, fileHandle_t *f, fsMode_t mode);
     void    (*trap_FS_FCloseFile)(fileHandle_t f);
     void    (*trap_Error)(const char *fmt, ...);
     float   (*trap_Cvar_VariableValue)(const char *name);
     void    (*trap_Cvar_Update)(vmCvar_t *cv);
     void    (*trap_Cvar_VariableStringBuffer)(const char *name, char *buf, int size);
     void    *_unused_0e;
     void    (*trap_Cvar_Set)(const char *name, const char *value);
     void    *_unused_10;
     void    (*trap_Cvar_Register)(vmCvar_t *cv, const char *name, const char *defaultValue, int flags);
     void    (*trap_Argv)(int n, char *buf, int size);
     void    *_unused_13;
     int     (*trap_Argc)(void);
     void    *_unused_15;
     void    (*trap_LocateGameData)(gentity_t *gEnts, int numGEntities, int sizeofGEntity, gclient_t *clients, int sizeofGClient);
     void    (*trap_DropClient)(int clientNum, const char *reason);
     void    (*trap_SendServerCommand)(int clientNum, const char *text);
     void    (*trap_SetConfigstring)(int index, const char *value);
     void    (*trap_GetConfigstring)(int index, char *buf, int size);
     void    *_unused_1b;
     void    (*trap_GetUserinfo)(int clientNum, char *buf, int size);
     void    (*trap_SetUserinfo)(int clientNum, const char *info);
     void    (*trap_GetServerinfo)(char *buf, int size);
     void    (*trap_SetBrushModel)(gentity_t *ent, const char *model);
     void    (*trap_Trace)(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask);
     void    (*trap_TraceCapsule)(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask);
     int     (*trap_PointContents)(const vec3_t point, int passEntityNum);
     void    *_unused_23;
     void    *_unused_24;
     void    (*trap_AdjustAreaPortalState)(gentity_t *ent, qboolean open);
     void    *_unused_26;
     void    (*trap_LinkEntity)(gentity_t *ent);
     void    (*trap_UnlinkEntity)(gentity_t *ent);
     int     (*trap_EntitiesInBox)(const vec3_t mins, const vec3_t maxs, int *list, int maxcount);
     qboolean(*trap_EntityContact)(const vec3_t mins, const vec3_t maxs, const gentity_t *ent);
     int     (*trap_BotAllocateClient)(void);
     void    (*trap_BotFreeClient)(int clientNum);
     void    (*trap_GetUsercmd)(int clientNum, usercmd_t *cmd);
     qboolean(*trap_GetEntityToken)(char *buf, int size);
     void    *_unused_2f;
     void    *_unused_30;
     int     (*trap_BotLibSetup)(void);
     int     (*trap_BotLibShutdown)(void);
     int     (*trap_BotLibVarSet)(const char *name, const char *value);
     void    *_unused_34;
     int     (*trap_BotLibDefine)(const char *symbol);
     int     (*trap_BotLibStartFrame)(float time);
     int     (*trap_BotLibLoadMap)(const char *mapname);
     int     (*trap_BotLibUpdateEntity)(int entNum, void *entityInfo);
     void    *_unused_39;
     int     (*trap_BotGetSnapshotEntity)(int clientNum, int sequence);
     void    *_unused_3b;
     void    (*trap_BotUserCommand)(int clientNum, usercmd_t *cmd);
     void    *_unused_3d;
     void    *_unused_3e;
     void    (*trap_AAS_EntityInfo)(int entNum, void *info);
     qboolean(*trap_AAS_Initialized)(void);
     void    *_unused_41;
     float   (*trap_AAS_Time)(void);
     int     (*trap_AAS_PointAreaNum)(const vec3_t point);
     void    *_unused_44;
     int     (*trap_AAS_TraceAreas)(const vec3_t start, const vec3_t end, int *areas, void *points, int maxareas);

    void    (*trap_AAS_PresenceTypeBoundingBox)(int presencetype, vec3_t mins, vec3_t maxs);
    int     (*trap_AAS_PointReachabilityAreaIndex)(vec3_t point);
    int     (*trap_AAS_BBoxAreas)(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
    int     (*trap_AAS_AreaInfo)(int areanum, void *info);
    int     (*trap_AAS_PointContents)(vec3_t point);
    int     (*trap_AAS_NextBSPEntity)(int ent);
    int     (*trap_AAS_ValueForBSPEpairKey)(int ent, char *key, char *value, int size);
    int     (*trap_AAS_VectorForBSPEpairKey)(int ent, char *key, vec3_t v);
    int     (*trap_AAS_FloatForBSPEpairKey)(int ent, char *key, float *value);
    int     (*trap_AAS_IntForBSPEpairKey)(int ent, char *key, int *value);
    int     (*trap_AAS_AreaReachability)(int areanum);
    int     (*trap_AAS_AreaTravelTimeToGoalArea)(int areanum, vec3_t origin, int goalareanum, int travelflags);
    int     (*trap_AAS_EnableRoutingArea)(int areanum, int enable);
    int     (*trap_AAS_PredictRoute)(void *route, int areanum, vec3_t origin, int goalareanum, int travelflags, int maxareas, int maxtime, int stopevent, int stopcontents, int stoptfl, int stopareanum);
    int     (*trap_AAS_AlternativeRouteGoals)(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags, void *altroutegoals, int maxaltroutegoals, int type);
    int     (*trap_AAS_Swimming)(vec3_t origin);
    int     (*trap_AAS_PredictClientMovement)(void *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);

    void    (*trap_EA_Command)(int client, char *command);
    void    (*trap_EA_Say)(int client, char *str);
    void    (*trap_EA_SayTeam)(int client, char *str);
    void    (*trap_EA_Action)(int client, int action);
    void    (*trap_EA_Gesture)(int client);
    void    (*trap_EA_Talk)(int client);
    void    (*trap_EA_Attack)(int client);
    void    (*trap_EA_Use)(int client);
    void    (*trap_EA_Respawn)(int client);
    void    (*trap_EA_MoveUp)(int client);
    void    (*trap_EA_MoveDown)(int client);
    void    (*trap_EA_MoveForward)(int client);
    void    (*trap_EA_MoveBack)(int client);
    void    (*trap_EA_MoveLeft)(int client);
    void    (*trap_EA_MoveRight)(int client);
    void    (*trap_EA_Crouch)(int client);
    void    (*trap_EA_SelectWeapon)(int client, int weapon);
    void    (*trap_EA_Jump)(int client);
    void    (*trap_EA_DelayedJump)(int client);
    void    (*trap_EA_Move)(int client, vec3_t dir, float speed);
    void    (*trap_EA_View)(int client, vec3_t viewangles);
    void    (*trap_EA_EndRegular)(int client, float thinktime);
    void    (*trap_EA_GetInput)(int client, float thinktime, void *input);
    void    (*trap_EA_ResetInput)(int client);

    int     (*trap_BotLoadCharacter)(char *charfile, float skill);
    void    (*trap_BotFreeCharacter)(int character);
    float   (*trap_Characteristic_Float)(int character, int index);
    float   (*trap_Characteristic_BFloat)(int character, int index, float min, float max);
    int     (*trap_Characteristic_Integer)(int character, int index);
    int     (*trap_Characteristic_BInteger)(int character, int index, int min, int max);
    void    (*trap_Characteristic_String)(int character, int index, char *buf, int size);

    int     (*trap_BotAllocChatState)(void);
    void    (*trap_BotFreeChatState)(int handle);
    void    (*trap_BotQueueConsoleMessage)(int chatstate, int type, char *message);
    void    (*trap_BotRemoveConsoleMessage)(int chatstate, int handle);
    int     (*trap_BotNextConsoleMessage)(int chatstate, void *cm);
    int     (*trap_BotNumConsoleMessages)(int chatstate);
    void    (*trap_BotInitialChat)(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
    int     (*trap_BotNumInitialChats)(int chatstate, char *type);
    int     (*trap_BotReplyChat)(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
    int     (*trap_BotChatLength)(int chatstate);
    void    (*trap_BotEnterChat)(int chatstate, int client, int sendto);
    void    (*trap_BotGetChatMessage)(int chatstate, char *buf, int size);
    int     (*trap_StringContains)(char *str1, char *str2, int casesensitive);
    int     (*trap_BotFindMatch)(char *str, void *match, unsigned long context);
    void    (*trap_BotMatchVariable)(void *match, int variable, char *buf, int size);
    void    (*trap_UnifyWhiteSpaces)(char *string);
    void    (*trap_BotReplaceSynonyms)(char *string, unsigned long context);
    void    (*trap_BotSetChatGender)(int chatstate, int gender);
    void    (*trap_BotSetChatName)(int chatstate, char *name, int client);
    int     (*trap_BotLoadChatFile)(int chatstate, char *chatfile, char *chatname);

    void    (*trap_BotResetGoalState)(int goalstate);
    void    (*trap_BotResetAvoidGoals)(int goalstate);
    void    (*trap_BotRemoveFromAvoidGoals)(int goalstate, int number);
    void    (*trap_BotPushGoal)(int goalstate, void *goal);
    void    (*trap_BotPopGoal)(int goalstate);
    void    (*trap_BotEmptyGoalStack)(int goalstate);
    void    (*trap_BotDumpAvoidGoals)(int goalstate);
    void    (*trap_BotDumpGoalStack)(int goalstate);
    void    (*trap_BotGoalName)(int number, char *name, int size);
    int     (*trap_BotGetTopGoal)(int goalstate, void *goal);
    int     (*trap_BotGetSecondGoal)(int goalstate, void *goal);
    int     (*trap_BotChooseLTGItem)(int goalstate, vec3_t origin, int *inventory, int travelflags);
    int     (*trap_BotChooseNBGItem)(int goalstate, vec3_t origin, int *inventory, int travelflags, void *ltg, float maxtime);
    int     (*trap_BotTouchingGoal)(vec3_t origin, void *goal);
    int     (*trap_BotItemGoalInVisButNotVisible)(int viewer, vec3_t eye, vec3_t viewangles, void *goal);
    int     (*trap_BotGetLevelItemGoal)(int index, char *classname, void *goal);
    int     (*trap_BotGetNextCampSpotGoal)(int num, void *goal);
    int     (*trap_BotGetMapLocationGoal)(char *name, void *goal);
    float   (*trap_BotAvoidGoalTime)(int goalstate, int number);
    void    (*trap_BotSetAvoidGoalTime)(int goalstate, int number, float avoidtime);
    void    (*trap_BotInitLevelItems)(void);
    void    (*trap_BotUpdateEntityItems)(void);
    int     (*trap_BotLoadItemWeights)(int goalstate, char *filename);
    void    (*trap_BotFreeItemWeights)(int goalstate);
    void    (*trap_BotInterbreedGoalFuzzyLogic)(int parent1, int parent2, int child);
    void    (*trap_BotSaveGoalFuzzyLogic)(int goalstate, char *filename);
    void    (*trap_BotMutateGoalFuzzyLogic)(int goalstate, float range);
    int     (*trap_BotAllocGoalState)(int client);
    void    (*trap_BotFreeGoalState)(int handle);

    void    (*trap_BotResetMoveState)(int movestate);
    void    (*trap_BotMoveToGoal)(void *result, int movestate, void *goal, int travelflags);
    int     (*trap_BotMoveInDirection)(int movestate, vec3_t dir, float speed, int type);
    void    (*trap_BotResetAvoidReach)(int movestate);
    void    (*trap_BotResetLastAvoidReach)(int movestate);
    int     (*trap_BotReachabilityArea)(vec3_t origin, int testground);
    int     (*trap_BotMovementViewTarget)(int movestate, void *goal, int travelflags, float lookahead, vec3_t target);
    int     (*trap_BotPredictVisiblePosition)(vec3_t origin, int areanum, void *goal, int travelflags, vec3_t target);
    int     (*trap_BotAllocMoveState)(void);
    void    (*trap_BotFreeMoveState)(int handle);
    void    (*trap_BotInitMoveState)(int handle, void *initmove);
    void    (*trap_BotAddAvoidSpot)(int movestate, vec3_t origin, float radius, int type);

    int     (*trap_BotChooseBestFightWeapon)(int weaponstate, int *inventory);
    void    (*trap_BotGetWeaponInfo)(int weaponstate, int weapon, void *weaponinfo);
    int     (*trap_BotLoadWeaponWeights)(int weaponstate, char *filename);
    int     (*trap_BotAllocWeaponState)(void);
    void    (*trap_BotFreeWeaponState)(int weaponstate);
    void    (*trap_BotResetWeaponState)(int weaponstate);

    int     (*trap_GeneticParentsAndChildSelection)(int numranks, float *ranks, int *parent1, int *parent2, int *child);
    int     (*trap_BotGetServerCommand)(int clientNum, char *message, int size);
    int     (*trap_BotLibVarGet)(char *var_name, char *value, int size);
    int     (*trap_BotLibTest)(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
    int     (*trap_PC_LoadSource)(const char *filename);
    int     (*trap_PC_FreeSource)(int handle);
    int     (*trap_PC_ReadToken)(int handle, pc_token_t *pc_token);
    int     (*trap_PC_SourceFileAndLine)(int handle, char *filename, int *line);

    void    *_reserved[7];

     uint64_t(*trap_GetSteamID)(int clientNum);
     void    *_unused_c9;
     void    (*trap_GetServerSteamID)(char *buf, int size);
     void    (*trap_SetAchievement)(int clientNum, int achievement);
     qboolean(*trap_HasAchievement)(int clientNum, int achievement);
     qboolean(*trap_VerifySteamAuth)(int clientNum);
     qboolean(*trap_FactoryExists)(const char *id);

     void    (*trap_GenerateMatchGUID)(char *buf, int size);
     void    (*trap_StatsEvent)(uint64_t steamId, const char *type, const char *document);

     void    (*trap_IncrementSteamStat)(int clientNum, int stat, int amount);

} gameImport_t;

typedef enum {
    GAME_UNUSED_0                = 0,
    GAME_CVAR_CHANGED            = 1,
    GAME_SHUTDOWN                = 2,
    GAME_RUN_FRAME               = 3,
    GAME_REGISTER_CVARS          = 4,
    GAME_INIT                    = 5,
    GAME_CONSOLE_COMMAND         = 6,
    GAME_CLIENT_USERINFO_CHANGED = 7,
    GAME_CLIENT_THINK            = 8,
    GAME_CLIENT_DISCONNECT       = 9,
    GAME_CLIENT_CONNECT          = 10,
    GAME_CLIENT_COMMAND          = 11,
    GAME_CLIENT_BEGIN            = 12,
    GAME_BOT_AI_START_FRAME      = 13,
    GAME_SNAPSHOT_ALWAYS_SEND    = 14,
    GAME_SNAPSHOT_SKIP_ENTITY    = 15,
    GAME_ENTITY_ALWAYS_SEND      = 16,
    GAME_VOICE_FILTER            = 17,
    GAME_CLIENT_IS_ADMIN         = 18,
    GAME_SNAPSHOT_VISIBILITY     = 19,
    GAME_CLIENT_SCORE            = 20,

    GAME_EXPORT_COUNT
} gameExport_t;
