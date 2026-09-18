/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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

#include "client.h"
#include "snd_codec.h"
#include "snd_local.h"
#include "snd_public.h"

cvar_t* s_volume;
cvar_t* s_muted;
cvar_t* s_musicVolume;
cvar_t* s_doppler;
cvar_t* s_announcerVolume;
cvar_t* s_voiceVolume;
cvar_t* s_voiceStep;
cvar_t* s_muteBackground;

extern cvar_t* s_mixahead;
extern cvar_t* s_mixPreStep;
extern cvar_t* s_pvs;
extern cvar_t* s_show;

static soundInterface_t si;

/*
=================
S_ValidateInterface
=================
*/
static qboolean S_ValidSoundInterface(soundInterface_t* si) {
    if (!si->Shutdown)
        return qfalse;
    if (!si->StartSound)
        return qfalse;
    if (!si->StartLocalSound)
        return qfalse;
    if (!si->StartBackgroundTrack)
        return qfalse;
    if (!si->StopBackgroundTrack)
        return qfalse;
    if (!si->RawSamples)
        return qfalse;
    if (!si->StopAllSounds)
        return qfalse;
    if (!si->ClearLoopingSounds)
        return qfalse;
    if (!si->AddLoopingSound)
        return qfalse;
    if (!si->AddRealLoopingSound)
        return qfalse;
    if (!si->StopLoopingSound)
        return qfalse;
    if (!si->Respatialize)
        return qfalse;
    if (!si->UpdateEntityPosition)
        return qfalse;
    if (!si->Update)
        return qfalse;
    if (!si->DisableSounds)
        return qfalse;
    if (!si->BeginRegistration)
        return qfalse;
    if (!si->RegisterSound)
        return qfalse;
    if (!si->ClearSoundBuffer)
        return qfalse;
    if (!si->SoundInfo)
        return qfalse;
    if (!si->SoundList)
        return qfalse;
    return qtrue;
}

/*
=================
S_StartSound
=================
*/
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx) {
    if (si.StartSound) {
        si.StartSound(origin, entnum, entchannel, sfx);
    }
}

/*
=================
S_StartLocalSound
=================
*/
void S_StartLocalSound(sfxHandle_t sfx, int channelNum) {
    if (si.StartLocalSound) {
        si.StartLocalSound(sfx, channelNum);
    }
}

/*
=================
S_StartLocalSoundVolume

[QL] quakelive_steam.exe 0x4da380, the cgame import at +0xa0 (0x4afe50): the local sound
path with the volume the channel starts at instead of the fixed 1. cgame's kill beep plays
through it with s_killBeepVolume.
=================
*/
void S_StartLocalSoundVolume(sfxHandle_t sfx, int channelNum, float volume) {
    if (si.StartLocalSoundVolume) {
        si.StartLocalSoundVolume(sfx, channelNum, volume);
    }
}

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack(const char* intro, const char* loop) {
    if (si.StartBackgroundTrack) {
        si.StartBackgroundTrack(intro, loop);
    }
}

/*
=================
S_StopBackgroundTrack
=================
*/
void S_StopBackgroundTrack(void) {
    if (si.StopBackgroundTrack) {
        si.StopBackgroundTrack();
    }
}

/*
=================
S_RawSamples
=================
*/
void S_RawSamples(int stream, int samples, int rate, int width, int channels, const byte* data, float volume, int entityNum) {
    if (si.RawSamples)
        si.RawSamples(stream, samples, rate, width, channels, data, volume, entityNum);
}

/*
=================
S_StopAllSounds
=================
*/
void S_StopAllSounds(void) {
    if (si.StopAllSounds) {
        si.StopAllSounds();
    }
}

/*
=================
S_ClearLoopingSounds
=================
*/
void S_ClearLoopingSounds(qboolean killall) {
    if (si.ClearLoopingSounds) {
        si.ClearLoopingSounds(killall);
    }
}

/*
=================
S_AddLoopingSound
=================
*/
void S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx) {
    if (si.AddLoopingSound) {
        si.AddLoopingSound(entityNum, origin, velocity, sfx);
    }
}

