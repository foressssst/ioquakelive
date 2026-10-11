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
#define CTF

#define MAX_ITEMS 256
#define BFL_STRAFERIGHT 1
#define BFL_ATTACKED 2
#define BFL_ATTACKJUMPED 4
#define BFL_AIMATENEMY 8
#define BFL_AVOIDRIGHT 16
#define BFL_IDEALVIEWSET 32
#define BFL_FIGHTSUICIDAL 64
#define LTG_TEAMHELP 1
#define LTG_TEAMACCOMPANY 2
#define LTG_DEFENDKEYAREA 3
#define LTG_GETFLAG 4
#define LTG_RUSHBASE 5
#define LTG_RETURNFLAG 6
#define LTG_CAMP 7
#define LTG_CAMPORDER 8
#define LTG_PATROL 9
#define LTG_GETITEM 10
#define LTG_KILL 11
#define LTG_HARVEST 12
#define LTG_ATTACKENEMYBASE 13
#define LTG_MAKELOVE_UNDER 14
#define LTG_MAKELOVE_ONTOP 15
#define LTG_FOLLOWME 16
#define LTG_TRAINING 18
#define LTG_DODGETRAINER 22
#define TEAM_HELP_TIME 60
#define TEAM_ACCOMPANY_TIME 600
#define TEAM_DEFENDKEYAREA_TIME 600
#define TEAM_CAMP_TIME 600
#define TEAM_PATROL_TIME 600
#define TEAM_LEAD_TIME 600
#define TEAM_GETITEM_TIME 60
#define TEAM_KILL_SOMEONE 180
#define TEAM_ATTACKENEMYBASE_TIME 600
#define TEAM_HARVEST_TIME 120
#define CTF_GETFLAG_TIME 600
#define CTF_RUSHBASE_TIME 120
#define CTF_RETURNFLAG_TIME 180
#define CTF_ROAM_TIME 60
#define PATROL_LOOP 1
#define PATROL_REVERSE 2
#define PATROL_BACK 4
#define TEAMTP_DEFENDER 1
#define TEAMTP_ATTACKER 2
#define CTFS_AGRESSIVE 1
#define PRESENCE_NONE 1
#define PRESENCE_NORMAL 2
#define PRESENCE_CROUCH 4
#define MAX_PROXMINES 64

typedef struct bot_waypoint_s {
    int inuse;
    char name[32];
    bot_goal_t goal;
    struct bot_waypoint_s *next, *prev;
} bot_waypoint_t;

#define MAX_ACTIVATESTACK 8
#define MAX_ACTIVATEAREAS 32

typedef struct bot_activategoal_s {
    int inuse;
    bot_goal_t goal;
    float time;
    float start_time;
    float justused_time;
    int shoot;
    int weapon;
    vec3_t target;
    vec3_t origin;
    int areas[MAX_ACTIVATEAREAS];
    int numareas;
    int areasdisabled;
    struct bot_activategoal_s* next;
} bot_activategoal_t;

