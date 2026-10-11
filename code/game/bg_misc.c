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

const char *gametypeShortNames[] = {
	"ffa",
	"duel",
	"race",
	"tdm",
	"ca",
	"ctf",
	"1f",
	"ob",
	"har",
	"ft",
	"dom",
	"ad",
	"rr"
};

const char *gametypeDisplayNames[] = {
	"Free For All",
	"Duel",
	"Race",
	"Team Deathmatch",
	"Clan Arena",
	"Capture the Flag",
	"One Flag CTF",
	"Overload",
	"Harvester",
	"Freeze Tag",
	"Domination",
	"Attack and Defend",
	"Red Rover"
};

int bg_weaponReloadTime[WP_NUM_WEAPONS] = {
    0,
    400,
    100,
    1000,
    800,
    800,
    50,
    1500,
    100,
    300,
    100,
    1000,
    800,
    50,
    75,
};

void BG_SetWeaponReload(int weapon, int reloadTime) {
    if (weapon >= 0 && weapon < WP_NUM_WEAPONS) {
        bg_weaponReloadTime[weapon] = reloadTime;
    }
}

gitem_t bg_itemlist[] =
    {
        {NULL,
         NULL,
         {NULL,
          NULL,
          NULL, NULL},
          NULL,
          NULL,
         0,
         0,
         0,
          "",
          ""},

        {
            "item_armor_shard",
            "sound/misc/ar1_pkup.wav",
            {"models/powerups/armor/shard.md3",
             "models/powerups/armor/shard_sphere.md3",
             NULL, NULL},
             "icons/iconr_shard",
             "Armor Shard",
            5,
            IT_ARMOR,
            4,
             "",
             ""},

        {
            "item_armor_combat",
            "sound/misc/ar2_pkup.wav",
            {"models/powerups/armor/armor_yel.md3",
             NULL, NULL, NULL},
             "icons/iconr_yellow",
             "Yellow Armor",
            50,
            IT_ARMOR,
            2,
             "",
             "",
             1},

        {
            "item_armor_body",
            "sound/misc/ar2_pkup.wav",
            {"models/powerups/armor/armor_red.md3",
             NULL, NULL, NULL},
             "icons/iconr_red",
             "Red Armor",
            100,
            IT_ARMOR,
            1,
             "",
             "",
             1},

        {
            "item_armor_jacket",
            "sound/misc/ar2_pkup.wav",
            {"models/powerups/armor/armor_grn.md3",
             NULL, NULL, NULL},
             "icons/iconr_green",
             "Green Armor",
            25,
            IT_ARMOR,
            3,
             "",
             "",
             1},

        {
            "item_health_small",
            "sound/items/s_health.wav",
            {"models/powerups/health/small_cross.md3",
             "models/powerups/health/small_sphere.md3",
             NULL, NULL},
             "icons/iconh_green",
             "5 Health",
            5,
            IT_HEALTH,
            4,
             "",
             ""},

        {
            "item_health",
            "sound/items/n_health.wav",
            {"models/powerups/health/medium_cross.md3",
             "models/powerups/health/medium_sphere.md3",
             NULL, NULL},
             "icons/iconh_yellow",
             "25 Health",
            25,
            IT_HEALTH,
            3,
             "",
             ""},

        {
            "item_health_large",
            "sound/items/l_health.wav",
            {"models/powerups/health/large_cross.md3",
             "models/powerups/health/large_sphere.md3",
             NULL, NULL},
             "icons/iconh_red",
             "50 Health",
            50,
            IT_HEALTH,
            2,
             "",
             ""},

        {
            "item_health_mega",
            "sound/items/m_health.wav",
            {"models/powerups/health/mega_cross.md3",
             "models/powerups/health/mega_sphere.md3",
             NULL, NULL},
             "icons/iconh_mega",
             "Mega Health",
            100,
            IT_HEALTH,
            1,
             "",
             "",
             1},

        {
            "weapon_gauntlet",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/gauntlet/gauntlet.md3",
             NULL, NULL, NULL},
             "icons/iconw_gauntlet",
             "Gauntlet",
            0,
            IT_WEAPON,
            WP_GAUNTLET,
             "",
             ""},

        {
            "weapon_shotgun",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/shotgun/shotgun.md3",
             NULL, NULL, NULL},
             "icons/iconw_shotgun",
             "Shotgun",
            10,
            IT_WEAPON,
            WP_SHOTGUN,
             "",
             ""},

        {
            "weapon_machinegun",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/machinegun/machinegun.md3",
             NULL, NULL, NULL},
             "icons/iconw_machinegun",
             "Machine Gun",
            100,
            IT_WEAPON,
            WP_MACHINEGUN,
             "",
             ""},

        {
            "weapon_grenadelauncher",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/grenadel/grenadel.md3",
             NULL, NULL, NULL},
             "icons/iconw_grenade",
             "Grenade Launcher",
            10,
            IT_WEAPON,
            WP_GRENADE_LAUNCHER,
             "",
             "sound/weapons/grenade/hgrenb1a.wav sound/weapons/grenade/hgrenb2a.wav"},

        {
            "weapon_rocketlauncher",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/rocketl/rocketl.md3",
             NULL, NULL, NULL},
             "icons/iconw_rocket",
             "Rocket Launcher",
            10,
            IT_WEAPON,
            WP_ROCKET_LAUNCHER,
             "",
             ""},

        {
            "weapon_lightning",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/lightning/lightning.md3",
             NULL, NULL, NULL},
             "icons/iconw_lightning",
             "Lightning Gun",
            100,
            IT_WEAPON,
            WP_LIGHTNING,
             "",
             ""},

        {
            "weapon_railgun",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/railgun/railgun.md3",
             NULL, NULL, NULL},
             "icons/iconw_railgun",
             "Railgun",
            10,
            IT_WEAPON,
            WP_RAILGUN,
             "",
             ""},

        {
            "weapon_plasmagun",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/plasma/plasma.md3",
             NULL, NULL, NULL},
             "icons/iconw_plasma",
             "Plasma Gun",
            50,
            IT_WEAPON,
            WP_PLASMAGUN,
             "",
             ""},

        {
            "weapon_bfg",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/bfg/bfg.md3",
             NULL, NULL, NULL},
             "icons/iconw_bfg",
             "BFG10K",
            10,
            IT_WEAPON,
            WP_BFG,
             "",
             ""},

        {
            "weapon_grapplinghook",
            "sound/misc/w_pkup.wav",
            {"models/weapons2/grapple/grapple.md3",
             NULL, NULL, NULL},
             "icons/iconw_grapple",
             "Grappling Hook",
            0,
            IT_WEAPON,
            WP_GRAPPLING_HOOK,
             "",
             ""},

        {
            "ammo_shells",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/shotgunam.md3",
             NULL, NULL, NULL},
             "icons/icona_shotgun",
             "Shells",
            5,
            IT_AMMO,
            WP_SHOTGUN,
             "",
             ""},

        {
            "ammo_bullets",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/machinegunam.md3",
             NULL, NULL, NULL},
             "icons/icona_machinegun",
             "Bullets",
            50,
            IT_AMMO,
            WP_MACHINEGUN,
             "",
             ""},

        {
            "ammo_grenades",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/grenadeam.md3",
             NULL, NULL, NULL},
             "icons/icona_grenade",
             "Grenades",
            5,
            IT_AMMO,
            WP_GRENADE_LAUNCHER,
             "",
             ""},

        {
            "ammo_cells",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/plasmaam.md3",
             NULL, NULL, NULL},
             "icons/icona_plasma",
             "Cells",
            50,
            IT_AMMO,
            WP_PLASMAGUN,
             "",
             ""},

        {
            "ammo_lightning",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/lightningam.md3",
             NULL, NULL, NULL},
             "icons/icona_lightning",
             "Lightning",
            50,
            IT_AMMO,
            WP_LIGHTNING,
             "",
             ""},

        {
            "ammo_rockets",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/rocketam.md3",
             NULL, NULL, NULL},
             "icons/icona_rocket",
             "Rockets",
            5,
            IT_AMMO,
            WP_ROCKET_LAUNCHER,
             "",
             ""},

        {
            "ammo_slugs",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/railgunam.md3",
             NULL, NULL, NULL},
             "icons/icona_railgun",
             "Slugs",
            5,
            IT_AMMO,
            WP_RAILGUN,
             "",
             ""},

        {
            "ammo_bfg",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/bfgam.md3",
             NULL, NULL, NULL},
             "icons/icona_bfg",
             "BFG Ammo",
            5,
            IT_AMMO,
            WP_BFG,
             "",
             ""},

        {
            "holdable_teleporter",
            "sound/items/holdable.wav",
            {"models/powerups/holdable/teleporter.md3",
             NULL, NULL, NULL},
             "icons/teleporter",
             "Personal Teleporter",
            60,
            IT_HOLDABLE,
            HI_TELEPORTER,
             "",
             ""},
        {
            "holdable_medkit",
            "sound/items/holdable.wav",
            {"models/powerups/holdable/medkit.md3",
             "models/powerups/holdable/medkit_sphere.md3",
             NULL, NULL},
             "icons/medkit",
             "Medkit",
            60,
            IT_HOLDABLE,
            HI_MEDKIT,
             "",
             "sound/items/use_medkit.wav",
             1},

        {
            "item_quad",
            "sound/items/damage3.ogg",
            {"models/powerups/instant/quad.md3",
             "models/powerups/instant/quad_ring.md3",
             NULL, NULL},
             "icons/quad",
             "Quad Damage",
            30,
            IT_POWERUP,
            PW_QUAD,
             "",
             "sound/items/damage3.ogg",
             1},

        {
            "item_enviro",
            "sound/items/protect.wav",
            {"models/powerups/instant/enviro.md3",
             "models/powerups/instant/enviro_ring.md3",
             NULL, NULL},
             "icons/envirosuit",
             "Battle Suit",
            30,
            IT_POWERUP,
            PW_BATTLESUIT,
             "",
             "sound/items/protect3.ogg",
             1},

        {
            "item_haste",
            "sound/items/guard.wav",
            {"models/powerups/instant/haste.md3",
             "models/powerups/instant/haste_ring.md3",
             NULL, NULL},
             "icons/haste",
             "Haste",
            30,
            IT_POWERUP,
            PW_HASTE,
             "",
             "",
             1},

        {
            "item_invis",
            "sound/items/holdable.wav",
            {"models/powerups/instant/invis.md3",
             "models/powerups/instant/invis_ring.md3",
             NULL, NULL},
             "icons/invis",
             "Invisibility",
            30,
            IT_POWERUP,
            PW_INVIS,
             "",
             "",
             1},

        {
            "item_regen",
            "sound/items/holdable.wav",
            {"models/powerups/instant/regen.md3",
             "models/powerups/instant/regen_ring.md3",
             NULL, NULL},
             "icons/regen",
             "Regeneration",
            30,
            IT_POWERUP,
            PW_REGEN,
             "",
             "sound/items/regen.wav",
             1},

        {
            "item_flight",
            "sound/items/flight.wav",
            {"models/powerups/instant/flight.md3",
             "models/powerups/instant/flight_ring.md3",
             NULL, NULL},
             "icons/flight",
             "Flight",
            60,
            IT_HOLDABLE,
            HI_FLIGHT,
             "",
             "sound/items/flight.wav"},

        {
            "team_CTF_redflag",
            NULL,
            {"models/flags/r_flag.md3",
             "models/flag3/r_flag3.md3",
             NULL, NULL},
             "gfx/2d/flag_status/red_flag_at_base",
             "Red Flag",
            0,
            IT_TEAM,
            PW_REDFLAG,
             "",
             "",
             0,
             (1 << GT_AD)},

        {
            "team_CTF_blueflag",
            NULL,
            {"models/flags/b_flag.md3",
             "models/flag3/b_flag3.md3",
             NULL, NULL},
             "gfx/2d/flag_status/blue_flag_at_base",
             "Blue Flag",
            0,
            IT_TEAM,
            PW_BLUEFLAG,
             "",
             "",
             0,
             (1 << GT_AD)},

        {
            "holdable_kamikaze",
            "sound/items/holdable.wav",
            {"models/powerups/kamikazi.md3",
             NULL, NULL, NULL},
             "icons/kamikaze",
             "Kamikaze",
            60,
            IT_HOLDABLE,
            HI_KAMIKAZE,
             "",
             "sound/items/kamikazerespawn.wav"},

        {
            "holdable_portal",
            "sound/items/holdable.wav",
            {"models/powerups/holdable/porter.md3",
             NULL, NULL, NULL},
             "icons/portal",
             "Portal",
            60,
            IT_HOLDABLE,
            HI_PORTAL,
             "",
             ""},

        {
            "holdable_invulnerability",
            "sound/items/holdable.wav",
            {"models/powerups/holdable/invulnerability.md3",
             NULL, NULL, NULL},
             "icons/invulnerability",
             "Invulnerability",
            60,
            IT_HOLDABLE,
            HI_INVULNERABILITY,
             "",
             ""},

        {
            "ammo_nails",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/nailgunam.md3",
             NULL, NULL, NULL},
             "icons/icona_nailgun",
             "Nails",
            5,
            IT_AMMO,
            WP_NAILGUN,
             "",
             ""},

        {
            "ammo_mines",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/proxmineam.md3",
             NULL, NULL, NULL},
             "icons/icona_proxlauncher",
             "Proximity Mines",
            5,
            IT_AMMO,
            WP_PROX_LAUNCHER,
             "",
             ""},

        {
            "ammo_belt",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/chaingunam.md3",
             NULL, NULL, NULL},
             "icons/icona_chaingun",
             "Chaingun Belt",
            100,
            IT_AMMO,
            WP_CHAINGUN,
             "",
             ""},

        {
            "item_scout",
            "sound/items/scout.ogg",
            {"models/powerups/scout.md3",
             NULL, NULL, NULL},
             "icons/scout",
             "Scout",
            0,
            IT_PERSISTANT_POWERUP,
            1,
             "",
             ""},

        {
            "item_guard",
            "sound/items/guard.ogg",
            {"models/powerups/guard.md3",
             NULL, NULL, NULL},
             "icons/guard",
             "Guard",
            0,
            IT_PERSISTANT_POWERUP,
            2,
             "",
             ""},

        {
            "item_doubler",
            "sound/items/damage.ogg",
            {"models/powerups/doubler.md3",
             NULL, NULL, NULL},
             "icons/doubler",
             "Damage",
            0,
            IT_PERSISTANT_POWERUP,
            3,
             "",
             ""},

        {
            "item_armorregen",
            "sound/items/armorregen.ogg",
            {"models/powerups/ammo.md3",
             NULL, NULL, NULL},
             "icons/armor_regen",
             "Armor Regen",
            0,
            IT_PERSISTANT_POWERUP,
            4,
             "",
             ""},

        {
            "team_CTF_neutralflag",
            NULL,
            {"models/flags/n_flag.md3",
             "models/flag3/n_flag3.md3",
             NULL, NULL},
             "gfx/2d/flag_status/flag_at_base",
             "Neutral Flag",
            0,
            IT_TEAM,
            PW_NEUTRALFLAG,
             "",
             ""},

        {"item_redcube",
         "sound/misc/am_pkup.wav",
         {"models/powerups/orb/r_orb.md3",
          NULL, NULL, NULL},
          "icons/iconh_rorb",
          "Red Skull",
         0,
         IT_TEAM,
         0,
          "",
          ""},

        {"item_bluecube",
         "sound/misc/am_pkup.wav",
         {"models/powerups/orb/b_orb.md3",
          NULL, NULL, NULL},
          "icons/iconh_borb",
          "Blue Skull",
         0,
         IT_TEAM,
         0,
          "",
          ""},
        {
            "weapon_nailgun",
            "sound/misc/w_pkup.wav",
            {"models/weapons/nailgun/nailgun.md3",
             NULL, NULL, NULL},
             "icons/iconw_nailgun",
             "Nailgun",
            10,
            IT_WEAPON,
            WP_NAILGUN,
             "",
             ""},

        {
            "weapon_prox_launcher",
            "sound/misc/w_pkup.wav",
            {"models/weapons/proxmine/proxmine.md3",
             NULL, NULL, NULL},
             "icons/iconw_proxlauncher",
             "Prox Launcher",
            5,
            IT_WEAPON,
            WP_PROX_LAUNCHER,
             "",
             "sound/weapons/proxmine/wstbtick.wav "
                         "sound/weapons/proxmine/wstbactv.wav "
                         "sound/weapons/proxmine/wstbimpl.wav "
                         "sound/weapons/proxmine/wstbimpm.wav "
                         "sound/weapons/proxmine/wstbimpd.wav "
                         "sound/weapons/proxmine/wstbactv.wav"},

        {
            "weapon_chaingun",
            "sound/misc/w_pkup.wav",
            {"models/weapons/vulcan/vulcan.md3",
             NULL, NULL, NULL},
             "icons/iconw_chaingun",
             "Chaingun",
            100,
            IT_WEAPON,
            WP_CHAINGUN,
             "",
             "sound/weapons/vulcan/wvulwind.wav"},

        {
            "item_spawnarmor",
            NULL,
            {NULL, NULL, NULL, NULL},
             "icons/spawnarmor",
             "Spawn Armor",
            0,
            IT_POWERUP,
            0,
             "",
             ""},

        {
            "weapon_hmg",
            "sound/misc/w_pkup.wav",
            {"models/weapons3/hmg/hmg.md3",
             NULL, NULL, NULL},
             "icons/weap_hmg",
             "Heavy Machinegun",
            100,
            IT_WEAPON,
            WP_HMG,
             "",
             ""},

        {
            "ammo_hmg",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/hmgam.md3",
             NULL, NULL, NULL},
             "icons/ammo_hmg",
             "Heavy Bullets",
            50,
            IT_AMMO,
            WP_HMG,
             "",
             ""},

        {
            "ammo_pack",
            "sound/misc/am_pkup.wav",
            {"models/powerups/ammo/ammopack.md3",
             NULL, NULL, NULL},
             "icons/ammo_pack",
             "Ammo Pack",
            1,
            IT_AMMO,
            WP_NUM_WEAPONS,
             "",
             ""},

        {
            "item_key_silver",
            "sound/items/key_silver.wav",
            {"models/powerups/keys/key_silver.md3",
             NULL, NULL, NULL},
             "icons/key_silver",
             "Silver Key",
            1,
            IT_KEY,
            KEY_SILVER,
             "",
             ""},

        {
            "item_key_gold",
            "sound/items/key_gold.wav",
            {"models/powerups/keys/key_gold.md3",
             NULL, NULL, NULL},
             "icons/key_gold",
             "Gold Key",
            1,
            IT_KEY,
            KEY_GOLD,
             "",
             ""},

        {
            "item_key_master",
            "sound/items/key_gold.wav",
            {"models/powerups/keys/key_master.md3",
             NULL, NULL, NULL},
             "icons/key_master",
             "Master Key",
            1,
            IT_KEY,
            KEY_MASTER,
             "",
             ""},

        {NULL}};

