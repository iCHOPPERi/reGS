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

#define CINTERFACE

#include "quakedef.h"
#include "sound.h"
#include <WinSock2.h>
#include <dsound.h>

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
bool sound_started;
volatile dma_t* shm;
volatile dma_t sn;
sfx_t* known_sfx;
int num_sfx = 0;
int snd_blocked;
int soundtime;
int paintedtime;

extern HANDLE		hData;
extern HPSTR		lpData, lpData2;

extern HGLOBAL		hWaveHdr;
extern LPWAVEHDR	lpWaveHdr;

extern HWAVEOUT    hWaveOut;

extern WAVEOUTCAPS	wavecaps;

extern DWORD	gSndBufSize;

extern MMTIME		mmstarttime;

extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

extern HINSTANCE hInstDS;

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
        shm->speed = SOUND_DMA_SPEED;
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

void S_Startup(void)
{
    if (!snd_initialized)
        return;

    if (fakedma || SNDDMA_Init())
    {
        sound_started = true;
    }
    else
    {
        Con_Printf("S_Startup: SNDDMA_Init failed.\n");
        sound_started = false;
    }
}

void S_Shutdown()
{
    // VOX_Shutdown();
    if (sound_started)
    {
        if (shm)
            shm->gamealive = false;

        shm = 0;
        sound_started = 0;

        if (fakedma == false)
            SNDDMA_Shutdown();
    }
}

sfx_t* __cdecl S_FindName(char* name, int* pfInCache)
{
    //TODO: implement - ScriptedSnark
    sfx_t* sfx = NULL;
    return sfx;
}

sfx_t* S_PrecacheSound(char* name)
{
    int len;
    sfx_t* sfx;

    if (((sound_started != 0) && (nosound.value == 0.0)) && (len = Q_strlen(name), len < 64)) {
        if ((*name != '!') && (*name != '*')) {
            sfx = S_FindName(name, NULL);
            if (fs_lazy_precache.value == 0.0) {
                //S_LoadSound(sfx, NULL); - IMPLEMENT - ScriptedSnark
                return sfx;
            }
            return sfx;
        }
        sfx = S_FindName(name, NULL);
        return sfx;
    }

    return NULL;
}

void S_StartDynamicSound(int entnum, int entchannel, sfx_t* sfx, vec_t* origin, float fvol, float attenuation, int flags, int pitch)
{
	//TODO: implement - Solokiller
}

void __cdecl S_Update(vec_t* origin, vec_t* forward, vec_t* right, vec_t* up)
{/* TODO: reverse S_Update func - ScriptedSnark
    int v4; // eax
    channel_t* v5; // ebx
    int v6; // esi
    int v7; // ebp
    channel_t* v8; // ebx
    int i; // esi
    int v10; // ecx
    int v11; // edi
    unsigned int v12; // eax
    int v13; // [esp+18h] [ebp-34h]
    unsigned int v14; // [esp+1Ch] [ebp-30h]

    if (sound_started && snd_blocked <= 0)
    {
        listener_origin[0] = *origin;
        listener_origin[1] = origin[1];
        listener_origin[2] = origin[2];
        listener_forward[0] = *forward;
        listener_forward[1] = forward[1];
        listener_forward[2] = forward[2];
        listener_right[0] = *right;
        listener_right[1] = right[1];
        listener_right[2] = right[2];
        listener_up[0] = *up;
        listener_up[1] = up[1];
        listener_up[2] = up[2];
        S_UpdateAmbientSounds();
        v4 = total_channels;
        if (total_channels > 4)
        {
            v5 = &channels[4];
            v6 = 4;
            do
            {
                if (v5->sfx)
                {
                    SND_Spatialize(v5);
                    v4 = total_channels;
                }
                ++v6;
                ++v5;
            } while (v6 < v4);
        }
        if (snd_show.value != 0.0)
        {
            v7 = 0;
            if (v4 > 0)
            {
                v8 = channels;
                for (i = 0; i < v4; ++i)
                {
                    if (v8->sfx)
                    {
                        v10 = v8->leftvol;
                        v11 = v8->rightvol;
                        if (v10 || v11)
                        {
                            ++v7;
                            Con_Printf("%3i %3i %s\n", v10, v11, v8->sfx->name);
                            v4 = total_channels;
                        }
                    }
                    ++v8;
                }
            }
            Con_Printf("----(%i)----\n", v7);
        }
        if (sound_started && snd_blocked <= 0)
        {
            GetSoundtime();
            if (shm)
            {
                v14 = (__int64)((long double)soundtime + (long double)shm->dmaspeed * snd_mixahead.value);
                v13 = shm->samples >> (LOBYTE(shm->channels) - 1);
            }
            v12 = v13 + soundtime;
            if ((int)(v14 - soundtime) <= v13)
                v12 = v14;
            S_PaintChannels(v12 >> 1);
            SNDDMA_Submit(origin, forward, right, up);
        }
    }*/
}

