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
// g_training.c -- [QL] the Crash Course: the map's tour, and the match around it
//
// The Crash Course is qztraining with the "_training" factory (g_training 1). The
// tour is the map's: 47 info_tour_point entities chained by tourPointTargetName and
// tourPointTarget, each holding one line of Crash's script. Crash is the bot
// "Trainer" of scripts/bots.txt, and walking her from stop to stop is the bot's
// (AINode_Training, ai_dmnet.c). The cinematic it opens with is the player held in
// PM_TUTORIAL while bot_training is 1.
//
// This file is what the game does around that, from qagamex86.dll:
//   SP_info_tour_point      0x1005a1f0
//   its think               0x10059fd0
//   G_TrainingHumanClient   0x10024ea0
//   G_TrainingBotClient     0x10024ee0
//   G_TrainingForceCvar     0x10024f20
//   G_TrainingAddTrainer    0x100378e0
//   G_TrainingFrame         0x10024fa0, from BotAIStartFrame
//   Cmd_SkipTraining        0x10045a80, from readyup
//

#include "g_local.h"

// the tick's latches (0x100907dc, 0x100907d8, 0x100907d0, 0x100907c4): retail keeps
// them for the life of the module, which is one map
static qboolean trainingMusicStopped;
static qboolean trainingTrainerAdded;
static qboolean trainingLoopStarted;
static qboolean trainingEndMusicPlayed;

/*
===============
G_TrainingHumanClient

0x10024ea0: the first client in use, connected, and local -- the player the
training is for. Returns its client number or -1.
===============
*/
int G_TrainingHumanClient(void) {
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        gentity_t* ent = &g_entities[i];

        if (!ent->inuse || !ent->client) {
            continue;
        }
        if (ent->client->pers.connected != CON_CONNECTED || !ent->client->pers.localClient) {
            continue;
        }
        return ent->s.clientNum;
    }
    return -1;
}

/*
===============
G_TrainingBotClient

0x10024ee0: the first client in use, connected, and a bot -- Crash.
===============
*/
int G_TrainingBotClient(void) {
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        gentity_t* ent = &g_entities[i];

        if (!ent->inuse || !ent->client) {
            continue;
        }
        if (ent->client->pers.connected != CON_CONNECTED || !(ent->r.svFlags & SVF_BOT)) {
            continue;
        }
        return ent->s.clientNum;
    }
    return -1;
}

// 0x10024f20: sets a cvar only when it holds something else
static void G_TrainingForceCvar(const char* name, const char* value) {
    char current[1024];

    trap_Cvar_VariableStringBuffer(name, current, sizeof(current));
    if (strcmp(current, value)) {
        trap_Cvar_Set(name, value);
    }
}

// 0x100378e0: Crash comes in five seconds later, and the client loads her at once
static void G_TrainingAddTrainer(float skill) {
    G_AddBot("Trainer", skill, "", 5000, "");
    trap_SendServerCommand(-1, "loaddeferred\n");
}

/*
===============================================================================

THE TOUR POINTS

===============================================================================
*/

/*
===============
TourPoint_Think

0x10059fd0, a few frames after the level loads. A point with spawnflag 128 hides
what its box holds -- items, but not flags or persistant powerups -- and every item
it targets, until the tour gets there and uses them. Then the next point of the
chain is found once, so the bot does not have to look for it.
===============
*/
static void TourPoint_Think(gentity_t* self) {
    if (self->spawnflags & 128) {
        vec3_t mins, maxs;
        int touch[MAX_GENTITIES];
        gentity_t* ent;
        int num, i;

        VectorAdd(self->r.currentOrigin, self->r.mins, mins);
        VectorAdd(self->r.currentOrigin, self->r.maxs, maxs);
        num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);
        for (i = 0; i < num; i++) {
            ent = &g_entities[touch[i]];
            if (ent->s.eType != ET_ITEM || !ent->item) {
                continue;
            }
            if (ent->item->giType == IT_TEAM || ent->item->giType == IT_PERSISTANT_POWERUP) {
                continue;
            }
            ent->r.svFlags |= SVF_NOCLIENT;
            ent->s.eFlags |= EF_NODRAW;
            ent->r.contents = 0;
            trap_LinkEntity(ent);
        }

        ent = NULL;
        while ((ent = G_Find(ent, FOFS(targetname), self->target)) != NULL) {
            if (!ent->item) {
                continue;
            }
            ent->r.svFlags |= SVF_NOCLIENT;
            ent->s.eFlags |= EF_NODRAW;
            ent->r.contents = 0;
            trap_LinkEntity(ent);
        }
    }

    if (self->tourPointTarget) {
        self->tourNext = G_Find(NULL, FOFS(tourPointTargetName), self->tourPointTarget);
        if (!self->tourNext) {
            G_Printf("info_tour_point with no tourPointTarget at %s!\n", vtos(self->s.origin));
        }
    }
}