int bg_numItems = ARRAY_LEN(bg_itemlist) - 1;

gitem_t* BG_FindItemForPowerup(powerup_t pw) {
    int i;

    if (pw >= PW_SCOUT && pw <= PW_AMMOREGEN) {
        for (i = 1; i < bg_numItems; i++) {
            if (bg_itemlist[i].giType == IT_PERSISTANT_POWERUP && bg_itemlist[i].giTag == pw - PW_INVULNERABILITY) {
                return &bg_itemlist[i];
            }
        }
        Com_Error(ERR_DROP, "BG_FindItemForPowerup: couldn't find TA rune %i", pw - PW_INVULNERABILITY);
        return NULL;
    }

    if (pw == PW_INVULNERABILITY) {
        for (i = 1; i < bg_numItems; i++) {
            if (bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == HI_INVULNERABILITY) {
                return &bg_itemlist[i];
            }
        }
        pw = (powerup_t)HI_INVULNERABILITY;
    }

    for (i = 0; i < bg_numItems; i++) {
        if (bg_itemlist[i].giType == IT_POWERUP && bg_itemlist[i].giTag == pw) {
            return &bg_itemlist[i];
        }
    }
    for (i = 0; i < bg_numItems; i++) {
        if (bg_itemlist[i].giType == IT_TEAM && bg_itemlist[i].giTag == pw) {
            return &bg_itemlist[i];
        }
    }

    Com_Error(ERR_DROP, "BG_FindItemForPowerup: couldn't find item for powerup %i", pw);
    return NULL;
}