void GetSoundtime(void)
{
    int        samplepos;
    static    int        buffers;
    static    int        oldsamplepos;
    int        fullsamples;

    if (shm)
        fullsamples = shm->samples / shm->channels;

    samplepos = SNDDMA_GetDMAPos();

    if (samplepos < oldsamplepos)
    {
        buffers++;                    // buffer wrapped

        if (paintedtime > 0x40000000)
        {
            // time to chop things off to avoid 32 bit limits
            buffers = 0;
            paintedtime = fullsamples;
            S_StopAllSounds(true);
        }
    }
    oldsamplepos = samplepos;

    if (shm)
        soundtime = fullsamples * buffers + samplepos / shm->channels;
}

void S_ClearBuffer(void)
{
    int        clear;

    if (!sound_started || !shm || (!shm->buffer && !pDSBuf))
        return;

    if (shm->samplebits == 8)
        clear = 128;
    else
        clear = 0;

    if (pDSBuf)
    {
        DWORD    dwSize;
        PVOID pData;
        int        reps;
        HRESULT    hresult;

        reps = 0;

        while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0)) != DS_OK)
        {
            if (hresult != DSERR_BUFFERLOST)
            {
                Con_Printf("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
                S_Shutdown();
                return;
            }

            if (++reps > 10000)
            {
                Con_Printf("S_ClearBuffer: DS: couldn't restore buffer\n");
                S_Shutdown();
                return;
            }
        }

        Q_memset(pData, clear, shm->samples * shm->samplebits / 8);

        pDSBuf->lpVtbl->Unlock(pDSBuf, pData, dwSize, NULL, 0);

    }
    else
    {
        Q_memset(shm->buffer, clear, shm->samplebits * shm->samples / 8);
    }
}

void S_Update_(void)
{
    unsigned        endtime;
    int                samps;

    if (!sound_started || (snd_blocked > 0))
        return;

    // Updates DMA time
    GetSoundtime();

    // mix ahead of current position
    if (shm)
    {
        endtime = soundtime + _snd_mixahead.value * shm->dmaspeed;
        samps = shm->samples >> ((char)shm->channels - 1 & 0x1F);
    }

    if ((int)(endtime - soundtime) > samps) // Cast to int to shut up John Carmack's code warning -Enko
        endtime = soundtime + samps;

    // if the buffer was lost or stopped, restore it and/or restart it
    if (pDSBuf)
    {
        DWORD    dwStatus;

        if (pDSBuf->lpVtbl->GetStatus(pDSBuf, &dwStatus) != DS_OK)
            Con_Printf("Couldn't get sound buffer status\n");

        if (dwStatus & DSBSTATUS_BUFFERLOST)
            pDSBuf->lpVtbl->Restore(pDSBuf);

        if (!(dwStatus & DSBSTATUS_PLAYING))
            pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
    }

    // S_PaintChannels(endtime >> 1); - TODO: implement - ScriptedSnark

    SNDDMA_Submit();
}

void S_StopSound(int entnum, int entchannel)
{
	//TODO: implement - Solokiller
}

void S_StopAllSounds(bool clear)
{
	//TODO: implement  - Solokiller
}