/*QUAKED info_tour_point (1 0.5 0) (-16 -16 -24) (16 16 32) START
[QL] One stop of a training map's tour. Only a training map keeps them.
"tourPointTargetName"  this stop's name; only the first stop (START) may lack it
"tourPointTarget"      the stop after it
"noise"                what the trainer says here
"message"              the line the player reads
"printChatTextTime"    how long the line stays up (2)
"wait"                 how long the stop lasts
"radius"               how near the player has to be (300)
"ignorePlayerLocation" the stop does not wait for the player
"cvar" "cvarValue"     a cvar set on the way (-1 sets nothing)
*/
void SP_info_tour_point(gentity_t* ent) {
    char* s;

    if (!level.mapIsTrainingMap) {
        G_FreeEntity(ent);
        return;
    }
    if (!ent->tourPointTargetName && !(ent->spawnflags & 1)) {
        G_Printf("info_tour_point with no tourPointTargetName at %s\n", vtos(ent->s.origin));
        G_FreeEntity(ent);
        return;
    }

    G_SpawnString("radius", "300", &s);
    ent->tourRadius = atof(s);
    G_SpawnString("cvarValue", "-1.0f", &s);
    ent->tourCvarValue = atof(s);
    G_SpawnString("printChatTextTime", "2.0f", &s);
    ent->tourChatTime = atof(s);
    G_SpawnString("ignorePlayerLocation", "0", &s);
    ent->tourIgnorePlayer = atoi(s);
    G_SpawnString("noise", "", &s);
    if (s[0]) {
        G_SoundIndex(s);
    }

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorClear(ent->s.pos.trDelta);
    ent->s.pos.trType = TR_STATIONARY;
    ent->s.pos.trTime = 0;
    ent->s.pos.trDuration = 0;
    VectorCopy(ent->s.origin, ent->r.currentOrigin);
    VectorSet(ent->r.mins, -16, -16, -24);
    VectorSet(ent->r.maxs, 16, 16, 32);

    // the think runs four server frames in, when every point has spawned
    ent->nextthink = level.time - 4 * (int)(-1000.0 / sv_fps.integer);
    ent->think = TourPoint_Think;
    trap_LinkEntity(ent);
}

/*
===============================================================================

THE TRAINING MATCH

===============================================================================
*/

/*
===============
Cmd_SkipTraining

0x10045a80, from readyup while g_skipTrainingEnable: the tour ends where it is and
the match starts over. Crash can pick things up again with a machinegun and 100
bullets, nobody is invulnerable any more, everyone respawns, and the player's chat
and sounds are cleared.
===============
*/
void Cmd_SkipTraining(int clientNum) {
    int i;

    trap_Cvar_Set("g_training", "1");
    trap_Cvar_Set("bot_training", "0");
    trap_Cvar_Set("bot_dynamicSkill", "1");
    trap_Cvar_Set("bot_followMe", "0");
    trap_Cvar_Set("bot_gauntlet", "0");
    trap_Cvar_Set("g_skipTrainingEnable", "0");

    for (i = 0; i < level.maxclients; i++) {
        gentity_t* ent = &g_entities[i];

        if (level.clients[i].pers.connected != CON_CONNECTED) {
            continue;
        }
        if (ent->r.svFlags & SVF_BOT) {
            ent->flags &= ~FL_NOPICKUP;
            ent->client->ps.ammo[WP_MACHINEGUN] = 100;
            ent->client->ps.stats[STAT_WEAPONS] |= 1 << WP_MACHINEGUN;
        }
        ent->flags &= ~(FL_TRAINER | FL_GODMODE);
        ClientSpawn(ent);
    }

    trap_SendServerCommand(clientNum, "clearChat");
    trap_SendServerCommand(clientNum, "clearSounds");
}

