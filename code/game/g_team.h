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
#define CTF_CAPTURE_BONUS 100
#define CTF_TEAM_BONUS 25
#define CTF_RECOVERY_BONUS 10
#define CTF_FLAG_BONUS 10
#define CTF_FRAG_CARRIER_BONUS 20
#define CTF_FLAG_RETURN_TIME 40000

#define CTF_CARRIER_DANGER_PROTECT_BONUS 5
#define CTF_CARRIER_PROTECT_BONUS 2
#define CTF_FLAG_DEFENSE_BONUS 10
#define CTF_RETURN_FLAG_ASSIST_BONUS 10
#define CTF_FRAG_CARRIER_ASSIST_BONUS 10

#define CTF_TARGET_PROTECT_RADIUS 1000
#define CTF_ATTACKER_PROTECT_RADIUS 1000

#define CTF_CARRIER_DANGER_PROTECT_TIMEOUT 8000
#define CTF_FRAG_CARRIER_ASSIST_TIMEOUT 10000
#define CTF_RETURN_FLAG_ASSIST_TIMEOUT 10000

#define CTF_GRAPPLE_SPEED 750
#define CTF_GRAPPLE_PULL_SPEED 750

#define OVERLOAD_ATTACK_BASE_SOUND_TIME 20000

int OtherTeam(int team);
const char* TeamName(int team);
const char* TeamColorString(int team);
void AddTeamScore(vec3_t origin, int team, int score);

void Team_DroppedFlagThink(gentity_t* ent);
void Team_FragBonuses(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker);
void Team_CheckHurtCarrier(gentity_t* targ, gentity_t* attacker);
void Team_InitGame(void);
void Team_ReturnFlag(int team);
void Team_FreeEntity(gentity_t* ent);
void Team_CheckLostFlag(int team);
gentity_t* Team_GetLocation(gentity_t* ent);
qboolean Team_GetLocationMsg(gentity_t* ent, char* loc, int loclen);
void TeamOverlayMessage(gentity_t* ent);
void CheckTeamStatus(void);

int Pickup_Team(gentity_t* ent, gentity_t* other);