gitem_t* BG_FindItemForHoldable(holdable_t pw) {
    int i;

    for (i = 0; i < bg_numItems; i++) {
        if (bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == pw) {
            return &bg_itemlist[i];
        }
    }

    Com_Error(ERR_DROP, "HoldableItem not found");

    return NULL;
}

qboolean BG_WeaponInLoadout(int weapon) {
    switch (weapon) {
        case WP_SHOTGUN:
        case WP_ROCKET_LAUNCHER:
        case WP_LIGHTNING:
        case WP_RAILGUN:
        case WP_PLASMAGUN:
        case WP_HMG:
            return qtrue;
        default:
            return qfalse;
    }
}

gitem_t* BG_FindItemForWeapon(weapon_t weapon) {
    gitem_t* it;

    for (it = bg_itemlist + 1; it->classname; it++) {
        if (it->giType == IT_WEAPON && it->giTag == weapon) {
            return it;
        }
    }

    Com_Error(ERR_DROP, "Couldn't find item for weapon %i", weapon);
    return NULL;
}

gitem_t* BG_FindItem(const char* pickupName) {
    gitem_t* it;

    for (it = bg_itemlist + 1; it->classname; it++) {
        if (!Q_stricmp(it->pickup_name, pickupName))
            return it;
    }

    return NULL;
}

