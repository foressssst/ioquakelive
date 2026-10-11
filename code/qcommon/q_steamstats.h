
#ifndef Q_STEAMSTATS_H
#define Q_STEAMSTATS_H

static const char* const steamStatNames[] = {
    "version",
    "kill_gauntlet", "kill_machinegun", "kill_shotgun", "kill_grenade", "kill_rocket",
    "kill_lightning", "kill_railgun", "kill_plasma", "kill_bfg", "kill_nailgun", "kill_proxmine",
    "kill_chaingun", "kill_hmg",
    "hits_machinegun", "hits_shotgun", "hits_grenade", "hits_rocket", "hits_lightning",
    "hits_railgun", "hits_plasma", "hits_bfg", "hits_nailgun", "hits_proxmine", "hits_chaingun",
    "hits_hmg",
    "shots_machinegun", "shots_shotgun", "shots_grenade", "shots_rocket", "shots_lightning",
    "shots_railgun", "shots_plasma", "shots_bfg", "shots_nailgun", "shots_proxmine",
    "shots_chaingun", "shots_hmg",
    "mod_shotgun", "mod_gauntlet", "mod_machinegun", "mod_grenade", "mod_rocket", "mod_plasma",
    "mod_railgun", "mod_lightning", "mod_bfg", "mod_water", "mod_slime", "mod_lava", "mod_crush",
    "mod_telefrag", "mod_laser", "BROKEN1", "mod_nailgun", "mod_chaingun", "mod_proxmine",
    "mod_kamikaze", "mod_juiced", "mod_suicide", "mod_falling", "mod_grapple", "mod_hmg",
    "mod_lightning_discharge", "mod_other",
    "medal_firstfrag", "medal_gauntlet", "medal_excellent", "medal_revenge", "medal_combokill",
    "medal_midair", "medal_perforated", "medal_rampage", "medal_impressive", "medal_capture",
    "medal_assist", "medal_defense", "medal_headshot", "medal_quadgod", "medal_perfect",
    "medal_accuracy",
    "wins", "losses", "played", "BROKEN2", "mod_hurt", "total_kills", "total_deaths",
};

#define STEAM_NUM_STATS ((int)(sizeof(steamStatNames) / sizeof(steamStatNames[0])))

#endif
