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

// sound.h -- client sound i/o functions

// Reversed by 1Doomsayer

#ifndef SOUND_H
#define SOUND_H
#ifdef _WIN32
#pragma once
#endif

#include "FileSystem.h"

#define	PAINTBUFFER_SIZE	512

// sound engine rate defines
#define SOUND_DMA_SPEED		22050		// hardware playback rate

#define SOUND_11k			11025		// 11khz sample rate

// sentence groups
#define CBSENTENCENAME_MAX		16
#define CVOXFILESENTENCEMAX		1536		// max number of sentences in game. NOTE: this must match
											// CVOXFILESENTENCEMAX in dlls\util.h!!!

//=====================================================================
// FX presets
//=====================================================================

#define SXROOM_OFF			0		

#define SXROOM_GENERIC		1		// general, low reflective, diffuse room

#define SXROOM_METALIC_S	2		// highly reflective, parallel surfaces
#define SXROOM_METALIC_M	3
#define SXROOM_METALIC_L	4

#define SXROOM_TUNNEL_S		5		// resonant reflective, long surfaces
#define SXROOM_TUNNEL_M		6
#define SXROOM_TUNNEL_L		7

#define SXROOM_CHAMBER_S	8		// diffuse, moderately reflective surfaces
#define SXROOM_CHAMBER_M	9
#define SXROOM_CHAMBER_L	10

#define SXROOM_BRITE_S		11		// diffuse, highly reflective
#define SXROOM_BRITE_M		12
#define SXROOM_BRITE_L		13

#define SXROOM_WATER1		14		// underwater fx
#define SXROOM_WATER2		15
#define SXROOM_WATER3		16

#define SXROOM_CONCRETE_S	17		// bare, reflective, parallel surfaces
#define SXROOM_CONCRETE_M	18
#define SXROOM_CONCRETE_L	19

#define SXROOM_OUTSIDE1		20		// echoing, moderately reflective
#define SXROOM_OUTSIDE2		21		// echoing, dull
#define SXROOM_OUTSIDE3		22		// echoing, very dull

#define SXROOM_CAVERN_S		23		// large, echoing area
#define SXROOM_CAVERN_M		24
#define SXROOM_CAVERN_L		25

#define SXROOM_WEIRDO1		26		
#define SXROOM_WEIRDO2		27
#define SXROOM_WEIRDO3		28

#define CSXROOM				29

// !!! if this is changed, it much be changed in asm_i386.h too !!!
struct portable_samplepair_t
{
	int left;
	int right;
};

struct sfx_t
{
	char 	name[MAX_QPATH];
	cache_user_t	cache;
	int		servercount;
};

// !!! if this is changed, it much be changed in asm_i386.h too !!!
struct sfxcache_t
{
	int 	length;
	int 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	byte	data[1];		// variable sized
};

struct dma_t
{
	qboolean		gamealive;
	qboolean		soundalive;
	qboolean		splitbuffer;
	int				channels;
	int				samples;				// mono samples in buffer
	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;
	int				dmaspeed;
	unsigned char*	buffer;
};

// !!! if this is changed, it much be changed in asm_i386.h too !!!
struct channel_t
{
	sfx_t*	sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume

	int end;			// end time in global paintsamples
	int pos;			// sample position in sfx

	int looping;		// where to loop, -1 = no looping

	int entnum;			// to allow overriding a specific sound
	int entchannel;		// TODO: Define as enum, modify const.h then -Enko

	vec3_t origin;		// origin of sound effect
	vec_t dist_mult;	// distance multiplier (attenuation/clipK)

	int master_vol;		// 0-255 master volume

	int isentence;		// true if playing linked sentence
	int iword;

	int pitch;
};

struct wavinfo_t
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
};

struct wavstream_t
{
	int		csamplesplayed;
	int		csamplesinmem;
	FileHandle_t hFile;
	wavinfo_t info;
	int		lastposloaded;
};

struct voxword_t
{
	int		volume;					// increase percent, ie: 125 = 125% increase
	int		pitch;					// pitch shift up percent
	int		start;					// offset start of wave percent
	int		end;					// offset end of wave percent
	int		cbtrim;					// end of wave after being trimmed to 'end'
	int		fKeepCached;			// 1 if this word was already in cache before sentence referenced it
	int		samplefrac;				// if pitch shifting, this is position into wav * 256
	int		timecompress;			// % of wave to skip during playback (causes no pitch shift)
	sfx_t*	sfx;					// name and cache pointer
};

#define CVOXWORDMAX		32