static const char* bg_weaponFlagNames[] = {
    "g",
    "mg",
    "sg",
    "gl",
    "rl",
    "lg",
    "rg",
    "pg",
    "bfg",
    "gh",
    "ng",
    "pl",
    "cg",
    "hmg"
};

int BG_ParseWeaponStringtoFlag(char* string) {
    const char* token;
    int mask = 0;
    int count;
    int i;

    for (count = 0; count < 16; count++) {
        token = COM_ParseExt(&string, qtrue);
        if (!token[0]) {
            break;
        }
        for (i = 0; i < (int)ARRAY_LEN(bg_weaponFlagNames); i++) {
            if (bg_weaponFlagNames[i] && !Q_stricmp(token, bg_weaponFlagNames[i])) {
                mask |= 1 << (i + 1);
            }
        }
    }

    return mask;
}

qboolean BG_PlayerTouchesItem(playerState_t* ps, entityState_t* item, int atTime, int itemHeight) {
    const gitem_t* it;
    vec3_t origin;

    BG_EvaluateTrajectory(&item->pos, atTime, origin);

    it = &bg_itemlist[item->modelindex];
    if (it->giType == IT_TEAM && it->giTag >= PW_REDFLAG && it->giTag <= PW_NEUTRALFLAG) {
        itemHeight = 35;
    }

    if (ps->origin[0] - origin[0] > 36 || ps->origin[0] - origin[0] < -36 || ps->origin[1] - origin[1] > 36 || ps->origin[1] - origin[1] < -36 || ps->origin[2] - origin[2] > itemHeight + 29 || ps->origin[2] - origin[2] < -50) {
        return qfalse;
    }

    return qtrue;
}