/*
=================
S_AddRealLoopingSound
=================
*/
void S_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx) {
    if (si.AddRealLoopingSound) {
        si.AddRealLoopingSound(entityNum, origin, velocity, sfx);
    }
}

/*
=================
S_StopLoopingSound
=================
*/
void S_StopLoopingSound(int entityNum) {
    if (si.StopLoopingSound) {
        si.StopLoopingSound(entityNum);
    }
}

/*
=================
S_Respatialize
=================
*/
void S_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater) {
    if (si.Respatialize) {
        si.Respatialize(entityNum, origin, axis, inwater);
    }
}

/*
=================
S_UpdateEntityPosition
=================
*/
void S_UpdateEntityPosition(int entityNum, const vec3_t origin) {
    if (si.UpdateEntityPosition) {
        si.UpdateEntityPosition(entityNum, origin);
    }
}

/*
=================
S_Update

[QL] quakelive_steam.exe creates its DirectSound buffer without global focus unless
s_muteBackground is 0, so the game goes silent while its window is not the active one.
=================
*/
void S_Update(void) {
    qboolean background = s_muteBackground->integer && (com_unfocused->integer || com_minimized->integer);

    if (s_muted->integer != background) {
        s_muted->integer = background;
        s_muted->modified = qtrue;
    }

    if (si.Update) {
        si.Update();
    }
}

/*
=================
S_DeviceLost

[ioquakelive] Windows le puede quitar el dispositivo de audio al juego: al cambiar de salida,
al conectar unos audifonos, o cuando otro programa lo toma en exclusiva. SDL cierra el
dispositivo y avisa, y hasta ahora nadie escuchaba ese aviso, asi que el sonido se iba y no
volvia hasta reiniciar el juego.

Primero se intenta lo barato, que es reabrir solo el dispositivo. Si el nuevo no tiene el mismo
formato, los sonidos cargados ya no sirven -- estan remuestreados al anterior -- y ahi si hace
falta el snd_restart entero.
=================
*/
void S_DeviceLost(void) {
    if (S_Base_ReopenDevice()) {
        Com_Printf("Audio device reopened.\n");
        return;
    }
    Com_Printf("The audio device changed format; restarting sound.\n");
    Cbuf_AddText("snd_restart\n");
}

/*
=================
S_DisableSounds
=================
*/
void S_DisableSounds(void) {
    if (si.DisableSounds) {
        si.DisableSounds();
    }
}

/*
=================
S_BeginRegistration
=================
*/
void S_BeginRegistration(void) {
    if (si.BeginRegistration) {
        si.BeginRegistration();
    }
}

/*
=================
S_RegisterSound
=================
*/
sfxHandle_t S_RegisterSound(const char* sample, qboolean compressed) {
    if (si.RegisterSound) {
        return si.RegisterSound(sample, compressed);
    } else {
        return 0;
    }
}

/*
=================
S_ClearSoundBuffer
=================
*/
void S_ClearSoundBuffer(void) {
    if (si.ClearSoundBuffer) {
        si.ClearSoundBuffer();
    }
}

/*
=================
S_SoundInfo
=================
*/
void S_SoundInfo(void) {
    if (si.SoundInfo) {
        si.SoundInfo();
    }
}

/*
=================
S_SoundList
=================
*/
void S_SoundList(void) {
    if (si.SoundList) {
        si.SoundList();
    }
}

/*
=================
S_Play_f

[QL] quakelive_steam.exe 0x4db710. A name without an extension gets ".wav" added, built from
the first argument whatever its position, as Quake III did.
=================
*/
void S_Play_f(void) {
    int i;
    sfxHandle_t h;
    char name[256];

    if (!si.RegisterSound || !si.StartLocalSound) {
        return;
    }

    for (i = 1; i < Cmd_Argc(); i++) {
        if (!strrchr(Cmd_Argv(i), '.')) {
            Com_sprintf(name, sizeof(name), "%s.wav", Cmd_Argv(1));
        } else {
            Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
        }
        h = si.RegisterSound(name, qfalse);
        if (h) {
            si.StartLocalSound(h, CHAN_LOCAL_SOUND);
        }
    }
}