typedef struct bot_state_s {
    int inuse;
    int botthink_residual;
    int client;
    int entitynum;
    playerState_t cur_ps;
    int last_eFlags;
    usercmd_t lastucmd;
    int entityeventTime[MAX_GENTITIES];
    bot_settings_t settings;
    int (*ainode)(struct bot_state_s* bs);
    float thinktime;
    vec3_t origin;
    vec3_t velocity;
    int presencetype;
    vec3_t eye;
    int areanum;
    int inventory[MAX_ITEMS];
    int tfl;
    int flags;
    int respawn_wait;
    int lasthealth;
    int lastkilledplayer;
    int lastkilledby;
    int botdeathtype;
    int enemydeathtype;
    int botsuicide;
    int enemysuicide;
    int setupcount;
    int map_restart;
    int entergamechat;
    int num_deaths;
    int num_kills;
    int revenge_enemy;
    int revenge_kills;
    int lastframe_health;
    int lasthitcount;
    int chatto;
    float walker;
    float ltime;
    float entergame_time;
    float ltg_time;
    float nbg_time;
    float respawn_time;
    float respawnchat_time;
    float chase_time;
    float enemyvisible_time;
    float check_time;
    float stand_time;
    float lastchat_time;
    float kamikaze_time;
    float invulnerability_time;
    float standfindenemy_time;
    float attackstrafe_time;
    float attackcrouch_time;
    float attackchase_time;
    float attackjump_time;
    float enemysight_time;
    float enemydeath_time;
    float enemyposition_time;
    float defendaway_time;
    float defendaway_range;
    float rushbaseaway_time;
    float attackaway_time;
    float harvestaway_time;
    float ctfroam_time;
    float killedenemy_time;
    float arrive_time;
    float lastair_time;
    float teleport_time;
    float camp_time;
    float weaponchange_time;
    float firethrottlewait_time;
    float firethrottleshoot_time;
    float notblocked_time;
    float blockedbyavoidspot_time;
    float predictobstacles_time;
    int predictobstacles_goalareanum;
    vec3_t aimtarget;
    vec3_t enemyvelocity;
    vec3_t enemyorigin;
    int kamikazebody;
    int proxmines[MAX_PROXMINES];
    int numproxmines;
    int character;
    int ms;
    int gs;
    int cs;
    int ws;
    int enemy;
    int lastenemyareanum;
    vec3_t lastenemyorigin;
    int weaponnum;
    vec3_t viewangles;
    vec3_t ideal_viewangles;
    vec3_t viewanglespeed;
    int ltgtype;
    int teammate;
    int decisionmaker;
    int ordered;
    float order_time;
    int owndecision_time;
    bot_goal_t teamgoal;
    bot_goal_t altroutegoal;
    float reachedaltroutegoal_time;
    float teammessage_time;
    float teamgoal_time;
    float teammatevisible_time;
    int teamtaskpreference;
    int lastgoal_decisionmaker;
    int lastgoal_ltgtype;
    int lastgoal_teammate;
    bot_goal_t lastgoal_teamgoal;
    int lead_teammate;
    bot_goal_t lead_teamgoal;
    float lead_time;
    float leadvisible_time;
    float leadmessage_time;
    float leadbackup_time;
    char teamleader[MAX_NETNAME];
    float askteamleader_time;
    float becometeamleader_time;
    float teamgiveorders_time;
    float lastflagcapture_time;
    int numteammates;
    int redflagstatus;
    int blueflagstatus;
    int neutralflagstatus;
    int flagstatuschanged;
    int forceorders;
    int flagcarrier;
    int ctfstrategy;
    char subteam[32];
    float formation_dist;

    bot_activategoal_t* activatestack;
    bot_activategoal_t activategoalheap[MAX_ACTIVATESTACK];

    bot_waypoint_t* checkpoints;
    bot_waypoint_t* patrolpoints;
    bot_waypoint_t* curpatrolpoint;
    int patrolflags;

    float tourFollowDist;
    int tourPoint;
    int tourPrevPoint;
    int tourAnnounced;
    int tourStopReached;
    int tourNextTime;
    vec3_t tourGoal;
    vec3_t tourPrevGoal;
    vec3_t tourLookTarget;
    float tourLookUntil;
    float tourLastNudge;
    float tourReachTime;
    float tourNextSay;
    int heardClients;
    int enemyHeard;
    char ainodename[80];
    float skillStep;
    int skillBestDiff;
    int skillLastDiff;
    int skillDiffTime;
    struct {
        int diff;
        float skill;
        int time;
    } skillLog[32];
    int numSkillLog;
    int tourSaidCareful;
    int tourPlayerNear;
    int tourWait;
    int tourWaitForPlayer;
    int tourLostPlayer;
    int tourReturning;
    bot_goal_t tourSavedGoal;
    vec3_t tourSavedLook;
    int tourSavedLookSet;
    int tourGoBack;
    int nodeEnterTime;
    vec3_t dodgeAnchor;
    int dodgeDir;
    float dodgeRunStart;
    float dodgeTurnTime;
    float dodgeHitTime;
    float dodgeJumpTime;
    int dodgeMode;
    float dodgeModeEnd;
    float dodgeRadial;
    float dodgeHeat;
    float dodgeThink;
    float dodgeJumped;
} bot_state_t;

void BotResetState(bot_state_t* bs);
int NumBots(void);
void BotEntityInfo(int entnum, aas_entityinfo_t* info);

extern float floattime;
#define FloatTime() floattime

void QDECL BotAI_Print(int type, char* fmt, ...) __attribute__((format(printf, 2, 3)));
void QDECL QDECL BotAI_BotInitialChat(bot_state_t* bs, char* type, ...);
void BotAI_Trace(bsp_trace_t* bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
int BotAI_GetClientState(int clientNum, playerState_t* state);
int BotAI_GetEntityState(int entityNum, entityState_t* state);
int BotAI_GetSnapshotEntity(int clientNum, int sequence, entityState_t* state);
int BotTeamLeader(bot_state_t* bs);
