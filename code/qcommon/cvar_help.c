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
// The console shows a cvar's help in "help <name>", in the panel under the
// console and next to completions. Retail Quake Live has no help texts: these
// say what each cvar does in this engine and its modules, as they were ported
// from retail. The range and the default come from the cvar itself and are
// printed with the text, so the texts don't repeat them.
//
// Cvar_Get looks the name up when it creates a cvar, which gives the modules'
// cvars their text as they register. A cvar with no entry has no help; read-only
// state the game keeps for its menus is left out on purpose.
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
    {"cg_weaponConfig", "commands run whenever you switch weapons"},
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
    {"cg_forceEnemySkin", "skin of " HC("cg_forceEnemyModel")},
    {"cg_forceTeamModel", "model every teammate is drawn with; empty for their own"},
    {"cg_forceTeamSkin", "skin of " HC("cg_forceTeamModel")},
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
    {"ch_crosshairPulse", "the crosshair jumps to twice its size and eases back\n"
                          HV("0") "Never\n"
                          HV("1") "When you pick up an item\n"
                          HV("2") "When you damage an enemy\n"
                          HV("3") "When you frag an enemy"},

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
    {"cl_mouseAccel", "mouse acceleration"},
    {"cl_mouseAccelStyle", "mouse acceleration curve\n"
                           HV("0") "Quake 3's\n"
                           HV("1") "A power curve starting after " HC("cl_mouseAccelOffset")},
    {"cl_mouseAccelOffset", "offset of the style 1 mouse acceleration"},
    {"m_pitch", "vertical mouse speed factor; negative inverts the mouse"},
    {"m_yaw", "horizontal mouse speed factor"},
    {"m_forward", "speed of forward movement with the mouse"},
    {"m_side", "speed of sideways movement with the mouse"},
    {"m_filter", "smooths the mouse by averaging two readings"},
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
    {"r_allowResize", "the window can be resized"},
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
    {"con_autochat", "text typed in the console without a slash is said in chat"},
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
============
*/
const char* Cvar_BuiltInHelp(const char* var_name) {
    int i, h;

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

    for (i = helpHash[Cvar_HelpHash(var_name)]; i != -1; i = helpNext[i]) {
        if (!Q_stricmp(cvarHelp[i].name, var_name)) {
            return cvarHelp[i].help;
        }
    }
    return NULL;
}