qboolean BG_IsRoundBasedGameType(int gametype) {
    switch (gametype) {
    case GT_CA:
    case GT_FREEZE:
    case GT_AD:
    case GT_RR:
        return qtrue;
    default:
        return qfalse;
    }
}

const int maxAmmoStandard[WP_NUM_WEAPONS] = {
    0,
    -1,
    150,
    25,
    25,
    25,
    150,
    25,
    150,
    25,
    -1,
    25,
    5,
    200,
    150,
};

static qboolean BG_CanWeaponBeGrabbed(const entityState_t* ent, const playerState_t* ps,
                                      int weaponRespawn) {
    gitem_t* item = &bg_itemlist[ent->modelindex];

    if (ps->pm_flags & PMF_LOADOUT_FORCED) {
        return qfalse;
    }
    if (weaponRespawn != 0 ||
        !(ps->stats[STAT_WEAPONS] & (1 << item->giTag)) ||
        ps->ammo[item->giTag] == 0 ||
        ent->modelindex2 != 0) {
        return qtrue;
    }
    return qfalse;
}

qboolean BG_CanItemBeGrabbed(int atTime, int warmupTime, int armorTiered, int weaponRespawn,
                             int gametype, const entityState_t* ent, const playerState_t* ps) {
    gitem_t* item;
    int i;

    if (ent->modelindex < 1 || ent->modelindex >= bg_numItems) {
        Com_Error(ERR_DROP, "BG_CanItemBeGrabbed: index out of range");
    }

    if (ent->modelindex2 != 0 &&
        atTime < ent->time + 1000 &&
        ps->clientNum == ent->clientNum) {
        return qfalse;
    }

    item = &bg_itemlist[ent->modelindex];

    switch (item->giType) {
        case IT_WEAPON:
            return BG_CanWeaponBeGrabbed(ent, ps, weaponRespawn);

        case IT_AMMO:
            if (gametype != GT_DOMINATION) {
                if (item->giTag != WP_NUM_WEAPONS) {
                    return (qboolean)(ps->ammo[item->giTag] < maxAmmoStandard[item->giTag]);
                }
                for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
                    if ((ps->stats[STAT_WEAPONS] & (1 << i)) &&
                        maxAmmoStandard[i] >= 1 &&
                        ps->ammo[i] < maxAmmoStandard[i]) {
                        return qtrue;
                    }
                }
                return qfalse;
            }
            break;

        case IT_ARMOR:
            if (armorTiered == 0) {
                return (qboolean)(ps->stats[STAT_ARMOR] < ps->stats[STAT_MAX_HEALTH] * 2);
            }
            if (item->quantity == 100) {
                return (qboolean)(ps->stats[STAT_ARMOR] < 200);
            }
            if (item->quantity == 50) {
                if (ps->stats[STAT_ARMORTYPE] < 2) {
                    return (qboolean)(ps->stats[STAT_ARMOR] < 150);
                }
                if ((double)ps->stats[STAT_ARMOR] * 0.75 > 99.0) {
                    return qfalse;
                }
            } else if (item->quantity == 25) {
                if (ps->stats[STAT_ARMORTYPE] == 0) {
                    return (qboolean)(ps->stats[STAT_ARMOR] < 100);
                }
                if (ps->stats[STAT_ARMORTYPE] == 1) {
                    if ((double)ps->stats[STAT_ARMOR] * 0.66 > 50.0) {
                        return qfalse;
                    }
                } else {
                    if ((double)ps->stats[STAT_ARMOR] * 0.75 > 50.0) {
                        return qfalse;
                    }
                }
            } else {
                return (qboolean)(ps->stats[STAT_ARMOR] < ps->stats[STAT_MAX_HEALTH] * 2);
            }
            return qtrue;

        case IT_HEALTH:
            if (item->quantity != 5 && item->quantity != 100) {
                return (qboolean)(ps->stats[STAT_HEALTH] < ps->stats[STAT_MAX_HEALTH]);
            }
            return (qboolean)(ps->stats[STAT_HEALTH] < ps->stats[STAT_MAX_HEALTH] * 2);

        case IT_POWERUP:
        case IT_KEY:
            break;

        case IT_HOLDABLE:
            return (qboolean)(ps->stats[STAT_HOLDABLE_ITEM] == 0);

        case IT_PERSISTANT_POWERUP:
            if (ps->stats[STAT_PERSISTANT_POWERUP]) {
                return qfalse;
            }
            if ((ent->generic1 & 2) && ps->persistant[PERS_TEAM] != TEAM_RED) {
                return qfalse;
            }
            if ((ent->generic1 & 4) && ps->persistant[PERS_TEAM] != TEAM_BLUE) {
                return qfalse;
            }
            break;

        case IT_TEAM:
            if (gametype == GT_AD) {
                if (warmupTime != 0) {
                    return qfalse;
                }
                if (ps->persistant[PERS_TEAM] == TEAM_RED) {
                    if (item->giTag == PW_BLUEFLAG) {
                        return qtrue;
                    }
                    if (item->giTag != PW_REDFLAG) {
                        return qfalse;
                    }
                    return (qboolean)(ps->powerups[PW_BLUEFLAG] != 0);
                }
                if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
                    if (item->giTag == PW_REDFLAG) {
                        return qtrue;
                    }
                    if (item->giTag != PW_BLUEFLAG) {
                        return qfalse;
                    }
                    return (qboolean)(ps->powerups[PW_REDFLAG] != 0);
                }
                return qfalse;
            }
            if (gametype == GT_CTF) {
                if (ps->persistant[PERS_TEAM] == TEAM_RED) {
                    if (item->giTag == PW_BLUEFLAG) {
                        return qtrue;
                    }
                    if (item->giTag != PW_REDFLAG) {
                        return qfalse;
                    }
                    if (ent->modelindex2) {
                        return qtrue;
                    }
                    return (qboolean)(ps->powerups[PW_BLUEFLAG] != 0);
                }
                if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
                    if (item->giTag == PW_REDFLAG) {
                        return qtrue;
                    }
                    if (item->giTag != PW_BLUEFLAG) {
                        return qfalse;
                    }
                    if (ent->modelindex2) {
                        return qtrue;
                    }
                    return (qboolean)(ps->powerups[PW_REDFLAG] != 0);
                }
                return qfalse;
            }
            if (gametype == GT_1FCTF) {
                if (item->giTag == PW_NEUTRALFLAG) {
                    return qtrue;
                }
                if (ps->persistant[PERS_TEAM] == TEAM_RED) {
                    if (item->giTag != PW_BLUEFLAG) {
                        return qfalse;
                    }
                    return (qboolean)(ps->powerups[PW_NEUTRALFLAG] != 0);
                }
                if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
                    if (item->giTag != PW_REDFLAG) {
                        return qfalse;
                    }
                    return (qboolean)(ps->powerups[PW_NEUTRALFLAG] != 0);
                }
                return qfalse;
            }
            if (gametype == GT_HARVESTER) {
                return qtrue;
            }
            return qfalse;

        default:
            Com_Error(ERR_DROP, "BG_CanItemBeGrabbed: invalid item type %d", item->giType);
            return qfalse;
    }

    return qtrue;
}