/*
===============
G_TrainingFrame

0x10024fa0, at the end of every BotAIStartFrame while g_training is set.

Away from a training map it switches the training off; with nobody to train it
holds the match at retail's settings. Otherwise the music stops for the warmup,
Crash is added once the player is on a team, bot_training 3 starts the arena's loop
and the intro's g_training 2 becomes 1 when the warmup is over. Until the player
readies up, bot_training 1 holds them in the cinematic; readying up ends the tour.
===============
*/
void G_TrainingFrame(void) {
    int human, bot;
    gclient_t* client;

    if (!g_training.integer) {
        return;
    }
    if (!level.mapIsTrainingMap) {
        G_TrainingForceCvar("g_training", "0");
        G_TrainingForceCvar("bot_gauntlet", "0");
        G_TrainingForceCvar("bot_training", "0");
        G_TrainingForceCvar("bot_dynamicSkill", "1");
        return;
    }

    human = G_TrainingHumanClient();
    if (human == -1) {
        G_TrainingForceCvar("bot_training", "1");
        G_TrainingForceCvar("bot_followDist", "125");
        G_TrainingForceCvar("g_doWarmup", "1");
        G_TrainingForceCvar("g_warmup", "10");
        G_TrainingForceCvar("timelimit", "10");
        G_TrainingForceCvar("fraglimit", "10");
        G_TrainingForceCvar("bot_gauntlet", "1");
        G_TrainingForceCvar("bot_dynamicSkill", "1");
        return;
    }
    client = g_entities[human].client;

    if (!trainingMusicStopped && level.warmupTime) {
        trap_SendServerCommand(human, "stopMusic");
        trainingMusicStopped = qtrue;
    }

    if (!BotNumBots() && !trainingTrainerAdded && client->pers.connected == CON_CONNECTED &&
        client->sess.sessionTeam != TEAM_SPECTATOR) {
        float skill;

        if (bot_startingSkill.integer < 2) {
            skill = 1.0f;
        } else if (bot_startingSkill.integer == 2) {
            skill = 3.0f;
        } else {
            skill = 4.0f;
        }
        trap_Cvar_Set("g_spSkill", va("%f", skill));
        G_TrainingAddTrainer(skill);
        trainingTrainerAdded = qtrue;
    }

    if (bot_training.integer == 3 && !trainingLoopStarted) {
        trap_SendServerCommand(human, "playMusic music/fla22k_01_loop");
        trainingLoopStarted = qtrue;
    }

    if (g_training.integer != 2 || !level.warmupTime) {
        if (g_training.integer == 2) {
            trap_Cvar_Set("g_training", "1");
        }
        // a Crash below skill 3 leaves the powerups alone
        if (!level.warmupTime) {
            bot = G_TrainingBotClient();
            if (bot != -1 && BotGetSkill(bot) < 3.0f) {
                g_entities[bot].flags |= FL_NOPOWERUP;
            }
        }
    }

    if (client->pers.ready) {
        bot = G_TrainingBotClient();
        if (bot != -1) {
            g_entities[bot].flags &= ~FL_NOPICKUP;
            g_entities[bot].flags |= FL_NOPOWERUP;
        }
        trap_Cvar_Set("bot_training", "0");
        trap_Cvar_Set("bot_dynamicSkill", "1");
        trap_Cvar_Set("bot_followMe", "0");
        client->freezePlayer = qfalse;
    } else if (bot_training.integer == 1) {
        client->freezePlayer = qtrue;
    } else {
        bot = G_TrainingBotClient();
        if (bot != -1) {
            float skill = trap_Cvar_VariableValue("g_spSkill");

            if (skill < 1.0f) {
                skill = 1.0f;
            } else if (skill > 5.0f) {
                skill = 5.0f;
            }
            if (skill > BotGetSkill(bot)) {
                BotSetSkill(bot, skill);
                trap_Cvar_Set("bot_startingSkill", va("%f", skill));
            }
        }
        client->freezePlayer = qfalse;
    }

    if (level.intermissionTime && !trainingEndMusicPlayed) {
        trainingEndMusicPlayed = qtrue;
        trap_SendServerCommand(human, human == level.sortedClients[0] ? "playMusic music/win" : "playMusic music/loss");
    }
}

