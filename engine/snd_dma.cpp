/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/**
*	@file
*
*	main control for any streaming sound output device
*/

#include "quakedef.h"
#include "sound.h"

cvar_t suitvolume = { "suitvolume", "0.25", FCVAR_ARCHIVE };
cvar_t s_show = { "s_show", "0", FCVAR_ARCHIVE };
cvar_t nosound = { "nosound", "0", FCVAR_ARCHIVE };
cvar_t volume = { "volume", "0.800", FCVAR_ARCHIVE };
cvar_t hisound = { "hisound", "0", FCVAR_ARCHIVE };
cvar_t loadas8bit = { "loadas8bit", "0", FCVAR_ARCHIVE };
cvar_t bgmvolume = { "bgmvolume", "0", FCVAR_ARCHIVE };
cvar_t MP3Volume = { "MP3Volume", "0.8", FCVAR_ARCHIVE };
cvar_t MP3FadeTime = { "MP3FadeTime", "2.0", FCVAR_ARCHIVE };
cvar_t ambient_level = { "ambient_level", "0.300", FCVAR_ARCHIVE };
cvar_t ambient_fade = { "ambient_fade", "100", FCVAR_ARCHIVE };
cvar_t snd_noextraupdate = { "snd_noextraupdate", "0", FCVAR_ARCHIVE };
cvar_t snd_show = { "snd_show", "0", FCVAR_ARCHIVE };
cvar_t _snd_mixahead = { "_snd_mixahead", "0.100", FCVAR_ARCHIVE };
cvar_t speak_enable = { "speak_enabled", "1", FCVAR_ARCHIVE };
cvar_t fs_lazy_precache = { "fs_lazy_precache", "0", FCVAR_ARCHIVE };

bool fakedma = false;
bool snd_initialized = false;
int sound_started = 0;
volatile dma_t* shm;
sfx_t* known_sfx;
int num_sfx = 0;

void S_Init()
{
    Con_DPrintf("Sound Initialization\n");
    VOX_Init();
    if (COM_CheckParm("-nosound") != 0) {
        return;
    }
    if (COM_CheckParm("-simsound") != 0) {
        fakedma = true;
    }
    /* NEED TO REVERSE S_ FUNCS FOR ADDING THESE CONCMDS - ScriptedSnark
    Cmd_AddCommand("play", S_Play);
    Cmd_AddCommand("playvol", S_PlayVol);
    Cmd_AddCommand("speak", S_Say);
    Cmd_AddCommand("spk", S_Say_Reliable);
    Cmd_AddCommand("stopsound", S_StopAllSoundsC);
    Cmd_AddCommand("soundlist", S_SoundList);
    Cmd_AddCommand("soundinfo", S_SoundInfo_f);
    */
    Cvar_RegisterVariable(&s_show);
    Cvar_RegisterVariable(&nosound);
    Cvar_RegisterVariable(&volume);
    Cvar_RegisterVariable(&suitvolume);
    Cvar_RegisterVariable(&hisound);
    Cvar_RegisterVariable(&loadas8bit);
    Cvar_RegisterVariable(&bgmvolume);
    Cvar_RegisterVariable(&MP3Volume);
    Cvar_RegisterVariable(&MP3FadeTime);
    Cvar_RegisterVariable(&ambient_level);
    Cvar_RegisterVariable(&ambient_fade);
    Cvar_RegisterVariable(&snd_noextraupdate);
    Cvar_RegisterVariable(&snd_show);
    Cvar_RegisterVariable(&_snd_mixahead);
    Cvar_RegisterVariable(&speak_enable);
    if (host_parms.memsize < 0x800000) {
        Cvar_Set("loadas8bit", "1");
        Con_DPrintf("loading all sounds as 8bit\n");
    }
    snd_initialized = true;
    if (!fakedma) {
        if (SNDDMA_Init() == 0) {
            Con_Printf("S_Startup: SNDDMA_Init failed.\n");
            sound_started = 0;
        }
    }
    sound_started = 1;
    SND_InitScaletable();
    known_sfx = (sfx_t*) Hunk_AllocName(73728, "sfx_t");
    num_sfx = 0;
    if (fakedma) {
        shm = (volatile dma_t*) Hunk_AllocName(44, "shm");
        shm->splitbuffer = false;
        shm->samplebits = 16;
        shm->speed = SOUND_22k;
        shm->channels = 2;
        shm->samples = 32768;
        shm->samplepos = 0;
        shm->soundalive = true;
        shm->gamealive = true;
        shm->submission_chunk = 1;
        shm->buffer = (byte*) Hunk_AllocName(65536, "shmbuf");
        Con_DPrintf("Sound sampling rate: %i\n", shm->speed);
    }
    if (sound_started != 0) {
        S_StopAllSounds(true);
    }
    /* ALSO NEED TO IMPLEMENT - ScriptedSnark
    SX_Init();
    Wavstream_Init();
    */
    return;
}

void S_Shutdown()
{
	//TODO: implement - Solokiller
}

sfx_t* S_PrecacheSound(const char* name)
{
 /* TODO: UNCOMMENT THIS CODE WHEN S_ FUNCTIONS WILL BE REVERSED - ScriptedSnark
    int len;
    sfx_t* sfx;

    if (((sound_started != 0) && (nosound.value == 0.0)) && (len = Q_strlen(name), len < 64)) {
        if ((*name != '!') && (*name != '*')) {
            sfx = S_FindName(name, NULL);
            if (fs_lazy_precache.value == 0.0) {
                S_LoadSound(sfx, NULL);
                return sfx;
            }
            return sfx;
        }
        sfx = S_FindName(name, NULL);
        return sfx;
    }
    */
    return NULL;
}

void S_StartDynamicSound(int entnum, int entchannel, sfx_t* sfx, vec3_t origin, float fvol, float attenuation, int flags, int pitch)
{
	//TODO: implement - Solokiller
}

void S_StopSound(int entnum, int entchannel)
{
	//TODO: implement - Solokiller
}

void S_StopAllSounds(bool clear)
{
	//TODO: implement  - Solokiller
}
