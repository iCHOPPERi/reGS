/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#define CINTERFACE

// snd_dma.cpp - Main control for any streaming sound output device

#include "quakedef.h"
#include "cd.h"
#include "sound.h"
#include "pr_cmds.h"
#include "SDL2/SDL.h"
#include "vgui_int.h"
#include "mathlib.h"
#include "com_model.h"
#include "host.h"
#include "client.h"
#include "cl_main.h"

#include "protocol.h"

#include "winsani_in.h"
#include <Windows.h>
#include "winsani_out.h"

#include <SDL2/SDL_syswm.h>
#include <mmeapi.h>
#include <mmsystem.h>
#include <dsound.h>

extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

extern DWORD gSndBufSize;

void S_Play( void );
void S_PlayVol( void );
void S_SoundList( void );
void S_Say( void );
void S_Say_Reliable( void );
void S_Update_( void );
void S_StopAllSounds( const bool clear );
void S_StopAllSoundsC( void );

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t   channels[MAX_CHANNELS];

int	total_channels;

int snd_blocked = 0;
bool snd_ambient = true;
bool snd_initialized = false;

// pointer should go away
volatile dma_t* shm = 0;
volatile dma_t sn;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist = 1000.f;

int	soundtime;		// sample PAIRS
int paintedtime; 	// sample PAIRS

sfx_t* known_sfx = nullptr;		// hunk allocated [MAX_SFX]
int num_sfx = 0;

sfx_t* ambient_sfx[NUM_AMBIENTS];

int 		desired_speed = 11025;
int 		desired_bits = 16;

bool sound_started = false;

cvar_t s_show = { "s_show", "0" };

cvar_t a3d = { "s_a3d", "0" };
cvar_t s_eax = { "s_eax", "0" };

cvar_t bgmvolume = { "bgmvolume", "1", FCVAR_ARCHIVE };
cvar_t MP3Volume = { "MP3Volume", "0.8", FCVAR_ARCHIVE | FCVAR_FILTERABLE };
cvar_t MP3FadeTime = { "MP3FadeTime", "2.0", FCVAR_ARCHIVE };
cvar_t volume = { "volume", "0.7", FCVAR_ARCHIVE | FCVAR_FILTERABLE };
cvar_t suitvolume = { "suitvolume", "0.25", FCVAR_ARCHIVE };
cvar_t hisound = { "hisound", "1.0", FCVAR_ARCHIVE };

cvar_t nosound = { "nosound", "0" };
cvar_t loadas8bit = { "loadas8bit", "0" };
cvar_t ambient_level = { "ambient_level", "0.3" };
cvar_t ambient_fade = { "ambient_fade", "100" };
cvar_t snd_noextraupdate = { "snd_noextraupdate", "1" };
cvar_t snd_show = { "snd_show", "0", FCVAR_SPONLY };
cvar_t _snd_mixahead = { "_snd_mixahead", "0.1", FCVAR_ARCHIVE };

cvar_t speak_enable = { "speak_enabled", "1", FCVAR_FILTERABLE };

extern wavstream_t wavstreams[MAX_CHANNELS];
void SX_RoomFX( int count, int fFilter, int fTimefx );

mleaf_t* Mod_PointInLeaf( vec_t* p, model_t* model );

// ====================================================================
// User-setable variables
// ====================================================================

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

bool fakedma = false;

void S_AmbientOff( void )
{
	snd_ambient = false;
}


void S_AmbientOn( void )
{
	snd_ambient = true;
}



void S_SoundInfo_f( void )
{
	if (!sound_started || shm == nullptr)
	{
		Con_Printf("sound system not started\n");
		return;
	}

	Con_Printf("%5d stereo\n", shm->channels - 1);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d samplebits\n", shm->samplebits);
	Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf("%5d speed\n", shm->speed);
	Con_Printf("0x%x dma buffer\n", shm->buffer);
	Con_Printf("%5d total_channels\n", total_channels);
}

