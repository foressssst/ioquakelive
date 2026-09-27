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
// cvar_help.c -- [ioquakelive] help texts for Quake Live's cvars
//
// The console shows a cvar's help in "help <name>", in the panel under the console
// and next to completions. Retail has no help texts: these say what each cvar does
// in this engine and its modules. The range and the default come from the cvar
// itself and are printed with the text, so the texts don't repeat them.
//
// Cvar_Get looks the name up when it creates a cvar, which gives the modules'
// cvars their text as they register. A cvar with no entry has no help.
//

#include "q_shared.h"
#include "qcommon.h"

// "    value = " at the start of a line that explains one value
#define HV(value) S_COLOR_VAL "    " value " " S_COLOR_HELP "= "
// the name of another cvar inside a text
#define HC(name) S_COLOR_CVAR name S_COLOR_HELP

typedef struct {
    const char* name;
    const char* help;
} cvarHelp_t;

static const cvarHelp_t cvarHelp[] = {
    //
    // cgame: view and weapon
    //
    {"cg_fov", "field of view, in degrees"},
    {"cg_zoomfov", "field of view while zoomed, in degrees"},
    {"cg_zoomToggle", "the zoom key toggles the zoom instead of holding it"},
    {"cg_zoomScaling", "zooms in and out gradually instead of at once"},
    {"cg_zoomSensitivity", "mouse sensitivity factor while zoomed"},
    {"cg_zoomOutOnDeath", "zooms back out when you die or freeze"},
    {"cg_specFov", "a spectator following a player sees with that player's field of view"},
    {"cg_viewsize", "size of the 3D view, in percent of the screen"},
    {"cg_drawGun", "the weapon in your hand\n"
                   HV("0") "Hidden\n"
                   HV("1") "Normal, with bobbing\n"
                   HV("2") "Still\n"
                   HV("3") "Transparent"},
    {"cg_gunX", "moves the weapon in your hand forward or back"},
    {"cg_gunY", "moves the weapon in your hand left or right"},
    {"cg_gunZ", "moves the weapon in your hand down"},
    {"cg_bob", "how much the view bobs as you move"},
    {"cg_kickScale", "how much taking damage kicks the view"},
    {"cg_waterWarp", "warps the view under water"},
    {"cg_vignette", "darkens the edges of the screen"},
    {"cg_muzzleFlash", "muzzle flash on your own weapon"},
    {"cg_autoswitch", "switches to a better weapon when you pick it up"},
    {"cg_switchOnEmpty", "switches to another weapon when yours runs out of ammo"},
    {"cg_switchToEmpty", "lets you select weapons that have no ammo"},
    {"cg_autoHop", "keeps jumping while the jump key is held"},
    {"cg_predictItems", "picks items up at once on your screen, before the server confirms it"},
    {"cg_predictLocalRailshots", "draws your own rail shots at once, before the server confirms them"},
    {"cg_nopredict", "turns off movement prediction; the view waits for the server"},
    {"cg_errordecay", "milliseconds over which a prediction error is smoothed out"},
    {"cg_weaponConfig", "commands run when you switch to a weapon that has no cg_weaponConfig_ of its own"},
    {"cg_weaponConfig_g", "commands run when you switch to the gauntlet"},
    {"cg_weaponConfig_mg", "commands run when you switch to the machinegun"},
    {"cg_weaponConfig_sg", "commands run when you switch to the shotgun"},
    {"cg_weaponConfig_gl", "commands run when you switch to the grenade launcher"},
    {"cg_weaponConfig_rl", "commands run when you switch to the rocket launcher"},
    {"cg_weaponConfig_lg", "commands run when you switch to the lightning gun"},
    {"cg_weaponConfig_rg", "commands run when you switch to the railgun"},
    {"cg_weaponConfig_pg", "commands run when you switch to the plasma gun"},
    {"cg_weaponConfig_bfg", "commands run when you switch to the BFG"},
    {"cg_weaponConfig_gh", "commands run when you switch to the grappling hook"},
    {"cg_weaponConfig_ng", "commands run when you switch to the nailgun"},
    {"cg_weaponConfig_pl", "commands run when you switch to the proximity launcher"},
    {"cg_weaponConfig_cg", "commands run when you switch to the chaingun"},
    {"cg_weaponConfig_hmg", "commands run when you switch to the heavy machinegun"},

    //
    // cgame: crosshair
    //
    {"cg_drawCrosshair", "crosshair shape, 0 for none"},
    {"cg_crosshairSize", "crosshair size"},
    {"cg_crosshairX", "moves the crosshair right from the centre"},
    {"cg_crosshairY", "moves the crosshair down from the centre"},
    {"cg_crosshairColor", "crosshair colour, from a palette of 26: 1 to 24 go round the colour wheel from red,\n"
                          "25 is white and 26 grey"},
    {"cg_crosshairBrightness", "crosshair brightness"},
    {"cg_crosshairHealth", "tints the crosshair by your health"},
    {"cg_crosshairPulse", "the crosshair pulses when you pick up an item"},
    {"cg_crosshairHitStyle", "what the crosshair does for " HC("cg_crosshairHitTime") " milliseconds after you hit someone\n"
                             HV("0") "Nothing\n"
                             HV("1") "Takes a colour by the kind of hit\n"
                             HV("2") "Takes " HC("cg_crosshairHitColor") "\n"
                             HV("3") "Grows by the kind of hit\n"
                             HV("4") "Grows and takes a colour by the kind of hit\n"
                             HV("5") "Grows by the kind of hit and takes " HC("cg_crosshairHitColor") "\n"
                             HV("6") "Pulses\n"
                             HV("7") "Pulses and takes a colour by the kind of hit\n"
                             HV("8") "Pulses and takes " HC("cg_crosshairHitColor")},
    {"cg_crosshairHitColor", "crosshair colour after a hit, from the " HC("cg_crosshairColor") " palette"},
    {"cg_crosshairHitTime", "milliseconds the crosshair hit effect lasts"},
    {"cg_enemyCrosshairNames", "shows an enemy's name under the crosshair"},
    {"cg_enemyCrosshairNamesOpacity", "opacity of an enemy's name under the crosshair"},
    {"cg_teammateCrosshairNames", "shows a teammate's name under the crosshair, when " HC("cg_teammateNames") " is 0"},
    {"cg_teammateCrosshairNamesOpacity", "opacity of a teammate's name under the crosshair"},
    {"cg_drawCrosshairTeamHealth", "a teammate's health and armor under their name at the crosshair\n"
                                   HV("0") "Off\n"
                                   HV("1") "In white\n"
                                   HV("2") "Tinted by value\n"
                                   "Shown with " HC("cg_teammateCrosshairNames") " on."},
    {"cg_drawCrosshairTeamHealthSize", "text size of a teammate's health and armor under the crosshair"},

    //
    // cgame: HUD
    //
    {"cg_draw2D", "draws the 2D HUD"},
    {"cg_drawStatus", "draws the HUD's status: health, armor, ammo and the rest of the HUD menus"},
    {"cg_draw3dIcons", "draws 3D models instead of flat icons in the HUD"},
    {"cg_drawIcons", "draws the HUD's icons"},
    {"cg_hudFiles", "file with the list of HUD menus\n"
                    HV("ui/hud.txt") "Normal\n"
                    HV("ui/hud2.txt") "Small\n"
                    HV("ui/hud3.txt") "Large"},
    {"cg_drawFPS", "shows the frame rate"},
    {"cg_drawSnapshot", "shows the number of the current snapshot"},
    {"cg_lagometer", "network graph in the lower right\n"
                     HV("0") "Off\n"
                     HV("1") "Basic\n"
                     HV("2") "Advanced, with the ping"},
    {"cg_speedometer", "shows your speed\n"
                       HV("0") "Off\n"
                       HV("1") "Graph on the right\n"
                       HV("2") "Graph in the centre\n"
                       HV("3") "Number in the centre\n"
                       HV("4") "Only where the HUD has a place for it"},
    {"cg_drawAttacker", "shows the last player who damaged you"},
    {"cg_drawAmmoWarning", "warns when the weapon in your hand is low on ammo\n"
                           HV("0") "Off\n"
                           HV("1") "Large\n"
                           HV("2") "Small"},
    {"cg_lowAmmoWarningPercentile", "ammo, as a fraction of the most a weapon holds, below which it counts as low"},
    {"cg_lowAmmoWarningSound", "sound when your ammo runs low; 1 and 2 are two sounds, 0 is none"},
    {"cg_lowAmmoWeaponBarWarning", "marks the weapons low on ammo in the weapon bar"},
    {"cg_weaponBar", "weapon bar\n"
                     HV("0") "Off\n"
                     HV("1") "On the left\n"
                     HV("2") "On the right\n"
                     HV("3") "Centred\n"
                     HV("4") "Classic"},
    {"cg_drawFullWeaponBar", "the weapon bar shows every weapon of the match, not only the ones you carry"},
    {"cg_drawItemPickups", "the last item you picked up, in the lower left; add the values\n"
                           HV("1") "Icon\n"
                           HV("2") "Name\n"
                           HV("4") "The match time of the pickup"},
    {"cg_drawRewards", "shows medals"},
    {"cg_drawRewardsRowSize", "medals shown side by side; past that, one medal with its count"},
    {"cg_drawFragMessages", "shows \"You fragged\" messages"},
    {"cg_obituaryRowSize", "frag messages shown at once"},
    {"cg_scorePlums", "shows score changes over players"},
    {"cg_damagePlum", "weapons whose hits show the damage as a number, by their short names\n"
                      "(g mg sg gl rl lg rg pg bfg gh cg ng pl hmg); empty for none"},
    {"cg_damagePlumColorStyle", "colour of the damage numbers\n"
                                HV("1") "One colour that fades\n"
                                HV("2") "By damage\n"
                                HV("3") "By weapon"},
    {"cg_levelTimerDirection", "the match clock\n"
                               HV("0") "Counts up\n"
                               HV("1") "Counts down when there is a time limit"},
    {"cg_drawPregameMessages", "shows what the warmup is waiting for"},
    {"cg_drawSpecMessages", "shows the spectator messages"},
    {"cg_drawDemoHUD", "draws the HUD while playing demos"},
    {"cg_drawProfileImages", "shows players' Steam profile pictures where the HUD has them"},
    {"cg_drawCheckpointRemaining", "race: shows how many checkpoints are left once three or fewer are"},
    {"cg_drawTieredArmorAvailability", "shows the tiered armor state in the HUD"},
    {"cg_useItemMessage", "message when you use an item\n"
                          HV("0") "Off\n"
                          HV("1") "Large\n"
                          HV("2") "Small"},
    {"cg_useItemWarning", "message when you try to use an item and have none\n"
                          HV("0") "Off\n"
                          HV("1") "Large\n"
                          HV("2") "Small"},
    {"cg_complaintWarning", "after a teammate kills you, asks whether to file a complaint"},
    {"cg_centertime", "seconds messages in the centre of the screen stay"},
    {"cg_drawInputCmds", "arrows for the movement keys being held\n"
                         HV("0") "Off\n"
                         HV("1") "Always, except for a free-flying spectator\n"
                         HV("2") "Only while following a player"},
    {"cg_drawInputCmdsSize", "size of the movement arrows"},
    {"cg_drawInputCmdsX", "horizontal position of the movement arrows"},
    {"cg_drawInputCmdsY", "vertical position of the movement arrows"},
    {"cg_itemTimers", "shows item respawn times when the server sends them (" HC("g_itemTimers") ")"},

    //
    // cgame: team
    //
    {"cg_drawTeamOverlay", "your teammates' health, armor, weapon and location\n"
                           HV("0") "Off\n"
                           HV("1") "Top right\n"
                           HV("2") "Bottom right"},
    {"cg_drawTeamOverlayOpacity", "opacity of the team overlay's background"},
    {"cg_drawTeamOverlaySize", "text size of the team overlay"},
    {"cg_drawTeamOverlayX", "moves the team overlay right"},
    {"cg_drawTeamOverlayY", "moves the team overlay down"},
    {"cg_selfOnTeamOverlay", "lists you in the team overlay too"},
    {"cg_teammateNames", "names over your teammates, with their health and armor\n"
                         HV("0") "Off\n"
                         HV("1") "Only near the middle of the screen\n"
                         HV("2") "Always"},
    {"cg_overheadNamesWidth", "most width of the names over players"},
    {"cg_teammatePOIs", "markers over your teammates when " HC("cg_teammateNames") " is 0"},
    {"cg_teammatePOIsMaxWidth", "largest size of the teammate markers"},
    {"cg_teammatePOIsMinWidth", "smallest size of the teammate markers"},
    {"cg_drawHitFriendTime", "milliseconds a teammate's marker shows they were hit"},
    {"cg_drawDeadFriendTime", "milliseconds a marker stays where a teammate died"},
    {"cg_flagPOIs", "markers over the flags"},
    {"cg_powerupPOIs", "markers over the powerups; 0 turns them off"},
    {"cg_poiMaxWidth", "largest size of the flag and powerup markers"},
    {"cg_poiMinWidth", "smallest size of the flag and powerup markers"},
    {"cg_drawSprites", "sprites over players: chat balloon, medals and markers"},
    {"cg_drawSpriteSelf", "draws those sprites over your own player in third person"},
    {"cg_voiceChatIndicator", "shows who is talking on voice chat"},

    //
    // cgame: chat
    //
    {"cg_chatbeep", "beeps on chat messages"},
    {"cg_teamChatBeep", "beeps on team chat messages"},
    {"cg_teamChatsOnly", "shows only team chat"},
    {"cg_teamChatTime", "milliseconds chat lines stay on the screen"},
    {"cg_teamChatHeight", "lines of chat kept on the screen"},
    {"cg_chatHistoryLength", "lines kept in the chat history"},

    //
    // cgame: spectating
    //
    {"cg_specItemTimers", "spectators: item respawn timers down the left; add the values\n"
                          HV("1") "Powerups\n"
                          HV("2") "Mega health\n"
                          HV("4") "Red armor\n"
                          HV("8") "Yellow armor"},
    {"cg_specItemTimersSize", "size of the spectator item timers"},
    {"cg_specItemTimersX", "horizontal position of the spectator item timers"},
    {"cg_specItemTimersY", "vertical position of the spectator item timers"},
    {"cg_specTeamVitals", "spectators: both teams' health and armor down the sides\n"
                          HV("0") "Off\n"
                          HV("1") "On\n"
                          HV("2") "With the health as a number"},
    {"cg_specTeamVitalsHealthColor", "thin bars tinted by health instead of team colours in " HC("cg_specTeamVitals")},
    {"cg_specTeamVitalsWidth", "width of the " HC("cg_specTeamVitals") " rows"},
    {"cg_specTeamVitalsY", "vertical position of the " HC("cg_specTeamVitals") " rows"},
    {"cg_specNames", "spectators: names over players\n"
                     HV("0") "Off\n"
                     HV("1") "Only near the middle of the screen\n"
                     HV("2") "Always"},
    {"cg_specDuelHealthArmor", "duel spectators: bars with both players' health and armor"},
    {"cg_specDuelHealthColor", "tints the duel spectator bars by health"},
    {"cg_followKiller", "a spectator following a player switches to whoever killed them"},
    {"cg_followPowerup", "a spectator follows whoever picks up\n"
                         HV("0") "Nothing\n"
                         HV("1") "Any powerup\n"
                         HV("2") "A flag"},

    //
    // cgame: sounds
    //
    {"cg_announcer", "announcer voice\n"
                     HV("1") "Default\n"
                     HV("2") "Vadrigar\n"
                     HV("3") "Daemia"},
    {"cg_announcerLastStandingVO", "the announcer calls out the last player standing"},
    {"cg_announcerLeadsVO", "the announcer calls out changes of the lead"},
    {"cg_announcerRewardsVO", "the announcer calls out medals"},
    {"cg_announcerTiesVO", "the announcer calls out ties"},
    {"cg_hitBeep", "sound when you hit someone\n"
                   HV("0") "Off\n"
                   HV("1") "One pitch\n"
                   HV("2") "Pitch falling with the damage\n"
                   HV("3") "Pitch rising with the damage"},
    {"cg_killBeep", "sound when you frag someone\n"
                    HV("0") "Off\n"
                    HV("1") "Ting\n"
                    HV("2") "Tink\n"
                    HV("3") "Dramatic\n"
                    HV("4") "Voosh\n"
                    HV("5") "Drum\n"
                    HV("6") "Bang\n"
                    HV("7") "Ding\n"
                    HV("8") "Cha-ching"},
    {"cg_raceBeep", "race: sound at each checkpoint, one of the " HC("cg_killBeep") " sounds"},
    {"cg_buzzerSound", "plays the buzzer when the time runs out"},
    {"cg_allowTaunt", "plays other players' taunts"},
    {"cg_footsteps", "footstep sounds"},
    {"s_killBeepVolume", "volume of " HC("cg_killBeep")},
    {"s_announcerVolume", "volume of the announcer"},
    {"s_ambient", "the maps' ambient sounds"},

    //
    // cgame: players
    //
    {"cg_forceEnemyModel", "model every enemy is drawn with; empty for their own"},
    {"cg_forceEnemySkin", "skin every enemy is drawn with, over " HC("cg_forceEnemyModel") " or their own model\n"
                          HV("bright") "A flat skin in your enemy colours\n"
                          HV("sport") "The sport skin\n"
                          "Empty leaves each of them their own."},
    {"cg_forceTeamModel", "model every teammate is drawn with; empty for their own"},
    {"cg_forceTeamSkin", "skin every teammate is drawn with, like " HC("cg_forceEnemySkin")},
    {"cg_forceRedTeamModel", "model the red team is drawn with, for spectators"},
    {"cg_forceBlueTeamModel", "model the blue team is drawn with, for spectators"},
    {"cg_enemyHeadColor", "colour of the forced enemy model's head, as 0xRRGGBBAA"},
    {"cg_enemyUpperColor", "colour of the forced enemy model's torso, as 0xRRGGBBAA"},
    {"cg_enemyLowerColor", "colour of the forced enemy model's legs, as 0xRRGGBBAA"},
    {"cg_teamHeadColor", "colour of the forced teammate model's head, as 0xRRGGBBAA"},
    {"cg_teamUpperColor", "colour of the forced teammate model's torso, as 0xRRGGBBAA"},
    {"cg_teamLowerColor", "colour of the forced teammate model's legs, as 0xRRGGBBAA"},
    {"cg_forceEnemyWeaponColor", "enemies' weapons take the enemy model's colours"},
    {"cg_forceTeamWeaponColor", "teammates' weapons take the teammate model's colours"},
    {"cg_deadBodyDarken", "darkens dead bodies to " HC("cg_deadBodyColor")},
    {"cg_deadBodyColor", "colour of dead bodies, as 0xRRGGBBAA"},
    {"cg_playerLean", "players lean into their strafes"},
    {"cg_shadows", "players' shadows"},
    {"cg_deferPlayers", "puts off loading a new player's model until a quiet moment"},
    {"cg_blood", "blood effects"},
    {"cg_screenDamage", "colour of the flash when an enemy hurts you, as 0xRRGGBBAA"},
    {"cg_screenDamage_Self", "colour of the flash when you hurt yourself, as 0xRRGGBBAA"},
    {"cg_screenDamage_Team", "colour of the flash when a teammate hurts you, as 0xRRGGBBAA"},
    {"cg_screenDamageAlpha", "strength of the flash when an enemy hurts you"},
    {"cg_screenDamageAlpha_Team", "strength of the flash when a teammate hurts you"},

    //
    // cgame: effects and items
    //
    {"cg_railStyle", "rail trail\n"
                     HV("1") "Beam\n"
                     HV("2") "Spiral"},
    {"cg_railTrailTime", "milliseconds rail trails last"},
    {"cg_rocketStyle", "rocket trail\n"
                       HV("1") "No plume\n"
                       HV("2") "Fire plume"},
    {"cg_plasmaStyle", "plasma bolts\n"
                       HV("1") "No trail\n"
                       HV("2") "Particle trails"},
    {"cg_lightningStyle", "lightning gun beam\n"
                          HV("1") "Quake Live\n"
                          HV("2") "QuakeWorld\n"
                          HV("3") "Team Arena\n"
                          HV("4") "Thin\n"
                          HV("5") "Wide"},
    {"cg_trueLightning", "how closely the lightning beam follows your aim: 1 exactly, 0 as the server sees it,\n"
                         "values in between mix the two"},
    {"cg_lightningImpact", "draws where the lightning beam hits"},
    {"cg_trueShotgun", "shotgun impacts\n"
                       HV("0") "A fancy spread\n"
                       HV("1") "Where the server's pellets land"},
    {"cg_flagStyle", "flags\n"
                     HV("1") "Cloth\n"
                     HV("2") "Hologram"},
    {"cg_weaponColor_grenade", "colour of grenades, as 0xRRGGBBAA"},
    {"cg_marks", "marks left by shots and explosions on walls"},
    {"cg_impactMarkTime", "milliseconds those marks stay"},
    {"cg_brassTime", "milliseconds spent shells stay; 0 drops none"},
    {"cg_bubbleTrail", "bubble trails under water"},
    {"cg_impactSparks", "sparks where bullets hit"},
    {"cg_impactSparksLifetime", "milliseconds the impact sparks last"},
    {"cg_impactSparksSize", "size of the impact sparks"},
    {"cg_impactSparksVelocity", "vertical speed of the impact sparks: negative falls, positive rises"},
    {"cg_smoke_SG", "shotgun smoke"},
    {"cg_smokeRadius_GL", "size of grenade smoke; 0 for none"},
    {"cg_smokeRadius_RL", "size of rocket smoke; 0 for none"},
    {"cg_smokeRadius_NG", "size of nail smoke; 0 for none"},
    {"cg_smokeRadius_haste", "size of the haste smoke; 0 for none"},
    {"cg_smokeRadius_flight", "size of the flight smoke; 0 for none"},
    {"cg_smokeRadius_dust", "size of the dust kicked up on landing; 0 for none"},
    {"cg_itemFx", "item effects, adding up bobbing, spinning and the respawn scaling"},
    {"cg_simpleItems", "draws items as flat icons"},
    {"cg_simpleItemsBob", "how simple items bob"},
    {"cg_simpleItemsHeightOffset", "height of simple items over the floor"},
    {"cg_simpleItemsRadius", "size of simple items"},

    //
    // cgame: demos and camera
    //
    {"cg_autoAction", "does at every match; add the values\n"
                      HV("1") "Records a demo\n"
                      HV("2") "Takes a screenshot at the end"},
    {"cg_thirdPerson", "third person view"},
    {"cg_thirdPersonRange", "distance of the third person camera"},
    {"cg_thirdPersonAngle", "angle of the third person camera around the player"},
    {"cg_thirdPersonPitch", "pitch of the third person camera; -1 follows your aim"},
    {"cg_cameraOrbit", "turns the camera around the player by this many degrees each step"},
    {"cg_cameraOrbitDelay", "milliseconds between camera orbit steps"},
    {"cg_timescaleFadeEnd", "timescale the fade reaches"},
    {"cg_timescaleFadeSpeed", "speed of the timescale fade; 0 for none"},
    {"cg_stereoSeparation", "eye separation for stereo rendering"},
    {"cg_scalePlayerModelsToBB", "scales player models to their bounding boxes"},
    {"cg_noplayeranims", "freezes player animations"},
    {"cg_animspeed", "player animations; 0 freezes them"},
    {"cg_showmiss", "prints prediction misses"},
    {"cg_debuganim", "prints the animations of this client"},
    {"cg_debugevents", "prints entity events"},
    {"cg_tracerchance", "chance that a machinegun bullet draws a tracer"},
    {"cg_tracerlength", "length of machinegun tracers"},
    {"cg_tracerwidth", "width of machinegun tracers"},

    //
    // ui
    //
    {"ui_browserSortKey", "column the server browser sorts by"},
    {"ui_browserShowFull", "the server browser lists full servers"},
    {"ui_browserShowEmpty", "the server browser lists empty servers"},
    {"ui_browserMaster", "master server the server browser asks"},
    {"ui_browserGameType", "game type the server browser shows"},
    {"ui_mousePitch", "the menu's invert mouse option"},

    //
    // game: match
    //
    {"g_gametype", "game type\n"
                   HV("0") "Free for all\n"
                   HV("1") "Duel\n"
                   HV("2") "Race\n"
                   HV("3") "Team deathmatch\n"
                   HV("4") "Clan arena\n"
                   HV("5") "Capture the flag\n"
                   HV("6") "One flag CTF\n"
                   HV("7") "Overload\n"
                   HV("8") "Harvester\n"
                   HV("9") "Freeze tag\n"
                   HV("10") "Domination\n"
                   HV("11") "Attack and defend\n"
                   HV("12") "Red rover"},
    {"g_factory", "the factory the map was started with"},
    {"g_factoryTitle", "name of the factory the map was started with"},
    {"fraglimit", "frags that win the match; 0 for no limit"},
    {"timelimit", "minutes the match lasts; 0 for no limit"},
    {"capturelimit", "captures that win the match; 0 for no limit"},
    {"scorelimit", "score that wins a round-based match; 0 for no limit"},
    {"roundlimit", "rounds that win the match"},
    {"roundtimelimit", "seconds each round lasts"},
    {"mercylimit", "score lead that ends the match early; 0 for none"},
    {"teamsize", "most players on each team; 0 for no limit"},
    {"g_teamSizeMin", "fewest players each team needs for the match to start"},
    {"g_overtime", "seconds each overtime lasts"},
    {"g_doWarmup", "players must ready up before the match starts"},
    {"g_warmup", "seconds of the countdown before the match starts"},
    {"sv_warmupReadyPercentage", "fraction of the players that must be ready for the match to start"},
    {"g_warmupReadyDelay", "seconds, once enough players are ready, before the rest are dealt with by "
                           HC("g_warmupReadyDelayAction") "; 0 never"},
    {"g_warmupReadyDelayAction", "what happens to the players still not ready when " HC("g_warmupReadyDelay") " runs out"},
    {"g_roundWarmupDelay", "milliseconds before each round starts"},
    {"g_freezeRoundDelay", "freeze tag: milliseconds before each round starts"},
    {"g_timeoutCount", "timeouts each team may call; 0 for none"},
    {"g_timeoutLen", "seconds a timeout lasts"},
    {"g_allowForfeit", "players may forfeit the match"},
    {"g_friendlyFire", "teammates can hurt each other"},
    {"g_friendlyFireDampen", "damage factor between teammates when " HC("g_friendlyFire") " is on"},
    {"g_teamForceBalance", "players can't join the team with more players"},
    {"g_teamAutoJoin", "connecting players join a team at once"},
    {"g_switchTeamDelay", "seconds before a player may switch teams again"},
    {"g_teamSpecFreeCam", "players out of a round may fly around instead of following their team"},
    {"g_shuffle_automatic", "shuffles the teams at the end of each match"},
    {"g_mercytime", "minutes into the match before " HC("mercylimit") " can end it"},
    {"g_customSettings", "settings changed from the factory's, as flags the scoreboard shows"},

    //
    // game: players, items and weapons
    //
    {"g_startingHealth", "health players spawn with"},
    {"g_startingHealthBonus", "extra health players spawn with, which counts down to " HC("g_startingHealth")},
    {"g_startingArmor", "armor players spawn with"},
    {"g_startingWeapons", "weapons players spawn with, as a bitmask"},
    {"g_startingAmmo_g", "gauntlet ammo players spawn with"},
    {"g_startingAmmo_mg", "machinegun ammo players spawn with"},
    {"g_startingAmmo_sg", "shotgun ammo players spawn with"},
    {"g_startingAmmo_gl", "grenade launcher ammo players spawn with"},
    {"g_startingAmmo_rl", "rocket launcher ammo players spawn with"},
    {"g_startingAmmo_lg", "lightning gun ammo players spawn with"},
    {"g_startingAmmo_rg", "railgun ammo players spawn with"},
    {"g_startingAmmo_pg", "plasma gun ammo players spawn with"},
    {"g_startingAmmo_bfg", "BFG ammo players spawn with"},
    {"g_startingAmmo_gh", "grappling hook ammo players spawn with"},
    {"g_startingAmmo_ng", "nailgun ammo players spawn with"},
    {"g_startingAmmo_pl", "proximity launcher ammo players spawn with"},
    {"g_startingAmmo_cg", "chaingun ammo players spawn with"},
    {"g_startingAmmo_hmg", "heavy machinegun ammo players spawn with"},
    {"g_loadout", "players choose the weapons they spawn with"},
    {"g_infiniteAmmo", "weapons never run out of ammo"},
    {"g_instaGib", "instagib: railguns only, and one hit kills"},
    {"g_weaponRespawn", "seconds before a picked up weapon comes back"},
    {"g_ammoRespawn", "seconds before picked up ammo comes back"},
    {"g_powerupRespawn", "seconds before a picked up powerup comes back"},
    {"g_spawnDelay_powerup", "seconds before the powerups first appear"},
    {"g_spawnDelayRandom_powerup", "random seconds added to " HC("g_spawnDelay_powerup")},
    {"g_quadDamageFactor", "damage factor of the quad damage"},
    {"g_battleSuitDampen", "fraction of the damage the battle suit lets through"},
    {"g_dropPowerups", "players drop their powerups when they die"},
    {"g_dropCmds", "players may drop weapons, powerups and flags with the drop commands"},
    {"g_itemTimers", "sends item respawn times to the players (" HC("cg_itemTimers") ")"},
    {"g_specItemTimers", "sends item respawn times to the spectators (" HC("cg_specItemTimers") ")"},
    {"g_damagePlums", "sends the damage of each hit to the attacker (" HC("cg_damagePlum") ")"},
    {"g_regenHealth", "health comes back over time"},
    {"g_regenHealthRate", "how fast health comes back with " HC("g_regenHealth")},
    {"g_regenArmor", "armor comes back over time"},
    {"g_regenArmorRate", "how fast armor comes back with " HC("g_regenArmor")},
    {"g_vampiricDamage", "fraction of the damage you deal that comes back to you as health"},
    {"g_respawn_delay_min", "fewest milliseconds before a dead player can respawn"},
    {"g_respawn_delay_max", "most milliseconds before a dead player respawns"},
    {"g_forcerespawn", "seconds before a dead player respawns on their own; 0 never"},
    {"g_suicidePenaltyTime", "extra respawn delay after a suicide"},
    {"g_railJump", "railgun shots push you back, for rail jumps"},
    {"g_guidedRocket", "rockets follow the shooter's aim"},
    {"g_lightningDischarge", "a lightning gun fired under water discharges into the water"},
    {"g_speed", "players' running speed"},
    {"g_gravity", "gravity"},
    {"g_knockback", "how far damage pushes players"},
    {"g_playerCylinders", "players are hit as cylinders instead of boxes"},
    {"g_midAirMinHeight", "height the victim must be off the ground for a mid-air medal"},
    {"g_proxMineTimeout", "milliseconds before a proximity mine blows up on its own"},
    {"g_maxFlightFuel", "flight powerup: fuel"},
    {"g_flightRefuelRate", "flight powerup: how fast the fuel refills"},
    {"g_flightThrust", "flight powerup: thrust"},
    {"armor_tiered", "tiered armor rules"},

    //
    // game: weapons
    //
    {"g_damage_g", "damage of the gauntlet"},
    {"g_damage_mg", "damage of a machinegun bullet"},
    {"g_damage_sg", "damage of a shotgun pellet"},
    {"g_damage_gl", "damage of a grenade"},
    {"g_damage_rl", "damage of a rocket"},
    {"g_damage_lg", "damage of the lightning gun"},
    {"g_damage_rg", "damage of the railgun"},
    {"g_damage_pg", "damage of a plasma bolt"},
    {"g_damage_bfg", "damage of a BFG shot"},
    {"g_damage_gh", "damage of the grappling hook"},
    {"g_damage_ng", "damage of a nail"},
    {"g_damage_pl", "damage of a proximity mine"},
    {"g_damage_cg", "damage of a chaingun bullet"},
    {"g_damage_hmg", "damage of a heavy machinegun bullet"},
    {"g_knockback_g", "knockback of the gauntlet"},
    {"g_knockback_mg", "knockback of the machinegun"},
    {"g_knockback_sg", "knockback of the shotgun"},
    {"g_knockback_gl", "knockback of grenades"},
    {"g_knockback_rl", "knockback of rockets"},
    {"g_knockback_rl_self", "knockback of your own rockets"},
    {"g_knockback_lg", "knockback of the lightning gun"},
    {"g_knockback_rg", "knockback of the railgun"},
    {"g_knockback_pg", "knockback of plasma"},
    {"g_knockback_pg_self", "knockback of your own plasma"},
    {"g_knockback_bfg", "knockback of the BFG"},
    {"g_knockback_gh", "knockback of the grappling hook"},
    {"g_knockback_ng", "knockback of nails"},
    {"g_knockback_pl", "knockback of proximity mines"},
    {"g_knockback_cg", "knockback of the chaingun"},
    {"g_knockback_hmg", "knockback of the heavy machinegun"},
    {"weapon_reload_gauntlet", "milliseconds between gauntlet hits"},
    {"weapon_reload_mg", "milliseconds between machinegun shots"},
    {"weapon_reload_sg", "milliseconds between shotgun shots"},
    {"weapon_reload_gl", "milliseconds between grenades"},
    {"weapon_reload_rl", "milliseconds between rockets"},
    {"weapon_reload_lg", "milliseconds between lightning gun shots"},
    {"weapon_reload_rg", "milliseconds between railgun shots"},
    {"weapon_reload_pg", "milliseconds between plasma bolts"},
    {"weapon_reload_bfg", "milliseconds between BFG shots"},
    {"weapon_reload_gh", "milliseconds between grappling hook shots"},
    {"weapon_reload_hook", "milliseconds between grappling hook shots"},
    {"weapon_reload_ng", "milliseconds between nailgun shots"},
    {"weapon_reload_prox", "milliseconds between proximity mines"},
    {"weapon_reload_cg", "milliseconds between chaingun shots"},
    {"weapon_reload_hmg", "milliseconds between heavy machinegun shots"},
    {"g_splashdamage_gl", "splash damage of grenades"},
    {"g_splashdamage_rl", "splash damage of rockets"},
    {"g_splashdamage_pg", "splash damage of plasma"},
    {"g_splashdamage_bfg", "splash damage of the BFG"},
    {"g_splashdamage_pl", "splash damage of proximity mines"},
    {"g_splashradius_gl", "splash radius of grenades"},
    {"g_splashradius_rl", "splash radius of rockets"},
    {"g_splashradius_pg", "splash radius of plasma"},
    {"g_splashradius_bfg", "splash radius of the BFG"},
    {"g_splashradius_pl", "splash radius of proximity mines"},
    {"g_velocity_gl", "speed of grenades"},
    {"g_velocity_rl", "speed of rockets"},
    {"g_velocity_pg", "speed of plasma bolts"},
    {"g_velocity_bfg", "speed of BFG shots"},
    {"g_velocity_gh", "speed of the grappling hook"},
    {"g_nailspeed", "speed of nails"},
    {"g_nailcount", "nails in each nailgun shot"},
    {"g_nailspread", "spread of the nailgun"},
    {"g_nailbounce", "nails bounce off walls"},
    {"g_nailbouncepercentage", "chance that a nail bounces"},

    //
    // game: modes
    //
    {"g_adCaptureScoreBonus", "attack and defend: points for a capture"},
    {"g_adElimScoreBonus", "attack and defend: points for eliminating the other team"},
    {"g_adTouchScoreBonus", "attack and defend: points for touching the flag"},
    {"g_roundDrawLivingCount", "a round that runs out of time goes to the team with more players alive"},
    {"g_roundDrawHealthCount", "a round that runs out of time goes to the team with more health"},
    {"g_freezeThawTime", "freeze tag: milliseconds a teammate needs to thaw you"},
    {"g_freezeAutoThawTime", "freeze tag: milliseconds before a frozen player thaws on their own; 0 never"},
    {"g_freezeThawRadius", "freeze tag: how close a teammate must stand to thaw you"},
    {"g_freezeThawThroughSurface", "freeze tag: teammates can thaw you through walls"},
    {"g_freezeProtectedSpawnTime", "freeze tag: milliseconds of protection after spawning"},
    {"g_freezeEnvironmentalRespawnDelay", "freeze tag: respawn delay after dying to the map"},
    {"g_freezeRemovePowerupsOnRound", "freeze tag: powerups are taken away at each round"},
    {"g_freezeResetHealthOnRound", "freeze tag: health is reset at each round"},
    {"g_freezeResetArmorOnRound", "freeze tag: armor is reset at each round"},
    {"g_freezeResetWeaponsOnRound", "freeze tag: weapons are reset at each round"},
    {"g_domCapTime", "domination: time needed to capture a point"},
    {"g_domScoreRate", "domination: how often captured points score"},
    {"g_obeliskHealth", "overload: health of the obelisks"},
    {"g_obeliskRegenPeriod", "overload: seconds between obelisk health regenerations"},
    {"g_obeliskRegenAmount", "overload: health an obelisk regains each time"},
    {"g_obeliskRespawnDelay", "overload: seconds before a destroyed obelisk comes back"},
    {"g_cubeTimeout", "harvester: seconds before a dropped skull disappears"},
    {"g_returnFlagOnSuicide", "a flag carrier who kills themself returns the flag"},
    {"g_throwFlagVelocity", "speed of a dropped flag"},
    {"g_rrInfected", "red rover: infection, fragged players join the zombies"},
    {"g_quadHog", "quad hog (free for all)"},

    //
    // game: voting, admin and chat
    //
    {"g_allowVote", "players may call votes"},
    {"g_allowVoteMidGame", "votes may be called once the match has started"},
    {"g_allowSpecVote", "spectators may call votes"},
    {"g_voteFlags", "vote types that can't be called, as a bitmask"},
    {"g_voteLimit", "votes each player may call on a map; 0 for no limit"},
    {"g_voteDelay", "time after the map starts before votes may be called"},
    {"g_accessFile", "file listing the server's admins, moderators and bans"},
    {"g_allTalk", "both teams hear each other on voice chat"},
    {"g_complaintLimit", "complaints before a team killer is kicked; 0 for no limit"},
    {"g_complaintDamageThreshold", "team damage needed before the victim can complain"},
    {"g_floodprot_maxcount", "chat lines a player may send in a row before flood protection holds them"},
    {"g_floodprot_decay", "how fast the flood protection count drains"},
    {"g_kickBadUserinfo", "kicks players whose userinfo is broken"},
    {"g_inactivity", "seconds without input before a player is moved to the spectators, or dropped with "
                     HC("g_dropInactive") "; 0 never"},
    {"g_dropInactive", "players inactive for " HC("g_inactivity") " seconds are dropped instead of moved to the spectators"},
    {"g_inactivityWarning", "seconds before " HC("g_inactivity") " acts that the player is warned"},
    {"g_motd", "message of the day shown to connecting players"},
    {"g_password", "password to join the server"},
    {"g_needpass", "the server has a password"},
    {"g_banIPs", "banned addresses, separated by spaces"},
    {"g_filterBan", "1 bans the addresses in " HC("g_banIPs") ", 0 only lets them in"},
    {"g_log", "file the game log is written to; empty for none"},
    {"g_logSync", "writes each line of the game log at once"},
    {"g_allowCustomHeadmodels", "players may use a head model other than their model's"},
    {"g_playermodelOverride", "model every player is drawn with; empty for their own"},
    {"g_playerheadmodelOverride", "head model every player is drawn with; empty for their own"},
    {"g_playerModelScale", "size of the player models"},
    {"g_playerheadScale", "size of the players' heads"},
    {"g_playerheadScaleOffset", "height offset of the scaled heads"},
    {"g_botSpawnList", "bots the server picks from to fill the game"},
    {"g_maxGameClients", "most players in the match; the rest spectate. 0 for no limit"},
    {"sv_quitOnExitLevel", "quits the server when the map ends"},
    {"g_podiumDist", "distance of the podium at the end of the match"},
    {"g_podiumDrop", "height of the podium at the end of the match"},

    //
    // game: movement
    //
    {"pmove_AirControl", "players can steer in the air"},
    {"pmove_AutoHop", "holding jump keeps jumping"},
    {"pmove_DoubleJump", "a jump soon after landing goes higher"},
    {"pmove_ChainJump", "jumps chained on landing get faster"},
    {"pmove_CrouchSlide", "crouching at speed slides"},
    {"pmove_CrouchSlideTime", "milliseconds a crouch slide lasts"},
    {"pmove_CrouchSlideFriction", "friction while crouch sliding"},
    {"pmove_RampJump", "jumps off ramps keep their upward speed"},
    {"pmove_RampJumpScale", "how much of that upward speed is kept"},
    {"pmove_StepJump", "jumping right after a step up goes higher"},
    {"pmove_AirSteps", "players can step up stairs in the air"},
    {"pmove_StepHeight", "highest step a player walks up"},
    {"pmove_JumpVelocity", "upward speed of a jump"},
    {"pmove_ChainJumpVelocity", "upward speed added by a chained jump"},
    {"pmove_StepJumpVelocity", "upward speed added by a step jump"},
    {"pmove_AirAccel", "acceleration in the air"},
    {"pmove_AirStopAccel", "deceleration in the air"},
    {"pmove_StrafeAccel", "acceleration while strafing in the air"},
    {"pmove_WalkAccel", "acceleration on the ground"},
    {"pmove_WalkFriction", "friction on the ground"},
    {"pmove_WaterSwimScale", "speed factor while swimming"},
    {"pmove_WaterWadeScale", "speed factor while wading"},
    {"pmove_WeaponDropTime", "milliseconds to put a weapon away"},
    {"pmove_WeaponRaiseTime", "milliseconds to bring a weapon up"},
    {"pmove_noPlayerClip", "players pass through each other"},
    {"pmove_velocity_gh", "speed at which the grappling hook pulls"},

    //
    // server
    //
    {"sv_hostname", "name of the server"},
    {"sv_maxclients", "most players on the server"},
    {"sv_privateClients", "slots kept for players with " HC("sv_privatePassword")},
    {"sv_privatePassword", "password for the private slots"},
    {"sv_fps", "server frames per second"},
    {"sv_pure", "players may only load files from the server's paks"},
    {"sv_cheats", "allows cheat commands and cheat protected cvars"},
    {"sv_minRate", "lowest rate the server lets players use; 0 for no limit"},
    {"sv_maxRate", "highest rate the server lets players use; 0 for no limit"},
    {"sv_minPing", "players with a lower ping can't connect; 0 for no limit"},
    {"sv_maxPing", "players with a higher ping can't connect; 0 for no limit"},
    {"sv_timeout", "seconds without packets before a player is dropped"},
    {"sv_zombietime", "seconds a dropped player's slot stays reserved"},
    {"sv_reconnectlimit", "seconds before a player may connect again"},
    {"sv_floodProtect", "limits how many commands a player may send each second"},
    {"sv_allowDownload", "players may download missing files from the server"},
    {"sv_dlURL", "web address players download missing files from"},
    {"sv_dlRate", "download speed for each player, in kilobytes per second"},
    {"sv_lanForceRate", "players on the local network get the highest rate"},
    {"sv_keywords", "keywords for the server browser"},
    {"sv_banFile", "file the server's bans are kept in"},
    {"sv_mapPoolFile", "file with the maps offered in the end-of-match vote"},
    {"sv_includeCurrentMapInVote", "the end-of-match vote may offer the current map"},
    {"nextmap", "command run when the map ends"},
    {"dmflags", "rule flags, as a bitmask"},
    {"bot_enable", "bots may be added"},
    {"bot_minplayers", "adds bots until the game has this many players"},
    {"bot_nochat", "bots don't chat"},
    {"bot_thinktime", "milliseconds between bot decisions"},
    {"bot_grapple", "bots use the grappling hook"},
    {"bot_rocketjump", "bots rocket jump"},

    //
    // client
    //
    {"name", "your player name"},
    {"model", "your player model, as model/skin"},
    {"headmodel", "your head model, as model/skin"},
    {"handicap", "your handicap, in percent: below 100 lowers your maximum health"},
    {"color1", "your weapon colour: it tints your rail trail, your weapon and its muzzle flash"},
    {"password", "password for the server you join"},
    {"rate", "most bytes per second the server may send you"},
    {"cl_maxpackets", "most packets per second sent to the server"},
    {"cl_packetdup", "earlier commands each packet repeats, against packet loss"},
    {"cl_timeNudge", "shifts the view in time, in milliseconds: negative is closer to the server's present"},
    {"cl_timeout", "seconds without server packets before disconnecting"},
    {"cl_maxPing", "the server browser leaves out servers with a higher ping"},
    {"sensitivity", "mouse sensitivity"},
    {"cl_mouseAccel", "mouse acceleration: how much the sensitivity grows with speed; negative lowers it"},
    {"cl_mouseAccelOffset", "speed at which the acceleration starts"},
    {"cl_mouseAccelPower", "shape of the acceleration: 2 grows with the speed, more grows faster"},
    {"cl_mouseSensCap", "most sensitivity the acceleration can reach; 0 for no cap"},
    {"cl_mouseAccelDebug", "writes each frame's mouse motion to mouse.log"},
    {"m_pitch", "vertical mouse speed factor; negative inverts the mouse"},
    {"m_yaw", "horizontal mouse speed factor"},
    {"m_forward", "speed of forward movement with the mouse"},
    {"m_side", "speed of sideways movement with the mouse"},
    {"m_filter", "frames of view angles averaged to smooth the mouse; 0 is off, up to 31"},
    {"m_cpi", "counts per inch of the mouse; with it the speed is in cm/s and " HC("m_yaw")
              " and " HC("m_pitch") " are degrees per cm"},
    {"in_mouse", "how the mouse is read\n"
                 HV("2") "Raw input\n"
                 HV("1") "The system's mouse\n"
                 HV("-1") "The cursor, warped to the centre\n"
                 HV("0") "No mouse"},
    {"in_mouseMode", "how the mouse ended up being read"},
    {"cl_freelook", "the mouse also looks up and down"},
    {"cl_run", "runs by default; +speed walks"},
    {"cl_yawspeed", "turning speed with the keys, in degrees per second"},
    {"cl_pitchspeed", "looking up and down with the keys, in degrees per second"},
    {"cl_anglespeedkey", "turning speed factor while +speed is held"},
    {"cl_allowDownload", "downloads missing files from servers that offer them"},
    {"cl_consoleKeys", "keys that open the console, separated by spaces"},
    {"cl_autoRecordDemo", "records a demo of every map you play"},
    {"cl_aviFrameRate", "frames per second of video capture"},
    {"cl_aviMotionJpeg", "captures video as motion JPEG"},
    {"cl_forceavidemo", "plays demos at " HC("cl_aviFrameRate") " while capturing video"},
    {"cl_conXOffset", "moves the console text right, in pixels"},
    {"cl_noprint", "stops printing to the console"},
    {"cl_lanForcePackets", "sends a packet every frame on the local network"},
    {"cl_renderer", "renderer library to load"},
    {"rconPassword", "password for remote console commands"},
    {"rconAddress", "server rcon commands are sent to; empty for the one you are on"},
    {"activeAction", "command run once you enter the game"},
    {"timedemo", "plays demos as fast as possible and prints the frame rate"},
    {"cl_timedemoLog", "file timedemo writes its frame times to"},
    {"cl_freezeDemo", "freezes demo playback"},
    {"cl_demoLength", "length of the demo being played, in milliseconds"},
    {"cl_demoPos", "where the demo being played is, in milliseconds from its start"},
    {"cl_demoMarks", "where the match restarts in the demo, in milliseconds, marked on its timeline"},
    {"cg_xray", "shows players through walls in their team colour; only in demos and as a spectator"},
    {"cg_freecam", "a free camera in demos, flown with the movement keys and the mouse"},
    {"cl_shownet", "prints network messages"},
    {"cl_showSend", "prints sent packets"},
    {"cl_showTimeDelta", "prints the time difference to the server"},
    {"cl_nodelta", "turns off delta compression"},
    {"cl_debugMove", "graph of the mouse movement"},
    {"r_inGameVideo", "plays videos inside maps"},

    //
    // sound
    //
    {"s_volume", "volume of sound effects"},
    {"s_musicvolume", "volume of the music"},
    {"s_voiceVolume", "volume of voice chat"},
    {"s_doppler", "moving sounds change their pitch"},
    {"s_initsound", "turns sound on"},
    {"s_mixahead", "seconds of sound mixed ahead"},
    {"s_mixPreStep", "extra sound mixed ahead each frame, in seconds"},
    {"s_show", "prints the sounds that play"},
    {"s_testsound", "plays a test tone"},
    {"s_sdlBits", "sound sample bits"},
    {"s_sdlSpeed", "sound sample rate; 0 for the default"},
    {"s_sdlChannels", "sound channels"},
    {"s_sdlDevSamps", "size of the sound device's buffer, in samples; 0 for the default"},
    {"s_sdlMixSamps", "size of the mixing buffer, in samples; 0 for the default"},
    {"com_soundMegs", "megabytes of memory for sounds"},

    //
    // input
    //
    {"in_mouse", "uses the mouse"},
    {"in_nograb", "doesn't take hold of the mouse"},
    {"in_joystick", "uses a joystick or gamepad"},
    {"in_joystickNo", "which joystick to use"},
    {"in_joystickUseAnalog", "reads analog sticks as analog axes"},
    {"joy_threshold", "dead zone of the joystick axes"},
    {"in_keyboardDebug", "prints key events"},

    //
    // renderer
    //
    {"r_mode", "screen mode\n"
               HV("-2") "The desktop's resolution\n"
               HV("-1") HC("r_customwidth") " by " HC("r_customheight") "\n"
               "0 and up are the modes " S_COLOR_CMD "modelist" S_COLOR_HELP " prints."},
    {"r_fullscreen", "fullscreen; a window uses " HC("r_windowedMode") " instead of " HC("r_mode")},
    {"r_noborder", "a window without borders"},
    {"r_customwidth", "screen width for " HC("r_mode") " -1"},
    {"r_customheight", "screen height for " HC("r_mode") " -1"},
    {"r_windowedMode", "mode of the window, like " HC("r_mode") "; -1 is " HC("r_windowedWidth") " by " HC("r_windowedHeight")},
    {"r_windowedWidth", "window width for " HC("r_windowedMode") " -1"},
    {"r_windowedHeight", "window height for " HC("r_windowedMode") " -1"},
    {"r_aspectRatio", "shape of the screen the video mode gives, for the menus\n"
                      HV("0") "4:3\n"
                      HV("1") "16:9\n"
                      HV("2") "16:10\n"
                      HV("3") "5:4"},
    {"r_enablePostProcess", "post effects: the colour correction and the bloom"},
    {"r_enableColorCorrect", "colour correction: gamma, overbright and contrast done on the picture, also in a window"},
    {"r_enableBloom", "bloom: light spilling around bright spots"},
    {"r_postProcessActive", "whether the post effects are running"},
    {"r_colorCorrectActive", "whether the colour correction is running"},
    {"r_bloomActive", "whether the bloom is running"},
    {"r_floatingPointFBOs", "16 bits a channel for the post effects' frame buffers"},
    {"r_contrast", "contrast: 0 is flat grey, 1 unchanged, more is stronger"},
    {"r_bloomBrightThreshold", "brightness a spot needs to bloom"},
    {"r_bloomIntensity", "strength of the bloom"},
    {"r_bloomSaturation", "colour of the bloom: 0 is grey, 1 as it is"},
    {"r_bloomSceneIntensity", "brightness of the picture under the bloom"},
    {"r_bloomSceneSaturation", "colour of the picture under the bloom: 0 is grey, 1 as it is"},
    {"r_bloomPasses", "1 blurs the bloom at half size; 2 blurs it again at a quarter, wider"},
    {"r_bloomBlurFalloff", "not used"},
    {"r_bloomBlurRadius", "not used"},
    {"r_bloomBlurScale", "not used"},
    {"r_displayRefresh", "refresh rate in fullscreen, in hertz; 0 for the default"},
    {"r_swapInterval", "waits for vertical sync"},
    {"r_centerWindow", "centres the window on the screen"},
    {"r_gamma", "brightness"},
    {"r_ignorehwgamma", "doesn't use the video card's gamma"},
    {"r_overBrightBits", "overbright bits of the screen; more is brighter"},
    {"r_mapOverBrightBits", "brightness of the maps' lighting"},
    {"r_intensity", "brightness factor of textures"},
    {"r_greyscale", "takes the colour out of the picture: 0 is colour, 1 grey"},
    {"r_picmip", "texture detail: 0 is full, each step halves the textures"},
    {"r_texturebits", "texture colour depth: 0 for the default, 16 or 32"},
    {"r_textureMode", "texture filtering\n"
                      HV("GL_NEAREST_MIPMAP_NEAREST") "None\n"
                      HV("GL_LINEAR_MIPMAP_NEAREST") "Bilinear\n"
                      HV("GL_LINEAR_MIPMAP_LINEAR") "Trilinear"},
    {"r_ext_texture_filter_anisotropic", "anisotropic texture filtering"},
    {"r_ext_max_anisotropy", "level of anisotropic filtering"},
    {"r_ext_compressed_textures", "compresses textures in video memory"},
    {"r_ext_multisample", "antialiasing samples; 0 for none"},
    {"r_colorbits", "colour bits of the screen; 0 for the default"},
    {"r_depthbits", "depth buffer bits; 0 for the default"},
    {"r_stencilbits", "stencil buffer bits"},
    {"r_detailtextures", "detail textures"},
    {"r_lodbias", "model detail: lower is more detailed"},
    {"r_subdivisions", "detail of curved surfaces: lower is smoother"},
    {"r_vertexLight", "lights the world by its vertices instead of lightmaps"},
    {"r_fastsky", "draws the sky in a flat colour, " HC("r_fastSkyColor")},
    {"r_fastSkyColor", "colour of " HC("r_fastSky") ", as 0xRRGGBB"},
    {"r_drawSkyFloor", "draws the cloud layer under the sky too"},
    {"r_teleporterFlash", "flashes the screen while teleporting; 0 leaves it black"},
    {"r_lodScale", "distance at which models lose detail: more keeps it longer"},
    {"r_dynamiclight", "dynamic lights from explosions and shots"},
    {"r_flares", "light flares"},
    {"r_railWidth", "width of the rail trail's spiral"},
    {"r_railCoreWidth", "width of the rail trail's core"},
    {"r_railSegmentLength", "length of the rail trail's segments"},
    {"r_ambientScale", "ambient light on models"},
    {"r_directedScale", "directed light on models"},
    {"r_finish", "waits for the video card to finish each frame"},
    {"r_screenshotJpegQuality", "quality of JPEG screenshots"},
    {"r_aviMotionJpegQuality", "quality of motion JPEG video capture"},
    {"r_postProcess", "post processing of the picture"},
    {"r_hdr", "high dynamic range rendering"},
    {"r_toneMap", "tone mapping"},
    {"r_autoExposure", "adjusts the exposure to the scene"},
    {"r_ssao", "screen space ambient occlusion"},
    {"r_normalMapping", "normal maps"},
    {"r_specularMapping", "specular maps"},
    {"r_deluxeMapping", "deluxe maps"},
    {"r_parallaxMapping", "parallax mapping"},
    {"r_cubeMapping", "cube map reflections"},
    {"r_sunShadows", "shadows from the sun"},
    {"r_shadowMapSize", "size of the shadow maps"},
    {"r_speeds", "prints renderer counts each frame"},
    {"r_showtris", "draws the outlines of triangles"},
    {"r_shownormals", "draws the normals"},
    {"r_lightmap", "shows only the lightmaps"},
    {"r_fullbright", "draws everything without lighting"},
    {"r_drawworld", "draws the world"},
    {"r_drawentities", "draws entities"},
    {"r_novis", "ignores the map's visibility data"},
    {"r_nocull", "doesn't cull anything"},
    {"r_lockpvs", "freezes the visible set where you stand"},
    {"r_measureOverdraw", "measures overdraw"},
    {"r_verbose", "prints more renderer messages"},

    //
    // common
    //
    {"com_maxfps", "most frames per second"},
    {"com_maxfpsUnfocused", "most frames per second while the window is in the background; 0 for no limit"},
    {"com_maxfpsMinimized", "most frames per second while the window is minimized; 0 for no limit"},
    {"com_busyWait", "waits for the next frame by spinning instead of sleeping"},
    {"com_hunkMegs", "megabytes of hunk memory"},
    {"com_zoneMegs", "megabytes of zone memory"},
    {"com_ansiColor", "colours in the terminal"},
    {"com_pipefile", "file the game reads commands from"},
    {"cl_allowConsoleChat", "text typed in the console without a slash is said in chat"},
    {"com_allowConsole", "lets the console be opened with the console key or toggleconsole"},
    {"cl_demoRecordMessage", "the recording indicator while a demo is being recorded: 0 none, 1 full, 2 short"},
    {"developer", "prints developer messages"},
    {"logfile", "writes the console to qconsole.log; 2 writes each line at once"},
    {"timescale", "game speed factor"},
    {"fixedtime", "each frame lasts this many milliseconds"},
    {"dedicated", "runs a dedicated server\n"
                  HV("0") "No\n"
                  HV("1") "On the local network\n"
                  HV("2") "On the internet"},
    {"journal", "records or plays back a journal of input"},
    {"fs_basepath", "folder with the game's data"},
    {"fs_homepath", "folder with your configs, screenshots and demos"},
    {"fs_game", "mod folder"},
    {"fs_basegame", "base game folder"},
    {"fs_debug", "prints file system activity"},
    {"fs_steampath", "Quake Live's folder in Steam"},

    //
    // network
    //
    {"net_enabled", "network protocols; add the values\n"
                    HV("1") "IPv4\n"
                    HV("2") "IPv6\n"
                    HV("4") "Prefers IPv6\n"
                    HV("8") "No multicast"},
    {"net_ip", "IPv4 address to use"},
    {"net_ip6", "IPv6 address to use"},
    {"net_port", "IPv4 port to use"},
    {"net_port6", "IPv6 port to use"},
    {"net_qport", "number that tells connections from the same address apart"},
    {"net_socksEnabled", "connects through a SOCKS proxy"},
    {"net_socksServer", "address of the SOCKS proxy"},
    {"net_socksPort", "port of the SOCKS proxy"},
    {"net_socksUsername", "user name for the SOCKS proxy"},
    {"net_socksPassword", "password for the SOCKS proxy"},
    {"net_dropsim", "drops this percentage of packets"},
    {"showpackets", "prints packets"},
    {"showdrop", "prints dropped packets"},
    {"cl_packetdelay", "delays sent packets by this many milliseconds"},
    {"sv_packetdelay", "delays the server's packets by this many milliseconds"},
    {"ch_drawLocalTime", "the clock of your machine, over the game\n"
                         HV("0") "Off\n"
                         HV("1") "24 hours\n"
                         HV("2") "12 hours, with am and pm"},
    {"ch_recordMessage", "while a demo is being recorded, its name on the screen"},
    {"ch_autoStats", "when a match ends, writes the console, with the scores in it, to a file "
                     "under stats/"},
    {"cg_demoKeys", "the list of the demo player's keys, which F1 opens and closes"},
    {"ch_obituaryBand", "a dark band behind each line of the kill feed, so it reads over the "
                        "map; it fades with the line"},
    {"ch_obituaryY", "how far down the kill feed sits from where the HUD puts it"},
    {"ch_recordingDemo", "CPMA's REC with the demo's name in the upper right while recording; the engine's own indicator is " HC("cl_demoRecordMessage")},
    {"ch_obituaryScale", "size of the kill feed's text, over the one the HUD asks for"},
    {"ch_obituaryX", "moves the kill feed sideways, in pixels of a 640x480 screen"},
    {"ch_obituaryRowGap", "the space between one line of the kill feed and the next, in pixels"},
    {"ch_obituaryPadX", "how far the band reaches past the names at each end, in pixels"},
    {"ch_obituaryPadY", "how far the band reaches over and under the line, in pixels"},
    {"ch_obituaryIconScale", "the weapon's size, over the height of a capital letter"},
    {"ch_obituaryIconGap", "the space at each side of the weapon, over the height of a capital letter"},
    {"ch_obituaryIconY", "moves the weapon up or down inside the band, in pixels"},
    {"ch_obituaryTextY", "moves the names up or down inside the band, in pixels"},
    {"ch_obituaryBandAlpha", "how dark the band is, from 0 to 1"},
    {"ch_obituaryCenter", "1 puts the capitals and the weapon on the band's middle line"},
    {"ch_followKiller", "watching, the camera jumps to whoever gets a frag, at most once "
                        "every two seconds; " HC("cg_followPowerup") " does the same with the "
                        "powerups and the flag"},
    {"ch_gameEvents", "the kill feed also carries the flag taken and captured, and, while you "
                      "watch, what the players pick up"},
    {"ch_drawKeys", "the movement keys of whoever you are watching, over the left of the "
                    "screen; yours are the ones you press, anybody else's are read back "
                    "from the way they move"},
    {"ch_drawTargetStatus", "watching a demo or spectating, the health and armour under the "
                            "crosshair are anybody's, not only your team's; it needs "
                            HC("cg_drawCrosshairTeamHealth")},
    {"cl_demorecording", "set by the game: 1 while a demo is being recorded"},
    {"cl_demoname", "set by the game: the demo being recorded"},

    //
    // engine: more
    //
    {"cl_anonymous", "kept from Quake 3, where it marked a player who gave no name (no effect in this build)"},
    {"cl_autoTimeNudge", "works the time nudge out from your ping instead of taking " HC("cl_timeNudge")},
    {"cl_demoFrags", "set by the game: where the frags are in the demo, for the marks on the demo bar"},
    {"cl_downloadName", "set by the game: the file being downloaded"},
    {"cl_motdString", "set by the game: the message of the day the master server sent"},
    {"cl_paused", "set by the game: 1 while the game is paused"},
    {"cl_quitOnDemoCompleted", "the game quits once the demo it plays ends"},
    {"cl_rendererDefault", "the renderer that was the default when your config was written, so a new default reaches a config that kept the old one"},
    {"cl_running", "set by the game: 1 while the client runs"},
    {"cl_serverStatusResendTime", "milliseconds before asking a server for its status again"},
    {"cl_steamId", "the Steam account the game gives the servers; 0 takes the one Steam is signed in with"},
    {"cl_viewAccel", "view acceleration of the joystick (no effect in this build)"},
    {"sex", "kept from Quake 3, where it picked your player's voice (no effect in this build)"},
    {"teamtask", "the job you took in the team orders menu, sent to the server"},
    {"color2", "your second colour: it tints the rings of your rail trail"},
    {"com_abnormalExit", "set by the game: 1 when the last run did not end cleanly"},
    {"com_basegame", "folder the game takes its files from; read when the game starts"},
    {"com_blood", "blood effects"},
    {"com_cameraMode", "hides your player and your weapon, to film with the camera"},
    {"com_configVersion", "set by the game: the version of the settings your config was written with"},
    {"com_errorMessage", "set by the game: the last error the game ran into"},
    {"com_gamename", "name the game gives the master server and the browser; read when the game starts"},
    {"com_homepath", "folder under your own where the game writes; read when the game starts"},
    {"com_minimized", "set by the game: 1 while the window is minimised"},
    {"com_preciseFrameTiming", "the game times each frame itself instead of leaving it to the system"},
    {"com_protocol", "protocol the game speaks to the servers; read when the game starts"},
    {"com_showtrace", "prints how many traces the game runs each frame"},
    {"com_speeds", "prints how long each part of a frame takes"},
    {"com_unfocused", "set by the game: 1 while another window is the one in front"},
    {"com_workers", "worker threads the game uses; -1 is one per core bar the main one"},
    {"protocol", "set by the game: the protocol this build speaks"},
    {"debug_protocol", "protocol the server browser asks for instead of this build's"},
    {"arch", "set by the game: the system and the processor the game was built for"},
    {"username", "set by the game: the user the game runs as"},
    {"version", "set by the game: this build of the game"},
    {"gamedate", "set by the game: the date the game module was built"},
    {"mapname", "set by the game: the map the match is on"},
    {"nextmaps", "set by the game: the maps offered in the vote at the end of the match"},
    {"practiceflags", "the practice settings the server runs with, sent to everyone playing"},
    {"session#", "set by the game: what a player keeps across a map change, one per client slot"},
    {"botsession#", "set by the game: what a bot keeps across a map change, one per client slot"},
    {"server#", "address of a server kept in the favourites (no effect in this build)"},
    {"cm_noAreas", "the map's areas are ignored, so everything is in sight of everything"},
    {"cm_noCurves", "the map's curved surfaces stop blocking"},
    {"cm_playerCurveClip", "players are stopped by the map's curved surfaces"},
    {"net_mcast6addr", "IPv6 multicast address local servers are looked for on; read when the network starts"},
    {"net_mcast6iface", "network interface used for the IPv6 multicast; 0 picks it by itself"},
    {"net_noudp", "turns the network off; read when the network starts"},
    {"net_strict", "the game quits when it cannot take the address and the port it was given"},
    {"j_forward", "how much the joystick moves you forward and back"},
    {"j_side", "how much the joystick moves you sideways"},
    {"j_up", "how much the joystick moves you up and down"},
    {"j_pitch", "how much the joystick looks up and down"},
    {"j_yaw", "how much the joystick turns you"},
    {"j_forward_axis", "joystick axis that moves you forward and back"},
    {"j_side_axis", "joystick axis that moves you sideways"},
    {"j_up_axis", "joystick axis that moves you up and down"},
    {"j_pitch_axis", "joystick axis that looks up and down"},
    {"j_yaw_axis", "joystick axis that turns you"},
    {"in_raw_useWindowHandle", "the raw mouse input is tied to the game's window instead of the desktop"},
    {"s_asyncLoad", "sounds are decoded on the workers while the map loads"},
    {"s_muteBackground", "the game goes quiet while another window is in front; read when the sound starts"},
    {"s_ambient", "the sounds the map makes by itself, like machinery and wind"},
    {"s_muted", "set by the game: 1 while the game is quiet"},
    {"s_pvs", "only plays the sounds of what you could see from where you stand"},
    {"s_voiceStep", "how much the voice volume moves per step (no effect in this build)"},
    {"r_availableModes", "set by the game: the screen modes the card and the screen take"},
    {"r_customPixelAspect", "shape of the pixel for " HC("r_mode") " -1; 1 is square"},
    {"r_dlightSaturation", "colour of the dynamic lights; 1 is the full colour and 0 is grey"},
    {"r_mapOverBrightCap", "brightest the map's lighting is allowed to get"},
    {"r_maxQueuedFrames", "frames the card may have queued; 0 leaves it to the driver, as retail does"},
    {"r_prefetchTextures", "the map's textures are decoded on the workers ahead of time; 0 loads them one by one"},
    {"r_preferOpenGLES", "uses OpenGL ES\n"
                         HV("-1") "Only where the system has no other\n"
                         HV("0") "Never\n"
                         HV("1") "Whenever it can"},
    {"r_allowSoftwareGL", "lets the game run on a driver that draws without a card"},
    {"r_sdlDriver", "set by the game: the driver SDL opened the window with"},
    {"r_gl_vendor", "set by the game: who made the graphics card"},
    {"r_gl_renderer", "set by the game: the graphics card the game draws with"},
    {"r_gl_reserved", "set by the game: reserved by retail; always 0"},
    {"r_stereoEnabled", "asks the driver for a stereo window"},
    {"r_uifullscreen", "set by the game: 1 while the menus own the whole screen"},
    {"r_saveFontData", "writes the font data the game builds to disk (no effect in this build)"},
    {"r_debugAds", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_debugFontAtlas", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_debugShaderIndex", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_debugSortExcept", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_ext_gamma_control", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_forceMergeEntities", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_glDriver", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_ignore", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_ignoreFastPath", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_logFile", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_noFastRestart", "the window changing size restarts the whole video instead of taking the "
                        "new size where the renderer can"},
    {"r_primitives", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_showSmp", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_skipLargeBatches", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_skipSmallBatches", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_smp", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"r_stereo", "retail registers it for its own renderer, which this one does not have (no effect in this build)"},
    {"debuggraph", "draws the debugging graph"},
    {"timegraph", "the debugging graph shows how long each frame takes"},
    {"graphheight", "height of the debugging graph"},
    {"graphscale", "how much the debugging graph stretches what it draws"},
    {"graphshift", "how far up the debugging graph draws"},
    {"zmq_rcon_enable", "the server takes rcon over ZeroMQ; read when the server starts"},
    {"zmq_rcon_ip", "address the ZeroMQ rcon listens on; read when the server starts"},
    {"zmq_rcon_port", "port the ZeroMQ rcon listens on; read when the server starts"},
    {"zmq_rcon_password", "password the ZeroMQ rcon asks for"},
    {"zmq_stats_enable", "the server publishes its statistics over ZeroMQ; read when the server starts"},
    {"zmq_stats_ip", "address the statistics are published on; read when the server starts"},
    {"zmq_stats_port", "port the statistics are published on; read when the server starts"},
    {"zmq_stats_password", "password asked of whoever reads the statistics"},

    //
    // server, movement and bots: more
    //
    {"sv_altEntDir", "reads the level's entities from \"<dir>/<map>.ent\" instead of taking the map's own"},
    {"sv_dumpEntities", "writes the map's entities to \"ents/<map>.ent\" as the level loads"},
    {"sv_errorExit", "a dedicated server shuts itself down when it hits an error (no effect in this build)"},
    {"sv_gtid", "set by the game: the identifier this match has on the service (no effect in this build)"},
    {"sv_idleExit", "seconds a dedicated server waits with no map before shutting itself down; 0 to stay up"},
    {"sv_killserver", "shuts down the server running here; the menus use it\n"
                      HV("0") "Keeps it running\n"
                      HV("1") "Shuts it down\n"
                      HV("2") "Shuts it down without dropping the player here"},
    {"sv_mapChecksum", "set by the game: the checksum of the map being played"},
    {"sv_mapname", "set by the game: the map being played"},
    {"sv_master", "the server answers queries and shows up in the server browser, the local network one included"},
    {"sv_padPackets", "adds this many empty bytes to every message sent, to measure the rate"},
    {"sv_pakNames", "set by the game: the names of the paks a pure server allows"},
    {"sv_paks", "set by the game: the checksums of the paks a pure server allows"},
    {"sv_paused", "set by the game: 1 while the server is paused"},
    {"sv_quitOnEmpty", "a dedicated server shuts itself down once the last player leaves"},
    {"sv_referencedPakNames", "set by the game: the names of the paks the map being played needs"},
    {"sv_referencedPaks", "set by the game: the checksums of the paks the map being played needs"},
    {"sv_referencedSteamworks", "set by the game: the Workshop items the match uses"},
    {"sv_running", "set by the game: 1 while a server runs here"},
    {"sv_serverid", "set by the game: the number that tells this map load apart from the last"},
    {"sv_serverType", "how far the server reaches\n"
                      HV("0") "Anyone\n"
                      HV("1") "Local network only"},
    {"sv_setSteamAccount", "the Steam game server account the server logs in with; empty to log in anonymously"},
    {"sv_showloss", "prints the moves lost between a client and the server (no effect in this build)"},
    {"sv_vac", "the server runs with VAC; read when the game starts"},
    {"sv_workshopFile", "file that lists the Workshop items the server loads (no effect in this build)"},
    {"pmove_AirStepFriction", "speed lost when stepping up in the air"},
    {"pmove_BunnyHop", "hopping forward keeps accelerating past the running speed"},
    {"pmove_CircleStrafeFriction", "friction while running forward and strafing at once"},
    {"pmove_CrouchStepJump", "jumping right after a crouched step up goes higher"},
    {"pmove_JumpTimeDeltaMin", "milliseconds that have to pass before another jump counts"},
    {"pmove_JumpVelocityMax", "highest upward speed a jump off a ramp reaches"},
    {"pmove_JumpVelocityScaleAdd", "how much a chained jump scales the jump speed up by"},
    {"pmove_JumpVelocityTimeThreshold", "milliseconds after a jump in which the next one counts as chained"},
    {"pmove_JumpVelocityTimeThresholdOffset", "part of that time in which a chained jump still gets the whole boost"},
    {"pmove_WishSpeed", "top speed a pure strafe asks for in the air"},
    {"weapon_gravity_rl", "gravity pulling rockets down; 0 for none"},
    {"weapon_gravity_pg", "gravity pulling plasma bolts down; 0 for none"},
    {"weapon_gravity_bfg", "gravity pulling BFG shots down; 0 for none"},
    {"weapon_gravity_ng", "gravity pulling nails down; 0 for none"},
    {"bot_autoReady", "bots ready up on their own during warmup"},
    {"bot_challenge", "bots play to challenge you: they aim straight at an enemy they have had in sight"},
    {"bot_dynamicSkill", "bots raise their skill to keep up with the player (no effect in this build)"},
    {"bot_gauntlet", "the training bot carries the gauntlet instead of the rocket launcher"},
    {"bot_gauntletOnly", "bots only take damage from the gauntlet, and do no damage to each other"},
    {"bot_instaGibAimSkill", "how well bots aim in instagib (no effect in this build)"},
    {"bot_itemDelayTime", "milliseconds a bot waits before going for an item (no effect in this build)"},
    {"bot_predictobstacles", "bots look ahead for doors and movers in their way"},
    {"bot_startingSkill", "skill the bots start at, from 1 to 5"},
    {"bot_teamkill", "bots shoot at their own team (no effect in this build)"},
    {"bot_training", "the training match the bot leads\n"
                     HV("0") "None\n"
                     HV("1") "The tour of the map\n"
                     HV("2") "The tour, with the bot waiting for you\n"
                     HV("3") "The practice round after the tour"},
    {"bot_followMe", "the training bot follows you around"},
    {"bot_followDist", "units the bot keeps between you and itself while it follows you"},
    {"bot_showTourPoints", "draws the stops of the training tour (no effect in this build)"},
    {"bot_fastchat", "bots chat as often as they can"},
    {"bot_testichat", "prints the initial chats of a bot character, to check them"},
    {"bot_testrchat", "prints the replies a bot has for what it is told, to check them"},
    {"bot_report", "teammate bots report what they are doing in capture the flag"},
    {"bot_log", "writes what the bots think to a log file"},
    {"bot_aasoptimize", "the navigation file is optimised as it is written"},
    {"bot_forceclustering", "works out the map's clusters again instead of taking those in the navigation file"},
    {"bot_forcereachability", "works out the map's reachabilities again instead of taking those in the navigation file"},
    {"bot_forcewrite", "writes the navigation file again even when the one on disk is good"},
    {"bot_saveroutingcache", "writes the routing the bots worked out to disk, so the next match starts with it"},
    {"bot_reloadcharacters", "reads the bot characters from disk every time one is loaded"},
    {"bot_reachability", "draws the ways out of the area you stand in"},
    {"bot_groundonly", "draws only the floor of the areas shown"},
    {"bot_maxdebugpolys", "how many shapes the bot debugging can draw at once"},
    {"bot_showAreas", "draws the navigation areas of the map (no effect in this build)"},
    {"bot_showAreaNumber", "draws the number of the navigation areas (no effect in this build)"},
    {"bot_showAvoidSpots", "draws the spots the bots keep away from (no effect in this build)"},
    {"bot_showPath", "draws the path a bot is following (no effect in this build)"},
    {"bot_visualizejumppads", "draws where every jump pad in the map throws you"},
    {"bot_testclusters", "checks the map's clusters and prints what is wrong with them"},
    {"bot_testsolid", "checks the navigation areas for ones stuck inside the map"},
    {"bot_debug", "the bots take the debugging commands"},
    {"bot_developer", "prints what the bot library is doing while it works"},
    {"bot_memorydump", "prints what the bots have in memory"},
    {"bot_pause", "the bots stop thinking and stand still"},
    {"bot_hud", "client the bot debugging draws its display for; -1 for none (no effect in this build)"},
    {"bot_breakPoint", "stops on the bot the debugging is following (no effect in this build)"},
    {"bot_debugVar", "value the bot debugging reads, whatever it is being used for (no effect in this build)"},
    {"bot_interbreedbots", "how many bots take part in the interbreeding"},
    {"bot_interbreedchar", "bot character the interbreeding starts from"},
    {"bot_interbreedcycle", "matches that go by between one interbreeding and the next"},
    {"bot_interbreedwrite", "file the interbred bots are written to"},

    //
    // game: more
    //
    {"g_damage_sg_outer", "damage of the shotgun pellets that land outside the middle of the spread"},
    {"g_damage_sg_falloff", "the shotgun loses damage with distance"},
    {"g_range_sg_falloff", "units over which the shotgun loses its damage"},
    {"g_damage_lg_falloff", "the lightning gun loses damage with distance"},
    {"g_range_lg_falloff", "units over which the lightning gun loses its damage"},
    {"g_headShotDamage_rg", "damage a railgun shot does to the head; 0 for the same as anywhere else"},
    {"g_ironsights_mg", "how much the machinegun spread tightens while you hold the zoom"},
    {"g_gauntletSpeedFactor", "how fast you move with the gauntlet in hand, as a factor of your speed"},
    {"g_max_knockback", "the most knockback a hit can give"},
    {"g_knockback_z", "upward push added to the knockback of a hit"},
    {"g_knockback_z_self", "upward push added to the knockback of your own splash"},
    {"g_knockback_cripple", "seconds you cannot steer after taking knockback; 0 for none"},
    {"g_splashdamageOffset", "how far from the surface the centre of a splash sits"},
    {"g_rocketsplashOffset", "how far back along its flight a rocket looks for a clear line to its splash"},
    {"g_latchedHookOffset", "how far into the surface the hook latches"},
    {"g_kamiAttenuate", "units over which the kamikaze loses its force (no effect in this build)"},
    {"g_kamiMinRatio", "the least of its damage the kamikaze does at the edge (no effect in this build)"},
    {"g_dropDamagedHealth", "a player who takes damage sheds health bubbles worth what they lost"},
    {"g_grantItemOnSpawn", "item every player is given when they spawn (no effect in this build)"},
    {"g_bestStartingWeapons", "the weapons, best first, that a starting weapon choice falls back through"},
    {"g_accelFactor_rl", "how much rockets speed up every g_accelRate step; 1 keeps their speed"},
    {"g_accelFactor_pg", "how much plasma bolts speed up every g_accelRate step; 1 keeps their speed"},
    {"g_accelFactor_bfg", "how much BFG shots speed up every g_accelRate step; 1 keeps their speed"},
    {"g_accelRate_rl", "milliseconds between the speed ups of rockets"},
    {"g_accelRate_pg", "milliseconds between the speed ups of plasma bolts"},
    {"g_accelRate_bfg", "milliseconds between the speed ups of BFG shots"},
    {"g_forceDmgThroughSurface", "rockets damage through every thin surface, not only the ones marked for it"},
    {"g_dmgThroughSurfaceAngularThreshold", "how squarely a rocket has to face a thin surface to damage through it"},
    {"g_dmgThroughSurfaceDistance", "how deep the surface may be for a rocket to damage through it"},
    {"g_dmgThroughSurfaceDampening", "part of the splash radius that still reaches the far side of the surface"},
    {"g_ammoPack", "every ammo box on the map is a pack that gives ammo for several weapons"},
    {"g_ammoPackHack", "every ammo box spawns, whatever its kind, and becomes what " HC("g_ammoPack") " asks for"},
    {"g_itemHeight", "how high over the floor the items on the map float"},
    {"g_spawnItems", "items to spawn on every map, by name; empty leaves the map as it is"},
    {"g_spawnItemAmmo", "the map's ammo boxes spawn"},
    {"g_spawnItemArmor", "the map's armors spawn"},
    {"g_spawnItemHealth", "the map's health spawns"},
    {"g_spawnItemHoldable", "the map's holdable items spawn"},
    {"g_spawnItemPowerup", "the map's powerups spawn"},
    {"g_spawnItemWeapons", "the map's weapons spawn"},
    {"g_spawnArmor", "armor every player spawns with"},
    {"g_spawnArmorDmgScale", "part of the damage that spawn armor takes for you"},
    {"g_spawnDelay_key", "seconds before a key comes back"},
    {"g_spawnDelayRandom_key", "seconds of random delay added before a key comes back"},
    {"g_spawnMinDistance", "how far from a living player a spawn spot has to be"},
    {"g_spawnRandomRatio", "part of the furthest spawn spots the game picks at random among"},
    {"g_enemyTeamRespawnRatio", "how much heavier an enemy weighs than a teammate when a spawn spot is picked"},
    {"g_maxDeferredSpawns", "failed team spawns in a row before any spot on the map will do"},
    {"g_droppedPowerupsDecay", "a dropped powerup runs out on its own"},
    {"g_dropSkulls", "a player killed in Harvester drops their skulls"},
    {"g_runes", "runes, the powerups a player keeps until they die"},
    {"g_flagBounce", "how much a dropped flag bounces"},
    {"g_flagPhysics", "a dropped flag slides and rolls instead of stopping where it lands"},
    {"g_droppedFlagBonus", "score for taking a flag someone else dropped"},
    {"g_throwFlagForwardMult", "how hard a thrown flag is sent forward"},
    {"g_tackleFlag", "a flag carrier can be tackled (no effect in this build)"},
    {"g_neutralFlagPingRate", "milliseconds between the marks that give away the carrier of the neutral flag"},
    {"g_teamForcePresent", "a team has to have someone in it for the match to go on"},
    {"g_teamSpawnAsSpec", "a player who joins a team in a round already going starts it as a spectator"},
    {"g_teamSpecSayEnable", "a player out of the round can talk to their team (no effect in this build)"},
    {"g_localTeamPref", "the team the menu asked for when starting a match here"},
    {"g_shuffle_minplayers", "players needed for a shuffle to go through"},
    {"g_shuffle_automatic_minplayers", "players needed for the automatic shuffle"},
    {"g_shuffle_timedelay", "milliseconds the teams are shown before a shuffle takes effect"},
    {"g_forfeit", "a team left alone loses the match"},
    {"g_domDistressThreshold", "how far a point has to be taken before its owners are warned"},
    {"g_domEnableContention", "a point stops being taken while both teams stand on it"},
    {"g_domNeutralFlag", "a point goes back to neutral before the other team can take it"},
    {"g_domTeammateCapScale", "how much each teammate beyond the first adds to the speed a point is taken at"},
    {"g_freezeThawTick", "a thawing player ticks the seconds left"},
    {"g_freezeThawWinningTeam", "the winning team is thawed too when a round ends"},
    {"g_freezeAllowRespawn", "a frozen player can spawn again instead of waiting to be thawed (no effect in this build)"},
    {"g_debugThawTime", "prints the thaw time left while someone is being thawed"},
    {"g_rrAllowNegativeScores", "a Red Rover score can go below zero"},
    {"g_rrDamageScoreBonus", "extra score for a frag in Red Rover"},
    {"g_rrDeathScorePenalty", "score taken away when you die in Red Rover"},
    {"g_rrRoundScoreBonus", "extra score for winning a Red Rover round"},
    {"g_rrInfectedSpreadTime", "seconds before the infection spreads"},
    {"g_rrInfectedSpreadWarningTime", "seconds before the infection spreads that the warning goes out"},
    {"g_rrInfectedSurvivorMinSpeed", "speed a survivor has to keep to score"},
    {"g_rrInfectedSurvivorPingRate", "milliseconds between the marks that give a survivor away"},
    {"g_rrInfectedSurvivorScoreBonus", "score a survivor is given each time"},
    {"g_rrInfectedSurvivorScoreMethod", "how a survivor scores\n"
                                        HV("0") "Nothing\n"
                                        HV("1") "For staying alive\n"
                                        HV("2") "For staying alive and moving"},
    {"g_rrInfectedSurvivorScoreRate", "seconds between a survivor's scores"},
    {"g_rrInfectedZombieFragBonus", "score a zombie is given for a frag"},
    {"g_rrInfectedZombieHealthBonus", "extra health a zombie spawns with"},
    {"g_rrInfectedZombieSpeed", "how fast a zombie moves, as a factor of the normal speed"},
    {"g_quadHogTime", "seconds the quad waits on the ground before it goes back"},
    {"g_quadHogIdle", "seconds a quad carrier may stand still before the quad is taken away"},
    {"g_quadHogPingRate", "milliseconds between the marks that give the quad carrier away"},
    {"g_suddenDeathRespawn", "players come back in sudden death"},
    {"g_suddenDeathRespawnStart", "seconds you wait to come back at the start of sudden death"},
    {"g_suddenDeathRespawnIncrement", "seconds added to that wait each time"},
    {"g_suddenDeathRespawnMax", "the longest that wait gets"},
    {"g_suddenDeathRespawnTick", "seconds between those steps"},
    {"g_suddenDeathRespawnPrint", "tells players how long they wait to come back in sudden death (no effect in this build)"},
    {"g_regenArmorAfterHealth", "armor only regenerates once health is full"},
    {"g_lastManStandingWarning", "the last player of a team is told they are alone"},
    {"g_lastManStandingMessage", "what the last player standing is told"},
    {"g_allowKill", "the kill command works"},
    {"g_warmupDelay", "seconds of countdown once everyone is ready"},
    {"g_disableLoadout", "weapons taken out of the starting weapon choice, as a bitmask of the weapon numbers"},
    {"g_training", "set by the game: 1 while the tutorial runs"},
    {"g_trainingSpanish", "the tutorial speaks Spanish"},
    {"g_skipTrainingEnable", "the tutorial can be skipped"},
    {"g_isBotOnly", "set by the game: 1 while a training match runs, where bots are not added by hand"},
    {"g_enableDust", "set by the map: dust under the players' feet"},
    {"g_forceAtmosphericEffects", "weather to force on every map: rain or snow; empty leaves the map as it is"},
    {"g_lagHaxMs", "milliseconds of lag the server makes up for when it checks a shot"},
    {"g_lagHaxHistory", "frames of player positions the server keeps to make up for lag"},
    {"g_arenasFile", "file the list of arenas is read from; empty for scripts/arenas.txt"},
    {"g_botsFile", "file the list of bots is read from; empty for scripts/bots.txt"},
    {"g_gameState", "set by the game: where the match is: PRE_GAME, COUNT_DOWN, IN_PROGRESS"},
    {"g_levelStartTime", "set by the game: the clock time the level started at"},
    {"g_restarted", "set by the game: 1 while the match restarts, so a restart is not taken for a new match"},
    {"g_version", "set by the game: the version of the game module"},
    {"g_spSkill", "skill of the bots in a single player match"},
    {"g_spScores1", "set by the game: the single player scores at skill 1"},
    {"g_spScores2", "set by the game: the single player scores at skill 2"},
    {"g_spScores3", "set by the game: the single player scores at skill 3"},
    {"g_spScores4", "set by the game: the single player scores at skill 4"},
    {"g_spScores5", "set by the game: the single player scores at skill 5"},
    {"g_spAwards", "set by the game: the medals won in single player"},
    {"g_spVideos", "set by the game: the single player videos already seen"},
    {"g_autoAction", "records a demo of every match on the server (no effect in this build)"},
    {"g_accuracyFlags", "which shots count toward accuracy (no effect in this build)"},
    {"g_forceSendConfigstring", "sends every configstring again, even the ones that did not change (no effect in this build)"},
    {"g_forceSmallScoreboardMessage", "sends the scores in the short form (no effect in this build)"},
    {"g_enableDebugTrace", "prints the traces the game makes (no effect in this build)"},
    {"g_debugAlloc", "prints every block of memory the game takes"},
    {"g_debugDamage", "prints the damage of every hit"},
    {"g_debugMove", "prints what the movement code does"},
    {"g_debugInactivity", "prints how long each player has been still"},
    {"g_debugVampiricDamage", "prints the health leeched from each hit"},
    {"g_debugFlags", "debugging flags the server sends the clients"},

    //
    // ui: more
    //
    {"ui_menuFiles", "file that lists the menus to load"},
    {"ui_netSource", "server list the browser shows\n"
                     HV("0") "Local network\n"
                     HV("2") "Internet\n"
                     HV("3") "Favorites"},
    {"ui_netGametype", "game type the server browser filters by, as its place in the menu"},
    {"ui_actualNetGametype", "game type the server browser filters by, as the game's number for it"},
    {"ui_joinGametype", "game type chosen in the join menu"},
    {"ui_currentNetMap", "map selected in the server browser"},
    {"ui_findPlayer", "name the find player search looks for"},
    {"ui_favoriteName", "name of the server the menu is adding to the favorites"},
    {"ui_favoriteAddress", "address of the server the menu is adding to the favorites"},
    {"ui_serverStatusTimeOut", "milliseconds a server has to answer the status request before it counts as down"},
    {"ui_lastServerRefresh_0", "when the local network list was last refreshed"},
    {"ui_lastServerRefresh_1", "when the second server list was last refreshed"},
    {"ui_lastServerRefresh_2", "when the internet list was last refreshed"},
    {"ui_lastServerRefresh_3", "when the favorites list was last refreshed"},
    {"ui_gametype", "game type chosen for a server started from the menu"},
    {"ui_currentMap", "map selected for a server started from the menu"},
    {"ui_mapIndex", "place of the selected map in the start server menu"},
    {"ui_dedicated", "how a server started from the menu runs\n"
                     HV("0") "Played here\n"
                     HV("1") "Dedicated, local network\n"
                     HV("2") "Dedicated, internet"},
    {"ui_fragLimit", "frag limit of a server started from the menu"},
    {"ui_captureLimit", "capture limit of a server started from the menu"},
    {"ui_doWarmup", "a server started from the menu has a warmup"},
    {"ui_warmup", "seconds of warmup on a server started from the menu"},
    {"ui_friendlyFire", "friendly fire on a server started from the menu"},
    {"ui_pure", "a server started from the menu only allows files the server has"},
    {"ui_saveFragLimit", "set by the game: the frag limit to put back after a single player match"},
    {"ui_saveCaptureLimit", "set by the game: the capture limit to put back after a single player match"},
    {"ui_redteam", "team picked to play red on a server started from the menu"},
    {"ui_blueteam", "team picked to play blue on a server started from the menu"},
    {"ui_teamName", "your team in the single player menus"},
    {"ui_opponentName", "the opposing team in the single player menus"},
    {"ui_opponentModel", "the opponent's model in the single player menus"},
    {"ui_recordSPDemo", "records a demo of every single player match"},
    {"ui_recordSPDemoName", "set by the game: the name of the single player demo being recorded"},
    {"ui_cvGameType", "game type chosen in the call vote menu; -1 for none"},
    {"ui_announcer", "set by the game: the announcer voice picked in the menu, after its sample plays"},
    {"ui_teamHeadColor", "the head colour picked for teammates in the menu"},
    {"ui_teamUpperColor", "the upper body colour picked for teammates in the menu"},
    {"ui_teamLowerColor", "the legs colour picked for teammates in the menu"},
    {"ui_enemyHeadColor", "the head colour picked for enemies in the menu"},
    {"ui_enemyUpperColor", "the upper body colour picked for enemies in the menu"},
    {"ui_enemyLowerColor", "the legs colour picked for enemies in the menu"},
    {"ui_forceTeamModel", "the teammate model picked in the options menu"},
    {"ui_forceTeamSkin", "the teammate skin picked in the options menu"},
    {"ui_forceEnemyModel", "the enemy model picked in the options menu"},
    {"ui_forceEnemySkin", "the enemy skin picked in the options menu"},
    {"ui_forceTeamModelBright", "set by the game: 1 when the teammate model picked is a bright one"},
    {"ui_forceEnemyModelBright", "set by the game: 1 when the enemy model picked is a bright one"},
    {"ui_screenDamage", "the options menu's screen damage setting"},
    {"ui_screenDamage_Team", "the options menu's screen damage from teammates setting"},
    {"ui_bloomPreset", "the options menu's bloom preset"},
    {"ui_smallFont", "size of the menus' small text"},
    {"ui_bigFont", "size of the menus' big text"},
    {"ui_version", "set by the game: the version of the menus module"},
    {"ui_videomode", "set by the game: the screen size the game runs at"},
    {"ui_mainmenu", "set by the game: 1 while the main menu is up"},
    {"ui_intermission", "set by the game: 1 while a match is over and shows its scores"},
    {"ui_voteactive", "set by the game: 1 while a vote is going on"},
    {"ui_votestring", "set by the game: what the vote going on is about"},
    {"ui_priv", "set by the game: your privileges on the server, which decide the admin options the menus show"},
    {"ui_singlePlayerActive", "set by the game: 1 while you play a server started here"},
    {"ui_matchStartTime", "set by the game: when the single player match started"},
    {"ui_mapVotingDisabled", "set by the game: 1 when the server does not allow voting for a map"},
    {"ui_gameTypeVotingDisabled", "set by the game: 1 when the server does not allow voting for a game type"},
    {"ui_endMapVotingDisabled", "set by the game: 1 when there is no map vote at the end of a match"},
    {"ui_spSelection", "set by the game: the single player match picked"},
    {"ui_scoreAccuracy", "set by the game: on the single player score sheet, your accuracy"},
    {"ui_scoreImpressives", "set by the game: on the single player score sheet, your impressive medals"},
    {"ui_scoreExcellents", "set by the game: on the single player score sheet, your excellent medals"},
    {"ui_scoreDefends", "set by the game: on the single player score sheet, your defence medals"},
    {"ui_scoreAssists", "set by the game: on the single player score sheet, your assist medals"},
    {"ui_scoreGauntlets", "set by the game: on the single player score sheet, your gauntlet frags"},
    {"ui_scoreScore", "set by the game: on the single player score sheet, your score"},
    {"ui_scorePerfect", "set by the game: on the single player score sheet, your perfect medals"},
    {"ui_scoreTeam", "set by the game: on the single player score sheet, the team score"},
    {"ui_scoreBase", "set by the game: on the single player score sheet, the base score"},
    {"ui_scoreTimeBonus", "set by the game: on the single player score sheet, the time bonus"},
    {"ui_scoreSkillBonus", "set by the game: on the single player score sheet, the skill bonus"},
    {"ui_scoreShutoutBonus", "set by the game: on the single player score sheet, the shutout bonus"},
    {"ui_scoreTime", "set by the game: on the single player score sheet, the time played"},
    {"ui_scoreCaptures", "set by the game: on the single player score sheet, your captures"},
    {"ui_scoreAccuracy2", "set by the game: on the single player score sheet, the second player's accuracy"},
    {"ui_scoreImpressives2", "set by the game: on the single player score sheet, the second player's impressive medals"},
    {"ui_scoreExcellents2", "set by the game: on the single player score sheet, the second player's excellent medals"},
    {"ui_scoreDefends2", "set by the game: on the single player score sheet, the second player's defence medals"},
    {"ui_scoreAssists2", "set by the game: on the single player score sheet, the second player's assist medals"},
    {"ui_scoreGauntlets2", "set by the game: on the single player score sheet, the second player's gauntlet frags"},
    {"ui_scoreScore2", "set by the game: on the single player score sheet, the second player's score"},
    {"ui_scorePerfect2", "set by the game: on the single player score sheet, the second player's perfect medals"},
    {"ui_scoreTeam2", "set by the game: on the single player score sheet, the second player's team score"},
    {"ui_scoreBase2", "set by the game: on the single player score sheet, the second player's base score"},
    {"ui_scoreTimeBonus2", "set by the game: on the single player score sheet, the second player's time bonus"},
    {"ui_scoreSkillBonus2", "set by the game: on the single player score sheet, the second player's skill bonus"},
    {"ui_scoreShutoutBonus2", "set by the game: on the single player score sheet, the second player's shutout bonus"},
    {"ui_scoreTime2", "set by the game: on the single player score sheet, the second player's time played"},
    {"ui_scoreCaptures2", "set by the game: on the single player score sheet, the second player's captures"},
    {"ui_debug", "debugging output of the menus (no effect in this build)"},
    {"ui_initialized", "set by the game: 1 once the menus have loaded (no effect in this build)"},
    {"ui_new", "kept from Quake 3 (no effect in this build)"},
    {"ui_cdkeychecked", "kept from Quake 3, where it marked a checked CD key (no effect in this build)"},
    {"ui_gibs", "kept from the options menu (no effect in this build)"},
    {"ui_currentOpponent", "opponent of the single player tier (no effect in this build)"},
    {"ui_currentTier", "single player tier (no effect in this build)"},
    {"ui_teamColor", "kept from the options menu (no effect in this build)"},
    {"ui_enemyColor", "kept from the options menu (no effect in this build)"},
    {"ui_ffa_fraglimit", "frag limit the menu starts free for all servers with (no effect in this build)"},
    {"ui_ffa_timelimit", "time limit the menu starts free for all servers with (no effect in this build)"},
    {"ui_tourney_fraglimit", "frag limit the menu starts duel servers with (no effect in this build)"},
    {"ui_tourney_timelimit", "time limit the menu starts duel servers with (no effect in this build)"},
    {"ui_team_fraglimit", "frag limit the menu starts team servers with (no effect in this build)"},
    {"ui_team_timelimit", "time limit the menu starts team servers with (no effect in this build)"},
    {"ui_team_friendly", "friendly fire the menu starts team servers with (no effect in this build)"},
    {"ui_ctf_capturelimit", "capture limit the menu starts capture the flag servers with (no effect in this build)"},
    {"ui_ctf_timelimit", "time limit the menu starts capture the flag servers with (no effect in this build)"},
    {"ui_ctf_friendly", "friendly fire the menu starts capture the flag servers with (no effect in this build)"},
    {"ui_redteam1", "bot in red team slot 1 of the start server menu (no effect in this build)"},
    {"ui_redteam2", "bot in red team slot 2 of the start server menu (no effect in this build)"},
    {"ui_redteam3", "bot in red team slot 3 of the start server menu (no effect in this build)"},
    {"ui_redteam4", "bot in red team slot 4 of the start server menu (no effect in this build)"},
    {"ui_redteam5", "bot in red team slot 5 of the start server menu (no effect in this build)"},
    {"ui_blueteam1", "bot in blue team slot 1 of the start server menu (no effect in this build)"},
    {"ui_blueteam2", "bot in blue team slot 2 of the start server menu (no effect in this build)"},
    {"ui_blueteam3", "bot in blue team slot 3 of the start server menu (no effect in this build)"},
    {"ui_blueteam4", "bot in blue team slot 4 of the start server menu (no effect in this build)"},
    {"ui_blueteam5", "bot in blue team slot 5 of the start server menu (no effect in this build)"},

    //
    // cgame: more
    //
    {"cg_animateChatBalloon", "animates the balloon over a player who is typing"},
    {"ch_animateRewardIcons", "animates the medals when you earn one\n"
                              HV("0") "Still\n"
                              HV("1") "Plays once and stops on the last frame\n"
                              HV("2") "Loops"},
    {"ch_animateScroll", "the chat and the item pickups fade in, slide as lines come and go, and fade out"},
    {"ch_chatLines", "lines of chat shown with " HC("ch_animateScroll")},
    {"cg_zoomAnimationTime", "milliseconds the zoom takes to go in or out, up to 500"},
    {"cg_drawBrightWeapons", "draws weapons as flat, bright shapes; add the values\n"
                             HV("1") "Your own\n"
                             HV("2") "Teammates'\n"
                             HV("4") "Enemies'\n"
                             HV("8") "The ones lying on the map\n"
                             HV("16") "Everyone's while you spectate with the free camera"},
    {"cg_filter_angles", "smooths the view by averaging its angles over this many frames, up to 31; 0 for none"},
    {"cg_lightningImpactCap", "closer than this distance the lightning gun's impact gets smaller; 0 keeps it whole"},
    {"cg_railReloadTime", "milliseconds the railgun in your hand takes to light up again after a shot"},
    {"cg_skipItems", "hides the items lying on the map; their timers stay"},
    {"cg_smoothClients", "shows the other players where the server extrapolates them, smoother on a bad connection"},
    {"cg_lastmsg", "the last chat message, which the chat recall shows again"},
    {"cg_redTeamName", "name the scoreboards show for the red team; empty shows Red Team"},
    {"cg_blueTeamName", "name the scoreboards show for the blue team; empty shows Blue Team"},
    {"cg_selectedPlayer", "the teammate the team orders go to"},
    {"cg_selectedPlayerName", "name of the teammate the team orders go to"},
    {"cg_currentSelectedPlayer", "set by the game: the teammate selected in the team orders menu"},
    {"cg_currentSelectedPlayerName", "set by the game: the name of the teammate selected in the team orders menu (no effect in this build)"},
    {"cg_weaponPrimaryQueued", "starting weapon you chose for your next spawn, by its short name: mg, sg, gl, rl, lg, rg, pg, hmg..."},
    {"cg_playVoiceChats", "plays the sound of voice chats"},
    {"cg_showVoiceText", "prints the text of voice chats"},
    {"cg_armorTiered", "set by the game: 1 when the server uses tiered armor, where each armor protects differently"},
    {"cg_gametype", "set by the game: the game type of the server"},
    {"cg_loadout", "set by the game: 1 when the server lets you choose your starting weapon"},
    {"cg_weaponPrimary", "set by the game: the starting weapon the server gave you"},
    {"cg_spectating", "set by the game: 1 while you spectate"},
    {"cg_playerTeam", "set by the game: the team your view was last on"},
    {"cg_ignoreMouseInput", "set by the game: 1 while the mouse moves nothing in the game, as in the tutorial"},
    {"cg_version", "set by the game: the version of the game module"},
    {"cg_latchedHookOffset", "set by the game (no effect in this build)"},
    {"cg_disableLoadout_g", "set by the game: 1 when the server takes the gauntlet out of the starting weapons you can choose"},
    {"cg_disableLoadout_mg", "set by the game: 1 when the server takes the machinegun out of the starting weapons you can choose"},
    {"cg_disableLoadout_sg", "set by the game: 1 when the server takes the shotgun out of the starting weapons you can choose"},
    {"cg_disableLoadout_gl", "set by the game: 1 when the server takes the grenade launcher out of the starting weapons you can choose"},
    {"cg_disableLoadout_rl", "set by the game: 1 when the server takes the rocket launcher out of the starting weapons you can choose"},
    {"cg_disableLoadout_lg", "set by the game: 1 when the server takes the lightning gun out of the starting weapons you can choose"},
    {"cg_disableLoadout_rg", "set by the game: 1 when the server takes the railgun out of the starting weapons you can choose"},
    {"cg_disableLoadout_pg", "set by the game: 1 when the server takes the plasma gun out of the starting weapons you can choose"},
    {"cg_disableLoadout_bfg", "set by the game: 1 when the server takes the BFG out of the starting weapons you can choose"},
    {"cg_disableLoadout_gh", "set by the game: 1 when the server takes the grappling hook out of the starting weapons you can choose"},
    {"cg_disableLoadout_ng", "set by the game: 1 when the server takes the nailgun out of the starting weapons you can choose"},
    {"cg_disableLoadout_pl", "set by the game: 1 when the server takes the proximity launcher out of the starting weapons you can choose"},
    {"cg_disableLoadout_cg", "set by the game: 1 when the server takes the chaingun out of the starting weapons you can choose"},
    {"cg_disableLoadout_hmg", "set by the game: 1 when the server takes the heavy machinegun out of the starting weapons you can choose"},
    {"cg_gameInfo1", "set by the game (no effect in this build)"},
    {"cg_gameInfo2", "set by the game (no effect in this build)"},
    {"cg_gameInfo3", "set by the game (no effect in this build)"},
    {"cg_gameInfo4", "set by the game (no effect in this build)"},
    {"cg_gameInfo5", "set by the game (no effect in this build)"},
    {"cg_gameInfo6", "set by the game (no effect in this build)"},
    {"cg_gibCount", "spheres a player bursts into when gibbed; 0 for none"},
    {"cg_gibTime", "milliseconds a gib sphere lasts"},
    {"cg_gibVelocity", "speed of the gib spheres"},
    {"cg_gibVelocityRandomness", "random speed added to each gib sphere"},
    {"cg_gibDirScale", "how much the spheres follow the direction of the killing shot, 0 to 1"},
    {"cg_gibJump", "extra upward speed of the gib spheres"},
    {"cg_gibRandomness", "random sideways speed of the gib spheres"},
    {"cg_gibRandomnessZ", "random upward speed of the gib spheres"},
    {"cg_gibOriginOffset", "how far from the body the gib spheres start, sideways"},
    {"cg_gibOriginOffsetZ", "how far from the body the gib spheres start, up and down"},
    {"cg_gibGravity", "gravity on the gib spheres; 0 lets them fly straight"},
    {"cg_gibBounceFactor", "how much the gib spheres bounce, 0 to 1"},
    {"cg_gibColor", "colour of the gib spheres, as 0xRRGGBB; empty for the default"},
    {"cg_gibSparksColor", "colour of the sparks the gib spheres leave, as 0xRRGGBB; empty for the default"},
    {"cg_gibSparksHighlight", "draws the gib sparks with the brighter tracer"},
    {"cg_gibSparksSize", "size of the gib sparks"},
    {"cg_gibStepTime", "milliseconds between the sparks a gib sphere leaves"},
    {"cg_gibFloatingVelocity", "speed at which the floating sparks rise from the body"},
    {"cg_gibFloatingRandomness", "random speed added to the floating sparks"},
    {"cg_gibFloatingOriginOffset", "how far from the body the floating sparks start, sideways"},
    {"cg_gibFloatingOriginOffsetZ", "how far from the body the floating sparks start, up and down"},
    {"cg_stats", "prints the client frame number every frame"},
    {"cg_swingSpeed", "how fast player models turn their torso and legs toward where they move"},
    {"cg_ignore", "for debugging (no effect in this build)"},
    {"cg_debugFlags", "debugging flags (no effect in this build)"},
    {"cg_trackPlayer", "follows a player with the camera, for testing (no effect in this build)"},
    {"cg_cameraSmartMode", "camera test mode (no effect in this build)"},
    {"cg_cameraThirdPersonSmartMode", "third person camera test mode (no effect in this build)"},
    {"cg_announcerLastStanding", "the announcer tells the last player standing in the round modes (no effect in this build)"},
    {"cg_autoProjectileNudge", "sets the projectile nudge from your ping (no effect in this build)"},
    {"cg_projectileNudge", "milliseconds projectiles are drawn ahead to make up for your ping (no effect in this build)"},
    {"cg_forceDrawCrosshair", "keeps the crosshair drawn where it would be hidden (no effect in this build)"},
    {"cg_preferredStartingWeapons", "starting weapons you prefer when the server lets you choose (no effect in this build)"},
    {"cg_noTaunt", "turns off the players' taunts (no effect in this build)"},
    {"cg_oldPlasma", "the plasma gun's older look (no effect in this build)"},
    {"cg_oldRocket", "the rocket's older look (no effect in this build)"},
    {"cg_noProjectileTrail", "kept from Quake 3; the trails follow the cg_smokeRadius cvars (no effect in this build)"},
};