void BG_EvaluateTrajectory(const trajectory_t* tr, int atTime, vec3_t result) {
    float deltaTime;
    float phase;

    switch (tr->trType) {
        case TR_STATIONARY:
        case TR_INTERPOLATE:
            VectorCopy(tr->trBase, result);
            break;
        case TR_LINEAR:
            deltaTime = (atTime - tr->trTime) * 0.001;
            VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
            break;
        case TR_SINE:
            deltaTime = (atTime - tr->trTime) / (float)tr->trDuration;
            phase = sin(deltaTime * M_PI * 2);
            VectorMA(tr->trBase, phase, tr->trDelta, result);
            break;
        case TR_LINEAR_STOP:
            if (atTime > tr->trTime + tr->trDuration) {
                atTime = tr->trTime + tr->trDuration;
            }
            deltaTime = (atTime - tr->trTime) * 0.001;
            if (deltaTime < 0) {
                deltaTime = 0;
            }
            VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
            break;
        case TR_GRAVITY:
            deltaTime = (atTime - tr->trTime) * 0.001;
            VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
            result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;
            break;
        case TR_CUSTOM_GRAVITY:
            deltaTime = (atTime - tr->trTime) * 0.001;
            VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
            result[2] -= 0.5 * tr->gravity * deltaTime * deltaTime;
            break;
        default:
            Com_Error(ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trType);
            break;
    }
}

void BG_EvaluateTrajectoryDelta(const trajectory_t* tr, int atTime, vec3_t result) {
    float deltaTime;
    float phase;

    switch (tr->trType) {
        case TR_STATIONARY:
        case TR_INTERPOLATE:
            VectorClear(result);
            break;
        case TR_LINEAR:
            VectorCopy(tr->trDelta, result);
            break;
        case TR_SINE:
            deltaTime = (atTime - tr->trTime) / (float)tr->trDuration;
            phase = cos(deltaTime * M_PI * 2);
            phase *= 0.5;
            VectorScale(tr->trDelta, phase, result);
            break;
        case TR_LINEAR_STOP:
            if (atTime > tr->trTime + tr->trDuration) {
                VectorClear(result);
                return;
            }
            VectorCopy(tr->trDelta, result);
            break;
        case TR_GRAVITY:
            deltaTime = (atTime - tr->trTime) * 0.001;
            VectorCopy(tr->trDelta, result);
            result[2] -= DEFAULT_GRAVITY * deltaTime;
            break;
        case TR_CUSTOM_GRAVITY:
            deltaTime = (atTime - tr->trTime) * 0.001;
            VectorCopy(tr->trDelta, result);
            result[2] -= tr->gravity * deltaTime;
            break;
        default:
            Com_Error(ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trType);
            break;
    }
}