void S_Startup( void )
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

/*
================
S_Init
================
*/
void S_Init( void )
{
	Con_DPrintf("Sound Initialization\n");

	VOX_Init();

	if (COM_CheckParm("-nosound"))
		return;

	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("speak", S_Say);
	Cmd_AddCommand("spk", S_Say_Reliable);
	Cmd_AddCommand("stopsound", S_StopAllSoundsC);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

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

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set("loadas8bit", "1");
		Con_DPrintf("loading all sounds as 8bit\n");
	}

	snd_initialized = true;

	S_Startup();

	SND_InitScaletable();

	known_sfx = (sfx_t*)Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

	if (fakedma)
	{
		shm = (volatile dma_t*)Hunk_AllocName(sizeof(dma_t), "shm");

		shm->splitbuffer = false;
		shm->samplebits = 16;
		shm->speed = SOUND_DMA_SPEED;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;

		shm->submission_chunk = 1;
		shm->buffer = (byte*)Hunk_AllocName(1 << 16, "shmbuf");

		Con_DPrintf("Sound sampling rate: %i\n", shm->speed);
	}

	S_StopAllSounds(true);

	SX_Init();
	Wavstream_Init();
}

// =======================================================================
// Shutdown sound engine
// =======================================================================
void S_Shutdown( void )
{
	VOX_Shutdown();

	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = false;

	shm = 0;
	sound_started = false;

	if (!fakedma)
	{
		SNDDMA_Shutdown();
	}
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
// Return sfx and set pfInCache to 1 if 
// name is in name cache. Otherwise, alloc
// a new spot in name cache and return 0 
// in pfInCache.
sfx_t* S_FindName( char* name, int* pfInCache )
{
	int i;
	sfx_t* sfx = nullptr;

	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	for (i = 0; i < num_sfx; i++)
	{
		if (!Q_strcasecmp(known_sfx[i].name, name))
		{
			if (pfInCache)
				*pfInCache = Cache_Check(&known_sfx[i].cache) ? 1 : 0;

			if (known_sfx[i].servercount > 0)
				known_sfx[i].servercount = cl.servercount;

			return &known_sfx[i];
		}

		if (!sfx)
		{
			if (known_sfx[i].servercount > 0)
			{
				if (known_sfx[i].servercount != cl.servercount)
					sfx = &known_sfx[i];
			}
		}
	}

	if (!sfx)
	{
		if (num_sfx == MAX_SFX)
			Sys_Error("S_FindName: out of sfx_t");

		sfx = &known_sfx[i];
		num_sfx++;
	}
	else
	{
		if (Cache_Check(&sfx->cache))
			Cache_Free(&sfx->cache);
	}

	Q_strncpy(sfx->name, name, sizeof(sfx->name) - 1);
	sfx->name[sizeof(sfx->name) - 1] = 0;

	if (pfInCache)
	{
		*pfInCache = 0;
	}

	sfx->servercount = cl.servercount;
	return sfx;
}

/*
==================
S_PrecacheSound

Reserve space for the name of the sound in a global array.
Load the data for the sound and assign a valid pointer,
unless the sound is a streaming or sentence type.  These
defer loading of data until just before playback.
==================
*/
sfx_t* S_PrecacheSound( char* name )
{
	sfx_t* sfx = nullptr;

	if (!sound_started || nosound.value || Q_strlen(name) >= MAX_QPATH)
		return nullptr;

	if (name[0] == '*' || name[0] == '!')
	{
		// This is a streaming sound or a sentence name.
		// don't actually precache data, just store name

		sfx = S_FindName(name, nullptr);
		return sfx;
	}
	else
	{
		// Entity sound.
		sfx = S_FindName(name, nullptr);

		if (fs_lazy_precache.value == 0.0)
		{
			// cache sound
			S_LoadSound(sfx, nullptr);
		}

		return sfx;
	}
}

bool SND_FStreamIsPlaying( sfx_t* sfx )
{
	int	ch_idx;

	for (ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++)
	{
		if (channels[ch_idx].sfx == sfx)
			return true;
	}

	return false;
}

/*
=================
SND_PickDynamicChannel
Select a channel from the dynamic channel allocation area.  For the given entity,
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t* SND_PickDynamicChannel( int entnum, int entchannel, sfx_t* sfx )
{
	int	ch_idx;
	int	first_to_die;
	int	life_left;

	if (entchannel == CHAN_STREAM && SND_FStreamIsPlaying(sfx))
		return nullptr;

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;

	ch_idx = NUM_AMBIENTS;

	for (; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		channel_t* ch = &channels[ch_idx];

		// Never override a streaming sound that is currently playing or
		// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
		if (ch->entchannel == CHAN_STREAM &&
			wavstreams[ch_idx].hFile)
		{
			if (entchannel == CHAN_VOICE)
				return nullptr;

			continue;
		}

		if (entchannel != CHAN_AUTO		// channel 0 never overrides
			&& ch->entnum == entnum
			&& (ch->entchannel == entchannel || entchannel == -1))
		{	
			// allways override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (ch->entnum == cl.viewentity && entnum != cl.viewentity && ch->sfx)
			continue;

		if (ch->end - paintedtime < life_left)
		{
			life_left = ch->end - paintedtime;
			first_to_die = ch_idx;
		}
	}
	
	if (first_to_die == -1)
		return nullptr;

	if (channels[first_to_die].sfx)
	{
		S_FreeChannel(&channels[first_to_die]);
		channels[first_to_die].sfx = nullptr;
	}

	return &channels[first_to_die];
}



/*
=====================
SND_PickStaticChannel
=====================
Pick an empty channel from the static sound area, or allocate a new
channel.  Only fails if we're at max_channels (128!!!) or if
we're trying to allocate a channel for a stream sound that is
already playing.

*/
channel_t* SND_PickStaticChannel( int entnum, int entchannel, sfx_t* sfx )
{
	int i;
	channel_t* ch = nullptr;

	if (sfx->name[0] == '*' && SND_FStreamIsPlaying(sfx))
		return nullptr;

	// Check for replacement sound, or find the best one to replace
	for (i = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i < total_channels; i++)
		if (channels[i].sfx == nullptr)
			break;


	if (i < total_channels)
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if (total_channels == MAX_CHANNELS)
		{
			Con_DPrintf("total_channels == MAX_CHANNELS\n");
			return nullptr;
		}


		// get a channel for the static sound

		ch = &channels[total_channels];
		total_channels++;
	}

	return ch;
}


