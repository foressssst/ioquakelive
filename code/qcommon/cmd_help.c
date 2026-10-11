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
#include "q_shared.h"
#include "qcommon.h"

typedef struct {
    const char* name;
    const char* help;
} cmdHelp_t;

#define HELD(what) "holds " what " while the bound key is down"
#define RELEASE(cmd) "releases " cmd

static const cmdHelp_t cmdHelp[] = {

    {"+forward", HELD("forward")},
    {"-forward", RELEASE("+forward")},
    {"+back", HELD("backward")},
    {"-back", RELEASE("+back")},
    {"+moveleft", HELD("strafe left")},
    {"-moveleft", RELEASE("+moveleft")},
    {"+moveright", HELD("strafe right")},
    {"-moveright", RELEASE("+moveright")},
    {"+moveup", HELD("jump")},
    {"-moveup", RELEASE("+moveup")},
    {"+movedown", HELD("crouch")},
    {"-movedown", RELEASE("+movedown")},
    {"+left", HELD("turn left")},
    {"-left", RELEASE("+left")},
    {"+right", HELD("turn right")},
    {"-right", RELEASE("+right")},
    {"+lookup", HELD("look up")},
    {"-lookup", RELEASE("+lookup")},
    {"+lookdown", HELD("look down")},
    {"-lookdown", RELEASE("+lookdown")},
    {"+speed", "holds the run/walk toggle while the bound key is down\n"
               "which of the two it selects depends on cl_run"},
    {"-speed", RELEASE("+speed")},
    {"+strafe", "turns left and right into strafing while the bound key is down"},
    {"-strafe", RELEASE("+strafe")},
    {"+mlook", "steers with the mouse while the bound key is down\n"
               "with cl_freelook on this is already the normal state"},
    {"-mlook", "releases +mlook, and recentres the view"},
    {"centerview", "looks straight ahead again"},

    {"+attack", HELD("fire")},
    {"-attack", RELEASE("+attack")},
    {"+button0", "holds fire while the bound key is down, same as +attack"},
    {"-button0", RELEASE("+button0")},
    {"+button1", "holds the chat button while the bound key is down"},
    {"-button1", RELEASE("+button1")},
    {"+button2", "holds use while the bound key is down"},
    {"-button2", RELEASE("+button2")},
    {"+button3", "holds the taunt gesture while the bound key is down"},
    {"-button3", RELEASE("+button3")},
    {"+button4", "holds the walk button while the bound key is down"},
    {"-button4", RELEASE("+button4")},
    {"+button5", "holds the affirmative button while the bound key is down"},
    {"-button5", RELEASE("+button5")},
    {"+button6", "holds the negative button while the bound key is down"},
    {"-button6", RELEASE("+button6")},
    {"+button7", "holds a spare game button while the bound key is down"},
    {"-button7", RELEASE("+button7")},
    {"+button8", "holds a spare game button while the bound key is down"},
    {"-button8", RELEASE("+button8")},
    {"+button9", "holds a spare game button while the bound key is down"},
    {"-button9", RELEASE("+button9")},
    {"+button10", "holds a spare game button while the bound key is down"},
    {"-button10", RELEASE("+button10")},
    {"+button11", "holds a spare game button while the bound key is down"},
    {"-button11", RELEASE("+button11")},
    {"+button12", "holds a spare game button while the bound key is down"},
    {"-button12", RELEASE("+button12")},
    {"+button13", "holds a spare game button while the bound key is down"},
    {"-button13", RELEASE("+button13")},
    {"+button14", "holds a spare game button while the bound key is down"},
    {"-button14", RELEASE("+button14")},

    {"+voice", "talks over voice chat while the bound key is down\n"
               "the voice travels over Steam, so it needs Steam running"},
    {"-voice", RELEASE("+voice")},

    {"ListInputDevices", "prints the game controllers SDL can see"},
    {"inputrate", "measures the real mouse rate and what servicing it costs\n"
                  "usage: inputrate [frames]\n"
                  "answers whether a 4000 or 8000 Hz mouse is costing this machine frames"},
    {"in_restart", "restarts the input system, picking up a changed device"},

    {"bind", "binds a key to a command\n"
             "usage: bind <key> [command]\n"
             "without a command it prints what that key is bound to"},
    {"unbind", "clears a key's binding\n"
               "usage: unbind <key>"},
    {"unbindall", "clears every key binding"},
    {"bindlist", "prints every key that has a binding"},

    {"connect", "joins a server\n"
                "usage: connect <address[:port]>"},
    {"reconnect", "rejoins the last server"},
    {"disconnect", "leaves the server"},
    {"cmd", "sends a command to the server as it is\n"
            "usage: cmd <command> [arguments]"},
    {"rcon", "runs a command on the server as its administrator\n"
             "usage: rcon <command>\n"
             "the password comes from rconpassword, and retail takes rcon over ZeroMQ"},
    {"say", "says something to everyone\n"
            "usage: say <text>"},
    {"sayto", "says something to one player\n"
              "usage: sayto <player> <text>"},
    {"model", "sets the player model\n"
              "usage: model <name>"},
    {"userinfo", "prints what this client tells the server about itself"},
    {"clientinfo", "prints what the client knows about its own state"},
    {"configstrings", "prints the configstrings the server has sent"},
    {"showip", "prints this machine's network addresses"},

    {"globalservers", "asks Steam for the server list\n"
                      "usage: globalservers <source> [protocol] [keywords]\n"
                      "the source is 0 internet, 1 LAN, 2 friends, 3 favourites, 4 history"},
    {"localservers", "looks for servers on this network"},
    {"friends", "prints the Steam friends list\n"
                "usage: friends [join|invite|profile <number>]\n"
                "the number is the one printed beside each friend"},
    {"clientfriendinvite", "opens Steam's add-friend window on a player in this match\n"
                           "usage: clientfriendinvite <client number>"},
    {"clientviewprofile", "opens a player's Steam profile\n"
                          "usage: clientviewprofile <client number>"},

    {"steamstatus", "prints what Steam is doing: the library, the session, VAC and the public IP"},
    {"steamservers", "prints the state of the server list request"},
    {"steam_downloadugc", "downloads a Workshop item by id\n"
                          "usage: steam_downloadugc <item id>"},
    {"steam_subscribeugc", "subscribes to a Workshop item\n"
                           "usage: steam_subscribeugc <item id>"},
    {"steam_unsubscribeugc", "unsubscribes from a Workshop item\n"
                             "usage: steam_unsubscribeugc <item id>"},

    {"record", "starts recording a demo\n"
               "usage: record [name]\n"
               "without a name it numbers one"},
    {"stoprecord", "stops recording"},
    {"demo", "plays a demo back\n"
             "usage: demo <name>"},
    {"rewind", "jumps back in the demo, 10 seconds unless told\n"
               "usage: rewind [seconds]"},
    {"fastforward", "jumps ahead in the demo, 10 seconds unless told\n"
                    "usage: fastforward [seconds]"},
    {"seekdemo", "jumps to a time counted from the start of the demo\n"
                 "usage: seekdemo <seconds> or <m:ss>"},

    {"map", "loads a map and starts a match\n"
            "usage: map <map> <factory>\n"
            "the factory is required, as it is in retail; map with no factory prints the list"},
    {"devmap", "loads a map with cheats on\n"
               "usage: devmap <map> <factory>"},
    {"map_restart", "restarts the current match"},
    {"killserver", "shuts the server down without leaving the game"},
    {"startRandomMap", "starts a map taken from the server's map pool"},
    {"reload_mappool", "rereads the map pool files"},
    {"reload_arenas", "rereads the arena definitions"},
    {"reload_factories", "rereads scripts/factories.txt"},
    {"arena", "loads an arena script\n"
              "usage: arena <name>, for scripts/<name>.sp_arena"},
    {"status", "prints who is on the server, with their score and ping"},
    {"serverinfo", "prints the server's own settings"},
    {"dumpuser", "prints what one player told the server about itself\n"
                 "usage: dumpuser <player>"},
    {"kick", "removes a player by name\n"
             "usage: kick <player>"},
    {"clientkick", "removes a player by client number\n"
                   "usage: clientkick <client number>"},
    {"sectorlist", "prints how the world is divided for collision, and what is in each part"},

    {"alias", "names a list of commands\n"
              "usage: alias <name> <commands>\n"
              "without commands it prints what that name runs"},
    {"unalias", "forgets one alias\n"
                "usage: unalias <name>"},
    {"unaliasall", "forgets every alias"},

    {"dir", "lists files in a directory of the game's filesystem\n"
            "usage: dir <directory> [extension]"},
    {"fdir", "lists files matching a pattern\n"
             "usage: fdir <pattern>, as in fdir *.cfg"},
    {"path", "prints the search path: the folders and pk3s, in the order they are read"},
    {"touchFile", "opens and closes a file, to see which pk3 answers for it\n"
                  "usage: touchFile <file>"},
    {"fs_openedList", "prints the files opened since the map started"},
    {"fs_referencedList", "prints the pk3s this match refers to"},
    {"writeconfig", "writes the settings to a config file\n"
                    "usage: writeconfig <name>"},
    {"writeClientConfig", "writes the client's own settings to a config file\n"
                          "usage: writeClientConfig <name>"},

    {"vid_restart", "restarts the renderer, applying video settings"},
    {"snd_restart", "restarts the sound system, reloading every sound"},
    {"net_restart", "restarts the network, picking up changed net settings"},
    {"postprocess_restart", "reloads the post-processing shaders"},

    {"screenshot", "saves a screenshot as TGA"},
    {"screenshotJPEG", "saves a screenshot as JPEG"},
    {"minimize", "minimises the window"},

    {"gfxinfo", "prints the video driver, the context and what it supports"},
    {"gfxmeminfo", "prints what the driver says about video memory"},
    {"modelist", "prints the video modes r_mode can name"},
    {"imagelist", "prints the loaded textures and what they take"},
    {"shaderlist", "prints the loaded shaders"},
    {"skinlist", "prints the loaded skins"},
    {"modellist", "prints the loaded models"},
    {"fontlist", "prints the loaded fonts and the ones a cvar can name"},
    {"exportCubemaps", "writes the map's cubemaps out as files"},

    {"play", "plays a sound file\n"
             "usage: play <file>"},
    {"music", "plays background music\n"
              "usage: music <file> [looping file]"},
    {"s_stop", "stops every sound that is playing"},
    {"s_list", "prints the loaded sounds"},
    {"s_info", "prints the sound device's format and buffer"},
    {"soundload", "prints where the time went while loading this map's sounds"},

    {"quit", "leaves the game"},
    {"meminfo", "prints how the hunk and the zone are being used"},
    {"hunklog", "writes the hunk's blocks to a log"},
    {"hunksmalllog", "writes the hunk's small blocks to a log"},
    {"zonelog", "writes the zone's blocks to a log"},
    {"setenv", "sets an environment variable for this process\n"
               "usage: setenv <name> [value], and with no value it clears it"},
    {"changeVectors", "prints how often each network field changes, for the delta tables"},
    {"openupdate", "opens the download page of the newer version found at startup, if there is one"},
    {"frametimes", "measures the real time between frames, in microseconds\n"
                   "usage: frametimes [frames]\n"
                   "this is what says whether something made the game smoother; com_speeds\n"
                   "measures each frame's work instead, and in whole milliseconds"},
    {"jobtest", "checks the worker threads: the same sum spread out and done in one go"},
    {"error", "raises an error on purpose, to test the handler\n"
              "usage: error <message>"},
    {"crash", "crashes on purpose, to test the handler"},
    {"freeze", "stops the engine for a while, to test the network\n"
               "usage: freeze <seconds>"},

    {"web_changeHash", "moves the embedded browser to another page\n"
                       "usage: web_changeHash <path>, and only while in a match"},
    {"web_hideBrowser", "closes the embedded browser"},

    {"+scores", HELD("the scoreboard up")},
    {"-scores", RELEASE("+scores")},
    {"+acc", HELD("the accuracy sheet up")},
    {"-acc", RELEASE("+acc")},
    {"+pstats", HELD("your match statistics up")},
    {"-pstats", RELEASE("+pstats")},
    {"+chat", HELD("the chat history up")},
    {"-chat", RELEASE("+chat")},
    {"+zoom", HELD("the view zoomed in")},
    {"-zoom", RELEASE("+zoom")},

    {"weapnext", "takes the next weapon you carry"},
    {"weapprev", "takes the weapon before the one you carry"},
    {"weapon", "takes a weapon by its number\n"
               "usage: weapon <1-15>, and several numbers pick the first you have"},
    {"dropweapon", "drops the weapon you are holding"},
    {"dropflag", "drops the flag you carry"},
    {"droppowerup", "drops the powerup you carry"},
    {"droprune", "drops the rune you carry"},

    {"sizeup", "makes the view bigger"},
    {"sizedown", "makes the view smaller"},
    {"viewpos", "prints where you stand and where you look"},
    {"setviewpos", "puts you somewhere else, with cheats on\n"
                   "usage: setviewpos <x> <y> <z> <yaw>"},
    {"startOrbit", "swings the camera around you, with developer on"},
    {"nextframe", "steps the test model on to its next frame"},
    {"prevframe", "steps the test model back a frame"},
    {"levelshot", "takes the picture the menus show of the map, with cheats on"},
    {"loaddeferred", "loads the player models that were left for later"},
    {"loadhud", "reads the HUD in cg_hudFiles again"},
    {"togglechathistory", "shows the chat history, or hides it"},
    {"print", "prints what it is given on your own console\n"
              "usage: print <text>"},
    {"fragforward", "in a demo, jumps to just before the next frag of the player you watch\n"
                    "usage: fragforward [seconds before it]"},
    {"vkinfo", "prints what the Vulkan renderer runs on"},

    {"say_team", "says something to your team\n"
                 "usage: say_team <message>"},
    {"tell", "says something to one player\n"
             "usage: tell <player> <message>"},
    {"tell_target", "says something to the player under your crosshair\n"
                    "usage: tell_target <message>"},
    {"tell_attacker", "says something to whoever hit you last\n"
                      "usage: tell_attacker <message>"},
    {"vtell_target", "plays a voice line for the player under your crosshair\n"
                     "usage: vtell_target <line>"},
    {"vtell_attacker", "plays a voice line for whoever hit you last\n"
                       "usage: vtell_attacker <line>"},
    {"tcmd", "orders the player under your crosshair about\n"
             "usage: tcmd <order>"},
    {"tauntTaunt", "taunts"},
    {"tauntPraise", "praises your team"},
    {"tauntKillInsult", "insults whoever you just killed"},
    {"tauntDeathInsult", "insults whoever just killed you"},
    {"tauntGauntlet", "crows about a gauntlet frag"},
    {"clientmute", "stops you hearing a player's voice chat\n"
                   "usage: clientmute <player>"},

    {"nextOrder", "moves on to the next order in the team menu"},
    {"confirmOrder", "takes the order you were given"},
    {"denyOrder", "turns the order you were given down"},
    {"nextTeamMember", "picks the next teammate in the team menu"},
    {"prevTeamMember", "picks the teammate before that one"},
    {"taskOffense", "tells your team you are going for the attack"},
    {"taskDefense", "tells your team you are defending"},
    {"taskPatrol", "tells your team you are on patrol"},
    {"taskCamp", "tells your team you are holding a spot"},
    {"taskFollow", "tells your team you are following them"},
    {"taskRetrieve", "tells your team you are going for the flag"},
    {"taskEscort", "tells your team you are escorting the carrier"},
    {"taskSuicide", "tells the player under your crosshair to kill themselves"},
    {"taskOwnFlag", "tells the player under your crosshair to get your own flag back"},
    {"setteamcolor", "sets the colour your teammates are drawn in\n"
                     "usage: setteamcolor <colour>"},
    {"setenemycolor", "sets the colour your enemies are drawn in\n"
                      "usage: setenemycolor <colour>"},

    {"team", "joins a team\n"
             "usage: team <r|b|f|s|a>, and with nothing it prints the one you are on"},
    {"spec", "watches the next player that matches\n"
             "usage: spec [stats|red|blue|powerup], and with nothing it watches the next one"},
    {"follow", "watches a player from the sidelines\n"
               "usage: follow <player>, and with nothing it stops"},
    {"readyup", "readies you up during warmup"},
    {"kill", "kills you"},
    {"forfeit", "gives the match up, where the server allows it"},
    {"ragequit", "leaves the server there and then"},
    {"callvote", "calls a vote\n"
                 "usage: callvote <what> [value]"},
    {"vote", "votes on the vote going on\n"
             "usage: vote <yes|no>"},
    {"players", "prints who is on the server and what they are up to"},
    {"timeout", "calls a time out, which pauses the match"},
    {"timein", "calls the time back in, with a countdown of five seconds"},
    {"give", "gives you what you ask for, with cheats on\n"
             "usage: give <all|weapons|ammo|health|armor|item>"},
    {"god", "nothing hurts you, with cheats on"},
    {"notarget", "the bots stop going for you, with cheats on"},
    {"noclip", "you walk through the map, with cheats on"},

    {"addbot", "adds a bot to the match\n"
               "usage: addbot <name> [skill 1-5] [team] [delay] [other name]"},
    {"allready", "readies everyone up and starts the match (referee)"},
    {"pause", "pauses the match for as long as it takes (referee)"},
    {"unpause", "puts the match back on (referee)"},
    {"abort", "gives the match up and goes back to warmup (referee)"},
    {"lock", "stops players joining a team (referee)\n"
             "usage: lock [team], and with nothing it locks both"},
    {"unlock", "lets players join a team again (referee)\n"
               "usage: unlock [team], and with nothing it opens both"},
    {"put", "moves a player to a team (referee)\n"
            "usage: put <player> <r|b|s|f>"},
    {"mute", "stops a player from chatting (referee)\n"
             "usage: mute <player>"},
    {"unmute", "lets a muted player chat again (referee)\n"
               "usage: unmute <player>"},
    {"tempban", "throws a player out for the rest of the match (referee)\n"
                "usage: tempban <player>"},
    {"ban", "bans a player from the server (referee)\n"
            "usage: ban <player>"},
    {"unban", "takes an account off the ban list (referee)\n"
              "usage: unban <account>"},
    {"listaccess", "prints the accounts that have access (referee)\n"
                   "usage: listaccess [page]"},
    {"opsay", "says something to everyone as the server (referee)\n"
              "usage: opsay <message>"},
    {"addscore", "adds points to a player's score (referee)\n"
                 "usage: addscore <player> <points>"},
    {"addteamscore", "adds points to a team's score (referee)\n"
                     "usage: addteamscore <r|b> <points>"},
    {"setmatchtime", "sets how long the match has been going (referee)\n"
                     "usage: setmatchtime <seconds>"},
    {"addadmin", "gives a player the run of the server (administrator)\n"
                 "usage: addadmin <player>"},
    {"addmod", "makes a player a referee (administrator)\n"
               "usage: addmod <player>"},
    {"demote", "takes a player's privileges away (administrator)\n"
               "usage: demote <player>"},
    {"reload_access", "reads the access file again and applies it to everyone playing"},
};

const char* Cmd_BuiltInHelp(const char* cmd_name) {
    int i;

    if (!cmd_name || !cmd_name[0]) {
        return NULL;
    }

    for (i = 0; i < ARRAY_LEN(cmdHelp); i++) {
        if (!Q_stricmp(cmdHelp[i].name, cmd_name)) {
            return cmdHelp[i].help;
        }
    }

    return NULL;
}