char* eventnames[] = {
    "EV_NONE",
    "EV_FOOTSTEP",
    "EV_FOOTSTEP_METAL",
    "EV_FOOTSPLASH",
    "EV_FOOTWADE",
    "EV_SWIM",
    "EV_FALL_SHORT",
    "EV_FALL_MEDIUM",
    "EV_FALL_FAR",
    "EV_JUMP_PAD",
    "EV_JUMP",
    "EV_WATER_TOUCH",
    "EV_WATER_LEAVE",
    "EV_WATER_UNDER",
    "EV_WATER_CLEAR",
    "EV_ITEM_PICKUP",
    "EV_GLOBAL_ITEM_PICKUP",
    "EV_NOAMMO",
    "EV_CHANGE_WEAPON",
    "EV_DROP_WEAPON",
    "EV_FIRE_WEAPON",
    "EV_USE_ITEM0",
    "EV_USE_ITEM1",
    "EV_USE_ITEM2",
    "EV_USE_ITEM3",
    "EV_USE_ITEM4",
    "EV_USE_ITEM5",
    "EV_USE_ITEM6",
    "EV_USE_ITEM7",
    "EV_USE_ITEM8",
    "EV_USE_ITEM9",
    "EV_USE_ITEM10",
    "EV_USE_ITEM11",
    "EV_USE_ITEM12",
    "EV_USE_ITEM13",
    "EV_USE_ITEM14",
    "EV_USE_ITEM15",
    "EV_ITEM_RESPAWN",
    "EV_ITEM_POP",
    "EV_PLAYER_TELEPORT_IN",
    "EV_PLAYER_TELEPORT_OUT",
    "EV_GRENADE_BOUNCE",
    "EV_GENERAL_SOUND",
    "EV_GLOBAL_SOUND",
    "EV_GLOBAL_TEAM_SOUND",
    "EV_BULLET_HIT_FLESH",
    "EV_BULLET_HIT_WALL",
    "EV_MISSILE_HIT",
    "EV_MISSILE_MISS",
    "EV_MISSILE_MISS_METAL",
    "EV_RAILTRAIL",
    "EV_SHOTGUN",
    "EV_BULLET",
    "EV_PAIN",
    "EV_DEATH1",
    "EV_DEATH2",
    "EV_DEATH3",
    "EV_DROWN",
    "EV_OBITUARY",
    "EV_POWERUP_QUAD",
    "EV_POWERUP_BATTLESUIT",
    "EV_POWERUP_REGEN",
    "EV_POWERUP_ARMORREGEN",
    "EV_GIB_PLAYER",
    "EV_SCOREPLUM",
    "EV_PROXIMITY_MINE_STICK",
    "EV_PROXIMITY_MINE_TRIGGER",
    "EV_KAMIKAZE",
    "EV_OBELISKEXPLODE",
    "EV_OBELISKPAIN",
    "EV_INVUL_IMPACT",
    "EV_JUICED",
    "EV_LIGHTNINGBOLT",
    "EV_DEBUG_LINE",
    "EV_TAUNT",
    "EV_TAUNT_YES",
    "EV_TAUNT_NO",
    "EV_TAUNT_FOLLOWME",
    "EV_TAUNT_GETFLAG",
    "EV_TAUNT_GUARDBASE",
    "EV_TAUNT_PATROL",
    "EV_FOOTSTEP_SNOW",
    "EV_FOOTSTEP_WOOD",
    "EV_ITEM_PICKUP_SPEC",
    "EV_OVERTIME",
    "EV_GAMEOVER",
    "EV_MISSILE_MISS_DMGTHROUGH",
    "EV_THAW_PLAYER",
    "EV_THAW_TICK",
    "EV_SHOTGUN_KILL",
    "EV_POI",
    "EV_DEBUG_HITBOX",
    "EV_LIGHTNING_DISCHARGE",
    "EV_RACE_START",
    "EV_RACE_CHECKPOINT",
    "EV_RACE_FINISH",
    "EV_DAMAGEPLUM",
    "EV_AWARD",
    "EV_INFECTED",
    "EV_NEW_HIGH_SCORE",
};

void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);

void BG_AddPredictableEventToPlayerstate(int newEvent, int eventParm, playerState_t* ps) {
#ifdef _DEBUG
    {
        char buf[256];
        trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));
        if (atof(buf) != 0) {
#ifdef QAGAME
            Com_Printf(" game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount , ps->eventSequence, eventnames[newEvent], eventParm);
#else
            Com_Printf("Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount , ps->eventSequence, eventnames[newEvent], eventParm);
#endif
        }
    }
#endif
    ps->events[ps->eventSequence & (MAX_PS_EVENTS - 1)] = newEvent;
    ps->eventParms[ps->eventSequence & (MAX_PS_EVENTS - 1)] = eventParm;
    ps->eventSequence++;
}

void BG_TouchJumpPad(playerState_t* ps, entityState_t* jumppad) {
    if (ps->pm_type != PM_NORMAL && ps->pm_type != PM_FREEZE) {
        return;
    }

    if (ps->powerups[PW_FLIGHT]) {
        return;
    }

    if (ps->jumppad_ent != jumppad->number) {
        ps->events[ps->eventSequence & (MAX_PS_EVENTS - 1)] = EV_JUMP_PAD;
        ps->eventParms[ps->eventSequence & (MAX_PS_EVENTS - 1)] = 0;
        ps->eventSequence++;
    }
    ps->jumppad_ent = jumppad->number;
    ps->jumppad_frame = ps->pmove_framecount;
    ps->doubleJumped = 1;
    VectorCopy(jumppad->origin2, ps->velocity);
}