// search through all channels for a channel that matches this
// entnum, entchannel and sfx, and perform alteration on channel
// as indicated by 'flags' parameter. If shut down request and
// sfx contains a sentence name, shut off the sentence.
// returns TRUE if sound was altered,
// returns FALSE if sound was not found (sound is not playing)

int S_AlterChannel( int entnum, int entchannel, sfx_t* sfx, int vol, int pitch, int flags )
{
	int	ch_idx;

	if (sfx->name[0] == '!')
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isentence >= 0 and matches the
		// soundsource.
		for (ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++)
		{
			if (channels[ch_idx].entnum == entnum
				&& channels[ch_idx].entchannel == entchannel
				&& channels[ch_idx].sfx != nullptr
				&& channels[ch_idx].isentence >= 0)
			{
				if (flags & SND_CHANGE_PITCH)
					channels[ch_idx].pitch = pitch;

				if (flags & SND_CHANGE_VOL)
					channels[ch_idx].master_vol = vol;

				if (flags & SND_STOP)
				{
					S_FreeChannel(&channels[ch_idx]);
				}

				return true;
			}
		}
		// channel not found
		return false;
	}

	// regular sound or streaming sound

	for (ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++)
	{
		if (channels[ch_idx].entnum == entnum
			&& channels[ch_idx].entchannel == entchannel
			&& channels[ch_idx].sfx == sfx)
		{
			if (flags & SND_CHANGE_PITCH)
				channels[ch_idx].pitch = pitch;

			if (flags & SND_CHANGE_VOL)
				channels[ch_idx].master_vol = vol;

			if (flags & SND_STOP)
			{
				S_FreeChannel(&channels[ch_idx]);
			}

			return true;
		}
	}

	return false;
}