#define HELP_HASH_SIZE 1024

static int helpHash[HELP_HASH_SIZE];
static int helpNext[ARRAY_LEN(cvarHelp)];
static qboolean helpHashed;

static int Cvar_HelpHash(const char* name) {
    unsigned int hash = 0;

    while (*name) {
        hash = hash * 31 + (unsigned int)tolower((unsigned char)*name++);
    }
    return (int)(hash & (HELP_HASH_SIZE - 1));
}

/*
============
Cvar_BuiltInHelp

The help text of a cvar, NULL if it has none. The names are hashed on the
first call, so creating a cvar costs a lookup, not a pass over the table.
A numbered one takes the entry of its family, with a '#' in place of the number.
============
*/
static const char* Cvar_HelpLookup(const char* var_name) {
    int i;

    for (i = helpHash[Cvar_HelpHash(var_name)]; i != -1; i = helpNext[i]) {
        if (!Q_stricmp(cvarHelp[i].name, var_name)) {
            return cvarHelp[i].help;
        }
    }
    return NULL;
}

const char* Cvar_BuiltInHelp(const char* var_name) {
    const char* help;
    char family[64];
    int i, h, len;

    if (!helpHashed) {
        for (i = 0; i < HELP_HASH_SIZE; i++) {
            helpHash[i] = -1;
        }
        for (i = 0; i < (int)ARRAY_LEN(cvarHelp); i++) {
            h = Cvar_HelpHash(cvarHelp[i].name);
            helpNext[i] = helpHash[h];
            helpHash[h] = i;
        }
        helpHashed = qtrue;
    }

    help = Cvar_HelpLookup(var_name);
    if (help) {
        return help;
    }

    // the ones the game numbers, one per client or per slot: session0 takes the
    // entry of its family, written "session#"
    len = (int)strlen(var_name);
    if (len < 2 || len + 2 > (int)sizeof(family) || var_name[len - 1] < '0' || var_name[len - 1] > '9') {
        return NULL;
    }
    while (len > 1 && var_name[len - 1] >= '0' && var_name[len - 1] <= '9') {
        len--;
    }
    Com_Memcpy(family, var_name, len);
    family[len] = '#';
    family[len + 1] = '\0';
    return Cvar_HelpLookup(family);
}