void S_Init( void );
void S_Startup( void );
void S_Shutdown( void );
void S_StartDynamicSound( int entnum, int entchannel, sfx_t* sfx, vec_t* origin, float fvol, float attenuation, int flags, int pitch );
void S_StartStaticSound( int entnum, int entchannel, sfx_t* sfxin, vec_t* origin, float fvol, float attenuation, int flags, int pitch );
void S_StopSound( int entnum, int entchannel );
void S_StopAllSounds( const bool clear );
void S_ClearBuffer( void );
void S_Update( vec_t* origin, vec_t* forward, vec_t* right, vec_t* up );
void S_ExtraUpdate( void );

sfx_t* S_PrecacheSound( char* name );
void S_BeginPrecaching( void );
void S_EndPrecaching( void );
void S_PaintChannels( int endtime );

// spatializes a channel
void SND_Spatialize( channel_t* ch );

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init( void );

// gets the current DMA position
int SNDDMA_GetDMAPos( void );

// shutdown the DMA xfer.
void SNDDMA_Shutdown( void );

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	8

extern channel_t    channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern int			total_channels;

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern bool				fakedma;
extern int		paintedtime;
extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern volatile dma_t* shm;
extern volatile dma_t sn;
extern vec_t sound_nominal_clip_dist;

extern	cvar_t loadas8bit;
extern	cvar_t bgmvolume;
extern	cvar_t volume;
extern	cvar_t hisound;
extern	cvar_t suitvolume;

extern bool snd_initialized;

extern int snd_blocked;

extern bool g_fUseDInput;
extern bool sound_started;

extern int cszrawsentences;
extern char* rgpszrawsentence[CVOXFILESENTENCEMAX];
extern float g_SND_VoiceOverdrive;

sfxcache_t* S_LoadSound( sfx_t* s, channel_t* channel );
sfx_t* S_FindName( char* name, int* pfInCache );

void SND_InitScaletable( void );
void SNDDMA_Submit( void );

void S_AmbientOff( void );
void S_AmbientOn( void );
void S_FreeChannel(channel_t* ch);

void S_PrintStats( void );
void ResampleSfx( sfx_t* sfx, int inrate, int inwidth, byte* data, int datasize );

void SetCareerAudioState( int paused );
void CareerAudio_Command_f( void );

void SND_PaintChannelFrom8Offs( portable_samplepair_t* paintbuffer, channel_t* ch, sfxcache_t* sc, int count, int offset );
void SND_PaintChannelFrom16Offs( portable_samplepair_t* paintbuffer, channel_t* ch, short* sfx, int count, int offset );

//=============================================================================

void S_TransferStereo16( int end );
void S_TransferPaintBuffer( int endtime );
void S_MixChannelsToPaintbuffer(int end, int fPaintHiresSounds, int voiceOnly);
bool S_CheckWavEnd( channel_t* ch, sfxcache_t** psc, int ltime, int ichan );

extern void SND_MoveMouth( channel_t* ch, sfxcache_t* sc, int count );
extern void SND_MoveMouth16( int entnum, short* pdata, int count );
extern void SND_CloseMouth( channel_t* ch );
extern void SND_InitMouth( int entnum, int entchannel );
extern void SND_ForceInitMouth( int entnum );
extern void SND_ForceCloseMouth( int entnum );

// DSP Routines

void SX_Init( void );
void SX_Free( void );
void SX_ReloadRoomFX( void );
void SX_RoomFX( int count, int fFilter, int fTimefx );

// WAVE Stream

int Wavstream_Init( void );
void Wavstream_Close( int i );
void Wavstream_GetNextChunk( channel_t* ch, sfx_t* s );

void Snd_ReleaseBuffer( void );
void Snd_AcquireBuffer( void );

void S_BlockSound( void );
void S_UnblockSound( void );

void SetMouseEnable( qboolean fEnable );

extern void				VOX_Init( void );
extern void				VOX_Shutdown( void );
extern char*			VOX_GetDirectory( char* szpath, char* psz, int nsize );
extern void				VOX_SetChanVol( channel_t* ch );
extern int				VOX_ParseWordParams( char* psz, voxword_t* pvoxword, int fFirst );
extern void				VOX_ReadSentenceFile( void );
extern sfxcache_t*		VOX_LoadSound( channel_t* pchan, char* pszin );
extern char*			VOX_LookupString( char* pszin, int* psentencenum );
extern void				VOX_MakeSingleWordSentence( channel_t* ch, int pitch );
extern void				VOX_TrimStartEndTimes( channel_t* ch, sfxcache_t* sc );
extern int				VOX_FPaintPitchChannelFrom8Offs( portable_samplepair_t* paintbuffer, channel_t* ch, sfxcache_t* sc, int count, int pitch, int timecompress, int offset );

extern "C"
{
	extern portable_samplepair_t paintbuffer[(PAINTBUFFER_SIZE + 1) * 2];
}

#endif // SOUND_H