int BG_GetWeaponFromMeansOfDeath(int mod) {
    switch (mod) {
    case MOD_SHOTGUN:
        return WP_SHOTGUN;
    case MOD_GAUNTLET:
        return WP_GAUNTLET;
    case MOD_MACHINEGUN:
        return WP_MACHINEGUN;
    case MOD_GRENADE:
    case MOD_GRENADE_SPLASH:
        return WP_GRENADE_LAUNCHER;
    case MOD_ROCKET:
    case MOD_ROCKET_SPLASH:
        return WP_ROCKET_LAUNCHER;
    case MOD_PLASMA:
    case MOD_PLASMA_SPLASH:
        return WP_PLASMAGUN;
    case MOD_RAILGUN:
    case MOD_RAILGUN_HEADSHOT:
        return WP_RAILGUN;
    case MOD_LIGHTNING:
    case MOD_LIGHTNING_DISCHARGE:
        return WP_LIGHTNING;
    case MOD_BFG:
    case MOD_BFG_SPLASH:
        return WP_BFG;
    case MOD_NAIL:
        return WP_NAILGUN;
    case MOD_CHAINGUN:
        return WP_CHAINGUN;
    case MOD_PROXIMITY_MINE:
        return WP_PROX_LAUNCHER;
    case MOD_HMG:
        return WP_HMG;
    default:
        return WP_NONE;
    }
}

void BG_PlayerStateToEntityState(playerState_t* ps, entityState_t* s, qboolean snap) {
    int i;

    if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR) {
        s->eType = ET_INVISIBLE;
    } else if (ps->stats[STAT_HEALTH] <= GIB_HEALTH) {
        s->eType = ET_INVISIBLE;
    } else {
        s->eType = ET_PLAYER;
    }

    s->number = ps->clientNum;

    s->pos.trType = TR_INTERPOLATE;
    VectorCopy(ps->origin, s->pos.trBase);
    if (snap) {
        SnapVector(s->pos.trBase);
    }
    VectorCopy(ps->velocity, s->pos.trDelta);

    s->apos.trType = TR_INTERPOLATE;
    VectorCopy(ps->viewangles, s->apos.trBase);
    if (snap) {
        SnapVector(s->apos.trBase);
    }

    s->angles2[YAW] = ps->movementDir;
    s->legsAnim = ps->legsAnim;
    s->torsoAnim = ps->torsoAnim;
    s->clientNum = ps->clientNum;
    s->eFlags = ps->eFlags;
    if (ps->stats[STAT_HEALTH] <= 0 && ps->powerups[PW_FREEZE] == 0) {
        s->eFlags |= EF_DEAD;
    } else {
        s->eFlags &= ~EF_DEAD;
    }

    if (ps->externalEvent) {
        s->event = ps->externalEvent;
        s->eventParm = ps->externalEventParm;
    } else if (ps->entityEventSequence < ps->eventSequence) {
        int seq;

        if (ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
            ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
        }
        seq = ps->entityEventSequence & (MAX_PS_EVENTS - 1);
        s->event = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
        s->eventParm = ps->eventParms[seq];
        ps->entityEventSequence++;
    }

    s->weapon = ps->weapon;
    s->groundEntityNum = ps->groundEntityNum;

    s->powerups = 0;
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (ps->powerups[i]) {
            s->powerups |= 1 << i;
        }
    }

    s->loopSound = ps->loopSound;
    s->generic1 = ps->generic1 & 0x3f;
}

void BG_PlayerStateToEntityStateExtraPolate(playerState_t* ps, entityState_t* s, int time, qboolean snap) {
    int i;

    if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR) {
        s->eType = ET_INVISIBLE;
    } else if (ps->stats[STAT_HEALTH] <= GIB_HEALTH) {
        s->eType = ET_INVISIBLE;
    } else {
        s->eType = ET_PLAYER;
    }

    s->number = ps->clientNum;

    s->pos.trType = TR_LINEAR_STOP;
    VectorCopy(ps->origin, s->pos.trBase);
    if (snap) {
        SnapVector(s->pos.trBase);
    }
    VectorCopy(ps->velocity, s->pos.trDelta);
    s->pos.trTime = time;
    s->pos.trDuration = 50;

    s->apos.trType = TR_INTERPOLATE;
    VectorCopy(ps->viewangles, s->apos.trBase);
    if (snap) {
        SnapVector(s->apos.trBase);
    }

    s->angles2[YAW] = ps->movementDir;
    s->legsAnim = ps->legsAnim;
    s->torsoAnim = ps->torsoAnim;
    s->clientNum = ps->clientNum;
    s->eFlags = ps->eFlags;
    if (ps->stats[STAT_HEALTH] <= 0 && ps->powerups[PW_FREEZE] == 0) {
        s->eFlags |= EF_DEAD;
    } else {
        s->eFlags &= ~EF_DEAD;
    }

    if (ps->externalEvent) {
        s->event = ps->externalEvent;
        s->eventParm = ps->externalEventParm;
    } else if (ps->entityEventSequence < ps->eventSequence) {
        int seq;

        if (ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
            ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
        }
        seq = ps->entityEventSequence & (MAX_PS_EVENTS - 1);
        s->event = ps->events[seq] | ((ps->entityEventSequence & 3) << 8);
        s->eventParm = ps->eventParms[seq];
        ps->entityEventSequence++;
    }

    s->weapon = ps->weapon;
    s->groundEntityNum = ps->groundEntityNum;

    s->powerups = 0;
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (ps->powerups[i]) {
            s->powerups |= 1 << i;
        }
    }

    s->loopSound = ps->loopSound;
    s->generic1 = ps->generic1 & 0x3f;
}