extern cvar_t sys_timescale; // TODO: Move this extern somewhere -Doomsayer

// =======================================================================
// S_StartDynamicSound
// =======================================================================
// Start a sound effect for the given entity on the given channel (ie; voice, weapon etc).  
// Try to grab a channel out of the 8 dynamic spots available.
// Currently used for looping sounds, streaming sounds, sentences, and regular entity sounds.
// NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
// Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

// NOTE: it's not a good idea to play looping sounds through StartDynamicSound, because
// if the looping sound starts out of range, or is bumped from the buffer by another sound
// it will never be restarted.  Use S_StartStaticSound (pass CHAN_STATIC to EMIT_SOUND or
// SV_StartSound.


void S_StartDynamicSound( int entnum, int entchannel, sfx_t* sfx, vec_t* origin, float fvol, float attenuation, int flags, int pitch )
{
	int	vol;
	int	fsentence = 0;
	char sentencename[MAX_QPATH];

	sfxcache_t* sc = nullptr;
	channel_t* target_chan = nullptr;

	if (!sound_started || !sfx || nosound.value)
		return;

	// See we are are playing stream sound
	if (sfx->name[0] == '*')
		entchannel = CHAN_STREAM;

	if (entchannel == CHAN_STREAM && pitch != PITCH_NORM)
	{
		Con_DPrintf("Warning: pitch shift ignored on stream sound %s\n", sfx->name);
		pitch = PITCH_NORM;
	}

	// Make the volume [0..255] based
	vol = fvol * 255;

	if (vol > 255)
	{
		Con_DPrintf("S_StartDynamicSound: %s volume > 255", sfx->name);
		vol = 255;
	}

	if (flags & (SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH))
	{
		if (S_AlterChannel(entnum, entchannel, sfx, vol, pitch, flags))
			return;

		if (flags & SND_STOP)
			return;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it u
	}

	if (pitch == 0)
	{
		Con_DPrintf("Warning: S_StartDynamicSound Ignored, called with pitch 0");
		return;
	}

	// pick a channel to play on
	target_chan = SND_PickDynamicChannel(entnum, entchannel, sfx);

	if (!target_chan)
		return;

	if (sfx->name[0] == '!' || sfx->name[0] == '#')
		fsentence = 1;

	// spatialize
	Q_memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);

	// reference_dist / (reference_power_level / actual_power_level)
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;

	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	target_chan->pitch = (sys_timescale.value + 1.0) * (float)pitch * 0.5;
	target_chan->isentence = -1;

	SND_Spatialize(target_chan);

	// If a client can't hear a sound when they FIRST receive the StartSound message,
	// the client will never be able to hear that sound. This is so that out of 
	// range sounds don't fill the playback buffer.  For streaming sounds, we bypass this optimization.

	if (!target_chan->leftvol && !target_chan->rightvol)
	{
		// if this is a streaming sound, play
		// the whole thing.

		if (entchannel != CHAN_STREAM)
		{
			target_chan->sfx = nullptr;
			return;		// not audible at all
		}
	}

	if (fsentence)
	{
		// this is a sentence
		// link all words and load the first word

		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 

		Q_strncpy(sentencename, &sfx->name[1], sizeof(sentencename) - 1);
		sentencename[sizeof(sentencename) - 1] = 0;

		sc = VOX_LoadSound(target_chan, sentencename);
	}
	else
	{
		// regular or streamed sound fx
		sc = S_LoadSound(sfx, target_chan);
		target_chan->sfx = sfx;
	}

	if (!sc)
	{
		target_chan->sfx = nullptr;
		return;
	}

	target_chan->pos = 0;
	target_chan->end = sc->length + paintedtime;

	if (!fsentence && target_chan->pitch != PITCH_NORM)
		VOX_MakeSingleWordSentence(target_chan, target_chan->pitch);

	VOX_TrimStartEndTimes(target_chan, sc);

	// Init client entity mouth movement vars
	SND_InitMouth(entnum, entchannel);

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder

	// UNDONE: this should be implemented using a start delay timer in the 
	// mixer, instead of skipping forward in the wave.  Either method also cause
	// phasing problems for skip times of < 10 milliseconds. KB


	int		ch_idx;
	int		skip;
	channel_t* check = &channels[0];
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;

		if (check->sfx == sfx && !check->pos)
		{
			skip = RandomLong(0, (int32)(shm->speed * 0.1));		// skip up to 0.1 seconds of audio

			if (skip >= target_chan->end)
				skip = target_chan->end - 1;

			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}