/*
===============================================================================

CRASH IN SPANISH (g_trainingSpanish)

[ioquakelive] Not retail's: a switch for testing the tour in Spanish. Her voice
stays the original, there is no Spanish audio.

The map's lines are keyed by the sound file that goes with them, not by their text:
the text in qztraining.bsp carries Windows-1252 curly quotes, and matching those
from source would be a matter of luck. The wav names are plain ASCII, one per line.

===============================================================================
*/

typedef struct {
    const char* key;  // the line's sound file, or the English text for the bot's own lines
    const char* es;
} trainingLine_t;

static const trainingLine_t trainingSpanish[] = {
    // the tour, in order (info_tour_point message + noise)
    {"crash_new/01_02.wav", "Hola, bienvenido al juego."},
    {"crash_new/02_02.wav", "Me llamo Crash."},
    {"crash_new/03_03.wav",
     "Estoy aqui para darte algunas indicaciones y de paso ver cuanta experiencia tienes."},
    {"crash_new/04_04.wav", "En QUAKE LIVE hay jugadores de todos los niveles."},
    {"crash_new/07a_02.wav", "Esto toma unos 10 minutos nada mas."},
    {"crash_new/06_05.wav", "Sigueme y empecemos."},
    {"crash_new/08_08.wav", "Este pasillo es una serie de desafios cortos."},
    {"crash_new/09_01.wav", "La idea es llegar lo mas lejos que puedas."},
    {"crash_new/10_06.wav", "En cuanto no puedas seguir, cruza el portal mas cercano."},
    {"crash_new/11_06.wav", "Nos vemos en la arena."},
    {"crash_new/12_01.wav", "Excelente, lo lograste."},
    {"crash_new/13_03.wav", "Esta es una de tantas arenas de combate; resulta que es mi favorita."},
    {"crash_new/14_02.wav",
     "Venimos a jugar una partida corta, pero primero quiero mostrarte esto y darte un par de consejos."},
    {"crash_new/15_03.wav",
     "Si ya sabes lo que haces, aprieta F3 cuando quieras y saltamos directo a nuestra partida."},
    {"crash_new/16_07.wav", "Tu objetivo en QUAKE LIVE es simple: matar, fraguear a los enemigos."},
    {"crash_new/17_01.wav",
     "Cuando apareces en la arena se le dice spawnear, y partes solo con la ametralladora."},
    {"crash_new/18_02.wav",
     "Pero hay armas mas potentes, armadura y salud repartidas por toda la arena."},
    {"crash_new/19_03.wav",
     "Tienes que juntar ese equipo para tener mas opciones de fraguear y de seguir vivo."},
    {"crash_new/20_06.wav",
     "Cuando te fraguean pierdes tu equipo, pero vuelves a spawnear al tiro con la salud llena."},
    {"crash_new/21_05.wav", "Acuerdate siempre: spawnear, equiparse, pelear y reponer."},
    {"crash_new/22_03.wav", "Sigueme y te muestro la arena antes de que peleemos."},
    {"crash_new/23_01.wav", "Los objetos se recogen con solo pasar por encima."},
    {"crash_new/24_02.wav",
     "Cuando recoges un objeto desaparece unos segundos antes de volver a estar disponible."},
    {"crash_new/25_02.wav", "Este es el lanzacohetes: una de las armas mas populares de QUAKE LIVE."},
    {"crash_new/26_01.wav", "Casi todas las armas te obligan a pegarle directo al rival."},
    {"crash_new/27_03.wav",
     "Pero los cohetes explotan al chocar con cualquier cosa y danan a todos los que esten cerca, tu incluido."},
    {"crash_new/28_01.wav", "Esta es la armadura roja."},
    {"crash_new/29_01.wav", "La armadura reduce la salud que pierdes cuando te pegan."},
    {"crash_new/30_01.wav", "Repon la municion de tus armas juntando las cajas de municion."},
    {"crash_new/31_03.wav", "Si tu salud baja de 100, junta globos de salud para recuperarte."},
    {"crash_new/32_01.wav", "Las plataformas de salto te impulsan rapido a las zonas altas."},
    {"crash_new/33_01.wav", "Esta es la escopeta: muy efectiva en combate cercano."},
    {"crash_new/34_01.wav", "Por lo general puedes caer o saltar desde mas alto de lo que crees sin hacerte dano."},
    {"crash_new/35_02.wav",
     "Los power-ups como esta megasalud dan ventajas temporales que se van con el tiempo."},
    {"crash_new/36_04.wav", "Listo, creo que estas preparado para una partida."},
    {"crash_new/37_02.wav",
     "Vamos a jugar un uno contra uno, asi que solo tienes que sobrevivir e intentar fraguearme."},
    {"crash_new/37b_07_alt.wav", "Quiero que lo pases bien."},
    {"crash_new/37c_05.wav", "Asi que capaz juegue mas facil o mas dificil para que este parejo."},
    {"crash_new/38_01.wav", "Y acuerdate: spawnear, equiparse, pelear y reponer."},
    {"crash_new/39_01.wav", "Suerte y pasalo bien."},

    // what the bot says on its own, by its English text
    {"This way! No time to waste!", "Por aca! No hay tiempo que perder!"},
    {"Let's continue on to the next stop in the tour! Follow me!",
     "Sigamos a la proxima parada del recorrido! Sigueme!"},
    {"Follow me to continue the tour! Lets go!", "Sigueme para continuar el recorrido! Vamos!"},
    {"You've got to be careful in the Arena!", "Tienes que tener cuidado en la Arena!"},
    {"Well done! Let's get back to the tutorial. Follow me.",
     "Bien hecho! Volvamos al tutorial. Sigueme."},
    {"Grab the flag! No time to waste!", "Agarra la bandera! No hay tiempo que perder!"},
    {"Exit the tutorial whenever you're ready to fight me!",
     "Sal del tutorial cuando quieras pelear conmigo!"},
    {"Grab this weapon! No time to waste!", "Agarra esta arma! No hay tiempo que perder!"},
    {"Grab this ammo! No time to waste!", "Agarra esta municion! No hay tiempo que perder!"},
    {"Grab this armor! No time to waste!", "Agarra esta armadura! No hay tiempo que perder!"},
    {"Grab this health! No time to waste!", "Agarra esta salud! No hay tiempo que perder!"},
    {"Grab this powerup! No time to waste!", "Agarra este power-up! No hay tiempo que perder!"},
    {"Grab this holdable! No time to waste!", "Agarra este objeto! No hay tiempo que perder!"},
    {"Grab this item! No time to waste!", "Agarra este objeto! No hay tiempo que perder!"},
    {"Oops! Um... I meant to do that! Just don't you do it, ok?",
     "Uy! Eh... lo hice a proposito! Pero tu no lo hagas, ok?"},
    {"Stop wasting your ammo! I'm here to help you out!",
     "Deja de gastar municion! Estoy aqui para ayudarte!"},
};