/*
=================
S_Music_f

[QL] quakelive_steam.exe 0x4db810. With one argument the track plays once.
=================
*/
void S_Music_f(void) {
    int c;

    if (!si.StartBackgroundTrack) {
        return;
    }

    c = Cmd_Argc();

    if (c == 2) {
        si.StartBackgroundTrack(Cmd_Argv(1), Cmd_Argv(1));
        S_Base_ClearBackgroundLoop();
    } else if (c == 3) {
        si.StartBackgroundTrack(Cmd_Argv(1), Cmd_Argv(2));
    } else {
        Com_Printf("music <musicfile> [loopfile]\n");
    }
}

//=============================================================================

/*
=================
S_Init
=================
*/
void S_Init(void) {
    cvar_t* cv;

    // [QL] quakelive_steam.exe 0x4db870: every sound cvar exists even with sound disabled,
    // and the software mixer is the only one
    Com_Printf("\n------- sound initialization -------\n");

    s_announcerVolume = Cvar_Get("s_announcerVolume", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    Cvar_CheckRange(s_announcerVolume, 0.0f, 2.0f, qfalse);
    s_doppler = Cvar_Get("s_doppler", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    cv = Cvar_Get("s_initsound", "1", 0);
    s_mixahead = Cvar_Get("s_mixahead", "0.140", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    s_mixPreStep = Cvar_Get("s_mixPreStep", "0.05", CVAR_ARCHIVE | CVAR_REPLICATE);
    s_musicVolume = Cvar_Get("s_musicvolume", "0.5", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    Cvar_CheckRange(s_musicVolume, 0.0f, 2.0f, qfalse);
    s_pvs = Cvar_Get("s_pvs", "0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    Cvar_CheckRange(s_pvs, 0, 1, qtrue);
    s_voiceVolume = Cvar_Get("s_voiceVolume", "1.0", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    Cvar_CheckRange(s_voiceVolume, 0.0f, 2.0f, qfalse);
    s_voiceStep = Cvar_Get("s_voiceStep", "0.02", CVAR_ARCHIVE | CVAR_REPLICATE);
    s_show = Cvar_Get("s_show", "0", CVAR_CHEAT);
    s_testsound = Cvar_Get("s_testsound", "0", CVAR_CHEAT);
    s_volume = Cvar_Get("s_volume", "0.8", CVAR_ARCHIVE | CVAR_REPLICATE | CVAR_USERSAVE);
    Cvar_CheckRange(s_volume, 0.0f, 2.0f, qfalse);
    s_muteBackground = Cvar_Get("s_muteBackground", "1", CVAR_ARCHIVE | CVAR_LATCH);
    s_muted = Cvar_Get("s_muted", "0", CVAR_ROM);

    if (!cv->integer) {
        Com_Printf("not initializing.\n");
        Com_Printf("------------------------------------\n");
        return;
    }

    S_CodecInit();

    Cmd_AddCommand("play", S_Play_f);
    Cmd_AddCommand("music", S_Music_f);
    Cmd_AddCommand("s_list", S_SoundList);
    Cmd_AddCommand("s_info", S_SoundInfo);
    Cmd_AddCommand("s_stop", S_StopAllSounds);

    if (!S_Base_Init(&si)) {
        Com_Printf("------------------------------------\n");
        return;
    }
    Com_Printf("------------------------------------\n");

    if (!S_ValidSoundInterface(&si)) {
        Com_Error(ERR_FATAL, "Sound interface invalid");
    }

    S_SoundInfo();
}

/*
=================
S_Shutdown
=================
*/
void S_Shutdown(void) {
    if (si.Shutdown) {
        si.Shutdown();
    }

    Com_Memset(&si, 0, sizeof(soundInterface_t));

    Cmd_RemoveCommand("play");
    Cmd_RemoveCommand("music");
    Cmd_RemoveCommand("s_list");
    Cmd_RemoveCommand("s_stop");
    Cmd_RemoveCommand("s_info");

    S_CodecShutdown();
}