/*
=================
S_StartStaticSound
=================
Start playback of a sound, loaded into the static portion of the channel array.
Currently, this should be used for looping ambient sounds, looping sounds
that should not be interrupted until complete, non-creature sentences,
and one-shot ambient streaming sounds.  Can also play 'regular' sounds one-shot,
in case designers want to trigger regular game sounds.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

  NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
*/
void S_StartStaticSound( int entnum, int entchannel, sfx_t* sfxin, vec_t* origin, float fvol, float attenuation, int flags, int pitch )
{
	int vol;
	int fvox = 0;
	char sentencename[MAX_QPATH];

	sfxcache_t* sc = nullptr;
	channel_t* ch = nullptr;

	if (!sfxin)
		return;

	if (sfxin->name[0] == '*')
		entchannel = CHAN_STREAM;

	if (entchannel == CHAN_STREAM && pitch != PITCH_NORM)
	{
		Con_DPrintf("Warning: pitch shift ignored on stream sound %s\n", sfxin->name);
		pitch = PITCH_NORM;
	}

	vol = fvol * 255;

	if (vol > 255)
	{
		Con_DPrintf("S_StartStaticSound: %s volume > 255", sfxin->name);
		vol = 255;
	}

	if ((flags & SND_STOP) || (flags & SND_CHANGE_VOL) || (flags & SND_CHANGE_PITCH))
	{
		if (S_AlterChannel(entnum, entchannel, sfxin, vol, pitch, flags) || (flags & SND_STOP))
			return;
	}

	if (pitch == 0)
	{
		Con_DPrintf("Warning: S_StartStaticSound Ignored, called with pitch 0");
		return;
	}

	// pick a channel to play on from the static area
	ch = SND_PickStaticChannel(entnum, entchannel, sfxin);

	if (!ch)
		return;

	if (sfxin->name[0] == '!' || sfxin->name[0] == '#')
	{
		// this is a sentence. link words to play in sequence.

		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 

		Q_strncpy(sentencename, &sfxin->name[1], sizeof(sentencename) - 1);
		sentencename[sizeof(sentencename) - 1] = 0;

		// link all words and load the first word
		sc = VOX_LoadSound(ch, sentencename);
		fvox = 1;
	}
	else
	{
		// load regular or stream sound

		sc = S_LoadSound(sfxin, ch);
		ch->sfx = sfxin;
		ch->isentence = -1;
	}

	if (!sc)
	{
		ch->sfx = nullptr;
		return;
	}

	VectorCopy(origin, ch->origin);

	ch->pos = 0;

	ch->master_vol = vol;
	ch->dist_mult = attenuation / sound_nominal_clip_dist;

	ch->end = sc->length + paintedtime;
	ch->entnum = entnum;
	ch->pitch = (sys_timescale.value + 1.0) * (float)pitch * 0.5;
	ch->entchannel = entchannel;

	if (!fvox && ch->pitch != PITCH_NORM)
		VOX_MakeSingleWordSentence(ch, ch->pitch);

	VOX_TrimStartEndTimes(ch, sc);

	SND_Spatialize(ch);
}