/*
===============
G_TrainingText

The Spanish line for a key, or the key itself when the switch is off or the line
is not in the table.
===============
*/
const char* G_TrainingText(const char* key) {
    int i;

    if (!key || !key[0] || !g_trainingSpanish.integer) {
        return key;
    }
    for (i = 0; i < (int)ARRAY_LEN(trainingSpanish); i++) {
        if (!Q_stricmp(trainingSpanish[i].key, key)) {
            return trainingSpanish[i].es;
        }
    }
    return key;
}

/*
===============
G_TrainingSay

A whole chat command ("say <line>" or "botSay <line>") with its line translated,
or the command untouched when the switch is off or the line is not in the table.
===============
*/
const char* G_TrainingSay(const char* command) {
    const char* space;
    const char* text;
    char verb[16];
    int len;

    if (!g_trainingSpanish.integer || !command) {
        return command;
    }
    space = strchr(command, ' ');
    if (!space) {
        return command;
    }
    len = (int)(space - command);
    if (len >= (int)sizeof(verb)) {
        return command;
    }
    memcpy(verb, command, len);
    verb[len] = '\0';
    text = G_TrainingText(space + 1);
    if (text == space + 1) {
        return command;
    }
    return va("%s %s", verb, text);
}

/*
===============
G_TrainingMessage

A tour stop's line: the map's text, or its Spanish translation, which is found by
the stop's sound rather than by its text.
===============
*/
const char* G_TrainingMessage(const gentity_t* point) {
    const char* sound;

    if (!g_trainingSpanish.integer || !point->noise || !point->noise[0]) {
        return point->message;
    }
    // the table's keys drop the "sound/vo/" the map writes in front
    sound = point->noise;
    if (!Q_stricmpn(sound, "sound/vo/", 9)) {
        sound += 9;
    }
    return G_TrainingText(sound);
}