void S_ClearBuffer(void)
{
	int		clear;

	if (!sound_started || !shm || (!shm->buffer && !pDSBuf))
		return;

	if (shm->samplebits == 8)
		clear = 128;
	else
		clear = 0;

	if (pDSBuf)
	{
		DWORD	dwSize;
		PVOID pData;
		int		reps;
		HRESULT	hresult;

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

// Stop all sounds for entity on a channel.
void S_StopSound( int entnum, int entchannel )
{
	int	i;

	for (i = NUM_AMBIENTS; i < total_channels; i++)
	{
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel)
		{
			S_FreeChannel(&channels[i]);
		}
	}
}

void S_StopAllSounds( const bool clear )
{
	int		i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (channels[i].sfx)
			S_FreeChannel(&channels[i]);
	}

	Q_memset(channels, 0, sizeof(channels));
	Wavstream_Init();

	if (clear)
		S_ClearBuffer();
}

void S_StopAllSoundsC( void )
{
	S_StopAllSounds(true);
}

void S_PrintStats( void )
{
	int i;
	channel_t* ch = nullptr;

	if (!s_show.value || cl.maxclients > 2)
		return;

	for (i = 0; i < total_channels; i++)
	{
		ch = &channels[i];

		if (ch->sfx)
		{
			Con_NPrintf(i & 0x1F, "%s %03i %02i %20s", vstr(ch->origin), ch->entnum, ch->entchannel, ch->sfx->name);
		}
		else
		{
			Con_NPrintf(i & 0x1F, "");
		}
	}
}

//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds( void )
{
	int	ambient_channel;
	float		vol;

	channel_t* chan;
	mleaf_t* l = nullptr;

	if (!snd_ambient)
		return;

	// calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf(listener_origin, cl.worldmodel);

	if (!l || !ambient_level.value)
	{
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
			channels[ambient_channel].sfx = nullptr;

		return;
	}

	for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

/*
=================
SND_Spatialize

spatializes a channel
=================
*/
void SND_Spatialize( channel_t* ch )
{
	float normv;
	vec3_t source_vec;
	float lleft, lright;

	cl_entity_t* pent = nullptr;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		VOX_SetChanVol(ch);
		return;
	}

	// Make sure index is valid
	if (ch->entnum > 0 && ch->entnum < cl.num_entities)
	{
		pent = &cl_entities[ch->entnum];

		if (pent && pent->model &&
			pent->curstate.messagenum == cl.parsecount)
		{
			VectorCopy(pent->origin, ch->origin);

			if (pent->model->type == mod_brush)
			{
				ch->origin[0] = pent->origin[0] + (pent->model->mins[0] + pent->model->maxs[0]) * 0.5;
				ch->origin[1] = pent->origin[1] + (pent->model->mins[1] + pent->model->maxs[1]) * 0.5;
				ch->origin[2] = pent->origin[2] + (pent->model->mins[2] + pent->model->maxs[2]) * 0.5;
			}
		}
	}

	VectorSubtract(ch->origin, listener_origin, source_vec);
	normv = VectorNormalize(source_vec) * ch->dist_mult;
	
	if (shm && shm->channels == 1)
	{
		lleft = 1.0;
		lright = 1.0;
	}
	else
	{
		lleft = 1.0f - DotProduct(source_vec, listener_right);
		lright = DotProduct(source_vec, listener_right) + 1.0;
	}

	ch->leftvol = lleft * (1.0 - normv) * ch->master_vol;
	ch->rightvol = lright * (1.0 - normv) * ch->master_vol;

	VOX_SetChanVol(ch);

	if (ch->rightvol < 0)
		ch->rightvol = 0;
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update( vec_t* origin, vec_t* forward, vec_t* right, vec_t* up )
{
	int			i;
	int			total;
	channel_t* ch;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds();

	// update spatialization for static and dynamic sounds	
	ch = channels + NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < total_channels; i++, ch++)
	{
		if (ch->sfx)
			SND_Spatialize(ch);	// respatialize channel
	}
	
	//
	// debugging output
	//
	if (snd_show.value)
	{
		total = 0;
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++)
		{
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				Con_Printf("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		}

		Con_Printf("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

void S_ExtraUpdate( void )
{
	if (!VGuiWrap2_IsGameUIVisible())
	{
		SDL_PumpEvents();
		ClientDLL_IN_Accumulate();
	}

	if (snd_noextraupdate.value)
		return;		// don't pollute timings

	S_Update_();
}

void GetSoundtime( void )
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	if (shm)
		fullsamples = shm->samples / shm->channels;
	
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

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

void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

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
		DWORD	dwStatus;

		if (pDSBuf->lpVtbl->GetStatus(pDSBuf, &dwStatus) != DS_OK)
			Con_Printf("Couldn't get sound buffer status\n");

		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBuf->lpVtbl->Restore(pDSBuf);

		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
	}

	S_PaintChannels(endtime >> 1);

	SNDDMA_Submit();
}

void S_BeginPrecaching( void )
{
	cl.fPrecaching = true;
}

void S_EndPrecaching( void )
{
	cl.fPrecaching = false;
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play( void )
{
	static int hash = 345;
	int i;
	char name[256];
	sfx_t* sfx;

	if (!speak_enable.value)
		return;

	i = 1;
	while (i < Cmd_Argc())
	{
		Q_strncpy(name, Cmd_Argv(i), sizeof(name) - 4 - 1);
		name[sizeof(name) - 4 - 1] = 0;

		if (!Q_strrchr(Cmd_Argv(i), '.'))
			strcat(name, ".wav");

		sfx = S_PrecacheSound(name);
		S_StartDynamicSound(hash++, CHAN_AUTO, sfx, listener_origin, VOL_NORM, 1.0, 0, PITCH_NORM);
		i++;
	}
}

void S_PlayVol( void )
{
	static int hash = 543;
	int i;
	float vol;
	char name[256];
	sfx_t* sfx;

	if (!speak_enable.value)
		return;

	i = 1;
	while (i < Cmd_Argc())
	{
		Q_strncpy(name, Cmd_Argv(i), sizeof(name) - 4 - 1);
		name[sizeof(name) - 4 - 1] = 0;

		if (!Q_strrchr(Cmd_Argv(i), '.'))
			strcat(name, ".wav");

		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i + 1));
		S_StartDynamicSound(hash++, CHAN_AUTO, sfx, listener_origin, vol, 1.0, 0, PITCH_NORM);
		i += 2;
	}
}

// speak a sentence from console; works by passing in "!sentencename"
// or "sentence"

void S_Say( void )
{
	sfx_t* sfx;
	char name[256];

	if (nosound.value)
		return;

	if (!sound_started)
		return;

	if (Cmd_Argc() < 2 || !speak_enable.value)
		return;

	Q_strncpy(name, Cmd_Argv(1), sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	// DEBUG - test performance of dsp code
	if (!Q_strcmp(name, "dsp"))
	{
		unsigned time;
		int i;
		int count = 10000;

		for (i = 0; i < PAINTBUFFER_SIZE; i++)
		{
			paintbuffer[i].left = RandomLong(0, 2999);
			paintbuffer[i].right = RandomLong(0, 2999);
		}

		Con_Printf("Start profiling 10,000 calls to DSP\n");

		// get system time

		time = timeGetTime();

		for (i = 0; i < count; i++)
		{
			SX_RoomFX(PAINTBUFFER_SIZE, true, true);
		}
		// display system time delta 
		Con_Printf("%d milliseconds \n", timeGetTime() - time);
		return;
	}
	else if (!Q_strcmp(name, "paint"))
	{
		unsigned time;
		int count = 10000;
		static int hash = 543;
		int psav = paintedtime;

		Con_Printf("Start profiling S_PaintChannels\n");

		sfx = S_PrecacheSound("ambience/labdrone1.wav");
		S_StartDynamicSound(hash++, CHAN_AUTO, sfx, listener_origin, VOL_NORM, 1.0, 0, PITCH_NORM);

		// get system time
		time = timeGetTime();

		// paint a boatload of sound

		S_PaintChannels(paintedtime + 512 * count);

		// display system time delta 
		Con_Printf("%d milliseconds \n", timeGetTime() - time);
		paintedtime = psav;
		return;
	}
	// DEBUG

	if (name[0] != '!')
	{
		// build a fake sentence name, then play the sentence text

		snprintf(name, sizeof(name), "xxtestxx %s", Cmd_Argv(1));

		// insert null terminator after sentence name
		name[8] = 0;

		rgpszrawsentence[cszrawsentences] = name;
		cszrawsentences++;

		sfx = S_PrecacheSound("!xxtestxx");
		if (!sfx)
		{
			Con_Printf("S_Say: can't cache %s\n", name);
			return;
		}

		S_StartDynamicSound(cl.viewentity, -1, sfx, vec3_origin, VOL_NORM, 1.0, 0, PITCH_NORM);

		// remove last
		rgpszrawsentence[--cszrawsentences] = nullptr;
	}
	else
	{
		sfx = S_FindName(name, nullptr);
		if (!sfx)
		{
			Con_Printf("S_Say: can't find sentence name %s\n", name);
			return;
		}

		S_StartDynamicSound(cl.viewentity, -1, sfx, vec3_origin, VOL_NORM, 1.0, 0, PITCH_NORM);
	}
}

void S_Say_Reliable( void )
{
	sfx_t* sfx;
	char name[256];

	if (nosound.value)
		return;

	if (!sound_started)
		return;

	if (Cmd_Argc() < 2 || !speak_enable.value)
		return;

	Q_strncpy(name, Cmd_Argv(1), sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	if (name[0] != '!')
	{
		// build a fake sentence name, then play the sentence text

		snprintf(name, sizeof(name), "xxtestxx %s", Cmd_Argv(1));

		// insert null terminator after sentence name
		name[8] = 0;

		rgpszrawsentence[cszrawsentences] = name;
		cszrawsentences++;

		sfx = S_PrecacheSound("!xxtestxx");
		if (!sfx)
		{
			Con_Printf("S_Say_Reliable: can't cache %s\n", name);
			return;
		}

		S_StartStaticSound(cl.viewentity, CHAN_STATIC, sfx, vec3_origin, VOL_NORM, 1.0, 0, PITCH_NORM);

		// remove last
		rgpszrawsentence[--cszrawsentences] = nullptr;
	}
	else
	{
		sfx = S_FindName(name, nullptr);
		if (!sfx)
		{
			Con_Printf("S_Say_Reliable: can't find sentence name %s\n", name);
			return;
		}

		S_StartStaticSound(cl.viewentity, CHAN_STATIC, sfx, vec3_origin, VOL_NORM, 1.0, 0, PITCH_NORM);
	}
}

void S_SoundList( void )
{
	int		i;
	sfx_t* sfx;
	sfxcache_t* sc;
	int		size, total;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++)
	{
		sc = (sfxcache_t*)Cache_Check(&sfx->cache);

		if (!sc)
			continue;

		size = sc->length * sc->width * (sc->stereo + 1);
		total += size;

		if (sc->loopstart >= 0)
			Con_Printf("L");
		else
			Con_Printf(" ");

		Con_Printf("(%2db) %6i : %s\n", sc->width * 8, size, sfx->name);
	}

	Con_Printf("Total resident: %i\n", total);
}